/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-SnapShot debugging framework for Exynos SoC
 *
 * Author: Hosung Kim <Hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/ktime.h>
#include <linux/printk.h>
#include <linux/exynos-ss.h>
#include <linux/kallsyms.h>
#include <linux/platform_device.h>
#include <linux/pstore_ram.h>
#include <linux/clk-private.h>
#include <linux/clockchips.h>
#ifdef CONFIG_SEC_PM_DEBUG
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#endif
#include <linux/input.h>
#include <linux/of_address.h>
#include <linux/smc.h>
#include <linux/exynos-busmon.h>

#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/ptrace.h>
#include <asm/memory.h>
#include <asm/map.h>
#include <asm/mmu.h>
#include <asm/smp_plat.h>
#include <soc/samsung/exynos-pmu.h>

#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#include <linux/sec_debug_hard_reset_hook.h>

extern void (*mach_restart)(char mode, const char *cmd);
#endif

#ifdef CONFIG_SEC_BSP
#include <linux/sec_bsp.h>
#endif

extern int dhd_dongle_mem_dump(void);

/*  Size domain */
#define ESS_KEEP_HEADER_SZ		(SZ_256 * 3)
#define ESS_HEADER_SZ			SZ_4K
#define ESS_MMU_REG_SZ			SZ_4K
#define ESS_CORE_REG_SZ			SZ_4K
#define ESS_HEADER_TOTAL_SZ		(ESS_HEADER_SZ + ESS_MMU_REG_SZ + ESS_CORE_REG_SZ)
#define ESS_HEADER_ALLOC_SZ		SZ_2M

/*  Length domain */
#define ESS_LOG_STRING_LENGTH		SZ_128
#define ESS_MMU_REG_OFFSET		SZ_512
#define ESS_CORE_REG_OFFSET		SZ_512
#define ESS_CORE_PC_OFFSET		0x600
#define ESS_LOG_MAX_NUM			SZ_1K
#define ESS_API_MAX_NUM			SZ_2K
#define ESS_EX_MAX_NUM			SZ_8
#define ESS_IN_MAX_NUM			SZ_8
#define ESS_CALLSTACK_MAX_NUM		4
#define ESS_ITERATION			5
#define ESS_NR_CPUS			NR_CPUS
#define ESS_ITEM_MAX_NUM		10

/* Sign domain */
#define ESS_SIGN_RESET			0x0
#define ESS_SIGN_RESERVED		0x1
#define ESS_SIGN_SCRATCH		0xD
#define ESS_SIGN_ALIVE			0xFACE
#define ESS_SIGN_DEAD			0xDEAD
#define ESS_SIGN_PANIC			0xBABA
#define ESS_SIGN_SAFE_FAULT		0xFAFA
#define ESS_SIGN_NORMAL_REBOOT		0xCAFE
#define ESS_SIGN_FORCE_REBOOT		0xDAFE

/*  Specific Address Information */
#define ESS_FIXED_VIRT_BASE		(VMALLOC_START + 0xF6000000)
#define ESS_OFFSET_SCRATCH		(0x100)
#define ESS_OFFSET_LAST_LOGBUF		(0x200)
#define ESS_OFFSET_EMERGENCY_REASON	(0x300)
#define ESS_OFFSET_CORE_POWER_STAT	(0x400)
#define ESS_OFFSET_PANIC_STAT		(0x500)
#define ESS_OFFSET_LAST_PC		(0x600)

/* S5P_VA_SS_BASE + 0xC00 -- 0xFFF is reserved */
#define ESS_OFFSET_PANIC_STRING 	(0xC00)
#define ESS_OFFSET_SPARE_BASE		(ESS_HEADER_SZ + ESS_MMU_REG_SZ + ESS_CORE_REG_SZ)

#define mpidr_cpu_num(mpidr)			\
	( MPIDR_AFFINITY_LEVEL(mpidr, 1) << 2	\
	 | MPIDR_AFFINITY_LEVEL(mpidr, 0))


int ess_boot_logging = 5000;

struct exynos_ss_base {
	size_t size;
	size_t vaddr;
	size_t paddr;
	unsigned int persist;
	unsigned int enabled;
	unsigned int enabled_init;
};

struct exynos_ss_item {
	char *name;
	struct exynos_ss_base entry;
	unsigned char *head_ptr;
	unsigned char *curr_ptr;
	unsigned long long time;
};

struct exynos_ss_log {
	struct task_log {
		unsigned long long time;
		unsigned long ttbr0_el1;
		struct task_struct *task;
		char *task_comm;

	} task[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct task_log task_boot[ESS_NR_CPUS][ESS_LOG_MAX_NUM * 2];

	struct work_log {
		unsigned long long time;
		unsigned long sp;
		struct worker *worker;
		struct work_struct *work;
		work_func_t fn;
		int en;
	} work[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct cpuidle_log {
		unsigned long long time;
		unsigned long sp;
		char *modes;
		unsigned state;
		u32 num_online_cpus;
		int delta;
		int en;
	} cpuidle[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct suspend_log {
		unsigned long long time;
		unsigned long sp;
		void *fn;
		struct device *dev;
		int en;
	} suspend[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct irq_log {
		unsigned long long time;
		unsigned long sp;
		int irq;
		void *fn;
		unsigned long val;
		unsigned int preempt;
		int en;
	} irq[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	struct irq_exit_log {
		unsigned long long time;
		unsigned long sp;
		unsigned long long end_time;
		unsigned long long latency;
		int irq;
	} irq_exit[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
	struct spinlock_log {
		unsigned long long time;
		unsigned long sp;
		unsigned long long jiffies;
		struct task_struct *owner;
		char *task_comm;
		unsigned int owner_cpu;
		int en;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} spinlock[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
	struct irqs_disabled_log {
		unsigned long long time;
		unsigned long index;
		struct task_struct *task;
		char *task_comm;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} irqs_disabled[ESS_NR_CPUS][SZ_32];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	struct clk_log {
		unsigned long long time;
		struct vclk *clk;
		const char* f_name;
		int mode;
	} clk[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
	struct freq_log {
		unsigned long long time;
		int cpu;
		char* freq_name;
		unsigned long old_freq;
		unsigned long target_freq;
		int en;
	} freq[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	struct reg_log {
		unsigned long long time;
		int read;
		size_t val;
		size_t reg;
		int en;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} reg[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
	struct hrtimer_log {
		unsigned long long time;
		unsigned long long now;
		struct hrtimer *timer;
		void *fn;
		int en;
	} hrtimers[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
	struct regulator_log {
		unsigned long long time;
		int cpu;
		char name[SZ_16];
		unsigned int reg;
		unsigned int voltage;
		int en;
	} regulator[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	struct thermal_log {
		unsigned long long time;
		int cpu;
		struct exynos_tmu_platform_data *data;
		unsigned int temp;
		char* cooling_device;
		unsigned int cooling_state;
	} thermal[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_MBOX
	struct mailbox_log {
		unsigned long long time;
		unsigned int buf[4];
		int mode;
		int cpu;
		char* name;
		unsigned int atl_vol;
		unsigned int apo_vol;
		unsigned int g3d_vol;
		unsigned int mif_vol;
	} mailbox[ESS_LOG_MAX_NUM];
#endif
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	struct clockevent_log {
		unsigned long long time;
		struct clock_event_device* dev;
		unsigned long long clc;
		int64_t	delta;
		ktime_t	next_event;
	} clockevent[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct printkl_log {
		unsigned long long time;
		int cpu;
		size_t msg;
		size_t val;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} printkl[ESS_API_MAX_NUM];

	struct printk_log {
		unsigned long long time;
		int cpu;
		char log[ESS_LOG_STRING_LENGTH];
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} printk[ESS_API_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT
	struct core_log {
		void *last_pc[ESS_ITERATION];
	} core[ESS_NR_CPUS];
#endif
};

struct exynos_ss_log_idx {
	atomic_t task_log_idx[ESS_NR_CPUS];
	atomic_t task_boot_idx[ESS_NR_CPUS];
	atomic_t work_log_idx[ESS_NR_CPUS];
	atomic_t cpuidle_log_idx[ESS_NR_CPUS];
	atomic_t suspend_log_idx[ESS_NR_CPUS];
	atomic_t irq_log_idx[ESS_NR_CPUS];
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
	atomic_t spinlock_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
	atomic_t irqs_disabled_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	atomic_t irq_exit_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	atomic_t reg_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
	atomic_t hrtimer_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	atomic_t clk_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
	atomic_t freq_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
	atomic_t regulator_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
	atomic_t thermal_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_MBOX
	atomic_t mailbox_log_idx;
#endif
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	atomic_t clockevent_log_idx[ESS_NR_CPUS];
	atomic_t printkl_log_idx;
	atomic_t printk_log_idx;
#endif
};
#ifdef CONFIG_ARM64
struct exynos_ss_mmu_reg {
	long SCTLR_EL1;
	long TTBR0_EL1;
	long TTBR1_EL1;
	long TCR_EL1;
	long ESR_EL1;
	long FAR_EL1;
	long CONTEXTIDR_EL1;
	long TPIDR_EL0;
	long TPIDRRO_EL0;
	long TPIDR_EL1;
	long MAIR_EL1;
};

#else
struct exynos_ss_mmu_reg {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP
struct exynos_ss_sfrdump {
	char *name;
	void __iomem *reg;
	unsigned int phy_reg;
	unsigned int num;
	struct device_node *node;
	struct list_head list;
#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP_ADV
	int pwr_mode;
#endif
};

#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP_ADV
enum pmumodes {
	PWR_NONE = 0,
	PWR_REGULAR = 1,
	PWR_GPU = 2,
	PWR_TYPES,
};
struct exynos_ss_sfrdump_func {
	char *name;
	bool (*fn)(struct exynos_ss_sfrdump *sfrdump,
				const struct device_node *np,
				void *io_base);
};
#endif

#endif

struct exynos_ss_desc {
	struct list_head sfrdump_list;
#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP_ADV
	void __iomem *sfrdump_pmu;
	u32 sfrdump_phy_pmu;
	u32 sfrdump_phy_pmu_end;
#endif

	raw_spinlock_t lock;
	unsigned int kevents_num;
	unsigned int log_kernel_num;
	unsigned int log_platform_num;
	unsigned int log_sfr_num;
	unsigned int log_pstore_num;
	unsigned int log_etm_num;
	bool need_header;

	unsigned int callstack;
	unsigned long hardlockup_core_mask;
	unsigned long hardlockup_core_pc[ESS_NR_CPUS];
	int hardlockup;
	int no_wdt_dev;

	struct vm_struct vm;
};

struct exynos_ss_interface {
	struct exynos_ss_log *info_event;
	struct exynos_ss_item info_log[ESS_ITEM_MAX_NUM];
};

#ifdef CONFIG_S3C2410_WATCHDOG
extern int s3c2410wdt_set_emergency_stop(void);
extern int s3c2410wdt_set_emergency_reset(unsigned int timeout);
extern int s3c2410wdt_keepalive_emergency(void);
#else
#define s3c2410wdt_set_emergency_stop() 	(-1)
#define s3c2410wdt_set_emergency_reset(a)	do { } while(0)
#define s3c2410wdt_keepalive_emergency()	do { } while(0)
#endif
extern void *return_address(int);
extern void (*arm_pm_restart)(char str, const char *cmd);
#ifdef CONFIG_EXYNOS_CORESIGHT_PC_INFO
extern unsigned long exynos_cs_pc[NR_CPUS][ESS_ITERATION];
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
extern void register_hook_logbuf(void (*)(const char));
#else
extern void register_hook_logbuf(void (*)(const char *, size_t));
#endif
extern void register_hook_logger(void (*)(const char *, const char *, size_t));
#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
void register_hook_pstore_log(void (*func)(const char *name, const char *buf, size_t size));
#endif

extern int exynos_check_hardlockup_reason(void);

typedef int (*ess_initcall_t)(const struct device_node *);

#ifdef CONFIG_SEC_PM_DEBUG
static bool sec_log_full;
#endif

#ifdef CONFIG_SEC_DEBUG_AUTO_SUMMARY
static void (*func_hook_auto_comm_lastfreq)(int type, int old_freq, int new_freq, u64 time);
#endif

/*
 * ---------------------------------------------------------------------------
 *  User defined Start
 * ---------------------------------------------------------------------------
 *
 *  clarified exynos-snapshot items, before using exynos-snapshot we should
 *  evince memory-map of snapshot
 */
static struct exynos_ss_item ess_items[] = {
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	{"log_kevents",	{SZ_8M,		0, 0, false, true, true}, NULL ,NULL, 0},
	{"log_kernel",	{SZ_2M,		0, 0, false, true, true}, NULL ,NULL, 0},
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	{"log_main",	{SZ_4M,		0, 0, false, true, true}, NULL ,NULL, 0},
#endif
#ifdef CONFIG_ANDROID_LOGGER
	{"log_radio",	{SZ_2M,		0, 0, false, true, true}, NULL ,NULL, 0},
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
	{"log_pstore",	{SZ_2M, 	0, 0, true, true, true}, NULL ,NULL, 0},
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP
	{"log_sfr",	{SZ_4M,		0, 0, false, true, true}, NULL ,NULL, 0},
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
	{"log_etm",	{SZ_8M,		0, 0, true, true, true}, NULL ,NULL, 0},
#endif
#else /* MINIMIZED MODE */
	{"log_kevents",	{SZ_2M,		0, 0, false, true, true}, NULL ,NULL, 0},
	{"log_kernel",	{SZ_2M,		0, 0, false, true, true}, NULL ,NULL, 0},
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	{"log_main",{SZ_2M,		0, 0, false, true, true}, NULL ,NULL, 0},
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
	{"log_pstore",	{SZ_2M,		0, 0, true, true, true}, NULL ,NULL, 0},
#endif
#endif
};

/*
 *  including or excluding options
 *  if you want to except some interrupt, it should be written in this array
 */
static int ess_irqlog_exlist[] = {
/*  interrupt number ex) 152, 153, 154, */
	-1,
};

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static int ess_irqexit_exlist[] = {
/*  interrupt number ex) 152, 153, 154, */
	-1,
};

static unsigned ess_irqexit_threshold =
		CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT_THRESHOLD;
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
struct ess_reg_list {
	size_t addr;
	size_t size;
};

static struct ess_reg_list ess_reg_exlist[] = {
/*
 *  if it wants to reduce effect enabled reg feautre to system,
 *  you must add these registers - mct, serial
 *  because they are called very often.
 *  physical address, size ex) {0x10C00000, 0x1000},
 */
	{ESS_REG_MCT_ADDR, ESS_REG_MCT_SIZE},
	{ESS_REG_UART_ADDR, ESS_REG_UART_SIZE},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
};
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
static char *ess_freq_name[] = {
	"APL", "ATL", "INT", "MIF", "ISP", "DISP",
};
#endif
/*
 * ---------------------------------------------------------------------------
 *  User defined End
 * ---------------------------------------------------------------------------
 */

/*  External interface variable for trace debugging */
static struct exynos_ss_interface ess_info;

/*  Internal interface variable */
static struct exynos_ss_base ess_base;
static struct exynos_ss_log_idx ess_idx;
static struct exynos_ss_log *ess_log = NULL;
static struct exynos_ss_desc ess_desc;

/* Extern variable */
unsigned int *exynos_ss_base_enabled = &ess_base.enabled;

DEFINE_PER_CPU(struct pt_regs *, ess_core_reg);
DEFINE_PER_CPU(struct exynos_ss_mmu_reg *, ess_mmu_reg);

static void exynos_ss_save_mmu(struct exynos_ss_mmu_reg *mmu_reg)
{
	if (!exynos_ss_get_enable("log_kevents", true))
		return;

#ifdef CONFIG_ARM64
	asm("mrs x1, SCTLR_EL1\n\t"		/* SCTLR_EL1 */
	    "str x1, [%0]\n\t"
	    "mrs x1, TTBR0_EL1\n\t"		/* TTBR0_EL1 */
	    "str x1, [%0,#8]\n\t"
	    "mrs x1, TTBR1_EL1\n\t"		/* TTBR1_EL1 */
	    "str x1, [%0,#16]\n\t"
	    "mrs x1, TCR_EL1\n\t"		/* TCR_EL1 */
	    "str x1, [%0,#24]\n\t"
	    "mrs x1, ESR_EL1\n\t"		/* ESR_EL1 */
	    "str x1, [%0,#32]\n\t"
	    "mrs x1, FAR_EL1\n\t"		/* FAR_EL1 */
	    "str x1, [%0,#40]\n\t"
	    /* Don't populate AFSR0_EL1 and AFSR1_EL1 */
	    "mrs x1, CONTEXTIDR_EL1\n\t"	/* CONTEXTIDR_EL1 */
	    "str x1, [%0,#48]\n\t"
	    "mrs x1, TPIDR_EL0\n\t"		/* TPIDR_EL0 */
	    "str x1, [%0,#56]\n\t"
	    "mrs x1, TPIDRRO_EL0\n\t"		/* TPIDRRO_EL0 */
	    "str x1, [%0,#64]\n\t"
	    "mrs x1, TPIDR_EL1\n\t"		/* TPIDR_EL1 */
	    "str x1, [%0,#72]\n\t"
	    "mrs x1, MAIR_EL1\n\t"		/* MAIR_EL1 */
	    "str x1, [%0,#80]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%x1", "memory"			/* clobbered register */
	);

#else
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
	    "str r1, [%0]\n\t"
	    "mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
	    "str r1, [%0,#4]\n\t"
	    "mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
	    "str r1, [%0,#8]\n\t"
	    "mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
	    "str r1, [%0,#12]\n\t"
	    "mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
	    "str r1, [%0,#16]\n\t"
	    "mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
	    "str r1, [%0,#20]\n\t"
	    "mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
	    "str r1, [%0,#24]\n\t"
	    "mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
	    "str r1, [%0,#28]\n\t"
	    "mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
	    "str r1, [%0,#32]\n\t"
	    /* Don't populate DAFSR and RAFSR */
	    "mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
	    "str r1, [%0,#44]\n\t"
	    "mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
	    "str r1, [%0,#48]\n\t"
	    "mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
	    "str r1, [%0,#52]\n\t"
	    "mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
	    "str r1, [%0,#56]\n\t"
	    "mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
	    "str r1, [%0,#60]\n\t"
	    "mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
	    "str r1, [%0,#64]\n\t"
	    "mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
	    "str r1, [%0,#68]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%r1", "memory"			/* clobbered register */
	);
#endif
}

void __iomem *exynos_ss_get_base_vaddr(void)
{
	return (void __iomem *)(ess_base.vaddr);
}

void __iomem *exynos_ss_get_base_paddr(void)
{
	return (void __iomem *)(ess_base.paddr);
}

static void exynos_ss_core_power_stat(unsigned int val, unsigned cpu)
{
	if (exynos_ss_get_enable("log_kevents", true))
		__raw_writel(val, (exynos_ss_get_base_vaddr() +
					ESS_OFFSET_CORE_POWER_STAT + cpu * 4));
}

static unsigned int exynos_ss_get_core_panic_stat(unsigned cpu)
{
	if (exynos_ss_get_enable("log_kevents", true))
		return __raw_readl(exynos_ss_get_base_vaddr() +
					ESS_OFFSET_PANIC_STAT + cpu * 4);
	else
		return 0;
}

static void exynos_ss_set_core_panic_stat(unsigned int val, unsigned cpu)
{
	if (exynos_ss_get_enable("log_kevents", true))
		__raw_writel(val, (exynos_ss_get_base_vaddr() +
					ESS_OFFSET_PANIC_STAT + cpu * 4));
}

static void exynos_ss_scratch_reg(unsigned int val)
{
	if (exynos_ss_get_enable("log_kevents", true) || ess_desc.need_header)
		__raw_writel(val, exynos_ss_get_base_vaddr() + ESS_OFFSET_SCRATCH);
}

static void exynos_ss_report_reason(unsigned int val)
{
	if (exynos_ss_get_enable("log_kevents", true))
		__raw_writel(val, exynos_ss_get_base_vaddr() + ESS_OFFSET_EMERGENCY_REASON);
}

unsigned long exynos_ss_get_last_pc_paddr(void)
{
	/*
	 * Basically we want to save the pc value to non-cacheable region
	 * if ESS is enabled. But we should also consider cases that are not so.
	 */

	if (exynos_ss_get_enable("log_kevents", true))
		return (exynos_ss_get_item_paddr("log_kevents") + ESS_CORE_PC_OFFSET);
	else
		return virt_to_phys((void *)ess_desc.hardlockup_core_pc);
}

unsigned long exynos_ss_get_last_pc(unsigned int cpu)
{
	if (exynos_ss_get_enable("log_kevents", true))
		return __raw_readq(exynos_ss_get_base_vaddr()
				+ ESS_OFFSET_LAST_PC + cpu * 8);
	else
		return ess_desc.hardlockup_core_pc[cpu];
}

unsigned long exynos_ss_get_spare_vaddr(unsigned int offset)
{
	return (unsigned long)(exynos_ss_get_base_vaddr() +
				ESS_OFFSET_SPARE_BASE + offset);
}

unsigned long exynos_ss_get_spare_paddr(unsigned int offset)
{
	unsigned long kevent_vaddr = 0;
	unsigned int kevent_paddr = exynos_ss_get_item_paddr("log_kevents");

	if (kevent_paddr) {
		kevent_vaddr = (unsigned long)(kevent_paddr + ESS_HEADER_SZ +
				ESS_MMU_REG_SZ + ESS_CORE_REG_SZ + offset);
	}
	return kevent_vaddr;
}

static u32 exynos_ss_get_reason(void)
{
	if (exynos_ss_get_enable("log_kevents", true))
		return	__raw_readl(exynos_ss_get_base_vaddr() + ESS_OFFSET_EMERGENCY_REASON);
	return -1;
}

unsigned int exynos_ss_get_item_size(char* name)
{
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name)))
			return ess_items[i].entry.size;
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_get_item_size);

unsigned int exynos_ss_get_item_paddr(char* name)
{
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name)))
			return ess_items[i].entry.paddr;
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_get_item_paddr);

int exynos_ss_get_hardlockup(void)
{
	return ess_desc.hardlockup;
}
EXPORT_SYMBOL(exynos_ss_get_hardlockup);

int exynos_ss_set_hardlockup(int val)
{
	unsigned long flags;

	if (unlikely(!ess_base.enabled))
		return 0;

	raw_spin_lock_irqsave(&ess_desc.lock, flags);
	ess_desc.hardlockup = val;
	raw_spin_unlock_irqrestore(&ess_desc.lock, flags);
	return 0;
}
EXPORT_SYMBOL(exynos_ss_set_hardlockup);

int exynos_ss_prepare_panic(void)
{
	unsigned cpu;

	if (unlikely(!ess_base.enabled))
		return 0;

	exynos_ss_report_reason(ESS_SIGN_PANIC);
	/*
	 * kick watchdog to prevent unexpected reset during panic sequence
	 * and it prevents the hang during panic sequence by watchedog
	 */
	s3c2410wdt_keepalive_emergency();

	for (cpu = 0; cpu < ESS_NR_CPUS; cpu++) {
		if (exynos_cpu.power_state(cpu))
			exynos_ss_core_power_stat(ESS_SIGN_ALIVE, cpu);
		else
			exynos_ss_core_power_stat(ESS_SIGN_DEAD, cpu);
	}

	if(exynos_check_hardlockup_reason()) {
		/* no core got stucks in EL3 monitor */
		pr_emerg("%s: no core got stucks in EL3" \
				" monitor.\n", __func__);
	}

        if (busmon_get_panic() == true) {
		pr_emerg("[%s]### before run : dhd_dongle_mem_dump()\n", __func__);
		dhd_dongle_mem_dump();
		pr_emerg("[%s]### after  run : dhd_dongle_mem_dump()\n", __func__);
        }

	return 0;
}
EXPORT_SYMBOL(exynos_ss_prepare_panic);

void exynos_ss_hook_hardlockup_entry(void *v_regs)
{
	int cpu = get_current_cpunum();

	if (!ess_base.enabled ||
		!ess_desc.hardlockup_core_mask) {
		return;
	}

	/* re-check the cpu number which is lockup */
	if (ess_desc.hardlockup_core_mask & BIT(cpu)) {
		int ret;
		unsigned long last_pc;
		struct pt_regs *regs;
		unsigned long timeout = USEC_PER_SEC;

		do {
			/*
			 * If one cpu is occurred to lockup,
			 * others are going to output its own information
			 * without side-effect.
			 */
			ret = do_raw_spin_trylock(&ess_desc.lock);
			if (!ret)
				udelay(1);
		} while (!ret && timeout--);

		last_pc = exynos_ss_get_last_pc(cpu);

		regs = (struct pt_regs *)v_regs;

		/* Replace real pc value even if it is invalid */
		regs->pc = last_pc;

		/* Then, we expect bug() function works well */
		pr_emerg("\n--------------------------------------------------------------------------\n"
			"      Debugging Information for Hardlockup core - CPU %d"
			"\n--------------------------------------------------------------------------\n\n", cpu);
	}
}

void exynos_ss_hook_hardlockup_exit(void)
{
	int cpu = get_current_cpunum();

	if (!ess_base.enabled ||
		!ess_desc.hardlockup_core_mask) {
		return;
	}

	/* re-check the cpu number which is lockup */
	if (ess_desc.hardlockup_core_mask & BIT(cpu)) {
		/* clear bit to complete replace */
		ess_desc.hardlockup_core_mask &= ~(BIT(cpu));
		/*
		 * If this unlock function does not make a side-effect
		 * even it's not lock
		 */
		do_raw_spin_unlock(&ess_desc.lock);
	}
}

static void exynos_ss_recall_hardlockup_core(void)
{
	int i, ret;
	unsigned long cpu_mask = 0, tmp_bit = 0;
	unsigned long last_pc_addr = 0, timeout;

	for (i = 0; i < ESS_NR_CPUS; i++) {
		if (i == get_current_cpunum())
			continue;
		tmp_bit = cpu_online_mask->bits[ESS_NR_CPUS/SZ_64] & (1 << i);
		if (tmp_bit)
			cpu_mask |= tmp_bit;
	}

	if (!cpu_mask)
		goto out;

	last_pc_addr = exynos_ss_get_last_pc_paddr();

	pr_emerg("exynos-snapshot: core hardlockup mask information: 0x%lx\n", cpu_mask);
	ess_desc.hardlockup_core_mask = cpu_mask;

	/* Setup for generating NMI interrupt to unstopped CPUs */
	ret = exynos_smc(SMC_CMD_KERNEL_PANIC_NOTICE,
			 cpu_mask,
			 (unsigned long)exynos_ss_bug,
			 last_pc_addr);
	if (ret) {
		pr_emerg("exynos-snapshot: failed to generate NMI, "
			 "not support to dump information of core\n");
		ess_desc.hardlockup_core_mask = 0;
		goto out;
	}

	/* Wait up to 3 seconds for NMI interrupt */
	timeout = USEC_PER_SEC * 3;
	while (ess_desc.hardlockup_core_mask != 0 && timeout--)
		udelay(1);
out:
	return;
}

int exynos_ss_post_panic(void)
{
	if (ess_base.enabled) {
		exynos_ss_recall_hardlockup_core();
		exynos_ss_dump_sfr();

		exynos_ss_save_context(NULL);
		flush_cache_all();
#ifdef CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT
		if (!ess_desc.no_wdt_dev) {
#ifdef CONFIG_EXYNOS_SNAPSHOT_WATCHDOG_RESET
			if (ess_desc.hardlockup || num_online_cpus() > 1) {
				/* for stall cpu */
				while(1)
					wfi();
			}
#endif
		}
#endif
	}

#ifdef CONFIG_SEC_DEBUG
	hard_reset_delay();
#endif
	
#ifdef CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT
	arm_pm_restart(0, "panic");
#endif
	/* for stall cpu when not enabling panic reboot */
	while(1)
		wfi();

	/* Never run this function */
	pr_emerg("exynos-snapshot: %s DO NOT RUN this function (CPU:%d)\n",
					__func__, raw_smp_processor_id());
	return 0;
}
EXPORT_SYMBOL(exynos_ss_post_panic);

int exynos_ss_dump_panic(char *str, size_t len)
{
	if (unlikely(!ess_base.enabled) ||
		!exynos_ss_get_enable("log_kevents", true))
		return 0;

	/*  This function is only one which runs in panic funcion */
	if (str && len && len < 1024)
		memcpy(exynos_ss_get_base_vaddr() + ESS_OFFSET_PANIC_STRING, str, len);

	return 0;
}
EXPORT_SYMBOL(exynos_ss_dump_panic);

int exynos_ss_post_reboot(void)
{
	int cpu;

	if (unlikely(!ess_base.enabled))
		return 0;

	/* clear ESS_SIGN_PANIC when normal reboot */
	for (cpu = 0; cpu < ESS_NR_CPUS; cpu++)
		exynos_ss_set_core_panic_stat(ESS_SIGN_RESET, cpu);

	return 0;
}
EXPORT_SYMBOL(exynos_ss_post_reboot);

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO

unsigned long merr_symptom;

#define L2MERR0SR	0
#define TBWMERR0SR	1
#define LSMERR0SR	2
#define FEMERR0SR	3
#define L2MERRSR	4
#define CPUMERRSR	5

#define FATAL_MASK_M	(BIT(1) | BIT(0))
#define FATAL_MASK_A	(BIT(63) | BIT(31))

#define hook_merr(merr, reg, mask)		\
({						\
	if ((reg & mask) == mask)		\
		merr_symptom |= (1 << merr);	\
})

#else

#define hook_merr(merr, reg, mask)	({})

#endif

int exynos_ss_dump(void)
{
	unsigned long val;
	/*
	 *  Output CPU Memory Error syndrome Register
	 *  CPUMERRSR, L2MERRSR
	 */
#ifdef CONFIG_ARM64
	unsigned long reg1, reg2;

	if ((read_cpuid_implementor() == ARM_CPU_IMP_SEC)
			&& (read_cpuid_part_number() == ARM_CPU_PART_MONGOOSE)){
		/* for mngs */
		asm ("mrs %0, S3_1_c15_c2_0\n\t"
			"mrs %1, S3_1_c15_c2_4\n"
			: "=r" (reg1), "=r" (reg2));
		pr_emerg("FEMERR0SR: %016lx, FEMERR1SR: %016lx\n", reg1, reg2);
		hook_merr(FEMERR0SR, reg1, FATAL_MASK_M);

		asm ("mrs %0, S3_1_c15_c2_1\n\t"
			"mrs %1, S3_1_c15_c2_5\n"
			: "=r" (reg1), "=r" (reg2));
		pr_emerg("LSMERR0SR: %016lx, LSMERR1SR: %016lx\n", reg1, reg2);
		hook_merr(LSMERR0SR, reg1, FATAL_MASK_M);

		asm ("mrs %0, S3_1_c15_c2_2\n\t"
			"mrs %1, S3_1_c15_c2_6\n"
			: "=r" (reg1), "=r" (reg2));
		pr_emerg("TBWMERR0SR: %016lx, TBWMERR1SR: %016lx\n", reg1, reg2);
		hook_merr(TBWMERR0SR, reg1, FATAL_MASK_M);

		asm ("mrs %0, S3_1_c15_c2_3\n\t"
			"mrs %1, S3_1_c15_c2_7\n"
			: "=r" (reg1), "=r" (reg2));
		pr_emerg("L2MERR0SR: %016lx, L2MERR1SR: %016lx\n", reg1, reg2);
		hook_merr(L2MERR0SR, reg1, FATAL_MASK_M);

	} else {
		/* for apollo */
		asm ("mrs %0, S3_1_c15_c2_2\n\t"
			"mrs %1, S3_1_c15_c2_3\n"
			: "=r" (reg1), "=r" (reg2));
		pr_emerg("CPUMERRSR: %016lx, L2MERRSR: %016lx\n", reg1, reg2);
		hook_merr(CPUMERRSR, reg1, FATAL_MASK_A);
		hook_merr(L2MERRSR, reg2, FATAL_MASK_A);
	}
#else
	unsigned long reg0;
	asm ("mrc p15, 0, %0, c0, c0, 0\n": "=r" (reg0));
	if (((reg0 >> 4) & 0xFFF) == 0xC0F) {
		/*  Only Cortex-A15 */
		unsigned long reg1, reg2, reg3;
		asm ("mrrc p15, 0, %0, %1, c15\n\t"
			"mrrc p15, 1, %2, %3, c15\n"
			: "=r" (reg0), "=r" (reg1),
			"=r" (reg2), "=r" (reg3));
		pr_emerg("CPUMERRSR: %08lx_%08lx, L2MERRSR: %08lx_%08lx\n",
				reg1, reg0, reg3, reg2);
	}
#endif
	asm volatile("mrs %0, sctlr_el1" : "=r" (val));
	pr_emerg("SCTLR_EL1: %08lX,\n", val);

	return 0;
}
EXPORT_SYMBOL(exynos_ss_dump);

int exynos_ss_save_reg(void *v_regs)
{
	register unsigned long sp asm ("sp");
	struct pt_regs *regs = (struct pt_regs *)v_regs;
	struct pt_regs *core_reg =
			per_cpu(ess_core_reg, smp_processor_id());

	if(!exynos_ss_get_enable("log_kevents", true))
		return 0;

	if (!regs) {
		asm("str x0, [%0, #0]\n\t"
		    "mov x0, %0\n\t"
		    "str x1, [x0, #8]\n\t"
		    "str x2, [x0, #16]\n\t"
		    "str x3, [x0, #24]\n\t"
		    "str x4, [x0, #32]\n\t"
		    "str x5, [x0, #40]\n\t"
		    "str x6, [x0, #48]\n\t"
		    "str x7, [x0, #56]\n\t"
		    "str x8, [x0, #64]\n\t"
		    "str x9, [x0, #72]\n\t"
		    "str x10, [x0, #80]\n\t"
		    "str x11, [x0, #88]\n\t"
		    "str x12, [x0, #96]\n\t"
		    "str x13, [x0, #104]\n\t"
		    "str x14, [x0, #112]\n\t"
		    "str x15, [x0, #120]\n\t"
		    "str x16, [x0, #128]\n\t"
		    "str x17, [x0, #136]\n\t"
		    "str x18, [x0, #144]\n\t"
		    "str x19, [x0, #152]\n\t"
		    "str x20, [x0, #160]\n\t"
		    "str x21, [x0, #168]\n\t"
		    "str x22, [x0, #176]\n\t"
		    "str x23, [x0, #184]\n\t"
		    "str x24, [x0, #192]\n\t"
		    "str x25, [x0, #200]\n\t"
		    "str x26, [x0, #208]\n\t"
		    "str x27, [x0, #216]\n\t"
		    "str x28, [x0, #224]\n\t"
		    "str x29, [x0, #232]\n\t"
		    "str x30, [x0, #240]\n\t" :
		    : "r"(core_reg));
		core_reg->sp = (unsigned long)(sp);
		core_reg->pc =
			(unsigned long)(core_reg->regs[30] - sizeof(unsigned int));
	} else {
		memcpy(core_reg, regs, sizeof(struct pt_regs));
	}

	pr_emerg("exynos-snapshot: core register saved(CPU:%d)\n",
						smp_processor_id());
	return 0;
}
EXPORT_SYMBOL(exynos_ss_save_reg);

int __exynos_ss_save_context(void *v_regs, unsigned int val)
{
	unsigned long flags;
	struct pt_regs *regs = (struct pt_regs *)v_regs;

	if (unlikely(!ess_base.enabled))
		return 0;

	//exynos_trace_stop();

	local_irq_save(flags);

	/* If it was already saved the context information, it should be skipped */
	if (exynos_ss_get_core_panic_stat(smp_processor_id()) !=  ESS_SIGN_PANIC) {
		exynos_ss_save_mmu(per_cpu(ess_mmu_reg, smp_processor_id()));
		exynos_ss_save_reg(regs);
		exynos_ss_dump();
		exynos_ss_set_core_panic_stat(val, smp_processor_id());
		pr_emerg("exynos-snapshot: context saved(CPU:%d)\n",
							smp_processor_id());
	} else
		pr_emerg("exynos-snapshot: skip context saved(CPU:%d)\n",
							smp_processor_id());

	flush_cache_all();
	local_irq_restore(flags);
	return 0;
}

int exynos_ss_save_context(void *v_regs)
{
	u32 val = 0;

	val = exynos_ss_get_reason();
	pr_emerg("exynos-snapshot: exynos_ss_get_reason 0x%X (CPU:%d) \n", val, smp_processor_id());
	__exynos_ss_save_context(v_regs, val);
	return 0;
}
EXPORT_SYMBOL(exynos_ss_save_context);

int exynos_ss_set_enable(const char *name, int en)
{
	struct exynos_ss_item *item = NULL;
	unsigned long i;

	if (!strncmp(name, "base", strlen(name))) {
		ess_base.enabled = en;
		pr_info("exynos-snapshot: %sabled\n", en ? "en" : "dis");
	} else {
		for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
			if (!strncmp(ess_items[i].name, name, strlen(name))) {
				item = &ess_items[i];
				item->entry.enabled = en;
				item->time = local_clock();
				pr_info("exynos-snapshot: item - %s is %sabled\n",
						name, en ? "en" : "dis");
				break;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_set_enable);

int exynos_ss_try_enable(const char *name, unsigned long long duration)
{
	struct exynos_ss_item *item = NULL;
	unsigned long long time;
	unsigned long i;
	int ret = -1;

	/* If ESS was disabled, just return */
	if (unlikely(!ess_base.enabled) || !exynos_ss_get_enable("log_kevents", true))
		return ret;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name))) {
			item = &ess_items[i];

			/* We only interest in disabled */
			if (item->entry.enabled == false) {
				time = local_clock() - item->time;
				if (time > duration) {
					item->entry.enabled = true;
					ret = 1;
				} else
					ret = 0;
			}
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(exynos_ss_try_enable);

int exynos_ss_get_enable(const char *name, bool init)
{
	struct exynos_ss_item *item = NULL;
	unsigned long i;
	int ret = -1;

	if (!strncmp(name, "base", strlen(name))) {
		ret = ess_base.enabled;
	} else {
		for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
			if (!strncmp(ess_items[i].name, name, strlen(name))) {
				item = &ess_items[i];
				if (init)
					ret = item->entry.enabled_init;
				else
					ret = item->entry.enabled;
				break;
			}
		}
	}
	return ret;
}
EXPORT_SYMBOL(exynos_ss_get_enable);

static inline int exynos_ss_check_eob(struct exynos_ss_item *item,
						size_t size)
{
	size_t max, cur;

	max = (size_t)(item->head_ptr + item->entry.size);
	cur = (size_t)(item->curr_ptr + size);

	if (unlikely(cur > max))
		return -1;
	else
		return 0;
}
#ifdef CONFIG_SEC_DEBUG_AUTO_SUMMARY
void register_set_auto_comm_lastfreq(void (*func)(int type, int old_freq, int new_freq, u64 time))
{
	func_hook_auto_comm_lastfreq = func;
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
static inline void exynos_ss_hook_logger(const char *name,
					 const char *buf, size_t size)
{
	struct exynos_ss_item *item = NULL;
	unsigned long i;

	for (i = ess_desc.log_platform_num; i < ARRAY_SIZE(ess_items); i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name))) {
			item = &ess_items[i];
			break;
		}
	}

	if (unlikely(!item))
		return;

	if (likely(ess_base.enabled == true && item->entry.enabled == true)) {
		if (unlikely((exynos_ss_check_eob(item, size))))
			item->curr_ptr = item->head_ptr;
		memcpy(item->curr_ptr, buf, size);
		item->curr_ptr += size;
	}
}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
static inline void exynos_ss_hook_logbuf(const char buf)
{
	unsigned int last_buf;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel];

	if (likely(ess_base.enabled == true && item->entry.enabled == true)) {
		if (exynos_ss_check_eob(item, 1)) {
			item->curr_ptr = item->head_ptr;
#ifdef CONFIG_SEC_PM_DEBUG
			sec_log_full = true;
#endif

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
			*((unsigned long long *)(item->head_ptr + item->entry.size - (size_t)0x08)) = SEC_LKMSG_MAGICKEY;
#endif
		}

		item->curr_ptr[0] = buf;
		item->curr_ptr++;

		/*  save the address of last_buf to physical address */
		last_buf = (unsigned int)item->curr_ptr;
		__raw_writel(item->entry.paddr + (last_buf - item->entry.vaddr),
				exynos_ss_get_base_vaddr() + ESS_OFFSET_LAST_LOGBUF);
	}
}
#else
static inline void exynos_ss_hook_logbuf(const char *buf, size_t size)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];

	if (likely(ess_base.enabled == true && item->entry.enabled == true)) {
		size_t last_buf;

		if (exynos_ss_check_eob(item, size)) {
			item->curr_ptr = item->head_ptr;
#ifdef CONFIG_SEC_PM_DEBUG
			sec_log_full = true;
#endif

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
			*((unsigned long long *)(item->head_ptr + item->entry.size - (size_t)0x08)) = SEC_LKMSG_MAGICKEY;
#endif
		}

		memcpy(item->curr_ptr, buf, size);
		item->curr_ptr += size;
		/*  save the address of last_buf to physical address */
		last_buf = (size_t)item->curr_ptr;

		__raw_writel(item->entry.paddr + (last_buf - item->entry.vaddr),
				exynos_ss_get_base_vaddr() + ESS_OFFSET_LAST_LOGBUF);
	}
}
#endif

static void exynos_ss_dump_one_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'Z', 'X', 'x', 'K', 'W'};
	unsigned char idx = 0;
	unsigned int state = (tsk->state & TASK_REPORT) | tsk->exit_state;
	unsigned long wchan;
	unsigned long pc = 0;
	char symname[KSYM_NAME_LEN];
	int permitted;
	struct mm_struct *mm;

	permitted = ptrace_may_access(tsk, PTRACE_MODE_READ);
	mm = get_task_mm(tsk);
	if (mm) {
		if (permitted)
			pc = KSTK_EIP(tsk);
	}

	wchan = get_wchan(tsk);
	if (lookup_symbol_name(wchan, symname) < 0) {
		if (!ptrace_may_access(tsk, PTRACE_MODE_READ))
			snprintf(symname, KSYM_NAME_LEN,  "_____");
		else
			snprintf(symname, KSYM_NAME_LEN, "%lu", wchan);
	}

	while (state) {
		idx++;
		state >>= 1;
	}

	/*
	 * kick watchdog to prevent unexpected reset during panic sequence
	 * and it prevents the hang during panic sequence by watchedog
	 */
	touch_softlockup_watchdog();
	s3c2410wdt_keepalive_emergency();

	pr_info("%8d %8d %8d %16lld %c(%d) %3d  %16zx %16zx  %16zx %c %16s [%s]\n",
			tsk->pid, (int)(tsk->utime), (int)(tsk->stime),
			tsk->se.exec_start, state_array[idx], (int)(tsk->state),
			task_cpu(tsk), wchan, pc, (unsigned long)tsk,
			is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->state == TASK_RUNNING
			|| tsk->state == TASK_UNINTERRUPTIBLE
			|| tsk->mm == NULL) {
		print_worker_info(KERN_INFO, tsk);
		show_stack(tsk, NULL);
		pr_info("\n");
	}
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next,
				struct task_struct,
				thread_group);
}

static void exynos_ss_dump_task_info(void)
{
	struct task_struct *frst_tsk;
	struct task_struct *curr_tsk;
	struct task_struct *frst_thr;
	struct task_struct *curr_thr;

	pr_info("\n");
	pr_info(" current proc : %d %s\n", current->pid, current->comm);
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");
	pr_info("     pid      uTime    sTime      exec(ns)  stat  cpu       wchan           user_pc        task_struct       comm   sym_wchan\n");
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk != NULL) {
		exynos_ss_dump_one_task_info(curr_tsk,  true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = get_next_thread(curr_tsk);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					exynos_ss_dump_one_task_info(curr_thr, false);
					curr_thr = get_next_thread(curr_thr);
					if (curr_thr == curr_tsk)
						break;
				}
			}
		}
		curr_tsk = container_of(curr_tsk->tasks.next,
					struct task_struct, tasks);
		if (curr_tsk == frst_tsk)
			break;
	}
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP_ADV
static bool exynos_ss_check_pmu_regular(struct exynos_ss_sfrdump *sfrdump,
						const struct device_node *np,
						void *io_base)
{
	u32 pmu_addr;
	unsigned int offset, val;
	int ret;

	/* get address */
	ret = of_property_read_u32_index(np, "pmu-addr", 0, &pmu_addr);
	if (ret < 0) {
		pr_err("failed to get pmu-addr - %s\n", sfrdump->name);
		return false;
	}

	/* check boundary */
	if (pmu_addr < ess_desc.sfrdump_phy_pmu || pmu_addr >= ess_desc.sfrdump_phy_pmu_end) {
		pr_err("pmu-addr not in pmu boundery - %s\n", sfrdump->name);
		return false;
	}

	offset = pmu_addr - ess_desc.sfrdump_phy_pmu;
	val = __raw_readl(io_base + offset);

	if ((val & 0xF) != 0xF) {
		pr_info("not powered - %s: (%08x=%x)\n", sfrdump->name, pmu_addr, val);
		return false;
	}

	return true;
}

static bool exynos_ss_check_pmu_gpu(struct exynos_ss_sfrdump *sfrdump,
						const struct device_node *np,
						void *io_base)
{
	u32 pmu_addr[2];
	unsigned int offset, val;
	int ret;

	/* get address */
	ret = of_property_read_u32_array(np, "pmu-addr", &pmu_addr[0], 2);
	if (ret < 0) {
		pr_err("failed to get pmu-addr - %s\n", sfrdump->name);
		return false;
	}

	/* check boundary */
	if (pmu_addr[0] < ess_desc.sfrdump_phy_pmu	||
				pmu_addr[0] >= ess_desc.sfrdump_phy_pmu_end ||
				pmu_addr[1] < ess_desc.sfrdump_phy_pmu ||
				pmu_addr[1] >= ess_desc.sfrdump_phy_pmu_end) {
		pr_err("pmu-addr not in pmu boundery - %s\n", sfrdump->name);
		return false;
	}

	/* check power */
	offset = pmu_addr[0] - ess_desc.sfrdump_phy_pmu;
	val = __raw_readl(io_base + offset);
	if ((val & 0xF) != 0xF) {
		pr_info("not powered - %s: (%08x=%x)\n", sfrdump->name, pmu_addr[0], val);
		return false;
	}

	/* check DVS */
	offset = pmu_addr[1] - ess_desc.sfrdump_phy_pmu;
	val = __raw_readl(io_base + offset);
	if (!(val & 0x1)) {
		pr_info("gpu voltage is low - %s: (%08x=%x)\n", sfrdump->name, pmu_addr[1], val);
		return false;
	}

	return true;
}

static struct exynos_ss_sfrdump_func ess_dumpsfr_funcs[] = {
	[PWR_NONE] = {
		.name = "none",
		.fn = NULL,
	},
	[PWR_REGULAR] = {
		.name = "regular",
		.fn = exynos_ss_check_pmu_regular,
	},
	[PWR_GPU] = {
		.name = "gpu",
		.fn = exynos_ss_check_pmu_gpu,
	},
};

static int exynos_ss_check_pmu(struct exynos_ss_sfrdump *sfrdump,
						const struct device_node *np)
{
	int ret = 0;
	int mode;

	/* check power mode valid */
	if (sfrdump->pwr_mode == PWR_NONE)
		return 1;

	if (sfrdump->pwr_mode >= PWR_TYPES) {
		pr_err("invalid pwr_mode:%d\n", sfrdump->pwr_mode);
		return -EINVAL;
	}

	if (!ess_desc.sfrdump_pmu) {
		pr_err("invalid ess_info.sfrdump_pmu\n");
		return -EINVAL;
	}

	mode = sfrdump->pwr_mode;

	/* check function valid */
	if (ess_dumpsfr_funcs[mode].fn)
		ret = ess_dumpsfr_funcs[mode].fn(sfrdump, np, ess_desc.sfrdump_pmu);
	else
		ret = -EINVAL;

	return ret;
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP
void exynos_ss_dump_sfr(void)
{
	struct exynos_ss_sfrdump *sfrdump;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_sfr_num];
	struct list_head *entry;
	struct device_node *np;
	unsigned int reg, offset, val, size;
	int i, ret;
	static char buf[SZ_64];

	if (unlikely(!ess_base.enabled))
		return;

	if (list_empty(&ess_desc.sfrdump_list) || unlikely(!item) ||
		unlikely(item->entry.enabled == false)) {
		pr_emerg("exynos-snapshot: %s: No information\n", __func__);
		return;
	}

	list_for_each(entry, &ess_desc.sfrdump_list) {
		sfrdump = list_entry(entry, struct exynos_ss_sfrdump, list);
		np = of_node_get(sfrdump->node);

#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP_ADV
		if (sfrdump->pwr_mode != PWR_NONE) {
			ret = exynos_ss_check_pmu(sfrdump, np);
			if (!ret)
				/* may off */
				continue;
			if (ret < 0) {
				pr_info("exynos-snapshot: fail to check power of %s (%d)\n", sfrdump->name, ret);
				continue;
			}
		}
#endif
		for (i = 0; i < SZ_2K; i++) {
			ret = of_property_read_u32_index(np, "addr", i, &reg);
			if (ret < 0) {
				pr_err("exynos-snapshot: failed to get address information - %s\n",
					sfrdump->name);
				break;
			}
			if (reg == 0xFFFFFFFF || reg == 0)
				break;
			offset = reg - sfrdump->phy_reg;
			if (reg < offset) {
				pr_err("exynos-snapshot: invalid address information - %s: 0x%08x\n",
				sfrdump->name, reg);
				break;
			}
			val = __raw_readl(sfrdump->reg + offset);
			snprintf(buf, SZ_64, "0x%X = 0x%0X\n",reg, val);
			size = strlen(buf);
			if (unlikely((exynos_ss_check_eob(item, size))))
				item->curr_ptr = item->head_ptr;
			memcpy(item->curr_ptr, buf, strlen(buf));
			item->curr_ptr += strlen(buf);
		}
		of_node_put(np);
		pr_info("exynos-snapshot: complete to dump %s\n", sfrdump->name);
	}

}

static int exynos_ss_sfr_dump_init(struct device_node *np)
{
	struct device_node *dump_np;
	struct exynos_ss_sfrdump *sfrdump;
	char *dump_str;
#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP_ADV
	u32 pwr_mode;
	u32 pmu_regs[2];
	void *pmu_reg;
#endif

	unsigned int phy_regs[2];
	int count, ret, i;

	ret = of_property_count_strings(np, "sfr-dump-list");
	if (ret < 0) {
		pr_err("failed to get sfr-dump-list\n");
		return ret;
	}
	count = ret;

#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP_ADV
	ret = of_property_read_u32_array(np, "pmu-regs", pmu_regs, 2);
	if (ret >= 0) {
		pmu_reg = ioremap(pmu_regs[0], pmu_regs[1]);
		if (!pmu_reg) {
			pr_err("failed to get i/o address pmu-regs\n");
			return -ENXIO;
		}
		ess_desc.sfrdump_pmu = pmu_reg;
		ess_desc.sfrdump_phy_pmu = pmu_regs[0];
		ess_desc.sfrdump_phy_pmu_end = pmu_regs[0] + pmu_regs[1];
	} else {
		pr_warn("failed to get pmu-regs\n");
		ess_desc.sfrdump_pmu = 0;
	}
#endif

	INIT_LIST_HEAD(&ess_desc.sfrdump_list);
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "sfr-dump-list", i,
						(const char **)&dump_str);
		if (ret < 0) {
			pr_err("failed to get sfr-dump-list\n");
			continue;
		}

		dump_np = of_get_child_by_name(np, dump_str);
		if (!dump_np) {
			pr_err("failed to get %s node, count:%d\n", dump_str, count);
			continue;
		}

		sfrdump = kzalloc(sizeof(struct exynos_ss_sfrdump), GFP_KERNEL);
		if (!sfrdump) {
			pr_err("failed to get memory region of exynos_ss_sfrdump\n");
			of_node_put(dump_np);
			continue;
		}

		ret = of_property_read_u32_array(dump_np, "reg", phy_regs, 2);
		if (ret < 0) {
			pr_err("failed to get register information\n");
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}

		sfrdump->reg = ioremap(phy_regs[0], phy_regs[1]);
		if (!sfrdump->reg) {
			pr_err("failed to get i/o address %s node\n", dump_str);
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}
		sfrdump->name = dump_str;

		ret = of_property_count_u32_elems(dump_np, "addr");
		if (ret < 0) {
			pr_err("failed to get addr count\n");
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}
		sfrdump->phy_reg = phy_regs[0];
		sfrdump->num = ret;

#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP_ADV
		ret = of_property_read_u32(dump_np, "pwr-mode", &pwr_mode);
		if (ret == -EINVAL) {
			/* no property */
			pwr_mode = PWR_NONE;
		} else if (ret < 0) {
			pr_err("failed to get pwr-mode\n");
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}
		sfrdump->pwr_mode = pwr_mode;
#endif

		sfrdump->node = dump_np;
		list_add(&sfrdump->list, &ess_desc.sfrdump_list);

		pr_info("success to regsiter %s\n", sfrdump->name);
		of_node_put(dump_np);
	}
	return ret;
}

#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_CRASH_KEY
void exynos_ss_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	static const unsigned int VOLUME_UP = KEY_VOLUMEUP;
	static const unsigned int VOLUME_DOWN = KEY_VOLUMEDOWN;

	if (code == KEY_POWER)
		pr_info("exynos-snapshot: POWER-KEY %s\n", value ? "pressed" : "released");

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
				pr_info
				    ("exynos-snapshot: count for entering forced upload [%d]\n",
				     ++loopcount);
				if (loopcount == 2) {
					panic("Crash Key");
				}
			}
		}
	} else {
		if (code == VOLUME_UP)
			volup_p = false;
		if (code == VOLUME_DOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}
#endif

static int exynos_ss_reboot_handler(struct notifier_block *nb,
				    unsigned long l, void *p)
{
	if (unlikely(!ess_base.enabled))
		return 0;

	pr_emerg("exynos-snapshot: normal reboot [%s]\n", __func__);
	exynos_ss_report_reason(ESS_SIGN_NORMAL_REBOOT);
	exynos_ss_scratch_reg(ESS_SIGN_RESET);
	exynos_ss_save_context(NULL);

#ifdef CONFIG_SEC_DEBUG
	sec_debug_reboot_handler();
#endif
	flush_cache_all();
	return 0;
}

static int exynos_ss_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	if (unlikely(!ess_base.enabled))
		return 0;

#ifdef CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT
	local_irq_disable();
	exynos_ss_report_reason(ESS_SIGN_PANIC);
	pr_emerg("exynos-snapshot: panic - reboot[%s]\n", __func__);
	exynos_ss_dump_task_info();
#ifdef CONFIG_EXYNOS_CORESIGHT_PC_INFO
	if (exynos_ss_get_enable("log_kevents", true))
		memcpy(ess_log->core, exynos_cs_pc, sizeof(ess_log->core));
#endif
	flush_cache_all();
#ifdef CONFIG_SEC_DEBUG
	sec_debug_panic_handler(buf, true);
#endif
#else
	exynos_ss_report_reason(ESS_SIGN_PANIC);
	pr_emerg("exynos-snapshot: panic - normal[%s]\n", __func__);
	exynos_ss_dump_task_info();
	flush_cache_all();
#endif
	return 0;
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = exynos_ss_reboot_handler
};

static struct notifier_block nb_panic_block = {
	.notifier_call = exynos_ss_panic_handler,
};

void exynos_ss_panic_handler_safe(struct pt_regs *regs)
{
	char text[1024];
	size_t len;

	if (unlikely(!ess_base.enabled))
		return;

	snprintf(text, 1024, "safe panic handler at cpu %d", (int)raw_smp_processor_id());
	len = (size_t)strnlen(text, 1024);

	exynos_ss_report_reason(ESS_SIGN_SAFE_FAULT);
	exynos_ss_dump_panic(text, len);
	s3c2410wdt_set_emergency_reset(100);

}

static size_t __init exynos_ss_remap(void)
{
	unsigned long i;
	unsigned int enabled_count = 0;
	size_t pre_paddr, pre_vaddr, item_size;
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	int page_size, ret;
	struct page *page;
	struct page **pages;

	page_size = ess_desc.vm.size / PAGE_SIZE;
	pages = kzalloc(sizeof(struct page*) * page_size, GFP_KERNEL);
	page = phys_to_page(ess_desc.vm.phys_addr);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	ret = map_vm_area(&ess_desc.vm, prot, pages);
	if (ret) {
		pr_err("exynos-snapshot: failed to mapping between virt and phys for firmware");
		return -ENOMEM;
	}
	kfree(pages);

	/* initializing value */
	pre_paddr = (size_t)ess_base.paddr;
	pre_vaddr = (size_t)ess_base.vaddr;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		/* fill rest value of ess_items arrary */
		if (i == ess_desc.kevents_num ||
				ess_items[i].entry.enabled_init) {

			if (i == ess_desc.kevents_num && ess_desc.need_header)
				item_size = ESS_HEADER_ALLOC_SZ;
			else
				item_size = ess_items[i].entry.size;

			ess_items[i].entry.vaddr = pre_vaddr;
			ess_items[i].entry.paddr = pre_paddr;

			ess_items[i].head_ptr = (unsigned char *)ess_items[i].entry.vaddr;
			ess_items[i].curr_ptr = (unsigned char *)ess_items[i].entry.vaddr;

			/* For Next */
			pre_vaddr = ess_items[i].entry.vaddr + item_size;
			pre_paddr = ess_items[i].entry.paddr + item_size;

			enabled_count++;
		}
	}
	return (size_t)(enabled_count ? exynos_ss_get_base_vaddr() : 0);
}

static int __init exynos_ss_init_desc(void)
{
	unsigned int i, len;

	/* initialize ess_desc */
	memset((struct exynos_ss_desc *)&ess_desc, 0, sizeof(struct exynos_ss_desc));
	ess_desc.callstack = CONFIG_EXYNOS_SNAPSHOT_CALLSTACK;
	raw_spin_lock_init(&ess_desc.lock);

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		len = strlen(ess_items[i].name);
		if (!strncmp(ess_items[i].name, "log_kevents", len))
			ess_desc.kevents_num = i;
		else if (!strncmp(ess_items[i].name, "log_kernel", len))
			ess_desc.log_kernel_num = i;
		else if (!strncmp(ess_items[i].name, "log_platform", len))
			ess_desc.log_platform_num = i;
		else if (!strncmp(ess_items[i].name, "log_sfr", len))
			ess_desc.log_sfr_num = i;
		else if (!strncmp(ess_items[i].name, "log_pstore", len))
			ess_desc.log_pstore_num = i;
		else if (!strncmp(ess_items[i].name, "log_etm", len))
			ess_desc.log_etm_num = i;
	}

	if (!ess_items[ess_desc.kevents_num].entry.enabled_init)
		ess_desc.need_header = true;

	return 0;
}

static int __init exynos_ss_setup(char *str)
{
	unsigned long i;
	size_t size = 0;
	size_t base = 0;

#ifdef CONFIG_SEC_DEBUG
	if (sec_debug_init()) {
		pr_info("exynos-snapshot: disabled because sec_debug is not activated\n");
		return -1;
	}
#endif
	if (kstrtoul(str, 0, (unsigned long *)&base))
		goto out;

	exynos_ss_init_desc();

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (ess_items[i].entry.enabled_init)
			size += ess_items[i].entry.size;
	}

	/* More need the size for Header */
	if (ess_desc.need_header)
		size += ESS_HEADER_ALLOC_SZ;

	pr_info("exynos-snapshot: try to reserve dedicated memory : 0x%zx, 0x%zx\n",
			base, size);

#ifdef CONFIG_NO_BOOTMEM
	if (!memblock_is_region_reserved(base, size) &&
		!memblock_reserve(base, size)) {
#else
	if (!reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE)) {
#endif
		ess_base.paddr = base;
		ess_base.vaddr = (size_t)(ESS_FIXED_VIRT_BASE);
		ess_base.size = size;
		ess_base.enabled = false;

		/* Reserved fixed virtual memory within VMALLOC region */
		ess_desc.vm.phys_addr = base;
		ess_desc.vm.addr = (void *)(ESS_FIXED_VIRT_BASE);
		ess_desc.vm.size = size + PAGE_SIZE;

		vm_area_add_early(&ess_desc.vm);

		pr_info("exynos-snapshot: memory reserved complete : 0x%zx, 0x%zx, 0x%zx\n",
			base, (size_t)(ESS_FIXED_VIRT_BASE), size);
		return 0;
	}
out:
	pr_err("exynos-snapshot: buffer reserved failed : 0x%zx, 0x%zx\n", base, size);
	return -1;
}
__setup("ess_setup=", exynos_ss_setup);

/*
 *  Normally, exynos-snapshot has 2-types debug buffer - log and hook.
 *  hooked buffer is for log_buf of kernel and loggers of platform.
 *  Each buffer has 2Mbyte memory except loggers. Loggers is consist of 4
 *  division. Each logger has 1Mbytes.
 *  ---------------------------------------------------------------------
 *  - dummy data:phy_addr, virtual_addr, buffer_size, magic_key(4K)	-
 *  ---------------------------------------------------------------------
 *  -		Cores MMU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		Cores CPU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		log buffer(3Mbyte - Headers(12K))			-
 *  ---------------------------------------------------------------------
 *  -		Hooked buffer of kernel's log_buf(2Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked main logger buffer of platform(3Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked system logger buffer of platform(1Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked radio logger buffer of platform(?Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked events logger buffer of platform(?Mbyte)		-
 *  ---------------------------------------------------------------------
 */
static int __init exynos_ss_output(void)
{
	unsigned long i;

	pr_info("exynos-snapshot physical / virtual memory layout:\n");
	for (i = 0; i < ARRAY_SIZE(ess_items); i++)
		if (ess_items[i].entry.enabled_init)
			pr_info("%-12s: phys:0x%zx / virt:0x%zx / size:0x%zx\n",
				ess_items[i].name,
				ess_items[i].entry.paddr,
				ess_items[i].entry.vaddr,
				ess_items[i].entry.size);

#ifdef CONFIG_S3C2410_WATCHDOG
	ess_desc.no_wdt_dev = false;
#else
	ess_desc.no_wdt_dev = true;
#endif

#ifdef CONFIG_SEC_DEBUG
	sec_getlog_supply_kernel((void*)phys_to_virt(ess_items[ess_desc.log_kernel_num].entry.paddr));
#endif
	return 0;
}

/*	Header dummy data(4K)
 *	-------------------------------------------------------------------------
 *		0		4		8		C
 *	-------------------------------------------------------------------------
 *	0	vaddr	phy_addr	size		magic_code
 *	4	Scratch_val	logbuf_addr	0		0
 *	-------------------------------------------------------------------------
*/
static void __init exynos_ss_fixmap_header(void)
{
	/*  fill 0 to next to header */
	size_t vaddr, paddr, size;
	size_t *addr;
	int i;

	vaddr = ess_items[ess_desc.kevents_num].entry.vaddr;
	paddr = ess_items[ess_desc.kevents_num].entry.paddr;
	size = ess_items[ess_desc.kevents_num].entry.size;

	/*  set to confirm exynos-snapshot */
	addr = (size_t *)vaddr;
	memcpy(addr, &ess_base, sizeof(struct exynos_ss_base));

	for (i = 0; i < ESS_NR_CPUS; i++) {
		per_cpu(ess_mmu_reg, i) = (struct exynos_ss_mmu_reg *)
					  (vaddr + ESS_HEADER_SZ +
					   i * ESS_MMU_REG_OFFSET);
		per_cpu(ess_core_reg, i) = (struct pt_regs *)
					   (vaddr + ESS_HEADER_SZ + ESS_MMU_REG_SZ +
					    i * ESS_CORE_REG_OFFSET);
	}

	if (!exynos_ss_get_enable("log_kevents", true))
		return;

	/*  kernel log buf */
	ess_log = (struct exynos_ss_log *)(vaddr + ESS_HEADER_TOTAL_SZ);

	/*  set fake translation to virtual address to debug trace */

    /* DO NOT integrate LSI addressing scheme for next line */
	ess_info.info_event = phys_to_virt(paddr + ESS_HEADER_TOTAL_SZ);
    /* DO NOT integrate LSI addressing scheme for upper line*/

#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	atomic_set(&(ess_idx.printk_log_idx), -1);
	atomic_set(&(ess_idx.printkl_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
	atomic_set(&(ess_idx.regulator_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	atomic_set(&(ess_idx.thermal_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_MBOX
	atomic_set(&(ess_idx.mailbox_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
	atomic_set(&(ess_idx.freq_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	atomic_set(&(ess_idx.clk_log_idx), -1);
#endif
	for (i = 0; i < ESS_NR_CPUS; i++) {
		atomic_set(&(ess_idx.task_log_idx[i]), -1);
		atomic_set(&(ess_idx.work_log_idx[i]), -1);
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
		atomic_set(&(ess_idx.clockevent_log_idx[i]), -1);
#endif
		atomic_set(&(ess_idx.cpuidle_log_idx[i]), -1);
		atomic_set(&(ess_idx.suspend_log_idx[i]), -1);
		atomic_set(&(ess_idx.irq_log_idx[i]), -1);
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
		atomic_set(&(ess_idx.spinlock_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
		atomic_set(&(ess_idx.irqs_disabled_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
		atomic_set(&(ess_idx.irq_exit_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
		atomic_set(&(ess_idx.reg_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
		atomic_set(&(ess_idx.hrtimer_log_idx[i]), -1);
#endif
	}
	/*  initialize kernel event to 0 except only header */
	memset((size_t *)(vaddr + ESS_KEEP_HEADER_SZ), 0, size - ESS_KEEP_HEADER_SZ);
}

static int __init exynos_ss_fixmap(void)
{
	size_t last_buf;
	size_t vaddr, paddr, size;
	unsigned long i;

	/*  fixmap to header first */
	exynos_ss_fixmap_header();

	for (i = 1; i < ARRAY_SIZE(ess_items); i++) {
		if (!ess_items[i].entry.enabled_init)
			continue;

		/*  assign kernel log information */
		paddr = ess_items[i].entry.paddr;
		vaddr = ess_items[i].entry.vaddr;
		size = ess_items[i].entry.size;

		if (!strncmp(ess_items[i].name, "log_kernel", strlen(ess_items[i].name))) {
			/*  load last_buf address value(phy) by virt address */
			last_buf = (size_t)__raw_readl(exynos_ss_get_base_vaddr() +
							ESS_OFFSET_LAST_LOGBUF);
			/*  check physical address offset of kernel logbuf */
			if (last_buf >= ess_items[i].entry.paddr &&
				(last_buf) <= (ess_items[i].entry.paddr + ess_items[i].entry.size)) {
				/*  assumed valid address, conversion to virt */
				ess_items[i].curr_ptr = (unsigned char *)(ess_items[i].entry.vaddr +
							(last_buf - ess_items[i].entry.paddr));
			} else {
				/*  invalid address, set to first line */
				ess_items[i].curr_ptr = (unsigned char *)vaddr;
				/*  initialize logbuf to 0 */
				memset((size_t *)vaddr, 0, size);
			}
		} else {
			/*  initialized log to 0 if persist == false */
			if (ess_items[i].entry.persist == false)
				memset((size_t *)vaddr, 0, size);
		}
		ess_info.info_log[i - 1].name = kstrdup(ess_items[i].name, GFP_KERNEL);
		ess_info.info_log[i - 1].head_ptr = (unsigned char *)ess_items[i].entry.vaddr;
		ess_info.info_log[i - 1].curr_ptr = NULL;
		ess_info.info_log[i - 1].entry.size = size;
	}

	/* output the information of exynos-snapshot */
	exynos_ss_output();

#ifdef CONFIG_SEC_DEBUG
	sec_debug_save_last_kmsg(ess_items[ess_desc.log_kernel_num].head_ptr,
			ess_items[ess_desc.log_kernel_num].curr_ptr,
			ess_items[ess_desc.log_kernel_num].entry.size);
#endif

	return 0;
}

static int exynos_ss_init_dt_parse(struct device_node *np)
{
	int ret = 0;
#ifdef CONFIG_EXYNOS_SNAPSHOT_SFRDUMP
	struct device_node *sfr_dump_np = of_get_child_by_name(np, "dump-info");
	if (!sfr_dump_np) {
		pr_err("failed to get dump-info node\n");
		ret = -ENODEV;
	} else {
		ret = exynos_ss_sfr_dump_init(sfr_dump_np);
		if (ret < 0) {
			pr_err("failed to register sfr dump node\n");
			ret = -ENODEV;
			of_node_put(sfr_dump_np);
		}
	}
	of_node_put(np);
#endif

	if (of_property_read_u32(np, "ess_boot_logging", &ess_boot_logging)) {
		pr_warn("ess_boot_logging property is omitted!\n");
	}
	pr_warn("ess_boot_logging is %d\n", ess_boot_logging);

	/* TODO: adding more dump information */
	return ret;
}

static const struct of_device_id ess_of_match[] __initconst = {
	{ .compatible = "samsung,exynos-snapshot",     .data = exynos_ss_init_dt_parse},
	{},
};

static int __init exynos_ss_init_dt(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	ess_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, ess_of_match, &matched_np);

	if (!np) {
		pr_info("%s: error\n", __func__);
		return -ENODEV;
	}

	init_fn = (ess_initcall_t)matched_np->data;
	return init_fn(np);
}

static int __init exynos_ss_init(void)
{
	if (ess_base.vaddr && ess_base.paddr && ess_base.size) {
	/*
	 *  for debugging when we don't know the virtual address of pointer,
	 *  In just privous the debug buffer, It is added 16byte dummy data.
	 *  start address(dummy 16bytes)
	 *  --> @virtual_addr | @phy_addr | @buffer_size | @magic_key(0xDBDBDBDB)
	 *  And then, the debug buffer is shown.
	 */
		exynos_ss_remap();
		exynos_ss_fixmap();
		exynos_ss_init_dt();
		exynos_ss_scratch_reg(ESS_SIGN_SCRATCH);
		exynos_ss_set_enable("base", true);

		register_hook_logbuf(exynos_ss_hook_logbuf);

#if defined(CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER) && defined(CONFIG_ANDROID_LOGGER)
		register_hook_logger(exynos_ss_hook_logger);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
		register_hook_pstore_log(exynos_ss_hook_logger);
#endif 
		register_reboot_notifier(&nb_reboot_block);
		atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	} else
		pr_err("exynos-snapshot: %s failed\n", __func__);

	return 0;
}
early_initcall(exynos_ss_init);

void __exynos_ss_task(int cpu, void *v_task)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	unsigned long val;

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		unsigned long idx = atomic_inc_return(&ess_idx.task_log_idx[cpu]);
		unsigned long i;
		unsigned long long time;
		time = cpu_clock(cpu);
		if (unlikely(ess_boot_logging)) {
			i = idx & (ARRAY_SIZE(ess_log->task_boot[0]) - 1);
			ess_log->task_boot[cpu][i].time = cpu_clock(cpu);
			asm volatile("mrs %0,   ttbr0_el1" : "=r" (val));
			ess_log->task_boot[cpu][i].ttbr0_el1 = (unsigned long) val;
			ess_log->task_boot[cpu][i].task = (struct task_struct *)v_task;
			ess_log->task_boot[cpu][i].task_comm = ess_log->task_boot[cpu][i].task->comm;
		} else {
			i = idx & (ARRAY_SIZE(ess_log->task[0]) - 1);
			ess_log->task[cpu][i].time = time;
			asm volatile("mrs %0,   ttbr0_el1" : "=r" (val));
			ess_log->task[cpu][i].ttbr0_el1 = (unsigned long) val;
			ess_log->task[cpu][i].task = (struct task_struct *)v_task;
			ess_log->task[cpu][i].task_comm = ess_log->task[cpu][i].task->comm;
		}
 
	}
}

void __exynos_ss_work(void *worker, void *work, void *fn, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;

	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.work_log_idx[cpu]) &
					(ARRAY_SIZE(ess_log->work[0]) - 1);

		ess_log->work[cpu][i].time = cpu_clock(cpu);
		ess_log->work[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->work[cpu][i].worker = (struct worker *)worker;
		ess_log->work[cpu][i].work = (struct work_struct *)work;
		ess_log->work[cpu][i].fn = (work_func_t)fn;
		ess_log->work[cpu][i].en = en;
	}
}

void __exynos_ss_cpuidle(char *modes, unsigned state, int diff, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.cpuidle_log_idx[cpu]) &
				(ARRAY_SIZE(ess_log->cpuidle[0]) - 1);

		ess_log->cpuidle[cpu][i].time = cpu_clock(cpu);
		ess_log->cpuidle[cpu][i].modes = modes;
		ess_log->cpuidle[cpu][i].state = state;
		ess_log->cpuidle[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->cpuidle[cpu][i].num_online_cpus = num_online_cpus();
		ess_log->cpuidle[cpu][i].delta = diff;
		ess_log->cpuidle[cpu][i].en = en;
	}
}

void __exynos_ss_suspend(void *fn, void *dev, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.suspend_log_idx[cpu]) &
				(ARRAY_SIZE(ess_log->suspend[0]) - 1);

		ess_log->suspend[cpu][i].time = cpu_clock(cpu);
		ess_log->suspend[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->suspend[cpu][i].fn = fn;
		ess_log->suspend[cpu][i].dev = (struct device *)dev;
		ess_log->suspend[cpu][i].en = en;
	}
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
void __exynos_ss_regulator(char *f_name, unsigned int addr, unsigned int volt, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.regulator_log_idx) &
				(ARRAY_SIZE(ess_log->regulator) - 1);
		int size = strlen(f_name);
		if (size >= SZ_16)
			size = SZ_16 - 1;
		ess_log->regulator[i].time = cpu_clock(cpu);
		ess_log->regulator[i].cpu = cpu;
		strncpy(ess_log->regulator[i].name, f_name, size);
		ess_log->regulator[i].reg = addr;
		ess_log->regulator[i].en = en;
		ess_log->regulator[i].voltage = volt;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
void __exynos_ss_thermal(void *data, unsigned int temp, char *name, unsigned int max_cooling)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.thermal_log_idx) &
				(ARRAY_SIZE(ess_log->thermal) - 1);

		ess_log->thermal[i].time = cpu_clock(cpu);
		ess_log->thermal[i].cpu = cpu;
		ess_log->thermal[i].data = (struct exynos_tmu_platform_data *)data;
		ess_log->thermal[i].temp = temp;
		ess_log->thermal[i].cooling_device = name;
		ess_log->thermal[i].cooling_state = max_cooling;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_MBOX
void __exynos_ss_mailbox(void *msg, int mode, char *f_name, void *volt)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	u32 *msg_data = (u32 *)msg;
	u32 *volt_data = (u32 *)volt;
	int cnt;

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.mailbox_log_idx) &
				(ARRAY_SIZE(ess_log->mailbox) - 1);

		ess_log->mailbox[i].time = cpu_clock(cpu);
		ess_log->mailbox[i].mode = mode;
		ess_log->mailbox[i].cpu = cpu;
		ess_log->mailbox[i].name = f_name;
		ess_log->mailbox[i].atl_vol = volt_data[0];
		ess_log->mailbox[i].apo_vol = volt_data[1];
		ess_log->mailbox[i].g3d_vol = volt_data[2];
		ess_log->mailbox[i].mif_vol = volt_data[3];
		for (cnt = 0; cnt < 4; cnt++) {
			ess_log->mailbox[i].buf[cnt] = msg_data[cnt];
		}
	}
}
#endif

void __exynos_ss_irq(int irq, void *fn, unsigned long val, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i;

		for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++) {
			if (irq == ess_irqlog_exlist[i])
				return;
		}

		i = atomic_inc_return(&ess_idx.irq_log_idx[cpu]) &
				(ARRAY_SIZE(ess_log->irq[0]) - 1);

		ess_log->irq[cpu][i].time = cpu_clock(cpu);
		ess_log->irq[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->irq[cpu][i].irq = irq;
		ess_log->irq[cpu][i].fn = (void *)fn;
		ess_log->irq[cpu][i].preempt = preempt_count();
		ess_log->irq[cpu][i].val = val;
		ess_log->irq[cpu][i].en = en;
	}
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
void __exynos_ss_irq_exit(unsigned int irq, unsigned long long start_time)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	unsigned long i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;

	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++)
		if (irq == 0 || irq == ess_irqexit_exlist[i])
			return;
	{
		int cpu = get_current_cpunum();
		unsigned long long time, latency;

		i = atomic_inc_return(&ess_idx.irq_exit_log_idx[cpu]) &
			(ARRAY_SIZE(ess_log->irq_exit[0]) - 1);

		time = cpu_clock(cpu);
		latency = time - start_time;

		if (unlikely(latency >
			(ess_irqexit_threshold * 1000))) {
			ess_log->irq_exit[cpu][i].latency = latency;
			ess_log->irq_exit[cpu][i].sp = (unsigned long) current_stack_pointer;
			ess_log->irq_exit[cpu][i].end_time = time;
			ess_log->irq_exit[cpu][i].time = start_time;
			ess_log->irq_exit[cpu][i].irq = irq;
		} else
			atomic_dec(&ess_idx.irq_exit_log_idx[cpu]);
	}
}
#endif

#ifdef CONFIG_ARM64
static inline unsigned long pure_arch_local_irq_save(void)
{
	unsigned long flags;

	asm volatile(
		"mrs	%0, daif		// arch_local_irq_save\n"
		"msr	daifset, #2"
		: "=r" (flags)
		:
		: "memory");

	return flags;
}

static inline void pure_arch_local_irq_restore(unsigned long flags)
{
	asm volatile(
		"msr    daif, %0                // arch_local_irq_restore"
		:
		: "r" (flags)
		: "memory");
}
#else
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	cpsid	i"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

static inline void arch_local_irq_restore(unsigned long flags)
{
	asm volatile(
		"	msr	cpsr_c, %0	@ local_irq_restore"
		:
		: "r" (flags)
		: "memory", "cc");
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
void __exynos_ss_spinlock(void *v_lock, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned index = atomic_inc_return(&ess_idx.spinlock_log_idx[cpu]);
		unsigned long j, i = index & (ARRAY_SIZE(ess_log->spinlock[0]) - 1);
		raw_spinlock_t *lock = (raw_spinlock_t *)v_lock;
		struct task_struct *task = (struct task_struct *)lock->owner;

#ifdef CONFIG_ARM_ARCH_TIMER
		ess_log->spinlock[cpu][i].time = cpu_clock(cpu);
#else
		ess_log->spinlock[cpu][i].time = index;
#endif
		ess_log->spinlock[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->spinlock[cpu][i].jiffies = jiffies_64;
		ess_log->spinlock[cpu][i].owner = task;
		ess_log->spinlock[cpu][i].task_comm = task->comm;
		ess_log->spinlock[cpu][i].owner_cpu = lock->owner_cpu;
		ess_log->spinlock[cpu][i].en = en;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->spinlock[cpu][i].caller[j] =
				(void *)((size_t)return_address(j + 1));
		}
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
void __exynos_ss_irqs_disabled(unsigned long flags)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	int cpu = get_current_cpunum();

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;

	if (unlikely(flags)) {
		unsigned j, local_flags = pure_arch_local_irq_save();

		/* If flags has one, it shows interrupt enable status */
		atomic_set(&ess_idx.irqs_disabled_log_idx[cpu], -1);
		ess_log->irqs_disabled[cpu][0].time = 0;
		ess_log->irqs_disabled[cpu][0].index = 0;
		ess_log->irqs_disabled[cpu][0].task = NULL;
		ess_log->irqs_disabled[cpu][0].task_comm = NULL;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->irqs_disabled[cpu][0].caller[j] = NULL;
		}

		pure_arch_local_irq_restore(local_flags);
	} else {
		unsigned index = atomic_inc_return(&ess_idx.irqs_disabled_log_idx[cpu]);
		unsigned long j, i = index % ARRAY_SIZE(ess_log->irqs_disabled[0]);

		ess_log->irqs_disabled[cpu][0].time = jiffies_64;
		ess_log->irqs_disabled[cpu][i].index = index;
		ess_log->irqs_disabled[cpu][i].task = get_current();
		ess_log->irqs_disabled[cpu][i].task_comm = get_current()->comm;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->irqs_disabled[cpu][i].caller[j] =
				(void *)((size_t)return_address(j + 1));
		}
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
void __exynos_ss_clk(void *clock, const char *func_name, int mode)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.clk_log_idx) &
				(ARRAY_SIZE(ess_log->clk) - 1);

		ess_log->clk[i].time = cpu_clock(cpu);
		ess_log->clk[i].mode = mode;
		ess_log->clk[i].clk = (struct vclk *)clock;
		ess_log->clk[i].f_name = func_name;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
void __exynos_ss_freq(int type, unsigned long old_freq, unsigned long target_freq, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.freq_log_idx) &
				(ARRAY_SIZE(ess_log->freq) - 1);

		ess_log->freq[i].time = cpu_clock(cpu);
		ess_log->freq[i].cpu = cpu;
		ess_log->freq[i].freq_name = ess_freq_name[type];
		ess_log->freq[i].old_freq = old_freq;
		ess_log->freq[i].target_freq = target_freq;
		ess_log->freq[i].en = en;

#ifdef CONFIG_SEC_DEBUG_AUTO_SUMMARY
		if(func_hook_auto_comm_lastfreq && en == ESS_FLAG_OUT)
			func_hook_auto_comm_lastfreq(type, old_freq, target_freq, ess_log->freq[i].time);
#endif

	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
void __exynos_ss_hrtimer(void *timer, s64 *now, void *fn, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.hrtimer_log_idx[cpu]) &
				(ARRAY_SIZE(ess_log->hrtimers[0]) - 1);

		ess_log->hrtimers[cpu][i].time = cpu_clock(cpu);
		ess_log->hrtimers[cpu][i].now = *now;
		ess_log->hrtimers[cpu][i].timer = (struct hrtimer *)timer;
		ess_log->hrtimers[cpu][i].fn = fn;
		ess_log->hrtimers[cpu][i].en = en;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static phys_addr_t virt_to_phys_high(size_t vaddr)
{
	phys_addr_t paddr = 0;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	if (virt_addr_valid((void *) vaddr)) {
		paddr = virt_to_phys((void *) vaddr);
		goto out;
	}

	pgd = pgd_offset_k(vaddr);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		goto out;

	if (pgd_val(*pgd) & 2) {
		paddr = pgd_val(*pgd) & SECTION_MASK;
		goto out;
	}

	pmd = pmd_offset((pud_t *)pgd, vaddr);
	if (pmd_none_or_clear_bad(pmd))
		goto out;

	pte = pte_offset_kernel(pmd, vaddr);
	if (pte_none(*pte))
		goto out;

	paddr = pte_val(*pte) & PAGE_MASK;

out:
	return paddr | (vaddr & UL(SZ_4K - 1));
}

void __exynos_ss_reg(unsigned int read, size_t val, size_t reg, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	int cpu = get_current_cpunum();
	unsigned long i, j;
	size_t phys_reg, start_addr, end_addr;

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;

	if (ess_reg_exlist[0].addr == 0)
		return;

	phys_reg = virt_to_phys_high(reg);
	if (unlikely(!phys_reg))
		return;

	for (j = 0; j < ARRAY_SIZE(ess_reg_exlist); j++) {
		if (ess_reg_exlist[j].addr == 0)
			break;
		start_addr = ess_reg_exlist[j].addr;
		end_addr = start_addr + ess_reg_exlist[j].size;
		if (start_addr <= phys_reg && phys_reg <= end_addr)
			return;
	}

	i = atomic_inc_return(&ess_idx.reg_log_idx[cpu]) &
		(ARRAY_SIZE(ess_log->reg[0]) - 1);

	ess_log->reg[cpu][i].time = cpu_clock(cpu);
	ess_log->reg[cpu][i].read = read;
	ess_log->reg[cpu][i].val = val;
	ess_log->reg[cpu][i].reg = phys_reg;
	ess_log->reg[cpu][i].en = en;

	for (j = 0; j < ess_desc.callstack; j++) {
		ess_log->reg[cpu][i].caller[j] =
			(void *)((size_t)return_address(j + 1));
	}
}
#endif

#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
void __exynos_ss_clockevent(void* dev, unsigned long long clc, int64_t delta, void *next_event)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned i;

		i = atomic_inc_return(&ess_idx.clockevent_log_idx[cpu]) &
			(ARRAY_SIZE(ess_log->clockevent[0]) - 1);

		ess_log->clockevent[cpu][i].time = cpu_clock(cpu);
		ess_log->clockevent[cpu][i].clc = clc;
		ess_log->clockevent[cpu][i].dev = (struct clock_event_device*)dev;
		ess_log->clockevent[cpu][i].delta = delta;
		ess_log->clockevent[cpu][i].next_event = *((ktime_t *)next_event);
	}
}

void __exynos_ss_printk(const char *fmt, ...)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		va_list args;
		unsigned long j, i = atomic_inc_return(&ess_idx.printk_log_idx) &
				(ARRAY_SIZE(ess_log->printk) - 1);

		va_start(args, fmt);
		vsnprintf(ess_log->printk[i].log,
				sizeof(ess_log->printk[i].log), fmt, args);
		va_end(args);

		ess_log->printk[i].time = cpu_clock(cpu);
		ess_log->printk[i].cpu = cpu;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->printk[i].caller[j] =
				(void *)((size_t)return_address(j));
		}
	}
}

void __exynos_ss_printkl(size_t msg, size_t val)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled || !item->entry.enabled_init))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long j, i = atomic_inc_return(&ess_idx.printkl_log_idx) &
				(ARRAY_SIZE(ess_log->printkl) - 1);

		ess_log->printkl[i].time = cpu_clock(cpu);
		ess_log->printkl[i].cpu = cpu;
		ess_log->printkl[i].msg = msg;
		ess_log->printkl[i].val = val;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->printkl[i].caller[j] =
				(void *)((size_t)return_address(j));
		}
	}
}
#endif

/* This defines are for PSTORE */
#define ESS_LOGGER_LEVEL_HEADER 	(1)
#define ESS_LOGGER_LEVEL_PREFIX 	(2)
#define ESS_LOGGER_LEVEL_TEXT		(3)
#define ESS_LOGGER_LEVEL_MAX		(4)
#define ESS_LOGGER_SKIP_COUNT		(4)
#define ESS_LOGGER_STRING_PAD		(1)
#define ESS_LOGGER_HEADER_SIZE		(68)

#define ESS_LOG_ID_MAIN 		(0)
#define ESS_LOG_ID_RADIO		(1)
#define ESS_LOG_ID_EVENTS		(2)
#define ESS_LOG_ID_SYSTEM		(3)
#define ESS_LOG_ID_CRASH		(4)
#define ESS_LOG_ID_KERNEL		(5)

typedef struct __attribute__((__packed__)) {
    uint8_t magic;
    uint16_t len;
    uint16_t uid;
    uint16_t pid;
} ess_pmsg_log_header_t;

typedef struct __attribute__((__packed__)) {
	unsigned char id;
	uint16_t tid;
	int32_t tv_sec;
	int32_t tv_nsec;
} ess_android_log_header_t;

#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
typedef struct ess_logger {
	uint16_t	len;
	uint16_t	id;
	uint16_t	pid;
	uint16_t	tid;
	uint16_t	uid;
	uint16_t	level;
	int32_t		tv_sec;
	int32_t		tv_nsec;
	char		msg[0];
	char*		buffer;
	void		(*func_hook_logger)(const char*, const char*, size_t);
} __attribute__((__packed__)) ess_logger;

static ess_logger logger;

void register_hook_pstore_log(void (*func)(const char *name, const char *buf, size_t size))
{
	logger.func_hook_logger = func;
	logger.buffer = vmalloc(PAGE_SIZE * 3);

	if (logger.buffer)
		pr_info("exynos-snapshot: logger buffer alloc address: 0x%p\n", logger.buffer);
}
EXPORT_SYMBOL(register_hook_pstore_log);
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
#ifdef CONFIG_SEC_EVENT_LOG
struct event_log_tag_t {
	int nTagNum;
	char *event_msg;
};

enum event_type {
	EVENT_TYPE_INT		= 0,
	EVENT_TYPE_LONG		= 1,
	EVENT_TYPE_STRING	= 2,
	EVENT_TYPE_LIST		= 3,
	EVENT_TYPE_FLOAT 	= 4, 
};

// NOTICE : it must have order.
struct event_log_tag_t event_tags[] = {
	{ 42 , "answer"},
	{ 314 , "pi"},
	{ 1003 , "auditd"},
	{ 2718 , "e"},
	{ 2719 , "configuration_changed"},
	{ 2720 , "sync"},
	{ 2721 , "cpu"},
	{ 2722 , "battery_level"},
	{ 2723 , "battery_status"},
	{ 2724 , "power_sleep_requested"},
	{ 2725 , "power_screen_broadcast_send"},
	{ 2726 , "power_screen_broadcast_done"},
	{ 2727 , "power_screen_broadcast_stop"},
	{ 2728 , "power_screen_state"},
	{ 2729 , "power_partial_wake_state"},
	{ 2730 , "battery_discharge"},
	{ 2740 , "location_controller"},
	{ 2741 , "force_gc"},
	{ 2742 , "tickle"},
	{ 2744 , "free_storage_changed"},
	{ 2745 , "low_storage"},
	{ 2746 , "free_storage_left"},
	{ 2747 , "contacts_aggregation"},
	{ 2748 , "cache_file_deleted"},
	{ 2750 , "notification_enqueue"},
	{ 2751 , "notification_cancel"},
	{ 2752 , "notification_cancel_all"},
	{ 2753 , "idle_maintenance_window_start"},
	{ 2754 , "idle_maintenance_window_finish"},
	{ 2755 , "fstrim_start"},
	{ 2756 , "fstrim_finish"},
	{ 2802 , "watchdog"},
	{ 2803 , "watchdog_proc_pss"},
	{ 2804 , "watchdog_soft_reset"},
	{ 2805 , "watchdog_hard_reset"},
	{ 2806 , "watchdog_pss_stats"},
	{ 2807 , "watchdog_proc_stats"},
	{ 2808 , "watchdog_scheduled_reboot"},
	{ 2809 , "watchdog_meminfo"},
	{ 2810 , "watchdog_vmstat"},
	{ 2811 , "watchdog_requested_reboot"},
	{ 2820 , "backup_data_changed"},
	{ 2821 , "backup_start"},
	{ 2822 , "backup_transport_failure"},
	{ 2823 , "backup_agent_failure"},
	{ 2824 , "backup_package"},
	{ 2825 , "backup_success"},
	{ 2826 , "backup_reset"},
	{ 2827 , "backup_initialize"},
	{ 2830 , "restore_start"},
	{ 2831 , "restore_transport_failure"},
	{ 2832 , "restore_agent_failure"},
	{ 2833 , "restore_package"},
	{ 2834 , "restore_success"},
	{ 2840 , "full_backup_package"},
	{ 2841 , "full_backup_agent_failure"},
	{ 2842 , "full_backup_transport_failure"},
	{ 2843 , "full_backup_success"},
	{ 2844 , "full_restore_package"},
	{ 2850 , "backup_transport_lifecycle"},
	{ 3000 , "boot_progress_start"},
	{ 3010 , "boot_progress_system_run"},
	{ 3020 , "boot_progress_preload_start"},
	{ 3030 , "boot_progress_preload_end"},
	{ 3040 , "boot_progress_ams_ready"},
	{ 3050 , "boot_progress_enable_screen"},
	{ 3060 , "boot_progress_pms_start"},
	{ 3070 , "boot_progress_pms_system_scan_start"},
	{ 3080 , "boot_progress_pms_data_scan_start"},
	{ 3090 , "boot_progress_pms_scan_end"},
	{ 3100 , "boot_progress_pms_ready"},
	{ 3110 , "unknown_sources_enabled"},
	{ 3120 , "pm_critical_info"},
	{ 4000 , "calendar_upgrade_receiver"},
	{ 4100 , "contacts_upgrade_receiver"},
	{ 20003 , "dvm_lock_sample"},
	{ 27500 , "notification_panel_revealed"},
	{ 27501 , "notification_panel_hidden"},
	{ 27510 , "notification_visibility_changed"},
	{ 27511 , "notification_expansion"},
	{ 27520 , "notification_clicked"},
	{ 27530 , "notification_canceled"},
	{ 27531 , "notification_visibility" },
	{ 30001 , "am_finish_activity"},
	{ 30002 , "am_task_to_front"},
	{ 30003 , "am_new_intent"},
	{ 30004 , "am_create_task"},
	{ 30005 , "am_create_activity"},
	{ 30006 , "am_restart_activity"},
	{ 30007 , "am_resume_activity"},
	{ 30008 , "am_anr"},
	{ 30009 , "am_activity_launch_time"},
	{ 30010 , "am_proc_bound"},
	{ 30011 , "am_proc_died"},
	{ 30012 , "am_failed_to_pause"},
	{ 30013 , "am_pause_activity"},
	{ 30014 , "am_proc_start"},
	{ 30015 , "am_proc_bad"},
	{ 30016 , "am_proc_good"},
	{ 30017 , "am_low_memory"},
	{ 30018 , "am_destroy_activity"},
	{ 30019 , "am_relaunch_resume_activity"},
	{ 30020 , "am_relaunch_activity"},
	{ 30021 , "am_on_paused_called"},
	{ 30022 , "am_on_resume_called"},
	{ 30023 , "am_kill"},
	{ 30024 , "am_broadcast_discard_filter"},
	{ 30025 , "am_broadcast_discard_app"},
	{ 30030 , "am_create_service"},
	{ 30031 , "am_destroy_service"},
	{ 30032 , "am_process_crashed_too_much"},
	{ 30033 , "am_drop_process"},
	{ 30034 , "am_service_crashed_too_much"},
	{ 30035 , "am_schedule_service_restart"},
	{ 30036 , "am_provider_lost_process"},
	{ 30037 , "am_process_start_timeout"},
	{ 30039 , "am_crash"},
	{ 30040 , "am_wtf"},
	{ 30041 , "am_switch_user"},
	{ 30042 , "am_activity_fully_drawn_time"},
	{ 30043 , "am_focused_activity"},	
	{ 30044 , "am_focused_stack"},
	{ 30045 , "am_pre_boot"},
	{ 30046 , "am_meminfo"},	
	{ 30047 , "am_pss"},	
	{ 30048 , "am_stop_activity"},
	{ 30049 , "am_on_stop_called"},	
	{ 30050 , "am_mem_factor"},	
	{ 31000 , "wm_no_surface_memory"},
	{ 31001 , "wm_task_created"},
	{ 31002 , "wm_task_moved"},
	{ 31003 , "wm_task_removed"},
	{ 31004 , "wm_stack_created"},
	{ 31005 , "wm_home_stack_moved"},
	{ 31006 , "wm_stack_removed"},
	{ 31007 , "boot_progress_enable_screen"},
	{ 32000 , "imf_force_reconnect_ime"},
	{ 36000 , "sysui_statusbar_touch"},
	{ 36001 , "sysui_heads_up_status"},
	{ 36004 , "sysui_status_bar_state"},
	{ 36010 , "sysui_panelbar_touch"},
	{ 36020 , "sysui_notificationpanel_touch"},
	{ 36030 , "sysui_quickpanel_touch"},
	{ 36040 , "sysui_panelholder_touch"},
	{ 36050 , "sysui_searchpanel_touch"},
	{ 40000 , "volume_changed" },
	{ 40001 , "stream_devices_changed" },
	{ 50000 , "menu_item_selected"},
	{ 50001 , "menu_opened"},
	{ 50020 , "connectivity_state_changed"},
	{ 50021 , "wifi_state_changed"},
	{ 50022 , "wifi_event_handled"},
	{ 50023 , "wifi_supplicant_state_changed"},
	{ 50100 , "pdp_bad_dns_address"},
	{ 50101 , "pdp_radio_reset_countdown_triggered"},
	{ 50102 , "pdp_radio_reset"},
	{ 50103 , "pdp_context_reset"},
	{ 50104 , "pdp_reregister_network"},
	{ 50105 , "pdp_setup_fail"},
	{ 50106 , "call_drop"},
	{ 50107 , "data_network_registration_fail"},
	{ 50108 , "data_network_status_on_radio_off"},
	{ 50109 , "pdp_network_drop"},
	{ 50110 , "cdma_data_setup_failed"},
	{ 50111 , "cdma_data_drop"},
	{ 50112 , "gsm_rat_switched"},
	{ 50113 , "gsm_data_state_change"},
	{ 50114 , "gsm_service_state_change"},
	{ 50115 , "cdma_data_state_change"},
	{ 50116 , "cdma_service_state_change"},
	{ 50117 , "bad_ip_address"},
	{ 50118 , "data_stall_recovery_get_data_call_list"},
	{ 50119 , "data_stall_recovery_cleanup"},
	{ 50120 , "data_stall_recovery_reregister"},
	{ 50121 , "data_stall_recovery_radio_restart"},
	{ 50122 , "data_stall_recovery_radio_restart_with_prop"},
	{ 50123 , "gsm_rat_switched_new"},
	{ 50125 , "exp_det_sms_denied_by_user"},
	{ 50128 , "exp_det_sms_sent_by_user"},
	{ 51100 , "netstats_mobile_sample"},
	{ 51101 , "netstats_wifi_sample"},
	{ 51200 , "lockdown_vpn_connecting"},
	{ 51201 , "lockdown_vpn_connected"},
	{ 51202 , "lockdown_vpn_error"},
	{ 51300 , "config_install_failed"},
	{ 51400 , "ifw_intent_matched"},
	{ 52000 , "db_sample"},
	{ 52001 , "http_stats"},
	{ 52002 , "content_query_sample"},
	{ 52003 , "content_update_sample"},
	{ 52004 , "binder_sample"},
	{ 60000 , "viewroot_draw"},
	{ 60001 , "viewroot_layout"},
	{ 60002 , "view_build_drawing_cache"},
	{ 60003 , "view_use_drawing_cache"},
	{ 60100 , "sf_frame_dur"},
	{ 60110 , "sf_stop_bootanim"},
	{ 65537 , "exp_det_netlink_failure"},
	{ 70000 , "screen_toggled"},
	{ 70101 , "browser_zoom_level_change"},
	{ 70102 , "browser_double_tap_duration"},
	{ 70103 , "browser_bookmark_added"},
	{ 70104 , "browser_page_loaded"},
	{ 70105 , "browser_timeonpage"},
	{ 70150 , "browser_snap_center"},
	{ 70151 , "exp_det_attempt_to_call_object_getclass"},
	{ 70200 , "aggregation"},
	{ 70201 , "aggregation_test"},
	{ 70300 , "telephony_event"},
	{ 70301 , "phone_ui_enter"},
	{ 70302 , "phone_ui_exit"},
	{ 70303 , "phone_ui_button_click"},
	{ 70304 , "phone_ui_ringer_query_elapsed"},
	{ 70305 , "phone_ui_multiple_query"},
	{ 70310 , "telecom_event"},
	{ 70311 , "telecom_service"},
	{ 71001 , "qsb_start"},
	{ 71002 , "qsb_click"},
	{ 71003 , "qsb_search"},
	{ 71004 , "qsb_voice_search"},
	{ 71005 , "qsb_exit"},
	{ 71006 , "qsb_latency"},
	{ 73001 , "input_dispatcher_slow_event_processing"},
	{ 73002 , "input_dispatcher_stale_event"},
	{ 73100 , "looper_slow_lap_time"},
	{ 73200 , "choreographer_frame_skip"},
	{ 75000 , "sqlite_mem_alarm_current"},
	{ 75001 , "sqlite_mem_alarm_max"},
	{ 75002 , "sqlite_mem_alarm_alloc_attempt"},
	{ 75003 , "sqlite_mem_released"},
	{ 75004 , "sqlite_db_corrupt"},
	{ 76001 , "tts_speak_success"},
	{ 76002 , "tts_speak_failure"},
	{ 76003 , "tts_v2_speak_success"},
	{ 76004 , "tts_v2_speak_failure"},
	{ 78001 , "exp_det_dispatchCommand_overflow"},
	{ 80100 , "bionic_event_memcpy_buffer_overflow"},
	{ 80105 , "bionic_event_strcat_buffer_overflow"},
	{ 80110 , "bionic_event_memmov_buffer_overflow"},
	{ 80115 , "bionic_event_strncat_buffer_overflow"},
	{ 80120 , "bionic_event_strncpy_buffer_overflow"},
	{ 80125 , "bionic_event_memset_buffer_overflow"},
	{ 80130 , "bionic_event_strcpy_buffer_overflow"},
	{ 80200 , "bionic_event_strcat_integer_overflow"},
	{ 80205 , "bionic_event_strncat_integer_overflow"},
	{ 80300 , "bionic_event_resolver_old_response"},
	{ 80305 , "bionic_event_resolver_wrong_server"},
	{ 80310 , "bionic_event_resolver_wrong_query"},
	{ 90100 , "exp_det_cert_pin_failure"},
	{ 90200 , "lock_screen_type"},
	{ 90201 , "exp_det_device_admin_activated_by_user"},
	{ 90202 , "exp_det_device_admin_declined_by_user"},
	{ 90300 , "install_package_attempt"},
	{ 201001 , "system_update"},
	{ 201002 , "system_update_user"},
	{ 202001 , "vending_reconstruct"},
	{ 202901 , "transaction_event"},
	{ 203001 , "sync_details"},
	{ 203002 , "google_http_request"},
	{ 204001 , "gtalkservice"},
	{ 204002 , "gtalk_connection"},
	{ 204003 , "gtalk_conn_close"},
	{ 204004 , "gtalk_heartbeat_reset"},
	{ 204005 , "c2dm"},
	{ 205001 , "setup_server_timeout"},
	{ 205002 , "setup_required_captcha"},
	{ 205003 , "setup_io_error"},
	{ 205004 , "setup_server_error"},
	{ 205005 , "setup_retries_exhausted"},
	{ 205006 , "setup_no_data_network"},
	{ 205007 , "setup_completed"},
	{ 205008 , "gls_account_tried"},
	{ 205009 , "gls_account_saved"},
	{ 205010 , "gls_authenticate"},
	{ 205011 , "google_mail_switch"},
	{ 206001 , "snet"},
	{ 206003 , "exp_det_snet"},
	{ 1050101 , "nitz_information"},
	{ 1230000 , "am_create_stack"},
	{ 1230001 , "am_remove_stack"},
	{ 1230002 , "am_move_task_to_stack"},
	{ 1230003 , "am_exchange_task_to_stack"},
	{ 1230004 , "am_create_task_to_stack"},
	{ 1230005 , "am_focus_stack"},
	{ 1260001 , "vs_move_task_to_display"},
	{ 1260002 , "vs_create_display"},
	{ 1260003 , "vs_remove_display"},
	{ 1261000 , "am_start_user "},
	{ 1261001 , "am_stop_user "},
	{ 1397638484 , "snet_event_log"},
};

static const char * find_tag_name_from_id ( int id )
{
	int l = 0;
	int r = ARRAY_SIZE(event_tags)-1;
	int mid = 0;
	
	while ( l <= r )
	{		
		mid = (l+r)/2;

		if (event_tags[mid].nTagNum == id )
			return event_tags[mid].event_msg;
		else if ( event_tags[mid].nTagNum < id )
			l = mid + 1;
		else
			r = mid - 1;
	}
	
	return NULL;	
}

static char * parse_buffer(char *buffer, unsigned char type)
{
	unsigned int buf_len =0;
	char buf[64] = {0};
	
	switch(type)
	{
		case EVENT_TYPE_INT:
		{
			int val = *(int *)buffer;
			buffer+=sizeof(int);
			buf_len = snprintf(buf, 64, "%d", val);
			logger.func_hook_logger("log_main", buf, buf_len);
		}
		break;
		case EVENT_TYPE_LONG:
		{
			long long val = *(long long *)buffer;
			buffer+=sizeof(long long);
			buf_len = snprintf(buf, 64, "%lld", val);
			logger.func_hook_logger("log_main", buf, buf_len);
		}
		break;
		case EVENT_TYPE_FLOAT:
		{
//			float val = *(float  *)buffer;
			buffer+=sizeof(float);
//			buf_len = snprintf(buf, 64, "%f", val);
//			logger.func_hook_logger("log_platform", buf, buf_len);
		}
		break;
		case EVENT_TYPE_STRING:
		{
  			unsigned int len = *(int *)buffer;
  			unsigned int _len = len;
  			if ( len >= 64 ) len = 63;
  			buffer+=sizeof(int);
			memcpy(buf, buffer, len);
 			logger.func_hook_logger("log_main", buf, len);
            buffer+=_len;
		}
		break;
	}
	
	return buffer;
}

#endif

static int exynos_ss_combine_pmsg(char *buffer, size_t count, unsigned int level)
{
	char *logbuf = logger.buffer;
	if (!logbuf)
		return -ENOMEM;

	switch(level) {
	case ESS_LOGGER_LEVEL_HEADER:
		{
			struct tm tmBuf;
			u64 tv_kernel;
			unsigned int logbuf_len;
			unsigned long rem_nsec;
#ifndef	CONFIG_SEC_EVENT_LOG
			if (logger.id == ESS_LOG_ID_EVENTS)
				break;
#endif
			tv_kernel = local_clock();
			rem_nsec = do_div(tv_kernel, 1000000000);
			time_to_tm(logger.tv_sec, 0, &tmBuf);

			logbuf_len = snprintf(logbuf, ESS_LOGGER_HEADER_SIZE,
					"\n[%5lu.%06lu][%d:%16s] %02d-%02d %02d:%02d:%02d.%03d %5d %5d  ",
					(unsigned long)tv_kernel, rem_nsec / 1000,
					raw_smp_processor_id(), current->comm,
					tmBuf.tm_mon + 1, tmBuf.tm_mday,
					tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
					logger.tv_nsec / 1000000, logger.pid, logger.tid);

			logger.func_hook_logger("log_main", logbuf, logbuf_len);
		}
		break;
	case ESS_LOGGER_LEVEL_PREFIX:
		{
			static const char* kPrioChars = "!.VDIWEFS";
			unsigned char prio = logger.msg[0];

			if (logger.id == ESS_LOG_ID_EVENTS)
				break;

			logbuf[0] = prio < strlen(kPrioChars) ? kPrioChars[prio] : '?';
			logbuf[1] = ' ';
			
#ifdef CONFIG_SEC_EVENT_LOG
			logger.msg[0] = 0xff;
#endif

			logger.func_hook_logger("log_main", logbuf, ESS_LOGGER_LEVEL_PREFIX);
		}
		break;
	case ESS_LOGGER_LEVEL_TEXT:
		{
			char *eatnl = buffer + count - ESS_LOGGER_STRING_PAD;

			if (logger.id == ESS_LOG_ID_EVENTS)
			{
#ifdef CONFIG_SEC_EVENT_LOG
				unsigned int buf_len;
				char buf[64] = {0};
				int tag_id = *(int *)buffer;
				const char * tag_name  = NULL;
				
				if ( count == 4 && (tag_name = find_tag_name_from_id(tag_id)) != NULL )
				{					
					buf_len = snprintf(buf, 64, "# %s ", tag_name);
					logger.func_hook_logger("log_main", buf, buf_len);
				}
				else
				{
					// SINGLE ITEM
					// logger.msg[0] => count == 1 , if event log, it is type.
					if ( logger.msg[0] == EVENT_TYPE_LONG || logger.msg[0] == EVENT_TYPE_INT || logger.msg[0] == EVENT_TYPE_FLOAT )
						parse_buffer(buffer, logger.msg[0]);
					else if ( count > 6 ) // TYPE(1) + ITEMS(1) + SINGLEITEM(5) or STRING(2+4+1>..)
					// CASE 2,3:
					// STRING OR LIST ITEM
					{
						if ( *buffer == EVENT_TYPE_LIST )
						{
							unsigned char items = *(buffer+1);
							unsigned char i = 0;
							buffer+=2;
							
							logger.func_hook_logger("log_main", "[", 1);
							
							for (;i<items;++i)
							{
								unsigned char type = *buffer;
								buffer++;
								buffer = parse_buffer(buffer, type);
								logger.func_hook_logger("log_main", ":", 1);
							}
							
							logger.func_hook_logger("log_main", "]", 1);
		
						}
						else if ( *buffer == EVENT_TYPE_STRING )
							parse_buffer(buffer+1, EVENT_TYPE_STRING);
					}
						
					logger.msg[0]=0xff; // dummy value;	
				}
#else
				break;
#endif
			}
			else
			{
				if (count == ESS_LOGGER_SKIP_COUNT && *eatnl != '\0')
					break;

				memcpy((void *)logbuf, buffer, count);
				eatnl = logbuf + count - ESS_LOGGER_STRING_PAD;

				/* Mark End of String for safe to buffer */
				*eatnl = '\0';
				while (--eatnl >= logbuf) {
					if (*eatnl == '\n' || *eatnl == '\0')
						*eatnl = ' ';
				};

				logger.func_hook_logger("log_main", logbuf, count);
#ifdef CONFIG_SEC_EXT
				if (count > 1 && strncmp(logbuf, "!@", 2) == 0) {
					logbuf[count-1] = '\0';
					pr_info("%s\n", logbuf);
#ifdef CONFIG_SEC_BSP
					if (count > 5 && strncmp(logbuf, "!@Boot", 6) == 0){
						sec_boot_stat_add(logbuf);
					}
#endif /* CONFIG_SEC_BSP */
				}
#endif /* CONFIG_SEC_EXT */
			}
		}
		break;
	default:
		break;
	}
	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
int exynos_ss_hook_pmsg(char *buffer, size_t count)
{
	ess_android_log_header_t header;
	ess_pmsg_log_header_t pmsg_header;

	if (!logger.buffer)
		return -ENOMEM;

	switch(count) {
	case sizeof(pmsg_header):
		memcpy((void *)&pmsg_header, buffer, count);
		if (pmsg_header.magic != 'l') {
			exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		} else {
			/* save logger data */
			logger.pid = pmsg_header.pid;
			logger.uid = pmsg_header.uid;
			logger.len = pmsg_header.len;
		}
		break;
	case sizeof(header):
		/* save logger data */
		memcpy((void *)&header, buffer, count);
		logger.id = header.id;
		logger.tid = header.tid;
		logger.tv_sec = header.tv_sec;
		logger.tv_nsec  = header.tv_nsec;
		if (logger.id < 0 || logger.id > 7) {
			/* write string */
			exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		} else {
			/* write header */
			exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_HEADER);
		}
		break;
	case sizeof(unsigned char):
		logger.msg[0] = buffer[0];
		/* write char for prefix */
		exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_PREFIX);
		break;
	default:
		/* write string */
		exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(exynos_ss_hook_pmsg);
#endif

/*
 *  To support pstore/pmsg/pstore_ram, following is implementation for exynos-snapshot
 *  ess_ramoops platform_device is used by pstore fs.
 */

#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
static struct ramoops_platform_data ess_ramoops_data = {
	.record_size	= SZ_512K,
	.console_size	= SZ_512K,
	.ftrace_size	= SZ_512K,
	.pmsg_size	= SZ_512K,
	.dump_oops	= 1,
};

static struct platform_device ess_ramoops = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ess_ramoops_data,
	},
};

static int __init ess_pstore_init(void)
{
	if (exynos_ss_get_enable("log_pstore", true)) {
		ess_ramoops_data.mem_size = exynos_ss_get_item_size("log_pstore");
		ess_ramoops_data.mem_address = exynos_ss_get_item_paddr("log_pstore");
	}
	return platform_device_register(&ess_ramoops);
}

static void __exit ess_pstore_exit(void)
{
	platform_device_unregister(&ess_ramoops);
}
module_init(ess_pstore_init);
module_exit(ess_pstore_exit);

MODULE_DESCRIPTION("Exynos Snapshot pstore module");
MODULE_LICENSE("GPL");
#endif

/*
 *  sysfs implementation for exynos-snapshot
 *  you can access the sysfs of exynos-snapshot to /sys/devices/system/exynos-ss
 *  path.
 */
static struct bus_type ess_subsys = {
	.name = "exynos-ss",
	.dev_name = "exynos-ss",
};

static ssize_t ess_enable_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	struct exynos_ss_item *item;
	unsigned long i;
	ssize_t n = 0;

	/*  item  */
	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		item = &ess_items[i];
		n += scnprintf(buf + n, 24, "%-12s : %sable\n",
			item->name, item->entry.enabled ? "en" : "dis");
        }

	/*  base  */
	n += scnprintf(buf + n, 24, "%-12s : %sable\n",
			"base", ess_base.enabled ? "en" : "dis");

	return n;
}

static ssize_t ess_enable_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int en;
	char *name;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	name[count - 1] = '\0';

	en = exynos_ss_get_enable(name, false);

	if (en == -1)
		pr_info("echo name > enabled\n");
	else {
		if (en)
			exynos_ss_set_enable(name, false);
		else
			exynos_ss_set_enable(name, true);
	}

	kfree(name);
	return count;
}

static ssize_t ess_callstack_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n = scnprintf(buf, 24, "callstack depth : %d\n", ess_desc.callstack);

	return n;
}

static ssize_t ess_callstack_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long callstack;

	callstack = simple_strtoul(buf, NULL, 0);
	pr_info("callstack depth(min 1, max 4) : %lu\n", callstack);

	if (callstack < 5 && callstack > 0) {
		ess_desc.callstack = callstack;
		pr_info("success inserting %lu to callstack value\n", callstack);
	}
	return count;
}

static ssize_t ess_irqlog_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned long i;
	ssize_t n = 0;

	n = scnprintf(buf, 24, "excluded irq number\n");

	for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++) {
		if (ess_irqlog_exlist[i] == 0)
			break;
		n += scnprintf(buf + n, 24, "irq num: %-4d\n", ess_irqlog_exlist[i]);
        }
	return n;
}

static ssize_t ess_irqlog_exlist_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long i;
	unsigned long irq;

	irq = simple_strtoul(buf, NULL, 0);
	pr_info("irq number : %lu\n", irq);

	for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++) {
		if (ess_irqlog_exlist[i] == 0)
			break;
	}

	if (i == ARRAY_SIZE(ess_irqlog_exlist)) {
		pr_err("list is full\n");
		return count;
	}

	if (irq != 0) {
		ess_irqlog_exlist[i] = irq;
		pr_info("success inserting %lu to list\n", irq);
	}
	return count;
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static ssize_t ess_irqexit_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned long i;
	ssize_t n = 0;

	n = scnprintf(buf, 36, "Excluded irq number\n");
	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++) {
		if (ess_irqexit_exlist[i] == 0)
			break;
		n += scnprintf(buf + n, 24, "IRQ num: %-4d\n", ess_irqexit_exlist[i]);
        }
	return n;
}

static ssize_t ess_irqexit_exlist_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long i;
	unsigned long irq;

	irq = simple_strtoul(buf, NULL, 0);
	pr_info("irq number : %lu\n", irq);

	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++) {
		if (ess_irqexit_exlist[i] == 0)
			break;
	}

	if (i == ARRAY_SIZE(ess_irqexit_exlist)) {
		pr_err("list is full\n");
		return count;
	}

	if (irq != 0) {
		ess_irqexit_exlist[i] = irq;
		pr_info("success inserting %lu to list\n", irq);
	}
	return count;
}

static ssize_t ess_irqexit_threshold_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	ssize_t n;

	n = scnprintf(buf, 46, "threshold : %12u us\n", ess_irqexit_threshold);
	return n;
}

static ssize_t ess_irqexit_threshold_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;

	val = simple_strtoul(buf, NULL, 0);
	pr_info("threshold value : %lu\n", val);

	if (val != 0) {
		ess_irqexit_threshold = val;
		pr_info("success %lu to threshold\n", val);
	}
	return count;
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static ssize_t ess_reg_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned long i;
	ssize_t n = 0;

	n = scnprintf(buf, 36, "excluded register address\n");
	for (i = 0; i < ARRAY_SIZE(ess_reg_exlist); i++) {
		if (ess_reg_exlist[i].addr == 0)
			break;
		n += scnprintf(buf + n, 40, "register addr: %08zx size: %08zx\n",
				ess_reg_exlist[i].addr, ess_reg_exlist[i].size);
        }
	return n;
}

static ssize_t ess_reg_exlist_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long i;
	size_t addr;

	addr = simple_strtoul(buf, NULL, 0);
	pr_info("register addr: %zx\n", addr);

	for (i = 0; i < ARRAY_SIZE(ess_reg_exlist); i++) {
		if (ess_reg_exlist[i].addr == 0)
			break;
	}
	if (addr != 0) {
		ess_reg_exlist[i].size = SZ_4K;
		ess_reg_exlist[i].addr = addr;
		pr_info("success %zx to threshold\n", (addr));
	}
	return count;
}
#endif

static struct kobj_attribute ess_enable_attr =
        __ATTR(enabled, 0644, ess_enable_show, ess_enable_store);
static struct kobj_attribute ess_callstack_attr =
        __ATTR(callstack, 0644, ess_callstack_show, ess_callstack_store);
static struct kobj_attribute ess_irqlog_attr =
        __ATTR(exlist_irqdisabled, 0644, ess_irqlog_exlist_show,
					ess_irqlog_exlist_store);
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static struct kobj_attribute ess_irqexit_attr =
        __ATTR(exlist_irqexit, 0644, ess_irqexit_exlist_show,
					ess_irqexit_exlist_store);
static struct kobj_attribute ess_irqexit_threshold_attr =
        __ATTR(threshold_irqexit, 0644, ess_irqexit_threshold_show,
					ess_irqexit_threshold_store);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static struct kobj_attribute ess_reg_attr =
        __ATTR(exlist_reg, 0644, ess_reg_exlist_show, ess_reg_exlist_store);
#endif

static struct attribute *ess_sysfs_attrs[] = {
	&ess_enable_attr.attr,
	&ess_callstack_attr.attr,
	&ess_irqlog_attr.attr,
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	&ess_irqexit_attr.attr,
	&ess_irqexit_threshold_attr.attr,
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	&ess_reg_attr.attr,
#endif
	NULL,
};

static struct attribute_group ess_sysfs_group = {
	.attrs = ess_sysfs_attrs,
};

static const struct attribute_group *ess_sysfs_groups[] = {
	&ess_sysfs_group,
	NULL,
};

static int __init exynos_ss_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&ess_subsys, ess_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos-snapshop subsys\n");

	return ret;
}
late_initcall(exynos_ss_sysfs_init);

#ifdef CONFIG_SEC_PM_DEBUG
static ssize_t sec_log_read_all(struct file *file, char __user *buf,
				size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;
	size_t size;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];

	if (sec_log_full)
		size = item->entry.size;
	else
		size = (size_t)(item->curr_ptr - item->head_ptr);

	if (pos >= size)
		return 0;

	count = min(len, size);

	if ((pos + count) > size)
		count = size - pos;

	if (copy_to_user(buf, item->head_ptr + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations sec_log_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_log_read_all,
};

static int __init sec_log_late_init(void)
{
	struct proc_dir_entry *entry;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];

	if (!item->head_ptr)
		return 0;

	entry = proc_create("sec_log", S_IRUSR | S_IRGRP, NULL, &sec_log_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, item->entry.size);

	return 0;
}

late_initcall(sec_log_late_init);
#endif
