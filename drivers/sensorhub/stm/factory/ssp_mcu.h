/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SSP_MCU_H__
#define __SSP_MCU_H__

#include "../ssp.h"

/* Factory Test */
#define FACTORY_DATA_MAX	100
#define MCU_FACTORY		0x8C
#define MCU_SLEEP_FACTORY	0x8D


ssize_t mcu_reset_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_dump_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_revision_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_update_ums_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_update_kernel_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_update_kernel_crashed_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_factorytest_store(struct device *, struct device_attribute *,
	const char *, size_t);
ssize_t mcu_factorytest_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_model_name_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_sleep_factorytest_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_sleep_factorytest_store(struct device *,
	struct device_attribute *, const char *, size_t);

#endif
