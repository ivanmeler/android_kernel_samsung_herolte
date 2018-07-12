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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#if defined(CONFIG_FMP_UFS)
#include <linux/smc.h>
#endif

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-powermode.h>

#include "ufshcd.h"
#include "unipro.h"
#include "mphy.h"
#include "ufshcd-pltfrm.h"
#include "ufs-exynos.h"

/*
 * Unipro attribute value
 */
#define TXTRAILINGCLOCKS	0x10
#define TACTIVATE_10_USEC	400	/* unit: 10us */

/* Device ID */
#define DEV_ID	0x00
#define PEER_DEV_ID	0x01
#define PEER_CPORT_ID	0x00
#define TRAFFIC_CLASS	0x00

/*
 * Default M-PHY parameter
 */
#define TX_DIF_P_NSEC		3000000	/* unit: ns */
#define TX_DIF_N_NSEC		1000000	/* unit: ns */
#define RX_DIF_P_NSEC		1000000	/* unit: ns */
#define RX_HIBERN8_WAIT_NSEC	4000000	/* unit: ns */
#define HIBERN8_TIME		40	/* unit: 100us */
#define RX_BASE_UNIT_NSEC	100000	/* unit: ns */
#define RX_GRAN_UNIT_NSEC	4000	/* unit: ns */
#define RX_SLEEP_CNT		1280	/* unit: ns */
#define RX_STALL_CNT		320	/* unit: ns */

#define TX_HIGH_Z_CNT_NSEC	20000	/* unit: ns */
#define TX_BASE_UNIT_NSEC	100000	/* unit: ns */
#define TX_GRAN_UNIT_NSEC	4000	/* unit: ns */
#define TX_SLEEP_CNT		1000	/* unit: ns */
#define IATOVAL_NSEC		20000	/* unit: ns */

#define UNIPRO_PCLK_PERIOD(ufs) (NSEC_PER_SEC / ufs->pclk_rate)
#define UNIPRO_MCLK_PERIOD(ufs) (NSEC_PER_SEC / ufs->mclk_rate)
#define PHY_PMA_COMN_ADDR(reg)		((reg) << 2)
#define PHY_PMA_TRSV_ADDR(reg, lane)	(((reg) + (0x30 * (lane))) << 2)

/* Throughput Monitor Period */
#define TP_MON_PERIOD	HZ

/* PM QoS Expiration Time in TP mon */
#define TP_MON_PM_QOS_LIFETIME	2000000

#define phy_pma_writel(ufs, val, reg)	\
	writel((val), (ufs)->phy.reg_pma + (reg))
#define phy_pma_readl(ufs, reg)	\
	readl((ufs)->phy.reg_pma + (reg))

#define EXYNOS_UFS_MMIO_FUNC(name)						\
static inline void name##_writel(struct exynos_ufs *ufs, u32 val, u32 reg)	\
{										\
	writel(val, ufs->reg_##name + reg);					\
}										\
										\
static inline u32 name##_readl(struct exynos_ufs *ufs, u32 reg)			\
{										\
	return readl(ufs->reg_##name + reg);					\
}

EXYNOS_UFS_MMIO_FUNC(hci);
EXYNOS_UFS_MMIO_FUNC(unipro);
EXYNOS_UFS_MMIO_FUNC(ufsp);
#undef EXYNOS_UFS_MMIO_FUNC

#define for_each_ufs_lane(ufs, i) \
	for (i = 0; i < (ufs)->avail_ln_rx; i++)
#define for_each_phy_cfg(cfg) \
	for (; (cfg)->flg != PHY_CFG_NONE; (cfg)++)

static const struct of_device_id exynos_ufs_match[];

static inline const struct exynos_ufs_soc *to_phy_soc(struct exynos_ufs *ufs)
{
	return ufs->phy.soc;
}

static inline struct exynos_ufs *to_exynos_ufs(struct ufs_hba *hba)
{
	return dev_get_platdata(hba->dev);
}

static inline void exynos_ufs_ctrl_phy_pwr(struct exynos_ufs *ufs, bool en)
{
	/* TODO: offset, mask */
	writel(!!en, ufs->phy.reg_pmu);
}

static struct exynos_ufs *ufs_host_backup[1];
static int ufs_host_index = 0;
static int dump_once_again = 1;
static int dump_sfr[292];
static int dump_attr[188];

static struct exynos_ufs_sfr_log ufs_cfg_log_sfr[] = {
	{"STD HCI SFR"			,	LOG_STD_HCI_SFR,		0},

	{"CAPABILITIES"			,	REG_CONTROLLER_CAPABILITIES,	0},
	{"UFS VERSION"			,	REG_UFS_VERSION,		0},
	{"PRODUCT ID"			,	REG_CONTROLLER_DEV_ID,		0},
	{"MANUFACTURE ID"		,	REG_CONTROLLER_PROD_ID,		0},
	{"INTERRUPT STATUS"		,	REG_INTERRUPT_STATUS,		0},
	{"INTERRUPT ENABLE"		,	REG_INTERRUPT_ENABLE,		0},
	{"CONTROLLER STATUS"		,	REG_CONTROLLER_STATUS,		0},
	{"CONTROLLER ENABLE"		,	REG_CONTROLLER_ENABLE,		0},
	{"UIC ERR PHY ADAPTER LAYER"	,	REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER,		0},
	{"UIC ERR DATA LINK LAYER"	,	REG_UIC_ERROR_CODE_DATA_LINK_LAYER,		0},
	{"UIC ERR NETWORK LATER"	,	REG_UIC_ERROR_CODE_NETWORK_LAYER,		0},
	{"UIC ERR TRANSPORT LAYER"	,	REG_UIC_ERROR_CODE_TRANSPORT_LAYER,		0},
	{"UIC ERR DME"			,	REG_UIC_ERROR_CODE_DME,		0},
	{"UTP TRANSF REQ INT AGG CNTRL"	,	REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL,		0},
	{"UTP TRANSF REQ LIST BASE L"	,	REG_UTP_TRANSFER_REQ_LIST_BASE_L,		0},
	{"UTP TRANSF REQ LIST BASE H"	,	REG_UTP_TRANSFER_REQ_LIST_BASE_H,		0},
	{"UTP TRANSF REQ DOOR BELL"	,	REG_UTP_TRANSFER_REQ_DOOR_BELL,		0},
	{"UTP TRANSF REQ LIST CLEAR"	,	REG_UTP_TRANSFER_REQ_LIST_CLEAR,		0},
	{"UTP TRANSF REQ LIST RUN STOP"	,	REG_UTP_TRANSFER_REQ_LIST_RUN_STOP,		0},
	{"UTP TASK REQ LIST BASE L"	,	REG_UTP_TASK_REQ_LIST_BASE_L,		0},
	{"UTP TASK REQ LIST BASE H"	,	REG_UTP_TASK_REQ_LIST_BASE_H,		0},
	{"UTP TASK REQ DOOR BELL"	,	REG_UTP_TASK_REQ_DOOR_BELL,		0},
	{"UTP TASK REQ LIST CLEAR"	,	REG_UTP_TASK_REQ_LIST_CLEAR,		0},
	{"UTP TASK REQ LIST RUN STOP"	,	REG_UTP_TASK_REQ_LIST_RUN_STOP,		0},
	{"UIC COMMAND"			,	REG_UIC_COMMAND,		0},
	{"UIC COMMAND ARG1"		,	REG_UIC_COMMAND_ARG_1,		0},
	{"UIC COMMAND ARG2"		,	REG_UIC_COMMAND_ARG_2,		0},
	{"UIC COMMAND ARG3"		,	REG_UIC_COMMAND_ARG_3,		0},

	{"VS HCI SFR"			,	LOG_VS_HCI_SFR,			0},

	{"TXPRDT ENTRY SIZE"		,	HCI_TXPRDT_ENTRY_SIZE,		0},
	{"RXPRDT ENTRY SIZE"		,	HCI_RXPRDT_ENTRY_SIZE,		0},
	{"TO CNT DIV VAL"		,	HCI_TO_CNT_DIV_VAL,		0},
	{"1US TO CNT VAL"		,	HCI_1US_TO_CNT_VAL,		0},
	{"INVALID UPIU CTRL"		,	HCI_INVALID_UPIU_CTRL,		0},
	{"INVALID UPIU BADDR"		,	HCI_INVALID_UPIU_BADDR,		0},
	{"INVALID UPIU UBADDR"		,	HCI_INVALID_UPIU_UBADDR,		0},
	{"INVALID UTMR OFFSET ADDR"	,	HCI_INVALID_UTMR_OFFSET_ADDR,		0},
	{"INVALID UTR OFFSET ADDR"	,	HCI_INVALID_UTR_OFFSET_ADDR,		0},
	{"INVALID DIN OFFSET ADDR"	,	HCI_INVALID_DIN_OFFSET_ADDR,		0},
	{"DBR TIMER CONFIG"		,	HCI_DBR_TIMER_CONFIG,		0},
	{"DBR TIMER STATUS"		,	HCI_DBR_TIMER_STATUS,		0},
	{"VENDOR SPECIFIC IS"		,	HCI_VENDOR_SPECIFIC_IS,		0},
	{"VENDOR SPECIFIC IE"		,	HCI_VENDOR_SPECIFIC_IE,		0},
	{"UTRL NEXUS TYPE"		,	HCI_UTRL_NEXUS_TYPE,		0},
	{"UTMRL NEXUS TYPE"		,	HCI_UTMRL_NEXUS_TYPE,		0},
	{"E2EFC CTRL"			,	HCI_E2EFC_CTRL,		0},
	{"SW RST"			,	HCI_SW_RST,		0},
	{"LINK VERSION"			,	HCI_LINK_VERSION,		0},
	{"IDLE TIMER CONFIG"		,	HCI_IDLE_TIMER_CONFIG,		0},
	{"RX UPIU MATCH ERROR CODE"	,	HCI_RX_UPIU_MATCH_ERROR_CODE,		0},
	{"DATA REORDER"			,	HCI_DATA_REORDER,		0},
	{"MAX DOUT DATA SIZE"		,	HCI_MAX_DOUT_DATA_SIZE,		0},
	{"UNIPRO APB CLK CTRL"		,	HCI_UNIPRO_APB_CLK_CTRL,		0},
	{"AXIDMA RWDATA BURST LEN"	,	HCI_AXIDMA_RWDATA_BURST_LEN,		0},
	{"GPIO OUT"			,	HCI_GPIO_OUT,		0},
	{"WRITE DMA CTRL"		,	HCI_WRITE_DMA_CTRL,		0},
	{"ERROR EN PA LAYER"		,	HCI_ERROR_EN_PA_LAYER,		0},
	{"ERROR EN DL LAYER"		,	HCI_ERROR_EN_DL_LAYER,		0},
	{"ERROR EN N LAYER"		,	HCI_ERROR_EN_N_LAYER,		0},
	{"ERROR EN T LAYER"		,	HCI_ERROR_EN_T_LAYER,		0},
	{"ERROR EN DME LAYER"		,	HCI_ERROR_EN_DME_LAYER,		0},
	{"REQ HOLD EN"			,	HCI_REQ_HOLD_EN,		0},
	{"CLKSTOP CTRL"			,	HCI_CLKSTOP_CTRL,		0},
	{"FORCE HCS"			,	HCI_FORCE_HCS,		0},
	{"FSM MONITOR"			,	HCI_FSM_MONITOR,		0},
	{"PRDT HIT RATIO"		,	HCI_PRDT_HIT_RATIO,		0},
	{"DMA0 MONITOR STATE"		,	HCI_DMA0_MONITOR_STATE,		0},
	{"DMA0 MONITOR CNT"		,	HCI_DMA0_MONITOR_CNT,		0},
	{"DMA1 MONITOR STATE"		,	HCI_DMA1_MONITOR_STATE,		0},
	{"DMA1 MONITOR CNT"		,	HCI_DMA1_MONITOR_CNT,		0},

	{"FMP SFR"			,	LOG_FMP_SFR,			0},

	{"UFSPRCTRL"			,	UFSPRCTRL,			0},
	{"UFSPRSTAT"			,	UFSPRSTAT,			0},
	{"UFSPRSECURITY"		,	UFSPRSECURITY,			0},
	{"UFSPVERSION"			,	UFSPVERSION,			0},
	{"UFSPWCTRL"			,	UFSPWCTRL,			0},
	{"UFSPWSTAT"			,	UFSPWSTAT,			0},
	{"UFSPSBEGIN0"			,	UFSPSBEGIN0,			0},
	{"UFSPSEND0"			,	UFSPSEND0,			0},
	{"UFSPSLUN0"			,	UFSPSLUN0,			0},
	{"UFSPSCTRL0"			,	UFSPSCTRL0,			0},
	{"UFSPSBEGIN1"			,	UFSPSBEGIN1,			0},
	{"UFSPSEND1"			,	UFSPSEND1,			0},
	{"UFSPSLUN1"			,	UFSPSLUN1,			0},
	{"UFSPSCTRL1"			,	UFSPSCTRL1,			0},
	{"UFSPSBEGIN2"			,	UFSPSBEGIN2,			0},
	{"UFSPSEND2"			,	UFSPSEND2,			0},
	{"UFSPSLUN2"			,	UFSPSLUN2,			0},
	{"UFSPSCTRL2"			,	UFSPSCTRL2,			0},
	{"UFSPSBEGIN3"			,	UFSPSBEGIN3,			0},
	{"UFSPSEND3"			,	UFSPSEND3,			0},
	{"UFSPSLUN3"			,	UFSPSLUN3,			0},
	{"UFSPSCTRL3"			,	UFSPSCTRL3,			0},
	{"UFSPSBEGIN4"			,	UFSPSBEGIN4,			0},
	{"UFSPSLUN4"			,	UFSPSLUN4,			0},
	{"UFSPSCTRL4"			,	UFSPSCTRL4,			0},
	{"UFSPSBEGIN5"			,	UFSPSBEGIN5,			0},
	{"UFSPSEND5"			,	UFSPSEND5,			0},
	{"UFSPSLUN5"			,	UFSPSLUN5,			0},
	{"UFSPSCTRL5"			,	UFSPSCTRL5,			0},
	{"UFSPSBEGIN6"			,	UFSPSBEGIN6,			0},
	{"UFSPSEND6"			,	UFSPSEND6,			0},
	{"UFSPSLUN6"			,	UFSPSLUN6,			0},
	{"UFSPSCTRL6"			,	UFSPSCTRL6,			0},
	{"UFSPSBEGIN7"			,	UFSPSBEGIN7,			0},
	{"UFSPSEND7"			,	UFSPSEND7,			0},
	{"UFSPSLUN7"			,	UFSPSLUN7,			0},
	{"UFSPSCTRL7"			,	UFSPSCTRL7,			0},

	{"UNIPRO SFR"			,	LOG_UNIPRO_SFR,			0},

	{"COMP_VERSION"			,	UNIP_COMP_VERSION			,	0},
	{"COMP_INFO"			,	UNIP_COMP_INFO				,	0},
	{"COMP_RESET"			,	UNIP_COMP_RESET				,	0},
	{"DME_POWERON_REQ"		,	UNIP_DME_POWERON_REQ			,	0},
	{"DME_POWERON_CNF_RESULT"	,	UNIP_DME_POWERON_CNF_RESULT		,	0},
	{"DME_POWEROFF_REQ"		,	UNIP_DME_POWEROFF_REQ			,	0},
	{"DME_POWEROFF_CNF_RESULT"	,	UNIP_DME_POWEROFF_CNF_RESULT		,	0},
	{"DME_RESET_REQ"		,	UNIP_DME_RESET_REQ			,	0},
	{"DME_RESET_REQ_LEVEL"		,	UNIP_DME_RESET_REQ_LEVEL		,	0},
	{"DME_ENABLE_REQ"		,	UNIP_DME_ENABLE_REQ			,	0},
	{"DME_ENABLE_CNF_RESULT"	,	UNIP_DME_ENABLE_CNF_RESULT		,	0},
	{"DME_ENDPOINTRESET_REQ"	,	UNIP_DME_ENDPOINTRESET_REQ		,	0},
	{"DME_ENDPOINTRESET_CNF_RESULT"	,	UNIP_DME_ENDPOINTRESET_CNF_RESULT	,	0},
	{"DME_LINKSTARTUP_REQ"		,	UNIP_DME_LINKSTARTUP_REQ		,	0},
	{"DME_LINKSTARTUP_CNF_RESULT"	,	UNIP_DME_LINKSTARTUP_CNF_RESULT		,	0},
	{"DME_HIBERN8_ENTER_REQ"	,	UNIP_DME_HIBERN8_ENTER_REQ		,	0},
	{"DME_HIBERN8_ENTER_CNF_RESULT"	,	UNIP_DME_HIBERN8_ENTER_CNF_RESULT	,	0},
	{"DME_HIBERN8_ENTER_IND_RESULT"	,	UNIP_DME_HIBERN8_ENTER_IND_RESULT	,	0},
	{"DME_HIBERN8_EXIT_REQ"		,	UNIP_DME_HIBERN8_EXIT_REQ		,	0},
	{"DME_HIBERN8_EXIT_CNF_RESULT"	,	UNIP_DME_HIBERN8_EXIT_CNF_RESULT	,	0},
	{"DME_HIBERN8_EXIT_IND_RESULT"	,	UNIP_DME_HIBERN8_EXIT_IND_RESULT	,	0},
	{"DME_PWR_REQ"			,	UNIP_DME_PWR_REQ			,	0},
	{"DME_PWR_REQ_POWERMODE	"	,	UNIP_DME_PWR_REQ_POWERMODE		,	0},
	{"DME_PWR_REQ_LOCALL2TIMER0"	,	UNIP_DME_PWR_REQ_LOCALL2TIMER0		,	0},
	{"DME_PWR_REQ_LOCALL2TIMER1"	,	UNIP_DME_PWR_REQ_LOCALL2TIMER1		,	0},
	{"DME_PWR_REQ_LOCALL2TIMER2"	,	UNIP_DME_PWR_REQ_LOCALL2TIMER2		,	0},
	{"DME_PWR_REQ_REMOTEL2TIMER0"	,	UNIP_DME_PWR_REQ_REMOTEL2TIMER0		,	0},
	{"DME_PWR_REQ_REMOTEL2TIMER1"	,	UNIP_DME_PWR_REQ_REMOTEL2TIMER1		,	0},
	{"DME_PWR_REQ_REMOTEL2TIMER2"	,	UNIP_DME_PWR_REQ_REMOTEL2TIMER2		,	0},
	{"DME_PWR_CNF_RESULT"		,	UNIP_DME_PWR_CNF_RESULT			,	0},
	{"DME_PWR_IND_RESULT"		,	UNIP_DME_PWR_IND_RESULT			,	0},
	{"DME_TEST_MODE_REQ"		,	UNIP_DME_TEST_MODE_REQ			,	0},
	{"DME_TEST_MODE_CNF_RESULT"	,	UNIP_DME_TEST_MODE_CNF_RESULT		,	0},
	{"DME_ERROR_IND_LAYER"		,	UNIP_DME_ERROR_IND_LAYER		,	0},
	{"DME_ERROR_IND_ERRCODE"	,	UNIP_DME_ERROR_IND_ERRCODE		,	0},
	{"DME_PACP_CNFBIT"		,	UNIP_DME_PACP_CNFBIT			,	0},
	{"DME_DL_FRAME_IND"		,	UNIP_DME_DL_FRAME_IND			,	0},
	{"DME_INTR_STATUS"		,	UNIP_DME_INTR_STATUS			,	0},
	{"DME_INTR_ENABLE"		,	UNIP_DME_INTR_ENABLE			,	0},
	{"DME_GETSET_ADDR"		,	UNIP_DME_GETSET_ADDR			,	0},
	{"DME_GETSET_WDATA"		,	UNIP_DME_GETSET_WDATA			,	0},
	{"DME_GETSET_RDATA"		,	UNIP_DME_GETSET_RDATA			,	0},
	{"DME_GETSET_CONTROL"		,	UNIP_DME_GETSET_CONTROL			,	0},
	{"DME_GETSET_RESULT"		,	UNIP_DME_GETSET_RESULT			,	0},
	{"DME_PEER_GETSET_ADDR"		,	UNIP_DME_PEER_GETSET_ADDR		,	0},
	{"DME_PEER_GETSET_WDATA"	,	UNIP_DME_PEER_GETSET_WDATA		,	0},
	{"DME_PEER_GETSET_RDATA"	,	UNIP_DME_PEER_GETSET_RDATA		,	0},
	{"DME_PEER_GETSET_CONTROL"	,	UNIP_DME_PEER_GETSET_CONTROL		,	0},
	{"DME_PEER_GETSET_RESULT"	,	UNIP_DME_PEER_GETSET_RESULT		,	0},
	{"DME_DIRECT_GETSET_BASE"	,	UNIP_DME_DIRECT_GETSET_BASE		,	0},
	{"DME_DIRECT_GETSET_ERR_ADDR"	,	UNIP_DME_DIRECT_GETSET_ERR_ADDR		,	0},
	{"DME_DIRECT_GETSET_ERR_CODE"	,	UNIP_DME_DIRECT_GETSET_ERR_CODE		,	0},
	{"DME_INTR_ERROR_CODE"		,	UNIP_DME_INTR_ERROR_CODE		,	0},
	{"DME_DEEPSTALL_ENTER_REQ"	,	UNIP_DME_DEEPSTALL_ENTER_REQ		,	0},
	{"DME_DISCARD_CPORT_ID"		,	UNIP_DME_DISCARD_CPORT_ID		,	0},
	{"DBG_DME_CTRL_STATE"		,	UNIP_DBG_DME_CTRL_STATE			,	0},
	{"DBG_FORCE_DME_CTRL_STATE"	,	UNIP_DBG_FORCE_DME_CTRL_STATE		,	0},
	{"DBG_AUTO_DME_LINKSTARTUP"	,	UNIP_DBG_AUTO_DME_LINKSTARTUP,			0},
	{"DBG_PA_CTRLSTATE"		,	UNIP_DBG_PA_CTRLSTATE			,	0},
	{"DBG_PA_TX_STATE"		,	UNIP_DBG_PA_TX_STATE			,	0},
	{"DBG_BREAK_DME_CTRL_STATE"	,	UNIP_DBG_BREAK_DME_CTRL_STATE		,	0},
	{"DBG_STEP_DME_CTRL_STATE"	,	UNIP_DBG_STEP_DME_CTRL_STATE		,	0},
	{"DBG_NEXT_DME_CTRL_STATE"	,	UNIP_DBG_NEXT_DME_CTRL_STATE		,	0},

	{"PMA SFR"			,	LOG_PMA_SFR,			0},

	{"COMN 0x00"			,	(0x00<<2),			0},
	{"COMN 0x01"			,	(0x01<<2),			0},
	{"COMN 0x02"			,	(0x02<<2),			0},
	{"COMN 0x03"			,	(0x03<<2),			0},
	{"COMN 0x04"			,	(0x04<<2),			0},
	{"COMN 0x05"			,	(0x05<<2),			0},
	{"COMN 0x06"			,	(0x06<<2),			0},
	{"COMN 0x07"			,	(0x07<<2),			0},
	{"COMN 0x08"			,	(0x08<<2),			0},
	{"COMN 0x0a"			,	(0x0a<<2),			0},
	{"COMN 0x0b"			,	(0x0b<<2),			0},
	{"COMN 0x0c"			,	(0x0c<<2),			0},
	{"COMN 0x0d"			,	(0x0d<<2),			0},
	{"COMN 0x0e"			,	(0x0e<<2),			0},
	{"COMN 0x0f"			,	(0x0f<<2),			0},
	{"COMN 0x10"			,	(0x10<<2),			0},
	{"COMN 0x11"			,	(0x11<<2),			0},
	{"COMN 0x12"			,	(0x12<<2),			0},
	{"COMN 0x13"			,	(0x13<<2),			0},
	{"COMN 0x14"			,	(0x14<<2),			0},
	{"COMN 0x15"			,	(0x15<<2),			0},
	{"COMN 0x16"			,	(0x16<<2),			0},
	{"COMN 0x17"			,	(0x17<<2),			0},
	{"COMN 0x18"			,	(0x18<<2),			0},
	{"COMN 0x19"			,	(0x19<<2),			0},
	{"COMN 0x1a"			,	(0x1a<<2),			0},
	{"COMN 0x1b"			,	(0x1b<<2),			0},
	{"COMN 0x1c"			,	(0x1c<<2),			0},
	{"COMN 0x1d"			,	(0x1d<<2),			0},
	{"COMN 0x1e"			,	(0x1e<<2),			0},
	{"COMN 0x1f"			,	(0x1f<<2),			0},
	{"COMN 0x20"			,	(0x20<<2),			0},
	{"COMN 0x21"			,	(0x21<<2),			0},
	{"COMN 0x22"			,	(0x22<<2),			0},
	{"COMN 0x23"			,	(0x23<<2),			0},
	{"COMN 0x24"			,	(0x24<<2),			0},
	{"COMN 0x25"			,	(0x25<<2),			0},
	{"COMN 0x26"			,	(0x26<<2),			0},
	{"COMN 0x27"			,	(0x27<<2),			0},
	{"COMN 0x28"			,	(0x28<<2),			0},
	{"COMN 0x29"			,	(0x29<<2),			0},
	{"COMN 0x2a"			,	(0x2a<<2),			0},
	{"COMN 0x2b"			,	(0x2b<<2),			0},
	{"COMN 0x2c"			,	(0x2c<<2),			0},
	{"COMN 0x2d"			,	(0x2d<<2),			0},
	{"COMN 0x2e"			,	(0x2e<<2),			0},
	{"COMN 0x2f"			,	(0x2f<<2),			0},
	{"COMN 0x30"			,	(0x30<<2),			0},
	{"TRSV 0x31"			,	(0x31<<2),			0},
	{"TRSV 0x32"			,	(0x32<<2),			0},
	{"TRSV 0x33"			,	(0x33<<2),			0},
	{"TRSV 0x34"			,	(0x34<<2),			0},
	{"TRSV 0x35"			,	(0x35<<2),			0},
	{"TRSV 0x36"			,	(0x36<<2),			0},
	{"TRSV 0x37"			,	(0x37<<2),			0},
	{"TRSV 0x38"			,	(0x38<<2),			0},
	{"TRSV 0x3a"			,	(0x3a<<2),			0},
	{"TRSV 0x3b"			,	(0x3b<<2),			0},
	{"TRSV 0x3c"			,	(0x3c<<2),			0},
	{"TRSV 0x3d"			,	(0x3d<<2),			0},
	{"TRSV 0x3e"			,	(0x3e<<2),			0},
	{"TRSV 0x3f"			,	(0x3f<<2),			0},
	{"TRSV 0x40"			,	(0x40<<2),			0},
	{"TRSV 0x41"			,	(0x41<<2),			0},
	{"TRSV 0x42"			,	(0x42<<2),			0},
	{"TRSV 0x43"			,	(0x43<<2),			0},
	{"TRSV 0x44"			,	(0x44<<2),			0},
	{"TRSV 0x45"			,	(0x45<<2),			0},
	{"TRSV 0x46"			,	(0x46<<2),			0},
	{"TRSV 0x47"			,	(0x47<<2),			0},
	{"TRSV 0x48"			,	(0x48<<2),			0},
	{"TRSV 0x49"			,	(0x49<<2),			0},
	{"TRSV 0x4a"			,	(0x4a<<2),			0},
	{"TRSV 0x4b"			,	(0x4b<<2),			0},
	{"TRSV 0x4c"			,	(0x4c<<2),			0},
	{"TRSV 0x4d"			,	(0x4d<<2),			0},
	{"TRSV 0x4e"			,	(0x4e<<2),			0},
	{"TRSV 0x4f"			,	(0x4f<<2),			0},
	{"TRSV 0x50"			,	(0x50<<2),			0},
	{"TRSV 0x51"			,	(0x51<<2),			0},
	{"TRSV 0x52"			,	(0x52<<2),			0},
	{"TRSV 0x53"			,	(0x53<<2),			0},
	{"TRSV 0x54"			,	(0x54<<2),			0},
	{"TRSV 0x55"			,	(0x55<<2),			0},
	{"TRSV 0x56"			,	(0x56<<2),			0},
	{"TRSV 0x57"			,	(0x57<<2),			0},
	{"TRSV 0x58"			,	(0x58<<2),			0},
	{"TRSV 0x59"			,	(0x59<<2),			0},
	{"TRSV 0x5a"			,	(0x5a<<2),			0},
	{"TRSV 0x5b"			,	(0x5b<<2),			0},
	{"TRSV 0x5c"			,	(0x5c<<2),			0},
	{"TRSV 0x5d"			,	(0x5d<<2),			0},
	{"TRSV 0x5e"			,	(0x5e<<2),			0},
	{"TRSV 0x5f"			,	(0x5f<<2),			0},
	{"TRSV 0x60"			,	(0x60<<2),			0},
	{"TRSV 0x61"			,	(0x61<<2),			0},
	{"TRSV 0x62"			,	(0x62<<2),			0},
	{"TRSV 0x63"			,	(0x63<<2),			0},
	{"TRSV 0x64"			,	(0x64<<2),			0},
	{"TRSV 0x65"			,	(0x65<<2),			0},
	{"TRSV 0x66"			,	(0x66<<2),			0},
	{"TRSV 0x67"			,	(0x67<<2),			0},
	{"TRSV 0x68"			,	(0x68<<2),			0},
	{"TRSV 0x6a"			,	(0x6a<<2),			0},
	{"TRSV 0x6b"			,	(0x6b<<2),			0},
	{"TRSV 0x6c"			,	(0x6c<<2),			0},
	{"TRSV 0x6d"			,	(0x6d<<2),			0},
	{"TRSV 0x6e"			,	(0x6e<<2),			0},
	{"TRSV 0x6f"			,	(0x6f<<2),			0},
	{"TRSV 0x70"			,	(0x70<<2),			0},
	{"TRSV 0x71"			,	(0x71<<2),			0},
	{"TRSV 0x72"			,	(0x72<<2),			0},
	{"TRSV 0x73"			,	(0x73<<2),			0},
	{"TRSV 0x74"			,	(0x74<<2),			0},
	{"TRSV 0x75"			,	(0x75<<2),			0},
	{"TRSV 0x76"			,	(0x76<<2),			0},
	{"TRSV 0x77"			,	(0x77<<2),			0},
	{"TRSV 0x78"			,	(0x78<<2),			0},
	{"TRSV 0x79"			,	(0x79<<2),			0},
	{"TRSV 0x7a"			,	(0x7a<<2),			0},
	{"TRSV 0x7b"			,	(0x7b<<2),			0},
	{"TRSV 0x7c"			,	(0x7c<<2),			0},
	{"TRSV 0x7d"			,	(0x7d<<2),			0},
	{"TRSV 0x7e"			,	(0x7e<<2),			0},
	{"TRSV 0x7f"			,	(0x7f<<2),			0},

	{},
};

static struct exynos_ufs_attr_log ufs_cfg_log_attr[] = {
	/* PA Standard */
	{UIC_ARG_MIB(0x1520),	0, 0},
	{UIC_ARG_MIB(0x1540),	0, 0},
	{UIC_ARG_MIB(0x1543),	0, 0},
	{UIC_ARG_MIB(0x155C),	0, 0},
	{UIC_ARG_MIB(0x155D),	0, 0},
	{UIC_ARG_MIB(0x155E),	0, 0},
	{UIC_ARG_MIB(0x155F),	0, 0},
	{UIC_ARG_MIB(0x1560),	0, 0},
	{UIC_ARG_MIB(0x1561),	0, 0},
	{UIC_ARG_MIB(0x1564),	0, 0},
	{UIC_ARG_MIB(0x1567),	0, 0},
	{UIC_ARG_MIB(0x1568),	0, 0},
	{UIC_ARG_MIB(0x1569),	0, 0},
	{UIC_ARG_MIB(0x156A),	0, 0},
	{UIC_ARG_MIB(0x1571),	0, 0},
	{UIC_ARG_MIB(0x1580),	0, 0},
	{UIC_ARG_MIB(0x1581),	0, 0},
	{UIC_ARG_MIB(0x1582),	0, 0},
	{UIC_ARG_MIB(0x1583),	0, 0},
	{UIC_ARG_MIB(0x1584),	0, 0},
	{UIC_ARG_MIB(0x1585),	0, 0},
	{UIC_ARG_MIB(0x1590),	0, 0},
	{UIC_ARG_MIB(0x1591),	0, 0},
	{UIC_ARG_MIB(0x15A1),	0, 0},
	{UIC_ARG_MIB(0x15A2),	0, 0},
	{UIC_ARG_MIB(0x15A3),	0, 0},
	{UIC_ARG_MIB(0x15A4),	0, 0},
	{UIC_ARG_MIB(0x15A7),	0, 0},
	{UIC_ARG_MIB(0x15A8),	0, 0},
	{UIC_ARG_MIB(0x15C0),	0, 0},
	{UIC_ARG_MIB(0x15C1),	0, 0},
	/* PA Debug */
	{UIC_ARG_MIB(0x9500),	0, 0},
	{UIC_ARG_MIB(0x9501),	0, 0},
	{UIC_ARG_MIB(0x9502),	0, 0},
	{UIC_ARG_MIB(0x9503),	0, 0},
	{UIC_ARG_MIB(0x9510),	0, 0},
	{UIC_ARG_MIB(0x9511),	0, 0},
	{UIC_ARG_MIB(0x9514),	0, 0},
	{UIC_ARG_MIB(0x9515),	0, 0},
	{UIC_ARG_MIB(0x9516),	0, 0},
	{UIC_ARG_MIB(0x9517),	0, 0},
	{UIC_ARG_MIB(0x9520),	0, 0},
	{UIC_ARG_MIB(0x9521),	0, 0},
	{UIC_ARG_MIB(0x9522),	0, 0},
	{UIC_ARG_MIB(0x9523),	0, 0},
	{UIC_ARG_MIB(0x9525),	0, 0},
	{UIC_ARG_MIB(0x9528),	0, 0},
	{UIC_ARG_MIB(0x9529),	0, 0},
	{UIC_ARG_MIB(0x952A),	0, 0},
	{UIC_ARG_MIB(0x952B),	0, 0},
	{UIC_ARG_MIB(0x952C),	0, 0},
	{UIC_ARG_MIB(0x9534),	0, 0},
	{UIC_ARG_MIB(0x9535),	0, 0},
	{UIC_ARG_MIB(0x9536),	0, 0},
	{UIC_ARG_MIB(0x9539),	0, 0},
	{UIC_ARG_MIB(0x9540),	0, 0},
	{UIC_ARG_MIB(0x9541),	0, 0},
	{UIC_ARG_MIB(0x9542),	0, 0},
	{UIC_ARG_MIB(0x9543),	0, 0},
	{UIC_ARG_MIB(0x9546),	0, 0},
	{UIC_ARG_MIB(0x9551),	0, 0},
	{UIC_ARG_MIB(0x9552),	0, 0},
	{UIC_ARG_MIB(0x9554),	0, 0},
	{UIC_ARG_MIB(0x9556),	0, 0},
	{UIC_ARG_MIB(0x9557),	0, 0},
	{UIC_ARG_MIB(0x9558),	0, 0},
	{UIC_ARG_MIB(0x9559),	0, 0},
	{UIC_ARG_MIB(0x9560),	0, 0},
	{UIC_ARG_MIB(0x9561),	0, 0},
	{UIC_ARG_MIB(0x9562),	0, 0},
	{UIC_ARG_MIB(0x9563),	0, 0},
	{UIC_ARG_MIB(0x9564),	0, 0},
	{UIC_ARG_MIB(0x9565),	0, 0},
	{UIC_ARG_MIB(0x9566),	0, 0},
	{UIC_ARG_MIB(0x9567),	0, 0},
	{UIC_ARG_MIB(0x9568),	0, 0},
	{UIC_ARG_MIB(0x9569),	0, 0},
	{UIC_ARG_MIB(0x9570),	0, 0},
	{UIC_ARG_MIB(0x9571),	0, 0},
	{UIC_ARG_MIB(0x9572),	0, 0},
	{UIC_ARG_MIB(0x9573),	0, 0},
	/* DL Standard */
	{UIC_ARG_MIB(0x2002),	0, 0},
	{UIC_ARG_MIB(0x2003),	0, 0},
	{UIC_ARG_MIB(0x2004),	0, 0},
	{UIC_ARG_MIB(0x2005),	0, 0},
	{UIC_ARG_MIB(0x2006),	0, 0},
	{UIC_ARG_MIB(0x2040),	0, 0},
	{UIC_ARG_MIB(0x2041),	0, 0},
	{UIC_ARG_MIB(0x2042),	0, 0},
	{UIC_ARG_MIB(0x2043),	0, 0},
	{UIC_ARG_MIB(0x2044),	0, 0},
	{UIC_ARG_MIB(0x2045),	0, 0},
	{UIC_ARG_MIB(0x2046),	0, 0},
	{UIC_ARG_MIB(0x2047),	0, 0},
	{UIC_ARG_MIB(0x2060),	0, 0},
	{UIC_ARG_MIB(0x2061),	0, 0},
	{UIC_ARG_MIB(0x2062),	0, 0},
	{UIC_ARG_MIB(0x2063),	0, 0},
	{UIC_ARG_MIB(0x2064),	0, 0},
	{UIC_ARG_MIB(0x2065),	0, 0},
	{UIC_ARG_MIB(0x2066),	0, 0},
	{UIC_ARG_MIB(0x2067),	0, 0},
	/* DL Debug */
	{UIC_ARG_MIB(0xA000),	0, 0},
	{UIC_ARG_MIB(0xA003),	0, 0},
	{UIC_ARG_MIB(0xA004),	0, 0},
	{UIC_ARG_MIB(0xA005),	0, 0},
	{UIC_ARG_MIB(0xA006),	0, 0},
	{UIC_ARG_MIB(0xA007),	0, 0},
	{UIC_ARG_MIB(0xA009),	0, 0},
	{UIC_ARG_MIB(0xA010),	0, 0},
	{UIC_ARG_MIB(0xA011),	0, 0},
	{UIC_ARG_MIB(0xA012),	0, 0},
	{UIC_ARG_MIB(0xA013),	0, 0},
	{UIC_ARG_MIB(0xA014),	0, 0},
	{UIC_ARG_MIB(0xA015),	0, 0},
	{UIC_ARG_MIB(0xA016),	0, 0},
	{UIC_ARG_MIB(0xA020),	0, 0},
	{UIC_ARG_MIB(0xA021),	0, 0},
	{UIC_ARG_MIB(0xA022),	0, 0},
	{UIC_ARG_MIB(0xA023),	0, 0},
	{UIC_ARG_MIB(0xA024),	0, 0},
	{UIC_ARG_MIB(0xA025),	0, 0},
	{UIC_ARG_MIB(0xA026),	0, 0},
	{UIC_ARG_MIB(0xA027),	0, 0},
	{UIC_ARG_MIB(0xA028),	0, 0},
	{UIC_ARG_MIB(0xA029),	0, 0},
	{UIC_ARG_MIB(0xA02A),	0, 0},
	{UIC_ARG_MIB(0xA02B),	0, 0},
	{UIC_ARG_MIB(0xA02C),	0, 0},
	{UIC_ARG_MIB(0xA02D),	0, 0},
	{UIC_ARG_MIB(0xA02E),	0, 0},
	{UIC_ARG_MIB(0xA02F),	0, 0},
	{UIC_ARG_MIB(0xA030),	0, 0},
	{UIC_ARG_MIB(0xA031),	0, 0},
	{UIC_ARG_MIB(0xA041),	0, 0},
	{UIC_ARG_MIB(0xA042),	0, 0},
	{UIC_ARG_MIB(0xA043),	0, 0},
	{UIC_ARG_MIB(0xA044),	0, 0},
	{UIC_ARG_MIB(0xA045),	0, 0},
	{UIC_ARG_MIB(0xA046),	0, 0},
	{UIC_ARG_MIB(0xA047),	0, 0},
	{UIC_ARG_MIB(0xA060),	0, 0},
	{UIC_ARG_MIB(0xA061),	0, 0},
	{UIC_ARG_MIB(0xA062),	0, 0},
	{UIC_ARG_MIB(0xA063),	0, 0},
	{UIC_ARG_MIB(0xA064),	0, 0},
	{UIC_ARG_MIB(0xA065),	0, 0},
	{UIC_ARG_MIB(0xA066),	0, 0},
	{UIC_ARG_MIB(0xA067),	0, 0},
	{UIC_ARG_MIB(0xA068),	0, 0},
	{UIC_ARG_MIB(0xA069),	0, 0},
	{UIC_ARG_MIB(0xA06A),	0, 0},
	{UIC_ARG_MIB(0xA06B),	0, 0},
	{UIC_ARG_MIB(0xA06C),	0, 0},
	{UIC_ARG_MIB(0xA080),	0, 0},
	/* NL Standard */
	{UIC_ARG_MIB(0x3000),	0, 0},
	{UIC_ARG_MIB(0x3001),	0, 0},
	{UIC_ARG_MIB(0x4020),	0, 0},
	{UIC_ARG_MIB(0x4021),	0, 0},
	{UIC_ARG_MIB(0x4022),	0, 0},
	{UIC_ARG_MIB(0x4023),	0, 0},
	{UIC_ARG_MIB(0x4025),	0, 0},
	{UIC_ARG_MIB(0x402B),	0, 0},
	/* MPHY */
	{UIC_ARG_MIB(0x0021),	0, 0},
	{UIC_ARG_MIB(0x0022),	0, 0},
	{UIC_ARG_MIB(0x0023),	0, 0},
	{UIC_ARG_MIB(0x0024),	0, 0},
	{UIC_ARG_MIB(0x0028),	0, 0},
	{UIC_ARG_MIB(0x0029),	0, 0},
	{UIC_ARG_MIB(0x002A),	0, 0},
	{UIC_ARG_MIB(0x002B),	0, 0},
	{UIC_ARG_MIB(0x002C),	0, 0},
	{UIC_ARG_MIB(0x002D),	0, 0},
	{UIC_ARG_MIB(0x0033),	0, 0},
	{UIC_ARG_MIB(0x0035),	0, 0},
	{UIC_ARG_MIB(0x0036),	0, 0},
	{UIC_ARG_MIB(0x0041),	0, 0},
	{UIC_ARG_MIB(0x00A1),	0, 0},
	{UIC_ARG_MIB(0x00A2),	0, 0},
	{UIC_ARG_MIB(0x00A3),	0, 0},
	{UIC_ARG_MIB(0x00A4),	0, 0},
	{UIC_ARG_MIB(0x00A7),	0, 0},
	{UIC_ARG_MIB(0x00C1),	0, 0},
	{UIC_ARG_MIB(0x029b),	0, 0},
	{UIC_ARG_MIB(0x035d),	0, 0},
	{UIC_ARG_MIB(0x028B),	0, 0},
	{UIC_ARG_MIB(0x029A),	0, 0},
	{UIC_ARG_MIB(0x0277),	0, 0},

	{},
};

static void exynos_ufs_get_misc(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_ufs_clk_info *clki;
	struct list_head *head = &ufs->debug.misc.clk_list_head;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk))
			clki->freq = clk_get_rate(clki->clk);
	}
	ufs->debug.misc.isolation = readl(ufs->phy.reg_pmu);
}

static void exynos_ufs_get_sfr(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_ufs_sfr_log* cfg = ufs->debug.sfr;
	int sel_api = 0;
	int i = 0;

	while(cfg) {
		if (!cfg->name)
			break;

		if (cfg->offset >= LOG_STD_HCI_SFR) {
			/* Select an API to get SFRs */
			sel_api = cfg->offset;
		} else {
			/* Fetch value */
			if (sel_api == LOG_STD_HCI_SFR)
				cfg->val = ufshcd_readl(hba, cfg->offset);
			else if (sel_api == LOG_VS_HCI_SFR)
				cfg->val = hci_readl(ufs, cfg->offset);
			else if (sel_api == LOG_FMP_SFR)
				cfg->val = exynos_smc(SMC_CMD_SMU, FMP_SMU_DUMP, cfg->offset, 0);
			else if (sel_api == LOG_UNIPRO_SFR)
				cfg->val = unipro_readl(ufs, cfg->offset);
			else if (sel_api == LOG_PMA_SFR)
				cfg->val = phy_pma_readl(ufs, cfg->offset);
			else
				cfg->val = 0xFFFFFFFF;

		}

		if (dump_once_again)
			dump_sfr[i++] = cfg->val;

		/* Next SFR */
		cfg++;
	}
}

static void exynos_ufs_get_attr(struct ufs_hba *hba)
{
	u32 i;
	u32 intr_enable;
	struct exynos_ufs_attr_log* cfg = ufs_cfg_log_attr;
	int j = 0;

	/* Disable and backup interrupts */
	intr_enable = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
	ufshcd_writel(hba, 0, REG_INTERRUPT_ENABLE);

	while(cfg) {
		if (cfg->offset == 0)
			break;

		/* Send DME_GET */
		ufshcd_writel(hba, cfg->offset, REG_UIC_COMMAND_ARG_1);
		ufshcd_writel(hba, UIC_CMD_DME_GET, REG_UIC_COMMAND);

		i = 0;
		while(!(ufshcd_readl(hba, REG_INTERRUPT_STATUS) &
					UIC_COMMAND_COMPL)) {
			if (i++ > 20000) {
				dev_err(hba->dev,
					"Failed to fetch a value of %x",
					cfg->offset);
				goto out;
			}
		}

		/* Clear UIC command completion */
		ufshcd_writel(hba, UIC_COMMAND_COMPL, REG_INTERRUPT_STATUS);

		/* Fetch result and value */
		cfg->res = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2 &
				MASK_UIC_COMMAND_RESULT);
		cfg->val = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);

		if (dump_once_again)
			dump_attr[j++] = cfg->val;

		/* Next attribute */
		cfg++;
	}

out:
	/* Restore and enable interrupts */
	ufshcd_writel(hba, intr_enable, REG_INTERRUPT_ENABLE);
}

static void exynos_ufs_misc_dump(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_ufs_misc_log* cfg = &ufs->debug.misc;
	struct exynos_ufs_clk_info *clki;
	struct list_head *head = &cfg->clk_list_head;

	dev_err(hba->dev, ": --------------------------------------------------- \n");
	dev_err(hba->dev, ": \t\tMISC DUMP\n");
	dev_err(hba->dev, ": --------------------------------------------------- \n");

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			dev_err(hba->dev, "%s: %d\n",
					clki->name, clki->freq);
		}
	}
	dev_err(hba->dev, "iso: %d\n", ufs->debug.misc.isolation);
}

static void exynos_ufs_sfr_dump(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_ufs_sfr_log* cfg = ufs->debug.sfr;

	dev_err(hba->dev, ": --------------------------------------------------- \n");
	dev_err(hba->dev, ": \t\tREGISTER DUMP\n");
	dev_err(hba->dev, ": --------------------------------------------------- \n");

	while(cfg) {
		if (!cfg->name)
			break;

		/* Dump */
		dev_err(hba->dev, ": %s(0x%04x):\t\t\t\t0x%08x\n",
				cfg->name, cfg->offset, cfg->val);

		/* Next SFR */
		cfg++;
	}
}

static void exynos_ufs_attr_dump(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct exynos_ufs_attr_log* cfg = ufs->debug.attr;

	dev_err(hba->dev, ": --------------------------------------------------- \n");
	dev_err(hba->dev, ": \t\tATTRIBUTE DUMP\n");
	dev_err(hba->dev, ": --------------------------------------------------- \n");

	while(cfg) {
		if (!cfg->offset)
			break;

		/* Dump */
		dev_err(hba->dev, ": 0x%04x:\t\t0x%08x\t\t0x%08x\n",
				cfg->offset, cfg->val, cfg->res);

		/* Next SFR */
		cfg++;
	}
}

static void exynos_ufs_get_debug_info(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if ((hba->debug.flag & UFSHCD_DEBUG_DUMP) && dump_once_again)
		goto dump;

	if (!(ufs->misc_flags & EXYNOS_UFS_MISC_TOGGLE_LOG))
		return;

dump:
	exynos_ufs_get_sfr(hba);
	exynos_ufs_get_attr(hba);
	exynos_ufs_get_misc(hba);
	printk("dump_once_again %d\n", dump_once_again);
	WARN_ON(1);

#if defined(CONFIG_SCSI_UFS_TEST_MODE)
		/* do not recover system if test mode is enabled */
		BUG();
#endif

	if (hba->debug.flag & UFSHCD_DEBUG_DUMP)
		dump_once_again = 0;

	if (!(hba->debug.flag & UFSHCD_DEBUG_DUMP))
		ufs->misc_flags &= ~(EXYNOS_UFS_MISC_TOGGLE_LOG);
}

static inline
struct exynos_ufs_soc *exynos_ufs_get_drv_data(struct device *dev)
{
	const struct of_device_id *match;

	match = of_match_node(exynos_ufs_match, dev->of_node);
	return ((struct ufs_hba_variant *)match->data)->vs_data;
}

#define NUM_OF_PHY_CONFIGS	9
static int exynos_ufs_populate_dt_phy_cfg(struct device *dev,
		struct exynos_ufs_soc **org_soc)
{
	int ret = 1;
	int i, j;
	u32* val;
	int num_of_configs[NUM_OF_PHY_CONFIGS];
	struct exynos_ufs_soc *soc;
	struct ufs_phy_cfg *phy_cfg;
	const char *const configs[NUM_OF_PHY_CONFIGS] = {
		"phy-init",
		"post-phy-init",
		"calib-of-pwm",
		"calib-of-hs-rate-a",
		"calib-of-hs-rate-b",
		"post-calib-of-pwm",
		"post-calib-of-hs-rate-a",
		"post-calib-of-hs-rate-b",
		"pma-restore",
	};

	soc = devm_kzalloc(dev, sizeof(*soc), GFP_KERNEL);
	if (!soc) {
		dev_err(dev, "cannot allocate mem for phy configs\n");
		return -ENOMEM;
	}
	phy_cfg = soc->tbl_phy_init;

	dev_dbg(dev, "=== PHY config allocation complete ===\n");
	for (i=0 ; i<NUM_OF_PHY_CONFIGS ; i++) {
		/* Check if each config table exits */
		if(!of_get_property(dev->of_node, configs[i], &num_of_configs[i]))
			goto out;
		num_of_configs[i] /= sizeof(int);
		dev_dbg(dev, "%s: %d\n", configs[i], num_of_configs[i]);

		/* Buffer allocation for each table */
		phy_cfg = devm_kzalloc(dev,
			(num_of_configs[i] * sizeof(*phy_cfg)/sizeof(int)), GFP_KERNEL);
		if (!phy_cfg) {
			dev_err(dev, "cannot allocate mem for a phy table\n");
			return -ENOMEM;
		}

		/* Fetch config data from DT */
		if(of_property_read_u32_array(dev->of_node, configs[i], (u32*)phy_cfg,
					num_of_configs[i])) {
			devm_kfree(dev, phy_cfg);
			goto out;
		}
		val = (u32*)phy_cfg;
		for (j=0 ; j<num_of_configs[i]; j+=sizeof(int))
			dev_dbg(dev, "%08X %08X %08X %08X\n",
					val[j],val[j+1],val[j+2],val[j+3]);

		/* Move a pointer to indicate each table */
		*(&soc->tbl_phy_init+i) = phy_cfg;
	}
	ret = 0;
out:
	*org_soc = soc;
	return ret;
}

inline void exynos_ufs_ctrl_hci_core_clk(struct exynos_ufs *ufs, bool en)
{
	u32 reg = hci_readl(ufs, HCI_FORCE_HCS);

	if (en)
		hci_writel(ufs, reg | HCI_CORECLK_STOP_EN, HCI_FORCE_HCS);
	else
		hci_writel(ufs, reg & ~HCI_CORECLK_STOP_EN, HCI_FORCE_HCS);
}

static inline void exynos_ufs_ctrl_clk(struct exynos_ufs *ufs, bool en)
{
	u32 reg = hci_readl(ufs, HCI_FORCE_HCS);

	if (en)
		hci_writel(ufs, reg | CLK_STOP_CTRL_EN_ALL, HCI_FORCE_HCS);
	else
		hci_writel(ufs, reg & ~CLK_STOP_CTRL_EN_ALL, HCI_FORCE_HCS);
}

static inline void exynos_ufs_gate_clk(struct exynos_ufs *ufs, bool en)
{

	u32 reg = hci_readl(ufs, HCI_CLKSTOP_CTRL);

	if (en)
		hci_writel(ufs, reg | CLK_STOP_ALL, HCI_CLKSTOP_CTRL);
	else
		hci_writel(ufs, reg & ~CLK_STOP_ALL, HCI_CLKSTOP_CTRL);
}

static void exynos_ufs_set_unipro_pclk(struct exynos_ufs *ufs)
{
	u32 pclk_rate;
	u32 f_min, f_max;
	u8 div = 0;

	f_min = ufs->pclk_avail_min;
	f_max = ufs->pclk_avail_max;
	pclk_rate = clk_get_rate(ufs->pclk);

	do {
		pclk_rate /= (div + 1);

		if (pclk_rate <= f_max)
			break;
		else
			div++;
	} while (pclk_rate >= f_min);

	WARN(pclk_rate < f_min, "not available pclk range %d\n", pclk_rate);

	ufs->pclk_rate = pclk_rate;
}

static void exynos_ufs_set_unipro_mclk(struct exynos_ufs *ufs)
{
	ufs->mclk_rate = clk_get_rate(ufs->clk_unipro);
}

static void exynos_ufs_set_unipro_clk(struct exynos_ufs *ufs)
{
	exynos_ufs_set_unipro_pclk(ufs);
	exynos_ufs_set_unipro_mclk(ufs);
}

static void exynos_ufs_set_pwm_clk_div(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	const int div = 30, mult = 20;
	const unsigned long pwm_min = 3 * 1000 * 1000;
	const unsigned long pwm_max = 9 * 1000 * 1000;
	long clk_period;
	const int divs[] = {32, 16, 8, 4};
	unsigned long _clk, clk = 0;
	int i = 0, clk_idx = -1;

	clk_period = UNIPRO_PCLK_PERIOD(ufs);

	for (i = 0; i < ARRAY_SIZE(divs); i++) {
		_clk = NSEC_PER_SEC * mult / (clk_period * divs[i] * div);
		if (_clk >= pwm_min && _clk <= pwm_max) {
			if (_clk > clk) {
				clk_idx = i;
				clk = _clk;
			}
		}
	}
	ufs->pwm_freq = clk;

	if (clk_idx >= 0)
		ufshcd_dme_set(hba, UIC_ARG_MIB(CMN_PWM_CMN_CTRL),
				clk_idx & PWM_CMN_CTRL_MASK);
	else
		dev_err(ufs->dev, "not dicided pwm clock divider\n");
}

static long exynos_ufs_calc_time_cntr(struct exynos_ufs *ufs, long period)
{
	const int precise = 10;
	long pclk_rate = ufs->pclk_rate;
	long clk_period, fraction;

	clk_period = UNIPRO_PCLK_PERIOD(ufs);
	fraction = ((NSEC_PER_SEC % pclk_rate) * precise) / pclk_rate;

	return (period * precise) / ((clk_period * precise) + fraction);
}

static int exynos_ufs_calc_line_init_pre_len(struct exynos_ufs *ufs, int tx_ls_prep_len)
{
	u32 pwm_g1_ns;
	u32 val = 1;
	u32 result;
	u32 pwm_gear = 1;
	long pwm_g1_freq;
	int i;
	int pow_val;

	pwm_g1_freq = ufs->pwm_freq;

	pwm_g1_ns = (1000 *1000 *1000 / pwm_g1_freq); //ns

	pow_val = (tx_ls_prep_len + pwm_gear - 7);
	if (pow_val <= 0)
		val = 1;
	else {
		for(i = 0; i < pow_val; i++)
			val *= 2;
	}

	result = pwm_g1_ns * 10 * val;

	return (result/1000);
}

static void exynos_ufs_set_line_init_prep_len(struct exynos_ufs *ufs)
{
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;
	struct ufs_hba *hba = ufs->hba;
	u32 tx_ls_prepare_length;
	u32 result;
	u32 result_div;
	u32 rate = act_pmd->hs_series;
	int i;

	for (i = 0; i <= 15; i++) {  /* gear1 prepare length 1~15 */
		tx_ls_prepare_length = i;

		result = exynos_ufs_calc_line_init_pre_len(ufs, tx_ls_prepare_length);

		if (result > 200)
			break;
	}

	if (rate == 1)
		result_div = ((result * 10000) / 160);
	else
		result_div = ((result * 10000) / 136);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), TRUE);
	for_each_ufs_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x29E, i), (result_div >> 8) & 0xFF);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x29F, i), result_div & 0xFF);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x29D, i), 0x10);
	}
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), FALSE);
}


static void exynos_ufs_compute_phy_time_v(struct exynos_ufs *ufs,
					struct phy_tm_parm *tm_parm)
{
	tm_parm->tx_linereset_p =
		exynos_ufs_calc_time_cntr(ufs, TX_DIF_P_NSEC);
	tm_parm->tx_linereset_n =
		exynos_ufs_calc_time_cntr(ufs, TX_DIF_N_NSEC);
	tm_parm->tx_high_z_cnt =
		exynos_ufs_calc_time_cntr(ufs, TX_HIGH_Z_CNT_NSEC);
	tm_parm->tx_base_n_val =
		exynos_ufs_calc_time_cntr(ufs, TX_BASE_UNIT_NSEC);
	tm_parm->tx_gran_n_val =
		exynos_ufs_calc_time_cntr(ufs, TX_GRAN_UNIT_NSEC);
	tm_parm->tx_sleep_cnt =
		exynos_ufs_calc_time_cntr(ufs, TX_SLEEP_CNT);

	tm_parm->rx_linereset =
		exynos_ufs_calc_time_cntr(ufs, RX_DIF_P_NSEC);
	tm_parm->rx_hibern8_wait =
		exynos_ufs_calc_time_cntr(ufs, RX_HIBERN8_WAIT_NSEC);
	tm_parm->rx_base_n_val =
		exynos_ufs_calc_time_cntr(ufs, RX_BASE_UNIT_NSEC);
	tm_parm->rx_gran_n_val =
		exynos_ufs_calc_time_cntr(ufs, RX_GRAN_UNIT_NSEC);
	tm_parm->rx_sleep_cnt =
		exynos_ufs_calc_time_cntr(ufs, RX_SLEEP_CNT);
	tm_parm->rx_stall_cnt =
		exynos_ufs_calc_time_cntr(ufs, RX_STALL_CNT);
}

static void exynos_ufs_config_phy_time_v(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	struct phy_tm_parm tm_parm;
	int i;

	exynos_ufs_compute_phy_time_v(ufs, &tm_parm);

	exynos_ufs_set_pwm_clk_div(ufs);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), TRUE);

	for_each_ufs_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_FILLER_ENABLE, i),
				0x2);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_LINERESET_VAL, i),
				RX_LINERESET(tm_parm.rx_linereset));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_BASE_NVAL_07_00, i),
				RX_BASE_NVAL_L(tm_parm.rx_base_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_BASE_NVAL_15_08, i),
				RX_BASE_NVAL_H(tm_parm.rx_base_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_GRAN_NVAL_07_00, i),
				RX_GRAN_NVAL_L(tm_parm.rx_gran_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_GRAN_NVAL_10_08, i),
				RX_GRAN_NVAL_H(tm_parm.rx_gran_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_OV_SLEEP_CNT_TIMER, i),
				RX_OV_SLEEP_CNT(tm_parm.rx_sleep_cnt));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_OV_STALL_CNT_TIMER, i),
				RX_OV_STALL_CNT(tm_parm.rx_stall_cnt));
	}

	for_each_ufs_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_LINERESET_P_VAL, i),
				TX_LINERESET_P(tm_parm.tx_linereset_p));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_HIGH_Z_CNT_07_00, i),
				TX_HIGH_Z_CNT_L(tm_parm.tx_high_z_cnt));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_HIGH_Z_CNT_11_08, i),
				TX_HIGH_Z_CNT_H(tm_parm.tx_high_z_cnt));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_BASE_NVAL_07_00, i),
				TX_BASE_NVAL_L(tm_parm.tx_base_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_BASE_NVAL_15_08, i),
				TX_BASE_NVAL_H(tm_parm.tx_base_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_GRAN_NVAL_07_00, i),
				TX_GRAN_NVAL_L(tm_parm.tx_gran_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_GRAN_NVAL_10_08, i),
				TX_GRAN_NVAL_H(tm_parm.tx_gran_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_OV_SLEEP_CNT_TIMER, i),
				TX_OV_H8_ENTER_EN |
				TX_OV_SLEEP_CNT(tm_parm.tx_sleep_cnt));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_MIN_ACTIVATE_TIME, i),
				0xA);
	}

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), FALSE);
}

static void exynos_ufs_config_phy_cap_attr(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	int i;

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), TRUE);

	for_each_ufs_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G1_SYNC_LENGTH_CAP, i),
				SYNC_RANGE_COARSE | SYNC_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G2_SYNC_LENGTH_CAP, i),
				SYNC_RANGE_COARSE | SYNC_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G3_SYNC_LENGTH_CAP, i),
				SYNC_RANGE_COARSE | SYNC_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G1_PREP_LENGTH_CAP, i),
				PREP_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G2_PREP_LENGTH_CAP, i),
				PREP_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G3_PREP_LENGTH_CAP, i),
				PREP_LEN(0xf));
	}

	if (ufs->rx_adv_fine_gran_sup_en == 0) {
		for_each_ufs_lane(ufs, i) {
			ufshcd_dme_set(hba,
				UIC_ARG_MIB_SEL(RX_ADV_GRANULARITY_CAP, i), 0);

			ufshcd_dme_set(hba,
				UIC_ARG_MIB_SEL(TX_ADV_GRANULARITY_CAP, i), 0);

			if (ufs->rx_min_actv_time_cap)
				ufshcd_dme_set(hba,
					UIC_ARG_MIB_SEL(RX_MIN_ACTIVATETIME_CAP, i),
					ufs->rx_min_actv_time_cap);

			if (ufs->rx_hibern8_time_cap)
				ufshcd_dme_set(hba,
					UIC_ARG_MIB_SEL(RX_HIBERN8TIME_CAP, i),
					ufs->rx_hibern8_time_cap);

			if (ufs->tx_hibern8_time_cap)
				ufshcd_dme_set(hba,
					UIC_ARG_MIB_SEL(TX_HIBERN8TIME_CAP, i),
					ufs->tx_hibern8_time_cap);
		}
	} else if (ufs->rx_adv_fine_gran_sup_en == 1) {
		for_each_ufs_lane(ufs, i) {
			if (ufs->rx_adv_fine_gran_step)
				ufshcd_dme_set(hba,
					UIC_ARG_MIB_SEL(RX_ADV_GRANULARITY_CAP, i),
					RX_ADV_FINE_GRAN_STEP(ufs->rx_adv_fine_gran_step));

			if (ufs->rx_adv_min_actv_time_cap)
				ufshcd_dme_set(hba,
					UIC_ARG_MIB_SEL(RX_ADV_MIN_ACTIVATETIME_CAP, i),
					ufs->rx_adv_min_actv_time_cap);

			if (ufs->rx_adv_hibern8_time_cap)
				ufshcd_dme_set(hba,
					UIC_ARG_MIB_SEL(RX_ADV_HIBERN8TIME_CAP, i),
					ufs->rx_adv_hibern8_time_cap);
		}
	}

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), FALSE);
}

static void exynos_ufs_establish_connt(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;

	/* allow cport attributes to be set */
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CONNECTIONSTATE), CPORT_IDLE);

	/* local */
	ufshcd_dme_set(hba, UIC_ARG_MIB(N_DEVICEID), DEV_ID);
	ufshcd_dme_set(hba, UIC_ARG_MIB(N_DEVICEID_VALID), TRUE);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_PEERDEVICEID), PEER_DEV_ID);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_PEERCPORTID), PEER_CPORT_ID);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CPORTFLAGS), CPORT_DEF_FLAGS);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_TRAFFICCLASS), TRAFFIC_CLASS);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CONNECTIONSTATE), CPORT_CONNECTED);
}

static void exynos_ufs_config_smu(struct exynos_ufs *ufs)
{
	int ret;

#if defined(CONFIG_UFS_FMP_DM_CRYPT) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
	ret = exynos_smc(SMC_CMD_FMP, FMP_SECURITY, UFS_FMP, FMP_DESC_ON);
#else
	ret = exynos_smc(SMC_CMD_FMP, FMP_SECURITY, UFS_FMP, FMP_DESC_OFF);
#endif
	if (ret)
		dev_err(ufs->dev, "Fail to smc call for FMP SECURITY\n");

#if defined(CONFIG_UFS_FMP_DM_CRYPT) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
	ret = exynos_smc(SMC_CMD_SMU, FMP_SMU_INIT, FMP_SMU_ON, 0);
#else
	ret = exynos_smc(SMC_CMD_SMU, FMP_SMU_INIT, FMP_SMU_OFF, 0);
#endif
	if (ret)
		dev_err(ufs->dev, "Fail to smc call for FMP SMU Initialization\n");

	return;
}

static void exynos_ufs_enable_io_coherency(struct exynos_ufs *ufs)
{
#if defined(CONFIG_SCSI_SKIP_CPU_SYNC)
	u32 reg;

	if (!of_get_child_by_name(ufs->dev->of_node, "ufs-sys")) {
		panic("Not configured to use IO coherency");
		return;
	}

	reg = readl(ufs->sys.reg_sys);
	writel(reg | ((1 << 16) | (1 << 17)), ufs->sys.reg_sys);
#endif
}

static bool exynos_ufs_wait_pll_lock(struct exynos_ufs *ufs)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(100);
	u32 reg;

	do {
		reg = phy_pma_readl(ufs, PHY_PMA_COMN_ADDR(0x1E));
		if ((reg >> 5) & 0x1)
			return true;
		usleep_range(1, 1);
	} while (time_before(jiffies, timeout));

	dev_err(ufs->dev, "timeout mphy pll lock\n");
	exynos_ufs_get_sfr(ufs->hba);
	exynos_ufs_sfr_dump(ufs->hba);

	return false;
}

static bool exynos_ufs_wait_cdr_lock(struct exynos_ufs *ufs)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(100);
	u32 reg, val;

	val = ufs->wait_cdr_lock;

	do {
		/* Need to check for all lane? */
		reg = phy_pma_readl(ufs, PHY_PMA_TRSV_ADDR(val, 0));
		if ((reg >> 4) & 0x1)
			return true;
		usleep_range(1, 1);
	} while (time_before(jiffies, timeout));

	dev_err(ufs->dev, "timeout mphy cdr lock\n");
	exynos_ufs_get_sfr(ufs->hba);
	exynos_ufs_sfr_dump(ufs->hba);

	return false;
}

static inline bool __match_mode_by_cfg(struct uic_pwr_mode *pmd, int mode)
{
	bool match = false;
	u8 _m, _l, _g;

	_m = pmd->mode;
	_g = pmd->gear;
	_l = pmd->lane;

	if (mode == PMD_ALL)
		match = true;
	else if (IS_PWR_MODE_HS(_m) && mode == PMD_HS)
		match = true;
	else if (IS_PWR_MODE_PWM(_m) && mode == PMD_PWM)
		match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 1 && _l == 1
			&& mode & PMD_HS_G1_L1)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 1 && _l == 2
			&& mode & PMD_HS_G1_L2)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 2 && _l == 1
			&& mode & PMD_HS_G2_L1)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 2 && _l == 2
			&& mode & PMD_HS_G2_L2)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 3 && _l == 1
			&& mode & PMD_HS_G3_L1)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 3 && _l == 2
			&& mode & PMD_HS_G3_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 1 && _l == 1
			&& mode & PMD_PWM_G1_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 1 && _l == 2
			&& mode & PMD_PWM_G1_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 2 && _l == 1
			&& mode & PMD_PWM_G2_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 2 && _l == 2
			&& mode & PMD_PWM_G2_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 3 && _l == 1
			&& mode & PMD_PWM_G3_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 3 && _l == 2
			&& mode & PMD_PWM_G3_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 4 && _l == 1
			&& mode & PMD_PWM_G4_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 4 && _l == 2
			&& mode & PMD_PWM_G4_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 5 && _l == 1
			&& mode & PMD_PWM_G5_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 5 && _l == 2
			&& mode & PMD_PWM_G5_L2)
			match = true;

	return match;
}

static void exynos_ufs_config_phy(struct exynos_ufs *ufs,
				  const struct ufs_phy_cfg *cfg,
				  struct uic_pwr_mode *pmd)
{
	struct ufs_hba *hba = ufs->hba;
	int i;

	if (!cfg)
		return;

	for_each_phy_cfg(cfg) {
		for_each_ufs_lane(ufs, i) {
			if (pmd && !__match_mode_by_cfg(pmd, cfg->flg))
				continue;

			switch (cfg->lyr) {
			case PHY_PCS_COMN:
			case UNIPRO_STD_MIB:
			case UNIPRO_DBG_MIB:
				if (i == 0)
					ufshcd_dme_set(hba, UIC_ARG_MIB(cfg->addr), cfg->val);
				break;
			case PHY_PCS_RXTX:
				ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr, i), cfg->val);
				break;
			case PHY_PMA_COMN:
				if (i == 0)
					phy_pma_writel(ufs, cfg->val, PHY_PMA_COMN_ADDR(cfg->addr));
				break;
			case PHY_PMA_TRSV:
				phy_pma_writel(ufs, cfg->val, PHY_PMA_TRSV_ADDR(cfg->addr, i));
				break;
			case UNIPRO_DBG_APB:
				unipro_writel(ufs, cfg->val, cfg->addr);
			}
		}
	}
}

static void exynos_ufs_config_sync_pattern_mask(struct exynos_ufs *ufs,
					struct uic_pwr_mode *pmd)
{
	struct ufs_hba *hba = ufs->hba;
	u8 g = pmd->gear;
	u32 mask, sync_len;
	int i;
#define SYNC_LEN_G1	(80 * 1000) /* 80 us */
#define SYNC_LEN_G2	(40 * 1000) /* 40 us */
#define SYNC_LEN_G3	(20 * 1000) /* 20 us */

	if (g == 1)
		sync_len = SYNC_LEN_G1;
	else if (g == 2)
		sync_len = SYNC_LEN_G2;
	else if (g == 3)
		sync_len = SYNC_LEN_G3;
	else
		return;

	mask = exynos_ufs_calc_time_cntr(ufs, sync_len);
	mask = (mask >> 8) & 0xff;

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), TRUE);

	for_each_ufs_lane(ufs, i)
		ufshcd_dme_set(hba,
			UIC_ARG_MIB_SEL(RX_SYNC_MASK_LENGTH, i), mask);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), FALSE);
}

static int exynos_ufs_pre_prep_pmc(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *pwr_max,
		struct ufs_pa_layer_attr *pwr_req)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	const struct exynos_ufs_soc *soc = to_phy_soc(ufs);
	struct uic_pwr_mode *req_pmd = &ufs->req_pmd_parm;
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;

	pwr_req->gear_rx
		= act_pmd->gear= min_t(u8, pwr_max->gear_rx, req_pmd->gear);
	pwr_req->gear_tx
		= act_pmd->gear = min_t(u8, pwr_max->gear_tx, req_pmd->gear);
	pwr_req->lane_rx
		= act_pmd->lane = min_t(u8, pwr_max->lane_rx, req_pmd->lane);
	pwr_req->lane_tx
		= act_pmd->lane = min_t(u8, pwr_max->lane_tx, req_pmd->lane);
	pwr_req->pwr_rx = act_pmd->mode = req_pmd->mode;
	pwr_req->pwr_tx = act_pmd->mode = req_pmd->mode;
	pwr_req->hs_rate = act_pmd->hs_series = req_pmd->hs_series;
	act_pmd->local_l2_timer[0] = req_pmd->local_l2_timer[0];
	act_pmd->local_l2_timer[1] = req_pmd->local_l2_timer[1];
	act_pmd->local_l2_timer[2] = req_pmd->local_l2_timer[2];

	act_pmd->remote_l2_timer[0] = req_pmd->remote_l2_timer[0];
	act_pmd->remote_l2_timer[1] = req_pmd->remote_l2_timer[1];
	act_pmd->remote_l2_timer[2] = req_pmd->remote_l2_timer[2];

	/* Set L2 timer */
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(DL_FC0PROTTIMEOUTVAL), act_pmd->local_l2_timer[0]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(DL_TC0REPLAYTIMEOUTVAL), act_pmd->local_l2_timer[1]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(DL_AFC0REQTIMEOUTVAL), act_pmd->local_l2_timer[2]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(PA_PWRMODEUSERDATA0), act_pmd->remote_l2_timer[0]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(PA_PWRMODEUSERDATA1), act_pmd->remote_l2_timer[1]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(PA_PWRMODEUSERDATA2), act_pmd->remote_l2_timer[2]);

	unipro_writel(ufs, act_pmd->local_l2_timer[0], UNIP_DME_PWR_REQ_LOCALL2TIMER0);
	unipro_writel(ufs, act_pmd->local_l2_timer[1], UNIP_DME_PWR_REQ_LOCALL2TIMER1);
	unipro_writel(ufs, act_pmd->local_l2_timer[2], UNIP_DME_PWR_REQ_LOCALL2TIMER2);
	unipro_writel(ufs, act_pmd->remote_l2_timer[0], UNIP_DME_PWR_REQ_REMOTEL2TIMER0);
	unipro_writel(ufs, act_pmd->remote_l2_timer[1], UNIP_DME_PWR_REQ_REMOTEL2TIMER1);
	unipro_writel(ufs, act_pmd->remote_l2_timer[2], UNIP_DME_PWR_REQ_REMOTEL2TIMER2);

	if (!soc)
		goto out;

	if (IS_PWR_MODE_HS(act_pmd->mode)) {
		exynos_ufs_config_sync_pattern_mask(ufs, act_pmd);

		switch (act_pmd->hs_series) {
		case PA_HS_MODE_A:
			exynos_ufs_config_phy(ufs,
				soc->tbl_calib_of_hs_rate_a, act_pmd);
			break;
		case PA_HS_MODE_B:
			exynos_ufs_config_phy(ufs,
				soc->tbl_calib_of_hs_rate_b, act_pmd);
			break;
		default:
			break;
		}
	} else if (IS_PWR_MODE_PWM(act_pmd->mode)) {
		exynos_ufs_config_phy(ufs, soc->tbl_calib_of_pwm, act_pmd);
	}

out:
	return 0;
}

static int exynos_ufs_post_prep_pmc(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *pwr_max,
		struct ufs_pa_layer_attr *pwr_req)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	const struct exynos_ufs_soc *soc = to_phy_soc(ufs);
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;
	int ret = 0;

	if (!soc)
		goto out;

	if (IS_PWR_MODE_HS(act_pmd->mode)) {
		switch (act_pmd->hs_series) {
		case PA_HS_MODE_A:
			exynos_ufs_config_phy(ufs,
				soc->tbl_post_calib_of_hs_rate_a, act_pmd);
			break;
		case PA_HS_MODE_B:
			exynos_ufs_config_phy(ufs,
				soc->tbl_post_calib_of_hs_rate_b, act_pmd);
			break;
		default:
			break;
		}

		if (!(exynos_ufs_wait_pll_lock(ufs) &&
		      exynos_ufs_wait_cdr_lock(ufs)))
			ret = -EPERM;
	} else if (IS_PWR_MODE_PWM(act_pmd->mode)) {
		exynos_ufs_config_phy(ufs, soc->tbl_post_calib_of_pwm, act_pmd);
	}

out:
	dev_info(ufs->dev,
		"Power mode change(%d): M(%d)G(%d)L(%d)HS-series(%d)\n",
		 ret, act_pmd->mode, act_pmd->gear,
		 act_pmd->lane, act_pmd->hs_series);

	return ret;
}

static void exynos_ufs_set_nexus_t_xfer_req(struct ufs_hba *hba,
				int tag, struct scsi_cmnd *cmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	u32 type;

	type =  hci_readl(ufs, HCI_UTRL_NEXUS_TYPE);

	if (cmd)
		type |= (1 << tag);
	else
		type &= ~(1 << tag);

	hci_writel(ufs, type, HCI_UTRL_NEXUS_TYPE);
}

static void exynos_ufs_set_nexus_t_task_mgmt(struct ufs_hba *hba, int tag, u8 tm_func)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	u32 type;

	type =  hci_readl(ufs, HCI_UTMRL_NEXUS_TYPE);

	switch (tm_func) {
	case UFS_ABORT_TASK:
	case UFS_QUERY_TASK:
		type |= (1 << tag);
		break;
	case UFS_ABORT_TASK_SET:
	case UFS_CLEAR_TASK_SET:
	case UFS_LOGICAL_RESET:
	case UFS_QUERY_TASK_SET:
		type &= ~(1 << tag);
		break;
	}

	hci_writel(ufs, type, HCI_UTMRL_NEXUS_TYPE);
}

static void exynos_ufs_phy_init(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	const struct exynos_ufs_soc *soc = to_phy_soc(ufs);

	if (ufs->avail_ln_rx == 0 || ufs->avail_ln_tx == 0) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_AVAILRXDATALANES),
			&ufs->avail_ln_rx);
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_AVAILTXDATALANES),
			&ufs->avail_ln_tx);
		WARN(ufs->avail_ln_rx != ufs->avail_ln_tx,
			"available data lane is not equal(rx:%d, tx:%d)\n",
			ufs->avail_ln_rx, ufs->avail_ln_tx);
	}

	if (!soc)
		return;

	exynos_ufs_config_phy(ufs, soc->tbl_phy_init, NULL);
}

static void exynos_ufs_config_unipro(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;

	unipro_writel(ufs, 0x1, UNIP_DME_PACP_CNFBIT);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_CLK_PERIOD),
		DIV_ROUND_UP(NSEC_PER_SEC, ufs->mclk_rate));
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTRAILINGCLOCKS), TXTRAILINGCLOCKS);
}

static void exynos_ufs_config_intr(struct exynos_ufs *ufs, u32 errs, u8 index)
{
	switch(index) {
	case UNIP_PA_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_PA_LAYER);
		break;
	case UNIP_DL_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_DL_LAYER);
		break;
	case UNIP_N_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_N_LAYER);
		break;
	case UNIP_T_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_T_LAYER);
		break;
	case UNIP_DME_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_DME_LAYER);
		break;
	}
}

static int exynos_ufs_pre_link(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* refer to hba */
	ufs->hba = hba;

	/* hci */
	exynos_ufs_config_intr(ufs, DFES_DEF_DL_ERRS, UNIP_DL_LYR);
	exynos_ufs_config_intr(ufs, DFES_DEF_N_ERRS, UNIP_N_LYR);
	exynos_ufs_config_intr(ufs, DFES_DEF_T_ERRS, UNIP_T_LYR);

	exynos_ufs_set_unipro_clk(ufs);
	exynos_ufs_ctrl_clk(ufs, true);

	/* mphy */
	exynos_ufs_phy_init(ufs);

	/* unipro */
	exynos_ufs_config_unipro(ufs);

	/* mphy */
	exynos_ufs_config_phy_time_v(ufs);
	exynos_ufs_config_phy_cap_attr(ufs);

	if (ufs->opts & EXYNOS_UFS_OPTS_SET_LINE_INIT_PREP_LEN)
		exynos_ufs_set_line_init_prep_len(ufs);
	return 0;
}

static void exynos_ufs_fit_aggr_timeout(struct exynos_ufs *ufs)
{
	const u8 cnt_div_val = 40;
	u32 cnt_val;

	cnt_val = exynos_ufs_calc_time_cntr(ufs, IATOVAL_NSEC / cnt_div_val);
	hci_writel(ufs, cnt_val & CNT_VAL_1US_MASK, HCI_1US_TO_CNT_VAL);
}

static int exynos_ufs_post_link(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	const struct exynos_ufs_soc *soc = to_phy_soc(ufs);
	u32 peer_rx_min_actv_time_cap;
	u32 max_rx_hibern8_time_cap;

	exynos_ufs_establish_connt(ufs);
	exynos_ufs_fit_aggr_timeout(ufs);

	hci_writel(ufs, 0xA, HCI_DATA_REORDER);
	hci_writel(ufs, PRDT_PREFECT_EN | PRDT_SET_SIZE(12),
			HCI_TXPRDT_ENTRY_SIZE);
	hci_writel(ufs, PRDT_SET_SIZE(12), HCI_RXPRDT_ENTRY_SIZE);
	hci_writel(ufs, 0xFFFFFFFF, HCI_UTRL_NEXUS_TYPE);
	hci_writel(ufs, 0xFFFFFFFF, HCI_UTMRL_NEXUS_TYPE);
	hci_writel(ufs, 0xf, HCI_AXIDMA_RWDATA_BURST_LEN);

	if (ufs->opts & EXYNOS_UFS_OPTS_SKIP_CONNECTION_ESTAB)
		ufshcd_dme_set(hba,
			UIC_ARG_MIB(T_DBG_SKIP_INIT_HIBERN8_EXIT), TRUE);

	if (ufs->pa_granularity) {
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_MODE), TRUE);
		ufshcd_dme_set(hba,
			UIC_ARG_MIB(PA_GRANULARITY), ufs->pa_granularity);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_MODE), FALSE);

		if (ufs->pa_tactivate)
			ufshcd_dme_set(hba,
				UIC_ARG_MIB(PA_TACTIVATE), ufs->pa_tactivate);

		if (ufs->pa_hibern8time)
			ufshcd_dme_set(hba,
				UIC_ARG_MIB(PA_HIBERN8TIME), ufs->pa_hibern8time);
	} else {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TACTIVATE), &peer_rx_min_actv_time_cap);
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_HIBERN8TIME), &max_rx_hibern8_time_cap);
		if (ufs->rx_min_actv_time_cap <= peer_rx_min_actv_time_cap)
			ufshcd_dme_peer_set(hba, UIC_ARG_MIB(PA_TACTIVATE), peer_rx_min_actv_time_cap + 1);
		ufshcd_dme_set(hba,UIC_ARG_MIB(PA_HIBERN8TIME), max_rx_hibern8_time_cap + 1);
	}

	if (soc)
		exynos_ufs_config_phy(ufs, soc->tbl_post_phy_init, NULL);

	return 0;
}

static int exynos_ufs_init(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct list_head *head = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	struct exynos_ufs_clk_info *exynos_clki;

	exynos_ufs_ctrl_hci_core_clk(ufs, false);
	exynos_ufs_config_smu(ufs);

	/* setup for debugging */
	ufs_host_backup[ufs_host_index++] = ufs;
	ufs->debug.sfr = ufs_cfg_log_sfr;
	ufs->debug.attr = ufs_cfg_log_attr;
	INIT_LIST_HEAD(&ufs->debug.misc.clk_list_head);

	if (!head || list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		exynos_clki = devm_kzalloc(hba->dev, sizeof(*exynos_clki), GFP_KERNEL);
		if (!exynos_clki) {
			return -ENOMEM;
		}
		exynos_clki->clk = clki->clk;
		exynos_clki->name = clki->name;
		exynos_clki->freq = 0;
		list_add_tail(&exynos_clki->list, &ufs->debug.misc.clk_list_head);

		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (!strcmp(clki->name, "aclk_ufs"))
				ufs->clk_hci = clki->clk;
			if (!strcmp(clki->name, "sclk_ufsunipro"))
				ufs->clk_unipro = clki->clk;

			if (!(ufs->opts & EXYNOS_UFS_OPTS_USE_SEPERATED_PCLK)) {
				if (!strcmp(clki->name, "aclk_ufs"))
					ufs->pclk = clki->clk;
			} else {
				if (!strcmp(clki->name, "sclk_ufsunipro20_cfg"))
					ufs->pclk = clki->clk;
			}
		}
	}
out:
	if (!ufs->clk_hci || !ufs->clk_unipro)
		return -EINVAL;

	return 0;
}

static void exynos_ufs_host_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	unsigned long timeout = jiffies + msecs_to_jiffies(1);

	exynos_ufs_ctrl_hci_core_clk(ufs, false);

	hci_writel(ufs, UFS_SW_RST_MASK, HCI_SW_RST);

	do {
		if (!(hci_readl(ufs, HCI_SW_RST) & UFS_SW_RST_MASK))
			return;
	} while (time_before(jiffies, timeout));

	dev_err(ufs->dev, "timeout host sw-reset\n");

	exynos_ufs_get_debug_info(hba);
	exynos_ufs_misc_dump(hba);
	exynos_ufs_sfr_dump(hba);
	exynos_ufs_attr_dump(hba);
}

static inline void exynos_ufs_dev_reset_ctrl(struct exynos_ufs *ufs, bool en)
{

	if (en)
		hci_writel(ufs, 1 << 0, HCI_GPIO_OUT);
	else
		hci_writel(ufs, 0 << 0, HCI_GPIO_OUT);
}

static void exynos_ufs_dev_hw_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* bit[1] for resetn */
	hci_writel(ufs, 0 << 0, HCI_GPIO_OUT);
	udelay(5);
	hci_writel(ufs, 1 << 0, HCI_GPIO_OUT);
}

static void exynos_ufs_pre_hibern8(struct ufs_hba *hba, u8 enter)
{
}

static void exynos_ufs_post_hibern8(struct ufs_hba *hba, u8 enter)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if (!enter) {
		struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;
		u32 mode = 0;

		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE), &mode);
		if (mode != (act_pmd->mode << 4 | act_pmd->mode)) {
			dev_warn(hba->dev, "%s: power mode change\n", __func__);
			hba->pwr_info.pwr_rx = (mode >> 4) & 0xf;
			hba->pwr_info.pwr_tx = mode & 0xf;
			ufshcd_config_pwr_mode(hba, &hba->max_pwr_info.info);
		}

		if (!(ufs->opts & EXYNOS_UFS_OPTS_SKIP_CONNECTION_ESTAB))
			exynos_ufs_establish_connt(ufs);
	}
}

static int exynos_ufs_link_startup_notify(struct ufs_hba *hba, bool notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		exynos_ufs_enable_io_coherency(ufs);
		exynos_ufs_dev_hw_reset(hba);
		ret = exynos_ufs_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = exynos_ufs_post_link(hba);
		break;
	default:
		break;
	}

	return ret;
}

static int exynos_ufs_pwr_change_notify(struct ufs_hba *hba, bool notify,
					struct ufs_pa_layer_attr *pwr_max,
					struct ufs_pa_layer_attr *pwr_req)
{
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		ret = exynos_ufs_pre_prep_pmc(hba, pwr_max, pwr_req);
		break;
	case POST_CHANGE:
		ret = exynos_ufs_post_prep_pmc(hba, NULL, pwr_req);
		break;
	default:
		break;
	}

	return ret;
}

static void exynos_ufs_hibern8_notify(struct ufs_hba *hba,
				u8 enter, bool notify)
{
	switch (notify) {
	case PRE_CHANGE:
		exynos_ufs_pre_hibern8(hba, enter);
		break;
	case POST_CHANGE:
		exynos_ufs_post_hibern8(hba, enter);
		break;
	default:
		break;
	}
}

static void exynos_ufs_clock_control_notify(struct ufs_hba *hba, bool on, bool notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	s32 pm_qos_int_value = ufs->pm_qos_int_value;

	switch (notify) {
	case PRE_CHANGE:
		if (on) {
#ifdef CONFIG_CPU_IDLE
			exynos_update_ip_idle_status(ufs->idle_ip_index, 0);
#endif
			exynos_ufs_ctrl_hci_core_clk(ufs, false);
			exynos_ufs_gate_clk(ufs, false);
		} else {
			pm_qos_update_request(&ufs->pm_qos_int, 0);

			phy_pma_writel(ufs, 0xff, PHY_PMA_TRSV_ADDR(0x4e, 0));
			phy_pma_writel(ufs, 0x3f, PHY_PMA_TRSV_ADDR(0x4f, 0));
			phy_pma_writel(ufs, 0x78, PHY_PMA_COMN_ADDR(0x15));
			phy_pma_readl(ufs, PHY_PMA_COMN_ADDR(0x15));
		}
		break;
	case POST_CHANGE:
		if (on) {
			phy_pma_writel(ufs, 0x00, PHY_PMA_COMN_ADDR(0x15));
			phy_pma_writel(ufs, 0x00, PHY_PMA_TRSV_ADDR(0x4f, 0));
			phy_pma_writel(ufs, 0x80, PHY_PMA_TRSV_ADDR(0x4e, 0));

			pm_qos_update_request(&ufs->pm_qos_int, pm_qos_int_value);
		} else {
			exynos_ufs_gate_clk(ufs, true);
			exynos_ufs_ctrl_hci_core_clk(ufs, true);
#ifdef CONFIG_CPU_IDLE
			exynos_update_ip_idle_status(ufs->idle_ip_index, 1);
#endif
		}
		break;
	default:
		break;
	}
}

static int __exynos_ufs_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	pm_qos_update_request(&ufs->pm_qos_int, 0);

	exynos_ufs_dev_reset_ctrl(ufs, false);

	exynos_ufs_ctrl_phy_pwr(ufs, false);

	return 0;
}

static int __exynos_ufs_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
#if defined(CONFIG_UFS_FMP_DM_CRYPT) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
	int ret;
#endif
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	exynos_ufs_ctrl_phy_pwr(ufs, true);

	if (ufshcd_is_clkgating_allowed(hba))
		clk_prepare_enable(ufs->clk_hci);
	exynos_ufs_ctrl_hci_core_clk(ufs, false);
	exynos_ufs_config_smu(ufs);

#if defined(CONFIG_UFS_FMP_DM_CRYPT) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
	ret = exynos_smc(SMC_CMD_RESUME, 0, UFS_FMP, FMP_DESC_ON);
#else
	ret = exynos_smc(SMC_CMD_RESUME, 0, UFS_FMP, FMP_DESC_OFF);
#endif

	if (ufshcd_is_clkgating_allowed(hba))
		clk_disable_unprepare(ufs->clk_hci);

	if (ret)
		dev_warn(ufs->dev, "failed to smc call for FMP: %x\n", ret);

	return 0;
}

static void exynos_ufs_pm_qos_add(struct exynos_ufs *ufs)
{
	pm_qos_add_request(&ufs->pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);

	if (!ufs->tp_mon_tbl)
		return;

	pm_qos_add_request(&ufs->pm_qos_cluster1, PM_QOS_CLUSTER1_FREQ_MIN, 0);
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	pm_qos_add_request(&ufs->pm_qos_cluster0, PM_QOS_CLUSTER0_FREQ_MIN, 0);
#endif
	pm_qos_add_request(&ufs->pm_qos_mif, PM_QOS_BUS_THROUGHPUT, 0);

	schedule_delayed_work(&ufs->tp_mon, TP_MON_PERIOD);
}

static void exynos_ufs_pm_qos_remove(struct exynos_ufs *ufs)
{
	pm_qos_remove_request(&ufs->pm_qos_int);
	if (!ufs->tp_mon_tbl)
		return;

	cancel_delayed_work_sync(&ufs->tp_mon);
	pm_qos_remove_request(&ufs->pm_qos_cluster1);
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	pm_qos_remove_request(&ufs->pm_qos_cluster0);
#endif
	pm_qos_remove_request(&ufs->pm_qos_mif);
}

static void exynos_ufs_pm_qos_control(struct ufs_hba *hba, u8 pm_enter)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if (!ufs->tp_mon_tbl)
		return;

	if (!pm_enter)
		schedule_delayed_work(&ufs->tp_mon, TP_MON_PERIOD);
	else {
		if (delayed_work_pending(&ufs->tp_mon))
			cancel_delayed_work_sync(&ufs->tp_mon);
		hba->tp_per_period = 0;
	}
}

static void exynos_ufs_tp_mon(struct work_struct *work)
{
	struct exynos_ufs *ufs = container_of(work, struct exynos_ufs,
			tp_mon.work);
	struct ufs_hba *hba = ufs->hba;
	struct exynos_ufs_tp_mon_table *tp_tbl = ufs->tp_mon_tbl;
	s32 cluster1_value = 0;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	s32 cluster0_value = 0;
#endif
	s32 mif_value = 0;

	while (tp_tbl->threshold) {
		if (hba->tp_per_period > tp_tbl->threshold)
			break;
		tp_tbl++;
	}

	if (tp_tbl->threshold) {
		cluster1_value = tp_tbl->cluster1_value;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		cluster0_value = tp_tbl->cluster0_value;
#endif
		mif_value = tp_tbl->mif_value;
	} else {
		cluster1_value = PM_QOS_DEFAULT_VALUE;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		cluster0_value = PM_QOS_DEFAULT_VALUE;
#endif
		mif_value = PM_QOS_DEFAULT_VALUE;
	}

	pm_qos_update_request_timeout(&ufs->pm_qos_cluster1,
					cluster1_value, TP_MON_PM_QOS_LIFETIME);
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	pm_qos_update_request_timeout(&ufs->pm_qos_cluster0,
					cluster0_value, TP_MON_PM_QOS_LIFETIME);
#endif
	pm_qos_update_request_timeout(&ufs->pm_qos_mif,
					mif_value, TP_MON_PM_QOS_LIFETIME);

	hba->tp_per_period = 0;
	schedule_delayed_work(&ufs->tp_mon, TP_MON_PERIOD);
}

static int __exynos_ufs_clk_set_parent(struct device *dev, const char *c, const char *p)
{
	struct clk *_c, *_p;

	_c = devm_clk_get(dev, c);
	if (IS_ERR(_c)) {
		dev_err(dev, "failed to get clock %s\n", c);
		return -EINVAL;
	}

	_p = devm_clk_get(dev, p);
	if (IS_ERR(_p)) {
		dev_err(dev, "failed to get clock %s\n", p);
		return -EINVAL;
	}

	return clk_set_parent(_c, _p);
}

static int exynos_ufs_clk_init(struct device *dev, struct exynos_ufs *ufs)
{
	int ret = 0;

	const char *const clks[] = {
		"mout_sclk_combo_phy_embedded", "top_sclk_phy_fsys1_26m",
	};

	if (!(ufs->opts & EXYNOS_UFS_OPTS_USE_SEPERATED_PCLK))
		ret = __exynos_ufs_clk_set_parent(dev, clks[0], clks[1]);

	if (ret)
		dev_err(dev, "failed to set parent %s of clock %s\n",
				clks[1], clks[0]);

	return ret;
}

static int exynos_ufs_populate_dt_sys(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *np;
	struct exynos_ufs_sys *sys = &ufs->sys;
	struct resource io_res;
	int ret;

	np = of_get_child_by_name(dev->of_node, "ufs-sys");
	if (!np) {
		dev_err(dev, "failed to get ufs-sys node\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(np, 0, &io_res);
	if (ret) {
		dev_err(dev, "failed to get i/o address sysreg\n");
		goto err_0;
	}

	sys->reg_sys = devm_ioremap_resource(dev, &io_res);
	if (!sys->reg_sys) {
		dev_err(dev, "failed to ioremap sysreg\n");
		ret = -ENOMEM;
		goto err_0;
	}
err_0:
	of_node_put(np);

	return ret;
}

static int exynos_ufs_populate_dt_phy(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *ufs_phy, *phy_sys;
	struct exynos_ufs_phy *phy = &ufs->phy;
	struct resource io_res;
	int ret;

	ufs_phy = of_get_child_by_name(dev->of_node, "ufs-phy");
	if (!ufs_phy) {
		dev_err(dev, "failed to get ufs-phy node\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(ufs_phy, 0, &io_res);
	if (ret) {
		dev_err(dev, "failed to get i/o address phy pma\n");
		goto err_0;
	}

	phy->reg_pma = devm_ioremap_resource(dev, &io_res);
	if (!phy->reg_pma) {
		dev_err(dev, "failed to ioremap for phy pma\n");
		ret = -ENOMEM;
		goto err_0;
	}

	phy_sys = of_get_child_by_name(ufs_phy, "ufs-phy-sys");
	if (!phy_sys) {
		dev_err(dev, "failed to get ufs-phy-sys node\n");
		ret = -ENODEV;
		goto err_0;
	}

	ret = of_address_to_resource(phy_sys, 0, &io_res);
	if (ret) {
		dev_err(dev, "failed to get i/o address ufs-phy pmu\n");
		goto err_1;
	}

	phy->reg_pmu = devm_ioremap_resource(dev, &io_res);
	if (!phy->reg_pmu) {
		dev_err(dev, "failed to ioremap for ufs-phy pmu\n");
		ret = -ENOMEM;
	}

	if (exynos_ufs_populate_dt_phy_cfg(dev, &(phy->soc)))
		phy->soc = exynos_ufs_get_drv_data(dev);

err_1:
	of_node_put(phy_sys);
err_0:
	of_node_put(ufs_phy);

	return ret;
}

static int exynos_ufs_get_pwr_mode(struct device_node *np,
				struct exynos_ufs *ufs)
{
	struct uic_pwr_mode *pmd = &ufs->req_pmd_parm;
	const char *str = NULL;

	if (!of_property_read_string(np, "ufs,pmd-attr-mode", &str)) {
		if (!strncmp(str, "FAST", sizeof("FAST")))
			pmd->mode = FAST_MODE;
		else if (!strncmp(str, "SLOW", sizeof("SLOW")))
			pmd->mode = SLOW_MODE;
		else if (!strncmp(str, "FAST_auto", sizeof("FAST_auto")))
			pmd->mode = FASTAUTO_MODE;
		else if (!strncmp(str, "SLOW_auto", sizeof("SLOW_auto")))
			pmd->mode = SLOWAUTO_MODE;
		else
			pmd->mode = FAST_MODE;
	} else {
		pmd->mode = FAST_MODE;
	}

	if (of_property_read_u8(np, "ufs,pmd-attr-lane", &pmd->lane))
		pmd->lane = 1;

	if (of_property_read_u8(np, "ufs,pmd-attr-gear", &pmd->gear))
		pmd->gear = 1;

	if (IS_PWR_MODE_HS(pmd->mode)) {
		if (!of_property_read_string(np, "ufs,pmd-attr-hs-series", &str)) {
			if (!strncmp(str, "HS_rate_b", sizeof("HS_rate_b")))
				pmd->hs_series = PA_HS_MODE_B;
			else if (!strncmp(str, "HS_rate_a", sizeof("HS_rate_a")))
				pmd->hs_series = PA_HS_MODE_A;
			else
				pmd->hs_series = PA_HS_MODE_A;
		} else {
			pmd->hs_series = PA_HS_MODE_A;
		}
	}

	if (of_property_read_u32_array(
		np, "ufs,pmd-local-l2-timer", pmd->local_l2_timer, 3)) {
		pmd->local_l2_timer[0] = FC0PROTTIMEOUTVAL;
		pmd->local_l2_timer[1] = TC0REPLAYTIMEOUTVAL;
		pmd->local_l2_timer[2] = AFC0REQTIMEOUTVAL;
	}

	if (of_property_read_u32_array(
		np, "ufs,pmd-remote-l2-timer", pmd->remote_l2_timer, 3)) {
		pmd->remote_l2_timer[0] = FC0PROTTIMEOUTVAL;
		pmd->remote_l2_timer[1] = TC0REPLAYTIMEOUTVAL;
		pmd->remote_l2_timer[2] = AFC0REQTIMEOUTVAL;
	}

	return 0;
}

static int exynos_ufs_populate_dt(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *np = dev->of_node;
	u32 freq[2];
	u32 tp_mon_depth = 0;
	u32 tp_table_size = 0;
	u32 *tp_mon_tbl;
	u32 i;
	int ret;

	ret = exynos_ufs_populate_dt_phy(dev, ufs);
	if (ret) {
		dev_err(dev, "failed to populate dt-phy\n");
		goto out;
	}

	ret = exynos_ufs_populate_dt_sys(dev, ufs);
	if (ret)
		dev_err(dev, "failed to populate ufs-sys\n");

	ret = of_property_read_u32_array(np,
			"pclk-freq-avail-range",freq, ARRAY_SIZE(freq));
	if (!ret) {
		ufs->pclk_avail_min = freq[0];
		ufs->pclk_avail_max = freq[1];
	} else {
		dev_err(dev, "faild to get available pclk range\n");
		goto out;
	}

	exynos_ufs_get_pwr_mode(np, ufs);

	if (of_find_property(np, "ufs-opts-skip-connection-estab", NULL))
		ufs->opts |= EXYNOS_UFS_OPTS_SKIP_CONNECTION_ESTAB;

	if (of_find_property(np, "ufs-opts-use-seperated-pclk", NULL))
		ufs->opts |= EXYNOS_UFS_OPTS_USE_SEPERATED_PCLK;

	if (of_find_property(np, "ufs-opts-set-line-init-prep-len", NULL))
		ufs->opts |= EXYNOS_UFS_OPTS_SET_LINE_INIT_PREP_LEN;

	if (!of_property_read_u32(np, "ufs-rx-adv-fine-gran-sup_en",
				&ufs->rx_adv_fine_gran_sup_en)) {
		if (ufs->rx_adv_fine_gran_sup_en == 0) {
			/* 100us step */
			if (of_property_read_u32(np,
					"ufs-rx-min-activate-time-cap",
					&ufs->rx_min_actv_time_cap))
				dev_warn(dev,
					"ufs-rx-min-activate-time-cap is empty\n");

			if (of_property_read_u32(np,
					"ufs-rx-hibern8-time-cap",
					&ufs->rx_hibern8_time_cap))
				dev_warn(dev,
					"ufs-rx-hibern8-time-cap is empty\n");

			if (of_property_read_u32(np,
					"ufs-tx-hibern8-time-cap",
					&ufs->tx_hibern8_time_cap))
				dev_warn(dev,
					"ufs-tx-hibern8-time-cap is empty\n");
		} else if (ufs->rx_adv_fine_gran_sup_en == 1) {
			/* fine granularity step */
			if (of_property_read_u32(np,
					"ufs-rx-adv-fine-gran-step",
					&ufs->rx_adv_fine_gran_step))
				dev_warn(dev,
					"ufs-rx-adv-fine-gran-step is empty\n");

			if (of_property_read_u32(np,
					"ufs-rx-adv-min-activate-time-cap",
					&ufs->rx_adv_min_actv_time_cap))
				dev_warn(dev,
					"ufs-rx-adv-min-activate-time-cap is empty\n");

			if (of_property_read_u32(np,
					"ufs-rx-adv-hibern8-time-cap",
					&ufs->rx_adv_hibern8_time_cap))
				dev_warn(dev,
					"ufs-rx-adv-hibern8-time-cap is empty\n");
		} else {
			dev_warn(dev,
				"not supported val for ufs-rx-adv-fine-gran-sup_en %d\n",
				ufs->rx_adv_fine_gran_sup_en);
		}
	} else {
		ufs->rx_adv_fine_gran_sup_en = 0xf;
	}

	if (!of_property_read_u32(np,
				"ufs-pa-granularity", &ufs->pa_granularity)) {
		if (of_property_read_u32(np,
				"ufs-pa-tacctivate", &ufs->pa_tactivate))
			dev_warn(dev, "ufs-pa-tacctivate is empty\n");

		if (of_property_read_u32(np,
				"ufs-pa-hibern8time", &ufs->pa_hibern8time))
			dev_warn(dev, "ufs-pa-hibern8time is empty\n");
	}

	if (of_property_read_u32(np, "ufs-wait-cdr-lock", &ufs->wait_cdr_lock))
		ufs->wait_cdr_lock = 0x5e;

	if (!of_property_read_u32(np,
				"tp_mon_depth", &tp_mon_depth)) {
		tp_mon_tbl = devm_kzalloc(dev,
				(sizeof(struct exynos_ufs_tp_mon_table)
					* tp_mon_depth), GFP_KERNEL);
		if (!tp_mon_tbl) {
			dev_err(dev, "could not allocate memory for tp_mon_tbl\n");
			return -1;
		}
		tp_table_size = (sizeof(struct exynos_ufs_tp_mon_table) /
						sizeof(u32));
		ret = of_property_read_u32_array(np, "tp_mon_table", tp_mon_tbl,
				tp_table_size * tp_mon_depth);
		if (ret == 0) {
			ufs->tp_mon_tbl =
				(struct exynos_ufs_tp_mon_table *) tp_mon_tbl;
			for (i = 0; i < tp_mon_depth; i++) {
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
				dev_info(dev, "TP table info LV %d\n", i);
				dev_info(dev, "threshold : %d CLUSTER1 : %d CLUSTER0 : %d MIF : %d\n",
				ufs->tp_mon_tbl[i].threshold,
				ufs->tp_mon_tbl[i].cluster1_value,
				ufs->tp_mon_tbl[i].cluster0_value,
				ufs->tp_mon_tbl[i].mif_value);
#else
				dev_info(dev, "TP table info LV %d\n", i);
				dev_info(dev, "threshold : %d CLUSTER1 : %d MIF : %d\n",
				ufs->tp_mon_tbl[i].threshold,
				ufs->tp_mon_tbl[i].cluster1_value,
				ufs->tp_mon_tbl[i].mif_value);
#endif
			}
		}
	}

	if (of_property_read_u32(np, "ufs-pm-qos-int", &ufs->pm_qos_int_value))
		ufs->pm_qos_int_value = 0;
out:
	return ret;
}

#ifdef CONFIG_CPU_IDLE
static int exynos_ufs_lp_event(struct notifier_block *nb, unsigned long event, void *data)
{
	struct exynos_ufs *ufs =
		container_of(nb, struct exynos_ufs, lpa_nb);
	const struct exynos_ufs_soc *soc = to_phy_soc(ufs);
	struct ufs_hba *hba = dev_get_drvdata(ufs->dev);
	int ret = NOTIFY_DONE;

	switch (event) {
	case LPA_ENTER:
		WARN_ON(!ufshcd_is_link_hibern8(hba));
		if (ufshcd_is_clkgating_allowed(hba))
			clk_prepare_enable(ufs->clk_hci);
		exynos_ufs_ctrl_hci_core_clk(ufs, false);
		exynos_ufs_ctrl_phy_pwr(ufs, false);

		if (hba->debug.flag & UFSHCD_DEBUG_LEVEL1)
			dev_info(hba->dev, "LPA+\n");
		break;
	case LPA_EXIT:
		exynos_ufs_ctrl_phy_pwr(ufs, true);
		exynos_ufs_ctrl_hci_core_clk(ufs, true);
		if (ufshcd_is_clkgating_allowed(hba))
			clk_disable_unprepare(ufs->clk_hci);

		/*
		 * This condition means that PMA is reset.
		 * So, PMA SFRs should be restored as expected values
		 */
		if (ufshcd_is_clkgating_allowed(hba)) {
			clk_prepare_enable(ufs->clk_hci);
			clk_prepare_enable(ufs->pclk);
		}

		exynos_ufs_gate_clk(ufs, false);
		exynos_ufs_config_phy(ufs, soc->tbl_pma_restore, NULL);
		if (ufshcd_is_clkgating_allowed(hba)) {
			clk_disable_unprepare(ufs->clk_hci);
			clk_disable_unprepare(ufs->pclk);
		}

		if (hba->debug.flag & UFSHCD_DEBUG_LEVEL1)
			dev_info(hba->dev, "LPA-\n");
		break;
	}

	return ret;
}
#endif

static u64 exynos_ufs_dma_mask = DMA_BIT_MASK(32);

static int exynos_ufs_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct exynos_ufs *ufs;
	struct resource *res;
	int ret;

	match = of_match_node(exynos_ufs_match, dev->of_node);
	if (!match)
		return -ENODEV;

	ufs = devm_kzalloc(dev, sizeof(*ufs), GFP_KERNEL);
	if (!ufs) {
		dev_err(dev, "cannot allocate mem for exynos-ufs\n");
		return -ENOMEM;
	}

	/* exynos-specific hci */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	ufs->reg_hci = devm_ioremap_resource(dev, res);
	if (!ufs->reg_hci) {
		dev_err(dev, "cannot ioremap for hci vendor register\n");
		return -ENOMEM;
	}

	/* unipro */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	ufs->reg_unipro = devm_ioremap_resource(dev, res);
	if (!ufs->reg_unipro) {
		dev_err(dev, "cannot ioremap for unipro register\n");
		return -ENOMEM;
	}

	/* ufs protector */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	ufs->reg_ufsp = devm_ioremap_resource(dev, res);
	if (!ufs->reg_ufsp) {
		dev_err(dev, "cannot ioremap for ufs protector register\n");
		return -ENOMEM;
	}

	ret = exynos_ufs_populate_dt(dev, ufs);
	if (ret) {
		dev_err(dev, "failed to get dt info.\n");
		return ret;
	}

	/* PHY power contorl */
	exynos_ufs_ctrl_phy_pwr(ufs, true);

	ret = exynos_ufs_clk_init(dev, ufs);
	if (ret) {
		dev_err(dev, "failed to clock initialization\n");
		return ret;
	}

#ifdef CONFIG_CPU_IDLE
	ufs->lpa_nb.notifier_call = exynos_ufs_lp_event;
	ufs->lpa_nb.next = NULL;
	ufs->lpa_nb.priority = 0;

	ret = exynos_pm_register_notifier(&ufs->lpa_nb);
	if (ret) {
		dev_err(dev, "failed to register low power mode notifier\n");
		return ret;
	}
	ufs->idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev));
	exynos_update_ip_idle_status(ufs->idle_ip_index, 0);
#endif
	ufs->misc_flags = EXYNOS_UFS_MISC_TOGGLE_LOG;

	ufs->dev = dev;
	dev->platform_data = ufs;
	dev->dma_mask = &exynos_ufs_dma_mask;

	INIT_DELAYED_WORK(&ufs->tp_mon, exynos_ufs_tp_mon);
	exynos_ufs_pm_qos_add(ufs);

	return ufshcd_pltfrm_init(pdev, match->data);
}

static int exynos_ufs_remove(struct platform_device *pdev)
{
	struct exynos_ufs *ufs = dev_get_platdata(&pdev->dev);

	exynos_ufs_pm_qos_remove(ufs);

	ufshcd_pltfrm_exit(pdev);

#ifdef CONFIG_CPU_IDLE
	exynos_pm_unregister_notifier(&ufs->lpa_nb);
#endif

	exynos_ufs_ctrl_phy_pwr(ufs, false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_ufs_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	exynos_ufs_pm_qos_control(hba, 1);

	return ufshcd_system_suspend(hba);
}

static int exynos_ufs_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	exynos_ufs_pm_qos_control(hba, 0);

	return ufshcd_system_resume(hba);
}
#else
#define exynos_ufs_suspend	NULL
#define exynos_ufs_resume	NULL
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_RUNTIME
static int exynos_ufs_runtime_suspend(struct device *dev)
{
	return ufshcd_system_suspend(dev_get_drvdata(dev));
}

static int exynos_ufs_runtime_resume(struct device *dev)
{
	return ufshcd_system_resume(dev_get_drvdata(dev));
}

static int exynos_ufs_runtime_idle(struct device *dev)
{
	return ufshcd_runtime_idle(dev_get_drvdata(dev));
}

#else
#define exynos_ufs_runtime_suspend	NULL
#define exynos_ufs_runtime_resume	NULL
#define exynos_ufs_runtime_idle		NULL
#endif /* CONFIG_PM_RUNTIME */

static void exynos_ufs_shutdown(struct platform_device *pdev)
{
	ufshcd_shutdown((struct ufs_hba *)platform_get_drvdata(pdev));
}

static const struct dev_pm_ops exynos_ufs_dev_pm_ops = {
	.suspend		= exynos_ufs_suspend,
	.resume			= exynos_ufs_resume,
	.runtime_suspend	= exynos_ufs_runtime_suspend,
	.runtime_resume		= exynos_ufs_runtime_resume,
	.runtime_idle		= exynos_ufs_runtime_idle,
};

static const struct ufs_hba_variant_ops exynos_ufs_ops = {
	.init = exynos_ufs_init,
	.host_reset = exynos_ufs_host_reset,
	.link_startup_notify = exynos_ufs_link_startup_notify,
	.pwr_change_notify = exynos_ufs_pwr_change_notify,
	.set_nexus_t_xfer_req = exynos_ufs_set_nexus_t_xfer_req,
	.set_nexus_t_task_mgmt = exynos_ufs_set_nexus_t_task_mgmt,
	.hibern8_notify = exynos_ufs_hibern8_notify,
	.clock_control_notify = exynos_ufs_clock_control_notify,
	.get_debug_info = exynos_ufs_get_debug_info,
	.suspend = __exynos_ufs_suspend,
	.resume = __exynos_ufs_resume,
};

static struct ufs_phy_cfg init_cfg[] = {
	{PA_DBG_OPTION_SUITE, 0x30103, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_DBG_AUTOMODE_THLD, 0x222E, PMD_ALL, UNIPRO_DBG_MIB},
	{0x00f, 0xfa, PMD_ALL, PHY_PMA_COMN},
	{0x010, 0x82, PMD_ALL, PHY_PMA_COMN},
	{0x011, 0x1e, PMD_ALL, PHY_PMA_COMN},
	{0x017, 0x84, PMD_ALL, PHY_PMA_COMN},
	{0x035, 0x58, PMD_ALL, PHY_PMA_TRSV},
	{0x036, 0x32, PMD_ALL, PHY_PMA_TRSV},
	{0x037, 0x40, PMD_ALL, PHY_PMA_TRSV},
	{0x03b, 0x83, PMD_ALL, PHY_PMA_TRSV},
	{0x042, 0x88, PMD_ALL, PHY_PMA_TRSV},
	{0x043, 0xa6, PMD_ALL, PHY_PMA_TRSV},
	{0x048, 0x74, PMD_ALL, PHY_PMA_TRSV},
	{0x04c, 0x5b, PMD_ALL, PHY_PMA_TRSV},
	{0x04d, 0x83, PMD_ALL, PHY_PMA_TRSV},
	{0x05c, 0x14, PMD_ALL, PHY_PMA_TRSV},
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x297, 0x17, PMD_ALL, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{},
};


static struct ufs_phy_cfg post_init_cfg[] = {
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x28b, 0x83, PMD_ALL, PHY_PCS_RXTX},
	{0x29a, 0x7, PMD_ALL, PHY_PCS_RXTX},
	{0x277, (200000 / 10) >> 10, PMD_ALL, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{},
};

/* Calibration for PWM mode */
static struct ufs_phy_cfg calib_of_pwm[] = {
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x376, 0x00, PMD_PWM, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{0x04d, 0x03, PMD_PWM, PHY_PMA_COMN},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_SAVECONFIGTIME, 0xbb8, PMD_ALL, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},
	{UNIP_DBG_FORCE_DME_CTRL_STATE, 0x22, PMD_ALL, UNIPRO_DBG_APB},
	{},
};

/* Calibration for HS mode series A */
static struct ufs_phy_cfg calib_of_hs_rate_a[] = {
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x362, 0xff, PMD_HS, PHY_PCS_RXTX},
	{0x363, 0x00, PMD_HS, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{0x00f, 0xfa, PMD_HS, PHY_PMA_COMN},
	{0x010, 0x82, PMD_HS, PHY_PMA_COMN},
	{0x011, 0x1e, PMD_HS, PHY_PMA_COMN},
	/* Setting order: 1st(0x16), 2nd(0x15) */
	{0x016, 0xff, PMD_HS, PHY_PMA_COMN},
	{0x015, 0x80, PMD_HS, PHY_PMA_COMN},
	{0x017, 0x94, PMD_HS, PHY_PMA_COMN},
	{0x036, 0x32, PMD_HS, PHY_PMA_TRSV},
	{0x037, 0x43, PMD_HS, PHY_PMA_TRSV},
	{0x038, 0x3f, PMD_HS, PHY_PMA_TRSV},
	{0x042, 0x88, PMD_HS, PHY_PMA_TRSV},
	{0x043, 0xa6, PMD_HS, PHY_PMA_TRSV},
	{0x048, 0x74, PMD_HS, PHY_PMA_TRSV},
	{0x034, 0x35, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{0x035, 0x5b, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_SAVECONFIGTIME, 0xbb8, PMD_ALL, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},
	{UNIP_DBG_FORCE_DME_CTRL_STATE, 0x22, PMD_ALL, UNIPRO_DBG_APB},
	{},
};

/* Calibration for HS mode series B */
static struct ufs_phy_cfg calib_of_hs_rate_b[] = {
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x362, 0xff, PMD_HS, PHY_PCS_RXTX},
	{0x363, 0x00, PMD_HS, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{0x00f, 0xfa, PMD_HS, PHY_PMA_COMN},
	{0x010, 0x82, PMD_HS, PHY_PMA_COMN},
	{0x011, 0x1e, PMD_HS, PHY_PMA_COMN},
	/* Setting order: 1st(0x16), 2nd(0x15) */
	{0x016, 0xff, PMD_HS, PHY_PMA_COMN},
	{0x015, 0x80, PMD_HS, PHY_PMA_COMN},
	{0x017, 0x94, PMD_HS, PHY_PMA_COMN},
	{0x036, 0x32, PMD_HS, PHY_PMA_TRSV},
	{0x037, 0x43, PMD_HS, PHY_PMA_TRSV},
	{0x038, 0x3f, PMD_HS, PHY_PMA_TRSV},
	{0x042, 0xbb, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{0x043, 0xa6, PMD_HS, PHY_PMA_TRSV},
	{0x048, 0x74, PMD_HS, PHY_PMA_TRSV},
	{0x034, 0x36, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{0x035, 0x5c, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_SAVECONFIGTIME, 0xbb8, PMD_ALL, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},
	{UNIP_DBG_FORCE_DME_CTRL_STATE, 0x22, PMD_ALL, UNIPRO_DBG_APB},
	{},
};

/* Calibration for PWM mode atfer PMC */
static struct ufs_phy_cfg post_calib_of_pwm[] = {
	{PA_DBG_RXPHY_CFGUPDT, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x363, 0x00, PMD_PWM, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{},
};

/* Calibration for HS mode series A atfer PMC */
static struct ufs_phy_cfg post_calib_of_hs_rate_a[] = {
	{PA_DBG_RXPHY_CFGUPDT, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{0x015, 0x00, PMD_HS, PHY_PMA_COMN},
	{0x04d, 0x83, PMD_HS, PHY_PMA_TRSV},
	{0x41a, 0x00, PMD_HS_G3_L1 | PMD_HS_G3_L2, PHY_PCS_COMN},
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x363, 0x00, PMD_HS, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_CONNECTEDTXDATALANES, 1, PMD_HS_G1_L1 | PMD_HS_G2_L1 | PMD_HS_G3_L1, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},
	{},
};

/* Calibration for HS mode series B after PMC*/
static struct ufs_phy_cfg post_calib_of_hs_rate_b[] = {
	{PA_DBG_RXPHY_CFGUPDT, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{0x015, 0x00, PMD_HS, PHY_PMA_COMN},
	{0x04d, 0x83, PMD_HS, PHY_PMA_TRSV},
	{0x41a, 0x00, PMD_HS_G3_L1 | PMD_HS_G3_L2, PHY_PCS_COMN},
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x363, 0x00, PMD_HS, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_CONNECTEDTXDATALANES, 1, PMD_HS_G1_L1 | PMD_HS_G2_L1 | PMD_HS_G3_L1, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},

	{},
};

static struct ufs_phy_cfg pma_restore[] = {
	{},
};

static struct exynos_ufs_soc exynos_ufs_soc_data = {
	.tbl_phy_init			= init_cfg,
	.tbl_post_phy_init		= post_init_cfg,
	.tbl_calib_of_pwm		= calib_of_pwm,
	.tbl_calib_of_hs_rate_a		= calib_of_hs_rate_a,
	.tbl_calib_of_hs_rate_b		= calib_of_hs_rate_b,
	.tbl_post_calib_of_pwm		= post_calib_of_pwm,
	.tbl_post_calib_of_hs_rate_a	= post_calib_of_hs_rate_a,
	.tbl_post_calib_of_hs_rate_b	= post_calib_of_hs_rate_b,
	.tbl_pma_restore		= pma_restore,
};

static const struct ufs_hba_variant exynos_ufs_drv_data = {
	.ops		= &exynos_ufs_ops,
	.quirks		= UFSHCI_QUIRK_BROKEN_DWORD_UTRD |
			  UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR |
			  UFSHCI_QUIRK_USE_ABORT_TASK |
			  UFSHCI_QUIRK_SKIP_INTR_AGGR,
	.vs_data	= &exynos_ufs_soc_data,
};

static const struct of_device_id exynos_ufs_match[] = {
	{ .compatible = "samsung,exynos-ufs",
			.data = &exynos_ufs_drv_data, },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ufs_match);

static struct platform_driver exynos_ufs_driver = {
	.driver = {
		.name = "exynos-ufs",
		.owner = THIS_MODULE,
		.pm = &exynos_ufs_dev_pm_ops,
		.of_match_table = exynos_ufs_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_ufs_probe,
	.remove = exynos_ufs_remove,
	.shutdown = exynos_ufs_shutdown,
};

module_platform_driver(exynos_ufs_driver);
MODULE_DESCRIPTION("Exynos Specific UFSHCI driver");
MODULE_AUTHOR("Seungwon Jeon <tgih.jun@samsung.com>");
MODULE_LICENSE("GPL");
