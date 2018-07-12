/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * exynos fimc-is2 device interface functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-interface-wrap.h"
#include "fimc-is-interface-library.h"
#include "fimc-is-param.h"

int fimc_is_itf_s_param_wrap(struct fimc_is_device_ischain *device,
	u32 lindex, u32 hindex, u32 indexes)
{
	struct fimc_is_hardware *hardware = NULL;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_set_param(hardware, instance, device->is_region,
		lindex, hindex, hardware->hw_map[instance]);
	if (ret) {
		merr("fimc_is_hardware_set_param is fail(%d)", device, ret);
		return ret;
	}

	return ret;
}

int fimc_is_itf_a_param_wrap(struct fimc_is_device_ischain *device, u32 group)
{
	struct fimc_is_hardware *hardware = NULL;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

#if !defined(SETFILE_DISABLE)
	ret = fimc_is_hardware_apply_setfile(hardware, instance,
		(group & GROUP_ID_PARM_MASK), device->setfile,
		hardware->hw_map[instance]);
	if (ret) {
		merr("fimc_is_hardware_apply_setfile is fail(%d)", device, ret);
		return ret;
	}
#endif
	return ret;
}

int fimc_is_itf_f_param_wrap(struct fimc_is_device_ischain *device, u32 group)
{
	struct fimc_is_hardware *hardware= NULL;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

#if !defined(SETFILE_DISABLE)
	ret = fimc_is_hardware_apply_setfile(hardware,
		instance,
		(group & GROUP_ID_PARM_MASK),
		device->setfile,
		hardware->hw_map[instance]);
	if (ret) {
		merr("fimc_is_hardware_apply_setfile is fail(%d)", device, ret);
		return ret;
	}
#endif
	return ret;
}

int fimc_is_itf_enum_wrap(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	return ret;
}

void fimc_is_itf_storefirm_wrap(struct fimc_is_device_ischain *device)
{
	dbg_hw("%s\n", __func__);

	return;
}

void fimc_is_itf_restorefirm_wrap(struct fimc_is_device_ischain *device)
{
	dbg_hw("%s\n", __func__);

	return;
}

int fimc_is_itf_set_fwboot_wrap(struct fimc_is_device_ischain *device, u32 val)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	return ret;
}

int fimc_is_itf_open_wrap(struct fimc_is_device_ischain *device, u32 module_id,
	u32 flag, u32 offset_path)
{
	struct fimc_is_hardware *hardware;
	struct fimc_is_path_info *path;
	struct fimc_is_group *group;
	struct is_region *region;
	u32 instance = 0;
	u32 hw_id = 0;
	u32 group_slot = -1;
	u32 group_id = -1;
	int ret = 0;
	int hw_list[GROUP_HW_MAX];
	int hw_index;
	int hw_maxnum = 0;
	u32 rsccount;

	info_hw("%s: offset_path(0x%8x) flag(%d) sen(%d)\n", __func__, offset_path, flag, module_id);

	instance = device->instance;
	hardware = device->hardware;
	path = (struct fimc_is_path_info *)&device->is_region->shared[offset_path];
	rsccount = atomic_read(&hardware->rsccount);

	region = device->is_region;
	region->shared[MAX_SHARED_COUNT-1] = MAGIC_NUMBER;

	for (hw_id = 0; hw_id < DEV_HW_END; hw_id++)
		clear_bit(hw_id, &hardware->hw_map[instance]);

	group_slot = GROUP_SLOT_3AA;
	group = &device->group_3aa;
	group_id = path->group[group_slot];
	dbg_hw("itf_open_wrap: group[SLOT_%d]=[%x]\n", group_slot, group_id);
	hw_maxnum = fimc_is_get_hw_list(group_id, hw_list);
	for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
		hw_id = hw_list[hw_index];
		ret = fimc_is_hardware_open(hardware, hw_id, group, instance,
				flag, module_id);
		if (ret) {
			err("fimc_is_hardware_open(%d) is fail", hw_id);
			return ret;
		}
	}

	group_slot = GROUP_SLOT_ISP;
	group = &device->group_isp;
	group_id = path->group[group_slot];
	dbg_hw("itf_open_wrap: group[SLOT_%d]=[%x]\n", group_slot, group_id);
	hw_maxnum = fimc_is_get_hw_list(group_id, hw_list);
	for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
		hw_id = hw_list[hw_index];
		ret = fimc_is_hardware_open(hardware, hw_id, group, instance,
				flag, module_id);
		if (ret) {
			err("fimc_is_hardware_open(%d) is fail", hw_id);
			return ret;
		}
	}

	group_slot = GROUP_SLOT_DIS;
	group = &device->group_dis;
	group_id = path->group[group_slot];
	dbg_hw("itf_open_wrap: group[SLOT_%d]=[%x]\n", group_slot, group_id);
	hw_maxnum = fimc_is_get_hw_list(group_id, hw_list);
	for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
		hw_id = hw_list[hw_index];
		ret = fimc_is_hardware_open(hardware, hw_id, group, instance,
				flag, module_id);
		if (ret) {
			err("fimc_is_hardware_open(%d) is fail", hw_id);
			return ret;
		}
	}

	group_slot = GROUP_SLOT_MCS;
	group = &device->group_mcs;
	group_id = path->group[group_slot];
	dbg_hw("itf_open_wrap: group[SLOT_%d]=[%x]\n", group_slot, group_id);
	hw_maxnum = fimc_is_get_hw_list(group_id, hw_list);
	for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
		hw_id = hw_list[hw_index];
		ret = fimc_is_hardware_open(hardware, hw_id, group, instance,
				flag, module_id);
		if (ret) {
			err("fimc_is_hardware_open(%d) is fail", hw_id);
			return ret;
		}
	}

	atomic_inc(&hardware->rsccount);

	info("%s: done: hw_map[0x%lx][RSC:%d]\n", __func__,
		hardware->hw_map[instance], atomic_read(&hardware->rsccount));

	return ret;
}

int fimc_is_itf_close_wrap(struct fimc_is_device_ischain *device)
{
	struct fimc_is_hardware *hardware;
	struct fimc_is_path_info *path;
	u32 offset_path = 0;
	u32 instance = 0;
	u32 hw_id = 0;
	u32 group_slot = -1;
	u32 group_id = -1;
	int ret = 0;
	int hw_list[GROUP_HW_MAX];
	int hw_index;
	int hw_maxnum = 0;
	u32 rsccount;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;
	offset_path = (sizeof(struct sensor_open_extended) / 4) + 1;
	path = (struct fimc_is_path_info *)&device->is_region->shared[offset_path];
	rsccount = atomic_read(&hardware->rsccount);

#if !defined(SETFILE_DISABLE)
	ret = fimc_is_hardware_delete_setfile(hardware, instance,
			hardware->hw_map[instance]);
	if (ret) {
		merr("fimc_is_hardware_delete_setfile is fail(%d)", device, ret);
			return ret;
	}
#endif

	if (rsccount == 1)
		fimc_is_flush_ddk_thread();

	group_slot = GROUP_SLOT_3AA;
	group_id = path->group[group_slot];
	dbg_hw("itf_close_wrap: group[SLOT_%d]=[%x]\n", group_slot, group_id);
	hw_maxnum = fimc_is_get_hw_list(group_id, hw_list);
	for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
		hw_id = hw_list[hw_index];
		ret = fimc_is_hardware_close(hardware, hw_id, instance);
		if (ret) {
			err("fimc_is_hardware_close(%d) is fail", hw_id);
			return ret;
		}
	}

	group_slot = GROUP_SLOT_ISP;
	group_id = path->group[group_slot];
	dbg_hw("itf_close_wrap: group[SLOT_%d]=[%x]\n", group_slot, group_id);
	hw_maxnum = fimc_is_get_hw_list(group_id, hw_list);
	for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
		hw_id = hw_list[hw_index];
		ret = fimc_is_hardware_close(hardware, hw_id, instance);
		if (ret) {
			err("fimc_is_hardware_close(%d) is fail", hw_id);
			return ret;
		}
	}

	group_slot = GROUP_SLOT_DIS;
	group_id = path->group[group_slot];
	dbg_hw("itf_close_wrap: group[SLOT_%d]=[%x]\n", group_slot, group_id);
	hw_maxnum = fimc_is_get_hw_list(group_id, hw_list);
	for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
		hw_id = hw_list[hw_index];
		ret = fimc_is_hardware_close(hardware, hw_id, instance);
		if (ret) {
			err("fimc_is_hardware_close(%d) is fail", hw_id);
			return ret;
		}
	}

	group_slot = GROUP_SLOT_MCS;
	group_id = path->group[group_slot];
	dbg_hw("itf_close_wrap: group[SLOT_%d]=[%x]\n", group_slot, group_id);
	hw_maxnum = fimc_is_get_hw_list(group_id, hw_list);
	for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
		hw_id = hw_list[hw_index];
		ret = fimc_is_hardware_close(hardware, hw_id, instance);
		if (ret) {
			err("fimc_is_hardware_close(%d) is fail", hw_id);
			return ret;
		}
	}

	atomic_dec(&hardware->rsccount);

	if (rsccount == 1)
		check_lib_memory_leak();

	info("%s: done: hw_map[0x%lx][RSC:%d]\n", __func__,
		hardware->hw_map[instance], atomic_read(&hardware->rsccount));

	return ret;
}

int fimc_is_itf_setaddr_wrap(struct fimc_is_interface *itf,
	struct fimc_is_device_ischain *device, u32 *setfile_addr)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	*setfile_addr = FIMC_IS_SETFILE_OFFSET;

	return ret;
}

int fimc_is_itf_setfile_wrap(struct fimc_is_interface *itf, ulong setfile_addr,
	struct fimc_is_device_ischain *device)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

#if !defined(SETFILE_DISABLE)
	ret = fimc_is_hardware_load_setfile(hardware, setfile_addr, instance,
				hardware->hw_map[instance]);
	if (ret) {
		merr("fimc_is_hardware_load_setfile is fail(%d)", device, ret);
		return ret;
	}
#endif

	return ret;
}

int fimc_is_itf_map_wrap(struct fimc_is_device_ischain *device,
	u32 group, u32 shot_addr, u32 shot_size)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	return ret;
}

int fimc_is_itf_unmap_wrap(struct fimc_is_device_ischain *device, u32 group)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	return ret;
}

int fimc_is_itf_stream_on_wrap(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	struct fimc_is_hardware *hardware;
	u32 instance = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_stream_on(hardware, instance,
					hardware->hw_map[instance]);
	if (ret) {
		merr("fimc_is_hardware_stream_on is fail(%d)", device, ret);
		return ret;
	}

	return ret;
}

int fimc_is_itf_stream_off_wrap(struct fimc_is_device_ischain *device)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_stream_off(hardware, instance,
					hardware->hw_map[instance]);
	if (ret) {
		merr("fimc_is_hardware_stream_off is fail(%d)", device, ret);
		return ret;
	}

	return ret;
}

int fimc_is_itf_process_on_wrap(struct fimc_is_device_ischain *device, u32 group)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_process_start(hardware, instance, group);
	if (ret) {
		merr("fimc_is_hardware_process_start is fail(%d)", device, ret);
		return ret;
	}

	return ret;
}

int fimc_is_itf_process_off_wrap(struct fimc_is_device_ischain *device, u32 group,
	u32 fstop)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_process_stop(hardware, instance, group, fstop);
	if (ret) {
		merr("fimc_is_hardware_process_stop is fail(%d)", device, ret);
		return ret;
	}

	return ret;
}

int fimc_is_itf_power_down_wrap(struct fimc_is_interface *interface, u32 instance)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	return ret;
}

int fimc_is_itf_sys_ctl_wrap(struct fimc_is_device_ischain *device,
	int cmd, int val)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	return ret;
}

int fimc_is_itf_sensor_mode_wrap(struct fimc_is_device_ischain *ischain,
	struct fimc_is_sensor_cfg *cfg)
{
	dbg_hw("%s\n", __func__);

	return 0;
}

void fimc_is_itf_fwboot_init(struct fimc_is_interface *this)
{
	clear_bit(IS_IF_LAUNCH_FIRST, &this->launch_state);
	clear_bit(IS_IF_LAUNCH_SUCCESS, &this->launch_state);
	clear_bit(IS_IF_RESUME, &this->fw_boot);
	clear_bit(IS_IF_SUSPEND, &this->fw_boot);
	this->fw_boot_mode = COLD_BOOT;
}

int fimc_is_itf_shot_wrap(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group, struct fimc_is_frame *frame)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_grp_shot(hardware, instance, group, frame,
					hardware->hw_map[instance]);
	if (ret) {
		merr("fimc_is_hardware_grp_shot is fail(%d)", device, ret);
		return ret;
	}

	return ret;
}
