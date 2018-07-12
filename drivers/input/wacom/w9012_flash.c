/*
 *  w9012_flash.c - Wacom Digitizer Controller Flash Driver
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
#include "wacom.h"
#include "wacom_i2c_func.h"
#include "wacom_i2c_firm.h"
#include "w9012_flash.h"

#if 0
int wacom_i2c_master_send(struct i2c_client *client, const char *buf, int count,
			  unsigned char addr)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	msg.addr = addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (char *)buf;

	ret = i2c_transfer(adap, &msg, 1);

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	return (ret == 1) ? count : ret;
}

int wacom_i2c_master_recv(struct i2c_client *client, const char *buf, int count,
			  unsigned char addr)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	msg.addr = addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = (char *)buf;

	ret = i2c_transfer(adap, &msg, 1);

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	return (ret == 1) ? count : ret;
}
#endif
static int wacom_flash_cmd(struct wacom_i2c *wac_i2c)
{
	u8 command[10];
	int len = 0;
	int ret;

	command[len++] = 0x0d;
	command[len++] = FLASH_START0;
	command[len++] = FLASH_START1;
	command[len++] = FLASH_START2;
	command[len++] = FLASH_START3;
	command[len++] = FLASH_START4;
	command[len++] = FLASH_START5;
	command[len++] = 0x0d;

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:Sending flash command failed\n");
		return -EXIT_FAIL;
	}

	msleep(300);

	return 0;
}

int flash_query(struct wacom_i2c *wac_i2c)
{
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ret, ECH;
	int len = 0;

	command[len++] = 4;	/* Command Register-LSB */
	command[len++] = 0;	/* Command Register-MSB */
	command[len++] = 0x37;	/* Command-LSB, ReportType:Feature(11) ReportID:7 */
	command[len++] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
	command[len++] = 5;	/* Data Register-LSB */
	command[len++] = 0;	/* Data-Register-MSB */
	command[len++] = 5;	/* Length Field-LSB */
	command[len++] = 0;	/* Length Field-MSB */
	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_QUERY;	/* Report:Boot Query command */
	command[len++] = ECH = 7;	/* Report:echo */

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 2 ret:%d \n", __func__, ret);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	len = 0;
	command[len++] = 4;	/* Command Register-LSB */
	command[len++] = 0;	/* Command Register-MSB */
	command[len++] = 0x38;	/* Command-LSB, ReportType:Feature(11) ReportID:8 */
	command[len++] = CMD_GET_FEATURE;	/* Command-MSB, GET_REPORT */
	command[len++] = 5;	/* Data Register-LSB */
	command[len++] = 0;	/* Data Register-MSB */

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 2 ret:%d \n", __func__, ret);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	msleep(10);

	ret = wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
			     WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 5 ret:%d \n", __func__, ret);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if ((response[3] != QUERY_CMD) || (response[4] != ECH)) {
		printk(KERN_DEBUG"epen:%s res3:%x res4:%x \n", __func__, response[3],
		       response[4]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}
	if (response[5] != QUERY_RSP) {
		printk(KERN_DEBUG"epen:%s res5:%x \n", __func__, response[5]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	return 0;
}

static bool flash_blver(struct wacom_i2c *wac_i2c, int *blver)
{
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ret, ECH;
	int len = 0;

	command[len++] = 4;	/* Command Register-LSB */
	command[len++] = 0;	/* Command Register-MSB */
	command[len++] = 0x37;	/* Command-LSB, ReportType:Feature(11) ReportID:7 */
	command[len++] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
	command[len++] = 5;	/* Data Register-LSB */
	command[len++] = 0;	/* Data-Register-MSB */
	command[len++] = 5;	/* Length Field-LSB */
	command[len++] = 0;	/* Length Field-MSB */
	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_BLVER;	/* Report:Boot Version command */
	command[len++] = ECH = 7;	/* Report:echo */

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 2 ret:%d \n", __func__, ret);
		return false;
	}

	len = 0;
	command[len++] = 4;	/* Command Register-LSB */
	command[len++] = 0;	/* Command Register-MSB */
	command[len++] = 0x38;	/* Command-LSB, ReportType:Feature(11) ReportID:8 */
	command[len++] = CMD_GET_FEATURE;	/* Command-MSB, GET_REPORT */
	command[len++] = 5;	/* Data Register-LSB */
	command[len++] = 0;	/* Data Register-MSB */

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 4 ret:%d \n", __func__, ret);
		return false;
	}

	msleep(10);

	ret =
	    wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
			   WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 5 ret:%d \n", __func__, ret);
		return false;
	}

	if ((response[3] != BOOT_CMD) || (response[4] != ECH)) {
		printk(KERN_DEBUG"epen:%s res[3]:%x res[4]:%x \n", __func__, response[3],
		       response[4]);
		return false;
	}

	*blver = (int)response[5];

	return true;
}

static bool flash_mputype(struct wacom_i2c *wac_i2c, int *pMpuType)
{
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ret, ECH;
	int len = 0;

	command[len++] = 4;	/* Command Register-LSB */
	command[len++] = 0;	/* Command Register-MSB */
	command[len++] = 0x37;	/* Command-LSB, ReportType:Feature(11) ReportID:7 */
	command[len++] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
	command[len++] = 5;	/* Data Register-LSB */
	command[len++] = 0;	/* Data-Register-MSB */
	command[len++] = 5;	/* Length Field-LSB */
	command[len++] = 0;	/* Length Field-MSB */
	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_MPU;	/* Report:Boot Query command */
	command[len++] = ECH = 7;	/* Report:echo */

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 2 ret:%d \n", __func__, ret);
		return false;
	}

	len = 0;
	command[len++] = 4;	/* Command Register-LSB */
	command[len++] = 0;	/* Command Register-MSB */
	command[len++] = 0x38;	/* Command-LSB, ReportType:Feature(11) ReportID:8 */
	command[len++] = CMD_GET_FEATURE;	/* Command-MSB, GET_REPORT */
	command[len++] = 5;	/* Data Register-LSB */
	command[len++] = 0;	/* Data Register-MSB */

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 4 ret:%d \n", __func__, ret);
		return false;
	}

	msleep(10);

	ret =
	    wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
			   WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 5 ret:%d \n", __func__, ret);
		return false;
	}

	if ((response[3] != MPU_CMD) || (response[4] != ECH)) {
		printk(KERN_DEBUG"epen:%s res[3]:%x res[4]:%x \n", __func__, response[3],
		       response[4]);
		return false;
	}

	*pMpuType = (int)response[5];
	return true;
}

static bool flash_end(struct wacom_i2c *wac_i2c)
{
	u8 command[CMD_SIZE];
	int ret, ECH;
	int len = 0;

	command[len++] = 4;
	command[len++] = 0;
	command[len++] = 0x37;
	command[len++] = CMD_SET_FEATURE;
	command[len++] = 5;
	command[len++] = 0;
	command[len++] = 5;
	command[len++] = 0;
	command[len++] = BOOT_CMD_REPORT_ID;
	command[len++] = BOOT_EXIT;
	command[len++] = ECH = 7;

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 2 ret:%d \n", __func__, ret);
		return false;
	}
	msleep(200);

	return true;
}

static bool erase_datamem(struct wacom_i2c *wac_i2c)
{
	u8 command[CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	unsigned char sum = 0;
	unsigned char cmd_chksum;
	int ret, ECH, j;
	int len = 0;

	command[len++] = 4;	/* Command Register-LSB */
	command[len++] = 0;	/* Command Register-MSB */
	command[len++] = 0x37;	/* Command-LSB, ReportType:Feature(11) ReportID:7 */
	command[len++] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
	command[len++] = 5;	/* Data Register-LSB */
	command[len++] = 0;	/* Data-Register-MSB */
	command[len++] = 0x07;	/* Length Field-LSB */
	command[len++] = 0;	/* Length Field-MSB */
	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_ERASE_DATAMEM;	/* Report:erase datamem command */
	command[len++] = ECH = BOOT_ERASE_DATAMEM;	/* Report:echo */
	command[len++] = DATAMEM_SECTOR0;	/* Report:erased block No. */

	sum = 0;
	for (j = 4; j < 12; j++)
		sum += command[j];
	cmd_chksum = ~sum + 1;	/* Report:check sum */
	command[len++] = cmd_chksum;

	ret = wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s failing 1: %d \n", __func__, ret);
		return false;
	}

	do {
		len = 0;
		command[len++] = 4;
		command[len++] = 0;
		command[len++] = 0x38;
		command[len++] = CMD_GET_FEATURE;
		command[len++] = 5;
		command[len++] = 0;

		ret =
		    wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
			printk(KERN_DEBUG"epen:%s failing 2:%d \n", __func__, ret);
			return false;
		}

		ret =
		    wacom_i2c_recv(wac_i2c, response,
				   BOOT_RSP_SIZE, WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
			printk(KERN_DEBUG"epen:%s failing 3:%d \n", __func__, ret);
			return false;
		}

		if ((response[3] != 0x0e || response[4] != ECH)
		    || (response[5] != 0xff && response[5] != 0x00))
			return false;

	} while (response[3] == 0x0e && response[4] == ECH
		 && response[5] == 0xff);

	return true;
}

static bool erase_codemem(struct wacom_i2c *wac_i2c, int *eraseBlock, int num)
{
	u8 command[CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	unsigned char sum = 0;
	unsigned char cmd_chksum;
	int ret, ECH;
	int len = 0;
	int i, j;

	for (i = 0; i < num; i++) {
		len = 0;

		command[len++] = 4;	/* Command Register-LSB */
		command[len++] = 0;	/* Command Register-MSB */
		command[len++] = 0x37;	/* Command-LSB, ReportType:Feature(11) ReportID:7 */
		command[len++] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
		command[len++] = 5;	/* Data Register-LSB */
		command[len++] = 0;	/* Data-Register-MSB */
		command[len++] = 7;	/* Length Field-LSB */
		command[len++] = 0;	/* Length Field-MSB */
		command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
		command[len++] = BOOT_ERASE_FLASH;	/* Report:erase command */
		command[len++] = ECH = i;	/* Report:echo */
		command[len++] = *eraseBlock;	/* Report:erased block No. */
		eraseBlock++;

		sum = 0;
		for (j = 4; j < 12; j++)
			sum += command[j];
		cmd_chksum = ~sum + 1;	/* Report:check sum */
		command[len++] = cmd_chksum;

		ret =
		    wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
			printk(KERN_DEBUG"epen:%s failing 1:%d \n", __func__, i);
			return false;
		}

		do {
			len = 0;
			command[len++] = 4;
			command[len++] = 0;
			command[len++] = 0x38;
			command[len++] = CMD_GET_FEATURE;
			command[len++] = 5;
			command[len++] = 0;

			ret =
			    wacom_i2c_send(wac_i2c, command, len,
					   WACOM_I2C_MODE_BOOT);
			if (ret < 0) {
				printk(KERN_DEBUG"epen:%s failing 2:%d \n", __func__, i);
				return false;
			}

			ret =
			    wacom_i2c_recv(wac_i2c, response,
					   BOOT_RSP_SIZE, WACOM_I2C_MODE_BOOT);
			if (ret < 0) {
				printk(KERN_DEBUG"epen:%s failing 3:%d \n", __func__, i);
				return false;
			}

			if ((response[3] != 0x00 || response[4] != ECH)
			    || (response[5] != 0xff && response[5] != 0x00))
				return false;

		} while (response[3] == 0x00 && response[4] == ECH
			 && response[5] == 0xff);
	}

	return true;
}

static bool flash_erase(struct wacom_i2c *wac_i2c, int *eraseBlock, int num)
{
	bool ret;

	printk(KERN_DEBUG"epen:%s erasing the data mem\n", __func__);
	ret = erase_datamem(wac_i2c);
	if (!ret) {
		printk(KERN_DEBUG"epen:%s erasing datamem failed \n", __func__);
		return false;
	}

	printk(KERN_DEBUG"epen:%s erasing the code mem\n", __func__);
	ret = erase_codemem(wac_i2c, eraseBlock, num);
	if (!ret) {
		printk(KERN_DEBUG"epen:%s erasing codemem failed \n", __func__);
		return false;
	}

	return true;
}

static bool flash_write_block(struct wacom_i2c *wac_i2c, char *flash_data,
			      unsigned long ulAddress, u8 * pcommand_id,
			      int *ECH)
{
	const int MAX_COM_SIZE = (16 + FLASH_BLOCK_SIZE + 2);	//16: num of command[0] to command[15]
	//FLASH_BLOCK_SIZE: unit to erase the block
	//Num of Last 2 checksums
	u8 command[300];
	unsigned char sum = 0;
	int ret, i;

	command[0] = 4;		/* Command Register-LSB */
	command[1] = 0;		/* Command Register-MSB */
	command[2] = 0x37;	/* Command-LSB, ReportType:Feature(11) ReportID:7 */
	command[3] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
	command[4] = 5;		/* Data Register-LSB */
	command[5] = 0;		/* Data-Register-MSB */
	command[6] = 76;	/* Length Field-LSB */
	command[7] = 0;		/* Length Field-MSB */
	command[8] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[9] = BOOT_WRITE_FLASH;	/* Report:program  command */
	command[10] = *ECH = ++(*pcommand_id);	/* Report:echo */
	command[11] = ulAddress & 0x000000ff;
	command[12] = (ulAddress & 0x0000ff00) >> 8;
	command[13] = (ulAddress & 0x00ff0000) >> 16;
	command[14] = (ulAddress & 0xff000000) >> 24;	/* Report:address(4bytes) */
	command[15] = 8;	/* Report:size(8*8=64) */

	sum = 0;
	for (i = 4; i < 16; i++)
		sum += command[i];
	command[MAX_COM_SIZE - 2] = ~sum + 1;	/* Report:command checksum */

	sum = 0;
	for (i = 16; i < (FLASH_BLOCK_SIZE + 16); i++) {
		command[i] = flash_data[ulAddress + (i - 16)];
		sum += flash_data[ulAddress + (i - 16)];
	}

	command[MAX_COM_SIZE - 1] = ~sum + 1;	/* Report:data checksum */

	ret =
	    wacom_i2c_send(wac_i2c, command, (BOOT_CMD_SIZE + 4),
			   WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s 1 ret:%d \n", __func__, ret);
		return false;
	}

	return true;
}

static bool flash_write(struct wacom_i2c *wac_i2c,
			unsigned char *flash_data,
			unsigned long start_address, unsigned long *max_address)
{
	bool bRet = false;
	u8 command_id = 0;
	u8 command[BOOT_RSP_SIZE];
	u8 response[BOOT_RSP_SIZE];
	int ret, i, j, len, ECH = 0, ECH_len = 0;
	int ECH_ARRAY[3];
	unsigned long ulAddress;

	j = 0;
	for (ulAddress = start_address; ulAddress < *max_address;
	     ulAddress += FLASH_BLOCK_SIZE) {
		for (i = 0; i < FLASH_BLOCK_SIZE; i++) {
			if (flash_data[ulAddress + i] != 0xFF)
				break;
		}
		if (i == (FLASH_BLOCK_SIZE))
			continue;
		/* for debug */
		//printk(KERN_DEBUG"epen:write data %#x\n", (unsigned int)ulAddress);
		bRet =
		    flash_write_block(wac_i2c, flash_data, ulAddress,
				      &command_id, &ECH);
		if (!bRet)
			return false;

		if (ECH_len == 3)
			ECH_len = 0;

		ECH_ARRAY[ECH_len++] = ECH;

		if (ECH_len == 3) {
			for (j = 0; j < 3; j++) {
				do {
					len = 0;
					command[len++] = 4;
					command[len++] = 0;
					command[len++] = 0x38;
					command[len++] = CMD_GET_FEATURE;
					command[len++] = 5;
					command[len++] = 0;

					ret =
					    wacom_i2c_send(wac_i2c,
							   command, len,
							   WACOM_I2C_MODE_BOOT);
					if (ret < 0) {
						printk(KERN_DEBUG"epen:%s failing 2:%d \n",
						       __func__, i);
						return false;
					}

					ret =
					    wacom_i2c_recv(wac_i2c,
							   response,
							   BOOT_RSP_SIZE,
							   WACOM_I2C_MODE_BOOT);
					if (ret < 0) {
						printk(KERN_DEBUG"epen:%s failing 3:%d \n",
						       __func__, i);
						return false;
					}

					if ((response[3] != 0x01
					     || response[4] != ECH_ARRAY[j])
					    || (response[5] != 0xff
						&& response[5] != 0x00))
						return false;
					//printk(KERN_DEBUG"epen:addr: %x res:%x \n", ulAddress, response[5]);
				} while (response[3] == 0x01
					 && response[4] == ECH_ARRAY[j]
					 && response[5] == 0xff);
			}
		}
	}

	return true;
}

int wacom_i2c_flash_w9012(struct wacom_i2c *wac_i2c, unsigned char *fw_data)
{
	bool bRet = false;
	int result, i;
	int eraseBlock[200], eraseBlockNum;
	int iBLVer, iMpuType;
	unsigned long max_address = 0;	/* Max.address of Load data */
	unsigned long start_address = 0x2000;	/* Start.address of Load data */

	/*Obtain boot loader version */
	if (!flash_blver(wac_i2c, &iBLVer)) {
		printk(KERN_DEBUG"epen:%s failed to get Boot Loader version \n", __func__);
		return -EXIT_FAIL_GET_BOOT_LOADER_VERSION;
	}
	printk(KERN_DEBUG"epen:BL version: %x \n", iBLVer);

	/*Obtain MPU type: this can be manually done in user space */
	if (!flash_mputype(wac_i2c, &iMpuType)) {
		printk(KERN_DEBUG"epen:%s failed to get MPU type \n", __func__);
		return -EXIT_FAIL_GET_MPU_TYPE;
	}
	if (iMpuType != MPU_W9012) {
		printk(KERN_DEBUG"epen:MPU is not for W9012 : %x \n", iMpuType);
		return -EXIT_FAIL_GET_MPU_TYPE;
	}
	printk(KERN_DEBUG"epen:MPU type: %x \n", iMpuType);

	/*-----------------------------------*/
	/*Flashing operation starts from here */

	/*Set start and end address and block numbers */
	eraseBlockNum = 0;
	start_address = W9012_START_ADDR;
	max_address = W9012_END_ADDR;
	for (i = BLOCK_NUM; i >= 8; i--) {
		eraseBlock[eraseBlockNum] = i;
		eraseBlockNum++;
	}

	msleep(300);

	/*Erase the old program */
	printk(KERN_DEBUG"epen:%s erasing the current firmware \n", __func__);
	bRet = flash_erase(wac_i2c, eraseBlock, eraseBlockNum);
	if (!bRet) {
		printk(KERN_DEBUG"epen:%s failed to erase the user program \n", __func__);
		result = -EXIT_FAIL_ERASE;
		goto fail;
	}

	/*Write the new program */
	printk(KERN_DEBUG"epen:%s writing new firmware \n", __func__);
	bRet = flash_write(wac_i2c, fw_data, start_address, &max_address);
	if (!bRet) {
		printk(KERN_DEBUG"epen:%s failed to write firmware \n", __func__);
		result = -EXIT_FAIL_WRITE_FIRMWARE;
		goto fail;
	}

	/*Return to the user mode */
	printk(KERN_DEBUG"epen:%s closing the boot mode \n", __func__);
	bRet = flash_end(wac_i2c);
	if (!bRet) {
		printk(KERN_DEBUG"epen:%s closing boot mode failed  \n", __func__);
		result = -EXIT_FAIL_WRITING_MARK_NOT_SET;
		goto fail;
	}

	printk(KERN_DEBUG"epen:%s write and verify completed \n", __func__);
	result = EXIT_OK;

 fail:
	return result;
}

int wacom_i2c_flash(struct wacom_i2c *wac_i2c)
{
	int ret;

	if (fw_data == NULL) {
		printk(KERN_ERR "epen:Data is NULL. Exit.\n");
		return -1;
	}

	wac_i2c->pdata->compulsory_flash_mode(true);
	wac_i2c->pdata->reset_platform_hw();
	msleep(200);

	ret = wacom_flash_cmd(wac_i2c);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s cannot send flash command \n", __func__);
	}

	ret = flash_query(wac_i2c);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s Error: cannot send query \n", __func__);
		ret = -EXIT_FAIL;
		goto end_wacom_flash;
	}

	ret = wacom_i2c_flash_w9012(wac_i2c, fw_data);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s Error: flash failed \n", __func__);
		ret = -EXIT_FAIL;
		goto end_wacom_flash;
	}

 end_wacom_flash:
	wac_i2c->pdata->compulsory_flash_mode(false);
	wac_i2c->pdata->reset_platform_hw();
	msleep(200);

	return ret;
}

int wacom_i2c_usermode(struct wacom_i2c *wac_i2c)
{
	int ret;
	bool bRet = false;

	wac_i2c->pdata->compulsory_flash_mode(true);

	ret = wacom_flash_cmd(wac_i2c);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:%s cannot send flash command at user-mode \n", __func__);
		return ret;
	}

	/*Return to the user mode */
	printk(KERN_DEBUG"epen:%s closing the boot mode \n", __func__);
	bRet = flash_end(wac_i2c);
	if (!bRet) {
		printk(KERN_DEBUG"epen:%s closing boot mode failed  \n", __func__);
		ret = -EXIT_FAIL_WRITING_MARK_NOT_SET;
		goto end_usermode;
	}

	wac_i2c->pdata->compulsory_flash_mode(false);

	printk(KERN_DEBUG"epen:%s making user-mode completed \n", __func__);
	ret = EXIT_OK;


 end_usermode:
	return ret;
}
