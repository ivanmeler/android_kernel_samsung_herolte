/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS BUS MONITOR Driver for Samsung EXYNOS SoC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS_BUSMONITOR__H
#define EXYNOS_BUSMONITOR__H

#ifdef CONFIG_EXYNOS_BUSMONITOR
struct busmon_notifier {
	char *init_desc;
	char *target_desc;
	char *masterip_desc;
	unsigned int masterip_idx;
	unsigned long target_addr;
};
extern void busmon_notifier_chain_register(struct notifier_block *n);
extern bool busmon_get_panic(void);
#else
#define busmon_notifier_chain_register(x)		do { } while(0)
#endif

#endif
