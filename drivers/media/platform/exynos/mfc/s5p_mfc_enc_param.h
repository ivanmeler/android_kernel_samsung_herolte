/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_enc_param.h
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

#ifndef __S5P_MFC_ENC_PARAM_H
#define __S5P_MFC_ENC_PARAM_H __FILE__

#include "s5p_mfc_common.h"

int s5p_mfc_set_slice_mode(struct s5p_mfc_ctx *ctx);
int s5p_mfc_set_enc_params_h264(struct s5p_mfc_ctx *ctx);
int s5p_mfc_set_enc_params_mpeg4(struct s5p_mfc_ctx *ctx);
int s5p_mfc_set_enc_params_h263(struct s5p_mfc_ctx *ctx);
int s5p_mfc_set_enc_params_vp8(struct s5p_mfc_ctx *ctx);
int s5p_mfc_set_enc_params_vp9(struct s5p_mfc_ctx *ctx);
int s5p_mfc_set_enc_params_hevc(struct s5p_mfc_ctx *ctx);

#endif /* __S5P_MFC_ENC_PARAM_H */
