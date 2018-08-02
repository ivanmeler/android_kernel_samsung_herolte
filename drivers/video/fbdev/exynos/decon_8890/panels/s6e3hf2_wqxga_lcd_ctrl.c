/*
 * drivers/video/decon/panels/s6e3hf2_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>

#include "../dsim.h"

#include "panel_info.h"

unsigned int s6e3hf2_lcd_type = S6E3HF2_LCDTYPE_WQHD;

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e3hf2_wqxga_aid_dimming.h"
#endif


#ifdef CONFIG_PANEL_AID_DIMMING
//static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {SEQ_HBM_OFF, SEQ_HBM_ON};
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {S6E3HF2_SEQ_ACL_OFF, S6E3HF2_SEQ_ACL_ON};
static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {S6E3HF2_SEQ_ACL_OFF_OPR, S6E3HF2_SEQ_ACL_ON_OPR_8, S6E3HF2_SEQ_ACL_ON_OPR_15};

static const unsigned int br_tbl[EXTEND_BRIGHTNESS + 1] = {
	2, 2, 2, 3,	4, 5, 6, 7,	8,	9,	10,	11,	12,	13,	14,	15,		// 16
	16,	17,	18,	19,	20,	21,	22,	23,	25,	27,	29,	31,	33,	36,   	// 14
	39,	41,	41,	44,	44,	47,	47,	50,	50,	53,	53,	56,	56,	56,		// 14
	60,	60,	60,	64,	64,	64,	68,	68,	68,	72,	72,	72,	72,	77,		// 14
	77,	77,	82,	82,	82,	82,	87,	87,	87,	87,	93,	93,	93,	93,		// 14
	98,	98,	98,	98,	98,	105, 105, 105, 105,	111, 111, 111,		// 12
	111, 111, 111, 119, 119, 119, 119, 119, 126, 126, 126,		// 11
	126, 126, 126, 134, 134, 134, 134, 134,	134, 134, 143,
	143, 143, 143, 143, 143, 152, 152, 152, 152, 152, 152,
	152, 152, 162, 162,	162, 162, 162, 162,	162, 172, 172,
	172, 172, 172, 172,	172, 172, 183, 183, 183, 183, 183,
	183, 183, 183, 183, 195, 195, 195, 195, 195, 195, 195,
	195, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
	220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 234,
	234, 234, 234, 234,	234, 234, 234, 234,	234, 234, 249,
	249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
	265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265,
	265, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
	282, 282, 282, 300, 300, 300, 300, 300,	300, 300, 300,
	300, 300, 300, 300, 316, 316, 316, 316, 316, 316, 316,
	316, 316, 316, 316, 316, 333, 333, 333, 333, 333, 333,
	333, 333, 333, 333, 333, 333, 333,							//7
	[256 ... 262] = 333,
	[263 ... 278] = 360,
	[279 ... 296] = 382,
	[297 ... 315] = 407,
	[316 ... 336] = 433,
	[337 ... 357] = 461,
	[358 ... 376] = 490,
	[377 ... 397] = 516,
	[398 ... 438] = 544,
	[439 ... 439] = 600
};

static const short center_gamma[NUM_VREF][CI_MAX] = {
	{0x000, 0x000, 0x000},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x100, 0x100, 0x100},
};

// aid sheet in rev.D
struct SmtDimInfo daisy_dimming_info_RD[MAX_BR_INFO] = {				// add hbm array
	{.br = 2, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl2nit, .cTbl = RDdctbl2nit, .aid = aid9798, .elvCaps = delvCaps5, .elv = delv5, .way = W1},
	{.br = 3, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl3nit, .cTbl = RDdctbl3nit, .aid = aid9720, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 4, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl4nit, .cTbl = RDdctbl4nit, .aid = aid9612, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 5, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl5nit, .cTbl = RDdctbl5nit, .aid = aid9530, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 6, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl6nit, .cTbl = RDdctbl6nit, .aid = aid9449, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 7, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl7nit, .cTbl = RDdctbl7nit, .aid = aid9359, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 8, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl8nit, .cTbl = RDdctbl8nit, .aid = aid9282, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 9, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl9nit, .cTbl = RDdctbl9nit, .aid = aid9208, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 10, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl10nit, .cTbl = RDdctbl10nit, .aid = aid9115, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 11, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl11nit, .cTbl = RDdctbl11nit, .aid = aid8998, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 12, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl12nit, .cTbl = RDdctbl12nit, .aid = aid8960, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 13, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl13nit, .cTbl = RDdctbl13nit, .aid = aid8894, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 14, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl14nit, .cTbl = RDdctbl14nit, .aid = aid8832, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 15, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl15nit, .cTbl = RDdctbl15nit, .aid = aid8723, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 16, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl16nit, .cTbl = RDdctbl16nit, .aid = aid8653, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 17, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl17nit, .cTbl = RDdctbl17nit, .aid = aid8575, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 19, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl19nit, .cTbl = RDdctbl19nit, .aid = aid8381, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 20, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl20nit, .cTbl = RDdctbl20nit, .aid = aid8292, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 21, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl21nit, .cTbl = RDdctbl21nit, .aid = aid8218, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 22, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl22nit, .cTbl = RDdctbl22nit, .aid = aid8148, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 24, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl24nit, .cTbl = RDdctbl24nit, .aid = aid7966, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 25, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl25nit, .cTbl = RDdctbl25nit, .aid = aid7865, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 27, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl27nit, .cTbl = RDdctbl27nit, .aid = aid7710, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 29, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl29nit, .cTbl = RDdctbl29nit, .aid = aid7512, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 30, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl30nit, .cTbl = RDdctbl30nit, .aid = aid7384, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 32, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl32nit, .cTbl = RDdctbl32nit, .aid = aid7267, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 34, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl34nit, .cTbl = RDdctbl34nit, .aid = aid7089, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 37, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl37nit, .cTbl = RDdctbl37nit, .aid = aid6813, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 39, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl39nit, .cTbl = RDdctbl39nit, .aid = aid6658, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 41, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl41nit, .cTbl = RDdctbl41nit, .aid = aid6467, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 44, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl44nit, .cTbl = RDdctbl44nit, .aid = aid6188, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 47, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl47nit, .cTbl = RDdctbl47nit, .aid = aid5920, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 50, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl50nit, .cTbl = RDdctbl50nit, .aid = aid5637, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 53, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl53nit, .cTbl = RDdctbl53nit, .aid = aid5365, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 56, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl56nit, .cTbl = RDdctbl56nit, .aid = aid5085, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 60, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl60nit, .cTbl = RDdctbl60nit, .aid = aid4732, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 64, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl64nit, .cTbl = RDdctbl64nit, .aid = aid4344, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 68, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl68nit, .cTbl = RDdctbl68nit, .aid = aid3956, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 72, .refBr = 112, .cGma = gma2p15, .rTbl = RDdrtbl72nit, .cTbl = RDdctbl72nit, .aid = aid3637, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 77, .refBr = 120, .cGma = gma2p15, .rTbl = RDdrtbl77nit, .cTbl = RDdctbl77nit, .aid = aid3637, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 82, .refBr = 127, .cGma = gma2p15, .rTbl = RDdrtbl82nit, .cTbl = RDdctbl82nit, .aid = aid3637, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 87, .refBr = 132, .cGma = gma2p15, .rTbl = RDdrtbl87nit, .cTbl = RDdctbl87nit, .aid = aid3637, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 93, .refBr = 142, .cGma = gma2p15, .rTbl = RDdrtbl93nit, .cTbl = RDdctbl93nit, .aid = aid3637, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 98, .refBr = 148, .cGma = gma2p15, .rTbl = RDdrtbl98nit, .cTbl = RDdctbl98nit, .aid = aid3637, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 105, .refBr = 158, .cGma = gma2p15, .rTbl = RDdrtbl105nit, .cTbl = RDdctbl105nit, .aid = aid3637, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 110, .refBr = 165, .cGma = gma2p15, .rTbl = RDdrtbl110nit, .cTbl = RDdctbl110nit, .aid = aid3637, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 119, .refBr = 176, .cGma = gma2p15, .rTbl = RDdrtbl119nit, .cTbl = RDdctbl119nit, .aid = aid3637, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 126, .refBr = 186, .cGma = gma2p15, .rTbl = RDdrtbl126nit, .cTbl = RDdctbl126nit, .aid = aid3637, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 134, .refBr = 196, .cGma = gma2p15, .rTbl = RDdrtbl134nit, .cTbl = RDdctbl134nit, .aid = aid3637, .elvCaps = delvCaps5, .elv = delv5, .way = W1},
	{.br = 143, .refBr = 205, .cGma = gma2p15, .rTbl = RDdrtbl143nit, .cTbl = RDdctbl143nit, .aid = aid3637, .elvCaps = delvCaps6, .elv = delv6, .way = W1},
	{.br = 152, .refBr = 217, .cGma = gma2p15, .rTbl = RDdrtbl152nit, .cTbl = RDdctbl152nit, .aid = aid3637, .elvCaps = delvCaps6, .elv = delv6, .way = W1},
	{.br = 162, .refBr = 228, .cGma = gma2p15, .rTbl = RDdrtbl162nit, .cTbl = RDdctbl162nit, .aid = aid3637, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 172, .refBr = 243, .cGma = gma2p15, .rTbl = RDdrtbl172nit, .cTbl = RDdctbl172nit, .aid = aid3637, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 183, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl183nit, .cTbl = RDdctbl183nit, .aid = aid3637, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 195, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl195nit, .cTbl = RDdctbl195nit, .aid = aid3168, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 207, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl207nit, .cTbl = RDdctbl207nit, .aid = aid2659, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 220, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl220nit, .cTbl = RDdctbl220nit, .aid = aid2186, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 234, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl234nit, .cTbl = RDdctbl234nit, .aid = aid1580, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 249, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl249nit, .cTbl = RDdctbl249nit, .aid = aid1005, .elvCaps = delvCaps9, .elv = delv9, .way = W1},		// 249 ~ 360nit acl off
	{.br = 265, .refBr = 274, .cGma = gma2p15, .rTbl = RDdrtbl265nit, .cTbl = RDdctbl265nit, .aid = aid1005, .elvCaps = delvCaps9, .elv = delv9, .way = W1},
	{.br = 282, .refBr = 286, .cGma = gma2p15, .rTbl = RDdrtbl282nit, .cTbl = RDdctbl282nit, .aid = aid1005, .elvCaps = delvCaps10, .elv = delv10, .way = W1},
	{.br = 300, .refBr = 307, .cGma = gma2p15, .rTbl = RDdrtbl300nit, .cTbl = RDdctbl300nit, .aid = aid1005, .elvCaps = delvCaps10, .elv = delv10, .way = W1},
	{.br = 316, .refBr = 320, .cGma = gma2p15, .rTbl = RDdrtbl316nit, .cTbl = RDdctbl316nit, .aid = aid1005, .elvCaps = delvCaps11, .elv = delv11, .way = W1},
	{.br = 333, .refBr = 339, .cGma = gma2p15, .rTbl = RDdrtbl333nit, .cTbl = RDdctbl333nit, .aid = aid1005, .elvCaps = delvCaps11, .elv = delv11, .way = W1},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W2},
/*hbm interpolation */
	{.br = 382, .refBr = 382, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 407, .refBr = 407, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 433, .refBr = 433, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 461, .refBr = 461, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps13, .elv = delv13, .way = W3},  // hbm is acl on
	{.br = 491, .refBr = 490, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps14, .elv = delv14, .way = W3},  // hbm is acl on
	{.br = 517, .refBr = 516, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps15, .elv = delv15, .way = W3},  // hbm is acl on
	{.br = 545, .refBr = 544, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps16, .elv = delv16, .way = W3},  // hbm is acl on
/* hbm */
	{.br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps17, .elv = delv17, .way = W4},
};

static int set_gamma_to_center(struct SmtDimInfo *brInfo)
{
	int i, j;
	int ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;

	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] = (unsigned char)((center_gamma[i][j] >> 8) & 0x01);
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			} else {
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
		}
	}
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}

static int gammaToVolt255(int gamma)
{
	int ret;

	if (gamma > vreg_element_max[V255]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int voltToGamma(int hbm_volt_table[][3], int* vt, int tp, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	if(tp == V3)
	{
		t1 = DOUBLE_MULTIPLE_VREGOUT - hbm_volt_table[V3][color];
		t2 = DOUBLE_MULTIPLE_VREGOUT - hbm_volt_table[V11][color];
	}
	else
	{
		t1 = vt[color] - hbm_volt_table[tp][color];
		t2 = vt[color] - hbm_volt_table[tp + 1][color];
	}

	temp = ((unsigned long)t1 * (unsigned long)fix_const[tp].de) / (unsigned long)t2;
	ret = temp - fix_const[tp].nu;

	return ret;

}

static int set_gamma_to_hbm(struct SmtDimInfo *brInfo, struct dim_data *dimData, u8 *hbm)
{
	int ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;
	int i, j, idx;
	int temp = 0;
	int voltTableHbm[NUM_VREF][CI_MAX];

	memset(result, 0, OLED_CMD_GAMMA_CNT);

	result[index++] = OLED_CMD_GAMMA;

	memcpy(result+1, hbm, S6E3HF2_HBMGAMMA_LEN);


	memcpy(voltTableHbm[V0], dimData->volt[0], sizeof(voltTableHbm[V0]));
	memcpy(voltTableHbm[V3], dimData->volt[1], sizeof(voltTableHbm[V3]));
	memcpy(voltTableHbm[V11], dimData->volt[10], sizeof(voltTableHbm[V11]));
	memcpy(voltTableHbm[V23], dimData->volt[26], sizeof(voltTableHbm[V23]));
	memcpy(voltTableHbm[V35], dimData->volt[40], sizeof(voltTableHbm[V35]));
	memcpy(voltTableHbm[V51], dimData->volt[60], sizeof(voltTableHbm[V51]));
	memcpy(voltTableHbm[V87], dimData->volt[105], sizeof(voltTableHbm[V87]));
	memcpy(voltTableHbm[V151], dimData->volt[182], sizeof(voltTableHbm[V151]));
	memcpy(voltTableHbm[V203], dimData->volt[238], sizeof(voltTableHbm[V203]));


	idx = 0;
	for(i = CI_RED; i < CI_MAX; i++, idx += 2) {
		temp = (hbm[idx] << 8) | (hbm[idx + 1]);
		voltTableHbm[V255][i] = gammaToVolt255(temp + dimData->mtp[V255][i]);
	}

	idx = 1;
	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				idx += 2;
			} else if(i == V0) {
				idx++;
			} else {
				temp = voltToGamma(voltTableHbm, dimData->volt_vt, i, j) - dimData->mtp[i][j];
				if(temp <= 0)
					temp = 0;
				result[idx] = temp;
				idx ++;
			}
		}
	}

	dsim_info("============ TUNE HBM GAMMA ========== : \n");
	for (i= 0; i < S6E3HF2_HBMGAMMA_LEN; i ++) {
		dsim_info("HBM GAMMA[%d] : %x\n", i, result[i]);
	}

	return ret;
}

/* gamma interpolaion table */
const unsigned int tbl_hbm_inter[7] = {
	94, 201, 311, 431, 559, 670, 789
};


static int interpolation_gamma_to_hbm(struct SmtDimInfo *dimInfo, int br_idx)
{
	int i, j;
	int ret = 0;
	int idx = 0;
	int tmp = 0;
	int hbmcnt, refcnt, gap = 0;
	int ref_idx = 0;
	int hbm_idx = 0;
	int rst = 0;
	int hbm_tmp, ref_tmp;
	unsigned char *result = dimInfo[br_idx].gamma;

	for (i = 0; i < MAX_BR_INFO; i++) {
		if (dimInfo[i].br == S6E3HF2_MAX_BRIGHTNESS)
			ref_idx = i;

		if (dimInfo[i].br == S6E3HF2_HBM_BRIGHTNESS)
			hbm_idx = i;
	}

	if ((ref_idx == 0) || (hbm_idx == 0)) {
		dsim_info("%s failed to get index ref index : %d, hbm index : %d\n",
					__func__, ref_idx, hbm_idx);
		ret = -EINVAL;
		goto exit;
	}

	result[idx++] = OLED_CMD_GAMMA;
	tmp = (br_idx - ref_idx) - 1;

	hbmcnt = 1;
	refcnt = 1;

	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				hbm_tmp = (dimInfo[hbm_idx].gamma[hbmcnt] << 8) | (dimInfo[hbm_idx].gamma[hbmcnt+1]);
				ref_tmp = (dimInfo[ref_idx].gamma[refcnt] << 8) | (dimInfo[ref_idx].gamma[refcnt+1]);

				if (hbm_tmp > ref_tmp) {
					gap = hbm_tmp - ref_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst += ref_tmp;
				}
				else {
					gap = ref_tmp - hbm_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst = ref_tmp - rst;
				}
				result[idx++] = (unsigned char)((rst >> 8) & 0x01);
				result[idx++] = (unsigned char)rst & 0xff;
				hbmcnt += 2;
				refcnt += 2;
			} else {
				hbm_tmp = dimInfo[hbm_idx].gamma[hbmcnt++];
				ref_tmp = dimInfo[ref_idx].gamma[refcnt++];

				if (hbm_tmp > ref_tmp) {
					gap = hbm_tmp - ref_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst += ref_tmp;
				}
				else {
					gap = ref_tmp - hbm_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst = ref_tmp - rst;
				}
				result[idx++] = (unsigned char)rst & 0xff;
			}
		}
	}

	dsim_info("tmp index : %d\n", tmp);

exit:
	return ret;
}

static int init_dimming(struct dsim_device *dsim, u8 *mtp, u8 *hbm)
{
	int i, j;
	int pos = 0;
	int ret = 0;
	short temp;
	int method = 0;
	unsigned char panelrev, paneltype, panelline;
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;
	int string_offset;
	char string_buf[1024];

	dimming = (struct dim_data *)kmalloc(sizeof(struct dim_data), GFP_KERNEL);
	if (!dimming) {
		dsim_err("failed to allocate memory for dim data\n");
		ret = -ENOMEM;
		goto error;
	}

	panelline = panel->id[0] & 0xc0;
	paneltype = panel->id[1] & 0x01;
	panelrev = panel->id[2] & 0x0F;
	dsim_info("%s : Panel Line : %x Panel type : %d Panel rev : %d\n",
			__func__, panelline, paneltype, panelrev);

	diminfo = (void *)daisy_dimming_info_RD;

	panel->dim_data= (void *)dimming;
	panel->dim_info = (void *)diminfo;

	panel->br_tbl = (unsigned int *)br_tbl;
	panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE;
	panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos+1];
		dimming->t_gamma[V255][j] = (int)center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int)center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}
	/* for vt */
	temp = (mtp[pos+1]) << 8 | mtp[pos];

	for(i=0;i<CI_MAX;i++)
		dimming->vt_mtp[i] = (temp >> (i*4)) &0x0f;
#ifdef SMART_DIMMING_DEBUG
	dimm_info("Center Gamma Info : \n");
	for(i=0;i<VMAX;i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE] );
	}
#endif
	dimm_info("VT MTP : \n");
	dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE] );

	dimm_info("MTP Info : \n");
	for(i=0;i<VMAX;i++) {
		dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE] );
	}

	ret = generate_volt_table(dimming);
	if (ret) {
		dimm_err("[ERR:%s] failed to generate volt table\n", __func__);
		goto error;
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;

		if (method == DIMMING_METHOD_FILL_CENTER) {
			ret = set_gamma_to_center(&diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to get center gamma\n", __func__);
				goto error;
			}
		}
		else if (method == DIMMING_METHOD_FILL_HBM) {
			ret = set_gamma_to_hbm(&diminfo[i], dimming, hbm);
			if (ret) {
				dsim_err("%s : failed to get hbm gamma\n", __func__);
				goto error;
			}
		}
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;
		if (method == DIMMING_METHOD_AID) {
			ret = cal_gamma_from_index(dimming, &diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
				goto error;
			}
		}
		if (method == DIMMING_METHOD_INTERPOLATION) {
			ret = interpolation_gamma_to_hbm(diminfo, i);
			if (ret) {
				dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
				goto error;
			}
		}
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		memset(string_buf, 0, sizeof(string_buf));
		string_offset = sprintf(string_buf, "gamma[%3d] : ",diminfo[i].br);

		for(j = 0; j < GAMMA_CMD_CNT; j++)
			string_offset += sprintf(string_buf + string_offset, "%02x ", diminfo[i].gamma[j]);

		dsim_info("%s\n", string_buf);
	}

error:
	return ret;

}

#endif

static int s6e3hf2_read_init_info(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E3HF2_COORDINATE_LEN] = {0,};
	unsigned char buf[S6E3HF2_MTP_DATE_SIZE] = {0, };
	unsigned char hbm_gamma[S6E3HF2_HBMGAMMA_LEN + 1] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	// id
	ret = dsim_read_hl_data(dsim, S6E3HF2_ID_REG, S6E3HF2_ID_LEN, dsim->priv.id);
	if (ret != S6E3HF2_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNECTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6E3HF2_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	// mtp
	ret = dsim_read_hl_data(dsim, S6E3HF2_MTP_ADDR, S6E3HF2_MTP_DATE_SIZE, buf);
	if (ret != S6E3HF2_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E3HF2_MTP_SIZE);
	memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3HF2_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3HF2_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	// coordinate
	ret = dsim_read_hl_data(dsim, S6E3HF2_COORDINATE_REG, S6E3HF2_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3HF2_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim_info("READ coordi : ");
	for(i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");


	// code
	ret = dsim_read_hl_data(dsim, S6E3HF2_CODE_REG, S6E3HF2_CODE_LEN, dsim->priv.code);
	if (ret != S6E3HF2_CODE_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for(i = 0; i < S6E3HF2_CODE_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");

	// elvss
	ret = dsim_read_hl_data(dsim, ELVSS_REG, ELVSS_LEN - 1, dsim->priv.elvss_set);
	if (ret < ELVSS_LEN - 1) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("READ elvss : ");
	for(i = 0; i < ELVSS_LEN - 1; i++)
		dsim_info("%x \n", dsim->priv.elvss_set[i]);

	// hbm gamma
	ret = dsim_read_hl_data(dsim, S6E3HF2_HBMGAMMA_REG, S6E3HF2_HBMGAMMA_LEN + 1, hbm_gamma);
	if (ret != S6E3HF2_HBMGAMMA_LEN + 1) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("HBM Gamma : ");
	memcpy(hbm, hbm_gamma + 1, S6E3HF2_HBMGAMMA_LEN);

	for(i = 1; i < S6E3HF2_HBMGAMMA_LEN + 1; i++)
		dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;

}


static int s6e3hf2_wqhd_dump(struct dsim_device *dsim)
{
	int ret = 0;
	int i;
	unsigned char id[S6E3HF2_ID_LEN];
	unsigned char rddpm[4];
	unsigned char rddsm[4];
	unsigned char err_buf[4];

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
	}

	ret = dsim_read_hl_data(dsim, 0xEA, 3, err_buf);
	if (ret != 3) {
		dsim_err("%s : can't read Panel's EA Reg\n",__func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's 0xEA Reg Value ===\n");
	dsim_info("* 0xEA : buf[0] = %x\n", err_buf[0]);
	dsim_info("* 0xEA : buf[1] = %x\n", err_buf[1]);

	ret = dsim_read_hl_data(dsim, S6E3HF2_RDDPM_ADDR, 3, rddpm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDPM Reg\n",__func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's RDDPM Reg Value : %x ===\n", rddpm[0]);

	if (rddpm[0] & 0x80)
		dsim_info("* Booster Voltage Status : ON\n");
	else
		dsim_info("* Booster Voltage Status : OFF\n");

	if (rddpm[0] & 0x40)
		dsim_info("* Idle Mode : On\n");
	else
		dsim_info("* Idle Mode : OFF\n");

	if (rddpm[0] & 0x20)
		dsim_info("* Partial Mode : On\n");
	else
		dsim_info("* Partial Mode : OFF\n");

	if (rddpm[0] & 0x10)
		dsim_info("* Sleep OUT and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rddpm[0] & 0x08)
		dsim_info("* Normal Mode On and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rddpm[0] & 0x04)
		dsim_info("* Display On and Working Ok\n");
	else
		dsim_info("* Display Off\n");

	ret = dsim_read_hl_data(dsim, S6E3HF2_RDDSM_ADDR, 3, rddsm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDSM Reg\n",__func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's RDDSM Reg Value : %x ===\n", rddsm[0]);

	if (rddsm[0] & 0x80)
		dsim_info("* TE On\n");
	else
		dsim_info("* TE OFF\n");

	if (rddsm[0] & 0x02)
		dsim_info("* S_DSI_ERR : Found\n");

	if (rddsm[0] & 0x01)
		dsim_info("* DSI_ERR : Found\n");

	ret = dsim_read_hl_data(dsim, S6E3HF2_ID_REG, S6E3HF2_ID_LEN, id);
	if (ret != S6E3HF2_ID_LEN) {
		dsim_err("%s : can't read panel id\n",__func__);
		goto dump_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6E3HF2_ID_LEN; i++)
		dsim_info("%02x, ", id[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}
dump_exit:
	return ret;

}
static int s6e3hf2_wqhd_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3HF2_MTP_SIZE] = {0, };
	unsigned char hbm[S6E3HF2_HBMGAMMA_LEN] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	ret = s6e3hf2_read_init_info(dsim, mtp, hbm);
	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

/* test */
/*
	panel->id[0] = 0x40;
	panel->id[1] = 0x11;
	panel->id[2] = 0x96;
*/

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp, hbm);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif

probe_exit:
	return ret;

}


static int s6e3hf2_wqhd_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
 		goto displayon_err;
	}

displayon_err:
	return ret;

}


static int s6e3hf2_wqhd_exit(struct dsim_device *dsim)
{
	int ret = 0;
	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(120);

exit_err:
	return ret;
}

static int s6e3hf2_wqhd_init(struct dsim_device *dsim)
{
	int ret = 0;
	dsim_info("MDD : %s was called\n", __func__);

	/* 7. Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(5);

	/* 9. Interface Setting */
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SINGLE_DSI_1, ARRAY_SIZE(SEQ_SINGLE_DSI_1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_1\n", __func__);
		//goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SINGLE_DSI_2, ARRAY_SIZE(SEQ_SINGLE_DSI_2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_2\n", __func__);
		//goto init_exit;
	}

	msleep(120);

	/* Common Setting */
	ret = dsim_write_hl_data(dsim, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TSP_TE, ARRAY_SIZE(SEQ_TSP_TE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PENTILE_SETTING, ARRAY_SIZE(SEQ_PENTILE_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PENTILE_SETTING\n", __func__);
		goto init_exit;
	}

	/* POC setting */
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	/* PCD setting */
	ret = dsim_write_hl_data(dsim, SEQ_PCD_SETTING, ARRAY_SIZE(SEQ_PCD_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PCD_SETTING\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ERR_FG_SETTING, ARRAY_SIZE(SEQ_ERR_FG_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TE_START_SETTING, ARRAY_SIZE(SEQ_TE_START_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_START_SETTING\n", __func__);
		goto init_exit;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	/* elvss */
	ret = dsim_write_hl_data(dsim, SEQ_TSET_GLOBAL, ARRAY_SIZE(SEQ_TSET_GLOBAL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ELVSS_SET\n", __func__);
		goto init_exit;
	}
	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF2_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HF2_SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF2_SEQ_ACL_OFF_OPR, ARRAY_SIZE(S6E3HF2_SEQ_ACL_OFF_OPR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}
#endif
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}


struct dsim_panel_ops s6e3hf2_panel_ops = {
	.early_probe = NULL,
	.probe		= s6e3hf2_wqhd_probe,
	.displayon	= s6e3hf2_wqhd_displayon,
	.exit		= s6e3hf2_wqhd_exit,
	.init		= s6e3hf2_wqhd_init,
	.dump 		= s6e3hf2_wqhd_dump,
};

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &s6e3hf2_panel_ops;
}

static int __init s6e3hf2_get_lcd_type(char *arg)
{
	unsigned int lcdtype;

	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	return 0;
}
early_param("lcdtype", s6e3hf2_get_lcd_type);

