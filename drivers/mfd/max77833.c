/*
 * max77833.c - mfd core driver for the Maxim 77833
 *
 * Copyright (C) 2011 Samsung Electronics
 * SeoYoung Jeong <seo0.jeong@samsung.com>
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
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77833.h>
#include <linux/mfd/max77833-private.h>
#include <linux/regulator/machine.h>

#include <linux/muic/muic.h>
#include <linux/muic/max77833-muic.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_PMIC	(0x92 >> 1)	/* Top sys, Haptic */
#define I2C_ADDR_MUIC	(0x4A >> 1)
#define I2C_ADDR_FG     (0x6C >> 1)

static struct mfd_cell max77833_devs[] = {
#if defined(CONFIG_MUIC_MAX77833)
	{ .name = MUIC_DEV_NAME, },
#endif /* CONFIG_MUIC_MAX77833 */
#if defined(CONFIG_REGULATOR_MAX77833)
	{ .name = "max77833-safeout", },
#endif /* CONFIG_REGULATOR_MAX77833 */
#if defined(CONFIG_FUELGAUGE_MAX77833)
	{ .name = "max77833-fuelgauge", },
#endif
#if defined(CONFIG_CHARGER_MAX77833)
	{ .name = "max77833-charger", },
#endif
#if defined(CONFIG_MOTOR_DRV_MAX77833)
	{ .name = "max77833-haptic", },
#endif /* CONFIG_MAX77833_HAPTIC */
#if defined(CONFIG_LEDS_MAX77833_RGB)
	{ .name = "leds-max77833-rgb", },
#endif /* CONFIG_LEDS_MAX77833_RGB */
#if defined(CONFIG_LEDS_MAX77833_FLASH)
	{ .name = "max77833-flash", },
#endif
};

int max77833_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77833->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&max77833->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(max77833_read_reg);

int max77833_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77833->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77833->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(max77833_bulk_read);

int max77833_read_word(struct i2c_client *i2c, u8 reg)
{
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77833->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&max77833->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL_GPL(max77833_read_word);

int max77833_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77833->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&max77833->i2c_lock);
	if (ret < 0)
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(max77833_write_reg);

int max77833_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77833->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77833->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(max77833_bulk_write);

int max77833_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77833->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&max77833->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL_GPL(max77833_write_word);

int max77833_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77833->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&max77833->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(max77833_update_reg);

int max77833_read_fg(struct i2c_client *i2c, u16 reg, u16 *dest)
{
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2], rbuf[2]; 

	if (!i2c->adapter) {
		pr_err("%s: Could not find adapter!\n", __func__);
		return -ENODEV;
	}

	msg[0].addr = i2c->addr;
	msg[0].flags = i2c->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[1] = (reg & 0xFF00) >> 8;
	wbuf[0] = (reg & 0xFF);

	msg[1].addr = i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rbuf;

	ret = i2c_transfer(i2c->adapter, msg, 2);
	if (ret < 0)
		pr_err("%s: i2c treansfer fail", __func__);

	*dest = ((rbuf[1] << 8) | rbuf[0]);

	return ret;
}
EXPORT_SYMBOL_GPL(max77833_read_fg);

int max77833_write_fg(struct i2c_client *i2c, u16 reg, u16 val)
{
	int ret = 0;
	struct i2c_msg msg[1];
	u8 wbuf[4];

	if (!i2c->adapter) {
		pr_err("%s: Could not find adapter!\n", __func__);
		return -ENODEV;
	}

	msg->addr = i2c->addr;
	msg->flags = 0;
	msg->len = 4;
	msg->buf = wbuf;
	wbuf[1] = (reg & 0xFF00) >> 8;
	wbuf[0] = (reg & 0xFF);
	wbuf[3] = (val & 0xFF00) >> 8;
	wbuf[2] = (val & 0xFF);

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s: i2c treansfer fail(%d)", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(max77833_write_fg);

#if defined(CONFIG_OF)
static int of_max77833_dt(struct device *dev, struct max77833_platform_data *pdata)
{
	struct device_node *np_max77833 = dev->of_node;

	if(!np_max77833)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np_max77833, "max77833,irq-gpio", 0);
	pdata->wakeup = of_property_read_bool(np_max77833, "max77833,wakeup");

	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

	return 0;
}
#else
static int of_max77833_dt(struct device *dev, struct max77833_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int max77833_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct max77833_dev *max77833;
	struct max77833_platform_data *pdata = i2c->dev.platform_data;

	u8 reg_data;
	int ret = 0;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	max77833 = kzalloc(sizeof(struct max77833_dev), GFP_KERNEL);
	if (!max77833) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for max77833\n", __func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct max77833_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory \n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_max77833_dt(&i2c->dev, pdata);
		if (ret < 0){
			dev_err(&i2c->dev, "Failed to get device of_node \n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	max77833->dev = &i2c->dev;
	max77833->i2c = i2c;
	max77833->irq = i2c->irq;
	if (pdata) {
		max77833->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, MAX77833_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
					MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			max77833->irq_base = pdata->irq_base;

		max77833->irq_gpio = pdata->irq_gpio;
		max77833->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&max77833->i2c_lock);

	i2c_set_clientdata(i2c, max77833);

	if (max77833_read_reg(i2c, MAX77833_PMIC_REG_PMICREV, &reg_data) < 0) {
		dev_err(max77833->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	} else {
		/* print rev */
		max77833->pmic_rev = (reg_data & 0x7);
		max77833->pmic_ver = ((reg_data & 0xF8) >> 0x3);
		pr_info("%s:%s device found: rev.0x%x, ver.0x%x\n",
				MFD_DEV_NAME, __func__,
				max77833->pmic_rev, max77833->pmic_ver);
	}

	/* No active discharge on safeout ldo 1,2 */
	max77833_update_reg(i2c, MAX77833_PMIC_REG_SAFEOUT_CTRL, 0x00, 0x30);

	max77833->muic = i2c_new_dummy(i2c->adapter, I2C_ADDR_MUIC);
	i2c_set_clientdata(max77833->muic, max77833);

	max77833->fuelgauge = i2c_new_dummy(i2c->adapter, I2C_ADDR_FG);
	i2c_set_clientdata(max77833->fuelgauge, max77833);

	ret = max77833_irq_init(max77833);

	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(max77833->dev, -1, max77833_devs,
			ARRAY_SIZE(max77833_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(max77833->dev, pdata->wakeup);

	return ret;

err_mfd:
	mfd_remove_devices(max77833->dev);
err_irq_init:
	i2c_unregister_device(max77833->muic);
err_w_lock:
	mutex_destroy(&max77833->i2c_lock);
err:
	kfree(max77833);
	return ret;
}

static int max77833_i2c_remove(struct i2c_client *i2c)
{
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);

	mfd_remove_devices(max77833->dev);
	i2c_unregister_device(max77833->muic);
	kfree(max77833);

	return 0;
}

static const struct i2c_device_id max77833_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_MAX77833 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77833_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id max77833_i2c_dt_ids[] = {
	{ .compatible = "maxim,max77833" },
	{ },
};
MODULE_DEVICE_TABLE(of, max77833_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int max77833_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(max77833->irq);

	disable_irq(max77833->irq);

	return 0;
}

static int max77833_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(max77833->irq);

	enable_irq(max77833->irq);

	return 0;
}
#else
#define max77833_suspend	NULL
#define max77833_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_HIBERNATION

/*
u8 max77833_dumpaddr_pmic[] = {
#if 0
	MAX77833_LED_REG_IFLASH,
	MAX77833_LED_REG_IFLASH1,
	MAX77833_LED_REG_IFLASH2,
	MAX77833_LED_REG_ITORCH,
	MAX77833_LED_REG_ITORCHTORCHTIMER,
	MAX77833_LED_REG_FLASH_TIMER,
	MAX77833_LED_REG_FLASH_EN,
	MAX77833_LED_REG_MAX_FLASH1,
	MAX77833_LED_REG_MAX_FLASH2,
	MAX77833_LED_REG_VOUT_CNTL,
	MAX77833_LED_REG_VOUT_FLASH,
	MAX77833_LED_REG_VOUT_FLASH1,
	MAX77833_LED_REG_FLASH_INT_STATUS,
#endif
	MAX77833_PMIC_REG_PMICID1,
	MAX77833_PMIC_REG_PMICREV,
	MAX77833_PMIC_REG_MAINCTRL1,
	MAX77833_PMIC_REG_MCONFIG,
};

u8 max77833_dumpaddr_muic[] = {
	MAX77833_MUIC_REG_INTMASK1,
	MAX77833_MUIC_REG_INTMASK2,
	MAX77833_MUIC_REG_INTMASK3,
	MAX77833_MUIC_REG_CDETCTRL1,
	MAX77833_MUIC_REG_CDETCTRL2,
	MAX77833_MUIC_REG_CTRL1,
	MAX77833_MUIC_REG_CTRL2,
	MAX77833_MUIC_REG_CTRL3,
};

u8 max77833_dumpaddr_haptic[] = {
	MAX77833_HAPTIC_REG_CONFIG1,
	MAX77833_HAPTIC_REG_CONFIG2,
	MAX77833_HAPTIC_REG_CONFIG_CHNL,
	MAX77833_HAPTIC_REG_CONFG_CYC1,
	MAX77833_HAPTIC_REG_CONFG_CYC2,
	MAX77833_HAPTIC_REG_CONFIG_PER1,
	MAX77833_HAPTIC_REG_CONFIG_PER2,
	MAX77833_HAPTIC_REG_CONFIG_PER3,
	MAX77833_HAPTIC_REG_CONFIG_PER4,
	MAX77833_HAPTIC_REG_CONFIG_DUTY1,
	MAX77833_HAPTIC_REG_CONFIG_DUTY2,
	MAX77833_HAPTIC_REG_CONFIG_PWM1,
	MAX77833_HAPTIC_REG_CONFIG_PWM2,
	MAX77833_HAPTIC_REG_CONFIG_PWM3,
	MAX77833_HAPTIC_REG_CONFIG_PWM4,
};
*/

u8 max77833_dumpaddr_led[] = {
	MAX77833_RGBLED_REG_LEDEN,
	MAX77833_RGBLED_REG_LED0BRT,
	MAX77833_RGBLED_REG_LED1BRT,
	MAX77833_RGBLED_REG_LED2BRT,
	MAX77833_RGBLED_REG_LED3BRT,
	MAX77833_RGBLED_REG_LEDBLNK,
	MAX77833_RGBLED_REG_LEDRMP,
};

static int max77833_freeze(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < ARRAY_SIZE(max77833_dumpaddr_pmic); i++)
		max77833_read_reg(i2c, max77833_dumpaddr_pmic[i],
				&max77833->reg_pmic_dump[i]);
#if 0
	for (i = 0; i < ARRAY_SIZE(max77833_dumpaddr_muic); i++)
		max77833_read_reg(i2c, max77833_dumpaddr_muic[i],
				&max77833->reg_muic_dump[i]);
#endif
	for (i = 0; i < ARRAY_SIZE(max77833_dumpaddr_led); i++)
		max77833_read_reg(i2c, max77833_dumpaddr_led[i],
				&max77833->reg_led_dump[i]);

	disable_irq(max77833->irq);

	return 0;
}

static int max77833_restore(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77833_dev *max77833 = i2c_get_clientdata(i2c);
	int i;

	enable_irq(max77833->irq);

	for (i = 0; i < ARRAY_SIZE(max77833_dumpaddr_pmic); i++)
		max77833_write_reg(i2c, max77833_dumpaddr_pmic[i],
				max77833->reg_pmic_dump[i]);
#if 0
	for (i = 0; i < ARRAY_SIZE(max77833_dumpaddr_muic); i++)
		max77833_write_reg(i2c, max77833_dumpaddr_muic[i],
				max77833->reg_muic_dump[i]);
#endif
	for (i = 0; i < ARRAY_SIZE(max77833_dumpaddr_led); i++)
		max77833_write_reg(i2c, max77833_dumpaddr_led[i],
				max77833->reg_led_dump[i]);

	return 0;
}
#endif

const struct dev_pm_ops max77833_pm = {
	.suspend = max77833_suspend,
	.resume = max77833_resume,
#ifdef CONFIG_HIBERNATION
	.freeze =  max77833_freeze,
	.thaw = max77833_restore,
	.restore = max77833_restore,
#endif
};

static struct i2c_driver max77833_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &max77833_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= max77833_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= max77833_i2c_probe,
	.remove		= max77833_i2c_remove,
	.id_table	= max77833_i2c_id,
};

static int __init max77833_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&max77833_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(max77833_i2c_init);

static void __exit max77833_i2c_exit(void)
{
	i2c_del_driver(&max77833_i2c_driver);
}
module_exit(max77833_i2c_exit);

MODULE_DESCRIPTION("MAXIM 77833 multi-function core driver");
MODULE_AUTHOR("SeoYoung Jeong <seo0.jeong@samsung.com>");
MODULE_LICENSE("GPL");
