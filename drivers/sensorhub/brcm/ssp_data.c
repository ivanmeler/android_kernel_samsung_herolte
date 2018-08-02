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
#include <linux/math64.h>
#include <linux/sched.h>

/* SSP -> AP Instruction */
#define MSG2AP_INST_BYPASS_DATA			0x37
#define MSG2AP_INST_LIBRARY_DATA		0x01
#define MSG2AP_INST_DEBUG_DATA			0x03
#define MSG2AP_INST_BIG_DATA			0x04
#define MSG2AP_INST_META_DATA			0x05
#define MSG2AP_INST_TIME_SYNC			0x06
#define MSG2AP_INST_RESET			0x07
#define MSG2AP_INST_GYRO_CAL			0x08
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_COMMON
#define MSG2AP_INST_MAG_CAL             0x09
#endif
#define MSG2AP_INST_SENSOR_INIT_DONE	0x0a
#define MSG2AP_INST_COLLECT_BIGDATA          0x0b
#define MSG2AP_INST_SCONTEXT_DATA		0x0c

#define CAL_DATA_FOR_BIG                             0x01

#define VDIS_TIMESTAMP_FORMAT 0xFFFFFFFF
#define NORMAL_TIMESTAMP_FORMAT 0x0
#define get_prev_index(a) (a - 1 + SIZE_TIMESTAMP_BUFFER) % SIZE_TIMESTAMP_BUFFER
#define get_next_index(a) (a + 1) % SIZE_TIMESTAMP_BUFFER

#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING // HIFI batch
#define U64_MS2NS 1000000ULL
#define U64_US2NS 1000ULL
#define U64_MS2US 1000ULL
#define MS_IDENTIFIER 1000000000U
#endif

u64 get_delay(u64 kernel_delay){
	u64 ret = kernel_delay;
	u64 ms_delay = kernel_delay/1000000;

	if(ms_delay == 200)
	{
		ret = 180 * 1000000;
	}
	else if(ms_delay >= 60 && ms_delay < 70)
	{
		ret = 60 * 1000000;
	}
	return ret;
}

/*
	Compensate timestamp for jitter.
*/
u64 get_leveling_timestamp(u64 timestamp, struct ssp_data *data, u64 time_delta, int sensor_type){
	u64 ret = timestamp;
	u64 base_time = get_delay(data->adDelayBuf[sensor_type]);
	u64 prev_time = data->lastTimestamp[sensor_type];
	u64 threshold = base_time + prev_time + ((base_time / 10000) * data->timestamp_factor);
	u64 level_threshold = base_time + prev_time + ((base_time / 10) * 19);

	if(data->first_sensor_data[sensor_type] == true){
		if(time_delta == 1){
			// check first data for base_time
			//pr_err("[SSP_DEBUG_TIME] sensor_type: %2d first_timestamp: %lld\n", sensor_type, timestamp);
			data->first_sensor_data[sensor_type] = false;
		}
		goto exit_current;
	}

	if(threshold < timestamp && level_threshold > timestamp){
		return timestamp - threshold < 30 * 1000000 ? threshold : timestamp;
	}

exit_current:
	return ret;
}

static void get_timestamp(struct ssp_data *data, char *pchRcvDataFrame,
		int *iDataIdx, struct sensor_value *sensorsdata,
		u16 batch_mode, int sensor_type)
{
#if ANDROID_VERSION < 80000
	unsigned int idxTimestamp = 0;
	unsigned int time_delta_us = 0;
	u64 time_delta_ns = 0;
	u64 update_timestamp = 0;
	u64 current_timestamp = get_current_timestamp();
	unsigned int ts_index = data->ts_stacked_cnt;

	memset(&time_delta_us, 0, 4);
	memcpy(&time_delta_us, pchRcvDataFrame + *iDataIdx, 4);
	memset(&idxTimestamp, 0, 4);
	memcpy(&idxTimestamp, pchRcvDataFrame + *iDataIdx + 4, 4);

	if (time_delta_us > MS_IDENTIFIER) {
		//We condsider, unit is ms (MS->NS)
		time_delta_ns = ((u64) (time_delta_us % MS_IDENTIFIER)) * U64_MS2NS;
	} else {
		time_delta_ns = (((u64) time_delta_us) * U64_US2NS);//US->NS
	}

	// TODO: Reverse calculation of timestamp when non wake up batching.
	if (batch_mode == BATCH_MODE_RUN) {
		// BATCHING MODE
		//ssp_dbg("[SSP_TIM] BATCH [%3d] TS %lld %lld\n", sensor_type, data->lastTimestamp[sensor_type],time_delta_ns);
		data->lastTimestamp[sensor_type] += time_delta_ns;
	} else {
		// NORMAL MODE
		switch (sensor_type) {
		case ACCELEROMETER_SENSOR:
		case GYROSCOPE_SENSOR:
		case GYRO_UNCALIB_SENSOR:
		case GEOMAGNETIC_SENSOR:
		case GEOMAGNETIC_UNCALIB_SENSOR:
		case GEOMAGNETIC_RAW:
		case ROTATION_VECTOR:
		case GAME_ROTATION_VECTOR:
		case PRESSURE_SENSOR:
		case LIGHT_SENSOR:
		case PROXIMITY_RAW:
			if (data->ts_stacked_cnt < idxTimestamp) {
				int i = 0;
				unsigned int offset = idxTimestamp - ts_index;

				data->ts_stacked_cnt += offset;
				update_timestamp = get_leveling_timestamp(current_timestamp, data, time_delta_us, sensor_type);

				ssp_debug_time("[SSP_DEBUG_TIME] sensor_type: %d stacked_cnt: %d idx: %d current_timestamp: %lld replace_timestamp: %lld offset: %d\n", sensor_type, ts_index, idxTimestamp, current_timestamp, update_timestamp, offset);

				// INSERT DUMMY INDEXING & SKIP 1 EVENT.
				for (i = 1; i <= offset; i++) {
					data->ts_index_buffer[(ts_index + i) % SIZE_TIMESTAMP_BUFFER] = data->ts_irq_last < update_timestamp ? update_timestamp : current_timestamp;
				}
				//data->ts_stacked_cnt += offset;
				data->skipEventReport = true;// SKIP OVERFLOWED INDEX
			} else if((sensor_type == GYROSCOPE_SENSOR) && (data->cameraGyroSyncMode == true)) {
			    //specific case for camera sync(gyro)
			    if (data->lastTimestamp[sensor_type] > data->ts_index_buffer[idxTimestamp % SIZE_TIMESTAMP_BUFFER])
			        data->lastTimestamp[sensor_type] = data->lastTimestamp[sensor_type] + time_delta_us;
			    else
			        data->lastTimestamp[sensor_type] = data->ts_index_buffer[idxTimestamp % SIZE_TIMESTAMP_BUFFER];
			} else {
				u64 received_timestamp = data->ts_index_buffer[idxTimestamp % SIZE_TIMESTAMP_BUFFER];
				//ToDo: conditional leveling
				update_timestamp = get_leveling_timestamp(received_timestamp, data, time_delta_us, sensor_type);
				ssp_debug_time("[SSP_DEBUG_TIME] sensor_type: %2d received_timestamp: %lld update_timestamp: %lld diff: %lld latency: %lld idx: %d\n", sensor_type, received_timestamp, update_timestamp, update_timestamp - data->lastTimestamp[sensor_type], current_timestamp - update_timestamp, idxTimestamp % SIZE_TIMESTAMP_BUFFER);
				data->lastTimestamp[sensor_type] = update_timestamp;
			}

			break;

		// None Indexing
		default:
			data->lastTimestamp[sensor_type] = data->timestamp;
			break;
		}
	}
#else
	u64 time_delta_ns = 0;
	u64 update_timestamp = 0;
	u64 current_timestamp = get_current_timestamp();
	u32 ts_index = 0;
	u32 ts_flag = 0;

	if (data->IsVDIS_Enabled == true && sensor_type == GYROSCOPE_SENSOR) {

		memcpy(&ts_index, pchRcvDataFrame + *iDataIdx, 4);
		memcpy(&ts_flag, pchRcvDataFrame + *iDataIdx + 4, 4);

		if (ts_flag == VDIS_TIMESTAMP_FORMAT) {
			u64 prev_index = get_prev_index(ts_index);

			if (data->ts_index_buffer[ts_index] < data->ts_index_buffer[prev_index]) {
				int i = 0;
				ssp_debug_time("[SSP_DEBUG_TIME] ts_index: %u prev_index: %llu stacked_cnt: %d", 
						ts_index, prev_index, data->ts_stacked_cnt);
				for (i = data->ts_stacked_cnt; i != ts_index; i = get_next_index(i)) {
					data->ts_index_buffer[get_next_index(i)] = data->ts_index_buffer[i] 
						+ data->adDelayBuf[GYROSCOPE_SENSOR];
				}
			}
			time_delta_ns = data->ts_index_buffer[ts_index];
		} else {
			pr_err("[SSP_TIME] ts_flag(%x), error\n", ts_flag);
			goto normal_parse;
		}
	} else {
normal_parse:
		memset(&time_delta_ns, 0, 8);
		memcpy(&time_delta_ns, pchRcvDataFrame + *iDataIdx, 8);
	}

	update_timestamp = time_delta_ns;

	if (ts_flag == VDIS_TIMESTAMP_FORMAT) {
		ssp_debug_time("[SSP_DEBUG_TIME] ts_index: %u stacked_cnt: %u ts_flag: %x timestamp: %llu ts_buffer: %llu", ts_index,
				data->ts_stacked_cnt, ts_flag, time_delta_ns, data->ts_index_buffer[ts_index]);
	} else { 
		ssp_debug_time("[SSP_DEBUG_TIME] sensor_type: %2d update_ts: %lld current_ts: %lld diff: %lld latency: %lld\n",
			sensor_type, update_timestamp, current_timestamp,
			update_timestamp - data->lastTimestamp[sensor_type], current_timestamp - update_timestamp);
	}

	data->lastTimestamp[sensor_type] = time_delta_ns;
#endif
	sensorsdata->timestamp = data->lastTimestamp[sensor_type];
	*iDataIdx += 8;
}

static void get_3axis_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 6);
	*iDataIdx += 6;
}
#if ANDROID_VERSION >= 80000
static void get_gyro_sensordata(char *pchRcvDataFrame, int *iDataIdx,
struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 12);
	*iDataIdx += 12;
}
#endif
static void get_uncalib_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
#if ANDROID_VERSION < 80000
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 12);
	*iDataIdx += 12;
#else

	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 24);
	*iDataIdx += 24;
#endif
}

static void get_geomagnetic_uncaldata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, UNCAL_MAGNETIC_SIZE);
	*iDataIdx += UNCAL_MAGNETIC_SIZE;
}

static void get_geomagnetic_caldata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, MAGNETIC_SIZE);
	*iDataIdx += MAGNETIC_SIZE;
}

static void get_geomagnetic_rawdata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 6);
	*iDataIdx += 6;
}

static void get_rot_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 17);
	*iDataIdx += 17;
}

static void get_step_det_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}

static void get_light_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
#ifdef CONFIG_SENSORS_SSP_LIGHT_REPORT_LUX
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 18);
	*iDataIdx += 18;
#else
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 10);
	*iDataIdx += 10;
#endif
}

#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
static void get_light_ir_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 12);
	*iDataIdx += 12;
}
#endif

static void get_light_flicker_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(&sensorsdata->light_flicker, pchRcvDataFrame + *iDataIdx, 2);
	*iDataIdx += 2;
}
#if ANDROID_VERSION >= 80000
static void get_light_cct_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
#ifdef CONFIG_SENSORS_SSP_LIGHT_REPORT_LUX
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 18);
	*iDataIdx += 18;
#else
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 10);
	*iDataIdx += 10;
#endif
}
#endif
static void get_pressure_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 6);
	*iDataIdx += 6;
}

static void get_gesture_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 20);
	*iDataIdx += 20;
}

static void get_proximity_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
#if defined(CONFIG_SENSORS_SSP_TMG399x) || defined(CONFIG_SENSORS_SSP_TMD3725)
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 2);
	*iDataIdx += 2;
#else	//CONFIG_SENSORS_SSP_TMD4903, CONFIG_SENSORS_SSP_TMD3782, CONFIG_SENSORS_SSP_TMD4904
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 3);
	*iDataIdx += 3;
#endif
}

static void get_proximity_alert_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 3);
	*iDataIdx += 3;
}

static void get_proximity_rawdata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
#if defined(CONFIG_SENSORS_SSP_TMG399x) || defined(CONFIG_SENSORS_SSP_TMD3725)
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
#else	//CONFIG_SENSORS_SSP_TMD4903, CONFIG_SENSORS_SSP_TMD3782, CONFIG_SENSORS_SSP_TMD4904
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 2);
	*iDataIdx += 2;
#endif
}

#ifdef CONFIG_SENSORS_SSP_SX9306
static void get_grip_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 9);
		*iDataIdx += 9;
}
#endif

static void get_temp_humidity_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memset(&sensorsdata->data[2], 0, 2);
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 5);
	*iDataIdx += 5;
}

static void get_sig_motion_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}

static void get_step_cnt_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(&sensorsdata->step_diff, pchRcvDataFrame + *iDataIdx, 4);
	*iDataIdx += 4;
}

static void get_shake_cam_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}

static void get_tilt_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}

static void get_pickup_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 1);
	*iDataIdx += 1;
}
#if ANDROID_VERSION >= 80000
static void get_ucal_accel_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, 12);
	*iDataIdx += 12;
}
#endif
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING // HIFI batch
/*
static void get_sensor_data(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata, int data_size)
{
	memcpy(sensorsdata, pchRcvDataFrame + *iDataIdx, data_size);
	*iDataIdx += data_size;
}
*/

bool ssp_check_buffer(struct ssp_data *data)
{
	int idx_data = 0;
	u8 sensor_type = 0;
	bool res = true;
	u64 ts = get_current_timestamp();
	pr_err("[SSP_BUF] start check %lld\n", ts);
	do{
		sensor_type = data->batch_event.batch_data[idx_data++];

		if ((sensor_type != ACCELEROMETER_SENSOR) &&
			(sensor_type != GEOMAGNETIC_UNCALIB_SENSOR) &&
			(sensor_type != PRESSURE_SENSOR) &&
			(sensor_type != GAME_ROTATION_VECTOR) &&
			(sensor_type != PROXIMITY_SENSOR) &&
			(sensor_type != META_SENSOR)
#if ANDROID_VERSION >= 80000
			&& (sensor_type != ACCEL_UNCALIB_SENSOR)
#endif
			) {
			pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
					sensor_type, idx_data - 1);
			res = false;
			break;
		}

		switch (sensor_type) {
		case ACCELEROMETER_SENSOR:
			idx_data += 14;
			break;
		case GEOMAGNETIC_UNCALIB_SENSOR:
#ifdef CONFIG_SSP_SUPPORT_MAGNETIC_OVERFLOW
            idx_data += 21;
#else
            idx_data += 20;
#endif
            break;
#if ANDROID_VERSION >= 80000
         case ACCEL_UNCALIB_SENSOR:
            idx_data += 20;
			break;
#endif
		case PRESSURE_SENSOR:
			idx_data += 14;
			break;
		case GAME_ROTATION_VECTOR:
			idx_data += 25;
			break;
		case PROXIMITY_SENSOR:
			idx_data += 11;
			break;
		case META_SENSOR:
			idx_data += 1;
			break;
		}

		if(idx_data > data->batch_event.batch_length){
			//stop index over max length
			pr_info("[SSP_CHK] invalid data1\n");
			res = false;
			break;
		}

		// run until max length
		if(idx_data == data->batch_event.batch_length){
			//pr_info("[SSP_CHK] valid data\n");
			break;
		} else if (idx_data + 1 == data->batch_event.batch_length) {
			//stop if only sensor type exist
			pr_info("[SSP_CHK] invalid data2\n");
			res = false;
			break;
		}
	}while(true);
	ts = get_current_timestamp();
	pr_err("[SSP_BUF] finish check %lld\n", ts);

	return res;
}

void ssp_batch_resume_check(struct ssp_data *data)
{
	u64 acc_offset = 0, uncal_mag_offset = 0, press_offset = 0, grv_offset = 0, proxi_offset = 0;
	//if suspend -> wakeup case. calc. FIFO last timestamp
	if (data->bIsResumed) {
		u8 sensor_type = 0;
		struct sensor_value sensor_data;
#if ANDROID_VERSION < 80000
		unsigned int delta_time_us = 0;
		unsigned int idx_timestamp = 0;
#else
		u64 delta_time_us = 0;
#endif
		int idx_data = 0;
		u64 timestamp = get_current_timestamp();
		//ssp_dbg("[SSP_BAT] LENGTH = %d, start index = %d ts %lld resume %lld\n", data->batch_event.batch_length, idx_data, timestamp, data->resumeTimestamp);

		timestamp = data->resumeTimestamp = data->timestamp;

		while (idx_data < data->batch_event.batch_length) {
			sensor_type = data->batch_event.batch_data[idx_data++];
			if(sensor_type == META_SENSOR)	{
				sensor_data.meta_data.sensor = data->batch_event.batch_data[idx_data++];
				continue;
			}

			if ((sensor_type != ACCELEROMETER_SENSOR) &&
				(sensor_type != GEOMAGNETIC_UNCALIB_SENSOR) &&
				(sensor_type != PRESSURE_SENSOR) &&
				(sensor_type != GAME_ROTATION_VECTOR) &&
				(sensor_type != PROXIMITY_SENSOR)
#if ANDROID_VERSION >= 80000
			&& (sensor_type != ACCEL_UNCALIB_SENSOR)
#endif
			) {
				pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
						sensor_type, idx_data - 1);
				data->bIsResumed = false;
				data->resumeTimestamp = 0ULL;
				return ;
			}

			data->get_sensor_data[sensor_type](data->batch_event.batch_data, &idx_data, &sensor_data);
#if ANDROID_VERSION < 80000
			memset(&delta_time_us, 0, 4);
			memcpy(&delta_time_us, data->batch_event.batch_data + idx_data, 4);
			memset(&idx_timestamp, 0, 4);
			memcpy(&idx_timestamp, data->batch_event.batch_data + idx_data + 4, 4);
			if (delta_time_us > MS_IDENTIFIER) {
				//We condsider, unit is ms (MS->NS)
				delta_time_us = ((u64) (delta_time_us % MS_IDENTIFIER)) * U64_MS2NS;
			} else {
				delta_time_us = (((u64) delta_time_us) * U64_US2NS);//US->NS
			}
#else
			memset(&delta_time_us, 0, 8);
			memcpy(&delta_time_us, data->batch_event.batch_data + idx_data, 8);
#endif
			switch (sensor_type) {
#if ANDROID_VERSION < 80000
				case ACCELEROMETER_SENSOR:
					acc_offset += delta_time_us;
					break;
				case GEOMAGNETIC_UNCALIB_SENSOR:
					uncal_mag_offset += delta_time_us;
					break;
				case GAME_ROTATION_VECTOR:
					grv_offset += delta_time_us;
					break;
				case PRESSURE_SENSOR:
					press_offset += delta_time_us;
					break;
				case PROXIMITY_SENSOR:
					proxi_offset += delta_time_us;
					break;
				default:
					break;
#else
			case ACCELEROMETER_SENSOR:
			case GEOMAGNETIC_UNCALIB_SENSOR:
			case GAME_ROTATION_VECTOR:
			case PRESSURE_SENSOR:
			case PROXIMITY_SENSOR:
#if ANDROID_VERSION >= 80000
                        case ACCEL_UNCALIB_SENSOR:
#endif
				data->lastTimestamp[sensor_type] = delta_time_us;
				break;
#endif
			}
			idx_data += 8;
		}

#if ANDROID_VERSION < 80000
		if(acc_offset > 0)
			data->lastTimestamp[ACCELEROMETER_SENSOR] = timestamp - acc_offset;
		if(uncal_mag_offset > 0)
			data->lastTimestamp[GEOMAGNETIC_UNCALIB_SENSOR] = timestamp - uncal_mag_offset;
		if(press_offset > 0)
			data->lastTimestamp[PRESSURE_SENSOR] = timestamp - press_offset;
		if(grv_offset > 0)
			data->lastTimestamp[GAME_ROTATION_VECTOR] = timestamp - grv_offset;
		if(proxi_offset > 0)
			data->lastTimestamp[PROXIMITY_SENSOR] = timestamp - proxi_offset;
#endif
		ssp_dbg("[SSP_BAT] resume calc. acc %lld. uncalmag %lld. pressure %lld. GRV %lld proxi %lld \n",
			acc_offset, uncal_mag_offset, press_offset, grv_offset, proxi_offset);
	}
	data->bIsResumed = false;
	data->resumeTimestamp = 0ULL;
}

void ssp_batch_report(struct ssp_data *data)
{
	u8 sensor_type = 0;
	struct sensor_value sensor_data;
	int idx_data = 0;
	int count = 0;
	u64 timestamp = get_current_timestamp();

	ssp_dbg("[SSP_BAT] LENGTH = %d, start index = %d ts %lld\n", data->batch_event.batch_length, idx_data, timestamp);

	while (idx_data < data->batch_event.batch_length)
	{
		//ssp_dbg("[SSP_BAT] bcnt %d\n", count);
		sensor_type = data->batch_event.batch_data[idx_data++];

		if(sensor_type == META_SENSOR)	{
			sensor_data.meta_data.sensor = data->batch_event.batch_data[idx_data++];
			report_meta_data(data, &sensor_data);
			count++;
			continue;
		}

		if ((sensor_type != ACCELEROMETER_SENSOR) &&
			(sensor_type != GEOMAGNETIC_UNCALIB_SENSOR) &&
			(sensor_type != PRESSURE_SENSOR) &&
			(sensor_type != GAME_ROTATION_VECTOR) &&
			(sensor_type != PROXIMITY_SENSOR)
#if ANDROID_VERSION >= 80000
			&& (sensor_type != ACCEL_UNCALIB_SENSOR)
#endif
			) {
			pr_err("[SSP]: %s - Mcu data frame1 error %d, idx_data %d\n", __func__,
					sensor_type, idx_data - 1);
			return ;
		}

		usleep_range(150, 151);
		//ssp_dbg("[SSP_BAT] cnt %d\n", count);
		data->get_sensor_data[sensor_type](data->batch_event.batch_data, &idx_data, &sensor_data);

		get_timestamp(data, data->batch_event.batch_data, &idx_data, &sensor_data, BATCH_MODE_RUN, sensor_type);
		ssp_debug_time("[SSP_BAT]: sensor %d, AP %lld MCU %lld, diff %lld, count: %d\n",
			sensor_type, timestamp, sensor_data.timestamp, timestamp - sensor_data.timestamp, count);

		data->report_sensor_data[sensor_type](data, &sensor_data);
		data->reportedData[sensor_type] = true;
		count++;
	}
	ssp_dbg("[SSP_BAT] max cnt %d\n", count);
}


// Control batched data with long term
// Ref ssp_read_big_library_task
void ssp_batch_data_read_task(struct work_struct *work)
{
	struct ssp_big *big = container_of(work, struct ssp_big, work);
	struct ssp_data *data = big->data;
	struct ssp_msg *msg;
	int buf_len, residue, ret = 0, index = 0, pos = 0;
	u64 ts = 0;

	mutex_lock(&data->batch_events_lock);
	wake_lock(&data->ssp_wake_lock);

	residue = big->length;
	data->batch_event.batch_length = big->length;
	data->batch_event.batch_data = vmalloc(big->length);
	if (data->batch_event.batch_data == NULL)
	{
		ssp_dbg("[SSP_BAT] batch data alloc fail \n");
		kfree(big);
		wake_unlock(&data->ssp_wake_lock);
		mutex_unlock(&data->batch_events_lock);
		return;
	}

	//ssp_dbg("[SSP_BAT] IN : LENGTH = %d \n", big->length);

	while (residue > 0) {
		buf_len = residue > DATA_PACKET_SIZE
			? DATA_PACKET_SIZE : residue;

		msg = kzalloc(sizeof(*msg),GFP_ATOMIC);
		msg->cmd = MSG2SSP_AP_GET_BIG_DATA;
		msg->length = buf_len;
		msg->options = AP2HUB_READ | (index++ << SSP_INDEX);
		msg->data = big->addr;
		msg->buffer = data->batch_event.batch_data + pos;
		msg->free_buffer = 0;

		ret = ssp_spi_sync(big->data, msg, 1000);
		if (ret != SUCCESS) {
			pr_err("[SSP_BAT] read batch data err(%d) ignor\n", ret);
			vfree(data->batch_event.batch_data);
			data->batch_event.batch_data = NULL;
			data->batch_event.batch_length = 0;
			kfree(big);
			wake_unlock(&data->ssp_wake_lock);
			mutex_unlock(&data->batch_events_lock);
			return;
		}

		pos += buf_len;
		residue -= buf_len;
		pr_info("[SSP_BAT] read batch data (%5d / %5d)\n", pos, big->length);
	}

	// TODO: Do not parse, jut put in to FIFO, and wake_up thread.

	// READ DATA FROM MCU COMPLETED
	//Wake up check
	if(ssp_check_buffer(data)){
		ssp_batch_resume_check(data);

		// PARSE DATA FRAMES, Should run loop
		ts = get_current_timestamp();
		pr_info("[SSP] report start %lld\n", ts);
		ssp_batch_report(data);
		ts = get_current_timestamp();
		pr_info("[SSP] report finish %lld\n", ts);
	}

	vfree(data->batch_event.batch_data);
	data->batch_event.batch_data = NULL;
	data->batch_event.batch_length = 0;
	kfree(big);
	wake_unlock(&data->ssp_wake_lock);
	mutex_unlock(&data->batch_events_lock);
}
#endif
int handle_big_data(struct ssp_data *data, char *pchRcvDataFrame, int *pDataIdx)
{
	u8 bigType = 0;
	struct ssp_big *big = kzalloc(sizeof(*big), GFP_KERNEL);
	big->data = data;
	bigType = pchRcvDataFrame[(*pDataIdx)++];
	memcpy(&big->length, pchRcvDataFrame + *pDataIdx, 4);
	*pDataIdx += 4;
	memcpy(&big->addr, pchRcvDataFrame + *pDataIdx, 4);
	*pDataIdx += 4;

	if (bigType >= BIG_TYPE_MAX) {
		kfree(big);
		return FAIL;
	}

	INIT_WORK(&big->work, data->ssp_big_task[bigType]);
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING // HIFI batch
	if(bigType != BIG_TYPE_READ_HIFI_BATCH)
		queue_work(data->debug_wq, &big->work);
	else
		queue_work(data->batch_wq, &big->work);
#else
	queue_work(data->debug_wq, &big->work);
#endif
	return SUCCESS;
}
#if ANDROID_VERSION >= 80000
void handle_timestamp_sync(struct ssp_data *data, char *pchRcvDataFrame, int *index)
{
	u64 mcu_timestamp;
	u64 current_timestamp = get_current_timestamp();

	memcpy(&mcu_timestamp, pchRcvDataFrame + *index, sizeof(mcu_timestamp));
	data->timestamp_offset = current_timestamp - mcu_timestamp;
	schedule_delayed_work(&data->work_ssp_tiemstamp_sync, msecs_to_jiffies(100));

	*index += 8;
}
#endif

/*************************************************************************/
/* SSP parsing the dataframe                                             */
/*************************************************************************/
int parse_dataframe(struct ssp_data *data, char *pchRcvDataFrame, int iLength)
{
	int iDataIdx;
	int sensor_type;
	u16 length = 0;

	struct sensor_value sensorsdata;
#if ANDROID_VERSION < 80000
	s16 caldata[3] = { 0, };
	int RecheckMsgInst = 0;
#else
	s32 caldata[3] = { 0, };
#endif
	char msg_inst = 0;
	u64 timestampforReset;
#if ANDROID_VERSION < 80000
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING // HIFI batch
	u16 batch_event_count;
	u16 batch_mode;
#else
	struct ssp_time_diff sensortime;
	sensortime.time_diff = 0;
#endif
#endif
	data->uIrqCnt++;

	for (iDataIdx = 0; iDataIdx < iLength;) {
		switch (msg_inst = pchRcvDataFrame[iDataIdx++]) {
#if ANDROID_VERSION >= 80000
		case MSG2AP_INST_TIMESTAMP_OFFSET:
			handle_timestamp_sync(data, pchRcvDataFrame, &iDataIdx);
			break;
#endif
		case MSG2AP_INST_SENSOR_INIT_DONE:
			pr_err("[SSP]: MCU sensor init done\n");
			complete(&data->hub_data->mcu_init_done);
			break;
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING // HIFI batch
		case MSG2AP_INST_BYPASS_DATA:
			sensor_type = pchRcvDataFrame[iDataIdx++];
			if ((sensor_type < 0) || (sensor_type >= SENSOR_MAX)) {
				pr_err("[SSP]: %s - Mcu data frame1 error %d\n", __func__,
						sensor_type);
				goto error_return;
			}
#if ANDROID_VERSION < 80000
			memcpy(&length, pchRcvDataFrame + iDataIdx, 2);
			iDataIdx += 2;

			batch_event_count = length;
			batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;
			if(batch_mode == BATCH_MODE_NONE)
			{
				timestampforReset = get_current_timestamp();
				data->LastSensorTimeforReset[sensor_type] = timestampforReset;
			}
			//pr_err("[SSP]: %s batch count (%d)\n", __func__, batch_event_count);

			// TODO: When batch_event_count = 0, we should not run.
			do {
                                if(data->get_sensor_data[sensor_type] == NULL)
                                    goto error_return;
				data->get_sensor_data[sensor_type](pchRcvDataFrame, &iDataIdx, &sensorsdata);
				// TODO: Integrate get_sensor_data function.
				// TODO: get_sensor_data(pchRcvDataFrame, &iDataIdx, &sensorsdata, data->sensor_data_size[sensor_type]);
				// TODO: Divide control data batch and non batch.

				data->skipEventReport = false;

				//if(sensor_type == GYROSCOPE_SENSOR) //check packet recieve time
				//	pr_err("[SSP_PARSE] bbd event time %lld\n", data->timestamp);
				get_timestamp(data, pchRcvDataFrame, &iDataIdx, &sensorsdata, batch_mode, sensor_type);
				if (data->skipEventReport == false){
					data->report_sensor_data[sensor_type](data, &sensorsdata);
				} else {
					// we've already known index mismatching between AP and sensorhub at get_timestamp().
					// so, skip to check next sensor data at same pakcet.
					batch_event_count = 0;
					break;
				}
				batch_event_count--;
				//pr_err("[SSP]: %s batch count (%d)\n", __func__, batch_event_count);
			} while ((batch_event_count > 0) && (iDataIdx < iLength));

			if (batch_event_count > 0)
				pr_err("[SSP]: %s batch count error (%d)\n", __func__, batch_event_count);
#else

			timestampforReset = get_current_timestamp();
			data->LastSensorTimeforReset[sensor_type] = timestampforReset;

			if (data->get_sensor_data[sensor_type] == NULL)
				goto error_return;

			data->get_sensor_data[sensor_type](pchRcvDataFrame, &iDataIdx, &sensorsdata);
			data->skipEventReport = false;

			get_timestamp(data, pchRcvDataFrame, &iDataIdx, &sensorsdata, 0, sensor_type);
			if (data->skipEventReport == false) {
				//Check sensor is enabled, before report sensordata
				u64 AddedSensorState = (u64)atomic64_read(&data->aSensorEnable);

				if (AddedSensorState & (1ULL << sensor_type)
				|| sensor_type == GEOMAGNETIC_RAW
				|| sensor_type == PROXIMITY_RAW)
					data->report_sensor_data[sensor_type](data, &sensorsdata);
				else {
					if (data->skipSensorData == 30) {
						pr_info("[SSP]: sensor not added,but sensor(%d) came", sensor_type);
						data->skipSensorData = 0;
					}
					data->skipSensorData++;
				}
			}

#endif
			data->reportedData[sensor_type] = true;
			//pr_err("[SSP]: (%d / %d)\n", iDataIdx, iLength);
			break;
		case MSG2AP_INST_DEBUG_DATA:
			sensor_type = print_mcu_debug(pchRcvDataFrame, &iDataIdx, iLength);
			if (sensor_type) {
				pr_err("[SSP]: %s - Mcu data frame3 error %d\n", __func__,
						sensor_type);
				return ERROR;
			}
			break;
#else
		case MSG2AP_INST_BYPASS_DATA:
			sensor_type = pchRcvDataFrame[iDataIdx++];
			if ((sensor_type < 0) || (sensor_type >= SENSOR_MAX)) {
				pr_err("[SSP]: %s - Mcu data frame1 error %d\n", __func__,
						sensor_type);
				return ERROR;
			}

			memcpy(&length, pchRcvDataFrame + iDataIdx, 2);
			iDataIdx += 2;
			sensortime.batch_count = sensortime.batch_count_fixed = length;
			sensortime.batch_mode = length > 1 ? BATCH_MODE_RUN : BATCH_MODE_NONE;
			sensortime.irq_diff = data->timestamp - data->lastTimestamp[sensor_type];

			if (sensortime.batch_mode == BATCH_MODE_RUN) {
				if (data->reportedData[sensor_type] == true) {
					u64 time;
					sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[sensor_type]), (s64)length);
					if (length > 8)
						time = data->adDelayBuf[sensor_type] * 18;
					else if (length > 4)
						time = data->adDelayBuf[sensor_type] * 25;
					else if (length > 2)
						time = data->adDelayBuf[sensor_type] * 50;
					else
						time = data->adDelayBuf[sensor_type] * 130;
					if ((sensortime.time_diff * 10) > time) {
						data->lastTimestamp[sensor_type] = data->timestamp - (data->adDelayBuf[sensor_type] * length);
						sensortime.time_diff = data->adDelayBuf[sensor_type];
					} else {
						time = data->adDelayBuf[sensor_type] * 11;
						if ((sensortime.time_diff * 10) > time)
							sensortime.time_diff = data->adDelayBuf[sensor_type];
					}
				} else {
					if (data->lastTimestamp[sensor_type] < (data->timestamp - (data->adDelayBuf[sensor_type] * length))) {
						data->lastTimestamp[sensor_type] = data->timestamp - (data->adDelayBuf[sensor_type] * length);
						sensortime.time_diff = data->adDelayBuf[sensor_type];
					} else
						sensortime.time_diff = div64_long((s64)(data->timestamp - data->lastTimestamp[sensor_type]), (s64)length);
				}
			} else {
				if (data->reportedData[sensor_type] == false)
					sensortime.irq_diff = data->adDelayBuf[sensor_type];
			}

			do {
				data->get_sensor_data[sensor_type](pchRcvDataFrame, &iDataIdx, &sensorsdata);
				get_timestamp(data, pchRcvDataFrame, &iDataIdx, &sensorsdata, &sensortime, sensor_type);
				if (sensortime.irq_diff > 1000000)
					data->report_sensor_data[sensor_type](data, &sensorsdata);
				else if ((sensor_type == PROXIMITY_SENSOR) || (sensor_type == PROXIMITY_RAW)
						|| (sensor_type == STEP_COUNTER)   || (sensor_type == STEP_DETECTOR)
						|| (sensor_type == GESTURE_SENSOR) || (sensor_type == SIG_MOTION_SENSOR)
						|| (sensor_type == PROXIMITY_ALERT_SENSOR))
					data->report_sensor_data[sensor_type](data, &sensorsdata);
				else
					pr_err("[SSP]: %s irq_diff is under 1msec (%d)\n", __func__, sensor_type);
				sensortime.batch_count--;
			} while ((sensortime.batch_count > 0) && (iDataIdx < iLength));

			if (sensortime.batch_count > 0)
				pr_err("[SSP]: %s batch count error (%d)\n", __func__, sensortime.batch_count);

			data->lastTimestamp[sensor_type] = data->timestamp;
			data->reportedData[sensor_type] = true;
			break;
		case MSG2AP_INST_DEBUG_DATA:
			sensor_type = print_mcu_debug(pchRcvDataFrame, &iDataIdx, iLength);
			if (sensor_type) {
				pr_err("[SSP]: %s - Mcu data frame3 error %d\n", __func__,
						sensor_type);
				return ERROR;
			}
			break;
#endif
		case MSG2AP_INST_LIBRARY_DATA:
#if ANDROID_VERSION < 80000
			RecheckMsgInst = pchRcvDataFrame[iDataIdx++];
			if(RecheckMsgInst != MSG2AP_INST_LIBRARY_DATA)
				 goto error_return; 
#endif
			memcpy(&length, pchRcvDataFrame + iDataIdx, 2);
			iDataIdx += 2;
			ssp_sensorhub_handle_data(data, pchRcvDataFrame, iDataIdx,
					iDataIdx + length);
			iDataIdx += length;
			break;
		case MSG2AP_INST_BIG_DATA:
			handle_big_data(data, pchRcvDataFrame, &iDataIdx);
			break;
		case MSG2AP_INST_META_DATA:
			sensorsdata.meta_data.what = pchRcvDataFrame[iDataIdx++];
			sensorsdata.meta_data.sensor = pchRcvDataFrame[iDataIdx++];

			if(sensorsdata.meta_data.what != 1)
				goto error_return;

			if ((sensorsdata.meta_data.sensor < 0) || (sensorsdata.meta_data.sensor >= SENSOR_MAX)) {
				pr_err("[SSP]: %s - Mcu meta_data frame1 error %d\n", __func__,
						sensorsdata.meta_data.sensor);
				goto error_return;
			}
			report_meta_data(data, &sensorsdata);
			break;
		case MSG2AP_INST_TIME_SYNC:
			data->bTimeSyncing = true;
			break;
		case MSG2AP_INST_GYRO_CAL:
			pr_info("[SSP]: %s - Gyro caldata received from MCU\n",  __func__);
			memcpy(caldata, pchRcvDataFrame + iDataIdx, sizeof(caldata));
			wake_lock(&data->ssp_wake_lock);
			save_gyro_caldata(data, caldata);
			wake_unlock(&data->ssp_wake_lock);
			iDataIdx += sizeof(caldata);
			break;
		case SH_MSG2AP_GYRO_CALIBRATION_EVENT_OCCUR:
			data->gyro_lib_state = GYRO_CALIBRATION_STATE_EVENT_OCCUR;
			break;
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_COMMON            
        case MSG2AP_INST_MAG_CAL:
			wake_lock(&data->ssp_wake_lock);
			save_magnetic_cal_param_to_nvm(data, pchRcvDataFrame, &iDataIdx);
			wake_unlock(&data->ssp_wake_lock);
			break;
#endif
#if ANDROID_VERSION >= 80000
		case MSG2AP_INST_COLLECT_BIGDATA:
			switch (pchRcvDataFrame[iDataIdx++]) {
			case CAL_DATA_FOR_BIG:
				sensor_type = pchRcvDataFrame[iDataIdx++];
				if (sensor_type == ACCELEROMETER_SENSOR)
					set_AccelCalibrationInfoData(pchRcvDataFrame, &iDataIdx);
				else if (sensor_type == GYROSCOPE_SENSOR)
					set_GyroCalibrationInfoData(pchRcvDataFrame, &iDataIdx);
				break;
			default:
				pr_info("[SSP] wrong sub inst for big data\n");
				break;
			}
			break;
		case MSG2AP_INST_SCONTEXT_DATA:
			memcpy(sensorsdata.scontext_buf, pchRcvDataFrame + iDataIdx, SCONTEXT_DATA_SIZE);
			report_scontext_data(data, &sensorsdata);
			iDataIdx += SCONTEXT_DATA_SIZE;
				break;
#endif
		default:
			goto error_return;
		}
	}
	if(data->pktErrCnt >= 1) // if input error packet doesn't comes continually, do not reset
		data->pktErrCnt = 0;
	return SUCCESS;
error_return:
	pr_err("[SSP] %s err Inst 0x%02x\n", __func__, msg_inst);
	data->pktErrCnt++;
	if (data->pktErrCnt >= 2) {
		pr_err("[SSP] %s packet is polluted\n", __func__);
		data->mcuAbnormal = true;
		data->pktErrCnt = 0;
		data->errorCount++;
	}
    return ERROR;
}

void initialize_function_pointer(struct ssp_data *data)
{
	data->get_sensor_data[ACCELEROMETER_SENSOR] = get_3axis_sensordata;
#if ANDROID_VERSION < 80000
	data->get_sensor_data[GYROSCOPE_SENSOR] = get_3axis_sensordata;
#else
	data->get_sensor_data[GYROSCOPE_SENSOR] = get_gyro_sensordata;
#endif
	data->get_sensor_data[GEOMAGNETIC_UNCALIB_SENSOR] =
		get_geomagnetic_uncaldata;
	data->get_sensor_data[GEOMAGNETIC_RAW] = get_geomagnetic_rawdata;
	data->get_sensor_data[GEOMAGNETIC_SENSOR] =
		get_geomagnetic_caldata;
	data->get_sensor_data[PRESSURE_SENSOR] = get_pressure_sensordata;
	data->get_sensor_data[GESTURE_SENSOR] = get_gesture_sensordata;
	data->get_sensor_data[PROXIMITY_SENSOR] = get_proximity_sensordata;
	data->get_sensor_data[PROXIMITY_ALERT_SENSOR] = get_proximity_alert_sensordata;
	data->get_sensor_data[LIGHT_FLICKER_SENSOR] = get_light_flicker_sensordata;

	data->get_sensor_data[PROXIMITY_RAW] = get_proximity_rawdata;
#ifdef CONFIG_SENSORS_SSP_SX9306
	data->get_sensor_data[GRIP_SENSOR] = get_grip_sensordata;
#endif
	data->get_sensor_data[LIGHT_SENSOR] = get_light_sensordata;
#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
	data->get_sensor_data[LIGHT_IR_SENSOR] = get_light_ir_sensordata;
#endif
#if ANDROID_VERSION >= 80000
	data->get_sensor_data[LIGHT_CCT_SENSOR] = get_light_cct_sensordata;
	data->get_sensor_data[ACCEL_UNCALIB_SENSOR] = get_ucal_accel_sensordata;
#endif
	data->get_sensor_data[TEMPERATURE_HUMIDITY_SENSOR] =
		get_temp_humidity_sensordata;
	data->get_sensor_data[ROTATION_VECTOR] = get_rot_sensordata;
	data->get_sensor_data[GAME_ROTATION_VECTOR] = get_rot_sensordata;
	data->get_sensor_data[STEP_DETECTOR] = get_step_det_sensordata;
	data->get_sensor_data[SIG_MOTION_SENSOR] = get_sig_motion_sensordata;
	data->get_sensor_data[GYRO_UNCALIB_SENSOR] = get_uncalib_sensordata;
	data->get_sensor_data[STEP_COUNTER] = get_step_cnt_sensordata;
	data->get_sensor_data[SHAKE_CAM] = get_shake_cam_sensordata;
#ifdef CONFIG_SENSORS_SSP_INTERRUPT_GYRO_SENSOR
#if ANDROID_VERSION < 80000
	data->get_sensor_data[INTERRUPT_GYRO_SENSOR] = get_3axis_sensordata;
#else
	data->get_sensor_data[INTERRUPT_GYRO_SENSOR] = get_gyro_sensordata;
#endif
#endif
	data->get_sensor_data[TILT_DETECTOR] = get_tilt_sensordata;
	data->get_sensor_data[PICKUP_GESTURE] = get_pickup_sensordata;

	data->get_sensor_data[BULK_SENSOR] = NULL;
	data->get_sensor_data[GPS_SENSOR] = NULL;

	data->report_sensor_data[ACCELEROMETER_SENSOR] = report_acc_data;
	data->report_sensor_data[GYROSCOPE_SENSOR] = report_gyro_data;
	data->report_sensor_data[GEOMAGNETIC_UNCALIB_SENSOR] =
		report_mag_uncaldata;
	data->report_sensor_data[GEOMAGNETIC_RAW] = report_geomagnetic_raw_data;
	data->report_sensor_data[GEOMAGNETIC_SENSOR] =
		report_mag_data;
	data->report_sensor_data[PRESSURE_SENSOR] = report_pressure_data;
	data->report_sensor_data[GESTURE_SENSOR] = report_gesture_data;
	data->report_sensor_data[PROXIMITY_SENSOR] = report_prox_data;
	data->report_sensor_data[PROXIMITY_ALERT_SENSOR] = report_prox_alert_data;
	data->report_sensor_data[LIGHT_FLICKER_SENSOR] = report_light_flicker_data;

	data->report_sensor_data[PROXIMITY_RAW] = report_prox_raw_data;
#ifdef CONFIG_SENSORS_SSP_SX9306
	data->report_sensor_data[GRIP_SENSOR] = report_grip_data;
#endif
	data->report_sensor_data[LIGHT_SENSOR] = report_light_data;
#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
	data->report_sensor_data[LIGHT_IR_SENSOR] = report_light_ir_data;
#endif
	data->report_sensor_data[TEMPERATURE_HUMIDITY_SENSOR] =
		report_temp_humidity_data;
	data->report_sensor_data[ROTATION_VECTOR] = report_rot_data;
	data->report_sensor_data[GAME_ROTATION_VECTOR] = report_game_rot_data;
	data->report_sensor_data[STEP_DETECTOR] = report_step_det_data;
	data->report_sensor_data[SIG_MOTION_SENSOR] = report_sig_motion_data;
	data->report_sensor_data[GYRO_UNCALIB_SENSOR] = report_uncalib_gyro_data;
	data->report_sensor_data[STEP_COUNTER] = report_step_cnt_data;
	data->report_sensor_data[SHAKE_CAM] = report_shake_cam_data;
#ifdef CONFIG_SENSORS_SSP_INTERRUPT_GYRO_SENSOR
	data->report_sensor_data[INTERRUPT_GYRO_SENSOR] = report_interrupt_gyro_data;
#endif
	data->report_sensor_data[TILT_DETECTOR] = report_tilt_data;
	data->report_sensor_data[PICKUP_GESTURE] = report_pickup_data;

	data->report_sensor_data[BULK_SENSOR] = NULL;
	data->report_sensor_data[GPS_SENSOR] = NULL;
#if ANDROID_VERSION >= 80000
	data->report_sensor_data[LIGHT_CCT_SENSOR] = report_light_cct_data;
	data->report_sensor_data[ACCEL_UNCALIB_SENSOR] = report_uncalib_accel_data;
#endif
	data->ssp_big_task[BIG_TYPE_DUMP] = ssp_dump_task;
	data->ssp_big_task[BIG_TYPE_READ_LIB] = ssp_read_big_library_task;
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING // HIFI batch
	data->ssp_big_task[BIG_TYPE_READ_HIFI_BATCH] = ssp_batch_data_read_task;
#endif
	data->ssp_big_task[BIG_TYPE_VOICE_NET] = ssp_send_big_library_task;
	data->ssp_big_task[BIG_TYPE_VOICE_GRAM] = ssp_send_big_library_task;
	data->ssp_big_task[BIG_TYPE_VOICE_PCM] = ssp_pcm_dump_task;
	data->ssp_big_task[BIG_TYPE_TEMP] = ssp_temp_task;
}
