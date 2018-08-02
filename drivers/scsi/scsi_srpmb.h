/*
 * Secure RPMB header for Exynos scsi rpmb
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _SCSI_SRPMB_H
#define _SCSI_SRPMB_H

#define GET_WRITE_COUNTER			1
#define WRITE_DATA				2
#define READ_DATA				3

#define AUTHEN_KEY_PROGRAM_RES			0x0100
#define AUTHEN_KEY_PROGRAM_REQ			0x0001
#define RESULT_READ_REQ				0x0005
#define RPMB_END_ADDRESS			0x4000

#define RPMB_PACKET_SIZE 			512

#define WRITE_COUTNER_DATA_LEN_ERROR		0x601
#define WRITE_COUTNER_SECURITY_OUT_ERROR	0x602
#define WRITE_COUTNER_SECURITY_IN_ERROR		0x603
#define WRITE_DATA_LEN_ERROR			0x604
#define WRITE_DATA_SECURITY_OUT_ERROR		0x605
#define WRITE_DATA_RESULT_SECURITY_OUT_ERROR	0x606
#define WRITE_DATA_SECURITY_IN_ERROR		0x607
#define READ_LEN_ERROR				0x608
#define READ_DATA_SECURITY_OUT_ERROR		0x609
#define READ_DATA_SECURITY_IN_ERROR		0x60A

#define PASS_STATUS 				0xBABA

#define IS_INCLUDE_RPMB_DEVICE			"0:0:0:1"

#define ON					1
#define OFF					0

#define RPMB_BUF_MAX_SIZE			64 * 1024

/* Interrupt SPI number */
#if defined(CONFIG_SOC_EXYNOS8890)
#define RPMB_SPI				254
#define GICD_RPMB_SET				0x40000000
#define RPMB_PENDING_OFFSET			0x29C
#define GICD_ISPENDR_BASE			0x11000000
#endif

struct rpmb_irq_ctx {
	struct device *dev;
	u8 *vir_addr;
	dma_addr_t phy_addr;
	volatile u8 __iomem *srpmb_pendr;
	struct work_struct work;
	struct workqueue_struct *srpmb_queue;
};

struct rpmb_packet {
       u16     request;
       u16     result;
       u16     count;
       u16     address;
       u32     write_counter;
       u8      nonce[16];
       u8      data[256];
       u8      Key_MAC[32];
       u8      stuff[196];
};

#endif /* _SCSI_SRPMB_H */
