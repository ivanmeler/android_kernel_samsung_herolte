/*
 * SWD_UpperPacketLayer.c
 * Cypress CapSense FW download upper packet layer implementation.
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

/******************************************************************************
*   Header file Inclusion
******************************************************************************/
#include "SWD_PacketLayer.h"
#include "SWD_UpperPacketLayer.h"

/******************************************************************************
*   Function Definitions
******************************************************************************/

/******************************************************************************
* Function Name: Read_DAP
*******************************************************************************
*
* Summary:
*  Reads the Swd_DAP_Register and stores the returned 32 bit value
*
* Parameters:
*  swd_DAP_Register - MACROS for reading Debug Port and Access Port Register
*  data_32          - 32-bit variable containing the returned value
*
* Return:
*  None
*
******************************************************************************/
void Read_DAP(u8 swd_DAP_Register, u32 * data_32)
{
	swd_PacketHeader = swd_DAP_Register;

	Swd_ReadPacket();
	if (swd_PacketAck != SWD_OK_ACK) {
		return;
	}

	*data_32 =
	    (((u32) swd_PacketData[3] << 24) | ((u32) swd_PacketData[2] << 16) |
	     ((u32) swd_PacketData[1] << 8) | ((u32) swd_PacketData[0]));
}

/******************************************************************************
* Function Name: Write_DAP
*******************************************************************************
*
* Summary:
*  Writes a 32-bit value to swd_DAP_Register
*
* Parameters:
*  swd_DAP_Register - MACROS for writing to Debug Port and Access Port Register
*  data_32          - 32-bit variable containing the value to be written
*
* Return:
*  None
*
******************************************************************************/
void Write_DAP(u8 swd_DAP_Register, u32 data_32)
{
	swd_PacketHeader = swd_DAP_Register;

	swd_PacketData[3] = (u8) (data_32 >> 24);
	swd_PacketData[2] = (u8) (data_32 >> 16);
	swd_PacketData[1] = (u8) (data_32 >> 8);
	swd_PacketData[0] = (u8) (data_32);

	Swd_WritePacket();
}

/******************************************************************************
* Function Name: Read_IO
*******************************************************************************
*
* Summary:
*  Reads 32-bit date from specified address of CPU address space.
*  Returns OK ACK if all SWD transactions succeeds (ACKed).
*
* Parameters:
*  addr_32 - Address from which the data has to be read from CPU address space
*  data_32 - 32-bit variable containing the read value
*
* Return:
*  Swd_PacketAck - Acknowledge packet for the last SWD transaction
*
*******************************************************************************/
u8 Read_IO(u32 addr_32, u32 * data_32)
{
	Write_DAP(DPACC_AP_TAR_WRITE, addr_32);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (swd_PacketAck);
	}

	Read_DAP(DPACC_AP_DRW_READ, data_32);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (swd_PacketAck);
	}

	Read_DAP(DPACC_AP_DRW_READ, data_32);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (swd_PacketAck);
	}
	return (swd_PacketAck);
}

/******************************************************************************
* Function Name: Write_IO
*******************************************************************************
*
* Summary:
*  Writes 32-bit data into specified address of CPU address space.
*  Returns “true” if all SWD transactions succeeded (ACKed).
*
* Parameters:
*  addr_32 - Address at which the data has to be written in CPU address space
*  data_32 - 32-bit variable containing the value to be written
*
* Return:
*  swd_PacketAck - Acknowledge packet for the last SWD transaction
*
******************************************************************************/
u8 Write_IO(u32 addr_32, u32 data_32)
{
	Write_DAP(DPACC_AP_TAR_WRITE, addr_32);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (swd_PacketAck);
	}

	Write_DAP(DPACC_AP_DRW_WRITE, data_32);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (swd_PacketAck);
	}

	return (swd_PacketAck);
}

/* [] END OF FILE */
