/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-tpu.h"
#include "fimc-is-regs.h"
#include "fimc-is-param.h"

const struct fimc_is_hw_ip_ops fimc_is_hw_tpu_ops = {
	.open			= fimc_is_hw_tpu_open,
	.init			= fimc_is_hw_tpu_init,
	.close			= fimc_is_hw_tpu_close,
	.enable			= fimc_is_hw_tpu_enable,
	.disable		= fimc_is_hw_tpu_disable,
	.shot			= fimc_is_hw_tpu_shot,
	.set_param		= fimc_is_hw_tpu_set_param,
	.frame_ndone		= fimc_is_hw_tpu_frame_ndone,
	.load_setfile		= fimc_is_hw_tpu_load_setfile,
	.apply_setfile		= fimc_is_hw_tpu_apply_setfile,
	.delete_setfile		= fimc_is_hw_tpu_delete_setfile,
	.size_dump		= fimc_is_hw_tpu_size_dump
};

int fimc_is_hw_tpu_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc, int id)
{
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!hw_ip);
	BUG_ON(!itf);
	BUG_ON(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	hw_ip->ops  = &fimc_is_hw_tpu_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->Vvalid, 0);
	atomic_set(&hw_ip->otf_start, 0);
	atomic_set(&hw_ip->ref_count, 0);
	init_waitqueue_head(&hw_ip->wait_queue);

	hw_slot = fimc_is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid hw_slot (%d,%d)", id, hw_slot);
		return 0;
	}

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	info_hw("[ID:%2d] probe done\n", id);

	return ret;
}

int fimc_is_hw_tpu_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size)
{
	BUG_ON(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	*size = 0;

	return 0;
}

int fimc_is_hw_tpu_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
	bool flag, u32 module_id)
{
	u32 instance = 0;
	int ret = 0;

	BUG_ON(!hw_ip);

	instance = group->instance;
	hw_ip->group[instance] = group;

	return ret;
}

int fimc_is_hw_tpu_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	info_hw("[%d]close (%d)(%d)\n", instance, hw_ip->id, atomic_read(&hw_ip->ref_count));

	return ret;
}

int fimc_is_hw_tpu_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	ulong hw_map)
{
	int ret = 0;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] not initialized\n", frame->instance, hw_ip->id);
		return -EINVAL;
	}

	set_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

int fimc_is_hw_tpu_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d][ID:%d] not initialized\n", instance, hw_ip->id);
		return -EINVAL;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		/* enable tpu */
		set_bit(HW_RUN, &hw_ip->state);
	}

	return ret;
}

int fimc_is_hw_tpu_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	info_hw("[%d][ID:%d]stream_off \n", instance, hw_ip->id);

	if (test_bit(HW_RUN, &hw_ip->state)) {
		/* disable tpu */
		clear_bit(HW_RUN, &hw_ip->state);
	} else {
		dbg_hw("[%d]already disabled (%d)\n", instance, hw_ip->id);
	}

	return ret;
}

int fimc_is_hw_tpu_set_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
{
	struct tpu_param *tpu_param = NULL;
	int ret = 0;

	BUG_ON(!hw_ip);
	BUG_ON(!region);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] not initialized\n", instance, hw_ip->id);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	/* set full-bypass */
	tpu_param = &(hw_ip->region[instance]->parameter.tpu);

	if (tpu_param->control.cmd == CONTROL_COMMAND_STOP) {
		if (tpu_param->control.bypass == CONTROL_BYPASS_ENABLE) {
			fimc_is_hw_set_fullbypass(hw_ip->itfc, hw_ip->id, true);
		} else {
			fimc_is_hw_set_fullbypass(hw_ip->itfc, hw_ip->id, false);
		}
	} else if (tpu_param->control.cmd == CONTROL_COMMAND_START) {
		fimc_is_hw_set_fullbypass(hw_ip->itfc, hw_ip->id, false);
	}

	return ret;
}

int fimc_is_hw_tpu_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	u32 instance, bool late_flag)
{
	int ret = 0;

	return ret;
}

int fimc_is_hw_tpu_load_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map)
{
	int ret = 0;

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] Not initialized\n", instance, hw_ip->id);
		return -ESRCH;
	}

	return ret;
}

int fimc_is_hw_tpu_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map)
{
	u32 setfile_index = 0;
	int ret = 0;

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] Not initialized\n", instance, hw_ip->id);
		return -ESRCH;
	}

	if (hw_ip->setfile_info.num == 0)
		return 0;

	setfile_index = hw_ip->setfile_info.index[index];
	info_hw("[%d][ID:%d] setfile (%d) scenario (%d)\n",
		instance, hw_ip->id, setfile_index, index);

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int fimc_is_hw_tpu_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance,
	ulong hw_map)
{
	int ret = 0;

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (hw_ip->setfile_info.num == 0)
		return 0;

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

void fimc_is_hw_tpu_size_dump(struct fimc_is_hw_ip *hw_ip)
{
	return;
}

