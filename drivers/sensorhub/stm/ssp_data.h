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

#ifndef __SSP_DATA_H__
#define __SSP_DATA_H__

#include "ssp.h"

/* SSP -> AP Instruction */
#define MSG2AP_INST_BYPASS_DATA		0x37
#define MSG2AP_INST_LIBRARY_DATA	0x01
#define MSG2AP_INST_DEBUG_DATA		0x03
#define MSG2AP_INST_BIG_DATA		0x04
#define MSG2AP_INST_META_DATA		0x05
#define MSG2AP_INST_TIME_SYNC		0x06
#define MSG2AP_INST_RESET		0x07
#define MSG2AP_INST_GYRO_CAL		0x08
#define MSG2AP_INST_DUMP_DATA		0xDD


struct ssp_data;
struct sensor_value;

int handle_big_data(struct ssp_data *, char *, int *);
void refresh_task(struct work_struct *);
int queue_refresh_task(struct ssp_data *, int);
int parse_dataframe(struct ssp_data *, char *, int);
void initialize_function_pointer(struct ssp_data *);
void get_sensordata(struct ssp_data *, char *, int *, int,
			struct sensor_value *);

#endif
