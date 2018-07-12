/*
 * sec-core.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/irqnr.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/mfd/core.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/irq.h>
#include <linux/mfd/samsung/rtc.h>
#include <linux/regmap.h>

#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890)
#ifdef CONFIG_EXYNOS_MBOX
#include <mach/apm-exynos.h>
#endif
#else
#ifdef CONFIG_EXYNOS_MBOX
#include <linux/apm-exynos.h>
#include <linux/mailbox-exynos.h>
#endif
#endif

#ifdef CONFIG_EXYNOS_MBOX
enum {
	PMIC,
	RTC,
};
unsigned int apm_status = 0;
static DEFINE_MUTEX(sec_lock);
#endif

static struct mfd_cell s5m8751_devs[] = {
	{
		.name = "s5m8751-pmic",
	}, {
		.name = "s5m-charger",
	}, {
		.name = "s5m8751-codec",
	},
};

static struct mfd_cell s5m8763_devs[] = {
	{
		.name = "s5m8763-pmic",
	}, {
		.name = "s5m-rtc",
	}, {
		.name = "s5m-charger",
	},
};

static struct mfd_cell s5m8767_devs[] = {
	{
		.name = "s5m8767-pmic",
	}, {
		.name = "s5m-rtc",
	},
};

static struct mfd_cell s2mps11_devs[] = {
	{
		.name = "s2mps11-pmic",
	}, {
		.name = "s2m-rtc",
	},
};

static struct mfd_cell s2mps13_devs[] = {
	{
		.name = "s2mps13-pmic",
	}, {
		.name = "s2m-rtc",
	},
};

static struct mfd_cell s2mps15_devs[] = {
	{
		.name = "s2mps15-pmic",
	}, {
		.name = "s2m-rtc",
	},
};

static struct mfd_cell s2mpu03_devs[] = {
	{
		.name = "s2mpu03-pmic",
	}, {
		.name = "s2m-rtc",
	},
};

static struct mfd_cell s2mps16_devs[] = {
	{
		.name = "s2mps16-pmic",
	}, {
		.name = "s2m-rtc",
	},
};

#ifdef CONFIG_OF
static struct of_device_id sec_dt_match[] = {
	{	.compatible = "samsung,s5m8767-pmic",
		.data = (void *)S5M8767X,
	},
	{	.compatible = "samsung,s2mps13-pmic",
		.data = (void *)S2MPS13X,
	},
	{	.compatible = "samsung,s2mps11-pmic",
		.data = (void *)S2MPS11X,
	},
	{	.compatible = "samsung,s2mps15-pmic",
		.data = (void *)S2MPS15X,
	},
	{	.compatible = "samsung,s2mpu03-pmic",
		.data = (void *)S2MPU03X,
	},
	{	.compatible = "samsung,s2mps16-pmic",
		.data = (void *)S2MPS16X,
	},
	{},
};
#endif

#ifdef CONFIG_EXYNOS_MBOX
static int exynos_regulator_apm_notifier(struct notifier_block *notifier,
						unsigned long pm_event, void *v)
{
	switch (pm_event) {
		case APM_READY:
			apm_status = true;
			break;
		case APM_SLEEP:
		case APM_TIMEOUT:
			apm_status = false;
			break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_apm_notifier = {
	.notifier_call = exynos_regulator_apm_notifier,
};
#endif

int sec_reg_read(struct sec_pmic_dev *sec_pmic, u32 reg, void *dest)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);

	if (!apm_status) {
		ret = regmap_read(sec_pmic->regmap, reg, dest);
	} else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_read(PMIC, reg, dest);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0) {
			ret = regmap_read(sec_pmic->regmap, reg, dest);
		}
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_read(sec_pmic->regmap, reg, dest);
#endif
}
EXPORT_SYMBOL_GPL(sec_reg_read);

int sec_bulk_read(struct sec_pmic_dev *sec_pmic, u32 reg, int count, u8 *buf)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);

	if (!apm_status)
		ret = regmap_bulk_read(sec_pmic->regmap, reg, buf, count);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_bulk_read(PMIC, reg, buf, count);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0) {
			ret = regmap_bulk_read(sec_pmic->regmap, reg, buf, count);
		}
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_bulk_read(sec_pmic->regmap, reg, buf, count);
#endif
}
EXPORT_SYMBOL_GPL(sec_bulk_read);

int sec_reg_write(struct sec_pmic_dev *sec_pmic, u32 reg, u32 value)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);
	if (!apm_status)
		ret = regmap_write(sec_pmic->regmap, reg, value);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_write(PMIC, reg, value);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0) {
			ret = regmap_write(sec_pmic->regmap, reg, value);
		}
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_write(sec_pmic->regmap, reg, value);
#endif
}
EXPORT_SYMBOL_GPL(sec_reg_write);

int sec_bulk_write(struct sec_pmic_dev *sec_pmic, u32 reg, int count, u8 *buf)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);
	if (!apm_status)
		ret = regmap_raw_write(sec_pmic->regmap, reg, buf, count);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_bulk_write(PMIC, reg, buf, count);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0) {
			ret = regmap_raw_write(sec_pmic->regmap, reg, buf, count);
		}
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_raw_write(sec_pmic->regmap, reg, buf, count);
#endif
}
EXPORT_SYMBOL_GPL(sec_bulk_write);

int sec_reg_update(struct sec_pmic_dev *sec_pmic, u32 reg, u32 val, u32 mask)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);
	if (!apm_status)
		ret = regmap_update_bits(sec_pmic->regmap, reg, mask, val);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_update_bits(PMIC, reg, mask, val);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0) {
			ret = regmap_update_bits(sec_pmic->regmap, reg, mask, val);
		}
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_update_bits(sec_pmic->regmap, reg, mask, val);
#endif
}
EXPORT_SYMBOL_GPL(sec_reg_update);

int sec_rtc_read(struct sec_pmic_dev *sec_pmic, u32 reg, void *dest)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);
	if (!apm_status)
		ret = regmap_read(sec_pmic->rtc_regmap, reg, dest);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_read(RTC, reg, dest);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0)
			ret = regmap_read(sec_pmic->rtc_regmap, reg, dest);
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_read(sec_pmic->rtc_regmap, reg, dest);
#endif
}
EXPORT_SYMBOL_GPL(sec_rtc_read);

int sec_rtc_bulk_read(struct sec_pmic_dev *sec_pmic, u32 reg, int count,
		u8 *buf)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);
	if (!apm_status)
		ret = regmap_bulk_read(sec_pmic->rtc_regmap, reg, buf, count);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_bulk_read(RTC, reg, buf, count);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0)
			ret = regmap_bulk_read(sec_pmic->rtc_regmap, reg, buf, count);
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_bulk_read(sec_pmic->rtc_regmap, reg, buf, count);
#endif
}
EXPORT_SYMBOL_GPL(sec_rtc_bulk_read);

int sec_rtc_write(struct sec_pmic_dev *sec_pmic, u32 reg, u32 value)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);
	if (!apm_status)
		ret = regmap_write(sec_pmic->rtc_regmap, reg, value);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_write(RTC, reg, value);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0)
			ret = regmap_write(sec_pmic->rtc_regmap, reg, value);
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_write(sec_pmic->rtc_regmap, reg, value);
#endif
}
EXPORT_SYMBOL_GPL(sec_rtc_write);

int sec_rtc_bulk_write(struct sec_pmic_dev *sec_pmic, u32 reg, int count,
		u8 *buf)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);
	if (!apm_status)
		ret = regmap_raw_write(sec_pmic->rtc_regmap, reg, buf, count);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_bulk_write(RTC, reg, buf, count);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0)
			ret = regmap_raw_write(sec_pmic->rtc_regmap, reg, buf, count);
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_raw_write(sec_pmic->rtc_regmap, reg, buf, count);
#endif
}
EXPORT_SYMBOL_GPL(sec_rtc_bulk_write);

int sec_rtc_update(struct sec_pmic_dev *sec_pmic, u32 reg, u32 val,
		u32 mask)
{
#ifdef CONFIG_EXYNOS_MBOX
	int ret;

	mutex_lock(&sec_lock);
	if (!apm_status)
		ret = regmap_update_bits(sec_pmic->rtc_regmap, reg, mask, val);
	else {
		mutex_lock(&sec_pmic->iolock);
		ret = sec_pmic->ops->apm_update_bits(RTC, reg, mask, val);
		mutex_unlock(&sec_pmic->iolock);
		if (ret < 0)
			ret = regmap_update_bits(sec_pmic->rtc_regmap, reg, mask, val);
	}
	mutex_unlock(&sec_lock);

	return ret;
#else
	return regmap_update_bits(sec_pmic->rtc_regmap, reg, mask, val);
#endif
}
EXPORT_SYMBOL_GPL(sec_rtc_update);

#ifdef CONFIG_EXYNOS_MBOX
void sec_core_lock(void)
{
	mutex_lock(&sec_lock);
}
EXPORT_SYMBOL_GPL(sec_core_lock);

void sec_core_unlock(void)
{
	mutex_unlock(&sec_lock);
}
EXPORT_SYMBOL_GPL(sec_core_unlock);
#endif

static struct regmap_config sec_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};


#ifdef CONFIG_OF
static struct sec_platform_data *sec_pmic_i2c_parse_dt_pdata(
					struct device *dev)
{
	struct sec_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;
	u32 val;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
	dev->platform_data = pdata;
	pdata->irq_base = -1;

	/* WTSR, SMPL */
	pdata->wtsr_smpl = devm_kzalloc(dev, sizeof(*pdata->wtsr_smpl),
			GFP_KERNEL);
	if (!pdata->wtsr_smpl)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_u32(np, "wtsr_en", &val);
	if (ret)
		return ERR_PTR(ret);
	pdata->wtsr_smpl->wtsr_en = !!val;

	ret = of_property_read_u32(np, "smpl_en", &val);
	if (ret)
		return ERR_PTR(ret);
	pdata->wtsr_smpl->smpl_en = !!val;

	ret = of_property_read_u32(np, "wtsr_timer_val",
			&pdata->wtsr_smpl->wtsr_timer_val);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "smpl_timer_val",
			&pdata->wtsr_smpl->smpl_timer_val);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "check_jigon", &val);
	if (ret)
		return ERR_PTR(ret);
	pdata->wtsr_smpl->check_jigon = !!val;

	if (of_get_property(np, "ten-bit-address", NULL))
		pdata->ten_bit_address = true;

	/* init time */
	pdata->init_time = devm_kzalloc(dev, sizeof(*pdata->init_time),
			GFP_KERNEL);
	if (!pdata->init_time)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_u32(np, "init_time,sec",
			&pdata->init_time->tm_sec);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "init_time,min",
			&pdata->init_time->tm_min);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "init_time,hour",
			&pdata->init_time->tm_hour);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "init_time,mday",
			&pdata->init_time->tm_mday);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "init_time,mon",
			&pdata->init_time->tm_mon);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "init_time,year",
			&pdata->init_time->tm_year);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "init_time,wday",
			&pdata->init_time->tm_wday);
	if (ret)
		return ERR_PTR(ret);

	return pdata;
}
#else
static struct sec_platform_data *sec_pmic_i2c_parse_dt_pdata(
					struct device *dev)
{
	return NULL;
}
#endif

static inline int sec_i2c_get_driver_data(struct i2c_client *i2c,
						const struct i2c_device_id *id)
{
#ifdef CONFIG_OF
	if (i2c->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(sec_dt_match, i2c->dev.of_node);
		return (unsigned long)match->data;
	}
#endif
	return (int)id->driver_data;
}

static int sec_pmic_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct sec_platform_data *pdata = i2c->dev.platform_data;
	struct sec_pmic_dev *sec_pmic;
	int ret;

	sec_pmic = devm_kzalloc(&i2c->dev, sizeof(struct sec_pmic_dev),
				GFP_KERNEL);
	if (sec_pmic == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, sec_pmic);
	sec_pmic->dev = &i2c->dev;
	sec_pmic->i2c = i2c;
	sec_pmic->irq = i2c->irq;
	sec_pmic->type = sec_i2c_get_driver_data(i2c, id);

	mutex_init(&sec_pmic->iolock);

	if (sec_pmic->dev->of_node) {
		pdata = sec_pmic_i2c_parse_dt_pdata(sec_pmic->dev);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			return ret;
		}
		pdata->device_type = sec_pmic->type;
	}

	if (pdata) {
		sec_pmic->device_type = pdata->device_type;
		sec_pmic->ono = pdata->ono;
		sec_pmic->irq_base = pdata->irq_base;
		sec_pmic->wakeup = true;
		sec_pmic->pdata = pdata;
		sec_pmic->irq = i2c->irq;
	}

	sec_pmic->regmap = devm_regmap_init_i2c(i2c, &sec_regmap_config);
	if (IS_ERR(sec_pmic->regmap)) {
		ret = PTR_ERR(sec_pmic->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	sec_pmic->rtc = i2c_new_dummy(i2c->adapter, RTC_I2C_ADDR);
	if (!sec_pmic->rtc) {
		dev_err(&i2c->dev, "Failed to allocate I2C for RTC\n");
		return -ENODEV;
	}

	if (pdata && pdata->ten_bit_address)
		sec_pmic->rtc->flags |= I2C_CLIENT_TEN;

	i2c_set_clientdata(sec_pmic->rtc, sec_pmic);
	sec_pmic->rtc_regmap = devm_regmap_init_i2c(sec_pmic->rtc,
						&sec_regmap_config);
	if (IS_ERR(sec_pmic->rtc_regmap)) {
		ret = PTR_ERR(sec_pmic->rtc_regmap);
		dev_err(&sec_pmic->rtc->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	if (pdata && pdata->cfg_pmic_irq)
		pdata->cfg_pmic_irq();

	sec_irq_init(sec_pmic);
#ifdef CONFIG_EXYNOS_MBOX
	sec_pmic->ops = &exynos_apm_function_ops;
	register_apm_notifier(&exynos_apm_notifier);
#endif
	pm_runtime_set_active(sec_pmic->dev);

	switch (sec_pmic->device_type) {
	case S5M8751X:
		ret = mfd_add_devices(sec_pmic->dev, -1, s5m8751_devs,
				      ARRAY_SIZE(s5m8751_devs), NULL, 0, NULL);
		break;
	case S5M8763X:
		ret = mfd_add_devices(sec_pmic->dev, -1, s5m8763_devs,
				      ARRAY_SIZE(s5m8763_devs), NULL, 0, NULL);
		break;
	case S5M8767X:
		ret = mfd_add_devices(sec_pmic->dev, -1, s5m8767_devs,
				      ARRAY_SIZE(s5m8767_devs), NULL, 0, NULL);
		break;
	case S2MPS11X:
		ret = mfd_add_devices(sec_pmic->dev, -1, s2mps11_devs,
				      ARRAY_SIZE(s2mps11_devs), NULL, 0, NULL);
		break;
	case S2MPS13X:
		ret = mfd_add_devices(sec_pmic->dev, -1, s2mps13_devs,
				      ARRAY_SIZE(s2mps13_devs), NULL, 0, NULL);
		break;
	case S2MPS15X:
		ret = mfd_add_devices(sec_pmic->dev, -1, s2mps15_devs,
				      ARRAY_SIZE(s2mps15_devs), NULL, 0, NULL);
		break;
	case S2MPU03X:
		ret = mfd_add_devices(sec_pmic->dev, -1, s2mpu03_devs,
				      ARRAY_SIZE(s2mpu03_devs), NULL, 0, NULL);
		break;
	case S2MPS16X:
		ret = mfd_add_devices(sec_pmic->dev, -1, s2mps16_devs,
				      ARRAY_SIZE(s2mps16_devs), NULL, 0, NULL);
		break;
	default:
		/* If this happens the probe function is problem */
		BUG();
	}

	if (ret < 0)
		goto err;

	return ret;

err:
	mfd_remove_devices(sec_pmic->dev);
	sec_irq_exit(sec_pmic);
	i2c_unregister_device(sec_pmic->rtc);
#ifdef CONFIG_EXYNOS_MBOX
	unregister_apm_notifier(&exynos_apm_notifier);
#endif
	return ret;
}

static int sec_pmic_remove(struct i2c_client *i2c)
{
	struct sec_pmic_dev *sec_pmic = i2c_get_clientdata(i2c);

	mfd_remove_devices(sec_pmic->dev);
	sec_irq_exit(sec_pmic);
	regmap_exit(sec_pmic->rtc_regmap);
	regmap_exit(sec_pmic->regmap);
	i2c_unregister_device(sec_pmic->rtc);
	return 0;
}

static const struct i2c_device_id sec_pmic_id[] = {
	{ "sec_pmic", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sec_pmic_id);

#ifdef CONFIG_PM
static int sec_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sec_pmic_dev *sec_pmic = i2c_get_clientdata(i2c);

	if (sec_pmic->wakeup)
		enable_irq_wake(sec_pmic->irq);

	disable_irq(sec_pmic->irq);
	if (sec_pmic->device_type == S2MPS15X && sec_pmic->adc_en)
		sec_reg_write(sec_pmic, 0x5C, 0x00);
	else if (sec_pmic->device_type == S2MPS16X && sec_pmic->adc_en)
		sec_reg_write(sec_pmic, 0x6B, 0x00);
	return 0;
}

static int sec_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sec_pmic_dev *sec_pmic = i2c_get_clientdata(i2c);

	if (sec_pmic->wakeup)
		disable_irq_wake(sec_pmic->irq);

	enable_irq(sec_pmic->irq);
	if (sec_pmic->device_type == S2MPS15X && sec_pmic->adc_en)
		sec_reg_write(sec_pmic, 0x5C, 0x80);
	else if (sec_pmic->device_type == S2MPS16X && sec_pmic->adc_en)
		sec_reg_write(sec_pmic, 0x6B, 0x80);
	return 0;
}
#else
#define sec_suspend	NULL
#define sec_resume	NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_SEC_PM
extern unsigned int get_smpl_warn_number(void);
extern void clear_smpl_warn_number(void);

static unsigned int smpl_warn_counts = 0;
static int get_smpl_warn_count(char *val, const struct kernel_param *kp)
{
	smpl_warn_counts = get_smpl_warn_number();

	pr_info("%s: smpl_warn_counts=0x%x\n", __func__, smpl_warn_counts);

	return param_get_uint(val, kp);
}

static int clear_smpl_warn_count(const char *val, const struct kernel_param *kp)
{

	int rv = param_set_uint(val, kp);

	if(*val=='c') {
		clear_smpl_warn_number();  /* clear after reading */
		pr_info("%s: smpl_warn_counts clear\n", __func__);
	}

	return rv;
}

static struct kernel_param_ops smpl_warn_count_param_ops = {
	.get = get_smpl_warn_count,
	.set = clear_smpl_warn_count,
};

module_param_cb(smpl_warn_count, &smpl_warn_count_param_ops, &smpl_warn_counts, 0660);
MODULE_PARM_DESC(smpl_warn_count, "check smpl warn count");
#endif

const struct dev_pm_ops sec_pmic_apm = {
	.suspend = sec_suspend,
	.resume = sec_resume,
};

static struct i2c_driver sec_pmic_driver = {
	.driver = {
		   .name = "sec_pmic",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(sec_dt_match),
		   .pm = &sec_pmic_apm,
		   .suppress_bind_attrs = true,
	},
	.probe = sec_pmic_probe,
	.remove = sec_pmic_remove,
	.id_table = sec_pmic_id,
};

static int __init sec_pmic_init(void)
{
	return i2c_add_driver(&sec_pmic_driver);
}

subsys_initcall(sec_pmic_init);

static void __exit sec_pmic_exit(void)
{
	i2c_del_driver(&sec_pmic_driver);
}
module_exit(sec_pmic_exit);

MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("Core support for the SAMSUNG MFD");
MODULE_LICENSE("GPL");
