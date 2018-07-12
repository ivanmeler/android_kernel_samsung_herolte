/*
 * Copyright 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com/
 *
 * Header file for tmu support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_TMU_H
#define __ASM_ARCH_TMU_H

#include <linux/platform_data/exynos_thermal.h>

#define MUX_ADDR_VALUE 6
#define TMU_SAVE_NUM 10
#define TMU_DC_VALUE 25
#define UNUSED_THRESHOLD 0xFF

#define COLD_TEMP		19
#define HOT_NORMAL_TEMP		95
#define HOT_CRITICAL_TEMP	110
#define MIF_TH_TEMP1		55
#define MIF_TH_TEMP2		95

#if defined(CONFIG_SOC_EXYNOS5433)
#define GPU_TH_TEMP1		95
#define GPU_TH_TEMP2		100
#define GPU_TH_TEMP3		105
#define GPU_TH_TEMP4		110
#define GPU_TH_TEMP5		115
#elif defined(CONFIG_SOC_EXYNOS7420)
#define GPU_TH_TEMP1		90
#define GPU_TH_TEMP2		95
#define GPU_TH_TEMP3		100
#define GPU_TH_TEMP4		105
#define GPU_TH_TEMP5		110
#elif defined(CONFIG_SOC_EXYNOS7890)
#define GPU_TH_TEMP1		85
#define GPU_TH_TEMP2		90
#define GPU_TH_TEMP3		95
#define GPU_TH_TEMP4		100
#define GPU_TH_TEMP5		105
#else
#define GPU_TH_TEMP1		85
#define GPU_TH_TEMP2		90
#define GPU_TH_TEMP3		95
#define GPU_TH_TEMP4		100
#define GPU_TH_TEMP5		110
#endif

#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890)
#define ISP_TH_TEMP1		85
#define ISP_TH_TEMP2		95
#define ISP_TH_TEMP3		100
#define ISP_TH_TEMP4		105
#define ISP_TH_TEMP5		110
#endif

enum isp_noti_state_t {
	ISP_NORMAL = 0,
	ISP_COLD,
	ISP_THROTTLING1,
	ISP_THROTTLING2,
	ISP_THROTTLING3,
	ISP_THROTTLING4,
	ISP_TRIPPING,
};

enum tmu_status_t {
	TMU_STATUS_INIT = 0,
	TMU_STATUS_NORMAL,
	TMU_STATUS_THROTTLED,
	TMU_STATUS_TRIPPED,
};

enum mif_noti_state_t {
	MIF_TH_LV1 = 4,
	MIF_TH_LV2,
	MIF_TH_LV3,
};

enum tmu_noti_state_t {
	TMU_NORMAL,
	TMU_COLD,
	TMU_HOT,
	TMU_CRITICAL,
};

enum gpu_noti_state_t {
	GPU_NORMAL,
	GPU_COLD,
	GPU_THROTTLING1,
	GPU_THROTTLING2,
	GPU_THROTTLING3,
	GPU_THROTTLING4,
	GPU_TRIPPING,
	GPU_THROTTLING,
};

enum mif_thermal_state_t {
	MIF_NORMAL = 0,
	MIF_THROTTLING1,
	MIF_THROTTLING2,
	MIF_TRIPPING,
};

#if defined(CONFIG_SOC_EXYNOS5433)
enum tmu_core_number {
	EXYNOS_TMU_CORE_BIG0 = 0,
	EXYNOS_TMU_CORE_BIG1,
	EXYNOS_TMU_CORE_GPU,
	EXYNOS_TMU_CORE_LITTLE,
	EXYNOS_TMU_CORE_ISP,
};
#elif defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890)
enum tmu_core_number {
	EXYNOS_TMU_CORE_BIG	= 0,
	EXYNOS_TMU_CORE_LITTLE,
	EXYNOS_TMU_CORE_GPU,
	EXYNOS_TMU_CORE_ISP,
};
#endif

#ifdef CONFIG_EXYNOS_THERMAL
extern int exynos_tmu_add_notifier(struct notifier_block *n);
#if !defined(CONFIG_GPU_THERMAL) && defined(CONFIG_SOC_EXYNOS7580)
static inline int exynos_gpu_add_notifier(struct notifier_block *n)
{
	return 0;
}
#else
extern int exynos_gpu_add_notifier(struct notifier_block *n);
#endif
#else
static inline int exynos_tmu_add_notifier(struct notifier_block *n)
{
	return 0;
}
#endif

#if defined(CONFIG_EXYNOS_THERMAL) && (defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890) || defined(CONFIG_SOC_EXYNOS8890))
extern int exynos_tmu_isp_add_notifier(struct notifier_block *n);
#else
static inline int exynos_tmu_isp_add_notifier(struct notifier_block *n)
{
	return 0;
}
#endif

#if defined(CONFIG_EXYNOS_THERMAL) && (defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890) \
	|| defined(CONFIG_SOC_EXYNOS8890))
extern void exynos_tmu_core_control(bool on, int id);
#else
static inline void exynos_tmu_core_control(bool on, int id)
{
	return;
}
#endif

#endif /* __ASM_ARCH_TMU_H */
