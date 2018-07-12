/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-3aa.h"
#include "fimc-is-hw-isp.h"

extern struct fimc_is_lib_support gPtr_lib_support;

const struct fimc_is_hw_ip_ops fimc_is_hw_isp_ops = {
	.open			= fimc_is_hw_isp_open,
	.init			= fimc_is_hw_isp_init,
	.close			= fimc_is_hw_isp_close,
	.enable			= fimc_is_hw_isp_enable,
	.disable		= fimc_is_hw_isp_disable,
	.shot			= fimc_is_hw_isp_shot,
	.set_param		= fimc_is_hw_isp_set_param,
	.frame_ndone		= fimc_is_hw_isp_frame_ndone,
	.load_setfile		= fimc_is_hw_isp_load_setfile,
	.apply_setfile		= fimc_is_hw_isp_apply_setfile,
	.delete_setfile		= fimc_is_hw_isp_delete_setfile
};

int fimc_is_hw_isp_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc, int id)
{
	int ret = 0;

	BUG_ON(!hw_ip);
	BUG_ON(!itf);
	BUG_ON(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	hw_ip->ops  = &fimc_is_hw_isp_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->Vvalid, 0);
	atomic_set(&hw_ip->ref_count, 0);
	init_waitqueue_head(&hw_ip->wait_queue);

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	info_hw("[ID:%2d] probe done\n", id);

	return ret;
}

int fimc_is_hw_isp_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	*size = sizeof(struct fimc_is_hw_isp);

	frame_manager_probe(hw_ip->framemgr, FRAMEMGR_ID_HW | hw_ip->id);
	frame_manager_open(hw_ip->framemgr, FIMC_IS_MAX_HW_FRAME);

	frame_manager_probe(hw_ip->framemgr_late, FRAMEMGR_ID_HW | hw_ip->id | 0xF0);
	frame_manager_open(hw_ip->framemgr_late, FIMC_IS_MAX_HW_FRAME_LATE);

	return ret;
}

int fimc_is_hw_isp_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
	bool flag, u32 module_id)
{
	int ret = 0;
	u32 instance = 0;
	struct fimc_is_hw_isp *hw_isp = NULL;

	BUG_ON(!hw_ip);
	BUG_ON(!hw_ip->priv_info);
	BUG_ON(!group);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	instance = group->instance;
	hw_ip->group[instance] = group;

	if (hw_isp->lib_func == NULL) {
		ret = get_lib_func(LIB_FUNC_ISP, (void **)&hw_isp->lib_func);
		dbg_hw("lib_interface_func is set (%d)\n", hw_ip->id);
	}

	if (hw_isp->lib_func == NULL) {
		err_hw("func_isp(null) (%d)", hw_ip->id);
		fimc_is_load_clear();
		return -EINVAL;
	}

	hw_isp->lib_support = &gPtr_lib_support;
	hw_isp->lib[instance].object = NULL;
	hw_isp->lib[instance].func   = hw_isp->lib_func;
	hw_isp->param_set[instance].reprocessing = flag;

	if (test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d]chain is already created (%d)\n", instance, hw_ip->id);
	} else {
		ret = fimc_is_lib_isp_chain_create(hw_ip, &hw_isp->lib[instance],
				instance);
		if (ret) {
			err_hw("[%d]chain create fail (%d)", instance, hw_ip->id);
			return -EINVAL;
		}
	}

	if (hw_isp->lib[instance].object != NULL) {
		dbg_hw("[%d]object is already created (%d)\n", instance, hw_ip->id);
	} else {
		ret = fimc_is_lib_isp_object_create(hw_ip, &hw_isp->lib[instance],
				instance, (u32)flag, module_id);
		if (ret) {
			err_hw("[%d]object create fail (%d)", instance, hw_ip->id);
			return -EINVAL;
		}
	}

	return ret;
}

int fimc_is_hw_isp_object_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_isp *hw_isp;

	BUG_ON(!hw_ip);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;

	fimc_is_lib_isp_object_destroy(hw_ip, &hw_isp->lib[instance], instance);
        hw_isp->lib[instance].object = NULL;

	return ret;
}

int fimc_is_hw_isp_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_hw_isp *hw_isp;

	BUG_ON(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;

	/* TODO: check 3aa */
	for (i = 0; i < FIMC_IS_MAX_TASK_WORKER; i++) {
		if (hw_isp->lib_support->task_taaisp[i].task == NULL) {
			err_hw("task is null");
		} else {
			if (hw_isp->lib_support->task_taaisp[i].task->state <= 0) {
				warn_hw("kthread state(%ld), exit_code(%d), exit_state(%d)\n",
					hw_isp->lib_support->task_taaisp[i].task->state,
					hw_isp->lib_support->task_taaisp[i].task->exit_code,
					hw_isp->lib_support->task_taaisp[i].task->exit_state);

				ret = kthread_stop(hw_isp->lib_support->task_taaisp[i].task);
				if (ret)
					err_hw("kthread_stop fail (%d)", ret);
			}
		}
	}

	fimc_is_lib_isp_chain_destroy(hw_ip, &hw_isp->lib[instance], instance);
	frame_manager_close(hw_ip->framemgr);
	frame_manager_close(hw_ip->framemgr_late);

	info_hw("[%d]close (%d)(%d)\n", instance, hw_ip->id, atomic_read(&hw_ip->ref_count));

	return ret;
}

int fimc_is_hw_isp_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
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

int fimc_is_hw_isp_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 timetowait;
	struct fimc_is_hw_isp *hw_isp;
	struct isp_param_set *param_set;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	info_hw("[%d][ID:%d]stream_off \n", instance, hw_ip->id);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	param_set = &hw_isp->param_set[instance];

	timetowait = wait_event_timeout(hw_ip->wait_queue,
		!atomic_read(&hw_ip->Vvalid),
		FIMC_IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		err_hw("[%d][ID:%d] wait FRAME_END timeout (%u)", instance,
			hw_ip->id, timetowait);
		ret = -ETIME;
	}

	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		fimc_is_lib_isp_stop(hw_ip, &hw_isp->lib[instance], instance);
		clear_bit(HW_RUN, &hw_ip->state);
	} else {
		dbg_hw("[%d]already disabled (%d)\n", instance, hw_ip->id);
	}

	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

int fimc_is_hw_isp_shot(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	u32 lindex, hindex;
	struct fimc_is_hw_isp *hw_isp;
	struct isp_param_set *param_set;
	struct is_region *region;
	struct isp_param *param;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	dbg_hw("[%d][ID:%d]shot [F:%d]\n", frame->instance, hw_ip->id, frame->fcount);

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("not initialized!! (%d)\n", hw_ip->id);
		return -EINVAL;
	}

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	set_bit(hw_ip->id, &frame->core_flag);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	param_set = &hw_isp->param_set[frame->instance];
	region = hw_ip->region[frame->instance];
	param = &region->parameter.isp;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		/* OTF INPUT case */
		param_set->dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->input_dva = frame->dvaddr_buffer[0];
		param_set->dma_output_chunk.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_chunk = 0x0;
		param_set->dma_output_yuv.cmd  = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_yuv = 0x0;
		goto config;
	} else {
		/* per-frame control
		* check & update size from region */
		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;

		if (hw_ip->internal_fcount != 0) {
			hw_ip->internal_fcount = 0;

			if (param->otf_input.cmd != param_set->otf_input.cmd)
				lindex |= LOWBIT_OF(PARAM_ISP_OTF_INPUT);
			if (param->vdma1_input.cmd != param_set->dma_input.cmd)
				lindex |= LOWBIT_OF(PARAM_ISP_VDMA1_INPUT);
			if (param->otf_output.cmd != param_set->otf_output.cmd)
				lindex |= LOWBIT_OF(PARAM_ISP_OTF_OUTPUT);
			if (param->vdma4_output.cmd != param_set->dma_output_chunk.cmd)
				lindex |= LOWBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
			if (param->vdma5_output.cmd != param_set->dma_output_yuv.cmd)
				lindex |= LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
		}
	}

	fimc_is_hw_isp_update_param(hw_ip, region, param_set, lindex, hindex,
		frame->instance);

	/* DMA settings */
	param_set->input_dva = 0;
	param_set->output_dva_chunk = 0;
	param_set->output_dva_yuv = 0;

	if (param_set->dma_input.cmd != DMA_INPUT_COMMAND_DISABLE) {
		param_set->input_dva = frame->dvaddr_buffer[0];
		if (frame->dvaddr_buffer[0] == 0) {
			info_hw("[F:%d]dvaddr_buffer[0] is zero", frame->fcount);
			BUG_ON(1);
		}
	}

	if (param_set->dma_output_chunk.cmd != DMA_OUTPUT_COMMAND_DISABLE) {
		param_set->output_dva_chunk = frame->shot->uctl.scalerUd.ixpTargetAddress[0];
		if (frame->shot->uctl.scalerUd.ixpTargetAddress[0] == 0) {
			info_hw("[F:%d]ixpTargetAddress[0] is zero", frame->fcount);
			param_set->dma_output_chunk.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}
	}

	if (param_set->dma_output_yuv.cmd != DMA_OUTPUT_COMMAND_DISABLE) {
		param_set->output_dva_yuv = frame->shot->uctl.scalerUd.ixcTargetAddress[0];
		if (frame->shot->uctl.scalerUd.ixcTargetAddress[0] == 0) {
			info_hw("[F:%d]ixcTargetAddress[0] is zero", frame->fcount);
			param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}
	}

config:
	param_set->instance_id = frame->instance;
	param_set->fcount = frame->fcount;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		fimc_is_log_write("[%d]isp_shot [T:%d][R:%d][F:%d][IN:0x%x] [%d][OUT:0x%x]\n",
			param_set->instance_id, frame->type, param_set->reprocessing,
			param_set->fcount, param_set->input_dva,
			param_set->dma_output_yuv.cmd, param_set->output_dva_yuv);
	}

	if (frame->shot) {
		ret = fimc_is_lib_isp_set_ctrl(hw_ip, &hw_isp->lib[frame->instance], frame);
		if (ret)
			err_hw("[%d] set_ctrl fail", frame->instance);
	}

	if (param_set->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		struct fimc_is_hw_ip *hw_ip_3aa;
		struct fimc_is_hw_3aa *hw_3aa;
		enum fimc_is_hardware_id hw_id;
		int hw_slot;

		if (test_bit(DEV_HW_3AA0, &hw_map)) {
			hw_id = DEV_HW_3AA0;
		} else  if (test_bit(DEV_HW_3AA1, &hw_map)) {
			hw_id = DEV_HW_3AA1;
		}

		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_ip_3aa = &hw_ip->hardware->hw_ip[hw_slot];
			hw_3aa = (struct fimc_is_hw_3aa *)hw_ip_3aa->priv_info;
			param_set->taa_param = &hw_3aa->param_set[frame->instance];
		}
	}

	fimc_is_lib_isp_shot(hw_ip, &hw_isp->lib[frame->instance], param_set, frame->shot);

	set_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

int fimc_is_hw_isp_set_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_isp *hw_isp;
	struct isp_param_set *param_set;

	BUG_ON(!hw_ip);
	BUG_ON(!region);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	param_set = &hw_isp->param_set[instance];

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("not initialized!! (%d)", hw_ip->id);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	fimc_is_hw_isp_update_param(hw_ip, region, param_set, lindex, hindex, instance);

	return ret;
}

void fimc_is_hw_isp_update_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	struct isp_param_set *param_set, u32 lindex, u32 hindex, u32 instance)
{
	struct isp_param *param;

	BUG_ON(!region);
	BUG_ON(!param_set);

	param = &region->parameter.isp;

	/* check input */
	if (lindex & LOWBIT_OF(PARAM_ISP_OTF_INPUT)) {
		memcpy(&param_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));
	}

	if (lindex & LOWBIT_OF(PARAM_ISP_VDMA1_INPUT)) {
		memcpy(&param_set->dma_input, &param->vdma1_input,
			sizeof(struct param_dma_input));
	}

	/* check output*/
	if (lindex & LOWBIT_OF(PARAM_ISP_OTF_OUTPUT)) {
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));
	}

	if (lindex & LOWBIT_OF(PARAM_ISP_VDMA4_OUTPUT)) {
		memcpy(&param_set->dma_output_chunk, &param->vdma4_output,
			sizeof(struct param_dma_output));
	}

	if (lindex & LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT)) {
		memcpy(&param_set->dma_output_yuv, &param->vdma5_output,
			sizeof(struct param_dma_output));
	}
}

int fimc_is_hw_isp_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	u32 instance, bool late_flag)
{
	int ret = 0;
	int wq_id_ixc, wq_id_ixp, output_id;
	enum fimc_is_frame_done_type done_type;

	BUG_ON(!hw_ip);

	switch (hw_ip->id) {
	case DEV_HW_ISP0:
		wq_id_ixc = WORK_I0C_FDONE;
		wq_id_ixp = WORK_I0P_FDONE;
		break;
	case DEV_HW_ISP1:
		wq_id_ixc = WORK_I1C_FDONE;
		wq_id_ixp = WORK_I1P_FDONE;
		break;
	default:
		err_hw("invalid hw (%d)", hw_ip->id);
		return -EINVAL;
		break;
	}

	if (late_flag == true)
		done_type = FRAME_DONE_LATE_SHOT;
	else
		done_type = FRAME_DONE_FORCE;

	output_id = ENTRY_IXC;
	if (test_bit(output_id, &frame->out_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id_ixc,
				output_id, 1, done_type);

	output_id = ENTRY_IXP;
	if (test_bit(output_id, &frame->out_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id_ixp,
				output_id, 1, done_type);

	output_id = FIMC_IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, -1,
				output_id, 1, done_type);

	return ret;
}

int fimc_is_hw_isp_load_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map)
{
	int flag = 0, ret = 0;
	ulong addr;
	u32 size;
	struct fimc_is_hw_isp *hw_isp = NULL;

	BUG_ON(!hw_ip);

	if (test_bit(DEV_HW_3AA0, &hw_map) || test_bit(DEV_HW_3AA1, &hw_map))
		return 0;

	if (!test_bit(hw_ip->id, &hw_map)) {
		dbg_hw("[%d]%s: hw_map(0x%lx)\n", instance, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		return -ESRCH;
	}

	switch (hw_ip->setfile_info.version) {
	case SETFILE_V2:
		flag = false;
		break;
	case SETFILE_V3:
		flag = true;
		break;
	default:
		err_hw("[%d][ID:%d] invalid version (%d)", instance, hw_ip->id,
			hw_ip->setfile_info.version);
		return -EINVAL;
		break;
	}

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	addr = hw_ip->setfile_info.table[index].addr;
	size = hw_ip->setfile_info.table[index].size;
	ret = fimc_is_lib_isp_create_tune_set(&hw_isp->lib[instance],
		addr, size, (u32)index, flag, instance);

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int fimc_is_hw_isp_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 setfile_index = 0;
	struct fimc_is_hw_isp *hw_isp = NULL;

	BUG_ON(!hw_ip);

	if (test_bit(DEV_HW_3AA0, &hw_map) || test_bit(DEV_HW_3AA1, &hw_map))
		return 0;

	if (!test_bit(hw_ip->id, &hw_map)) {
		dbg_hw("[%d]%s: hw_map(0x%lx)\n", instance, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		return -ESRCH;
	}

	if (hw_ip->setfile_info.num == 0)
		return 0;

	setfile_index = hw_ip->setfile_info.index[index];
	info_hw("[%d][ID:%d] setfile (%d) scenario (%d)\n", instance, hw_ip->id,
		setfile_index, index);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	ret = fimc_is_lib_isp_apply_tune_set(&hw_isp->lib[instance], setfile_index, instance);

	return ret;
}

int fimc_is_hw_isp_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct fimc_is_hw_isp *hw_isp = NULL;
	int i, ret = 0;

	BUG_ON(!hw_ip);

	if (test_bit(DEV_HW_3AA0, &hw_map) || test_bit(DEV_HW_3AA1, &hw_map))
		return 0;

	if (!test_bit(hw_ip->id, &hw_map)) {
		dbg_hw("[%d]%s: hw_map(0x%lx)\n", instance, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] Not initialized\n", instance, hw_ip->id);
		return 0;
	}

	if (hw_ip->setfile_info.num == 0)
		return 0;

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	for (i = 0; i < hw_ip->setfile_info.num; i++)
		ret = fimc_is_lib_isp_delete_tune_set(&hw_isp->lib[instance],
				(u32)i, instance);

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}
