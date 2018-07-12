/*
 * sec_cisd.h
 * Samsung Mobile Charger Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SEC_CISD_H
#define __SEC_CISD_H __FILE__

#define CISD_STATE_NONE			0x00
#define CISD_STATE_CAP_OVERFLOW	0x01
#define CISD_STATE_VOLT_DROP	0x02
#define CISD_STATE_SOC_DROP		0x04
#define CISD_STATE_RESET		0x08
#define CISD_STATE_LEAK_A		0x10
#define CISD_STATE_LEAK_B		0x20
#define CISD_STATE_LEAK_C		0x40
#define CISD_STATE_LEAK_D		0x80
#define CISD_STATE_OVER_VOLTAGE		0x100
#define CISD_STATE_LEAK_E		0x200
#define CISD_STATE_LEAK_F		0x400
#define CISD_STATE_LEAK_G		0x800

#define is_cisd_check_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_TA || \
	cable_type == SEC_BATTERY_CABLE_9V_TA || \
	cable_type == SEC_BATTERY_CABLE_9V_UNKNOWN || \
	cable_type == SEC_BATTERY_CABLE_9V_ERR || \
	cable_type == SEC_BATTERY_CABLE_PDIC)

enum cisd_data {
	CISD_DATA_FULL_COUNT = 0,
	CISD_DATA_CAP_MAX,
	CISD_DATA_CAP_MIN,
	CISD_DATA_CAP_ONCE,
	CISD_DATA_LEAKAGE_A,
	CISD_DATA_LEAKAGE_B,
	CISD_DATA_LEAKAGE_C,
	CISD_DATA_LEAKAGE_D,
	CISD_DATA_CAP_PER_TIME,
	CISD_DATA_ERRCAP_LOW,
	CISD_DATA_ERRCAP_HIGH,

	CISD_DATA_OVER_VOLTAGE,
	CISD_DATA_LEAKAGE_E,
	CISD_DATA_LEAKAGE_F,
	CISD_DATA_LEAKAGE_G,
	CISD_DATA_RECHARGING_TIME,
	CISD_DATA_VALERT_COUNT,
	CISD_DATA_CYCLE,
	CISD_DATA_WIRE_COUNT,
	CISD_DATA_WIRELESS_COUNT,
	CISD_DATA_HIGH_TEMP_SWELLING,

	CISD_DATA_LOW_TEMP_SWELLING,
	CISD_DATA_SWELLING_CHARGING_COUNT,
	CISD_DATA_SAFETY_TIMER_3,
	CISD_DATA_SAFETY_TIMER_5,
	CISD_DATA_SAFETY_TIMER_10,
	CISD_DATA_AICL_COUNT,
        CISD_DATA_BATT_TEMP_MAX,
        CISD_DATA_BATT_TEMP_MIN,
        CISD_DATA_CHG_TEMP_MAX,
        CISD_DATA_CHG_TEMP_MIN,

	CISD_DATA_WPC_TEMP_MAX,
	CISD_DATA_WPC_TEMP_MIN,
	CISD_UNSAFE_VOLTAGE,
	CISD_UNSAFE_TEMPERATURE,
	CISD_SAFETY_TIMER,
	CISD_VSYS_OVP,
	CISD_VBAT_OVP,
	CISD_WATER_DETECT,
	CISD_AFC_FAIL,

	CISD_DATA_MAX,
};

struct cisd {
	unsigned int cisd_alg_index;
	unsigned int state;

	unsigned int delay_time;
	int diff_volt_now;
	int diff_cap_now;
	int curr_cap_max;
	int err_cap_max_thrs;
	int err_cap_high_thr;
	int err_cap_low_thr;
	int overflow_cap_thr;
	unsigned int cc_delay_time;
	unsigned int full_delay_time;
	unsigned int lcd_off_delay_time;
	unsigned int recharge_delay_time;
	unsigned int diff_time;
	unsigned long cc_start_time;
	unsigned long full_start_time;
	unsigned long lcd_off_start_time;
	unsigned long overflow_start_time;
	unsigned long charging_end_time;
	unsigned long charging_end_time_2;
	unsigned int recharge_count;
	unsigned int recharge_count_2;
	unsigned int recharge_count_thres;
	unsigned long leakage_e_time;
	unsigned long leakage_f_time;
	unsigned long leakage_g_time;
	int current_max_thres;
	int charging_current_thres;
	int current_avg_thres;

	unsigned int ab_vbat_max_count;
	unsigned int ab_vbat_check_count;
	unsigned int max_voltage_thr;

	/* Big Data Field */
	int capacity_now;
	int data[CISD_DATA_MAX];

#if defined(CONFIG_QH_ALGORITHM)
	unsigned long prev_time;
	unsigned long qh_valid_time;
	int prev_qh_value;
	int prev_qh_vfsoc;
	int qh_value_now;
	int qh_vfsoc_now;
#endif
};

#endif /* __SEC_CISD_H */
