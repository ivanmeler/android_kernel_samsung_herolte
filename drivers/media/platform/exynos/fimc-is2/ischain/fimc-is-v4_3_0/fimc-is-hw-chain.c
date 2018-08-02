/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "fimc-is-config.h"
#include "fimc-is-param.h"
#include "fimc-is-type.h"
#include "fimc-is-regs.h"
#include "fimc-is-core.h"
#include "fimc-is-hw-chain.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-flite.h"
#include "fimc-is-device-csi.h"
#include "fimc-is-device-ischain.h"

/* Define default subdev ops if there are not used subdev IP */
const struct fimc_is_subdev_ops fimc_is_subdev_scc_ops;
const struct fimc_is_subdev_ops fimc_is_subdev_scp_ops;

int fimc_is_hw_group_cfg(void *group_data)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_device_ischain *device;

	BUG_ON(!group_data);

	group = group_data;
	device = group->device;

	if (!device) {
		err("device is NULL");
		BUG();
	}

	switch (group->slot) {
	case GROUP_SLOT_3AA:
		group->subdev[ENTRY_3AA] = &device->group_3aa.leader;
		group->subdev[ENTRY_3AC] = &device->txc;
		group->subdev[ENTRY_3AP] = &device->txp;
		group->subdev[ENTRY_ISP] = NULL;
		group->subdev[ENTRY_IXC] = NULL;
		group->subdev[ENTRY_IXP] = NULL;
		group->subdev[ENTRY_DRC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_ODC] = NULL;
		group->subdev[ENTRY_DNR] = NULL;
		group->subdev[ENTRY_SCC] = NULL;
		group->subdev[ENTRY_SCP] = NULL;
		group->subdev[ENTRY_VRA] = NULL;

		device->txc.param_dma_ot = PARAM_3AA_VDMA4_OUTPUT;
		device->txp.param_dma_ot = PARAM_3AA_VDMA2_OUTPUT;
		break;
	case GROUP_SLOT_ISP:
		group->subdev[ENTRY_3AA] = NULL;
		group->subdev[ENTRY_3AC] = NULL;
		group->subdev[ENTRY_3AP] = NULL;
		group->subdev[ENTRY_ISP] = &device->group_isp.leader;
		group->subdev[ENTRY_IXC] = &device->ixc;
		group->subdev[ENTRY_IXP] = &device->ixp;
		group->subdev[ENTRY_DRC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_ODC] = NULL;
		group->subdev[ENTRY_DNR] = NULL;
		group->subdev[ENTRY_SCC] = NULL;
		group->subdev[ENTRY_SCP] = NULL;
		group->subdev[ENTRY_VRA] = NULL;
		break;
	case GROUP_SLOT_DIS:
		group->subdev[ENTRY_3AA] = NULL;
		group->subdev[ENTRY_3AC] = NULL;
		group->subdev[ENTRY_3AP] = NULL;
		group->subdev[ENTRY_ISP] = NULL;
		group->subdev[ENTRY_IXC] = NULL;
		group->subdev[ENTRY_IXP] = NULL;
		group->subdev[ENTRY_DRC] = NULL;
		group->subdev[ENTRY_DIS] = &device->group_dis.leader;
		group->subdev[ENTRY_ODC] = &device->odc;
		group->subdev[ENTRY_DNR] = &device->dnr;
		group->subdev[ENTRY_SCC] = NULL;
		group->subdev[ENTRY_SCP] = NULL;
		group->subdev[ENTRY_VRA] = NULL;
		break;
	case GROUP_SLOT_MCS:
		group->subdev[ENTRY_3AA] = NULL;
		group->subdev[ENTRY_3AC] = NULL;
		group->subdev[ENTRY_3AP] = NULL;
		group->subdev[ENTRY_ISP] = NULL;
		group->subdev[ENTRY_IXC] = NULL;
		group->subdev[ENTRY_IXP] = NULL;
		group->subdev[ENTRY_DRC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_ODC] = NULL;
		group->subdev[ENTRY_DNR] = NULL;
		group->subdev[ENTRY_SCC] = NULL;
		group->subdev[ENTRY_SCP] = NULL;

		group->subdev[ENTRY_MCS] = &device->group_mcs.leader;
		group->subdev[ENTRY_M0P] = &device->m0p;
		group->subdev[ENTRY_M1P] = &device->m1p;
		group->subdev[ENTRY_M2P] = &device->m2p;
		group->subdev[ENTRY_M3P] = &device->m3p;
		group->subdev[ENTRY_M4P] = &device->m4p;
		group->subdev[ENTRY_VRA] = &device->group_vra.leader;

		device->m0p.param_dma_ot = PARAM_MCS_OUTPUT0;
		device->m1p.param_dma_ot = PARAM_MCS_OUTPUT1;
		device->m2p.param_dma_ot = PARAM_MCS_OUTPUT2;
		device->m3p.param_dma_ot = PARAM_MCS_OUTPUT3;
		device->m4p.param_dma_ot = PARAM_MCS_OUTPUT4;
		break;
	case GROUP_SLOT_VRA:
		group->subdev[ENTRY_3AA] = NULL;
		group->subdev[ENTRY_3AC] = NULL;
		group->subdev[ENTRY_3AP] = NULL;
		group->subdev[ENTRY_ISP] = NULL;
		group->subdev[ENTRY_IXC] = NULL;
		group->subdev[ENTRY_IXP] = NULL;
		group->subdev[ENTRY_DRC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_ODC] = NULL;
		group->subdev[ENTRY_DNR] = NULL;
		group->subdev[ENTRY_SCC] = NULL;
		group->subdev[ENTRY_SCP] = NULL;
		group->subdev[ENTRY_M0P] = NULL;
		group->subdev[ENTRY_M1P] = NULL;
		group->subdev[ENTRY_M2P] = NULL;
		group->subdev[ENTRY_M3P] = NULL;
		group->subdev[ENTRY_M4P] = NULL;
		group->subdev[ENTRY_VRA] = NULL;
		break;
	default:
		probe_err("group slot(%d) is invalid", group->slot);
		BUG();
		break;
	}

	return ret;
}

int fimc_is_hw_group_open(void *group_data)
{
	int ret = 0;
	u32 group_id;
	struct fimc_is_subdev *leader;
	struct fimc_is_group *group;
	struct fimc_is_device_ischain *device;

	BUG_ON(!group_data);

	group = group_data;
	leader = &group->leader;
	device = group->device;
	group_id = group->id;

	switch (group_id) {
	case GROUP_ID_3AA0:
	case GROUP_ID_3AA1:
		leader->constraints_width = 6532;
		leader->constraints_height = 3676;
		break;
	case GROUP_ID_ISP0:
		leader->constraints_width = 5120;
		leader->constraints_height = 2704;
		break;
	case GROUP_ID_ISP1:
		leader->constraints_width = 6532;
		leader->constraints_height = 3676;
		break;
	case GROUP_ID_DIS0:
		leader->constraints_width = 5120;
		leader->constraints_height = 2704;
		break;
	case GROUP_ID_MCS0:
		leader->constraints_width = 4096;
		leader->constraints_height = 2160;
		break;
	case GROUP_ID_MCS1:
		leader->constraints_width = 6532;
		leader->constraints_height = 3676;
		break;
	case GROUP_ID_VRA0:
		leader->constraints_width = 4096;
		leader->constraints_height = 2160;
		break;
	default:
		merr("group id is invalid(%d)", group, group_id);
		break;
	}

	return ret;
}

int fimc_is_hw_camif_cfg(void *sensor_data)
{
	int ret = 0;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_flite *flite;
	struct fimc_is_device_csi *csi;
	struct fimc_is_device_ischain *ischain;
	bool is_otf = false;

	BUG_ON(!sensor_data);

	sensor = sensor_data;
	ischain = sensor->ischain;
	flite = (struct fimc_is_device_flite *)v4l2_get_subdevdata(sensor->subdev_flite);
	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
	is_otf = (ischain && test_bit(FIMC_IS_GROUP_OTF_INPUT, &ischain->group_3aa.state));

	/* always clear csis dummy */
	clear_bit(CSIS_DUMMY, &csi->state);

	switch (csi->instance) {
	case 0:
		clear_bit(FLITE_DUMMY, &flite->state);

		csi->dma_subdev[ENTRY_SSVC0] = NULL;
		csi->dma_subdev[ENTRY_SSVC1] = &sensor->ssvc1;
		csi->dma_subdev[ENTRY_SSVC2] = NULL;
		csi->dma_subdev[ENTRY_SSVC3] = NULL;
		break;
	case 2:
		if (is_otf)
			clear_bit(FLITE_DUMMY, &flite->state);
		else
			set_bit(FLITE_DUMMY, &flite->state);

		csi->dma_subdev[ENTRY_SSVC0] = NULL;
		csi->dma_subdev[ENTRY_SSVC1] = &sensor->ssvc1;
		csi->dma_subdev[ENTRY_SSVC2] = NULL;
		csi->dma_subdev[ENTRY_SSVC3] = NULL;
		break;
	case 1:
	case 3:
		set_bit(FLITE_DUMMY, &flite->state);
		break;
	default:
		merr("sensor id is invalid(%d)", sensor, csi->instance);
		break;
	}

#ifdef CONFIG_CSIS_V4_0
	/* HACK: For dual scanario in EVT0, we should use CSI DMA out */
	if (csi->instance == 2 && !is_otf) {
		struct fimc_is_device_flite backup_flite;
		struct fimc_is_device_csi backup_csi;

		minfo("For dual scanario, reconfigure front camera", sensor);

		ret = fimc_is_csi_close(sensor->subdev_csi);
		if (ret)
			merr("fimc_is_csi_close is fail(%d)", sensor, ret);

		ret = fimc_is_flite_close(sensor->subdev_flite);
		if (ret)
			merr("fimc_is_flite_close is fail(%d)", sensor, ret);

		set_bit(CSIS_DMA_ENABLE, &csi->state);
		clear_bit(FLITE_DMA_ENABLE, &flite->state);
		memcpy(&backup_flite, flite, sizeof(struct fimc_is_device_flite));
		memcpy(&backup_csi, csi, sizeof(struct fimc_is_device_csi));

		ret = fimc_is_csi_open(sensor->subdev_csi, GET_FRAMEMGR(sensor->vctx));
		if (ret)
			merr("fimc_is_csi_open is fail(%d)", sensor, ret);

		ret = fimc_is_flite_open(sensor->subdev_flite, GET_FRAMEMGR(sensor->vctx));
		if (ret)
			merr("fimc_is_flite_open is fail(%d)", sensor, ret);

		backup_csi.framemgr = csi->framemgr;
		memcpy(flite, &backup_flite, sizeof(struct fimc_is_device_flite));
		memcpy(csi, &backup_csi, sizeof(struct fimc_is_device_csi));
	}
#endif
	return ret;
}

int fimc_is_hw_camif_open(void *sensor_data)
{
	int ret = 0;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_flite *flite;
	struct fimc_is_device_csi *csi;

	BUG_ON(!sensor_data);

	sensor = sensor_data;
	flite = (struct fimc_is_device_flite *)v4l2_get_subdevdata(sensor->subdev_flite);
	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);

	switch (csi->instance) {
#ifdef CONFIG_CSIS_V4_0
	case 0:
		clear_bit(CSIS_DMA_ENABLE, &csi->state);
		set_bit(FLITE_DMA_ENABLE, &flite->state);
		break;
	case 2:
		clear_bit(CSIS_DMA_ENABLE, &csi->state);
		set_bit(FLITE_DMA_ENABLE, &flite->state);
		break;
#else
	case 0:
		set_bit(CSIS_DMA_ENABLE, &csi->state);
		clear_bit(FLITE_DMA_ENABLE, &flite->state);
		break;
	case 2:
		set_bit(CSIS_DMA_ENABLE, &csi->state);
		clear_bit(FLITE_DMA_ENABLE, &flite->state);
		break;
#endif
	case 1:
	case 3:
		set_bit(CSIS_DMA_ENABLE, &csi->state);
		clear_bit(FLITE_DMA_ENABLE, &flite->state);
		break;
	default:
		merr("sensor id is invalid(%d)", sensor, csi->instance);
		break;
	}

	return ret;
}

int fimc_is_hw_ischain_cfg(void *ischain_data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_device_ischain *device;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_csi *csi;
	int i, sensor_cnt = 0;
	void __iomem *regs;
	u32 val;

	BUG_ON(!ischain_data);

	device = (struct fimc_is_device_ischain *)ischain_data;
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		return ret;

	core = (struct fimc_is_core *)platform_get_drvdata(device->pdev);
	sensor = device->sensor;
	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);

	/* checked single/dual camera */
	for (i = 0; i < FIMC_IS_STREAM_COUNT; i++)
		if (test_bit(FIMC_IS_SENSOR_OPEN, &(core->sensor[i].state)))
			sensor_cnt++;

	/* get MCUCTL base */
	regs = device->interface->regs;
	/* get MCUCTL controller reg2 value */
	val = fimc_is_hw_get_reg(regs, &mcuctl_regs[MCUCTRLR2]);

	/* NRT setting */
	val = fimc_is_hw_set_field_value(val,
			&mcuctl_fields[MCUCTRLR2_ISPCPU_RT_INFO], 0);
	/* 1) RT setting */
	val = fimc_is_hw_set_field_value(val,
			&mcuctl_fields[MCUCTRLR2_FIMC_3AA0_RT_INFO], 1);
	/* changed ISP0 NRT NRT from RT due to using 3AA0/ISP M2M */
	val = fimc_is_hw_set_field_value(val,
			&mcuctl_fields[MCUCTRLR2_FIMC_ISP0_RT_INFO], 0);

	if (sensor_cnt > 1) {
		/*
		 * DUAL scenario
		 *
		 * 2) Pixel Sync Buf, depending on Back Camera
		 *    BNS->3AA : 1 <= if Binning Scaling was applied
		 *    3AA->ISP : 0
		 */
		if (fimc_is_sensor_g_bns_ratio(&core->sensor[0]) > 1000)
			val = fimc_is_hw_set_field_value(val,
				&mcuctl_fields[MCUCTRLR2_SHARED_BUFFER_SEL], 1);
		else
			val = fimc_is_hw_set_field_value(val,
				&mcuctl_fields[MCUCTRLR2_SHARED_BUFFER_SEL], 0);

		/*
		 * 3) BNS Input Select
		 *    CSIS0 : 0 (BACK) <= Always
		 * 4) 3AA0 Input Select
		 *    CSIS0 : 0 (BNS) <= Always
		 */
		val = fimc_is_hw_set_field_value(val,
			&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_BNS], 0);
		val = fimc_is_hw_set_field_value(val,
			&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_3AA0], 0);
	} else {
		/*
		 * SINGLE scenario
		 *
		 * 2) Pixel Sync Buf
		 *    BNS->3AA : 1 <= if Binning Scaling was applied
		 *    3AA->ISP : 0
		 */
		if (fimc_is_sensor_g_bns_ratio(device->sensor) > 1000)
			val = fimc_is_hw_set_field_value(val,
				&mcuctl_fields[MCUCTRLR2_SHARED_BUFFER_SEL], 1);
		else
			val = fimc_is_hw_set_field_value(val,
				&mcuctl_fields[MCUCTRLR2_SHARED_BUFFER_SEL], 0);

		/*
		 * 3) BNS Input Select
		 *    CSIS0 : 0 (BACK)
		 *    CSIS2 : 1 (FRONT)
		 * 4) 3AA0 Input Select
		 *    CSIS0 : 0 (BNS) <= Always
		 */
		if (csi->instance == 0) {
			val = fimc_is_hw_set_field_value(val,
				&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_BNS], 0);
			val = fimc_is_hw_set_field_value(val,
				&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_3AA0], 0);
		} else {
			val = fimc_is_hw_set_field_value(val,
				&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_BNS], 1);
			val = fimc_is_hw_set_field_value(val,
				&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_3AA0], 0);
		}
	}

	/*
	 * 5) ISP1 Input Select
	 *    3AA1 : 1 <= Always
	 */
	val = fimc_is_hw_set_field_value(val,
		&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_ISP1], 1);

	/*
	 * 7) TPU Output Select
	 *    MCS0 : 0 <= Always
	 */
	val = fimc_is_hw_set_field_value(val,
		&mcuctl_fields[MCUCTRLR2_OUT_PATH_SEL_TPU], 0);

	/*
	 * 8) TPU Input Select
	 *    ISP0 : 0 <= Always
	 */
	val = fimc_is_hw_set_field_value(val,
		&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_TPU], 0);

	/*
	 * 9) MCS1 Input Select
	 *    ISP1 : 1 <= Always
	 */
	val = fimc_is_hw_set_field_value(val,
		&mcuctl_fields[MCUCTRLR2_IN_PATH_SEL_MCS1], 1);

	minfo("MCUCTL2 MUX 0x%08X", device, val);
	fimc_is_hw_set_reg(regs, &mcuctl_regs[MCUCTRLR2], val);

	return ret;
}
