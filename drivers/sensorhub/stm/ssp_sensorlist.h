/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SSP_SENSORS_H__
#define __SSP_SENSORS_H__

#define SENSOR_NAME_MAX_LEN		30
#define SENSOR_NAME {			\
	"accelerometer_sensor",		\
	"gyro_sensor",			\
	"uncal_geomagnetic_sensor",	\
	"",				\
	"geomagnetic_sensor",		\
	"pressure_sensor",		\
	"gesture_sensor",		\
	"proximity_sensor",		\
	"temp_humidity_sensor",		\
	"light_sensor",			\
	"",				\
	"",				\
	"step_det_sensor",		\
	"sig_motion_sensor",		\
	"uncal_gyro_sensor",		\
	"game_rotation_vector_sensor",	\
	"rotation_vector_sensor",	\
	"step_cnt_sensor",		\
	"",				\
	"",				\
	"",				\
	"",				\
	"shake_cam",			\
	"",				\
	"light_ir_sensor",		\
	"interrupt_gyro_sensor",	\
	"meta_event", }

/* 0: disable sensor, 1: enable as iio device */
#define SENSOR_ENABLE {		\
	1, /* acc */		\
	1, /* gyro */		\
	1, /* uncal_mag*/	\
	0, /* raw_mag */	\
	1, /* mag */		\
	1, /* pressure */	\
	0, /* gesture */	\
	1, /* proximity */	\
	0, /* temp_humi */	\
	1, /* light */		\
	0, /* proximity raw */	\
	0, /* orientation */	\
	1, /* step detector */	\
	1, /* sig motion */	\
	1, /* uncal_gyro */	\
	1, /* game rotation */	\
	1, /* rotation */	\
	1, /* step counter */	\
	0, /* bio hrm raw */	\
	0, /* bio hrm rawfac */	\
	0, /* bio hrm lib */	\
	0, /* touch? */		\
	0, /* shake cam */	\
	0, /* ? */		\
	1, /* light ir */	\
	1, /* interrupt gyro */	\
	1, /* meta sensor */ }

#if defined(CONFIG_SENSORS_SSP_TMG399X)
/* byte unit - not including timestamp */
#define SENSOR_DATA_LEN	{ \
	6, 6, 12, 6, 7, 6, 20, 2, 5, 10, \
	1, 0, 1, 1, 12, 17, 17, 4, 0, 0, \
	0, 0, 1, 0, 12, 6, 8, }

/* byte unit - not including timestamp */
#define SENSOR_REPORT_LEN { \
	6, 10, 12, 6, 7, 14, 20, 2, 5, 10, \
	0, 0, 1, 1, 12, 17, 17, 12, 0, 0, \
	0, 0, 1, 0, 12, 10, 8, }
#else
/* byte unit - not including timestamp */
#define SENSOR_DATA_LEN	{ \
	6, 6, 12, 6, 7, 6, 20, 3, 5, 10, \
	2, 0, 1, 1, 12, 17, 17, 4, 0, 0, \
	0, 0, 1, 0, 12, 6, 8, }

/* byte unit - not including timestamp */
#define SENSOR_REPORT_LEN { \
	6, 10, 12, 6, 7, 14, 20, 3, 5, 10, \
	0, 0, 1, 1, 12, 17, 17, 12, 0, 0, \
	0, 0, 1, 0, 12, 10, 8, }
#endif

#endif
