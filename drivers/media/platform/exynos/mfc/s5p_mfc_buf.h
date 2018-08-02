/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_buf.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver
 * Contains declarations of hw related functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S5P_MFC_BUF_H
#define __S5P_MFC_BUF_H __FILE__

#include "s5p_mfc_common.h"

#define MFC_NV12M_HALIGN                  512
#define MFC_NV12MT_HALIGN                 16
#define MFC_NV12MT_VALIGN                 16

void mfc_fill_dynamic_dpb(struct s5p_mfc_ctx *ctx, struct vb2_buffer *vb);
int s5p_mfc_set_dec_frame_buffer(struct s5p_mfc_ctx *ctx);
int s5p_mfc_set_dec_stream_buffer(struct s5p_mfc_ctx *ctx,
		struct s5p_mfc_buf *mfc_buf,
		unsigned int start_num_byte,
		unsigned int buf_size);

void s5p_mfc_set_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr[], int num_planes);
int s5p_mfc_set_enc_stream_buffer(struct s5p_mfc_ctx *ctx,
		struct s5p_mfc_buf *mfc_buf);
void s5p_mfc_get_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr[], int num_planes);
int s5p_mfc_set_enc_ref_buffer(struct s5p_mfc_ctx *mfc_ctx);

/* Memory allocation */
int s5p_mfc_alloc_codec_buffers(struct s5p_mfc_ctx *ctx);
void s5p_mfc_release_codec_buffers(struct s5p_mfc_ctx *ctx);

int s5p_mfc_alloc_instance_buffer(struct s5p_mfc_ctx *ctx);
void s5p_mfc_release_instance_buffer(struct s5p_mfc_ctx *ctx);
int s5p_mfc_alloc_dev_context_buffer(struct s5p_mfc_dev *dev);
void s5p_mfc_release_dev_context_buffer(struct s5p_mfc_dev *dev);
int s5p_mfc_alloc_enc_roi_buffer(struct s5p_mfc_ctx *ctx);
void s5p_mfc_release_enc_roi_buffer(struct s5p_mfc_ctx *ctx);

void s5p_mfc_dec_calc_dpb_size(struct s5p_mfc_ctx *ctx);
void s5p_mfc_enc_calc_src_size(struct s5p_mfc_ctx *ctx);

#endif /* __S5P_MFC_BUF_H */
