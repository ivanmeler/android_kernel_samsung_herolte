/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <exynos-fimc-is-module.h>
#include "fimc-is-binary.h"
#include "fimc-is-vender.h"
#include "fimc-is-vender-specific.h"
#include "fimc-is-core.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-dt.h"
#include "fimc-is-sysfs.h"

#if defined(CONFIG_OIS_USE)
#include "fimc-is-device-ois.h"
#endif
#include "fimc-is-device-preprocessor.h"
#include "fimc-is-i2c.h"

extern int fimc_is_create_sysfs(struct fimc_is_core *core);
extern bool crc32_check;
extern bool crc32_header_check;
extern bool crc32_check_front;
extern bool crc32_header_check_front;
extern bool is_dumped_fw_loading_needed;
extern bool force_caldata_dump;

static u32  rear_sensor_id;
static u32  front_sensor_id;
static bool check_sensor_vendor;
static bool skip_cal_loading;
static bool use_ois_hsi2c;
static bool use_ois;
static bool use_module_check;
static bool is_hw_init_running = false;
#ifdef CONFIG_SECURE_CAMERA_USE
static u32  secure_sensor_id;
#endif

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
struct workqueue_struct *sensor_pwr_ctrl_wq = 0;
#define CAMERA_WORKQUEUE_MAX_WAITING	1000
#endif

#ifdef USE_CAMERA_HW_BIG_DATA
static struct cam_hw_param_collector cam_hwparam_collector;

void fimc_is_sec_init_err_cnt_file(struct cam_hw_param *hw_param)
{
	if (hw_param) {
		memset(hw_param, 0, sizeof(struct cam_hw_param));
	}
}

int fimc_is_sec_get_rear_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.rear_hwparam;
	return 0;
}

int fimc_is_sec_get_front_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.front_hwparam;
	return 0;
}

int fimc_is_sec_get_iris_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.iris_hwparam;
	return 0;
}

bool fimc_is_sec_is_valid_moduleid(char *moduleid)
{
	int i = 0;
	
	if (moduleid == NULL || strlen(moduleid) < 5) {
		goto err;
	}
	
	for (i = 0; i < 5; i++)
	{
		if (!((moduleid[i] > 47 && moduleid[i] < 58) || // 0 to 9
			(moduleid[i] > 64 && moduleid[i] < 91))) {  // A to Z
			goto err;
		}
	}
	
	return true;
	
err:
	warn("invalid moduleid\n");
	return false;
}
#endif

int fimc_is_vender_probe(struct fimc_is_vender *vender)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;

	BUG_ON(!vender);

	core = container_of(vender, struct fimc_is_core, vender);
	snprintf(vender->fw_path, sizeof(vender->fw_path), "%s", FIMC_IS_FW_SDCARD);
	snprintf(vender->request_fw_path, sizeof(vender->request_fw_path), "%s", FIMC_IS_FW);

	specific = (struct fimc_is_vender_specific *)kmalloc(sizeof(struct fimc_is_vender_specific), GFP_KERNEL);

	/* init mutex for spi read */
	mutex_init(&specific->spi_lock);
	specific->running_front_camera = false;
	specific->running_rear_camera = false;

	specific->retention_data.firmware_size = 0;
	memset(&specific->retention_data.firmware_crc32, 0, FIMC_IS_COMPANION_CRC_SIZE);

#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
	SET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_OFF);
	info("%s: COMPANION STATE %u\n", __func__, GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION));
#endif

#ifdef CONFIG_COMPANION_DCDC_USE
	/* Init companion dcdc */
	comp_pmic_init(&specific->companion_dcdc, NULL);
#endif

	if (fimc_is_create_sysfs(core)) {
		probe_err("fimc_is_create_sysfs is failed");
		ret = -EINVAL;
		goto p_err;
	}

	specific->rear_sensor_id = rear_sensor_id;
	specific->front_sensor_id = front_sensor_id;
	specific->check_sensor_vendor = check_sensor_vendor;
	specific->use_ois = use_ois;
	specific->use_ois_hsi2c = use_ois_hsi2c;
	specific->use_module_check = use_module_check;
	specific->skip_cal_loading = skip_cal_loading;
	specific->eeprom_client0 = NULL;
	specific->eeprom_client1 = NULL;
	specific->suspend_resume_disable = false;
	specific->need_cold_reset = false;
#ifdef CONFIG_SENSOR_RETENTION_USE
	specific->need_retention_init = true;
#endif
#ifdef CONFIG_SECURE_CAMERA_USE
	specific->secure_sensor_id = secure_sensor_id;
#endif
	vender->private_data = specific;

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
	if (!sensor_pwr_ctrl_wq) {
		sensor_pwr_ctrl_wq = create_singlethread_workqueue("sensor_pwr_ctrl");
	}
#endif

p_err:
	return ret;
}

#ifdef CAMERA_SYSFS_V2
static int parse_sysfs_caminfo(struct device_node *np,
				struct fimc_is_cam_info *cam_infos, int camera_num)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(np, "isp", cam_infos[camera_num].isp);
	DT_READ_U32(np, "cal_memory", cam_infos[camera_num].cal_memory);
	DT_READ_U32(np, "read_version", cam_infos[camera_num].read_version);
	DT_READ_U32(np, "core_voltage", cam_infos[camera_num].core_voltage);
	DT_READ_U32(np, "upgrade", cam_infos[camera_num].upgrade);
	DT_READ_U32(np, "fw_write", cam_infos[camera_num].fw_write);
	DT_READ_U32(np, "fw_dump", cam_infos[camera_num].fw_dump);
	DT_READ_U32(np, "companion", cam_infos[camera_num].companion);
	DT_READ_U32(np, "ois", cam_infos[camera_num].ois);

	return 0;
}
#endif

int fimc_is_vender_dt(struct device_node *np)
{
	int ret = 0;
#ifdef CAMERA_SYSFS_V2
	struct device_node *camInfo_np;
	struct fimc_is_cam_info *camera_infos;
	char camInfo_string[15];
	int camera_num;
	int total_camera_num;
#endif

	ret = of_property_read_u32(np, "rear_sensor_id", &rear_sensor_id);
	if (ret) {
		probe_err("rear_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(np, "front_sensor_id", &front_sensor_id);
	if (ret) {
		probe_err("front_sensor_id read is fail(%d)", ret);
	}

#ifdef CONFIG_SECURE_CAMERA_USE
	ret = of_property_read_u32(np, "secure_sensor_id", &secure_sensor_id);
	if (ret) {
		probe_err("secure_sensor_id read is fail(%d)", ret);
		secure_sensor_id = 0;
	}
#endif

	check_sensor_vendor = of_property_read_bool(np, "check_sensor_vendor");
	if (!check_sensor_vendor) {
		probe_info("check_sensor_vendor not use(%d)\n", check_sensor_vendor);
	}

	use_ois = of_property_read_bool(np, "use_ois");
	if (!use_ois) {
		probe_err("use_ois not use(%d)", use_ois);
	}

	use_ois_hsi2c = of_property_read_bool(np, "use_ois_hsi2c");
	if (!use_ois_hsi2c) {
		probe_err("use_ois_hsi2c not use(%d)", use_ois_hsi2c);
	}

	use_module_check = of_property_read_bool(np, "use_module_check");
	if (!use_module_check) {
		probe_err("use_module_check not use(%d)", use_module_check);
	}

	skip_cal_loading = of_property_read_bool(np, "skip_cal_loading");
	if (!skip_cal_loading) {
		probe_info("skip_cal_loading not use(%d)\n", skip_cal_loading);
	}

#ifdef CAMERA_SYSFS_V2
	ret = of_property_read_u32(np, "total_camera_num", &total_camera_num);
	if (ret) {
		err("total_camera_num read is fail(%d)", ret);
		total_camera_num = 0;
	}
	fimc_is_get_cam_info(&camera_infos);

	for (camera_num = 0; camera_num < total_camera_num; camera_num++) {
		sprintf(camInfo_string, "%s%d", "camera_info", camera_num);

		camInfo_np = of_find_node_by_name(np, camInfo_string);
		if (!camInfo_np) {
			printk(KERN_ERR "%s: can't find camInfo_string node\n", __func__);
			return -ENOENT;
		}
		parse_sysfs_caminfo(camInfo_np, camera_infos, camera_num);
	}
#endif

	return ret;
}

bool fimc_is_vender_check_sensor(struct fimc_is_core *core)
{
	int i = 0;
	bool ret = false;
	int retry_count = 20;

	do {
		ret = false;
		for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
			if (!test_bit(FIMC_IS_SENSOR_PROBE, &core->sensor[i].state)) {
				ret = true;
				break;
			}
		}

		if (i == FIMC_IS_SENSOR_COUNT && ret == false) {
			info("Retry count = %d\n", retry_count);
			break;
		}

		mdelay(100);
		if (retry_count > 0) {
			--retry_count;
		} else {
			err("Could not get sensor before start ois fw update routine.\n");
			break;
		}
	} while (ret);

	return ret;
}

void fimc_is_vender_check_hw_init_running(void)
{
	int retry = 50;

	do {
		if (!is_hw_init_running) {
			break;
		}
		--retry;
		msleep(100);
	} while (retry > 0);

	if (retry <= 0) {
		err("HW init is not completed.");
	}

	return;
}

int fimc_is_vender_hw_init(struct fimc_is_vender *vender)
{
	bool ret = false;
	struct device *dev  = NULL;
	struct fimc_is_core *core;

	core = container_of(vender, struct fimc_is_core, vender);
	dev = &core->ischain[0].pdev->dev;

	is_hw_init_running = true;
	ret = fimc_is_vender_check_sensor(core);
	if (ret) {
		err("Do not init hw routine. Check sensor failed!\n");
		is_hw_init_running = false;
		return -EINVAL;
	} else {
		info("Start hw init. Check sensor success!\n");
	}

	ret = fimc_is_sec_run_fw_sel(dev, SENSOR_POSITION_REAR);
	if (ret) {
		err("fimc_is_sec_run_fw_sel for rear is fail(%d)", ret);
	}

	ret = fimc_is_sec_run_fw_sel(dev, SENSOR_POSITION_FRONT);
	if (ret) {
		err("fimc_is_sec_run_fw_sel for front is fail(%d)", ret);
	}

#ifdef CONFIG_COMPANION_USE
	ret = fimc_is_sec_concord_fw_sel(core, dev);
	if (ret) {
		err("fimc_is_sec_concord_fw_sel is fail(%d)", ret);
	}
#endif

#ifdef CONFIG_OIS_USE
	fimc_is_ois_fw_update(core);
#endif

	is_hw_init_running = false;

	return 0;
}

int fimc_is_vender_fw_prepare(struct fimc_is_vender *vender)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_device_preproc *device;

	BUG_ON(!vender);

	core = container_of(vender, struct fimc_is_core, vender);
	device = &core->preproc;

	ret = fimc_is_sec_run_fw_sel(&device->pdev->dev, SENSOR_POSITION_REAR);
	if (core->current_position == SENSOR_POSITION_REAR) {
		if (ret < 0) {
			err("fimc_is_sec_run_fw_sel is fail1(%d)", ret);
			goto p_err;
		}
	}

	ret = fimc_is_sec_run_fw_sel(&device->pdev->dev, SENSOR_POSITION_FRONT);
	if (core->current_position == SENSOR_POSITION_FRONT) {
		if (ret < 0) {
			err("fimc_is_sec_run_fw_sel is fail2(%d)", ret);
			goto p_err;
		}
	}

#ifdef CONFIG_COMPANION_USE
	ret = fimc_is_sec_concord_fw_sel(core, &device->pdev->dev);
	if (ret) {
		err("fimc_is_sec_concord_fw_sel is fail(%d)", ret);
		goto p_err;
	}

	/* TODO: loading firmware */
	fimc_is_s_int_comb_isp(core, false, INTMR2_INTMCIS22);
#endif

	/* Set SPI function */
	fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_ISP_FW);

p_err:
	return ret;
}

int fimc_is_vender_fw_filp_open(struct fimc_is_vender *vender, struct file **fp, int bin_type)
{
	int ret = FW_SKIP;
	struct fimc_is_from_info *sysfs_finfo;
	char fw_path[FIMC_IS_PATH_LEN];
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
	struct fimc_is_core *core;
	core = container_of(vender, struct fimc_is_core, vender);
#endif

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
	memset(fw_path, 0x00, sizeof(fw_path));

	if (bin_type == FIMC_IS_BIN_FW) {
		if (is_dumped_fw_loading_needed) {
			snprintf(fw_path, sizeof(fw_path),
					"%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo->load_fw_name);
			*fp = filp_open(fw_path, O_RDONLY, 0);
			if (IS_ERR_OR_NULL(*fp)) {
				*fp = NULL;
				ret = FW_FAIL;
			} else {
				ret = FW_SUCCESS;
			}
		} else {
			ret = FW_SKIP;
		}
	} else if (bin_type == FIMC_IS_BIN_SETFILE) {
		if (is_dumped_fw_loading_needed) {
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
			if (core->current_position == SENSOR_POSITION_FRONT) {
				snprintf(fw_path, sizeof(fw_path),
					"%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo->load_front_setfile_name);
			} else
#endif
			{
				snprintf(fw_path, sizeof(fw_path),
					"%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo->load_setfile_name);
			}
			*fp = filp_open(fw_path, O_RDONLY, 0);
			if (IS_ERR_OR_NULL(*fp)) {
				*fp = NULL;
				ret = FW_FAIL;
			} else {
				ret = FW_SUCCESS;
			}
		} else {
			ret = FW_SKIP;
		}
	}

	return ret;
}

#ifdef CONFIG_COMPANION_USE
int fimc_is_vender_preproc_fw_load(struct fimc_is_vender *vender)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_device_preproc *device;
#if defined(CONFIG_PREPROCESSOR_STANDBY_USE)
	struct fimc_is_vender_specific *specific;
#endif
	BUG_ON(!vender);

#if defined(CONFIG_PREPROCESSOR_STANDBY_USE)
	specific = vender->private_data;
#endif
	core = container_of(vender, struct fimc_is_core, vender);
	device = &core->preproc;

	/* Set pin function to ISP I2C for Host to use I2C0 */
	/* In case of S/W I2c, pin direction should be set input direction. */
	/* if pin direction doesn't set input direction, cause timing issue on start condition.*/
	fimc_is_i2c_s_pin(core->client0, I2C_PIN_STATE_HOST);

	/* Set SPI function */
	fimc_is_spi_s_pin(&core->spi1, SPI_PIN_STATE_HOST);

	if (fimc_is_comp_is_valid(core) == 0) {
#if defined(CONFIG_PREPROCESSOR_STANDBY_USE)
		if (force_caldata_dump == false) {
			if (GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION) == SENSOR_STATE_OFF) {
				ret = fimc_is_comp_loadfirm(core);
			} else {
				ret = fimc_is_comp_retention(core);
				if (ret == -EINVAL) {
					info("companion restart..\n");
					ret = fimc_is_comp_loadfirm(core);
				}
			}
			SET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_ON);
			info("%s: COMPANION STATE %u\n", __func__, GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION));
 		} else {
			ret = fimc_is_comp_loadfirm(core);
		}
#else
		ret = fimc_is_comp_loadfirm(core);
#endif
		if (ret) {
			err("fimc_is_comp_loadfirm() fail");
			goto p_err;
		}

#ifdef CONFIG_COMPANION_DCDC_USE
#if defined(CONFIG_COMPANION_C2_USE) || defined(CONFIG_COMPANION_C3_USE)
		if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT1)
#endif
		{
			if (specific->companion_dcdc.type == DCDC_VENDOR_PMIC) {
				if (!fimc_is_sec_file_exist(FIMC_IS_ISP_CV))
					fimc_is_power_binning(core);
			} else {
				fimc_is_power_binning(core);
			}
		}
#endif /* CONFIG_COMPANION_DCDC_USE*/

		ret = fimc_is_comp_loadsetf(core);
		if (ret) {
			err("fimc_is_comp_loadsetf() fail");
			goto p_err;
		}
	}

	fimc_is_i2c_s_pin(core->client0, I2C_PIN_STATE_FW);

#if defined(CONFIG_OIS_USE)
	if(specific->use_ois && core->current_position == SENSOR_POSITION_REAR) {
		if (!specific->use_ois_hsi2c) {
			fimc_is_i2c_s_pin(core->client1, I2C_PIN_STATE_HOST);
		}

		if (!specific->ois_ver_read) {
			fimc_is_ois_check_fw(core);
		}

		fimc_is_ois_exif_data(core);

		if (!specific->use_ois_hsi2c) {
			fimc_is_i2c_s_pin(core->client1, I2C_PIN_STATE_FW);
		}
	}
#endif

p_err:
	fimc_is_i2c_s_pin(core->client1, I2C_PIN_STATE_OFF);
	fimc_is_spi_s_pin(&core->spi1, SPI_PIN_STATE_ISP_FW);
	return ret;
}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static int fimc_is_ischain_loadcalb_eeprom(struct fimc_is_core *core,
struct fimc_is_module_enum *active_sensor, int position)
{
	int ret = 0;
	char *cal_ptr;
	char *cal_buf = NULL;
	u32 start_addr = 0;
	int cal_size = 0;
	struct fimc_is_from_info *finfo;
	struct fimc_is_from_info *pinfo;
	char *loaded_fw_ver;

	mdbgd_ischain("%s\n", device, __func__);

	if (!fimc_is_sec_check_from_ver(core, position)) {
		err("Camera : Did not load cal data.");
		return 0;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		start_addr = FIMC_IS_CAL_OFFSET1;
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		fimc_is_sec_get_sysfs_finfo_front(&finfo);
		fimc_is_sec_get_front_cal_buf(&cal_buf);
	} else
#endif
	{
		start_addr = FIMC_IS_CAL_OFFSET0;
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		fimc_is_sec_get_sysfs_finfo(&finfo);
		fimc_is_sec_get_cal_buf(&cal_buf);
	}

	fimc_is_sec_get_sysfs_pinfo(&pinfo);
	fimc_is_sec_get_loaded_fw(&loaded_fw_ver);

	cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr + start_addr);

	info("CAL DATA : MAP ver : %c%c%c%c\n", cal_buf[0x40], cal_buf[0x41],
		cal_buf[0x42], cal_buf[0x43]);

	info("Camera : Front Sensor Version : 0x%x\n", cal_buf[0x5C]);

	info("eeprom_fw_version = %s, phone_fw_version = %s, loaded_fw_version = %s\n",
		finfo->header_ver, pinfo->header_ver, loaded_fw_ver);

	/* CRC check */
	if (position == SENSOR_POSITION_FRONT) {
		if (crc32_check_front == true) {
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Front Camera : the dumped Cal. data was applied successfully.\n");
		} else {
			if (crc32_header_check_front == true) {
				err("Front Camera : CRC32 error but only header section is no problem.");
				memset((void *)(cal_ptr + 0x1000), 0xFF, cal_size - 0x1000);
			} else {
				err("Front Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
		}
	} else {
		if (crc32_check == true) {
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Rear Camera : the dumped Cal. data was applied successfully.\n");
		} else {
			if (crc32_header_check == true) {
				err("Rear Camera : CRC32 error but only header section is no problem.");
				memset((void *)(cal_ptr + 0x1000), 0xFF, cal_size - 0x1000);
			} else {
				err("Rear Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
		}
	}

	vb2_ion_sync_for_device(core->resourcemgr.minfo.fw_cookie,
		start_addr, cal_size, DMA_TO_DEVICE);
	if (ret)
		warn("calibration loading is fail");
	else
		info("calibration loading is success\n");

	return ret;
}
#endif

#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
static int fimc_is_ischain_loadcalb(struct fimc_is_core *core,
	struct fimc_is_module_enum *active_sensor, int position)
{
	int ret = 0;
	char *cal_ptr;
	u32 start_addr = 0;
	int cal_size = 0;
	struct fimc_is_from_info *sysfs_finfo;
	struct fimc_is_from_info *sysfs_pinfo;
	char *loaded_fw_ver;
	char *cal_buf;

	if (!fimc_is_sec_check_from_ver(core, position)) {
		err("Camera : Did not load cal data.");
		return 0;
	}

	if (position == SENSOR_POSITION_FRONT) {
		start_addr = FIMC_IS_CAL_OFFSET1;
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		fimc_is_sec_get_sysfs_finfo_front(&sysfs_finfo);
		fimc_is_sec_get_front_cal_buf(&cal_buf);
	} else {
		start_addr = FIMC_IS_CAL_OFFSET0;
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
		fimc_is_sec_get_cal_buf(&cal_buf);
	}

	fimc_is_sec_get_sysfs_pinfo(&sysfs_pinfo);
	fimc_is_sec_get_loaded_fw(&loaded_fw_ver);

	cal_ptr = (char *)(core->resourcemgr.minfo.kvaddr + start_addr);

	info("CAL DATA : MAP ver : %c%c%c%c\n", cal_buf[0x60], cal_buf[0x61],
		cal_buf[0x62], cal_buf[0x63]);

	info("from_fw_version = %s, phone_fw_version = %s, loaded_fw_version = %s\n",
		sysfs_finfo->header_ver, sysfs_pinfo->header_ver, loaded_fw_ver);

	/* CRC check */
	if (position == SENSOR_POSITION_FRONT) {
		if (crc32_check_front  == true) {
#ifdef CONFIG_COMPANION_USE
			if (fimc_is_sec_check_from_ver(core, position)) {
				memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
				info("Camera : the dumped Cal. data was applied successfully.\n");
			} else {
				info("Camera : Did not load dumped Cal. Sensor version is lower than V004.\n");
			}
#else
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Camera : the dumped Cal. data was applied successfully.\n");
#endif
		} else {
			if (crc32_header_check_front  == true) {
				err("Camera : CRC32 error but only header section is no problem.");
				memcpy((void *)(cal_ptr),
					(void *)cal_buf,
					EEP_HEADER_CAL_DATA_START_ADDR);
				memset((void *)(cal_ptr + EEP_HEADER_CAL_DATA_START_ADDR),
					0xFF,
					cal_size - EEP_HEADER_CAL_DATA_START_ADDR);
			} else {
				err("Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
		}
	} else {
		if (crc32_check == true) {
#ifdef CONFIG_COMPANION_USE
			if (fimc_is_sec_check_from_ver(core, position)) {
				memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
				info("Camera : the dumped Cal. data was applied successfully.\n");
			} else {
				info("Camera : Did not load dumped Cal. Sensor version is lower than V004.\n");
			}
#else
			memcpy((void *)(cal_ptr) ,(void *)cal_buf, cal_size);
			info("Camera : the dumped Cal. data was applied successfully.\n");
#endif
		} else {
			if (crc32_header_check == true) {
				err("Camera : CRC32 error but only header section is no problem.");
				memcpy((void *)(cal_ptr),
					(void *)cal_buf,
					FROM_HEADER_CAL_DATA_START_ADDR);
				memset((void *)(cal_ptr + FROM_HEADER_CAL_DATA_START_ADDR),
					0xFF,
					cal_size - FROM_HEADER_CAL_DATA_START_ADDR);
			} else {
				err("Camera : CRC32 error for all section.");
				memset((void *)(cal_ptr), 0xFF, cal_size);
				ret = -EIO;
			}
		}
	}

	vb2_ion_sync_for_device(core->resourcemgr.minfo.fw_cookie,
		FIMC_IS_CAL_OFFSET0, FIMC_IS_MAX_CAL_SIZE, DMA_TO_DEVICE);
	if (ret)
		warn("calibration loading is fail");
	else
		info("calibration loading is success\n");

	return ret;
}
#endif

int fimc_is_vender_cal_load(struct fimc_is_vender *vender,
	void *module_data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_module_enum *module = module_data;

	core = container_of(vender, struct fimc_is_core, vender);

	if(module->position == SENSOR_POSITION_REAR) {
		/* Load calibration data from sensor */
		module->ext.sensor_con.cal_address = FIMC_IS_CAL_OFFSET0;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		ret = fimc_is_ischain_loadcalb_eeprom(core, NULL, SENSOR_POSITION_REAR);
#else
		ret = fimc_is_ischain_loadcalb(core, NULL, SENSOR_POSITION_REAR);
#endif
		if (ret) {
			err("loadcalb fail, load default caldata\n");
		}
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		module->ext.sensor_con.cal_address = FIMC_IS_CAL_OFFSET1;
		ret = fimc_is_ischain_loadcalb_eeprom(core, NULL, SENSOR_POSITION_FRONT);
		if (ret) {
			err("loadcalb fail, load default caldata\n");
		}
#else
		module->ext.sensor_con.cal_address = 0;
#endif
	}

	return ret;
}

int fimc_is_vender_module_sel(struct fimc_is_vender *vender, void *module_data)
{
	int ret = 0;
	struct fimc_is_module_enum *module = module_data;
	struct fimc_is_vender_specific *specific;

	BUG_ON(!module);

	specific = vender->private_data;

	if (module->position == SENSOR_POSITION_FRONT)
		specific->running_front_camera = true;
	else
		specific->running_rear_camera = true;

	return ret;
}

int fimc_is_vender_module_del(struct fimc_is_vender *vender, void *module_data)
{
	int ret = 0;
	struct fimc_is_module_enum *module = module_data;
	struct fimc_is_vender_specific *specific;

	BUG_ON(!module);

	specific = vender->private_data;

	if (module->position == SENSOR_POSITION_FRONT)
		specific->running_front_camera = false;
	else
		specific->running_rear_camera = false;

	return ret;
}

int fimc_is_vender_fw_sel(struct fimc_is_vender *vender)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct device *dev;
	struct fimc_is_from_info *sysfs_finfo;

	BUG_ON(!vender);

	core = container_of(vender, struct fimc_is_core, vender);
	dev = &core->pdev->dev;

	if (!test_bit(FIMC_IS_PREPROC_S_INPUT, &core->preproc.state)) {
		ret = fimc_is_sec_run_fw_sel(dev, core->current_position);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
			goto p_err;
		}
	}

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
	snprintf(vender->request_fw_path, sizeof(vender->request_fw_path), "%s",
		sysfs_finfo->load_fw_name);

p_err:
	return ret;
}

int fimc_is_vender_setfile_sel(struct fimc_is_vender *vender, char *setfile_name)
{
	int ret = 0;
#if defined(CONFIG_COMPANION_USE) || defined(SELECT_SETFILE_BY_FROM_VERSION)
	struct fimc_is_core *core;
#endif
#ifdef SELECT_SETFILE_BY_FROM_VERSION
	char dst_name[50];
	struct fimc_is_from_info *sysfs_finfo;
	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
#endif

	BUG_ON(!vender);
	BUG_ON(!setfile_name);

#if defined(CONFIG_COMPANION_USE) || defined(SELECT_SETFILE_BY_FROM_VERSION)
	core = container_of(vender, struct fimc_is_core, vender);
#endif
#ifdef CONFIG_COMPANION_USE
	fimc_is_s_int_comb_isp(core, false, INTMR2_INTMCIS22);
#endif

#ifdef SELECT_SETFILE_BY_FROM_VERSION
	if (core->current_position == SENSOR_POSITION_REAR) {
		if (fimc_is_sec_fw_module_compare(sysfs_finfo->header_ver, FW_IMX260_D)) {
			snprintf(dst_name, sizeof(dst_name), "setfile_imx260_d.bin");
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo->header_ver, FW_2L1_D)) {
			snprintf(dst_name, sizeof(dst_name), "setfile_2l1_d.bin");
		} else {
			snprintf(dst_name, sizeof(dst_name), setfile_name);
		}
	} else {
		if (fimc_is_sec_fw_module_compare(sysfs_finfo->header_ver, FW_IMX260_D) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo->header_ver, FW_2L1_D)) {
			snprintf(dst_name, sizeof(dst_name), "setfile_4e6_d.bin");
		} else {
			snprintf(dst_name, sizeof(dst_name), setfile_name);
		}
	}

	snprintf(vender->setfile_path, sizeof(vender->setfile_path), "%s%s",
		FIMC_IS_SETFILE_SDCARD_PATH, setfile_name);
	snprintf(vender->request_setfile_path, sizeof(vender->request_setfile_path), "%s",
		dst_name);
#else
	snprintf(vender->setfile_path, sizeof(vender->setfile_path), "%s%s",
		FIMC_IS_SETFILE_SDCARD_PATH, setfile_name);
	snprintf(vender->request_setfile_path, sizeof(vender->request_setfile_path), "%s",
		setfile_name);
#endif

	return ret;
}

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
void sensor_pwr_ctrl(struct work_struct *work)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *pdata;
	struct fimc_is_module_enum *g_module = NULL;
	struct fimc_is_core *core;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return;
	}

	ret = fimc_is_preproc_g_module(&core->preproc, &g_module);
	if (ret) {
		err("fimc_is_sensor_g_module is fail(%d)", ret);
		return;
	}

	pdata = g_module->pdata;
	ret = pdata->gpio_cfg(g_module->dev, SENSOR_SCENARIO_NORMAL,
		GPIO_SCENARIO_STANDBY_OFF_SENSOR);
	if (ret) {
		err("gpio_cfg(sensor) is fail(%d)", ret);
	}
}

static DECLARE_DELAYED_WORK(sensor_pwr_ctrl_work, sensor_pwr_ctrl);
#endif

#ifdef CONFIG_COMPANION_USE
int fimc_is_vender_preprocessor_gpio_on_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario)
{
	int ret = 0;
	struct fimc_is_core *core;

#ifdef CONFIG_COMPANION_DCDC_USE
	struct dcdc_power *dcdc;
	const char *vout_str = NULL;
	int vout = 0;
#endif
#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
	int waitWorkqueue;
	struct exynos_platform_fimc_is_module *pdata;
	struct fimc_is_module_enum *module;
#endif
	struct fimc_is_vender_specific *specific;
	specific = vender->private_data;

	core = container_of(vender, struct fimc_is_core, vender);

	/* Set spi pin to out */
	if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1)
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE_INPU);
	else if (specific->rear_sensor_id == SENSOR_NAME_IMX260)
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE_INPD);
	else
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE);

	fimc_is_spi_s_pin(&core->spi1, SPI_PIN_STATE_IDLE);

#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
	if (scenario == SENSOR_SCENARIO_NORMAL) {
		if (GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION) == SENSOR_STATE_STANDBY) {
			*gpio_scenario = GPIO_SCENARIO_STANDBY_OFF;
#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
			queue_delayed_work(sensor_pwr_ctrl_wq, &sensor_pwr_ctrl_work, 0);
#endif
		}
	}
#endif

#ifdef CONFIG_COMPANION_DCDC_USE
	dcdc = &specific->companion_dcdc;

	if (dcdc->type == DCDC_VENDOR_PMIC) {
		/* Set default voltage without power binning if FIMC_IS_ISP_CV not exist. */
		if (!fimc_is_sec_file_exist(FIMC_IS_ISP_CV)) {
			info("Companion file not exist (%s), version : %X\n", FIMC_IS_ISP_CV, fimc_is_comp_get_ver());

			/* Get default vout in power binning table if EVT1 */
			if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT1) {
				vout = dcdc->get_vout_val(0);
				vout_str = dcdc->get_vout_str(0);
			/* If not, get default vout for both EVT0 and EVT1 */
			} else {
				if (dcdc->get_default_vout_val(&vout, &vout_str))
					err("fail to get companion default vout");
			}

			info("Companion: Set default voltage %sV\n", vout_str ? vout_str : "0");
			dcdc->set_vout(dcdc->client, vout);
		/* Do power binning if FIMC_IS_ISP_CV exist with PMIC */
		} else {
			fimc_is_power_binning(core);
		}
	}

#else /* !CONFIG_COMPANION_DCDC_USE*/
	/* Temporary Fixes. Set voltage to 0.85V for EVT0, 0.8V for EVT1 */
	if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT1) {
		info("%s: Companion EVT1. Set voltage 0.8V\n", __func__);
		ret = fimc_is_comp_set_voltage("VDDD_CORE_0.8V_COMP", 800000);
	} else if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT0) {
		info("%s: Companion EVT0. Set voltage %sV\n", __func__, COMP_DEFAULT_VOUT_STR);
		ret = fimc_is_comp_set_voltage("VDDD_CORE_0.8V_COMP", COMP_DEFAULT_VOUT_VAL);
	} else {
		info("%s: Companion unknown rev. Set default voltage %sV\n", __func__, COMP_DEFAULT_VOUT_STR);
		ret = fimc_is_comp_set_voltage("VDDD_CORE_0.8V_COMP", COMP_DEFAULT_VOUT_VAL);
	}
	if (ret < 0) {
		err("Companion core_0.8v setting fail!");
	}
#endif /* CONFIG_COMPANION_DCDC_USE */

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
	if (*gpio_scenario == GPIO_SCENARIO_STANDBY_OFF) {
		ret = fimc_is_preproc_g_module(&core->preproc, &module);
		if (ret) {
			err("fimc_is_sensor_g_module is fail(%d)", ret);
			goto p_err;
		}

		pdata = module->pdata;
		ret = pdata->gpio_cfg(module->dev, scenario, GPIO_SCENARIO_STANDBY_OFF_PREPROCESSOR);
		if (ret) {
			clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg(companion) is fail(%d)", ret);
			goto p_err;
		}

		waitWorkqueue = 0;
		/* Waiting previous workqueue */
		while (work_busy(&sensor_pwr_ctrl_work.work) &&
			waitWorkqueue < CAMERA_WORKQUEUE_MAX_WAITING) {
			if (!(waitWorkqueue % 100))
				info("Waiting Sensor power sequence...\n");
			usleep_range(100, 100);
			waitWorkqueue++;
		}
		info("workQueue is waited %d times\n", waitWorkqueue);
	}

p_err:
#endif

	return ret;
}
#endif

int fimc_is_vender_preprocessor_gpio_on(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
	return ret;
}

int fimc_is_vender_sensor_gpio_on_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario)
{
	int ret = 0;
#if 0
	struct fimc_is_core *core;
	core = container_of(vender, struct fimc_is_core, vender);

	/* In dual camera scenario,
	while loading cal data to C3 with spi in rear camera, changing spi config in front camera is not valid.
	Due to this issue, disable spi config here. (C3 + spi0, spi1 use case in rear camera)
	Need to consider this on other project later depending on spi use cases.
	*/
	/* Set spi pin to out */
	if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1)
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE_INPU);
	else if (specific->rear_sensor_id == SENSOR_NAME_IMX260)
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE_INPD);
	else
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE);

	fimc_is_spi_s_pin(&core->spi1, SPI_PIN_STATE_IDLE);
#endif

	return ret;
}

int fimc_is_vender_sensor_gpio_on(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
	return ret;
}

int fimc_is_vender_preprocessor_gpio_off_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario,
			void *module_data)
{
	int ret = 0;
	struct fimc_is_core *core;
#if defined(CONFIG_OIS_USE)	
	struct fimc_is_module_enum *module = module_data;
#endif
#ifdef CONFIG_SENSOR_RETENTION_USE
	struct fimc_is_from_info *sysfs_finfo;
#endif
	struct fimc_is_vender_specific *specific;
	specific = vender->private_data;

	core = container_of(vender, struct fimc_is_core, vender);

	/* Set spi pin to out */
	if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1)
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE_INPU);
	else if (specific->rear_sensor_id == SENSOR_NAME_IMX260)
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE_INPD);
	else
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE);

	fimc_is_spi_s_pin(&core->spi1, SPI_PIN_STATE_IDLE);

#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
	if (scenario == SENSOR_SCENARIO_NORMAL) {
#if defined(CONFIG_COMPANION_C2_USE)
		if (fimc_is_comp_get_ver() == FIMC_IS_COMPANION_VERSION_EVT1)
#endif
		{
			if (GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION) == SENSOR_STATE_ON) {
#ifdef CONFIG_SENSOR_RETENTION_USE
				fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
				if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1 && sysfs_finfo->sensor_version >= 0xC0
					&& force_caldata_dump == false) {
					*gpio_scenario = GPIO_SCENARIO_SENSOR_RETENTION_ON;

#if defined(CONFIG_OIS_USE)					
					/* Enable OIS gyro sleep */
					if (module->position == SENSOR_POSITION_REAR) {
						fimc_is_ois_gyro_sleep(core);
					}
#endif
					/* Set i2c pin to default */
					fimc_is_i2c_s_pin(core->client0, I2C_PIN_STATE_DEFAULT);
					fimc_is_i2c_s_pin(core->client1, I2C_PIN_STATE_DEFAULT);
				} else
#endif
				{
					*gpio_scenario = GPIO_SCENARIO_STANDBY_ON;
				}
			}
		}
	}
#endif

	return ret;
}

int fimc_is_vender_preprocessor_gpio_off(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
	struct fimc_is_vender_specific *specific;

	specific = vender->private_data;

	if (scenario == SENSOR_SCENARIO_NORMAL) {
		if (gpio_scenario == GPIO_SCENARIO_STANDBY_ON
#ifdef CONFIG_SENSOR_RETENTION_USE
			|| gpio_scenario == GPIO_SCENARIO_SENSOR_RETENTION_ON
#endif
			) {
			SET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_STANDBY);
		} else if (gpio_scenario == GPIO_SCENARIO_OFF) {
			SET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION, SENSOR_STATE_OFF);
		}
		info("%s: COMPANION STATE %u\n", __func__, GET_SENSOR_STATE(specific->standby_state, SENSOR_STATE_COMPANION));
	}
#endif

	return ret;
}

int fimc_is_vender_sensor_gpio_off_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario)
{
	int ret = 0;

#if 0
	struct fimc_is_core *core;
	core = container_of(vender, struct fimc_is_core, vender);

	/* In dual camera scenario,
	while loading cal data to C3 with spi in rear camera, changing spi config in front camera is not valid.
	Due to this issue, disable spi config here. (C3 + spi0, spi1 use case in rear camera)
	Need to consider this on other project later depending on spi use cases.
	*/
	/* Set spi pin to out */
	if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1)
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE_INPU);
	else if (specific->rear_sensor_id == SENSOR_NAME_IMX260)
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE_INPD);
	else
		fimc_is_spi_s_pin(&core->spi0, SPI_PIN_STATE_IDLE);

	fimc_is_spi_s_pin(&core->spi1, SPI_PIN_STATE_IDLE);
#endif

	return ret;
}

int fimc_is_vender_sensor_gpio_off(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;

	return ret;
}

void fimc_is_vender_itf_open(struct fimc_is_vender *vender, struct sensor_open_extended *ext_info)
{
	struct fimc_is_vender_specific *specific;
	struct fimc_is_from_info *sysfs_finfo;
	struct fimc_is_core *core;

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
	specific = vender->private_data;
	core = container_of(vender, struct fimc_is_core, vender);

#ifdef CONFIG_SENSOR_RETENTION_USE
	if (((specific->rear_sensor_id == SENSOR_NAME_IMX260 && sysfs_finfo->sensor_version >= 0x06)
#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
		|| (specific->rear_sensor_id == SENSOR_NAME_S5K2L1 && sysfs_finfo->sensor_version >= 0xC0)
#endif
		)	
		&& (force_caldata_dump == false)
		&& (core->current_position == SENSOR_POSITION_REAR)
	) {
		if (specific->need_retention_init) {
			ext_info->use_retention_mode = SENSOR_RETENTION_READY;
			info("Sensor[id = %d, version = 0x%02x] Set retention mode ready.\n",
				specific->rear_sensor_id, sysfs_finfo->sensor_version);
			specific->need_retention_init = false;
		} else {
			ext_info->use_retention_mode = SENSOR_RETENTION_USE;
			info("Sensor[id = %d, version = 0x%02x] Set retention mode use.\n",
				specific->rear_sensor_id, sysfs_finfo->sensor_version);
		}
	} else
#endif
	{
		ext_info->use_retention_mode = SENSOR_RETENTION_DISABLE;
		if (core->current_position == SENSOR_POSITION_REAR) {
			info("Sensor[id = %d, version = 0x%02x] does not support retention mode.\n",
			specific->rear_sensor_id, sysfs_finfo->sensor_version);
		} else {
			info("Front camera does not support retention mode.\n");
		}
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	fimc_is_i2c_s_pin(specific->eeprom_client0, I2C_PIN_STATE_OFF);
#endif
	fimc_is_i2c_s_pin(core->client0, I2C_PIN_STATE_FW);
	if (specific->use_ois) {
		fimc_is_i2c_s_pin(core->client1, I2C_PIN_STATE_OFF);
	}

	return;
}

/* Flash Mode Control */
#ifdef CONFIG_LEDS_LM3560
extern int lm3560_reg_update_export(u8 reg, u8 mask, u8 data);
#endif
#ifdef CONFIG_LEDS_SKY81296
extern int sky81296_torch_ctrl(int state);
#endif
#if defined(CONFIG_TORCH_CURRENT_CHANGE_SUPPORT) && defined(CONFIG_LEDS_S2MPB02)
extern int s2mpb02_set_torch_current(bool movie);
#endif

int fimc_is_vender_set_torch(u32 aeflashMode)
{
	switch (aeflashMode) {
	case AA_FLASHMODE_ON_ALWAYS: /*TORCH mode*/
#ifdef CONFIG_LEDS_LM3560
		lm3560_reg_update_export(0xE0, 0xFF, 0xEF);
#elif defined(CONFIG_LEDS_SKY81296)
		sky81296_torch_ctrl(1);
#endif
#if defined(CONFIG_TORCH_CURRENT_CHANGE_SUPPORT) && defined(CONFIG_LEDS_S2MPB02)
		s2mpb02_set_torch_current(true);
#endif
		break;
	case AA_FLASHMODE_START: /*Pre flash mode*/
#ifdef CONFIG_LEDS_LM3560
		lm3560_reg_update_export(0xE0, 0xFF, 0xEF);
#elif defined(CONFIG_LEDS_SKY81296)
		sky81296_torch_ctrl(1);
#endif
#if defined(CONFIG_TORCH_CURRENT_CHANGE_SUPPORT) && defined(CONFIG_LEDS_S2MPB02)
		s2mpb02_set_torch_current(false);
#endif
		break;
	case AA_FLASHMODE_CAPTURE: /*Main flash mode*/
		break;
	case AA_FLASHMODE_OFF: /*OFF mode*/
#ifdef CONFIG_LEDS_SKY81296
		sky81296_torch_ctrl(0);
#endif
		break;
	default:
		break;
	}

	return 0;
}

int fimc_is_vender_video_s_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = (struct fimc_is_device_ischain *)device_data;
	struct fimc_is_core *core;
	struct fimc_is_vender_specific *specific;
	unsigned int value = 0;
	unsigned int captureIntent = 0;
	unsigned int captureCount = 0;

	BUG_ON(!device);
	BUG_ON(!ctrl);

	core = (struct fimc_is_core *)platform_get_drvdata(device->pdev);
	specific = core->vender.private_data;

	switch (ctrl->id) {
	case V4L2_CID_IS_INTENT:
		ctrl->id = VENDER_S_CTRL;
		value = (unsigned int)ctrl->value;
		captureIntent = (value >> 16) & 0x0000FFFF;
		if (captureIntent == AA_CAPTRUE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT
			|| captureIntent == AA_CAPTRUE_INTENT_STILL_CAPTURE_OIS_DYNAMIC_SHOT
			|| captureIntent == AA_CAPTRUE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT) {
			captureCount = value & 0x0000FFFF;
		} else {
			captureIntent = ctrl->value;
			captureCount = 0;
		}
		device->group_3aa.intent_ctl.captureIntent = captureIntent;
		device->group_3aa.intent_ctl.vendor_captureCount = captureCount;
		minfo("[VENDER] s_ctrl intent(%d) count(%d)\n", device, captureIntent, captureCount);
		break;
	case V4L2_CID_IS_CAPTURE_EXPOSURETIME:
		ctrl->id = VENDER_S_CTRL;
		device->group_3aa.intent_ctl.vendor_captureExposureTime = ctrl->value;
		minfo("[VENDER] s_ctrl vendor_captureExposureTime(%d)\n", device, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_TYPE:
		ctrl->id = VENDER_S_CTRL;
		switch (ctrl->value) {
		case IS_COLD_BOOT:
			/* change value to X when !TWIZ | front */
			fimc_is_itf_fwboot_init(device->interface);
			break;
		case IS_WARM_BOOT:
			if (specific ->need_cold_reset) {
				minfo("[VENDER] FW first launching mode for reset\n", device);
				device->interface->fw_boot_mode = FIRST_LAUNCHING;
			} else {
				/* change value to X when TWIZ & back | frist time back camera */
				if (!test_bit(IS_IF_LAUNCH_FIRST, &device->interface->launch_state))
					device->interface->fw_boot_mode = FIRST_LAUNCHING;
				else
					device->interface->fw_boot_mode = WARM_BOOT;
			}
			break;
		case IS_COLD_RESET:
			specific ->need_cold_reset = true;
			minfo("[VENDER] need cold reset!!!\n", device);
			break;
		default:
			err("[VENDER]unsupported ioctl(0x%X)", ctrl->id);
			ret = -EINVAL;
			break;
		}
		break;
#ifdef CONFIG_SENSOR_RETENTION_USE
	case V4L2_CID_IS_PREVIEW_STATE:
		ctrl->id = VENDER_S_CTRL;
#if 0 /* Do not control error state at Host side. Controled by Firmware */
		specific->need_retention_init = true;
		err("[VENDER]  need_retention_init = %d\n", specific->need_retention_init);
#endif
		break;
#endif
	}

	return ret;
}
