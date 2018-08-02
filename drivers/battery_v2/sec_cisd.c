/*
 *  sec_cisd.c
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
#include "include/sec_cisd.h"

bool sec_bat_cisd_check(struct sec_battery_info *battery)
{
	union power_supply_propval incur_val = {0, };
	union power_supply_propval chgcur_val = {0, };
	union power_supply_propval capcurr_val = {0, };
	union power_supply_propval vbat_val = {0, };
	struct cisd *pcisd = &battery->cisd;
	struct timespec now_ts;
	bool ret = false;
	static int prev_fullcap_rep;
#if defined(CONFIG_QH_ALGORITHM)
	union power_supply_propval qh_val = {0, };
	union power_supply_propval vfsoc_val = {0, };
	union power_supply_propval fullcap_nom_val = {0, };
	struct timeval cur_time = {0, 0};
	struct tm cur_date;
#endif

	if (battery->factory_mode || battery->is_jig_on) {
		dev_dbg(battery->dev, "%s: No need to check in factory mode\n",
			__func__);
		return ret;
	}

	if ((battery->status == POWER_SUPPLY_STATUS_CHARGING) ||
		(battery->status == POWER_SUPPLY_STATUS_FULL)) {
		/* charging */
#if defined(CONFIG_QH_ALGORITHM)
		do_gettimeofday(&cur_time);

		qh_val.intval = SEC_BATTERY_CAPACITY_QH;
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_ENERGY_NOW, qh_val);
		pcisd->qh_value_now =  qh_val.intval;

		vfsoc_val.intval = SEC_BATTERY_CAPACITY_VFSOC;
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_ENERGY_NOW, vfsoc_val);
		pcisd->qh_vfsoc_now = vfsoc_val.intval;

		pr_info("%s: QH_VALUE_NOW(0~65535): %d, QH_VFSOC_NOW(0.001%%): %d\n", __func__,
				pcisd->qh_value_now, pcisd->qh_vfsoc_now);             

		if (!pcisd->prev_qh_value &&
				battery->current_now > battery->cisd_qh_current_low_thr &&
				battery->current_avg > battery->cisd_qh_current_low_thr &&
				battery->current_now <= battery->cisd_qh_current_high_thr &&
				battery->current_avg <= battery->cisd_qh_current_high_thr &&
				vfsoc_val.intval >= 101000) {
			pr_info("%s: Set reference QH value\n", __func__);

			pcisd->prev_qh_value = qh_val.intval;
			pcisd->prev_qh_vfsoc = vfsoc_val.intval;
			pcisd->prev_time = cur_time.tv_sec;
			battery->qh_start = true;

			time_to_tm(cur_time.tv_sec, 0, &cur_date);
			pr_info("%s: Start time = %ldY %dM %dD %dH %dM %dS, QH value : %d, VFSOC : %d\n",
					__func__,
					cur_date.tm_year + 1900, cur_date.tm_mon + 1,
					cur_date.tm_mday, cur_date.tm_hour,
					cur_date.tm_min, cur_date.tm_sec, pcisd->prev_qh_value, pcisd->prev_qh_vfsoc);
		} else if (pcisd->prev_qh_value && !battery->qh_start &&
				(cur_time.tv_sec - pcisd->prev_time >= battery->cisd.qh_valid_time) &&
				battery->current_now > battery->cisd_qh_current_low_thr &&
				battery->current_avg > battery->cisd_qh_current_low_thr &&
				battery->current_now <= battery->cisd_qh_current_high_thr &&
				battery->current_avg <= battery->cisd_qh_current_high_thr &&
				vfsoc_val.intval >= 101000) {
			int value, delta;

			qh_val.intval = SEC_BATTERY_CAPACITY_QH;
			psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_ENERGY_NOW, qh_val);

			fullcap_nom_val.intval = SEC_BATTERY_CAPACITY_AGEDCELL;
			psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_ENERGY_NOW, fullcap_nom_val);

			if (qh_val.intval & 0x8000)
				qh_val.intval = qh_val.intval - 0xFFFF;

			if (pcisd->prev_qh_value & 0x8000)
				pcisd->prev_qh_value = pcisd->prev_qh_value - 0xFFFF;

			delta = qh_val.intval - pcisd->prev_qh_value;


			value = (((delta * 1000) -
						(vfsoc_val.intval - pcisd->prev_qh_vfsoc) * fullcap_nom_val.intval / 100) / 1000) * 3600 /
				(cur_time.tv_sec - pcisd->prev_time);
			pcisd->data[CISD_DATA_CAP_PER_TIME] = (pcisd->data[CISD_DATA_CAP_PER_TIME] > value) ?
				pcisd->data[CISD_DATA_CAP_PER_TIME] : value;

			pr_info("%s: Full To Full QH check Prev QH : %d, Current QH : %d, Delta : %d, Prev time : %ld, Cur time : %ld, Fullcap_Nom : %d, Leakage : %d\n",
					__func__, pcisd->prev_qh_value, qh_val.intval, delta, pcisd->prev_time, cur_time.tv_sec, fullcap_nom_val.intval, value);

			pcisd->prev_qh_value = qh_val.intval;
			pcisd->prev_qh_vfsoc = vfsoc_val.intval;
			pcisd->prev_time = cur_time.tv_sec;
			battery->qh_start = true;

			time_to_tm(cur_time.tv_sec, 0, &cur_date);
			pr_info("%s: Ref time = %ldy %dm %dd %dh %dm %ds, QH value : %d, VFSOC : %d\n",
					__func__,
					cur_date.tm_year + 1900, cur_date.tm_mon + 1,
					cur_date.tm_mday, cur_date.tm_hour,
					cur_date.tm_min, cur_date.tm_sec, pcisd->prev_qh_value, pcisd->prev_qh_vfsoc);
		}
		pr_info("%s: [DEBUG] Prev QH : %d, Current QH : %d, Prev time : %ld, Cur time : %ld\n",
				__func__, pcisd->prev_qh_value, qh_val.intval, pcisd->prev_time, cur_time.tv_sec);

#if 0
		/* Long Full QH check */
		} else if (battery->qh_start && (cur_time.tv_sec - pcisd->prev_time >= 10800) &&
				((battery->cable_type == POWER_SUPPLY_TYPE_BATTERY && battery->current_now < 0) ||
				 vfsoc_val.intval < 100000)) {
			int value, delta;
			union power_supply_propval fullcap_nom_val, qh_val;

			battery->qh_start = false;

			qh_val.intval = SEC_BATTERY_CAPACITY_QH;
			psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_ENERGY_NOW, qh_val);

			fullcap_nom_val.intval = SEC_BATTERY_CAPACITY_AGEDCELL;
			psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_ENERGY_NOW, fullcap_nom_val);

			time_to_tm(cur_time.tv_sec, 0, &cur_date);
			pr_info("%s: End time = %ldY %dM %dD %dH %dM %dS, QH value : %d, VFSOC : %d\n",
					__func__,
					cur_date.tm_year + 1900, cur_date.tm_mon + 1,
					cur_date.tm_mday, cur_date.tm_hour,
					cur_date.tm_min, cur_date.tm_sec, pcisd->prev_qh_value, pcisd->prev_qh_vfsoc);

			if (qh_val.intval & 0x8000) {
				pcisd->data[CISD_DATA_LEAKAGE_F] = qh_val.intval;
				qh_val.intval = qh_val.intval - 0xFFFF;
			}

			if (pcisd->prev_qh_value & 0x8000) {
				pcisd->data[CISD_DATA_LEAKAGE_G] = pcisd->prev_qh_value;
				pcisd->prev_qh_value = pcisd->prev_qh_value - 0xFFFF;
			}

			delta = qh_val.intval - pcisd->prev_qh_value;

			value = (((delta * 1000) -
						(vfsoc_val.intval - pcisd->prev_qh_vfsoc) * fullcap_nom_val.intval / 100) / 1000) * 3600 /
				(cur_time.tv_sec - pcisd->prev_time);

			pr_info("%s: Long Full QH check Prev QH : %d, Current QH : %d, Delta : %d, Prev VFSOC : %d, Cur VFSOC : %d"
					" Prev time : %ld, Cur time : %ld, Fullcap_Nom : %d, Leakage : %d\n",
					__func__, pcisd->prev_qh_value, qh_val.intval, delta, pcisd->prev_qh_vfsoc, vfsoc_val.intval,
					pcisd->prev_time, cur_time.tv_sec, fullcap_nom_val.intval, value);
		}
#endif

#endif /* CONFIG_QH_ALGORITHM */

		/* check abnormal vbat */
		pcisd->ab_vbat_check_count = battery->voltage_now > pcisd->max_voltage_thr ?
				pcisd->ab_vbat_check_count + 1 : 0;
		
		if ((pcisd->ab_vbat_check_count >= pcisd->ab_vbat_max_count) &&
			!(pcisd->state & CISD_STATE_OVER_VOLTAGE)) {
			dev_info(battery->dev, "%s : [CISD] Battery Over Voltage Protction !! vbat(%d)mV\n",
				 __func__, battery->voltage_now);
			vbat_val.intval = true;
			psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_VBAT_OVP,
					vbat_val);
			pcisd->data[CISD_DATA_OVER_VOLTAGE]++;
			pcisd->state |= CISD_STATE_OVER_VOLTAGE;
		}

		/* get actual input current */
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_CURRENT_AVG, incur_val);

		/* get actual charging current */
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, chgcur_val);

		dev_info(battery->dev, "%s: [CISD] iavg: %d, incur: %d, chgcur: %d,\n"
			"cc_T: %ld, lcd_off_T: %ld, passed_T: %ld, full_T: %ld, chg_end_T: %ld, cisd: 0x%x\n",__func__,
			battery->current_avg, incur_val.intval, chgcur_val.intval,
			pcisd->cc_start_time, pcisd->lcd_off_start_time, battery->charging_passed_time,
			battery->charging_fullcharged_time, pcisd->charging_end_time, pcisd->state);

#if 0
		if (is_cisd_check_type(battery->cable_type) && incur_val.intval > pcisd->current_max_thres &&
			chgcur_val.intval > pcisd->charging_current_thres && battery->current_now > 0 &&
			battery->siop_level == 100 && battery->charging_mode == SEC_BATTERY_CHARGING_1ST) {
			/* Not entered to Full after latest lcd off charging even though 270 min passed */
			if (pcisd->lcd_off_start_time <= 0) {
				pcisd->lcd_off_start_time = battery->charging_passed_time;
			}

			/* Not entered to Full with charging current greater than latest 1000mA even though 60 min passed */
			if (battery->current_avg < pcisd->current_avg_thres && pcisd->cc_start_time <= 0) {
				pcisd->cc_start_time = battery->charging_passed_time;
			} else if (battery->current_avg >= 1000 && pcisd->cc_start_time) {
				pcisd->state &= ~CISD_STATE_LEAK_B;
				pcisd->cc_start_time = 0;
			}
		} else {
			pcisd->state &= ~CISD_STATE_LEAK_A;
			pcisd->state &= ~CISD_STATE_LEAK_B;
			pcisd->cc_start_time = 0;
			pcisd->lcd_off_start_time = 0;
		}

		if (is_cisd_check_type(battery->cable_type) && battery->siop_level == 100 &&
			battery->current_now > 0 && battery->charging_mode == SEC_BATTERY_CHARGING_2ND) {
			if (pcisd->full_start_time <= 0) {
				pcisd->full_start_time = battery->charging_passed_time;
			}
		} else {
			pcisd->state &= ~CISD_STATE_LEAK_C;
			pcisd->full_start_time = 0;
		}

		if (is_cisd_check_type(battery->cable_type) && battery->siop_level == 100 &&
			battery->current_now >= 0 && ((battery->status == POWER_SUPPLY_STATUS_FULL &&
		    battery->charging_mode == SEC_BATTERY_CHARGING_NONE) || battery->is_recharging)) {
			now_ts = ktime_to_timespec(ktime_get_boottime());
			dev_info(battery->dev, "%s: cisd - leakage D Test(now time = %ld)\n", __func__, ((unsigned long)now_ts.tv_sec));
			if (pcisd->charging_end_time <= 0)
				pcisd->charging_end_time = now_ts.tv_sec;
		} else {
			pcisd->state &= ~CISD_STATE_LEAK_D;
			pcisd->recharge_count = 0;
			pcisd->charging_end_time = 0;
		}
#endif

		if (battery->siop_level != 100 ||
			battery->current_now < 0 || (battery->status != POWER_SUPPLY_STATUS_FULL)) {
			pcisd->state &= ~(CISD_STATE_LEAK_E|CISD_STATE_LEAK_F|CISD_STATE_LEAK_G);
			dev_info(battery->dev, "%s: cisd - clear EFG\n", __func__);
			pcisd->recharge_count_2 = 0;
			pcisd->charging_end_time_2 = 0;
		}

		now_ts = ktime_to_timespec(ktime_get_boottime());
#if 0
		/* check cisd leak case */
		if ((!(pcisd->state & CISD_STATE_LEAK_A) && !(pcisd->state & CISD_STATE_LEAK_B)) &&
			pcisd->lcd_off_start_time &&
			battery->charging_passed_time - pcisd->lcd_off_start_time > pcisd->lcd_off_delay_time) {
			pcisd->state |= CISD_STATE_LEAK_A;
			pcisd->data[CISD_DATA_LEAKAGE_A]++;
		} else if (!(pcisd->state & CISD_STATE_LEAK_B) && pcisd->cc_start_time &&
			battery->charging_passed_time - pcisd->cc_start_time > pcisd->cc_delay_time) {
			pcisd->state |= CISD_STATE_LEAK_B;
			pcisd->data[CISD_DATA_LEAKAGE_B]++;
		} else if (!(pcisd->state & CISD_STATE_LEAK_C) && pcisd->full_start_time &&
			battery->charging_passed_time - pcisd->full_start_time > pcisd->full_delay_time) {
			pcisd->state |= CISD_STATE_LEAK_C;
			pcisd->data[CISD_DATA_LEAKAGE_C]++;
		} else if (!(pcisd->state & CISD_STATE_LEAK_D) && pcisd->recharge_count >= pcisd->recharge_count_thres) {
			dev_info(battery->dev, "%s: cisd - leakage D Test(now time = %ld)\n", __func__, ((unsigned long)now_ts.tv_sec));
			if ((unsigned long)now_ts.tv_sec - pcisd->charging_end_time <= pcisd->recharge_delay_time) {
				pcisd->state |= CISD_STATE_LEAK_D;
				pcisd->data[CISD_DATA_LEAKAGE_D]++;
			}
		}
#endif
		
#if !defined(CONFIG_QH_ALGORITHM)
		if (!(pcisd->state & (CISD_STATE_LEAK_E|CISD_STATE_LEAK_F|CISD_STATE_LEAK_G))
			&& pcisd->charging_end_time_2 > 0 && pcisd->recharge_count_2 > 0) {
			if ((unsigned long)now_ts.tv_sec - pcisd->charging_end_time_2 <= pcisd->leakage_e_time) {
				dev_info(battery->dev, "%s: cisd - leakage E Test(now time = %ld)\n", __func__, ((unsigned long)now_ts.tv_sec));
				pcisd->state |= CISD_STATE_LEAK_E;
				pcisd->data[CISD_DATA_LEAKAGE_E]++;
				pcisd->recharge_count_2 = 0;
				pcisd->charging_end_time_2 = 0;
			} else if ((unsigned long)now_ts.tv_sec - pcisd->charging_end_time_2 <= pcisd->leakage_f_time) {
				dev_info(battery->dev, "%s: cisd - leakage F Test(now time = %ld)\n", __func__, ((unsigned long)now_ts.tv_sec));
				pcisd->state |= CISD_STATE_LEAK_F;
				pcisd->data[CISD_DATA_LEAKAGE_F]++;
				pcisd->recharge_count_2 = 0;
				pcisd->charging_end_time_2 = 0;
			} else if ((unsigned long)now_ts.tv_sec - pcisd->charging_end_time_2 <= pcisd->leakage_g_time) {
				dev_info(battery->dev, "%s: cisd - leakage G Test(now time = %ld)\n", __func__, ((unsigned long)now_ts.tv_sec));
				pcisd->state |= CISD_STATE_LEAK_G;
				pcisd->data[CISD_DATA_LEAKAGE_G]++;
				pcisd->recharge_count_2 = 0;
				pcisd->charging_end_time_2 = 0;
			}
		}
#endif

		dev_info(battery->dev, "%s: [CISD] iavg: %d, incur: %d, chgcur: %d,\n"
			"cc_T: %ld, lcd_off_T: %ld, passed_T: %ld, full_T: %ld, chg_end_T: %ld, recnt: %d, cisd: 0x%x\n",__func__,
			battery->current_avg, incur_val.intval, chgcur_val.intval,
			pcisd->cc_start_time, pcisd->lcd_off_start_time, battery->charging_passed_time,
			battery->charging_fullcharged_time, pcisd->charging_end_time, pcisd->recharge_count, pcisd->state);

		pcisd->state &= ~CISD_STATE_CAP_OVERFLOW;

		capcurr_val.intval = SEC_BATTERY_CAPACITY_CURRENT;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, capcurr_val);

		if (capcurr_val.intval >= pcisd->capacity_now && capcurr_val.intval > pcisd->overflow_cap_thr &&
			pcisd->overflow_start_time <= 0) {
			now_ts = ktime_to_timespec(ktime_get_boottime());
			pcisd->overflow_start_time = now_ts.tv_sec;
		}
	} else  {
		/* discharging */
		if (battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			/* check abnormal vbat */
			pcisd->ab_vbat_check_count = battery->voltage_now > pcisd->max_voltage_thr ?
				pcisd->ab_vbat_check_count + 1 : 0;

			if ((pcisd->ab_vbat_check_count >= pcisd->ab_vbat_max_count) &&
					!(pcisd->state & CISD_STATE_OVER_VOLTAGE)) {
				pcisd->data[CISD_DATA_OVER_VOLTAGE]++;
				pcisd->state |= CISD_STATE_OVER_VOLTAGE;
			}
		}

		pcisd->state &= ~(CISD_STATE_LEAK_A|CISD_STATE_LEAK_B|CISD_STATE_LEAK_C|CISD_STATE_LEAK_D|CISD_STATE_LEAK_E|CISD_STATE_LEAK_F|CISD_STATE_LEAK_G);

		now_ts = ktime_to_timespec(ktime_get_boottime());
		capcurr_val.intval = SEC_BATTERY_CAPACITY_FULL;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, capcurr_val);
		if (capcurr_val.intval == -1) {
			dev_info(battery->dev, "%s: [CISD] FG I2C fail. skip cisd check \n", __func__);
			return ret;
		}

		if (!(pcisd->state & CISD_STATE_CAP_OVERFLOW) &&
			(capcurr_val.intval != prev_fullcap_rep)) {
			if (capcurr_val.intval > pcisd->err_cap_high_thr) {
				pcisd->state |= CISD_STATE_CAP_OVERFLOW;
				pcisd->data[CISD_DATA_ERRCAP_HIGH] += 1;
				prev_fullcap_rep = capcurr_val.intval;
			} else if (capcurr_val.intval > pcisd->err_cap_low_thr) {
				pcisd->state |= CISD_STATE_CAP_OVERFLOW;
				pcisd->data[CISD_DATA_ERRCAP_LOW] += 1;
				prev_fullcap_rep = capcurr_val.intval;
			}
		}
#if defined(CONFIG_QH_ALGORITHM)
		vfsoc_val.intval = SEC_BATTERY_CAPACITY_VFSOC;
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_ENERGY_NOW, vfsoc_val);
		if (vfsoc_val.intval < 101000)
			battery->qh_start = false;
#else
		if (pcisd->overflow_start_time > 0 && capcurr_val.intval > pcisd->overflow_cap_thr) {
			if ((capcurr_val.intval - pcisd->overflow_cap_thr) * 3600 /
				(now_ts.tv_sec - pcisd->overflow_start_time) > pcisd->data[CISD_DATA_CAP_PER_TIME])
				pcisd->data[CISD_DATA_CAP_PER_TIME] = (capcurr_val.intval - pcisd->overflow_cap_thr) * 3600 /
					(now_ts.tv_sec - pcisd->overflow_start_time);
		}
#endif
		pcisd->data[CISD_DATA_CAP_ONCE] = capcurr_val.intval > pcisd->data[CISD_DATA_CAP_ONCE] ?
			capcurr_val.intval : pcisd->data[CISD_DATA_CAP_ONCE];
		pcisd->capacity_now = capcurr_val.intval;
		pcisd->overflow_cap_thr = capcurr_val.intval > 3850 ? capcurr_val.intval : 3850;
		pcisd->overflow_start_time = 0;

		if (capcurr_val.intval > pcisd->data[CISD_DATA_CAP_MAX])
			pcisd->data[CISD_DATA_CAP_MAX] = capcurr_val.intval;
		if (capcurr_val.intval < pcisd->data[CISD_DATA_CAP_MIN])
			pcisd->data[CISD_DATA_CAP_MIN] = capcurr_val.intval;
	}

	pr_info("cisd - stt:%d, cp:%d/%d, cpmm:%d/%d/%d, dcpt:%d, ovc:%d, rct:%d\n",
		pcisd->state, pcisd->curr_cap_max, pcisd->err_cap_max_thrs, pcisd->data[CISD_DATA_CAP_MIN],
		pcisd->capacity_now, pcisd->data[CISD_DATA_CAP_MAX], pcisd->data[CISD_DATA_CAP_PER_TIME],
		pcisd->data[CISD_DATA_OVER_VOLTAGE], pcisd->data[CISD_DATA_RECHARGING_TIME]);


	pr_info("cisd_debug: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
		pcisd->data[CISD_DATA_FULL_COUNT], pcisd->data[CISD_DATA_CAP_MAX],
		pcisd->data[CISD_DATA_CAP_MIN], pcisd->data[CISD_DATA_CAP_ONCE],
		pcisd->data[CISD_DATA_LEAKAGE_A], pcisd->data[CISD_DATA_LEAKAGE_B],
		pcisd->data[CISD_DATA_LEAKAGE_C], pcisd->data[CISD_DATA_LEAKAGE_D],
		pcisd->data[CISD_DATA_CAP_PER_TIME], pcisd->data[CISD_DATA_ERRCAP_LOW],
		pcisd->data[CISD_DATA_ERRCAP_HIGH], pcisd->data[CISD_DATA_OVER_VOLTAGE],
		pcisd->data[CISD_DATA_LEAKAGE_E], pcisd->data[CISD_DATA_LEAKAGE_F],
		pcisd->data[CISD_DATA_LEAKAGE_G], pcisd->data[CISD_DATA_RECHARGING_TIME],
		pcisd->data[CISD_DATA_VALERT_COUNT], pcisd->cisd_alg_index);
	return ret;
}

void sec_battery_cisd_init(struct sec_battery_info *battery)
{
	union power_supply_propval capfull_val;

	battery->cisd.state = CISD_STATE_NONE;

	battery->cisd.delay_time = 600; /* 10 min */
	battery->cisd.diff_volt_now = 40;
	battery->cisd.diff_cap_now = 5;

	capfull_val.intval = SEC_BATTERY_CAPACITY_FULL;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_ENERGY_NOW, capfull_val);
	battery->cisd.curr_cap_max = capfull_val.intval;
	battery->cisd.err_cap_high_thr = battery->pdata->cisd_cap_high_thr;
	battery->cisd.err_cap_low_thr = battery->pdata->cisd_cap_low_thr;
	battery->cisd.cc_delay_time = 3600; /* 60 min */
	battery->cisd.lcd_off_delay_time = 10200; /* 230 min */
	battery->cisd.full_delay_time = 3600; /* 60 min */
	battery->cisd.recharge_delay_time = 9000; /* 150 min */
	battery->cisd.cc_start_time = 0;
	battery->cisd.full_start_time = 0;
	battery->cisd.lcd_off_start_time = 0;
	battery->cisd.overflow_start_time = 0;
	battery->cisd.charging_end_time = 0;
	battery->cisd.charging_end_time_2 = 0;
	battery->cisd.recharge_count = 0;
	battery->cisd.recharge_count_2 = 0;
	battery->cisd.recharge_count_thres = 2;
	battery->cisd.leakage_e_time = 3600; /* 60 min */
	battery->cisd.leakage_f_time = 7200; /* 120 min */
	battery->cisd.leakage_g_time = 14400; /* 240 min */
	battery->cisd.current_max_thres = 1600;
	battery->cisd.charging_current_thres = 1000;
	battery->cisd.current_avg_thres = 1000;
#if defined(CONFIG_QH_ALGORITHM)
	battery->cisd_qh_current_high_thr = 210;
	battery->cisd_qh_current_low_thr = 190;
	battery->cisd.qh_valid_time = 3600; /* 60 min */
	battery->cisd.prev_qh_value = 0;
	battery->cisd.prev_qh_vfsoc = 0;
	battery->cisd.prev_time = 0;
	battery->cisd.qh_value_now = 0;
	battery->cisd.qh_vfsoc_now = 0;
	battery->qh_start = false;
#endif

	battery->cisd.data[CISD_DATA_FULL_COUNT] = 1;
	battery->cisd.data[CISD_DATA_CAP_MIN] = 0xFFFF;
	battery->cisd.data[CISD_DATA_RECHARGING_TIME] = 0x7FFFFFFF;
	battery->cisd.data[CISD_DATA_CAP_ONCE] = 0;
	battery->cisd.data[CISD_DATA_CAP_PER_TIME] = 0;
	battery->cisd.data[CISD_DATA_BATT_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_CHG_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_WPC_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_BATT_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_CHG_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_WPC_TEMP_MIN] = 1000;

	battery->cisd.capacity_now = capfull_val.intval;
	battery->cisd.overflow_cap_thr = capfull_val.intval > battery->pdata->cisd_cap_limit ?
		capfull_val.intval : battery->pdata->cisd_cap_limit;

	battery->cisd.ab_vbat_max_count = 1; /* should be 1 */
	battery->cisd.ab_vbat_check_count = 0;
	battery->cisd.max_voltage_thr = battery->pdata->max_voltage_thr;
	battery->cisd.cisd_alg_index = 6;
	pr_info("%s: cisd.err_cap_high_thr:%d, cisd.err_cap_low_thr:%d, cisd.overflow_cap_thr:%d\n", __func__,
		battery->cisd.err_cap_high_thr, battery->cisd.err_cap_low_thr, battery->cisd.overflow_cap_thr);
}
