/*
 * max17058_fuelgauge.h
 * Samsung MAX17058 Fuel Gauge Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
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

#ifndef __MAX17058_FUELGAUGE_H
#define __MAX17058_FUELGAUGE_H __FILE__

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#define SEC_FUELGAUGE_I2C_SLAVEADDR 0x36

#define MAX17058_REG_VCELL		0x02
#define MAX17058_REG_SOC		0x04
#define MAX17058_REG_MODE		0x06
#define MAX17058_REG_VERSION		0x08
#define MAX17058_REG_CONFIG		0x0C
#define MAX17058_REG_VRESET		0x18
#define MAX17058_REG_STATUS		0x1A
#define MAX17058_REG_CMD		0xFE

struct battery_data_t {
	u8 *type_str;
};

struct sec_fg_info {
	bool dummy;
};

#endif /* __MAX17058_FUELGAUGE_H */
