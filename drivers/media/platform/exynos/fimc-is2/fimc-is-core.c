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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>
#include <linux/bug.h>
#include <linux/v4l2-mediabus.h>
#include <linux/gpio.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-core.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-debug.h"
#include "fimc-is-hw.h"
#include "fimc-is-err.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-clk-gate.h"
#include "fimc-is-dvfs.h"
#include "include/fimc-is-module.h"
#include "fimc-is-device-sensor.h"

#ifdef ENABLE_FAULT_HANDLER
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
#include <linux/exynos_iovmm.h>
#else
#include <plat/sysmmu.h>
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define PM_QOS_CAM_THROUGHPUT	PM_QOS_RESERVED
#endif

struct device *fimc_is_dev = NULL;

extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;

/* sysfs global variable for debug */
struct fimc_is_sysfs_debug sysfs_debug;

#ifndef ENABLE_IS_CORE
/* sysfs global variable for set position to actuator */
struct fimc_is_sysfs_actuator sysfs_actuator;
#endif

#ifdef CONFIG_CPU_THERMAL_IPA
static int fimc_is_mif_throttling_notifier(struct notifier_block *nb,
		unsigned long val, void *v)
{
	struct fimc_is_core *core = NULL;
	struct fimc_is_device_sensor *device = NULL;
	int i;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is null");
		goto exit;
	}

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		if (test_bit(FIMC_IS_SENSOR_OPEN, &core->sensor[i].state)) {
			device = &core->sensor[i];
			break;
		}
	}

	if (device && !test_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state))
		/* Set DTP */
		set_bit(FIMC_IS_MIF_THROTTLING_STOP, &device->force_stop);
	else
		err("any sensor is not opened");

exit:
	err("MIF: cause of mif_throttling, mif_qos is [%lu]!!!\n", val);

	return NOTIFY_OK;
}

static struct notifier_block exynos_fimc_is_mif_throttling_nb = {
	.notifier_call = fimc_is_mif_throttling_notifier,
};
#endif

static int fimc_is_suspend(struct device *dev)
{
	pr_debug("FIMC_IS Suspend\n");
	return 0;
}

static int fimc_is_resume(struct device *dev)
{
	pr_debug("FIMC_IS Resume\n");
	return 0;
}

#ifdef ENABLE_FAULT_HANDLER
static void __fimc_is_fault_handler(struct device *dev)
{
	u32 i, j, k;
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_subdev *subdev;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_resourcemgr *resourcemgr;

	core = dev_get_drvdata(dev);
	if (core) {
		resourcemgr = &core->resourcemgr;

		fimc_is_hw_fault(&core->interface);
		/* dump FW page table 1nd(~16KB), 2nd(16KB~32KB) */
		fimc_is_hw_memdump(&core->interface,
			resourcemgr->minfo.kvaddr + FIMC_IS_TTB_OFFSET, /* TTB_BASE ~ 32KB */
			resourcemgr->minfo.kvaddr + FIMC_IS_TTB_OFFSET + FIMC_IS_TTB_SIZE);
		fimc_is_hw_logdump(&core->interface);

		/* SENSOR */
		for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
			sensor = &core->sensor[i];
			framemgr = GET_FRAMEMGR(sensor->vctx);
			if (test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state) && framemgr) {
				struct fimc_is_device_flite *flite;
				struct fimc_is_device_csi *csi;

				for (j = 0; j < framemgr->num_frames; ++j) {
					for (k = 0; k < framemgr->frames[j].planes; k++) {
						pr_err("[SS%d] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
							framemgr->frames[j].dvaddr_buffer[k],
							framemgr->frames[j].mem_state);
					}
				}

				/* vc0 */
				framemgr = GET_SUBDEV_FRAMEMGR(&sensor->ssvc0);
				if (test_bit(FIMC_IS_SUBDEV_START, &sensor->ssvc0.state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[SS%dVC0] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
									framemgr->frames[j].dvaddr_buffer[k],
									framemgr->frames[j].mem_state);
						}
					}
				}

				/* vc1 */
				framemgr = GET_SUBDEV_FRAMEMGR(&sensor->ssvc1);
				if (test_bit(FIMC_IS_SUBDEV_START, &sensor->ssvc1.state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[SS%dVC1] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
									framemgr->frames[j].dvaddr_buffer[k],
									framemgr->frames[j].mem_state);
						}
					}
				}

				/* vc2 */
				framemgr = GET_SUBDEV_FRAMEMGR(&sensor->ssvc2);
				if (test_bit(FIMC_IS_SUBDEV_START, &sensor->ssvc2.state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[SS%dVC2] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
									framemgr->frames[j].dvaddr_buffer[k],
									framemgr->frames[j].mem_state);
						}
					}
				}

				/* vc3 */
				framemgr = GET_SUBDEV_FRAMEMGR(&sensor->ssvc3);
				if (test_bit(FIMC_IS_SUBDEV_START, &sensor->ssvc3.state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[SS%dVC3] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
									framemgr->frames[j].dvaddr_buffer[k],
									framemgr->frames[j].mem_state);
						}
					}
				}

				/* csis, bns sfr dump */
				flite = (struct fimc_is_device_flite *)v4l2_get_subdevdata(sensor->subdev_flite);
				if (flite)
					flite_hw_dump(flite->base_reg);

				csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
				if (csi)
					csi_hw_dump(csi->base_reg);
			}
		}

		/* ISCHAIN */
		for (i = 0; i < FIMC_IS_STREAM_COUNT; i++) {
			ischain = &core->ischain[i];
			if (test_bit(FIMC_IS_ISCHAIN_OPEN, &ischain->state)) {
				/* 3AA */
				subdev = &ischain->group_3aa.leader;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][3XS] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* 3AAC */
				subdev = &ischain->txc;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][3XC] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* 3AAP */
				subdev = &ischain->txp;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][3XP] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* ISP */
				subdev = &ischain->group_isp.leader;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][IXS] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* ISPC */
				subdev = &ischain->ixc;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][IXC] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* ISPP */
				subdev = &ischain->ixp;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][IXP] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* DIS */
				subdev = &ischain->group_dis.leader;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][DIS] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* SCC */
				subdev = &ischain->scc;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][SCC] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* SCP */
				subdev = &ischain->scp;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][SCP] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* MCS */
				subdev = &ischain->group_mcs.leader;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][MCS] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* M0P */
				subdev = &ischain->m0p;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][M0P] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* M1P */
				subdev = &ischain->m1p;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][M1P] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* M2P */
				subdev = &ischain->m2p;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][M2P] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* M3P */
				subdev = &ischain->m3p;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][M3P] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* M4P */
				subdev = &ischain->m4p;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][M4P] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
				/* VRA */
				subdev = &ischain->group_vra.leader;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < framemgr->num_frames; ++j) {
						for (k = 0; k < framemgr->frames[j].planes; k++) {
							pr_err("[%d][VRA] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frames[j].dvaddr_buffer[k],
								framemgr->frames[j].mem_state);
						}
					}
				}
			}
		}
	} else {
		pr_err("failed to get core\n");
	}
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0))
#define SECT_ORDER 20
#define LPAGE_ORDER 16
#define SPAGE_ORDER 12

#define lv1ent_page(sent) ((*(sent) & 3) == 1)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)
#define lv2table_base(sent) (*(sent) & 0xFFFFFC00)

static unsigned long *section_entry(unsigned long *pgtable, unsigned long iova)
{
	return pgtable + lv1ent_offset(iova);
}

static unsigned long *page_entry(unsigned long *sent, unsigned long iova)
{
	return (unsigned long *)__va(lv2table_base(sent)) + lv2ent_offset(iova);
}

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNKNOWN FAULT"
};

static int fimc_is_fault_handler(struct device *dev, const char *mmuname,
					enum exynos_sysmmu_inttype itype,
					unsigned long pgtable_base,
					unsigned long fault_addr)
{
	unsigned long *ent;

	if ((itype >= SYSMMU_FAULTS_NUM) || (itype < SYSMMU_PAGEFAULT))
		itype = SYSMMU_FAULT_UNKNOWN;

	pr_err("%s occured at 0x%lx by '%s'(Page table base: 0x%lx)\n",
		sysmmu_fault_name[itype], fault_addr, mmuname, pgtable_base);

	ent = section_entry(__va(pgtable_base), fault_addr);
	pr_err("\tLv1 entry: 0x%lx\n", *ent);

	if (lv1ent_page(ent)) {
		ent = page_entry(ent, fault_addr);
		pr_err("\t Lv2 entry: 0x%lx\n", *ent);
	}

	__fimc_is_fault_handler(dev);

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");

	BUG();

	return 0;
}
#else
static int __attribute__((unused)) fimc_is_fault_handler(struct iommu_domain *domain,
	struct device *dev,
	unsigned long fault_addr,
	int fault_flag,
	void *token)
{
	pr_err("<FIMC-IS FAULT HANDLER>\n");
	pr_err("Device virtual(0x%X) is invalid access\n", (u32)fault_addr);

	__fimc_is_fault_handler(dev);

	return -EINVAL;
}
#endif
#endif /* ENABLE_FAULT_HANDLER */

static ssize_t show_clk_gate_mode(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.clk_gate_mode);
}

static ssize_t store_clk_gate_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef HAS_FW_CLOCK_GATE
	switch (buf[0]) {
	case '0':
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_HOST;
		break;
	case '1':
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_FW;
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}
#endif
	return count;
}

static ssize_t show_en_clk_gate(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_clk_gate);
}

static ssize_t store_en_clk_gate(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef ENABLE_CLOCK_GATE
	switch (buf[0]) {
	case '0':
		sysfs_debug.en_clk_gate = false;
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_HOST;
		break;
	case '1':
		sysfs_debug.en_clk_gate = true;
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_HOST;
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}
#endif
	return count;
}

static ssize_t show_en_dvfs(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_dvfs);
}

static ssize_t store_en_dvfs(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef ENABLE_DVFS
	struct fimc_is_core *core =
		(struct fimc_is_core *)platform_get_drvdata(to_platform_device(dev));
	struct fimc_is_resourcemgr *resourcemgr;
	int i;

	BUG_ON(!core);

	resourcemgr = &core->resourcemgr;

	switch (buf[0]) {
	case '0':
		sysfs_debug.en_dvfs = false;
		/* update dvfs lever to max */
		mutex_lock(&resourcemgr->dvfs_ctrl.lock);
		for (i = 0; i < FIMC_IS_STREAM_COUNT; i++) {
			if (test_bit(FIMC_IS_ISCHAIN_OPEN, &((core->ischain[i]).state)))
				fimc_is_set_dvfs(core, &(core->ischain[i]), FIMC_IS_SN_MAX);
		}
		fimc_is_dvfs_init(resourcemgr);
		resourcemgr->dvfs_ctrl.static_ctrl->cur_scenario_id = FIMC_IS_SN_MAX;
		mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
		break;
	case '1':
		/* It can not re-define static scenario */
		sysfs_debug.en_dvfs = true;
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}
#endif
	return count;
}

#ifndef ENABLE_IS_CORE
static ssize_t store_actuator_init_step(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret_count;
	unsigned int init_step;

	ret_count = sscanf(buf, "%u", &init_step);
	if (ret_count != 1)
		return -EINVAL;

	switch (init_step) {
		/* case number is step of set position */
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			sysfs_actuator.init_step = init_step;
			break;
		/* default actuator setting (2step default) */
		default:
			sysfs_actuator.init_step = 0;
			break;
	}

	return count;
}

static ssize_t store_actuator_init_positions(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int i;
	int ret_count;
	unsigned int init_positions[INIT_MAX_SETTING];

	ret_count = sscanf(buf, "%u %u %u %u %u", &init_positions[0], &init_positions[1],
						&init_positions[2], &init_positions[3], &init_positions[4]);
	if (ret_count > INIT_MAX_SETTING)
		return -EINVAL;

	for (i = 0; i < ret_count; i++) {
		if (init_positions[i] >= 0 && init_positions[i] < 1024)
			sysfs_actuator.init_positions[i] = init_positions[i];
		else
			sysfs_actuator.init_positions[i] = 0;
	}

	return count;
}

static ssize_t store_actuator_init_delays(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret_count;
	int i;
	unsigned int init_delays[INIT_MAX_SETTING];

	ret_count = sscanf(buf, "%u %u %u %u %u", &init_delays[0], &init_delays[1],
							&init_delays[2], &init_delays[3], &init_delays[4]);
	if (ret_count > INIT_MAX_SETTING)
		return -EINVAL;

	for (i = 0; i < ret_count; i++) {
		if (init_delays[i] >= 0)
			sysfs_actuator.init_delays[i] = init_delays[i];
		else
			sysfs_actuator.init_delays[i] = 0;
	}

	return count;
}

static ssize_t show_actuator_init_step(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_actuator.init_step);
}

static ssize_t show_actuator_init_positions(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d %d\n", sysfs_actuator.init_positions[0],
						sysfs_actuator.init_positions[1], sysfs_actuator.init_positions[2],
						sysfs_actuator.init_positions[3], sysfs_actuator.init_positions[4]);
}

static ssize_t show_actuator_init_delays(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d %d\n", sysfs_actuator.init_delays[0],
							sysfs_actuator.init_delays[1], sysfs_actuator.init_delays[2],
							sysfs_actuator.init_delays[3], sysfs_actuator.init_delays[4]);
}

static DEVICE_ATTR(init_step, 0644, show_actuator_init_step, store_actuator_init_step);
static DEVICE_ATTR(init_positions, 0644, show_actuator_init_positions, store_actuator_init_positions);
static DEVICE_ATTR(init_delays, 0644, show_actuator_init_delays, store_actuator_init_delays);
#endif

static DEVICE_ATTR(en_clk_gate, 0644, show_en_clk_gate, store_en_clk_gate);
static DEVICE_ATTR(clk_gate_mode, 0644, show_clk_gate_mode, store_clk_gate_mode);
static DEVICE_ATTR(en_dvfs, 0644, show_en_dvfs, store_en_dvfs);

static struct attribute *fimc_is_debug_entries[] = {
	&dev_attr_en_clk_gate.attr,
	&dev_attr_clk_gate_mode.attr,
	&dev_attr_en_dvfs.attr,
#ifndef ENABLE_IS_CORE
	&dev_attr_init_step.attr,
	&dev_attr_init_positions.attr,
	&dev_attr_init_delays.attr,
#endif
	NULL,
};
static struct attribute_group fimc_is_debug_attr_group = {
	.name	= "debug",
	.attrs	= fimc_is_debug_entries,
};

static int fimc_is_probe(struct platform_device *pdev)
{
	struct exynos_platform_fimc_is *pdata;
#if defined (ENABLE_IS_CORE) || defined (USE_MCUCTL)
	struct resource *mem_res;
	struct resource *regs_res;
#endif
	struct fimc_is_core *core;
	int ret = -ENODEV;
#ifndef ENABLE_IS_CORE
	int i;
#endif
	u32 stream;
	struct pinctrl_state *s;

	probe_info("%s:start(%ld, %ld)\n", __func__,
		sizeof(struct fimc_is_core), sizeof(struct fimc_is_video_ctx));

	core = kzalloc(sizeof(struct fimc_is_core), GFP_KERNEL);
	if (!core) {
		probe_err("core is NULL");
		return -ENOMEM;
	}

	fimc_is_dev = &pdev->dev;
	dev_set_drvdata(fimc_is_dev, core);

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
#ifdef CONFIG_OF
		ret = fimc_is_parse_dt(pdev);
		if (ret) {
			err("fimc_is_parse_dt is fail(%d)", ret);
			return ret;
		}

		pdata = dev_get_platdata(&pdev->dev);
#else
		BUG();
#endif
	}

#ifdef USE_ION_ALLOC
	core->fimc_ion_client = ion_client_create(ion_exynos, "fimc-is");
#endif
	core->pdev = pdev;
	core->pdata = pdata;
	core->current_position = SENSOR_POSITION_REAR;
	device_init_wakeup(&pdev->dev, true);

	/* for mideaserver force down */
	atomic_set(&core->rsccount, 0);

#if defined (ENABLE_IS_CORE) || defined (USE_MCUCTL)
	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		probe_err("Failed to get io memory region(%p)", mem_res);
		goto p_err1;
	}

	regs_res = request_mem_region(mem_res->start, resource_size(mem_res), pdev->name);
	if (!regs_res) {
		probe_err("Failed to request io memory region(%p)", regs_res);
		goto p_err1;
	}

	core->regs_res = regs_res;
	core->regs =  ioremap_nocache(mem_res->start, resource_size(mem_res));
	if (!core->regs) {
		probe_err("Failed to remap io region(%p)", core->regs);
		goto p_err2;
	}
#else
	core->regs_res = NULL;
	core->regs = NULL;
#endif

#ifdef ENABLE_IS_CORE
	core->irq = platform_get_irq(pdev, 0);
	if (core->irq < 0) {
		probe_err("Failed to get irq(%d)", core->irq);
		goto p_err3;
	}
#endif
	ret = pdata->clk_get(&pdev->dev);
	if (ret) {
		probe_err("clk_get is fail(%d)", ret);
		goto p_err3;
	}

	ret = fimc_is_mem_probe(&core->resourcemgr.mem, core->pdev);
	if (ret) {
		probe_err("fimc_is_mem_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = fimc_is_resourcemgr_probe(&core->resourcemgr, core);
	if (ret) {
		probe_err("fimc_is_resourcemgr_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = fimc_is_interface_probe(&core->interface,
		&core->resourcemgr.minfo,
		(ulong)core->regs,
		core->irq,
		core);
	if (ret) {
		probe_err("fimc_is_interface_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = fimc_is_debug_probe();
	if (ret) {
		probe_err("fimc_is_deubg_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = fimc_is_vender_probe(&core->vender);
	if (ret) {
		probe_err("fimc_is_vender_probe is fail(%d)", ret);
		goto p_err3;
	}

	/* group initialization */
	ret = fimc_is_groupmgr_probe(&core->groupmgr);
	if (ret) {
		probe_err("fimc_is_groupmgr_probe is fail(%d)", ret);
		goto p_err3;
	}

	for (stream = 0; stream < FIMC_IS_STREAM_COUNT; ++stream) {
		ret = fimc_is_ischain_probe(&core->ischain[stream],
			&core->interface,
			&core->resourcemgr,
			&core->groupmgr,
			&core->resourcemgr.mem,
			core->pdev,
			stream);
		if (ret) {
			probe_err("fimc_is_ischain_probe(%d) is fail(%d)", stream, ret);
			goto p_err3;
		}

#ifndef ENABLE_IS_CORE
		core->ischain[stream].hardware = &core->hardware;
#endif
	}

	ret = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fimc-is v4l2 device\n");
		goto p_err3;
	}

#ifdef SOC_30S
	/* video entity - 3a0 */
	fimc_is_30s_video_probe(core);
#endif

#ifdef SOC_30C
	/* video entity - 3a0 capture */
	fimc_is_30c_video_probe(core);
#endif

#ifdef SOC_30P
	/* video entity - 3a0 preview */
	fimc_is_30p_video_probe(core);
#endif

#ifdef SOC_31S
	/* video entity - 3a1 */
	fimc_is_31s_video_probe(core);
#endif

#ifdef SOC_31C
	/* video entity - 3a1 capture */
	fimc_is_31c_video_probe(core);
#endif

#ifdef SOC_31P
	/* video entity - 3a1 preview */
	fimc_is_31p_video_probe(core);
#endif

#ifdef SOC_I0S
	/* video entity - isp0 */
	fimc_is_i0s_video_probe(core);
#endif

#ifdef SOC_I0C
	/* video entity - isp0 capture */
	fimc_is_i0c_video_probe(core);
#endif

#ifdef SOC_I0P
	/* video entity - isp0 preview */
	fimc_is_i0p_video_probe(core);
#endif

#ifdef SOC_I1S
	/* video entity - isp1 */
	fimc_is_i1s_video_probe(core);
#endif

#ifdef SOC_I1C
	/* video entity - isp1 capture */
	fimc_is_i1c_video_probe(core);
#endif

#ifdef SOC_I1P
	/* video entity - isp1 preview */
	fimc_is_i1p_video_probe(core);
#endif

#ifdef SOC_DIS
	/* video entity - dis */
	fimc_is_dis_video_probe(core);
#endif

#ifdef SOC_SCC
	/* video entity - scc */
	fimc_is_scc_video_probe(core);
#endif

#ifdef SOC_SCP
	/* video entity - scp */
	fimc_is_scp_video_probe(core);
#endif

#ifdef SOC_MCS
	/* video entity - scp */
	fimc_is_m0s_video_probe(core);
	fimc_is_m1s_video_probe(core);
	fimc_is_m0p_video_probe(core);
	fimc_is_m1p_video_probe(core);
	fimc_is_m2p_video_probe(core);
	fimc_is_m3p_video_probe(core);
	fimc_is_m4p_video_probe(core);
#endif

	platform_set_drvdata(pdev, core);

#ifndef ENABLE_IS_CORE
	ret = fimc_is_interface_ischain_probe(&core->interface_ischain,
		&core->hardware,
		&core->resourcemgr,
		core->pdev,
		(ulong)core->regs);
	if (ret) {
		dev_err(&pdev->dev, "interface_ischain_probe fail\n");
		goto p_err1;
	}

	ret = fimc_is_hardware_probe(&core->hardware, &core->interface, &core->interface_ischain);
	if (ret) {
		dev_err(&pdev->dev, "hardware_probe fail\n");
		goto p_err1;
	}

	/* set sysfs for set position to actuator */
	sysfs_actuator.init_step = 0;
	for (i = 0; i < INIT_MAX_SETTING; i++) {
		sysfs_actuator.init_positions[i] = -1;
		sysfs_actuator.init_delays[i] = -1;
	}
#endif

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->resourcemgr.mem.alloc_ctx)
		vb2_ion_attach_iommu(core->resourcemgr.mem.alloc_ctx);
#endif
#endif

	EXYNOS_MIF_ADD_NOTIFIER(&exynos_fimc_is_mif_throttling_nb);

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

#ifdef ENABLE_FAULT_HANDLER
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0))
	exynos_sysmmu_set_fault_handler(fimc_is_dev, fimc_is_fault_handler);
#else
	iovmm_set_fault_handler(fimc_is_dev, fimc_is_fault_handler, NULL);
#endif
#endif

	/* set sysfs for debuging */
	sysfs_debug.en_clk_gate = 0;
	sysfs_debug.en_dvfs = 1;
#ifdef ENABLE_CLOCK_GATE
	sysfs_debug.en_clk_gate = 1;
#ifdef HAS_FW_CLOCK_GATE
	sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_FW;
#else
	sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_HOST;
#endif
#endif
	ret = sysfs_create_group(&core->pdev->dev.kobj, &fimc_is_debug_attr_group);

	s = pinctrl_lookup_state(pdata->pinctrl, "release");

	if (pinctrl_select_state(pdata->pinctrl, s) < 0) {
		probe_err("pinctrl_select_state is fail\n");
		goto p_err3;
	}

	probe_info("%s:end\n", __func__);
	return 0;

p_err3:
	iounmap(core->regs);
#if defined (ENABLE_IS_CORE) || defined (USE_MCUCTL)
p_err2:
	release_mem_region(regs_res->start, resource_size(regs_res));
#endif
p_err1:
	kfree(core);
	return ret;
}

static int fimc_is_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	.suspend		= fimc_is_suspend,
	.resume			= fimc_is_resume,
	.runtime_suspend	= fimc_is_ischain_runtime_suspend,
	.runtime_resume		= fimc_is_ischain_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_match);

static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove		= fimc_is_remove,
	.driver = {
		.name	= FIMC_IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
		.of_match_table = exynos_fimc_is_match,
	}
};

#else
static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove 	= __devexit_p(fimc_is_remove),
	.driver = {
		.name	= FIMC_IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
	}
};
#endif

static int __init fimc_is_init(void)
{
	int ret = platform_driver_register(&fimc_is_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}
device_initcall(fimc_is_init);

static void __exit fimc_is_exit(void)
{
	platform_driver_unregister(&fimc_is_driver);
}
module_exit(fimc_is_exit);

MODULE_AUTHOR("Gilyeon im<kilyeon.im@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS2 driver");
MODULE_LICENSE("GPL");
