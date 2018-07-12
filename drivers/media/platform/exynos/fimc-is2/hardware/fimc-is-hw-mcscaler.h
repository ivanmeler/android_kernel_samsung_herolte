/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SUBDEV_MCSC_H
#define FIMC_IS_SUBDEV_MCSC_H

#include "fimc-is-hw-control.h"
#include "fimc-is-interface-library.h"
#include "fimc-is-param.h"

#define MCSC_INTR_MASK		(0x00000070)
#define MAX_OTF_WIDTH		(3856)
#define USE_DMA_BUFFER_INDEX	(0) /* 0 ~ 7 */

enum mcsc_img_format {
	MCSC_YUV422_1P_YUYV = 0,
	MCSC_YUV422_1P_YVYU,
	MCSC_YUV422_1P_UYVY,
	MCSC_YUV422_1P_VYUY,
	MCSC_YUV422_2P_UFIRST,
	MCSC_YUV422_2P_VFIRST,
	MCSC_YUV422_3P,
	MCSC_YUV420_2P_UFIRST,
	MCSC_YUV420_2P_VFIRST,
	MCSC_YUV420_3P
};

enum mcsc_param_change_flag {
	MCSC_PARAM_OTFINPUT_CHANGE,
	MCSC_PARAM_OTFOUTPUT_CHANGE,
	MCSC_PARAM_DMAOUTPUT_CHANGE,
	MCSC_PARAM_INPUTCROP_CHANGE,
	MCSC_PARAM_OUTPUTCROP_CHANGE,
	MCSC_PARAM_FLIP_CHANGE,
	MCSC_PARAM_IMAGEEFFECT_CHANGE,
	MCSC_PARAM_YUVRANGE_CHANGE,
	MCSC_PARAM_CHANGE_MAX,
};

struct fimc_is_hw_mcsc {
	u32	img_format;
	bool	conv420_en;
	bool	param_change_flags[MCSC_PARAM_CHANGE_MAX];
	struct hw_api_scaler_setfile *setfile;
};

int fimc_is_hw_mcsc_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc, int id);
int fimc_is_hw_mcsc_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size);
int fimc_is_hw_mcsc_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
	bool flag, u32 module_id);
int fimc_is_hw_mcsc_close(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map);
int fimc_is_hw_mcsc_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map);
int fimc_is_hw_mcsc_shot(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	ulong hw_map);
int fimc_is_hw_mcsc_set_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map);
void fimc_is_hw_mcsc_update_param(struct scp_param *param,
	struct fimc_is_hw_mcsc *hw_scp);
int fimc_is_hw_mcsc_update_register(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	u32 instance, bool late_flag);
int fimc_is_hw_mcsc_reset(struct fimc_is_hw_ip *hw_ip);
int fimc_is_hw_mcsc_load_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map);
int fimc_is_hw_mcsc_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map);
int fimc_is_hw_mcsc_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance,
	ulong hw_map);
int fimc_is_hw_mcsc_otf_input(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_effect(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_input_crop(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_output_crop(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_flip(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_otf_output(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_dma_output(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_mcsc_output_yuvrange(struct fimc_is_hw_ip *hw_ip, u32 instance);

int fimc_is_hw_mcsc_adjust_img_fmt(u32 format, u32 plane, u32 order, u32 *img_format,
	bool *conv420_flag);
void fimc_is_hw_mcsc_adjust_stride(u32 width, u32 plane, bool conv420_flag,
	u32 *y_stride, u32 *uv_stride);
void fimc_is_hw_mcsc_size_dump(struct fimc_is_hw_ip *hw_ip);
#endif
