/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm_qos.h>
#include <linux/device.h>

#include "fimc-is-sysfs.h"
#include "fimc-is-core.h"
#include "fimc-is-err.h"
#include "fimc-is-sec-define.h"
#ifdef CONFIG_OIS_USE
#include "fimc-is-device-ois.h"
#endif
#ifdef CONFIG_AF_HOST_CONTROL
#include "fimc-is-device-af.h"
#endif
#include "fimc-is-vender-specific.h"
/* #define FORCE_CAL_LOAD */

extern struct device *fimc_is_dev;
extern struct kset *devices_kset;
struct class *camera_class = NULL;
struct device *camera_front_dev;
struct device *camera_rear_dev;
#ifdef CONFIG_SECURE_CAMERA_USE
struct device *camera_secure_dev;
#endif
#ifdef CONFIG_OIS_USE
struct device *camera_ois_dev;
#endif
static struct fimc_is_core *sysfs_core;

extern bool crc32_fw_check;
extern bool crc32_setfile_check;
extern bool crc32_front_setfile_check;
extern bool crc32_check;
extern bool crc32_check_factory;
extern bool fw_version_crc_check;
extern bool is_latest_cam_module;
extern bool is_final_cam_module;

#ifdef CONFIG_COMPANION_USE
extern bool crc32_c1_fw_check;
extern bool crc32_c1_check;
extern bool crc32_c1_check_factory;
#endif /* CONFIG_COMPANION_USE */
extern bool fw_version_crc_check;
static struct fimc_is_from_info *pinfo = NULL;
static struct fimc_is_from_info *finfo = NULL;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
extern bool crc32_check_factory_front;
extern bool is_final_cam_module_front;
static struct fimc_is_from_info *front_finfo = NULL;
#endif

#ifdef CAMERA_SYSFS_V2
static struct fimc_is_cam_info cam_infos[CAM_INFO_MAX];
#endif

extern bool force_caldata_dump;
#ifdef CONFIG_OIS_USE
static bool check_ois_power = false;
int ois_threshold;
#endif
static bool check_module_init = false;

#ifdef CONFIG_COMPANION_FACTORY_VALIDATION
extern int comp_fac_i2c_check;
extern u16 comp_fac_valid_check;
#endif

#ifdef CONFIG_SECURE_CAMERA_USE
extern bool is_final_cam_module_iris;
extern bool is_iris_ver_read;
#endif

#ifdef CAMERA_SYSFS_V2
int fimc_is_get_cam_info(struct fimc_is_cam_info **caminfo)
{
	*caminfo = cam_infos;
	return 0;
}
#endif

static int read_from_firmware_version(int position)
{
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;
	int ret = 0;

	fimc_is_sec_get_sysfs_pinfo(&pinfo);
	fimc_is_sec_get_sysfs_finfo(&finfo);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	fimc_is_sec_get_sysfs_finfo_front(&front_finfo);
#endif

	if (((position == SENSOR_POSITION_REAR) && (!finfo->is_caldata_read))
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		|| ((position == SENSOR_POSITION_FRONT) && (!front_finfo->is_caldata_read))
#endif
	) {
		ret = fimc_is_sec_run_fw_sel(is_dev, position);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
		}
	}

#ifdef CONFIG_COMPANION_USE
	if ((position == SENSOR_POSITION_REAR) && (!finfo->is_c1_caldata_read)) {
		fimc_is_sec_concord_fw_sel(sysfs_core, is_dev);
	}
#endif

	return 0;
}

#ifdef CONFIG_OIS_USE
static bool read_ois_version(void)
{
	bool ret = true;
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	if (!specific->running_rear_camera) {
		if (!specific->ois_ver_read) {
			fimc_is_ois_gpio_on(sysfs_core);
			msleep(150);

			ret = fimc_is_ois_check_fw(sysfs_core);
			if (!specific->running_rear_camera) {
				fimc_is_ois_gpio_off(sysfs_core);
			}
		}
	}
	return ret;
}
#endif

static ssize_t camera_front_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_core *core = NULL;
	struct fimc_is_vender_specific *specific;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;

	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", specific->front_sensor_id);
}

static ssize_t camera_rear_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_core *core = NULL;
	struct fimc_is_vender_specific *specific;
	int ret = 0;
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;

	if (force_caldata_dump == false) {
		ret = fimc_is_sec_run_fw_sel(is_dev, SENSOR_POSITION_REAR);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
		}
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;
	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", specific->rear_sensor_id);
}

static DEVICE_ATTR(front_sensorid, S_IRUGO, camera_front_sensorid_show, NULL);
static DEVICE_ATTR(rear_sensorid, S_IRUGO, camera_rear_sensorid_show, NULL);

#ifdef CONFIG_SECURE_CAMERA_USE
static ssize_t camera_secure_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_core *core = NULL;
	struct fimc_is_vender_specific *specific;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	specific = core->vender.private_data;

	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", specific->secure_sensor_id);
}
static DEVICE_ATTR(secure_sensorid, S_IRUGO, camera_secure_sensorid_show, NULL);
#endif

static int fimc_is_get_sensor_data(struct device *dev, char *maker, char *name, int position)
{
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific = NULL;
	struct fimc_is_device_sensor *device;
	struct fimc_is_module_enum *module;
	int sensor_id = 0;
	int i = 0;

	if (!fimc_is_dev) {
		dev_err(dev, "%s: fimc_is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		dev_err(dev, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	specific = core->vender.private_data;

	if (position == SENSOR_POSITION_REAR) {
		sensor_id = specific->rear_sensor_id;
	} else if(position == SENSOR_POSITION_FRONT) {
		sensor_id = specific->front_sensor_id;
	}
#ifdef CONFIG_SECURE_CAMERA_USE
	else if(position == SENSOR_POSITION_SECURE) {
		sensor_id = specific->secure_sensor_id;
	}
#endif
	else {
		dev_err(dev, "%s: position value is wrong", __func__);
		return -EINVAL;
	}

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
	}

	if(module) {
		if (maker != NULL)
			sprintf(maker, "%s", module->sensor_maker ?
					module->sensor_maker : "UNKNOWN");
		if (name != NULL)
			sprintf(name, "%s", module->sensor_name ?
					module->sensor_name : "UNKNOWN");
		return 0;
	}

	dev_err(dev, "%s: there's no matched sensor id", __func__);

	return -ENODEV;
}

static ssize_t camera_front_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_maker[50];
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, sensor_maker, sensor_name, SENSOR_POSITION_FRONT);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_front_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	char command_ack[20] = {0, };

	fimc_is_vender_check_hw_init_running();
	read_from_firmware_version(SENSOR_POSITION_FRONT);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
		err(" NG, invalid FROM version");
		strcpy(command_ack, "NG_CD3");
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

	if (crc32_check_factory_front) {
		return sprintf(buf, "%s %s\n", front_finfo->header_ver, front_finfo->header_ver);
	} else {
		err(" NG, crc check fail");
		strcpy(command_ack, "NG_");
		if (!crc32_check_factory_front)
			strcat(command_ack, "CD3");
		if (front_finfo->header_ver[3] != 'L')
			strcat(command_ack, "_Q");
		return sprintf(buf, "%s %s\n", front_finfo->header_ver, command_ack);
	}
#else
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, NULL, sensor_name, SENSOR_POSITION_FRONT);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN UNKNOWN\n");
	else
		return sprintf(buf, "%s N\n", sensor_name);
#endif
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static ssize_t camera_front_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };

	read_from_firmware_version(SENSOR_POSITION_FRONT);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
		err(" NG, invalid FROM version");
		strcpy(command_ack, "NG_CD3");
		return sprintf(buf, "%s %s %s\n", "NULL", "N", command_ack);
	}

	if (crc32_check_factory_front) {
		return sprintf(buf, "%s %s %s\n", front_finfo->header_ver, "N", front_finfo->header_ver);
	} else {
		err(" NG, crc check fail");
		strcpy(command_ack, "NG_");
		if (!crc32_check_factory_front)
			strcat(command_ack, "CD3");
		if (front_finfo->header_ver[3] != 'L')
			strcat(command_ack, "_Q");
		return sprintf(buf, "%s %s %s\n", front_finfo->header_ver, "N", command_ack);
	}
}

static ssize_t camera_front_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[10] = {0, };

	read_from_firmware_version(SENSOR_POSITION_FRONT);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
		err(" NG, invalid FROM version");
#ifdef CAMERA_SYSFS_V2
		return sprintf(buf, "%s\n", "NG_VER");
#else
		return sprintf(buf, "%s\n", "NG");
#endif
	}

	if (crc32_check_factory_front) {
		if (!is_final_cam_module_front) {
			err(" NG, not final cam module");
#ifdef CAMERA_SYSFS_V2
			strcpy(command_ack, "NG_VER\n");
#else
			strcpy(command_ack, "NG\n");
#endif
		} else {
			strcpy(command_ack, "OK\n");
		}
	} else {
		err(" NG, crc check fail");
#ifdef CAMERA_SYSFS_V2
		strcpy(command_ack, "NG_CRC\n");
#else
		strcpy(command_ack, "NG\n");
#endif
	}
	return sprintf(buf, "%s", command_ack);
}
#endif

#ifdef CAMERA_SYSFS_V2
static ssize_t camera_front_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char camera_info[110] = {0, };
#ifdef CONFIG_OF
	struct fimc_is_cam_info *front_cam_info = &(cam_infos[CAM_INFO_FRONT]);
	strcpy(camera_info, "ISP=");
	switch(front_cam_info->isp) {
		case CAM_INFO_ISP_TYPE_INTERNAL :
			strcat(camera_info, "INT;");
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL :
			strcat(camera_info, "EXT;");
			break;
		case CAM_INFO_ISP_TYPE_SOC :
			strcat(camera_info, "SOC;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CALMEM=");
	switch(front_cam_info->cal_memory) {
		case CAM_INFO_CAL_MEM_TYPE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM :
		case CAM_INFO_CAL_MEM_TYPE_EEPROM :
		case CAM_INFO_CAL_MEM_TYPE_OTP :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "READVER=");
	switch(front_cam_info->read_version) {
		case CAM_INFO_READ_VER_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_READ_VER_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "COREVOLT=");
	switch(front_cam_info->core_voltage) {
		case CAM_INFO_CORE_VOLT_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CORE_VOLT_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "UPGRADE=");
	switch(front_cam_info->upgrade) {
		case CAM_INFO_FW_UPGRADE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_FW_UPGRADE_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "FWWRITE=");
	switch(front_cam_info->fw_write) {
		case CAM_INFO_FW_WRITE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_WRITE_OS :
			strcat(camera_info, "OS;");
			break;
		case CAM_INFO_FW_WRITE_SD :
			strcat(camera_info, "SD;");
			break;
		case CAM_INFO_FW_WRITE_ALL :
			strcat(camera_info, "ALL;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "FWDUMP=");
	switch(front_cam_info->fw_dump) {
		case CAM_INFO_FW_DUMP_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_DUMP_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CC=");
	switch(front_cam_info->companion) {
		case CAM_INFO_COMPANION_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_COMPANION_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "OIS=");
	switch(front_cam_info->ois) {
		case CAM_INFO_OIS_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_OIS_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	return sprintf(buf, "%s\n", camera_info);
#endif
	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL");

	return sprintf(buf, "%s\n", camera_info);
}
#endif

static DEVICE_ATTR(front_camtype, S_IRUGO,
		camera_front_camtype_show, NULL);
#ifdef CAMERA_SYSFS_V2
static DEVICE_ATTR(front_caminfo, S_IRUGO,
		camera_front_info_show, NULL);
#endif
static DEVICE_ATTR(front_camfw, S_IRUGO, camera_front_camfw_show, NULL);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static DEVICE_ATTR(front_camfw_full, S_IRUGO, camera_front_camfw_full_show, NULL);
static DEVICE_ATTR(front_checkfw_factory, S_IRUGO, camera_front_checkfw_factory_show, NULL);
#endif

#ifdef CAMERA_MODULE_DUALIZE
static ssize_t camera_rear_writefw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
/* Disabled for product. Security Hole */ 
#if 0
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;
	int ret = 0;

	ret = fimc_is_sec_write_fw(sysfs_core, is_dev);

	if (ret)
		return sprintf(buf, "NG\n");
	else
		return sprintf(buf, "OK\n");
#else
	return sprintf(buf, "OK\n");
#endif
}
#endif

static ssize_t camera_rear_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_maker[50];
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, sensor_maker, sensor_name, SENSOR_POSITION_REAR);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_rear_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };
	char *loaded_fw;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);

	fimc_is_vender_check_hw_init_running();
	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid FROM version");
		strcpy(command_ack, "NG_FWCDFW1CD1");
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	loaded_fw = pinfo->header_ver;
#else
	fimc_is_sec_get_loaded_fw(&loaded_fw);
#endif

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory && crc32_setfile_check
			&& crc32_front_setfile_check
#ifdef CONFIG_COMPANION_USE
		    && crc32_c1_fw_check && crc32_c1_check_factory
#endif
		) {
			return sprintf(buf, "%s %s\n", finfo->header_ver, loaded_fw);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			if (!crc32_fw_check)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
			if (!crc32_setfile_check || !crc32_front_setfile_check)
				strcat(command_ack, "SET");
#ifdef CONFIG_COMPANION_USE
			if (!crc32_c1_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_c1_check_factory)
				strcat(command_ack, "CD1");
#endif
			if (finfo->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s\n", finfo->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		strcat(command_ack, "CD");
#else
		strcat(command_ack, "FWCD");
#endif
#ifdef CONFIG_COMPANION_USE
		strcat(command_ack, "FW1CD1");
#endif
		return sprintf(buf, "%s %s\n", finfo->header_ver, command_ack);
	}
}

static ssize_t camera_rear_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };
	char *loaded_fw;

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid FROM version");
		strcpy(command_ack, "NG_FWCDFW1CD1");
		return sprintf(buf, "%s %s %s\n", "NULL", "NULL", command_ack);
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	loaded_fw = pinfo->header_ver;
#else
	fimc_is_sec_get_loaded_fw(&loaded_fw);
#endif

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory && crc32_setfile_check
			&& crc32_front_setfile_check
#ifdef CONFIG_COMPANION_USE
		    && crc32_c1_fw_check && crc32_c1_check_factory
#endif
		) {
			return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, loaded_fw);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			if (!crc32_fw_check)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
			if (!crc32_setfile_check || !crc32_front_setfile_check)
				strcat(command_ack, "SET");
#ifdef CONFIG_COMPANION_USE
			if (!crc32_c1_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_c1_check_factory)
				strcat(command_ack, "CD1");
#endif
			if (finfo->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		strcat(command_ack, "CD");
#else
		strcat(command_ack, "FWCD");
#endif
#ifdef CONFIG_COMPANION_USE
		strcat(command_ack, "FW1CD1");
#endif
		return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, command_ack);
	}
}

static ssize_t camera_rear_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *cal_buf;

	read_from_firmware_version(SENSOR_POSITION_REAR);

	/* 2T2 Sensor Only. Can be removed after development */
	fimc_is_sec_get_cal_buf(&cal_buf);
	if (((finfo->header_ver[0] == 'A') && (finfo->header_ver[1] == '2') && (finfo->header_ver[2] == '0'))
		   && (cal_buf[0xC0] < 0xC0)) {
		err(" NG, old sensor revision");
		return sprintf(buf, "%s\n", "NG");
	}

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid FROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
		    && crc32_c1_fw_check && crc32_c1_check_factory
#endif
		) {
			if (!is_latest_cam_module) {
				err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[10] = {0, };
#ifdef CONFIG_OIS_USE
	struct fimc_is_ois_info *ois_minfo = NULL;
	bool ois_ret = false;

	ois_ret = read_ois_version();
	fimc_is_ois_get_module_version(&ois_minfo);
#endif

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid FROM version");
#ifdef CAMERA_SYSFS_V2
		return sprintf(buf, "%s\n", "NG_VER");
#else
		return sprintf(buf, "%s\n", "NG");
#endif
	}

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory && crc32_setfile_check
			&& crc32_front_setfile_check
#ifdef CONFIG_COMPANION_USE
		    && crc32_c1_fw_check && crc32_c1_check_factory
#endif
		) {
			if (!is_final_cam_module) {
				err(" NG, not final cam module");
#ifdef CAMERA_SYSFS_V2
				strcpy(command_ack, "NG_VER\n");
#else
				strcpy(command_ack, "NG\n");
#endif
			} else {
#ifdef CONFIG_OIS_USE
				if (ois_minfo->checksum != 0x00 || ois_minfo->caldata != 0x00 || !ois_ret) {
					err(" NG, OIS crc check fail, checksum = 0x%02x, caldata = 0x%02x, ois_ret = %d",
						ois_minfo->checksum, ois_minfo->caldata, ois_ret);
#ifdef CAMERA_SYSFS_V2
					strcpy(command_ack, "NG_CRC\n");
#else
					strcpy(command_ack, "NG\n");
#endif
				} else {
					strcpy(command_ack, "OK\n");
				}
#else
				strcpy(command_ack, "OK\n");
#endif
			}
		} else {
			err(" NG, crc check fail");
#ifdef CAMERA_SYSFS_V2
			strcpy(command_ack, "NG_CRC\n");
#else
			strcpy(command_ack, "NG\n");
#endif
		}
	} else {
		err(" NG, fw ver crc check fail");
#ifdef CAMERA_SYSFS_V2
		strcpy(command_ack, "NG_VER\n");
#else
		strcpy(command_ack, "NG\n");
#endif
	}

	return sprintf(buf, "%s", command_ack);
}
#ifdef CAMERA_SYSFS_V2
static ssize_t camera_rear_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char camera_info[110] = {0, };
#ifdef CONFIG_OF
	struct fimc_is_cam_info *rear_cam_info = &(cam_infos[CAM_INFO_REAR]);

	strcpy(camera_info, "ISP=");
	switch(rear_cam_info->isp) {
		case CAM_INFO_ISP_TYPE_INTERNAL :
			strcat(camera_info, "INT;");
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL :
			strcat(camera_info, "EXT;");
			break;
		case CAM_INFO_ISP_TYPE_SOC :
			strcat(camera_info, "SOC;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CALMEM=");
	switch(rear_cam_info->cal_memory) {
		case CAM_INFO_CAL_MEM_TYPE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM :
		case CAM_INFO_CAL_MEM_TYPE_EEPROM :
		case CAM_INFO_CAL_MEM_TYPE_OTP :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "READVER=");
	switch(rear_cam_info->read_version) {
		case CAM_INFO_READ_VER_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_READ_VER_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "COREVOLT=");
	switch(rear_cam_info->core_voltage) {
		case CAM_INFO_CORE_VOLT_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CORE_VOLT_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "UPGRADE=");
	switch(rear_cam_info->upgrade) {
		case CAM_INFO_FW_UPGRADE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_FW_UPGRADE_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "FWWRITE=");
	switch(rear_cam_info->fw_write) {
		case CAM_INFO_FW_WRITE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_WRITE_OS :
			strcat(camera_info, "OS;");
			break;
		case CAM_INFO_FW_WRITE_SD :
			strcat(camera_info, "SD;");
			break;
		case CAM_INFO_FW_WRITE_ALL :
			strcat(camera_info, "ALL;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "FWDUMP=");
	switch(rear_cam_info->fw_dump) {
		case CAM_INFO_FW_DUMP_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_DUMP_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CC=");
	switch(rear_cam_info->companion) {
		case CAM_INFO_COMPANION_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_COMPANION_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "OIS=");
	switch(rear_cam_info->ois) {
		case CAM_INFO_OIS_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_OIS_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	return sprintf(buf, "%s\n", camera_info);
#endif
	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL");

	return sprintf(buf, "%s\n", camera_info);
}
#endif

static ssize_t camera_rear_sensor_standby(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	switch (buf[0]) {
	case '0':
		break;
	case '1':
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}

static ssize_t camera_rear_sensor_standby_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Rear sensor standby \n");
}

#ifdef CONFIG_COMPANION_USE
static ssize_t camera_rear_companionfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *loaded_c1_fw;

	fimc_is_vender_check_hw_init_running();
	read_from_firmware_version(SENSOR_POSITION_REAR);
	fimc_is_sec_get_loaded_c1_fw(&loaded_c1_fw);

	return sprintf(buf, "%s %s\n",
		finfo->concord_header_ver, loaded_c1_fw);
}

static ssize_t camera_rear_companionfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *loaded_c1_fw;

	read_from_firmware_version(SENSOR_POSITION_REAR);
	fimc_is_sec_get_loaded_c1_fw(&loaded_c1_fw);

	return sprintf(buf, "%s %s %s\n",
		finfo->concord_header_ver, pinfo->concord_header_ver, loaded_c1_fw);
}
#endif

static ssize_t camera_rear_camfw_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;

	if ((size == 1 || size == 2) && (buf[0] == 'F' || buf[0] == 'f')) {
		fimc_is_sec_set_force_caldata_dump(true);
		ret = size;
	} else {
		fimc_is_sec_set_force_caldata_dump(false);
	}
	return ret;
}

static ssize_t camera_rear_calcheck_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char rear_sensor[10] = {0, };
#ifdef CONFIG_COMPANION_USE
	char rear_companion[10] = {0, };
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_FRONT
	char front_sensor[10] = {0, };
#endif

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR) && crc32_check_factory)
		strcpy(rear_sensor, "Normal");
	else
		strcpy(rear_sensor, "Abnormal");

#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_FRONT
	read_from_firmware_version(SENSOR_POSITION_FRONT);
#ifdef CONFIG_COMPANION_USE
	if (fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR) && crc32_c1_check_factory)
		strcpy(rear_companion, "Normal");
	else
		strcpy(rear_companion, "Abnormal");

	if (fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_FRONT) && crc32_check_factory_front)
		strcpy(front_sensor, "Normal");
	else
		strcpy(front_sensor, "Abnormal");

	return sprintf(buf, "%s %s %s\n", rear_sensor, rear_companion, front_sensor);
#else
	return sprintf(buf, "%s %s\n", rear_sensor, front_sensor);
#endif
#else
#ifdef CONFIG_COMPANION_USE
	if (fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR) && crc32_c1_check_factory)
		strcpy(rear_companion, "Normal");
	else
		strcpy(rear_companion, "Abnormal");

	return sprintf(buf, "%s %s %s\n", rear_sensor, rear_companion, "Null");
#else
	return sprintf(buf, "%s %s\n", rear_sensor, "Null");
#endif
#endif
}

#ifdef CONFIG_COMPANION_USE
static ssize_t camera_isp_core_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_COMPANION_DCDC_USE
	int sel;
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	if (DCDC_VENDOR_NONE == specific->companion_dcdc.type)
		return sprintf(buf, "none\n");

	sel = fimc_is_power_binning(sysfs_core);
	return sprintf(buf, "%s\n", specific->companion_dcdc.get_vout_str(sel));
#else
	return sprintf(buf, "none\n");
#endif
}
#endif

static ssize_t camera_hw_init_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_vender *vender;

	vender = &sysfs_core->vender;

	if (!check_module_init) {
		fimc_is_vender_hw_init(vender);
		check_module_init = true;
	}

	return sprintf(buf, "%s\n", "HW init done.");
}

#ifdef CONFIG_OIS_USE
static ssize_t camera_ois_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)

{
	fimc_is_vender_check_hw_init_running();

	switch (buf[0]) {
	case '0':
		fimc_is_ois_gpio_off(sysfs_core);
		check_ois_power = false;
		break;
	case '1':
		fimc_is_ois_gpio_on(sysfs_core);
		check_ois_power = true;
		msleep(150);
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}

static ssize_t camera_ois_autotest_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)

{
	int value = 0;

	if (kstrtoint(buf, 10, &value)) {
		err("convert fail");
	}

	ois_threshold = value;

	return count;
}

static ssize_t camera_ois_autotest_show(struct device *dev,
				    struct device_attribute *attr, char *buf)

{
	bool x_result = false, y_result = false;
	int sin_x = 0, sin_y = 0;

	if (check_ois_power) {
		fimc_is_ois_auto_test(sysfs_core, ois_threshold,
			&x_result, &y_result, &sin_x, &sin_y);

		if (x_result && y_result) {
			return sprintf(buf, "%s,%d,%s,%d\n", "pass", 0, "pass", 0);
		} else if (x_result) {
			return sprintf(buf, "%s,%d,%s,%d\n", "pass", 0, "fail", sin_y);
		} else if (y_result) {
			return sprintf(buf, "%s,%d,%s,%d\n", "fail", sin_x, "pass", 0);
		} else {
			return sprintf(buf, "%s,%d,%s,%d\n", "fail", sin_x, "fail", sin_y);
		}
	} else {
		err("OIS power is not enabled.");
		return sprintf(buf, "%s,%d,%s,%d\n", "fail", sin_x, "fail", sin_y);
	}
}

static ssize_t camera_ois_selftest_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int result_total = 0;
	bool result_offset = 0, result_selftest = 0;
	int selftest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0;

	if (check_ois_power) {
		fimc_is_ois_offset_test(sysfs_core, &raw_data_x, &raw_data_y);
		msleep(50);
		selftest_ret = fimc_is_ois_self_test(sysfs_core);

		if (selftest_ret == 0x0) {
			result_selftest = true;
		} else {
			result_selftest = false;
		}

		if (abs(raw_data_x) > CAMERA_OIS_GYRO_OFFSET_SPEC || abs(raw_data_y) > CAMERA_OIS_GYRO_OFFSET_SPEC)  {
			result_offset = false;
		} else {
			result_offset = true;
		}

		if (result_offset && result_selftest) {
			result_total = 0;
		} else if (!result_offset && !result_selftest) {
			result_total = 3;
		} else if (!result_offset) {
			result_total = 1;
		} else if (!result_selftest) {
			result_total = 2;
		}

		if (raw_data_x < 0 && raw_data_y < 0) {
			return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
				abs(raw_data_y /1000), abs(raw_data_y % 1000));
		} else if (raw_data_x < 0) {
			return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
				raw_data_y /1000, raw_data_y % 1000);
		} else if (raw_data_y < 0) {
			return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
				abs(raw_data_y /1000), abs(raw_data_y % 1000));
		} else {
			return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
				raw_data_y /1000, raw_data_y % 1000);
		}
	} else {
		err("OIS power is not enabled.");
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	long raw_data_x = 0, raw_data_y = 0;

	if (check_ois_power) {
		fimc_is_ois_get_offset_data(sysfs_core, &raw_data_x, &raw_data_y);

		if (raw_data_x < 0 && raw_data_y < 0) {
			return sprintf(buf, "-%ld.%03ld,-%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
				abs(raw_data_y /1000), abs(raw_data_y % 1000));
		} else if (raw_data_x < 0) {
			return sprintf(buf, "-%ld.%03ld,%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
				raw_data_y /1000, raw_data_y % 1000);
		} else if (raw_data_y < 0) {
			return sprintf(buf, "%ld.%03ld,-%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
				abs(raw_data_y /1000), abs(raw_data_y % 1000));
		} else {
			return sprintf(buf, "%ld.%03ld,%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
				raw_data_y /1000, raw_data_y % 1000);
		}
	} else {
		err("OIS power is not enabled.");
		return sprintf(buf, "%ld.%03ld,%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

#ifdef CONFIG_COMPANION_FACTORY_VALIDATION
static ssize_t camera_comp_ic_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct fimc_is_vender_specific *specific = sysfs_core->vender.private_data;

	info("%s: buf[0]=%c running_rear_camera=%d running_front_camera=%d\n",
		__func__,buf[0],
		specific->running_rear_camera,specific->running_front_camera);

	switch (buf[0]) {
	case '0':
		break;
	case '1':
		comp_fac_i2c_check = 0;
		comp_fac_valid_check = 0;
		if (!specific->running_rear_camera) {
			if (!specific->running_front_camera) {
    				ret = fimc_is_comp_fac_valid(sysfs_core);
			}
		}
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	} 

	return count;
}

static ssize_t camera_comp_ic_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
    
	info("%s: comp_fac_i2c_check[%d] comp_fac_valid_check[0x%04x]\n",
		__func__, comp_fac_i2c_check, comp_fac_valid_check);

	if (comp_fac_i2c_check) {
		len = sprintf(buf, "%s\n", "NG_I2C");
	} else if (comp_fac_valid_check) {
		len = sprintf(buf, "%04X\n", comp_fac_valid_check);
	} else {
		len = sprintf(buf, "%s\n", "NG");
	}

	info("buf: %s\n",buf);

	return len;
}
#endif

static ssize_t camera_ois_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	bool ret = false;

	fimc_is_vender_check_hw_init_running();
	ret = read_ois_version();
	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);

	if (ois_minfo->checksum != 0x00 || !ret) {
		return sprintf(buf, "%s %s\n", "NG_FW2", "NULL");
	} else if (ois_minfo->caldata != 0x00) {
		return sprintf(buf, "%s %s\n", "NG_CD2", ois_pinfo->header_ver);
	} else {
		return sprintf(buf, "%s %s\n", ois_minfo->header_ver, ois_pinfo->header_ver);
	}
}

static ssize_t camera_ois_diff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result = 0;
	int x_diff = 0, y_diff = 0;

	if (check_ois_power) {
		result = fimc_is_ois_diff_test(sysfs_core, &x_diff, &y_diff);

		return sprintf(buf, "%d,%d,%d\n", result == true ? 0 : 1, x_diff, y_diff);
	} else {
		err("OIS power is not enabled.");
		return sprintf(buf, "%d,%d,%d\n", 0, x_diff, y_diff);
	}
}

static ssize_t camera_ois_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	struct fimc_is_ois_info *ois_uinfo = NULL;
	struct fimc_is_ois_exif *ois_exif = NULL;

	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);
	fimc_is_ois_get_user_version(&ois_uinfo);
	fimc_is_ois_get_exif_data(&ois_exif);

	return sprintf(buf, "%s %s %s %d %d", ois_minfo->header_ver, ois_pinfo->header_ver,
		ois_uinfo->header_ver, ois_exif->error_data, ois_exif->status_data);
}
#endif

#ifdef FORCE_CAL_LOAD
static ssize_t camera_rear_force_cal_load_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	fimc_is_sec_set_force_caldata_dump(true);
	return sprintf(buf, "FORCE CALDATA LOAD\n");
}
#endif

static ssize_t camera_rear_afcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_REAR);

	return sprintf(buf, "1 %d %d\n", finfo->af_cal_macro, finfo->af_cal_pan);
}

static ssize_t camera_rear_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_REAR);

	return sprintf(buf, "%s", finfo->from_sensor_id);
}

#ifdef FROM_HEADER_MODULE_ID_ADDR
static ssize_t camera_rear_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_REAR);

	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
		finfo->from_module_id[0], finfo->from_module_id[1], finfo->from_module_id[2],
		finfo->from_module_id[3], finfo->from_module_id[4], finfo->from_module_id[5],
		finfo->from_module_id[6], finfo->from_module_id[7], finfo->from_module_id[8],
		finfo->from_module_id[9]);
}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static ssize_t camera_front_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_FRONT);

	return sprintf(buf, "%s", front_finfo->from_sensor_id);
}
#endif

#ifdef CONFIG_SECURE_CAMERA_USE
static ssize_t camera_iris_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_name[50];
	int ret;

	if (is_iris_ver_read) {
		ret = fimc_is_get_sensor_data(dev, NULL, sensor_name, SENSOR_POSITION_SECURE);

		if (ret < 0) {
			return sprintf(buf, "UNKNOWN N\n");
		} else {
			return sprintf(buf, "%s N\n", sensor_name);
		}
	} else {
		return sprintf(buf, "UNKNOWN N\n");
	}
}

static ssize_t camera_iris_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_name[50];
	int ret;

	if (is_iris_ver_read) {
		ret = fimc_is_get_sensor_data(dev, NULL, sensor_name, SENSOR_POSITION_SECURE);

		if (ret < 0) {
			return sprintf(buf, "UNKNOWN N N\n");
		} else {
			return sprintf(buf, "%s N N\n", sensor_name);
		}
	} else {
		return sprintf(buf, "UNKNOWN N N\n");
	}
}

static ssize_t camera_iris_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (is_iris_ver_read) {
		if (is_final_cam_module_iris) {
			return sprintf(buf, "%s\n", "OK");
		} else {
#ifdef CAMERA_SYSFS_V2
			return sprintf(buf, "%s\n", "NG_VER");
#else
			return sprintf(buf, "%s\n", "NG");
#endif
		}
	} else {
		return sprintf(buf, "%s\n", "NG_VER");
	}
}

static ssize_t camera_iris_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (is_iris_ver_read) {
		if (is_final_cam_module_iris)
			return sprintf(buf, "%s\n", "OK");
		else
			return sprintf(buf, "%s\n", "NG");
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}


#ifdef CAMERA_SYSFS_V2
static ssize_t camera_iris_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char camera_info[110] = {0, };
#ifdef CONFIG_OF
	struct fimc_is_cam_info *iris_cam_info = &(cam_infos[CAM_INFO_IRIS]);

	strcpy(camera_info, "ISP=");
	switch(iris_cam_info->isp) {
		case CAM_INFO_ISP_TYPE_INTERNAL :
			strcat(camera_info, "INT;");
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL :
			strcat(camera_info, "EXT;");
			break;
		case CAM_INFO_ISP_TYPE_SOC :
			strcat(camera_info, "SOC;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CALMEM=");
	switch(iris_cam_info->cal_memory) {
		case CAM_INFO_CAL_MEM_TYPE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM :
		case CAM_INFO_CAL_MEM_TYPE_EEPROM :
		case CAM_INFO_CAL_MEM_TYPE_OTP :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "READVER=");
	switch(iris_cam_info->read_version) {
		case CAM_INFO_READ_VER_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_READ_VER_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "COREVOLT=");
	switch(iris_cam_info->core_voltage) {
		case CAM_INFO_CORE_VOLT_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CORE_VOLT_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "UPGRADE=");
	switch(iris_cam_info->upgrade) {
		case CAM_INFO_FW_UPGRADE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_FW_UPGRADE_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "FWWRITE=");
	switch(iris_cam_info->fw_write) {
		case CAM_INFO_FW_WRITE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_WRITE_OS :
			strcat(camera_info, "OS;");
			break;
		case CAM_INFO_FW_WRITE_SD :
			strcat(camera_info, "SD;");
			break;
		case CAM_INFO_FW_WRITE_ALL :
			strcat(camera_info, "ALL;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "FWDUMP=");
	switch(iris_cam_info->fw_dump) {
		case CAM_INFO_FW_DUMP_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_DUMP_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CC=");
	switch(iris_cam_info->companion) {
		case CAM_INFO_COMPANION_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_COMPANION_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "OIS=");
	switch(iris_cam_info->ois) {
		case CAM_INFO_OIS_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_OIS_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	return sprintf(buf, "%s\n", camera_info);
#endif
	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL");

	return sprintf(buf, "%s\n", camera_info);
}
#endif
#endif

#ifdef USE_CAMERA_HW_BIG_DATA
static ssize_t rear_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;

	read_from_firmware_version(SENSOR_POSITION_REAR);

	fimc_is_sec_get_rear_hw_param(&ec_param);

	if (fimc_is_sec_is_valid_moduleid(finfo->from_module_id)) {
		return sprintf(buf, "\"CAMIR_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR_AF\":\"%d\","
			"\"I2CR_COM\":\"%d\",\"I2CR_OIS\":\"%d\",\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
			finfo->from_module_id[0], finfo->from_module_id[1], finfo->from_module_id[2], finfo->from_module_id[3],
			finfo->from_module_id[4], finfo->from_module_id[7], finfo->from_module_id[8], finfo->from_module_id[9],
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
		return sprintf(buf, "\"CAMIR_ID\":\"MIR_ERR\",\"I2CR_AF\":\"%d\","
			"\"I2CR_COM\":\"%d\",\"I2CR_OIS\":\"%d\",\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
			ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t rear_camera_hw_param_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;

	if (!strncmp(buf, "c", 1)) {
		fimc_is_sec_get_rear_hw_param(&ec_param);

		if (ec_param)
			fimc_is_sec_init_err_cnt_file(ec_param);
	}

	return count;
}

static ssize_t front_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;

	fimc_is_sec_get_front_hw_param(&ec_param);

	return sprintf(buf, "\"CAMIF_ID\":\"MIR_NO\",\"I2CF_AF\":\"%d\","
		"\"I2CF_COM\":\"%d\",\"I2CF_OIS\":\"%d\",\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
		ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt, ec_param->i2c_sensor_err_cnt,
		ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
}

static ssize_t front_camera_hw_param_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;

	if (!strncmp(buf, "c", 1)) {
		fimc_is_sec_get_front_hw_param(&ec_param);

		if (ec_param)
			fimc_is_sec_init_err_cnt_file(ec_param);
	}

	return count;
}

static ssize_t iris_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;

	fimc_is_sec_get_iris_hw_param(&ec_param);

	return sprintf(buf, "\"I2CI_AF\":\"%d\",\"I2CI_COM\":\"%d\",\"I2CI_OIS\":\"%d\","
		"\"I2CI_SEN\":\"%d\",\"MIPII_COM\":\"%d\",\"MIPII_SEN\":\"%d\"\n",
		ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
		ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
}

static ssize_t iris_camera_hw_param_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;

	if (!strncmp(buf, "c", 1)) {
		fimc_is_sec_get_iris_hw_param(&ec_param);

		if (ec_param)
			fimc_is_sec_init_err_cnt_file(ec_param);
	}

	return count;
}
#endif

#ifdef CAMERA_MODULE_DUALIZE
static DEVICE_ATTR(from_write, S_IRUGO,
		camera_rear_writefw_show, NULL);
#endif
static DEVICE_ATTR(rear_camtype, S_IRUGO,
		camera_rear_camtype_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO,
		camera_rear_camfw_show, camera_rear_camfw_write);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO,
		camera_rear_camfw_full_show, NULL);
#ifdef CONFIG_COMPANION_USE
static DEVICE_ATTR(rear_companionfw, S_IRUGO,
		camera_rear_companionfw_show, NULL);
static DEVICE_ATTR(rear_companionfw_full, S_IRUGO,
		camera_rear_companionfw_full_show, NULL);
#endif
static DEVICE_ATTR(rear_calcheck, S_IRUGO,
		camera_rear_calcheck_show, NULL);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO,
		camera_rear_checkfw_user_show, NULL);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO,
		camera_rear_checkfw_factory_show, NULL);
static DEVICE_ATTR(rear_sensor_standby, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
		camera_rear_sensor_standby_show, camera_rear_sensor_standby);
#ifdef CAMERA_SYSFS_V2
static DEVICE_ATTR(rear_caminfo, S_IRUGO,
		camera_rear_info_show, NULL);
#endif
#ifdef CONFIG_COMPANION_USE
static DEVICE_ATTR(isp_core, S_IRUGO,
		camera_isp_core_show, NULL);
#endif
static DEVICE_ATTR(fw_update, S_IRUGO,
		camera_hw_init_show, NULL);
#ifdef CONFIG_COMPANION_FACTORY_VALIDATION
static DEVICE_ATTR(companion_ic_check, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, 
		camera_comp_ic_check_show, camera_comp_ic_check_store);
#endif
#ifdef CONFIG_OIS_USE
static DEVICE_ATTR(selftest, S_IRUGO,
		camera_ois_selftest_show, NULL);
static DEVICE_ATTR(ois_power, S_IWUSR,
		NULL, camera_ois_power_store);
static DEVICE_ATTR(autotest, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
		camera_ois_autotest_show, camera_ois_autotest_store);
static DEVICE_ATTR(ois_rawdata, S_IRUGO,
		camera_ois_rawdata_show, NULL);
static DEVICE_ATTR(oisfw, S_IRUGO,
		camera_ois_version_show, NULL);
static DEVICE_ATTR(ois_diff, S_IRUGO,
		camera_ois_diff_show, NULL);
static DEVICE_ATTR(ois_exif, S_IRUGO,
		camera_ois_exif_show, NULL);
#endif
#ifdef FORCE_CAL_LOAD
static DEVICE_ATTR(rear_force_cal_load, S_IRUGO, camera_rear_force_cal_load_show, NULL);
#endif
static DEVICE_ATTR(rear_afcal, S_IRUGO, camera_rear_afcal_show, NULL);
static DEVICE_ATTR(rear_sensorid_exif, S_IRUGO, camera_rear_sensorid_exif_show, NULL);
#ifdef FROM_HEADER_MODULE_ID_ADDR
static DEVICE_ATTR(rear_moduleid, S_IRUGO, camera_rear_moduleid_show, NULL);
static DEVICE_ATTR(SVC_rear_module, S_IRUGO, camera_rear_moduleid_show, NULL);
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static DEVICE_ATTR(front_sensorid_exif, S_IRUGO, camera_front_sensorid_exif_show, NULL);
#endif

#ifdef CONFIG_SECURE_CAMERA_USE
static DEVICE_ATTR(iris_camfw, S_IRUGO, camera_iris_camfw_show, NULL);
static DEVICE_ATTR(iris_camfw_full, S_IRUGO, camera_iris_camfw_full_show, NULL);
static DEVICE_ATTR(iris_checkfw_factory, S_IRUGO, camera_iris_checkfw_factory_show, NULL);
static DEVICE_ATTR(iris_checkfw_user, S_IRUGO, camera_iris_checkfw_user_show, NULL);
#ifdef CAMERA_SYSFS_V2
static DEVICE_ATTR(iris_caminfo, S_IRUGO, camera_iris_info_show, NULL);
#endif
#endif

#ifdef USE_CAMERA_HW_BIG_DATA
static DEVICE_ATTR(rear_hwparam, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
				rear_camera_hw_param_show, rear_camera_hw_param_store);
static DEVICE_ATTR(front_hwparam, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
				front_camera_hw_param_show, front_camera_hw_param_store);
static DEVICE_ATTR(iris_hwparam, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
				iris_camera_hw_param_show, iris_camera_hw_param_store);
#endif

int svc_cheating_prevent_device_file_create(struct kobject **obj)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct kobject *Camera;

	/* To find SVC kobject */
	svc_sd = sysfs_get_dirent(devices_kset->kobj.sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", &devices_kset->kobj);
		if (IS_ERR_OR_NULL(data)) {
			pr_info("Failed to create sys/devices/svc already exist svc : 0x%p\n", data);
		} else {
			pr_info("Success to create sys/devices/svc svc : 0x%p\n", data);
		}
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Success to find svc_sd : 0x%p svc : 0x%p\n", svc_sd, data);
	}

	Camera = kobject_create_and_add("Camera", data);
	if (IS_ERR_OR_NULL(Camera)) {
		pr_info("Failed to create sys/devices/svc/Camera : 0x%p\n", Camera);
	} else {
		pr_info("Success to create sys/devices/svc/Camera : 0x%p\n", Camera);
	}

	*obj = Camera;
	return 0;
}

int fimc_is_create_sysfs(struct fimc_is_core *core)
{
	struct kobject *svc = 0;

	if (!core) {
		err("fimc_is_core is null");
		return -EINVAL;
	}

	svc_cheating_prevent_device_file_create(&svc);

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			pr_err("Failed to create class(camera)!\n");
			return PTR_ERR(camera_class);
		}
	}

	camera_front_dev = device_create(camera_class, NULL, 0, NULL, "front");
	if (IS_ERR(camera_front_dev)) {
		printk(KERN_ERR "failed to create front device!\n");
	} else {
		if (device_create_file(camera_front_dev,
				&dev_attr_front_sensorid) < 0) {
			printk(KERN_ERR "failed to create front device file, %s\n",
					dev_attr_front_sensorid.attr.name);
		}

		if (device_create_file(camera_front_dev,
					&dev_attr_front_camtype) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camtype.attr.name);
		}
		if (device_create_file(camera_front_dev,
					&dev_attr_front_camfw) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camfw.attr.name);
		}
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		if (device_create_file(camera_front_dev,
					&dev_attr_front_camfw_full) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camfw_full.attr.name);
		}
		if (device_create_file(camera_front_dev,
				&dev_attr_front_checkfw_factory) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_checkfw_factory.attr.name);
		}
		if (device_create_file(camera_front_dev,
				&dev_attr_front_sensorid_exif) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_sensorid_exif.attr.name);
		}
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
		if (device_create_file(camera_front_dev,
				&dev_attr_front_hwparam) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_hwparam.attr.name);
		}
#endif
	}
#ifdef CAMERA_SYSFS_V2
		if (device_create_file(camera_front_dev,
					&dev_attr_front_caminfo) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_caminfo.attr.name);
		}
#endif
	camera_rear_dev = device_create(camera_class, NULL, 1, NULL, "rear");
	if (IS_ERR(camera_rear_dev)) {
		printk(KERN_ERR "failed to create rear device!\n");
	} else {
		if (device_create_file(camera_rear_dev, &dev_attr_rear_sensorid) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_sensorid.attr.name);
		}
#ifdef CAMERA_MODULE_DUALIZE
		if (device_create_file(camera_rear_dev, &dev_attr_from_write) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_from_write.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev, &dev_attr_rear_camtype) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_camtype.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_camfw) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_camfw.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_camfw_full) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_camfw_full.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_checkfw_user) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_checkfw_user.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_checkfw_factory) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_checkfw_factory.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_sensor_standby) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_sensor_standby.attr.name);
		}
#ifdef CAMERA_SYSFS_V2
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_caminfo) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_caminfo.attr.name);
		}
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
		if (device_create_file(camera_rear_dev, &dev_attr_rear_hwparam) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_hwparam.attr.name);
		}
#endif
#ifdef CONFIG_COMPANION_USE
		if (device_create_file(camera_rear_dev, &dev_attr_rear_companionfw) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_companionfw.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_companionfw_full) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_companionfw_full.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev, &dev_attr_rear_calcheck) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_calcheck.attr.name);
		}
#ifdef CONFIG_COMPANION_USE
		if (device_create_file(camera_rear_dev, &dev_attr_isp_core) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_isp_core.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev, &dev_attr_fw_update) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_fw_update.attr.name);
		}
#ifdef FORCE_CAL_LOAD
		if (device_create_file(camera_rear_dev, &dev_attr_rear_force_cal_load) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_force_cal_load.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev, &dev_attr_rear_afcal) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_afcal.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_sensorid_exif) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_sensorid_exif.attr.name);
		}
#ifdef FROM_HEADER_MODULE_ID_ADDR
		if (device_create_file(camera_rear_dev, &dev_attr_rear_moduleid) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_moduleid.attr.name);
		}
		if (sysfs_create_file(svc, &dev_attr_SVC_rear_module.attr) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_SVC_rear_module.attr.name);
		}
#endif
#ifdef CONFIG_COMPANION_FACTORY_VALIDATION
		if (device_create_file(camera_rear_dev, &dev_attr_companion_ic_check) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_companion_ic_check.attr.name);
		}
#endif
	}

#ifdef CONFIG_OIS_USE
	camera_ois_dev = device_create(camera_class, NULL, 2, NULL, "ois");
	if (IS_ERR(camera_ois_dev)) {
		printk(KERN_ERR "failed to create ois device!\n");
	} else {
		if (device_create_file(camera_ois_dev, &dev_attr_selftest) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_selftest.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_ois_power) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_ois_power.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_autotest) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_autotest.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_ois_rawdata) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_ois_rawdata.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_oisfw) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_oisfw.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_ois_diff) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_ois_diff.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_ois_exif) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_ois_exif.attr.name);
		}
	}
#endif
#ifdef CONFIG_SECURE_CAMERA_USE
	camera_secure_dev = device_create(camera_class, NULL, 3, NULL, "secure");
	if (IS_ERR(camera_secure_dev)) {
		printk(KERN_ERR "failed to create secure device!\n");
	} else {
		if (device_create_file(camera_secure_dev, &dev_attr_secure_sensorid) < 0) {
			printk(KERN_ERR "failed to create secure device file, %s\n",
				dev_attr_secure_sensorid.attr.name);
		}
		if (device_create_file(camera_secure_dev, &dev_attr_iris_camfw) < 0) {
			printk(KERN_ERR "failed to create iris device file, %s\n",
				dev_attr_iris_camfw.attr.name);
		}
		if (device_create_file(camera_secure_dev, &dev_attr_iris_camfw_full) < 0) {
			printk(KERN_ERR "failed to create iris device file, %s\n",
				dev_attr_iris_camfw_full.attr.name);
		}
		if (device_create_file(camera_secure_dev, &dev_attr_iris_checkfw_user) < 0) {
			printk(KERN_ERR "failed to create iris device file, %s\n",
				dev_attr_iris_checkfw_user.attr.name);
		}
		if (device_create_file(camera_secure_dev, &dev_attr_iris_checkfw_factory) < 0) {
			printk(KERN_ERR "failed to create iris device file, %s\n",
				dev_attr_iris_checkfw_factory.attr.name);
		}
#ifdef CAMERA_SYSFS_V2
		if (device_create_file(camera_secure_dev, &dev_attr_iris_caminfo) < 0) {
			printk(KERN_ERR "failed to create iris device file, %s\n",
				dev_attr_iris_caminfo.attr.name);
		}
#endif
#ifdef USE_CAMERA_HW_BIG_DATA
		if (device_create_file(camera_secure_dev, &dev_attr_iris_hwparam) < 0) {
			printk(KERN_ERR "failed to create iris device file, %s\n",
				dev_attr_iris_hwparam.attr.name);
		}
#endif
	}
#endif

	sysfs_core = core;

	return 0;
}

int fimc_is_destroy_sysfs(struct fimc_is_core *core)
{
	if (camera_front_dev) {
		device_remove_file(camera_front_dev, &dev_attr_front_sensorid);
		device_remove_file(camera_front_dev, &dev_attr_front_camtype);
		device_remove_file(camera_front_dev, &dev_attr_front_camfw);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		device_remove_file(camera_front_dev, &dev_attr_front_camfw_full);
		device_remove_file(camera_front_dev, &dev_attr_front_checkfw_factory);
		device_remove_file(camera_front_dev, &dev_attr_front_sensorid_exif);
#endif
#ifdef CAMERA_SYSFS_V2
		device_remove_file(camera_front_dev, &dev_attr_front_caminfo);
#endif
	}

	if (camera_rear_dev) {
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid);
#ifdef CAMERA_MODULE_DUALIZE
		device_remove_file(camera_rear_dev, &dev_attr_from_write);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_rear_camtype);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw_full);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_user);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_factory);
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensor_standby);
#ifdef CONFIG_COMPANION_USE
		device_remove_file(camera_rear_dev, &dev_attr_rear_companionfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_companionfw_full);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_rear_calcheck);
#ifdef CAMERA_SYSFS_V2
		device_remove_file(camera_rear_dev, &dev_attr_rear_caminfo);
#endif
#ifdef CONFIG_COMPANION_USE
		device_remove_file(camera_rear_dev, &dev_attr_isp_core);
#endif
#ifdef FORCE_CAL_LOAD
		device_remove_file(camera_rear_dev, &dev_attr_rear_force_cal_load);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_rear_afcal);
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid_exif);
#ifdef FROM_HEADER_MODULE_ID_ADDR
		device_remove_file(camera_rear_dev, &dev_attr_rear_moduleid);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_fw_update);
#ifdef CONFIG_COMPANION_FACTORY_VALIDATION
		device_remove_file(camera_rear_dev, &dev_attr_companion_ic_check);
#endif
	}

#ifdef CONFIG_OIS_USE
	if (camera_ois_dev) {
		device_remove_file(camera_ois_dev, &dev_attr_selftest);
		device_remove_file(camera_ois_dev, &dev_attr_ois_power);
		device_remove_file(camera_ois_dev, &dev_attr_autotest);
		device_remove_file(camera_ois_dev, &dev_attr_ois_rawdata);
		device_remove_file(camera_ois_dev, &dev_attr_oisfw);
		device_remove_file(camera_ois_dev, &dev_attr_ois_diff);
		device_remove_file(camera_ois_dev, &dev_attr_ois_exif);
	}
#endif
#ifdef CONFIG_SECURE_CAMERA_USE 
	if (camera_secure_dev) {
		device_remove_file(camera_secure_dev, &dev_attr_secure_sensorid);
		device_remove_file(camera_secure_dev, &dev_attr_iris_camfw);
		device_remove_file(camera_secure_dev, &dev_attr_iris_camfw_full);
		device_remove_file(camera_secure_dev, &dev_attr_iris_checkfw_user);
		device_remove_file(camera_secure_dev, &dev_attr_iris_checkfw_factory);
#ifdef CAMERA_SYSFS_V2
		device_remove_file(camera_secure_dev, &dev_attr_iris_caminfo);
#endif
	}
#endif

	if (camera_class) {
		if (camera_front_dev)
			device_destroy(camera_class, camera_front_dev->devt);

		if (camera_rear_dev)
			device_destroy(camera_class, camera_rear_dev->devt);

#ifdef CONFIG_OIS_USE
		if (camera_ois_dev)
			device_destroy(camera_class, camera_ois_dev->devt);
#endif
#ifdef CONFIG_SECURE_CAMERA_USE
		if (camera_secure_dev)
			device_destroy(camera_class, camera_secure_dev->devt);
#endif
	}

	class_destroy(camera_class);

	return 0;
}
