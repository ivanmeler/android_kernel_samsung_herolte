/*
 *  Copyright (C) 2014, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef __SSP_MISC_H__
#define __SSP_MISC_H__

#include "ssp.h"

#define	SHAKE_ON	1
#define	SHAKE_OFF	2

struct remove_af_noise {
	void *af_pdata;
	int16_t (*af_func)(void *, bool);
};

int remove_af_noise_register(struct remove_af_noise *af_cam);
EXPORT_SYMBOL(remove_af_noise_register);

void remove_af_noise_unregister(struct remove_af_noise *af_cam);
EXPORT_SYMBOL(remove_af_noise_unregister);

void report_shake_cam_data(struct ssp_data *data,
	struct sensor_value *shake_cam_data);

#endif
