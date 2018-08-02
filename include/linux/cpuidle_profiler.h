/*
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CPUIDLE_PROFILE_H
#define CPUIDLE_PROFILE_H __FILE__

#include <linux/ktime.h>
#include <linux/cpuidle.h>

#include <asm/cputype.h>
#include <asm/psci.h>

/*
 * cpuidle major state
 */
#define PROFILE_C1		0
#define PROFILE_C2		1
#define PROFILE_LPM		2

#define has_sub_state(_state)	( _state > PROFILE_C1 ? 1 : 0)

/*
 * C2 sub state
 */
#define C2_CPD			PSCI_CLUSTER_SLEEP
#define C2_SICD			PSCI_SYSTEM_IDLE
#define C2_SICD_CPD		PSCI_SYSTEM_IDLE_CLUSTER_SLEEP
#define C2_SICD_AUD		PSCI_SYSTEM_IDLE_AUDIO

/*
 * LPM sub state
 */
#define	LPM_SICD		SYS_SICD
#define	LPM_SICD_CPD		SYS_SICD_CPD
#define	LPM_SICD_AUD		SYS_SICD_AUD
#define	LPM_AFTR		SYS_AFTR
#define	LPM_STOP		SYS_STOP
#define	LPM_DSTOP		SYS_DSTOP
#define	LPM_LPD			SYS_LPD
#define LPM_ALPA		SYS_ALPA

#define to_cluster(cpu)		MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1)

struct cpuidle_profile_state_usage {
	unsigned int entry_count;
	unsigned int early_wakeup_count;
	unsigned long long time;
};

struct cpuidle_profile_info {
	ktime_t last_entry_time;
	int cur_state;
	int state_count;

	struct cpuidle_profile_state_usage *usage;
};

extern void cpuidle_profile_start(int cpu, int state, int sub_state);
extern void cpuidle_profile_start_no_substate(int cpu, int state);
extern void cpuidle_profile_finish(int cpuid, int early_wakeup);
extern void cpuidle_profile_finish_no_earlywakeup(int cpuid);
extern void cpuidle_profile_register(struct cpuidle_driver *drv);

#ifdef CONFIG_CPU_IDLE
extern void cpuidle_profile_collect_idle_ip(int mode,
				int index, unsigned int idle_ip);
#else
static inline void cpuidle_profile_collect_idle_ip(int mode,
				int index, unsigned int idle_ip)
{
	/* Nothing to do */
}
#endif

#endif /* CPUIDLE_PROFILE_H */
