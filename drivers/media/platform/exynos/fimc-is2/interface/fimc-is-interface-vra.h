/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_API_VRA_H
#define FIMC_IS_API_VRA_H

#include "fimc-is-interface-library.h"
#include "fimc-is-lib-vra.h"
#include "fimc-is-metadata.h"
#include "fimc-is-param.h"

#define VRA_TOTAL_SENSORS		FIMC_IS_MAX_INSTANCES
/* #define VRA_DMA_TEST_BY_IMAGE */
#define VRA_DMA_TEST_IMAGE_PATH		"/data/2.yuv"

typedef int (*vra_load_binary_funcs_t)(void *func);

typedef u32 (*vra_load_itf_funcs_t)(void *func);
static const vra_load_itf_funcs_t get_lib_vra_func = (vra_load_itf_funcs_t)LIB_VRA_API_ADDR;

enum fimc_is_lib_vra_state_list {
	VRA_LIB_INIT,
};

enum fimc_is_lib_vra_input_type {
	VRA_INPUT_OTF,
	VRA_INPUT_MEMORY
};

enum fimc_is_lib_vra_dir {
	VRA_REAR_ORIENTATION,
	VRA_FRONT_ORIENTATION
};

struct fimc_is_lib_vra_os_system_funcs {
	void (*control_task_set_event)(u32 event_type);
	void (*fw_algs_task_set_event)(u32 event_type);
	int (*set_dram_adr_from_core_to_vdma)(ulong src_adr, u32 *target_adr);
	void (*clean_cache_region)(ulong start_addr, u32 size);
	void (*invalidate_cache_region)(ulong start_addr, u32 size);
	void (*data_write_back_cache_region)(ulong start_adr, u32 size);
	int (*log_write_console)(char *str);
	int (*log_write)(const char *str, ...);
};

struct fimc_is_lib_vra_interface_funcs {
	enum api_vra_type (*ex_get_memory_sizes)(const struct api_vra_alloc_info *alloc_ptr,
			vra_uint32 *fr_work_ram_size,
			vra_uint32 *sensor_ram_size,
			vra_uint32 *dma_ram_size);
	enum api_vra_type (*vra_frame_work_init)(void *fr_obj_ptr,
			vra_uint32 fr_obj_size_in_bytes,
			const struct api_vra_alloc_info *alloc_ptr,
			const struct api_vra_fr_work_init *init_info_ptr,
			const struct api_vra_dma_ram *dma_ram_info_ptr,
			unsigned int api_version);
	enum api_vra_type (*vra_sensor_init)(void *sen_obj_ptr,
			vra_uint32 sen_ram_size_in_bytes,
			vra_uint32 callbacks_handle,
			const struct api_vra_input_desc *def_input_ptr,
			enum api_vra_track_mode def_track_mode);
	unsigned int (*set_parameter)(void *fr_obj_ptr, void *sen_obj_ptr,
			const struct api_vra_tune_data *tune_data);
	enum api_vra_type (*set_orientation)(void *sen_obj_ptr,
			enum api_vra_orientation orientation);
	enum api_vra_type (*set_input)(void *sen_obj_ptr,
			struct api_vra_input_desc *input_ptr,
			enum api_vra_update_tr_data_base update_tr_data_base);
	unsigned int (*get_parameter)(void *fr_obj_ptr,
			void *sen_obj_ptr,
			struct api_vra_tune_data *tune_data,
			enum api_vra_orientation *orientation);
	enum api_vra_type (*on_new_frame)(void *sen_obj_ptr,
			vra_uint32 unique_index,
			vra_uint32 diff_prev_fr_100_micro,
			unsigned char *base_adrress);
	enum api_vra_type (*on_control_task_event)(void *fr_obj_ptr);
	enum api_vra_type (*on_fw_algs_task_event)(void *fr_obj_ptr);
	enum api_vra_type (*on_interrupt)(void *fr_obj_ptr, unsigned int index);
	enum api_vra_type (*frame_work_abort)(void *fr_obj_ptr,
			vra_boolean reset_tr_data_base);
	enum api_vra_type (*frame_work_terminate)(void *fr_obj_ptr);
};

struct fimc_is_lib_vra_debug {
	u32					lost_frame_cnt[PR_ALL];
	u32					err_cnt;
	enum api_vra_sen_err			last_err_type;
	u32					last_err_info;
};

struct fimc_is_lib_vra_frame_lock {
	u32					lock_frame_num;
	u32					init_frames_per_lock; /* DMA */
	u32					normal_frames_per_lock; /* OTF */
};

struct fimc_is_lib_vra_tune_data {
	struct api_vra_tune_data		api_tune;
	enum fimc_is_lib_vra_dir		dir;
	struct fimc_is_lib_vra_frame_lock	frame_lock;
};

struct fimc_is_lib_vra_state {
	ulong					frame_work;
	ulong					frame_desc[VRA_TOTAL_SENSORS];
};

struct fimc_is_lib_vra {
	struct fimc_is_lib_vra_state		state;

	u32					fr_index;
	struct api_vra_alloc_info		alloc_info;

	/* VRA frame work */
	struct api_vra_fr_work_init		fr_work_init;
	void					*fr_work_heap;
	u32					fr_work_size;

	/* VRA frame descript */
	struct api_vra_input_desc		frame_desc[VRA_TOTAL_SENSORS];
	void					*frame_desc_heap[VRA_TOTAL_SENSORS];
	u32					frame_desc_size;

	/* VRA out dma */
	struct api_vra_dma_ram			dma_out;
	void					*dma_out_heap;
	u32					dma_out_size;

	/* VRA tune data */
	struct fimc_is_lib_vra_tune_data	tune[VRA_TOTAL_SENSORS];

	/* VRA Task */
	struct fimc_is_lib_task			task_vra;
	enum api_vra_ctrl_task_set_event	ctl_task_type;
	enum api_vra_fw_algs_task_set_event	algs_task_type;
	spinlock_t				ctl_lock;
	ulong					ctl_irq_flag;
	spinlock_t				algs_lock;
	ulong					algs_irq_flag;

	/* VRA interrupt lock */
	spinlock_t				intr_lock;

	/* VRA library functions */
	struct fimc_is_lib_vra_interface_funcs	itf_func;

	/* VRA output data */
	u32					all_face_num;
	const struct api_vra_out_list_info	*out_list_info;
	struct api_vra_out_face			out_faces[MAX_FACE_COUNT];

	/* VRA debug data */
	struct fimc_is_lib_vra_debug		debug;

#ifdef VRA_DMA_TEST_BY_IMAGE
	void					*test_input_buffer;
	bool					image_load;
#endif
};

void fimc_is_lib_vra_os_funcs(struct fimc_is_lib_vra_os_system_funcs *funcs);
int fimc_is_lib_vra_get_interface_funcs(struct fimc_is_lib_vra *lib_vra);
int fimc_is_lib_vra_init_task(struct fimc_is_lib_vra *lib_vra);
int fimc_is_lib_vra_alloc_memory(struct fimc_is_lib_vra *lib_vra,
	ulong dma_address);
int fimc_is_lib_vra_free_memory(struct fimc_is_lib_vra *lib_vra);
int fimc_is_lib_vra_create_object(struct fimc_is_lib_vra *lib_vra,
	void __iomem *base_addr, enum fimc_is_lib_vra_input_type input_type,
	u32 instance);
int fimc_is_lib_vra_stop(struct fimc_is_lib_vra *lib_vra);
int fimc_is_lib_vra_destory_object(struct fimc_is_lib_vra *lib_vra, u32 instance);
int fimc_is_lib_vra_config_param(struct fimc_is_lib_vra *lib_vra,
	struct vra_param *param, u32 instance);
int fimc_is_lib_vra_set_param(struct fimc_is_lib_vra *lib_vra,
	struct fimc_is_lib_vra_tune_data *vra_tune,
	enum fimc_is_lib_vra_dir dir, u32 instance);
int fimc_is_lib_vra_set_orientation(struct fimc_is_lib_vra *lib_vra,
	u32 orientation, u32 instance);
int fimc_is_lib_vra_new_frame(struct fimc_is_lib_vra *lib_vra,
	unsigned char *buffer, u32 instance);
int fimc_is_lib_vra_handle_interrupt(struct fimc_is_lib_vra *lib_vra, u32 id);
int fimc_is_lib_vra_get_meta(struct fimc_is_lib_vra *lib_vra,
	struct camera2_shot *shot);
int fimc_is_lib_vra_test_image_load(struct fimc_is_lib_vra *lib_vra);
#endif
