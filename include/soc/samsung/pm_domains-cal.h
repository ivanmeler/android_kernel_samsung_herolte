/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARCH_PM_RUNTIME_H
#define __ASM_ARCH_PM_RUNTIME_H __FILE__

#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <linux/mfd/samsung/core.h>

#include "../../../drivers/soc/samsung/pwrcal/pwrcal.h"

#include <soc/samsung/exynos-powermode.h>
#include <soc/samsung/exynos-devfreq.h>

#define PM_DOMAIN_PREFIX	"PM DOMAIN: "

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifdef CONFIG_EXYNOS_PM_DOMAIN_DEBUG
#define DEBUG_PRINT_INFO(fmt, ...) printk(PM_DOMAIN_PREFIX pr_fmt(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INFO(fmt, ...)
#endif

/* In Exynos, the number of MAX_POWER_DOMAIN is less than 15 */
#define MAX_PARENT_POWER_DOMAIN	15

struct exynos_pm_domain;

struct exynos_pm_domain {
	struct generic_pm_domain genpd;
	char *name;
	void __iomem *base;
	char *cal_vclkname;
	unsigned int cal_pdid;
	struct device_node *of_node;
	int (*pd_control)(unsigned int cal_id, int on);
	int (*check_status)(struct exynos_pm_domain *pd);
	unsigned int bts;
	struct mutex access_lock;
	int idle_ip_index;
};

#endif /* __ASM_ARCH_PM_RUNTIME_H */
