/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kmsg_dump.h>
#include <linux/kallsyms.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/tick.h>
#include <linux/sec_ext.h>
#include <linux/sec_debug.h>
#include <linux/sec_debug_hard_reset_hook.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/mount.h>

#include <asm/cacheflush.h>

#include <soc/samsung/exynos-pmu.h>

#ifdef CONFIG_SEC_DEBUG

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union sec_debug_level_t {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
} sec_debug_level = { .en.kernel_fault = 1, };
module_param_named(enable, sec_debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, sec_debug_level.en.user_fault, ushort, 0644);
module_param_named(level, sec_debug_level.uint_val, uint, 0644);

int sec_debug_get_debug_level(void)
{
	return sec_debug_level.uint_val;
}

static void sec_debug_user_fault_dump(void)
{
	if (sec_debug_level.en.kernel_fault == 1 &&
	    sec_debug_level.en.user_fault == 1)
		panic("User Fault");
}

static ssize_t sec_debug_user_fault_write(struct file *file, const char __user *buffer, size_t count, loff_t *offs)
{
	char buf[100];

	if (count > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	if (strncmp(buf, "dump_user_fault", 15) == 0)
		sec_debug_user_fault_dump();

	return count;
}

static const struct file_operations sec_debug_user_fault_proc_fops = {
	.write = sec_debug_user_fault_write,
};

static int __init sec_debug_user_fault_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("user_fault", S_IWUSR | S_IWGRP, NULL,
			    &sec_debug_user_fault_proc_fops);
	if (!entry)
		return -ENOMEM;
	return 0;
}
device_initcall(sec_debug_user_fault_init);

/* layout of SDRAM : First 4KB of DRAM
*         0x0: magic            (4B)
*   0x4~0x3FF: panic string     (1020B)
* 0x400~0x7FF: panic Extra Info (1KB)
* 0x800~0xFFB: panic dumper log (2KB - 4B)
*       0xFFC: copy of magic    (4B)
*/

enum sec_debug_upload_magic_t {
	UPLOAD_MAGIC_INIT		= 0x0,
	UPLOAD_MAGIC_PANIC		= 0x66262564,
};

enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT		= 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC	= 0x000000C8,
	UPLOAD_CAUSE_FORCED_UPLOAD	= 0x00000022,
#ifdef CONFIG_SEC_UPLOAD
	UPLOAD_CAUSE_USER_FORCED_UPLOAD	= 0x00000074,
#endif
	UPLOAD_CAUSE_CP_ERROR_FATAL	= 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT		= 0x0000002F,
	UPLOAD_CAUSE_HSIC_DISCONNECTED	= 0x000000DD,
	UPLOAD_CAUSE_POWERKEY_LONG_PRESS = 0x00000085,
	UPLOAD_CAUSE_HARD_RESET	= 0x00000066,
};

static void sec_debug_set_upload_magic(unsigned magic, char *str)
{
	*(unsigned int *)SEC_DEBUG_MAGIC_VA = magic;
	*(unsigned int *)(SEC_DEBUG_MAGIC_VA + SZ_4K - 4) = magic;

	if (str) {
		strncpy((char *)SEC_DEBUG_MAGIC_VA + 4, str, SZ_1K - 4);

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
		sec_debug_set_extra_info_panic(str);
		sec_debug_finish_extra_info();
#endif
	}
	pr_emerg("sec_debug: set magic code (0x%x)\n", magic);
}

static void sec_debug_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	exynos_pmu_write(EXYNOS_PMU_INFORM3, type);

	pr_emerg("sec_debug: set upload cause (0x%x)\n", type);
}

static void sec_debug_kmsg_dump(struct kmsg_dumper *dumper, enum kmsg_dump_reason reason)
{
	kmsg_dump_get_buffer(dumper, true, (char *)SEC_DEBUG_DUMPER_LOG_VA, SZ_2K - 4, NULL);
}

static struct kmsg_dumper sec_dumper = {
	.dump = sec_debug_kmsg_dump,
};

static int sec_debug_reserved(phys_addr_t base, phys_addr_t size)
{
#ifdef CONFIG_NO_BOOTMEM
	return (memblock_is_region_reserved(base, size) ||
		memblock_reserve(base, size));
#else
	return reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE);
#endif
}

int __init sec_debug_init(void)
{
	phys_addr_t size = SZ_4K;
	phys_addr_t base = 0;

	base = SEC_DEBUG_MAGIC_PA;

	/* clear traps info */
	memset((void *)SEC_DEBUG_MAGIC_VA + 4, 0, SZ_1K - 4);

	if (!sec_debug_reserved(base, size)) {
		pr_info("%s: Reserved Mem(0x%llx, 0x%llx) - Success\n",
			__FILE__, base, size);

		sec_debug_set_upload_magic(UPLOAD_MAGIC_PANIC, NULL);
	} else {
		goto out;
	}

	kmsg_dump_register(&sec_dumper);

	return 0;
out:
	pr_err("%s: Reserved Mem(0x%llx, 0x%llx) - Failed\n", __FILE__, base, size);
	return -ENOMEM;
}

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif
#ifdef arch_idle_time
static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}
#else
static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = usecs_to_cputime64(iowait_time);

	return iowait;
}
#endif

static void sec_debug_dump_cpu_stat(void)
{
	int i, j;
	u64 user = 0;
	u64 nice = 0;
	u64 system = 0;
	u64 idle = 0;
	u64 iowait = 0;
	u64 irq = 0;
	u64 softirq = 0;
	u64 steal = 0;
	u64 guest = 0;
	u64 guest_nice = 0;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};

	char *softirq_to_name[NR_SOFTIRQS] = { "HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "BLOCK_IOPOLL", "TASKLET", "SCHED", "HRTIMER", "RCU" };

	for_each_possible_cpu(i) {
		user	+= kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice	+= kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system	+= kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle	+= get_idle_time(i);
		iowait	+= get_iowait_time(i);
		irq	+= kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq	+= kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal	+= kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest	+= kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		sum	+= kstat_cpu_irqs_sum(i);
		sum	+= arch_irq_stat_cpu(i);

		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);

			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();

	pr_info("\n");
	pr_info("cpu   user:%llu \tnice:%llu \tsystem:%llu \tidle:%llu \tiowait:%llu \tirq:%llu \tsoftirq:%llu \t %llu %llu %llu\n",
			(unsigned long long)cputime64_to_clock_t(user),
			(unsigned long long)cputime64_to_clock_t(nice),
			(unsigned long long)cputime64_to_clock_t(system),
			(unsigned long long)cputime64_to_clock_t(idle),
			(unsigned long long)cputime64_to_clock_t(iowait),
			(unsigned long long)cputime64_to_clock_t(irq),
			(unsigned long long)cputime64_to_clock_t(softirq),
			(unsigned long long)cputime64_to_clock_t(steal),
			(unsigned long long)cputime64_to_clock_t(guest),
			(unsigned long long)cputime64_to_clock_t(guest_nice));
	pr_info("-------------------------------------------------------------------------------------------------------------\n");

	for_each_possible_cpu(i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user	= kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice	= kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system	= kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle	= get_idle_time(i);
		iowait	= get_iowait_time(i);
		irq	= kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq	= kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal	= kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest	= kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice = kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];

		pr_info("cpu%d  user:%llu \tnice:%llu \tsystem:%llu \tidle:%llu \tiowait:%llu \tirq:%llu \tsoftirq:%llu \t %llu %llu %llu\n",
			i,
			(unsigned long long)cputime64_to_clock_t(user),
			(unsigned long long)cputime64_to_clock_t(nice),
			(unsigned long long)cputime64_to_clock_t(system),
			(unsigned long long)cputime64_to_clock_t(idle),
			(unsigned long long)cputime64_to_clock_t(iowait),
			(unsigned long long)cputime64_to_clock_t(irq),
			(unsigned long long)cputime64_to_clock_t(softirq),
			(unsigned long long)cputime64_to_clock_t(steal),
			(unsigned long long)cputime64_to_clock_t(guest),
			(unsigned long long)cputime64_to_clock_t(guest_nice));
	}
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info("irq : %llu", (unsigned long long)sum);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	/* sum again ? it could be updated? */
	for_each_irq_nr(j) {
		unsigned int irq_stat = kstat_irqs(j);

		if (irq_stat) {
			struct irq_desc *desc = irq_to_desc(j);
#if defined(CONFIG_SEC_DEBUG_PRINT_IRQ_PERCPU)
			pr_info("irq-%-4d : %8u :", j, irq_stat);
			for_each_possible_cpu(i)
				pr_info(" %8u", kstat_irqs_cpu(j, i));
			pr_info(" %s", desc->action ? desc->action->name ? : "???" : "???");
			pr_info("\n");
#else
			pr_info("irq-%-4d : %8u %s\n", j, irq_stat,
				desc->action ? desc->action->name ? : "???" : "???");
#endif
		}
	}
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info("softirq : %llu", (unsigned long long)sum_softirq);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	for (i = 0; i < NR_SOFTIRQS; i++)
		if (per_softirq_sums[i])
			pr_info("softirq-%d : %8u %s\n", i, per_softirq_sums[i], softirq_to_name[i]);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
}

void sec_debug_reboot_handler(void)
{
	/* Clear magic code in normal reboot */
	sec_debug_set_upload_magic(UPLOAD_MAGIC_INIT, NULL);
}

void sec_debug_panic_handler(void *buf, bool dump)
{
	/* Set upload cause */
	sec_debug_set_upload_magic(UPLOAD_MAGIC_PANIC, buf);
	if (!strncmp(buf, "User Fault", 10))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (is_hard_reset_occurred())
		sec_debug_set_upload_cause(UPLOAD_CAUSE_HARD_RESET);
	else if (!strncmp(buf, "Crash Key", 9))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
#ifdef CONFIG_SEC_UPLOAD
	else if (!strncmp(buf, "User Crash Key", 14))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FORCED_UPLOAD);
#endif
	else if (!strncmp(buf, "CP Crash", 8))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strncmp(buf, "HSIC Disconnected", 17))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_HSIC_DISCONNECTED);
	else
		sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	/* dump debugging info */
	if (dump) {
		sec_debug_dump_cpu_stat();
		debug_show_all_locks();
	}

	/* hw reset */
	pr_emerg("sec_debug: %s\n", linux_banner);
	pr_emerg("sec_debug: rebooting...\n");

	flush_cache_all();
}


#ifdef CONFIG_SEC_DEBUG_FILE_LEAK
int sec_debug_print_file_list(void)
{
	int i = 0;
	unsigned int count = 0;
	struct file *file = NULL;
	struct files_struct *files = current->files;
	const char *p_rootname = NULL;
	const char *p_filename = NULL;
	int ret=0;

	count = files->fdt->max_fds;

	pr_err("[Opened file list of process %s(PID:%d, TGID:%d) :: %d]\n",
	       current->group_leader->comm, current->pid, current->tgid, count);

	for (i = 0; i < count; i++) {
		rcu_read_lock();
		file = fcheck_files(files, i);

		p_rootname = NULL;
		p_filename = NULL;

		if (file) {
			if (file->f_path.mnt && file->f_path.mnt->mnt_root &&
			    file->f_path.mnt->mnt_root->d_name.name)
				p_rootname = file->f_path.mnt->mnt_root->d_name.name;

			if (file->f_path.dentry && file->f_path.dentry->d_name.name)
				p_filename = file->f_path.dentry->d_name.name;

			pr_err("[%04d]%s%s\n", i, p_rootname ? p_rootname : "null",
			       p_filename ? p_filename : "null");
			ret++;
		}
		rcu_read_unlock();
	}
	if(ret > count - 50)
		return 1;
	else
		return 0;
}

void sec_debug_EMFILE_error_proc(unsigned long files_addr)
{
	if (files_addr != (unsigned long)(current->files)) {
		pr_err("Too many open files Error at %pS\n"
		       "%s(%d) thread of %s process tried fd allocation by proxy.\n"
		       "files_addr = 0x%lx, current->files=0x%p\n",
		       __builtin_return_address(0),
		       current->comm, current->tgid, current->group_leader->comm,
		       files_addr, current->files);
		return;
	}

	pr_err("Too many open files(%d:%s) at %pS\n",
	       current->tgid, current->group_leader->comm, __builtin_return_address(0));

	if (!sec_debug_level.en.kernel_fault)
		return;

	/* We check EMFILE error in only "system_server","mediaserver" and "surfaceflinger" process.*/
	if (!strcmp(current->group_leader->comm, "system_server") ||
	    !strcmp(current->group_leader->comm, "mediaserver") ||
	    !strcmp(current->group_leader->comm, "surfaceflinger")) {
		if (sec_debug_print_file_list() == 1) {
			panic("Too many open files");
		}
	}
}
#endif /* CONFIG_SEC_DEBUG_FILE_LEAK */

/* leave the following definithion of module param call here for the compatibility with other models */
module_param_call(force_error, sec_debug_force_error, NULL, NULL, 0644);

static struct sec_debug_shared_info *sec_debug_info;

static void sec_debug_init_base_buffer(unsigned long base, unsigned long size)
{
	sec_debug_info = (struct sec_debug_shared_info *)phys_to_virt(base);

	if (sec_debug_info) {

		sec_debug_info->magic[0] = SEC_DEBUG_SHARED_MAGIC0;
		sec_debug_info->magic[1] = SEC_DEBUG_SHARED_MAGIC1;
		sec_debug_info->magic[2] = SEC_DEBUG_SHARED_MAGIC2;
		sec_debug_info->magic[3] = SEC_DEBUG_SHARED_MAGIC3;

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
		sec_debug_set_kallsyms_info(sec_debug_info);
		sec_debug_init_extra_info(sec_debug_info);
#endif
	}
	pr_info("%s, base(virt):0x%lx size:0x%lx\n", __func__, (unsigned long)sec_debug_info, size);

}

static int __init sec_debug_base_setup(char *str)
{
	unsigned long size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || *str != '@' || kstrtoul(str + 1, 0, &base)) {
		pr_err("%s: failed to parse address.\n", __func__);
		goto out;
	}

	if (sec_debug_reserved(base, size)) {
		/* size is not match with -size and size + sizeof(...) */
		pr_err("%s: failed to reserve size:0x%lx at base 0x%lx\n",
		       __func__, size, base);
		goto out;
	}

	sec_debug_init_base_buffer(base, size);

	pr_info("%s, base(phys):0x%lx size:0x%lx\n", __func__, base, size);
out:
	return 0;
}
__setup("sec_debug.base=", sec_debug_base_setup);

#ifdef CONFIG_SEC_UPLOAD
extern void check_crash_keys_in_user(unsigned int code, int onoff);
#endif

void sec_debug_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	static const unsigned int VOLUME_UP = KEY_VOLUMEUP;
	static const unsigned int VOLUME_DOWN = KEY_VOLUMEDOWN;

	hard_reset_hook(code, value);
	
	if (!sec_debug_level.en.kernel_fault) {
#ifdef CONFIG_SEC_UPLOAD
		check_crash_keys_in_user(code, value);
#endif
		return;
	}

	if (code == KEY_POWER)
		pr_info("%s: POWER-KEY(%d)\n", __FILE__, value);

	/* Enter Forced Upload
	 *  Hold volume down key first
	 *  and then press power key twice
	 *  and volume up key should not be pressed
	 */
	if (value) {
		if (code == VOLUME_UP)
			volup_p = true;
		if (code == VOLUME_DOWN)
			voldown_p = true;
		if (!volup_p && voldown_p) {
			if (code == KEY_POWER) {
				pr_info("%s: Forced Upload Count: %d\n",
					__FILE__, ++loopcount);
				if (loopcount == 2)
					panic("Crash Key");
			}
		}

#if defined(CONFIG_SEC_DEBUG_TSP_LOG) && (defined(CONFIG_TOUCHSCREEN_FTS) || defined(CONFIG_TOUCHSCREEN_SEC_TS))
		/* dump TSP rawdata
		 *	Hold volume up key first
		 *	and then press home key twice
		 *	and volume down key should not be pressed
		 */
		if (volup_p && !voldown_p) {
			if (code == KEY_HOMEPAGE) {
				pr_info("%s: count to dump tsp rawdata : %d\n",
					 __func__, ++loopcount);
				if (loopcount == 2) {
#if defined(CONFIG_TOUCHSCREEN_FTS)
					tsp_dump();
#elif defined(CONFIG_TOUCHSCREEN_SEC_TS)
					tsp_dump_sec();
#endif
					loopcount = 0;
				}
			}
		}
#endif
	} else {
		if (code == VOLUME_UP)
			volup_p = false;
		if (code == VOLUME_DOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}

#if defined(CONFIG_BOOTING_VALIDATION)
static bool sec_debug_valid_boot(void)
{
	unsigned int v;

	exynos_pmu_read(EXYNOS_PMU_INFORM2, &v);

	if (v == 0xF00DCAFE)
		return true;
	else
		return false;
}

int sec_debug_valid_boot_init(void)
{
	if (!sec_debug_valid_boot())
		BUG();

	return 0;
}
late_initcall(sec_debug_valid_boot_init);
#endif	/* CONFIG_BOOTING_VALIDATION */

#endif /* CONFIG_SEC_DEBUG */
