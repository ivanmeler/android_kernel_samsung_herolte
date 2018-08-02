/*
 *sec_debug_extrainfo.c
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
#include <linux/device.h>
#include <linux/sec_debug.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/stacktrace.h>
#include <asm/esr.h>

#define SZ_96	0x00000060
#define SZ_960	0x000003c0

#define EXTRA_VERSION	"RA30"
/******************************************************************************
 * sec_debug_extra_info details
 *
 *	etr_a : basic reset information
 *	etr_b : basic reset information
 *	etr_c : hard-lockup information (callstack)
 *	etr_m : mfc error information
 *
 ******************************************************************************/

struct sec_debug_panic_extra_info sec_debug_extra_info_init = {
	.item = {
		/* basic reset information */
		{"AID",		"", SZ_32},
		{"KTIME",	"", SZ_8},
		{"BIN",		"", SZ_16},
		{"FTYPE",	"", SZ_16},
		{"FAULT",	"", SZ_32},
		{"BUG",		"", SZ_64},
		{"PANIC",	"", SZ_96},
		{"PC",		"", SZ_64},
		{"LR",		"", SZ_64},
		{"STACK",	"", SZ_256},
		{"RR",		"", SZ_8},
		{"PINFO",	"", SZ_256},
		{"SMU",		"", SZ_64},
		{"BUS",		"", SZ_128},
		{"DPM",		"", SZ_32},
		{"SMP",		"", SZ_8},
		{"ETC",		"", SZ_256},
		{"ESR",		"", SZ_64},
		{"MER",		"", SZ_16},
		{"PCB",		"", SZ_8},
		{"SMD",		"", SZ_16},
		{"CHI",		"", SZ_4},
		{"LPI",		"", SZ_4},
		{"CDI",		"", SZ_4},
		{"KLG",		"", SZ_128},
		{"HINT",	"", SZ_16},
		{"LEV",		"", SZ_4},
		{"DCN",		"", SZ_32},
		{"WAK",		"", SZ_16},
		{"BAT",		"", SZ_32},

		/* extrb reset information */
		{"BID",		"", SZ_32},
		{"BRR",		"", SZ_8},
		{"ASB",		"", SZ_32},
		{"PSITE",	"", SZ_32},
		{"DDRID",	"", SZ_32},
		{"RST",		"", SZ_32},
		{"INFO2",	"", SZ_32},
		{"INFO3",	"", SZ_32},
		{"RBASE",	"", SZ_32},
		{"MAGIC",	"", SZ_32},
		{"PWR",		"", SZ_8},
		{"PWROFF",	"", SZ_8},
		{"PINT1",	"", SZ_8},
		{"PINT2",	"", SZ_8},
		{"PINT5",	"", SZ_8},
		{"PINT6",	"", SZ_8},
		{"RVD1",	"", SZ_256},
		{"RVD2",	"", SZ_256},
		{"RVD3",	"", SZ_256},

		/* core lockup information */
		{"CID",		"", SZ_32},
		{"CRR",		"", SZ_8},
		{"CPU0",	"", SZ_256},
		{"CPU1",	"", SZ_256},
		{"CPU2",	"", SZ_256},
		{"CPU3",	"", SZ_256},
		{"CPU4",	"", SZ_256},
		{"CPU5",	"", SZ_256},
		{"CPU6",	"", SZ_256},
		{"CPU7",	"", SZ_256},

		/* core lockup information */
		{"MID",		"", SZ_32},
		{"MRR",		"", SZ_8},
		{"MFC",		"", SZ_960},
	}
};

const char *ftype_items[MAX_EXTRA_INFO_KEY_LEN] = {
	"UNDF",
	"BAD",
	"WATCH",
	"KERN",
	"MEM",
	"SPPC",
	"PAGE",
	"AUF",
	"EUF",
	"AUOF"
};

struct sec_debug_panic_extra_info *sec_debug_extra_info;
struct sec_debug_panic_extra_info *sec_debug_extra_info_backup;

#define MAX_DRAMINFO	15
static char dram_info[MAX_DRAMINFO + 1];

static int __init sec_hw_param_get_dram_info(char *arg)
{
	if (strlen(arg) > MAX_DRAMINFO)
		return 0;

	memcpy(dram_info, arg, (int)strlen(arg));

	return 0;
}

early_param("androidboot.dram_info", sec_hw_param_get_dram_info);

/******************************************************************************
 * sec_debug_init_extra_info() - function to init extra info
 *
 * This function simply initialize each filed of sec_debug_panic_extra_info.
 ******************************************************************************/

void sec_debug_init_extra_info(struct sec_debug_shared_info *sec_debug_info)
{
	if (sec_debug_info) {
		sec_debug_extra_info = &sec_debug_info->sec_debug_extra_info;
		sec_debug_extra_info_backup = &sec_debug_info->sec_debug_extra_info_backup;

		if (reset_reason == RR_K || reset_reason == RR_D || 
			reset_reason == RR_P || reset_reason == RR_S) {
			memcpy(sec_debug_extra_info_backup, sec_debug_extra_info,
			       sizeof(struct sec_debug_panic_extra_info));
		}

		memcpy(sec_debug_extra_info, &sec_debug_extra_info_init,
		       sizeof(struct sec_debug_panic_extra_info));
	}
}

/******************************************************************************
 * sec_debug_clear_extra_info() - function to clear each extra info field
 *
 * This function simply clear the specified field of sec_debug_panic_extra_info.
******************************************************************************/

void sec_debug_clear_extra_info(enum sec_debug_extra_buf_type type)
{
	if (sec_debug_extra_info)
		strcpy(sec_debug_extra_info->item[type].val, "");
}

/******************************************************************************
 * sec_debug_set_extra_info() - function to set each extra info field
 *
 * This function simply set each filed of sec_debug_panic_extra_info.
******************************************************************************/

void sec_debug_set_extra_info(enum sec_debug_extra_buf_type type,
				const char *fmt, ...)
{
	va_list args;

	if (sec_debug_extra_info) {
		if (!strlen(sec_debug_extra_info->item[type].val)) {
			va_start(args, fmt);
			vsnprintf(sec_debug_extra_info->item[type].val,
					sec_debug_extra_info->item[type].max, fmt, args);
			va_end(args);
		}
	}
}

/******************************************************************************
 * sec_debug_store_extra_info - function to export extra info
 *
 * This function finally export the extra info to destination buffer.
 * The contents of buffer will be deliverd to framework at the next booting.
 *****************************************************************************/

void sec_debug_store_extra_info(int start, int end)
{
	int i;
	char *ptr = (char *)SEC_DEBUG_EXTRA_INFO_VA;

	/* initialize extra info output buffer */
	memset((void *)SEC_DEBUG_EXTRA_INFO_VA, 0, SZ_1K);

	if (!sec_debug_extra_info_backup)
		return;

	ptr += sprintf(ptr, "\"%s\":\"%s\"", sec_debug_extra_info_backup->item[start].key,
		sec_debug_extra_info_backup->item[start].val);

	for (i = start + 1; i < end; i++) {
		if (ptr + strlen(sec_debug_extra_info_backup->item[i].key) +
				strlen(sec_debug_extra_info_backup->item[i].val) +
				MAX_EXTRA_INFO_HDR_LEN > (char *)SEC_DEBUG_EXTRA_INFO_VA + SZ_1K)
			break;

		ptr += sprintf(ptr, ",\"%s\":\"%s\"",
			sec_debug_extra_info_backup->item[i].key,
			sec_debug_extra_info_backup->item[i].val);
	}
}

/******************************************************************************
 * sec_debug_store_extra_info_A
 ******************************************************************************/

void sec_debug_store_extra_info_A(void)
{
	sec_debug_store_extra_info(INFO_AID, INFO_MAX_A);
}

/******************************************************************************
 * sec_debug_store_extra_info_B
 ******************************************************************************/

void sec_debug_store_extra_info_B(void)
{
	sec_debug_store_extra_info(INFO_BID, INFO_MAX_B);
}

/******************************************************************************
 * sec_debug_store_extra_info_C
 ******************************************************************************/

void sec_debug_store_extra_info_C(void)
{
	sec_debug_store_extra_info(INFO_CID, INFO_MAX_C);
}

/******************************************************************************
 * sec_debug_store_extra_info_M
 ******************************************************************************/

void sec_debug_store_extra_info_M(void)
{
	sec_debug_store_extra_info(INFO_MID, INFO_MAX_M);
}

/******************************************************************************
 * sec_debug_set_extra_info_id
 ******************************************************************************/

void sec_debug_set_extra_info_id(void)
{
	struct timespec ts;

	getnstimeofday(&ts);

	sec_debug_set_extra_info(INFO_AID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_BID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_CID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_MID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);

	sec_debug_set_extra_info(INFO_DDRID, "%s", dram_info);

}

/******************************************************************************
 * sec_debug_set_extra_info_ktime
******************************************************************************/

void sec_debug_set_extra_info_ktime(void)
{
	u64 ts_nsec;

	ts_nsec = local_clock();
	do_div(ts_nsec, 1000000000);
	sec_debug_set_extra_info(INFO_KTIME, "%lu", (unsigned long)ts_nsec);
}

/******************************************************************************
 * sec_debug_set_extra_info_fault
******************************************************************************/

void sec_debug_set_extra_info_fault(enum sec_debug_extra_fault_type type,
				    unsigned long addr, struct pt_regs *regs)
{
	if (regs) {
		pr_crit("%s = %s / 0x%lx\n", __func__, ftype_items[type], addr);
		sec_debug_set_extra_info(INFO_FTYPE, "%s", ftype_items[type]);
		sec_debug_set_extra_info(INFO_FAULT, "0x%lx", addr);
		sec_debug_set_extra_info(INFO_PC, "%pS", regs->pc);
		sec_debug_set_extra_info(INFO_LR, "%pS",
					 compat_user_mode(regs) ?
					 regs->compat_lr : regs->regs[30]);

	}
}

/******************************************************************************
 * sec_debug_set_extra_info_bug
******************************************************************************/

void sec_debug_set_extra_info_bug(const char *file, unsigned int line)
{
	sec_debug_set_extra_info(INFO_BUG, "%s:%u", file, line);
}

/******************************************************************************
 * sec_debug_set_extra_info_panic
******************************************************************************/

void sec_debug_set_extra_info_panic(char *str)
{
	if (strstr(str, "\nPC is at"))
		strcpy(strstr(str, "\nPC is at"), "");

	sec_debug_set_extra_info(INFO_PANIC, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_backtrace
******************************************************************************/

void sec_debug_set_extra_info_backtrace(struct pt_regs *regs)
{
	char buf[64];
	struct stackframe frame;
	int offset = 0;
	int sym_name_len;

	if (!sec_debug_extra_info)
		return;
	if (strlen(sec_debug_extra_info->item[INFO_STACK].val))
		return;

	pr_crit("sec_debug_store_backtrace\n");

	if (regs) {
		frame.fp = regs->regs[29];
		frame.sp = regs->sp;
		frame.pc = regs->pc;
	} else {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_stack_pointer;
		frame.pc = (unsigned long)sec_debug_set_extra_info_backtrace;
	}

	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(&frame);
		if (ret < 0)
			break;

		snprintf(buf, sizeof(buf), "%pf", (void *)where);
		sym_name_len = strlen(buf);

		if (offset + sym_name_len > MAX_EXTRA_INFO_VAL_LEN)
			break;

		if (offset)
			offset += sprintf((char *)sec_debug_extra_info->item[INFO_STACK].val + offset, ":");

		sprintf((char *)sec_debug_extra_info->item[INFO_STACK].val + offset, "%s", buf);
		offset += sym_name_len;
	}
}

/******************************************************************************
 * sec_debug_set_extra_info_backtrace_cpu
 ******************************************************************************/

void sec_debug_set_extra_info_backtrace_cpu(struct pt_regs *regs, int cpu)
{
	char buf[64];
	struct stackframe frame;
	int offset = 0;
	int sym_name_len;

	if (!sec_debug_extra_info)
		return;

	if (strlen(sec_debug_extra_info->item[INFO_CPU0 + (cpu % 8)].val))
		return;

	pr_crit("sec_debug_store_backtrace_cpu(%d)\n", cpu);

	if (regs) {
		frame.fp = regs->regs[29];
		frame.sp = regs->sp;
		frame.pc = regs->pc;
	} else {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_stack_pointer;
		frame.pc = (unsigned long)sec_debug_set_extra_info_backtrace_cpu;
	}

	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(&frame);
		if (ret < 0)
			break;

		snprintf(buf, sizeof(buf), "%pf", (void *)where);
		sym_name_len = strlen(buf);
		if (offset + sym_name_len > MAX_EXTRA_INFO_VAL_LEN)
			break;

		if (offset)
			offset += sprintf((char *)sec_debug_extra_info->item[INFO_CPU0 + (cpu % 8)].val + offset, ":");

		sprintf((char *)sec_debug_extra_info->item[INFO_CPU0 + (cpu % 8)].val + offset, "%s", buf);
		offset += sym_name_len;
	}
}

/******************************************************************************
 * sec_debug_set_extra_info_sysmmu
******************************************************************************/

void sec_debug_set_extra_info_sysmmu(char *str)
{
	sec_debug_set_extra_info(INFO_SYSMMU, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_sysmmu
******************************************************************************/

void sec_debug_set_extra_info_busmon(char *str)
{
	sec_debug_set_extra_info(INFO_BUSMON, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_dpm_timeout
******************************************************************************/

void sec_debug_set_extra_info_dpm_timeout(char *devname)
{
	sec_debug_set_extra_info(INFO_DPM, "%s", devname);
}

/******************************************************************************
 * sec_debug_set_extra_info_smpl
******************************************************************************/

void sec_debug_set_extra_info_smpl(unsigned long count)
{
	sec_debug_clear_extra_info(INFO_SMPL);
	sec_debug_set_extra_info(INFO_SMPL, "%lu", count);
}

/******************************************************************************
 * sec_debug_set_extra_info_ufs_error
******************************************************************************/

void sec_debug_set_extra_info_ufs_error(char *str)
{
	sec_debug_set_extra_info(INFO_ETC, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_zswap
******************************************************************************/

void sec_debug_set_extra_info_zswap(char *str)
{
	sec_debug_set_extra_info(INFO_ETC, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_mfc_error
******************************************************************************/

void sec_debug_set_extra_info_mfc_error(char *str)
{
	sec_debug_clear_extra_info(INFO_STACK); /* erase STACK */
	sec_debug_set_extra_info(INFO_STACK, "MFC ERROR");
	sec_debug_set_extra_info(INFO_MFC, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_esr
******************************************************************************/

void sec_debug_set_extra_info_esr(unsigned int esr)
{
	sec_debug_set_extra_info(INFO_ESR, "%s (0x%08x)",
				esr_get_class_string(esr), esr);
}

/******************************************************************************
 * sec_debug_set_extra_info_merr
******************************************************************************/

void sec_debug_set_extra_info_merr(void)
{
	sec_debug_set_extra_info(INFO_MERR, "0x%x", merr_symptom);
}

/******************************************************************************
 * sec_debug_set_extra_info_hint
 ******************************************************************************/

void sec_debug_set_extra_info_hint(u64 hint)
{
	if (hint)
		sec_debug_set_extra_info(INFO_HINT, "%llx", hint);
}

/******************************************************************************
 * sec_debug_set_extra_info_decon
******************************************************************************/

void sec_debug_set_extra_info_decon(unsigned int err)
{
	sec_debug_set_extra_info(INFO_DECON, "%08x", err);
}

/******************************************************************************
 * sec_debug_set_extra_info_batt
******************************************************************************/

void sec_debug_set_extra_info_batt(int cap, int volt, int temp, int curr)
{
	sec_debug_clear_extra_info(INFO_BATT);
	sec_debug_set_extra_info(INFO_BATT, "%03d/%04d/%04d/%06d", cap, volt, temp, curr);
}

void sec_debug_finish_extra_info(void)
{
	sec_debug_set_extra_info_ktime();
	sec_debug_set_extra_info_merr();
}

static int set_debug_reset_extra_info_proc_show(struct seq_file *m, void *v)
{
	char buf[SZ_1K];

	if (reset_reason == RR_K || reset_reason == RR_D || 
		reset_reason == RR_P || reset_reason == RR_S) {
		sec_debug_store_extra_info_A();
		memcpy(buf, (char *)SEC_DEBUG_EXTRA_INFO_VA, SZ_1K);

		seq_printf(m, buf);
	} else {
		return -ENOENT;
	}

	return 0;
}

static int sec_debug_reset_extra_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_debug_reset_extra_info_proc_show, NULL);
}

static const struct file_operations sec_debug_reset_extra_info_proc_fops = {
	.open = sec_debug_reset_extra_info_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sec_debug_reset_extra_info_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason_extra_info",
			    0644, NULL, &sec_debug_reset_extra_info_proc_fops);
	if (!entry)
		return -ENOMEM;
	proc_set_size(entry, SZ_1K);

	sec_debug_set_extra_info_id();
	return 0;
}
device_initcall(sec_debug_reset_extra_info_init);
