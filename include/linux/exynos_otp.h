/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_OTP_H__
#define __EXYNOS_OTP_H__

/* Bank12 magic code */
#define OTP_MAGIC_MIPI_DSI0_M4S4		0x4430		/* ascii: D0 */
#define OTP_MAGIC_MIPI_DSI1_M4S0		0x4431		/* D1 */
#define OTP_MAGIC_MIPI_DSI2_M1S0		0x4432		/* D2 */
#define OTP_MAGIC_MIPI_CSI0		0x4330		/* C0 */
#define OTP_MAGIC_MIPI_CSI1		0x4331		/* C1 */
#define OTP_MAGIC_MIPI_CSI2		0x4332		/* C2 */
#define OTP_MAGIC_MIPI_CSI3		0x4333		/* C3 */
#define OTP_MAGIC_USB3		0x5542		/* UB */
#define OTP_MAGIC_USB2		0x5532		/* U2 */
/* Bank19 magic code */
#define OTP_MAGIC_PCIE_WIFI0		0x5030		/* ascii: P0 */
#define OTP_MAGIC_PCIE_WIFI1		0x5031		/* P1 */
#define OTP_MAGIC_HDMI			0x4844		/* HD */
#define OTP_MAGIC_EDP			0x4450		/* DP */

#define OTP_BANK12_SIZE			128
#define OTP_BANK19_SIZE			128

/* OTP tune bits structure of each IP block */
#define OTP_OFFSET_MAGIC	0	/* bit0 */
#define OTP_OFFSET_TYPE		2	/* bit16 */
#define OTP_OFFSET_COUNT	3	/* bit24 */
#define OTP_OFFSET_INDEX	4	/* bit32 */
#define OTP_OFFSET_DATA		5	/* bit40 */

#define OTP_TYPE_BYTE		0
#define OTP_TYPE_WORD		1

/* Parsed OTP tune bits structure */
struct tune_bits {
	u8	index;		/* SFR addr = base addr + (index * 4) */
	u32	value;		/* if OTP_TYPE is BYTE, only LSB 8bit is valid */
};

int otp_tune_bits_offset(u16 magic, unsigned long *addr);
int otp_tune_bits_parsed(u16 magic, u8 *dt_type, u8 *nr_data, struct tune_bits **data);

#endif /* __EXYNOS_OTP_H__ */
