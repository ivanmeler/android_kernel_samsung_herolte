/* sound/soc/samsung/eax-dai.c
 *
 * Exynos Audio Mixer slowpath driver
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
#define EAX_NAME_MAX	PLATFORM_NAME_SIZE

#define EAX_RATES	SNDRV_PCM_RATE_8000_192000
#define EAX_FMTS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)


static struct eax_slowpath_info {
	struct platform_device		*pdev;
	struct mutex			mutex;
	spinlock_t			lock;
	int				ch_max;
	bool				master;
	struct snd_soc_dai		*master_dai;
	const struct snd_soc_dai_ops	*master_dai_ops;
	int (*master_dai_suspend)(struct snd_soc_dai *dai);
	int (*master_dai_resume)(struct snd_soc_dai *dai);
} esi;

static LIST_HEAD(slowpath_ch_list);

static DECLARE_WAIT_QUEUE_HEAD(eax_slowpath_wq);

static int eax_slowpath_dai_probe(struct snd_soc_dai *dai);
static int eax_slowpath_dai_remove(struct snd_soc_dai *dai);
static int eax_slowpath_dai_suspend(struct snd_soc_dai *dai);
static int eax_slowpath_dai_resume(struct snd_soc_dai *dai);
static const struct snd_soc_dai_ops eax_slowpath_dai_ops;

int eax_slowpath_dev_register(struct device *dev_master, const char *name,
		 struct s3c_dma_params *dma_params, int ch)
{
	struct ch_info *ci;
	int ret, n;

	if (ch > EAX_CH_MAX) {
		pr_err("%s: Channel error! (max. %d)\n",
				__func__, EAX_CH_MAX);
		return -EINVAL;
	}

	eax_slowpath_params_register(dma_params);

	for (n = 0; n < ch; n++) {
		ci = kzalloc(sizeof(struct ch_info), GFP_KERNEL);
		if (!ci) {
			pr_err("%s: Memory alloc fails!\n", __func__);
			return -ENOMEM;
		}

		snprintf(ci->name, EAX_NAME_MAX, "samsung-eax-slow.%d", n);

		ci->dev_master = dev_master;
		ci->dma_params = dma_params;
		ci->opened = false;
		ci->running = false;
		ci->dai_drv.name = name;
		ci->dai_drv.symmetric_rates = 1;
		ci->dai_drv.probe = eax_slowpath_dai_probe;
		ci->dai_drv.remove = eax_slowpath_dai_remove;
		ci->dai_drv.ops = &eax_slowpath_dai_ops;
		ci->dai_drv.suspend = eax_slowpath_dai_suspend;
		ci->dai_drv.resume = eax_slowpath_dai_resume;
		ci->dai_drv.playback.channels_min = 2;
		ci->dai_drv.playback.channels_max = 2;
		ci->dai_drv.playback.rates = EAX_RATES;
		ci->dai_drv.playback.formats = EAX_FMTS;

		ci->pdev = platform_device_alloc(ci->name, -1);
		if (IS_ERR(ci->pdev)) {
			kfree(ci);
			return -ENODEV;
		}

		esi.ch_max++;
		list_add(&ci->node, &slowpath_ch_list);
		platform_set_drvdata(ci->pdev, ci);
		ret = platform_device_add(ci->pdev);
		if (ret < 0)
			return -ENODEV;
	}

	return 0;
}

int eax_slowpath_dai_register(struct snd_soc_dai *dai,
		const struct snd_soc_dai_ops *dai_ops,
		int (*dai_suspend)(struct snd_soc_dai *dai),
		int (*dai_resume)(struct snd_soc_dai *dai))
{
	pr_debug("%s: dai %p, dai_ops %p\n", __func__, dai, dai_ops);

	esi.master = true;
	esi.master_dai = dai;
	esi.master_dai_ops = dai_ops;
	esi.master_dai_suspend = dai_suspend;
	esi.master_dai_resume = dai_resume;

	eax_slowpath_dma_dai_register(dai);

	return 0;
}

int eax_slowpath_dai_unregister(void)
{
	esi.master = false;
	esi.master_dai = NULL;
	esi.master_dai_ops = NULL;
	esi.master_dai_suspend = NULL;
	esi.master_dai_resume = NULL;

	eax_slowpath_dma_dai_unregister();

	return 0;
}

static inline struct ch_info *to_info(struct snd_soc_dai *dai)
{
	return snd_soc_dai_get_drvdata(dai);
}

static inline bool eax_slowpath_dai_any_tx_opened(void)
{
	struct ch_info *ci;

	list_for_each_entry(ci, &slowpath_ch_list, node) {
		if (ci->opened)
			return true;
	}

	return false;
}

static inline bool eax_slowpath_dai_any_tx_running(void)
{
	struct ch_info *ci;

	list_for_each_entry(ci, &slowpath_ch_list, node) {
		if (ci->running)
			return true;
	}

	return false;
}

static int eax_slowpath_dai_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct ch_info *ci = to_info(dai);
	unsigned long flags;
	int ret = 0;

	if (!esi.master)
		return -ENODEV;

	spin_lock_irqsave(&esi.lock, flags);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (!eax_slowpath_dai_any_tx_running())
			ret = (*esi.master_dai_ops->trigger)(substream,
							cmd, esi.master_dai);
		ci->running = true;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		ci->running = false;
		if (!eax_slowpath_dai_any_tx_running())
			ret = (*esi.master_dai_ops->trigger)(substream,
							cmd, esi.master_dai);
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&esi.lock, flags);

	return ret;
}

#ifdef CONFIG_SND_SOC_I2S_1840_TDM
static int eax_slowpath_dai_set_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	unsigned long flags;
	if (!esi.master)
		return -ENODEV;

	spin_lock_irqsave(&esi.lock, flags);
	if (eax_slowpath_dai_any_tx_running()) {
		spin_unlock_irqrestore(&esi.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&esi.lock, flags);

	return (*esi.master_dai_ops->set_tdm_slot)(esi.master_dai,
		tx_mask, rx_mask, slots, slot_width);
}
#endif

static int eax_slowpath_dai_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned long flags;

	if (!esi.master)
		return -ENODEV;

	spin_lock_irqsave(&esi.lock, flags);
	if (eax_slowpath_dai_any_tx_running()) {
		spin_unlock_irqrestore(&esi.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&esi.lock, flags);

	return (*esi.master_dai_ops->hw_params)(substream, params,
						esi.master_dai);
}

static int eax_slowpath_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	unsigned long flags;

	if (!esi.master)
		return -ENODEV;

	spin_lock_irqsave(&esi.lock, flags);
	if (eax_slowpath_dai_any_tx_running()) {
		spin_unlock_irqrestore(&esi.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&esi.lock, flags);

	return (*esi.master_dai_ops->set_fmt)(esi.master_dai, fmt);
}

static int eax_slowpath_dai_set_clkdiv(struct snd_soc_dai *dai, int div_id, int div)
{
	unsigned long flags;

	if (!esi.master)
		return -ENODEV;

	spin_lock_irqsave(&esi.lock, flags);
	if (eax_slowpath_dai_any_tx_running()) {
		spin_unlock_irqrestore(&esi.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&esi.lock, flags);

	return (*esi.master_dai_ops->set_clkdiv)(esi.master_dai, div_id, div);
}

static int eax_slowpath_dai_set_sysclk(struct snd_soc_dai *dai,
	int clk_id, unsigned int rfs, int dir)
{
	unsigned long flags;

	if (!esi.master)
		return -ENODEV;

	spin_lock_irqsave(&esi.lock, flags);
	if (eax_slowpath_dai_any_tx_running()) {
		spin_unlock_irqrestore(&esi.lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&esi.lock, flags);

	return (*esi.master_dai_ops->set_sysclk)(esi.master_dai,
						clk_id, rfs, dir);
}

static int eax_slowpath_dai_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct ch_info *ci = to_info(dai);
	int ret = 0;

	if (!esi.master)
		return -ENODEV;

	lpass_add_stream();

	mutex_lock(&esi.mutex);

	if (!eax_slowpath_dai_any_tx_opened())
		ret = (*esi.master_dai_ops->startup)(substream, esi.master_dai);

	ci->opened = true;

	mutex_unlock(&esi.mutex);

	return ret;
}

static void eax_slowpath_dai_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct ch_info *ci = to_info(dai);

	if (!esi.master)
		return;

	mutex_lock(&esi.mutex);

	ci->opened = false;

	if (!eax_slowpath_dai_any_tx_opened())
		(*esi.master_dai_ops->shutdown)(substream, esi.master_dai);

	mutex_unlock(&esi.mutex);

	lpass_remove_stream();
}

static int eax_slowpath_dai_probe(struct snd_soc_dai *dai)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static int eax_slowpath_dai_remove(struct snd_soc_dai *dai)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static int eax_slowpath_dai_suspend(struct snd_soc_dai *dai)
{
	if (dai->active && esi.master_dai_suspend)
		return (*esi.master_dai_suspend)(esi.master_dai);

	return 0;
}

static int eax_slowpath_dai_resume(struct snd_soc_dai *dai)
{
	if (dai->active && esi.master_dai_resume)
		return (*esi.master_dai_resume)(esi.master_dai);

	return 0;
}

static const struct snd_soc_dai_ops eax_slowpath_dai_ops = {
	.trigger	= eax_slowpath_dai_trigger,
	.hw_params	= eax_slowpath_dai_hw_params,
	.set_fmt	= eax_slowpath_dai_set_fmt,
	.set_clkdiv	= eax_slowpath_dai_set_clkdiv,
	.set_sysclk	= eax_slowpath_dai_set_sysclk,
#ifdef CONFIG_SND_SOC_I2S_1840_TDM
	.set_tdm_slot	= eax_slowpath_dai_set_tdm_slot,
#endif
	.startup	= eax_slowpath_dai_startup,
	.shutdown	= eax_slowpath_dai_shutdown,
};

static const struct snd_soc_component_driver eax_slowpath_dai_component = {
	.name		= "samsung-eax-slowpath",
};

static int eax_slowpath_ch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ch_info *ci;

	ci = platform_get_drvdata(pdev);

	snd_soc_register_component(dev, &eax_slowpath_dai_component, &ci->dai_drv, 1);
	eax_slowpath_asoc_platform_register(dev);

	return 0;
}

static int eax_slowpath_ch_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	snd_soc_unregister_component(dev);

	return 0;
}

static const char banner[] =
	KERN_INFO "Exynos Audio Slowpath Mixer driver, (c)2015 Samsung Electronics\n";

static int eax_slowpath_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	printk(banner);

	mutex_init(&esi.mutex);
	spin_lock_init(&esi.lock);

	esi.pdev = pdev;

	pm_runtime_enable(dev);

	return 0;
}

static int eax_slowpath_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int eax_slowpath_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s entered\n", __func__);

	return 0;
}

static int eax_slowpath_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s entered\n", __func__);

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int eax_slowpath_suspend(struct device *dev)
{
	dev_dbg(dev, "%s entered\n", __func__);

	return 0;
}

static int eax_slowpath_resume(struct device *dev)
{
	dev_dbg(dev, "%s entered\n", __func__);

	return 0;
}
#else
#define eax_slowpath_suspend	NULL
#define eax_slowpath_resume	NULL
#endif

static struct platform_device_id eax_slowpath_ch_driver_ids[] = {
	{ .name	= "samsung-eax-slow.0", },
	{ .name	= "samsung-eax-slow.1", },
	{},
};
MODULE_DEVICE_TABLE(platform, eax_slowpath_ch_driver_ids);

static const struct dev_pm_ops eax_slowpath_ch_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		eax_slowpath_suspend,
		eax_slowpath_resume
	)
	SET_RUNTIME_PM_OPS(
		eax_slowpath_runtime_suspend,
		eax_slowpath_runtime_resume,
		NULL
	)
};

static struct platform_driver eax_slowpath_dai_driver = {
	.probe		= eax_slowpath_ch_probe,
	.remove		= eax_slowpath_ch_remove,
	.id_table	= eax_slowpath_ch_driver_ids,
	.driver		= {
		.name	= "samsung-eax-slowpath",
		.owner	= THIS_MODULE,
		.pm	= &eax_slowpath_ch_pmops,
	},
};

module_platform_driver(eax_slowpath_dai_driver);

static struct platform_device_id eax_slowpath_driver_ids[] = {
	{
		.name	= "samsung-amixer-slow",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, eax_slowpath_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id exynos_eax_slowpath_match[] = {
	{
		.compatible = "samsung,exynos-amixer-slowpath",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_eax_slowpath_match);
#endif

static struct platform_driver eax_slowpath_driver = {
	.probe		= eax_slowpath_probe,
	.remove		= eax_slowpath_remove,
	.id_table	= eax_slowpath_driver_ids,
	.driver		= {
		.name	= "samsung-amixer-slow",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_eax_slowpath_match),
	},
};

module_platform_driver(eax_slowpath_driver);

MODULE_AUTHOR("Hyunwoong Kim <khw0178.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos Audio Mixer Slowpath driver");
MODULE_LICENSE("GPL");
