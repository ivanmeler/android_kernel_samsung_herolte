/*
 * cirrus-calibration.h -- CS35L33 calibration driver
 *
 * Copyright 2015 Cirrus Logic, Inc.
 *
 * Author: Nikesh Oswal <Nikesh.Oswal@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define CALIB_CLASS_NAME "cirrus"
#define CALIB_DIR_NAME "cirrus_calib"

#define FILEPATH_CRUS_RDC_CAL "/efs/cirrus/rdc_cal"
#define FILEPATH_CRUS_TEMP_CAL "/efs/cirrus/temp_cal"

extern int cirrus_amp_calib_set_regmap(struct regmap *regmap);

extern int cirrus_amp_calib_apply(void);
