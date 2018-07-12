/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-sec-define.h"
#include "fimc-is-vender-specific.h"

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
#include <linux/i2c.h>
#include "fimc-is-device-eeprom.h"
#endif

bool crc32_fw_check = true;
bool crc32_setfile_check = true;
bool crc32_front_setfile_check = true;
bool crc32_check = true;
bool crc32_check_factory = true;
bool crc32_header_check = true;
bool crc32_header_check_front = true;
bool crc32_check_factory_front = true;
bool fw_version_crc_check = true;
bool is_latest_cam_module = false;
bool is_latest_cam_module_front = false;
bool is_final_cam_module = false;
#if defined(CONFIG_SOC_EXYNOS5433)
bool is_right_prj_name = true;
#endif
#ifdef CONFIG_COMPANION_USE
bool crc32_c1_fw_check = true;
bool crc32_c1_check = true;
bool crc32_c1_check_factory = true;
bool companion_lsc_isvalid = false;
bool companion_coef_isvalid = false;
#if defined(CONFIG_COMPANION_C2_USE) || defined(CONFIG_COMPANION_C3_USE)
bool crc32_c1_check_front = true;
bool companion_front_lsc_isvalid = false;
#endif
#endif

#define FIMC_IS_DEFAULT_CAL_SIZE	(20 * 1024)
#define FIMC_IS_DUMP_CAL_SIZE	(172 * 1024)
#define FIMC_IS_LATEST_FROM_VERSION_M	'M'

//static bool is_caldata_read = false;
//static bool is_c1_caldata_read = false;
bool force_caldata_dump = false;

static int cam_id = CAMERA_SINGLE_REAR;
bool is_dumped_fw_loading_needed = false;
bool is_dumped_c1_fw_loading_needed = false;
char fw_core_version;
//struct class *camera_class;
//struct device *camera_front_dev; /*sys/class/camera/front*/
//struct device *camera_rear_dev; /*sys/class/camera/rear*/
static struct fimc_is_from_info sysfs_finfo;
static struct fimc_is_from_info sysfs_pinfo;
bool crc32_check_front = true;
bool is_final_cam_module_front = false;
static struct fimc_is_from_info sysfs_finfo_front;
static struct fimc_is_from_info sysfs_pinfo_front;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static char cal_buf_front[FIMC_IS_MAX_CAL_SIZE_FRONT];
#endif

static char cal_buf[FIMC_IS_MAX_CAL_SIZE];
#ifdef CAMERA_MODULE_DUALIZE
static char fw_buf[FIMC_IS_MAX_FW_BUFFER_SIZE];
#endif
char loaded_fw[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
char loaded_companion_fw[30] = {0, };

#ifdef CONFIG_COMPANION_FACTORY_VALIDATION
int comp_fac_i2c_check = 0x00;
u16 comp_fac_valid_check = 0x00;
#endif

bool fimc_is_sec_get_force_caldata_dump(void)
{
	return force_caldata_dump;
}

int fimc_is_sec_set_force_caldata_dump(bool fcd)
{
	force_caldata_dump = fcd;
	if (fcd)
		info("forced caldata dump enabled!!\n");
	return 0;
}

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo)
{
	*finfo = &sysfs_finfo;
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo;
	return 0;
}

int fimc_is_sec_get_sysfs_finfo_front(struct fimc_is_from_info **finfo)
{
	*finfo = &sysfs_finfo_front;
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo_front(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo_front;
	return 0;
}

int fimc_is_sec_get_front_cal_buf(char **buf)
{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	*buf = &cal_buf_front[0];
#else
	*buf = NULL;
#endif
	return 0;
}

int fimc_is_sec_get_cal_buf(char **buf)
{
	*buf = &cal_buf[0];
	return 0;
}

int fimc_is_sec_get_loaded_fw(char **buf)
{
	*buf = &loaded_fw[0];
	return 0;
}

int fimc_is_sec_set_loaded_fw(char *buf)
{
	strncpy(loaded_fw, buf, FIMC_IS_HEADER_VER_SIZE);
	return 0;
}

int fimc_is_sec_get_loaded_c1_fw(char **buf)
{
	*buf = &loaded_companion_fw[0];
	return 0;
}

int fimc_is_sec_set_loaded_c1_fw(char *buf)
{
	strncpy(loaded_companion_fw, buf, FIMC_IS_HEADER_VER_SIZE);
	return 0;
}

int fimc_is_sec_set_camid(int id)
{
	cam_id = id;
	return 0;
}

int fimc_is_sec_get_camid(void)
{
	return cam_id;
}

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name)
{
#if 0
	char buf[1];
	loff_t pos = 0;
	int pixelSize;

	read_data_from_file("/data/CameraID.txt", buf, 1, &pos);
	if (buf[0] == '0')
		cam_id = CAMERA_SINGLE_REAR;
	else if (buf[0] == '1')
		cam_id = CAMERA_SINGLE_FRONT;
	else if (buf[0] == '2')
		cam_id = CAMERA_DUAL_REAR;
	else if (buf[0] == '3')
		cam_id = CAMERA_DUAL_FRONT;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
		snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX135)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
		snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX134)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
		snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
	} else {
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
		if (pixelSize == 13) {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		} else if (pixelSize == 8) {
			snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		}
	}

	if (cam_id == CAMERA_SINGLE_FRONT ||
		cam_id == CAMERA_DUAL_FRONT) {
		snprintf(setf_name, sizeof(FIMC_IS_6B2_SETF), "%s", FIMC_IS_6B2_SETF);
	}
#else
	err("%s: waring, you're calling the disabled func!", __func__);
#endif
	return 0;
}

int fimc_is_sec_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_PUB_YEAR] - 58) * 10000;
	revision = revision + ((int)fw_ver[FW_PUB_MON] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_PUB_NUM] - 48) * 10;
	revision = revision + (int)fw_ver[FW_PUB_NUM + 1] - 48;

	return revision;
}

bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_CORE_VER] != fw_ver2[FW_CORE_VER]
		|| fw_ver1[FW_PIXEL_SIZE] != fw_ver2[FW_PIXEL_SIZE]
		|| fw_ver1[FW_PIXEL_SIZE + 1] != fw_ver2[FW_PIXEL_SIZE + 1]
		|| fw_ver1[FW_ISP_COMPANY] != fw_ver2[FW_ISP_COMPANY]
		|| fw_ver1[FW_SENSOR_MAKER] != fw_ver2[FW_SENSOR_MAKER]) {
		return false;
	}

	return true;
}

u8 fimc_is_sec_compare_ver(int position)
{
	u32 from_ver = 0, def_ver = 0;
	u8 ret = 0;
	char ver[3] = {'V', '0', '0'};
	struct fimc_is_from_info *finfo = NULL;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT)
		finfo = &sysfs_finfo_front;
	else
#endif
		finfo = &sysfs_finfo;

	def_ver = ver[0] << 16 | ver[1] << 8 | ver[2];
	from_ver = finfo->cal_map_ver[0] << 16 | finfo->cal_map_ver[1] << 8 | finfo->cal_map_ver[2];
	if (from_ver == def_ver) {
		return finfo->cal_map_ver[3];
	} else {
		err("FROM core version is invalid. version is %c%c%c%c",
			finfo->cal_map_ver[0], finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
		return 0;
	}

	return ret;
}

bool fimc_is_sec_check_from_ver(struct fimc_is_core *core, int position)
{
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	char compare_version;
	u8 from_ver;
	u8 latest_from_ver;

	if (specific->skip_cal_loading) {
		err("skip_cal_loading implemented");
		return false;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		finfo = &sysfs_finfo_front;
		latest_from_ver = CAL_MAP_ES_VERSION_FRONT;
		compare_version = CAMERA_MODULE_ES_VERSION_FRONT;
	} else
#endif
	{
		finfo = &sysfs_finfo;
		latest_from_ver = CAL_MAP_ES_VERSION_REAR;
		compare_version = CAMERA_MODULE_ES_VERSION_REAR;
	}

	from_ver = fimc_is_sec_compare_ver(position);

	if ((from_ver < latest_from_ver) ||
		(finfo->header_ver[10] < compare_version)) {
		err("invalid from version. from_ver %u, header_ver[10] %c", from_ver, finfo->header_ver[10]);
		return false;
	} else {
		return true;
	}
}

bool fimc_is_sec_check_cal_crc32(char *buf, int id)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 check_base;
	u32 check_length;
	u32 checksum_base;
	u32 address_boundary;
	bool crc32_temp, crc32_header_temp;
	struct fimc_is_from_info *finfo = NULL;

	buf32 = (u32 *)buf;

	printk(KERN_INFO "+++ %s\n", __func__);

	crc32_temp = true;
#ifdef CONFIG_COMPANION_USE
	if (id == SENSOR_POSITION_REAR)
		crc32_c1_check = true;
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		address_boundary = FIMC_IS_MAX_CAL_SIZE_FRONT;
	} else
#endif
	{
		address_boundary = FIMC_IS_MAX_CAL_SIZE;
	}

	/* Header data */
	check_base = 0;
	checksum = 0;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		finfo = &sysfs_finfo_front;
		check_length = HEADER_CRC32_LEN_FRONT;
	} else
#endif
	{
		finfo = &sysfs_finfo;
		check_length = HEADER_CRC32_LEN;
	}

	checksum_base = finfo->header_section_crc_addr / 4;
	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the header (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		crc32_header_temp = false;
		goto out;
	} else {
		crc32_header_temp = true;
	}

#if defined(CONFIG_COMPANION_C3_USE)
	if(id == SENSOR_POSITION_REAR) {
		/* Calibration data : for HERO with 73C3 */
		check_base = finfo->cal_data_start_addr / 4;
		checksum = 0;
		check_length = (finfo->cal_data_end_addr - finfo->cal_data_start_addr + 1) ;
		checksum_base = finfo->cal_data_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: cal data address has error: start(0x%08X), end(0x%08X)",
				finfo->cal_data_start_addr, finfo->cal_data_end_addr);
			crc32_temp = false;
			crc32_c1_check = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the cal data (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			crc32_c1_check = false;
			goto out;
		}

#ifdef CAMERA_PAF_CAL_ERROR_CHECK        
		/* check PAF error code */
		if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M 
			&& buf32[FROM_CAL_PAF_ERROR_CHECK_CODE_ADDR / 4] != 0) {
			err("Camera: CRC32 error at the check PAF error code. PAF error code is %d",
				buf32[FROM_CAL_PAF_ERROR_CHECK_CODE_ADDR / 4]);
			crc32_temp = false;
			crc32_c1_check = false;
			goto out;
		}
#endif

		goto out;
	}
#endif

#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
	/* OEM */
	check_base = finfo->oem_start_addr / 4;
	checksum = 0;
	check_length = (finfo->oem_end_addr - finfo->oem_start_addr + 1);
	checksum_base = finfo->oem_section_crc_addr / 4;

	if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
		err("Camera: OEM address has error: start(0x%08X), end(0x%08X)",
			finfo->oem_start_addr, finfo->oem_end_addr);
		crc32_temp = false;
		goto out;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the OEM (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		goto out;
	}
#endif

	/* AWB */
	check_base = finfo->awb_start_addr / 4;
	checksum = 0;
	check_length = (finfo->awb_end_addr - finfo->awb_start_addr + 1) ;
	checksum_base = finfo->awb_section_crc_addr / 4;

	if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
		err("Camera: AWB address has error: start(0x%08X), end(0x%08X)",
			finfo->awb_start_addr, finfo->awb_end_addr);
		crc32_temp = false;
		goto out;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the AWB (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		goto out;
	}

	/* Shading */
	check_base = finfo->shading_start_addr / 4;
	checksum = 0;
	check_length = (finfo->shading_end_addr - finfo->shading_start_addr + 1) ;
	checksum_base = finfo->shading_section_crc_addr / 4;

	if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
		err("Camera: Shading address has error: start(0x%08X), end(0x%08X)",
			finfo->shading_start_addr, finfo->shading_end_addr);
		crc32_temp = false;
		goto out;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the Shading (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		goto out;
	}

#if defined(CONFIG_COMPANION_C2_USE) && !defined(CONFIG_FRONT_COMPANION_C2_DISABLE)
	/* c2 Shading */
	if (id == SENSOR_POSITION_FRONT) {
		check_base = finfo->c2_shading_start_addr / 4;
		checksum = 0;
		check_length = (finfo->c2_shading_end_addr - finfo->c2_shading_start_addr + 1);
		checksum_base = finfo->comp_shading_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: C2 Shading address has error: start(0x%08X), end(0x%08X)",
				finfo->c2_shading_start_addr, finfo->c2_shading_end_addr);
			crc32_temp = false;
			crc32_c1_check_front = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the C2 Shading (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			crc32_c1_check_front = false;
			goto out;
		}
	}
#endif

#ifdef CONFIG_COMPANION_USE
	/* pdaf cal */
	if (id == SENSOR_POSITION_REAR) {
		check_base = finfo->pdaf_cal_start_addr / 4;
		checksum = 0;
		check_length = (finfo->pdaf_cal_end_addr - finfo->pdaf_cal_start_addr + 1);
		checksum_base = finfo->paf_cal_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: pdaf address has error: start(0x%08X), end(0x%08X)",
				finfo->pdaf_start_addr, finfo->pdaf_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the pdaf cal (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}

		/* concord cal */
		check_base = finfo->concord_cal_start_addr / 4;
		checksum = 0;
		check_length = (finfo->concord_cal_end_addr - finfo->concord_cal_start_addr + 1);
		checksum_base = finfo->concord_cal_section_crc_addr / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: concord cal address has error: start(0x%08X), end(0x%08X)",
				finfo->concord_cal_start_addr, finfo->concord_cal_end_addr);
			crc32_c1_check = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the concord cal (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_c1_check = false;
			goto out;
		}
	}
#endif

out:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		crc32_check_front = crc32_temp;
		crc32_header_check_front = crc32_header_temp;
		return crc32_check_front;
	} else
#endif
	{
		crc32_check = crc32_temp;
		crc32_header_check = crc32_header_temp;
#ifdef CONFIG_COMPANION_USE
		return crc32_check && crc32_c1_check;
#else
		return crc32_check;
#endif
	}
}

#ifdef CAMERA_MODULE_DUALIZE
bool fimc_is_sec_check_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;
	u32 checksum_seed;

	buf32 = (u32 *)buf;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_C)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_D)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B))
		checksum_seed = CHECKSUM_SEED_ISP_FW_LL;
	else
		checksum_seed = CHECKSUM_SEED_ISP_FW_LS;

	info("Camera: Start checking CRC32 FW\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], sysfs_finfo.fw_size, NULL, NULL);
	checksum_base = (checksum_seed & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_fw_check = false;
	} else {
		crc32_fw_check = true;
	}

	info("Camera: End checking CRC32 FW\n");

	return crc32_fw_check;
}
#endif

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
bool fimc_is_sec_check_front_otp_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	bool crc32_temp, crc32_header_temp;
	u32 checksumFromOTP;

	buf32 = (u32 *)buf;
	checksumFromOTP = buf[41] +( buf[42] << 8) +( buf[43] << 16) + (buf[44] << 24);

	/* Header data */
	checksum = (u32)getCRC((u16 *)&buf32[0], 41, NULL, NULL);

	if(checksum != checksumFromOTP) {
		crc32_temp = crc32_header_temp = false;
		err("Camera: CRC32 error at the header data section (0x%08X != 0x%08X)",
					checksum, checksumFromOTP);
	} else {
		crc32_temp = crc32_header_temp = true;
		pr_info("Camera: End checking CRC32 (0x%08X = 0x%08X)",
					checksum, checksumFromOTP);
	}

	crc32_check_front = crc32_temp;
	crc32_header_check_front = crc32_header_temp;
	return crc32_check_front;
}
#endif

#ifdef CAMERA_MODULE_DUALIZE
bool fimc_is_sec_check_setfile_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;
	u32 checksum_seed;

	buf32 = (u32 *)buf;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_C)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_D)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B))
		checksum_seed = CHECKSUM_SEED_SETF_LL;
	else
		checksum_seed = CHECKSUM_SEED_SETF_LS;

	info("Camera: Start checking CRC32 Setfile\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], sysfs_finfo.setfile_size, NULL, NULL);
	checksum_base = (checksum_seed & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_setfile_check = false;
	} else {
		crc32_setfile_check = true;
	}

	info("Camera: End checking CRC32 Setfile\n");

	return crc32_setfile_check;
}

#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
bool fimc_is_sec_check_front_setfile_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;
	u32 checksum_seed;

	buf32 = (u32 *)buf;

	checksum_seed = CHECKSUM_SEED_FRONT_SETF_LL;

	info("Camera: Start checking CRC32 Front setfile\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], sysfs_finfo.front_setfile_size, NULL, NULL);
	checksum_base = (checksum_seed & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_front_setfile_check = false;
	} else {
		crc32_front_setfile_check = true;
	}

	info("Camera: End checking CRC32 front setfile\n");

	return crc32_front_setfile_check;
}
#endif
#endif

#ifdef CONFIG_COMPANION_USE
bool fimc_is_sec_check_companion_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;
	u32 checksum_seed;

	buf32 = (u32 *)buf;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_C)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_D)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B))
		checksum_seed = CHECKSUM_SEED_COMP_FW_LL;
	else
		checksum_seed = CHECKSUM_SEED_COMP_FW_LS;

	info("Camera: Start checking CRC32 Companion FW\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], sysfs_finfo.comp_fw_size, NULL, NULL);
	checksum_base = ((checksum_seed & 0xffffffff)) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_c1_fw_check = false;
	} else {
		crc32_c1_fw_check = true;
	}

	info("Camera: End checking CRC32 Companion FW\n");

	return crc32_c1_fw_check;
}
#endif

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx = -ENOENT;
	int fd, old_mask;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	if (force_caldata_dump) {
		sys_rmdir(name);
		fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	} else {
		fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0664);
	}
	if (fd < 0) {
		err("open file error: %s", name);
		sys_umask(old_mask);
		set_fs(old_fs);
		return -EINVAL;
	}

	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, count, pos);
		if (tx != count) {
			err("fail to write %s. ret %zd", name, tx);
			tx = -ENOENT;
		}

		vfs_fsync(fp, 0);
		fput(fp);
	} else {
		err("fail to get file *: %s", name);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

	return tx;
}

ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	int fd;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = sys_open(name, O_RDONLY, 0664);
	if (fd < 0) {
		if (-ENOENT == fd)
			info("%s: file(%s) not exist!\n", __func__, name);
		else
			info("%s: error %d, failed to open %s\n", __func__, fd, name);

		set_fs(old_fs);
		return -EINVAL;
	}
	fp = fget(fd);
	if (fp) {
		vfs_read(fp, buf, count, pos);
		fput(fp);
	}
	sys_close(fd);
	set_fs(old_fs);

	return count;
}

bool fimc_is_sec_file_exist(char *name)
{
	mm_segment_t old_fs;
	bool exist = true;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = sys_access(name, 0);
	if (ret) {
		exist = false;
		if (-ENOENT == ret)
			info("%s: file(%s) not exist!\n", __func__, name);
		else
			info("%s: error %d, failed to access %s\n", __func__, ret, name);
	}

	set_fs(old_fs);
	return exist;
}

void fimc_is_sec_make_crc32_table(u32 *table, u32 id)
{
	u32 i, j, k;

	for (i = 0; i < 256; ++i) {
		k = i;
		for (j = 0; j < 8; ++j) {
			if (k & 1)
				k = (k >> 1) ^ id;
			else
				k >>= 1;
		}
		table[i] = k;
	}
}

#if 0 /* unused */
static void fimc_is_read_sensor_version(void)
{
	int ret;
	char buf[0x50];

	memset(buf, 0x0, 0x50);

	printk(KERN_INFO "+++ %s\n", __func__);

	ret = fimc_is_spi_read(buf, 0x0, 0x50);

	printk(KERN_INFO "--- %s\n", __func__);

	if (ret) {
		err("fimc_is_spi_read - can't read sensor version\n");
	}

	err("Manufacturer ID(0x40): 0x%02x\n", buf[0x40]);
	err("Pixel Number(0x41): 0x%02x\n", buf[0x41]);
}

static void fimc_is_read_sensor_version2(void)
{
	char *buf;
	char *cal_data;
	u32 cur;
	u32 count = SETFILE_SIZE/READ_SIZE;
	u32 extra = SETFILE_SIZE%READ_SIZE;

	printk(KERN_ERR "%s\n", __func__);

	buf = (char *)kmalloc(READ_SIZE, GFP_KERNEL);
	cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

	memset(buf, 0x0, READ_SIZE);
	memset(cal_data, 0x0, SETFILE_SIZE);

	for (cur = 0; cur < SETFILE_SIZE; cur += READ_SIZE) {
		fimc_is_spi_read(buf, cur, READ_SIZE);
		memcpy(cal_data+cur, buf, READ_SIZE);
		memset(buf, 0x0, READ_SIZE);
	}

	if (extra != 0) {
		fimc_is_spi_read(buf, cur, extra);
		memcpy(cal_data+cur, buf, extra);
		memset(buf, 0x0, extra);
	}

	info("Manufacturer ID(0x40): 0x%02x\n", cal_data[0x40]);
	info("Pixel Number(0x41): 0x%02x\n", cal_data[0x41]);

	info("Manufacturer ID(0x4FE7): 0x%02x\n", cal_data[0x4FE7]);
	info("Pixel Number(0x4FE8): 0x%02x\n", cal_data[0x4FE8]);
	info("Manufacturer ID(0x4FE9): 0x%02x\n", cal_data[0x4FE9]);
	info("Pixel Number(0x4FEA): 0x%02x\n", cal_data[0x4FEA]);

	kfree(buf);
	kfree(cal_data);
}

static int fimc_is_get_cal_data(void)
{
	int err = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long ret = 0;
	u8 mem0 = 0, mem1 = 0;
	u32 CRC = 0;
	u32 DataCRC = 0;
	u32 IntOriginalCRC = 0;
	u32 crc_index = 0;
	int retryCnt = 2;
	u32 header_crc32 =	0x1000;
	u32 oem_crc32 =		0x2000;
	u32 awb_crc32 =		0x3000;
	u32 shading_crc32 = 0x6000;
	u32 shading_header = 0x22C0;

	char *cal_data;

	crc32_check = true;
	printk(KERN_INFO "%s\n", __func__);
	printk(KERN_INFO "+++ %s\n", __func__);

	fimc_is_spi_read(cal_map_version, 0x60, 0x4);
	printk(KERN_INFO "cal_map_version = %.4s\n", cal_map_version);

	if (cal_map_version[3] == '5') {
		shading_crc32 = 0x6000;
		shading_header = 0x22C0;
	} else if (cal_map_version[3] == '6') {
		shading_crc32 = 0x4000;
		shading_header = 0x920;
	} else {
		shading_crc32 = 0x5000;
		shading_header = 0x22C0;
	}

	/* Make CRC Table */
	fimc_is_sec_make_crc32_table((u32 *)&crc_table, 0xEDB88320);


	retry:
		cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

		memset(cal_data, 0x0, SETFILE_SIZE);

		mem0 = 0, mem1 = 0;
		CRC = 0;
		DataCRC = 0;
		IntOriginalCRC = 0;
		crc_index = 0;

		fimc_is_spi_read(cal_data, 0, SETFILE_SIZE);

		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x80)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made HEADER CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[header_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[header_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[header_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[header_crc32-1]&0x00ff);
		printk(KERN_INFO "Original HEADER CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;

		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0xC0)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x1000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x1000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made OEM CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[oem_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[oem_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[oem_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[oem_crc32-1]&0x00ff);
		printk(KERN_INFO "Original OEM CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x20)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x2000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x2000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made AWB CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[awb_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[awb_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[awb_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[awb_crc32-1]&0x00ff);
		printk(KERN_INFO "Original AWB CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (shading_header)/2; crc_index++) {

			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x3000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x3000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made SHADING CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[shading_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[shading_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[shading_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[shading_crc32-1]&0x00ff);
		printk(KERN_INFO "Original SHADING CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		old_fs = get_fs();
		set_fs(KERNEL_DS);

		if (crc32_check == true) {
			printk(KERN_INFO "make cal_data.bin~~~~ \n");
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		} else {
			if (retryCnt > 0) {
				set_fs(old_fs);
				retryCnt--;
				goto retry;
			}
		}

/*
		{
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		}
*/

		if (fp != NULL)
			filp_close(fp, current->files);

	out:
		set_fs(old_fs);
		kfree(cal_data);
		return err;

}

#endif

/**
 * fimc_is_sec_ldo_enabled: check whether the ldo has already been enabled.
 *
 * @ return: true, false or error value
 */
int fimc_is_sec_ldo_enabled(struct device *dev, char *name) {
	struct regulator *regulator = NULL;
	int enabled = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail", __func__, name);
		return -EINVAL;
	}

	enabled = regulator_is_enabled(regulator);

	regulator_put(regulator);

	return enabled;
}

int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on)
{
	struct regulator *regulator = NULL;
	int ret = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail", __func__, name);
		return -EINVAL;
	}

	if (on) {
		if (regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already enabled\n", name);
			goto exit;
		}

		ret = regulator_enable(regulator);
		if (ret) {
			err("%s : regulator_enable(%s) fail", __func__, name);
			goto exit;
		}
	} else {
		if (!regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already disabled\n", name);
			goto exit;
		}

		ret = regulator_disable(regulator);
		if (ret) {
			err("%s : regulator_disable(%s) fail", __func__, name);
			goto exit;
		}
	}

exit:
	regulator_put(regulator);

	return ret;
}

int fimc_is_sec_rom_power_on(struct fimc_is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	info("%s: Sensor position = %d.", __func__, position);

	if(position == SENSOR_POSITION_REAR)
		sensor_id = specific->rear_sensor_id;
	else
		sensor_id = specific->front_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module->dev, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sec_rom_power_off(struct fimc_is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;

	info("%s: Sensor position = %d.", __func__, position);

	if(position == SENSOR_POSITION_REAR)
		sensor_id = specific->rear_sensor_id;
	else
		sensor_id = specific->front_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module->dev, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_i2c_read(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	const u32 addr_size = 2, max_retry = 2;
	u8 addr_buf[addr_size];
	int retries = max_retry;
	int ret = 0;

	if (!client) {
		info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr */
	addr_buf[0] = ((u16)addr) >> 8;
	addr_buf[1] = (u8)addr;

	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, addr_buf, addr_size);
		if (likely(addr_size == ret))
			break;

		info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to write 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	/* Receive data */
	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_recv(client, buf, size);
		if (likely(ret == size))
			break;

		info("%s: i2c_master_recv failed(%d), try %d\n", __func__,  ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to read 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

int fimc_is_i2c_write(struct i2c_client *client, u16 addr, u8 data)
{
	const u32 write_buf_size = 3, max_retry = 2;
	u8 write_buf[write_buf_size];
	int retries = max_retry;
	int ret = 0;

	if (!client) {
		pr_info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr+data */
	write_buf[0] = ((u16)addr) >> 8;
	write_buf[1] = (u8)addr;
	write_buf[2] = data;


	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, write_buf, write_buf_size);
		if (likely(write_buf_size == ret))
			break;

		pr_info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		pr_err("%s: error %d, fail to write 0x%04X\n", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

static int fimc_is_i2c_config(struct i2c_client *client, bool onoff)
{
	struct device *i2c_dev = client->dev.parent->parent;
	struct pinctrl *pinctrl_i2c = NULL;

	info("(%s):onoff(%d)\n", __func__, onoff);
	if (onoff) {
		/* ON */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
	} else {
		/* OFF */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "default");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
	}

	return 0;
}

int fimc_is_sec_check_reload(struct fimc_is_core *core)
{
	struct file *reload_key_fp = NULL;
	struct file *supend_resume_key_fp = NULL;
	mm_segment_t old_fs;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	reload_key_fp = filp_open("/data/media/0/reload/r1e2l3o4a5d.key", O_RDONLY, 0);
	if (IS_ERR(reload_key_fp)) {
		reload_key_fp = NULL;
	} else {
		info("Reload KEY exist, reload cal data.\n");
		force_caldata_dump = true;
		specific->suspend_resume_disable = true;
	}

	if (reload_key_fp)
		filp_close(reload_key_fp, current->files);

	supend_resume_key_fp = filp_open("/data/media/0/i1s2p3s4r.key", O_RDONLY, 0);
	if (IS_ERR(supend_resume_key_fp)) {
		supend_resume_key_fp = NULL;
	} else {
		info("Supend_resume KEY exist, disable runtime supend/resume. \n");
		specific->suspend_resume_disable = true;
	}

	if (supend_resume_key_fp)
		filp_close(supend_resume_key_fp, current->files);

	set_fs(old_fs);

	return 0;
}

int fimc_is_sec_read_eeprom_header(struct device *dev, int position)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific;
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	struct i2c_client *client;

	specific = core->vender.private_data;
	client = specific->eeprom_client0;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT) {
		if (specific->use_ois_hsi2c) {
			fimc_is_i2c_config(client, true);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, true);
	}

	if(position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		ret = fimc_is_i2c_read(client, header_version,
				EEP_HEADER_VERSION_START_ADDR_FRONT, FIMC_IS_HEADER_VER_SIZE);
#endif
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		ret = fimc_is_i2c_read(client, header_version,
				EEP_HEADER_VERSION_START_ADDR, FIMC_IS_HEADER_VER_SIZE);
#endif
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT) {
		if (specific->use_ois_hsi2c) {
			fimc_is_i2c_config(client, false);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, false);
	}

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(sysfs_finfo.header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

	return ret;
}

int fimc_is_sec_readcal_eeprom(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		finfo = &sysfs_finfo_front;
		fimc_is_sec_get_front_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		client = specific->eeprom_client1;
	} else
#endif
	{
		finfo = &sysfs_finfo;
		fimc_is_sec_get_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		client = specific->eeprom_client0;
	}

	if (client == NULL) {
		err("i2c client is null. position(%d)\n", position);
		ret = -ENODEV;
		goto exit;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT) {
		if (specific->use_ois_hsi2c) {
			fimc_is_i2c_config(client, true);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, true);
	}

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
				       EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT,
				       FIMC_IS_CAL_MAP_VER_SIZE);
		ret = fimc_is_i2c_read(client, finfo->header_ver,
				       EEP_HEADER_VERSION_START_ADDR_FRONT,
				       FIMC_IS_HEADER_VER_SIZE);
#endif
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
				       EEP_HEADER_CAL_MAP_VER_START_ADDR,
				       FIMC_IS_CAL_MAP_VER_SIZE);
		ret = fimc_is_i2c_read(client, finfo->header_ver,
				       EEP_HEADER_VERSION_START_ADDR,
				       FIMC_IS_HEADER_VER_SIZE);
#endif
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT) {
		if (specific->use_ois_hsi2c) {
			fimc_is_i2c_config(client, false);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, false);
	}

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	printk(KERN_INFO "Camera: EEPROM Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	if (!fimc_is_sec_check_from_ver(core, position)) {
		info("Camera: Do not read eeprom cal data. EEPROM version is low.\n");
		return 0;
	}

crc_retry:

	/* read cal data */
	info("Camera: I2C read cal data\n");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT) {
		if (specific->use_ois_hsi2c) {
			fimc_is_i2c_config(client, true);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, true);
	}

	ret = fimc_is_i2c_read(client, buf, 0x0, cal_size);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if(position == SENSOR_POSITION_FRONT) {
		if (specific->use_ois_hsi2c) {
			fimc_is_i2c_config(client, false);
		}
	} else
#endif
	{
		fimc_is_i2c_config(client, false);
	}

	if (ret) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		info("FRONT EEPROM header version = %s\n", finfo->header_ver);
#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR_FRONT]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR_FRONT]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#endif
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR_FRONT]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR_FRONT]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR_FRONT]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR_FRONT]);
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
		if (finfo->shading_end_addr > 0x3AFF) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->shading_end_addr = 0x3AFF;
		}
#if (defined(CONFIG_COMPANION_C2_USE) || defined(CONFIG_COMPANION_C3_USE)) && !defined(CONFIG_FRONT_COMPANION_C2_DISABLE)
		finfo->c2_shading_start_addr = *((u32 *)&buf[EEP_HEADER_C2_SHADING_START_ADDR_FRONT]);
		finfo->c2_shading_end_addr = *((u32 *)&buf[EEP_HEADER_C2_SHADING_END_ADDR_FRONT]);
		info("c2_shading start = 0x%08x, end = 0x%08x\n",
				(finfo->c2_shading_start_addr), (finfo->c2_shading_end_addr));
		if (finfo->c2_shading_end_addr > 0x1CFF) {
			err("C2 Shading end_addr has error!! 0x%08x", finfo->c2_shading_end_addr);
			finfo->c2_shading_end_addr = 0x1CFF;
		}

		/* C2 SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->c2_shading_ver, &buf[EEP_C2_SHADING_VER_START_ADDR_FRONT], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->comp_shading_section_crc_addr = EEP_CHECKSUM_C2_SHADING_ADDR_FRONT;

		finfo->lsc_i0_gain_addr = EEP_C2_SHADING_LSC_I0_GAIN_ADDR_FRONT;
		info("Shading lsc_i0 start = 0x%08x\n", finfo->lsc_i0_gain_addr);
		finfo->lsc_j0_gain_addr = EEP_C2_SHADING_LSC_J0_GAIN_ADDR_FRONT;
		info("Shading lsc_j0 start = 0x%08x\n", finfo->lsc_j0_gain_addr);
		finfo->lsc_a_gain_addr = EEP_C2_SHADING_LSC_A_GAIN_ADDR_FRONT;
		info("Shading lsc_a start = 0x%08x\n", finfo->lsc_a_gain_addr);
		finfo->lsc_k4_gain_addr = EEP_C2_SHADING_LSC_K4_GAIN_ADDR_FRONT;
		info("Shading lsc_k4 start = 0x%08x\n", finfo->lsc_k4_gain_addr);
		finfo->lsc_scale_gain_addr = EEP_C2_SHADING_LSC_SCALE_GAIN_ADDR_FRONT;
		info("Shading lsc_scale start = 0x%08x\n", finfo->lsc_scale_gain_addr);
		finfo->grasTuning_AwbAshCord_N_addr = EEP_C2_SHADING_GRASTUNING_AWB_ASH_CORD_ADDR_FRONT;
		info("Shading grasTuning_AwbAshCord_N start = 0x%08x\n", finfo->grasTuning_AwbAshCord_N_addr);
		finfo->grasTuning_awbAshCordIndexes_N_addr = EEP_C2_SHADING_GRASTUNING_AWB_ASH_CORD_INDEX_ADDR_FRONT;
		info("Shading grasTuning_awbAshCordIndexes_N start = 0x%08x\n",
				finfo->grasTuning_awbAshCordIndexes_N_addr);
		finfo->grasTuning_GASAlpha_M__N_addr = EEP_C2_SHADING_GRASTUNING_GAS_ALPHA_ADDR_FRONT;
		info("Shading grasTuning_GASAlpha_M__N_addr start = 0x%08x\n", finfo->grasTuning_GASAlpha_M__N_addr);
		finfo->grasTuning_GASBeta_M__N_addr = EEP_C2_SHADING_GRASTUNING_GAS_BETA_ADDR_FRONT;
		info("Shading grasTuning_GASBeta_M__N start = 0x%08x\n", finfo->grasTuning_GASBeta_M__N_addr);
		finfo->grasTuning_GASOutdoorAlpha_N_addr = EEP_C2_SHADING_GRASTUNING_GAS_OUTDOOR_ALPHA_ADDR_FRONT;
		info("Shading grasTuning_GASOutdoorAlpha_N start = 0x%08x\n",
				finfo->grasTuning_GASOutdoorAlpha_N_addr);
		finfo->grasTuning_GASOutdoorBeta_N_addr = EEP_C2_SHADING_GRASTUNING_GAS_OUTDOOR_BETA_ADDR_FRONT;
		info("Shading grasTuning_GASOutdoorBeta_N start = 0x%08x\n", finfo->grasTuning_GASOutdoorBeta_N_addr);
		finfo->grasTuning_GASIndoorAlpha_N_addr = EEP_C2_SHADING_GRASTUNING_GAS_INDOOR_ALPHA_ADDR_FRONT;
		info("Shading grasTuning_GASIndoorAlpha_N start = 0x%08x\n", finfo->grasTuning_GASIndoorAlpha_N_addr);
		finfo->grasTuning_GASIndoorBeta_N_addr = EEP_C2_SHADING_GRASTUNING_GAS_INDOOR_BETA_ADDR_FRONT;
		info("Shading grasTuning_GASIndoorBeta_N start = 0x%08x\n", finfo->grasTuning_GASIndoorBeta_N_addr);

		finfo->lsc_gain_start_addr = EEP_C2_SHADING_LSC_GAIN_START_ADDR_FRONT;
		finfo->lsc_gain_end_addr = EEP_C2_SHADING_LSC_GAIN_END_ADDR_FRONT;
		info("LSC start = 0x%04x, end = 0x%04x\n", finfo->lsc_gain_start_addr, finfo->lsc_gain_end_addr);
		finfo->lsc_gain_crc_addr = EEP_C2_SHADING_LSC_GAIN_CRC_ADDR_FRONT;
		info("lsc_gain_crc_addr = 0x%04x,\n", finfo->lsc_gain_crc_addr);
		finfo->lsc_parameter_crc_addr = EEP_C2_SHADING_LSC_PARAMETER_CRC_ARRD_FRONT;
		info("lsc_parameter_crc_addr = 0x%04x,\n", finfo->lsc_parameter_crc_addr);
		info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
#endif
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[EEP_HEADER_VERSION_START_ADDR_FRONT], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver,
		       &buf[EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT], FIMC_IS_CAL_MAP_VER_SIZE);

		memcpy(finfo->project_name,
		       &buf[EEP_HEADER_PROJECT_NAME_START_ADDR_FRONT], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
		finfo->header_section_crc_addr = EEP_CHECKSUM_HEADER_ADDR_FRONT;

#if defined(EEP_HEADER_SENSOR_ID_ADDR)
		memcpy(finfo->from_sensor_id, &buf[EEP_HEADER_SENSOR_ID_ADDR], FIMC_IS_SENSOR_ID_SIZE);
		finfo->from_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
#endif

#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[EEP_OEM_VER_START_ADDR_FRONT], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = EEP_CHECKSUM_OEM_ADDR_FRONT;
#endif
		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[EEP_AWB_VER_START_ADDR_FRONT], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = EEP_CHECKSUM_AWB_ADDR_FRONT;

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[EEP_AP_SHADING_VER_START_ADDR_FRONT], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = EEP_CHECKSUM_AP_SHADING_ADDR_FRONT;
#endif
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR]);
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));

		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[EEP_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[EEP_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);

		memcpy(finfo->project_name, &buf[EEP_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
		finfo->header_section_crc_addr = EEP_CHECKSUM_HEADER_ADDR;

		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[EEP_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
		finfo->oem_section_crc_addr = EEP_CHECKSUM_OEM_ADDR;

		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[EEP_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
		finfo->awb_section_crc_addr = EEP_CHECKSUM_AWB_ADDR;

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[EEP_AP_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
		finfo->shading_section_crc_addr = EEP_CHECKSUM_AP_SHADING_ADDR;
#endif
	}

	/* debug info dump */
#if defined(EEPROM_DEBUG)
	info("++++ EEPROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", finfo->header_ver);
	info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE],
		finfo->header_ver[FW_PIXEL_SIZE+1]);
	info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM],
		finfo->header_ver[FW_PUB_NUM+1]);
	info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	info("project_name : %s\n", finfo->project_name);
	info("Cal data map ver : %s\n", finfo->cal_map_ver);
	info("2. OEM info\n");
	info("Module info : %s\n", finfo->oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", finfo->awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", finfo->shading_ver);
	info("---- EEPROM data info\n");
#endif

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	if (position == SENSOR_POSITION_FRONT) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}
	} else {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}
	}

#if (defined(CONFIG_COMPANION_C2_USE) || defined(CONFIG_COMPANION_C3_USE)) && !defined(CONFIG_FRONT_COMPANION_C2_DISABLE)
	if (fimc_is_sec_check_from_ver(core, position)) {
		/* If FROM LSC value is not valid, loading default lsc data */
		if (*((u32 *)&cal_buf_front[sysfs_finfo_front.lsc_gain_start_addr]) == 0x00000000) {
			companion_front_lsc_isvalid = false;
		} else {
			companion_front_lsc_isvalid = true;
		}
	}
#endif

	/* check CAMERA_MODULE_ES_VERSION in EEPROM */
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] >= CAMERA_MODULE_ES_VERSION_FRONT) {
				is_latest_cam_module_front = true;
			} else {
				is_latest_cam_module_front = false;
			}
		} else {
			is_latest_cam_module_front = true;
		}
	}
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	if (position == SENSOR_POSITION_REAR) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
				is_latest_cam_module = true;
			} else {
				is_latest_cam_module = false;
			}
		} else {
			is_latest_cam_module = true;
		}
	}
#endif

	/* check FIMC_IS_LATEST_FROM_VERSION_M in EEPROM */
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front = true;
			} else {
				is_final_cam_module_front = false;
			}
		} else {
			is_final_cam_module_front = true;
		}
	}        
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	if (position == SENSOR_POSITION_REAR) {
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module = true;
			} else {
				is_final_cam_module = false;
			}
		} else {
			is_final_cam_module = true;
		}
	}
#endif

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump EEPROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/eeprom_cal.bin", cal_buf, FIMC_IS_MAX_CAL_SIZE, &pos) < 0) {
				info("Failedl to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);

	set_fs(old_fs);
exit:
	return ret;
}

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
int fimc_is_sec_readcal_otprom(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(dev);
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	char selected_page[2] = {0,};

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		finfo = &sysfs_finfo_front;
		fimc_is_sec_get_front_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		client = specific->eeprom_client1;
#endif
	} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		finfo = &sysfs_finfo;
		fimc_is_sec_get_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		client = specific->eeprom_client0;
#endif
	}


	fimc_is_i2c_config(client, true);
	msleep(10);

	ret = fimc_is_i2c_write(client, 0xA00, 0x04);
	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	fimc_is_i2c_write(client, 0xA02, 0x02);
	fimc_is_i2c_write(client, 0xA00, 0x01);

	ret = fimc_is_i2c_read(client, selected_page, 0xA12, 0x1);
	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	printk(KERN_INFO "Camera: otp_bank = %d\n", selected_page[0]);
	if (selected_page[0] == 0x3) {
		printk(KERN_INFO "Camera: OTP 3 page selected\n");
		fimc_is_i2c_write(client, 0xA00, 0x04);
		fimc_is_i2c_write(client, 0xA00, 0x00);

		msleep(1);

		fimc_is_i2c_write(client, 0xA00, 0x04);
		fimc_is_i2c_write(client, 0xA02, 0x03);
		fimc_is_i2c_write(client, 0xA00, 0x01);
	}
	fimc_is_i2c_read(client, cal_map_version, 0xA22, 0x4);

	fimc_is_i2c_write(client, 0xA00, 0x04);
	fimc_is_i2c_write(client, 0xA00, 0x00);

	if(finfo->cal_map_ver[0] != 'V') {
		printk(KERN_INFO "Camera: Cal Map version read fail or there's no available data.\n");
		crc32_check_factory_front = false;
		goto exit;
	}

	printk(KERN_INFO "Camera: OTPROM Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

crc_retry:
	cal_size = 50;
	fimc_is_i2c_write(client, 0xA00, 0x04);
	if(selected_page[0] == 1)
		fimc_is_i2c_write(client, 0xA02, 0x02);
	else
		fimc_is_i2c_write(client, 0xA02, 0x03);
	fimc_is_i2c_write(client, 0xA00, 0x01);

	/* read cal data */
	pr_info("Camera: I2C read cal data\n\n");
	fimc_is_i2c_read(client, buf, 0xA15, cal_size);

	fimc_is_i2c_write(client, 0xA00, 0x04);
	fimc_is_i2c_write(client, 0xA00, 0x00);

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[OPT_HEADER_VERSION_START_ADDR_FRONT], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[OPT_HEADER_CAL_MAP_VER_START_ADDR_FRONT], FIMC_IS_CAL_MAP_VER_SIZE);
#endif
	} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[OPT_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[OPT_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
#endif
	}

	/* debug info dump */
	pr_info("++++ OTPROM data info\n");
	pr_info("1. Header info\n");
	pr_info("Module info : %s\n", finfo->header_ver);
	pr_info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	pr_info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE],
		finfo->header_ver[FW_PIXEL_SIZE+1]);
	pr_info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	pr_info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	pr_info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	pr_info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	pr_info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM],
		finfo->header_ver[FW_PUB_NUM+1]);
	pr_info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	pr_info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	pr_info("---- OTPROM data info\n");

	/* CRC check */
	ret = fimc_is_sec_check_front_otp_crc32(buf);

	if (!ret && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	if (position == SENSOR_POSITION_FRONT) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}
		if (specific->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front = true;
			} else {
				is_final_cam_module_front = false;
			}
		} else {
			is_final_cam_module_front = true;
		}
	} else {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}


		if (!specific->use_module_check) {
			is_latest_cam_module = true;
		} else {
			if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION) {
				is_latest_cam_module = true;
			} else {
				is_latest_cam_module = false;
			}
		}

		if (!specific->use_module_check) {
			is_final_cam_module = true;
		} else {
			if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module = true;
			} else {
				is_final_cam_module = false;
			}
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		pr_info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			pr_info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			pr_info("dump folder exist, Dump OTPROM cal data.\n");
			if (position == SENSOR_POSITION_FRONT) {
				if (write_data_to_file(FIMC_IS_CAL_DUMP_FRONT, buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
					pr_info("Failed to dump cal data.\n");
					goto dump_err;
				}
			} else {
				if (write_data_to_file(FIMC_IS_CAL_DUMP, buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
					pr_info("Failed to dump cal data.\n");
					goto dump_err;
				}
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);
exit:
	fimc_is_i2c_config(client, false);

	return ret;
}
#endif
#endif

#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_read_from_header(struct device *dev)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	ret = fimc_is_spi_read(&core->spi0, header_version, FROM_HEADER_VERSION_START_ADDR, FIMC_IS_HEADER_VER_SIZE);
	if (ret < 0) {
		printk(KERN_ERR "failed to fimc_is_spi_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(sysfs_finfo.header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

	return ret;
}

int fimc_is_sec_check_status(struct fimc_is_core *core)
{
	int retry_read = 50;
	u8 temp[5] = {0x0, };
	int ret = 0;

	do {
		memset(temp, 0x0, sizeof(temp));
		fimc_is_spi_read_status_bit(&core->spi0, &temp[0]);
		if (retry_read < 0) {
			ret = -EINVAL;
			err("check status failed.");
			break;
		}
		retry_read--;
		msleep(3);
	} while (temp[0]);

	return ret;
}

#ifdef CAMERA_MODULE_DUALIZE
int fimc_is_sec_read_fw_from_sdcard(char *name, unsigned long *size)
{
	struct file *fw_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	char data_path[100];
	int ret = 0;
	unsigned long fsize;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	snprintf(data_path, sizeof(data_path), "%s", name);
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);

	fw_fp = filp_open(data_path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fw_fp)) {
		info("%s does not exist.\n", data_path);
		fw_fp = NULL;
		ret = -EIO;
		goto fw_err;
	} else {
		info("%s exist, Dump from sdcard.\n", name);
		fsize = fw_fp->f_path.dentry->d_inode->i_size;
		if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
			read_data_from_file(name, fw_buf, fsize, &pos);
			*size = fsize;
		} else {
			err("FW size is larger than FW buffer.\n");
			BUG();
		}
	}

fw_err:
	if (fw_fp)
		filp_close(fw_fp, current->files);
	set_fs(old_fs);

	return ret;
}

u32 fimc_is_sec_get_fw_crc32(char *buf, size_t size)
{
	u32 *buf32 = NULL;
	u32 checksum;

	buf32 = (u32 *)buf;
	checksum = (u32)getCRC((u16 *)&buf32[0], size, NULL, NULL);

	return checksum;
}

int fimc_is_sec_change_from_header(struct fimc_is_core *core)
{
	int ret = 0;
	u8 crc_value[4];
	u32 crc_result = 0;

	/* read header data */
	info("Camera: Start SPI read header data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);

	ret = fimc_is_spi_read(&core->spi0, fw_buf, 0x0, HEADER_CRC32_LEN);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	fw_buf[0x7] = (sysfs_finfo.bin_end_addr & 0xFF000000) >> 24;
	fw_buf[0x6] = (sysfs_finfo.bin_end_addr & 0xFF0000) >> 16;
	fw_buf[0x5] = (sysfs_finfo.bin_end_addr & 0xFF00) >> 8;
	fw_buf[0x4] = (sysfs_finfo.bin_end_addr & 0xFF);
	fw_buf[0x27] = (sysfs_finfo.setfile_end_addr & 0xFF000000) >> 24;
	fw_buf[0x26] = (sysfs_finfo.setfile_end_addr & 0xFF0000) >> 16;
	fw_buf[0x25] = (sysfs_finfo.setfile_end_addr & 0xFF00) >> 8;
	fw_buf[0x24] = (sysfs_finfo.setfile_end_addr & 0xFF);
	fw_buf[0x37] = (sysfs_finfo.concord_bin_end_addr & 0xFF000000) >> 24;
	fw_buf[0x36] = (sysfs_finfo.concord_bin_end_addr & 0xFF0000) >> 16;
	fw_buf[0x35] = (sysfs_finfo.concord_bin_end_addr & 0xFF00) >> 8;
	fw_buf[0x34] = (sysfs_finfo.concord_bin_end_addr & 0xFF);

	strncpy(&fw_buf[0x40], sysfs_finfo.header_ver, 9);
	strncpy(&fw_buf[0x50], sysfs_finfo.concord_header_ver, FIMC_IS_HEADER_VER_SIZE);
	strncpy(&fw_buf[0x64], sysfs_finfo.setfile_ver, FIMC_IS_ISP_SETFILE_VER_SIZE);

	fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_erase_sector(&core->spi0, 0x0);
	if (ret) {
		err("failed to fimc_is_spi_erase_sector (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_write(&core->spi0, 0x0, fw_buf, HEADER_CRC32_LEN);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	crc_result = fimc_is_sec_get_fw_crc32(fw_buf, HEADER_CRC32_LEN);
	crc_value[3] = (crc_result & 0xFF000000) >> 24;
	crc_value[2] = (crc_result & 0xFF0000) >> 16;
	crc_value[1] = (crc_result & 0xFF00) >> 8;
	crc_value[0] = (crc_result & 0xFF);

	ret = fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_write(&core->spi0, 0x0FFC, crc_value, 0x4);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	info("Camera: End SPI read header data\n");

exit:
	return ret;
}

int fimc_is_sec_write_fw_to_from(struct fimc_is_core *core, char *name, bool first_section)
{
	int ret = 0;
	unsigned long i = 0;
	unsigned long size = 0;
	u32 start_addr = 0, erase_addr = 0, end_addr = 0;
	u32 checksum_addr = 0, crc_result = 0, erase_end_addr = 0;
	u8 crc_value[4];

	if (!strcmp(name, FIMC_IS_FW_FROM_SDCARD)) {
		ret = fimc_is_sec_read_fw_from_sdcard(FIMC_IS_FW_FROM_SDCARD, &size);
		start_addr = sysfs_finfo.bin_start_addr;
		end_addr = (u32)size + start_addr - 1;
		sysfs_finfo.bin_end_addr = end_addr;
		checksum_addr = 0x3FFFFF;
		sysfs_finfo.fw_size = size;
		strncpy(sysfs_finfo.header_ver, &fw_buf[size - 11], 9);
	} else if (!strcmp(name, FIMC_IS_SETFILE_FROM_SDCARD)) {
		ret = fimc_is_sec_read_fw_from_sdcard(FIMC_IS_SETFILE_FROM_SDCARD, &size);
		start_addr = sysfs_finfo.setfile_start_addr;
		end_addr = (u32)size + start_addr - 1;
		sysfs_finfo.setfile_end_addr = end_addr;
		if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B))
			checksum_addr = FROM_WRITE_CHECKSUM_SETF_LL;
		else
			checksum_addr = FROM_WRITE_CHECKSUM_SETF_LS;
		sysfs_finfo.setfile_size = size;
		strncpy(sysfs_finfo.setfile_ver, &fw_buf[size - 64], 6);
	} else if (!strcmp(name, FIMC_IS_COMPANION_FROM_SDCARD)) {
		ret = fimc_is_sec_read_fw_from_sdcard(FIMC_IS_COMPANION_FROM_SDCARD, &size);
		start_addr = sysfs_finfo.concord_bin_start_addr;
		end_addr = (u32)size + start_addr - 1;
		sysfs_finfo.concord_bin_end_addr = end_addr;
		if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B))
			checksum_addr = FROM_WRITE_CHECKSUM_COMP_LL;
		else
			checksum_addr = FROM_WRITE_CHECKSUM_COMP_LS;
		erase_end_addr = 0x3FFFFF;
		sysfs_finfo.comp_fw_size = size;
		strncpy(sysfs_finfo.concord_header_ver, &fw_buf[size - 16], FIMC_IS_HEADER_VER_SIZE);
	} else {
		err("Not supported binary type.");
		return -EIO;
	}

	if (ret < 0) {
		err("FW is not exist in sdcard.");
		return -EIO;
	}

	info("Start %s write to FROM.\n", name);

	if (first_section) {
		for (erase_addr = start_addr; erase_addr < erase_end_addr; erase_addr += FIMC_IS_FROM_ERASE_SIZE) {
			ret = fimc_is_spi_write_enable(&core->spi0);
			ret |= fimc_is_spi_erase_block(&core->spi0, erase_addr);
			if (ret) {
				err("failed to fimc_is_spi_erase_block (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
			ret = fimc_is_sec_check_status(core);
			if (ret) {
				err("failed to fimc_is_sec_check_status (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	for (i = 0; i < size; i += 256) {
		ret = fimc_is_spi_write_enable(&core->spi0);
		if (size - i >= 256) {
			ret = fimc_is_spi_write(&core->spi0, start_addr + i, fw_buf + i, 256);
			if (ret) {
				err("failed to fimc_is_spi_write (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		} else {
			ret = fimc_is_spi_write(&core->spi0, start_addr + i, fw_buf + i, size - i);
			if (ret) {
				err("failed to fimc_is_spi_write (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		}
		ret = fimc_is_sec_check_status(core);
		if (ret) {
			err("failed to fimc_is_sec_check_status (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
	}

	crc_result = fimc_is_sec_get_fw_crc32(fw_buf, size);
	crc_value[3] = (crc_result & 0xFF000000) >> 24;
	crc_value[2] = (crc_result & 0xFF0000) >> 16;
	crc_value[1] = (crc_result & 0xFF00) >> 8;
	crc_value[0] = (crc_result & 0xFF);

	ret = fimc_is_spi_write_enable(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_write_enable (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_write(&core->spi0, checksum_addr -4 + 1, crc_value, 0x4);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	info("End %s write to FROM.\n", name);

exit:
	return ret;
}

int fimc_is_sec_write_fw(struct fimc_is_core *core, struct device *dev)
{
	int ret = 0;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_spi *spi = &core->spi0;
#endif
	struct file *key_fp = NULL;
	struct file *comp_fw_fp = NULL;
	struct file *setfile_fp = NULL;
	struct file *isp_fw_fp = NULL;
	mm_segment_t old_fs;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open(FIMC_IS_KEY_FROM_SDCARD, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		ret = -EIO;
		goto key_err;
	} else {
		comp_fw_fp = filp_open(FIMC_IS_COMPANION_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(comp_fw_fp)) {
			info("Companion FW does not exist.\n");
			comp_fw_fp = NULL;
			ret = -EIO;
			goto comp_fw_err;
		}

		setfile_fp = filp_open(FIMC_IS_SETFILE_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(setfile_fp)) {
			info("setfile does not exist.\n");
			setfile_fp = NULL;
			ret = -EIO;
			goto setfile_err;
		}

		isp_fw_fp = filp_open(FIMC_IS_FW_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(isp_fw_fp)) {
			info("ISP FW does not exist.\n");
			isp_fw_fp = NULL;
			ret = -EIO;
			goto isp_fw_err;
		}
	}

	info("FW file exist, Write Firmware to FROM .\n");

	if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM"))
		fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);

#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_pin(spi, SPI_PIN_STATE_HOST);
#endif
	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_COMPANION_FROM_SDCARD, true);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto err;
	}

	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_SETFILE_FROM_SDCARD, false);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto err;
	}

	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_FW_FROM_SDCARD, false);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto err;
	}
#ifdef CONFIG_COMPANION_USE
	if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1)
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE_INPU);
	else if (specific->rear_sensor_id == SENSOR_NAME_IMX260)
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE_INPD);
	else
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE);
#endif

	/* Off to reset FROM operation. Without this routine, spi read does not work. */
	if (!specific->running_rear_camera)
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);

	if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM"))
		fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);

#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_pin(spi, SPI_PIN_STATE_HOST);
#endif
	ret = fimc_is_sec_change_from_header(core);
	if (ret) {
		err("fimc_is_sec_change_from_header failed.");
		ret = -EIO;
		goto err;
	}

err:
#ifdef CONFIG_COMPANION_USE
	if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1)
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE_INPU);
	else if (specific->rear_sensor_id == SENSOR_NAME_IMX260)
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE_INPD);
	else
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE);
#endif

	if (!specific->running_rear_camera)
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);

isp_fw_err:
	if (isp_fw_fp)
		filp_close(isp_fw_fp, current->files);

setfile_err:
	if (setfile_fp)
		filp_close(setfile_fp, current->files);

comp_fw_err:
	if (comp_fw_fp)
		filp_close(comp_fw_fp, current->files);

key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);

	return ret;
}
#endif

#if !defined(CONFIG_COMPANION_USE)
int fimc_is_sec_readcal(struct fimc_is_core *core)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u16 id;

	struct fimc_is_vender_specific *specific = core->vender.private_data;

	/* reset spi */
	if (!core->spi0.device) {
		err("spi0 device is not available");
		goto exit;
	}

	ret = fimc_is_spi_reset(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_read_module_id(&core->spi0, &id, FROM_HEADER_MODULE_ID_START_ADDR, FROM_HEADER_MODULE_ID_SIZE);
	if (ret) {
		printk(KERN_ERR "fimc_is_spi_read_module_id (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Module ID = 0x%04x\n", id);

	ret = fimc_is_spi_read(&core->spi0, sysfs_finfo.cal_map_ver,
			       FROM_HEADER_CAL_MAP_VER_START_ADDR, FIMC_IS_CAL_MAP_VER_SIZE);
	if (ret) {
		printk(KERN_ERR "failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Cal map_version = %c%c%c%c\n", sysfs_finfo.cal_map_ver[0],
			sysfs_finfo.cal_map_ver[1], sysfs_finfo.cal_map_ver[2], sysfs_finfo.cal_map_ver[3]);

crc_retry:
	/* read cal data */
	info("Camera: SPI read cal data\n");
	ret = fimc_is_spi_read(&core->spi0, cal_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_START_ADDR]);
	sysfs_finfo.bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_END_ADDR]);
	info("Binary start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_START_ADDR]);
	sysfs_finfo.shading_end_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_END_ADDR]);
	info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
	sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_START_ADDR]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_END_ADDR]);
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	if (sysfs_finfo.setfile_end_addr < FROM_ISP_BINARY_SETFILE_START_ADDR
	|| sysfs_finfo.setfile_end_addr > FROM_ISP_BINARY_SETFILE_END_ADDR) {
		info("setfile end_addr has error!!  0x%08x\n", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	memcpy(sysfs_finfo.header_ver, &cal_buf[FROM_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.cal_map_ver, &cal_buf[FROM_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
	memcpy(sysfs_finfo.setfile_ver, &cal_buf[FROM_HEADER_ISP_SETFILE_VER_START_ADDR], FIMC_IS_ISP_SETFILE_VER_SIZE);
	sysfs_finfo.setfile_ver[FIMC_IS_ISP_SETFILE_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.project_name, &cal_buf[FROM_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
	sysfs_finfo.project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
	sysfs_finfo.header_section_crc_addr = FROM_CHECKSUM_HEADER_ADDR;

	sysfs_finfo.oem_start_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_START_ADDR]);
	sysfs_finfo.oem_end_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_END_ADDR]);
	info("OEM start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.oem_start_addr), (sysfs_finfo.oem_end_addr));
	memcpy(sysfs_finfo.oem_ver, &cal_buf[FROM_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
	sysfs_finfo.oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
	sysfs_finfo.oem_section_crc_addr = FROM_CHECKSUM_OEM_ADDR;

	sysfs_finfo.awb_start_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_START_ADDR]);
	sysfs_finfo.awb_end_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_END_ADDR]);
	info("AWB start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.awb_start_addr), (sysfs_finfo.awb_end_addr));
	memcpy(sysfs_finfo.awb_ver, &cal_buf[FROM_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
	sysfs_finfo.awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
	sysfs_finfo.awb_section_crc_addr = FROM_CHECKSUM_AWB_ADDR;

	memcpy(sysfs_finfo.shading_ver, &cal_buf[FROM_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
	sysfs_finfo.shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
	sysfs_finfo.shading_section_crc_addr = FROM_CHECKSUM_SHADING_ADDR;

	fw_core_version = sysfs_finfo.header_ver[0];
	sysfs_finfo.fw_size = sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1;
	sysfs_finfo.setfile_size = sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1;
	sysfs_finfo.comp_fw_size = sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1;
	info("fw_size = %ld\n", sysfs_finfo.fw_size);
	info("setfile_size = %ld\n", sysfs_finfo.setfile_size);
	info("comp_fw_size = %ld\n", sysfs_finfo.comp_fw_size);

	/* debug info dump */
	info("++++ FROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", sysfs_finfo.header_ver);
	info(" ID : %c\n", sysfs_finfo.header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[FW_PIXEL_SIZE],
							sysfs_finfo.header_ver[FW_PIXEL_SIZE + 1]);
	info(" ISP ID : %c\n", sysfs_finfo.header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", sysfs_finfo.header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", sysfs_finfo.header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", sysfs_finfo.header_ver[FW_PUB_NUM],
							sysfs_finfo.header_ver[FW_PUB_NUM + 1]);
	info(" Manufacturer ID : %c\n", sysfs_finfo.header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", sysfs_finfo.header_ver[FW_VERSION_INFO]);
	info("Cal data map ver : %s\n", sysfs_finfo.cal_map_ver);
	info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	info("Project name : %s\n", sysfs_finfo.project_name);
	info("2. OEM info\n");
	info("Module info : %s\n", sysfs_finfo.oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", sysfs_finfo.awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", sysfs_finfo.shading_ver);
	info("---- FROM data info\n");

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	if (sysfs_finfo.header_ver[3] == 'L') {
		crc32_check_factory = crc32_check;
	} else {
		crc32_check_factory = false;
	}

	if (!specific->use_module_check) {
		is_latest_cam_module = true;
	} else {
		if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
			is_latest_cam_module = true;
		} else {
			is_latest_cam_module = false;
		}
	}

	if (specific->use_module_check) {
		if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M
#if defined(CAMERA_MODULE_CORE_CS_VERSION) && defined(CAMERA_MODULE_CORE_CS_VERSION2)
		    && (sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
			|| sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION2)
#else
#if defined(CAMERA_MODULE_CORE_CS_VERSION)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
#endif
#if defined(CAMERA_MODULE_CORE_CS_VERSION2)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION2
#endif
#endif
		) {
			is_final_cam_module = true;
		} else {
			is_final_cam_module = false;
		}
	} else {
		is_final_cam_module = true;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump FROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/from_cal.bin", cal_buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
				info("Failedl to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);
exit:
	return ret;
}
#endif

#if defined(CONFIG_COMPANION_C1_USE)
int fimc_is_sec_readcal(struct fimc_is_core *core)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u16 id;

	struct fimc_is_vender_specific *specific = core->vender.private_data;

	/* reset spi */
	if (!core->spi0.device) {
		err("spi0 device is not available");
		goto exit;
	}

	ret = fimc_is_spi_reset(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_read_module_id(&core->spi0, &id, FROM_HEADER_MODULE_ID_START_ADDR, FROM_HEADER_MODULE_ID_SIZE);
	if (ret) {
		printk(KERN_ERR "fimc_is_spi_read_module_id (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Module ID = 0x%04x\n", id);

	ret = fimc_is_spi_read(&core->spi0, sysfs_finfo.cal_map_ver,
			       FROM_HEADER_CAL_MAP_VER_START_ADDR, FIMC_IS_CAL_MAP_VER_SIZE);
	if (ret) {
		printk(KERN_ERR "failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Cal map_version = %c%c%c%c\n", sysfs_finfo.cal_map_ver[0],
			sysfs_finfo.cal_map_ver[1], sysfs_finfo.cal_map_ver[2], sysfs_finfo.cal_map_ver[3]);

crc_retry:
	/* read cal data */
	info("Camera: SPI read cal data\n");
	ret = fimc_is_spi_read(&core->spi0, cal_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_START_ADDR]);
	sysfs_finfo.bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_END_ADDR]);
	info("Binary start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_START_ADDR]);
	sysfs_finfo.shading_end_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_END_ADDR]);
	info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
	sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_START_ADDR]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_END_ADDR]);
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	if (sysfs_finfo.setfile_end_addr < FROM_ISP_BINARY_SETFILE_START_ADDR
	|| sysfs_finfo.setfile_end_addr > FROM_ISP_BINARY_SETFILE_END_ADDR) {
		info("setfile end_addr has error!!  0x%08x\n", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	memcpy(sysfs_finfo.header_ver, &cal_buf[FROM_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.cal_map_ver, &cal_buf[FROM_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
	memcpy(sysfs_finfo.setfile_ver, &cal_buf[FROM_HEADER_ISP_SETFILE_VER_START_ADDR], FIMC_IS_ISP_SETFILE_VER_SIZE);
	sysfs_finfo.setfile_ver[FIMC_IS_ISP_SETFILE_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.project_name, &cal_buf[FROM_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
	sysfs_finfo.project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
	sysfs_finfo.header_section_crc_addr = FROM_CHECKSUM_HEADER_ADDR;

	sysfs_finfo.oem_start_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_START_ADDR]);
	sysfs_finfo.oem_end_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_END_ADDR]);
	info("OEM start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.oem_start_addr), (sysfs_finfo.oem_end_addr));
	memcpy(sysfs_finfo.oem_ver, &cal_buf[FROM_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
	sysfs_finfo.oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
	sysfs_finfo.oem_section_crc_addr = FROM_CHECKSUM_OEM_ADDR;

	sysfs_finfo.awb_start_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_START_ADDR]);
	sysfs_finfo.awb_end_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_END_ADDR]);
	info("AWB start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.awb_start_addr), (sysfs_finfo.awb_end_addr));
	memcpy(sysfs_finfo.awb_ver, &cal_buf[FROM_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
	sysfs_finfo.awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
	sysfs_finfo.awb_section_crc_addr = FROM_CHECKSUM_AWB_ADDR;

	memcpy(sysfs_finfo.shading_ver, &cal_buf[FROM_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
	sysfs_finfo.shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
	sysfs_finfo.shading_section_crc_addr = FROM_CHECKSUM_SHADING_ADDR;

	sysfs_finfo.concord_bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_START_ADDR]);
	sysfs_finfo.concord_bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_END_ADDR]);
	info("concord bin start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_bin_start_addr, sysfs_finfo.concord_bin_end_addr);
	sysfs_finfo.pdaf_cal_start_addr = *((u32 *)&cal_buf[FROM_HEADER_PDAF_CAL_START_ADDR]);
	sysfs_finfo.pdaf_cal_end_addr = *((u32 *)&cal_buf[FROM_HEADER_PDAF_CAL_END_ADDR]);
	info("pdaf start = 0x%08x, end = 0x%08x\n", sysfs_finfo.pdaf_cal_start_addr, sysfs_finfo.pdaf_cal_end_addr);
	sysfs_finfo.lsc_i0_gain_addr = FROM_SHADING_LSC_I0_GAIN_ADDR;
	info("Shading lsc_i0 start = 0x%08x\n", sysfs_finfo.lsc_i0_gain_addr);
	sysfs_finfo.lsc_j0_gain_addr = FROM_SHADING_LSC_J0_GAIN_ADDR;
	info("Shading lsc_j0 start = 0x%08x\n", sysfs_finfo.lsc_j0_gain_addr);
	sysfs_finfo.lsc_a_gain_addr = FROM_SHADING_LSC_A_GAIN_ADDR;
	info("Shading lsc_a start = 0x%08x\n", sysfs_finfo.lsc_a_gain_addr);
	sysfs_finfo.lsc_k4_gain_addr = FROM_SHADING_LSC_K4_GAIN_ADDR;
	info("Shading lsc_k4 start = 0x%08x\n", sysfs_finfo.lsc_k4_gain_addr);
	sysfs_finfo.lsc_scale_gain_addr = FROM_SHADING_LSC_SCALE_GAIN_ADDR;
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.lsc_scale_gain_addr);
	sysfs_finfo.lsc_gain_start_addr = FROM_SHADING_LSC_GAIN_START_ADDR;
	sysfs_finfo.lsc_gain_end_addr = FROM_SHADING_LSC_GAIN_END_ADDR;
	info("LSC start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.lsc_gain_start_addr, sysfs_finfo.lsc_gain_end_addr);

	memcpy(sysfs_finfo.concord_header_ver,
		&cal_buf[FROM_HEADER_CONCORD_HEADER_VER_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.concord_header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	sysfs_finfo.lsc_gain_crc_addr = FROM_SHADING_LSC_GAIN_CRC_ADDR;
	sysfs_finfo.pdaf_crc_addr = FROM_CONCORD_PDAF_CRC_ADDR;

	sysfs_finfo.coef1_start = FROM_CONCORD_XTALK_10_START_ADDR;
	sysfs_finfo.coef1_end = FROM_CONCORD_XTALK_10_END_ADDR;
	info("coefficient1_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef1_start, sysfs_finfo.coef1_end);
	sysfs_finfo.coef2_start = FROM_CONCORD_XTALK_20_START_ADDR;
	sysfs_finfo.coef2_end = FROM_CONCORD_XTALK_20_END_ADDR;
	info("coefficient2_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef2_start, sysfs_finfo.coef2_end);
	sysfs_finfo.coef3_start = FROM_CONCORD_XTALK_30_START_ADDR;
	sysfs_finfo.coef3_end = FROM_CONCORD_XTALK_30_END_ADDR;
	info("coefficient3_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef3_start, sysfs_finfo.coef3_end);
	sysfs_finfo.coef4_start = FROM_CONCORD_XTALK_40_START_ADDR;
	sysfs_finfo.coef4_end = FROM_CONCORD_XTALK_40_END_ADDR;
	info("coefficient4_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef4_start, sysfs_finfo.coef4_end);
	sysfs_finfo.coef5_start = FROM_CONCORD_XTALK_50_START_ADDR;
	sysfs_finfo.coef5_end = FROM_CONCORD_XTALK_50_END_ADDR;
	info("coefficient5_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef5_start, sysfs_finfo.coef5_end);
	sysfs_finfo.coef6_start = FROM_CONCORD_XTALK_60_START_ADDR;
	sysfs_finfo.coef6_end = FROM_CONCORD_XTALK_60_END_ADDR;
	info("coefficient6_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef6_start, sysfs_finfo.coef6_end);
	sysfs_finfo.wcoefficient1_addr = FROM_CONCORD_WCOEF_ADDR;
	info("Shading wcoefficient1 start = 0x%04x\n", sysfs_finfo.wcoefficient1_addr);

	sysfs_finfo.af_inf_addr = FROM_OEM_AF_INF_ADDR;
	sysfs_finfo.af_macro_addr = FROM_OEM_AF_INF_ADDR;
	sysfs_finfo.coef1_crc_addr= FROM_CONCORD_XTALK_10_CHECKSUM_ADDR;
	sysfs_finfo.coef2_crc_addr = FROM_CONCORD_XTALK_20_CHECKSUM_ADDR;
	sysfs_finfo.coef3_crc_addr = FROM_CONCORD_XTALK_30_CHECKSUM_ADDR;
	sysfs_finfo.coef4_crc_addr = FROM_CONCORD_XTALK_40_CHECKSUM_ADDR;
	sysfs_finfo.coef5_crc_addr = FROM_CONCORD_XTALK_50_CHECKSUM_ADDR;
	sysfs_finfo.coef6_crc_addr = FROM_CONCORD_XTALK_60_CHECKSUM_ADDR;

	sysfs_finfo.concord_cal_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_CAL_START_ADDR]);
	sysfs_finfo.concord_cal_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_CAL_END_ADDR]);
	info("concord cal start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_cal_start_addr, sysfs_finfo.concord_cal_end_addr);
	sysfs_finfo.concord_master_setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MASTER_SETFILE_START_ADDR]);
	sysfs_finfo.concord_master_setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MASTER_SETFILE_START_ADDR]);
	sysfs_finfo.concord_mode_setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MODE_SETFILE_START_ADDR]);
	sysfs_finfo.concord_mode_setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MODE_SETFILE_END_ADDR]);
	sysfs_finfo.pdaf_start_addr = FROM_CONCORD_CAL_PDAF_START_ADDR;
	sysfs_finfo.pdaf_end_addr = FROM_CONCORD_CAL_PDAF_END_ADDR;
	info("pdaf start = 0x%04x, end = 0x%04x\n", sysfs_finfo.pdaf_start_addr, sysfs_finfo.pdaf_end_addr);

	sysfs_finfo.paf_cal_section_crc_addr = FROM_CHECKSUM_PAF_CAL_ADDR;
	sysfs_finfo.concord_cal_section_crc_addr = FROM_CHECKSUM_CONCORD_CAL_ADDR;

	fw_core_version = sysfs_finfo.header_ver[0];
	sysfs_finfo.fw_size = sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1;
	sysfs_finfo.setfile_size = sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1;
	sysfs_finfo.comp_fw_size = sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1;
	info("fw_size = %ld\n", sysfs_finfo.fw_size);
	info("setfile_size = %ld\n", sysfs_finfo.setfile_size);
	info("comp_fw_size = %ld\n", sysfs_finfo.comp_fw_size);

	/* debug info dump */
	info("++++ FROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", sysfs_finfo.header_ver);
	info("Companion version info : %s\n", sysfs_finfo.concord_header_ver);
	info(" ID : %c\n", sysfs_finfo.header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[FW_PIXEL_SIZE],
							sysfs_finfo.header_ver[FW_PIXEL_SIZE + 1]);
	info(" ISP ID : %c\n", sysfs_finfo.header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", sysfs_finfo.header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", sysfs_finfo.header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", sysfs_finfo.header_ver[FW_PUB_NUM],
							sysfs_finfo.header_ver[FW_PUB_NUM + 1]);
	info(" Manufacturer ID : %c\n", sysfs_finfo.header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", sysfs_finfo.header_ver[FW_VERSION_INFO]);
	info("Cal data map ver : %s\n", sysfs_finfo.cal_map_ver);
	info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	info("Project name : %s\n", sysfs_finfo.project_name);
	info("2. OEM info\n");
	info("Module info : %s\n", sysfs_finfo.oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", sysfs_finfo.awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", sysfs_finfo.shading_ver);
	info("---- FROM data info\n");

	/* CRC check */
	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
			retry--;
			goto crc_retry;
		}
	} else {
		fw_version_crc_check = false;
		crc32_check = false;
		crc32_c1_check = false;
	}

	if (sysfs_finfo.header_ver[3] == 'L') {
		crc32_check_factory = crc32_check;
		crc32_c1_check_factory = crc32_c1_check;
	} else {
		crc32_check_factory = false;
		crc32_c1_check_factory = false;
	}

	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (crc32_check && crc32_c1_check) {
			/* If FROM LSC value is not valid, loading default lsc data */
			if (*((u32 *)&cal_buf[sysfs_finfo.lsc_gain_start_addr]) == 0x00000000) {
				companion_lsc_isvalid = false;
			} else {
				companion_lsc_isvalid = true;
			}

			if (*((u32 *)&cal_buf[sysfs_finfo.coef1_start]) == 0x00000000) {
				companion_coef_isvalid = false;
			} else {
				companion_coef_isvalid = true;
			}
		} else {
			companion_lsc_isvalid = false;
			companion_coef_isvalid = false;
		}
	} else {
		companion_lsc_isvalid = true;
		companion_coef_isvalid = true;
	}

	if (!specific->use_module_check) {
		is_latest_cam_module = true;
	} else {
		if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
			is_latest_cam_module = true;
		} else {
			is_latest_cam_module = false;
		}
	}

	if (specific->use_module_check) {
		if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M
#if defined(CAMERA_MODULE_CORE_CS_VERSION) && defined(CAMERA_MODULE_CORE_CS_VERSION2)
		    && (sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
			|| sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION2)
#else
#if defined(CAMERA_MODULE_CORE_CS_VERSION)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
#endif
#if defined(CAMERA_MODULE_CORE_CS_VERSION2)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION2
#endif
#endif
		) {
			is_final_cam_module = true;
		} else {
			is_final_cam_module = false;
		}
	} else {
		is_final_cam_module = true;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump FROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/from_cal.bin", cal_buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
				info("Failedl to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);
exit:
	return ret;
}
#endif

#if defined(CONFIG_COMPANION_C2_USE)
int fimc_is_sec_readcal(struct fimc_is_core *core)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u16 id;

	struct fimc_is_vender_specific *specific = core->vender.private_data;

	/* reset spi */
	if (!core->spi0.device) {
		err("spi0 device is not available");
		goto exit;
	}

	ret = fimc_is_spi_reset(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_read_module_id(&core->spi0, &id, FROM_HEADER_MODULE_ID_START_ADDR, FROM_HEADER_MODULE_ID_SIZE);
	if (ret) {
		printk(KERN_ERR "fimc_is_spi_read_module_id (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Module ID = 0x%04x\n", id);

	ret = fimc_is_spi_read(&core->spi0, sysfs_finfo.cal_map_ver,
			       FROM_HEADER_CAL_MAP_VER_START_ADDR, FIMC_IS_CAL_MAP_VER_SIZE);
	if (ret) {
		printk(KERN_ERR "failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Cal map_version = %c%c%c%c\n", sysfs_finfo.cal_map_ver[0],
			sysfs_finfo.cal_map_ver[1], sysfs_finfo.cal_map_ver[2], sysfs_finfo.cal_map_ver[3]);

crc_retry:
	/* read cal data */
	info("Camera: SPI read cal data\n");
	ret = fimc_is_spi_read(&core->spi0, cal_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_START_ADDR]);
	sysfs_finfo.bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_END_ADDR]);
	info("Binary start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_START_ADDR]);
	sysfs_finfo.shading_end_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_END_ADDR]);
	info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
	sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_START_ADDR]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_END_ADDR]);
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	if (sysfs_finfo.setfile_end_addr < FROM_ISP_BINARY_SETFILE_START_ADDR
	|| sysfs_finfo.setfile_end_addr > FROM_ISP_BINARY_SETFILE_END_ADDR) {
		info("setfile end_addr has error!!  0x%08x\n", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	memcpy(sysfs_finfo.header_ver, &cal_buf[FROM_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.cal_map_ver, &cal_buf[FROM_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
	memcpy(sysfs_finfo.setfile_ver, &cal_buf[FROM_HEADER_ISP_SETFILE_VER_START_ADDR], FIMC_IS_ISP_SETFILE_VER_SIZE);
	sysfs_finfo.setfile_ver[FIMC_IS_ISP_SETFILE_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.project_name, &cal_buf[FROM_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
	sysfs_finfo.project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
	sysfs_finfo.header_section_crc_addr = FROM_CHECKSUM_HEADER_ADDR;

	sysfs_finfo.oem_start_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_START_ADDR]);
	sysfs_finfo.oem_end_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_END_ADDR]);
	info("OEM start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.oem_start_addr), (sysfs_finfo.oem_end_addr));
	memcpy(sysfs_finfo.oem_ver, &cal_buf[FROM_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
	sysfs_finfo.oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
	sysfs_finfo.oem_section_crc_addr = FROM_CHECKSUM_OEM_ADDR;

	sysfs_finfo.awb_start_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_START_ADDR]);
	sysfs_finfo.awb_end_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_END_ADDR]);
	info("AWB start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.awb_start_addr), (sysfs_finfo.awb_end_addr));
	memcpy(sysfs_finfo.awb_ver, &cal_buf[FROM_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
	sysfs_finfo.awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
	sysfs_finfo.awb_section_crc_addr = FROM_CHECKSUM_AWB_ADDR;

	memcpy(sysfs_finfo.shading_ver, &cal_buf[FROM_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
	sysfs_finfo.shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
	sysfs_finfo.shading_section_crc_addr = FROM_CHECKSUM_SHADING_ADDR;

	sysfs_finfo.concord_bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_START_ADDR]);
	sysfs_finfo.concord_bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_END_ADDR]);
	info("concord bin start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_bin_start_addr, sysfs_finfo.concord_bin_end_addr);
	sysfs_finfo.pdaf_cal_start_addr = *((u32 *)&cal_buf[FROM_HEADER_PDAF_CAL_START_ADDR]);
	sysfs_finfo.pdaf_cal_end_addr = *((u32 *)&cal_buf[FROM_HEADER_PDAF_CAL_END_ADDR]);
	info("pdaf start = 0x%08x, end = 0x%08x\n", sysfs_finfo.pdaf_cal_start_addr, sysfs_finfo.pdaf_cal_end_addr);
	sysfs_finfo.lsc_i0_gain_addr = FROM_SHADING_LSC_I0_GAIN_ADDR;
	info("Shading lsc_i0 start = 0x%08x\n", sysfs_finfo.lsc_i0_gain_addr);
	sysfs_finfo.lsc_j0_gain_addr = FROM_SHADING_LSC_J0_GAIN_ADDR;
	info("Shading lsc_j0 start = 0x%08x\n", sysfs_finfo.lsc_j0_gain_addr);
	sysfs_finfo.lsc_a_gain_addr = FROM_SHADING_LSC_A_GAIN_ADDR;
	info("Shading lsc_a start = 0x%08x\n", sysfs_finfo.lsc_a_gain_addr);
	sysfs_finfo.lsc_k4_gain_addr = FROM_SHADING_LSC_K4_GAIN_ADDR;
	info("Shading lsc_k4 start = 0x%08x\n", sysfs_finfo.lsc_k4_gain_addr);
	sysfs_finfo.lsc_scale_gain_addr = FROM_SHADING_LSC_SCALE_GAIN_ADDR;
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.lsc_scale_gain_addr);
	sysfs_finfo.lsc_gain_start_addr = FROM_SHADING_LSC_GAIN_START_ADDR;
	sysfs_finfo.lsc_gain_end_addr = FROM_SHADING_LSC_GAIN_END_ADDR;
	info("LSC start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.lsc_gain_start_addr, sysfs_finfo.lsc_gain_end_addr);

	memcpy(sysfs_finfo.concord_header_ver,
		&cal_buf[FROM_HEADER_CONCORD_HEADER_VER_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.concord_header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	sysfs_finfo.lsc_gain_crc_addr = FROM_SHADING_LSC_GAIN_CRC_ADDR;
	sysfs_finfo.pdaf_crc_addr = FROM_CONCORD_PDAF_CRC_ADDR;

	sysfs_finfo.xtalk_coef_start = FROM_CONCORD_XTALK_COEF_ADDR;
	sysfs_finfo.coef_offset_R = FROM_CONCORD_COEF_OFFSET_R_ADDR;
	sysfs_finfo.coef_offset_G = FROM_CONCORD_COEF_OFFSET_G_ADDR;
	sysfs_finfo.coef_offset_B = FROM_CONCORD_COEF_OFFSET_B_ADDR;
	sysfs_finfo.xtalk_coef_crc_addr = FROM_CONCORD_XTALK_COEF_CRC_ADDR;

	sysfs_finfo.concord_cal_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_CAL_START_ADDR]);
	sysfs_finfo.concord_cal_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_CAL_END_ADDR]);
	info("concord cal start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_cal_start_addr, sysfs_finfo.concord_cal_end_addr);
	sysfs_finfo.concord_master_setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MASTER_SETFILE_START_ADDR]);
	sysfs_finfo.concord_master_setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MASTER_SETFILE_START_ADDR]);
	sysfs_finfo.concord_mode_setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MODE_SETFILE_START_ADDR]);
	sysfs_finfo.concord_mode_setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MODE_SETFILE_END_ADDR]);
	sysfs_finfo.pdaf_start_addr = FROM_CONCORD_CAL_PDAF_START_ADDR;
	sysfs_finfo.pdaf_end_addr = FROM_CONCORD_CAL_PDAF_END_ADDR;
	info("pdaf start = 0x%04x, end = 0x%04x\n", sysfs_finfo.pdaf_start_addr, sysfs_finfo.pdaf_end_addr);

	sysfs_finfo.paf_cal_section_crc_addr = FROM_CHECKSUM_PAF_CAL_ADDR;
	sysfs_finfo.concord_cal_section_crc_addr = FROM_CHECKSUM_CONCORD_CAL_ADDR;

	sysfs_finfo.grasTuning_AwbAshCord_N_addr = FROM_SHADING_GRASTUNING_AWB_ASH_CORD_ADDR;
	info("Shading grasTuning_AwbAshCord_N start = 0x%08x\n", sysfs_finfo.grasTuning_AwbAshCord_N_addr);
	sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr = FROM_SHADING_GRASTUNING_AWB_ASH_CORD_INDEX_ADDR;
	info("Shading grasTuning_awbAshCordIndexes_N start = 0x%08x\n", sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr);
	sysfs_finfo.grasTuning_GASAlpha_M__N_addr = FROM_SHADING_GRASTUNING_GAS_ALPHA_ADDR;
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.grasTuning_GASAlpha_M__N_addr);
	sysfs_finfo.grasTuning_GASBeta_M__N_addr = FROM_SHADING_GRASTUNING_GAS_BETA_ADDR;
	info("Shading grasTuning_GASBeta_M__N start = 0x%08x\n", sysfs_finfo.grasTuning_GASBeta_M__N_addr);
	sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr = FROM_SHADING_GRASTUNING_GAS_OUTDOOR_ALPHA_ADDR;
	info("Shading grasTuning_GASOutdoorAlpha_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr);
	sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr = FROM_SHADING_GRASTUNING_GAS_OUTDOOR_BETA_ADDR;
	info("Shading grasTuning_GASOutdoorBeta_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr);
	sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr = FROM_SHADING_GRASTUNING_GAS_INDOOR_ALPHA_ADDR;
	info("Shading grasTuning_GASIndoorAlpha_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr);
	sysfs_finfo.grasTuning_GASIndoorBeta_N_addr = FROM_SHADING_GRASTUNING_GAS_INDOOR_BETA_ADDR;
	info("Shading grasTuning_GASIndoorBeta_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASIndoorBeta_N_addr);

	sysfs_finfo.pdaf_shad_start_addr = FROM_CONCORD_CAL_PDAF_SHADING_START_ADDR;
	sysfs_finfo.pdaf_shad_end_addr = FROM_CONCORD_CAL_PDAF_SHADING_END_ADDR;
	info("pdaf_shad start = 0x%04x, end = 0x%04x\n", sysfs_finfo.pdaf_shad_start_addr,
		sysfs_finfo.pdaf_shad_end_addr);
	sysfs_finfo.pdaf_shad_crc_addr = FROM_CONCORD_PDAF_SHAD_CRC_ADDR;
	sysfs_finfo.lsc_parameter_crc_addr = FROM_SHADING_LSC_PARAMETER_CRC_ARRD;
	info("xtalk_coef_start = 0x%04x\n", sysfs_finfo.xtalk_coef_start);
	info("lsc_gain_crc_addr = 0x%04x\n", sysfs_finfo.lsc_gain_crc_addr);
	info("pdaf_crc_addr = 0x%04x\n", sysfs_finfo.pdaf_crc_addr);
	info("pdaf_shad_crc_addr = 0x%04x\n", sysfs_finfo.pdaf_shad_crc_addr);
	info("xtalk_coef_crc_addr= 0x%04x\n", sysfs_finfo.xtalk_coef_crc_addr);
	info("lsc_parameter_crc_addr = 0x%04x\n", sysfs_finfo.lsc_parameter_crc_addr);

	fw_core_version = sysfs_finfo.header_ver[0];
	sysfs_finfo.fw_size = sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1;
	sysfs_finfo.setfile_size = sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1;
	sysfs_finfo.comp_fw_size = sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1;
	info("fw_size = %ld\n", sysfs_finfo.fw_size);
	info("setfile_size = %ld\n", sysfs_finfo.setfile_size);
	info("comp_fw_size = %ld\n", sysfs_finfo.comp_fw_size);

	sysfs_finfo.af_cal_pan = *((u32 *)&cal_buf[FROM_AF_CAL_PAN_ADDR]);
	sysfs_finfo.af_cal_macro = *((u32 *)&cal_buf[FROM_AF_CAL_MACRO_ARRD]);

	memcpy(sysfs_finfo.from_sensor_id, &cal_buf[FROM_HEADER_SENSOR_ID_ADDR], FIMC_IS_SENSOR_ID_SIZE);
	sysfs_finfo.from_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';

	/* debug info dump */
	info("++++ FROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", sysfs_finfo.header_ver);
	info("Companion version info : %s\n", sysfs_finfo.concord_header_ver);
	info(" ID : %c\n", sysfs_finfo.header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[FW_PIXEL_SIZE],
							sysfs_finfo.header_ver[FW_PIXEL_SIZE + 1]);
	info(" ISP ID : %c\n", sysfs_finfo.header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", sysfs_finfo.header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", sysfs_finfo.header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", sysfs_finfo.header_ver[FW_PUB_NUM],
							sysfs_finfo.header_ver[FW_PUB_NUM + 1]);
	info(" Manufacturer ID : %c\n", sysfs_finfo.header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", sysfs_finfo.header_ver[FW_VERSION_INFO]);
	info("Cal data map ver : %s\n", sysfs_finfo.cal_map_ver);
	info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	info("Project name : %s\n", sysfs_finfo.project_name);
	info("2. OEM info\n");
	info("Module info : %s\n", sysfs_finfo.oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", sysfs_finfo.awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", sysfs_finfo.shading_ver);
	info("---- FROM data info\n");

	/* CRC check */
	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
			retry--;
			goto crc_retry;
		}
	} else {
		fw_version_crc_check = false;
		crc32_check = false;
		crc32_c1_check = false;
	}

	if (sysfs_finfo.header_ver[3] == 'L') {
		crc32_check_factory = crc32_check;
		crc32_c1_check_factory = crc32_c1_check;
	} else {
		crc32_check_factory = false;
		crc32_c1_check_factory = false;
	}

	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (crc32_check && crc32_c1_check) {
			/* If FROM LSC value is not valid, loading default lsc data */
			if (*((u32 *)&cal_buf[sysfs_finfo.lsc_gain_start_addr]) == 0x00000000) {
				companion_lsc_isvalid = false;
			} else {
				companion_lsc_isvalid = true;
			}

			if (*((u32 *)&cal_buf[sysfs_finfo.xtalk_coef_start]) == 0x00000000) {
				companion_coef_isvalid = false;
			} else {
				companion_coef_isvalid = true;
			}
		} else {
			companion_lsc_isvalid = false;
			companion_coef_isvalid = false;
		}
	} else {
		companion_lsc_isvalid = true;
		companion_coef_isvalid = true;
	}

	if (!specific->use_module_check) {
		is_latest_cam_module = true;
	} else {
		if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
			is_latest_cam_module = true;
		} else {
			is_latest_cam_module = false;
		}
	}

	if (specific->use_module_check) {
		if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M
#if defined(CAMERA_MODULE_CORE_CS_VERSION) && defined(CAMERA_MODULE_CORE_CS_VERSION2)
		    && (sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
			|| sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION2)
#else
#if defined(CAMERA_MODULE_CORE_CS_VERSION)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
#endif
#if defined(CAMERA_MODULE_CORE_CS_VERSION2)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION2
#endif
#endif
		) {
			is_final_cam_module = true;
		} else {
			is_final_cam_module = false;
		}
	} else {
		is_final_cam_module = true;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump FROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/from_cal.bin", cal_buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
				info("Failedl to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);
exit:
	return ret;
}
#endif

#if defined(CONFIG_COMPANION_C3_USE)
int fimc_is_sec_readcal(struct fimc_is_core *core)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u16 id;

	struct fimc_is_vender_specific *specific = core->vender.private_data;

	/* reset spi */
	if (!core->spi0.device) {
		err("spi0 device is not available");
		goto exit;
	}

	ret = fimc_is_spi_reset(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_read_module_id(&core->spi0, &id, FROM_HEADER_MODULE_ID_START_ADDR, FROM_HEADER_MODULE_ID_SIZE);
	if (ret) {
		printk(KERN_ERR "fimc_is_spi_read_module_id (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Module ID = 0x%04x\n", id);

	ret = fimc_is_spi_read(&core->spi0, sysfs_finfo.cal_map_ver,
			       FROM_HEADER_CAL_MAP_VER_START_ADDR, FIMC_IS_CAL_MAP_VER_SIZE);
	if (ret) {
		printk(KERN_ERR "failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Cal map_version = %c%c%c%c\n", sysfs_finfo.cal_map_ver[0],
			sysfs_finfo.cal_map_ver[1], sysfs_finfo.cal_map_ver[2], sysfs_finfo.cal_map_ver[3]);

crc_retry:
	/* read cal data */
	info("Camera: SPI read cal data\n");
	ret = fimc_is_spi_read(&core->spi0, cal_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_START_ADDR]);
	sysfs_finfo.bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_END_ADDR]);
	info("Binary start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_START_ADDR]);
	sysfs_finfo.shading_end_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_END_ADDR]);
	info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
	sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_START_ADDR]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_END_ADDR]);
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
	sysfs_finfo.front_setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_FRONT_SETFILE_START_ADDR]);
	sysfs_finfo.front_setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_FRONT_SETFILE_END_ADDR]);
	info("Front setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.front_setfile_start_addr), (sysfs_finfo.front_setfile_end_addr));
#endif

	if (sysfs_finfo.setfile_end_addr < FROM_ISP_BINARY_SETFILE_START_ADDR
	|| sysfs_finfo.setfile_end_addr > FROM_ISP_BINARY_SETFILE_END_ADDR) {
		info("setfile end_addr has error!!  0x%08x\n", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	memcpy(sysfs_finfo.header_ver, &cal_buf[FROM_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.cal_map_ver, &cal_buf[FROM_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
	memcpy(sysfs_finfo.setfile_ver, &cal_buf[FROM_HEADER_ISP_SETFILE_VER_START_ADDR], FIMC_IS_ISP_SETFILE_VER_SIZE);
	sysfs_finfo.setfile_ver[FIMC_IS_ISP_SETFILE_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.project_name, &cal_buf[FROM_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
	sysfs_finfo.project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
	sysfs_finfo.header_section_crc_addr = FROM_CHECKSUM_HEADER_ADDR;

	sysfs_finfo.concord_bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_START_ADDR]);
	sysfs_finfo.concord_bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_END_ADDR]);
	info("concord bin start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_bin_start_addr, sysfs_finfo.concord_bin_end_addr);
	sysfs_finfo.pdaf_cal_start_addr = *((u32 *)&cal_buf[FROM_HEADER_PDAF_CAL_START_ADDR]);
	sysfs_finfo.pdaf_cal_end_addr = *((u32 *)&cal_buf[FROM_HEADER_PDAF_CAL_END_ADDR]);
	info("pdaf start = 0x%08x, end = 0x%08x\n", sysfs_finfo.pdaf_cal_start_addr, sysfs_finfo.pdaf_cal_end_addr);
	sysfs_finfo.lsc_i0_gain_addr = FROM_SHADING_LSC_I0_GAIN_ADDR;
	info("Shading lsc_i0 start = 0x%08x\n", sysfs_finfo.lsc_i0_gain_addr);
	sysfs_finfo.lsc_j0_gain_addr = FROM_SHADING_LSC_J0_GAIN_ADDR;
	info("Shading lsc_j0 start = 0x%08x\n", sysfs_finfo.lsc_j0_gain_addr);
	sysfs_finfo.lsc_a_gain_addr = FROM_SHADING_LSC_A_GAIN_ADDR;
	info("Shading lsc_a start = 0x%08x\n", sysfs_finfo.lsc_a_gain_addr);
	sysfs_finfo.lsc_k4_gain_addr = FROM_SHADING_LSC_K4_GAIN_ADDR;
	info("Shading lsc_k4 start = 0x%08x\n", sysfs_finfo.lsc_k4_gain_addr);
	sysfs_finfo.lsc_scale_gain_addr = FROM_SHADING_LSC_SCALE_GAIN_ADDR;
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.lsc_scale_gain_addr);
	sysfs_finfo.lsc_gain_start_addr = FROM_SHADING_LSC_GAIN_START_ADDR;
	sysfs_finfo.lsc_gain_end_addr = FROM_SHADING_LSC_GAIN_END_ADDR;
	info("LSC start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.lsc_gain_start_addr, sysfs_finfo.lsc_gain_end_addr);

	memcpy(sysfs_finfo.concord_header_ver,
		&cal_buf[FROM_HEADER_CONCORD_HEADER_VER_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.concord_header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	sysfs_finfo.lsc_gain_crc_addr = FROM_SHADING_LSC_GAIN_CRC_ADDR;
	sysfs_finfo.pdaf_crc_addr = FROM_CONCORD_PDAF_CRC_ADDR;

	sysfs_finfo.cal_data_section_crc_addr = FROM_CHECKSUM_CAL_DATA_ADDR;
	sysfs_finfo.cal_data_start_addr = FROM_CAL_DATA_START_ADDR;
	sysfs_finfo.cal_data_end_addr = FROM_CAL_DATA_END_ADDR;

	sysfs_finfo.grasTuning_AwbAshCord_N_addr = FROM_SHADING_GRASTUNING_AWB_ASH_CORD_ADDR;
	info("Shading grasTuning_AwbAshCord_N start = 0x%08x\n", sysfs_finfo.grasTuning_AwbAshCord_N_addr);
	sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr = FROM_SHADING_GRASTUNING_AWB_ASH_CORD_INDEX_ADDR;
	info("Shading grasTuning_awbAshCordIndexes_N start = 0x%08x\n", sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr);
	sysfs_finfo.grasTuning_GASAlpha_M__N_addr = FROM_SHADING_GRASTUNING_GAS_ALPHA_ADDR;
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.grasTuning_GASAlpha_M__N_addr);
	sysfs_finfo.grasTuning_GASBeta_M__N_addr = FROM_SHADING_GRASTUNING_GAS_BETA_ADDR;
	info("Shading grasTuning_GASBeta_M__N start = 0x%08x\n", sysfs_finfo.grasTuning_GASBeta_M__N_addr);
	sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr = FROM_SHADING_GRASTUNING_GAS_OUTDOOR_ALPHA_ADDR;
	info("Shading grasTuning_GASOutdoorAlpha_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr);
	sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr = FROM_SHADING_GRASTUNING_GAS_OUTDOOR_BETA_ADDR;
	info("Shading grasTuning_GASOutdoorBeta_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr);
	sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr = FROM_SHADING_GRASTUNING_GAS_INDOOR_ALPHA_ADDR;
	info("Shading grasTuning_GASIndoorAlpha_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr);
	sysfs_finfo.grasTuning_GASIndoorBeta_N_addr = FROM_SHADING_GRASTUNING_GAS_INDOOR_BETA_ADDR;
	info("Shading grasTuning_GASIndoorBeta_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASIndoorBeta_N_addr);

	sysfs_finfo.pdaf_shad_start_addr = FROM_CONCORD_CAL_PDAF_SHADING_START_ADDR;
	sysfs_finfo.pdaf_shad_end_addr = FROM_CONCORD_CAL_PDAF_SHADING_END_ADDR;
	info("pdaf_shad start = 0x%04x, end = 0x%04x\n", sysfs_finfo.pdaf_shad_start_addr,
		sysfs_finfo.pdaf_shad_end_addr);
	sysfs_finfo.pdaf_shad_crc_addr = FROM_CONCORD_PDAF_SHAD_CRC_ADDR;
	sysfs_finfo.lsc_parameter_crc_addr = FROM_SHADING_LSC_PARAMETER_CRC_ARRD;
	info("lsc_gain_crc_addr = 0x%04x\n", sysfs_finfo.lsc_gain_crc_addr);
	info("pdaf_crc_addr = 0x%04x\n", sysfs_finfo.pdaf_crc_addr);
	info("pdaf_shad_crc_addr = 0x%04x\n", sysfs_finfo.pdaf_shad_crc_addr);
	info("lsc_parameter_crc_addr = 0x%04x\n", sysfs_finfo.lsc_parameter_crc_addr);

	fw_core_version = sysfs_finfo.header_ver[0];
	sysfs_finfo.fw_size = sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1;
	sysfs_finfo.setfile_size = sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1;
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
	sysfs_finfo.front_setfile_size = sysfs_finfo.front_setfile_end_addr - sysfs_finfo.front_setfile_start_addr + 1;
#endif
	sysfs_finfo.comp_fw_size = sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1;
	info("fw_size = %ld\n", sysfs_finfo.fw_size);
	info("setfile_size = %ld\n", sysfs_finfo.setfile_size);
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
	info("front setfile_size = %ld\n", sysfs_finfo.front_setfile_size);
#endif
	info("comp_fw_size = %ld\n", sysfs_finfo.comp_fw_size);

	sysfs_finfo.af_cal_pan = *((u32 *)&cal_buf[FROM_AF_CAL_PAN_ADDR]);
	sysfs_finfo.af_cal_macro = *((u32 *)&cal_buf[FROM_AF_CAL_MACRO_ARRD]);
	sysfs_finfo.sensor_version= *((u8 *)&cal_buf[FROM_HEADER_SENSOR_VERSION_ADDR]);

	memcpy(sysfs_finfo.from_sensor_id, &cal_buf[FROM_HEADER_SENSOR_ID_ADDR], FIMC_IS_SENSOR_ID_SIZE);
	sysfs_finfo.from_sensor_id[FIMC_IS_SENSOR_ID_SIZE] = '\0';
#ifdef FROM_HEADER_MODULE_ID_ADDR
	memcpy(sysfs_finfo.from_module_id, &cal_buf[FROM_HEADER_MODULE_ID_ADDR], FIMC_IS_MODULE_ID_SIZE);
#else
	memset(sysfs_finfo.from_module_id, 0x0, FIMC_IS_MODULE_ID_SIZE);
#endif

	/* debug info dump */
	info("++++ FROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", sysfs_finfo.header_ver);
	info("Companion version info : %s\n", sysfs_finfo.concord_header_ver);
	info(" ID : %c\n", sysfs_finfo.header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[FW_PIXEL_SIZE],
							sysfs_finfo.header_ver[FW_PIXEL_SIZE + 1]);
	info(" ISP ID : %c\n", sysfs_finfo.header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", sysfs_finfo.header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", sysfs_finfo.header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", sysfs_finfo.header_ver[FW_PUB_NUM],
							sysfs_finfo.header_ver[FW_PUB_NUM + 1]);
	info(" Manufacturer ID : %c\n", sysfs_finfo.header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", sysfs_finfo.header_ver[FW_VERSION_INFO]);
	info("Cal data map ver : %s\n", sysfs_finfo.cal_map_ver);
	info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	info("Project name : %s\n", sysfs_finfo.project_name);
	info("SENSOR ID : 0x%02x\n", sysfs_finfo.sensor_version);
	info("MODULE ID : %c%c%c%c%c%02X%02X%02X%02X%02X\n", 
		sysfs_finfo.from_module_id[0], sysfs_finfo.from_module_id[1], sysfs_finfo.from_module_id[2], 
		sysfs_finfo.from_module_id[3], sysfs_finfo.from_module_id[4], sysfs_finfo.from_module_id[5],
		sysfs_finfo.from_module_id[6], sysfs_finfo.from_module_id[7], sysfs_finfo.from_module_id[8],
		sysfs_finfo.from_module_id[9]);
	info("---- FROM data info\n");

	/* CRC check */
	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
			retry--;
			goto crc_retry;
		}
	} else {
		fw_version_crc_check = false;
		crc32_check = false;
		crc32_c1_check = false;
	}

	if (sysfs_finfo.header_ver[3] == 'L') {
		crc32_check_factory = crc32_check;
		crc32_c1_check_factory = crc32_c1_check;
	} else {
		crc32_check_factory = false;
		crc32_c1_check_factory = false;
	}

	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (crc32_check && crc32_c1_check) {
			/* If FROM LSC value is not valid, loading default lsc data */
			if (*((u32 *)&cal_buf[sysfs_finfo.lsc_gain_start_addr]) == 0x00000000) {
				companion_lsc_isvalid = false;
			} else {
				companion_lsc_isvalid = true;
			}
		} else {
			companion_lsc_isvalid = false;
		}
	} else {
		companion_lsc_isvalid = true;
	}

	if (!specific->use_module_check) {
		is_latest_cam_module = true;
	} else {
		if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION_REAR) {
			is_latest_cam_module = true;
		} else {
			is_latest_cam_module = false;
		}
	}

	if (specific->use_module_check) {
		if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M
#if defined(CAMERA_MODULE_CORE_CS_VERSION) && defined(CAMERA_MODULE_CORE_CS_VERSION2)
		    && (sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
			|| sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION2)
#else
#if defined(CAMERA_MODULE_CORE_CS_VERSION)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION
#endif
#if defined(CAMERA_MODULE_CORE_CS_VERSION2)
		    && sysfs_finfo.header_ver[0] == CAMERA_MODULE_CORE_CS_VERSION2
#endif
#endif
		) {
			is_final_cam_module = true;
		} else {
			is_final_cam_module = false;
		}
	} else {
		is_final_cam_module = true;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump FROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/from_cal.bin", cal_buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
				info("Failedl to dump cal data.\n");
				goto dump_err;
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);
exit:
	return ret;
}
#endif

#ifdef CAMERA_MODULE_DUALIZE
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
int fimc_is_sec_get_front_setf_name(struct fimc_is_core *core)
{
	int ret = 0;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	sensor_id = specific->front_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	memcpy(sysfs_finfo.load_front_setfile_name, module->setfile_name,
		sizeof(sysfs_finfo.load_front_setfile_name));

p_err:
	return ret;
}
#endif

int fimc_is_sec_readfw(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char fw_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	u8 *buf = NULL;
#endif

	info("Camera: FW need to be dumped\n");

crc_retry:
	/* read fw data */
	if (FIMC_IS_MAX_FW_BUFFER_SIZE >= FIMC_IS_MAX_FW_SIZE) {
		info("Camera: Start SPI read fw data\n");
		memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
		ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.bin_start_addr, FIMC_IS_MAX_FW_SIZE);
		if (ret) {
			err("failed to fimc_is_spi_read (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
		info("Camera: End SPI read fw data\n");

		/* CRC check */
		if (!fimc_is_sec_check_fw_crc32(fw_buf) && (retry > 0)) {
			retry--;
			goto crc_retry;
		} else if (!retry) {
			ret = -EINVAL;
			goto exit;
		}

		snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_fw_name);
		pos = 0;

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
		buf = (u8 *)fw_buf;
		fimc_is_sec_inflate_fw(&buf, &sysfs_finfo.fw_size);
#endif
		if (write_data_to_file(fw_path, fw_buf, sysfs_finfo.fw_size, &pos) < 0) {
			ret = -EIO;
			goto exit;
		}

		info("Camera: FW Data has dumped successfully\n");
	} else {
		err("FW size is larger than FW buffer.\n");
		BUG();
	}

	info("Camera: FW Data has dumped successfully\n");

exit:
	return ret;
}

int fimc_is_sec_read_setfile(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	u8 *buf = NULL;
#endif

	info("Camera: Setfile need to be dumped\n");

setfile_crc_retry:
	/* read setfile data */
	info("Camera: Start SPI read setfile data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_C)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_D)
		|| fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B)) {
		ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.setfile_start_addr,
			FIMC_IS_MAX_SETFILE_SIZE_LL);
	} else {
		ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.setfile_start_addr,
			FIMC_IS_MAX_SETFILE_SIZE_LS);
	}
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read setfile data\n");

	/* CRC check */
	if (!fimc_is_sec_check_setfile_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto setfile_crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_setfile_name);
	pos = 0;

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	buf = (u8 *)fw_buf;
	fimc_is_sec_inflate_fw(&buf, &sysfs_finfo.setfile_size);
#endif
	if (write_data_to_file(setfile_path, fw_buf, sysfs_finfo.setfile_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Setfile has dumped successfully\n");

exit:
	return ret;
}

#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
int fimc_is_sec_read_front_setfile(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	u8 *buf = NULL;
#endif

	info("Camera: Front setfile need to be dumped\n");

setfile_crc_retry:
	/* read setfile data */
	info("Camera: Start SPI read front setfile data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
	
	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.front_setfile_start_addr,
		FIMC_IS_MAX_SETFILE_SIZE_FRONT_LL);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read front setfile data\n");

	/* CRC check */
	if (!fimc_is_sec_check_front_setfile_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto setfile_crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	fimc_is_sec_get_front_setf_name(core);
	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH,
		sysfs_finfo.load_front_setfile_name);
	pos = 0;

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
	buf = (u8 *)fw_buf;
	fimc_is_sec_inflate_fw(&buf, &sysfs_finfo.front_setfile_size);
#endif
	if (write_data_to_file(setfile_path, fw_buf, sysfs_finfo.front_setfile_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Front setfile has dumped successfully\n");

exit:
	return ret;
}
#endif

#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
int fimc_is_sec_inflate_fw(u8 **buf, unsigned long *size)
{
	z_stream zs_inflate;
	int ret = 0;
	char *unzip_buf;

	unzip_buf = vmalloc(FIMC_IS_MAX_FW_BUFFER_SIZE);
	if (!unzip_buf) {
		err("failed to allocate memory\n");
		ret = -ENOMEM;
		goto exit;
	}
	memset(unzip_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);

	zs_inflate.workspace = vmalloc(zlib_inflate_workspacesize());
	ret = zlib_inflateInit2(&zs_inflate, -MAX_WBITS);
	if (ret != Z_OK) {
		err("Camera : inflateInit error\n");
	}

	zs_inflate.next_in = *buf;
	zs_inflate.next_out = unzip_buf;
	zs_inflate.avail_in = *size;
	zs_inflate.avail_out = FIMC_IS_MAX_FW_BUFFER_SIZE;

	ret = zlib_inflate(&zs_inflate, Z_NO_FLUSH);
	if (ret != Z_STREAM_END) {
		err("Camera : zlib_inflate error\n");
	}

	zlib_inflateEnd(&zs_inflate);
	vfree(zs_inflate.workspace);

	*size = FIMC_IS_MAX_FW_BUFFER_SIZE - zs_inflate.avail_out;
	memset(*buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
	memcpy(*buf, unzip_buf, *size);
	vfree(unzip_buf);

exit:
	return ret;
}
#endif
#endif
#endif

#ifdef CAMERA_MODULE_DUALIZE
#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_read_companion_fw(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char fw_path[100];
	//char master_setfile_path[100];
	//char mode_setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;

	info("Camera: Companion FW, Setfile need to be dumped\n");

crc_retry:
	/* read companion fw data */
	info("Camera: Start SPI read companion fw data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.concord_bin_start_addr, FIMC_IS_MAX_COMPANION_FW_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read companion fw data\n");

	/* CRC check */
	if (!fimc_is_sec_check_companion_fw_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s",
		FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_fw_name);

	if (write_data_to_file(fw_path, fw_buf, sysfs_finfo.comp_fw_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Companion FW Data has dumped successfully\n");
#if 0
	snprintf(master_setfile_path, sizeof(master_setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_mastersetf_name);
	snprintf(mode_setfile_path, sizeof(mode_setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_modesetf_name);
	pos = 0;

	if (write_data_to_file(master_setfile_path,
			comp_fw_buf + sysfs_finfo.concord_master_setfile_start_addr,
			sysfs_finfo.concord_master_setfile_end_addr - sysfs_finfo.concord_master_setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}
	pos = 0;
	if (write_data_to_file(mode_setfile_path,
			comp_fw_buf + sysfs_finfo.concord_mode_setfile_start_addr,
			sysfs_finfo.concord_mode_setfile_end_addr - sysfs_finfo.concord_mode_setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Companion Setfile has dumped successfully\n");
#endif

exit:
	return ret;
}
#endif
#endif

#if 0
int fimc_is_sec_gpio_enable(struct exynos_platform_fimc_is *pdata, char *name, bool on)
{
	struct gpio_set *gpio;
	int ret = 0;
	int i = 0;

	for (i = 0; i < FIMC_IS_MAX_GPIO_NUM; i++) {
			gpio = &pdata->gpio_info->cfg[i];
			if (strcmp(gpio->name, name) == 0)
				break;
			else
				continue;
	}

	if (i == FIMC_IS_MAX_GPIO_NUM) {
		err("GPIO %s is not found!!", name);
		ret = -EINVAL;
		goto exit;
	}

	ret = gpio_request(gpio->pin, gpio->name);
	if (ret) {
		err("Request GPIO error(%s)", gpio->name);
		goto exit;
	}

	if (on) {
		switch (gpio->act) {
		case GPIO_PULL_NONE:
			s3c_gpio_cfgpin(gpio->pin, gpio->value);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			break;
		case GPIO_OUTPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_INPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_RESET:
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_direction_output(gpio->pin, 0);
			gpio_direction_output(gpio->pin, 1);
			break;
		default:
			err("unknown act for gpio");
			break;
		}
	} else {
		s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_DOWN);
	}

	gpio_free(gpio->pin);

exit:
	return ret;
}
#endif

int fimc_is_sec_get_pixel_size(char *header_ver)
{
	int pixelsize = 0;

	pixelsize += (int) (header_ver[FW_PIXEL_SIZE] - 0x30) * 10;
	pixelsize += (int) (header_ver[FW_PIXEL_SIZE + 1] - 0x30);

	return pixelsize;
}

int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver)
{
	struct regulator *regulator = NULL;
	int ret = 0;
	int minV, maxV;
	int pixelSize = 0;

	regulator = regulator_get(dev, "cam_sensor_core_1.2v");
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get fail",
			__func__);
		return -EINVAL;
	}
	pixelSize = fimc_is_sec_get_pixel_size(header_ver);

	if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SONY) {
		if (pixelSize == 13) {
			minV = 1050000;
			maxV = 1050000;
		} else if (pixelSize == 8) {
			minV = 1100000;
			maxV = 1100000;
		} else {
			minV = 1050000;
			maxV = 1050000;
		}
	} else if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SLSI) {
		minV = 1200000;
		maxV = 1200000;
	} else {
		minV = 1050000;
		maxV = 1050000;
	}

	ret = regulator_set_voltage(regulator, minV, maxV);

	if (ret >= 0)
		info("%s : set_core_voltage %d, %d successfully\n",
				__func__, minV, maxV);
	regulator_put(regulator);

	return ret;
}

int fimc_is_sec_fw_find(struct fimc_is_core *core)
{
	int sensor_id = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_F) ||
	    fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_I) ||
	    fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_A) ||
	    fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_B) ||
	    fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_C)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
		specific->rear_sensor_id = SENSOR_NAME_S5K2P2;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_12M)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
		specific->rear_sensor_id = SENSOR_NAME_S5K2P2_12M;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P3)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
		specific->rear_sensor_id = SENSOR_NAME_S5K2P3;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3P3)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_3P3), "%s", FIMC_IS_FW_3P3);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_3P3_SETF), "%s", FIMC_IS_3P3_SETF);
		specific->rear_sensor_id = SENSOR_NAME_S5K3P3;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4H5)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
		specific->rear_sensor_id = SENSOR_NAME_S5K4H5;
        } else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4H5YC_P)) {
                snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
                snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
                specific->rear_sensor_id = SENSOR_NAME_S5K4H5YC_FF;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX240_A) ||
	           fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX240_B) ||
	           fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX240_C)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX240), "%s", FIMC_IS_FW_IMX240);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX240_SETF), "%s", FIMC_IS_IMX240_SETF);
		specific->rear_sensor_id = SENSOR_NAME_IMX240;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX260_C)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX260), "%s", FIMC_IS_FW_IMX260);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX260_SETF), "%s", FIMC_IS_IMX260_SETF);
		specific->rear_sensor_id = SENSOR_NAME_IMX260;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX260_D)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX260_D), "%s", FIMC_IS_FW_IMX260_D);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX260_SETF_D), "%s", FIMC_IS_IMX260_SETF_D);
		specific->rear_sensor_id = SENSOR_NAME_IMX260;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX228)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX228), "%s", FIMC_IS_FW_IMX228);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX228_SETF), "%s", FIMC_IS_IMX228_SETF);
		specific->rear_sensor_id = SENSOR_NAME_IMX228;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2T2)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2T2_EVT1), "%s", FIMC_IS_FW_2T2_EVT1);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2T2_SETF), "%s", FIMC_IS_2T2_SETF);
		specific->rear_sensor_id = SENSOR_NAME_S5K2T2;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_C)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2L1), "%s", FIMC_IS_FW_2L1);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2L1_SETF), "%s", FIMC_IS_2L1_SETF);
		specific->rear_sensor_id = SENSOR_NAME_S5K2L1;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2L1_D)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2L1_D), "%s", FIMC_IS_FW_2L1_D);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2L1_SETF_D), "%s", FIMC_IS_2L1_SETF_D);
		specific->rear_sensor_id = SENSOR_NAME_S5K2L1;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4E6)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4E6), "%s", FIMC_IS_FW_4E6);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4E6_SETF), "%s", FIMC_IS_4E6_SETF);
		specific->rear_sensor_id = SENSOR_NAME_S5K4E6;
	} else {
		/* default firmware and setfile */
		sensor_id = specific->rear_sensor_id;
		if (sensor_id == SENSOR_NAME_IMX240) {
			/* IMX240 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX240), "%s", FIMC_IS_FW_IMX240);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX240_SETF), "%s", FIMC_IS_IMX240_SETF);
		} else if (sensor_id == SENSOR_NAME_IMX228) {
			/* IMX228 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX228), "%s", FIMC_IS_FW_IMX228);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX228_SETF), "%s", FIMC_IS_IMX228_SETF);
		} else if (sensor_id == SENSOR_NAME_IMX134) {
			/* IMX134 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K2P2_12M) {
			/* 2P2_12M */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K2P2) {
			/* 2P2 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K4H5 ||
			   sensor_id == SENSOR_NAME_S5K4H5YC ||
			   sensor_id == SENSOR_NAME_S5K4H5YC_FF) {
			/* 4H5 or 4H5YC or 4H5YC_FF*/
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
		} else if ( sensor_id == SENSOR_NAME_S5K2P3 ) {
			/* 2P3 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K2T2) {
			/* 2T2 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2T2_EVT1), "%s", FIMC_IS_FW_2T2_EVT1);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2T2_SETF), "%s", FIMC_IS_2T2_SETF);
		} else if (sensor_id == SENSOR_NAME_IMX260) {
			/* IMX260 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX260), "%s", FIMC_IS_FW_IMX260);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX260_SETF), "%s", FIMC_IS_IMX260_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K3P3) {
			/* 3P3 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_3P3), "%s", FIMC_IS_FW_3P3);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_3P3_SETF), "%s", FIMC_IS_3P3_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K2L1) {
			/* 2L1 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2L1), "%s", FIMC_IS_FW_2L1);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2L1_SETF), "%s", FIMC_IS_2L1_SETF);
		} else if (sensor_id == SENSOR_NAME_S5K4E6) {
			/* 4E6 */
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4E6), "%s", FIMC_IS_FW_4E6);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4E6_SETF), "%s", FIMC_IS_4E6_SETF);
		} else {
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
		}
	}

	return 0;
}

int fimc_is_sec_run_fw_sel(struct device *dev, int position)
{
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	int ret = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	/* Check reload cal data enabled */
	if (!sysfs_finfo.is_check_cal_reload) {
		fimc_is_sec_check_reload(core);
	}
	sysfs_finfo.is_check_cal_reload = true;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_REAR, true);
#else
/* When using C2 retention, Cal loading for both front and rear cam will be done at a time */
#if !defined(CONFIG_COMPANION_C2_USE) && !defined(CONFIG_COMPANION_C3_USE)
			ret = fimc_is_sec_fw_sel(core, dev, true);
#endif
#endif
		}

		if (!sysfs_finfo_front.is_caldata_read || force_caldata_dump) {
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_FRONT, false);
			if (ret < 0) {
				err("failed to select firmware (%d)", ret);
				goto p_err;
			}
		}
	} else
#endif
	{
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_REAR, false);
#else
			ret = fimc_is_sec_fw_sel(core, dev, false);
#endif
			if (ret < 0) {
				err("failed to select firmware (%d)", ret);
				goto p_err;
			}
		}
	}

p_err:
	if (position == SENSOR_POSITION_REAR) {
		if (specific->check_sensor_vendor) {
			if (fimc_is_sec_check_from_ver(core, position)) {
				if (sysfs_finfo.header_ver[3] != 'L') {
					err("Not supported module. Module ver = %s", sysfs_finfo.header_ver);
					return  -EIO;
				}
			}
		}
	}
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	else {
		if (specific->check_sensor_vendor) {
			if (fimc_is_sec_check_from_ver(core, position)) {
				if (sysfs_finfo_front.header_ver[3] != 'L') {
					err("Not supported front module. Module ver = %s", sysfs_finfo_front.header_ver);
					return  -EIO;
				}
			}
		}
	}
#endif

	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_fw_sel_eeprom(struct device *dev, int id, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_ldo_enabled[2];
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	is_ldo_enabled[0] = false;
	is_ldo_enabled[1] = false;

	/* Use mutex for i2c read */
	mutex_lock(&specific->spi_lock);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		if (!sysfs_finfo_front.is_caldata_read || force_caldata_dump) {
			if (force_caldata_dump)
				info("forced caldata dump!!\n");

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_VT")) {
				fimc_is_sec_rom_power_on(core, SENSOR_POSITION_FRONT);
				is_ldo_enabled[0] = true;
			}

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
				fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);
				is_ldo_enabled[1] = true;
			}

			info("Camera: read cal data from Front EEPROM\n");
			if (!fimc_is_sec_readcal_eeprom(dev, SENSOR_POSITION_FRONT)) {
				sysfs_finfo_front.is_caldata_read = true;
			}
		}
		goto exit;
	} else
#endif
	{
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
			is_dumped_fw_loading_needed = false;
			if (force_caldata_dump)
				info("forced caldata dump!!\n");

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
				fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);
				is_ldo_enabled[0] = true;
			}

			info("Camera: read cal data from Rear EEPROM\n");
			if (headerOnly) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
				fimc_is_sec_read_eeprom_header(dev, SENSOR_POSITION_REAR);
#endif
			} else {
				if (!fimc_is_sec_readcal_eeprom(dev, SENSOR_POSITION_REAR)) {
					sysfs_finfo.is_caldata_read = true;
				}
			}
		}
	}

	fimc_is_sec_fw_find(core);
	if (headerOnly) {
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, sysfs_finfo.load_fw_name);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		err("Camera: Failed open phone firmware");
		ret = -EIO;
		fp = NULL;
		goto read_phone_fw_exit;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n",
		fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
	if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
		memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
		temp_buf = fw_buf;
	} else
#endif
	{
		info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
		read_buf = vmalloc(fsize);
		if (!read_buf) {
			err("failed to allocate memory");
			ret = -ENOMEM;
			goto read_phone_fw_exit;
		}
		temp_buf = read_buf;
	}
	nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		err("failed to read firmware file, %ld Bytes", nread);
		ret = -EIO;
		goto read_phone_fw_exit;
	}

	strncpy(phone_fw_version, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
	strncpy(sysfs_pinfo.header_ver, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
	info("Camera: phone fw version: %s\n", phone_fw_version);

read_phone_fw_exit:
	if (read_buf) {
		vfree(read_buf);
		read_buf = NULL;
		temp_buf = NULL;
	}

	if (fp) {
		filp_close(fp, current->files);
		fp = NULL;
	}

	set_fs(old_fs);

exit:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		if (is_ldo_enabled[0] && !specific->running_front_camera)
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_FRONT);

		if (is_ldo_enabled[1] && !specific->running_rear_camera)
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);
	} else
#endif
	{
		if (is_ldo_enabled[0] && !specific->running_rear_camera)
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);
	}

	mutex_unlock(&specific->spi_lock);

	return ret;
}
#endif

#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_fw_sel(struct fimc_is_core *core, struct device *dev, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char dump_fw_path[100];
	char dump_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
#if defined(CAMERA_MODULE_DUALIZE) && defined(CAMERA_MODULE_AVAILABLE_DUMP_VERSION)
	int from_fw_revision = 0;
	int dump_fw_revision = 0;
	int phone_fw_revision = 0;
	bool dump_flag = false;
	struct file *setfile_fp = NULL;
	char setfile_path[100];
#endif
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = false;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_spi *spi = &core->spi0;
#endif
	bool is_ldo_enabled = false;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null\n");
		return -EINVAL;
	}

	/* Use mutex for spi read */
	mutex_lock(&specific->spi_lock);
	if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
		is_dumped_fw_loading_needed = false;
		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
			fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);
			is_ldo_enabled = true;
		}
		info("read cal data from FROM\n");
#ifdef CONFIG_COMPANION_USE
		/* Set SPI function */
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_HOST);
#endif

		if (headerOnly) {
			fimc_is_sec_read_from_header(dev);
		} else {
			if (!fimc_is_sec_readcal(core)) {
				sysfs_finfo.is_caldata_read = true;
			}
		}

		/*select AF actuator*/
		if (!crc32_header_check) {
			info("Camera : CRC32 error for all section.\n");
		}

		fimc_is_sec_fw_find(core);
		if (headerOnly) {
			goto exit;
		}

		snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, sysfs_finfo.load_fw_name);

		snprintf(dump_fw_path, sizeof(dump_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_fw_name);
		info("Camera: f-rom fw version: %s\n", sysfs_finfo.header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		fp = filp_open(dump_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			info("Camera: There is no dumped firmware(%s)\n", dump_fw_path);
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			dump_fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Dumped FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_fw_version, temp_buf + nread-11, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: dumped fw version: %s\n", dump_fw_version);

read_phone_fw:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			err("Camera: Failed open phone firmware(%s)", fw_path);
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n", fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw_exit;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_fw_version, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
		strncpy(sysfs_pinfo.header_ver, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: phone fw version: %s\n", phone_fw_version);

read_phone_fw_exit:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

#if defined(CAMERA_MODULE_DUALIZE) && defined(CAMERA_MODULE_AVAILABLE_DUMP_VERSION)
		if (!strncmp(CAMERA_MODULE_AVAILABLE_DUMP_VERSION, sysfs_finfo.header_ver, 5)) {
			from_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.header_ver);
			phone_fw_revision = fimc_is_sec_fw_revision(phone_fw_version);
			if (is_dump_existed) {
				dump_fw_revision = fimc_is_sec_fw_revision(dump_fw_version);
			}

			info("from_fw_revision = %d, phone_fw_revision = %d, dump_fw_revision = %d\n",
				from_fw_revision, phone_fw_revision, dump_fw_revision);

			if (fimc_is_sec_compare_ver(SENSOR_POSITION_REAR) /* Check if a module is connected or not */
				&& (!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, phone_fw_version) ||
				   (from_fw_revision > phone_fw_revision))) {
				is_dumped_fw_loading_needed = true;
				if (is_dump_existed) {
					if (!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver,
								dump_fw_version)) {
						is_dump_needed = true;
					} else if (from_fw_revision > dump_fw_revision) {
						is_dump_needed = true;
					} else {
						is_dump_needed = false;
					}
				} else {
					is_dump_needed = true;
				}
			} else {
				is_dump_needed = false;
				if (is_dump_existed) {
					if (!fimc_is_sec_fw_module_compare(phone_fw_version,
						dump_fw_version)) {
						is_dumped_fw_loading_needed = false;
					} else if (phone_fw_revision > dump_fw_revision) {
						is_dumped_fw_loading_needed = false;
					} else {
						is_dumped_fw_loading_needed = true;
					}
				} else {
					is_dumped_fw_loading_needed = false;
				}
			}

			if (force_caldata_dump) {
				if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, phone_fw_version))
					|| (from_fw_revision > phone_fw_revision))
					dump_flag = true;
			} else {
				if (is_dump_needed) {
					dump_flag = true;
					crc32_fw_check = false;
					crc32_setfile_check = false;
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
					crc32_front_setfile_check = false;
#endif
				}
			}

			if (dump_flag) {
				info("Dump ISP Firmware.\n");
				ret = fimc_is_sec_readfw(core);
				msleep(20);
				ret |= fimc_is_sec_read_setfile(core);
#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
				msleep(20);
				ret |= fimc_is_sec_read_front_setfile(core);
#endif
				if (ret < 0) {
					if (!crc32_fw_check || !crc32_setfile_check || !crc32_front_setfile_check) {
						is_dumped_fw_loading_needed = false;
						err("Firmware CRC is not valid. Does not use dumped firmware.\n");
					}
				}
			}

			if (phone_fw_version[0] == 0) {
				strcpy(sysfs_pinfo.header_ver, "NULL");
			}

			if (is_dumped_fw_loading_needed) {
				old_fs = get_fs();
				set_fs(KERNEL_DS);
				snprintf(setfile_path, sizeof(setfile_path), "%s%s",
					FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_setfile_name);
				setfile_fp = filp_open(setfile_path, O_RDONLY, 0);
				if (IS_ERR_OR_NULL(setfile_fp)) {
					crc32_setfile_check = false;
					info("setfile does not exist. Retry setfile dump.\n");

					fimc_is_sec_read_setfile(core);
					setfile_fp = NULL;
				} else {
					if (setfile_fp) {
						filp_close(setfile_fp, current->files);
					}
					setfile_fp = NULL;
				}

#ifdef CAMERA_MODULE_FRONT_SETF_DUMP
				memset(setfile_path, 0x0, sizeof(setfile_path));
				fimc_is_sec_get_front_setf_name(core);
				snprintf(setfile_path, sizeof(setfile_path), "%s%s",
					FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_front_setfile_name);
				setfile_fp = filp_open(setfile_path, O_RDONLY, 0);
				if (IS_ERR_OR_NULL(setfile_fp)) {
					crc32_front_setfile_check = false;
					info("setfile does not exist. Retry front setfile dump.\n");

					fimc_is_sec_read_front_setfile(core);
					setfile_fp = NULL;
				} else {
					if (setfile_fp) {
						filp_close(setfile_fp, current->files);
					}
					setfile_fp = NULL;
				}
#endif
				set_fs(old_fs);
			}
		}
#endif

		if (is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, sysfs_finfo.header_ver, FIMC_IS_HEADER_VER_SIZE);
		} else if (!is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, dump_fw_version, FIMC_IS_HEADER_VER_SIZE);
		} else {
			strncpy(loaded_fw, phone_fw_version, FIMC_IS_HEADER_VER_SIZE);
		}

	} else {
		info("already loaded the firmware, Phone version=%s, F-ROM version=%s\n",
			sysfs_pinfo.header_ver, sysfs_finfo.header_ver);
	}

exit:
#ifdef CONFIG_COMPANION_USE
	/* Set spi pin to out */
	if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1)
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE_INPU);
	else if (specific->rear_sensor_id == SENSOR_NAME_IMX260)
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE_INPD);
	else
		fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE);
#endif
	if (is_ldo_enabled && !specific->running_rear_camera)
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);

	mutex_unlock(&specific->spi_lock);

	return ret;
}
#endif

#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_concord_fw_sel(struct fimc_is_core *core, struct device *dev)
{
	int ret = 0;
	char c1_fw_path[100];
	char dump_c1_fw_path[100];
	char dump_c1_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	char phone_c1_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
#ifdef CAMERA_MODULE_DUALIZE
	int from_c1_fw_revision = 0;
	int dump_c1_fw_revision = 0;
	int phone_c1_fw_revision = 0;
#endif
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = false;
	int sensor_id = 0;
	bool is_ldo_enabled = false;
	struct fimc_is_spi *spi = &core->spi0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	mutex_lock(&specific->spi_lock);
	if ((!sysfs_finfo.is_c1_caldata_read &&
	    (cam_id == CAMERA_SINGLE_REAR /* || cam_id == CAMERA_DUAL_FRONT*/)) ||
	    force_caldata_dump) {
		is_dumped_c1_fw_loading_needed = false;
		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		sysfs_finfo.is_c1_caldata_read = true;

		if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_F) ||
		    fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_I) ||
		    fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_A) ||
		    fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_B) ||
		    fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_C)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_EVT1);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX228)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX228_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX228_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX228_EVT1);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX228_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX228_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX228_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX228_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX240_A) ||
		           fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX240_B) ||
		           fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX240_C)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX260_C)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX260);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX260), "%s", FIMC_IS_FW_COMPANION_IMX260);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX260_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX260_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX260_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX260_MODE_SETF);
		}  else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX260_D)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX260_D);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX260_D), "%s", FIMC_IS_FW_COMPANION_IMX260_D);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX260_MASTER_SETF_D), "%s", FIMC_IS_COMPANION_IMX260_MASTER_SETF_D);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX260_MODE_SETF_D), "%s", FIMC_IS_COMPANION_IMX260_MODE_SETF_D);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_12M)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_12M_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2T2)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2T2_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2T2_EVT1), "%s", FIMC_IS_FW_COMPANION_2T2_EVT1);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2T2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2T2_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2T2_MODE_SETF), "%s", FIMC_IS_COMPANION_2T2_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2L1_C)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2L1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2L1), "%s", FIMC_IS_FW_COMPANION_2L1);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2L1_MASTER_SETF), "%s", FIMC_IS_COMPANION_2L1_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2L1_MODE_SETF), "%s", FIMC_IS_COMPANION_2L1_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2L1_D)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2L1_D);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2L1_D), "%s", FIMC_IS_FW_COMPANION_2L1_D);
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2L1_MASTER_SETF_D), "%s", FIMC_IS_COMPANION_2L1_MASTER_SETF_D);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2L1_MODE_SETF_D), "%s", FIMC_IS_COMPANION_2L1_MODE_SETF_D);
		} else {
			err("Companion FW selection failed! Default FW will be used");
			sensor_id = specific->rear_sensor_id;
			if (sensor_id == SENSOR_NAME_IMX240) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_IMX228) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX228_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX228_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX228_EVT1);
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX228_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX228_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX228_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX228_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2P2_12M) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_12M_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2P2) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_EVT1);
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2T2) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2T2_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2T2_EVT1), "%s", FIMC_IS_FW_COMPANION_2T2_EVT1);
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2T2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2T2_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2T2_MODE_SETF), "%s", FIMC_IS_COMPANION_2T2_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_IMX260) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX260);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX260), "%s", FIMC_IS_FW_COMPANION_IMX260);
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX260_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX260_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX260_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX260_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2L1) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2L1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2L1), "%s", FIMC_IS_FW_COMPANION_2L1);
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2L1_MASTER_SETF), "%s", FIMC_IS_COMPANION_2L1_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2L1_MODE_SETF), "%s", FIMC_IS_COMPANION_2L1_MODE_SETF);
			} else {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
			}
		}

		snprintf(dump_c1_fw_path, sizeof(dump_c1_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_fw_name);
		info("Camera: f-rom fw version: %s\n", sysfs_finfo.concord_header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = 0;
		fp = filp_open(dump_c1_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			info("Camera: There is no dumped Companion firmware(%s)\n", dump_c1_fw_path);
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			dump_c1_fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Dumped Companion FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_c1_fw_version, temp_buf + nread - 16, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: dumped companion fw version: %s\n", dump_c1_fw_version);

read_phone_fw:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);
		if (ret < 0)
			goto exit;

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(c1_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			err("Camera: Failed open phone companion firmware(%s)", c1_fw_path);
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			c1_fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_BUFFER_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_BUFFER_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Phone Companion FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw_exit;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read companion firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_c1_fw_version, temp_buf + nread - 16, FIMC_IS_HEADER_VER_SIZE);
		strncpy(sysfs_pinfo.concord_header_ver, temp_buf + nread - 16, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: phone companion fw version: %s\n", phone_c1_fw_version);

read_phone_fw_exit:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

#if defined(CAMERA_MODULE_DUALIZE) && defined(CAMERA_MODULE_AVAILABLE_DUMP_VERSION)
		if (!strncmp(CAMERA_MODULE_AVAILABLE_DUMP_VERSION, sysfs_finfo.header_ver, 5)) {
			from_c1_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.concord_header_ver);
			phone_c1_fw_revision = fimc_is_sec_fw_revision(phone_c1_fw_version);
			if (is_dump_existed) {
				dump_c1_fw_revision = fimc_is_sec_fw_revision(dump_c1_fw_version);
			}

			info("from_c1_fw_revision = %d, phone_c1_fw_revision = %d, dump_c1_fw_revision = %d\n",
				from_c1_fw_revision, phone_c1_fw_revision, dump_c1_fw_revision);

			if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, phone_c1_fw_version)) ||
					(from_c1_fw_revision > phone_c1_fw_revision)) {
				is_dumped_c1_fw_loading_needed = true;
				if (is_dump_existed) {
					if (!fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver,
								dump_c1_fw_version)) {
						is_dump_needed = true;
					} else if (from_c1_fw_revision > dump_c1_fw_revision) {
						is_dump_needed = true;
					} else {
						is_dump_needed = false;
					}
				} else {
					is_dump_needed = true;
				}
			} else {
				is_dump_needed = false;
				if (is_dump_existed) {
					if (!fimc_is_sec_fw_module_compare(phone_c1_fw_version,
								dump_c1_fw_version)) {
						is_dumped_c1_fw_loading_needed = false;
					} else if (phone_c1_fw_revision > dump_c1_fw_revision) {
						is_dumped_c1_fw_loading_needed = false;
					} else {
						is_dumped_c1_fw_loading_needed = true;
					}
				} else {
					is_dumped_c1_fw_loading_needed = false;
				}
			}

			if (is_dump_needed) {
				info("Dump companion Firmware.\n");
				crc32_c1_fw_check = false;
				if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
					fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);
					is_ldo_enabled = true;
				}

				/* Set SPI function */
				fimc_is_spi_s_pin(spi, SPI_PIN_STATE_HOST);

				ret = fimc_is_sec_read_companion_fw(core);
				if (ret < 0) {
					if (!crc32_c1_fw_check) {
						is_dumped_c1_fw_loading_needed = false;
						err("Companion Firmware CRC is not valid. Does not use dumped firmware.\n");
					}
				}

				/* Set spi pin to out */
				if (specific->rear_sensor_id == SENSOR_NAME_S5K2L1)
					fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE_INPU);
				else if (specific->rear_sensor_id == SENSOR_NAME_IMX260)
					fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE_INPD);
				else
					fimc_is_spi_s_pin(spi, SPI_PIN_STATE_IDLE);

				if (is_ldo_enabled && !specific->running_rear_camera)
					fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);
			}

			if (phone_c1_fw_version[0] == 0) {
				strcpy(sysfs_pinfo.concord_header_ver, "NULL");
			}
		}
#endif

		if (is_dump_needed && is_dumped_c1_fw_loading_needed) {
			strncpy(loaded_companion_fw, sysfs_finfo.concord_header_ver, FIMC_IS_HEADER_VER_SIZE);
		} else if (!is_dump_needed && is_dumped_c1_fw_loading_needed) {
			strncpy(loaded_companion_fw, dump_c1_fw_version, FIMC_IS_HEADER_VER_SIZE);
		} else {
			strncpy(loaded_companion_fw, phone_c1_fw_version, FIMC_IS_HEADER_VER_SIZE);
		}
	} else {
		info("already loaded the firmware, Phone_Comp version=%s, F-ROM_Comp version=%s\n",
			sysfs_pinfo.concord_header_ver, sysfs_finfo.concord_header_ver);
	}

exit:
	mutex_unlock(&specific->spi_lock);

	return ret;
}
#endif
