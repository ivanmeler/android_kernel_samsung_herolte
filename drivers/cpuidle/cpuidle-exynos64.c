/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * CPUIDLE driver for exynos 64bit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/cpuidle_profiler.h>
#ifdef CONFIG_SEC_PM
#include <linux/moduleparam.h>
#endif

#include <asm/suspend.h>
#include <asm/tlbflush.h>
#include <asm/psci.h>
#include <asm/cpuidle.h>

#include <soc/samsung/exynos-powermode.h>

#include "dt_idle_states.h"

#ifdef CONFIG_SEC_PM
#define CPUIDLE_ENABLE_MASK (ENABLE_C2 | ENABLE_LPM)

static enum {
	ENABLE_C2	= BIT(0),
	ENABLE_LPM	= BIT(1),
} enable_mask = CPUIDLE_ENABLE_MASK;

DEFINE_SPINLOCK(enable_mask_lock);

static int set_enable_mask(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_uint(val, kp);
	unsigned long flags;

	pr_info("%s: enable_mask=0x%x\n", __func__, enable_mask);

	if (rv)
		return rv;

	spin_lock_irqsave(&enable_mask_lock, flags);

	if (!(enable_mask & ENABLE_C2)) {
		unsigned int cpuid = smp_processor_id();
		int i;
		for_each_online_cpu(i) {
			if (i == cpuid)
				continue;
			smp_send_reschedule(i);
		}
	}

	spin_unlock_irqrestore(&enable_mask_lock, flags);

	return 0;
}

static struct kernel_param_ops enable_mask_param_ops = {
	.set = set_enable_mask,
	.get = param_get_uint,
};

module_param_cb(enable_mask, &enable_mask_param_ops, &enable_mask, 0644);
MODULE_PARM_DESC(enable_mask, "bitmask for C states - C2, C3(LPM)");
#endif /* CONFIG_SEC_PM */

#ifdef CONFIG_SEC_PM_DEBUG
unsigned int log_en;
module_param_named(log_en, log_en, uint, 0644);
#endif /* CONFIG_SEC_PM_DEBUG */

/*
 * Exynos cpuidle driver supports the below idle states
 *
 * IDLE_C1 : WFI(Wait For Interrupt) low-power state
 * IDLE_C2 : Local CPU power gating
 * IDLE_LPM : Low Power Mode, specified by platform
 */
enum exynos_cpuidle_state {
	IDLE_C1 = 0,
	IDLE_C2,
	IDLE_LPM,
	IDLE_STATE_MAX,
};

static char *idle_state_name[IDLE_STATE_MAX] = {"c1", "c2", "lpm"};

/***************************************************************************
 *                             Helper function                             *
 ***************************************************************************/
static void prepare_idle(unsigned int cpuid)
{
	cpu_pm_enter();
}

static void post_idle(unsigned int cpuid)
{
	cpu_pm_exit();
}

static bool nonboot_cpus_working(void)
{
	return (num_online_cpus() > 1);
}

static int find_available_low_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, unsigned int index)
{
	while (--index > 0) {
		struct cpuidle_state *s = &drv->states[index];
		struct cpuidle_state_usage *su = &dev->states_usage[index];

		if (s->disabled || su->disable)
			continue;
		else
			return index;
	}

	return IDLE_C1;
}

/***************************************************************************
 *                           Cpuidle state handler                         *
 ***************************************************************************/
static int exynos_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	cpuidle_profile_start_no_substate(dev->cpu, index);

	cpu_do_idle();

	cpuidle_profile_finish_no_earlywakeup(dev->cpu);

	return index;
}

static int exynos_enter_c2(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	int ret, entry_index;

#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(log_en & ENABLE_C2))
		pr_info("+++c2\n");
#endif
	prepare_idle(dev->cpu);

	entry_index = enter_c2(dev->cpu, index);

	cpuidle_profile_start(dev->cpu, index, entry_index);

	ret = cpu_suspend(entry_index);
	if (ret)
		flush_tlb_all();

	cpuidle_profile_finish(dev->cpu, ret);

	wakeup_from_c2(dev->cpu, ret);

	post_idle(dev->cpu);

#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(log_en & ENABLE_C2))
		pr_info("---c2\n");
#endif
	return index;
}

static int exynos_enter_lpm(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	int ret, mode;

	mode = determine_lpm();

#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(log_en & ENABLE_LPM))
		pr_info("+++lpm:%d\n", mode);
#endif
	prepare_idle(dev->cpu);

	exynos_prepare_sys_powerdown(mode);

	cpuidle_profile_start(dev->cpu, index, mode);

	ret = cpu_suspend(index);

	cpuidle_profile_finish(dev->cpu, ret);

	exynos_wakeup_sys_powerdown(mode, (bool)ret);

	post_idle(dev->cpu);

#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(log_en & ENABLE_LPM))
		pr_info("+++lpm:%d\n", mode);
#endif
	return index;
}

static int exynos_enter_idle_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	int (*func)(struct cpuidle_device *, struct cpuidle_driver *, int);
	ktime_t time_start, time_end;
	int ret;

	exynos_ss_cpuidle(idle_state_name[index], index, 0, ESS_FLAG_IN);
	time_start = ktime_get();

#ifdef CONFIG_SEC_PM
	switch (index) {
	case IDLE_C2:
		if (unlikely(!(enable_mask & ENABLE_C2)))
			index = IDLE_C1;
		break;
	case IDLE_LPM:
		if (unlikely(!(enable_mask & ENABLE_LPM))) {
			if (enable_mask & ENABLE_C2)
				index = IDLE_C2;
			else
				index = IDLE_C1;
		}
		break;
	default:
		break;
	}
#endif

	switch (index) {
	case IDLE_C1:
		func = exynos_enter_idle;
		break;
	case IDLE_C2:
		func = exynos_enter_c2;
		break;
	case IDLE_LPM:
		/*
		 * In exynos, system can enter LPM when only boot core is running.
		 * In other words, non-boot cores should be shutdown to enter LPM.
		 */
		if (nonboot_cpus_working()) {
			index = find_available_low_state(dev, drv, index);
			return exynos_enter_idle_state(dev, drv, index);
		} else {
			func = exynos_enter_lpm;
		}
		break;
	default:
		pr_err("%s : Invalid index: %d\n", __func__, index);
		return -EINVAL;
	}

	ret = (*func)(dev, drv, index);

	time_end = ktime_get();
	exynos_ss_cpuidle(idle_state_name[index], ret,
		(int)ktime_to_us(ktime_sub(time_end, time_start)), ESS_FLAG_OUT);

	return ret;
}

/***************************************************************************
 *                            Define notifier call                         *
 ***************************************************************************/
static int exynos_cpuidle_reboot_notifier(struct notifier_block *this,
				unsigned long event, void *_cmd)
{
	switch (event) {
	case SYSTEM_POWER_OFF:
	case SYS_RESTART:
		cpuidle_pause();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpuidle_reboot_nb = {
	.notifier_call = exynos_cpuidle_reboot_notifier,
};

/***************************************************************************
 *                         Initialize cpuidle driver                       *
 ***************************************************************************/
#define EXYNOS_CPUIDLE_WFI_STATE {\
	.enter                  = exynos_enter_idle,\
	.exit_latency           = 1,\
	.target_residency       = 1,\
	.power_usage		= UINT_MAX,\
	.flags                  = CPUIDLE_FLAG_TIME_VALID,\
	.name                   = "WFI",\
	.desc                   = "ARM WFI",\
}

static struct cpuidle_driver exynos_idle_boot_cluster_driver = {
	.name = "boot_cluster_idle",
	.owner = THIS_MODULE,
	.states[0] = EXYNOS_CPUIDLE_WFI_STATE,
};

static struct cpuidle_driver exynos_idle_nonboot_cluster_driver = {
	.name = "non-boot_cluster_idle",
	.owner = THIS_MODULE,
	.states[0] = EXYNOS_CPUIDLE_WFI_STATE,
#ifdef CONFIG_CPU_IDLE_GOV_MENU
	.skip_correction = 0,
#endif
};

static const struct of_device_id exynos_idle_state_match[] __initconst = {
	{ .compatible = "exynos,idle-state",
	  .data = exynos_enter_idle_state },
	{ },
};

static void __init exynos_idle_driver_init(struct cpuidle_driver *drv,
					struct cpumask* cpumask, int part_id)
{
	int cpu;

	/* HACK : need to change using part id */
	for_each_possible_cpu(cpu)
		if (((cpu & 0x4) >> 2) == part_id)
			cpumask_set_cpu(cpu, cpumask);

	drv->cpumask = cpumask;
}

static struct cpumask boot_cluster_cpumask;
static struct cpumask nonboot_cluster_cpumask;

static int __init exynos_idle_init(void)
{
	int ret, cpu;

	exynos_idle_driver_init(&exynos_idle_boot_cluster_driver,
						&boot_cluster_cpumask, 0);
	exynos_idle_driver_init(&exynos_idle_nonboot_cluster_driver,
						&nonboot_cluster_cpumask, 1);

	/*
	 * Initialize idle states data, starting at index 1.
	 * This driver is DT only, if no DT idle states are detected (ret == 0)
	 * let the driver initialization fail accordingly since there is no
	 * reason to initialize the idle driver if only wfi is supported.
	 */
	ret = dt_init_idle_driver(&exynos_idle_boot_cluster_driver,
					exynos_idle_state_match, 1);
	if (ret < 0)
		return ret;

	ret = dt_init_idle_driver(&exynos_idle_nonboot_cluster_driver,
					exynos_idle_state_match, 1);
	if (ret < 0)
		return ret;

	/*
	 * Call arch CPU operations in order to initialize
	 * idle states suspend back-end specific data
	 */
	for_each_possible_cpu(cpu) {
		ret = cpu_init_idle(cpu);
		if (ret) {
			pr_err("CPU %d failed to init idle CPU ops\n", cpu);
			return ret;
		}
	}

	ret = cpuidle_register(&exynos_idle_boot_cluster_driver, NULL);
	if (ret)
		return ret;

	ret = cpuidle_register(&exynos_idle_nonboot_cluster_driver, NULL);
	if (ret)
		goto out_unregister_boot_cluster;

	register_reboot_notifier(&exynos_cpuidle_reboot_nb);

	cpuidle_profile_register(&exynos_idle_boot_cluster_driver);

	pr_info("Exynos cpuidle driver Initialized\n");

	return 0;

out_unregister_boot_cluster:
	cpuidle_unregister(&exynos_idle_boot_cluster_driver);

	return ret;
}
device_initcall(exynos_idle_init);
