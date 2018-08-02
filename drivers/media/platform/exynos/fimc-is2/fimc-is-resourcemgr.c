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
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <soc/samsung/tmu.h>

#ifdef CONFIG_EXYNOS_BUSMONITOR
#include <linux/exynos-busmon.h>
#endif

#include "fimc-is-resourcemgr.h"
#include "fimc-is-hw.h"
#include "fimc-is-debug.h"
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-clk-gate.h"
#if !defined(ENABLE_IS_CORE)
#include "fimc-is-interface-library.h"
#endif
#include <linux/memblock.h>
#include <asm/map.h>

#define CLUSTER_MIN_MASK			0x0000FFFF
#define CLUSTER_MIN_SHIFT			0
#define CLUSTER_MAX_MASK			0xFFFF0000
#define CLUSTER_MAX_SHIFT			16

struct pm_qos_request exynos_isp_qos_int;
struct pm_qos_request exynos_isp_qos_mem;
struct pm_qos_request exynos_isp_qos_cam;
struct pm_qos_request exynos_isp_qos_disp;
struct pm_qos_request exynos_isp_qos_hpg;
struct pm_qos_request exynos_isp_qos_cluster0_min;
struct pm_qos_request exynos_isp_qos_cluster0_max;
struct pm_qos_request exynos_isp_qos_cluster1_min;
struct pm_qos_request exynos_isp_qos_cluster1_max;

#define C0MIN_QOS_ADD(freq) pm_qos_add_request(&exynos_isp_qos_cluster0_min, PM_QOS_CLUSTER0_FREQ_MIN, freq * 1000)
#define C0MIN_QOS_DEL() pm_qos_remove_request(&exynos_isp_qos_cluster0_min)
#define C0MIN_QOS_UPDATE(freq) pm_qos_update_request(&exynos_isp_qos_cluster0_min, freq * 1000)
#define C0MAX_QOS_ADD(freq) pm_qos_add_request(&exynos_isp_qos_cluster0_max, PM_QOS_CLUSTER0_FREQ_MAX, freq * 1000)
#define C0MAX_QOS_DEL() pm_qos_remove_request(&exynos_isp_qos_cluster0_max)
#define C0MAX_QOS_UPDATE(freq) pm_qos_update_request(&exynos_isp_qos_cluster0_max, freq * 1000)
#define C1MIN_QOS_ADD(freq) pm_qos_add_request(&exynos_isp_qos_cluster1_min, PM_QOS_CLUSTER1_FREQ_MIN, freq * 1000)
#define C1MIN_QOS_DEL() pm_qos_remove_request(&exynos_isp_qos_cluster1_min)
#define C1MIN_QOS_UPDATE(freq) pm_qos_update_request(&exynos_isp_qos_cluster1_min, freq * 1000)
#define C1MAX_QOS_ADD(freq) pm_qos_add_request(&exynos_isp_qos_cluster1_max, PM_QOS_CLUSTER1_FREQ_MAX, freq * 1000)
#define C1MAX_QOS_DEL() pm_qos_remove_request(&exynos_isp_qos_cluster1_max)
#define C1MAX_QOS_UPDATE(freq) pm_qos_update_request(&exynos_isp_qos_cluster1_max, freq * 1000)

extern struct fimc_is_sysfs_debug sysfs_debug;
extern int fimc_is_sensor_runtime_suspend(struct device *dev);
extern int fimc_is_sensor_runtime_resume(struct device *dev);

#ifdef ENABLE_IS_CORE
static int fimc_is_resourcemgr_allocmem(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	void *fw_cookie;
	size_t fw_size = FIMC_IS_A5_MEM_SIZE;
#ifdef FW_SUSPEND_RESUME
	fw_size += FIMC_IS_BACKUP_SIZE;
#endif
#ifdef ENABLE_ODC
	fw_size += SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF;
#endif
#ifdef ENABLE_VDIS
	fw_size += SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF;
#endif
#ifdef ENABLE_DNR
	fw_size += SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF;
#endif
#ifdef ENABLE_FD_SW
	fw_size += SIZE_LHFD_INTERNEL_BUF * NUM_LHFD_INTERNAL_BUF;
#endif
#ifdef ENABLE_FD_DMA_INPUT
	fw_size += SIZE_LHFD_SHOT_BUF * MAX_LHFD_SHOT_BUF;
#endif

	fw_size = PAGE_ALIGN(fw_size);

	dbg_core("Allocating memory for FIMC-IS firmware.\n");

	fw_cookie = vb2_ion_private_alloc(resourcemgr->mem.alloc_ctx, fw_size);
	if (IS_ERR(fw_cookie)) {
		err("Allocating bitprocessor buffer failed");
		fw_cookie = NULL;
		ret = -ENOMEM;
		goto p_err;
	}

	ret = vb2_ion_dma_address(fw_cookie, &resourcemgr->minfo.dvaddr);
	if ((ret < 0) || (resourcemgr->minfo.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.");
		vb2_ion_private_free(fw_cookie);
		resourcemgr->minfo.dvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto p_err;
	}

#ifdef PRINT_BUFADDR
	info("[RSC] daddr = %pa, size = %08X\n", &resourcemgr->minfo.dvaddr, FIMC_IS_A5_MEM_SIZE);
#endif

	resourcemgr->minfo.kvaddr = (ulong)vb2_ion_private_vaddr(fw_cookie);
	if (IS_ERR((void *)resourcemgr->minfo.kvaddr)) {
		err("Bitprocessor memory remap failed");
		vb2_ion_private_free(fw_cookie);
		resourcemgr->minfo.kvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto p_err;
	}

	vb2_ion_sync_for_device(fw_cookie, 0, fw_size, DMA_BIDIRECTIONAL);

p_err:
	info("[RSC] Device virtual for internal: %08lx\n", resourcemgr->minfo.kvaddr);
	resourcemgr->minfo.fw_cookie = fw_cookie;

	return ret;
}

static int fimc_is_resourcemgr_initmem(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
#ifdef ENABLE_FD_SW
	int num_buf = 0;
#endif
	u32 offset;

	dbg_core("fimc_is_init_mem - ION\n");

	ret = fimc_is_resourcemgr_allocmem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		ret = -ENOMEM;
		goto p_err;
	}

	offset = FIMC_IS_SHARED_OFFSET;
	resourcemgr->minfo.dvaddr_fshared = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_fshared = resourcemgr->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
	resourcemgr->minfo.dvaddr_region = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_region = resourcemgr->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE;
#ifdef FW_SUSPEND_RESUME
	offset += FIMC_IS_BACKUP_SIZE;
#endif

#ifdef ENABLE_ODC
	resourcemgr->minfo.dvaddr_odc = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_odc = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_odc = 0;
	resourcemgr->minfo.kvaddr_odc = 0;
#endif

#ifdef ENABLE_VDIS
	resourcemgr->minfo.dvaddr_dis = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_dis = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_dis = 0;
	resourcemgr->minfo.kvaddr_dis = 0;
#endif

#ifdef ENABLE_DNR
	resourcemgr->minfo.dvaddr_3dnr = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_3dnr = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_3dnr = 0;
	resourcemgr->minfo.kvaddr_3dnr = 0;
#endif

#ifdef ENABLE_FD_SW
	for (num_buf = 0; num_buf < NUM_LHFD_INTERNAL_BUF; num_buf++) {
		resourcemgr->minfo.dvaddr_fd[num_buf] = resourcemgr->minfo.dvaddr + offset;
		resourcemgr->minfo.kvaddr_fd[num_buf] = resourcemgr->minfo.kvaddr + offset;
		offset += (SIZE_LHFD_INTERNEL_BUF);
	}
#else
	memset(&resourcemgr->minfo.dvaddr_fd, 0, sizeof(resourcemgr->minfo.dvaddr_fd));
	memset(&resourcemgr->minfo.kvaddr_fd, 0, sizeof(resourcemgr->minfo.dvaddr_fd));
#endif

	dbg_core("fimc_is_init_mem done\n");

p_err:
	return ret;
}
#else /* #ifdef ENABLE_IS_CORE */
static int fimc_is_resourcemgr_allocmem(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	void *fw_cookie;
	size_t fw_size = FIMC_IS_A5_MEM_SIZE;
#ifdef ENABLE_RESERVED_INTERNAL_DMA
	fw_size += FIMC_IS_THUMBNAIL_SDMA_SIZE + FIMC_IS_GUARD_SIZE;
#if defined (ENABLE_FD_SW)
	fw_size += FIMC_IS_LHFD_DMA_SIZE + FIMC_IS_GUARD_SIZE;
#elif defined (ENABLE_VRA)
	fw_size += FIMC_IS_VRA_DMA_SIZE + FIMC_IS_GUARD_SIZE;
#endif
	fw_size += FIMC_IS_RESERVE_SIZE + FIMC_IS_GUARD_SIZE;
#endif

#ifdef ENABLE_ODC
	fw_size += SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF;
#endif
#ifdef ENABLE_VDIS
	fw_size += SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF;
#endif
#ifdef ENABLE_DNR
	fw_size += SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF;
#endif

	probe_info("[RSC] Internal memory size : %08lx\n", fw_size);
	fw_size = PAGE_ALIGN(fw_size);
	probe_info("[RSC] Internal memory size (aligned) : %08lx\n", fw_size);

	fw_cookie = vb2_ion_private_alloc(resourcemgr->mem.alloc_ctx, fw_size);
	if (IS_ERR(fw_cookie)) {
		err("Allocating bitprocessor buffer failed");
		fw_cookie = NULL;
		ret = -ENOMEM;
		goto p_err;
	}

	ret = vb2_ion_dma_address(fw_cookie, &resourcemgr->minfo.dvaddr);
	if ((ret < 0) || (resourcemgr->minfo.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.");
		vb2_ion_private_free(fw_cookie);
		resourcemgr->minfo.dvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto p_err;
	}

#ifdef PRINT_BUFADDR
	info("[RSC] daddr = %pa, size = %08X\n", &resourcemgr->minfo.dvaddr, FIMC_IS_A5_MEM_SIZE);
#endif

	resourcemgr->minfo.kvaddr = (ulong)vb2_ion_private_vaddr(fw_cookie);
	if (IS_ERR((void *)resourcemgr->minfo.kvaddr)) {
		err("Bitprocessor memory remap failed");
		vb2_ion_private_free(fw_cookie);
		resourcemgr->minfo.kvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto p_err;
	}

	vb2_ion_sync_for_device(fw_cookie, 0, fw_size, DMA_BIDIRECTIONAL);

p_err:
	probe_info("[RSC] Kernel virtual for internal: %08lx\n", resourcemgr->minfo.kvaddr);
	probe_info("[RSC] Device virtual for internal: %08lx\n", (ulong)resourcemgr->minfo.dvaddr);
	resourcemgr->minfo.fw_cookie = fw_cookie;

	return ret;
}

static int fimc_is_resourcemgr_initmem(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
#ifdef ENABLE_FD_SW
	int num_buf = 0;
#endif
	u32 offset;

	dbg_core("fimc_is_init_mem - ION\n");

	ret = fimc_is_resourcemgr_allocmem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		ret = -ENOMEM;
		goto p_err;
	}

	offset = FIMC_IS_SHARED_REGION_OFFSET;
	resourcemgr->minfo.dvaddr_fshared = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_fshared = resourcemgr->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
	resourcemgr->minfo.dvaddr_region = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_region = resourcemgr->minfo.kvaddr + offset;

	offset = FIMC_IS_RESERVE_OFFSET + FIMC_IS_GUARD_SIZE;
#ifdef ENABLE_ODC
	resourcemgr->minfo.dvaddr_odc = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_odc = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_odc = 0;
	resourcemgr->minfo.kvaddr_odc = 0;
#endif

#ifdef ENABLE_VDIS
	resourcemgr->minfo.dvaddr_dis = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_dis = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_dis = 0;
	resourcemgr->minfo.kvaddr_dis = 0;
#endif

#ifdef ENABLE_DNR
	resourcemgr->minfo.dvaddr_3dnr = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_3dnr = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_3dnr = 0;
	resourcemgr->minfo.kvaddr_3dnr = 0;
#endif
#if defined(ENABLE_FD_SW) && defined(ENABLE_RESERVED_INTERNAL_DMA)
	offset = FIMC_IS_LHFD_MAP_OFFSET;
	for (num_buf = 0; num_buf < NUM_LHFD_INTERNAL_BUF; num_buf++) {
		resourcemgr->minfo.dvaddr_fd[num_buf] = resourcemgr->minfo.dvaddr + offset;
		resourcemgr->minfo.kvaddr_fd[num_buf] = resourcemgr->minfo.kvaddr + offset;
		offset += (SIZE_LHFD_INTERNEL_BUF);
	}
#elif defined(ENABLE_VRA) && defined(ENABLE_RESERVED_INTERNAL_DMA)
	offset = FIMC_IS_VRA_DMA_OFFSET;
	resourcemgr->minfo.dvaddr_vra = resourcemgr->minfo.dvaddr_vra + offset;
	resourcemgr->minfo.kvaddr_vra = resourcemgr->minfo.kvaddr_vra + offset;
	offset += SIZE_VRA_INTERNEL_BUF;
#else
	memset(&resourcemgr->minfo.dvaddr_fd, 0, sizeof(resourcemgr->minfo.dvaddr_fd));
	memset(&resourcemgr->minfo.kvaddr_fd, 0, sizeof(resourcemgr->minfo.dvaddr_fd));
#endif

	dbg_core("fimc_is_init_mem done\n");

p_err:
	return ret;
}

static int __init fimc_is_init_static_mem(char *str)
{
	struct fimc_is_static_mem static_mem[] = {
		{0, FIMC_IS_SDK_LIB_ADDR, FIMC_IS_SDK_LIB_SIZE},
		{0, FIMC_IS_VRA_LIB_ADDR, FIMC_IS_VRA_LIB_SIZE}};
	struct map_desc fimc_iodesc[ARRAY_SIZE(static_mem)];
	u32 i;

	for (i = 0; i < ARRAY_SIZE(static_mem); i++) {
		static_mem[i].paddr = memblock_alloc(static_mem[i].size, SZ_4K);

		fimc_iodesc[i].virtual = static_mem[i].vaddr;
		fimc_iodesc[i].pfn = __phys_to_pfn(static_mem[i].paddr);
		fimc_iodesc[i].length = static_mem[i].size;
		fimc_iodesc[i].type = MT_MEMORY;
	}
	iotable_init_exec(fimc_iodesc, ARRAY_SIZE(static_mem));

	return 0;
}
__setup("reserve-fimc=", fimc_is_init_static_mem);
#endif

#ifndef ENABLE_RESERVED_MEM
static int fimc_is_resourcemgr_deinitmem(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;

	vb2_ion_private_free(resourcemgr->minfo.fw_cookie);

	return ret;
}
#endif

static int fimc_is_tmu_notifier(struct notifier_block *nb,
	unsigned long state, void *data)
{
#ifdef CONFIG_EXYNOS_THERMAL
	int ret = 0;
	struct fimc_is_resourcemgr *resourcemgr;
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	char *cooling_device_name = "ISP";
#endif
	resourcemgr = container_of(nb, struct fimc_is_resourcemgr, tmu_notifier);

	switch (state) {
	case ISP_NORMAL:
		resourcemgr->tmu_state = ISP_NORMAL;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_COLD:
		resourcemgr->tmu_state = ISP_COLD;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_THROTTLING1:
		resourcemgr->tmu_state = ISP_THROTTLING1;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_THROTTLING2:
		resourcemgr->tmu_state = ISP_THROTTLING2;
		resourcemgr->limited_fps = 0;
		warn("[RSC] THROTTLING2 : Unlimited FPS");
		break;
	case ISP_THROTTLING3:
		resourcemgr->tmu_state = ISP_THROTTLING3;
		resourcemgr->limited_fps = 15;
		warn("[RSC] THROTTLING3 : Limited 15FPS");
		break;
	case ISP_THROTTLING4:
		resourcemgr->tmu_state = ISP_THROTTLING4;
		resourcemgr->limited_fps = 5;
		warn("[RSC] THROTTLING4 : Limited 5FPS");
		break;
	case ISP_TRIPPING:
		resourcemgr->tmu_state = ISP_TRIPPING;
		resourcemgr->limited_fps = 5;
		warn("[RSC] THROTTLING5 : Limited 5FPS");
		break;
	default:
		err("[RSC] invalid tmu state(%ld)", state);
		break;
	}

#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	exynos_ss_thermal(NULL, 0, cooling_device_name, resourcemgr->limited_fps);
#endif
	return ret;
#else
	return 0;
#endif
}

static int fimc_is_bmu_notifier(struct notifier_block *nb,
	unsigned long state, void *data)
{
#ifdef CONFIG_EXYNOS_BUSMONITOR
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_resourcemgr *resourcemgr;
	struct busmon_notifier *busmon;
	bool found = false;

	resourcemgr = container_of(nb, struct fimc_is_resourcemgr, bmu_notifier);
	core = container_of(resourcemgr, struct fimc_is_core, resourcemgr);
	busmon = (struct busmon_notifier *)data;

	if (strstr((char *)busmon->init_desc, "CAM") || strstr((char *)busmon->init_desc, "ISP"))
		found = true;
	else if (strstr((char *)busmon->target_desc, "CAM") || strstr((char *)busmon->target_desc, "ISP"))
		found = true;
	else if (strstr((char *)busmon->masterip_desc, "CAM") || strstr((char *)busmon->masterip_desc, "ISP"))
		found = true;
	else
		found = false;

	if (!found)
		goto p_err;

	info("1. NOC info\n");
	info("%s: init description : %s\n", __func__, busmon->init_desc);
	info("%s: target descrition: %s\n", __func__, busmon->target_desc);
	info("%s: user description : %s\n", __func__, busmon->masterip_desc);
	info("%s: user id          : %u\n", __func__, busmon->masterip_idx);
	info("%s: target address   : %lX\n",__func__, busmon->target_addr);

	info("2. FW log dump\n");
	fimc_is_hw_logdump(&core->interface);

	info("3. clock info\n");
	CALL_POPS(core, print_clk);

p_err:
	return ret;
#else
	return 0;
#endif
}

#ifdef ENABLE_FW_SHARE_DUMP
static int fimc_is_fw_share_dump(void)
{
	int ret = 0;
	u8 *buf;
	struct fimc_is_core *core = NULL;
	struct fimc_is_resourcemgr *resourcemgr;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		goto p_err;

	resourcemgr = &core->resourcemgr;
	buf = (u8 *)resourcemgr->fw_share_dump_buf;

	/* dump share region in fw area */
	if (IS_ERR_OR_NULL(buf)) {
		err("%s: fail to alloc", __func__);
		ret = -ENOMEM;
		goto p_err;
	}

	/* sync with fw for memory */
	vb2_ion_sync_for_cpu(resourcemgr->minfo.fw_cookie, 0,
			FIMC_IS_SHARED_OFFSET, DMA_BIDIRECTIONAL);

	memcpy(buf, (u8 *)resourcemgr->minfo.kvaddr_fshared, FIMC_IS_SHARED_SIZE);

	info("%s: dumped ramdump addr(virt/phys/size): (%p/%p/0x%X)", __func__, buf,
			(void *)virt_to_phys(buf), FIMC_IS_SHARED_SIZE);
p_err:
	return ret;
}
#endif

int fimc_is_resource_dump(void)
{
	struct fimc_is_core *core = NULL;
	struct fimc_is_device_ischain *device = NULL;
	int i;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		goto exit;

	info("### %s dump start ###\n", __func__);

	for (i = 0; i < FIMC_IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state))
			continue;

		if (test_bit(FIMC_IS_ISCHAIN_CLOSING, &device->state))
			continue;

		/* clock & gpio dump */
		CALL_POPS(device, print_clk);
#ifdef ENABLE_IS_CORE
		/* fw log, mcuctl dump */
		fimc_is_hw_logdump(&core->interface);
		fimc_is_hw_regdump(&core->interface);
#else
		/* ddk log dump */
		fimc_is_lib_logdump();
#endif
		break;
	}

	info("### %s dump end ###\n", __func__);

exit:
	return 0;
}

#ifdef ENABLE_PANIC_HANDLER
static int fimc_is_panic_handler(struct notifier_block *nb, ulong l,
	void *buf)
{
#if !defined(ENABLE_IS_CORE)
	fimc_is_resource_dump();
#endif
#ifdef ENABLE_FW_SHARE_DUMP
	/* dump share area in fw region */
	fimc_is_fw_share_dump();
#endif
	return 0;
}

static struct notifier_block notify_panic_block = {
	.notifier_call = fimc_is_panic_handler,
};
#endif

int fimc_is_resourcemgr_probe(struct fimc_is_resourcemgr *resourcemgr,
	void *private_data)
{
	int ret = 0;

	BUG_ON(!resourcemgr);
	BUG_ON(!private_data);

	resourcemgr->private_data = private_data;

	clear_bit(FIMC_IS_RM_COM_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS0_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS1_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS2_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS3_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS4_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS5_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_ISC_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_POWER_ON, &resourcemgr->state);
	atomic_set(&resourcemgr->rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor0.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor1.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor2.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor3.rsccount, 0);
	atomic_set(&resourcemgr->resource_ischain.rsccount, 0);
	atomic_set(&resourcemgr->resource_preproc.rsccount, 0);

	resourcemgr->cluster0 = 0;
	resourcemgr->cluster1 = 0;
	resourcemgr->hal_version = IS_HAL_VER_1_0;

	/* temperature monitor unit */
	resourcemgr->tmu_notifier.notifier_call = fimc_is_tmu_notifier;
	resourcemgr->tmu_notifier.priority = 0;
	resourcemgr->tmu_state = ISP_NORMAL;
	resourcemgr->limited_fps = 0;

	/* bus monitor unit */
	resourcemgr->bmu_notifier.notifier_call = fimc_is_bmu_notifier;
	resourcemgr->bmu_notifier.priority = 0;

	ret = exynos_tmu_isp_add_notifier(&resourcemgr->tmu_notifier);
	if (ret) {
		probe_err("exynos_tmu_isp_add_notifier is fail(%d)", ret);
		goto p_err;
	}

#ifdef CONFIG_EXYNOS_BUSMONITOR
	busmon_notifier_chain_register(&resourcemgr->bmu_notifier);
#endif

#ifdef ENABLE_RESERVED_MEM
	ret = fimc_is_resourcemgr_initmem(resourcemgr);
	if (ret) {
		probe_err("fimc_is_resourcemgr_initmem is fail(%d)", ret);
		goto p_err;
	}
#endif

#ifdef ENABLE_DVFS
	/* dvfs controller init */
	ret = fimc_is_dvfs_init(resourcemgr);
	if (ret) {
		probe_err("%s: fimc_is_dvfs_init failed!\n", __func__);
		goto p_err;
	}
#endif

#ifdef ENABLE_PANIC_HANDLER
	atomic_notifier_chain_register(&panic_notifier_list, &notify_panic_block);
#endif

#ifdef ENABLE_FW_SHARE_DUMP
	/* to dump share region in fw area */
	resourcemgr->fw_share_dump_buf = (ulong)kzalloc(FIMC_IS_SHARED_SIZE, GFP_KERNEL);
#endif

p_err:
	probe_info("[RSC] %s(%d)\n", __func__, ret);
	return ret;
}

int fimc_is_resource_open(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type, void **device)
{
	int ret = 0;
	u32 stream;
	void *result;
	struct fimc_is_resource *resource;
	struct fimc_is_core *core;
	struct fimc_is_device_ischain *ischain;

	BUG_ON(!resourcemgr);
	BUG_ON(!resourcemgr->private_data);
	BUG_ON(rsc_type >= RESOURCE_TYPE_MAX);

	result = NULL;
	core = (struct fimc_is_core *)resourcemgr->private_data;
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (rsc_type) {
	case RESOURCE_TYPE_PREPROC:
		result = &core->preproc;
		resource->pdev = core->preproc.pdev;
		break;
	case RESOURCE_TYPE_SENSOR0:
		result = &core->sensor[RESOURCE_TYPE_SENSOR0];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR0].pdev;
		break;
	case RESOURCE_TYPE_SENSOR1:
		result = &core->sensor[RESOURCE_TYPE_SENSOR1];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR1].pdev;
		break;
	case RESOURCE_TYPE_SENSOR2:
		result = &core->sensor[RESOURCE_TYPE_SENSOR2];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR2].pdev;
		break;
	case RESOURCE_TYPE_SENSOR3:
		result = &core->sensor[RESOURCE_TYPE_SENSOR3];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR3].pdev;
		break;
	case RESOURCE_TYPE_SENSOR4:
		result = &core->sensor[RESOURCE_TYPE_SENSOR4];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR4].pdev;
		break;
	case RESOURCE_TYPE_SENSOR5:
		result = &core->sensor[RESOURCE_TYPE_SENSOR5];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR5].pdev;
		break;
	case RESOURCE_TYPE_ISCHAIN:
		for (stream = 0; stream < FIMC_IS_STREAM_COUNT; ++stream) {
			ischain = &core->ischain[stream];
			if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &ischain->state)) {
				result = ischain;
				resource->pdev = ischain->pdev;
				break;
			}
		}
		break;
	}

	if (device)
		*device = result;

p_err:
	dbg_resource("%s\n", __func__);
	return ret;
}

int fimc_is_resource_get(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int ret = 0;
	u32 rsccount;
	struct fimc_is_resource *resource;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);
	BUG_ON(!resourcemgr->private_data);
	BUG_ON(rsc_type >= RESOURCE_TYPE_MAX);

	core = (struct fimc_is_core *)resourcemgr->private_data;
	rsccount = atomic_read(&core->rsccount);
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!core->pdev) {
		err("[RSC] pdev is NULL");
		ret = -EMFILE;
		goto p_err;
	}

	if (rsccount >= (FIMC_IS_STREAM_COUNT + FIMC_IS_VIDEO_SS5_NUM)) {
		err("[RSC] Invalid rsccount(%d)", rsccount);
		ret = -EMFILE;
		goto p_err;
	}

	if (rsccount == 0) {
		pm_stay_awake(&core->pdev->dev);

		resourcemgr->cluster0 = 0;
		resourcemgr->cluster1 = 0;

#ifdef ENABLE_DVFS
		/* dvfs controller init */
		ret = fimc_is_dvfs_init(resourcemgr);
		if (ret) {
			err("%s: fimc_is_dvfs_init failed!\n", __func__);
			goto p_err;
		}
#endif
	}

	if (atomic_read(&resource->rsccount) == 0) {
		switch (rsc_type) {
		case RESOURCE_TYPE_PREPROC:
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_preproc_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_COM_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR0:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_SS0_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR1:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_SS1_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR2:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_SS2_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR3:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_SS3_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR4:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_SS4_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR5:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_SS5_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_ISCHAIN:
			if (test_bit(FIMC_IS_RM_POWER_ON, &resourcemgr->state)) {
				err("all resource is not power off(%lX)", resourcemgr->state);
				ret = -EINVAL;
				goto p_err;
			}

#ifndef ENABLE_RESERVED_MEM
			ret = fimc_is_resourcemgr_initmem(resourcemgr);
			if (ret) {
				err("fimc_is_resourcemgr_initmem is fail(%d)\n", ret);
				goto p_err;
			}
#endif

			ret = fimc_is_debug_open(&resourcemgr->minfo);
			if (ret) {
				err("fimc_is_debug_open is fail(%d)", ret);
				goto p_err;
			}

			ret = fimc_is_interface_open(&core->interface);
			if (ret) {
				err("fimc_is_interface_open is fail(%d)", ret);
				goto p_err;
			}

			ret = fimc_is_ischain_power(&core->ischain[0], 1);
			if (ret) {
				err("fimc_is_ischain_power is fail(%d)", ret);
				fimc_is_ischain_power(&core->ischain[0], 0);
				goto p_err;
			}

			/* W/A for a lower version MCUCTL */
			fimc_is_interface_reset(&core->interface);

#ifdef ENABLE_CLOCK_GATE
			if (sysfs_debug.en_clk_gate &&
					sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
				fimc_is_clk_gate_init(core);
#endif

			set_bit(FIMC_IS_RM_ISC_POWER_ON, &resourcemgr->state);
			set_bit(FIMC_IS_RM_POWER_ON, &resourcemgr->state);
			break;
		default:
			err("[RSC] resource type(%d) is invalid", rsc_type);
			BUG();
			break;
		}
	}

	atomic_inc(&resource->rsccount);
	atomic_inc(&core->rsccount);

p_err:
	info("[RSC] rsctype : %d, rsccount : %d\n", rsc_type, rsccount + 1);
	return ret;
}

int fimc_is_resource_put(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int ret = 0;
	u32 rsccount;
	struct fimc_is_resource *resource;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);
	BUG_ON(!resourcemgr->private_data);
	BUG_ON(rsc_type >= RESOURCE_TYPE_MAX);

	core = (struct fimc_is_core *)resourcemgr->private_data;
	rsccount = atomic_read(&core->rsccount);
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!core->pdev) {
		err("[RSC] pdev is NULL");
		ret = -EMFILE;
		goto p_err;
	}

	if (rsccount == 0) {
		err("[RSC] Invalid rsccount(%d)\n", rsccount);
		ret = -EMFILE;
		goto p_err;
	}

	/* local update */
	if (atomic_read(&resource->rsccount) == 1) {
		/* clear hal version, default 1.0 */
		resourcemgr->hal_version = IS_HAL_VER_1_0;

		switch (rsc_type) {
		case RESOURCE_TYPE_PREPROC:
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_preproc_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_COM_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR0:
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_SS0_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR1:
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_SS1_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR2:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_SS2_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR3:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_SS3_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR4:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_SS4_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR5:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_SS5_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_ISCHAIN:
			ret = fimc_is_itf_power_down(&core->interface);
			if (ret)
				err("power down cmd is fail(%d)", ret);

			ret = fimc_is_ischain_power(&core->ischain[0], 0);
			if (ret)
				err("fimc_is_ischain_power is fail(%d)", ret);

			ret = fimc_is_interface_close(&core->interface);
			if (ret)
				err("fimc_is_interface_close is fail(%d)", ret);

			ret = fimc_is_debug_close();
			if (ret)
				err("fimc_is_debug_close is fail(%d)", ret);

#ifndef ENABLE_RESERVED_MEM
			ret = fimc_is_resourcemgr_deinitmem(resourcemgr);
			if (ret)
				err("fimc_is_resourcemgr_deinitmem is fail(%d)", ret);
#endif

			clear_bit(FIMC_IS_RM_ISC_POWER_ON, &resourcemgr->state);
			break;
		default:
			err("[RSC] resource type(%d) is invalid", rsc_type);
			BUG();
			break;
		}
	}

	/* global update */
	if (atomic_read(&core->rsccount) == 1) {
		u32 current_min, current_max;

		current_min = (resourcemgr->cluster0 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
		current_max = (resourcemgr->cluster0 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
		if (current_min) {
			C0MIN_QOS_DEL();
			warn("[RSC] cluster0 minfreq is not removed(%dMhz)\n", current_min);
		}

		if (current_max) {
			C0MAX_QOS_DEL();
			warn("[RSC] cluster0 maxfreq is not removed(%dMhz)\n", current_max);
		}

		current_min = (resourcemgr->cluster1 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
		current_max = (resourcemgr->cluster1 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
		if (current_min) {
			C1MIN_QOS_DEL();
			warn("[RSC] cluster1 minfreq is not removed(%dMhz)\n", current_min);
		}

		if (current_max) {
			C1MAX_QOS_DEL();
			warn("[RSC] cluster1 maxfreq is not removed(%dMhz)\n", current_max);
		}

		resourcemgr->cluster0 = 0;
		resourcemgr->cluster1 = 0;

		ret = fimc_is_runtime_suspend_post(&resource->pdev->dev);
		if (ret)
			err("fimc_is_runtime_suspend_post is fail(%d)", ret);

		pm_relax(&core->pdev->dev);

		clear_bit(FIMC_IS_RM_POWER_ON, &resourcemgr->state);
	}

	atomic_dec(&resource->rsccount);
	atomic_dec(&core->rsccount);

p_err:
	info("[RSC] rsctype : %d, rsccount : %d\n", rsc_type, rsccount - 1);
	return ret;
}

int fimc_is_resource_ioctl(struct fimc_is_resourcemgr *resourcemgr, struct v4l2_control *ctrl)
{
	int ret = 0;

	BUG_ON(!resourcemgr);
	BUG_ON(!ctrl);

	switch (ctrl->id) {
	/* APOLLO CPU0~3 */
	case V4L2_CID_IS_DVFS_CLUSTER0:
		{
			u32 current_min, current_max;
			u32 request_min, request_max;

			current_min = (resourcemgr->cluster0 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			current_max = (resourcemgr->cluster0 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
			request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

			if (current_min) {
				if (request_min)
					C0MIN_QOS_UPDATE(request_min);
				else
					C0MIN_QOS_DEL();
			} else {
				if (request_min)
					C0MIN_QOS_ADD(request_min);
			}

			if (current_max) {
				if (request_max)
					C0MAX_QOS_UPDATE(request_max);
				else
					C0MAX_QOS_DEL();
			} else {
				if (request_max)
					C0MAX_QOS_ADD(request_max);
			}

			info("[RSC] cluster0 minfreq : %dMhz\n", request_min);
			info("[RSC] cluster0 maxfreq : %dMhz\n", request_max);
			resourcemgr->cluster0 = (request_max << CLUSTER_MAX_SHIFT) | request_min;
		}
		break;
	/* ATLAS CPU4~7 */
	case V4L2_CID_IS_DVFS_CLUSTER1:
		{
			u32 current_min, current_max;
			u32 request_min, request_max;

			current_min = (resourcemgr->cluster1 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			current_max = (resourcemgr->cluster1 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
			request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

			if (current_min) {
				if (request_min)
					C1MIN_QOS_UPDATE(request_min);
				else
					C1MIN_QOS_DEL();
			} else {
				if (request_min)
					C1MIN_QOS_ADD(request_min);
			}

			if (current_max) {
				if (request_max)
					C1MAX_QOS_UPDATE(request_max);
				else
					C1MAX_QOS_DEL();
			} else {
				if (request_max)
					C1MAX_QOS_ADD(request_max);
			}

			info("[RSC] cluster1 minfreq : %dMhz\n", request_min);
			info("[RSC] cluster1 maxfreq : %dMhz\n", request_max);
			resourcemgr->cluster1 = (request_max << CLUSTER_MAX_SHIFT) | request_min;
		}
		break;
	}

	return ret;
}

int fimc_is_logsync(struct fimc_is_interface *itf, u32 sync_id, u32 msg_test_id)
{
	int ret = 0;

	/* print kernel sync log */
	log_sync(sync_id);

#ifdef ENABLE_FW_SYNC_LOG
	ret = fimc_is_hw_msg_test(itf, sync_id, msg_test_id);
	if (ret)
	err("fimc_is_hw_msg_test(%d)", ret);
#endif
	return ret;
}
