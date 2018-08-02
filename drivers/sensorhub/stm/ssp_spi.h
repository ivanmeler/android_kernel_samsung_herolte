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

#ifndef __SSP_SPI_H__
#define __SSP_SPI_H__

#include "ssp.h"

/* ssp_msg options bit*/
#define SSP_SPI		0       /* read write mask */
#define SSP_RETURN	2       /* write and read option */
#define SSP_GYRO_DPS	3       /* gyro dps mask */
#define SSP_INDEX	3       /* data index mask */
#define SSP_SPI_MASK	(3 << SSP_SPI)  /* read write mask */


struct ssp_data;
struct ssp_msg;

int ssp_spi_async(struct ssp_data *data, struct ssp_msg *msg);
int ssp_spi_sync(struct ssp_data *data, struct ssp_msg *msg, int timeout);
int select_irq_msg(struct ssp_data *data);
void clean_pending_list(struct ssp_data *data);
int ssp_send_cmd(struct ssp_data *data, char command, int arg);
int send_instruction(struct ssp_data *data, u8 uInst,
	u8 uSensorType, u8 *uSendBuf, u16 uLength);
int send_instruction_sync(struct ssp_data *data, u8 uInst,
	u8 uSensorType, u8 *uSendBuf, u16 uLength);
int flush(struct ssp_data *data, u8 uSensorType);
int get_batch_count(struct ssp_data *data, u8 uSensorType);
int get_chipid(struct ssp_data *data);
int set_sensor_position(struct ssp_data *data);
void set_proximity_threshold(struct ssp_data *data,
	unsigned int uData1, unsigned int uData2);
#ifdef CONFIG_SENSORS_MULTIPLE_GLASS_TYPE
int set_glass_type(struct ssp_data *);
#endif
void set_proximity_barcode_enable(struct ssp_data *data, bool bEnable);
void set_gesture_current(struct ssp_data *data, unsigned char uData1);
unsigned int get_sensor_scanning_info(struct ssp_data *data);
unsigned int get_firmware_rev(struct ssp_data *data);
int set_big_data_start(struct ssp_data *data, u8 type, u32 length);
int set_time(struct ssp_data *data);
int get_time(struct ssp_data *data);

#endif
