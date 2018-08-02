/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU Hotplug support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __EXYNOS_CPU_HOTPLUG_H
#define __EXYNOS_CPU_HOTPLUG_H __FILE__

struct kobject *exynos_cpu_hotplug_kobj(void);
bool exynos_cpu_hotplug_enabled(void);

#ifdef CONFIG_EXYNOS_HOTPLUG_GOVERNOR
extern void inc_boost_req_count(void);
extern void dec_boost_req_count(bool delayed_boost);
extern int hpgov_default_level(void);
#else
static inline void inc_boost_req_count(void) { };
static inline void dec_boost_req_count(bool delayed_boost) { };
static inline int hpgov_default_level(void) { return 0; };
#endif
#endif /* __EXYNOS_CPU_HOTPLUG_H */
