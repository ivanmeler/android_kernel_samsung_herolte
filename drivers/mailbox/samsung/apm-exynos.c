/* linux/arch/arm/mach-exynos/apm-exynos.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - APM driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/err.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mailbox_client.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mailbox-exynos.h>
#include <linux/apm-exynos.h>
#include <asm/io.h>
#include <soc/samsung/asv-exynos.h>

/* Add OPS */
struct cl_ops *cl_ops;
int apm_wfi_prepare = APM_ON;
static int cm3_status;
static unsigned int cl_mode_status = CL_ON;
extern struct cl_ops exynos_cl_function_ops;

static DEFINE_MUTEX(cl_lock);

void cl_dvfs_lock(void)
{
	mutex_lock(&cl_lock);
}
EXPORT_SYMBOL_GPL(cl_dvfs_lock);

void cl_dvfs_unlock(void)
{
	mutex_unlock(&cl_lock);
}
EXPORT_SYMBOL_GPL(cl_dvfs_unlock);

/* Routines for PM-transition notifications */
static BLOCKING_NOTIFIER_HEAD(apm_chain_head);

int register_apm_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&apm_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_apm_notifier);

int unregister_apm_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&apm_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_apm_notifier);

int apm_notifier_call_chain(unsigned long val)
{
	int ret;

	ret = blocking_notifier_call_chain(&apm_chain_head, val, NULL);

	return notifier_to_errno(ret);
}

/* exynos_apm_reset_release()
 * exynos_apm_reset_release set PMU register.
 */
void exynos_apm_reset_release(void)
{
	cl_ops->apm_reset();
}

/* exynos_apm_power_up()
 * exynos_apm_power_up set PMU register.
 */
void exynos_apm_power_up(void)
{
	cl_ops->apm_power_up();
}

/* exynos_apm_power_down()
 * exynos_apm_power_down set PMU register.
 */
void exynos_apm_power_down(void)
{
	cl_ops->apm_power_down();
}

/* exynos_cl_dvfs_setup()
 * exynos_cl_dvfs_setup set voltage margin limit and period.
 */
int exynos_cl_dvfs_setup(unsigned int atlas_cl_limit, unsigned int apollo_cl_limit,
					unsigned int g3d_cl_limit, unsigned int mif_cl_limit, unsigned int cl_period)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = cl_ops->cl_dvfs_setup(atlas_cl_limit, apollo_cl_limit,
					g3d_cl_limit, mif_cl_limit, cl_period);
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_cl_dvfs_setup);

/* exynos7420_cl_dvfs_start()
 * cl_dvfs_start means os send cl_dvfs start command.
 * We change voltage and frequency, after than start cl-dvfs.
 */
int exynos_cl_dvfs_start(unsigned int cl_domain)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	if (cl_mode_status)
		ret = cl_ops->cl_dvfs_start(cl_domain);
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_cl_dvfs_start);

/* exynos_cl_dvfs_stop()
 * cl_dvfs_stop means os send cl_dvfs stop command to CM3.
 * We need change voltage and frequency. first, we stop cl-dvfs.
 */
int exynos_cl_dvfs_stop(unsigned int cl_domain, unsigned int level)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	if (cl_mode_status)
		ret = cl_ops->cl_dvfs_stop(cl_domain, level);
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_cl_dvfs_stop);

/* exynos7420_cl_dvfs_mode_disable()
 * cl_dvfs_stop means os send cl_dvfs stop command to CM3.
 * We need change voltage and frequency. first, we stop cl-dvfs.
 */
int exynos_cl_dvfs_mode_enable(void)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = cl_ops->cl_dvfs_enable();
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_cl_dvfs_mode_enable);

/* exynos7420_cl_dvfs_mode_disable()
 * cl_dvfs_stop means os send cl_dvfs stop command to CM3.
 * We need change voltage and frequency. first, we stop cl-dvfs.
 */
int exynos_cl_dvfs_mode_disable(void)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = cl_ops->cl_dvfs_disable();
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_cl_dvfs_mode_disable);

/* exynos_g3d_power_on_noti_apm()
 * APM driver notice g3d power on status to CM3.
 */
int exynos_g3d_power_on_noti_apm(void)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = cl_ops->g3d_power_on();
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_g3d_power_on_noti_apm);

/* exynos7420_g3d_power_on_noti_apm()
 * APM driver notice g3d power off status to CM3.
 */
int exynos_g3d_power_down_noti_apm(void)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = cl_ops->g3d_power_down();
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_g3d_power_down_noti_apm);

/* exynos_apm_enter_wfi();
 * This function send CM3 go to WFI message to CM3.
 */
int exynos_apm_enter_wfi(void)
{
	cl_ops->enter_wfi();

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_apm_enter_wfi);

static int exynos_cm3_status_show(struct seq_file *buf, void *d)
{
	/* Show pmic communcation mode */
	if (cm3_status == HSI2C_MODE) seq_printf(buf, "mode : HSI2C \n");
	else if (cm3_status == APM_MODE) seq_printf(buf, "mode : APM \n");
	else if (cm3_status == APM_TIMOUT) seq_printf(buf, "mode : HSI2C (CM3 timeout) \n");

	return 0;
}

int cm3_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_cm3_status_show, inode->i_private);
}

#ifdef CONFIG_EXYNOS_MBOX
static int exynos_apm_function_notifier(struct notifier_block *notifier,
						unsigned long pm_event, void *v)
{
	switch (pm_event) {
		case APM_READY:
			cm3_status = APM_MODE;
			apm_wfi_prepare = APM_OFF;
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
			samsung_mbox_enable_irq();
#endif
			pr_info("mailbox: hsi2c -> apm mode \n");
			break;
		case APM_SLEEP:
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
			if (cm3_status != APM_TIMOUT)
				samsung_mbox_disable_irq();
#endif
			if (cm3_status == APM_MODE)
				pr_info("mailbox: apm -> hsi2c mode \n");
			cm3_status = HSI2C_MODE;
			apm_wfi_prepare = APM_ON;
			break;
		case APM_TIMEOUT:
			cm3_status = APM_TIMOUT;
			apm_wfi_prepare = APM_WFI_TIMEOUT;
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
			samsung_mbox_disable_irq();
#endif
			pr_info("mailbox: apm -> hsi2c mode(timeout) \n");
			break;
		case CL_ENABLE:
			cl_mode_status = CL_ON;
			break;
		case CL_DISABLE:
			cl_mode_status = CL_OFF;
			break;
		default:
			break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_apm_notifier = {
	.notifier_call = exynos_apm_function_notifier,
};
#endif

static int exynos_apm_probe(struct platform_device *pdev)
{
#ifdef CONFIG_EXYNOS_MBOX
	register_apm_notifier(&exynos_apm_notifier);
	cl_ops = &exynos_cl_function_ops;
	exynos_mbox_client_init(&pdev->dev);
#endif
	return 0;
}

static int exynos_apm_remove(struct platform_device *pdev)
{
#ifdef CONFIG_EXYNOS_MBOX
	unregister_apm_notifier(&exynos_apm_notifier);
#endif
	return 0;
}

static const struct of_device_id apm_smc_match[] = {
	{ .compatible = "samsung,exynos-apm" },
	{},
};

static struct platform_driver exynos_apm_driver = {
	.probe	= exynos_apm_probe,
	.remove = exynos_apm_remove,
	.driver	= {
		.name = "exynos-apm-driver",
		.owner	= THIS_MODULE,
		.of_match_table	= apm_smc_match,
	},
};

static int __init exynos_apm_init(void)
{
	return platform_driver_register(&exynos_apm_driver);
}
fs_initcall(exynos_apm_init);

static void __exit exynos_apm_exit(void)
{
	platform_driver_unregister(&exynos_apm_driver);
}
module_exit(exynos_apm_exit);
