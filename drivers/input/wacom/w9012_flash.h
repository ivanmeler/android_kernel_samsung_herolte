/*
 *  w9012_flash.h - Wacom Digitizer Controller Flash Driver
 *
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _WACOM_I2C_FLASH_H
#define _WACOM_I2C_FLASH_H

#define FLASH_START0	'f'
#define FLASH_START1	'l'
#define FLASH_START2	'a'
#define FLASH_START3	's'
#define FLASH_START4	'h'
#define FLASH_START5	'\r'
#define FLASH_ACK		0x06

/*Address for boot is 0x09 by default, but it may be different in other models*/
#define WACOM_FLASH_W9012		0x09
#define WACOM_QUERY_SIZE		19
#define pen_QUERY				'*'
#define ACK						0

#define MPU_W9012				0x2d

#define FLASH_BLOCK_SIZE		64
#define DATA_SIZE				(65536 * 2)
#define BLOCK_NUM				127
#define W9012_START_ADDR		0x2000
#define W9012_END_ADDR			0x1ffff

#define CMD_GET_FEATURE	2
#define CMD_SET_FEATURE	3

#define BOOT_CMD_SIZE			78
#define BOOT_RSP_SIZE			6
#define BOOT_CMD_REPORT_ID		7
#define BOOT_ERASE_DATAMEM		0x0e
#define BOOT_ERASE_FLASH		0
#define BOOT_WRITE_FLASH		1
#define BOOT_EXIT				3
#define BOOT_BLVER				4
#define BOOT_MPU				5
#define BOOT_QUERY				7

#define QUERY_CMD				0x07
#define BOOT_CMD				0x04
#define MPU_CMD					0x05
#define ERS_CMD					0x00

#define QUERY_RSP				0x06
#define ERS_RSP					0x00
#define WRITE_RSP				0x00

#define CMD_SIZE				(72+6)
#define RSP_SIZE				6

/*Sector Nos for erasing datamem*/
#define WRITE_CMD				0x01
#define DATAMEM_SECTOR0			0
#define DATAMEM_SECTOR1			1
#define DATAMEM_SECTOR2			2
#define DATAMEM_SECTOR3			3
#define DATAMEM_SECTOR4			4
#define DATAMEM_SECTOR5			5
#define DATAMEM_SECTOR6			6
#define DATAMEM_SECTOR7			7

//
// exit codes
//
#define EXIT_OK							(0)
#define EXIT_REBOOT						(1)
#define EXIT_FAIL						(2)
#define EXIT_USAGE						(3)
#define EXIT_NO_SUCH_FILE				(4)
#define EXIT_NO_INTEL_HEX				(5)
#define EXIT_FAIL_OPEN_COM_PORT			(6)
#define EXIT_FAIL_ENTER_FLASH_MODE		(7)
#define EXIT_FAIL_FLASH_QUERY			(8)
#define EXIT_FAIL_BAUDRATE_CHANGE		(9)
#define EXIT_FAIL_WRITE_FIRMWARE		(10)
#define EXIT_FAIL_EXIT_FLASH_MODE		(11)
#define EXIT_CANCEL_UPDATE				(12)
#define EXIT_SUCCESS_UPDATE				(13)
#define EXIT_FAIL_HID2SERIAL			(14)
#define EXIT_FAIL_VERIFY_FIRMWARE		(15)
#define EXIT_FAIL_MAKE_WRITING_MARK		(16)
#define EXIT_FAIL_ERASE_WRITING_MARK	(17)
#define EXIT_FAIL_READ_WRITING_MARK		(18)
#define EXIT_EXIST_MARKING				(19)
#define EXIT_FAIL_MISMATCHING			(20)
#define EXIT_FAIL_ERASE					(21)
#define EXIT_FAIL_GET_BOOT_LOADER_VERSION	(22)
#define EXIT_FAIL_GET_MPU_TYPE			(23)
#define EXIT_MISMATCH_BOOTLOADER		(24)
#define EXIT_MISMATCH_MPUTYPE			(25)
#define EXIT_FAIL_ERASE_BOOT			(26)
#define EXIT_FAIL_WRITE_BOOTLOADER		(27)
#define EXIT_FAIL_SWAP_BOOT				(28)
#define EXIT_FAIL_WRITE_DATA			(29)
#define EXIT_FAIL_GET_FIRMWARE_VERSION	(30)
#define EXIT_FAIL_GET_UNIT_ID			(31)
#define EXIT_FAIL_SEND_STOP_COMMAND		(32)
#define EXIT_FAIL_SEND_QUERY_COMMAND	(33)
#define EXIT_NOT_FILE_FOR_535			(34)
#define EXIT_NOT_FILE_FOR_514			(35)
#define EXIT_NOT_FILE_FOR_503			(36)
#define EXIT_MISMATCH_MPU_TYPE			(37)
#define EXIT_NOT_FILE_FOR_515			(38)
#define EXIT_NOT_FILE_FOR_1024			(39)
#define EXIT_FAIL_VERIFY_WRITING_MARK	(40)
#define EXIT_DEVICE_NOT_FOUND			(41)
#define EXIT_FAIL_WRITING_MARK_NOT_SET	(42)
#define EXIT_FAIL_SET_PDCT				(43)
#define ERR_SET_PDCT					(44)
#define ERR_GET_PDCT					(45)

#endif /*_WACOM_I2C_FLASH_H*/
