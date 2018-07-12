/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEVICE_S5K4H5YC_H
#define FIMC_IS_DEVICE_S5K4H5YC_H

#define SENSOR_S5K4H5YC_INSTANCE	0
#define SENSOR_S5K4H5YC_FF_INSTANCE	0

#ifdef USE_FF_MODULE
#define SENSOR_S5K4H5YC_NAME    SENSOR_NAME_S5K4H5YC_FF
#else
#define SENSOR_S5K4H5YC_NAME    SENSOR_NAME_S5K4H5YC
#endif

int sensor_4h5yc_probe(struct platform_device *pdev);

#endif

