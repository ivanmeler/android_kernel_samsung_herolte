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

#include "fimc-is-device-ak7348.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-core.h"

#include "fimc-is-helper-i2c.h"

#define ACTUATOR_NAME		"AK7348"

#define DEF_AK7348_FIRST_POSITION		120
#define DEF_AK7348_FIRST_DELAY			10

extern struct fimc_is_sysfs_actuator sysfs_actuator;

static int sensor_ak7348_write_position(struct i2c_client *client, u32 val)
{
	int ret = 0;
	u8 val_high = 0, val_low = 0;

	BUG_ON(!client);

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (val > AK7348_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					val, AK7348_POS_MAX_SIZE);
		ret = -EINVAL;
		goto p_err;
	}

	val_high = (val & 0x01FF) >> 1;
	val_low = (val & 0x0001) << 7;

	ret = fimc_is_sensor_addr8_write8(client, AK7348_REG_POS_HIGH, val_high);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_addr8_write8(client, AK7348_REG_POS_LOW, val_low);
	if (ret < 0)
		goto p_err;

	return ret;

p_err:
	ret = -ENODEV;
	return ret;
}

static int sensor_ak7348_valid_check(struct i2c_client * client)
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

static void sensor_ak7348_print_log(int step)
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

static int sensor_ak7348_init_position(struct i2c_client *client,
		struct fimc_is_actuator *actuator)
{
	int i;
	int ret = 0;
	int init_step = 0;

	init_step = sensor_ak7348_valid_check(client);

	if (init_step > 0) {
		for (i = 0; i < init_step; i++) {
			ret = sensor_ak7348_write_position(client, sysfs_actuator.init_positions[i]);
			if (ret < 0)
				goto p_err;

			mdelay(sysfs_actuator.init_delays[i]);
		}

		actuator->position = sysfs_actuator.init_positions[i];

		sensor_ak7348_print_log(init_step);

	} else {
		ret = sensor_ak7348_write_position(client, DEF_AK7348_FIRST_POSITION);
		if (ret < 0)
			goto p_err;

		mdelay(DEF_AK7348_FIRST_DELAY);

		actuator->position = DEF_AK7348_FIRST_POSITION;

		dbg_sensor("initial position %d setting\n", DEF_AK7348_FIRST_POSITION);
	}


p_err:
	return ret;
}

int sensor_ak7348_actuator_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	u8 product_id = 0;
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

	ret = fimc_is_sensor_addr8_read8(client, 0x03, &product_id);
	if (ret < 0)
		goto p_err;

#if 0
	if (product_id != AK7348_PRODUCT_ID) {
		err("AK7348 is not detected(%d), Slave: %d\n", product_id, client->addr);
		goto p_err;
	}
#endif

	/* ToDo: Cal init data from FROM */

	ret = sensor_ak7348_init_position(client, actuator);
	if (ret <0)
		goto p_err;

	/* Go active mode */
	ret = fimc_is_sensor_addr8_write8(client, 0x02, 0);
	if (ret <0)
		goto p_err;

	/* ToDo */
	/* Wait Settling(>20ms) */
	/* SysSleep(30/MS_PER_TICK, NULL); */

#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_ak7348_actuator_get_status(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	struct fimc_is_actuator *actuator = NULL;
	struct i2c_client *client = NULL;
	enum fimc_is_actuator_status status = ACTUATOR_STATUS_NO_BUSY;
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
	 * The info is busy flag.
	 * But, this module can't get busy flag.
	 */
	status = ACTUATOR_STATUS_NO_BUSY;
	*info = status;

#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_ak7348_actuator_set_position(struct v4l2_subdev *subdev, u32 *info)
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
	if (position > AK7348_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					position, AK7348_POS_MAX_SIZE);
		ret = -EINVAL;
		goto p_err;
	}

	/* position Set */
	ret = sensor_ak7348_write_position(client, position);
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

static int sensor_ak7348_actuator_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	u32 val = 0;

	switch(ctrl->id) {
	case V4L2_CID_ACTUATOR_GET_STATUS:
		ret = sensor_ak7348_actuator_get_status(subdev, &val);
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

static int sensor_ak7348_actuator_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;

	switch(ctrl->id) {
	case V4L2_CID_ACTUATOR_SET_POSITION:
		ret = sensor_ak7348_actuator_set_position(subdev, &ctrl->value);
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
	.init = sensor_ak7348_actuator_init,
	.g_ctrl = sensor_ak7348_actuator_g_ctrl,
	.s_ctrl = sensor_ak7348_actuator_s_ctrl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

int sensor_ak7348_actuator_probe(struct i2c_client *client,
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
		err("sensor device is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	sensor_peri = find_peri_by_act_id(device, ACTUATOR_NAME_AK7348);
	if (!sensor_peri) {
		probe_err("sensor peri is net yet probed");
		return -EPROBE_DEFER;
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
	actuator->id = ACTUATOR_NAME_AK7348;
	actuator->subdev = subdev_actuator;
	actuator->device = sensor_id;
	actuator->client = client;
	actuator->position = 0;
	actuator->max_position = AK7348_POS_MAX_SIZE;
	actuator->pos_size_bit = AK7348_POS_SIZE_BIT;
	actuator->pos_direction = AK7348_POS_DIRECTION;

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

static int sensor_ak7348_actuator_remove(struct i2c_client *client)
{
	int ret = 0;

	return ret;
}

static const struct of_device_id exynos_fimc_is_ak7348_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-actuator-ak7348",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_ak7348_match);

static const struct i2c_device_id actuator_ak7348_idt[] = {
	{ ACTUATOR_NAME, 0 },
};

static struct i2c_driver actuator_ak7348_driver = {
	.driver = {
		.name	= ACTUATOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = exynos_fimc_is_ak7348_match
	},
	.probe	= sensor_ak7348_actuator_probe,
	.remove	= sensor_ak7348_actuator_remove,
	.id_table = actuator_ak7348_idt
};
module_i2c_driver(actuator_ak7348_driver);
