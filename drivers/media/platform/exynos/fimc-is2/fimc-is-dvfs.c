/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-hw-dvfs.h"
#include <linux/videodev2_exynos_camera.h>

#ifdef CONFIG_PM_DEVFREQ
extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;
extern struct pm_qos_request exynos_isp_qos_hpg;

static inline int fimc_is_get_start_sensor_cnt(struct fimc_is_core *core)
{
	int i, sensor_cnt = 0;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++)
		if (test_bit(FIMC_IS_SENSOR_FRONT_START, &(core->sensor[i].state)))
			sensor_cnt++;

	return sensor_cnt;
}

static int fimc_is_get_target_resol(struct fimc_is_device_ischain *device)
{
	int resol = 0;
#ifdef SOC_MCS
	int i = 0;
	struct fimc_is_group *group;

	group = &device->group_mcs;

	if (!test_bit(FIMC_IS_GROUP_INIT, &group->state))
		return resol;

	for (i = ENTRY_M0P; i <= ENTRY_M4P; i++)
		if (group->subdev[i] && test_bit(FIMC_IS_SUBDEV_START, &(group->subdev[i]->state)))
			resol = max_t(int, resol, group->subdev[i]->output.width * group->subdev[i]->output.height);
#else
	resol = device->scp.output.width * device->scp.output.height;
#endif
	return resol;
}

int fimc_is_dvfs_init(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;

	BUG_ON(!resourcemgr);

	dvfs_ctrl = &resourcemgr->dvfs_ctrl;

	dvfs_ctrl->cur_int_qos = 0;
	dvfs_ctrl->cur_mif_qos = 0;
	dvfs_ctrl->cur_cam_qos = 0;
	dvfs_ctrl->cur_i2c_qos = 0;
	dvfs_ctrl->cur_disp_qos = 0;
	dvfs_ctrl->cur_hpg_qos = 0;
	dvfs_ctrl->cur_hmp_bst = 0;

	/* init spin_lock for clock gating */
	mutex_init(&dvfs_ctrl->lock);

	if (!(dvfs_ctrl->static_ctrl))
		dvfs_ctrl->static_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);
	if (!(dvfs_ctrl->dynamic_ctrl))
		dvfs_ctrl->dynamic_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);
	if (!(dvfs_ctrl->external_ctrl))
		dvfs_ctrl->external_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);

	if (!dvfs_ctrl->static_ctrl || !dvfs_ctrl->dynamic_ctrl || !dvfs_ctrl->external_ctrl) {
		err("dvfs_ctrl alloc is failed!!\n");
		return -ENOMEM;
	}

	/* assign static / dynamic scenario check logic data */
	ret = fimc_is_hw_dvfs_init((void *)dvfs_ctrl);
	if (ret) {
		err("fimc_is_hw_dvfs_init is failed(%d)\n", ret);
		return -EINVAL;
	}

	/* default value is 0 */
	dvfs_ctrl->dvfs_table_idx = 0;
	clear_bit(FIMC_IS_DVFS_SEL_TABLE, &dvfs_ctrl->state);

	return 0;
}

int fimc_is_dvfs_sel_table(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;
	u32 dvfs_table_idx = 0;

	BUG_ON(!resourcemgr);

	dvfs_ctrl = &resourcemgr->dvfs_ctrl;

	if (test_bit(FIMC_IS_DVFS_SEL_TABLE, &dvfs_ctrl->state))
		return 0;

	switch(resourcemgr->hal_version) {
	case IS_HAL_VER_1_0:
		dvfs_table_idx = 0;
		break;
	case IS_HAL_VER_3_2:
		dvfs_table_idx = 1;
		break;
	default:
		err("hal version is unknown");
		dvfs_table_idx = 0;
		ret = -EINVAL;
		break;
	}

	if (dvfs_table_idx >= dvfs_ctrl->dvfs_table_max) {
		err("dvfs index(%d) is invalid", dvfs_table_idx);
		ret = -EINVAL;
		goto p_err;
	}

	resourcemgr->dvfs_ctrl.dvfs_table_idx = dvfs_table_idx;
	set_bit(FIMC_IS_DVFS_SEL_TABLE, &dvfs_ctrl->state);

p_err:
	info("[RSC] %s(%d):%d\n", __func__, dvfs_table_idx, ret);
	return ret;
}

int fimc_is_dvfs_sel_static(struct fimc_is_device_ischain *device)
{
	struct fimc_is_core *core;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl;
	struct fimc_is_dvfs_scenario *scenarios;
	struct fimc_is_resourcemgr *resourcemgr;
	int i, scenario_id, scenario_cnt;
	int position, resol, fps, stream_cnt;

	BUG_ON(!device);
	BUG_ON(!device->interface);

	core = (struct fimc_is_core *)device->interface->core;
	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	static_ctrl = dvfs_ctrl->static_ctrl;

	if (!test_bit(FIMC_IS_DVFS_SEL_TABLE, &dvfs_ctrl->state)) {
		err("dvfs table is NOT selected");
		return -EINVAL;
	}

	/* static scenario */
	if (!static_ctrl) {
		err("static_dvfs_ctrl is NULL");
		return -EINVAL;
	}

	if (static_ctrl->scenario_cnt == 0) {
		pr_debug("static_scenario's count is zero");
		return -EINVAL;
	}

	scenarios = static_ctrl->scenarios;
	scenario_cnt = static_ctrl->scenario_cnt;
	position = fimc_is_sensor_g_position(device->sensor);
	resol = fimc_is_get_target_resol(device);
	fps = fimc_is_sensor_g_framerate(device->sensor);
	stream_cnt = fimc_is_get_start_sensor_cnt(core);

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		if ((scenarios[i].check_func(device, NULL, position, resol, fps, stream_cnt)) > 0) {
			scenario_id = scenarios[i].scenario_id;
			static_ctrl->cur_scenario_id = scenario_id;
			static_ctrl->cur_scenario_idx = i;
			static_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			return scenario_id;
		}
	}

	warn("couldn't find static dvfs scenario [sensor:(%d/%d)/fps:%d/setfile:%d/resol:(%d)]\n",
		fimc_is_get_start_sensor_cnt(core),
		device->sensor->pdev->id,
		fps, (device->setfile & FIMC_IS_SETFILE_MASK), resol);

	static_ctrl->cur_scenario_id = FIMC_IS_SN_DEFAULT;
	static_ctrl->cur_scenario_idx = -1;
	static_ctrl->cur_frame_tick = -1;

	return FIMC_IS_SN_DEFAULT;
}

int fimc_is_dvfs_sel_dynamic(struct fimc_is_device_ischain *device, struct fimc_is_group *group)
{
	int ret;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *dynamic_ctrl;
	struct fimc_is_dvfs_scenario *scenarios;
	struct fimc_is_resourcemgr *resourcemgr;
	int i, scenario_id, scenario_cnt;
	int position, resol, fps;

	BUG_ON(!device);

	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;

	if (!test_bit(FIMC_IS_DVFS_SEL_TABLE, &dvfs_ctrl->state)) {
		err("dvfs table is NOT selected");
		return -EINVAL;
	}

	/* dynamic scenario */
	if (!dynamic_ctrl) {
		err("dynamic_dvfs_ctrl is NULL");
		return -EINVAL;
	}

	if (dynamic_ctrl->scenario_cnt == 0) {
		pr_debug("dynamic_scenario's count is zero");
		return -EINVAL;
	}

	scenarios = dynamic_ctrl->scenarios;
	scenario_cnt = dynamic_ctrl->scenario_cnt;

	if (dynamic_ctrl->cur_frame_tick >= 0) {
		(dynamic_ctrl->cur_frame_tick)--;
		/*
		 * when cur_frame_tick is lower than 0, clear current scenario.
		 * This means that current frame tick to keep dynamic scenario
		 * was expired.
		 */
		if (dynamic_ctrl->cur_frame_tick < 0) {
			dynamic_ctrl->cur_scenario_id = -1;
			dynamic_ctrl->cur_scenario_idx = -1;
		}
	}

	if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		goto p_again;

	position = fimc_is_sensor_g_position(device->sensor);
	resol = fimc_is_get_target_resol(device);
	fps = fimc_is_sensor_g_framerate(device->sensor);

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		ret = scenarios[i].check_func(device, group, position, resol, fps, 0);
		switch (ret) {
		case DVFS_MATCHED:
			scenario_id = scenarios[i].scenario_id;
			dynamic_ctrl->cur_scenario_id = scenario_id;
			dynamic_ctrl->cur_scenario_idx = i;
			dynamic_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;

			return scenario_id;
		case DVFS_SKIP:
			goto p_again;
		case DVFS_NOT_MATCHED:
		default:
			continue;
		}
	}

p_again:
	return  -EAGAIN;
}

int fimc_is_dvfs_sel_external(struct fimc_is_device_sensor *device)
{
	struct fimc_is_core *core;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *external_ctrl;
	struct fimc_is_dvfs_scenario *scenarios;
	struct fimc_is_resourcemgr *resourcemgr;
	int i, scenario_id, scenario_cnt;
	int position, resol, fps, stream_cnt;

	BUG_ON(!device);

	core = device->private_data;
	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	external_ctrl = dvfs_ctrl->external_ctrl;

	if (!test_bit(FIMC_IS_DVFS_SEL_TABLE, &dvfs_ctrl->state)) {
		err("dvfs table is NOT selected");
		return -EINVAL;
	}

	/* external scenario */
	if (!external_ctrl) {
		warn("external_dvfs_ctrl is NULL, default max dvfs lv");
		return FIMC_IS_SN_MAX;
	}

	scenarios = external_ctrl->scenarios;
	scenario_cnt = external_ctrl->scenario_cnt;
	position = fimc_is_sensor_g_position(device);
	resol = fimc_is_sensor_g_width(device) * fimc_is_sensor_g_height(device);
	fps = fimc_is_sensor_g_framerate(device);
	stream_cnt = fimc_is_get_start_sensor_cnt(core);

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].ext_check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		if ((scenarios[i].ext_check_func(device, position, resol, fps, stream_cnt)) > 0) {
			scenario_id = scenarios[i].scenario_id;
			external_ctrl->cur_scenario_id = scenario_id;
			external_ctrl->cur_scenario_idx = i;
			external_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			return scenario_id;
		}
	}

	warn("couldn't find external dvfs scenario [sensor:(%d/%d)/fps:%d/resol:(%d)]\n",
		stream_cnt, position, fps, resol);

	external_ctrl->cur_scenario_id = FIMC_IS_SN_MAX;
	external_ctrl->cur_scenario_idx = -1;
	external_ctrl->cur_frame_tick = -1;

	return FIMC_IS_SN_MAX;
}


int fimc_is_get_qos(struct fimc_is_core *core, u32 type, u32 scenario_id)
{
	struct exynos_platform_fimc_is	*pdata = NULL;
	int qos = 0;
	u32 dvfs_idx = core->resourcemgr.dvfs_ctrl.dvfs_table_idx;

	pdata = core->pdata;
	if (pdata == NULL) {
		err("pdata is NULL\n");
		return -EINVAL;
	}

	if (type >= FIMC_IS_DVFS_END) {
		err("Cannot find DVFS value");
		return -EINVAL;
	}

	if (dvfs_idx >= FIMC_IS_DVFS_TABLE_IDX_MAX) {
		err("invalid dvfs index(%d)", dvfs_idx);
		dvfs_idx = 0;
	}

	qos = pdata->dvfs_data[dvfs_idx][scenario_id][type];

	return qos;
}

int fimc_is_set_dvfs(struct fimc_is_core *core, struct fimc_is_device_ischain *device, u32 scenario_id)
{
	int ret = 0;
	int int_qos, mif_qos, i2c_qos, cam_qos, disp_qos, hpg_qos;
	struct fimc_is_resourcemgr *resourcemgr;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;

	if (core == NULL) {
		err("core is NULL\n");
		return -EINVAL;
	}

	resourcemgr = &core->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);

	int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, scenario_id);
	mif_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_MIF, scenario_id);
	i2c_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_I2C, scenario_id);
	cam_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CAM, scenario_id);
	disp_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_DISP, scenario_id);
	hpg_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_HPG, scenario_id);

	if ((int_qos < 0) || (mif_qos < 0) || (i2c_qos < 0)
	|| (cam_qos < 0) || (disp_qos < 0)) {
		err("getting qos value is failed!!\n");
		return -EINVAL;
	}

	/* check current qos */
	if (int_qos && dvfs_ctrl->cur_int_qos != int_qos) {
		if (i2c_qos && device) {
			ret = fimc_is_itf_i2c_lock(device, i2c_qos, true);
			if (ret) {
				err("fimc_is_itf_i2_clock fail\n");
				goto exit;
			}
		}

		pm_qos_update_request(&exynos_isp_qos_int, int_qos);
		dvfs_ctrl->cur_int_qos = int_qos;

		if (i2c_qos && device) {
			/* i2c unlock */
			ret = fimc_is_itf_i2c_lock(device, i2c_qos, false);
			if (ret) {
				err("fimc_is_itf_i2c_unlock fail\n");
				goto exit;
			}
		}
	}

	if (mif_qos && dvfs_ctrl->cur_mif_qos != mif_qos) {
		pm_qos_update_request(&exynos_isp_qos_mem, mif_qos);
		dvfs_ctrl->cur_mif_qos = mif_qos;
	}

	if (cam_qos && dvfs_ctrl->cur_cam_qos != cam_qos) {
		pm_qos_update_request(&exynos_isp_qos_cam, cam_qos);
		dvfs_ctrl->cur_cam_qos = cam_qos;
	}

	/* hpg_qos : number of minimum online CPU */
	if (hpg_qos && dvfs_ctrl->cur_hpg_qos != hpg_qos) {
		pm_qos_update_request(&exynos_isp_qos_hpg, hpg_qos);
		dvfs_ctrl->cur_hpg_qos = hpg_qos;

		/* for migration to big core */
		if (hpg_qos >= 6) {
			if (!dvfs_ctrl->cur_hmp_bst) {
				set_hmp_boost(1);
				dvfs_ctrl->cur_hmp_bst = 1;
			}
		} else {
			if (dvfs_ctrl->cur_hmp_bst) {
				set_hmp_boost(0);
				dvfs_ctrl->cur_hmp_bst = 0;
			}
		}
	}

	info("[RSC:%d]: New QoS [INT(%d), MIF(%d), CAM(%d), DISP(%d), I2C(%d), HPG(%d, %d)]\n",
			device ? device->instance : 0, int_qos, mif_qos,
			cam_qos, disp_qos, i2c_qos, hpg_qos, dvfs_ctrl->cur_hmp_bst);
exit:
	return ret;
}
#endif
