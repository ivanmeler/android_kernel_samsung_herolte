/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/sec_debug_hard_reset_hook.h>
#include <linux/gpio.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static unsigned int hard_reset_keys[] = { KEY_POWER, KEY_VOLUMEDOWN };

static unsigned int hold_keys;
static ktime_t hold_time;
static struct hrtimer hard_reset_hook_timer;
static bool hard_reset_occurred;

static bool is_hard_reset_key(unsigned int code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			return true;
	return false;
}

static int hard_reset_key_set(unsigned int code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			hold_keys |= 0x1 << i;
	return hold_keys;
}

static int hard_reset_key_unset(unsigned int code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			hold_keys &= ~(0x1 << i);

	return hold_keys;
}

static int hard_reset_key_all_pressed(void)
{
	int i;
	unsigned int all_pressed = 0x0;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		all_pressed |= 0x1 << i;

	return (hold_keys == all_pressed);
}

#define GPIO_nPOWER 26
#define GPIO_VOLUP 10
static int check_hard_reset_key_pressed(void)
{
	if (!gpio_get_value(GPIO_nPOWER))
		pr_err("%s:%d pressed\n", __func__, KEY_POWER);
	else
		pr_err("%s:%d not pressed\n", __func__, KEY_POWER);

	if (!gpio_get_value(GPIO_VOLUP))
		pr_err("%s:%d pressed\n", __func__, KEY_VOLUMEDOWN);
	else
		pr_err("%s:%d not pressed\n", __func__, KEY_VOLUMEDOWN);

	return 0;
}

static enum hrtimer_restart hard_reset_hook_callback(struct hrtimer *hrtimer)
{
	check_hard_reset_key_pressed();
	pr_err("Hard Reset\n");
	hard_reset_occurred = true;
	BUG();
	return HRTIMER_RESTART;
}

void hard_reset_hook(unsigned int code, int pressed)
{
	if (!is_hard_reset_key(code))
		return;

	if (pressed)
		hard_reset_key_set(code);
	else
		hard_reset_key_unset(code);

	if (hard_reset_key_all_pressed())
		hrtimer_start(&hard_reset_hook_timer,
			      hold_time, HRTIMER_MODE_REL);
	else
		hrtimer_cancel(&hard_reset_hook_timer);
}

bool is_hard_reset_occurred(void)
{
	return hard_reset_occurred;
}

void hard_reset_delay(void)
{
	/* HQE team request hard reset key should guarantee 7 seconds.
	 * To get proper stack, hard reset hook starts after 6 seconds.
	 * And it will reboot before 7 seconds.
	 * Add delay to keep the 7 seconds
	 */
	if (hard_reset_occurred) {
		pr_err("wait until PMIC reset occurred");
		mdelay(2000);
	}
}

int __init hard_reset_hook_init(void)
{
	hrtimer_init(&hard_reset_hook_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	hard_reset_hook_timer.function = hard_reset_hook_callback;
	hold_time = ktime_set(6, 0); /* 6 seconds */

	return 0;
}

early_initcall(hard_reset_hook_init);
