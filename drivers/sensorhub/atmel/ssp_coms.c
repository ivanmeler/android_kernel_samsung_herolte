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

#define LIMIT_DELAY_CNT		200
#define RECEIVEBUFFERSIZE	12
#define DEBUG_SHOW_DATA	0

static void clean_msg(struct ssp_msg *msg) {
	if (msg->free_buffer)
		kfree(msg->buffer);
	kfree(msg);
}


static int do_transfer(struct ssp_data *data, struct ssp_msg *msg,
		struct completion *done, int timeout) {
	int status = 0;
	int iDelaycnt = 0;
	bool msg_dead = false;
	bool use_no_irq = msg->length == 0;

	msg->dead_hook = &msg_dead;
	msg->dead = false;
	msg->done = done;

	mutex_lock(&data->comm_mutex);

	gpio_set_value_cansleep(data->ap_int, 0);
	while (gpio_get_value_cansleep(data->mcu_int2)) {
		mdelay(3);
		if (iDelaycnt++ > 500) { // snamy.jeong_0702 300 -> 500 : flash write timming apply.
			pr_err("[SSP]: %s exit1 - Time out!!\n", __func__);
			gpio_set_value_cansleep(data->ap_int, 1);
			status = -1;
			goto exit;
		}
	}

	status = spi_write(data->spi, msg, 8) >= 0;
#if 0
	if (status == 0) {
		pr_err("[SSP]: %s spi_write fail!!\n", __func__);
		gpio_set_value_cansleep(data->ap_int, 1);
		status = -1;
		goto exit;
	}
#endif	
	if (status <= 0) {
		pr_err("[SSP]: %s spi_write fail!!\n", __func__);
		gpio_set_value_cansleep(data->ap_int, 1);
		status = -1;
		goto exit;
	}

	if (!use_no_irq) {
		mutex_lock(&data->pending_mutex);
		list_add_tail(&msg->list, &data->pending_list);
		mutex_unlock(&data->pending_mutex);
	}

	iDelaycnt = 0;
	gpio_set_value_cansleep(data->ap_int, 1);
	while (!gpio_get_value_cansleep(data->mcu_int2)) {
		mdelay(3);
		if (iDelaycnt++ > 500) { // snamy.jeong_0702 300 -> 500 : flash write timming apply.
			pr_err("[SSP]: %s exit2 - Time out!!\n", __func__);
			status = -2;
			goto exit;
		}
	}

exit:
	mutex_unlock(&data->comm_mutex);

	if (status == -1) {
		pr_err("[SSP]: spi_write failed in %s\n", __func__);
		clean_msg(msg);
		return status;
	}

	if (status == 1 && done != NULL)
		if (wait_for_completion_timeout(done, msecs_to_jiffies(timeout)) == 0)
		{
			pr_err("[SSP]: Timeout waiting for pin interrupt in %s\n", __func__);                                     // 20130925
			status = -2;
		}

	mutex_lock(&data->pending_mutex);
	if (!msg_dead) {
		msg->done = NULL;
		msg->dead_hook = NULL;

		if (status != 1)
			msg->dead = true;
		if (status == -2)
			data->uTimeOutCnt++;
	}
	mutex_unlock(&data->pending_mutex);

	if (use_no_irq)
		clean_msg(msg);

	return status;
}

int ssp_spi_async(struct ssp_data *data, struct ssp_msg *msg) {
	int status = 0;

	status = do_transfer(data, msg, NULL, 0);

	return status;
}

int ssp_spi_sync(struct ssp_data *data, struct ssp_msg *msg, int timeout) {
	DECLARE_COMPLETION_ONSTACK(done);
	int status = 0;

	if (msg->length == 0) {
		pr_err("[SSP]: %s length must not be 0\n", __func__);
		clean_msg(msg);
		return status;
	}

	status = do_transfer(data, msg, &done, timeout);

	return status;
}

int select_irq_msg(struct ssp_data *data) {
	struct ssp_msg *msg, *n;
	bool found = false;
	u16 chLength = 0;
	u8 msg_type = 0;
	int iRet = 0;
	char* buffer;

	char chTempBuf[3] = { -1 };
//	pr_err("[SSP]: %s received interrupt from MCU\n", __func__);                                                                 // 20130925
	iRet = spi_read(data->spi, chTempBuf, sizeof(chTempBuf));
	if (iRet < 0) {
		pr_err("[SSP]: %s spi_read fail!!\n", __func__);
		return ERROR;
	}

	msg_type = chTempBuf[0] & SSP_SPI_MASK;
	pr_err("[SSP] chTempBuf[1] =  %d\n", chTempBuf[1]);                                                                                                      //20131001
	memcpy(&chLength, &chTempBuf[1], 2);

	switch (msg_type) {
	case AP2HUB_READ:
	case AP2HUB_WRITE:
		mutex_lock(&data->pending_mutex);
		if (!list_empty(&data->pending_list)) {
			list_for_each_entry_safe(msg, n, &data->pending_list, list)
			{
				if ((msg->options & SSP_SPI_MASK)== msg_type) {
					list_del(&msg->list);
					found = true;
					break;
				}
			}

			if (!found) {
				pr_err("[SSP]: %s - Not match error\n", __func__);
				mutex_unlock(&data->pending_mutex);
				break;
			}

			if (msg->dead && !msg->free_buffer) {
				msg->buffer = (char*) kzalloc(msg->length, GFP_KERNEL);
				msg->free_buffer = 1;
			} // For dead msg, make a temporary buffer to read.

			if (msg_type == AP2HUB_READ)
			{
				pr_err("[SSP] AP2HUB_READ Length : %d\n", msg->length);                                       //20130930
				iRet = spi_read(data->spi, msg->buffer, msg->length);
			}
			if (msg_type == AP2HUB_WRITE)
				iRet = spi_write(data->spi, msg->buffer, msg->length);

			if (msg->done != NULL && !completion_done(msg->done))
				complete(msg->done);
			else
				pr_err("[SSP] Failed to set done: %s\n", __func__);                                                                       //20130930
			if (msg->dead_hook != NULL)
				*(msg->dead_hook) = true;

			clean_msg(msg);
		} else
			pr_err("[SSP]List empty error(%d)\n", msg_type);
		mutex_unlock(&data->pending_mutex);
		break;
	case HUB2AP_WRITE:
		buffer = (char*) kzalloc(chLength, GFP_KERNEL);
		iRet = spi_read(data->spi, buffer, chLength);
		parse_dataframe(data, buffer, chLength);
		kfree(buffer);
		break;
	default:
		pr_err("[SSP]No type error(%d)\n", msg_type);
		break;
	}

	if (iRet < 0) {
		pr_err("[SSP]: %s - MSG2SSP_SSD error %d\n", __func__, iRet);
		return ERROR;
	}

	return SUCCESS;
}

void clean_pending_list(struct ssp_data *data) {
	struct ssp_msg *msg, *n;
	pr_err("[SSP] function = %s\n", __func__);                                                                       //20131001
	mutex_lock(&data->pending_mutex);
	list_for_each_entry_safe(msg, n, &data->pending_list, list)
	{
		list_del(&msg->list);
		if (msg->done != NULL && !completion_done(msg->done))
			complete(msg->done);
		if (msg->dead_hook != NULL)
			*(msg->dead_hook) = true;

		clean_msg(msg);
	}
	mutex_unlock(&data->pending_mutex);
}

int ssp_send_cmd(struct ssp_data *data, char command, int arg)
{
	int iRet = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = command;
	msg->length = 0;
	msg->options = AP2HUB_WRITE;
	msg->data = arg;
	msg->free_buffer = 0;

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - command 0x%x failed %d\n",
				__func__, command, iRet);
		return ERROR;
	}

	data->uInstFailCnt = 0;
	ssp_dbg("[SSP]: %s - command 0x%x\n", __func__, command);

	return SUCCESS;
}

int send_instruction(struct ssp_data *data, u8 uInst,
	u8 uSensorType, u8 *uSendBuf, u8 uLength)
{
	char command;
	int iRet = 0;
	struct ssp_msg *msg;

	if (data->fw_dl_state == FW_DL_STATE_DOWNLOADING) {
		pr_err("[SSP] %s - Skip Inst! DL state = %d\n",
			__func__, data->fw_dl_state);
		return SUCCESS;
	} else if ((!(data->uSensorState & (1 << uSensorType)))
		&& (uInst <= CHANGE_DELAY)) {
		pr_err("[SSP]: %s - Bypass Inst Skip! - %u\n",
			__func__, uSensorType);
		return FAIL;
	}

	switch (uInst) {
	case REMOVE_SENSOR:
		command = MSG2SSP_INST_BYPASS_SENSOR_REMOVE;
		break;
	case ADD_SENSOR:
		command = MSG2SSP_INST_BYPASS_SENSOR_ADD;
		break;
	case CHANGE_DELAY:
		command = MSG2SSP_INST_CHANGE_DELAY;
		break;
	case GO_SLEEP:
		command = MSG2SSP_AP_STATUS_SLEEP;
		break;
	case REMOVE_LIBRARY:
		command = MSG2SSP_INST_LIBRARY_REMOVE;
		break;
	case ADD_LIBRARY:
		command = MSG2SSP_INST_LIBRARY_ADD;
		break;
	default:
		command = uInst;
		break;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = command;
	msg->length = uLength + 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char*) kzalloc(uLength + 1, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = uSensorType;
	memcpy(&msg->buffer[1], uSendBuf, uLength);

	ssp_dbg("[SSP]: %s - Inst = 0x%x, Sensor Type = 0x%x, data = %u\n",
			__func__, command, uSensorType, msg->buffer[1]);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - Instruction CMD Fail %d\n", __func__, iRet);
		return ERROR;
	}

	data->uInstFailCnt = 0;

	return SUCCESS;
}

int get_chipid(struct ssp_data *data)
{
	int iRet;
	char buffer = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_WHOAMI;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = &buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	pr_info("[SSP]: %s returned %d\n", __func__, buffer);

	if (iRet == SUCCESS)
		return buffer;
	else {
		pr_info("[SSP]: ssp_spi_sync() returned %d\n", iRet);
		return ERROR;
	}
}

int set_sensor_position(struct ssp_data *data)
{
	int iRet = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SENSOR_FORMATION;
	msg->length = 3;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char*) kzalloc(3, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = data->accel_position;
	msg->buffer[1] = data->accel_position;
	msg->buffer[2] = data->mag_position;

	iRet = ssp_spi_async(data, msg);

	pr_info("[SSP] Sensor Posision A : %u, G : %u, M: %u, P: %u\n",
			data->accel_position, data->accel_position, data->mag_position, 0);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - spi fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

void set_proximity_threshold(struct ssp_data *data,
	unsigned int uData1, unsigned int uData2)
{
	int iRet = 0;

	struct ssp_msg *msg;

	if (!(data->uSensorState & 0x20)) {
		pr_info("[SSP]: %s - Skip this function!!!"\
			", proximity sensor is not connected(0x%x)\n",
			__func__, data->uSensorState);
		return;
	}

	msg= kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SENSOR_PROXTHRESHOLD;
	msg->length = 4;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char*) kzalloc(4, GFP_KERNEL);
	msg->free_buffer = 1;

	pr_err("[SSP]: %s - SENSOR_PROXTHRESHOL",__func__);

	msg->buffer[0] = ((char) (uData1 >> 8) & 0x07);
	msg->buffer[1] = (char) uData1;
	msg->buffer[2] = ((char) (uData2 >> 8) & 0x07);
	msg->buffer[3] = (char) uData2;

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - SENSOR_PROXTHRESHOLD CMD fail %d\n",
			__func__, iRet);
		return;
	}

	data->uInstFailCnt = 0;
	pr_info("[SSP]: Proximity Threshold - %u, %u\n", uData1, uData2);
}

void set_proximity_barcode_enable(struct ssp_data *data, bool bEnable)
{
	int iRet = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SENSOR_BARCODE_EMUL;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char*) kzalloc(1, GFP_KERNEL);
	msg->free_buffer = 1;

	data->bBarcodeEnabled = bEnable;
	msg->buffer[0] = bEnable;

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - SENSOR_BARCODE_EMUL CMD fail %d\n",
				__func__, iRet);
		return;
	}

	data->uInstFailCnt = 0;
	pr_info("[SSP] Proximity Barcode En : %u\n", bEnable);
}

void set_gesture_current(struct ssp_data *data, unsigned char uData1)
{
	int iRet = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SENSOR_GESTURE_CURRENT;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char*) kzalloc(1, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = uData1;
	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - SENSOR_GESTURE_CURRENT CMD fail %d\n", __func__,
				iRet);
		return;
	}

	data->uInstFailCnt = 0;
	pr_info("[SSP]: Gesture Current Setting - %u\n", uData1);
}

unsigned int get_sensor_scanning_info(struct ssp_data *data)
{
	int iRet = 0;
	char buffer[2] = { 0, };

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SENSOR_SCANNING;
	msg->length = 2;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - spi failed %d\n", __func__, iRet);
		return 0;
	}
	pr_err("[SSP]: %s - %d %d\n", __func__, buffer[0] ,buffer[1]);
	return ((unsigned int)buffer[0] << 8) | buffer[1];
}

unsigned int get_firmware_rev(struct ssp_data *data)
{
	unsigned int uRev = 99999;
	int iRet;
	char buffer[3] = { 0, };

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_FIRMWARE_REV;
	msg->length = 3;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (iRet != SUCCESS)
		pr_err("[SSP]: %s - spi fail %d\n", __func__, iRet);
	else
		uRev = ((unsigned int)buffer[0] << 16)
			| ((unsigned int)buffer[1] << 8) | buffer[2];
	return uRev;
}

int get_fuserom_data(struct ssp_data *data)
{
	int iRet = 0;
	char buffer[3] = { 0, };

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_FUSEROM;
	msg->length = 3;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (iRet) {
		data->uFuseRomData[0] = buffer[0];
		data->uFuseRomData[1] = buffer[1];
		data->uFuseRomData[2] = buffer[2];
	} else {
		data->uFuseRomData[0] = 0;
		data->uFuseRomData[1] = 0;
		data->uFuseRomData[2] = 0;
		return FAIL;
	}

	pr_info("[SSP] FUSE ROM Data %d , %d, %d\n", data->uFuseRomData[0],
			data->uFuseRomData[1], data->uFuseRomData[2]);

	return SUCCESS;
}

int set_big_data_start(struct ssp_data *data, u8 type, u32 length) {
	int iRet = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_START_BIG_DATA;
	msg->length = 5;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char*) kzalloc(5, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = type;
	memcpy(&msg->buffer[1], &length, 4);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - spi fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}
void sanity_check(struct ssp_data *data) {
	int iRet;
	char buffer = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_MCU_SANITY_CHECK;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = &buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	if (iRet == SUCCESS && buffer != MCU_IS_SANE)
		if (initialize_mcu(data) > 0) {
			sync_sensor_state(data);
			ssp_sensorhub_report_notice(data, MSG2SSP_AP_STATUS_RESET);
			pr_err("[SSP]: %s %d\n", __func__, iRet);
			if( data->uLastAPState!=0 ) ssp_send_cmd(data, data->uLastAPState, 0);
			if( data->uLastResumeState != 0) ssp_send_cmd(data, data->uLastResumeState, 0);
		}
}
