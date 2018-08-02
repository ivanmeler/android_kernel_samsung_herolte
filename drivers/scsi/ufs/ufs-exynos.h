/*
 * UFS Host Controller driver for Exynos specific extensions
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _UFS_EXYNOS_H_
#define _UFS_EXYNOS_H_

#include <linux/pm_qos.h>

/*
 * Exynos's Vendor specific registers for UFSHCI
 */
#define HCI_TXPRDT_ENTRY_SIZE		0x00
#define HCI_RXPRDT_ENTRY_SIZE		0x04
#define HCI_TO_CNT_DIV_VAL              0x08
#define HCI_1US_TO_CNT_VAL		0x0C
 #define CNT_VAL_1US_MASK	0x3ff
#define HCI_INVALID_UPIU_CTRL		0x10
#define HCI_INVALID_UPIU_BADDR		0x14
#define HCI_INVALID_UPIU_UBADDR		0x18
#define HCI_INVALID_UTMR_OFFSET_ADDR	0x1C
#define HCI_INVALID_UTR_OFFSET_ADDR	0x20
#define HCI_INVALID_DIN_OFFSET_ADDR	0x24
#define HCI_DBR_TIMER_CONFIG		0x28
#define HCI_DBR_TIMER_STATUS		0x2C
#define HCI_VENDOR_SPECIFIC_IS		0x38
#define HCI_VENDOR_SPECIFIC_IE		0x3C
#define HCI_UTRL_NEXUS_TYPE		0x40
#define HCI_UTMRL_NEXUS_TYPE		0x44
#define HCI_E2EFC_CTRL			0x48
#define HCI_SW_RST			0x50
 #define UFS_LINK_SW_RST	(1 << 0)
 #define UFS_UNIPRO_SW_RST	(1 << 1)
 #define UFS_SW_RST_MASK	(UFS_UNIPRO_SW_RST | UFS_LINK_SW_RST)
#define HCI_LINK_VERSION		0x54
#define HCI_IDLE_TIMER_CONFIG		0x58
#define HCI_RX_UPIU_MATCH_ERROR_CODE	0x5C
#define HCI_DATA_REORDER		0x60
#define HCI_MAX_DOUT_DATA_SIZE		0x64
#define HCI_UNIPRO_APB_CLK_CTRL		0x68
#define HCI_AXIDMA_RWDATA_BURST_LEN	0x6C
#define HCI_GPIO_OUT			0x70
#define HCI_WRITE_DMA_CTRL		0x74
#define HCI_ERROR_EN_PA_LAYER		0x78
#define HCI_ERROR_EN_DL_LAYER		0x7C
#define HCI_ERROR_EN_N_LAYER		0x80
#define HCI_ERROR_EN_T_LAYER		0x84
#define HCI_ERROR_EN_DME_LAYER		0x88
#define HCI_REQ_HOLD_EN			0xAC
#define HCI_CLKSTOP_CTRL		0xB0
 #define REFCLK_STOP		BIT(2)
 #define UNIPRO_MCLK_STOP	BIT(1)
 #define UNIPRO_PCLK_STOP	BIT(0)
 #define CLK_STOP_ALL		(REFCLK_STOP |\
				UNIPRO_MCLK_STOP |\
				UNIPRO_PCLK_STOP)
#define HCI_FORCE_HCS			0xB4
 #define REFCLK_STOP_EN		BIT(7)
 #define UNIPRO_PCLK_STOP_EN	BIT(6)
 #define UNIPRO_MCLK_STOP_EN	BIT(5)
 #define HCI_CORECLK_STOP_EN	BIT(4)
 #define CLK_STOP_CTRL_EN_ALL	(REFCLK_STOP_EN |\
				UNIPRO_PCLK_STOP_EN |\
				UNIPRO_MCLK_STOP_EN)
#define HCI_FSM_MONITOR                  0xC0
#define HCI_PRDT_HIT_RATIO               0xC4
#define HCI_DMA0_MONITOR_STATE           0xC8
#define HCI_DMA0_MONITOR_CNT             0xCC
#define HCI_DMA1_MONITOR_STATE           0xD0
#define HCI_DMA1_MONITOR_CNT             0xD4

/* Device fatal error */
#define DFES_ERR_EN	BIT(31)
#define DFES_DEF_DL_ERRS	(UIC_DATA_LINK_LAYER_ERROR_RX_BUF_OF |\
				 UIC_DATA_LINK_LAYER_ERROR_PA_INIT)
#define DFES_DEF_N_ERRS		(UIC_NETWORK_UNSUPPORTED_HEADER_TYPE |\
				 UIC_NETWORK_BAD_DEVICEID_ENC |\
				 UIC_NETWORK_LHDR_TRAP_PACKET_DROPPING)
#define DFES_DEF_T_ERRS		(UIC_TRANSPORT_UNSUPPORTED_HEADER_TYPE |\
				 UIC_TRANSPORT_UNKNOWN_CPORTID |\
				 UIC_TRANSPORT_NO_CONNECTION_RX |\
				 UIC_TRANSPORT_BAD_TC)

/* TXPRDT defines */
#define PRDT_PREFECT_EN		BIT(31)
#define PRDT_SET_SIZE(x)	((x) & 0x1F)

enum {
	UNIP_PA_LYR = 0,
	UNIP_DL_LYR,
	UNIP_N_LYR,
	UNIP_T_LYR,
	UNIP_DME_LYR,
};

/*
 * UNIPRO registers
 */
#define UNIP_COMP_VERSION			0x000
#define UNIP_COMP_INFO				0x004
#define UNIP_COMP_RESET				0x010
#define UNIP_DME_POWERON_REQ			0x040
#define UNIP_DME_POWERON_CNF_RESULT		0x044
#define UNIP_DME_POWEROFF_REQ			0x048
#define UNIP_DME_POWEROFF_CNF_RESULT		0x04C
#define UNIP_DME_RESET_REQ			0x050
#define UNIP_DME_RESET_REQ_LEVEL		0x054
#define UNIP_DME_ENABLE_REQ			0x058
#define UNIP_DME_ENABLE_CNF_RESULT		0x05C
#define UNIP_DME_ENDPOINTRESET_REQ		0x060
#define UNIP_DME_ENDPOINTRESET_CNF_RESULT	0x064
#define UNIP_DME_LINKSTARTUP_REQ		0x068
#define UNIP_DME_LINKSTARTUP_CNF_RESULT		0x06C
#define UNIP_DME_HIBERN8_ENTER_REQ		0x070
#define UNIP_DME_HIBERN8_ENTER_CNF_RESULT	0x074
#define UNIP_DME_HIBERN8_ENTER_IND_RESULT	0x078
#define UNIP_DME_HIBERN8_EXIT_REQ		0x080
#define UNIP_DME_HIBERN8_EXIT_CNF_RESULT	0x084
#define UNIP_DME_HIBERN8_EXIT_IND_RESULT	0x088
#define UNIP_DME_PWR_REQ			0x090
#define UNIP_DME_PWR_REQ_POWERMODE		0x094
#define UNIP_DME_PWR_REQ_LOCALL2TIMER0		0x098
#define UNIP_DME_PWR_REQ_LOCALL2TIMER1		0x09C
#define UNIP_DME_PWR_REQ_LOCALL2TIMER2		0x0A0
#define UNIP_DME_PWR_REQ_REMOTEL2TIMER0		0x0A4
#define UNIP_DME_PWR_REQ_REMOTEL2TIMER1		0x0A8
#define UNIP_DME_PWR_REQ_REMOTEL2TIMER2		0x0AC
#define UNIP_DME_PWR_CNF_RESULT			0x0B0
#define UNIP_DME_PWR_IND_RESULT			0x0B4
#define UNIP_DME_TEST_MODE_REQ			0x0B8
#define UNIP_DME_TEST_MODE_CNF_RESULT		0x0BC
#define UNIP_DME_ERROR_IND_LAYER		0x0C0
#define UNIP_DME_ERROR_IND_ERRCODE		0x0C4
#define UNIP_DME_PACP_CNFBIT			0x0C8
#define UNIP_DME_DL_FRAME_IND			0x0D0
#define UNIP_DME_INTR_STATUS			0x0E0
#define UNIP_DME_INTR_ENABLE			0x0E4
#define UNIP_DME_GETSET_ADDR			0x100
#define UNIP_DME_GETSET_WDATA			0x104
#define UNIP_DME_GETSET_RDATA			0x108
#define UNIP_DME_GETSET_CONTROL			0x10C
#define UNIP_DME_GETSET_RESULT			0x110
#define UNIP_DME_PEER_GETSET_ADDR		0x120
#define UNIP_DME_PEER_GETSET_WDATA		0x124
#define UNIP_DME_PEER_GETSET_RDATA		0x128
#define UNIP_DME_PEER_GETSET_CONTROL		0x12C
#define UNIP_DME_PEER_GETSET_RESULT		0x130
#define UNIP_DME_DIRECT_GETSET_BASE		0x134
#define UNIP_DME_DIRECT_GETSET_ERR_ADDR		0x138
#define UNIP_DME_DIRECT_GETSET_ERR_CODE		0x13c
#define UNIP_DME_INTR_ERROR_CODE		0x140
#define UNIP_DME_DEEPSTALL_ENTER_REQ		0x144
#define UNIP_DME_DISCARD_CPORT_ID		0x148
#define UNIP_DBG_DME_CTRL_STATE			0x14C
#define UNIP_DBG_FORCE_DME_CTRL_STATE		0x150
#define UNIP_DBG_AUTO_DME_LINKSTARTUP		0x158
#define UNIP_DBG_PA_CTRLSTATE			0x15C
#define UNIP_DBG_PA_TX_STATE			0x160
#define UNIP_DBG_BREAK_DME_CTRL_STATE		0x164
#define UNIP_DBG_STEP_DME_CTRL_STATE		0x168
#define UNIP_DBG_NEXT_DME_CTRL_STATE		0x16C

/*
 * UFS Protector registers
 */
#define UFSPRCTRL	0x000
#define UFSPRSTAT	0x008
#define UFSPRSECURITY	0x010
 #define NSSMU		BIT(14)
#define DESCTYPE(type)		((type & 0x3) << 19)
#define ARPROTPRDT(type)	((type & 0x3) << 16)
#define ARPROTDESC(type)	((type & 0x3) << 9)
#define ARPROTDATA(type)	((type & 0x3) << 6)
#define AWPROTDESC(type)	((type & 0x3) << 3)
#define AWPROTDATA(type)	((type & 0x3))
#define CFG_AXPROT(type)	(ARPROTPRDT(type) | ARPROTDESC(type) | \
				ARPROTDATA(type) | AWPROTDESC(type) | \
				AWPROTDATA(type))
#define SMU_ABORT(type)		((type & 0x2) >> 1)

#define UFSPVERSION	0x01C
#define UFSPRENCKEY0	0x020
#define UFSPRENCKEY1	0x024
#define UFSPRENCKEY2	0x028
#define UFSPRENCKEY3	0x02C
#define UFSPRENCKEY4	0x030
#define UFSPRENCKEY5	0x034
#define UFSPRENCKEY6	0x038
#define UFSPRENCKEY7	0x03C
#define UFSPRTWKEY0	0x040
#define UFSPRTWKEY1	0x044
#define UFSPRTWKEY2	0x048
#define UFSPRTWKEY3	0x04C
#define UFSPRTWKEY4	0x050
#define UFSPRTWKEY5	0x054
#define UFSPRTWKEY6	0x058
#define UFSPRTWKEY7	0x05C
#define UFSPWCTRL	0x100
#define UFSPWSTAT	0x108
#define UFSPWSECURITY	0x110
#define UFSPWENCKEY0	0x120
#define UFSPWENCKEY1	0x124
#define UFSPWENCKEY2	0x128
#define UFSPWENCKEY3	0x12C
#define UFSPWENCKEY4	0x130
#define UFSPWENCKEY5	0x134
#define UFSPWENCKEY6	0x138
#define UFSPWENCKEY7	0x13C
#define UFSPWTWKEY0	0x140
#define UFSPWTWKEY1	0x144
#define UFSPWTWKEY2	0x148
#define UFSPWTWKEY3	0x14C
#define UFSPWTWKEY4	0x150
#define UFSPWTWKEY5	0x154
#define UFSPWTWKEY6	0x158
#define UFSPWTWKEY7	0x15C
#define UFSPSBEGIN0	0x200
#define UFSPSEND0	0x204
#define UFSPSLUN0	0x208
#define UFSPSCTRL0	0x20C
#define UFSPSBEGIN1	0x210
#define UFSPSEND1	0x214
#define UFSPSLUN1	0x218
#define UFSPSCTRL1	0x21C
#define UFSPSBEGIN2	0x220
#define UFSPSEND2	0x224
#define UFSPSLUN2	0x228
#define UFSPSCTRL2	0x22C
#define UFSPSBEGIN3	0x230
#define UFSPSEND3	0x234
#define UFSPSLUN3	0x238
#define UFSPSCTRL3	0x23C
#define UFSPSBEGIN4	0x240
#define UFSPSEND4	0x244
#define UFSPSLUN4	0x248
#define UFSPSCTRL4	0x24C
#define UFSPSBEGIN5	0x250
#define UFSPSEND5	0x254
#define UFSPSLUN5	0x258
#define UFSPSCTRL5	0x25C
#define UFSPSBEGIN6	0x260
#define UFSPSEND6	0x264
#define UFSPSLUN6	0x268
#define UFSPSCTRL6	0x26C
#define UFSPSBEGIN7	0x270
#define UFSPSEND7	0x274
#define UFSPSLUN7	0x278
#define UFSPSCTRL7	0x27C

/*
 * MIBs for PA debug registers
 */
#define PA_DBG_CLK_PERIOD		0x9514
#define PA_DBG_TXPHY_CFGUPDT		0x9518
#define PA_DBG_RXPHY_CFGUPDT		0x9519
#define PA_DBG_MODE			0x9529
#define PA_DBG_TX_PHY_LATENCY		0x952C
#define PA_DBG_AUTOMODE_THLD		0x9536
#define PA_DBG_OV_TM			0x9540
#define PA_DBG_MIB_CAP_SEL		0x9546
#define PA_DBG_RESUME_HIBERN8		0x9550
#define PA_DBG_TX_PHY_LATENCY_EN	0x9552
#define PA_DBG_IGNORE_INCOMING		0x9559
#define PA_DBG_LINE_RESET_THLD		0x9561
#define PA_DBG_OPTION_SUITE		0x9564

/*
 * MIBs for Transport Layer debug registers
 */
#define T_DBG_SKIP_INIT_HIBERN8_EXIT	0xc001

/*
 * Exynos MPHY attributes
 */
#define TX_LINERESET_N_VAL		0x0277
 #define TX_LINERESET_N(v)	(((v) >> 10) & 0xff)
#define TX_LINERESET_P_VAL		0x027D
 #define TX_LINERESET_P(v)	(((v) >> 12) & 0xff)
#define TX_OV_SLEEP_CNT_TIMER		0x028E
 #define TX_OV_H8_ENTER_EN		(1 << 7)
 #define TX_OV_SLEEP_CNT(v)	(((v) >> 5) & 0x7f)

#define TX_HIGH_Z_CNT_11_08		0x028c
 #define TX_HIGH_Z_CNT_H(v)	(((v) >> 8) & 0xf)
#define TX_HIGH_Z_CNT_07_00		0x028d
 #define TX_HIGH_Z_CNT_L(v)	((v) & 0xff)
#define TX_BASE_NVAL_07_00		0x0293
 #define TX_BASE_NVAL_L(v)	((v) & 0xff)
#define TX_BASE_NVAL_15_08		0x0294
 #define TX_BASE_NVAL_H(v)	(((v) >> 8) & 0xff)
#define TX_GRAN_NVAL_07_00		0x0295
 #define TX_GRAN_NVAL_L(v)	((v) & 0xff)
#define TX_GRAN_NVAL_10_08		0x0296
 #define TX_GRAN_NVAL_H(v)	(((v) >> 8) & 0x3)

#define RX_FILLER_ENABLE		0x0316
 #define RX_FILLER_EN		(1 << 1)
#define RX_LCC_IGNORE			0x0318
#define RX_LINERESET_VAL		0x0317
 #define RX_LINERESET(v)	(((v) >> 12) & 0xff)
#define RX_SYNC_MASK_LENGTH		0x0321
#define RX_HIBERN8_WAIT_VAL_BIT_20_16	0x0331
#define RX_HIBERN8_WAIT_VAL_BIT_15_08	0x0332
#define RX_HIBERN8_WAIT_VAL_BIT_07_00	0x0333

#define RX_OV_SLEEP_CNT_TIMER		0x0340
 #define RX_OV_SLEEP_CNT(v)	(((v) >> 6) & 0x1f)
#define RX_OV_STALL_CNT_TIMER		0x0341
 #define RX_OV_STALL_CNT(v)	(((v) >> 4) & 0xff)
#define RX_BASE_NVAL_07_00		0x0355
 #define RX_BASE_NVAL_L(v)	((v) & 0xff)
#define RX_BASE_NVAL_15_08		0x0354
 #define RX_BASE_NVAL_H(v)	(((v) >> 8) & 0xff)
#define RX_GRAN_NVAL_07_00		0x0353
 #define RX_GRAN_NVAL_L(v)	((v) & 0xff)
#define RX_GRAN_NVAL_10_08		0x0352
 #define RX_GRAN_NVAL_H(v)	(((v) >> 8) & 0x3)

#define CMN_PWM_CMN_CTRL		0x0402
 #define PWM_CMN_CTRL_MASK	0x3
#define CMN_REFCLK_PLL_LOCK		0x0406
#define CMN_REFCLK_STREN		0x044C
#define CMN_REFCLK_OUT			0x044E
#define CMN_REFCLK_SEL_PLL		0x044F

/*
 * Driver specific definitions
 */

enum {
	PHY_CFG_NONE = 0,
	PHY_PCS_COMN,
	PHY_PCS_RXTX,
	PHY_PMA_COMN,
	PHY_PMA_TRSV,
	UNIPRO_STD_MIB,
	UNIPRO_DBG_MIB,
	UNIPRO_DBG_APB,
};

enum {
	__PMD_PWM_G1_L1,
	__PMD_PWM_G1_L2,
	__PMD_PWM_G2_L1,
	__PMD_PWM_G2_L2,
	__PMD_PWM_G3_L1,
	__PMD_PWM_G3_L2,
	__PMD_PWM_G4_L1,
	__PMD_PWM_G4_L2,
	__PMD_PWM_G5_L1,
	__PMD_PWM_G5_L2,
	__PMD_HS_G1_L1,
	__PMD_HS_G1_L2,
	__PMD_HS_G2_L1,
	__PMD_HS_G2_L2,
	__PMD_HS_G3_L1,
	__PMD_HS_G3_L2,
};

#define PMD_PWM_G1_L1	(1U << __PMD_PWM_G1_L1)
#define PMD_PWM_G1_L2	(1U << __PMD_PWM_G1_L2)
#define PMD_PWM_G2_L1	(1U << __PMD_PWM_G2_L1)
#define PMD_PWM_G2_L2	(1U << __PMD_PWM_G2_L2)
#define PMD_PWM_G3_L1	(1U << __PMD_PWM_G3_L1)
#define PMD_PWM_G3_L2	(1U << __PMD_PWM_G3_L2)
#define PMD_PWM_G4_L1	(1U << __PMD_PWM_G4_L1)
#define PMD_PWM_G4_L2	(1U << __PMD_PWM_G4_L2)
#define PMD_PWM_G5_L1	(1U << __PMD_PWM_G5_L1)
#define PMD_PWM_G5_L2	(1U << __PMD_PWM_G5_L2)
#define PMD_HS_G1_L1	(1U << __PMD_HS_G1_L1)
#define PMD_HS_G1_L2	(1U << __PMD_HS_G1_L2)
#define PMD_HS_G2_L1	(1U << __PMD_HS_G2_L1)
#define PMD_HS_G2_L2	(1U << __PMD_HS_G2_L2)
#define PMD_HS_G3_L1	(1U << __PMD_HS_G3_L1)
#define PMD_HS_G3_L2	(1U << __PMD_HS_G3_L2)

#define PMD_ALL		(PMD_HS_G3_L2 - 1)
#define PMD_PWM		(PMD_PWM_G4_L2 - 1)
#define PMD_HS		(PMD_ALL ^ PMD_PWM)

struct ufs_phy_cfg {
	u32 addr;
	u32 val;
	u32 flg;
	u32 lyr;
};

struct exynos_ufs_soc {
	struct ufs_phy_cfg *tbl_phy_init;
	struct ufs_phy_cfg *tbl_post_phy_init;
	struct ufs_phy_cfg *tbl_calib_of_pwm;
	struct ufs_phy_cfg *tbl_calib_of_hs_rate_a;
	struct ufs_phy_cfg *tbl_calib_of_hs_rate_b;
	struct ufs_phy_cfg *tbl_post_calib_of_pwm;
	struct ufs_phy_cfg *tbl_post_calib_of_hs_rate_a;
	struct ufs_phy_cfg *tbl_post_calib_of_hs_rate_b;
	struct ufs_phy_cfg *tbl_pma_restore;
};

struct exynos_ufs_phy {
	void __iomem *reg_pma;
	void __iomem *reg_pmu;
	struct exynos_ufs_soc *soc;
};

struct exynos_ufs_sys {
	void __iomem *reg_sys;
};

struct uic_pwr_mode {
	u8 lane;
	u8 gear;
	u8 mode;
	u8 hs_series;
	bool termn;
	u32 local_l2_timer[3];
	u32 remote_l2_timer[3];
};

struct exynos_ufs_tp_mon_table {
	u32	threshold;
	s32     cluster1_value;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	s32     cluster0_value;
#endif
	s32     mif_value;
};

struct exynos_ufs_clk_info {
	struct list_head list;
	struct clk *clk;
	const char *name;
	u32 freq;
};

struct exynos_ufs_misc_log {
	struct list_head clk_list_head;
	bool isolation;
};

struct exynos_ufs_sfr_log {
	const char* name;
	const u32 offset;
#define LOG_STD_HCI_SFR		0xFFFFFFF0
#define LOG_VS_HCI_SFR		0xFFFFFFF1
#define LOG_FMP_SFR		0xFFFFFFF2
#define LOG_UNIPRO_SFR		0xFFFFFFF3
#define LOG_PMA_SFR		0xFFFFFFF4
	u32 val;
};

struct exynos_ufs_attr_log {
	const u32 offset;
	u32 res;
	u32 val;
};

struct exynos_ufs_debug {
	struct exynos_ufs_sfr_log* sfr;
	struct exynos_ufs_attr_log* attr;
	struct exynos_ufs_misc_log misc;
};

struct exynos_ufs {
	struct device *dev;
	struct ufs_hba *hba;

	void __iomem *reg_hci;
	void __iomem *reg_unipro;
	void __iomem *reg_ufsp;

	struct clk *clk_hci;
	struct clk *pclk;
	struct clk *clk_unipro;
	struct clk *clk_refclk_1;
	struct clk *clk_refclk_2;
	struct clk *clk_phy_symb[4];
	u32 pclk_rate;
	u32 pclk_avail_min;
	u32 pclk_avail_max;
	u32 mclk_rate;
	u32 pwm_freq;

	int avail_ln_rx;
	int avail_ln_tx;

	struct exynos_ufs_phy phy;
	struct exynos_ufs_sys sys;
	struct notifier_block lpa_nb;
	struct uic_pwr_mode req_pmd_parm;
	struct uic_pwr_mode act_pmd_parm;

	u32 rx_adv_fine_gran_sup_en;
	u32 rx_adv_fine_gran_step;
	u32 rx_min_actv_time_cap;
	u32 rx_hibern8_time_cap;
	u32 tx_hibern8_time_cap;
	u32 rx_adv_min_actv_time_cap;
	u32 rx_adv_hibern8_time_cap;

	u32 pa_granularity;
	u32 pa_tactivate;
	u32 pa_hibern8time;
	u32 wait_cdr_lock;


	u32 opts;
#define EXYNOS_UFS_OPTS_SKIP_CONNECTION_ESTAB	BIT(0)
#define EXYNOS_UFS_OPTS_USE_SEPERATED_PCLK	BIT(1)	/* for CAL */
#define EXYNOS_UFS_OPTS_SET_LINE_INIT_PREP_LEN	BIT(2)

	/* Performance */
	struct exynos_ufs_tp_mon_table *tp_mon_tbl;
	struct delayed_work	tp_mon;
	struct pm_qos_request	pm_qos_cluster1;
	struct pm_qos_request	pm_qos_cluster0;
	struct pm_qos_request	pm_qos_mif;
	u32 last_threshold;
	struct pm_qos_request	pm_qos_int;
	s32			pm_qos_int_value;

	/* Support system power mode */
	int idle_ip_index;

	/* for miscellaneous control */
	u32 misc_flags;
#define EXYNOS_UFS_MISC_TOGGLE_LOG	BIT(0)
	struct exynos_ufs_debug debug;
};

struct phy_tm_parm {
	u32 tx_linereset_p;
	u32 tx_linereset_n;
	u32 tx_high_z_cnt;
	u32 tx_base_n_val;
	u32 tx_gran_n_val;
	u32 tx_sleep_cnt;

	u32 rx_linereset;
	u32 rx_hibern8_wait;
	u32 rx_base_n_val;
	u32 rx_gran_n_val;
	u32 rx_sleep_cnt;
	u32 rx_stall_cnt;
};
#endif /* _UFS_EXYNOS_H_ */
