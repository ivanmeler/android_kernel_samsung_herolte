/*
 *  sec_adc.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_adc.h>

struct adc_list {
	const char*	name;
	struct iio_channel *channel;
	bool is_used;
};

static struct adc_list batt_adc_list[] = {
	{.name = "adc-cable"},
	{.name = "adc-bat"},
	{.name = "adc-temp"},
	{.name = "adc-temp"},
	{.name = "adc-full"},
	{.name = "adc-volt"},
	{.name = "adc-chg-temp"},
	{.name = "adc-in-bat"},
	{.name = "adc-dischg"},
	{.name = "adc-dischg-ntc"},
	{.name = "adc-wpc-temp"},
	{.name = "adc-slave-chg-temp"},
	{.name = "adc-coil-temp"},
};

static void sec_bat_adc_ap_init(struct platform_device *pdev)
{
	int i = 0;
	struct iio_channel *temp_adc;

	for (i = 0; i < SEC_BAT_ADC_CHANNEL_NUM; i++) {
		temp_adc = iio_channel_get(&pdev->dev, batt_adc_list[i].name);
		batt_adc_list[i].channel = temp_adc;
		batt_adc_list[i].is_used = !IS_ERR_OR_NULL(temp_adc);
	}
}

static int sec_bat_adc_ap_read(int channel)
{
	int data = -1;
	int ret = 0;

	ret = (batt_adc_list[channel].is_used) ?
		iio_read_channel_raw(batt_adc_list[channel].channel, &data) : 0;

	return data;
}

static void sec_bat_adc_ap_exit(void)
{
	int i = 0;
	for (i = 0; i < SEC_BAT_ADC_CHANNEL_NUM; i++) {
		if (batt_adc_list[i].is_used) {
			iio_channel_release(batt_adc_list[i].channel);
		}
	}
}

static void sec_bat_adc_none_init(struct platform_device *pdev)
{
}

static int sec_bat_adc_none_read(int channel)
{
	return 0;
}

static void sec_bat_adc_none_exit(void)
{
}

static void sec_bat_adc_ic_init(struct platform_device *pdev)
{
}

static int sec_bat_adc_ic_read(int channel)
{
	return 0;
}

static void sec_bat_adc_ic_exit(void)
{
}
static int adc_read_type(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	if ((!battery->pdata->self_discharging_en) &&
	    ((channel == SEC_BAT_ADC_CHANNEL_DISCHARGING_CHECK) ||
	     (channel == SEC_BAT_ADC_CHANNEL_DISCHARGING_NTC))) {
		pr_info("%s : Doesn't enable Self Discharging Algorithm\n", __func__);
		return 0;
	}

	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		adc = sec_bat_adc_none_read(channel);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		adc = sec_bat_adc_ap_read(channel);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		adc = sec_bat_adc_ic_read(channel);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
	return adc;
}

static void adc_init_type(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_init(pdev);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_init(pdev);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_init(pdev);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

static void adc_exit_type(struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

int adc_read(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	adc = adc_read_type(battery, channel);

	dev_dbg(battery->dev, "[%s]adc = %d\n", __func__, adc);

	return adc;
}

void adc_init(struct platform_device *pdev, struct sec_battery_info *battery)
{
	adc_init_type(pdev, battery);
}

void adc_exit(struct sec_battery_info *battery)
{
	adc_exit_type(battery);
}

