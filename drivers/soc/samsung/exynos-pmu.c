/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/smp.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/cpumask.h>

#include <asm/smp_plat.h>

#include <soc/samsung/exynos-pmu.h>

/**
 * register offset value from base address
 */
#define PMU_CP_STAT				0x0038

#define PMU_CPU_CONFIG_BASE			0x2000
#define PMU_CPU_STATUS_BASE			0x2004
#define PMU_CPU_ADDR_OFFSET			0x80
#define CPU_LOCAL_PWR_CFG			0xF

#define PMU_NONCPU_STATUS_BASE			0x2404
#define PMU_CPUSEQ_OPTION_BASE			0x2488
#define PMU_L2_STATUS_BASE			0x2604
#define PMU_CLUSTER_ADDR_OFFSET			0x20
#define NONCPU_LOCAL_PWR_CFG			0xF
#define L2_LOCAL_PWR_CFG			0x7

/**
 * "pmureg" has the mapped base address of PMU(Power Management Unit)
 */
static struct regmap *pmureg;

/**
 * No driver refers the "pmureg" directly, through the only exported API.
 */
int exynos_pmu_read(unsigned int offset, unsigned int *val)
{
	return regmap_read(pmureg, offset, val);
}

int exynos_pmu_write(unsigned int offset, unsigned int val)
{
	return regmap_write(pmureg, offset, val);
}

int exynos_pmu_update(unsigned int offset, unsigned int mask, unsigned int val)
{
	return regmap_update_bits(pmureg, offset, mask, val);
}

EXPORT_SYMBOL(exynos_pmu_read);
EXPORT_SYMBOL(exynos_pmu_write);
EXPORT_SYMBOL(exynos_pmu_update);

/**
 * CPU power control registers in PMU are arranged at regular intervals
 * (interval = 0x80). pmu_cpu_offset calculates how far cpu is from address
 * of first cpu. This expression is based on cpu and cluster id in MPIDR,
 * refer below.

 * cpu address offset : ((cluster id << 2) | (cpu id)) * 0x80
 */
#define pmu_cpu_offset(mpidr)			\
	(( MPIDR_AFFINITY_LEVEL(mpidr, 1) << 2	\
	 | MPIDR_AFFINITY_LEVEL(mpidr, 0))	\
	 * PMU_CPU_ADDR_OFFSET)

static void exynos_cpu_up(unsigned int cpu)
{
	unsigned int mpidr = cpu_logical_map(cpu);
	unsigned int offset;

	offset = pmu_cpu_offset(mpidr);
	regmap_update_bits(pmureg, PMU_CPU_CONFIG_BASE + offset,
			CPU_LOCAL_PWR_CFG, CPU_LOCAL_PWR_CFG);
}

static void exynos_cpu_down(unsigned int cpu)
{
	unsigned int mpidr = cpu_logical_map(cpu);
	unsigned int offset;

	offset = pmu_cpu_offset(mpidr);
	regmap_update_bits(pmureg, PMU_CPU_CONFIG_BASE + offset,
			CPU_LOCAL_PWR_CFG, 0);
}

static int exynos_cpu_state(unsigned int cpu)
{
	unsigned int mpidr = cpu_logical_map(cpu);
	unsigned int offset, val;

	offset = pmu_cpu_offset(mpidr);
	regmap_read(pmureg, PMU_CPU_STATUS_BASE + offset, &val);

	return ((val & CPU_LOCAL_PWR_CFG) == CPU_LOCAL_PWR_CFG);
}

static int exynos_cluster_state(unsigned int cluster)
{
	unsigned int noncpu_stat, l2_stat;
	unsigned int offset;

	offset = cluster * PMU_CLUSTER_ADDR_OFFSET;

	regmap_read(pmureg, PMU_NONCPU_STATUS_BASE + offset, &noncpu_stat);
	regmap_read(pmureg, PMU_L2_STATUS_BASE + offset, &l2_stat);

	return ((l2_stat & L2_LOCAL_PWR_CFG) == L2_LOCAL_PWR_CFG) &&
		((noncpu_stat & NONCPU_LOCAL_PWR_CFG) == NONCPU_LOCAL_PWR_CFG);
}

/**
 * While Exynos with multi cluster supports to shutdown down both cluster,
 * there is no benefit in boot cluster. So Exynos-PMU driver supports
 * only non-boot cluster down.
 */
void exynos_cpu_sequencer_ctrl(unsigned int cluster, int enable)
{
	unsigned int offset;

	offset = cluster * PMU_CLUSTER_ADDR_OFFSET;
	regmap_update_bits(pmureg, PMU_CPUSEQ_OPTION_BASE + offset, 1, enable);
}

static void exynos_cluster_up(unsigned int cluster)
{
	exynos_cpu_sequencer_ctrl(cluster, false);
}

static void exynos_cluster_down(unsigned int cluster)
{
	exynos_cpu_sequencer_ctrl(cluster, true);
}

struct exynos_cpu_power_ops exynos_cpu = {
	.power_up = exynos_cpu_up,
	.power_down = exynos_cpu_down,
	.power_state = exynos_cpu_state,
	.cluster_up = exynos_cluster_up,
	.cluster_down = exynos_cluster_down,
	.cluster_state = exynos_cluster_state,
};

#ifdef CONFIG_EXYNOS_BIG_FREQ_BOOST
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/cpu.h>

static int pmu_cpus_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	unsigned long timeout;
	int cpu, cnt = 0;
	int ret = NOTIFY_OK;
	struct cpumask mask;

	switch (event) {
	case CPUS_DOWN_COMPLETE:
		cpumask_andnot(&mask, &hmp_fast_cpu_mask, (struct cpumask *)data);
		timeout = jiffies + msecs_to_jiffies(1000);
		while (time_before(jiffies, timeout)) {
			for_each_cpu(cpu, &mask) {
				if (cpu_is_offline(cpu) && !exynos_cpu_state(cpu))
					cnt++;
			}
			if (cnt == cpumask_weight(&mask))
				break;
			else
				cnt = 0;
			udelay(1);
		}
		if (!cnt)
			ret = NOTIFY_BAD;

		break;
	default:
		break;
	}

	return ret;
}

static struct notifier_block exynos_pmu_cpus_nb = {
	.notifier_call = pmu_cpus_notifier,
	.priority = INT_MAX,				/* want to be called first */
};
#endif

static int exynos_pmu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pmureg = syscon_regmap_lookup_by_phandle(dev->of_node,
						"samsung,syscon-phandle");
	if (IS_ERR(pmureg)) {
		pr_err("Fail to get regmap of PMU\n");
		return PTR_ERR(pmureg);
	}

#ifdef CONFIG_EXYNOS_BIG_FREQ_BOOST
	register_cpus_notifier(&exynos_pmu_cpus_nb);
#endif

	return 0;
}

static const struct of_device_id of_exynos_pmu_match[] = {
	{ .compatible = "samsung,exynos-pmu", },
	{ },
};

static const struct platform_device_id exynos_pmu_ids[] = {
	{ "exynos-pmu", },
	{ }
};

static struct platform_driver exynos_pmu_driver = {
	.driver = {
		.name = "exynos-pmu",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_pmu_match,
	},
	.probe		= exynos_pmu_probe,
	.id_table	= exynos_pmu_ids,
};

int __init exynos_pmu_init(void)
{
	return platform_driver_register(&exynos_pmu_driver);
}
subsys_initcall(exynos_pmu_init);
