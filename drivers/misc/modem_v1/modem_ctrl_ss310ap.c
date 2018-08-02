/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mcu_ipc.h>
#include <linux/smc.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "pmu-cp.h"

#ifdef CONFIG_EXYNOS_BUSMONITOR
#include <linux/exynos-busmon.h>
#endif

#define MIF_INIT_TIMEOUT	(15 * HZ)

#ifdef CONFIG_REGULATOR_S2MPU01A
#include <linux/mfd/samsung/s2mpu01a.h>
static inline int change_cp_pmu_manual_reset(void)
{
	return change_mr_reset();
}
#else
static inline int change_cp_pmu_manual_reset(void) {return 0; }
#endif

static struct modem_ctl *g_mc;

static int sys_rev;

static irqreturn_t cp_wdt_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct io_device *iod;
	enum modem_state new_state;

	mif_disable_irq(&mc->irq_cp_wdt);
	mif_err("%s: ERR! CP_WDOG occurred\n", mc->name);

	/* Disable debug Snapshot */
	mif_set_snapshot(false);

	exynos_clear_cp_reset(mc);
	new_state = STATE_CRASH_WATCHDOG;

	mif_err("new_state = %s\n", cp_state_str(new_state));

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, new_state);
	}

	return IRQ_HANDLED;
}

static irqreturn_t cp_fail_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct io_device *iod;
	enum modem_state new_state;

	mif_disable_irq(&mc->irq_cp_fail);
	mif_err("%s: ERR! CP_FAIL occurred\n", mc->name);

	exynos_cp_active_clear(mc);
	new_state = STATE_CRASH_RESET;

	mif_err("new_state = %s\n", cp_state_str(new_state));

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, new_state);
	}

	return IRQ_HANDLED;
}

static void cp_active_handler(void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct io_device *iod;
	int cp_on = exynos_get_cp_power_status(mc);
	int cp_active = mbox_get_value(mc->mbx_phone_active);
	enum modem_state old_state = mc->phone_state;
	enum modem_state new_state = mc->phone_state;

	mif_err("old_state:%s cp_on:%d cp_active:%d\n",
		cp_state_str(old_state), cp_on, cp_active);

	if (!cp_active) {
		if (cp_on > 0) {
			new_state = STATE_OFFLINE;
			complete_all(&mc->off_cmpl);
		} else {
			mif_err("don't care!!!\n");
		}
	}

	if (old_state != new_state) {
		mif_err("new_state = %s\n", cp_state_str(new_state));

		list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
			if (iod && atomic_read(&iod->opened) > 0)
				iod->modem_state_changed(iod, new_state);
		}
	}
}

#ifdef CONFIG_HW_REV_DETECT
static int __init console_setup(char *str)
{
	get_option(&str, &sys_rev);
	mif_info("board_rev : %d\n", sys_rev);
	
	return 0;
}
__setup("androidboot.hw_rev=", console_setup);
#else
static int get_system_rev(struct device_node *np)
{
	int value, cnt, gpio_cnt;
	unsigned gpio_hw_rev, hw_rev = 0;

	gpio_cnt = of_gpio_count(np);
	if (gpio_cnt < 0) {
		mif_err("failed to get gpio_count from DT(%d)\n", gpio_cnt);
		return gpio_cnt;
	}

	for (cnt = 0; cnt < gpio_cnt; cnt++) {
		gpio_hw_rev = of_get_gpio(np, cnt);
		if (!gpio_is_valid(gpio_hw_rev)) {
			mif_err("gpio_hw_rev%d: Invalied gpio\n", cnt);
			return -EINVAL;
		}

		value = gpio_get_value(gpio_hw_rev);
		hw_rev |= (value & 0x1) << cnt;
	}

	return hw_rev;
}
#endif

#ifdef CONFIG_GPIO_DS_DETECT
static int get_ds_detect(struct device_node *np)
{
	unsigned gpio_ds_det;

	gpio_ds_det = of_get_named_gpio(np, "mif,gpio_ds_det", 0);
	if (!gpio_is_valid(gpio_ds_det)) {
		mif_err("gpio_ds_det: Invalid gpio\n");
		return 0;
	}

	return gpio_get_value(gpio_ds_det);
}
#else
static int ds_detect = 1;
module_param(ds_detect, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(ds_detect, "Dual SIM detect");

static int get_ds_detect(struct device_node *np)
{
	mif_info("Dual SIM detect = %d\n", ds_detect);
	return ds_detect - 1;
}
#endif

static int init_mailbox_regs(struct modem_ctl *mc)
{
	struct platform_device *pdev = to_platform_device(mc->dev);
	struct device_node *np = pdev->dev.of_node;
	unsigned int info_val, val;
	int ds_det, i;

	for (i = 0; i < MAX_MBOX_NUM; i++)
		mbox_set_value(i, 0);

	if (np) {
		mif_dt_read_u32(np, "mbx_ap2cp_info_value", info_val);

#ifndef CONFIG_HW_REV_DETECT
		sys_rev = get_system_rev(np);
#endif
		ds_det = get_ds_detect(np);
		if (sys_rev < 0 || ds_det < 0)
			return -EINVAL;

		val = sys_rev | (ds_det & 0x1) << 8;
		mbox_set_value(info_val, val);

		mif_info("sys_rev:%d, ds_det:%u (0x%x)\n",
				sys_rev, ds_det, mbox_get_value(info_val));
	} else {
		mif_info("non-DT project, can't set system_rev\n");
	}

	return 0;
}

static int ss310ap_on(struct modem_ctl *mc)
{
	int ret;
	int cp_active = mbox_get_value(mc->mbx_phone_active);
	int cp_status = mbox_get_value(mc->mbx_cp_status);
#ifdef CONFIG_SOC_EXYNOS8890
	void __iomem *base = shm_get_ipc_region();
#endif
#ifdef CONFIG_SOC_EXYNOS8890
	u32 __maybe_unused et_dac_cal;
	register u64 reg0 __asm__("x0");
	register u64 reg1 __asm__("x1");
	register u64 reg2 __asm__("x2");
	register u64 reg3 __asm__("x3");
#endif

	mif_err("+++\n");
	mif_err("cp_active:%d cp_status:%d\n", cp_active, cp_status);

	/* Enable debug Snapshot */
	mif_set_snapshot(true);

	mc->phone_state = STATE_OFFLINE;

	if (init_mailbox_regs(mc))
		mif_err("Failed to initialize mbox regs\n");

#ifdef CONFIG_SOC_EXYNOS8890
	/* Transfer DRAM CAL data to CP */
	memmove(base + 0x1000, mc->sysram_alive, 0x800);

	/* Transfer ET_DAC_CAL tuning bit to CP */
	reg0 = 0;
	reg1 = 0;
	reg2 = 0;
	reg3 = 0;

	/* SMC_ID = 0x82001014, CMD_ID = 0x2002, read_idx=0x1C */
	ret = exynos_smc(0x82001014, 0, 0x2002, 0x1C);
	__asm__ volatile(
		"\t"
		: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	);
	et_dac_cal = (uint32_t)((0x00000000FFFF0000 & reg2) >> 16);

	mif_err("ret = 0x%x, et_cac_cal value = 0x%x\n", ret, et_dac_cal);
	mbox_set_value(mc->mbx_et_dac_cal,(u32)et_dac_cal);
#endif
	mbox_set_value(mc->mbx_pda_active, 1);

	if (exynos_get_cp_power_status(mc) > 0) {
		mif_err("CP aleady Power on, Just start!\n");
		exynos_cp_release(mc);
	} else {
		exynos_set_cp_power_onoff(mc, CP_POWER_ON);
	}

	msleep(300);
	ret = change_cp_pmu_manual_reset();
	mif_err("change_mr_reset -> %d\n", ret);

	mif_err("---\n");
	return 0;
}

static int ss310ap_off(struct modem_ctl *mc)
{
	mif_err("+++\n");

	exynos_set_cp_power_onoff(mc, CP_POWER_OFF);

	mif_err("---\n");
	return 0;
}

static int ss310ap_shutdown(struct modem_ctl *mc)
{
	struct io_device *iod;
	unsigned long timeout = msecs_to_jiffies(3000);
	unsigned long remain;

	mif_err("+++\n");

	if (mc->phone_state == STATE_OFFLINE
		|| exynos_get_cp_power_status(mc) <= 0)
		goto exit;

	init_completion(&mc->off_cmpl);
	remain = wait_for_completion_timeout(&mc->off_cmpl, timeout);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		mc->phone_state = STATE_OFFLINE;
		list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
			if (iod && atomic_read(&iod->opened) > 0)
				iod->modem_state_changed(iod, STATE_OFFLINE);
		}
	}

exit:
	exynos_set_cp_power_onoff(mc, CP_POWER_OFF);
	mif_err("---\n");
	return 0;
}

static int ss310ap_reset(struct modem_ctl *mc)
{
	void __iomem *base = shm_get_ipc_region();
	mif_err("+++\n");

	/* mc->phone_state = STATE_OFFLINE; */

	/* FIXME: For CP debug */
	if (*(unsigned int *)(base + 0xF80) == 0xDEB)
		return 0;

	if (exynos_get_cp_power_status(mc) > 0) {
		mif_err("CP aleady Power on, try reset\n");
		exynos_cp_reset(mc);
	}

	mif_err("---\n");
	return 0;
}

static int ss310ap_boot_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	struct io_device *iod;
	int cnt = 100;

	mif_info("+++\n");

	if (ld->boot_on)
		ld->boot_on(ld, mc->bootd);

	init_completion(&mc->init_cmpl);
	init_completion(&mc->off_cmpl);

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, STATE_BOOTING);
	}

	while (mbox_get_value(mc->mbx_cp_status) == 0) {
		if (--cnt > 0)
			usleep_range(10000, 20000);
		else
			return -EACCES;
	}

	mif_disable_irq(&mc->irq_cp_wdt);
	mif_enable_irq(&mc->irq_cp_fail);

	mif_info("cp_status=%u\n", mbox_get_value(mc->mbx_cp_status));

	mif_info("---\n");
	return 0;
}

static int ss310ap_boot_off(struct modem_ctl *mc)
{
	struct io_device *iod;
	unsigned long remain;
	int err = 0;
	mif_info("+++\n");

	remain = wait_for_completion_timeout(&mc->init_cmpl, MIF_INIT_TIMEOUT);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		err = -EAGAIN;
		goto exit;
	}

	mif_enable_irq(&mc->irq_cp_wdt);

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, STATE_ONLINE);
	}

	mif_info("---\n");

exit:
	mif_disable_irq(&mc->irq_cp_fail);
	return err;
}

static int ss310ap_boot_done(struct modem_ctl *mc)
{
	mif_info("+++\n");
	mif_info("---\n");
	return 0;
}

static int ss310ap_force_crash_exit(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);
	mif_err("+++\n");

	/* Make DUMP start */
	ld->force_dump(ld, mc->bootd);

	mif_err("---\n");
	return 0;
}

int ss310ap_force_crash_exit_ext(void)
{
	if (g_mc)
		ss310ap_force_crash_exit(g_mc);

	return 0;
}

static int ss310ap_dump_start(struct modem_ctl *mc)
{
	int err;
	struct link_device *ld = get_current_link(mc->bootd);
	mif_err("+++\n");

	if (!ld->dump_start) {
		mif_err("ERR! %s->dump_start not exist\n", ld->name);
		return -EFAULT;
	}

	err = ld->dump_start(ld, mc->bootd);
	if (err)
		return err;

	exynos_cp_release(mc);

	mbox_set_value(mc->mbx_ap_status, 1);

	mif_err("---\n");
	return err;
}

static void ss310ap_modem_boot_confirm(struct modem_ctl *mc)
{
	mbox_set_value(mc->mbx_ap_status, 1);
	mif_info("ap_status=%u\n", mbox_get_value(mc->mbx_ap_status));
}

static void ss310ap_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = ss310ap_on;
	mc->ops.modem_off = ss310ap_off;
	mc->ops.modem_shutdown = ss310ap_shutdown;
	mc->ops.modem_reset = ss310ap_reset;
	mc->ops.modem_boot_on = ss310ap_boot_on;
	mc->ops.modem_boot_off = ss310ap_boot_off;
	mc->ops.modem_boot_done = ss310ap_boot_done;
	mc->ops.modem_force_crash_exit = ss310ap_force_crash_exit;
	mc->ops.modem_dump_start = ss310ap_dump_start;
	mc->ops.modem_boot_confirm = ss310ap_modem_boot_confirm;
}

static void ss310ap_get_pdata(struct modem_ctl *mc, struct modem_data *modem)
{
	struct modem_mbox *mbx = modem->mbx;

	mc->mbx_pda_active = mbx->mbx_ap2cp_active;
	mc->int_pda_active = mbx->int_ap2cp_active;

	mc->mbx_phone_active = mbx->mbx_cp2ap_active;
	mc->irq_phone_active = mbx->irq_cp2ap_active;

	mc->mbx_ap_status = mbx->mbx_ap2cp_status;
	mc->mbx_cp_status = mbx->mbx_cp2ap_status;

	mc->mbx_perf_req = mbx->mbx_cp2ap_perf_req;
	mc->irq_perf_req = mbx->irq_cp2ap_perf_req;

	mc->mbx_et_dac_cal = mbx->mbx_ap2cp_et_dac_cal;
}

#ifdef CONFIG_EXYNOS_BUSMONITOR
static int ss310ap_busmon_notifier(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct busmon_notifier *info = (struct busmon_notifier *)data;
	char *init_desc = info->init_desc;

	if (init_desc != NULL &&
		(strncmp(init_desc, "CP", strlen(init_desc)) == 0 ||
		strncmp(init_desc, "APB_CORE_CP", strlen(init_desc)) == 0 ||
		strncmp(init_desc, "MIF_CP", strlen(init_desc)) == 0)) {
		struct modem_ctl *mc =
			container_of(nb, struct modem_ctl, busmon_nfb);

		ss310ap_force_crash_exit(mc);
	}
	return 0;
}
#endif

int ss310ap_init_modemctl_device(struct modem_ctl *mc, struct modem_data *pdata)
{
	struct platform_device *pdev = to_platform_device(mc->dev);
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	unsigned int irq_num;
	struct resource *sysram_alive;
	unsigned long flags = IRQF_NO_SUSPEND | IRQF_NO_THREAD;
	unsigned int cp_rst_n ;

	mif_err("+++\n");

	g_mc = mc;

	ss310ap_get_ops(mc);
	ss310ap_get_pdata(mc, pdata);
	dev_set_drvdata(mc->dev, mc);

	/*
	** Register CP_FAIL interrupt handler
	*/
	irq_num = platform_get_irq(pdev, 0);
	mif_init_irq(&mc->irq_cp_fail, irq_num, "cp_fail", flags);

	ret = mif_request_irq(&mc->irq_cp_fail, cp_fail_handler, mc);
	if (ret)	{
		mif_err("Failed to request_irq with(%d)", ret);
		return ret;
	}

	/* CP_FAIL interrupt must be enabled only during CP booting */
	mc->irq_cp_fail.active = true;
	mif_disable_irq(&mc->irq_cp_fail);

	/*
	** Register CP_WDT interrupt handler
	*/
	irq_num = platform_get_irq(pdev, 1);
	mif_init_irq(&mc->irq_cp_wdt, irq_num, "cp_wdt", flags);

	ret = mif_request_irq(&mc->irq_cp_wdt, cp_wdt_handler, mc);
	if (ret) {
		mif_err("Failed to request_irq with(%d)", ret);
		return ret;
	}

	/* CP_WDT interrupt must be enabled only after CP booting */
	mc->irq_cp_wdt.active = true;
	mif_disable_irq(&mc->irq_cp_wdt);

	sysram_alive = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mc->sysram_alive = devm_ioremap_resource(&pdev->dev, sysram_alive);
	if (IS_ERR(mc->sysram_alive)) {
		ret = PTR_ERR(mc->sysram_alive);
		return ret;
	}

	/*
	** Register LTE_ACTIVE MBOX interrupt handler
	*/
	ret = mbox_request_irq(mc->irq_phone_active, cp_active_handler, mc);
	if (ret) {
		mif_err("Failed to mbox_request_irq %u with(%d)",
				mc->irq_phone_active, ret);
		return ret;
	}

	init_completion(&mc->off_cmpl);

	/*
	** Get/set CP_RST_N
	*/
	if (np)	{
		cp_rst_n = of_get_named_gpio(np, "modem_ctrl,gpio_cp_rst_n", 0);
		if (gpio_is_valid(cp_rst_n)) {
			mif_err("cp_rst_n: %d\n", cp_rst_n);
			ret = gpio_request(cp_rst_n, "CP_RST_N");
			if (ret)	{
				mif_err("fail req gpio %s:%d\n", "CP_RST_N", ret);
				return -ENODEV;
			}

			gpio_direction_output(cp_rst_n, 1);
		} else {
			mif_err("cp_rst_n: Invalied gpio pins\n");
		}
	} else {
		mif_err("cannot find device_tree for pmu_cu!\n");
		return -ENODEV;
	}

#ifdef CONFIG_EXYNOS_BUSMONITOR
	/*
	 ** Register BUS Mon notifier
	 */
	mc->busmon_nfb.notifier_call = ss310ap_busmon_notifier;
	busmon_notifier_chain_register(&mc->busmon_nfb);
#endif

	mif_err("---\n");
	return 0;
}
