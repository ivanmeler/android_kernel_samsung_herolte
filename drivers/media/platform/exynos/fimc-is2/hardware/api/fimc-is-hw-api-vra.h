/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_API_VRA_H
#define FIMC_IS_HW_API_VRA_H

#include "fimc-is-hw-api-common.h"
#include "../../interface/fimc-is-interface-ischain.h"

enum hw_api_vra_format_mode {
	VRA_FORMAT_PREVIEW_MODE, /* OTF mode */
	VRA_FORMAT_PLAYBACK_MODE, /* M2M mode */
	VRA_FORMAT_MODE_END
};

struct hw_api_vra_setfile_half {
	u32 setfile_version;
	u32 skip_frames;
	u32 flag;
	u32 min_face;
	u32 max_face;
	u32 central_lock_area_percent_w;
	u32 central_lock_area_percent_h;
	u32 central_max_face_num_limit;
	u32 frame_per_lock;
	u32 smooth;
	s32 boost_fd_vs_fd;
	u32 max_face_cnt;
	s32 boost_fd_vs_speed;
	u32 min_face_size_feature_detect;
	u32 max_face_size_feature_detect;
	s32 keep_faces_over_frame;
	s32 keep_limit_no_face;
	u32 frame_per_lock_no_face;
	s32 lock_angle[16];
};

struct hw_api_vra_setfile {
	struct hw_api_vra_setfile_half preveiw;
	struct hw_api_vra_setfile_half playback;
};

u32 fimc_is_vra_chain0_get_all_intr(void __iomem *base_addr);
void fimc_is_vra_chain0_set_clear_intr(void __iomem *base_addr, u32 value);
u32 fimc_is_vra_chain0_get_status_intr(void __iomem *base_addr);
u32 fimc_is_vra_chain0_get_enable_intr(void __iomem *base_addr);

u32 fimc_is_vra_chain1_get_all_intr(void __iomem *base_addr);
u32 fimc_is_vra_chain1_get_status_intr(void __iomem *base_addr);
u32 fimc_is_vra_chain1_get_enable_intr(void __iomem *base_addr);
void fimc_is_vra_chain1_set_clear_intr(void __iomem *base_addr, u32 value);
#endif
