/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-interface-vra.h"
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static struct fimc_is_lib_vra *g_lib_vra;

extern bool vra_binary_load;

/* Callback functions for VRA library */
static void fimc_is_lib_vra_callback_frw_abort(void)
{
	dbg_lib("vra_callback_frw_abort\n");
}

static void fimc_is_lib_vra_callback_frw_hw_err(u32 ch_index, u32 err_mask)
{
	err_lib("callback_frw_hw_err: ch_index = %#x, err_mask = %#x",
		       ch_index, err_mask);
}

static void fimc_is_lib_vra_callback_output_ready(u32 handle,
		u32 num_all_faces, const struct api_vra_out_face *faces_ptr,
		const struct api_vra_out_list_info *out_list_info_ptr)
{
	int i;
	struct fimc_is_lib_vra *lib_vra;

	lib_vra = g_lib_vra;
	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return;
	}

	dbg_lib("callback_output_ready: num_all_faces(%d)\n", num_all_faces);

	lib_vra->out_list_info = out_list_info_ptr;
	lib_vra->all_face_num = (num_all_faces > MAX_FACE_COUNT) ?
				MAX_FACE_COUNT : num_all_faces;

	for (i = 0; i < lib_vra->all_face_num; i++) {
		lib_vra->out_faces[i] = faces_ptr[i];
		dbg_lib(">>>>>>>>>>>>>>[%d] pos(%d,%d) size(%d,%d) Score:%d\n",
				i, faces_ptr[i].base.rect.left,
				faces_ptr[i].base.rect.top,
				faces_ptr[i].base.rect.width,
				faces_ptr[i].base.rect.height,
				faces_ptr[i].base.score);
	}
}

static void fimc_is_lib_vra_callback_end_input(u32 handle,
		u32 frame_index, unsigned char *base_address)
{
	dbg_lib("callback_end_input: baseAdr: %#lx\n", (ulong)base_address);
}

static void fimc_is_lib_vra_callback_frame_error(u32 handle,
		enum api_vra_sen_err error_type, u32 additonal_info)
{
	struct fimc_is_lib_vra *lib_vra;

	lib_vra = g_lib_vra;

	if (VRA_ERR_FRAME_LOST) {
		lib_vra->debug.lost_frame_cnt[additonal_info]++;
	} else {
		lib_vra->debug.err_cnt++;
		lib_vra->debug.last_err_type = error_type;
		lib_vra->debug.last_err_info = additonal_info;
	}
}

void fimc_is_lib_vra_task_trigger(struct fimc_is_lib_vra *lib_vra,
	void *func)
{
	u32 work_index = 0;
	struct fimc_is_lib_task *task_vra;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return;
	}

	task_vra = &lib_vra->task_vra;

	spin_lock(&task_vra->work_lock);

	task_vra->work[task_vra->work_index % FIMC_IS_MAX_TASK].func = func;
	task_vra->work[task_vra->work_index % FIMC_IS_MAX_TASK].params = lib_vra;
	task_vra->work_index++;
	work_index = (task_vra->work_index - 1) % FIMC_IS_MAX_TASK;

	spin_unlock(&task_vra->work_lock);

	queue_kthread_work(&task_vra->worker, &task_vra->work[work_index].work);
}

int fimc_is_lib_vra_invoke_contol_event(struct fimc_is_lib_vra *lib_vra)
{
	enum api_vra_type status = VRA_NO_ERROR;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	dbg_lib("vra_invoke_contol_event: ctl_task_type (%d)\n", lib_vra->ctl_task_type);

	spin_lock_irqsave(&lib_vra->ctl_lock, lib_vra->ctl_irq_flag);
	status = lib_vra->itf_func.on_control_task_event(lib_vra->fr_work_heap);
	if (status) {
		err_lib("vra_invoke_contol_event: VRA control task is fail (%#x)",
			status);
		spin_unlock_irqrestore(&lib_vra->ctl_lock, lib_vra->ctl_irq_flag);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&lib_vra->ctl_lock, lib_vra->ctl_irq_flag);

	return 0;
}

int fimc_is_lib_vra_invoke_fwalgs_event(struct fimc_is_lib_vra *lib_vra)
{
	enum api_vra_type status = VRA_NO_ERROR;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	dbg_lib("vra_invoke_fwalgs_event: algs_task_type (%d)\n",
		lib_vra->algs_task_type);

	spin_lock_irqsave(&lib_vra->algs_lock, lib_vra->algs_irq_flag);
	status = lib_vra->itf_func.on_fw_algs_task_event(lib_vra->fr_work_heap);
	if (status) {
		err_lib("vra_invoke_fwalgs_event: VRA frame work task is fail (%#x)",
			status);
		spin_unlock_irqrestore(&lib_vra->algs_lock, lib_vra->algs_irq_flag);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&lib_vra->algs_lock, lib_vra->algs_irq_flag);

	return 0;
}

void fimc_is_lib_vra_task_work(struct kthread_work *work)
{
	struct fimc_is_task_work *cur_work;
	struct fimc_is_lib_vra *lib_vra;

	cur_work = container_of(work, struct fimc_is_task_work, work);
	lib_vra = (struct fimc_is_lib_vra *)cur_work->params;

	cur_work->func((void *)lib_vra);
}

int fimc_is_lib_vra_init_task(struct fimc_is_lib_vra *lib_vra)
{
	s32 ret = 0, cnt = 0;
	struct sched_param param = {.sched_priority = 0};

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	spin_lock_init(&lib_vra->task_vra.work_lock);
	init_kthread_worker(&lib_vra->task_vra.worker);

	lib_vra->task_vra.task = kthread_run(kthread_worker_fn,
		&lib_vra->task_vra.worker, "fimc_is_lib_vra");
	if (unlikely(!lib_vra->task_vra.task)) {
		err_lib("failed to create VRA task");
		return -ENOMEM;
	}

	ret = sched_setscheduler_nocheck(lib_vra->task_vra.task,
		SCHED_NORMAL, &param);
	if (ret) {
		err("sched_setscheduler_nocheck is fail(%d)", ret);
		return ret;
	}

	lib_vra->task_vra.work_index = 0;

	for (cnt = 0; cnt < FIMC_IS_MAX_TASK; cnt++) {
		lib_vra->task_vra.work[cnt].func = NULL;
		lib_vra->task_vra.work[cnt].params = NULL;
		init_kthread_work(&lib_vra->task_vra.work[cnt].work,
			fimc_is_lib_vra_task_work);
	}

	return 0;
}

void fimc_is_lib_vra_control_set_event(u32 event_type)
{
	int ret;
	struct fimc_is_lib_vra *lib_vra;

	if (unlikely(!g_lib_vra)) {
		err_lib("VRA library is NULL");
		return;
	}

	lib_vra = g_lib_vra;

	switch (event_type) {
	case CTRL_TASK_SET_CH0_INT:
		lib_vra->ctl_task_type = CTRL_TASK_SET_CH0_INT;
		fimc_is_lib_vra_task_trigger(lib_vra,
			fimc_is_lib_vra_invoke_contol_event);
		break;
	case CTRL_TASK_SET_CH1_INT:
		lib_vra->ctl_task_type = CTRL_TASK_SET_CH1_INT;
		fimc_is_lib_vra_task_trigger(lib_vra,
			fimc_is_lib_vra_invoke_contol_event);
		break;
	case CTRL_TASK_SET_NEWFR:
		lib_vra->ctl_task_type = CTRL_TASK_SET_NEWFR;
		ret = fimc_is_lib_vra_invoke_contol_event(lib_vra);
		if (ret) {
			err_lib("vra control set is fail(%#x)", ret);
			return;
		}
		break;
	case CTRL_TASK_SET_ABORT:
		lib_vra->ctl_task_type = CTRL_TASK_SET_ABORT;
		ret = fimc_is_lib_vra_invoke_contol_event(lib_vra);
		if (ret) {
			err_lib("vra control set is fail(%d)", ret);
			return;
		}
		break;
	case CTRL_TASK_SET_FWALGS:
		lib_vra->ctl_task_type = CTRL_TASK_SET_FWALGS;
		fimc_is_lib_vra_task_trigger(lib_vra,
			fimc_is_lib_vra_invoke_contol_event);
		break;
	default:
		err_lib("vra_control_set_event is undefine (%d)", event_type);
		break;

	}
}

void fimc_is_lib_vra_fw_algs_set_event(u32 event_type)
{
	struct fimc_is_lib_vra *lib_vra;

	lib_vra = g_lib_vra;

	switch (event_type) {
	case FWALGS_TASK_SET_ABORT:
		/* This event is processed in fimc_is_lib_vra_fwalgs_stop */
		break;
	case FWALGS_TASK_SET_ALGS:
		lib_vra->algs_task_type = FWALGS_TASK_SET_ALGS;
		dbg_lib("FWALGS_TASK_SET_ALGS\n");
		fimc_is_lib_vra_task_trigger(lib_vra,
			fimc_is_lib_vra_invoke_fwalgs_event);
		break;
	default:
		break;
	}
}

int fimc_is_lib_vra_alloc_memory(struct fimc_is_lib_vra *lib_vra,
		ulong dma_address)
{
	int index;
	enum api_vra_type status = VRA_NO_ERROR;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	/* ToDo: make define */
	lib_vra->alloc_info.max_image.width = 640;
	lib_vra->alloc_info.max_image.height = 480;
	lib_vra->alloc_info.track_faces = 30;
	lib_vra->alloc_info.dt_faces_hw_res = 2100;
	lib_vra->alloc_info.tr_hw_res_per_face = 110;
	lib_vra->alloc_info.ff_hw_res_per_face = 500;
	lib_vra->alloc_info.ff_hw_res_per_list =
		lib_vra->alloc_info.ff_hw_res_per_face;
	lib_vra->alloc_info.cache_line_length = 32;
	lib_vra->alloc_info.pad_size = 0;
	lib_vra->alloc_info.allow_3planes = 0;
	lib_vra->alloc_info.image_slots = 5;
	lib_vra->alloc_info.max_sensors = VRA_TOTAL_SENSORS;
	lib_vra->alloc_info.max_tr_res_frames = 5;

	status = lib_vra->itf_func.ex_get_memory_sizes(&lib_vra->alloc_info,
			&lib_vra->fr_work_size, &lib_vra->frame_desc_size,
			&lib_vra->dma_out_size);
	if (status) {
		err_lib("get_memory_size is fail (%d)", status);
		return -ENOMEM;
	}

	if (SIZE_VRA_INTERNEL_BUF < lib_vra->dma_out_size) {
		err_lib("invalid dma size");
		return -ENOMEM;
	}

	dbg_lib("vra frame work:: dma_size (%d), frame_desc_size(%d), fr_work_size(%d)\n",
		lib_vra->dma_out_size, lib_vra->frame_desc_size, lib_vra->fr_work_size);

#ifdef ENABLE_RESERVED_INTERNAL_DMA
	lib_vra->dma_out_heap =
		fimc_is_alloc_dma_buffer(lib_vra->dma_out_size);
	memset(lib_vra->dma_out_heap, 0, lib_vra->dma_out_size);
#else
	lib_vra->dma_out_heap = (void *)dma_address;
#endif

	lib_vra->fr_work_heap =
		fimc_is_alloc_reserved_buffer(lib_vra->fr_work_size);
	memset(lib_vra->fr_work_heap, 0, lib_vra->fr_work_size);

	for (index = 0; index < VRA_TOTAL_SENSORS; index++) {
		lib_vra->frame_desc_heap[index] =
			fimc_is_alloc_reserved_buffer(lib_vra->frame_desc_size);
		memset(lib_vra->frame_desc_heap[index], 0, lib_vra->frame_desc_size);
	}

	dbg_lib("vra frame work: dma_out_heap (0x%p), fr_work_heap(0x%p)\n",
		lib_vra->dma_out_heap, lib_vra->fr_work_heap);
	for (index = 0; index < VRA_TOTAL_SENSORS; index++)
		dbg_lib("frame_desc_heap_%d (0x%p)\n", index,
			lib_vra->frame_desc_heap[index]);

	return 0;
}

int fimc_is_lib_vra_free_memory(struct fimc_is_lib_vra *lib_vra)
{
	u32 index;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

#ifdef ENABLE_RESERVED_INTERNAL_DMA
	fimc_is_free_dma_buffer(lib_vra->dma_out_heap);
#endif

	fimc_is_free_reserved_buffer(lib_vra->fr_work_heap);

	for (index = 0; index < VRA_TOTAL_SENSORS; index++)
		fimc_is_free_reserved_buffer(lib_vra->frame_desc_heap[index]);

	return 0;
}

int fimc_is_lib_vra_init_frame_work(struct fimc_is_lib_vra *lib_vra,
		void __iomem *base_addr,
		enum fimc_is_lib_vra_input_type input_type)
{
	enum api_vra_type status = VRA_NO_ERROR;
	int ret;
	int index;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	/* Connected vra library to global value */
	g_lib_vra = lib_vra;

	lib_vra->fr_index = 0;
#ifdef VRA_DMA_TEST_BY_IMAGE
	lib_vra->image_load = false;
#endif

	spin_lock_init(&lib_vra->ctl_lock);
	spin_lock_init(&lib_vra->algs_lock);
	spin_lock_init(&lib_vra->intr_lock);

	lib_vra->dma_out.total_size = lib_vra->dma_out_size;
	lib_vra->dma_out.base_adr = lib_vra->dma_out_heap;
	lib_vra->dma_out.is_cached = true;

	lib_vra->fr_work_init.hw_regs_base_adr = (ulong)base_addr;
	lib_vra->fr_work_init.int_type = VRA_INT_LEVEL;

	if (input_type == VRA_INPUT_OTF) {
		lib_vra->fr_work_init.dram_input = false;
	} else if (input_type == VRA_INPUT_MEMORY) {
		lib_vra->fr_work_init.dram_input = true;
	} else {
		err_lib("VRA input type is unknown");
		return -EINVAL;
	}

	dbg_lib("vra frame work base address(%#lx)\n",
		(ulong)lib_vra->fr_work_init.hw_regs_base_adr);

	lib_vra->fr_work_init.call_backs.frw_abort_func_ptr =
		fimc_is_lib_vra_callback_frw_abort;
	lib_vra->fr_work_init.call_backs.frw_hw_err_func_ptr =
		fimc_is_lib_vra_callback_frw_hw_err;
	lib_vra->fr_work_init.call_backs.sen_out_ready_func_ptr =
		fimc_is_lib_vra_callback_output_ready;
	lib_vra->fr_work_init.call_backs.sen_end_input_proc_ptr =
		fimc_is_lib_vra_callback_end_input;
	lib_vra->fr_work_init.call_backs.sen_error_ptr =
		fimc_is_lib_vra_callback_frame_error;
	lib_vra->fr_work_init.call_backs.sen_stat_collected_ptr = NULL;

	lib_vra->fr_work_init.hw_clock_freq_mhz = 533; /* Not used */
	lib_vra->fr_work_init.sw_clock_freq_mhz = 400;
	lib_vra->fr_work_init.block_new_fr_on_transaction = false;
	lib_vra->fr_work_init.block_new_fr_on_input_set = false;
	lib_vra->fr_work_init.wait_on_lock = true;
	lib_vra->fr_work_init.reset_uniqu_id_on_reset_list = true;
	lib_vra->fr_work_init.crop_faces_out_of_image_pixels = true;

	status = lib_vra->itf_func.vra_frame_work_init(lib_vra->fr_work_heap,
			lib_vra->fr_work_size, &lib_vra->alloc_info,
			&lib_vra->fr_work_init, &lib_vra->dma_out,
			VRA_DICO_API_VERSION);
	if (status) {
		err_lib("frame work init fail(%d)", status);
		ret = -EINVAL;
		goto free;
	}

	for (index = 0; index < FIMC_IS_MAX_NODES; index++)
		clear_bit(VRA_LIB_INIT, &lib_vra->state.frame_desc[index]);

	return 0;
free:
	ret = fimc_is_lib_vra_free_memory(lib_vra);
	if (ret) {
		err_lib("VRA memory free is fail");
		return ret;
	}

	return ret;
}

int fimc_is_lib_vra_init_frame_desc(struct fimc_is_lib_vra *lib_vra, u32 instance)
{
	enum api_vra_type status;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	/*
	 * Default set for create frame descript.
	 * The value are changed in the set param
	 */
	if (lib_vra->fr_work_init.dram_input) {
		lib_vra->frame_desc[instance].sizes.width = 640;
		lib_vra->frame_desc[instance].sizes.height = 480;
		lib_vra->frame_desc[instance].hdr_lines = 0;
		lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_422;
		lib_vra->frame_desc[instance].u_before_v = true;
		lib_vra->frame_desc[instance].dram.pix_component_store_bits = 8;
		lib_vra->frame_desc[instance].dram.pix_component_data_bits = 8;
		lib_vra->frame_desc[instance].dram.planes_num = 1;
		lib_vra->frame_desc[instance].dram.un_pack_data = 0;
		lib_vra->frame_desc[instance].dram.line_ofs_fst_plane =
			lib_vra->frame_desc[instance].sizes.width * 2;
		lib_vra->frame_desc[instance].dram.line_ofs_other_planes = 0;
		lib_vra->frame_desc[instance].dram.adr_ofs_bet_planes =
			lib_vra->frame_desc[instance].sizes.height *
			lib_vra->frame_desc[instance].dram.line_ofs_fst_plane;
	} else {
		lib_vra->frame_desc[instance].sizes.width = 640;
		lib_vra->frame_desc[instance].sizes.height = 480;
		lib_vra->frame_desc[instance].hdr_lines = 0;
		lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_444;
		lib_vra->frame_desc[instance].u_before_v = true;
	}

	status = lib_vra->itf_func.vra_sensor_init(lib_vra->frame_desc_heap[instance],
			lib_vra->frame_desc_size, 0,
			&lib_vra->frame_desc[instance], VRA_TRM_ROI_TRACK);
	if (status) {
		err_lib("[%d]VRA frame descript init fail(%#x)", instance, status);
		return -EINVAL;
	}

	return 0;
}

int fimc_is_lib_vra_create_object(struct fimc_is_lib_vra *lib_vra,
	void __iomem *base_addr, enum fimc_is_lib_vra_input_type input_type,
	u32 instance)
{
	int ret;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	if (!test_bit(VRA_LIB_INIT, &lib_vra->state.frame_work)) {
		ret = fimc_is_lib_vra_init_frame_work(lib_vra, base_addr, input_type);
		if (ret) {
			err_lib("VRA frame work init is fail");
			return ret;
		}
		set_bit(VRA_LIB_INIT, &lib_vra->state.frame_work);
	}

	if (!test_bit(VRA_LIB_INIT, &lib_vra->state.frame_desc[instance])) {
		ret = fimc_is_lib_vra_init_frame_desc(lib_vra, instance);
		if (ret) {
			err_lib("VRA frame descript init is fail (%d)", instance);
			return ret;
		}
		set_bit(VRA_LIB_INIT, &lib_vra->state.frame_desc[instance]);
	}

	return 0;
}

int fimc_is_lib_vra_set_orientation(struct fimc_is_lib_vra *lib_vra,
	u32 scaler_orientation, u32 instance)
{
	enum api_vra_type status = VRA_NO_ERROR;
	enum api_vra_orientation vra_orientation;
	enum fimc_is_lib_vra_dir dir;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	dir = lib_vra->tune[instance].dir;

	if (dir == VRA_REAR_ORIENTATION) {
		switch (scaler_orientation) {
		case 0:
			vra_orientation = VRA_ORIENT_TOP_LEFT_TO_RIGHT;
			break;
		case 90:
			vra_orientation = VRA_ORIENT_TOP_RIGHT_TO_BTM;
			break;
		case 180:
			vra_orientation = VRA_ORIENT_BTM_RIGHT_TO_LEFT;
			break;
		case 270:
			vra_orientation = VRA_ORIENT_BTM_LEFT_TO_TOP;
			break;
		default:
			warn_lib("vra_set_orientation: unknown orientation");
			vra_orientation = VRA_ORIENT_TOP_LEFT_TO_RIGHT;
			break;
		}
	} else if (dir == VRA_FRONT_ORIENTATION) {
		switch (scaler_orientation) {
		case 0:
			vra_orientation = VRA_ORIENT_TOP_LEFT_TO_RIGHT;
			break;
		case 90:
			vra_orientation = VRA_ORIENT_BTM_LEFT_TO_TOP;
			break;
		case 180:
			vra_orientation = VRA_ORIENT_BTM_RIGHT_TO_LEFT;
			break;
		case 270:
			vra_orientation = VRA_ORIENT_TOP_RIGHT_TO_BTM;
			break;
		default:
			warn_lib("vra_set_orientation: unknown orientation");
			vra_orientation = VRA_ORIENT_TOP_LEFT_TO_RIGHT;
			break;
		}
	}

	dbg_lib("scaler orientation(%d) --> vra orientation index(%d)\n",
		scaler_orientation, vra_orientation);

	status = lib_vra->itf_func.set_orientation(lib_vra->frame_desc_heap[instance],
			vra_orientation);
	if (status) {
		err_lib("vra_set_orientation if fail(%#x)", status);
		return -EINVAL;
	}

	return 0;
}

int fimc_is_lib_vra_new_frame(struct fimc_is_lib_vra *lib_vra,
	unsigned char *buffer, u32 instance)
{
	enum api_vra_type status = VRA_NO_ERROR;
	unsigned char *input_dma_buf = NULL;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

#ifdef VRA_DMA_TEST_BY_IMAGE
	fimc_is_lib_vra_test_image_load(lib_vra);
	input_dma_buf = lib_vra->test_input_buffer;
#endif
	input_dma_buf = buffer;

	status = lib_vra->itf_func.on_new_frame(lib_vra->frame_desc_heap[instance],
			lib_vra->fr_index,
			0,
			input_dma_buf);
	if (status == VRA_ERR_NEW_FR_PREV_REQ_NOT_HANDLED ||
		status == VRA_ERR_NEW_FR_NEXT_EXIST ||
		status == VRA_BUSY ||
		status == VRA_ERR_FRWORK_ABORTING) {
		err_lib("VRA sensor new frame set fail(%#x)", status);
		return -EINVAL;
	}

	return 0;
}

int fimc_is_lib_vra_handle_interrupt(struct fimc_is_lib_vra *lib_vra, u32 id)
{
	enum api_vra_type result;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	spin_lock(&lib_vra->intr_lock);
	result = lib_vra->itf_func.on_interrupt(lib_vra->fr_work_heap, id);
	if (result) {
		err_lib("VRA API interrupt handle fail (%#x)", result);
		spin_unlock(&lib_vra->intr_lock);
		return -EINVAL;
	}
	spin_unlock(&lib_vra->intr_lock);

	return 0;
}

static int fimc_is_lib_vra_fwalgs_stop(struct fimc_is_lib_vra *lib_vra)
{
	int ret;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	lib_vra->algs_task_type = FWALGS_TASK_SET_ABORT;
	dbg_lib("FWALGS_TASK_SET_ABORT\n");

	ret = fimc_is_lib_vra_invoke_fwalgs_event(lib_vra);
	if (ret) {
		err_lib("%s: FWALGS_TASK_SET_ABORT fail(%#x)",
			__func__, ret);
		return ret;
	}

	return 0;
}

int fimc_is_lib_vra_stop(struct fimc_is_lib_vra *lib_vra)
{
	int ret;
	enum api_vra_type result;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	result = lib_vra->itf_func.frame_work_abort(lib_vra->fr_work_heap, true);
	if (result) {
		err_lib("vra_abort is fail (%#x)", result);
		return -EINVAL;
	}

	ret = fimc_is_lib_vra_fwalgs_stop(lib_vra);
	if (ret) {
		err_lib("vra_fwalgs_abort is fail(%d)", ret);
		return ret;
	}

	return 0;
}

int fimc_is_lib_vra_destory_object(struct fimc_is_lib_vra *lib_vra, u32 instance)
{
	enum api_vra_type result;
	int ret;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	result = lib_vra->itf_func.frame_work_terminate(lib_vra->fr_work_heap);
	if (result) {
		err_lib("vra_terminate is fail (%#x)", result);
		return -EINVAL;
	}

	ret = fimc_is_lib_vra_fwalgs_stop(lib_vra);
	if (ret) {
		err_lib("vra_fwalgs_abort is fail(%d)", ret);
		return ret;
	}

	if (lib_vra->task_vra.task != NULL)
		kthread_stop(lib_vra->task_vra.task);

	clear_bit(VRA_LIB_INIT, &lib_vra->state.frame_work);
	clear_bit(VRA_LIB_INIT, &lib_vra->state.frame_desc[instance]);

	return 0;
}

int fimc_is_lib_vra_update_dm(struct fimc_is_lib_vra *lib_vra,
	enum facedetect_mode *faceDetectMode, struct camera2_stats_dm *dm)
{
	int face_num;

	if (unlikely(!lib_vra)) {
		err_lib("fimc_is_lib_vra is NULL");
		return -EINVAL;
	}

	if (unlikely(!faceDetectMode)) {
		err_lib("fimc_is_lib_vra is NULL");
		return -EINVAL;
	}

	if (unlikely(!dm)) {
		err_lib("camera2_stats_dm is NULL");
		return -EINVAL;
	}

	if (*faceDetectMode == FACEDETECT_MODE_OFF) {
		dm->faceDetectMode = FACEDETECT_MODE_OFF;
		return 0;
	}

	/* ToDo: FACEDETECT_MODE_FULL*/
	dm->faceDetectMode = FACEDETECT_MODE_SIMPLE;

	if (!lib_vra->all_face_num) {
		memset(&dm->faceIds, 0, sizeof(dm->faceIds));
		memset(&dm->faceRectangles, 0, sizeof(dm->faceRectangles));
		memset(&dm->faceScores, 0, sizeof(dm->faceScores));
		memset(&dm->faceLandmarks, 0, sizeof(dm->faceIds));
	}

	for (face_num = 0; face_num < lib_vra->all_face_num; face_num++) {
		/* X min */
		dm->faceRectangles[face_num][0] =
			lib_vra->out_faces[face_num].base.rect.left;
		/* Y min */
		dm->faceRectangles[face_num][1] =
			lib_vra->out_faces[face_num].base.rect.top;
		/* X max */
		dm->faceRectangles[face_num][2] =
			lib_vra->out_faces[face_num].base.rect.left +
			lib_vra->out_faces[face_num].base.rect.width;
		/* Y max */
		dm->faceRectangles[face_num][3] =
			lib_vra->out_faces[face_num].base.rect.top +
			lib_vra->out_faces[face_num].base.rect.height;
		/* Score */
		dm->faceScores[face_num] =
			lib_vra->out_faces[face_num].base.score > 0xff ?
			0xff : lib_vra->out_faces[face_num].base.score;
		/* ID */
		dm->faceIds[face_num] =
			lib_vra->out_faces[face_num].base.unique_id;

		dbg_lib("%s: face position <%d, %d>\n", __func__,
			dm->faceRectangles[face_num][0],
			dm->faceRectangles[face_num][1]);
		dbg_lib("face size <%d, %d> confidance: <%d> id: <%d>\n",
			dm->faceRectangles[face_num][2],
			dm->faceRectangles[face_num][3],
			dm->faceScores[face_num], dm->faceIds[face_num]);
	}

	/* ToDo: Add error handler for detected face range */

	return 0;
}

int fimc_is_lib_vra_update_sm(struct fimc_is_lib_vra *lib_vra)
{
	/* ToDo */
	return 0;
}

int fimc_is_lib_vra_get_meta(struct fimc_is_lib_vra *lib_vra,
				struct camera2_shot *shot)
{
	int ret = 0;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	if (unlikely(!shot)) {
		err_lib("camera2_shot is NULL");
		return -EINVAL;
	}

	ret = fimc_is_lib_vra_update_dm(lib_vra,
			&shot->ctl.stats.faceDetectMode, &shot->dm.stats);
	if (ret) {
		err_lib("VRA get meta is fail (%#x)", ret);
		return -EINVAL;
	}

	/* ToDo : fimc_is_lib_vra_update_sm */

	return 0;
}

#ifdef VRA_DMA_TEST_BY_IMAGE
int fimc_is_lib_vra_test_image_load(struct fimc_is_lib_vra *lib_vra)
{
	int ret = 0;
	struct file *vra_dma_image = NULL;
	long fsize, nread;
	mm_segment_t old_fs;

	if (lib_vra->image_load)
		return 0;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	vra_dma_image = filp_open(VRA_DMA_TEST_IMAGE_PATH, O_RDONLY, 0);
	if (unlikely(!vra_dma_image)) {
		err("filp_open fail!!\n");
		return -EEXIST;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fsize = vra_dma_image->f_path.dentry->d_inode->i_size;
	fsize -= 1;

	dbg_lib("%s: start, file path: size %ld Bytes\n", __func__, fsize);

	lib_vra->test_input_buffer = fimc_is_alloc_dma_buffer(fsize);
	nread = vfs_read(vra_dma_image,
			(char __user *)lib_vra->test_input_buffer,
			fsize, &vra_dma_image->f_pos);
	if (nread != fsize) {
		err_lib("failed to read firmware file, %ld Bytes", nread);
		ret = -EINVAL;
		goto buf_free;
	}

	lib_vra->image_load = true;
	set_fs(old_fs);
	return 0;

buf_free:
	fimc_is_free_dma_buffer(lib_vra->test_input_buffer);
	set_fs(old_fs);

	return ret;
}
#endif

void fimc_is_lib_vra_os_funcs(struct fimc_is_lib_vra_os_system_funcs *funcs)
{
	if (unlikely(!vra_binary_load)) {
		err_lib("VRA library is not loading");
		return;
	}

	funcs->control_task_set_event = fimc_is_lib_vra_control_set_event;
	funcs->fw_algs_task_set_event = fimc_is_lib_vra_fw_algs_set_event;
	funcs->set_dram_adr_from_core_to_vdma = fimc_is_translate_kva_to_dva;
	funcs->clean_cache_region = fimc_is_res_cache_invalid;
	funcs->invalidate_cache_region = fimc_is_res_cache_invalid;
	funcs->data_write_back_cache_region = fimc_is_res_cache_flush;
	funcs->log_write_console = fimc_is_log_write_console;
	funcs->log_write = fimc_is_log_write;

	((vra_load_binary_funcs_t)LIB_VRA_BINARY_ADDR)((void *)funcs);
}

int fimc_is_lib_vra_get_interface_funcs(struct fimc_is_lib_vra *lib_vra)
{
	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	get_lib_vra_func((void *)&lib_vra->itf_func);

	dbg_lib("VRA library interface functions setting\n");

	return 0;
}

int fimc_is_lib_vra_config_param(struct fimc_is_lib_vra *lib_vra,
		struct vra_param *param, u32 instance)
{
	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	if (unlikely(!param)) {
		err_lib("VRA param is NULL");
		return -EINVAL;
	}

	if (param->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		lib_vra->frame_desc[instance].sizes.width = param->otf_input.width;
		lib_vra->frame_desc[instance].sizes.height = param->otf_input.height;
		lib_vra->frame_desc[instance].hdr_lines = 0;

		switch (param->otf_input.format) {
		case OTF_OUTPUT_FORMAT_YUV444:
			lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_444;
			lib_vra->frame_desc[instance].u_before_v = true;
			break;
		case OTF_OUTPUT_FORMAT_YUV422:
			lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_422;
			lib_vra->frame_desc[instance].u_before_v = true;
			break;
		case OTF_OUTPUT_FORMAT_YUV420:
			lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_420;
			lib_vra->frame_desc[instance].u_before_v = true;
			break;
		default:
			err_lib("VRA OTF output format is wrong!!");
			break;
		}
	} else if (param->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		lib_vra->frame_desc[instance].sizes.width = param->dma_input.width;
		lib_vra->frame_desc[instance].sizes.height = param->dma_input.height;
		lib_vra->frame_desc[instance].hdr_lines = 0;

		switch (param->dma_input.format) {
		case DMA_OUTPUT_FORMAT_YUV444:
			lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_444;
			lib_vra->frame_desc[instance].u_before_v = true;
			lib_vra->frame_desc[instance].dram.line_ofs_fst_plane =
			param->dma_input.width * 3;
			break;
		case DMA_OUTPUT_FORMAT_YUV422:
			lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_422;
			lib_vra->frame_desc[instance].u_before_v =
				param->dma_input.plane == 2 ? false : true;
			lib_vra->frame_desc[instance].dram.line_ofs_fst_plane =
				param->dma_input.width * 2;
			break;
		case DMA_OUTPUT_FORMAT_YUV420:
			lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_420;
			lib_vra->frame_desc[instance].u_before_v = true;
			lib_vra->frame_desc[instance].dram.line_ofs_fst_plane =
				param->dma_input.width;
			break;
		default:
			err_lib("VRA DMA output format is wrong!!");
			break;
		}

		lib_vra->frame_desc[instance].dram.pix_component_store_bits =
			param->dma_input.bitwidth;
		lib_vra->frame_desc[instance].dram.pix_component_data_bits =
			param->dma_input.bitwidth;
		lib_vra->frame_desc[instance].dram.planes_num = param->dma_input.plane;
		lib_vra->frame_desc[instance].dram.un_pack_data = 0;
		lib_vra->frame_desc[instance].dram.line_ofs_other_planes = 0;
		lib_vra->frame_desc[instance].dram.adr_ofs_bet_planes =
			param->dma_input.height * lib_vra->frame_desc[instance].dram.line_ofs_fst_plane;
	} else {
		err_lib("VRA output format is wrong!!");
		return -EINVAL;
	}

#ifdef VRA_DMA_TEST_BY_IMAGE
	track_mode = VRA_TRM_SINGLE_FRAME;
	lib_vra->frame_desc[instance].sizes.width = 640;
	lib_vra->frame_desc[instance].sizes.height = 480;
	lib_vra->frame_desc[instance].hdr_lines = 0;
	lib_vra->frame_desc[instance].yuv_format = VRA_YUV_FMT_422;
	lib_vra->frame_desc[instance].u_before_v = true;
	lib_vra->frame_desc[instance].dram.pix_component_store_bits = 8;
	lib_vra->frame_desc[instance].dram.pix_component_data_bits = 8;
	lib_vra->frame_desc[instance].dram.planes_num = 1;
	lib_vra->frame_desc[instance].dram.un_pack_data = 0;
	lib_vra->frame_desc[instance].dram.line_ofs_fst_plane =
		lib_vra->frame_desc[instance].sizes.width * 2;
	lib_vra->frame_desc[instance].dram.line_ofs_other_planes = 0;
	lib_vra->frame_desc[instance].dram.adr_ofs_bet_planes =
		lib_vra->frame_desc[instance].sizes.height *
		lib_vra->frame_desc[instance].dram.line_ofs_fst_plane;
#endif

	return 0;
}

int fimc_is_lib_vra_set_param(struct fimc_is_lib_vra *lib_vra,
		struct fimc_is_lib_vra_tune_data *vra_tune,
		enum fimc_is_lib_vra_dir dir, u32 instance)
{
	struct api_vra_tune_data dbg_tune;
	enum api_vra_orientation dbg_orientation;
	enum api_vra_type status;
	int cnt;

	if (unlikely(!lib_vra)) {
		err_lib("VRA library is NULL");
		return -EINVAL;
	}

	lib_vra->tune[instance].dir = dir;

	if (!vra_tune) {
		lib_vra->tune[instance].api_tune.tracking_mode = VRA_TRM_ROI_TRACK;
		lib_vra->tune[instance].api_tune.enable_features = 0;
		lib_vra->tune[instance].api_tune.full_frame_detection_freq = 1;
		lib_vra->tune[instance].api_tune.min_face_size = 40;
		lib_vra->tune[instance].api_tune.max_face_count = 10;
		lib_vra->tune[instance].api_tune.face_priority = 1;
		lib_vra->tune[instance].api_tune.disable_frontal_rot_mask = 0x00;
		lib_vra->tune[instance].api_tune.disable_profile_rot_mask = 0xFE;
		lib_vra->tune[instance].api_tune.working_point = 865;
		lib_vra->tune[instance].api_tune.tracking_smoothness = 10;

		lib_vra->tune[instance].frame_lock.lock_frame_num = 0;
		lib_vra->tune[instance].frame_lock.init_frames_per_lock = 1;
		lib_vra->tune[instance].frame_lock.normal_frames_per_lock = 1;
	} else {
		memcpy(&lib_vra->tune[instance], vra_tune,
				sizeof(struct fimc_is_lib_vra_tune_data));

		if (lib_vra->fr_work_init.dram_input)
			lib_vra->tune[instance].api_tune.full_frame_detection_freq =
					vra_tune->frame_lock.init_frames_per_lock;
		else
			lib_vra->tune[instance].api_tune.full_frame_detection_freq =
					vra_tune->frame_lock.normal_frames_per_lock;
	}

	cnt = lib_vra->itf_func.set_parameter(lib_vra->fr_work_heap,
			lib_vra->frame_desc_heap[instance],
			&lib_vra->tune[instance].api_tune);
	if (cnt) {
		err_lib("VRA set param is fail, %d parameter setting fail", cnt);
		return -EINVAL;
	}

	status = lib_vra->itf_func.set_input(lib_vra->frame_desc_heap[instance],
			&lib_vra->frame_desc[instance],
			VRA_KEEP_TR_DATA_BASE);
	if (status) {
		err_lib("VRA set input fail(%#x)", status);
		return -EINVAL;
	}

	cnt = lib_vra->itf_func.get_parameter(lib_vra->fr_work_heap,
			lib_vra->frame_desc_heap[instance], &dbg_tune, &dbg_orientation);
	if (cnt) {
		err_lib("VRA get param is fail, %d parameter get fail", cnt);
		return -EINVAL;
	}

	dbg_lib("VRA set parameter: tracking_mode(%#x), enable_features(%#x)\n",
			dbg_tune.tracking_mode, dbg_tune.enable_features);
	dbg_lib("min_face_size(%#x), max_face_count(%#x)\n",
			dbg_tune.min_face_size, dbg_tune.max_face_count);
	dbg_lib("full_frame_detection_freq(%#x), face_priority(%#x)\n",
			dbg_tune.full_frame_detection_freq,
			dbg_tune.face_priority);
	dbg_lib("disable_frontal_rot_mask(%#x), disable_profile_rot_mask(%#x)\n",
			dbg_tune.disable_frontal_rot_mask,
			dbg_tune.disable_profile_rot_mask);
	dbg_lib("working_point(%#x), tracking_smoothness(%#x)\n",
			dbg_tune.working_point,
			dbg_tune.tracking_smoothness);

	return 0;
}
