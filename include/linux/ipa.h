/*
 *  ARM Intelligent Power Allocation
 *
 *  Copyright (C) 2013 ARM Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DRIVERS_THERMAL_IPA_H
#define _DRIVERS_THERMAL_IPA_H

#include <linux/cpufreq.h>
#include <linux/sysfs.h>

#define THERMAL_NEW_MAX_FREQ 0

struct ipa_sensor_conf {
	int (*read_soc_temperature)(void *data);
	int (*read_skin_temperature)(void);
	void *private_data;
};

struct thermal_limits {
	int max_freq;
	int cur_freq;
	cpumask_t cpus;
};

struct ipa_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj,
                    struct kobj_attribute *attr, char *buf);
	ssize_t (*store)(struct kobject *a, struct kobj_attribute *b,
			 const char *c, size_t count);
};

struct ipa_attr_ro {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj,
                    struct kobj_attribute *attr, char *buf);
};

#define define_ipa_attr(_name)			\
static struct ipa_attr _name =			\
__ATTR(_name, 0664, _name##_show, _name##_store)

#define define_ipa_attr_ro(_name)		\
static struct ipa_attr _name =			\
__ATTR(_name, 0444, _name##_show, NULL)

int ipa_hotplug(bool remove_cores);  // this is not very generic

#ifdef CONFIG_CPU_THERMAL_IPA

void ipa_cpufreq_requested(struct cpufreq_policy *p, unsigned int freq);
int ipa_register_thermal_sensor(struct ipa_sensor_conf *);
int thermal_register_notifier(struct notifier_block *nb);
int thermal_unregister_notifier(struct notifier_block *nb);

#else

static void __maybe_unused ipa_cpufreq_requested(struct cpufreq_policy *p, unsigned int *freqs)
{
}

static inline int ipa_register_thermal_sensor(struct ipa_sensor_conf *p)
{
	return 0;
}

static inline int thermal_register_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int thermal_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}

#endif

#endif /* _DRIVERS_THERMAL_IPA_H */
