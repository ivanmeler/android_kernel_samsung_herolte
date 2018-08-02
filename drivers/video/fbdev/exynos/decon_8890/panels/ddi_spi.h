/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : ddi_spi.h

	Description : header of SPI interface

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	History :
	----------------------------------------------------------------------
*******************************************************************************/
#ifndef __DDI_SPI__
#define __DDI_SPI__

#ifdef __cplusplus
extern "C" {
#endif

static const unsigned char SEQ_B0_13[] = {
	0xB0,
	0x13
};

static const unsigned char SEQ_D8_02[] = {
	0xD8,
	0x02
};

static const unsigned char SEQ_B0_1F[] = {
	0xB0,
	0x1F
};

static const unsigned char SEQ_FE_06[] = {
	0xFE,
	0x06
};

//extern void ddi_spi_read_opr_start(void);
//extern void ddi_spi_read_opr_end(void);
int* ddi_spi_read_opr(void);


#ifdef __cplusplus
}
#endif

#endif /* __DDI_SPI__ */

