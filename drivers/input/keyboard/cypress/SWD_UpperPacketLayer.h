/*
 * SWD_UpperPacketLayer.h
 * Cypress CapSense FW download upper packet layer interface.
 * Supported parts include:
 * CY8C40XXX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */


#ifndef __SWD_UPPERPACKETLAYER_H
#define __SWD_UPPERPACKETLAYER_H

#include <linux/slab.h>

/******************************************************************************
*   Constant definitions
******************************************************************************/
/* Macro Definitions for Debug and Access Port registers read and write Commands
   used in the functions defined in UpperPacketLayer.c */
#define DPACC_DP_CTRLSTAT_READ 	0x8D
#define DPACC_DP_IDCODE_READ   	0xA5
#define DPACC_AP_CSW_READ      	0x87
#define DPACC_AP_TAR_READ      	0xAF
#define DPACC_AP_DRW_READ	   	0x9F

#define DPACC_DP_CTRLSTAT_WRITE	0xA9
#define DPACC_DP_SELECT_WRITE   0xB1
#define DPACC_AP_CSW_WRITE      0xA3
#define DPACC_AP_TAR_WRITE  	0x8B
#define DPACC_AP_DRW_WRITE    	0xBB

/******************************************************************************
*   Function Prototypes
******************************************************************************/
void Read_DAP(u8 swd_DAP_Register, u32 *data_32);
void Write_DAP(u8 swd_DAP_Register, u32 data_32);
u8 Read_IO(u32 addr_32, u32 *data_32);
u8 Write_IO(u32 addr_32, u32 data_32);

#endif /* __SWD_UPPERPACKETLAYER_H */


/* [] END OF FILE */
