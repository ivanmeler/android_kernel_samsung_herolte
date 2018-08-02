/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "fimc-is-device-dw9714.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-core.h"
#include "fimc-is-device-actuator-i2c.h"

#define ACTUATOR_NAME		"DW9714"

#define DEF_DW9714_FIRST_POSITION		15
#define DEF_DW9714_FIRST_DELAY			10

extern struct fimc_is_sysfs_actuator sysfs_actuator;

static int sensor_dw9714_write_position(struct i2c_client *client, u32 val)
{
	int ret = 0;
	u8 val_high = 0, val_low = 0;

	BUG_ON(!client);

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (val > DW9714_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					val, DW9714_POS_MAX_SIZE);
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * If when use uSlewRate in driver show the example code
	 * val_low = ((val & 0x0F) << 4) | uSlewRate
	 */
	val_high = (val & 0x3F0) >> 4;
	val_low = ((val & 0x0F) << 4) | 0xF;

	ret = fimc_is_sensor_data_write16(client, val_high, val_low);
	if (ret < 0)
		goto p_err;

	return ret;

p_err:
	ret = -ENODEV;
	return ret;
}

static int sensor_dw9714_valid_check(struct i2c_client * client)
{
	int i;

	BUG_ON(!client);

	if (sysfs_actuator.init_step > 0) {
		for (i = 0; i < sysfs_actuator.init_step; i++) {
			if (sysfs_actuator.init_positions[i] < 0) {
				warn("invalid position value, default setting to position");
				return 0;
			} else if (sysfs_actuator.init_delays[i] < 0) {
				warn("invalid delay value, default setting to delay");
				return 0;
			}
		}
	} else
		return 0;

	return sysfs_actuator.init_step;
}

static void sensor_dw9714_print_log(int step)
{
	int i;

	if (step > 0) {
		dbg_sensor("initial position ");
		for (i = 0; i < step; i++) {
			dbg_sensor(" %d", sysfs_actuator.init_positions[i]);
		}
		dbg_sensor(" setting");
	}
}


static int sensor_dw9714_init_position(struct i2c_client *client,
		struct fimc_is_actuator *actuator)
{
	int i;
	int ret = 0;
	int init_step = 0;

	init_step = sensor_dw9714_valid_check(client);

	if (init_step > 0) {
		for (i = 0; i < init_step; i++) {
			ret = sensor_dw9714_write_position(client, sysfs_actuator.init_positions[i]);
			if (ret < 0)
				goto p_err;

			mdelay(sysfs_actuator.init_delays[i]);
		}

		actuator->position = sysfs_actuator.init_positions[i];

		sensor_dw9714_print_log(init_step);

	} else {
		ret = sensor_dw9714_write_position(client, DEF_DW9714_FIRST_POSITION);
		if (ret < 0)
			goto p_err;

		mdelay(DEF_DW9714_FIRST_DELAY);

		actuator->position = DEF_DW9714_FIRST_POSITION;

		dbg_sensor("initial position %d setting\n", DEF_DW9714_FIRST_POSITION);
	}

p_err:
	return ret;
}

int sensor_dw9714_actuator_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_actuator *actuator;
	struct i2c_client *client = NULL;
#ifdef DEBUG_ACTUATOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);

	dbg_sensor("%s\n", __func__);

	actuator = (struct fimc_is_actuator *)v4l2_get_subdevdata(subdev);
	if (!actuator) {
		err("actuator is not detect!\n");
		goto p_err;
	}

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* ToDo */
	/* Wait Settling(>12ms) */
	/* SysSleep(12/MS_PER_TICK, NULL); */

	/* STEP 1 : Protect Off */
	ret = fimc_is_sensor_data_write16(client, 0xEC, 0xA3);
	if (ret < 0)
		goto p_err;

	/* SETP 2 : T_SRC setting (default is 0x00) */
	ret = fimc_is_sensor_data_write16(client, 0xF2, (0x9 << 3));
	if (ret <0)
		goto p_err;

	/* STEP 3 : Protection On */
	ret = fimc_is_sensor_data_write16(client, 0xDC, 0x51);
	if (ret <0)
		goto p_err;

	ret = sensor_dw9714_init_position(client, actuator);
	if (ret <0)
		goto p_err;

#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_dw9714_actuator_get_status(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	u16 val = 0;
	struct fimc_is_actuator *actuator = NULL;
	struct i2c_client *client = NULL;
#ifdef DEBUG_ACTUATOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	dbg_sensor("%s\n", __func__);

	BUG_ON(!subdev);
	BUG_ON(!info);

	actuator = (struct fimc_is_actuator *)v4l2_get_subdevdata(subdev);
	BUG_ON(!actuator);

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * Byte1                     Byte2
	 * PD FL AG D9 D8 D7 D6 D5   D4 D3 D2 D1 D0 S3 S2 S1 S0
	 * PD : Power down mode. 1: Power down mode (active high), 0: Normal operation mode
	 * FL : Busy flag. FLAG must keep "L" at writing operation.
	 */
	ret = fimc_is_sensor_data_read16(client, &val);

	*info = (((val) & 0x4000) == 0) ? ACTUATOR_STATUS_NO_BUSY : ACTUATOR_STATUS_BUSY;
#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_dw9714_actuator_set_position(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	struct fimc_is_actuator *actuator;
	struct i2c_client *client;
	u32 position = 0;
#ifdef DEBUG_ACTUATOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);
	BUG_ON(!info);

	dbg_sensor("%s\n", __func__);

	actuator = (struct fimc_is_actuator *)v4l2_get_subdevdata(subdev);
	BUG_ON(!actuator);

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	position = *info;
	if (position > DW9714_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					position, DW9714_POS_MAX_SIZE);
		ret = -EINVAL;
		goto p_err;
	}

	/* position Set */
	ret = sensor_dw9714_write_position(client, position);
	if (ret <0)
		goto p_err;
	actuator->position = position;

	dbg_sensor("Actuator position: %d\n", position);

#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif
p_err:
	return ret;
}

static int sensor_dw9714_actuator_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	u32 val = 0;

	switch(ctrl->id) {
	case V4L2_CID_ACTUATOR_GET_STATUS:
		ret = sensor_dw9714_actuator_get_status(subdev, &val);
		if (ret < 0) {
			err("err!!! ret(%d), actuator status(%d)", ret, val);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

	ctrl->value = val;

p_err:
	return ret;
}

static int sensor_dw9714_actuator_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;

	switch(ctrl->id) {
	case V4L2_CID_ACTUATOR_SET_POSITION:
		ret = sensor_dw9714_actuator_set_position(subdev, &ctrl->value);
		if (ret) {
			err("failed to actuator set position: %d, (%d)\n", ctrl->value, ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_dw9714_actuator_init,
	.g_ctrl = sensor_dw9714_actuator_g_ctrl,
	.s_ctrl = sensor_dw9714_actuator_s_ctrl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

int sensor_dw9714_actuator_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core= NULL;
	struct v4l2_subdev *subdev_actuator = NULL;
	struct fimc_is_actuator *actuator = NULL;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id = 0;
	struct device *dev;
	struct device_node *dnode;

	BUG_ON(!fimc_is_dev);
	BUG_ON(!client);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	probe_info("%s sensor_id %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];
	if (!device) {
		probe_err("sensor peri is net yet probed");
		return -EPROBE_DEFER;
	}

	sensor_peri = find_peri_by_act_id(device, ACTUATOR_NAME_DWXXXX);
	if (!sensor_peri) {
		err("sensor peri is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	actuator = &sensor_peri->actuator;
	if (!actuator) {
		err("acuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_actuator = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_actuator) {
		err("subdev_actuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	sensor_peri->subdev_actuator = subdev_actuator;

	/* This name must is match to sensor_open_extended actuator name */
	actuator->id = ACTUATOR_NAME_DWXXXX;
	actuator->subdev = subdev_actuator;
	actuator->device = sensor_id;
	actuator->client = client;
	actuator->position = 0;
	actuator->max_position = DW9714_POS_MAX_SIZE;
	actuator->pos_size_bit = DW9714_POS_SIZE_BIT;
	actuator->pos_direction = DW9714_POS_DIRECTION;

	v4l2_i2c_subdev_init(subdev_actuator, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_actuator, actuator);
	v4l2_set_subdev_hostdata(subdev_actuator, device);
	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_actuator);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state);

	snprintf(subdev_actuator->name, V4L2_SUBDEV_NAME_SIZE, "actuator-subdev.%d", actuator->id);
p_err:
	probe_info("%s done\n", __func__);
	return ret;
}

static int sensor_dw9714_actuator_remove(struct i2c_client *client)
{
	int ret = 0;

	return ret;
}

static const struct of_device_id exynos_fimc_is_dw9714_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-actuator-dw9714",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_dw9714_match);

static const struct i2c_device_id actuator_dw9714_idt[] = {
	{ ACTUATOR_NAME, 0 },
};

static struct i2c_driver actuator_dw9714_driver = {
	.driver = {
		.name	= ACTUATOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = exynos_fimc_is_dw9714_match
	},
	.probe	= sensor_dw9714_actuator_probe,
	.remove	= sensor_dw9714_actuator_remove,
	.id_table = actuator_dw9714_idt
};
module_i2c_driver(actuator_dw9714_driver);
