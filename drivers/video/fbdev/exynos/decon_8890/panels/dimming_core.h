/* linux/drivers/video/exynos_decon/panel/dimming_core.h
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

#ifndef __DIMMING_CORE_H__
#define __DIMMING_CORE_H__

//#define SMART_DIMMING_DEBUG
#define CONFIG_REF_SHIFT
#define CONFIG_COLOR_SHIFT

#include "../dsim.h"
#include "aid_dimming.h"


#define dimm_err(fmt, ...)					\
	do {							\
		pr_err(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define dimm_info(fmt, ...)					\
	do {							\
		pr_info(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define dimm_dbg(fmt, ...)					\
	do {							\
		pr_debug(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

int generate_volt_table(struct dim_data *data);
int cal_gamma_from_index(struct dim_data *data, struct SmtDimInfo *brInfo);

#endif
