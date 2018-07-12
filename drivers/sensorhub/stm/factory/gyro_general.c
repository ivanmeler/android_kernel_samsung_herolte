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
#include <linux/kernel.h>
#include "../ssp.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define INV_ID				0
#define VENDOR_INV			"INVENSENSE"
#define CHIP_ID_INV			"MPU6500"

#define STM_ID				1
#define VENDOR_STM			"STM"
#define CHIP_ID_STM			"K6DS3TR"

#define BOSCH_ID			2
#define VENDOR_BOSCH			"BOSCH"
#define CHIP_ID_BOSCH			"BMI168"


#define CALIBRATION_FILE_PATH		"/efs/FactoryApp/gyro_cal_data"
#define VERBOSE_OUT			(1)
#define CALIBRATION_DATA_AMOUNT		(20)
#define DEF_GYRO_FULLSCALE		(2000)
#define DEF_GYRO_SENS			(32768 / DEF_GYRO_FULLSCALE)
#define DEF_BIAS_LSB_THRESH_SELF	(20 * DEF_GYRO_SENS)
#define DEF_BIAS_LSB_THRESH_SELF_6500	(30 * DEF_GYRO_SENS)
#define DEF_RMS_LSB_TH_SELF		(5 * DEF_GYRO_SENS)
#define DEF_RMS_THRESH			((DEF_RMS_LSB_TH_SELF) * (DEF_RMS_LSB_TH_SELF))
#define DEF_SCALE_FOR_FLOAT		(1000)
#define DEF_RMS_SCALE_FOR_RMS		(10000)
#define DEF_SQRT_SCALE_FOR_RMS		(100)
#define GYRO_LIB_DL_FAIL		(9990)

#define DEF_GYRO_SENS_STM		(70) /* 0.07 * 1000 */
#define DEF_BIAS_LSB_THRESH_SELF_STM	(40000 / DEF_GYRO_SENS_STM)

/* Bosch */
#define SELFTEST_LIMITATION_OF_ERROR	(5250)

#ifndef ABS
#define ABS(a) ((a) > 0 ? (a) : -(a))
#endif

static ssize_t gyro_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->acc_type == INV_ID)
		return sprintf(buf, "%s\n", VENDOR_INV);
	else if (data->acc_type == STM_ID)
		return sprintf(buf, "%s\n", VENDOR_STM);
	else
		return sprintf(buf, "%s\n", VENDOR_BOSCH);
}

static ssize_t gyro_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->acc_type == INV_ID)
		return sprintf(buf, "%s\n", CHIP_ID_INV);
	else if (data->acc_type == STM_ID)
		return sprintf(buf, "%s\n", CHIP_ID_STM);
	else
		return sprintf(buf, "%s\n", CHIP_ID_BOSCH);
}

int gyro_open_calibration(struct ssp_data *data)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
				O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);

		data->gyrocal.x = 0;
		data->gyrocal.y = 0;
		data->gyrocal.z = 0;

		return ret;
	}

	ret = cal_filp->f_op->read(cal_filp, (char *)&data->gyrocal,
		sizeof(data->gyrocal), &cal_filp->f_pos);
	if (ret != sizeof(data->gyrocal))
		ret = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	ssp_info("open gyro calibration %d, %d, %d",
		data->gyrocal.x, data->gyrocal.y, data->gyrocal.z);
	return ret;
}

int save_gyro_caldata(struct ssp_data *data, s16 *cal_data)
{
	int ret = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	data->gyrocal.x = cal_data[0];
	data->gyrocal.y = cal_data[1];
	data->gyrocal.z = cal_data[2];

	ssp_info("do gyro calibrate %d, %d, %d",
		data->gyrocal.x, data->gyrocal.y, data->gyrocal.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW |
			O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open calibration file\n", __func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return -EIO;
	}

	ret = cal_filp->f_op->write(cal_filp, (char *)&data->gyrocal,
		sizeof(data->gyrocal), &cal_filp->f_pos);
	if (ret != sizeof(data->gyrocal)) {
		pr_err("[SSP]: %s - Can't write gyro cal to file\n", __func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int set_gyro_cal(struct ssp_data *data)
{
	int ret = 0;
	struct ssp_msg *msg;
	s16 gyro_cal[3];
	if (!(data->uSensorState & (1 << GYROSCOPE_SENSOR))) {
		pr_info("[SSP]: %s - Skip this function!!!"\
			", gyro sensor is not connected(0x%x)\n",
			__func__, data->uSensorState);
		return ret;
	}

	gyro_cal[0] = data->gyrocal.x;
	gyro_cal[1] = data->gyrocal.y;
	gyro_cal[2] = data->gyrocal.z;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_MCU_SET_GYRO_CAL;
	msg->length = 6;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(6, GFP_KERNEL);

	msg->free_buffer = 1;
	memcpy(msg->buffer, gyro_cal, 6);

	ret = ssp_spi_async(data, msg);

	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, ret);
		ret = ERROR;
	}

	pr_info("[SSP] Set gyro cal data %d, %d, %d\n",
		gyro_cal[0], gyro_cal[1], gyro_cal[2]);
	return ret;
}

static ssize_t gyro_power_off(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssp_infof();

	return sprintf(buf, "%d\n", 1);
}

static ssize_t gyro_power_on(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssp_infof();

	return sprintf(buf, "%d\n", 1);
}

short do_gyro_get_temp(struct ssp_data *data)
{
	char temp[2] = { 0};
	unsigned char reg[2];
	short temperature = 0;
	int ret = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = GYROSCOPE_TEMP_FACTORY;
	msg->length = 2;
	msg->options = AP2HUB_READ;
	msg->buffer = temp;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 3000);

	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Temp Timeout!!\n", __func__);
		goto exit;
	}

	reg[0] = temp[1];
	reg[1] = temp[0];
	temperature = (short) (((reg[0]) << 8) | reg[1]);
	ssp_infof("%d", temperature);

exit:
	return temperature;
}

static ssize_t gyro_temp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", do_gyro_get_temp(data));
}

u32 selftest_sqrt(u32 sqsum)
{
	u32 sq_rt;
	u32 g0, g1, g2, g3, g4;
	u32 seed;
	u32 next;
	u32 step;

	g4 = sqsum / 100000000;
	g3 = (sqsum - g4 * 100000000) / 1000000;
	g2 = (sqsum - g4 * 100000000 - g3 * 1000000) / 10000;
	g1 = (sqsum - g4 * 100000000 - g3 * 1000000 - g2 * 10000) / 100;
	g0 = (sqsum - g4 * 100000000 - g3 * 1000000 - g2 * 10000 - g1 * 100);

	next = g4;
	step = 0;
	seed = 0;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = seed * 10000;
	next = (next - (seed * step)) * 100 + g3;

	step = 0;
	seed = 2 * seed * 10;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step * 1000;
	next = (next - seed * step) * 100 + g2;
	seed = (seed + step) * 10;
	step = 0;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step * 100;
	next = (next - seed * step) * 100 + g1;
	seed = (seed + step) * 10;
	step = 0;

	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step * 10;
	next = (next - seed * step) * 100 + g0;
	seed = (seed + step) * 10;
	step = 0;

	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step;

	return sq_rt;
}

static ssize_t k6ds3tr_gyro_selftest(char *buf, struct ssp_data *data)
{
	char chTempBuf[36] = { 0,};
	u8 initialized = 0;
	s8 hw_result = 0;
	int i = 0, j = 0, total_count = 0, ret_val = 0, gyro_lib_dl_fail = 0;
	long avg[3] = {0,}, rms[3] = {0,};
	int gyro_bias[3] = {0,}, gyro_rms[3] = {0,};
	s16 shift_ratio[3] = {0,}; /* self_diff value */
	s16 cal_data[3] = {0,};
	char a_name[3][2] = { "X", "Y", "Z" };
	int ret = 0;
	int dps_rms[3] = { 0, };
	u32 temp = 0;
	int bias_thresh = DEF_BIAS_LSB_THRESH_SELF_STM;
	int fifo_ret = 0;
	int cal_ret = 0;
	s16 st_zro[3] = {0, };
	s16 st_bias[3] = {0, };
	int gyro_fifo_avg[3] = {0,}, gyro_self_zro[3] = {0,};
	int gyro_self_bias[3] = {0,}, gyro_self_diff[3] = {0,};

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n",
			__func__);
		goto exit;
	}
	msg->cmd = GYROSCOPE_FACTORY;
	msg->length = 36;
	msg->options = AP2HUB_READ;
	msg->buffer = chTempBuf;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 7000);

	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Selftest Timeout!!\n", __func__);
		ret_val = 1;
		goto exit;
	}

	data->uTimeOutCnt = 0;

	pr_err("[SSP]%d %d %d %d %d %d %d %d %d %d %d %d\n", chTempBuf[0],
		chTempBuf[1], chTempBuf[2], chTempBuf[3], chTempBuf[4],
		chTempBuf[5], chTempBuf[6], chTempBuf[7], chTempBuf[8],
		chTempBuf[9], chTempBuf[10], chTempBuf[11]);

	initialized = chTempBuf[0];
	shift_ratio[0] = (s16)((chTempBuf[2] << 8) +
				chTempBuf[1]);
	shift_ratio[1] = (s16)((chTempBuf[4] << 8) +
				chTempBuf[3]);
	shift_ratio[2] = (s16)((chTempBuf[6] << 8) +
				chTempBuf[5]);
	hw_result = (s8)chTempBuf[7];
	total_count = (int)((chTempBuf[11] << 24) +
				(chTempBuf[10] << 16) +
				(chTempBuf[9] << 8) +
				chTempBuf[8]);
	avg[0] = (long)((chTempBuf[15] << 24) +
				(chTempBuf[14] << 16) +
				(chTempBuf[13] << 8) +
				chTempBuf[12]);
	avg[1] = (long)((chTempBuf[19] << 24) +
				(chTempBuf[18] << 16) +
				(chTempBuf[17] << 8) +
				chTempBuf[16]);
	avg[2] = (long)((chTempBuf[23] << 24) +
				(chTempBuf[22] << 16) +
				(chTempBuf[21] << 8) +
				chTempBuf[20]);
	rms[0] = (long)((chTempBuf[27] << 24) +
				(chTempBuf[26] << 16) +
				(chTempBuf[25] << 8) +
				chTempBuf[24]);
	rms[1] = (long)((chTempBuf[31] << 24) +
				(chTempBuf[30] << 16) +
				(chTempBuf[29] << 8) +
				chTempBuf[28]);
	rms[2] = (long)((chTempBuf[35] << 24) +
				(chTempBuf[34] << 16) +
				(chTempBuf[33] << 8) +
				chTempBuf[32]);

	st_zro[0] = (s16)((chTempBuf[25] << 8) +
				chTempBuf[24]);
	st_zro[1] = (s16)((chTempBuf[27] << 8) +
				chTempBuf[26]);
	st_zro[2] = (s16)((chTempBuf[29] << 8) +
				chTempBuf[28]);

	st_bias[0] = (s16)((chTempBuf[31] << 8) +
				chTempBuf[30]);
	st_bias[1] = (s16)((chTempBuf[33] << 8) +
				chTempBuf[32]);
	st_bias[2] = (s16)((chTempBuf[35] << 8) +
				chTempBuf[34]);

	pr_info("[SSP] init: %d, total cnt: %d\n", initialized, total_count);
	pr_info("[SSP] hw_result: %d, %d, %d, %d\n", hw_result,
		shift_ratio[0], shift_ratio[1],	shift_ratio[2]);
	pr_info("[SSP] avg %+8ld %+8ld %+8ld (LSB)\n", avg[0], avg[1], avg[2]);
	pr_info("[SSP] rms %+8ld %+8ld %+8ld (LSB)\n", rms[0], rms[1], rms[2]);

	/* FIFO ZRO check pass / fail */
	gyro_fifo_avg[0] = avg[0] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_fifo_avg[1] = avg[1] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_fifo_avg[2] = avg[2] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	/* ZRO self test */
	gyro_self_zro[0] = st_zro[0] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_zro[1] = st_zro[1] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_zro[2] = st_zro[2] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	/* bias */
	gyro_self_bias[0]
		= st_bias[0] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_bias[1]
		= st_bias[1] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_bias[2]
		= st_bias[2] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	/* diff = bias - ZRO */
	gyro_self_diff[0]
		= shift_ratio[0] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_diff[1]
		= shift_ratio[1] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_diff[2]
		= shift_ratio[2] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;

	if (total_count != 128) {
		pr_err("[SSP] %s, total_count is not 128. goto exit\n",
			__func__);
		ret_val = 2;
		goto exit;
	} else
		cal_ret = fifo_ret = 1;

	if (hw_result < 0) {
		pr_err("[SSP] %s - hw selftest fail(%d), sw selftest skip\n",
			__func__, hw_result);
		if (shift_ratio[0] == GYRO_LIB_DL_FAIL &&
			shift_ratio[1] == GYRO_LIB_DL_FAIL &&
			shift_ratio[2] == GYRO_LIB_DL_FAIL) {
			pr_err("[SSP] %s - gyro lib download fail\n", __func__);
			gyro_lib_dl_fail = 1;
		} else {
/*
			ssp_dbg("[SSP]: %s - %d,%d,%d fail.\n",
				__func__,
				shift_ratio[0] / 10,
				shift_ratio[1] / 10,
				shift_ratio[2] / 10);
			return sprintf(buf, "%d,%d,%d\n",
				shift_ratio[0] / 10,
				shift_ratio[1] / 10,
				shift_ratio[2] / 10);
*/
			ssp_dbg("[SSP]: %s - %d,%d,%d fail.\n",
				__func__, gyro_self_diff[0], gyro_self_diff[1],
				gyro_self_diff[2]);
			return sprintf(buf, "%d,%d,%d\n",
				gyro_self_diff[0], gyro_self_diff[1],
				gyro_self_diff[2]);
		}
	}

	/* AVG value range test +/- 40 */
	if ((ABS(gyro_fifo_avg[0]) > 40) || (ABS(gyro_fifo_avg[1]) > 40) ||
		(ABS(gyro_fifo_avg[2]) > 40)) {
		ssp_dbg("[SSP]: %s - %d,%d,%d fail.\n",	__func__,
			gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2]);
		return sprintf(buf, "%d,%d,%d\n",
			gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2]);
	}

	/* STMICRO */
	gyro_bias[0] = avg[0] * DEF_GYRO_SENS_STM;
	gyro_bias[1] = avg[1] * DEF_GYRO_SENS_STM;
	gyro_bias[2] = avg[2] * DEF_GYRO_SENS_STM;
	cal_data[0] = (s16)avg[0];
	cal_data[1] = (s16)avg[1];
	cal_data[2] = (s16)avg[2];

	if (VERBOSE_OUT) {
		pr_info("[SSP] abs bias : %+8d.%03d %+8d.%03d %+8d.%03d (dps)\n",
			(int)abs(gyro_bias[0]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[0]) % DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[1]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[1]) % DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[2]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[2]) % DEF_SCALE_FOR_FLOAT);
	}

	for (j = 0; j < 3; j++) {
		if (unlikely(abs(avg[j]) > bias_thresh)) {
			pr_err("[SSP] %s-Gyro bias (%ld) exceeded threshold "
				"(threshold = %d LSB)\n", a_name[j],
				avg[j], bias_thresh);
			ret_val |= 1 << (3 + j);
		}
	}

	/* STMICRO */
	/* 3rd, check RMS for dead gyros
	   If any of the RMS noise value returns zero,
	   then we might have dead gyro or FIFO/register failure,
	   the part is sleeping, or the part is not responsive */
	/* if (rms[0] == 0 || rms[1] == 0 || rms[2] == 0)
		ret_val |= 1 << 6; */

	if (VERBOSE_OUT) {
		pr_info("[SSP] RMS ^ 2 : %+8ld %+8ld %+8ld\n",
			(long)rms[0] / total_count,
			(long)rms[1] / total_count, (long)rms[2] / total_count);
	}

	for (i = 0; i < 3; i++) {
		if (rms[i] > 10000) {
			temp =
			    ((u32) (rms[i] / total_count)) *
			    DEF_RMS_SCALE_FOR_RMS;
		} else {
			temp =
			    ((u32) (rms[i] * DEF_RMS_SCALE_FOR_RMS)) /
			    total_count;
		}
		if (rms[i] < 0)
			temp = 1 << 31;

		dps_rms[i] = selftest_sqrt(temp) / DEF_GYRO_SENS_STM;

		gyro_rms[i] =
		    dps_rms[i] * DEF_SCALE_FOR_FLOAT / DEF_SQRT_SCALE_FOR_RMS;
	}

	pr_info("[SSP] RMS : %+8d.%03d	 %+8d.%03d  %+8d.%03d (dps)\n",
		(int)abs(gyro_rms[0]) / DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[0]) % DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[1]) / DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[1]) % DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[2]) / DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[2]) % DEF_SCALE_FOR_FLOAT);

	if (gyro_lib_dl_fail) {
		pr_err("[SSP] gyro_lib_dl_fail, Don't save cal data\n");
		ret_val = -1;
		goto exit;
	}

	if (likely(!ret_val)) {
		save_gyro_caldata(data, cal_data);
	} else {
		pr_err("[SSP] ret_val != 0, gyrocal is 0 at all axis\n");
		data->gyrocal.x = 0;
		data->gyrocal.y = 0;
		data->gyrocal.z = 0;
	}
exit:
	ssp_dbg("[SSP]: %s - "
		"%d,%d,%d,"
		"%d,%d,%d,"
		"%d,%d,%d,"
		"%d,%d,%d,%d,%d\n",
		__func__,
		gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2],
		gyro_self_zro[0], gyro_self_zro[1], gyro_self_zro[2],
		gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2],
		gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2],
		fifo_ret,
		cal_ret);

	/* Gyro Calibration pass / fail, buffer 1~6 values. */
	if ((fifo_ret == 0) || (cal_ret == 0))
		return sprintf(buf, "%d,%d,%d\n",
			gyro_self_diff[0], gyro_self_diff[1],
			gyro_self_diff[2]);

	return sprintf(buf,
		"%d,%d,%d,"
		"%d,%d,%d,"
		"%d,%d,%d,"
		"%d,%d,%d,%d,%d\n",
		gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2],
		gyro_self_zro[0], gyro_self_zro[1], gyro_self_zro[2],
		gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2],
		gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2],
		fifo_ret,
		cal_ret);
}

ssize_t mpu6500_gyro_selftest(char *buf, struct ssp_data *data)
{
	char chTempBuf[36] = { 0,};
	u8 initialized = 0;
	s8 hw_result = 0;
	int i = 0, j = 0, total_count = 0, ret_val = 0, gyro_lib_dl_fail = 0;
	long avg[3] = {0,}, rms[3] = {0,};
	int gyro_bias[3] = {0,}, gyro_rms[3] = {0,};
	s16 shift_ratio[3] = {0,};
	s16 cal_data[3] = {0,};
	char a_name[3][2] = { "X", "Y", "Z" };
	int ret = 0;
	int dps_rms[3] = { 0, };
	u32 temp = 0;
	int bias_thresh = DEF_BIAS_LSB_THRESH_SELF_6500;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = GYROSCOPE_FACTORY;
	msg->length = 36;
	msg->options = AP2HUB_READ;
	msg->buffer = chTempBuf;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 7000);

	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Selftest Timeout!!\n", __func__);
		ret_val = 1;
		goto exit;
	}

	data->uTimeOutCnt = 0;
	pr_err("[SSP]%d %d %d %d %d %d %d %d %d %d %d %d",
		chTempBuf[0], chTempBuf[1], chTempBuf[2], chTempBuf[3],
		chTempBuf[4], chTempBuf[5], chTempBuf[6], chTempBuf[7],
		chTempBuf[8], chTempBuf[9], chTempBuf[10], chTempBuf[11]);

	initialized = chTempBuf[0];
	shift_ratio[0] = (s16)((chTempBuf[2] << 8) +
				chTempBuf[1]);
	shift_ratio[1] = (s16)((chTempBuf[4] << 8) +
				chTempBuf[3]);
	shift_ratio[2] = (s16)((chTempBuf[6] << 8) +
				chTempBuf[5]);
	hw_result = (s8)chTempBuf[7];
	total_count = (int)((chTempBuf[11] << 24) +
				(chTempBuf[10] << 16) +
				(chTempBuf[9] << 8) +
				chTempBuf[8]);
	avg[0] = (long)((chTempBuf[15] << 24) +
				(chTempBuf[14] << 16) +
				(chTempBuf[13] << 8) +
				chTempBuf[12]);
	avg[1] = (long)((chTempBuf[19] << 24) +
				(chTempBuf[18] << 16) +
				(chTempBuf[17] << 8) +
				chTempBuf[16]);
	avg[2] = (long)((chTempBuf[23] << 24) +
				(chTempBuf[22] << 16) +
				(chTempBuf[21] << 8) +
				chTempBuf[20]);
	rms[0] = (long)((chTempBuf[27] << 24) +
				(chTempBuf[26] << 16) +
				(chTempBuf[25] << 8) +
				chTempBuf[24]);
	rms[1] = (long)((chTempBuf[31] << 24) +
				(chTempBuf[30] << 16) +
				(chTempBuf[29] << 8) +
				chTempBuf[28]);
	rms[2] = (long)((chTempBuf[35] << 24) +
				(chTempBuf[34] << 16) +
				(chTempBuf[33] << 8) +
				chTempBuf[32]);
	pr_info("[SSP] init: %d, total cnt: %d\n", initialized, total_count);
	pr_info("[SSP] hw_result: %d, %d, %d, %d\n", hw_result,
		shift_ratio[0], shift_ratio[1],	shift_ratio[2]);
	pr_info("[SSP] avg %+8ld %+8ld %+8ld (LSB)\n", avg[0], avg[1], avg[2]);
	pr_info("[SSP] rms %+8ld %+8ld %+8ld (LSB)\n", rms[0], rms[1], rms[2]);

	if (total_count == 0) {
		pr_err("[SSP] %s, total_count is 0. goto exit\n", __func__);
		ret_val = 2;
		goto exit;
	}

	if (hw_result < 0) {
		pr_err("[SSP] %s - hw selftest fail(%d), sw selftest skip\n",
			__func__, hw_result);
		if (shift_ratio[0] == GYRO_LIB_DL_FAIL &&
			shift_ratio[1] == GYRO_LIB_DL_FAIL &&
			shift_ratio[2] == GYRO_LIB_DL_FAIL) {
			pr_err("[SSP] %s - gyro lib download fail\n", __func__);
			gyro_lib_dl_fail = 1;
		} else {
			return sprintf(buf, "-1,0,0,0,0,0,0,%d.%d,%d.%d,%d.%d,0,0,0\n",
				shift_ratio[0] / 10, shift_ratio[0] % 10,
				shift_ratio[1] / 10, shift_ratio[1] % 10,
				shift_ratio[2] / 10, shift_ratio[2] % 10);
		}
	}
	gyro_bias[0] = (avg[0] * DEF_SCALE_FOR_FLOAT) / DEF_GYRO_SENS;
	gyro_bias[1] = (avg[1] * DEF_SCALE_FOR_FLOAT) / DEF_GYRO_SENS;
	gyro_bias[2] = (avg[2] * DEF_SCALE_FOR_FLOAT) / DEF_GYRO_SENS;
	cal_data[0] = (s16)avg[0];
	cal_data[1] = (s16)avg[1];
	cal_data[2] = (s16)avg[2];

	if (VERBOSE_OUT) {
		pr_info("[SSP] abs bias : %+8d.%03d %+8d.%03d %+8d.%03d (dps)\n",
			(int)abs(gyro_bias[0]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[0]) % DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[1]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[1]) % DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[2]) / DEF_SCALE_FOR_FLOAT,
			(int)abs(gyro_bias[2]) % DEF_SCALE_FOR_FLOAT);
	}

	for (j = 0; j < 3; j++) {
		if (unlikely(abs(avg[j]) > bias_thresh)) {
			pr_err("[SSP] %s-Gyro bias (%ld) exceeded threshold "
				"(threshold = %d LSB)\n", a_name[j],
				avg[j], bias_thresh);
			ret_val |= 1 << (3 + j);
		}
	}
	/* 3rd, check RMS for dead gyros
	   If any of the RMS noise value returns zero,
	   then we might have dead gyro or FIFO/register failure,
	   the part is sleeping, or the part is not responsive */
	if (rms[0] == 0 || rms[1] == 0 || rms[2] == 0)
		ret_val |= 1 << 6;

	if (VERBOSE_OUT) {
		pr_info("[SSP] RMS ^ 2 : %+8ld %+8ld %+8ld\n",
			(long)rms[0] / total_count,
			(long)rms[1] / total_count, (long)rms[2] / total_count);
	}

	for (j = 0; j < 3; j++) {
		if (unlikely(rms[j] / total_count > DEF_RMS_THRESH)) {
			pr_err("[SSP] %s-Gyro rms (%ld) exceeded threshold "
				"(threshold = %d LSB)\n", a_name[j],
				rms[j] / total_count, DEF_RMS_THRESH);
			ret_val |= 1 << (7 + j);
		}
	}

	for (i = 0; i < 3; i++) {
		if (rms[i] > 10000) {
			temp =
			    ((u32) (rms[i] / total_count)) *
			    DEF_RMS_SCALE_FOR_RMS;
		} else {
			temp =
			    ((u32) (rms[i] * DEF_RMS_SCALE_FOR_RMS)) /
			    total_count;
		}
		if (rms[i] < 0)
			temp = 1 << 31;

		dps_rms[i] = selftest_sqrt(temp) / DEF_GYRO_SENS;

		gyro_rms[i] =
		    dps_rms[i] * DEF_SCALE_FOR_FLOAT / DEF_SQRT_SCALE_FOR_RMS;
	}

	pr_info("[SSP] RMS : %+8d.%03d	 %+8d.%03d  %+8d.%03d (dps)\n",
		(int)abs(gyro_rms[0]) / DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[0]) % DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[1]) / DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[1]) % DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[2]) / DEF_SCALE_FOR_FLOAT,
		(int)abs(gyro_rms[2]) % DEF_SCALE_FOR_FLOAT);

	if (gyro_lib_dl_fail) {
		pr_err("[SSP] gyro_lib_dl_fail, Don't save cal data\n");
		ret_val = -1;
		goto exit;
	}

	if (likely(!ret_val)) {
		save_gyro_caldata(data, cal_data);
	} else {
		pr_err("[SSP] ret_val != 0, gyrocal is 0 at all axis\n");
		data->gyrocal.x = 0;
		data->gyrocal.y = 0;
		data->gyrocal.z = 0;
	}

exit:
	ssp_infof("%d,"
		"%d.%03d,%d.%03d,%d.%03d,"
		"%d.%03d,%d.%03d,%d.%03d,"
		"%d.%d,%d.%d,%d.%d,"
		"%d,%d,%d\n",
		ret_val,
		(int)abs(gyro_bias[0]/1000),
		(int)abs(gyro_bias[0])%1000,
		(int)abs(gyro_bias[1]/1000),
		(int)abs(gyro_bias[1])%1000,
		(int)abs(gyro_bias[2]/1000),
		(int)abs(gyro_bias[2])%1000,
		gyro_rms[0]/1000,
		(int)abs(gyro_rms[0])%1000,
		gyro_rms[1]/1000,
		(int)abs(gyro_rms[1])%1000,
		gyro_rms[2]/1000,
		(int)abs(gyro_rms[2])%1000,
		shift_ratio[0] / 10, shift_ratio[0] % 10,
		shift_ratio[1] / 10, shift_ratio[1] % 10,
		shift_ratio[2] / 10, shift_ratio[2] % 10,
		(int)(total_count/3),
		(int)(total_count/3),
		(int)(total_count/3));

	return sprintf(buf, "%d,"
		"%d.%03d,%d.%03d,%d.%03d,"
		"%d.%03d,%d.%03d,%d.%03d,"
		"%d.%d,%d.%d,%d.%d,"
		"%d,%d,%d\n",
		ret_val,
		(int)abs(gyro_bias[0]/1000),
		(int)abs(gyro_bias[0])%1000,
		(int)abs(gyro_bias[1]/1000),
		(int)abs(gyro_bias[1])%1000,
		(int)abs(gyro_bias[2]/1000),
		(int)abs(gyro_bias[2])%1000,
		gyro_rms[0]/1000,
		(int)abs(gyro_rms[0])%1000,
		gyro_rms[1]/1000,
		(int)abs(gyro_rms[1])%1000,
		gyro_rms[2]/1000,
		(int)abs(gyro_rms[2])%1000,
		shift_ratio[0] / 10, shift_ratio[0] % 10,
		shift_ratio[1] / 10, shift_ratio[1] % 10,
		shift_ratio[2] / 10, shift_ratio[2] % 10,
		(int)(total_count/3),
		(int)(total_count/3),
		(int)(total_count/3));
}

ssize_t bmi168_gyro_selftest(char *buf, struct ssp_data *data)
{
	char chTempBuf[19] = {0, };
	u8 bist = 0, selftest = 0;
	int datax_check = 0;
	int datay_check = 0;
	int dataz_check = 0;
	s16 cal_data[3] = {0, };
	int ret = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->cmd = GYROSCOPE_FACTORY;
	msg->length = 19;
	msg->options = AP2HUB_READ;
	msg->buffer = chTempBuf;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 3000);
	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Selftest Timeout!!\n", __func__);
		selftest = 1;
		goto exit;
	}

	pr_info("[SSP]: %s - %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		__func__, chTempBuf[0], chTempBuf[1], chTempBuf[2],
		chTempBuf[3], chTempBuf[4], chTempBuf[5], chTempBuf[6],
		chTempBuf[7], chTempBuf[8], chTempBuf[9], chTempBuf[10],
		chTempBuf[11], chTempBuf[12]);

	data->uTimeOutCnt = 0;

	/* 1: X axis fail, 2: X axis fail, 4: X axis fail, 8: Bist fail*/
	selftest = chTempBuf[0];
	if (selftest == 0)
		bist = 1;
	else
		bist = 0;

	datax_check = (int)((chTempBuf[4] << 24) + (chTempBuf[3] << 16)
		+(chTempBuf[2] << 8) + chTempBuf[1]);
	datay_check = (int)((chTempBuf[8] << 24) + (chTempBuf[7] << 16)
		+(chTempBuf[6] << 8) + chTempBuf[5]);
	dataz_check = (int)((chTempBuf[12] << 24) + (chTempBuf[11] << 16)
		+(chTempBuf[10] << 8) + chTempBuf[9]);

	cal_data[0] = (s16)((chTempBuf[14] << 8) + chTempBuf[13]);
	cal_data[1] = (s16)((chTempBuf[16] << 8) + chTempBuf[15]);
	cal_data[2] = (s16)((chTempBuf[18] << 8) + chTempBuf[17]);

	pr_info("[SSP]: %s - bist: %d, selftest: %d\n",
		__func__, bist, selftest);
	pr_info("[SSP]: %s - X: %d, Y: %d, Z: %d\n",
		__func__, datax_check, datay_check, dataz_check);
	pr_info("[SSP]: %s - CalData X: %d, Y: %d, Z: %d\n",
		__func__, cal_data[0], cal_data[1], cal_data[2]);

	if ((datax_check <= SELFTEST_LIMITATION_OF_ERROR)
		&& (datay_check <= SELFTEST_LIMITATION_OF_ERROR)
		&& (dataz_check <= SELFTEST_LIMITATION_OF_ERROR)) {
		pr_info("[SSP]: %s - Gyro zero rate OK!- Gyro selftest Pass\n",
			__func__);
		save_gyro_caldata(data, cal_data);
	} else {
		pr_info("[SSP]: %s - Gyro zero rate NG!- Gyro selftest fail!\n",
			__func__);
		selftest |= 1;
	}
exit:
	pr_info("[SSP] %s - %d,%d,%d.%03d,%d.%03d,%d.%03d\n", __func__,
			selftest ? 0 : 1, bist,
			(datax_check / 1000), (int)abs(datax_check % 1000),
			(datay_check / 1000), (int)abs(datay_check % 1000),
			(dataz_check / 1000), (int)abs(dataz_check % 1000));

	return sprintf(buf, "%d,%d,%d.%03d,%d.%03d,%d.%03d,"\
			"%d,%d,%d,%d,%d,%d,%d,%d" "\n",
			selftest ? 0 : 1, bist,
			(datax_check / 1000), (int)abs(datax_check % 1000),
			(datay_check / 1000), (int)abs(datay_check % 1000),
			(dataz_check / 1000), (int)abs(dataz_check % 1000),
			ret, ret, ret, ret, ret, ret, ret, ret);
}

static ssize_t gyro_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->acc_type == INV_ID)
		return mpu6500_gyro_selftest(buf, data);
	else if (data->acc_type == STM_ID)
		return k6ds3tr_gyro_selftest(buf, data);
	else
		return bmi168_gyro_selftest(buf, data);
}

static ssize_t gyro_selftest_dps_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int new_dps = 0;
	int ret = 0;
	char chTempBuf = 0;

	struct ssp_data *data = dev_get_drvdata(dev);

	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << GYROSCOPE_SENSOR)))
		goto exit;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = GYROSCOPE_DPS_FACTORY;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = &chTempBuf;
	msg->free_buffer = 0;

	sscanf(buf, "%d", &new_dps);

	if (new_dps == GYROSCOPE_DPS250)
		msg->options |= 0 << SSP_GYRO_DPS;
	else if (new_dps == GYROSCOPE_DPS500)
		msg->options |= 1 << SSP_GYRO_DPS;
	else if (new_dps == GYROSCOPE_DPS2000)
		msg->options |= 2 << SSP_GYRO_DPS;
	else {
		msg->options |= 1 << SSP_GYRO_DPS;
		new_dps = GYROSCOPE_DPS500;
	}

	ret = ssp_spi_sync(data, msg, 3000);

	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Selftest DPS Timeout!!\n", __func__);
		goto exit;
	}

	if (chTempBuf != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Selftest DPS Error!!\n", __func__);
		goto exit;
	}

	data->buf[GYROSCOPE_SENSOR].gyro_dps = (unsigned int)new_dps;
	pr_err("[SSP]: %s - %u dps stored\n", __func__,
			data->buf[GYROSCOPE_SENSOR].gyro_dps);
exit:
	return count;
}

static ssize_t gyro_selftest_dps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->buf[GYROSCOPE_SENSOR].gyro_dps);
}

static DEVICE_ATTR(name, S_IRUGO, gyro_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, gyro_vendor_show, NULL);
static DEVICE_ATTR(power_off, S_IRUGO, gyro_power_off, NULL);
static DEVICE_ATTR(power_on, S_IRUGO, gyro_power_on, NULL);
static DEVICE_ATTR(temperature, S_IRUGO, gyro_temp_show, NULL);
static DEVICE_ATTR(selftest, S_IRUGO, gyro_selftest_show, NULL);
static DEVICE_ATTR(selftest_dps, S_IRUGO | S_IWUSR | S_IWGRP,
	gyro_selftest_dps_show, gyro_selftest_dps_store);

static struct device_attribute *gyro_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	&dev_attr_power_on,
	&dev_attr_power_off,
	&dev_attr_temperature,
	&dev_attr_selftest_dps,
	NULL,
};

void initialize_gyro_factorytest(struct ssp_data *data)
{
	sensors_register(data->devices[GYROSCOPE_SENSOR], data, gyro_attrs,
			data->name[GYROSCOPE_SENSOR]);
}

void remove_gyro_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->devices[GYROSCOPE_SENSOR], gyro_attrs);
}
