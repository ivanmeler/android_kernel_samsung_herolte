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

#ifndef __SSP_DEBUG_H__
#define __SSP_DEBUG_H__

#include "ssp.h"
#if CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif
#include <linux/fs.h>


#define SSP_DEBUG_TIMER_SEC	(10 * HZ)
#define LIMIT_RESET_CNT		20
#define LIMIT_TIMEOUT_CNT	3
#define DUMP_FILE_PATH		"/data/log/MCU_DUMP"
#define DEBUG_DUMP_FILE_PATH	"/data/log/SensorHubDump"
#define DEBUG_DUMP_DATA_COMPLETE 0xDD


struct ssp_data;

void ssp_dump_task(struct work_struct *work);
int print_mcu_debug(char *pchRcvDataFrame, int *pDataIdx,
		int iRcvDataFrameLength);
void reset_mcu(struct ssp_data *data);
void sync_sensor_state(struct ssp_data *data);
void enable_debug_timer(struct ssp_data *data);
void disable_debug_timer(struct ssp_data *data);
int initialize_debug_timer(struct ssp_data *data);
int debug_crash_dump(struct ssp_data *data, char *pchRcvDataFrame, int iLength);

#endif
