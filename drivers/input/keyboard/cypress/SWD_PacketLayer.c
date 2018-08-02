/*
 * SWD_PacketLayer.c
 * Cypress CapSense FW download packet layer implementation.
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

/******************************************************************************
*   Physical Layer
******************************************************************************/
/* "SWD_PhysicalLayer.h" file contains the bit banging routines for
   programming */
//#include "SWD_PhysicalLayer.h"

#include <linux/gpio.h>
#define SetSwdckHigh() 			gpio_set_value(physical.swdck_gpio, 1)
#define SetSwdckLow()  			gpio_set_value(physical.swdck_gpio, 0)

#define SetSwdioCmosOutput()    gpio_direction_output(physical.swdio_gpio, 0)
#define SetSwdioHizInput()		gpio_direction_input(physical.swdio_gpio)
#define SetSwdioHigh() 			gpio_set_value(physical.swdio_gpio, 1)
#define SetSwdioLow() 			gpio_set_value(physical.swdio_gpio, 0)
#define ReadSwdio()				gpio_get_value(physical.swdio_gpio)

#define SWDIO_OUTPUT_HIGH      SetSwdioHigh()
#define SWDIO_OUTPUT_LOW       SetSwdioLow()

#define SWDCK_OUTPUT_HIGH      SetSwdckHigh()
#define SWDCK_OUTPUT_LOW       SetSwdckLow()

#define XRES_OUTPUT_HIGH
#define XRES_OUTPUT_LOW

static struct {
	int swdio_gpio;
	int swdck_gpio;
} physical;

void Swd_configPhysical(int swdio_gpio, int swdck_gpio)
{
	physical.swdio_gpio = swdio_gpio;
	physical.swdck_gpio = swdck_gpio;
}

/******************************************************************************
*   Global Variable definitions
******************************************************************************/
/* Stores the 8-bit SWD Packet header */
u8 swd_PacketHeader;

/* Stores the 3-bit SWD Packet ACK data */
u8 swd_PacketAck;

/* 4-byte Read, Write packet data */
u8 swd_PacketData[DATA_BYTES_PER_PACKET];

/******************************************************************************
*   Function Definitions
******************************************************************************/

/******************************************************************************
* Function Name: Swd_SendByte
*******************************************************************************
*
* Summary:
*  Sends a byte of data on the SWD lines (SWDIO, SWDCK)
*
* Parameters:
*  txByte: Data byte to be sent by host on SWDIO line (Least significant bit
*		   sent first)
*
* Return:
*  None.
*
* Note:
*  This function is called for sending SWD header data in SWD Read/Write
*  packets, and also to send data in a SWD Write packet.
*
******************************************************************************/
static void Swd_SendByte(u8 txByte)
{
	u8 loop = 0;

	/* Loop for 8-bits of a byte */
	for (loop = 0; loop < 8; loop++) {
		if (txByte & LSB_BIT_MASK) {	/* Send a '1' */
			SetSwdckLow();
			SetSwdioHigh();
			SetSwdckHigh();
		} else {	/* Send a '0' */

			SetSwdckLow();
			SetSwdioLow();
			SetSwdckHigh();
		}

		txByte = txByte >> 1;	/* Make the next bit to send as LS bit */
	}
}

/******************************************************************************
* Function Name: Swd_ReceiveByte
*******************************************************************************
*
* Summary:
*  Receives a byte of data on the SWD lines (SWDIO, SWDCK)
*
* Parameters:
*  None.
*
* Return:
*  rxByte: Data byte received by host on SWDIO line (Least significant bit is
*          first received bit)
*
* Note:
*  This function is called for reading data bytes in a SWD Read Packet
*
******************************************************************************/
static u8 Swd_ReceiveByte(void)
{
	u8 loop = 0;
	u8 rxBit = 0;
	u8 rxByte = 0;

	/* Loop for 8-bits of a byte */
	for (loop = 0; loop < 8; loop++) {
		SWDCK_OUTPUT_LOW;

		/* Read the SWDIO input line */
		rxBit = ReadSwdio();

		SWDCK_OUTPUT_HIGH;

		rxByte = rxByte >> 1;

		if (rxBit) {	/* Received a '1' */
			rxByte = rxByte | MSB_BIT_MASK;
		} else {	/* Received a '0' */

			rxByte = rxByte & (~MSB_BIT_MASK);
		}

	}

	return (rxByte);
}

/******************************************************************************
* Function Name: Swd_FirstTurnAroundPhase
*******************************************************************************
*
* Summary:
*  Generates one SWDCK clock cycle for Turnaround phase of a SWD packet.
*  SWDIO drive mode is changed to High-Z drive mode so that the host can read
*  the data on the SWDIO.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Note:
*  This function is called during SWD Read/Write packets
*
******************************************************************************/
static void Swd_FirstTurnAroundPhase(void)
{
	/* Change to High-Z drive mode for host to read the SWDIO line */
	SetSwdioHizInput();
	SetSwdckLow();
	SetSwdckHigh();
}

/******************************************************************************
* Function Name: Swd_SecondTurnAroundPhase
*******************************************************************************
*
* Summary:
*  Generates one SWDCK clock cycle for Turnaround phase of a SWD packet.
*  SWDIO drive mode is changed to CMOS output drive mode so that the host can
*  write data on SWDIO.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Note:
*  This function is called during SWD Read/Write packets
*
******************************************************************************/
static void Swd_SecondTurnAroundPhase(void)
{
	/* Change to CMOS output drive mode for host to write to SWDIO */
	SetSwdioCmosOutput();
	SetSwdckLow();
	SetSwdckHigh();
}

/******************************************************************************
* Function Name: Swd_GetAckSegment
********************************************************************************
*
* Summary:
*  Gets the 3-bit ACK response in a SWD packet
*
* Parameters:
*  None.
*
* Return:
*  ack:
*   3-bit ACK response is returned as a byte. Three possible return values are:
*       0x01 - SWD_OK_ACK
*       0x02 - SWD_WAIT_ACK
*       0x04 - SWD_FAULT_ACK
*       Any other return value - Undefined ACK code.
*       Treat it similar to SWD_FAULT_ACK and abort operation.
*
* Note:
*  This function is called during SWD Read/Write packets
*
******************************************************************************/
static u8 Swd_GetAckSegment(void)
{
	u8 ack = 0;
	u8 receiveBit = 0;
	u8 loop = 0;

	/* ACK bits are received lsb bit first */
	for (loop = 0; loop < NUMBER_OF_ACK_BITS; loop++) {
		SetSwdckLow();

		/* Store the ACK bit received */
		receiveBit = ReadSwdio();
		SetSwdckHigh();

		/* Concatenate the ACK bit with ACK data byte */
		ack = ack | (receiveBit << loop);
	}

	return (ack);
}

/******************************************************************************
* Function Name: Swd_SendParity
*******************************************************************************
*
* Summary:
*  Sends the 1-bit parity data on SWD lines
*
* Parameters:
*  parity:
*    1-bit parity data received as a byte (either '1' or '0')
*
* Return:
*  None.
*
* Note:
*  This function is called during SWD Write packet
*
******************************************************************************/
static void Swd_SendParity(u8 parity)
{
	/* Make the clock low, Send SWDIO data, Make Clock high */

	SetSwdckLow();
	if (parity) {
		SetSwdioHigh();
	} else {
		SetSwdioLow();
	}
	SetSwdckHigh();
}

/******************************************************************************
* Function Name: Swd_ReceiveParity
*******************************************************************************
*
* Summary:
*  Receives the 1-bit parity data on SWD lines
*
* Parameters:
*  None.
*
* Return:
*  parity:
*    1-bit parity data returned as a byte (either '1' or '0')
*
* Note:
*  This function is called during SWD Read packet
*
******************************************************************************/
static u8 Swd_ReceiveParity(void)
{
	u8 parity;

	/* Make the clock low, Read SWDIO data, Make Clock high */

	SetSwdckLow();
	parity = ReadSwdio();
	SetSwdckHigh();

	return (parity);
}

/******************************************************************************
* Function Name: Swd_SendDummyClocks
*******************************************************************************
*
* Summary:
*  Sends three SWDCK clock cycles with SWDIO line held low
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Note:
*  This function is called during SWD Read/Write packets.
*
******************************************************************************/
static void Swd_SendDummyClocks(void)
{
	u8 loop;

	/* Send three SWDCK clock cycles with SWDIO line held low */
	SetSwdioLow();
	for (loop = 0; loop < NUMBER_OF_DUMMY_SWD_CLOCK_CYCLES; loop++) {
		SetSwdckLow();
		SetSwdckHigh();
	}
}

/******************************************************************************
* Function Name: Swd_CountOneBits
*******************************************************************************
*
* Summary:
*  Counts the number of 1's in a byte
*
* Parameters:
*  dataByte:
*   Byte for which the number of 1's need to be found
*
* Return:
*  count:
*   Number of 1's present in DataByte
*
* Note:
*  This function is used for computing parity of data
*
******************************************************************************/
static u8 Swd_CountOneBits(u8 dataByte)
{
	u8 count = 0;
	u8 i;

	/* Loop for 8-bits of a byte */
	/* Increment count variable if LS bit is set */
	for (i = 0; i < 8; i++) {
		if (dataByte & LSB_BIT_MASK) {
			count++;
		}

		dataByte = dataByte >> 1;
	}

	return (count);
}

/******************************************************************************
* Function Name: Swd_ComputeDataParity
*******************************************************************************
*
* Summary:
*  Computes Even parity  of 4-byte data
*
* Parameters:
*  swd_PacketData[DATA_BYTES_PER_PACKET] - Global variable that holds the data
*
* Return:
*   Even parity bit
*   0x01 - Parity is '1' if 4-byte data has an odd number of 1's
*   0x00 - Parity is '0' if 4-byte data has an even number of 1's
*
* Note:
*  This function is called during SWD Read/Write packets
*
******************************************************************************/
static u8 Swd_ComputeDataParity(void)
{
	u8 count = 0;
	u8 i;

	/* Count number of 1's in 4-byte data */
	for (i = 0; i < DATA_BYTES_PER_PACKET; i++) {
		count = count + Swd_CountOneBits(swd_PacketData[i]);
	}

	/* Return even parity. If Lsb bit is 1, it implies the number of 1's is odd,
	   and hence the even parity bit is set. For even number of 1's, parity bit
	   is 0 */
	return (count & LSB_BIT_MASK);
}

/******************************************************************************
* Function Name: Swd_ReadPacket
*******************************************************************************
*
* Summary:
*  Sends a single SWD Read packet
*
* Parameters:
* swd_PacketHeader - Global variable that holds the 8-bit header data for packet
*
* Return:
*  swd_PacketData[DATA_BYTES_PER_PACKET] - Global variable that holds the 4-byte
*										   data that has been read
*
*  swd_PacketAck - Global variable that holds the status of the SWD transaction
*    ACK response is stored as a byte. Three possible ACK return values are:
*       0x01 - SWD_OK_ACK
*       0x02 - SWD_WAIT_ACK
*       0x04 - SWD_FAULT_ACK
*       0x09 - SWD_OK_ACK, but Parity error in data received
*       Any other ACK value - Undefined ACK code.
*                             Treat it similar to SWD_FAULT_ACK and abort
*							  operation.
*
* Note:
*  1.)Swd_RawReadPacket() is called during SPC Polling operations when the host
*	  must read the SPC status register continuously till a Programming operation
*	  is completed or data is ready to be read.
*
*  2.)This function is called duing continuous multi byte read operations from
*	  the SPC_DATA register
*
*  3.)This function is called during IDCODE instruction for reading the Device
*	  JTAG ID
*
*  4.)This function is not called during normal SWD read transactions. To read
*     a register data, two SWD Read packets must be sent (call this function
*     twice). This method of reading twice has been implemented in
*	  SWD_ReadPacket() function which will be called during normal read
*	  transactions.
*
******************************************************************************/
void Swd_ReadPacket(void)
{
	u8 parity;
	u8 loop = 0;
	u8 i;

	do {
		/* 8-bit Header data */
		Swd_SendByte(swd_PacketHeader);

		/* First Turnaround phase */
		Swd_FirstTurnAroundPhase();

		/* Get the 3-bit ACK data */
		swd_PacketAck = Swd_GetAckSegment();

		/* Read 4-byte data and store in Global array swd_PacketData[] */
		for (i = 0; i < DATA_BYTES_PER_PACKET; i++) {
			swd_PacketData[i] = Swd_ReceiveByte();
		}

		/* Parity phase */
		parity = Swd_ReceiveParity();

		/* Second Turnaround phase */
		Swd_SecondTurnAroundPhase();

		/* Dummy clock phase since clock is not free running */
		Swd_SendDummyClocks();

		/* Repeat for a WAIT ACK till timeout loop expires */
		loop++;

	} while ((swd_PacketAck == SWD_WAIT_ACK)
		 && (loop < NUMBER_OF_WAIT_ACK_LOOPS));

	/* For a OK ACK, check the parity bit received with parity computed */
	if (swd_PacketAck == SWD_OK_ACK) {
		if (parity != Swd_ComputeDataParity()) {
			/* Set the Parity error bit in ACK code */
			swd_PacketAck = swd_PacketAck | SWD_PARITY_ERROR;
		}
	}

	/* Swd_PacketAck global variable holds the status of the SWD transaction */
}

/******************************************************************************
* Function Name: Swd_WritePacket
*******************************************************************************
*
* Summary:
*  Sends a single SWD Write packet
*
* Parameters:
*  swd_PacketHeader - Global variable that holds the 8-bit header data for packet
*  swd_PacketData[DATA_BYTES_PER_PACKET] - Global variable that holds the data
*										   to be sent
*
* Return:
*  swd_PacketAck - Global variable that holds the status of the SWD transaction
*    ACK response is stored as a byte. Three possible ACK return values are:
*       0x01 - SWD_OK_ACK
*       0x02 - SWD_WAIT_ACK
*       0x04 - SWD_FAULT_ACK
*       Any other ACK value - Undefined ACK code.
*       Treat it similar to SWD_FAULT_ACK and abort operation.
*
* Note:
*  This function is called for Address Write, Data Write operations
*
******************************************************************************/
void Swd_WritePacket(void)
{
	u8 parity;
	u8 loop = 0;
	u8 i;

	/* Compute Even parity for 4-byte data */
	parity = Swd_ComputeDataParity();

	do {
		/* 8-bit Header data */
		Swd_SendByte(swd_PacketHeader);

		/* First Turnaround phase */
		Swd_FirstTurnAroundPhase();

		/* Get the 3-bit ACK data */
		swd_PacketAck = Swd_GetAckSegment();

		/* Second Turnaround phase */
		Swd_SecondTurnAroundPhase();

		/* Send 4-byte data stored in Global array swd_PacketData[] */
		for (i = 0; i < DATA_BYTES_PER_PACKET; i++) {
			Swd_SendByte(swd_PacketData[i]);
		}

		/* Parity phase */
		Swd_SendParity(parity);

		/* Dummy clock phase since clock is not free running */
		Swd_SendDummyClocks();

		loop++;

		/* Repeat for a WAIT ACK till timeout loop expires */

	} while ((swd_PacketAck == SWD_WAIT_ACK)
		 && (loop < NUMBER_OF_WAIT_ACK_LOOPS));

	/* swd_PacketAck global variable holds the status of the SWD transaction */
}

/******************************************************************************
* Function Name: Swd_LineReset
*******************************************************************************
*
* Summary:
*  Resets the SWD line by sending 51 SWDCK clock cycles with SWDIO line high
*
* Parameters:
*  None
*
* Return:
*  None
*
* Note:
*  This function is called as part of SWD switching sequence
*
******************************************************************************/
void Swd_LineReset(void)
{
	u8 i;

	/* To reset SWD line, clock more than 50 SWDCK clock cycles with SWDIO
	   high. */

	SWDIO_OUTPUT_HIGH;

	for (i = 0; i < NUMBER_OF_SWD_RESET_CLOCK_CYCLES; i++) {
		SWDCK_OUTPUT_LOW;
		SWDCK_OUTPUT_HIGH;
	}
	SWDCK_OUTPUT_LOW;
	SWDIO_OUTPUT_LOW;
	SWDCK_OUTPUT_HIGH;
}

/* []END OF FILE */
