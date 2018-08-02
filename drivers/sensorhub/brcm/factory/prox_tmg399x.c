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
#include "../ssp.h"

#define	VENDOR		"AMS"
#if defined(CONFIG_SENSORS_SSP_TMG399x)
#define	CHIP_ID		"TMG399X"
#elif defined(CONFIG_SENSORS_SSP_TMD3725)
#define CHIP_ID		"TMD3725"
#elif defined(CONFIG_SENSORS_SSP_TMD3782)
#define CHIP_ID		"TMD3782"
#elif defined(CONFIG_SENSORS_SSP_TMD4903)
#define CHIP_ID		"TMD4903"
#elif defined(CONFIG_SENSORS_SSP_TMD4904)
#define CHIP_ID		"TMD4904"
#else
#define CHIP_ID		"UNKNOWN"
#endif

#define CANCELATION_FILE_PATH	"/efs/FactoryApp/prox_cal"
#define LCD_LDI_FILE_PATH	"/sys/class/lcd/panel/window_type"

#define LINE_1		'4'
#define LINE_2		'2'

#define LDI_OTHERS	'0'
#define LDI_GRAY	'1'
#define LDI_WHITE	'2'

#define TBD_HIGH_THRESHOLD	185
#define TBD_LOW_THRESHOLD	145
#define WHITE_HIGH_THRESHOLD	185
#define WHITE_LOW_THRESHOLD	145

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static ssize_t prox_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t prox_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	struct ssp_data *data = dev_get_drvdata(dev);
	int device_id = 0;
	device_id = get_proximity_device_id(data);

	if(device_id == TMD4903)
	{
		return sprintf(buf, "%s\n", "TMD4903");
	}
	else if(device_id ==  TMD4904)
	{
		return sprintf(buf, "%s\n", "TMD4904");
	}
	else
	{
		pr_err("[SSP]: %s - Unkown proximity device \n", __func__);
		return sprintf(buf, "%s\n", "UNKNOWN");
	}
#else
	return sprintf(buf, "%s\n", CHIP_ID);
#endif
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[PROXIMITY_RAW].prox_raw[1],
		data->buf[PROXIMITY_RAW].prox_raw[2],
		data->buf[PROXIMITY_RAW].prox_raw[3]);
}

static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char chTempbuf[4] = { 0 };
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable) {
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 4);
		data->bProximityRawEnabled = true;
	} else {
		send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
			chTempbuf, 4);
		data->bProximityRawEnabled = false;
	}

	return size;
}

static u16 get_proximity_rawdata(struct ssp_data *data)
{
	u16 uRowdata = 0;
	char chTempbuf[4] = { 0 };

	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	if (data->bProximityRawEnabled == false) {
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 4);
		msleep(200);
		uRowdata = data->buf[PROXIMITY_RAW].prox_raw[0];
		send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
			chTempbuf, 4);
	} else {
		uRowdata = data->buf[PROXIMITY_RAW].prox_raw[0];
	}

	return uRowdata;
}

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", get_proximity_rawdata(data));
}

static ssize_t proximity_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", get_proximity_rawdata(data));
}

#ifdef CONFIG_SENSOR_SSP_PROXIMTY_FOR_WINDOW_TYPE
static int proximity_open_lcd_ldi(struct ssp_data *data)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cancel_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(LCD_LDI_FILE_PATH, O_RDONLY, 0660);
	if (IS_ERR(cancel_filp)) {
		iRet = PTR_ERR(cancel_filp);
		if (iRet != -ENOENT)
			pr_err("[SSP]: %s - Can't open lcd ldi file\n",
				__func__);
		set_fs(old_fs);
		data->chLcdLdi[0] = 0;
		data->chLcdLdi[1] = 0;
		goto exit;
	}

	iRet = cancel_filp->f_op->read(cancel_filp,
		(u8 *)data->chLcdLdi, sizeof(u8) * 2, &cancel_filp->f_pos);
	if (iRet != (sizeof(u8) * 2)) {
		pr_err("[SSP]: %s - Can't read the lcd ldi data\n", __func__);
		data->chLcdLdi[0] = 0;
		data->chLcdLdi[1] = 0;
		iRet = -EIO;
	}

	ssp_dbg("[SSP]: %s - %c%c\n", __func__,
		data->chLcdLdi[0], data->chLcdLdi[1]);

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

exit:
	switch (data->chLcdLdi[1]) {
	case LDI_GRAY:
		data->uProxHiThresh_default = TBD_HIGH_THRESHOLD;
		data->uProxLoThresh_default = TBD_LOW_THRESHOLD;
		break;
	case LDI_WHITE:
		data->uProxHiThresh_default = WHITE_HIGH_THRESHOLD;
		data->uProxLoThresh_default = WHITE_LOW_THRESHOLD;
		break;
	case LDI_OTHERS:
		data->uProxHiThresh_default = DEFUALT_HIGH_THRESHOLD;
		data->uProxLoThresh_default = DEFUALT_LOW_THRESHOLD;
		break;
	default:
		data->uProxHiThresh_default = DEFUALT_HIGH_THRESHOLD;
		data->uProxLoThresh_default = DEFUALT_LOW_THRESHOLD;
		break;
	}

	return iRet;
}
#endif

void get_proximity_threshold(struct ssp_data *data)
{
#ifdef CONFIG_SENSOR_SSP_PROXIMTY_FOR_WINDOW_TYPE
	proximity_open_lcd_ldi(data);
#endif
	data->uProxHiThresh = data->uProxHiThresh_default;
	data->uProxLoThresh = data->uProxLoThresh_default;

#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	//for tmd4904
	data->uProxHiThresh_tmd4904 = data->uProxHiThresh_default_tmd4904;
	data->uProxLoThresh_tmd4904 = data->uProxLoThresh_default_tmd4904;
#endif
}

int proximity_open_calibration(struct ssp_data *data)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cancel_filp = NULL;
	
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH, O_RDONLY, 0660);
	if (IS_ERR(cancel_filp)) {
		iRet = PTR_ERR(cancel_filp);
		if (iRet != -ENOENT)
			pr_err("[SSP]: %s - Can't open cancelation file\n",
				__func__);
		set_fs(old_fs);
		goto exit;
	}

	iRet = cancel_filp->f_op->read(cancel_filp,
		(u8 *)&data->uProxCanc, sizeof(unsigned int), &cancel_filp->f_pos);
	if (iRet != sizeof(u8)) {
		pr_err("[SSP]: %s - Can't read the cancel data\n", __func__);
		iRet = -EIO;
	}

	/*If there is an offset cal data. */
	if (data->uProxCanc != 0) {	
#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
		//for tmd4904
		int device_id = 0;
		device_id = get_proximity_device_id(data);
		if(device_id == TMD4903)
		{
			data->uProxHiThresh =
				data->uProxHiThresh_default + data->uProxCanc;
			data->uProxLoThresh =
				data->uProxLoThresh_default + data->uProxCanc;
			
			pr_info("[SSP] %s: proximity ps_canc = %d, ps_thresh hi - %d lo - %d\n",
				__func__, data->uProxCanc, data->uProxHiThresh, data->uProxLoThresh);
		}
		else if(device_id == TMD4904)
		{
			//for tmd4904
			data->uProxHiThresh_tmd4904 = data->uProxHiThresh_default_tmd4904 + data->uProxCanc;
			data->uProxLoThresh_tmd4904 = data->uProxLoThresh_default_tmd4904 + data->uProxCanc;
			
			pr_info("[SSP] %s: proximity TMD4904 ps_canc = %d, ps_thresh hi - %d lo - %d\n",
				__func__, data->uProxCanc, data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904);
		}
		else
		{
			pr_info("[SSP] %s: unkonwn proximity \n", __func__);
		}
#else
		data->uProxHiThresh =
			data->uProxHiThresh_default + data->uProxCanc;
		data->uProxLoThresh =
			data->uProxLoThresh_default + data->uProxCanc;
		
		pr_info("[SSP] %s: proximity ps_canc = %d, ps_thresh hi - %d lo - %d\n",
			__func__, data->uProxCanc, data->uProxHiThresh, data->uProxLoThresh);
#endif
	}

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

exit:
	return iRet;
}

static int calculate_proximity_threshold(struct ssp_data *data)
{
#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	//for tmd4904
	int device_id = 0;
	device_id = get_proximity_device_id(data);

	if(device_id == TMD4903)
	{
		if (data->uCrosstalk < data->uProxLoThresh_cal) {
			data->uProxCanc = 0;
			data->uProxCalResult = 2;
		} else if (data->uCrosstalk <= data->uProxHiThresh_cal) {
			data->uProxCanc = data->uCrosstalk * 5 / 10;
			data->uProxCalResult = 1;
		} else {
			data->uProxCanc = 0;
			data->uProxCalResult = 0;
			pr_info("[SSP] crosstalk > %d, calibration failed\n",
				data->uProxHiThresh_cal);
			return ERROR;
		}
		data->uProxHiThresh = data->uProxHiThresh_default + data->uProxCanc;
		data->uProxLoThresh = data->uProxLoThresh_default + data->uProxCanc;

		pr_info("[SSP] %s - TMD4903 crosstalk_offset = %u(%u), HI_THD = %u, LOW_THD = %u\n",
			__func__, data->uProxCanc, data->uCrosstalk, data->uProxHiThresh, data->uProxLoThresh);

	}
	else if(device_id == TMD4904)
	{
		// for tmd4904
		if (data->uCrosstalk < data->uProxLoThresh_cal_tmd4904) {
			data->uProxCanc = 0;
			data->uProxCalResult = 2;
		} else if (data->uCrosstalk <= data->uProxHiThresh_cal_tmd4904) {
			data->uProxCanc = data->uCrosstalk * 5 / 10;
			data->uProxCalResult = 1;
		} else {
			data->uProxCanc = 0;
			data->uProxCalResult = 0;
			pr_info("[SSP] TMD4904 crosstalk > %d, calibration failed\n",
				data->uProxHiThresh_cal);
			return ERROR;
		}
		data->uProxHiThresh_tmd4904 = data->uProxHiThresh_default_tmd4904 + data->uProxCanc;
		data->uProxLoThresh_tmd4904 = data->uProxLoThresh_default_tmd4904 + data->uProxCanc;

		pr_info("[SSP] %s - crosstalk_offset = %u(%u), HI_THD = %u, LOW_THD = %u\n",
		__func__, data->uProxCanc, data->uCrosstalk, data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904);
	}
	else
	{
		if (data->uCrosstalk < data->uProxLoThresh_cal) {
			data->uProxCanc = 0;
			data->uProxCalResult = 2;
		} else if (data->uCrosstalk <= data->uProxHiThresh_cal) {
			data->uProxCanc = data->uCrosstalk * 5 / 10;
			data->uProxCalResult = 1;
		} else {
			data->uProxCanc = 0;
			data->uProxCalResult = 0;
			pr_info("[SSP] crosstalk > %d, calibration failed\n",
				data->uProxHiThresh_cal);
			return ERROR;
		}
		data->uProxHiThresh = data->uProxHiThresh_default + data->uProxCanc;
		data->uProxLoThresh = data->uProxLoThresh_default + data->uProxCanc;

		pr_info("[SSP] %s - UNKNOWN crosstalk_offset = %u(%u), HI_THD = %u, LOW_THD = %u\n",
			__func__, data->uProxCanc, data->uCrosstalk, data->uProxHiThresh, data->uProxLoThresh);
	}
#else
	if (data->uCrosstalk < data->uProxLoThresh_cal) {
			data->uProxCanc = 0;
			data->uProxCalResult = 2;
		} else if (data->uCrosstalk <= data->uProxHiThresh_cal) {
			data->uProxCanc = data->uCrosstalk * 5 / 10;
			data->uProxCalResult = 1;
		} else {
			data->uProxCanc = 0;
			data->uProxCalResult = 0;
			pr_info("[SSP] crosstalk > %d, calibration failed\n",
				data->uProxHiThresh_cal);
			return ERROR;
		}
		data->uProxHiThresh = data->uProxHiThresh_default + data->uProxCanc;
		data->uProxLoThresh = data->uProxLoThresh_default + data->uProxCanc;

		pr_info("[SSP] %s - crosstalk_offset = %u(%u), HI_THD = %u, LOW_THD = %u\n",
			__func__, data->uProxCanc, data->uCrosstalk, data->uProxHiThresh, data->uProxLoThresh);
#endif

	return SUCCESS;
}

static int proximity_store_cancelation(struct ssp_data *data, int iCalCMD)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cancel_filp = NULL;

	if (iCalCMD) {
		data->uCrosstalk = get_proximity_rawdata(data);
		iRet = calculate_proximity_threshold(data);
	} else {
		data->uProxHiThresh = data->uProxHiThresh_default;
		data->uProxLoThresh = data->uProxLoThresh_default;
		
#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
		//for tmd 4904
		data->uProxHiThresh_tmd4904 = data->uProxHiThresh_default_tmd4904;
		data->uProxLoThresh_tmd4904 = data->uProxLoThresh_default_tmd4904;
#endif
		data->uProxCanc = 0;
	}

	if (iRet != ERROR)
		set_proximity_threshold(data);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(cancel_filp)) {
		pr_err("%s: Can't open cancelation file\n", __func__);
		set_fs(old_fs);
		iRet = PTR_ERR(cancel_filp);
		return iRet;
	}

	iRet = cancel_filp->f_op->write(cancel_filp, (u8 *)&data->uProxCanc,
		sizeof(unsigned int), &cancel_filp->f_pos);
	if (iRet != sizeof(unsigned int)) {
		pr_err("%s: Can't write the cancel data to file\n", __func__);
		iRet = -EIO;
	}

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

	return iRet;
}

static ssize_t proximity_cancel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	//for tmd4904
	int device_id = 0;
	device_id = get_proximity_device_id(data);

	if(device_id == TMD4903)
	{
		ssp_dbg("[SSP]: TMD4903 uProxThresh : hi : %u lo : %u, uProxCanc = %u\n",
			data->uProxHiThresh, data->uProxLoThresh, data->uProxCanc);

		return sprintf(buf, "%u,%u,%u\n", data->uProxCanc,
			data->uProxHiThresh, data->uProxLoThresh);
	}
	else if(device_id == TMD4904)
	{
		ssp_dbg("[SSP]: TMD4904 uProxThresh : hi : %u lo : %u, uProxCanc = %u\n",
			data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904, data->uProxCanc);

		return sprintf(buf, "%u,%u,%u\n", data->uProxCanc,
			data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904);
	}
	else
	{
		ssp_dbg("[SSP] %s: UNKNOWN Proximity device \n", __func__);
	}
#else
	ssp_dbg("[SSP]: uProxThresh : hi : %u lo : %u, uProxCanc = %u\n",
		data->uProxHiThresh, data->uProxLoThresh, data->uProxCanc);

	return sprintf(buf, "%u,%u,%u\n", data->uProxCanc,
		data->uProxHiThresh, data->uProxLoThresh);
#endif

	return sprintf(buf, "%u,%u,%u\n", data->uProxCanc,
		data->uProxHiThresh, data->uProxLoThresh);
}

static ssize_t proximity_cancel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iCalCMD = 0, iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* calibrate cancelation value */
		iCalCMD = 1;
	else if (sysfs_streq(buf, "0")) /* reset cancelation value */
		iCalCMD = 0;
	else {
		pr_debug("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	iRet = proximity_store_cancelation(data, iCalCMD);
	if (iRet < 0) {
		pr_err("[SSP]: - %s proximity_store_cancelation() failed\n",
			__func__);
		return iRet;
	}

	ssp_dbg("[SSP]: %s - %u\n", __func__, iCalCMD);
	return size;
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	//for tmd4904
	int device_id = 0;
	device_id = get_proximity_device_id(data);
	
	if(device_id == TMD4903)
	{
		ssp_dbg("[SSP]: TMD4903 uProxThresh = hi - %u, lo - %u\n",
			data->uProxHiThresh, data->uProxLoThresh);

		return sprintf(buf, "%u,%u\n", data->uProxHiThresh, data->uProxLoThresh);
	}
	else if(device_id == TMD4904)
	{
		ssp_dbg("[SSP]: TMD4904 uProxThresh = hi - %u, lo - %u\n",
			data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904);

		return sprintf(buf, "%u,%u\n", data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904);
	}
	else
	{
		ssp_dbg("[SSP]: %s - UNKNOWN Proximity Device \n", __func__);
	}
#else 
	ssp_dbg("[SSP]: TMD4903 uProxThresh = hi - %u, lo - %u\n",
		data->uProxHiThresh, data->uProxLoThresh);

	return sprintf(buf, "%u,%u\n", data->uProxHiThresh, data->uProxLoThresh);
#endif

	return sprintf(buf, "%u,%u\n", data->uProxHiThresh, data->uProxLoThresh);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	int device_id = 0;
#endif

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrtoint failed.(%d)\n", __func__, iRet);
	else {
		if(uNewThresh & 0xc000)
			pr_err("[SSP]: %s - allow 14bits.(%d)\n", __func__, uNewThresh);
		else {
			uNewThresh &= 0x3fff;

#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
			//for tmd4904
			device_id = get_proximity_device_id(data);

			if(device_id == TMD4903)
			{
				data->uProxHiThresh = data->uProxHiThresh_default = uNewThresh;
			}
			else if(device_id == TMD4904)
			{
				//for tmd4904
				data->uProxHiThresh_tmd4904 = data->uProxHiThresh_default_tmd4904 = uNewThresh;
			}
#else
			data->uProxHiThresh = data->uProxHiThresh_default = uNewThresh;
#endif
			set_proximity_threshold(data);
		}
	}

#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u, lo - %u, hi - %u, lo - %u \n",
		__func__, data->uProxHiThresh, data->uProxLoThresh, data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904);
#else
	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u, lo - %u\n",
		__func__, data->uProxHiThresh, data->uProxLoThresh);
#endif

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	//for tmd4904
	int device_id = 0;
	device_id = get_proximity_device_id(data);
	
	if(device_id == TMD4903)
	{
		ssp_dbg("[SSP]: TMD4903 uProxThresh = hi - %u, lo - %u\n",
			data->uProxHiThresh, data->uProxLoThresh);

		return sprintf(buf, "%u,%u\n", data->uProxHiThresh, data->uProxLoThresh);
	}
	else if(device_id == TMD4904)
	{
		ssp_dbg("[SSP]: TMD4903 uProxThresh = hi - %u, lo - %u\n",
			data->uProxHiThresh, data->uProxLoThresh);

		return sprintf(buf, "%u,%u\n", data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904);
	}
	else
	{
		ssp_dbg("[SSP]: %s - UNKNOWN Proximity Device \n",__func__);
	}
#else
	ssp_dbg("[SSP]: uProxThresh = hi - %u, lo - %u\n",
			data->uProxHiThresh, data->uProxLoThresh);
#endif
	return sprintf(buf, "%u,%u\n", data->uProxHiThresh,
		data->uProxLoThresh);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	int device_id = 0;
#endif

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrtoint failed.(%d)\n", __func__, iRet);
	else {
		if(uNewThresh & 0xc000)
			pr_err("[SSP]: %s - allow 14bits.(%d)\n", __func__, uNewThresh);
		else {
			uNewThresh &= 0x3fff;

#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
			//for tmd4904
			device_id = get_proximity_device_id(data);
			if(device_id == TMD4903)
			{
				data->uProxLoThresh = data->uProxLoThresh_default = uNewThresh;
			}
			else if(device_id == TMD4904)
			{
				// for tmd4904
				data->uProxLoThresh_tmd4904 = data->uProxLoThresh_default_tmd4904 = uNewThresh;
			}
#else
			data->uProxLoThresh = data->uProxLoThresh_default = uNewThresh;
#endif
			set_proximity_threshold(data);
		}
	}


#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u, lo - %u, hi - %u, lo - %u \n",
		__func__, data->uProxHiThresh, data->uProxLoThresh, data->uProxHiThresh_tmd4904, data->uProxLoThresh_tmd4904);
#else
	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u, lo - %u\n",
		__func__, data->uProxHiThresh, data->uProxLoThresh);
#endif
	return size;
}

static ssize_t proximity_cancel_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s, %u\n", __func__, data->uProxCalResult);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->uProxCalResult);
}

static ssize_t proximity_default_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet, iReties = 0;
	struct ssp_msg *msg;
	u8 buffer[8] = {0,};
	int trim;

retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP]: %s - failed to allocate memory\n", __func__);
		return FAIL;
	}
	msg->cmd = MSG2SSP_AP_PROX_GET_TRIM;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);

		if (iReties++ < 2) {
			pr_err("[SSP] %s fail, retry\n", __func__);
			mdelay(5);
			goto retries;
		}
		return FAIL;
	}

	trim = (int)buffer[0];
	
	pr_info("[SSP] %s - %d \n", __func__, trim);
	
	return snprintf(buf, PAGE_SIZE, "%d\n", trim);
}

static ssize_t proximity_probe_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	bool probe_pass_fail = FAIL;

	if (data->uSensorState & (1 << PROXIMITY_SENSOR)) {
        probe_pass_fail = SUCCESS;
	}
	else {
        probe_pass_fail = FAIL;
	}

	pr_info("[SSP]: %s - All sensor 0x%llx, prox_sensor %d \n",
		__func__, data->uSensorState, probe_pass_fail);
	
	return snprintf(buf, PAGE_SIZE, "%d\n", probe_pass_fail);
}

static ssize_t barcode_emul_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->bBarcodeEnabled);
}

static ssize_t barcode_emul_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable)
		set_proximity_barcode_enable(data, true);
	else
		set_proximity_barcode_enable(data, false);

	return size;
}

static DEVICE_ATTR(vendor, S_IRUGO, prox_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, prox_name_show, NULL);
static DEVICE_ATTR(state, S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, proximity_raw_data_show, NULL);
static DEVICE_ATTR(barcode_emul_en, S_IRUGO | S_IWUSR | S_IWGRP,
	barcode_emul_enable_show, barcode_emul_enable_store);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_cancel_show, proximity_cancel_store);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_low_show, proximity_thresh_low_store);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO, proximity_cancel_pass_show, NULL);
static DEVICE_ATTR(prox_trim, S_IRUGO, proximity_default_trim_show, NULL);
static DEVICE_ATTR(prox_probe, S_IRUGO, proximity_probe_show, NULL);

static struct device_attribute *prox_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_state,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_prox_cal,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_barcode_emul_en,
	&dev_attr_prox_offset_pass,
	&dev_attr_prox_trim,
	&dev_attr_prox_probe,
	NULL,
};

void initialize_prox_factorytest(struct ssp_data *data)
{
	sensors_register(data->prox_device, data,
		prox_attrs, "proximity_sensor");
}

void remove_prox_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->prox_device, prox_attrs);
}
