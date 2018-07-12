/* regs-dsim.h
 *
 * Register definition file for Samsung MIPI-DSIM driver
 *
 * Copyright (c) 2015 Samsung Electronics
 * Jiun Yu <jiun.yu@samsung.com>
 * Seuni Park <seuni.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_DSIM_H
#define _REGS_DSIM_H

#define DSIM_LINK_STATUS			(0x4)
#define DSIM_STATUS_PLL_STABLE			(1 << 24)

#define DSIM_DPHY_STATUS			(0x8)
#define DSIM_STATUS_STOP_STATE_DAT(_x)		(((_x) & 0xf) << 0)
#define DSIM_STATUS_ULPS_DAT(_x)		(((_x) & 0xf) << 4)
#define DSIM_STATUS_STOP_STATE_CLK		(1 << 8)
#define DSIM_STATUS_ULPS_CLK			(1 << 9)
#define DSIM_STATUS_TX_READY_HS_CLK		(1 << 10)
#define DSIM_STATUS_ULPS_DATA_LANE_GET(x)	(((x) >> 4) & 0xf)

#define DSIM_SWRST				(0xc)
#define DSIM_SWRST_FUNCRST			(1 << 16)
#define DSIM_DPHY_RST				(1 << 8)
#define DSIM_SWRST_RESET			(1 << 0)

#define DSIM_CLKCTRL				(0x10)
#define DSIM_CLKCTRL_TX_REQUEST_HSCLK		(1 << 20)
#define DSIM_CLKCTRL_BYTECLK_EN			(1 << 17)
#define DSIM_CLKCTRL_ESCCLK_EN			(1 << 16)
#define DSIM_CLKCTRL_LANE_ESCCLK_EN_MASK	(0x1f << 8)
#define DSIM_CLKCTRL_LANE_ESCCLK_EN(_x)		((_x) << 8)
#define DSIM_CLKCTRL_ESC_PRESCALER(_x)		((_x) << 0)
#define DSIM_CLKCTRL_ESC_PRESCALER_MASK		(0xffff << 0)

/* Time out register */
#define DSIM_TIMEOUT0				(0x14)
#define DSIM_TIMEOUT_BTA_TOUT(_x)		((_x) << 16)
#define DSIM_TIMEOUT_BTA_TOUT_MASK		(0xff << 16)
#define DSIM_TIMEOUT_LPDR_TOUT(_x)		((_x) << 0)
#define DSIM_TIMEOUT_LPDR_TOUT_MASK		(0xffff << 0)

/* Time out register */
#define DSIM_TIMEOUT1				(0x18)
#define DSIM_TIMEOUT_HSYNC_TOUT(_x)		((_x) << 0)
#define DSIM_TIMEOUT_HSYNC_TOUT_MASK		(0xff << 0)

/* Escape mode register */
#define DSIM_ESCMODE				(0x1c)
#define DSIM_ESCMODE_STOP_STATE_CNT(_x)		((_x) << 21)
#define DSIM_ESCMODE_STOP_STATE_CNT_MASK	(0x7ff << 21)
#define DSIM_ESCMODE_FORCE_STOP_STATE		(1 << 20)
#define DSIM_ESCMODE_FORCE_BTA			(1 << 16)
#define DSIM_ESCMODE_CMD_LPDT			(1 << 7)
#define DSIM_ESCMODE_TRIGGER_RST		(1 << 4)
#define DSIM_ESCMODE_TX_ULPS_DATA		(1 << 3)
#define DSIM_ESCMODE_TX_ULPS_DATA_EXIT		(1 << 2)
#define DSIM_ESCMODE_TX_ULPS_CLK		(1 << 1)
#define DSIM_ESCMODE_TX_ULPS_CLK_EXIT		(1 << 0)

/* Display image resolution register */
#define DSIM_RESOL				(0x20)
#define DSIM_RESOL_VRESOL(x)			(((x) & 0xfff) << 16)
#define DSIM_RESOL_VRESOL_MASK			(0xfff << 16)
#define DSIM_RESOL_HRESOL(x)			(((x) & 0Xfff) << 0)
#define DSIM_RESOL_HRESOL_MASK			(0xfff << 0)
#define DSIM_RESOL_LINEVAL_GET(_v)		(((_v) >> 16) & 0xfff)
#define DSIM_RESOL_HOZVAL_GET(_v)		(((_v) >> 0) & 0xfff)

/* Main display Vporch register */
#define DSIM_VPORCH				(0x24)
#define DSIM_VPORCH_CMD_ALLOW(_x)		((_x) << 28)
#define DSIM_VPORCH_CMD_ALLOW_MASK		(0xf << 28)
#define DSIM_VPORCH_STABLE_VFP(_x)		((_x) << 16)
#define DSIM_VPORCH_STABLE_VFP_MASK		(0x7ff << 16)
#define DSIM_VPORCH_VBP(_x)			((_x) << 0)
#define DSIM_VPORCH_VBP_MASK			(0x7ff << 0)

/* Main display Hporch register */
#define DSIM_HPORCH				(0x28)
#define DSIM_HPORCH_HFP(_x)			((_x) << 16)
#define DSIM_HPORCH_HFP_MASK			(0xffff << 16)
#define DSIM_HPORCH_HBP(_x)			((_x) << 0)
#define DSIM_HPORCH_HBP_MASK			(0xffff << 0)

/* Main display sync area register */
#define DSIM_SYNC				(0x2C)
#define DSIM_SYNC_VSA(_x)			((_x) << 16)
#define DSIM_SYNC_VSA_MASK			(0x7ff << 16)
#define DSIM_SYNC_HSA(_x)			((_x) << 0)
#define DSIM_SYNC_HSA_MASK			(0xffff << 0)

/* Configuration register */
#define DSIM_CONFIG				(0x30)
#define DSIM_CONFIG_NONCONTINUOUS_CLOCK_LANE	(1 << 31)
#define DSIM_CONFIG_CLKLANE_STOP_START		(1 << 30)
#define DSIM_CONFIG_FLUSH_VS			(1 << 29)
#define DSIM_CONFIG_SYNC_INFORM			(1 << 27)
#define DSIM_CONFIG_BURST_MODE			(1 << 26)
#define DSIM_CONFIG_HSYNC_PRESERVE		(1 << 25)
#define DSIM_CONFIG_HSE_DISABLE			(1 << 23)
#define DSIM_CONFIG_HFP_DISABLE			(1 << 22)
#define DSIM_CONFIG_HBP_DISABLE			(1 << 21)
#define DSIM_CONFIG_HSA_DISABLE			(1 << 20)
#define DSIM_CONFIG_CPRS_EN			(1 << 19)
#define DSIM_CONFIG_VIDEO_MODE			(1 << 18)
#define DSIM_CONFIG_VC_CONTROL			(1 << 17)
#define DSIM_CONFIG_VC_ID(_x)			((_x) << 16)
#define DSIM_CONFIG_VC_ID_MASK			(0x3 << 16)
#define DSIM_CONFIG_PIXEL_FORMAT(_x)		((_x) << 12)
#define DSIM_CONFIG_PIXEL_FORMAT_MASK		(0x7 << 12)
#define DSIM_CONFIG_MULTI_PIX			(1 << 11)
#define DSIM_CONFIG_Q_CHANNEL_EN		(1 << 10)
#define DSIM_CONFIG_PER_FRAME_READ_EN		(1 << 9)
#define DSIM_CONFIG_NUM_OF_DATA_LANE(_x)	((_x) << 5)
#define DSIM_CONFIG_NUM_OF_DATA_LANE_MASK	(0x3 << 5)
#define DSIM_CONFIG_LANES_EN(_x)		(((_x) & 0x1f) << 0)

/* Interrupt source register */
#define DSIM_INTSRC				(0x34)
#define DSIM_INTSRC_PLL_STABLE			(1 << 31)
#define DSIM_INTSRC_SW_RST_RELEASE		(1 << 30)
#define DSIM_INTSRC_SFR_PL_FIFO_EMPTY		(1 << 29)
#define DSIM_INTSRC_SFR_PH_FIFO_EMPTY		(1 << 28)
#define DSIM_INTSRC_SFR_PH_FIFO_OVERFLOW	(1 << 26)
#define DSIM_INTSRC_BUS_TURN_OVER		(1 << 25)
#define DSIM_INTSRC_FRAME_DONE			(1 << 24)
#define DSIM_INTSRC_ABNRMAL_CMD_ST		(1 << 22)
#define DSIM_INTSRC_LPDR_TOUT			(1 << 21)
#define DSIM_INTSRC_BTA_TOUT			(1 << 20)
#define DSIM_INTSRC_HSYNC_TOUT			(1 << 19)
#define DSIM_INTSRC_RX_DATA_DONE		(1 << 18)
#define DSIM_INTSRC_ERR_RX_ECC			(1 << 15)

/* Interrupt mask register */
#define DSIM_INTMSK				(0x38)
#define DSIM_INTMSK_PLL_STABLE			(1 << 31)
#define DSIM_INTSRC_SW_RST_RELEASE		(1 << 30)
#define DSIM_INTMSK_SFR_PL_FIFO_EMPTY		(1 << 29)
#define DSIM_INTMSK_SFR_PH_FIFO_EMPTY		(1 << 28)
#define DSIM_INTMSK_SFR_PH_FIFO_OVERFLOW	(1 << 26)
#define DSIM_INTMSK_BUS_TURN_OVER		(1 << 25)
#define DSIM_INTMSK_FRAME_DONE			(1 << 24)
#define DSIM_INTMSK_ABNORMAL_CMD_ST		(1 << 22)
#define DSIM_INTMSK_LPDR_TOUT			(1 << 21)
#define DSIM_INTMSK_BTA_TOUT			(1 << 20)
#define DSIM_INTMSK_HSYNC_TOUT			(1 << 19)
#define DSIM_INTMSK_RX_DATA_DONE		(1 << 18)
#define DSIM_INTMSK_ERR_RX_ECC			(1 << 15)

/* Packet Header FIFO register */
#define DSIM_PKTHDR				(0x3c)
#define DSIM_PKTHDR_ID(_x)			((_x) << 0)
#define DSIM_PKTHDR_DATA0(_x)			((_x) << 8)
#define DSIM_PKTHDR_DATA1(_x)			((_x) << 16)

/* Payload FIFO register */
#define DSIM_PAYLOAD				(0x40)

/* Read FIFO register */
#define DSIM_RXFIFO				(0x44)

/* SFR control Register for Stanby & Shadow*/
#define DSIM_SFR_CTRL				(0x48)
#define DSIM_SFR_CTRL_STANDBY			(1 << 4)
#define DSIM_SFR_CTRL_SHADOW_UPDATE		(1 << 1)
#define DSIM_SFR_CTRL_SHADOW_EN			(1 << 0)

/* FIFO status and control register */
#define DSIM_FIFOCTRL				(0x4C)
#define DSIM_FIFOCTRL_NUMBER_OF_PH_SFR(_x)	(((_x) & 0x3f) << 16)
#define DSIM_FIFOCTRL_NUMBER_OF_PH_SFR_GET(_x)	(((_x) >> 16) & 0x3f)
#define DSIM_FIFOCTRL_EMPTY_RX			(1 << 12)
#define DSIM_FIFOCTRL_FULL_PH_SFR		(1 << 11)
#define DSIM_FIFOCTRL_EMPTY_PH_SFR		(1 << 10)
#define DSIM_FIFOCTRL_FULL_PL_SFR		(1 << 9)
#define DSIM_FIFOCTRL_INIT_RX			(1 << 2)
#define DSIM_FIFOCTRL_INIT_SFR			(1 << 1)

/* Muli slice setting register*/
#define DSIM_CPRS_CTRL				(0x58)
#define DSIM_CPRS_CTRL_MULI_SLICE_PACKET	(1 << 3)
#define DSIM_CPRS_CTRL_NUM_OF_SLICE(_x)		((_x) << 0)
#define DSIM_CPRS_CTRL_NUM_OF_SLICE_MASK	(0x7 << 0)
#define DSIM_CPRS_CTRL_NUM_OF_SLICE_GET(x)	(((x) >> 0) & 0x7)

/*Slice01 size register*/
#define DSIM_SLICE01				(0x5C)
#define DSIM_SLICE01_SIZE_OF_SLICE1(_x)		((_x) << 16)
#define DSIM_SLICE01_SIZE_OF_SLICE1_MASK	(0x1fff << 16)
#define DSIM_SLICE01_SIZE_OF_SLICE1_GET(x)	(((x) >> 16) & 0x1fff)
#define DSIM_SLICE01_SIZE_OF_SLICE0(_x)		((_x) << 0)
#define DSIM_SLICE01_SIZE_OF_SLICE0_MASK	(0x1fff << 0)
#define DSIM_SLICE01_SIZE_OF_SLICE0_GET(x)	(((x) >> 0) & 0x1fff)

/*Slice23 size register*/
#define DSIM_SLICE23				(0x60)
#define DSIM_SLICE23_SIZE_OF_SLICE3(_x)		((_x) << 16)
#define DSIM_SLICE23_SIZE_OF_SLICE3_MASK	(0x1fff << 16)
#define DSIM_SLICE23_SIZE_OF_SLICE3_GET(x)	(((x) >> 16) & 0x1fff)
#define DSIM_SLICE23_SIZE_OF_SLICE2(_x)		((_x) << 0)
#define DSIM_SLICE23_SIZE_OF_SLICE2_MASK	(0x1fff << 0)
#define DSIM_SLICE23_SIZE_OF_SLICE2_GET(x)	(((x) >> 0) & 0x1fff)

/* Command configuration register */
#define DSIM_CMD_CONFIG				(0x78)
#define DSIM_CMD_CONFIG_TE_DETECT_POLARITY	(1 << 25)
#define DSIM_CMD_CONFIG_VSYNC_DETECT_POLARITY	(1 << 24)
#define DSIM_CMD_CONFIG_PKT_GO_RDY		(1 << 22)
#define DSIM_CMD_CONFIG_PKT_GO_EN		(1 << 21)
#define DSIM_CMD_CONFIG_CMD_CTRL_SEL		(1 << 20)
#define DSIM_CMD_CONFIG_PKT_SEND_CNT(_x)	((_x) << 8)
#define DSIM_CMD_CONFIG_PKT_SEND_CNT_MASK	(0xfff << 8)
#define DSIM_CMD_CONFIG_MULTI_CMD_PKT_EN	(1 << 7)
#define DSIM_CMD_CONFIG_MULTI_PKT_CNT(_x)	((_x) << 0)
#define DSIM_CMD_CONFIG_MULTI_PKT_CNT_MASK	(0xffff << 0)

/* TE based command register*/
#define DSIM_CMD_TE_CTRL0			(0x7C)
#define DSIM_CMD_TE_CTRL0_TIME_STABLE_VFP(_x)	((_x) << 16)
#define DSIM_CMD_TE_CTRL0_TIME_STABLE_VFP_MASK	(0xffff << 16)
#define DSIM_CMD_TE_CTRL0_TIME_VSYNC_TOUT(_x)	((_x) << 0)
#define DSIM_CMD_TE_CTRL0_TIME_VSYNC_TOUT_MASK	(0xffff << 0)

/* TE based command register*/
#define DSIM_CMD_TE_CTRL1			(0x80)
#define DSIM_CMD_TE_CTRL1_TIME_TE_PROTECT_ON(_x) ((_x) << 16)
#define DSIM_CMD_TE_CTRL1_TIME_TE_PROTECT_ON_MASK (0xffff << 16)
#define DSIM_CMD_TE_CTRL1_TIME_TE_TOUT(_x)	((_x) << 0)
#define DSIM_CMD_TE_CTRL1_TIME_TE_TOUT_MASK	(0xffff << 0)

/*Command Mode Status register*/
#define DSIM_CMD_STATUS				(0x87)
#define	DSIM_CMD_STATUS_ABNORMAL_CAUSE_ST(_x)	(((_x) & 0x3ff) << 0)

/*BIST generation register*/
#define	DSIM_BIST_GEN				(0x88)
#define DSIM_BIST_GEN_BIST_VFP(_x)		((_x) << 20)
#define DSIM_BIST_GEN_BIST_VFP_MASK		(0x7ff << 20)
#define DSIM_BIST_GEN_BIST_START		(1 << 16)
#define	DSIM_BIST_GEN_COLORIMETRIC_FORMAT	(1 << 6)
#define	DSIM_BIST_GEN_HSYNC_POLARITY		(1 << 5)
#define	DSIM_BIST_GEN_VSYNC_POLARITY		(1 << 4)
#define	DSIM_BIST_GEN_BIST_BAR_WIDTH		(1 << 3)
#define	DSIM_BIST_GEN_BIST_TYPE(_x)		((_x) << 1)
#define	DSIM_BIST_GEN_BIST_TYPE_MASK		(0x3 << 1)
#define	DSIM_BIST_GEN_BIST_EN			(1 << 0)

/*DSIM to CSI loopback register*/
#define	DSIM_CSIS_LB				(0x8C)
#define	DSIM_CSIS_LB_CSIS_LB_EN			(1 << 8)
#define DSIM_CSIS_LB_CSIS_PH(_x)		((_x) << 0)
#define DSIM_CSIS_LB_CSIS_PH_MASK		(0xff << 0)

/* PLL control register */
#define DSIM_PLLCTRL				(0x94)
#define DSIM_PLLCTRL_DPDN_SWAP_CLK		(1 << 25)
#define DSIM_PLLCTRL_DPDN_SWAP_DATA		(1 << 24)
#define DSIM_PLLCTRL_PLL_EN			(1 << 23)
#define DSIM_PLLCTRL_PMS_MASK			(0x7ffff << 0)

/* M_PLL CTR1 register*/
#define DSIM_PLL_CTRL1				(0x98)
#define DSIM_PLL_CTRL1_M_PLL_CTRL1		(0xffffffff << 0)

/* M_PLL CTR1 register*/
#define DSIM_PLL_CTRL2				(0x9C)
#define DSIM_PLL_CTRL2_M_PLL_CTRL2		(0xffffffff << 0)

/* PLL timer register */
#define DSIM_PLLTMR				(0xa0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL				(0xa4)
#define DSIM_PHYCTRL_VREG			(1 << 26)
#define DSIM_PHYCTRL_B_DPHYCTL0(_x)		((_x) << 0)
#define DSIM_PHYCTRL_B_DPHYCTL0_MASK		(0x3ff << 0)

/* D-PHY Master global operating timing register */
#define DSIM_PHYTIMING				(0xb4)
#define DSIM_PHYTIMING_M_TLPXCTL(_x)		((_x) << 8)
#define DSIM_PHYTIMING_M_TLPXCTL_MASK		(0xff << 8)
#define DSIM_PHYTIMING_M_THSEXITCTL(_x)		((_x) << 0)
#define DSIM_PHYTIMING_M_THSEXITCTL_MASK	(0xff << 0)

#define DSIM_PHYTIMING1				(0xb8)
#define DSIM_PHYTIMING1_M_TCLKPRPRCTL(_x)	((_x) << 24)
#define DSIM_PHYTIMING1_M_TCLKPRPRCTL_MASK	(0xff << 24)
#define DSIM_PHYTIMING1_M_TCLKZEROCTL(_x)	((_x) << 16)
#define DSIM_PHYTIMING1_M_TCLKZEROCTL_MASK	(0xff << 16)
#define DSIM_PHYTIMING1_M_TCLKPOSTCTL(_x)	((_x) << 8)
#define DSIM_PHYTIMING1_M_TCLKPOSTCTL_MASK	(0xff << 8)
#define DSIM_PHYTIMING1_M_TCLKTRAILCTL(_x)	((_x) << 0)
#define DSIM_PHYTIMING1_M_TCLKTRAILCTL_MASK	(0xff << 0)

#define DSIM_PHYTIMING2				(0xbc)
#define DSIM_PHYTIMING2_M_THSPRPRCTL(_x)	((_x) << 16)
#define DSIM_PHYTIMING2_M_THSPRPRCTL_MASK	(0xff << 16)
#define DSIM_PHYTIMING2_M_THSZEROCTL(_x)	((_x) << 8)
#define DSIM_PHYTIMING2_M_THSZEROCTL_MASK	(0xff << 8)
#define DSIM_PHYTIMING2_M_THSTRAILCTL(_x)	((_x) << 0)
#define DSIM_PHYTIMING2_M_THSTRAILCTL_MASK	(0xff << 0)

#endif /* _REGS_DSIM_H */
