/*
 *  sec_psb.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/battery/sec_psb.h>

#define DEFAULT_TRIGGER_TIME_AFTER_FULL		48 * 60 * 60
#define DEFAULT_RELEASE_DISCHARGING_TIME	2 * 60 * 60
#define DEFAULT_MAX_CYCLE					1000

#define CMD_MAX		0
#define CMD_MIN		1
#define CMD_ADD		2
#define CMD_CLEAR	3

typedef struct {
	unsigned int trigger_time_after_full;
	unsigned int release_discharging_time;
	unsigned int release_discharging_soc;
	unsigned int release_discharging_voltage;
	unsigned int max_cycle;
	unsigned int num_age_step;
	unsigned int *step_count_condition;
} sec_psb_pdata;

typedef struct {
	sec_psb_pdata *pdata;

	unsigned long start_charging_time;
	unsigned int prev_charging_time;
	bool		is_time_set;

	int raw_soc;

	unsigned int state;
	unsigned long start_time_after_full;
	unsigned long start_time_after_discharging;
	int adjust_cycle;
	int prevent_adjust_cycle;
	int adjust_age_step;
	unsigned int *step_count;

	unsigned int *data;
} sec_psb;

enum sec_psb_sysfs {
	PSB_DATA =	0,
#if defined(CONFIG_BATTERY_CISD)
	PSB_DATA_JSON,
#endif
	PSB_CONDITION,
};

enum sec_psb_state {
	PSB_STATE_NONE = 0,
	PSB_STATE_CHARGE_DONE,
	PSB_STATE_DISCHARGING,
	PSB_STATE_CHARGING,
};

enum sec_psb_type {
	PSB_TYPE_NONE	= 0, /* TEMP */
	PSB_TYPE_MAX_CHARGING_TIME,
	PSB_TYPE_TEMP_CHARGING_TIME,
	PSB_TYPE_LEVEL,
	PSB_TYPE_MAX_CYCLE,
	PSB_TYPE_STEP_COUNT,
};

#if defined(CONFIG_BATTERY_CISD)
const char *psb_data_json_str[] = {NULL, "MAX_CHARGING_TIME", NULL, "LEVEL", "MAX_CYCLE", "STEP_COUNT"};
#endif

static ssize_t sec_psb_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t sec_psb_store_attrs(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count);

#define SEC_PSB_ATTR(_name) \
{ \
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_psb_show_attrs, \
	.store = sec_psb_store_attrs, \
}

static struct device_attribute sec_psb_attrs[] = {
	SEC_PSB_ATTR(psb_data),
#if defined(CONFIG_BATTERY_CISD)
	SEC_PSB_ATTR(psb_data_json),
#endif
	SEC_PSB_ATTR(psb_condition),
};

static void sec_psb_set_data(sec_psb *psb, int index, int cmd, unsigned int value)
{
	switch (cmd) {
	case CMD_MAX:
		psb->data[index] = max(psb->data[index], value);
		break;
	case CMD_MIN:
		psb->data[index] = min(psb->data[index], value);
		break;
	case CMD_ADD:
		psb->data[index] += value;
		break;
	default:
		break;
	}
}

static bool sec_psb_check_step_count(struct sec_battery_info *battery, int step)
{
	static bool is_skip = false;
	sec_psb *psb = battery->psb;

	if (is_skip || step == 0)
		pr_info("%s: skip function\n", __func__);
	else if (psb->pdata->step_count_condition[step] &&
		psb->data[PSB_TYPE_STEP_COUNT + step] >= psb->pdata->step_count_condition[step])
		is_skip = true;

	return is_skip;
}

static bool sec_psb_update_age_data(struct sec_battery_info *battery, int cycle)
{
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	sec_psb *psb = battery->psb;
	int prev_step = psb->adjust_age_step;
	int calc_step = -1;

	for (calc_step = battery->pdata->num_age_step - 1; calc_step >= 0; calc_step--) {
		if (battery->pdata->age_data[calc_step].cycle <= cycle)
			break;
	}

	if (calc_step == prev_step)
		return false;

	psb->adjust_age_step = calc_step;
	sec_psb_set_data(psb, PSB_TYPE_STEP_COUNT + calc_step, CMD_ADD, 1);

	if (!sec_psb_check_step_count(battery, calc_step))
		return false;

	/* float voltage */
	battery->pdata->chg_float_voltage =
		battery->pdata->age_data[calc_step].float_voltage;
	battery->pdata->swelling_normal_float_voltage =
		battery->pdata->chg_float_voltage;
	/* full/recharge condition */
	battery->pdata->recharge_condition_vcell =
		battery->pdata->age_data[calc_step].recharge_condition_vcell;
	battery->pdata->full_condition_soc =
		battery->pdata->age_data[calc_step].full_condition_soc;
	battery->pdata->full_condition_vcell =
		battery->pdata->age_data[calc_step].full_condition_vcell;
	return true;
#else
	return false;
#endif
}

void sec_bat_update_psb_level(struct sec_battery_info *battery)
{
	sec_psb *psb = battery->psb;
	union power_supply_propval value = {0, };


	if (battery->status == POWER_SUPPLY_STATUS_FULL &&
		battery->capacity >= 100) {

		value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
		
		if (psb->raw_soc < value.intval) {
			psb->raw_soc = value.intval;
			pr_info("%s: [%s] update raw_soc(%d)\n",
				__func__, sec_bat_status_str[battery->status], psb->raw_soc);
		} else if (psb->raw_soc > value.intval) {
			sec_psb_set_data(psb, PSB_TYPE_LEVEL, CMD_ADD, (psb->raw_soc - value.intval));

#if defined(CONFIG_BATTERY_AGE_FORECAST)
			//battery->batt_cycle += adj_cycle;
#endif
			pr_info("%s: [%s] bc(%d), rs(%d), nrs(%d), pl(%d)\n",
				__func__, sec_bat_status_str[battery->status],
				battery->batt_cycle, psb->raw_soc, value.intval, psb->data[PSB_TYPE_LEVEL]);

			psb->raw_soc = value.intval;
		}
	} else
		psb->raw_soc = 0;
}

void sec_bat_start_psb(struct sec_battery_info *battery)
{
	sec_psb *psb = battery->psb;
	struct timespec ts = {0, };
	int batt_cycle = 0;

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	batt_cycle = battery->batt_cycle;
#endif
	if (batt_cycle > psb->pdata->max_cycle)
		return;
	else if ((battery->status == POWER_SUPPLY_STATUS_FULL) &&
			(psb->state == PSB_STATE_CHARGE_DONE)) {
		psb->start_time_after_discharging = 0;
		return;
	}
	psb->state = PSB_STATE_CHARGE_DONE;
	psb->start_time_after_discharging = 0;
	get_monotonic_boottime(&ts);
	psb->start_time_after_full = ts.tv_sec;
	pr_info("%s: [2ND-FULL] staf(%ld)\n", __func__, psb->start_time_after_full);
}

static void sec_psb_check_float_voltage(struct sec_battery_info *battery)
{
	bool is_swelling_mode = false;

#if defined(CONFIG_BATTERY_SWELLING)
	is_swelling_mode = !!battery->swelling_mode;
#endif

	if (!is_swelling_mode) {
		union power_supply_propval value = {0, };
		bool is_charging = !battery->charging_block;

		/* check float voltage */
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, value);
		if (is_charging)
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
		if (value.intval != battery->pdata->chg_float_voltage) {
			value.intval = battery->pdata->chg_float_voltage;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, value);
		}
		if (is_charging)
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
	}
}

void sec_bat_check_psb(struct sec_battery_info *battery)
{
	sec_psb *psb = battery->psb;
	struct timespec ts = {0, };
	unsigned long diff_time;
	bool is_age_step_changed;
	int batt_cycle = 0;

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	batt_cycle = battery->batt_cycle;
#endif
	if (batt_cycle > psb->pdata->max_cycle)
		return;
	else if (psb->state == PSB_STATE_NONE)
		return;

	switch (battery->status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		/*
		 * case1. full --> charging
		*/
		if (psb->state == PSB_STATE_CHARGE_DONE) {
			/* update values about full state */
			psb->state = PSB_STATE_CHARGING;
			psb->prevent_adjust_cycle += psb->adjust_cycle;
			psb->adjust_cycle = 0;
			psb->start_time_after_full = 0;
			psb->start_time_after_discharging = 0;
			pr_info("%s: [CHARGING] pac(%d), staf(%ld), stad(%ld)\n",
				__func__, psb->prevent_adjust_cycle,
				psb->start_time_after_full,
				psb->start_time_after_discharging);
		}
		break;
	case POWER_SUPPLY_STATUS_FULL:
		/*
		 * case1. full
		*/
		if (psb->state == PSB_STATE_CHARGE_DONE) {
			get_monotonic_boottime(&ts);
			/* calculate idle time after charge done */
			diff_time = (ts.tv_sec >= psb->start_time_after_full) ?
				(ts.tv_sec - psb->start_time_after_full) :
				(0xFFFFFFFF - psb->start_time_after_full + ts.tv_sec);
			psb->adjust_cycle =
				(((diff_time * 100) * psb->pdata->max_cycle) /
				psb->pdata->trigger_time_after_full) / 100;
			pr_info("%s: [FULL] ac(%d), pac(%d), staf(%ld), dt(%ld)\n",
				__func__,
				psb->adjust_cycle, psb->prevent_adjust_cycle,
				psb->start_time_after_full, diff_time);

			is_age_step_changed = sec_psb_update_age_data(battery,
				psb->adjust_cycle + psb->prevent_adjust_cycle);
			pr_info("%s: [FULL] icas(%d), vn(%d), rcv(%d)\n",
				__func__, is_age_step_changed,
				battery->voltage_now, battery->pdata->recharge_condition_vcell);

			if (is_age_step_changed) {
				battery->is_recharging = false;
				battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_BUCK_OFF);
			} else if (battery->is_recharging) {
				sec_psb_check_float_voltage(battery);
			}
		}
		return;
	default: /* DISCHARGING or NOT-CHARGING */
		/*
		 * case1. full --> discharging
		 * case2. full --> discharging --> charging --> discharging
		 * case3. full --> charging --> discharging
		 * case4. full --> charging --> full --> discharging
		 * ...
		*/
		get_monotonic_boottime(&ts);

		if (psb->state != PSB_STATE_DISCHARGING) {
			/* update values about full state */
			psb->state = PSB_STATE_DISCHARGING;
			psb->prevent_adjust_cycle += psb->adjust_cycle;
			psb->adjust_cycle = 0;
			psb->start_time_after_full = 0;
			psb->start_time_after_discharging = ts.tv_sec;
			pr_info("%s: [DISCHARGING] pac(%d), staf(%ld), stad(%ld)\n",
				__func__, psb->prevent_adjust_cycle,
				psb->start_time_after_full,
				psb->start_time_after_discharging);
		} else {
			/* calculate discharging time */
			diff_time = (ts.tv_sec >= psb->start_time_after_discharging) ?
				(ts.tv_sec - psb->start_time_after_discharging) :
				(0xFFFFFFFF - psb->start_time_after_discharging + ts.tv_sec);
			pr_info("%s: [DISCHARGING] dt(%ld)\n", __func__, diff_time);

			/* check release condition (discharging time) */
			if (psb->pdata->release_discharging_time < diff_time) {
				/* release age step */
				psb->state = PSB_STATE_NONE;
				psb->start_time_after_full = 0;
				psb->start_time_after_discharging = 0;
				psb->adjust_cycle = 0;
				psb->prevent_adjust_cycle = 0;
				//psb->adjust_age_step = 0;
				sec_psb_update_age_data(battery, batt_cycle);
			}
		}
		break;
	}

	/* check release condition (voltage or soc) */
	if ((battery->voltage_now < psb->pdata->release_discharging_voltage) ||
			(battery->capacity < psb->pdata->release_discharging_soc)) {
		/* release age step */
		psb->state = PSB_STATE_NONE;
		psb->start_time_after_full = 0;
		psb->start_time_after_discharging = 0;
		psb->adjust_cycle = 0;
		psb->prevent_adjust_cycle = 0;
		//psb->adjust_age_step = 0;
		sec_psb_update_age_data(battery, batt_cycle);
		pr_info("%s: [%s] release state(voltage:%d, soc:%d)\n",
			__func__, sec_bat_status_str[battery->status], battery->voltage_now, battery->capacity);

		/* update float voltage */
		sec_psb_check_float_voltage(battery);
	}

	sec_psb_set_data(psb, PSB_TYPE_MAX_CYCLE,
		CMD_MAX, psb->adjust_cycle + psb->prevent_adjust_cycle);
}

void sec_bat_check_full_state(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };
	sec_psb *psb = battery->psb;

	/* check float voltage & status */
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	pr_info("%s: check full-state(%d, %d, %d, %d, %d)\n", __func__,
		psb->state, psb->adjust_cycle, psb->prevent_adjust_cycle,
		battery->capacity, value.intval);
	if ((battery->status != POWER_SUPPLY_STATUS_FULL) &&
			(battery->capacity >= 100) &&
			(value.intval >= battery->pdata->chg_float_voltage)) {
		sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_FULL);
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		battery->is_recharging = false;
		sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_BUCK_OFF);

		/* start psb */
		sec_bat_start_psb(battery);
	} else {
		sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
	}
}

static void sec_psb_check_charging_time(struct sec_battery_info *battery)
{
	sec_psb *psb = battery->psb;
	struct timespec ts = {0, };
	unsigned int total_charging_time = 0;

	if (!is_nocharge_type(battery->cable_type) && !psb->start_charging_time) {
		get_monotonic_boottime(&ts);
		psb->start_charging_time = ts.tv_sec;
		psb->prev_charging_time = psb->data[PSB_TYPE_TEMP_CHARGING_TIME];
	} else if (psb->start_charging_time) {
		get_monotonic_boottime(&ts);
		total_charging_time = ts.tv_sec - psb->start_charging_time;
		sec_psb_set_data(psb, PSB_TYPE_MAX_CHARGING_TIME,
			CMD_MAX, total_charging_time + psb->prev_charging_time);
		if (psb->is_time_set)
			psb->data[PSB_TYPE_TEMP_CHARGING_TIME] =
				total_charging_time + psb->prev_charging_time;

		if (is_nocharge_type(battery->cable_type)) {
			if (psb->is_time_set)
				psb->data[PSB_TYPE_TEMP_CHARGING_TIME] = 0;
			psb->prev_charging_time = 0;
			psb->start_charging_time = 0;
		}
	}
}

static ssize_t sec_psb_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);
	const ptrdiff_t offset = attr - sec_psb_attrs;
	sec_psb *psb = battery->psb;
	char temp_buf[1024] = {0,};
	int i = 0, j = 0, size = 1024;

	switch (offset) {
	case PSB_DATA:
		/* update charging time */
		sec_psb_check_charging_time(battery);
		
		for (j = 0; j < PSB_TYPE_STEP_COUNT + psb->pdata->num_age_step; j++) {
			snprintf(temp_buf + strlen(temp_buf), size, "%d ", psb->data[j]);
			size = sizeof(temp_buf) - strlen(temp_buf);
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);		
		break;
#if defined(CONFIG_BATTERY_CISD)
	case PSB_DATA_JSON:
		for (j = 0; j < PSB_TYPE_STEP_COUNT + psb->pdata->num_age_step; j++) {
			if (j >= PSB_TYPE_STEP_COUNT)
				sprintf(temp_buf+strlen(temp_buf), ",\"%s_%d\":\"%d\"",
					psb_data_json_str[PSB_TYPE_STEP_COUNT], j - PSB_TYPE_STEP_COUNT, psb->data[j]);
			else if (psb_data_json_str[j] != NULL)
				sprintf(temp_buf+strlen(temp_buf), "\"%s\":\"%d\"",
					psb_data_json_str[j], psb->data[j]);
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		break;
#endif
	case PSB_CONDITION:
		i += scnprintf(buf + i, PAGE_SIZE - i, "tt:%d rt:%d rs:%d rv:%d, ac:%d, pc:%d\n",
			psb->pdata->trigger_time_after_full, psb->pdata->release_discharging_time,
			psb->pdata->release_discharging_soc, psb->pdata->release_discharging_voltage,
			psb->adjust_cycle, psb->prevent_adjust_cycle);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

static ssize_t sec_psb_store_attrs(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);
	const ptrdiff_t offset = attr - sec_psb_attrs;
	sec_psb *psb = battery->psb;
	int ret = -EINVAL, x = 0;

	switch (offset) {
	case PSB_DATA:
	{
		const char *p = buf;
		int i = 0;
		pr_info("%s: %s\n", __func__, buf);

		for (i = 0; i < PSB_TYPE_STEP_COUNT + psb->pdata->num_age_step; i++) {
			if (sscanf(p, "%10d%n", &psb->data[i], &x) > 0) {
				p += (size_t)x;

				/* update step condition */
				if (i >= PSB_TYPE_STEP_COUNT)
					sec_psb_check_step_count(battery, i - PSB_TYPE_STEP_COUNT);
			} else {
				pr_info("%s: NO DATA (IDX:%d, PSB_STEP_COUNT)\n", __func__, i);
				psb->data[i] = 0;
				break;
			}
		}

		/* check clear cmd */
		if (psb->data[PSB_TYPE_NONE] == CMD_CLEAR) {
			for (i = 0; i < PSB_TYPE_STEP_COUNT + psb->pdata->num_age_step; i++)
				psb->data[i] = 0;
		} else {
			if (is_nocharge_type(battery->cable_type)) {
				sec_psb_set_data(psb, PSB_TYPE_MAX_CHARGING_TIME, CMD_MAX, psb->data[PSB_TYPE_TEMP_CHARGING_TIME]);
				psb->data[PSB_TYPE_TEMP_CHARGING_TIME] = 0;
			} else {
				psb->prev_charging_time = psb->data[PSB_TYPE_TEMP_CHARGING_TIME];
			}
			psb->is_time_set = true;
		}
		ret = count;
	}
		break;
#if defined(CONFIG_BATTERY_CISD)
	case PSB_DATA_JSON:
		ret = count;
		break;
#endif
	case PSB_CONDITION:
	{
		char tc;
	
		if (sscanf(buf, "%c %d\n", &tc, &x) == 2) {
			unsigned int old_value = 0;

			switch (tc) {
			case 't':
				old_value = psb->pdata->trigger_time_after_full;
				psb->pdata->trigger_time_after_full = x;
				break;
			case 'r':
				old_value = psb->pdata->release_discharging_time;
				psb->pdata->release_discharging_time = x;
				break;
			case 's':
				old_value = psb->pdata->release_discharging_soc;
				psb->pdata->release_discharging_soc = x;
				break;
			case 'v':
				old_value = psb->pdata->release_discharging_voltage;
				psb->pdata->release_discharging_voltage = x;
				break;
			case 'c':
				old_value = psb->prevent_adjust_cycle;
				if (psb->state != PSB_STATE_NONE) {
					psb->state = PSB_STATE_NONE;
					psb->start_time_after_full = 0;
					psb->start_time_after_discharging = 0;
					psb->adjust_cycle = 0;
					psb->prevent_adjust_cycle = 0;
					//psb->adjust_age_step = 0;
					sec_psb_update_age_data(battery, battery->batt_cycle);
					pr_info("%s: init psb parameters.\n", __func__);
				}
				psb->prevent_adjust_cycle = x;
				break;
			default:
				break;
			}
			pr_info("%s: changed condition(%c: %d --> %d)\n",
				__func__, tc, old_value, x);
			ret = count;
		}
	}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void sec_bat_init_psb(struct sec_battery_info *battery)
{
	struct device_node *np = NULL;
	sec_psb *psb = NULL;
	sec_psb_pdata *pdata = NULL;
	const u32 *p = NULL;
	int ret = 0, len = 0, i;
	char temp_buf[1024] = {0,};

	/* 0. create & alloc psb */
	psb = kzalloc(sizeof(sec_psb), GFP_KERNEL);
	if (!psb) {
		pr_err("%s: failed to alloc sec_psd\n", __func__);
		return;
	}
	pdata = kzalloc(sizeof(sec_psb_pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: failed to alloc pdata\n", __func__);
		goto err_alloc_pdata;
	}
	psb->pdata = pdata;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	pdata->num_age_step = battery->pdata->num_age_step;
#endif
	if (pdata->num_age_step > 0) {
		pdata->step_count_condition =
			kzalloc(sizeof(unsigned int) * pdata->num_age_step, GFP_KERNEL);
		if (!pdata->step_count_condition) {
			pr_err("%s: failed to alloc step array\n", __func__);
			goto err_alloc_step_condition;
		}
	}

	len = (PSB_TYPE_STEP_COUNT + pdata->num_age_step) * sizeof(unsigned int);
	psb->data = kzalloc(len, GFP_KERNEL);
	if (!psb->data) {
		pr_err("%s: failed to alloc data\n", __func__);
		goto err_alloc_data;
	}

	/* 1. update condition values from the dt data. */
	np = of_find_node_by_name(NULL, "prevent_swelling_battery");
	if (!np) {
		pr_err("%s: failed to find battery node\n", __func__);
		goto err_find_node;
	}
	ret = of_property_read_u32(np, "psb,trigger_time_after_full",
		&pdata->trigger_time_after_full);
	if (ret)
		pdata->trigger_time_after_full = DEFAULT_TRIGGER_TIME_AFTER_FULL;

	ret = of_property_read_u32(np, "psb,release_discharging_time",
		&pdata->release_discharging_time);
	if (ret)
		pdata->release_discharging_time = DEFAULT_RELEASE_DISCHARGING_TIME;

	ret = of_property_read_u32(np, "psb,release_discharging_soc",
		&pdata->release_discharging_soc);
	if (ret) {
#if defined(CONFIG_BATTERY_AGE_FORECAST)
		pdata->release_discharging_soc = 100 -
			battery->pdata->age_data[pdata->num_age_step - 1].full_condition_soc;
#else
		pdata->release_discharging_soc = battery->pdata->full_condition_soc;
#endif
	}

	ret = of_property_read_u32(np, "psb,release_discharging_voltage",
		&pdata->release_discharging_voltage);
	if (ret) {
#if defined(CONFIG_BATTERY_AGE_FORECAST)
		pdata->release_discharging_voltage = 
			battery->pdata->age_data[pdata->num_age_step - 1].full_condition_vcell;
#else
		pdata->release_discharging_voltage = battery->pdata->full_condition_soc;
#endif
	}

	if (pdata->num_age_step > 0) {
		p = of_get_property(np, "psb,step_count_condition", &len);
		if (p) {
			/* check array size */
			len = len / sizeof(unsigned int);
			if (len > pdata->num_age_step)
				len = pdata->num_age_step;
		
			ret = of_property_read_u32_array(np, "psb,step_count_condition",
					 (u32 *)pdata->step_count_condition, len);
			if (ret) {
				pr_err("%s: failed to read step_count_condition(ret:%d - len:%d)\n",
					__func__, ret, len);
		
				for (i = 0; i < len; i++)
					pdata->step_count_condition[i] = 0;
			}
		}
		for (i = 0; i < pdata->num_age_step; i++)
			sprintf(temp_buf+strlen(temp_buf), "%d ", pdata->step_count_condition[i]);
	} else
		sprintf(temp_buf, "NULL");

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	pdata->max_cycle = battery->pdata->age_data[battery->pdata->num_age_step - 1].cycle;
#else
	pdata->max_cycle = DEFAULT_MAX_CYCLE;
#endif
	pr_err("%s: read dt(tt:%d, rt:%d, rs:%d, rv:%d, mc:%d, scc:%s)\n", __func__,
		pdata->trigger_time_after_full,
		pdata->release_discharging_time,
		pdata->release_discharging_soc,
		pdata->release_discharging_voltage,
		pdata->max_cycle,
		temp_buf);

	/* 2. create sysfs nodes. */
	for (i = 0; i < ARRAY_SIZE(sec_psb_attrs); i++) {
		ret = device_create_file(battery->psy_bat.dev, &sec_psb_attrs[i]);
		if (ret)
			goto err_create_attr;
	}

	/* 3. init state values. */
	psb->raw_soc = 0;

	psb->state = PSB_STATE_NONE;
	psb->start_time_after_full = 0;
	psb->start_time_after_discharging = 0;
	psb->adjust_cycle = 0;
	psb->prevent_adjust_cycle = 0;
	psb->adjust_age_step = 0;

	battery->psb = psb;
	return;

err_create_attr:
	while (i--)
		device_remove_file(battery->dev, &sec_psb_attrs[i]);
err_find_node:
	if (psb->data)
		kfree(psb->data);
err_alloc_data:
	if (pdata->step_count_condition)
		kfree(pdata->step_count_condition);
err_alloc_step_condition:
	kfree(psb->pdata);
err_alloc_pdata:
	kfree(psb);
}

void sec_bat_remove_psb(struct sec_battery_info *battery)
{
	sec_psb *psb = battery->psb;

	if (!psb)
		return;

	if (psb->pdata) {
		sec_psb_pdata *pdata = psb->pdata;

		if (pdata->step_count_condition)
			kfree(pdata->step_count_condition);
		kfree(psb->pdata);
	}
	if (psb->data)
		kfree(psb->data);
	kfree(psb);
}

