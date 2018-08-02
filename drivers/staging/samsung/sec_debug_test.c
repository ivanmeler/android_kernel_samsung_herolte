/*
 *sec_debug_test.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <soc/samsung/exynos-pmu.h>

typedef void (*force_error_func)(char *arg);

static void simulate_KP(char *arg);
static void simulate_DP(char *arg);
static void simulate_WP(char *arg);
static void simulate_TP(char *arg);
static void simulate_PANIC(char *arg);
static void simulate_BUG(char *arg);
static void simulate_WARN(char *arg);
static void simulate_DABRT(char *arg);
static void simulate_PABRT(char *arg);
static void simulate_UNDEF(char *arg);
static void simulate_DFREE(char *arg);
static void simulate_DREF(char *arg);
static void simulate_MCRPT(char *arg);
static void simulate_LOMEM(char *arg);
static void simulate_SOFT_LOCKUP(char *arg);
static void simulate_HARD_LOCKUP(char *arg);
static void simulate_SPIN_LOCKUP(char *arg);
static void simulate_PC_ABORT(char *arg);
static void simulate_SP_ABORT(char *arg);
static void simulate_JUMP_ZERO(char *arg);
static void simulate_BUSMON_ERROR(char *arg);
static void simulate_UNALIGNED(char *arg);
static void simulate_WRITE_RO(char *arg);
static void simulate_OVERFLOW(char *arg);

enum {
	FORCE_KERNEL_PANIC = 0,		/* KP */
	FORCE_WATCHDOG,			/* DP */
	FORCE_WARM_RESET,		/* WP */
	FORCE_HW_TRIPPING,		/* TP */
	FORCE_PANIC,			/* PANIC */
	FORCE_BUG,			/* BUG */
	FORCE_WARN,			/* WARN */
	FORCE_DATA_ABORT,		/* DABRT */
	FORCE_PREFETCH_ABORT,		/* PABRT */
	FORCE_UNDEFINED_INSTRUCTION,	/* UNDEF */
	FORCE_DOUBLE_FREE,		/* DFREE */
	FORCE_DANGLING_REFERENCE,	/* DREF */
	FORCE_MEMORY_CORRUPTION,	/* MCRPT */
	FORCE_LOW_MEMEMORY,		/* LOMEM */
	FORCE_SOFT_LOCKUP,		/* SOFT LOCKUP */
	FORCE_HARD_LOCKUP,		/* HARD LOCKUP */
	FORCE_SPIN_LOCKUP,		/* SPIN LOCKUP */
	FORCE_PC_ABORT,			/* PC ABORT */
	FORCE_SP_ABORT,			/* SP ABORT */
	FORCE_JUMP_ZERO,		/* JUMP TO ZERO */
	FORCE_BUSMON_ERROR,		/* BUSMON ERROR */
	FORCE_UNALIGNED,		/* UNALIGNED WRITE */
	FORCE_WRITE_RO,			/* WRITE RODATA */
	FORCE_OVERFLOW,			/* STACK OVERFLOW */
	NR_FORCE_ERROR,
};

struct force_error_item {
	char errname[SZ_32];
	force_error_func errfunc;
};

struct force_error {
	struct force_error_item item[NR_FORCE_ERROR];
};

struct force_error force_error_vector = {
	.item = {
		{"KP",		&simulate_KP},
		{"DP",		&simulate_DP},
		{"WP",		&simulate_WP},
		{"TP",		&simulate_TP},
		{"panic",	&simulate_PANIC},
		{"bug",		&simulate_BUG},
		{"warn",	&simulate_WARN},
		{"dabrt",	&simulate_DABRT},
		{"pabrt",	&simulate_PABRT},
		{"undef",	&simulate_UNDEF},
		{"dfree",	&simulate_DFREE},
		{"danglingref",	&simulate_DREF},
		{"memcorrupt",	&simulate_MCRPT},
		{"lowmem",	&simulate_LOMEM},
		{"softlockup",	&simulate_SOFT_LOCKUP},
		{"hardlockup",	&simulate_HARD_LOCKUP},
		{"spinlockup",	&simulate_SPIN_LOCKUP},
		{"pcabort",	&simulate_PC_ABORT},
		{"spabort",	&simulate_SP_ABORT},
		{"jumpzero",	&simulate_JUMP_ZERO},
		{"busmon",	&simulate_BUSMON_ERROR},
		{"unaligned",	&simulate_UNALIGNED},
		{"writero",	&simulate_WRITE_RO},
		{"overflow",	&simulate_OVERFLOW},
	}
};

static DEFINE_SPINLOCK(sec_debug_test_lock);

static int str_to_num(char *s)
{
	if (s) {
		switch (s[0]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			return (s[0]-'0');

		default:
			return -1;
		}
	}
	return -1;
}

/* timeout for dog bark/bite */
#define DELAY_TIME 30000
#define EXYNOS_PS_HOLD_CONTROL 0x330c

static void pull_down_other_cpus(void)
{
#ifdef CONFIG_HOTPLUG_CPU
	int cpu;

	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		cpu_down(cpu);
	}
#endif
}

static void simulate_KP(char *arg)
{
	pr_crit("%s()\n", __func__);
	*(unsigned int *)0x0 = 0x0; /* SVACE: intended */
}

static void simulate_DP(char *arg)
{
	pr_crit("%s()\n", __func__);
	pull_down_other_cpus();
	local_irq_disable();
	mdelay(DELAY_TIME);
	local_irq_enable();

	/* should not reach here */
	pr_crit("%s() failed\n", __func__);
}

static void simulate_WP(char *arg)
{
	unsigned int ps_hold_control;

	pr_crit("%s()\n", __func__);
	exynos_pmu_read(EXYNOS_PS_HOLD_CONTROL, &ps_hold_control);
	exynos_pmu_write(EXYNOS_PS_HOLD_CONTROL, ps_hold_control & 0xFFFFFEFF);
}

static void simulate_TP(char *arg)
{
	pr_crit("%s()\n", __func__);
}

static void simulate_PANIC(char *arg)
{
	pr_crit("%s()\n", __func__);
	panic("simulate_panic");
}

static void simulate_BUG(char *arg)
{
	pr_crit("%s()\n", __func__);
	BUG();
}

static void simulate_WARN(char *arg)
{
	pr_crit("%s()\n", __func__);
	WARN_ON(1);
}

static void simulate_DABRT(char *arg)
{
	pr_crit("%s()\n", __func__);
	*((int *) 0) = 0; /* SVACE: intended */
}

static void simulate_PABRT(char *arg)
{
	pr_crit("%s()\n", __func__);
	((void (*)(void))0x0)(); /* SVACE: intended */
}

static void simulate_UNDEF(char *arg)
{
	pr_crit("%s()\n", __func__);
	asm volatile(".word 0xe7f001f2\n\t");
	unreachable();
}

static void simulate_DFREE(char *arg)
{
	void *p;

	pr_crit("%s()\n", __func__);
	p = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (p) {
		*(unsigned int *)p = 0x0;
		kfree(p);
		msleep(1000);
		kfree(p); /* SVACE: intended */
	}
}

static void simulate_DREF(char *arg)
{
	unsigned int *p;

	pr_crit("%s()\n", __func__);
	p = kmalloc(sizeof(int), GFP_KERNEL);
	if (p) {
		kfree(p);
		*p = 0x1234; /* SVACE: intended */
	}
}

static void simulate_MCRPT(char *arg)
{
	int *ptr;

	pr_crit("%s()\n", __func__);
	ptr = kmalloc(sizeof(int), GFP_KERNEL);
	if (ptr) {
		*ptr++ = 4;
		*ptr = 2;
		panic("MEMORY CORRUPTION");
	}
}

static void simulate_LOMEM(char *arg)
{
	int i = 0;

	pr_crit("%s()\n", __func__);
	pr_crit("Allocating memory until failure!\n");
	while (kmalloc(128 * 1024, GFP_KERNEL)) /* SVACE: intended */
		i++;
	pr_crit("Allocated %d KB!\n", i * 128);
}

static void simulate_SOFT_LOCKUP(char *arg)
{
	pr_crit("%s()\n", __func__);
	softlockup_panic = 1;
	preempt_disable();
	asm("b .");
	preempt_enable();
}

static void simulate_HARD_LOCKUP_handler(void *info)
{
	asm("b .");
}

static void simulate_HARD_LOCKUP(char *arg)
{
	int cpu;

	pr_crit("%s()\n", __func__);

	if (arg) {
		cpu = str_to_num(arg);
		smp_call_function_single(cpu, simulate_HARD_LOCKUP_handler, 0, 0);
	} else {
		for_each_online_cpu(cpu) {
			if (cpu == smp_processor_id())
				continue;
			smp_call_function_single(cpu, simulate_HARD_LOCKUP_handler, 0, 0);
		}
	}
}

static void simulate_SPIN_LOCKUP(char *arg)
{
	pr_crit("%s()\n", __func__);

	spin_lock(&sec_debug_test_lock);
	spin_lock(&sec_debug_test_lock);
}

static void simulate_PC_ABORT(char *arg)
{
	pr_crit("%s()\n", __func__);
	asm("add x30, x30, #0x1\n\t"
	    "ret");
}

static void simulate_SP_ABORT(char *arg)
{
	pr_crit("%s()\n", __func__);
	asm("mov x29, #0xff00\n\t"
	    "mov sp, #0xff00\n\t"
	    "ret");
}

static void simulate_JUMP_ZERO(char *arg)
{
	pr_crit("%s()\n", __func__);
	asm("mov x0, #0x0\n\t"
	    "br x0");
}

static void simulate_BUSMON_ERROR(char *arg)
{
	pr_crit("%s()\n", __func__);

}

static void simulate_UNALIGNED(char *arg)
{
	static u8 data[5] __aligned(4) = {1, 2, 3, 4, 5};
	u32 *p;
	u32 val = 0x12345678;

	pr_crit("%s()\n", __func__);

	p = (u32 *)(data + 1);
	if (*p == 0)
		val = 0x87654321;
	*p = val;
}

static void simulate_WRITE_RO(char *arg)
{
	unsigned long *ptr;

	pr_crit("%s()\n", __func__);

	ptr = (unsigned long *)simulate_WRITE_RO;
	*ptr ^= 0x12345678;
}

#define BUFFER_SIZE SZ_1K

static int recursive_loop(int remaining)
{
	char buf[BUFFER_SIZE];

	/* Make sure compiler does not optimize this away. */
	memset(buf, (remaining & 0xff) | 0x1, BUFFER_SIZE);
	if (!remaining)
		return 0;
	else
		return recursive_loop(remaining - 1);
}

static void simulate_OVERFLOW(char *arg)
{
	pr_crit("%s()\n", __func__);

	recursive_loop(100);
}

int sec_debug_force_error(const char *val, struct kernel_param *kp)
{
	int i;
	char *temp;
	char *ptr;

	for (i = 0; i < NR_FORCE_ERROR; i++)
		if (!strncmp(val, force_error_vector.item[i].errname,
			     strlen(force_error_vector.item[i].errname))) {
			temp = (char *)val;
			ptr = strsep(&temp, " ");	/* ignore the first token */
			ptr = strsep(&temp, " ");	/* take the second token */
			force_error_vector.item[i].errfunc(ptr);
	}
	return 0;
}
EXPORT_SYMBOL(sec_debug_force_error);
