/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_qos.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <soc/samsung/bts.h>

#include "s5p_mfc_qos.h"

#include "../../../../soc/samsung/pwrcal/pwrcal.h"
#include "../../../../soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h"

#define EXTRA_DEFAULT		0x0
#define EXTRA_NO_LIMIT_MO	0x1
#define EXTRA_LIMIT_CLK		0x2

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
enum {
	MFC_QOS_ADD,
	MFC_QOS_UPDATE,
	MFC_QOS_REMOVE,
	MFC_QOS_EXTRA,
};

static void mfc_qos_operate(struct s5p_mfc_ctx *ctx, int opr_type, int idx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_platdata *pdata = dev->pdata;
	struct s5p_mfc_qos *qos_table = pdata->qos_table;

	switch (opr_type) {
	case MFC_QOS_ADD:
		MFC_TRACE_CTX("++ QOS add[%d] (int:%d, mif:%d)\n",
			idx, qos_table[idx].freq_int, qos_table[idx].freq_mif);

		pm_qos_add_request(&dev->qos_req_int,
				PM_QOS_DEVICE_THROUGHPUT,
				qos_table[idx].freq_int);
		pm_qos_add_request(&dev->qos_req_mif,
				PM_QOS_BUS_THROUGHPUT,
				qos_table[idx].freq_mif);

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_add_request(&dev->qos_req_cluster1,
				PM_QOS_CLUSTER1_FREQ_MIN,
				qos_table[idx].freq_cpu);
		pm_qos_add_request(&dev->qos_req_cluster0,
				PM_QOS_CLUSTER0_FREQ_MIN,
				qos_table[idx].freq_kfc);
#endif
		atomic_set(&dev->qos_req_cur, idx + 1);
		MFC_TRACE_CTX("-- QOS add[%d] (int:%d, mif:%d)\n",
			idx, qos_table[idx].freq_int, qos_table[idx].freq_mif);
		mfc_info_ctx("QoS request: %d\n", idx + 1);
		break;
	case MFC_QOS_UPDATE:
		MFC_TRACE_CTX("++ QOS update[%d] (int:%d, mif:%d)\n",
				idx, qos_table[idx].freq_int, qos_table[idx].freq_mif);

		pm_qos_update_request(&dev->qos_req_int,
				qos_table[idx].freq_int);
		pm_qos_update_request(&dev->qos_req_mif,
				qos_table[idx].freq_mif);

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_update_request(&dev->qos_req_cluster1,
				qos_table[idx].freq_cpu);
		pm_qos_update_request(&dev->qos_req_cluster0,
				qos_table[idx].freq_kfc);
#endif
		atomic_set(&dev->qos_req_cur, idx + 1);
		MFC_TRACE_CTX("-- QOS update[%d] (int:%d, mif:%d)\n",
				idx, qos_table[idx].freq_int, qos_table[idx].freq_mif);
		mfc_info_ctx("QoS update: %d\n", idx + 1);
		break;
	case MFC_QOS_REMOVE:
		MFC_TRACE_CTX("++ QOS remove\n");

		pm_qos_remove_request(&dev->qos_req_int);
		pm_qos_remove_request(&dev->qos_req_mif);

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_remove_request(&dev->qos_req_cluster1);
		pm_qos_remove_request(&dev->qos_req_cluster0);
#endif
#ifdef CONFIG_EXYNOS8890_BTS_OPTIMIZATION
		if (dev->extra_qos == EXTRA_LIMIT_CLK) {
			cal_dfs_ext_ctrl(dvfs_int, cal_dfs_rate_lock, false);
			dev->extra_qos = EXTRA_DEFAULT;
			mfc_info_ctx("QoS extra: restore CLK\n");
		} else if (dev->extra_qos == EXTRA_NO_LIMIT_MO) {
			bts_ext_scenario_set(TYPE_MFC, TYPE_HIGHPERF, false);
			dev->extra_qos = EXTRA_DEFAULT;
			mfc_info_ctx("QoS extra: restore MO\n");
		}
#endif

		atomic_set(&dev->qos_req_cur, 0);
		MFC_TRACE_CTX("-- QOS remove\n");
		mfc_info_ctx("QoS remove\n");
		break;
	case MFC_QOS_EXTRA:
#ifdef CONFIG_EXYNOS8890_BTS_OPTIMIZATION
		if (idx == 1 && (dev->extra_qos != EXTRA_LIMIT_CLK) && dev->is_only_h264_enc) {
			if (dev->extra_qos == EXTRA_NO_LIMIT_MO) {
				bts_ext_scenario_set(TYPE_MFC, TYPE_HIGHPERF, false);
				mfc_debug(2, "QoS extra: changed MO remove -> clk limit\n");
			}
			/* limit clock for QoS table [1] */
			cal_dfs_ext_ctrl(dvfs_int, cal_dfs_rate_lock, true);
			dev->extra_qos = EXTRA_LIMIT_CLK;
			MFC_TRACE_CTX("** QOS extra: limit clk\n");
			mfc_info_ctx("QoS extra: limit clk\n");
		} else if (idx > 7 && (dev->extra_qos != EXTRA_NO_LIMIT_MO)) {
			if (dev->extra_qos == EXTRA_LIMIT_CLK) {
				cal_dfs_ext_ctrl(dvfs_int, cal_dfs_rate_lock, false);
				mfc_debug(2, "QoS extra: changed clk limit -> MO remove\n");
			}
			/* remove MO limitation for QoS table[8]~[10] */
			bts_ext_scenario_set(TYPE_MFC, TYPE_HIGHPERF, true);
			dev->extra_qos = EXTRA_NO_LIMIT_MO;
			MFC_TRACE_CTX("** QOS extra: no limit MO\n");
			mfc_info_ctx("QoS extra: no limit MO\n");
		} else if (idx == 0 || (idx > 1 && idx <= 7)) {
			/* restore default setting for QoS table[0],[2]~[7] */
			if (dev->extra_qos == EXTRA_LIMIT_CLK) {
				cal_dfs_ext_ctrl(dvfs_int, cal_dfs_rate_lock, false);
				dev->extra_qos = EXTRA_DEFAULT;
				MFC_TRACE_CTX("** QOS extra: default\n");
				mfc_info_ctx("QoS extra: default\n");
			} else if (dev->extra_qos == EXTRA_NO_LIMIT_MO) {
				bts_ext_scenario_set(TYPE_MFC, TYPE_HIGHPERF, false);
				dev->extra_qos = EXTRA_DEFAULT;
				MFC_TRACE_CTX("** QOS extra: default\n");
				mfc_info_ctx("QoS extra: default\n");
			}
		}
#endif
		break;
	default:
		mfc_err_ctx("Unknown request for opr [%d]\n", opr_type);
		break;
	}
}

static void mfc_qos_print(struct s5p_mfc_ctx *ctx,
		struct s5p_mfc_qos *qos_table, int index)
{
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	mfc_info_ctx("\tint: %d, mif: %d, cpu: %d, kfc: %d\n",
			qos_table[index].freq_int,
			qos_table[index].freq_mif,
			qos_table[index].freq_cpu,
			qos_table[index].freq_kfc);
#endif
}
static void mfc_qos_add_or_update(struct s5p_mfc_ctx *ctx, int total_mb)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_platdata *pdata = dev->pdata;
	struct s5p_mfc_qos *qos_table = pdata->qos_table;
	int i;

	for (i = (pdata->num_qos_steps - 1); i >= 0; i--) {
		mfc_debug(7, "QoS index: %d\n", i + 1);
		if (total_mb > qos_table[i].thrd_mb) {
			/* Table is different between decoder and encoder */
			if (dev->has_enc_ctx && qos_table[i].has_enc_table) {
				mfc_debug(2, "Table changes %d -> %d\n", i, i - 1);
				i = i - 1;
			}
			if (atomic_read(&dev->qos_req_cur) == 0) {
				mfc_qos_print(ctx, qos_table, i);
				mfc_qos_operate(ctx, MFC_QOS_ADD, i);
				mfc_qos_operate(ctx, MFC_QOS_EXTRA, i);
			} else if (atomic_read(&dev->qos_req_cur) != (i + 1)) {
				mfc_qos_print(ctx, qos_table, i);
				mfc_qos_operate(ctx, MFC_QOS_UPDATE, i);
				mfc_qos_operate(ctx, MFC_QOS_EXTRA, i);
			}
			break;
		}
	}
}

static inline int get_ctx_mb(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec = NULL;
	int mb_width, mb_height, fps, ctx_mb;

	mb_width = (ctx->img_width + 15) / 16;
	mb_height = (ctx->img_height + 15) / 16;
	fps = ctx->framerate / 1000;

	mfc_debug(2, "ctx[%d:%s], %d x %d @ %d fps\n", ctx->num,
			(ctx->type == MFCINST_ENCODER ? "ENC" : "DEC"),
			ctx->img_width, ctx->img_height, fps);

	ctx_mb = mb_width * mb_height * fps;

	if (ctx->type == MFCINST_DECODER) {
		dec = ctx->dec_priv;
		if (dec && dec->is_10bit)
			ctx_mb = (ctx_mb * 133) / 100;
	}

	return ctx_mb;
}

void s5p_mfc_qos_on(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_ctx *qos_ctx;
	int found = 0, total_mb = 0;

	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		total_mb += get_ctx_mb(qos_ctx);
		if (qos_ctx == ctx)
			found = 1;
	}

	if (!found) {
		list_add_tail(&ctx->qos_list, &dev->qos_queue);
		total_mb += get_ctx_mb(ctx);
	}

	dev->has_enc_ctx = 0;
	dev->is_only_h264_enc = 1;
	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		if (qos_ctx->type == MFCINST_ENCODER) {
 			dev->has_enc_ctx = 1;
		}
		if ((qos_ctx->codec_mode != S5P_FIMV_CODEC_H264_ENC)
				&& (qos_ctx->codec_mode != S5P_FIMV_CODEC_H264_MVC_ENC)) {
			dev->is_only_h264_enc = 0;
			mfc_debug(2, "is not 264 encoder: %d\n", qos_ctx->codec_mode);
		} else {
			mfc_debug(2, "is 264 encoder: %d\n", qos_ctx->codec_mode);
		}
	}

	mfc_qos_add_or_update(ctx, total_mb);
}

void s5p_mfc_qos_off(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_ctx *qos_ctx;
	int found = 0, total_mb = 0;

	if (list_empty(&dev->qos_queue)) {
		if (atomic_read(&dev->qos_req_cur) != 0) {
			mfc_err_ctx("MFC request count is wrong!\n");
			mfc_qos_operate(ctx, MFC_QOS_REMOVE, 0);
		}

		return;
	}

	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		total_mb += get_ctx_mb(qos_ctx);
		if (qos_ctx == ctx)
			found = 1;
	}

	if (found) {
		list_del(&ctx->qos_list);
		total_mb -= get_ctx_mb(ctx);
	}

	dev->has_enc_ctx = 0;
	dev->is_only_h264_enc = 1;
	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		if (qos_ctx->type == MFCINST_ENCODER) {
			dev->has_enc_ctx = 1;
		}
		if ((qos_ctx->codec_mode != S5P_FIMV_CODEC_H264_ENC)
				&& (qos_ctx->codec_mode != S5P_FIMV_CODEC_H264_MVC_ENC)) {
			dev->is_only_h264_enc = 0;
			mfc_debug(2, "is not 264 encoder: %d\n", qos_ctx->codec_mode);
		} else {
			mfc_debug(2, "is 264 encoder: %d\n", qos_ctx->codec_mode);
		}
	}

	if (list_empty(&dev->qos_queue))
		mfc_qos_operate(ctx, MFC_QOS_REMOVE, 0);
	else
		mfc_qos_add_or_update(ctx, total_mb);
}
#endif
