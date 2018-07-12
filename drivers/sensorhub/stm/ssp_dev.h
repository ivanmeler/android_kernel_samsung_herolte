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

#ifndef __SSP_DEV_H__
#define __SSP_DEV_H__

#include "ssp.h"
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif

/* ssp mcu device ID */
#define DEVICE_ID               0x55

#define NORMAL_SENSOR_STATE_K	0x3FEFF
#define SSP_SW_RESET_TIME       3000

void ssp_enable(struct ssp_data *data, bool enable);
int initialize_mcu(struct ssp_data *data);

#endif
