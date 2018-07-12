/*
 * drivers/video/decon/panels/s6e3fa2_lcd_ctrl.c
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

unsigned int s6e3fa2_lcd_type = S6E3FA2_LCDTYPE_FHD;

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e3fa2_fhd_aid_dimming.h"
#endif


static unsigned char SEQ_PARTIAL_AREA[] = {
	0x30,
	0x00, 0x00, 0x00, 0x00
};


#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_15};
static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {SEQ_ACL_OFF_OPR_AVR, SEQ_ACL_ON_OPR_AVR};

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
	333, 333, 333, 333, 333, 333, 350,
	[256 ... 281] = 382,
	[282 ... 295] = 407,
	[296 ... 309] = 433,
	[310 ... 323] = 461,
	[324 ... 336] = 491,
	[337 ... 350] = 517,
	[351 ... 364] = 545,
	[365 ... 365] = 600
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

struct SmtDimInfo tulip_dimming_info[MAX_BR_INFO] = {				// add hbm array
	{.br =	2, .refBr = 114, .cGma = gma2p15, .rTbl =  rtbl2nit, .cTbl =  ctbl2nit, .aid = aid9793, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br =	3, .refBr = 114, .cGma = gma2p15, .rTbl =  rtbl3nit, .cTbl =  ctbl3nit, .aid = aid9711, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br =	4, .refBr = 114, .cGma = gma2p15, .rTbl =  rtbl4nit, .cTbl =  ctbl4nit, .aid = aid9633, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br =	5, .refBr = 114, .cGma = gma2p15, .rTbl =  rtbl5nit, .cTbl =  ctbl5nit, .aid = aid9551, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br =	6, .refBr = 114, .cGma = gma2p15, .rTbl =  rtbl6nit, .cTbl =  ctbl6nit, .aid = aid9468, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br =	7, .refBr = 114, .cGma = gma2p15, .rTbl =  rtbl7nit, .cTbl =  ctbl7nit, .aid = aid9385, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br =	8, .refBr = 114, .cGma = gma2p15, .rTbl =  rtbl8nit, .cTbl =  ctbl8nit, .aid = aid9303, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br =	9, .refBr = 114, .cGma = gma2p15, .rTbl =  rtbl9nit, .cTbl =  ctbl9nit, .aid = aid9215, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 10, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl10nit, .cTbl = ctbl10nit, .aid = aid9127, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 11, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl11nit, .cTbl = ctbl11nit, .aid = aid9044, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 12, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl12nit, .cTbl = ctbl12nit, .aid = aid8967, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 13, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl13nit, .cTbl = ctbl13nit, .aid = aid8879, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 14, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl14nit, .cTbl = ctbl14nit, .aid = aid8786, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 15, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl15nit, .cTbl = ctbl15nit, .aid = aid8704, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 16, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl16nit, .cTbl = ctbl16nit, .aid = aid8611, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 17, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl17nit, .cTbl = ctbl17nit, .aid = aid8528, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 19, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl19nit, .cTbl = ctbl19nit, .aid = aid8363, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 20, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl20nit, .cTbl = ctbl20nit, .aid = aid8177, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 21, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl21nit, .cTbl = ctbl21nit, .aid = aid8135, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 22, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl22nit, .cTbl = ctbl22nit, .aid = aid8053, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 24, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl24nit, .cTbl = ctbl24nit, .aid = aid7929, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 25, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl25nit, .cTbl = ctbl25nit, .aid = aid7846, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 27, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl27nit, .cTbl = ctbl27nit, .aid = aid7665, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 29, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl29nit, .cTbl = ctbl29nit, .aid = aid7495, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 30, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl30nit, .cTbl = ctbl30nit, .aid = aid7397, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 32, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl32nit, .cTbl = ctbl32nit, .aid = aid7221, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 34, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl34nit, .cTbl = ctbl34nit, .aid = aid7040, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 37, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl37nit, .cTbl = ctbl37nit, .aid = aid6761, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 39, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl39nit, .cTbl = ctbl39nit, .aid = aid6586, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 41, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl41nit, .cTbl = ctbl41nit, .aid = aid6374, .elvCaps = elv0C, .elv = elv0C, .way = W1},
	{.br = 44, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl44nit, .cTbl = ctbl44nit, .aid = aid6105, .elvCaps = elv0C, .elv = elv0C, .way = W1},
	{.br = 47, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl47nit, .cTbl = ctbl47nit, .aid = aid5795, .elvCaps = elv0D, .elv = elv0D, .way = W1},
	{.br = 50, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl50nit, .cTbl = ctbl50nit, .aid = aid5511, .elvCaps = elv0D, .elv = elv0D, .way = W1},
	{.br = 53, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl53nit, .cTbl = ctbl53nit, .aid = aid5191, .elvCaps = elv0E, .elv = elv0E, .way = W1},
	{.br = 56, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl56nit, .cTbl = ctbl56nit, .aid = aid4861, .elvCaps = elv0F, .elv = elv0F, .way = W1},
	{.br = 60, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl60nit, .cTbl = ctbl60nit, .aid = aid4411, .elvCaps = elv10, .elv = elv10, .way = W1},
	{.br = 64, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl64nit, .cTbl = ctbl64nit, .aid = aid4179, .elvCaps = elv10, .elv = elv10, .way = W1},
	{.br = 68, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl68nit, .cTbl = ctbl68nit, .aid = aid4179, .elvCaps = elv10, .elv = elv10, .way = W1},
	{.br = 72, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl72nit, .cTbl = ctbl72nit, .aid = aid4179, .elvCaps = elv10, .elv = elv10, .way = W1},
	{.br = 77, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl77nit, .cTbl = ctbl77nit, .aid = aid4179, .elvCaps = elv10, .elv = elv10, .way = W1},
	{.br = 82, .refBr = 120, .cGma = gma2p15, .rTbl = rtbl82nit, .cTbl = ctbl82nit, .aid = aid4179, .elvCaps = elv0F, .elv = elv0F, .way = W1},
	{.br = 87, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl87nit, .cTbl = ctbl87nit, .aid = aid4179, .elvCaps = elv0F, .elv = elv0F, .way = W1},
	{.br = 93, .refBr = 137, .cGma = gma2p15, .rTbl = rtbl93nit, .cTbl = ctbl93nit, .aid = aid4179, .elvCaps = elv0F, .elv = elv0F, .way = W1},
	{.br = 98, .refBr = 143, .cGma = gma2p15, .rTbl = rtbl98nit, .cTbl = ctbl98nit, .aid = aid4179, .elvCaps = elv0E, .elv = elv0E, .way = W1},
	{.br = 105, .refBr = 153, .cGma = gma2p15, .rTbl = rtbl105nit, .cTbl = ctbl105nit, .aid = aid4179, .elvCaps = elv0E, .elv = elv0E, .way = W1},
	{.br = 110, .refBr = 162, .cGma = gma2p15, .rTbl = rtbl111nit, .cTbl = ctbl111nit, .aid = aid4179, .elvCaps = elv0D, .elv = elv0D, .way = W1},
	{.br = 119, .refBr = 171, .cGma = gma2p15, .rTbl = rtbl119nit, .cTbl = ctbl119nit, .aid = aid4179, .elvCaps = elv0D, .elv = elv0D, .way = W1},
	{.br = 126, .refBr = 179, .cGma = gma2p15, .rTbl = rtbl126nit, .cTbl = ctbl126nit, .aid = aid4179, .elvCaps = elv0C, .elv = elv0C, .way = W1},
	{.br = 134, .refBr = 190, .cGma = gma2p15, .rTbl = rtbl134nit, .cTbl = ctbl134nit, .aid = aid4179, .elvCaps = elv0C, .elv = elv0C, .way = W1},
	{.br = 143, .refBr = 202, .cGma = gma2p15, .rTbl = rtbl143nit, .cTbl = ctbl143nit, .aid = aid4179, .elvCaps = elv0B, .elv = elv0B, .way = W1},
	{.br = 152, .refBr = 213, .cGma = gma2p15, .rTbl = rtbl152nit, .cTbl = ctbl152nit, .aid = aid4179, .elvCaps = elv0A, .elv = elv0A, .way = W1},
	{.br = 162, .refBr = 228, .cGma = gma2p15, .rTbl = rtbl162nit, .cTbl = ctbl162nit, .aid = aid3626, .elvCaps = elv09, .elv = elv09, .way = W1},
	{.br = 172, .refBr = 238, .cGma = gma2p15, .rTbl = rtbl172nit, .cTbl = ctbl172nit, .aid = aid3177, .elvCaps = elv09, .elv = elv09, .way = W1},
	{.br = 183, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl183nit, .cTbl = ctbl183nit, .aid = aid2794, .elvCaps = elv09, .elv = elv09, .way = W1},
	{.br = 195, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl195nit, .cTbl = ctbl195nit, .aid = aid2299, .elvCaps = elv09, .elv = elv09, .way = W1},
	{.br = 207, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl207nit, .cTbl = ctbl207nit, .aid = aid1725, .elvCaps = elv09, .elv = elv09, .way = W1},
	{.br = 220, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl220nit, .cTbl = ctbl220nit, .aid = aid1105, .elvCaps = elv09, .elv = elv09, .way = W1},
	{.br = 234, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl234nit, .cTbl = ctbl234nit, .aid = aid0429, .elvCaps = elv09, .elv = elv09, .way = W1},
	{.br = 249, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl249nit, .cTbl = ctbl249nit, .aid = aid0072, .elvCaps = elv09, .elv = elv09, .way = W1},
	{.br = 265, .refBr = 268, .cGma = gma2p15, .rTbl = rtbl265nit, .cTbl = ctbl265nit, .aid = aid0072, .elvCaps = elv08, .elv = elv08, .way = W1},
	{.br = 282, .refBr = 283, .cGma = gma2p15, .rTbl = rtbl282nit, .cTbl = ctbl282nit, .aid = aid0072, .elvCaps = elv07, .elv = elv07, .way = W1},
	{.br = 300, .refBr = 300, .cGma = gma2p15, .rTbl = rtbl300nit, .cTbl = ctbl300nit, .aid = aid0072, .elvCaps = elv06, .elv = elv06, .way = W1},
	{.br = 316, .refBr = 315, .cGma = gma2p15, .rTbl = rtbl316nit, .cTbl = ctbl316nit, .aid = aid0072, .elvCaps = elv06, .elv = elv06, .way = W1},
	{.br = 333, .refBr = 334, .cGma = gma2p15, .rTbl = rtbl333nit, .cTbl = ctbl333nit, .aid = aid0072, .elvCaps = elv05, .elv = elv05, .way = W1},
	{.br = 350, .refBr = 350, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elv04, .elv = elv04, .way = W2},
/*hbm interpolation */
	{.br = 382, .refBr = 382, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elvAcl08, .elv = elvAcl08, .way = W3},  // hbm is acl on
	{.br = 407, .refBr = 407, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elvAcl08, .elv = elvAcl08, .way = W3},  // hbm is acl on
	{.br = 433, .refBr = 433, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elvAcl08, .elv = elvAcl08, .way = W3},  // hbm is acl on
	{.br = 461, .refBr = 461, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elvAcl08, .elv = elvAcl08, .way = W3},  // hbm is acl on
	{.br = 491, .refBr = 491, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elvAcl08, .elv = elvAcl08, .way = W3},  // hbm is acl on
	{.br = 517, .refBr = 517, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elvAcl08, .elv = elvAcl08, .way = W3},  // hbm is acl on
	{.br = 545, .refBr = 545, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elvAcl08, .elv = elvAcl08, .way = W3},  // hbm is acl on
/* hbm */
	{.br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvCaps = elvAcl08, .elv = elvAcl08, .way = W4},  // hbm is acl on
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

	memcpy(result+1, hbm, S6E3FA2_HBMGAMMA_LEN);


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
	for (i= 0; i < S6E3FA2_HBMGAMMA_LEN; i ++) {
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
		if (dimInfo[i].br == S6E3FA2_MAX_BRIGHTNESS)
			ref_idx = i;

		if (dimInfo[i].br == S6E3FA2_HBM_BRIGHTNESS)
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

#if 0
	panel->octa_a2_read_data_work_q = NULL;

	/* support a2 line panel efs data start */
	if ( 0 && panelline == S6E3FA2_A3_LINE_ID) {
		dsim_info("%s init dimming info for A3 Line Daisy rev.D, E panel\n", __func__);
		diminfo = (void *)a3_daisy_dimming_info_RD;

		INIT_DELAYED_WORK(&panel->octa_a2_read_data_work,
				(work_func_t)a3_line_read_work_fn);

		panel->octa_a2_read_data_work_q =
				create_singlethread_workqueue("octa_a3_workqueue");
/* support a3 line panel efs data end */
	} else
#endif
	{
			dsim_info("%s init dimming info for s6e3fa2 panel\n", __func__);
			diminfo = tulip_dimming_info;
	}

	panel->dim_data= (void *)dimming;
	panel->dim_info = (void *)diminfo;

	panel->br_tbl = (unsigned int *)br_tbl;
	panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE;
	panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE;

	memcpy( panel->aid_set, aid9793, sizeof(aid9793) );
	panel->aid_len = AID_CMD_CNT;
	panel->aid_reg_offset = 1;

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
#if 0 // kyNam_151105_
	/* for vt */
	temp = (mtp[pos+1]) << 8 | mtp[pos];
	for(i=0;i<CI_MAX;i++)
		dimming->vt_mtp[i] = (temp >> (i*4)) &0x0f;
#else
	for(i=0;i<CI_MAX;i++)	{
		dimming->vt_mtp[i] = dimming->mtp[V0][i];
		dimming->mtp[V0][i] = 0;
	}
#endif


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

/* support a3 line panel efs data start */
#if 0
	if (panel->octa_a2_read_data_work_q) {
		panel->octa_a2_read_cnt = 50;
		queue_delayed_work(panel->octa_a2_read_data_work_q,
					&panel->octa_a2_read_data_work, msecs_to_jiffies(500));
	}
#endif
/* support a3 line panel efs data end */


error:
	return ret;

}


#endif

static int s6e3fa2_read_init_info(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E3FA2_COORDINATE_LEN] = {0,};
	unsigned char buf[S6E3FA2_MTP_DATE_SIZE] = {0, };
	unsigned char hbm_gamma[S6E3FA2_HBMGAMMA_LEN + 1] = {0, };


	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	// id
	ret = dsim_read_hl_data(dsim, S6E3FA2_ID_REG, S6E3FA2_ID_LEN, dsim->priv.id);
	if (ret != S6E3FA2_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNECTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6E3FA2_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	// mtp
	ret = dsim_read_hl_data(dsim, S6E3FA2_MTP_ADDR, S6E3FA2_MTP_DATE_SIZE, buf);
	if (ret != S6E3FA2_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E3FA2_MTP_SIZE);
	memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3FA2_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3FA2_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	// coordinate
	ret = dsim_read_hl_data(dsim, S6E3FA2_COORDINATE_REG, S6E3FA2_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3FA2_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim_info("READ coordi : ");
	for(i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");


#if 0 // kyNam_151021_ FA2 = No chip id
	// code = chip id
	ret = dsim_read_hl_data(dsim, S6E3FA2_CODE_REG, S6E3FA2_CODE_LEN, dsim->priv.code);
	if (ret != S6E3FA2_CODE_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for(i = 0; i < S6E3FA2_CODE_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");
#endif

	// tset
	ret = dsim_read_hl_data(dsim, TSET_REG, TSET_LEN - 1, dsim->priv.tset_set);
	if (ret < TSET_LEN - 1) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ tset : ");
	for(i = 0; i < TSET_LEN - 1; i++)
		dsim_info("%x, ", dsim->priv.tset_set[i]);
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

#if 0
	/* read hbm elvss for hbm interpolation */
	panel->hbm_elvss = dsim->priv.elvss_set[S6E3FA2_HBM_ELVSS_INDEX];

	ret = dsim_read_hl_data(dsim, S6E3FA2_HBMGAMMA_REG, S6E3FA2_HBMGAMMA_LEN + 1, hbm_gamma);
	if (ret != S6E3FA2_HBMGAMMA_LEN + 1) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("HBM Gamma : ");
	memcpy(hbm, hbm_gamma + 1, S6E3FA2_HBMGAMMA_LEN);
#endif
	for(i = 1; i < S6E3FA2_HBMGAMMA_LEN + 1; i++)
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

static int s6e3fa2_wqhd_dump(struct dsim_device *dsim)
{
	int ret = 0;
	int i;
	unsigned char id[S6E3FA2_ID_LEN];
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

	ret = dsim_read_hl_data(dsim, S6E3FA2_RDDPM_ADDR, 3, rddpm);
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

	ret = dsim_read_hl_data(dsim, S6E3FA2_RDDSM_ADDR, 3, rddsm);
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

	ret = dsim_read_hl_data(dsim, S6E3FA2_ID_REG, S6E3FA2_ID_LEN, id);
	if (ret != S6E3FA2_ID_LEN) {
		dsim_err("%s : can't read panel id\n",__func__);
		goto dump_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6E3FA2_ID_LEN; i++)
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
static int s6e3fa2_wqhd_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3FA2_MTP_SIZE] = {0, };
	unsigned char hbm[S6E3FA2_HBMGAMMA_LEN] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;
#ifdef CONFIG_LCD_ALPM
	panel->alpm = 0;
	panel->current_alpm = 0;
	mutex_init(&panel->alpm_lock);
#endif

	ret = s6e3fa2_read_init_info(dsim, mtp, hbm);
	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}
#ifdef CONFIG_LCD_ALPM
	panel->mtpForALPM[0] = 0xC8;
	memcpy(&(panel->mtpForALPM[1]), mtp, ARRAY_SIZE(mtp));
	panel->prev_VT[0] = panel->mtpForALPM[34];
	panel->prev_VT[1] = panel->mtpForALPM[35];
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp, hbm);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif

probe_exit:
	return ret;

}


static int s6e3fa2_wqhd_displayon(struct dsim_device *dsim)
{
	int ret = 0;
#ifdef CONFIG_LCD_ALPM
	struct panel_private *panel = &dsim->priv;
#endif

	dsim_info("MDD : %s was called\n", __func__);

#ifdef CONFIG_LCD_ALPM
	if (panel->current_alpm && panel->alpm) {
		 dsim_info("%s : ALPM mode\n", __func__);
	} else if (panel->current_alpm) {
		ret = alpm_set_mode(dsim, ALPM_OFF);
		if (ret) {
			dsim_err("failed to exit alpm.\n");
			goto displayon_err;
		}
	} else {
		if (panel->alpm) {
			ret = alpm_set_mode(dsim, ALPM_ON);
			if (ret) {
				dsim_err("failed to initialize alpm.\n");
				goto displayon_err;
			}
		} else {
			ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
				goto displayon_err;
			}
		}
	}
#else
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
 		goto displayon_err;
	}
#endif

displayon_err:
	return ret;

}


static int s6e3fa2_wqhd_exit(struct dsim_device *dsim)
{
	int ret = 0;
#ifdef CONFIG_LCD_ALPM
	struct panel_private *panel = &dsim->priv;
#endif
	dsim_info("MDD : %s was called\n", __func__);
#ifdef CONFIG_LCD_ALPM
	mutex_lock(&panel->alpm_lock);
	if (panel->current_alpm && panel->alpm) {
		dsim->alpm = 1;
		dsim_info( "%s : ALPM mode\n", __func__);
	} else {
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
	}

	dsim_info("MDD : %s was called unlock\n", __func__);
#else
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
#endif

exit_err:
#ifdef CONFIG_LCD_ALPM
	mutex_unlock(&panel->alpm_lock);
#endif
	return ret;
}

static int s6e3fa2_wqhd_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

#ifdef CONFIG_LCD_ALPM
	if (dsim->priv.current_alpm) {
		dsim_info("%s : ALPM mode\n", __func__);

		return ret;
	}
#endif

	/* 7. Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(5);

	/* 9. Interface Setting */
#if 0
	ret = dsim_write_hl_data(dsim, SEQ_MIC_ENABLE, ARRAY_SIZE(SEQ_MIC_ENABLE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_MIC_ENABLE\n", __func__);
		goto init_exit;
	}
#endif
	msleep(120);

	/* Common Setting */
#if 1
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONTROL_SET_350CD, ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET_350CD));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_CONTROL_SET_350CD\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE_L\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TSET_GLOBAL, ARRAY_SIZE(SEQ_TSET_GLOBAL));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF_OPR_AVR, ARRAY_SIZE(SEQ_ACL_OFF_OPR_AVR));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_OFF_OPR_AVR\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ELVSS_SET\n", __func__);
		goto init_exit;
	}
#endif
	// kyNam_151020_ ToDo : UpdateBrightness

	/* POC setting */
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ERR_FG, ARRAY_SIZE(SEQ_ERR_FG));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ERR_FG\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TOUCH_HSYNC_ON, ARRAY_SIZE(SEQ_TOUCH_HSYNC_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TOUCH_HSYNC_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PCD_SET_DET_LOW, ARRAY_SIZE(SEQ_PCD_SET_DET_LOW));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PCD_SET_DET_LOW\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PARTIAL_AREA, ARRAY_SIZE(SEQ_PARTIAL_AREA));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PARTIAL_AREA\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	dsim_info( "%s : TE_ON. (decon_dsim\n", __func__ );
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}


struct dsim_panel_ops s6e3fa2_panel_ops = {
	.early_probe = NULL,
	.probe		= s6e3fa2_wqhd_probe,
	.displayon	= s6e3fa2_wqhd_displayon,
	.exit		= s6e3fa2_wqhd_exit,
	.init		= s6e3fa2_wqhd_init,
	.dump 		= s6e3fa2_wqhd_dump,
};

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &s6e3fa2_panel_ops;
}

static int __init s6e3fa2_get_lcd_type(char *arg)
{
	unsigned int lcdtype;

	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	s6e3fa2_lcd_type = S6E3FA2_LCDTYPE_FHD;
	dsim_info("S6E3FA2 LCD TYPE : %d (FHD)\n", s6e3fa2_lcd_type);
	return 0;
}
early_param("lcdtype", s6e3fa2_get_lcd_type);

