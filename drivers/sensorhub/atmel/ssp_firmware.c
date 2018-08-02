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

#define SSP_FIRMWARE_REVISION		14060701

/* SPI Speeds */
#define BOOT_SPI_HZ 4800000
#define NORM_SPI_HZ  4800000
/* Bootloader id */
#define BL_EXTENDED_ID_BYTE_ID      0x24u
#define BL_EXTENDED_ID_BYTE_VERSION 0x01u

#if defined(CONFIG_SENSORS_MPU6500_BMI058_DUAL)
#define SSP_BMI_FIRMWARE_REVISION		14031700
#define BL_BMI_FW_NAME   "ssp_at_bmi.fw"
#endif

/* Bootload mode cmd */
#define BL_FW_NAME   "ssp_at.fw"
#define BL_UMS_FW_NAME   "ssp_at.bin"
#define BL_CRASHED_FW_NAME  "ssp_crashed_at.fw"
#define BL_UMS_FW_PATH   255
/* Bootloader mode status */
#define BL_WAITING_BOOTLOAD_CMD		0xc0	/* valid 7 6 bit only */
#define BL_WAITING_FRAME_DATA		0x80	/* valid 7 6 bit only */
#define BL_FRAME_CRC_CHECK		0x02
#define BL_FRAME_CRC_FAIL		0x03
#define BL_FRAME_CRC_PASS		0x04
#define BL_APP_CRC_FAIL			0x40	/* valid 7 8 bit only */
#define BL_BOOT_STATUS_MASK		0x3f

#define BL_VERSION				0x24
#define BL_EXTENDED			0x01

/* Command to unlock bootloader */
#define BL_UNLOCK_CMD_MSB		0xaa
#define BL_UNLOCK_CMD_LSB		0xdc

static int ssp_wait_for_chg(struct ssp_data *data)
{
	int timeout_counter = 0;
	int count = 1E4;

	if (data->mcu_int2 < 0) {
		usleep_range(10000, 10000);
		pr_err( "data->mcu_int2 not initialized");
		return -EIO;
	}

	while ((timeout_counter++ <= count) && gpio_get_value(data->mcu_int2))
		udelay(20);

	if (timeout_counter >= count) {
		pr_err( "ssp_wait_for_chg() timeout!\n");
		return -EIO;
	}

	return 0;
}

unsigned int get_module_rev(struct ssp_data *data)
{
#if defined(CONFIG_SENSORS_MPU6500_BMI058_DUAL)
	if (data->ap_rev < MPU6500_REV)
		return SSP_BMI_FIRMWARE_REVISION;
	else
		return SSP_FIRMWARE_REVISION;
#else
	return SSP_FIRMWARE_REVISION;
#endif
}

static int spi_read_wait_chg(struct ssp_data *data, uint8_t *buffer, int len)
{
	int ret;

	ret = ssp_wait_for_chg(data);

	if (ret < 0) {
		pr_err("[SSP] Error %d while waiting for chg\n", ret);
	}

	ret = spi_read(data->spi, buffer, len);

	if (ret < 0) {
		pr_err("[SSP] Error in %d spi_read()\n", ret);
	}

	return ret;
}

static int spi_write_wait_chg(struct ssp_data *data, const uint8_t *buffer, int len)
{
	int ret;

	ret = ssp_wait_for_chg(data);

	if (ret < 0) {
		pr_err("[SSP] Error %d while waiting for chg\n", ret);
	}

	ret = spi_write(data->spi, buffer, len);

	if (ret < 0) {
		pr_err("[SSP] Error in %d spi_write()\n", ret);
	}

	return ret;
}

static int check_bootloader(struct ssp_data *data, unsigned int uState)
{
	u8 uVal;
	u8 uTemp;

recheck:
	if (spi_read_wait_chg(data, &uVal, 1)  < 0)
		return -EIO;

	pr_debug("%s, uVal = 0x%x\n", __func__, uVal);
	if (uVal & 0x20) {
		if (spi_read_wait_chg(data, &uTemp, 1) < 0) {
			pr_err("[SSP]: %s - spi read fail\n", __func__);
			return -EIO;
		}

		if (spi_read_wait_chg(data, &uTemp, 1) < 0) {
			pr_err("[SSP]: %s - spi read fail\n", __func__);
			return -EIO;
		}

		uVal &= ~0x20;
	}

	if ((uVal & 0xF0) == BL_APP_CRC_FAIL) {
		pr_info("[SSP] SSP_APP_CRC_FAIL - There is no bootloader.\n");
		if (spi_read_wait_chg(data, &uVal, 1) < 0) {
			pr_err("[SSP]: %s - spi read fail\n", __func__);
			return -EIO;
		}

		if (uVal & 0x20) {
			if (spi_read_wait_chg(data, &uTemp, 1)< 0) {
				pr_err("[SSP]: %s - spi read fail\n", __func__);
				return -EIO;
			}

			if (spi_read_wait_chg(data, &uTemp, 1) < 0) {
				pr_err("[SSP]: %s - spi read fail\n", __func__);
				return -EIO;
			}

			uVal &= ~0x20;
		}
	}

	switch (uState) {
	case BL_WAITING_BOOTLOAD_CMD:
	case BL_WAITING_FRAME_DATA:
		uVal &= ~BL_BOOT_STATUS_MASK;
		break;
	case BL_FRAME_CRC_PASS:
		if (uVal == BL_FRAME_CRC_CHECK) {
			goto recheck;
		}
		break;
	default:
		return -EINVAL;
	}

	if (uVal != uState) {
		pr_err("[SSP]: %s - Invalid bootloader mode state\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int unlock_bootloader(struct ssp_data *data)
{
	u8 uBuf[2];

	uBuf[0] = BL_UNLOCK_CMD_LSB;
	uBuf[1] = BL_UNLOCK_CMD_MSB;

	if (spi_write_wait_chg(data, uBuf, 2) < 0) {
		pr_err("[SSP]: %s - spi write failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int fw_write(struct ssp_data *data,
	const u8 *pData, unsigned int uFrameSize)
{
	if (spi_write_wait_chg(data, pData, uFrameSize) < 0) {
		pr_err("[SSP]: %s - spi write failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int load_ums_fw_bootmode(struct ssp_data *data, const char *pFn)
{
	const u8 *buff = NULL;
	char fw_path[BL_UMS_FW_PATH+1];
	unsigned int uFrameSize;
	unsigned int uFSize = 0, uNRead = 0;
	unsigned int uPos = 0;
	int iRet = SUCCESS;
	int iCheckFrameCrcError = 0;
	int iCheckWatingFrameDataError = 0;
	int count = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs = get_fs();

	pr_info("[SSP] ssp_load_ums_fw start!!!\n");

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, BL_UMS_FW_PATH, "/sdcard/ssp/%s", pFn);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		iRet = ERROR;
		pr_err("[SSP] file %s open error : %d\n", fw_path, (s32)fp);
		goto err_open;
	}

	uFSize = (unsigned int)fp->f_path.dentry->d_inode->i_size;
	pr_info("[SSP] load_ums_firmware size : %u\n", uFSize);

	buff = kzalloc((size_t)uFSize, GFP_KERNEL);
	if (!buff) {
		iRet = ERROR;
		pr_err("[SSP] fail to alloc buffer for ums fw\n");
		goto err_alloc;
	}

	uNRead = (unsigned int)vfs_read(fp, (char __user *)buff,
				(unsigned int)uFSize, &fp->f_pos);
	if (uNRead != uFSize) {
		iRet = ERROR;
		pr_err("[SSP] fail to read file %s (nread = %u)\n", fw_path, uNRead);
		goto err_fw_size;
	}

	/* Unlock bootloader */
	iRet = unlock_bootloader(data);
	if (iRet < 0) {
		pr_err("[SSP] %s - unlock_bootloader failed! %d\n",
			__func__, iRet);
		goto out;
	}

	while (uPos < uFSize) {
		if (check_bootloader(data, BL_WAITING_FRAME_DATA)) {
			iCheckWatingFrameDataError++;
			if (iCheckWatingFrameDataError > 10) {
				iRet = ERROR;
				pr_err("[SSP]: %s - F/W update fail\n",
					__func__);
				goto out;
			} else {
				pr_err("[SSP]: %s - F/W data_error %d, retry\n",
					__func__, iCheckWatingFrameDataError);
				continue;
			}
		}

		uFrameSize = (unsigned int)((*(buff + uPos) << 8) |
			*(buff + uPos + 1));

		/* We should add 2 at frame size as the the firmware data is not
		*  included the CRC bytes.
		*/
		uFrameSize += 2;

		/* Write one frame to device */
		fw_write(data, buff + uPos, uFrameSize);
		if (check_bootloader(data, BL_FRAME_CRC_PASS)) {
			iCheckFrameCrcError++;
			if (iCheckFrameCrcError > 10) {
				iRet = ERROR;
				pr_err("[SSP]: %s - F/W Update Fail. crc err\n",
					__func__);
				goto out;
			} else {
				pr_err("[SSP]: %s - F/W crc_error %d, retry\n",
					__func__, iCheckFrameCrcError);
				continue;
			}
		}

		uPos += uFrameSize;
		if (count++ == 100) {
			pr_info("[SSP] Updated %u bytes / %u bytes\n", uPos,
				uFSize);
			count = 0;
		}
	}

out:
err_fw_size:
	kfree(buff);
err_alloc:
	filp_close(fp, NULL);
err_open:
	set_fs(old_fs);

	return iRet;
}

static int load_kernel_fw_bootmode(struct ssp_data *data, const char *pFn)
{
	const struct firmware *fw = NULL;
	unsigned int uFrameSize;
	unsigned int uPos = 0;
	int iRet;
	int iCheckFrameCrcError = 0;
	int iCheckWatingFrameDataError = 0;
	int count = 0;

	pr_info("[SSP] ssp_load_fw start!!!\n");

	iRet = request_firmware(&fw, pFn, &data->spi->dev);
	if (iRet) {
		pr_err("[SSP]: %s - Unable to open firmware %s\n",
			__func__, pFn);
		return iRet;
	}

	/* Unlock bootloader */
	iRet = unlock_bootloader(data);
	if (iRet < 0) {
		pr_err("[SSP] %s - unlock_bootloader failed! %d\n",
			__func__, iRet);
		goto out;
	}

	while (uPos < fw->size) {
		iRet = check_bootloader(data, BL_WAITING_FRAME_DATA);
		if (iRet) {
			iCheckWatingFrameDataError++;
			if (iCheckWatingFrameDataError > 10) {
				pr_err("[SSP]: %s - F/W update fail\n",
					__func__);
				goto out;
			} else {
				pr_err("[SSP]: %s - F/W data_error %d, retry\n",
					__func__, iCheckWatingFrameDataError);
				continue;
			}
		}

		uFrameSize = ((*(fw->data + uPos) << 8) |
			*(fw->data + uPos + 1));

		/* We should add 2 at frame size as the the firmware data is not
		*  included the CRC bytes.
		*/
		uFrameSize += 2;

		/* Write one frame to device */
		fw_write(data, fw->data + uPos, uFrameSize);
		iRet = check_bootloader(data, BL_FRAME_CRC_PASS);
		if (iRet) {
			iCheckFrameCrcError++;
			if (iCheckFrameCrcError > 10) {
				pr_err("[SSP]: %s - F/W Update Fail. crc err\n",
					__func__);
				goto out;
			} else {
				pr_err("[SSP]: %s - F/W crc_error %d, retry\n",
					__func__, iCheckFrameCrcError);
				continue;
			}
		}

		uPos += uFrameSize;
		if (count++ == 100) {
			pr_info("[SSP] Updated %d bytes / %zd bytes\n", uPos,
				fw->size);
			count = 0;
		}
	}
	pr_info("[SSP] Firmware download is success.(%d bytes)\n", uPos);
out:
	release_firmware(fw);
	return iRet;
}

static int change_to_bootmode(struct ssp_data *data)
{
	int iCnt;

	for (iCnt = 0; iCnt < 10; iCnt++) {
		gpio_set_value_cansleep(data->rst, 0);
		usleep_range(15000, 15500);
		gpio_set_value_cansleep(data->rst, 1);
		mdelay(50);
	}

	msleep(50);

	if (gpio_get_value(data->mcu_int2))
	{
		pr_err("[SSP]: Failed to enter bootmode in %s", __func__);
		return -EIO;
	}
	return 0;
}

void toggle_mcu_reset(struct ssp_data *data)
{
	gpio_set_value_cansleep(data->rst, 0);

	usleep_range(1000, 1200);

	gpio_set_value_cansleep(data->rst, 1);

	msleep(50);

	if (!gpio_get_value(data->mcu_int2)) {
		pr_err("[SSP]: SH has entered bootloader in %s !!!!!", __func__);
		//return -EIO;
	}
	//return 0;
}

static int update_mcu_bin(struct ssp_data *data, int iBinType)
{

        int iRet = SUCCESS;
        uint8_t bfr[3];

        pr_info("[SSP] ssp_change_to_bootmode\n");
        change_to_bootmode(data);

        iRet = spi_read_wait_chg(data, bfr, 3);
        if (iRet < 0) {
	        pr_err("[SSP] unable to contact bootmode = %d\n", iRet);
               return iRet;
        }

        if (bfr[1] != BL_VERSION ||
             bfr[2] != BL_EXTENDED) {
                pr_err("[SSP] unable to enter bootmode= %d\n", iRet);
                return -EIO;
        }

	switch (iBinType) {
	case KERNEL_BINARY:
#if defined(CONFIG_SENSORS_MPU6500_BMI058_DUAL)
		if (data->ap_rev < MPU6500_REV)
			iRet = load_kernel_fw_bootmode(data,	BL_BMI_FW_NAME);
		else
			iRet = load_kernel_fw_bootmode(data,	BL_FW_NAME);
#else
		iRet = load_kernel_fw_bootmode(data,	BL_FW_NAME);
#endif
		break;
	case KERNEL_CRASHED_BINARY:
		iRet = load_kernel_fw_bootmode(data,	BL_CRASHED_FW_NAME);
		break;
	case UMS_BINARY:
		iRet = load_ums_fw_bootmode(data, BL_UMS_FW_NAME);
		break;
	default:
		pr_err("[SSP] binary type error!!\n");
	}

	return iRet;
}

int forced_to_download_binary(struct ssp_data *data, int iBinType)
{
	int iRet = 0;
	int retry = 3;

	ssp_dbg("[SSP] %s, mcu binany update!\n", __func__);
	ssp_enable(data, false);

	data->fw_dl_state = FW_DL_STATE_DOWNLOADING;

	pr_info("[SSP] %s, DL state = %d\n", __func__, data->fw_dl_state);

	data->spi->max_speed_hz = BOOT_SPI_HZ;
	if (spi_setup(data->spi))
		pr_err("failed to setup spi for ssp_boot\n");

	do {
		pr_info("[SSP] %d try\n", 3 - retry);
		iRet = update_mcu_bin(data, iBinType);
	} while (retry -- > 0 && iRet < 0);

	data->spi->max_speed_hz = NORM_SPI_HZ;
	if (spi_setup(data->spi))
		pr_err("failed to setup spi for ssp_norm\n");

	if (iRet < 0) {
		ssp_dbg("[SSP] %s, update_mcu_bin failed!\n", __func__);
		goto out;
	}

	data->fw_dl_state = FW_DL_STATE_SYNC;
	pr_info("[SSP] %s, DL state = %d\n", __func__, data->fw_dl_state);
	ssp_enable(data, true);

	proximity_open_lcd_ldi(data);
	proximity_open_calibration(data);
	accel_open_calibration(data);
	gyro_open_calibration(data);
	pressure_open_calibration(data);

	data->fw_dl_state = FW_DL_STATE_DONE;
	pr_info("[SSP] %s, DL state = %d\n", __func__, data->fw_dl_state);

	iRet = SUCCESS;
out:
	return iRet;
}

int check_fwbl(struct ssp_data *data)
{
	int iRet;
	unsigned int fw_revision;

#if defined(CONFIG_SENSORS_MPU6500_BMI058_DUAL)
	if (data->ap_rev < MPU6500_REV)
		fw_revision = SSP_BMI_FIRMWARE_REVISION;
	else
		fw_revision = SSP_FIRMWARE_REVISION;
#else
	fw_revision = SSP_FIRMWARE_REVISION;
#endif
	data->uCurFirmRev = get_firmware_rev(data);

	if ((data->uCurFirmRev == SSP_INVALID_REVISION) \
		|| (data->uCurFirmRev == SSP_INVALID_REVISION2)) {
		iRet = check_bootloader(data, BL_WAITING_BOOTLOAD_CMD);

		if (iRet >= 0)
			pr_info("[SSP] ssp_load_fw_bootmode\n");
		else {
			pr_warn("[SSP] Firm Rev is invalid(%8u). Retry.\n",
				data->uCurFirmRev);
			data->uCurFirmRev = get_firmware_rev(data);
		}
		data->uCurFirmRev = SSP_INVALID_REVISION;
		pr_err("[SSP] SSP_INVALID_REVISION\n");
		return FW_DL_STATE_NEED_TO_SCHEDULE;
	} else {
		if (data->uCurFirmRev != fw_revision) {
			pr_info("[SSP] MCU Firm Rev : Old = %8u, New = %8u\n",
				data->uCurFirmRev, fw_revision);

			return FW_DL_STATE_NEED_TO_SCHEDULE;
		}
		pr_info("[SSP] MCU Firm Rev : Old = %8u, New = %8u\n",
			data->uCurFirmRev, fw_revision);
	}

	return FW_DL_STATE_NONE;
}
