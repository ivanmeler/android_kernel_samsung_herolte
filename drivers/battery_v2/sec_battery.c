/*
 *  sec_battery.c
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
#include <linux/sec_ext.h>
#include <linux/sec_debug.h>

bool sleep_mode = false;

static struct device_attribute sec_battery_attrs[] = {
	SEC_BATTERY_ATTR(batt_reset_soc),
	SEC_BATTERY_ATTR(batt_read_raw_soc),
	SEC_BATTERY_ATTR(batt_read_adj_soc),
	SEC_BATTERY_ATTR(batt_type),
	SEC_BATTERY_ATTR(batt_vfocv),
	SEC_BATTERY_ATTR(batt_vol_adc),
	SEC_BATTERY_ATTR(batt_vol_adc_cal),
	SEC_BATTERY_ATTR(batt_vol_aver),
	SEC_BATTERY_ATTR(batt_vol_adc_aver),
	SEC_BATTERY_ATTR(batt_current_ua_now),
	SEC_BATTERY_ATTR(batt_current_ua_avg),
	SEC_BATTERY_ATTR(batt_filter_cfg),
	SEC_BATTERY_ATTR(batt_temp),
	SEC_BATTERY_ATTR(batt_temp_adc),
	SEC_BATTERY_ATTR(batt_temp_aver),
	SEC_BATTERY_ATTR(batt_temp_adc_aver),
	SEC_BATTERY_ATTR(usb_temp),
	SEC_BATTERY_ATTR(usb_temp_adc),	
	SEC_BATTERY_ATTR(chg_temp),
	SEC_BATTERY_ATTR(chg_temp_adc),
	SEC_BATTERY_ATTR(slave_chg_temp),
	SEC_BATTERY_ATTR(slave_chg_temp_adc),

	SEC_BATTERY_ATTR(batt_vf_adc),
	SEC_BATTERY_ATTR(batt_slate_mode),

	SEC_BATTERY_ATTR(batt_lp_charging),
	SEC_BATTERY_ATTR(siop_activated),
	SEC_BATTERY_ATTR(siop_level),
	SEC_BATTERY_ATTR(siop_event),
	SEC_BATTERY_ATTR(batt_charging_source),
	SEC_BATTERY_ATTR(fg_reg_dump),
	SEC_BATTERY_ATTR(fg_reset_cap),
	SEC_BATTERY_ATTR(fg_capacity),
	SEC_BATTERY_ATTR(fg_asoc),
	SEC_BATTERY_ATTR(auth),
	SEC_BATTERY_ATTR(chg_current_adc),
	SEC_BATTERY_ATTR(wc_adc),
	SEC_BATTERY_ATTR(wc_status),
	SEC_BATTERY_ATTR(wc_enable),
	SEC_BATTERY_ATTR(wc_control),
	SEC_BATTERY_ATTR(wc_control_cnt),
	SEC_BATTERY_ATTR(hv_charger_status),
	SEC_BATTERY_ATTR(hv_wc_charger_status),
	SEC_BATTERY_ATTR(hv_charger_set),
	SEC_BATTERY_ATTR(factory_mode),
	SEC_BATTERY_ATTR(store_mode),
	SEC_BATTERY_ATTR(update),
	SEC_BATTERY_ATTR(test_mode),

	SEC_BATTERY_ATTR(call),
	SEC_BATTERY_ATTR(2g_call),
	SEC_BATTERY_ATTR(talk_gsm),
	SEC_BATTERY_ATTR(3g_call),
	SEC_BATTERY_ATTR(talk_wcdma),
	SEC_BATTERY_ATTR(music),
	SEC_BATTERY_ATTR(video),
	SEC_BATTERY_ATTR(browser),
	SEC_BATTERY_ATTR(hotspot),
	SEC_BATTERY_ATTR(camera),
	SEC_BATTERY_ATTR(camcorder),
	SEC_BATTERY_ATTR(data_call),
	SEC_BATTERY_ATTR(wifi),
	SEC_BATTERY_ATTR(wibro),
	SEC_BATTERY_ATTR(lte),
	SEC_BATTERY_ATTR(lcd),
	SEC_BATTERY_ATTR(gps),
	SEC_BATTERY_ATTR(event),
	SEC_BATTERY_ATTR(batt_temp_table),
	SEC_BATTERY_ATTR(batt_high_current_usb),
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	SEC_BATTERY_ATTR(test_charge_current),
#endif
	SEC_BATTERY_ATTR(set_stability_test),
	SEC_BATTERY_ATTR(batt_capacity_max),
	SEC_BATTERY_ATTR(batt_inbat_voltage),
	SEC_BATTERY_ATTR(batt_inbat_voltage_ocv),
	SEC_BATTERY_ATTR(check_slave_chg),
	SEC_BATTERY_ATTR(batt_inbat_wireless_cs100),
	SEC_BATTERY_ATTR(hmt_ta_connected),
	SEC_BATTERY_ATTR(hmt_ta_charge),
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	SEC_BATTERY_ATTR(fg_cycle),
	SEC_BATTERY_ATTR(fg_full_voltage),
	SEC_BATTERY_ATTR(fg_fullcapnom),
	SEC_BATTERY_ATTR(battery_cycle),
#if defined(CONFIG_BATTERY_AGE_FORECAST_DETACHABLE)
	SEC_BATTERY_ATTR(batt_after_manufactured),
#endif
#endif
	SEC_BATTERY_ATTR(batt_wpc_temp),
	SEC_BATTERY_ATTR(batt_wpc_temp_adc),
	SEC_BATTERY_ATTR(batt_coil_temp), /* Wireless Coil therm */
	SEC_BATTERY_ATTR(batt_coil_temp_adc), /* Wireless Coil therm */
	SEC_BATTERY_ATTR(mst_switch_test), /* MFC MST switch test */
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	SEC_BATTERY_ATTR(batt_wireless_firmware_update),
	SEC_BATTERY_ATTR(otp_firmware_result),
	SEC_BATTERY_ATTR(wc_ic_grade),
	SEC_BATTERY_ATTR(otp_firmware_ver_bin),
	SEC_BATTERY_ATTR(otp_firmware_ver),
	SEC_BATTERY_ATTR(tx_firmware_result),
	SEC_BATTERY_ATTR(tx_firmware_ver),
	SEC_BATTERY_ATTR(batt_tx_status),
#endif
	SEC_BATTERY_ATTR(wc_vout),
	SEC_BATTERY_ATTR(wc_vrect),
	SEC_BATTERY_ATTR(batt_hv_wireless_status),
	SEC_BATTERY_ATTR(batt_hv_wireless_pad_ctrl),
	SEC_BATTERY_ATTR(wc_op_freq),
	SEC_BATTERY_ATTR(wc_cmd_info),
	SEC_BATTERY_ATTR(batt_tune_float_voltage),
	SEC_BATTERY_ATTR(batt_tune_input_charge_current),
	SEC_BATTERY_ATTR(batt_tune_fast_charge_current),
	SEC_BATTERY_ATTR(batt_tune_ui_term_cur_1st),
	SEC_BATTERY_ATTR(batt_tune_ui_term_cur_2nd),
	SEC_BATTERY_ATTR(batt_tune_temp_high_normal),
	SEC_BATTERY_ATTR(batt_tune_temp_high_rec_normal),
	SEC_BATTERY_ATTR(batt_tune_temp_low_normal),
	SEC_BATTERY_ATTR(batt_tune_temp_low_rec_normal),
	SEC_BATTERY_ATTR(batt_tune_chg_temp_high),
	SEC_BATTERY_ATTR(batt_tune_chg_temp_rec),
	SEC_BATTERY_ATTR(batt_tune_chg_limit_cur),
	SEC_BATTERY_ATTR(batt_tune_coil_temp_high),
	SEC_BATTERY_ATTR(batt_tune_coil_temp_rec),
	SEC_BATTERY_ATTR(batt_tune_coil_limit_cur),
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	SEC_BATTERY_ATTR(batt_update_data),
#endif
	SEC_BATTERY_ATTR(batt_misc_event),
	SEC_BATTERY_ATTR(batt_ext_dev_chg),
	SEC_BATTERY_ATTR(batt_wdt_control),
	SEC_BATTERY_ATTR(mode),
	SEC_BATTERY_ATTR(check_ps_ready),
	SEC_BATTERY_ATTR(batt_chip_id),
	SEC_BATTERY_ATTR(cisd_fullcaprep_max),
#if defined(CONFIG_BATTERY_CISD)
	SEC_BATTERY_ATTR(cisd_data),
	SEC_BATTERY_ATTR(cisd_data_json),
	SEC_BATTERY_ATTR(cisd_wire_count),
#endif	
#if defined(CONFIG_BATTERY_SBM_DATA)
	SEC_BATTERY_ATTR(sbm_data),
#endif
	SEC_BATTERY_ATTR(safety_timer_set),
	SEC_BATTERY_ATTR(batt_swelling_control),
	SEC_BATTERY_ATTR(safety_timer_info),
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	SEC_BATTERY_ATTR(batt_temp_test),
#endif
	SEC_BATTERY_ATTR(batt_current_event),
};

static enum power_supply_property sec_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
#if defined(CONFIG_CALC_TIME_TO_FULL)
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
#endif
	POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL,
};

static enum power_supply_property sec_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property sec_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property sec_ps_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

char *sec_cable_type[SEC_BATTERY_CABLE_MAX] = {
	"UNKNOWN",                 /* 0 */
	"NONE",                    /* 1 */
	"PREAPARE_TA",             /* 2 */
	"TA",                      /* 3 */
	"USB",                     /* 4 */
	"USB_CDP",                 /* 5 */
	"9V_TA",                   /* 6 */
	"9V_ERR",                  /* 7 */ 
	"9V_UNKNOWN",              /* 8 */
	"12V_TA",                  /* 9 */
	"WIRELESS",                /* 10 */
	"HV_WIRELESS",             /* 11 */
	"PMA_WIRELESS",            /* 12 */
	"WIRELESS_PACK",           /* 13 */
	"WIRELESS_PACK_TA",        /* 14 */
	"WIRELESS_STAND",          /* 15 */
	"WIRELESS_HV_STAND",       /* 16 */
	"OC20",                    /* 17 */
	"QC30",                    /* 18 */
	"PDIC",                    /* 19 */
	"UARTOFF",                 /* 20 */
	"OTG",                     /* 21 */
	"LAN_HUB",                 /* 22 */
	"POWER_SHARGING",          /* 23 */
	"HMT_CONNECTED",           /* 24 */
	"HMT_CHARGE",              /* 25 */
	"HV_TA_CHG_LIMIT",          /* 26 */
	"WIRELESS_VEHICLE",          /* 27 */
	"WIRELESS_HV_VEHICLE",	   /* 28 */
	"WIRELESS_HV_PREPARE",	   /* 29 */
	"TIMEOUT",                 /* 30 */
	"SMART_OTG",               /* 31 */
	"SMART_NOTG"               /* 32 */
};

char *sec_bat_charging_mode_str[] = {
	"None",
	"Normal",
	"Additional",
	"Re-Charging",
	"ABS"
};

char *sec_bat_status_str[] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not-charging",
	"Full"
};

char *sec_bat_health_str[] = {
	"Unknown",
	"Good",
	"Overheat",
	"Warm",
	"Dead",
	"OverVoltage",
	"UnspecFailure",
	"Cold",
	"Cool",
	"WatchdogTimerExpire",
	"SafetyTimerExpire",
	"UnderVoltage",
	"OverheatLimit",
	"VsysOVP",
	"VbatOVP",
};

static void sec_bat_set_misc_event(struct sec_battery_info *battery,
	const int misc_event_type, bool do_clear) {

	mutex_lock(&battery->misclock);
	pr_info("%s: %s misc event(now=0x%x, value=0x%x)\n",
		__func__, ((do_clear) ? "clear" : "set"), battery->misc_event, misc_event_type);
	if (do_clear) {
		battery->misc_event &= ~misc_event_type;
	} else {
		battery->misc_event |= misc_event_type;
	}
	mutex_unlock(&battery->misclock);

	if (battery->prev_misc_event != battery->misc_event) {
		cancel_delayed_work(&battery->misc_event_work);
		wake_lock(&battery->misc_event_wake_lock);
		queue_delayed_work(battery->monitor_wqueue,
			&battery->misc_event_work, 0);
	}
}

static void sec_bat_set_current_event(struct sec_battery_info *battery,
				      unsigned int current_event_val, unsigned int current_event_mask)
{
	unsigned int temp = battery->current_event;

	mutex_lock(&battery->current_eventlock);

	battery->current_event &= ~current_event_mask;
	battery->current_event |= current_event_val;

	pr_info("%s: current event before(0x%x), after(0x%x)\n",
		__func__, temp, battery->current_event);

	mutex_unlock(&battery->current_eventlock);
}

static void sec_bat_change_default_current(struct sec_battery_info *battery,
					int cable_type, int input, int output)
{
	battery->pdata->charging_current[cable_type].input_current_limit = input;
	battery->pdata->charging_current[cable_type].fast_charging_current = output;
	pr_info("%s: cable_type: %d input: %d output: %d\n",__func__, cable_type, input, output);
}

static int sec_bat_get_wireless_current(struct sec_battery_info *battery, int incurr)
{

	/* 1. SIOP EVENT */
	if (battery->siop_event & SIOP_EVENT_WPC_CALL &&
			(battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK_TA)) {
		if (battery->capacity >= battery->pdata->wireless_cc_cv) {
			if (incurr > battery->pdata->siop_call_cv_current)
				incurr = battery->pdata->siop_call_cv_current;
		} else {
			if (incurr > battery->pdata->siop_call_cc_current)
				incurr = battery->pdata->siop_call_cc_current;
		}
	}

	/* 2. WPC_SLEEP_MODE */
	if (is_hv_wireless_type(battery->cable_type) && sleep_mode) {
		if(incurr > battery->pdata->sleep_mode_limit_current)
			incurr = battery->pdata->sleep_mode_limit_current;
		pr_info("%s sleep_mode =%d, chg_limit =%d, in_curr = %d \n", __func__,
			sleep_mode, battery->chg_limit, incurr);
	}

	/* 3. WPC_TEMP_MODE */
	if (is_wireless_type(battery->cable_type) && battery->chg_limit) {
		if ((battery->siop_level >= 100) &&
				(incurr > battery->pdata->wpc_charging_limit_current))
			incurr = battery->pdata->wpc_charging_limit_current;
		else if ((battery->siop_level < 100) &&
				(incurr > battery->pdata->wpc_lcd_on_charging_limit_current))
			incurr = battery->pdata->wpc_lcd_on_charging_limit_current;
	}

	/* 4. WPC_CV_MODE */
	if (battery->wc_cv_mode) {
		if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS ||
		    battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND ||
		    battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK_TA) {
			if (incurr > battery->pdata->wc_cv_current)
				incurr = battery->pdata->wc_cv_current;
		} else if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK) {
			if (incurr > battery->pdata->wc_cv_pack_current)
				incurr = battery->pdata->wc_cv_pack_current;
		}
	}

	/* 5. Full-Additional state */
	if (battery->status == POWER_SUPPLY_STATUS_FULL && battery->charging_mode == SEC_BATTERY_CHARGING_2ND) {
		if (incurr > battery->pdata->siop_hv_wireless_input_limit_current)
			incurr = battery->pdata->siop_hv_wireless_input_limit_current;
	}

	/* 6. Full-None state && SIOP_LEVEL 100 */
	if (battery->siop_level == 100 &&
		battery->status == POWER_SUPPLY_STATUS_FULL && battery->charging_mode == SEC_BATTERY_CHARGING_NONE) {
		incurr = battery->pdata->wc_full_input_limit_current;
	}

	return incurr;
}

static void sec_bat_get_charging_current_by_siop(struct sec_battery_info *battery,
		int *input_current, int *charging_current) {
	int usb_charging_current = 500;

	if (battery->siop_level == 3) {
		/* side sync scenario : siop_level 3 */
		if (is_nv_wireless_type(battery->cable_type)) {
			if (*input_current > battery->pdata->siop_wireless_input_limit_current)
				*input_current = battery->pdata->siop_wireless_input_limit_current;
			*charging_current = battery->pdata->siop_wireless_charging_limit_current;
		} else if (is_hv_wireless_type(battery->cable_type)) {
			if (*input_current > battery->pdata->siop_hv_wireless_input_limit_current)
				*input_current = battery->pdata->siop_hv_wireless_input_limit_current;
			*charging_current = battery->pdata->siop_hv_wireless_charging_limit_current;
		} else if (is_hv_wire_type(battery->cable_type)) {
			if (*input_current > 450)
				*input_current = 450;
			*charging_current = battery->pdata->siop_hv_charging_limit_current;
#if defined(CONFIG_CCIC_NOTIFIER)
		} else if (battery->cable_type == SEC_BATTERY_CABLE_PDIC) {
			if (*input_current > (4000 / battery->input_voltage))
				*input_current = 4000 / battery->input_voltage;
			*charging_current = battery->pdata->siop_hv_charging_limit_current;
#endif
		} else {
			if (*input_current > 800)
				*input_current = 800;
			*charging_current = battery->pdata->charging_current[
				battery->cable_type].fast_charging_current;
			if (*charging_current > battery->pdata->siop_charging_limit_current)
				*charging_current = battery->pdata->siop_charging_limit_current;
		}
	} else if (battery->siop_level == 5) {
		/* special senario : calling or browsing during wired charging */
		if (is_hv_wire_type(battery->cable_type) &&
		    !(battery->current_event & SEC_BAT_CURRENT_EVENT_CHG_LIMIT)) {
			if (battery->cable_type == SEC_BATTERY_CABLE_12V_TA)
				*input_current = 440;
			else
				*input_current = 600;
			*charging_current = 900;
		} else if (battery->cable_type == SEC_BATTERY_CABLE_TA ||
			   (battery->current_event & SEC_BAT_CURRENT_EVENT_CHG_LIMIT)) {
			*input_current = 1000;
			*charging_current = 900;
		}
	} else if (battery->siop_level < 100) {
		/* decrease the charging current according to siop level */
		*charging_current = *charging_current * battery->siop_level / 100;

		/* do forced set charging current */
		if (*charging_current > 0 && *charging_current < usb_charging_current)
			*charging_current = usb_charging_current;

		if (is_nv_wireless_type(battery->cable_type)) {
			if(*input_current > battery->pdata->siop_wireless_input_limit_current)
				*input_current = battery->pdata->siop_wireless_input_limit_current;
			if (*charging_current > battery->pdata->siop_wireless_charging_limit_current)
				*charging_current = battery->pdata->siop_wireless_charging_limit_current;
		} else if (is_hv_wireless_type(battery->cable_type)) {
			if(*input_current > battery->pdata->siop_hv_wireless_input_limit_current)
				*input_current = battery->pdata->siop_hv_wireless_input_limit_current;
			if (*charging_current > battery->pdata->siop_hv_wireless_charging_limit_current)
				*charging_current = battery->pdata->siop_hv_wireless_charging_limit_current;
		} else if (is_hv_wire_type(battery->cable_type) && !(battery->current_event & SEC_BAT_CURRENT_EVENT_CHG_LIMIT)) {
			if (is_hv_wire_12v_type(battery->cable_type)) {
				if (*input_current > battery->pdata->siop_hv_12v_input_limit_current)
					*input_current = battery->pdata->siop_hv_12v_input_limit_current;
				if (*charging_current > battery->pdata->siop_hv_12v_charging_limit_current)
					*charging_current = battery->pdata->siop_hv_12v_charging_limit_current;
			} else {
				if (*input_current > battery->pdata->siop_hv_input_limit_current)
					*input_current = battery->pdata->siop_hv_input_limit_current;
				if (*charging_current > battery->pdata->siop_hv_charging_limit_current)
					*charging_current = battery->pdata->siop_hv_charging_limit_current;
			}
#if defined(CONFIG_CCIC_NOTIFIER)
		} else if (battery->cable_type == SEC_BATTERY_CABLE_PDIC) {
			if (*input_current > (6000 / battery->input_voltage))
				*input_current = 6000 / battery->input_voltage;
			if (*charging_current > battery->pdata->siop_charging_limit_current)
				*charging_current = battery->pdata->siop_charging_limit_current;
#endif
		} else {
			if (*input_current > battery->pdata->siop_input_limit_current)
				*input_current = battery->pdata->siop_input_limit_current;
			if (*charging_current > battery->pdata->siop_charging_limit_current)
				*charging_current = battery->pdata->siop_charging_limit_current;
		}
	}
}

#if defined(CONFIG_MUIC_HV) || defined(CONFIG_SUPPORT_QC30)
extern int muic_afc_set_voltage(int vol);
#endif
#if !defined(CONFIG_SEC_FACTORY)
static int sec_bat_check_mix_temp(struct sec_battery_info *battery, int input_current)
{
	if (battery->pdata->chg_temp_check && battery->siop_level >= 100 && is_not_wireless_type(battery->cable_type)) {
		if ((!battery->mix_limit &&
				(battery->temperature > battery->pdata->mix_high_temp) &&
				(battery->chg_temp > battery->pdata->mix_high_chg_temp)) ||
			(battery->mix_limit &&
				(battery->temperature >= battery->pdata->mix_high_temp_recovery))) {
			int max_input_current =
				battery->pdata->full_check_current_1st + 50;

			/* inpu current = float voltage * (topoff_current_1st + 50mA(margin)) / (vbus_level * 0.9) */
			input_current = ((battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv) * max_input_current) /
				(battery->input_voltage * 90) / 10;
			if (input_current > max_input_current)
				input_current = max_input_current;

			battery->mix_limit = true;
			/* skip other heating control */
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL,
						  SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL);
		} else if (battery->mix_limit) {
			battery->mix_limit = false;
		}

		pr_info("%s: mix_limit(%d), temp(%d), chg_temp(%d), input_current(%d)\n",
			__func__, battery->mix_limit, battery->temperature, battery->chg_temp, input_current);
	} else {
		battery->mix_limit = false;
	}
	return input_current;
}

static int sec_bat_check_wpc_temp(struct sec_battery_info *battery, int input_current)
{
	if (is_wireless_type(battery->cable_type)) {
		union power_supply_propval value = {0, };
		int wpc_vout_level = battery->wpc_vout_level;

		if (battery->siop_level >= 100) {
			unsigned int chg_limit_index = battery->chg_limit;

			if ((chg_limit_index < battery->pdata->wpc_high_temp_size) &&
				(battery->chg_temp > battery->pdata->wpc_high_temp[chg_limit_index])) {
				do {
					pr_info("%s: TRIGGER - chg_limit_index(%d), wpc_high_temp(%d)\n",
						__func__, chg_limit_index, battery->pdata->wpc_high_temp[chg_limit_index]);
					chg_limit_index++;
					if (is_hv_wireless_type(battery->cable_type))
						wpc_vout_level = WIRELESS_VOUT_5V;
				} while ((chg_limit_index < battery->pdata->wpc_high_temp_size) &&
					(battery->chg_temp > battery->pdata->wpc_high_temp[chg_limit_index]));
			} else if ((chg_limit_index) &&
				(battery->chg_temp < battery->pdata->wpc_high_temp_recovery[chg_limit_index - 1])) {
				do {
					pr_info("%s: RELEASE - chg_limit_index(%d), wpc_high_temp_recovery(%d)\n",
						__func__, chg_limit_index, battery->pdata->wpc_high_temp_recovery[chg_limit_index - 1]);
					chg_limit_index--;
					if (is_hv_wireless_type(battery->cable_type))
						wpc_vout_level = WIRELESS_VOUT_10V;
				} while ((chg_limit_index) &&
					(battery->chg_temp < battery->pdata->wpc_high_temp_recovery[chg_limit_index - 1]));
			}

			input_current = (!chg_limit_index) ? input_current :
				battery->pdata->wpc_charging_limit_current;
			battery->chg_limit = chg_limit_index;
		} else {
			if (is_hv_wireless_type(battery->cable_type)) {
				if (battery->cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE) {
					if ((!battery->chg_limit &&	battery->chg_temp > battery->pdata->wpc_lcd_on_high_temp) ||
						(battery->chg_limit && battery->chg_temp >= battery->pdata->wpc_lcd_on_high_temp_rec)) {
						input_current = battery->pdata->wpc_lcd_on_charging_limit_current;
						battery->chg_limit = true;
						wpc_vout_level = (battery->capacity < 95) ? WIRELESS_VOUT_5V : WIRELESS_VOUT_10V;
					} else if (battery->chg_limit) {
						battery->chg_limit = false;
					}
				}
			}
		}

		if (is_hv_wireless_type(battery->cable_type)) {
			if (battery->current_event & SEC_BAT_CURRENT_EVENT_SWELLING_MODE)
				wpc_vout_level = WIRELESS_VOUT_5V;
			else if(!battery->chg_limit)
				wpc_vout_level = WIRELESS_VOUT_10V;

			if (wpc_vout_level != battery->wpc_vout_level) {
				battery->wpc_vout_level = wpc_vout_level;
				value.intval = wpc_vout_level;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
				pr_info("%s: change vout level(%d)",
					__func__, battery->wpc_vout_level);
				battery->aicl_current = 0; /* reset aicl current */
			}
		}

		pr_info("%s: change input_current(%d), vout_level(%d), chg_limit(%d)\n",
			__func__, input_current, battery->wpc_vout_level, battery->chg_limit);
	}

	return input_current;
}

static void sec_bat_check_afc_temp(struct sec_battery_info *battery, int *input_current, int *charging_current)
{
#if defined(CONFIG_MUIC_HV) || defined(CONFIG_SUPPORT_QC30)
	if (battery->siop_level >= 100) {
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_CHG_LIMIT) {
			battery->chg_limit = battery->vbus_chg_by_siop = false;
			*input_current = battery->pdata->pre_afc_input_current;
			{
				/* change input current */
				union power_supply_propval value;
				battery->charge_power = battery->input_voltage * (*input_current);
				value.intval = *input_current;
				psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CURRENT_MAX, value);
				battery->input_current = *input_current;
			}
			/* set current event */
			cancel_delayed_work(&battery->afc_work);
			wake_unlock(&battery->afc_wake_lock);
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_AFC,
						  (SEC_BAT_CURRENT_EVENT_CHG_LIMIT | SEC_BAT_CURRENT_EVENT_AFC));
			/* vbus level : 5V --> 9V */
			if (battery->chg_limit_recovery_cable == SEC_BATTERY_CABLE_12V_TA) {
				muic_afc_set_voltage(SEC_INPUT_VOLTAGE_12V);
			} else if (battery->chg_limit_recovery_cable == SEC_BATTERY_CABLE_9V_TA) {
				muic_afc_set_voltage(SEC_INPUT_VOLTAGE_9V);
			} else
				pr_info("%s: cable_type(%d), chg_limit_recovery_cable(%d) vbus_by_siop(%d)\n", __func__,
					battery->cable_type, battery->chg_limit_recovery_cable, battery->vbus_chg_by_siop);
		} else if (!battery->chg_limit && is_hv_wire_type(battery->cable_type) && (battery->chg_temp > battery->pdata->chg_high_temp)) {
			*input_current = battery->pdata->chg_input_limit_current;
			*charging_current = battery->pdata->chg_charging_limit_current;
			battery->chg_limit = true;
		} else if (!battery->chg_limit && battery->max_charge_power >= (battery->pdata->pd_charging_charge_power - 500) && (battery->chg_temp > battery->pdata->chg_high_temp)) {
			*input_current = RP_CURRENT_DEFAULT_IN;
			*charging_current = RP_CURRENT_DEFAULT_OUT;
			battery->chg_limit = true;
		} else if (battery->chg_limit && is_hv_wire_type(battery->cable_type)) {
			if (battery->chg_temp < battery->pdata->chg_high_temp_recovery) {
				*input_current = battery->pdata->charging_current[battery->cable_type].input_current_limit;
				*charging_current = battery->pdata->charging_current[battery->cable_type].fast_charging_current;
				battery->chg_limit = false;
			} else {
				*input_current = battery->pdata->chg_input_limit_current;
				*charging_current = battery->pdata->chg_charging_limit_current;
				battery->chg_limit = true;
			}
		} else if (battery->chg_limit && battery->max_charge_power >= (battery->pdata->pd_charging_charge_power - 500)) {
			if (battery->chg_temp < battery->pdata->chg_high_temp_recovery) {
				*input_current = battery->pdata->charging_current[battery->cable_type].input_current_limit;
				*charging_current = battery->pdata->charging_current[battery->cable_type].fast_charging_current;
				battery->chg_limit = false;
			} else {
				*input_current = battery->pdata->chg_input_limit_current;
				*charging_current = battery->pdata->chg_charging_limit_current;
				battery->chg_limit = true;
			}
		}
		pr_info("%s: cable_type(%d), chg_limit(%d) vbus_by_siop(%d)\n", __func__,
			battery->cable_type, battery->chg_limit, battery->vbus_chg_by_siop);
	} else if (is_hv_wire_type(battery->cable_type) && is_hv_wire_type(battery->wire_status) &&
		   (battery->cable_type != SEC_BATTERY_CABLE_QC30) &&
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
		   (battery->muic_cable_type != ATTACHED_DEV_POGO_DOCK_9V_MUIC) &&
#endif
		   (battery->status == POWER_SUPPLY_STATUS_CHARGING) && !battery->vbus_chg_by_siop) {
			battery->chg_limit_recovery_cable = battery->cable_type;
			battery->vbus_chg_by_siop = true;
			battery->chg_limit = false;
			/* vbus level : 9V --> 5V */
			muic_afc_set_voltage(SEC_INPUT_VOLTAGE_5V);
			pr_info("%s: vbus set 5V by siop(recovery cable: %d)\n", __func__,battery->chg_limit_recovery_cable);
	}
#else
	if (!battery->chg_limit && is_hv_wire_type(battery->cable_type) && (battery->chg_temp > battery->pdata->chg_high_temp)) {
		*input_current = battery->pdata->chg_input_limit_current;
		*charging_current = battery->pdata->chg_charging_limit_current;
		battery->chg_limit = true;
	} else if (battery->chg_limit && is_hv_wire_type(battery->cable_type) && (battery->chg_temp < battery->pdata->chg_high_temp_recovery)) {
		*input_current = battery->pdata->charging_current[battery->cable_type].input_current_limit;
		*charging_current = battery->pdata->charging_current[battery->cable_type].fast_charging_current;
		battery->chg_limit = false;
	}
#endif
}

#if defined(CONFIG_CCIC_NOTIFIER)
extern void select_pdo(int num);
static int sec_bat_check_pdic_temp(struct sec_battery_info *battery, int input_current)
{
	if (battery->pdic_ps_rdy && battery->pdata->chg_temp_check &&
		battery->siop_level >= 100 && battery->cable_type == SEC_BATTERY_CABLE_PDIC) {

		struct sec_bat_pdic_list *pd_list = &battery->pd_list;
		int pd_index = pd_list->now_pd_index;

		if (!battery->chg_limit) {
			if (battery->chg_temp > battery->pdata->chg_high_temp) {
				battery->chg_limit = true;
				pd_index--;
			} else
				pd_index++;
		} else {
			if (battery->chg_temp < battery->pdata->chg_high_temp_recovery)
				battery->chg_limit = false;
			else if (battery->chg_temp > battery->pdata->chg_high_temp)
				pd_index--;
		}

		if (pd_index < 0) {
			if (battery->chg_limit) {
				input_current = (input_current > (10000 / (pd_list->pd_info[0].input_voltage / 1000))) ?
					(10000 / (pd_list->pd_info[0].input_voltage / 1000)) : input_current;
				pd_index = -1;
			} else {
				pd_index = 0;
			}
			pd_list->now_pd_index = pd_index;
		} else {
			pd_index = 
				(pd_index >= pd_list->max_pd_count) ? (pd_list->max_pd_count - 1) : pd_index;

			if (pd_list->now_pd_index != pd_index) {
				/* change input current */
				input_current = (pd_list->pd_info[pd_index].input_current < input_current) ?
					pd_list->pd_info[pd_index].input_current : input_current;
				/* select next pdo */
				battery->pdic_ps_rdy = false;
				select_pdo(pd_list->pd_info[pd_index].pdo_index);
				pr_info("%s: change pd_list - index: %d, pdo_index: %d\n",
					__func__, pd_index, pd_list->pd_info[pd_index].pdo_index);
			}
		}
		pr_info("%s: pd_index(%d), input_current(%d), chg_limit(%d)\n",
			__func__, pd_index, input_current, battery->chg_limit);
	}

	return input_current;
}
#endif
#endif

static int sec_bat_check_afc_input_current(struct sec_battery_info *battery, int input_current)
{
	if (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC) {
		int work_delay = 0;

		if (!is_wireless_type(battery->cable_type)) {
			input_current = battery->pdata->pre_afc_input_current; // 1000mA
			work_delay = battery->pdata->pre_afc_work_delay;
		} else {
			input_current = battery->pdata->pre_wc_afc_input_current;
			/* do not reduce this time, this is for noble pad */
			work_delay = battery->pdata->pre_wc_afc_work_delay;
		}

		wake_lock(&battery->afc_wake_lock);
		if (!delayed_work_pending(&battery->afc_work))
			queue_delayed_work(battery->monitor_wqueue,
				&battery->afc_work , msecs_to_jiffies(work_delay));

		pr_info("%s: change input_current(%d), cable_type(%d)\n", __func__, input_current, battery->cable_type);
	}

	return input_current;
}

#if defined(CONFIG_CCIC_NOTIFIER)
static void sec_bat_get_input_current_in_power_list(struct sec_battery_info *battery)
{
	int pdo_num = battery->pdic_info.sink_status.current_pdo_num;
	int max_input_current = 0;

	max_input_current = battery->pdata->charging_current[SEC_BATTERY_CABLE_PDIC].input_current_limit =
		battery->pdic_info.sink_status.power_list[pdo_num].max_current;

	pr_info("%s:max_input_current : %dmA\n", __func__, max_input_current);
}

static void sec_bat_get_charging_current_in_power_list(struct sec_battery_info *battery)
{
	int max_charging_current = 0;
	int pdo_num = battery->pdic_info.sink_status.current_pdo_num;
	int pd_power = (battery->pdic_info.sink_status.power_list[pdo_num].max_voltage *
		battery->pdic_info.sink_status.power_list[pdo_num].max_current);

	/* We assume that output voltage to float voltage */
	max_charging_current = pd_power / (battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv);
	max_charging_current = max_charging_current > battery->pdata->max_charging_current ?
		battery->pdata->max_charging_current : max_charging_current;
	battery->pdata->charging_current[SEC_BATTERY_CABLE_PDIC].fast_charging_current = max_charging_current;
	battery->charge_power = pd_power;

	pr_info("%s:pd_charge_power : %dmW, max_charging_current : %dmA\n", __func__,
		battery->charge_power, max_charging_current);
}
#endif

static int sec_bat_set_charging_current(struct sec_battery_info *battery)
{
	static int afc_init = false;
	union power_supply_propval value = {0, };
	int input_current = battery->pdata->charging_current[battery->cable_type].input_current_limit,
		charging_current = battery->pdata->charging_current[battery->cable_type].fast_charging_current,
		topoff_current = battery->pdata->full_check_current_1st;
#if !defined(CONFIG_SEC_FACTORY)
	int temp = 0;
#endif

	if (battery->aicl_current)
		input_current = battery->aicl_current;
	mutex_lock(&battery->iolock);
	if (battery->cable_type == SEC_BATTERY_CABLE_NONE) {
	} else {
#if !defined(CONFIG_SEC_FACTORY)
		if (!(battery->current_event & SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL)) {
			input_current = sec_bat_check_mix_temp(battery, input_current);
		}
#endif

		/* check input current */
#if !defined(CONFIG_SEC_FACTORY)
		if (!(battery->current_event & SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL)) {
			if (battery->pdata->wpc_temp_check) {
				temp = sec_bat_check_wpc_temp(battery, input_current);
				if (input_current > temp)
					input_current = temp;
			}
			if (battery->pdata->chg_temp_check)
				sec_bat_check_afc_temp(battery, &input_current, &charging_current);
#if defined(CONFIG_CCIC_NOTIFIER)
			input_current = sec_bat_check_pdic_temp(battery, input_current);
#endif
		}
#endif

		input_current = sec_bat_check_afc_input_current(battery, input_current);
		/* Set limited max current with hv wire cable when store mode is set and LDU 
			Limited max current should be set with over 5% capacity since target could be turned off during boot up */ 
		if (battery->store_mode && is_hv_wire_type(battery->cable_type) && (battery->capacity >= 5)) {
			input_current = battery->pdata->store_mode_afc_input_current;
		}

		sec_bat_get_charging_current_by_siop(battery, &input_current, &charging_current);

		/* Calculate wireless input current under the specific conditions (siop_event, wpc_sleep_mode, chg_limit)*/
		if (battery->wc_status != SEC_WIRELESS_PAD_NONE) {
			input_current = sec_bat_get_wireless_current(battery, input_current);
		}

		/* check topoff current */
		if (battery->charging_mode == SEC_BATTERY_CHARGING_2ND &&
			battery->pdata->full_check_type_2nd == SEC_BATTERY_FULLCHARGED_CHGPSY) {
			topoff_current =
				battery->pdata->full_check_current_2nd;
		}

		/* check swelling state */
		if (is_wireless_type(battery->cable_type)) {
			if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING) {
				charging_current = (charging_current > battery->pdata->swelling_wc_low_temp_current) ?
					battery->pdata->swelling_wc_low_temp_current : charging_current;
				topoff_current = (topoff_current > battery->pdata->swelling_low_temp_topoff) ?
					battery->pdata->swelling_low_temp_topoff : topoff_current;
			} else if (battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING) {
				charging_current = (charging_current > battery->pdata->swelling_wc_high_temp_current) ?
					battery->pdata->swelling_wc_high_temp_current : charging_current;
				topoff_current = (topoff_current > battery->pdata->swelling_high_temp_topoff) ?
					battery->pdata->swelling_high_temp_topoff : topoff_current;
			} else if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP) {
				charging_current = (charging_current > battery->pdata->swelling_wc_low_temp_current) ?
					battery->pdata->swelling_wc_low_temp_current : charging_current;
			}
		} else {
			if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING) {
				charging_current = (charging_current > battery->pdata->swelling_low_temp_current) ?
					battery->pdata->swelling_low_temp_current : charging_current;
				topoff_current = (topoff_current > battery->pdata->swelling_low_temp_topoff) ?
					battery->pdata->swelling_low_temp_topoff : topoff_current;
			} else if (battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING) {
				charging_current = (charging_current > battery->pdata->swelling_high_temp_current) ?
					battery->pdata->swelling_high_temp_current : charging_current;
				topoff_current = (topoff_current > battery->pdata->swelling_high_temp_topoff) ?
					battery->pdata->swelling_high_temp_topoff : topoff_current;
			} else if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP) {
				charging_current = (charging_current > battery->pdata->swelling_low_temp_current) ?
					battery->pdata->swelling_low_temp_current : charging_current;
			}

			/* usb unconfigured or suspend*/
			if ((battery->cable_type == SEC_BATTERY_CABLE_USB) && !lpcharge &&
				(battery->pdic_info.sink_status.rp_currentlvl == RP_CURRENT_LEVEL_DEFAULT)) {
				if (battery->current_event & SEC_BAT_CURRENT_EVENT_USB_100MA) {
					pr_info("%s: usb unconfigured\n", __func__);
					input_current = USB_CURRENT_UNCONFIGURED;
					charging_current = USB_CURRENT_UNCONFIGURED;
				}
			}
		}
	}

	/* In wireless charging, must be set charging current before input current. */
	if (is_wireless_type(battery->cable_type) &&
		battery->charging_current != charging_current) {
		value.intval = charging_current;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_AVG, value);
		battery->charging_current = charging_current;
	}
	/* set input current, charging current */
	if ((battery->input_current != input_current) ||
		(battery->charging_current != charging_current)) {
		/* update charge power */
		battery->charge_power = battery->input_voltage * input_current;
		if (battery->charge_power > battery->max_charge_power)
			battery->max_charge_power = battery->charge_power;

		value.intval = input_current;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);
		battery->input_current = input_current;

		value.intval = charging_current;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_NOW, value);

		if (charging_current <= 100)
			battery->charging_current = 100;
		else
			battery->charging_current = charging_current;
		pr_info("%s: power(%d), input(%d), charge(%d)\n", __func__,
			battery->charge_power, battery->input_current, battery->charging_current);
	}

	/* set topoff current */
	if (battery->topoff_current != topoff_current) {
		value.intval = topoff_current;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_FULL, value);
		battery->topoff_current = topoff_current;
	}
	if (!afc_init) {
		afc_init = true;
#if defined(CONFIG_AFC_CHARGER_MODE)
		value.intval = 1;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_AFC_CHARGER_MODE,
			value);
#endif
	}
	mutex_unlock(&battery->iolock);
	return 0;
}

static int sec_bat_set_charge(
				struct sec_battery_info *battery,
				int chg_mode)
{
	union power_supply_propval val = {0, };
	ktime_t current_time = {0, };
	struct timespec ts = {0, };

	if (battery->cable_type == SEC_BATTERY_CABLE_HMT_CONNECTED)
		return 0;

	if ((battery->current_event & SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE) &&
		(chg_mode == SEC_BAT_CHG_MODE_CHARGING)) {
		dev_info(battery->dev, "%s: charge disable by HMT\n", __func__);
		chg_mode = SEC_BAT_CHG_MODE_CHARGING_OFF;
	}

	val.intval = battery->status;
	psy_do_property(battery->pdata->charger_name, set,
		POWER_SUPPLY_PROP_STATUS, val);
	current_time = ktime_get_boottime();
	ts = ktime_to_timespec(current_time);

	if (chg_mode == SEC_BAT_CHG_MODE_CHARGING) {
		/*Reset charging start time only in initial charging start */
		if (battery->charging_start_time == 0) {
			if (ts.tv_sec < 1)
				ts.tv_sec = 1;
			battery->charging_start_time = ts.tv_sec;
			battery->charging_next_time =
				battery->pdata->charging_reset_time;
		}
		battery->charging_block = false;

#if defined(CONFIG_BATTERY_SBM_DATA)
		sec_bat_add_sbm_data(battery, SBM_DATA_SET_CHARGE);
#endif		
	} else {
		battery->charging_start_time = 0;
		battery->charging_passed_time = 0;
		battery->charging_next_time = 0;
		battery->charging_fullcharged_time = 0;
		battery->full_check_cnt = 0;
		battery->charging_block = true;
#if defined(CONFIG_STEP_CHARGING)
		sec_bat_reset_step_charging(battery);
#endif
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.ab_vbat_check_count = 0;
#endif
	}

	battery->temp_highlimit_cnt = 0;
	battery->temp_high_cnt = 0;
	battery->temp_low_cnt = 0;
	battery->temp_recover_cnt = 0;

	val.intval = chg_mode;
	psy_do_property(battery->pdata->charger_name, set,
		POWER_SUPPLY_PROP_CHARGING_ENABLED, val);

	return 0;
}

static bool sec_bat_check_by_psy(struct sec_battery_info *battery)
{
	char *psy_name = NULL;
	union power_supply_propval value = {0, };
	bool ret = true;

	switch (battery->pdata->battery_check_type) {
	case SEC_BATTERY_CHECK_PMIC:
		psy_name = battery->pdata->pmic_name;
		break;
	case SEC_BATTERY_CHECK_FUELGAUGE:
		psy_name = battery->pdata->fuelgauge_name;
		break;
	case SEC_BATTERY_CHECK_CHARGER:
		psy_name = battery->pdata->charger_name;
		break;
	default:
		dev_err(battery->dev,
			"%s: Invalid Battery Check Type\n", __func__);
		ret = false;
		goto battery_check_error;
		break;
	}

	psy_do_property(psy_name, get,
		POWER_SUPPLY_PROP_PRESENT, value);
	ret = (bool)value.intval;

battery_check_error:
	return ret;
}

static bool sec_bat_check(struct sec_battery_info *battery)
{
	bool ret = true;

	if (battery->factory_mode || battery->is_jig_on) {
		dev_dbg(battery->dev, "%s: No need to check in factory mode\n",
			__func__);
		return ret;
	}

	if (battery->health != POWER_SUPPLY_HEALTH_GOOD &&
		battery->health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
		dev_dbg(battery->dev, "%s: No need to check\n", __func__);
		return ret;
	}

	switch (battery->pdata->battery_check_type) {
	case SEC_BATTERY_CHECK_ADC:
		if(battery->cable_type == SEC_BATTERY_CABLE_NONE)
			ret = battery->present;
		else
			ret = sec_bat_check_vf_adc(battery);
		break;
	case SEC_BATTERY_CHECK_INT:
	case SEC_BATTERY_CHECK_CALLBACK:
		if(battery->cable_type == SEC_BATTERY_CABLE_NONE) {
			ret = battery->present;
		} else {
			if (battery->pdata->check_battery_callback)
				ret = battery->pdata->check_battery_callback();
		}
		break;
	case SEC_BATTERY_CHECK_PMIC:
	case SEC_BATTERY_CHECK_FUELGAUGE:
	case SEC_BATTERY_CHECK_CHARGER:
		ret = sec_bat_check_by_psy(battery);
		break;
	case SEC_BATTERY_CHECK_NONE:
		dev_dbg(battery->dev, "%s: No Check\n", __func__);
	default:
		break;
	}

	return ret;
}

static bool sec_bat_get_cable_type(
			struct sec_battery_info *battery,
			int cable_source_type)
{
	bool ret = false;
	int cable_type = battery->cable_type;

	if (cable_source_type & SEC_BATTERY_CABLE_SOURCE_CALLBACK) {
		if (battery->pdata->check_cable_callback)
			cable_type =
				battery->pdata->check_cable_callback();
	}

	if (cable_source_type & SEC_BATTERY_CABLE_SOURCE_ADC) {
		if (gpio_get_value_cansleep(
			battery->pdata->bat_gpio_ta_nconnected) ^
			battery->pdata->bat_polarity_ta_nconnected)
			cable_type = SEC_BATTERY_CABLE_NONE;
		else
			cable_type =
				sec_bat_get_charger_type_adc(battery);
	}

	if (battery->cable_type == cable_type) {
		dev_dbg(battery->dev,
			"%s: No need to change cable status\n", __func__);
	} else {
		if (cable_type < SEC_BATTERY_CABLE_NONE ||
			cable_type >= SEC_BATTERY_CABLE_MAX) {
			dev_err(battery->dev,
				"%s: Invalid cable type\n", __func__);
		} else {
			battery->cable_type = cable_type;
			if (battery->pdata->check_cable_result_callback)
				battery->pdata->check_cable_result_callback(
						battery->cable_type);

			ret = true;

			dev_dbg(battery->dev, "%s: Cable Changed (%d)\n",
				__func__, battery->cable_type);
		}
	}

	return ret;
}

static void sec_bat_set_charging_status(struct sec_battery_info *battery,
		int status) {
	union power_supply_propval value = {0, };

	switch (status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (battery->siop_level != 100)
			battery->stop_timer = true;
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		if (battery->status == POWER_SUPPLY_STATUS_FULL ||
		    (battery->capacity == 100 && !battery->slate_mode)) {
			value.intval = 100;
			psy_do_property(battery->pdata->fuelgauge_name, set,
					POWER_SUPPLY_PROP_CHARGE_FULL, value);
			/* To get SOC value (NOT raw SOC), need to reset value */
			value.intval = 0;
			psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_CAPACITY, value);
			battery->capacity = value.intval;
		}
		battery->expired_time = battery->pdata->expired_time;
		battery->prev_safety_time = 0;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		if (is_wireless_type(battery->cable_type)) {
#ifdef CONFIG_CS100_JPNCONCEPT
				if (battery->charging_mode == SEC_BATTERY_CHARGING_2ND ||
					battery->charging_passed_time > battery->pdata->charging_total_time) {
#endif
					value.intval = POWER_SUPPLY_STATUS_FULL;
					psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_PROP_STATUS, value);
#ifdef CONFIG_CS100_JPNCONCEPT
				}
#endif
			}
			break;
		default:
			break;
	}
	battery->status = status;
}

static bool sec_bat_battery_cable_check(struct sec_battery_info *battery)
{
	if (!sec_bat_check(battery)) {
		if (battery->check_count < battery->pdata->check_count)
			battery->check_count++;
		else {
			dev_err(battery->dev,
				"%s: Battery Disconnected\n", __func__);
			battery->present = false;
			battery->health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;

			if (battery->status !=
				POWER_SUPPLY_STATUS_DISCHARGING) {
				sec_bat_set_charging_status(battery,
						POWER_SUPPLY_STATUS_NOT_CHARGING);
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_BUCK_OFF);
			}

			if (battery->pdata->check_battery_result_callback)
				battery->pdata->
					check_battery_result_callback();
			return false;
		}
	} else
		battery->check_count = 0;

	battery->present = true;

	if (battery->health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
		battery->health = POWER_SUPPLY_HEALTH_GOOD;

		if (battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_CHARGING);
#if defined(CONFIG_BATTERY_SWELLING)
			if (!battery->swelling_mode)
#endif
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
		}
	}

	dev_dbg(battery->dev, "%s: Battery Connected\n", __func__);

	if (battery->pdata->cable_check_type &
		SEC_BATTERY_CABLE_CHECK_POLLING) {
		if (sec_bat_get_cable_type(battery,
			battery->pdata->cable_source_type)) {
			wake_lock(&battery->cable_wake_lock);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->cable_work, 0);
		}
	}
	return true;
}

static int sec_bat_ovp_uvlo_by_psy(struct sec_battery_info *battery)
{
	char *psy_name = NULL;
	union power_supply_propval value = {0, };

	value.intval = POWER_SUPPLY_HEALTH_GOOD;

	switch (battery->pdata->ovp_uvlo_check_type) {
	case SEC_BATTERY_OVP_UVLO_PMICPOLLING:
		psy_name = battery->pdata->pmic_name;
		break;
	case SEC_BATTERY_OVP_UVLO_CHGPOLLING:
		psy_name = battery->pdata->charger_name;
		break;
	default:
		dev_err(battery->dev,
			"%s: Invalid OVP/UVLO Check Type\n", __func__);
		goto ovp_uvlo_check_error;
		break;
	}

	psy_do_property(psy_name, get,
		POWER_SUPPLY_PROP_HEALTH, value);

ovp_uvlo_check_error:
	return value.intval;
}

static bool sec_bat_ovp_uvlo_result(
		struct sec_battery_info *battery, int health)
{
	if (battery->health != health) {
		battery->health = health;
		switch (health) {
		case POWER_SUPPLY_HEALTH_GOOD:
			dev_info(battery->dev, "%s: Safe voltage\n", __func__);
			dev_info(battery->dev, "%s: is_recharging : %d\n", __func__, battery->is_recharging);
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_CHARGING);
			battery->charging_mode = SEC_BATTERY_CHARGING_1ST;
#if defined(CONFIG_BATTERY_SWELLING)
			if (!battery->swelling_mode)
#endif
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
			battery->health_check_count = 0;
			break;
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			dev_info(battery->dev,
				"%s: Unsafe voltage (%d)\n",
				__func__, health);
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_NOT_CHARGING);
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			battery->is_recharging = false;
			battery->health_check_count = DEFAULT_HEALTH_CHECK_COUNT;
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_UNSAFE_VOLTAGE]++;
#endif
			/* Take the wakelock during 10 seconds
			   when over-voltage status is detected	 */
			wake_lock_timeout(&battery->vbus_wake_lock, HZ * 10);
			break;
		}
		power_supply_changed(&battery->psy_bat);
		return true;
	}

	return false;
}

static bool sec_bat_ovp_uvlo(struct sec_battery_info *battery)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;

	if (battery->wdt_kick_disable) {
		dev_dbg(battery->dev,
			"%s: No need to check in wdt test\n",
			__func__);
		return false;
	} else if ((battery->status == POWER_SUPPLY_STATUS_FULL) &&
		   (battery->charging_mode == SEC_BATTERY_CHARGING_NONE)) {
		dev_dbg(battery->dev, "%s: No need to check in Full status", __func__);
		return false;
	}

	if (battery->health != POWER_SUPPLY_HEALTH_GOOD &&
		battery->health != POWER_SUPPLY_HEALTH_OVERVOLTAGE &&
		battery->health != POWER_SUPPLY_HEALTH_UNDERVOLTAGE) {
		dev_dbg(battery->dev, "%s: No need to check\n", __func__);
		return false;
	}

	health = battery->health;

	switch (battery->pdata->ovp_uvlo_check_type) {
	case SEC_BATTERY_OVP_UVLO_CALLBACK:
		if (battery->pdata->ovp_uvlo_callback)
			health = battery->pdata->ovp_uvlo_callback();
		break;
	case SEC_BATTERY_OVP_UVLO_PMICPOLLING:
	case SEC_BATTERY_OVP_UVLO_CHGPOLLING:
		health = sec_bat_ovp_uvlo_by_psy(battery);
		break;
	case SEC_BATTERY_OVP_UVLO_PMICINT:
	case SEC_BATTERY_OVP_UVLO_CHGINT:
		/* nothing for interrupt check */
	default:
		break;
	}

	/* Move the location for calling the get_health
	   in case of attaching the jig */ 
	if (battery->factory_mode || battery->is_jig_on) {
		dev_dbg(battery->dev,
			"%s: No need to check in factory mode\n",
			__func__);
		return false;
	}

	return sec_bat_ovp_uvlo_result(battery, health);
}

static bool sec_bat_check_recharge(struct sec_battery_info *battery)
{
#if defined(CONFIG_BATTERY_SWELLING)
	if (battery->swelling_mode == SWELLING_MODE_CHARGING ||
		battery->swelling_mode == SWELLING_MODE_FULL) {
		pr_info("%s: Skip normal recharge check routine for swelling mode\n",
			__func__);
		return false;
	}
#endif
	if ((battery->status == POWER_SUPPLY_STATUS_CHARGING) &&
			(battery->pdata->full_condition_type &
			 SEC_BATTERY_FULL_CONDITION_NOTIMEFULL) &&
			(battery->charging_mode == SEC_BATTERY_CHARGING_NONE)) {
		dev_info(battery->dev,
				"%s: Re-charging by NOTIMEFULL (%d)\n",
				__func__, battery->capacity);
		goto check_recharge_check_count;
	}

	if (battery->status == POWER_SUPPLY_STATUS_FULL &&
			battery->charging_mode == SEC_BATTERY_CHARGING_NONE) {
		int recharging_voltage = battery->pdata->recharge_condition_vcell;
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP) {
			recharging_voltage -= 100; // float voltage - 100mV
			dev_info(battery->dev, "%s: recharging voltage changed by low temp(%d)\n",
					__func__, recharging_voltage);
		}
		dev_info(battery->dev, "%s: recharging voltage (%d)\n",
				__func__, recharging_voltage);

		if ((battery->pdata->recharge_condition_type &
					SEC_BATTERY_RECHARGE_CONDITION_SOC) &&
				(battery->capacity <=
				 battery->pdata->recharge_condition_soc)) {
			battery->expired_time = battery->pdata->recharging_expired_time;
			battery->prev_safety_time = 0;
			dev_info(battery->dev,
					"%s: Re-charging by SOC (%d)\n",
					__func__, battery->capacity);
			goto check_recharge_check_count;
		}

		if ((battery->pdata->recharge_condition_type &
		     SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL) &&
		    (battery->voltage_avg <= recharging_voltage)) {
			battery->expired_time = battery->pdata->recharging_expired_time;
			battery->prev_safety_time = 0;
			dev_info(battery->dev,
					"%s: Re-charging by average VCELL (%d)\n",
					__func__, battery->voltage_avg);
			goto check_recharge_check_count;
		}

		if ((battery->pdata->recharge_condition_type &
		     SEC_BATTERY_RECHARGE_CONDITION_VCELL) &&
		    (battery->voltage_now <= recharging_voltage)) {
			battery->expired_time = battery->pdata->recharging_expired_time;
			battery->prev_safety_time = 0;
			dev_info(battery->dev,
					"%s: Re-charging by VCELL (%d)\n",
					__func__, battery->voltage_now);
			goto check_recharge_check_count;
		}
	}

	battery->recharge_check_cnt = 0;
	return false;

check_recharge_check_count:
	if (battery->recharge_check_cnt <
		battery->pdata->recharge_check_count)
		battery->recharge_check_cnt++;
	dev_dbg(battery->dev,
		"%s: recharge count = %d\n",
		__func__, battery->recharge_check_cnt);

	if (battery->recharge_check_cnt >=
		battery->pdata->recharge_check_count)
		return true;
	else
		return false;
}

static bool sec_bat_voltage_check(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		dev_dbg(battery->dev,
			"%s: Charging Disabled\n", __func__);
		return true;
	}

	/* OVP/UVLO check */
	if (sec_bat_ovp_uvlo(battery)) {
		if (battery->pdata->ovp_uvlo_result_callback)
			battery->pdata->
				ovp_uvlo_result_callback(battery->health);
		return false;
	}

	if ((battery->status == POWER_SUPPLY_STATUS_FULL) &&
#if defined(CONFIG_BATTERY_SWELLING)
	    (battery->charging_mode == SEC_BATTERY_CHARGING_2ND ||
	     battery->is_recharging || battery->swelling_mode)) {
#else
		(battery->charging_mode == SEC_BATTERY_CHARGING_2ND ||
		 battery->is_recharging)) {
#endif
		value.intval = 0;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
		if (value.intval <
			battery->pdata->full_condition_soc &&
				battery->voltage_now <
				(battery->pdata->recharge_condition_vcell - 50)) {
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_CHARGING);
			battery->voltage_now = 1080;
			battery->voltage_avg = 1080;
			power_supply_changed(&battery->psy_bat);
			dev_info(battery->dev,
				"%s: battery status full -> charging, RepSOC(%d)\n", __func__, value.intval);
			return false;
		}
	}

	/* Re-Charging check */
	if (sec_bat_check_recharge(battery)) {
		if (battery->pdata->full_check_type !=
			SEC_BATTERY_FULLCHARGED_NONE)
			battery->charging_mode = SEC_BATTERY_CHARGING_1ST;
		else
			battery->charging_mode = SEC_BATTERY_CHARGING_2ND;
		battery->is_recharging = true;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.recharge_count++;
		battery->cisd.recharge_count_2++;
		if (battery->cisd.charging_end_time_2 > 0) {
			struct timespec now_ts;
			struct cisd *pcisd = &battery->cisd;
			now_ts = ktime_to_timespec(ktime_get_boottime());
			if ((now_ts.tv_sec - pcisd->charging_end_time_2) < pcisd->data[CISD_DATA_RECHARGING_TIME]) {
				pcisd->data[CISD_DATA_RECHARGING_TIME] = (int)(now_ts.tv_sec - pcisd->charging_end_time_2);
				dev_info(battery->dev,
						"%s: cisd Recharging TIME(%d)\n", __func__, pcisd->data[CISD_DATA_RECHARGING_TIME]);
			}
		}
#endif
#if defined(CONFIG_BATTERY_SWELLING)
		if (!battery->swelling_mode)
#endif
		sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
		return false;
	}

	return true;
}

static void sec_bat_set_swelling_float_voltage(struct sec_battery_info *battery, int float_voltage)
{
	union power_supply_propval val = {0, };

	val.intval = float_voltage;

	if ((battery->voltage_now <= battery->pdata->wa_volt_recov) && (float_voltage == battery->pdata->wa_float_voltage)) {
		val.intval = battery->pdata->swelling_drop_float_voltage;
	} else if (float_voltage > battery->pdata->swelling_drop_float_voltage) {
		if (battery->voltage_now >= battery->pdata->wa_volt_thr) {
			val.intval = battery->pdata->wa_float_voltage;
		} else {
			val.intval = battery->pdata->swelling_drop_float_voltage;
		}
	}

	if (float_voltage != val.intval) {
		if (!battery->charging_block) {
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, val);
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
		} else {
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, val);
		}
		pr_info("%s: float voltage change(%d -> %d)\n", __func__, float_voltage, val.intval);
	}
}

#if defined(CONFIG_BATTERY_SWELLING)
static void sec_bat_swelling_check(struct sec_battery_info *battery)
{
	union power_supply_propval val = {0, };
	int swelling_rechg_voltage = battery->pdata->swelling_high_rechg_voltage;
	bool en_swelling = false, en_rechg = false;

	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, val);

	pr_info("%s: status(%d), swell_mode(%d:%d:%d), cv(%d)mV, temp(%d)\n",
		__func__, battery->status, battery->swelling_mode,
		battery->charging_block, (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP),
		val.intval, battery->temperature);

	/* swelling_mode
		under voltage over voltage, battery missing */
	if ((battery->status == POWER_SUPPLY_STATUS_DISCHARGING) ||\
	    (battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING) ||
	    battery->skip_swelling) {
		pr_debug("%s: DISCHARGING or NOT-CHARGING or 15 test mode. stop swelling mode\n", __func__);
		battery->swelling_mode = SWELLING_MODE_NONE;
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
		goto skip_swelling_check;
	}

	if (!battery->swelling_mode) {
		if (((battery->temperature >= battery->pdata->swelling_high_temp_block) ||
			(battery->temperature <= battery->pdata->swelling_low_temp_block_2nd)) &&
			battery->pdata->temp_check_type) {
			pr_info("%s: swelling mode start. stop charging\n", __func__);
			battery->swelling_mode = SWELLING_MODE_CHARGING;
			battery->swelling_full_check_cnt = 0;
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
#if defined(CONFIG_BATTERY_CISD)
			if (battery->temperature >= battery->pdata->swelling_high_temp_block)
				battery->cisd.data[CISD_DATA_HIGH_TEMP_SWELLING]++;
			else if (battery->temperature <= battery->pdata->swelling_low_temp_block_2nd)
				battery->cisd.data[CISD_DATA_LOW_TEMP_SWELLING]++;
#endif
			en_swelling = true;
		} else if ((battery->temperature <= battery->pdata->swelling_low_temp_block_1st) &&
			!(battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP)) {
			pr_info("%s: low temperature reduce current\n", __func__);
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP,
						  SEC_BAT_CURRENT_EVENT_LOW_TEMP);
		} else if ((battery->temperature >= battery->pdata->swelling_low_temp_recov_1st) &&
			(battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP)) {
			pr_info("%s: normal temperature temperature recover current\n", __func__);
			sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_LOW_TEMP);
		}
	}

	if (!battery->voltage_now)
		return;

	if (battery->swelling_mode) {
		if (battery->temperature <= battery->pdata->swelling_low_temp_recov_2nd) { 
			swelling_rechg_voltage = battery->pdata->swelling_low_rechg_voltage;
		}

		sec_bat_set_swelling_float_voltage(battery, val.intval);

		if ((battery->temperature <= battery->pdata->swelling_high_temp_recov) &&
		    (battery->temperature >= battery->pdata->swelling_low_temp_recov_2nd)) {
			pr_info("%s: swelling mode end. restart charging\n", __func__);
			battery->swelling_mode = SWELLING_MODE_NONE;
			battery->charging_mode = SEC_BATTERY_CHARGING_1ST;
			sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
			/* restore 4.4V float voltage */
			val.intval = battery->pdata->swelling_normal_float_voltage;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, val);
			if (battery->temperature <= battery->pdata->swelling_low_temp_block_1st) {
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP,
							  SEC_BAT_CURRENT_EVENT_LOW_TEMP);
			}
		} else if (battery->voltage_now < swelling_rechg_voltage &&
			   battery->charging_block) {
			pr_info("%s: swelling mode recharging start. Vbatt(%d)\n",
				__func__, battery->voltage_now);
			battery->charging_mode = SEC_BATTERY_CHARGING_1ST;
			en_rechg = true;

			/* set charging enable */
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
			if (battery->temperature <= battery->pdata->swelling_low_temp_recov_2nd) {
				pr_info("%s: swelling mode reduce charging current(LOW-temp:%d)\n",
					__func__, battery->temperature);
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING,
							  SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING);
			} else if (battery->temperature >= battery->pdata->swelling_high_temp_recov) {
				pr_info("%s: swelling mode reduce charging current(HIGH-temp:%d)\n",
					__func__, battery->temperature);
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING,
							  SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING);
			}
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_SWELLING_CHARGING_COUNT]++;
#endif
		}
	}

	if (en_swelling && !en_rechg) {
		pr_info("%s : SAFETY TIME RESET (SWELLING MODE CHARING STOP!)\n", __func__);
		battery->expired_time = battery->pdata->expired_time;
		battery->prev_safety_time = 0;
	}

skip_swelling_check:
	dev_dbg(battery->dev, "%s end\n", __func__);
}
#endif

#if defined(CONFIG_BATTERY_AGE_FORECAST)
static bool sec_bat_set_aging_step(struct sec_battery_info *battery, int step)
{
	union power_supply_propval value = {0, };

	if (battery->pdata->num_age_step <= 0 || step < 0 || step >= battery->pdata->num_age_step) {
		pr_info("%s: [AGE] abnormal age step : %d/%d\n",
			__func__, step, battery->pdata->num_age_step-1);
		return false;
	}

	battery->pdata->age_step = step;

	/* float voltage */
	battery->pdata->chg_float_voltage =
		battery->pdata->age_data[battery->pdata->age_step].float_voltage;
	battery->pdata->swelling_normal_float_voltage =
		battery->pdata->chg_float_voltage;
	if (!battery->swelling_mode) {
		value.intval = battery->pdata->chg_float_voltage;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, value);
	}

	/* full/recharge condition */
	battery->pdata->recharge_condition_vcell =
		battery->pdata->age_data[battery->pdata->age_step].recharge_condition_vcell;
	battery->pdata->full_condition_soc =
		battery->pdata->age_data[battery->pdata->age_step].full_condition_soc;
	battery->pdata->full_condition_vcell =
		battery->pdata->age_data[battery->pdata->age_step].full_condition_vcell;

	value.intval = battery->pdata->full_condition_soc;
	psy_do_property(battery->pdata->fuelgauge_name, set,
		POWER_SUPPLY_PROP_CAPACITY_LEVEL, value);

	dev_info(battery->dev,
		 "%s: Step(%d/%d), Cycle(%d), float_v(%d), r_v(%d), f_s(%d), f_vl(%d)\n",
		 __func__,
		 battery->pdata->age_step, battery->pdata->num_age_step-1, battery->batt_cycle,
		 battery->pdata->chg_float_voltage,
		 battery->pdata->recharge_condition_vcell,
		 battery->pdata->full_condition_soc,
		 battery->pdata->full_condition_vcell);

#if defined(CONFIG_BATTERY_AGE_FORECAST_B2B)
	{
		int i;
		bool bChanged = false;
		battery->pdata->max_charging_current =
			battery->pdata->age_data[battery->pdata->age_step].max_charging_current;

		for (i = 0; i < SEC_BATTERY_CABLE_MAX; i++) {
			if (battery->pdata->charging_current[i].fast_charging_current >
				battery->pdata->max_charging_current) {

				dev_info(battery->dev, "%s: cable(%d) charging current(%d->%d)\n",
					__func__, i,
					battery->pdata->charging_current[i].fast_charging_current, 
					battery->pdata->max_charging_current);
				battery->pdata->charging_current[i].fast_charging_current =
					battery->pdata->max_charging_current;
				if (battery->cable_type == i)
					bChanged = true;
			}
		}
		if (bChanged)
			sec_bat_set_charging_current(battery);
	}
#endif

	return true;
}

static void sec_bat_aging_check(struct sec_battery_info *battery)
{
	int prev_step = battery->pdata->age_step;
	int calc_step = -1;
	bool ret = 0;

	if (battery->pdata->num_age_step <= 0 || battery->batt_cycle < 0)
		return;

	if (battery->temperature < 50) {
		pr_info("%s: [AGE] skip (temperature:%d)\n", __func__, battery->temperature);
		return;
	}

	for (calc_step = battery->pdata->num_age_step - 1; calc_step >= 0; calc_step--) {
		if (battery->pdata->age_data[calc_step].cycle <= battery->batt_cycle)
			break;
	}

	if (calc_step == prev_step)
		return;

	ret = sec_bat_set_aging_step(battery, calc_step);
	dev_info(battery->dev,
		 "%s: %s change step (%d->%d), Cycle(%d)\n",
		 __func__, ret ? "Succeed in" : "Fail to",
		 prev_step, battery->pdata->age_step, battery->batt_cycle);
}
#endif

static bool sec_bat_temperature(
				struct sec_battery_info *battery)
{
	bool ret;
	ret = true;

	if (is_wireless_type(battery->cable_type)) {
		battery->temp_highlimit_threshold =
			battery->pdata->temp_highlimit_threshold_normal;
		battery->temp_highlimit_recovery =
			battery->pdata->temp_highlimit_recovery_normal;
		battery->temp_high_threshold =
			battery->pdata->wpc_high_threshold_normal;
		battery->temp_high_recovery =
			battery->pdata->wpc_high_recovery_normal;
		battery->temp_low_recovery =
			battery->pdata->wpc_low_recovery_normal;
		battery->temp_low_threshold =
			battery->pdata->wpc_low_threshold_normal;
	} else {
		if (lpcharge) {
			battery->temp_highlimit_threshold =
				battery->pdata->temp_highlimit_threshold_lpm;
			battery->temp_highlimit_recovery =
				battery->pdata->temp_highlimit_recovery_lpm;
			battery->temp_high_threshold =
				battery->pdata->temp_high_threshold_lpm;
			battery->temp_high_recovery =
				battery->pdata->temp_high_recovery_lpm;
			battery->temp_low_recovery =
				battery->pdata->temp_low_recovery_lpm;
			battery->temp_low_threshold =
				battery->pdata->temp_low_threshold_lpm;
		} else {
			battery->temp_highlimit_threshold =
				battery->pdata->temp_highlimit_threshold_normal;
			battery->temp_highlimit_recovery =
				battery->pdata->temp_highlimit_recovery_normal;
			battery->temp_high_threshold =
				battery->pdata->temp_high_threshold_normal;
			battery->temp_high_recovery =
				battery->pdata->temp_high_recovery_normal;
			battery->temp_low_recovery =
				battery->pdata->temp_low_recovery_normal;
			battery->temp_low_threshold =
				battery->pdata->temp_low_threshold_normal;
		}
	}
	dev_info(battery->dev,
		"%s: HLT(%d) HLR(%d) HT(%d), HR(%d), LT(%d), LR(%d)\n",
		__func__, battery->temp_highlimit_threshold,
		battery->temp_highlimit_recovery,
		battery->temp_high_threshold,
		battery->temp_high_recovery,
		battery->temp_low_threshold,
		battery->temp_low_recovery);
	return ret;
}

static bool sec_bat_temperature_check(
				struct sec_battery_info *battery)
{
	int temp_value = 0;
	int pre_health = POWER_SUPPLY_HEALTH_GOOD;

	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		battery->health_change = false;
		dev_dbg(battery->dev,
			"%s: Charging Disabled\n", __func__);
		return true;
	}

	if (battery->health != POWER_SUPPLY_HEALTH_GOOD &&
		battery->health != POWER_SUPPLY_HEALTH_OVERHEAT &&
		battery->health != POWER_SUPPLY_HEALTH_COLD &&
		battery->health != POWER_SUPPLY_HEALTH_OVERHEATLIMIT) {
		dev_dbg(battery->dev, "%s: No need to check\n", __func__);
		return false;
	}

#if defined(CONFIG_ENG_BATTERY_CONCEPT) || defined(CONFIG_SEC_FACTORY)
	if (!battery->cooldown_mode) {
		dev_err(battery->dev, "%s: Forced temp check block\n", __func__);
		return true;
	}
#endif

	sec_bat_temperature(battery);

	switch (battery->pdata->temp_check_type) {
	case SEC_BATTERY_TEMP_CHECK_ADC:
		temp_value = battery->temp_adc;
		break;
	case SEC_BATTERY_TEMP_CHECK_TEMP:
		temp_value = battery->temperature;
		break;
	default:
		dev_err(battery->dev,
			"%s: Invalid Temp Check Type\n", __func__);
		return true;
	}
	pre_health = battery->health;

	if (battery->pdata->usb_thermal_source && (battery->usb_temp >= battery->temp_highlimit_threshold)) {
		if (battery->health != POWER_SUPPLY_HEALTH_OVERHEATLIMIT) {
			if (battery->temp_highlimit_cnt <
			    battery->pdata->temp_check_count) {
				battery->temp_highlimit_cnt++;
				battery->temp_high_cnt = 0;
				battery->temp_low_cnt = 0;
				battery->temp_recover_cnt = 0;
			}
			dev_err(battery->dev,
				"%s: usb therm highlimit count = %d\n",
				__func__, battery->temp_highlimit_cnt);
		}
	} else if (battery->pdata->usb_thermal_source && (battery->usb_temp > battery->temp_highlimit_recovery)
		&& (battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT)) {
			dev_err(battery->dev,
				"%s: usb therm highlimit \n",__func__);
	} else if (temp_value >= battery->temp_highlimit_threshold && !battery->pdata->usb_thermal_source) {
		if (battery->health != POWER_SUPPLY_HEALTH_OVERHEATLIMIT) {
			if (battery->temp_highlimit_cnt <
			    battery->pdata->temp_check_count) {
				battery->temp_highlimit_cnt++;
				battery->temp_high_cnt = 0;
				battery->temp_low_cnt = 0;
				battery->temp_recover_cnt = 0;
			}
			dev_err(battery->dev,
				"%s: highlimit count = %d\n",
				__func__, battery->temp_highlimit_cnt);
		}
	} else if (temp_value >= battery->temp_high_threshold) {
		if (battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT && !battery->pdata->usb_thermal_source) {
			if (temp_value <= battery->temp_highlimit_recovery) {
				if (battery->temp_recover_cnt <
				    battery->pdata->temp_check_count) {
					battery->temp_recover_cnt++;
					battery->temp_highlimit_cnt = 0;
					battery->temp_high_cnt = 0;
					battery->temp_low_cnt = 0;
				}
				dev_err(battery->dev,
					"%s: recovery count = %d\n",
					__func__, battery->temp_recover_cnt);
			}
		} else if (battery->health != POWER_SUPPLY_HEALTH_OVERHEAT) {
			if (battery->temp_high_cnt <
			    battery->pdata->temp_check_count) {
				battery->temp_high_cnt++;
				battery->temp_highlimit_cnt = 0;
				battery->temp_low_cnt = 0;
				battery->temp_recover_cnt = 0;
			}
			dev_err(battery->dev,
				"%s: high count = %d\n",
				__func__, battery->temp_high_cnt);
		}
	} else if ((temp_value <= battery->temp_high_recovery) &&
				(temp_value >= battery->temp_low_recovery)) {
		if (battery->health == POWER_SUPPLY_HEALTH_OVERHEAT ||
			battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT ||
		    battery->health == POWER_SUPPLY_HEALTH_COLD) {
			if (battery->temp_recover_cnt <
			    battery->pdata->temp_check_count) {
				battery->temp_recover_cnt++;
				battery->temp_highlimit_cnt = 0;
				battery->temp_high_cnt = 0;
				battery->temp_low_cnt = 0;
			}
			dev_err(battery->dev,
				"%s: recovery count = %d\n",
				__func__, battery->temp_recover_cnt);
		}
	} else if (temp_value <= battery->temp_low_threshold) {
		if (battery->health != POWER_SUPPLY_HEALTH_COLD) {
			if (battery->temp_low_cnt <
			    battery->pdata->temp_check_count) {
				battery->temp_low_cnt++;
				battery->temp_highlimit_cnt = 0;
				battery->temp_high_cnt = 0;
				battery->temp_recover_cnt = 0;
			}
			dev_err(battery->dev,
				"%s: low count = %d\n",
				__func__, battery->temp_low_cnt);
		}
	} else {
		battery->temp_highlimit_cnt = 0;
		battery->temp_high_cnt = 0;
		battery->temp_low_cnt = 0;
		battery->temp_recover_cnt = 0;
	}

	if (battery->temp_highlimit_cnt >=
	    battery->pdata->temp_check_count) {
		battery->health = POWER_SUPPLY_HEALTH_OVERHEATLIMIT;
		battery->temp_highlimit_cnt = 0;
	} else if (battery->temp_high_cnt >=
		battery->pdata->temp_check_count) {
		battery->health = POWER_SUPPLY_HEALTH_OVERHEAT;
		battery->temp_high_cnt = 0;
	} else if (battery->temp_low_cnt >=
		battery->pdata->temp_check_count) {
		battery->health = POWER_SUPPLY_HEALTH_COLD;
		battery->temp_low_cnt = 0;
	} else if (battery->temp_recover_cnt >=
		 battery->pdata->temp_check_count) {
		if (battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT) {
			battery->health = POWER_SUPPLY_HEALTH_OVERHEAT;
		} else {
			battery->health = POWER_SUPPLY_HEALTH_GOOD;
		}
		battery->temp_recover_cnt = 0;
	}
	if (pre_health != battery->health) {
		battery->health_change = true;
		dev_info(battery->dev, "%s, health_change true\n", __func__);
#if defined(CONFIG_BATTERY_SBM_DATA)
		sec_bat_add_sbm_data(battery, SBM_DATA_TEMP);
#endif
	} else {
		battery->health_change = false;
	}

	if ((battery->health == POWER_SUPPLY_HEALTH_OVERHEAT) ||
		(battery->health == POWER_SUPPLY_HEALTH_COLD) ||
		(battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT)) {
		union power_supply_propval val = {0, };
		if (battery->health_change) {
			battery->is_abnormal_temp = true;
			if (is_wireless_type(battery->cable_type)) {
				val.intval = battery->health;
				psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_PROP_HEALTH, val);
			}
			dev_info(battery->dev,
				"%s: Unsafe Temperature\n", __func__);
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_UNSAFE_TEMPERATURE]++;
#endif

			if (battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT) {
				/* change charging current to battery (default 0mA) */
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_BUCK_OFF);
				if (is_hv_afc_wire_type(battery->cable_type) && !battery->vbus_limit) {
#if defined(CONFIG_MUIC_HV) || defined(CONFIG_SUPPORT_QC30)
					muic_afc_set_voltage(SEC_INPUT_VOLTAGE_0V);
#endif
					battery->vbus_limit = true;
					pr_info("%s: Set AFC TA to 0V\n", __func__);
				}
			} else {
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
			}

			psy_do_property(battery->pdata->charger_name, get,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, val);
			sec_bat_set_swelling_float_voltage(battery, val.intval);

			return false;
		}

		psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, val);
		sec_bat_set_swelling_float_voltage(battery, val.intval);
	} else {
		/* if recovered from not charging */
		if ((battery->health == POWER_SUPPLY_HEALTH_GOOD) &&
			(battery->status ==
			 POWER_SUPPLY_STATUS_NOT_CHARGING)) {
			battery->is_abnormal_temp = false;
			dev_info(battery->dev,
					"%s: Safe Temperature\n", __func__);
			if (battery->capacity >= 100)
				sec_bat_set_charging_status(battery,
						POWER_SUPPLY_STATUS_FULL);
			else	/* Normal Charging */
				sec_bat_set_charging_status(battery,
						POWER_SUPPLY_STATUS_CHARGING);
#if defined(CONFIG_BATTERY_SWELLING)
			if ((temp_value >= battery->pdata->swelling_high_temp_recov) ||
				(temp_value <= battery->pdata->swelling_low_temp_recov_2nd)) {
				pr_info("%s: swelling mode start. stop charging\n", __func__);
				battery->swelling_mode = SWELLING_MODE_CHARGING;
				battery->swelling_full_check_cnt = 0;
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
			} else {
				union power_supply_propval val = {0, };
				/* restore 4.4V float voltage */
				val.intval = battery->pdata->swelling_normal_float_voltage;
				psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_VOLTAGE_MAX, val);
				/* turn on charger by cable type */
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
			}
#else
			/* turn on charger by cable type */
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
#endif
			return false;
		}
	}
	return true;
}

static bool sec_bat_check_fullcharged_condition(
					struct sec_battery_info *battery)
{
	int full_check_type = SEC_BATTERY_FULLCHARGED_NONE;

	if (battery->charging_mode == SEC_BATTERY_CHARGING_1ST)
		full_check_type = battery->pdata->full_check_type;
	else
		full_check_type = battery->pdata->full_check_type_2nd;

	switch (full_check_type) {
	case SEC_BATTERY_FULLCHARGED_ADC:
	case SEC_BATTERY_FULLCHARGED_FG_CURRENT:
	case SEC_BATTERY_FULLCHARGED_SOC:
	case SEC_BATTERY_FULLCHARGED_CHGGPIO:
	case SEC_BATTERY_FULLCHARGED_CHGPSY:
		break;

	/* If these is NOT full check type or NONE full check type,
	 * it is full-charged
	 */
	case SEC_BATTERY_FULLCHARGED_CHGINT:
	case SEC_BATTERY_FULLCHARGED_TIME:
	case SEC_BATTERY_FULLCHARGED_NONE:
	default:
		return true;
		break;
	}

	if (battery->pdata->full_condition_type &
		SEC_BATTERY_FULL_CONDITION_SOC) {
		if (battery->capacity <
			battery->pdata->full_condition_soc) {
			dev_dbg(battery->dev,
				"%s: Not enough SOC (%d%%)\n",
				__func__, battery->capacity);
			return false;
		}
	}

	if (battery->pdata->full_condition_type &
		SEC_BATTERY_FULL_CONDITION_VCELL) {
		if (battery->voltage_now <
			battery->pdata->full_condition_vcell) {
			dev_dbg(battery->dev,
				"%s: Not enough VCELL (%dmV)\n",
				__func__, battery->voltage_now);
			return false;
		}
	}

	if (battery->pdata->full_condition_type &
		SEC_BATTERY_FULL_CONDITION_AVGVCELL) {
		if (battery->voltage_avg <
			battery->pdata->full_condition_avgvcell) {
			dev_dbg(battery->dev,
				"%s: Not enough AVGVCELL (%dmV)\n",
				__func__, battery->voltage_avg);
			return false;
		}
	}

	if (battery->pdata->full_condition_type &
		SEC_BATTERY_FULL_CONDITION_OCV) {
		if (battery->voltage_ocv <
			battery->pdata->full_condition_ocv) {
			dev_dbg(battery->dev,
				"%s: Not enough OCV (%dmV)\n",
				__func__, battery->voltage_ocv);
			return false;
		}
	}

	return true;
}

static void sec_bat_do_test_function(
		struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	switch (battery->test_mode) {
		case 1:
			if (battery->status == POWER_SUPPLY_STATUS_CHARGING) {
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
				sec_bat_set_charging_status(battery,
						POWER_SUPPLY_STATUS_DISCHARGING);
			}
			break;
		case 2:
			if(battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
				psy_do_property(battery->pdata->charger_name, get,
						POWER_SUPPLY_PROP_STATUS, value);
				sec_bat_set_charging_status(battery, value.intval);
			}
			battery->test_mode = 0;
			break;
		case 3: // clear temp block
			battery->health = POWER_SUPPLY_HEALTH_GOOD;
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_DISCHARGING);
			break;
		case 4:
			if(battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
				psy_do_property(battery->pdata->charger_name, get,
						POWER_SUPPLY_PROP_STATUS, value);
				sec_bat_set_charging_status(battery, value.intval);
			}
			break;
		default:
			pr_info("%s: error test: unknown state\n", __func__);
			break;
	}
}

static bool sec_bat_time_management(
				struct sec_battery_info *battery)
{
	struct timespec ts = {0, };
	unsigned long charging_time;

	if (battery->charging_start_time == 0 || !battery->safety_timer_set) {
		dev_dbg(battery->dev,
			"%s: Charging Disabled\n", __func__);
		return true;
	}

	get_monotonic_boottime(&ts);

	if (ts.tv_sec >= battery->charging_start_time) {
		charging_time = ts.tv_sec - battery->charging_start_time;
	} else {
		charging_time = 0xFFFFFFFF - battery->charging_start_time
			+ ts.tv_sec;
	}

	battery->charging_passed_time = charging_time;

	switch (battery->status) {
	case POWER_SUPPLY_STATUS_FULL:
		if (battery->is_recharging && (battery->expired_time == 0)) {
			dev_info(battery->dev,
				"%s: Recharging Timer Expired\n", __func__);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			battery->health = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
			battery->is_recharging = false;
			if (sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF)) {
				dev_err(battery->dev,
					"%s: Fail to Set Charger\n", __func__);
				return true;
			}

			return false;
		}
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if ((battery->pdata->full_condition_type &
		     SEC_BATTERY_FULL_CONDITION_NOTIMEFULL) &&
		    (battery->is_recharging && (battery->expired_time == 0))) {
			dev_info(battery->dev,
			"%s: Recharging Timer Expired\n", __func__);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			battery->health = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
			battery->is_recharging = false;
			if (sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF)) {
				dev_err(battery->dev,
					"%s: Fail to Set Charger\n", __func__);
				return true;
			}
			return false;
		} else if (!battery->is_recharging &&
			   (battery->expired_time == 0)) {
			dev_info(battery->dev,
				"%s: Charging Timer Expired\n", __func__);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			battery->health = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_SAFETY_TIMER]++;
#endif
			if (sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF)) {
				dev_err(battery->dev,
					"%s: Fail to Set Charger\n", __func__);
				return true;
			}
#if 0
			if (charging_total_time == battery->pdata->hv_charging_total_time)
				battery->cisd.data[CISD_DATA_SAFETY_TIMER_3]++;
			else if (charging_total_time == battery->pdata->normal_charging_total_time)
				battery->cisd.data[CISD_DATA_SAFETY_TIMER_5]++;
			else
				battery->cisd.data[CISD_DATA_SAFETY_TIMER_10]++;
#endif
			return false;
		}
		break;
	default:
		dev_err(battery->dev,
			"%s: Undefine Battery Status\n", __func__);
		return true;
	}

	return true;
}

static bool sec_bat_check_fullcharged(
				struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };
	int current_adc = 0;
	int full_check_type = SEC_BATTERY_FULLCHARGED_NONE;
	bool ret = false;
	int err = 0;

	if (!sec_bat_check_fullcharged_condition(battery))
		goto not_full_charged;

	if (battery->charging_mode == SEC_BATTERY_CHARGING_1ST)
		full_check_type = battery->pdata->full_check_type;
	else
		full_check_type = battery->pdata->full_check_type_2nd;

	switch (full_check_type) {
	case SEC_BATTERY_FULLCHARGED_ADC:
		current_adc =
			sec_bat_get_adc_data(battery,
			SEC_BAT_ADC_CHANNEL_FULL_CHECK,
			battery->pdata->adc_check_count);

		dev_dbg(battery->dev,
			"%s: Current ADC (%d)\n",
			__func__, current_adc);

		if (current_adc < 0)
			break;
		battery->current_adc = current_adc;

		if (battery->current_adc <
			(battery->charging_mode ==
			SEC_BATTERY_CHARGING_1ST ?
			battery->pdata->full_check_current_1st :
			battery->pdata->full_check_current_2nd)) {
			battery->full_check_cnt++;
			dev_dbg(battery->dev,
				"%s: Full Check ADC (%d)\n",
				__func__,
				battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_FG_CURRENT:
		if ((battery->current_now > 0 && battery->current_now <
			battery->pdata->full_check_current_1st) &&
			(battery->current_avg > 0 && battery->current_avg <
			(battery->charging_mode ==
			SEC_BATTERY_CHARGING_1ST ?
			battery->pdata->full_check_current_1st :
			battery->pdata->full_check_current_2nd))) {
				battery->full_check_cnt++;
				dev_dbg(battery->dev,
				"%s: Full Check Current (%d)\n",
				__func__,
				battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_TIME:
		if ((battery->charging_mode ==
			SEC_BATTERY_CHARGING_2ND ?
			(battery->charging_passed_time -
			battery->charging_fullcharged_time) :
			battery->charging_passed_time) >
			(battery->charging_mode ==
			SEC_BATTERY_CHARGING_1ST ?
			battery->pdata->full_check_current_1st :
			battery->pdata->full_check_current_2nd)) {
			battery->full_check_cnt++;
			dev_dbg(battery->dev,
				"%s: Full Check Time (%d)\n",
				__func__,
				battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_SOC:
		if (battery->capacity <=
			(battery->charging_mode ==
			SEC_BATTERY_CHARGING_1ST ?
			battery->pdata->full_check_current_1st :
			battery->pdata->full_check_current_2nd)) {
			battery->full_check_cnt++;
			dev_dbg(battery->dev,
				"%s: Full Check SOC (%d)\n",
				__func__,
				battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_CHGGPIO:
		err = gpio_request(
			battery->pdata->chg_gpio_full_check,
			"GPIO_CHG_FULL");
		if (err) {
			dev_err(battery->dev,
				"%s: Error in Request of GPIO\n", __func__);
			break;
		}
		if (!(gpio_get_value_cansleep(
			battery->pdata->chg_gpio_full_check) ^
			!battery->pdata->chg_polarity_full_check)) {
			battery->full_check_cnt++;
			dev_dbg(battery->dev,
				"%s: Full Check GPIO (%d)\n",
				__func__, battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		gpio_free(battery->pdata->chg_gpio_full_check);
		break;

	case SEC_BATTERY_FULLCHARGED_CHGINT:
	case SEC_BATTERY_FULLCHARGED_CHGPSY:
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_STATUS, value);

		if (value.intval == POWER_SUPPLY_STATUS_FULL) {
			battery->full_check_cnt++;
			dev_info(battery->dev,
				"%s: Full Check Charger (%d)\n",
				__func__, battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	/* If these is NOT full check type or NONE full check type,
	 * it is full-charged
	 */
	case SEC_BATTERY_FULLCHARGED_NONE:
		battery->full_check_cnt = 0;
		ret = true;
		break;
	default:
		dev_err(battery->dev,
			"%s: Invalid Full Check\n", __func__);
		break;
	}

	if (battery->full_check_cnt >=
		battery->pdata->full_check_count) {
		battery->full_check_cnt = 0;
		ret = true;
	}

not_full_charged:
	return ret;
}

static void sec_bat_do_fullcharged(
				struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	/* To let charger/fuel gauge know the full status,
	 * set status before calling sec_bat_set_charge()
	 */
#if defined(CONFIG_BATTERY_CISD)
	struct timespec now_ts;

	if (battery->status != POWER_SUPPLY_STATUS_FULL)
		battery->cisd.data[CISD_DATA_FULL_COUNT]++;
#endif
	sec_bat_set_charging_status(battery,
			POWER_SUPPLY_STATUS_FULL);

	if (battery->charging_mode == SEC_BATTERY_CHARGING_1ST &&
		battery->pdata->full_check_type_2nd != SEC_BATTERY_FULLCHARGED_NONE) {
		battery->charging_mode = SEC_BATTERY_CHARGING_2ND;
		battery->charging_fullcharged_time = battery->charging_passed_time;
		value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;
		psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
		sec_bat_set_charging_current(battery);
		sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
#if defined(CONFIG_BATTERY_SBM_DATA)
		sec_bat_add_sbm_data(battery, SBM_DATA_FULL_1ST);
#endif		
	} else {
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
#if defined(CONFIG_BATTERY_CISD)
		now_ts = ktime_to_timespec(ktime_get_boottime());
		if (!battery->is_recharging) {
			battery->cisd.charging_end_time = now_ts.tv_sec;
		}
		if (battery->siop_level == 100) {
			dev_info(battery->dev, "%s: cisd - leakage EFGH start(%ld)\n", __func__, ((unsigned long)now_ts.tv_sec));
			battery->cisd.state &= ~(CISD_STATE_LEAK_E|CISD_STATE_LEAK_F|CISD_STATE_LEAK_G);
			battery->cisd.charging_end_time_2 = now_ts.tv_sec;
			battery->cisd.recharge_count_2 = 0;
		} else {
			battery->cisd.state &= ~(CISD_STATE_LEAK_E|CISD_STATE_LEAK_F|CISD_STATE_LEAK_G);
			battery->cisd.recharge_count_2 = 0;
			battery->cisd.charging_end_time_2 = 0;
		}
#endif
		battery->is_recharging = false;

		if (!battery->wdt_kick_disable) {
			pr_info("%s: wdt kick enable -> Charger Off, %d\n",
					__func__, battery->wdt_kick_disable);
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
		} else {
			pr_info("%s: wdt kick disabled -> skip charger off, %d\n",
					__func__, battery->wdt_kick_disable);
		}

#if defined(CONFIG_BATTERY_AGE_FORECAST)
		sec_bat_aging_check(battery);
#endif

		value.intval = POWER_SUPPLY_STATUS_FULL;
		psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_STATUS, value);
#if defined(CONFIG_BATTERY_SBM_DATA)
		sec_bat_add_sbm_data(battery, SBM_DATA_FULL_2ND);
#endif		
	}

	/* platform can NOT get information of battery
	 * because wakeup time is too short to check uevent
	 * To make sure that target is wakeup if full-charged,
	 * activated wake lock in a few seconds
	 */
	if (battery->pdata->polling_type == SEC_BATTERY_MONITOR_ALARM)
		wake_lock_timeout(&battery->vbus_wake_lock, HZ * 10);
}

static bool sec_bat_fullcharged_check(
				struct sec_battery_info *battery)
{
	if ((battery->charging_mode == SEC_BATTERY_CHARGING_NONE) ||
		(battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
		dev_dbg(battery->dev,
			"%s: No Need to Check Full-Charged\n", __func__);
		return true;
	}

	if (sec_bat_check_fullcharged(battery)) {
		union power_supply_propval value = {0, };
		if (battery->capacity < 100) {
			battery->full_check_cnt = battery->pdata->full_check_count;
		} else {
			sec_bat_do_fullcharged(battery);
		}

		/* update capacity max */
		value.intval = battery->capacity;
		psy_do_property(battery->pdata->fuelgauge_name, set,
				POWER_SUPPLY_PROP_CHARGE_FULL, value);
		pr_info("%s : forced full-charged sequence for the capacity(%d)\n",
				__func__, battery->capacity);
	}

	dev_info(battery->dev,
		"%s: Charging Mode : %s\n", __func__,
		battery->is_recharging ?
		sec_bat_charging_mode_str[SEC_BATTERY_CHARGING_RECHARGING] :
		sec_bat_charging_mode_str[battery->charging_mode]);

	return true;
}

static void sec_bat_get_temperature_info(
				struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	switch (battery->pdata->thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_TEMP, value);
		battery->temperature = value.intval;

		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
		battery->temper_amb = value.intval;
		break;
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		if (battery->pdata->get_temperature_callback) {
			battery->pdata->get_temperature_callback(
				POWER_SUPPLY_PROP_TEMP, &value);
			battery->temperature = value.intval;
			psy_do_property(battery->pdata->fuelgauge_name, set,
				POWER_SUPPLY_PROP_TEMP, value);

			battery->pdata->get_temperature_callback(
				POWER_SUPPLY_PROP_TEMP_AMBIENT, &value);
			battery->temper_amb = value.intval;
			psy_do_property(battery->pdata->fuelgauge_name, set,
				POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
		}
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_TEMP, &value);
		battery->temperature = value.intval;

		sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT, &value);
		battery->temper_amb = value.intval;

		if (battery->pdata->usb_thermal_source) {
			sec_bat_get_value_by_adc(battery,
				   SEC_BAT_ADC_CHANNEL_USB_TEMP, &value);
			battery->usb_temp = value.intval;
			if (battery->vbus_limit && battery->usb_temp <= battery->temp_highlimit_recovery)
				battery->vbus_limit = false;
		}

		if (battery->pdata->chg_thermal_source) {
			sec_bat_get_value_by_adc(battery,
				   SEC_BAT_ADC_CHANNEL_CHG_TEMP, &value);
			battery->chg_temp = value.intval;
		}

		if (battery->pdata->wpc_thermal_source) {
			sec_bat_get_value_by_adc(battery,
				SEC_BAT_ADC_CHANNEL_WPC_TEMP, &value);
			battery->wpc_temp = value.intval;
			battery->coil_temp = value.intval;
		}

		if (battery->pdata->slave_thermal_source) {
			sec_bat_get_value_by_adc(battery,
				SEC_BAT_ADC_CHANNEL_SLAVE_CHG_TEMP, &value);
			battery->slave_chg_temp = value.intval;

			/* set temperature */
			value.intval = ((battery->slave_chg_temp) << 16) | (battery->chg_temp);
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_TEMP, value);
		}

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
		if (battery->temperature_test_battery > -300 && battery->temperature_test_battery < 3000) {
			pr_info("%s : battery temperature test %d\n", __func__, battery->temperature_test_battery);
			battery->temperature = battery->temperature_test_battery;
		}
		if (battery->temperature_test_usb > -300 && battery->temperature_test_usb < 3000) {
			pr_info("%s : usb temperature test %d\n", __func__, battery->temperature_test_usb);
			battery->usb_temp = battery->temperature_test_usb;
		}
		if (battery->temperature_test_wpc > -300 && battery->temperature_test_wpc < 3000) {
			pr_info("%s : wpc temperature test %d\n", __func__, battery->temperature_test_wpc);
			battery->wpc_temp = battery->temperature_test_wpc;
			battery->coil_temp = battery->temperature_test_wpc;
		}
		if (battery->temperature_test_chg > -300 && battery->temperature_test_chg < 3000) {
			pr_info("%s : chg temperature test %d\n", __func__, battery->temperature_test_chg);
			battery->chg_temp = battery->temperature_test_chg;
		}
#endif

#if defined(CONFIG_SEC_FACTORY)
		if (battery->temperature <= (-200)) {
			value.intval = battery->usb_temp;
		} else {
			value.intval = battery->temperature;
		}
#else
		value.intval = battery->temperature;
#endif
		psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_TEMP, value);

		psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_TEMP_AMBIENT, value);

		break;
	default:
		break;
	}
#if defined(CONFIG_BATTERY_CISD)
	if (battery->temperature > battery->cisd.data[CISD_DATA_BATT_TEMP_MAX])
		battery->cisd.data[CISD_DATA_BATT_TEMP_MAX] = battery->temperature;
	if (battery->temperature < battery->cisd.data[CISD_DATA_BATT_TEMP_MIN])
		battery->cisd.data[CISD_DATA_BATT_TEMP_MIN] = battery->temperature;

	if (battery->chg_temp > battery->cisd.data[CISD_DATA_CHG_TEMP_MAX])
		battery->cisd.data[CISD_DATA_CHG_TEMP_MAX] = battery->chg_temp;
	if (battery->chg_temp < battery->cisd.data[CISD_DATA_CHG_TEMP_MIN])
		battery->cisd.data[CISD_DATA_CHG_TEMP_MIN] = battery->chg_temp;

	if (battery->wpc_temp > battery->cisd.data[CISD_DATA_WPC_TEMP_MAX])
		battery->cisd.data[CISD_DATA_WPC_TEMP_MAX] = battery->wpc_temp;
	if (battery->wpc_temp < battery->cisd.data[CISD_DATA_WPC_TEMP_MIN])
		battery->cisd.data[CISD_DATA_WPC_TEMP_MIN] = battery->wpc_temp;
#endif
}

static void sec_bat_get_battery_info(
				struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	battery->voltage_now = value.intval;

	value.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	battery->voltage_avg = value.intval;

	/* Do not call it to reduce time after cable_work, this funtion call FG full log*/
	if (!(battery->current_event & SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL)) {
		value.intval = SEC_BATTERY_VOLTAGE_OCV;
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
		battery->voltage_ocv = value.intval;
	}

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value);
	battery->current_now = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CURRENT_AVG, value);
	battery->current_avg = value.intval;

	/* input current limit in charger */
	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CURRENT_MAX, value);
	battery->current_max = value.intval;

	/* check abnormal status for wireless charging */
	if (is_wireless_type(battery->cable_type)) {
		value.intval = battery->capacity;
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
	}

	sec_bat_get_temperature_info(battery);

	/* To get SOC value (NOT raw SOC), need to reset value */
	value.intval = 0;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
	/* if the battery status was full, and SOC wasn't 100% yet,
		then ignore FG SOC, and report (previous SOC +1)% */
	battery->capacity = value.intval;

		dev_info(battery->dev,
			"%s:Vnow(%dmV),Inow(%dmA),Imax(%dmA),Ichg(%dmA),SOC(%d%%),Tbat(%d), Tusb(%d), Tchg(%d),Twpc(%d)"
			"\n", __func__,
			battery->voltage_now, battery->current_now,
			battery->current_max, battery->charging_current,
			battery->capacity, battery->temperature, 
			battery->usb_temp,battery->chg_temp, battery->wpc_temp
		);
	dev_dbg(battery->dev,
		"%s,Vavg(%dmV),Vocv(%dmV),Tamb(%d),"
		"Iavg(%dmA),Iadc(%d)\n",
		battery->present ? "Connected" : "Disconnected",
		battery->voltage_avg, battery->voltage_ocv,
		battery->temper_amb,
		battery->current_avg, battery->current_adc);

#if defined(CONFIG_SEC_DEBUG_EXTRA_INFO)
	sec_debug_set_extra_info_batt(battery->capacity, battery->voltage_avg, battery->temperature, battery->current_avg);
#endif
}

static void sec_bat_polling_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(
		work, struct sec_battery_info, polling_work.work);

	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	dev_dbg(battery->dev, "%s: Activated\n", __func__);
}

static void sec_bat_program_alarm(
				struct sec_battery_info *battery, int seconds)
{
	alarm_start(&battery->polling_alarm,
		    ktime_add(battery->last_poll_time, ktime_set(seconds, 0)));
}

static unsigned int sec_bat_get_polling_time(
	struct sec_battery_info *battery)
{
	if (battery->status ==
		POWER_SUPPLY_STATUS_FULL)
		battery->polling_time =
			battery->pdata->polling_time[
			POWER_SUPPLY_STATUS_CHARGING];
	else
		battery->polling_time =
			battery->pdata->polling_time[
			battery->status];

	battery->polling_short = true;

	switch (battery->status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (battery->polling_in_sleep)
			battery->polling_short = false;
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		if (battery->polling_in_sleep && (battery->ps_enable != true)) {
			battery->polling_time =
				battery->pdata->polling_time[
					SEC_BATTERY_POLLING_TIME_SLEEP];
		} else
			battery->polling_time =
				battery->pdata->polling_time[
				battery->status];
		if (!battery->wc_enable) {
			battery->polling_time = battery->pdata->polling_time[
					SEC_BATTERY_POLLING_TIME_CHARGING];
			pr_info("%s: wc_enable is false, polling time is 30sec\n", __func__);
		}
		battery->polling_short = false;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		if (battery->polling_in_sleep) {
			if (!(battery->pdata->full_condition_type &
				SEC_BATTERY_FULL_CONDITION_NOSLEEPINFULL) &&
				battery->charging_mode ==
				SEC_BATTERY_CHARGING_NONE) {
				battery->polling_time =
					battery->pdata->polling_time[
						SEC_BATTERY_POLLING_TIME_SLEEP];
			}
			battery->polling_short = false;
		} else {
			if (battery->charging_mode ==
				SEC_BATTERY_CHARGING_NONE)
				battery->polling_short = false;
		}
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if ((battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE ||
			(battery->health == POWER_SUPPLY_HEALTH_UNDERVOLTAGE)) &&
			(battery->health_check_count > 0)) {
			battery->health_check_count--;
			battery->polling_time = 1;
			battery->polling_short = false;
		}
		break;
	}

	if (battery->polling_short)
		return battery->pdata->polling_time[
			SEC_BATTERY_POLLING_TIME_BASIC];
	/* set polling time to 46s to reduce current noise on wc */
	else if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS &&
			battery->status == POWER_SUPPLY_STATUS_CHARGING)
		battery->polling_time = 46;

	return battery->polling_time;
}

static bool sec_bat_is_short_polling(
	struct sec_battery_info *battery)
{
	/* Change the full and short monitoring sequence
	 * Originally, full monitoring was the last time of polling_count
	 * But change full monitoring to first time
	 * because temperature check is too late
	 */
	if (!battery->polling_short || battery->polling_count == 1)
		return false;
	else
		return true;
}

static void sec_bat_update_polling_count(
	struct sec_battery_info *battery)
{
	/* do NOT change polling count in sleep
	 * even though it is short polling
	 * to keep polling count along sleep/wakeup
	 */
	if (battery->polling_short && battery->polling_in_sleep)
		return;

	if (battery->polling_short &&
		((battery->polling_time /
		battery->pdata->polling_time[
		SEC_BATTERY_POLLING_TIME_BASIC])
		> battery->polling_count))
		battery->polling_count++;
	else
		battery->polling_count = 1;	/* initial value = 1 */
}

static void sec_bat_set_polling(
	struct sec_battery_info *battery)
{
	unsigned int polling_time_temp = 0;

	dev_dbg(battery->dev, "%s: Start\n", __func__);

	polling_time_temp = sec_bat_get_polling_time(battery);

	dev_info(battery->dev,
		"%s: Status:%s, Sleep:%s, Charging:%s, Short Poll:%s\n",
		__func__, sec_bat_status_str[battery->status],
		battery->polling_in_sleep ? "Yes" : "No",
		(battery->charging_mode ==
		SEC_BATTERY_CHARGING_NONE) ? "No" : "Yes",
		battery->polling_short ? "Yes" : "No");
	dev_info(battery->dev,
		"%s: Polling time %d/%d sec.\n", __func__,
		battery->polling_short ?
		(polling_time_temp * battery->polling_count) :
		polling_time_temp, battery->polling_time);

	/* To sync with log above,
	 * change polling count after log is displayed
	 * Do NOT update polling count in initial monitor
	 */
	if (!battery->pdata->monitor_initial_count)
		sec_bat_update_polling_count(battery);
	else
		dev_dbg(battery->dev,
			"%s: Initial monitor %d times left.\n", __func__,
			battery->pdata->monitor_initial_count);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		if (battery->pdata->monitor_initial_count) {
			battery->pdata->monitor_initial_count--;
			schedule_delayed_work(&battery->polling_work, HZ);
		} else
			schedule_delayed_work(&battery->polling_work,
				polling_time_temp * HZ);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		battery->last_poll_time = ktime_get_boottime();

		if (battery->pdata->monitor_initial_count) {
			battery->pdata->monitor_initial_count--;
			sec_bat_program_alarm(battery, 1);
		} else
			sec_bat_program_alarm(battery, polling_time_temp);
		break;
	case SEC_BATTERY_MONITOR_TIMER:
		break;
	default:
		break;
	}
	dev_dbg(battery->dev, "%s: End\n", __func__);
}

/* OTG during HV wireless charging or sleep mode have 4.5W normal wireless charging UI */
static bool sec_bat_hv_wc_normal_mode_check(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
	if (value.intval || sleep_mode) {
		pr_info("%s: otg(%d), sleep_mode(%d)\n", __func__, value.intval, sleep_mode);
		return true;
	}
	return false;
}

#if defined(CONFIG_BATTERY_SWELLING)
static void sec_bat_swelling_fullcharged_check(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_STATUS, value);

	if (value.intval == POWER_SUPPLY_STATUS_FULL) {
		battery->swelling_full_check_cnt++;
		pr_info("%s: Swelling mode full-charged check (%d)\n",
			__func__, battery->swelling_full_check_cnt);
	} else
		battery->swelling_full_check_cnt = 0;

	if (battery->swelling_full_check_cnt >=
		battery->pdata->full_check_count) {
		battery->swelling_full_check_cnt = 0;
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		battery->is_recharging = false;
		battery->swelling_mode = SWELLING_MODE_FULL;
		sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
		battery->expired_time = battery->pdata->expired_time;
		battery->prev_safety_time = 0;
	}
}
#endif

#if defined(CONFIG_CALC_TIME_TO_FULL)
static void sec_bat_calc_time_to_full(struct sec_battery_info * battery)
{
	if (battery->status == POWER_SUPPLY_STATUS_CHARGING ||
		(battery->status == POWER_SUPPLY_STATUS_FULL && battery->capacity != 100)) {
		union power_supply_propval value = {0, };
		int charge = 0;

		if (is_hv_wire_12v_type(battery->cable_type) ||
				battery->max_charge_power >= (battery->pdata->pd_charging_charge_power + 5000)) { /* 20000mW */
			charge = battery->pdata->ttf_hv_12v_charge_current;
		} else if (is_hv_wire_type(battery->cable_type) ||
			   /* if max_charge_power could support over than max_charging_current, calculate based on ttf_hv_charge_current */
			   battery->max_charge_power >= (battery->pdata->max_charging_current * 5)) {
			charge = battery->pdata->ttf_hv_charge_current;
		} else if (is_hv_wireless_type(battery->cable_type)) {
			if (sec_bat_hv_wc_normal_mode_check(battery))
				charge = battery->pdata->ttf_wireless_charge_current;
			 else 
				charge = battery->pdata->ttf_hv_wireless_charge_current;
		} else if (is_nv_wireless_type(battery->cable_type)) {
			charge = battery->pdata->ttf_wireless_charge_current;
		} else {
			charge = battery->max_charge_power / 5;
		}
		value.intval = charge;
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_TIME_TO_FULL_NOW, value);
		dev_info(battery->dev, "%s: T: %5d sec, passed time: %5ld, current: %d\n",
				__func__, value.intval, battery->charging_passed_time, charge);
		battery->timetofull = value.intval;
	} else {
		battery->timetofull = -1;
	}
}

static void sec_bat_time_to_full_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, timetofull_work.work);
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CURRENT_MAX, value);
	battery->current_max = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value);
	battery->current_now = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CURRENT_AVG, value);
	battery->current_avg = value.intval;

	sec_bat_calc_time_to_full(battery);
	battery->complete_timetofull = true;
	dev_info(battery->dev, "%s: \n",__func__);
	if (battery->voltage_now > 0)
		battery->voltage_now--;
	power_supply_changed(&battery->psy_bat);
}
#endif

extern bool get_usb_enumeration_state(void);
/* To disaply slow charging when usb charging 100MA*/
static void sec_bat_check_slowcharging_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, slowcharging_work.work);

	if (battery->cable_type == SEC_BATTERY_CABLE_USB) {
		if (!get_usb_enumeration_state() &&
			(battery->current_event & SEC_BAT_CURRENT_EVENT_USB_100MA)) {
			sec_bat_set_misc_event(battery, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE, 0);
			battery->max_charge_power = battery->input_voltage * battery->current_max;
		}
	}
	dev_info(battery->dev, "%s: \n",__func__);
}

static void sec_bat_wc_cv_mode_check(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	pr_info("%s: battery->wc_cv_mode = %d \n", __func__, battery->wc_cv_mode);

	if (battery->capacity >= battery->pdata->wireless_cc_cv) {
		pr_info("%s: 4.5W WC Changed Vout input current limit\n", __func__);
		battery->wc_cv_mode = true;
		sec_bat_set_charging_current(battery);
		value.intval = WIRELESS_VOUT_CC_CV_VOUT; // 5.5V
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		value.intval = WIRELESS_VRECT_ADJ_ROOM_5; // 80mv
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		if ((battery->cable_type == SEC_BATTERY_CABLE_WIRELESS ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK_TA)) {
			value.intval = WIRELESS_CLAMP_ENABLE;
			psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		}
		/* Change FOD values for CV mode */
		value.intval = POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE;
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_PROP_STATUS, value);
	}
}

static void sec_bat_siop_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, siop_work.work);
#if 0
#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
	if (battery->siop_event == SIOP_EVENT_WPC_CALL_START) {
		value.intval = battery->siop_event;
		pr_info("%s : set current by siop event(%d)\n",__func__, battery->siop_event);
		psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, value);
		if (battery->capacity >= battery->pdata->wireless_cc_cv) {
			pr_info("%s SIOP EVENT CALL START.\n", __func__);
			value.intval = WIRELESS_VOUT_CV_CALL;
			psy_do_property(battery->pdata->wireless_charger_name, set,
							POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		} else {
			pr_info("%s SIOP EVENT CALL START.\n", __func__);
			value.intval = WIRELESS_VOUT_CC_CALL;
			psy_do_property(battery->pdata->wireless_charger_name, set,
							POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		}
		wake_unlock(&battery->siop_wake_lock);
		return;
	} else if (battery->siop_event == SIOP_EVENT_WPC_CALL_END) {
		battery->siop_event = 0;
		value.intval = WIRELESS_VOUT_5V;
		psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
	}
#endif
#endif
	pr_info("%s : set current by siop level(%d)\n",__func__, battery->siop_level);

	sec_bat_set_charging_current(battery);
	wake_unlock(&battery->siop_wake_lock);
}

static void sec_bat_siop_level_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
			struct sec_battery_info, siop_level_work.work);

	if (battery->siop_prev_event != battery->siop_event) {
		wake_unlock(&battery->siop_level_wake_lock);
		return;
	}

	if (is_wireless_type(battery->cable_type)) {
		queue_delayed_work(battery->monitor_wqueue, &battery->siop_work, 0);
	}
	else
		queue_delayed_work(battery->monitor_wqueue, &battery->siop_work, 0);

	wake_lock(&battery->siop_wake_lock);
	wake_unlock(&battery->siop_level_wake_lock);
}

static void sec_bat_wc_headroom_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
			struct sec_battery_info, wc_headroom_work.work);
	union power_supply_propval value = {0, };

	/* The default headroom is high, because initial wireless charging state is unstable.
		After 10sec wireless charging, however, recover headroom level to avoid chipset damage */
	if (battery->wc_status != SEC_WIRELESS_PAD_NONE) {
		/* When the capacity is higher than 99, and the device is in 5V wireless charging state,
			then Vrect headroom has to be headroom_2.
			Refer to the sec_bat_siop_work function. */
		if (battery->capacity < 99 && battery->status != POWER_SUPPLY_STATUS_FULL) {
			if (is_nv_wireless_type(battery->cable_type)) {
				if (battery->capacity < battery->pdata->wireless_cc_cv)
					value.intval = WIRELESS_VRECT_ADJ_ROOM_4; /* WPC 4.5W, Vrect Room 30mV */
				else
					value.intval = WIRELESS_VRECT_ADJ_ROOM_5; /* WPC 4.5W, Vrect Room 80mV */
			} else if (is_hv_wireless_type(battery->cable_type)) {
				value.intval = WIRELESS_VRECT_ADJ_ROOM_5;
			} else {
				value.intval = WIRELESS_VRECT_ADJ_OFF;
			}
			psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			pr_info("%s: Changed Vrect adjustment from Rx activation(10seconds)", __func__);
		}
		if (is_nv_wireless_type(battery->cable_type))
			sec_bat_wc_cv_mode_check(battery);
	}
	wake_unlock(&battery->wc_headroom_wake_lock);
}

static void sec_bat_siop_event_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
			struct sec_battery_info, siop_event_work.work);

	union power_supply_propval value = {0, };

	if (battery->cable_type != SEC_BATTERY_CABLE_WIRELESS_PACK &&
		battery->cable_type != SEC_BATTERY_CABLE_WIRELESS_PACK_TA) {
		battery->siop_prev_event = battery->siop_event;
		wake_unlock(&battery->siop_event_wake_lock);
		return;
	}

	if (!(battery->siop_prev_event & SIOP_EVENT_WPC_CALL) && (battery->siop_event & SIOP_EVENT_WPC_CALL)) {
		pr_info("%s : set current by siop event(%d)\n",__func__, battery->siop_event);
		if (battery->capacity >= battery->pdata->wireless_cc_cv) {
			pr_info("%s SIOP EVENT CALL CV START.\n", __func__);
			value.intval = WIRELESS_VOUT_CV_CALL;
		} else {
			pr_info("%s SIOP EVENT CALL CC START.\n", __func__);
			value.intval = WIRELESS_VOUT_CC_CALL;
		}
		/* set current first */
		sec_bat_set_charging_current(battery);
		psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
	} else if ((battery->siop_prev_event & SIOP_EVENT_WPC_CALL) && !(battery->siop_event & SIOP_EVENT_WPC_CALL)) {
		if (battery->wc_cv_mode)
			value.intval = WIRELESS_VOUT_CC_CV_VOUT; // 5.5V
		else
			value.intval = WIRELESS_VOUT_5V;
		psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		wake_lock(&battery->siop_level_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->siop_level_work, 0);
	}
	battery->siop_prev_event = battery->siop_event;
	wake_unlock(&battery->siop_event_wake_lock);
}

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
static void sec_bat_fw_update_work(struct sec_battery_info *battery, int mode)
{
	union power_supply_propval value = {0, };

	dev_info(battery->dev, "%s \n", __func__);

	wake_lock_timeout(&battery->vbus_wake_lock, HZ * 10);

	switch (mode) {
		case SEC_WIRELESS_RX_SDCARD_MODE:
		case SEC_WIRELESS_RX_BUILT_IN_MODE:
			value.intval = mode;
			psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL, value);
			break;
		case SEC_WIRELESS_TX_ON_MODE:
			value.intval = true;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL, value);

			value.intval = mode;
			psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL, value);

			break;
		case SEC_WIRELESS_TX_OFF_MODE:
			value.intval = false;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL, value);
			break;
		default:
			break;
	}
}

static void sec_bat_fw_init_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, fw_init_work.work);

	union power_supply_propval value = {0, };
	int uno_status = 0, wpc_det = 0;

	dev_info(battery->dev, "%s \n", __func__);

	wpc_det = gpio_get_value(battery->pdata->wpc_det);

	pr_info("%s wpc_det = %d \n", __func__, wpc_det);

	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL, value);
	uno_status = value.intval;
	pr_info("%s uno = %d \n", __func__, uno_status);

	if (!uno_status && !wpc_det) {
		pr_info("%s uno on \n", __func__);
		value.intval = true;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL, value);
	}

	value.intval = SEC_WIRELESS_RX_INIT;
	psy_do_property(battery->pdata->wireless_charger_name, set,
		POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL, value);

	if (!uno_status && !wpc_det) {
		pr_info("%s uno off \n", __func__);
		value.intval = false;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL, value);
	}
}
#endif
#if defined(CONFIG_UPDATE_BATTERY_DATA)
static int sec_bat_parse_dt(struct device *dev, struct sec_battery_info *battery);
static void sec_bat_update_data_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, batt_data_work.work);

	sec_battery_update_data(battery->data_path);
	wake_unlock(&battery->batt_data_wake_lock);
}
#endif

static void sec_bat_misc_event_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, misc_event_work.work);
	int xor_misc_event = battery->prev_misc_event ^ battery->misc_event;

	if ((xor_misc_event & (BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE |
		BATT_MISC_EVENT_HICCUP_TYPE)) &&
		(battery->cable_type == SEC_BATTERY_CABLE_NONE)) {
		if (battery->misc_event & (BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE |
			BATT_MISC_EVENT_HICCUP_TYPE)) {
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_BUCK_OFF);
		} else if (battery->prev_misc_event & (BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE |
			BATT_MISC_EVENT_HICCUP_TYPE)) {
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
		}
	}

	pr_info("%s: change misc event(0x%x --> 0x%x)\n",
		__func__, battery->prev_misc_event, battery->misc_event);
	battery->prev_misc_event = battery->misc_event;
	wake_unlock(&battery->misc_event_wake_lock);

	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
}

static void sec_bat_calculate_safety_time(struct sec_battery_info *battery)
{
	unsigned long long expired_time = battery->expired_time;
	struct timespec ts = {0, };
	int curr = 0;
	int input_power = battery->current_max * battery->input_voltage * 1000;
	int charging_power = battery->charging_current * (battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv);
	static int discharging_cnt = 0;

	if (battery->current_avg < 0) {
		discharging_cnt++;
	} else {
		discharging_cnt = 0;
	}

	if (discharging_cnt >= 5) {
		battery->expired_time = battery->pdata->expired_time;
		battery->prev_safety_time = 0;
		pr_info("%s : SAFETY TIME RESET! DISCHARGING CNT(%d)\n",
			__func__, discharging_cnt);
		discharging_cnt = 0;
		return;
	} else if (battery->lcd_status && battery->stop_timer) {
		battery->prev_safety_time = 0;
		return;
	}

	get_monotonic_boottime(&ts);

	if (battery->prev_safety_time == 0) {
		battery->prev_safety_time = ts.tv_sec;
	}

	if (input_power > charging_power) {
		curr = battery->charging_current;
	} else {
		curr = input_power / (battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv);
		curr = (curr * 9) / 10;
	}

	if (battery->lcd_status && !battery->stop_timer) {
		battery->stop_timer = true;
	} else if (!battery->lcd_status && battery->stop_timer) {
		battery->stop_timer = false;
	}

	pr_info("%s : EXPIRED_TIME(%llu), IP(%d), CP(%d), CURR(%d), STANDARD(%d)\n",
		__func__, expired_time, input_power, charging_power, curr, battery->pdata->standard_curr);

	if (curr == 0)
		return;

	expired_time = (expired_time * battery->pdata->standard_curr) / curr;

	pr_info("%s : CAL_EXPIRED_TIME(%llu) TIME NOW(%ld) TIME PREV(%ld)\n", __func__, expired_time, ts.tv_sec, battery->prev_safety_time);

	if (expired_time <= ((ts.tv_sec - battery->prev_safety_time) * 1000))
		expired_time = 0;
	else
		expired_time -= ((ts.tv_sec - battery->prev_safety_time) * 1000);

	battery->cal_safety_time = expired_time;
	expired_time = (expired_time * curr) / battery->pdata->standard_curr;

	battery->expired_time = expired_time;
	battery->prev_safety_time = ts.tv_sec;
	pr_info("%s : REMAIN_TIME(%ld) CAL_REMAIN_TIME(%ld)\n", __func__, battery->expired_time, battery->cal_safety_time);
}

static void sec_bat_monitor_work(
				struct work_struct *work)
{
	struct sec_battery_info *battery =
		container_of(work, struct sec_battery_info,
		monitor_work.work);
	static struct timespec old_ts = {0, };
	struct timespec c_ts = {0, };

	dev_dbg(battery->dev, "%s: Start\n", __func__);
	c_ts = ktime_to_timespec(ktime_get_boottime());

	if (!battery->wc_enable) {
		pr_info("%s: wc_enable(%d), cnt(%d)\n",
			__func__, battery->wc_enable, battery->wc_enable_cnt);
		if (battery->wc_enable_cnt > battery->wc_enable_cnt_value) {
			battery->wc_enable = true;
			battery->wc_enable_cnt = 0;
			if (battery->pdata->wpc_en) {
				gpio_direction_output(battery->pdata->wpc_en, 0);
				pr_info("%s: WC CONTROL: Enable", __func__);
			}
			pr_info("%s: wpc_en(%d)\n",
				__func__, gpio_get_value(battery->pdata->wpc_en));
		}
		battery->wc_enable_cnt++;
	}

	/* monitor once after wakeup */
	if (battery->polling_in_sleep) {
		battery->polling_in_sleep = false;
		if ((battery->status == POWER_SUPPLY_STATUS_DISCHARGING) &&
			(battery->ps_enable != true)) {
			if ((unsigned long)(c_ts.tv_sec - old_ts.tv_sec) < 10 * 60) {
				union power_supply_propval value = {0, };

					psy_do_property(battery->pdata->fuelgauge_name, get,
						POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
					battery->voltage_now = value.intval;

					value.intval = 0;
					psy_do_property(battery->pdata->fuelgauge_name, get,
							POWER_SUPPLY_PROP_CAPACITY, value);
					battery->capacity = value.intval;

					sec_bat_get_temperature_info(battery);
#if defined(CONFIG_BATTERY_CISD)
					sec_bat_cisd_check(battery);
#endif
					power_supply_changed(&battery->psy_bat);
					pr_info("Skip monitor work(%ld, Vnow:%d(mV), SoC:%d(%%), Tbat:%d(0.1'C))\n",
						c_ts.tv_sec - old_ts.tv_sec, battery->voltage_now, battery->capacity, battery->temperature);

				goto skip_monitor;
			}
		}
	}
	/* update last monitor time */
	old_ts = c_ts;

	sec_bat_get_battery_info(battery);
#if defined(CONFIG_BATTERY_CISD)
	sec_bat_cisd_check(battery);
#endif

#if defined(CONFIG_STEP_CHARGING)
	sec_bat_check_step_charging(battery);
#endif
#if defined(CONFIG_CALC_TIME_TO_FULL)
	/* time to full check */
	sec_bat_calc_time_to_full(battery);
#endif

	/* 0. test mode */
	if (battery->test_mode) {
		dev_err(battery->dev, "%s: Test Mode\n", __func__);
		sec_bat_do_test_function(battery);
		if (battery->test_mode != 0)
			goto continue_monitor;
	}

	/* 1. battery check */
	if (!sec_bat_battery_cable_check(battery))
		goto continue_monitor;

	/* 2. voltage check */
	if (!sec_bat_voltage_check(battery))
		goto continue_monitor;

	/* monitor short routine in initial monitor */
	if (battery->pdata->monitor_initial_count || sec_bat_is_short_polling(battery))
		goto skip_current_monitor;

	/* 3. time management */
	if (!sec_bat_time_management(battery))
		goto continue_monitor;

	/* 4. temperature check */
	if (!sec_bat_temperature_check(battery))
		goto continue_monitor;

#if defined(CONFIG_BATTERY_SWELLING)
	sec_bat_swelling_check(battery);

	if ((battery->swelling_mode == SWELLING_MODE_CHARGING || battery->swelling_mode == SWELLING_MODE_FULL) &&
		(!battery->charging_block))
		sec_bat_swelling_fullcharged_check(battery);
	else
		sec_bat_fullcharged_check(battery);
#else
	/* 5. full charging check */
	sec_bat_fullcharged_check(battery);
#endif /* CONFIG_BATTERY_SWELLING */

	/* 6. additional check */
	if (battery->pdata->monitor_additional_check)
		battery->pdata->monitor_additional_check();

#if defined(CONFIG_BATTERY_SBM_DATA)
	sec_bat_add_sbm_data(battery, SBM_DATA_COMMON_INFO);
#endif

	if ((battery->cable_type == SEC_BATTERY_CABLE_WIRELESS ||
		battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND ||
		battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK_TA) &&
		!battery->wc_cv_mode && battery->charging_passed_time > 10)
		sec_bat_wc_cv_mode_check(battery);

continue_monitor:
	/* clear HEATING_CONTROL*/
	sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL);

	/* calculate safety time */
	if (!battery->charging_block)
		sec_bat_calculate_safety_time(battery);

	/* set charging current */
	sec_bat_set_charging_current(battery);

skip_current_monitor:
	dev_info(battery->dev,
		"%s: HLT(%d) HLR(%d) HT(%d), HR(%d), LT(%d), LR(%d), lpcharge(%d)\n",
		__func__, battery->temp_highlimit_threshold, battery->temp_highlimit_recovery,
		battery->temp_high_threshold, battery->temp_high_recovery,
		battery->temp_low_threshold, battery->temp_low_recovery, lpcharge);

	dev_info(battery->dev,
		 "%s: Status(%s), mode(%s), Health(%s), Cable(%s, %s, %d, %d), level(%d%%), slate_mode(%d), store_mode(%d)"
#if defined(CONFIG_AFC_CHARGER_MODE)
		", HV(%s), sleep_mode(%d)"
#endif
#if defined(CONFIG_BATTERY_AGE_FORECAST)
#if defined(CONFIG_BATTERY_AGE_FORECAST_DETACHABLE)
		", Cycle(%dw)"
#else
		", Cycle(%d)"
#endif
#endif
		 "\n", __func__,
		 sec_bat_status_str[battery->status],
		 sec_bat_charging_mode_str[battery->charging_mode],
		 sec_bat_health_str[battery->health],
		 sec_cable_type[battery->cable_type],
		 sec_cable_type[battery->wire_status],
		 battery->muic_cable_type,
		 battery->pd_usb_attached,
		 battery->siop_level,
		 battery->slate_mode,
		 battery->store_mode
#if defined(CONFIG_AFC_CHARGER_MODE)
		, battery->hv_chg_name, sleep_mode
#endif
#if defined(CONFIG_BATTERY_AGE_FORECAST)
		, battery->batt_cycle
#endif
		 );
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	dev_info(battery->dev,
			"%s: battery->stability_test(%d), battery->eng_not_full_status(%d)\n",
			__func__, battery->stability_test, battery->eng_not_full_status);
#endif
#if defined(CONFIG_SEC_FACTORY)
	if ((battery->cable_type != SEC_BATTERY_CABLE_NONE) && (battery->cable_type != SEC_BATTERY_CABLE_OTG)) {
#else
	if (battery->store_mode) {
#endif
		dev_info(battery->dev,
			 "%s: @battery->capacity = (%d), battery->status= (%d), battery->store_mode=(%d)\n",
			 __func__, battery->capacity, battery->status, battery->store_mode);

		if (battery->capacity >= battery->pdata->store_mode_max) {
			int chg_mode = battery->misc_event &
				(BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE | BATT_MISC_EVENT_HICCUP_TYPE) ?
				SEC_BAT_CHG_MODE_BUCK_OFF : SEC_BAT_CHG_MODE_CHARGING_OFF;
			/* to discharge the battery, off buck */
			if (battery->capacity > battery->pdata->store_mode_max)
				chg_mode = SEC_BAT_CHG_MODE_BUCK_OFF;

			sec_bat_set_charging_status(battery,
						    POWER_SUPPLY_STATUS_DISCHARGING);
			sec_bat_set_charge(battery, chg_mode);
		}

		if ((battery->capacity <= battery->pdata->store_mode_min) && (battery->status == POWER_SUPPLY_STATUS_DISCHARGING)) {
			sec_bat_set_charging_status(battery,
						    POWER_SUPPLY_STATUS_CHARGING);
			sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING);
		}
	}
	power_supply_changed(&battery->psy_bat);

skip_monitor:
	sec_bat_set_polling(battery);

	if (battery->capacity <= 0 || battery->health_change)
		wake_lock_timeout(&battery->monitor_wake_lock, HZ * 5);
	else
		wake_unlock(&battery->monitor_wake_lock);

	dev_dbg(battery->dev, "%s: End\n", __func__);

	return;
}

static enum alarmtimer_restart sec_bat_alarm(
	struct alarm *alarm, ktime_t now)
{
	struct sec_battery_info *battery = container_of(alarm,
				struct sec_battery_info, polling_alarm);

	dev_dbg(battery->dev,
			"%s\n", __func__);

	/* In wake up, monitor work will be queued in complete function
	 * To avoid duplicated queuing of monitor work,
	 * do NOT queue monitor work in wake up by polling alarm
	 */
	if (!battery->polling_in_sleep) {
		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		dev_dbg(battery->dev, "%s: Activated\n", __func__);
	}

	return ALARMTIMER_NORESTART;
}

static void sec_bat_check_input_voltage(struct sec_battery_info *battery)
{
	unsigned int voltage = 0;
	int input_current = battery->pdata->charging_current[battery->cable_type].input_current_limit;

	if (battery->cable_type == SEC_BATTERY_CABLE_PDIC) {
		battery->max_charge_power = battery->pd_max_charge_power;
		return;
	}
	else if (is_hv_wire_12v_type(battery->cable_type))
		voltage = SEC_INPUT_VOLTAGE_12V;
	else if (is_hv_wire_9v_type(battery->cable_type))
		voltage = SEC_INPUT_VOLTAGE_9V;
	else if (is_hv_wireless_type(battery->cable_type))
		voltage = SEC_INPUT_VOLTAGE_10V;
	else
		voltage = SEC_INPUT_VOLTAGE_5V;

	battery->input_voltage = voltage;
	battery->charge_power = voltage * input_current;
#if !defined(CONFIG_SEC_FACTORY)
	if (battery->charge_power > battery->max_charge_power)
#endif
	battery->max_charge_power = battery->charge_power;

	pr_info("%s: battery->input_voltage : %dV, %dmW, %dmW)\n", __func__,
		battery->input_voltage, battery->charge_power, battery->max_charge_power);
}

static void sec_bat_cable_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, cable_work.work);
	union power_supply_propval val = {0, };
	int current_cable_type = SEC_BATTERY_CABLE_NONE;

	dev_info(battery->dev, "%s: Start\n", __func__);
	sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL,
				  SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL);
#if defined(CONFIG_CCIC_NOTIFIER)
	if (battery->wire_status == SEC_BATTERY_CABLE_PDIC) {
		sec_bat_get_input_current_in_power_list(battery);
		sec_bat_get_charging_current_in_power_list(battery);
	}
#endif

	if (battery->wc_status && battery->wc_enable) {
		int wireless_current, wire_current;

		if (battery->wc_status == SEC_WIRELESS_PAD_WPC)
			current_cable_type = SEC_BATTERY_CABLE_WIRELESS;
		else if (battery->wc_status == SEC_WIRELESS_PAD_WPC_HV)
			current_cable_type = SEC_BATTERY_CABLE_HV_WIRELESS;
		else if (battery->wc_status == SEC_WIRELESS_PAD_WPC_PACK)
			current_cable_type = SEC_BATTERY_CABLE_WIRELESS_PACK;
		else if (battery->wc_status == SEC_WIRELESS_PAD_WPC_PACK_TA)
			current_cable_type = SEC_BATTERY_CABLE_WIRELESS_PACK_TA;
		else if (battery->wc_status == SEC_WIRELESS_PAD_WPC_STAND)
			current_cable_type = SEC_BATTERY_CABLE_WIRELESS_STAND;
		else if (battery->wc_status == SEC_WIRELESS_PAD_WPC_STAND_HV)
			current_cable_type = SEC_BATTERY_CABLE_WIRELESS_HV_STAND;
		else if (battery->wc_status == SEC_WIRELESS_PAD_VEHICLE)
			current_cable_type = SEC_BATTERY_CABLE_WIRELESS_VEHICLE;
		else if (battery->wc_status == SEC_WIRELESS_PAD_VEHICLE_HV)
			current_cable_type = SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE;
		else if (battery->wc_status == SEC_WIRELESS_PAD_PREPARE_HV)
			current_cable_type = SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV;
		else
			current_cable_type = SEC_BATTERY_CABLE_PMA_WIRELESS;

		if (battery->wire_status != SEC_BATTERY_CABLE_NONE) {
			wireless_current = battery->pdata->charging_current[current_cable_type].input_current_limit;
			wireless_current = wireless_current * (is_hv_wireless_type(current_cable_type) ?
				SEC_INPUT_VOLTAGE_9V : SEC_INPUT_VOLTAGE_5V);
			if (battery->wire_status == SEC_BATTERY_CABLE_PDIC) {
				if (wireless_current < battery->pd_max_charge_power)
					current_cable_type = battery->wire_status;
			} else {
				wire_current = (battery->wire_status == SEC_BATTERY_CABLE_PREPARE_TA ?
					battery->pdata->charging_current[SEC_BATTERY_CABLE_TA].input_current_limit :
					battery->pdata->charging_current[battery->wire_status].input_current_limit);

				wire_current = wire_current * (is_hv_wire_type(battery->wire_status) ?
					(battery->wire_status == SEC_BATTERY_CABLE_12V_TA ? SEC_INPUT_VOLTAGE_12V : SEC_INPUT_VOLTAGE_9V)
					: SEC_INPUT_VOLTAGE_5V);
				pr_info("%s: wl_cur(%d), wr_cur(%d), wc_cable_type(%d), wire_cable_type(%d)\n",
					__func__, wireless_current, wire_current, current_cable_type, battery->wire_status);

				if (wireless_current < wire_current)
					current_cable_type = battery->wire_status;
			}
		}
	} else
		current_cable_type = battery->wire_status;

	if (current_cable_type == SEC_BATTERY_CABLE_PDIC &&
		battery->cable_type == SEC_BATTERY_CABLE_PDIC &&
		!battery->slate_mode) {
		cancel_delayed_work(&battery->afc_work);
		wake_unlock(&battery->afc_wake_lock);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_AFC);
		battery->aicl_current = 0;
		sec_bat_set_charging_current(battery);
		goto end_of_cable_work;
	}

	/* to clear this value when cable type switched without dettach */
	if ((is_wired_type(battery->cable_type) && is_wireless_type(current_cable_type)) ||
	    (is_wireless_type(battery->cable_type) && is_wired_type(current_cable_type)))
		battery->max_charge_power = 0;

	if ((current_cable_type == battery->cable_type) && !battery->slate_mode) {
		dev_dbg(battery->dev,
				"%s: Cable is NOT Changed(%d)\n",
				__func__, battery->cable_type);
		/* Do NOT activate cable work for NOT changed */
		goto end_of_cable_work;
	}

#if defined(CONFIG_BATTERY_SWELLING)
	if ((current_cable_type == SEC_BATTERY_CABLE_NONE) ||
		(battery->cable_type == SEC_BATTERY_CABLE_NONE && battery->swelling_mode == SWELLING_MODE_NONE)) {
		battery->swelling_mode = SWELLING_MODE_NONE;
		/* restore 4.4V float voltage */
		val.intval = battery->pdata->swelling_normal_float_voltage;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, val);
		pr_info("%s: float voltage = %d\n", __func__, val.intval);
	} else {
		pr_info("%s: skip  float_voltage setting, swelling_mode(%d)\n",
			__func__, battery->swelling_mode);
	}
#endif

#if defined(CONFIG_CALC_TIME_TO_FULL)
	battery->complete_timetofull = false;
#endif
	battery->cable_type = current_cable_type;

	if (is_wireless_type(battery->cable_type)) {
		power_supply_changed(&battery->psy_bat);
		/* After 10sec wireless charging, Vrect headroom has to be reduced */
		wake_lock(&battery->wc_headroom_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->wc_headroom_work,
			msecs_to_jiffies(10000));
	}

	/* get vout type of rx */
	psy_do_property(battery->pdata->wireless_charger_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
	battery->wpc_vout_level = val.intval;

	if (battery->pdata->check_cable_result_callback)
		battery->pdata->check_cable_result_callback(battery->cable_type);
	/* platform can NOT get information of cable connection
	 * because wakeup time is too short to check uevent
	 * To make sure that target is wakeup
	 * if cable is connected and disconnected,
	 * activated wake lock in a few seconds
	 */
	wake_lock_timeout(&battery->vbus_wake_lock, HZ * 10);

	if (battery->cable_type == SEC_BATTERY_CABLE_NONE ||
		((battery->pdata->cable_check_type &
		SEC_BATTERY_CABLE_CHECK_NOINCOMPATIBLECHARGE) &&
		battery->cable_type == SEC_BATTERY_CABLE_UNKNOWN)) {
		/* initialize all status */
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		battery->vbus_chg_by_siop = false;
		battery->is_recharging = false;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.charging_end_time = 0;
		battery->cisd.recharge_count = 0;
		battery->cisd.charging_end_time_2 = 0;
		battery->cisd.recharge_count_2 = 0;
		battery->cisd.ab_vbat_check_count = 0;
		battery->cisd.state &= ~CISD_STATE_OVER_VOLTAGE;
#endif
		battery->input_voltage = 0;
		battery->charge_power = 0;
		battery->max_charge_power = 0;
		battery->pd_max_charge_power = 0;
		sec_bat_set_charging_status(battery,
				POWER_SUPPLY_STATUS_DISCHARGING);
		battery->chg_limit = false;
		battery->mix_limit = false;
		battery->chg_limit_recovery_cable = SEC_BATTERY_CABLE_NONE;
		battery->wc_heating_start_time = 0;
		battery->health = POWER_SUPPLY_HEALTH_GOOD;
		cancel_delayed_work(&battery->afc_work);
		wake_unlock(&battery->afc_wake_lock);
		sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_USB,
				USB_CURRENT_HIGH_SPEED, USB_CURRENT_HIGH_SPEED);
		sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_TA,
				RP_CURRENT_DEFAULT_IN, RP_CURRENT_DEFAULT_OUT);
		/* usb default current is 100mA before configured*/
		sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_USB_100MA,
					  (SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE |
					   SEC_BAT_CURRENT_EVENT_AFC |
					   SEC_BAT_CURRENT_EVENT_USB_SUPER |
					   SEC_BAT_CURRENT_EVENT_USB_100MA |
					   SEC_BAT_CURRENT_EVENT_VBAT_OVP |
					   SEC_BAT_CURRENT_EVENT_VSYS_OVP));
#if defined(CONFIG_CALC_TIME_TO_FULL)
		cancel_delayed_work(&battery->timetofull_work);
#endif
		cancel_delayed_work(&battery->slowcharging_work);
		battery->wc_cv_mode = false;
		battery->is_sysovlo = false;
		battery->is_vbatovlo = false;
		battery->is_abnormal_temp = false;

		if (sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF))
			goto end_of_cable_work;
	} else if(battery->slate_mode) {
		dev_info(battery->dev,
			"%s:slate mode on\n",__func__);
		battery->is_recharging = false;
		battery->cable_type = SEC_BATTERY_CABLE_NONE;
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		battery->health = POWER_SUPPLY_HEALTH_GOOD;
		sec_bat_set_charging_status(battery,
			POWER_SUPPLY_STATUS_DISCHARGING);
		sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_BUCK_OFF);
	} else {
#if defined(CONFIG_EN_OOPS)
		val.intval = battery->cable_type;
		psy_do_property(battery->pdata->fuelgauge_name, set,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, val);
#endif
		/* Do NOT display the charging icon when OTG or HMT_CONNECTED is enabled */
		if (battery->cable_type == SEC_BATTERY_CABLE_OTG ||
			battery->cable_type == SEC_BATTERY_CABLE_POWER_SHARING) {
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			battery->status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else if (!battery->is_sysovlo && !battery->is_vbatovlo && !battery->is_abnormal_temp &&
				(!battery->charging_block || !battery->swelling_mode)) {
			if (battery->pdata->full_check_type !=
				SEC_BATTERY_FULLCHARGED_NONE)
				battery->charging_mode =
					SEC_BATTERY_CHARGING_1ST;
			else
				battery->charging_mode =
					SEC_BATTERY_CHARGING_2ND;

			if (battery->status == POWER_SUPPLY_STATUS_FULL)
				sec_bat_set_charging_status(battery,
						POWER_SUPPLY_STATUS_FULL);
			else
				sec_bat_set_charging_status(battery,
						POWER_SUPPLY_STATUS_CHARGING);
		}

		if (!battery->is_sysovlo && !battery->is_vbatovlo && !battery->is_abnormal_temp)
			battery->health = POWER_SUPPLY_HEALTH_GOOD;

		if (battery->cable_type == SEC_BATTERY_CABLE_TA ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS ||
			battery->cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS) {
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_AFC, SEC_BAT_CURRENT_EVENT_AFC);
		} else {
			cancel_delayed_work(&battery->afc_work);
			wake_unlock(&battery->afc_wake_lock);
			sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_AFC);
		}

		if (battery->cable_type == SEC_BATTERY_CABLE_OTG ||
			battery->cable_type == SEC_BATTERY_CABLE_POWER_SHARING) {
			if (sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF))
				goto end_of_cable_work;
		} else if (!battery->is_sysovlo && !battery->is_vbatovlo && !battery->is_abnormal_temp &&
				(!battery->charging_block || !battery->swelling_mode)) {
			if (sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING))
				goto end_of_cable_work;
		}

#if defined(CONFIG_CALC_TIME_TO_FULL)
		queue_delayed_work(battery->monitor_wqueue, &battery->timetofull_work,
					msecs_to_jiffies(7000));
#endif
		if (battery->cable_type == SEC_BATTERY_CABLE_USB)
			queue_delayed_work(battery->monitor_wqueue, &battery->slowcharging_work,
						msecs_to_jiffies(3000));
	}

	/* set online(cable type) */
	val.intval = battery->cable_type;
	psy_do_property(battery->pdata->charger_name, set,
		POWER_SUPPLY_PROP_ONLINE, val);
	psy_do_property(battery->pdata->fuelgauge_name, set,
		POWER_SUPPLY_PROP_ONLINE, val);
	/* set charging current */
	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CURRENT_AVG, val);
	battery->aicl_current = 0;
	battery->input_current = val.intval;
	/* to init battery type current when wireless charging -> battery case */
	if (battery->cable_type == SEC_BATTERY_CABLE_NONE)
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, val);
	if (battery->status != POWER_SUPPLY_STATUS_DISCHARGING)
		sec_bat_check_input_voltage(battery);
	sec_bat_set_charging_current(battery);

	/* polling time should be reset when cable is changed
	 * polling_in_sleep should be reset also
	 * before polling time is re-calculated
	 * to prevent from counting 1 for events
	 * right after cable is connected
	 */
	battery->polling_in_sleep = false;
	sec_bat_get_polling_time(battery);

	dev_info(battery->dev,
		"%s: Status:%s, Sleep:%s, Charging:%s, Short Poll:%s\n",
		__func__, sec_bat_status_str[battery->status],
		battery->polling_in_sleep ? "Yes" : "No",
		(battery->charging_mode ==
		SEC_BATTERY_CHARGING_NONE) ? "No" : "Yes",
		battery->polling_short ? "Yes" : "No");
	dev_info(battery->dev,
		"%s: Polling time is reset to %d sec.\n", __func__,
		battery->polling_time);

	battery->polling_count = 1;	/* initial value = 1 */

	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
end_of_cable_work:
	wake_unlock(&battery->cable_wake_lock);
	dev_info(battery->dev, "%s: End\n", __func__);
}

static void sec_bat_afc_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, afc_work.work);
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CURRENT_MAX, value);
	battery->current_max = value.intval;

	if (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC) {
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_AFC);
		if ((battery->wc_status != SEC_WIRELESS_PAD_NONE &&
		     battery->current_max >= battery->pdata->pre_wc_afc_input_current) ||
		    ((battery->cable_type == SEC_BATTERY_CABLE_TA) &&
		     battery->current_max >= battery->pdata->pre_afc_input_current)) {
			sec_bat_set_charging_current(battery);
		}
	}
	wake_unlock(&battery->afc_wake_lock);
}

ssize_t sec_bat_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);
	const ptrdiff_t offset = attr - sec_battery_attrs;
	union power_supply_propval value = {0, };
	int i = 0;
	int ret = 0;

	switch (offset) {
	case BATT_RESET_SOC:
		break;
	case BATT_READ_RAW_SOC:
		{
			value.intval =
				SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_CAPACITY, value);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_READ_ADJ_SOC:
		break;
	case BATT_TYPE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			battery->batt_type);
		break;
	case BATT_VFOCV:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->voltage_ocv);
		break;
	case BATT_VOL_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->inbat_adc);
		break;
	case BATT_VOL_ADC_CAL:
		break;
	case BATT_VOL_AVER:
		break;
	case BATT_VOL_ADC_AVER:
		break;

	case BATT_CURRENT_UA_NOW:
		{
			value.intval = SEC_BATTERY_CURRENT_UA;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_CURRENT_NOW, value);
#if defined(CONFIG_SEC_FACTORY)
			pr_err("%s: batt_current_ua_now (%d)\n",
					__func__, value.intval);
#endif
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_CURRENT_UA_AVG:
		{
			value.intval = SEC_BATTERY_CURRENT_UA;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_CURRENT_AVG, value);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;

	case BATT_FILTER_CFG:
		{
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_FILTER_CFG, value);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
				value.intval);
		}
		break;
	case BATT_TEMP:
		switch (battery->pdata->thermal_source) {
		case SEC_BATTERY_THERMAL_SOURCE_FG:
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_TEMP, value);
			break;
		case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
			if (battery->pdata->get_temperature_callback) {
			battery->pdata->get_temperature_callback(
				POWER_SUPPLY_PROP_TEMP, &value);
			}
			break;
		case SEC_BATTERY_THERMAL_SOURCE_ADC:
			sec_bat_get_value_by_adc(battery,
				SEC_BAT_ADC_CHANNEL_TEMP, &value);
			break;
		default:
			break;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case BATT_TEMP_ADC:
		/*
			If F/G is used for reading the temperature and
			compensation table is used,
			the raw value that isn't compensated can be read by
			POWER_SUPPLY_PROP_TEMP_AMBIENT
		 */
		switch (battery->pdata->thermal_source) {
		case SEC_BATTERY_THERMAL_SOURCE_FG:
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
			battery->temp_adc = value.intval;
			break;
		default:
			break;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->temp_adc);
		break;
	case BATT_TEMP_AVER:
		break;
	case BATT_TEMP_ADC_AVER:
		break;
	case USB_TEMP:
		if (battery->pdata->usb_thermal_source) {
			sec_bat_get_value_by_adc(battery,
					       SEC_BAT_ADC_CHANNEL_USB_TEMP, &value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case USB_TEMP_ADC:
		if (battery->pdata->usb_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       battery->usb_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;		
	case CHG_TEMP:
		if (battery->pdata->chg_thermal_source) {
			sec_bat_get_value_by_adc(battery,
					       SEC_BAT_ADC_CHANNEL_CHG_TEMP, &value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case CHG_TEMP_ADC:
		if (battery->pdata->chg_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       battery->chg_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case SLAVE_CHG_TEMP:
		if (battery->pdata->slave_thermal_source) {
			sec_bat_get_value_by_adc(battery,
						   SEC_BAT_ADC_CHANNEL_SLAVE_CHG_TEMP, &value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					   value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					   0);
		}
		break;
	case SLAVE_CHG_TEMP_ADC:
		if (battery->pdata->slave_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					   battery->slave_chg_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					   0);
		}
		break;
	case BATT_VF_ADC:
		break;
	case BATT_SLATE_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->slate_mode);
		break;

	case BATT_LP_CHARGING:
		if (lpcharge) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       lpcharge ? 1 : 0);
		}
		break;
	case SIOP_ACTIVATED:
		break;
	case SIOP_LEVEL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->siop_level);
		break;
	case SIOP_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->siop_event);
		break;
	case BATT_CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->cable_type);
		break;
	case FG_REG_DUMP:
		break;
	case FG_RESET_CAP:
		break;
	case FG_CAPACITY:
	{
		value.intval =
			SEC_BATTERY_CAPACITY_DESIGNED;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x ",
			value.intval);

		value.intval =
			SEC_BATTERY_CAPACITY_ABSOLUTE;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x ",
			value.intval);

		value.intval =
			SEC_BATTERY_CAPACITY_TEMPERARY;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x ",
			value.intval);

		value.intval =
			SEC_BATTERY_CAPACITY_CURRENT;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x\n",
			value.intval);
	}
		break;
	case FG_ASOC:
		value.intval = -1;
		{
			struct power_supply *psy_fg = NULL;
			psy_fg = get_power_supply_by_name(battery->pdata->fuelgauge_name);
			if (!psy_fg) {
				pr_err("%s: Fail to get psy (%s)\n",
						__func__, battery->pdata->fuelgauge_name);
			} else {
				if (psy_fg->get_property != NULL) {
					ret = psy_fg->get_property(psy_fg,
							POWER_SUPPLY_PROP_ENERGY_FULL, &value);
					if (ret < 0) {
						pr_err("%s: Fail to %s get (%d=>%d)\n",
								__func__, battery->pdata->fuelgauge_name,
								POWER_SUPPLY_PROP_ENERGY_FULL, ret);
					}
				}
			}
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       value.intval);
		break;
	case AUTH:
		break;
	case CHG_CURRENT_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->current_adc);
		break;
	case WC_ADC:
		break;
	case WC_STATUS:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			is_wireless_type(battery->cable_type) ? 1: 0);
		break;
	case WC_ENABLE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->wc_enable);
		break;
	case WC_CONTROL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->wc_enable);
		break;
	case WC_CONTROL_CNT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
		battery->wc_enable_cnt_value);
		break;
	case HV_CHARGER_STATUS:
		{
			int check_val = 0;
			if (is_hv_wire_12v_type(battery->cable_type) ||
				battery->max_charge_power >= HV_CHARGER_STATUS_STANDARD2) /* 20000mW */
				check_val = 2;
			else if (is_hv_wire_type(battery->cable_type) ||
				battery->cable_type == SEC_BATTERY_CABLE_PREPARE_TA ||
				battery->max_charge_power >= HV_CHARGER_STATUS_STANDARD1) /* 12000mW */
				check_val = 1;

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", check_val);
		}
		break;
	case HV_WC_CHARGER_STATUS:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			is_hv_wireless_type(battery->cable_type) ? 1 : 0);
		break;
	case HV_CHARGER_SET:
		break;
	case FACTORY_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->factory_mode);
		break;
	case STORE_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->store_mode);
		break;
	case UPDATE:
		break;
	case TEST_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->test_mode);
		break;

	case BATT_EVENT_CALL:
	case BATT_EVENT_2G_CALL:
	case BATT_EVENT_TALK_GSM:
		break;
	case BATT_EVENT_3G_CALL:
	case BATT_EVENT_TALK_WCDMA:
		break;
	case BATT_EVENT_MUSIC:
		break;
	case BATT_EVENT_VIDEO:
		break;
	case BATT_EVENT_BROWSER:
		break;
	case BATT_EVENT_HOTSPOT:
		break;
	case BATT_EVENT_CAMERA:
		break;
	case BATT_EVENT_CAMCORDER:
		break;
	case BATT_EVENT_DATA_CALL:
		break;
	case BATT_EVENT_WIFI:
		break;
	case BATT_EVENT_WIBRO:
		break;
	case BATT_EVENT_LTE:
		break;
	case BATT_EVENT_LCD:
		break;
	case BATT_EVENT_GPS:
		break;
	case BATT_EVENT:
		break;
	case BATT_TEMP_TABLE:
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"%d %d %d %d %d %d %d %d\n",
			battery->pdata->temp_high_threshold_normal,
			battery->pdata->temp_high_recovery_normal,
			battery->pdata->temp_low_threshold_normal,
			battery->pdata->temp_low_recovery_normal,
			battery->pdata->temp_high_threshold_lpm,
			battery->pdata->temp_high_recovery_lpm,
			battery->pdata->temp_low_threshold_lpm,
			battery->pdata->temp_low_recovery_lpm);
		break;
	case BATT_HIGH_CURRENT_USB:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->is_hc_usb);
		break;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case TEST_CHARGE_CURRENT:
		{
			psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_CURRENT_NOW, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					value.intval);
		}
		break;
#endif
	case SET_STABILITY_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->stability_test);
		break;
	case BATT_CAPACITY_MAX:
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case BATT_INBAT_VOLTAGE:
	case BATT_INBAT_VOLTAGE_OCV:
		if(battery->pdata->support_fgsrc_change == true) {
			int j, k, ocv, ocv_data[10];
			value.intval = 0;
			psy_do_property(battery->pdata->fgsrc_switch_name, set,
					POWER_SUPPLY_PROP_ENERGY_NOW, value);
			for (j = 0; j < 10; j++) {
				mdelay(175);
				psy_do_property(battery->pdata->fuelgauge_name, get,
						POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
				ocv_data[j] = value.intval;
			}
			value.intval = 1;
			psy_do_property(battery->pdata->fgsrc_switch_name, set,
					POWER_SUPPLY_PROP_ENERGY_NOW, value);
			for (j = 1; j < 10; j++) {
				ocv = ocv_data[j];
				k = j;
				while (k > 0 && ocv_data[k-1] > ocv) {
					ocv_data[k] = ocv_data[k-1];
					k--;
				}
				ocv_data[k] = ocv;
			}
			ocv = 0;
			for (j = 2; j < 8; j++) {
				ocv += ocv_data[j];
			}
			ret = ocv / 6;
		} else {
			ret = sec_bat_get_inbat_vol_by_adc(battery);
		}
		dev_info(battery->dev, "in-battery voltage ocv(%d)\n", ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case CHECK_SLAVE_CHG:
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHECK_SLAVE_I2C, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		pr_info("%s : CHECK_SLAVE_CHG=%d\n",__func__,value.intval);
		break;
	case BATT_INBAT_WIRELESS_CS100:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_STATUS, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case HMT_TA_CONNECTED:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->cable_type == SEC_BATTERY_CABLE_HMT_CONNECTED) ? 1 : 0);
		break;
	case HMT_TA_CHARGE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->cable_type == SEC_BATTERY_CABLE_HMT_CHARGE) ? 1 : 0);
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case FG_CYCLE:
		value.intval = SEC_BATTERY_CAPACITY_CYCLE;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
		value.intval = value.intval / 100;
		dev_info(battery->dev, "fg cycle(%d)\n", value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case FG_FULL_VOLTAGE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", battery->pdata->chg_float_voltage);
		break;
	case FG_FULLCAPNOM:
		value.intval =
			SEC_BATTERY_CAPACITY_AGEDCELL;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST_DETACHABLE)
	case BATT_AFTER_MANUFACTURED:
#if defined(CONFIG_ENG_BATTERY_CONCEPT) || defined(CONFIG_SEC_FACTORY)
	case BATTERY_CYCLE:
#endif
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", battery->batt_cycle);
		break;	
#else
	case BATTERY_CYCLE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", battery->batt_cycle);
		break;
#endif
#endif
	case BATT_WPC_TEMP:
		if (battery->pdata->wpc_thermal_source) {
			sec_bat_get_value_by_adc(battery,
				SEC_BAT_ADC_CHANNEL_WPC_TEMP, &value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				0);
		}
		break;
	case BATT_WPC_TEMP_ADC:
		if (battery->pdata->wpc_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->wpc_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				0);
		}
		break;
	case BATT_COIL_TEMP:
		if (battery->pdata->coil_thermal_source) {
			sec_bat_get_value_by_adc(battery,
				SEC_BAT_ADC_CHANNEL_WPC_TEMP, &value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				0);
		}
		break;
	case BATT_COIL_TEMP_ADC:
		if (battery->pdata->coil_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->coil_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				0);
		}
		break;
	case BATT_WIRELESS_MST_SWITCH_TEST:
		value.intval = SEC_WIRELESS_MST_SWITCH_VERIFY;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		pr_info("%s MST switch verify. result: %x\n", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	case BATT_WIRELESS_FIRMWARE_UPDATE:
		value.intval = SEC_WIRELESS_OTP_FIRM_VERIFY;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		pr_info("%s RX firmware verify. result: %d\n", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case OTP_FIRMWARE_RESULT:
		value.intval = SEC_WIRELESS_OTP_FIRM_RESULT;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case WC_IC_GRADE:
		value.intval = SEC_WIRELESS_IC_GRADE;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%x ", value.intval);

		value.intval = SEC_WIRELESS_IC_REVISION;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%x\n", value.intval);
		break;
	case OTP_FIRMWARE_VER_BIN:
		value.intval = SEC_WIRELESS_OTP_FIRM_VER_BIN;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
	case OTP_FIRMWARE_VER:
		value.intval = SEC_WIRELESS_OTP_FIRM_VER;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
	case TX_FIRMWARE_RESULT:
		value.intval = SEC_WIRELESS_TX_FIRM_RESULT;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case TX_FIRMWARE_VER:
		value.intval = SEC_WIRELESS_TX_FIRM_VER;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
	case BATT_TX_STATUS:
		value.intval = SEC_TX_FIRMWARE;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
#endif
	case WC_VOUT:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case WC_VRECT:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_ENERGY_AVG, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case BATT_HV_WIRELESS_STATUS:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case BATT_HV_WIRELESS_PAD_CTRL:
		break;
	case WC_OP_FREQ:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_OP_FREQ, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case WC_CMD_INFO:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_CMD, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x ",
			value.intval);

		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VAL, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x ",
			value.intval);	
		break;
	case BATT_TUNE_FLOAT_VOLTAGE:
		ret = battery->pdata->chg_float_voltage;
		pr_info("%s float voltage = %d mA",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_INPUT_CHARGE_CURRENT:
		ret = battery->pdata->charging_current[i].input_current_limit;
		pr_info("%s input charge current = %d mA",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_FAST_CHARGE_CURRENT:
		ret = battery->pdata->charging_current[i].fast_charging_current;
		pr_info("%s fast charge current = %d mA",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_UI_TERM_CURRENT_1ST:
		ret = battery->pdata->full_check_current_1st;
		pr_info("%s ui term current = %d mA",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_UI_TERM_CURRENT_2ND:
		ret = battery->pdata->full_check_current_2nd;
		pr_info("%s ui term current = %d mA",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_TEMP_HIGH_NORMAL:
		ret = battery->pdata->temp_high_threshold_normal;
		pr_info("%s temp high normal block	= %d ",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_TEMP_HIGH_REC_NORMAL:
		ret = battery->pdata->temp_high_recovery_normal;
		pr_info("%s temp high normal recover  = %d ",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_TEMP_LOW_NORMAL:
		ret = battery->pdata->temp_low_threshold_normal;
		pr_info("%s temp low normal block  = %d ",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_TEMP_LOW_REC_NORMAL:
		ret = battery->pdata->temp_low_recovery_normal;
		pr_info("%s temp low normal recover  = %d ",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_CHG_TEMP_HIGH:
		ret = battery->pdata->chg_high_temp;
		pr_info("%s chg_high_temp = %d ",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_CHG_TEMP_REC:
		ret = battery->pdata->chg_high_temp_recovery;
		pr_info("%s chg_high_temp_recovery	= %d ",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_CHG_LIMMIT_CUR:
		ret = battery->pdata->chg_charging_limit_current;
		pr_info("%s chg_charging_limit_current = %d ",__func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_COIL_TEMP_HIGH:
		break;
	case BATT_TUNE_COIL_TEMP_REC:
		break;
	case BATT_TUNE_COIL_LIMMIT_CUR:
		break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	case BATT_UPDATE_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				battery->data_path ? "OK" : "NOK");
		break;
#endif
	case BATT_MISC_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->misc_event);
		break;
	case BATT_EXT_DEV_CHG:
		break;
	case BATT_WDT_CONTROL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->wdt_kick_disable);
		break;
	case MODE:
		value.strval = NULL;
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			(value.strval) ? value.strval : "master");
		break;
	case CHECK_PS_READY:
#if defined(CONFIG_CCIC_NOTIFIER)
		value.intval = battery->pdic_ps_rdy;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		pr_info("%s : CHECK_PS_READY=%d\n",__func__,value.intval);
#endif
		break;
	case BATT_CHIP_ID:
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHIP_ID, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		break;
	case CISD_FULLCAPREP_MAX:
		{
			union power_supply_propval fullcaprep_val;
			
			fullcaprep_val.intval = SEC_BATTERY_CAPACITY_FULL;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_ENERGY_NOW, fullcaprep_val);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					fullcaprep_val.intval);
		}
		break;
#if defined(CONFIG_BATTERY_CISD)
	case CISD_DATA:
		{
			struct cisd *pcisd = &battery->cisd;

			i+= scnprintf(buf + i, PAGE_SIZE - i, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
				pcisd->data[CISD_DATA_FULL_COUNT], pcisd->data[CISD_DATA_CAP_MAX],
				pcisd->data[CISD_DATA_CAP_MIN], pcisd->data[CISD_DATA_CAP_ONCE],
				pcisd->data[CISD_DATA_LEAKAGE_A], pcisd->data[CISD_DATA_LEAKAGE_B],
				pcisd->data[CISD_DATA_LEAKAGE_C], pcisd->data[CISD_DATA_LEAKAGE_D],
				pcisd->data[CISD_DATA_CAP_PER_TIME], pcisd->data[CISD_DATA_ERRCAP_LOW],
				pcisd->data[CISD_DATA_ERRCAP_HIGH], pcisd->data[CISD_DATA_OVER_VOLTAGE],
				pcisd->data[CISD_DATA_LEAKAGE_E], pcisd->data[CISD_DATA_LEAKAGE_F],
				pcisd->data[CISD_DATA_LEAKAGE_G], pcisd->data[CISD_DATA_RECHARGING_TIME],
				pcisd->data[CISD_DATA_VALERT_COUNT], pcisd->data[CISD_DATA_CYCLE],
				pcisd->data[CISD_DATA_WIRE_COUNT], pcisd->data[CISD_DATA_WIRELESS_COUNT],
				pcisd->data[CISD_DATA_HIGH_TEMP_SWELLING], pcisd->data[CISD_DATA_LOW_TEMP_SWELLING],
				pcisd->data[CISD_DATA_SWELLING_CHARGING_COUNT], pcisd->data[CISD_DATA_SAFETY_TIMER_3],
				pcisd->data[CISD_DATA_SAFETY_TIMER_5], pcisd->data[CISD_DATA_SAFETY_TIMER_10],
				pcisd->data[CISD_DATA_AICL_COUNT],
				pcisd->data[CISD_DATA_BATT_TEMP_MAX], pcisd->data[CISD_DATA_BATT_TEMP_MIN],
				pcisd->data[CISD_DATA_CHG_TEMP_MAX], pcisd->data[CISD_DATA_CHG_TEMP_MIN],
				pcisd->data[CISD_DATA_WPC_TEMP_MAX], pcisd->data[CISD_DATA_WPC_TEMP_MIN],
				pcisd->data[CISD_UNSAFE_VOLTAGE], pcisd->data[CISD_UNSAFE_TEMPERATURE],
				pcisd->data[CISD_SAFETY_TIMER], pcisd->data[CISD_VSYS_OVP],
				pcisd->data[CISD_VBAT_OVP], pcisd->data[CISD_WATER_DETECT],
				pcisd->data[CISD_AFC_FAIL],
				pcisd->cisd_alg_index);
		}
		break;
	case CISD_DATA_JSON:
		{
			struct cisd *pcisd = &battery->cisd;

			i+= scnprintf(buf + i, PAGE_SIZE - i,
				"\"FULL_COUNT\":\"%d\",\"CAP_MAX\":\"%d\",\"CAP_MIN\":\"%d\",\"CAP_ONCE\":\"%d\","
				"\"LEAKAGE_A\":\"%d\",\"LEAKAGE_B\":\"%d\",\"LEAKAGE_C\":\"%d\",\"LEAKAGE_D\":\"%d\","
				"\"CAP_PER_TIME\":\"%d\",\"ERRCAP_LOW\":\"%d\",\"ERRCAP_HIGH\":\"%d\",\"OVER_VOLTAGE\":\"%d\","
				"\"LEAKAGE_E\":\"%d\",\"LEAKAGE_F\":\"%d\",\"LEAKAGE_G\":\"%d\",\"RECHARGING_TIME\":\"%d\","
				"\"VALERT_COUNT\":\"%d\",\"BATT_CYCLE\":\"%d\",\"WIRE_COUNT\":\"%d\",\"WIRELESS_COUNT\":\"%d\","
				"\"HIGH_SWELLING_COUNT\":\"%d\",\"LOW_SWELLING_COUNT\":\"%d\",\"SWELLING_CHARGING\":\"%d\","
				"\"SAFETY_TIMER_3\":\"%d\",\"SAFETY_TIMER_5\":\"%d\",\"SAFETY_TIMER_10\":\"%d\","
				"\"AICL_COUNT\":\"%d\",\"BATT_THM_MAX\":\"%d\",\"BATT_THM_MIN\":\"%d\","
				"\"CHG_THM_MAX\":\"%d\",\"CHG_THM_MIN\":\"%d\",\"WPC_THM_MAX\":\"%d\",\"WPC_THM_MIN\":\"%d\","
				"\"UNSAFE_VOLTAGE\":\"%d\",\"UNSAFE_TEMPERATURE\":\"%d\",\"SAFETY_TIMER\":\"%d\",\"VSYS_OVP\":\"%d\","
				"\"VBAT_OVP\":\"%d\",\"WATER_DETECT\":\"%d\",\"AFC_FAIL\":\"%d\","
				"\"ALG_INDEX\":\"%d\"\n",
				pcisd->data[CISD_DATA_FULL_COUNT], pcisd->data[CISD_DATA_CAP_MAX],
				pcisd->data[CISD_DATA_CAP_MIN], pcisd->data[CISD_DATA_CAP_ONCE],
				pcisd->data[CISD_DATA_LEAKAGE_A], pcisd->data[CISD_DATA_LEAKAGE_B],
				pcisd->data[CISD_DATA_LEAKAGE_C], pcisd->data[CISD_DATA_LEAKAGE_D],
				pcisd->data[CISD_DATA_CAP_PER_TIME], pcisd->data[CISD_DATA_ERRCAP_LOW],
				pcisd->data[CISD_DATA_ERRCAP_HIGH], pcisd->data[CISD_DATA_OVER_VOLTAGE],
				pcisd->data[CISD_DATA_LEAKAGE_E], pcisd->data[CISD_DATA_LEAKAGE_F],
				pcisd->data[CISD_DATA_LEAKAGE_G], pcisd->data[CISD_DATA_RECHARGING_TIME],
				pcisd->data[CISD_DATA_VALERT_COUNT], pcisd->data[CISD_DATA_CYCLE],
				pcisd->data[CISD_DATA_WIRE_COUNT], pcisd->data[CISD_DATA_WIRELESS_COUNT],
				pcisd->data[CISD_DATA_HIGH_TEMP_SWELLING], pcisd->data[CISD_DATA_LOW_TEMP_SWELLING],
				pcisd->data[CISD_DATA_SWELLING_CHARGING_COUNT],	pcisd->data[CISD_DATA_SAFETY_TIMER_3],
				pcisd->data[CISD_DATA_SAFETY_TIMER_5], pcisd->data[CISD_DATA_SAFETY_TIMER_10],
				pcisd->data[CISD_DATA_AICL_COUNT],
				pcisd->data[CISD_DATA_BATT_TEMP_MAX], pcisd->data[CISD_DATA_BATT_TEMP_MIN],
				pcisd->data[CISD_DATA_CHG_TEMP_MAX], pcisd->data[CISD_DATA_CHG_TEMP_MIN],
				pcisd->data[CISD_DATA_WPC_TEMP_MAX], pcisd->data[CISD_DATA_WPC_TEMP_MIN],
				pcisd->data[CISD_UNSAFE_VOLTAGE], pcisd->data[CISD_UNSAFE_TEMPERATURE],
				pcisd->data[CISD_SAFETY_TIMER], pcisd->data[CISD_VSYS_OVP],
				pcisd->data[CISD_VBAT_OVP], pcisd->data[CISD_WATER_DETECT],
				pcisd->data[CISD_AFC_FAIL],
				pcisd->cisd_alg_index);

			pcisd->data[CISD_DATA_CAP_ONCE] = 0;
			pcisd->data[CISD_DATA_CAP_PER_TIME] = 0;
		}
		break;
	case CISD_WIRE_COUNT:
		{
			struct cisd *pcisd = &battery->cisd;
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				pcisd->data[CISD_DATA_WIRE_COUNT]);
		}
		break;
#endif
#if defined(CONFIG_BATTERY_SBM_DATA)
	case SBM_DATA:
		if (battery->sbm_data) {
			sec_bat_get_sbm_data_string(&value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", value.strval);
			battery->sbm_data = false;
		} else {
			i = -EINVAL;
		}
		break;
#endif
	case SAFETY_TIMER_SET:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       battery->safety_timer_set);
		break;
	case BATT_SWELLING_CONTROL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       battery->skip_swelling);
		break;
	case SAFETY_TIMER_INFO:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%ld\n",
			       battery->cal_safety_time);
		break;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case BATT_TEMP_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d %d %d\n",
			battery->temperature_test_battery,
			battery->temperature_test_usb,
			battery->temperature_test_wpc,
			battery->temperature_test_chg);
		break;
#endif
	case BATT_CURRENT_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->current_event); 	
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

void update_external_temp_table(struct sec_battery_info *battery, int temp[])
{
	battery->pdata->temp_high_threshold_normal = temp[0];
	battery->pdata->temp_high_recovery_normal = temp[1];
	battery->pdata->temp_low_threshold_normal = temp[2];
	battery->pdata->temp_low_recovery_normal = temp[3];
	battery->pdata->temp_high_threshold_lpm = temp[4];
	battery->pdata->temp_high_recovery_lpm = temp[5];
	battery->pdata->temp_low_threshold_lpm = temp[6];
	battery->pdata->temp_low_recovery_lpm = temp[7];

}

ssize_t sec_bat_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);
	const ptrdiff_t offset = attr - sec_battery_attrs;
	int ret = -EINVAL;
	int x = 0;
	int t[12];
	int i = 0;
	union power_supply_propval value = {0, };

	switch (offset) {
	case BATT_RESET_SOC:
		/* Do NOT reset fuel gauge in charging mode */
		if ((battery->cable_type == SEC_BATTERY_CABLE_NONE) ||
			battery->is_jig_on) {
			battery->voltage_now = 1234;
			battery->voltage_avg = 1234;
			power_supply_changed(&battery->psy_bat);

			value.intval =
				SEC_FUELGAUGE_CAPACITY_TYPE_RESET;
			psy_do_property(battery->pdata->fuelgauge_name, set,
					POWER_SUPPLY_PROP_CAPACITY, value);
			dev_info(battery->dev,"do reset SOC\n");
			/* update battery info */
			sec_bat_get_battery_info(battery);
		}
		ret = count;
		break;
	case BATT_READ_RAW_SOC:
		break;
	case BATT_READ_ADJ_SOC:
		break;
	case BATT_TYPE:
		strncpy(battery->batt_type, buf, sizeof(battery->batt_type) - 1);
		battery->batt_type[sizeof(battery->batt_type)-1] = '\0';
		ret = count;
		break;
	case BATT_VFOCV:
		break;
	case BATT_VOL_ADC:
		break;
	case BATT_VOL_ADC_CAL:
		break;
	case BATT_VOL_AVER:
		break;
	case BATT_VOL_ADC_AVER:
		break;
	case BATT_CURRENT_UA_NOW:
		break;
	case BATT_CURRENT_UA_AVG:
		break;
	case BATT_FILTER_CFG:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property(battery->pdata->fuelgauge_name, set,
					POWER_SUPPLY_PROP_FILTER_CFG, value);
			ret = count;
		}
		break;
	case BATT_TEMP:
#if defined(CONFIG_ENG_BATTERY_CONCEPT) || defined(CONFIG_SEC_FACTORY)
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev,
				"%s: cooldown mode %s \n", __func__, (x ? "enable" : "disable"));
			if (x == 0)
				battery->cooldown_mode = false;
			else
				battery->cooldown_mode = true;
			ret = count;
		}
#endif
		break;
	case BATT_TEMP_ADC:
		break;
	case BATT_TEMP_AVER:
		break;
	case BATT_TEMP_ADC_AVER:
		break;
	case USB_TEMP:
		break;
	case USB_TEMP_ADC:
		break;		
	case CHG_TEMP:
		break;
	case CHG_TEMP_ADC:
		break;
	case SLAVE_CHG_TEMP:
		break;
	case SLAVE_CHG_TEMP_ADC:
		break;
	case BATT_VF_ADC:
		break;
	case BATT_SLATE_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == battery->slate_mode) {
				dev_info(battery->dev,
					 "%s : skip same slate mode : %d\n", __func__, x);
				return count;
			} else if (x == 1) {
				battery->slate_mode = true;
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SLATE, SEC_BAT_CURRENT_EVENT_SLATE);
				dev_info(battery->dev,
					"%s: enable slate mode : %d\n", __func__, x);
			} else if (x == 0) {
				battery->slate_mode = false;
				sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SLATE);
				dev_info(battery->dev,
					"%s: disable slate mode : %d\n", __func__, x);
			} else {
				dev_info(battery->dev,
					"%s: SLATE MODE unknown command\n", __func__);
				return -EINVAL;
			}
			wake_lock(&battery->cable_wake_lock);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->cable_work, 0);
			ret = count;
		}
		break;
	case BATT_LP_CHARGING:
		break;
	case SIOP_ACTIVATED:
		break;
	case SIOP_LEVEL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev,
					"%s: siop level: %d\n", __func__, x);

			battery->wc_heating_start_time = 0;
			if (x == battery->siop_level) {
				dev_info(battery->dev,
					"%s: skip same siop level: %d\n", __func__, x);
				return count;
			} else if (x >= 0 && x <= 100) {
				battery->siop_level = x;
			} else {
				battery->siop_level = 100;
			}

			if (battery->siop_event == SIOP_EVENT_WPC_CALL_START ||
				battery->siop_event == SIOP_EVENT_WPC_CALL_END)
				return count;

			if (delayed_work_pending(&battery->siop_event_work))
				return count;

			cancel_delayed_work(&battery->siop_work);
			wake_lock(&battery->siop_level_wake_lock);
			queue_delayed_work(battery->monitor_wqueue, &battery->siop_level_work, 0);

			ret = count;
		}
		break;
	case SIOP_EVENT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (battery->pdata->siop_event_check_type & SIOP_EVENT_WPC_CALL) { // To reduce call noise with battery pack
				if (x == SIOP_EVENT_WPC_CALL_START) {
					battery->siop_event |= SIOP_EVENT_WPC_CALL;
					pr_info("%s : WPC Enable & SIOP EVENT CALL START. 0x%x\n",
						__func__, battery->siop_event);
					cancel_delayed_work(&battery->siop_level_work);
					cancel_delayed_work(&battery->siop_work);
					wake_lock(&battery->siop_event_wake_lock);
					queue_delayed_work(battery->monitor_wqueue, &battery->siop_event_work, 0);
				} else if (x == SIOP_EVENT_WPC_CALL_END) {
					battery->siop_event &= ~SIOP_EVENT_WPC_CALL;
					pr_info("%s : WPC Enable & SIOP EVENT CALL END. 0x%x\n",
						__func__, battery->siop_event);
					cancel_delayed_work(&battery->siop_level_work);
					cancel_delayed_work(&battery->siop_work);
					wake_lock(&battery->siop_event_wake_lock);
					queue_delayed_work(battery->monitor_wqueue, &battery->siop_event_work,
							msecs_to_jiffies(5000));
				} else {
					battery->siop_event &= ~SIOP_EVENT_WPC_CALL;
					pr_info("%s : WPC Disable & SIOP EVENT 0x%x\n", __func__, battery->siop_event);
				}
			}
			ret = count;
		}
		break;
	case BATT_CHARGING_SOURCE:
		break;
	case FG_REG_DUMP:
		break;
	case FG_RESET_CAP:
		break;
	case FG_CAPACITY:
		break;
	case FG_ASOC:
		break;
	case AUTH:
		break;
	case CHG_CURRENT_ADC:
		break;
	case WC_ADC:
		break;
	case WC_STATUS:
		break;
	case WC_ENABLE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == 0) {
				battery->wc_enable = false;
				battery->wc_enable_cnt = 0;
			} else if (x == 1) {
				battery->wc_enable = true;
				battery->wc_enable_cnt = 0;
			} else {
				dev_info(battery->dev,
					"%s: WPC ENABLE unknown command\n",
					__func__);
				return -EINVAL;
			}
			wake_lock(&battery->cable_wake_lock);
			queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work, 0);
			ret = count;
		}
		break;
	case WC_CONTROL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == 0) {
				battery->wc_enable = false;
				battery->wc_enable_cnt = 0;
				if (battery->pdata->wpc_en) {
					gpio_direction_output(battery->pdata->wpc_en, 1);
					pr_info("%s: WC CONTROL: Disable", __func__);
				}
			} else if (x == 1) {
				battery->wc_enable = true;
				battery->wc_enable_cnt = 0;
				if (battery->pdata->wpc_en) {
					gpio_direction_output(battery->pdata->wpc_en, 0);
					pr_info("%s: WC CONTROL: Enable", __func__);
				}
			} else {
				dev_info(battery->dev,
					"%s: WC CONTROL unknown command\n",
					__func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
	case WC_CONTROL_CNT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			battery->wc_enable_cnt_value = x;
			ret = count;
		}
		break;
	case HV_CHARGER_STATUS:
		break;
	case HV_WC_CHARGER_STATUS:
		break;
	case HV_CHARGER_SET:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev,
				"%s: HV_CHARGER_SET(%d)\n", __func__, x);
			if (x == 1) {
				battery->wire_status = SEC_BATTERY_CABLE_9V_TA;
				wake_lock(&battery->cable_wake_lock);
				queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
			} else {
				battery->wire_status = SEC_BATTERY_CABLE_NONE;
				wake_lock(&battery->cable_wake_lock);
				queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
			}
			ret = count;
		}
		break;
	case FACTORY_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			battery->factory_mode = x ? true : false;
			ret = count;
		}
		break;
	case STORE_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if !defined(CONFIG_SEC_FACTORY)
			if (x) {
				battery->store_mode = true;
				if (is_hv_wire_type(battery->cable_type) ||
					is_hv_wireless_type(battery->cable_type)) {
					sec_bat_set_charging_current(battery);
				}
			}
#endif
			ret = count;
		}
		break;
	case UPDATE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			/* update battery info */
			sec_bat_get_battery_info(battery);
			ret = count;
		}
		break;
	case TEST_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			battery->test_mode = x;
			wake_lock(&battery->monitor_wake_lock);
			queue_delayed_work(battery->monitor_wqueue,
				&battery->monitor_work, 0);
			ret = count;
		}
		break;

	case BATT_EVENT_CALL:
	case BATT_EVENT_2G_CALL:
	case BATT_EVENT_TALK_GSM:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_3G_CALL:
	case BATT_EVENT_TALK_WCDMA:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_MUSIC:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_VIDEO:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_BROWSER:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_HOTSPOT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_CAMERA:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_CAMCORDER:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_DATA_CALL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_WIFI:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_WIBRO:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_LTE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_EVENT_LCD:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			struct timespec ts;
			get_monotonic_boottime(&ts);
			if (x) {
				battery->lcd_status = true;
			} else {
				battery->lcd_status = false;
			}
			ret = count;
		}
		break;
	case BATT_EVENT_GPS:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			ret = count;
		}
		break;
	case BATT_TEMP_TABLE:
		if (sscanf(buf, "%10d %10d %10d %10d %10d %10d %10d %10d\n",
			&t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &t[6], &t[7]) == 8) {
			pr_info("%s: (new) %d %d %d %d %d %d %d %d\n",
				__func__, t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]);
			pr_info("%s: (default) %d %d %d %d %d %d %d %d\n",
				__func__,
				battery->pdata->temp_high_threshold_normal,
				battery->pdata->temp_high_recovery_normal,
				battery->pdata->temp_low_threshold_normal,
				battery->pdata->temp_low_recovery_normal,
				battery->pdata->temp_high_threshold_lpm,
				battery->pdata->temp_high_recovery_lpm,
				battery->pdata->temp_low_threshold_lpm,
				battery->pdata->temp_low_recovery_lpm);
			update_external_temp_table(battery, t);
			ret = count;
		}
		break;
	case BATT_HIGH_CURRENT_USB:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			battery->is_hc_usb = x ? true : false;
			value.intval = battery->is_hc_usb;

			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_USB_HC, value);

			pr_info("%s: is_hc_usb (%d)\n", __func__, battery->is_hc_usb);
			ret = count;
		}
		break;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case TEST_CHARGE_CURRENT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x >= 0 && x <= 2000) {
				dev_err(battery->dev,
					"%s: BATT_TEST_CHARGE_CURRENT(%d)\n", __func__, x);
				battery->pdata->charging_current[
					SEC_BATTERY_CABLE_USB].input_current_limit = x;
				battery->pdata->charging_current[
					SEC_BATTERY_CABLE_USB].fast_charging_current = x;
				if (x > 500) {
					battery->eng_not_full_status = true;
					battery->pdata->temp_check_type =
						SEC_BATTERY_TEMP_CHECK_NONE;
				}
				if (battery->cable_type == SEC_BATTERY_CABLE_USB) {
					value.intval = x;
					psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CURRENT_NOW,
						value);
				}
			}
			ret = count;
		}
		break;
#endif
	case SET_STABILITY_TEST:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_err(battery->dev,
				"%s: BATT_STABILITY_TEST(%d)\n", __func__, x);
			if (x) {
				battery->stability_test = true;
				battery->eng_not_full_status = true;
			}
			else {
				battery->stability_test = false;
				battery->eng_not_full_status = false;
			}
			ret = count;
		}
		break;
	case BATT_CAPACITY_MAX:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_err(battery->dev,
					"%s: BATT_CAPACITY_MAX(%d), fg_reset(%d)\n", __func__, x, fg_reset);
			if (!fg_reset) {
				value.intval = x;
				psy_do_property(battery->pdata->fuelgauge_name, set,
						POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, value);

				/* update soc */
				value.intval = 0;
				psy_do_property(battery->pdata->fuelgauge_name, get,
						POWER_SUPPLY_PROP_CAPACITY, value);
				battery->capacity = value.intval;
			} else {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				battery->fg_reset = 1;
#endif
			}
			ret = count;
		}
		break;
	case BATT_INBAT_VOLTAGE:
		break;
	case BATT_INBAT_VOLTAGE_OCV:
		break;
	case CHECK_SLAVE_CHG:
		break;
	case BATT_INBAT_WIRELESS_CS100:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s send cs100 command \n",__func__);
			value.intval = POWER_SUPPLY_STATUS_FULL;
			psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_STATUS, value);
			ret = count;
		}
		break;
	case HMT_TA_CONNECTED:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if !defined(CONFIG_CCIC_NOTIFIER)
			dev_info(battery->dev,
					"%s: HMT_TA_CONNECTED(%d)\n", __func__, x);
			if (x) {
				value.intval = false;
				psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
						value);
				dev_info(battery->dev,
						"%s: changed to OTG cable detached\n", __func__);

				battery->wire_status = SEC_BATTERY_CABLE_HMT_CONNECTED;
				wake_lock(&battery->cable_wake_lock);
				queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
			} else {
				value.intval = true;
				psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
						value);
				dev_info(battery->dev,
						"%s: changed to OTG cable attached\n", __func__);

				battery->wire_status = SEC_BATTERY_CABLE_OTG;
				wake_lock(&battery->cable_wake_lock);
				queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
			}
#endif
			ret = count;
		}
		break;
	case HMT_TA_CHARGE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if defined(CONFIG_CCIC_NOTIFIER)
			dev_info(battery->dev,
					"%s: HMT_TA_CHARGE(%d)\n", __func__, x);
			if (x) {
				sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE);
			} else {
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE,
							  SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE);
			}
			sec_bat_set_charge(battery, x? SEC_BAT_CHG_MODE_CHARGING:SEC_BAT_CHG_MODE_CHARGING_OFF);
			ret = count;
#else
			dev_info(battery->dev,
					"%s: HMT_TA_CHARGE(%d)\n", __func__, x);
			psy_do_property(battery->pdata->charger_name, get,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
			if (value.intval) {
				dev_info(battery->dev,
					"%s: ignore HMT_TA_CHARGE(%d)\n", __func__, x);
			} else {
				if (x) {
					value.intval = false;
					psy_do_property(battery->pdata->charger_name, set,
							POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
							value);
					dev_info(battery->dev,
						"%s: changed to OTG cable detached\n", __func__);
					battery->wire_status = SEC_BATTERY_CABLE_HMT_CHARGE;
					wake_lock(&battery->cable_wake_lock);
					queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
				} else {
					value.intval = false;
					psy_do_property(battery->pdata->charger_name, set,
							POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
							value);
					dev_info(battery->dev,
							"%s: changed to OTG cable detached\n", __func__);
					battery->wire_status = SEC_BATTERY_CABLE_HMT_CONNECTED;
					wake_lock(&battery->cable_wake_lock);
					queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
				}
			}
			ret = count;
#endif
		}
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case FG_CYCLE:
		break;
	case FG_FULL_VOLTAGE:
		break;
	case FG_FULLCAPNOM:
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST_DETACHABLE)
	case BATT_AFTER_MANUFACTURED:
#else
	case BATTERY_CYCLE:
#endif
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev, "%s: %s(%d)\n", __func__,
				(offset == BATTERY_CYCLE) ?
				"BATTERY_CYCLE" : "BATTERY_CYCLE(W)", x);
			if (x >= 0) {
				int prev_battery_cycle = battery->batt_cycle;
				battery->batt_cycle = x;
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_CYCLE] = x;
#endif				
				if (prev_battery_cycle < 0) {
					sec_bat_aging_check(battery);
				}
			}
			ret = count;
		}
		break;
#endif
	case BATT_WPC_TEMP:
	case BATT_WPC_TEMP_ADC:
	case BATT_COIL_TEMP:
	case BATT_COIL_TEMP_ADC:
		break;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	case BATT_WIRELESS_FIRMWARE_UPDATE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == SEC_WIRELESS_RX_SDCARD_MODE) {
				pr_info("%s fw mode is SDCARD \n", __func__);
				sec_bat_fw_update_work(battery, SEC_WIRELESS_RX_SDCARD_MODE);
			} else if (x == SEC_WIRELESS_RX_BUILT_IN_MODE) {
				pr_info("%s fw mode is BUILD IN \n", __func__);
				sec_bat_fw_update_work(battery, SEC_WIRELESS_RX_BUILT_IN_MODE);
			} else if (x == SEC_WIRELESS_TX_ON_MODE) {
				pr_info("%s tx mode is on \n", __func__);
				sec_bat_fw_update_work(battery, SEC_WIRELESS_TX_ON_MODE);
			} else if (x == SEC_WIRELESS_TX_OFF_MODE) {
				pr_info("%s tx mode is off \n", __func__);
				sec_bat_fw_update_work(battery, SEC_WIRELESS_TX_OFF_MODE);
			} else {
				dev_info(battery->dev, "%s: wireless firmware unknown command\n", __func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
	case OTP_FIRMWARE_RESULT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == 2) {
				value.intval = x;
				pr_info("%s RX firmware update ready!\n", __func__);
				psy_do_property(battery->pdata->wireless_charger_name, set,
								POWER_SUPPLY_PROP_MANUFACTURER, value);
			} else {
				dev_info(battery->dev, "%s: firmware unknown command\n", __func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
	case WC_IC_GRADE:
	case OTP_FIRMWARE_VER_BIN:
	case OTP_FIRMWARE_VER:
	case TX_FIRMWARE_RESULT:
	case TX_FIRMWARE_VER:
		break;
	case BATT_TX_STATUS:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == SEC_TX_OFF) {
				pr_info("%s TX mode is off \n", __func__);
				sec_bat_fw_update_work(battery, SEC_WIRELESS_TX_OFF_MODE);
			} else if (x == SEC_TX_STANDBY) {
				pr_info("%s TX mode is on \n", __func__);
				sec_bat_fw_update_work(battery, SEC_WIRELESS_TX_ON_MODE);
			} else {
				dev_info(battery->dev, "%s: TX firmware unknown command\n", __func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
#endif
	case WC_VOUT:
	case WC_VRECT:
		break;
	case BATT_HV_WIRELESS_STATUS:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == 1 && is_hv_wireless_type(battery->cable_type)) {
				wake_lock(&battery->cable_wake_lock);
#ifdef CONFIG_SEC_FACTORY
				pr_info("%s change cable type HV WIRELESS -> WIRELESS \n", __func__);
				battery->wc_status = SEC_WIRELESS_PAD_WPC;
				battery->cable_type = SEC_BATTERY_CABLE_WIRELESS;
				sec_bat_set_charging_current(battery);
#endif
				pr_info("%s HV_WIRELESS_STATUS set to 1. Vout set to 5V. \n", __func__);
				value.intval = WIRELESS_VOUT_5V;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
				wake_unlock(&battery->cable_wake_lock);
			} else if (x == 3 && is_hv_wireless_type(battery->cable_type)) {
				pr_info("%s HV_WIRELESS_STATUS set to 3. Vout set to 10V. \n", __func__);
				value.intval = WIRELESS_VOUT_10V;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			} else {
				dev_info(battery->dev, "%s: HV_WIRELESS_STATUS unknown command\n", __func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
	case BATT_HV_WIRELESS_PAD_CTRL:
		if (sscanf(buf, "%10d\n", &x) == 1) {

			pr_err("%s: x : %d\n", __func__, x);

			if (x == 1) {
				ret = sec_set_param(CM_OFFSET, '1');
				if (ret < 0) {
					pr_err("%s:sec_set_param failed\n", __func__);
					return ret;
				} else {
					pr_info("%s fan off \n", __func__);
					sleep_mode = true;
					if (is_hv_wireless_type(battery->cable_type)) {
#if defined(CONFIG_CALC_TIME_TO_FULL)
						battery->complete_timetofull = false;
#endif
						value.intval = WIRELESS_PAD_FAN_ON;
						psy_do_property(battery->pdata->wireless_charger_name, set,
									POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
						msleep(250);
						value.intval = WIRELESS_PAD_FAN_OFF;
						psy_do_property(battery->pdata->wireless_charger_name, set,
							POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);

						msleep(250);
						value.intval = battery->pdata->sleep_mode_limit_current;
						psy_do_property(battery->pdata->charger_name, set,
								POWER_SUPPLY_PROP_CURRENT_MAX, value);
						battery->input_current = value.intval;
#if defined(CONFIG_CALC_TIME_TO_FULL)
						queue_delayed_work(battery->monitor_wqueue, &battery->timetofull_work,
								msecs_to_jiffies(5000));
#endif
						wake_lock(&battery->monitor_wake_lock);
						queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
					}
				}
			} else if (x == 2) {
				ret = sec_set_param(CM_OFFSET, '0');
				if (ret < 0) {
					pr_err("%s: sec_set_param failed\n", __func__);
					return ret;
				} else {
					sleep_mode = false;
					pr_info("%s fan on \n", __func__);
					if (is_hv_wireless_type(battery->cable_type)) {
#if defined(CONFIG_CALC_TIME_TO_FULL)
						battery->complete_timetofull = false;
#endif
						value.intval = WIRELESS_PAD_FAN_ON;
						psy_do_property(battery->pdata->wireless_charger_name, set,
									POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);

						msleep(250);

#if defined(CONFIG_CALC_TIME_TO_FULL)
						queue_delayed_work(battery->monitor_wqueue, &battery->timetofull_work,
								msecs_to_jiffies(5000));
#endif
						wake_lock(&battery->monitor_wake_lock);
						queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
					}
				}
			} else if (x == 3) {
				pr_info("%s led off \n", __func__);
				value.intval = WIRELESS_PAD_LED_OFF;
				psy_do_property(battery->pdata->wireless_charger_name, set,
								POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			} else if (x == 4) {
				pr_info("%s led on \n", __func__);
				value.intval = WIRELESS_PAD_LED_ON;
				psy_do_property(battery->pdata->wireless_charger_name, set,
								POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			} else {
				dev_info(battery->dev, "%s: BATT_HV_WIRELESS_PAD_CTRL unknown command\n", __func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
	case BATT_TUNE_FLOAT_VOLTAGE:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s float voltage = %d mV",__func__, x);

		if(x > 4000 && x <= 4400 ){
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, value);
		}
		break;
	case BATT_TUNE_INPUT_CHARGE_CURRENT:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s input charge current = %d mA",__func__, x);

		if(x >= 0 && x <= 4000 ){
			for(i=0; i < SEC_BATTERY_CABLE_MAX; i++)
				battery->pdata->charging_current[i].input_current_limit = x;

			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CURRENT_MAX, value);
		}
		break;
	case BATT_TUNE_FAST_CHARGE_CURRENT:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s fast charge current = %d mA",__func__, x);
		if(x >= 0 && x <= 4000 ){
			for(i=0; i < SEC_BATTERY_CABLE_MAX; i++)
				battery->pdata->charging_current[i].fast_charging_current = x;

			value.intval = x;
				psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CURRENT_AVG, value);
		}
		break;
	case BATT_TUNE_UI_TERM_CURRENT_1ST:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s ui term current = %d mA",__func__, x);

		if(x > 0 && x < 1000 ){
			battery->pdata->full_check_current_1st = x;
		}
		break;
	case BATT_TUNE_UI_TERM_CURRENT_2ND:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s ui term current = %d mA",__func__, x);

		if(x > 0 && x < 1000 ){
			battery->pdata->full_check_current_2nd = x;
		}
		break;	
	case BATT_TUNE_TEMP_HIGH_NORMAL:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s temp high normal block	= %d ",__func__, x);
		if(x < 1000 && x >= -200)
			battery->pdata->temp_high_threshold_normal = x;
		break;
	case BATT_TUNE_TEMP_HIGH_REC_NORMAL:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s temp high normal recover  = %d ",__func__, x);
		if(x < 1000 && x >= -200)
			battery->pdata->temp_high_recovery_normal = x;
		break;
	case BATT_TUNE_TEMP_LOW_NORMAL:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s temp low normal block  = %d ",__func__, x);
		if(x < 1000 && x >= -200)
			battery->pdata->temp_low_threshold_normal = x;
		break;
	case BATT_TUNE_TEMP_LOW_REC_NORMAL:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s temp low normal recover  = %d ",__func__, x);
		if(x < 1000 && x >= -200)
			battery->pdata->temp_low_recovery_normal = x;
		break;
	case BATT_TUNE_CHG_TEMP_HIGH:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s chg_high_temp  = %d ",__func__, x);
		if(x < 1000 && x >= -200)
			battery->pdata->chg_high_temp = x;
		break;
	case BATT_TUNE_CHG_TEMP_REC:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s chg_high_temp_recovery	= %d ",__func__, x);
		if(x < 1000 && x >= -200)
			battery->pdata->chg_high_temp_recovery = x;
		break;
	case BATT_TUNE_CHG_LIMMIT_CUR:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s chg_charging_limit_current	= %d ",__func__, x);
		if(x < 3000 && x > 0)
		{
			battery->pdata->chg_charging_limit_current = x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_ERR].input_current_limit= x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_UNKNOWN].input_current_limit= x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_TA].input_current_limit= x;
		}
		break;
	case BATT_TUNE_COIL_TEMP_HIGH:
		break;
	case BATT_TUNE_COIL_TEMP_REC:
		break;
	case BATT_TUNE_COIL_LIMMIT_CUR:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s wpc_charging_limit_current	= %d ",__func__, x);
		if(x < 3000 && x > 0)
		{
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_ERR].input_current_limit= x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_UNKNOWN].input_current_limit= x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_TA].input_current_limit= x;
		}
		break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	case BATT_UPDATE_DATA:
		if (!battery->data_path && (count * sizeof(char)) < 256) {
			battery->data_path = kzalloc((count * sizeof(char) + 1), GFP_KERNEL);
			if (battery->data_path) {
				sscanf(buf, "%s\n", battery->data_path);
				cancel_delayed_work(&battery->batt_data_work);
				wake_lock(&battery->batt_data_wake_lock);
				queue_delayed_work(battery->monitor_wqueue,
					&battery->batt_data_work, msecs_to_jiffies(100));
			} else {
				pr_info("%s: failed to alloc data_path buffer\n", __func__);
			}
		}
		ret = count;
		break;
#endif
	case BATT_MISC_EVENT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: PMS sevice hiccup read done : %d ", __func__, x);
			if (!battery->hiccup_status &&
				(battery->misc_event & BATT_MISC_EVENT_HICCUP_TYPE)) {
				sec_bat_set_misc_event(battery,
					BATT_MISC_EVENT_HICCUP_TYPE, 1);
			}
		}
		ret = count;
		break;
	case BATT_EXT_DEV_CHG:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: Connect Ext Device : %d ",__func__, x);

			switch (x) {
				case EXT_DEV_NONE:
					battery->wire_status = SEC_BATTERY_CABLE_NONE;
					value.intval = 0;
					break;
				case EXT_DEV_GAMEPAD_CHG:
					battery->wire_status = SEC_BATTERY_CABLE_TA;
					value.intval = 0;
					break;
				case EXT_DEV_GAMEPAD_OTG:
					battery->wire_status = SEC_BATTERY_CABLE_OTG;
					value.intval = 1;
					break;
				default:
					break;
			}

			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
					value);

			queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work, 0);
			ret = count;
		}
		break;
	case BATT_WDT_CONTROL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: Charger WDT Set : %d\n", __func__, x);
			battery->wdt_kick_disable = x;
		}
		ret = count;
		break;
	case MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE, value);
			ret = count;
		}
		break;
	case CHECK_PS_READY:
	case BATT_CHIP_ID:
		break;
	case CISD_FULLCAPREP_MAX:
		break;
#if defined(CONFIG_BATTERY_CISD)
	case CISD_DATA:
		{
			int temp_data[CISD_DATA_MAX] = {0,};

			if (sscanf(buf, "%10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d\n",
					&temp_data[0], &temp_data[1], &temp_data[2],
					&temp_data[3], &temp_data[4], &temp_data[5],
					&temp_data[6], &temp_data[7], &temp_data[8],
					&temp_data[9], &temp_data[10], &temp_data[11],
					&temp_data[12], &temp_data[13], &temp_data[14],
					&temp_data[15], &temp_data[16], &temp_data[17],
					&temp_data[18], &temp_data[19], &temp_data[20],
					&temp_data[21], &temp_data[22], &temp_data[23],
					&temp_data[24], &temp_data[25], &temp_data[26],
					&temp_data[27], &temp_data[28], &temp_data[29],
					&temp_data[30], &temp_data[31], &temp_data[32],
					&temp_data[33], &temp_data[34], &temp_data[35],
					&temp_data[36], &temp_data[37], &temp_data[38],
					&temp_data[39]) <= CISD_DATA_MAX) {
				struct cisd *pcisd = &battery->cisd;

				for (i = 0; i < CISD_DATA_MAX; i++) {
					if (i == CISD_DATA_CAP_PER_TIME ||
						i == CISD_DATA_BATT_TEMP_MIN ||
						i == CISD_DATA_CHG_TEMP_MIN ||
						i == CISD_DATA_WPC_TEMP_MIN ||
						i == CISD_DATA_CAP_MIN)
						continue;

					if (temp_data[i] < 0) {
						temp_data[0] = -1;
						break;
					}
				}

				pr_info("%s: %s cisd data\n", __func__, ((temp_data[0] <= 0 || battery->fg_reset) ? "init" : "update"));
				if (temp_data[0] <= 0 || battery->fg_reset) {
					/* initialize data */
					for (i = 0; i < CISD_DATA_MAX; i++)
						battery->cisd.data[i] = 0;
					battery->fg_reset = 0;

					pcisd->data[CISD_DATA_FULL_COUNT] = 1;
					pcisd->data[CISD_DATA_CAP_ONCE] = 0;
					pcisd->data[CISD_DATA_CAP_PER_TIME] = 0;
					pcisd->data[CISD_DATA_RECHARGING_TIME] = 0x7FFFFFFF;
					pcisd->data[CISD_DATA_BATT_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_WPC_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_BATT_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_WPC_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CAP_MIN] = 0xFFFF;
				} else {
					/* update data */
					for (i = 0; i < CISD_DATA_MAX; i++)
						pcisd->data[i] = temp_data[i];
				}
				ret = count;

				wake_lock(&battery->monitor_wake_lock);
				queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
			}
		}
		break;
	case CISD_DATA_JSON:
		{
			char tc;
			struct cisd *pcisd = &battery->cisd;

			if (sscanf(buf, "%1c\n", &tc) == 1) {
				if (tc == 'c') {
					for (i = 0; i < CISD_DATA_MAX; i++)
						pcisd->data[i] = 0;

					pcisd->data[CISD_DATA_CAP_ONCE] = 0;
					pcisd->data[CISD_DATA_CAP_PER_TIME] = 0;
					pcisd->data[CISD_DATA_RECHARGING_TIME] = 0x7FFFFFFF;
					pcisd->data[CISD_DATA_BATT_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_WPC_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_BATT_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_WPC_TEMP_MIN] = 1000;
				}
			}
			ret = count;
		}
		break;
	case CISD_WIRE_COUNT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			struct cisd *pcisd = &battery->cisd;
			pr_info("%s: Wire Count : %d\n", __func__, x);
			pcisd->data[CISD_DATA_WIRE_COUNT] = x;
		}
		ret = count;
		break;
#endif
#if defined(CONFIG_BATTERY_SBM_DATA)
	case SBM_DATA:
		break;
#endif
	case SAFETY_TIMER_SET:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x) {
				battery->safety_timer_set = true;
			} else {
				battery->safety_timer_set = false;
			}
			ret = count;
		}
		break;
	case BATT_SWELLING_CONTROL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x) {
				pr_info("%s : 15TEST START!! SWELLING MODE DISABLE\n", __func__);
				battery->skip_swelling = true;
			} else {
				pr_info("%s : 15TEST END!! SWELLING MODE END\n", __func__);
				battery->skip_swelling = false;
			}
			ret = count;
		}
		break;		
	case SAFETY_TIMER_INFO:
		break;	
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case BATT_TEMP_TEST:
	{
		char tc;
		if (sscanf(buf, "%c %10d\n", &tc, &x) == 2) {
			pr_info("%s : temperature t: %c, temp: %d\n", __func__, tc, x);
			if (tc == 'u') {
				battery->temperature_test_usb = x;
			} else if (tc == 'w') {
				battery->temperature_test_wpc = x;
			} else if (tc == 'b') {
				battery->temperature_test_battery = x;
			} else if (tc == 'c') {
				battery->temperature_test_chg = x;
			}
			ret = count;
		}
		break;
	}
#endif
	case BATT_CURRENT_EVENT:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sec_bat_create_attrs(struct device *dev)
{
	unsigned long i = 0;
	int rc = 0;

	for (i = 0; i < ARRAY_SIZE(sec_battery_attrs); i++) {
		rc = device_create_file(dev, &sec_battery_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_battery_attrs[i]);
create_attrs_succeed:
	return rc;
}

static int sec_bat_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);
	int current_cable_type = SEC_BATTERY_CABLE_NONE;
	int full_check_type = SEC_BATTERY_FULLCHARGED_NONE;
	union power_supply_propval value = {0, };
	enum power_supply_ext_property ext_psp = psp;

	dev_dbg(battery->dev,
		"%s: (%d,%d)\n", __func__, psp, val->intval);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (battery->charging_mode == SEC_BATTERY_CHARGING_1ST)
			full_check_type = battery->pdata->full_check_type;
		else
			full_check_type = battery->pdata->full_check_type_2nd;
		if ((full_check_type == SEC_BATTERY_FULLCHARGED_CHGINT) &&
			(val->intval == POWER_SUPPLY_STATUS_FULL))
			sec_bat_do_fullcharged(battery);
		sec_bat_set_charging_status(battery, val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		sec_bat_ovp_uvlo_result(battery, val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		current_cable_type = val->intval;
#if !defined(CONFIG_CCIC_NOTIFIER)
		if ((battery->muic_cable_type != ATTACHED_DEV_SMARTDOCK_TA_MUIC)
			&& ((current_cable_type == SEC_BATTERY_CABLE_SMART_OTG) ||
			(current_cable_type == SEC_BATTERY_CABLE_SMART_NOTG)))
			break;
#endif

		if (current_cable_type < 0) {
			dev_info(battery->dev,
					"%s: ignore event(%d)\n",
					__func__, current_cable_type);
		} else if (current_cable_type == SEC_BATTERY_CABLE_OTG) {
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			battery->is_recharging = false;
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_DISCHARGING);
			battery->cable_type = current_cable_type;
			wake_lock(&battery->monitor_wake_lock);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->monitor_work, 0);
			break;
		} else {
			battery->wire_status = current_cable_type;
			if ((battery->wire_status == SEC_BATTERY_CABLE_NONE) &&
				(battery->wc_status != SEC_WIRELESS_PAD_NONE) )
				current_cable_type = SEC_BATTERY_CABLE_WIRELESS;
		}
		dev_info(battery->dev,
				"%s: current_cable(%d), wc_status(%d), wire_status(%d)\n",
				__func__, current_cable_type, battery->wc_status,
				battery->wire_status);

		/* cable is attached or detached
		 * if current_cable_type is minus value,
		 * check cable by sec_bat_get_cable_type()
		 * although SEC_BATTERY_CABLE_SOURCE_EXTERNAL is set
		 * (0 is SEC_BATTERY_CABLE_UNKNOWN)
		 */
		if ((current_cable_type >= 0) &&
			(current_cable_type < SEC_BATTERY_CABLE_MAX) &&
			(battery->pdata->cable_source_type &
			SEC_BATTERY_CABLE_SOURCE_EXTERNAL)) {

			wake_lock(&battery->cable_wake_lock);
				queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work,0);
		} else {
			if (sec_bat_get_cable_type(battery,
						battery->pdata->cable_source_type)) {
				wake_lock(&battery->cable_wake_lock);
					queue_delayed_work(battery->monitor_wqueue,
						&battery->cable_work,0);
			}
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		battery->capacity = val->intval;
		power_supply_changed(&battery->psy_bat);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* If JIG is attached, the voltage is set as 1079 */
		pr_info("%s : set to the battery history : (%d)\n",__func__, val->intval);
		if(val->intval == 1079)	{
			battery->voltage_now = 1079;
			battery->voltage_avg = 1079;
			power_supply_changed(&battery->psy_bat);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		battery->present = val->intval;

		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work(battery->monitor_wqueue,
				   &battery->monitor_work, 0);
		break;
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		break;
#endif
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		value.intval = val->intval;
		pr_info("%s: CHGIN-OTG %s\n", __func__, value.intval > 0 ? "on" : "off");
		psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
		break;
	case POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL:
		value.intval = val->intval;
		pr_info("%s: WCIN-UNO %s\n", __func__, value.intval > 0 ? "on" : "off");
		psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL, value);
		break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	case POWER_SUPPLY_PROP_POWER_DESIGN:
		sec_bat_parse_dt(battery->dev, battery);
		break;
#endif
#if defined(CONFIG_BATTERY_CISD)
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		pr_info("%s: Valert was occured! run monitor work for updating cisd data!\n", __func__);
		battery->cisd.data[CISD_DATA_VALERT_COUNT]++;
		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work_on(0, battery->monitor_wqueue,
			&battery->monitor_work, 0);
		break;
#endif
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_AICL_CURRENT:
			battery->aicl_current = val->intval;
			battery->max_charge_power = battery->charge_power = battery->input_voltage * val->intval;
			pr_info("%s: aicl : %dmA, %dmW)\n", __func__,
				battery->aicl_current, battery->charge_power);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_AICL_COUNT]++;
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_SYSOVLO:
			if (battery->status != POWER_SUPPLY_STATUS_DISCHARGING) {
				pr_info("%s: Vsys is ovlo !!\n", __func__);
				battery->is_sysovlo = true;
				battery->is_recharging = false;
				battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
				battery->health = POWER_SUPPLY_HEALTH_VSYS_OVP;
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_VSYS_OVP, SEC_BAT_CURRENT_EVENT_VSYS_OVP);				
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_VSYS_OVP]++;
#endif
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
				wake_lock(&battery->monitor_wake_lock);
				queue_delayed_work(battery->monitor_wqueue,
						   &battery->monitor_work, 0);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_VBAT_OVP:
			if (battery->status != POWER_SUPPLY_STATUS_DISCHARGING) {
				pr_info("%s: Vbat is ovlo !!\n", __func__);
				battery->is_vbatovlo = true;
				battery->is_recharging = false;
				battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
				battery->health = POWER_SUPPLY_HEALTH_VBAT_OVP;
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_VBAT_OVP, SEC_BAT_CURRENT_EVENT_VBAT_OVP);	
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_VBAT_OVP]++;
#endif
				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_CHARGING_OFF);
				wake_lock(&battery->monitor_wake_lock);
				queue_delayed_work(battery->monitor_wqueue,
						   &battery->monitor_work, 0);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_USB_CONFIGURE:
			if (battery->pdic_info.sink_status.rp_currentlvl > RP_CURRENT_LEVEL_DEFAULT)
				return 0;
			pr_info("%s: usb configured %d\n", __func__, val->intval);
			if (val->intval == USB_CURRENT_UNCONFIGURED) {
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_USB_100MA,
							  (SEC_BAT_CURRENT_EVENT_USB_100MA | SEC_BAT_CURRENT_EVENT_USB_SUPER));
			} else if (val->intval == USB_CURRENT_HIGH_SPEED) {
				sec_bat_set_misc_event(battery, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE, 1);
				sec_bat_set_current_event(battery, 0,
							  (SEC_BAT_CURRENT_EVENT_USB_100MA | SEC_BAT_CURRENT_EVENT_USB_SUPER));
				sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_USB,
						USB_CURRENT_HIGH_SPEED, USB_CURRENT_HIGH_SPEED);
			} else if (val->intval == USB_CURRENT_SUPER_SPEED) {
				sec_bat_set_misc_event(battery, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE, 1);
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_USB_SUPER,
							  (SEC_BAT_CURRENT_EVENT_USB_100MA | SEC_BAT_CURRENT_EVENT_USB_SUPER));
				sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_USB,
						USB_CURRENT_SUPER_SPEED, USB_CURRENT_SUPER_SPEED);
			}
			sec_bat_set_charging_current(battery);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_bat_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);
	union power_supply_propval value = {0, };
	enum power_supply_ext_property ext_psp = psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if ((battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) ||
			(battery->health == POWER_SUPPLY_HEALTH_UNDERVOLTAGE)) {
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if ((battery->pdata->cable_check_type &
				SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE) &&
				!lpcharge) {
				switch (battery->cable_type) {
				case SEC_BATTERY_CABLE_USB:
				case SEC_BATTERY_CABLE_USB_CDP:
					val->intval =
						POWER_SUPPLY_STATUS_DISCHARGING;
					return 0;
				}
			}
#if defined(CONFIG_STORE_MODE)
			if (battery->store_mode && !lpcharge &&
					battery->cable_type != SEC_BATTERY_CABLE_NONE &&
					battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			} else
#endif
				val->intval = battery->status;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (battery->cable_type == SEC_BATTERY_CABLE_NONE) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		} else {
			psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_CHARGE_TYPE, value);
			if (value.intval == SEC_BATTERY_CABLE_UNKNOWN)
				/* if error in CHARGE_TYPE of charger
				 * set CHARGE_TYPE as NONE
				 */
				val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			else
				val->intval = value.intval;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (battery->health >= POWER_SUPPLY_HEALTH_MAX)
			val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		else
			val->intval = battery->health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = battery->present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (is_hv_wireless_type(battery->cable_type)) {
			if (sec_bat_hv_wc_normal_mode_check(battery))
				val->intval = SEC_BATTERY_CABLE_WIRELESS;
			else
				val->intval = SEC_BATTERY_CABLE_HV_WIRELESS_ETX;
		}
		else if(battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK)
			val->intval = SEC_BATTERY_CABLE_WIRELESS;
		else if(battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK_TA)
			val->intval = SEC_BATTERY_CABLE_WIRELESS;
		else if(battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND)
			val->intval = SEC_BATTERY_CABLE_WIRELESS;
		else if(battery->cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS)
			val->intval = SEC_BATTERY_CABLE_WIRELESS;
		else if(battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_VEHICLE)
			val->intval = SEC_BATTERY_CABLE_WIRELESS;
		else
			val->intval = battery->cable_type;
		pr_info("%s cable type = %d sleep_mode = %d\n", __func__, val->intval, sleep_mode);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = battery->pdata->technology;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#ifdef CONFIG_SEC_FACTORY
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
		battery->voltage_now = value.intval;
		dev_err(battery->dev,
			"%s: voltage now(%d)\n", __func__, battery->voltage_now);
#endif
		/* voltage value should be in uV */
		val->intval = battery->voltage_now * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
#ifdef CONFIG_SEC_FACTORY
		value.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
		battery->voltage_avg = value.intval;
		dev_err(battery->dev,
			"%s: voltage avg(%d)\n", __func__, battery->voltage_avg);
#endif
		/* voltage value should be in uV */
		val->intval = battery->voltage_avg * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = battery->current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = battery->current_avg;
		break;
	/* charging mode (differ from power supply) */
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = battery->charging_mode;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (battery->pdata->fake_capacity) {
			val->intval = 90;
			pr_info("%s : capacity(%d)\n", __func__, val->intval);
		} else {
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
			if (battery->status == POWER_SUPPLY_STATUS_FULL) {
				if(battery->eng_not_full_status)
					val->intval = battery->capacity;
				else
					val->intval = 100;
			} else {
				val->intval = battery->capacity;
			}
#else
			if (battery->status == POWER_SUPPLY_STATUS_FULL)
				val->intval = 100;
			else
				val->intval = battery->capacity;
#endif
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->temperature;
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = battery->temper_amb;
		break;
#if defined(CONFIG_CALC_TIME_TO_FULL)
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		if (battery->capacity == 100) {
			val->intval = -1;
			break;
		}

		if (((battery->status == POWER_SUPPLY_STATUS_CHARGING) ||
			(battery->status == POWER_SUPPLY_STATUS_FULL && battery->capacity != 100)) &&
			battery->complete_timetofull &&
			!battery->swelling_mode)
			val->intval = battery->timetofull;
		else
			val->intval = -1;
		break;
#endif
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (battery->swelling_mode)
			val->intval = 1;
		else
			val->intval = 0;
		break;
#endif
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		val->intval = battery->wire_status;
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
	case POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL:
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		val->intval = battery->charge_power;
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_SUB_PBA_TEMP_REC:
			val->intval = !battery->vbus_limit;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sec_usb_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	if ((battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) ||
		(battery->health == POWER_SUPPLY_HEALTH_UNDERVOLTAGE)) {
		val->intval = 0;
		return 0;
	}
	/* Set enable=1 only if the USB charger is connected */
	switch (battery->wire_status) {
	case SEC_BATTERY_CABLE_USB:
	case SEC_BATTERY_CABLE_USB_CDP:
		val->intval = 1;
		break;
	case SEC_BATTERY_CABLE_PDIC:
	        val->intval = (battery->pd_usb_attached) ? 1:0;
	        break;
	default:
		val->intval = 0;
		break;
	}

	if (battery->slate_mode)
		val->intval = 0;
	return 0;
}

static int sec_ac_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_ac);
	enum power_supply_ext_property ext_psp = psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if ((battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) ||
				(battery->health == POWER_SUPPLY_HEALTH_UNDERVOLTAGE)) {
			val->intval = 0;
			return 0;
		}

		/* Set enable=1 only if the AC charger is connected */
		switch (battery->cable_type) {
		case SEC_BATTERY_CABLE_TA:
		case SEC_BATTERY_CABLE_UARTOFF:
		case SEC_BATTERY_CABLE_LAN_HUB:
		case SEC_BATTERY_CABLE_UNKNOWN:
		case SEC_BATTERY_CABLE_PREPARE_TA:
		case SEC_BATTERY_CABLE_9V_ERR:
		case SEC_BATTERY_CABLE_9V_UNKNOWN:
		case SEC_BATTERY_CABLE_9V_TA:
		case SEC_BATTERY_CABLE_12V_TA:
		case SEC_BATTERY_CABLE_HMT_CONNECTED:
		case SEC_BATTERY_CABLE_HMT_CHARGE:
		case SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT:
		case SEC_BATTERY_CABLE_QC20:
		case SEC_BATTERY_CABLE_QC30:
		case SEC_BATTERY_CABLE_TIMEOUT:
		case SEC_BATTERY_CABLE_SMART_OTG:
		case SEC_BATTERY_CABLE_SMART_NOTG:	
			val->intval = 1;
			break;
                case SEC_BATTERY_CABLE_PDIC:
                        val->intval = (battery->pd_usb_attached) ? 0:1;
                        break;
		default:
			val->intval = 0;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->chg_temp;
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
			case POWER_SUPPLY_EXT_PROP_WATER_DETECT:
				if (battery->misc_event & BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE) {
					val->intval = 1;
					pr_info("%s: Water Detect\n", __func__);
				} else {
					val->intval = 0;
				}
   				break;
  			default:
				return -EINVAL;
  		}
  		break;
	default:
		return -EINVAL;
	}

	if (lpcharge && (battery->misc_event & BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE)) {
		val->intval = 1;
	}

	return 0;
}

static int sec_wireless_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_wireless);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	val->intval = battery->wc_status;

	return 0;
}

static int sec_wireless_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_wireless);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#if defined(CONFIG_BATTERY_CISD)
		if (val->intval != SEC_WIRELESS_PAD_NONE && battery->wc_status == SEC_WIRELESS_PAD_NONE)
			battery->cisd.data[CISD_DATA_WIRELESS_COUNT]++;
#endif
		if (val->intval == SEC_WIRELESS_PAD_A4WP)
			battery->wc_status = SEC_WIRELESS_PAD_WPC;
		else
			battery->wc_status = val->intval;

		wake_lock(&battery->cable_wake_lock);
		queue_delayed_work(battery->monitor_wqueue,
			&battery->cable_work, 0);
		if (battery->wc_status == SEC_WIRELESS_PAD_NONE ||
			battery->wc_status == SEC_WIRELESS_PAD_WPC_PACK ||
			battery->wc_status == SEC_WIRELESS_PAD_WPC_PACK_TA ||
			battery->wc_status == SEC_WIRELESS_PAD_VEHICLE) {
			sec_bat_set_misc_event(battery, BATT_MISC_EVENT_WIRELESS_BACKPACK_TYPE,
				(battery->wc_status == SEC_WIRELESS_PAD_NONE));
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_ps_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_ps);
	union power_supply_propval value;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == 0 && battery->ps_enable == true) {
			battery->ps_enable = false;
			value.intval = val->intval;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
		} else if ((val->intval == 1) && (battery->ps_enable == false) &&
				(battery->ps_status == true)) {
			battery->ps_enable = true;
			value.intval = val->intval;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
		} else {
			dev_err(battery->dev,
				"%s: invalid setting (%d)\n", __func__, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval == SEC_BATTERY_CABLE_POWER_SHARING) {
			battery->ps_status = true;
			battery->ps_enable = true;
			value.intval = battery->ps_enable;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
		} else {
			battery->ps_status = false;
			battery->ps_enable = false;
			value.intval = battery->ps_enable;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_ps_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_ps);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = (battery->ps_enable) ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (battery->ps_status) ? 1 : 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) || defined(CONFIG_MUIC_NOTIFIER)
static int sec_bat_cable_check(struct sec_battery_info *battery,
				muic_attached_dev_t attached_dev)
{
	int current_cable_type = -1;
	union power_supply_propval val = {0, };

	pr_info("[%s]ATTACHED(%d)\n", __func__, attached_dev);

	switch (attached_dev)
	{
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		battery->is_jig_on = true;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_NONE;
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNDEFINED_RANGE_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_NONE;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.data[CISD_WATER_DETECT]++;
#endif
		break;
	case ATTACHED_DEV_HICCUP_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_NONE;
		break;
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_OTG;
		break;
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_TIMEOUT;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_USB;
		break;
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_UARTOFF;
		break;
	case ATTACHED_DEV_RDU_TA_MUIC:
		battery->store_mode = true;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC:
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	case ATTACHED_DEV_POGO_DOCK_MUIC:
#endif
		current_cable_type = SEC_BATTERY_CABLE_TA;
		break;
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	case ATTACHED_DEV_POGO_DOCK_5V_MUIC:
#endif
		if (is_hv_wire_type(battery->cable_type) &&
		    (battery->chg_limit || battery->vbus_chg_by_siop)) {
			current_cable_type = SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT;
		} else if (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC) {
			current_cable_type = SEC_BATTERY_CABLE_PREPARE_TA;
		} else {
			current_cable_type = SEC_BATTERY_CABLE_TA;
		}
		break;
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_USB_CDP;
		break;
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_LAN_HUB;
		break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_POWER_SHARING;
		break;
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_PREPARE_TA;
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	case ATTACHED_DEV_POGO_DOCK_9V_MUIC:
#endif
		current_cable_type = SEC_BATTERY_CABLE_9V_TA;
		break;
#if defined(CONFIG_MUIC_HV_12V)
	case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_12V_TA;
		break;
#endif
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_9V_ERR;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.data[CISD_AFC_FAIL]++;
#endif
		break;
	case ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_9V_UNKNOWN;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.data[CISD_AFC_FAIL]++;
#endif
		break;
	case ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_UNKNOWN;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, attached_dev);
		break;
	}

	if (battery->is_jig_on && !battery->pdata->support_fgsrc_change)
		psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);

	return current_cable_type;
}
#endif

#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#if defined(CONFIG_CCIC_NOTIFIER)
static int sec_bat_get_pd_list_index(PDIC_SINK_STATUS *sink_status, struct sec_bat_pdic_list *pd_list)
{
	int i = 0;

	for (i = 0; i < pd_list->max_pd_count; i++) {
		if (pd_list->pd_info[i].pdo_index == sink_status->current_pdo_num)
			return i;
	}

	return 0;
}

static void sec_bat_set_rp_current(struct sec_battery_info *battery, int cable_type)
{
	if (battery->pdic_info.sink_status.rp_currentlvl > RP_CURRENT_LEVEL_DEFAULT && cable_type == SEC_BATTERY_CABLE_USB)
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_USB_100MA);
	if (battery->pdic_info.sink_status.rp_currentlvl == RP_CURRENT_LEVEL3)
		sec_bat_change_default_current(battery, cable_type,
			RP_CURRENT_RP3, battery->pdata->max_charging_current);
	else if (battery->pdic_info.sink_status.rp_currentlvl == RP_CURRENT_LEVEL2)
		sec_bat_change_default_current(battery, cable_type,
			RP_CURRENT_RP2, RP_CURRENT_RP2);
	else if (cable_type == SEC_BATTERY_CABLE_USB)
		sec_bat_change_default_current(battery, cable_type,
			RP_CURRENT_RP1, RP_CURRENT_RP1);

	pr_info("%s:(%d)\n", __func__, battery->pdic_info.sink_status.rp_currentlvl);
	battery->max_charge_power = 0;
	sec_bat_set_charging_current(battery);
}
#endif

static int make_pd_list(struct sec_battery_info *battery)
{
	int i = 0, j = 0, min = 0, temp_voltage = 0, temp_current = 0, temp_index = 0;
	int base_charge_power = 0, selected_pdo_voltage = 0, selected_pdo_num = 0;
	int pd_list_index = 0;

	for(base_charge_power = battery->pdata->pd_charging_charge_power * 1000;
	    base_charge_power >= 1000000; base_charge_power -= 1000000)
	{
		selected_pdo_voltage = battery->pdata->max_input_voltage + 1;
		selected_pdo_num = 0;
		for(i=1; i<= battery->pdic_info.sink_status.available_pdo_num; i++)
		{
			if (battery->pdic_info.sink_status.power_list[i].max_voltage *
			    battery->pdic_info.sink_status.power_list[i].max_current > base_charge_power - 1000000 &&
			    battery->pdic_info.sink_status.power_list[i].max_voltage *
			    battery->pdic_info.sink_status.power_list[i].max_current <= base_charge_power)
			{
				if (battery->pdic_info.sink_status.power_list[i].max_voltage < selected_pdo_voltage)
				{
					selected_pdo_voltage = battery->pdic_info.sink_status.power_list[i].max_voltage;
					selected_pdo_num = i;
					battery->pdic_info.sink_status.power_list[i].max_current =
						battery->pdic_info.sink_status.power_list[i].max_current > 
						battery->pdata->max_input_current ?
						battery->pdata->max_input_current : battery->pdic_info.sink_status.power_list[i].max_current;
				}
			}
		}
		if (selected_pdo_num)
		{
			battery->pd_list.pd_info[pd_list_index].input_voltage =
				battery->pdic_info.sink_status.power_list[selected_pdo_num].max_voltage;
			battery->pd_list.pd_info[pd_list_index].input_current = 
				battery->pdic_info.sink_status.power_list[selected_pdo_num].max_current;
			battery->pd_list.pd_info[pd_list_index].pdo_index = selected_pdo_num;
			pd_list_index++;
		}
	}
	pr_info("%s: total pd_list_index: %d\n", __func__, pd_list_index);
	if (pd_list_index <= 0) {
		pr_info("%s : PDO list is empty!!\n", __func__);
		return 0;
	}

	for (i = 0; i < pd_list_index - 1; i++) {
		min = i;
		for (j = i + 1; j < pd_list_index; j++) {
			if (battery->pd_list.pd_info[j].input_voltage <
				battery->pd_list.pd_info[min].input_voltage)
				min = j;
		}
		temp_voltage = battery->pd_list.pd_info[i].input_voltage;
		battery->pd_list.pd_info[i].input_voltage =
			battery->pd_list.pd_info[min].input_voltage;
		battery->pd_list.pd_info[min].input_voltage = temp_voltage;
		temp_current = battery->pd_list.pd_info[i].input_current;
		battery->pd_list.pd_info[i].input_current =
			battery->pd_list.pd_info[min].input_current;
		battery->pd_list.pd_info[min].input_current = temp_current;
		temp_index = battery->pd_list.pd_info[i].pdo_index;
		battery->pd_list.pd_info[i].pdo_index =
			battery->pd_list.pd_info[min].pdo_index;
		battery->pd_list.pd_info[min].pdo_index = temp_index;
	}
	for(i = 0; i < pd_list_index; i++) {
		pr_info("%s: Made pd_list[%d], voltage : %d, current : %d, index : %d\n", __func__, i,
		battery->pd_list.pd_info[i].input_voltage,
		battery->pd_list.pd_info[i].input_current,
		battery->pd_list.pd_info[i].pdo_index);
	}
	battery->pd_list.max_pd_count = pd_list_index;
	battery->max_charge_power = battery->pdic_info.sink_status.power_list[ \
		battery->pd_list.pd_info[pd_list_index-1].pdo_index].max_voltage * \
		battery->pdic_info.sink_status.power_list[battery->pd_list.pd_info[ \
		pd_list_index-1].pdo_index].max_current / 1000;
	battery->pd_max_charge_power = battery->max_charge_power;

	if (battery->pdic_info.sink_status.selected_pdo_num == battery->pd_list.pd_info[pd_list_index-1].pdo_index) {
		battery->pd_list.now_pd_index = pd_list_index - 1;
		battery->pdic_ps_rdy = true;
		dev_info(battery->dev, "%s: battery->pdic_ps_rdy(%d)\n", __func__, battery->pdic_ps_rdy);
	} else {
		battery->pdic_ps_rdy = false;
		select_pdo(battery->pd_list.pd_info[pd_list_index-1].pdo_index);
	}

	return battery->pd_list.max_pd_count;
}

static int usb_typec_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	const char *cmd;
	int cable_type = SEC_BATTERY_CABLE_NONE, i = 0, current_pdo = 0;
	struct sec_battery_info *battery =
			container_of(nb, struct sec_battery_info, usb_typec_nb);
	CC_NOTI_ATTACH_TYPEDEF usb_typec_info = *(CC_NOTI_ATTACH_TYPEDEF *)data;

	dev_info(battery->dev, "%s: action (%ld) dump(0x%01x, 0x%01x, 0x%02x, 0x%04x, 0x%04x, 0x%04x)\n",
		__func__, action, usb_typec_info.src, usb_typec_info.dest, usb_typec_info.id,
		usb_typec_info.attach, usb_typec_info.rprd, usb_typec_info.cable_type);

	if (usb_typec_info.dest != CCIC_NOTIFY_DEV_BATTERY) {
		dev_info(battery->dev, "%s: skip handler dest(%d)\n",
			__func__, usb_typec_info.dest);
		return 0;
	}

	mutex_lock(&battery->typec_notylock);
	switch (usb_typec_info.id) {
	case CCIC_NOTIFY_ID_WATER:
	case CCIC_NOTIFY_ID_ATTACH:
		switch (usb_typec_info.attach) {
		case MUIC_NOTIFY_CMD_DETACH:
		case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
			cmd = "DETACH";
			battery->is_jig_on = false;
			battery->pd_usb_attached = false;
			cable_type = SEC_BATTERY_CABLE_NONE;
			battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
			battery->pdic_info.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
			break;
		case MUIC_NOTIFY_CMD_ATTACH:
		case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
			/* Skip notify from MUIC if PDIC is attached already */
			if (battery->wire_status == SEC_BATTERY_CABLE_PDIC) {
				mutex_unlock(&battery->typec_notylock);
				return 0;
			}
			cmd = "ATTACH";
			battery->muic_cable_type = usb_typec_info.cable_type;
			cable_type = sec_bat_cable_check(battery, battery->muic_cable_type);
			if (battery->pdic_info.sink_status.rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT &&
				(cable_type == SEC_BATTERY_CABLE_USB || cable_type == SEC_BATTERY_CABLE_TA)) {
				sec_bat_set_rp_current(battery, cable_type);
			} else if ((struct pdic_notifier_struct *)usb_typec_info.pd != NULL &&
				(*(struct pdic_notifier_struct *)usb_typec_info.pd).event == PDIC_NOTIFY_EVENT_CCIC_ATTACH &&
				(*(struct pdic_notifier_struct *)usb_typec_info.pd).sink_status.rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT &&
				(cable_type == SEC_BATTERY_CABLE_USB || cable_type == SEC_BATTERY_CABLE_TA)) {
				battery->pdic_info.sink_status.rp_currentlvl =
					(*(struct pdic_notifier_struct *)usb_typec_info.pd).sink_status.rp_currentlvl;
				sec_bat_set_rp_current(battery, cable_type);
			}
			break;
		default:
			cmd = "ERROR";
			cable_type = -1;
			battery->muic_cable_type = usb_typec_info.cable_type;
			break;
		}
		battery->pdic_attach = false;
		battery->pdic_ps_rdy = false;
#if defined(CONFIG_AFC_CHARGER_MODE)
		if (battery->muic_cable_type == ATTACHED_DEV_QC_CHARGER_9V_MUIC ||
			battery->muic_cable_type == ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC)
			battery->hv_chg_name = "QC";
		else if (battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_9V_MUIC ||
			battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC ||
			battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC ||
			battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC)
			battery->hv_chg_name = "AFC";
#if defined(CONFIG_MUIC_HV_12V)
		else if (battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_12V_MUIC ||
			battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC)
			battery->hv_chg_name = "12V";
#endif
		else
			battery->hv_chg_name = "NONE";
#endif
		break;
	case CCIC_NOTIFY_ID_POWER_STATUS:
#ifdef CONFIG_SEC_FACTORY
		dev_info(battery->dev, "%s: pd_event(%d)\n", __func__,
			(*(struct pdic_notifier_struct *)usb_typec_info.pd).event);
#endif
		if ((*(struct pdic_notifier_struct *)usb_typec_info.pd).event == PDIC_NOTIFY_EVENT_DETACH){
			dev_info(battery->dev, "%s: skip pd operation - attach(%d)\n", __func__, usb_typec_info.attach);
			battery->pdic_attach = false;
			battery->pdic_ps_rdy = false;
			battery->pd_list.now_pd_index = 0;
			mutex_unlock(&battery->typec_notylock);
			return 0;
		}

		cmd = "PD_ATTACH";
		if ((*(struct pdic_notifier_struct *)usb_typec_info.pd).event == PDIC_NOTIFY_EVENT_CCIC_ATTACH) {
			battery->pdic_info.sink_status.rp_currentlvl =
				(*(struct pdic_notifier_struct *)usb_typec_info.pd).sink_status.rp_currentlvl;
			dev_info(battery->dev, "%s: battery->rp_currentlvl(%d)\n", __func__, battery->pdic_info.sink_status.rp_currentlvl);
			if (battery->wire_status == SEC_BATTERY_CABLE_USB || battery->wire_status == SEC_BATTERY_CABLE_TA) {
				cable_type = battery->wire_status;
				battery->chg_limit = false;
				sec_bat_set_rp_current(battery, cable_type);
				goto skip_cable_check;
			}
			mutex_unlock(&battery->typec_notylock);
			return 0;
		}
		if ((*(struct pdic_notifier_struct *)usb_typec_info.pd).event == PDIC_NOTIFY_EVENT_PD_SINK_CAP)
			battery->pdic_attach = false;
		if (!battery->pdic_attach) {
			battery->pdic_info = *(struct pdic_notifier_struct *)usb_typec_info.pd;
			battery->pd_list.now_pd_index = 0;
		} else {
			battery->pdic_info.sink_status.current_pdo_num =
				(*(struct pdic_notifier_struct *)usb_typec_info.pd).sink_status.current_pdo_num;
			battery->pd_list.now_pd_index = sec_bat_get_pd_list_index(&battery->pdic_info.sink_status,
				&battery->pd_list);
			battery->pdic_ps_rdy = true;
			dev_info(battery->dev, "%s: battery->pdic_ps_rdy(%d)\n", __func__, battery->pdic_ps_rdy);
		}
		current_pdo = battery->pdic_info.sink_status.current_pdo_num;
		cable_type = SEC_BATTERY_CABLE_PDIC;
		battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
#if defined(CONFIG_AFC_CHARGER_MODE)
		battery->hv_chg_name = "PDIC";
#endif
		battery->input_voltage =
				battery->pdic_info.sink_status.power_list[current_pdo].max_voltage / 1000;
		dev_info(battery->dev, "%s: available pdo : %d, current pdo : %d\n", __func__,
			battery->pdic_info.sink_status.available_pdo_num, current_pdo);

		for(i=1; i<= battery->pdic_info.sink_status.available_pdo_num; i++) {
			pr_info("%s: power_list[%d], voltage : %d, current : %d, power : %d\n", __func__, i,
				battery->pdic_info.sink_status.power_list[i].max_voltage,
				battery->pdic_info.sink_status.power_list[i].max_current,
				battery->pdic_info.sink_status.power_list[i].max_voltage *
				battery->pdic_info.sink_status.power_list[i].max_current);

			if ((battery->pdic_info.sink_status.power_list[i].max_voltage *
			     battery->pdic_info.sink_status.power_list[i].max_current) >
			    (battery->pdata->pd_charging_charge_power * 1000)) {
				battery->pdic_info.sink_status.power_list[i].max_current =
					(battery->pdata->pd_charging_charge_power * 1000) /
					battery->pdic_info.sink_status.power_list[i].max_voltage;

				pr_info("%s: ->updated [%d], voltage : %d, current : %d, power : %d\n", __func__, i,
					battery->pdic_info.sink_status.power_list[i].max_voltage,
					battery->pdic_info.sink_status.power_list[i].max_current,
					battery->pdic_info.sink_status.power_list[i].max_voltage *
					battery->pdic_info.sink_status.power_list[i].max_current);
			}

		}
		if (!battery->pdic_attach) {
			if (make_pd_list(battery) <= 0)
				goto skip_cable_work;
		}
		battery->pdic_attach = true;
		break;
	case CCIC_NOTIFY_ID_USB:
		if(usb_typec_info.cable_type == PD_USB_TYPE)
			battery->pd_usb_attached = true;
		dev_info(battery->dev, "%s: CCIC_NOTIFY_ID_USB: %d\n",__func__, battery->pd_usb_attached);
		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		mutex_unlock(&battery->typec_notylock);
		return 0;
	default:
		cmd = "ERROR";
		cable_type = -1;
		battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
#if defined(CONFIG_AFC_CHARGER_MODE)
		battery->hv_chg_name = "NONE";
#endif
		break;
	}

skip_cable_check:
	sec_bat_set_misc_event(battery, BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE,
		(battery->muic_cable_type != ATTACHED_DEV_UNDEFINED_CHARGING_MUIC) &&
		(battery->muic_cable_type != ATTACHED_DEV_UNDEFINED_RANGE_MUIC));
	if (battery->muic_cable_type == ATTACHED_DEV_HICCUP_MUIC) {
		sec_bat_set_misc_event(battery, BATT_MISC_EVENT_HICCUP_TYPE, 0);
		battery->hiccup_status = 1;
	} else {
		battery->hiccup_status = 0;
		if (battery->misc_event & BATT_MISC_EVENT_HICCUP_TYPE) {
			wake_lock(&battery->monitor_wake_lock);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		}
	}

	/* showing charging icon and noti(no sound, vi, haptic) only
	   if slow insertion is detected by MUIC */
	sec_bat_set_misc_event(battery, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE,
		(battery->muic_cable_type != ATTACHED_DEV_TIMEOUT_OPEN_MUIC));

	if (cable_type < 0 || cable_type > SEC_BATTERY_CABLE_MAX) {
		dev_info(battery->dev, "%s: ignore event(%d)\n",
			__func__, battery->muic_cable_type);
		goto skip_cable_work;
	} else if ((cable_type == SEC_BATTERY_CABLE_UNKNOWN) &&
		   (battery->status != POWER_SUPPLY_STATUS_DISCHARGING)) {
		battery->cable_type = cable_type;
		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		dev_info(battery->dev, "%s: UNKNOWN cable plugin\n", __func__);
		goto skip_cable_work;
	}
	battery->wire_status = cable_type;

	cancel_delayed_work(&battery->cable_work);

	if (cable_type == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT) {
		/* set current event */
		cancel_delayed_work(&battery->afc_work);
		wake_unlock(&battery->afc_wake_lock);
		sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_CHG_LIMIT,
					  (SEC_BAT_CURRENT_EVENT_CHG_LIMIT | SEC_BAT_CURRENT_EVENT_AFC));
		wake_lock(&battery->monitor_wake_lock);
		battery->polling_count = 1;	/* initial value = 1 */
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	} else if ((battery->wire_status == battery->cable_type) &&
		(((battery->wire_status == SEC_BATTERY_CABLE_USB || battery->wire_status == SEC_BATTERY_CABLE_TA) &&
		battery->pdic_info.sink_status.rp_currentlvl > RP_CURRENT_LEVEL_DEFAULT) ||
		is_hv_wire_type(battery->wire_status))) {
		cancel_delayed_work(&battery->afc_work);
		wake_unlock(&battery->afc_wake_lock);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_AFC);

		wake_lock(&battery->monitor_wake_lock);
		battery->polling_count = 1;	/* initial value = 1 */
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	} else if (cable_type == SEC_BATTERY_CABLE_PREPARE_TA) {
		wake_lock(&battery->cable_wake_lock);
		queue_delayed_work(battery->monitor_wqueue,
				   &battery->cable_work, msecs_to_jiffies(500));
	} else {
		wake_lock(&battery->cable_wake_lock);
		queue_delayed_work(battery->monitor_wqueue,
				&battery->cable_work, 0);
	}

skip_cable_work:
	dev_info(battery->dev, "%s: CMD[%s], CABLE_TYPE[%d]\n", __func__, cmd, cable_type);
	mutex_unlock(&battery->typec_notylock);
	return 0;
}
#else
#if defined(CONFIG_CCIC_NOTIFIER)
static int batt_pdic_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	const char *cmd;
	struct sec_battery_info *battery =
		container_of(nb, struct sec_battery_info,
				pdic_nb);
	battery->pdic_info = *(struct pdic_notifier_struct *)data;

	mutex_lock(&battery->batt_handlelock);
	pr_info("%s: pdic_event: %d\n", __func__, battery->pdic_info.event);

	switch (battery->pdic_info.event) {
		int i, selected_pdo;

		case PDIC_NOTIFY_EVENT_DETACH:
			cmd = "DETACH";
			battery->pdic_attach = false;
			if (battery->wire_status == SEC_BATTERY_CABLE_PDIC) {
				battery->wire_status = SEC_BATTERY_CABLE_NONE;
				wake_lock(&battery->cable_wake_lock);
				queue_delayed_work(battery->monitor_wqueue,
						&battery->cable_work, 0);
			}
			break;
		case PDIC_NOTIFY_EVENT_CCIC_ATTACH:
			cmd = "ATTACH";
			break;
		case PDIC_NOTIFY_EVENT_PD_SINK:
			selected_pdo = battery->pdic_info.sink_status.selected_pdo_num;
			cmd = "ATTACH";
			battery->wire_status = SEC_BATTERY_CABLE_PDIC;
			battery->pdic_attach = true;
			battery->input_voltage =
				battery->pdic_info.sink_status.power_list[selected_pdo].max_voltage / 1000;

			pr_info("%s: total pdo : %d, selected pdo : %d\n", __func__,
					battery->pdic_info.sink_status.available_pdo_num, selected_pdo);
			for(i=1; i<= battery->pdic_info.sink_status.available_pdo_num; i++)
			{
				pr_info("%s: power_list[%d], voltage : %d, current : %d, power : %d\n", __func__, i,
						battery->pdic_info.sink_status.power_list[i].max_voltage,
						battery->pdic_info.sink_status.power_list[i].max_current,
						battery->pdic_info.sink_status.power_list[i].max_voltage *
						battery->pdic_info.sink_status.power_list[i].max_current);
			}
			wake_lock(&battery->cable_wake_lock);
			queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work, 0);
			break;
		case PDIC_NOTIFY_EVENT_PD_SOURCE:
			cmd = "ATTACH";
			break;
		default:
			cmd = "ERROR";
			break;
	}
	pr_info("%s: CMD=%s, cable_type : %d\n", __func__, cmd, battery->cable_type);
	mutex_unlock(&battery->batt_handlelock);
	return 0;
}
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
static int batt_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	const char *cmd;
	int cable_type = SEC_BATTERY_CABLE_NONE;
	struct sec_battery_info *battery =
		container_of(nb, struct sec_battery_info,
			     batt_nb);
	union power_supply_propval value = {0, };

#if defined(CONFIG_CCIC_NOTIFIER)
	CC_NOTI_ATTACH_TYPEDEF *p_noti = (CC_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = p_noti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif

	mutex_lock(&battery->batt_handlelock);
	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		cmd = "DETACH";
		battery->is_jig_on = false;
		cable_type = SEC_BATTERY_CABLE_NONE;
		battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		cmd = "ATTACH";
		cable_type = sec_bat_cable_check(battery, attached_dev);
		battery->muic_cable_type = attached_dev;
		break;
	default:
		cmd = "ERROR";
		cable_type = -1;
		battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
		break;
	}

	sec_bat_set_misc_event(battery, BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE,
#if !defined(CONFIG_ENG_BATTERY_CONCEPT) && !defined(CONFIG_SEC_FACTORY)
		(battery->muic_cable_type != ATTACHED_DEV_JIG_UART_ON_MUIC) &&
		(battery->muic_cable_type != ATTACHED_DEV_JIG_USB_ON_MUIC) &&
#endif
		(battery->muic_cable_type != ATTACHED_DEV_UNDEFINED_RANGE_MUIC));
	if (battery->muic_cable_type == ATTACHED_DEV_HICCUP_MUIC) {
		sec_bat_set_misc_event(battery, BATT_MISC_EVENT_HICCUP_TYPE, 0);
		battery->hiccup_status = 1;
	} else {
		battery->hiccup_status = 0;
		if (battery->misc_event & BATT_MISC_EVENT_HICCUP_TYPE) {
			wake_lock(&battery->monitor_wake_lock);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		}
	}

#if defined(CONFIG_CCIC_NOTIFIER)
	/* If PD cable is already attached, return this function */
	if(battery->pdic_attach) {
		dev_info(battery->dev, "%s: ignore event pdic attached(%d)\n",
			__func__, battery->pdic_attach);
		mutex_unlock(&battery->batt_handlelock);
		return 0;
	}
#endif

	if (attached_dev == ATTACHED_DEV_MHL_MUIC) {
		mutex_unlock(&battery->batt_handlelock);
		return 0;
	}

	if (cable_type < 0) {
		dev_info(battery->dev, "%s: ignore event(%d)\n",
			__func__, cable_type);
	} else if (cable_type == SEC_BATTERY_CABLE_POWER_SHARING) {
		battery->ps_status = true;
		battery->ps_enable = true;
		battery->wire_status = cable_type;
		dev_info(battery->dev, "%s: power sharing cable plugin\n", __func__);
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_PACK;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK_TA) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_PACK_TA;
	} else if (cable_type == SEC_BATTERY_CABLE_HV_WIRELESS) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_HV;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_STAND;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_STAND) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_STAND_HV;	
	} else if (cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS) {
		battery->wc_status = SEC_WIRELESS_PAD_PMA;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_VEHICLE) {
		battery->wc_status = SEC_WIRELESS_PAD_VEHICLE;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE) {
		battery->wc_status = SEC_WIRELESS_PAD_VEHICLE_HV;
	} else if ((cable_type == SEC_BATTERY_CABLE_UNKNOWN) &&
		   (battery->status != POWER_SUPPLY_STATUS_DISCHARGING)) {
		battery->cable_type = cable_type;
		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		dev_info(battery->dev,
			"%s: UNKNOWN cable plugin\n", __func__);
		mutex_unlock(&battery->batt_handlelock);
		return 0;
	} else {
		battery->wire_status = cable_type;
		if ((battery->wire_status == SEC_BATTERY_CABLE_NONE)	&&
			(battery->wc_status) && (!battery->ps_status))
			cable_type = SEC_BATTERY_CABLE_WIRELESS;
	}
	dev_info(battery->dev,
			"%s: current_cable(%d), wc_status(%d), wire_status(%d)\n",
			__func__, cable_type, battery->wc_status,
			battery->wire_status);

	mutex_unlock(&battery->batt_handlelock);
	if (attached_dev == ATTACHED_DEV_USB_LANHUB_MUIC) {
		if (!strcmp(cmd, "ATTACH")) {
			value.intval = true;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL,
					value);
			dev_info(battery->dev,
				"%s: Powered OTG cable attached\n", __func__);
		} else {
			value.intval = false;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL,
					value);
			dev_info(battery->dev,
				"%s: Powered OTG cable detached\n", __func__);
		}
	}

#if defined(CONFIG_AFC_CHARGER_MODE)
	if (!strcmp(cmd, "ATTACH")) {
		if ((battery->muic_cable_type >= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC) &&
		    (battery->muic_cable_type <= ATTACHED_DEV_QC_CHARGER_9V_MUIC)) {
			battery->hv_chg_name = "QC";
		} else if ((battery->muic_cable_type >= ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC) &&
			 (battery->muic_cable_type <= ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC)) {
			battery->hv_chg_name = "AFC";
#if defined(CONFIG_MUIC_HV_12V)
		} else if (battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_12V_MUIC ||
			battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC) {
			battery->hv_chg_name = "12V";
#endif
		} else
			battery->hv_chg_name = "NONE";
	} else {
			battery->hv_chg_name = "NONE";
	}

	pr_info("%s : HV_CHARGER_NAME(%s)\n",
		__func__, battery->hv_chg_name);
#endif

	if ((cable_type >= 0) &&
	    cable_type <= SEC_BATTERY_CABLE_MAX) {
		if (cable_type == SEC_BATTERY_CABLE_POWER_SHARING) {
			value.intval = battery->ps_enable;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
			wake_lock(&battery->monitor_wake_lock);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		} else if((cable_type == SEC_BATTERY_CABLE_NONE) && (battery->ps_status)) {
			if (battery->ps_enable) {
				battery->ps_enable = false;
				value.intval = battery->ps_enable;
				psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
			}
			battery->ps_status = false;
			wake_lock(&battery->monitor_wake_lock);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		} else if(cable_type != battery->cable_type) {
			wake_lock(&battery->cable_wake_lock);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->cable_work, 0);
		} else {
			dev_info(battery->dev,
				"%s: Cable is Not Changed(%d)\n",
				__func__, battery->cable_type);
		}
	}

	pr_info("%s: CMD=%s, attached_dev=%d\n", __func__, cmd, attached_dev);

	return 0;
}
#endif /* CONFIG_MUIC_NOTIFIER */
#endif

#if defined(CONFIG_VBUS_NOTIFIER)
static int vbus_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	vbus_status_t vbus_status = *(vbus_status_t *)data;
	struct sec_battery_info *battery =
		container_of(nb, struct sec_battery_info,
			     vbus_nb);
	union power_supply_propval value = {0, };

	mutex_lock(&battery->batt_handlelock);
	if (battery->muic_cable_type == ATTACHED_DEV_HMT_MUIC &&
		battery->muic_vbus_status != vbus_status &&
		battery->muic_vbus_status == STATUS_VBUS_HIGH &&
		vbus_status == STATUS_VBUS_LOW) {
		msleep(500);
		value.intval = true;
		psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
				value);
		dev_info(battery->dev,
			"%s: changed to OTG cable attached\n", __func__);

		battery->wire_status = SEC_BATTERY_CABLE_OTG;
		wake_lock(&battery->cable_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
	}
	pr_info("%s: action=%d, vbus_status=%d\n", __func__, (int)action, vbus_status);
	mutex_unlock(&battery->batt_handlelock);
	battery->muic_vbus_status = vbus_status;

	return 0;
}
#endif

#ifdef CONFIG_OF
static int sec_bat_parse_dt(struct device *dev,
		struct sec_battery_info *battery)
{
	struct device_node *np;
	sec_battery_platform_data_t *pdata = battery->pdata;
	int ret = 0, len = 0;
	unsigned int i = 0;
	const u32 *p;
	u32 temp = 0;

	np = of_find_node_by_name(NULL, "cable-info");
	if (!np) {
		pr_err ("%s : np NULL\n", __func__);
	} else {
		struct device_node *child;
		u32 input_current = 0, charging_current = 0;

		ret = of_property_read_u32(np, "default_input_current", &input_current);
		ret = of_property_read_u32(np, "default_charging_current", &charging_current);
		ret = of_property_read_u32(np, "full_check_current_1st", &pdata->full_check_current_1st);
		ret = of_property_read_u32(np, "full_check_current_2nd", &pdata->full_check_current_2nd);

		pdata->charging_current =
			kzalloc(sizeof(sec_charging_current_t) * SEC_BATTERY_CABLE_MAX,
				GFP_KERNEL);

		for (i = 0; i < SEC_BATTERY_CABLE_MAX; i++) {
			pdata->charging_current[i].input_current_limit = (unsigned int)input_current;
			pdata->charging_current[i].fast_charging_current = (unsigned int)charging_current;
		}

		for_each_child_of_node(np, child) {
			ret = of_property_read_u32(child, "input_current", &input_current);
			ret = of_property_read_u32(child, "charging_current", &charging_current);
			
			p = of_get_property(child, "cable_number", &len);
			if (!p)
				return 1;

			len = len / sizeof(u32);

			for (i = 0; i <= len; i++) {
				ret = of_property_read_u32_index(child, "cable_number", i, &temp);
				pdata->charging_current[temp].input_current_limit = (unsigned int)input_current;
				pdata->charging_current[temp].fast_charging_current = (unsigned int)charging_current;
			}

		}
	}

	for (i = 0; i < SEC_BATTERY_CABLE_MAX; i++) {
		pr_info("%s : CABLE_NUM(%d) INPUT(%d) CHARGING(%d)\n",
			__func__, i,
			pdata->charging_current[i].input_current_limit,
			pdata->charging_current[i].fast_charging_current);
	}

	pr_info("%s : TOPOFF_1ST(%d), TOPOFF_2ND(%d)\n",
		__func__, pdata->full_check_current_1st, pdata->full_check_current_2nd);
#ifdef CONFIG_SEC_FACTORY
	pdata->charging_current[SEC_BATTERY_CABLE_TA].fast_charging_current = 1500;
#endif
	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	}

#if defined(CONFIG_BATTERY_CISD)	
	ret = of_property_read_u32(np, "battery,battery_full_capacity",
		&pdata->battery_full_capacity);
	if (ret) {
		pr_info("%s : battery_full_capacity is Empty\n", __func__);
	} else {
		pr_info("%s : battery_full_capacity : %d\n", __func__, pdata->battery_full_capacity);
		pdata->cisd_cap_high_thr = pdata->battery_full_capacity + 1000;
		pdata->cisd_cap_low_thr = pdata->battery_full_capacity + 500;
		pdata->cisd_cap_limit = (pdata->battery_full_capacity * 11) / 10;
	}

	ret = of_property_read_u32(np, "battery,cisd_max_voltage_thr",
		&pdata->max_voltage_thr);
	if (ret) {
		pr_info("%s : cisd_max_voltage_thr is Empty\n", __func__);
		pdata->max_voltage_thr = 4400;
	}	
#endif

	ret = of_property_read_u32(np,
				   "battery,expired_time", &temp);
	if (ret) {
		pr_info("expired time is empty\n");
		pdata->expired_time = 3 * 60 * 60;
	} else {
		pdata->expired_time = (unsigned int) temp;
	}
	pdata->expired_time *= 1000;
	battery->expired_time = pdata->expired_time;

	ret = of_property_read_u32(np,
				   "battery,recharging_expired_time", &temp);
	if (ret) {
		pr_info("expired time is empty\n");
		pdata->recharging_expired_time = 90 * 60;
	} else {
		pdata->recharging_expired_time = (unsigned int) temp;
	}
	pdata->recharging_expired_time *= 1000;

	ret = of_property_read_u32(np,
				   "battery,standard_curr", &pdata->standard_curr);
	if (ret) {
		pr_info("standard_curr is empty\n");
		pdata->standard_curr = 2150;
	}

	ret = of_property_read_string(np,
		"battery,vendor", (char const **)&pdata->vendor);
	if (ret)
		pr_info("%s: Vendor is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,charger_name", (char const **)&pdata->charger_name);
	if (ret)
		pr_info("%s: Charger name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,fuelgauge_name", (char const **)&pdata->fuelgauge_name);
	if (ret)
		pr_info("%s: Fuelgauge name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,wireless_charger_name", (char const **)&pdata->wireless_charger_name);
	if (ret)
		pr_info("%s: Wireless charger name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,fgsrc_switch_name", (char const **)&pdata->fgsrc_switch_name);
	if (ret) {
		pdata->support_fgsrc_change = false;
		pr_info("%s: fgsrc_switch_name is Empty\n", __func__);
	}
	else
		pdata->support_fgsrc_change = true;

	ret = of_property_read_string(np,
		"battery,wireless_charger_name", (char const **)&pdata->wireless_charger_name);
	if (ret)
		pr_info("%s: Wireless charger name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,chip_vendor", (char const **)&pdata->chip_vendor);
	if (ret)
		pr_info("%s: Chip vendor is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,technology",
		&pdata->technology);
	if (ret)
		pr_info("%s : technology is Empty\n", __func__);

	ret = of_property_read_u32(np,
		"battery,wireless_cc_cv", &pdata->wireless_cc_cv);

	pdata->fake_capacity = of_property_read_bool(np,
						     "battery,fake_capacity");

	p = of_get_property(np, "battery,polling_time", &len);
	if (!p)
		return 1;

	len = len / sizeof(u32);
	pdata->polling_time = kzalloc(sizeof(*pdata->polling_time) * len, GFP_KERNEL);
	ret = of_property_read_u32_array(np, "battery,polling_time",
					 pdata->polling_time, len);
	if (ret)
		pr_info("%s : battery,polling_time is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,thermal_source",
		&pdata->thermal_source);
	if (ret)
		pr_info("%s : Thermal source is Empty\n", __func__);

	if (pdata->thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
		p = of_get_property(np, "battery,temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->temp_adc_table_size = len;
		pdata->temp_amb_adc_table_size = len;

		pdata->temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->temp_adc_table_size, GFP_KERNEL);
		pdata->temp_amb_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->temp_adc_table_size, GFP_KERNEL);

		for(i = 0; i < pdata->temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
					 "battery,temp_table_adc", i, &temp);
			pdata->temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,temp_table_data", i, &temp);
			pdata->temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : Temp_adc_table(data) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,temp_table_adc", i, &temp);
			pdata->temp_amb_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : Temp_amb_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,temp_table_data", i, &temp);
			pdata->temp_amb_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : Temp_amb_adc_table(data) is Empty\n",
					__func__);
		}
	}
	ret = of_property_read_u32(np, "battery,usb_thermal_source",
		&pdata->usb_thermal_source);
	if (ret)
		pr_info("%s : usb_thermal_source is Empty\n", __func__);

	if(pdata->usb_thermal_source) {
		p = of_get_property(np, "battery,usb_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->usb_temp_adc_table_size = len;

		pdata->usb_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->usb_temp_adc_table_size, GFP_KERNEL);

		for(i = 0; i < pdata->usb_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,usb_temp_table_adc", i, &temp);
			pdata->usb_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : Usb_Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,usb_temp_table_data", i, &temp);
			pdata->usb_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : Usb_Temp_adc_table(data) is Empty\n",
					__func__);
		}
	}

	ret = of_property_read_u32(np, "battery,chg_thermal_source",
		&pdata->chg_thermal_source);
	if (ret)
		pr_info("%s : chg_thermal_source is Empty\n", __func__);

	if(pdata->chg_thermal_source) {
		p = of_get_property(np, "battery,chg_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->chg_temp_adc_table_size = len;

		pdata->chg_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->chg_temp_adc_table_size, GFP_KERNEL);

		for(i = 0; i < pdata->chg_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,chg_temp_table_adc", i, &temp);
			pdata->chg_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : CHG_Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,chg_temp_table_data", i, &temp);
			pdata->chg_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : CHG_Temp_adc_table(data) is Empty\n",
					__func__);
		}
	}

	ret = of_property_read_u32(np, "battery,wpc_thermal_source",
		&pdata->wpc_thermal_source);
	if (ret)
		pr_info("%s : wpc_thermal_source is Empty\n", __func__);

	if(pdata->wpc_thermal_source) {
		p = of_get_property(np, "battery,wpc_temp_table_adc", &len);
		if (!p) {
			pr_info("%s : wpc_temp_table_adc(adc) is Empty\n",__func__);
		} else {
			len = len / sizeof(u32);

			pdata->wpc_temp_adc_table_size = len;

			pdata->wpc_temp_adc_table =
				kzalloc(sizeof(sec_bat_adc_table_data_t) *
					pdata->wpc_temp_adc_table_size, GFP_KERNEL);

			for(i = 0; i < pdata->wpc_temp_adc_table_size; i++) {
				ret = of_property_read_u32_index(np,
								 "battery,wpc_temp_table_adc", i, &temp);
				pdata->wpc_temp_adc_table[i].adc = (int)temp;
				if (ret)
					pr_info("%s : WPC_Temp_adc_table(adc) is Empty\n",
						__func__);

				ret = of_property_read_u32_index(np,
								 "battery,wpc_temp_table_data", i, &temp);
				pdata->wpc_temp_adc_table[i].data = (int)temp;
				if (ret)
					pr_info("%s : WPC_Temp_adc_table(data) is Empty\n",
						__func__);
			}
		}
	}

	ret = of_property_read_u32(np, "battery,coil_thermal_source",
		&pdata->coil_thermal_source);
	if (ret)
		pr_info("%s : coil_thermal_source is Empty\n", __func__);
	else
		pr_info("%s : coil_thermal_source exists\n", __func__);

	ret = of_property_read_u32(np, "battery,slave_thermal_source",
		&pdata->slave_thermal_source);
	if (ret)
		pr_info("%s : slave_thermal_source is Empty\n", __func__);

	if(pdata->slave_thermal_source) {
		p = of_get_property(np, "battery,slave_chg_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->slave_chg_temp_adc_table_size = len;

		pdata->slave_chg_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->slave_chg_temp_adc_table_size, GFP_KERNEL);

		for(i = 0; i < pdata->slave_chg_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,slave_chg_temp_table_adc", i, &temp);
			pdata->slave_chg_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : slave_chg_temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,slave_chg_temp_table_data", i, &temp);
			pdata->slave_chg_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : slave_chg_temp_adc_table(data) is Empty\n",
					__func__);
		}
	}
	ret = of_property_read_u32(np, "battery,slave_chg_temp_check",
		&pdata->slave_chg_temp_check);
	if (ret)
		pr_info("%s : slave_chg_temp_check is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_temp_check",
		&pdata->chg_temp_check);
	if (ret)
		pr_info("%s : chg_temp_check is Empty\n", __func__);

	if (pdata->chg_temp_check) {
		ret = of_property_read_u32(np, "battery,chg_12v_high_temp",
					   &temp);
		pdata->chg_12v_high_temp = (int)temp;
		if (ret)
			pr_info("%s : chg_12v_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_high_temp",
					   &temp);
		pdata->chg_high_temp = (int)temp;
		if (ret)
			pr_info("%s : chg_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_high_temp_recovery",
					   &temp);
		pdata->chg_high_temp_recovery = (int)temp;
		if (ret)
			pr_info("%s : chg_temp_recovery is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_charging_limit_current",
					   &pdata->chg_charging_limit_current);
		if (ret)
			pr_info("%s : chg_charging_limit_current is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_input_limit_current",
					   &pdata->chg_input_limit_current);
		if (ret)
			pr_info("%s : chg_input_limit_current is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,mix_high_temp",
					   &temp);
		pdata->mix_high_temp = (int)temp;
		if (ret)
			pr_info("%s : mix_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,mix_high_chg_temp",
					   &temp);
		pdata->mix_high_chg_temp = (int)temp;
		if (ret)
			pr_info("%s : mix_high_chg_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,mix_high_temp_recovery",
					   &temp);
		pdata->mix_high_temp_recovery = (int)temp;
		if (ret)
			pr_info("%s : mix_high_temp_recovery is Empty\n", __func__);
	}

	ret = of_property_read_u32(np, "battery,wpc_temp_check",
		&pdata->wpc_temp_check);
	if (ret)
		pr_info("%s : wpc_temp_check is Empty\n", __func__);

	if (pdata->wpc_temp_check) {
		p = of_get_property(np, "battery,wpc_high_temp", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);
		pdata->wpc_high_temp_size = len;
		pdata->wpc_high_temp = kzalloc(sizeof(unsigned int) * len, GFP_KERNEL);
		pdata->wpc_high_temp_recovery = kzalloc(sizeof(unsigned int) * len, GFP_KERNEL);
		if (!pdata->wpc_high_temp ||
			!pdata->wpc_high_temp_recovery)
			return 1;
		
		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				"battery,wpc_high_temp", i, &temp);
			pdata->wpc_high_temp[i] = (int)temp;
			if (ret)
				pr_info("%s : wpc_high_temp is Empty\n", __func__);

			ret = of_property_read_u32_index(np,
				"battery,wpc_high_temp_recovery", i, &temp);
			pdata->wpc_high_temp_recovery[i] = (int)temp;
			if (ret)
				pr_info("%s : wpc_high_temp_recovery is Empty\n", __func__);
		}

		pr_info("%s: wpc_high_temp_size(%d)\n", __func__, pdata->wpc_high_temp_size);
		for (i = 0; i < len; i++) {
			pr_info("%s: index(%d) - wpc_high_temp(%d), wpc_high_temp_recovery(%d)\n",
				__func__, i,
				pdata->wpc_high_temp[i],
				pdata->wpc_high_temp_recovery[i]);
		}

		ret = of_property_read_u32(np, "battery,wpc_charging_limit_current",
				&pdata->wpc_charging_limit_current);
		if (ret)
			pr_info("%s : wpc_charging_limit_current is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_lcd_on_high_temp",
				&pdata->wpc_lcd_on_high_temp);
		if (ret)
			pr_info("%s : wpc_lcd_on_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_lcd_on_high_temp_rec",
				&pdata->wpc_lcd_on_high_temp_rec);
		if (ret)
			pr_info("%s : wpc_lcd_on_high_temp_rec is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_lcd_on_charging_limit_current",
				&pdata->wpc_lcd_on_charging_limit_current);
		if (ret) {
			pr_info("%s : wpc_lcd_on_charging_limit_current is Empty\n", __func__);
			pdata->wpc_lcd_on_charging_limit_current =
				pdata->wpc_charging_limit_current;
		}		
	}

	ret = of_property_read_u32(np, "battery,wc_full_input_limit_current",
		&pdata->wc_full_input_limit_current);
	if (ret)
		pr_info("%s : wc_full_input_limit_current is Empty\n", __func__);	

	ret = of_property_read_u32(np, "battery,wc_cv_current",
		&pdata->wc_cv_current);
	if (ret)
		pr_info("%s : wc_cv_current is Empty\n", __func__);	

	ret = of_property_read_u32(np, "battery,wc_cv_pack_current",
		&pdata->wc_cv_pack_current);
	if (ret) {
		pr_info("%s : wc_cv_pack_current is Empty\n", __func__);
		pdata->wc_cv_pack_current = 500;
	}

	ret = of_property_read_u32(np, "battery,sleep_mode_limit_current",
			&pdata->sleep_mode_limit_current);
	if (ret)
		pr_info("%s : sleep_mode_limit_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,inbat_voltage",
			&pdata->inbat_voltage);
	if (ret)
		pr_info("%s : inbat_voltage is Empty\n", __func__);

	if (pdata->inbat_voltage) {
		p = of_get_property(np, "battery,inbat_voltage_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->inbat_adc_table_size = len;

		pdata->inbat_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
					pdata->inbat_adc_table_size, GFP_KERNEL);

		for(i = 0; i < pdata->inbat_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,inbat_voltage_table_adc", i, &temp);
			pdata->inbat_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : inbat_adc_table(adc) is Empty\n",
						__func__);

			ret = of_property_read_u32_index(np,
							 "battery,inbat_voltage_table_data", i, &temp);
			pdata->inbat_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : inbat_adc_table(data) is Empty\n",
						__func__);
		}
	}

	ret = of_property_read_u32(np, "battery,pre_afc_input_current",
		&pdata->pre_afc_input_current);
	if (ret) {
		pr_info("%s : pre_afc_input_current is Empty\n", __func__);
		pdata->pre_afc_input_current = 1000;
	}

	ret = of_property_read_u32(np, "battery,pre_afc_work_delay",
			&pdata->pre_afc_work_delay);
	if (ret) {
		pr_info("%s : pre_afc_work_delay is Empty\n", __func__);
		pdata->pre_afc_work_delay = 2000;
	}

	ret = of_property_read_u32(np, "battery,pre_wc_afc_input_current",
		&pdata->pre_wc_afc_input_current);
	if (ret) {
		pr_info("%s : pre_wc_afc_input_current is Empty\n", __func__);
		pdata->pre_wc_afc_input_current = 500; /* wc input default */
	}

	ret = of_property_read_u32(np, "battery,pre_wc_afc_work_delay",
			&pdata->pre_wc_afc_work_delay);
	if (ret) {
		pr_info("%s : pre_wc_afc_work_delay is Empty\n", __func__);
		pdata->pre_wc_afc_work_delay = 4000;
	}

	ret = of_property_read_u32(np, "battery,store_mode_afc_input_current",
		&pdata->store_mode_afc_input_current);
	if (ret) {
		pr_info("%s : store_mode_afc_input_current is Empty\n", __func__);
		pdata->store_mode_afc_input_current = 440;
	}

	ret = of_property_read_u32(np, "battery,store_mode_hv_wireless_input_current",
		&pdata->store_mode_hv_wireless_input_current);
	if (ret) {
		pr_info("%s : store_mode_hv_wireless_input_current is Empty\n", __func__);
		pdata->store_mode_hv_wireless_input_current = 400;
	}

	ret = of_property_read_u32(np, "battery,adc_check_count",
		&pdata->adc_check_count);
	if (ret)
		pr_info("%s : Adc check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_adc_type",
		&pdata->temp_adc_type);
	if (ret)
		pr_info("%s : Temp adc type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,cable_check_type",
		&pdata->cable_check_type);
	if (ret)
		pr_info("%s : Cable check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,cable_source_type",
		&pdata->cable_source_type);
	if (ret)
		pr_info("%s: Cable_source_type is Empty\n", __func__);
	ret = of_property_read_u32(np, "battery,polling_type",
		&pdata->polling_type);
	if (ret)
		pr_info("%s : Polling type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,monitor_initial_count",
		&pdata->monitor_initial_count);
	if (ret)
		pr_info("%s : Monitor initial count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,battery_check_type",
		&pdata->battery_check_type);
	if (ret)
		pr_info("%s : Battery check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,check_count",
		&pdata->check_count);
	if (ret)
		pr_info("%s : Check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,check_adc_max",
		&pdata->check_adc_max);
	if (ret)
		pr_info("%s : Check adc max is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,check_adc_min",
		&pdata->check_adc_min);
	if (ret)
		pr_info("%s : Check adc min is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,ovp_uvlo_check_type",
		&pdata->ovp_uvlo_check_type);
	if (ret)
		pr_info("%s : Ovp Uvlo check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_check_type",
		&pdata->temp_check_type);
	if (ret)
		pr_info("%s : Temp check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_check_count",
		&pdata->temp_check_count);
	if (ret)
		pr_info("%s : Temp check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_highlimit_threshold_normal",
				   &temp);
	pdata->temp_highlimit_threshold_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp highlimit threshold normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_highlimit_recovery_normal",
				   &temp);
	pdata->temp_highlimit_recovery_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp highlimit recovery normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_high_threshold_normal",
				   &temp);
	pdata->temp_high_threshold_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp high threshold normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_high_recovery_normal",
				   &temp);
	pdata->temp_high_recovery_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp high recovery normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_low_threshold_normal",
				   &temp);
	pdata->temp_low_threshold_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp low threshold normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_low_recovery_normal",
				   &temp);
	pdata->temp_low_recovery_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp low recovery normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_highlimit_threshold_lpm",
				   &temp);
	pdata->temp_highlimit_threshold_lpm = (int)temp;
	if (ret)
		pr_info("%s : Temp highlimit threshold lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_highlimit_recovery_lpm",
				   &temp);
	pdata->temp_highlimit_recovery_lpm = (int)temp;
	if (ret)
		pr_info("%s : Temp highlimit recovery lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_high_threshold_lpm",
				   &temp);
	pdata->temp_high_threshold_lpm = (int)temp;
	if (ret)
		pr_info("%s : Temp high threshold lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_high_recovery_lpm",
				   &temp);
	pdata->temp_high_recovery_lpm = (int)temp;
	if (ret)
		pr_info("%s : Temp high recovery lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_low_threshold_lpm",
				   &temp);
	pdata->temp_low_threshold_lpm = (int)temp;
	if (ret)
		pr_info("%s : Temp low threshold lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_low_recovery_lpm",
				   &temp);
	pdata->temp_low_recovery_lpm = (int)temp;
	if (ret)
		pr_info("%s : Temp low recovery lpm is Empty\n", __func__);

	pr_info("%s : HIGHLIMIT_THRESHOLD_NOLMAL(%d), HIGHLIMIT_RECOVERY_NORMAL(%d)\n"
		"HIGH_THRESHOLD_NORMAL(%d), HIGH_RECOVERY_NORMAL(%d) LOW_THRESHOLD_NORMAL(%d), LOW_RECOVERY_NORMAL(%d)\n"
		"HIGHLIMIT_THRESHOLD_LPM(%d), HIGHLIMIT_RECOVERY_LPM(%d)\n"
		"HIGH_THRESHOLD_LPM(%d), HIGH_RECOVERY_LPM(%d) LOW_THRESHOLD_LPM(%d), LOW_RECOVERY_LPM(%d)\n",
		__func__,
		pdata->temp_highlimit_threshold_normal, pdata->temp_highlimit_recovery_normal,
		pdata->temp_high_threshold_normal, pdata->temp_high_recovery_normal,
		pdata->temp_low_threshold_normal, pdata->temp_low_recovery_normal,
		pdata->temp_highlimit_threshold_lpm, pdata->temp_highlimit_recovery_lpm,
		pdata->temp_high_threshold_lpm, pdata->temp_high_recovery_lpm,
		pdata->temp_low_threshold_lpm, pdata->temp_low_recovery_lpm);

	ret = of_property_read_u32(np, "battery,wpc_high_threshold_normal",
				   &temp);
	pdata->wpc_high_threshold_normal =  (int)temp;
	if (ret)
		pr_info("%s : wpc_high_threshold_normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,wpc_high_recovery_normal",
				   &temp);
	pdata->wpc_high_recovery_normal =  (int)temp;
	if (ret)
		pr_info("%s : wpc_high_recovery_normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,wpc_low_threshold_normal",
				   &temp);
	pdata->wpc_low_threshold_normal =  (int)temp;
	if (ret)
		pr_info("%s : wpc_low_threshold_normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,wpc_low_recovery_normal",
				   &temp);
	pdata->wpc_low_recovery_normal =  (int)temp;
	if (ret)
		pr_info("%s : wpc_low_recovery_normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_type",
		&pdata->full_check_type);
	if (ret)
		pr_info("%s : Full check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_type_2nd",
		&pdata->full_check_type_2nd);
	if (ret)
		pr_info("%s : Full check type 2nd is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_count",
		&pdata->full_check_count);
	if (ret)
		pr_info("%s : Full check count is Empty\n", __func__);

        ret = of_property_read_u32(np, "battery,chg_gpio_full_check",
                &pdata->chg_gpio_full_check);
	if (ret)
		pr_info("%s : Chg gpio full check is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_polarity_full_check",
		&pdata->chg_polarity_full_check);
	if (ret)
		pr_info("%s : Chg polarity full check is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_condition_type",
		&pdata->full_condition_type);
	if (ret)
		pr_info("%s : Full condition type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_condition_soc",
		&pdata->full_condition_soc);
	if (ret)
		pr_info("%s : Full condition soc is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_condition_vcell",
		&pdata->full_condition_vcell);
	if (ret)
		pr_info("%s : Full condition vcell is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,recharge_check_count",
		&pdata->recharge_check_count);
	if (ret)
		pr_info("%s : Recharge check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,recharge_condition_type",
		&pdata->recharge_condition_type);
	if (ret)
		pr_info("%s : Recharge condition type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,recharge_condition_soc",
		&pdata->recharge_condition_soc);
	if (ret)
		pr_info("%s : Recharge condition soc is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,recharge_condition_vcell",
		&pdata->recharge_condition_vcell);
	if (ret)
		pr_info("%s : Recharge condition vcell is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,charging_total_time",
		(unsigned int *)&pdata->charging_total_time);
	if (ret)
		pr_info("%s : Charging total time is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,hv_charging_total_time",
				   &pdata->hv_charging_total_time);
	if (ret) {
		pdata->hv_charging_total_time = 3 * 60 * 60;
		pr_info("%s : HV Charging total time is %d\n",
			__func__, pdata->hv_charging_total_time);
	}

	ret = of_property_read_u32(np, "battery,normal_charging_total_time",
				   &pdata->normal_charging_total_time);
	if (ret) {
		pdata->normal_charging_total_time = 5 * 60 * 60;
		pr_info("%s : Normal(WC) Charging total time is %d\n",
			__func__, pdata->normal_charging_total_time);
	}

	ret = of_property_read_u32(np, "battery,usb_charging_total_time",
				   &pdata->usb_charging_total_time);
	if (ret) {
		pdata->usb_charging_total_time = 10 * 60 * 60;
		pr_info("%s : USB Charging total time is %d\n",
			__func__, pdata->usb_charging_total_time);
	}

	ret = of_property_read_u32(np, "battery,recharging_total_time",
		(unsigned int *)&pdata->recharging_total_time);
	if (ret)
		pr_info("%s : Recharging total time is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,charging_reset_time",
		(unsigned int *)&pdata->charging_reset_time);
	if (ret)
		pr_info("%s : Charging reset time is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,charging_reset_time",
		(unsigned int *)&pdata->charging_reset_time);
	if (ret)
		pr_info("%s : Charging reset time is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_float_voltage",
		(unsigned int *)&pdata->chg_float_voltage);
	if (ret) {
		pr_info("%s: chg_float_voltage is Empty\n", __func__);
		pdata->chg_float_voltage = 43500;
	}

	ret = of_property_read_u32(np, "battery,chg_float_voltage_conv",
				   &pdata->chg_float_voltage_conv);
	if (ret) {
		pr_info("%s: chg_float_voltage_conv is Empty\n", __func__);
		pdata->chg_float_voltage_conv = 1;
	}

	ret = of_property_read_u32(np, "battery,wa_volt_recov",
				   (unsigned int *)&pdata->wa_volt_recov);
	if (ret) {
		pdata->wa_volt_recov = 4090;
		pr_info("%s: wa volt recov is Empty\n", __func__);
	}

	ret = of_property_read_u32(np, "battery,wa_volt_thr",
				   (unsigned int *)&pdata->wa_volt_thr);
	if (ret) {
		pdata->wa_volt_thr = 4100;
		pr_info("%s: wa volt thr is Empty\n", __func__);
	}

	ret = of_property_read_u32(np, "battery,wa_float_voltage",
				   (unsigned int *)&pdata->wa_float_voltage);
	if (ret) {
		pdata->wa_float_voltage = 40500;
		pr_info("%s: wa float voltage is Empty\n", __func__);
	}

#if defined(CONFIG_BATTERY_SWELLING)
	ret = of_property_read_u32(np, "battery,chg_float_voltage",
		(unsigned int *)&pdata->swelling_normal_float_voltage);
	if (ret)
		pr_info("%s: chg_float_voltage is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,swelling_high_temp_block",
				   &temp);
	pdata->swelling_high_temp_block = (int)temp;
	if (ret)
		pr_info("%s: swelling high temp block is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,swelling_high_temp_recov",
				   &temp);
	pdata->swelling_high_temp_recov = (int)temp;
	if (ret)
		pr_info("%s: swelling high temp recovery is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,swelling_low_temp_block_1st",
				   &temp);
	pdata->swelling_low_temp_block_1st = (int)temp;
	if (ret)
		pr_info("%s: swelling low temp block is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,swelling_low_temp_recov_1st",
				   &temp);
	pdata->swelling_low_temp_recov_1st = (int)temp;
	if (ret)
		pr_info("%s: swelling low temp recovery is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,swelling_low_temp_block_2nd",
				   &temp);
	pdata->swelling_low_temp_block_2nd = (int)temp;
	if (ret)
		pr_info("%s: swelling low temp block is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,swelling_low_temp_recov_2nd",
				   &temp);
	pdata->swelling_low_temp_recov_2nd = (int)temp;
	if (ret)
		pr_info("%s: swelling low temp recovery 2nd is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,swelling_low_temp_current", 
					&pdata->swelling_low_temp_current);
	if (ret) {
		pr_info("%s: swelling_low_temp_current is Empty, Defualt value 600mA \n", __func__);
		pdata->swelling_low_temp_current = 600;
	}

	ret = of_property_read_u32(np, "battery,swelling_low_temp_topoff", 
					&pdata->swelling_low_temp_topoff);
	if (ret) {
		pr_info("%s: swelling_low_temp_topoff is Empty, Defualt value 200mA \n", __func__);
		pdata->swelling_low_temp_topoff = 200;
	}

	ret = of_property_read_u32(np, "battery,swelling_high_temp_current", 
					&pdata->swelling_high_temp_current);
	if (ret) {
		pr_info("%s: swelling_low_temp_current is Empty, Defualt value 1300mA \n", __func__);
		pdata->swelling_high_temp_current = 1300;
	}

	ret = of_property_read_u32(np, "battery,swelling_high_temp_topoff", 
					&pdata->swelling_high_temp_topoff);
	if (ret) {
		pr_info("%s: swelling_high_temp_topoff is Empty, Defualt value 200mA \n", __func__);
		pdata->swelling_high_temp_topoff = 200;
	}

	ret = of_property_read_u32(np, "battery,swelling_wc_high_temp_current", 
					&pdata->swelling_wc_high_temp_current);
	if (ret) {
		pr_info("%s: swelling_wc_high_temp_current is Empty, Defualt value 600mA \n", __func__);
		pdata->swelling_low_temp_current = 600;
	}

	ret = of_property_read_u32(np, "battery,swelling_wc_low_temp_current", 
					&pdata->swelling_wc_low_temp_current);
	if (ret) {
		pr_info("%s: swelling_wc_low_temp_current is Empty, Defualt value 600mA \n", __func__);
		pdata->swelling_low_temp_current = 600;
	}

	ret = of_property_read_u32(np, "battery,swelling_drop_float_voltage",
		(unsigned int *)&pdata->swelling_drop_float_voltage);
	if (ret) {
		pr_info("%s: swelling drop float voltage is Empty, Default value 4250mV \n", __func__);
		pdata->swelling_drop_float_voltage = 4250;
		pdata->swelling_drop_voltage_condition = 4250;
	} else {	
		pdata->swelling_drop_voltage_condition = (pdata->swelling_drop_float_voltage > 10000) ?
			(pdata->swelling_drop_float_voltage / 10) : (pdata->swelling_drop_float_voltage);
		pr_info("%s : swelling drop voltage(set : %d, condition : %d)\n", __func__,
			pdata->swelling_drop_float_voltage, pdata->swelling_drop_voltage_condition);
	}

	ret = of_property_read_u32(np, "battery,swelling_high_rechg_voltage",
		(unsigned int *)&pdata->swelling_high_rechg_voltage);
	if (ret) {
		pr_info("%s: swelling_high_rechg_voltage is Empty\n", __func__);
		pdata->swelling_high_rechg_voltage = 4150;
	}

	ret = of_property_read_u32(np, "battery,swelling_low_rechg_voltage",
		(unsigned int *)&pdata->swelling_low_rechg_voltage);
	if (ret) {
		pr_info("%s: swelling_low_rechg_voltage is Empty\n", __func__);
				pdata->swelling_low_rechg_voltage = 4050;
	}

	pr_info("%s : SWELLING_HIGH_TEMP(%d) SWELLING_HIGH_TEMP_RECOVERY(%d)\n"
		"SWELLING_LOW_TEMP_1st(%d) SWELLING_LOW_TEMP_RECOVERY_1st(%d) "
		"SWELLING_LOW_TEMP_2nd(%d) SWELLING_LOW_TEMP_RECOVERY_2nd(%d) "
		"SWELLING_LOW_CURRENT(%d, %d), SWELLING_HIGH_CURRENT(%d, %d)\n",
		__func__, pdata->swelling_high_temp_block, pdata->swelling_high_temp_recov,
		pdata->swelling_low_temp_block_1st, pdata->swelling_low_temp_recov_1st,
		pdata->swelling_low_temp_block_2nd, pdata->swelling_low_temp_recov_2nd,
		pdata->swelling_low_temp_current, pdata->swelling_low_temp_topoff,
		pdata->swelling_high_temp_current, pdata->swelling_high_temp_topoff);
#endif

#if defined(CONFIG_CALC_TIME_TO_FULL)
	ret = of_property_read_u32(np, "battery,ttf_hv_12v_charge_current", 
					&pdata->ttf_hv_12v_charge_current);
	if (ret) {
		pdata->ttf_hv_12v_charge_current =
			pdata->charging_current[SEC_BATTERY_CABLE_12V_TA].fast_charging_current;
		pr_info("%s: ttf_hv_12v_charge_current is Empty, Defualt value %d \n",
			__func__, pdata->ttf_hv_12v_charge_current);
	}
	ret = of_property_read_u32(np, "battery,ttf_hv_charge_current", 
					&pdata->ttf_hv_charge_current);
	if (ret) {
		pdata->ttf_hv_charge_current =
			pdata->charging_current[SEC_BATTERY_CABLE_9V_TA].fast_charging_current;
		pr_info("%s: ttf_hv_charge_current is Empty, Defualt value %d \n",
			__func__, pdata->ttf_hv_charge_current);
	}

	ret = of_property_read_u32(np, "battery,ttf_hv_wireless_charge_current", 
					&pdata->ttf_hv_wireless_charge_current);
	if (ret) {
		pr_info("%s: ttf_hv_wireless_charge_current is Empty, Defualt value 0 \n", __func__);
		pdata->ttf_hv_wireless_charge_current =
			pdata->charging_current[SEC_BATTERY_CABLE_HV_WIRELESS].fast_charging_current - 300;
	}

	ret = of_property_read_u32(np, "battery,ttf_wireless_charge_current", 
					&pdata->ttf_wireless_charge_current);
	if (ret) {
		pr_info("%s: ttf_wireless_charge_current is Empty, Defualt value 0 \n", __func__);
		pdata->ttf_wireless_charge_current =
			pdata->charging_current[SEC_BATTERY_CABLE_WIRELESS].input_current_limit;
	}
#endif

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	/* wpc_det */
	ret = pdata->wpc_det = of_get_named_gpio(np, "battery,wpc_det", 0);
	if (ret < 0) {
		pr_info("%s : can't get wpc_det\n", __func__);
	}
#endif

	/* wpc_en */
	ret = pdata->wpc_en = of_get_named_gpio(np, "battery,wpc_en", 0);
	if (ret < 0) {
		pr_info("%s : can't get wpc_en\n", __func__);
	}
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	p = of_get_property(np, "battery,age_data", &len);
	if (p) {
		battery->pdata->num_age_step = len / sizeof(sec_age_data_t);
		battery->pdata->age_data = kzalloc(len, GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,age_data",
				 (u32 *)battery->pdata->age_data, len/sizeof(u32));
		if (ret) {
			pr_err("%s failed to read battery->pdata->age_data: %d\n",
					__func__, ret);
			kfree(battery->pdata->age_data);
			battery->pdata->age_data = NULL;
			battery->pdata->num_age_step = 0;
		}
		pr_err("%s num_age_step : %d\n", __func__, battery->pdata->num_age_step);
#if defined(CONFIG_BATTERY_AGE_FORECAST_B2B)
		for (len = 0; len < battery->pdata->num_age_step; ++len) {
			pr_err("[%d/%d]cycle:%d, float:%d, full_v:%d, recharge_v:%d, soc:%d, max_c:%d\n",
				len, battery->pdata->num_age_step-1,
				battery->pdata->age_data[len].cycle,
				battery->pdata->age_data[len].float_voltage,
				battery->pdata->age_data[len].full_condition_vcell,
				battery->pdata->age_data[len].recharge_condition_vcell,
				battery->pdata->age_data[len].full_condition_soc,
				battery->pdata->age_data[len].max_charging_current);
		}
#else
		for (len = 0; len < battery->pdata->num_age_step; ++len) {
			pr_err("[%d/%d]cycle:%d, float:%d, full_v:%d, recharge_v:%d, soc:%d\n",
				len, battery->pdata->num_age_step-1,
				battery->pdata->age_data[len].cycle,
				battery->pdata->age_data[len].float_voltage,
				battery->pdata->age_data[len].full_condition_vcell,
				battery->pdata->age_data[len].recharge_condition_vcell,
				battery->pdata->age_data[len].full_condition_soc);
		}
#endif
	} else {
		battery->pdata->num_age_step = 0;
		pr_err("%s there is not age_data\n", __func__);
	}
#endif

	ret = of_property_read_u32(np, "battery,siop_event_check_type",
			&pdata->siop_event_check_type);
	ret = of_property_read_u32(np, "battery,siop_call_cc_current",
			&pdata->siop_call_cc_current);
	ret = of_property_read_u32(np, "battery,siop_call_cv_current",
			&pdata->siop_call_cv_current);

	ret = of_property_read_u32(np, "battery,siop_input_limit_current",
			&pdata->siop_input_limit_current);
	if (ret)
		pdata->siop_input_limit_current = SIOP_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_charging_limit_current",
			&pdata->siop_charging_limit_current);
	if (ret)
		pdata->siop_charging_limit_current = SIOP_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_12v_input_limit_current",
			&pdata->siop_hv_12v_input_limit_current);
	if (ret)
		pdata->siop_hv_12v_input_limit_current = SIOP_HV_12V_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_12v_charging_limit_current",
			&pdata->siop_hv_12v_charging_limit_current);
	if (ret)
		pdata->siop_hv_12v_charging_limit_current = SIOP_HV_12V_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_input_limit_current",
			&pdata->siop_hv_input_limit_current);
	if (ret)
		pdata->siop_hv_input_limit_current = SIOP_HV_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_charging_limit_current",
			&pdata->siop_hv_charging_limit_current);
	if (ret)
		pdata->siop_hv_charging_limit_current = SIOP_HV_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_wireless_input_limit_current",
			&pdata->siop_wireless_input_limit_current);
	if (ret)
		pdata->siop_wireless_input_limit_current = SIOP_WIRELESS_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_wireless_charging_limit_current",
			&pdata->siop_wireless_charging_limit_current);
	if (ret)
		pdata->siop_wireless_charging_limit_current = SIOP_WIRELESS_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_wireless_input_limit_current",
			&pdata->siop_hv_wireless_input_limit_current);
	if (ret)
		pdata->siop_hv_wireless_input_limit_current = SIOP_HV_WIRELESS_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_wireless_charging_limit_current",
			&pdata->siop_hv_wireless_charging_limit_current);
	if (ret)
		pdata->siop_hv_wireless_charging_limit_current = SIOP_HV_WIRELESS_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,max_input_voltage",
			&pdata->max_input_voltage);
	if (ret)
		pdata->max_input_voltage = 12000;

	ret = of_property_read_u32(np, "battery,max_input_current",
			&pdata->max_input_current);
	if (ret)
		pdata->max_input_current = 3000;

	ret = of_property_read_u32(np, "battery,pd_charging_charge_power",
			&pdata->pd_charging_charge_power);
	if (ret) {
		pr_err("%s: pd_charging_charge_power is Empty\n", __func__);
		pdata->pd_charging_charge_power = 15000;
	}

	pr_info("%s: vendor : %s, technology : %d, cable_check_type : %d\n"
		"cable_source_type : %d, event_waiting_time : %d\n"
		"polling_type : %d, initial_count : %d, check_count : %d\n"
		"check_adc_max : %d, check_adc_min : %d\n"
		"ovp_uvlo_check_type : %d, thermal_source : %d\n"
		"temp_check_type : %d, temp_check_count : %d\n",
		__func__,
		pdata->vendor, pdata->technology,pdata->cable_check_type,
		pdata->cable_source_type, pdata->event_waiting_time,
		pdata->polling_type, pdata->monitor_initial_count,
		pdata->check_count, pdata->check_adc_max, pdata->check_adc_min,
		pdata->ovp_uvlo_check_type, pdata->thermal_source,
		pdata->temp_check_type, pdata->temp_check_count);
#if defined(CONFIG_SEC_FACTORY)
	ret = of_property_read_u32(np, "battery,factory_store_mode_max",
		&pdata->store_mode_max);
	if (ret) {
		pdata->store_mode_max = STORE_MODE_CHARGING_MAX;
		pr_info("%s : factory_store_mode_max is Empty\n", __func__);
	} else {
		pr_info("%s : factory_store_mode_max : %d\n", __func__, pdata->store_mode_max);
	}

	ret = of_property_read_u32(np, "battery,factory_store_mode_min",
		&pdata->store_mode_min);
	if (ret) {
		pdata->store_mode_min = STORE_MODE_CHARGING_MIN;
		pr_info("%s : factory_store_mode_min is Empty\n", __func__);
	} else {
		pr_info("%s : factory_store_mode_min : %d\n", __func__, pdata->store_mode_min);
	}
#else /* normal bin */
	pdata->store_mode_max = STORE_MODE_CHARGING_MAX;
	pdata->store_mode_min = STORE_MODE_CHARGING_MIN;
#endif
#if defined(CONFIG_STEP_CHARGING)
	sec_step_charging_init(battery, dev);
#else
	ret = of_property_read_u32(np, "battery,max_charging_current",
			&pdata->max_charging_current);
	if (ret) {
		pr_err("%s: max_charging_current is Empty\n", __func__);
		pdata->max_charging_current = 3000;
	}

#endif
	return 0;
}
#endif

#ifdef CONFIG_OF
extern sec_battery_platform_data_t sec_battery_pdata;
#endif

#if !defined(CONFIG_MUIC_NOTIFIER)
static void cable_initial_check(struct sec_battery_info *battery)
{
	union power_supply_propval value;

	pr_info("%s : current_cable_type : (%d)\n", __func__, battery->cable_type);

	if (SEC_BATTERY_CABLE_NONE !=  battery->cable_type) {
		if (battery->cable_type == SEC_BATTERY_CABLE_POWER_SHARING) {
			value.intval =  battery->cable_type;
			psy_do_property("ps", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		} else {
			value.intval =  battery->cable_type;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
	} else {
		psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == SEC_BATTERY_CABLE_WIRELESS) {
			value.intval = 1;
			psy_do_property("wireless", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		}
	}
}
#endif

static int sec_battery_probe(struct platform_device *pdev)
{
	sec_battery_platform_data_t *pdata = NULL;
	struct sec_battery_info *battery;
	int ret = 0;
#ifndef CONFIG_OF
	int i = 0;
#endif

	union power_supply_propval value = {0, };

	dev_info(&pdev->dev,
		"%s: SEC Battery Driver Loading\n", __func__);

	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(sec_battery_platform_data_t),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_bat_free;
		}

		battery->pdata = pdata;

		if (sec_bat_parse_dt(&pdev->dev, battery)) {
			dev_err(&pdev->dev,
				"%s: Failed to get battery dt\n", __func__);
			ret = -EINVAL;
			goto err_bat_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		battery->pdata = pdata;
	}

	platform_set_drvdata(pdev, battery);

	battery->dev = &pdev->dev;

	mutex_init(&battery->adclock);
	mutex_init(&battery->iolock);
	mutex_init(&battery->misclock);
	mutex_init(&battery->batt_handlelock);
	mutex_init(&battery->current_eventlock);
	mutex_init(&battery->typec_notylock);
#if defined(CONFIG_BATTERY_SBM_DATA)
	mutex_init(&battery->sbmlock);
	sec_bat_init_sbm(battery);
#endif

	dev_dbg(battery->dev, "%s: ADC init\n", __func__);

#ifdef CONFIG_OF
	adc_init(pdev, battery);
#else
	for (i = 0; i < SEC_BAT_ADC_CHANNEL_NUM; i++)
		adc_init(pdev, pdata, i);
#endif
	wake_lock_init(&battery->monitor_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-monitor");
	wake_lock_init(&battery->cable_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-cable");
	wake_lock_init(&battery->vbus_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-vbus");
	wake_lock_init(&battery->afc_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-afc");
	wake_lock_init(&battery->siop_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-siop");
	wake_lock_init(&battery->siop_level_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-siop_level");
	wake_lock_init(&battery->siop_event_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-siop_event");
	wake_lock_init(&battery->wc_headroom_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-wc_headroom");
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	wake_lock_init(&battery->batt_data_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-update-data");
#endif
	wake_lock_init(&battery->misc_event_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-misc-event");

	/* initialization of battery info */
	sec_bat_set_charging_status(battery,
			POWER_SUPPLY_STATUS_DISCHARGING);
	battery->health = POWER_SUPPLY_HEALTH_GOOD;
	battery->present = true;
	battery->is_jig_on = false;
	battery->wdt_kick_disable = 0;

	battery->polling_count = 1;	/* initial value = 1 */
	battery->polling_time = pdata->polling_time[
		SEC_BATTERY_POLLING_TIME_DISCHARGING];
	battery->polling_in_sleep = false;
	battery->polling_short = false;

	battery->check_count = 0;
	battery->check_adc_count = 0;
	battery->check_adc_value = 0;

	battery->input_current = 0;
	battery->charging_current = 0;
	battery->topoff_current = 0;
	battery->wpc_vout_level = WIRELESS_VOUT_5V;
	battery->charging_start_time = 0;
	battery->charging_passed_time = 0;
	battery->wc_heating_start_time = 0;
	battery->wc_heating_passed_time = 0;
	battery->charging_next_time = 0;
	battery->charging_fullcharged_time = 0;
	battery->siop_level = 100;
	battery->siop_event = 0;
	battery->wc_enable = 1;
	battery->wc_enable_cnt = 0;
	battery->wc_enable_cnt_value = 3;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	battery->stability_test = 0;
	battery->eng_not_full_status = 0;
	battery->temperature_test_battery = 0x7FFF;
	battery->temperature_test_usb = 0x7FFF;
	battery->temperature_test_wpc = 0x7FFF;
	battery->temperature_test_chg = 0x7FFF;
#endif
	battery->ps_enable = false;
    battery->wc_status = SEC_WIRELESS_PAD_NONE;
	battery->wc_cv_mode = false;
	battery->wire_status = SEC_BATTERY_CABLE_NONE;

#if defined(CONFIG_BATTERY_SWELLING)
	battery->swelling_mode = SWELLING_MODE_NONE;
#endif
	battery->charging_block = false;
	battery->chg_limit = false;
	battery->mix_limit = false;
	battery->vbus_limit = false;
	battery->usb_temp = 0;
#if defined(CONFIG_ENG_BATTERY_CONCEPT) || defined(CONFIG_SEC_FACTORY)
	battery->cooldown_mode = true;
#endif
	battery->skip_swelling = false;
	battery->hiccup_status = 0;
	sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_USB_100MA, SEC_BAT_CURRENT_EVENT_USB_100MA);

	if (lpcharge) {
		battery->temp_highlimit_threshold =
			battery->pdata->temp_highlimit_threshold_lpm;
		battery->temp_highlimit_recovery =
			battery->pdata->temp_highlimit_recovery_lpm;
		battery->temp_high_threshold =
			battery->pdata->temp_high_threshold_lpm;
		battery->temp_high_recovery =
			battery->pdata->temp_high_recovery_lpm;
		battery->temp_low_recovery =
			battery->pdata->temp_low_recovery_lpm;
		battery->temp_low_threshold =
			battery->pdata->temp_low_threshold_lpm;
	} else {
		battery->temp_highlimit_threshold =
			battery->pdata->temp_highlimit_threshold_normal;
		battery->temp_highlimit_recovery =
			battery->pdata->temp_highlimit_recovery_normal;
		battery->temp_high_threshold =
			battery->pdata->temp_high_threshold_normal;
		battery->temp_high_recovery =
			battery->pdata->temp_high_recovery_normal;
		battery->temp_low_recovery =
			battery->pdata->temp_low_recovery_normal;
		battery->temp_low_threshold =
			battery->pdata->temp_low_threshold_normal;
	}

	battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
	battery->is_recharging = false;
	battery->cable_type = SEC_BATTERY_CABLE_NONE;
	battery->test_mode = 0;
	battery->factory_mode = false;
	battery->store_mode = false;
	battery->slate_mode = false;
	battery->is_hc_usb = false;
	battery->is_sysovlo = false;
	battery->is_vbatovlo = false;
	battery->is_abnormal_temp = false;

	battery->safety_timer_set = true;
	battery->stop_timer = false;
	battery->prev_safety_time = 0;
	battery->lcd_status = false;

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	battery->batt_cycle = -1;
	battery->pdata->age_step = 0;
#endif

	battery->health_change = false;

	if(charging_night_mode == 49)
		sleep_mode = true;
	else
		sleep_mode = false;

#if defined(CONFIG_BATTERY_SBM_DATA)
	battery->sbm_data = false;
#endif

	if (battery->pdata->charger_name == NULL)
		battery->pdata->charger_name = "sec-charger";
	if (battery->pdata->fuelgauge_name == NULL)
		battery->pdata->fuelgauge_name = "sec-fuelgauge";

	battery->psy_bat.name = "battery",
	battery->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY,
	battery->psy_bat.properties = sec_battery_props,
	battery->psy_bat.num_properties = ARRAY_SIZE(sec_battery_props),
	battery->psy_bat.get_property = sec_bat_get_property,
	battery->psy_bat.set_property = sec_bat_set_property,
	battery->psy_usb.name = "usb",
	battery->psy_usb.type = POWER_SUPPLY_TYPE_USB,
	battery->psy_usb.supplied_to = supply_list,
	battery->psy_usb.num_supplicants = ARRAY_SIZE(supply_list),
	battery->psy_usb.properties = sec_power_props,
	battery->psy_usb.num_properties = ARRAY_SIZE(sec_power_props),
	battery->psy_usb.get_property = sec_usb_get_property,
	battery->psy_ac.name = "ac",
	battery->psy_ac.type = POWER_SUPPLY_TYPE_MAINS,
	battery->psy_ac.supplied_to = supply_list,
	battery->psy_ac.num_supplicants = ARRAY_SIZE(supply_list),
	battery->psy_ac.properties = sec_ac_props,
	battery->psy_ac.num_properties = ARRAY_SIZE(sec_ac_props),
	battery->psy_ac.get_property = sec_ac_get_property;
	battery->psy_wireless.name = "wireless",
	battery->psy_wireless.type = POWER_SUPPLY_TYPE_WIRELESS,
	battery->psy_wireless.supplied_to = supply_list,
	battery->psy_wireless.num_supplicants = ARRAY_SIZE(supply_list),
	battery->psy_wireless.properties = sec_power_props,
	battery->psy_wireless.num_properties = ARRAY_SIZE(sec_power_props),
	battery->psy_wireless.get_property = sec_wireless_get_property;
	battery->psy_wireless.set_property = sec_wireless_set_property;
	battery->psy_ps.name = "ps",
	battery->psy_ps.type = POWER_SUPPLY_TYPE_POWER_SHARING,
	battery->psy_ps.supplied_to = supply_list,
	battery->psy_ps.num_supplicants = ARRAY_SIZE(supply_list),
	battery->psy_ps.properties = sec_ps_props,
	battery->psy_ps.num_properties = ARRAY_SIZE(sec_ps_props),
	battery->psy_ps.get_property = sec_ps_get_property;
	battery->psy_ps.set_property = sec_ps_set_property;

	/* create work queue */
	battery->monitor_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!battery->monitor_wqueue) {
		dev_err(battery->dev,
			"%s: Fail to Create Workqueue\n", __func__);
		goto err_irq;
	}

	INIT_DELAYED_WORK(&battery->monitor_work, sec_bat_monitor_work);
	INIT_DELAYED_WORK(&battery->cable_work, sec_bat_cable_work);
#if defined(CONFIG_CALC_TIME_TO_FULL)
	INIT_DELAYED_WORK(&battery->timetofull_work, sec_bat_time_to_full_work);
#endif
	INIT_DELAYED_WORK(&battery->slowcharging_work, sec_bat_check_slowcharging_work);
	INIT_DELAYED_WORK(&battery->afc_work, sec_bat_afc_work);
	INIT_DELAYED_WORK(&battery->siop_work, sec_bat_siop_work);
	INIT_DELAYED_WORK(&battery->siop_event_work, sec_bat_siop_event_work);
	INIT_DELAYED_WORK(&battery->siop_level_work, sec_bat_siop_level_work);
	INIT_DELAYED_WORK(&battery->wc_headroom_work, sec_bat_wc_headroom_work);
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	INIT_DELAYED_WORK(&battery->fw_init_work, sec_bat_fw_init_work);
#endif
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	INIT_DELAYED_WORK(&battery->batt_data_work, sec_bat_update_data_work);
#endif
	INIT_DELAYED_WORK(&battery->misc_event_work, sec_bat_misc_event_work);

	switch (pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		INIT_DELAYED_WORK(&battery->polling_work,
			sec_bat_polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		battery->last_poll_time = ktime_get_boottime();
		alarm_init(&battery->polling_alarm, ALARM_BOOTTIME,
			sec_bat_alarm);
		break;
	default:
		break;
	}

#if defined(CONFIG_BATTERY_CISD)
	sec_battery_cisd_init(battery);
#endif

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &battery->psy_ps);
	if (ret) {
		dev_err(battery->dev,
				"%s: Failed to Register psy_ps\n", __func__);
		goto err_workqueue;
	}

	ret = power_supply_register(&pdev->dev, &battery->psy_wireless);
	if (ret) {
		dev_err(battery->dev,
				"%s: Failed to Register psy_wireless\n", __func__);
		goto err_supply_unreg_ps;
	}

	ret = power_supply_register(&pdev->dev, &battery->psy_usb);
	if (ret) {
		dev_err(battery->dev,
				"%s: Failed to Register psy_usb\n", __func__);
		goto err_supply_unreg_wireless;
	}

	ret = power_supply_register(&pdev->dev, &battery->psy_ac);
	if (ret) {
		dev_err(battery->dev,
				"%s: Failed to Register psy_ac\n", __func__);
		goto err_supply_unreg_usb;
	}

	ret = power_supply_register(&pdev->dev, &battery->psy_bat);
	if (ret) {
		dev_err(battery->dev,
				"%s: Failed to Register psy_bat\n", __func__);
		goto err_supply_unreg_ac;
	}

	ret = sec_bat_create_attrs(battery->psy_bat.dev);
	if (ret) {
		dev_err(battery->dev,
			"%s : Failed to create_attrs\n", __func__);
		goto err_req_irq;
	}

	/* initialize battery level*/
	value.intval = 0;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
	battery->capacity = value.intval;

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	/* queue_delayed_work(battery->monitor_wqueue, &battery->fw_init_work, 0); */
#endif

	value.intval = 0;
	psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_TYPE, value);

#if defined(CONFIG_STORE_MODE) && !defined(CONFIG_SEC_FACTORY)
	battery->store_mode = true;
#endif

#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	battery->pdic_info.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	manager_notifier_register(&battery->usb_typec_nb,
		usb_typec_handle_notification, MANAGER_NOTIFY_CCIC_BATTERY);
#else
#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&battery->batt_nb,
       batt_handle_notification, MUIC_NOTIFY_DEV_CHARGER);
#else
	cable_initial_check(battery);
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
	pr_info("%s: Registering PDIC_NOTIFY.\n", __func__);
	pdic_notifier_register(&battery->pdic_nb,
		batt_pdic_handle_notification, PDIC_NOTIFY_DEV_BATTERY);
#endif
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&battery->vbus_nb,
		vbus_handle_notification, VBUS_NOTIFY_DEV_CHARGER);
#endif

	value.intval = true;
	psy_do_property(battery->pdata->charger_name, set,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_ONLINE, value);

	if ((value.intval == SEC_BATTERY_CABLE_NONE) ||
			(value.intval == SEC_BATTERY_CABLE_PREPARE_TA)) {
		dev_info(&pdev->dev,
				"%s: SEC Battery Driver Monitorwork\n", __func__);
		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	}

	if (battery->pdata->check_battery_callback)
		battery->present = battery->pdata->check_battery_callback();

	dev_info(battery->dev,
		"%s: SEC Battery Driver Loaded\n", __func__);
	return 0;

err_req_irq:
	if (battery->pdata->bat_irq)
		free_irq(battery->pdata->bat_irq, battery);
	power_supply_unregister(&battery->psy_bat);
err_supply_unreg_ac:
	power_supply_unregister(&battery->psy_ac);
err_supply_unreg_usb:
	power_supply_unregister(&battery->psy_usb);
err_supply_unreg_wireless:
	power_supply_unregister(&battery->psy_wireless);
err_supply_unreg_ps:
	power_supply_unregister(&battery->psy_ps);
err_workqueue:
	destroy_workqueue(battery->monitor_wqueue);
err_irq:
	wake_lock_destroy(&battery->monitor_wake_lock);
	wake_lock_destroy(&battery->cable_wake_lock);
	wake_lock_destroy(&battery->vbus_wake_lock);
	wake_lock_destroy(&battery->afc_wake_lock);
	wake_lock_destroy(&battery->siop_wake_lock);
	wake_lock_destroy(&battery->siop_level_wake_lock);
	wake_lock_destroy(&battery->siop_event_wake_lock);
	wake_lock_destroy(&battery->wc_headroom_wake_lock);
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	wake_lock_destroy(&battery->batt_data_wake_lock);
#endif
	wake_lock_destroy(&battery->misc_event_wake_lock);
	mutex_destroy(&battery->adclock);
	mutex_destroy(&battery->iolock);
	mutex_destroy(&battery->misclock);
	mutex_destroy(&battery->batt_handlelock);
	mutex_destroy(&battery->current_eventlock);
	mutex_destroy(&battery->typec_notylock);
#if defined(CONFIG_BATTERY_SBM_DATA)
	mutex_destroy(&battery->sbmlock);
	sec_bat_exit_sbm(battery);
#endif	
	kfree(pdata);
err_bat_free:
	kfree(battery);

	return ret;
}

static int sec_battery_remove(struct platform_device *pdev)
{
	struct sec_battery_info *battery = platform_get_drvdata(pdev);
#ifndef CONFIG_OF
	int i;
#endif

	dev_dbg(battery->dev, "%s: Start\n", __func__);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		cancel_delayed_work(&battery->polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		alarm_cancel(&battery->polling_alarm);
		break;
	default:
		break;
	}

	flush_workqueue(battery->monitor_wqueue);
	destroy_workqueue(battery->monitor_wqueue);
	wake_lock_destroy(&battery->monitor_wake_lock);
	wake_lock_destroy(&battery->cable_wake_lock);
	wake_lock_destroy(&battery->vbus_wake_lock);
	wake_lock_destroy(&battery->afc_wake_lock);
	wake_lock_destroy(&battery->siop_wake_lock);
	wake_lock_destroy(&battery->siop_level_wake_lock);
	wake_lock_destroy(&battery->siop_event_wake_lock);
	wake_lock_destroy(&battery->misc_event_wake_lock);
	mutex_destroy(&battery->adclock);
	mutex_destroy(&battery->iolock);
	mutex_destroy(&battery->misclock);
	mutex_destroy(&battery->batt_handlelock);
	mutex_destroy(&battery->current_eventlock);
	mutex_destroy(&battery->typec_notylock);

#if defined(CONFIG_BATTERY_SBM_DATA)
	mutex_destroy(&battery->sbmlock);
	sec_bat_exit_sbm(battery);
#endif

#ifdef CONFIG_OF
	adc_exit(battery);
#else
	for (i = 0; i < SEC_BAT_ADC_CHANNEL_NUM; i++)
		adc_exit(battery->pdata, i);
#endif
	power_supply_unregister(&battery->psy_ps);
	power_supply_unregister(&battery->psy_wireless);
	power_supply_unregister(&battery->psy_ac);
	power_supply_unregister(&battery->psy_usb);
	power_supply_unregister(&battery->psy_bat);

	dev_dbg(battery->dev, "%s: End\n", __func__);
	kfree(battery);

	return 0;
}

static int sec_battery_prepare(struct device *dev)
{
	struct sec_battery_info *battery
		= dev_get_drvdata(dev);

	dev_info(battery->dev, "%s: Start\n", __func__);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		cancel_delayed_work(&battery->polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		alarm_cancel(&battery->polling_alarm);
		break;
	default:
		break;
	}

	/* monitor_wake_lock should be unlocked before cancle monitor_work */
	wake_unlock(&battery->monitor_wake_lock);
	cancel_delayed_work_sync(&battery->monitor_work);

	battery->polling_in_sleep = true;

	sec_bat_set_polling(battery);

	/* cancel work for polling
	 * that is set in sec_bat_set_polling()
	 * no need for polling in sleep
	 */
	if (battery->pdata->polling_type ==
		SEC_BATTERY_MONITOR_WORKQUEUE)
		cancel_delayed_work(&battery->polling_work);

	dev_info(battery->dev, "%s: End\n", __func__);

	return 0;
}

static int sec_battery_suspend(struct device *dev)
{
	return 0;
}

static int sec_battery_resume(struct device *dev)
{
	return 0;
}

static void sec_battery_complete(struct device *dev)
{
	struct sec_battery_info *battery
		= dev_get_drvdata(dev);

	dev_info(battery->dev, "%s: Start\n", __func__);

	/* cancel current alarm and reset after monitor work */
	if (battery->pdata->polling_type == SEC_BATTERY_MONITOR_ALARM)
		alarm_cancel(&battery->polling_alarm);

	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue,
		&battery->monitor_work, 0);

	dev_info(battery->dev, "%s: End\n", __func__);

	return;
}

static void sec_battery_shutdown(struct platform_device *pdev)
{
	struct sec_battery_info *battery
		= platform_get_drvdata(pdev);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		cancel_delayed_work(&battery->polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		alarm_cancel(&battery->polling_alarm);
		break;
	default:
		break;
	}
}

#ifdef CONFIG_OF
static struct of_device_id sec_battery_dt_ids[] = {
	{ .compatible = "samsung,sec-battery" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_battery_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_battery_pm_ops = {
	.prepare = sec_battery_prepare,
	.suspend = sec_battery_suspend,
	.resume = sec_battery_resume,
	.complete = sec_battery_complete,
};

static struct platform_driver sec_battery_driver = {
	.driver = {
		   .name = "sec-battery",
		   .owner = THIS_MODULE,
		   .pm = &sec_battery_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = sec_battery_dt_ids,
#endif
	},
	.probe = sec_battery_probe,
	.remove = sec_battery_remove,
	.shutdown = sec_battery_shutdown,
};

static int __init sec_battery_init(void)
{
	return platform_driver_register(&sec_battery_driver);
}

static void __exit sec_battery_exit(void)
{
	platform_driver_unregister(&sec_battery_driver);
}

late_initcall(sec_battery_init);
module_exit(sec_battery_exit);

MODULE_DESCRIPTION("Samsung Battery Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
