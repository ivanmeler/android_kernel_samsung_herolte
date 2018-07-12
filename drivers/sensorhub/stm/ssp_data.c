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

#include "ssp_data.h"


static void generate_data(struct ssp_data *data, struct sensor_value *sensorsdata,
						int sensor, u64 timestamp)
{
	u64 move_timestamp = data->lastTimestamp[sensor];
	if ((sensor != PROXIMITY_SENSOR) && (sensor != GESTURE_SENSOR)
		&& (sensor != STEP_DETECTOR) && (sensor != SIG_MOTION_SENSOR)
		&& (sensor != STEP_COUNTER)) {
		while ((move_timestamp * 10 + data->adDelayBuf[sensor] * 15) < (timestamp * 10)) {
			move_timestamp += data->adDelayBuf[sensor];
			sensorsdata->timestamp = move_timestamp;
			report_sensordata(data, sensor, sensorsdata);
		}
	}
}

static void get_timestamp(struct ssp_data *data, char *dataframe,
		int *index, struct sensor_value *sensorsdata,
		struct ssp_time_diff *sensortime, int sensor)
{
	if (sensortime->batch_mode == BATCH_MODE_RUN) {
		if (sensortime->batch_count == sensortime->batch_count_fixed) {
			if (sensortime->time_diff == data->adDelayBuf[sensor]) {
				generate_data(data, sensorsdata, sensor,
						(data->timestamp - data->adDelayBuf[sensor] * (sensortime->batch_count_fixed - 1)));
			}
			sensorsdata->timestamp = data->timestamp - ((sensortime->batch_count - 1) * sensortime->time_diff);
		} else {
			if (sensortime->batch_count > 1)
				sensorsdata->timestamp = data->timestamp - ((sensortime->batch_count - 1) * sensortime->time_diff);
			else
				sensorsdata->timestamp = data->timestamp;
		}
	} else {
		if (((sensortime->irq_diff * 10) > (data->adDelayBuf[sensor] * 15))
			&& ((sensortime->irq_diff * 10) < (data->adDelayBuf[sensor] * 100))) {
			generate_data(data, sensorsdata, sensor, data->timestamp);
		}
		sensorsdata->timestamp = data->timestamp;
	}
	*index += 4;
}

void get_sensordata(struct ssp_data *data, char *dataframe,
		int *index, int sensor, struct sensor_value *sensordata)
{
	memcpy(sensordata, dataframe + *index, data->data_len[sensor]);
	*index += data->data_len[sensor];
}

int handle_big_data(struct ssp_data *data, char *dataframe, int *pDataIdx)
{
	u8 bigType = 0;
	struct ssp_big *big = kzalloc(sizeof(*big), GFP_KERNEL);
	big->data = data;
	bigType = dataframe[(*pDataIdx)++];
	memcpy(&big->length, dataframe + *pDataIdx, 4);
	*pDataIdx += 4;
	memcpy(&big->addr, dataframe + *pDataIdx, 4);
	*pDataIdx += 4;

	if (bigType >= BIG_TYPE_MAX) {
		kfree(big);
		return FAIL;
	}

	INIT_WORK(&big->work, data->ssp_big_task[bigType]);
	queue_work(data->debug_wq, &big->work);
	return SUCCESS;
}

void refresh_task(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
			struct ssp_data, work_refresh);

	if (data->bSspShutdown == true) {
		ssp_errf("ssp already shutdown");
		return;
	}

	wake_lock(&data->ssp_wake_lock);
	ssp_errf();
	data->uResetCnt++;

	if (initialize_mcu(data) > 0) {
		sync_sensor_state(data);
		ssp_sensorhub_report_notice(data, MSG2SSP_AP_STATUS_RESET);
		if (data->uLastAPState != 0)
			ssp_send_cmd(data, data->uLastAPState, 0);
		if (data->uLastResumeState != 0)
			ssp_send_cmd(data, data->uLastResumeState, 0);
		data->uTimeOutCnt = 0;
	} else
		data->uSensorState = 0;

	wake_unlock(&data->ssp_wake_lock);
}

int queue_refresh_task(struct ssp_data *data, int delay)
{
	cancel_delayed_work_sync(&data->work_refresh);

	INIT_DELAYED_WORK(&data->work_refresh, refresh_task);
	queue_delayed_work(data->debug_wq, &data->work_refresh,
			msecs_to_jiffies(delay));
	return SUCCESS;
}

int parse_dataframe(struct ssp_data *data, char *dataframe, int frame_len)
{
	struct sensor_value sensorsdata;
	struct ssp_time_diff sensortime;
	int sensor, index;
	u16 length = 0;
	s16 caldata[3] = { 0, };

	memset(&sensorsdata, 0, sizeof(sensorsdata));

	for (index = 0; index < frame_len;) {
		switch (dataframe[index++]) {
		case MSG2AP_INST_BYPASS_DATA:
			sensor = dataframe[index++];
			if ((sensor < 0) || (sensor >= SENSOR_MAX)) {
				ssp_errf("Mcu bypass dataframe err %d", sensor);
				return ERROR;
			}

			memcpy(&length, dataframe + index, 2);
			index += 2;
			sensortime.batch_count = sensortime.batch_count_fixed = length;
			sensortime.batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;
			sensortime.irq_diff = data->timestamp - data->lastTimestamp[sensor];

			if (sensortime.batch_mode == BATCH_MODE_RUN) {
				if (data->reportedData[sensor] == true) {
					u64 time;
					sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[sensor]), (s64)length);
					if (length > 8)
						time = data->adDelayBuf[sensor] * 18;
					else if (length > 4)
						time = data->adDelayBuf[sensor] * 25;
					else if (length > 2)
						time = data->adDelayBuf[sensor] * 50;
					else
						time = data->adDelayBuf[sensor] * 100;
					if ((sensortime.time_diff * 10) > time) {
						data->lastTimestamp[sensor] = data->timestamp - (data->adDelayBuf[sensor] * length);
						sensortime.time_diff = data->adDelayBuf[sensor];
					} else {
						time = data->adDelayBuf[sensor] * 11;
						if ((sensortime.time_diff * 10) > time)
							sensortime.time_diff = data->adDelayBuf[sensor];
					}
				} else {
					if (data->lastTimestamp[sensor] < (data->timestamp - (data->adDelayBuf[sensor] * length))) {
						data->lastTimestamp[sensor] = data->timestamp - (data->adDelayBuf[sensor] * length);
						sensortime.time_diff = data->adDelayBuf[sensor];
					} else
						sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[sensor]), (s64)length);
				}
			} else {
				if (data->reportedData[sensor] == false)
					sensortime.irq_diff = data->adDelayBuf[sensor];
			}

			do {
				get_sensordata(data, dataframe, &index,
					sensor, &sensorsdata);

				get_timestamp(data, dataframe, &index, &sensorsdata, &sensortime, sensor);
				if (sensortime.irq_diff > 1000000)
					report_sensordata(data, sensor, &sensorsdata);
				else if ((sensor == PROXIMITY_SENSOR) || (sensor == PROXIMITY_RAW)
						|| (sensor == GESTURE_SENSOR) || (sensor == SIG_MOTION_SENSOR))
					report_sensordata(data, sensor, &sensorsdata);
				else
					ssp_errf("irq_diff is under 1msec (%d)", sensor);
				sensortime.batch_count--;
			} while ((sensortime.batch_count > 0) && (index < frame_len));

			if (sensortime.batch_count > 0)
				ssp_errf("batch count error (%d)", sensortime.batch_count);

			data->lastTimestamp[sensor] = data->timestamp;
			data->reportedData[sensor] = true;
			break;
		case MSG2AP_INST_DEBUG_DATA:
			sensor = print_mcu_debug(dataframe, &index, frame_len);
			if (sensor) {
				ssp_errf("Mcu debug dataframe err %d", sensor);
				return ERROR;
			}
			break;
		case MSG2AP_INST_LIBRARY_DATA:
			memcpy(&length, dataframe + index, 2);
			index += 2;
			ssp_sensorhub_handle_data(data, dataframe, index,
					index + length);
			index += length;
			break;
		case MSG2AP_INST_BIG_DATA:
			handle_big_data(data, dataframe, &index);
			break;
		case MSG2AP_INST_META_DATA:
			sensorsdata.meta_data.what = dataframe[index++];
			sensorsdata.meta_data.sensor = dataframe[index++];
			report_meta_data(data, META_SENSOR, &sensorsdata);
			break;
		case MSG2AP_INST_TIME_SYNC:
			data->bTimeSyncing = true;
			break;
		case MSG2AP_INST_RESET:
			ssp_infof("Reset MSG received from MCU");
			queue_refresh_task(data, 0);
			break;
		case MSG2AP_INST_GYRO_CAL:
			ssp_infof("Gyro caldata received from MCU");
			memcpy(caldata, dataframe + index, sizeof(caldata));
			wake_lock(&data->ssp_wake_lock);
			save_gyro_caldata(data, caldata);
			wake_unlock(&data->ssp_wake_lock);
			index += sizeof(caldata);
			break;
		case MSG2AP_INST_DUMP_DATA:
			debug_crash_dump(data, dataframe, frame_len);
			return SUCCESS;
			break;
		}
	}

	return SUCCESS;
}

void initialize_function_pointer(struct ssp_data *data)
{
	data->ssp_big_task[BIG_TYPE_DUMP] = ssp_dump_task;
	data->ssp_big_task[BIG_TYPE_READ_LIB] = ssp_read_big_library_task;
}
