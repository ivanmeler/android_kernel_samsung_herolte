/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>

#ifndef ARRAYSIZE
#define ARRAYSIZE(a)		(sizeof(a) / sizeof(a[0]))
#endif

/* Input sequence 9530 */
#define CRASH_COUNT_FIRST 9
#define CRASH_COUNT_SECOND 5
#define CRASH_COUNT_THIRD 3

#define KEY_STATE_DOWN 1
#define KEY_STATE_UP 0

/* #define DEBUG */

#ifdef DEBUG
#define SEC_LOG(fmt, args...) pr_info("%s:%s: " fmt "\n", \
		"sec_upload", __func__, ##args)
#else
#define SEC_LOG(fmt, args...) do { } while (0)
#endif

struct crash_key {
	unsigned int key_code;
	unsigned int crash_count;
};

struct crash_key user_crash_key_combination[] = {
	{KEY_POWER, CRASH_COUNT_FIRST},
	{KEY_VOLUMEUP, CRASH_COUNT_SECOND},
	{KEY_POWER, CRASH_COUNT_THIRD},
};

struct key_state {
	unsigned int key_code;
	unsigned int state;
};

struct key_state key_states[] = {
	{KEY_VOLUMEDOWN, KEY_STATE_UP},
	{KEY_VOLUMEUP, KEY_STATE_UP},
	{KEY_POWER, KEY_STATE_UP},
	{KEY_HOMEPAGE, KEY_STATE_UP},
};

static unsigned int hold_key = KEY_VOLUMEDOWN;
static unsigned int hold_key_hold = KEY_STATE_UP;
static unsigned int check_count;
static unsigned int check_step;

static int is_hold_key(unsigned int code)
{
	return (code == hold_key);
}

static void set_hold_key_hold(int state)
{
	hold_key_hold = state;
}

static unsigned int is_hold_key_hold(void)
{
	return hold_key_hold;
}

static unsigned int get_current_step_key_code(void)
{
	return user_crash_key_combination[check_step].key_code;
}

static int is_key_matched_for_current_step(unsigned int code)
{
	SEC_LOG("%d == %d", code, get_current_step_key_code());
	return (code == get_current_step_key_code());
}

static int is_crash_keys(unsigned int code)
{
	int i;

	for (i = 0; i < ARRAYSIZE(key_states); i++)
		if (key_states[i].key_code == code)
			return 1;
	return 0;
}

static int get_count_for_next_step(void)
{
	int i;
	int count = 0;

	for (i = 0; i < check_step + 1; i++)
		count += user_crash_key_combination[i].crash_count;
	SEC_LOG("%d", count);
	return count;
}

static int is_reaching_count_for_next_step(void)
{
	 return (check_count == get_count_for_next_step());
}

static int get_count_for_panic(void)
{
	int i;
	int count = 0;

	for (i = 0; i < ARRAYSIZE(user_crash_key_combination); i++)
		count += user_crash_key_combination[i].crash_count;
	return count - 1;
}

static unsigned int is_key_state_down(unsigned int code)
{
	int i;

	if (is_hold_key(code))
		return is_hold_key_hold();

	for (i = 0; i < ARRAYSIZE(key_states); i++)
		if (key_states[i].key_code == code)
			return key_states[i].state == KEY_STATE_DOWN;
	/* Do not reach here */
	panic("Invalid Keycode");
}

static void set_key_state_down(unsigned int code)
{
	int i;

	if (is_hold_key(code))
		set_hold_key_hold(KEY_STATE_DOWN);

	for (i = 0; i < ARRAYSIZE(key_states); i++)
		if (key_states[i].key_code == code)
			key_states[i].state = KEY_STATE_DOWN;
	SEC_LOG("code %d", code);
}

static void set_key_state_up(unsigned int code)
{
	int i;

	if (is_hold_key(code))
		set_hold_key_hold(KEY_STATE_UP);

	for (i = 0; i < ARRAYSIZE(key_states); i++)
		if (key_states[i].key_code == code)
			key_states[i].state = KEY_STATE_UP;
}

static void increase_step(void)
{
	if (check_step < ARRAYSIZE(user_crash_key_combination))
		check_step++;
	else
		panic("User Crash key");
	SEC_LOG("%d", check_step);
}

static void reset_step(void)
{
	check_step = 0;
	SEC_LOG("");
}

static void increase_count(void)
{
	if (check_count < get_count_for_panic())
		check_count++;
	else
		panic("User Crash Key");
	SEC_LOG("%d < %d", check_count, get_count_for_panic());
}

static void reset_count(void)
{
	check_count = 0;
	SEC_LOG("");
}

void check_crash_keys_in_user(unsigned int code, int state)
{
	if (!is_crash_keys(code))
		return;

	if (state == KEY_STATE_DOWN) {
		/* Duplicated input */
		if (is_key_state_down(code))
			return;
		set_key_state_down(code);

		if (is_hold_key(code)) {
			set_hold_key_hold(KEY_STATE_DOWN);
			return;
		}
		if (is_hold_key_hold()) {
			if (is_key_matched_for_current_step(code)) {
				increase_count();
			} else {
				pr_info("%s: crash key reset\n", "sec_upload");
				reset_count();
				reset_step();
			}
			if (is_reaching_count_for_next_step())
				increase_step();
		}

	} else {
		set_key_state_up(code);
		if (is_hold_key(code)) {
			pr_info("%s: crash key reset\n", "sec_upload");
			set_hold_key_hold(KEY_STATE_UP);
			reset_step();
			reset_count();
		}
	}
}
EXPORT_SYMBOL(check_crash_keys_in_user);
