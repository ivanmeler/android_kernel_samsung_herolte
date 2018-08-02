/*
 * s2mpb01.c - Regulator driver for the S.LSI S2MPB01
 *
 * Copyright (C) 2013 Samsung Electronics
 * SangYoung Son <hello.son@samsung.com>
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
 * This driver is based on max77826.c
 */

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/s2mpb01.h>

struct voltage_map_desc {
	int min;
	int max;
	int step;
	unsigned int n_bits;
};

static struct voltage_map_desc ldo_nmos_type1_voltage_map_desc = {
	.min = 800000,	.max = 1500000,	.step = 25000,	.n_bits = 6,
};

static struct voltage_map_desc ldo_nmos_type2_voltage_map_desc = {
	.min = 600000,	.max = 1500000,	.step = 25000,	.n_bits = 6,
};

static struct voltage_map_desc ldo_pmos_type1_voltage_map_desc = {
	.min = 800000,	.max = 2375000,	.step = 25000,	.n_bits = 6,
};

static struct voltage_map_desc ldo_pmos_type2_voltage_map_desc = {
	.min = 1800000,	.max = 3375000,	.step = 25000,	.n_bits = 6,
};

static struct voltage_map_desc buck_voltage_map_desc = {
	.min = 400000,	.max = 1500000,	.step = 6250,	.n_bits = 8,
};

static struct voltage_map_desc buck_boost_voltage_map_desc = {
	.min = 2600000,	.max = 4000000,	.step = 12500,	.n_bits = 7,
};

static struct voltage_map_desc *reg_voltage_map[] = {
	[S2MPB01_LDO1] = &ldo_nmos_type1_voltage_map_desc,
	[S2MPB01_LDO2] = &ldo_nmos_type2_voltage_map_desc,
	[S2MPB01_LDO3] = &ldo_nmos_type2_voltage_map_desc,
	[S2MPB01_LDO4] = &ldo_pmos_type1_voltage_map_desc,
	[S2MPB01_LDO5] = &ldo_pmos_type1_voltage_map_desc,
	[S2MPB01_LDO6] = &ldo_pmos_type1_voltage_map_desc,
	[S2MPB01_LDO7] = &ldo_pmos_type1_voltage_map_desc,
	[S2MPB01_LDO8] = &ldo_pmos_type1_voltage_map_desc,
	[S2MPB01_LDO9] = &ldo_pmos_type1_voltage_map_desc,
	[S2MPB01_LDO10] = &ldo_pmos_type2_voltage_map_desc,
	[S2MPB01_LDO11] = &ldo_pmos_type2_voltage_map_desc,
	[S2MPB01_LDO12] = &ldo_pmos_type2_voltage_map_desc,
	[S2MPB01_LDO13] = &ldo_pmos_type2_voltage_map_desc,
	[S2MPB01_LDO14] = &ldo_pmos_type2_voltage_map_desc,
	[S2MPB01_LDO15] = &ldo_pmos_type2_voltage_map_desc,
	[S2MPB01_BUCK1] = &buck_voltage_map_desc,
	[S2MPB01_BUCK2] = &buck_boost_voltage_map_desc,
};

int s2mpb01_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct s2mpb01_dev *s2mpb01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpb01->io_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&s2mpb01->io_lock);

	if (ret < 0) {
		pr_err("%s: client=%d, reg=%d, ret=%d\n",
				__func__, i2c->addr, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}

int s2mpb01_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mpb01_dev *s2mpb01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpb01->io_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mpb01->io_lock);
	if (ret < 0)
		return ret;

	return 0;
}

int s2mpb01_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct s2mpb01_dev *s2mpb01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpb01->io_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&s2mpb01->io_lock);
	return ret;
}

int s2mpb01_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mpb01_dev *s2mpb01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpb01->io_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mpb01->io_lock);
	if (ret < 0)
		return ret;

	return 0;
}

int s2mpb01_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2mpb01_dev *s2mpb01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpb01->io_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&s2mpb01->io_lock);
	return ret;
}

static int s2mpb01_get_en_register(struct regulator_dev *rdev,
		int *reg, int *shift, int *mask)
{
	int rid = rdev_get_id(rdev);

	switch (rid) {
	case S2MPB01_LDO1 ... S2MPB01_LDO15:
		*reg = S2MPB01_REG_LDO1_CTRL + (rid - S2MPB01_LDO1);
		*shift = 6;
		*mask = 0x03;
		break;

	case S2MPB01_BUCK1:
		*reg = S2MPB01_REG_BUCK_CTRL;
		*shift = 6;
		*mask = 0x03;
		break;

	case S2MPB01_BUCK2:
		*reg = S2MPB01_REG_BB_CTRL;
		*shift = 6;
		*mask = 0x03;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mpb01_get_voltage_register(struct regulator_dev *rdev,
		int *reg, int *shift, int *mask)
{
	int rid = rdev_get_id(rdev);

	switch (rid) {
	case S2MPB01_LDO1 ... S2MPB01_LDO15:
		*reg = S2MPB01_REG_LDO1_CTRL + (rid - S2MPB01_LDO1);
		*shift = 0;
		*mask = 0x3F;
		break;

	case S2MPB01_BUCK1:
		*reg = S2MPB01_REG_BUCK_OUT;
		*shift = 0;
		*mask = 0xFF;
		break;

	case S2MPB01_BUCK2:
		*reg = S2MPB01_REG_BB_OUT;
		*shift = 0;
		*mask = 0x7F;
		break;

	default:
		return -EINVAL;
	}

	pr_info("%s: id=%d, reg=0x%x, shift=0x%x, mask=0x%x\n",
		__func__, rid, *reg, *shift, *mask);

	return 0;
}

static int s2mpb01_ldo_is_enabled(struct regulator_dev *rdev)
{
	struct s2mpb01_dev *s2mpb01 = rdev_get_drvdata(rdev);
	int ret = 0, reg = 0, shift = 0, mask = 0;
	u8 val;

	ret = s2mpb01_get_en_register(rdev, &reg, &shift, &mask);
	if (ret) {
		pr_err("%s: id=%d, error=%d\n", __func__,
					rdev_get_id(rdev), ret);
		return ret;
	}

	ret = s2mpb01_read_reg(s2mpb01->i2c, reg, &val);
	if (ret)
		return ret;

	pr_debug("%s: id=%d, val=0x%x, enval=0x%x\n",
		__func__, rdev_get_id(rdev), val, ((val >> shift) == 0x02));

	return ((val >> shift) == 0x02);
}

static int s2mpb01_ldo_enable(struct regulator_dev *rdev)
{
	struct s2mpb01_dev *s2mpb01 = rdev_get_drvdata(rdev);
	int ret = 0, reg = 0, shift = 0, mask = 0;

	ret = s2mpb01_get_en_register(rdev, &reg, &shift, &mask);
	if (ret) {
		pr_err("%s: id=%d, error=%d\n", __func__,
					rdev_get_id(rdev), ret);
		return ret;
	}

	pr_debug("%s: id=%d, reg=0x%x, shift=0x%x, mask=0x%x\n",
		__func__, rdev_get_id(rdev), reg, shift, mask);

	return s2mpb01_update_reg(s2mpb01->i2c, reg,
			(0x02 << shift), (mask << shift));
}

static int s2mpb01_ldo_disable(struct regulator_dev *rdev)
{
	struct s2mpb01_dev *s2mpb01 = rdev_get_drvdata(rdev);
	int ret = 0, reg = 0, shift = 0, mask = 0;

	ret = s2mpb01_get_en_register(rdev, &reg, &shift, &mask);
	if (ret) {
		pr_err("%s: id=%d, error=%d\n", __func__,
					rdev_get_id(rdev), ret);
		return ret;
	}

	pr_debug("%s: id=%d, reg=0x%x, shift=0x%x, mask=0x%x\n",
		__func__, rdev_get_id(rdev), reg, shift, mask);

	return s2mpb01_update_reg(s2mpb01->i2c, reg,
			((~mask) << shift), (mask << shift));
}

static int s2mpb01_ldo_list_voltage(struct regulator_dev *rdev,
		unsigned int selector)
{
	const struct voltage_map_desc *desc;
	int rid = rdev_get_id(rdev);
	int val;

	if (rid >= ARRAY_SIZE(reg_voltage_map) ||
			rid < 0)
		return -EINVAL;

	desc = reg_voltage_map[rid];
	if (desc == NULL)
		return -EINVAL;

	val = desc->min + desc->step * selector;
	if (val > desc->max)
		return -EINVAL;

	pr_debug("%s: id=%d, val=%d\n", __func__, rdev_get_id(rdev), val);

	return val;
}

static int s2mpb01_ldo_get_voltage(struct regulator_dev *rdev)
{
	struct s2mpb01_dev *s2mpb01 = rdev_get_drvdata(rdev);
	int reg, shift, mask, ret;
	u8 val;

	ret = s2mpb01_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret) {
		pr_err("%s: id=%d, error=%d\n", __func__,
					rdev_get_id(rdev), ret);
		return ret;
	}

	ret = s2mpb01_read_reg(s2mpb01->i2c, reg, &val);
	if (ret)
		return ret;

	val = (val & mask) >> shift;

	pr_debug("%s: id=%d, reg=0x%x, mask=0x%x, val=0x%x\n",
		__func__, rdev_get_id(rdev), reg, mask, val);

	return s2mpb01_ldo_list_voltage(rdev, val);
}

static inline int s2mpb01_get_voltage_proper_val(
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

static int s2mpb01_ldo_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct s2mpb01_dev *s2mpb01 = rdev_get_drvdata(rdev);
	const struct voltage_map_desc *desc;
	int rid = rdev_get_id(rdev);
	int reg, shift, mask, ret;
	int i;
	u8 val;

	desc = reg_voltage_map[rid];

	i = s2mpb01_get_voltage_proper_val(desc, min_uV, max_uV);
	if (i < 0)
		return i;

	ret = s2mpb01_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret) {
		pr_err("%s: id=%d, error=%d\n", __func__,
					rdev_get_id(rdev), ret);
		return ret;
	}

	s2mpb01_read_reg(s2mpb01->i2c, reg, &val);
	val = (val & mask) >> shift;

	pr_info("%s: id=%d, reg=0x%x, mask=0x%x, val=0x%x, new=0x%x\n",
		__func__, rdev_get_id(rdev), reg, mask, val, i);

	ret = s2mpb01_update_reg(s2mpb01->i2c, reg, i << shift, mask << shift);
	*selector = i;

	switch (rid) {
	case S2MPB01_LDO1:
	case S2MPB01_LDO3:
	case S2MPB01_LDO10:
	case S2MPB01_LDO12:
	case S2MPB01_LDO14:
	case S2MPB01_BUCK1:
		if (val < i)
			udelay(DIV_ROUND_UP(desc->step * (i - val), 12500));
		break;
	default:
		break;
	}

	return ret;
}

static struct regulator_ops s2mpb01_ldo_ops = {
	.is_enabled = s2mpb01_ldo_is_enabled,
	.enable = s2mpb01_ldo_enable,
	.disable = s2mpb01_ldo_disable,
	.list_voltage = s2mpb01_ldo_list_voltage,
	.get_voltage = s2mpb01_ldo_get_voltage,
	.set_voltage = s2mpb01_ldo_set_voltage,
};

#define regulator_desc_ldo(num)		{	\
	.name		= "LDO"#num,			\
	.id			= S2MPB01_LDO##num,	\
	.ops		= &s2mpb01_ldo_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,			\
}
#define regulator_desc_buck(num)	{	\
	.name		= "BUCK"#num,			\
	.id			= S2MPB01_BUCK##num,	\
	.ops		= &s2mpb01_ldo_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,			\
}

static struct regulator_desc regulators[] = {
	regulator_desc_ldo(1),
	regulator_desc_ldo(2),
	regulator_desc_ldo(3),
	regulator_desc_ldo(4),
	regulator_desc_ldo(5),
	regulator_desc_ldo(6),
	regulator_desc_ldo(7),
	regulator_desc_ldo(8),
	regulator_desc_ldo(9),
	regulator_desc_ldo(10),
	regulator_desc_ldo(11),
	regulator_desc_ldo(12),
	regulator_desc_ldo(13),
	regulator_desc_ldo(14),
	regulator_desc_ldo(15),
	regulator_desc_buck(1),
	regulator_desc_buck(2),
};

#ifdef CONFIG_OF
static int s2mpb01_pmic_dt_parse_pdata(struct s2mpb01_dev *s2mpb01,
					struct s2mpb01_platform_data *pdata)
{

	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpb01_regulator_subdev *rdata;
	unsigned int i;

		pmic_np = s2mpb01->dev->of_node;
		if (!pmic_np) {
			dev_err(s2mpb01->dev, "could not find pmic sub-node\n");
			return -ENODEV;
		}

		regulators_np = of_find_node_by_name(pmic_np, "regulators");
		if (!regulators_np) {
			dev_err(s2mpb01->dev, "could not find regulators sub-node\n");
			return -EINVAL;
		}

		/* count the number of regulators to be supported in pmic */
		pdata->num_regulators = 0;
		for_each_child_of_node(regulators_np, reg_np) {
			pdata->num_regulators++;
		}

		rdata = devm_kzalloc(s2mpb01->dev, sizeof(*rdata) *
					pdata->num_regulators, GFP_KERNEL);
		if (!rdata) {
			dev_err(s2mpb01->dev,
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
				dev_warn(s2mpb01->dev,
				"don't know how to configure regulator %s\n",
				reg_np->name);
				continue;
			}

			rdata->id = i;
			rdata->initdata = of_get_regulator_init_data(
							s2mpb01->dev, reg_np);
			rdata->reg_node = reg_np;
			rdata++;
		}

	return 0;
}
#else
static int s2mpb01_pmic_dt_parse_pdata(struct s2mpb01_dev *s2mpb01,
					struct s2mpb01_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpb01_setup_regulators(struct s2mpb01_dev *s2mpb01,
	struct s2mpb01_platform_data *pdata)
{
	int i, err, ret;
	struct regulator_config config = { };


#ifdef CONFIG_OF
	if (s2mpb01->dev->of_node) {
		ret = s2mpb01_pmic_dt_parse_pdata(s2mpb01, pdata);
		if (ret)
			return ret;
	}
#endif

	s2mpb01->num_regulators = pdata->num_regulators;
	s2mpb01->rdev = kcalloc(pdata->num_regulators,
				sizeof(struct regulator_dev *), GFP_KERNEL);
	if (!s2mpb01->rdev) {
		err = -ENOMEM;
		goto err_nomem;
	}

	/* Register the regulators */
	for (i = 0; i < s2mpb01->num_regulators; i++) {
		struct s2mpb01_regulator_subdev *reg = &pdata->regulators[i];

		config.dev = s2mpb01->dev;
		config.init_data = reg->initdata;
		config.driver_data = s2mpb01;
		config.of_node = reg->reg_node;

		s2mpb01->rdev[i] =
			regulator_register(&regulators[reg->id], &config);
		if (IS_ERR(s2mpb01->rdev[i])) {
			err = PTR_ERR(s2mpb01->rdev[i]);
			dev_err(s2mpb01->dev, "regulator init failed: %d\n",
				err);
			goto error;
		}
	}

	return 0;

error:
	while (--i >= 0)
		regulator_unregister(s2mpb01->rdev[i]);

	kfree(s2mpb01->rdev);
	s2mpb01->rdev = NULL;

err_nomem:
	return err;
}

#ifdef CONFIG_OF
static int s2mpb01_pmic_i2c_dt_parse_pdata(struct i2c_client *i2c,
					struct s2mpb01_platform_data *pdata)
{
	return 0;
}
#else
static int s2mpb01_pmic_i2c_dt_parse_pdata(struct i2c_client *i2c,
					struct s2mpb01_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpb01_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct s2mpb01_dev *s2mpb01;
	struct s2mpb01_platform_data *pdata;
	int ret;

	pr_info("%s\n", __func__);

	/*Check I2C functionality */
	ret = i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C);
	if (!ret) {
		printk(KERN_ERR "%s: No I2C functionality found\n", __func__);
		ret = -ENODEV;
		goto err_i2c_fail;
	}

	if (i2c->dev.of_node) {
		pr_info("%s: of_node\n", __func__);

		pdata = devm_kzalloc(&i2c->dev,
			sizeof(struct s2mpb01_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev,
				"%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		ret = s2mpb01_pmic_i2c_dt_parse_pdata(i2c, pdata);
		if (ret)
			return ret;
	} else {
		pr_info("%s: pdata\n", __func__);

		pdata = i2c->dev.platform_data;
		if (!pdata) {
			dev_err(&i2c->dev, "%s: no pdata\n", __func__);
			ret = -ENODEV;
			goto err_i2c_fail;
		}
	}

	s2mpb01 = kzalloc(sizeof(struct s2mpb01_dev), GFP_KERNEL);
	if (!s2mpb01)
		return -ENOMEM;

	s2mpb01->i2c = i2c;
	s2mpb01->dev = &i2c->dev;

	mutex_init(&s2mpb01->io_lock);

	i2c_set_clientdata(i2c, s2mpb01);

	{
		u8 reg = 0;
		ret = s2mpb01_read_reg(s2mpb01->i2c, S2MPB01_REG_PMIC_ID, &reg);
		if (ret) {
			pr_err("%s: device cannot detect\n", __func__);
			goto err_detect;
		}
		pr_info("%s: PMIC ID: 0x%x\n", __func__, reg);

	}

	ret = s2mpb01_setup_regulators(s2mpb01, pdata);
	if (ret < 0)
		goto err_detect;

	return 0;

err_detect:
	kfree(s2mpb01);
err_i2c_fail:
	return ret;
}

static int  s2mpb01_i2c_remove(struct i2c_client *i2c)
{
	struct s2mpb01_dev *s2mpb01 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < s2mpb01->num_regulators; i++)
		regulator_unregister(s2mpb01->rdev[i]);
	kfree(s2mpb01->rdev);
	kfree(s2mpb01);
	return 0;
}

static const struct i2c_device_id s2mpb01_i2c_id[] = {
	{ "s2mpb01", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mpb01_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id s2mpb01_match_table[] = {
	{ .compatible = "s2mpb01,s2mpb01-regulator",},
	{ },
};
#endif

static struct i2c_driver s2mpb01_i2c_driver = {
	.driver = {
		.name = "s2mpb01",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = s2mpb01_match_table,
#endif
	},
	.probe    = s2mpb01_i2c_probe,
	.remove   = s2mpb01_i2c_remove,
	.id_table = s2mpb01_i2c_id,
};

static int __init s2mpb01_module_init(void)
{
	pr_info("%s\n", __func__);

	return i2c_add_driver(&s2mpb01_i2c_driver);
}
subsys_initcall(s2mpb01_module_init);

static void __exit s2mpb01_module_exit(void)
{
	i2c_del_driver(&s2mpb01_i2c_driver);
}
module_exit(s2mpb01_module_exit);

MODULE_DESCRIPTION("S.LSI S2MPB01 Regulator Driver");
MODULE_AUTHOR("SangYoung Son <hello.son@samsung.com>");
MODULE_LICENSE("GPL");
