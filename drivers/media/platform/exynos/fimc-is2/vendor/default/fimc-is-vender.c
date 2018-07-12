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

#include <linux/slab.h>

#include "fimc-is-vender.h"
#include "fimc-is-vender-specific.h"
#include "fimc-is-core.h"

int fimc_is_vender_probe(struct fimc_is_vender *vender)
{
	int ret = 0;

	BUG_ON(!vender);

	snprintf(vender->fw_path, sizeof(vender->fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, FIMC_IS_FW);
	snprintf(vender->request_fw_path, sizeof(vender->request_fw_path), "%s", FIMC_IS_FW);

	vender->private_data = NULL;

	return ret;
}

int fimc_is_vender_dt(struct device_node *np)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_fw_prepare(struct fimc_is_vender *vender)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_fw_filp_open(struct fimc_is_vender *vender, struct file **fp, int bin_type)
{
	return FW_SKIP;
}

int fimc_is_vender_preproc_fw_load(struct fimc_is_vender *vender)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_cal_load(struct fimc_is_vender *vender,
	void *module_data)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_module_sel(struct fimc_is_vender *vender, void *module_data)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_module_del(struct fimc_is_vender *vender, void *module_data)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_fw_sel(struct fimc_is_vender *vender)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_setfile_sel(struct fimc_is_vender *vender, char *setfile_name)
{
	int ret = 0;

	BUG_ON(!vender);
	BUG_ON(!setfile_name);

	snprintf(vender->setfile_path, sizeof(vender->setfile_path), "%s%s",
		FIMC_IS_SETFILE_SDCARD_PATH, setfile_name);
	snprintf(vender->request_setfile_path, sizeof(vender->request_setfile_path), "%s",
		setfile_name);

	return ret;
}

int fimc_is_vender_gpio_on_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scneario)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_gpio_on(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
	return ret;
}

int fimc_is_vender_gpio_off_sel(struct fimc_is_vender *vender, u32 scenario, u32 *gpio_scneario)
{
	int ret = 0;

	return ret;
}

int fimc_is_vender_gpio_off(struct fimc_is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;

	return ret;
}

void fimc_is_vender_itf_open(struct fimc_is_vender *vender, struct sensor_open_extended *ext_info)
{
	return;
}

int fimc_is_vender_set_torch(u32 aeflashMode)
{
	return 0;
}

int fimc_is_vender_video_s_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}
