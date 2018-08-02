/*
 * Platform Dependent file for Samsung Exynos
 *
 * Copyright (C) 1999-2017, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_custom_exynos.c 676020 2016-12-20 04:35:47Z $
 */
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/bug.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/wlan_plat.h>

#if defined(CONFIG_64BIT)
#include <asm/gpio.h>
#else
#include <mach/gpio.h>
#endif /* CONFIG_64BIT */
#include <mach/irqs.h>
#include <linux/sec_sysfs.h>

#include <plat/gpio-cfg.h>

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
extern int dhd_init_wlan_mem(void);
extern void *dhd_wlan_mem_prealloc(int section, unsigned long size);
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

#define WIFI_TURNON_DELAY	200
static struct device *wlan_dev;
static int wlan_pwr_on = -1;
int wlan_host_wake_irq = 0;
EXPORT_SYMBOL(wlan_host_wake_irq);

#ifdef CONFIG_BCMDHD_PCIE
#define EXYNOS_PCIE_RC_ONOFF
#endif /* CONFIG_BCMDHD_PCIE */

#ifdef EXYNOS_PCIE_RC_ONOFF
#ifdef CONFIG_MACH_UNIVERSAL5433
#define SAMSUNG_PCIE_CH_NUM
#elif defined(CONFIG_MACH_UNIVERSAL7420)
#define SAMSUNG_PCIE_CH_NUM 1
#elif defined(CONFIG_MACH_EXSOM7420)
#define SAMSUNG_PCIE_CH_NUM 1
#elif defined(CONFIG_SOC_EXYNOS8890)
#define SAMSUNG_PCIE_CH_NUM 0
#elif defined(CONFIG_SOC_EXYNOS8895)
#define SAMSUNG_PCIE_CH_NUM 0
#endif /* CONFIG_MACH_UNIVERSAL5433 */

#if defined(CONFIG_MACH_UNIVERSAL5433)
extern void exynos_pcie_poweron(void);
extern void exynos_pcie_poweroff(void);
extern int check_rev(void);
#else
extern void exynos_pcie_poweron(int);
extern void exynos_pcie_poweroff(int);
#endif /* CONFIG_MACH_UNIVERSAL5433 */
#endif /* EXYNOS_PCIE_RC_ONOFF */

#if defined(CONFIG_ARGOS)
extern int argos_irq_affinity_setup_label(unsigned int irq, const char *label,
	struct cpumask *affinity_cpu_mask,
	struct cpumask *default_cpu_mask);
#endif /* CONFIG_ARGOS */
#ifdef CONFIG_MACH_UNIVERSAL3475
extern struct mmc_host *wlan_mmc;
extern void mmc_ctrl_power(struct mmc_host *host, bool onoff);
#endif /* CONFIG_MACH_UNIVERSAL3475 */

static int
dhd_wlan_power(int onoff)
{
	printk(KERN_INFO"------------------------------------------------");
	printk(KERN_INFO"------------------------------------------------\n");
	printk(KERN_INFO"%s Enter: power %s\n", __FUNCTION__, onoff ? "on" : "off");

#ifdef EXYNOS_PCIE_RC_ONOFF

	if (!onoff) {
		exynos_pcie_poweroff(SAMSUNG_PCIE_CH_NUM);
	}

#if defined(CONFIG_MACH_UNIVERSAL5433)
	/* Old revision chip can't control WL_REG_ON */
	if (check_rev()) {
#endif /* CONFIG_MACH_UINVERSAL5433 */
		if (gpio_direction_output(wlan_pwr_on, onoff)) {
			printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
				__FUNCTION__, onoff ? "HIGH" : "LOW");
			return -EIO;
		}
#if defined(CONFIG_MACH_UNIVERSAL5433)
	}
#endif /* CONFIG_MACH_UINVERSAL5433 */

	if (onoff) {
		exynos_pcie_poweron(SAMSUNG_PCIE_CH_NUM);
	}
#else
	/* In no pcie case */
	if (gpio_direction_output(wlan_pwr_on, onoff)) {
		printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
			__FUNCTION__, onoff ? "HIGH" : "LOW");
		return -EIO;
	}
#endif /* EXYNOS_PCIE_RC_ONOFF */

#ifdef CONFIG_MACH_UNIVERSAL3475
	if (wlan_mmc)
		mmc_ctrl_power(wlan_mmc, onoff);
#endif /* CONFIG_MACH_UNIVERSAL3475 */
	return 0;
}

static int
dhd_wlan_reset(int onoff)
{
	return 0;
}

#ifndef CONFIG_BCMDHD_PCIE
extern void (*notify_func_callback)(void *dev_id, int state);
extern void *mmc_host_dev;

static int
dhd_wlan_set_carddetect(int val)
{
	pr_err("%s: notify_func=%p, mmc_host_dev=%p, val=%d\n",
		__FUNCTION__, notify_func_callback, mmc_host_dev, val);

	if (notify_func_callback) {
		notify_func_callback(mmc_host_dev, val);
	} else {
		pr_warning("%s: Nobody to notify\n", __FUNCTION__);
	}

	return 0;
}
#endif /* !CONFIG_BCMDHD_PCIE */

int __init
dhd_wlan_init_gpio(void)
{
	const char *wlan_node = "samsung,brcm-wlan";
	unsigned int wlan_host_wake_up = -1;
	struct device_node *root_node = NULL;

	wlan_dev = sec_device_create(NULL, "wlan");
	BUG_ON(!wlan_dev);

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		WARN(1, "failed to get device node of bcm4354\n");
		return -ENODEV;
	}

	/* ========== WLAN_PWR_EN ============ */
	wlan_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(wlan_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_pwr_on);
		return -ENODEV;
	}

	if (gpio_request(wlan_pwr_on, "WLAN_REG_ON")) {
		WARN(1, "fail to request gpio(WLAN_REG_ON)\n");
		return -ENODEV;
	}
#ifdef CONFIG_BCMDHD_PCIE
	gpio_direction_output(wlan_pwr_on, 1);
#else
	gpio_direction_output(wlan_pwr_on, 0);
#endif /* CONFIG_BCMDHD_PCIE */
	gpio_export(wlan_pwr_on, 1);
	gpio_export_link(wlan_dev, "WLAN_REG_ON", wlan_pwr_on);
	msleep(WIFI_TURNON_DELAY);
#if defined(CONFIG_MACH_UNIVERSAL7420)
	exynos_pcie_poweron(1);
#endif /* CONFIG_MACH_UNIVERSAL7420 */

	/* ========== WLAN_HOST_WAKE ============ */
	wlan_host_wake_up = of_get_gpio(root_node, 1);
	if (!gpio_is_valid(wlan_host_wake_up)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_host_wake_up);
		return -ENODEV;
	}

	if (gpio_request(wlan_host_wake_up, "WLAN_HOST_WAKE")) {
		WARN(1, "fail to request gpio(WLAN_HOST_WAKE)\n");
		return -ENODEV;
	}
	gpio_direction_input(wlan_host_wake_up);
	gpio_export(wlan_host_wake_up, 1);
	gpio_export_link(wlan_dev, "WLAN_HOST_WAKE", wlan_host_wake_up);

	wlan_host_wake_irq = gpio_to_irq(wlan_host_wake_up);

	return 0;
}

#if defined(CONFIG_ARGOS)
#if defined(CONFIG_SPLIT_ARGOS_SET)
#define ARGOS_WIFI_TABLE_LABEL "WIFI TX"
#else /* CONFIG_SPLIT_ARGOS_SET */
#define ARGOS_WIFI_TABLE_LABEL "WIFI"
#endif /* CONFIG_SPLIT_ARGOS_SET  */
#if defined(CONFIG_BCMDHD_PCIE)
#if defined(CONFIG_MACH_UNIVERSAL7420)
#define ARGOS_IRQ_NUMBER 237
#else
#define ARGOS_IRQ_NUMBER 277
#endif /* CONFIG_MACH_UNIVERSAL7420 */
#endif /* CONFIG_BCMDHD_PCIE */

void
set_cpucore_for_interrupt(cpumask_var_t default_cpu_mask,
	cpumask_var_t affinity_cpu_mask) {
	argos_irq_affinity_setup_label(ARGOS_IRQ_NUMBER,
		ARGOS_WIFI_TABLE_LABEL, affinity_cpu_mask, default_cpu_mask);
}
EXPORT_SYMBOL(set_cpucore_for_interrupt);
#endif /* CONFIG_ARGOS */

void
interrupt_set_cpucore(int set, unsigned int dpc_cpucore, unsigned int primary_cpucore)
{
	printk(KERN_INFO "%s: set: %d\n", __FUNCTION__, set);
	if (set) {
#if defined(CONFIG_MACH_UNIVERSAL5422)
		irq_set_affinity(EXYNOS5_IRQ_HSMMC1, cpumask_of(dpc_cpucore));
		irq_set_affinity(EXYNOS_IRQ_EINT16_31, cpumask_of(dpc_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL5422 */
#if defined(CONFIG_MACH_UNIVERSAL5430)
		irq_set_affinity(IRQ_SPI(226), cpumask_of(dpc_cpucore));
		irq_set_affinity(IRQ_SPI(2), cpumask_of(dpc_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL5430 */
#if defined(CONFIG_MACH_UNIVERSAL7580)
		irq_set_affinity(IRQ_SPI(246), cpumask_of(dpc_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL7580 */
	} else {
#if defined(CONFIG_MACH_UNIVERSAL5422)
		irq_set_affinity(EXYNOS5_IRQ_HSMMC1, cpumask_of(primary_cpucore));
		irq_set_affinity(EXYNOS_IRQ_EINT16_31, cpumask_of(primary_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL5422 */
#if defined(CONFIG_MACH_UNIVERSAL5430)
		irq_set_affinity(IRQ_SPI(226), cpumask_of(primary_cpucore));
		irq_set_affinity(IRQ_SPI(2), cpumask_of(primary_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL5430 */
#if defined(CONFIG_MACH_UNIVERSAL7580)
		irq_set_affinity(IRQ_SPI(246), cpumask_of(primary_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL7580 */
	}
}

struct resource dhd_wlan_resources = {
	.name	= "bcmdhd_wlan_irq",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE |
#ifdef CONFIG_BCMDHD_PCIE
	IORESOURCE_IRQ_HIGHEDGE,
#else
	IORESOURCE_IRQ_HIGHLEVEL,
#endif /* CONFIG_BCMDHD_PCIE */
};
EXPORT_SYMBOL(dhd_wlan_resources);

struct wifi_platform_data dhd_wlan_control = {
	.set_power	= dhd_wlan_power,
	.set_reset	= dhd_wlan_reset,
#ifndef CONFIG_BCMDHD_PCIE
	.set_carddetect	= dhd_wlan_set_carddetect,
#endif /* !CONFIG_BCMDHD_PCIE */
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= dhd_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
};
EXPORT_SYMBOL(dhd_wlan_control);

int __init
dhd_wlan_init(void)
{
	int ret;

	printk(KERN_INFO "%s: START.......\n", __FUNCTION__);
	ret = dhd_wlan_init_gpio();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initiate GPIO, ret=%d\n",
			__FUNCTION__, ret);
		goto fail;
	}

	dhd_wlan_resources.start = wlan_host_wake_irq;
	dhd_wlan_resources.end = wlan_host_wake_irq;

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	ret = dhd_init_wlan_mem();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to alloc reserved memory,"
			" ret=%d\n", __FUNCTION__, ret);
	}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

fail:
	return ret;
}

#if defined(CONFIG_MACH_UNIVERSAL7420)
#if defined(CONFIG_DEFERRED_INITCALLS)
deferred_module_init(dhd_wlan_init);
#else
late_initcall(dhd_wlan_init);
#endif /* CONFIG_DEFERRED_INITCALLS */
#else
device_initcall(dhd_wlan_init);
#endif /* CONFIG_MACH_UNIVERSAL7420 */
