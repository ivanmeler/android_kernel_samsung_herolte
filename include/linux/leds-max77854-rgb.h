/*
 * RGB-led driver for Maxim MAX77854
 *
 * Copyright (C) 2013 Maxim Integrated Product
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LEDS_MAX77854_RGB_H__
#define __LEDS_MAX77854_RGB_H__

struct max77854_rgb_platform_data
{
	char *name[4];
};

#endif /* __LEDS_MAX77854_RGB_H__ */
