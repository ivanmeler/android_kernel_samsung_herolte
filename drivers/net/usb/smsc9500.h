/***************************************************************************
 *
 * Copyright (C) 2007-2008  SMSC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ***************************************************************************
 * File: smsc9500.h
 *****************************************************************************/

#ifndef _LAN9500_H
#define _LAN9500_H

typedef unsigned char BYTE;

#define ON TRUE
#define OFF FALSE
#define STATUS_WORD_LEN			4

#define RX_OFFSET
#define LAN_REGISTER_RANGE		0xFF
#define MAC_REGISTER_RANGE_MIN		0x100
#define MAC_REGISTER_RANGE_MAX		0x130

/* product id definition */
#define PID_LAN9500			0x9500
#define PID_LAN9500A			0x9E00
#define PID_LAN9512			0xEC00
#define PID_LAN9530			0x9530
#define PID_LAN9535			0x9535
#define PID_LAN9730			0x9730
#define PID_LAN9735			0x9735
#define PID_LAN89530			0x9E08

/* Tx MAT Cmds */
#define TX_CMD_A_DATA_OFFSET_		(0x001F0000UL)
#define TX_CMD_A_FIRST_SEG_		(0x00002000UL)
#define TX_CMD_A_LAST_SEG_		(0x00001000UL)
#define TX_CMD_A_BUF_SIZE_		(0x000007FFUL)

#define TX_CMD_B_CSUM_ENABLE		(0x00004000UL)
#define TX_CMD_B_ADD_CRC_DISABLE_	(0x00002000UL)
#define TX_CMD_B_DISABLE_PADDING_	(0x00001000UL)
#define TX_CMD_B_PKT_BYTE_LENGTH_	(0x000007FFUL)

/* Rx sts Wd */
#define	RX_STS_FF_			0x40000000UL	// Filter Fail
#define	RX_STS_FL_			0x3FFF0000UL	// Frame Length
#define RX_STS_DL_SHIFT_			16UL
#define	RX_STS_ES_			0x00008000UL	// Error Summary
#define	RX_STS_BF_			0x00002000UL	// Broadcast Frame
#define	RX_STS_LE_			0x00001000UL	// Length Error
#define	RX_STS_RF_			0x00000800UL	// Runt Frame
#define	RX_STS_MF_			0x00000400UL	// Multicast Frame
#define	RX_STS_TL_			0x00000080UL	// Frame too long
#define	RX_STS_CS_			0x00000040UL	// Collision Seen
#define	RX_STS_FT_			0x00000020UL	// Frame Type
#define	RX_STS_RW_			0x00000010UL	// Receive Watchdog
#define	RX_STS_ME_			0x00000008UL	// Mii Error
#define RX_STS_DB_			0x00000004UL	// Dribbling
#define RX_STS_CRC_			0x00000002UL	// CRC Error
#define RX_STS_ERR_MASK_		0x000088DEUL


/* SCSRs */
#define ID_REV				(0x00UL)
	#define ID_REV_CHIP_ID_MASK_   		(0xffff0000UL)
	#define ID_REV_CHIP_REV_MASK_  		(0x0000ffffUL)
	#define ID_REV_CHIP_ID_9500_        	(0x9500UL)
	#define GetChipIdFromID_REV(dwReg)     	((dwReg & ID_REV_CHIP_ID_MASK_) >> 16)
	#define GetChiprevFromID_REV(dwReg)    	(dwReg & ID_REV_CHIP_REV_MASK_)

#define	FPGA_REV			(0x04UL)

#define INT_STS                  	(0x08UL)
	#define INT_STS_TX_STOP_		(0x00020000UL)
	#define INT_STS_RX_STOP_		(0x00010000UL)
	#define INT_STS_PHY_INT_		(0x00008000UL)
	#define INT_STS_TXE_			(0x00004000UL)
	#define INT_STS_TDFU_			(0x00002000UL)
	#define INT_STS_TDFO_			(0x00001000UL)
	#define INT_STS_RXDF_			(0x00000800UL)
	#define INT_STS_GPIOS_	    		(0x000007FFUL)

#define RX_CFG                  	(0x0CUL)
	#define RX_FIFO_FLUSH_			(0x00000001UL)

#define TX_CFG				(0x10UL)
	#define TX_CFG_ON_			(0x00000004UL)
	#define TX_CFG_STOP_			(0x00000002UL)
	#define TX_CFG_FIFO_FLUSH_		(0x00000001UL)

#define HW_CFG                  	(0x14UL)
	#define HW_CFG_SMDET_STS		(0x00040000UL)
	#define HW_CFG_SMDET_EN			(0x00020000UL)
	#define HW_CFG_EEM			(0x00010000UL)
	#define HW_CFG_RST_PROTECT		(0x00008000UL)
	#define HW_CFG_PHY_BOOST		(0x00006000UL)
		#define HW_CFG_PHY_BOOST_NORMAL	(0x00000000UL)
		#define HW_CFG_PHY_BOOST_4	(0x00002000UL)
		#define HW_CFG_PHY_BOOST_8	(0x00004000UL)
		#define HW_CFG_PHY_BOOST_12	(0x00006000UL)
	#define HW_CFG_BIR_			(0x00001000UL)
	#define HW_CFG_LEDB_			(0x00000800UL)
	#define HW_CFG_RXDOFF_			(0x00000600UL)
	#define HW_CFG_RXDOFF_2_		(0x00000400UL)
	#define HW_CFG_SBP_			(0x00000100UL)
	#define HW_CFG_IME_			(0x00000080UL)
	#define HW_CFG_DRP_			(0x00000040UL)
	#define HW_CFG_MEF_			(0x00000020UL)
	#define HW_CFG_ETC_			(0x00000010UL)
	#define HW_CFG_LRST_			(0x00000008UL)
	#define HW_CFG_PSEL_			(0x00000004UL)
	#define HW_CFG_BCE_			(0x00000002UL)
	#define HW_CFG_SRST_			(0x00000001UL)

#define RX_FIFO_INF			(0x18UL)

#define TX_FIFO_INF			(0x1CUL)

#define PM_CTRL				(0x20UL)
	#define PM_CTL_RES_CLR_WKP_STS		(0x00000200UL)
	#define PM_CTL_RES_CLR_WKP_EN		(0x00000100UL)
	#define PM_CTL_DEV_RDY_			(0x00000080UL)
	#define	PM_CTL_SUS_MODE_		(0x00000060UL)
	#define	PM_CTL_SUS_MODE_0		(0x00000000UL)
	#define	PM_CTL_SUS_MODE_1		(0x00000020UL)
	#define	PM_CTL_SUS_MODE_2		(0x00000040UL)
	#define	PM_CTL_SUS_MODE_3		(0x00000060UL)
	#define PM_CTL_PHY_RST_			(0x00000010UL)
	#define PM_CTL_WOL_EN_			(0x00000008UL)
	#define PM_CTL_ED_EN_			(0x00000004UL)
	#define PM_CTL_WUPS_			(0x00000003UL)
	#define PM_CTL_WUPS_NO_			(0x00000000UL)
	#define PM_CTL_WUPS_ED_			(0x00000001UL)
	#define PM_CTL_WUPS_WOL_		(0x00000002UL)
	#define PM_CTL_WUPS_MULTI_		(0x00000003UL)

#define LED_GPIO_CFG			(0x24UL)
	#define LED_GPIO_CFG_LED_SEL_		(0x80000000UL)
	#define LED_GPIO_CFG_GPCTL_10_		(0x03000000UL)
	#define LED_GPIO_CFG_GPCTL_10_SH	24
	#define LED_GPIO_CFG_GPCTL_09_		(0x00300000UL)
	#define LED_GPIO_CFG_GPCTL_09_SH	20
	#define LED_GPIO_CFG_GPCTL_08_		(0x00030000UL)
	#define LED_GPIO_CFG_GPCTL_08_SH	16

	#define LED_GPIO_CFG_GPCTL_DIAG2_	(0x3UL)
	#define LED_GPIO_CFG_GPCTL_DIAG1_	(0x2UL)
	#define LED_GPIO_CFG_GPCTL_LED_		(0x1UL)
	#define LED_GPIO_CFG_GPCTL_GPIO_	(0x0UL)

	#define LED_GPIO_CFG_GPBUF_		(0x00000700UL)
	#define LED_GPIO_CFG_GPBUF_10_		(0x00000400UL)
	#define LED_GPIO_CFG_GPBUF_09_		(0x00000200UL)
	#define LED_GPIO_CFG_GPBUF_08_		(0x00000100UL)
	#define LED_GPIO_CFG_GPDIR_		(0x00000070UL)
	#define LED_GPIO_CFG_GPDIR_10_		(0x00000040UL)
	#define LED_GPIO_CFG_GPDIR_09_		(0x00000020UL)
	#define LED_GPIO_CFG_GPDIR_08_		(0x00000010UL)
	#define LED_GPIO_CFG_GPDATA_		(0x00000007UL)
	#define LED_GPIO_CFG_GPDATA_10_		(0x00000004UL)
	#define LED_GPIO_CFG_GPDATA_09_		(0x00000002UL)
	#define LED_GPIO_CFG_GPDATA_08_		(0x00000001UL)

#define GPIO_CFG			(0x28UL)
	#define GPIO_CFG_GPEN_			(0xFF000000UL)
	#define GPIO_CFG_GPO0_EN_		(0x01000000UL)
	#define GPIO_CFG_GPTYPE_		(0x00FF0000UL)
	#define GPIO_CFG_GPO0_TYPE_		(0x00010000UL)
	#define GPIO_CFG_GPDIR_			(0x0000FF00UL)
	#define GPIO_CFG_GPO0_DIR_		(0x00000100UL)
	#define GPIO_CFG_GPDATA_		(0x000000FFUL)
	#define GPIO_CFG_GPO0_DATA_		(0x00000001UL)

#define AFC_CFG				(0x2CUL)
	#define AFC_CFG_DEFAULT			(0x00f830A1UL)	// Hi watermark = 15.5Kb (~10 mtu pkts)
								// low watermark = 3k (~2 mtu pkts)
								// backpressure duration = ~ 350us
								// Apply FC on any frame.
#define E2P_CMD				(0x30UL)
	#define	E2P_CMD_BUSY_			0x80000000UL
	#define	E2P_CMD_MASK_			0x70000000UL
	#define	E2P_CMD_READ_			0x00000000UL
	#define	E2P_CMD_EWDS_			0x10000000UL
	#define	E2P_CMD_EWEN_			0x20000000UL
	#define	E2P_CMD_WRITE_			0x30000000UL
	#define	E2P_CMD_WRAL_			0x40000000UL
	#define	E2P_CMD_ERASE_			0x50000000UL
	#define	E2P_CMD_ERAL_			0x60000000UL
	#define	E2P_CMD_RELOAD_			0x70000000UL
	#define	E2P_CMD_TIMEOUT_		0x00000400UL
	#define	E2P_CMD_LOADED_			0x00000200UL
	#define	E2P_CMD_ADDR_			0x000001FFUL
#define E2P_DATA			(0x34UL)
	#define	E2P_DATA_MASK_			0x000000FFUL

#define BURST_CAP			(0x38UL)
#define STRAP_DBG			(0x3CUL)
	#define EEPROM_DISABLE			(0x1)
	#define EEPROM_SIZE			(0x4)
#define DP_SEL				(0x40UL)
	#define DP_SEL_DPRDY			(0x80000000UL)
	#define DP_SEL_RSEL			(0x00000006UL)
		#define DP_SEL_RSEL_FCT		(0x00000000UL)
		#define DP_SEL_RSEL_EEPROM	(0x00000002UL)
		#define DP_SEL_RSEL_TXTLI	(0x00000004UL)
		#define DP_SEL_RSEL_RXTLI	(0x00000006UL)
	#define DP_SEL_TESTEN			(0x00000001UL)
#define DP_CMD				(0x44UL)
	#define DP_CMD_READ			(0x0)
	#define DP_CMD_WRITE			(0x1)
#define DP_ADDR 			(0x48UL)
#define DP_DATA0			(0x4CUL)
#define DP_DATA1			(0x50UL)
#define RAM_BIST0			(0x54UL)
#define RAM_BIST1			(0x58UL)
#define RAM_BIST2			(0x5CUL)
#define RAM_BIST3			(0x60UL)
#define GPIO_WAKE			(0x64UL)
#define BULK_IN_DLY			(0x6CUL)

#define INT_EP_CTL			(0x68UL)
	#define	INT_EP_CTL_INTEP_		(0x80000000UL)
	#define	INT_EP_CTL_MACRTO_		(0x00080000UL)
	#define	INT_EP_CTL_RX_FIFO_EN_		(0x00040000UL)
	#define	INT_EP_CTL_TX_STOP_		(0x00020000UL)
	#define	INT_EP_CTL_RX_STOP_		(0x00010000UL)
	#define	INT_EP_CTL_PHY_INT_		(0x00008000UL)
	#define	INT_EP_CTL_TXE_			(0x00004000UL)
	#define	INT_EP_CTL_TDFU_  		(0x00002000UL)
	#define	INT_EP_CTL_TDFO_  		(0x00001000UL)
	#define	INT_EP_CTL_RXDF_  		(0x00000800UL)
	#define	INT_EP_CTL_GPIOS_ 		(0x000007FFUL)

#define FLAG_ATTR			(0xB0UL)
	#define FLAG_ATTR_RMT_WKP		(0x20000UL)
	#define FLAG_ATTR_SELF_PWR		(0x10000UL)
	#define FLAG_ATTR_PME_ENABLE		(0x80UL)
	#define FLAG_ATTR_PME_CFG_PULSE		(0x40UL)
	#define FLAG_ATTR_PME_LEN_150MS		(0x20UL)
	#define FLAG_ATTR_PME_POL		(0x10UL)
	#define FLAG_ATTR_PME_BUFF_TYPE		(0x8UL)
	#define FLAG_ATTR_PME_WAKE_PHY		(0x4UL)
	#define FLAG_ATTR_PME_GPIO10_HIGH	(0x2UL)
	
#define MAC_CR			(0x100UL)
	#define MAC_CR_RXALL_			0x80000000UL
	#define MAC_CR_ENABLE_EEE		0x02000000UL
	#define MAC_CR_RCVOWN_			0x00800000UL
	#define MAC_CR_LOOPBK_			0x00200000UL
	#define MAC_CR_FDPX_			0x00100000UL
	#define MAC_CR_MCPAS_			0x00080000UL
	#define MAC_CR_PRMS_			0x00040000UL
	#define MAC_CR_INVFILT_			0x00020000UL
	#define MAC_CR_PASSBAD_		    	0x00010000UL
	#define MAC_CR_HFILT_			0x00008000UL
	#define MAC_CR_HPFILT_			0x00002000UL
	#define MAC_CR_LCOLL_			0x00001000UL
	#define MAC_CR_BCAST_			0x00000800UL
	#define MAC_CR_DISRTY_			0x00000400UL
	#define MAC_CR_PADSTR_			0x00000100UL
	#define MAC_CR_BOLMT_MASK		0x000000C0UL
	#define MAC_CR_DFCHK_			0x00000020UL
	#define MAC_CR_TXEN_			0x00000008UL
	#define MAC_CR_RXEN_			0x00000004UL
#define ADDRH				(0x104UL)
#define ADDRL				(0x108UL)
#define HASHH				(0x10CUL)
#define HASHL				(0x110UL)

#define MII_ADDR			(0x114UL)
	#define MII_WRITE_			(0x02UL)
	#define MII_BUSY_			(0x01UL)
	#define MII_READ_			(0x00UL) // ~of MII Write bit
#define MII_DATA			(0x118UL)
#define FLOW				(0x11CUL)
	#define FLOW_FCPT_			(0xFFFF0000UL)
	#define FLOW_FCPASS_			(0x00000004UL)
	#define FLOW_FCEN_			(0x00000002UL)
	#define FLOW_FCBSY_			(0x00000001UL)

#define VLAN1				(0x120UL)
#define VLAN2				(0x124UL)
#define WUFF				(0x128UL)
#define WUCSR				(0x12CUL)
	#define WUCSR_GUE_			(0x00000200UL)
	#define WUCSR_WUFR_			(0x00000040UL)
	#define WUCSR_MPR_			(0x00000020UL)
	#define WUCSR_WAKE_EN_			(0x00000004UL)
	#define WUCSR_MPEN_			(0x00000002UL)

#define COE_CR				(0x130UL)
	#define Tx_COE_EN_			(0x00010000UL)
	#define Rx_COE_MODE_			(0x00000002UL)
	#define Rx_COE_EN_			(0x00000001UL)

/* PHY Definitions */
#define LAN_PHY_ADDR			(1UL)

#define PHY_BCR				((u32)0U)
	#define PHY_BCR_RESET_			((u16)0x8000U)
	#define PHY_BCR_LOOPBACK_		((u16)0x4000U)
	#define PHY_BCR_SPEED_SELECT_		((u16)0x2000U)
	#define PHY_BCR_AUTO_NEG_ENABLE_	((u16)0x1000U)
	#define PHY_BCR_POWER_DOWN              ((u16)0x0800U)
	#define PHY_BCR_ISOLATE                	((u16)0x0400U)
	#define PHY_BCR_RESTART_AUTO_NEG_	((u16)0x0200U)
	#define PHY_BCR_DUPLEX_MODE_		((u16)0x0100U)

#define PHY_BSR				((u32)1U)
	#define PHY_BSR_AUTO_NEG_COMP_		((u16)0x0020U)
	#define PHY_BSR_REMOTE_FAULT_		((u16)0x0010U)
	#define PHY_BSR_LINK_STATUS_		((u16)0x0004U)

#define PHY_ID_1			((u32)2U)
#define PHY_ID_2			((u32)3U)

#define PHY_ANEG_ADV			((u32)4U)
	#define PHY_ANEG_NEXT_PAGE		((u16)0x8000)
	#define PHY_ANEG_ADV_PAUSE_		((u16)0x0C00)
	#define PHY_ANEG_ADV_ASYMP_		((u16)0x0800)
	#define PHY_ANEG_ADV_SYMP_		((u16)0x0400)
	#define PHY_ANEG_ADV_SPEED_		((u16)0x1E0)
	#define PHY_ANEG_ADV_100F_		((u16)0x100)
	#define PHY_ANEG_ADV_100H_		((u16)0x80)
	#define PHY_ANEG_ADV_10F_		((u16)0x40)
	#define PHY_ANEG_ADV_10H_		((u16)0x20)

#define PHY_ANEG_LPA			((u32)5U)
#define PHY_ANEG_LPA_ASYMP_			((u16)0x0800)
#define PHY_ANEG_LPA_SYMP_			((u16)0x0400)
#define PHY_ANEG_LPA_T4_			((u16)0x0200)
#define PHY_ANEG_LPA_100FDX_			((u16)0x0100)
#define PHY_ANEG_LPA_100HDX_			((u16)0x0080)
#define PHY_ANEG_LPA_10FDX_			((u16)0x0040)
#define PHY_ANEG_LPA_10HDX_			((u16)0x0020)

#define PHY_ANEG_REG			((u32)6U)
	#define PHY_LP_AN_ABLE			((u16)0x0001)

#define PHY_SILICON_REV			((u32)16U)
	#define PHY_SILICON_REV_TX_NLP_EN_	 BIT_15
	#define PHY_SILICON_REV_TX_NLP_I_MSK_	(BIT_14 | BIT13)
	#define PHY_SILICON_REV_TX_NLP_I_256_	(BIT_14 | BIT13)
	#define PHY_SILICON_REV_TX_NLP_I_512_	BIT_14
	#define PHY_SILICON_REV_TX_NLP_I_768_	BIT_13
	#define PHY_SILICON_REV_TX_NLP_I_1S_	0
	#define PHY_SILICON_REV_RX_1_NLP_	BIT_12
	#define PHY_SILICON_REV_EEE_ENABLE	BIT_2
	#define PHY_SILICON_REV_EDPD_XTD_XOVR	BIT_1
	#define PHY_SILICON_REV_AMDIX_XTD_	BIT_0
	#define EDPD_CFG_DEFAULT		(PHY_SILICON_REV_TX_NLP_EN_ | PHY_SILICON_REV_TX_NLP_I_768_ | PHY_SILICON_REV_RX_1_NLP_)

#define PHY_MODE_CTRL_STS		((u32)17)
	#define MODE_CTRL_STS_FASTRIP_		((u16)0x4000U)
	#define MODE_CTRL_STS_EDPWRDOWN_	((u16)0x2000U)
	#define MODE_CTRL_STS_LOWSQEN_		((u16)0x0800U)
	#define MODE_CTRL_STS_MDPREBP_		((u16)0x0400U)
	#define MODE_CTRL_STS_FARLOOPBACK_	((u16)0x0200U)
	#define MODE_CTRL_STS_FASTEST_		((u16)0x0100U)
	#define MODE_CTRL_STS_REFCLKEN_		((u16)0x0010U)
	#define MODE_CTRL_STS_PHYADBP_		((u16)0x0008U)
	#define MODE_CTRL_STS_FORCE_G_LINK_	((u16)0x0004U)
	#define MODE_CTRL_STS_ENERGYON_		((u16)0x0002U)

#define PHY_SPECIAL_MODES		((u32)18)

#define PHY_TSTCNTL			((u32)20)
	#define PHY_TSTCNTL_READ		((u16)0x8000U)
	#define PHY_TSTCNTL_WRITE		((u16)0x4000U)
	#define PHY_TSTCNTL_TEST_MODE		((u16)0x0400U)
	#define PHY_TSTCNTL_READ_MASK		((u16)0x02E0U)
	#define PHY_TSTCNTL_WRITE_MASK		((u16)0x001FU)

#define PHY_TSTREAD1			((u32)21)	// Testability data Read for LSB
#define PHY_TSTREAD2			((u32)22)	// Testability data Read for MSB
#define PHY_TSTWRITE			((u32)23)	// Testability/Configuration data Write

#define PHY_SPECIAL_CTRL_STS                ((u32)27)
	#define SPECIAL_CTRL_STS_OVRRD_AMDIX_   ((u16)0x8000U)
	#define SPECIAL_CTRL_STS_AMDIX_ENABLE_  ((u16)0x4000U)
	#define SPECIAL_CTRL_STS_AMDIX_STATE_   ((u16)0x2000U)

#define PHY_SITC			((u32)28) 

#define PHY_INT_SRC			((u32)29)
	#define PHY_INT_SRC_ENERGY_ON_		((u16)0x0080U)
	#define PHY_INT_SRC_ANEG_COMP_		((u16)0x0040U)
	#define PHY_INT_SRC_REMOTE_FAULT_	((u16)0x0020U)
	#define PHY_INT_SRC_LINK_DOWN_		((u16)0x0010U)

#define PHY_INT_MASK			((u32)30)
	#define PHY_INT_MASK_ALL		((u16)0x00FFU)
	#define PHY_INT_MASK_ENERGY_ON_		((u16)0x0080U)
	#define PHY_INT_MASK_ANEG_COMP_		((u16)0x0040U)
	#define PHY_INT_MASK_REMOTE_FAULT_	((u16)0x0020U)
	#define PHY_INT_MASK_LINK_DOWN_		((u16)0x0010U)

#define PHY_SPECIAL			((u32)31)
	#define PHY_SPECIAL_SPD_		((u16)0x001CU)
	#define PHY_SPECIAL_SPD_10HALF_		((u16)0x0004U)
	#define PHY_SPECIAL_SPD_10FULL_		((u16)0x0014U)
	#define PHY_SPECIAL_SPD_100HALF_	((u16)0x0008U)
	#define PHY_SPECIAL_SPD_100FULL_	((u16)0x0018U)

#define AMDIX_DISABLE_STRAIGHT		((u16)0x0U)
#define AMDIX_DISABLE_CROSSOVER		((u16)0x01U)
#define AMDIX_ENABLE			((u16)0x02U)

#define PHY_LAN83C180_INT_ACK_REG	0x15
#define PHY_LAN83C180_OPTIONS		0x18

#define PHY_LAN83C183_STATUS_OUTPUT	0x12
#define PHY_LAN83C183_INT_MASK		0x13
#define PHY_LAN83C183_MASK_BASE		0xFFC0
#define PHY_LAN83C183_MASK_INT		~0x8000
#define PHY_LAN83C183_MASK_LINK		~0x4000

#define PHY_FOX_INT_SOURCE		0x1D
#define PHY_FOX_INT_ENERGY_ON		BIT_7
#define PHY_FOX_INT_NWAY_DONE		BIT_6
#define PHY_FOX_INT_REMOTE_FAULT	BIT_5
#define PHY_FOX_INT_LINK_DOWN		BIT_4
#define PHY_FOX_INT_NWAY_LP_ACK		BIT_3
#define PHY_FOX_INT_PARALLEL_DET	BIT_2
#define PHY_FOX_INT_NWAY_PAGE_RX	BIT_1
#define PHY_FOX_INT_RESERVED		BIT_0

#define PHY_FOX_INT_MASK		0x1E

#define PHY_QSI_INT_SOURCE_REG		0x1D 
#define PHY_QSI_AN_COMPLETE_INT    	BIT_6
#define	PHY_QSI_REM_FAULT_INT       	BIT_5
#define	PHY_QSI_LINK_DOWN_INT       	BIT_4
#define	PHY_QSI_AN_LP_ACK_INT       	BIT_3
#define	PHY_QSI_PAR_DET_INT         	BIT_2
#define	PHY_QSI_AN_PAGE_RCVD_INT    	BIT_1
#define	PHY_QSI_RCV_ERR_CNT_INT     	BIT_0
#define PHY_QSI_INT_MASK_REG		0x1E
#define PHY_QSI_INT_MODE		BIT_15

/* Bit Mask definitions  */
#define BIT_NONE			0x00000000
#define BIT_0				0x0001
#define BIT_1				0x0002
#define BIT_2				0x0004
#define BIT_3				0x0008
#define BIT_4				0x0010
#define BIT_5				0x0020
#define BIT_6				0x0040
#define BIT_7				0x0080
#define BIT_8				0x0100
#define BIT_9				0x0200
#define BIT_10				0x0400
#define BIT_11				0x0800
#define BIT_12				0x1000
#define BIT_13				0x2000
#define BIT_14				0x4000
#define BIT_15				0x8000
#define BIT_16				0x10000
#define BIT_17				0x20000
#define BIT_18				0x40000
#define BIT_19				0x80000
#define BIT_20				0x100000
#define BIT_21				0x200000
#define BIT_22				0x400000
#define BIT_23				0x800000
#define BIT_24				0x1000000
#define BIT_25				0x2000000
#define BIT_26				0x4000000
#define BIT_27				0x8000000
#define BIT_28				0x10000000
#define BIT_29				0x20000000
#define BIT_30				0x40000000
#define BIT_31				0x80000000

/* Vendor Requests */
#define	USB_VENDOR_REQUEST_WRITE_REGISTER	0xA0
#define	USB_VENDOR_REQUEST_READ_REGISTER	0xA1
#define	USB_VENDOR_REQUEST_GET_STATS		0xA2

#define rx_stat_count			8
#define tx_stat_count			10

/* Stats block structures */
typedef struct _SMSC9500_RX_STATS {
	u32	RxGoodFrames;
	u32	RxCrcErrors;
	u32	RxRuntFrameErrors;
	u32	RxAlignmentErrors;
	u32	RxFrameTooLongError;
	u32	RxLaterCollisionError;
	u32	RxBadFrames;
	u32	RxFifoDroppedFrames;
	u32     EeeRxLpiTransitions;
	u32     EeeRxLpiTime;
}SMSC9500_RX_STATS, *pSMSC9500_RX_STATS;

typedef struct _SMSC9500_TX_STATS {
	u32	TxGoodFrames;
	u32	TxPauseFrames;
	u32	TxSingleCollisions;
	u32	TxMultipleCollisions;
	u32	TxExcessiveCollisionErrors;
	u32	TxLateCollisionErrors;
	u32	TxBufferUnderrunErrors;
	u32	TxExcessiveDeferralErrors;
	u32	TxCarrierErrors;
	u32	TxBadFrames;
	u32     EeeTxLpiTransitions;
	u32     EeeTxLpiTime;
} SMSC9500_TX_STATS, *pSMSC9500_TX_STATS;

/* Interrupt Endpoint status word bitfields */
#define	INT_ENP_RFHF_			(0x00040000UL)
#define	INT_ENP_TX_STOP_		ETH_INT_STS_TX_STOP_
#define	INT_ENP_RX_STOP_		ETH_INT_STS_RX_STOP_
#define	INT_ENP_PHY_INT_		ETH_INT_STS_PHY_INT_
#define	INT_ENP_TXE_			ETH_INT_STS_TXE_
#define	INT_ENP_TDFU_			ETH_INT_STS_TDFU_
#define	INT_ENP_TDFO_			ETH_INT_STS_TDFO_
#define	INT_ENP_RXDF_			ETH_INT_STS_RXDF_
#define	INT_ENP_GPIOS_			ETH_INT_STS_GPIOS_

struct USB_CONTEXT{
	struct usb_ctrlrequest req;
	struct completion notify;
};

enum{
	ASYNC_RW_SUCCESS,
	ASYNC_RW_FAIL,
	ASYNC_RW_TIMEOUT,
};

#define USE_DEBUG
#ifdef USE_DEBUG
#define USE_WARNING
#define USE_TRACE
#define USE_ASSERT
#endif //USE_DEBUG

#define HIBYTE(word)  ((BYTE)(((u16)(word))>>8))
#define LOBYTE(word)  ((BYTE)(((u16)(word))&0x00FFU))
#define HIWORD(dWord) ((u16)(((u32)(dWord))>>16))
#define LOWORD(dWord) ((u16)(((u32)(dWord))&0x0000FFFFUL))

/*******************************************************
* Macro: SMSC_TRACE
* Description:
*    This macro is used like printf.
*    It can be used anywhere you want to display information
*    For any release version it should not be left in
*      performance sensitive Tx and Rx code paths.
*    To use this macro define USE_TRACE and set bit 0 of debug_mode
*******************************************************/
#ifdef USING_LINT
extern void SMSC_TRACE(unsigned long dbgBit, const char * a, ...);
#else //not USING_LINT
#ifdef USE_TRACE
extern u32 debug_mode;
#ifndef USE_WARNING
#define USE_WARNING
#endif
#   define SMSC_TRACE(dbgBit,msg,args...)   \
    if(debug_mode&dbgBit) {                 \
        printk("SMSC_9500: " msg "\n", ## args);    \
    }
#else
#   define SMSC_TRACE(dbgBit,msg,args...)
#endif
#endif //end of not USING_LINT

/*
* Macro: SMSC_WARNING
* Description:
*    This macro is used like printf.
*    It can be used anywhere you want to display warning information
*    For any release version it should not be left in
*      performance sensitive Tx and Rx code paths.
*    To use this macro define USE_TRACE or
*      USE_WARNING and set bit 1 of debug_mode
*/
#ifdef USING_LINT
extern void SMSC_WARNING(const char * a, ...);
#else //not USING_LINT
#ifdef USE_WARNING
extern u32 debug_mode;
#ifndef USE_ASSERT
#define USE_ASSERT
#endif
#   define SMSC_WARNING(msg, args...)               \
    if(debug_mode&DBG_WARNING) {                    \
        printk("SMSC_9500_WARNING: ");\
        printk(__FUNCTION__);\
        printk(": " msg "\n",## args);\
    }
#else
#   define SMSC_WARNING(msg, args...)
#endif
#endif //end of not USING_LINT

/*
* Macro: SMSC_ASSERT
* Description:
*    This macro is used to test assumptions made when coding.
*    It can be used anywhere, but is intended only for situations
*      where a failure is fatal.
*    If code execution where allowed to continue it is assumed that
*      only further unrecoverable errors would occur and so this macro
*      includes an infinite loop to prevent further corruption.
*    Assertions are only intended for use during developement to
*      insure consistency of logic through out the driver.
*    A driver should not be released if assertion failures are
*      still occuring.
*    To use this macro define USE_TRACE or USE_WARNING or
*      USE_ASSERT
*/
#ifdef USING_LINT
extern void SMSC_ASSERT(BOOLEAN condition);
#else //not USING_LINT
#ifdef USE_ASSERT
    #define SMSC_ASSERT(condition) \
    if(!(condition)) { \
        printk("SMSC_9500_ASSERTION_FAILURE: \n"); \
        printk("    Condition = " #condition "\n"); \
        printk("    Function  = ");printk(__FUNCTION__);printk("\n"); \
        printk("    File      = " __FILE__ "\n"); \
        printk("    Line      = %d\n",__LINE__); \
        while(1); \
    }
#else
#define SMSC_ASSERT(condition)
#endif
#endif //end of not USING_LINT

#define LINK_INIT			(0xFFFF)
#define LINK_OFF			(0x00UL)
#define LINK_SPEED_10HD			(0x01UL)
#define LINK_SPEED_10FD			(0x02UL)
#define LINK_SPEED_100HD		(0x04UL)
#define LINK_SPEED_100FD		(0x08UL)
#define LINK_SYMMETRIC_PAUSE		(0x10UL)
#define LINK_ASYMMETRIC_PAUSE		(0x20UL)
#define LINK_AUTO_NEGOTIATE		(0x40UL)

#define INT_END_MACRTO_INT_		(0x00080000UL)
#define INT_END_RXFIFO_HAS_FRAME_	(0x00040000UL)
#define INT_END_TXSTOP_INT_		(0x00020000UL)
#define INT_END_RXSTOP_INT_		(0x00010000UL)
#define INT_END_PHY_INT_		(0x00008000UL)
#define INT_END_TXE_			(0x00004000UL)
#define INT_END_TDFU_			(0x00002000UL)
#define INT_END_TDFO_			(0x00001000UL)
#define INT_END_RXDF_INT_		(0x00000800UL)
#define INT_END_GPIO_INT_		(0x000007FFUL)

#define DBG_TRACE			(0x01UL)
#define DBG_WARNING			(0x02UL)
#define DBG_INIT			(0x80000000UL)
#define DBG_CLOSE			(0x40000000UL)
#define DBG_INTR			(0x20000000UL)
#define DBG_PWR				(0x10000000UL)
#define DBG_IOCTL			(0x08000000UL)
#define DBG_LINK			(0x04000000UL)
#define DBG_RX				(0x02000000UL)
#define DBG_TX				(0x01000000UL)
#define DBG_MCAST			(0x00800000UL)
#define DBG_HOST			(0x00400000UL)
#define DBG_LINK_CHANGE			(0x00200000UL)

#define DEFAULT_BULK_IN_DELAY		(0x00002000UL)
#define ETH_HEADER_SIZE			14
#define ETH_MAX_DATA_SIZE		1500
#define ETH_MAX_PACKET_SIZE		(ETH_MAX_DATA_SIZE + ETH_HEADER_SIZE)
#define LAN9500_EEPROM_MAGIC		0x9500UL
#define EEPROM_MAC_OFFSET		0x01

struct smsc9500_int_data {
    u32 IntEndPoint;
} __attribute__ ((packed));

#define MAX_WUFF_NUM			8
#define LAN9500_WUFF_NUM		4
#define LAN9500A_WUFF_NUM		8

typedef struct _WAKEUP_FILTER {
    u32 dwFilterMask[MAX_WUFF_NUM*4];
    u32 dwCommad[MAX_WUFF_NUM/4];
    u32 dwOffset[MAX_WUFF_NUM/4];
    u32 dwCRC[MAX_WUFF_NUM/2];
} WAKEUP_FILTER, *PWAKEUP_FILTER;

static struct {
	const char str[ETH_GSTRING_LEN];
} ethtool_stats_keys[] = {
	{ "rx_packets"},
	{ "tx_packets"},
	{ "rx_bytes"},
	{ "tx_bytes"},
	{ "rx_errors"},
	{ "tx_errors"},
	{ "tx_dropped"},
	{ "rx_no_buffer_count"},
	{ "rx_length_errors"},
	{ "rx_over_errors"},
	{ "rx_crc_errors"},
	{ "rx_frame_errors"},
	{ "RxGoodFrames"},
	{ "RxCrcErrors"},
	{ "RxRuntFrameErrors"},
	{ "RxAlignmentErrors"},
	{ "RxFrameTooLongError"},
	{ "RxLaterCollisionError"},
	{ "RxBadFrames"},
	{ "RxFifoDroppedFrames"},
	{ "TxGoodFrames"},
	{ "TxPauseFrames"},
	{ "TxSingleCollisions"},
	{ "TxMultipleCollisions"},
	{ "TxExcessiveCollisionErrors"},
	{ "TxLateCollisionErrors"},
	{ "TxBufferUnderrunErrors"},
	{ "TxExcessiveDeferralErrors"},
	{ "TxCarrierErrors"},
	{ "TxBadFrames"},
};

typedef struct _ADAPTER_DATA {
    u32 dwIdRev;
    u32 dwPhyAddress;
    u32 dwPhyId;
    u32 dwFpgaRev;
    u32 macAddrHi16;	//Mac address high 16 bits
    u32 MmacAddrLo32;   //Mac address Low 32 bits
    BOOLEAN internalPhy;

    BOOLEAN UseTxCsum;
    BOOLEAN UseRxCsum;

    BOOLEAN LanInitialized;
    BYTE bPhyModel;
    BYTE bPhyRev;
    u32 dwLinkSpeed;
    u32 dwLinkSettings;
    u32 dwSavedLinkSettings;
    struct semaphore phy_mutex;		// Mutex for PHY access
    struct semaphore eeprom_mutex;	//Mutex for eeprom access
    struct semaphore internal_ram_mutex;//Mutex for internal ram operation
    struct semaphore RxFilterLock;

    u16 wLastADV;
    u16 wLastADVatRestart;

    spinlock_t TxQueueLock;
    BOOLEAN TxInitialized;
    u32 dwTxQueueDisableMask;
    BOOLEAN TxQueueDisabled;

    u32 WolWakeupOpts;
    u32 wakeupOptsBackup;
    u32   dwWUFF[20];

    /* for Rx Multicast work around */
    volatile u32 HashLo;
    volatile u32 HashHi;
    volatile BOOLEAN MulticastUpdatePending;
    volatile u32 set_bits_mask;
    volatile u32 clear_bits_mask;

    u32 LinkLedOnGpio;		//Gpio port number for link led
    u32 LinkLedOnGpioPolarity;  //Gpio Output polarity
    u32 LinkActLedCfg;		//
    u32 LinkLedOnGpioBufType;

    u32 dynamicSuspendPHYEvent; //Store PHY interrupt source for dynamic suspend
    u32 systemSuspendPHYEvent; //Store PHY interrupt source for system suspend

    u16 eepromSize;
    BOOLEAN eepromContentValid;

	 // Count of transmit errors & other stats.
	u64	TxGoodFrames;
	u32	TxPauseFrames;
	u32	TxSingleCollisions;
	u32	TxMultipleCollisions;
	u32	TxExcessiveCollisionErrors;
	u32	TxLateCollisionErrors;
	u32	TxBufferUnderrunErrors;
	u32	TxExcessiveDeferralErrors;
	u32	TxCarrierErrors;
	u32	TxBadFrames;
	u32     EeeTxLpiTransitions;
	u32     EeeTxLpiTime;

	// Count of receive errors
	u64	RxGoodFrames;
	u32	RxCrcErrors;
	u32	RxRuntFrameErrors;
	u32	RxAlignmentErrors;
	u32	RxFrameTooLongError;
	u32	RxLaterCollisionError;
	u32	RxBadFrames;
	u32	RxFifoDroppedFrames;
	u32     EeeRxLpiTransitions;
	u32     EeeRxLpiTime;
	
	SMSC9500_RX_STATS rx_statistics;
	SMSC9500_TX_STATS tx_statistics;
	
} ADAPTER_DATA, *PADAPTER_DATA;

typedef struct _MAC_ADDR_IN_RAM{
	u32 signature;
	u32 MacAddrL;
	u32 MacAddrH;
	u16 crc;
	u16 crcComplement;
}MAC_ADDR_IN_RAM;

enum{
	RAMSEL_FCT,
	RAMSEL_EEPROM,
	RAMSEL_TXTLI,
	RAMSEL_RXTLI
};

#ifndef min
#define    min(_a, _b)      (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef max
#define    max(_a, _b)      (((_a) > (_b)) ? (_a) : (_b))
#endif

#define BCAST_LEN   6
#define MCAST_LEN   3
#define ARP_LEN     2

#endif    // _LAN9500_H



