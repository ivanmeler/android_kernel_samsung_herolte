/*
 * Samsung EXYNOS SoC series MIPI CSIS/DSIM DPHY driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * Author: Sewoon Park <seuni.park@samsung.com>
 * Author: Wooki Min <wooki.min@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <asm/io.h>

#define EXYNOS_MIPI_PHY_ISO_BYPASS  BIT(0)
#define EXYNOS_MIPI_PHYS_NUM 4

#define MIPI_PHY_MxSx_UNIQUE 	(0 << 1)
#define MIPI_PHY_MxSx_SHARED 	(1 << 1)
#define MIPI_PHY_MxSx_INIT_DONE (2 << 1)

/* reference count for phy-m4s4 */
static int phy_m4s4_count;

enum exynos_mipi_phy_type {
	EXYNOS_MIPI_PHY_FOR_DSIM,
	EXYNOS_MIPI_PHY_FOR_CSIS,
};

struct mipi_phy_data {
	enum exynos_mipi_phy_type type;
	u8 flags;
};

struct exynos_mipi_phy {
	struct device *dev;
	spinlock_t slock;
	void __iomem *regs;
	struct regmap *reg_pmu;
	struct mipi_phy_desc {
		struct phy *phy;
		unsigned int index;
		enum exynos_mipi_phy_type type;
		unsigned int iso_offset;
		unsigned int rst_bit;
		u8 flags;
	} phys[EXYNOS_MIPI_PHYS_NUM];
};

/* 1: Isolation bypass, 0: Isolation enable */
static int __set_phy_isolation(struct regmap *reg_pmu,
		unsigned int offset, unsigned int on)
{
	unsigned int val;
	int ret;

	val = on ? EXYNOS_MIPI_PHY_ISO_BYPASS : 0;

	ret = regmap_update_bits(reg_pmu, offset,
			EXYNOS_MIPI_PHY_ISO_BYPASS, val);

	pr_debug("%s off=0x%x, val=0x%x\n", __func__, offset, val);
	return ret;
}

/* 1: Enable reset -> release reset, 0: Enable reset */
static int __set_phy_reset(struct exynos_mipi_phy *state,
		unsigned int bit, unsigned int on)
{
	void __iomem *addr = state->regs;
	unsigned int cfg;

	if (IS_ERR_OR_NULL(addr)) {
		dev_err(state->dev, "%s Invalid address\n", __func__);
		return -EINVAL;
	}

	cfg = readl(addr);
	cfg &= ~(1 << bit);
	writel(cfg, addr);

	/* release a reset before using a PHY */
	if (on) {
		cfg |= (1 << bit);
		writel(cfg, addr);
	}

	pr_debug("%s bit=%d, val=0x%x\n", __func__, bit, cfg);
	return 0;
}

static int __set_phy_init(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned int cfg;

	ret = regmap_read(state->reg_pmu,
			phy_desc->iso_offset, &cfg);
	if (ret) {
		dev_err(state->dev, "%s Can't read 0x%x\n",
				__func__, phy_desc->iso_offset);
		ret = -EINVAL;
		goto phy_exit;
	}

	/* Add INIT_DONE flag when ISO is already bypass(LCD_ON_UBOOT) */
	if (cfg && EXYNOS_MIPI_PHY_ISO_BYPASS)
		phy_desc->flags |= MIPI_PHY_MxSx_INIT_DONE;

phy_exit:
	return ret;
}

static int __set_phy_alone(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&state->slock, flags);
	if (on) {
		ret = __set_phy_isolation(state->reg_pmu,
				phy_desc->iso_offset, on);

		__set_phy_reset(state, phy_desc->rst_bit, on);

	} else {
		__set_phy_reset(state, phy_desc->rst_bit, on);

		ret = __set_phy_isolation(state->reg_pmu,
				phy_desc->iso_offset, on);

	}
	pr_debug("%s: isolation 0x%x, reset 0x%x\n", __func__,
			phy_desc->iso_offset, phy_desc->rst_bit);
	spin_unlock_irqrestore(&state->slock, flags);

	return ret;
}

static DEFINE_SPINLOCK(lock_share);
static int __set_phy_share(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&lock_share, flags);

	on ? ++phy_m4s4_count : --phy_m4s4_count;

	/* If phy is already initialization(power_on) */
	if (phy_desc->flags & MIPI_PHY_MxSx_INIT_DONE) {
		phy_desc->flags &= (~MIPI_PHY_MxSx_INIT_DONE);
		spin_unlock_irqrestore(&lock_share, flags);
		return ret;
	}

	if (on) {
		/* Isolation bypass when reference count is 1 */
		if (phy_m4s4_count == 1)
			ret = __set_phy_isolation(state->reg_pmu,
					phy_desc->iso_offset, on);

		__set_phy_reset(state, phy_desc->rst_bit, on);

	} else {
		__set_phy_reset(state, phy_desc->rst_bit, on);

		/* Isolation enabled when reference count is zero */
		if (phy_m4s4_count == 0)
			ret = __set_phy_isolation(state->reg_pmu,
					phy_desc->iso_offset, on);
	}

	pr_debug("%s: isolation 0x%x, reset 0x%x\n", __func__,
			phy_desc->iso_offset, phy_desc->rst_bit);
	spin_unlock_irqrestore(&lock_share, flags);

	return ret;
}

static int __set_phy_state(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;

	if (phy_desc->flags & MIPI_PHY_MxSx_SHARED)
		ret = __set_phy_share(state, phy_desc, on);
	else
		ret = __set_phy_alone(state, phy_desc, on);

	return ret;
}

static const struct mipi_phy_data mipi_phy_m4sx = {
	.type = EXYNOS_MIPI_PHY_FOR_DSIM,
	.flags =  MIPI_PHY_MxSx_SHARED,
};

static const struct mipi_phy_data mipi_phy_mxs4 = {
	.type = EXYNOS_MIPI_PHY_FOR_CSIS,
	.flags =  MIPI_PHY_MxSx_SHARED,
};

static const struct mipi_phy_data mipi_phy_mxs0 = {
	.type = EXYNOS_MIPI_PHY_FOR_DSIM,
	.flags =  MIPI_PHY_MxSx_UNIQUE,
};

static const struct mipi_phy_data mipi_phy_m0sx = {
	.type = EXYNOS_MIPI_PHY_FOR_CSIS,
	.flags =  MIPI_PHY_MxSx_UNIQUE,
};

static const struct of_device_id exynos_mipi_phy_of_table[] = {
	{
		.compatible = "samsung,mipi-phy-dsim",
		.data = &mipi_phy_m4sx,
	},
	{
		.compatible = "samsung,mipi-phy-m4",
		.data = &mipi_phy_mxs0,
	},
	{
		.compatible = "samsung,mipi-phy-m2",
		.data = &mipi_phy_mxs0,
	},
	{
		.compatible = "samsung,mipi-phy-m1",
		.data = &mipi_phy_mxs0,
	},
	{
		.compatible = "samsung,mipi-phy-csis",
		.data = &mipi_phy_mxs4,
	},
	{
		.compatible = "samsung,mipi-phy-s4",
		.data = &mipi_phy_m0sx,
	},
	{
		.compatible = "samsung,mipi-phy-s2",
		.data = &mipi_phy_m0sx,
	},
	{
		.compatible = "samsung,mipi-phy-s1",
		.data = &mipi_phy_m0sx,
	},
};
MODULE_DEVICE_TABLE(of, exynos_mipi_phy_of_table);

#define to_mipi_video_phy(desc) \
	container_of((desc), struct exynos_mipi_phy, phys[(desc)->index])

static int exynos_mipi_phy_init(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_init(state, phy_desc, 1);
}


static int exynos_mipi_phy_power_on(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_state(state, phy_desc, 1);
}

static int exynos_mipi_phy_power_off(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_state(state, phy_desc, 0);
}

static struct phy *exynos_mipi_phy_of_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct exynos_mipi_phy *state = dev_get_drvdata(dev);

	if (WARN_ON(args->args[0] >= EXYNOS_MIPI_PHYS_NUM))
		return ERR_PTR(-ENODEV);

	return state->phys[args->args[0]].phy;
}

static struct phy_ops exynos_mipi_phy_ops = {
	.init		= exynos_mipi_phy_init,
	.power_on	= exynos_mipi_phy_power_on,
	.power_off	= exynos_mipi_phy_power_off,
	.owner		= THIS_MODULE,
};

static int exynos_mipi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	struct exynos_mipi_phy *state;
	struct phy_provider *phy_provider;
	struct mipi_phy_data *phy_data;
	const struct of_device_id *of_id;
	unsigned int iso[EXYNOS_MIPI_PHYS_NUM];
	unsigned int rst[EXYNOS_MIPI_PHYS_NUM];
	unsigned int i, elements;
	int ret = 0;

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	state->dev  = &pdev->dev;
	state->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(state->regs))
		return PTR_ERR(state->regs);

	of_id = of_match_device(of_match_ptr(exynos_mipi_phy_of_table), dev);
	if (!of_id)
		return -EINVAL;

	phy_data = (struct mipi_phy_data *)of_id->data;
	phy_m4s4_count = 0;

	dev_set_drvdata(dev, state);
	spin_lock_init(&state->slock);

	state->reg_pmu = syscon_regmap_lookup_by_phandle(node,
						   "samsung,pmu-syscon");
	if (IS_ERR(state->reg_pmu)) {
		dev_err(dev, "Failed to lookup PMU regmap\n");
		return PTR_ERR(state->reg_pmu);
	}

	elements = of_property_count_u32_elems(node, "isolation");
	ret = of_property_read_u32_array(node, "isolation", iso,
					elements);
	if (ret) {
		dev_err(dev, "cannot get mipi-phy isolation!!!\n");
		return ret;
	}

	ret = of_property_read_u32_array(node, "reset", rst,
					elements);
	if (ret) {
		dev_err(dev, "cannot get mipi-phy reset!!!\n");
		return ret;
	}

	for (i = 0; i < elements; i++) {
		state->phys[i].iso_offset = iso[i];
		state->phys[i].rst_bit    = rst[i];
		dev_dbg(dev, "%s: iso 0x%x, reset %d\n", __func__,
			state->phys[i].iso_offset, state->phys[i].rst_bit);
	}

	for (i = 0; i < elements; i++) {
		struct phy *generic_phy = devm_phy_create(dev, NULL,
				&exynos_mipi_phy_ops, NULL);
		if (IS_ERR(generic_phy)) {
			dev_err(dev, "failed to create PHY\n");
			return PTR_ERR(generic_phy);
		}

		state->phys[i].index	= i;
		state->phys[i].phy	= generic_phy;
		state->phys[i].type	= phy_data->type;
		if (i == 0)
			state->phys[i].flags	= phy_data->flags;
		else /* 0 index only can support MIPI_PHY_MxSx_SHARED */
			state->phys[i].flags	= MIPI_PHY_MxSx_UNIQUE;
		phy_set_drvdata(generic_phy, &state->phys[i]);
	}

	phy_provider = devm_of_phy_provider_register(dev,
			exynos_mipi_phy_of_xlate);

	if (IS_ERR(phy_provider))
		dev_err(dev, "failed to create exynos mipi-phy\n");
	else
		dev_err(dev, "Creating exynos-mipi-phy\n");

	return PTR_ERR_OR_ZERO(phy_provider);
}

static int exynos_mipi_phy_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s, successfully removed\n", __func__);
	return 0;
}

static struct platform_driver exynos_mipi_phy_driver = {
	.probe	= exynos_mipi_phy_probe,
	.remove	= exynos_mipi_phy_remove,
	.driver = {
		.name  = "exynos-mipi-phy",
		.of_match_table = of_match_ptr(exynos_mipi_phy_of_table),
		.suppress_bind_attrs = true,
	}
};
module_platform_driver(exynos_mipi_phy_driver);

MODULE_DESCRIPTION("Samsung EXYNOS SoC MIPI CSI/DSI PHY driver");
MODULE_LICENSE("GPL v2");
