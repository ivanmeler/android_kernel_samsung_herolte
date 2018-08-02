/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Sung-Hyun Na <sunghyun.na@samsung.com>
 *
 * USBPHY configuration definitions for Samsung USB PHY CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PHY_SAMSUNG_USB_FW_CAL_H__
#define __PHY_SAMSUNG_USB_FW_CAL_H__

#define EXYNOS_USBCON_VER_01_0_0	0x0100	/* Istor	*/
#define EXYNOS_USBCON_VER_01_0_1	0x0101	/* JF 3.0	*/
#define EXYNOS_USBCON_VER_01_MAX	0x01FF

#define EXYNOS_USBCON_VER_02_0_0	0x0200	/* Insel-D, Island	*/
#define EXYNOS_USBCON_VER_02_0_1	0x0201	/* JF EVT0 2.0 Host	*/
#define EXYNOS_USBCON_VER_02_1_0	0x0210
#define EXYNOS_USBCON_VER_02_1_1	0x0211	/* JF EVT1 2.0 Host	*/
#define EXYNOS_USBCON_VER_02_MAX	0x02FF

#define EXYNOS_USBCON_VER_F2_0_0	0xF200
#define EXYNOS_USBCON_VER_F2_MAX	0xF2FF

enum exynos_usbphy_mode {
	USBPHY_MODE_DEV = 0,
	USBPHY_MODE_HOST = 1,

	/* usb phy for uart bypass mode */
	USBPHY_MODE_BYPASS = 0x10,
};

enum exynos_usbphy_refclk {
	USBPHY_REFCLK_DIFF_100MHZ = 0x80 | 0x27,
	USBPHY_REFCLK_DIFF_26MHZ = 0x80 | 0x02,
	USBPHY_REFCLK_DIFF_24MHZ = 0x80 | 0x2a,

	USBPHY_REFCLK_EXT_50MHZ = 0x07,
	USBPHY_REFCLK_EXT_24MHZ = 0x05,
	USBPHY_REFCLK_EXT_12MHZ = 0x02,
};

enum exynos_usbphy_refsel {
	USBPHY_REFSEL_CLKCORE = 0x2,
	USBPHY_REFSEL_EXT_OSC = 0x1,
	USBPHY_REFSEL_EXT_XTAL = 0x0,

	USBPHY_REFSEL_DIFF_PAD = 0x6,
	USBPHY_REFSEL_DIFF_INTERNAL = 0x4,
	USBPHY_REFSEL_DIFF_SINGLE = 0x3,
};

enum exynos_usbphy_utmi {
	USBPHY_UTMI_FREECLOCK, USBPHY_UTMI_PHYCLOCK,
};

/* HS PHY tune parameter */
struct exynos_usbphy_hs_tune {
	u8 tx_vref;
	u8 tx_pre_emp;
	u8 tx_pre_emp_plus;
	u8 tx_res;
	u8 tx_rise;
	u8 tx_hsxv;
	u8 tx_fsls;
	u8 rx_sqrx;
	u8 compdis;
	u8 otg;
	bool enalbe_user_imp;
	u8 user_imp_value;
	enum exynos_usbphy_utmi utim_clk;
};

/* SS PHY tune parameter */
struct exynos_usbphy_ss_tune {
	/* TX Swing Level*/
	u8 tx_boost_level;
	u8 tx_swing_level;
	u8 tx_swing_full;
	u8 tx_swing_low;
	/* TX De-Emphasis */
	u8 tx_deemphasis_mode;
	u8 tx_deemphasis_3p5db;
	u8 tx_deemphasis_6db;
	/* SSC Operation*/
	u8 enable_ssc;
	u8 ssc_range;
	/* Loss-of-Signal detector threshold level */
	u8 los_bias;
	/* Loss-of-Signal mask width */
	u16 los_mask_val;
	/* RX equalizer mode */
	u8 enable_fixed_rxeq_mode;
	u8 fix_rxeq_value;

	u8 set_crport_level_en;
	u8 set_crport_mpll_charge_pump;
};

/**
 * struct exynos_usbphy_info : USBPHY information to share USBPHY CAL code
 * @version: PHY controller version
 *	     0x0100 - for EXYNOS_USB3 : EXYNOS7420, EXYNOS7890
 *	     0x0101 -			EXYNOS8890
 *	     0x0200 - for EXYNOS_USB2 : EXYNOS7580, EXYNOS3475
 *	     0x0210 -			EXYNOS8890_EVT1
 *	     0xF200 - for EXT	      : EXYNOS7420_HSIC
 * @refclk: reference clock frequency for USBPHY
 * @refsrc: reference clock source path for USBPHY
 * @use_io_for_ovc: use over-current notification io for USBLINK
 * @regs_base: base address of PHY control register *
 */

struct exynos_usbphy_info {
	u32	version;
	enum exynos_usbphy_refclk refclk;
	enum exynos_usbphy_refsel refsel;

	bool use_io_for_ovc;
	bool common_block_enable;
	bool not_used_vbus_pad;

	void __iomem *regs_base;

	/* HS PHY tune parameter */
	struct exynos_usbphy_hs_tune *hs_tune;

	/* SS PHY tune parameter */
	struct exynos_usbphy_ss_tune *ss_tune;
};



#endif	/* __PHY_SAMSUNG_USB_FW_CAL_H__ */
