/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef fimc_is_device_sensor_peri_H
#define fimc_is_device_sensor_peri_H

#include <linux/interrupt.h>
#include "fimc-is-mem.h"
#include "fimc-is-param.h"
#include "fimc-is-interface-sensor.h"
#include "fimc-is-control-sensor.h"

#define HRTIMER_IMPOSSIBLE		0
#define HRTIMER_POSSIBLE		1
#define VIRTUAL_COORDINATE_WIDTH		32768
#define VIRTUAL_COORDINATE_HEIGHT		32768

struct fimc_is_cis {
	u32				id;
	struct v4l2_subdev		*subdev; /* connected module subdevice */
	u32				device; /* connected sensor device */
	struct i2c_client		*client;

	cis_shared_data			*cis_data;
	struct fimc_is_cis_ops		*cis_ops;
	enum otf_input_order		bayer_order;
	enum apex_aperture_value	aperture_num;
	bool				use_dgain;
	bool				hdr_ctrl_by_again;

	struct fimc_is_sensor_ctl	sensor_ctls[CAM2P0_UCTL_LIST_SIZE];

	/* store current ctrl */
	camera2_lens_uctl_t		cur_lens_uctrl;
	camera2_sensor_uctl_t		cur_sensor_uctrl;
	camera2_flash_uctl_t		cur_flash_uctrl;

	/* settings for mode change */
	bool				need_mode_change;
	u32				mode_chg_expo;
	u32				mode_chg_again;
	u32				mode_chg_dgain;

	/* expected dms */
	camera2_lens_dm_t		expecting_lens_dm[EXPECT_DM_NUM];
	camera2_sensor_dm_t		expecting_sensor_dm[EXPECT_DM_NUM];
	camera2_flash_dm_t		expecting_flash_dm[EXPECT_DM_NUM];

	/* expected udm */
	camera2_lens_udm_t		expecting_lens_udm[EXPECT_DM_NUM];

	/* For sensor status dump */
	struct work_struct		cis_status_dump_work;
};

struct fimc_is_actuator_data {
	struct timer_list			timer_wait;
	u32							check_time_out;

	bool						actuator_init;

	/* M2M AF */
	struct hrtimer              afwindow_timer;
	struct work_struct			actuator_work;
	u32							timer_check;
};

struct fimc_is_actuator {
	u32					id;
	struct v4l2_subdev			*subdev; /* connected module subdevice */
	u32					device; /* connected sensor device */
	struct i2c_client			*client;

	u32					position;
	u32					max_position;

	/* for M2M AF */
	struct timeval				start_time;
	struct timeval				end_time;
	u32					valid_flag;
	u32					valid_time;

	/* WinAf value for M2M AF */
	u32					left_x;
	u32					left_y;
	u32					right_x;
	u32					right_y;

	int					actuator_index;

	u32					pre_position[EXPECT_DM_NUM];
	u32					pre_frame_cnt[EXPECT_DM_NUM];

	enum fimc_is_actuator_pos_size_bit	pos_size_bit;
	enum fimc_is_actuator_direction		pos_direction;

	struct fimc_is_actuator_data		actuator_data;
};

struct fimc_is_flash_data {
	enum flash_mode			mode;
	u32				intensity;
	u32				firing_time_us;
	bool				flash_fired;
	struct work_struct		flash_fire_work;
	struct timer_list		flash_expire_timer;
	struct work_struct		flash_expire_work;
};

struct fimc_is_flash {
	u32				id;
	struct v4l2_subdev		*subdev; /* connected module subdevice */
	u32				device; /* connected sensor device */
	struct i2c_client		*client;

	int				flash_gpio;
	int				torch_gpio;

	struct fimc_is_flash_data	flash_data;
	struct fimc_is_flash_expo_gain  flash_ae;
};

struct fimc_is_device_sensor_peri {
	struct fimc_is_module_enum			*module;

	struct fimc_is_cis				cis;
	struct v4l2_subdev				*subdev_cis;

	struct fimc_is_actuator				actuator;
	struct v4l2_subdev				*subdev_actuator;

	struct fimc_is_flash				flash;
	struct v4l2_subdev				*subdev_flash;

	unsigned long					peri_state;

	/* Thread for M2M sensor setting */
	u32				work_index;
	spinlock_t			work_lock;
	struct task_struct		*task;
	struct kthread_worker		worker;
	struct kthread_work		work;

	struct fimc_is_sensor_interface			sensor_interface;
};

void fimc_is_sensor_work_fn(struct kthread_work *work);
int fimc_is_sensor_init_m2m_thread(struct fimc_is_device_sensor_peri *sensor_peri);
void fimc_is_sensor_deinit_m2m_thread(struct fimc_is_device_sensor_peri *sensor_peri);

struct fimc_is_device_sensor_peri *find_peri_by_cis_id(struct fimc_is_device_sensor *device,
							u32 cis);
struct fimc_is_device_sensor_peri *find_peri_by_act_id(struct fimc_is_device_sensor *device,
							u32 actuator);
struct fimc_is_device_sensor_peri *find_peri_by_flash_id(struct fimc_is_device_sensor *device,
							u32 flash);

void fimc_is_sensor_set_cis_uctrl_list(struct fimc_is_device_sensor_peri *sensor_peri,
		u32 long_exp, u32 short_exp,
		u32 long_total_gain, u32 short_total_gain,
		u32 long_analog_gain, u32 short_analog_gain,
		u32 long_digital_gain, u32 short_digital_gain);

int fimc_is_sensor_peri_notify_vsync(struct v4l2_subdev *subdev, void *arg);
int fimc_is_sensor_peri_notify_vblank(struct v4l2_subdev *subdev, void *arg);
int fimc_is_sensor_peri_notify_flash_fire(struct v4l2_subdev *subdev, void *arg);
int fimc_is_sensor_peri_pre_flash_fire(struct v4l2_subdev *subdev, void *arg);
int fimc_is_sensor_peri_notify_actuator(struct v4l2_subdev *subdev, void *arg);
int fimc_is_sensor_peri_notify_m2m_actuator(void *arg);
int fimc_is_sensor_peri_notify_actuator_init(struct v4l2_subdev *subdev);

int fimc_is_sensor_peri_call_m2m_actuator(struct fimc_is_device_sensor *device);

enum hrtimer_restart fimc_is_actuator_m2m_af_set(struct hrtimer *timer);

int fimc_is_actuator_notify_m2m_actuator(struct v4l2_subdev *subdev);

void fimc_is_sensor_peri_probe(struct fimc_is_device_sensor_peri *sensor_peri);
int fimc_is_sensor_peri_s_stream(struct fimc_is_device_sensor *device,
					bool on);

int fimc_is_sensor_peri_s_frame_duration(struct fimc_is_device_sensor *device,
				u32 frame_duration);
int fimc_is_sensor_peri_s_exposure_time(struct fimc_is_device_sensor *device,
				u32 long_exposure_time, u32 short_exposure_time);
int fimc_is_sensor_peri_s_analog_gain(struct fimc_is_device_sensor *device,
				u32 long_analog_gain, u32 short_analog_gain);
int fimc_is_sensor_peri_s_digital_gain(struct fimc_is_device_sensor *device,
				u32 long_digital_gain, u32 short_digital_gain);
int fimc_is_sensor_peri_adj_ctrl(struct fimc_is_device_sensor *device,
				u32 input, struct v4l2_control *ctrl);

int fimc_is_sensor_peri_compensate_gain_for_ext_br(struct fimc_is_device_sensor *device,
				u32 expo, u32 *again, u32 *dgain);

int fimc_is_sensor_peri_actuator_softlanding(struct fimc_is_device_sensor_peri *device);

int fimc_is_sensor_peri_debug_fixed(struct fimc_is_device_sensor *device);

void fimc_is_sensor_flash_fire_work(struct work_struct *data);
void fimc_is_sensor_flash_expire_handler(unsigned long data);
void fimc_is_sensor_flash_expire_work(struct work_struct *data);
int fimc_is_sensor_flash_fire(struct fimc_is_device_sensor_peri *device,
				u32 on);

void fimc_is_sensor_actuator_soft_move(struct work_struct *data);

#define CALL_CISOPS(s, op, args...) (((s)->cis_ops->op) ? ((s)->cis_ops->op(args)) : 0)

#endif
