/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "../ssp.h"

#define	VENDOR		"AMS"
#if defined(CONFIG_SENSORS_SSP_TMG399x)
#define	CHIP_ID		"TMG399X"
#elif defined(CONFIG_SENSORS_SSP_TMD3782)
#define CHIP_ID		"TMD3782"
#elif defined(CONFIG_SENSORS_SSP_TMD4903)
#define CHIP_ID 	"TMD4903"
#elif defined(CONFIG_SENSORS_SSP_TMD4904)
#define CHIP_ID 	"TMD4904"
#elif defined(CONFIG_SENSORS_SSP_TMD4905)
#define CHIP_ID 	"TMD4905"
#else
#define CHIP_ID 	"UNKNOWN"
#endif

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/
static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	struct ssp_data *data = dev_get_drvdata(dev);
	int device_id = 0;
	device_id = get_proximity_device_id(data);

	if(device_id == TMD4903)
	{
		return sprintf(buf, "%s\n", "TMD4903");
	}
	else if(device_id ==  TMD4904)
	{
		return sprintf(buf, "%s\n", "TMD4904");
	}
	else
	{
		pr_err("[SSP]: %s - Unkown proximity device \n", __func__);
		return sprintf(buf, "%s\n", "UNKNOWN");
	}
#else
	return sprintf(buf, "%s\n", CHIP_ID);
#endif
}

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n",
		data->buf[LIGHT_SENSOR].r, data->buf[LIGHT_SENSOR].g,
		data->buf[LIGHT_SENSOR].b, data->buf[LIGHT_SENSOR].w,
		data->buf[LIGHT_SENSOR].a_time, data->buf[LIGHT_SENSOR].a_gain);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n",
		data->buf[LIGHT_SENSOR].r, data->buf[LIGHT_SENSOR].g,
		data->buf[LIGHT_SENSOR].b, data->buf[LIGHT_SENSOR].w,
		data->buf[LIGHT_SENSOR].a_time, data->buf[LIGHT_SENSOR].a_gain);
}

static ssize_t light_coef_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet, iReties = 0;
	struct ssp_msg *msg;
	int coef_buf[7];

	memset(coef_buf,0,sizeof(int)*7);
retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP]: %s - failed to allocate memory\n", __func__);
		return FAIL;
	}
	msg->cmd = MSG2SSP_AP_GET_LIGHT_COEF;
	msg->length = 28;
	msg->options = AP2HUB_READ;
	msg->buffer = (u8*)coef_buf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);

		if (iReties++ < 2) {
			pr_err("[SSP] %s fail, retry\n", __func__);
			mdelay(5);
			goto retries;
		}
		return FAIL;
	}

	pr_info("[SSP] %s - %d %d %d %d %d %d %d\n",__func__,
		coef_buf[0],coef_buf[1],coef_buf[2],coef_buf[3],coef_buf[4],coef_buf[5],coef_buf[6]);
	
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d\n", 
		coef_buf[0],coef_buf[1],coef_buf[2],coef_buf[3],coef_buf[4],coef_buf[5],coef_buf[6]);
}
#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static ssize_t light_coef_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet, i;
	int coef[7];
	char* token;
	char* str;
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s - %s\n", __func__,buf);

	//parsing
	str = (char*)buf;
	for(i=0 ; i<7; i++)
	{
		token = strsep(&str, " \n");
		if(token == NULL)
		{
			pr_err("[SSP] %s : too few arguments (7 needed)",__func__);
				return -EINVAL;
		}

		iRet = kstrtos32(token, 10, &coef[i]);
		if (iRet<0) {
			pr_err("[SSP] %s : kstrtou8 error %d",__func__,iRet);
			return iRet;
		}
	}

	memcpy(data->light_coef, coef, sizeof(data->light_coef));

	set_light_coef(data);
	
	return size;
}
#endif

static DEVICE_ATTR(vendor, S_IRUGO, light_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, light_name_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static DEVICE_ATTR(coef, S_IRUGO | S_IWUSR | S_IWGRP, light_coef_show, light_coef_store);
#else
static DEVICE_ATTR(coef, S_IRUGO, light_coef_show, NULL);
#endif

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_coef,
	NULL,
};

void initialize_light_factorytest(struct ssp_data *data)
{
	sensors_register(data->light_device, data, light_attrs, "light_sensor");
}

void remove_light_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->light_device, light_attrs);
}
