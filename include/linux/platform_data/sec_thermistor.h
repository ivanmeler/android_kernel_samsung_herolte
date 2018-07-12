/*
 * sec_thermistor.h - SEC Thermistor
 *
 *  Copyright (C) 2013 Samsung Electronics
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __LINUX_SEC_THERMISTOR_H
#define __LINUX_SEC_THERMISTOR_H __FILE__

/**
 * struct sec_therm_adc_table - adc to temperature table for sec thermistor
 * driver
 * @adc: adc value
 * @temperature: temperature(C) * 10
 */
struct sec_therm_adc_table {
	int adc;
	int temperature;
};

/**
 * struct sec_bat_plaform_data - init data for sec batter driver
 * @adc_channel: adc channel that connected to thermistor
 * @adc_table: array of adc to temperature data
 * @adc_arr_size: size of adc_table
 */
struct sec_therm_platform_data {
	unsigned int adc_channel;
	unsigned int adc_arr_size;
	struct sec_therm_adc_table *adc_table;
};

#endif /* __LINUX_SEC_THERMISTOR_H */
