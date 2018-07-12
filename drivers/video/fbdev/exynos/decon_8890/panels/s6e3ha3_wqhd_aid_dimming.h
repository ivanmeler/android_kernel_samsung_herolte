/* linux/drivers/video/exynos_decon/panel/s6e3ha2_wqhd_aid_dimming.h
*
* Header file for Samsung aid Dimming Driver.
*
* Copyright (c) 2013 Samsung Electronics
* Minwoo Kim <minwoo7945.kim@samsung.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef __S6E3HA2_WQHD_AID_DIMMING_H__
#define __S6E3HA2_WQHD_AID_DIMMING_H__

/********************************************* HA2 ******************************/

static signed char HA2_aid9825[3] = {0xB2, 0xE3, 0x90};
static signed char HA2_aid9713[3] = {0xB2, 0xC6, 0x90};
static signed char HA2_aid9635[3] = {0xB2, 0xB2, 0x90};
static signed char HA2_aid9523[3] = {0xB2, 0x95, 0x90};
static signed char HA2_aid9445[3] = {0xB2, 0x81, 0x90};
static signed char HA2_aid9344[3] = {0xB2, 0x67, 0x90};
static signed char HA2_aid9270[3] = {0xB2, 0x54, 0x90};
static signed char HA2_aid9200[3] = {0xB2, 0x42, 0x90};
static signed char HA2_aid9130[3] = {0xB2, 0x30, 0x90};
static signed char HA2_aid9030[3] = {0xB2, 0x16, 0x90};
static signed char HA2_aid8964[3] = {0xB2, 0x05, 0x90};
static signed char HA2_aid8894[3] = {0xB2, 0xF3, 0x80};
static signed char HA2_aid8832[3] = {0xB2, 0xE3, 0x80};
static signed char HA2_aid8723[3] = {0xB2, 0xC7, 0x80};
static signed char HA2_aid8653[3] = {0xB2, 0xB5, 0x80};
static signed char HA2_aid8575[3] = {0xB2, 0xA1, 0x80};
static signed char HA2_aid8393[3] = {0xB2, 0x72, 0x80};
static signed char HA2_aid8284[3] = {0xB2, 0x56, 0x80};
static signed char HA2_aid8218[3] = {0xB2, 0x45, 0x80};
static signed char HA2_aid8148[3] = {0xB2, 0x33, 0x80};
static signed char HA2_aid7974[3] = {0xB2, 0x06, 0x80};
static signed char HA2_aid7896[3] = {0xB2, 0xF2, 0x70};
static signed char HA2_aid7717[3] = {0xB2, 0xC4, 0x70};
static signed char HA2_aid7535[3] = {0xB2, 0x95, 0x70};
static signed char HA2_aid7469[3] = {0xB2, 0x84, 0x70};
static signed char HA2_aid7290[3] = {0xB2, 0x56, 0x70};
static signed char HA2_aid7104[3] = {0xB2, 0x26, 0x70};
static signed char HA2_aid6848[3] = {0xB2, 0xE4, 0x60};
static signed char HA2_aid6669[3] = {0xB2, 0xB6, 0x60};
static signed char HA2_aid6491[3] = {0xB2, 0x88, 0x60};
static signed char HA2_aid6231[3] = {0xB2, 0x45, 0x60};
static signed char HA2_aid5947[3] = {0xB2, 0xFC, 0x50};
static signed char HA2_aid5675[3] = {0xB2, 0xB6, 0x50};
static signed char HA2_aid5419[3] = {0xB2, 0x74, 0x50};
static signed char HA2_aid5140[3] = {0xB2, 0x2C, 0x50};
static signed char HA2_aid4752[3] = {0xB2, 0xC8, 0x40};
static signed char HA2_aid4383[3] = {0xB2, 0x69, 0x40};
static signed char HA2_aid4006[3] = {0xB2, 0x08, 0x40};
static signed char HA2_aid3622[3] = {0xB2, 0xA5, 0x30};
static signed char HA2_aid3133[3] = {0xB2, 0x27, 0x30};
static signed char HA2_aid2620[3] = {0xB2, 0xA3, 0x20};
static signed char HA2_aid2158[3] = {0xB2, 0x2C, 0x20};
static signed char HA2_aid1611[3] = {0xB2, 0x9F, 0x10};
static signed char HA2_aid1005[3] = {0xB2, 0x03, 0x10};


static unsigned char HA2_elv1[3] = {
	0x8C, 0x16
};
static unsigned char HA2_elv2[3] = {
	0x8C, 0x15
};
static unsigned char HA2_elv3[3] = {
	0x8C, 0x14
};
static unsigned char HA2_elv4[3] = {
	0x8C, 0x13
};
static unsigned char HA2_elv5[3] = {
	0x8C, 0x12
};
static unsigned char HA2_elv6[3] = {
	0x8C, 0x11
};
static unsigned char HA2_elv7[3] = {
	0x8C, 0x10
};
static unsigned char HA2_elv8[3] = {
	0x8C, 0x0F
};
static unsigned char HA2_elv9[3] = {
	0x8C, 0x0E
};
static unsigned char HA2_elv10[3] = {
	0x8C, 0x0D
};
static unsigned char HA2_elv11[3] = {
	0x8C, 0x0C
};
static unsigned char HA2_elv12[3] = {
	0x8C, 0x0B
};
static unsigned char HA2_elv13[3] = {
	0x8C, 0x0A
};
// hbm interpolation
static unsigned char HA2_elv14[3]  = {
	0x8C, 0x09
};
static unsigned char HA2_elv15[3]  = {
	0x8C, 0x08
};
static unsigned char HA2_elv16[3]  = {
	0x8C, 0x06
};
static unsigned char HA2_elv17[3]  = {
	0x8C, 0x04
};
static unsigned char HA2_elv18[3]  = {
	0x8C, 0x02
};



static unsigned char HA2_elvCaps1[3]  = {
	0x9C, 0x16
};
static unsigned char HA2_elvCaps2[3]  = {
	0x9C, 0x15
};
static unsigned char HA2_elvCaps3[3]  = {
	0x9C, 0x14
};
static unsigned char HA2_elvCaps4[3]  = {
	0x9C, 0x13
};
static unsigned char HA2_elvCaps5[3]  = {
	0x9C, 0x12
};
static unsigned char HA2_elvCaps6[3]  = {
	0x9C, 0x11
};
static unsigned char HA2_elvCaps7[3]  = {
	0x9C, 0x10
};
static unsigned char HA2_elvCaps8[3]  = {
	0x9C, 0x0F
};
static unsigned char HA2_elvCaps9[3]  = {
	0x9C, 0x0E
};
static unsigned char HA2_elvCaps10[3]  = {
	0x9C, 0x0D
};
static unsigned char HA2_elvCaps11[3]  = {
	0x9C, 0x0C
};
static unsigned char HA2_elvCaps12[3]  = {
	0x9C, 0x0B
};
static unsigned char HA2_elvCaps13[3]  = {
	0x9C, 0x0A
};

// hbm interpolation
static unsigned char HA2_elvCaps14[3]  = {
	0x9C, 0x09
};
static unsigned char HA2_elvCaps15[3]  = {
	0x9C, 0x08
};
static unsigned char HA2_elvCaps16[3]  = {
	0x9C, 0x06
};
static unsigned char HA2_elvCaps17[3]  = {
	0x9C, 0x04
};
static unsigned char HA2_elvCaps18[3]  = {
	0x9C, 0x02
};





/********************************************** HA3 **********************************************/

// start of aid sheet in rev.D
static signed char HA3_rtbl2nit[10] = {0, 7, 19, 21, 20, 17, 13, 9, 7, 0};
static signed char HA3_rtbl3nit[10] = {0, 18, 20, 19, 17, 15, 13, 8, 6, 0};
static signed char HA3_rtbl4nit[10] = {0, 18, 20, 18, 15, 13, 12, 7, 6, 0};
static signed char HA3_rtbl5nit[10] = {0, 20, 19, 17, 14, 13, 12, 7, 6, 0};
static signed char HA3_rtbl6nit[10] = {0, 22, 19, 17, 14, 13, 12, 8, 6, 0};
static signed char HA3_rtbl7nit[10] = {0, 18, 18, 15, 13, 11, 10, 6, 5, 0};
static signed char HA3_rtbl8nit[10] = {0, 18, 17, 14, 12, 11, 10, 6, 5, 0};
static signed char HA3_rtbl9nit[10] = {0, 17, 17, 14, 12, 11, 10, 5, 5, 0};
static signed char HA3_rtbl10nit[10] = {0, 18, 16, 13, 11, 10, 9, 5, 5, 0};
static signed char HA3_rtbl11nit[10] = {0, 18, 15, 13, 11, 9, 8, 5, 5, 0};
static signed char HA3_rtbl12nit[10] = {0, 18, 15, 12, 10, 9, 7, 5, 4, 0};
static signed char HA3_rtbl13nit[10] = {0, 18, 15, 12, 10, 9, 8, 5, 4, 0};
static signed char HA3_rtbl14nit[10] = {0, 16, 14, 11, 9, 8, 7, 4, 4, 0};
static signed char HA3_rtbl15nit[10] = {0, 19, 13, 11, 9, 7, 7, 4, 4, 0};
static signed char HA3_rtbl16nit[10] = {0, 17, 13, 10, 8, 7, 6, 3, 4, 0};
static signed char HA3_rtbl17nit[10] = {0, 16, 13, 10, 8, 7, 6, 4, 4, 0};
static signed char HA3_rtbl19nit[10] = {0, 16, 13, 10, 8, 6, 5, 3, 4, 0};
static signed char HA3_rtbl20nit[10] = {0, 18, 12, 9, 8, 6, 5, 3, 4, 0};
static signed char HA3_rtbl21nit[10] = {0, 16, 12, 9, 8, 5, 5, 2, 4, 0};
static signed char HA3_rtbl22nit[10] = {0, 15, 12, 9, 7, 6, 5, 2, 4, 0};
static signed char HA3_rtbl24nit[10] = {0, 16, 11, 9, 7, 5, 5, 2, 4, 0};
static signed char HA3_rtbl25nit[10] = {0, 12, 11, 8, 6, 5, 4, 2, 4, 0};
static signed char HA3_rtbl27nit[10] = {0, 16, 10, 8, 6, 4, 4, 3, 4, 0};
static signed char HA3_rtbl29nit[10] = {0, 12, 10, 7, 6, 4, 4, 2, 4, 0};
static signed char HA3_rtbl30nit[10] = {0, 12, 10, 7, 6, 4, 3, 2, 4, 0};
static signed char HA3_rtbl32nit[10] = {0, 13, 9, 7, 5, 4, 4, 2, 3, 0};
static signed char HA3_rtbl34nit[10] = {0, 12, 9, 7, 5, 4, 3, 2, 3, 0};
static signed char HA3_rtbl37nit[10] = {0, 9, 9, 6, 5, 4, 3, 2, 3, 0};
static signed char HA3_rtbl39nit[10] = {0, 9, 8, 6, 4, 3, 3, 2, 3, 0};
static signed char HA3_rtbl41nit[10] = {0, 12, 7, 5, 4, 3, 3, 2, 3, 0};
static signed char HA3_rtbl44nit[10] = {0, 9, 7, 5, 4, 3, 2, 1, 3, 0};
static signed char HA3_rtbl47nit[10] = {0, 6, 7, 5, 4, 3, 3, 1, 3, 0};
static signed char HA3_rtbl50nit[10] = {0, 7, 6, 4, 3, 2, 2, 1, 3, 0};
static signed char HA3_rtbl53nit[10] = {0, 5, 6, 4, 3, 2, 2, 1, 3, 0};
static signed char HA3_rtbl56nit[10] = {0, 6, 6, 4, 3, 3, 3, 2, 4, 1};
static signed char HA3_rtbl60nit[10] = {0, 7, 5, 4, 3, 2, 2, 1, 3, 0};
static signed char HA3_rtbl64nit[10] = {0, 5, 5, 4, 2, 2, 2, 1, 3, 0};
static signed char HA3_rtbl68nit[10] = {0, 8, 4, 3, 2, 2, 1, 1, 3, 0};
static signed char HA3_rtbl72nit[10] = {0, 4, 4, 3, 2, 2, 2, 1, 3, 0};
static signed char HA3_rtbl77nit[10] = {0, 4, 4, 3, 3, 1, 2, 1, 3, 0};
static signed char HA3_rtbl82nit[10] = {0, 5, 4, 3, 2, 1, 2, 3, 4, 0};
static signed char HA3_rtbl87nit[10] = {0, 6, 4, 3, 2, 1, 2, 2, 3, 0};
static signed char HA3_rtbl93nit[10] = {0, 4, 4, 3, 3, 1, 3, 3, 3, 0};
static signed char HA3_rtbl98nit[10] = {0, 5, 4, 3, 2, 1, 3, 3, 2, 0};
static signed char HA3_rtbl105nit[10] = {0, 5, 4, 3, 2, 1, 3, 3, 2, 0};
static signed char HA3_rtbl111nit[10] = {0, 7, 4, 3, 3, 2, 3, 4, 2, 0};
static signed char HA3_rtbl119nit[10] = {0, 6, 4, 3, 2, 2, 3, 3, 2, 0};
static signed char HA3_rtbl126nit[10] = {0, 6, 3, 2, 2, 2, 2, 3, 1, 0};
static signed char HA3_rtbl134nit[10] = {0, 6, 3, 2, 2, 2, 3, 4, 2, 0};
static signed char HA3_rtbl143nit[10] = {0, 3, 4, 3, 2, 2, 4, 4, 1, 0};
static signed char HA3_rtbl152nit[10] = {0, 2, 4, 3, 2, 3, 3, 4, 2, 0};
static signed char HA3_rtbl162nit[10] = {0, 3, 4, 3, 2, 2, 3, 4, 1, 0};
static signed char HA3_rtbl172nit[10] = {0, 5, 3, 2, 2, 3, 4, 4, 3, 0};
static signed char HA3_rtbl183nit[10] = {0, 4, 3, 2, 2, 2, 4, 5, 4, 2};
static signed char HA3_rtbl195nit[10] = {0, 0, 3, 2, 2, 2, 3, 4, 2, 0};
static signed char HA3_rtbl207nit[10] = {0, 2, 2, 2, 2, 1, 2, 3, 1, 0};
static signed char HA3_rtbl220nit[10] = {0, 2, 2, 2, 2, 1, 2, 4, 1, 0};
static signed char HA3_rtbl234nit[10] = {0, 2, 1, 1, 1, 1, 2, 3, 1, 0};
static signed char HA3_rtbl249nit[10] = {0, 0, 2, 1, 1, 1, 2, 2, 1, 0};
static signed char HA3_rtbl265nit[10] = {0, 0, 2, 1, 1, 1, 1, 2, 1, 0};
static signed char HA3_rtbl282nit[10] = {0, 0, 1, 0, 0, 1, 2, 2, 1, 0};
static signed char HA3_rtbl300nit[10] = {0, 2, 0, 0, 0, 0, 1, 2, 1, 0};
static signed char HA3_rtbl316nit[10] = {0, 0, 1, 0, 0, 1, 1, 1, 1, 0};
static signed char HA3_rtbl333nit[10] = {0, 0, 1, 0, 0, 0, 1, 1, 1, 0};
static signed char HA3_rtbl350nit[10] = {0, 0, 0, 0, 0, -1, 0, 0, 0, 0};
static signed char HA3_rtbl357nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
static signed char HA3_rtbl365nit[10] = {0, 1, -1, 0, 0, 0, 0, 0, 1, 0};
static signed char HA3_rtbl372nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl380nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl387nit[10] = {0, 0, -1, -1, -1, -1, 0, 0, 0, 0};
static signed char HA3_rtbl395nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char HA3_ctbl2nit[30] = {0, 0, 0, 0, 0, 0,4, 1, -1, -1, 3, -5, -5, 3, -8, -3, 3, -9, -2, 2, -4, 0, 1, -1, -2, 0, -2, -7, 0, -4};
static signed char HA3_ctbl3nit[30] = {0, 0, 0, 0, 0, 0,4, 1, -1, -2, 3, -8, -3, 3, -6, 0, 3, -6, -3, 1, -5, -1, 0, -2, -2, 0, -2, -5, 1, -2};
static signed char HA3_ctbl4nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -6, -2, 3, -7, -2, 3, -7, 0, 2, -8, -2, 1, -3, -2, 1, -2, -1, 0, -1, -4, 1, -2};
static signed char HA3_ctbl5nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -8, -1, 3, -7, -1, 3, -6, -1, 2, -8, -2, 1, -3, -1, 1, -2, -1, 0, -1, -5, 0, -2};
static signed char HA3_ctbl6nit[30] = {0, 0, 0, 0, 0, 0,4, 3, -8, -2, 2, -8, -1, 2, -7, -1, 2, -8, -2, 1, -4, -2, 0, -2, -1, 0, -1, -3, 1, -1};
static signed char HA3_ctbl7nit[30] = {0, 0, 0, 0, 0, 0,2, 3, -8, -1, 2, -10, 0, 2, -6, -1, 2, -7, -2, 1, -4, -1, 1, -1, -1, 0, -1, -2, 1, 0};
static signed char HA3_ctbl8nit[30] = {0, 0, 0, 0, 0, 0,1, 2, -10, 1, 3, -8, 0, 3, -7, 1, 2, -6, -2, 1, -3, -2, 0, -2, -1, 0, -1, -3, 0, -1};
static signed char HA3_ctbl9nit[30] = {0, 0, 0, 0, 0, 0,2, 3, -10, 1, 3, -7, 1, 2, -8, -1, 1, -7, -3, 0, -3, -2, 0, -2, 0, 0, 0, -3, 0, -1};
static signed char HA3_ctbl10nit[30] = {0, 0, 0, 0, 0, 0,2, 2, -9, 1, 3, -9, 0, 2, -8, 0, 2, -6, -3, 0, -3, -1, 0, -1, -1, 0, -1, -2, 0, 0};
static signed char HA3_ctbl11nit[30] = {0, 0, 0, 0, 0, 0,7, 4, -8, 1, 3, -8, -1, 2, -8, 1, 2, -6, -3, 0, -4, -1, 0, -1, -1, 0, -1, -2, 0, 0};
static signed char HA3_ctbl12nit[30] = {0, 0, 0, 0, 0, 0,3, 3, -10, 0, 2, -9, 0, 2, -8, -1, 1, -6, -2, 1, -2, -1, 0, -1, 0, 0, 0, -2, 0, 0};
static signed char HA3_ctbl13nit[30] = {0, 0, 0, 0, 0, 0,2, 2, -11, 0, 2, -9, -1, 2, -8, 1, 2, -6, -3, 0, -3, -1, 0, -1, -1, 0, -1, -1, 0, 1};
static signed char HA3_ctbl14nit[30] = {0, 0, 0, 0, 0, 0,4, 3, -10, 1, 3, -7, 0, 2, -8, 0, 2, -6, -3, 0, -2, -1, 0, -1, -1, 0, 0, 0, 0, 1};
static signed char HA3_ctbl15nit[30] = {0, 0, 0, 0, 0, 0,7, 4, -11, 1, 3, -7, 1, 2, -6, 2, 2, -5, -3, 0, -2, -1, 0, -1, -1, 0, -1, 0, 0, 2};
static signed char HA3_ctbl16nit[30] = {0, 0, 0, 0, 0, 0,5, 4, -9, 2, 3, -9, 0, 2, -8, 1, 2, -4, -3, 0, -3, -1, 0, -1, -1, 0, 0, 1, 0, 2};
static signed char HA3_ctbl17nit[30] = {0, 0, 0, 0, 0, 0,5, 4, -9, 2, 3, -9, 0, 2, -8, 0, 1, -5, -2, 0, -2, -1, 0, -1, -1, 0, 0, 1, 0, 2};
static signed char HA3_ctbl19nit[30] = {0, 0, 0, 0, 0, 0,4, 2, -12, -1, 2, -11, 1, 2, -6, 0, 1, -5, -1, 0, -1, -1, 0, 0, -1, 0, -1, 2, 0, 3};
static signed char HA3_ctbl20nit[30] = {0, 0, 0, 0, 0, 0,6, 4, -10, 2, 3, -11, 0, 2, -6, 1, 1, -4, -1, 0, -1, -2, 0, -1, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl21nit[30] = {0, 0, 0, 0, 0, 0,7, 4, -10, 1, 3, -11, -2, 0, -8, 1, 1, -5, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl22nit[30] = {0, 0, 0, 0, 0, 0,4, 3, -12, 2, 3, -9, 1, 2, -7, -1, 0, -5, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl24nit[30] = {0, 0, 0, 0, 0, 0,9, 4, -13, -3, 3, -9, 0, 1, -6, 0, 0, -5, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl25nit[30] = {0, 0, 0, 0, 0, 0,5, 3, -10, 1, 3, -9, 0, 1, -6, 0, 1, -5, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl27nit[30] = {0, 0, 0, 0, 0, 0,6, 4, -13, 1, 2, -9, -1, 1, -6, 0, 1, -4, -1, 0, -1, -1, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl29nit[30] = {0, 0, 0, 0, 0, 0,5, 3, -11, 1, 2, -11, -1, 1, -5, 0, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl30nit[30] = {0, 0, 0, 0, 0, 0,4, 2, -13, 0, 2, -10, 0, 1, -4, 1, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl32nit[30] = {0, 0, 0, 0, 0, 0,4, 4, -14, 1, 2, -8, -1, 1, -6, 0, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl34nit[30] = {0, 0, 0, 0, 0, 0,4, 4, -14, 1, 2, -8, -2, 0, -6, 0, 0, -4, -1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl37nit[30] = {0, 0, 0, 0, 0, 0,2, 1, -12, 1, 2, -9, -2, 1, -5, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl39nit[30] = {0, 0, 0, 0, 0, 0,3, 4, -14, 1, 2, -7, 0, 1, -3, 1, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl41nit[30] = {0, 0, 0, 0, 0, 0,5, 4, -14, 2, 2, -7, -1, 1, -4, 1, 1, -3, 0, 0, 1, 1, 0, 0, -1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl44nit[30] = {0, 0, 0, 0, 0, 0,4, 4, -13, 3, 2, -6, -2, 0, -4, 1, 0, -3, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl47nit[30] = {0, 0, 0, 0, 0, 0,2, 3, -13, 2, 1, -6, -1, 0, -4, 1, 1, -2, 0, 0, 1, 1, 0, 0, -1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl50nit[30] = {0, 0, 0, 0, 0, 0,4, 3, -13, 2, 2, -5, -1, 0, -4, 2, 1, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl53nit[30] = {0, 0, 0, 0, 0, 0,2, 3, -13, 3, 2, -4, -1, 0, -3, 1, 0, -2, 0, 0, 1, 1, 0, 0, -1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl56nit[30] = {0, 0, 0, 0, 0, 0,2, 4, -12, 2, 1, -6, -1, 0, -4, 1, 0, -2, -1, 0, 0, 2, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl60nit[30] = {0, 0, 0, 0, 0, 0,3, 3, -14, 3, 1, -4, -2, 0, -3, 2, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl64nit[30] = {0, 0, 0, 0, 0, 0,3, 3, -13, 1, 0, -5, -2, 0, -3, 2, 0, -1, -1, 0, 0, 2, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl68nit[30] = {0, 0, 0, 0, 0, 0,4, 4, -14, 2, 1, -4, -2, 0, -4, 0, 0, -1, 0, 0, 1, 2, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl72nit[30] = {0, 0, 0, 0, 0, 0,4, 4, -12, 1, 1, -4, -1, 0, -3, 2, 0, 0, 0, 0, 1, 1, 0, 0, -1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl77nit[30] = {0, 0, 0, 0, 0, 0,4, 4, -11, 2, 1, -4, -1, 0, -2, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 3, 0, 4};
static signed char HA3_ctbl82nit[30] = {0, 0, 0, 0, 0, 0,4, 4, -12, 2, 1, -3, 0, 0, -2, 1, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 3, 0, 4};
static signed char HA3_ctbl87nit[30] = {0, 0, 0, 0, 0, 0,3, 4, -12, 3, 1, -2, -2, 0, -3, 2, 0, -1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 3, 0, 4};
static signed char HA3_ctbl93nit[30] = {0, 0, 0, 0, 0, 0,3, 3, -11, 0, 0, -4, 0, 0, -2, 1, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl98nit[30] = {0, 0, 0, 0, 0, 0,3, 3, -11, 0, 0, -4, -1, 0, -2, 2, 0, 1, -1, 0, 0, 1, 0, 0, 0, 0, 0, 3, 0, 4};
static signed char HA3_ctbl105nit[30] = {0, 0, 0, 0, 0, 0,2, 3, -11, 3, 1, -3, -1, 0, -2, 1, 0, -1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 2, 0, 3};
static signed char HA3_ctbl111nit[30] = {0, 0, 0, 0, 0, 0,3, 3, -10, 1, 0, -4, 0, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 0, 0, 0, 0, 2, 0, 4};
static signed char HA3_ctbl119nit[30] = {0, 0, 0, 0, 0, 0,1, 2, -10, 1, 0, -3, -1, 0, -2, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HA3_ctbl126nit[30] = {0, 0, 0, 0, 0, 0,2, 2, -10, 0, 0, -3, 0, 0, -1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 3, 0, 2, 1, 0, 2};
static signed char HA3_ctbl134nit[30] = {0, 0, 0, 0, 0, 0,1, 2, -9, 0, 0, -3, 0, 0, 0, 1, 0, 0, 2, 0, 2, 0, 0, 1, 0, 0, 0, 3, 0, 3};
static signed char HA3_ctbl143nit[30] = {0, 0, 0, 0, 0, 0,2, 2, -7, -1, 0, -3, 0, 0, 0, 1, 0, 0, 0, 0, 0, -2, 0, 0, 1, 0, 1, 3, 0, 3};
static signed char HA3_ctbl152nit[30] = {0, 0, 0, 0, 0, 0,3, 2, -7, 0, 0, -2, 0, 0, -1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 2, 0, 2};
static signed char HA3_ctbl162nit[30] = {0, 0, 0, 0, 0, 0,3, 2, -6, 1, 0, -1, -1, 0, -2, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 2, 2, 0, 2};
static signed char HA3_ctbl172nit[30] = {0, 0, 0, 0, 0, 0,3, 2, -7, 1, 0, -2, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 2, 0, 2};
static signed char HA3_ctbl183nit[30] = {0, 0, 0, 0, 0, 0,2, 1, -6, 1, 0, -2, 1, 0, -1, 0, 0, 0, -1, 0, 0, 1, 0, 1, 0, 0, 1, 2, 0, 2};
static signed char HA3_ctbl195nit[30] = {0, 0, 0, 0, 0, 0,3, 1, -4, 1, 0, -1, 1, 0, -1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 2, 0, 2};
static signed char HA3_ctbl207nit[30] = {0, 0, 0, 0, 0, 0,1, 1, -5, 1, 0, -1, -1, 0, -2, 2, 0, 2, 0, 0, 0, 1, 0, 1, 0, 0, 0, 2, 0, 2};
static signed char HA3_ctbl220nit[30] = {0, 0, 0, 0, 0, 0,2, 1, -3, 1, 0, -1, 0, 0, -1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HA3_ctbl234nit[30] = {0, 0, 0, 0, 0, 0,1, 1, -4, 2, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HA3_ctbl249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -3, 2, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char HA3_ctbl265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -3, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 2, 0, 0, 0, 1, 0, 1, 1, 0, 1};
static signed char HA3_ctbl282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -2, 1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -2, 1, 0, -1, 1, 0, 0, -1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0};
static signed char HA3_ctbl316nit[30] = {0, 0, 0, 0, 0, 0,1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl333nit[30] = {0, 0, 0, 0, 0, 0,1, -1, 0, -1, 0, -1, 0, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl350nit[30] = {0, 0, 0, 0, 0, 0,2, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl357nit[30] = {0, 0, 0, 0, 0, 0,1, -1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0};
static signed char HA3_ctbl365nit[30] = {0, 0, 0, 0, 0, 0,1, 0, 1, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl372nit[30] = {0, 0, 0, 0, 0, 0,1, -1, 1, 0, 0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl380nit[30] = {0, 0, 0, 0, 0, 0,0, -1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl387nit[30] = {0, 0, 0, 0, 0, 0,1, 0, 2, 0, 0, 0, 1, 0, -1, 1, 0, 2, 0, 0, 0, 1, 0, 1, -1, 0, -1, 1, 0, 1};
static signed char HA3_ctbl395nit[30] = {0, 0, 0, 0, 0, 0,1, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl403nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl412nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, -1, 0, -1, 0, 0, 0};
static signed char HA3_ctbl420nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};



static signed char HA3_aid9802[3] = {0xB1, 0x90, 0xE1};
static signed char HA3_aid9690[3] = {0xB1, 0x90, 0xC4};
static signed char HA3_aid9570[3] = {0xB1, 0x90, 0xA5};
static signed char HA3_aid9442[3] = {0xB1, 0x90, 0x84};
static signed char HA3_aid9333[3] = {0xB1, 0x90, 0x68};
static signed char HA3_aid9240[3] = {0xB1, 0x90, 0x50};
static signed char HA3_aid9136[3] = {0xB1, 0x90, 0x35};
static signed char HA3_aid9054[3] = {0xB1, 0x90, 0x20};
static signed char HA3_aid8973[3] = {0xB1, 0x90, 0x0B};
static signed char HA3_aid8884[3] = {0xB1, 0x80, 0xF4};
static signed char HA3_aid8806[3] = {0xB1, 0x80, 0xE0};
static signed char HA3_aid8740[3] = {0xB1, 0x80, 0xCF};
static signed char HA3_aid8663[3] = {0xB1, 0x80, 0xBB};
static signed char HA3_aid8597[3] = {0xB1, 0x80, 0xAA};
static signed char HA3_aid8512[3] = {0xB1, 0x80, 0x94};
static signed char HA3_aid8453[3] = {0xB1, 0x80, 0x85};
static signed char HA3_aid8306[3] = {0xB1, 0x80, 0x5F};
static signed char HA3_aid8240[3] = {0xB1, 0x80, 0x4E};
static signed char HA3_aid8167[3] = {0xB1, 0x80, 0x3B};
static signed char HA3_aid8074[3] = {0xB1, 0x80, 0x23};
static signed char HA3_aid7884[3] = {0xB1, 0x70, 0xF2};
static signed char HA3_aid7818[3] = {0xB1, 0x70, 0xE1};
static signed char HA3_aid7628[3] = {0xB1, 0x70, 0xB0};
static signed char HA3_aid7446[3] = {0xB1, 0x70, 0x81};
static signed char HA3_aid7353[3] = {0xB1, 0x70, 0x69};
static signed char HA3_aid7163[3] = {0xB1, 0x70, 0x38};
static signed char HA3_aid6973[3] = {0xB1, 0x70, 0x07};
static signed char HA3_aid6709[3] = {0xB1, 0x60, 0xC3};
static signed char HA3_aid6508[3] = {0xB1, 0x60, 0x8F};
static signed char HA3_aid6329[3] = {0xB1, 0x60, 0x61};
static signed char HA3_aid6054[3] = {0xB1, 0x60, 0x1A};
static signed char HA3_aid5756[3] = {0xB1, 0x50, 0xCD};
static signed char HA3_aid5469[3] = {0xB1, 0x50, 0x83};
static signed char HA3_aid5186[3] = {0xB1, 0x50, 0x3A};
static signed char HA3_aid4996[3] = {0xB1, 0x50, 0x09};
static signed char HA3_aid4523[3] = {0xB1, 0x40, 0x8F};
static signed char HA3_aid4093[3] = {0xB1, 0x40, 0x20};
static signed char HA3_aid3717[3] = {0xB1, 0x30, 0xBF};
static signed char HA3_aid3349[3] = {0xB1, 0x30, 0x60};
static signed char HA3_aid3260[3] = {0xB1, 0x30, 0x49};
static signed char HA3_aid2578[3] = {0xB1, 0x20, 0x99};
static signed char HA3_aid2143[3] = {0xB1, 0x20, 0x29};
static signed char HA3_aid1550[3] = {0xB1, 0x10, 0x90};
static signed char HA3_aid1035[3] = {0xB1, 0x10, 0x0B};
static signed char HA3_aid957[3] = {0xB1, 0x00, 0xF7};
static signed char HA3_aid725[3] = {0xB1, 0x00, 0xBB};
static signed char HA3_aid465[3] = {0xB1, 0x00, 0x78};
static signed char HA3_aid368[3] = {0xB1, 0x00, 0x5F};
static signed char HA3_aid109[3] = {0xB1, 0x00, 0x1C};
static signed char HA3_aid39[3] = {0xB1, 0x00, 0x0A};


static  unsigned char HA3_elv1[3] = {
	0xAC, 0x16
};
static  unsigned char HA3_elv2[3] = {
	0xAC, 0x15
};
static  unsigned char HA3_elv3[3] = {
	0xAC, 0x14
};
static  unsigned char HA3_elv4[3] = {
	0xAC, 0x13
};
static  unsigned char HA3_elv5[3] = {
	0xAC, 0x12
};
static  unsigned char HA3_elv6[3] = {
	0xAC, 0x11
};
static  unsigned char HA3_elv7[3] = {
	0xAC, 0x10
};
static  unsigned char HA3_elv8[3] = {
	0xAC, 0x0F
};
static  unsigned char HA3_elv9[3] = {
	0xAC, 0x0E
};
static  unsigned char HA3_elv10[3] = {
	0xAC, 0x0D
};
static  unsigned char HA3_elv11[3] = {
	0xAC, 0x0C
};
static	unsigned char HA3_elv12[3] = {
	0xAC, 0x0B
};
static  unsigned char HA3_elv13[3] = {
	0xAC, 0x0A
};
/*
static  unsigned char HA3_elv15[3] = {
	0xAC, 0x08
};
static  unsigned char HA3_elv16[3] = {
	0xAC, 0x07
};
static  unsigned char HA3_elv17[3] = {
	0xAC, 0x05
};
static  unsigned char HA3_elv18[3] = {
	0xAC, 0x04
};
*/


static  unsigned char HA3_elvCaps1[3] = {
	0xBC, 0x16
};
static  unsigned char HA3_elvCaps2[3] = {
	0xBC, 0x15
};
static  unsigned char HA3_elvCaps3[3] = {
	0xBC, 0x14
};
static  unsigned char HA3_elvCaps4[3] = {
	0xBC, 0x13
};
static  unsigned char HA3_elvCaps5[3] = {
	0xBC, 0x12
};
static  unsigned char HA3_elvCaps6[3] = {
	0xBC, 0x11
};
static  unsigned char HA3_elvCaps7[3] = {
	0xBC, 0x10
};
static  unsigned char HA3_elvCaps8[3] = {
	0xBC, 0x0F
};
static  unsigned char HA3_elvCaps9[3] = {
	0xBC, 0x0E
};
static  unsigned char HA3_elvCaps10[3] = {
	0xBC, 0x0D
};
static  unsigned char HA3_elvCaps11[3] = {
	0xBC, 0x0C
};
static	unsigned char HA3_elvCaps12[3] = {
	0xBC, 0x0B
};
static  unsigned char HA3_elvCaps13[3] = {
	0xBC, 0x0A
};


static char HA3_elvss_offset1[3] = {0x00, 0x04, 0x00};
static char HA3_elvss_offset2[3] = {0x00, 0x03, -0x01};
static char HA3_elvss_offset3[3] = {0x00, 0x02, -0x02};
static char HA3_elvss_offset4[3] = {0x00, 0x01, -0x03};
static char HA3_elvss_offset5[3] = {0x00, 0x00, -0x04};
static char HA3_elvss_offset6[3] = {0x00, 0x00, 0x00};

static char HA3_elvss_offset7[3] = {0x00, 0x00, 0x00};
static signed char HA3_ctbl_da2nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -10, -10, 1, -8, -15, 0, -6, -17, 0, -6, -12, 0, -3, -3, 0, -2, -2, 0, -1, -10, 0, -8};
static signed char HA3_ctbl_da3nit[30] = {0, 0, 0, 0, 0, 0,-13, 2, -11, -11, 0, -11, -13, 0, -4, -15, 1, -6, -7, 0, -2, -3, 1, -2, -2, 0, -1, -8, 0, -7};
static signed char HA3_ctbl_da4nit[30] = {0, 0, 0, 0, 0, 0,-13, 0, -14, -12, 0, -10, -13, 0, -5, -11, 1, -5, -8, 0, -3, -3, 1, -2, -1, 0, -1, -6, 0, -6};
static signed char HA3_ctbl_da5nit[30] = {0, 0, 0, 0, 0, 0,-14, 1, -16, -12, 0, -9, -11, 0, -5, -11, 1, -5, -7, 1, -4, -3, 1, -2, -1, 0, -1, -5, 0, -5};
static signed char HA3_ctbl_da6nit[30] = {0, 0, 0, 0, 0, 0,-14, 0, -16, -11, 0, -8, -10, 0, -6, -8, 2, -4, -8, 1, -5, -1, 1, -1, -1, 0, -1, -4, 0, -4};
static signed char HA3_ctbl_da7nit[30] = {0, 0, 0, 0, 0, 0,-14, 2, -15, -10, 0, -10, -9, 0, -5, -10, 1, -6, -8, 0, -6, -1, 0, -1, -1, 0, -1, -4, 0, -4};
static signed char HA3_ctbl_da8nit[30] = {0, 0, 0, 0, 0, 0,-15, 1, -15, -9, 1, -8, -10, 0, -7, -10, 1, -6, -6, 0, -5, -1, 0, -1, -1, 0, -1, -3, 0, -3};
static signed char HA3_ctbl_da9nit[30] = {0, 0, 0, 0, 0, 0,-15, 1, -15, -8, 3, -7, -8, 3, -5, -7, 0, -5, -7, 0, -6, -1, 0, -1, -2, 0, -1, -1, 0, -2};
static signed char HA3_ctbl_da10nit[30] = {0, 0, 0, 0, 0, 0,-14, 2, -16, -8, 3, -7, -6, 3, -5, -8, 0, -6, -5, 0, -5, -1, 0, -1, -1, 0, 0, -1, 0, -2};
static signed char HA3_ctbl_da11nit[30] = {0, 0, 0, 0, 0, 0,-15, 2, -16, -9, 0, -9, -7, 3, -5, -7, 1, -6, -4, 0, -4, -1, 0, -1, -1, 0, 0, -1, 0, -2};
static signed char HA3_ctbl_da12nit[30] = {0, 0, 0, 0, 0, 0,-14, 2, -16, -5, 3, -7, -7, 2, -6, -7, 1, -6, -5, 0, -5, -1, 0, -1, -1, 0, 0, 0, 0, -1};
static signed char HA3_ctbl_da13nit[30] = {0, 0, 0, 0, 0, 0,-10, 5, -14, -6, 2, -8, -7, 2, -6, -7, 0, -7, -4, 0, -4, -1, 0, -1, -1, 0, 0, 0, 0, -1};
static signed char HA3_ctbl_da14nit[30] = {0, 0, 0, 0, 0, 0,-10, 4, -13, -6, 4, -7, -5, 3, -5, -8, 0, -7, -3, 0, -3, -1, 0, -1, 0, 0, 1, 0, 0, -1};
static signed char HA3_ctbl_da15nit[30] = {0, 0, 0, 0, 0, 0,-10, 4, -13, -5, 3, -8, -6, 2, -6, -8, 0, -7, -3, 0, -3, 0, 0, 0, -1, 0, 0, 1, 0, 0};
static signed char HA3_ctbl_da16nit[30] = {0, 0, 0, 0, 0, 0,-10, 5, -14, -4, 2, -7, -3, 3, -5, -6, 0, -6, -3, 0, -3, -1, 0, -1, 0, 0, 1, 1, 0, 0};
static signed char HA3_ctbl_da17nit[30] = {0, 0, 0, 0, 0, 0,-10, 5, -14, -2, 5, -6, -3, 3, -5, -7, 0, -7, -3, 0, -4, 0, 0, 0, -1, 0, 0, 2, 0, 1};
static signed char HA3_ctbl_da19nit[30] = {0, 0, 0, 0, 0, 0,-7, 5, -14, -3, 4, -8, -2, 3, -3, -6, 0, -6, -2, 0, -3, 0, 0, 0, 0, 0, 1, 2, 0, 1};
static signed char HA3_ctbl_da20nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -15, -5, 2, -9, -1, 4, -3, -6, 1, -6, -1, 0, -2, 0, 0, 0, 0, 0, 1, 2, 0, 1};
static signed char HA3_ctbl_da21nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -14, -1, 5, -7, -1, 4, -3, -3, 3, -4, -1, 0, -2, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da22nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -15, -2, 4, -8, -1, 4, -2, -3, 3, -4, -1, 0, -2, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da24nit[30] = {0, 0, 0, 0, 0, 0,-9, 1, -17, -3, 3, -8, -1, 4, -2, -5, 2, -5, -1, 0, -2, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da25nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -18, -3, 3, -8, -2, 4, -2, -5, 1, -5, -1, 0, -2, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da27nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -15, -5, 2, -9, -2, 4, -2, -4, 1, -5, 0, 0, -2, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da29nit[30] = {0, 0, 0, 0, 0, 0,-12, 1, -19, -5, 1, -9, -3, 3, -2, -4, 1, -6, 0, 0, -1, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da30nit[30] = {0, 0, 0, 0, 0, 0,-12, 1, -18, -6, 1, -9, -3, 3, -2, -3, 1, -5, -1, 0, -2, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da32nit[30] = {0, 0, 0, 0, 0, 0,-11, 0, -17, -6, 0, -10, -4, 2, -3, -4, 0, -5, -2, 0, -3, 1, 0, 1, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da34nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -17, -6, 0, -9, -5, 1, -3, -3, 0, -5, -2, 0, -3, 1, 0, 1, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da37nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -16, -5, 0, -8, -4, 2, -2, -3, 0, -5, -1, 0, -1, 1, 0, 1, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da39nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -15, -3, 2, -8, -1, 4, 1, -3, 0, -5, -1, 0, -1, 1, 0, 1, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da41nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -17, -5, 1, -8, 0, 3, 0, -3, 0, -4, -1, 0, -1, 0, 0, 0, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da44nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, -18, -5, 0, -8, -1, 2, -1, -3, 0, -4, -1, 0, -2, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da47nit[30] = {0, 0, 0, 0, 0, 0,-8, 1, -19, -4, 1, -6, 2, 4, 2, -3, 0, -4, -1, 0, -2, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da50nit[30] = {0, 0, 0, 0, 0, 0,-4, 6, -14, -5, 0, -7, 1, 3, 1, -3, 0, -4, 0, 0, -1, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da53nit[30] = {0, 0, 0, 0, 0, 0,-2, 6, -13, -1, 4, -4, 0, 2, 0, -3, 0, -4, 0, 0, -1, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da56nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -14, -3, 0, -6, 2, 4, 2, -2, 0, -3, 0, 0, 0, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da60nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -17, -3, 0, -5, 1, 3, 2, -2, 0, -3, 0, 0, 0, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da64nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -15, 0, 3, -4, -1, 1, 1, -1, 0, -2, 0, 0, 0, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da68nit[30] = {0, 0, 0, 0, 0, 0,-8, 1, -18, -1, 0, -4, 2, 4, 3, -1, 0, -2, 0, 0, 0, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da72nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -13, -2, 1, -4, 1, 2, 1, -1, 0, -2, 1, 0, 1, 1, 0, 1, 0, 0, 1, 4, 0, 3};
static signed char HA3_ctbl_da77nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, -16, 1, 3, -3, -1, 0, 0, -1, 0, -2, 0, 0, -1, 1, 0, 1, 1, 0, 1, 3, 0, 3};
static signed char HA3_ctbl_da82nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -15, -2, 1, -4, -1, 0, -1, -1, 0, -3, 1, 0, 0, 1, 0, 2, 1, 0, 1, 3, 0, 3};
static signed char HA3_ctbl_da87nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -13, 1, 3, -2, -1, 0, 0, 0, 0, -1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 3, 0, 3};
static signed char HA3_ctbl_da93nit[30] = {0, 0, 0, 0, 0, 0,-3, 6, -11, -1, 1, -3, -1, 0, 0, -1, 0, -2, 1, 0, 1, 1, 0, 1, 1, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da98nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -15, 2, 3, -1, 0, 0, -1, 0, 0, -1, 1, 0, 0, -1, 0, 1, 1, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da105nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -11, 2, 3, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 3, 0, 2};
static signed char HA3_ctbl_da111nit[30] = {0, 0, 0, 0, 0, 0,-2, 6, -9, -1, 1, -2, -1, 0, 0, 0, 0, -1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da119nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -8, -1, 1, -2, -1, 0, 0, 1, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da126nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, 1, 2, 0, 0, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da134nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -8, -1, 0, -2, -1, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da143nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -8, -1, 0, -2, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char HA3_ctbl_da152nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -6, -1, 2, -1, -1, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 3, 0, 1};
static signed char HA3_ctbl_da162nit[30] = {0, 0, 0, 0, 0, 0,-7, 0, -8, -3, 0, -2, -2, 0, 0, 0, 0, -1, 2, 0, 2, 0, 0, 0, 0, 0, 1, 3, 0, 1};
static signed char HA3_ctbl_da172nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -5, -2, 0, -2, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 4, 0, 2};
static signed char HA3_ctbl_da183nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -4, 0, 1, -1, -1, 0, 1, 0, 0, -1, 1, 0, 2, 0, 0, 0, 1, 0, 1, 2, 0, 1};
static signed char HA3_ctbl_da195nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -4, -1, 1, -1, -1, 0, 0, 0, 0, -1, 1, 0, 2, 0, 0, 0, 1, 0, 1, 2, 0, 1};
static signed char HA3_ctbl_da207nit[30] = {0, 0, 0, 0, 0, 0,-7, 1, -4, -2, 0, -2, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 2, 0, 1};
static signed char HA3_ctbl_da220nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -4, -2, 0, -2, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 2, 0, 1};
static signed char HA3_ctbl_da234nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -3, -2, 0, -2, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 2, 0, 1};
static signed char HA3_ctbl_da249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da350nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da357nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da365nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da372nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da380nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da387nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da395nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da403nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da412nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl_da420nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


//static signed char HA3_aid9802[3] = {0xB1, 0x90, 0xE1};
static signed char HA3_aid9694[3] = {0xB1, 0x90, 0xC5};
static signed char HA3_aid9593[3] = {0xB1, 0x90, 0xAB};
static signed char HA3_aid9473[3] = {0xB1, 0x90, 0x8C};
static signed char HA3_aid9364[3] = {0xB1, 0x90, 0x70};
static signed char HA3_aid9264[3] = {0xB1, 0x90, 0x56};
static signed char HA3_aid9178[3] = {0xB1, 0x90, 0x40};
static signed char HA3_aid9097[3] = {0xB1, 0x90, 0x2B};
static signed char HA3_aid9000[3] = {0xB1, 0x90, 0x12};
static signed char HA3_aid8919[3] = {0xB1, 0x80, 0xFD};
static signed char HA3_aid8833[3] = {0xB1, 0x80, 0xE7};
static signed char HA3_aid8756[3] = {0xB1, 0x80, 0xD3};
static signed char HA3_aid8682[3] = {0xB1, 0x80, 0xC0};
static signed char HA3_aid8605[3] = {0xB1, 0x80, 0xAC};
static signed char HA3_aid8519[3] = {0xB1, 0x80, 0x96};
//static signed char HA3_aid8453[3] = {0xB1, 0x80, 0x85};
static signed char HA3_aid8310[3] = {0xB1, 0x80, 0x60};
static signed char HA3_aid8275[3] = {0xB1, 0x80, 0x57};
static signed char HA3_aid8209[3] = {0xB1, 0x80, 0x46};
static signed char HA3_aid8128[3] = {0xB1, 0x80, 0x31};
static signed char HA3_aid7942[3] = {0xB1, 0x80, 0x01};
static signed char HA3_aid7857[3] = {0xB1, 0x70, 0xEB};
static signed char HA3_aid7678[3] = {0xB1, 0x70, 0xBD};
static signed char HA3_aid7488[3] = {0xB1, 0x70, 0x8C};
static signed char HA3_aid7399[3] = {0xB1, 0x70, 0x75};
static signed char HA3_aid7217[3] = {0xB1, 0x70, 0x46};
static signed char HA3_aid7035[3] = {0xB1, 0x70, 0x17};
static signed char HA3_aid6764[3] = {0xB1, 0x60, 0xD1};
static signed char HA3_aid6578[3] = {0xB1, 0x60, 0xA1};
static signed char HA3_aid6407[3] = {0xB1, 0x60, 0x75};
static signed char HA3_aid6143[3] = {0xB1, 0x60, 0x31};
static signed char HA3_aid5853[3] = {0xB1, 0x50, 0xE6};
static signed char HA3_aid5581[3] = {0xB1, 0x50, 0xA0};
static signed char HA3_aid5295[3] = {0xB1, 0x50, 0x56};
static signed char HA3_aid5027[3] = {0xB1, 0x50, 0x11};
static signed char HA3_aid4651[3] = {0xB1, 0x40, 0xB0};
static signed char HA3_aid4326[3] = {0xB1, 0x40, 0x5C};
static signed char HA3_aid3934[3] = {0xB1, 0x30, 0xF7};
static signed char HA3_aid3578[3] = {0xB1, 0x30, 0x9B};
static signed char HA3_aid3353[3] = {0xB1, 0x30, 0x61};
static signed char HA3_aid2829[3] = {0xB1, 0x20, 0xDA};
static signed char HA3_aid2419[3] = {0xB1, 0x20, 0x70};
static signed char HA3_aid1891[3] = {0xB1, 0x10, 0xE8};
static signed char HA3_aid1395[3] = {0xB1, 0x10, 0x68};
static signed char HA3_aid950[3] = {0xB1, 0x00, 0xF5};
static signed char HA3_aid744[3] = {0xB1, 0x00, 0xC0};
static signed char HA3_aid609[3] = {0xB1, 0x00, 0x9D};
static signed char HA3_aid422[3] = {0xB1, 0x00, 0x6D};
static signed char HA3_aid205[3] = {0xB1, 0x00, 0x35};



// ha3 da
static signed char HA3_rtbl_da2nit[10] = {0, 35, 33, 30, 25, 22, 18, 13, 8, 0};
static signed char HA3_rtbl_da3nit[10] = {0, 31, 29, 25, 21, 18, 15, 11, 7, 0};
static signed char HA3_rtbl_da4nit[10] = {0, 30, 28, 24, 20, 17, 15, 11, 7, 0};
static signed char HA3_rtbl_da5nit[10] = {0, 29, 27, 23, 19, 17, 15, 10, 7, 0};
static signed char HA3_rtbl_da6nit[10] = {0, 28, 26, 22, 18, 16, 15, 10, 7, 0};
static signed char HA3_rtbl_da7nit[10] = {0, 27, 25, 21, 18, 16, 15, 10, 7, 0};
static signed char HA3_rtbl_da8nit[10] = {0, 25, 24, 20, 17, 15, 14, 9, 7, 0};
static signed char HA3_rtbl_da9nit[10] = {0, 24, 22, 18, 15, 15, 13, 9, 6, 0};
static signed char HA3_rtbl_da10nit[10] = {0, 23, 21, 17, 14, 14, 12, 8, 6, 0};
static signed char HA3_rtbl_da11nit[10] = {0, 23, 21, 17, 13, 13, 12, 8, 6, 0};
static signed char HA3_rtbl_da12nit[10] = {0, 22, 20, 16, 13, 13, 11, 7, 6, 0};
static signed char HA3_rtbl_da13nit[10] = {0, 21, 19, 16, 13, 13, 11, 7, 6, 0};
static signed char HA3_rtbl_da14nit[10] = {0, 20, 18, 15, 12, 12, 10, 7, 6, 0};
static signed char HA3_rtbl_da15nit[10] = {0, 20, 18, 15, 12, 12, 10, 7, 6, 0};
static signed char HA3_rtbl_da16nit[10] = {0, 19, 17, 14, 11, 11, 10, 7, 6, 0};
static signed char HA3_rtbl_da17nit[10] = {0, 18, 16, 13, 11, 11, 9, 6, 5, 0};
static signed char HA3_rtbl_da19nit[10] = {0, 17, 15, 12, 10, 10, 8, 5, 5, 0};
static signed char HA3_rtbl_da20nit[10] = {0, 17, 15, 12, 9, 9, 8, 5, 5, 0};
static signed char HA3_rtbl_da21nit[10] = {0, 16, 14, 10, 8, 8, 8, 5, 5, 0};
static signed char HA3_rtbl_da22nit[10] = {0, 16, 14, 10, 8, 8, 8, 5, 5, 0};
static signed char HA3_rtbl_da24nit[10] = {0, 16, 14, 10, 8, 8, 7, 5, 5, 0};
static signed char HA3_rtbl_da25nit[10] = {0, 16, 14, 10, 8, 8, 7, 5, 5, 0};
static signed char HA3_rtbl_da27nit[10] = {0, 15, 13, 9, 7, 7, 6, 5, 5, 0};
static signed char HA3_rtbl_da29nit[10] = {0, 15, 13, 9, 7, 7, 6, 5, 5, 0};
static signed char HA3_rtbl_da30nit[10] = {0, 14, 13, 9, 7, 7, 6, 4, 5, 0};
static signed char HA3_rtbl_da32nit[10] = {0, 14, 13, 9, 7, 7, 6, 4, 5, 0};
static signed char HA3_rtbl_da34nit[10] = {0, 14, 12, 9, 7, 7, 6, 4, 5, 0};
static signed char HA3_rtbl_da37nit[10] = {0, 13, 11, 8, 6, 6, 5, 4, 5, 0};
static signed char HA3_rtbl_da39nit[10] = {0, 12, 10, 7, 5, 6, 5, 4, 4, 0};
static signed char HA3_rtbl_da41nit[10] = {0, 12, 10, 7, 5, 5, 5, 3, 4, 0};
static signed char HA3_rtbl_da44nit[10] = {0, 11, 10, 7, 5, 5, 4, 3, 4, 0};
static signed char HA3_rtbl_da47nit[10] = {0, 10, 9, 6, 4, 5, 4, 3, 4, 0};
static signed char HA3_rtbl_da50nit[10] = {0, 10, 8, 6, 4, 5, 4, 3, 4, 0};
static signed char HA3_rtbl_da53nit[10] = {0, 9, 7, 5, 4, 4, 4, 3, 4, 0};
static signed char HA3_rtbl_da56nit[10] = {0, 9, 7, 5, 3, 4, 3, 3, 4, 0};
static signed char HA3_rtbl_da60nit[10] = {0, 9, 7, 5, 3, 4, 3, 3, 4, 0};
static signed char HA3_rtbl_da64nit[10] = {0, 8, 6, 4, 3, 3, 3, 3, 4, 0};
static signed char HA3_rtbl_da68nit[10] = {0, 8, 6, 4, 2, 3, 3, 3, 4, 0};
static signed char HA3_rtbl_da72nit[10] = {0, 7, 5, 4, 2, 3, 3, 3, 4, 0};
static signed char HA3_rtbl_da77nit[10] = {0, 8, 6, 3, 3, 3, 3, 2, 3, 0};
static signed char HA3_rtbl_da82nit[10] = {0, 8, 6, 4, 3, 2, 2, 2, 2, 0};
static signed char HA3_rtbl_da87nit[10] = {0, 7, 5, 3, 2, 1, 2, 2, 2, 0};
static signed char HA3_rtbl_da93nit[10] = {0, 7, 4, 3, 2, 2, 3, 2, 2, 0};
static signed char HA3_rtbl_da98nit[10] = {0, 7, 5, 3, 3, 3, 3, 4, 3, 0};
static signed char HA3_rtbl_da105nit[10] = {0, 7, 4, 2, 2, 2, 3, 3, 2, 0};
static signed char HA3_rtbl_da111nit[10] = {0, 7, 4, 3, 2, 3, 4, 4, 2, 0};
static signed char HA3_rtbl_da119nit[10] = {0, 6, 4, 3, 2, 2, 4, 4, 1, 0};
static signed char HA3_rtbl_da126nit[10] = {0, 6, 3, 2, 2, 2, 4, 4, 1, 0};
static signed char HA3_rtbl_da134nit[10] = {0, 5, 3, 3, 3, 3, 4, 4, 2, 0};
static signed char HA3_rtbl_da143nit[10] = {0, 5, 4, 4, 4, 4, 4, 4, 1, 0};
static signed char HA3_rtbl_da152nit[10] = {0, 4, 3, 2, 2, 3, 3, 5, 2, 0};
static signed char HA3_rtbl_da162nit[10] = {0, 4, 3, 2, 2, 2, 3, 5, 2, 0};
static signed char HA3_rtbl_da172nit[10] = {0, 4, 2, 2, 2, 2, 3, 4, 2, 0};
static signed char HA3_rtbl_da183nit[10] = {0, 4, 2, 1, 2, 2, 2, 3, 1, 0};
static signed char HA3_rtbl_da195nit[10] = {0, 4, 2, 1, 2, 1, 2, 3, 1, 0};
static signed char HA3_rtbl_da207nit[10] = {0, 3, 2, 1, 1, 1, 2, 3, 1, 0};
static signed char HA3_rtbl_da220nit[10] = {0, 3, 1, 1, 1, 1, 2, 2, 1, 0};
static signed char HA3_rtbl_da234nit[10] = {0, 2, 1, 1, 1, 1, 2, 2, 1, 0};
static signed char HA3_rtbl_da249nit[10] = {0, 2, 1, 1, 1, 2, 2, 3, 1, 0};
static signed char HA3_rtbl_da265nit[10] = {0, 2, 1, 1, 1, 1, 1, 2, 1, 0};
static signed char HA3_rtbl_da282nit[10] = {0, 2, 0, 0, 1, 1, 1, 2, 1, 0};
static signed char HA3_rtbl_da300nit[10] = {0, 1, 0, 1, 1, 1, 1, 1, 1, 0};
static signed char HA3_rtbl_da316nit[10] = {0, 1, 0, 0, 1, 0, 1, 1, 1, 0};
static signed char HA3_rtbl_da333nit[10] = {0, 1, 0, 0, 0, 0, 1, 1, 0, 0};
static signed char HA3_rtbl_da350nit[10] = {0, 2, 0, -1, -1, -1, 0, 0, 0, 0};
static signed char HA3_rtbl_da357nit[10] = {0, 1, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HA3_rtbl_da365nit[10] = {0, 0, -1, 0, 0, 0, 0, 1, 1, 0};
static signed char HA3_rtbl_da372nit[10] = {0, 0, -1, -1, -1, -1, -1, 0, 0, 0};
static signed char HA3_rtbl_da380nit[10] = {0, 0, -1, -1, -1, -1, -1, 0, 0, 0};
static signed char HA3_rtbl_da387nit[10] = {0, 0, -1, -1, -1, -1, -1, 0, 0, 0};
static signed char HA3_rtbl_da395nit[10] = {0, 0, -1, -1, -1, -1, -1, 0, 0, 0};
static signed char HA3_rtbl_da403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl_da412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl_da420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static  unsigned char HA3_da_elv1[3] = {
	0xAC, 0x1A
};
static  unsigned char HA3_da_elv2[3] = {
	0xAC, 0x19
};
static  unsigned char HA3_da_elv3[3] = {
	0xAC, 0x18
};
static  unsigned char HA3_da_elv4[3] = {
	0xAC, 0x17
};
static  unsigned char HA3_da_elv5[3] = {
	0xAC, 0x16
};
static  unsigned char HA3_da_elv6[3] = {
	0xAC, 0x15
};
static  unsigned char HA3_da_elv7[3] = {
	0xAC, 0x14
};
static  unsigned char HA3_da_elv8[3] = {
	0xAC, 0x13
};
static  unsigned char HA3_da_elv9[3] = {
	0xAC, 0x12
};
static  unsigned char HA3_da_elv10[3] = {
	0xAC, 0x11
};
static  unsigned char HA3_da_elv11[3] = {
	0xAC, 0x10
};
static	unsigned char HA3_da_elv12[3] = {
	0xAC, 0x0F
};
static  unsigned char HA3_da_elv13[3] = {
	0xAC, 0x0E
};
static  unsigned char HA3_da_elv14[3] = {
	0xAC, 0x0D
};
static  unsigned char HA3_da_elv15[3] = {
	0xAC, 0x0C
};
static  unsigned char HA3_da_elv16[3] = {
	0xAC, 0x0B
};
static  unsigned char HA3_da_elv17[3] = {
	0xAC, 0x0A
};


static  unsigned char HA3_da_elvCaps1[3] = {
	0xBC, 0x1A
};
static  unsigned char HA3_da_elvCaps2[3] = {
	0xBC, 0x19
};
static  unsigned char HA3_da_elvCaps3[3] = {
	0xBC, 0x18
};
static  unsigned char HA3_da_elvCaps4[3] = {
	0xBC, 0x17
};
static  unsigned char HA3_da_elvCaps5[3] = {
	0xBC, 0x16
};
static  unsigned char HA3_da_elvCaps6[3] = {
	0xBC, 0x15
};
static  unsigned char HA3_da_elvCaps7[3] = {
	0xBC, 0x14
};
static  unsigned char HA3_da_elvCaps8[3] = {
	0xBC, 0x13
};
static  unsigned char HA3_da_elvCaps9[3] = {
	0xBC, 0x12
};
static  unsigned char HA3_da_elvCaps10[3] = {
	0xBC, 0x11
};
static  unsigned char HA3_da_elvCaps11[3] = {
	0xBC, 0x10
};
static	unsigned char HA3_da_elvCaps12[3] = {
	0xBC, 0x0F
};
static  unsigned char HA3_da_elvCaps13[3] = {
	0xBC, 0x0E
};
static  unsigned char HA3_da_elvCaps14[3] = {
	0xBC, 0x0D
};
static  unsigned char HA3_da_elvCaps15[3] = {
	0xBC, 0x0C
};
static  unsigned char HA3_da_elvCaps16[3] = {
	0xBC, 0x0B
};
static  unsigned char HA3_da_elvCaps17[3] = {
	0xBC, 0x0A
};


/// hf4
static signed char rtbl2nit[10] = {0, 9, 13, 20, 18, 18, 13, 9, 7, 3};
static signed char rtbl3nit[10] = {0, 12, 15, 24, 22, 18, 13, 8, 6, 1};
static signed char rtbl4nit[10] = {0, 13, 16, 20, 19, 16, 12, 7, 4, 1};
static signed char rtbl5nit[10] = {0, 11, 15, 18, 16, 14, 10, 5, 3, 1};
static signed char rtbl6nit[10] = {0, 8, 14, 18, 16, 14, 11, 6, 4, 1};
static signed char rtbl7nit[10] = {0, 9, 16, 17, 15, 13, 9, 5, 4, 1};
static signed char rtbl8nit[10] = {0, 9, 15, 16, 14, 12, 9, 5, 4, 1};
static signed char rtbl9nit[10] = {0, 9, 16, 15, 13, 11, 8, 4, 3, 1};
static signed char rtbl10nit[10] = {0, 10, 17, 15, 13, 11, 7, 4, 3, 1};
static signed char rtbl11nit[10] = {0, 11, 17, 14, 12, 10, 6, 3, 3, 1};
static signed char rtbl12nit[10] = {0, 11, 17, 14, 12, 10, 6, 3, 3, 1};
static signed char rtbl13nit[10] = {0, 11, 16, 13, 11, 9, 6, 3, 3, 0};
static signed char rtbl14nit[10] = {0, 15, 16, 13, 11, 9, 6, 3, 3, 0};
static signed char rtbl15nit[10] = {0, 14, 15, 11, 9, 7, 4, 1, 2, 0};
static signed char rtbl16nit[10] = {0, 14, 15, 11, 9, 7, 4, 2, 3, 0};
static signed char rtbl17nit[10] = {0, 13, 14, 11, 9, 7, 4, 1, 3, 0};
static signed char rtbl19nit[10] = {0, 12, 13, 9, 8, 6, 3, 1, 2, 0};
static signed char rtbl20nit[10] = {0, 11, 12, 8, 7, 5, 2, 1, 1, 0};
static signed char rtbl21nit[10] = {0, 11, 12, 8, 7, 5, 2, 0, 1, 0};
static signed char rtbl22nit[10] = {0, 11, 12, 8, 7, 5, 2, 1, 0, 0};
static signed char rtbl24nit[10] = {0, 12, 12, 8, 7, 5, 2, 1, 0, 0};
static signed char rtbl25nit[10] = {0, 12, 12, 8, 7, 5, 2, 1, 3, 0};
static signed char rtbl27nit[10] = {0, 11, 11, 7, 5, 4, 1, 0, 0, 0};
static signed char rtbl29nit[10] = {0, 12, 11, 7, 6, 4, 2, 1, 0, 0};
static signed char rtbl30nit[10] = {0, 9, 10, 6, 4, 3, 1, -1, -1, 0};
static signed char rtbl32nit[10] = {0, 10, 10, 6, 5, 3, 1, 1, 2, 0};
static signed char rtbl34nit[10] = {0, 9, 8, 5, 4, 3, 1, 0, 1, 0};
static signed char rtbl37nit[10] = {0, 10, 9, 5, 4, 3, 1, 0, 1, 0};
static signed char rtbl39nit[10] = {0, 8, 8, 5, 4, 3, 1, 0, 1, 0};
static signed char rtbl41nit[10] = {0, 10, 8, 5, 4, 3, 1, 0, 0, 0};
static signed char rtbl44nit[10] = {0, 10, 8, 5, 4, 3, 1, 0, 1, 0};
static signed char rtbl47nit[10] = {0, 10, 7, 4, 3, 2, 0, 1, 1, 0};
static signed char rtbl50nit[10] = {0, 6, 7, 4, 3, 2, 1, 0, 3, 0};
static signed char rtbl53nit[10] = {0, 6, 6, 3, 3, 2, 0, -1, 1, 0};
static signed char rtbl56nit[10] = {0, 7, 6, 3, 3, 2, 1, 0, 0, 0};
static signed char rtbl60nit[10] = {0, 6, 5, 3, 3, 2, 0, 0, 0, 0};
static signed char rtbl64nit[10] = {0, 6, 4, 2, 2, 1, 0, -1, 0, 0};
static signed char rtbl68nit[10] = {0, 2, 3, 2, 1, 1, 0, -1, 1, 0};
static signed char rtbl72nit[10] = {0, 4, 4, 4, 3, 2, 2, 1, 4, 0};
static signed char rtbl77nit[10] = {0, 5, 5, 3, 2, 1, 2, 0, 1, 0};
static signed char rtbl82nit[10] = {0, 6, 5, 3, 2, 1, 1, 1, 1, 0};
static signed char rtbl87nit[10] = {0, 6, 5, 3, 2, 1, 2, 2, 4, 0};
static signed char rtbl93nit[10] = {0, 6, 5, 3, 2, 2, 2, 3, 2, 0};
static signed char rtbl98nit[10] = {0, 5, 4, 3, 3, 2, 3, 3, 2, 0};
static signed char rtbl105nit[10] = {0, 5, 4, 3, 2, 2, 4, 4, 6, 0};
static signed char rtbl111nit[10] = {0, 5, 4, 3, 3, 2, 3, 4, 4, 0};
static signed char rtbl119nit[10] = {0, 5, 4, 3, 2, 2, 3, 5, 4, 0};
static signed char rtbl126nit[10] = {0, 5, 4, 2, 2, 2, 3, 4, 3, 0};
static signed char rtbl134nit[10] = {0, 4, 3, 2, 3, 3, 4, 5, 5, 0};
static signed char rtbl143nit[10] = {0, 5, 4, 3, 3, 3, 4, 7, 6, 0};
static signed char rtbl152nit[10] = {0, 5, 4, 3, 3, 4, 4, 7, 6, 0};
static signed char rtbl162nit[10] = {0, 5, 4, 3, 3, 3, 4, 8, 5, 0};
static signed char rtbl172nit[10] = {0, 6, 4, 3, 3, 4, 5, 9, 5, 0};
static signed char rtbl183nit[10] = {0, 6, 4, 3, 2, 2, 4, 6, 6, 0};
static signed char rtbl195nit[10] = {0, 4, 3, 3, 2, 2, 3, 7, 6, 0};
static signed char rtbl207nit[10] = {0, 4, 3, 2, 2, 2, 3, 7, 6, 0};
static signed char rtbl220nit[10] = {0, 4, 3, 3, 2, 2, 3, 6, 2, 0};
static signed char rtbl234nit[10] = {0, 4, 2, 2, 2, 2, 3, 7, 5, 0};
static signed char rtbl249nit[10] = {0, 3, 2, 1, 1, 2, 2, 8, 2, 0};
static signed char rtbl265nit[10] = {0, 2, 2, 1, 1, 2, 3, 5, 2, 0};
static signed char rtbl282nit[10] = {0, 1, 1, 1, 1, 1, 1, 5, 2, 0};
static signed char rtbl300nit[10] = {0, 2, 1, 1, 1, 2, 3, 5, 4, 0};
static signed char rtbl316nit[10] = {0, 2, 1, 0, 0, 1, 1, 4, 1, 0};
static signed char rtbl333nit[10] = {0, 1, 1, 1, 0, 1, 0, 5, 0, 0};
static signed char rtbl350nit[10] = {0, 2, 1, 1, 0, 1, 0, 2, 2, 0};
static signed char rtbl357nit[10] = {0, 1, 0, 0, 0, -1, 0, 4, 3, 0};
static signed char rtbl365nit[10] = {0, 2, 0, 1, 0, 1, 0, 3, -2, 0};
static signed char rtbl372nit[10] = {0, 1, 0, 0, 0, 0, 0, 6, 1, 0};
static signed char rtbl380nit[10] = {0, 1, 0, 1, 0, 1, 1, 4, 0, 0};
static signed char rtbl387nit[10] = {0, 1, 0, 0, 0, 0, 0, 5, 0, 0};
static signed char rtbl395nit[10] = {0, 1, 0, -1, -1, -1, -2, 3, 0, 0};
static signed char rtbl403nit[10] = {0, 3, 1, 1, 1, 0, 0, 2, 0, 0};
static signed char rtbl412nit[10] = {0, -1, -1, 0, 0, 0, 0, 2, -1, 0};
static signed char rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char ctbl2nit[30] = {0, 0, 0, -4, 6, -5, -3, 8, -3, 1, 2, -6, 4, 13, 1, -8, 6, -8, -5, 5, -1, -2, 2, -1, -1, 1, -1, -9, 0, -9};
static signed char ctbl3nit[30] = {0, 0, 0, -11, 0, -12, -4, 8, -4, -4, 0, -8, -11, -1, -11, -13, 2, -10, -7, 2, -5, -2, 1, -2, -2, 1, -2, -7, 0, -7};
static signed char ctbl4nit[30] = {0, 0, 0, -11, 0, -12, -13, -1, -13, -3, 2, -7, -10, 0, -10, -12, 0, -10, -8, 1, -5, -3, 0, -3, 0, 1, 0, -5, 1, -5};
static signed char ctbl5nit[30] = {0, 0, 0, -8, 1, -9, -10, 0, -10, -5, 1, -10, -10, 1, -9, -12, 0, -9, -7, 1, -5, -3, 0, -3, 0, 1, 0, -4, 1, -4};
static signed char ctbl6nit[30] = {0, 0, 0, 0, 7, -1, -1, 8, 0, -8, -1, -12, -11, 1, -9, -11, 1, -9, -7, 0, -6, -3, 0, -3, -1, 0, 0, -4, 0, -5};
static signed char ctbl7nit[30] = {0, 0, 0, 1, 7, -1, -9, 0, -8, -7, 1, -11, -12, -1, -11, -11, 0, -9, -5, 1, -4, -3, 0, -3, 0, 0, 0, -4, 0, -4};
static signed char ctbl8nit[30] = {0, 0, 0, -7, -1, -8, 1, 8, 1, -10, 0, -14, -9, 0, -10, -10, 0, -8, -5, 0, -5, -3, 0, -2, 0, 0, 0, -3, 0, -4};
static signed char ctbl9nit[30] = {0, 0, 0, -4, 0, -6, -5, 1, -6, -9, 0, -13, -7, 2, -10, -9, 0, -8, -6, 1, -5, -2, 0, -2, 0, 0, 0, -2, 0, -3};
static signed char ctbl10nit[30] = {0, 0, 0, -2, 1, -4, -5, 0, -7, -8, 0, -12, -9, 1, -11, -11, -1, -9, -4, 0, -4, -2, 0, -1, 0, 0, -1, -2, 0, -2};
static signed char ctbl11nit[30] = {0, 0, 0, -4, 0, -5, -5, -1, -8, -8, 0, -13, -8, 1, -11, -9, -1, -8, -3, 1, -2, -2, 0, -2, 0, 0, 0, -1, 0, -2};
static signed char ctbl12nit[30] = {0, 0, 0, 0, 3, -1, -2, 2, -12, -8, 0, -12, -8, 1, -12, -9, -1, -8, -4, 0, -3, -1, 0, -1, 0, 0, 0, -1, 0, -2};
static signed char ctbl13nit[30] = {0, 0, 0, 1, 2, 0, -2, 0, -15, -8, 0, -12, -7, 1, -11, -9, 0, -8, -3, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, -2};
static signed char ctbl14nit[30] = {0, 0, 0, 13, -11, 13, -4, 2, -17, -10, -1, -14, -6, 1, -11, -9, 0, -8, -3, 0, -2, -1, 0, -1, -1, 0, -2, 0, 0, -1};
static signed char ctbl15nit[30] = {0, 0, 0, 18, -7, 17, -7, -2, -20, -11, -1, -15, -4, 2, -9, -7, 1, -5, -3, 1, -2, 1, 0, 1, 0, 0, 0, 0, 0, -1};
static signed char ctbl16nit[30] = {0, 0, 0, 21, -1, 20, -6, -2, -19, -12, -1, -16, -5, 1, -10, -7, 0, -6, -1, 2, 0, -1, 0, -1, 0, 0, 0, 0, 0, -2};
static signed char ctbl17nit[30] = {0, 0, 0, 19, -2, 16, -4, 3, -16, -11, -1, -16, -5, 1, -10, -7, -1, -6, -4, 0, -2, 0, 0, -1, 0, 1, 0, 0, 0, -1};
static signed char ctbl19nit[30] = {0, 0, 0, 17, -2, 15, -8, -1, -19, -7, 2, -13, -5, 1, -9, -7, -1, -6, -2, 1, -1, 0, 0, 0, 0, 1, 0, 1, 0, -1};
static signed char ctbl20nit[30] = {0, 0, 0, 18, -1, 15, -8, 1, -20, -7, 2, -11, -5, 2, -8, -5, 0, -5, 0, 2, 0, 0, -1, 0, 0, 1, 1, 1, 0, -1};
static signed char ctbl21nit[30] = {0, 0, 0, 22, 2, 18, -11, -1, -23, -6, 3, -12, -7, 0, -9, -5, 1, -5, -1, 1, -1, 0, 1, 1, 0, 0, -1, 1, 0, 0};
static signed char ctbl22nit[30] = {0, 0, 0, -14, 3, -13, -14, 2, -22, -7, 3, -11, -7, 0, -9, -5, 1, -5, 0, 1, 0, 0, -1, 0, -1, 0, 0, 1, 0, -1};
static signed char ctbl24nit[30] = {0, 0, 0, -16, 11, -12, -12, 2, -19, -9, 1, -14, -8, 0, -9, -5, -1, -5, -1, 1, 0, 0, -1, -1, 0, 0, 1, 1, 0, -1};
static signed char ctbl25nit[30] = {0, 0, 0, -20, -1, -14, -15, 0, -24, -9, 0, -13, -8, 0, -9, -6, -1, -6, 0, 1, 1, 1, -1, -1, -1, 1, 0, 1, 0, -1};
static signed char ctbl27nit[30] = {0, 0, 0, -16, 3, -6, -15, -1, -25, -11, -2, -15, -4, 2, -6, -4, 0, -3, -1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, -1};
static signed char ctbl29nit[30] = {0, 0, 0, -19, 1, -7, -13, 0, -23, -10, 0, -13, -7, -1, -9, -4, 0, -3, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0};
static signed char ctbl30nit[30] = {0, 0, 0, -13, -1, -11, -13, -1, -24, -11, -2, -15, -4, 2, -6, -1, 3, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, -1};
static signed char ctbl32nit[30] = {0, 0, 0, -15, -1, -13, -15, -2, -27, -10, -1, -13, -8, -1, -10, -1, 2, 0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 1, 0, 0};
static signed char ctbl34nit[30] = {0, 0, 0, -16, -2, -11, -10, 3, -22, -7, 3, -7, -4, 1, -6, -1, 2, 0, 0, 1, 1, -1, 0, -1, 1, 0, 0, 1, 0, 0};
static signed char ctbl37nit[30] = {0, 0, 0, -13, 2, -8, -18, -2, -26, -9, 0, -10, -4, 1, -6, -2, 1, -1, 0, 1, 0, 0, -1, 0, 0, 0, 1, 1, 0, -1};
static signed char ctbl39nit[30] = {0, 0, 0, -16, 2, -13, -12, 2, -23, -8, 0, -9, -5, 1, -7, -2, 0, -1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, -1, -1};
static signed char ctbl41nit[30] = {0, 0, 0, -17, 0, -13, -14, 1, -25, -9, -1, -9, -4, 1, -6, -2, 0, -1, -1, 0, -1, 0, 0, 1, 0, 0, 0, 2, 0, 0};
static signed char ctbl44nit[30] = {0, 0, 0, -17, 0, -14, -14, -1, -24, -9, -1, -9, -5, 0, -7, -1, 0, -1, -1, -1, 0, 0, 1, 0, 1, 0, 1, 0, -1, -2};
static signed char ctbl47nit[30] = {0, 0, 0, -15, 0, -12, -15, -1, -26, -6, -1, -7, -5, -1, -6, -1, 1, 0, 1, 1, 1, 0, -1, 1, 1, 0, 0, 1, 0, 0};
static signed char ctbl50nit[30] = {0, 0, 0, -9, 0, -7, -17, -2, -27, -6, -1, -7, -5, -1, -7, 0, 1, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char ctbl53nit[30] = {0, 0, 0, -10, 1, -8, -16, 1, -23, -2, 2, -4, -6, -1, -6, -1, -1, -1, 1, 1, 2, 1, 0, 0, 0, 0, 0, 2, 0, 0};
static signed char ctbl56nit[30] = {0, 0, 0, -4, 0, -11, -19, -3, -26, 0, 4, -1, -5, -1, -6, 0, 0, 1, 0, 0, 0, -1, -1, 0, 1, 0, 1, 1, 0, -1};
static signed char ctbl60nit[30] = {0, 0, 0, 0, 2, -6, -13, 3, -21, -2, 2, -4, -7, -3, -7, 0, -1, 0, 0, 1, 1, 0, -1, 0, 1, 0, 0, 1, 0, 0};
static signed char ctbl64nit[30] = {0, 0, 0, -3, 1, -10, -15, 2, -24, -4, 1, -5, -3, -1, -5, 2, 2, 2, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, -1};
static signed char ctbl68nit[30] = {0, 0, 0, 4, 3, 1, -6, 5, -14, -1, 2, -2, 1, 3, -1, 1, 1, 2, 0, 0, 1, 0, -1, -1, 1, 0, 1, 2, 0, 1};
static signed char ctbl72nit[30] = {0, 0, 0, -3, -2, -9, -1, 14, -7, -7, -2, -8, -1, -1, -4, -2, -1, -1, 0, -1, 0, 0, -1, 1, 1, 0, 0, 2, 0, 1};
static signed char ctbl77nit[30] = {0, 0, 0, 4, 4, 0, -12, 3, -17, -5, 0, -4, -2, -1, -5, 1, 1, 1, 0, -1, 1, 1, 0, -1, 0, 0, 1, 2, 0, 0};
static signed char ctbl82nit[30] = {0, 0, 0, -1, 2, -5, -13, 1, -20, -2, 2, -2, -2, -1, -5, -2, -1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 2, 0, -1};
static signed char ctbl87nit[30] = {0, 0, 0, -4, 0, -8, -17, -2, -23, 1, 4, 1, -2, -1, -4, 1, 2, 2, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0};
static signed char ctbl93nit[30] = {0, 0, 0, -2, 4, -6, -18, -3, -24, -3, 0, -4, -1, 0, -3, 0, -1, -1, 0, 0, 2, 1, 0, 0, 0, 0, 0, 2, 0, 0};
static signed char ctbl98nit[30] = {0, 0, 0, -1, 3, -5, -14, 3, -17, 1, 4, 0, -2, -1, -4, 0, 0, 0, 0, -1, 1, -1, 0, 0, 0, 0, 1, 2, 0, 0};
static signed char ctbl105nit[30] = {0, 0, 0, 2, 6, -1, -16, 0, -20, 0, 3, 0, 0, 1, -1, 1, 1, 1, 0, 0, 0, 0, 0, 1, -1, 0, 0, 2, -1, -1};
static signed char ctbl111nit[30] = {0, 0, 0, -1, 4, -4, -12, 3, -16, -2, 0, -3, -1, 0, -2, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 1, -1, -1};
static signed char ctbl119nit[30] = {0, 0, 0, -3, 1, -7, -10, 4, -11, -4, 0, -4, 1, 2, 0, 1, 0, 2, 0, -1, 0, 1, 0, 1, -1, 0, 0, 1, -1, -1};
static signed char ctbl126nit[30] = {0, 0, 0, -7, 2, -10, -11, 0, -14, 2, 3, 1, 0, 1, -1, 0, -1, -1, 0, -1, 1, 0, 1, 0, 0, 1, 1, 1, -1, -1};
static signed char ctbl134nit[30] = {0, 0, 0, -3, 1, -7, -9, 4, -10, 1, 4, 1, 0, 0, -1, -1, -1, -1, 0, -1, 0, 0, 0, 1, 0, 1, 0, 2, -1, 0};
static signed char ctbl143nit[30] = {0, 0, 0, -6, 2, -9, -10, 1, -12, 0, 2, -1, -2, -1, -2, 0, 0, 0, 1, -1, 1, 0, 0, 1, -1, 0, 0, 1, 0, -1};
static signed char ctbl152nit[30] = {0, 0, 0, -5, 5, -6, -8, 5, -9, -1, 0, -3, -1, -1, -2, -1, -1, -1, 0, -1, 1, 0, 0, 0, 0, 0, 1, 2, 0, 0};
static signed char ctbl162nit[30] = {0, 0, 0, -7, 2, -9, -10, 2, -11, 0, 2, 0, -1, -1, -3, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 3, 0, 0};
static signed char ctbl172nit[30] = {0, 0, 0, -9, 2, -9, -11, -1, -13, -1, 0, -3, 0, 0, -1, 0, 0, 1, 1, 0, 1, -1, 0, 0, 0, 0, 1, 1, -1, 0};
static signed char ctbl183nit[30] = {0, 0, 0, -8, 0, -10, -9, -1, -10, -3, -1, -4, 0, 0, -1, 1, 1, 1, 1, -1, 1, 0, 1, 1, 0, 0, 0, 2, -1, -1};
static signed char ctbl195nit[30] = {0, 0, 0, 0, 9, 0, -6, 2, -8, 0, 1, -1, 0, 0, 0, 2, 1, 1, 0, -1, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0};
static signed char ctbl207nit[30] = {0, 0, 0, -3, 5, -3, -11, 0, -10, 1, 3, 0, 1, 0, 0, 0, 0, -1, 1, -1, 1, 0, 0, 1, -1, 0, 0, 3, 0, 0};
static signed char ctbl220nit[30] = {0, 0, 0, -5, 2, -6, -8, 0, -8, -1, 0, -2, 0, -1, -1, 0, 0, -1, 1, -1, 1, -1, 1, 1, 0, 0, 0, 2, -1, 0};
static signed char ctbl234nit[30] = {0, 0, 0, -8, -1, -7, -10, 0, -9, 2, 2, 0, 0, -1, -1, -1, -1, 0, 2, 0, 0, -1, 0, 1, 0, 0, 0, 1, -1, -1};
static signed char ctbl249nit[30] = {0, 0, 0, -2, 1, -4, -12, -2, -9, 1, 0, -1, 2, 2, 2, 1, 1, 1, 0, -1, 0, 0, 0, 1, 0, 0, 0, 1, 0, -1};
static signed char ctbl265nit[30] = {0, 0, 0, -1, 0, -4, -9, -1, -6, 1, 3, 1, 1, 1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 1, 0, 0, -1, 2, 0, 0};
static signed char ctbl282nit[30] = {0, 0, 0, 3, 3, 0, -5, 2, -2, -2, 0, -2, 2, 2, 1, 1, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, 1, 1, 0, -1};
static signed char ctbl300nit[30] = {0, 0, 0, -4, 2, -4, -4, -1, -3, 1, 1, 1, 2, 2, 2, 1, 1, 1, 0, -1, 0, -1, 0, 0, 0, 0, 0, 1, -1, -1};
static signed char ctbl316nit[30] = {0, 0, 0, -7, -1, -6, -6, -2, -4, 1, 0, -1, 2, 2, 2, 1, 1, 1, 0, 0, 0, -1, 0, 0, 0, 0, 1, 1, 0, -1};
static signed char ctbl333nit[30] = {0, 0, 0, -2, 3, -2, -5, -1, -3, 0, -1, -1, 1, 1, 1, -1, -1, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char ctbl350nit[30] = {0, 0, 0, -7, 1, -3, -6, 0, -2, -1, -1, -2, 0, -1, -1, 1, 1, 1, -1, -1, -1, 0, 0, 1, 0, 0, 0, 1, 0, 0};
static signed char ctbl357nit[30] = {0, 0, 0, -2, 3, 1, -5, -1, -2, 2, 1, 0, -1, -1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1};
static signed char ctbl365nit[30] = {0, 0, 0, -7, -1, -3, -4, 0, -1, -1, -1, -2, 1, 0, 2, 0, 0, -2, 0, -1, 1, -1, 0, 0, 0, 0, -1, 1, 0, 1};
static signed char ctbl372nit[30] = {0, 0, 0, -2, 3, 1, -4, 0, -2, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, -1, 0, 1, 0, 0, -1, 0, -1, -1};
static signed char ctbl380nit[30] = {0, 0, 0, -3, 3, -1, -2, 1, 0, -1, -2, -2, 1, 1, 1, 0, 0, 0, 1, 0, 1, -1, 0, 1, 0, 0, -1, 0, -1, -1};
static signed char ctbl387nit[30] = {0, 0, 0, -6, 0, -3, -3, 0, -1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0};
static signed char ctbl395nit[30] = {0, 0, 0, 0, 2, 1, -4, -1, -2, 0, -1, -1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0};
static signed char ctbl403nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl412nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl420nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char aid9795[3] = {0xB1, 0x90, 0xE3};
static signed char aid9710[3] = {0xB1, 0x90, 0xCD};
static signed char aid9598[3] = {0xB1, 0x90, 0xB0};
static signed char aid9485[3] = {0xB1, 0x90, 0x93};
static signed char aid9400[3] = {0xB1, 0x90, 0x7D};
static signed char aid9292[3] = {0xB1, 0x90, 0x61};
static signed char aid9214[3] = {0xB1, 0x90, 0x4D};
static signed char aid9106[3] = {0xB1, 0x90, 0x31};
static signed char aid9029[3] = {0xB1, 0x90, 0x1D};
static signed char aid8920[3] = {0xB1, 0x90, 0x01};
static signed char aid8843[3] = {0xB1, 0x80, 0xED};
static signed char aid8735[3] = {0xB1, 0x80, 0xD1};
static signed char aid8657[3] = {0xB1, 0x80, 0xBD};
static signed char aid8549[3] = {0xB1, 0x80, 0xA1};
static signed char aid8471[3] = {0xB1, 0x80, 0x8D};
static signed char aid8355[3] = {0xB1, 0x80, 0x6F};
static signed char aid8170[3] = {0xB1, 0x80, 0x3F};
static signed char aid8061[3] = {0xB1, 0x80, 0x23};
static signed char aid7972[3] = {0xB1, 0x80, 0x0C};
static signed char aid7860[3] = {0xB1, 0x70, 0xEF};
static signed char aid7663[3] = {0xB1, 0x70, 0xBC};
static signed char aid7558[3] = {0xB1, 0x70, 0xA1};
static signed char aid7357[3] = {0xB1, 0x70, 0x6D};
static signed char aid7132[3] = {0xB1, 0x70, 0x33};
static signed char aid7051[3] = {0xB1, 0x70, 0x1E};
static signed char aid6823[3] = {0xB1, 0x60, 0xE3};
static signed char aid6629[3] = {0xB1, 0x60, 0xB1};
static signed char aid6327[3] = {0xB1, 0x60, 0x63};
static signed char aid6118[3] = {0xB1, 0x60, 0x2D};
static signed char aid5898[3] = {0xB1, 0x50, 0xF4};
static signed char aid5577[3] = {0xB1, 0x50, 0xA1};
static signed char aid5263[3] = {0xB1, 0x50, 0x50};
static signed char aid4957[3] = {0xB1, 0x50, 0x01};
static signed char aid4636[3] = {0xB1, 0x40, 0xAE};
static signed char aid4319[3] = {0xB1, 0x40, 0x5C};
static signed char aid3874[3] = {0xB1, 0x30, 0xE9};
static signed char aid3483[3] = {0xB1, 0x30, 0x84};
static signed char aid3050[3] = {0xB1, 0x30, 0x14};
static signed char aid2612[3] = {0xB1, 0x20, 0xA3};
static signed char aid2055[3] = {0xB1, 0x20, 0x13};
static signed char aid1529[3] = {0xB1, 0x10, 0x8B};
static signed char aid1002[3] = {0xB1, 0x10, 0x03};
static signed char aid755[3] = {0xB1, 0x00, 0xC3};
static signed char aid507[3] = {0xB1, 0x00, 0x83};
static signed char aid399[3] = {0xB1, 0x00, 0x67};
static signed char aid174[3] = {0xB1, 0x00, 0x2D};
static signed char aid46[3] = {0xB1, 0x00, 0x0C};

static unsigned char elv1[3] = {
	0xAC, 0x14
};
static unsigned char elv2[3] = {
	0xAC, 0x13
};
static unsigned char elv3[3] = {
	0xAC, 0x12
};
static unsigned char elv4[3] = {
	0xAC, 0x11
};
static unsigned char elv5[3] = {
	0xAC, 0x10
};
static unsigned char elv6[3] = {
	0xAC, 0x0F
};
static unsigned char elv7[3] = {
	0xAC, 0x0E
};
static unsigned char elv8[3] = {
	0xAC, 0x0D
};
static unsigned char elv9[3] = {
	0xAC, 0x0C
};
static unsigned char elv10[3] = {
	0xAC, 0x0B
};
static unsigned char elv11[3] = {
	0xAC, 0x0A
};
static unsigned char elv12[3] = {
	0xAC, 0x09
};
static unsigned char elv13[3] = {
	0xAC, 0x08
};
static unsigned char elv14[3] = {
	0xAC, 0x06
};
static unsigned char elv15[3] = {
	0xAC, 0x05
};
static unsigned char elv16[3] = {
	0xAC, 0x04
};
static unsigned char elv17[3] = {
	0xAC, 0x03
};
static unsigned char elv18[3] = {
	0xAC, 0x02
};
static unsigned char elv19[3] = {
	0xAC, 0x01
};
static unsigned char elv20[3] = {
	0xAC, 0x00
};
static unsigned char elv21[3] = {
	0xAC, 0x07
};


static unsigned char elvCaps1[3]  = {
	0xBC, 0x14
};
static unsigned char elvCaps2[3]  = {
	0xBC, 0x13
};
static unsigned char elvCaps3[3]  = {
	0xBC, 0x12
};
static unsigned char elvCaps4[3]  = {
	0xBC, 0x11
};
static unsigned char elvCaps5[3]  = {
	0xBC, 0x10
};
static unsigned char elvCaps6[3]  = {
	0xBC, 0x0F
};
static unsigned char elvCaps7[3]  = {
	0xBC, 0x0E
};
static unsigned char elvCaps8[3]  = {
	0xBC, 0x0D
};
static unsigned char elvCaps9[3]  = {
	0xBC, 0x0C
};
static unsigned char elvCaps10[3]  = {
	0xBC, 0x0B
};
static unsigned char elvCaps11[3]  = {
	0xBC, 0x0A
};
static unsigned char elvCaps12[3] = {
	0xBC, 0x09
};
static unsigned char elvCaps13[3] = {
	0xBC, 0x08
};
static unsigned char elvCaps14[3] = {
	0xBC, 0x06
};
static unsigned char elvCaps15[3] = {
	0xBC, 0x05
};
static unsigned char elvCaps16[3] = {
	0xBC, 0x04
};
static unsigned char elvCaps17[3] = {
	0xBC, 0x03
};
static unsigned char elvCaps18[3] = {
	0xBC, 0x02
};
static unsigned char elvCaps19[3] = {
	0xBC, 0x01
};
static unsigned char elvCaps20[3] = {
	0xBC, 0x00
};

static unsigned char elvCaps21[3] = {
	0xBC, 0x07
};


static char elvss_offset1[3] = {0x00, 0x04, 0x00};
static char elvss_offset2[3] = {0x00, 0x03, -0x01};
static char elvss_offset3[3] = {0x00, 0x02, -0x02};
static char elvss_offset4[3] = {0x00, 0x01, -0x03};
static char elvss_offset5[3] = {0x00, 0x00, -0x04};
static char elvss_offset6[3] = {-0x02, -0x02, -0x07};
static char elvss_offset7[3] = {-0x04, -0x04, -0x09};
static char elvss_offset8[3] = {-0x05, -0x05, -0x0A};


#ifdef CONFIG_LCD_HMT
static signed char HA3_HMTrtbl10nit[10] = {0, 4, 10, 8, 6, 5, 4, 3, 2, 0};
static signed char HA3_HMTrtbl11nit[10] = {0, 1, 2, 9, 7, 6, 4, 3, 1, 0};
static signed char HA3_HMTrtbl12nit[10] = {0, 2, 10, 8, 7, 5, 4, 3, 1, 0};
static signed char HA3_HMTrtbl13nit[10] = {0, 0, 2, 8, 7, 6, 3, 2, 0, 0};
static signed char HA3_HMTrtbl14nit[10] = {0, 7, 1, 8, 6, 5, 3, 3, 0, 0};
static signed char HA3_HMTrtbl15nit[10] = {0, 5, 3, 8, 7, 4, 3, 2, -1, 0};
static signed char HA3_HMTrtbl16nit[10] = {0, 1, 10, 8, 6, 5, 3, 2, -1, 0};
static signed char HA3_HMTrtbl17nit[10] = {0, 0, 1, 8, 6, 5, 3, 2, 1, 0};
static signed char HA3_HMTrtbl19nit[10] = {0, 2, 9, 7, 6, 5, 3, 2, 1, 0};
static signed char HA3_HMTrtbl20nit[10] = {0, 10, 10, 8, 5, 4, 3, 2, 2, 0};
static signed char HA3_HMTrtbl21nit[10] = {0, 0, 3, 8, 6, 5, 3, 2, 1, 0};
static signed char HA3_HMTrtbl22nit[10] = {0, 10, 10, 7, 6, 4, 3, 1, 2, 0};
static signed char HA3_HMTrtbl23nit[10] = {0, 2, 1, 7, 5, 5, 3, 1, 1, 0};
static signed char HA3_HMTrtbl25nit[10] = {0, 1, 9, 7, 6, 4, 3, 1, 2, 0};
static signed char HA3_HMTrtbl27nit[10] = {0, 1, 9, 7, 6, 4, 3, 2, 2, 0};
static signed char HA3_HMTrtbl29nit[10] = {0, 2, 1, 7, 5, 4, 4, 1, 3, 0};
static signed char HA3_HMTrtbl31nit[10] = {0, 0, 9, 6, 6, 4, 3, 2, 3, 0};
static signed char HA3_HMTrtbl33nit[10] = {0, 4, 9, 7, 5, 4, 4, 2, 3, 0};
static signed char HA3_HMTrtbl35nit[10] = {0, 4, 1, 7, 6, 4, 4, 4, 3, 0};
static signed char HA3_HMTrtbl37nit[10] = {0, 1, 8, 6, 5, 4, 4, 4, 3, 0};
static signed char HA3_HMTrtbl39nit[10] = {0, 0, 9, 7, 6, 5, 5, 3, 3, 0};
static signed char HA3_HMTrtbl41nit[10] = {0, 10, 9, 7, 5, 4, 5, 4, 4, 0};
static signed char HA3_HMTrtbl44nit[10] = {0, 2, 9, 7, 6, 5, 5, 5, 4, 0};
static signed char HA3_HMTrtbl47nit[10] = {0, 4, 9, 7, 6, 6, 5, 6, 4, 0};
static signed char HA3_HMTrtbl50nit[10] = {0, 3, 8, 6, 5, 4, 5, 6, 4, 0};
static signed char HA3_HMTrtbl53nit[10] = {0, 4, 9, 7, 6, 6, 6, 6, 4, 0};
static signed char HA3_HMTrtbl56nit[10] = {0, 11, 9, 7, 6, 6, 6, 7, 5, 0};
static signed char HA3_HMTrtbl60nit[10] = {0, 0, 9, 7, 6, 6, 6, 7, 4, 0};
static signed char HA3_HMTrtbl64nit[10] = {0, 10, 9, 7, 6, 6, 7, 8, 5, 0};
static signed char HA3_HMTrtbl68nit[10] = {0, 1, 9, 7, 6, 6, 6, 7, 5, 0};
static signed char HA3_HMTrtbl72nit[10] = {0, 10, 8, 7, 5, 6, 5, 7, 5, 0};
static signed char HA3_HMTrtbl77nit[10] = {0, 7, 5, 4, 3, 3, 4, 5, 3, 0};
static signed char HA3_HMTrtbl82nit[10] = {0, 7, 5, 4, 3, 3, 5, 5, 4, 0};
static signed char HA3_HMTrtbl87nit[10] = {0, 6, 5, 3, 4, 3, 5, 6, 4, 0};
static signed char HA3_HMTrtbl93nit[10] = {0, 5, 6, 4, 3, 3, 5, 6, 5, 0};
static signed char HA3_HMTrtbl99nit[10] = {0, 6, 5, 4, 4, 4, 5, 6, 5, 0};
static signed char HA3_HMTrtbl105nit[10] = {0, 6, 5, 4, 3, 4, 5, 6, 4, 0};


static signed char HA3_HMTctbl10nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -8, -1, 3, -7, -3, 4, -8, -3, 4, -8, -3, 2, -4, -1, 0, 0, -1, 0, -1, -1, 0, -1};
static signed char HA3_HMTctbl11nit[30] = {0, 0, 0, 0, 0, 0,-4, -12, 26, 0, 2, -6, -4, 3, -8, -3, 3, -7, -2, 1, -3, -2, 0, -2, 0, 0, 0, -1, 0, -1};
static signed char HA3_HMTctbl12nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -7, 0, 3, -8, -4, 3, -7, -5, 3, -8, -2, 1, -3, -2, 0, -2, 0, 0, 0, -1, 0, -1};
static signed char HA3_HMTctbl13nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, 0, 0, 3, -8, -4, 4, -8, -3, 3, -7, -2, 1, -3, -1, 0, -1, 0, 0, -1, -2, 0, -1};
static signed char HA3_HMTctbl14nit[30] = {0, 0, 0, 0, 0, 0,-3, -1, 4, -1, 2, -6, -3, 4, -8, -5, 3, -7, -1, 1, -3, -1, 0, -1, -1, 0, -1, -1, 0, 0};
static signed char HA3_HMTctbl15nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 3, -7, -4, 3, -7, -4, 3, -7, -1, 1, -2, -2, 0, -2, -1, 0, 0, -1, 0, -1};
static signed char HA3_HMTctbl16nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -9, -1, 3, -7, -4, 3, -7, -3, 3, -7, -1, 1, -3, -2, 0, 0, 0, 0, -1, 0, 0, 0};
static signed char HA3_HMTctbl17nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, -2, 3, -8, -5, 3, -8, -1, 3, -6, -2, 0, -2, -2, 0, -2, 0, 0, 0, -1, 0, -1};
static signed char HA3_HMTctbl19nit[30] = {0, 0, 0, 0, 0, 0,1, 4, -8, -1, 3, -8, -4, 3, -7, -3, 2, -6, -2, 0, -2, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char HA3_HMTctbl20nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -9, 0, 2, -6, -5, 3, -8, -3, 2, -6, -2, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl21nit[30] = {0, 0, 0, 0, 0, 0,30, 0, 0, -2, 3, -8, -3, 3, -6, -3, 2, -6, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl22nit[30] = {0, 0, 0, 0, 0, 0,1, 3, -8, -2, 3, -8, -4, 3, -7, -3, 2, -6, -1, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, 0};
static signed char HA3_HMTctbl23nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -6, -1, 4, -8, -5, 3, -8, -2, 2, -4, -2, 0, -2, -2, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HA3_HMTctbl25nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -10, -3, 3, -7, -4, 3, -6, -3, 2, -5, -1, 1, -2, -1, 0, -1, 0, 0, 0, -1, 0, 0};
static signed char HA3_HMTctbl27nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -10, -2, 3, -8, -4, 3, -6, -2, 2, -5, -2, 0, -1, 0, 0, -1, -2, 0, -1, 0, 0, 0};
static signed char HA3_HMTctbl29nit[30] = {0, 0, 0, 0, 0, 0,1, 0, 0, -3, 4, -8, -5, 2, -6, -1, 2, -4, -3, 0, -2, -1, 0, -2, 1, 0, 0, 0, 0, 1};
static signed char HA3_HMTctbl31nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -10, -3, 4, -8, -4, 3, -6, -1, 2, -4, -2, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl33nit[30] = {0, 0, 0, 0, 0, 0,0, 5, -11, -3, 3, -8, -5, 2, -6, -1, 2, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl35nit[30] = {0, 0, 0, 0, 0, 0,3, 0, -1, -3, 3, -8, -4, 2, -6, -2, 2, -4, -2, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl37nit[30] = {0, 0, 0, 0, 0, 0,0, 5, -10, -2, 3, -7, -4, 2, -6, -2, 2, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl39nit[30] = {0, 0, 0, 0, 0, 0,0, 5, -10, -3, 3, -8, -4, 2, -5, -3, 1, -4, -2, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl41nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -9, -3, 3, -8, -4, 2, -5, -2, 1, -3, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl44nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -11, -2, 3, -7, -4, 2, -5, -2, 1, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl47nit[30] = {0, 0, 0, 0, 0, 0,0, 5, -11, -3, 3, -8, -3, 2, -5, -2, 1, -4, -2, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl50nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -11, -3, 3, -8, -3, 2, -5, -1, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl53nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -10, -3, 3, -7, -4, 2, -6, -2, 1, -3, -1, 0, -1, -2, 0, -2, 0, 0, 0, 1, 0, 0};
static signed char HA3_HMTctbl56nit[30] = {0, 0, 0, 0, 0, 0,0, 5, -10, -4, 3, -8, -2, 1, -4, -2, 1, -3, -2, 0, -2, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HA3_HMTctbl60nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -11, -3, 2, -6, -2, 2, -5, -2, 1, -3, -2, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HA3_HMTctbl64nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -10, -4, 3, -7, -1, 2, -4, -2, 1, -2, -1, 0, -1, -2, 0, -1, 0, 0, 0, 2, 0, 0};
static signed char HA3_HMTctbl68nit[30] = {0, 0, 0, 0, 0, 0,-2, 5, -10, -4, 2, -6, -2, 1, -4, -2, 1, -3, -1, 0, -1, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HA3_HMTctbl72nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -11, -3, 2, -5, -2, 2, -4, -1, 1, -2, -2, 0, -1, -1, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char HA3_HMTctbl77nit[30] = {0, 0, 0, 0, 0, 0,2, 4, -8, -1, 2, -4, -4, 1, -4, -1, 1, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static signed char HA3_HMTctbl82nit[30] = {0, 0, 0, 0, 0, 0,2, 4, -8, -2, 2, -6, -3, 1, -3, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HA3_HMTctbl87nit[30] = {0, 0, 0, 0, 0, 0,3, 4, -8, -2, 2, -4, -3, 1, -3, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1};
static signed char HA3_HMTctbl93nit[30] = {0, 0, 0, 0, 0, 0,2, 3, -7, -3, 2, -5, -2, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HA3_HMTctbl99nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -8, -2, 2, -4, -1, 1, -3, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HA3_HMTctbl105nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -8, -2, 2, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1};

static signed char HA3_HMTaid8004[3] = {0xB1, 0x20, 0x03};
static signed char HA3_HMTaid7004[3] = {0xB1, 0X30, 0x05};

static unsigned char HA3_HMTelv[3] = {
	0xAC, 0x0A
};
static unsigned char HA3_HMTelvCaps[3] = {
	0xBC, 0x0A
};

static signed char HA3_da_HMTrtbl10nit[10] = {0, -1, 4, 6, 5, 3, 2, 0, 1, 0};
static signed char HA3_da_HMTrtbl11nit[10] = {0, -1, 1, 6, 4, 3, 2, 1, 0, 0};
static signed char HA3_da_HMTrtbl12nit[10] = {0, 0, 7, 6, 5, 4, 3, 2, 1, 0};
static signed char HA3_da_HMTrtbl13nit[10] = {0, -1, 1, 7, 5, 4, 3, 2, -1, 0};
static signed char HA3_da_HMTrtbl14nit[10] = {0, 0, 0, 6, 5, 4, 2, 1, -1, 0};
static signed char HA3_da_HMTrtbl15nit[10] = {0, 0, 0, 6, 5, 3, 3, 1, -1, 0};
static signed char HA3_da_HMTrtbl16nit[10] = {0, 0, 8, 6, 4, 4, 3, 2, -1, 0};
static signed char HA3_da_HMTrtbl17nit[10] = {0, 0, 7, 6, 5, 4, 3, 1, 0, 0};
static signed char HA3_da_HMTrtbl19nit[10] = {0, 2, 0, 6, 4, 4, 3, 2, 0, 0};
static signed char HA3_da_HMTrtbl20nit[10] = {0, 3, 0, 6, 4, 3, 3, 2, 1, 0};
static signed char HA3_da_HMTrtbl21nit[10] = {0, 4, 8, 6, 4, 4, 3, 2, 1, 0};
static signed char HA3_da_HMTrtbl22nit[10] = {0, 7, 2, 6, 4, 3, 2, 1, 2, 0};
static signed char HA3_da_HMTrtbl23nit[10] = {0, 1, 8, 6, 4, 4, 2, 2, 2, 0};
static signed char HA3_da_HMTrtbl25nit[10] = {0, 1, 1, 6, 5, 4, 3, 2, 3, 0};
static signed char HA3_da_HMTrtbl27nit[10] = {0, 1, 7, 6, 5, 4, 3, 1, 3, 0};
static signed char HA3_da_HMTrtbl29nit[10] = {0, 0, 1, 6, 5, 4, 4, 3, 4, 0};
static signed char HA3_da_HMTrtbl31nit[10] = {0, 0, 8, 5, 5, 4, 4, 3, 4, 0};
static signed char HA3_da_HMTrtbl33nit[10] = {0, 1, 8, 6, 4, 4, 4, 3, 3, 0};
static signed char HA3_da_HMTrtbl35nit[10] = {0, 11, 0, 6, 5, 4, 4, 4, 3, 0};
static signed char HA3_da_HMTrtbl37nit[10] = {0, 9, 7, 5, 4, 4, 4, 4, 2, 0};
static signed char HA3_da_HMTrtbl39nit[10] = {0, 0, 8, 6, 5, 5, 4, 4, 2, 0};
static signed char HA3_da_HMTrtbl41nit[10] = {0, 2, 7, 5, 5, 4, 5, 4, 2, 0};
static signed char HA3_da_HMTrtbl44nit[10] = {0, 9, 8, 5, 4, 4, 5, 4, 2, 0};
static signed char HA3_da_HMTrtbl47nit[10] = {0, 0, 8, 6, 5, 5, 5, 5, 2, 0};
static signed char HA3_da_HMTrtbl50nit[10] = {0, 9, 7, 5, 4, 4, 5, 5, 1, 0};
static signed char HA3_da_HMTrtbl53nit[10] = {0, 0, 7, 5, 4, 5, 6, 6, 3, 0};
static signed char HA3_da_HMTrtbl56nit[10] = {0, 0, 7, 5, 5, 4, 5, 6, 3, 0};
static signed char HA3_da_HMTrtbl60nit[10] = {0, 8, 8, 6, 5, 5, 6, 6, 4, 0};
static signed char HA3_da_HMTrtbl64nit[10] = {0, 8, 7, 5, 5, 5, 7, 7, 3, 0};
static signed char HA3_da_HMTrtbl68nit[10] = {0, 8, 7, 6, 6, 5, 6, 7, 3, 0};
static signed char HA3_da_HMTrtbl72nit[10] = {0, 9, 7, 6, 6, 6, 6, 7, 4, 0};
static signed char HA3_da_HMTrtbl77nit[10] = {0, 6, 4, 3, 3, 3, 4, 4, 1, 0};
static signed char HA3_da_HMTrtbl82nit[10] = {0, 6, 4, 3, 3, 4, 4, 6, 3, 0};
static signed char HA3_da_HMTrtbl87nit[10] = {0, 5, 4, 3, 3, 3, 4, 5, 2, 0};
static signed char HA3_da_HMTrtbl93nit[10] = {0, 5, 5, 3, 3, 4, 5, 5, 2, 0};
static signed char HA3_da_HMTrtbl99nit[10] = {0, 5, 4, 3, 3, 3, 5, 5, 3, 0};
static signed char HA3_da_HMTrtbl105nit[10] = {0, 5, 4, 2, 3, 3, 5, 5, 3, 0};


static signed char HA3_da_HMTctbl10nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -8, -5, 4, -8, -3, 2, -5, -5, 2, -5, -3, 2, -5, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl11nit[30] = {0, 0, 0, 0, 0, 0,-4, -12, 26, -3, 3, -6, -3, 3, -6, -4, 2, -5, -3, 1, -4, 0, 0, -1, 0, 0, 0, -1, 0, -1};
static signed char HA3_da_HMTctbl12nit[30] = {0, 0, 0, 0, 0, 0,1, 4, -8, -6, 4, -9, -3, 2, -5, -3, 2, -5, -3, 1, -4, 0, 0, -2, 0, 0, 0, -1, 0, 0};
static signed char HA3_da_HMTctbl13nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, 0, -5, 3, -7, -4, 2, -5, -4, 2, -5, -2, 1, -4, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char HA3_da_HMTctbl14nit[30] = {0, 0, 0, 0, 0, 0,-3, -1, 4, -6, 3, -8, -3, 2, -4, -2, 2, -4, -3, 2, -4, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char HA3_da_HMTctbl15nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, -5, 3, -7, -4, 2, -5, -4, 2, -5, -2, 1, -3, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char HA3_da_HMTctbl16nit[30] = {0, 0, 0, 0, 0, 0,-2, 4, -9, -7, 3, -8, -6, 2, -6, -3, 2, -4, -1, 1, -3, 0, 0, 0, -1, 0, 0, 0, 0, 1};
static signed char HA3_da_HMTctbl17nit[30] = {0, 0, 0, 0, 0, 0,-3, 5, -11, -4, 3, -7, -4, 2, -4, -3, 2, -5, -3, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl19nit[30] = {0, 0, 0, 0, 0, 0,1, 4, -8, -5, 3, -6, -3, 2, -5, -3, 2, -4, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl20nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -9, -5, 3, -8, -3, 1, -3, -3, 2, -4, -1, 1, -2, 0, 0, -1, 0, 0, 1, 0, 0, 0};
static signed char HA3_da_HMTctbl21nit[30] = {0, 0, 0, 0, 0, 0,-7, 5, -10, -5, 2, -6, -3, 2, -4, -3, 2, -4, -1, 1, -3, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char HA3_da_HMTctbl22nit[30] = {0, 0, 0, 0, 0, 0,1, 3, -8, -5, 2, -6, -3, 2, -4, -3, 2, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl23nit[30] = {0, 0, 0, 0, 0, 0,-10, 5, -12, -5, 3, -7, -4, 2, -4, -4, 1, -4, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl25nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -10, -5, 3, -7, -3, 1, -4, -3, 2, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl27nit[30] = {0, 0, 0, 0, 0, 0,-11, 6, -13, -4, 2, -6, -2, 2, -4, -3, 2, -4, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl29nit[30] = {0, 0, 0, 0, 0, 0,1, 0, 0, -4, 2, -6, -3, 1, -3, -3, 2, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl31nit[30] = {0, 0, 0, 0, 0, 0,-9, 5, -10, -4, 3, -6, -3, 1, -4, -2, 2, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl33nit[30] = {0, 0, 0, 0, 0, 0,-9, 5, -12, -4, 2, -5, -4, 1, -4, -3, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl35nit[30] = {0, 0, 0, 0, 0, 0,3, 0, -1, -5, 2, -6, -4, 1, -4, -3, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl37nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -11, -4, 2, -6, -4, 1, -4, -3, 1, -3, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl39nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -10, -6, 2, -6, -3, 1, -3, -2, 1, -4, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl41nit[30] = {0, 0, 0, 0, 0, 0,-7, 6, -12, -4, 2, -5, -2, 1, -4, -1, 1, -3, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl44nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -9, -6, 2, -6, -1, 1, -3, -3, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl47nit[30] = {0, 0, 0, 0, 0, 0,-8, 4, -10, -3, 2, -5, -3, 1, -4, -2, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl50nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -10, -2, 2, -4, -3, 2, -4, -1, 1, -2, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl53nit[30] = {0, 0, 0, 0, 0, 0,-9, 5, -10, -2, 2, -4, -3, 1, -4, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HA3_da_HMTctbl56nit[30] = {0, 0, 0, 0, 0, 0,-9, 5, -11, -2, 2, -5, -2, 1, -4, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl60nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -10, -3, 2, -5, -2, 1, -4, -1, 1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl64nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -3, 2, -5, -1, 1, -2, -1, 0, -2, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HA3_da_HMTctbl68nit[30] = {0, 0, 0, 0, 0, 0,-9, 5, -10, -1, 2, -4, -2, 1, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_da_HMTctbl72nit[30] = {0, 0, 0, 0, 0, 0,-7, 5, -10, -3, 2, -4, -1, 1, -3, -1, 0, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HA3_da_HMTctbl77nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -8, -2, 2, -4, -1, 1, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static signed char HA3_da_HMTctbl82nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -2, 1, -4, -1, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char HA3_da_HMTctbl87nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -9, -2, 1, -3, 0, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HA3_da_HMTctbl93nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -6, -3, 1, -4, -1, 1, -2, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HA3_da_HMTctbl99nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -3, 1, -4, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HA3_da_HMTctbl105nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -8, -3, 1, -3, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1};


static signed char HF4_A2_HMTrtbl10nit[10] = {0, 12, 12, 10, 9, 7, 4, 3, 2, 0};
static signed char HF4_A2_HMTrtbl11nit[10] = {0, 12, 12, 11, 9, 7, 5, 3, 0, 0};
static signed char HF4_A2_HMTrtbl12nit[10] = {0, 12, 12, 11, 9, 7, 5, 3, 0, 0};
static signed char HF4_A2_HMTrtbl13nit[10] = {0, 12, 12, 10, 9, 7, 5, 2, 0, 0};
static signed char HF4_A2_HMTrtbl14nit[10] = {0, 12, 12, 10, 9, 7, 5, 3, 1, 0};
static signed char HF4_A2_HMTrtbl15nit[10] = {0, 12, 12, 10, 9, 7, 5, 2, 1, 0};
static signed char HF4_A2_HMTrtbl16nit[10] = {0, 12, 12, 10, 8, 6, 5, 3, 2, 0};
static signed char HF4_A2_HMTrtbl17nit[10] = {0, 12, 12, 10, 9, 7, 4, 3, 3, 0};
static signed char HF4_A2_HMTrtbl19nit[10] = {0, 12, 12, 9, 8, 7, 4, 3, 3, 0};
static signed char HF4_A2_HMTrtbl20nit[10] = {0, 12, 12, 9, 9, 7, 4, 2, 3, 0};
static signed char HF4_A2_HMTrtbl21nit[10] = {0, 12, 12, 9, 8, 6, 4, 2, 2, 0};
static signed char HF4_A2_HMTrtbl22nit[10] = {0, 12, 12, 9, 8, 5, 3, 1, 3, 0};
static signed char HF4_A2_HMTrtbl23nit[10] = {0, 12, 12, 9, 8, 6, 3, 1, 2, 0};
static signed char HF4_A2_HMTrtbl25nit[10] = {0, 12, 12, 9, 7, 6, 3, 2, 3, 0};
static signed char HF4_A2_HMTrtbl27nit[10] = {0, 12, 12, 9, 9, 7, 5, 3, 3, 0};
static signed char HF4_A2_HMTrtbl29nit[10] = {0, 12, 12, 9, 8, 6, 4, 2, 3, 0};
static signed char HF4_A2_HMTrtbl31nit[10] = {0, 12, 12, 9, 8, 6, 4, 4, 4, 0};
static signed char HF4_A2_HMTrtbl33nit[10] = {0, 12, 12, 8, 8, 6, 4, 4, 4, 0};
static signed char HF4_A2_HMTrtbl35nit[10] = {0, 12, 12, 9, 7, 6, 5, 5, 4, 0};
static signed char HF4_A2_HMTrtbl37nit[10] = {0, 12, 12, 9, 8, 7, 6, 6, 5, 0};
static signed char HF4_A2_HMTrtbl39nit[10] = {0, 12, 12, 9, 7, 6, 5, 5, 5, 0};
static signed char HF4_A2_HMTrtbl41nit[10] = {0, 12, 12, 9, 8, 6, 5, 6, 2, 0};
static signed char HF4_A2_HMTrtbl44nit[10] = {0, 12, 11, 8, 8, 6, 6, 6, 3, 0};
static signed char HF4_A2_HMTrtbl47nit[10] = {0, 12, 11, 9, 8, 6, 7, 6, 4, 0};
static signed char HF4_A2_HMTrtbl50nit[10] = {0, 12, 11, 9, 9, 6, 7, 7, 3, 0};
static signed char HF4_A2_HMTrtbl53nit[10] = {0, 12, 11, 9, 8, 6, 6, 8, 5, 0};
static signed char HF4_A2_HMTrtbl56nit[10] = {0, 12, 11, 8, 8, 6, 7, 8, 4, 0};
static signed char HF4_A2_HMTrtbl60nit[10] = {0, 12, 11, 9, 9, 6, 7, 9, 5, 0};
static signed char HF4_A2_HMTrtbl64nit[10] = {0, 12, 11, 8, 8, 6, 8, 9, 6, 0};
static signed char HF4_A2_HMTrtbl68nit[10] = {0, 11, 11, 9, 9, 7, 8, 10, 8, 0};
static signed char HF4_A2_HMTrtbl72nit[10] = {0, 11, 11, 10, 9, 8, 8, 10, 9, 0};
static signed char HF4_A2_HMTrtbl77nit[10] = {0, 9, 8, 5, 4, 5, 5, 7, 4, 0};
static signed char HF4_A2_HMTrtbl82nit[10] = {0, 9, 8, 6, 5, 5, 6, 7, 4, 0};
static signed char HF4_A2_HMTrtbl87nit[10] = {0, 9, 7, 5, 5, 5, 6, 8, 5, 0};
static signed char HF4_A2_HMTrtbl93nit[10] = {0, 9, 7, 6, 5, 5, 6, 8, 5, 0};
static signed char HF4_A2_HMTrtbl99nit[10] = {0, 10, 8, 5, 5, 5, 6, 8, 7, 0};
static signed char HF4_A2_HMTrtbl105nit[10] = {0, 9, 8, 6, 5, 5, 6, 9, 6, 0};

static signed char HF4_A2_HMTctbl10nit[30] = {0, 0, 0, 6, 5, 4, -4, 8, -6, -4, 5, -12, -6, 4, -8, -7, 4, -10, -2, 2, -6, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl11nit[30] = {0, 0, 0, 6, 5, 4, -4, 10, -5, -2, 4, -8, -7, 4, -9, -6, 4, -9, -1, 2, -5, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl12nit[30] = {0, 0, 0, 6, 5, 4, -2, 11, -4, -3, 4, -10, -7, 3, -8, -5, 4, -8, -2, 2, -5, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl13nit[30] = {0, 0, 0, 6, 5, 6, -2, 13, -4, -3, 5, -10, -8, 4, -10, -5, 3, -8, -2, 1, -4, -1, 0, -2, 1, 0, 0, 0, 0, 0};
static signed char HF4_A2_HMTctbl14nit[30] = {0, 0, 0, 7, 6, 6, -3, 13, -5, -4, 5, -12, -6, 3, -8, -4, 4, -8, -2, 2, -4, 0, 0, 0, 0, 0, -1, 1, 0, 0};
static signed char HF4_A2_HMTctbl15nit[30] = {0, 0, 0, 5, 6, 4, -4, 8, -9, -3, 4, -8, -7, 3, -8, -4, 4, -8, -2, 1, -3, 0, 0, -2, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl16nit[30] = {0, 0, 0, 5, 6, 4, -4, 8, -9, -3, 3, -8, -7, 4, -8, -4, 3, -8, -2, 2, -4, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF4_A2_HMTctbl17nit[30] = {0, 0, 0, 4, 5, 4, -4, 8, -9, -5, 4, -10, -5, 3, -8, -5, 3, -8, -2, 1, -4, 1, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HF4_A2_HMTctbl19nit[30] = {0, 0, 0, 4, 6, 3, -2, 11, -7, -4, 4, -10, -7, 4, -8, -3, 3, -7, -3, 1, -4, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl20nit[30] = {0, 0, 0, 6, 7, 5, -4, 8, -9, -4, 5, -11, -6, 3, -8, -4, 3, -6, -1, 1, -2, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl21nit[30] = {0, 0, 0, 6, 7, 5, -6, 8, -11, -5, 5, -11, -6, 3, -8, -3, 3, -6, 0, 1, -2, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char HF4_A2_HMTctbl22nit[30] = {0, 0, 0, 9, 11, 7, -8, 4, -13, -4, 4, -10, -7, 3, -7, -5, 3, -8, 0, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl23nit[30] = {0, 0, 0, 9, 11, 6, -8, 5, -14, -5, 4, -10, -6, 3, -7, -4, 3, -6, -1, 1, -3, 1, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HF4_A2_HMTctbl25nit[30] = {0, 0, 0, 8, 11, 6, -8, 5, -14, -4, 4, -9, -5, 3, -7, -3, 3, -6, -1, 1, -2, 1, 0, -1, 0, 0, 0, 1, 0, 1};
static signed char HF4_A2_HMTctbl27nit[30] = {0, 0, 0, 5, 6, 4, -8, 8, -14, -4, 5, -10, -6, 3, -7, -4, 3, -6, -2, 0, -2, 0, 0, -2, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl29nit[30] = {0, 0, 0, 4, 6, 2, -8, 8, -14, -5, 5, -10, -6, 3, -7, -3, 2, -6, 0, 1, -2, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HF4_A2_HMTctbl31nit[30] = {0, 0, 0, 7, 11, 5, -8, 2, -14, -5, 4, -10, -4, 3, -6, -5, 2, -6, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char HF4_A2_HMTctbl33nit[30] = {0, 0, 0, 6, 11, 5, -8, 3, -14, -6, 4, -10, -5, 3, -7, -3, 2, -5, 0, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HF4_A2_HMTctbl35nit[30] = {0, 0, 0, 6, 11, 4, -8, 5, -14, -4, 4, -9, -5, 3, -6, -3, 2, -5, -1, 0, -2, 0, 0, 0, 1, 0, 0, 1, 0, 1};
static signed char HF4_A2_HMTctbl37nit[30] = {0, 0, 0, 6, 11, 4, -8, 4, -14, -4, 4, -8, -4, 3, -6, -4, 2, -5, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static signed char HF4_A2_HMTctbl39nit[30] = {0, 0, 0, 6, 11, 4, -8, 4, -14, -4, 4, -9, -5, 2, -6, -3, 2, -4, 0, 1, -2, 0, 0, 0, 1, 0, 1, 2, 0, 1};
static signed char HF4_A2_HMTctbl41nit[30] = {0, 0, 0, 5, 11, 4, -8, 7, -14, -4, 4, -8, -6, 3, -8, -3, 2, -5, 0, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HF4_A2_HMTctbl44nit[30] = {0, 0, 0, 0, 6, -2, -6, 6, -12, -6, 4, -10, -4, 3, -6, -1, 1, -4, -1, 1, -2, 0, 0, 0, 1, 0, 0, 2, 0, 2};
static signed char HF4_A2_HMTctbl47nit[30] = {0, 0, 0, 2, 5, -1, -7, 9, -12, -4, 4, -8, -4, 3, -6, -2, 2, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HF4_A2_HMTctbl50nit[30] = {0, 0, 0, 2, 5, 0, -7, 9, -12, -4, 4, -8, -4, 3, -6, -3, 2, -4, 0, 0, -1, 0, 0, -1, 1, 0, 0, 1, 0, 2};
static signed char HF4_A2_HMTctbl53nit[30] = {0, 0, 0, 4, 6, 1, -5, 9, -14, -5, 4, -8, -4, 2, -6, -3, 1, -4, -1, 0, -1, 0, 0, -1, 0, 0, -1, 2, 0, 3};
static signed char HF4_A2_HMTctbl56nit[30] = {0, 0, 0, 4, 9, 1, -9, 5, -10, -4, 4, -8, -5, 3, -6, -1, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 2, 0, 3};
static signed char HF4_A2_HMTctbl60nit[30] = {0, 0, 0, 4, 10, 2, -8, 6, -14, -4, 4, -8, -4, 3, -6, -2, 1, -3, -1, 0, -2, 0, 0, 0, 0, 0, 0, 3, 0, 3};
static signed char HF4_A2_HMTctbl64nit[30] = {0, 0, 0, 3, 10, 0, -6, 7, -11, -6, 4, -9, -4, 2, -6, -1, 1, -2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 3, 0, 3};
static signed char HF4_A2_HMTctbl68nit[30] = {0, 0, 0, 3, 10, 0, -6, 7, -11, -5, 4, -8, -3, 2, -5, -3, 1, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 2, 0, 2};
static signed char HF4_A2_HMTctbl72nit[30] = {0, 0, 0, 2, 9, 0, -6, 9, -11, -4, 3, -8, -4, 2, -5, -2, 1, -3, 0, 0, 0, 0, 0, -1, 0, 0, 0, 3, 0, 3};
static signed char HF4_A2_HMTctbl77nit[30] = {0, 0, 0, -1, 3, -4, -5, 5, -12, -3, 3, -6, -3, 2, -4, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF4_A2_HMTctbl82nit[30] = {0, 0, 0, -1, 2, -4, -5, 6, -14, -3, 3, -6, -2, 2, -4, -1, 1, -2, 0, 0, 0, 0, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HF4_A2_HMTctbl87nit[30] = {0, 0, 0, -1, 2, -4, -5, 6, -14, -2, 4, -6, -3, 1, -4, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF4_A2_HMTctbl93nit[30] = {0, 0, 0, -1, 3, -5, -3, 7, -12, -2, 4, -5, -2, 2, -4, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HF4_A2_HMTctbl99nit[30] = {0, 0, 0, -3, 1, -10, -7, 3, -12, -3, 5, -5, -4, 2, -2, 0, 1, -2, 0, 0, 0, 1, 0, 0, 1, 0, 0, 2, 0, 2};
static signed char HF4_A2_HMTctbl105nit[30] = {0, 0, 0, -2, 2, -9, -6, 5, -10, -4, 3, -4, -4, 1, -2, 0, 0, -1, 0, 0, -1, 1, 0, 0, 0, 0, 0, 2, 0, 3};

static signed char HF4_HMTaid7999[3] = {0xB1, 0x20, 0x05};
static signed char HF4_HMTaid7001[3] = {0xB1, 0X30, 0x07};


#endif

//// hf4 a3 ///

static signed char a3_rtbl2nit[10] = {0, 46, 46, 42, 38, 34, 26, 15, 8, 0};
static signed char a3_rtbl3nit[10] = {0, 45, 45, 40, 36, 32, 25, 15, 8, 0};
static signed char a3_rtbl4nit[10] = {0, 36, 36, 32, 28, 25, 20, 11, 6, 0};
static signed char a3_rtbl5nit[10] = {0, 33, 33, 29, 25, 22, 18, 10, 6, 0};
static signed char a3_rtbl6nit[10] = {0, 30, 30, 27, 24, 20, 16, 10, 6, 0};
static signed char a3_rtbl7nit[10] = {0, 28, 28, 24, 21, 18, 14, 9, 6, 0};
static signed char a3_rtbl8nit[10] = {0, 26, 26, 22, 19, 16, 13, 8, 5, 0};
static signed char a3_rtbl9nit[10] = {0, 25, 25, 21, 18, 15, 12, 7, 5, 0};
static signed char a3_rtbl10nit[10] = {0, 23, 23, 19, 16, 14, 11, 6, 4, 0};
static signed char a3_rtbl11nit[10] = {0, 23, 23, 19, 16, 14, 11, 6, 4, 0};
static signed char a3_rtbl12nit[10] = {0, 22, 22, 18, 15, 13, 10, 6, 4, 0};
static signed char a3_rtbl13nit[10] = {0, 22, 22, 18, 15, 13, 10, 6, 4, 0};
static signed char a3_rtbl14nit[10] = {0, 22, 22, 18, 15, 13, 8, 4, 4, 0};
static signed char a3_rtbl15nit[10] = {0, 20, 20, 16, 13, 11, 8, 4, 4, 0};
static signed char a3_rtbl16nit[10] = {0, 20, 20, 16, 13, 11, 8, 5, 5, 0};
static signed char a3_rtbl17nit[10] = {0, 18, 18, 15, 12, 10, 6, 3, 3, 0};
static signed char a3_rtbl19nit[10] = {0, 16, 16, 13, 11, 9, 7, 4, 4, 0};
static signed char a3_rtbl20nit[10] = {0, 16, 16, 13, 10, 8, 6, 4, 5, 0};
static signed char a3_rtbl21nit[10] = {0, 15, 15, 12, 10, 8, 6, 4, 4, 0};
static signed char a3_rtbl22nit[10] = {0, 14, 14, 11, 9, 8, 6, 4, 3, 0};
static signed char a3_rtbl24nit[10] = {0, 14, 14, 11, 9, 7, 6, 3, 4, 0};
static signed char a3_rtbl25nit[10] = {0, 14, 14, 11, 9, 7, 5, 3, 3, 0};
static signed char a3_rtbl27nit[10] = {0, 13, 13, 10, 8, 6, 4, 3, 2, 0};
static signed char a3_rtbl29nit[10] = {0, 12, 12, 9, 7, 5, 4, 2, 2, 0};
static signed char a3_rtbl30nit[10] = {0, 11, 11, 8, 7, 5, 4, 2, 2, 0};
static signed char a3_rtbl32nit[10] = {0, 10, 10, 7, 6, 5, 4, 3, 3, 0};
static signed char a3_rtbl34nit[10] = {0, 9, 9, 6, 5, 4, 3, 2, 2, 0};
static signed char a3_rtbl37nit[10] = {0, 9, 9, 6, 5, 4, 3, 3, 3, 0};
static signed char a3_rtbl39nit[10] = {0, 9, 9, 6, 5, 4, 4, 2, 3, 0};
static signed char a3_rtbl41nit[10] = {0, 8, 8, 6, 5, 4, 3, 2, 2, 0};
static signed char a3_rtbl44nit[10] = {0, 8, 8, 6, 5, 4, 3, 3, 3, 0};
static signed char a3_rtbl47nit[10] = {0, 7, 7, 5, 4, 3, 2, 2, 2, 0};
static signed char a3_rtbl50nit[10] = {0, 7, 7, 5, 4, 3, 2, 1, 2, 0};
static signed char a3_rtbl53nit[10] = {0, 6, 6, 4, 3, 3, 2, 2, 3, 0};
static signed char a3_rtbl56nit[10] = {0, 4, 4, 3, 2, 2, 1, 1, 2, 0};
static signed char a3_rtbl60nit[10] = {0, 4, 4, 3, 2, 2, 1, 1, 2, 0};
static signed char a3_rtbl64nit[10] = {0, 4, 4, 3, 2, 2, 1, 0, 2, 0};
static signed char a3_rtbl68nit[10] = {0, 3, 3, 2, 2, 2, 1, 1, 1, 0};
static signed char a3_rtbl72nit[10] = {0, 4, 4, 3, 3, 2, 1, 1, 2, 0};
static signed char a3_rtbl77nit[10] = {0, 5, 4, 2, 2, 1, 1, 1, 3, 0};
static signed char a3_rtbl82nit[10] = {0, 3, 3, 2, 1, 1, 2, 2, 3, 0};
static signed char a3_rtbl87nit[10] = {0, 3, 3, 2, 2, 1, 1, 1, 3, 0};
static signed char a3_rtbl93nit[10] = {0, 4, 3, 2, 2, 1, 3, 3, 1, 0};
static signed char a3_rtbl98nit[10] = {0, 4, 3, 2, 2, 2, 3, 2, 3, 0};
static signed char a3_rtbl105nit[10] = {0, 4, 3, 2, 2, 2, 2, 1, 0, 0};
static signed char a3_rtbl111nit[10] = {0, 5, 4, 3, 2, 2, 2, 2, 1, 0};
static signed char a3_rtbl119nit[10] = {0, 4, 3, 2, 2, 2, 3, 3, 1, 0};
static signed char a3_rtbl126nit[10] = {0, 3, 3, 2, 2, 2, 2, 2, 0, 0};
static signed char a3_rtbl134nit[10] = {0, 3, 3, 3, 2, 2, 3, 1, 2, 0};
static signed char a3_rtbl143nit[10] = {0, 3, 3, 2, 3, 2, 3, 3, 3, 0};
static signed char a3_rtbl152nit[10] = {0, 3, 3, 2, 2, 2, 3, 3, 1, 0};
static signed char a3_rtbl162nit[10] = {0, 4, 3, 2, 2, 2, 2, 3, 1, 0};
static signed char a3_rtbl172nit[10] = {0, 3, 2, 2, 1, 2, 2, 3, 2, 0};
static signed char a3_rtbl183nit[10] = {0, 3, 3, 2, 2, 2, 2, 4, 4, 0};
static signed char a3_rtbl195nit[10] = {0, 2, 2, 1, 2, 2, 3, 3, 2, 0};
static signed char a3_rtbl207nit[10] = {0, 3, 2, 1, 2, 2, 3, 4, 2, 0};
static signed char a3_rtbl220nit[10] = {0, 3, 1, 1, 2, 2, 2, 3, 2, 0};
static signed char a3_rtbl234nit[10] = {0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_rtbl249nit[10] = {0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char a3_rtbl265nit[10] = {0, 0, 0, 0, 0, 0, 1, 3, 3, 0};
static signed char a3_rtbl282nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_rtbl300nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char a3_rtbl316nit[10] = {0, 1, 0, 0, 1, 0, 1, 3, 2, 0};
static signed char a3_rtbl333nit[10] = {0, 1, 0, 0, 0, 0, 1, 3, 2, 0};
static signed char a3_rtbl350nit[10] = {0, 1, 0, 0, 0, 0, 1, 1, 0, 0};
static signed char a3_rtbl357nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_rtbl365nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_rtbl372nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_rtbl380nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_rtbl387nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_rtbl395nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_rtbl403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_rtbl412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char a3_ctbl2nit[30] = {0, 0, 0, -30, -16, -37, -6, 1, -13, 1, 0, -9, 1, 1, -11, -2, 1, -15, -4, 1, -11, -3, 1, -5, 0, 1, -2, -4, 1, -7};
static signed char a3_ctbl3nit[30] = {0, 0, 0, -29, -15, -36, -8, -1, -15, -1, -1, -14, -3, 0, -13, -5, -1, -16, -6, -1, -13, -5, -1, -6, -1, 0, -2, -4, -1, -7};
static signed char a3_ctbl4nit[30] = {0, 0, 0, -28, -14, -36, -6, 1, -15, 0, -1, -14, 0, 1, -12, -5, -1, -16, -6, 0, -10, -2, 0, -4, -1, 0, -1, -2, 0, -5};
static signed char a3_ctbl5nit[30] = {0, 0, 0, -28, -13, -35, -6, 1, -15, -1, -1, -15, -1, 1, -12, -4, 0, -15, -6, -1, -11, -2, 0, -3, -1, 0, -2, -2, 0, -4};
static signed char a3_ctbl6nit[30] = {0, 0, 0, -27, -13, -35, -6, 1, -15, 0, 1, -12, -2, -2, -15, -6, -1, -15, -5, 0, -9, -2, 0, -3, 0, 0, -1, -1, 0, -3};
static signed char a3_ctbl7nit[30] = {0, 0, 0, -26, -11, -33, -9, -1, -17, 0, 1, -13, 0, 1, -13, -5, 0, -15, -4, 1, -7, -1, 0, -2, -1, 0, -2, 0, 0, -2};
static signed char a3_ctbl8nit[30] = {0, 0, 0, -26, -11, -32, -7, 0, -15, -1, 0, -15, -1, 0, -13, -4, 1, -13, -4, 0, -6, -2, 0, -2, 0, 0, -1, 0, 0, -2};
static signed char a3_ctbl9nit[30] = {0, 0, 0, -26, -11, -33, -7, 1, -14, -2, 0, -16, -2, -1, -14, -3, 1, -12, -5, 0, -6, -1, 0, -2, 0, 0, -1, 1, 0, -1};
static signed char a3_ctbl10nit[30] = {0, 0, 0, -26, -10, -33, -6, 2, -14, 0, 1, -14, 0, 1, -13, -3, 1, -12, -5, 0, -6, -1, 0, -2, 0, 0, 0, 1, 0, -1};
static signed char a3_ctbl11nit[30] = {0, 0, 0, -26, -10, -33, -6, 2, -14, -1, 1, -15, 0, 1, -12, -4, -1, -13, -5, 0, -6, -2, -1, -2, 1, 0, 0, 2, 0, 0};
static signed char a3_ctbl12nit[30] = {0, 0, 0, -26, -10, -32, -8, -1, -17, 0, 1, -14, -1, 1, -12, -4, -1, -13, -3, 0, -4, -1, 0, -2, 0, 0, 0, 2, 0, 0};
static signed char a3_ctbl13nit[30] = {0, 0, 0, -26, -10, -32, -9, -1, -17, -1, 1, -16, -2, 0, -13, -4, -1, -12, -4, -1, -5, 0, 0, -1, 0, 0, -1, 2, 0, 0};
static signed char a3_ctbl14nit[30] = {0, 0, 0, -26, -10, -32, -9, -1, -17, -2, -1, -17, -2, -1, -13, -8, -5, -16, -3, 1, -3, 0, 0, -1, 0, 0, 0, 3, 0, 1};
static signed char a3_ctbl15nit[30] = {0, 0, 0, -25, -10, -32, -9, -1, -16, -1, 0, -16, -1, 0, -12, -4, -1, -12, -4, 0, -4, 1, 1, 0, 0, 0, 0, 3, 0, 1};
static signed char a3_ctbl16nit[30] = {0, 0, 0, -18, -9, -23, -4, -2, -26, -2, -1, -16, -3, -1, -14, -5, -1, -12, -3, 0, -3, 0, 0, -1, 0, 0, 0, 3, 0, 1};
static signed char a3_ctbl17nit[30] = {0, 0, 0, 11, -9, -18, 0, 1, -21, -1, -1, -17, -2, 0, -12, -5, -3, -13, -2, 1, -2, 0, 1, 0, 0, 0, 0, 3, 0, 1};
static signed char a3_ctbl19nit[30] = {0, 0, 0, -7, -8, -6, 0, 1, -25, 0, 2, -15, -2, -1, -12, -3, 0, -10, -3, 0, -3, 0, 0, -1, 0, 0, 0, 3, 0, 1};
static signed char a3_ctbl20nit[30] = {0, 0, 0, -6, -7, -5, -2, 1, -25, -4, -2, -18, -1, 0, -10, -3, 0, -10, -1, 1, -1, 0, 0, 0, 0, 0, 0, 3, 0, 1};
static signed char a3_ctbl21nit[30] = {0, 0, 0, 15, -7, -13, -1, 2, -24, 0, 1, -15, -2, -1, -10, -3, -1, -10, -1, 1, -1, 0, 0, -1, 0, 0, 0, 3, 0, 1};
static signed char a3_ctbl22nit[30] = {0, 0, 0, 13, -7, -10, 1, 1, -22, 0, 2, -14, 0, 2, -9, -2, 0, -9, -3, 0, -2, -1, -1, -1, 1, 0, 0, 3, 0, 1};
static signed char a3_ctbl24nit[30] = {0, 0, 0, -7, -7, -4, 0, 1, -22, -2, 0, -16, -3, -1, -10, -2, 1, -7, -3, -1, -2, 0, 1, -1, 1, 0, 1, 3, 0, 1};
static signed char a3_ctbl25nit[30] = {0, 0, 0, -7, -7, -16, -1, 0, -23, -2, -1, -16, -4, -2, -10, -2, -1, -9, -3, 0, -2, 0, 1, 0, 1, 0, 1, 3, 0, 2};
static signed char a3_ctbl27nit[30] = {0, 0, 0, -7, -2, -1, 0, 0, -23, -3, -2, -17, -4, -1, -10, -2, -1, -8, -1, 0, 0, 0, 0, 0, 1, 0, 1, 3, 0, 2};
static signed char a3_ctbl29nit[30] = {0, 0, 0, -6, -7, -6, 1, 0, -22, -4, -3, -17, -4, -1, -9, -1, 0, -7, -2, 0, 0, 1, 1, 0, 1, 0, 1, 3, 0, 2};
static signed char a3_ctbl30nit[30] = {0, 0, 0, 15, -7, -3, 1, 0, -23, 0, 2, -11, -3, -1, -9, -1, 0, -7, -3, -1, -1, -1, 0, -1, 2, 0, 1, 3, 0, 1};
static signed char a3_ctbl32nit[30] = {0, 0, 0, 16, -6, -8, 1, -1, -23, 2, 3, -8, 0, 2, -5, -2, 0, -7, -1, 0, 0, -1, 0, -1, 1, 0, 0, 3, 0, 2};
static signed char a3_ctbl34nit[30] = {0, 0, 0, 14, -6, -5, 0, -1, -23, 4, 3, -5, -1, 2, -5, 0, 1, -5, -1, 1, 0, -1, 0, -1, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl37nit[30] = {0, 0, 0, 12, -3, -3, -2, -3, -25, 2, 1, -7, -1, 2, -5, -1, 0, -6, -1, 1, 1, -1, 0, -1, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl39nit[30] = {0, 0, 0, 12, -6, -5, -4, -4, -26, 2, 1, -6, -2, 1, -6, 0, 1, -4, -3, -2, -1, 0, 0, 0, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl41nit[30] = {0, 0, 0, 14, -6, -2, 2, 1, -21, 1, 0, -7, -2, 0, -5, -1, -1, -6, -3, -1, -1, -1, 0, -1, 2, 0, 2, 3, 0, 2};
static signed char a3_ctbl44nit[30] = {0, 0, 0, 14, -6, -4, 1, 1, -22, 0, -1, -8, -4, -1, -6, -1, -1, -6, -1, 0, 1, -1, 0, -1, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl47nit[30] = {0, 0, 0, 13, -6, -8, 1, 0, -21, 0, -2, -6, -2, -1, -6, -1, 0, -4, -1, 0, 1, -1, 0, -2, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl50nit[30] = {0, 0, 0, 12, -6, -6, -1, -2, -24, 0, -2, -6, -3, -2, -6, -1, -1, -4, -1, 0, 0, -1, 0, 0, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl53nit[30] = {0, 0, 0, 13, -2, -7, -3, -3, -24, 3, -1, -3, -1, 2, -3, -1, -1, -4, 0, 1, 2, 0, 0, -1, 0, 0, 1, 3, 0, 2};
static signed char a3_ctbl56nit[30] = {0, 0, 0, 19, -1, -2, 2, 2, -18, 5, 1, 2, 1, 2, -3, 1, 1, -2, -1, 0, 1, -1, 0, -1, 1, 0, 1, 3, 0, 2};
static signed char a3_ctbl60nit[30] = {0, 0, 0, 14, -4, -4, 3, 2, -17, 2, -2, -1, 0, 1, -2, 0, 0, -3, -1, 0, 1, 0, 0, 0, 1, 0, 1, 3, 0, 2};
static signed char a3_ctbl64nit[30] = {0, 0, 0, 15, -4, -5, 1, 1, -18, 3, -3, -1, -2, 0, -3, 0, -1, -3, -1, 0, 0, 0, -1, 1, 1, 0, 1, 3, 0, 2};
static signed char a3_ctbl68nit[30] = {0, 0, 0, 15, -4, -5, 2, 1, -17, 6, 1, 4, -1, 0, -3, 2, 0, -2, -1, 0, 1, 0, 0, 0, 0, 0, 1, 3, 0, 2};
static signed char a3_ctbl72nit[30] = {0, 0, 0, 17, 0, -2, -3, -2, -22, 6, 2, 4, -2, 0, -3, 0, -2, -4, -1, 0, 1, 0, -1, 0, 0, 1, 1, 3, 0, 2};
static signed char a3_ctbl77nit[30] = {0, 0, 0, -11, 0, 4, -4, -2, -23, 5, 0, 2, -2, -1, -3, 1, 0, -2, 0, 1, 3, 1, 0, -1, 1, 0, 1, 4, 0, 3};
static signed char a3_ctbl82nit[30] = {0, 0, 0, 6, -2, -2, 2, 4, -12, 4, -1, 2, 2, 2, 0, 2, 2, -1, 0, 1, 2, 0, 0, 1, 0, 0, 0, 4, 0, 2};
static signed char a3_ctbl87nit[30] = {0, 0, 0, 18, 2, 2, 1, 1, -15, 1, -3, -1, 0, 2, 0, 2, 0, -3, 0, 1, 2, 1, 0, 1, 1, 0, 2, 3, 0, 2};
static signed char a3_ctbl93nit[30] = {0, 0, 0, 13, 0, -3, 0, 1, -15, 4, 1, 3, 0, 0, -2, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 4, 0, 2};
static signed char a3_ctbl98nit[30] = {0, 0, 0, -3, 3, -1, -3, -1, -18, 4, 1, 2, 2, 2, 1, 1, 1, 0, 1, 0, 2, -1, 0, -1, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl105nit[30] = {0, 0, 0, 11, 0, -5, 0, 4, -11, 2, -2, -1, 1, 1, 0, 1, 0, -2, 0, 0, 2, 0, 0, 0, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl111nit[30] = {0, 0, 0, 12, 3, -3, -3, 0, -14, 1, -1, -2, -1, -1, -3, 0, 0, -2, 0, 0, 2, 1, 0, 0, 0, 0, 1, 4, 0, 2};
static signed char a3_ctbl119nit[30] = {0, 0, 0, 15, 6, 0, -2, -2, -14, 0, -2, -2, 1, 3, 1, 1, 1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 2, 4, 0, 1};
static signed char a3_ctbl126nit[30] = {0, 0, 0, 3, 0, -6, -2, -1, -13, 2, 0, 0, 0, 0, -1, -1, -1, -2, 2, 1, 3, 0, 1, 1, 1, 0, 1, 3, 0, 2};
static signed char a3_ctbl134nit[30] = {0, 0, 0, 2, 0, -6, 2, 2, -7, 0, -1, -2, 0, 0, -2, 0, 0, 0, 1, 0, 2, 0, 0, 1, 0, 0, 0, 5, 0, 3};
static signed char a3_ctbl143nit[30] = {0, 0, 0, 6, 3, -3, 0, 0, -9, 3, 3, 1, 0, -1, -2, 0, 0, -1, 1, 1, 2, 0, 0, 1, 0, 0, 0, 5, 0, 3};
static signed char a3_ctbl152nit[30] = {0, 0, 0, 6, 3, -3, 0, 0, -6, 3, 3, 2, 1, 1, -1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 5, 0, 3};
static signed char a3_ctbl162nit[30] = {0, 0, 0, 2, 0, -8, -3, -2, -10, 1, 0, -1, 2, 2, 0, 1, -1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 4, 0, 2};
static signed char a3_ctbl172nit[30] = {0, 0, 0, 7, 3, -4, 2, 1, -6, 1, 0, -1, 3, 2, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 1, 0, 1, 4, 0, 2};
static signed char a3_ctbl183nit[30] = {0, 0, 0, 5, -1, -6, -3, -2, -5, -1, 0, -3, 2, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 4, 0, 2};
static signed char a3_ctbl195nit[30] = {0, 0, 0, 11, 3, -2, 2, 2, -2, 1, 2, 1, 2, 1, -1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 4, 0, 2};
static signed char a3_ctbl207nit[30] = {0, 0, 0, 6, 1, -7, 0, -1, -5, 1, 1, 1, 2, 0, -1, 0, 1, 1, 2, 0, 1, -1, 0, 0, 1, 0, 0, 5, 0, 3};
static signed char a3_ctbl220nit[30] = {0, 0, 0, 2, -1, -10, 3, 3, -1, 2, 1, 1, 0, -1, -1, -1, -1, -1, 2, 1, 3, -1, 0, -1, 1, 0, 1, 5, 0, 3};
static signed char a3_ctbl234nit[30] = {0, 0, 0, -1, 0, -4, -1, 0, -3, -2, -3, -2, 3, 1, 1, 1, 1, 2, 0, 0, 1, 1, 0, 0, 1, 0, 1, 4, 0, 3};
static signed char a3_ctbl249nit[30] = {0, 0, 0, 4, 3, -2, 3, 3, 2, 3, 0, 3, 2, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 3, 0, 2};
static signed char a3_ctbl265nit[30] = {0, 0, 0, 3, 0, -5, -1, 0, 0, 4, 1, 4, 2, 2, 1, 2, 1, 2, 1, 0, 2, 1, 0, 0, 0, 0, 1, 2, 0, 1};
static signed char a3_ctbl282nit[30] = {0, 0, 0, 7, 3, -2, -3, -3, -2, 1, 0, 1, 1, -1, 0, -1, -1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 3, 0, 1};
static signed char a3_ctbl300nit[30] = {0, 0, 0, 0, -2, -4, -4, -3, -3, 4, 3, 4, 0, -1, -1, -2, -2, -1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 3, 0, 1};
static signed char a3_ctbl316nit[30] = {0, 0, 0, 3, 1, -2, -2, -1, 0, 0, 0, 0, 1, -1, -1, -1, -1, 0, 0, -1, 0, 0, 0, 1, 1, 0, 0, 3, 0, 2};
static signed char a3_ctbl333nit[30] = {0, 0, 0, 1, -2, -5, -4, -4, -2, 0, 0, 1, 2, 0, 0, -1, -1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 2, 0, 1};
static signed char a3_ctbl350nit[30] = {0, 0, 0, 3, 1, -2, -1, -1, 2, 0, 0, 0, -1, -2, -2, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 2, 0, 0};
static signed char a3_ctbl357nit[30] = {0, 0, 0, 5, 3, 0, -4, -4, -2, -3, -3, -2, 1, -1, 0, -2, -2, -1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0};
static signed char a3_ctbl365nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_ctbl372nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_ctbl380nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_ctbl387nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_ctbl395nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_ctbl403nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_ctbl412nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_ctbl420nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char aid9412[3] = {0xB1, 0x90, 0x80};
static signed char aid9303[3] = {0xB1, 0x90, 0x64};
static signed char aid9226[3] = {0xB1, 0x90, 0x50};
static signed char aid9118[3] = {0xB1, 0x90, 0x34};
static signed char aid9040[3] = {0xB1, 0x90, 0x20};
static signed char aid8932[3] = {0xB1, 0x90, 0x04};
static signed char aid8854[3] = {0xB1, 0x80, 0xF0};
static signed char aid8781[3] = {0xB1, 0x80, 0xDD};
static signed char aid8676[3] = {0xB1, 0x80, 0xC2};
static signed char aid8599[3] = {0xB1, 0x80, 0xAE};
static signed char aid8491[3] = {0xB1, 0x80, 0x92};
static signed char aid8375[3] = {0xB1, 0x80, 0x74};
static signed char aid8189[3] = {0xB1, 0x80, 0x44};
static signed char aid8104[3] = {0xB1, 0x80, 0x2E};
static signed char aid8042[3] = {0xB1, 0x80, 0x1E};
static signed char aid7926[3] = {0xB1, 0x80, 0x00};
static signed char aid7740[3] = {0xB1, 0x70, 0xD0};
static signed char aid7624[3] = {0xB1, 0x70, 0xB2};
static signed char aid7430[3] = {0xB1, 0x70, 0x80};
static signed char aid7237[3] = {0xB1, 0x70, 0x4E};
static signed char aid6935[3] = {0xB1, 0x70, 0x00};
static signed char aid6745[3] = {0xB1, 0x60, 0xCF};
static signed char aid6440[3] = {0xB1, 0x60, 0x80};
static signed char aid6258[3] = {0xB1, 0x60, 0x51};
static signed char aid6068[3] = {0xB1, 0x60, 0x20};
static signed char aid5759[3] = {0xB1, 0x50, 0xD0};
static signed char aid5453[3] = {0xB1, 0x50, 0x81};
static signed char aid5143[3] = {0xB1, 0x50, 0x31};
static signed char aid4841[3] = {0xB1, 0x40, 0xE3};
static signed char aid4524[3] = {0xB1, 0x40, 0x91};
static signed char aid4094[3] = {0xB1, 0x40, 0x22};
static signed char aid3657[3] = {0xB1, 0x30, 0xB1};
static signed char aid3034[3] = {0xB1, 0x30, 0x10};
static signed char aid2496[3] = {0xB1, 0x20, 0x85};
static signed char aid1974[3] = {0xB1, 0x10, 0xFE};
static signed char aid1281[3] = {0xB1, 0x10, 0x4B};
static signed char aid813[3] = {0xB1, 0x00, 0xD2};
static signed char aid337[3] = {0xB1, 0x00, 0x57};
static signed char aid58[3] = {0xB1, 0x00, 0x0F};


static signed char a3_da_rtbl2nit[10] = {0, 31, 31, 31, 28, 25, 19, 13, 6, 0};
static signed char a3_da_rtbl3nit[10] = {0, 28, 28, 28, 25, 21, 16, 9, 5, 0};
static signed char a3_da_rtbl4nit[10] = {0, 27, 27, 25, 22, 19, 15, 9, 5, 0};
static signed char a3_da_rtbl5nit[10] = {0, 24, 24, 22, 20, 18, 14, 9, 5, 0};
static signed char a3_da_rtbl6nit[10] = {0, 24, 24, 21, 19, 17, 13, 8, 5, 0};
static signed char a3_da_rtbl7nit[10] = {0, 23, 23, 20, 18, 16, 12, 7, 4, 0};
static signed char a3_da_rtbl8nit[10] = {0, 22, 22, 19, 17, 15, 11, 6, 4, 0};
static signed char a3_da_rtbl9nit[10] = {0, 20, 20, 17, 15, 14, 10, 5, 3, 0};
static signed char a3_da_rtbl10nit[10] = {0, 19, 19, 16, 14, 13, 9, 5, 3, 0};
static signed char a3_da_rtbl11nit[10] = {0, 19, 19, 16, 14, 13, 9, 5, 4, 0};
static signed char a3_da_rtbl12nit[10] = {0, 18, 18, 15, 13, 12, 8, 5, 3, 0};
static signed char a3_da_rtbl13nit[10] = {0, 17, 17, 14, 12, 11, 7, 4, 3, 0};
static signed char a3_da_rtbl14nit[10] = {0, 17, 17, 14, 12, 11, 7, 4, 3, 0};
static signed char a3_da_rtbl15nit[10] = {0, 17, 16, 13, 11, 10, 7, 4, 2, 0};
static signed char a3_da_rtbl16nit[10] = {0, 17, 16, 13, 11, 10, 6, 3, 2, 0};
static signed char a3_da_rtbl17nit[10] = {0, 18, 16, 13, 11, 9, 5, 3, 2, 0};
static signed char a3_da_rtbl19nit[10] = {0, 16, 14, 11, 10, 9, 5, 3, 2, 0};
static signed char a3_da_rtbl20nit[10] = {0, 16, 14, 10, 9, 8, 4, 2, 2, 0};
static signed char a3_da_rtbl21nit[10] = {0, 16, 14, 10, 9, 8, 4, 2, 2, 0};
static signed char a3_da_rtbl22nit[10] = {0, 15, 13, 9, 8, 7, 4, 2, 2, 0};
static signed char a3_da_rtbl24nit[10] = {0, 14, 12, 8, 7, 6, 3, 2, 2, 0};
static signed char a3_da_rtbl25nit[10] = {0, 14, 12, 8, 7, 6, 3, 1, 2, 0};
static signed char a3_da_rtbl27nit[10] = {0, 13, 11, 8, 7, 6, 3, 1, 2, 0};
static signed char a3_da_rtbl29nit[10] = {0, 13, 11, 8, 7, 6, 3, 1, 1, 0};
static signed char a3_da_rtbl30nit[10] = {0, 12, 10, 7, 6, 5, 3, 1, 1, 0};
static signed char a3_da_rtbl32nit[10] = {0, 12, 10, 7, 6, 5, 2, 1, 1, 0};
static signed char a3_da_rtbl34nit[10] = {0, 11, 9, 6, 5, 4, 2, 1, 1, 0};
static signed char a3_da_rtbl37nit[10] = {0, 10, 8, 5, 4, 4, 2, 1, 1, 0};
static signed char a3_da_rtbl39nit[10] = {0, 10, 8, 5, 4, 4, 2, 1, 1, 0};
static signed char a3_da_rtbl41nit[10] = {0, 10, 8, 5, 4, 4, 1, 1, 1, 0};
static signed char a3_da_rtbl44nit[10] = {0, 8, 6, 4, 3, 3, 1, 1, 1, 0};
static signed char a3_da_rtbl47nit[10] = {0, 8, 6, 4, 3, 3, 1, 1, 0, 0};
static signed char a3_da_rtbl50nit[10] = {0, 8, 6, 4, 3, 3, 0, 0, 0, 0};
static signed char a3_da_rtbl53nit[10] = {0, 7, 5, 3, 2, 2, 0, 0, 0, 0};
static signed char a3_da_rtbl56nit[10] = {0, 6, 4, 2, 2, 2, 0, 0, 0, 0};
static signed char a3_da_rtbl60nit[10] = {0, 6, 4, 2, 2, 2, 0, 0, 0, 0};
static signed char a3_da_rtbl64nit[10] = {0, 6, 4, 2, 2, 2, 0, 0, 0, 0};
static signed char a3_da_rtbl68nit[10] = {0, 5, 3, 2, 1, 2, 0, 0, 2, 0};
static signed char a3_da_rtbl72nit[10] = {0, 5, 3, 2, 1, 1, 0, 0, 2, 0};
static signed char a3_da_rtbl77nit[10] = {0, 5, 4, 2, 1, 1, 1, 0, 1, 0};
static signed char a3_da_rtbl82nit[10] = {0, 5, 4, 2, 1, 1, 0, 1, 1, 0};
static signed char a3_da_rtbl87nit[10] = {0, 6, 5, 2, 1, 1, 1, 1, 0, 0};
static signed char a3_da_rtbl93nit[10] = {0, 4, 3, 2, 1, 1, 1, 2, 0, 0};
static signed char a3_da_rtbl98nit[10] = {0, 4, 3, 2, 1, 1, 1, 1, 0, 0};
static signed char a3_da_rtbl105nit[10] = {0, 4, 3, 1, 1, 1, 1, 2, 1, 0};
static signed char a3_da_rtbl111nit[10] = {0, 5, 4, 2, 1, 2, 1, 2, 0, 0};
static signed char a3_da_rtbl119nit[10] = {0, 4, 2, 1, 1, 1, 2, 3, 0, 0};
static signed char a3_da_rtbl126nit[10] = {0, 4, 3, 2, 2, 2, 2, 3, 0, 0};
static signed char a3_da_rtbl134nit[10] = {0, 4, 3, 2, 1, 2, 2, 2, 0, 0};
static signed char a3_da_rtbl143nit[10] = {0, 3, 2, 1, 1, 1, 2, 3, 0, 0};
static signed char a3_da_rtbl152nit[10] = {0, 4, 3, 2, 1, 1, 2, 3, 0, 0};
static signed char a3_da_rtbl162nit[10] = {0, 3, 1, 1, 1, 1, 2, 3, 0, 0};
static signed char a3_da_rtbl172nit[10] = {0, 3, 1, 1, 1, 1, 2, 4, 1, 0};
static signed char a3_da_rtbl183nit[10] = {0, 2, 1, 1, 1, 1, 1, 2, 1, 0};
static signed char a3_da_rtbl195nit[10] = {0, 2, 1, 1, 1, 1, 1, 2, 0, 0};
static signed char a3_da_rtbl207nit[10] = {0, 2, 1, 0, 1, 1, 1, 2, 1, 0};
static signed char a3_da_rtbl220nit[10] = {0, 1, 0, 0, 0, 0, 1, 1, 0, 0};
static signed char a3_da_rtbl234nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char a3_da_rtbl249nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_da_rtbl265nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_da_rtbl282nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 2, 0};
static signed char a3_da_rtbl300nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 2, 0};
static signed char a3_da_rtbl316nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl333nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_da_rtbl350nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_da_rtbl357nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char a3_da_rtbl365nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char a3_da_rtbl372nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 0, 0};
static signed char a3_da_rtbl380nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl387nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl395nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char a3_da_ctbl2nit[30] = {0, 0, 0, -13, 4, -22, -9, 4, -14, -6, 1, -15, -7, 1, -12, -15, 0, -15, -9, 1, -8, -5, 0, -6, -2, 0, -2, -7, 0, -10};
static signed char a3_da_ctbl3nit[30] = {0, 0, 0, -13, 4, -20, 1, 0, -14, -9, 0, -17, -11, -1, -13, -15, 0, -15, -9, 1, -9, -4, 1, -4, -1, 1, -1, -6, 0, -9};
static signed char a3_da_ctbl4nit[30] = {0, 0, 0, -13, 4, -20, 1, 0, -14, -10, 0, -19, -10, 1, -12, -12, 2, -14, -10, 1, -10, -3, 1, -3, -2, 0, -2, -4, 0, -7};
static signed char a3_da_ctbl5nit[30] = {0, 0, 0, -15, 1, -22, 1, 5, -14, -8, 3, -17, -7, 3, -10, -12, 0, -15, -8, 1, -9, -4, 0, -3, -2, 0, -2, -3, 0, -6};
static signed char a3_da_ctbl6nit[30] = {0, 0, 0, -15, 1, -22, -2, 2, -18, -9, 3, -16, -6, 3, -11, -11, 2, -14, -8, 1, -9, -3, 0, -3, -2, 0, -2, -2, 0, -5};
static signed char a3_da_ctbl7nit[30] = {0, 0, 0, -15, 2, -22, -5, 3, -20, -8, 2, -17, -6, 3, -11, -12, 0, -15, -8, 1, -9, -2, 0, -2, -1, 0, -1, -2, 0, -5};
static signed char a3_da_ctbl8nit[30] = {0, 0, 0, -15, 2, -22, -5, 4, -21, -8, 2, -16, -8, 1, -14, -11, 0, -14, -7, 1, -8, -2, 0, -2, -1, 0, -1, -1, 0, -4};
static signed char a3_da_ctbl9nit[30] = {0, 0, 0, -12, 4, -19, -6, 5, -22, -6, 3, -15, -5, 4, -11, -10, 2, -13, -7, 0, -9, -2, 1, -2, 0, 0, 0, -1, 0, -4};
static signed char a3_da_ctbl10nit[30] = {0, 0, 0, -16, 0, -23, -5, 4, -22, -7, 4, -15, -4, 4, -10, -10, 2, -13, -5, 1, -7, -3, 0, -3, 0, 0, 0, 0, 0, -3};
static signed char a3_da_ctbl11nit[30] = {0, 0, 0, -16, 0, -23, -5, 3, -23, -7, 3, -16, -4, 3, -10, -11, 0, -14, -5, 1, -7, -2, 0, -2, 0, 0, 0, 0, 0, -3};
static signed char a3_da_ctbl12nit[30] = {0, 0, 0, -13, 2, -21, -5, 3, -21, -8, 2, -18, -2, 4, -9, -9, 2, -14, -6, 1, -6, -1, 0, -2, -1, 0, 0, 1, 0, -2};
static signed char a3_da_ctbl13nit[30] = {0, 0, 0, 3, -3, -1, -7, 2, -25, -8, 2, -18, -1, 4, -8, -10, 0, -14, -4, 2, -5, -2, 0, -2, 0, 0, 0, 1, 0, -2};
static signed char a3_da_ctbl14nit[30] = {0, 0, 0, 3, -3, -1, -7, 2, -25, -8, 2, -18, -1, 4, -8, -10, 0, -14, -4, 2, -5, -2, 0, -2, 0, 0, 0, 1, 0, -2};
static signed char a3_da_ctbl15nit[30] = {0, 0, 0, -2, 11, -2, -7, 2, -26, -9, 2, -19, -1, 4, -7, -8, 3, -12, -5, 1, -6, -2, 0, -2, 0, 0, 0, 2, 0, -1};
static signed char a3_da_ctbl16nit[30] = {0, 0, 0, 5, 9, 2, -8, 1, -27, -9, 0, -20, -3, 4, -9, -10, 1, -13, -4, 0, -5, -2, 0, -2, 1, 0, 1, 1, 0, -2};
static signed char a3_da_ctbl17nit[30] = {0, 0, 0, -2, 2, -4, -9, 1, -28, -11, -2, -22, -7, -1, -11, -9, 1, -13, -3, 1, -3, -2, 0, -2, 0, 0, 0, 2, 0, -1};
static signed char a3_da_ctbl19nit[30] = {0, 0, 0, -3, 3, -5, -7, 5, -26, -7, 3, -19, -2, 2, -8, -11, -1, -13, -4, 0, -4, -1, 0, -1, 0, 0, 0, 2, 0, -1};
static signed char a3_da_ctbl20nit[30] = {0, 0, 0, -3, 2, -5, -11, -2, -32, -8, 3, -18, -2, 3, -6, -10, 0, -13, -2, 1, -3, -2, 0, -1, 1, 0, 0, 1, 0, -1};
static signed char a3_da_ctbl21nit[30] = {0, 0, 0, 0, 2, -5, -13, -3, -33, -8, 2, -18, -3, 2, -7, -10, -1, -13, -3, 1, -3, -1, 0, -1, 1, 0, 0, 1, 0, -1};
static signed char a3_da_ctbl22nit[30] = {0, 0, 0, 1, 4, -2, -10, -1, -32, -8, 2, -18, -4, 3, -6, -7, 1, -10, -3, 1, -3, -1, 0, -1, 0, 0, 0, 2, 0, -1};
static signed char a3_da_ctbl24nit[30] = {0, 0, 0, -3, 3, -5, -12, -2, -33, -6, 3, -17, -2, 2, -4, -7, 2, -10, -2, 1, -3, -1, 0, 0, 1, 0, 0, 1, 0, -1};
static signed char a3_da_ctbl25nit[30] = {0, 0, 0, -2, 3, -5, -13, -4, -35, -7, 2, -17, -2, 2, -4, -8, 1, -11, -3, 1, -2, 1, 1, 0, 0, 0, 1, 2, 0, -1};
static signed char a3_da_ctbl27nit[30] = {0, 0, 0, -4, 2, -7, -5, 5, -27, -9, 1, -19, -3, 0, -5, -8, 0, -10, -4, 0, -3, 1, 1, 0, 0, 0, 1, 2, 0, -1};
static signed char a3_da_ctbl29nit[30] = {0, 0, 0, -4, 1, -6, -7, 3, -30, -10, -1, -19, -3, -1, -5, -9, -1, -11, -3, 0, -3, -1, 0, 0, 1, 0, 1, 2, 0, -1};
static signed char a3_da_ctbl30nit[30] = {0, 0, 0, -3, 2, -7, -6, 3, -30, -8, 1, -17, -4, 0, -4, -5, 2, -7, -3, 0, -3, 0, 0, 0, 0, 0, 1, 2, 0, -1};
static signed char a3_da_ctbl32nit[30] = {0, 0, 0, -3, 3, -6, -8, 1, -32, -9, -1, -17, -5, -1, -5, -6, 0, -9, -2, 1, -2, -1, 0, 0, 1, 0, 1, 2, 0, -1};
static signed char a3_da_ctbl34nit[30] = {0, 0, 0, -2, 2, -7, -10, 0, -33, -5, 2, -13, -5, -2, -6, -3, 3, -6, -2, 1, -2, 0, 0, 0, 0, 0, 1, 2, 0, -1};
static signed char a3_da_ctbl37nit[30] = {0, 0, 0, -3, 2, -8, -11, 0, -35, -4, 3, -11, 0, 3, -1, -4, 2, -6, -2, 0, -2, -1, 0, -1, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl39nit[30] = {0, 0, 0, -3, 2, -8, -13, -2, -36, -5, 1, -12, -1, 2, -2, -4, 2, -6, -2, 0, -2, -1, -1, -1, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl41nit[30] = {0, 0, 0, -4, 2, -9, -15, -3, -38, -6, -1, -13, -1, 2, -1, -6, -1, -8, -1, 1, -1, -1, -1, -1, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl44nit[30] = {0, 0, 0, -4, 3, -10, -6, 6, -28, -3, 2, -9, -2, 1, -1, -2, 3, -4, -1, 1, -1, -1, -1, -1, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl47nit[30] = {0, 0, 0, -4, 2, -10, -9, 4, -30, -5, 1, -10, -2, 0, -1, -3, 2, -5, -1, 0, -1, -1, -1, -1, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl50nit[30] = {0, 0, 0, -3, 2, -10, -10, 1, -32, -6, -1, -10, -3, -1, -2, -4, -1, -6, 0, 1, 0, -1, 0, 0, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl53nit[30] = {0, 0, 0, -2, 1, -10, -11, 2, -32, -6, -1, -10, 0, 0, 0, -1, 3, -2, 0, 1, 0, -1, 0, 0, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl56nit[30] = {0, 0, 0, -2, 0, -10, -10, 2, -31, 1, 6, -4, 0, 0, 0, -1, 3, -2, 0, 0, 0, -1, 0, 0, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl60nit[30] = {0, 0, 0, -3, 0, -11, -13, -1, -33, 0, 5, -4, 0, -1, 0, -2, 1, -3, 0, 1, 0, -1, -1, 0, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl64nit[30] = {0, 0, 0, -3, -1, -12, -16, -3, -35, 0, 4, -4, -2, -3, -1, -1, 1, -3, -1, 0, 0, -1, -1, 0, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl68nit[30] = {0, 0, 0, -4, 0, -12, -6, 7, -25, 0, 3, -4, 2, 2, 3, -3, -1, -5, -1, 0, 0, 0, 0, -1, 0, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl72nit[30] = {0, 0, 0, -4, 0, -12, -7, 5, -26, 0, 2, -4, 3, 3, 4, -2, 1, -3, 1, 1, 1, 0, -1, 0, 0, 0, 0, 2, 0, 1};
static signed char a3_da_ctbl77nit[30] = {0, 0, 0, -3, -3, -13, -13, 1, -29, -2, 0, -6, 1, -1, 2, -2, 1, -4, 0, 0, 0, 0, 0, 0, 1, 0, 2, 2, 0, 0};
static signed char a3_da_ctbl82nit[30] = {0, 0, 0, -4, -2, -13, -15, -1, -30, -2, -1, -6, 1, -1, 2, 1, 3, -2, 0, 0, 1, 0, 1, 1, 0, 0, 1, 3, 0, 1};
static signed char a3_da_ctbl87nit[30] = {0, 0, 0, -5, -2, -14, -26, -14, -41, 4, 5, 1, -1, -3, 0, 0, 3, -1, -1, 0, -1, 1, 0, 1, 1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl93nit[30] = {0, 0, 0, 5, 5, -6, -9, 5, -22, -1, 0, -5, -1, -3, 0, 0, 2, -2, 0, 1, 1, 0, 0, 0, 2, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl98nit[30] = {0, 0, 0, 4, 5, -6, -10, 3, -23, -1, 0, -4, 2, 1, 3, -1, 1, -3, 0, 0, 1, 0, 0, 1, 2, 0, 1, 1, 0, 0};
static signed char a3_da_ctbl105nit[30] = {0, 0, 0, 4, 4, -7, -11, 2, -23, 3, 5, 0, 1, 0, 1, -1, 2, -2, 1, 1, 1, 0, 0, 1, 1, 0, 0, 2, 0, 1};
static signed char a3_da_ctbl111nit[30] = {0, 0, 0, 1, 5, -8, -14, -3, -25, -1, 1, -3, 2, 2, 3, -2, -1, -4, 0, 0, 0, -1, -1, 0, 1, 0, 1, 1, 0, 0};
static signed char a3_da_ctbl119nit[30] = {0, 0, 0, 1, 4, -8, -5, 6, -16, 0, 1, -3, 2, 2, 3, 0, 2, -1, 1, 0, 2, -1, 0, -1, 1, 0, 1, 2, 0, 1};
static signed char a3_da_ctbl126nit[30] = {0, 0, 0, 1, 4, -9, -9, 1, -19, 1, 3, -1, -1, -2, -1, -1, 0, -3, -1, 0, 0, 0, -1, 0, 1, 0, 0, 1, 0, 1};
static signed char a3_da_ctbl134nit[30] = {0, 0, 0, -1, 3, -10, -11, -1, -19, 0, 2, -2, 1, 1, 2, -2, -1, -4, 1, 0, 1, -1, 0, 0, 2, 0, 0, 1, 0, 0};
static signed char a3_da_ctbl143nit[30] = {0, 0, 0, -1, 1, -9, -3, 7, -11, 1, 2, -1, 2, 2, 2, 0, 1, -1, 1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0, 1};
static signed char a3_da_ctbl152nit[30] = {0, 0, 0, -3, 1, -11, -9, 0, -14, -3, -2, -4, 0, -1, 0, 0, 2, 0, 0, 0, 0, -1, 0, -1, 1, 0, 1, 1, 0, 1};
static signed char a3_da_ctbl162nit[30] = {0, 0, 0, -3, -1, -11, -2, 7, -7, -1, 1, -1, 0, -1, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 0, 0};
static signed char a3_da_ctbl172nit[30] = {0, 0, 0, -4, -1, -12, -2, 6, -6, -1, 1, -1, 3, 2, 2, -1, 0, -1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0};
static signed char a3_da_ctbl183nit[30] = {0, 0, 0, 5, 9, -2, 5, 12, 1, -5, -4, -5, 1, 1, 0, -1, -1, -2, 0, 0, -1, 1, 2, 2, 1, 0, 1, 2, 0, 1};
static signed char a3_da_ctbl195nit[30] = {0, 0, 0, 4, 7, -1, 3, 9, -1, -5, -5, -6, 2, 0, 2, -1, -1, -2, 0, -1, 0, 1, 1, -1, 0, 0, 1, 3, 0, 1};
static signed char a3_da_ctbl207nit[30] = {0, 0, 0, 4, 5, -2, -5, 1, -7, 0, 1, 0, 2, 0, 1, -1, -1, -1, 0, -1, -1, 1, 2, 2, 1, 0, 1, 2, 0, 1};
static signed char a3_da_ctbl220nit[30] = {0, 0, 0, 2, 6, -2, 5, 8, 2, -3, -4, -3, 1, 0, 1, -1, -1, -2, 1, 0, 2, 1, 1, 0, 1, 0, 1, 2, 0, 1};
static signed char a3_da_ctbl234nit[30] = {0, 0, 0, 2, 6, -2, 5, 8, 2, -3, -4, -3, 1, 0, 1, -3, -3, -4, 3, 2, 3, 1, 1, 0, 1, 0, 1, 2, 0, 1};
static signed char a3_da_ctbl249nit[30] = {0, 0, 0, 1, 3, -3, 4, 6, 1, -4, -4, -4, 2, -1, 1, 1, 0, 0, -1, -1, -1, 1, 1, 1, 0, 0, 0, 2, 0, 1};
static signed char a3_da_ctbl265nit[30] = {0, 0, 0, 1, 2, -3, 5, 6, 2, 1, 0, 1, 0, -2, 0, 0, -1, 0, 1, 1, 2, 1, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char a3_da_ctbl282nit[30] = {0, 0, 0, -4, -2, -5, -2, -2, -4, -3, -3, -3, 1, 0, 1, -3, -4, -2, 1, 0, 1, 0, -1, 0, 1, 1, 1, 1, 0, 0};
static signed char a3_da_ctbl300nit[30] = {0, 0, 0, -4, -3, -5, -3, -2, -4, 2, 3, 2, 0, 0, 0, -2, -4, -3, -1, -1, 1, 0, 0, 0, 2, 2, 2, 2, 0, 1};
static signed char a3_da_ctbl316nit[30] = {0, 0, 0, -5, -4, -4, 1, 0, 0, -7, -6, -6, -2, -3, -3, 0, -1, 1, 2, 2, 2, -1, -2, -1, 0, 0, 0, 2, 0, 2};
static signed char a3_da_ctbl333nit[30] = {0, 0, 0, -7, -5, -4, -2, 0, -2, -3, -4, -3, -2, -3, -2, 0, -1, 1, 1, 0, 0, 1, 1, 1, -1, 0, 0, 2, 0, 0};
static signed char a3_da_ctbl350nit[30] = {0, 0, 0, 5, 6, 4, 4, 4, 4, -3, -3, -3, -2, -3, -2, 2, 1, 2, -3, -3, -2, 2, 0, 1, -1, 0, 1, 2, 0, 0};
static signed char a3_da_ctbl357nit[30] = {0, 0, 0, 3, 4, 4, -6, -6, -5, -3, -3, -3, 0, 0, 1, -1, -2, -2, 0, -1, 1, 0, 1, 0, 0, 0, 1, 2, 0, 1};
static signed char a3_da_ctbl365nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_ctbl372nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_ctbl380nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_ctbl387nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_ctbl395nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_ctbl403nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_ctbl412nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_ctbl420nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//static signed char aid9795[3] = {0xB1, 0x90, 0xE3};
static signed char aid9698[3] = {0xB1, 0x90, 0xCA};
static signed char aid9594[3] = {0xB1, 0x90, 0xAF};
//static signed char aid9485[3] = {0xB1, 0x90, 0x93};
static signed char aid9396[3] = {0xB1, 0x90, 0x7C};
//static signed char aid9292[3] = {0xB1, 0x90, 0x61};
//static signed char aid9214[3] = {0xB1, 0x90, 0x4D};
//static signed char aid9106[3] = {0xB1, 0x90, 0x31};
//static signed char aid9029[3] = {0xB1, 0x90, 0x1D};
static signed char aid8924[3] = {0xB1, 0x90, 0x02};
static signed char aid8847[3] = {0xB1, 0x80, 0xEE};
static signed char aid8746[3] = {0xB1, 0x80, 0xD4};
static signed char aid8669[3] = {0xB1, 0x80, 0xC0};
static signed char aid8560[3] = {0xB1, 0x80, 0xA4};
static signed char aid8475[3] = {0xB1, 0x80, 0x8E};
static signed char aid8371[3] = {0xB1, 0x80, 0x73};
static signed char aid8185[3] = {0xB1, 0x80, 0x43};
static signed char aid8069[3] = {0xB1, 0x80, 0x25};
static signed char aid7999[3] = {0xB1, 0x80, 0x13};
static signed char aid7690[3] = {0xB1, 0x70, 0xC3};
static signed char aid7682[3] = {0xB1, 0x70, 0xC1};
static signed char aid7608[3] = {0xB1, 0x70, 0xAE};
static signed char aid7388[3] = {0xB1, 0x70, 0x75};
static signed char aid7194[3] = {0xB1, 0x70, 0x43};
static signed char aid7113[3] = {0xB1, 0x70, 0x2E};
static signed char aid6892[3] = {0xB1, 0x60, 0xF5};
static signed char aid6699[3] = {0xB1, 0x60, 0xC3};
static signed char aid6397[3] = {0xB1, 0x60, 0x75};
static signed char aid6204[3] = {0xB1, 0x60, 0x43};
static signed char aid5995[3] = {0xB1, 0x60, 0x0D};
static signed char aid5708[3] = {0xB1, 0x50, 0xC3};
static signed char aid5395[3] = {0xB1, 0x50, 0x72};
static signed char aid5089[3] = {0xB1, 0x50, 0x23};
static signed char aid4772[3] = {0xB1, 0x40, 0xD1};
static signed char aid4470[3] = {0xB1, 0x40, 0x83};
static signed char aid4044[3] = {0xB1, 0x40, 0x15};
static signed char aid3649[3] = {0xB1, 0x30, 0xAF};
static signed char aid3545[3] = {0xB1, 0x30, 0x94};
static signed char aid3092[3] = {0xB1, 0x30, 0x1F};
static signed char aid2570[3] = {0xB1, 0x20, 0x98};
static signed char aid2101[3] = {0xB1, 0x20, 0x1F};
static signed char aid1521[3] = {0xB1, 0x10, 0x89};
//static signed char aid1002[3] = {0xB1, 0x10, 0x03};
//static signed char aid755[3] = {0xB1, 0x00, 0xC3};
static signed char aid534[3] = {0xB1, 0x00, 0x8A};
static signed char aid461[3] = {0xB1, 0x00, 0x77};
static signed char aid197[3] = {0xB1, 0x00, 0x33};
//static signed char aid46[3] = {0xB1, 0x00, 0x0C};

#ifdef CONFIG_LCD_HMT
static signed char HF4_A3_HMTrtbl10nit[10] = {0, 12, 12, 10, 9, 8, 4, 1, 0, 0};
static signed char HF4_A3_HMTrtbl11nit[10] = {0, 12, 12, 10, 8, 6, 3, 1, 0, 0};
static signed char HF4_A3_HMTrtbl12nit[10] = {0, 12, 12, 11, 8, 6, 4, 1, 1, 0};
static signed char HF4_A3_HMTrtbl13nit[10] = {0, 12, 12, 9, 8, 6, 3, 1, 1, 0};
static signed char HF4_A3_HMTrtbl14nit[10] = {0, 13, 12, 10, 8, 6, 3, 1, 1, 0};
static signed char HF4_A3_HMTrtbl15nit[10] = {0, 12, 11, 10, 8, 6, 3, 1, 1, 0};
static signed char HF4_A3_HMTrtbl16nit[10] = {0, 12, 11, 9, 7, 5, 2, 2, 2, 0};
static signed char HF4_A3_HMTrtbl17nit[10] = {0, 12, 11, 9, 7, 5, 2, 1, 2, 0};
static signed char HF4_A3_HMTrtbl19nit[10] = {0, 12, 11, 8, 7, 5, 1, 1, 3, 0};
static signed char HF4_A3_HMTrtbl20nit[10] = {0, 12, 12, 9, 6, 5, 1, 1, 3, 0};
static signed char HF4_A3_HMTrtbl21nit[10] = {0, 12, 11, 9, 7, 5, 2, 0, 1, 0};
static signed char HF4_A3_HMTrtbl22nit[10] = {0, 12, 11, 10, 8, 6, 2, 1, 3, 0};
static signed char HF4_A3_HMTrtbl23nit[10] = {0, 12, 11, 8, 7, 4, 2, 0, 3, 0};
static signed char HF4_A3_HMTrtbl25nit[10] = {0, 13, 10, 9, 8, 5, 3, 1, 3, 0};
static signed char HF4_A3_HMTrtbl27nit[10] = {0, 12, 12, 9, 7, 5, 3, 1, 2, 0};
static signed char HF4_A3_HMTrtbl29nit[10] = {0, 12, 12, 8, 6, 4, 3, 1, 1, 0};
static signed char HF4_A3_HMTrtbl31nit[10] = {0, 12, 12, 9, 7, 5, 3, 1, 1, 0};
static signed char HF4_A3_HMTrtbl33nit[10] = {0, 12, 11, 8, 6, 5, 3, 2, 2, 0};
static signed char HF4_A3_HMTrtbl35nit[10] = {0, 12, 11, 9, 7, 6, 4, 3, 2, 0};
static signed char HF4_A3_HMTrtbl37nit[10] = {0, 12, 11, 8, 7, 5, 4, 2, 2, 0};
static signed char HF4_A3_HMTrtbl39nit[10] = {0, 12, 11, 8, 6, 5, 4, 1, 2, 0};
static signed char HF4_A3_HMTrtbl41nit[10] = {0, 13, 11, 9, 7, 5, 4, 1, 2, 0};
static signed char HF4_A3_HMTrtbl44nit[10] = {0, 12, 10, 8, 6, 5, 3, 1, 2, 0};
static signed char HF4_A3_HMTrtbl47nit[10] = {0, 12, 11, 8, 6, 5, 4, 1, 2, 0};
static signed char HF4_A3_HMTrtbl50nit[10] = {0, 12, 11, 8, 7, 5, 4, 1, 2, 0};
static signed char HF4_A3_HMTrtbl53nit[10] = {0, 12, 11, 8, 6, 5, 3, 1, 2, 0};
static signed char HF4_A3_HMTrtbl56nit[10] = {0, 12, 10, 8, 6, 5, 5, 2, 2, 0};
static signed char HF4_A3_HMTrtbl60nit[10] = {0, 12, 10, 8, 7, 5, 4, 3, 1, 0};
static signed char HF4_A3_HMTrtbl64nit[10] = {0, 12, 10, 8, 7, 5, 5, 3, 1, 0};
static signed char HF4_A3_HMTrtbl68nit[10] = {0, 11, 11, 9, 7, 6, 6, 5, 2, 0};
static signed char HF4_A3_HMTrtbl72nit[10] = {0, 11, 11, 9, 8, 7, 6, 5, 3, 0};
static signed char HF4_A3_HMTrtbl77nit[10] = {0, 8, 8, 5, 3, 3, 2, 0, -1, 0};
static signed char HF4_A3_HMTrtbl82nit[10] = {0, 8, 7, 5, 3, 3, 3, 0, -1, 0};
static signed char HF4_A3_HMTrtbl87nit[10] = {0, 8, 7, 5, 4, 3, 3, 1, 0, 0};
static signed char HF4_A3_HMTrtbl93nit[10] = {0, 8, 7, 4, 3, 3, 3, 1, 1, 0};
static signed char HF4_A3_HMTrtbl99nit[10] = {0, 8, 8, 5, 3, 3, 4, 2, -1, 0};
static signed char HF4_A3_HMTrtbl105nit[10] = {0, 8, 8, 6, 4, 4, 4, 2, 1, 0};


static signed char HF4_A3_HMTctbl10nit[30] = {0, 0, 0, 13, 4, 8, 9, 2, -6, 1, 5, -12, -2, 5, -11, -2, 4, -10, -4, 2, -6, 0, 0, -1, 0, 0, 0, 0, 0, -1};
static signed char HF4_A3_HMTctbl11nit[30] = {0, 0, 0, 13, 4, 6, 9, 2, -10, 2, 5, -10, -1, 4, -9, -3, 5, -11, -3, 2, -5, -2, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char HF4_A3_HMTctbl12nit[30] = {0, 0, 0, 13, 4, 6, 9, 2, -10, 0, 5, -10, -1, 4, -10, -4, 5, -10, -4, 2, -5, -2, 0, -1, 0, 0, 0, 1, 0, 1};
static signed char HF4_A3_HMTctbl13nit[30] = {0, 0, 0, 11, 4, 8, 7, 2, -10, 2, 5, -12, -1, 5, -10, -3, 4, -10, -3, 2, -4, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HF4_A3_HMTctbl14nit[30] = {0, 0, 0, 7, 1, 1, 7, 6, -11, 1, 5, -10, -2, 4, -9, -4, 4, -10, -3, 1, -4, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HF4_A3_HMTctbl15nit[30] = {0, 0, 0, 6, 1, 0, 7, 6, -12, 1, 4, -10, -2, 4, -9, -2, 4, -9, -3, 1, -4, -2, 0, -2, 0, 0, 0, 1, 0, 0};
static signed char HF4_A3_HMTctbl16nit[30] = {0, 0, 0, 5, 1, 0, 5, 6, -12, 0, 4, -10, -1, 4, -9, -3, 4, -9, -4, 1, -4, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HF4_A3_HMTctbl17nit[30] = {0, 0, 0, 10, 1, 3, 5, 7, -14, 1, 5, -10, -2, 4, -10, -1, 4, -8, -4, 1, -3, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char HF4_A3_HMTctbl19nit[30] = {0, 0, 0, 10, 1, 0, 5, 7, -14, 1, 5, -10, -1, 4, -8, -3, 3, -8, -3, 1, -3, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char HF4_A3_HMTctbl20nit[30] = {0, 0, 0, 9, 3, 0, 3, 6, -14, 0, 4, -9, -1, 4, -8, -4, 3, -8, -3, 1, -2, 0, 0, -1, 0, 0, 0, 2, 0, 1};
static signed char HF4_A3_HMTctbl21nit[30] = {0, 0, 0, 6, 3, 0, 5, 6, -13, 1, 5, -10, -2, 4, -8, -2, 3, -7, -4, 1, -3, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char HF4_A3_HMTctbl22nit[30] = {0, 0, 0, 6, 3, 0, 5, 6, -13, 0, 5, -10, -2, 3, -8, -4, 3, -7, -2, 1, -4, -1, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char HF4_A3_HMTctbl23nit[30] = {0, 0, 0, 5, 1, 0, 5, 6, -13, 0, 6, -12, -2, 3, -7, -4, 3, -8, -1, 1, -2, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char HF4_A3_HMTctbl25nit[30] = {0, 0, 0, 5, 2, -2, 5, 7, -14, 0, 5, -11, -2, 3, -7, -4, 3, -7, -1, 1, -2, -1, 0, 0, 0, 0, 1, 1, 0, 0};
static signed char HF4_A3_HMTctbl27nit[30] = {0, 0, 0, 4, 1, -2, 3, 6, -14, 0, 5, -11, -2, 3, -7, -3, 3, -6, -2, 1, -3, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char HF4_A3_HMTctbl29nit[30] = {0, 0, 0, 4, 1, -2, 3, 6, -14, -1, 5, -12, -2, 3, -8, -4, 3, -6, -1, 1, -2, 0, 0, 0, 0, 0, 0, 3, 0, 1};
static signed char HF4_A3_HMTctbl31nit[30] = {0, 0, 0, 4, 1, -2, 3, 6, -14, -1, 5, -11, -2, 3, -8, -2, 3, -6, -3, 0, -2, -1, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HF4_A3_HMTctbl33nit[30] = {0, 0, 0, 6, 3, -2, 2, 6, -15, 0, 5, -10, -2, 3, -8, -3, 2, -6, -1, 1, -2, 0, 0, 0, 0, 0, 1, 1, 0, 1};
static signed char HF4_A3_HMTctbl35nit[30] = {0, 0, 0, 4, 4, -5, 2, 6, -15, 0, 5, -10, -2, 3, -6, -3, 2, -6, -2, 0, -2, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char HF4_A3_HMTctbl37nit[30] = {0, 0, 0, 6, 6, -5, 2, 6, -15, -1, 5, -11, -3, 2, -6, -3, 2, -6, -1, 0, -1, -1, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char HF4_A3_HMTctbl39nit[30] = {0, 0, 0, 4, 2, -5, 2, 6, -15, -1, 5, -10, -1, 3, -6, -3, 2, -5, -2, 0, -2, -1, 0, 0, 0, 0, 0, 3, 0, 3};
static signed char HF4_A3_HMTctbl41nit[30] = {0, 0, 0, 1, 0, -7, 1, 7, -14, 0, 4, -9, -3, 3, -7, -3, 2, -5, -2, 0, -1, 0, 0, -2, 0, 0, 0, 3, 0, 3};
static signed char HF4_A3_HMTctbl44nit[30] = {0, 0, 0, 1, 0, -8, 1, 8, -17, 0, 4, -8, -2, 3, -7, -3, 2, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 3, 0, 2};
static signed char HF4_A3_HMTctbl47nit[30] = {0, 0, 0, 3, 2, -6, 0, 6, -15, -1, 4, -10, -2, 3, -7, -3, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 4, 0, 3};
static signed char HF4_A3_HMTctbl50nit[30] = {0, 0, 0, 4, 3, -6, 1, 7, -16, -2, 4, -10, -2, 2, -6, -3, 1, -4, -3, 0, -2, 0, 0, 0, 0, 0, 0, 4, 0, 4};
static signed char HF4_A3_HMTctbl53nit[30] = {0, 0, 0, 4, 3, -6, 1, 7, -16, -2, 4, -10, -2, 2, -6, -4, 1, -4, -2, 0, -1, 0, 0, -1, 0, 0, 0, 5, 0, 4};
static signed char HF4_A3_HMTctbl56nit[30] = {0, 0, 0, 2, 2, -6, 0, 7, -16, -1, 4, -8, -2, 3, -6, -3, 1, -4, -2, 0, -1, 0, 0, -1, 0, 0, 0, 4, 0, 3};
static signed char HF4_A3_HMTctbl60nit[30] = {0, 0, 0, 3, 3, -6, 1, 8, -16, -2, 4, -10, -3, 2, -6, -1, 1, -2, -2, 0, -1, 0, 0, -1, 0, 0, 0, 4, 0, 4};
static signed char HF4_A3_HMTctbl64nit[30] = {0, 0, 0, 5, 3, -3, 0, 8, -18, -2, 4, -8, -3, 2, -6, -2, 1, -2, 0, 0, -1, -1, 0, -1, 1, 0, 0, 4, 0, 4};
static signed char HF4_A3_HMTctbl68nit[30] = {0, 0, 0, 5, 3, -3, 0, 8, -16, -1, 4, -8, -3, 2, -6, -2, 1, -2, -1, 0, -1, -1, 0, -1, -1, 0, -1, 4, -1, 4};
static signed char HF4_A3_HMTctbl72nit[30] = {0, 0, 0, 5, 3, -3, 0, 8, -16, -2, 4, -9, -2, 2, -6, -2, 1, -2, -2, 0, -1, -1, 0, -1, 0, 0, -1, 4, -1, 4};
static signed char HF4_A3_HMTctbl77nit[30] = {0, 0, 0, 5, 3, -5, 2, 6, -12, -2, 3, -8, -1, 2, -4, -1, 1, -2, 0, 0, -1, -1, 0, 0, 0, 0, 0, 4, 0, 4};
static signed char HF4_A3_HMTctbl82nit[30] = {0, 0, 0, 5, 3, -5, 1, 7, -15, 0, 3, -7, -2, 1, -4, -2, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 4, -1, 4};
static signed char HF4_A3_HMTctbl87nit[30] = {0, 0, 0, 6, 4, -5, 1, 7, -14, -1, 3, -7, -2, 1, -4, -1, 1, -2, -2, 0, -2, 0, 0, 0, 0, 0, 0, 4, 0, 4};
static signed char HF4_A3_HMTctbl93nit[30] = {0, 0, 0, 6, 4, -5, 2, 7, -14, -2, 3, -7, -3, 1, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 4};
static signed char HF4_A3_HMTctbl99nit[30] = {0, 0, 0, 2, 0, -6, 0, 6, -13, 0, 3, -6, -3, 1, -4, -1, 0, -2, 0, 0, 0, -2, 0, -1, 0, 0, 0, 5, 0, 4};
static signed char HF4_A3_HMTctbl105nit[30] = {0, 0, 0, 4, 1, -6, 0, 6, -14, -1, 3, -6, -3, 1, -4, -1, 1, -2, 0, 0, 0, -2, 0, -1, 0, 0, 0, 4, -1, 4};

static signed char HF4_A3_HMTaid7999[3] = {0xB1, 0x20, 0x05};
static signed char HF4_A3_HMTaid7001[3] = {0xB1, 0X30, 0x07};

static signed char HF4_A3_da_HMTrtbl10nit[10] = {0, 12, 11, 9, 7, 6, 4, 0, -1, 0};
static signed char HF4_A3_da_HMTrtbl11nit[10] = {0, 12, 11, 9, 8, 6, 4, 1, -2, 0};
static signed char HF4_A3_da_HMTrtbl12nit[10] = {0, 12, 11, 10, 7, 6, 4, 1, -2, 0};
static signed char HF4_A3_da_HMTrtbl13nit[10] = {0, 12, 11, 9, 8, 6, 4, 0, -2, 0};
static signed char HF4_A3_da_HMTrtbl14nit[10] = {0, 12, 11, 9, 7, 6, 3, 1, -2, 0};
static signed char HF4_A3_da_HMTrtbl15nit[10] = {0, 12, 11, 9, 8, 6, 4, 1, -2, 0};
static signed char HF4_A3_da_HMTrtbl16nit[10] = {0, 12, 11, 8, 7, 5, 3, 1, -2, 0};
static signed char HF4_A3_da_HMTrtbl17nit[10] = {0, 12, 11, 9, 7, 6, 3, 0, -2, 0};
static signed char HF4_A3_da_HMTrtbl19nit[10] = {0, 12, 11, 8, 7, 5, 2, -1, -1, 0};
static signed char HF4_A3_da_HMTrtbl20nit[10] = {0, 12, 11, 8, 8, 6, 3, 0, -1, 0};
static signed char HF4_A3_da_HMTrtbl21nit[10] = {0, 12, 11, 8, 7, 5, 3, -1, -1, 0};
static signed char HF4_A3_da_HMTrtbl22nit[10] = {0, 12, 11, 9, 7, 5, 3, 0, 0, 0};
static signed char HF4_A3_da_HMTrtbl23nit[10] = {0, 12, 11, 8, 7, 5, 3, 0, 0, 0};
static signed char HF4_A3_da_HMTrtbl25nit[10] = {0, 12, 11, 8, 7, 5, 3, 0, 1, 0};
static signed char HF4_A3_da_HMTrtbl27nit[10] = {0, 12, 11, 7, 7, 5, 3, 0, 0, 0};
static signed char HF4_A3_da_HMTrtbl29nit[10] = {0, 12, 11, 8, 7, 5, 4, 0, -1, 0};
static signed char HF4_A3_da_HMTrtbl31nit[10] = {0, 12, 10, 7, 7, 5, 4, 1, 0, 0};
static signed char HF4_A3_da_HMTrtbl33nit[10] = {0, 12, 10, 7, 6, 5, 4, 1, 0, 0};
static signed char HF4_A3_da_HMTrtbl35nit[10] = {0, 12, 10, 8, 6, 6, 4, 2, 0, 0};
static signed char HF4_A3_da_HMTrtbl37nit[10] = {0, 12, 10, 7, 7, 6, 5, 2, -1, 0};
static signed char HF4_A3_da_HMTrtbl39nit[10] = {0, 13, 11, 8, 6, 5, 4, 1, -2, 0};
static signed char HF4_A3_da_HMTrtbl41nit[10] = {0, 13, 11, 8, 7, 5, 4, 2, -1, 0};
static signed char HF4_A3_da_HMTrtbl44nit[10] = {0, 12, 10, 7, 6, 5, 4, 3, -1, 0};
static signed char HF4_A3_da_HMTrtbl47nit[10] = {0, 12, 10, 8, 6, 6, 5, 2, -1, 0};
static signed char HF4_A3_da_HMTrtbl50nit[10] = {0, 12, 10, 7, 7, 6, 5, 4, -1, 0};
static signed char HF4_A3_da_HMTrtbl53nit[10] = {0, 12, 10, 8, 6, 5, 4, 3, -1, 0};
static signed char HF4_A3_da_HMTrtbl56nit[10] = {0, 12, 10, 7, 6, 5, 5, 3, -1, 0};
static signed char HF4_A3_da_HMTrtbl60nit[10] = {0, 12, 10, 8, 7, 5, 5, 4, 0, 0};
static signed char HF4_A3_da_HMTrtbl64nit[10] = {0, 12, 10, 7, 7, 5, 5, 4, 0, 0};
static signed char HF4_A3_da_HMTrtbl68nit[10] = {0, 11, 10, 8, 7, 5, 5, 4, 0, 0};
static signed char HF4_A3_da_HMTrtbl72nit[10] = {0, 11, 9, 7, 6, 6, 5, 4, 0, 0};
static signed char HF4_A3_da_HMTrtbl77nit[10] = {0, 8, 7, 4, 4, 3, 3, 2, -3, 0};
static signed char HF4_A3_da_HMTrtbl82nit[10] = {0, 8, 6, 4, 4, 3, 4, 2, -2, 0};
static signed char HF4_A3_da_HMTrtbl87nit[10] = {0, 8, 6, 4, 4, 3, 4, 3, -1, 0};
static signed char HF4_A3_da_HMTrtbl93nit[10] = {0, 8, 6, 4, 4, 3, 4, 3, -1, 0};
static signed char HF4_A3_da_HMTrtbl99nit[10] = {0, 8, 7, 5, 4, 4, 4, 4, 1, 0};
static signed char HF4_A3_da_HMTrtbl105nit[10] = {0, 8, 7, 4, 5, 4, 4, 4, 0, 0};

static signed char HF4_A3_da_HMTctbl10nit[30] = {0, 0, 0, 6, -12, 1, -9, 6, -21, -3, 6, -12, -7, 6, -14, -8, 6, -12, -2, 3, -6, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HF4_A3_da_HMTctbl11nit[30] = {0, 0, 0, 10, -8, 1, -7, 8, -21, -4, 7, -15, -6, 6, -13, -6, 6, -12, -5, 2, -6, -2, 0, -2, 0, 0, 0, 0, 0, 0};
static signed char HF4_A3_da_HMTctbl12nit[30] = {0, 0, 0, 12, -6, -2, -5, 10, -21, -5, 4, -10, -7, 6, -14, -6, 6, -12, -4, 2, -5, -2, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HF4_A3_da_HMTctbl13nit[30] = {0, 0, 0, 12, -6, -2, -5, 10, -21, -5, 5, -12, -6, 6, -12, -7, 5, -12, -3, 2, -4, -2, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HF4_A3_da_HMTctbl14nit[30] = {0, 0, 0, 2, 0, -2, -11, 9, -19, -4, 5, -12, -7, 6, -13, -6, 5, -11, -3, 2, -4, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HF4_A3_da_HMTctbl15nit[30] = {0, 0, 0, 0, 0, -4, -15, 5, -22, -5, 6, -14, -6, 5, -12, -5, 5, -11, -4, 2, -5, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HF4_A3_da_HMTctbl16nit[30] = {0, 0, 0, 0, 0, -4, -15, 5, -22, -5, 6, -14, -6, 5, -12, -6, 5, -11, -3, 2, -4, -1, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HF4_A3_da_HMTctbl17nit[30] = {0, 0, 0, 2, 2, -4, -15, 7, -22, -7, 5, -12, -6, 6, -12, -6, 4, -10, -3, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HF4_A3_da_HMTctbl19nit[30] = {0, 0, 0, 2, 2, -4, -15, 7, -22, -7, 6, -14, -6, 5, -12, -6, 4, -10, -3, 1, -3, 0, 0, -2, 1, 0, 0, 0, 0, 2};
static signed char HF4_A3_da_HMTctbl20nit[30] = {0, 0, 0, 2, 3, -4, -17, 5, -23, -5, 6, -14, -7, 5, -11, -5, 4, -9, -3, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HF4_A3_da_HMTctbl21nit[30] = {0, 0, 0, 1, 2, -5, -17, 5, -23, -5, 7, -14, -6, 5, -11, -6, 4, -9, -3, 1, -4, -1, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HF4_A3_da_HMTctbl22nit[30] = {0, 0, 0, 6, 8, -1, -18, 4, -24, -7, 5, -12, -7, 5, -11, -4, 3, -8, -2, 1, -3, -1, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HF4_A3_da_HMTctbl23nit[30] = {0, 0, 0, 6, 8, -1, -18, 4, -24, -8, 6, -13, -6, 5, -11, -3, 3, -7, -2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF4_A3_da_HMTctbl25nit[30] = {0, 0, 0, 6, 8, -1, -17, 5, -23, -7, 6, -13, -7, 5, -11, -3, 3, -6, -1, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 2};
static signed char HF4_A3_da_HMTctbl27nit[30] = {0, 0, 0, -6, 0, -9, -16, 6, -22, -8, 7, -14, -6, 4, -10, -3, 3, -6, -2, 1, -2, -1, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HF4_A3_da_HMTctbl29nit[30] = {0, 0, 0, -6, 1, -9, -14, 8, -20, -7, 6, -12, -6, 4, -9, -3, 2, -6, -2, 1, -3, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF4_A3_da_HMTctbl31nit[30] = {0, 0, 0, -4, 1, -7, -14, 8, -20, -7, 6, -13, -7, 4, -10, -4, 2, -6, -1, 1, -2, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HF4_A3_da_HMTctbl33nit[30] = {0, 0, 0, -4, 1, -8, -14, 8, -21, -7, 6, -13, -6, 4, -10, -3, 2, -6, -1, 0, -2, 0, 0, 0, 0, 0, 0, 1, 0, 3};
static signed char HF4_A3_da_HMTctbl35nit[30] = {0, 0, 0, -5, 0, -8, -14, 8, -22, -7, 5, -12, -5, 4, -9, -3, 2, -5, -1, 1, -2, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HF4_A3_da_HMTctbl37nit[30] = {0, 0, 0, -4, 2, -8, -14, 8, -23, -7, 6, -13, -5, 4, -8, -3, 2, -5, -1, 0, -2, -1, 0, -1, 0, 0, 0, 1, 0, 3};
static signed char HF4_A3_da_HMTctbl39nit[30] = {0, 0, 0, -4, 0, -10, -18, 5, -25, -7, 5, -10, -5, 3, -8, -4, 2, -6, 0, 0, -1, -1, 0, -1, 0, 0, 0, 2, 0, 3};
static signed char HF4_A3_da_HMTctbl41nit[30] = {0, 0, 0, -4, 0, -10, -17, 6, -24, -5, 5, -11, -6, 4, -8, -3, 2, -5, -2, 0, -2, 0, 0, 0, 0, 0, 0, 3, 0, 3};
static signed char HF4_A3_da_HMTctbl44nit[30] = {0, 0, 0, -7, -1, -13, -17, 7, -24, -6, 5, -10, -6, 4, -8, -3, 2, -5, -1, 0, -1, -1, 0, -1, 1, 0, 0, 3, 0, 4};
static signed char HF4_A3_da_HMTctbl47nit[30] = {0, 0, 0, -6, -1, -13, -16, 7, -23, -6, 5, -11, -5, 4, -8, -3, 1, -4, -2, 0, -2, -2, 0, -1, 1, 0, 0, 3, 0, 4};
static signed char HF4_A3_da_HMTctbl50nit[30] = {0, 0, 0, -6, -1, -12, -16, 7, -22, -5, 5, -11, -5, 3, -8, -3, 2, -4, -1, 0, -1, -1, 0, 0, 0, 0, 0, 3, 0, 4};
static signed char HF4_A3_da_HMTctbl53nit[30] = {0, 0, 0, -6, 3, -11, -16, 8, -21, -6, 4, -10, -5, 3, -8, -1, 1, -3, -2, 0, -2, 0, 0, 0, 0, 0, 0, 3, 0, 4};
static signed char HF4_A3_da_HMTctbl56nit[30] = {0, 0, 0, -5, 5, -11, -16, 5, -21, -6, 5, -10, -5, 3, -8, -2, 1, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 4, 0, 4};
static signed char HF4_A3_da_HMTctbl60nit[30] = {0, 0, 0, -5, 6, -11, -16, 5, -21, -6, 4, -10, -4, 3, -6, -3, 1, -4, -1, 0, -1, 0, 0, 0, 1, 0, 0, 2, 0, 3};
static signed char HF4_A3_da_HMTctbl64nit[30] = {0, 0, 0, -5, 6, -10, -15, 5, -20, -4, 4, -9, -5, 3, -6, -2, 1, -4, -2, 0, -1, 0, 0, 0, 0, 0, 0, 3, 0, 4};
static signed char HF4_A3_da_HMTctbl68nit[30] = {0, 0, 0, -5, 6, -10, -15, 5, -20, -5, 4, -9, -4, 2, -6, -1, 1, -3, -1, 0, 0, 0, 0, -1, 0, 0, 0, 3, 0, 4};
static signed char HF4_A3_da_HMTctbl72nit[30] = {0, 0, 0, -5, 6, -10, -13, 5, -20, -5, 4, -8, -3, 3, -6, -2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 4};
static signed char HF4_A3_da_HMTctbl77nit[30] = {0, 0, 0, -5, 4, -9, -8, 4, -19, -5, 4, -7, -2, 2, -4, -3, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 3};
static signed char HF4_A3_da_HMTctbl82nit[30] = {0, 0, 0, -3, 4, -9, -6, 6, -18, -6, 3, -8, -3, 2, -5, -2, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 3, 0, 3};
static signed char HF4_A3_da_HMTctbl87nit[30] = {0, 0, 0, -4, 4, -11, -6, 5, -18, -6, 3, -8, -3, 2, -5, -1, 1, -2, -1, 0, -1, 0, 0, 0, 1, 0, 0, 3, 0, 4};
static signed char HF4_A3_da_HMTctbl93nit[30] = {0, 0, 0, -4, 4, -12, -7, 7, -16, -4, 3, -7, -2, 2, -5, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF4_A3_da_HMTctbl99nit[30] = {0, 0, 0, -7, 0, -15, -9, 4, -18, -4, 3, -6, -2, 2, -5, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF4_A3_da_HMTctbl105nit[30] = {0, 0, 0, -7, -1, -15, -9, 7, -16, -4, 2, -5, -2, 1, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 3, 0, 4};

static unsigned char HF4_HMTelv[3] = {
	0xAC, 0x0A
};
static unsigned char HF4_HMTelvCaps[3] = {
	0xBC, 0x0A
};

#endif

static signed char aid9799[3] = {0xB1, 0x90, 0xE4};
static signed char aid9717[3] = {0xB1, 0x90, 0xCF};
static signed char aid9601[3] = {0xB1, 0x90, 0xB1};
static signed char aid9489[3] = {0xB1, 0x90, 0x94};
static signed char aid9404[3] = {0xB1, 0x90, 0x7E};
static signed char aid9300[3] = {0xB1, 0x90, 0x63};
static signed char aid9222[3] = {0xB1, 0x90, 0x4F};
static signed char aid9114[3] = {0xB1, 0x90, 0x33};
static signed char aid9036[3] = {0xB1, 0x90, 0x1F};
static signed char aid8963[3] = {0xB1, 0x90, 0x0C};
static signed char aid8858[3] = {0xB1, 0x80, 0xF1};
static signed char aid8789[3] = {0xB1, 0x80, 0xDF};
static signed char aid8680[3] = {0xB1, 0x80, 0xC3};
static signed char aid8243[3] = {0xB1, 0x80, 0x52};
static signed char aid8131[3] = {0xB1, 0x80, 0x35};
static signed char aid8046[3] = {0xB1, 0x80, 0x1F};
static signed char aid7786[3] = {0xB1, 0x70, 0xDC};
static signed char aid7488[3] = {0xB1, 0x70, 0x8F};
static signed char aid7291[3] = {0xB1, 0x70, 0x5C};
static signed char aid7187[3] = {0xB1, 0x70, 0x41};
static signed char aid7001[3] = {0xB1, 0x70, 0x11};
static signed char aid6788[3] = {0xB1, 0x60, 0xDA};
static signed char aid6502[3] = {0xB1, 0x60, 0x90};
static signed char aid6308[3] = {0xB1, 0x60, 0x5E};
static signed char aid6115[3] = {0xB1, 0x60, 0x2C};
static signed char aid5813[3] = {0xB1, 0x50, 0xDE};
static signed char aid5507[3] = {0xB1, 0x50, 0x8F};
static signed char aid5159[3] = {0xB1, 0x50, 0x35};
static signed char aid4884[3] = {0xB1, 0x40, 0xEE};
static signed char aid4559[3] = {0xB1, 0x40, 0x9A};
static signed char aid4141[3] = {0xB1, 0x40, 0x2E};
static signed char aid3727[3] = {0xB1, 0x30, 0xC3};
static signed char aid3560[3] = {0xB1, 0x30, 0x98};
static signed char aid3487[3] = {0xB1, 0x30, 0x85};
static signed char aid2968[3] = {0xB1, 0x20, 0xFF};
static signed char aid2415[3] = {0xB1, 0x20, 0x70};
static signed char aid1931[3] = {0xB1, 0x10, 0xF3};
static signed char aid1312[3] = {0xB1, 0x10, 0x53};
static signed char aid759[3] = {0xB1, 0x00, 0xC4};
static signed char aid402[3] = {0xB1, 0x00, 0x68};
static signed char aid151[3] = {0xB1, 0x00, 0x27};

static signed char a2_final_rtbl2nit[10] = {0, 28, 28, 28, 25, 21, 17, 11, 7, 0};
static signed char a2_final_rtbl3nit[10] = {0, 25, 25, 25, 22, 18, 16, 10, 6, 0};
static signed char a2_final_rtbl4nit[10] = {0, 21, 21, 21, 19, 16, 14, 9, 6, 0};
static signed char a2_final_rtbl5nit[10] = {0, 21, 21, 21, 18, 15, 13, 7, 5, 0};
static signed char a2_final_rtbl6nit[10] = {0, 22, 22, 21, 18, 15, 13, 8, 6, 0};
static signed char a2_final_rtbl7nit[10] = {0, 21, 21, 19, 16, 13, 12, 7, 5, 0};
static signed char a2_final_rtbl8nit[10] = {0, 20, 20, 18, 15, 13, 11, 6, 5, 0};
static signed char a2_final_rtbl9nit[10] = {0, 21, 20, 17, 14, 12, 10, 5, 4, 0};
static signed char a2_final_rtbl10nit[10] = {0, 18, 17, 16, 13, 11, 9, 4, 4, 0};
static signed char a2_final_rtbl11nit[10] = {0, 18, 17, 15, 12, 10, 8, 4, 4, 0};
static signed char a2_final_rtbl12nit[10] = {0, 17, 16, 15, 12, 10, 8, 5, 5, 0};
static signed char a2_final_rtbl13nit[10] = {0, 18, 17, 15, 12, 9, 7, 3, 3, 0};
static signed char a2_final_rtbl14nit[10] = {0, 19, 17, 14, 11, 8, 6, 2, 2, 0};
static signed char a2_final_rtbl15nit[10] = {0, 18, 15, 12, 9, 7, 5, 2, 2, 0};
static signed char a2_final_rtbl16nit[10] = {0, 17, 15, 11, 9, 7, 5, 3, 3, 0};
static signed char a2_final_rtbl17nit[10] = {0, 17, 14, 11, 9, 7, 5, 2, 3, 0};
static signed char a2_final_rtbl19nit[10] = {0, 14, 13, 11, 8, 6, 4, 2, 3, 0};
static signed char a2_final_rtbl20nit[10] = {0, 13, 12, 9, 7, 5, 4, 2, 3, 0};
static signed char a2_final_rtbl21nit[10] = {0, 13, 12, 9, 7, 5, 4, 2, 3, 0};
static signed char a2_final_rtbl22nit[10] = {0, 13, 12, 9, 7, 5, 4, 2, 3, 0};
static signed char a2_final_rtbl24nit[10] = {0, 13, 12, 9, 7, 5, 4, 2, 3, 0};
static signed char a2_final_rtbl25nit[10] = {0, 13, 11, 8, 6, 5, 4, 1, 3, 0};
static signed char a2_final_rtbl27nit[10] = {0, 13, 11, 8, 6, 5, 4, 2, 4, 0};
static signed char a2_final_rtbl29nit[10] = {0, 11, 10, 7, 5, 4, 3, 1, 3, 0};
static signed char a2_final_rtbl30nit[10] = {0, 11, 9, 6, 4, 3, 3, 2, 3, 0};
static signed char a2_final_rtbl32nit[10] = {0, 11, 9, 6, 4, 3, 3, 1, 3, 0};
static signed char a2_final_rtbl34nit[10] = {0, 11, 9, 6, 4, 3, 3, 1, 3, 0};
static signed char a2_final_rtbl37nit[10] = {0, 11, 9, 6, 4, 3, 2, 1, 3, 0};
static signed char a2_final_rtbl39nit[10] = {0, 11, 9, 6, 4, 3, 2, 1, 3, 0};
static signed char a2_final_rtbl41nit[10] = {0, 10, 8, 4, 3, 2, 2, 1, 3, 0};
static signed char a2_final_rtbl44nit[10] = {0, 9, 7, 4, 2, 2, 1, 0, 2, 0};
static signed char a2_final_rtbl47nit[10] = {0, 8, 7, 4, 2, 2, 1, 0, 3, 0};
static signed char a2_final_rtbl50nit[10] = {0, 8, 7, 4, 2, 1, 1, 0, 0, 0};
static signed char a2_final_rtbl53nit[10] = {0, 8, 6, 3, 2, 1, 0, 0, 1, 0};
static signed char a2_final_rtbl56nit[10] = {0, 7, 5, 3, 2, 1, 0, 0, 2, 0};
static signed char a2_final_rtbl60nit[10] = {0, 7, 4, 2, 1, 1, 1, 0, 1, 0};
static signed char a2_final_rtbl64nit[10] = {0, 6, 4, 2, 0, 0, 0, 0, 1, 0};
static signed char a2_final_rtbl68nit[10] = {0, 6, 5, 3, 2, 2, 1, 0, 0, 0};
static signed char a2_final_rtbl72nit[10] = {0, 7, 5, 3, 1, 1, 1, 0, 1, 0};
static signed char a2_final_rtbl77nit[10] = {0, 5, 4, 2, 1, 0, 0, 0, 1, 0};
static signed char a2_final_rtbl82nit[10] = {0, 5, 4, 2, 1, 0, 1, 0, 1, 0};
static signed char a2_final_rtbl87nit[10] = {0, 4, 4, 2, 1, 0, 1, 1, 1, 0};
static signed char a2_final_rtbl93nit[10] = {0, 5, 4, 2, 1, 0, 1, 1, 0, 0};
static signed char a2_final_rtbl98nit[10] = {0, 5, 4, 2, 1, 1, 0, 1, 0, 0};
static signed char a2_final_rtbl105nit[10] = {0, 5, 4, 2, 1, 0, 0, 2, 0, 0};
static signed char a2_final_rtbl111nit[10] = {0, 5, 4, 2, 1, 1, 1, 1, 1, 0};
static signed char a2_final_rtbl119nit[10] = {0, 4, 3, 2, 1, 1, 2, 2, 1, 0};
static signed char a2_final_rtbl126nit[10] = {0, 4, 3, 1, 1, 1, 2, 1, 1, 0};
static signed char a2_final_rtbl134nit[10] = {0, 4, 3, 2, 1, 1, 1, 1, 1, 0};
static signed char a2_final_rtbl143nit[10] = {0, 4, 3, 1, 0, 1, 1, 1, 1, 0};
static signed char a2_final_rtbl152nit[10] = {0, 4, 3, 2, 1, 1, 1, 2, 1, 0};
static signed char a2_final_rtbl162nit[10] = {0, 3, 2, 1, 0, 0, 2, 3, 0, 0};
static signed char a2_final_rtbl172nit[10] = {0, 3, 2, 1, 1, 1, 2, 3, 0, 0};
static signed char a2_final_rtbl183nit[10] = {0, 3, 2, 0, 0, 0, 0, 1, 1, 0};
static signed char a2_final_rtbl195nit[10] = {0, 3, 1, 0, 0, 0, 0, 1, 0, 0};
static signed char a2_final_rtbl207nit[10] = {0, 2, 1, 0, 0, 0, 0, 2, 1, 0};
static signed char a2_final_rtbl220nit[10] = {0, 2, 1, 0, 0, 0, 0, 2, 0, 0};
static signed char a2_final_rtbl234nit[10] = {0, 2, 1, 0, 0, 0, 0, 2, 0, 0};
static signed char a2_final_rtbl249nit[10] = {0, 1, 0, 0, 0, 0, 0, 2, 0, 0};
static signed char a2_final_rtbl265nit[10] = {0, 1, 1, 1, 0, 0, 0, 2, 1, 0};
static signed char a2_final_rtbl282nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char a2_final_rtbl300nit[10] = {0, 1, 0, 1, 0, 0, 0, 1, 1, 0};
static signed char a2_final_rtbl316nit[10] = {0, 1, 1, 0, 0, 0, 0, 1, 0, 0};
static signed char a2_final_rtbl333nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 1, 0};
static signed char a2_final_rtbl350nit[10] = {0, 1, 1, 0, 0, 0, 0, 1, 0, 0};
static signed char a2_final_rtbl357nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_rtbl365nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char a2_final_rtbl372nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_rtbl380nit[10] = {0, 1, 0, 0, 0, 1, 0, 0, 0, 0};
static signed char a2_final_rtbl387nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_rtbl395nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_rtbl403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_rtbl412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char a2_final_ctbl2nit[30] = {0, 0, 0, -23, -6, -28, -34, -27, -35, -6, 1, -10, -10, 1, -9, -18, 0, -10, -9, 1, -4, -4, 0, -4, -3, 0, -2, -9, 0, -8};
static signed char a2_final_ctbl3nit[30] = {0, 0, 0, -22, -7, -28, -34, -27, -34, -2, 0, -11, -15, 0, -11, -12, 1, -8, -9, -1, -7, -3, 0, -3, -3, 0, -2, -6, 0, -7};
static signed char a2_final_ctbl4nit[30] = {0, 0, 0, -21, -6, -28, -25, -17, -24, -4, 1, -9, -11, 1, -9, -11, 0, -9, -7, 0, -6, -3, 0, -3, -2, 0, -1, -5, 0, -6};
static signed char a2_final_ctbl5nit[30] = {0, 0, 0, -24, -10, -31, -9, -2, -8, -7, 0, -12, -12, 0, -9, -11, 0, -9, -6, 0, -6, -2, 0, -2, -2, 0, -1, -4, 0, -5};
static signed char a2_final_ctbl6nit[30] = {0, 0, 0, -20, -8, -27, -2, 3, -2, -8, 0, -12, -10, 0, -10, -11, 0, -9, -6, 0, -6, -2, 0, -2, -2, 0, -1, -3, 0, -4};
static signed char a2_final_ctbl7nit[30] = {0, 0, 0, -16, -4, -23, -1, 3, -2, -10, 0, -13, -11, -1, -12, -8, 1, -9, -6, 0, -5, -1, 0, -2, -2, 0, -1, -2, 0, -3};
static signed char a2_final_ctbl8nit[30] = {0, 0, 0, -14, 1, -20, -7, 0, -7, -9, 1, -12, -7, 1, -9, -9, 0, -10, -6, -1, -5, -1, 0, -2, -1, 0, 0, -2, 0, -3};
static signed char a2_final_ctbl9nit[30] = {0, 0, 0, -13, 0, -19, -6, 0, -7, -8, 1, -11, -9, 0, -11, -9, -1, -10, -5, 0, -4, -1, 0, -2, -1, 0, -1, -1, 0, -2};
static signed char a2_final_ctbl10nit[30] = {0, 0, 0, -15, -1, -22, -6, 2, -6, -6, 1, -11, -8, 0, -11, -9, 1, -10, -4, 0, -4, 0, 0, -1, -1, 0, -1, -1, 0, -2};
static signed char a2_final_ctbl11nit[30] = {0, 0, 0, -15, -1, -22, -9, -2, -9, -7, 1, -12, -6, 1, -9, -8, 0, -9, -4, 0, -4, 0, 0, -1, -1, 0, 0, 0, 0, -1};
static signed char a2_final_ctbl12nit[30] = {0, 0, 0, -14, 0, -20, -6, 2, -6, -7, 0, -13, -7, 1, -8, -8, -1, -10, -3, 0, -3, 0, 0, -1, -1, 0, 0, 0, 0, -1};
static signed char a2_final_ctbl13nit[30] = {0, 0, 0, -11, 2, -18, -5, 1, -6, -7, -1, -14, -10, -1, -12, -8, -1, -10, -3, 0, -3, 0, 0, -1, -1, 0, -1, 1, 0, 0};
static signed char a2_final_ctbl14nit[30] = {0, 0, 0, -13, 0, -18, -6, 0, -8, -6, -1, -13, -10, -1, -10, -8, -1, -10, -3, 0, -3, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char a2_final_ctbl15nit[30] = {0, 0, 0, -12, 0, -17, -6, -1, -10, -6, 0, -14, -7, 2, -6, -7, 0, -10, -1, 1, -1, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char a2_final_ctbl16nit[30] = {0, 0, 0, -9, 1, -15, -7, -2, -11, -5, 2, -13, -7, 2, -7, -7, 0, -9, -2, 1, -2, 0, 0, 0, 0, 0, -1, 1, 0, 0};
static signed char a2_final_ctbl17nit[30] = {0, 0, 0, -10, 0, -15, -3, 1, -9, -5, 2, -12, -7, 1, -7, -8, -1, -10, -1, 0, -2, 0, 0, 0, -1, 0, -1, 1, 0, 0};
static signed char a2_final_ctbl19nit[30] = {0, 0, 0, -13, 1, -20, -5, 1, -6, -9, -2, -16, -7, 0, -6, -7, -1, -9, -1, 1, -2, 0, 0, 1, 0, 1, 0, 2, 0, 1};
static signed char a2_final_ctbl20nit[30] = {0, 0, 0, -1, 1, -8, -4, 2, -5, -6, 1, -13, -6, 1, -5, -6, 1, -8, 0, 1, -1, 0, 0, 0, 0, 1, 0, 2, 0, 1};
static signed char a2_final_ctbl21nit[30] = {0, 0, 0, 3, 4, -5, -1, 5, -3, -8, -2, -16, -7, 1, -5, -6, 1, -8, -1, 1, -1, 0, 0, -1, 1, 1, 1, 2, 0, 1};
static signed char a2_final_ctbl22nit[30] = {0, 0, 0, 3, 4, -5, -1, 5, -3, -8, -2, -16, -7, 1, -5, -6, 1, -8, -1, 1, -1, 0, 0, -1, 1, 1, 1, 2, 0, 1};
static signed char a2_final_ctbl24nit[30] = {0, 0, 0, -12, 1, -16, -7, -1, -8, -9, -2, -14, -7, -1, -6, -6, 0, -8, -1, 0, -2, 0, 0, 1, 0, 0, -1, 2, 0, 1};
static signed char a2_final_ctbl25nit[30] = {0, 0, 0, -13, -1, -17, -3, 3, -5, -6, 0, -12, -5, 2, -2, -7, -1, -9, -1, 0, -2, 1, 0, 1, 0, 1, 0, 2, 0, 1};
static signed char a2_final_ctbl27nit[30] = {0, 0, 0, -9, 2, -14, -5, 0, -8, -9, -2, -15, -5, 1, -2, -6, -1, -8, -1, 0, -2, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char a2_final_ctbl29nit[30] = {0, 0, 0, -8, 1, -15, -6, 0, -7, -7, 0, -11, -5, 1, -4, -4, -1, -6, 0, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0, 1};
static signed char a2_final_ctbl30nit[30] = {0, 0, 0, -9, -1, -15, -2, 4, -5, -5, 2, -10, -5, 1, -3, -3, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char a2_final_ctbl32nit[30] = {0, 0, 0, -5, 2, -12, -4, 1, -8, -7, -1, -12, -6, 1, -3, -2, 1, -4, 0, 0, -1, 1, 0, 1, 0, 0, 0, 2, 0, 1};
static signed char a2_final_ctbl34nit[30] = {0, 0, 0, -3, 5, -9, -8, 1, -22, -5, 2, -9, -6, 0, -3, -3, 0, -5, 0, -1, 0, 0, 0, -1, 0, 0, 1, 2, 0, 1};
static signed char a2_final_ctbl37nit[30] = {0, 0, 0, -6, 2, -11, -10, 0, -25, -9, -2, -12, -6, 0, -4, -4, -1, -6, 0, 1, 0, 1, 0, 1, 0, 0, 0, 2, 0, 1};
static signed char a2_final_ctbl39nit[30] = {0, 0, 0, -4, 5, -8, -10, 1, -22, -8, -2, -11, -7, -1, -3, -4, -1, -6, 0, 0, 0, 1, 0, -1, 0, 0, 1, 2, 0, 1};
static signed char a2_final_ctbl41nit[30] = {0, 0, 0, -6, 2, -10, -15, -2, -24, -2, 3, -7, -5, -1, -2, -2, 1, -4, 0, 0, 0, 1, 0, 1, 0, 0, 0, 2, 0, 1};
static signed char a2_final_ctbl44nit[30] = {0, 0, 0, -3, 4, -7, -13, -2, -24, -4, 2, -7, -3, 2, 1, -2, -1, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char a2_final_ctbl47nit[30] = {0, 0, 0, -7, 2, -13, -14, -1, -25, -5, -2, -7, -3, 1, -1, -3, -1, -4, 1, 0, 0, 1, 1, 0, -1, 0, 0, 2, 0, 1};
static signed char a2_final_ctbl50nit[30] = {0, 0, 0, -8, 2, -15, -15, -2, -26, -5, -2, -7, -6, -2, -4, 0, 2, -2, 1, 0, 1, 0, 0, -1, 0, 0, 0, 2, 0, 1};
static signed char a2_final_ctbl53nit[30] = {0, 0, 0, -10, -1, -16, -13, -1, -22, -2, 2, -5, -6, -2, -3, -3, -1, -5, 1, 1, 1, 1, 1, 1, 1, 0, 1, 2, 0, 1};
static signed char a2_final_ctbl56nit[30] = {0, 0, 0, -6, 2, -12, -10, 3, -20, 1, 4, -2, -7, -2, -3, -2, -1, -4, 0, 0, 0, 2, 1, 2, 0, 0, -1, 2, 0, 1};
static signed char a2_final_ctbl60nit[30] = {0, 0, 0, -9, 0, -14, -10, 1, -22, 0, 2, -4, -1, 2, 2, -1, 0, -3, 0, -1, 0, -1, 0, -1, 1, 0, 0, 2, 0, 2};
static signed char a2_final_ctbl64nit[30] = {0, 0, 0, -5, 2, -9, -13, -2, -24, -3, 0, -6, 0, 3, 2, 1, 1, -1, 1, 1, 1, 1, 0, 1, 0, 0, -1, 2, 0, 2};
static signed char a2_final_ctbl68nit[30] = {0, 0, 0, -1, 5, -6, -13, 0, -22, -1, 1, -4, -6, -2, -2, -2, -1, -4, 1, 0, 0, -1, -2, 0, 2, 0, 1, 1, 0, 1};
static signed char a2_final_ctbl72nit[30] = {0, 0, 0, -5, 2, -9, -16, -2, -25, -2, -1, -6, -4, -1, 0, -1, 0, -3, 1, 0, 1, 0, -1, -2, 1, 1, 1, 2, 0, 2};
static signed char a2_final_ctbl77nit[30] = {0, 0, 0, 3, 2, 1, -12, 2, -19, 0, 2, -4, -4, -2, -1, 0, 1, -1, 2, 0, 1, 0, 0, 0, 1, 1, 2, 2, 0, 1};
static signed char a2_final_ctbl82nit[30] = {0, 0, 0, 0, -1, -3, -9, 2, -16, 0, 4, -1, -4, -2, -1, 1, 1, -2, 0, 0, 1, 0, -1, 0, 2, 1, 0, 2, 0, 2};
static signed char a2_final_ctbl87nit[30] = {0, 0, 0, 6, 2, 1, -6, 3, -13, -2, 1, -3, -2, 1, 2, 1, 1, -2, 0, 0, 1, 1, 0, 0, 1, 0, 0, 2, 0, 2};
static signed char a2_final_ctbl93nit[30] = {0, 0, 0, 4, -1, -5, -7, 0, -15, -4, -1, -5, -1, 1, 3, -1, 1, -3, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1};
static signed char a2_final_ctbl98nit[30] = {0, 0, 0, 7, 3, -2, -11, -1, -18, 1, 2, -2, -3, -1, 0, -1, 0, -3, 2, 0, 1, 1, 0, 0, 0, 0, 1, 2, 0, 1};
static signed char a2_final_ctbl105nit[30] = {0, 0, 0, 4, 1, -5, -9, 2, -15, -3, -1, -4, 0, 1, 2, -1, 0, -2, 1, 1, 1, 1, 0, 0, 1, 0, 2, 1, 0, 2};
static signed char a2_final_ctbl111nit[30] = {0, 0, 0, 7, 5, -2, -11, -1, -18, -2, 1, -2, -2, 0, 0, 0, 0, -2, 1, 1, 1, 1, 0, 1, 1, 1, 1, 2, 0, 1};
static signed char a2_final_ctbl119nit[30] = {0, 0, 0, 11, 9, 2, -6, 4, -12, -2, -1, -4, -2, -1, 0, 1, 2, 0, 1, 0, 0, 0, 0, 1, 2, 0, 1, 1, 0, 1};
static signed char a2_final_ctbl126nit[30] = {0, 0, 0, 8, 4, 0, -9, 1, -14, 1, 1, -1, 0, 2, 2, 0, 1, 0, 1, 0, -1, 2, 1, 2, 1, 0, 2, 1, 0, 1};
static signed char a2_final_ctbl134nit[30] = {0, 0, 0, 4, 1, -4, -7, 4, -11, -2, -1, -3, -2, -1, 1, -1, 0, -2, 1, 0, 2, 1, 1, 0, 1, 1, 2, 1, 0, 0};
static signed char a2_final_ctbl143nit[30] = {0, 0, 0, -5, -2, -11, -6, 2, -10, -1, -1, -3, 0, 2, 3, 0, 1, -1, 0, -1, 0, 3, 2, 2, 1, 1, 1, 1, 0, 1};
static signed char a2_final_ctbl152nit[30] = {0, 0, 0, -1, 1, -8, -6, 3, -8, -1, 0, -2, 0, 1, 2, -1, 0, -1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1};
static signed char a2_final_ctbl162nit[30] = {0, 0, 0, 3, 4, -4, -6, 1, -9, -2, -1, -2, -1, 0, 1, 1, 2, 0, 1, 0, 1, 1, 1, 0, 1, 0, 2, 0, 0, 0};
static signed char a2_final_ctbl172nit[30] = {0, 0, 0, 6, 7, 0, -9, -1, -12, -1, 0, -2, 0, 2, 2, 0, 0, 0, 0, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, 0};
static signed char a2_final_ctbl183nit[30] = {0, 0, 0, 2, 4, -3, -5, 2, -7, -2, -1, -2, 0, 2, 2, 0, 1, 0, -1, -2, -1, 5, 5, 5, 0, 0, 0, 1, 0, 1};
static signed char a2_final_ctbl195nit[30] = {0, 0, 0, -2, 1, -6, -4, 4, -4, 1, 2, 0, 1, 2, 2, -1, 0, 0, 0, 0, 1, 4, 3, 3, 0, 0, 0, 1, 0, 1};
static signed char a2_final_ctbl207nit[30] = {0, 0, 0, 3, 3, -1, -1, 6, -1, -1, -1, -2, 0, 2, 2, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char a2_final_ctbl220nit[30] = {0, 0, 0, 0, 1, -4, -4, 2, -4, 1, 2, 1, 0, 1, 1, -1, 0, -1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char a2_final_ctbl234nit[30] = {0, 0, 0, 3, 4, 0, -7, 0, -7, -1, -1, -2, 1, 1, 2, -1, -1, 0, 1, 0, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl249nit[30] = {0, 0, 0, 0, 0, -1, -2, 3, 0, 2, 2, 0, 1, 1, 2, 1, 1, 0, 1, 1, 2, 0, 0, 0, 1, 0, 1, 0, 0, 0};
static signed char a2_final_ctbl265nit[30] = {0, 0, 0, 5, 2, 2, -2, 4, 0, -1, -1, -2, 0, 1, 1, 1, 1, 2, 0, -2, -1, 1, 1, 1, 0, 0, 1, 0, 0, 0};
static signed char a2_final_ctbl282nit[30] = {0, 0, 0, 1, 1, -1, -2, 2, -1, 0, 1, 0, 0, 1, 2, -1, -2, -1, 0, 0, -1, 1, 1, 2, 2, 0, 1, 0, 0, 0};
static signed char a2_final_ctbl300nit[30] = {0, 0, 0, 3, 4, 2, -1, 3, 1, -2, -2, -2, -1, 0, 0, -1, -2, -1, 1, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl316nit[30] = {0, 0, 0, 1, 4, 1, -3, -2, -2, -3, -2, -3, 0, 2, 2, 1, 0, 1, -1, -2, -2, 2, 1, 2, -1, 0, -1, 0, 0, 0};
static signed char a2_final_ctbl333nit[30] = {0, 0, 0, -2, 1, -1, 0, 1, 1, 0, 1, 1, -3, -2, -1, 1, 0, 1, 0, -1, -1, -1, -1, -1, 1, 1, 1, 0, 0, 0};
static signed char a2_final_ctbl350nit[30] = {0, 0, 0, 0, 4, 2, -3, -1, -2, -3, -3, -2, 0, 1, 0, -1, -1, 0, 0, -1, -1, 2, 1, 1, 0, 0, 1, 0, 0, 0};
static signed char a2_final_ctbl357nit[30] = {0, 0, 0, -2, 1, 0, -1, 2, 1, 0, 0, 0, -3, -2, -2, 2, 2, 2, 0, -1, 0, 1, 0, 1, -1, 0, -1, 0, 0, 0};
static signed char a2_final_ctbl365nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl372nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl380nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl387nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl395nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl403nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl412nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a2_final_ctbl420nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#endif

