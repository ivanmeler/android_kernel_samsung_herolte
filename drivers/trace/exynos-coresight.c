/*
 * linux/arch/arm/mach-exynos/exynos-coresight.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/of.h>

#include <asm/core_regs.h>
#include <asm/cputype.h>
#include <asm/io.h>

#include <soc/samsung/exynos-pmu.h>

#define CS_READ(base, offset)		__raw_readl(base + offset)
#define CS_READQ(base, offset)		__raw_readq(base + offset)
#define CS_WRITE(val, base, offset)	__raw_writel(val, base + offset)

#define SYS_READ(reg, val)	asm volatile("mrs %0, " #reg : "=r" (val))
#define SYS_WRITE(reg, val)	asm volatile("msr " #reg ", %0" :: "r" (val))

#define DBG_UNLOCK(base)	\
	do { mb(); __raw_writel(OSLOCK_MAGIC, base + DBGLAR); }while(0)
#define DBG_LOCK(base)		\
	do { __raw_writel(0x1, base + DBGLAR); mb(); }while(0)

#define DBG_REG_MAX_SIZE	(8)
#define DBG_BW_REG_MAX_SIZE	(30)
#define OS_LOCK_FLAG		(DBG_REG_MAX_SIZE - 1)
#define ITERATION		CONFIG_PC_ITERATION
#define CORE_CNT		CONFIG_NR_CPUS
#define MSB_PADDING		(0xFFFFFFC000000000)
#define MSB_MASKING		(0x0001ffc000000000)

struct cs_dbg_cpu {
	void __iomem		*base;
	ssize_t			reg[DBG_REG_MAX_SIZE];
};

struct cs_dbg {
	u8			arch;
	u8			nr_wp;
	u8			nr_bp;
	ssize_t			bw_reg[DBG_BW_REG_MAX_SIZE];
	struct cs_dbg_cpu	cpu[CORE_CNT];
};
static struct cs_dbg dbg;

static DEFINE_SPINLOCK(debug_lock);
static unsigned int cs_arm_arch;
static unsigned int cs_reg_base;
bool FLAG_T32_EN = false;

static inline void get_arm_arch_version(void)
{
	cs_arm_arch = CS_READ(dbg.cpu[0].base, MIDR);
	cs_arm_arch = (cs_arm_arch >> 4) & 0xf00;
}

static inline void dbg_os_lock(void __iomem *base)
{
	switch (cs_arm_arch) {
	case ARMV8_PROCESSOR:
		CS_WRITE(0x1, base, DBGOSLAR);
		break;
	default:
		break;
	}
	mb();
	isb();
}

static inline void dbg_os_unlock(void __iomem *base)
{
	isb();
	mb();
	switch (cs_arm_arch) {
	case ARMV8_PROCESSOR:
		CS_WRITE(0x0, base, DBGOSLAR);
		break;
	default:
		break;
	}
}

#ifdef CONFIG_EXYNOS_CORESIGHT_PC_INFO
static int exynos_cs_stat;
unsigned long exynos_cs_pc[CORE_CNT][ITERATION];

static inline bool have_pc_offset(void __iomem *base)
{
	 return !(CS_READ(base, DBGDEVID1) & 0xf);
}

unsigned long exynos_cs_get_pcval(int cpu)
{
	unsigned long valLo, valHi;
	void __iomem *base = dbg.cpu[cpu].base;

	if(!cpu_online(cpu) || !exynos_cpu.power_state(cpu))
		return 0;

	DBG_UNLOCK(base);
	dbg_os_unlock(base);
	valLo = CS_READ(base, DBGPCSRlo);
	valHi = CS_READ(base, DBGPCSRhi);
	dbg_os_lock(base);
	DBG_LOCK(base);

	return ((valHi << 32UL) | valLo);
}
EXPORT_SYMBOL(exynos_cs_get_pcval);

void exynos_cs_show_pcval(void)
{
	unsigned long flags;
	unsigned int cpu, iter, curr_cpu;
	unsigned long val = 0, valHi = 0;
	void __iomem *base;
	char buf[KSYM_SYMBOL_LEN];

	if (exynos_cs_stat < 0)
		return;

	spin_lock_irqsave(&debug_lock, flags);
	curr_cpu = raw_smp_processor_id();

	for (iter = 0; iter < ITERATION; iter++) {
		for (cpu = 0; cpu < CORE_CNT; cpu++) {
			base = dbg.cpu[cpu].base;
			exynos_cs_pc[cpu][iter] = 0;
			if (base == NULL || cpu == curr_cpu)
				continue;

			if (!exynos_cpu.power_state(cpu))
				continue;

			DBG_UNLOCK(base);
			dbg_os_unlock(base);

			val = CS_READ(base, DBGPCSRlo);
			valHi = CS_READ(base, DBGPCSRhi);

			val |= (valHi << 32L);
			if (have_pc_offset(base))
				val -= 0x8;

			dbg_os_lock(base);
			DBG_LOCK(base);

			if(MSB_MASKING == (MSB_MASKING & val)) {
				exynos_cs_pc[cpu][iter] = MSB_PADDING | val;
			}
			else exynos_cs_pc[cpu][iter] = val;
		}
	}

	spin_unlock_irqrestore(&debug_lock, flags);

	for (cpu = 0; cpu < CORE_CNT; cpu++) {
		pr_err("CPU[%d] saved pc value\n", cpu);
		for (iter = 0; iter < ITERATION; iter++) {
			if (exynos_cs_pc[cpu][iter] == 0)
				continue;

			sprint_symbol(buf, exynos_cs_pc[cpu][iter]);
			pr_err("      0x%016zx : %s\n",
				exynos_cs_pc[cpu][iter], buf);
		}
	}
}
EXPORT_SYMBOL(exynos_cs_show_pcval);

/* check sjtag status */
static int __init exynos_cs_sjtag_init(void)
{
	void __iomem *sjtag_base;
	unsigned int sjtag;

	/* Check Secure JTAG */
	sjtag_base = ioremap(cs_reg_base + CS_SJTAG_OFFSET, SZ_8);
	if (!sjtag_base) {
		pr_err("%s: cannot ioremap cs base.\n", __func__);
		exynos_cs_stat = -ENOMEM;
		goto err_func;
	}

	sjtag = __raw_readl(sjtag_base + SJTAG_STATUS);
	iounmap(sjtag_base);

	if (sjtag & SJTAG_SOFT_LOCK) {
		exynos_cs_stat = -EIO;
		goto err_func;
	}

	pr_info("exynos-coresight Secure Jtag state is soft unlock.\n");
err_func:
	return exynos_cs_stat;
}
#endif

#ifdef CONFIG_EXYNOS_CORESIGHT_MAINTAIN_DBG_REG
/* save debug resgisters when suspending */
static void debug_save_bw_reg(int cpu)
{
	int i, idx = 0;

	pr_debug("%s: cpu %d\n", __func__, cpu);

	for (i = 0; i < CORE_CNT; i++) {
		if (!dbg.cpu[i].reg[OS_LOCK_FLAG])
			return;
	}

	switch (dbg.arch) {
	case DEBUG_ARCH_V8:
		SYS_READ(DBGBVR0_EL1, dbg.bw_reg[idx++]); /* DBGBVR */
		SYS_READ(DBGBVR1_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBVR2_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBVR3_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBVR4_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBVR5_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBCR0_EL1, dbg.bw_reg[idx++]); /* DBGDCR */
		SYS_READ(DBGBCR1_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBCR2_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBCR3_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBCR4_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGBCR5_EL1, dbg.bw_reg[idx++]);

		SYS_READ(DBGWVR0_EL1, dbg.bw_reg[idx++]); /* DBGWVR */
		SYS_READ(DBGWVR1_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGWVR2_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGWVR3_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGWCR0_EL1, dbg.bw_reg[idx++]); /* DBGDCR */
		SYS_READ(DBGWCR1_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGWCR2_EL1, dbg.bw_reg[idx++]);
		SYS_READ(DBGWCR3_EL1, dbg.bw_reg[idx++]);
		break;
	default:
		break;
	}
}

static void debug_suspend_cpu(int cpu)
{
	int idx = 0;
	struct cs_dbg_cpu *cpudata = &dbg.cpu[cpu];
	void __iomem *base = cpudata->base;

	pr_debug("%s: cpu %d\n", __func__, cpu);

	if (!FLAG_T32_EN)
		return;

	DBG_UNLOCK(base);

	spin_lock(&debug_lock);
	dbg_os_lock(base);
	cpudata->reg[OS_LOCK_FLAG] = 1;
	debug_save_bw_reg(cpu);
	spin_unlock(&debug_lock);

	switch (dbg.arch) {
	case DEBUG_ARCH_V8:
		SYS_READ(MDSCR_EL1, cpudata->reg[idx++]); /* DBGDSCR */
		SYS_READ(OSECCR_EL1, cpudata->reg[idx++]); /* DBGECCR */
		SYS_READ(DBGDTRTX_EL0, cpudata->reg[idx++]); /* DBGDTRTX */
		SYS_READ(DBGDTRRX_EL0, cpudata->reg[idx++]); /* DBGDTRRX */
		SYS_READ(DBGCLAIMCLR_EL1, cpudata->reg[idx++]); /* DBGCLAIMCLR */
		break;
	default:
		break;
	}

	DBG_LOCK(base);

	pr_debug("%s: cpu %d done\n", __func__, cpu);
}

/* restore debug registers when resuming */
static void debug_restore_bw_reg(int cpu)
{
	int core = 0,  idx = 0;
	struct cs_dbg_cpu *cpudata = &dbg.cpu[cpu];
	void __iomem *a_base = NULL;

	pr_debug("%s: cpu %d\n", __func__, cpu);

	/* If debugger is not connected, do not accecs some registers. */
	if (!(cpudata->reg[0] & (1<<14))) {
		return;
	}

	for (core = 0; core < CORE_CNT; core++) {
		if (!dbg.cpu[core].reg[OS_LOCK_FLAG]) {
			a_base = dbg.cpu[core].base;
			break;
		}
	}

	switch (dbg.arch) {
	case DEBUG_ARCH_V8:
		if (core < CORE_CNT) {
			SYS_WRITE(DBGBVR0_EL1, CS_READQ(a_base, DBGBVRn(0)));
			SYS_WRITE(DBGBVR1_EL1, CS_READQ(a_base, DBGBVRn(1)));
			SYS_WRITE(DBGBVR2_EL1, CS_READQ(a_base, DBGBVRn(2)));
			SYS_WRITE(DBGBVR3_EL1, CS_READQ(a_base, DBGBVRn(3)));
			SYS_WRITE(DBGBVR4_EL1, CS_READQ(a_base, DBGBVRn(4)));
			SYS_WRITE(DBGBVR5_EL1, CS_READQ(a_base, DBGBVRn(5)));
			SYS_WRITE(DBGBCR0_EL1, CS_READ(a_base, DBGBCRn(0)));
			SYS_WRITE(DBGBCR1_EL1, CS_READ(a_base, DBGBCRn(1)));
			SYS_WRITE(DBGBCR2_EL1, CS_READ(a_base, DBGBCRn(2)));
			SYS_WRITE(DBGBCR3_EL1, CS_READ(a_base, DBGBCRn(3)));
			SYS_WRITE(DBGBCR4_EL1, CS_READ(a_base, DBGBCRn(4)));
			SYS_WRITE(DBGBCR5_EL1, CS_READ(a_base, DBGBCRn(5)));

			SYS_WRITE(DBGWVR0_EL1, CS_READQ(a_base, DBGWVRn(0)));
			SYS_WRITE(DBGWVR1_EL1, CS_READQ(a_base, DBGWVRn(1)));
			SYS_WRITE(DBGWVR2_EL1, CS_READQ(a_base, DBGWVRn(2)));
			SYS_WRITE(DBGWVR3_EL1, CS_READQ(a_base, DBGWVRn(3)));
			SYS_WRITE(DBGWCR0_EL1, CS_READ(a_base, DBGWCRn(0)));
			SYS_WRITE(DBGWCR1_EL1, CS_READ(a_base, DBGWCRn(1)));
			SYS_WRITE(DBGWCR2_EL1, CS_READ(a_base, DBGWCRn(2)));
			SYS_WRITE(DBGWCR3_EL1, CS_READ(a_base, DBGWCRn(3)));
		} else {
			SYS_WRITE(DBGBVR0_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBVR1_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBVR2_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBVR3_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBVR4_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBVR5_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBCR0_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBCR1_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBCR2_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBCR3_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBCR4_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGBCR5_EL1, dbg.bw_reg[idx++]);

			SYS_WRITE(DBGWVR0_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGWVR1_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGWVR2_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGWVR3_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGWCR0_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGWCR1_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGWCR2_EL1, dbg.bw_reg[idx++]);
			SYS_WRITE(DBGWCR3_EL1, dbg.bw_reg[idx++]);
		}
		break;
	default:
		break;
	}

	pr_debug("%s: cpu %d\n", __func__, cpu);
}

static void debug_resume_cpu(int cpu)
{
	int idx = 0;
	struct cs_dbg_cpu *cpudata = &dbg.cpu[cpu];
	void __iomem *base = cpudata->base;

	pr_debug("%s: cpu %d\n", __func__, cpu);

	if (!FLAG_T32_EN && !cpudata->reg[OS_LOCK_FLAG])
		return;

	DBG_UNLOCK(base);
	dbg_os_lock(base);

	switch (dbg.arch) {
	case DEBUG_ARCH_V8:
		SYS_WRITE(MDSCR_EL1, cpudata->reg[idx++]); /* DBGDSCR */
		SYS_WRITE(OSECCR_EL1, cpudata->reg[idx++]); /* DBGECCR */
		SYS_WRITE(DBGDTRTX_EL0, cpudata->reg[idx++]); /* DBGDTRTX */
		SYS_WRITE(DBGDTRRX_EL0, cpudata->reg[idx++]); /* DBGDTRRX */
		SYS_WRITE(DBGCLAIMSET_EL1, cpudata->reg[idx++]); /* DBGCLAIMSET */
		break;
	default:
		break;
	}
	spin_lock(&debug_lock);
	debug_restore_bw_reg(cpu);
	dbg_os_unlock(base);
	cpudata->reg[OS_LOCK_FLAG] = 0;
	spin_unlock(&debug_lock);
	DBG_LOCK(base);

	pr_debug("%s: %d done\n", __func__, cpu);
}

static inline bool dbg_arch_supported(u8 arch)
{
	switch (arch) {
	case DEBUG_ARCH_V8:
		break;
	default:
		return false;
	}
	return true;
}

static inline void get_dbg_arch_info(u32 dbgdidr)
{
	dbgdidr = CS_READ(dbg.cpu[0].base, ID_AA64DFR0_EL1);
	dbg.arch = dbgdidr & 0xf;
	dbg.nr_bp = ((dbgdidr >> 12) & 0xf) + 1;
	dbg.nr_wp = ((dbgdidr >> 20) & 0xf) + 1;
}

static int exynos_cs_pm_notifier(struct notifier_block *self,
		unsigned long cmd, void *v)
{
	int cpu = raw_smp_processor_id();

	switch (cmd) {
	case CPU_PM_ENTER:
		debug_suspend_cpu(cpu);
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		debug_resume_cpu(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER:
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		break;
	}

	return NOTIFY_OK;
}

static int __cpuinit exynos_cs_cpu_notifier(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	int cpu = (unsigned long)hcpu;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
	case CPU_DOWN_FAILED:
		debug_resume_cpu(cpu);
		break;
	case CPU_DYING:
		debug_suspend_cpu(cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata exynos_cs_pm_notifier_block = {
	.notifier_call = exynos_cs_pm_notifier,
};

static struct notifier_block __cpuinitdata exynos_cs_cpu_notifier_block = {
	.notifier_call = exynos_cs_cpu_notifier,
};

static int __init exynos_cs_debug_init(void)
{
	unsigned int dbgdidr;
	int ret = 0;

	get_dbg_arch_info(dbgdidr);

	if (!dbg_arch_supported(dbg.arch)) {
		pr_err("%s: DBG archtecture is not supported.\n", __func__);
		ret = -EPERM;
		goto err;
	}

	ret = cpu_pm_register_notifier(&exynos_cs_pm_notifier_block);
	if (ret < 0)
		goto err;

	ret = register_cpu_notifier(&exynos_cs_cpu_notifier_block);
	if (ret < 0)
		goto err;

	pr_info("exynos-coresight debug enable: arch: %x:%d bp:%d, wp:%d\n",
			cs_arm_arch, dbg.arch, dbg.nr_bp, dbg.nr_wp);
err:
	return ret;
}
#endif

static const struct of_device_id of_exynos_cs_matches[] __initconst= {
	{.compatible = "exynos,coresight"},
	{},
};

static int exynos_cs_init_dt(void)
{
	struct device_node *np = NULL;
	const unsigned int *cs_reg, *offset;
	unsigned int cs_offset;
	int len, i = 0;

	np = of_find_matching_node(NULL, of_exynos_cs_matches);

	cs_reg = of_get_property(np, "base", &len);
	if (!cs_reg)
		return -ESPIPE;

	cs_reg_base = be32_to_cpup(cs_reg);

	while ((np = of_find_node_by_type(np, "cs"))) {
		if (i >= CORE_CNT)
			break;

		offset = of_get_property(np, "dbg-offset", &len);
		if (!offset)
			return -ESPIPE;

		cs_offset = be32_to_cpup(offset);
		dbg.cpu[i].base = ioremap(cs_reg_base + cs_offset, SZ_4K);
		if (!dbg.cpu[i].base) {
			pr_err("%s: cannot ioremap cs base.\n", __func__);
			return -ENOMEM;
		}

		i++;
	}
	return 0;
}

static int __init exynos_cs_init(void)
{
	int ret = 0;

	ret = exynos_cs_init_dt();
	if (ret < 0)
		goto err;

	get_arm_arch_version();

#ifdef CONFIG_EXYNOS_CORESIGHT_PC_INFO
	ret = exynos_cs_sjtag_init();
	if (ret < 0)
		goto err;
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_MAINTAIN_DBG_REG
	ret = exynos_cs_debug_init();
	if (ret < 0)
		goto err;
#endif
err:
	return ret;
}
subsys_initcall(exynos_cs_init);

static struct bus_type ecs_subsys = {
	.name = "exynos-cs",
	.dev_name = "exynos-cs",
};

static ssize_t ecs_enable_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, 10, "%sable\n", FLAG_T32_EN ? "en" : "dis");
}

static ssize_t ecs_enable_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int en;

	if( sscanf(buf, "%1d", &en) != 1)
		return -EINVAL;

	if (en)
		FLAG_T32_EN = true;
	else
		FLAG_T32_EN = false;

	return count;
}

static struct kobj_attribute ecs_enable_attr =
        __ATTR(enabled, 0644, ecs_enable_show, ecs_enable_store);

static struct attribute *ecs_sysfs_attrs[] = {
	&ecs_enable_attr.attr,
	NULL,
};

static struct attribute_group ecs_sysfs_group = {
	.attrs = ecs_sysfs_attrs,
};

static const struct attribute_group *ecs_sysfs_groups[] = {
	&ecs_sysfs_group,
	NULL,
};

static int __init exynos_cs_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&ecs_subsys, ecs_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos-coresight subsys\n");

	return ret;
}
late_initcall(exynos_cs_sysfs_init);
