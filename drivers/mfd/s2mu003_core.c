/* drivers/mfd/s2mu003_core.c
 * S2MU003 Multifunction Device Driver
 * Charger / Buck / LDOs / FlashLED
 *
 * Copyright (C) 2014 Samsung Technology Corp.
 * Author: Patrick Chang <patrick_chang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include <linux/mfd/samsung/s2mu003.h>
#include <linux/mfd/samsung/s2mu003_irq.h>
#include <linux/mfd/core.h>
#ifdef CONFIG_CHARGER_S2MU003
#include <linux/battery/charger/s2mu003_charger.h>
#endif

#if defined(CONFIG_MFD_S2MU003_USE_DT)
#define S2MU003_USE_NEW_MFD_DT_API
#endif

#ifdef CONFIG_CHARGER_S2MU003
const static struct resource s2mu003_charger_res[] = {
	S2MU003_DECLARE_IRQ(S2MU003_EOC_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CINIR_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_BATP_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_BATLV_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_TOPOFF_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CINOVP_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CHGTSD_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CHGVINVR_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CHGTR_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_TMROUT_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_RECHG_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CHGTERM_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_BATOVP_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CHGVIN_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CIN2VAT_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_CHGSTS_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_OTGILIM_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_BSTINLV_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_BSTILIM_IRQ),
	S2MU003_DECLARE_IRQ(S2MU003_VMIDOVP_IRQ),
};

static struct mfd_cell s2mu003_charger_devs[] = {
	{
		.name		= "s2mu003-charger",
		.num_resources	= ARRAY_SIZE(s2mu003_charger_res),
		.id		= -1,
		.resources = s2mu003_charger_res,
#ifdef CONFIG_OF
		.of_compatible = "samsung,s2mu003-charger"
#endif
	},
};
#endif /*CONFIG_CHARGER_S2MU003*/

#ifdef CONFIG_LEDS_S2MU003
static struct mfd_cell s2mu003_fled_devs[] = {
	{
		.name		= "s2mu003-leds",
#ifdef CONFIG_OF
	.of_compatible = "samsung,s2mu003-leds"
#endif
	},
};
#endif

#ifdef CONFIG_REGULATOR_S2MU003
static struct mfd_cell s2mu003_regulator_devs[] = {
	{ .name = "s2mu003-regulator", },
};
#endif /* CONFIG_REGULATOR_S2MU003 */

const static uint8_t s2mu003_chg_group1_default[] = { 0x40, 0x58 };
const static uint8_t s2mu003_chg_group2_default[] = {0x41, 0xAB, 0x35};

static inline int s2mu003_read_device(struct i2c_client *i2c,
				int reg, int bytes, void *dest)
{
	int ret;
	if (bytes > 1) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, bytes, dest);
	} else {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret < 0)
			return ret;
		*(u8 *)dest = (u8)ret;
	}
	return ret;
}

static inline int s2mu003_write_device(struct i2c_client *i2c,
				int reg, int bytes, void *src)
{
	int ret;
	uint8_t *data;
	if (bytes > 1)
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, bytes, src);
	else {
		data = src;
		ret = i2c_smbus_write_byte_data(i2c, reg, *data);
	}
	return ret;
}

int s2mu003_block_read_device(struct i2c_client *i2c,
			int reg, int bytes, void *dest)
{
	struct s2mu003_mfd_chip *chip = i2c_get_clientdata(i2c);
	int ret;
	mutex_lock(&chip->io_lock);
	ret = s2mu003_read_device(i2c, reg, bytes, dest);
	mutex_unlock(&chip->io_lock);
	return ret;
}
EXPORT_SYMBOL(s2mu003_block_read_device);

int s2mu003_block_write_device(struct i2c_client *i2c,
		int reg, int bytes, void *src)
{
	struct s2mu003_mfd_chip *chip = i2c_get_clientdata(i2c);
	int ret;

	if (!chip)
		dev_err(&i2c->dev, "Failed to get the clientdata\n");

	mutex_lock(&chip->io_lock);
	ret = s2mu003_write_device(i2c, reg, bytes, src);
	mutex_unlock(&chip->io_lock);
	return ret;
}
EXPORT_SYMBOL(s2mu003_block_write_device);

int s2mu003_reg_read(struct i2c_client *i2c, int reg)
{
	struct s2mu003_mfd_chip *chip = i2c_get_clientdata(i2c);
	u8 data = 0;
	int ret;

	if (!chip)
		dev_err(&i2c->dev, "Failed to get the clientdata\n");

	mutex_lock(&chip->io_lock);
	ret = s2mu003_read_device(i2c, reg, 1, &data);
	mutex_unlock(&chip->io_lock);

	if (ret < 0)
		return ret;
	else
		return (int)data;
}
EXPORT_SYMBOL(s2mu003_reg_read);

int s2mu003_reg_write(struct i2c_client *i2c, int reg,
		u8 data)
{
	struct s2mu003_mfd_chip *chip = i2c_get_clientdata(i2c);
	int ret;

	if (!chip)
		dev_err(&i2c->dev, "Failed to get the clientdata\n");

	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}
EXPORT_SYMBOL(s2mu003_reg_write);

int s2mu003_assign_bits(struct i2c_client *i2c, int reg,
		u8 mask, u8 data)
{
	struct s2mu003_mfd_chip *chip = i2c_get_clientdata(i2c);
	u8 value;
	int ret;

	if (!chip)
		dev_err(&i2c->dev, "Failed to get the clientdata\n");

	mutex_lock(&chip->io_lock);
	ret = s2mu003_read_device(i2c, reg, 1, &value);

	if (ret < 0)
		goto out;

	value &= ~mask;
	value |= data;
	ret = i2c_smbus_write_byte_data(i2c, reg, value);

out:
	mutex_unlock(&chip->io_lock);
	return ret;
}
EXPORT_SYMBOL(s2mu003_assign_bits);

int s2mu003_set_bits(struct i2c_client *i2c, int reg,
		u8 mask)
{
	return s2mu003_assign_bits(i2c, reg, mask, mask);
}
EXPORT_SYMBOL(s2mu003_set_bits);

int s2mu003_clr_bits(struct i2c_client *i2c, int reg,
		u8 mask)
{
	return s2mu003_assign_bits(i2c, reg, mask, 0);
}
EXPORT_SYMBOL(s2mu003_clr_bits);

static int s2mu003mfd_parse_dt(struct device *dev,
		s2mu003_mfd_platform_data_t *pdata)
{
	int ret;
	struct device_node *np = dev->of_node;

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -EINVAL;
	}

	ret = pdata->irq_gpio = of_get_named_gpio(np, "s2mu003,irq-gpio", 0);
	if (ret < 0) {
		dev_err(dev, "%s : can't get irq-gpio\r\n", __func__);
		return ret;
	}

	pdata->irq_base = -1;
	return 0;
}

static int s2mu003_mfd_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	int ret = 0;
	u8 data = 0;
	struct device_node *of_node = i2c->dev.of_node;
	s2mu003_mfd_chip_t *chip;
	s2mu003_mfd_platform_data_t *pdata = i2c->dev.platform_data;

	pr_info("%s : S2MU003 MFD Driver start probe\n", __func__);
	if (of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_dt_nomem;
		}
		ret = s2mu003mfd_parse_dt(&i2c->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		pdata = i2c->dev.platform_data;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip ==  NULL) {
		dev_err(&i2c->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_mfd_nomem;
	}
	chip->dev = &i2c->dev;

	ret = i2c_check_functionality(i2c->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(i2c->adapter);
		dev_err(chip->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}

	chip->i2c_client = i2c;
	chip->pdata = pdata;

	/* if board-init had already assigned irq_base (>=0) ,
	no need to allocate it;
	assign -1 to let this driver allocate resource by itself*/
	if (pdata->irq_base < 0)
		pdata->irq_base = irq_alloc_descs(-1, 0, S2MU003_IRQS_NR, 0);
	if (pdata->irq_base < 0) {
		pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
				"s2mu003-mfd", __func__, pdata->irq_base);
		ret = -EINVAL;
		goto irq_base_err;
	} else {
		chip->irq_base = pdata->irq_base;
		pr_info("%s:%s irq_base = %d\n",
				"s2mu003-mfd", __func__, chip->irq_base);
	irq_domain_add_legacy(of_node, S2MU003_IRQS_NR, chip->irq_base, 0,
				&irq_domain_simple_ops, NULL);
	}

	i2c_set_clientdata(i2c, chip);
	mutex_init(&chip->io_lock);

	wake_lock_init(&(chip->irq_wake_lock), WAKE_LOCK_SUSPEND,
			"s2mu003mfd_wakelock");

	/* To disable MRST function should be
	finished before set any reg init-value*/
	data = s2mu003_reg_read(i2c, 0x47);
	pr_info("%s : Manual Reset Data = 0x%x", __func__, data);
	s2mu003_clr_bits(i2c, 0x47, 1<<3); /*Disable Manual Reset*/

	ret = s2mu003_init_irq(chip);

	if (ret < 0) {
		dev_err(chip->dev,
				"Error : can't initialize S2MU003 MFD irq\n");
		goto err_init_irq;
	}

#ifdef CONFIG_REGULATOR_S2MU003
	ret = mfd_add_devices(chip->dev, 0, &s2mu003_regulator_devs[0],
			ARRAY_SIZE(s2mu003_regulator_devs),
			NULL, chip->irq_base, NULL);
	if (ret < 0) {
		dev_err(chip->dev,
				"Error : can't add regulator\n");
		goto err_add_regulator_devs;
	}
#endif /*CONFIG_REGULATOR_S2MU003*/

#ifdef CONFIG_LEDS_S2MU003
	ret = mfd_add_devices(chip->dev, 0, &s2mu003_fled_devs[0],
			ARRAY_SIZE(s2mu003_fled_devs),
			NULL, chip->irq_base, NULL);
	if (ret < 0) {
		dev_err(chip->dev, "Failed : add FlashLED devices");
		goto err_add_fled_devs;
	}
#endif /*CONFIG_LEDS_S2MU003*/


#ifdef CONFIG_CHARGER_S2MU003
	ret = mfd_add_devices(chip->dev, 0, &s2mu003_charger_devs[0],
			ARRAY_SIZE(s2mu003_charger_devs),
			NULL, chip->irq_base, NULL);
	if (ret < 0) {
		dev_err(chip->dev, "Failed : add charger devices\n");
		goto err_add_chg_devs;
	}
#endif /*CONFIG_CHARGER_S2MU003*/

	device_init_wakeup(chip->dev, 1);
	enable_irq_wake(chip->irq);

	pr_info("%s : S2MU003 MFD Driver Fin probe\n", __func__);
	return ret;

#ifdef CONFIG_CHARGER_S2MU003
err_add_chg_devs:
#endif /*CONFIG_CHARGER_S2MU003*/

#ifdef CONFIG_LEDS_S2MU003
err_add_fled_devs:
#endif /*CONFIG_LEDS_S2MU003*/
	mfd_remove_devices(chip->dev);
#ifdef CONFIG_REGULATOR_S2MU003
err_add_regulator_devs:
#endif /*CONFIG_REGULATOR_S2MU003*/
err_init_irq:
	wake_lock_destroy(&(chip->irq_wake_lock));
	mutex_destroy(&chip->io_lock);
	kfree(chip);
irq_base_err:
err_mfd_nomem:
err_i2cfunc_not_support:
err_parse_dt:
err_dt_nomem:
	return ret;
}

static int s2mu003_mfd_remove(struct i2c_client *i2c)
{
	s2mu003_mfd_chip_t *chip = i2c_get_clientdata(i2c);

	pr_info("%s : S2MU003 MFD Driver remove\n", __func__);

	mfd_remove_devices(chip->dev);
	wake_lock_destroy(&(chip->irq_wake_lock));
	mutex_destroy(&chip->io_lock);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_PM
/*check here cause error
  type define must de changed....
  use struct device *dev for function attribute
 */
int s2mu003_mfd_suspend(struct device *dev)
{

	struct i2c_client *i2c =
		container_of(dev, struct i2c_client, dev);

	s2mu003_mfd_chip_t *chip = i2c_get_clientdata(i2c);
	BUG_ON(chip == NULL);

	disable_irq(chip->irq);

	return 0;
}

int s2mu003_mfd_resume(struct device *dev)
{
	struct i2c_client *i2c =
		container_of(dev, struct i2c_client, dev);
	s2mu003_mfd_chip_t *chip = i2c_get_clientdata(i2c);
	BUG_ON(chip == NULL);

	pr_info("%s: S2MU003 Resume\n", __func__);

	enable_irq(chip->irq);

	return 0;
}
#endif /* CONFIG_PM */

static void s2mu003_mfd_shutdown(struct i2c_client *client)
{
}

static const struct i2c_device_id s2mu003_mfd_id_table[] = {
	{ "s2mu003-mfd", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, s2mu003_id_table);


#ifdef CONFIG_PM
const struct dev_pm_ops s2mu003_pm = {
	.suspend = s2mu003_mfd_suspend,
	.resume = s2mu003_mfd_resume,

};
#endif

#ifdef CONFIG_OF
static struct of_device_id s2mu003_match_table[] = {
	{ .compatible = "samsung,s2mu003mfd",},
	{},
};
#else
#define s2mu003_match_table NULL
#endif

static struct i2c_driver s2mu003_mfd_driver = {
	.driver	= {
		.name	= "s2mu003-mfd",
		.owner	= THIS_MODULE,
		.of_match_table = s2mu003_match_table,
#ifdef CONFIG_PM
		.pm		= &s2mu003_pm,
#endif
	},
	.shutdown	= s2mu003_mfd_shutdown,
	.probe		= s2mu003_mfd_probe,
	.remove		= s2mu003_mfd_remove,
	.id_table	= s2mu003_mfd_id_table,
};

static int __init s2mu003_mfd_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&s2mu003_mfd_driver);
	if (ret != 0)
		pr_info("%s : Failed to register S2MU003 MFD I2C driver\n",
				__func__);

	return ret;
}
subsys_initcall(s2mu003_mfd_i2c_init);

static void __exit s2mu003_mfd_i2c_exit(void)
{
	i2c_del_driver(&s2mu003_mfd_driver);
}
module_exit(s2mu003_mfd_i2c_exit);

MODULE_DESCRIPTION("Richtek S2MU003 MFD I2C Driver");
MODULE_AUTHOR("Patrick Chang <patrick_chang@samsung.com>");
MODULE_VERSION(S2MU003_DRV_VER);
MODULE_LICENSE("GPL");
