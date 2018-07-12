/* sound/soc/samsung/lpass.c
 *
 * Low Power Audio SubSystem driver for Samsung Exynos
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/fb.h>
#include <linux/iommu.h>
#include <linux/exynos_iovmm.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <asm/tlbflush.h>

#include <sound/exynos.h>

#include <soc/samsung/exynos-pm.h>

#if 0
#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/cpufreq.h>
#endif

#include "lpass.h"

#ifdef USE_EXYNOS_AUD_SCHED
#define AUD_TASK_CPU_UHQ	(5)
#ifdef CONFIG_SOC_EXYNOS5422
#define USE_AUD_TASK_RT
#endif
#endif

#ifdef CONFIG_PM_DEVFREQ
#define USE_AUD_DEVFREQ
#ifdef CONFIG_SOC_EXYNOS5422
#define AUD_CPU_FREQ_UHQA	(1000000)
#define AUD_KFC_FREQ_UHQA	(1300000)
#define AUD_MIF_FREQ_UHQA	(413000)
#define AUD_INT_FREQ_UHQA	(0)
#define AUD_CPU_FREQ_HIGH	(0)
#define AUD_KFC_FREQ_HIGH	(1300000)
#define AUD_MIF_FREQ_HIGH	(0)
#define AUD_INT_FREQ_HIGH	(0)
#define AUD_CPU_FREQ_NORM	(0)
#define AUD_KFC_FREQ_NORM	(0)
#define AUD_MIF_FREQ_NORM	(0)
#define AUD_INT_FREQ_NORM	(0)
#elif defined(CONFIG_SOC_EXYNOS5430)
#define AUD_CPU_FREQ_UHQA	(1000000)
#define AUD_KFC_FREQ_UHQA	(1300000)
#define AUD_MIF_FREQ_UHQA	(413000)
#define AUD_INT_FREQ_UHQA	(0)
#define AUD_CPU_FREQ_HIGH	(0)
#define AUD_KFC_FREQ_HIGH	(600000)
#define AUD_MIF_FREQ_HIGH	(0)
#define AUD_INT_FREQ_HIGH	(0)
#define AUD_CPU_FREQ_NORM	(0)
#define AUD_KFC_FREQ_NORM	(0)
#define AUD_MIF_FREQ_NORM	(0)
#define AUD_INT_FREQ_NORM	(0)
#elif defined(CONFIG_SOC_EXYNOS5433)
#define AUD_CPU_FREQ_UHQA	(1000000)
#define AUD_KFC_FREQ_UHQA	(1300000)
#define AUD_MIF_FREQ_UHQA	(413000)
#define AUD_INT_FREQ_UHQA	(0)
#define AUD_CPU_FREQ_HIGH	(0)
#define AUD_KFC_FREQ_HIGH	(500000)
#define AUD_MIF_FREQ_HIGH	(0)
#define AUD_INT_FREQ_HIGH	(0)
#define AUD_CPU_FREQ_NORM	(0)
#define AUD_KFC_FREQ_NORM	(0)
#define AUD_MIF_FREQ_NORM	(0)
#define AUD_INT_FREQ_NORM	(0)
#elif defined(CONFIG_SOC_EXYNOS7420)
#define AUD_CPU_FREQ_UHQA	(1000000)
#define AUD_KFC_FREQ_UHQA	(1300000)
#define AUD_MIF_FREQ_UHQA	(413000)
#define AUD_INT_FREQ_UHQA	(0)
#define AUD_CPU_FREQ_HIGH	(0)
#define AUD_KFC_FREQ_HIGH	(0)
#define AUD_MIF_FREQ_HIGH	(416000)
#define AUD_INT_FREQ_HIGH	(0)
#define AUD_CPU_FREQ_NORM	(0)
#define AUD_KFC_FREQ_NORM	(0)
#define AUD_MIF_FREQ_NORM	(0)
#define AUD_INT_FREQ_NORM	(0)
#elif defined(CONFIG_SOC_EXYNOS8890)
#define AUD_CPU_FREQ_UHQA	(0)
#define AUD_KFC_FREQ_UHQA	(1040000)
#define AUD_MIF_FREQ_UHQA	(413000)
#define AUD_INT_FREQ_UHQA	(0)
#define AUD_CPU_FREQ_HIGH	(0)
#define AUD_KFC_FREQ_HIGH	(0)
#define AUD_MIF_FREQ_HIGH	(500000)
#define AUD_INT_FREQ_HIGH	(0)
#define AUD_CPU_FREQ_NORM	(0)
#define AUD_KFC_FREQ_NORM	(0)
#define AUD_MIF_FREQ_NORM	(0)
#define AUD_INT_FREQ_NORM	(0)
#else
#define AUD_CPU_FREQ_UHQA	(1000000)
#define AUD_KFC_FREQ_UHQA	(1300000)
#define AUD_MIF_FREQ_UHQA	(413000)
#define AUD_INT_FREQ_UHQA	(0)
#define AUD_CPU_FREQ_HIGH	(0)
#define AUD_KFC_FREQ_HIGH	(500000)
#define AUD_MIF_FREQ_HIGH	(0)
#define AUD_INT_FREQ_HIGH	(0)
#define AUD_CPU_FREQ_NORM	(0)
#define AUD_KFC_FREQ_NORM	(0)
#define AUD_MIF_FREQ_NORM	(0)
#define AUD_INT_FREQ_NORM	(0)
#endif
#endif

/* Default interrupt mask */
#define INTR_CA5_MASK_VAL	(LPASS_INTR_SFR)
#define INTR_CPU_MASK_VAL	(LPASS_INTR_I2S | \
				 LPASS_INTR_PCM | LPASS_INTR_SB | \
				 LPASS_INTR_UART | LPASS_INTR_SFR)
#define INTR_CPU_DMA_VAL	(LPASS_INTR_DMA)

#define EXYNOS_PMU_PMU_DEBUG_OFFSET		0x0A00
#define EXYNOS_GPIO_MODE_AUD_SYS_PWR_REG_OFFSET	0x1340
#define EXYNOS_PAD_RETENTION_AUD_OPTION_OFFSET 	0x3028

#ifdef CONFIG_SOC_EXYNOS8890
#define SRAM_BASE 0x3000000
#define SRAM_SIZE 0x24000
#endif

/* Audio subsystem version */
enum {
	LPASS_VER_000100 = 0,		/* pega/carmen */
	LPASS_VER_010100,		/* hudson */
	LPASS_VER_100100 = 16,		/* gaia/adonis */
	LPASS_VER_110100,		/* ares */
	LPASS_VER_270120,		/* insel-d */
	LPASS_VER_370100 = 32,		/* rhea/helsinki */
	LPASS_VER_370200,		/* helsinki prime */
	LPASS_VER_370210,		/* istor */
	LPASS_VER_MAX
};

static struct lpass_info lpass;

struct aud_reg {
	void __iomem		*reg;
	u32			val;
	struct list_head	node;
};

struct subip_info {
	struct device		*dev;
	const char		*name;
	void			(*cb)(struct device *dev);
	atomic_t		use_cnt;
	struct list_head	node;
};

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
DEFINE_MUTEX(lpass_mutex);

unsigned int cpu_lock[14] =
		{0, 200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000, 1000000, 1104000, 1200000, 1296000, 1400000};

void lpass_set_cpu_lock(int level)
{
	mutex_lock(&lpass_mutex);
#ifdef USE_AUD_DEVFREQ
	pm_qos_update_request(&lpass.aud_cluster0_qos, cpu_lock[level]);
#endif
	mutex_unlock(&lpass_mutex);
}
#endif

static LIST_HEAD(reg_list);
static LIST_HEAD(subip_list);

extern int check_adma_status(void);
extern int check_fdma_status(void);
extern int check_esa_status(void);
extern int check_eax_dma_status(void);
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
extern void esa_compr_alpa_notifier(bool on);
extern int check_esa_compr_state(void);
#endif

static void lpass_update_qos(void);

static bool cp_available;
static atomic_t dram_usage_cnt;

void lpass_disable_mif_status(bool on)
{
	u32 val = on ? 1 << 2 : 0x0;
	writel(val, lpass.regs + LPASS_MIF_POWER);
}

void lpass_mif_power_on(void)
{
	unsigned int timeout = 3000;

	writel(0x2, lpass.regs + LPASS_MIF_POWER);
	do {
		mdelay(1);
		timeout--;

		if (readl(lpass.regs + LPASS_MIF_POWER) & 0x1)
			break;

	} while (timeout);

	if (!timeout) {
		pr_err("%s : LPASS driver failed to enable MIF\n",
			__func__);
	}
}

void lpass_inc_dram_usage_count(void)
{
	atomic_inc(&dram_usage_cnt);
}

void lpass_dec_dram_usage_count(void)
{
	atomic_dec(&dram_usage_cnt);
}

int lpass_get_dram_usage_count(void)
{
	return atomic_read(&dram_usage_cnt);
}

bool lpass_i2s_master_mode(void)
{
	return lpass.i2s_master_mode;
}

void update_cp_available(bool cpen)
{
	cp_available = cpen;
	pr_info("%s: cp_available = %d\n", __func__, cp_available);
}

bool is_cp_aud_enabled(void)
{
	return cp_available;
}
EXPORT_SYMBOL(is_cp_aud_enabled);

/**
 * LPASS version for Exynos7580 is 0x270120. The operation of this LPASS is
 * closer to the LPASS versions 0x370100 and above, hence it is grouped with
 * them.
 */
static inline bool is_old_ass(void)
{
	return lpass.ver < LPASS_VER_270120 ? true : false;
}

/**
 * LPASS version for Exynos7580 is 0x270120. This LPASS version doesn't support
 * CA5, timer and interrupt clocks. Hence keeping this out of this class in
 * these cases and the operations are protected by this check.
 */
static inline bool is_new_ass(void)
{
	return lpass.ver >= LPASS_VER_370100 ? true : false;
}

static inline bool is_running_only(const char *name)
{
	struct subip_info *si;

	if (atomic_read(&lpass.use_cnt) != 1)
		return false;

	list_for_each_entry(si, &subip_list, node) {
		if (atomic_read(&si->use_cnt) > 0 &&
			!strncmp(name, si->name, strlen(si->name)))
			return true;
	}

	return false;
}

int exynos_check_aud_pwr(void)
{
	int dram_used = check_adma_status();

#ifdef CONFIG_SND_SAMSUNG_FAKEDMA
	dram_used |= check_fdma_status();
#endif
#if defined(CONFIG_SND_SAMSUNG_SEIREN) || defined(CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD)
	dram_used |= check_esa_status();
#endif
	dram_used |= check_eax_dma_status();

	if (!lpass.enabled)
		return AUD_PWR_SLEEP;
	else if (is_running_only("aud-uart"))
		return AUD_PWR_LPC;
	else if (!dram_used)
		return AUD_PWR_LPA;

	if (is_new_ass())
		return AUD_PWR_ALPA;
	else
		return AUD_PWR_AFTR;
}

void lpass_update_lpclock(u32 ctrlid, bool idle)
{
	lpass_update_lpclock_impl(&lpass.pdev->dev, ctrlid, idle);
}

void __iomem *lpass_get_regs(void)
{
	return lpass.regs;
}

void __iomem *lpass_get_regs_s(void)
{
	return lpass.regs_s;
}

void __iomem *lpass_get_mem(void)
{
	return lpass.mem;
}

struct iommu_domain *lpass_get_iommu_domain(void)
{
	return lpass.domain;
}

void lpass_set_dma_intr(bool on)
{
	u32 cfg = readl(lpass.regs + LPASS_INTR_CPU_MASK);
	if (on)
		cfg |= INTR_CPU_DMA_VAL;
	else
		cfg &= ~(INTR_CPU_DMA_VAL);

	writel(cfg, lpass.regs + LPASS_INTR_CPU_MASK);
}

void lpass_dma_enable(bool on)
{
	unsigned long flags;

	spin_lock_irqsave(&lpass.lock, flags);
	if (on) {
		atomic_inc(&lpass.dma_use_cnt);
		if (atomic_read(&lpass.dma_use_cnt) == 1)
			lpass_set_dma_intr(true);
	} else {
		atomic_dec(&lpass.dma_use_cnt);
		if (atomic_read(&lpass.dma_use_cnt) == 0)
			lpass_set_dma_intr(false);
	}
	spin_unlock_irqrestore(&lpass.lock, flags);
}

void ass_reset(int ip, int op)
{
	unsigned long flags;

	spin_lock_irqsave(&lpass.lock, flags);

	spin_unlock_irqrestore(&lpass.lock, flags);
}

void lpass_reset(int ip, int op)
{
	u32 reg, val;
	u32 bit = 0;
	void __iomem *regs;
	unsigned long flags;

	if (is_old_ass()) {
		ass_reset(ip, op);
		return;
	}

	spin_lock_irqsave(&lpass.lock, flags);
	regs = lpass.regs;
	reg = LPASS_CORE_SW_RESET;
	switch (ip) {
	case LPASS_IP_DMA:
		bit = LPASS_SW_RESET_DMA;
		break;
	case LPASS_IP_MEM:
		bit = LPASS_SW_RESET_MEM;
		break;
	case LPASS_IP_TIMER:
		if (is_new_ass())
			bit = LPASS_SW_RESET_TIMER;
		break;
	case LPASS_IP_I2S:
		bit = LPASS_SW_RESET_I2S;
		break;
	case LPASS_IP_PCM:
		bit = LPASS_SW_RESET_PCM;
		break;
	case LPASS_IP_UART:
		bit = LPASS_SW_RESET_UART;
		break;
	case LPASS_IP_SLIMBUS:
		if (is_new_ass())
			bit = LPASS_SW_RESET_SB;
		break;
	case LPASS_IP_CA5:
		if (is_new_ass()) {
			regs = (lpass.ver >= LPASS_VER_370200) ?
							lpass.regs_s : regs;
			reg = LPASS_CA5_SW_RESET;
			bit = LPASS_SW_RESET_CA5;
		}
		break;
	default:
		spin_unlock_irqrestore(&lpass.lock, flags);
		pr_err("%s: wrong ip type %d!\n", __func__, ip);
		return;
	}

	val = readl(regs + reg);
	switch (op) {
	case LPASS_OP_RESET:
		val &= ~bit;
		break;
	case LPASS_OP_NORMAL:
		val |= bit;
		break;
	default:
		spin_unlock_irqrestore(&lpass.lock, flags);
		pr_err("%s: wrong op type %d!\n", __func__, op);
		return;
	}

	writel(val, regs + reg);
	spin_unlock_irqrestore(&lpass.lock, flags);
}

void lpass_reset_toggle(int ip)
{
	pr_debug("%s: %d\n", __func__, ip);

	lpass_reset(ip, LPASS_OP_RESET);
	udelay(100);
	lpass_reset(ip, LPASS_OP_NORMAL);
}

int lpass_register_subip(struct device *ip_dev, const char *ip_name)
{
	struct device *dev = &lpass.pdev->dev;
	struct subip_info *si;

	si = devm_kzalloc(dev, sizeof(struct subip_info), GFP_KERNEL);
	if (!si)
		return -1;

	si->dev = ip_dev;
	si->name = ip_name;
	si->cb = NULL;
	atomic_set(&si->use_cnt, 0);
	list_add(&si->node, &subip_list);

	pr_info("%s: %s(%p) registered\n", __func__, ip_name, ip_dev);

	return 0;
}

int lpass_set_gpio_cb(struct device *ip_dev, void (*ip_cb)(struct device *dev))
{
	struct subip_info *si;

	list_for_each_entry(si, &subip_list, node) {
		if (si->dev == ip_dev) {
			si->cb = ip_cb;
			pr_info("%s: %s(cb: %p)\n", __func__,
				si->name, si->cb);
			return 0;
		}
	}

	return -EINVAL;
}

void lpass_get_sync(struct device *ip_dev)
{
	struct subip_info *si;

	list_for_each_entry(si, &subip_list, node) {
		if (si->dev == ip_dev) {
			atomic_inc(&si->use_cnt);
			atomic_inc(&lpass.use_cnt);
			pr_info("%s: %s (use:%d)\n", __func__,
				si->name, atomic_read(&si->use_cnt));
			pm_runtime_get_sync(&lpass.pdev->dev);
		}
	}

	lpass_update_qos();
}

void lpass_put_sync(struct device *ip_dev)
{
	struct subip_info *si;

	list_for_each_entry(si, &subip_list, node) {
		if (si->dev == ip_dev) {
			atomic_dec(&si->use_cnt);
			atomic_dec(&lpass.use_cnt);
			pr_info("%s: %s (use:%d)\n", __func__,
				si->name, atomic_read(&si->use_cnt));
			pm_runtime_put_sync(&lpass.pdev->dev);
		}
	}

	lpass_update_qos();
}

#ifdef USE_EXYNOS_AUD_SCHED
void lpass_set_sched(pid_t pid, int mode)
{
#ifdef USE_AUD_TASK_RT
	struct sched_param param_fifo = {.sched_priority = MAX_RT_PRIO >> 1};
	struct task_struct *task = find_task_by_vpid(pid);
#endif
	switch (mode) {
	case AUD_MODE_UHQA:
		lpass.uhqa_on = true;
		break;
	case AUD_MODE_NORM:
		lpass.uhqa_on = false;
		break;
	default:
		break;
	}

	lpass_update_qos();

#ifdef USE_AUD_TASK_RT
	if (task) {
		sched_setscheduler_nocheck(task,
				SCHED_RR | SCHED_RESET_ON_FORK, &param_fifo);
		pr_info("%s: [%s] pid = %d, prio = %d\n",
				__func__, task->comm, pid, task->prio);
	} else {
		pr_err("%s: task not found (pid = %d)\n",
				__func__, pid);
	}
#endif
}
#endif

#ifdef USE_EXYNOS_AUD_CPU_HOTPLUG
void lpass_get_cpu_hotplug(void)
{
	pr_debug("%s ++\n", __func__);
	cluster0_core1_hotplug_in(true);
}

void lpass_put_cpu_hotplug(void)
{
	pr_debug("%s --\n", __func__);
	cluster0_core1_hotplug_in(false);
}
#endif

void lpass_add_stream(void)
{
	atomic_inc(&lpass.stream_cnt);
	lpass_update_qos();
}

void lpass_remove_stream(void)
{
	atomic_dec(&lpass.stream_cnt);
	lpass_update_qos();
}

static void lpass_reg_save(void)
{
	struct aud_reg *ar;

	pr_debug("Registers of LPASS are saved\n");

	list_for_each_entry(ar, &reg_list, node)
		ar->val = readl(ar->reg);

	return;
}

static void lpass_reg_restore(void)
{
	struct aud_reg *ar;

	pr_debug("Registers of LPASS are restore\n");

	list_for_each_entry(ar, &reg_list, node)
		writel(ar->val, ar->reg);

	return;
}


void lpass_retention_pad_reg(void)
{
	regmap_update_bits(lpass.pmureg,
			EXYNOS_GPIO_MODE_AUD_SYS_PWR_REG_OFFSET,
			0x1, 1);

//	writel(1, EXYNOS_PMUREG(0x1340));		/* GPIO_MODE_AUD_SYS_PWR_REG */
}

void lpass_release_pad_reg(void)
{
	regmap_update_bits(lpass.pmureg,
			EXYNOS_PAD_RETENTION_AUD_OPTION_OFFSET,
			0x10000000, 1);

	regmap_update_bits(lpass.pmureg,
			EXYNOS_GPIO_MODE_AUD_SYS_PWR_REG_OFFSET,
			0x1, 1);

//	writel(1 << 28, EXYNOS_PMUREG(0x3028));		/* PAD_RETENTION_AUD_OPTION */
//	writel(1, EXYNOS_PMUREG(0x1340));		/* GPIO_MODE_AUD_SYS_PWR_REG */
}

static void lpass_retention_pad(void)
{
	struct subip_info *si;

	/* Powerdown mode for gpio */
	list_for_each_entry(si, &subip_list, node) {
		if (si->cb != NULL)
			(*si->cb)(si->dev);
	}

	/* Set PAD retention */
	lpass_retention_pad_reg();
}

static void lpass_release_pad(void)
{
	struct subip_info *si;

	/* Restore gpio */
	list_for_each_entry(si, &subip_list, node) {
		if (si->cb != NULL)
			(*si->cb)(si->dev);
	}

	/* Release PAD retention */
	lpass_release_pad_reg();
}

#ifdef CONFIG_SOC_EXYNOS8890
static int __attribute__((unused)) lpass_sysmmu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token)
{
	if (lpass.mem && lpass.sram_fw_back) {
		memcpy(lpass.sram_fw_back, lpass.mem, SRAM_SIZE);
	} else {
		pr_err("LPASS driver failed to save sram region \n");
	}

	return 0;
}
#endif

static void ass_enable(void)
{
	int ret = 0;

#ifdef CONFIG_SOC_EXYNOS8890
	lpass.mem = ioremap_wc(SRAM_BASE, SRAM_SIZE);
	if (!lpass.mem) {
		pr_err("LPASS driver failed to ioremap sram \n");
		return;
	}
#endif
	/* Enable PLL */
	lpass_enable_pll(true);

	lpass_reg_restore();

	/* PLL path */
	lpass_set_mux_pll();

	clk_prepare_enable(lpass.clk_dmac);
	clk_prepare_enable(lpass.clk_timer);

	ret = iommu_attach_device(lpass.domain, &lpass.pdev->dev);
	if (ret) {
		dev_err(&lpass.pdev->dev,
			"Unable to attach iommu device: %d\n", ret);
	} else {
		lpass.enabled = true;
	}
}

static void lpass_enable(void)
{
	int ret = 0;

	if (!lpass.valid) {
		pr_debug("%s: LPASS is not available", __func__);
		return;
	}

	if (is_old_ass()) {
		ass_enable();
		return;
	}

	/* Enable PLL */
	lpass_enable_pll(true);

	lpass_reg_restore();

	/* PLL path */
	lpass_set_mux_pll();

	if (lpass.clk_dmac)
		clk_prepare_enable(lpass.clk_dmac);
	if (lpass.clk_sramc)
		clk_prepare_enable(lpass.clk_sramc);

	lpass_reset_toggle(LPASS_IP_MEM);
	lpass_reset_toggle(LPASS_IP_I2S);
	lpass_reset_toggle(LPASS_IP_DMA);

#ifdef CONFIG_SOC_EXYNOS8890
	if (!lpass.mem) {
		lpass.mem = ioremap_wc(SRAM_BASE, SRAM_SIZE);
		if (!lpass.mem) {
			lpass_enable_pll(false);
			pr_err("LPASS driver failed to ioremap sram \n");
			return;
		}
	}
#endif

	if (lpass.clk_dmac)
		clk_disable_unprepare(lpass.clk_dmac);

	/* PAD */
	lpass_release_pad();

	/* Clear memory */
	memset(lpass.mem, 0, lpass.mem_size);

	ret = iommu_attach_device(lpass.domain, &lpass.pdev->dev);
	if (ret) {
		dev_err(&lpass.pdev->dev,
			"Unable to attach iommu device: %d\n", ret);
	} else {
		lpass.enabled = true;
	}
}

static void ass_disable(void)
{
	lpass.enabled = false;

	if (lpass.clk_dmac)
		clk_disable_unprepare(lpass.clk_dmac);
	if (lpass.clk_timer)
		clk_disable_unprepare(lpass.clk_timer);

	lpass_reg_save();

	iommu_detach_device(lpass.domain, &lpass.pdev->dev);

	/* OSC path */
	lpass_set_mux_osc();

	/* Disable PLL */
	lpass_enable_pll(false);

#ifdef CONFIG_SOC_EXYNOS8890
	iounmap(lpass.mem);
	lpass.mem = NULL;
#endif

}

static void lpass_disable(void)
{
#ifdef CONFIG_SOC_EXYNOS8890
	unsigned long start;
	unsigned long end;
#endif

	if (!lpass.valid) {
		pr_debug("%s: LPASS is not available", __func__);
		return;
	}

	if (is_old_ass()) {
		ass_disable();
		return;
	}

	lpass.enabled = false;

	/* PAD */
	lpass_retention_pad();

	if (lpass.clk_sramc)
		clk_disable_unprepare(lpass.clk_sramc);

	lpass_reg_save();

	iommu_detach_device(lpass.domain, &lpass.pdev->dev);

	/* OSC path */
	lpass_set_mux_osc();

	/* Enable clocks */
	lpass_reset_clk_default();

	/* Disable PLL */
	lpass_enable_pll(false);

#ifdef CONFIG_SOC_EXYNOS8890
	start = (unsigned long)lpass.mem;
	end = (unsigned long)lpass.mem + lpass.mem_size;
	iounmap(lpass.mem);
	flush_tlb_kernel_range(start, end);
	lpass.mem = NULL;
#endif
}

#if 0
static int lpass_get_clk_ass(struct device *dev)
{
	lpass.clk_dmac = clk_get(dev, "dmac");
	if (IS_ERR(lpass.clk_dmac)) {
		dev_err(dev, "dmac clk not found\n");
		goto err0;
	}

	lpass.clk_timer = clk_get(dev, "timer");
	if (IS_ERR(lpass.clk_timer)) {
		dev_err(dev, "timer clk not found\n");
		goto err1;
	}

	return 0;
err1:
	clk_put(lpass.clk_dmac);
err0:
	return -1;
}

static int lpass_get_clk(struct device *dev)
{
	if (is_old_ass())
		return lpass_get_clk_ass(dev);

	lpass.clk_dmac = clk_get(dev, "dmac");
	if (IS_ERR(lpass.clk_dmac)) {
		dev_err(dev, "dmac clk not found\n");
		goto err0;
	}

	lpass.clk_sramc = clk_get(dev, "sramc");
	if (IS_ERR(lpass.clk_sramc)) {
		dev_err(dev, "sramc clk not found\n");
		goto err1;
	}

	/**
	 * Exynos7580 (LPASS_VER_270120) doesn't have timer and intr clocks,
	 * hence we need a special check for it.
	 */
	if (is_new_ass()) {
		lpass.clk_intr = clk_get(dev, "intr");
		if (IS_ERR(lpass.clk_intr)) {
			dev_err(dev, "intr clk not found\n");
			goto err2;
		}

		lpass.clk_timer = clk_get(dev, "timer");
		if (IS_ERR(lpass.clk_timer)) {
			dev_err(dev, "timer clk not found\n");
			goto err3;
		}
	}

	return 0;
err3:
	clk_put(lpass.clk_intr);
err2:
	clk_put(lpass.clk_sramc);
err1:
	clk_put(lpass.clk_dmac);
err0:
	return -1;
}

static void clk_put_all_ass(void)
{
	clk_put(lpass.clk_dmac);
	clk_put(lpass.clk_timer);
}

static void clk_put_all(void)
{
	if (is_old_ass()) {
		clk_put_all_ass();
		return;
	}

	clk_put(lpass.clk_dmac);
	clk_put(lpass.clk_sramc);

	/* Exynos7580 (LPASS_VER_270120) doesn't have clk_intr and clk_timer */
	if (is_new_ass()) {
		clk_put(lpass.clk_intr);
		clk_put(lpass.clk_timer);
	}
}
#endif

static void lpass_add_suspend_reg(void __iomem *reg)
{
	struct device *dev = &lpass.pdev->dev;
	struct aud_reg *ar;

	ar = devm_kzalloc(dev, sizeof(struct aud_reg), GFP_KERNEL);
	if (!ar)
		return;

	ar->reg = reg;
	list_add(&ar->node, &reg_list);
}

static void lpass_init_reg_list(void)
{
	int n = 0;

	do {
		if (lpass_cmu_save[n] == NULL)
			break;

		lpass_add_suspend_reg(lpass_cmu_save[n]);
	} while (++n);

	if (is_new_ass())
		lpass_add_suspend_reg(lpass.regs + LPASS_INTR_CA5_MASK);

	lpass_add_suspend_reg(lpass.regs + LPASS_INTR_CPU_MASK);

	if (lpass.sysmmu)
		lpass_add_suspend_reg(lpass.sysmmu);
}

static int lpass_proc_show(struct seq_file *m, void *v) {
	struct subip_info *si;
	int pmode = exynos_check_aud_pwr();

	seq_printf(m, "power: %s\n", lpass.enabled ? "on" : "off");
	seq_printf(m, "canbe: %s\n",
			(pmode == AUD_PWR_SLEEP) ? "sleep" :
			(pmode == AUD_PWR_LPA) ? "lpa" :
			(pmode == AUD_PWR_ALPA) ? "alpa" :
			(pmode == AUD_PWR_AFTR) ? "aftr" : "unknown");

	list_for_each_entry(si, &subip_list, node) {
		seq_printf(m, "subip: %s (%d)\n",
				si->name, atomic_read(&si->use_cnt));
	}

	seq_printf(m, "strm: %d\n", atomic_read(&lpass.stream_cnt));
	seq_printf(m, "uhqa: %s\n", lpass.uhqa_on ? "on" : "off");
#ifdef USE_AUD_DEVFREQ
	seq_printf(m, "cpu: %d, kfc: %d\n",
			lpass.cpu_qos / 1000, lpass.kfc_qos / 1000);
	seq_printf(m, "mif: %d, int: %d\n",
			lpass.mif_qos / 1000, lpass.int_qos / 1000);
#endif
	return 0;
}

static int lpass_proc_open(struct inode *inode, struct  file *file) {
	return single_open(file, lpass_proc_show, NULL);
}

static const struct file_operations lpass_proc_fops = {
	.owner = THIS_MODULE,
	.open = lpass_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_PM_SLEEP
static int lpass_suspend(struct device *dev)
{
	pr_debug("%s entered\n", __func__);

#ifdef CONFIG_PM_RUNTIME
	if (atomic_read(&lpass.use_cnt) > 0)
		lpass_disable();
#else
	lpass_disable();
#endif
	return 0;
}

static int lpass_resume(struct device *dev)
{
	pr_debug("%s entered\n", __func__);

#ifdef CONFIG_PM_RUNTIME
	if (atomic_read(&lpass.use_cnt) > 0)
		lpass_enable();
#else
	lpass_enable();
#endif
	return 0;
}
#else
#define lpass_suspend NULL
#define lpass_resume  NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id exynos_lpass_match[];

static int lpass_get_ver(struct device_node *np)
{
	const struct of_device_id *match;
	int ver;

	if (np) {
		match = of_match_node(exynos_lpass_match, np);
		ver = *(int *)(match->data);
	} else {
		ver = LPASS_VER_000100;
	}

	return ver;
}
#else
static int lpass_get_ver(struct device_node *np)
{
	return LPASS_VER_000100;
}
#endif

static void lpass_update_qos(void)
{
#ifdef USE_AUD_DEVFREQ
	int cpu_qos_new, kfc_qos_new, mif_qos_new, int_qos_new;

	if (!lpass.enabled) {
		cpu_qos_new = 0;
		kfc_qos_new = 0;
		mif_qos_new = 0;
		int_qos_new = 0;
	} else if (lpass.uhqa_on) {
		cpu_qos_new = AUD_CPU_FREQ_UHQA;
		kfc_qos_new = AUD_KFC_FREQ_UHQA;
		mif_qos_new = AUD_MIF_FREQ_UHQA;
		int_qos_new = AUD_INT_FREQ_UHQA;
	} else if (atomic_read(&lpass.stream_cnt) > 1) {
		cpu_qos_new = AUD_CPU_FREQ_HIGH;
		kfc_qos_new = AUD_KFC_FREQ_HIGH;
		mif_qos_new = AUD_MIF_FREQ_HIGH;
		int_qos_new = AUD_INT_FREQ_HIGH;
	} else {
		cpu_qos_new = AUD_CPU_FREQ_NORM;
		kfc_qos_new = AUD_KFC_FREQ_NORM;
		mif_qos_new = AUD_MIF_FREQ_NORM;
		int_qos_new = AUD_INT_FREQ_NORM;
	}

	if (lpass.cpu_qos != cpu_qos_new) {
		lpass.cpu_qos = cpu_qos_new;
		pm_qos_update_request(&lpass.aud_cluster1_qos, lpass.cpu_qos);
		pr_debug("%s: cpu_qos = %d\n", __func__, lpass.cpu_qos);
	}

	if (lpass.kfc_qos != kfc_qos_new) {
		lpass.kfc_qos = kfc_qos_new;
		pm_qos_update_request(&lpass.aud_cluster0_qos, lpass.kfc_qos);
		pr_debug("%s: kfc_qos = %d\n", __func__, lpass.kfc_qos);
	}

	if (lpass.mif_qos != mif_qos_new) {
		lpass.mif_qos = mif_qos_new;
		pm_qos_update_request(&lpass.aud_mif_qos, lpass.mif_qos);
		pr_debug("%s: mif_qos = %d\n", __func__, lpass.mif_qos);
	}

	if (lpass.int_qos != int_qos_new) {
		lpass.int_qos = int_qos_new;
		pm_qos_update_request(&lpass.aud_int_qos, lpass.int_qos);
		pr_debug("%s: int_qos = %d\n", __func__, lpass.int_qos);
	}
#endif
}

static int lpass_fb_state_chg(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		lpass.display_on = false;
		lpass_update_qos();
		break;
	case FB_BLANK_UNBLANK:
		lpass.display_on = true;
		lpass_update_qos();
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_noti_block = {
	.notifier_call = lpass_fb_state_chg,
};

static int exynos_aud_alpa_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	switch (event) {
	case SICD_AUD_ENTER:
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		esa_compr_alpa_notifier(true);
#endif
		break;
	case SICD_AUD_EXIT:
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		esa_compr_alpa_notifier(false);
#endif
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block lpass_lpa_nb = {
	.notifier_call = exynos_aud_alpa_notifier,
};

static char banner[] =
	KERN_INFO "Samsung Low Power Audio Subsystem driver, "\
		  "(c)2013 Samsung Electronics\n";

static int lpass_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	printk(banner);

	lpass.pdev = pdev;

	/* LPASS version */
	lpass.ver = lpass_get_ver(np);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Unable to get LPASS SFRs\n");
		return -ENXIO;
	}

	lpass.regs = ioremap(res->start, resource_size(res));
	if (!lpass.regs) {
		dev_err(dev, "SFR ioremap failed\n");
		return -ENOMEM;
	}
	pr_info("%s: regs_base = %08X (%08X bytes)\n",
		__func__, (u32)res->start, (u32)resource_size(res));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "Unable to get LPASS SRAM\n");
		return -ENXIO;
	}

#ifndef CONFIG_SOC_EXYNOS8890
	lpass.mem = ioremap_wc(res->start, resource_size(res));
	if (!lpass.mem) {
		dev_err(dev, "SRAM ioremap failed\n");
		return -ENOMEM;
	}
#else
	lpass.mem = NULL;
#endif
	lpass.mem_size = resource_size(res);
	pr_info("%s: sram_base = %08X (%08X bytes)\n",
		__func__, (u32)res->start, (u32)resource_size(res));

	if (lpass.ver >= LPASS_VER_370200) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (!res) {
			dev_err(dev, "Unable to get LPASS-s SFRs\n");
			return -ENXIO;
		}

		lpass.regs_s = ioremap(res->start, resource_size(res));
		if (!lpass.regs_s) {
			dev_err(dev, "SFR ioremap failed\n");
			return -ENOMEM;
		}
		pr_info("%s: regs-s_base = %08X (%08X bytes)\n",
			__func__, (u32)res->start, (u32)resource_size(res));
	} else {
		lpass.regs_s = lpass.regs;
		pr_debug("%s: regs-s_base is set as regs", __func__);
	}

	ret = lpass_get_clk(&pdev->dev, &lpass);
	if (ret) {
		dev_err(dev, "failed to get clock\n");
		return -ENXIO;
	}

	ret = lpass_set_clk_heirachy(&pdev->dev);
	if (ret) {
		dev_err(dev, "failed to set clock hierachy\n");
		return -ENXIO;
	}
	lpass_init_clk_gate();

#ifdef CONFIG_SND_SAMSUNG_IOMMU
	lpass.domain = get_domain_from_dev(dev);
	if (!lpass.domain) {
		dev_err(dev, "Unable to get iommu domain\n");
		return -ENOENT;
	}
#else
	/* Bypass SysMMU */
	if (lpass.ver >= LPASS_VER_370210) {
		lpass.sysmmu = ioremap(0x114e0000, SZ_32);
		writel(0, lpass.sysmmu);
	}
#endif

	lpass.proc_file = proc_create("driver/lpass", 0,
					NULL, &lpass_proc_fops);
	if (!lpass.proc_file)
		pr_info("Failed to register /proc/driver/lpadd\n");

	spin_lock_init(&lpass.lock);
	atomic_set(&lpass.dma_use_cnt, 0);
	atomic_set(&lpass.use_cnt, 0);
	atomic_set(&lpass.stream_cnt, 0);
	lpass_init_reg_list();

	/* unmask irq source */
	if (is_new_ass())
		writel(INTR_CA5_MASK_VAL, lpass.regs + LPASS_INTR_CA5_MASK);

	writel(INTR_CPU_MASK_VAL, lpass.regs + LPASS_INTR_CPU_MASK);
	lpass_reg_save();
	lpass.valid = true;

	lpass.pmureg = syscon_regmap_lookup_by_phandle(dev->of_node,
			"samsung,syscon-phandle");
	if (IS_ERR(lpass.pmureg)) {
		dev_err(&pdev->dev, "syscon regmap lookup failed.\n");
		return PTR_ERR(lpass.pmureg);
	}

	regmap_update_bits(lpass.pmureg,
			EXYNOS_PMU_PMU_DEBUG_OFFSET,
			0x1F00, 0x1F00);
	regmap_update_bits(lpass.pmureg,
			EXYNOS_PMU_PMU_DEBUG_OFFSET,
			0x1, 0x0);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&lpass.pdev->dev);

	if (is_new_ass())
		pm_runtime_get_sync(&lpass.pdev->dev);
#else
	lpass_enable();
#endif
	lpass.display_on = true;
	fb_register_client(&fb_noti_block);

	lpass_update_lpclock(LPCLK_CTRLID_LEGACY|LPCLK_CTRLID_OFFLOAD, false);

#ifdef USE_AUD_DEVFREQ
	lpass.cpu_qos = 0;
	lpass.kfc_qos = 0;
	lpass.mif_qos = 0;
	lpass.int_qos = 0;
	pm_qos_add_request(&lpass.aud_cluster1_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);
	pm_qos_add_request(&lpass.aud_cluster0_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&lpass.aud_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&lpass.aud_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
#endif

	exynos_pm_register_notifier(&lpass_lpa_nb);
#ifdef CONFIG_SOC_EXYNOS8890
	iovmm_set_fault_handler(&lpass.pdev->dev,
				lpass_sysmmu_fault_handler, NULL);
	lpass.sram_fw_back = kzalloc(SRAM_SIZE, GFP_KERNEL);
	if (!lpass.sram_fw_back)
		pr_err("LPASS driver failed to allocate memory for SRAM FW Backup\n");
#endif

	pr_info("%s: LPASS driver was registerd successfully\n", __func__);
	return 0;
}

static int lpass_remove(struct platform_device *pdev)
{
#ifndef CONFIG_SND_SAMSUNG_IOMMU
	if (lpass.sysmmu)
		iounmap(lpass.sysmmu);
#endif
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#else
	lpass_disable();
#endif
	iounmap(lpass.regs);
	iounmap(lpass.regs_s);
#ifndef CONFIG_SOC_EXYNOS8890
	iounmap(lpass.mem);
#endif
#ifdef CONFIG_SOC_EXYNOS8890
	kfree(lpass.sram_fw_back);
#endif
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int lpass_runtime_suspend(struct device *dev)
{
	pr_debug("%s entered\n", __func__);

	lpass_disable();

	return 0;
}

static int lpass_runtime_resume(struct device *dev)
{
	pr_debug("%s entered\n", __func__);

	lpass_enable();

	return 0;
}
#endif

static const int lpass_ver_data[] = {
	[LPASS_VER_000100] = LPASS_VER_000100,
	[LPASS_VER_010100] = LPASS_VER_010100,
	[LPASS_VER_100100] = LPASS_VER_100100,
	[LPASS_VER_110100] = LPASS_VER_110100,
	[LPASS_VER_270120] = LPASS_VER_270120,
	[LPASS_VER_370100] = LPASS_VER_370100,
	[LPASS_VER_370200] = LPASS_VER_370200,
	[LPASS_VER_370210] = LPASS_VER_370210,
};

static struct platform_device_id lpass_driver_ids[] = {
	{
		.name	= "samsung-lpass",
	}, {
		.name	= "samsung-audss",
	}, {},
};
MODULE_DEVICE_TABLE(platform, lpass_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id exynos_lpass_match[] = {
	{
		.compatible	= "samsung,exynos8890-lpass",
		.data		= &lpass_ver_data[LPASS_VER_370210],
	}, {
		.compatible	= "samsung,exynos7580-lpass",
		.data		= &lpass_ver_data[LPASS_VER_270120],
	}, {
		.compatible	= "samsung,exynos7420-lpass",
		.data		= &lpass_ver_data[LPASS_VER_370210],
	}, {
		.compatible	= "samsung,exynos5433-lpass",
		.data		= &lpass_ver_data[LPASS_VER_370200],
	}, {
		.compatible	= "samsung,exynos5430-lpass",
		.data		= &lpass_ver_data[LPASS_VER_370100],
	}, {
		.compatible	= "samsung,exynos5-audss",
		.data		= &lpass_ver_data[LPASS_VER_110100],
	}, {},
};
MODULE_DEVICE_TABLE(of, exynos_lpass_match);
#endif

static const struct dev_pm_ops lpass_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		lpass_suspend,
		lpass_resume
	)
	SET_RUNTIME_PM_OPS(
		lpass_runtime_suspend,
		lpass_runtime_resume,
		NULL
	)
};

static struct platform_driver lpass_driver = {
	.probe		= lpass_probe,
	.remove		= lpass_remove,
	.id_table	= lpass_driver_ids,
	.driver		= {
		.name	= "samsung-lpass",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_lpass_match),
		.pm	= &lpass_pmops,
	},
};

static int __init lpass_driver_init(void)
{
	return platform_driver_register(&lpass_driver);
}
fs_initcall(lpass_driver_init);

#ifdef CONFIG_PM_RUNTIME
static int lpass_driver_rpm_begin(void)
{
	pr_debug("%s entered\n", __func__);

	if (is_new_ass())
		pm_runtime_put_sync(&lpass.pdev->dev);

	return 0;
}
late_initcall(lpass_driver_rpm_begin);
#endif

/* Module information */
MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Samsung Low Power Audio Subsystem Interface");
MODULE_ALIAS("platform:samsung-lpass");
MODULE_LICENSE("GPL");
