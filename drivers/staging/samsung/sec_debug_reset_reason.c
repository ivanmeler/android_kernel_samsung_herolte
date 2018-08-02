/*
 *sec_debug_reset_reason.c
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
#include <linux/sec_debug.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/stacktrace.h>

unsigned int reset_reason = RR_N;

static int __init sec_debug_set_reset_reason(char *arg)
{
	get_option(&arg, &reset_reason);
	return 0;
}
early_param("sec_debug.reset_reason", sec_debug_set_reset_reason);

static int set_debug_reset_reason_proc_show(struct seq_file *m, void *v)
{
	if (reset_reason == RR_S)
		seq_puts(m, "SPON\n");
	else if (reset_reason == RR_W)
		seq_puts(m, "WPON\n");
	else if (reset_reason == RR_D)
		seq_puts(m, "DPON\n");
	else if (reset_reason == RR_K)
		seq_puts(m, "KPON\n");
	else if (reset_reason == RR_M)
		seq_puts(m, "MPON\n");
	else if (reset_reason == RR_P)
		seq_puts(m, "PPON\n");
	else if (reset_reason == RR_R)
		seq_puts(m, "RPON\n");
	else if (reset_reason == RR_B)
		seq_puts(m, "BPON\n");
	else if (reset_reason == RR_T)
		seq_puts(m, "TPON\n");
	else
		seq_puts(m, "NPON\n");

	return 0;
}

static int sec_debug_reset_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_debug_reset_reason_proc_show, NULL);
}

static const struct file_operations sec_debug_reset_reason_proc_fops = {
	.open = sec_debug_reset_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sec_debug_reset_reason_store_lastkmsg_proc_show(struct seq_file *m, void *v)
{
	if (reset_reason == RR_K || reset_reason == RR_D || 
		reset_reason == RR_P || reset_reason == RR_S)
		seq_puts(m, "1\n");
	else
		seq_puts(m, "0\n");

	return 0;
}

static int sec_debug_reset_reason_store_lastkmsg_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_debug_reset_reason_store_lastkmsg_proc_show, NULL);
}

static const struct file_operations sec_debug_reset_reason_store_lastkmsg_proc_fops = {
	.open = sec_debug_reset_reason_store_lastkmsg_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", S_IWUGO, NULL, &sec_debug_reset_reason_proc_fops);
	if (!entry)
		return -ENOMEM;

	entry = proc_create("store_lastkmsg", S_IWUGO, NULL, &sec_debug_reset_reason_store_lastkmsg_proc_fops);

	if (!entry)
		return -ENOMEM;

	return 0;
}
device_initcall(sec_debug_reset_reason_init);
