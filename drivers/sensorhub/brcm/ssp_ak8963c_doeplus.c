/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include "ssp.h"

void report_mag_data(struct ssp_data *data, struct sensor_value *magdata)
{
	data->buf[GEOMAGNETIC_SENSOR].x = magdata->x;
	data->buf[GEOMAGNETIC_SENSOR].y = magdata->y;
	data->buf[GEOMAGNETIC_SENSOR].z = magdata->z;

	input_report_abs(data->mag_input_dev, ABS_RX, magdata->x);
	input_report_abs(data->mag_input_dev, ABS_RY, magdata->y);
	input_report_abs(data->mag_input_dev, ABS_RZ, magdata->z);
	input_sync(data->mag_input_dev);
}

void initialize_magnetic(struct ssp_data *data)
{
	int iRet = 0;
	struct input_dev *mag_input_dev;

	mag_input_dev = input_allocate_device();
	if (mag_input_dev == NULL)
		goto exit;

	input_set_drvdata(mag_input_dev, data);
	mag_input_dev->name = "magnetic_sensor";

	input_set_capability(mag_input_dev, EV_ABS, ABS_RX);
	input_set_capability(mag_input_dev, EV_ABS, ABS_RY);
	input_set_capability(mag_input_dev, EV_ABS, ABS_RZ);
	input_set_abs_params(mag_input_dev, ABS_RX, -32767, 32768, 0, 0);
	input_set_abs_params(mag_input_dev, ABS_RY, -32767, 32768, 0, 0);
	input_set_abs_params(mag_input_dev, ABS_RZ, -32767, 32768, 0, 0);

	iRet = input_register_device(mag_input_dev);
	if (iRet < 0)
		goto exit;

	data->mag_input_dev = mag_input_dev;

	sensors_create_symlink(data->mag_input_dev);

exit:
	return;
}

void remove_magnetic(struct ssp_data *data)
{
	sensors_remove_symlink(data->mag_input_dev);

	input_unregister_device(data->mag_input_dev);
}
