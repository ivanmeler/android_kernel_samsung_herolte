/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "fimc-is-device-ischain.h"
#include "fimc-is-control-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-interface-sensor.h"

/* helper functions */
struct fimc_is_module_enum *get_subdev_module_enum(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct fimc_is_module_enum *module = NULL;

	if (unlikely(!itf)) {
		err("%s, interface in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	module = sensor_peri->module;
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

p_err:
	return module;
}

struct fimc_is_device_csi *get_subdev_csi(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_device_csi *csi = NULL;
	struct fimc_is_device_sensor *device;

	if (unlikely(!itf)) {
		err("%s, interface in is NULL", __func__);
		csi = NULL;
		goto p_err;
	}

	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	module = sensor_peri->module;
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

	subdev_module = module->subdev;
	if (!subdev_module) {
		err("module is not probed");
		subdev_module = NULL;
		goto p_err;
	}

	device = v4l2_get_subdev_hostdata(subdev_module);

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(device->subdev_csi);
	if (unlikely(!csi)) {
		err("%s, csi in is NULL", __func__);
		csi = NULL;
		goto p_err;
	}

p_err:
	return csi;
}

struct fimc_is_actuator *get_subdev_actuator(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct fimc_is_actuator *actuator = NULL;

	if (unlikely(!itf)) {
		err("%s, interface in is NULL", __func__);
		actuator = NULL;
		goto p_err;
	}

	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	if (unlikely(!sensor_peri->subdev_actuator)) {
		err("%s, subdev module in is NULL", __func__);
		actuator = NULL;
		goto p_err;
	}

	actuator = (struct fimc_is_actuator *)v4l2_get_subdevdata(sensor_peri->subdev_actuator);
	if (unlikely(!actuator)) {
		err("%s, module in is NULL", __func__);
		actuator = NULL;
		goto p_err;
	}

p_err:
	return actuator;
}

int sensor_get_ctrl(struct fimc_is_sensor_interface *itf,
			u32 ctrl_id, u32 *val)
{
	int ret = 0;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_module_enum *module = NULL;
	struct v4l2_subdev *subdev_module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct v4l2_control ctrl;

	if (unlikely(!itf)) {
		err("%s, interface in is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);
	BUG_ON(!val);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	module = sensor_peri->module;
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

	subdev_module = module->subdev;
	if (!subdev_module) {
		err("module is not probed");
		subdev_module = NULL;
		goto p_err;
	}

	device = v4l2_get_subdev_hostdata(subdev_module);
	if (unlikely(!device)) {
		err("%s, device in is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	ctrl.id = ctrl_id;
	ctrl.value = -1;
	ret = fimc_is_sensor_g_ctrl(device, &ctrl);
	*val = (u32)ctrl.value;
	if (ret < 0) {
		err("err!!! ret(%d), return_value(%d)", ret, *val);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

/* if target param has only one value(such as CIS_SMIA mode or flash_intensity),
   then set value to long_val, will be ignored short_val */
int set_interface_param(struct fimc_is_sensor_interface *itf,
			enum itf_cis_interface mode,
			enum itf_param_type target,
			u32 index,
			u32 long_val,
			u32 short_val)
{
	int ret = 0;
	u32 val[MAX_EXPOSURE_GAIN_PER_FRAME] = {0};

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	if (mode == ITF_CIS_SMIA) {
		val[EXPOSURE_GAIN_INDEX] = long_val;
		val[LONG_EXPOSURE_GAIN_INDEX] = 0;
		val[SHORT_EXPOSURE_GAIN_INDEX] = 0;
	} else if (mode == ITF_CIS_SMIA_WDR) {
		val[EXPOSURE_GAIN_INDEX] = 0;
		val[LONG_EXPOSURE_GAIN_INDEX] = long_val;;
		val[SHORT_EXPOSURE_GAIN_INDEX] = short_val;
	} else {
		pr_err("[%s] invalid mode (%d)\n", __func__, mode);
		ret = -EINVAL;
		goto p_err;
	}

	if (index >= NUM_FRAMES) {
		pr_err("[%s] invalid frame index (%d)\n", __func__, index);
		ret = -EINVAL;
		goto p_err;
	}

	switch (target) {
	case ITF_CIS_PARAM_TOTAL_GAIN:
		itf->total_gain[EXPOSURE_GAIN_INDEX][index] = val[EXPOSURE_GAIN_INDEX];
		itf->total_gain[LONG_EXPOSURE_GAIN_INDEX][index] = val[LONG_EXPOSURE_GAIN_INDEX];
		itf->total_gain[SHORT_EXPOSURE_GAIN_INDEX][index] = val[SHORT_EXPOSURE_GAIN_INDEX];
		dbg_sen_itf("%s: total gain[%d] %d %d %d\n", __func__, index, val[EXPOSURE_GAIN_INDEX], val[LONG_EXPOSURE_GAIN_INDEX], val[SHORT_EXPOSURE_GAIN_INDEX]);
		dbg_sen_itf("%s: total gain [0]:%d [1]:%d [2]:%d\n", __func__,
				itf->total_gain[LONG_EXPOSURE_GAIN_INDEX][0],
				itf->total_gain[LONG_EXPOSURE_GAIN_INDEX][1],
				itf->total_gain[LONG_EXPOSURE_GAIN_INDEX][2]);
		break;
	case ITF_CIS_PARAM_ANALOG_GAIN:
		itf->analog_gain[EXPOSURE_GAIN_INDEX][index] = val[EXPOSURE_GAIN_INDEX];
		itf->analog_gain[LONG_EXPOSURE_GAIN_INDEX][index] = val[LONG_EXPOSURE_GAIN_INDEX];
		itf->analog_gain[SHORT_EXPOSURE_GAIN_INDEX][index] = val[SHORT_EXPOSURE_GAIN_INDEX];
		dbg_sen_itf("%s: again[%d] %d %d %d\n", __func__, index, val[EXPOSURE_GAIN_INDEX], val[LONG_EXPOSURE_GAIN_INDEX], val[SHORT_EXPOSURE_GAIN_INDEX]);
		dbg_sen_itf("%s: again [0]:%d [1]:%d [2]:%d\n", __func__,
				itf->analog_gain[LONG_EXPOSURE_GAIN_INDEX][0],
				itf->analog_gain[LONG_EXPOSURE_GAIN_INDEX][1],
				itf->analog_gain[LONG_EXPOSURE_GAIN_INDEX][2]);
		break;
	case ITF_CIS_PARAM_DIGITAL_GAIN:
		itf->digital_gain[EXPOSURE_GAIN_INDEX][index] = val[EXPOSURE_GAIN_INDEX];
		itf->digital_gain[LONG_EXPOSURE_GAIN_INDEX][index] = val[LONG_EXPOSURE_GAIN_INDEX];
		itf->digital_gain[SHORT_EXPOSURE_GAIN_INDEX][index] = val[SHORT_EXPOSURE_GAIN_INDEX];
		dbg_sen_itf("%s: dgain[%d] %d %d %d\n", __func__, index, val[EXPOSURE_GAIN_INDEX], val[LONG_EXPOSURE_GAIN_INDEX], val[SHORT_EXPOSURE_GAIN_INDEX]);
		dbg_sen_itf("%s: dgain [0]:%d [1]:%d [2]:%d\n", __func__,
				itf->digital_gain[LONG_EXPOSURE_GAIN_INDEX][0],
				itf->digital_gain[LONG_EXPOSURE_GAIN_INDEX][1],
				itf->digital_gain[LONG_EXPOSURE_GAIN_INDEX][2]);
		break;
	case ITF_CIS_PARAM_EXPOSURE:
		itf->exposure[EXPOSURE_GAIN_INDEX][index] = val[EXPOSURE_GAIN_INDEX];
		itf->exposure[LONG_EXPOSURE_GAIN_INDEX][index] = val[LONG_EXPOSURE_GAIN_INDEX];
		itf->exposure[SHORT_EXPOSURE_GAIN_INDEX][index] = val[SHORT_EXPOSURE_GAIN_INDEX];
		dbg_sen_itf("%s: expo[%d] %d %d %d\n", __func__, index, val[EXPOSURE_GAIN_INDEX], val[LONG_EXPOSURE_GAIN_INDEX], val[SHORT_EXPOSURE_GAIN_INDEX]);
		dbg_sen_itf("%s: expo [0]:%d [1]:%d [2]:%d\n", __func__,
				itf->exposure[LONG_EXPOSURE_GAIN_INDEX][0],
				itf->exposure[LONG_EXPOSURE_GAIN_INDEX][1],
				itf->exposure[LONG_EXPOSURE_GAIN_INDEX][2]);
		break;
	case ITF_CIS_PARAM_FLASH_INTENSITY:
		itf->flash_intensity[index] = long_val;
		break;
	default:
		pr_err("[%s] invalid CIS_SMIA mode (%d)\n", __func__, mode);
		ret = -EINVAL;
		goto p_err;
		break;
	}

p_err:
	return ret;
}

int get_interface_param(struct fimc_is_sensor_interface *itf,
			enum itf_cis_interface mode,
			enum itf_param_type target,
			u32 index,
			u32 *long_val,
			u32 *short_val)
{
	int ret = 0;
	u32 val[MAX_EXPOSURE_GAIN_PER_FRAME] = {0};

	BUG_ON(!itf);
	BUG_ON(!long_val);
	BUG_ON(!short_val);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	if (index >= NUM_FRAMES) {
		pr_err("[%s] invalid frame index (%d)\n", __func__, index);
		ret = -EINVAL;
		goto p_err;
	}

	switch (target) {
	case ITF_CIS_PARAM_TOTAL_GAIN:
		val[EXPOSURE_GAIN_INDEX] = itf->total_gain[EXPOSURE_GAIN_INDEX][index];
		val[LONG_EXPOSURE_GAIN_INDEX] = itf->total_gain[LONG_EXPOSURE_GAIN_INDEX][index];
		val[SHORT_EXPOSURE_GAIN_INDEX] = itf->total_gain[SHORT_EXPOSURE_GAIN_INDEX][index];
		dbg_sen_itf("%s: total gain[%d] %d %d %d\n", __func__, index, val[EXPOSURE_GAIN_INDEX], val[LONG_EXPOSURE_GAIN_INDEX], val[SHORT_EXPOSURE_GAIN_INDEX]);
		break;
	case ITF_CIS_PARAM_ANALOG_GAIN:
		val[EXPOSURE_GAIN_INDEX] = itf->analog_gain[EXPOSURE_GAIN_INDEX][index];
		val[LONG_EXPOSURE_GAIN_INDEX] = itf->analog_gain[LONG_EXPOSURE_GAIN_INDEX][index];
		val[SHORT_EXPOSURE_GAIN_INDEX] = itf->analog_gain[SHORT_EXPOSURE_GAIN_INDEX][index];
		dbg_sen_itf("%s: again[%d] %d %d %d\n", __func__, index, val[EXPOSURE_GAIN_INDEX], val[LONG_EXPOSURE_GAIN_INDEX], val[SHORT_EXPOSURE_GAIN_INDEX]);
		break;
	case ITF_CIS_PARAM_DIGITAL_GAIN:
		val[EXPOSURE_GAIN_INDEX] = itf->digital_gain[EXPOSURE_GAIN_INDEX][index];
		val[LONG_EXPOSURE_GAIN_INDEX] = itf->digital_gain[LONG_EXPOSURE_GAIN_INDEX][index];
		val[SHORT_EXPOSURE_GAIN_INDEX] = itf->digital_gain[SHORT_EXPOSURE_GAIN_INDEX][index];
		dbg_sen_itf("%s: dgain[%d] %d %d %d\n", __func__, index, val[EXPOSURE_GAIN_INDEX], val[LONG_EXPOSURE_GAIN_INDEX], val[SHORT_EXPOSURE_GAIN_INDEX]);
		break;
	case ITF_CIS_PARAM_EXPOSURE:
		val[EXPOSURE_GAIN_INDEX] = itf->exposure[EXPOSURE_GAIN_INDEX][index];
		val[LONG_EXPOSURE_GAIN_INDEX] = itf->exposure[LONG_EXPOSURE_GAIN_INDEX][index];
		val[SHORT_EXPOSURE_GAIN_INDEX] = itf->exposure[SHORT_EXPOSURE_GAIN_INDEX][index];
		dbg_sen_itf("%s: exposure[%d] %d %d %d\n", __func__, index, val[EXPOSURE_GAIN_INDEX], val[LONG_EXPOSURE_GAIN_INDEX], val[SHORT_EXPOSURE_GAIN_INDEX]);
		break;
	case ITF_CIS_PARAM_FLASH_INTENSITY:
		val[EXPOSURE_GAIN_INDEX] = itf->flash_intensity[index];
		val[LONG_EXPOSURE_GAIN_INDEX] = itf->flash_intensity[index];
		break;
	default:
		pr_err("[%s] invalid CIS_SMIA mode (%d)\n", __func__, mode);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	if (mode == ITF_CIS_SMIA) {
		*long_val = val[EXPOSURE_GAIN_INDEX];
	} else if (mode == ITF_CIS_SMIA_WDR) {
		*long_val = val[LONG_EXPOSURE_GAIN_INDEX];
		*short_val = val[SHORT_EXPOSURE_GAIN_INDEX];
	} else {
		pr_err("[%s] invalid mode (%d)\n", __func__, mode);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

u32 get_vsync_count(struct fimc_is_sensor_interface *itf);

u32 get_frame_count(struct fimc_is_sensor_interface *itf)
{
	u32 frame_count = 0;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_module_enum *module = NULL;
	struct v4l2_subdev *subdev_module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	if (itf->otf_flag_3aa == true) {
		frame_count = get_vsync_count(itf);
	} else {
		/* Get 3AA active count */
		sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
		BUG_ON(!sensor_peri);

		module = sensor_peri->module;
		if (unlikely(!module)) {
			err("%s, module in is NULL", __func__);
			module = NULL;
			return 0;
		}

		subdev_module = module->subdev;
		if (!subdev_module) {
			err("module is not probed");
			subdev_module = NULL;
			return 0;
		}

		device = v4l2_get_subdev_hostdata(subdev_module);
		if (unlikely(!device)) {
			err("%s, device in is NULL", __func__);
			return 0;
		}

		frame_count = device->ischain->group_3aa.fcount;
	}

	/* Frame count have to start at 1 */
	if (frame_count == 0)
		frame_count = 1;

	return frame_count;
}

struct fimc_is_sensor_ctl *get_sensor_ctl_from_module(struct fimc_is_sensor_interface *itf,
							u32 frame_count)
{
	u32 index = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	index = frame_count % CAM2P0_UCTL_LIST_SIZE;

	return &sensor_peri->cis.sensor_ctls[index];
}

camera2_sensor_uctl_t *get_sensor_uctl_from_module(struct fimc_is_sensor_interface *itf,
							u32 frame_count)
{
	struct fimc_is_sensor_ctl *sensor_ctl = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_ctl = get_sensor_ctl_from_module(itf, frame_count);

	/* TODO: will be moved to report_sensor_done() */
	sensor_ctl->sensor_frame_number = frame_count;

	return &sensor_ctl->cur_cam20_sensor_udctrl;
}

void set_sensor_uctl_valid(struct fimc_is_sensor_interface *itf,
						u32 frame_count)
{
	struct fimc_is_sensor_ctl *sensor_ctl = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_ctl = get_sensor_ctl_from_module(itf, frame_count);

	sensor_ctl->is_valid_sensor_udctrl = true;
}

int set_exposure(struct fimc_is_sensor_interface *itf,
		enum itf_cis_interface mode,
		u32 long_exp,
		u32 short_exp)
{
	int ret = 0;
	u32 frame_count = 0;
	camera2_sensor_uctl_t *sensor_uctl;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	frame_count = get_frame_count(itf);
	sensor_uctl = get_sensor_uctl_from_module(itf, frame_count);

	BUG_ON(!sensor_uctl);

	/* set exposure */
	sensor_uctl->exposureTime = fimc_is_sensor_convert_us_to_ns(long_exp);
	if (mode == ITF_CIS_SMIA_WDR) {
		sensor_uctl->longExposureTime = fimc_is_sensor_convert_us_to_ns(long_exp);
		sensor_uctl->shortExposureTime = fimc_is_sensor_convert_us_to_ns(short_exp);
	}
	set_sensor_uctl_valid(itf, frame_count);

	return ret;
}

int set_gain_permile(struct fimc_is_sensor_interface *itf,
		enum itf_cis_interface mode,
		u32 long_total_gain, u32 short_total_gain,
		u32 long_analog_gain, u32 short_analog_gain,
		u32 long_digital_gain, u32 short_digital_gain)
{
	int ret = 0;
	u32 frame_count = 0;
	camera2_sensor_uctl_t *sensor_uctl;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	frame_count = get_frame_count(itf);
	sensor_uctl = get_sensor_uctl_from_module(itf, frame_count);

	BUG_ON(!sensor_uctl);

	/* set exposure */
	if (mode == ITF_CIS_SMIA) {
		sensor_uctl->sensitivity = long_total_gain;

		sensor_uctl->analogGain = long_analog_gain;
		sensor_uctl->digitalGain = long_digital_gain;

		set_sensor_uctl_valid(itf, frame_count);
	} else if (mode == ITF_CIS_SMIA_WDR) {
		sensor_uctl->sensitivity = long_total_gain;

		/* Caution: short values are setted at analog/digital gain */
		sensor_uctl->analogGain = short_analog_gain;
		sensor_uctl->digitalGain = short_digital_gain;
		sensor_uctl->longAnalogGain = long_analog_gain;
		sensor_uctl->shortAnalogGain = short_analog_gain;
		sensor_uctl->longDigitalGain = long_digital_gain;
		sensor_uctl->shortDigitalGain = short_digital_gain;

		set_sensor_uctl_valid(itf, frame_count);
	} else {
		pr_err("invalid cis interface mode (%d)\n", mode);
		ret = -EINVAL;
	}

	return ret;
}

/* new APIs */
int request_reset_interface(struct fimc_is_sensor_interface *itf,
				u32 exposure,
				u32 total_gain,
				u32 analog_gain,
				u32 digital_gain)
{
	int ret = 0;
	u32 i = 0;
	u32 end_index = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	dbg_sen_itf("[%s] exposure(%d), total_gain(%d), a-gain(%d), d-gain(%d)\n", __func__,
			exposure, total_gain, analog_gain, digital_gain);

	itf->vsync_flag = false;
	end_index = itf->otf_flag_3aa == true ? NEXT_NEXT_FRAME_OTF : NEXT_NEXT_FRAME_DMA;

	for (i = 0; i <= end_index; i++) {
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_TOTAL_GAIN, i, total_gain, total_gain);
		if (ret < 0)
			pr_err("[%s] set_interface_param TOTAL_GAIN fail(%d)\n", __func__, ret);
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_ANALOG_GAIN, i, analog_gain, analog_gain);
		if (ret < 0)
			pr_err("[%s] set_interface_param ANALOG_GAIN fail(%d)\n", __func__, ret);
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_DIGITAL_GAIN, i, digital_gain, digital_gain);
		if (ret < 0)
			pr_err("[%s] set_interface_param DIGITAL_GAIN fail(%d)\n", __func__, ret);
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_EXPOSURE, i, exposure, exposure);
		if (ret < 0)
			pr_err("[%s] set_interface_param EXPOSURE fail(%d)\n", __func__, ret);
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_FLASH_INTENSITY, i, 0, 0);
		if (ret < 0)
			pr_err("[%s] set_interface_param FLASH_INTENSITY fail(%d)\n", __func__, ret);
	}

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	fimc_is_sensor_set_cis_uctrl_list(sensor_peri,
			exposure,
			exposure,
			total_gain, total_gain,
			analog_gain, analog_gain,
			digital_gain, digital_gain);

	memset(sensor_peri->cis.cis_data->auto_exposure, 0, sizeof(sensor_peri->cis.cis_data->auto_exposure));

	return ret;
}

int get_calibrated_size(struct fimc_is_sensor_interface *itf,
			u32 *width,
			u32 *height)
{
	int ret = 0;
	struct fimc_is_module_enum *module = NULL;

	BUG_ON(!itf);
	BUG_ON(!width);
	BUG_ON(!height);

	module = get_subdev_module_enum(itf);
	BUG_ON(!module);

	*width = module->pixel_width;
	*height = module->pixel_height;

	pr_debug("%s, width(%d), height(%d)\n", __func__, *width, *height);

	return ret;
}

int get_bayer_order(struct fimc_is_sensor_interface *itf,
			u32 *bayer_order)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!bayer_order);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	*bayer_order = sensor_peri->cis.bayer_order;

	return ret;
}

u32 get_min_exposure_time(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;
	u32 exposure = 0;

	BUG_ON(!itf);

	ret = sensor_get_ctrl(itf, V4L2_CID_SENSOR_GET_MIN_EXPOSURE_TIME, &exposure);
	if (ret < 0 || exposure == 0) {
		err("err!!! ret(%d), return_value(%d)", ret, exposure);
		goto p_err;
	}

	dbg_sen_itf("%s:(%d:%d) min exp(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), exposure);

p_err:
	return exposure;
}

u32 get_max_exposure_time(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;
	u32 exposure = 0;

	BUG_ON(!itf);

	ret = sensor_get_ctrl(itf, V4L2_CID_SENSOR_GET_MAX_EXPOSURE_TIME, &exposure);
	if (ret < 0 || exposure == 0) {
		err("err!!! ret(%d), return_value(%d)", ret, exposure);
		goto p_err;
	}

	dbg_sen_itf("%s:(%d:%d) max exp(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), exposure);

p_err:
	return exposure;
}

u32 get_min_analog_gain(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;
	u32 again = 0;

	BUG_ON(!itf);

	ret = sensor_get_ctrl(itf, V4L2_CID_SENSOR_GET_MIN_ANALOG_GAIN, &again);
	if (ret < 0 || again == 0) {
		err("err!!! ret(%d), return_value(%d)", ret, again);
		goto p_err;
	}

	dbg_sen_itf("%s:(%d:%d) min analog gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), again);

p_err:
	return again;
}

u32 get_max_analog_gain(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;
	u32 again = 0;

	BUG_ON(!itf);

	ret = sensor_get_ctrl(itf, V4L2_CID_SENSOR_GET_MAX_ANALOG_GAIN, &again);
	if (ret < 0 || again == 0) {
		err("err!!! ret(%d), return_value(%d)", ret, again);
		goto p_err;
	}

	dbg_sen_itf("%s:(%d:%d) max analog gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), again);

p_err:
	return again;
}

u32 get_min_digital_gain(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;
	u32 dgain = 0;

	BUG_ON(!itf);

	ret = sensor_get_ctrl(itf, V4L2_CID_SENSOR_GET_MIN_DIGITAL_GAIN, &dgain);
	if (ret < 0 || dgain == 0) {
		err("err!!! ret(%d), return_value(%d)", ret, dgain);
		goto p_err;
	}

	dbg_sen_itf("%s:(%d:%d) min digital gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), dgain);

p_err:
	return dgain;
}

u32 get_max_digital_gain(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;
	u32 dgain = 0;

	BUG_ON(!itf);

	ret = sensor_get_ctrl(itf, V4L2_CID_SENSOR_GET_MAX_DIGITAL_GAIN, &dgain);
	if (ret < 0 || dgain == 0) {
		err("err!!! ret(%d), return_value(%d)", ret, dgain);
		goto p_err;
	}

	dbg_sen_itf("%s:(%d:%d) max digital gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), dgain);

p_err:
	return dgain;
}


u32 get_vsync_count(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_csi *csi;

	BUG_ON(!itf);

	csi = get_subdev_csi(itf);
	BUG_ON(!csi);

	return atomic_read(&csi->fcount);
}

u32 get_vblank_count(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_csi *csi;

	BUG_ON(!itf);

	csi = get_subdev_csi(itf);
	BUG_ON(!csi);

	return atomic_read(&csi->vblank_count);
}

bool is_vvalid_period(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_csi *csi;

	BUG_ON(!itf);

	csi = get_subdev_csi(itf);
	BUG_ON(!csi);

	return atomic_read(&csi->vvalid) <= 0 ? false : true;
}

int request_exposure(struct fimc_is_sensor_interface *itf,
			u32 long_exposure,
			u32 short_exposure)
{
	int ret = 0;
	u32 i = 0;
	u32 end_index = 0;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	dbg_sen_itf("[%s](%d:%d) long_exposure(%d), short_exposure(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), long_exposure, short_exposure);

	end_index = (itf->otf_flag_3aa == true ? NEXT_NEXT_FRAME_OTF : NEXT_NEXT_FRAME_DMA);

	i = (itf->vsync_flag == false ? 0 : end_index);
	for (; i <= end_index; i++) {
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_EXPOSURE, i, long_exposure, short_exposure);
		if (ret < 0) {
			pr_err("[%s] set_interface_param EXPOSURE fail(%d)\n", __func__, ret);
			goto p_err;
		}
	}
	itf->vsync_flag = true;

	/* set exposure */
	if (itf->otf_flag_3aa == true) {
		ret = set_exposure(itf, itf->cis_mode, long_exposure, short_exposure);
		if (ret < 0) {
			pr_err("[%s] set_exposure fail(%d)\n", __func__, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

int adjust_exposure(struct fimc_is_sensor_interface *itf,
			u32 long_exposure,
			u32 short_exposure,
			u32 *available_long_exposure,
			u32 *available_short_exposure,
			fimc_is_sensor_adjust_direction adjust_direction)
{
	/* NOT IMPLEMENTED YET */
	int ret = -1;

	dbg_sen_itf("[%s] NOT IMPLEMENTED YET\n", __func__);

	return ret;
}

int get_next_frame_timing(struct fimc_is_sensor_interface *itf,
			u32 *long_exposure,
			u32 *short_exposure,
			u32 *frame_period,
			u64 *line_period)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!long_exposure);
	BUG_ON(!short_exposure);
	BUG_ON(!frame_period);
	BUG_ON(!line_period);

	ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_EXPOSURE, NEXT_FRAME, long_exposure, short_exposure);
	if (ret < 0)
		pr_err("[%s] get_interface_param EXPOSURE fail(%d)\n", __func__, ret);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	*frame_period = sensor_peri->cis.cis_data->frame_time;
	*line_period = sensor_peri->cis.cis_data->line_readOut_time;

	dbg_sen_itf("%s:(%d:%d) exp(%d, %d), frame_period %d, line_period %lld\n", __func__, get_vsync_count(itf), get_frame_count(itf), *long_exposure, *short_exposure, *frame_period, *line_period);

	return ret;
}

int get_frame_timing(struct fimc_is_sensor_interface *itf,
			u32 *long_exposure,
			u32 *short_exposure,
			u32 *frame_period,
			u64 *line_period)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!long_exposure);
	BUG_ON(!short_exposure);
	BUG_ON(!frame_period);
	BUG_ON(!line_period);

	ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_EXPOSURE, CURRENT_FRAME, long_exposure, short_exposure);
	if (ret < 0)
		pr_err("[%s] get_interface_param EXPOSURE fail(%d)\n", __func__, ret);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	*frame_period = sensor_peri->cis.cis_data->frame_time;
	*line_period = sensor_peri->cis.cis_data->line_readOut_time;

	dbg_sen_itf("%s:(%d:%d) exp(%d, %d), frame_period %d, line_period %lld\n", __func__, get_vsync_count(itf), get_frame_count(itf), *long_exposure, *short_exposure, *frame_period, *line_period);

	return ret;
}

int request_analog_gain(struct fimc_is_sensor_interface *itf,
			u32 long_analog_gain,
			u32 short_analog_gain)
{
	int ret = 0;
	u32 i = 0;
	u32 end_index = 0;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	dbg_sen_itf("[%s](%d:%d) long_analog_gain(%d), short_analog_gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), long_analog_gain, short_analog_gain);

	end_index = (itf->otf_flag_3aa == true ? NEXT_NEXT_FRAME_OTF : NEXT_NEXT_FRAME_DMA);

	i = (itf->vsync_flag == false ? 0 : end_index);
	for (; i <= end_index; i++) {
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_ANALOG_GAIN, i, long_analog_gain, short_analog_gain);
		if (ret < 0) {
			pr_err("[%s] set_interface_param EXPOSURE fail(%d)\n", __func__, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

int request_gain(struct fimc_is_sensor_interface *itf,
		u32 long_total_gain,
		u32 long_analog_gain,
		u32 long_digital_gain,
		u32 short_total_gain,
		u32 short_analog_gain,
		u32 short_digital_gain)
{
	int ret = 0;
	u32 i = 0;
	u32 end_index = 0;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	dbg_sen_itf("[%s](%d:%d) long_total_gain(%d), short_total_gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), long_total_gain, short_total_gain);
	dbg_sen_itf("[%s](%d:%d) long_analog_gain(%d), short_analog_gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), long_analog_gain, short_analog_gain);
	dbg_sen_itf("[%s](%d:%d) long_digital_gain(%d), short_digital_gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), long_digital_gain, short_digital_gain);

	end_index = (itf->otf_flag_3aa == true ? NEXT_NEXT_FRAME_OTF : NEXT_NEXT_FRAME_DMA);

	i = (itf->vsync_flag == false ? 0 : end_index);
	for (; i <= end_index; i++) {
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_TOTAL_GAIN, i, long_total_gain, short_total_gain);
		if (ret < 0) {
			pr_err("[%s] set_interface_param EXPOSURE fail(%d)\n", __func__, ret);
			goto p_err;
		}
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_ANALOG_GAIN, i, long_analog_gain, short_analog_gain);
		if (ret < 0) {
			pr_err("[%s] set_interface_param EXPOSURE fail(%d)\n", __func__, ret);
			goto p_err;
		}
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_DIGITAL_GAIN, i, long_digital_gain, short_digital_gain);
		if (ret < 0) {
			pr_err("[%s] set_interface_param EXPOSURE fail(%d)\n", __func__, ret);
			goto p_err;
		}
	}

	/* set gain permile */
	if (itf->otf_flag_3aa == true) {
		ret = set_gain_permile(itf, itf->cis_mode,
				long_total_gain, short_total_gain,
				long_analog_gain, short_analog_gain,
				long_digital_gain, short_digital_gain);
		if (ret < 0) {
			pr_err("[%s] set_gain_permile fail(%d)\n", __func__, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

int adjust_analog_gain(struct fimc_is_sensor_interface *itf,
			u32 desired_long_analog_gain,
			u32 desired_short_analog_gain,
			u32 *actual_long_gain,
			u32 *actual_short_gain,
			fimc_is_sensor_adjust_direction adjust_direction)
{
	/* NOT IMPLEMENTED YET */
	int ret = -1;

	dbg_sen_itf("[%s] NOT IMPLEMENTED YET\n", __func__);

	return ret;
}

int get_next_analog_gain(struct fimc_is_sensor_interface *itf,
			u32 *long_analog_gain,
			u32 *short_analog_gain)
{
	int ret = 0;

	BUG_ON(!itf);
	BUG_ON(!long_analog_gain);
	BUG_ON(!short_analog_gain);

	ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_ANALOG_GAIN, NEXT_FRAME, long_analog_gain, short_analog_gain);
	if (ret < 0)
		pr_err("[%s] get_interface_param ANALOG_GAIN fail(%d)\n", __func__, ret);

	dbg_sen_itf("[%s](%d:%d) long_analog_gain(%d), short_analog_gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), *long_analog_gain, *short_analog_gain);

	return ret;
}

int get_analog_gain(struct fimc_is_sensor_interface *itf,
			u32 *long_analog_gain,
			u32 *short_analog_gain)
{
	int ret = 0;

	BUG_ON(!itf);
	BUG_ON(!long_analog_gain);
	BUG_ON(!short_analog_gain);

	ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_ANALOG_GAIN, CURRENT_FRAME, long_analog_gain, short_analog_gain);
	if (ret < 0)
		pr_err("[%s] get_interface_param ANALOG_GAIN fail(%d)\n", __func__, ret);

	dbg_sen_itf("[%s](%d:%d) long_analog_gain(%d), short_analog_gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), *long_analog_gain, *short_analog_gain);

	return ret;
}

int get_next_digital_gain(struct fimc_is_sensor_interface *itf,
				u32 *long_digital_gain,
				u32 *short_digital_gain)
{
	int ret = 0;

	BUG_ON(!itf);
	BUG_ON(!long_digital_gain);
	BUG_ON(!short_digital_gain);

	ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_DIGITAL_GAIN, NEXT_FRAME, long_digital_gain, short_digital_gain);
	if (ret < 0)
		pr_err("[%s] get_interface_param DIGITAL_GAIN fail(%d)\n", __func__, ret);

	dbg_sen_itf("[%s](%d:%d) long_digital_gain(%d), short_digital_gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), *long_digital_gain, *short_digital_gain);

	return ret;
}

int get_digital_gain(struct fimc_is_sensor_interface *itf,
			u32 *long_digital_gain,
			u32 *short_digital_gain)
{
	int ret = 0;

	BUG_ON(!itf);
	BUG_ON(!long_digital_gain);
	BUG_ON(!short_digital_gain);

	ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_DIGITAL_GAIN, CURRENT_FRAME, long_digital_gain, short_digital_gain);
	if (ret < 0)
		pr_err("[%s] get_interface_param DIGITAL_GAIN fail(%d)\n", __func__, ret);

	dbg_sen_itf("[%s](%d:%d) long_digital_gain(%d), short_digital_gain(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), *long_digital_gain, *short_digital_gain);

	return ret;
}

bool is_actuator_available(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	return test_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state);
}

bool is_flash_available(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	return test_bit(FIMC_IS_SENSOR_FLASH_AVAILABLE, &sensor_peri->peri_state);
}

bool is_companion_available(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	return test_bit(FIMC_IS_SENSOR_COMPANION_AVAILABLE, &sensor_peri->peri_state);
}

bool is_ois_available(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	return test_bit(FIMC_IS_SENSOR_OIS_AVAILABLE, &sensor_peri->peri_state);
}

int get_sensor_frame_timing(struct fimc_is_sensor_interface *itf,
			u32 *pclk,
			u32 *line_length_pck,
			u32 *frame_length_lines)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri;

	BUG_ON(!itf);
	BUG_ON(!pclk);
	BUG_ON(!line_length_pck);
	BUG_ON(!frame_length_lines);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	*pclk = sensor_peri->cis.cis_data->pclk;
	*line_length_pck = sensor_peri->cis.cis_data->line_length_pck;
	*frame_length_lines = sensor_peri->cis.cis_data->frame_length_lines;

	dbg_sen_itf("[%s](%d:%d) pclk(%d), line_length_pck(%d), frame_length_lines(%d)\n", __func__, get_vsync_count(itf), get_frame_count(itf), *pclk, *line_length_pck, *frame_length_lines);

	return ret;
}

int get_sensor_cur_size(struct fimc_is_sensor_interface *itf,
			u32 *cur_width,
			u32 *cur_height)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!cur_width);
	BUG_ON(!cur_height);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	*cur_width = sensor_peri->cis.cis_data->cur_width;
	*cur_height = sensor_peri->cis.cis_data->cur_height;

	return ret;
}

int get_sensor_max_fps(struct fimc_is_sensor_interface *itf,
			u32 *max_fps)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!max_fps);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	*max_fps = sensor_peri->cis.cis_data->max_fps;

	return ret;
}

int get_sensor_cur_fps(struct fimc_is_sensor_interface *itf,
			u32 *cur_fps)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!cur_fps);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	if (sensor_peri->cis.cis_data->cur_frame_us_time != 0) {
		*cur_fps = (u32)((1 * 1000 * 1000) / sensor_peri->cis.cis_data->cur_frame_us_time);
	} else {
		pr_err("[%s] cur_frame_us_time is ZERO\n", __func__);
		ret = -1;
	}

	return ret;
}

int get_hdr_ratio_ctl_by_again(struct fimc_is_sensor_interface *itf,
			u32 *ctrl_by_again)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!ctrl_by_again);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	*ctrl_by_again = sensor_peri->cis.hdr_ctrl_by_again;

	return ret;
}

int get_sensor_use_dgain(struct fimc_is_sensor_interface *itf,
			u32 *use_dgain)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!use_dgain);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	*use_dgain = sensor_peri->cis.use_dgain;

	return ret;
}

int set_alg_reset_flag(struct fimc_is_sensor_interface *itf,
			bool executed)
{
	int ret = 0;
	struct fimc_is_sensor_ctl *sensor_ctl = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_ctl = get_sensor_ctl_from_module(itf, get_frame_count(itf));

	if (sensor_ctl == NULL) {
		err("[%s]: get_sensor_ctl_from_module fail!!\n", __func__);
		return -1;
	}

	sensor_ctl->alg_reset_flag = executed;

	return ret;
}

int get_sensor_fnum(struct fimc_is_sensor_interface *itf,
			u32 *fnum)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!fnum);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	*fnum = sensor_peri->cis.aperture_num;

	return ret;
}

int set_initial_exposure_of_setfile(struct fimc_is_sensor_interface *itf,
				u32 expo)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	dbg_sen_itf("[%s] init expo (%d)\n", __func__, expo);

	sensor_peri->cis.cis_data->low_expo_start = expo;

	return ret;
}

int set_video_mode_of_setfile(struct fimc_is_sensor_interface *itf,
				bool video_mode)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	dbg_sen_itf("[%s] video mode (%d)\n", __func__, video_mode);

	sensor_peri->cis.cis_data->video_mode = video_mode;

	return ret;
}

int get_num_of_frame_per_one_3aa(struct fimc_is_sensor_interface *itf,
				u32 *num_of_frame)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 max_fps = 0;

	BUG_ON(!itf);
	BUG_ON(!num_of_frame);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	max_fps = sensor_peri->cis.cis_data->max_fps;

	/* TODO: SDK should know how many frames are needed to execute one 3a. */
	*num_of_frame = 1;

	/* This is FW HACK */
	if (sensor_peri->cis.id == SENSOR_NAME_S5K2T2
		|| sensor_peri->cis.id == SENSOR_NAME_IMX240
		|| sensor_peri->cis.id == SENSOR_NAME_S5K2P2
		/* || sensor_peri->cis.id == SENSOR_NAME_IMX228 */) {
		if (sensor_peri->cis.cis_data->video_mode == true) {
			if (max_fps >= 300) {
				*num_of_frame = 10;
			} else if (max_fps >= 240) {
				*num_of_frame = 24;
			} else if (max_fps >= 120) {
				*num_of_frame = 4;
			} else if (max_fps >= 60) {
				*num_of_frame = 2;
			}
		}
	}

	return 0;
}

int get_offset_from_cur_result(struct fimc_is_sensor_interface *itf,
				u32 *offset)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 max_fps = 0;

	BUG_ON(!itf);
	BUG_ON(!offset);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);
	BUG_ON(!sensor_peri->cis.cis_data);

	max_fps = sensor_peri->cis.cis_data->max_fps;

	/* TODO: SensorSdk delivers the result of the 3AA by taking into account this parameter. */
	*offset = 0;

	/* This is FW HACK */
	if (sensor_peri->cis.id == SENSOR_NAME_S5K2T2
		|| sensor_peri->cis.id == SENSOR_NAME_IMX240
		|| sensor_peri->cis.id == SENSOR_NAME_IMX260
		|| sensor_peri->cis.id == SENSOR_NAME_S5K2P2
		/* || sensor_peri->cis.id == SENSOR_NAME_IMX228 */) {
		if (sensor_peri->cis.cis_data->video_mode == true) {
			if (max_fps >= 300) {
				*offset = 1;
			} else if (max_fps >= 240) {
				*offset = 1;
			} else if (max_fps >= 120) {
				*offset = 1;
			} else if (max_fps >= 60) {
				*offset = 0;
			}
		}
	}

	return 0;
}

int set_cur_uctl_list(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	if (sensor_peri->cis.cis_data->companion_data.enable_wdr == true) {
		fimc_is_sensor_set_cis_uctrl_list(sensor_peri,
			sensor_peri->cis.cur_sensor_uctrl.longExposureTime,
			sensor_peri->cis.cur_sensor_uctrl.shortExposureTime,
			sensor_peri->cis.cur_sensor_uctrl.sensitivity,
			0,
			sensor_peri->cis.cur_sensor_uctrl.longAnalogGain,
			sensor_peri->cis.cur_sensor_uctrl.shortAnalogGain,
			sensor_peri->cis.cur_sensor_uctrl.longDigitalGain,
			sensor_peri->cis.cur_sensor_uctrl.shortDigitalGain);
	} else {
		fimc_is_sensor_set_cis_uctrl_list(sensor_peri,
			sensor_peri->cis.cur_sensor_uctrl.exposureTime,
			0,
			sensor_peri->cis.cur_sensor_uctrl.sensitivity,
			0,
			sensor_peri->cis.cur_sensor_uctrl.analogGain,
			0,
			sensor_peri->cis.cur_sensor_uctrl.digitalGain,
			0);
	}

	return 0;
}

int apply_sensor_setting(struct fimc_is_sensor_interface *itf)
{
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_module_enum *module = NULL;
	struct v4l2_subdev *subdev_module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	module = sensor_peri->module;
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

	subdev_module = module->subdev;
	if (!subdev_module) {
		err("module is not probed");
		subdev_module = NULL;
		goto p_err;
	}

	device = v4l2_get_subdev_hostdata(subdev_module);
	BUG_ON(!device);

	/* sensor control */
	fimc_is_sensor_ctl_frame_evt(device);

p_err:
	return 0;
}

int request_reset_expo_gain(struct fimc_is_sensor_interface *itf,
				u32 long_expo,
				u32 long_tgain,
				u32 long_again,
				u32 long_dgain,
				u32 short_expo,
				u32 short_tgain,
				u32 short_again,
				u32 short_dgain)
{
	int ret = 0;
	u32 i = 0;
	u32 end_index = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	dbg_sen_itf("[%s] long exposure(%d), total_gain(%d), a-gain(%d), d-gain(%d)\n", __func__,
			long_expo, long_tgain, long_again, long_dgain);
	dbg_sen_itf("[%s] short exposure(%d), total_gain(%d), a-gain(%d), d-gain(%d)\n", __func__,
			short_expo, short_tgain, short_again, short_dgain);

	end_index = itf->otf_flag_3aa == true ? NEXT_NEXT_FRAME_OTF : NEXT_NEXT_FRAME_DMA;

	for (i = 0; i <= end_index; i++) {
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_TOTAL_GAIN, i, long_tgain, short_tgain);
		if (ret < 0)
			pr_err("[%s] set_interface_param TOTAL_GAIN fail(%d)\n", __func__, ret);
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_ANALOG_GAIN, i, long_again, short_again);
		if (ret < 0)
			pr_err("[%s] set_interface_param ANALOG_GAIN fail(%d)\n", __func__, ret);
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_DIGITAL_GAIN, i, long_dgain, short_dgain);
		if (ret < 0)
			pr_err("[%s] set_interface_param DIGITAL_GAIN fail(%d)\n", __func__, ret);
		ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_EXPOSURE, i, long_expo, short_expo);
		if (ret < 0)
			pr_err("[%s] set_interface_param EXPOSURE fail(%d)\n", __func__, ret);
	}

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	fimc_is_sensor_set_cis_uctrl_list(sensor_peri,
			long_expo,
			short_expo,
			long_tgain, short_tgain,
			long_again, short_again,
			long_dgain, short_dgain);

	memset(sensor_peri->cis.cis_data->auto_exposure, 0, sizeof(sensor_peri->cis.cis_data->auto_exposure));

	return ret;
}

int set_sensor_info_mode_change(struct fimc_is_sensor_interface *itf,
		u32 expo,
		u32 again,
		u32 dgain)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);

	sensor_peri->cis.mode_chg_expo = expo;
	sensor_peri->cis.mode_chg_again = again;
	sensor_peri->cis.mode_chg_dgain = dgain;

	dbg_sen_itf("[%s] mode_chg_expo(%d), again(%d), dgain(%d)\n", __func__,
			sensor_peri->cis.mode_chg_expo,
			sensor_peri->cis.mode_chg_again,
			sensor_peri->cis.mode_chg_dgain);

	return ret;
}

int update_sensor_dynamic_meta(struct fimc_is_sensor_interface *itf,
		u32 frame_count,
		camera2_ctl_t *ctrl,
		camera2_dm_t *dm,
		camera2_udm_t *udm)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 index = 0;

	BUG_ON(!itf);
	BUG_ON(!ctrl);
	BUG_ON(!dm);
	BUG_ON(!udm);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	index = frame_count % EXPECT_DM_NUM;
	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);

	dm->sensor.exposureTime = sensor_peri->cis.expecting_sensor_dm[index].exposureTime;
	dm->sensor.frameDuration = sensor_peri->cis.cis_data->cur_frame_us_time;
	dm->sensor.sensitivity = sensor_peri->cis.expecting_sensor_dm[index].sensitivity;
	dm->sensor.rollingShutterSkew = sensor_peri->cis.cis_data->rolling_shutter_skew;

	dbg_sen_itf("[%s]: expo(%lld), duration(%lld), sensitivity(%d), rollingShutterSkew(%lld)\n",
			__func__, dm->sensor.exposureTime,
			dm->sensor.frameDuration,
			dm->sensor.sensitivity,
			dm->sensor.rollingShutterSkew);

	return ret;
}

int copy_sensor_ctl(struct fimc_is_sensor_interface *itf,
			u32 frame_count,
			camera2_shot_t *shot)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 index = 0;
	struct fimc_is_sensor_ctl *sensor_ctl;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	index = frame_count % EXPECT_DM_NUM;
	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	sensor_ctl = &sensor_peri->cis.sensor_ctls[index];

	if (shot == NULL) {
		sensor_ctl->ctl_frame_number = 0;
		sensor_ctl->valid_sensor_ctrl = false;
		sensor_ctl->is_sensor_request = false;
	} else {
		sensor_ctl->ctl_frame_number = shot->dm.request.frameCount;
		sensor_ctl->cur_cam20_sensor_ctrl = shot->ctl.sensor;
		sensor_ctl->valid_sensor_ctrl = true;
		sensor_ctl->is_sensor_request = true;
	}

	return ret;
}

int get_module_id(struct fimc_is_sensor_interface *itf, u32 *module_id)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct fimc_is_module_enum *module = NULL;

	BUG_ON(!itf);
	BUG_ON(!module_id);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	module = sensor_peri->module;
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

	*module_id = module->sensor_id;

p_err:
	return 0;

}

int get_module_position(struct fimc_is_sensor_interface *itf,
				enum exynos_sensor_position *module_position)
{
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct fimc_is_module_enum *module = NULL;

	BUG_ON(!itf);
	BUG_ON(!module_position);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	module = sensor_peri->module;
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

	*module_position = module->position;

p_err:
	return 0;

}

int set_sensor_3a_mode(struct fimc_is_sensor_interface *itf,
				u32 mode)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	if (mode > 1) {
		err("ERR[%s] invalid mode(%d)\n", __func__, mode);
		return -1;
	}

	/* 0: OTF, 1: M2M */
	itf->otf_flag_3aa = mode == 0 ? true : false;
	itf->diff_bet_sen_isp = itf->otf_flag_3aa? DIFF_OTF_DELAY : DIFF_M2M_DELAY;

	if (itf->otf_flag_3aa == false) {
		ret = fimc_is_sensor_init_m2m_thread(sensor_peri);
		if (ret) {
			err("fimc_is_sensor_init_m2m_thread is fail(%d)", ret);
			return ret;
		}
	}

	return 0;
}

int start_of_frame(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;
	u32 i = 0;
	u32 end_index = 0;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	dbg_sen_itf("%s: !!!!!!!!!!!!!!!!!!!!!!!\n", __func__);
	end_index = itf->otf_flag_3aa == true ? NEXT_NEXT_FRAME_OTF : NEXT_NEXT_FRAME_DMA;

	for (i = 0; i < end_index; i++) {
		if (itf->cis_mode == ITF_CIS_SMIA) {
			itf->total_gain[EXPOSURE_GAIN_INDEX][i] = itf->total_gain[EXPOSURE_GAIN_INDEX][i + 1];
			itf->analog_gain[EXPOSURE_GAIN_INDEX][i] = itf->analog_gain[EXPOSURE_GAIN_INDEX][i + 1];
			itf->digital_gain[EXPOSURE_GAIN_INDEX][i] = itf->digital_gain[EXPOSURE_GAIN_INDEX][i + 1];
			itf->exposure[EXPOSURE_GAIN_INDEX][i] = itf->exposure[EXPOSURE_GAIN_INDEX][i + 1];
		} else if (itf->cis_mode == ITF_CIS_SMIA_WDR){
			itf->total_gain[LONG_EXPOSURE_GAIN_INDEX][i] = itf->total_gain[LONG_EXPOSURE_GAIN_INDEX][i + 1];
			itf->total_gain[SHORT_EXPOSURE_GAIN_INDEX][i] = itf->total_gain[SHORT_EXPOSURE_GAIN_INDEX][i + 1];
			itf->analog_gain[LONG_EXPOSURE_GAIN_INDEX][i] = itf->analog_gain[LONG_EXPOSURE_GAIN_INDEX][i + 1];
			itf->analog_gain[SHORT_EXPOSURE_GAIN_INDEX][i] = itf->analog_gain[SHORT_EXPOSURE_GAIN_INDEX][i + 1];
			itf->digital_gain[LONG_EXPOSURE_GAIN_INDEX][i] = itf->digital_gain[LONG_EXPOSURE_GAIN_INDEX][i + 1];
			itf->digital_gain[SHORT_EXPOSURE_GAIN_INDEX][i] = itf->digital_gain[SHORT_EXPOSURE_GAIN_INDEX][i + 1];
			itf->exposure[LONG_EXPOSURE_GAIN_INDEX][i] = itf->exposure[LONG_EXPOSURE_GAIN_INDEX][i + 1];
			itf->exposure[SHORT_EXPOSURE_GAIN_INDEX][i] = itf->exposure[SHORT_EXPOSURE_GAIN_INDEX][i + 1];
		} else {
			pr_err("[%s] in valid cis_mode (%d)\n", __func__, itf->cis_mode);
			ret = -EINVAL;
			goto p_err;
		}

		itf->flash_intensity[i] = itf->flash_intensity[i + 1];
		itf->flash_mode[i] = itf->flash_mode[i + 1];
		itf->flash_firing_duration[i] = itf->flash_firing_duration[i + 1];
	}

	itf->flash_mode[i] = CAM2_FLASH_MODE_OFF;
	itf->flash_intensity[end_index] = 0;
	itf->flash_firing_duration[i] = 0;

	/* Flash setting */
	ret =  set_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_FLASH_INTENSITY, end_index, 0, 0);
	if (ret < 0)
		pr_err("[%s] set_interface_param FLASH_INTENSITY fail(%d)\n", __func__, ret);
	/* TODO */
	/*
	if (itf->flash_itf_ops) {
		(*itf->flash_itf_ops)->on_start_of_frame(itf->flash_itf_ops);
		(*itf->flash_itf_ops)->set_next_flash(itf->flash_itf_ops, itf->flash_intensity[NEXT_FRAME]);
	}
	*/

p_err:
	return ret;
}

int end_of_frame(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;
	u32 end_index = 0;
	u32 long_total_gain = 0;
	u32 short_total_gain = 0;
	u32 long_analog_gain = 0;
	u32 short_analog_gain = 0;
	u32 long_digital_gain = 0;
	u32 short_digital_gain = 0;
	u32 long_exposure = 0;
	u32 short_exposure = 0;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	dbg_sen_itf("%s: !!!!!!!!!!!!!!!!!!!!!!!\n", __func__);
	end_index = itf->otf_flag_3aa == true ? NEXT_NEXT_FRAME_OTF : NEXT_NEXT_FRAME_DMA;

	if (itf->vsync_flag == true) {
		/* TODO: sensor timing test */

		if (itf->otf_flag_3aa == false) {
			/* set gain */
			ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_TOTAL_GAIN,
						end_index, &long_total_gain, &short_total_gain);
			if (ret < 0)
				pr_err("[%s] get TOTAL_GAIN fail(%d)\n", __func__, ret);
			ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_ANALOG_GAIN,
						end_index, &long_analog_gain, &short_analog_gain);
			if (ret < 0)
				pr_err("[%s] get ANALOG_GAIN fail(%d)\n", __func__, ret);
			ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_DIGITAL_GAIN,
						end_index, &long_digital_gain, &short_digital_gain);
			if (ret < 0)
				pr_err("[%s] get DIGITAL_GAIN fail(%d)\n", __func__, ret);

			ret = set_gain_permile(itf, itf->cis_mode,
						long_total_gain, short_total_gain,
						long_analog_gain, short_analog_gain,
						long_digital_gain, short_digital_gain);
			if (ret < 0) {
				pr_err("[%s] set_gain_permile fail(%d)\n", __func__, ret);
				goto p_err;
			}

			/* set exposure */
			ret =  get_interface_param(itf, itf->cis_mode, ITF_CIS_PARAM_EXPOSURE,
						end_index, &long_exposure, &short_exposure);
			if (ret < 0)
				pr_err("[%s] get EXPOSURE fail(%d)\n", __func__, ret);

			ret = set_exposure(itf, itf->cis_mode, long_exposure, short_exposure);
			if (ret < 0) {
				pr_err("[%s] set_exposure fail(%d)\n", __func__, ret);
				goto p_err;
			}
		}
	}

	/* TODO */
	/*
	if (itf->flash_itf_ops) {
		(*itf->flash_itf_ops)->on_end_of_frame(itf->flash_itf_ops);
	}
	*/

p_err:
	return ret;
}

int apply_frame_settings(struct fimc_is_sensor_interface *itf)
{
	/* NOT IMPLEMENTED YET */
	int ret = -1;

	err("[%s] NOT IMPLEMENTED YET\n", __func__);

	return ret;
}

/* end of new APIs */

/* Flash interface */
int set_flash(struct fimc_is_sensor_interface *itf,
		u32 frame_count, u32 flash_mode, u32 intensity, u32 time)
{
	int ret = 0;
	struct fimc_is_sensor_ctl *sensor_ctl = NULL;
	camera2_flash_uctl_t *flash_uctl = NULL;
	enum flash_mode mode = CAM2_FLASH_MODE_OFF;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_ctl = get_sensor_ctl_from_module(itf, frame_count);
	flash_uctl = &sensor_ctl->cur_cam20_flash_udctrl;

	sensor_ctl->flash_frame_number = frame_count;

	if (intensity == 0) {
		mode = CAM2_FLASH_MODE_OFF;
	} else {
		switch (flash_mode) {
		case CAM2_FLASH_MODE_OFF:
		case CAM2_FLASH_MODE_SINGLE:
		case CAM2_FLASH_MODE_TORCH:
			mode = flash_mode;
			break;
		default:
			err("[%s] unknown scene_mode(%d)\n", __func__, flash_mode);
			break;
		}
	}

	flash_uctl->flashMode = mode;
	flash_uctl->firingPower = intensity;
	flash_uctl->firingTime = time;

	dbg_fls_itf("[%s] frame count %d,  mode %d, intensity %d, firing time %lld \n", __func__,
			frame_count,
			flash_uctl->flashMode,
			flash_uctl->firingPower,
			flash_uctl->firingTime);

	sensor_ctl->valid_flash_udctrl = true;

	return ret;
}

int request_flash(struct fimc_is_sensor_interface *itf,
				u32 mode,
				bool on,
				u32 intensity,
				u32 time)
{
	int ret = 0;
	u32 i = 0;
	u32 end_index = 0;
	u32 vsync_cnt = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	vsync_cnt = get_vsync_count(itf);

	dbg_fls_itf("[%s](%d) request_flash, mode(%d), on(%d), intensity(%d), time(%d)\n",
			__func__, vsync_cnt, mode, on, intensity, time);

	ret = get_num_of_frame_per_one_3aa(itf, &end_index);
	if (ret < 0) {
		pr_err("[%s] get_num_of_frame_per_one_3aa fail(%d)\n", __func__, ret);
		goto p_err;
	}

	for (i = 0; i < end_index; i++) {
		if (mode == CAM2_FLASH_MODE_TORCH && on == false) {
			dbg_fls_itf("[%s](%d) pre-flash off, mode(%d), on(%d), intensity(%d), time(%d)\n",
				__func__, vsync_cnt, mode, on, intensity, time);
			/* pre-flash off */
			sensor_peri->flash.flash_ae.pre_fls_ae_reset = true;
			sensor_peri->flash.flash_ae.frm_num_pre_fls = vsync_cnt + 1;
		} else if (mode == CAM2_FLASH_MODE_SINGLE && on == true) {
			dbg_fls_itf("[%s](%d) main on-off, mode(%d), on(%d), intensity(%d), time(%d)\n",
				__func__, vsync_cnt, mode, on, intensity, time);

			sensor_peri->flash.flash_data.mode = mode;
			sensor_peri->flash.flash_data.intensity = intensity;
			sensor_peri->flash.flash_data.firing_time_us = time;
			/* main-flash on off*/
			sensor_peri->flash.flash_ae.main_fls_ae_reset = true;
			sensor_peri->flash.flash_ae.frm_num_main_fls[0] = vsync_cnt + 1;
			sensor_peri->flash.flash_ae.frm_num_main_fls[1] = vsync_cnt + 2;
		} else {
			/* pre-flash on & flash off */
			ret = set_flash(itf, vsync_cnt + i, mode, intensity, time);
			if (ret < 0) {
				pr_err("[%s] set_flash fail(%d)\n", __func__, ret);
				goto p_err;
			}
		}
	}

p_err:
	return ret;
}

int request_flash_expo_gain(struct fimc_is_sensor_interface *itf,
			struct fimc_is_flash_expo_gain *flash_ae)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);
	BUG_ON(!sensor_peri);

	for (i = 0; i < 2; i++) {
		sensor_peri->flash.flash_ae.expo[i] = flash_ae->expo[i];
		sensor_peri->flash.flash_ae.tgain[i] = flash_ae->tgain[i];
		sensor_peri->flash.flash_ae.again[i] = flash_ae->again[i];
		sensor_peri->flash.flash_ae.dgain[i] = flash_ae->dgain[i];
		sensor_peri->flash.flash_ae.long_expo[i] = flash_ae->long_expo[i];
		sensor_peri->flash.flash_ae.long_tgain[i] = flash_ae->long_tgain[i];
		sensor_peri->flash.flash_ae.long_again[i] = flash_ae->long_again[i];
		sensor_peri->flash.flash_ae.long_dgain[i] = flash_ae->long_dgain[i];
		sensor_peri->flash.flash_ae.short_expo[i] = flash_ae->short_expo[i];
		sensor_peri->flash.flash_ae.short_tgain[i] = flash_ae->short_tgain[i];
		sensor_peri->flash.flash_ae.short_again[i] = flash_ae->short_again[i];
		sensor_peri->flash.flash_ae.short_dgain[i] = flash_ae->short_dgain[i];
		dbg_fls_itf("[%s] expo(%d, %d, %d), again(%d, %d, %d), dgain(%d, %d, %d)\n",
			__func__,
			sensor_peri->flash.flash_ae.expo[i],
			sensor_peri->flash.flash_ae.long_expo[i],
			sensor_peri->flash.flash_ae.short_expo[i],
			sensor_peri->flash.flash_ae.again[i],
			sensor_peri->flash.flash_ae.long_again[i],
			sensor_peri->flash.flash_ae.short_again[i],
			sensor_peri->flash.flash_ae.dgain[i],
			sensor_peri->flash.flash_ae.long_dgain[i],
			sensor_peri->flash.flash_ae.short_dgain[i]);
	}

	return ret;
}

int update_flash_dynamic_meta(struct fimc_is_sensor_interface *itf,
		u32 frame_count,
		camera2_ctl_t *ctrl,
		camera2_dm_t *dm,
		camera2_udm_t *udm)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!itf);
	BUG_ON(!ctrl);
	BUG_ON(!dm);
	BUG_ON(!udm);
	BUG_ON(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct fimc_is_device_sensor_peri, sensor_interface);

	dm->flash.flashMode = sensor_peri->flash.flash_data.mode;
	dm->flash.firingPower = sensor_peri->flash.flash_data.intensity;
	dm->flash.firingTime = sensor_peri->flash.flash_data.firing_time_us;
	if (sensor_peri->flash.flash_data.flash_fired)
		dm->flash.flashState = FLASH_STATE_FIRED;
	else
		dm->flash.flashState = FLASH_STATE_READY;

	dbg_sen_itf("[%s]: mode(%d), power(%d), time(%lld), state(%d)\n",
			__func__, dm->flash.flashMode,
			dm->flash.firingPower,
			dm->flash.firingTime,
			dm->flash.flashState);

	return ret;
}

int init_sensor_interface(struct fimc_is_sensor_interface *itf)
{
	int ret = 0;

	memset(itf, 0x0, sizeof(struct fimc_is_sensor_interface));

	itf->magic = SENSOR_INTERFACE_MAGIC;
	itf->vsync_flag = false;

	/* Default scenario is OTF */
	itf->otf_flag_3aa = true;
	/* TODO: check cis mode */
	itf->cis_mode = ITF_CIS_SMIA_WDR;
	/* OTF default is 3 frame delay */
	itf->diff_bet_sen_isp = itf->otf_flag_3aa ? DIFF_OTF_DELAY : DIFF_M2M_DELAY;

	/* struct fimc_is_cis_interface_ops */
	itf->cis_itf_ops.request_reset_interface = request_reset_interface;
	itf->cis_itf_ops.get_calibrated_size = get_calibrated_size;
	itf->cis_itf_ops.get_bayer_order = get_bayer_order;
	itf->cis_itf_ops.get_min_exposure_time = get_min_exposure_time;
	itf->cis_itf_ops.get_max_exposure_time = get_max_exposure_time;
	itf->cis_itf_ops.get_min_analog_gain = get_min_analog_gain;
	itf->cis_itf_ops.get_max_analog_gain = get_max_analog_gain;
	itf->cis_itf_ops.get_min_digital_gain = get_min_digital_gain;
	itf->cis_itf_ops.get_max_digital_gain = get_max_digital_gain;

	itf->cis_itf_ops.get_vsync_count = get_vsync_count;
	itf->cis_itf_ops.get_vblank_count = get_vblank_count;
	itf->cis_itf_ops.is_vvalid_period = is_vvalid_period;

	itf->cis_itf_ops.request_exposure = request_exposure;
	itf->cis_itf_ops.adjust_exposure = adjust_exposure;

	itf->cis_itf_ops.get_next_frame_timing = get_next_frame_timing;
	itf->cis_itf_ops.get_frame_timing = get_frame_timing;

	itf->cis_itf_ops.request_analog_gain = request_analog_gain;
	itf->cis_itf_ops.request_gain = request_gain;

	itf->cis_itf_ops.adjust_analog_gain = adjust_analog_gain;
	itf->cis_itf_ops.get_next_analog_gain = get_next_analog_gain;
	itf->cis_itf_ops.get_analog_gain = get_analog_gain;

	itf->cis_itf_ops.get_next_digital_gain = get_next_digital_gain;
	itf->cis_itf_ops.get_digital_gain = get_digital_gain;

	itf->cis_itf_ops.is_actuator_available = is_actuator_available;
	itf->cis_itf_ops.is_flash_available = is_flash_available;
	itf->cis_itf_ops.is_companion_available = is_companion_available;
	itf->cis_itf_ops.is_ois_available = is_ois_available;

	itf->cis_itf_ops.get_sensor_frame_timing = get_sensor_frame_timing;
	itf->cis_itf_ops.get_sensor_cur_size = get_sensor_cur_size;
	itf->cis_itf_ops.get_sensor_max_fps = get_sensor_max_fps;
	itf->cis_itf_ops.get_sensor_cur_fps = get_sensor_cur_fps;
	itf->cis_itf_ops.get_hdr_ratio_ctl_by_again = get_hdr_ratio_ctl_by_again;
	itf->cis_itf_ops.get_sensor_use_dgain = get_sensor_use_dgain;
	itf->cis_itf_ops.get_sensor_fnum = get_sensor_fnum;
	itf->cis_itf_ops.set_alg_reset_flag = set_alg_reset_flag;

	itf->cis_itf_ops.set_initial_exposure_of_setfile = set_initial_exposure_of_setfile;
	itf->cis_itf_ops.set_video_mode_of_setfile = set_video_mode_of_setfile;

	itf->cis_itf_ops.get_num_of_frame_per_one_3aa = get_num_of_frame_per_one_3aa;
	itf->cis_itf_ops.get_offset_from_cur_result = get_offset_from_cur_result;

	itf->cis_itf_ops.set_cur_uctl_list = set_cur_uctl_list;

	/* TODO: What is diff with apply_frame_settings at event_ops */
	itf->cis_itf_ops.apply_sensor_setting = apply_sensor_setting;

	/* reset exposure and gain for flash */
	itf->cis_itf_ops.request_reset_expo_gain = request_reset_expo_gain;

	itf->cis_itf_ops.set_sensor_info_mode_change = set_sensor_info_mode_change;
	itf->cis_itf_ops.update_sensor_dynamic_meta = update_sensor_dynamic_meta;
	itf->cis_itf_ops.copy_sensor_ctl = copy_sensor_ctl;
	itf->cis_itf_ops.get_module_id = get_module_id;
	itf->cis_itf_ops.get_module_position = get_module_position;
	itf->cis_itf_ops.set_sensor_3a_mode = set_sensor_3a_mode;

	/* struct fimc_is_cis_event_ops */
	itf->cis_evt_ops.start_of_frame = start_of_frame;
	itf->cis_evt_ops.end_of_frame = end_of_frame;
	itf->cis_evt_ops.apply_frame_settings = apply_frame_settings;

	/* Actuator interface */
	itf->actuator_itf.soft_landing_table.enable = false;
	itf->actuator_itf.position_table.enable = false;
	itf->actuator_itf.initialized = false;

	itf->actuator_itf_ops.set_actuator_position_table = set_actuator_position_table;
	itf->actuator_itf_ops.set_soft_landing_config = set_soft_landing_config;
	itf->actuator_itf_ops.set_position = set_position;
	itf->actuator_itf_ops.get_cur_frame_position = get_cur_frame_position;
	itf->actuator_itf_ops.get_applied_actual_position = get_applied_actual_position;
	itf->actuator_itf_ops.get_prev_frame_position = get_prev_frame_position;
	itf->actuator_itf_ops.set_af_window_position = set_af_window_position; /* AF window value for M2M AF */
	itf->actuator_itf_ops.get_status = get_status;

	/* Flash interface */
	itf->flash_itf_ops.request_flash = request_flash;
	itf->flash_itf_ops.request_flash_expo_gain = request_flash_expo_gain;
	itf->flash_itf_ops.update_flash_dynamic_meta = update_flash_dynamic_meta;

	return ret;
}

