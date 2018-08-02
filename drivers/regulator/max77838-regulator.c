/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/driver.h>
#include <linux/regmap.h>
#include <linux/regulator/max77838-regulator.h>
#include <linux/module.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>

#define M2SH	__CONST_FFS

/* Slave Address */
#define MAX77838_I2C_ADDR	(0xC0>>1)

/* Register */
#define REG_DEVICE_ID		0x00
#define BIT_VERSION		BITS(6, 3)
#define BIT_CHIP_REV		BITS(2, 0)

#define REG_TOPSYS_STAT		0x01
#define BIT_TJCT_140C		BIT(1)
#define BIT_TJCT_120C		BIT(0)

#define REG_REG_STAT		0x02
#define BIT_ELVDD_POKn		BIT(7)
#define BIT_ELVSS_POKn		BIT(6)
#define BIT_AVDD_POKn		BIT(5)
#define BIT_BUCK_POKn		BIT(4)
#define BIT_LDO4_POKn		BIT(3)
#define BIT_LDO3_POKn		BIT(2)
#define BIT_LDO2_POKn		BIT(1)
#define BIT_LDO1_POKn		BIT(0)

#define REG_REG_EN		0x03
#define BIT_B_EN		BIT(4)
#define BIT_L4_EN		BIT(3)
#define BIT_L3_EN		BIT(2)
#define BIT_L2_EN		BIT(1)
#define BIT_L1_EN		BIT(0)

#define REG_GPIO_PD_CTRL	0x04
#define BIT_EN_B_PD_DIS		BIT(0)

#define REG_UVLO_CFG1		0x05
#define BIT_UVLO_F		BITS(1, 0)

#define REG_I2C_CFG		0x0C
#define BIT_PAIR		BITS(7, 4)
#define BIT_GC_EN		BIT(2)
#define BIT_HS_EN		BIT(0)

#define REG_LDO1_CFG		0x10
#define REG_LDO2_CFG		0x11
#define REG_LDO3_CFG		0x12
#define REG_LDO4_CFG		0x13
#define BIT_LX_AD		BIT(7)
#define BIT_LX_VOUT		BITS(6, 0)

#define REG_LDOX_CFG(X)		(REG_LDO1_CFG + X - 1)

#define REG_BUCK_CFG1		0x20
#define BIT_B_RU_SR		BITS(7, 6)
#define BIT_B_RD_SR		BITS(5, 4)
#define BIT_B_AD		BIT(3)
#define BIT_B_FPWM		BIT(2)
#define BIT_B_FSRAD		BIT(0)

#define REG_BUCK_VOUT		0x21
#define BIT_B_VOUT		BITS(7, 0)

/* Undefined register address value */
#define REG_RESERVED		0xFF

/* define H/W default value */
#define HW_DEFAULT_VALUE	0xFF

/* voltage range and step */
#define PMOS_MINUV			600000		/* 0.6V */
#define PMOS_MAXUV			3775000		/* 3.775V */
#define PMOS_STEP			25000		/* 25mV */

#define BUCK_MINUV			500000		/* 0.5V */
#define BUCK_MAXUV			2093750		/* 2.09375V */
#define BUCK_STEP			6250		/* 6.25mV */

static const struct regmap_config max77838_regmap_config = {
	.reg_bits   = 8,
	.val_bits   = 8,
	.cache_type = REGCACHE_NONE,
};

struct max77838_reg {
	struct device *dev;
	struct regulator_dev *rdev[MAX77838_LDO_MAX];
	int num_regulators;
	struct regmap *regmap;
};

static struct regulator_ops max77838_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled			= regulator_is_enabled_regmap,
	.enable				= regulator_enable_regmap,
	.disable			= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
};

#define REGULATOR_DESC_PLDO(num) {				\
	.name		= "LDO"#num,					\
	.id			= MAX77838_LDO##num,			\
	.ops		= &max77838_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,					\
	.n_voltages	= BIT_LX_VOUT + 1,				\
	.min_uV		= PMOS_MINUV,					\
	.uV_step	= PMOS_STEP,					\
	.vsel_reg	= REG_LDO##num##_CFG,			\
	.vsel_mask	= BIT_LX_VOUT,					\
	.enable_reg	= REG_REG_EN,					\
	.enable_mask	= BIT_L##num##_EN,			\
}

#define REGULATOR_DESC_BUCK() {					\
	.name		= "BUCK",						\
	.id		= MAX77838_BUCK,				\
	.ops		= &max77838_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,					\
	.n_voltages	= BIT_B_VOUT + 1,				\
	.min_uV		= BUCK_MINUV,					\
	.uV_step	= BUCK_STEP,					\
	.vsel_reg	= REG_BUCK_VOUT,				\
	.vsel_mask	= BIT_B_VOUT,					\
	.enable_reg	= REG_REG_EN,					\
	.enable_mask	= BIT_B_EN,					\
}

const static struct regulator_desc max77838_reg_desc[] = {
	REGULATOR_DESC_PLDO(1),
	REGULATOR_DESC_PLDO(2),
	REGULATOR_DESC_PLDO(3),
	REGULATOR_DESC_PLDO(4),
	REGULATOR_DESC_BUCK(),
};

static int max77838_reg_set_active_discharge(struct max77838_reg *max77838_reg,
		struct max77838_regulator_platform_data *pdata, int reg_id)
{
	int rc, reg, val;

	switch (reg_id) {
	case MAX77838_LDO1 ... MAX77838_LDO4:
		val = pdata->regulators[reg_id - 1].active_discharge_enable ? BIT_LX_AD : 0;
		reg = REG_LDOX_CFG(reg_id);
		rc = regmap_update_bits(max77838_reg->regmap, reg, BIT_LX_AD, val);
		break;
	case MAX77838_BUCK:
		val = pdata->regulators[reg_id - 1].active_discharge_enable ? BIT_B_AD : 0;
		reg = REG_BUCK_CFG1;
		rc = regmap_update_bits(max77838_reg->regmap, reg, BIT_B_AD, val);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int max77838_reg_hw_init(struct max77838_reg *max77838_reg,
			struct max77838_regulator_platform_data *pdata)
{
	int rc, reg, val;

	/* buck configuration */
	reg = REG_BUCK_CFG1;
	val = pdata->buck_ramp_up<<M2SH(BIT_B_RU_SR) | pdata->buck_ramp_down<<M2SH(BIT_B_RD_SR) |
		pdata->buck_fpwm<<M2SH(BIT_B_FPWM) | pdata->buck_fsrad<<M2SH(BIT_B_FSRAD);
	rc = regmap_update_bits(max77838_reg->regmap, reg,
		BIT_B_RU_SR | BIT_B_RD_SR | BIT_B_FPWM | BIT_B_FSRAD, val);
	if (rc != 0)
		return rc;

	/* uvlo falling threshold */
	reg = REG_UVLO_CFG1;
	val = pdata->uvlo_fall_threshold<<M2SH(BIT_UVLO_F);
	rc = regmap_write(max77838_reg->regmap, reg, val);
	if (rc != 0)
		return rc;
	
	rc = regmap_read(max77838_reg->regmap, reg, &val);
	pr_info("UVLO_CFG = 0x%02x\n", val);

	return rc;
}


#ifdef CONFIG_OF
static struct max77838_regulator_platform_data *max77838_reg_parse_dt(struct device *dev)
{
	struct device_node *nproot = dev->of_node;
	struct device_node *regulators_np, *reg_np;
	struct max77838_regulator_platform_data *pdata;
	struct max77838_regulator_data *rdata;
	int i;
	int ret;

	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	regulators_np = of_find_node_by_name(nproot, "regulators");
	if (unlikely(regulators_np == NULL)) {
		dev_err(dev, "regulators node not found\n");
		return ERR_PTR(-EINVAL);
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = of_get_child_count(regulators_np);

	rdata = devm_kzalloc(dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		of_node_put(regulators_np);
		dev_err(dev, "could not allocate memory for regulator data\n");
		return ERR_PTR(-ENOMEM);
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(max77838_reg_desc); i++)
			if (!of_node_cmp(reg_np->name, max77838_reg_desc[i].name))
				break;

		if (i == ARRAY_SIZE(max77838_reg_desc)) {
			dev_warn(dev, "don't know how to configure regulator %s\n",
				 reg_np->name);
			continue;
		}

		rdata->initdata = of_get_regulator_init_data(dev, reg_np);
		rdata->of_node = reg_np;
		ret = of_property_read_u32(reg_np, "active-discharge-enable",	&rdata->active_discharge_enable);
		if (ret != 0)
			rdata->active_discharge_enable = 1;
		rdata++;
	}
	of_node_put(regulators_np);

	ret = of_property_read_u32(nproot, "buck-ramp-up", &pdata->buck_ramp_up);
	if (ret != 0)
		pdata->buck_ramp_up = 1;	/* 12.5mV/us */

	ret = of_property_read_u32(nproot, "buck-ramp-down", &pdata->buck_ramp_down);
	if (ret != 0)
		pdata->buck_ramp_down = 1;	/* 6.25mV/us */

	ret = of_property_read_u32(nproot, "buck-fpwm", &pdata->buck_fpwm);
	if (ret != 0)
		pdata->buck_fpwm = 0;		/* turn off FPWM */

	ret = of_property_read_u32(nproot, "buck-fsrad", &pdata->buck_fsrad);
	if (ret != 0)
		pdata->buck_fsrad = 1;		/* enable Active Discharge */

	ret = of_property_read_u32(nproot, "uvlo-fall-threshold",
					&pdata->uvlo_fall_threshold);
	if (ret != 0)
		pdata->uvlo_fall_threshold = 0;	/* 2.3V */

	return pdata;
}
#endif

static int max77838_regulator_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct max77838_regulator_platform_data *pdata;
	struct max77838_reg *max77838_reg;
	struct regulator_config config = { };

	int rc, i;

	pr_info("<%s> probe start\n", client->name);

	max77838_reg = devm_kzalloc(dev, sizeof(*max77838_reg), GFP_KERNEL);
	if (unlikely(!max77838_reg))
		return -ENOMEM;

	i2c_set_clientdata(client, max77838_reg);

#ifdef CONFIG_OF
	pdata = max77838_reg_parse_dt(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

	max77838_reg->dev = &client->dev;

	max77838_reg->regmap = devm_regmap_init_i2c(client, &max77838_regmap_config);
	if (unlikely(IS_ERR(max77838_reg->regmap))) {
		rc = PTR_ERR(max77838_reg->regmap);
		max77838_reg->regmap = NULL;
		pr_err("<%s> failed to initialize i2c regmap pmic [%d]\n", client->name,
			rc);
		goto abort;
	}
	max77838_reg->num_regulators = pdata->num_regulators;

	for (i = 0; i < pdata->num_regulators; i++) {
		config.dev = &client->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].of_node;
		config.regmap = max77838_reg->regmap;
		config.driver_data = max77838_reg;

		max77838_reg->rdev[i] = regulator_register(&max77838_reg_desc[i], &config);

		if (IS_ERR(max77838_reg->rdev[i])) {
			rc = PTR_ERR(max77838_reg->rdev[i]);
			pr_err("<%s> regulator init failed for %d\n", client->name, i);
			max77838_reg->rdev[i] = NULL;
			goto abort;
		}

		rc = max77838_reg_set_active_discharge(max77838_reg, pdata, max77838_reg_desc[i].id);
		if (IS_ERR_VALUE(rc)) {
			pr_err("<%s> fail to set active discharge\n", client->name);
			goto abort;
		}
	}

	/* initialize platform data */
	rc = max77838_reg_hw_init(max77838_reg, pdata);
	if (IS_ERR_VALUE(rc)) {
		pr_err("<%s> fail to initialize platform data\n", client->name);
		goto abort;
	}

	pr_info("<%s> probe end\n", client->name);

	return 0;

abort:
	for (i = 0; i < MAX77838_LDO_MAX; i++)
		regulator_unregister(max77838_reg->rdev[i]);

	i2c_set_clientdata(client, NULL);

	return rc;
}

static int max77838_regulator_remove(struct i2c_client *client)
{
	struct max77838_reg *max77838_reg = i2c_get_clientdata(client);
	int i;
	for (i = 0; i < max77838_reg->num_regulators; i++)
		regulator_unregister(max77838_reg->rdev[i]);

	i2c_set_clientdata(client, NULL);

	if (likely(max77838_reg->regmap))
		regmap_exit(max77838_reg->regmap);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id max77838_of_id[] = {
	{ .compatible = "maxim,max77838"      },
	{ },
};
MODULE_DEVICE_TABLE(of, max77838_of_id);
#endif /* CONFIG_OF */

static const struct i2c_device_id max77838_i2c_id[] = {
	{ "max77838", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max77838_i2c_id);

static struct i2c_driver max77838_i2c_driver = {
	.driver.name            = "max77838",
	.driver.owner           = THIS_MODULE,
#ifdef CONFIG_OF
	.driver.of_match_table  = max77838_of_id,
#endif /* CONFIG_OF */
	.id_table               = max77838_i2c_id,
	.probe                  = max77838_regulator_probe,
	.remove                 = max77838_regulator_remove,
};

static __init int max77838_init(void)
{
	int rc;

	rc = i2c_add_driver(&max77838_i2c_driver);

	if (rc < 0)
		pr_err("Failed to register I2C driver: %d\n", rc);

	return rc;
}
module_init(max77838_init);

static __exit void max77838_exit(void)
{
	i2c_del_driver(&max77838_i2c_driver);
}
module_exit(max77838_exit);

MODULE_ALIAS("platform:max77838-regulator");
MODULE_DESCRIPTION("Regulator driver for MAX77838");
MODULE_AUTHOR("TaiEup Kim<clark.kim@maximintegrated.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

