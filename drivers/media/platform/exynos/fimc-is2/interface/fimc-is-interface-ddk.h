/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_INTERFACE_LIBRARY_H
#define FIMC_IS_INTERFACE_LIBRARY_H

#include "fimc-is-core.h"
#include "fimc-is-interface-library.h"
#include "fimc-is-param.h"

#define CHAIN_ID_MASK		(0x0000000F)
#define CHAIN_ID_SHIFT		(0)
#define INSTANCE_ID_MASK	(0x000000F0)
#define INSTANCE_ID_SHIFT	(4)
#define REPROCESSING_FLAG_MASK	(0x00000F00)
#define REPROCESSING_FLAG_SHIFT	(8)
#define INPUT_TYPE_MASK		(0x0000F000)
#define INPUT_TYPE_SHIFT	(12)
#define MODULE_ID_MASK		(0x0FFF0000)
#define MODULE_ID_SHIFT		(16)

#define CONVRES(src, src_max, tar_max) \
	((src <= 0) ? (0) : ((src * tar_max + (src_max >> 1)) / src_max))

struct fimc_is_lib_isp {
	void				*object;
	struct lib_interface_func	*func;
};

enum lib_cb_event_type {
	LIB_EVENT_CONFIG_LOCK		= 1,
	LIB_EVENT_FRAME_START		= 2,
	LIB_EVENT_FRAME_END		= 3,
	LIB_EVENT_DMA_A_OUT_DONE	= 4,
	LIB_EVENT_DMA_B_OUT_DONE	= 5,
	LIB_EVENT_FRAME_START_ISR	= 6,
	LIB_EVENT_END
};

#ifdef SOC_DRC
struct grid_rectangle {
	u32 width;
	u32 height;
};

struct param_grid_dma_input {
	u32			drc_buffer0_addr;
	u32			drc_buffer1_addr;
	struct grid_rectangle	drc_grid0_size;
	struct grid_rectangle	drc_grid1_size;
	bool			drc_enable0;
	bool			drc_enable1;
};
#endif

struct taa_param_set {
	struct param_sensor_config	sensor_config;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_dma_input		ddma_input;	/* deprecated */
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output_before_bds;
	struct param_dma_output		dma_output_after_bds;
	struct param_dma_output		ddma_output;	/* deprecated */
#ifdef SOC_DRC
	bool				drc_en;
	u32				drc_grid0_dvaddr;
	u32				drc_grid1_dvaddr;
#endif
	u32				input_dva;
	u32				output_dva_before_bds;
	u32				output_dva_after_bds;
	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
};

struct isp_param_set {
	struct taa_param_set		*taa_param;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_dma_input		vdma3_input;	/* deprecated */
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output_chunk;
	struct param_dma_output		dma_output_yuv;
#ifdef SOC_DRC
	struct param_grid_dma_input	*drc_grid_input;
#endif
	u32				input_dva;
	u32				output_dva_chunk;
	u32				output_dva_yuv;
	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
};

struct lib_callback_func {
	void (*camera_callback)(void *hw_ip, enum lib_cb_event_type event_id,
		u32 instance_id, void *data);
	void (*io_callback)(void *hw_ip, enum lib_cb_event_type event_id,
		u32 instance_id);
};

struct lib_tune_set {
	u32 index;
	ulong addr;
	u32 size;
	int decrypt_flag;
};

struct cal_info {
	u32 data[16];
};

#define LIB_ISP_ADDR		(LIB_ISP_BASE_ADDR + LIB_ISP_OFFSET)
enum lib_func_type {
	LIB_FUNC_3AA = 1,
	LIB_FUNC_ISP,
	LIB_FUNC_TPU,
	LIB_FUNC_VRA,
	LIB_FUNC_TYPE_END
};

typedef u32 (*register_lib_isp)(u32 type, void **lib_func);
static const register_lib_isp get_lib_func = (register_lib_isp)LIB_ISP_ADDR;

struct lib_interface_func {
	int (*chain_create)(u32 chain_id, ulong addr, u32 offset,
		const struct lib_callback_func *cb);
	int (*object_create)(void **pobject, u32 obj_info, void *hw);
	int (*chain_destroy)(u32 chain_id);
	int (*object_destroy)(void *object, u32 sensor_id);
	int (*stop)(void *object, u32 instance_id);
	int (*reset)(u32 chain_id);
	int (*set_param)(void *object, void *param_set);
	int (*set_ctrl)(void *object, u32 instance, u32 frame_number,
		struct camera2_shot *shot);
	int (*shot)(void *object, void *param_set, struct camera2_shot *shot);
	int (*get_meta)(void *object, u32 instance, u32 frame_number,
		struct camera2_shot *shot);
	int (*create_tune_set)(void *isp_object, u32 instance, struct lib_tune_set *set);
	int (*apply_tune_set)(void *isp_object, u32 instance_id, u32 index);
	int (*delete_tune_set)(void *isp_object, u32 instance_id, u32 index);
	int (*set_parameter_flash)(void *object, struct param_isp_flash *p_flash);
	int (*set_parameter_af)(void *isp_object, struct param_isp_aa *param_aa,
		u32 frame_number, u32 instance);
	int (*load_cal_data)(void *isp_object, u32 instance_id, ulong kvaddr);
	int (*get_cal_data)(void *isp_object, u32 instance_id,
		struct cal_info *data, int type);
	int (*sensor_info_mode_chg)(void *isp_object, u32 instance_id,
		struct camera2_shot *shot);
	int (*sensor_update_ctl)(void *isp_object, u32 instance_id,
		u32 frame_count, struct camera2_shot *shot);
};

int fimc_is_lib_isp_chain_create(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id);
int fimc_is_lib_isp_object_create(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id, u32 rep_flag, u32 module_id);
void fimc_is_lib_isp_chain_destroy(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id);
void fimc_is_lib_isp_object_destroy(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id);
int fimc_is_lib_isp_s_param(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, void *param);
int fimc_is_lib_isp_set_ctrl(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, struct fimc_is_frame *frame);
void fimc_is_lib_isp_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, void *param_set, struct camera2_shot *shot);
int fimc_is_lib_isp_get_meta(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, struct fimc_is_frame *frame);
void fimc_is_lib_isp_stop(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id);
int fimc_is_lib_isp_create_tune_set(struct fimc_is_lib_isp *this,
	ulong addr, u32 size, u32 index, int flag, u32 instance_id);
int fimc_is_lib_isp_apply_tune_set(struct fimc_is_lib_isp *this,
	u32 index, u32 instance_id);
int fimc_is_lib_isp_delete_tune_set(struct fimc_is_lib_isp *this,
	u32 index, u32 instance_id);
int fimc_is_lib_isp_load_cal_data(struct fimc_is_lib_isp *this,
	u32 index, ulong addr);
int fimc_is_lib_isp_get_cal_data(struct fimc_is_lib_isp *this,
	u32 instance_id, struct cal_info *data, int type);
int fimc_is_lib_isp_sensor_info_mode_chg(struct fimc_is_lib_isp *this,
	u32 instance_id, struct camera2_shot *shot);
int fimc_is_lib_isp_sensor_update_control(struct fimc_is_lib_isp *this,
	u32 instance_id, u32 frame_count, struct camera2_shot *shot);
int fimc_is_lib_isp_convert_face_map(struct fimc_is_hardware *hardware,
	struct taa_param_set *param_set, struct fimc_is_frame *frame);
void fimc_is_lib_isp_configure_algorithm(void);
void fimc_is_isp_get_bcrop1_size(void __iomem *base_addr, u32 *width, u32 *height);
#endif
