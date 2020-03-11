/*
 * sec_psb.h
 * Samsung Mobile 'Prevent Swelling Battery' Header
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
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
#ifndef __SEC_PSB_H
#define __SEC_PSB_H __FILE__

#include <linux/battery/sec_battery.h>

extern char *sec_bat_status_str[];

int sec_bat_set_charge(struct sec_battery_info *battery, int chg_mode);
void sec_bat_set_charging_status(struct sec_battery_info *battery, int status);

void sec_bat_update_psb_level(struct sec_battery_info *battery);
void sec_bat_start_psb(struct sec_battery_info *battery);
void sec_bat_check_psb(struct sec_battery_info *battery);
void sec_bat_check_full_state(struct sec_battery_info *battery);

void sec_bat_init_psb(struct sec_battery_info *battery);
void sec_bat_remove_psb(struct sec_battery_info *battery);

#endif /* __SEC_PSB_H */
