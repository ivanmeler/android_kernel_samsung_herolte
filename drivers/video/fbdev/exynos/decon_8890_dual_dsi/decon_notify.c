/* linux/driver/video/fbdev/exynos/exynos8890_dual_dsi/decon_notify.c
 *
 * Header file for Samsung Decon driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * MinWoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/export.h>

static BLOCKING_NOTIFIER_HEAD(decon_notifier_list);

/**
 *	decon_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int decon_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&decon_notifier_list, nb);
}
EXPORT_SYMBOL(decon_register_client);

/**
 *	decon_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int decon_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&decon_notifier_list, nb);
}
EXPORT_SYMBOL(decon_unregister_client);

/**
 * decon_notifier_call_chain - notify clients of fb_events
 *
 */
int decon_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&decon_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(decon_notifier_call_chain);


