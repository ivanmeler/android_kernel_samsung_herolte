/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_I2C_H
#define FIMC_IS_I2C_H

#include <linux/i2c.h>

/* I2C PIN MODE */
enum {
	I2C_PIN_STATE_DEFAULT = 0,
	I2C_PIN_STATE_ON,
	I2C_PIN_STATE_OFF,
	I2C_PIN_STATE_HOST,
	I2C_PIN_STATE_FW,
};

int fimc_is_i2c_s_pin(struct i2c_client *client, int state);
#endif
