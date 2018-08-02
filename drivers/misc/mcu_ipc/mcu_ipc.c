/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * MCU IPC driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include "regs-mcu_ipc.h"
#include "mcu_ipc.h"

#ifdef CONFIG_MCU_IPC_LOG
#define LOG_MAX_NUM	SZ_4K
#define LOG_DIR_RX	0
#define LOG_DIR_TX	1

struct mailbox_log {
	int mcu_int;

	atomic_t gic_mbox_rx_idx;
	atomic_t mbox_idx;

	struct gic_mbox_rx {
		unsigned long long time;
		unsigned int count;
	} gic_mbox_rx_log[LOG_MAX_NUM];

	struct mbox_rx_tx {
		unsigned long long time;
		int dir;
		unsigned int count_rx;
		unsigned int count_tx;
		u32 intgr0;
		u32 intmr0;
		u32 intsr0;
		u32 intmsr0;
	} mbox_rx_tx_log[LOG_MAX_NUM * 2];
};

static struct mailbox_log mbx_log;

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

void mbox_check_mcu_irq(int irq)
{
	unsigned long flags;
	static unsigned int count;

	if (mbx_log.mcu_int != irq)
		return;

	flags = pure_arch_local_irq_save();
	{
		unsigned long i;
		int cpu = get_current_cpunum();

		i = atomic_inc_return(&mbx_log.gic_mbox_rx_idx) &
			(ARRAY_SIZE(mbx_log.gic_mbox_rx_log) - 1);

		mbx_log.gic_mbox_rx_log[i].time = cpu_clock(cpu);
		mbx_log.gic_mbox_rx_log[i].count = count++;
	}
	pure_arch_local_irq_restore(flags);

}
EXPORT_SYMBOL_GPL(mbox_check_mcu_irq);

static void mbox_save_rx_tx_log(int dir)
{
	unsigned long flags;
	static unsigned int count_rx;
	static unsigned int count_tx;

	flags = pure_arch_local_irq_save();
	{
		unsigned long i;
		int cpu = get_current_cpunum();

		i = atomic_inc_return(&mbx_log.mbox_idx) &
			(ARRAY_SIZE(mbx_log.mbox_rx_tx_log) - 1);

		mbx_log.mbox_rx_tx_log[i].time = cpu_clock(cpu);
		mbx_log.mbox_rx_tx_log[i].dir = dir;
		if (dir == LOG_DIR_RX)
			mbx_log.mbox_rx_tx_log[i].count_rx = count_rx++;
		else
			mbx_log.mbox_rx_tx_log[i].count_tx = count_tx++;

		mbx_log.mbox_rx_tx_log[i].intgr0 =
			mcu_ipc_readl(EXYNOS_MCU_IPC_INTGR0);
		mbx_log.mbox_rx_tx_log[i].intmr0 =
			mcu_ipc_readl(EXYNOS_MCU_IPC_INTMR0);
		mbx_log.mbox_rx_tx_log[i].intsr0 =
			mcu_ipc_readl(EXYNOS_MCU_IPC_INTSR0);
		mbx_log.mbox_rx_tx_log[i].intmsr0 =
			mcu_ipc_readl(EXYNOS_MCU_IPC_INTMSR0);
	}
	pure_arch_local_irq_restore(flags);
}
#endif

static irqreturn_t mcu_ipc_handler(int irq, void *data)
{
	u32 irq_stat, i;
	u32 mr0;

	if (!mcu_dat.ioaddr) {
		pr_err("%s: can't exec this func, probe failure\n", __func__);
		goto exit;
	}

#ifdef CONFIG_MCU_IPC_LOG
	mbox_save_rx_tx_log(LOG_DIR_RX);
#endif

	/* Check SFR vs Memory for INTMR0 */
	mr0 = mcu_ipc_readl(EXYNOS_MCU_IPC_INTMR0) & 0xFFFF0000;
	if (mr0 != mcu_dat.mr0) {
		dev_info(mcu_dat.mcu_ipc_dev, "ERR: INTMR0 is not matched. memory:%X SFR:%X\n"
				, mr0, mcu_dat.mr0);
		dev_info(mcu_dat.mcu_ipc_dev, "Update memory value to SFR\n");
		mcu_ipc_writel(mcu_dat.mr0, EXYNOS_MCU_IPC_INTMR0);
	}

	irq_stat = mcu_ipc_readl(EXYNOS_MCU_IPC_INTSR0) & 0xFFFF0000;
	/* Interrupt Clear */
	mcu_ipc_writel(irq_stat, EXYNOS_MCU_IPC_INTCR0);

	for (i = 0; i < 16; i++) {
		if (irq_stat & (1 << (i + 16))) {
			if ((1 << (i + 16)) & mcu_dat.registered_irq)
				mcu_dat.hd[i].handler(mcu_dat.hd[i].data);
			else
				dev_err_ratelimited(mcu_dat.mcu_ipc_dev,
					"irq:0x%x, Unregistered INT received.\n", irq_stat);

			irq_stat &= ~(1 << (i + 16));
		}

		if (!irq_stat)
			break;
	}

exit:
	return IRQ_HANDLED;
}

int mbox_request_irq(u32 int_num, void (*handler)(void *), void *data)
{
	u32 mr0;

	if ((!handler) || (int_num > 15) || !mcu_dat.ioaddr)
		return -EINVAL;

	mcu_dat.hd[int_num].data = data;
	mcu_dat.hd[int_num].handler = handler;
	mcu_dat.registered_irq |= 1 << (int_num + 16);

	/* Update SFR */
	mr0 = mcu_ipc_readl(EXYNOS_MCU_IPC_INTMR0) & 0xFFFF0000;
	mr0 &= ~(1 << (int_num + 16));
	mcu_ipc_writel(mr0, EXYNOS_MCU_IPC_INTMR0);

	/* Update Memory */
	mcu_dat.mr0 &= ~(1 << (int_num + 16));

	return 0;
}
EXPORT_SYMBOL(mbox_request_irq);

int mcu_ipc_unregister_handler(u32 int_num, void (*handler)(void *))
{
	u32 mr0;

	if (!handler || !mcu_dat.ioaddr ||
			(mcu_dat.hd[int_num].handler != handler))
		return -EINVAL;

	mcu_dat.hd[int_num].data = NULL;
	mcu_dat.hd[int_num].handler = NULL;
	mcu_dat.registered_irq &= ~(1 << (int_num + 16));

	/* Update SFR */
	mr0 = mcu_ipc_readl(EXYNOS_MCU_IPC_INTMR0) & 0xFFFF0000;
	mr0 |= (1 << (int_num + 16));
	mcu_ipc_writel(mr0, EXYNOS_MCU_IPC_INTMR0);

	/* Update Memory */
	mcu_dat.mr0 |= (1 << (int_num + 16));

	return 0;
}
EXPORT_SYMBOL(mcu_ipc_unregister_handler);

void mbox_set_interrupt(u32 int_num)
{
	if (!mcu_dat.ioaddr) {
		pr_err("%s: can't exec this func, probe failure\n", __func__);
		return;
	}

	/* generate interrupt */
	if (int_num < 16)
		mcu_ipc_writel(0x1 << int_num, EXYNOS_MCU_IPC_INTGR1);

#ifdef CONFIG_MCU_IPC_LOG
	mbox_save_rx_tx_log(LOG_DIR_TX);
#endif
}
EXPORT_SYMBOL(mbox_set_interrupt);

void mcu_ipc_send_command(u32 int_num, u16 cmd)
{
	if (!mcu_dat.ioaddr) {
		pr_err("%s: can't exec this func, probe failure\n", __func__);
		return;
	}

	/* write command */
	if (int_num < 16)
		mcu_ipc_writel(cmd, EXYNOS_MCU_IPC_ISSR0 + (8 * int_num));

	/* generate interrupt */
	mbox_set_interrupt(int_num);
}
EXPORT_SYMBOL(mcu_ipc_send_command);

void mcu_ipc_clear_all_interrupt(void)
{
	if (!mcu_dat.ioaddr) {
		pr_err("%s: can't exec this func, probe failure\n", __func__);
		return;
	}

	mcu_ipc_writel(0xFFFF, EXYNOS_MCU_IPC_INTCR1);

	/* apply all interrupt mask */
	mcu_ipc_writel(0xFFFF0000, EXYNOS_MCU_IPC_INTMR0);
	mcu_dat.mr0 = 0xFFFF0000;
}

u32 mbox_get_value(u32 mbx_num)
{
	if (!mcu_dat.ioaddr)
		return -EINVAL;

	if (mbx_num < 64)
		return mcu_ipc_readl(EXYNOS_MCU_IPC_ISSR0 + (4 * mbx_num));
	else
		return 0;
}
EXPORT_SYMBOL(mbox_get_value);

void mbox_set_value(u32 mbx_num, u32 msg)
{
	if (!mcu_dat.ioaddr) {
		pr_err("%s: can't exec this func, probe failure\n", __func__);
		return;
	}

	if (mbx_num < 64)
		mcu_ipc_writel(msg, EXYNOS_MCU_IPC_ISSR0 + (4 * mbx_num));
}
EXPORT_SYMBOL(mbox_set_value);

#ifdef CONFIG_ARGOS
static int mcu_ipc_set_affinity(struct device *dev, int irq)
{
	struct device_node *np = dev->of_node;
	u32 irq_affinity_mask = 0;

	if (!np)	{
		dev_err(dev, "non-DT project, can't set irq affinity\n");
		return -ENODEV;
	}

	if (of_property_read_u32(np, "mcu,irq_affinity_mask",
			&irq_affinity_mask))	{
		dev_err(dev, "Failed to get affinity mask\n");
		return -ENODEV;
	}

	dev_info(dev, "irq_affinity_mask = 0x%x\n", irq_affinity_mask);

	if (!zalloc_cpumask_var(&mcu_dat.dmask, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&mcu_dat.imask, GFP_KERNEL))
		return -ENOMEM;

	cpumask_or(mcu_dat.imask, mcu_dat.imask, cpumask_of(irq_affinity_mask));
	cpumask_copy(mcu_dat.dmask, get_default_cpu_mask());

	return argos_irq_affinity_setup_label(irq, "IPC", mcu_dat.imask,
			mcu_dat.dmask);
}
#else
static inline int mcu_ipc_set_affinity(struct device *dev, int irq)
{
	return 0;
}
#endif

#ifdef CONFIG_MCU_IPC_TEST
static void test_without_cp(void)
{
	int i;

	for (i = 0; i < 64; i++) {
		mbox_set_value(i, 64 - i);
		mdelay(50);
		dev_err(mcu_dat.mcu_ipc_dev, "Test without CP: Read mbox value[%d]: %d\n",
				i, mbox_get_value(i));
	}
}
#endif

static int mcu_ipc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res = NULL;
	int mcu_ipc_irq;
	int err = 0;

	dev_err(dev, "%s: mcu init\n", __func__);
	mcu_dat.mcu_ipc_dev = dev;

	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	if (!dev->coherent_dma_mask)
		dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (dev->of_node) {
		mcu_dt_read_string(dev->of_node, "mcu,name", mcu_dat.name);
		if (IS_ERR(&mcu_dat)) {
			dev_err(dev, "MCU IPC parse error!\n");
			return PTR_ERR(&mcu_dat);
		}
	}

	/* resource for mcu_ipc SFR region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no memory resource defined\n");
		return -ENOENT;
	}

	res = request_mem_region(res->start, resource_size(res), mcu_dat.name);
	if (!res) {
		dev_err(dev, "failded to request memory resource\n");
		return -ENOENT;
	}

	mcu_dat.ioaddr = ioremap(res->start, resource_size(res));
	if (!mcu_dat.ioaddr) {
		dev_err(dev, "failded to request memory resource\n");
		err = -ENXIO;
		goto release_mcu_ipc;
	}

	/* Request IRQ */
	mcu_ipc_irq = platform_get_irq(pdev, 0);
	dev_err(dev, "%s: mcu interrupt num: %d\n", __func__, mcu_ipc_irq);
	if (mcu_ipc_irq < 0) {
		dev_err(dev, "no irq specified\n");
		err = mcu_ipc_irq;
		goto unmap_ioaddr;
	}

	err = request_irq(mcu_ipc_irq, mcu_ipc_handler, 0, pdev->name, pdev);
	if (err) {
		dev_err(dev, "Can't request MCU_IPC IRQ\n");
		goto unmap_ioaddr;
	}

#ifdef CONFIG_MCU_IPC_LOG
	mbx_log.mcu_int = mcu_ipc_irq;
#endif

	mcu_ipc_clear_all_interrupt();

	/* set argos irq affinity */
	err = mcu_ipc_set_affinity(dev, mcu_ipc_irq);
	if (err)
		dev_err(dev, "Can't set IRQ affinity with(%d)\n", err);

#ifdef CONFIG_MCU_IPC_TEST
	test_without_cp();
#endif
	return 0;

unmap_ioaddr:
	iounmap(mcu_dat.ioaddr);
	mcu_dat.ioaddr = NULL;

release_mcu_ipc:
	release_mem_region(res->start, resource_size(res));
	mcu_dat.mcu_ipc_dev = NULL;

	return err;
}

static int __exit mcu_ipc_remove(struct platform_device *pdev)
{
	/* TODO */
	return 0;
}

#ifdef CONFIG_PM
static int mcu_ipc_suspend(struct device *dev)
{
	/* TODO */
	return 0;
}

static int mcu_ipc_resume(struct device *dev)
{
	/* TODO */
	return 0;
}
#else
#define mcu_ipc_suspend NULL
#define mcu_ipc_resume NULL
#endif

static const struct dev_pm_ops mcu_ipc_pm_ops = {
	.suspend = mcu_ipc_suspend,
	.resume = mcu_ipc_resume,
};

static const struct of_device_id exynos_mcu_ipc_dt_match[] = {
		{ .compatible = "samsung,exynos7580-mailbox", },
		{ .compatible = "samsung,exynos7890-mailbox", },
		{ .compatible = "samsung,exynos8890-mailbox", },
		{},
};
MODULE_DEVICE_TABLE(of, exynos_mcu_ipc_dt_match);

static struct platform_driver mcu_ipc_driver = {
	.probe		= mcu_ipc_probe,
	.remove		= mcu_ipc_remove,
	.driver		= {
		.name = "mcu_ipc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_mcu_ipc_dt_match),
		.pm = &mcu_ipc_pm_ops,
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(mcu_ipc_driver);

MODULE_DESCRIPTION("MCU IPC driver");
MODULE_AUTHOR("<hy50.seo@samsung.com>");
MODULE_LICENSE("GPL");
