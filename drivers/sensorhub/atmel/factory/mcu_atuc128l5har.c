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

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define MODEL_NAME			"SAMG53G19-UUR-101"

ssize_t mcu_revision_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "AT01%u,AT01%u\n", get_module_rev(data),
		data->uCurFirmRev);
}

ssize_t mcu_model_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MODEL_NAME);
}

ssize_t mcu_update_kernel_bin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - mcu binany update!\n", __func__);

	iRet = forced_to_download_binary(data, UMS_BINARY);
	if (iRet == SUCCESS) {
		bSuccess = true;
		goto out;
	}

	iRet = forced_to_download_binary(data, KERNEL_BINARY);
	if (iRet == SUCCESS)
		bSuccess = true;
	else
		bSuccess = false;
out:
	return sprintf(buf, "%s\n", (bSuccess ? "OK" : "NG"));
}

ssize_t mcu_update_kernel_crashed_bin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - mcu binany update!\n", __func__);
/*
	iRet = forced_to_download_binary(data, UMS_BINARY);
	if (iRet == SUCCESS) {
		bSuccess = true;
		goto out;
	}

	iRet = forced_to_download_binary(data, KERNEL_CRASHED_BINARY);
	if (iRet == SUCCESS)
		bSuccess = true;
	else
		bSuccess = false;
*/

	iRet = forced_to_download_binary(data, KERNEL_CRASHED_BINARY);
	if (iRet == SUCCESS) {
		bSuccess = true;
		goto out;
	}

	iRet = forced_to_download_binary(data, KERNEL_BINARY);
	if (iRet == SUCCESS)
		bSuccess = true;
	else
		bSuccess = false;
out:
	return sprintf(buf, "%s\n", (bSuccess ? "OK" : "NG"));
}

ssize_t mcu_update_ums_bin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - mcu binany update!\n", __func__);

	iRet = forced_to_download_binary(data, UMS_BINARY);
	if (iRet == SUCCESS)
		bSuccess = true;
	else
		bSuccess = false;

	return sprintf(buf, "%s\n", (bSuccess ? "OK" : "NG"));
}

ssize_t mcu_reset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	reset_mcu(data);

	return sprintf(buf, "OK\n");
}

ssize_t mcu_dump_show(struct device *dev, struct device_attribute *attr,
		char *buf) {
	struct ssp_data *data = dev_get_drvdata(dev);
	int status = 1, iDelaycnt = 0;

	data->bDumping = true;
	set_big_data_start(data, BIG_TYPE_DUMP, 0);
	msleep(300);
	while (data->bDumping) {
		mdelay(10);
		if (iDelaycnt++ > 1000) {
			status = 0;
			break;
		}
	}
	return sprintf(buf, "%s\n", status ? "OK" : "NG");
}

static char buffer[FACTORY_DATA_MAX];

ssize_t mcu_factorytest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;

	if (sysfs_streq(buf, "1")) {
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MCU_FACTORY;
		msg->length = 5;
		msg->options = AP2HUB_READ;
		msg->buffer = buffer;
		msg->free_buffer = 0;

		memset(msg->buffer, 0, 5);

		iRet = ssp_spi_async(data, msg);

	} else {
		pr_err("[SSP]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ssp_dbg("[SSP]: MCU Factory Test Start! - %d\n", iRet);

	return size;
}

ssize_t mcu_factorytest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bMcuTestSuccessed = false;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->bSspShutdown == true) {
		ssp_dbg("[SSP]: %s - MCU Bin is crashed\n", __func__);
		return sprintf(buf, "NG,NG,NG\n");
	}

	ssp_dbg("[SSP] MCU Factory Test Data : %u, %u, %u, %u, %u\n", buffer[0],
			buffer[1], buffer[2], buffer[3], buffer[4]);

		/* system clock, RTC, I2C Master, I2C Slave, externel pin */
	if ((buffer[0] == SUCCESS)
			&& (buffer[1] == SUCCESS)
			&& (buffer[2] == SUCCESS)
			&& (buffer[3] == SUCCESS)
			&& (buffer[4] == SUCCESS))
		bMcuTestSuccessed = true;

	ssp_dbg("[SSP]: MCU Factory Test Result - %s, %s, %s\n", MODEL_NAME,
		(bMcuTestSuccessed ? "OK" : "NG"), "OK");

	return sprintf(buf, "%s,%s,%s\n", MODEL_NAME,
		(bMcuTestSuccessed ? "OK" : "NG"), "OK");
}

ssize_t mcu_sleep_factorytest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;

	if (sysfs_streq(buf, "1")) {
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MCU_SLEEP_FACTORY;
		msg->length = FACTORY_DATA_MAX;
		msg->options = AP2HUB_READ;
		msg->buffer = buffer;
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);

	} else {
		pr_err("[SSP]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ssp_dbg("[SSP]: MCU Sleep Factory Test Start! - %d\n", 1);

	return size;
}

ssize_t mcu_sleep_factorytest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iDataIdx, iSensorData = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct sensor_value fsb[SENSOR_MAX];
	u16 chLength = 0;

	memcpy(&chLength, buffer, 2);
	memset(fsb, 0, sizeof(struct sensor_value) * SENSOR_MAX);

	for (iDataIdx = 2; iDataIdx < chLength + 2;) {
		iSensorData = (int)buffer[iDataIdx++];

		if ((iSensorData < 0) ||
			(iSensorData >= (SENSOR_MAX - 1))) {
			pr_err("[SSP]: %s - Mcu data frame error %d\n",
				__func__, iSensorData);
			goto exit;
		}

		data->get_sensor_data[iSensorData]((char *)buffer,
			&iDataIdx, &(fsb[iSensorData]));
	}

	fsb[PRESSURE_SENSOR].pressure[0] -= data->iPressureCal;

exit:
	ssp_dbg("[SSP]: %s Result\n"
		"[SSP]: accel %d,%d,%d\n"
		"[SSP]: gyro %d,%d,%d\n"
		"[SSP]: mag %d,%d,%d\n"
		"[SSP]: baro %d,%d\n"
		"[SSP]: ges %d,%d,%d,%d\n"
		"[SSP]: prox %u,%u\n"
		"[SSP]: temp %d,%d,%d\n"
#if defined(CONFIG_SENSORS_SSP_TMG399X) || defined(CONFIG_SENSORS_SSP_MAX88921)
		"[SSP]: light %u,%u,%u,%u,%u,%u\n", __func__,
#else
		"[SSP]: light %u,%u,%u,%u\n", __func__,
#endif
		fsb[ACCELEROMETER_SENSOR].x, fsb[ACCELEROMETER_SENSOR].y,
		fsb[ACCELEROMETER_SENSOR].z, fsb[GYROSCOPE_SENSOR].x,
		fsb[GYROSCOPE_SENSOR].y, fsb[GYROSCOPE_SENSOR].z,
		fsb[GEOMAGNETIC_SENSOR].cal_x, fsb[GEOMAGNETIC_SENSOR].cal_y,
		fsb[GEOMAGNETIC_SENSOR].cal_z, fsb[PRESSURE_SENSOR].pressure[0],
		fsb[PRESSURE_SENSOR].pressure[1],
		fsb[GESTURE_SENSOR].data[0], fsb[GESTURE_SENSOR].data[1],
		fsb[GESTURE_SENSOR].data[2], fsb[GESTURE_SENSOR].data[3],
		fsb[PROXIMITY_SENSOR].prox[0], fsb[PROXIMITY_SENSOR].prox[1],
		fsb[TEMPERATURE_HUMIDITY_SENSOR].x,
		fsb[TEMPERATURE_HUMIDITY_SENSOR].y,
		fsb[TEMPERATURE_HUMIDITY_SENSOR].z,
		fsb[LIGHT_SENSOR].r, fsb[LIGHT_SENSOR].g, fsb[LIGHT_SENSOR].b,
		fsb[LIGHT_SENSOR].w
#if defined(CONFIG_SENSORS_SSP_TMG399X)
		,fsb[LIGHT_SENSOR].a_time, fsb[LIGHT_SENSOR].a_gain
#elif defined(CONFIG_SENSORS_SSP_MAX88921)
		,fsb[LIGHT_SENSOR].ir_cmp, fsb[LIGHT_SENSOR].amb_pga
#endif
		);

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,"
#if defined(CONFIG_SENSORS_SSP_TMG399X) || defined(CONFIG_SENSORS_SSP_MAX88921)
		"%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d\n",
#else
		"%u,%u,%u,%u,%d,%d,%d,%d,%d,%d\n",
#endif
		fsb[ACCELEROMETER_SENSOR].x, fsb[ACCELEROMETER_SENSOR].y,
		fsb[ACCELEROMETER_SENSOR].z, fsb[GYROSCOPE_SENSOR].x,
		fsb[GYROSCOPE_SENSOR].y, fsb[GYROSCOPE_SENSOR].z,
		fsb[GEOMAGNETIC_SENSOR].cal_x, fsb[GEOMAGNETIC_SENSOR].cal_y,
		fsb[GEOMAGNETIC_SENSOR].cal_z, fsb[PRESSURE_SENSOR].pressure[0],
		fsb[PRESSURE_SENSOR].pressure[1], fsb[PROXIMITY_SENSOR].prox[1],
		fsb[LIGHT_SENSOR].r, fsb[LIGHT_SENSOR].g, fsb[LIGHT_SENSOR].b,
		fsb[LIGHT_SENSOR].w,
#if defined(CONFIG_SENSORS_SSP_TMG399X)
		fsb[LIGHT_SENSOR].a_time, fsb[LIGHT_SENSOR].a_gain,
#elif defined(CONFIG_SENSORS_SSP_MAX88921)
		fsb[LIGHT_SENSOR].ir_cmp, fsb[LIGHT_SENSOR].amb_pga,
#endif
		fsb[GESTURE_SENSOR].data[0], fsb[GESTURE_SENSOR].data[1],
		fsb[GESTURE_SENSOR].data[2], fsb[GESTURE_SENSOR].data[3],
		fsb[TEMPERATURE_HUMIDITY_SENSOR].x,
		fsb[TEMPERATURE_HUMIDITY_SENSOR].y);
}
