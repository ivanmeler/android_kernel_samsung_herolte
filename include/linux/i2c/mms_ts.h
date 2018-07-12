/*
 * MELFAS MMS Touchscreen Driver - Platform Data
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 * Default path : linux/platform_data/mms_ts.h
 *
 */

#ifndef _LINUX_MMS_TOUCH_H
#define _LINUX_MMS_TOUCH_H

struct mms_platform_data {
	unsigned int	max_x;
	unsigned int	max_y;
//	unsigned long	irqflags;
//	int				gpio_int;
//	int				vdd_regulator_volt;
//	const char		*inkernel_fw_name;
	int gpio_int;	//required (interrupt signal)
	int max_finger_cnt;		//required (support max finger count)
	u32	device_num;

	const char *fw_name;
	const char *fw_config_name;
	const char *regulator_dvdd;
	const char *regulator_avdd;
};
#endif

