/*
 * Samsung Mobile VE Group.
 *
 * drivers/gpio/secgpio_dvs.c
 *
 * Drivers for samsung gpio debugging & verification.
 *
 * Copyright (C) 2013, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "secgpio_dvs.h"

/*sys fs*/
struct class *secgpio_dvs_class;
EXPORT_SYMBOL(secgpio_dvs_class);

struct device *secgpio_dotest;
EXPORT_SYMBOL(secgpio_dotest);

/* extern GPIOMAP_RESULT GpioMap_result; */
static struct gpio_dvs_t *gdvs_info;

static ssize_t checked_init_secgpio_file_read(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t checked_sleep_secgpio_file_read(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t checked_secgpio_init_read_details(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t checked_secgpio_sleep_read_details(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t secgpio_checked_sleepgpio_read(
	struct device *dev, struct device_attribute *attr, char *buf);

static DEVICE_ATTR(gpioinit_check, 0444,
	checked_init_secgpio_file_read, NULL);
static DEVICE_ATTR(gpiosleep_check, 0444,
	checked_sleep_secgpio_file_read, NULL);
static DEVICE_ATTR(check_init_detail, 0444,
	checked_secgpio_init_read_details, NULL);
static DEVICE_ATTR(check_sleep_detail, 0444,
	checked_secgpio_sleep_read_details, NULL);
static DEVICE_ATTR(checked_sleepGPIO, 0444,
	secgpio_checked_sleepgpio_read, NULL);

static struct attribute *secgpio_dvs_attributes[] = {
		&dev_attr_gpioinit_check.attr,
		&dev_attr_gpiosleep_check.attr,
		&dev_attr_check_init_detail.attr,
		&dev_attr_check_sleep_detail.attr,
		&dev_attr_checked_sleepGPIO.attr,
		NULL,
};

static struct attribute_group secgpio_dvs_attr_group = {
		.attrs = secgpio_dvs_attributes,
};

static ssize_t checked_init_secgpio_file_read(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_buf[20];
	struct gpio_dvs_t *gdvs = dev_get_drvdata(dev);

	for (i = 0; i < gdvs->count; i++) {
		memset(temp_buf, 0, sizeof(char)*20);
		snprintf(temp_buf, 20, "%x ", gdvs->result->init[i]);
		strlcat(buf, temp_buf, PAGE_SIZE);
	}

	return strlen(buf);
}

static ssize_t checked_sleep_secgpio_file_read(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_buf[20];
	struct gpio_dvs_t *gdvs = dev_get_drvdata(dev);

	for (i = 0; i < gdvs->count; i++) {
		memset(temp_buf, 0, sizeof(char)*20);
		snprintf(temp_buf, 20, "%x ", gdvs->result->sleep[i]);
		strlcat(buf, temp_buf, PAGE_SIZE);
	}

	return strlen(buf);
}

static ssize_t checked_secgpio_init_read_details(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_buf[20];
	struct gpio_dvs_t *gdvs = dev_get_drvdata(dev);

	for (i = 0; i < gdvs->count; i++) {
		memset(temp_buf, 0, sizeof(char)*20);
		snprintf(temp_buf, 20, "GI[%d] - %x\n ",
			i, gdvs->result->init[i]);
		strlcat(buf, temp_buf, PAGE_SIZE);
	}

	return strlen(buf);
}
static ssize_t checked_secgpio_sleep_read_details(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_buf[20];
	struct gpio_dvs_t *gdvs = dev_get_drvdata(dev);

	for (i = 0; i < gdvs->count; i++) {
		memset(temp_buf, 0, sizeof(char)*20);
		snprintf(temp_buf, 20, "GS[%d] - %x\n ",
			i, gdvs->result->sleep[i]);
		strlcat(buf, temp_buf, PAGE_SIZE);
	}

	return strlen(buf);

}

static ssize_t secgpio_checked_sleepgpio_read(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_dvs_t *gdvs = dev_get_drvdata(dev);

	if (gdvs->check_sleep)
		return snprintf(buf, PAGE_SIZE, "1");
	else
		return snprintf(buf, PAGE_SIZE, "0");
}

void gpio_dvs_check_initgpio(void)
{
	if (gdvs_info && gdvs_info->check_gpio_status)
		gdvs_info->check_gpio_status(PHONE_INIT);
}

void gpio_dvs_check_sleepgpio(void)
{
	if (unlikely(!gdvs_info->check_sleep) && gdvs_info) { 
		gdvs_info->check_gpio_status(PHONE_SLEEP);
		gdvs_info->check_sleep = true;
	}
}

#ifdef CONFIG_OF
static const struct of_device_id secgpio_dvs_dt_match[] = {
	{ .compatible = "samsung,exynos8890-secgpio-dvs",
		.data = (void *)&exynos8890_secgpio_dvs },
	{ },
};
MODULE_DEVICE_TABLE(of, secgpio_dvs_dt_match);

static struct gpio_dvs_t *secgpio_dvs_get_soc_data(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device_node *node = pdev->dev.of_node;
	struct gpio_dvs_t *gdvs;

	match = of_match_node(secgpio_dvs_dt_match, node);
	if (!match) {
		dev_err(&pdev->dev, "failed to get SoC node\n");
		return NULL;
	}

	gdvs = (struct gpio_dvs_t *)match->data;
	if (!gdvs) {
		dev_err(&pdev->dev, "failed to get SoC data\n");
		return NULL;
	}

	return gdvs;
}
#else
static struct gpio_dvs_t *secgpio_dvs_get_soc_data(struct platform_device *pdev)
{
	return dev_get_platdata(&pdev->dev);
}
#endif

static int secgpio_dvs_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct class *secgpio_dvs_class;
	struct device *secgpio_dotest;
	struct gpio_dvs_t *gdvs = secgpio_dvs_get_soc_data(pdev);

	if (!gdvs)
		return -ENODEV;

	gdvs->count = gdvs->get_nr_gpio();
	pr_info("[GPIO_DVS] gpio nr:%d\n", gdvs->count);

	gdvs->result->init = devm_kzalloc(&pdev->dev, gdvs->count, GFP_KERNEL);
	if (!gdvs->result->init)
		return -ENOMEM;

	gdvs->result->sleep = devm_kzalloc(&pdev->dev, gdvs->count, GFP_KERNEL);
	if (!gdvs->result->sleep)
		return -ENOMEM;

	secgpio_dvs_class = class_create(THIS_MODULE, "secgpio_check");
	if (IS_ERR(secgpio_dvs_class)) {
		ret = PTR_ERR(secgpio_dvs_class);
		pr_err("Failed to create class(secgpio_check_all)");
		goto fail_out;
	}

	secgpio_dotest = device_create(secgpio_dvs_class,
				NULL, 0, NULL, "secgpio_check_all");
	if (IS_ERR(secgpio_dotest)) {
		ret = PTR_ERR(secgpio_dotest);
		pr_err("Failed to create device(secgpio_check_all)");
		goto fail_device_create;
	}
	dev_set_drvdata(secgpio_dotest, gdvs);
	gdvs_info = gdvs;

	ret = sysfs_create_group(&secgpio_dotest->kobj,
			&secgpio_dvs_attr_group);
	if (ret) {
		pr_err("Failed to create sysfs group");
		goto fail_create_group;
	}

	return 0;

fail_create_group:
	device_unregister(secgpio_dotest);
fail_device_create:
	class_destroy(secgpio_dvs_class);
fail_out:
	if (ret)
		pr_err("%s: (err = %d)!\n", __func__, ret);

	return ret;

}

static int secgpio_dvs_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver secgpio_dvs = {
	.driver = {
		.name = "secgpio_dvs",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(secgpio_dvs_dt_match),
#endif
	},
	.probe = secgpio_dvs_probe,
	.remove = secgpio_dvs_remove,
};

module_platform_driver(secgpio_dvs);

MODULE_AUTHOR("intae.jun@samsung.com");
MODULE_DESCRIPTION("GPIO debugging and verification");
MODULE_LICENSE("GPL");
