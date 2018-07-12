/* abov_touchkey.h -- Linux driver for abov chip as touchkey
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Junkyeong Kim <jk0430.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#ifndef __LINUX_ABOV_TOUCHKEY_H
#define __LINUX_ABOV_TOUCHKEY_H

#define ABOV_TK_NAME "abov-touchkey-ft1804"
#define ABOV_ID 0x40

extern int get_lcd_attached(char *);

#define LIGHT_VERSION_PATH		"/efs/FactoryApp/tkey_light_version"
#define LIGHT_TABLE_PATH		"/efs/FactoryApp/tkey_light"
#define LIGHT_CRC_PATH			"/efs/FactoryApp/tkey_light_crc"
#define LIGHT_TABLE_PATH_LEN		50
#define LIGHT_VERSION_LEN		25
#define LIGHT_CRC_SIZE			10
#define LIGHT_DATA_SIZE			5
#define LIGHT_REG_MIN_VAL		0x01
#define LIGHT_REG_MAX_VAL		0x1f

struct light_info {
	int octa_id;
	int led_reg;
};

enum WINDOW_COLOR {
	WINDOW_COLOR_BLACK_UTYPE = 0,
	WINDOW_COLOR_BLACK,
	WINDOW_COLOR_WHITE,
	WINDOW_COLOR_GOLD,
	WINDOW_COLOR_SILVER,
	WINDOW_COLOR_GREEN,
	WINDOW_COLOR_BLUE,
	WINDOW_COLOR_PINKGOLD,
};
#define WINDOW_COLOR_DEFAULT		WINDOW_COLOR_BLACK

#define LIGHT_TABLE_MAX			10
struct light_info tkey_light_reg_table[LIGHT_TABLE_MAX];

struct abov_touchkey_platform_data {
	unsigned long irq_flag;
	int gpio_en;
	int gpio_int;
	int gpio_sda;
	int gpio_scl;
	int gpio_rst;
	int gpio_tkey_led_en;
	int gpio_seperated;
	int sub_det;
	struct regulator *vdd_io_vreg;
	struct regulator *avdd_vreg;
	struct regulator *dvdd_vreg;
	void (*input_event) (void *data);
	int (*power) (void *, bool on);
	int (*keyled) (bool on);
	char *fw_path;
	bool boot_on_ldo;
	bool bringup;
	bool ta_notifier;
	int dt_light_version;
	int dt_light_table;
};

#endif /* LINUX_ABOV_TOUCHKEY_H */
