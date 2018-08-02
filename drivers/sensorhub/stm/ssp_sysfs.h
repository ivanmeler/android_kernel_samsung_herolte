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

#ifndef __SSP_SYSFS_H__
#define __SSP_SYSFS_H__

#include <linux/math64.h>
#include "ssp.h"

#define BATCH_IOCTL_MAGIC	0xFC

enum {
	SENSORS_BATCH_DRY_RUN = 0x00000001,
	SENSORS_BATCH_WAKE_UPON_FIFO_FULL = 0x00000002
};


int get_msdelay(int64_t dDelayRate);
int initialize_sysfs(struct ssp_data *data);
void remove_sysfs(struct ssp_data *data);

#endif
