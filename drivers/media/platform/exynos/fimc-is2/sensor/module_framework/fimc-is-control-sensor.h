/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CONTROL_SENSOR_H
#define FIMC_IS_CONTROL_SENSOR_H

#include <linux/workqueue.h>

#define CAM2P0_UCTL_LIST_SIZE   10
#define EXPECT_DM_NUM		10

struct gain_setting {
	u32 sensitivity;
	u32 long_again;
	u32 short_again;
	u32 long_dgain;
	u32 short_dgain;
};

/* Helper function */
u64 fimc_is_sensor_convert_us_to_ns(u32 usec);
u32 fimc_is_sensor_convert_ns_to_us(u64 nsec);

struct fimc_is_device_sensor;
void fimc_is_sensor_ctl_frame_evt(struct fimc_is_device_sensor *device);

/* Actuator funtion */
int fimc_is_actuator_ctl_set_position(struct fimc_is_device_sensor *device, u32 position);
int fimc_is_actuator_ctl_convert_position(u32 *pos,
				u32 src_max_pos, u32 src_direction,
				u32 tgt_max_pos, u32 tgt_direction);
int fimc_is_actuator_ctl_search_position(u32 position,
				u32 *position_table,
				u32 direction,
				u32 *searched_pos);
#endif
