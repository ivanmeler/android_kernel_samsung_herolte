/* linux/arch/arm64/mach-exynos/include/mach/cpufreq.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPUFreq support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ARCH_CPUFREQ_H
#define __ARCH_CPUFREQ_H __FILE__

#include <linux/notifier.h>

/*
 * Common definitions and structures
 */
#define APLL_FREQ(f, a0, a1, a2, a3, a4, a5, a6, a7, b0, b1, b2, m, p, s) \
	{ \
		.freq = (f) * 1000, \
		.clk_div_cpu0 = ((a0) | (a1) << 4 | (a2) << 8 | (a3) << 12 | \
			(a4) << 16 | (a5) << 20 | (a6) << 24 | (a7) << 28), \
		.clk_div_cpu1 = (b0 << 0 | b1 << 4 | b2 << 8), \
		.mps = ((m) << 16 | (p) << 8 | (s)), \
	}

/* APLL Macro for Atlas Frequency in ISTOR */
#define APLL_ATLAS_FREQ(f, a0, a1, a2, a3, a4, a5, b0, b1, b2, m, p, s) \
	{ \
		.freq = (f) * 1000, \
		.clk_div_cpu0 = ((a0) | (a1) << 4 | (a2) << 8 | (a3) << 12 | \
			(a4) << 20 | (a5) << 26), \
		.clk_div_cpu1 = (b0 << 0 | b1 << 4 | b2 << 8), \
		.mps = ((m) << 16 | (p) << 8 | (s)), \
	}

enum cpufreq_level_index {
	L0, L1, L2, L3, L4,
	L5, L6, L7, L8, L9,
	L10, L11, L12, L13, L14,
	L15, L16, L17, L18, L19,
	L20, L21, L22, L23, L24,
};

struct apll_freq {
	unsigned int freq;
	u32 clk_div_cpu0;
	u32 clk_div_cpu1;
	u32 mps;
};

struct exynos_dvfs_info {
	unsigned long	mpll_freq_khz;
	unsigned int	pll_safe_idx;
	unsigned int	max_idx_num;
	unsigned int	max_support_idx;
	unsigned int	min_support_idx;
	unsigned int	cluster_num;
	unsigned int	reboot_limit_freq;
	unsigned int	boost_freq;	/* use only KFC when enable HMP */
	unsigned int	boot_freq;
	unsigned int	boot_cpu_min_qos;
	unsigned int	boot_cpu_max_qos;
	unsigned int	boot_cpu_min_qos_timeout;
	unsigned int	boot_cpu_max_qos_timeout;
#ifdef CONFIG_SEC_PM
	unsigned int	jig_boot_cpu_max_qos;
#endif
	unsigned int    resume_freq;
	int		boot_freq_idx;
	int		*bus_table;
	bool		blocked;
	unsigned int	en_ema;
	unsigned int	en_smpl;
	unsigned int	cur_volt;
	struct clk	*cpu_clk;
	unsigned int	*volt_table;
	unsigned int	*abb_table;
	unsigned int	*max_support_idx_table;
	const unsigned int	*max_op_freqs;
	struct cpufreq_frequency_table	*freq_table;
	struct regulator *regulator;
	void (*set_freq)(unsigned int, unsigned int);
	unsigned int (*get_freq)(void);
	void (*set_ema)(unsigned int);
	bool (*need_apll_change)(unsigned int, unsigned int);
	bool (*is_alive)(void);
	void (*set_int_skew)(int);
	int (*check_smpl)(void);
	void (*clear_smpl)(void);
	int (*init_smpl)(void);
	int (*deinit_smpl)(void);
};

struct cpufreq_clkdiv {
	unsigned int	index;
	unsigned int	clkdiv0;
	unsigned int	clkdiv1;
};

struct cpufreq_dvfs_table {
	u32	index;
	u32	frequency;
	u32	voltage;
	s32	bus_qos_lock;
};

/*
 * common interfaces for IPA
 */
/* interfaces for IPA */
#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ) || defined(CONFIG_ARM_EXYNOS_CPUFREQ)
void exynos_set_max_freq(int max_freq, unsigned int cpu);
void ipa_set_clamp(int cpu, unsigned int clamp_freq, unsigned int gov_target);
#else
static inline void exynos_set_max_freq(int max_freq, unsigned int cpu) {}
static inline void ipa_set_clamp(int cpu, unsigned int clamp_freq, unsigned int gov_target) {}
#endif


/* interface for THERMAL */
extern void exynos_thermal_throttle(void);
extern void exynos_thermal_unthrottle(void);

/*
 * CPUFREQ init events and notifiers
 */
#define CPUFREQ_INIT_COMPLETE	0x0001

#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ) || defined(CONFIG_ARM_EXYNOS_CPUFREQ)
extern int exynos_cpufreq_init_register_notifier(struct notifier_block *nb);
extern int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb);
#else
static inline int exynos_cpufreq_init_register_notifier(struct notifier_block *nb)
{return 0;}
static inline int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb)
{return 0;}
#endif

#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
extern int exynos_cpufreq_smpl_warn_notify_call_chain(void);
#else
static inline int exynos_cpufreq_smpl_warn_notify_call_chain(void){return 0;}
#endif

#if defined(CONFIG_CPU_FREQ)
extern int exynos_cpufreq_cluster0_init(struct exynos_dvfs_info *);
extern int exynos_cpufreq_cluster1_init(struct exynos_dvfs_info *);
typedef enum {
	CL_ZERO,
	CL_ONE,
	CL_END,
} cluster_type;

#if defined(CONFIG_SOC_EXYNOS8890)
#define COLD_VOLT_OFFSET	25000
#else
#define COLD_VOLT_OFFSET	37500
#endif
#define LIMIT_COLD_VOLTAGE	1350000
#define MIN_COLD_VOLTAGE	950000
#define NR_CLUST0_CPUS		4
#define NR_CLUST1_CPUS		4

#define ENABLE_MIN_COLD		0

enum op_state {
	NORMAL,		/* Operation : Normal */
	SUSPEND,	/* Direct API will be blocked in this state */
	RESUME,		/* Re-enabling DVFS using direct API after resume */
};

/*
 * Keep frequency value for counterpart cluster DVFS
 * cur, min, max : Frequency (KHz),
 * c_id : Counter cluster with booting cluster, if booting cluster is
 * A15, c_id will be A7.
 */
struct cpu_info_alter {
	unsigned int cur;
	unsigned int min;
	unsigned int max;
	cluster_type boot_cluster;
	cluster_type c_id;
};

extern cluster_type exynos_boot_cluster;
#endif
#endif /* __ARCH_CPUFREQ_H */
