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

#ifndef FIMC_IS_VENDER_H
#define FIMC_IS_VENDER_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include "exynos-fimc-is-sensor.h"

#define FIMC_IS_PATH_LEN 100
#define VENDER_S_CTRL 0

struct fimc_is_vender {
	char fw_path[FIMC_IS_PATH_LEN];
	char request_fw_path[FIMC_IS_PATH_LEN];
	char setfile_path[FIMC_IS_PATH_LEN];
	char request_setfile_path[FIMC_IS_PATH_LEN];
	void *private_data;
};

enum {
	FW_SKIP,
	FW_SUCCESS,
	FW_FAIL,
};

#ifdef USE_CAMERA_HW_BIG_DATA
struct cam_hw_param {
	u32 i2c_sensor_err_cnt;
	u32 i2c_comp_err_cnt;
	u32 i2c_ois_err_cnt;
	u32 i2c_af_err_cnt;
	u32 mipi_sensor_err_cnt;
	u32 mipi_comp_err_cnt;
} __attribute__((__packed__));

struct cam_hw_param_collector {
	struct cam_hw_param rear_hwparam;
	struct cam_hw_param front_hwparam;
	struct cam_hw_param iris_hwparam;
}__attribute__((__packed__));

void fimc_is_sec_init_err_cnt_file(struct cam_hw_param *hw_param);
int fimc_is_sec_get_rear_hw_param(struct cam_hw_param **hw_param);
int fimc_is_sec_get_front_hw_param(struct cam_hw_param **hw_param);
int fimc_is_sec_get_iris_hw_param(struct cam_hw_param **hw_param);
bool fimc_is_sec_is_valid_moduleid(char *moduleid);
#endif

int fimc_is_vender_probe(struct fimc_is_vender *vender);
int fimc_is_vender_dt(struct device_node *np);
int fimc_is_vender_fw_prepare(struct fimc_is_vender *vender);
int fimc_is_vender_fw_filp_open(struct fimc_is_vender *vender, struct file **fp, int bin_type);
int fimc_is_vender_preproc_fw_load(struct fimc_is_vender *vender);
int fimc_is_vender_cal_load(struct fimc_is_vender *vender, void *module_data);
int fimc_is_vender_module_sel(struct fimc_is_vender *vender, void *module_data);
int fimc_is_vender_module_del(struct fimc_is_vender *vender, void *module_data);
int fimc_is_vender_fw_sel(struct fimc_is_vender *vender);
int fimc_is_vender_setfile_sel(struct fimc_is_vender *vender, char *setfile_name);
int fimc_is_vender_preprocessor_gpio_on_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario);
int fimc_is_vender_preprocessor_gpio_on(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario);
int fimc_is_vender_sensor_gpio_on_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario);
int fimc_is_vender_sensor_gpio_on(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario);
int fimc_is_vender_preprocessor_gpio_off_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario,
			void *module_data);
int fimc_is_vender_preprocessor_gpio_off(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario);
int fimc_is_vender_sensor_gpio_off_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scenario);
int fimc_is_vender_sensor_gpio_off(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario);
void fimc_is_vender_itf_open(struct fimc_is_vender *vender, struct sensor_open_extended *ext_info);
int fimc_is_vender_set_torch(u32 aeflashMode);
int fimc_is_vender_video_s_ctrl(struct v4l2_control *ctrl, void *device_data);
int fimc_is_vender_hw_init(struct fimc_is_vender *vender);
void fimc_is_vender_check_hw_init_running(void);
#endif
