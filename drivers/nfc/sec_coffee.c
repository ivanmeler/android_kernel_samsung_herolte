/*
 * Control node
 *
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 * Author: Sangmi Park <sm0327.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 416, Maetan 3-dong, Yeongtong-gu, Suwon-si, Gyeonggi-do 443-742, Korea
 *
 * Last update: 2015-07-31
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/spi/spidev.h>
#include <asm/uaccess.h>

#define CALL_DEV_MAJOR 241
#define CALL_DEV_NAME "p3" //test!!!
#define POWER_ON 1
#define POWER_OFF 0

#undef FEATURE_ESE_WAKELOCK

/* Device specific macro and structure */
struct coffee_platform_data {
	struct miscdevice miscdev; /* char device as misc driver */
	bool tz_mode;
	spinlock_t ese_spi_lock;

	bool enabled_clk;
#ifdef FEATURE_ESE_WAKELOCK
	struct wake_lock ese_lock;
#endif
	unsigned long speed;
	const char *vdd_1p8;
};

#ifdef CONFIG_ESE_SECURE
static int coffee_clk_prepare(struct coffee_platform_data *data)
{
	struct clk *ese_spi_pclk, *ese_spi_sclk;
	/* clk name form clk-exynos8890.c */
	ese_spi_pclk = clk_get(NULL, "ese-spi-pclk");/*This can move to parsedt */

	if (IS_ERR(ese_spi_pclk)) {
		pr_err("Can't get ese_spi_pclk\n");
		return -1;
	}

	ese_spi_sclk = clk_get(NULL, "ese-spi-sclk");
	if (IS_ERR(ese_spi_sclk)) {
		pr_err("Can't get ese_spi_sclk\n");
		return -1;
	}

	clk_prepare_enable(ese_spi_pclk);
	clk_prepare_enable(ese_spi_sclk);
	clk_set_rate(ese_spi_sclk, data->speed *2);
	usleep_range(3000, 3100);
	pr_info("%s open sclk rate:%lu\n", __func__, clk_get_rate(ese_spi_sclk));

	clk_put(ese_spi_pclk);
	clk_put(ese_spi_sclk);
	pr_info("%s: finished.\n",__func__);

	return 0;
}

static int coffee_clk_unprepare(struct coffee_platform_data *data)
{
	struct clk *ese_spi_pclk, *ese_spi_sclk;

	ese_spi_pclk = clk_get(NULL, "ese-spi-pclk");
	if (IS_ERR(ese_spi_pclk))
		pr_err("Can't get ese_spi_pclk\n");

	ese_spi_sclk = clk_get(NULL, "ese-spi-sclk");
	if (IS_ERR(ese_spi_sclk))
		pr_err("Can't get ese_spi_sclk\n");

	clk_disable_unprepare(ese_spi_pclk);
	clk_disable_unprepare(ese_spi_sclk);

	clk_put(ese_spi_pclk);
	clk_put(ese_spi_sclk);
	pr_info("%s: finished.\n",__func__);

	return 0;
}
#endif

static int coffee_regulator_onoff(struct coffee_platform_data *pdata,
		int onoff)
{
	int rc = 0;
	struct regulator *regulator_1p8;

	if (!pdata->vdd_1p8) {
		pr_err("%s no name for vdd_1p8 \n", __func__);
		return -1;
	}
	pr_info("%s [%d]\[%s]\n", __func__, onoff, pdata->vdd_1p8);
	regulator_1p8 = regulator_get(NULL, pdata->vdd_1p8);
	if (IS_ERR(regulator_1p8) || regulator_1p8 == NULL) {
		pr_err("%s - regulator_1p8 getting fail\n", __func__);
		return -ENODEV;
	}


	if (onoff == POWER_ON) {
		rc = regulator_enable(regulator_1p8);
		if (rc) {
			pr_err("%s - enable regulator_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_1p8);
		if (rc) {
			pr_err("%s - disable regulator_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	}

	done:
	regulator_put(regulator_1p8);

	return rc;
}

static int coffee_open (struct inode *inode, struct file *file)
{
	struct coffee_platform_data *pdata;
	int ret=0;
#if 0
	struct miscdevice *misc;

	misc = file->private_data;
	if (misc)
		pdata = dev_get_drvdata(misc->this_device);
#endif

	pdata = container_of(file->private_data,
						struct coffee_platform_data, miscdev);

	if (!pdata) {
		pr_err("%s cannot get pdata.\n", __func__);
		return -ENXIO;
	}

	pr_info("%s speed=%lu\n", __func__, pdata->speed);
#ifdef FEATURE_ESE_WAKELOCK
	if (wake_lock_active(&pdata->ese_lock)) {
		pr_err("%s Already opened\n", __func__);
	}
	else
		wake_lock(&pdata->ese_lock);
#endif

	ret = coffee_regulator_onoff(pdata, POWER_ON);
	if (ret < 0)
		pr_info("%s good to turn on regulator\n", __func__);
#ifdef CONFIG_ESE_SECURE
	ret = coffee_clk_prepare(pdata);
	if (ret < 0)
		pr_info("%s good to prepare clk.\n", __func__);
#endif
	pr_info("%s... [%d]\n", __func__, ret);

	return ret;
}
static int coffee_release (struct inode *inode, struct file *file)
{
	struct coffee_platform_data *pdata;
	int ret=0;

	pr_info("%s\n", __func__);

#ifdef FEATURE_ESE_WAKELOCK
	if (wake_lock_active(&pdata->ese_lock)) {
		wake_unlock(&pdata->ese_lock);
		pr_info("%s wake UNlocked", __func__);
	}
#endif

	pdata = container_of(file->private_data,
						struct coffee_platform_data, miscdev);
	if (!pdata) {
		pr_err("%s cannot get pdata.\n", __func__);
		return -ENXIO;
	}

	ret = coffee_regulator_onoff(pdata, POWER_OFF);
	if (ret < 0)
		pr_err("%s good to turn off regulator.\n",__func__);
#ifdef CONFIG_ESE_SECURE
	ret = coffee_clk_unprepare(pdata);
	if (ret < 0)
		pr_info("%s good to unprepare clk.\n", __func__);
#endif

	return 0;
}
static long coffee_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	/*pr_info("%s ", __func__);*/

	return 0;
}

static const struct file_operations coffee_node = {
	.owner          = THIS_MODULE,
	.open           = coffee_open,
	.release        = coffee_release,
	.unlocked_ioctl = coffee_ioctl,
	.compat_ioctl = coffee_ioctl,
};

static ssize_t latte_show(struct class *dev,
					struct class_attribute *attr,
					char *buf)
{
	int size=0;

	pr_info("%s coffee\n", __func__);

	return size;
}

static ssize_t latte_write(struct class *dev,
					struct class_attribute *attr,
					const char *buf, size_t size)
{
	char data[4]={0,};
	//struct coffee_platform_data *pdata = dev_get_drvdata(dev);

	pr_info("%s coffee.\n", __func__);
	memcpy (data, buf, 4);

	if(data[0] == '0'){
		pr_info("latte coffee: power off\n");
		//coffee_regulator_onoff(pdata, POWER_OFF);
	}else{
		pr_info("latte coffee: power on\n");
		//coffee_regulator_onoff(pdata, POWER_ON);
	}

	return size;
}
static CLASS_ATTR(ice, 0664, latte_show, latte_write);
#if 0
static struct miscdevice misc_coffee = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = CALL_DEV_NAME,
	.fops  = &coffee_node,
};
#endif

static int sec_coffee_probe(struct platform_device *pdev)
{
	int status=0;
	struct class *nfc_class;
	struct coffee_platform_data *pdata = NULL;

	pr_info("%s \n", __func__);

	/* Parse DT */
	if (pdev->dev.of_node) {
		struct device_node *np = pdev->dev.of_node;
		pdata = kzalloc(sizeof(struct coffee_platform_data), GFP_KERNEL);
		if (!pdata) {
			pr_err("%s Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		if (of_property_read_string(np, "coffee-vdd-1p8",
			&pdata->vdd_1p8) < 0) {
			pr_err("%s - getting vdd_1p8 error\n", __func__);
			pdata->vdd_1p8 = NULL;
		}
		else
			pr_info("%s success parseDT[%s]", __func__,
					pdata->vdd_1p8);
		pdata->speed = 6500000L;
	}
	else {
		pr_err("%s cannot parse DTS\n", __func__);
		return status;
	}

	pdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	pdata->miscdev.name = CALL_DEV_NAME;
	pdata->miscdev.fops = &coffee_node;
	pdata->miscdev.parent = &pdev->dev;

	status = misc_register(&pdata->miscdev);
	dev_set_drvdata(&pdev->dev, pdata);
#if 0
	status = misc_register(&misc_coffee);
	if (status < 0)
		pr_err("%s misc_register failed! %d\n", __func__,
				status);
	dev_set_drvdata(misc_coffee.this_device, pdata);
#endif

#ifdef FEATURE_ESE_WAKELOCK /*wake lock for spi communication*/
	wake_lock_init(&pdata->ese_lock, WAKE_LOCK_SUSPEND, "ese_wake_lock");
#endif

	nfc_class = class_create(THIS_MODULE, "latte");
	if (IS_ERR(&nfc_class))
		pr_err("%s failed to create coffee class\n", __func__);
	else {
		status = class_create_file(nfc_class, &class_attr_ice);
		if (status)
			pr_err("%s failed to create attr_test\n", __func__);
	}
	/* device_creat_file */

	return status;
}

static int sec_coffee_remove(struct platform_device *pdev)
{
	/*int ret=0;*/

	pr_info("%s \n", __func__);
#ifdef FEATURE_ESE_WAKELOCK
	//wake_lock_destroy(&gto->ese_lock);
#endif

	return 0;
}

static const struct of_device_id sec_coffee_match[] = {
	{ .compatible = "sec-coffee" },
	{ }
};

static struct platform_driver sec_coffee_driver = {
	.driver = {
		.name = "sec-coffee",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_coffee_match),
	},
	.probe = sec_coffee_probe,
	.remove = sec_coffee_remove,
};

static int __init sec_coffee_init(void)
{
	/*int ret;*/

	pr_err("%s\n", __func__);

	return platform_driver_register(&sec_coffee_driver);
}


static void __exit sec_coffee_exit(void)
{

	return;
}

module_init(sec_coffee_init);
module_exit(sec_coffee_exit);

MODULE_DESCRIPTION("Samsung control driver");
MODULE_LICENSE("GPL");
