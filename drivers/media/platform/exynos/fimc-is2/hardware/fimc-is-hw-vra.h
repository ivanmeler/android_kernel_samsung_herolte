/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_VRA_H
#define FIMC_IS_HW_VRA_H

#include "fimc-is-hw-api-vra.h"
#include "fimc-is-interface-vra.h"

#define VRA_SETFILE_VERSION	0x11030205

struct fimc_is_hw_vra_setfile{
    u32 setfile_version;
    u32 tracking_mode;
    u32 enable_features;
    u32 init_frames_per_lock;
    u32 normal_frames_per_lock;
    u32 min_face_size;
    u32 max_face_count;
    u32 face_priority;
    u32 disable_profile_detection;
    u32 limit_rotation_angles;
    u32 boost_dr_vs_fpr;
    u32 tracking_smoothness;
    u32 lock_frame_number;
    u32 front_orientation;
    u32 use_sensor_orientation;    // Not used
};

struct fimc_is_hw_vra {
	struct fimc_is_lib_vra lib_vra;
	struct fimc_is_hw_vra_setfile setfile;
};

int fimc_is_hw_vra_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc, int id);
int fimc_is_hw_vra_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size);
int fimc_is_hw_vra_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
	bool flag, u32 module_id);
int fimc_is_hw_vra_close(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_vra_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map);
int fimc_is_hw_vra_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map);
int fimc_is_hw_vra_shot(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	ulong hw_map);
int fimc_is_hw_vra_set_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map);
int fimc_is_hw_vra_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	u32 instance, bool late_flag);
void fimc_is_hw_vra_reset(struct fimc_is_hw_ip *hw_ip);
int fimc_is_hw_vra_load_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map);
int fimc_is_hw_vra_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map);
int fimc_is_hw_vra_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance,
	ulong hw_map);
int fimc_is_hw_vra_get_meta(struct fimc_is_hw_ip *hw_ip,
		struct fimc_is_frame *frame, unsigned long hw_map);

#endif
