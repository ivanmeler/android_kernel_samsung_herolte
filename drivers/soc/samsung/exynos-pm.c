/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/wakeup_reason.h>
#include <linux/gpio.h>
#include <linux/syscore_ops.h>
#include <asm/psci.h>
#include <asm/suspend.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>

#ifdef CONFIG_SEC_PM_DEBUG
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#endif

#define EXYNOS8890_PA_GPIO_ALIVE	0x10580000
#define WAKEUP_STAT_EINT		(1 << 0)
#define WAKEUP_STAT_RTC_ALARM		(1 << 1)
#define WAKEUP_STAT_INT_MBOX		(1 << 24)

/*
 * PMU register offset
 */
#define EXYNOS_PMU_WAKEUP_STAT		0x0600
#define EXYNOS_PMU_EINT_WAKEUP_MASK	0x060C

static void __iomem *exynos_eint_base;
extern u32 exynos_eint_to_pin_num(int eint);
#define EXYNOS_EINT_PEND(b, x)      ((b) + 0xA00 + (((x) >> 3) * 4))

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int i, size;
	long unsigned int ext_int_pend;
	u64 eint_wakeup_mask;
	bool found = 0;
	unsigned int val;

	exynos_pmu_read(EXYNOS_PMU_EINT_WAKEUP_MASK, &val);
	eint_wakeup_mask = val;

	for (i = 0, size = 8; i < 32; i += size) {

		ext_int_pend =
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, i));

		for_each_set_bit(bit, &ext_int_pend, size) {
			u32 gpio;
			int irq;

			if (eint_wakeup_mask & (1 << (i + bit)))
				continue;

			gpio = exynos_eint_to_pin_num(i + bit);
			irq = gpio_to_irq(gpio);

			log_wakeup_reason(irq);
			update_wakeup_reason_stats(irq, i + bit);
			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

static void exynos_show_wakeup_registers(unsigned long wakeup_stat)
{
	pr_info("WAKEUP_STAT: 0x%08lx\n", wakeup_stat);
	pr_info("EINT_PEND: 0x%02x, 0x%02x 0x%02x, 0x%02x\n",
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 0)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 8)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 16)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 24)));
}

static void exynos_show_wakeup_reason(bool sleep_abort)
{
	unsigned int wakeup_stat;

	if (sleep_abort)
		pr_info("PM: early wakeup!\n");

	exynos_pmu_read(EXYNOS_PMU_WAKEUP_STAT, &wakeup_stat);

	exynos_show_wakeup_registers(wakeup_stat);

	if (wakeup_stat & WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else if (wakeup_stat & WAKEUP_STAT_INT_MBOX)
		log_mbox_wakeup();
	else if (wakeup_stat & WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat 0x%08x\n",
			wakeup_stat);
}

#ifdef CONFIG_CPU_IDLE
static DEFINE_RWLOCK(exynos_pm_notifier_lock);
static RAW_NOTIFIER_HEAD(exynos_pm_notifier_chain);

int exynos_pm_register_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_register(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_register_notifier);

int exynos_pm_unregister_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_unregister(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_unregister_notifier);

static int __exynos_pm_notify(enum exynos_pm_event event, int nr_to_call, int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&exynos_pm_notifier_chain, event, NULL,
		nr_to_call, nr_calls);

	return notifier_to_errno(ret);
}

int exynos_pm_notify(enum exynos_pm_event event)
{
	int nr_calls;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = __exynos_pm_notify(event, -1, &nr_calls);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_notify);
#endif /* CONFIG_CPU_IDLE */

int early_wakeup;
int psci_index;

static int exynos_pm_syscore_suspend(void)
{
	psci_index = PSCI_SYSTEM_SLEEP;
	exynos_prepare_sys_powerdown(SYS_SLEEP);
	pr_info("%s: Enter sleep mode\n",__func__);

	return 0;
}

static void exynos_pm_syscore_resume(void)
{
	exynos_wakeup_sys_powerdown(SYS_SLEEP, (bool)early_wakeup);

	exynos_show_wakeup_reason((bool)early_wakeup);

	pr_debug("%s: post sleep, preparing to return\n", __func__);
}

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_syscore_suspend,
	.resume		= exynos_pm_syscore_resume,
};

#ifdef CONFIG_SEC_GPIO_DVS
extern void gpio_dvs_check_sleepgpio(void);
#endif

static int exynos_pm_enter(suspend_state_t state)
{
#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************/
	/* This function must be located in appropriate SLEEP position
	 * in accordance with the specification of each BB vendor.
	 */
	/************************ Caution !!! ****************************/
	gpio_dvs_check_sleepgpio();
#endif
	/* This will also act as our return point when
	 * we resume as it saves its own register state and restores it
	 * during the resume. */
	early_wakeup = cpu_suspend(psci_index);
	if (early_wakeup)
		pr_info("%s: return to originator\n", __func__);

	return early_wakeup;
}

static const struct platform_suspend_ops exynos_pm_ops = {
	.enter		= exynos_pm_enter,
	.valid		= suspend_valid_only_mem,
};

#if defined(CONFIG_SEC_PM_DEBUG)
enum dvfs_id {
	cal_asv_dvfs_big,
	cal_asv_dvfs_little,
	cal_asv_dvfs_g3d,
	cal_asv_dvfs_mif,
	cal_asv_dvfs_int,
	cal_asv_dvfs_cam,
	cal_asv_dvfs_disp,
	cal_asv_dvs_g3dm,
	num_of_dvfs,
};

enum asv_group {
	asv_max_lv,
	dvfs_freq,
	dvfs_voltage,
	dvfs_group,
	table_group,
	num_of_asc,
};

extern int asv_get_information(enum dvfs_id id,
	enum asv_group grp, unsigned int lv);

static int asv_group_show(struct seq_file *s, void *d)
{
	int i;

	for (i = 0; i < num_of_dvfs; i++) {
	int max_lv = asv_get_information(i, asv_max_lv, 0);
	char *name ="ASVGROUP";
	int lv;

	switch (i) {
		case cal_asv_dvfs_big:
			name ="MNGS";
			break;
		case cal_asv_dvfs_little:
			name = "APOLLO";
			break;
		case cal_asv_dvfs_g3d:
			name ="G3D";
			break;
		case cal_asv_dvfs_mif:
			name = "MIF";
			break;
		case cal_asv_dvfs_int:
			name = "INT";
			break;
		case cal_asv_dvfs_cam:
			name = "CAM";
			break;
		case cal_asv_dvfs_disp:
			name ="ISP";
			break;
		default:
			break;
	}
	seq_printf(s, "%s ASV group is %d max_lv is %d\n",
		name, asv_get_information(i, dvfs_group, 0), max_lv);

		for (lv = 0; lv < max_lv ; lv++) {
			seq_printf(s, "%s LV%d freq : %d volt : %d, "
			"group: %d\n",
			name, lv, asv_get_information(i, dvfs_freq, lv),
			asv_get_information(i, dvfs_voltage, lv),
			asv_get_information(i, dvfs_group, lv));
		}
	}

	return 0;
}

static int asv_group_open(struct inode *inode, struct file *file)
{
	return single_open(file, asv_group_show, inode->i_private);
}

const static struct file_operations asv_group_fops = {
	.open		= asv_group_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int asv_summary_show(struct seq_file *s, void *d)
{
	seq_printf(s, "big:%u, LITTLE:%u, INT:%u, MIF:%u, G3D:%u, ISP:%u\n",
		asv_get_information(cal_asv_dvfs_big, dvfs_group, 0),
		asv_get_information(cal_asv_dvfs_little, dvfs_group, 0),
		asv_get_information(cal_asv_dvfs_int, dvfs_group, 0),
		asv_get_information(cal_asv_dvfs_mif, dvfs_group, 0),
		asv_get_information(cal_asv_dvfs_g3d, dvfs_group, 0),
		asv_get_information(cal_asv_dvfs_disp, dvfs_group,0));

	return 0;
}

static int asv_summary_open(struct inode *inode, struct file *file)
{
	return single_open(file, asv_summary_show, inode->i_private);
}

const static struct file_operations asv_summary_fops = {
	.open		= asv_summary_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#endif /* CONFIG_SEC_PM_DEBUG */

#if defined(CONFIG_SEC_FACTORY)
enum ids_info {
	tg,
	lg,
	bg,
	g3dg,
	mifg,
	bids,
	gids,
};

extern int asv_ids_information(enum ids_info id);

static ssize_t show_asv_info(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int count = 0;

	/* Set asv group info to buf */
	count += sprintf(&buf[count], "%d ", asv_ids_information(tg));
	count += sprintf(&buf[count], "%03x ", asv_ids_information(bg));
	count += sprintf(&buf[count], "%03x ", asv_ids_information(g3dg));
	count += sprintf(&buf[count], "%u ", asv_ids_information(bids));
	count += sprintf(&buf[count], "%u ", asv_ids_information(gids));
	count += sprintf(&buf[count], "\n");

	return count;
}

static DEVICE_ATTR(asv_info, 0664, show_asv_info, NULL);

#endif /* CONFIG_SEC_FACTORY */


static __init int exynos_pm_drvinit(void)
{
#if defined(CONFIG_SEC_FACTORY)
	int ret;
#endif

	suspend_set_ops(&exynos_pm_ops);
	register_syscore_ops(&exynos_pm_syscore_ops);

	exynos_eint_base = ioremap(EXYNOS8890_PA_GPIO_ALIVE, SZ_8K);

	if (exynos_eint_base == NULL) {
		pr_err("%s: unable to ioremap for EINT base address\n",
				__func__);
		BUG();
	}

#if defined(CONFIG_SEC_PM_DEBUG)
	debugfs_create_file("asv_group", S_IRUSR, NULL, NULL, &asv_group_fops);
	debugfs_create_file("asv_summary", S_IRUSR, NULL, NULL, &asv_summary_fops);
#endif

#if defined(CONFIG_SEC_FACTORY)
	/* create sysfs group */
	ret = sysfs_create_file(power_kobj, &dev_attr_asv_info.attr);
	if (ret) {
		pr_err("%s: failed to create exynos8890 asv attribute file\n",
				__func__);
	}

#endif

	return 0;
}
arch_initcall(exynos_pm_drvinit);
