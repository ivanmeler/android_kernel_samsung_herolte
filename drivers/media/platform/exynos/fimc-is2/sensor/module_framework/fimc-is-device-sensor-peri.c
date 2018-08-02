/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "interface/fimc-is-interface-library.h"
#include "interface/fimc-is-interface-ddk.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-video.h"

extern struct device *fimc_is_dev;

struct fimc_is_device_sensor_peri *find_peri_by_cis_id(struct fimc_is_device_sensor *device,
							u32 cis)
{
	u32 mindex = 0, mmax = 0;
	struct fimc_is_module_enum *module_enum = NULL;
	struct fimc_is_resourcemgr *resourcemgr = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!device);
	resourcemgr = device->resourcemgr;
	module_enum = device->module_enum;
	BUG_ON(!module_enum);

	if (unlikely(resourcemgr == NULL))
		return NULL;

	mmax = atomic_read(&resourcemgr->rsccount_module);
	for (mindex = 0; mindex < mmax; mindex++) {
		if (module_enum[mindex].ext.sensor_con.product_name == cis) {
			sensor_peri = (struct fimc_is_device_sensor_peri *)module_enum[mindex].private_data;
			break;
		}
	}

	if (mindex >= mmax) {
		merr("cis(%d) is not found", device, cis);
	}

	return sensor_peri;
}

struct fimc_is_device_sensor_peri *find_peri_by_act_id(struct fimc_is_device_sensor *device,
							u32 actuator)
{
	u32 mindex = 0, mmax = 0;
	struct fimc_is_module_enum *module_enum = NULL;
	struct fimc_is_resourcemgr *resourcemgr = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!device);
	resourcemgr = device->resourcemgr;
	module_enum = device->module_enum;
	BUG_ON(!module_enum);

	if (unlikely(resourcemgr == NULL))
		return NULL;

	mmax = atomic_read(&resourcemgr->rsccount_module);
	for (mindex = 0; mindex < mmax; mindex++) {
		if (module_enum[mindex].ext.actuator_con.product_name == actuator) {
			sensor_peri = (struct fimc_is_device_sensor_peri *)module_enum[mindex].private_data;
			break;
		}
	}

	if (mindex >= mmax) {
		merr("actuator(%d) is not found", device, actuator);
	}

	return sensor_peri;
}

struct fimc_is_device_sensor_peri *find_peri_by_flash_id(struct fimc_is_device_sensor *device,
							u32 flash)
{
	u32 mindex = 0, mmax = 0;
	struct fimc_is_module_enum *module_enum = NULL;
	struct fimc_is_resourcemgr *resourcemgr = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!device);
	resourcemgr = device->resourcemgr;
	module_enum = device->module_enum;
	BUG_ON(!module_enum);

	if (unlikely(resourcemgr == NULL))
		return NULL;

	mmax = atomic_read(&resourcemgr->rsccount_module);
	for (mindex = 0; mindex < mmax; mindex++) {
		if (module_enum[mindex].ext.flash_con.product_name == flash) {
			sensor_peri = (struct fimc_is_device_sensor_peri *)module_enum[mindex].private_data;
			break;
		}
	}

	if (mindex >= mmax) {
		merr("flash(%d) is not found", device, flash);
	}

	return sensor_peri;
}

int fimc_is_sensor_wait_streamoff(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;
	u32 timeout = 500;
	u32 time = 0;
	u32 sleep_time = 2;

	BUG_ON(!device);
	BUG_ON(!device->subdev_csi);

	csi = v4l2_get_subdevdata(device->subdev_csi);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	timeout = (1000 / csi->image.framerate) / sleep_time + 1;

	msleep(sleep_time);
	while (time <= timeout) {
		if (atomic_read(&csi->vvalid) == 0)
			break;

		time++;
		dbg_flash("waiting stream off (%d), cur fps(%d)\n", time, csi->image.framerate);
		msleep(sleep_time);
	}

	if (time > timeout) {
		err("timeout for wait stream off");
		ret = -1;
	}

p_err:
	return ret;
}

void fimc_is_sensor_cis_status_dump_work(struct work_struct *data)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct fimc_is_device_sensor_peri *sensor_peri;

	BUG_ON(!data);

	cis = container_of(data, struct fimc_is_cis, cis_status_dump_work);
	BUG_ON(!cis);

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);

	ret = CALL_CISOPS(cis, cis_log_status, sensor_peri->subdev_cis);
	if (ret < 0) {
		err("err!!! log_status ret(%d)", ret);
	}
}

void fimc_is_sensor_set_cis_uctrl_list(struct fimc_is_device_sensor_peri *sensor_peri,
		u32 long_exp, u32 short_exp,
		u32 long_total_gain, u32 short_total_gain,
		u32 long_analog_gain, u32 short_analog_gain,
		u32 long_digital_gain, u32 short_digital_gain)
{
	int i = 0;
	camera2_sensor_uctl_t *sensor_uctl;

	BUG_ON(!sensor_peri);

	for (i = 0; i < CAM2P0_UCTL_LIST_SIZE; i++) {
		sensor_uctl = &sensor_peri->cis.sensor_ctls[i].cur_cam20_sensor_udctrl;

		if (sensor_peri->cis.cis_data->companion_data.enable_wdr == true) {
			sensor_uctl->exposureTime = 0;
			sensor_uctl->longExposureTime = fimc_is_sensor_convert_us_to_ns(long_exp);
			sensor_uctl->shortExposureTime = fimc_is_sensor_convert_us_to_ns(short_exp);

			sensor_uctl->sensitivity = long_total_gain;
			sensor_uctl->analogGain = 0;
			sensor_uctl->digitalGain = 0;

			sensor_uctl->longAnalogGain = long_analog_gain;
			sensor_uctl->shortAnalogGain = short_analog_gain;
			sensor_uctl->longDigitalGain = long_digital_gain;
			sensor_uctl->shortDigitalGain = short_digital_gain;
		} else {
			sensor_uctl->exposureTime = fimc_is_sensor_convert_us_to_ns(long_exp);
			sensor_uctl->longExposureTime = 0;
			sensor_uctl->shortExposureTime = 0;

			sensor_uctl->sensitivity = long_total_gain;
			sensor_uctl->analogGain = long_analog_gain;
			sensor_uctl->digitalGain = long_digital_gain;

			sensor_uctl->longAnalogGain = 0;
			sensor_uctl->shortAnalogGain = 0;
			sensor_uctl->longDigitalGain = 0;
			sensor_uctl->shortDigitalGain = 0;
		}
	}
}

void fimc_is_sensor_m2m_work_fn(struct kthread_work *work)
{
	struct fimc_is_device_sensor_peri *sensor_peri;
	struct fimc_is_device_sensor *device;

	sensor_peri = container_of(work, struct fimc_is_device_sensor_peri, work);

	device = v4l2_get_subdev_hostdata(sensor_peri->subdev_cis);

	fimc_is_sensor_ctl_frame_evt(device);
}

int fimc_is_sensor_init_m2m_thread(struct fimc_is_device_sensor_peri *sensor_peri)
{
	int ret = 0;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};

	spin_lock_init(&sensor_peri->work_lock);
	init_kthread_worker(&sensor_peri->worker);
	sensor_peri->task = kthread_run(kthread_worker_fn, &sensor_peri->worker, "fimc_is_sen_m2m_work");
	ret = sched_setscheduler_nocheck(sensor_peri->task, SCHED_FIFO, &param);
	if (ret) {
		err("sched_setscheduler_nocheck is fail(%d)", ret);
		return ret;
	}

	init_kthread_work(&sensor_peri->work, fimc_is_sensor_m2m_work_fn);
	sensor_peri->work_index = 0;

	return ret;
}

void fimc_is_sensor_deinit_m2m_thread(struct fimc_is_device_sensor_peri *sensor_peri)
{
	if (sensor_peri->task != NULL) {
		if (kthread_stop(sensor_peri->task))
			err("kthread_stop fail");
	}
}

int fimc_is_sensor_initial_setting_low_exposure(struct fimc_is_device_sensor_peri *sensor_peri)
{
	int ret = 0;
	struct fimc_is_device_sensor *device;

	BUG_ON(!sensor_peri);

	device = v4l2_get_subdev_hostdata(sensor_peri->subdev_cis);
	BUG_ON(!device);

	dbg_sensor("[%s] expo(%d), again(%d), dgain(%d)\n", __func__,
			sensor_peri->cis.cis_data->low_expo_start, 100, 100);

	fimc_is_sensor_peri_s_analog_gain(device, 1000, 1000);
	fimc_is_sensor_peri_s_digital_gain(device, 1000, 1000);
	fimc_is_sensor_peri_s_exposure_time(device,
			sensor_peri->cis.cis_data->low_expo_start,
			sensor_peri->cis.cis_data->low_expo_start);

	sensor_peri->sensor_interface.cis_itf_ops.request_reset_expo_gain(&sensor_peri->sensor_interface,
			sensor_peri->cis.cis_data->low_expo_start,
			1000,
			1000,
			1000,
			sensor_peri->cis.cis_data->low_expo_start,
			1000,
			1000,
			1000);

	fimc_is_sensor_set_cis_uctrl_list(sensor_peri,
			sensor_peri->cis.cis_data->low_expo_start,
			sensor_peri->cis.cis_data->low_expo_start,
			1000, 1000,
			1000, 1000,
			1000, 1000);

	return ret;
}

int fimc_is_sensor_setting_mode_change(struct fimc_is_device_sensor_peri *sensor_peri)
{
	int ret = 0;
	struct fimc_is_device_sensor *device;
	u32 expo = 0;
	u32 tgain = 0;
	u32 again = 0;
	u32 dgain = 0;

	BUG_ON(!sensor_peri);

	device = v4l2_get_subdev_hostdata(sensor_peri->subdev_cis);
	BUG_ON(!device);

	expo = sensor_peri->cis.mode_chg_expo;
	again = sensor_peri->cis.mode_chg_again;
	dgain = sensor_peri->cis.mode_chg_dgain;

	dbg_sensor("[%s] expo(%d), again(%d), dgain(%d)\n", __func__,
			expo, again, dgain);

	if (expo == 0 || again < 1000 || dgain < 1000) {
		err("[%s] invalid mode change sensor settings exp(%d), gain(%d, %d)\n",
				__func__, expo, again, dgain);
		expo = sensor_peri->cis.cis_data->low_expo_start;
		again = 1000;
		dgain = 1000;
	}

	if (dgain > 1000)
		tgain = dgain * (again / 1000);
	else
		tgain = again;

	fimc_is_sensor_peri_s_analog_gain(device, again, again);
	fimc_is_sensor_peri_s_digital_gain(device, dgain, dgain);
	fimc_is_sensor_peri_s_exposure_time(device, expo, expo);

	sensor_peri->sensor_interface.cis_itf_ops.request_reset_expo_gain(&sensor_peri->sensor_interface,
			expo,
			tgain,
			again,
			dgain,
			expo,
			tgain,
			again,
			dgain);

	fimc_is_sensor_set_cis_uctrl_list(sensor_peri,
			expo, expo,
			tgain, tgain,
			again, again,
			dgain, dgain);

	return ret;
}

void fimc_is_sensor_flash_fire_work(struct work_struct *data)
{
	int ret = 0;
	struct fimc_is_flash *flash;
	struct fimc_is_flash_data *flash_data;
	struct fimc_is_device_sensor *device;
	struct fimc_is_device_sensor_peri *sensor_peri;
	u32 step = 0;

	BUG_ON(!data);

	flash_data = container_of(data, struct fimc_is_flash_data, flash_fire_work);
	BUG_ON(!flash_data);

	flash = container_of(flash_data, struct fimc_is_flash, flash_data);
	BUG_ON(!flash);

	sensor_peri = container_of(flash, struct fimc_is_device_sensor_peri, flash);
	BUG_ON(!sensor_peri);

	device = v4l2_get_subdev_hostdata(sensor_peri->subdev_flash);
	BUG_ON(!device);

	/* sensor stream off */
	ret = CALL_CISOPS(&sensor_peri->cis, cis_stream_off, sensor_peri->subdev_cis);
	if (ret < 0) {
		err("[%s] stream off fail\n", __func__);
		goto p_err;
	}

	ret = fimc_is_sensor_wait_streamoff(device);

	dbg_flash("[%s] steram off done\n", __func__);

	/* flash setting */

	step = flash->flash_ae.main_fls_strm_on_off_step;

	if (sensor_peri->sensor_interface.cis_mode) {
		fimc_is_sensor_peri_s_analog_gain(device, flash->flash_ae.again[step], flash->flash_ae.again[step]);
		fimc_is_sensor_peri_s_digital_gain(device, flash->flash_ae.dgain[step], flash->flash_ae.dgain[step]);
		fimc_is_sensor_peri_s_exposure_time(device, flash->flash_ae.expo[step], flash->flash_ae.expo[step]);

		sensor_peri->sensor_interface.cis_itf_ops.request_reset_expo_gain(&sensor_peri->sensor_interface,
			flash->flash_ae.expo[step],
			flash->flash_ae.tgain[step],
			flash->flash_ae.again[step],
			flash->flash_ae.dgain[step],
			flash->flash_ae.expo[step],
			flash->flash_ae.tgain[step],
			flash->flash_ae.again[step],
			flash->flash_ae.dgain[step]);

		fimc_is_sensor_set_cis_uctrl_list(sensor_peri,
			flash->flash_ae.expo[step], flash->flash_ae.expo[step],
			flash->flash_ae.tgain[step], flash->flash_ae.tgain[step],
			flash->flash_ae.again[step], flash->flash_ae.again[step],
			flash->flash_ae.dgain[step], flash->flash_ae.dgain[step]);
	} else {
		fimc_is_sensor_peri_s_analog_gain(device, flash->flash_ae.long_again[step], flash->flash_ae.short_again[step]);
		fimc_is_sensor_peri_s_digital_gain(device, flash->flash_ae.long_dgain[step], flash->flash_ae.short_dgain[step]);
		fimc_is_sensor_peri_s_exposure_time(device, flash->flash_ae.long_expo[step], flash->flash_ae.short_expo[step]);

		sensor_peri->sensor_interface.cis_itf_ops.request_reset_expo_gain(&sensor_peri->sensor_interface,
			flash->flash_ae.long_expo[step],
			flash->flash_ae.long_tgain[step],
			flash->flash_ae.long_again[step],
			flash->flash_ae.long_dgain[step],
			flash->flash_ae.short_expo[step],
			flash->flash_ae.short_tgain[step],
			flash->flash_ae.short_again[step],
			flash->flash_ae.short_dgain[step]);

		fimc_is_sensor_set_cis_uctrl_list(sensor_peri,
			flash->flash_ae.long_expo[step], flash->flash_ae.short_expo[step],
			flash->flash_ae.long_tgain[step], flash->flash_ae.short_tgain[step],
			flash->flash_ae.long_again[step], flash->flash_ae.short_again[step],
			flash->flash_ae.long_dgain[step], flash->flash_ae.short_dgain[step]);
	}

	dbg_flash("[%s][FLASH] mode %d, intensity %d, firing time %d us, step %d\n", __func__,
			flash->flash_data.mode,
			flash->flash_data.intensity,
			flash->flash_data.firing_time_us,
			step);

	/* flash fire */
	if (flash->flash_ae.pre_fls_ae_reset == true) {
		if (flash->flash_ae.frm_num_pre_fls != 0) {
			flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
			flash->flash_data.intensity = 0;
			flash->flash_data.firing_time_us = 0;

			info("[%s] pre-flash OFF(%d), pow(%d), time(%d)\n",
					__func__,
					flash->flash_data.mode,
					flash->flash_data.intensity,
					flash->flash_data.firing_time_us);

			ret = fimc_is_sensor_flash_fire(sensor_peri, flash->flash_data.intensity);
			if (ret) {
				err("failed to turn off flash at flash expired handler\n");
			}

			flash->flash_ae.pre_fls_ae_reset = false;
			flash->flash_ae.frm_num_pre_fls = 0;
		}
	} else if (flash->flash_ae.main_fls_ae_reset == true) {
		if (flash->flash_ae.main_fls_strm_on_off_step == 0) {
			if (flash->flash_data.flash_fired == false) {
				flash->flash_data.mode = CAM2_FLASH_MODE_SINGLE;
				flash->flash_data.intensity = 10;
				flash->flash_data.firing_time_us = 500000;

				info("[%s] main-flash ON(%d), pow(%d), time(%d)\n",
					__func__,
					flash->flash_data.mode,
					flash->flash_data.intensity,
					flash->flash_data.firing_time_us);

				ret = fimc_is_sensor_flash_fire(sensor_peri, flash->flash_data.intensity);
				if (ret) {
					err("failed to turn off flash at flash expired handler\n");
				}
			} else {
				flash->flash_ae.main_fls_ae_reset = false;
				flash->flash_ae.main_fls_strm_on_off_step = 0;
				flash->flash_ae.frm_num_main_fls[0] = 0;
				flash->flash_ae.frm_num_main_fls[1] = 0;
			}
			flash->flash_ae.main_fls_strm_on_off_step++;
		} else if (flash->flash_ae.main_fls_strm_on_off_step == 1) {
			flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
			flash->flash_data.intensity = 0;
			flash->flash_data.firing_time_us = 0;

			info("[%s] main-flash OFF(%d), pow(%d), time(%d)\n",
					__func__,
					flash->flash_data.mode,
					flash->flash_data.intensity,
					flash->flash_data.firing_time_us);

			ret = fimc_is_sensor_flash_fire(sensor_peri, flash->flash_data.intensity);
			if (ret) {
				err("failed to turn off flash at flash expired handler\n");
			}

			flash->flash_ae.main_fls_ae_reset = false;
			flash->flash_ae.main_fls_strm_on_off_step = 0;
			flash->flash_ae.frm_num_main_fls[0] = 0;
			flash->flash_ae.frm_num_main_fls[1] = 0;
		}
	}

p_err:
	/* sensor stream on */
	ret = CALL_CISOPS(&sensor_peri->cis, cis_stream_on, sensor_peri->subdev_cis);
	if (ret < 0) {
		err("[%s] stream on fail\n", __func__);
	}
}

void fimc_is_sensor_flash_expire_handler(unsigned long data)
{
	struct fimc_is_device_sensor_peri *device = (struct fimc_is_device_sensor_peri *)data;
	struct v4l2_subdev *subdev_flash;
	struct fimc_is_flash *flash;

	BUG_ON(!device);

	subdev_flash = device->subdev_flash;
	BUG_ON(!subdev_flash);

	flash = v4l2_get_subdevdata(subdev_flash);
	BUG_ON(!flash);

	schedule_work(&device->flash.flash_data.flash_expire_work);
}

void fimc_is_sensor_flash_expire_work(struct work_struct *data)
{
	int ret = 0;
	struct fimc_is_flash *flash;
	struct fimc_is_flash_data *flash_data;
	struct fimc_is_device_sensor_peri *sensor_peri;

	BUG_ON(!data);

	flash_data = container_of(data, struct fimc_is_flash_data, flash_expire_work);
	BUG_ON(!flash_data);

	flash = container_of(flash_data, struct fimc_is_flash, flash_data);
	BUG_ON(!flash);

	sensor_peri = container_of(flash, struct fimc_is_device_sensor_peri, flash);


	ret = fimc_is_sensor_flash_fire(sensor_peri, 0);
	if (ret) {
		err("failed to turn off flash at flash expired handler\n");
	}
}

int fimc_is_sensor_flash_fire(struct fimc_is_device_sensor_peri *device,
				u32 on)
{
	int ret = 0;
	struct v4l2_subdev *subdev_flash;
	struct fimc_is_flash *flash;
	struct v4l2_control ctrl;

	BUG_ON(!device);

	subdev_flash = device->subdev_flash;
	if (!subdev_flash) {
		err("subdev_flash is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	flash = v4l2_get_subdevdata(subdev_flash);
	if (!flash) {
		err("flash is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (flash->flash_data.mode == CAM2_FLASH_MODE_OFF && on == 1) {
		err("Flash mode is off");
		flash->flash_data.flash_fired = false;
		goto p_err;
	}

	if (flash->flash_data.flash_fired != (bool)on) {
		ctrl.id = V4L2_CID_FLASH_SET_FIRE;
		ctrl.value = on ? flash->flash_data.intensity : 0;
		ret = v4l2_subdev_call(subdev_flash, core, s_ctrl, &ctrl);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			goto p_err;
		}
		flash->flash_data.flash_fired = (bool)on;
	}

	if (flash->flash_data.mode == CAM2_FLASH_MODE_SINGLE) {
		if (flash->flash_data.flash_fired == true) {
			/* Flash firing time have to be setted in case of capture flash
			 * Max firing time of capture flash is 1 sec
			 */
			if (flash->flash_data.firing_time_us == 0 || flash->flash_data.firing_time_us > 1 * 1000 * 1000)
				flash->flash_data.firing_time_us = 1 * 1000 * 1000;

			setup_timer(&flash->flash_data.flash_expire_timer, fimc_is_sensor_flash_expire_handler, (unsigned long)device);
			mod_timer(&flash->flash_data.flash_expire_timer, jiffies +  usecs_to_jiffies(flash->flash_data.firing_time_us));
		} else {
			del_timer(&flash->flash_data.flash_expire_timer);
		}
	}

p_err:
	return ret;
}

int fimc_is_sensor_peri_notify_actuator(struct v4l2_subdev *subdev, void *arg)
{
	int ret = 0;
	u32 frame_index;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct fimc_is_actuator_interface *actuator_itf = NULL;

	BUG_ON(!subdev);
	BUG_ON(!arg);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	if (unlikely(!sensor_peri)) {
		err("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		dbg_sensor("%s: FIMC_IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		goto p_err;
	}

	actuator_itf = &sensor_peri->sensor_interface.actuator_itf;

	/* Set expecting actuator position */
	frame_index = (*(u32 *)arg + 1) % EXPECT_DM_NUM;
	sensor_peri->cis.expecting_lens_udm[frame_index].pos = actuator_itf->virtual_pos;

	dbg_actuator("%s: expexting frame cnt(%d), algorithm position(%d)\n",
			__func__, (*(u32 *)arg + 1), actuator_itf->virtual_pos);

p_err:

	return ret;
}

int fimc_is_sensor_peri_notify_vsync(struct v4l2_subdev *subdev, void *arg)
{
	int ret = 0;
	u32 vsync_count = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!subdev);
	BUG_ON(!arg);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	vsync_count = *(u32 *)arg;

	sensor_peri->cis.cis_data->sen_vsync_count = vsync_count;

	if (sensor_peri->sensor_interface.otf_flag_3aa == false) {
		/* run sensor setting thread */
		queue_kthread_work(&sensor_peri->worker, &sensor_peri->work);
	}

	ret = fimc_is_sensor_peri_notify_flash_fire(subdev, arg);
	if (unlikely(ret < 0))
		err("err!!!(%s), notify flash fire fail(%d)", __func__, ret);

	if (test_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		/* M2M case */
		if (sensor_peri->sensor_interface.otf_flag_3aa == false) {
			if (sensor_peri->actuator.valid_flag == 1)
				do_gettimeofday(&sensor_peri->actuator.start_time);

			ret = fimc_is_actuator_notify_m2m_actuator(subdev);
			if (ret)
				err("err!!!(%s), sensor notify M2M actuator fail(%d)", __func__, ret);
		}
	}

p_err:
	return ret;
}

#define cal_dur_time(st, ed) ((ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec))
int fimc_is_sensor_peri_notify_vblank(struct v4l2_subdev *subdev, void *arg)
{
	int ret = 0;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct fimc_is_actuator *actuator = NULL;

	BUG_ON(!subdev);
	BUG_ON(!arg);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("%s, module is NULL", __func__);
		return -EINVAL;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	if (unlikely(!module)) {
		err("%s, sensor_peri is NULL", __func__);
		return -EINVAL;
	}
	actuator = &sensor_peri->actuator;

	if (test_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		/* M2M case */
		if (sensor_peri->sensor_interface.otf_flag_3aa == false) {
			/* valid_time is calculated at once */
			if (actuator->valid_flag == 1) {
				actuator->valid_flag = 0;

				do_gettimeofday(&actuator->end_time);
				actuator->valid_time = cal_dur_time(actuator->start_time, actuator->end_time);
			}
		}

		ret = fimc_is_sensor_peri_notify_actuator(subdev, arg);
		if (ret < 0) {
			err("%s, notify_actuator is NULL", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

int fimc_is_sensor_peri_notify_flash_fire(struct v4l2_subdev *subdev, void *arg)
{
	int ret = 0;
	u32 vsync_count = 0;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_flash *flash = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!subdev);
	BUG_ON(!arg);

	vsync_count = *(u32 *)arg;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	BUG_ON(!module);

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	flash = &sensor_peri->flash;
	BUG_ON(!flash);

	dbg_flash("[%s](%d), notify flash mode(%d), pow(%d), time(%d), pre-num(%d), main_num(%d)\n",
		__func__, vsync_count,
		flash->flash_data.mode,
		flash->flash_data.intensity,
		flash->flash_data.firing_time_us,
		flash->flash_ae.frm_num_pre_fls,
		flash->flash_ae.frm_num_main_fls[flash->flash_ae.main_fls_strm_on_off_step]);


	if (flash->flash_ae.frm_num_pre_fls != 0) {
		dbg_flash("[%s](%d), pre-flash schedule\n", __func__, vsync_count);

		schedule_work(&sensor_peri->flash.flash_data.flash_fire_work);
	}

	if (flash->flash_ae.frm_num_main_fls[flash->flash_ae.main_fls_strm_on_off_step] != 0) {
		if (flash->flash_ae.frm_num_main_fls[flash->flash_ae.main_fls_strm_on_off_step] == vsync_count) {
			dbg_flash("[%s](%d), main-flash schedule\n", __func__, vsync_count);

			schedule_work(&sensor_peri->flash.flash_data.flash_fire_work);
		}
	}

	return ret;
}

int fimc_is_sensor_peri_notify_actuator_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
			goto p_err;
	}

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	if (test_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state) &&
			(sensor_peri->actuator.actuator_data.actuator_init)) {
		ret = v4l2_subdev_call(sensor_peri->subdev_actuator, core, init, 0);
		if (ret)
			warn("Actuator init fail\n");

		sensor_peri->actuator.actuator_data.actuator_init = false;
	}

p_err:
	return ret;
}

int fimc_is_sensor_peri_pre_flash_fire(struct v4l2_subdev *subdev, void *arg)
{
	int ret = 0;
	u32 vsync_count = 0;
	struct fimc_is_module_enum *module = NULL;
	struct fimc_is_flash *flash = NULL;
	struct fimc_is_sensor_ctl *sensor_ctl = NULL;
	camera2_flash_uctl_t *flash_uctl = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!subdev);
	BUG_ON(!arg);

	vsync_count = *(u32 *)arg;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	BUG_ON(!module);

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	sensor_ctl = &sensor_peri->cis.sensor_ctls[vsync_count % CAM2P0_UCTL_LIST_SIZE];

	flash = &sensor_peri->flash;
	BUG_ON(!flash);

	if (sensor_ctl->valid_flash_udctrl == false)
		goto p_err;

	flash_uctl = &sensor_ctl->cur_cam20_flash_udctrl;

	if ((flash_uctl->flashMode != flash->flash_data.mode) ||
		(flash_uctl->flashMode != CAM2_FLASH_MODE_OFF && flash_uctl->firingPower == 0)) {
		flash->flash_data.mode = flash_uctl->flashMode;
		flash->flash_data.intensity = flash_uctl->firingPower;
		flash->flash_data.firing_time_us = flash_uctl->firingTime;

		info("[%s](%d) pre-flash mode(%d), pow(%d), time(%d)\n", __func__,
			vsync_count, flash->flash_data.mode,
			flash->flash_data.intensity, flash->flash_data.firing_time_us);
		ret = fimc_is_sensor_flash_fire(sensor_peri, flash->flash_data.intensity);
	}

	/* HACK: reset uctl */
	flash_uctl->flashMode = 0;
	flash_uctl->firingPower = 0;
	flash_uctl->firingTime = 0;
	sensor_ctl->flash_frame_number = 0;
	sensor_ctl->valid_flash_udctrl = false;

p_err:
	return ret;
}

void fimc_is_sensor_peri_m2m_actuator(struct work_struct *data)
{
	int ret = 0;
	int index;
	struct fimc_is_device_sensor *device;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri;
	struct fimc_is_actuator_interface *actuator_itf;
	struct fimc_is_actuator *actuator;
	struct fimc_is_actuator_data *actuator_data;
	struct v4l2_subdev *subdev_module;
	u32 pre_position, request_frame_cnt;
	u32 cur_frame_cnt;
	u32 i;

	actuator_data = container_of(data, struct fimc_is_actuator_data, actuator_work);
	BUG_ON(!actuator_data);

	actuator = container_of(actuator_data, struct fimc_is_actuator, actuator_data);
	BUG_ON(!actuator);

	sensor_peri = container_of(actuator, struct fimc_is_device_sensor_peri, actuator);
	BUG_ON(!sensor_peri);

	device = v4l2_get_subdev_hostdata(sensor_peri->subdev_actuator);
	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("subdev_module is NULL");
	}

	actuator_itf = &sensor_peri->sensor_interface.actuator_itf;

	cur_frame_cnt = device->ischain->group_3aa.fcount;
	request_frame_cnt = sensor_peri->actuator.pre_frame_cnt[0];
	pre_position = sensor_peri->actuator.pre_position[0];
	index = sensor_peri->actuator.actuator_index;

	for (i = 0; i < index; i++) {
		sensor_peri->actuator.pre_position[i] = sensor_peri->actuator.pre_position[i+1];
		sensor_peri->actuator.pre_frame_cnt[i] = sensor_peri->actuator.pre_frame_cnt[i+1];
	}

	/* After moving index, latest value change is Zero */
	sensor_peri->actuator.pre_position[index] = 0;
	sensor_peri->actuator.pre_frame_cnt[index] = 0;

	sensor_peri->actuator.actuator_index --;
	index = sensor_peri->actuator.actuator_index;

	if (cur_frame_cnt != request_frame_cnt)
		warn("AF frame count is not match (AF request count : %d, setting request count : %d\n",
				request_frame_cnt, cur_frame_cnt);

	ret = fimc_is_actuator_ctl_set_position(device, pre_position);
	if (ret < 0) {
		err("err!!! ret(%d), invalid position(%d)",
				ret, pre_position);
	}
	actuator_itf->hw_pos = pre_position;

	dbg_sensor("%s: pre_frame_count(%d), pre_position(%d), cur_frame_cnt (%d), \
			index(%d)\n", __func__,
			request_frame_cnt,
			pre_position,
			cur_frame_cnt,
			index);
}


void fimc_is_sensor_peri_probe(struct fimc_is_device_sensor_peri *sensor_peri)
{
	BUG_ON(!sensor_peri);

	clear_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state);
	clear_bit(FIMC_IS_SENSOR_FLASH_AVAILABLE, &sensor_peri->peri_state);
	clear_bit(FIMC_IS_SENSOR_COMPANION_AVAILABLE, &sensor_peri->peri_state);
	clear_bit(FIMC_IS_SENSOR_OIS_AVAILABLE, &sensor_peri->peri_state);

	INIT_WORK(&sensor_peri->flash.flash_data.flash_fire_work, fimc_is_sensor_flash_fire_work);
	INIT_WORK(&sensor_peri->flash.flash_data.flash_expire_work, fimc_is_sensor_flash_expire_work);

	INIT_WORK(&sensor_peri->cis.cis_status_dump_work, fimc_is_sensor_cis_status_dump_work);

	INIT_WORK(&sensor_peri->actuator.actuator_data.actuator_work, fimc_is_sensor_peri_m2m_actuator);

	hrtimer_init(&sensor_peri->actuator.actuator_data.afwindow_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
}

int fimc_is_sensor_peri_s_stream(struct fimc_is_device_sensor *device,
					bool on)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct fimc_is_cis *cis = NULL;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	subdev_cis = sensor_peri->subdev_cis;
	if (!subdev_cis) {
		err("[SEN:%d] no subdev_cis(s_stream, on:%d)", module->sensor_id, on);
		ret = -ENXIO;
		goto p_err;
	}
	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev_cis);
	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	ret = fimc_is_sensor_peri_debug_fixed((struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(subdev_module));
	if (ret) {
		err("fimc_is_sensor_peri_debug_fixed is fail(%d)", ret);
		goto p_err;
	}

	if (on) {
		if (cis->need_mode_change == false) {
#if 0
			/* only first time after camera on */
			fimc_is_sensor_initial_setting_low_exposure(sensor_peri);
#endif
			cis->need_mode_change = true;
			sensor_peri->actuator.actuator_data.timer_check = HRTIMER_POSSIBLE;
		} else {
			fimc_is_sensor_setting_mode_change(sensor_peri);
		}

		ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	} else {
		ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
		if (ret == 0) {
			ret = fimc_is_sensor_wait_streamoff(device);
		}

		hrtimer_cancel(&sensor_peri->actuator.actuator_data.afwindow_timer);
	}
	if (ret) {
		err("[SEN:%d] v4l2_subdev_call(s_stream, on:%d) is fail(%d)",
				module->sensor_id, on, ret);
		goto p_err;
	}

#ifdef HACK_SDK_RESET
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	sensor_peri->sensor_interface.reset_flag = false;
#endif

p_err:
	return ret;
}

int fimc_is_sensor_peri_s_frame_duration(struct fimc_is_device_sensor *device,
					u32 frame_duration)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

#if defined(FIXED_FPS_DEBUG)
	frame_duration = FPS_TO_DURATION_US(FIXED_FPS_VALUE);
	dbg_sensor("FIXED_FPS_DEBUG = %d\n", FIXED_FPS_VALUE);
#endif

	ret = CALL_CISOPS(&sensor_peri->cis, cis_set_frame_duration, sensor_peri->subdev_cis, frame_duration);
	if (ret < 0) {
		err("err!!! ret(%d)", ret);
		goto p_err;
	}
	device->frame_duration = frame_duration;

p_err:
	return ret;
}

int fimc_is_sensor_peri_s_exposure_time(struct fimc_is_device_sensor *device,
				u32 long_exposure_time, u32 short_exposure_time)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct ae_param exposure;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	if ((long_exposure_time <= 0) || (short_exposure_time <= 0)) {
		err("it is wrong exposure time (long:%d, short:%d)", long_exposure_time, short_exposure_time);
		ret = -EINVAL;
		goto p_err;
	}

#if defined(FIXED_EXPOSURE_DEBUG)
	long_exposure_time = FIXED_EXPOSURE_VALUE;
	short_exposure_time = FIXED_EXPOSURE_VALUE;
	dbg_sensor("FIXED_EXPOSURE_DEBUG = %d\n", FIXED_EXPOSURE_VALUE);
#endif

	exposure.long_val = long_exposure_time;
	exposure.short_val = short_exposure_time;
	ret = CALL_CISOPS(&sensor_peri->cis, cis_set_exposure_time, sensor_peri->subdev_cis, &exposure);
	if (ret < 0) {
		err("err!!! ret(%d)", ret);
		goto p_err;
	}
	device->exposure_time = long_exposure_time;

p_err:
	return ret;
}

int fimc_is_sensor_peri_s_analog_gain(struct fimc_is_device_sensor *device,
	u32 long_analog_gain, u32 short_analog_gain)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct ae_param again;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	if (long_analog_gain <= 0 ) {
		err("it is wrong analog gain(%d)", long_analog_gain);
		ret = -EINVAL;
		goto p_err;
	}

#if defined(FIXED_AGAIN_DEBUG)
	long_analog_gain = FIXED_AGAIN_VALUE * 10;
	short_analog_gain = FIXED_AGAIN_VALUE * 10;
	dbg_sensor("FIXED_AGAIN_DEBUG = %d\n", FIXED_AGAIN_VALUE);
#endif

	again.long_val = long_analog_gain;
	again.short_val = short_analog_gain;
	ret = CALL_CISOPS(&sensor_peri->cis, cis_set_analog_gain, sensor_peri->subdev_cis, &again);
	if (ret < 0) {
		err("err!!! ret(%d)", ret);
		goto p_err;
	}
	/* 0: Previous input, 1: Current input */
	sensor_peri->cis.cis_data->analog_gain[0] = sensor_peri->cis.cis_data->analog_gain[1];
	sensor_peri->cis.cis_data->analog_gain[1] = long_analog_gain;

p_err:
	return ret;
}

int fimc_is_sensor_peri_s_digital_gain(struct fimc_is_device_sensor *device,
	u32 long_digital_gain, u32 short_digital_gain)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	struct ae_param dgain;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	if (long_digital_gain <= 0 ) {
		err("it is wrong digital gain(%d)", long_digital_gain);
		ret = -EINVAL;
		goto p_err;
	}

#if defined(FIXED_DGAIN_DEBUG)
	long_digital_gain = FIXED_DGAIN_VALUE * 10;
	short_digital_gain = FIXED_DGAIN_VALUE * 10;
	dbg_sensor("FIXED_DGAIN_DEBUG = %d\n", FIXED_DGAIN_VALUE);
#endif

	dgain.long_val = long_digital_gain;
	dgain.short_val = short_digital_gain;
	ret = CALL_CISOPS(&sensor_peri->cis, cis_set_digital_gain, sensor_peri->subdev_cis, &dgain);
	if (ret < 0) {
		err("err!!! ret(%d)", ret);
		goto p_err;
	}
	/* 0: Previous input, 1: Current input */
	sensor_peri->cis.cis_data->digital_gain[0] = sensor_peri->cis.cis_data->digital_gain[1];
	sensor_peri->cis.cis_data->digital_gain[1] = long_digital_gain;

p_err:
	return ret;
}

int fimc_is_sensor_peri_adj_ctrl(struct fimc_is_device_sensor *device,
		u32 input,
		struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;

	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!ctrl);

	subdev_module = device->subdev_module;

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	switch (ctrl->id) {
	case V4L2_CID_SENSOR_ADJUST_FRAME_DURATION:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_adjust_frame_duration, sensor_peri->subdev_cis, input, &ctrl->value);
		break;
	case V4L2_CID_SENSOR_ADJUST_ANALOG_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_adjust_analog_gain, sensor_peri->subdev_cis, input, &ctrl->value);
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (ret < 0) {
		err("err!!! ret(%d)", ret);
		ctrl->value = 0;
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sensor_peri_compensate_gain_for_ext_br(struct fimc_is_device_sensor *device,
				u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;

	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);

	subdev_module = device->subdev_module;

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;

	ret = CALL_CISOPS(&sensor_peri->cis, cis_compensate_gain_for_extremely_br, sensor_peri->subdev_cis, expo, again, dgain);
	if (ret < 0) {
		err("err!!! ret(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sensor_peri_debug_fixed(struct fimc_is_device_sensor *device)
{
	int ret = 0;

	if (!device) {
		err("device is null\n");
		goto p_err;
	}

#ifdef FIXED_FPS_DEBUG
	dbg_sensor("FIXED_FPS_DEBUG = %d\n", FIXED_FPS_VALUE);
	if (fimc_is_sensor_peri_s_frame_duration(device, FPS_TO_DURATION_US(FIXED_FPS_VALUE))) {
		err("failed to set frame duration : %d\n - %d",
			FIXED_FPS_VALUE, ret);
		goto p_err;
	}
#endif
#ifdef FIXED_EXPOSURE_DEBUG
	dbg_sensor("FIXED_EXPOSURE_DEBUG = %d\n", FIXED_EXPOSURE_VALUE);
	if (fimc_is_sensor_peri_s_exposure_time(device, FIXED_EXPOSURE_VALUE, FIXED_EXPOSURE_VALUE)) {
		err("failed to set exposure time : %d\n - %d",
			FIXED_EXPOSURE_VALUE, ret);
		goto p_err;
	}
#endif
#ifdef FIXED_AGAIN_DEBUG
	dbg_sensor("FIXED_AGAIN_DEBUG = %d\n", FIXED_AGAIN_VALUE);
	ret = fimc_is_sensor_peri_s_analog_gain(device, FIXED_AGAIN_VALUE * 10, FIXED_AGAIN_VALUE * 10);
	if (ret < 0) {
		err("failed to set analog gain : %d\n - %d",
				FIXED_AGAIN_VALUE, ret);
		goto p_err;
	}
#endif
#ifdef FIXED_DGAIN_DEBUG
	dbg_sensor("FIXED_DGAIN_DEBUG = %d\n", FIXED_DGAIN_VALUE);
	ret = fimc_is_sensor_peri_s_digital_gain(device, FIXED_DGAIN_VALUE * 10, FIXED_DGAIN_VALUE * 10);
	if (ret < 0) {
		err("failed to set digital gain : %d\n - %d",
				FIXED_DGAIN_VALUE, ret);
		goto p_err;
	}
#endif

p_err:
	return ret;
}

void fimc_is_sensor_peri_actuator_check_landing_time(ulong data)
{
	u32 *check_time_out = (u32 *)data;

	BUG_ON(!check_time_out);

	warn("Actuator softlanding move is time overrun. Skip by force.\n");
	*check_time_out = true;
}

int fimc_is_sensor_peri_actuator_check_move_done(struct fimc_is_device_sensor_peri *device)
{
	int ret = 0;
	struct fimc_is_actuator *actuator;
	struct fimc_is_actuator_interface *actuator_itf;
	struct fimc_is_actuator_data *actuator_data;
	struct v4l2_control v4l2_ctrl;

	BUG_ON(!device);

	actuator = &device->actuator;
	actuator_itf = &device->sensor_interface.actuator_itf;
	actuator_data = &actuator->actuator_data;

	v4l2_ctrl.id = V4L2_CID_ACTUATOR_GET_STATUS;
	v4l2_ctrl.value = ACTUATOR_STATUS_BUSY;
	actuator_data->check_time_out = false;

	mod_timer(&actuator->actuator_data.timer_wait,
		jiffies +
		msecs_to_jiffies(actuator_itf->soft_landing_table.step_delay));
	do {
		ret = v4l2_subdev_call(device->subdev_actuator, core, g_ctrl, &v4l2_ctrl);
		if (ret) {
			err("[SEN:%d] v4l2_subdev_call(g_ctrl, id:%d) is fail",
					actuator->id, v4l2_ctrl.id);
			return ret;
		}
	} while (v4l2_ctrl.value == ACTUATOR_STATUS_BUSY &&
			actuator_data->check_time_out == false);

	del_timer(&actuator->actuator_data.timer_wait);

	return ret;
}

int fimc_is_sensor_peri_actuator_softlanding(struct fimc_is_device_sensor_peri *device)
{
	int ret = 0;
	int i;
	struct fimc_is_actuator *actuator;
	struct fimc_is_actuator_data *actuator_data;
	struct fimc_is_actuator_interface *actuator_itf;
	struct fimc_is_actuator_softlanding_table *soft_landing_table;
	struct v4l2_control v4l2_ctrl;

	BUG_ON(!device);

	if (!test_bit(FIMC_IS_SENSOR_ACTUATOR_AVAILABLE, &device->peri_state)) {
		dbg_sensor("%s: FIMC_IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		return ret;
	}

	actuator_itf = &device->sensor_interface.actuator_itf;
	actuator = &device->actuator;
	actuator_data = &actuator->actuator_data;
	soft_landing_table = &actuator_itf->soft_landing_table;

	setup_timer(&actuator_data->timer_wait,
			fimc_is_sensor_peri_actuator_check_landing_time,
			(ulong)&actuator_data->check_time_out);

	if (!soft_landing_table->enable) {
		soft_landing_table->position_num = 1;
		soft_landing_table->step_delay = 200;
		soft_landing_table->hw_table[0] = 0;
	}

	ret = fimc_is_sensor_peri_actuator_check_move_done(device);
	if (ret) {
		err("failed to get actuator position : ret(%d)\n", ret);
		return ret;
	}

	for (i = 0; i < soft_landing_table->position_num; i++) {
		if (actuator->position < soft_landing_table->hw_table[i])
			continue;

		dbg_sensor("%s: cur_pos(%d) --> tgt_pos(%d)\n",
					__func__,
					actuator->position, soft_landing_table->hw_table[i]);

		v4l2_ctrl.id = V4L2_CID_ACTUATOR_SET_POSITION;
		v4l2_ctrl.value = soft_landing_table->hw_table[i];
		ret = v4l2_subdev_call(device->subdev_actuator, core, s_ctrl, &v4l2_ctrl);
		if (ret) {
			err("[SEN:%d] v4l2_subdev_call(s_ctrl, id:%d) is fail(%d)",
					actuator->id, v4l2_ctrl.id, ret);
			return ret;
		}

		actuator_itf->virtual_pos = soft_landing_table->virtual_table[i];
		actuator_itf->hw_pos = soft_landing_table->hw_table[i];

		/* The actuator needs a delay time when lens moving for soft landing. */
		mdelay(soft_landing_table->step_delay);

		ret = fimc_is_sensor_peri_actuator_check_move_done(device);
		if (ret) {
			err("failed to get actuator position : ret(%d)\n", ret);
			return ret;
		}
	}

	return ret;
}

/* M2M AF position setting */
int fimc_is_sensor_peri_call_m2m_actuator(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	int index;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri;
	struct v4l2_subdev *subdev_module;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		return -EINVAL;
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("subdev_module is NULL");
		return -EINVAL;
	}

	sensor_peri = (struct fimc_is_device_sensor_peri *)module->private_data;
	if (!sensor_peri) {
		err("sensor_peri is NULL");
		return -EINVAL;
	}

	index = sensor_peri->actuator.actuator_index;

	if (index >= 0) {
		dbg_sensor("%s: M2M actuator set schedule\n", __func__);
		schedule_work(&sensor_peri->actuator.actuator_data.actuator_work);
	}
	else {
		/* request_count zero is not request set position in FW */
		dbg_sensor("actuator request position is Zero\n");
		sensor_peri->actuator.actuator_index = -1;

		return ret;
	}

	return ret;
}
