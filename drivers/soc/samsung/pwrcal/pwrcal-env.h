#ifndef __PWRCAL_INCLUDE_H__
#define __PWRCAL_INCLUDE_H__

#ifdef CONFIG_PWRCAL

#define PWRCAL_TARGET_LINUX

#include <linux/spinlock.h>
#include <linux/math64.h>
#include <linux/smc.h>
#include <linux/delay.h>

#else

#define PWRCAL_TARGET_FW

#include "types.h"
#include "console.h"
#include "kernel/spinlock.h"
#include <string.h>
#include <common.h>
#include <kernel/timer.h>
#include <kernel/panic.h>
#include <compat.h>

#define pr_err(_msg, args...)	\
	console_printf(0, "\033[1;31;5merror::func=%s, "_msg"\033[0m\n", \
							__func__, ##args);
#define pr_warn(_msg, args...)	\
	console_printf(1, "\033[1;31;5mwarning::func=%s, "_msg"\033[0m\n", \
							__func__, ##args);
#define pr_info(_msg, args...)	\
	console_printf(4, _msg, ##args)

#define do_div(a, b)		(a /= b)

#define spin_lock_init(x)	initialize_spinlock(x)
#define cpu_relax()		udelay(1)
#endif

#endif
