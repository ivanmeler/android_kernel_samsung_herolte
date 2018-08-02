/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_buf.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Samsung MFC (Multi Function Codec - FIMV) driver
 * This file contains hw related functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "s5p_mfc_buf.h"

#include "s5p_mfc_cmd.h"
#include "s5p_mfc_mem.h"

/* Scratch buffer size for MFC v9.0 */
#define DEC_V90_STATIC_BUFFER_SIZE	20480

#define CPB_GAP			512
#define set_strm_size_max(cpb_max)	((cpb_max) - CPB_GAP)

/* Allocate codec buffers */
int s5p_mfc_alloc_codec_buffers(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_enc *enc;
	unsigned int mb_width, mb_height;
	unsigned int lcu_width = 0, lcu_height = 0;
	void *alloc_ctx;
	int i;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	enc = ctx->enc_priv;
	alloc_ctx = dev->alloc_ctx;

	mb_width = mb_width(ctx->img_width);
	mb_height = mb_height(ctx->img_height);

	if (ctx->type == MFCINST_DECODER) {
		lcu_width = dec_lcu_width(ctx->img_width);
		lcu_height = dec_lcu_height(ctx->img_height);
		for (i = 0; i < ctx->raw_buf.num_planes; i++)
			mfc_debug(2, "Plane[%d] size:%d\n",
					i, ctx->raw_buf.plane_size[i]);
		mfc_debug(2, "MV size: %d, Totals bufs: %d\n",
				ctx->mv_size, dec->total_dpb_count);
	} else if (ctx->type == MFCINST_ENCODER) {
		if (!IS_MFCv78(dev) || !IS_MFCv10X(dev)) {
			enc->tmv_buffer_size =
				ENC_TMV_SIZE(mb_width, mb_height);
			enc->tmv_buffer_size =
				ALIGN(enc->tmv_buffer_size, 32) * 2;
		} else {
			enc->tmv_buffer_size = 0;
		}
		if (IS_MFCv9X(dev)) {
			lcu_width = enc_lcu_width(ctx->img_width);
			lcu_height = enc_lcu_height(ctx->img_height);
			if (ctx->codec_mode != S5P_FIMV_CODEC_HEVC_ENC) {
				enc->luma_dpb_size =
					ALIGN((mb_width * mb_height) * 256, 256);
				enc->chroma_dpb_size =
					ALIGN((mb_width * mb_height) * 128, 256);
				enc->me_buffer_size =
					(ENC_ME_SIZE(ctx->img_width, ctx->img_height,
						mb_width, mb_height));
			} else {
				enc->luma_dpb_size =
					ALIGN((((lcu_width * lcu_height) * 1024) + 64), 256);
				enc->chroma_dpb_size =
					ALIGN((((lcu_width * lcu_height) * 512) + 64), 256);
				enc->me_buffer_size =
					(ENC_HEVC_ME_SIZE(lcu_width, lcu_height));
			}
			enc->me_buffer_size = ALIGN(enc->me_buffer_size, 256);
		} else if (IS_MFCv10X(dev)) {
			lcu_width = enc_lcu_width(ctx->img_width);
			lcu_height = enc_lcu_height(ctx->img_height);
			if (ctx->codec_mode != S5P_FIMV_CODEC_HEVC_ENC &&
				ctx->codec_mode != S5P_FIMV_CODEC_VP9_ENC) {
				enc->luma_dpb_size =
					ALIGN((((mb_width * 16) + 63) / 64) * 64
						* (((mb_height * 16) + 31) / 32)
						* 32 + 64, 64);
				enc->chroma_dpb_size =
					ALIGN((((mb_width * 16) + 63) / 64)
						* 64 * (mb_height * 8) + 64, 64);
			} else {
				enc->luma_dpb_size =
					ALIGN((((lcu_width * 32 ) + 63 ) / 64) * 64
						* (((lcu_height * 32) + 31) / 32)
						* 32 + 64, 64);
				enc->chroma_dpb_size =
					ALIGN((((lcu_width * 32) + 63) / 64)
						* 64 * (lcu_height * 16) + 64, 64);
			}
		} else {
			enc->luma_dpb_size = ALIGN((mb_width * mb_height) * 256, 256);
			enc->chroma_dpb_size = ALIGN((mb_width * mb_height) * 128, 256);
			enc->me_buffer_size =
				(ENC_ME_SIZE(ctx->img_width, ctx->img_height,
					     mb_width, mb_height));
			enc->me_buffer_size = ALIGN(enc->me_buffer_size, 256);
		}

		mfc_debug(2, "recon luma size: %zu chroma size: %zu\n",
			  enc->luma_dpb_size, enc->chroma_dpb_size);
	} else {
		return -EINVAL;
	}

	/* Codecs have different memory requirements */
	switch (ctx->codec_mode) {
	case S5P_FIMV_CODEC_H264_DEC:
	case S5P_FIMV_CODEC_H264_MVC_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_H264_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv8X(dev))
			ctx->scratch_buf_size =
				DEC_V80_H264_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv9X(dev) || IS_MFCv10X(dev))
			mfc_debug(2, "Use min scratch buffer size \n");
		else
			ctx->scratch_buf_size =
				DEC_V65_H264_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size =
			ctx->scratch_buf_size +
			(dec->mv_count * ctx->mv_size);
		break;
	case S5P_FIMV_CODEC_MPEG4_DEC:
	case S5P_FIMV_CODEC_FIMV1_DEC:
	case S5P_FIMV_CODEC_FIMV2_DEC:
	case S5P_FIMV_CODEC_FIMV3_DEC:
	case S5P_FIMV_CODEC_FIMV4_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv8X(dev))
			ctx->scratch_buf_size =
				DEC_V80_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv9X(dev) || IS_MFCv10X(dev))
			mfc_debug(2, "Use min scratch buffer size \n");
		else
			ctx->scratch_buf_size =
				DEC_V65_MPEG4_SCRATCH_SIZE(mb_width, mb_height);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_VC1RCV_DEC:
	case S5P_FIMV_CODEC_VC1_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_VC1_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv9X(dev) || IS_MFCv10X(dev))
			mfc_debug(2, "Use min scratch buffer size \n");
		else
			ctx->scratch_buf_size =
				DEC_V65_VC1_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_MPEG2_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_MPEG2_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv9X(dev) || IS_MFCv10X(dev))
			mfc_debug(2, "Use min scratch buffer size \n");
		else
			ctx->scratch_buf_size =
				DEC_V65_MPEG2_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_H263_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv8X(dev))
			ctx->scratch_buf_size =
				DEC_V80_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv9X(dev) || IS_MFCv10X(dev))
			mfc_debug(2, "Use min scratch buffer size \n");
		else
			ctx->scratch_buf_size =
				DEC_V65_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_VP8_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_VP8_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv8X(dev))
			ctx->scratch_buf_size =
				DEC_V80_VP8_SCRATCH_SIZE(mb_width, mb_height);
		else if (IS_MFCv9X(dev) || IS_MFCv10X(dev))
			mfc_debug(2, "Use min scratch buffer size \n");
		else
			ctx->scratch_buf_size =
				DEC_V65_VP8_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_VP9_DEC:
		if (IS_MFCv9X(dev) || IS_MFCv10X(dev))
			mfc_debug(2, "Use min scratch buffer size \n");
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size =
			ctx->scratch_buf_size +
			DEC_V90_STATIC_BUFFER_SIZE;
		break;
	case S5P_FIMV_CODEC_HEVC_DEC:
		if (IS_MFCv9X(dev) || IS_MFCv10X(dev))
			mfc_debug(2, "Use min scratch buffer size \n");
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size =
			ctx->scratch_buf_size +
			(dec->mv_count * ctx->mv_size);
		break;
	case S5P_FIMV_CODEC_H264_ENC:
		if (mfc_version(dev) == 0x61) {
			ctx->scratch_buf_size =
				ENC_V61_H264_SCRATCH_SIZE(mb_width, mb_height);
		} else if (IS_MFCv8X(dev)) {
			ctx->scratch_buf_size =
				ENC_V80_H264_SCRATCH_SIZE(mb_width, mb_height);
		} else if (IS_MFCv9X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
		} else if (IS_MFCv10X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
			enc->me_buffer_size =
				ALIGN(ENC_V100_H264_ME_SIZE(mb_width, mb_height), 16);
		} else {
			ctx->scratch_buf_size =
				ENC_V65_H264_SCRATCH_SIZE(mb_width, mb_height);
		}
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));

		ctx->scratch_buf_size = max(ctx->scratch_buf_size, ctx->min_scratch_buf_size);
		ctx->min_scratch_buf_size = 0;
		break;
	case S5P_FIMV_CODEC_MPEG4_ENC:
	case S5P_FIMV_CODEC_H263_ENC:
		if (mfc_version(dev) == 0x61) {
			ctx->scratch_buf_size =
				ENC_V61_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		} else if (IS_MFCv9X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
		} else if (IS_MFCv10X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
			enc->me_buffer_size =
				ALIGN(ENC_V100_MPEG4_ME_SIZE(mb_width, mb_height), 16);
		} else {
			ctx->scratch_buf_size =
				ENC_V65_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		}
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case S5P_FIMV_CODEC_VP8_ENC:
		if (IS_MFCv8X(dev)) {
			ctx->scratch_buf_size =
				ENC_V80_VP8_SCRATCH_SIZE(mb_width, mb_height);
		} else if (IS_MFCv9X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
		} else if (IS_MFCv10X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
			enc->me_buffer_size =
				ALIGN(ENC_V100_VP8_ME_SIZE(mb_width, mb_height), 16);
		} else {
			ctx->scratch_buf_size =
				ENC_V70_VP8_SCRATCH_SIZE(mb_width, mb_height);
		}

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case S5P_FIMV_CODEC_VP9_ENC:
		if (IS_MFCv9X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
		} else if (IS_MFCv10X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
			enc->me_buffer_size =
				ALIGN(ENC_V100_VP9_ME_SIZE(lcu_width, lcu_height), 16);
		} else {
			mfc_err_ctx("MFC 0x%x is not supported codec type(%d)\n",
						mfc_version(dev), ctx->codec_mode);
			break;
		}

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case S5P_FIMV_CODEC_HEVC_ENC:
		if (IS_MFCv9X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
		} else if (IS_MFCv10X(dev)) {
			mfc_debug(2, "Use min scratch buffer size \n");
			enc->me_buffer_size =
				ALIGN(ENC_V100_HEVC_ME_SIZE(lcu_width, lcu_height), 16);
		} else {
			mfc_err_ctx("MFC 0x%x is not supported codec type(%d)\n",
						mfc_version(dev), ctx->codec_mode);
			break;
		}
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->codec_buf_size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	default:
		break;
	}

	if (ctx->is_drm)
		alloc_ctx = dev->alloc_ctx_drm;

	/* Allocate only if memory from bank 1 is necessary */
	if (ctx->codec_buf_size > 0) {
		ctx->codec_buf = s5p_mfc_mem_alloc_priv(
				alloc_ctx, ctx->codec_buf_size);
		if (IS_ERR(ctx->codec_buf)) {
			ctx->codec_buf = NULL;
			mfc_err_ctx("Allocating codec buffer failed.\n");
			return -ENOMEM;
		}
		ctx->codec_buf_phys = s5p_mfc_mem_daddr_priv(ctx->codec_buf);
		ctx->codec_buf_virt = s5p_mfc_mem_vaddr_priv(ctx->codec_buf);
		mfc_debug(2, "Codec buf alloc, ctx: %d, size: %ld, addr: 0x%lx\n",
			ctx->num, ctx->codec_buf_size, ctx->codec_buf_phys);
		if (!ctx->codec_buf_virt) {
			s5p_mfc_mem_free_priv(ctx->codec_buf);
			ctx->codec_buf = NULL;
			ctx->codec_buf_phys = 0;

			mfc_err_ctx("Get vaddr for codec buffer failed.\n");
			return -ENOMEM;
		}
	}

	if (ctx->is_drm) {
		int ret = 0;
		phys_addr_t addr = s5p_mfc_mem_phys_addr(ctx->codec_buf);
		ret = exynos_smc(SMC_DRM_SECBUF_CFW_PROT, addr,
					ctx->codec_buf_size, dev->id);
		if (ret != DRMDRV_OK) {
			mfc_err_ctx("failed to CFW PROT (%#x)\n", ret);
			return ret;
		}
	}

	mfc_debug_leave();

	return 0;
}

/* Release buffers allocated for codec */
void s5p_mfc_release_codec_buffers(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err_ctx("no mfc device to run\n");
		return;
	}

	if (ctx->codec_buf) {
		if (ctx->is_drm) {
			int ret = 0;
			phys_addr_t addr = s5p_mfc_mem_phys_addr(ctx->codec_buf);
			ret = exynos_smc(SMC_DRM_SECBUF_CFW_UNPROT, addr,
					ctx->codec_buf_size, dev->id);
			if (ret != DRMDRV_OK)
				mfc_err_ctx("failed to CFW PROT (%#x)\n", ret);
		}

		s5p_mfc_mem_free_priv(ctx->codec_buf);
		ctx->codec_buf = 0;
		ctx->codec_buf_phys = 0;
		ctx->codec_buf_size = 0;
	}
}

/* Allocate memory for instance data buffer */
int s5p_mfc_alloc_instance_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf_size_v6 *buf_size;
	void *alloc_ctx;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	buf_size = dev->variant->buf_size->buf;
	alloc_ctx = dev->alloc_ctx;

	switch (ctx->codec_mode) {
	case S5P_FIMV_CODEC_H264_DEC:
	case S5P_FIMV_CODEC_H264_MVC_DEC:
	case S5P_FIMV_CODEC_HEVC_DEC:
		ctx->ctx_buf_size = buf_size->h264_dec_ctx;
		break;
	case S5P_FIMV_CODEC_MPEG4_DEC:
	case S5P_FIMV_CODEC_H263_DEC:
	case S5P_FIMV_CODEC_VC1RCV_DEC:
	case S5P_FIMV_CODEC_VC1_DEC:
	case S5P_FIMV_CODEC_MPEG2_DEC:
	case S5P_FIMV_CODEC_VP8_DEC:
	case S5P_FIMV_CODEC_VP9_DEC:
	case S5P_FIMV_CODEC_FIMV1_DEC:
	case S5P_FIMV_CODEC_FIMV2_DEC:
	case S5P_FIMV_CODEC_FIMV3_DEC:
	case S5P_FIMV_CODEC_FIMV4_DEC:
		ctx->ctx_buf_size = buf_size->other_dec_ctx;
		break;
	case S5P_FIMV_CODEC_H264_ENC:
		ctx->ctx_buf_size = buf_size->h264_enc_ctx;
		break;
	case S5P_FIMV_CODEC_HEVC_ENC:
		ctx->ctx_buf_size = buf_size->hevc_enc_ctx;
		break;
	case S5P_FIMV_CODEC_MPEG4_ENC:
	case S5P_FIMV_CODEC_H263_ENC:
	case S5P_FIMV_CODEC_VP8_ENC:
	case S5P_FIMV_CODEC_VP9_ENC:
		ctx->ctx_buf_size = buf_size->other_enc_ctx;
		break;
	default:
		ctx->ctx_buf_size = 0;
		mfc_err_ctx("Codec type(%d) should be checked!\n", ctx->codec_mode);
		break;
	}

	if (ctx->is_drm)
		alloc_ctx = dev->alloc_ctx_drm;

	ctx->ctx.alloc = s5p_mfc_mem_alloc_priv(alloc_ctx, ctx->ctx_buf_size);
	if (IS_ERR(ctx->ctx.alloc)) {
		ctx->ctx.alloc = NULL;
		mfc_err_ctx("Allocating context buffer failed.\n");
		return PTR_ERR(ctx->ctx.alloc);
	}

	ctx->ctx.ofs = s5p_mfc_mem_daddr_priv(ctx->ctx.alloc);
	ctx->ctx.virt = s5p_mfc_mem_vaddr_priv(ctx->ctx.alloc);
	mfc_debug(2, "Instance buf alloc, ctx: %d, size: %d, addr:0x%lx\n",
			ctx->num, ctx->ctx_buf_size, ctx->ctx.ofs);
	if (!ctx->ctx.virt) {
		s5p_mfc_mem_free_priv(ctx->ctx.alloc);
		ctx->ctx.alloc = NULL;
		ctx->ctx.ofs = 0;
		ctx->ctx.virt = NULL;

		mfc_err_ctx("Remapping context buffer failed.\n");
		return -ENOMEM;
	}

	if (ctx->is_drm) {
		int ret = 0;
		phys_addr_t addr = s5p_mfc_mem_phys_addr(ctx->ctx.alloc);
		ret = exynos_smc(SMC_DRM_SECBUF_CFW_PROT, addr,
					ctx->ctx_buf_size, dev->id);
		if (ret != DRMDRV_OK) {
			mfc_err_ctx("failed to CFW PROT (%#x)\n", ret);
			return ret;
		}
	}

	mfc_debug_leave();

	return 0;
}

/* Release instance buffer */
void s5p_mfc_release_instance_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err_ctx("no mfc device to run\n");
		return;
	}

	if (ctx->ctx.virt) {
		if (ctx->is_drm) {
			int ret = 0;
			phys_addr_t addr = s5p_mfc_mem_phys_addr(ctx->ctx.alloc);
			ret = exynos_smc(SMC_DRM_SECBUF_CFW_UNPROT, addr,
					ctx->ctx_buf_size, dev->id);
			if (ret != DRMDRV_OK)
				mfc_err_ctx("failed to CFW PROT (%#x)\n", ret);
		}

		s5p_mfc_mem_free_priv(ctx->ctx.alloc);
		ctx->ctx.alloc = NULL;
		ctx->ctx.ofs = 0;
		ctx->ctx.virt = NULL;
	}

	mfc_debug_leave();
}

/* Allocation for internal usage */
static int mfc_alloc_dev_context_buffer(struct s5p_mfc_dev *dev,
					enum mfc_buf_usage_type buf_type)
{
	struct s5p_mfc_buf_size_v6 *buf_size;
	void *alloc_ctx;
	struct s5p_mfc_extra_buf *ctx_buf;
	int firmware_size;
	unsigned long fw_ofs;

	mfc_debug_enter();
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	buf_size = dev->variant->buf_size->buf;
	alloc_ctx = dev->alloc_ctx;
	ctx_buf = &dev->ctx_buf;
	fw_ofs = dev->fw_info.ofs;

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (buf_type == MFCBUF_DRM) {
		alloc_ctx = dev->alloc_ctx_drm;
		ctx_buf = &dev->ctx_buf_drm;
		fw_ofs = dev->drm_fw_info.ofs;
	}
#endif

	firmware_size = dev->variant->buf_size->firmware_code;

	ctx_buf->alloc = NULL;
	ctx_buf->virt = 0;
	ctx_buf->ofs = fw_ofs + firmware_size;

	mfc_debug_leave();

	return 0;
}

/* Wrapper : allocate context buffers for SYS_INIT */
int s5p_mfc_alloc_dev_context_buffer(struct s5p_mfc_dev *dev)
{
	int ret = 0;

	ret = mfc_alloc_dev_context_buffer(dev, MFCBUF_NORMAL);
	if (ret)
		return ret;
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (dev->drm_fw_status) {
		ret = mfc_alloc_dev_context_buffer(dev, MFCBUF_DRM);
		if (ret)
			return ret;
	}
#endif

	return ret;
}

/* Release context buffers for SYS_INIT */
static void mfc_release_dev_context_buffer(struct s5p_mfc_dev *dev,
					enum mfc_buf_usage_type buf_type)
{
	struct s5p_mfc_extra_buf *ctx_buf;
	struct s5p_mfc_buf_size_v6 *buf_size;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	ctx_buf = &dev->ctx_buf;
	buf_size = dev->variant->buf_size->buf;
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (buf_type == MFCBUF_DRM)
		ctx_buf = &dev->ctx_buf_drm;
#endif

	if (ctx_buf->virt) {
		s5p_mfc_mem_free_priv(ctx_buf->alloc);
		ctx_buf->alloc = NULL;
		ctx_buf->ofs = 0;
		ctx_buf->virt = NULL;
	} else {
		/* In case of using FW region for common context buffer */
		if (ctx_buf->ofs)
			ctx_buf->ofs = 0;
	}
}

/* Release context buffers for SYS_INIT */
void s5p_mfc_release_dev_context_buffer(struct s5p_mfc_dev *dev)
{
	mfc_release_dev_context_buffer(dev, MFCBUF_NORMAL);

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	mfc_release_dev_context_buffer(dev, MFCBUF_DRM);
#endif
}

/* Allocation buffer of ROI macroblock information */
int mfc_alloc_enc_roi_buffer(struct s5p_mfc_ctx *ctx, struct s5p_mfc_extra_buf *roi_buf)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_buf_size_v6 *buf_size = dev->variant->buf_size->buf;
	void *alloc_ctx;

	alloc_ctx = dev->alloc_ctx;
	roi_buf->alloc = s5p_mfc_mem_alloc_priv(alloc_ctx,
			buf_size->shared_buf);
	if (IS_ERR(roi_buf->alloc)) {
		mfc_err("failed to allocate shared memory\n");
		return PTR_ERR(roi_buf->alloc);
	}

	roi_buf->ofs = s5p_mfc_mem_daddr_priv(roi_buf->alloc);
	roi_buf->virt = s5p_mfc_mem_vaddr_priv(roi_buf->alloc);
	if (!roi_buf->virt) {
		s5p_mfc_mem_free_priv(roi_buf->alloc);
		roi_buf->ofs = 0;
		roi_buf->alloc = NULL;

		mfc_err("failed to virt addr of shared memory\n");
		return -ENOMEM;
	}

	memset((void *)roi_buf->virt, 0, buf_size->shared_buf);
	s5p_mfc_mem_clean_priv(roi_buf->alloc, roi_buf->virt, 0,
			buf_size->shared_buf);

	return 0;
}

/* Wrapper : allocation ROI buffers */
int s5p_mfc_alloc_enc_roi_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	int i;

	for (i = 0; i < MFC_MAX_EXTRA_BUF; i++) {
		if (mfc_alloc_enc_roi_buffer(ctx, &enc->roi_buf[i]) < 0) {
			mfc_err("Remapping shared mem buffer failed.\n");
			return -ENOMEM;
		}
	}

	return 0;
}

/* Release buffer of ROI macroblock information */
void s5p_mfc_release_enc_roi_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	int i;

	for (i = 0; i < MFC_MAX_EXTRA_BUF; i++) {
		if (enc->roi_buf[i].alloc) {
			s5p_mfc_mem_free_priv(enc->roi_buf[i].alloc);
			enc->roi_buf[i].alloc = NULL;
			enc->roi_buf[i].ofs = 0;
			enc->roi_buf[i].virt = NULL;
		}
	}
}

static int calc_plane(int width, int height, int is_tiled)
{
	int mbX, mbY;

	mbX = (width + 15)/16;
	mbY = (height + 15)/16;

	/* Alignment for interlaced processing */
	if (is_tiled)
		mbY = (mbY + 1) / 2 * 2;

	return (mbX * 16) * (mbY * 16);
}

void mfc_fill_dynamic_dpb(struct s5p_mfc_ctx *ctx, struct vb2_buffer *vb)
{
	struct s5p_mfc_raw_info *raw;
	int i;
	u8 color[3] = { 0x0, 0x80, 0x80 };
	unsigned char *dpb_vir;

	raw = &ctx->raw_buf;
	if (ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12N) {
		dpb_vir = vb2_plane_vaddr(vb, 0);
		for (i = 0; i < raw->num_planes; i++) {
			if (dpb_vir)
				memset(dpb_vir, color[i], raw->plane_size[i]);
			dpb_vir = NV12N_CBCR_BASE(dpb_vir, ctx->img_width,
						ctx->img_height);
		}
	} else if (ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12N_10B) {
		dpb_vir = vb2_plane_vaddr(vb, 0);
		for (i = 0; i < raw->num_planes; i++) {
			if (dpb_vir)
				memset(dpb_vir, color[i], raw->plane_size[i]);
			dpb_vir = NV12N_10B_CBCR_BASE(dpb_vir, ctx->img_width,
						ctx->img_height);
		}
	} else if (ctx->dst_fmt->fourcc == V4L2_PIX_FMT_YUV420N) {
		dpb_vir = vb2_plane_vaddr(vb, 0);
		for (i = 0; i < raw->num_planes; i++) {
			if (dpb_vir)
				memset(dpb_vir, color[i], raw->plane_size[i]);
			if (i == 0)
				dpb_vir = YUV420N_CB_BASE(dpb_vir, ctx->img_width,
						ctx->img_height);
			if (i == 1)
				dpb_vir = YUV420N_CR_BASE(dpb_vir, ctx->img_width,
						ctx->img_height);
		}
	} else {
		for (i = 0; i < raw->num_planes; i++) {
			dpb_vir = vb2_plane_vaddr(vb, i);
			if (dpb_vir)
				memset(dpb_vir, color[i], raw->plane_size[i]);
		}
	}
	s5p_mfc_mem_clean_vb(vb, vb->num_planes);
}

static void set_linear_stride_size(struct s5p_mfc_ctx *ctx,
				struct s5p_mfc_fmt *fmt)
{
	struct s5p_mfc_raw_info *raw;
	int i;

	raw = &ctx->raw_buf;

	switch (fmt->fourcc) {
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV420N:
	case V4L2_PIX_FMT_YVU420M:
		raw->stride[0] = ALIGN(ctx->img_width, 16);
		raw->stride[1] = ALIGN(raw->stride[0] >> 1, 16);
		raw->stride[2] = ALIGN(raw->stride[0] >> 1, 16);
		break;
	case V4L2_PIX_FMT_NV12MT_16X16:
	case V4L2_PIX_FMT_NV12MT:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12N:
	case V4L2_PIX_FMT_NV12N_10B:
	case V4L2_PIX_FMT_NV21M:
		raw->stride[0] = ALIGN(ctx->img_width, 16);
		raw->stride[1] = ALIGN(ctx->img_width, 16);
		raw->stride[2] = 0;
		break;
	case V4L2_PIX_FMT_RGB24:
		ctx->raw_buf.stride[0] = ctx->img_width * 3;
		ctx->raw_buf.stride[1] = 0;
		ctx->raw_buf.stride[2] = 0;
		break;
	case V4L2_PIX_FMT_RGB565:
		ctx->raw_buf.stride[0] = ctx->img_width * 2;
		ctx->raw_buf.stride[1] = 0;
		ctx->raw_buf.stride[2] = 0;
		break;
	case V4L2_PIX_FMT_RGB32X:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_ARGB32:
		ctx->raw_buf.stride[0] = (ctx->buf_stride > ctx->img_width) ?
			(ALIGN(ctx->img_width, 16) * 4) : (ctx->img_width * 4);
		ctx->raw_buf.stride[1] = 0;
		ctx->raw_buf.stride[2] = 0;
		break;
	default:
		break;
	}

	/* Decoder needs multiple of 16 alignment for stride */
	if (ctx->type == MFCINST_DECODER) {
		for (i = 0; i < 3; i++)
			ctx->raw_buf.stride[i] =
				ALIGN(ctx->raw_buf.stride[i], 16);
	}
}

void s5p_mfc_dec_calc_dpb_size(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_raw_info *raw, *tiled_ref;
	int i;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	raw = &ctx->raw_buf;
	raw->total_plane_size = 0;

	dec = ctx->dec_priv;
	tiled_ref = &dec->tiled_ref;

	ctx->buf_width = ALIGN(ctx->img_width, MFC_NV12MT_HALIGN);
	ctx->buf_height = ALIGN(ctx->img_height, MFC_NV12MT_VALIGN);
	mfc_info_ctx("SEQ Done: Movie dimensions %dx%d, "
			"buffer dimensions: %dx%d\n", ctx->img_width,
			ctx->img_height, ctx->buf_width, ctx->buf_height);

	switch (ctx->dst_fmt->fourcc) {
	case V4L2_PIX_FMT_NV12MT_16X16:
		raw->plane_size[0] =
			calc_plane(ctx->img_width, ctx->img_height, 1);
		raw->plane_size[1] =
			calc_plane(ctx->img_width, (ctx->img_height >> 1), 1);
		raw->plane_size[2] = 0;
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		raw->plane_size[0] =
			calc_plane(ctx->img_width, ctx->img_height, 0);
		raw->plane_size[1] =
			calc_plane(ctx->img_width, (ctx->img_height >> 1), 0);
		raw->plane_size[2] = 0;
		break;
	case V4L2_PIX_FMT_NV12N:
	case V4L2_PIX_FMT_NV12N_10B:
		raw->plane_size[0] = NV12N_Y_SIZE(ctx->img_width, ctx->img_height);
		raw->plane_size[1] = NV12N_CBCR_SIZE(ctx->img_width, ctx->img_height);
		raw->plane_size[2] = 0;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		raw->plane_size[0] =
			calc_plane(ctx->img_width, ctx->img_height, 0);
		raw->plane_size[1] =
			calc_plane(ctx->img_width >> 1, ctx->img_height >> 1, 0);
		raw->plane_size[2] =
			calc_plane(ctx->img_width >> 1, ctx->img_height >> 1, 0);
		break;
	case V4L2_PIX_FMT_YUV420N:
		raw->plane_size[0] = YUV420N_Y_SIZE(ctx->img_width, ctx->img_height);
		raw->plane_size[1] = YUV420N_CB_SIZE(ctx->img_width, ctx->img_height);
		raw->plane_size[2] = YUV420N_CR_SIZE(ctx->img_width, ctx->img_height);
		break;
	default:
		raw->plane_size[0] = 0;
		raw->plane_size[1] = 0;
		raw->plane_size[2] = 0;
		mfc_err_ctx("Invalid pixelformat : %s\n", ctx->dst_fmt->name);
		break;
	}

	if (IS_MFCv7X(dev)) {
		set_linear_stride_size(ctx, ctx->dst_fmt);
		tiled_ref->plane_size[0] =
			calc_plane(ctx->img_width, ctx->img_height, 1);
		tiled_ref->plane_size[1] =
			calc_plane(ctx->img_width, (ctx->img_height >> 1), 1);
		tiled_ref->plane_size[2] = 0;
	}

	if (IS_MFCv78(dev)) {
		raw->plane_size[0] = ALIGN(((ctx->img_width + 15) / 16) *
				((ctx->img_height + 15) / 16) * 256, 256);
		raw->plane_size[1] = ALIGN(((ctx->img_width + 15) / 16) *
				((ctx->img_height + 15) / 16) * 128, 256);
	}

	if (IS_MFCV8(dev)) {
		set_linear_stride_size(ctx, ctx->dst_fmt);

		switch (ctx->dst_fmt->fourcc) {
			case V4L2_PIX_FMT_NV12M:
			case V4L2_PIX_FMT_NV21M:
				raw->plane_size[0] += 64;
				raw->plane_size[1] =
					(((ctx->img_width + 15)/16)*16)
					*(((ctx->img_height + 15)/16)*8) + 64;
				break;
			case V4L2_PIX_FMT_YUV420M:
			case V4L2_PIX_FMT_YVU420M:
				raw->plane_size[0] += 64;
				raw->plane_size[1] =
					(((ctx->img_width + 15)/16)*16)
					*(((ctx->img_height + 15)/16)*8) + 64;
				raw->plane_size[2] =
					(((ctx->img_width + 15)/16)*16)
					*(((ctx->img_height + 15)/16)*8) + 64;
				break;
			default:
				break;
		}
	}

	if (dec->is_10bit) {
		switch (ctx->dst_fmt->fourcc) {
			case V4L2_PIX_FMT_NV12M:
			case V4L2_PIX_FMT_NV21M:
				raw->stride_2bits[0] = ALIGN(ctx->img_width / 4, 16);
				raw->stride_2bits[1] = ALIGN(ctx->img_width / 4, 16);
				raw->stride_2bits[2] = 0;
				raw->plane_size_2bits[0] =
					ALIGN(ctx->img_width / 4, 16) * ctx->img_height + 64;
				raw->plane_size_2bits[1] =
					ALIGN(ctx->img_width / 4, 16) * (ctx->img_height / 2) + 64;
				raw->plane_size_2bits[2] = 0;
				break;
			case V4L2_PIX_FMT_NV12N_10B:
				raw->stride_2bits[0] = ALIGN(ctx->img_width / 4, 16);
				raw->stride_2bits[1] = ALIGN(ctx->img_width / 4, 16);
				raw->stride_2bits[2] = 0;
				raw->plane_size_2bits[0] = NV12N_10B_Y_2B_SIZE(ctx->img_width, ctx->img_height);
				raw->plane_size_2bits[1] = NV12N_10B_CBCR_2B_SIZE(ctx->img_width, ctx->img_height);
				raw->plane_size_2bits[2] = 0;
				break;
			default:
				mfc_err_ctx("HEVC 10bit: not supported format: %s\n",
						ctx->dst_fmt->name);
				break;
		}
	}

	for (i = 0; i < raw->num_planes; i++) {
		raw->total_plane_size += raw->plane_size[i];
		mfc_debug(2, "Plane[%d] size = %d, stride = %d\n",
			i, raw->plane_size[i], raw->stride[i]);
	}
	if (dec->is_10bit) {
		for (i = 0; i < raw->num_planes; i++) {
			raw->total_plane_size += raw->plane_size_2bits[i];
			mfc_debug(2, "Plane[%d] 2bit size = %d, stride = %d\n",
					i, raw->plane_size_2bits[i],
					raw->stride_2bits[i]);
		}
	}
	mfc_debug(2, "total plane size: %d\n", raw->total_plane_size);

	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
			ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC) {
		ctx->mv_size = s5p_mfc_dec_mv_size(ctx->img_width,
				ctx->img_height);
		ctx->mv_size = ALIGN(ctx->mv_size, 32);
	} else if (ctx->codec_mode == S5P_FIMV_CODEC_HEVC_DEC) {
		ctx->mv_size = s5p_mfc_dec_hevc_mv_size(ctx->img_width,
				ctx->img_height);
		ctx->mv_size = ALIGN(ctx->mv_size, 32);
	} else {
		ctx->mv_size = 0;
	}
}

/* Set registers for decoding stream buffer */
int s5p_mfc_set_dec_stream_buffer(struct s5p_mfc_ctx *ctx, struct s5p_mfc_buf *mfc_buf,
		  unsigned int start_num_byte, unsigned int strm_size)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	size_t cpb_buf_size;
	dma_addr_t addr;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	if (mfc_buf)
		addr = s5p_mfc_mem_plane_addr(ctx, &mfc_buf->vb, 0);
	else
		addr = 0;

	cpb_buf_size = ALIGN(dec->src_buf_size, MFC_NV12M_HALIGN);

	if (strm_size >= set_strm_size_max(cpb_buf_size)) {
		mfc_info_ctx("Decrease strm_size : %u -> %zu, gap : %d\n",
				strm_size, set_strm_size_max(cpb_buf_size), CPB_GAP);
		strm_size = set_strm_size_max(cpb_buf_size);
		if (mfc_buf)
			mfc_buf->vb.v4l2_planes[0].bytesused = strm_size;
	}

	mfc_debug(2, "inst_no: %d, buf_addr: 0x%08llx\n", ctx->inst_no,
		(unsigned long long)addr);
	mfc_debug(2, "strm_size: 0x%08x cpb_buf_size: %zu offset: 0x%08x\n",
			strm_size, cpb_buf_size, start_num_byte);

	if (strm_size == 0)
		mfc_info_ctx("stream size is 0\n");

	MFC_WRITEL(strm_size, S5P_FIMV_D_STREAM_DATA_SIZE);
	MFC_WRITEL(addr, S5P_FIMV_D_CPB_BUFFER_ADDR);
	MFC_WRITEL(cpb_buf_size, S5P_FIMV_D_CPB_BUFFER_SIZE);
	MFC_WRITEL(start_num_byte, S5P_FIMV_D_CPB_BUFFER_OFFSET);

	mfc_debug_leave();
	return 0;
}

void s5p_mfc_set_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr[], int num_planes)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int i;

	if (IS_MFCv7X(dev) || IS_MFCV8(dev)) {
		for (i = 0; i < num_planes; i++)
			MFC_WRITEL(addr[i], S5P_FIMV_E_SOURCE_FIRST_ADDR + (i*4));
	} else {
		for (i = 0; i < num_planes; i++)
			MFC_WRITEL(addr[i], S5P_FIMV_E_SOURCE_LUMA_ADDR + (i*4));
	}
}

/* Set registers for encoding stream buffer */
int s5p_mfc_set_enc_stream_buffer(struct s5p_mfc_ctx *ctx,
		struct s5p_mfc_buf *mfc_buf)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	dma_addr_t addr;
	unsigned int size;

	addr = s5p_mfc_mem_plane_addr(ctx, &mfc_buf->vb, 0);
	size = (unsigned int)vb2_plane_size(&mfc_buf->vb, 0);
	size = ALIGN(size, 512);

	MFC_WRITEL(addr, S5P_FIMV_E_STREAM_BUFFER_ADDR); /* 16B align */
	MFC_WRITEL(size, S5P_FIMV_E_STREAM_BUFFER_SIZE);

	mfc_debug(2, "stream buf addr: 0x%08lx, size: 0x%08x(%d)",
		(unsigned long)addr, size, size);

	return 0;
}

void s5p_mfc_get_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr[], int num_planes)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long enc_recon_y_addr, enc_recon_c_addr;
	int i, addr_offset;

	if (IS_MFCv7X(dev) || IS_MFCV8(dev))
		addr_offset = S5P_FIMV_E_ENCODED_SOURCE_FIRST_ADDR;
	else
		addr_offset = S5P_FIMV_E_ENCODED_SOURCE_LUMA_ADDR;

	for (i = 0; i < num_planes; i++)
		addr[i] = MFC_READL(addr_offset + (i * 4));

	enc_recon_y_addr = MFC_READL(S5P_FIMV_E_RECON_LUMA_DPB_ADDR);
	enc_recon_c_addr = MFC_READL(S5P_FIMV_E_RECON_CHROMA_DPB_ADDR);

	mfc_debug(2, "recon y addr: 0x%08lx", enc_recon_y_addr);
	mfc_debug(2, "recon c addr: 0x%08lx", enc_recon_c_addr);
}

/* Set encoding ref & codec buffer */
int s5p_mfc_set_enc_ref_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	dma_addr_t buf_addr1;
	int buf_size1;
	int i;

	mfc_debug_enter();

	buf_addr1 = ctx->codec_buf_phys;
	buf_size1 = ctx->codec_buf_size;

	mfc_debug(2, "Buf1: %p (%d)\n", (void *)buf_addr1, buf_size1);

	/* start address of per buffer is aligned */
	for (i = 0; i < ctx->dpb_count; i++) {
		MFC_WRITEL(buf_addr1, S5P_FIMV_E_LUMA_DPB + (4 * i));
		buf_addr1 += enc->luma_dpb_size;
		buf_size1 -= enc->luma_dpb_size;
	}
	for (i = 0; i < ctx->dpb_count; i++) {
		MFC_WRITEL(buf_addr1, S5P_FIMV_E_CHROMA_DPB + (4 * i));
		buf_addr1 += enc->chroma_dpb_size;
		buf_size1 -= enc->chroma_dpb_size;
	}
	for (i = 0; i < ctx->dpb_count; i++) {
		MFC_WRITEL(buf_addr1, S5P_FIMV_E_ME_BUFFER + (4 * i));
		buf_addr1 += enc->me_buffer_size;
		buf_size1 -= enc->me_buffer_size;
	}

	MFC_WRITEL(buf_addr1, S5P_FIMV_E_SCRATCH_BUFFER_ADDR);
	MFC_WRITEL(ctx->scratch_buf_size, S5P_FIMV_E_SCRATCH_BUFFER_SIZE);
	buf_addr1 += ctx->scratch_buf_size;
	buf_size1 -= ctx->scratch_buf_size;

	if (!IS_MFCv78(dev)) {
		MFC_WRITEL(buf_addr1, S5P_FIMV_E_TMV_BUFFER0);
		buf_addr1 += enc->tmv_buffer_size >> 1;
		MFC_WRITEL(buf_addr1, S5P_FIMV_E_TMV_BUFFER1);
		buf_addr1 += enc->tmv_buffer_size >> 1;
		buf_size1 -= enc->tmv_buffer_size;
	}

	mfc_debug(2, "Buf1: %p, buf_size1: %d (ref frames %d)\n",
			(void *)buf_addr1, buf_size1, ctx->dpb_count);
	if (buf_size1 < 0) {
		mfc_debug(2, "Not enough memory has been allocated.\n");
		return -ENOMEM;
	}

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_INIT_BUFS, NULL);

	mfc_debug_leave();

	return 0;
}

void s5p_mfc_enc_calc_src_size(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_raw_info *raw;
	unsigned int mb_width, mb_height, default_size;
	int i, add_size;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	raw = &ctx->raw_buf;
	raw->total_plane_size = 0;
	mb_width = mb_width(ctx->img_width);
	mb_height = mb_height(ctx->img_height);
	add_size = mfc_linear_buf_size(mfc_version(dev));
	default_size = mb_width * mb_height * 256;

	ctx->buf_width = ALIGN(ctx->img_width, MFC_NV12M_HALIGN);

	switch (ctx->src_fmt->fourcc) {
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV420N:
	case V4L2_PIX_FMT_YVU420M:
		ctx->raw_buf.plane_size[0] = ALIGN(default_size, 256);
		ctx->raw_buf.plane_size[1] = ALIGN(default_size >> 2, 256);
		ctx->raw_buf.plane_size[2] = ALIGN(default_size >> 2, 256);
		break;
	case V4L2_PIX_FMT_NV12MT_16X16:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12N:
	case V4L2_PIX_FMT_NV21M:
		raw->plane_size[0] = ALIGN(default_size, 256);
		raw->plane_size[1] = ALIGN(default_size / 2, 256);
		raw->plane_size[2] = 0;
		break;
	case V4L2_PIX_FMT_RGB565:
		raw->plane_size[0] = ALIGN(default_size * 2, 256);
		raw->plane_size[1] = 0;
		raw->plane_size[2] = 0;
		break;
	case V4L2_PIX_FMT_RGB24:
		raw->plane_size[0] = ALIGN(default_size * 3, 256);
		raw->plane_size[1] = 0;
		raw->plane_size[2] = 0;
		break;
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_RGB32X:
	case V4L2_PIX_FMT_ARGB32:
		raw->plane_size[0] = ALIGN(default_size * 4, 256);
		raw->plane_size[1] = 0;
		raw->plane_size[2] = 0;
		break;
	default:
		raw->plane_size[0] = 0;
		raw->plane_size[1] = 0;
		raw->plane_size[2] = 0;
		mfc_err_ctx("Invalid pixel format(%d)\n", ctx->src_fmt->fourcc);
		break;
	}

	/* Add extra if necessary */
	for (i = 0; i < raw->num_planes; i++)
		raw->plane_size[i] += add_size;

	for (i = 0; i < raw->num_planes; i++) {
		raw->total_plane_size += raw->plane_size[i];
		mfc_debug(2, "Plane[%d] size = %d, stride = %d\n",
			i, raw->plane_size[i], raw->stride[i]);
	}
	mfc_debug(2, "total plane size: %d\n", raw->total_plane_size);

	if (IS_MFCv7X(dev) || IS_MFCV8(dev))
		set_linear_stride_size(ctx, ctx->src_fmt);
}

static int mfc_set_dec_stride_buffer(struct s5p_mfc_ctx *ctx, struct list_head *buf_queue)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int i;

	for (i = 0; i < ctx->raw_buf.num_planes; i++) {
		MFC_WRITEL(ctx->raw_buf.stride[i],
			S5P_FIMV_D_FIRST_PLANE_DPB_STRIDE_SIZE + (i * 4));
		mfc_debug(2, "# plane%d.size = %d, stride = %d\n", i,
			ctx->raw_buf.plane_size[i], ctx->raw_buf.stride[i]);
	}

	return 0;
}

/* Set decoding frame buffer */
int s5p_mfc_set_dec_frame_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	unsigned int i, frame_size_mv;
	dma_addr_t buf_addr1;
	int buf_size1;
	int align_gap;
	struct s5p_mfc_buf *buf;
	struct s5p_mfc_raw_info *raw, *tiled_ref;
	struct list_head *buf_queue;
	unsigned int reg = 0;
	unsigned int num_dpb;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	/* only 2 plane is supported for HEVC 10bit */
	if (dec->is_10bit)
		MFC_WRITEL(0, S5P_FIMV_PIXEL_FORMAT);

	raw = &ctx->raw_buf;
	tiled_ref = &dec->tiled_ref;
	buf_addr1 = ctx->codec_buf_phys;
	buf_size1 = ctx->codec_buf_size;

	mfc_debug(2, "Buf1: %p (%d)\n", (void *)buf_addr1, buf_size1);
	mfc_debug(2, "Total DPB COUNT: %d\n", dec->total_dpb_count);
	mfc_debug(2, "Setting display delay to %d\n", dec->display_delay);

	if (IS_MFCV8(dev)) {
		MFC_WRITEL(dec->total_dpb_count, S5P_FIMV_D_NUM_DPB);
		mfc_debug(2, "raw->num_planes %d\n", raw->num_planes);
		for (i = 0; i < raw->num_planes; i++) {
			mfc_debug(2, "raw->plane_size[%d]= %d\n", i, raw->plane_size[i]);
			MFC_WRITEL(raw->plane_size[i], S5P_FIMV_D_FIRST_PLANE_DPB_SIZE + i*4);
		}
	} else {
		MFC_WRITEL(dec->total_dpb_count, S5P_FIMV_D_NUM_DPB);
		MFC_WRITEL(raw->plane_size[0], S5P_FIMV_D_FIRST_PLANE_DPB_SIZE);
		MFC_WRITEL(raw->plane_size[1], S5P_FIMV_D_SECOND_PLANE_DPB_SIZE);
	}

	MFC_WRITEL(buf_addr1, S5P_FIMV_D_SCRATCH_BUFFER_ADDR);
	MFC_WRITEL(ctx->scratch_buf_size, S5P_FIMV_D_SCRATCH_BUFFER_SIZE);
	buf_addr1 += ctx->scratch_buf_size;
	buf_size1 -= ctx->scratch_buf_size;

	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
			ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC ||
			ctx->codec_mode == S5P_FIMV_CODEC_HEVC_DEC)
		MFC_WRITEL(ctx->mv_size, S5P_FIMV_D_MV_BUFFER_SIZE);

	if (ctx->codec_mode == S5P_FIMV_CODEC_VP9_DEC){
		MFC_WRITEL(buf_addr1, S5P_FIMV_D_STATIC_BUFFER_ADDR);
		MFC_WRITEL(DEC_V90_STATIC_BUFFER_SIZE, S5P_FIMV_D_STATIC_BUFFER_SIZE);
		buf_addr1 += DEC_V90_STATIC_BUFFER_SIZE;
		buf_size1 -= DEC_V90_STATIC_BUFFER_SIZE;
	}

	if ((ctx->codec_mode == S5P_FIMV_CODEC_MPEG4_DEC) &&
			FW_HAS_INITBUF_LOOP_FILTER(dev)) {
		if (dec->loop_filter_mpeg4) {
			if (dec->total_dpb_count >=
					(ctx->dpb_count + NUM_MPEG4_LF_BUF)) {
				if (!dec->internal_dpb) {
					dec->internal_dpb = NUM_MPEG4_LF_BUF;
					ctx->dpb_count += dec->internal_dpb;
				}
			} else {
				dec->loop_filter_mpeg4 = 0;
				mfc_debug(2, "failed to enable loop filter\n");
			}
		}
		reg |= (dec->loop_filter_mpeg4
				<< S5P_FIMV_D_OPT_LF_CTRL_SHIFT);
	}
	if ((ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) &&
			FW_HAS_INITBUF_TILE_MODE(dev))
		reg |= (0x1 << S5P_FIMV_D_OPT_TILE_MODE_SHIFT);
	if (dec->is_dynamic_dpb)
		reg |= (0x1 << S5P_FIMV_D_OPT_DYNAMIC_DPB_SET_SHIFT);

	if (not_coded_cond(ctx) && FW_HAS_NOT_CODED(dev)) {
		reg |= (0x1 << S5P_FIMV_D_OPT_NOT_CODED_SET_SHIFT);
		mfc_info_ctx("Notcoded frame copy mode start\n");
	}
	/* Enable 10bit Dithering */
	if (dec->is_10bit)
		reg |= (0x1 << S5P_FIMV_D_OPT_DITHERING_SET_SHIFT);

	MFC_WRITEL(reg, S5P_FIMV_D_INIT_BUFFER_OPTIONS);

	frame_size_mv = ctx->mv_size;
	mfc_debug(2, "Frame size: %d, %d, %d, mv: %d\n",
			raw->plane_size[0], raw->plane_size[1],
			raw->plane_size[2], frame_size_mv);

	if (dec->dst_memtype == V4L2_MEMORY_USERPTR || dec->dst_memtype == V4L2_MEMORY_DMABUF)
		buf_queue = &ctx->dst_queue;
	else
		buf_queue = &dec->dpb_queue;

	if (IS_MFCV8(dev)) {
		num_dpb = 0;
		list_for_each_entry(buf, buf_queue, list) {
			/* Do not setting DPB */
			if (dec->is_dynamic_dpb)
				break;
			for (i = 0; i < raw->num_planes; i++) {
				MFC_WRITEL(buf->planes.raw[i],
					S5P_FIMV_D_FIRST_PLANE_DPB0 + (i*0x100 + num_dpb*4));
			}

			if ((num_dpb == 0) && (!ctx->is_drm))
				mfc_fill_dynamic_dpb(ctx, &buf->vb);
			num_dpb++;
		}
	}

	mfc_set_dec_stride_buffer(ctx, buf_queue);
	if (dec->is_10bit) {
		for (i = 0; i < ctx->raw_buf.num_planes; i++) {
			MFC_WRITEL(raw->stride_2bits[i], S5P_FIMV_D_FIRST_PLANE_2BIT_DPB_STRIDE_SIZE + (i * 4));
			MFC_WRITEL(raw->plane_size_2bits[i], S5P_FIMV_D_FIRST_PLANE_2BIT_DPB_SIZE + (i * 4));
			mfc_debug(2, "# HEVC 10bit : 2bits plane%d.size = %d, stride = %d\n", i,
				ctx->raw_buf.plane_size_2bits[i], ctx->raw_buf.stride_2bits[i]);
		}
	}

	MFC_WRITEL(dec->mv_count, S5P_FIMV_D_NUM_MV);
	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
			ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC ||
			ctx->codec_mode == S5P_FIMV_CODEC_HEVC_DEC) {
		for (i = 0; i < dec->mv_count; i++) {
			/* To test alignment */
			align_gap = buf_addr1;
			buf_addr1 = ALIGN(buf_addr1, 16);
			align_gap = buf_addr1 - align_gap;
			buf_size1 -= align_gap;

			mfc_debug(2, "\tBuf1: %p, size: %d\n", (void *)buf_addr1, buf_size1);
			MFC_WRITEL(buf_addr1, S5P_FIMV_D_MV_BUFFER0 + i * 4);
			buf_addr1 += frame_size_mv;
			buf_size1 -= frame_size_mv;
		}
	}

	mfc_debug(2, "Buf1: %p, buf_size1: %d (frames %d)\n",
			(void *)buf_addr1, buf_size1, dec->total_dpb_count);
	if (buf_size1 < 0) {
		mfc_debug(2, "Not enough memory has been allocated.\n");
		return -ENOMEM;
	}

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_INIT_BUFS, NULL);

	mfc_debug(2, "After setting buffers.\n");
	return 0;
}

