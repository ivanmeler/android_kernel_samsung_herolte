/* sound/soc/samsung/lpeffwork.c
 *
 * Exynos Seiren DMA driver for Exynos Audio Subsystem
 *
 * Copyright (c) 2015 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/io.h>
#include <asm/cacheflush.h>

#include <sound/exynos.h>
#include <sound/soc.h>

#include "../lpass.h"
#include "seiren.h"
#include "seiren_ioctl.h"
#include "lpeffwork.h"
#include "../esa_sa_effect.h"

#define MAX_EFFCMD_NUM		10

#define LPEFF_DEBUG_HEADER	"[lpeff]"
#define lpeff_prinfo(s, ...)	pr_info( LPEFF_DEBUG_HEADER s, ##__VA_ARGS__ )
#define lpeff_prerr(s, ...)	pr_err( LPEFF_DEBUG_HEADER s, ##__VA_ARGS__ )

extern struct mutex esa_mutex;
extern int check_esa_compr_state(void);

struct lpeff_workstruct {
	struct task_struct *g_th_id;
	wait_queue_head_t msg_wait;
	spinlock_t slock;
	int msg_rdidx;
	int msg_wridx;
	int msg_count;
	enum exynos_effwork_cmd msg_queue[MAX_EFFCMD_NUM];
};
static struct lpeff_workstruct g_worker_data;

void __iomem *g_effect_addr = NULL;
char *lpeff_fw_lib_entry = NULL;

static struct seiren_info g_si;

void lpeff_set_effect_addr(void __iomem *effect_ram)
{
	g_effect_addr = effect_ram;
}

int lpeff_log(char *str)
{
	lpeff_prinfo("[LIB] %s", str);
	return 0;
}

int lpeff_memcpy(void * dest, const void *src, size_t n)
{
	memcpy(dest, src, n);
	return 0;
}

int lpeff_alloc(char *str)
{
	lpeff_prinfo("[LIB] %s", str);
	return 0;
}

static ssize_t lpeff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 elpe_cmd;
	u32 arg0, arg1, arg2, arg3, arg4, arg5, arg6;
	u32 arg7, arg8, arg9, arg10, arg11, arg12;

	if (!check_esa_compr_state())
		return 1;

	mutex_lock(&esa_mutex);
	if (pm_runtime_get_sync(&g_si.pdev->dev) < 0) {
		mutex_unlock(&esa_mutex);
		return 1;
	}

	if (g_effect_addr) {
		elpe_cmd = readl(g_effect_addr + ELPE_BASE + ELPE_CMD);
		arg0 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG0);
		arg1 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG1);
		arg2 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG2);
		arg3 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG3);
		arg4 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG4);
		arg5 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG5);
		arg6 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG6);
		arg7 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG7);
		arg8 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG8);
		arg9 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG9);
		arg10 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG10);
		arg11 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG11);
		arg12 = readl(g_effect_addr + ELPE_BASE + ELPE_ARG12);
	}

	/* change src, dst address to offset value */
#if 0
	src = (src & 0xFFFFFF) - 0x400000;
	dst = (dst & 0xFFFFFF) - 0x400000;
#endif

	pm_runtime_mark_last_busy(&g_si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&g_si.pdev->dev);
	mutex_unlock(&esa_mutex);

//	flush_cache_all();

	return scnprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			elpe_cmd, arg0, arg1, arg2, arg3, arg4, arg5, arg6,
			arg7, arg8, arg9, arg10, arg11, arg12);
}

static DEVICE_ATTR(lpeff, S_IRUGO, lpeff_show, NULL);

int lpeff_init(struct seiren_info *si)
{
	int ret = 0;

	memcpy(&g_si, si, sizeof(struct seiren_info));
#ifndef CONFIG_SOC_EXYNOS8890
	g_effect_addr = si->effect_ram;
	printk("g_effect_addr = 0x%p \n", g_effect_addr);
#endif

	ret = device_create_file(&g_si.pdev->dev, &dev_attr_lpeff);
	if (ret) {
		lpeff_prerr("failed to create lpeff file\n");
		return ret;
	}

	return 0;
}

void queue_lpeff_cmd(enum exynos_effwork_cmd msg)
{
	spin_lock(&g_worker_data.slock);

	g_worker_data.msg_queue[g_worker_data.msg_wridx] = msg;
	g_worker_data.msg_wridx++;
	g_worker_data.msg_wridx = g_worker_data.msg_wridx % MAX_EFFCMD_NUM;
	g_worker_data.msg_count++;

	spin_unlock(&g_worker_data.slock);
	wake_up(&g_worker_data.msg_wait);
}

static enum exynos_effwork_cmd lpeff_worker_dequeue(void)
{
	unsigned long flags;
	enum exynos_effwork_cmd msg = -1;

	spin_lock_irqsave(&g_worker_data.slock, flags);

	if(g_worker_data.msg_count == 0) {
		spin_unlock_irqrestore(&g_worker_data.slock, flags);
		wait_event(g_worker_data.msg_wait,(g_worker_data.msg_count>0));
		spin_lock_irqsave(&g_worker_data.slock, flags);
	}

	msg = g_worker_data.msg_queue[g_worker_data.msg_rdidx];
	g_worker_data.msg_rdidx++;
	g_worker_data.msg_rdidx = g_worker_data.msg_rdidx % MAX_EFFCMD_NUM;
	g_worker_data.msg_count--;

	spin_unlock_irqrestore(&g_worker_data.slock, flags);
	return msg;
}

void lpeff_send_effect_cmd(void)
{
	sysfs_notify(&g_si.pdev->dev.kobj, NULL, "lpeff");
}

static int lpeff_worker_func(void *arg)
{
	while(!kthread_should_stop()) {
		enum exynos_effwork_cmd msg = lpeff_worker_dequeue();
		switch(msg) {
		case LPEFF_EFFECT_CMD:
			lpeff_send_effect_cmd();
			break;
		case LPEFF_REVERB_CMD:
			break;
		case LPEFF_DEBUG_CMD:
			break;
		case LPEFF_EXIT_CMD:
			break;
		default:
			lpeff_prinfo("[lpeff] invalid command (%d)\n", msg);
		}
	}
	return 0;
}

void exynos_init_lpeffworker(void)
{
	memset(&g_worker_data, 0, sizeof(g_worker_data));

	g_worker_data.g_th_id = NULL;
	g_worker_data.msg_rdidx = 0;
	g_worker_data.msg_wridx = 0;
	g_worker_data.msg_count = 0;

	init_waitqueue_head(&g_worker_data.msg_wait);
	spin_lock_init(&g_worker_data.slock);

	if(g_worker_data.g_th_id == NULL)
		g_worker_data.g_th_id =
			(struct task_struct *)kthread_run(lpeff_worker_func,
					NULL, "lpeff_worker");
}

void exynos_lpeff_finalize(void)
{
	queue_lpeff_cmd(LPEFF_EXIT_CMD);
	if(g_worker_data.g_th_id){
		kthread_stop(g_worker_data.g_th_id);
		g_worker_data.g_th_id = NULL;
	}
	device_remove_file(&g_si.pdev->dev, &dev_attr_lpeff);
}

MODULE_AUTHOR("Donggyun Ko <donggyun.ko@samsung.com>");
MODULE_AUTHOR("Hyoungmin Seo <hmseo@samsung.com>");
MODULE_DESCRIPTION("Exynos Seiren LPEFF Driver");
MODULE_LICENSE("GPL");
