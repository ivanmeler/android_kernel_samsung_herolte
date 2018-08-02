/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-vra.h"
#include "../interface/fimc-is-interface-ischain.h"

static int fimc_is_hw_vra_ch0_handle_interrupt(u32 id, void *context)
{
	int ret = 0;
	u32 intr_status = 0;
	struct fimc_is_hw_ip *hw_ip = NULL;
	struct fimc_is_hw_vra *hw_vra = NULL;

	BUG_ON(!context);
	hw_ip = (struct fimc_is_hw_ip *)context;

	BUG_ON(!hw_ip->priv_info);
	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		err_hw("%s: hw_vra is null", __func__);
		return -EINVAL;
	}

	intr_status = fimc_is_vra_chain0_get_status_intr(hw_ip->regs);
	dbg_hw("%s: interrupt ID(%d), status(%d), enable bit (%d)\n",
		__func__, id, intr_status,
		fimc_is_vra_chain0_get_enable_intr(hw_ip->regs));

	ret = fimc_is_lib_vra_handle_interrupt(&hw_vra->lib_vra, id);
	if (ret) {
		err_hw("%s: error (%d)", __func__, ret);
		return ret;
	}

	if (intr_status & (1 << CH0INT_END_FRAME)) {
		dbg_hw("%s: CH0INT_END_FRAME\n", __func__);

		hw_ip->fe_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.E\n", hw_ip->id, hw_ip->fcount);

		atomic_set(&hw_ip->Vvalid, V_BLANK);

		/* ToDo: Consider DMA input mode */

		if (atomic_read(&hw_ip->fs_count) == atomic_read(&hw_ip->fe_count)) {
			err_hw("[VRA] fs(%d), fe(%d), cl(%d)",
				atomic_read(&hw_ip->fs_count),
				atomic_read(&hw_ip->fe_count),
				atomic_read(&hw_ip->cl_count));
		}
		atomic_inc(&hw_ip->fe_count);

		fimc_is_hardware_frame_done(hw_ip, NULL, -1,
			FIMC_IS_HW_CORE_END, 0, FRAME_DONE_NORMAL);
	}

	if (intr_status & (1 << CH0INT_CIN_FR_ST)) {
		dbg_hw("%s: CH0INT_CIN_FR_ST\n", __func__);

		hw_ip->fs_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.S\n", hw_ip->id, hw_ip->fcount);

		atomic_set(&hw_ip->Vvalid, V_VALID);
		clear_bit(HW_CONFIG, &hw_ip->state);
		atomic_inc(&hw_ip->fs_count);
	}

	return ret;
}

static int fimc_is_hw_vra_ch1_handle_interrupt(u32 id, void *context)
{
	int ret = 0;
	u32 intr_status = 0;
	struct fimc_is_hw_ip *hw_ip = NULL;
	struct fimc_is_hw_vra *hw_vra = NULL;

	BUG_ON(!context);
	hw_ip = (struct fimc_is_hw_ip *)context;

	BUG_ON(!hw_ip->priv_info);
	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		err_hw("%s: hw_vra is null", __func__);
		return -EINVAL;
	}

	intr_status = fimc_is_vra_chain1_get_status_intr(hw_ip->regs);
	dbg_hw("%s: interrupt ID(%d), status(%d), enable bit (%d)\n",
		__func__, id, intr_status,
		fimc_is_vra_chain1_get_enable_intr(hw_ip->regs));


	ret = fimc_is_lib_vra_handle_interrupt(&hw_vra->lib_vra, id);
	if (ret) {
		err_hw("%s: error (%d)", __func__, ret);
		return ret;
	}

	if (intr_status & (1 << CH1INT_IN_END_OF_CONTEXT))
		dbg_hw("%s: CH1INT_IN_END_OF_CONTEXT\n", __func__);

	return ret;
}


const struct fimc_is_hw_ip_ops fimc_is_hw_vra_ops = {
	.open			= fimc_is_hw_vra_open,
	.init			= fimc_is_hw_vra_init,
	.close			= fimc_is_hw_vra_close,
	.enable			= fimc_is_hw_vra_enable,
	.disable		= fimc_is_hw_vra_disable,
	.shot			= fimc_is_hw_vra_shot,
	.set_param		= fimc_is_hw_vra_set_param,
	.get_meta		= fimc_is_hw_vra_get_meta,
	.frame_ndone		= fimc_is_hw_vra_frame_ndone,
	.load_setfile		= fimc_is_hw_vra_load_setfile,
	.apply_setfile		= fimc_is_hw_vra_apply_setfile,
	.delete_setfile		= fimc_is_hw_vra_delete_setfile
};

int fimc_is_hw_vra_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc, int id)
{
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!hw_ip);
	BUG_ON(!itf);
	BUG_ON(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	hw_ip->ops  = &fimc_is_hw_vra_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;
	hw_ip->is_leader = false;
	atomic_set(&hw_ip->Vvalid, 0);
	atomic_set(&hw_ip->ref_count, 0);
	init_waitqueue_head(&hw_ip->wait_queue);

	/* set fd sfr base address */
	hw_slot = fimc_is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid hw_slot (%d,%d)", id, hw_slot);
		return 0;
	}

	/* set vra interrupt handler */
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &fimc_is_hw_vra_ch0_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &fimc_is_hw_vra_ch1_handle_interrupt;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	info_hw("[ID:%2d] probe done\n", id);

	return ret;
}

int fimc_is_hw_vra_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size)
{
	int ret = 0;
	struct fimc_is_lib_vra_os_system_funcs os_system_funcs;

	BUG_ON(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	*size = sizeof(struct fimc_is_hw_vra);

	fimc_is_lib_vra_os_funcs(&os_system_funcs);

	return ret;
}

int fimc_is_hw_vra_init(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *group, bool flag, u32 module_id)
{
	u32 instance = 0;
	int ret = 0;
	ulong dma_addr;
	struct fimc_is_hw_vra *hw_vra = NULL;
	struct fimc_is_subdev *leader;
	enum fimc_is_lib_vra_input_type input_type;

	BUG_ON(!hw_ip);
	BUG_ON(!hw_ip->priv_info);
	BUG_ON(!group);

	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;
	instance = group->instance;
	hw_ip->group[instance] = group;

	if (test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d]chain is already created (%d)\n", instance, hw_ip->id);
		return ret;
	}

	fimc_is_hw_vra_reset(hw_ip);

	ret = fimc_is_lib_vra_get_interface_funcs(&hw_vra->lib_vra);
	if (ret) {
		err_hw("[%d]library API get is fail (%d)", instance, hw_ip->id);
		fimc_is_load_vra_clear();
		return ret;
	}

	dma_addr = hw_ip->group[instance]->device->imemory.kvaddr_vra;
	ret = fimc_is_lib_vra_alloc_memory(&hw_vra->lib_vra, dma_addr);
	if (ret) {
		err_hw("[%d]alloc memory is fail (%d)", instance, hw_ip->id);
		return ret;
	}

	leader = &hw_ip->group[instance]->leader;
	if (leader->id == ENTRY_VRA)
		input_type = VRA_INPUT_MEMORY;
	else
		input_type = VRA_INPUT_OTF;

	ret = fimc_is_lib_vra_create_object(&hw_vra->lib_vra, hw_ip->regs,
		input_type, instance);
	if (ret) {
		err_hw("[%d]library init is fail (%d)", instance, hw_ip->id);
		return ret;
	}

	ret = fimc_is_lib_vra_init_task(&hw_vra->lib_vra);
	if (ret) {
		err_hw("VRA init task is fail (%d)", ret);
		return ret;
	}

	return 0;
}

int fimc_is_hw_vra_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_vra *hw_vra = NULL;

	BUG_ON(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		err_hw("fimc_is_hw_vra is is NULL");
		return -EINVAL;
	}

	ret = fimc_is_lib_vra_destory_object(&hw_vra->lib_vra, instance);
	if (ret) {
		err_hw("%s: fail (%d)", __func__, ret);
		return ret;
	}

	ret = fimc_is_lib_vra_free_memory(&hw_vra->lib_vra);
	if (ret) {
		err_hw("[%d]free memory is fail (%d)", instance, hw_ip->id);
		return ret;
	}

	info_hw("[%d]close (%d)(%d)\n", instance, hw_ip->id,
		atomic_read(&hw_ip->ref_count));

	return ret;
}

int fimc_is_hw_vra_enable(struct fimc_is_hw_ip *hw_ip, u32 instance,
	ulong hw_map)
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

int fimc_is_hw_vra_disable(struct fimc_is_hw_ip *hw_ip, u32 instance,
	ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_vra *hw_vra = NULL;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	info_hw("[%d][ID:%d]stream_off \n", instance, hw_ip->id);

	if (test_bit(HW_RUN, &hw_ip->state)) {
		hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;
		if (unlikely(!hw_vra)) {
			err_hw("fimc_is_hw_vra is is NULL");
			return -EINVAL;
		}

		ret = fimc_is_lib_vra_stop(&hw_vra->lib_vra);
		if (ret) {
			err_hw("%s: fail (%d)", __func__, ret);
			return ret;
		}

		fimc_is_hw_vra_reset(hw_ip);

		clear_bit(HW_RUN, &hw_ip->state);
	} else {
		dbg_hw("[%d]already disabled (%d)\n", instance, hw_ip->id);
	}

	return 0;
}

int fimc_is_hw_vra_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame, ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_vra *hw_vra = NULL;
	struct vra_param *param;
	struct camera2_uctl *uctl;
	unsigned char *buffer = NULL;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	dbg_hw("[%d][ID:%d][F:%d]\n", frame->instance, hw_ip->id,
		frame->fcount);

	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] hw_vra not initialized\n", frame->instance,
			hw_ip->id);
		return -EINVAL;
	}

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	param = (struct vra_param *)&hw_ip->region[frame->instance]->parameter.vra;
	if (param->control.cmd == CONTROL_COMMAND_STOP) {
		dbg_hw("VRA CONTROL_COMMAND_STOP.\n");
		ret = fimc_is_lib_vra_stop(&hw_vra->lib_vra);
		if (ret) {
			err_hw("VRA abort fail (%d)", ret);
			return ret;
		}
		return 0;
	}

	set_bit(hw_ip->id, &frame->core_flag);

	if (frame->type == SHOT_TYPE_INTERNAL) {
		dbg_hw("[%d][ID:%d] request not exist\n", frame->instance,
			hw_ip->id);
		goto internal;
	}

	if (unlikely(!frame->shot)) {
		err_hw("%s: shot is null", __func__);
		return -EINVAL;
	}

	uctl = &frame->shot->uctl;
	ret = fimc_is_lib_vra_set_orientation(&hw_vra->lib_vra,
		uctl->scalerUd.orientation, frame->instance);
	if (ret) {
		err_hw("VRA orientation set is fail (%d)", ret);
		return ret;
	}

	/*
	 * OTF mode: the buffer value is null.
	 * DMA mode: the buffer value is VRA input DMA address.
	 * ToDo: DMA input buffer set by buffer hiding
	 */
	ret = fimc_is_lib_vra_new_frame(&hw_vra->lib_vra, buffer,
		frame->instance);
	if (ret) {
		err_hw("VRA new frame set is fail (%d)", ret);
		return ret;
	}

internal:
	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;
}

int fimc_is_hw_vra_set_param(struct fimc_is_hw_ip *hw_ip,
	struct is_region *region, u32 lindex, u32 hindex, u32 instance,
	ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_vra *hw_vra;
	struct vra_param *param;

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

	param = &region->parameter.vra;
	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;

	ret = fimc_is_lib_vra_config_param(&hw_vra->lib_vra, param, instance);
	if (ret) {
		err_hw("[%d]library set param is fail (%d)", instance, hw_ip->id);
		return ret;
	}

	ret = fimc_is_lib_vra_set_param(&hw_vra->lib_vra, NULL,
			VRA_REAR_ORIENTATION, instance);
	if (ret) {
		err_hw("[%d]library set param is fail (%d)", instance, hw_ip->id);
		return ret;
	}

	return ret;
}

int fimc_is_hw_vra_frame_ndone(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame, u32 instance, bool late_flag)
{
	int ret = 0;
	int wq_id;
	int output_id;
	enum fimc_is_frame_done_type done_type;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	if (late_flag == true)
		done_type = FRAME_DONE_LATE_SHOT;
	else
		done_type = FRAME_DONE_FORCE;

	wq_id     = -1;
	output_id = FIMC_IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id, output_id, 1, done_type);

	return ret;
}

int fimc_is_hw_vra_load_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map)
{
	ulong addr;
	u32 size;
	struct fimc_is_hw_vra *hw_vra = NULL;

	BUG_ON(!hw_ip);

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
		break;
	case SETFILE_V3:
		break;
	default:
		err_hw("[%d][ID:%d] invalid version (%d)", instance, hw_ip->id,
				hw_ip->setfile_info.version);
		return -EINVAL;
		break;
	}

	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;
	addr = hw_ip->setfile_info.table[index].addr;
	size = hw_ip->setfile_info.table[index].size;
	set_bit(HW_TUNESET, &hw_ip->state);

	return 0;
}

int fimc_is_hw_vra_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index,
	u32 instance, ulong hw_map)
{
	struct fimc_is_setfile_info *setfile_info;
	struct fimc_is_hw_vra *hw_vra = NULL;
	struct fimc_is_lib_vra_tune_data tune;
	enum fimc_is_lib_vra_dir dir;
	u32 setfile_index = 0;
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map)) {
		dbg_hw("[%d]%s: hw_map(0x%lx)\n", instance, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		return -ESRCH;
	}

	setfile_info = &hw_ip->setfile_info;

	if (setfile_info->num == 0)
		return 0;

	setfile_index = setfile_info->index[index];
	info_hw("[%d][ID:%d] setfile (%d) scenario (%d)\n", instance, hw_ip->id,
			setfile_index, index);

	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		err_hw("fimc_is_hw_vra is is NULL");
		return -EINVAL;
	}

	hw_vra->setfile = *(struct fimc_is_hw_vra_setfile *)setfile_info->table[setfile_index].addr;

	dbg_hw("VRA apply setfile: version: %#x\n", hw_vra->setfile.setfile_version);

	if (hw_vra->setfile.setfile_version != VRA_SETFILE_VERSION)
		err_hw("VRA setfile version is wrong: expected virsion (%#x)",
				VRA_SETFILE_VERSION);

	tune.api_tune.tracking_mode = hw_vra->setfile.tracking_mode;
	tune.api_tune.enable_features = hw_vra->setfile.enable_features;
	tune.api_tune.min_face_size = hw_vra->setfile.min_face_size;
	tune.api_tune.max_face_count = hw_vra->setfile.max_face_count;
	tune.api_tune.face_priority = hw_vra->setfile.face_priority;
	tune.api_tune.disable_frontal_rot_mask =
		(hw_vra->setfile.limit_rotation_angles & 0xFF);

	if (hw_vra->setfile.disable_profile_detection)
		tune.api_tune.disable_profile_rot_mask =
			(hw_vra->setfile.limit_rotation_angles >> 8) | (0xFF);
	else
		tune.api_tune.disable_profile_rot_mask =
			(hw_vra->setfile.limit_rotation_angles >> 8) | (0xFE);

	tune.api_tune.working_point = hw_vra->setfile.boost_dr_vs_fpr;
	tune.api_tune.tracking_smoothness = hw_vra->setfile.tracking_smoothness;

	tune.frame_lock.lock_frame_num = hw_vra->setfile.lock_frame_number;
	tune.frame_lock.init_frames_per_lock = hw_vra->setfile.init_frames_per_lock;
	tune.frame_lock.normal_frames_per_lock = hw_vra->setfile.normal_frames_per_lock;

	dir = hw_vra->setfile.front_orientation;

	ret = fimc_is_lib_vra_set_param(&hw_vra->lib_vra, &tune, dir, instance);
	if (ret) {
		err_hw("VRA apply setfile is fail");
		return ret;
	}

	return 0;
}

int fimc_is_hw_vra_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance,
		ulong hw_map)
{
	clear_bit(HW_TUNESET, &hw_ip->state);

	return 0;
}

void fimc_is_hw_vra_reset(struct fimc_is_hw_ip *hw_ip)
{
	u32 all_intr;

	/* Interrupt clear */
	all_intr = fimc_is_vra_chain0_get_all_intr(hw_ip->regs);
	fimc_is_vra_chain0_set_clear_intr(hw_ip->regs, all_intr);

	all_intr = fimc_is_vra_chain1_get_all_intr(hw_ip->regs);
	fimc_is_vra_chain1_set_clear_intr(hw_ip->regs, all_intr);
}

int fimc_is_hw_vra_get_meta(struct fimc_is_hw_ip *hw_ip,
		struct fimc_is_frame *frame, unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_vra *hw_vra;

	if (unlikely(!frame)) {
		err_hw("%s: frame is null", __func__);
		return 0;
	}

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	hw_vra = (struct fimc_is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		err_hw("fimc_is_hw_vra is is NULL");
		return -EINVAL;
	}

	ret = fimc_is_lib_vra_get_meta(&hw_vra->lib_vra, frame->shot);
	if (ret)
		err_hw("[%d] get_meta fail", frame->instance);

	return ret;
}
