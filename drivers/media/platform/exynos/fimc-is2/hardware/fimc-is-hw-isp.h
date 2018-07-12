/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_ISP_H
#define FIMC_IS_HW_ISP_H

#include "fimc-is-hw-control.h"
#include "fimc-is-interface-ddk.h"

struct fimc_is_hw_isp {
	struct fimc_is_lib_isp		lib[FIMC_IS_MAX_NODES];
	struct fimc_is_lib_support	*lib_support;
	struct lib_interface_func	*lib_func;
	struct isp_param_set		param_set[FIMC_IS_MAX_NODES];
};

int fimc_is_hw_isp_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc, int id);
int fimc_is_hw_isp_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size);
int fimc_is_hw_isp_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
	bool flag, u32 module_id);
int fimc_is_hw_isp_object_close(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_isp_close(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_isp_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map);
int fimc_is_hw_isp_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map);
int fimc_is_hw_isp_shot(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	ulong hw_map);
int fimc_is_hw_isp_set_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map);
void fimc_is_hw_isp_update_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	struct isp_param_set *param_set, u32 lindex, u32 hindex, u32 instance);
int fimc_is_hw_isp_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	u32 instance, bool late_flag);
int fimc_is_hw_isp_reset(struct fimc_is_hw_ip *hw_ip);
int fimc_is_hw_isp_load_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map);
int fimc_is_hw_isp_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map);
int fimc_is_hw_isp_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance,
	ulong hw_map);
#endif
