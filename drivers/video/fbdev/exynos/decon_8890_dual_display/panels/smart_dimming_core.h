/* linux/drivers/video/exynos_decon/panel/smart_dimming_core.h
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

#ifndef __SMART_DIMMING_H__
#define __SMART_DIMMING_H__

#include "s6e3fa3_fhd_aid_basic_info.h"

struct volt_info {
	unsigned int vt[CI_MAX];
	unsigned int volt[MAX_GRAYSCALE][CI_MAX];
	unsigned int tmpv[VREF_POINT_CNT][CI_MAX];
};

struct dimming_param {
	int center_gma[VREF_POINT_CNT][CI_MAX];
	int ref_gma[VREF_POINT_CNT][CI_MAX];
	int mtp[VREF_POINT_CNT][CI_MAX];
	int vt_mtp[CI_MAX];
};

struct gamma_cmd {
	unsigned char gamma[GAMMA_COMMAND_CNT];
	unsigned char aor[AOR_COMMAND_CNT];
};

#define LOG dsim_info

//int init_smart_dimming(struct panel_info *command, char *refgamma, char *mtp);

#endif // __SMART_DIMMING_H__

