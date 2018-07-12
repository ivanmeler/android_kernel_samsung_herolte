/*
 * s2mu003_regulator.c - Regulator driver for the Samsung s2mu003
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/samsung/s2mu003.h>
#include <linux/mfd/samsung/s2mu003-private.h>
#include <linux/regulator/of_regulator.h>

struct s2mu003_regulator_info {
	s2mu003_mfd_chip_t *iodev;
	int num_regulators;
	struct regulator_dev *rdev[S2MU003_REGULATOR_MAX];
	int opmode[S2MU003_REGULATOR_MAX];
};

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mu003_regulator_info *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c_client;

	return s2mu003_assign_bits(i2c, rdev->desc->enable_reg,
				  rdev->desc->enable_mask,
				  info->opmode[rdev_get_id(rdev)]);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mu003_regulator_info *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c_client;
	u8 val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mu003_assign_bits(i2c, rdev->desc->enable_reg,
				   rdev->desc->enable_mask, val);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mu003_regulator_info *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c_client;
	int ret;
	u8 val;

	ret = s2mu003_reg_read(i2c, rdev->desc->enable_reg);
	if (ret < 0)
		return ret;
	val = (u8)ret;

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mu003_regulator_info *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c_client;
	int ret;
	u8 val;

	ret = s2mu003_reg_read(i2c, rdev->desc->vsel_reg);
	if (ret < 0)
		return ret;
	val = (u8)ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mu003_regulator_info *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c_client;
	int ret;

	ret = s2mu003_assign_bits(i2c, rdev->desc->vsel_reg,
				rdev->desc->vsel_mask, sel);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mu003_assign_bits(i2c, rdev->desc->apply_reg,
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
	struct s2mu003_regulator_info *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c_client;
	int ret;

	ret = s2mu003_assign_bits(i2c, rdev->desc->vsel_reg,
				rdev->desc->vsel_mask, sel);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mu003_assign_bits(i2c, rdev->desc->apply_reg,
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
	int old_volt, new_volt;

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, S2MU003_RAMP_DELAY);

	return 0;
}

static struct regulator_ops s2mu003_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
};

static struct regulator_ops s2mu003_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap_buck,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
};

static struct regulator_desc regulators[S2MU003_REGULATOR_MAX] = {
		/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
	{
		.name	= "s2mu003-ldo1",
		.id	= S2MU003_CAMLDO,
		.ops	= &s2mu003_ldo_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
		.min_uV	= S2MU003_LDO_MIN1,
		.uV_step = S2MU003_LDO_STEP1,
		.n_voltages	= S2MU003_CAMLDO_N_VOLTAGES,
		.vsel_reg	= S2MU003_LDO_CTRL,
		.vsel_mask	= S2MU003_CAMLDO_VSEL_MASK,
		.enable_reg	= S2MU003_Buck_LDO_CTRL,
		.enable_mask	= S2MU003_CAMLDO_ENABLE_MASK,
		.enable_time	= S2MU003_ENABLE_TIME_LDO,
	},
	{
		.name	= "s2mu003-buck1",
		.id	= S2MU003_BUCK,
		.ops	= &s2mu003_buck_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
		.min_uV	= S2MU003_BUCK_MIN1,
		.uV_step = S2MU003_BUCK_STEP1,
		.n_voltages	= S2MU003_BUCK_N_VOLTAGES,
		.vsel_reg	= S2MU003_Buck_CTRL,
		.vsel_mask	= S2MU003_BUCK_VSEL_MASK,
		.enable_reg	= S2MU003_Buck_LDO_CTRL,
		.enable_mask	= S2MU003_BUCK_ENABLE_MASK,
		.enable_time	= S2MU003_ENABLE_TIME_BUCK,
	},
};

#ifdef CONFIG_OF
static int s2mu003_pmic_dt_parse_pdata(s2mu003_mfd_chip_t *iodev,
					s2mu003_mfd_platform_data_t *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	s2mu003_regulator_platform_data_t *rdata;
	unsigned int i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

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
	of_node_put(regulators_np);

	return 0;
}
#else
static int s2mu003_pmic_dt_parse_pdata(s2mu003_mfd_chip_t *iodev,
					s2mu003_mfd_platform_data_t *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mu003_pmic_probe(struct platform_device *pdev)
{
	s2mu003_mfd_chip_t *iodev = dev_get_drvdata(pdev->dev.parent);
	s2mu003_mfd_platform_data_t *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mu003_regulator_info *s2mu003;
	struct i2c_client *i2c;
	int i, ret;

	dev_info(&pdev->dev, "%s\n", __func__);

	if (!pdata) {
		pr_info("[%s:%d] !pdata\n", __FILE__, __LINE__);
		dev_err(pdev->dev.parent, "No platform init data supplied.\n");
		return -ENODEV;
	}

	if (iodev->dev->of_node) {
		ret = s2mu003_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	s2mu003 = devm_kzalloc(&pdev->dev, sizeof(
				struct s2mu003_regulator_info),
				GFP_KERNEL);
	if (!s2mu003) {
		pr_info("[%s:%d] if (!s2mu003)\n", __FILE__, __LINE__);
		return -ENOMEM;
	}

	s2mu003->iodev = iodev;
	s2mu003->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, s2mu003);
	i2c = s2mu003->iodev->i2c_client;
	pr_info("[%s:%d] pdata->num_regulators:%d\n", __FILE__, __LINE__,
		pdata->num_regulators);

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mu003;
		config.of_node = pdata->regulators[i].reg_node;
		s2mu003->opmode[id] = regulators[id].enable_mask;
		s2mu003->rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(s2mu003->rdev[i])) {
			ret = PTR_ERR(s2mu003->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				id);
			s2mu003->rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
 err:
	pr_info("[%s:%d] err:\n", __FILE__, __LINE__);
	for (i = 0; i < s2mu003->num_regulators; i++)
		if (s2mu003->rdev[i])
			regulator_unregister(s2mu003->rdev[i]);

	kfree(s2mu003);

	return ret;
}

static int s2mu003_pmic_remove(struct platform_device *pdev)
{
	struct s2mu003_regulator_info *s2mu003 = platform_get_drvdata(pdev);
	int i;
	dev_info(&pdev->dev, "%s\n", __func__);
	for (i = 0; i < s2mu003->num_regulators; i++)
		if (s2mu003->rdev[i])
			regulator_unregister(s2mu003->rdev[i]);

	kfree(s2mu003->rdev);
	kfree(s2mu003);

	return 0;
}

static const struct platform_device_id s2mu003_pmic_id[] = {
	{"s2mu003-regulator", 0},
	{},
};
MODULE_DEVICE_TABLE(platform, s2mu003_pmic_id);

static struct platform_driver s2mu003_pmic_driver = {
	.driver = {
		   .name = "s2mu003-regulator",
		   .owner = THIS_MODULE,
		   },
	.probe = s2mu003_pmic_probe,
	.remove = s2mu003_pmic_remove,
	.id_table = s2mu003_pmic_id,
};

static int __init s2mu003_pmic_init(void)
{
	return platform_driver_register(&s2mu003_pmic_driver);
}
subsys_initcall(s2mu003_pmic_init);

static void __exit s2mu003_pmic_cleanup(void)
{
	platform_driver_unregister(&s2mu003_pmic_driver);
}

module_exit(s2mu003_pmic_cleanup);

MODULE_DESCRIPTION("SAMSUNG s2mu003 Regulator Driver");
MODULE_LICENSE("GPL");
