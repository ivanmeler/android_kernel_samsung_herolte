/*
 * s2mpu03.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/pinctrl-samsung.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mpu03.h>
#include <mach/regs-pmu.h>
#include <linux/io.h>

static struct s2mpu03_info *static_info;

struct s2mpu03_info {
	struct regulator_dev *rdev[S2MPU03_REGULATOR_MAX];
	unsigned int opmode[S2MPU03_REGULATOR_MAX];
	int num_regulators;
	struct sec_pmic_dev *iodev;
	bool g3d_en;
	int g3d_pin;
	const char *g3d_en_addr;
	const char *g3d_en_pin;
};

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mpu03_info *s2mpu03 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret, id = rdev_get_id(rdev);

	switch (mode) {
	case SEC_OPMODE_SUSPEND:		/* ON in Standby Mode */
		val = 0x1 << S2MPU03_ENABLE_SHIFT;
		break;
	case SEC_OPMODE_ON:			/* ON in Normal Mode */
		val = 0x3 << S2MPU03_ENABLE_SHIFT;
		break;
	default:
		pr_warn("%s: regulator_suspend_mode : 0x%x not supported\n",
			rdev->desc->name, mode);
		return -EINVAL;
	}

	ret = sec_reg_update(s2mpu03->iodev, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
	if (ret)
		return ret;

	s2mpu03->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpu03_info *s2mpu03 = rdev_get_drvdata(rdev);

	int reg_id = rdev_get_id(rdev);

	/* disregard BUCK4 enable */
	if (reg_id == S2MPU03_BUCK4 && s2mpu03->g3d_en &&
		gpio_is_valid(s2mpu03->g3d_pin))
		return 0;

	return sec_reg_update(s2mpu03->iodev, rdev->desc->enable_reg,
				  s2mpu03->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpu03_info *s2mpu03 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int val;

	/* disregard BUCK4 disable */
	if (reg_id == S2MPU03_BUCK4 && s2mpu03->g3d_en &&
		gpio_is_valid(s2mpu03->g3d_pin))
		return 0;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return sec_reg_update(s2mpu03->iodev, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpu03_info *s2mpu03 = rdev_get_drvdata(rdev);
	int ret, reg_id = rdev_get_id(rdev);
	unsigned int val;
	/* BUCK4 is controlled by g3d gpio */
	if (reg_id == S2MPU03_BUCK4 && s2mpu03->g3d_en &&
		gpio_is_valid(s2mpu03->g3d_pin)) {
		if ((__raw_readl(EXYNOS_PMU_G3D_STATUS) &
			LOCAL_PWR_CFG) == LOCAL_PWR_CFG)
			return 1;
		else
			return 0;
	} else {
		ret = sec_reg_read(s2mpu03->iodev, rdev->desc->enable_reg,
				&val);
		if (ret)
			return ret;
	}

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
}

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static int s2m_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct s2mpu03_info *s2mpu03 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	int ramp_mask = 0x03;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay/1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPU03_BUCK1:
	case S2MPU03_BUCK2:
		ramp_shift = 6;
		break;
	case S2MPU03_BUCK3:
	case S2MPU03_BUCK4:
		ramp_shift = 4;
		break;
	case S2MPU03_BUCK5:
	case S2MPU03_BUCK6:
		ramp_shift = 2;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mpu03->iodev, S2MPU03_REG_BUCK_RAMP,
			  ramp_value << ramp_shift, ramp_mask << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpu03_info *s2mpu03 = rdev_get_drvdata(rdev);
	int ret;
	unsigned int val;
	ret = sec_reg_read(s2mpu03->iodev, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu03_info *s2mpu03 = rdev_get_drvdata(rdev);
	int ret;

	ret = sec_reg_update(s2mpu03->iodev, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = sec_reg_update(s2mpu03->iodev, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
					unsigned sel)
{
	int ret;
	struct s2mpu03_info *s2mpu03 = rdev_get_drvdata(rdev);

	ret = sec_reg_write(s2mpu03->iodev, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = sec_reg_update(s2mpu03->iodev, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	unsigned int ramp_delay = 0;
	int old_volt, new_volt;

	if (rdev->constraints->ramp_delay)
		ramp_delay = rdev->constraints->ramp_delay;
	else if (rdev->desc->ramp_delay)
		ramp_delay = rdev->desc->ramp_delay;

	if (ramp_delay == 0) {
		pr_warn("%s: ramp_delay not set\n", rdev->desc->name);
		return -EINVAL;
	}

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, ramp_delay);

	return 0;
}

void g3d_pin_config_set()
{
	if (!static_info->g3d_en || !gpio_is_valid(static_info->g3d_pin)) {
		pr_warn("%s: g3d pin ctrl failed\n", __func__);
		return;
	}

	if (!static_info->g3d_en_addr || !static_info->g3d_en_pin) {
		pr_warn("%s: g3d pin ctrl gpio is invalid\n", __func__);
		return;
	}

	pin_config_set(static_info->g3d_en_addr, static_info->g3d_en_pin,
			PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
}
EXPORT_SYMBOL_GPL(g3d_pin_config_set);

static struct regulator_ops s2mpu03_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
};

static struct regulator_ops s2mpu03_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap_buck,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
	.set_ramp_delay		= s2m_set_ramp_delay,
};

#define _BUCK(macro)	S2MPU03_BUCK##macro
#define _buck_ops(num)	s2mpu03_buck_ops##num

#define _LDO(macro)	S2MPU03_LDO##macro
#define _REG(ctrl)	S2MPU03_REG##ctrl
#define _ldo_ops(num)	s2mpu03_ldo_ops##num
#define _TIME(macro)	S2MPU03_ENABLE_TIME##macro

#define BUCK_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU03_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU03_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU03_ENABLE_MASK,			\
	.enable_time	= t					\
}

#define LDO_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU03_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU03_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU03_ENABLE_MASK,			\
	.enable_time	= t					\
}

static struct regulator_desc regulators[S2MPU03_REGULATOR_MAX] = {
	/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1),
			_REG(_B1CTRL2), _REG(_B1CTRL1), _TIME(_BUCK1)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1),
			_REG(_B2CTRL2), _REG(_B2CTRL1), _TIME(_BUCK2)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1),
			_REG(_B3CTRL2), _REG(_B3CTRL1), _TIME(_BUCK3)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1),
			_REG(_B4CTRL2), _REG(_B4CTRL1), _TIME(_BUCK4)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2),
			_REG(_B5CTRL2), _REG(_B5CTRL1), _TIME(_BUCK5)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2),
			_REG(_B6CTRL2), _REG(_B6CTRL1), _TIME(_BUCK6)),
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1),
			_REG(_L1CTRL), _REG(_L1CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L2CTRL), _REG(_L2CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1),
			_REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1),
			_REG(_L5CTRL), _REG(_L5CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP1),
			_REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2),
			_REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO)),
	LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L24CTRL), _REG(_L24CTRL), _TIME(_LDO)),
	LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L25CTRL), _REG(_L25CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L26CTRL), _REG(_L26CTRL), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L27CTRL), _REG(_L27CTRL), _TIME(_LDO)),
	LDO_DESC("LDO28", _LDO(28), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L28CTRL), _REG(_L28CTRL), _TIME(_LDO)),
	LDO_DESC("LDO29", _LDO(29), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L29CTRL), _REG(_L29CTRL), _TIME(_LDO)),
	LDO_DESC("LDO30", _LDO(30), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L30CTRL), _REG(_L30CTRL), _TIME(_LDO)),
	LDO_DESC("LDO31", _LDO(31), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L31CTRL), _REG(_L31CTRL), _TIME(_LDO)),
	LDO_DESC("LDO32", _LDO(32), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L32CTRL), _REG(_L32CTRL), _TIME(_LDO)),
	LDO_DESC("LDO33", _LDO(33), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L33CTRL), _REG(_L33CTRL), _TIME(_LDO)),
	LDO_DESC("LDO34", _LDO(34), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L34CTRL), _REG(_L34CTRL), _TIME(_LDO)),
	LDO_DESC("LDO35", _LDO(35), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L35CTRL), _REG(_L35CTRL), _TIME(_LDO)),
	LDO_DESC("LDO36", _LDO(36), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L36CTRL), _REG(_L36CTRL), _TIME(_LDO)),
	LDO_DESC("LDO37", _LDO(37), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L37CTRL), _REG(_L37CTRL), _TIME(_LDO)),
	LDO_DESC("LDO38", _LDO(38), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP2),
			_REG(_L38CTRL), _REG(_L38CTRL), _TIME(_LDO)),
	LDO_DESC("LDO39", _LDO(39), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2),
			_REG(_L39CTRL), _REG(_L39CTRL), _TIME(_LDO)),
};

#ifdef CONFIG_OF
static int s2mpu03_pmic_dt_parse_pdata(struct sec_pmic_dev *iodev,
					struct sec_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct sec_regulator_data *rdata;
	unsigned int i;
	int ret;
	u32 val;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
#if 0
	/* get 1 gpio values */
	if (of_gpio_count(pmic_np) < 1) {
		dev_err(iodev->dev, "could not find pmic gpios\n");
		return -EINVAL;
	}
	pdata->g3d_pin = of_get_gpio(pmic_np, 0);
#endif

	ret = of_property_read_u32(pmic_np, "g3d_en", &val);
	if (ret)
		return -EINVAL;
	pdata->g3d_en = !!val;

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev,
			"could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++)
			if (!of_node_cmp(reg_np->name,
					regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
			"don't know how to configure regulator %s\n",
			reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(
						iodev->dev, reg_np);
		rdata->reg_node = reg_np;
		rdata++;
	}

	return 0;
}
#else
static int s2mpu03_pmic_dt_parse_pdata(struct sec_pmic_dev *iodev,
					struct sec_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpu03_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpu03_info *s2mpu03;
	int i, ret;

	ret = sec_reg_read(iodev, S2MPU03_REG_ID, &SEC_PMIC_REV(iodev));
	if (ret < 0)
		return ret;

	if (iodev->dev->of_node) {
		ret = s2mpu03_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu03 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu03_info),
				GFP_KERNEL);
	if (!s2mpu03)
		return -ENOMEM;

	s2mpu03->iodev = iodev;

	s2mpu03->g3d_en_pin =
		(const char *)of_get_property(iodev->dev->of_node,
						"buck4en_pin", NULL);
	s2mpu03->g3d_en_addr =
		(const char *)of_get_property(iodev->dev->of_node,
						"buck4en_addr", NULL);

	static_info = s2mpu03;

	s2mpu03->g3d_en = pdata->g3d_en;

	platform_set_drvdata(pdev, s2mpu03);

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.regmap = iodev->regmap;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mpu03;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpu03->opmode[id] = regulators[id].enable_mask;

		s2mpu03->rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(s2mpu03->rdev[i])) {
			ret = PTR_ERR(s2mpu03->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mpu03->rdev[i] = NULL;
			goto err;
		}
	}

	s2mpu03->num_regulators = pdata->num_regulators;

	if (gpio_is_valid(pdata->g3d_pin)) {
		ret = devm_gpio_request(&pdev->dev, pdata->g3d_pin,
					"S2MPU03 G3D_PIN");
		if (ret < 0)
			return ret;

		s2mpu03->g3d_pin = pdata->g3d_pin;

		if (pdata->g3d_en) {
			g3d_pin_config_set();
			/* for buck4 gpio control, disable i2c control */
			ret = sec_reg_update(iodev, S2MPU03_REG_B4CTRL1,
						0x00, 0xC0);
			if (ret) {
				dev_err(&pdev->dev, "buck4 gpio ctrl err\n");
				return ret;
			}
			/* sync buck4 and ldo10 */
			ret = sec_reg_update(iodev, S2MPU03_REG_LDO_EFUSE,
						0x10, 0x10);
			if (ret) {
				dev_err(&pdev->dev, "regulator sync en err\n");
				return ret;
			}
			/* sync buck4 and ldo10 */
			ret = sec_reg_update(iodev, S2MPU03_REG_L10CTRL,
						0x00, 0xC0);
			if (ret) {
				dev_err(&pdev->dev, "regulator sync en err\n");
				return ret;
			}
		}
	} else
		dev_err(&pdev->dev, "g3d pin is not valid\n");


	/* SELMIF : Buck3,LDO4,6,7,8 controlled by PWREN_MIF */
	ret = sec_reg_update(iodev, S2MPU03_REG_SELMIF, 0x3D, 0x7F);
	if (ret) {
		dev_err(&pdev->dev, "set Buck3,LDO4,6,7,8 controlled by PWREN_MIF : error\n");
		return ret;
	}
	/* RTC Low jitter mode */
	ret = sec_reg_update(iodev, S2MPU03_REG_RTC_BUF, 0x10, 0x10);
	if (ret) {
		dev_err(&pdev->dev, "set low jitter mode i2c write error\n");
		return ret;
	}

	/* On-Sequence Config for PWREN, PWREN_MIF, RF_PWRON, CP_PWR_DOWN*/
	sec_reg_update(iodev, 0x5E, 0x13, 0x1F); /* SEQ_CTRL[4:0] */
	sec_reg_write(iodev, 0x5F, 0x54);        /* Seq. Buck2, Buck1 */
	sec_reg_write(iodev, 0x60, 0x70);        /* Seq. Buck4, Buck3 */
	sec_reg_write(iodev, 0x61, 0x01);        /* Seq. Buck6, Buck5 */
	sec_reg_write(iodev, 0x62, 0x23);        /* Seq. LDO1, Buck7 */
	sec_reg_write(iodev, 0x63, 0xCF);        /* Seq. LDO3, LDO2 */
	sec_reg_write(iodev, 0x64, 0xD3);        /* Seq. LDO5, LDO4 */
	sec_reg_write(iodev, 0x65, 0x12);        /* Seq. LDO7, LDO6 */
	sec_reg_write(iodev, 0x66, 0x25);        /* Seq. LDO9, LDO8 */
	sec_reg_write(iodev, 0x67, 0x43);        /* Seq. LDO11,LDO10 */
	sec_reg_write(iodev, 0x68, 0x65);        /* Seq. LDO13,LDO12 */
	sec_reg_write(iodev, 0x69, 0x87);        /* Seq. LDO15,LDO14 */
	sec_reg_write(iodev, 0x6A, 0x00);        /* Seq. LDO17,LDO16 */
	sec_reg_write(iodev, 0x6B, 0x21);        /* Seq. LDO19,LDO18 */
	sec_reg_write(iodev, 0x6C, 0x43);        /* Seq. LDO21,LDO20 */
	sec_reg_write(iodev, 0x6D, 0x22);        /* Seq. LDO23,LDO22 */
	sec_reg_write(iodev, 0x6E, 0x22);        /* Seq. LDO25,LDO24 */
	sec_reg_write(iodev, 0x6F, 0x22);        /* Seq. LDO27,LDO26 */
	sec_reg_write(iodev, 0x70, 0x22);        /* Seq. LDO29,LDO28 */
	sec_reg_write(iodev, 0x71, 0x22);        /* Seq. LDO31,LDO30 */
	sec_reg_write(iodev, 0x72, 0x22);        /* Seq. LDO33,LDO32 */
	sec_reg_write(iodev, 0x73, 0x22);        /* Seq. LDO35,LDO34 */
	sec_reg_write(iodev, 0x74, 0x22);        /* Seq. LDO37,LDO36 */
	sec_reg_write(iodev, 0x75, 0x22);        /* Seq. LDO39,LDO38 */
	sec_reg_write(iodev, 0x76, 0x40);        /* Seq. LDO1, Buck7~1 */
	sec_reg_write(iodev, 0x77, 0x80);        /* Seq. LDO9~2 */
	sec_reg_write(iodev, 0x78, 0x3E);        /* Seq. LDO17~10 */
	sec_reg_write(iodev, 0x79, 0xF0);        /* Seq. LDO25~18 */
	sec_reg_write(iodev, 0x7A, 0xFF);        /* Seq. LDO33~26 */
	sec_reg_update(iodev, 0x7B, 0x3F, 0x3F); /* Seq. LDO39~34 */

	return 0;

err:
	for (i = 0; i < S2MPU03_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu03->rdev[i]);

	return ret;
}

static int s2mpu03_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu03_info *s2mpu03 = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < S2MPU03_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu03->rdev[i]);

	return 0;
}

static const struct platform_device_id s2mpu03_pmic_id[] = {
	{ "s2mpu03-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu03_pmic_id);

static struct platform_driver s2mpu03_pmic_driver = {
	.driver = {
		.name = "s2mpu03-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mpu03_pmic_probe,
	.remove = s2mpu03_pmic_remove,
	.id_table = s2mpu03_pmic_id,
};

static int __init s2mpu03_pmic_init(void)
{
	return platform_driver_register(&s2mpu03_pmic_driver);
}
subsys_initcall(s2mpu03_pmic_init);

static void __exit s2mpu03_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu03_pmic_driver);
}
module_exit(s2mpu03_pmic_exit);

/* Module information */
MODULE_AUTHOR("Samsung LSI");
MODULE_DESCRIPTION("SAMSUNG S2MPU03 Regulator Driver");
MODULE_LICENSE("GPL");
