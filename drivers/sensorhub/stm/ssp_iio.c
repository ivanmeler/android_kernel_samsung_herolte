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

#include "ssp_iio.h"

static void init_sensorlist(struct ssp_data *data)
{
	char name[SENSOR_MAX][SENSOR_NAME_MAX_LEN] = SENSOR_NAME;
	bool enable[SENSOR_MAX] = SENSOR_ENABLE;
	int report_len[SENSOR_MAX] = SENSOR_REPORT_LEN;
	int data_len[SENSOR_MAX] = SENSOR_DATA_LEN;

	memcpy(&data->name, name, sizeof(data->name));
	memcpy(&data->enable, enable, sizeof(data->enable));
	memcpy(&data->report_len, report_len, sizeof(data->report_len));
	memcpy(&data->data_len, data_len, sizeof(data->data_len));
}

static int ssp_preenable(struct iio_dev *indio_dev)
{
	return iio_sw_buffer_preenable(indio_dev);
}

static int ssp_predisable(struct iio_dev *indio_dev)
{
	return 0;
}

static const struct iio_buffer_setup_ops ssp_iio_ring_setup_ops = {
	.preenable = &ssp_preenable,
	.predisable = &ssp_predisable,
};

static int ssp_iio_configure_ring(struct iio_dev *indio_dev)
{
	struct iio_buffer *ring;

	ring = iio_kfifo_allocate(indio_dev);
	if (!ring)
		return -ENOMEM;

	ring->scan_timestamp = true;
	ring->bytes_per_datum = 8;
	indio_dev->buffer = ring;
	indio_dev->setup_ops = &ssp_iio_ring_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_HARDWARE;

	return 0;
}

static void ssp_iio_push_buffers(struct iio_dev *indio_dev, u64 timestamp,
				char *data, int data_len)
{
	char buf[data_len + sizeof(timestamp)];

	if (!indio_dev || !data)
		return;

	memcpy(buf, data, data_len);
	memcpy(buf + data_len, &timestamp, sizeof(timestamp));
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);
}

static void report_prox_raw_data(struct ssp_data *data, int sensor,
	struct sensor_value *proxrawdata)
{
	if (data->uFactoryProxAvg[0]++ >= PROX_AVG_READ_NUM) {
		data->uFactoryProxAvg[2] /= PROX_AVG_READ_NUM;
		data->buf[sensor].prox_raw[1] = (u16)data->uFactoryProxAvg[1];
		data->buf[sensor].prox_raw[2] = (u16)data->uFactoryProxAvg[2];
		data->buf[sensor].prox_raw[3] = (u16)data->uFactoryProxAvg[3];

		data->uFactoryProxAvg[0] = 0;
		data->uFactoryProxAvg[1] = 0;
		data->uFactoryProxAvg[2] = 0;
		data->uFactoryProxAvg[3] = 0;
	} else {
		data->uFactoryProxAvg[2] += proxrawdata->prox_raw[0];

		if (data->uFactoryProxAvg[0] == 1)
			data->uFactoryProxAvg[1] = proxrawdata->prox_raw[0];
		else if (proxrawdata->prox_raw[0] < data->uFactoryProxAvg[1])
			data->uFactoryProxAvg[1] = proxrawdata->prox_raw[0];

		if (proxrawdata->prox_raw[0] > data->uFactoryProxAvg[3])
			data->uFactoryProxAvg[3] = proxrawdata->prox_raw[0];
	}

	data->buf[sensor].prox_raw[0] = proxrawdata->prox_raw[0];
}

void report_sensordata(struct ssp_data *data, int sensor,
			struct sensor_value *sensordata)
{
	if (sensor == PROXIMITY_SENSOR) {
		ssp_info("Proximity Sensor Detect : %u, raw : %u",
			sensordata->prox, sensordata->prox_ex);

	} else if (sensor == PROXIMITY_RAW) {
		report_prox_raw_data(data, sensor, sensordata);
		return;

	} else if (sensor == LIGHT_SENSOR) {
		sensordata->a_gain &= 0x03;

#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
	} else if (sensor == LIGHT_IR_SENSOR) {
		sensordata->ir_a_gain &= 0x03;

		if (data->light_ir_log_cnt < 3) {
			ssp_info("#>IR irdata=%d r=%d g=%d b=%d c=%d atime=%d again=%d",
				sensordata->irdata, sensordata->ir_r,
				sensordata->ir_g, sensordata->ir_b,
				sensordata->ir_w,
				sensordata->ir_a_time, sensordata->ir_a_gain);
			data->light_ir_log_cnt++;
		}
#endif
	} else if (sensor == STEP_COUNTER) {
		data->buf[sensor].step_total += sensordata->step_diff;

	} else if (sensor == SHAKE_CAM) {
		report_shake_cam_data(data, sensordata);
		return;
	}

	memcpy(&data->buf[sensor], (char *)sensordata, data->data_len[sensor]);
	ssp_iio_push_buffers(data->indio_devs[sensor], sensordata->timestamp,
			(char *)&data->buf[sensor], data->report_len[sensor]);

	/* wake-up sensor */
	if (sensor == PROXIMITY_SENSOR ||
		sensor == SIG_MOTION_SENSOR) {
		wake_lock_timeout(&data->ssp_wake_lock, 3 * HZ);
	}
}

void report_meta_data(struct ssp_data *data, int sensor, struct sensor_value *s)
{
	ssp_infof("what: %d, sensor: %d",
		s->meta_data.what, s->meta_data.sensor);

	if ((s->meta_data.sensor == ACCELEROMETER_SENSOR)
		|| (s->meta_data.sensor == GYROSCOPE_SENSOR)
		|| (s->meta_data.sensor == PRESSURE_SENSOR)
		|| (s->meta_data.sensor == ROTATION_VECTOR)
		|| (s->meta_data.sensor == GAME_ROTATION_VECTOR)
		|| (s->meta_data.sensor == STEP_DETECTOR)) {
		char *meta_event
			= kzalloc(data->report_len[s->meta_data.sensor],
					GFP_KERNEL);
		if (!meta_event) {
			ssp_errf("fail to allocate memory for meta event");
			return;
		}

		memset(meta_event, META_EVENT,
			data->report_len[s->meta_data.sensor]);
		ssp_iio_push_buffers(data->indio_devs[s->meta_data.sensor],
				META_TIMESTAMP,	meta_event,
				data->report_len[s->meta_data.sensor]);
		kfree(meta_event);
	} else {
		ssp_iio_push_buffers(data->indio_devs[sensor],
				META_TIMESTAMP,	(char *)&s->meta_data,
				sizeof(s->meta_data));
	}
}

static void *init_indio_device(struct ssp_data *data,
			const struct iio_info *info,
			const struct iio_chan_spec *channels,
			const char *device_name)
{
	struct iio_dev *indio_dev;
	int ret = 0;

	indio_dev = iio_device_alloc(0);
	if (!indio_dev)
		goto err_alloc;

	indio_dev->name = device_name;
	indio_dev->dev.parent = &data->spi->dev;
	indio_dev->info = info;
	indio_dev->channels = channels;
	indio_dev->num_channels = 1;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	ret = ssp_iio_configure_ring(indio_dev);
	if (ret)
		goto err_config_ring;

	ret = iio_buffer_register(indio_dev,
			indio_dev->channels, indio_dev->num_channels);
	if (ret)
		goto err_register_buffer;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto err_register_device;

	return indio_dev;

err_register_device:
	ssp_err("fail to register %s device", device_name);
	iio_buffer_unregister(indio_dev);
err_register_buffer:
	ssp_err("fail to register %s buffer", device_name);
	iio_kfifo_free(indio_dev->buffer);
err_config_ring:
	ssp_err("fail to configure %s ring buffer", device_name);
	iio_device_free(indio_dev);
err_alloc:
	ssp_err("fail to allocate memory for iio %s device", device_name);
	return NULL;
}

static const struct iio_info indio_info = {
	.driver_module = THIS_MODULE,
};

int initialize_indio_dev(struct ssp_data *data)
{
	int timestamp_len = 0;
	int sensor;

	init_sensorlist(data);

	for (sensor = 0; sensor < SENSOR_MAX; sensor++) {
		if (!data->enable[sensor])
			continue;

		timestamp_len = sizeof(data->buf[sensor].timestamp);

		data->indio_channels[sensor].type = IIO_TIMESTAMP;
		data->indio_channels[sensor].channel = IIO_CHANNEL;
		data->indio_channels[sensor].scan_index = IIO_SCAN_INDEX;
		data->indio_channels[sensor].scan_type.sign = IIO_SIGN;
		data->indio_channels[sensor].scan_type.realbits =
			(data->report_len[sensor]+timestamp_len)*BITS_PER_BYTE;
		data->indio_channels[sensor].scan_type.storagebits =
			(data->report_len[sensor]+timestamp_len)*BITS_PER_BYTE;
		data->indio_channels[sensor].scan_type.shift = IIO_SHIFT;

		data->indio_devs[sensor]
			= (struct iio_dev *)init_indio_device(data,
				&indio_info, &data->indio_channels[sensor],
				data->name[sensor]);
		if (!data->indio_devs[sensor]) {
			ssp_err("fail to init %s iio dev", data->name[sensor]);
			remove_indio_dev(data);
			return ERROR;
		}
	}

	return SUCCESS;
}

void remove_indio_dev(struct ssp_data *data)
{
	int sensor;

	for (sensor = SENSOR_MAX-1; sensor >= 0; sensor--) {
		if (data->indio_devs[sensor])
			iio_device_unregister(data->indio_devs[sensor]);
	}
}
