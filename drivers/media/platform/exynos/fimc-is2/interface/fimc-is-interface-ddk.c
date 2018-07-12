/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-interface-ddk.h"
#include "fimc-is-hw-control.h"
#include "sfr/fimc-is-sfr-isp-v310.h"

static int frame_fcount(struct fimc_is_frame *frame, void *data)
{
	return frame->fcount - (u32)(ulong)data;
}

bool check_dma_done(struct fimc_is_hw_ip *hw_ip, u32 instance_id, u32 fcount)
{
	bool ret = false;
	struct fimc_is_frame *frame;
	struct fimc_is_frame *list_frame;
	struct fimc_is_framemgr *framemgr;
	u32 status = 0; /* 0:FRAME_DONE, 1:FRAME_NDONE */
	int wq_id0, wq_id1, output_id0, output_id1;

	BUG_ON(!hw_ip);

	framemgr = hw_ip->framemgr;

	framemgr_e_barrier(framemgr, 0);
	frame = peek_frame(framemgr, FS_COMPLETE);
	framemgr_x_barrier(framemgr, 0);

	dbg_hw("check_dma [ID:%d][F:%d], frame[F:%d]\n", hw_ip->id, fcount, frame->fcount);

	if (frame == NULL) {
		err_hw("[ID:%d][F:%d]check_dma_done: frame(null)!!", hw_ip->id, fcount);
		return 0;
	}

	if (fcount != frame->fcount) {
		framemgr_e_barrier(framemgr, 0);
		list_frame = find_frame(framemgr, FS_COMPLETE, frame_fcount,
					(void *)(ulong)fcount);
		framemgr_x_barrier(framemgr, 0);
		if (list_frame == NULL) {
			warn_hw("[ID:%d][F:%d]invalid frame", hw_ip->id, fcount);
		} else {
			frame = list_frame;
		}
	}

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
		wq_id0 = WORK_30P_FDONE; /* after BDS */
		output_id0 = ENTRY_3AP;
		wq_id1 = WORK_30C_FDONE; /* before BDS */
		output_id1 = ENTRY_3AC;
		break;
	case DEV_HW_3AA1:
		wq_id0 = WORK_31P_FDONE;
		output_id0 = ENTRY_3AP;
		wq_id1 = WORK_31C_FDONE;
		output_id1 = ENTRY_3AC;
		break;
	case DEV_HW_ISP0:
		wq_id0 = WORK_I0P_FDONE; /* chunk output */
		output_id0 = ENTRY_IXP;
		wq_id1 = WORK_I0C_FDONE; /* yuv output */
		output_id1 = ENTRY_IXC;
		break;
	case DEV_HW_ISP1:
		wq_id0 = WORK_I1P_FDONE;
		output_id0 = ENTRY_IXP;
		wq_id1 = WORK_I1C_FDONE;
		output_id1 = ENTRY_IXC;
		break;
	default:
		break;
	}

	if (test_bit(output_id0, &frame->out_flag)) {
		dbg_hw("[ID:%d][F:%d]output_id[0x%x],wq_id[0x%x]\n",
			hw_ip->id, frame->fcount, output_id0, wq_id0);
		fimc_is_hardware_frame_done(hw_ip, NULL, wq_id0, output_id0,
			status, FRAME_DONE_NORMAL);
	}
	if (test_bit(output_id1, &frame->out_flag)) {
		dbg_hw("[ID:%d][F:%d]output_id[0x%x],wq_id[0x%x]\n",
			hw_ip->id, frame->fcount, output_id1, wq_id1);
		fimc_is_hardware_frame_done(hw_ip, NULL, wq_id1, output_id1,
			status, FRAME_DONE_NORMAL);
	}

	return ret;
}

static void fimc_is_lib_io_callback(void *this, enum lib_cb_event_type event_id,
	u32 instance_id)
{
	struct fimc_is_hw_ip *hw_ip;
	u32 status = 0; /* 0:FRAME_DONE, 1:FRAME_NDONE */
	int wq_id, output_id = 0;

	BUG_ON(!this);

	hw_ip = (struct fimc_is_hw_ip *)this;

	switch (event_id) {
	case LIB_EVENT_DMA_A_OUT_DONE:
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]DMA A\n", instance_id, hw_ip->id,
				hw_ip->fcount);

		hw_ip->dma_time = cpu_clock(raw_smp_processor_id());
#if defined(SOC_3AAISP)
		wq_id = WORK_30C_FDONE;
		output_id = ENTRY_3AC;
#else
		switch (hw_ip->id) {
		case DEV_HW_3AA0: /* after BDS */
			wq_id = WORK_30P_FDONE;
			output_id = ENTRY_3AP;
			break;
		case DEV_HW_3AA1:
			wq_id = WORK_31P_FDONE;
			output_id = ENTRY_3AP;
			break;
		case DEV_HW_ISP0: /* chunk output */
			wq_id = WORK_I0P_FDONE;
			output_id = ENTRY_IXP;
			break;
		case DEV_HW_ISP1:
			wq_id = WORK_I1P_FDONE;
			output_id = ENTRY_IXP;
			break;
		default:
			break;
		}
#endif
		fimc_is_hardware_frame_done(hw_ip, NULL, wq_id, output_id,
			status, FRAME_DONE_NORMAL);
		break;
	case LIB_EVENT_DMA_B_OUT_DONE:
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]DMA B\n", instance_id, hw_ip->id,
				hw_ip->fcount);

		hw_ip->dma_time = cpu_clock(raw_smp_processor_id());
		switch (hw_ip->id) {
		case DEV_HW_3AA0: /* before BDS */
			wq_id = WORK_30C_FDONE;
			output_id = ENTRY_3AC;
			break;
		case DEV_HW_3AA1:
			wq_id = WORK_31C_FDONE;
			output_id = ENTRY_3AC;
			break;
		case DEV_HW_ISP0: /* yuv output */
			wq_id = WORK_I0C_FDONE;
			output_id = ENTRY_IXC;
			break;
		case DEV_HW_ISP1:
			wq_id = WORK_I1C_FDONE;
			output_id = ENTRY_IXC;
			break;
		default:
			break;
		}

		fimc_is_hardware_frame_done(hw_ip, NULL, wq_id, output_id,
			status, FRAME_DONE_NORMAL);
		break;
	default:
		break;
	}

	return;
};

static void fimc_is_lib_camera_callback(void *this, enum lib_cb_event_type event_id,
	u32 instance_id, void *data)
{
	struct fimc_is_hw_ip *hw_ip;
	ulong fcount;

	BUG_ON(!this);

	hw_ip = (struct fimc_is_hw_ip *)this;

	switch (event_id) {
	case LIB_EVENT_CONFIG_LOCK:
		fcount = (ulong)data;
		atomic_inc(&hw_ip->cl_count);
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]C.L %d\n", instance_id, hw_ip->id,
				hw_ip->fcount, (u32)fcount);

		fimc_is_hardware_config_lock(hw_ip, instance_id, (u32)fcount);
		break;
	case LIB_EVENT_FRAME_START_ISR:
		hw_ip->fs_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]F.S\n", instance_id, hw_ip->id,
				hw_ip->fcount);

		fimc_is_hardware_frame_start(hw_ip, instance_id);
		break;
	case LIB_EVENT_FRAME_END:
		hw_ip->fe_time = cpu_clock(raw_smp_processor_id());
		atomic_inc(&hw_ip->fe_count);
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]F.E\n", instance_id, hw_ip->id,
				hw_ip->fcount);

		fcount = (ulong)data;
		if (fimc_is_hw_frame_done_with_dma())
			check_dma_done(hw_ip, instance_id, (u32)fcount);
		else
			dbg_hw("dma done interupt separate\n");

		fimc_is_hardware_frame_done(hw_ip, NULL, -1, FIMC_IS_HW_CORE_END,
			 0, FRAME_DONE_NORMAL);
		atomic_dec(&hw_ip->Vvalid);
		wake_up(&hw_ip->wait_queue);
		break;
	default:
		break;
	}

	return;
};

struct lib_callback_func fimc_is_lib_cb_func = {
	.camera_callback	= fimc_is_lib_camera_callback,
	.io_callback		= fimc_is_lib_io_callback,
};

int fimc_is_lib_isp_chain_create(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id)
{
	int ret = 0;
	u32 chain_id, set_b_offset;
	ulong base_addr, base_addr_b;

	BUG_ON(!hw_ip);
	BUG_ON(!this->func);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
		chain_id = 1;
		break;
	case DEV_HW_ISP0:
		chain_id = 0;
		break;
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		return -EINVAL;
		break;
	}

	base_addr    = (ulong)hw_ip->regs;
	base_addr_b  = (ulong)hw_ip->regs_b;
	set_b_offset = (base_addr_b == 0) ? 0 : base_addr_b - base_addr;

	ret = this->func->chain_create(chain_id, base_addr, set_b_offset,
				&fimc_is_lib_cb_func);
	if (ret) {
		err_lib("chain_create fail (%d)", hw_ip->id);
		return -EINVAL;
	}

	info_lib("[%d]chain_create done [ID:%d][reg_base:0x%lx][b_offset:0x%x]\n",
		instance_id, hw_ip->id, base_addr, set_b_offset);

	return ret;
}

int fimc_is_lib_isp_object_create(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id, u32 rep_flag, u32 module_id)
{
	int ret = 0;
	u32 chain_id, input_type, obj_info = 0;
	struct fimc_is_group *parent;

	BUG_ON(!hw_ip);
	BUG_ON(!this->func);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
		chain_id = 1;
		break;
	case DEV_HW_ISP0:
		chain_id = 0;
		break;
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		return -EINVAL;
		break;
	}

	parent = hw_ip->group[instance_id];
	while (parent->parent)
		parent = parent->parent;
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &parent->state))
		input_type = 0; /* default */
	else
		input_type = 1;

	obj_info |= chain_id << CHAIN_ID_SHIFT;
	obj_info |= instance_id << INSTANCE_ID_SHIFT;
	obj_info |= rep_flag << REPROCESSING_FLAG_SHIFT;
	obj_info |= (input_type) << INPUT_TYPE_SHIFT;
	obj_info |= (module_id) << MODULE_ID_SHIFT;

	info_lib("obj_create: chain(%d), instance(%d), rep(%d), in_type(%d),"
		" obj_info(0x%08x), module_id(%d)\n",
		chain_id, instance_id, rep_flag, input_type, obj_info, module_id);
	ret = this->func->object_create(&this->object, obj_info, hw_ip);
	if (ret) {
		err_lib("object_create fail (%d)", hw_ip->id);
		return -EINVAL;
	}

	info_lib("[%d]object_create done [ID:%d]\n", instance_id, hw_ip->id);

	return ret;
}

void fimc_is_lib_isp_chain_destroy(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id)
{
	int ret = 0;
	u32 chain_id;

	BUG_ON(!hw_ip);
	BUG_ON(!this->func);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
		chain_id = 1;
		break;
	case DEV_HW_ISP0:
		chain_id = 0;
		break;
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		return;
		break;
	}

	ret = this->func->chain_destroy(chain_id);
	if (ret) {
		err_lib("chain_destroy fail (%d)", hw_ip->id);
		return;
	}
	info_lib("[%d]chain_destroy done [ID:%d]\n", instance_id, hw_ip->id);

	return;
}

void fimc_is_lib_isp_object_destroy(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id)
{
	int ret = 0;

	BUG_ON(!this);
	BUG_ON(!this->object);

	ret = this->func->object_destroy(this->object, instance_id);
	if (ret) {
		err_lib("object_destroy fail (%d)", hw_ip->id);
		return;
	}

	info_lib("[%d]object_destroy done [ID:%d]\n", instance_id, hw_ip->id);

	return;
}

int fimc_is_lib_isp_set_param(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, void *param)
{
	return 0;
}

int fimc_is_lib_isp_set_ctrl(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, struct fimc_is_frame *frame)
{
	int ret = 0;

	BUG_ON(!this);
	BUG_ON(!this->object);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		ret = this->func->set_ctrl(this->object, frame->instance,
					frame->fcount, frame->shot);
		if (ret)
			err_lib("3aa set_ctrl fail (%d)", hw_ip->id);
		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		ret = this->func->set_ctrl(this->object, frame->instance,
					frame->fcount, frame->shot);
		if (ret)
			err_lib("isp set_ctrl fail (%d)", hw_ip->id);
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		break;
	}

	return 0;
}

void fimc_is_lib_isp_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, void *param_set, struct camera2_shot *shot)
{
	int ret = 0;

	BUG_ON(!this);
	BUG_ON(!this->object);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		ret = this->func->shot(this->object,
				(struct taa_param_set *)param_set, shot);
		if (ret)
			err_lib("3aa shot fail (%d)", hw_ip->id);

		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		ret = this->func->shot(this->object,
				(struct isp_param_set *)param_set, shot);
		if (ret)
			err_lib("isp shot fail (%d)", hw_ip->id);
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		break;
	}
}

int fimc_is_lib_isp_get_meta(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, struct fimc_is_frame *frame)

{
	int ret = 0;

	BUG_ON(!this);
	BUG_ON(!this->object);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		ret = this->func->get_meta(this->object, frame->instance,
					frame->fcount, frame->shot);
		if (ret)
			err_lib("3aa get_meta fail (%d)", hw_ip->id);
		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		ret = this->func->get_meta(this->object, frame->instance,
					frame->fcount, frame->shot);
		if (ret)
			err_lib("isp get_meta fail (%d)", hw_ip->id);
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		break;
	}

	return ret;
}

void fimc_is_lib_isp_stop(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id)
{
	int ret = 0;

	BUG_ON(!this->object);
	BUG_ON(!this->func);

	ret = this->func->stop(this->object, instance_id);
	if (ret) {
		err_lib("object_suspend fail (%d)", hw_ip->id);
		return;
	}
	info_lib("[%d]object_suspend done [ID:%d]\n", instance_id, hw_ip->id);

	return;
}

int fimc_is_lib_isp_create_tune_set(struct fimc_is_lib_isp *this,
	ulong addr, u32 size, u32 index, int flag, u32 instance_id)
{
	int ret = 0;
	struct lib_tune_set tune_set;

	BUG_ON(!this->object);

	tune_set.index = index;
	tune_set.addr = addr;
	tune_set.size = size;
	tune_set.decrypt_flag = flag;
	ret = this->func->create_tune_set(this->object, instance_id, &tune_set);
	if (ret) {
		err_lib("create_tune_set fail (%d)", ret);
		return ret;
	}

	info_lib("[%d]create_tune_set index(%d)\n", instance_id, index);

	return ret;
}

int fimc_is_lib_isp_apply_tune_set(struct fimc_is_lib_isp *this,
	u32 index, u32 instance_id)
{
	int ret = 0;

	BUG_ON(!this->object);

	ret = this->func->apply_tune_set(this->object, instance_id, index);
	if (ret) {
		err_lib("apply_tune_set fail (%d)", ret);
		return ret;
	}

	return ret;
}

int fimc_is_lib_isp_delete_tune_set(struct fimc_is_lib_isp *this,
	u32 index, u32 instance_id)
{
	int ret = 0;

	BUG_ON(!this->object);

	ret = this->func->delete_tune_set(this->object, instance_id, index);
	if (ret) {
		err_lib("delete_tune_set fail (%d)", ret);
		return ret;
	}

	info_lib("[%d]delete_tune_set index(%d)\n", instance_id, index);

	return ret;
}

int fimc_is_lib_isp_load_cal_data(struct fimc_is_lib_isp *this,
	u32 instance_id, ulong addr)
{
	char version[20];
	int ret = 0;

	info_lib("CAL data addr 0x%08lx\n", addr);
	memcpy(version, (void *)(addr + 0x20), 16);
	version[16] = '\0';
	info_lib("CAL version: %s\n", version);

	ret = this->func->load_cal_data(this->object, instance_id, addr);
	if (ret) {
		err_lib("apply_tune_set fail (%d)", ret);
		return ret;
	}

	return ret;
}

int fimc_is_lib_isp_get_cal_data(struct fimc_is_lib_isp *this,
	u32 instance_id, struct cal_info *data, int type)
{
	int ret = 0;

	ret = this->func->get_cal_data(this->object, instance_id, data, type);
	if (ret) {
		err_lib("apply_tune_set fail (%d)", ret);
		return ret;
	}

	return ret;
}

int fimc_is_lib_isp_sensor_info_mode_chg(struct fimc_is_lib_isp *this,
	u32 instance_id, struct camera2_shot *shot)
{
	int ret = 0;

	ret = this->func->sensor_info_mode_chg(this->object, instance_id, shot);
	if (ret) {
		err_lib("sensor_info_mode_chg fail (%d)", ret);
		return ret;
	}

	return ret;
}

int fimc_is_lib_isp_sensor_update_control(struct fimc_is_lib_isp *this,
	u32 instance_id, u32 frame_count, struct camera2_shot *shot)
{
	int ret = 0;

	ret = this->func->sensor_update_ctl(this->object, instance_id,
				frame_count, shot);
	if (ret) {
		err_lib("sensor_update_ctl fail (%d)", ret);
		return ret;
	}

	return ret;
}

int fimc_is_lib_isp_convert_face_map(struct fimc_is_hardware *hardware,
	struct taa_param_set *param_set, struct fimc_is_frame *frame)
{
	int ret = 0;
	int i;
	u32 preview_width, preview_height;
	u32 bayer_crop_width, bayer_crop_height;
	struct fimc_is_group *group = NULL;
	struct camera2_shot *shot = NULL;
	struct fimc_is_device_ischain *device = NULL;

	BUG_ON(!hardware);
	BUG_ON(!param_set);
	BUG_ON(!frame);

	shot = frame->shot;
	BUG_ON(!shot);

	group = (struct fimc_is_group *)frame->group;
	BUG_ON(!group);

	device = group->device;
	BUG_ON(!device);

	if (shot->uctl.fdUd.faceDetectMode == FACEDETECT_MODE_OFF)
		return 0;

	/*
	 * The face size which an algorithm uses is determined
	 * by the input bayer crop size of 3aa.
	 */
	if (param_set->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		bayer_crop_width = param_set->otf_input.bayer_crop_width;
		bayer_crop_height = param_set->otf_input.bayer_crop_height;
	} else if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		bayer_crop_width = param_set->dma_input.bayer_crop_width;
		bayer_crop_height = param_set->dma_input.bayer_crop_height;
	} else {
		err_hw("invalid hw input!!\n");
		return -EINVAL;
	}

	if (bayer_crop_width == 0 || bayer_crop_height == 0) {
		warn_hw("%s: invalid crop size (%d * %d)!!\n",
			__func__, bayer_crop_width, bayer_crop_height);
		return 0;
	}

	/* The face size is determined by the preview size in FD uctl */
	preview_width = device->scp.output.width;
	preview_height = device->scp.output.height;
	if (preview_width == 0 || preview_height == 0) {
		dbg_hw("%s: invalid preview size (%d * %d)!!\n",
			__func__, preview_width, preview_height);
		return 0;
	}

	/* Convert face size */
	for (i = 0; i < CAMERA2_MAX_FACES; i++) {
		if (shot->uctl.fdUd.faceScores[i] == 0)
			continue;

		shot->uctl.fdUd.faceRectangles[i][0] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][0],
				preview_width, bayer_crop_width);
		shot->uctl.fdUd.faceRectangles[i][1] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][1],
				preview_height, bayer_crop_height);
		shot->uctl.fdUd.faceRectangles[i][2] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][2],
				preview_width, bayer_crop_width);
		shot->uctl.fdUd.faceRectangles[i][3] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][3],
				preview_height, bayer_crop_height);

		dbg_lib("%s: ID(%d), x_min(%d), y_min(%d), x_max(%d), y_max(%d)\n",
			__func__, shot->uctl.fdUd.faceIds[i],
			shot->uctl.fdUd.faceRectangles[i][0],
			shot->uctl.fdUd.faceRectangles[i][1],
			shot->uctl.fdUd.faceRectangles[i][2],
			shot->uctl.fdUd.faceRectangles[i][3]);
	}

	return ret;
}

void fimc_is_isp_get_bcrop1_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width  = fimc_is_hw_get_field(base_addr, &isp_regs[ISP_R_BCROP1_SIZE_X], &isp_fields[ISP_F_BCROP1_SIZE_X]);
	*height = fimc_is_hw_get_field(base_addr, &isp_regs[ISP_R_BCROP1_SIZE_Y], &isp_fields[ISP_F_BCROP1_SIZE_Y]);
}
