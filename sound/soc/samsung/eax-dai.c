/* sound/soc/samsung/eax-dai.c
 *
 * Exynos Audio Mixer driver
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/serio.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/firmware.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/exynos.h>

#include "lpass.h"
#include "dma.h"
#include "eax.h"


#define EAX_CH_MAX	8

#define EAX_RATES	SNDRV_PCM_RATE_8000_192000
#define EAX_FMTS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)


static struct eax_info {
	struct platform_device		*pdev;
	struct mutex			mutex;
	spinlock_t			lock;
	int				ch_max;
	bool				master;
	struct snd_soc_dai		*master_dai;
	const struct snd_soc_dai_ops	*master_dai_ops;
	int (*master_dai_suspend)(struct snd_soc_dai *dai);
	int (*master_dai_resume)(struct snd_soc_dai *dai);
} ei;

static LIST_HEAD(ch_list);

static DECLARE_WAIT_QUEUE_HEAD(eax_wq);

static int eax_dai_probe(struct snd_soc_dai *dai);
static int eax_dai_remove(struct snd_soc_dai *dai);
static int eax_dai_suspend(struct snd_soc_dai *dai);
static int eax_dai_resume(struct snd_soc_dai *dai);
static const struct snd_soc_dai_ops eax_dai_ops;

int eax_dev_register(struct device *dev_master, const char *name,
		 struct s3c_dma_params *dma_params, int ch)
{
	struct ch_info *ci;
	int ret, n;

	if (ch > EAX_CH_MAX) {
		pr_err("%s: Channel error! (max. %d)\n",
				__func__, EAX_CH_MAX);
		return -EINVAL;
	}

	eax_dma_params_register(dma_params);

	for (n = 0; n < ch; n++) {
		ci = kzalloc(sizeof(struct ch_info), GFP_KERNEL);
		if (!ci) {
			pr_err("%s: Memory alloc fails!\n", __func__);
			return -ENOMEM;
		}

		snprintf(ci->name, EAX_NAME_MAX, "samsung-eax.%d", n);

		ci->dev_master = dev_master;
		ci->dma_params = dma_params;
		ci->opened = false;
		ci->running = false;
		ci->ch_id = n;
		ci->dai_drv.name = name;
		ci->dai_drv.symmetric_rates = 1;
		ci->dai_drv.probe = eax_dai_probe;
		ci->dai_drv.remove = eax_dai_remove;
		ci->dai_drv.ops = &eax_dai_ops;
		ci->dai_drv.suspend = eax_dai_suspend;
		ci->dai_drv.resume = eax_dai_resume;
		ci->dai_drv.playback.channels_min = 2;
		ci->dai_drv.playback.channels_max = 2;
		ci->dai_drv.playback.rates = EAX_RATES;
		ci->dai_drv.playback.formats = EAX_FMTS;

		ci->pdev = platform_device_alloc(ci->name, -1);
		if (IS_ERR(ci->pdev)) {
			kfree(ci);
			return -ENODEV;
		}

		ei.ch_max++;
		list_add(&ci->node, &ch_list);
		platform_set_drvdata(ci->pdev, ci);
		ret = platform_device_add(ci->pdev);
		if (ret < 0)
			return -ENODEV;
	}

	return 0;
}

int eax_dai_register(struct snd_soc_dai *dai,
		const struct snd_soc_dai_ops *dai_ops,
		int (*dai_suspend)(struct snd_soc_dai *dai),
		int (*dai_resume)(struct snd_soc_dai *dai))
{
	pr_debug("%s: dai %p, dai_ops %p\n", __func__, dai, dai_ops);

	ei.master = true;
	ei.master_dai = dai;
	ei.master_dai_ops = dai_ops;
	ei.master_dai_suspend = dai_suspend;
	ei.master_dai_resume = dai_resume;

	eax_dma_dai_register(dai);

	return 0;
}

int eax_dai_unregister(void)
{
	ei.master = false;
	ei.master_dai = NULL;
	ei.master_dai_ops = NULL;
	ei.master_dai_suspend = NULL;
	ei.master_dai_resume = NULL;

	eax_dma_dai_unregister();

	return 0;
}

static inline struct ch_info *to_info(struct snd_soc_dai *dai)
{
	return snd_soc_dai_get_drvdata(dai);
}

static inline bool eax_dai_any_tx_opened(void)
{
	struct ch_info *ci;

	list_for_each_entry(ci, &ch_list, node) {
		if (ci->opened)
			return true;
	}

	return false;
}

static inline bool eax_dai_any_tx_running(void)
{
	struct ch_info *ci;

	list_for_each_entry(ci, &ch_list, node) {
		if (ci->running)
			return true;
	}

	return false;
}

static int eax_dai_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct ch_info *ci = to_info(dai);
	unsigned long flags;
	int ret = 0;

	if (!ei.master)
		return -ENODEV;

	spin_lock_irqsave(&ei.lock, flags);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (!eax_dai_any_tx_running())
			ret = (*ei.master_dai_ops->trigger)(substream,
							cmd, ei.master_dai);
		ci->running = true;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		ci->running = false;
		if (!eax_dai_any_tx_running())
			ret = (*ei.master_dai_ops->trigger)(substream,
							cmd, ei.master_dai);
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&ei.lock, flags);

	return ret;
}

#ifdef CONFIG_SND_SOC_I2S_1840_TDM
static int eax_dai_set_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	unsigned long flags;

	if (!ei.master)
		return -ENODEV;

	spin_lock_irqsave(&ei.lock, flags);
	if (eax_dai_any_tx_running()) {
		spin_unlock_irqrestore(&ei.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ei.lock, flags);

	return (*ei.master_dai_ops->set_tdm_slot)(ei.master_dai,
		tx_mask, rx_mask, slots, slot_width);
}
#endif

static int eax_dai_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned long flags;

	if (!ei.master)
		return -ENODEV;

	spin_lock_irqsave(&ei.lock, flags);
	if (eax_dai_any_tx_running()) {
		spin_unlock_irqrestore(&ei.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ei.lock, flags);

	return (*ei.master_dai_ops->hw_params)(substream, params,
						ei.master_dai);
}

static int eax_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	unsigned long flags;

	if (!ei.master)
		return -ENODEV;

	spin_lock_irqsave(&ei.lock, flags);
	if (eax_dai_any_tx_running()) {
		spin_unlock_irqrestore(&ei.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ei.lock, flags);

	return (*ei.master_dai_ops->set_fmt)(ei.master_dai, fmt);
}

static int eax_dai_set_clkdiv(struct snd_soc_dai *dai, int div_id, int div)
{
	unsigned long flags;

	if (!ei.master)
		return -ENODEV;

	spin_lock_irqsave(&ei.lock, flags);
	if (eax_dai_any_tx_running()) {
		spin_unlock_irqrestore(&ei.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ei.lock, flags);

	return (*ei.master_dai_ops->set_clkdiv)(ei.master_dai, div_id, div);
}

static int eax_dai_set_sysclk(struct snd_soc_dai *dai,
	int clk_id, unsigned int rfs, int dir)
{
	unsigned long flags;

	if (!ei.master)
		return -ENODEV;

	spin_lock_irqsave(&ei.lock, flags);
	if (eax_dai_any_tx_running()) {
		spin_unlock_irqrestore(&ei.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ei.lock, flags);

	return (*ei.master_dai_ops->set_sysclk)(ei.master_dai,
						clk_id, rfs, dir);
}

static int eax_dai_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct ch_info *ci = to_info(dai);
	int ret = 0;

	if (!ei.master)
		return -ENODEV;

	lpass_add_stream();

	mutex_lock(&ei.mutex);

	if (!eax_dai_any_tx_opened())
		ret = (*ei.master_dai_ops->startup)(substream, ei.master_dai);

	ci->opened = true;

	mutex_unlock(&ei.mutex);

	return ret;
}

static void eax_dai_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct ch_info *ci = to_info(dai);

	if (!ei.master)
		return;

	mutex_lock(&ei.mutex);

	ci->opened = false;

	if (!eax_dai_any_tx_opened())
		(*ei.master_dai_ops->shutdown)(substream, ei.master_dai);

	mutex_unlock(&ei.mutex);

	lpass_remove_stream();
}

static int eax_dai_probe(struct snd_soc_dai *dai)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static int eax_dai_remove(struct snd_soc_dai *dai)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static int eax_dai_suspend(struct snd_soc_dai *dai)
{
	if (dai->active && ei.master_dai_suspend)
		return (*ei.master_dai_suspend)(ei.master_dai);

	return 0;
}

static int eax_dai_resume(struct snd_soc_dai *dai)
{
	if (dai->active && ei.master_dai_resume)
		return (*ei.master_dai_resume)(ei.master_dai);

	return 0;
}

static const struct snd_soc_dai_ops eax_dai_ops = {
	.trigger	= eax_dai_trigger,
	.hw_params	= eax_dai_hw_params,
	.set_fmt	= eax_dai_set_fmt,
	.set_clkdiv	= eax_dai_set_clkdiv,
	.set_sysclk	= eax_dai_set_sysclk,
#ifdef CONFIG_SND_SOC_I2S_1840_TDM
	.set_tdm_slot	= eax_dai_set_tdm_slot,
#endif
	.startup	= eax_dai_startup,
	.shutdown	= eax_dai_shutdown,
};

static const struct snd_soc_component_driver eax_dai_component = {
	.name		= "samsung-eax",
};

static int eax_ch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ch_info *ci;

	ci = platform_get_drvdata(pdev);

	snd_soc_register_component(dev, &eax_dai_component, &ci->dai_drv, 1);
	eax_asoc_platform_register(dev);

	return 0;
}

static int eax_ch_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	snd_soc_unregister_component(dev);

	return 0;
}

static const char banner[] =
	KERN_INFO "Exynos Audio Mixer driver, (c)2014 Samsung Electronics\n";

static int eax_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	printk(banner);

	mutex_init(&ei.mutex);
	spin_lock_init(&ei.lock);

	ei.pdev = pdev;

	pm_runtime_enable(dev);

	return 0;
}

static int eax_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int eax_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s entered\n", __func__);

	return 0;
}

static int eax_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s entered\n", __func__);

	return 0;
}
#endif

#if !defined(CONFIG_PM_RUNTIME) && defined(CONFIG_PM_SLEEP)
static int eax_suspend(struct device *dev)
{
	dev_dbg(dev, "%s entered\n", __func__);

	return 0;
}

static int eax_resume(struct device *dev)
{
	dev_dbg(dev, "%s entered\n", __func__);

	return 0;
}
#else
#define eax_suspend	NULL
#define eax_resume	NULL
#endif

static struct platform_device_id eax_ch_driver_ids[] = {
	{ .name	= "samsung-eax.0", },
	{ .name	= "samsung-eax.1", },
	{ .name	= "samsung-eax.2", },
	{ .name	= "samsung-eax.3", },
	{ .name	= "samsung-eax.4", },
	{ .name	= "samsung-eax.5", },
	{ .name	= "samsung-eax.6", },
	{ .name	= "samsung-eax.7", },
	{},
};
MODULE_DEVICE_TABLE(platform, eax_ch_driver_ids);

static const struct dev_pm_ops eax_ch_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		eax_suspend,
		eax_resume
	)
	SET_RUNTIME_PM_OPS(
		eax_runtime_suspend,
		eax_runtime_resume,
		NULL
	)
};

static struct platform_driver eax_dai_driver = {
	.probe		= eax_ch_probe,
	.remove		= eax_ch_remove,
	.id_table	= eax_ch_driver_ids,
	.driver		= {
		.name	= "samsung-eax",
		.owner	= THIS_MODULE,
		.pm	= &eax_ch_pmops,
	},
};

module_platform_driver(eax_dai_driver);

static struct platform_device_id eax_driver_ids[] = {
	{
		.name	= "samsung-amixer",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, eax_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id exynos_eax_match[] = {
	{
		.compatible = "samsung,exynos-amixer",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_eax_match);
#endif

static struct platform_driver eax_driver = {
	.probe		= eax_probe,
	.remove		= eax_remove,
	.id_table	= eax_driver_ids,
	.driver		= {
		.name	= "samsung-amixer",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_eax_match),
	},
};

module_platform_driver(eax_driver);

MODULE_AUTHOR("Yeongman Seo <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Exynos Audio Mixer driver");
MODULE_LICENSE("GPL");
