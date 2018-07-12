/*
 * max77854.c - Regulator driver for the Maxim 77854
 *
 * Copyright (C) 2011 Samsung Electronics
 * Sukdong Kim <sukdong.kim@smasung.com>
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
 * This driver is based on max8997.c
 */

#include <linux/module.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max77854.h>
#include <linux/mfd/max77854-private.h>
#include <linux/regulator/of_regulator.h>

/* MAX77854 regulator IDs */
enum max77854_regulators {
	MAX77854_SAFEOUT1 = 0,
	MAX77854_SAFEOUT2,

	MAX77854_REG_MAX,
};

struct max77854_data {
	struct device *dev;
	struct max77854_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;

	u8 saved_states[MAX77854_REG_MAX];
};

struct voltage_map_desc {
	int min;
	int max;
	int step;
	unsigned int n_bits;
};

/* current map in mA */
static const struct voltage_map_desc charger_current_map_desc = {
	.min = 60, .max = 2580, .step = 20, .n_bits = 7,
};

static const struct voltage_map_desc topoff_current_map_desc = {
	.min = 50, .max = 200, .step = 10, .n_bits = 4,
};

static const struct voltage_map_desc *reg_voltage_map[] = {
	[MAX77854_SAFEOUT1] = NULL,
	[MAX77854_SAFEOUT2] = NULL,
};

int max77854_regulator_update_reg
		(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	u8 val_before = -1, val_after = -1;
	int ret;

	max77854_read_reg(i2c, reg, &val_before);

	ret = max77854_update_reg(i2c, reg, val, mask);
	if (ret)
		pr_err("%s: fail to update reg(%d)\n", __func__, ret);

	max77854_read_reg(i2c, reg, &val_after);

	pr_info("%s: reg(0x%02x): [0x%02x]+[0x%02x]:[mask 0x%02x]->[0x%02x]\n",
		__func__, reg, val_before, val, mask, val_after);

	return ret;
}

static inline int max77854_get_rid(struct regulator_dev *rdev)
{
	if (!rdev)
		return -ENODEV;

	if (!rdev->desc)
		return -ENODEV;

	dev_info(&rdev->dev, "func:%s\n", __func__);
	return rdev_get_id(rdev);
}

static bool max77854_regulator_check_disable(struct regulator_dev *rdev)
{
	int rid = max77854_get_rid(rdev);
	bool ret = false;

	switch(rid) {
	case MAX77854_SAFEOUT1:
		ret = is_muic_usb_path_ap_usb();
		break;
	case MAX77854_SAFEOUT2:
		ret = is_muic_usb_path_cp_usb();
		break;
	default:
		pr_err("%s: invalid value rid(%d)\n", __func__, rid);
		break;
	}

	if (ret) {
		pr_info("%s: cannot disable regulator(%d)\n", __func__, rid);
		return false;
	}

	return true;
}

static int max77854_list_voltage_safeout(struct regulator_dev *rdev,
					 unsigned int selector)
{
	int rid = max77854_get_rid(rdev);
	dev_info(&rdev->dev, "func:%s\n", __func__);
	if (rid == MAX77854_SAFEOUT1 || rid == MAX77854_SAFEOUT2) {
		switch (selector) {
		case 0:
			return 4850000;
		case 1:
			return 4900000;
		case 2:
			return 4950000;
		case 3:
			return 3300000;
		default:
			return -EINVAL;
		}
	}

	return -EINVAL;
}

static int max77854_get_enable_register(struct regulator_dev *rdev,
					int *reg, int *mask, int *pattern)
{
	int rid = max77854_get_rid(rdev);
	dev_info(&rdev->dev, "func:%s\n", __func__);
	switch (rid) {
	case MAX77854_SAFEOUT1...MAX77854_SAFEOUT2:
		*reg = MAX77854_PMIC_REG_SAFEOUT_CTRL;
		*mask = 0x40 << (rid - MAX77854_SAFEOUT1);
		*pattern = 0x40 << (rid - MAX77854_SAFEOUT1);
		break;
	default:
		/* Not controllable or not exists */
		return -EINVAL;
	}

	return 0;
}

static int max77854_get_disable_register(struct regulator_dev *rdev,
					int *reg, int *mask, int *pattern)
{
	int rid = max77854_get_rid(rdev);
	dev_info(&rdev->dev, "func:%s\n", __func__);

	switch (rid) {
	case MAX77854_SAFEOUT1...MAX77854_SAFEOUT2:
		*reg = MAX77854_PMIC_REG_SAFEOUT_CTRL;
		*mask = 0x40 << (rid - MAX77854_SAFEOUT1);
		*pattern = 0x00;
		break;
	default:
		/* Not controllable or not exists */
		return -EINVAL;
	}

	return 0;
}

static int max77854_reg_is_enabled(struct regulator_dev *rdev)
{
	struct max77854_data *max77854 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77854->iodev->i2c;
	int ret, reg, mask, pattern;
	u8 val;
	dev_info(&rdev->dev, "func:%s\n", __func__);
	ret = max77854_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret == -EINVAL)
		return 1;	/* "not controllable" */
	else if (ret)
		return ret;

	ret = max77854_read_reg(i2c, reg, &val);
	if (ret)
		return ret;

	return (val & mask) == pattern;
}

static int max77854_reg_enable(struct regulator_dev *rdev)
{
	struct max77854_data *max77854 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77854->iodev->i2c;
	int ret, reg, mask, pattern;
	dev_info(&rdev->dev, "func:%s\n", __func__);
	ret = max77854_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	return max77854_regulator_update_reg(i2c, reg, pattern, mask);
}

static int max77854_reg_disable(struct regulator_dev *rdev)
{
	struct max77854_data *max77854 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77854->iodev->i2c;
	int ret, reg, mask, pattern;
	dev_info(&rdev->dev, "func:%s\n", __func__);

	if (!max77854_regulator_check_disable(rdev))
		return -EINVAL;

	ret = max77854_get_disable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	return max77854_regulator_update_reg(i2c, reg, pattern, mask);
}

static int max77854_get_voltage_register(struct regulator_dev *rdev,
					 int *_reg, int *_shift, int *_mask)
{
	int rid;
	int reg, shift = 0, mask = 0x3f;

	if (!rdev)
		return -EINVAL;

	rid = max77854_get_rid(rdev);

	dev_info(&rdev->dev, "func:%s\n", __func__);
	switch (rid) {
	case MAX77854_SAFEOUT1...MAX77854_SAFEOUT2:
		reg = MAX77854_PMIC_REG_SAFEOUT_CTRL;
		shift = (rid == MAX77854_SAFEOUT2) ? 2 : 0;
		mask = 0x3;
		break;
	default:
		pr_err("%s: invalid value(%d)\n", __func__, rid);
		return -EINVAL;
	}

	*_reg = reg;
	*_shift = shift;
	*_mask = mask;

	return 0;
}

static int max77854_list_voltage(struct regulator_dev *rdev,
				 unsigned int selector)
{
	const struct voltage_map_desc *desc;
	int rid = max77854_get_rid(rdev);
	int val;
	dev_info(&rdev->dev, "func:%s\n", __func__);

	if (rid >= ARRAY_SIZE(reg_voltage_map) || rid < 0)
		return -EINVAL;

	desc = reg_voltage_map[rid];
	if (desc == NULL)
		return -EINVAL;

	val = desc->min + desc->step * selector;
	if (val > desc->max)
		return -EINVAL;

	return val * 1000;
}

static int max77854_get_voltage(struct regulator_dev *rdev)
{
	struct max77854_data *max77854;
	struct i2c_client *i2c;
	int reg, shift, mask, ret;

	u8 val;

	if (!rdev)
		return -1;

	max77854 = rdev_get_drvdata(rdev);
	i2c = max77854->iodev->i2c;

	dev_info(&rdev->dev, "func:%s\n", __func__);
	ret = max77854_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	ret = max77854_read_reg(i2c, reg, &val);
	if (ret)
		return ret;

	val >>= shift;
	val &= mask;

	if (rdev->desc && rdev->desc->ops && rdev->desc->ops->list_voltage)
		return rdev->desc->ops->list_voltage(rdev, val);

	/*
	 * max77854_list_voltage returns value for any rdev with voltage_map,
	 * which works for "CHARGER" and "CHARGER TOPOFF" that do not have
	 * list_voltage ops (they are current regulators).
	 */
	return max77854_list_voltage(rdev, val);
}

static inline int max77854_get_voltage_proper_val(
		const struct voltage_map_desc *desc,
		int min_vol, int max_vol)
{
	int i = 0;

	if (desc == NULL)
		return -EINVAL;

	if (max_vol < desc->min || min_vol > desc->max)
		return -EINVAL;

	while (desc->min + desc->step * i < min_vol &&
			desc->min + desc->step * i < desc->max)
		i++;

	if (desc->min + desc->step * i > max_vol)
		return -EINVAL;

	if (i >= (1 << desc->n_bits))
		return -EINVAL;

	return i;
}

static const int safeoutvolt[] = {
	3300000,
	4850000,
	4900000,
	4950000,
};

/* For SAFEOUT1 and SAFEOUT2 */
static int max77854_set_voltage_safeout(struct regulator_dev *rdev,
					int min_uV, int max_uV,
					unsigned *selector)
{
	struct max77854_data *max77854 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77854->iodev->i2c;
	int rid = max77854_get_rid(rdev);
	int reg, shift = 0, mask, ret;
	int i = 0;
	u8 val;
	dev_info(&rdev->dev, "func:%s\n", __func__);

	if (rid != MAX77854_SAFEOUT1 && rid != MAX77854_SAFEOUT2)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(safeoutvolt); i++) {
		if (min_uV <= safeoutvolt[i] && max_uV >= safeoutvolt[i])
			break;
	}

	if (i >= ARRAY_SIZE(safeoutvolt))
		return -EINVAL;

	if (i == 0)
		val = 0x3;
	else
		val = i - 1;

	ret = max77854_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	ret = max77854_regulator_update_reg(i2c, reg, val << shift, mask << shift);

	*selector = val;

	return ret;
}

static int max77854_reg_enable_suspend(struct regulator_dev *rdev)
{
	dev_info(&rdev->dev, "func:%s\n", __func__);
	return 0;
}

static int max77854_reg_disable_suspend(struct regulator_dev *rdev)
{
	struct max77854_data *max77854 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77854->iodev->i2c;
	int ret, reg, mask, pattern;
	int rid = max77854_get_rid(rdev);
	dev_info(&rdev->dev, "func:%s\n", __func__);
	ret = max77854_get_disable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	if (rid != MAX77854_SAFEOUT1 && rid != MAX77854_SAFEOUT2)
		return -EINVAL;

	max77854_read_reg(i2c, reg, &max77854->saved_states[rid]);

	dev_dbg(&rdev->dev, "Full Power-Off for %s (%xh -> %xh)\n",
		rdev->desc->name, max77854->saved_states[rid] & mask,
		(~pattern) & mask);
	return max77854_regulator_update_reg(i2c, reg, pattern, mask);
}

static struct regulator_ops max77854_safeout_ops = {
	.list_voltage = max77854_list_voltage_safeout,
	.is_enabled = max77854_reg_is_enabled,
	.enable = max77854_reg_enable,
	.disable = max77854_reg_disable,
	.get_voltage = max77854_get_voltage,
	.set_voltage = max77854_set_voltage_safeout,
	.set_suspend_enable = max77854_reg_enable_suspend,
	.set_suspend_disable = max77854_reg_disable_suspend,
};

static struct regulator_desc regulators[] = {
	{
		.name = "ESAFEOUT1",
		.id = MAX77854_SAFEOUT1,
		.ops = &max77854_safeout_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	}, {
		.name = "ESAFEOUT2",
		.id = MAX77854_SAFEOUT2,
		.ops = &max77854_safeout_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
};

static struct regulator_consumer_supply safeout1_supply[] = {
	REGULATOR_SUPPLY("safeout1", NULL),
};

static struct regulator_consumer_supply safeout2_supply[] = {
	REGULATOR_SUPPLY("safeout2", NULL),
};

#if defined(CONFIG_OF)

#define max77854_init_consumer_supplies(p_data, r_init_data, id)			\
	(r_init_data) = (p_data)->regulators[MAX77854_SAFEOUT##id].initdata;		\
	(r_init_data)->constraints.state_mem.enabled = 1;				\
	(r_init_data)->num_consumer_supplies = ARRAY_SIZE(safeout## id ##_supply);	\
	(r_init_data)->consumer_supplies = safeout## id ##_supply;

static int max77854_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct max77854_platform_data *pdata)
{
	struct max77854_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct max77854_regulator_data *rdata;
	struct regulator_init_data *r_initdata;

	unsigned int i;

	pmic_np = of_node_get(iodev->dev->of_node);
	if (!pmic_np) {
		dev_err(&pdev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(&pdev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;

	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		of_node_put(regulators_np);
		dev_err(&pdev->dev, "could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;

	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++) {
			if (!of_node_cmp(reg_np->name, regulators[i].name))
				break;
		}

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(&pdev->dev, "don't know how to configure regulator %s\n",
				 reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(&pdev->dev, reg_np);
		rdata->reg_node = reg_np;
		rdata++;
	}
	of_node_put(regulators_np);

	/* original shelve source */
	max77854_init_consumer_supplies(pdata, r_initdata, 1);
	max77854_init_consumer_supplies(pdata, r_initdata, 2);

	return 0;
}
#else
static int max77854_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct max77854_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static __devinit int max77854_pmic_probe(struct platform_device *pdev)
{
	struct max77854_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77854_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_config config = { };
	struct regulator_dev **rdev;
	struct max77854_data *max77854;
	struct i2c_client *i2c;
	int i, ret, size;

	dev_info(&pdev->dev, "%s\n", __func__);
	if (!pdata) {
		pr_info("[%s:%d] !pdata\n", __FILE__, __LINE__);
		dev_err(pdev->dev.parent, "No platform init data supplied.\n");
		return -ENODEV;
	}

	if (iodev->dev->of_node) {
		ret = max77854_pmic_dt_parse_pdata(pdev, pdata);
		if (ret)
			return ret;
	}

	max77854 = devm_kzalloc(&pdev->dev, sizeof(struct max77854_data),
				GFP_KERNEL);
	if (!max77854) {
		pr_info("[%s:%d] if (!max77854)\n", __FILE__, __LINE__);
		return -ENOMEM;
	}
	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	max77854->rdev = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!max77854->rdev) {
		pr_info("[%s:%d] if (!max77854->rdev)\n", __FILE__, __LINE__);
		kfree(max77854);
		return -ENOMEM;
	}

	rdev = max77854->rdev;
	max77854->dev = &pdev->dev;
	max77854->iodev = iodev;
	max77854->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, max77854);
	i2c = max77854->iodev->i2c;
	pr_info("[%s:%d] pdata->num_regulators:%d\n", __FILE__, __LINE__,
		pdata->num_regulators);

	for (i = 0; i < pdata->num_regulators; i++) {

		const struct voltage_map_desc *desc;
		int id = pdata->regulators[i].id;
		pr_info("[%s:%d] for in pdata->num_regulators:%d\n", __FILE__,
			__LINE__, pdata->num_regulators);
		desc = reg_voltage_map[id];
		if (id == MAX77854_SAFEOUT1 || id == MAX77854_SAFEOUT2)
			regulators[id].n_voltages = ARRAY_SIZE(safeoutvolt);

		config.dev = max77854->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = max77854;
		config.of_node = pdata->regulators[i].reg_node;

		rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(max77854->dev, "regulator init failed for %d\n",
				id);
			rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
 err:
	pr_info("[%s:%d] err:\n", __FILE__, __LINE__);
	for (i = 0; i < max77854->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(max77854);

	return ret;
}

static int __devexit max77854_pmic_remove(struct platform_device *pdev)
{
	struct max77854_data *max77854 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = max77854->rdev;
	int i;
	dev_info(&pdev->dev, "%s\n", __func__);
	for (i = 0; i < max77854->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(max77854->rdev);
	kfree(max77854);

	return 0;
}

static const struct platform_device_id max77854_pmic_id[] = {
	{"max77854-safeout", 0},
	{},
};
MODULE_DEVICE_TABLE(platform, max77854_pmic_id);

static struct platform_driver max77854_pmic_driver = {
	.driver = {
		   .name = "max77854-safeout",
		   .owner = THIS_MODULE,
		   },
	.probe = max77854_pmic_probe,
	.remove = __devexit_p(max77854_pmic_remove),
	.id_table = max77854_pmic_id,
};

static int __init max77854_pmic_init(void)
{
	return platform_driver_register(&max77854_pmic_driver);
}
subsys_initcall(max77854_pmic_init);

static void __exit max77854_pmic_cleanup(void)
{
	platform_driver_unregister(&max77854_pmic_driver);
}

module_exit(max77854_pmic_cleanup);

MODULE_DESCRIPTION("MAXIM 77843 Regulator Driver");
MODULE_AUTHOR("Sukdong Kim <Sukdong.Kim@samsung.com>");
MODULE_LICENSE("GPL");
