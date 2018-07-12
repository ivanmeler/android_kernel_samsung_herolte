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

#ifndef __SSP_FIRMWARE_H__
#define __SSP_FIRMWARE_H__

#include "ssp.h"

#if defined (CONFIG_SENSORS_SSP_NOBLELTE) \
	|| defined (CONFIG_SENSORS_SSP_ZENLTE) /* Noble or Zen */
#define SSP_FIRMWARE_REVISION_STM	15060401
#elif defined (CONFIG_SENSORS_SSP_VLTE) /* V */
#define SSP_FIRMWARE_REVISION_STM	15052800
#else /* Zero */
#define SSP_FIRMWARE_REVISION_STM	15050700
#endif

#define SSP_INVALID_REVISION            99999
#define SSP_INVALID_REVISION2           0xFFFFFF

#define BOOT_SPI_HZ			4800000
#define NORM_SPI_HZ			4800000

/* Bootload mode cmd */
#if defined (CONFIG_SENSORS_SSP_NOBLELTE) \
	|| defined (CONFIG_SENSORS_SSP_ZENLTE) /* Noble or Zen */
#define BL_FW_NAME			"ssp_stm_noble.fw"
#elif defined (CONFIG_SENSORS_SSP_VLTE) /* V */
#define BL_FW_NAME			"ssp_stm_v.fw"
#else /* Zero */
#define BL_FW_NAME			"ssp_stm.fw"
#endif

#define BL_UMS_FW_NAME			"ssp_stm.bin"
#define BL_CRASHED_FW_NAME		"ssp_crashed.fw"

#define BL_UMS_FW_PATH			255

#define APP_SLAVE_ADDR			0x18
#define BOOTLOADER_SLAVE_ADDR		0x26

/* Bootloader mode status */
#define BL_WAITING_BOOTLOAD_CMD		0xc0	/* valid 7 6 bit only */
#define BL_WAITING_FRAME_DATA		0x80	/* valid 7 6 bit only */
#define BL_FRAME_CRC_CHECK		0x02
#define BL_FRAME_CRC_FAIL		0x03
#define BL_FRAME_CRC_PASS		0x04
#define BL_APP_CRC_FAIL			0x40	/* valid 7 8 bit only */
#define BL_BOOT_STATUS_MASK		0x3f

/* Command to unlock bootloader */
#define BL_UNLOCK_CMD_MSB		0xaa
#define BL_UNLOCK_CMD_LSB		0xdc

/* STM */
#define SSP_STM_DEBUG			0

#define SEND_ADDR_LEN			5
#define BL_SPI_SOF			0x5A
#define BL_ACK				0x79
#define BL_ACK2				0xF9
#define BL_NACK				0x1F
#define BL_IDLE				0xA5
#define BL_DUMMY			0x00

#define STM_MAX_XFER_SIZE		256
#define STM_MAX_BUFFER_SIZE		260
#define STM_APP_ADDR			0x08000000

#define BYTE_DELAY_READ			10
#define BYTE_DELAY_WRITE		8

#define DEF_ACK_ERASE_NUMBER		14000 /* Erase time ack wait increase : Flash size adjust */
#define DEF_ACKCMD_NUMBER		40
#define DEF_ACKROOF_NUMBER		40

#define WMEM_COMMAND			0x31  /* Write Memory command */
#define GO_COMMAND			0x21  /* GO command */
#define EXT_ER_COMMAND			0x44  /* Erase Memory command */

#define XOR_RMEM_COMMAND		0xEE  /* Read Memory command */

#define XOR_WMEM_COMMAND		0xCE  /* Write Memory command */
#define XOR_GO_COMMAND			0xDE  /* GO command */
#define XOR_EXT_ER_COMMAND		0xBB  /* Erase Memory command */

#define EXT_ER_DATA_LEN			3
#define BLMODE_RETRYCOUNT		3
#define BYTETOBYTE_USED			0

unsigned int get_module_rev(struct ssp_data *data);
void toggle_mcu_reset(struct ssp_data *data);
int forced_to_download_binary(struct ssp_data *data, int iBinType);
int check_fwbl(struct ssp_data *data);

#endif
