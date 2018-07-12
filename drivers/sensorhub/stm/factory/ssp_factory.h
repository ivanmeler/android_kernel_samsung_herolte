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

#ifndef __SSP_FACTORY_H__
#define __SSP_FACTORY_H__

#include "../ssp.h"

/* Gyroscope DPS */
#define GYROSCOPE_DPS250                250
#define GYROSCOPE_DPS500                500
#define GYROSCOPE_DPS2000               2000

/* Gesture Sensor Current */
#define DEFUALT_IR_CURRENT              100 //0xF0

/* Proxy threshold */
#define DEFUALT_HIGH_THRESHOLD		2000
#define DEFUALT_LOW_THRESHOLD		1400

/* Magnetic */
#define PDC_SIZE			27

/* Factory Test */
#define ACCELEROMETER_FACTORY		0x80
#define GYROSCOPE_FACTORY		0x81
#define GEOMAGNETIC_FACTORY		0x82
#define PRESSURE_FACTORY		0x85
#define GESTURE_FACTORY			0x86
#define TEMPHUMIDITY_CRC_FACTORY	0x88
#define GYROSCOPE_TEMP_FACTORY		0x8A
#define GYROSCOPE_DPS_FACTORY		0x8B


struct ssp_data;

void initialize_accel_factorytest(struct ssp_data *);
void initialize_prox_factorytest(struct ssp_data *);
void initialize_light_factorytest(struct ssp_data *);
void initialize_gyro_factorytest(struct ssp_data *);
void initialize_pressure_factorytest(struct ssp_data *);
void initialize_magnetic_factorytest(struct ssp_data *);
void initialize_gesture_factorytest(struct ssp_data *data);
void initialize_irled_factorytest(struct ssp_data *data);
void remove_accel_factorytest(struct ssp_data *);
void remove_gyro_factorytest(struct ssp_data *);
void remove_prox_factorytest(struct ssp_data *);
void remove_light_factorytest(struct ssp_data *);
void remove_pressure_factorytest(struct ssp_data *);
void remove_magnetic_factorytest(struct ssp_data *);
void remove_gesture_factorytest(struct ssp_data *data);
void remove_irled_factorytest(struct ssp_data *data);
#ifdef CONFIG_SENSORS_SSP_MOBEAM
void initialize_mobeam(struct ssp_data *data);
void remove_mobeam(struct ssp_data *data);
#endif

int accel_open_calibration(struct ssp_data *);
int gyro_open_calibration(struct ssp_data *);
int pressure_open_calibration(struct ssp_data *);
int proximity_open_calibration(struct ssp_data *);
int set_gyro_cal(struct ssp_data *);
int save_gyro_caldata(struct ssp_data *, s16 *);
int set_accel_cal(struct ssp_data *);
int initialize_magnetic_sensor(struct ssp_data *);
void get_proximity_threshold(struct ssp_data *);

#endif
