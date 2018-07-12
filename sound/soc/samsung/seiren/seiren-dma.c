/* sound/soc/samsung/seiren/seiren-dma.c
 *
 * Exynos Seiren DMA driver for Exynos5430
 *
 * Copyright (c) 2014 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <linux/dma/dma-pl330.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/exynos.h>

#include <asm/dma.h>

#include "seiren.h"
#include "seiren-dma.h"
#include "../dma.h"

/* DMAC source/destination addr */
#define _SA			0x400
#define SA(n)			(_SA + (n)*0x20)

#define _DA			0x404
#define DA(n)			(_DA + (n)*0x20)

static struct seiren_dma_info {
	struct platform_device	*pdev;
	spinlock_t		lock;
	void __iomem		*regs;
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	struct iommu_domain	*domain;
#endif
	struct runtime		*rtd[DMA_CH_MAX];
} sdi;

struct runtime {
	int			ch;
	u32			peri;
	void __iomem		*regs;
	void __iomem		*reg_ack;
	void __iomem		*reg_sa;
	void __iomem		*reg_da;
};

static int of_dma_match_channel(struct device_node *np, const char *name,
				int index, struct of_phandle_args *dma_spec)
{
	const char *s;

	if (of_property_read_string_index(np, "dma-names", index, &s))
		return -ENODEV;

	if (strcmp(name, s))
		return -ENODEV;

	if (of_parse_phandle_with_args(np, "dmas", "#dma-cells", index,
				       dma_spec))
		return -ENODEV;

	return 0;
}

static unsigned long esa_dma_request(enum dma_ch dma_ch,
				struct samsung_dma_req *param,
				struct device *dev, char *ch_name)
{
	struct device_node *np;
	struct of_phandle_args dma_spec;
	struct runtime *rtd = NULL;
	int ch, count, n;

	ch = esa_dma_open();
	if (ch < 0) {
		esa_err("%s: dma ch alloc fails!!!\n", __func__);
		return -EIO;
	}

	rtd = kzalloc(sizeof(struct runtime), GFP_KERNEL);
	if (!rtd) {
		esa_err("%s: runtime data alloc fails!!!\n", __func__);
		esa_dma_close(ch);
		return -EIO;
	}

	pm_runtime_get_sync(&sdi.pdev->dev);

	sdi.rtd[ch] = rtd;
	rtd->ch = ch;
	rtd->regs = esa_dma_get_mem() + (ch * DMA_PARAM_SIZE);
	rtd->reg_ack = rtd->regs + DMA_ACK;
	rtd->reg_sa = sdi.regs + SA(ch);
	rtd->reg_da = sdi.regs + DA(ch);
	memset(rtd->regs, 0, DMA_PARAM_SIZE);

	np = dev->of_node;
	if (!np) {
		esa_err("%s: of_node not found!!!\n", __func__);
		goto err;
	}

	count = of_property_count_strings(np, "dma-names");
	if (count < 0) {
		esa_err("%s: dma-names property missing\n", __func__);
		goto err;
	}

	for (n = 0; n < count; n++) {
		if (!of_dma_match_channel(np, ch_name, n, &dma_spec)) {
			of_property_read_u32_index(np, "dmas",
					n * 2 + 1, &rtd->peri);
			esa_debug("%s: ch %d (peri %d)\n",
					__func__, rtd->ch, rtd->peri);
			writel(rtd->peri, rtd->regs + DMA_PERI);

			return (unsigned long)ch;
		}
	}
	esa_err("%s: dmas property of %s missing\n", __func__, ch_name);

err:
	esa_dma_close(ch);
	kfree(rtd);
	sdi.rtd[ch] = NULL;
	pm_runtime_put_sync(&sdi.pdev->dev);

	return -EIO;
}

static int esa_dma_release(unsigned  long ch, void *param)
{
	struct runtime *rtd = sdi.rtd[ch];

	esa_dma_close(rtd->ch);
	kfree(rtd);
	sdi.rtd[ch] = NULL;
	pm_runtime_put_sync(&sdi.pdev->dev);

	return 0;
}

static int esa_dma_config(unsigned long ch, struct samsung_dma_config *param)
{
	struct runtime *rtd = sdi.rtd[ch];
	void __iomem *regs = rtd->regs;
	u32 mode_1 = DMA_MODE_NOP;
	u32 src_pa = 0;
	u32 dst_pa = 0;

	if (param->direction == DMA_MEM_TO_DEV) {
		mode_1 = DMA_MODE_MEM2DEV;
		dst_pa = param->fifo;
	} else if (param->direction == DMA_DEV_TO_MEM) {
		mode_1 = DMA_MODE_DEV2MEM;
		src_pa = param->fifo;
	}

	writel(mode_1, regs + DMA_MODE_1);
	writel(src_pa, regs + DMA_SRC_PA);
	writel(dst_pa, regs + DMA_DST_PA);

	return 0;
}

static int esa_dma_prepare(unsigned long ch, struct samsung_dma_prep *param)
{
	struct runtime *rtd = sdi.rtd[ch];
	void __iomem *regs = rtd->regs;
	u32 mode_2;

	mode_2 = (param->infiniteloop) ? DMA_MODE_LOOP : DMA_MODE_ONCE;
	writel(mode_2, regs + DMA_MODE_2);

	if (param->direction == DMA_MEM_TO_DEV)
		writel(param->buf, regs + DMA_SRC_PA);
	else if (param->direction == DMA_DEV_TO_MEM)
		writel(param->buf, regs + DMA_DST_PA);

	writel(param->period, regs + DMA_PERIOD);
	writel(param->len / param->period, regs + DMA_PERIOD_CNT);

	esa_dma_set_callback(param->fp, param->fp_param, rtd->ch);
	esa_dma_send_cmd(CMD_DMA_PREPARE, rtd->ch, rtd->reg_ack);

	return 0;
}

static int esa_dma_trigger(unsigned long ch)
{
	struct runtime *rtd = sdi.rtd[ch];

	esa_dma_send_cmd(CMD_DMA_START, rtd->ch, rtd->reg_ack);

	return 0;
}

static int esa_dma_getposition(unsigned long ch, dma_addr_t *src, dma_addr_t *dst)
{
	struct runtime *rtd = sdi.rtd[ch];

	*src = readl(rtd->reg_sa);
	*dst = readl(rtd->reg_da);

	return 0;
}

static int esa_dma_flush(unsigned long ch)
{
	struct runtime *rtd = sdi.rtd[ch];

	esa_dma_send_cmd(CMD_DMA_STOP, rtd->ch, rtd->reg_ack);

	return 0;
}

static struct samsung_dma_ops esa_dma_ops = {
	.request	= esa_dma_request,
	.release	= esa_dma_release,
	.config		= esa_dma_config,
	.prepare	= esa_dma_prepare,
	.trigger	= esa_dma_trigger,
	.started	= NULL,
	.getposition	= esa_dma_getposition,
	.flush		= esa_dma_flush,
	.stop		= esa_dma_flush,
};

void *samsung_esa_dma_get_ops(void)
{
	return &esa_dma_ops;
}
EXPORT_SYMBOL_GPL(samsung_esa_dma_get_ops);

static const char banner[] =
	KERN_INFO "Exynos Seiren ADMA driver, (c)2014 Samsung Electronics\n";

static int esa_dma_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	printk(banner);

	spin_lock_init(&sdi.lock);

	sdi.pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Unable to get LPASS SFRs\n");
		return -ENXIO;
	}

	sdi.regs = devm_ioremap_resource(&pdev->dev, res);
	if (!sdi.regs) {
		dev_err(dev, "SFR ioremap failed\n");
		return -ENOMEM;
	}
	esa_debug("regs_base = %08X (%08X bytes)\n",
			(unsigned int)res->start, (unsigned int)resource_size(res));

	if (np) {
		if (of_find_property(np, "samsung,lpass-subip", NULL))
			lpass_register_subip(dev, "dmac");
	}

#ifdef CONFIG_SND_SAMSUNG_IOMMU
	sdi.domain = lpass_get_iommu_domain();
	if (!sdi.domain) {
		dev_err(dev, "iommu not available\n");
		goto err;
	}
#else
	dev_err(dev, "iommu not available\n");
	goto err;
#endif

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	pm_runtime_put_sync(dev);
#else
	esa_dma_do_resume(dev);
#endif

	return 0;

err:
	return -ENODEV;
}

static int esa_dma_remove(struct platform_device *pdev)
{
	int ret = 0;

#ifndef CONFIG_PM_RUNTIME
	lpass_put_sync(&pdev->dev);
#endif

	return ret;
}

static int esa_dma_do_suspend(struct device *dev)
{
	lpass_put_sync(dev);

	return 0;
}

static int esa_dma_do_resume(struct device *dev)
{
	lpass_get_sync(dev);

	return 0;
}

#if !defined(CONFIG_PM_RUNTIME) && defined(CONFIG_PM_SLEEP)
static int esa_dma_suspend(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_dma_do_suspend(dev);
}

static int esa_dma_resume(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_dma_do_resume(dev);
}
#else
#define esa_dma_suspend	NULL
#define esa_dma_resume	NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int esa_dma_runtime_suspend(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_dma_do_suspend(dev);
}

static int esa_dma_runtime_resume(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_dma_do_resume(dev);
}
#endif

static struct platform_device_id esa_dma_driver_ids[] = {
	{
		.name	= "samsung-seiren-dma",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, esa_dma_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id exynos_esa_dma_match[] = {
	{
		.compatible = "samsung,exynos5430-seiren-dma",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_esa_dma_match);
#endif

static const struct dev_pm_ops esa_dma_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		esa_dma_suspend,
		esa_dma_resume
	)
	SET_RUNTIME_PM_OPS(
		esa_dma_runtime_suspend,
		esa_dma_runtime_resume,
		NULL
	)
};

static struct platform_driver esa_dma_driver = {
	.probe		= esa_dma_probe,
	.remove		= esa_dma_remove,
	.id_table	= esa_dma_driver_ids,
	.driver		= {
		.name	= "samsung-seiren-dma",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_esa_dma_match),
		.pm	= &esa_dma_pmops,
	},
};

module_platform_driver(esa_dma_driver);

MODULE_AUTHOR("Yeongman Seo <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Exynos Seiren DMA Driver");
MODULE_LICENSE("GPL");
