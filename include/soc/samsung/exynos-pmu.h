/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __EXYNOS_PMU_H
#define __EXYNOS_PMU_H __FILE__

/**
 * struct exynos_cpu_power_ops
 *
 * CPU power control operations
 *
 * @power_up : Set cpu configuration register
 * @power_down : Clear cpu configuration register
 * @power_state : Show cpu status. Return true if cpu power on, otherwise return false.
 */
struct exynos_cpu_power_ops {
        void (*power_up)(unsigned int cpu);
        void (*power_down)(unsigned int cpu);
        int (*power_state)(unsigned int cpu);
        void (*cluster_up)(unsigned int cluster);
        void (*cluster_down)(unsigned int cluster);
        int (*cluster_state)(unsigned int cluster);
};
extern struct exynos_cpu_power_ops exynos_cpu;

/**
 * The APIs to control the PMU
 */
int exynos_pmu_read(unsigned int offset, unsigned int *val);
int exynos_pmu_write(unsigned int offset, unsigned int val);
int exynos_pmu_update(unsigned int offset, unsigned int mask, unsigned int val);

#endif /* __EXYNOS_PMU_H */
