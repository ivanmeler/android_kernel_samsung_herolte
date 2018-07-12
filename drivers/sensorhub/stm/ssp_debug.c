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

#include "ssp_debug.h"

static mm_segment_t backup_fs;

int debug_crash_dump(struct ssp_data *data, char *pchRcvDataFrame, int iLength)
{
	struct timeval cur_time;
	char strFilePath[100];
	int iRetWrite = 0, iRet = 0;
	unsigned char datacount = pchRcvDataFrame[1];
	unsigned int databodysize = iLength - 2;
	char *databody = &pchRcvDataFrame[2];
/*
	if(iLength != DEBUG_DUMP_DATA_SIZE)
	{
		ssp_errf("data length error(%d)", iLength);
		return FAIL;
	}
	else
		ssp_errf("length(%d)", databodysize);
*/
	ssp_errf("length(%d)", databodysize);

	if (data->bMcuDumpMode == true)	{
		wake_lock(&data->ssp_wake_lock);

		backup_fs = get_fs();
		set_fs(get_ds());

		if (data->realtime_dump_file == NULL) {

			do_gettimeofday(&cur_time);

			snprintf(strFilePath, sizeof(strFilePath), "%s%d.dump",
				DEBUG_DUMP_FILE_PATH, (int)cur_time.tv_sec);
			data->realtime_dump_file = filp_open(strFilePath,
					O_RDWR | O_CREAT | O_APPEND, 0660);

			ssp_err("save_crash_dump : open file(%s)", strFilePath);

			if (IS_ERR(data->realtime_dump_file)) {
				ssp_errf("Can't open dump file");
				set_fs(backup_fs);
				iRet = PTR_ERR(data->realtime_dump_file);
				data->realtime_dump_file = NULL;
				wake_unlock(&data->ssp_wake_lock);
				return iRet;
			}
		}

		data->total_dump_size += databodysize;
		/* ssp_errf("total receive size(%d)", data->total_dump_size); */
		iRetWrite = vfs_write(data->realtime_dump_file,
					(char __user *)databody, databodysize,
					&data->realtime_dump_file->f_pos);
		if (iRetWrite < 0) {
			ssp_errf("Can't write dump to file");
			filp_close(data->realtime_dump_file, current->files);
			set_fs(backup_fs);
			wake_unlock(&data->ssp_wake_lock);
			return FAIL;
		}

		if (datacount == DEBUG_DUMP_DATA_COMPLETE) {
			ssp_errf("close file(size=%d)", data->total_dump_size);
			data->uDumpCnt++;
			data->total_dump_size = 0;
			data->realtime_dump_file = NULL;
			data->bDumping = false;
		}

		filp_close(data->realtime_dump_file, current->files);
		set_fs(backup_fs);

		wake_unlock(&data->ssp_wake_lock);

		/*
		if(iLength == 2*1024)
			queue_refresh_task(data, 0);
		*/
	}

	return SUCCESS;
}

void ssp_dump_task(struct work_struct *work)
{
#if CONFIG_SEC_DEBUG
	struct ssp_big *big;
	struct file *dump_file;
	struct ssp_msg *msg;
	char *buffer;
	char strFilePath[100];
	struct timeval cur_time;
	mm_segment_t fs;
	int buf_len, packet_len, residue;
	int iRet = 0, index = 0, iRetTrans = 0, iRetWrite = 0;

	big = container_of(work, struct ssp_big, work);
	ssp_errf("start ssp dumping (%d)(%d)",
		big->data->bMcuDumpMode, big->data->uDumpCnt);

	big->data->uDumpCnt++;
	wake_lock(&big->data->ssp_wake_lock);

	fs = get_fs();
	set_fs(get_ds());

	if (big->data->bMcuDumpMode == true) {
		do_gettimeofday(&cur_time);
#ifdef CONFIG_SENSORS_SSP_ENG
		snprintf(strFilePath, sizeof(strFilePath), "%s%d.dump",
			DUMP_FILE_PATH,	(int)cur_time.tv_sec);
		dump_file = filp_open(strFilePath,
				O_RDWR | O_CREAT | O_APPEND, 0666);
#else
		snprintf(strFilePath, sizeof(strFilePath), "%s.dump",
			DUMP_FILE_PATH);
		dump_file = filp_open(strFilePath,
				O_RDWR | O_CREAT | O_TRUNC, 0666);
#endif

		if (IS_ERR(dump_file)) {
			ssp_errf("Can't open dump file");
			set_fs(fs);
			iRet = PTR_ERR(dump_file);
			wake_unlock(&big->data->ssp_wake_lock);
			kfree(big);
			return;
		}
	} else {
		dump_file = NULL;
	}

	buf_len = big->length > DATA_PACKET_SIZE
			? DATA_PACKET_SIZE : big->length;
	buffer = kzalloc(buf_len, GFP_KERNEL);
	residue = big->length;

	while (residue > 0) {
		packet_len = residue > DATA_PACKET_SIZE
				? DATA_PACKET_SIZE : residue;

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_GET_BIG_DATA;
		msg->length = packet_len;
		msg->options = AP2HUB_READ | (index++ << SSP_INDEX);
		msg->data = big->addr;
		msg->buffer = buffer;
		msg->free_buffer = 0;

		iRetTrans = ssp_spi_sync(big->data, msg, 1000);
		if (iRetTrans != SUCCESS) {
			ssp_errf("Fail to receive data %d (%d)",
				iRetTrans, residue);
			break;
		}

		if (big->data->bMcuDumpMode == true) {
			iRetWrite = vfs_write(dump_file, (char __user *)buffer,
						packet_len, &dump_file->f_pos);
			if (iRetWrite < 0) {
				ssp_errf("Can't write dump to file");
				break;
			}
		}
		residue -= packet_len;
	}

	if (big->data->bMcuDumpMode == true) {
		if (iRetTrans != SUCCESS || iRetWrite < 0) { /* error case */
			char FAILSTRING[100];
			snprintf(FAILSTRING, sizeof(FAILSTRING),
				"FAIL OCCURED(%d)(%d)(%d)", iRetTrans,
				iRetWrite, big->length);
			vfs_write(dump_file, (char __user *)FAILSTRING,
					strlen(FAILSTRING), &dump_file->f_pos);
		}

		filp_close(dump_file, current->files);
	}

	big->data->bDumping = false;

	set_fs(fs);

	wake_unlock(&big->data->ssp_wake_lock);
	kfree(buffer);
	kfree(big);
#endif
	ssp_errf("done");
}

/*************************************************************************/
/* SSP Debug timer function                                              */
/*************************************************************************/

int print_mcu_debug(char *pchRcvDataFrame, int *pDataIdx,
		int iRcvDataFrameLength)
{
	u16 length = 0;
	int cur = *pDataIdx;

	memcpy(&length, pchRcvDataFrame + *pDataIdx, 1);
	*pDataIdx += 1;

	if (length > iRcvDataFrameLength - *pDataIdx || length <= 0) {
		ssp_infof("[M] invalid debug length(%u/%d/%d)",
			length, iRcvDataFrameLength, cur);
		return length ? length : ERROR;
	}

	ssp_info("[M] %s", &pchRcvDataFrame[*pDataIdx]);
	*pDataIdx += length;
	return 0;
}

void reset_mcu(struct ssp_data *data)
{
	ssp_infof();
	ssp_enable(data, false);
	clean_pending_list(data);
	toggle_mcu_reset(data);
	ssp_enable(data, true);
}

void sync_sensor_state(struct ssp_data *data)
{
	unsigned char uBuf[9] = {0,};
	unsigned int uSensorCnt;
	int iRet = 0;

	iRet = set_gyro_cal(data);
	if (iRet < 0)
		ssp_errf("set_gyro_cal failed");

	iRet = set_accel_cal(data);
	if (iRet < 0)
		ssp_errf("set_accel_cal failed");

	udelay(10);

	for (uSensorCnt = 0; uSensorCnt < SENSOR_MAX; uSensorCnt++) {
		if (atomic_read(&data->aSensorEnable) & (1 << uSensorCnt)) {
			s32 dMsDelay
				= get_msdelay(data->adDelayBuf[uSensorCnt]);
			memcpy(&uBuf[0], &dMsDelay, 4);
			memcpy(&uBuf[4], &data->batchLatencyBuf[uSensorCnt], 4);
			uBuf[8] = data->batchOptBuf[uSensorCnt];
			send_instruction(data, ADD_SENSOR, uSensorCnt, uBuf, 9);
			udelay(10);
		}
	}

	if (data->bProximityRawEnabled == true) {
		s32 dMsDelay = 20;
		memcpy(&uBuf[0], &dMsDelay, 4);
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, uBuf, 4);
	}

	set_proximity_threshold(data, data->uProxHiThresh, data->uProxLoThresh);
	data->buf[PROXIMITY_SENSOR].prox = 0;
	report_sensordata(data, PROXIMITY_SENSOR, &data->buf[PROXIMITY_SENSOR]);

#if CONFIG_SEC_DEBUG
	data->bMcuDumpMode = sec_debug_is_enabled();
	iRet = ssp_send_cmd(data, MSG2SSP_AP_MCU_SET_DUMPMODE,
			data->bMcuDumpMode);
	if (iRet < 0)
		ssp_errf("MSG2SSP_AP_MCU_SET_DUMPMODE failed");
#endif
}

static void print_sensordata(struct ssp_data *data, unsigned int uSensor)
{
	switch (uSensor) {
	case ACCELEROMETER_SENSOR:
	case GYROSCOPE_SENSOR:
		ssp_info("%u : %d, %d, %d (%ums, %dms)", uSensor,
			data->buf[uSensor].x, data->buf[uSensor].y,
			data->buf[uSensor].z,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GEOMAGNETIC_SENSOR:
		ssp_info("%u : %d, %d, %d, %d (%ums)", uSensor,
			data->buf[uSensor].cal_x, data->buf[uSensor].cal_y,
			data->buf[uSensor].cal_y, data->buf[uSensor].accuracy,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GEOMAGNETIC_UNCALIB_SENSOR:
		ssp_info("%u : %d, %d, %d, %d, %d, %d (%ums)", uSensor,
			data->buf[uSensor].uncal_x, data->buf[uSensor].uncal_y,
			data->buf[uSensor].uncal_z, data->buf[uSensor].offset_x,
			data->buf[uSensor].offset_y, data->buf[uSensor].offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case PRESSURE_SENSOR:
		ssp_info("%u : %d, %d (%ums, %dms)", uSensor,
			data->buf[uSensor].pressure,
			data->buf[uSensor].temperature,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GESTURE_SENSOR:
		ssp_info("%u : %d, %d, %d, %d (%ums)", uSensor,
			data->buf[uSensor].data[3], data->buf[uSensor].data[4],
			data->buf[uSensor].data[5], data->buf[uSensor].data[6],
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case LIGHT_SENSOR:
		ssp_info("%u : %u, %u, %u, %u, %u, %u (%ums)", uSensor,
			data->buf[uSensor].r, data->buf[uSensor].g,
			data->buf[uSensor].b, data->buf[uSensor].w,
			data->buf[uSensor].a_time, data->buf[uSensor].a_gain,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case PROXIMITY_SENSOR:
		ssp_info("%u : %d, %d (%ums)", uSensor,
			data->buf[uSensor].prox, data->buf[uSensor].prox_ex,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case STEP_DETECTOR:
		ssp_info("%u : %u (%ums, %dms)", uSensor,
			data->buf[uSensor].step_det,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GAME_ROTATION_VECTOR:
	case ROTATION_VECTOR:
		ssp_info("%u : %d, %d, %d, %d, %d (%ums, %dms)", uSensor,
			data->buf[uSensor].quat_a, data->buf[uSensor].quat_b,
			data->buf[uSensor].quat_c, data->buf[uSensor].quat_d,
			data->buf[uSensor].acc_rot,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case SIG_MOTION_SENSOR:
		ssp_info("%u : %u(%ums)", uSensor,
			data->buf[uSensor].sig_motion,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GYRO_UNCALIB_SENSOR:
		ssp_info("%u : %d, %d, %d, %d, %d, %d (%ums)", uSensor,
			data->buf[uSensor].uncal_x, data->buf[uSensor].uncal_y,
			data->buf[uSensor].uncal_z, data->buf[uSensor].offset_x,
			data->buf[uSensor].offset_y,
			data->buf[uSensor].offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case STEP_COUNTER:
		ssp_info("%u : %u(%ums)", uSensor,
			data->buf[uSensor].step_diff,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case LIGHT_IR_SENSOR:
		ssp_info("%u : %u, %u, %u, %u, %u, %u, %u(%ums)", uSensor,
			data->buf[uSensor].irdata,
			data->buf[uSensor].ir_r, data->buf[uSensor].ir_g,
			data->buf[uSensor].ir_b, data->buf[uSensor].ir_w,
			data->buf[uSensor].ir_a_time,
			data->buf[uSensor].ir_a_gain,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case INTERRUPT_GYRO_SENSOR:
		ssp_info("%u : %d, %d, %d (%ums)", uSensor,
			data->buf[uSensor].x, data->buf[uSensor].y,
			data->buf[uSensor].z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	default:
		ssp_info("Wrong sensorCnt: %u", uSensor);
		break;
	}
}

static void recovery_mcu(struct ssp_data *data)
{
	if (data->uComFailCnt < LIMIT_RESET_CNT) {
		ssp_infof("- uTimeOutCnt(%u), pending(%u)",
			data->uTimeOutCnt, !list_empty(&data->pending_list));
		data->uComFailCnt++;
		reset_mcu(data);
	} else {
		ssp_enable(data, false);
	}

	data->uTimeOutCnt = 0;
}

static void debug_work_func(struct work_struct *work)
{
	unsigned int uSensorCnt;
	struct ssp_data *data = container_of(work, struct ssp_data, work_debug);

	ssp_infof("(%u) - Sensor state: 0x%x, RC: %u, CC: %u DC: %u"
		" TC: %u", data->uIrqCnt, data->uSensorState,
		data->uResetCnt, data->uComFailCnt, data->uDumpCnt,
		data->uTimeOutCnt);

	switch (data->fw_dl_state) {
	case FW_DL_STATE_FAIL:
	case FW_DL_STATE_DOWNLOADING:
	case FW_DL_STATE_SYNC:
		ssp_infof("firmware downloading state = %d",
				data->fw_dl_state);
		return;
	}

	for (uSensorCnt = 0; uSensorCnt < SENSOR_MAX; uSensorCnt++)
		if ((atomic_read(&data->aSensorEnable) & (1 << uSensorCnt))
			|| data->batchLatencyBuf[uSensorCnt])
			print_sensordata(data, uSensorCnt);

	if (((atomic_read(&data->aSensorEnable) & (1 << ACCELEROMETER_SENSOR))
		&& (data->batchLatencyBuf[ACCELEROMETER_SENSOR] == 0)
		&& (data->uIrqCnt == 0) && (data->uTimeOutCnt > 0))
		|| (data->uTimeOutCnt > LIMIT_TIMEOUT_CNT))
		recovery_mcu(data);

	data->uIrqCnt = 0;
}

static void debug_timer_func(unsigned long ptr)
{
	struct ssp_data *data = (struct ssp_data *)ptr;

	queue_work(data->debug_wq, &data->work_debug);
	mod_timer(&data->debug_timer,
		round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void enable_debug_timer(struct ssp_data *data)
{
	mod_timer(&data->debug_timer,
		round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void disable_debug_timer(struct ssp_data *data)
{
	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
}

int initialize_debug_timer(struct ssp_data *data)
{
	setup_timer(&data->debug_timer, debug_timer_func, (unsigned long)data);

	data->debug_wq = create_singlethread_workqueue("ssp_debug_wq");
	if (!data->debug_wq)
		return ERROR;

	INIT_WORK(&data->work_debug, debug_work_func);
	return SUCCESS;
}
