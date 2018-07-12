/*
 *  sec_step_charging.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "include/sec_battery.h"

#define STEP_CHARGING_CONDITION_VOLTAGE	0x01
#define STEP_CHARGING_CONDITION_SOC	0x02
#define STEP_CHARGING_CONDITION_CHARGE_POWER 0x04
#define STEP_CHARGING_CONDITION_ONLINE 0x08

void sec_bat_reset_step_charging(struct sec_battery_info *battery)
{
	battery->step_charging_status = -1;
}
/*
 * true: step is changed
 * false: not changed
 */
bool sec_bat_check_step_charging(struct sec_battery_info *battery)
{
	int i, value;

	if (!battery->step_charging_type)
		return false;

	if (battery->step_charging_type & STEP_CHARGING_CONDITION_CHARGE_POWER)
		if (battery->charge_power < battery->step_charging_charge_power)
			return false;

	if (battery->step_charging_type & STEP_CHARGING_CONDITION_ONLINE)
		if (!is_hv_wire_12v_type(battery->cable_type))
			return false;

	if (battery->step_charging_status < 0)
		i = 0;
	else
		i = battery->step_charging_status;

	if (battery->step_charging_type & STEP_CHARGING_CONDITION_VOLTAGE)
		value = battery->voltage_avg;
	else if (battery->step_charging_type & STEP_CHARGING_CONDITION_SOC)
		value = battery->capacity;
	else
		return false;
	
	while(i < battery->step_charging_step - 1) {
		if (value < battery->pdata->step_charging_condition[i]){
			break;
		}
		i++;
		if(battery->step_charging_status != -1)
			break;
	}

	if (i != battery->step_charging_status) {
		pr_info("%s : prev=%d, new=%d, value=%d, current=%d\n", __func__,
			battery->step_charging_status, i, value, battery->pdata->step_charging_current[i]);
		battery->pdata->charging_current[battery->cable_type].fast_charging_current = battery->pdata->step_charging_current[i];
		battery->step_charging_status = i;
		return true;
	}
	return false;
}

void sec_step_charging_init(struct sec_battery_info *battery, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret, len;
	sec_battery_platform_data_t *pdata = battery->pdata;
	unsigned int i;
	const u32 *p;

	ret = of_property_read_u32(np, "battery,step_charging_type",
			&battery->step_charging_type);
	pr_err("%s: step_charging_type 0x%x\n", __func__, battery->step_charging_type);
	if (ret) {
		pr_err("%s: step_charging_type is Empty\n", __func__);
		battery->step_charging_type = 0;
		return;
	}
	ret = of_property_read_u32(np, "battery,step_charging_charge_power",
			&battery->step_charging_charge_power);
	if (ret) {
		pr_err("%s: step_charging_charge_power is Empty\n", __func__);
		battery->step_charging_charge_power = 20000;
	}
	p = of_get_property(np, "battery,step_charging_condtion", &len);
	if (!p) {
		battery->step_charging_step = 0;
	} else {
		len = len / sizeof(u32);
		battery->step_charging_step = len;
		pdata->step_charging_condition = kzalloc(sizeof(u32) * len, GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,step_charging_condtion",
				pdata->step_charging_condition, len);
		if (ret) {
			pr_info("%s : step_charging_condtion read fail\n", __func__);
			battery->step_charging_step = 0;
		} else {
			pdata->step_charging_current = kzalloc(sizeof(u32) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,step_charging_current",
					pdata->step_charging_current, len);
			if (ret) {
				pr_info("%s : step_charging_current read fail\n", __func__);
				battery->step_charging_step = 0;
			} else {
				battery->step_charging_status = -1;
				for (i = 0; i < len; i++) {
					pr_info("%s : step condition(%d), current(%d)\n",
					__func__, pdata->step_charging_condition[i],
					pdata->step_charging_current[i]);
				}
				pdata->max_charging_current = pdata->step_charging_current[0]; 
			}
		}
	}
}
