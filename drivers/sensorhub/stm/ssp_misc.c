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

#include "ssp_misc.h"

static struct remove_af_noise af_sensor;

int remove_af_noise_register(struct remove_af_noise *af_cam)
{
	if (af_cam->af_pdata)
		af_sensor.af_pdata = af_cam->af_pdata;

	if (af_cam->af_func)
		af_sensor.af_func = af_cam->af_func;

	if (!af_cam->af_pdata || !af_cam->af_func) {
		ssp_errf("no af struct");
		return ERROR;
	}

	return 0;
}

void remove_af_noise_unregister(struct remove_af_noise *af_cam)
{
	af_sensor.af_pdata = NULL;
	af_sensor.af_func = NULL;
}

void report_shake_cam_data(struct ssp_data *data,
	struct sensor_value *shake_cam_data)
{
	data->buf[SHAKE_CAM].x = shake_cam_data->x;

	ssp_infof("shake = %d", (char)data->buf[SHAKE_CAM].x);

	if (likely(af_sensor.af_pdata && af_sensor.af_func)) {
		if ((char)data->buf[SHAKE_CAM].x == SHAKE_ON)
			af_sensor.af_func(af_sensor.af_pdata, true);
		else if ((char)data->buf[SHAKE_CAM].x == SHAKE_OFF)
			af_sensor.af_func(af_sensor.af_pdata, false);
	} else {
		ssp_errf("no af_func");
	}
}
