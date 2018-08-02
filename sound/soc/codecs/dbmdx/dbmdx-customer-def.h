/*
 * dbmdx-customer.h  --  DBMDX customer definitions
 *
 * Copyright (C) 2014 DSP Group
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMDX_CUSTOMER_DEF_H
#define _DBMDX_CUSTOMER_DEF_H

#define DBMD2_VA_FIRMWARE_NAME			"dbmd2_va_fw.bin"
#define DBMD2_VQE_FIRMWARE_NAME			"dbmd2_vqe_fw.bin"
#define DBMD2_VQE_OVERLAY_FIRMWARE_NAME		"dbmd2_vqe_overlay_fw.bin"

#define DBMD4_VA_FIRMWARE_NAME			"dbmd4_va_fw.bin"

#define DBMDX_VT_GRAM_NAME			"voice_grammar.bin"
#define DBMDX_VT_NET_NAME			"voice_net.bin"

#define DBMDX_VC_GRAM_NAME			"vc_grammar.bin"
#define DBMDX_VC_NET_NAME			"vc_net.bin"


/* ================ Defines related to kernel vesion ===============*/



#define USE_ALSA_API_3_10_XX	0




/* ==================================================================*/

/* ================ Custom Configuration ===============*/



#define DBMDX_DEFER_IF_SND_CARD_ID_0 1




/* ==================================================================*/

#endif
