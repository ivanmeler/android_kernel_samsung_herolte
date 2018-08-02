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

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <exynos-fimc-is-sensor.h>

#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-module-base.h"
#include "interface/fimc-is-interface-library.h"

int sensor_module_load_bin(void)
{
	int ret = 0;

	ret = fimc_is_load_bin();
	if (ret < 0) {
		err("fimc_is_load_bin is fail(%d)", ret);
		goto p_err;
	}
	info("fimc_is_load_bin done\n");

	ret = fimc_is_load_vra_bin();
	if (ret < 0) {
		err("fimc_is_load_vra_bin is fail(%d)", ret);
		goto p_err;
	}
	info("fimc_is_load_vra_bin done\n");

p_err:
	return ret;
}

int sensor_module_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct exynos_platform_fimc_is_module *pdata = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct v4l2_subdev *subdev_actuator = NULL;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	BUG_ON(!module);

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	BUG_ON(!sensor_peri);

	memset(&sensor_peri->sensor_interface, 0, sizeof(struct fimc_is_sensor_interface));
	memset(&sensor_peri->actuator.pre_position, 0, sizeof(u32 [EXPECT_DM_NUM]));
	memset(&sensor_peri->actuator.pre_frame_cnt, 0, sizeof(u32 [EXPECT_DM_NUM]));

#if !defined(LIB_DISABLE)
	ret = sensor_module_load_bin();
	if (ret) {
		err("sensor_module_load_bin is fail");
		goto p_err;
	}

	ret = init_sensor_interface(&sensor_peri->sensor_interface);
	if (ret) {
		err("fimc_is_resource_get is fail");
		goto p_err;
	}

	ret = register_sensor_itf((void *)&sensor_peri->sensor_interface);
	if (ret < 0) {
		goto p_err;
	}
#endif

	pdata = module->pdata;
	BUG_ON(!pdata);

	subdev_cis = sensor_peri->subdev_cis;
	BUG_ON(!subdev_cis);

	ret = CALL_CISOPS(&sensor_peri->cis, cis_init, subdev_cis);
	if (ret) {
		err("v4l2_subdev_call(init) is fail(%d)", ret);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state) &&
			pdata->af_product_name != ACTUATOR_NAME_NOTHING) {

		sensor_peri->actuator.actuator_data.actuator_init = true;
		sensor_peri->actuator.actuator_index = -1;
		sensor_peri->actuator.left_x = 0;
		sensor_peri->actuator.left_y = 0;
		sensor_peri->actuator.right_x = 0;
		sensor_peri->actuator.right_y = 0;

		sensor_peri->actuator.actuator_data.afwindow_timer.function = fimc_is_actuator_m2m_af_set;

		subdev_actuator = sensor_peri->subdev_actuator;
		BUG_ON(!subdev_actuator);

		ret = v4l2_subdev_call(subdev_actuator, core, init, 0);
		if (ret) {
			err("v4l2_actuator_call(init) is fail(%d)", ret);
			goto p_err;
		}
	}

	pr_info("[MOD:D:%d] %s(%d)\n", module->sensor_id, __func__, val);

p_err:
	return ret;
}

int sensor_module_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	BUG_ON(!module);

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	fimc_is_sensor_deinit_m2m_thread(sensor_peri);

	sensor_peri->flash.flash_data.mode = CAM2_FLASH_MODE_OFF;
	if (sensor_peri->flash.flash_data.flash_fired == true) {
		ret = fimc_is_sensor_flash_fire(sensor_peri, 0);
		if (ret) {
			err("failed to turn off flash at flash expired handler\n");
		}
	}

	ret = fimc_is_sensor_peri_actuator_softlanding(sensor_peri);
	if (ret)
		err("failed to soft landing control of actuator driver\n");

	cancel_work_sync(&sensor_peri->flash.flash_data.flash_fire_work);
	cancel_work_sync(&sensor_peri->flash.flash_data.flash_expire_work);
	cancel_work_sync(&sensor_peri->cis.cis_status_dump_work);

	fimc_is_load_clear();
	info("fimc_is_load_clear\n");

	fimc_is_load_vra_clear();
	info("fimc_is_load_vra_clear\n");

	return ret;
}

long sensor_module_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;

	BUG_ON(!subdev);

	switch(cmd) {
	case V4L2_CID_SENSOR_DEINIT:
		ret = sensor_module_deinit(subdev);
		if (ret) {
			err("err!!! ret(%d), sensor module deinit fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_VSYNC:
		ret = fimc_is_sensor_peri_notify_vsync(subdev, arg);
		if (ret) {
			err("err!!! ret(%d), sensor notify vsync fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_VBLANK:
		ret = fimc_is_sensor_peri_notify_vblank(subdev, arg);
		if (ret) {
			err("err!!! ret(%d), sensor notify vblank fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_FLASH_FIRE:
		ret = fimc_is_sensor_peri_notify_flash_fire(subdev, arg);
		if (ret) {
			err("err!!! ret(%d), sensor notify flash fire fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_ACTUATOR:
		ret = fimc_is_sensor_peri_notify_actuator(subdev, arg);
		if (ret) {
			err("err!!! ret(%d), sensor notify actuator fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_M2M_ACTUATOR:
		ret = fimc_is_actuator_notify_m2m_actuator(subdev);
		if (ret) {
			err("err!!! ret(%d), sensor notify M2M actuator fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_ACTUATOR_INIT:
		ret = fimc_is_sensor_peri_notify_actuator_init(subdev);
		if (ret) {
			err("err!!! ret(%d), actuator init fail\n", ret);
			goto p_err;
		}
		break;

	default:
		err("err!!! Unknown CID(%#x)", cmd);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_module_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	cis_setting_info info;
	info.param = NULL;
	info.return_value = 0;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	BUG_ON(!module);

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	BUG_ON(!sensor_peri);

	switch(ctrl->id) {
	case V4L2_CID_SENSOR_ADJUST_FRAME_DURATION:
		/* TODO: v4l2 g_ctrl cannot support adjust function */
#if 0
		info.param = (void *)&ctrl->value;
		ret = CALL_CISOPS(&sensor_peri->cis, cis_adjust_frame_duration, sensor_peri->subdev_cis, &info);
		if (ret < 0 || info.return_value == 0) {
			err("err!!! ret(%d), frame duration(%d)", ret, info.return_value);
			ctrl->value = 0;
			ret = -EINVAL;
			goto p_err;
		}
		ctrl->value = info.return_value;
#endif
		break;
	case V4L2_CID_SENSOR_GET_MIN_EXPOSURE_TIME:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_min_exposure_time, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d), min exposure time(%d)", ret, info.return_value);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MAX_EXPOSURE_TIME:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_max_exposure_time, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ctrl->value = 0;
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_ADJUST_ANALOG_GAIN:
		/* TODO: v4l2 g_ctrl cannot support adjust function */
#if 0
		info.param = (void *)&ctrl->value;
		ret = CALL_CISOPS(&sensor_peri->cis, cis_adjust_analog_gain, sensor_peri->subdev_cis, &info);
		if (ret < 0 || info.return_value == 0) {
			err("err!!! ret(%d), adjust analog gain(%d)", ret, info.return_value);
			ctrl->value = 0;
			ret = -EINVAL;
			goto p_err;
		}
		ctrl->value = info.return_value;
#endif
		break;
	case V4L2_CID_SENSOR_GET_ANALOG_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_analog_gain, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MIN_ANALOG_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_min_analog_gain, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MAX_ANALOG_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_max_analog_gain, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_DIGITAL_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_digital_gain, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MIN_DIGITAL_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_min_digital_gain, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MAX_DIGITAL_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_max_digital_gain, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_ACTUATOR_GET_STATUS:
		ret = v4l2_subdev_call(sensor_peri->subdev_actuator, core, g_ctrl, ctrl);
		if (ret) {
			err("[SEN:%d] v4l2_subdev_call(g_ctrl, id:%d) is fail(%d)",
					module->sensor_id, ctrl->id, ret);
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

int sensor_module_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	BUG_ON(!module);

	device = (struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	BUG_ON(!device);

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	BUG_ON(!sensor_peri);

	switch(ctrl->id) {
	case V4L2_CID_SENSOR_SET_AE_TARGET:
		/* long_exposure_time and short_exposure_time is same value */
		ret = fimc_is_sensor_peri_s_exposure_time(device, ctrl->value, ctrl->value);
		if (ret < 0) {
			err("failed to set exposure time : %d\n - %d",
					ctrl->value, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_RATE:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_frame_rate, sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_DURATION:
		ret = fimc_is_sensor_peri_s_frame_duration(device, ctrl->value);
		if (ret < 0) {
			err("failed to set frame duration : %d\n - %d",
					ctrl->value, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_GAIN:
	case V4L2_CID_SENSOR_SET_ANALOG_GAIN:
	if (sensor_peri->cis.cis_data->analog_gain[1] != ctrl->value) {
		/* long_analog_gain and short_analog_gain is same value */
		ret = fimc_is_sensor_peri_s_analog_gain(device, ctrl->value, ctrl->value);
		if (ret < 0) {
			err("failed to set analog gain : %d\n - %d",
					ctrl->value, ret);
			goto p_err;
		}
		break;
	}
	case V4L2_CID_SENSOR_SET_DIGITAL_GAIN:
		/* long_digital_gain and short_digital_gain is same value */
		ret = fimc_is_sensor_peri_s_digital_gain(device, ctrl->value, ctrl->value);
		if (ret < 0) {
			err("failed to set digital gain : %d\n - %d",
					ctrl->value, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_ACTUATOR_SET_POSITION:
		ret = v4l2_subdev_call(sensor_peri->subdev_actuator, core, s_ctrl, ctrl);
		if (ret) {
			err("[SEN:%d] v4l2_subdev_call(s_ctrl, id:%d) is fail(%d)",
					module->sensor_id, ctrl->id, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_FLASH_SET_INTENSITY:
	case V4L2_CID_FLASH_SET_FIRING_TIME:
		ret = v4l2_subdev_call(sensor_peri->subdev_flash, core, s_ctrl, ctrl);
		if (ret) {
			err("[SEN:%d] v4l2_subdev_call(s_ctrl, id:%d) is fail(%d)",
					module->sensor_id, ctrl->id, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_FLASH_SET_MODE:
		if (ctrl->value < CAM2_FLASH_MODE_OFF || ctrl->value > CAM2_FLASH_MODE_BEST) {
			err("failed to flash set mode: %d, \n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		if (sensor_peri->flash.flash_data.mode != ctrl->value) {
			ret = fimc_is_sensor_flash_fire(sensor_peri, 0);
			if (ret) {
				err("failed to flash fire: %d\n", ctrl->value);
				ret = -EINVAL;
				goto p_err;
			}
			sensor_peri->flash.flash_data.mode = ctrl->value;
		}
		break;
	case V4L2_CID_FLASH_SET_FIRE:
		ret = fimc_is_sensor_flash_fire(sensor_peri, ctrl->value);
		if (ret) {
			err("failed to flash fire: %d\n", ctrl->value);
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

int sensor_module_g_ext_ctrls(struct v4l2_subdev *subdev, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);

	/* TODO */
	pr_info("[MOD:D:%d] %s Not implemented\n", module->sensor_id, __func__);

	return ret;
}

int sensor_module_s_ext_ctrls(struct v4l2_subdev *subdev, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);

	/* TODO */
	pr_info("[MOD:D:%d] %s Not implemented\n", module->sensor_id, __func__);

	return ret;
}

int sensor_module_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct fimc_is_device_sensor *device = NULL;

	BUG_ON(!subdev);

	device = (struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	BUG_ON(!device);

	ret = fimc_is_sensor_peri_s_stream(device, enable);
	if (ret)
		err("[SEN] fimc_is_sensor_peri_s_stream is fail(%d)", ret);

	return ret;
}

int sensor_module_s_format(struct v4l2_subdev *subdev, struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct v4l2_subdev *sd = NULL;
	struct fimc_is_cis *cis = NULL;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	BUG_ON(!module);

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	BUG_ON(!sensor_peri);

	sd = sensor_peri->subdev_cis;
	if (!sd) {
		err("[SEN:%d] no subdev_cis(s_mbus_fmt)", module->sensor_id);
		ret = -ENXIO;
		goto p_err;
	}
	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(sd);
	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	device = (struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	BUG_ON(!device);

	if (cis->cis_data->sens_config_index_cur != device->cfg->mode) {
		dbg_sensor("[%s] mode changed(%d->%d)\n", __func__,
				cis->cis_data->sens_config_index_cur, device->cfg->mode);

		cis->cis_data->sens_config_index_cur = device->cfg->mode;
		cis->cis_data->cur_width = fmt->width;
		cis->cis_data->cur_height = fmt->height;
		ret = CALL_CISOPS(cis, cis_mode_change, sd, device->cfg->mode);
		if (ret) {
			err("[SEN:%d] CALL_CISOPS(cis_mode_change) is fail(%d)",
					module->sensor_id, ret);
			goto p_err;
		}
	}

	/* If need a cis_set_size this use */
#if 0
	/* ToDo: binning value get from user */
	cis->cis_data->binning = true;

	ret = CALL_CISOPS(cis, cis_set_size, sd, cis->cis_data);
	if (ret) {
		err("[SEN:%d] CALL_CISOPS(cis_set_size) is fail(%d)",
				module->sensor_id, ret);
		goto p_err;
	}
#endif

	dbg_sensor("[%s] set format done, size(%dx%d), code(%#x)\n", __func__,
			fmt->width, fmt->height, fmt->code);

p_err:
	return ret;
}

int sensor_module_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	if (unlikely(!subdev)) {
		goto p_err;
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		goto p_err;
	}

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	if (unlikely(!sensor_peri)) {
		goto p_err;
	}

	schedule_work(&sensor_peri->cis.cis_status_dump_work);

p_err:
	return ret;
}
