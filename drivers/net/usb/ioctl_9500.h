#ifndef IOCTL_9500_H_
#define IOCTL_9500_H_

/***************************************************************************

 *

 * Copyright (C) 2008-2009  SMSC

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

 * File: ioctl_500.h

 */


#define SMSC9500_DRIVER_SIGNATURE	(0x82745BACUL+DRIVER_VERSION)

#define SMSC9500_APP_SIGNATURE		(0x987BEF28UL+DRIVER_VERSION)



#define SMSC9500_IOCTL				(SIOCDEVPRIVATE + 0xB)
#define MAX_EEPROM_SIZE 512

enum{
    COMMAND_BASE =	0x974FB832UL,
    COMMAND_GET_SIGNATURE,
    COMMAND_LAN_GET_REG,
    COMMAND_LAN_SET_REG,
    COMMAND_MAC_GET_REG,
    COMMAND_MAC_SET_REG,
    COMMAND_PHY_GET_REG,
    COMMAND_PHY_SET_REG,
    COMMAND_DUMP_LAN_REGS,
    COMMAND_DUMP_MAC_REGS,
    COMMAND_DUMP_PHY_REGS,
    COMMAND_DUMP_EEPROM,
    COMMAND_GET_MAC_ADDRESS,
    COMMAND_SET_MAC_ADDRESS,
    COMMAND_LOAD_MAC_ADDRESS,
    COMMAND_SAVE_MAC_ADDRESS,
    COMMAND_SET_DEBUG_MODE,
    COMMAND_SET_POWER_MODE,
    COMMAND_GET_POWER_MODE,
    COMMAND_SET_LINK_MODE,
    COMMAND_GET_LINK_MODE,
    COMMAND_GET_CONFIGURATION,
    COMMAND_DUMP_TEMP,
    COMMAND_READ_BYTE,
    COMMAND_READ_WORD,
    COMMAND_READ_DWORD,
    COMMAND_WRITE_BYTE,
    COMMAND_WRITE_WORD,
    COMMAND_WRITE_DWORD,
    COMMAND_CHECK_LINK,
    COMMAND_GET_ERRORS,
    COMMAND_SET_EEPROM,
    COMMAND_GET_EEPROM,
    COMMAND_WRITE_EEPROM_FROM_FILE,
    COMMAND_WRITE_EEPROM_TO_FILE,
    COMMAND_VERIFY_EEPROM_WITH_FILE,

//the following codes are intended for cmd9500 only
//  they are not intended to have any use in the driver

    COMMAND_RUN_SERVER,
    COMMAND_RUN_TUNER,
    COMMAND_GET_FLOW_PARAMS,
    COMMAND_SET_FLOW_PARAMS,
    COMMAND_SET_AMDIX_STS,
    COMMAND_GET_AMDIX_STS,
    COMMAND_SET_EEPROM_BUFFER
};

enum{
	LAN_REG_ID_REV,
	LAN_REG_FPGA_REV,
	LAN_REG_INT_STS,
	LAN_REG_RX_CFG,
	LAN_REG_TX_CFG,
	LAN_REG_HW_CFG,
	LAN_REG_RX_FIFO_INF,
	LAN_REG_TX_FIFO_INF,
	LAN_REG_PMT_CTRL,
	LAN_REG_LED_GPIO_CFG,
	LAN_REG_GPIO_CFG,
	LAN_REG_AFC_CFG,
	LAN_REG_E2P_CMD,
	LAN_REG_E2P_DATA,
	LAN_REG_BURST_CAP,
	LAN_REG_STRAP_DBG,
	LAN_REG_DP_SEL,
	LAN_REG_DP_CMD,
	LAN_REG_DP_ADDR,
	LAN_REG_DP_DATA0,
	LAN_REG_DP_DATA1,
	LAN_REG_GPIO_WAKE,
	LAN_REG_INT_EP_CTL,
	LAN_REG_BULK_IN_DLY,
    MAX_LAN_REG_NUM
};

enum{
	MAC_REG_MAC_CR,
	MAC_REG_ADDRH,
	MAC_REG_ADDRL,
	MAC_REG_HASHH,
	MAC_REG_HASHL,
	MAC_REG_MII_ADDR,
	MAC_REG_MII_DATA,
	MAC_REG_FLOW,
	MAC_REG_VLAN1,
	MAC_REG_VLAN2,
	MAC_REG_WUFF,
	MAC_REG_WUCSR,
	MAC_REG_COE_CR,
    MAX_MAC_REG_NUM
};

enum{
	PHY_REG_BCR,
	PHY_REG_BSR,
	PHY_REG_ID1,
	PHY_REG_ID2,
	PHY_REG_ANEG_ADV,
	PHY_REG_ANEG_LPA,
	PHY_REG_ANEG_ER,
	PHY_REG_SILICON_REV,
	PHY_REG_MODE_CTRL_STS,
	PHY_REG_SPECIAL_MODES,
	PHY_REG_TSTCNTL,
	PHY_REG_TSTREAD1,
	PHY_REG_TSTREAD2,
	PHY_REG_TSTWRITE,
	PHY_REG_SPECIAL_CTRL_STS,
	PHY_REG_SITC,
	PHY_REG_INT_SRC,
	PHY_REG_INT_MASK,
	PHY_REG_SPECIAL,
    MAX_PHY_REG_NUM
};


typedef struct _SMSC9500_IOCTL_DATA {

	unsigned long dwSignature;

	unsigned long dwCommand;

	unsigned long Data[526];

	char Strng1[30];

	char Strng2[10];

} SMSC9500_IOCTL_DATA, *PSMSC9500_IOCTL_DATA;


#endif /*IOCTL_9500_H_*/
