/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_common.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This file contains definitions of enums and structs used by the codec
 * driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */

#ifndef __S5P_MFC_COMMON_H
#define __S5P_MFC_COMMON_H __FILE__

#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/exynos_mfc_media.h>

#include "exynos-mfc.h"

#if defined(CONFIG_EXYNOS_MFC_V10)
#include "regs-mfc-v10.h"
#endif

#include "s5p_mfc_reg.h"
#include "s5p_mfc_macros.h"
#include "s5p_mfc_data_struct.h"
#include "s5p_mfc_debug.h"

#define MFC_DRIVER_INFO		161116

#define MFC_MAX_REF_BUFS	2
#define MFC_FRAME_PLANES	2
#define MFC_INFO_INIT_FD	-1

#define MFC_MAX_DRM_CTX		2
/* Interrupt timeout */
#define MFC_INT_TIMEOUT		2000
/* Interrupt short timeout */
#define MFC_INT_SHORT_TIMEOUT	800
/* Busy wait timeout */
#define MFC_BW_TIMEOUT		500
/* Watchdog interval */
#define MFC_WATCHDOG_INTERVAL   1000
/* After how many executions watchdog should assume lock up */
#define MFC_WATCHDOG_CNT        15

#define MFC_NO_INSTANCE_SET	-1

#define MFC_ENC_CAP_PLANE_COUNT	1
#define MFC_ENC_OUT_PLANE_COUNT	2

#define MFC_NAME_LEN		16
#define MFC_FW_NAME		"mfc_fw.bin"

#define STUFF_BYTE		4
#define MFC_WORKQUEUE_LEN	32

#define MFC_BASE_MASK		((1 << 17) - 1)

#define FLAG_LAST_FRAME		0x80000000
#define FLAG_EMPTY_DATA		0x40000000
#define FLAG_CSD		0x20000000
#define MFC_MAX_INTERVAL	(2 * USEC_PER_SEC)

/* MFC conceal color is black */
#define MFC_CONCEAL_COLOR	0x8020000

#define vb_to_mfc_buf(x)	\
	container_of(x, struct s5p_mfc_buf, vb)

#define	MFC_CTRL_TYPE_GET	(MFC_CTRL_TYPE_GET_SRC | MFC_CTRL_TYPE_GET_DST)
#define	MFC_CTRL_TYPE_SRC	(MFC_CTRL_TYPE_SET | MFC_CTRL_TYPE_GET_SRC)
#define	MFC_CTRL_TYPE_DST	(MFC_CTRL_TYPE_GET_DST)

#define call_bop(b, op, args...)	\
	(b->op ? b->op(args) : 0)

#define fh_to_mfc_ctx(x)	\
	container_of(x, struct s5p_mfc_ctx, fh)

#define MFC_FMT_DEC	0
#define MFC_FMT_ENC	1
#define MFC_FMT_RAW	2

#define call_cop(c, op, args...)				\
	(((c)->c_ops->op) ?					\
		((c)->c_ops->op(args)) : 0)

#define MFC_VER_MAJOR(dev)	((mfc_version(dev) >> 8) & 0xF)
#define MFC_VER_MINOR(dev)	(mfc_version(dev) & 0xF)

/*
 * Version Description
 *
 * IS_MFCv5X : For MFC v5.X only
 * IS_MFCv6X : For MFC v6.X only
 * IS_MFCv7X : For MFC v7.X only
 * IS_MFCv8X : For MFC v8.X only
 * IS_MFCv9X : For MFC v9.X only
 * IS_MFCv10X : For MFC v10.X only
 * IS_MFCv78 : For MFC v7.8 only
 * IS_MFCV6 : For MFC v6 architecure
 * IS_MFCV8 : For after MFC v8 architecure
 */
#define IS_MFCv5X(dev)		(mfc_version(dev) == 0x51)
#define IS_MFCv6X(dev)		((mfc_version(dev) == 0x61) || \
				 (mfc_version(dev) == 0x65))
#define IS_MFCv7X(dev)		((mfc_version(dev) == 0x72) || \
				 (mfc_version(dev) == 0x723) || \
				 (mfc_version(dev) == 0x78))
#define IS_MFCv8X(dev)		(mfc_version(dev) == 0x80)
#define IS_MFCv9X(dev)		(mfc_version(dev) == 0x90)
#define IS_MFCv10X(dev)		((mfc_version(dev) == 0xA0) || \
				 (mfc_version(dev) == 0xA01))
#define IS_MFCv78(dev)		(mfc_version(dev) == 0x78)
#define IS_MFCV6(dev)		(IS_MFCv6X(dev) || IS_MFCv7X(dev) ||	\
				IS_MFCv8X(dev) || IS_MFCv9X(dev) ||	\
				IS_MFCv10X(dev))
#define IS_MFCV8(dev)		(IS_MFCv8X(dev) || IS_MFCv9X(dev) ||	\
				(IS_MFCv10X(dev)))

/* supported feature macros by F/W version */
#define FW_HAS_BUS_RESET(dev)		(dev->fw.date >= 0x120206)
#define FW_HAS_VER_INFO(dev)		(dev->fw.date >= 0x120328)
#define FW_HAS_INITBUF_TILE_MODE(dev)	(dev->fw.date >= 0x120629)
#define FW_HAS_INITBUF_LOOP_FILTER(dev)	(dev->fw.date >= 0x120831)
#define FW_HAS_SEI_S3D_REALLOC(dev)	(IS_MFCV6(dev) &&	\
					(dev->fw.date >= 0x121205))
#define FW_HAS_ENC_SPSPPS_CTRL(dev)	((IS_MFCV6(dev) &&		\
					(dev->fw.date >= 0x121005)) ||	\
					(IS_MFCv5X(dev) &&		\
					(dev->fw.date >= 0x120823)))
#define FW_HAS_ADV_RC_MODE(dev)		(IS_MFCV6(dev) &&		\
					(dev->fw.date >= 0x130329))
#define FW_HAS_I_LIMIT_RC_MODE(dev)	((IS_MFCv7X(dev) &&		\
					(dev->fw.date >= 0x140801)) ||	\
					(IS_MFCv8X(dev) &&		\
					(dev->fw.date >= 0x140808)) ||	\
					(IS_MFCv9X(dev) &&		\
					(dev->fw.date >= 0x141008)) ||	\
					IS_MFCv10X(dev))
#define FW_HAS_POC_TYPE_CTRL(dev)	(IS_MFCV6(dev) &&		\
					(dev->fw.date >= 0x130405))
#define FW_HAS_DYNAMIC_DPB(dev)		((IS_MFCv7X(dev) || IS_MFCV8(dev))&&	\
					(dev->fw.date >= 0x131108))
#define FW_HAS_NOT_CODED(dev)		(IS_MFCV8(dev) &&		\
					(dev->fw.date >= 0x140926))
#define FW_HAS_BASE_CHANGE(dev)		((IS_MFCv7X(dev) || IS_MFCV8(dev))&&	\
					(dev->fw.date >= 0x131108))
#define FW_HAS_TEMPORAL_SVC_CH(dev)	((IS_MFCv8X(dev) &&			\
					(dev->fw.date >= 0x140821)) ||		\
					(IS_MFCv9X(dev) &&			\
					(dev->fw.date >= 0x141008)) ||		\
					IS_MFCv10X(dev))
#define FW_WAKEUP_AFTER_RISC_ON(dev)	(IS_MFCV8(dev) || IS_MFCv78(dev))

#define FW_HAS_LAST_DISP_INFO(dev)	(IS_MFCv9X(dev) &&			\
					(dev->fw.date >= 0x141205))
#define FW_HAS_GOP2(dev)		((IS_MFCv9X(dev) &&			\
					(dev->fw.date >= 0x150316)) ||		\
					IS_MFCv10X(dev))
#define FW_HAS_E_MIN_SCRATCH_BUF(dev)	(IS_MFCv9X(dev) || IS_MFCv10X(dev))
#define FW_HAS_INT_TIMEOUT(dev)		(IS_MFCv9X(dev) || IS_MFCv10X(dev))
#define FW_HAS_CONCEAL_CONTROL(dev)	IS_MFCv10X(dev)

#define FW_SUPPORT_SKYPE(dev)		(IS_MFCv10X(dev) &&		\
					(dev->fw.date >= 0x150901))
#define FW_HAS_ROI_CONTROL(dev)		IS_MFCv10X(dev)
#define FW_HAS_VIDEO_SIGNAL_TYPE(dev)	(IS_MFCv10X(dev) &&		\
					(dev->fw.date >= 0x151223))
#define FW_HAS_SEI_INFO_FOR_HDR(dev)	(IS_MFCv10X(dev) &&		\
					(dev->fw.date >= 0x160415))
#define FW_HAS_FIXED_SLICE(dev)		(IS_MFCv10X(dev) &&		\
					(dev->fw.date >= 0x160202))

#define HW_LOCK_CLEAR_MASK		(0xFFFFFFFF)

#define is_h264(ctx)		((ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC) ||\
				(ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC))
#define is_mpeg4vc1(ctx)	((ctx->codec_mode == S5P_FIMV_CODEC_VC1RCV_DEC) ||\
				(ctx->codec_mode == S5P_FIMV_CODEC_VC1_DEC) ||\
				(ctx->codec_mode == S5P_FIMV_CODEC_MPEG4_DEC))
#define is_mpeg2(ctx)		(ctx->codec_mode == S5P_FIMV_CODEC_MPEG2_DEC)
#define MFC_UHD_RES		(3840*2160)
#define MFC_HD_RES		(1280*720)
#define is_UHD(ctx)		(((ctx)->img_width * (ctx)->img_height) == MFC_UHD_RES)
#define under_HD(ctx)		(((ctx)->img_width * (ctx)->img_height) <= MFC_HD_RES)
#define not_coded_cond(ctx)	is_mpeg4vc1(ctx)
#define interlaced_cond(ctx)	is_mpeg4vc1(ctx) || is_mpeg2(ctx) || is_h264(ctx)
#define on_res_change(ctx)	((ctx)->state >= MFCINST_RES_CHANGE_INIT &&	\
				 (ctx)->state <= MFCINST_RES_CHANGE_END)
#define need_to_wait_frame_start(ctx)		\
	(((ctx->state == MFCINST_FINISHING) ||	\
	  (ctx->state == MFCINST_RUNNING)) &&	\
	 test_bit(ctx->num, &ctx->dev->hw_lock))
#define need_to_wait_nal_abort(ctx)		 \
	(ctx->state == MFCINST_ABORT_INST)
#define need_to_continue(ctx)			\
	((ctx->state == MFCINST_DPB_FLUSHING) ||\
	(ctx->state == MFCINST_ABORT_INST) ||	\
	(ctx->state == MFCINST_RETURN_INST) ||	\
	(ctx->state == MFCINST_SPECIAL_PARSING) ||	\
	(ctx->state == MFCINST_SPECIAL_PARSING_NAL))
#define need_to_special_parsing(ctx)		\
	((ctx->state == MFCINST_GOT_INST) ||	\
	 (ctx->state == MFCINST_HEAD_PARSED))
#define need_to_special_parsing_nal(ctx)	\
	(ctx->state == MFCINST_RUNNING)

/* Extra information for Decoder */
#define	DEC_SET_DUAL_DPB		(1 << 0)
#define	DEC_SET_DYNAMIC_DPB		(1 << 1)
#define	DEC_SET_LAST_FRAME_INFO		(1 << 2)
#define	DEC_SET_SKYPE_FLAG		(1 << 3)
/* Extra information for Encoder */
#define	ENC_SET_RGB_INPUT		(1 << 0)
#define	ENC_SET_SPARE_SIZE		(1 << 1)
#define	ENC_SET_TEMP_SVC_CH		(1 << 2)
#define	ENC_SET_SKYPE_FLAG		(1 << 3)
#define	ENC_SET_ROI_CONTROL		(1 << 4)
#define	ENC_SET_QP_BOUND_PB		(1 << 5)
#define	ENC_SET_FIXED_SLICE		(1 << 6)

#define MFC_QOS_FLAG_NODATA		0xFFFFFFFF

static inline unsigned int mfc_linear_buf_size(unsigned int version)
{
	unsigned int size = 0;

	switch (version) {
	case 0x51:
	case 0x61:
	case 0x65:
		size = 0;
		break;
	case 0x72:
	case 0x723:
	case 0x80:
	case 0x90:
	case 0xA0:
	case 0xA01:
		size = 256;
		break;
	default:
		size = 0;
		break;
	}

	return size;
}

static inline unsigned int mfc_version(struct s5p_mfc_dev *dev)
{
	unsigned int version = 0;

	switch (dev->pdata->ip_ver) {
	case IP_VER_MFC_4P_0:
	case IP_VER_MFC_4P_1:
	case IP_VER_MFC_4P_2:
		version = 0x51;
		break;
	case IP_VER_MFC_5G_0:
		version = 0x61;
		break;
	case IP_VER_MFC_5G_1:
	case IP_VER_MFC_5A_0:
	case IP_VER_MFC_5A_1:
		version = 0x65;
		break;
	case IP_VER_MFC_6A_0:
	case IP_VER_MFC_6A_1:
		version = 0x72;
		break;
	case IP_VER_MFC_6A_2:
		version = 0x723;
		break;
	case IP_VER_MFC_7A_0:
		version = 0x80;
		break;
	case IP_VER_MFC_8I_0:
		version = 0x90;
		break;
	case IP_VER_MFC_6I_0:
		version = 0x78;
		break;
	case IP_VER_MFC_8J_0:
		version = 0xA0;
		break;
	case IP_VER_MFC_8J_1:
		version = 0xA01;
		break;
	}

	return version;
}

#ifdef CONFIG_ION_EXYNOS
extern struct ion_device *ion_exynos;
#endif

int s5p_mfc_dec_ctx_ready(struct s5p_mfc_ctx *ctx);
int s5p_mfc_enc_ctx_ready(struct s5p_mfc_ctx *ctx);

static inline int s5p_mfc_ctx_ready(struct s5p_mfc_ctx *ctx)
{
	if (ctx->type == MFCINST_DECODER)
		return s5p_mfc_dec_ctx_ready(ctx);
	else if (ctx->type == MFCINST_ENCODER)
		return s5p_mfc_enc_ctx_ready(ctx);

	return 0;
}

#endif /* __S5P_MFC_COMMON_H */
