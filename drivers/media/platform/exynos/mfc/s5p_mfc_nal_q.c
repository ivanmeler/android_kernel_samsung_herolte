/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_nal_q.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "s5p_mfc_common.h"
#include "s5p_mfc_nal_q.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_reg.h"
#include "s5p_mfc_cmd.h"

int s5p_mfc_nal_q_check_single_encoder(struct s5p_mfc_ctx *ctx)
{
	int ret = 0, ctx_num = 0;
	struct s5p_mfc_dev *dev;

	dev = ctx->dev;

	if (dev->num_inst != 1)
		return ret;

	while (!dev->ctx[ctx_num]) {
		ctx_num++;
		if (ctx_num >= MFC_NUM_CONTEXTS) {
			mfc_err("NAL Q: Can't found ctx to run nal q\n");
			return ret;
		}
	}

	if (ctx != dev->ctx[ctx_num])
		return ret;

	if (ctx->is_drm)
		return ret;

	if (ctx->type == MFCINST_ENCODER &&
		(ctx->state == MFCINST_RUNNING || ctx->state == MFCINST_RUNNING_NO_OUTPUT)) {
		dev->nal_q_handle->nal_q_ctx = ctx_num;
		ret = 1;
	} else {
		ret = 0;
	}

	return ret;
}

int s5p_mfc_nal_q_check_last_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_buf *temp_vb;
	struct s5p_mfc_dev *dev;
	unsigned long flags;

	dev = ctx->dev;
	spin_lock_irqsave(&dev->irqlock, flags);
	temp_vb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (temp_vb->vb.v4l2_buf.reserved2 & FLAG_LAST_FRAME)
		return 1;
	return 0;
}

int s5p_mfc_nal_q_find_ctx(struct s5p_mfc_dev *dev, EncoderOutputStr *pOutputStr)
{
	int i;

	for(i = 0; i < MFC_NUM_CONTEXTS; i++) {
		if (dev->ctx[i] && dev->ctx[i]->inst_no == pOutputStr->InstanceId)
			return i;
	}
	return -1;
}

static nal_queue_in_handle* s5p_mfc_nal_q_create_in_q(struct s5p_mfc_dev *dev,
		nal_queue_handle *nal_q_handle)
{
	nal_queue_in_handle *nal_q_in_handle;

	mfc_debug_enter();

	nal_q_in_handle = kzalloc(sizeof(*nal_q_in_handle), GFP_KERNEL);
	if (!nal_q_in_handle) {
		mfc_err("NAL Q: Failed to get memory for nal_queue_in_handle\n");
		return NULL;
	}

	nal_q_in_handle->nal_q_handle = nal_q_handle;
	nal_q_in_handle->in_alloc
		= s5p_mfc_mem_alloc_priv(dev->alloc_ctx, sizeof(*(nal_q_in_handle->nal_q_in_addr)));
	if (IS_ERR(nal_q_in_handle->in_alloc)) {
		mfc_err("NAL Q: failed to get memory\n");
		kfree(nal_q_in_handle);
		return NULL;
	}

	nal_q_in_handle->nal_q_in_addr
		= (nal_in_queue *)s5p_mfc_mem_vaddr_priv(nal_q_in_handle->in_alloc);
	if (!nal_q_in_handle->nal_q_in_addr) {
		mfc_err("NAL Q: failed to get vaddr\n");
		s5p_mfc_mem_free_priv(nal_q_in_handle->in_alloc);
		kfree(nal_q_in_handle);
		return NULL;
	}

	mfc_debug_leave();
	return nal_q_in_handle;
}

static nal_queue_out_handle* s5p_mfc_nal_q_create_out_q(struct s5p_mfc_dev *dev,
		nal_queue_handle *nal_q_handle)
{
	nal_queue_out_handle *nal_q_out_handle;

	mfc_debug_enter();

	nal_q_out_handle = kzalloc(sizeof(*nal_q_out_handle), GFP_KERNEL);
	if (!nal_q_out_handle) {
		mfc_err("NAL Q: failed to get memory for nal_queue_out_handle\n");
		return NULL;
	}

	nal_q_out_handle->nal_q_handle = nal_q_handle;
	nal_q_out_handle->out_alloc
		= s5p_mfc_mem_alloc_priv(dev->alloc_ctx, sizeof(*(nal_q_out_handle->nal_q_out_addr)));
	if (IS_ERR(nal_q_out_handle->out_alloc)) {
		mfc_err("NAL Q: failed to get memory\n");
		kfree(nal_q_out_handle);
		return NULL;
	}

	nal_q_out_handle->nal_q_out_addr
		= (nal_out_queue *)s5p_mfc_mem_vaddr_priv(nal_q_out_handle->out_alloc);
	if (!nal_q_out_handle->nal_q_out_addr) {
		mfc_err("NAL Q : failed to get vaddr\n");
		s5p_mfc_mem_free_priv(nal_q_out_handle->out_alloc);
		kfree(nal_q_out_handle);
		return NULL;
	}

	mfc_debug_leave();
	return nal_q_out_handle;
}

static int s5p_mfc_nal_q_destroy_in_q(nal_queue_in_handle *nal_q_in_handle)
{
	mfc_debug_enter();

	if (!nal_q_in_handle)
		return -EINVAL;

	if (nal_q_in_handle->in_alloc)
		s5p_mfc_mem_free_priv(nal_q_in_handle->in_alloc);
	if (nal_q_in_handle)
		kfree(nal_q_in_handle);

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_nal_q_destroy_out_q(nal_queue_out_handle *nal_q_out_handle)
{
	mfc_debug_enter();

	if (!nal_q_out_handle)
		return -EINVAL;

	if (nal_q_out_handle->out_alloc)
		s5p_mfc_mem_free_priv(nal_q_out_handle->out_alloc);
	if (nal_q_out_handle)
		kfree(nal_q_out_handle);

	mfc_debug_leave();

	return 0;
}

/*
  * This function should be called after s5p_mfc_alloc_firmware() being called.
  */
nal_queue_handle *s5p_mfc_nal_q_create(struct s5p_mfc_dev *dev)
{
	nal_queue_handle *nal_q_handle;

	mfc_debug_enter();

	if (!dev) {
		return NULL;
	}

	nal_q_handle = kzalloc(sizeof(*nal_q_handle), GFP_KERNEL);
	if (!nal_q_handle) {
		return NULL;
	}

	nal_q_handle->nal_q_in_handle = s5p_mfc_nal_q_create_in_q(dev, nal_q_handle);
	if (!nal_q_handle->nal_q_in_handle) {
		kfree(nal_q_handle);
		return NULL;
	}

	nal_q_handle->nal_q_out_handle = s5p_mfc_nal_q_create_out_q(dev, nal_q_handle);
	if (!nal_q_handle->nal_q_out_handle) {
		s5p_mfc_nal_q_destroy_in_q(nal_q_handle->nal_q_in_handle);
		kfree(nal_q_handle);
		return NULL;
	}

	nal_q_handle->nal_q_state = NAL_Q_STATE_CREATED;
	MFC_TRACE_DEV(">> nal q state changed : %d\n", nal_q_handle->nal_q_state);

	mfc_debug_leave();

	return nal_q_handle;
}

int s5p_mfc_nal_q_destroy(struct s5p_mfc_dev *dev, nal_queue_handle *nal_q_handle)
{
	int ret = 0;

	mfc_debug_enter();

	if (!nal_q_handle) {
		mfc_err_dev("there isn't nal_q_handle\n");
		return -EINVAL;
	}

	ret = s5p_mfc_nal_q_destroy_out_q(nal_q_handle->nal_q_out_handle);
	if (ret) {
		mfc_err_dev("failed nal_q_out_handle destroy\n");
		return ret;
	}

	ret = s5p_mfc_nal_q_destroy_in_q(nal_q_handle->nal_q_in_handle);
	if (ret) {
		mfc_err_dev("failed nal_q_in_handle destroy\n");
		return ret;
	}

	kfree(nal_q_handle);
	dev->nal_q_handle = NULL;

	mfc_debug_leave();

	return ret;
}

void s5p_mfc_nal_q_init(struct s5p_mfc_dev *dev, nal_queue_handle *nal_q_handle)
{
	mfc_debug_enter();

	if (!nal_q_handle ||
		((nal_q_handle->nal_q_state != NAL_Q_STATE_CREATED)
		&& (nal_q_handle->nal_q_state != NAL_Q_STATE_STOPPED))) {
		mfc_err("NAL Q: There is no handle or state is wrong\n");
		return;
	}

	MFC_WRITEL(0x0, S5P_FIMV_NAL_QUEUE_INPUT_COUNT);
	MFC_WRITEL(0x0, S5P_FIMV_NAL_QUEUE_OUTPUT_COUNT);
	MFC_WRITEL(0x0, S5P_FIMV_NAL_QUEUE_INPUT_EXE_COUNT);
	MFC_WRITEL(0x0, S5P_FIMV_NAL_QUEUE_INFO);

	nal_q_handle->nal_q_in_handle->in_exe_count = 0;
	nal_q_handle->nal_q_out_handle->out_exe_count = 0;

	mfc_debug(2, "NAL Q: S5P_FIMV_NAL_QUEUE_INPUT_COUNT=%d\n",
		MFC_READL(S5P_FIMV_NAL_QUEUE_INPUT_COUNT));
	mfc_debug(2, "NAL Q: S5P_FIMV_NAL_QUEUE_OUTPUT_COUNT=%d\n",
		MFC_READL(S5P_FIMV_NAL_QUEUE_OUTPUT_COUNT));
	mfc_debug(2, "NAL Q: S5P_FIMV_NAL_QUEUE_INPUT_EXE_COUNT=%d\n",
		MFC_READL(S5P_FIMV_NAL_QUEUE_INPUT_EXE_COUNT));
	mfc_debug(2, "NAL Q: S5P_FIMV_NAL_QUEUE_INFO=%d\n",
		MFC_READL(S5P_FIMV_NAL_QUEUE_INFO));

	nal_q_handle->nal_q_state = NAL_Q_STATE_INITIALIZED;
	nal_q_handle->nal_q_ctx = -1;
	MFC_TRACE_DEV(">> nal q state changed : %d\n", nal_q_handle->nal_q_state);

	mfc_debug_leave();
	return;
}

void s5p_mfc_nal_q_start(struct s5p_mfc_dev *dev, nal_queue_handle *nal_q_handle)
{
	dma_addr_t addr;

	mfc_debug_enter();

	if (!nal_q_handle || (nal_q_handle->nal_q_state != NAL_Q_STATE_INITIALIZED)) {
		return;
	}

	addr = s5p_mfc_mem_daddr_priv(nal_q_handle->nal_q_in_handle->in_alloc);
	MFC_WRITEL(addr, S5P_FIMV_NAL_QUEUE_INPUT_ADDR);
	MFC_WRITEL(NAL_Q_IN_ENTRY_SIZE * NAL_Q_IN_QUEUE_SIZE, S5P_FIMV_NAL_QUEUE_INPUT_SIZE);

	mfc_debug(2, "NAL Q: S5P_FIMV_NAL_QUEUE_INPUT_ADDR=0x%x\n",
		MFC_READL(S5P_FIMV_NAL_QUEUE_INPUT_ADDR));
	mfc_debug(2, "NAL Q: S5P_FIMV_NAL_QUEUE_INPUT_SIZE=%d\n",
		MFC_READL(S5P_FIMV_NAL_QUEUE_INPUT_SIZE));

	addr = s5p_mfc_mem_daddr_priv(nal_q_handle->nal_q_out_handle->out_alloc);
	MFC_WRITEL(addr, S5P_FIMV_NAL_QUEUE_OUTPUT_ADDR);
	MFC_WRITEL(NAL_Q_OUT_ENTRY_SIZE * NAL_Q_OUT_QUEUE_SIZE, S5P_FIMV_NAL_QUEUE_OUTPUT_SIZE);

	mfc_debug(2, "NAL Q: S5P_FIMV_NAL_QUEUE_OUTPUT_ADDR=0x%x\n",
		MFC_READL(S5P_FIMV_NAL_QUEUE_OUTPUT_ADDR));
	mfc_debug(2, "S5P_FIMV_NAL_QUEUE_OUTPUT_SIZE=%d\n",
		MFC_READL(S5P_FIMV_NAL_QUEUE_OUTPUT_SIZE));

	s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_NAL_QUEUE, NULL);

	nal_q_handle->nal_q_state = NAL_Q_STATE_STARTED;
	MFC_TRACE_DEV(">> nal q state changed : %d\n", nal_q_handle->nal_q_state);

	mfc_debug_leave();

	return;
}

void s5p_mfc_nal_q_stop(struct s5p_mfc_dev *dev, nal_queue_handle *nal_q_handle)
{
	mfc_debug_enter();

	if (!nal_q_handle) {
		return;
	}

	s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_STOP_QUEUE, NULL);

	nal_q_handle->nal_q_state = NAL_Q_STATE_STOPPED;
	MFC_TRACE_DEV(">> nal q state changed : %d\n", nal_q_handle->nal_q_state);

	mfc_debug_leave();

	return;
}

void s5p_mfc_nal_q_cleanup_queue(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_buf *src_buf, *dst_buf;
	struct s5p_mfc_ctx *ctx;
	unsigned long flags;
	mfc_debug_enter();

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->ctx[dev->nal_q_handle->nal_q_ctx];
	if (ctx) {
		while (!list_empty(&ctx->src_queue_nal_q)) {
			src_buf = list_entry(ctx->src_queue_nal_q.prev,
					struct s5p_mfc_buf, list);
			src_buf->used = 0;
			list_del(&src_buf->list);
			ctx->src_queue_cnt_nal_q--;
			mfc_debug(2, "NAL Q: cleanup, src_queue_nal_q -> src_queue, index:%d\n",
					src_buf->vb.v4l2_buf.index);
			list_add(&src_buf->list, &ctx->src_queue);
			ctx->src_queue_cnt++;
		}
		while (!list_empty(&ctx->dst_queue_nal_q)) {
			dst_buf = list_entry(ctx->dst_queue_nal_q.prev,
					struct s5p_mfc_buf, list);
			dst_buf->used = 0;
			list_del(&dst_buf->list);
			ctx->dst_queue_cnt_nal_q--;
			mfc_debug(2, "NAL Q: cleanup, dst_queue_nal_q -> dst_queue, index:%d\n",
					dst_buf->vb.v4l2_buf.index);
			list_add(&dst_buf->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;
		}
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);
	mfc_debug_leave();
	return;
}

/*
  * This function should be called in NAL_Q_STATE_INITIALIZED or NAL_Q_STATE_STARTED state.
  */
EncoderInputStr *s5p_mfc_nal_q_get_free_slot_in_q(struct s5p_mfc_dev *dev,
	nal_queue_in_handle *nal_q_in_handle)
{
	unsigned int input_count = 0;
	unsigned int input_exe_count = 0;
	int input_diff = 0;
	unsigned int index = 0;
	EncoderInputStr *pStr = NULL;

	mfc_debug_enter();

	if (!nal_q_in_handle || !nal_q_in_handle->nal_q_in_addr ||
		((nal_q_in_handle->nal_q_handle->nal_q_state != NAL_Q_STATE_INITIALIZED)
		&& (nal_q_in_handle->nal_q_handle->nal_q_state != NAL_Q_STATE_STARTED))) {
		mfc_err("NAL Q: There is no handle or state is wrong\n");
		return pStr;
	}

	input_count = MFC_READL(S5P_FIMV_NAL_QUEUE_INPUT_COUNT);
	input_exe_count = MFC_READL(S5P_FIMV_NAL_QUEUE_INPUT_EXE_COUNT);
	input_diff = input_count - input_exe_count;
	/* meaning of the variable input_diff
	* 0: 				number of available slots = NAL_Q_IN_QUEUE_SIZE
	* 1: 				number of available slots = NAL_Q_IN_QUEUE_SIZE - 1
	* ...
	* NAL_Q_IN_QUEUE_SIZE-1:	number of available slots = 1
	* NAL_Q_IN_QUEUE_SIZE:	number of available slots = 0
	*/

	mfc_debug(2, "NAL Q: input_diff = %d\n", input_diff);
	if ((input_diff < 0) || (input_diff >= NAL_Q_IN_QUEUE_SIZE)) {
		mfc_err("NAL Q: No available input slot(%d)\n", input_diff);
		return pStr;
	}

	index = input_count % NAL_Q_IN_QUEUE_SIZE;
	pStr = &(nal_q_in_handle->nal_q_in_addr->entry[index].enc);

	memset(pStr, 0, NAL_Q_IN_ENTRY_SIZE);

	mfc_debug_leave();
	return pStr;
}

void s5p_mfc_nal_q_set_slice_mode(struct s5p_mfc_ctx *ctx, EncoderInputStr *pInStr)
{
	struct s5p_mfc_enc *enc = ctx->enc_priv;

	/* multi-slice control */
	if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES)
		pInStr->MsliceMode = enc->slice_mode + 0x4;
	else if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB_ROW)
		pInStr->MsliceMode = enc->slice_mode - 0x2;
	else
		pInStr->MsliceMode = enc->slice_mode;

	/* multi-slice MB number or bit size */
	if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB || \
			enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB_ROW) {
		pInStr->MsliceSizeMb = enc->slice_size.mb;
	} else if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) {
		pInStr->MsliceSizeBits = enc->slice_size.bits;
	} else {
		pInStr->MsliceSizeMb = 0;
		pInStr->MsliceSizeBits = 0;
	}
}

int s5p_mfc_nal_q_set_in_buf_enc(struct s5p_mfc_ctx *ctx, EncoderInputStr *pInStr)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *src_buf, *dst_buf;
	struct s5p_mfc_raw_info *raw = NULL;
	dma_addr_t src_addr[3] = {0, 0, 0};
	unsigned int index, i;
	unsigned long flags;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("NAL Q: no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("NAL Q: no mfc device to run\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&dev->irqlock, flags);
	if (list_empty(&ctx->src_queue)) {
		mfc_err("NAL Q: no src buffers\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}
	if (list_empty(&ctx->dst_queue)) {
		mfc_err("NAL Q: no dst buffers\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	pInStr->startcode = 0xBBBBBBBB;
	pInStr->CommandId = 0;

	raw = &ctx->raw_buf;
	src_buf = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	src_buf->used = 1;

	for (i = 0; i < raw->num_planes; i++) {
		src_addr[i] = s5p_mfc_mem_plane_addr(ctx, &src_buf->vb, i);
		mfc_debug(2, "NAL Q: enc src[%d] addr: 0x%08lx\n",
				i, (unsigned long)src_addr[i]);
	}

	for (i = 0; i < raw->num_planes; i++)
		pInStr->SourceAddr[i] = src_addr[i];

	dst_buf = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	dst_buf->used = 1;
	s5p_mfc_mem_plane_addr(ctx, &dst_buf->vb, 0);

	pInStr->StreamBufferAddr = s5p_mfc_mem_plane_addr(ctx, &dst_buf->vb, 0);
	pInStr->StreamBufferSize = (unsigned int)vb2_plane_size(&dst_buf->vb, 0);

	index = src_buf->vb.v4l2_buf.index;
	if (call_cop(ctx, set_buf_ctrls_val_nal_q, ctx, &ctx->src_ctrls[index], pInStr) < 0)
		mfc_err_ctx("NAL Q: failed in set_buf_ctrals_val in nal q\n");

	mfc_debug(2, "NAL Q: input queue, src_queue -> src_queue_nal_q, index:%d\n",
			src_buf->vb.v4l2_buf.index);
	mfc_debug(2, "NAL Q: input queue, dst_queue -> dst_queue_nal_q, index:%d\n",
			dst_buf->vb.v4l2_buf.index);

	pInStr->InstanceId = ctx->inst_no;
	s5p_mfc_nal_q_set_slice_mode(ctx, pInStr);

	/* move src_queue -> src_queue_nal_q */
	list_del(&src_buf->list);
	ctx->src_queue_cnt--;
	list_add_tail(&src_buf->list, &ctx->src_queue_nal_q);
	ctx->src_queue_cnt_nal_q++;
	/* move dst_queue -> dst_queue_nal_q */
	list_del(&dst_buf->list);
	ctx->dst_queue_cnt--;
	list_add_tail(&dst_buf->list, &ctx->dst_queue_nal_q);
	ctx->dst_queue_cnt_nal_q++;

	dev->curr_ctx = ctx->num;

	spin_unlock_irqrestore(&dev->irqlock, flags);
	mfc_debug_leave();
	return 0;
}
int s5p_mfc_nal_q_set_in_buf(struct s5p_mfc_ctx *ctx, EncoderInputStr *pInStr)
{
	int ret = -1;
	mfc_debug_enter();

	if (ctx->type == MFCINST_ENCODER)
		ret = s5p_mfc_nal_q_set_in_buf_enc(ctx, pInStr);

	mfc_debug_leave();
	return ret;
}

void s5p_mfc_nal_q_get_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr[], int num_planes, EncoderOutputStr *pOutStr)
{
	unsigned long enc_recon_y_addr, enc_recon_c_addr;
	int i;

	for (i = 0; i < num_planes; i++)
		addr[i] = pOutStr->EncodedSourceAddr[i];

	enc_recon_y_addr = pOutStr->ReconLumaDpbAddr;
	enc_recon_c_addr = pOutStr->ReconChromaDpbAddr;

	mfc_debug(2, "NAL Q: recon y addr: 0x%08lx\n", enc_recon_y_addr);
	mfc_debug(2, "NAL Q: recon c addr: 0x%08lx\n", enc_recon_c_addr);
}

void s5p_mfc_nal_q_post_frame_enc(struct s5p_mfc_ctx *ctx, EncoderOutputStr *pOutStr)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_buf *src_buf, *dst_buf, *ref_buf, *next_entry;
	struct s5p_mfc_raw_info *raw;
	dma_addr_t enc_addr[3] = { 0, 0, 0 };
	dma_addr_t mb_addr[3] = { 0, 0, 0 };
	int slice_type, i;
	unsigned int strm_size;
	unsigned int pic_count;
	unsigned long flags;
	unsigned int index;

	mfc_debug_enter();

	slice_type = pOutStr->SliceType;
	strm_size = pOutStr->StreamSize;
	pic_count = pOutStr->EncCnt;

	mfc_debug(2, "NAL Q: encoded slice type: %d\n", slice_type);
	mfc_debug(2, "NAL Q: encoded stream size: %d\n", strm_size);
	mfc_debug(2, "NAL Q: display order: %d\n", pic_count);
/*
	if (enc->buf_full) {
		ctx->state = MFCINST_ABORT_INST;
		return 0;
	}
*/
	/* set encoded frame type */
	enc->frame_type = slice_type;
	raw = &ctx->raw_buf;

	spin_lock_irqsave(&dev->irqlock, flags);

	ctx->sequence++;
	if (strm_size > 0) {
		/* at least one more dest. buffers exist always  */
		dst_buf = list_entry(ctx->dst_queue_nal_q.next, struct s5p_mfc_buf, list);

		dst_buf->vb.v4l2_buf.flags &=
			~(V4L2_BUF_FLAG_KEYFRAME |
			  V4L2_BUF_FLAG_PFRAME |
			  V4L2_BUF_FLAG_BFRAME);

		switch (slice_type) {
		case S5P_FIMV_ENCODED_TYPE_I:
			dst_buf->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_KEYFRAME;
				break;
		case S5P_FIMV_ENCODED_TYPE_P:
			dst_buf->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_PFRAME;
			break;
		case S5P_FIMV_ENCODED_TYPE_B:
			dst_buf->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_BFRAME;
			break;
		default:
			dst_buf->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_KEYFRAME;
			break;
		}
		mfc_debug(2, "NAL Q: Slice type : %d\n", dst_buf->vb.v4l2_buf.flags);

		list_del(&dst_buf->list);
		ctx->dst_queue_cnt_nal_q--;
		vb2_set_plane_payload(&dst_buf->vb, 0, strm_size);

		index = dst_buf->vb.v4l2_buf.index;
		if (call_cop(ctx, get_buf_ctrls_val_nal_q, ctx,
				&ctx->dst_ctrls[index], pOutStr) < 0)
			mfc_err_ctx("NAL Q: failed in get_buf_ctrls_val in nal q\n");

		vb2_buffer_done(&dst_buf->vb, VB2_BUF_STATE_DONE);
	}

	if (slice_type >= 0) {
		if (ctx->state == MFCINST_RUNNING_NO_OUTPUT ||
			ctx->state == MFCINST_RUNNING_BUF_FULL)
			ctx->state = MFCINST_RUNNING;

		s5p_mfc_nal_q_get_enc_frame_buffer(ctx, &enc_addr[0],
					raw->num_planes, pOutStr);

		for (i = 0; i < raw->num_planes; i++)
			mfc_debug(2, "NAL Q: encoded[%d] addr: 0x%08lx\n", i,
					(unsigned long)enc_addr[i]);

		list_for_each_entry_safe(src_buf, next_entry,
						&ctx->src_queue_nal_q, list) {
			for (i = 0; i < raw->num_planes; i++) {
				mb_addr[i] = s5p_mfc_mem_plane_addr(ctx,
							&src_buf->vb, i);
				mfc_debug(2, "NAL Q: enc src[%d] addr: 0x%08lx\n", i,
						(unsigned long)mb_addr[i]);
			}

			if (enc_addr[0] == mb_addr[0]) {
				list_del(&src_buf->list);
				ctx->src_queue_cnt_nal_q--;

				vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
				break;
			}
		}

		list_for_each_entry(ref_buf, &enc->ref_queue, list) {
			for (i = 0; i < raw->num_planes; i++) {
				mb_addr[i] = s5p_mfc_mem_plane_addr(ctx,
						&ref_buf->vb, i);
				mfc_debug(2, "enc ref[%d] addr: 0x%08lx\n", i,
						(unsigned long)mb_addr[i]);
			}

			if (enc_addr[0] == mb_addr[0]) {
				list_del(&ref_buf->list);
				enc->ref_queue_cnt--;

				vb2_buffer_done(&ref_buf->vb, VB2_BUF_STATE_DONE);
				break;
			}
		}
	} else if (ctx->src_queue_cnt_nal_q > 0) {
		src_buf = list_entry(ctx->src_queue_nal_q.next, struct s5p_mfc_buf, list);

		if (src_buf->used) {
			list_del(&src_buf->list);
			ctx->src_queue_cnt_nal_q--;
			list_add_tail(&src_buf->list, &enc->ref_queue);
			enc->ref_queue_cnt++;
			mfc_debug(2, "NAL Q: no output, src_queue_nal_q -> ref_queue, index:%d\n",
					src_buf->vb.v4l2_buf.index);
		}
		/* slice_type = 4 && strm_size = 0, skipped enable
		   should be considered */
		if ((slice_type == -1) && (strm_size == 0)) {
			ctx->state = MFCINST_RUNNING_NO_OUTPUT;
			dst_buf = list_entry(ctx->dst_queue_nal_q.next,
					struct s5p_mfc_buf, list);
			dst_buf->used = 0;
			list_del(&dst_buf->list);
			ctx->dst_queue_cnt_nal_q--;
			list_add(&dst_buf->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;
			mfc_debug(2, "NAL Q: no output, dst_queue_nal_q -> dst_queue, index:%d\n",
					dst_buf->vb.v4l2_buf.index);
		}

		mfc_debug(2, "NAL Q: slice_type: %d, ctx->state: %d\n", slice_type, ctx->state);
		mfc_debug(2, "NAL Q: enc src count: %d, enc ref count: %d\n",
			  ctx->src_queue_cnt, enc->ref_queue_cnt);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);
	mfc_debug_leave();
	return;
}

int s5p_mfc_nal_q_get_out_buf(struct s5p_mfc_dev *dev, EncoderOutputStr *pOutStr)
{
	struct s5p_mfc_ctx *ctx;
	struct s5p_mfc_enc *enc;
	int ctx_num;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("NAL Q: no mfc device to run\n");
		return -EINVAL;
	}

	ctx_num = s5p_mfc_nal_q_find_ctx(dev, pOutStr);
	if (ctx_num < 0) {
		mfc_err("NAL Q: Can't find ctx in nal q\n");
		return -EINVAL;
	}

	ctx = dev->ctx[ctx_num];
	if (!ctx) {
		mfc_err("NAL Q: no mfc context to run\n");
		return -EINVAL;
	}

	if (ctx->type == MFCINST_ENCODER) {
		enc = ctx->enc_priv;
		if (!enc) {
			mfc_err("NAL Q: no mfc encoder to run\n");
			return -EINVAL;
		}
		s5p_mfc_nal_q_post_frame_enc(ctx, pOutStr);
	}

	mfc_debug_leave();
	return 0;
}

/*
  * This function should be called in NAL_Q_STATE_INITIALIZED or NAL_Q_STATE_STARTED state.
  */
int s5p_mfc_nal_q_queue_in_buf(struct s5p_mfc_dev *dev,
	nal_queue_in_handle *nal_q_in_handle)
{
	unsigned int input_count = 0;
	unsigned int input_exe_count = 0;
	int input_diff = 0;
	int ret = 0;
#ifdef NAL_Q_DUMP
	unsigned int index = 0;
	int *tmp;
	EncoderInputStr *pStr;
#endif
	mfc_debug_enter();

	if (!nal_q_in_handle || !nal_q_in_handle->nal_q_in_addr ||
		((nal_q_in_handle->nal_q_handle->nal_q_state != NAL_Q_STATE_INITIALIZED) &&
		(nal_q_in_handle->nal_q_handle->nal_q_state != NAL_Q_STATE_STARTED))) {
		mfc_err("NAL Q: There is no handle or state is wrong\n");
		return -EINVAL;
	}

	input_count = MFC_READL(S5P_FIMV_NAL_QUEUE_INPUT_COUNT);
	input_exe_count = MFC_READL(S5P_FIMV_NAL_QUEUE_INPUT_EXE_COUNT);
	input_diff = input_count - input_exe_count;

	/* meaning of the variable input_diff
	* 0: 				number of available slots = NAL_Q_IN_QUEUE_SIZE
	* 1: 				number of available slots = NAL_Q_IN_QUEUE_SIZE - 1
	* ...
	* NAL_Q_IN_QUEUE_SIZE-1:	number of available slots = 1
	* NAL_Q_IN_QUEUE_SIZE:		number of available slots = 0
	*/

	mfc_debug(2, "NAL Q: input_count = %d\n", input_count);
	mfc_debug(2, "NAL Q: input_exe_count = %d\n", input_exe_count);
	mfc_debug(2, "NAl Q: input_diff = %d\n", input_diff);
	if ((input_diff < 0) || (input_diff >= NAL_Q_IN_QUEUE_SIZE)) {
		mfc_err("NAL Q: No available input slot(%d)\n", input_diff);
		return -EINVAL;
	}

#ifdef NAL_Q_DUMP
	index = input_count % NAL_Q_IN_QUEUE_SIZE;
	pStr = &(nal_q_in_handle->nal_q_in_addr->entry[index].enc);
	tmp = (int *)pStr;
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4, tmp, 256, false);
	printk("...\n");
#endif
	input_count++;
	MFC_WRITEL(input_count, S5P_FIMV_NAL_QUEUE_INPUT_COUNT);

	mfc_debug_leave();
	return ret;
}

/*
  * This function should be called in NAL_Q_STATE_STARTED state.
  */
EncoderOutputStr *s5p_mfc_nal_q_dequeue_out_buf(struct s5p_mfc_dev *dev,
	nal_queue_out_handle *nal_q_out_handle, unsigned int *reason)
{
	unsigned int output_count = 0;
	unsigned int output_exe_count = 0;
	int output_diff = 0;
	unsigned int index = 0;
	EncoderOutputStr *pStr = NULL;
#ifdef NAL_Q_DUMP
	int *tmp;
#endif
	mfc_debug_enter();

	if (!nal_q_out_handle || !nal_q_out_handle->nal_q_out_addr) {
		mfc_err("NAL Q: There is no handle\n");
		return pStr;
	}

	output_count = MFC_READL(S5P_FIMV_NAL_QUEUE_OUTPUT_COUNT);
	output_exe_count = nal_q_out_handle->out_exe_count;
	output_diff = output_count - output_exe_count;

	/* meaning of the variable output_diff
	* 0:				number of output slots = 0
	* 1:				number of output slots = 1
	* ...
	* NAL_Q_OUT_QUEUE_SIZE-1:	number of output slots = NAL_Q_OUT_QUEUE_SIZE - 1
	* NAL_Q_OUT_QUEUE_SIZE:		number of output slots = NAL_Q_OUT_QUEUE_SIZE
	*/

	mfc_debug(2, "NAL Q: output_diff = %d\n", output_diff);
	if ((output_diff <= 0) || (output_diff > NAL_Q_OUT_QUEUE_SIZE)) {
		mfc_err("NAL Q: No available output slot(%d)\n", output_diff);
		return pStr;
	}

	index = output_exe_count % NAL_Q_OUT_QUEUE_SIZE;
	pStr = &(nal_q_out_handle->nal_q_out_addr->entry[index].enc);
#ifdef NAL_Q_DUMP
	tmp = (int *)pStr;
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4, tmp, 256, false);
	printk("...\n");
#endif
	nal_q_out_handle->out_exe_count++;

	if (pStr->ErrorCode) {
		*reason = S5P_FIMV_R2H_CMD_ERR_RET;
		mfc_err("NAL Q: Error : %d\n", pStr->ErrorCode);
	}
	if (pStr->startcode != 0xBBBBBBBB)
		mfc_err("NAL Q: start code is not 0xBBBBBBBB !!\n");

	mfc_debug_leave();
	return pStr;
}

/* not used function - only for reference sfr <-> structure */
void s5p_mfc_nal_q_fill_DecoderInputStr(struct s5p_mfc_dev *dev, DecoderInputStr *pStr)
{
	pStr->startcode			= 0xAAAAAAAA; // Decoder input start
//	pStr->CommandId			= MFC_READL(S5P_FIMV_HOST2RISC_CMD);		// 0x1100
	pStr->InstanceId		= MFC_READL(S5P_FIMV_INSTANCE_ID);		// 0xF008
	pStr->PictureTag		= MFC_READL(S5P_FIMV_D_PICTURE_TAG);		// 0xF5C8
	pStr->CpbBufferAddr		= MFC_READL(S5P_FIMV_D_CPB_BUFFER_ADDR);	// 0xF5B0
	pStr->CpbBufferSize		= MFC_READL(S5P_FIMV_D_CPB_BUFFER_SIZE);	// 0xF5B4
	pStr->CpbBufferOffset		= MFC_READL(S5P_FIMV_D_CPB_BUFFER_OFFSET);	// 0xF5C0
	pStr->StreamDataSize		= MFC_READL(S5P_FIMV_D_STREAM_DATA_SIZE);	// 0xF5D0
	pStr->AvailableDpbFlagUpper	= MFC_READL(S5P_FIMV_D_AVAILABLE_DPB_FLAG_UPPER);// 0xF5B8
	pStr->AvailableDpbFlagLower	= MFC_READL(S5P_FIMV_D_AVAILABLE_DPB_FLAG_LOWER);// 0xF5BC
	pStr->DynamicDpbFlagUpper	= MFC_READL(S5P_FIMV_D_DYNAMIC_DPB_FLAG_UPPER);	// 0xF5D4
	pStr->DynamicDpbFlagLower	= MFC_READL(S5P_FIMV_D_DYNAMIC_DPB_FLAG_LOWER);	// 0xF5D8
	pStr->FirstPlaneDpb		= MFC_READL(S5P_FIMV_D_FIRST_PLANE_DPB0);	// 0xF160+(index*4)
	pStr->SecondPlaneDpb		= MFC_READL(S5P_FIMV_D_SECOND_PLANE_DPB0);	// 0xF260+(index*4)
	pStr->ThirdPlaneDpb		= MFC_READL(S5P_FIMV_D_THIRD_PLANE_DPB0);	// 0xF360+(index*4)
	pStr->FirstPlaneDpbSize		= MFC_READL(S5P_FIMV_D_FIRST_PLANE_DPB_SIZE);	// 0xF144
	pStr->SecondPlaneDpbSize	= MFC_READL(S5P_FIMV_D_SECOND_PLANE_DPB_SIZE);	// 0xF148
	pStr->ThirdPlaneDpbSize		= MFC_READL(S5P_FIMV_D_THIRD_PLANE_DPB_SIZE);	// 0xF14C
	pStr->NalStartOptions		= MFC_READL(0xF5AC);// S5P_FIMV_D_NAL_START_OPTIONS 0xF5AC
	pStr->FirstPlaneStrideSize	= MFC_READL(S5P_FIMV_D_FIRST_PLANE_DPB_STRIDE_SIZE);// 0xF138
	pStr->SecondPlaneStrideSize	= MFC_READL(S5P_FIMV_D_SECOND_PLANE_DPB_STRIDE_SIZE);// 0xF13C
	pStr->ThirdPlaneStrideSize	= MFC_READL(S5P_FIMV_D_THIRD_PLANE_DPB_STRIDE_SIZE);// 0xF140
	pStr->FirstPlane2BitDpbSize	= MFC_READL(S5P_FIMV_D_FIRST_PLANE_2BIT_DPB_SIZE);// 0xF578
	pStr->SecondPlane2BitDpbSize	= MFC_READL(S5P_FIMV_D_SECOND_PLANE_2BIT_DPB_SIZE);// 0xF57C
	pStr->FirstPlane2BitStrideSize	= MFC_READL(S5P_FIMV_D_FIRST_PLANE_2BIT_DPB_STRIDE_SIZE);// 0xF580
	pStr->SecondPlane2BitStrideSize	= MFC_READL(S5P_FIMV_D_SECOND_PLANE_2BIT_DPB_STRIDE_SIZE);// 0xF584
	pStr->ScratchBufAddr		= MFC_READL(S5P_FIMV_D_SCRATCH_BUFFER_ADDR);	// 0xF560
	pStr->ScratchBufSize		= MFC_READL(S5P_FIMV_D_SCRATCH_BUFFER_SIZE);	// 0xF564
}

void s5p_mfc_nal_q_flush_DecoderOutputStr(struct s5p_mfc_dev *dev, DecoderOutputStr *pStr)
{
	//pStr->startcode; // 0xAAAAAAAA; // Decoder output start
//	MFC_WRITEL(pStr->CommandId, S5P_FIMV_RISC2HOST_CMD);				// 0x1104
	MFC_WRITEL(pStr->InstanceId, S5P_FIMV_RET_INSTANCE_ID);				// 0xF070
	MFC_WRITEL(pStr->ErrorCode, S5P_FIMV_ERROR_CODE);				// 0xF074
	MFC_WRITEL(pStr->PictureTagTop, S5P_FIMV_D_RET_PICTURE_TAG_TOP);		// 0xF674
	MFC_WRITEL(pStr->PictureTimeTop, S5P_FIMV_D_RET_PICTURE_TIME_TOP);		// 0xF67C
	MFC_WRITEL(pStr->DisplayFrameWidth, S5P_FIMV_D_DISPLAY_FRAME_WIDTH);		// 0xF600
	MFC_WRITEL(pStr->DisplayFrameHeight, S5P_FIMV_D_DISPLAY_FRAME_HEIGHT);		// 0xF604
	MFC_WRITEL(pStr->DisplayStatus, S5P_FIMV_D_DISPLAY_STATUS);			// 0xF608
	MFC_WRITEL(pStr->DisplayFirstPlaneAddr, S5P_FIMV_D_DISPLAY_FIRST_PLANE_ADDR);	// 0xF60C
	MFC_WRITEL(pStr->DisplaySecondPlaneAddr, S5P_FIMV_D_DISPLAY_SECOND_PLANE_ADDR);	// 0xF610
	MFC_WRITEL(pStr->DisplayThirdPlaneAddr,S5P_FIMV_D_DISPLAY_THIRD_PLANE_ADDR);	// 0xF614
	MFC_WRITEL(pStr->DisplayFrameType, S5P_FIMV_D_DISPLAY_FRAME_TYPE);		// 0xF618
	MFC_WRITEL(pStr->DisplayCropInfo1, S5P_FIMV_D_DISPLAY_CROP_INFO1);		// 0xF61C
	MFC_WRITEL(pStr->DisplayCropInfo2, S5P_FIMV_D_DISPLAY_CROP_INFO2);		// 0xF620
	MFC_WRITEL(pStr->DisplayPictureProfile, S5P_FIMV_D_DISPLAY_PICTURE_PROFILE);	// 0xF624
	MFC_WRITEL(pStr->DisplayAspectRatio, S5P_FIMV_D_DISPLAY_ASPECT_RATIO);		// 0xF634
	MFC_WRITEL(pStr->DisplayExtendedAr, S5P_FIMV_D_DISPLAY_EXTENDED_AR);		// 0xF638
	MFC_WRITEL(pStr->DecodeNalSize, S5P_FIMV_D_DECODED_NAL_SIZE);			// 0xF664
	MFC_WRITEL(pStr->UsedDpbFlagUpper, S5P_FIMV_D_USED_DPB_FLAG_UPPER);		// 0xF720
	MFC_WRITEL(pStr->UsedDpbFlagLower, S5P_FIMV_D_USED_DPB_FLAG_LOWER);		// 0xF724
	MFC_WRITEL(pStr->FramePackSeiAvail, S5P_FIMV_D_SEI_AVAIL);			// 0xF6DC
	MFC_WRITEL(pStr->FramePackArrgmentId, S5P_FIMV_D_FRAME_PACK_ARRGMENT_ID);	// 0xF6E0
	MFC_WRITEL(pStr->FramePackSeiInfo, S5P_FIMV_D_FRAME_PACK_SEI_INFO);		// 0xF6E4
	MFC_WRITEL(pStr->FramePackGridPos, S5P_FIMV_D_FRAME_PACK_GRID_POS);		// 0xF6E8
	MFC_WRITEL(pStr->DisplayRecoverySeiInfo, S5P_FIMV_D_DISPLAY_RECOVERY_SEI_INFO);	// 0xF6EC
	MFC_WRITEL(pStr->H264Info, S5P_FIMV_D_H264_INFO);				// 0xF690
	MFC_WRITEL(pStr->DisplayFirstCrc, S5P_FIMV_D_DISPLAY_FIRST_PLANE_CRC);		// 0xF628
	MFC_WRITEL(pStr->DisplaySecondCrc, S5P_FIMV_D_DISPLAY_SECOND_PLANE_CRC);	// 0xF62C
	MFC_WRITEL(pStr->DisplayThirdCrc, S5P_FIMV_D_DISPLAY_THIRD_PLANE_CRC);		// 0xF630
	MFC_WRITEL(pStr->DisplayFirst2BitCrc, S5P_FIMV_D_DISPLAY_FIRST_PLANE_2BIT_CRC);	// 0xF6FC
	MFC_WRITEL(pStr->DisplaySecond2BitCrc, S5P_FIMV_D_DISPLAY_SECOND_PLANE_2BIT_CRC);// 0xF700
	MFC_WRITEL(pStr->DecodedFrameWidth, S5P_FIMV_D_DECODED_FRAME_WIDTH);		// 0xF63C
	MFC_WRITEL(pStr->DecodedFrameHeight, S5P_FIMV_D_DECODED_FRAME_HEIGHT);		// 0xF640
	MFC_WRITEL(pStr->DecodedStatus, S5P_FIMV_D_DECODED_STATUS);			// 0xF644
	MFC_WRITEL(pStr->DecodedFirstPlaneAddr, S5P_FIMV_D_DECODED_FIRST_PLANE_ADDR);	// 0xF648
	MFC_WRITEL(pStr->DecodedSecondPlaneAddr, S5P_FIMV_D_DECODED_SECOND_PLANE_ADDR);	// 0xF64C
	MFC_WRITEL(pStr->DecodedThirdPlaneAddr, S5P_FIMV_D_DECODED_THIRD_PLANE_ADDR);	// 0xF650
	MFC_WRITEL(pStr->DecodedFrameType, S5P_FIMV_D_DECODED_FRAME_TYPE);		// 0xF654
	MFC_WRITEL(pStr->DecodedCropInfo1, S5P_FIMV_D_DECODED_CROP_INFO1);		// 0xF658
	MFC_WRITEL(pStr->DecodedCropInfo2, S5P_FIMV_D_DECODED_CROP_INFO2);		// 0xF65C
	MFC_WRITEL(pStr->DecodedPictureProfile, S5P_FIMV_D_DECODED_PICTURE_PROFILE);	// 0xF660
	MFC_WRITEL(pStr->DecodedRecoverySeiInfo, S5P_FIMV_D_DECODED_RECOVERY_SEI_INFO);	// 0xF6F0
	MFC_WRITEL(pStr->DecodedFirstCrc, S5P_FIMV_D_DECODED_FIRST_PLANE_CRC);		// 0xF668
	MFC_WRITEL(pStr->DecodedSecondCrc, S5P_FIMV_D_DECODED_SECOND_PLANE_CRC);	// 0xF66C
	MFC_WRITEL(pStr->DecodedThirdCrc, S5P_FIMV_D_DECODED_THIRD_PLANE_CRC);		// 0xF670
	MFC_WRITEL(pStr->DecodedFirst2BitCrc, S5P_FIMV_D_DECODED_FIRST_PLANE_2BIT_CRC);	// 0xF704
	MFC_WRITEL(pStr->DecodedSecond2BitCrc, S5P_FIMV_D_DECODED_SECOND_PLANE_2BIT_CRC);// 0xF708
	MFC_WRITEL(pStr->PictureTagBot, S5P_FIMV_D_RET_PICTURE_TAG_BOT);		// 0xF678
	MFC_WRITEL(pStr->PictureTimeBot, S5P_FIMV_D_RET_PICTURE_TIME_BOT);		// 0xF680
}
