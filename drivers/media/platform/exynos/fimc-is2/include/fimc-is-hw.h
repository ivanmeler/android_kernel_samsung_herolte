/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_H
#define FIMC_IS_HW_H

#include "fimc-is-type.h"

/* TODO: remove defined config to code clean */
#if defined(CONFIG_FIMC_IS_V4_0_0)
#include "../ischain/fimc-is-v4_0_0/fimc-is-hw-chain.h"
#elif defined(CONFIG_FIMC_IS_V3_11_0)
#include "../ischain/fimc-is-v3_11_0/fimc-is-hw-chain.h"
#endif

#define CSI_VIRTUAL_CH_0	0
#define CSI_VIRTUAL_CH_1	1
#define CSI_VIRTUAL_CH_2	2
#define CSI_VIRTUAL_CH_3	3
#define CSI_VIRTUAL_CH_MAX	4

#define CSI_DATA_LANES_1	0
#define CSI_DATA_LANES_2	1
#define CSI_DATA_LANES_3	2
#define CSI_DATA_LANES_4	3

#define CSI_MODE_CH0_ONLY	0
#define CSI_MODE_DT_ONLY	1
#define CSI_MODE_VC_ONLY	2
#define CSI_MODE_VC_DT		3

#define HW_FORMAT_YUV420_8BIT	0x18
#define HW_FORMAT_YUV420_10BIT	0x19
#define HW_FORMAT_YUV422_8BIT	0x1E
#define HW_FORMAT_YUV422_10BIT	0x1F
#define HW_FORMAT_RGB565	0x22
#define HW_FORMAT_RGB666	0x23
#define HW_FORMAT_RGB888	0x24
#define HW_FORMAT_RAW6		0x28
#define HW_FORMAT_RAW7		0x29
#define HW_FORMAT_RAW8		0x2A
#define HW_FORMAT_RAW10 	0x2B
#define HW_FORMAT_RAW12 	0x2C
#define HW_FORMAT_RAW14 	0x2D
#define HW_FORMAT_USER		0x30
#define HW_FORMAT_UNKNOWN	0x3F

/*
 * Get each lane speed (Mbps)
 * w : width, h : height, fps : framerate
 * bit : bit width per pixel, lanes : total lane number (1 ~ 4)
 * margin : S/W margin (15 means 15%)
 */
#define CSI_GET_LANE_SPEED(w, h, fps, bit, lanes, margin) \
	({u64 tmp; tmp = ((u64)w) * ((u64)h) * ((u64)fps) * ((u64)bit) / (lanes); \
	  tmp *= (100 + margin); tmp /= 100; tmp /= 1000000; (u32)tmp;})

/*
 * Get binning ratio.
 * The ratio is expressed by 0.5 step(500)
 * The improtant thing is that it was round off the ratio to the closet 500 unit.
 */
#define BINNING(x, y) (DIV_ROUND_CLOSEST((x) * 1000 / (y), 500) * 500)

/*
 * Get size ratio.
 * w : width, h : height
 * The return value is defined of the enum ratio_size.
 */
#define SIZE_RATIO(w, h) ((w) * 10 / (h))

/*
 * This enum will be used in order to know the size ratio based upon RATIO mecro return value.
 */
enum ratio_size {
	RATIO_1_1		= 1,
	RATIO_11_9		= 12,
	RATIO_4_3		= 13,
	RATIO_3_2		= 15,
	RATIO_5_3		= 16,
	RATIO_16_9		= 17,
};

/*
 * This enum will be used for masking each interrupt masking.
 * The irq_ids params which masked by shifting this bit(id)
 * was sended to csi_hw_irq_msk.
 */
enum csis_hw_irq_id {
	CSIS_IRQ_ID			= 0,
	CSIS_IRQ_CRC			= 1,
	CSIS_IRQ_ECC			= 2,
	CSIS_IRQ_WRONG_CFG		= 3,
	CSIS_IRQ_OVERFLOW_VC		= 4,
	CSIS_IRQ_LOST_FE_VC		= 5,
	CSIS_IRQ_LOST_FS_VC		= 6,
	CSIS_IRQ_SOT_VC			= 7,
	CSIS_IRQ_FRAME_END_VC		= 8,
	CSIS_IRQ_FRAME_START_VC		= 9,
	CSIS_IRQ_LINE_END_VC		= 10,
	CSIS_IRQ_DMA_FRM_START_VC	= 11,
	CSIS_IRQ_DMA_FRM_END_VC		= 12,
	CSIS_IRQ_ABORT_ERROR		= 13,
	CSIS_IRQ_ABORT_DONE		= 14,
	CSIS_IRQ_OTF_OVERLAP		= 15,
	CSIS_IRQ_END,
};

/*
 * This enum will be used for current error status by reading interrupt source.
 */
enum csis_hw_err_id {
	CSIS_ERR_ID = 0,
	CSIS_ERR_CRC = 1,
	CSIS_ERR_ECC = 2,
	CSIS_ERR_WRONG_CFG = 3,
	CSIS_ERR_OVERFLOW_VC = 4,
	CSIS_ERR_LOST_FE_VC = 5,
	CSIS_ERR_LOST_FS_VC = 6,
	CSIS_ERR_SOT_VC = 7,
	CSIS_ERR_OTF_OVERLAP = 8,
	CSIS_ERR_DMA_ERR_DMAFIFO_FULL = 9,
	CSIS_ERR_DMA_ERR_TRXFIFO_FULL = 10,
	CSIS_ERR_DMA_ERR_BRESP_ERR = 11,
	CSIS_ERR_END
};

/*
 * This enum will be used in csi_hw_s_control api to set specific functions.
 */
enum csis_hw_control_id {
	CSIS_CTRL_INTERLEAVE_MODE,
	CSIS_CTRL_LINE_RATIO,
	CSIS_CTRL_BUS_WIDTH,
	CSIS_CTRL_DMA_ABORT_REQ,
};

/*
 * This struct will be used in csi_hw_g_irq_src api.
 * In csi_hw_g_irq_src, all interrupt source status should be
 * saved to this structure. otf_start, otf_end, dma_start, dma_end,
 * line_end fields has virtual channel bit set info.
 * Ex. If otf end interrupt occured in virtual channel 0 and 2 and
 *        dma end interrupt occured in virtual channel 0 only,
 *   .otf_end = 0b0101
 *   .dma_end = 0b0001
 */
struct csis_irq_src {
	u32			otf_start;
	u32			otf_end;
	u32			dma_start;
	u32			dma_end;
	u32			line_end;
	bool			err_flag;
	u32			err_id[CSI_VIRTUAL_CH_MAX];
};

struct fimc_is_vci_config {
	u32			map;
	u32			hwformat;
};

struct fimc_is_vci {
	u32				pixelformat;
	struct fimc_is_vci_config	config[CSI_VIRTUAL_CH_MAX];
};

/*
 * This enum will be used for masking each interrupt masking.
 * The irq_ids params which masked by shifting this bit(id)
 * was sended to flite_hw_s_irq_msk.
 */
enum flite_hw_irq_id {
	FLITE_MASK_IRQ_START		= 0,
	FLITE_MASK_IRQ_END		= 1,
	FLITE_MASK_IRQ_OVERFLOW		= 2,
	FLITE_MASK_IRQ_LAST_CAPTURE	= 3,
	FLITE_MASK_IRQ_LINE		= 4,
	FLITE_MASK_IRQ_ALL,
};

/*
 * This enum will be used for current status by reading interrupt source.
 */
enum flite_hw_status_id {
	FLITE_STATUS_IRQ_SRC_START		= 0,
	FLITE_STATUS_IRQ_SRC_END		= 1,
	FLITE_STATUS_IRQ_SRC_OVERFLOW		= 2,
	FLITE_STATUS_IRQ_SRC_LAST_CAPTURE	= 3,
	FLITE_STATUS_OFY			= 4,
	FLITE_STATUS_OFCR			= 5,
	FLITE_STATUS_OFCB			= 6,
	FLITE_STATUS_MIPI_VALID			= 7,
	FLITE_STATUS_IRQ_SRC_LINE		= 8,
	FLITE_STATUS_ALL,
};

/*
 * This enum will be used in flite_hw_s_control api to set specific functions.
 */
enum flite_hw_control_id {
	FLITE_CTRL_TEST_PATTERN,
	FLITE_CTRL_LINE_RATIO,
};

/*
 * ******************
 * MIPI-CSIS H/W APIS
 * ******************
 */
void csi_hw_phy_otp_config(u32 __iomem *base_reg, u32 instance);
int csi_hw_reset(u32 __iomem *base_reg);
int csi_hw_s_settle(u32 __iomem *base_reg, u32 settle);
int csi_hw_s_lane(u32 __iomem *base_reg, struct fimc_is_image *img, u32 lanes, u32 mipi_speed);
int csi_hw_s_control(u32 __iomem *base_reg, u32 id, u32 value);
int csi_hw_s_config(u32 __iomem *base_reg, u32 channel, struct fimc_is_vci_config *config, u32 width, u32 height);
int csi_hw_s_irq_msk(u32 __iomem *base_reg, bool on);
int csi_hw_g_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear);
int csi_hw_enable(u32 __iomem *base_reg);
int csi_hw_disable(u32 __iomem *base_reg);
int csi_hw_dump(u32 __iomem *base_reg);
#if defined(CONFIG_USE_CSI_DMAOUT_FEATURE)
void csi_hw_s_dma_addr(u32 __iomem *base_reg, u32 vc, u32 number, u32 addr);
void csi_hw_s_output_dma(u32 __iomem *base_reg, u32 vc, bool enable);
bool csi_hw_g_output_dma_enable(u32 __iomem *base_reg, u32 vc);
bool csi_hw_g_output_cur_dma_enable(u32 __iomem *base_reg, u32 vc);
int csi_hw_s_config_dma(u32 __iomem *base_reg, u32 channel, struct fimc_is_image *image);
#endif

/*
 * ************************************
 * ISCHAIN AND CAMIF CONFIGURE H/W APIS
 * ************************************
 */
int fimc_is_hw_group_cfg(void *group_data);
int fimc_is_hw_group_open(void *group_data);
int fimc_is_hw_camif_cfg(void *sensor_data);
int fimc_is_hw_camif_open(void *sensor_data);
int fimc_is_hw_ischain_cfg(void *ischain_data);
int fimc_is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id);
int fimc_is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id);
int fimc_is_hw_request_irq(void *itfc_data, int hw_id);
int fimc_is_hw_slot_id(int hw_id);
int fimc_is_get_hw_list(int group_id, int *hw_list);
int fimc_is_hw_set_fullbypass(void *itfc_data, int hw_id, bool bypass);
int fimc_is_hw_set_chain_interrupt(void *itfc_data);
bool fimc_is_hw_frame_done_with_dma(void);
bool fimc_is_has_mcsc(void);
/*
 * *****************
 * FIMC-BNS H/W APIS
 * *****************
 */
void flite_hw_reset(u32 __iomem *base_reg);
void flite_hw_enable(u32 __iomem *base_reg);
void flite_hw_disable(u32 __iomem *base_reg);
void flite_hw_s_control(u32 __iomem *base_reg, u32 id, u32 value);
int flite_hw_set_bns(u32 __iomem *base_reg, bool enable, struct fimc_is_image *image);
void flite_hw_set_fmt_source(u32 __iomem *base_reg, struct fimc_is_image *image);
void flite_hw_set_fmt_dma(u32 __iomem *base_reg, struct fimc_is_image *image);
void flite_hw_set_output_dma(u32 __iomem *base_reg, bool enable);
bool flite_hw_get_output_dma(u32 __iomem *base_reg);
void flite_hw_set_dma_addr(u32 __iomem *base_reg, u32 number, bool use, u32 addr);
void flite_hw_set_output_otf(u32 __iomem *base_reg, bool enable);
void flite_hw_set_interrupt_mask(u32 __iomem *base_reg, bool enable, u32 irq_ids);
u32 flite_hw_get_interrupt_mask(u32 __iomem *base_reg, u32 irq_ids);
u32 flite_hw_get_status(u32 __iomem *base_reg, u32 status_ids, bool clear);
void flite_hw_set_path(u32 __iomem *base_reg, int instance,
		int csi_ch, int flite_ch, int taa_ch, int isp_ch);
void flite_hw_dump(u32 __iomem *base_reg);

/*
 * ********************
 * RUNTIME-PM FUNCTIONS
 * ********************
 */
int fimc_is_sensor_runtime_suspend_pre(struct device *dev);
int fimc_is_sensor_runtime_resume_pre(struct device *dev);

int fimc_is_ischain_runtime_suspend_post(struct device *dev);
int fimc_is_ischain_runtime_resume_pre(struct device *dev);
int fimc_is_ischain_runtime_resume_post(struct device *dev);

int fimc_is_ischain_runtime_suspend(struct device *dev);
int fimc_is_ischain_runtime_resume(struct device *dev);

int fimc_is_runtime_suspend_post(struct device *dev);
#endif
