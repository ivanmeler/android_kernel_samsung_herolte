/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-mcscaler.h"
#include "api/fimc-is-hw-api-mcscaler.h"
#include "../interface/fimc-is-interface-ischain.h"
#include "fimc-is-param.h"

static int fimc_is_hw_mcsc_handle_interrupt(u32 id, void *context)
{
	struct fimc_is_hw_ip *hw_ip = NULL;
	u32 status, intr_mask, intr_status;
	bool err_intr_flag = false;
	int ret = 0;

	hw_ip = (struct fimc_is_hw_ip *)context;

	/* read interrupt status register (sc_intr_status) */
	intr_mask = fimc_is_scaler_get_intr_mask(hw_ip->regs);
	intr_status = fimc_is_scaler_get_intr_status(hw_ip->regs);
	status = (~intr_mask) & intr_status;

	if (status & (1 << INTR_MC_SCALER_OVERFLOW)) {
		err_hw("[MCSC]OverFlow!!");
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_OUTSTALL)) {
		err_hw("[MCSC]Output Block BLOCKING!!");
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_UNF)) {
		err_hw("[MCSC]Input OTF Vertical Underflow!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_OVF)) {
		err_hw("[MCSC]Input OTF Vertical Overflow!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_UNF)) {
		err_hw("[MCSC]Input OTF Horizontal Underflow!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_OVF)) {
		err_hw("[MCSC]Input OTF Horizontal Overflow!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_PROTOCOL_ERR)) {
		err_hw("[MCSC]Input Protocol Error!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_WDMA_FINISH)) {
		hw_ip->dma_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.E DMA\n", hw_ip->id, hw_ip->fcount);

		atomic_inc(&hw_ip->cl_count);
		fimc_is_hardware_frame_done(hw_ip, NULL, WORK_SCP_FDONE, ENTRY_SCP,
			0, FRAME_DONE_NORMAL);
	}

	if (status & (1 << INTR_MC_SCALER_FRAME_START)) {
		hw_ip->fs_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.S\n", hw_ip->id, hw_ip->fcount);

		atomic_set(&hw_ip->Vvalid, V_VALID);
		clear_bit(HW_CONFIG, &hw_ip->state);
		atomic_inc(&hw_ip->fs_count);
	}

	if (status & (1 << INTR_MC_SCALER_FRAME_END)) {
		hw_ip->fe_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.E\n", hw_ip->id, hw_ip->fcount);

		atomic_set(&hw_ip->Vvalid, V_BLANK);
		if (atomic_read(&hw_ip->fs_count) == atomic_read(&hw_ip->fe_count)) {
			err_hw("[MCSC] fs(%d), fe(%d), cl(%d)\n",
				atomic_read(&hw_ip->fs_count),
				atomic_read(&hw_ip->fe_count),
				atomic_read(&hw_ip->cl_count));
		}
		atomic_inc(&hw_ip->fe_count);
		fimc_is_hardware_frame_done(hw_ip, NULL, -1, FIMC_IS_HW_CORE_END,
			0, FRAME_DONE_NORMAL);
	}

	if (err_intr_flag) {
		u32 hl = 0, vl = 0;

		fimc_is_scaler_get_input_status(hw_ip->regs, &hl, &vl);
		info_hw("[MCSC][F:%d] Ocurred error interrupt (%d,%d)\n",
			hw_ip->fcount, hl, vl);
		fimc_is_hardware_size_dump(hw_ip);
	}

	fimc_is_scaler_clear_intr_src(hw_ip->regs, status);

	return ret;
}

const struct fimc_is_hw_ip_ops fimc_is_hw_mcsc_ops = {
	.open			= fimc_is_hw_mcsc_open,
	.init			= fimc_is_hw_mcsc_init,
	.close			= fimc_is_hw_mcsc_close,
	.enable			= fimc_is_hw_mcsc_enable,
	.disable		= fimc_is_hw_mcsc_disable,
	.shot			= fimc_is_hw_mcsc_shot,
	.set_param		= fimc_is_hw_mcsc_set_param,
	.frame_ndone		= fimc_is_hw_mcsc_frame_ndone,
	.load_setfile		= fimc_is_hw_mcsc_load_setfile,
	.apply_setfile		= fimc_is_hw_mcsc_apply_setfile,
	.delete_setfile		= fimc_is_hw_mcsc_delete_setfile,
	.size_dump		= fimc_is_hw_mcsc_size_dump
};

int fimc_is_hw_mcsc_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc,	int id)
{
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!hw_ip);
	BUG_ON(!itf);
	BUG_ON(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	hw_ip->ops  = &fimc_is_hw_mcsc_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;
	hw_ip->is_leader = false;
	atomic_set(&hw_ip->Vvalid, 0);
	atomic_set(&hw_ip->ref_count, 0);
	init_waitqueue_head(&hw_ip->wait_queue);

	/* set mcsc sfr base address */
	hw_slot = fimc_is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid hw_slot (%d, %d)", id, hw_slot);
		return -EINVAL;
	}

	/* set mcsc interrupt handler */
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &fimc_is_hw_mcsc_handle_interrupt;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	info_hw("[ID:%2d] probe done\n", id);

	return ret;
}

int fimc_is_hw_mcsc_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	*size = sizeof(struct fimc_is_hw_mcsc);

	ret = fimc_is_hw_mcsc_reset(hw_ip);
	if (ret != 0) {
		err_hw("MCSC sw reset fail");
		return -ENODEV;
	}

	atomic_set(&hw_ip->Vvalid, V_BLANK);

	return ret;
}

int fimc_is_hw_mcsc_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
	bool flag, u32 module_id)
{
	int ret = 0;
	u32 instance = 0;

	BUG_ON(!hw_ip);

	instance = group->instance;
	hw_ip->group[instance] = group;

	return ret;
}

int fimc_is_hw_mcsc_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	info_hw("[%d]close (%d)(%d)\n", instance, hw_ip->id, atomic_read(&hw_ip->ref_count));

	return ret;
}

int fimc_is_hw_mcsc_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d]not initialized!! (%d)", instance, hw_ip->id);
		return -EINVAL;
	}

	set_bit(HW_RUN, &hw_ip->state);

	return ret;
}

int fimc_is_hw_mcsc_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	info_hw("[%d][ID:%d]stream_off \n", instance, hw_ip->id);

	if (test_bit(HW_RUN, &hw_ip->state)) {
		/* disable MCSC */
		fimc_is_scaler_clear_dma_out_addr(hw_ip->regs);
		fimc_is_scaler_stop(hw_ip->regs);

		ret = fimc_is_hw_mcsc_reset(hw_ip);
		if (ret != 0) {
			err_hw("MCSC sw reset fail");
			return -ENODEV;
		}

		clear_bit(HW_RUN, &hw_ip->state);
		clear_bit(HW_CONFIG, &hw_ip->state);
	} else {
		dbg_hw("[%d]already disabled (%d)\n", instance, hw_ip->id);
	}

	return ret;
}

int fimc_is_hw_mcsc_shot(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;
	u32 target_addr[4] = {0};

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	dbg_hw("[%d][ID:%d][F:%d]\n", frame->instance, hw_ip->id, frame->fcount);

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] hw_mcsc not initialized\n",
			frame->instance, hw_ip->id);
		return -EINVAL;
	}

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	set_bit(hw_ip->id, &frame->core_flag);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[frame->instance]->parameter.scalerp;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		dbg_hw("[%d][ID:%d] request not exist\n", frame->instance, hw_ip->id);
		goto config;
	}

	fimc_is_hw_mcsc_update_param(param, hw_mcsc);
	fimc_is_hw_mcsc_update_register(hw_ip, frame->instance);

	/* set mcsc dma out addr */
	target_addr[0] = frame->shot->uctl.scalerUd.scpTargetAddress[0];
	target_addr[1] = frame->shot->uctl.scalerUd.scpTargetAddress[1];
	target_addr[2] = frame->shot->uctl.scalerUd.scpTargetAddress[2];

	dbg_hw("[%d][MCSC][F:%d] target addr [0x%x]\n",
		frame->instance, frame->fcount, target_addr[0]);

config:
	if (param->dma_output.cmd != DMA_OUTPUT_COMMAND_DISABLE
		&& target_addr[0] != 0
		&& frame->type != SHOT_TYPE_INTERNAL) {
		fimc_is_scaler_set_dma_enable(hw_ip->regs, true);
		/* use only one buffer (per-frame) */
		fimc_is_scaler_set_dma_out_frame_seq(hw_ip->regs,
			0x1 << USE_DMA_BUFFER_INDEX);
		fimc_is_scaler_set_dma_out_addr(hw_ip->regs,
			target_addr[0], target_addr[1], target_addr[2],
			USE_DMA_BUFFER_INDEX);
	} else {
		fimc_is_scaler_set_dma_enable(hw_ip->regs, false);
		fimc_is_scaler_clear_dma_out_addr(hw_ip->regs);
		dbg_hw("[%d][ID:%d] Disable dma out\n", frame->instance, hw_ip->id);
	}

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		dbg_hw("scp_shot[F:%d][I:%d]\n", frame->fcount, frame->instance);
		fimc_is_scaler_start(hw_ip->regs);
		if (ret) {
			err_hw("[%d][ID:%d]enable failed!!\n", frame->instance, hw_ip->id);
			return -EINVAL;
		}
	}
	set_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

int fimc_is_hw_mcsc_set_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;

	BUG_ON(!hw_ip);
	BUG_ON(!region);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d]not initialized!!", instance);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	param = &region->parameter.scalerp;
	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	fimc_is_hw_mcsc_update_param(param, hw_mcsc);

	return ret;
}

void fimc_is_hw_mcsc_update_param(struct scp_param *param,
	struct fimc_is_hw_mcsc *hw_mcsc)
{
	if (param->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_OTFINPUT_CHANGE] = 1;

	if (param->input_crop.cmd == SCALER_CROP_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_INPUTCROP_CHANGE] = 1;

	if (param->output_crop.cmd == SCALER_CROP_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_OUTPUTCROP_CHANGE] = 1;

	if (param->otf_output.cmd == OTF_OUTPUT_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_OTFOUTPUT_CHANGE] = 1;

	if (param->dma_output.cmd == DMA_OUTPUT_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_DMAOUTPUT_CHANGE] = 1;

	if (SCALER_FLIP_COMMAND_NORMAL <= param->flip.cmd
		&& param->flip.cmd <= SCALER_FLIP_COMMAND_XY_MIRROR)
		hw_mcsc->param_change_flags[MCSC_PARAM_FLIP_CHANGE] = 1;

	if (param->effect.yuv_range >= SCALER_OUTPUT_YUV_RANGE_FULL)
		hw_mcsc->param_change_flags[MCSC_PARAM_YUVRANGE_CHANGE] = 1;
}

int fimc_is_hw_mcsc_update_register(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;

	BUG_ON(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (hw_mcsc->param_change_flags[MCSC_PARAM_OTFINPUT_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_otf_input(hw_ip, instance);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_INPUTCROP_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_input_crop(hw_ip, instance);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_OUTPUTCROP_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_output_crop(hw_ip, instance);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_FLIP_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_flip(hw_ip, instance);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_OTFOUTPUT_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_otf_output(hw_ip, instance);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_DMAOUTPUT_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_dma_output(hw_ip, instance);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_YUVRANGE_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_output_yuvrange(hw_ip, instance);

	return ret;
}

int fimc_is_hw_mcsc_reset(struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	ret = fimc_is_scaler_sw_reset(hw_ip->regs);
	if (ret != 0) {
		err_hw("MCSC sw reset fail");
		return -ENODEV;
	}

	fimc_is_scaler_clear_intr_all(hw_ip->regs);
	fimc_is_scaler_disable_intr(hw_ip->regs);
	fimc_is_scaler_mask_intr(hw_ip->regs, MCSC_INTR_MASK);

	/* set stop req post en to bypass(guided to 2) */
	fimc_is_scaler_set_stop_req_post_en_ctrl(hw_ip->regs, 2);

	return ret;
}

int fimc_is_hw_mcsc_load_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	struct fimc_is_setfile_info *info;
	u32 setfile_index = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		return -ESRCH;
	}

	if (!unlikely(hw_ip->priv_info)) {
		err_hw("MCSC priv info is NULL");
		return -EINVAL;
	}
	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	info = &hw_ip->setfile_info;

	switch (info->version) {
	case SETFILE_V2:
		break;
	case SETFILE_V3:
		break;
	default:
		err_hw("[%d][ID:%d] invalid version (%d)", instance, hw_ip->id,
			info->version);
		return -EINVAL;
	}

	setfile_index = info->index[index];
	hw_mcsc->setfile = (struct hw_api_scaler_setfile *)info->table[setfile_index].addr;
	if (hw_mcsc->setfile->setfile_version != MCSC_SETFILE_VERSION) {
		err_hw("[%d][ID:%d] setfile version(0x%x) is incorrect\n",
			instance, hw_ip->id, hw_mcsc->setfile->setfile_version);
		return -EINVAL;
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int fimc_is_hw_mcsc_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	struct fimc_is_setfile_info *info;
	u32 setfile_index = 0;

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		return -ESRCH;
	}

	if (!unlikely(hw_ip->priv_info)) {
		err_hw("MCSC priv info is NULL");
		return -EINVAL;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	info = &hw_ip->setfile_info;

	if (!hw_mcsc->setfile)
		return 0;

	setfile_index = info->index[index];
	info_hw("[%d][ID:%d] setfile (%d) scenario (%d)\n", instance, hw_ip->id,
		setfile_index, index);

	return ret;
}

int fimc_is_hw_mcsc_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance,
	ulong hw_map)
{
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (test_bit(HW_TUNESET, &hw_ip->state)) {
		warn_hw("[%d][ID:%d] setfile already deleted\n", instance, hw_ip->id);
		return 0;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	hw_mcsc->setfile = NULL;
	clear_bit(HW_TUNESET, &hw_ip->state);

	return 0;
}

int fimc_is_hw_mcsc_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	u32 instance, bool late_flag)
{
	int ret = 0;
	int wq_id, output_id;
	enum fimc_is_frame_done_type done_type;

	if (late_flag == true)
		done_type = FRAME_DONE_LATE_SHOT;
	else
		done_type = FRAME_DONE_FORCE;

	wq_id     = WORK_SCP_FDONE;
	output_id = ENTRY_SCP;
	if (test_bit(output_id, &frame->out_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id, output_id,
				1, done_type);

	wq_id     = -1;
	output_id = FIMC_IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id, output_id,
				1, done_type);

	return ret;
}

int fimc_is_hw_mcsc_otf_input(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	u32 width, height;
	u32 format, bit_width;
	u32 enable = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;
	struct param_otf_input *otf_input;

	BUG_ON(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter.scalerp;
	otf_input = &(param->otf_input);

	width = otf_input->width;
	height = otf_input->height;
	format = otf_input->format;
	bit_width = otf_input->bitwidth;

	if (hw_mcsc->param_change_flags[MCSC_PARAM_OTFINPUT_CHANGE] == 1) {
		enable = 1;
		hw_mcsc->param_change_flags[MCSC_PARAM_OTFINPUT_CHANGE] = 0;
	} else {
		enable = 0;
	}

	/* check otf input param */
	if (width < 16 || width > 8192) {
		err_hw("[%d]Invalid MCSC input width(%d)", instance, width);
		return -EINVAL;
	}

	if (height < 16 || height > 8192) {
		err_hw("[%d]Invalid MCSC input height(%d)", instance, width);
		return -EINVAL;
	}

	if (format != OTF_INPUT_FORMAT_YUV422) {
		err_hw("[%d]Invalid MCSC input format(%d)", instance, format);
		return -EINVAL;
	}

	if (bit_width != OTF_INPUT_BIT_WIDTH_12BIT) {
		err_hw("[%d]Invalid MCSC input bit_width(%d)", instance, bit_width);
		return -EINVAL;
	}

	fimc_is_scaler_set_poly_scaler_enable(hw_ip->regs, enable);
	fimc_is_scaler_set_poly_img_size(hw_ip->regs, width, height);
	fimc_is_scaler_set_dither(hw_ip->regs, enable);

	return ret;
}

int fimc_is_hw_mcsc_effect(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;

	BUG_ON(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	hw_mcsc->param_change_flags[MCSC_PARAM_IMAGEEFFECT_CHANGE] = 0;

	return ret;
}

int fimc_is_hw_mcsc_input_crop(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	u32 img_width, img_height;
	u32 src_pos_x, src_pos_y;
	u32 src_width, src_height;
	u32 out_width, out_height;
	u32 post_img_width, post_img_height;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;
	struct param_scaler_input_crop *input_crop;

	BUG_ON(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter.scalerp;
	input_crop = &(param->input_crop);

	img_width = input_crop->in_width;
	img_height = input_crop->in_height;
	src_pos_x = input_crop->pos_x;
	src_pos_y = input_crop->pos_y;
	src_width = input_crop->crop_width;
	src_height = input_crop->crop_height;
	out_width = input_crop->out_width;
	out_height = input_crop->out_height;

	hw_mcsc->param_change_flags[MCSC_PARAM_INPUTCROP_CHANGE] = 0;

	fimc_is_scaler_set_poly_img_size(hw_ip->regs, img_width, img_height);
	fimc_is_scaler_set_poly_src_size(hw_ip->regs, src_pos_x, src_pos_y,
		src_width, src_height);

	if (out_width < (src_width / 4)) {
		post_img_width = src_width / 4;
		if (out_height < (src_height / 4))
			post_img_height = src_height / 4;
		else
			post_img_height = out_height;
		fimc_is_scaler_set_post_scaler_enable(hw_ip->regs, 1);
	} else {
		post_img_width = out_width;
		if (out_height < (src_height / 4)) {
			post_img_height = src_height / 4;
			fimc_is_scaler_set_post_scaler_enable(hw_ip->regs, 1);
		} else {
			post_img_height = out_height;
			fimc_is_scaler_set_post_scaler_enable(hw_ip->regs, 0);
		}
	}

	fimc_is_scaler_set_poly_dst_size(hw_ip->regs, post_img_width, post_img_height);
	fimc_is_scaler_set_post_img_size(hw_ip->regs, post_img_width, post_img_height);

	return ret;
}

int fimc_is_hw_mcsc_output_crop(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	u32 position_x, position_y;
	u32 crop_width, crop_height;
	u32 format, plane;
	bool conv420_flag = false;
	u32 y_stride, uv_stride;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;
	struct param_scaler_output_crop *output_crop;

	BUG_ON(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter.scalerp;
	output_crop = &(param->output_crop);

	position_x = output_crop->pos_x;
	position_y = output_crop->pos_y;
	crop_width = output_crop->crop_width;
	crop_height = output_crop->crop_height;
	format = output_crop->format;
	plane = param->dma_output.plane;

	if (output_crop->cmd != SCALER_CROP_COMMAND_ENABLE) {
		dbg_hw("[%d][ID:%d] Skip output crop\n", instance, hw_ip->id);
		return 0;
	}

	hw_mcsc->param_change_flags[MCSC_PARAM_OUTPUTCROP_CHANGE] = 0;

	if (format == DMA_OUTPUT_FORMAT_YUV420)
		conv420_flag = true;

	fimc_is_hw_mcsc_adjust_stride(crop_width, plane, conv420_flag,
		&y_stride, &uv_stride);
	fimc_is_scaler_set_stride_size(hw_ip->regs, y_stride, uv_stride);

	return ret;
}

int fimc_is_hw_mcsc_flip(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;
	struct param_scaler_flip *flip;

	BUG_ON(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter.scalerp;
	flip = &(param->flip);

	if (flip->cmd > SCALER_FLIP_COMMAND_XY_MIRROR) {
		err_hw("[%d]Invalid MCSC flip cmd(%d)", instance, flip->cmd);
		return -EINVAL;
	}

	hw_mcsc->param_change_flags[MCSC_PARAM_FLIP_CHANGE] = 0;

	fimc_is_scaler_set_flip_mode(hw_ip->regs, flip->cmd);

	return ret;
}

int fimc_is_hw_mcsc_otf_output(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	u32 input_width, input_height;
	u32 crop_width, crop_height;
	u32 output_width, output_height;
	u32 dst_width, dst_height;
	u32 format, bit_witdh;
	u32 post_hratio, post_vratio;
	u32 hratio, vratio;
	ulong temp_width, temp_height;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;
	struct param_otf_output *otf_output;

	BUG_ON(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter.scalerp;
	otf_output = &(param->otf_output);

	dst_width = otf_output->width;
	dst_height = otf_output->height;
	format = otf_output->format;
	bit_witdh = otf_output->bitwidth;

	hw_mcsc->param_change_flags[MCSC_PARAM_OTFOUTPUT_CHANGE] = 0;

	if (dst_width < 16 || dst_width > 8192) {
		err_hw("[%d]Invalid MCSC output width(%d)", instance, dst_width);
		return -EINVAL;
	}

	if (dst_height < 16 || dst_height > 8192) {
		err_hw("[%d]Invalid MCSC output height(%d)", instance, dst_height);
		return -EINVAL;
	}

	if (format != OTF_OUTPUT_FORMAT_YUV422) {
		err_hw("[%d]Invalid MCSC output format(%d)", instance, dst_height);
		return -EINVAL;
	}

	if (bit_witdh != OTF_OUTPUT_BIT_WIDTH_8BIT) {
		err_hw("[%d]Invalid MCSC output bit width(%d)", instance, dst_height);
		return -EINVAL;
	}

	fimc_is_scaler_set_post_dst_size(hw_ip->regs, dst_width, dst_height);

	if (param->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		fimc_is_scaler_get_poly_img_size(hw_ip->regs, &input_width, &input_height);
		fimc_is_scaler_get_poly_src_size(hw_ip->regs, &crop_width, &crop_height);
		fimc_is_scaler_get_post_img_size(hw_ip->regs, &output_width, &output_height);

		temp_width = (ulong)crop_width;
		temp_height = (ulong)crop_height;
		hratio =(u32)((temp_width << 20) / output_width);
		vratio =(u32)((temp_height << 20) / output_height);

		post_hratio = (output_width << 20) / dst_width;
		post_vratio = (output_height << 20) / dst_height;

		fimc_is_scaler_set_poly_scaling_ratio(hw_ip->regs, hratio, vratio);
		fimc_is_scaler_set_post_scaling_ratio(hw_ip->regs, post_hratio, post_vratio);
		fimc_is_scaler_set_poly_scaler_coef(hw_ip->regs, hratio, vratio);
	}

	if (otf_output->cmd == OTF_OUTPUT_COMMAND_ENABLE) {
		/* set direct out path : DIRECT_FORMAT : 00 :  CbCr first */
		fimc_is_scaler_set_direct_out_path(hw_ip->regs, 0, 1);
	} else {
		fimc_is_scaler_set_direct_out_path(hw_ip->regs, 0, 0);
	}

	return ret;
}

int fimc_is_hw_mcsc_dma_output(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	u32 input_width, input_height;
	u32 crop_width, crop_height;
	u32 output_width, output_height;
	u32 scaled_width, scaled_height;
	u32 dst_width, dst_height;
	u32 post_hratio, post_vratio;
	u32 hratio, vratio;
	u32 format, plane, order;
	u32 y_stride, uv_stride;
	u32 img_format;
	bool conv420_en = false;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;
	struct param_dma_output *dma_output;

	BUG_ON(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter.scalerp;
	dma_output = &(param->dma_output);

	dst_width = dma_output->width;
	dst_height = dma_output->height;
	format = dma_output->format;
	plane = dma_output->plane;
	order = dma_output->order;

	hw_mcsc->param_change_flags[MCSC_PARAM_DMAOUTPUT_CHANGE] = 0;

	if (dma_output->cmd == DMA_OUTPUT_UPDATE_MASK_BITS) {
		/* TODO : set DMA output frame buffer sequence */
	} else if (dma_output->cmd != DMA_OUTPUT_COMMAND_ENABLE) {
		dbg_hw("[%d][ID:%d] Skip dma out\n", instance, hw_ip->id);
		fimc_is_scaler_set_dma_out_path(hw_ip->regs, 0, 0);
		return 0;
	} else if (dma_output->cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		ret = fimc_is_hw_mcsc_adjust_img_fmt(format, plane, order,
				&img_format, &conv420_en);
		if (ret < 0) {
			warn_hw("[%d][ID:%d] Invalid dma image format\n", instance, hw_ip->id);
			img_format = hw_mcsc->img_format;
			conv420_en = hw_mcsc->conv420_en;
		} else {
			hw_mcsc->img_format = img_format;
			hw_mcsc->conv420_en = conv420_en;
		}

		fimc_is_scaler_set_dma_out_path(hw_ip->regs, img_format, 1);
		fimc_is_scaler_set_420_conversion(hw_ip->regs, 0, conv420_en);

		if (fimc_is_scaler_get_otf_out_enable(hw_ip->regs)) {
			fimc_is_scaler_get_post_dst_size(hw_ip->regs, &scaled_width, &scaled_height);

			if ((scaled_width != dst_width) || (scaled_height != dst_height)) {
				dbg_hw("[%d][ID:%d] Invalid scaled size (%d/%d)(%d/%d)\n",
					instance, hw_ip->id, scaled_width, scaled_height,
					dst_width, dst_height);
				return -EINVAL;
			}
		} else {
			fimc_is_scaler_get_poly_src_size(hw_ip->regs, &crop_width, &crop_height);
			fimc_is_scaler_get_post_img_size(hw_ip->regs, &output_width, &output_height);

			hratio = (crop_width << 20) / output_width;
			vratio = (crop_height << 20) / output_height;

			post_hratio = (output_width << 20) / dst_width;
			post_vratio = (output_height << 20) / dst_height;

			fimc_is_scaler_set_poly_scaling_ratio(hw_ip->regs, hratio, vratio);
			fimc_is_scaler_set_post_scaling_ratio(hw_ip->regs, post_hratio, post_vratio);
			fimc_is_scaler_set_poly_scaler_coef(hw_ip->regs, hratio, vratio);
		}

		if (param->output_crop.cmd != SCALER_CROP_COMMAND_ENABLE) {
			if (dma_output->reserved[0] == SCALER_DMA_OUT_UNSCALED) {
				fimc_is_scaler_get_poly_img_size(hw_ip->regs,
					&input_width, &input_height);
				fimc_is_hw_mcsc_adjust_stride(input_width, plane, conv420_en,
					&y_stride, &uv_stride);
				fimc_is_scaler_set_stride_size(hw_ip->regs, y_stride, uv_stride);
			} else {
				fimc_is_hw_mcsc_adjust_stride(dst_width, plane, conv420_en,
					&y_stride, &uv_stride);
				fimc_is_scaler_set_stride_size(hw_ip->regs, y_stride, uv_stride);
			}
		}
	}

	fimc_is_scaler_set_dma_size(hw_ip->regs, dst_width, dst_height);

	return ret;
}

int fimc_is_hw_mcsc_output_yuvrange(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	int yuv_range = 0;
	struct scp_param *param;
	struct param_scaler_imageeffect *image_effect;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	scaler_setfile_contents contents;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter.scalerp;
	image_effect = &(param->effect);
	yuv_range = image_effect->yuv_range;

	fimc_is_scaler_set_bchs_enable(hw_ip->regs, 1);

	if (test_bit(HW_TUNESET, &hw_ip->state)) {
		/* set yuv range config value by scaler_param yuv_range mode */
		contents = hw_mcsc->setfile->contents[yuv_range];
		fimc_is_scaler_set_b_c(hw_ip->regs, contents.y_offset,
							contents.y_gain);
		fimc_is_scaler_set_h_s(hw_ip->regs,
			contents.c_gain00, contents.c_gain01,
			contents.c_gain10, contents.c_gain11);
		dbg_hw("[%d][ID:%d] set YUV range(%d) by setfile parameter\n",
			instance, hw_ip->id, yuv_range);
	} else {
		if (yuv_range == SCALER_OUTPUT_YUV_RANGE_FULL) {
			/* Y range - [0:255], U/V range - [0:255] */
			fimc_is_scaler_set_b_c(hw_ip->regs, 0, 256);
			fimc_is_scaler_set_h_s(hw_ip->regs, 256, 0, 0, 256);
		} else if (yuv_range == SCALER_OUTPUT_YUV_RANGE_NARROW) {
			/* Y range - [16:235], U/V range - [16:239] */
			fimc_is_scaler_set_b_c(hw_ip->regs, 16, 220);
			fimc_is_scaler_set_h_s(hw_ip->regs, 224, 0, 0, 224);
		}
		dbg_hw("[%d][ID:%d] YUV range set default settings\n", instance,
			hw_ip->id);
	}

	return ret;
}

int fimc_is_hw_mcsc_adjust_img_fmt(u32 format, u32 plane, u32 order, u32 *img_format,
	bool *conv420_flag)
{
	int ret = 0;

	switch (format) {
	case DMA_OUTPUT_FORMAT_YUV420:
		*conv420_flag = true;
		switch (plane) {
		case 2:
			switch (order) {
			case DMA_OUTPUT_ORDER_CbCr:
				*img_format = MCSC_YUV420_2P_UFIRST;
				break;
			case DMA_OUTPUT_ORDER_CrCb:
				* img_format = MCSC_YUV420_2P_VFIRST;
				break;
			default:
				err_hw("output order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 3:
			*img_format = MCSC_YUV420_3P;
			break;
		default:
			err_hw("output plane error - (%d/%d/%d)", format, order, plane);
			ret = -EINVAL;
			break;
		}
		break;
	case DMA_OUTPUT_FORMAT_YUV422:
		*conv420_flag = false;
		switch (plane) {
		case 1:
			switch (order) {
			case DMA_OUTPUT_ORDER_CrYCbY:
				*img_format = MCSC_YUV422_1P_VYUY;
				break;
			case DMA_OUTPUT_ORDER_CbYCrY:
				*img_format = MCSC_YUV422_1P_UYVY;
				break;
			case DMA_OUTPUT_ORDER_YCrYCb:
				*img_format = MCSC_YUV422_1P_YVYU;
				break;
			case DMA_OUTPUT_ORDER_YCbYCr:
				*img_format = MCSC_YUV422_1P_YUYV;
				break;
			default:
				err_hw("output order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 2:
			switch (order) {
			case DMA_OUTPUT_ORDER_CbCr:
				*img_format = MCSC_YUV422_2P_UFIRST;
				break;
			case DMA_OUTPUT_ORDER_CrCb:
				*img_format = MCSC_YUV422_2P_VFIRST;
				break;
			default:
				err_hw("output order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 3:
			*img_format = MCSC_YUV422_3P;
			break;
		default:
			err_hw("output plane error - (%d/%d/%d)", format, order, plane);
			ret = -EINVAL;
			break;
		}
		break;
	default:
		err_hw("output format error - (%d/%d/%d)", format, order, plane);
		ret = -EINVAL;
		break;
	}

	return ret;
}

void fimc_is_hw_mcsc_adjust_stride(u32 width, u32 plane, bool conv420_flag,
	u32 *y_stride, u32 *uv_stride)
{
	if ((conv420_flag == false) && (plane == 1)) {
		*y_stride = width * 2;
		*uv_stride = width;
	} else {
		*y_stride = width;
		if (plane == 3)
			*uv_stride = width / 2;
		else
			*uv_stride = width;
	}

	*y_stride = 2 * ((*y_stride / 2) + ((*y_stride % 2) > 0));
	*uv_stride = 2 * ((*uv_stride / 2) + ((*uv_stride % 2) > 0));
}

void fimc_is_hw_mcsc_size_dump(struct fimc_is_hw_ip *hw_ip)
{
	u32 otf_path, dma_path = 0;
	u32 poly_in_w, poly_in_h = 0;
	u32 poly_crop_w, poly_crop_h = 0;
	u32 poly_out_w, poly_out_h = 0;
	u32 post_in_w, post_in_h = 0;
	u32 post_out_w, post_out_h = 0;
	u32 dma_y_stride, dma_uv_stride = 0;

	otf_path = fimc_is_scaler_get_otf_out_enable(hw_ip->regs);
	dma_path = fimc_is_scaler_get_dma_enable(hw_ip->regs);

	fimc_is_scaler_get_poly_img_size(hw_ip->regs, &poly_in_w, &poly_in_h);
	fimc_is_scaler_get_poly_src_size(hw_ip->regs, &poly_crop_w, &poly_crop_h);
	fimc_is_scaler_get_poly_dst_size(hw_ip->regs, &poly_out_w, &poly_out_h);
	fimc_is_scaler_get_post_img_size(hw_ip->regs, &post_in_w, &post_in_h);
	fimc_is_scaler_get_post_dst_size(hw_ip->regs, &post_out_w, &post_out_h);
	fimc_is_scaler_get_stride_size(hw_ip->regs, &dma_y_stride, &dma_uv_stride);

	info_hw("[MCSC]=SIZE=====================================\n");
	info_hw("[OTF:%d], [DMA:%d]\n", otf_path, dma_path);
	info_hw("[POLY] IN:%dx%d, CROP:%dx%d, OUT:%dx%d\n",
		poly_in_w, poly_in_h, poly_crop_w, poly_crop_h, poly_out_w, poly_out_h);
	info_hw("[POST] IN:%dx%d, OUT:%dx%d\n", post_in_w, post_in_h, post_out_w, post_out_h);
	info_hw("[DMA_STRIDE] Y:%d, UV:%d\n", dma_y_stride, dma_uv_stride);
	info_hw("[MCSC]==========================================\n");

	return;
}

