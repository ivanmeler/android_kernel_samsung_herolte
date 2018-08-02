/*
 * sec_thermistor.c - SEC Thermistor
 *
 *  Copyright (C) 2013 Samsung Electronics
 *  Minsung Kim <ms925.kim@samsung.com>
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/iio/consumer.h>
#include <linux/platform_data/sec_thermistor.h>
#include <linux/sec_sysfs.h>

#define ADC_SAMPLING_CNT	5

struct sec_therm_info {
	struct device *dev;
	struct device *hwmon_dev;
	struct sec_therm_platform_data *pdata;
	struct iio_channel *chan;
	char name[PLATFORM_NAME_SIZE];
};

#ifdef CONFIG_OF
enum sec_thermistor_type {
	TYPE_SEC_THREM_AP,	/* Close to AP */
	TYPE_SEC_THREM_PA,	/* Close to PA */
	TYPE_SEC_THREM_CAM_FLASH,	/* Close to CAM_FLASH */
	NR_TYPE_SEC_TERM
};

static const struct platform_device_id sec_thermistor_id[] = {
	{ "sec-ap-thermistor", TYPE_SEC_THREM_AP },
	{ "sec-pa-thermistor", TYPE_SEC_THREM_PA },
	{ "sec-cf-thermistor", TYPE_SEC_THREM_CAM_FLASH },
	{ },
};

static const struct of_device_id sec_therm_match[] = {
	{ .compatible = "samsung,sec-ap-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_AP] },
	{ .compatible = "samsung,sec-pa-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_PA] },
	{ .compatible = "samsung,sec-cf-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_CAM_FLASH] },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_therm_match);

static bool sec_therm_single_inst[NR_TYPE_SEC_TERM] = {0,};

static struct sec_therm_platform_data *
sec_therm_parse_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sec_therm_platform_data *pdata;
	u32 len1, len2;
	int i;
	u32 adc, temp;

	if (!np)
		return NULL;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	if (!of_get_property(np, "adc_array", &len1))
		return ERR_PTR(-ENOENT);
	if (!of_get_property(np, "temp_array", &len2))
		return ERR_PTR(-ENOENT);

	if (len1 != len2) {
		dev_err(&pdev->dev, "%s: invalid array length(%u,%u)\n",
				__func__, len1, len2);
		return ERR_PTR(-EINVAL);
	}

	pdata->adc_arr_size = len1 / sizeof(u32);
	pdata->adc_table = devm_kzalloc(&pdev->dev,
			sizeof(*pdata->adc_table) * pdata->adc_arr_size,
			GFP_KERNEL);
	if (!pdata->adc_table)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < pdata->adc_arr_size; i++) {
		if (of_property_read_u32_index(np, "adc_array", i, &adc))
			return ERR_PTR(-EINVAL);
		if (of_property_read_u32_index(np, "temp_array", i, &temp))
			return ERR_PTR(-EINVAL);

		pdata->adc_table[i].adc = (int)adc;
		pdata->adc_table[i].temperature = (int)temp;
	}

	return pdata;
}
#else
static struct sec_therm_platform_data *
sec_therm_parse_dt(struct platform_device *pdev) { return NULL; }
#endif

static int sec_therm_get_adc_data(struct sec_therm_info *info)
{
	int adc_data;
	int adc_max = 0, adc_min = 0, adc_total = 0;
	int i;

	for (i = 0; i < ADC_SAMPLING_CNT; i++) {
		int ret = iio_read_channel_raw(info->chan, &adc_data);
		if (ret < 0) {
			dev_err(info->dev, "%s : err(%d) returned, skip read\n",
				__func__, adc_data);
			return ret;
		}

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (ADC_SAMPLING_CNT - 2);
}

static int convert_adc_to_temper(struct sec_therm_info *info, unsigned int adc)
{
	int low = 0;
	int high = 0;
	int temp = 0;
	int temp2 = 0;

	if (!info->pdata->adc_table || !info->pdata->adc_arr_size) {
		/* using fake temp */
		return 300;
	}

	high = info->pdata->adc_arr_size - 1;

	if (info->pdata->adc_table[low].adc >= adc)
		return info->pdata->adc_table[low].temperature;
	else if (info->pdata->adc_table[high].adc <= adc)
		return info->pdata->adc_table[high].temperature;

	while (low <= high) {
		int mid = 0;
		mid = (low + high) / 2;
		if (info->pdata->adc_table[mid].adc > adc)
			high = mid - 1;
		else if (info->pdata->adc_table[mid].adc < adc)
			low = mid + 1;
		else
			return info->pdata->adc_table[mid].temperature;
	}

	temp = info->pdata->adc_table[high].temperature;

	temp2 = (info->pdata->adc_table[low].temperature -
			info->pdata->adc_table[high].temperature) *
			(adc - info->pdata->adc_table[high].adc);

	temp += temp2 /
		(info->pdata->adc_table[low].adc -
			info->pdata->adc_table[high].adc);

	return temp;
}

static ssize_t sec_therm_show_temperature(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc, temp;

	adc = sec_therm_get_adc_data(info);

	if (adc < 0)
		return adc;
	else
		temp = convert_adc_to_temper(info, adc);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t sec_therm_show_temp_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc;

	adc = sec_therm_get_adc_data(info);

	return sprintf(buf, "%d\n", adc);
}

static ssize_t sec_therm_show_name(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", info->name);
}

static SENSOR_DEVICE_ATTR(temperature, S_IRUGO, sec_therm_show_temperature,
		NULL, 0);
static SENSOR_DEVICE_ATTR(temp_adc, S_IRUGO, sec_therm_show_temp_adc, NULL, 0);
static DEVICE_ATTR(name, S_IRUGO, sec_therm_show_name, NULL);

static struct attribute *sec_therm_attributes[] = {
	&sensor_dev_attr_temperature.dev_attr.attr,
	&sensor_dev_attr_temp_adc.dev_attr.attr,
	&dev_attr_name.attr,
	NULL
};

static const struct attribute_group sec_therm_attr_group = {
	.attrs = sec_therm_attributes,
};

static struct sec_therm_info *g_ap_therm_info;
int sec_therm_get_ap_temperature(void)
{
	int adc;
	int temp;

	if (unlikely(!g_ap_therm_info))
		return -ENODEV;

	adc = sec_therm_get_adc_data(g_ap_therm_info);

	if (adc < 0)
		return adc;
	else
		temp = convert_adc_to_temper(g_ap_therm_info, adc);

	return temp;
}

static int sec_therm_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(of_match_ptr(sec_therm_match), &pdev->dev);
	const struct platform_device_id *pdev_id;
	struct sec_therm_platform_data *pdata;
	struct sec_therm_info *info;
	int ret;

	dev_dbg(&pdev->dev, "%s: SEC Thermistor Driver Loading\n", __func__);

	pdata = sec_therm_parse_dt(pdev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	else if (pdata == NULL)
		pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "No platform init data supplied.\n");
		return -ENODEV;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	pdev_id = of_id ? of_id->data : platform_get_device_id(pdev);

	info->dev = &pdev->dev;
	info->pdata = pdata;
	strlcpy(info->name, pdev_id->name, sizeof(info->name));
	platform_set_drvdata(pdev, info);

	info->chan = iio_channel_get(&pdev->dev, NULL);
	if (IS_ERR(info->chan)) {
		dev_err(&pdev->dev, "%s: fail to get iio channel\n", __func__);
		return PTR_ERR(info->chan);
	}

	switch (pdev_id->driver_data) {
	case TYPE_SEC_THREM_AP:
	case TYPE_SEC_THREM_PA:
	case TYPE_SEC_THREM_CAM_FLASH:
		/* Allow only a single device instance for each device type */
		if (sec_therm_single_inst[pdev_id->driver_data])
			return -EPERM;
		else
			sec_therm_single_inst[pdev_id->driver_data] = true;

		info->dev = sec_device_create(info, pdev_id->name);
		if (IS_ERR(info->dev)) {
			dev_err(&pdev->dev, "%s: fail to create sec_dev\n",
					__func__);
			return PTR_ERR(info->dev);
		}
		break;
	default:
		dev_err(&pdev->dev, "%s: Unknown device type: %lu\n", __func__,
				pdev_id->driver_data);
		return -EINVAL;
	}

	ret = sysfs_create_group(&info->dev->kobj, &sec_therm_attr_group);
	if (ret) {
		dev_err(info->dev, "failed to create sysfs group\n");
		goto err_create_sysfs;
	}

	info->hwmon_dev = hwmon_device_register(info->dev);
	if (IS_ERR(info->hwmon_dev)) {
		dev_err(&pdev->dev, "unable to register as hwmon device.\n");
		ret = PTR_ERR(info->hwmon_dev);
		goto err_register_hwmon;
	}

	if (pdev_id->driver_data == TYPE_SEC_THREM_AP)
		g_ap_therm_info = info;

	dev_info(&pdev->dev, "%s successfully probed.\n", pdev_id->name);

	return 0;

err_register_hwmon:
	sysfs_remove_group(&info->dev->kobj, &sec_therm_attr_group);
err_create_sysfs:
	sec_device_destroy(info->dev->devt);
	return ret;
}

static int sec_therm_remove(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(of_match_ptr(sec_therm_match), &pdev->dev);
	struct sec_therm_info *info = platform_get_drvdata(pdev);
	const struct platform_device_id *pdev_id;

	if (!info)
		return 0;

	pdev_id = of_id ? of_id->data : platform_get_device_id(pdev);
	if (pdev_id->driver_data == TYPE_SEC_THREM_AP)
		g_ap_therm_info = NULL;

	hwmon_device_unregister(info->hwmon_dev);
	sysfs_remove_group(&info->dev->kobj, &sec_therm_attr_group);
	iio_channel_release(info->chan);
	sec_device_destroy(info->dev->devt);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver sec_thermistor_driver = {
	.driver = {
		.name = "sec-thermistor",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_therm_match),
	},
	.probe = sec_therm_probe,
	.remove = sec_therm_remove,
};

module_platform_driver(sec_thermistor_driver);

MODULE_DESCRIPTION("SEC Thermistor Driver");
MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sec-thermistor");
