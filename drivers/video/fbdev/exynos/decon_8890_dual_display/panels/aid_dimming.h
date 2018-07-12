/* linux/drivers/video/exynos_decon/panel/aid_dimming.h
 *
 * Header file for Samsung AID Dimming Driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __AID_DIMMING_H__
#define __AID_DIMMING_H__

#if defined(CONFIG_PANEL_S6E3HA2_DYNAMIC)
#include "s6e3ha2_wqhd_dimming.h"
#elif defined(CONFIG_PANEL_S6E3HA3_DYNAMIC)
#include "s6e3ha3_wqhd_dimming.h"
#elif defined(CONFIG_PANEL_S6E3HF3_DYNAMIC)
#include "s6e3ha3_wqhd_dimming.h"
#elif defined(CONFIG_PANEL_S6E3HF2_DYNAMIC)
#include "s6e3hf2_wqhd_dimming.h"
#elif defined(CONFIG_PANEL_S6E3HF4_WQHD)
#include "s6e3hf4_wqhd_dimming.h"
#else
#error "ERROR !! Check LCD Panel Header File"
#endif

#define	DIMMING_METHOD_AID				0
#define DIMMING_METHOD_FILL_CENTER		1
#define DIMMING_METHOD_INTERPOLATION	2
#define DIMMING_METHOD_FILL_HBM			3
#define DIMMING_METHOD_MAX				4

#define W1	DIMMING_METHOD_AID
#define W2	DIMMING_METHOD_FILL_CENTER
#define W3	DIMMING_METHOD_INTERPOLATION
#define W4	DIMMING_METHOD_FILL_HBM
enum {
	CI_RED = 0,
	CI_GREEN,
	CI_BLUE,
	CI_MAX,
};

struct dim_data {
	int t_gamma[NUM_VREF][CI_MAX];
	int mtp[NUM_VREF][CI_MAX];
	int volt[MAX_GRADATION][CI_MAX];
	int volt_vt[CI_MAX];
	int vt_mtp[CI_MAX];
	int look_volt[NUM_VREF][CI_MAX];
};

struct SmtDimInfo {
	unsigned int br;
	unsigned int refBr;
	const unsigned int *cGma;
	signed char *rTbl;
	signed char *cTbl;
	unsigned char *aid;
	unsigned char *elvCaps;
	unsigned char *elv;
	unsigned char gamma[OLED_CMD_GAMMA_CNT];
	unsigned int way;
	signed char* elvss_offset;
};

#endif

