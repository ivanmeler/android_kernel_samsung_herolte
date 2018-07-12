/*
 * drivers/media/video/exynos/fimc-is-mc2/interface/fimc-is-interface-ishcain.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *       http://www.samsung.com
 *
 * The header file related to camera
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/workqueue.h>
#include <linux/bug.h>

#include "fimc-is-core.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-groupmgr.h"

#include "fimc-is-interface.h"
#include "fimc-is-debug.h"
#include "fimc-is-clk-gate.h"

#include "fimc-is-interface-ischain.h"
#include "fimc-is-interface-library.h"
#include "../include/fimc-is-hw.h"

u32 __iomem *notify_fcount_sen0;
u32 __iomem *notify_fcount_sen1;
u32 __iomem *notify_fcount_sen2;
u32 __iomem *notify_fcount_sen3;

#define init_request_barrier(itf) mutex_init(&itf->request_barrier)
#define enter_request_barrier(itf) mutex_lock(&itf->request_barrier);
#define exit_request_barrier(itf) mutex_unlock(&itf->request_barrier);
#define init_process_barrier(itf) spin_lock_init(&itf->process_barrier);
#define enter_process_barrier(itf) spin_lock_irq(&itf->process_barrier);
#define exit_process_barrier(itf) spin_unlock_irq(&itf->process_barrier);

extern struct fimc_is_sysfs_debug sysfs_debug;
extern struct fimc_is_lib_support gPtr_lib_support;

int fimc_is_interface_ischain_probe(struct fimc_is_interface_ischain *this,
	struct fimc_is_hardware *hardware, struct fimc_is_resourcemgr *resourcemgr,
	struct platform_device *pdev, ulong core_regs)
{
	enum fimc_is_hardware_id hw_id = DEV_HW_END;
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!this);
	BUG_ON(!resourcemgr);

	this->state = 0;
	if (core_regs)
		this->regs_mcuctl = (void *)core_regs;
	else
		this->regs_mcuctl = NULL;

#if defined(SOC_30S)
	hw_id = DEV_HW_3AA0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].kvaddr = resourcemgr->minfo.kvaddr;
	this->itf_ip[hw_slot].dvaddr = resourcemgr->minfo.dvaddr;
	this->itf_ip[hw_slot].fw_cookie = resourcemgr->minfo.fw_cookie;
	this->itf_ip[hw_slot].alloc_ctx = resourcemgr->mem.alloc_ctx;

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);
	ret = fimc_is_interface_3aa_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

#if defined(SOC_31S)
	hw_id = DEV_HW_3AA1;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].kvaddr = resourcemgr->minfo.kvaddr;
	this->itf_ip[hw_slot].dvaddr = resourcemgr->minfo.dvaddr;
	this->itf_ip[hw_slot].fw_cookie = resourcemgr->minfo.fw_cookie;
	this->itf_ip[hw_slot].alloc_ctx = resourcemgr->mem.alloc_ctx;

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);
	ret = fimc_is_interface_3aa_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

#if (defined(SOC_I0S) && !defined(SOC_3AAISP))
	hw_id = DEV_HW_ISP0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].kvaddr = resourcemgr->minfo.kvaddr;
	this->itf_ip[hw_slot].dvaddr = resourcemgr->minfo.dvaddr;
	this->itf_ip[hw_slot].fw_cookie = resourcemgr->minfo.fw_cookie;
	this->itf_ip[hw_slot].alloc_ctx = resourcemgr->mem.alloc_ctx;

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);
	ret = fimc_is_interface_isp_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

#if (defined(SOC_I1S) && !defined(SOC_3AAISP))
	hw_id = DEV_HW_ISP1;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].kvaddr = resourcemgr->minfo.kvaddr;
	this->itf_ip[hw_slot].dvaddr = resourcemgr->minfo.dvaddr;
	this->itf_ip[hw_slot].fw_cookie = resourcemgr->minfo.fw_cookie;
	this->itf_ip[hw_slot].alloc_ctx = resourcemgr->mem.alloc_ctx;

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);
	ret = fimc_is_interface_isp_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

#if defined(SOC_DIS)
	hw_id = DEV_HW_TPU;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d)", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);

	ret = fimc_is_interface_tpu_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

#if (defined(SOC_SCP) && !defined(SOC_MCS_1x1))
	hw_id = DEV_HW_SCP;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d)", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);

	ret = fimc_is_interface_scaler_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

#if (defined(SOC_SCP) && defined(SOC_MCS_1x1))
	hw_id = DEV_HW_MCSC0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d)", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);

	ret = fimc_is_interface_scaler_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

#if defined(SOC_MCS)
	/* TODO : it will re-implemented as MCSC(2x5) used */
	hw_id = DEV_HW_MCSC0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d)", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);

	ret = fimc_is_interface_scaler_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

#if defined(SOC_VRA)
	hw_id = DEV_HW_VRA;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d)", hw_slot);
		return -EINVAL;
	}

	this->itf_ip[hw_slot].hw_ip = &(hardware->hw_ip[hw_slot]);

	ret = fimc_is_interface_vra_probe(this, hw_id, pdev);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, hw_slot);
		return -EINVAL;
	}
#endif

	set_bit(IS_CHAIN_IF_STATE_INIT, &this->state);


	if (ret)
		err_itfc("probe fail (%d,%d,%d)(%d)", hw_id, hw_slot, hw_slot, ret);

	return ret;
}

int fimc_is_interface_3aa_probe(struct fimc_is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev)
{
	struct fimc_is_interface_hwip *itf_3aa = NULL;
	int i, ret = 0;
	int hw_slot = -1;

	BUG_ON(!itfc);
	BUG_ON(!pdev);

	if ((hw_id < DEV_HW_3AA0) || (hw_id >= DEV_HW_ISP0)) {
		err_itfc("invalid hw_id(%d)", hw_id);
		return -EINVAL;
	}

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	itf_3aa = &itfc->itf_ip[hw_slot];
	itf_3aa->id = hw_id;
	itf_3aa->state = 0;

	ret = fimc_is_hw_get_address(itf_3aa, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_get_irq(itf_3aa, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_request_irq(itf_3aa, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	for (i = 0; i < INTR_HWIP_MAX; i++) {
		itf_3aa->handler[i].valid = false;

		gPtr_lib_support.intr_handler_taaisp[hw_slot][i]
			= (struct hwip_intr_handler *)&itf_3aa->handler[i];
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_3aa->state);

	/* library data settings */
	gPtr_lib_support.itfc      = itfc;
	gPtr_lib_support.kvaddr    = itf_3aa->kvaddr;
	gPtr_lib_support.dvaddr    = itf_3aa->dvaddr;
	gPtr_lib_support.fw_cookie = itf_3aa->fw_cookie;
	gPtr_lib_support.alloc_ctx = itf_3aa->alloc_ctx;
	gPtr_lib_support.pdev = pdev;

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

int fimc_is_interface_isp_probe(struct fimc_is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev)
{
	struct fimc_is_interface_hwip *itf_isp = NULL;
	int i, ret = 0;
	int hw_slot = -1;

	BUG_ON(!itfc);
	BUG_ON(!pdev);

	if (hw_id != DEV_HW_ISP0 && hw_id != DEV_HW_ISP1) {
		err_itfc("invalid hw_id(%d)", hw_id);
		return -EINVAL;
	}

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	itf_isp = &itfc->itf_ip[hw_slot];
	itf_isp->id = hw_id;
	itf_isp->state = 0;

	ret = fimc_is_hw_get_address(itf_isp, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_get_irq(itf_isp, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_request_irq(itf_isp, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	for (i = 0; i < INTR_HWIP_MAX; i++) {
		itf_isp->handler[i].valid = false;

		/* TODO: this is not cool */
		gPtr_lib_support.intr_handler_taaisp[hw_slot][i]
			= (struct hwip_intr_handler *)&itf_isp->handler[i];
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_isp->state);

	/* library data settings */
	gPtr_lib_support.kvaddr    = itf_isp->kvaddr;
	gPtr_lib_support.dvaddr    = itf_isp->dvaddr;
	gPtr_lib_support.fw_cookie = itf_isp->fw_cookie;
	gPtr_lib_support.alloc_ctx = itf_isp->alloc_ctx;
	gPtr_lib_support.pdev = pdev;

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

int fimc_is_interface_tpu_probe(struct fimc_is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev)
{
	struct fimc_is_interface_hwip *itf_tpu = NULL;
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!itfc);
	BUG_ON(!pdev);

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	itf_tpu = &itfc->itf_ip[hw_slot];
	itf_tpu->id = hw_id;
	itf_tpu->state = 0;

	ret = fimc_is_hw_get_address(itf_tpu, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_get_irq(itf_tpu, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_request_irq(itf_tpu, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_tpu->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

int fimc_is_interface_scaler_probe(struct fimc_is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev)
{
	struct fimc_is_interface_hwip *itf_scaler = NULL;
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!itfc);
	BUG_ON(!pdev);

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	itf_scaler = &itfc->itf_ip[hw_slot];
	itf_scaler->id = hw_id;
	itf_scaler->state = 0;

	ret = fimc_is_hw_get_address(itf_scaler, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_get_irq(itf_scaler, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_request_irq(itf_scaler, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_scaler->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

int fimc_is_interface_vra_probe(struct fimc_is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev)
{
	struct fimc_is_interface_hwip *itf_vra = NULL;
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!itfc);
	BUG_ON(!pdev);

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	itf_vra = &itfc->itf_ip[hw_slot];
	itf_vra->id = hw_id;
	itf_vra->state = 0;

	ret = fimc_is_hw_get_address(itf_vra, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_get_irq(itf_vra, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = fimc_is_hw_request_irq(itf_vra, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_vra->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

int print_fre_work_list(struct fimc_is_work_list *this)
{
	struct fimc_is_work *work, *temp;

	if (!(this->id & TRACE_WORK_ID_MASK))
		return 0;

	printk(KERN_ERR "[INF] fre(%02X, %02d) :", this->id, this->work_free_cnt);

	list_for_each_entry_safe(work, temp, &this->work_free_head, list) {
		printk(KERN_CONT "%X(%d)->", work->msg.command, work->fcount);
	}

	printk(KERN_CONT "X\n");

	return 0;
}

static int set_free_work(struct fimc_is_work_list *this, struct fimc_is_work *work)
{
	int ret = 0;
	ulong flags;

	if (work) {
		spin_lock_irqsave(&this->slock_free, flags);

		list_add_tail(&work->list, &this->work_free_head);
		this->work_free_cnt++;
#ifdef TRACE_WORK
		print_fre_work_list(this);
#endif

		spin_unlock_irqrestore(&this->slock_free, flags);
	} else {
		ret = -EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

int print_req_work_list(struct fimc_is_work_list *this)
{
	struct fimc_is_work *work, *temp;

	if (!(this->id & TRACE_WORK_ID_MASK))
		return 0;

	printk(KERN_ERR "[INF] req(%02X, %02d) :", this->id, this->work_request_cnt);

	list_for_each_entry_safe(work, temp, &this->work_request_head, list) {
		printk(KERN_CONT "%X(%d)->", work->msg.command, work->fcount);
	}

	printk(KERN_CONT "X\n");

	return 0;
}

static int get_req_work(struct fimc_is_work_list *this,
	struct fimc_is_work **work)
{
	int ret = 0;
	ulong flags;

	if (work) {
		spin_lock_irqsave(&this->slock_request, flags);

		if (this->work_request_cnt) {
			*work = container_of(this->work_request_head.next,
					struct fimc_is_work, list);
			list_del(&(*work)->list);
			this->work_request_cnt--;
		} else
			*work = NULL;

		spin_unlock_irqrestore(&this->slock_request, flags);
	} else {
		ret = -EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

static void init_work_list(struct fimc_is_work_list *this, u32 id, u32 count)
{
	u32 i;

	this->id = id;
	this->work_free_cnt	= 0;
	this->work_request_cnt	= 0;
	INIT_LIST_HEAD(&this->work_free_head);
	INIT_LIST_HEAD(&this->work_request_head);
	spin_lock_init(&this->slock_free);
	spin_lock_init(&this->slock_request);
	for (i = 0; i < count; ++i)
		set_free_work(this, &this->work[i]);

	init_waitqueue_head(&this->wait_queue);
}

static inline void wq_func_schedule(struct fimc_is_interface *itf,
	struct work_struct *work_wq)
{
	if (itf->workqueue)
		queue_work(itf->workqueue, work_wq);
	else
		schedule_work(work_wq);
}

static void wq_func_subdev(struct fimc_is_subdev *leader,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *sub_frame,
	u32 fcount, u32 rcount, u32 status)
{
	u32 done_state = VB2_BUF_STATE_DONE;
	u32 findex, mindex;
	struct fimc_is_video_ctx *ldr_vctx, *sub_vctx;
	struct fimc_is_framemgr *ldr_framemgr, *sub_framemgr;
	struct fimc_is_frame *ldr_frame;
	struct camera2_node *capture;

	BUG_ON(!sub_frame);

	ldr_vctx = leader->vctx;
	sub_vctx = subdev->vctx;

	ldr_framemgr = GET_FRAMEMGR(ldr_vctx);
	sub_framemgr = GET_FRAMEMGR(sub_vctx);

	findex = sub_frame->stream->findex;
	mindex = ldr_vctx->queue.buf_maxcount;
	if (findex >= mindex) {
		mserr("findex(%d) is invalid(max %d)", subdev, subdev, findex, mindex);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
		goto complete;
	}

	ldr_frame = &ldr_framemgr->frames[findex];
	if (ldr_frame->fcount != fcount) {
		mserr("frame mismatched(ldr%d, sub%d)", subdev, subdev, ldr_frame->fcount, fcount);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
		goto complete;
	}

	if (status) {
		msrinfo("[ERR] NDONE(%d, E%X)\n", subdev, subdev, ldr_frame, sub_frame->index, status);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
	} else {
#ifdef DBG_STREAMING
		msrinfo(" DONE(%d)\n", subdev, subdev, ldr_frame, sub_frame->index);
#endif
		sub_frame->stream->fvalid = 1;
	}

	capture = &ldr_frame->shot_ext->node_group.capture[subdev->cid];
	if (likely(capture->vid == subdev->vid)) {
		sub_frame->stream->input_crop_region[0] = capture->input.cropRegion[0];
		sub_frame->stream->input_crop_region[1] = capture->input.cropRegion[1];
		sub_frame->stream->input_crop_region[2] = capture->input.cropRegion[2];
		sub_frame->stream->input_crop_region[3] = capture->input.cropRegion[3];
		sub_frame->stream->output_crop_region[0] = capture->output.cropRegion[0];
		sub_frame->stream->output_crop_region[1] = capture->output.cropRegion[1];
		sub_frame->stream->output_crop_region[2] = capture->output.cropRegion[2];
		sub_frame->stream->output_crop_region[3] = capture->output.cropRegion[3];
	} else {
		mserr("capture vid is changed(%d != %d)", subdev, subdev, subdev->vid, capture->vid);
		sub_frame->stream->input_crop_region[0] = 0;
		sub_frame->stream->input_crop_region[1] = 0;
		sub_frame->stream->input_crop_region[2] = 0;
		sub_frame->stream->input_crop_region[3] = 0;
		sub_frame->stream->output_crop_region[0] = 0;
		sub_frame->stream->output_crop_region[1] = 0;
		sub_frame->stream->output_crop_region[2] = 0;
		sub_frame->stream->output_crop_region[3] = 0;
	}

	clear_bit(subdev->id, &ldr_frame->out_flag);

complete:
	sub_frame->stream->fcount = fcount;
	sub_frame->stream->rcount = rcount;

	trans_frame(sub_framemgr, sub_frame, FS_COMPLETE);
	CALL_VOPS(sub_vctx, done, sub_frame->index, done_state);
}

static void wq_func_frame(struct fimc_is_subdev *leader,
	struct fimc_is_subdev *subdev,
	u32 fcount, u32 rcount, u32 status)
{
	ulong flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!leader);
	BUG_ON(!subdev);
	BUG_ON(!leader->vctx);
	BUG_ON(!subdev->vctx);
	BUG_ON(!leader->vctx->video);
	BUG_ON(!subdev->vctx->video);

	framemgr = GET_FRAMEMGR(subdev->vctx);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_4, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	if (frame) {
		if (!frame->stream) {
			mserr("stream is NULL", subdev, subdev);
			BUG();
		}

		if (unlikely(frame->stream->fcount != fcount)) {
			while (frame) {
				if (fcount == frame->stream->fcount) {
					wq_func_subdev(leader, subdev, frame, fcount, rcount, status);
					break;
				} else if (fcount > frame->stream->fcount) {
					wq_func_subdev(leader, subdev, frame, frame->stream->fcount, rcount, 0xF);

					/* get next subdev frame */
					frame = peek_frame(framemgr, FS_PROCESS);
				} else {
					warn("%d frame done is ignored", frame->stream->fcount);
					break;
				}
			}
		} else {
			wq_func_subdev(leader, subdev, frame, fcount, rcount, status);
		}
	} else {
		mserr("frame done(%p) is occured without request", subdev, subdev, frame);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_4, flags);
}

static void wq_func_30c(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_30C_FDONE]);

	get_req_work(&itf->work_list[WORK_30C_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->txc;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_30C_FDONE], work);
		get_req_work(&itf->work_list[WORK_30C_FDONE], &work);
	}
}

static void wq_func_30p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_30P_FDONE]);

	get_req_work(&itf->work_list[WORK_30P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->txp;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_30P_FDONE], work);
		get_req_work(&itf->work_list[WORK_30P_FDONE], &work);
	}
}

static void wq_func_31c(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_31C_FDONE]);

	get_req_work(&itf->work_list[WORK_31C_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->txc;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_31C_FDONE], work);
		get_req_work(&itf->work_list[WORK_31C_FDONE], &work);
	}
}

static void wq_func_31p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_31P_FDONE]);

	get_req_work(&itf->work_list[WORK_31P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->txp;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_31P_FDONE], work);
		get_req_work(&itf->work_list[WORK_31P_FDONE], &work);
	}
}

static void wq_func_i0c(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_I0C_FDONE]);

	get_req_work(&itf->work_list[WORK_I0C_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->ixc;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_I0C_FDONE], work);
		get_req_work(&itf->work_list[WORK_I0C_FDONE], &work);
	}
}

static void wq_func_i0p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_I0P_FDONE]);

	get_req_work(&itf->work_list[WORK_I0P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->ixp;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_I0P_FDONE], work);
		get_req_work(&itf->work_list[WORK_I0P_FDONE], &work);
	}
}

static void wq_func_i1c(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_I1C_FDONE]);

	get_req_work(&itf->work_list[WORK_I1C_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->ixc;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_I1C_FDONE], work);
		get_req_work(&itf->work_list[WORK_I1C_FDONE], &work);
	}
}

static void wq_func_i1p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_I1P_FDONE]);

	get_req_work(&itf->work_list[WORK_I1P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->ixp;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_I1P_FDONE], work);
		get_req_work(&itf->work_list[WORK_I1P_FDONE], &work);
	}
}

static void wq_func_scc(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_SCC_FDONE]);

	get_req_work(&itf->work_list[WORK_SCC_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->scc;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_SCC_FDONE], work);
		get_req_work(&itf->work_list[WORK_SCC_FDONE], &work);
	}
}

static void wq_func_scp(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_SCP_FDONE]);

	get_req_work(&itf->work_list[WORK_SCP_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->scp;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_SCP_FDONE], work);
		get_req_work(&itf->work_list[WORK_SCP_FDONE], &work);
	}
}

static void wq_func_m0p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_M0P_FDONE]);

	get_req_work(&itf->work_list[WORK_M0P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->m0p;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		/* For supporting multi input to single output */
		if (subdev->vctx->video->try_smp) {
			subdev->vctx->video->try_smp = false;
			up(&subdev->vctx->video->smp_multi_input);
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_M0P_FDONE], work);
		get_req_work(&itf->work_list[WORK_M0P_FDONE], &work);
	}
}

static void wq_func_m1p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_M1P_FDONE]);

	get_req_work(&itf->work_list[WORK_M1P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->m1p;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		/* For supporting multi input to single output */
		if (subdev->vctx->video->try_smp) {
			subdev->vctx->video->try_smp = false;
			up(&subdev->vctx->video->smp_multi_input);
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_M1P_FDONE], work);
		get_req_work(&itf->work_list[WORK_M1P_FDONE], &work);
	}
}

static void wq_func_m2p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_M2P_FDONE]);

	get_req_work(&itf->work_list[WORK_M2P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->m2p;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		/* For supporting multi input to single output */
		if (subdev->vctx->video->try_smp) {
			subdev->vctx->video->try_smp = false;
			up(&subdev->vctx->video->smp_multi_input);
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_M2P_FDONE], work);
		get_req_work(&itf->work_list[WORK_M2P_FDONE], &work);
	}
}

static void wq_func_m3p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_M3P_FDONE]);

	get_req_work(&itf->work_list[WORK_M3P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->m3p;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		/* For supporting multi input to single output */
		if (subdev->vctx->video->try_smp) {
			subdev->vctx->video->try_smp = false;
			up(&subdev->vctx->video->smp_multi_input);
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_M3P_FDONE], work);
		get_req_work(&itf->work_list[WORK_M3P_FDONE], &work);
	}
}

static void wq_func_m4p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_M4P_FDONE]);

	get_req_work(&itf->work_list[WORK_M4P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->m4p;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		/* For supporting multi input to single output */
		if (subdev->vctx->video->try_smp) {
			subdev->vctx->video->try_smp = false;
			up(&subdev->vctx->video->smp_multi_input);
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_M4P_FDONE], work);
		get_req_work(&itf->work_list[WORK_M4P_FDONE], &work);
	}
}

static void wq_func_group_xxx(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status, u32 lindex, u32 hindex)
{
	u32 done_state = VB2_BUF_STATE_DONE;

	BUG_ON(!vctx);
	BUG_ON(!framemgr);
	BUG_ON(!frame);

	if (status) {
		mgrinfo("[ERR] NDONE(%d, E%X(L%X H%X))\n", group, group, frame, frame->index, status,
			lindex, hindex);
		done_state = VB2_BUF_STATE_ERROR;

		/* specially force set is enabled when perframe control is fail */
		if (lindex || hindex)
			set_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);

		if (status == IS_SHOT_OVERFLOW) {
#ifdef OVERFLOW_PANIC_ENABLE
			panic("G%d overflow", group->id);
#else
			err("G%d overflow", group->id);
			/* fimc_is_resource_dump(); */
#endif
		}
	}

#ifdef DBG_STREAMING
	if (!status)
		mgrinfo(" DONE(%d)\n", group, group, frame, frame->index);
#endif

	/* Cache Invalidation */
	fimc_is_ischain_meta_invalid(frame);

	frame->result = status;
	clear_bit(group->leader.id, &frame->out_flag);
	trans_frame(framemgr, frame, FS_COMPLETE);
	fimc_is_group_done(groupmgr, group, frame, done_state);
	CALL_VOPS(vctx, done, frame->index, done_state);
}

void wq_func_group(struct fimc_is_device_ischain *device,
	struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status, u32 fcount)
{
	u32 lindex = 0;
	u32 hindex = 0;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!framemgr);
	BUG_ON(!frame);
	BUG_ON(!vctx);

	/*
	 * complete count should be lower than 3 when
	 * buffer is queued or overflow can be occured
	 */
	if (framemgr->queued_count[FS_COMPLETE] >= DIV_ROUND_UP(framemgr->num_frames, 2))
		mgwarn(" complete bufs : %d", device, group, (framemgr->queued_count[FS_COMPLETE] + 1));

	if (status) {
		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		lindex &= ~frame->shot->dm.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;
		hindex &= ~frame->shot->dm.vendor_entry.highIndexParam;
	}

	if (unlikely(fcount != frame->fcount)) {
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			while (frame) {
				if (fcount == frame->fcount) {
					wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
						status, lindex, hindex);
					break;
				} else if (fcount > frame->fcount) {
					wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
						SHOT_ERR_MISMATCH, lindex, hindex);

					/* get next leader frame */
					frame = peek_frame(framemgr, FS_PROCESS);
				} else {
					warn("%d shot done is ignored", fcount);
					break;
				}
			}
		} else {
			wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
				SHOT_ERR_MISMATCH, lindex, hindex);
		}
	} else {
		wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
			status, lindex, hindex);
	}
}

static void wq_func_shot(struct work_struct *data)
{
	struct fimc_is_device_ischain *device;
	struct fimc_is_interface *itf;
	struct fimc_is_msg *msg;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_work_list *work_list;
	struct fimc_is_work *work;
	struct fimc_is_video_ctx *vctx;
	ulong flags;
	u32 fcount, status;
	int instance;
	int group_id;
	struct fimc_is_core *core;

	BUG_ON(!data);

	itf = container_of(data, struct fimc_is_interface, work_wq[INTR_SHOT_DONE]);
	work_list = &itf->work_list[INTR_SHOT_DONE];
	group  = NULL;
	vctx = NULL;
	framemgr = NULL;

	get_req_work(work_list, &work);
	while (work) {
		core = (struct fimc_is_core *)itf->core;
		instance = work->msg.instance;
		group_id = work->msg.group;
		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		groupmgr = device->groupmgr;

		msg = &work->msg;
		fcount = msg->param1;
		status = msg->param2;

		switch (group_id) {
		case GROUP_ID(GROUP_ID_3AA0):
		case GROUP_ID(GROUP_ID_3AA1):
			group = &device->group_3aa;
			break;
		case GROUP_ID(GROUP_ID_ISP0):
		case GROUP_ID(GROUP_ID_ISP1):
			group = &device->group_isp;
			break;
		case GROUP_ID(GROUP_ID_DIS0):
			group = &device->group_dis;
			break;
		case GROUP_ID(GROUP_ID_MCS0):
		case GROUP_ID(GROUP_ID_MCS1):
			group = &device->group_mcs;
			break;
		case GROUP_ID(GROUP_ID_VRA0):
			group = &device->group_vra;
			break;
		default:
			merr("unresolved group id %d", device, group_id);
			group = NULL;
			vctx = NULL;
			framemgr = NULL;
			goto remain;
		}

		if (!group) {
			merr("group is NULL", device);
			goto remain;
		}

		vctx = group->leader.vctx;
		if (!vctx) {
			merr("vctx is NULL", device);
			goto remain;
		}

		framemgr = GET_FRAMEMGR(vctx);
		if (!framemgr) {
			merr("framemgr is NULL", device);
			goto remain;
		}

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_5, flags);

		frame = peek_frame(framemgr, FS_PROCESS);

		if (frame) {
			PROGRAM_COUNT(13);
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
			do_gettimeofday(&frame->tzone[TM_SHOT_D]);
#endif
#endif

#ifdef ENABLE_CLOCK_GATE
			/* dynamic clock off */
			if (sysfs_debug.en_clk_gate &&
					sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
				fimc_is_clk_gate_set(core, group->id, false, false, true);
#endif
			wq_func_group(device, groupmgr, group, framemgr, frame,
				vctx, status, fcount);

			PROGRAM_COUNT(14);
		} else {
			mgerr("invalid shot done(%d)", device, group, fcount);
			frame_manager_print_queues(framemgr);
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_5, flags);
#ifdef ENABLE_CLOCK_GATE
		if (fcount == 1 &&
				sysfs_debug.en_clk_gate &&
				sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
			fimc_is_clk_gate_lock_set(core, instance, false);
#endif
remain:
		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static inline void print_framemgr_spinlock_usage(struct fimc_is_core *core)
{
	u32 i;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_subdev *subdev;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; ++i) {
		sensor = &core->sensor[i];
		if (test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state) && (framemgr = GET_SUBDEV_FRAMEMGR(sensor)))
			info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);
	}

	for (i = 0; i < FIMC_IS_STREAM_COUNT; ++i) {
		ischain = &core->ischain[i];
		if (test_bit(FIMC_IS_ISCHAIN_OPEN, &ischain->state)) {
			/* 3AA GROUP */
			subdev = &ischain->group_3aa.leader;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->txc;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->txp;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			/* ISP GROUP */
			subdev = &ischain->group_isp.leader;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->ixc;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->ixp;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			/* DIS GROUP */
			subdev = &ischain->group_dis.leader;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->scc;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->scp;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);
		}
	}
}

#define VERSION_OF_NO_NEED_IFLAG 221
int fimc_is_interface_probe(struct fimc_is_interface *this,
	struct fimc_is_minfo *minfo,
	ulong regs,
	u32 irq,
	void *core_data)
{
	int ret = 0;
	struct fimc_is_core *core = (struct fimc_is_core *)core_data;

	dbg_interface("%s\n", __func__);

	init_request_barrier(this);
	init_process_barrier(this);
	init_waitqueue_head(&this->lock_wait_queue);
	init_waitqueue_head(&this->init_wait_queue);
	init_waitqueue_head(&this->idle_wait_queue);
	spin_lock_init(&this->shot_check_lock);

	this->workqueue = alloc_workqueue("fimc-is/highpri", WQ_HIGHPRI, 0);
	if (!this->workqueue)
		probe_warn("failed to alloc own workqueue, will be use global one");

	INIT_WORK(&this->work_wq[WORK_SHOT_DONE], wq_func_shot);
	INIT_WORK(&this->work_wq[WORK_SHOT_DONE], wq_func_shot);
	INIT_WORK(&this->work_wq[WORK_30C_FDONE], wq_func_30c);
	INIT_WORK(&this->work_wq[WORK_30P_FDONE], wq_func_30p);
	INIT_WORK(&this->work_wq[WORK_31C_FDONE], wq_func_31c);
	INIT_WORK(&this->work_wq[WORK_31P_FDONE], wq_func_31p);
	INIT_WORK(&this->work_wq[WORK_I0C_FDONE], wq_func_i0c);
	INIT_WORK(&this->work_wq[WORK_I0P_FDONE], wq_func_i0p);
	INIT_WORK(&this->work_wq[WORK_I1C_FDONE], wq_func_i1c);
	INIT_WORK(&this->work_wq[WORK_I1P_FDONE], wq_func_i1p);
	INIT_WORK(&this->work_wq[WORK_SCC_FDONE], wq_func_scc);
	INIT_WORK(&this->work_wq[WORK_SCP_FDONE], wq_func_scp);
	INIT_WORK(&this->work_wq[WORK_M0P_FDONE], wq_func_m0p);
	INIT_WORK(&this->work_wq[WORK_M1P_FDONE], wq_func_m1p);
	INIT_WORK(&this->work_wq[WORK_M2P_FDONE], wq_func_m2p);
	INIT_WORK(&this->work_wq[WORK_M3P_FDONE], wq_func_m3p);
	INIT_WORK(&this->work_wq[WORK_M4P_FDONE], wq_func_m4p);

	this->core = (void *)core;
	this->regs = (void *)regs;

	clear_bit(IS_IF_STATE_OPEN, &this->state);
	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);
	clear_bit(IS_IF_STATE_READY, &this->state);
	clear_bit(IS_IF_STATE_LOGGING, &this->state);

	init_work_list(&this->nblk_cam_ctrl, TRACE_WORK_ID_CAMCTRL, MAX_NBLOCKING_COUNT);
	init_work_list(&this->work_list[WORK_GENERAL], TRACE_WORK_ID_GENERAL, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_SHOT_DONE], TRACE_WORK_ID_SHOT, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_30C_FDONE], TRACE_WORK_ID_30C, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_30P_FDONE], TRACE_WORK_ID_30P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_31C_FDONE], TRACE_WORK_ID_31C, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_31P_FDONE], TRACE_WORK_ID_31P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_I0C_FDONE], TRACE_WORK_ID_I0C, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_I0P_FDONE], TRACE_WORK_ID_I0P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_I1C_FDONE], TRACE_WORK_ID_I1C, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_I1P_FDONE], TRACE_WORK_ID_I1P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_SCC_FDONE], TRACE_WORK_ID_SCC, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_SCP_FDONE], TRACE_WORK_ID_SCP, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_M0P_FDONE], TRACE_WORK_ID_M0P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_M1P_FDONE], TRACE_WORK_ID_M1P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_M2P_FDONE], TRACE_WORK_ID_M2P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_M3P_FDONE], TRACE_WORK_ID_M3P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_M4P_FDONE], TRACE_WORK_ID_M4P, MAX_WORK_COUNT);

	this->err_report_vendor = NULL;

	return ret;
}

int fimc_is_interface_open(struct fimc_is_interface *this)
{
	int i;
	int ret = 0;

	if (test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already open");
		ret = -EMFILE;
		goto exit;
	}

	dbg_interface("%s\n", __func__);

	for (i = 0; i < FIMC_IS_STREAM_COUNT; i++) {
		this->streaming[i] = IS_IF_STREAMING_INIT;
		this->processing[i] = IS_IF_PROCESSING_INIT;
	}

	fimc_is_hw_set_chain_interrupt(this);

	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);
	clear_bit(IS_IF_STATE_READY, &this->state);
	clear_bit(IS_IF_STATE_LOGGING, &this->state);

	set_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

int fimc_is_interface_close(struct fimc_is_interface *this)
{
	int ret = 0;

	if (!test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already close");
		ret = -EMFILE;
		goto exit;
	}

	dbg_interface("%s\n", __func__);

	clear_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

void fimc_is_interface_reset(struct fimc_is_interface *this)
{
	return;
}

int fimc_is_hw_logdump(struct fimc_is_interface *this)
{
	return 0;
}

int fimc_is_hw_regdump(struct fimc_is_interface *this)
{
	return 0;
}

int fimc_is_hw_memdump(struct fimc_is_interface *this,
	ulong start,
	ulong end)
{
	return 0;
}

int fimc_is_hw_i2c_lock(struct fimc_is_interface *this,
	u32 instance, int i2c_clk, bool lock)
{
	return 0;
}

void fimc_is_interface_lock(struct fimc_is_interface *this)
{
	return;
}

void fimc_is_interface_unlock(struct fimc_is_interface *this)
{
	return;
}

int fimc_is_hw_g_capability(struct fimc_is_interface *this,
	u32 instance, u32 address)
{
	return 0;
}

int fimc_is_hw_fault(struct fimc_is_interface *this)
{
	return 0;
}
