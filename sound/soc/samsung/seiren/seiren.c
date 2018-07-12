/* sound/soc/samsung/seiren/seiren.c
 *
 * Exynos Seiren Audio driver for Exynos5430
 *
 * Copyright (c) 2013 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/serio.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/firmware.h>
#include <linux/kthread.h>
#include <linux/exynos_ion.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/file.h>

#include <asm/cacheflush.h>
#include <asm/cachetype.h>
#include <asm/tlbflush.h>

#include <sound/exynos.h>
#include <sound/soc.h>

#if 0
#include <plat/map-base.h>
#include <plat/map-s5p.h>
#endif

#include "../lpass.h"
#include "../eax.h"
#include "seiren.h"
#include "seiren-dma.h"
#include "seiren_ioctl.h"
#include "seiren_error.h"
#ifdef CONFIG_SND_SAMSUNG_ELPE
#include "lpeffwork.h"
#endif

#ifdef CONFIG_SND_ESA_SA_EFFECT
#include "../esa_sa_effect.h"
#endif

#ifdef CONFIG_PM_DEVFREQ
#ifdef CONFIG_SOC_EXYNOS5430
#define CA5_MIF_FREQ_NORM	(0)
#define CA5_MIF_FREQ_HIGH	(317000)
#else
#define CA5_MIF_FREQ_NORM	(0)
#define CA5_MIF_FREQ_HIGH	(317000)
#endif
#define CA5_MIF_FREQ_BOOST	(413000)
#define CA5_INT_FREQ_BOOST	(200000)
#endif

#ifdef CONFIG_SOC_EXYNOS8890
#define OFFLOAD_INT_LOCK_FREQ 255000
#endif

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

void __iomem *dma_reg = NULL;

DEFINE_MUTEX(esa_mutex);
static DECLARE_WAIT_QUEUE_HEAD(esa_wq);
static DECLARE_WAIT_QUEUE_HEAD(esa_fx_wq);
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static DECLARE_WAIT_QUEUE_HEAD(esa_cpu_lock_wq);
#endif

static struct seiren_info si;

static int esa_send_cmd_(u32 cmd_code, bool sram_only);
static irqreturn_t esa_isr(int irqno, void *id);

static DEFINE_RAW_SPINLOCK(esa_logbuf_lock);
int header_printed;
int start, end;
int end_index;

#define esa_send_cmd_sram(cmd)	esa_send_cmd_(cmd, true)
#define esa_send_cmd(cmd)	esa_send_cmd_(cmd, false)

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
int esa_compr_running(void);
static void esa_fw_snapshot(void);
#endif

int check_esa_status(void)
{
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	return si.fw_use_dram ? 1 : 0;
#else
	return esa_compr_running();
#endif
}

void esa_memset_mailbox(void)
{
	int i;

	for (i = 0; i < 0x80; i += 0x4)
		writel(0x0, si.mailbox + i);
}

void esa_memcpy_mailbox(bool save)
{
	unsigned int *src;
	unsigned int *dst;
	int i;

	if (save) {
		src = si.mailbox;
		dst = si.mailbox_bak;
		for (i = 0; i < 0x80; i += 0x4, dst++)
			*dst = readl(src + i);
	} else {
		src = si.mailbox_bak;
		dst = si.mailbox;
		for (i = 0; i < 0x80; i += 0x4, src++)
			writel(*src, dst + i);
	}
}

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static int esa_set_cpu_lock_kthread(void *arg)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible(esa_cpu_lock_wq, si.set_cpu_lock);
		si.set_cpu_lock = false;
		lpass_set_cpu_lock(si.cpu_lock_level);
	}

	return 0;
}

static struct audio_processor *ptr_ap;
static void esa_dump_fw_log(void);

int esa_compr_send_direct_cmd(int32_t cmd)
{
	int n, ack;

	switch (cmd) {
	case CMD_DMA_START:
		esa_info("%s: CMD_COMPR_DMA_START %d\n", __func__, cmd);
		break;
	case CMD_DMA_STOP:
		esa_info("%s: CMD_COMPR_DMA_STOP %d\n", __func__, cmd);
		break;
	default:
		esa_err("%s: unknown cmd %d\n", __func__, cmd);
		return -EIO;
	}

	spin_lock(&si.cmd_lock);
	writel(2, si.mailbox + COMPR_HANDLE_ID);
	writel(cmd, si.mailbox + COMPR_CMD_CODE);		/* command */
	writel(1, si.regs + SW_INTR_CA5);		/* trigger ca5 */

	for (n = 0, ack = 0; n < 2000; n++) {
		if (readl(si.mailbox + COMPR_ACK)) {	/* Wait for ACK */
			ack = 1;
			break;
		}
		udelay(100);
	}
	writel(0, si.mailbox + COMPR_ACK);		/* clear ACK */

	spin_unlock(&si.cmd_lock);

	if (!ack) {
		esa_err("%s: No ack error!(%x)", __func__, cmd);
		esa_dump_fw_log();
		return -EFAULT;
	}

	return 0;
}

int esa_compr_send_cmd(int32_t cmd, struct audio_processor* ap)
{
	int n, ack;

	switch (cmd) {
	case CMD_COMPR_CREATE:
		esa_info("%s: CMD_COMPR_CREATE %d\n", __func__, cmd);
		break;
	case CMD_COMPR_DESTROY:
		esa_info("%s: CMD_COMPR_DESTROY %d\n", __func__, cmd);
		break;
	case CMD_COMPR_SET_PARAM:
		esa_info("%s: CMD_COMPR_SET_PARAM %d\n", __func__, cmd);
#ifdef CONFIG_SND_ESA_SA_EFFECT
		spin_lock(&si.cmd_lock);
		writel(si.out_sample_rate, si.mailbox + COMPR_PARAM_RATE);
		writel(si.left_vol, si.mailbox + COMPR_LEFT_VOL);
		writel(si.right_vol, si.mailbox + COMPR_RIGHT_VOL);
		spin_unlock(&si.cmd_lock);
#endif
		break;
	case CMD_COMPR_WRITE:
		esa_debug("%s: CMD_COMPR_WRITE %d\n", __func__, cmd);
		break;
	case CMD_COMPR_READ:
		esa_debug("%s: CMD_COMPR_READ %d\n", __func__, cmd);
		break;
	case CMD_COMPR_START:
		esa_info("%s: CMD_COMPR_START %d\n", __func__, cmd);
		break;
	case CMD_COMPR_STOP:
		esa_info("%s: CMD_COMPR_STOP %d\n", __func__, cmd);
		break;
	case CMD_COMPR_PAUSE:
		esa_info("%s: CMD_COMPR_PAUSE %d\n", __func__, cmd);
		break;
	case CMD_COMPR_EOS:
		esa_info("%s: CMD_COMPR_EOS %d\n", __func__, cmd);
		break;
	case CMD_COMPR_GET_VOLUME:
		esa_debug("%s: CMD_COMPR_GET_VOLUME %d\n", __func__, cmd);
		break;
	case CMD_COMPR_SET_VOLUME:
		esa_debug("%s: CMD_COMPR_SET_VOLUME %d\n", __func__, cmd);
		break;
	case CMD_COMPR_CA5_WAKEUP:
		esa_debug("%s: CMD_COMPR_CA5_WAKEUP %d\n", __func__, cmd);
		break;
	case CMD_COMPR_HPDET_NOTIFY:
		esa_debug("%s: CMD_COMPR_HPDET_NOTIFY %d\n", __func__, cmd);
		break;
	default:
		esa_err("%s: unknown cmd %d\n", __func__, cmd);
		return -EIO;
	}

	spin_lock(&si.cmd_lock);
	writel(ap->handle_id, si.mailbox + COMPR_HANDLE_ID);
	writel(cmd, si.mailbox + COMPR_CMD_CODE);		/* command */
	writel(1, si.regs + SW_INTR_CA5);		/* trigger ca5 */

	for (n = 0, ack = 0; n < 2000; n++) {
		if (readl(ap->reg_ack)) {	/* Wait for ACK */
			ack = 1;
			break;
		}
		udelay(100);
	}
	writel(0, ap->reg_ack);		/* clear ACK */

	spin_unlock(&si.cmd_lock);

	if (!ack) {
		esa_err("%s: No ack error!(%x)", __func__, cmd);
		esa_dump_fw_log();
		return -EFAULT;
	}

	return 0;
}

int esa_compr_send_buffer(const size_t copy_size, struct audio_processor *ap)
{
	int ret;

	/* write mp3 data to firmware */
	spin_lock(&si.compr_lock);
	writel(copy_size, si.mailbox + COMPR_SIZE_OF_FRAGMENT);

	ret = esa_compr_send_cmd(CMD_COMPR_WRITE, ap);
	if (ret) {
		esa_err("%s: can't send CMD_COMPR_WRITE (%d)\n",
			__func__, ret);
		spin_unlock(&si.compr_lock);
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		esa_fw_snapshot();
#endif
		return ret;
	}
	spin_unlock(&si.compr_lock);

	return 0;
}

void __iomem *esa_compr_get_mem(void)
{
	return si.mailbox;
}

u32 esa_compr_pcm_size(void)
{
	/* update frame count(dma buffer) */
	return readl(si.mailbox + COMPR_RENDERED_PCM_SIZE);
}

int esa_compr_set_param(struct audio_processor* ap, uint8_t **buffer)
{
	unsigned int ip_type;
	unsigned char *ibuf;
	u32 ibuf_ca5_pa;
	u32 ibuf_offset;
	int ret;

	ptr_ap = ap;
	/* initialize in buffer */
	/* use free area in dram */
	ibuf = si.fwarea[1];
	ap->block_num = 1;

	/* calculate the physical address */
	ibuf_offset = ibuf - si.fwarea[ap->block_num];
	ibuf_ca5_pa = ibuf_offset + FWAREA_SIZE * ap->block_num;

	/* set buffer information at mailbox */
	spin_lock(&si.lock);
	writel(ap->buffer_size, si.mailbox + COMPR_SIZE_OF_INBUF);
	writel(ibuf_ca5_pa, si.mailbox + COMPR_PHY_ADDR_INBUF);
	writel(ap->sample_rate, si.mailbox + COMPR_PARAM_SAMPLE);
	writel(ap->num_channels, si.mailbox + COMPR_PARAM_CH);

	ip_type = ap->codec_id << 16;
	writel(ip_type, si.mailbox + COMPR_IP_TYPE);
	spin_unlock(&si.lock);

	si.isr_compr_created = 0;
	ret = esa_compr_send_cmd(CMD_COMPR_SET_PARAM, ap);
	if (ret) {
		esa_err("%s: can't send CMD_COMPR_SET_PARAM (%d)\n",
			__func__, ret);
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		esa_fw_snapshot();
#endif
		return ret;
	}

	/* wait until the parameter is set up */
	ret = wait_event_interruptible_timeout(esa_wq, si.isr_compr_created, HZ * 2);
	if (!ret) {
		esa_err("%s: compress set param timed out!!! (%d)\n",
			__func__, ret);
		writel(0, si.mailbox + COMPR_INTR_ACK);
		esa_dump_fw_log();
		si.fw_use_dram = true;
		ptr_ap = NULL;
		return -EBUSY;
	}

	/* created instance */
	ap->handle_id = readl(si.mailbox + COMPR_IP_ID);
	esa_info("%s: codec id:0x%x, ret_val:0x%x, handle_id:0x%x\n",
		__func__, (unsigned int)ap->codec_id,
		readl(si.mailbox + COMPR_RETURN_CMD),
		(unsigned int)ap->handle_id);

	/* return the buffer address for caller */
	*buffer = ibuf;
	esa_info("%s: allocated buffer address (0x%p), size(0x%x)\n",
		__func__, *buffer, ap->buffer_size);
#ifdef CONFIG_SND_ESA_SA_EFFECT
	si.effect_on = false;
#endif
	return 0;
}

int esa_compr_running(void)
{
	return (si.isr_compr_created ? true : false);
}

void esa_compr_set_state(bool flag)
{
	si.is_compr_open = flag;
	esa_debug("%s Compress-State %s\n", __func__,
			(si.is_compr_open ? "OPENED" : "CLOSED"));
	return;
}

int check_esa_compr_state(void)
{
	return (si.is_compr_open ? true : false);
}

/* Notify firmware when alpa is enter and exit
 * through mailbox interface */
void esa_compr_alpa_notifier(bool on)
{
	if (si.mailbox == NULL)
		return;
	writel(on ? 1 : 0, si.mailbox + COMPR_ALPA_NOTI);
	esa_debug("%s ALPA %s\n", __func__, (on ? "Enter" : "Exit"));
	if (!on) {
		/* Send software interrupt to CA5 to wakeup */
		if (ptr_ap) {
			if (esa_compr_send_cmd(CMD_COMPR_CA5_WAKEUP, ptr_ap))
				esa_err("%s Unable to Send CA5 Wakeup command\n",
					       __func__);
		}
	}

	return;
}

/* HP Detect notification command to FW */
void esa_compr_hpdet_notifier(bool on)
{
	esa_info("%s %s\n", __func__, (on ? "Jack Detected" : "Jack Removed"));
	if (on) {
		/* Send HP detect notification command */
		if (ptr_ap) {
			if (esa_compr_send_cmd(CMD_COMPR_HPDET_NOTIFY, ptr_ap))
				esa_err("%s Unable to Send HPDET_NOTIFY command\n",
					       __func__);
		}
	}

	return;
}

void esa_compr_ctrl_fxintr(bool fxon)
{
	writel(fxon ? 1 : 0, si.mailbox + EFFECT_EXT_ON);
	return;
}

#ifdef CONFIG_SND_ESA_SA_EFFECT
void esa_compr_save_volume(int left, int right)
{
	esa_debug("%s output volume l = %d, r = %d \n",
			__func__, left, right);
	si.left_vol = left;
	si.right_vol = right;
	return;
}

void esa_compr_set_sample_rate(u32 rate)
{
	esa_debug("%s output sample_rate %d \n", __func__, rate);
	si.out_sample_rate = rate;
	return;
}

u32 esa_compr_get_sample_rate(void)
{
	return si.out_sample_rate;
}
#endif

void esa_fw_start(void)
{
	pm_runtime_get_sync(&si.pdev->dev);
#ifdef ESA_DATA_DUMP
	if (!sram_filp)
		sram_filp =
		filp_open("/data/sram.bin", O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
#endif
}

void esa_fw_stop(void)
{
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
}

int esa_compr_open(void)
{
	int ret;
	void __iomem	*cmu_reg;
	u32 cfg;

	pm_qos_update_request(&si.ca5_int_qos, OFFLOAD_INT_LOCK_FREQ);
	ret = pm_runtime_get_sync(&si.pdev->dev);
	if (ret < 0) {
		pm_qos_update_request(&si.ca5_int_qos, 0);
		return ret;
	}

	cmu_reg = ioremap(0x114C0000, SZ_4K);
	cfg = readl(cmu_reg + 0x404) & ~(0xF);
	cfg |= 0x2; /* ACLK_AUD: 102.5MHz */
	writel(cfg, cmu_reg + 0x404);

	cfg = readl(cmu_reg + 0x400) & ~(0xF); /* CA5: 410MHz */
	writel(cfg, cmu_reg + 0x400);
	iounmap(cmu_reg);

	return 0;
}

void esa_compr_close(void)
{
	void __iomem	*cmu_reg;
	u32 cfg;

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	lpass_set_cpu_lock(0);
#endif
	cmu_reg = ioremap(0x114C0000, SZ_4K);
	cfg = readl(cmu_reg + 0x400) & ~(0xF);
	cfg |= 0x1; /* CA5: 205MHz */
	writel(cfg, cmu_reg + 0x400);

	cfg = readl(cmu_reg + 0x404) & ~(0xF);
	cfg |= 0x1; /* ACLK_AUD: 102.5MHz */
	writel(cfg, cmu_reg + 0x404);
	iounmap(cmu_reg);

	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	pm_qos_update_request(&si.ca5_int_qos, 0);
	ptr_ap = NULL;
#ifdef CONFIG_SND_ESA_SA_EFFECT
	si.out_sample_rate = 0;
#endif
}
#endif

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static void esa_fw_snapshot(void)
{
#if 0
        struct file *sram_filp;
        struct file *dram_filp;
        mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (si.sram) {
		memcpy(si.fwmem_sram_bak, si.sram, SRAM_FW_MAX);
	} else {
		esa_err("%s : Can't store seiren firmware\n", __func__);
		set_fs(old_fs);
		return;
	}

	sram_filp = filp_open("/data/seiren_fw_sram_snapsot.bin",
			O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	vfs_write(sram_filp, si.fwmem_sram_bak, SRAM_FW_MAX, &sram_filp->f_pos);
	vfs_fsync(sram_filp, 0);
	filp_close(sram_filp, NULL);

	dram_filp = filp_open("/data/seiren_fw_dram_snapsot.bin",
			O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	vfs_write(dram_filp, si.fwarea[0], SZ_4M, &dram_filp->f_pos);
	vfs_fsync(dram_filp, 0);
	filp_close(dram_filp, NULL);

	set_fs(old_fs);
#endif
}

struct kobject *seiren_fw_snapshot_kobj = NULL;
static ssize_t snapshot_seiren_fw(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	pr_info("%s: %d seiren snapshot is called\n", __func__, __LINE__);
	esa_fw_snapshot();

	return count;
}

static struct kobj_attribute seiren_fw_dump_attribute =
	__ATTR(seiren_fw_dump, S_IRUGO | S_IWUSR, NULL, snapshot_seiren_fw);
#endif

static void esa_dump_fw_log(void)
{
	char log[256];
	char *addr = si.fw_log_buf;
	int n, fwver;

	esa_info("fw log:\n");
	esa_info("running cnt:%d\n", readl(si.mailbox + COMPR_CHECK_RUNNING));
	fwver = readl(si.mailbox + VIRSION_ID);
	esa_info("Firmware Version: %c%c%c-%c\n",
		(fwver >> 24), ((fwver >> 16) & 0xFF),
		((fwver >> 8) & 0xFF), fwver & 0xFF);
	esa_info("ack status: lpcm(%d) compr(%d:%x)\n",
			readl(si.mailbox + COMPR_INTR_DMA_ACK),
			readl(si.mailbox + COMPR_INTR_ACK),
			readl(si.mailbox + COMPR_CHECK_CMD));

	if (addr) {
		for (n = 0; n < FW_LOG_LINE; n++, addr += 128) {
			memcpy(log, addr, 128);
			esa_info("%s", log);
		}
	}
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	if (si.sram) {
		memcpy(si.fwmem_sram_bak, si.sram, SRAM_FW_MAX);
	} else {
		esa_err("%s: Can't store seiren firmware\n", __func__);
	}
#endif
}

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static struct esa_rtd *esa_alloc_rtd(void)
{
	struct esa_rtd *rtd = NULL;
	int idx;

	rtd = kzalloc(sizeof(struct esa_rtd), GFP_KERNEL);

	if (rtd) {
		spin_lock(&si.lock);

		for (idx = 0; idx < INSTANCE_MAX; idx++) {
			if (!si.rtd_pool[idx]) {
				si.rtd_cnt++;
				si.rtd_pool[idx] = rtd;
				rtd->idx = idx;
				break;
			}
		}

		spin_unlock(&si.lock);

		if (idx == INSTANCE_MAX) {
			kfree(rtd);
			rtd = NULL;
		}
	}

	return rtd;
}

static void esa_free_rtd(struct esa_rtd *rtd)
{
	spin_lock(&si.lock);

	si.rtd_cnt--;
	si.rtd_pool[rtd->idx] = NULL;

	spin_unlock(&si.lock);

	kfree(rtd);
}

static int esa_send_cmd_exe(struct esa_rtd *rtd, unsigned char *ibuf,
				unsigned char *obuf, size_t size)
{
	u32 ibuf_ca5_pa, obuf_ca5_pa;
	u32 ibuf_offset, obuf_offset;
	int out_size;
	int response;

	if (rtd->use_sram) {	/* SRAM buffer */
		ibuf_offset = SRAM_IO_BUF + SRAM_IBUF_OFFSET;
		obuf_offset = SRAM_IO_BUF + SRAM_OBUF_OFFSET;
		ibuf_ca5_pa = ibuf_offset;
		obuf_ca5_pa = obuf_offset;
		if (si.sram) {
			memcpy(si.sram + ibuf_offset, ibuf, size);
		} else {
			esa_err("%s : there is no sram input buffer", __func__);
			return -ENOMEM;
		}
	} else {		/* DRAM buffer */
		ibuf_offset = ibuf - si.fwarea[rtd->block_num];
		obuf_offset = obuf - si.fwarea[rtd->block_num];
		ibuf_ca5_pa = ibuf_offset + FWAREA_SIZE * rtd->block_num;
		obuf_ca5_pa = obuf_offset + FWAREA_SIZE * rtd->block_num;
	}

	writel(rtd->handle_id, si.mailbox + HANDLE_ID);
	writel(size, si.mailbox + SIZE_OF_INDATA);
	writel(ibuf_ca5_pa, si.mailbox + PHY_ADDR_INBUF);
	writel(obuf_ca5_pa, si.mailbox + PHY_ADDR_OUTBUF);

	if (rtd->use_sram)
		esa_send_cmd_sram(CMD_EXE);
	else
		esa_send_cmd(CMD_EXE);

	/* check response of FW */
	response = readl(si.mailbox + RETURN_CMD);

	if (rtd->use_sram) {
		out_size = readl(si.mailbox + SIZE_OUT_DATA);
		if ((response == 0) && (out_size > 0)) {
			if (si.sram) {
				memcpy(obuf, si.sram + obuf_offset, out_size);
			} else {
				esa_err("%s : there is no sram output buffer", __func__);
				return -ENOMEM;
			}
		}
	}

	return response;
}
#endif

static void esa_fw_download(void)
{
	int n;

	esa_info("%s: fw size = sram(%d) dram(%d)\n", __func__,
			si.fw_sbin_size, si.fw_dbin_size);

	lpass_reset(LPASS_IP_CA5, LPASS_OP_RESET);
	udelay(20);

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	memset(si.mailbox, 0, 128);
#endif
	if (si.fw_suspended) {
		esa_info("%s: resume\n", __func__);
		/* Restore SRAM */
		if (si.sram) {
			memcpy(si.sram, si.fwmem_sram_bak, SRAM_FW_MEMSET_SIZE);
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
			memset(si.sram + FW_ZERO_SET_BASE, 0, FW_ZERO_SET_SIZE);
#else
			esa_memcpy_mailbox(false);
#endif
		} else {
			esa_err("%s: sram address is empty (%d)\n",
					__func__, __LINE__);
		}
	} else {
		esa_info("%s: intialize\n", __func__);
		for (n = 0; n < FWAREA_NUM; n++)
			memset(si.fwarea[n], 0, FWAREA_SIZE);

		esa_memset_mailbox();
		if (si.sram) {
			memset(si.sram, 0, SRAM_FW_MEMSET_SIZE); /* for ZI area */
			memcpy(si.sram, si.fwmem, si.fw_sbin_size);
		} else {
			esa_err("%s: sram address is empty (%d)\n",
					__func__, __LINE__);
		}
		memcpy(si.fwarea[0], si.fwmem + si.fw_sbin_size,
					si.fw_dbin_size);
	}

	lpass_reset(LPASS_IP_CA5, LPASS_OP_NORMAL);

	esa_info("%s: CA5 startup...\n", __func__);
}

static int esa_fw_startup(void)
{
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	unsigned int dec_ver;
#endif
	int ret;

	if (si.fw_ready)
		return 0;

	if (!si.fwmem_loaded) {
		lpass_update_lpclock(LPCLK_CTRLID_OFFLOAD, false);
		/* audio firmware hasn't beed loaded yet.
		   there is nothing to do */
		return 0;
	}

	/* Not to enter SICD_AUD */
	lpass_update_lpclock(LPCLK_CTRLID_LEGACY, true);
	lpass_mif_power_on();
	/* power on */
	si.fw_use_dram = true;
	esa_debug("Turn on CA5...\n");
	esa_fw_download();

	/* wait for fw ready */
	ret = wait_event_interruptible_timeout(esa_wq, si.fw_ready, HZ * 3);
	if (!ret) {
#ifdef CONFIG_SOC_EXYNOS8890
		u32 cfg;
		void __iomem	*cmu_reg;
		cmu_reg = ioremap(0x114C0000, SZ_4K);
		cfg = readl(cmu_reg + 0x800); /* Check CA5 clock */
		iounmap(cmu_reg);
		esa_err("%s: fw not ready!!! (%d), clk = %x\n", __func__,
			readl(si.mailbox + LAST_CHECKPT), cfg);
#else
		esa_err("%s: fw not ready!!! (%d)\n", __func__,
			readl(si.mailbox + LAST_CHECKPT));
#endif
		cfg = readl(si.sram); /* Save Reset Vector */
		writel(0xE320F003, si.sram); /* Enter CA5 into WFI */
		lpass_reset_toggle(LPASS_IP_CA5);
		if (!check_esa_compr_state())
			lpass_update_lpclock(LPCLK_CTRLID_OFFLOAD, false);
		udelay(100);
		writel(cfg, si.sram); /* Restore Reset Vector */
		lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
		si.fw_use_dram = false;
		return -EBUSY;
	}

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	/* check decoder version */
	esa_send_cmd(SYS_GET_STATUS);
	dec_ver = readl(si.mailbox) & 0xFF00;
	dec_ver = dec_ver >> 8;
	esa_debug("Decoder version : %x\n", dec_ver);
#endif
	lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
	return 0;
}

static void esa_fw_shutdown(void)
{
	u32 cnt, val;

	if (!si.fw_ready)
		return;

	if (!si.fwmem_loaded)
		return;

	/* Not to enter SICD_AUD */
	lpass_update_lpclock(LPCLK_CTRLID_LEGACY, true);
	/* SUSPEND & IDLE */
	esa_send_cmd(SYS_SUSPEND);

	si.fw_suspended = false;
	cnt = msecs_to_loops(100);
	while (--cnt) {
		val = readl(si.regs + CA5_STATUS);
		if (val & CA5_STATUS_WFI) {
			si.fw_suspended = true;
			break;
		}
		cpu_relax();
	}
	esa_debug("CA5_STATUS: %X\n", val);

	/* Backup SRAM */
	if (si.sram)
		memcpy(si.fwmem_sram_bak, si.sram, SRAM_FW_MEMSET_SIZE);
	else
		esa_err("%s : Failed to save seiren firmware\n", __func__);
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	esa_memcpy_mailbox(true);
#endif
	/* power off */
	esa_debug("Turn off CA5...\n");
	lpass_reset(LPASS_IP_CA5, LPASS_OP_RESET);
	si.fw_ready = false;
	si.fw_use_dram = false;
	lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
}

#if !defined(CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD) && defined(CONFIG_PM_DEVFREQ)
static bool esa_check_ip_exist(unsigned int ip_type)
{
	int idx;

	for (idx = 0; idx < INSTANCE_MAX; idx++) {
		if (si.rtd_pool[idx]) {
			if (si.rtd_pool[idx]->ip_type == ip_type)
				return true;
		}
	}

	return false;
}
#endif

static void esa_update_qos(void)
{
#if !defined(CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD) && defined(CONFIG_PM_DEVFREQ)
	int mif_qos_new;
#ifdef CONFIG_SND_ESA_SA_EFFECT
	int int_qos_new = 0;
#endif
	if (!si.fw_ready)
		mif_qos_new = 0;
	else if (esa_check_ip_exist(ADEC_AAC))
		mif_qos_new = CA5_MIF_FREQ_HIGH;
	else
		mif_qos_new = CA5_MIF_FREQ_NORM;

#ifdef CONFIG_SND_ESA_SA_EFFECT
	if (si.effect_on) {
		mif_qos_new = CA5_MIF_FREQ_BOOST;
		int_qos_new = CA5_INT_FREQ_BOOST;
	}

	if (si.int_qos != int_qos_new) {
		si.int_qos = int_qos_new;
		pm_qos_update_request(&si.ca5_int_qos, si.int_qos);
		pr_debug("%s: int_qos = %d\n", __func__, si.int_qos);
	}
#endif
	if (si.mif_qos != mif_qos_new) {
		si.mif_qos = mif_qos_new;
		pm_qos_update_request(&si.ca5_mif_qos, si.mif_qos);
		pr_debug("%s: mif_qos = %d\n", __func__, si.mif_qos);
	}
#endif
	return;
}

#ifdef CONFIG_SND_ESA_SA_EFFECT
int esa_effect_write(int type, int *value, int count)
{
	int effect_count = count;
	void __iomem *effect_addr;
	int i, *effect_value;
	int ret = 0;

	if (!check_esa_compr_state())
		return 0;

	if (pm_runtime_get_sync(&si.pdev->dev) < 0)
		return 0;

	effect_value = value;

	if (!si.effect_ram) {
		esa_err("%s: memory for effect parameters isn't ready\n",
				__func__);
		goto out;
	} else if (!si.fw_ready) {
		esa_err("%s: audio firmware isn't ready\n", __func__);
		goto out;
	}

	switch (type) {
	case SOUNDALIVE:
		effect_addr = si.effect_ram + SA_BASE;
		break;
	case MYSOUND:
		effect_addr = si.effect_ram + MYSOUND_BASE;
		break;
	case PLAYSPEED:
		effect_addr = si.effect_ram + VSP_BASE;
		break;
	case SOUNDBALANCE:
		effect_addr = si.effect_ram + LRSM_BASE;
		break;
	case MYSPACE:
		effect_addr = si.effect_ram + MYSPACE_BASE;
		break;
	case BASSBOOST:
		effect_addr = si.effect_ram + BB_BASE;
		pr_err("*************effect type : %d {BassBoost}\n", type);
		break;
	case EQUALIZER:
		effect_addr = si.effect_ram + EQ_BASE;
		pr_err("*************effect type : %d {Equalizer}\n", type);
		break;
	default	:
		pr_err("Not support effect type : %d\n", type);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < effect_count; i++) {
		pr_debug("effect_value[%d] = %d\n", i, effect_value[i]);
		writel(effect_value[i], effect_addr + 0x10 + (i * 4));
	}

	writel(CHANGE_BIT, effect_addr);

	esa_update_qos();
out:
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);

	return ret;
}
#endif

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static void esa_buffer_init(struct file *file)
{
	struct esa_rtd *rtd = file->private_data;
	unsigned long ibuf_size, obuf_size;
	unsigned int ibuf_count, obuf_count;
	unsigned char *buf;
	bool use_sram = false;

	esa_debug("iptype: %d", rtd->ip_type);

	switch (rtd->ip_type) {
	case ADEC_MP3:
		ibuf_size = DEC_IBUF_SIZE;
		obuf_size = DEC_OBUF_SIZE;
		ibuf_count = DEC_IBUF_NUM;
		obuf_count = DEC_OBUF_NUM;
		use_sram = true;
		break;
	case ADEC_AAC:
		ibuf_size = DEC_AAC_IBUF_SIZE;
		obuf_size = DEC_AAC_OBUF_SIZE;
		ibuf_count = DEC_IBUF_NUM;
		obuf_count = DEC_OBUF_NUM;
		break;
	case ADEC_FLAC:
		ibuf_size = DEC_FLAC_IBUF_SIZE;
		obuf_size = DEC_FLAC_OBUF_SIZE;
		ibuf_count = DEC_IBUF_NUM;
		obuf_count = DEC_OBUF_NUM;
		break;
	case SOUND_EQ:
		ibuf_size = SP_IBUF_SIZE;
		obuf_size = SP_OBUF_SIZE;
		ibuf_count = SP_IBUF_NUM;
		obuf_count = SP_OBUF_NUM;
		break;
	default:
		ibuf_size = 0x10000;
		obuf_size = 0x10000;
		ibuf_count = 1;
		obuf_count = 1;
		break;
	}

	rtd->ibuf_size = ibuf_size;
	rtd->obuf_size = obuf_size;
	rtd->ibuf_count = ibuf_count;
	rtd->obuf_count = obuf_count;
	rtd->use_sram = use_sram;

	/* You must consider seiren's module arrangement. */
	if (rtd->idx < 3) {
		buf = si.fwarea[0] + BASEMEM_OFFSET + rtd->idx * BUF_SIZE_MAX;
		rtd->block_num = 0;
	} else if (rtd->idx < 15) {
		buf = si.fwarea[1] + (rtd->idx - 3) * BUF_SIZE_MAX;
		rtd->block_num = 1;
	} else {
		buf = si.fwarea[2] + (rtd->idx - 15) * BUF_SIZE_MAX;
		rtd->block_num = 2;
	}
	rtd->ibuf0 = buf;

	buf += ibuf_count == 2 ? ibuf_size : 0;
	rtd->ibuf1 = buf;

	buf += ibuf_size;
	rtd->obuf0 = buf;

	buf += obuf_count == 2 ? obuf_size : 0;
	rtd->obuf1 = buf;
}
#endif

static int esa_send_cmd_(u32 cmd_code, bool sram_only)
{
	u32 cnt, val;
	int ret;

	si.isr_done = 0;
	writel(cmd_code, si.mailbox + CMD_CODE);	/* command */
	writel(1, si.regs + SW_INTR_CA5);		/* trigger ca5 */

	si.fw_use_dram = sram_only ? false : true;
	ret = wait_event_interruptible_timeout(esa_wq, si.isr_done, HZ / 2);
	if (!ret) {
		esa_err("%s: CMD(%08X) timed out!!!\n",
					__func__, cmd_code);
		esa_dump_fw_log();
		si.fw_use_dram = true;
		return -EBUSY;
	}

	cnt = msecs_to_loops(10);
	while (--cnt) {
		val = readl(si.regs + CA5_STATUS);
		if (val & CA5_STATUS_WFI)
			break;
		cpu_relax();
	}
	si.fw_use_dram = false;

	return 0;
}

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static ssize_t esa_write(struct file *file, const char *buffer,
					size_t size, loff_t *pos)
{
	struct esa_rtd *rtd = file->private_data;
	unsigned char *ibuf;
	unsigned char *obuf;
	unsigned int *obuf_filled_size;
	bool *obuf_filled;
	int response, consumed_size = 0;

	mutex_lock(&esa_mutex);
	if (pm_runtime_get_sync(&si.pdev->dev) < 0) {
		mutex_unlock(&esa_mutex);
		return 0;
	}

	if (rtd->obuf0_filled && rtd->obuf1_filled) {
		esa_err("%s: There is no unfilled obuf\n", __func__);
		goto err;
	}

	/* select IBUF0 or IBUF1 */
	if (rtd->select_ibuf == 0) {
		ibuf = rtd->ibuf0;
		obuf = rtd->obuf0;
		obuf_filled = &rtd->obuf0_filled;
		obuf_filled_size = &rtd->obuf0_filled_size;
		esa_debug("%s: use ibuf0\n", __func__);
	} else {
		ibuf = rtd->ibuf1;
		obuf = rtd->obuf1;
		obuf_filled = &rtd->obuf1_filled;
		obuf_filled_size = &rtd->obuf1_filled_size;
		esa_debug("%s: use ibuf1\n", __func__);
	}

	if (size > rtd->ibuf_size) {
		esa_err("%s: copy size is bigger than buffer size\n",
				__func__);
		goto err;
	}

	/* receive stream data from user */
	if (copy_from_user(ibuf, buffer, size)) {
		esa_err("%s: failed to copy_from_user\n", __func__);
		goto err;
	}

	/* select IBUF0 or IBUF1 for next writing */
	rtd->select_ibuf = !rtd->select_ibuf;

	/* send execute command to FW for decoding */
	response = esa_send_cmd_exe(rtd, ibuf, obuf, size);

	/* filled size in OBUF */
	*obuf_filled_size = readl(si.mailbox + SIZE_OUT_DATA);

	/* consumed size */
	consumed_size = readl(si.mailbox + CONSUMED_BYTE_IN);

	if (response == 0 && *obuf_filled_size > 0) {
		*obuf_filled = true;
	} else {
		if (consumed_size <= 0)
			consumed_size = response;
		if (rtd->need_config)
			rtd->need_config = false;
		else if (size != 0)
			esa_debug("%s: No output? response:%x\n", __func__, response);
	}

	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	esa_debug("%s: handle_id[%x], idx:[%d], consumed:[%d], filled_size:[%d], ibuf:[%d]\n",
			__func__, rtd->handle_id, rtd->idx, consumed_size,
			*obuf_filled_size, !rtd->select_ibuf);

	return consumed_size;
err:
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);
	return -EFAULT;
}

static ssize_t esa_read(struct file *file, char *buffer,
				size_t size, loff_t *pos)
{
	struct esa_rtd *rtd = file->private_data;
	unsigned char *obuf;
	unsigned int *obuf_filled_size;
	bool *obuf_filled;

	unsigned char *obuf_;
	unsigned int *obuf_filled_size_;
	bool *obuf_filled_;

	mutex_lock(&esa_mutex);
	if (pm_runtime_get_sync(&si.pdev->dev) < 0) {
		mutex_unlock(&esa_mutex);
		return 0;
	}

	/* select OBUF0 or OBUF1 */
	if (rtd->select_obuf == 0) {
		obuf = rtd->obuf0;
		obuf_filled = &rtd->obuf0_filled;
		obuf_filled_size = &rtd->obuf0_filled_size;

		obuf_ = rtd->obuf1;
		obuf_filled_ = &rtd->obuf1_filled;
		obuf_filled_size_ = &rtd->obuf1_filled_size;
		esa_debug("%s: use obuf0\n", __func__);
	} else {
		obuf = rtd->obuf1;
		obuf_filled = &rtd->obuf1_filled;
		obuf_filled_size = &rtd->obuf1_filled_size;

		obuf_ = rtd->obuf0;
		obuf_filled_ = &rtd->obuf0_filled;
		obuf_filled_size_ = &rtd->obuf0_filled_size;
		esa_debug("%s: use obuf1\n", __func__);
	}

	/* select OBUF0 or OBUF1 for next reading */
	rtd->select_obuf = !rtd->select_obuf;

	/* later... invalidate obuf cache */

	/* send pcm data to user */
	if (copy_to_user((void *)buffer, obuf, *obuf_filled_size)) {
		esa_err("%s: failed to copy_to_user\n", __func__);
		goto err;
	}

	/* if meet eos, it sholud also collect data of another buff */
	if (rtd->get_eos && !*obuf_filled_) {
		rtd->get_eos = EOS_FINAL;
	}

	esa_debug("%s: handle_id[%x], idx:[%d], obuf:[%d], obuf_filled_size:[%d]\n",
			__func__, rtd->handle_id, rtd->idx, !rtd->select_obuf,
			(u32)*obuf_filled_size);
	*obuf_filled = false;

	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	return *obuf_filled_size;
err:
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);
	return -EFAULT;
}

static int esa_exe(struct file *file, unsigned int param,
							unsigned long arg)
{
	struct esa_rtd *rtd = file->private_data;
	struct audio_mem_info_t ibuf_info, obuf_info;
	int response, obuf_filled_size;
	unsigned char *ibuf = rtd->ibuf0;
	unsigned char *obuf = rtd->obuf0;
	int ret = 0;

	mutex_lock(&esa_mutex);

	/* receive ibuf_info from user */
	if (copy_from_user(&ibuf_info, (struct audio_mem_info_t *)arg,
					sizeof(struct audio_mem_info_t))) {
		esa_err("%s: failed to copy_from_user ibuf_info\n", __func__);
		goto err;
	}

	/* receive obuf_info from user */
	arg += sizeof(struct audio_mem_info_t);
	if (copy_from_user(&obuf_info, (struct audio_mem_info_t *)arg,
					sizeof(struct audio_mem_info_t))) {
		esa_err("%s: failed to copy_from_user obuf_info\n", __func__);
		goto err;
	}

	/* prevent test alarmed about tainted data */
	if (ibuf_info.mem_size > rtd->ibuf_size) {
		ibuf_info.mem_size = rtd->ibuf_size;
		esa_err("%s: There is too much input data", __func__);
	}

	/* receive pcm data from user */
	if (copy_from_user(ibuf, (void *)(u64)ibuf_info.virt_addr,
					ibuf_info.mem_size)) {
		esa_err("%s: failed to copy_from_user\n", __func__);
		goto err;
	}

	/* send execute command to FW for decoding */
	response = esa_send_cmd_exe(rtd, ibuf, obuf, ibuf_info.mem_size);

	/* filled size in OBUF */
	obuf_filled_size = readl(si.mailbox + SIZE_OUT_DATA);

	/* later... invalidate obuf cache */

	if (!response && obuf_filled_size > 0) {
		esa_debug("%s: cmd_exe OK!\n", __func__);
		/* send pcm data to user */
		if (copy_to_user((void *)(u64)obuf_info.virt_addr,
					obuf, obuf_filled_size)) {
			esa_err("%s: failed to copy_to_user obuf pcm\n",
					__func__);
			goto err;
		}
	} else {
		esa_debug("%s: cmd_exe Fail: %d\n", __func__, response);
	}

	esa_debug("%s: handle_id[%x], idx:[%d], filled_size:[%d]\n",
			__func__, rtd->handle_id, rtd->idx, obuf_filled_size);

	return ret;
err:
	mutex_unlock(&esa_mutex);
	return -EFAULT;
}

static int esa_set_params(struct file *file, unsigned int param,
							unsigned long arg)
{
	struct esa_rtd *rtd = file->private_data;
	int ret = 0;

	switch (param) {
	case PCM_PARAM_MAX_SAMPLE_RATE:
		esa_debug("PCM_PARAM_MAX_SAMPLE_RATE: arg:%ld\n", arg);
		break;
	case PCM_PARAM_MAX_NUM_OF_CH:
		esa_debug("PCM_PARAM_MAX_NUM_OF_CH: arg:%ld\n", arg);
		break;
	case PCM_PARAM_MAX_BIT_PER_SAMPLE:
		esa_debug("PCM_PARAM_MAX_BIT_PER_SAMPLE: arg:%ld\n", arg);
		break;
	case PCM_PARAM_SAMPLE_RATE:
		esa_debug("%s: sampling rate: %ld\n", __func__, arg);
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		writel((unsigned int)arg, si.mailbox + PARAMS_VAL1);
		esa_send_cmd((param << 16) | CMD_SET_PARAMS);
		break;
	case PCM_PARAM_NUM_OF_CH:
		esa_debug("%s: channel: %ld\n", __func__, arg);
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		writel((unsigned int)arg, si.mailbox + PARAMS_VAL1);
		esa_send_cmd((param << 16) | CMD_SET_PARAMS);
		break;
	case PCM_PARAM_BIT_PER_SAMPLE:
		esa_debug("PCM_PARAM_BIT_PER_SAMPLE: arg:%ld\n", arg);
		break;
	case PCM_MAX_CONFIG_INFO:
		esa_debug("PCM_MAX_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case PCM_CONFIG_INFO:
		esa_debug("PCM_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case EQ_PARAM_NUM_OF_PRESETS:
		esa_debug("EQ_PARAM_NUM_OF_PRESETS: arg:%ld\n", arg);
		break;
	case EQ_PARAM_MAX_NUM_OF_BANDS:
		esa_debug("EQ_PARAM_MAX_NUM_OF_BANDS: arg:%ld\n", arg);
		break;
	case EQ_PARAM_RANGE_OF_BANDLEVEL:
		esa_debug("EQ_PARAM_RANGE_OF_BANDLEVEL: arg:%ld\n", arg);
		break;
	case EQ_PARAM_RANGE_OF_FREQ:
		esa_debug("EQ_PARAM_RANGE_OF_FREQ: arg:%ld\n", arg);
		break;
	case EQ_PARAM_PRESET_ID:
		esa_debug("%s: eq preset: %ld\n", __func__, arg);
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		writel((unsigned int)arg, si.mailbox + PARAMS_VAL1);
		esa_send_cmd((param << 16) | CMD_SET_PARAMS);
		break;
	case EQ_PARAM_NUM_OF_BANDS:
		esa_debug("EQ_PARAM_NUM_OF_BANDS: arg:%ld\n", arg);
		break;
	case EQ_PARAM_CENTER_FREQ:
		esa_debug("EQ_PARAM_CENTER_FREQ: arg:%ld\n", arg);
		break;
	case EQ_PARAM_BANDLEVEL:
		esa_debug("EQ_PARAM_BANDLEVEL: arg:%ld\n", arg);
		break;
	case EQ_PARAM_BANDWIDTH:
		esa_debug("EQ_PARAM_BANDWIDTH: arg:%ld\n", arg);
		break;
	case EQ_MAX_CONFIG_INFO:
		esa_debug("EQ_MAX_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case EQ_CONFIG_INFO:
		esa_debug("EQ_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case EQ_BAND_INFO:
		esa_debug("EQ_BAND_INFO: arg:%ld\n", arg);
		break;
	case ADEC_PARAM_SET_EOS:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd((param << 16) | CMD_SET_PARAMS);
		esa_debug("ADEC_PARAM_SET_EOS: handle_id:%x\n", rtd->handle_id);
		rtd->get_eos = EOS_YET;
		break;
	case SET_IBUF_POOL_INFO:
		esa_debug("SET_IBUF_POOL_INFO: arg:%ld\n", arg);
		break;
	case SET_OBUF_POOL_INFO:
		esa_debug("SET_OBUF_POOL_INFO: arg:%ld\n", arg);
		break;
	default:
		esa_err("%s: Unknown %ld\n", __func__, arg);
		break;
	}

	return ret;
}

static int esa_get_params(struct file *file, unsigned int param,
							unsigned long arg)
{
	struct esa_rtd *rtd = file->private_data;
	struct audio_mem_info_t *argp = (struct audio_mem_info_t *)arg;
	struct audio_pcm_config_info_t *argp_pcm_info;
	unsigned long val;
	int ret = 0;

	switch (param) {
	case PCM_PARAM_MAX_SAMPLE_RATE:
		esa_debug("PCM_PARAM_MAX_SAMPLE_RATE: arg:%ld\n", arg);
		break;
	case PCM_PARAM_MAX_NUM_OF_CH:
		esa_debug("PCM_PARAM_MAX_NUM_OF_CH: arg:%ld\n", arg);
		break;
	case PCM_PARAM_MAX_BIT_PER_SAMPLE:
		esa_debug("PCM_PARAM_MAX_BIT_PER_SAMPLE: arg:%ld\n", arg);
		break;
	case PCM_PARAM_SAMPLE_RATE:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd((param << 16) | CMD_GET_PARAMS);
		val = readl(si.mailbox + PARAMS_VAL1);
		esa_debug("%s: sampling rate:%ld\n", __func__, val);

		if (val >= SAMPLE_RATE_MIN) {
			esa_debug("SAMPLE_RATE: SUCCESS: arg:0x%p\n", (void*)arg);
			ret = copy_to_user((unsigned long *)arg, &val,
						sizeof(unsigned long));
		} else
			esa_debug("SAMPLE_RATE: FAILED: arg:0x%p\n", (void*)arg);
		break;
	case PCM_PARAM_NUM_OF_CH:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd((param << 16) | CMD_GET_PARAMS);
		val = readl(si.mailbox + PARAMS_VAL1);
		esa_debug("%s: Channel:%ld\n", __func__, val);

		if (val >= CH_NUM_MIN) {
			esa_debug("PCM_CONFIG_NUM_CH: SUCCESS: arg:0x%p\n", (void*)arg);
			ret = copy_to_user((unsigned long *)arg, &val,
						sizeof(unsigned long));
		} else
			esa_debug("PCM_PARAM_NUM_CH: FAILED: arg:0x%p\n", (void*)arg);
		break;
	case PCM_PARAM_BIT_PER_SAMPLE:
		esa_debug("PCM_PARAM_BIT_PER_SAMPLE: arg:%ld\n", arg);
		break;
	case PCM_MAX_CONFIG_INFO:
		esa_debug("PCM_MAX_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case PCM_CONFIG_INFO:
		argp_pcm_info = (struct audio_pcm_config_info_t *)arg;
		rtd->pcm_info.sample_rate = readl(si.mailbox + FREQ_SAMPLE);
		rtd->pcm_info.num_of_channel = readl(si.mailbox + CH_NUM);
		esa_debug("%s: rate:%d, ch:%d\n", __func__,
					rtd->pcm_info.sample_rate,
					rtd->pcm_info.num_of_channel);
		if (rtd->pcm_info.sample_rate >= 8000 &&
			rtd->pcm_info.num_of_channel > 0) {
			esa_debug("PCM_CONFIG_INFO: SUCCESS: arg:0x%p\n", (void*)arg);
			ret = copy_to_user(argp_pcm_info, &rtd->ibuf_info,
					sizeof(struct audio_pcm_config_info_t));
		} else
			esa_debug("PCM_CONFIG_INFO: FAILED: arg:0x%p\n", (void*)arg);
		break;
	case ADEC_PARAM_GET_OUTPUT_STATUS:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd((param << 16) | CMD_GET_PARAMS);
		val = readl(si.mailbox + PARAMS_VAL1);
		esa_debug("OUTPUT_STATUS:%ld, handle_id:%x\n", val, rtd->handle_id);
		if (val && rtd->get_eos == EOS_YET)
			val = 0;
		ret = copy_to_user((unsigned long *)arg, &val,
					sizeof(unsigned long));
		break;
	case GET_IBUF_POOL_INFO:
		esa_debug("GET_IBUF_POOL_INFO: arg:0x%p\n", (void*)arg);
		rtd->ibuf_info.mem_size = rtd->ibuf_size;
		rtd->ibuf_info.block_count = rtd->ibuf_count;
		ret = copy_to_user(argp, &rtd->ibuf_info,
					sizeof(struct audio_mem_info_t));
		break;
	case GET_OBUF_POOL_INFO:
		esa_debug("GET_OBUF_POOL_INFO: arg:0x%p\n", (void*)arg);
		rtd->obuf_info.mem_size = rtd->obuf_size;
		rtd->obuf_info.block_count = rtd->obuf_count;
		ret = copy_to_user(argp, &rtd->obuf_info,
					sizeof(struct audio_mem_info_t));
		break;
	default:
		esa_err("%s: Unknown %ld\n", __func__, arg);
		break;
	}

	if (ret)
		esa_err("%s: failed to copy_to_user\n", __func__);

	return ret;
}
#endif

static irqreturn_t esa_isr(int irqno, void *id)
{
	unsigned int fw_stat, log_size, val;
	int index;
	bool wakeup = false;

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	unsigned int size;
	int ret;
#endif
	writel(0, si.regs + SW_INTR_CPU);
	val = readl(si.mailbox + RETURN_CMD);
	if (val == 1)
		esa_err("%s: There is possibility of firmware CMD fail %u\n",
						__func__, val);
	fw_stat = val >> 16;
	esa_debug("fw_stat(%08x), val(%08x)\n", fw_stat, val);

	switch (fw_stat) {
	case INTR_WAKEUP:	/* handle wakeup interrupt from firmware  */
		si.isr_done = 1;
		wakeup = true;
		break;
	case INTR_READY:
		pr_debug("FW is ready!\n");
		si.fw_ready = true;
		wakeup = true;
		break;
	case INTR_DMA:
		index = readl(si.mailbox + COMPR_DMA_IDX);
#ifdef CONFIG_SND_SOC_EAX_SLOWPATH
		pr_debug("ADMA INTR index = %d !!!!\n", index);
		eax_slowpath_wakeup_buf_wq(index);
#endif
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
	case INTR_DMA_INDEX:
		index = readl(si.mailbox + RETURN_CMD) & 0xF;
#ifdef CONFIG_SND_SOC_EAX_SLOWPATH
		pr_debug("ADMA INTR index = %d , dma_ptr = %x!!!!\n",
			index, readl(dma_reg + 0x440));
		eax_slowpath_wakeup_buf_wq(index);
#endif
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	case INTR_CREATED:
		esa_info("INTR_CREATED\n");
		si.isr_compr_created = 1;
		wakeup = true;
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
	case INTR_DECODED:
		/* check the error */
		val &= 0xFF;
		if (val) {
			esa_err("INTR_DECODED err(%d)\n", val);
			esa_dump_fw_log();
			writel(0, si.mailbox + COMPR_INTR_ACK);
			break;
		}
		/* update copied total bytes */
		size = readl(si.mailbox + COMPR_SIZE_OUT_DATA);
		esa_debug("INTR_DECODED(%d)\n", size);
		if (ptr_ap) {
			ret = ptr_ap->ops(fw_stat, size, ptr_ap->priv);
			if (ret)
				esa_err("INTR_DECODED handler err(%d)\n", ret);
		}
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
	case INTR_FLUSH:
		/* check the error */
		val &= 0xFF;
		if (val) {
			esa_err("INTR_FLUSH err(%d)\n", val);
			esa_dump_fw_log();
			writel(0, si.mailbox + COMPR_INTR_ACK);
			break;
		}
		/* flush done */
		if (ptr_ap) {
			ret = ptr_ap->ops(fw_stat, 0, ptr_ap->priv);
			if (ret)
				esa_err("INTR_FLUSH handler err(%d)\n", ret);
		}
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
	case INTR_PAUSED:
		/* check the error */
		val &= 0xFF;
		if (val) {
			esa_err("INTR_PAUSED err(%d)\n", val);
			esa_dump_fw_log();
			writel(0, si.mailbox + COMPR_INTR_ACK);
			break;
		}
		/* paused */
		if (ptr_ap) {
			ret = ptr_ap->ops(fw_stat, 0, ptr_ap->priv);
			if (ret)
				esa_err("INTR_PAUSED handler err(%d)\n", ret);
		}
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
	case INTR_EOS:
		if (ptr_ap) {
			ret = ptr_ap->ops(fw_stat, 0, ptr_ap->priv);
			if (ret)
				esa_err("INTR_EOS handler err(%d)\n", ret);
		}
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
	case INTR_DESTROY:
		/* check the error */
		val &= 0xFF;
		if (val) {
			esa_err("INTR_DESTROY err(%d)\n", val);
			esa_dump_fw_log();
			writel(0, si.mailbox + COMPR_INTR_ACK);
			break;
		}
		/* destroied */
		if (ptr_ap) {
			ret = ptr_ap->ops(fw_stat, 0, ptr_ap->priv);
			if (ret)
				esa_err("INTR_DESTROY handler err(%d)\n",  ret);
		}
		writel(0, si.mailbox + COMPR_INTR_ACK);
		si.isr_compr_created = 0;
		break;
	case INTR_FX_EXT:
		esa_debug("INTR_FX_EXT: fw_stat(%08x), val(%08x)\n",
				fw_stat, val);
		si.fx_next_idx = val & 0xF;
		si.fx_irq_done = true;
		if (waitqueue_active(&esa_fx_wq))
			wake_up_interruptible(&esa_fx_wq);
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
#ifdef CONFIG_SND_SAMSUNG_ELPE
	case INTR_EFF_REQUEST:
		queue_lpeff_cmd(LPEFF_EFFECT_CMD);
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
#endif
#endif
	case INTR_FW_LOG:	/* print debug message from firmware */
		log_size = readl(si.mailbox + RETURN_CMD) & 0x00FF;
		if (!log_size) {
			esa_debug("FW: %s", si.fw_log);
			si.fw_log_pos = 0;
		} else {
			val = readl(si.mailbox + FW_LOG_VAL1);
			memcpy(si.fw_log + si.fw_log_pos, &val, 4);
			val = readl(si.mailbox + FW_LOG_VAL2);
			memcpy(si.fw_log + si.fw_log_pos + 4, &val, 4);
			si.fw_log_pos += log_size;
			si.fw_log[si.fw_log_pos] = '\0';
		}
		writel(0, si.mailbox + RETURN_CMD);
		break;
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	case INTR_SET_CPU_LOCK:	/* Set CPU LOCK for OFFLOAD EFFECT */
		si.cpu_lock_level = readl(si.mailbox + COMPR_CPU_LOCK_LV);
		si.set_cpu_lock = true;
		if (waitqueue_active(&esa_cpu_lock_wq))
			wake_up_interruptible(&esa_cpu_lock_wq);
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
#endif
	default:
		esa_err("%s: unknown intr_stat = 0x%08X\n",
						__func__, fw_stat);
		writel(0, si.mailbox + COMPR_INTR_ACK);
		break;
	}

	if (wakeup && waitqueue_active(&esa_wq))
		wake_up_interruptible(&esa_wq);

	return IRQ_HANDLED;
}

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static long esa_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct esa_rtd *rtd = file->private_data;
	int ret = 0;
	unsigned int param = cmd >> 16;

	cmd = cmd & 0xffff;

	esa_debug("%s: idx:%d, param:%x, cmd:%x\n", __func__,
				rtd->idx, param, cmd);

	mutex_lock(&esa_mutex);
	if (pm_runtime_get_sync(&si.pdev->dev) < 0) {
		mutex_unlock(&esa_mutex);
		return 0;
	}

	switch (cmd) {
	case SEIREN_IOCTL_CH_CREATE:
		rtd->ip_type = (unsigned int) arg;
		arg = arg << 16;
		writel(arg, si.mailbox + IP_TYPE);
		ret = esa_send_cmd(CMD_CREATE);
		if (ret == -EBUSY)
			break;
		ret = readl(si.mailbox + RETURN_CMD);
		if (ret != 0)
			break;
		rtd->handle_id = readl(si.mailbox + IP_ID);
		esa_buffer_init(file);
		esa_debug("CH_CREATE: ret_val:%x, handle_id:%x\n",
				readl(si.mailbox + RETURN_CMD),
				rtd->handle_id);
		esa_update_qos();
		break;
	case SEIREN_IOCTL_CH_DESTROY:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		ret = esa_send_cmd(CMD_DESTROY);
		if (ret == -EBUSY)
			break;
		ret = readl(si.mailbox + RETURN_CMD);
		if (ret != 0)
			break;
		esa_debug("CH_DESTROY: ret_val:%x, handle_id:%x\n",
				readl(si.mailbox + RETURN_CMD),
				rtd->handle_id);
		break;
	case SEIREN_IOCTL_CH_EXE:
		esa_debug("CH_EXE\n");
		ret = esa_exe(file, param, arg);
		break;
	case SEIREN_IOCTL_CH_SET_PARAMS:
		esa_debug("CH_SET_PARAMS\n");
		ret = esa_set_params(file, param, arg);
		break;
	case SEIREN_IOCTL_CH_GET_PARAMS:
		esa_debug("CH_GET_PARAMS\n");
		ret = esa_get_params(file, param, arg);
		break;
	case SEIREN_IOCTL_CH_RESET:
		esa_debug("CH_RESET\n");
		break;
	case SEIREN_IOCTL_CH_FLUSH:
		arg = arg << 16;
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd(CMD_RESET);
		esa_debug("CH_FLUSH: val: %x, handle_id : %x\n",
				readl(si.mailbox + RETURN_CMD),
				rtd->handle_id);
		rtd->get_eos = EOS_NO;
		rtd->select_ibuf = 0;
		rtd->select_obuf = 0;
		rtd->obuf0_filled = false;
		rtd->obuf1_filled = false;
		break;
	case SEIREN_IOCTL_CH_CONFIG:
		esa_debug("CH_CONFIG\n");
		rtd->need_config = true;
		break;
	}

	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	return ret;
}

static int esa_open(struct inode *inode, struct file *file)
{
	struct esa_rtd *rtd;

	if (!si.fwmem_loaded) {
		esa_err("Firmware not ready\n");
		return -ENXIO;
	}

	/* alloc rtd */
	rtd = esa_alloc_rtd();
	if (!rtd) {
		esa_debug("%s: Not enough memory\n", __func__);
		return -EFAULT;
	}

	esa_debug("%s: idx:%d\n", __func__, rtd->idx);

	/* initialize */
	file->private_data = rtd;
	rtd->get_eos = EOS_NO;
	rtd->need_config = false;
	rtd->select_ibuf = 0;
	rtd->select_obuf = 0;
	rtd->obuf0_filled = false;
	rtd->obuf1_filled = false;

	return 0;
}

static int esa_release(struct inode *inode, struct file *file)
{
	struct esa_rtd *rtd = file->private_data;

	esa_debug("%s: idx:%d\n", __func__, rtd->idx);

	/* de-initialize */
	file->private_data = NULL;

	esa_free_rtd(rtd);

	return 0;
}
#endif

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static int esa_open(struct inode *inode, struct file *file)
{
	esa_info("%s\n", __func__);

	return 0;
}

static int esa_release(struct inode *inode, struct file *file)
{
	esa_info("%s\n", __func__);

	return 0;
}

ssize_t esa_copy(unsigned long hwbuf, ssize_t size)
{
	int i, cnt, ret;

	mutex_lock(&esa_mutex);
	if (pm_runtime_get_sync(&si.pdev->dev) < 0) {
		mutex_unlock(&esa_mutex);
		return 0;
	}

	cnt = size / FX_BUF_SIZE;
	if (cnt && (size - (FX_BUF_SIZE * cnt))) {
		cnt++;
	} else {
		cnt = 1;
	}

	for (i = 0; i < cnt; i++) {
		size_t copy_size;
		ret = wait_event_interruptible_timeout(esa_fx_wq,
				si.fx_irq_done, HZ);
		if (!ret) {
			esa_err("%s: fx irq timeout\n", __func__);
			ret = -EBUSY;
			goto out;
		}

		si.fx_irq_done = false;
		if (si.sram)
			si.fx_work_buf = (unsigned char *)(si.sram + FX_BUF_OFFSET);
		else
			esa_err("%s: sram address is empty (%d)\n", __func__, __LINE__);
		si.fx_work_buf += si.fx_next_idx * FX_BUF_SIZE;
		esa_debug("%s: buf_idx = %d\n", __func__, si.fx_next_idx);

		copy_size = (size > FX_BUF_SIZE) ? FX_BUF_SIZE : size;
		memcpy((char *)hwbuf, si.fx_work_buf, copy_size);
		hwbuf += (unsigned long)copy_size;
		size -= copy_size;
	}
out:
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	return ret;
}


static ssize_t esa_read(struct file *file, char *buffer,
				size_t size, loff_t *pos)
{
	int ret;

	mutex_lock(&esa_mutex);
	if (pm_runtime_get_sync(&si.pdev->dev) < 0) {
		mutex_unlock(&esa_mutex);
		return 0;
	}

	if (!si.fx_ext_on) {
		esa_debug("%s: fx ext not enabled\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = wait_event_interruptible_timeout(esa_fx_wq, si.fx_irq_done, HZ);
	if (!ret) {
		esa_err("%s: fx irq timeout\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	si.fx_irq_done = false;
	if (si.sram)
		si.fx_work_buf = (unsigned char *)(si.sram + FX_BUF_OFFSET);
	else
		esa_err("%s: sram address is empty (%d)\n", __func__, __LINE__);
	si.fx_work_buf += si.fx_next_idx * FX_BUF_SIZE;
	esa_debug("%s: buf_idx = %d\n", __func__, si.fx_next_idx);

	if (copy_to_user((void *)buffer, si.fx_work_buf, FX_BUF_SIZE)) {
		esa_err("%s: failed to copy_to_user\n", __func__);
		ret = -EFAULT;
	} else {
		ret = FX_BUF_SIZE;
	}
out:
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	return ret;
}

static ssize_t esa_write(struct file *file, const char *buffer,
					size_t size, loff_t *pos)
{
	int ret;

	mutex_lock(&esa_mutex);
	if (pm_runtime_get_sync(&si.pdev->dev) < 0) {
		mutex_unlock(&esa_mutex);
		return 0;
	}

	if (!si.fx_ext_on) {
		esa_debug("%s: fx ext not enabled\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (!si.fx_work_buf) {
		esa_debug("%s: fx buf not ready\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	if (size > FX_BUF_SIZE) {
		esa_err("%s: copy size is bigger than buffer size\n",
				__func__);
		goto out;
	}

	if (copy_from_user(si.fx_work_buf, buffer, size)) {
		esa_err("%s: failed to copy_from_user\n", __func__);
		ret = -EFAULT;
	} else {
		esa_debug("%s: %lu bytes\n", __func__, size);
		ret = FX_BUF_SIZE;
	}
out:
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	return ret;
}

static long esa_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	mutex_lock(&esa_mutex);
	if (pm_runtime_get_sync(&si.pdev->dev) < 0) {
		mutex_unlock(&esa_mutex);
		return 0;
	}

	switch (cmd) {
	case SEIREN_IOCTL_FX_EXT:
		si.fx_ext_on = arg ? true : false;
		writel(si.fx_ext_on ? 1 : 0, si.mailbox + EFFECT_EXT_ON);
		break;
#ifdef CONFIG_SND_SAMSUNG_ELPE
	case SEIREN_IOCTL_ELPE_DONE:
		if (si.effect_ram) {
			writel(arg, si.effect_ram + ELPE_BASE + ELPE_RET);
			writel(0, si.effect_ram + ELPE_BASE + ELPE_DONE);
		} else {
			esa_err("%s: elpe effect ram is empty\n", __func__);
		}
		break;
#endif
	default:
		esa_err("%s: unknown cmd:%08X, arg:%08X\n",
			__func__, cmd, (unsigned int)arg);
		break;
	}

	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	return ret;
}
#endif

static int esa_mmap(struct file *filep, struct vm_area_struct *vmarea)
{
	unsigned int pfn;
	unsigned long len = vmarea->vm_end - vmarea->vm_start;
	int ret = 0;

	esa_info("%s: start=0x%p, size=%ld, offset=%ld, phys=0x%llX",
			__func__,
			(void *)vmarea->vm_start, len, vmarea->vm_pgoff,
			(u64)si.fwarea_pa[1]);

	if (len > FWAREA_SIZE) {
		esa_err("%s: failed to mmap\n", __func__);
		return -EINVAL;
	}

	vmarea->vm_flags |= VM_IO;

	pfn = (unsigned int)(si.fwarea_pa[1] >> PAGE_SHIFT);
	ret = (int)remap_pfn_range(vmarea, vmarea->vm_start,
			pfn, len, pgprot_noncached(vmarea->vm_page_prot));

	esa_info("%s: ret = 0x%x\n", __func__, ret);
	return ret;
}

static const struct file_operations esa_fops = {
	.owner		= THIS_MODULE,
	.open		= esa_open,
	.release	= esa_release,
	.read		= esa_read,
	.write		= esa_write,
	.unlocked_ioctl	= esa_ioctl,
	.compat_ioctl	= esa_ioctl,
	.mmap		= esa_mmap,
};

static struct miscdevice esa_miscdev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "seiren",
	.fops		= &esa_fops,
};

static int esa_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int esa_proc_open(struct inode *inode, struct  file *file)
{
	header_printed = 0;
	end_index = 0;

	return single_open(file, esa_proc_show, NULL);
}

static ssize_t esa_proc_read(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	ssize_t len, ret = 0;
	char *buf;
	int fwver;
	int n;
	char log[256];
	char *addr = si.fw_log_buf;
	int next;
	int i;
	int index, prev_index, start_index;
	int is_skip = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (header_printed == 0) {
		len = snprintf(buf + ret, PAGE_SIZE - ret,
				"fw is %s\n", si.fw_ready ? "ready" : "off");
		if (len > 0) ret += len;
		len = snprintf(buf + ret, PAGE_SIZE - ret,
				"rtd cnt: %d\n", si.rtd_cnt);
		if (len > 0) ret += len;
#ifdef CONFIG_PM_DEVFREQ
		len = snprintf(buf + ret, PAGE_SIZE - ret,
				"mif: %d\n", si.mif_qos / 1000);
		if (len > 0) ret += len;
#endif
		if (si.fw_ready) {
			fwver = readl(si.mailbox + VIRSION_ID);
			len = snprintf(buf + ret, PAGE_SIZE - ret,
					"Firmware Version: %c%c%c-%c\n",
					(fwver >> 24), ((fwver >> 16) & 0xFF),
					((fwver >> 8) & 0xFF), fwver & 0xFF);
			if (len > 0) ret += len;
			len = snprintf(buf + ret, PAGE_SIZE - ret,
					"fw_log:\n");
			if (len > 0) ret += len;
			len = snprintf(buf + ret, PAGE_SIZE - ret,
					"running cnt:%d\n",
					readl(si.mailbox + COMPR_CHECK_RUNNING));
			if (len > 0) ret += len;
			len = snprintf(buf + ret, PAGE_SIZE - ret,
					"ack status: lpcm(%d) compr(%d:%x)\n",
					readl(si.mailbox + COMPR_INTR_DMA_ACK),
					readl(si.mailbox + COMPR_INTR_ACK),
					readl(si.mailbox + COMPR_CHECK_CMD));
			if (len > 0) ret += len;
		}

		if (si.fw_ready) {
			/* sorting */
			prev_index = 0;
			start = 0;
			for (n = 0; n < FW_LOG_LINE; n++, addr += 128) {
				memcpy(log, addr, 128);
				index = 0;
				for (i = 0; i < 8; i++)
					index += (hex_to_bin(log[i]) << ((7-i)*4));
				if (index < 0)
					is_skip = 1;
				else if (prev_index <= index) {
					prev_index = index;
					end_index = index;
					end = n;
				} else {
					start_index = index;
					start = n;
					break;
				}
			}

			/* log print */
			if (!is_skip) {
				addr = si.fw_log_buf + start * 128;
				for (n = start; n < FW_LOG_LINE; n++, addr += 128) {
					memcpy(log, addr, 128);
					len = snprintf(buf + ret, PAGE_SIZE - ret,
							"%s", log);
					if (len > 0) ret += len;
				}
			}
			addr = si.fw_log_buf;
			for (n = 0; n <= end; n++, addr += 128) {
				memcpy(log, addr, 128);
				len = snprintf(buf + ret, PAGE_SIZE - ret,
						"%s", log);
				if (len > 0) ret += len;
			}
		}

		if (ret > PAGE_SIZE) ret = PAGE_SIZE;

		if (copy_to_user(user_buf, buf, ret)) {
			ret = -EFAULT;
		}
		header_printed = 1;
	} else {
		ret = 0;
		while (si.fw_ready) {
			udelay(500);
			raw_spin_lock_irq(&esa_logbuf_lock);
			/* Check prev printed index */
			addr = si.fw_log_buf;
			next = (end+1)%FW_LOG_LINE;
			memcpy(log, addr+(next*128), 128);
			index = 0;
			for (i = 0; i < 8; i++)
				index += (hex_to_bin(log[i]) << ((7-i)*4));
			if (index > end_index) {
				end_index = index;
				end = next;
				len = snprintf(buf + ret, PAGE_SIZE - ret,
						"%s", log);
				if (len > 0) {
					ret += len;
					if (ret > PAGE_SIZE) {
						raw_spin_unlock_irq(&esa_logbuf_lock);
						break;
					}
					if (copy_to_user(user_buf, buf, ret)) {
						printk("copy_to_user error \n");
						ret = -EFAULT;
					}
				}
			} else {
				if (ret != 0) {
					raw_spin_unlock_irq(&esa_logbuf_lock);
					break;
				}
			}
			raw_spin_unlock_irq(&esa_logbuf_lock);
		}
	}

	kfree(buf);
	return ret;
}

static const struct file_operations esa_proc_fops = {
	.owner = THIS_MODULE,
	.open = esa_proc_open,
	.read = esa_proc_read,
};

#ifdef CONFIG_PM_RUNTIME
static int esa_do_suspend(struct device *dev)
{
	esa_fw_shutdown();
	clk_disable_unprepare(si.clk_ca5);
	lpass_put_sync(dev);
	esa_update_qos();

	si.pm_on = false;

#ifdef CONFIG_SOC_EXYNOS8890
	si.sram = NULL;
	si.fw_log_buf = NULL;
	si.effect_ram = NULL;
	lpeff_set_effect_addr(NULL);
#endif
	lpass_update_lpclock(LPCLK_CTRLID_OFFLOAD, false);
	return 0;
}
#endif

static int esa_do_resume(struct device *dev)
{
	int ret;

	si.pm_on = true;

	lpass_update_lpclock(LPCLK_CTRLID_OFFLOAD, true);
	lpass_get_sync(dev);
#ifdef CONFIG_SOC_EXYNOS8890
	si.sram = lpass_get_mem();
	if (!si.sram) {
		esa_err("Failed to get lpass sram\n");
		return -ENOMEM;
	} else {
		si.fw_log_buf = si.sram + FW_LOG_ADDR;
		si.effect_ram = si.sram + EFFECT_OFFSET;
		lpeff_set_effect_addr(si.effect_ram);
	}
#endif
	clk_prepare_enable(si.clk_ca5);
	ret = esa_fw_startup();
	if (ret) {
		si.pm_on = false;
		clk_disable_unprepare(si.clk_ca5);
		lpass_put_sync(dev);
		lpass_update_lpclock(LPCLK_CTRLID_OFFLOAD, false);
		return ret;
	}
	esa_update_qos();

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int esa_suspend(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	if (!si.pm_on)
		return 0;

#ifdef CONFIG_SOC_EXYNOS8890
	si.sram = lpass_get_mem();
	if (!si.sram) {
		esa_err("Failed to get lpass sram %s\n", __func__);
		return -ENOMEM;
	} else {
		si.fw_log_buf = si.sram + FW_LOG_ADDR;
		si.effect_ram = si.sram + EFFECT_OFFSET;
		lpeff_set_effect_addr(si.effect_ram);
	}
#endif
	esa_fw_shutdown();
	clk_disable_unprepare(si.clk_ca5);

	si.pm_suspended = true;

	return 0;
}

static int esa_resume(struct device *dev)
{
#ifdef CONFIG_SOC_EXYNOS8890
	void __iomem	*cmu_reg;
#endif

	esa_debug("%s entered\n", __func__);

	if (!si.pm_suspended)
		return 0;

	si.pm_suspended = false;

#ifdef CONFIG_SOC_EXYNOS8890
	si.sram = lpass_get_mem();
	if (!si.sram) {
		esa_err("Failed to get lpass sram %s\n", __func__);
		return -ENOMEM;
	} else {
		si.fw_log_buf = si.sram + FW_LOG_ADDR;
		si.effect_ram = si.sram + EFFECT_OFFSET;
		lpeff_set_effect_addr(si.effect_ram);
	}
	cmu_reg = ioremap(0x114C0000, SZ_4K);
	writel(0x1, cmu_reg + 0x800); /* Enable CA5 clock */
	iounmap(cmu_reg);
#endif
	clk_prepare_enable(si.clk_ca5);
	esa_fw_startup();

	return 0;
}
#else
#define esa_suspend	NULL
#define esa_resume	NULL
#endif

#ifdef CONFIG_SND_SAMSUNG_IOMMU
static int esa_prepare_buffer(struct device *dev)
{
	unsigned long iova;
	int n, ret;

	/* Original firmware */
	si.fwmem = devm_kzalloc(dev, FWMEM_SIZE, GFP_KERNEL);
	if (!si.fwmem) {
		esa_err("Failed to alloc fwmem\n");
		goto err;
	}
	si.fwmem_pa = virt_to_phys(si.fwmem);

	/* Firmware backup for SRAM */
	si.fwmem_sram_bak = devm_kzalloc(dev, SRAM_FW_MAX, GFP_KERNEL);
	if (!si.fwmem_sram_bak) {
		esa_err("Failed to alloc fwmem_sram_bak\n");
		goto err;
	}

	/* Firmware for DRAM */
	for (n = 0; n < FWAREA_NUM; n++) {
		si.fwarea[n] = dma_alloc_coherent(dev, FWAREA_SIZE,
					&si.fwarea_pa[n], GFP_KERNEL);
		esa_info("si.fwarea[%d] v_address : 0x%p p_address : 0x%x\n",
				n, si.fwarea[n], (int)si.fwarea_pa[n]);
		if (!si.fwarea[n]) {
			esa_err("Failed to alloc fwarea\n");
			goto err0;
		}
	}

	for (n = 0, iova = FWAREA_IOVA;
			n < FWAREA_NUM; n++, iova += FWAREA_SIZE) {
		ret = iommu_map(si.domain, iova,
				si.fwarea_pa[n], FWAREA_SIZE, 0);
		if (ret) {
			esa_err("Failed to map iommu\n");
			goto err1;
		}
	}

	/* Base address for IBUF, OBUF and FW LOG  */
	si.bufmem = si.fwarea[0] + BASEMEM_OFFSET;
	si.bufmem_pa = si.fwarea_pa[0];
#ifndef CONFIG_SOC_EXYNOS8890
	si.fw_log_buf = si.sram + FW_LOG_ADDR;
#endif

	return 0;
err1:
	for (n = 0, iova = FWAREA_IOVA;
			n < FWAREA_NUM; n++, iova += FWAREA_SIZE) {
		iommu_unmap(si.domain, iova, FWAREA_SIZE);
	}
err0:
	for (n = 0; n < FWAREA_NUM; n++) {
		if (si.fwarea[n])
			dma_free_coherent(dev, FWAREA_SIZE,
					si.fwarea[n], si.fwarea_pa[n]);
	}
err:
	return -ENOMEM;
}
#endif

static void esa_fw_request_complete(const struct firmware *fw_sram, void *ctx)
{
	const struct firmware *fw_dram;
	struct device *dev = ctx;

	if (!fw_sram) {
		esa_err("Failed to requset firmware[%s]\n", FW_SRAM_NAME);
		return;
	}

	if (request_firmware(&fw_dram, FW_DRAM_NAME, dev)) {
		esa_err("Failed to requset firmware[%s]\n", FW_DRAM_NAME);
		return;
	}

	si.fwmem_loaded = true;
	si.fw_sbin_size = fw_sram->size;
	si.fw_dbin_size = fw_dram->size;

	memcpy(si.fwmem, fw_sram->data, si.fw_sbin_size);
	memcpy(si.fwmem + si.fw_sbin_size, fw_dram->data, si.fw_dbin_size);

	release_firmware(fw_dram);

	esa_info("FW Loaded (SRAM = %d, DRAM = %d)\n",
			si.fw_sbin_size, si.fw_dbin_size);

	return;
}

static const char banner[] =
	KERN_INFO "Exynos Seiren Audio driver, (c)2013 Samsung Electronics\n";

static int esa_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	struct sched_param param = { .sched_priority = 0 };
#endif

	int ret = 0;

	printk(banner);

	spin_lock_init(&si.lock);
#if defined(CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD)
	spin_lock_init(&si.cmd_lock);
	spin_lock_init(&si.compr_lock);
	si.is_compr_open = false;
#endif
	si.pdev = pdev;

	ret = misc_register(&esa_miscdev);
	if (ret) {
		esa_err("Cannot register miscdev\n");
		return -ENODEV;
	}

	si.regs = lpass_get_regs();
	if (!si.regs) {
		esa_err("Failed to get lpass regs\n");
		return -ENODEV;
	}

#ifndef CONFIG_SOC_EXYNOS8890
	si.sram = lpass_get_mem();
	if (!si.sram) {
		esa_err("Failed to get lpass sram\n");
		return -ENODEV;
	}
#endif

#ifdef CONFIG_SND_ESA_SA_EFFECT
#ifndef CONFIG_SOC_EXYNOS8890
	si.effect_ram = si.sram + EFFECT_OFFSET;
#endif
	si.out_sample_rate = 0;
#endif

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "Failed to get irq resource\n");
		return -ENXIO;
	}
	si.irq_ca5 = res->start;
	si.mailbox = si.regs + 0x80;

	si.clk_ca5 = clk_get(dev, "ca5");
	if (IS_ERR(si.clk_ca5)) {
		dev_err(dev, "ca5 clk not found\n");
		return -ENODEV;
	}

	if (np) {
		if (of_find_property(np, "samsung,lpass-subip", NULL))
			lpass_register_subip(dev, "ca5");
	}

#ifdef CONFIG_SND_SAMSUNG_IOMMU
	si.domain = lpass_get_iommu_domain();
	if (!si.domain) {
		dev_err(dev, "iommu not available\n");
		goto err;
	}

	/* prepare buffer */
	if (esa_prepare_buffer(dev))
		goto err;
#else
	dev_err(dev, "iommu not available\n");
	goto err;
#endif

	ret = request_firmware_nowait(THIS_MODULE,
				      FW_ACTION_HOTPLUG,
				      FW_SRAM_NAME,
				      dev,
				      GFP_KERNEL,
				      dev,
				      esa_fw_request_complete);
	if (ret) {
		dev_err(dev, "could not load firmware\n");
		goto err;
	} else {
		dev_err(dev, "load firmware success\n");
	}

	ret = request_irq(si.irq_ca5, esa_isr, 0, "lpass-ca5", 0);
	if (ret < 0) {
		esa_err("Failed to claim CA5 irq\n");
		goto err;
	}

#ifdef CONFIG_PROC_FS
	si.proc_file = proc_create("driver/seiren", 0, NULL, &esa_proc_fops);
	if (!si.proc_file)
		esa_info("Failed to register /proc/driver/seiren\n");
#endif
	/* hold reset */
	lpass_reset(LPASS_IP_CA5, LPASS_OP_RESET);

	esa_debug("regs       = %p\n", si.regs);
	esa_debug("mailbox    = %p\n", si.mailbox);
	esa_debug("bufmem_pa  = %p\n", (void*)si.bufmem_pa);
	esa_debug("fwmem_pa   = %p\n", (void*)si.fwmem_pa);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, 300);
	pm_runtime_enable(dev);
#else
	esa_do_resume(dev);
#endif
#ifdef CONFIG_SND_SAMSUNG_ELPE
	lpeff_init(&si);
	exynos_init_lpeffworker();
#endif
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	si.aud_cpu_lock_thrd = (struct task_struct *)
			kthread_run(esa_set_cpu_lock_kthread, NULL,
					"esa-set-cpu-lock");
	sched_setscheduler_nocheck(si.aud_cpu_lock_thrd, SCHED_NORMAL, &param);

	if (!seiren_fw_snapshot_kobj) {
		seiren_fw_snapshot_kobj	=
			kobject_create_and_add("snapshot-seiren-fw", NULL);
		if (sysfs_create_file(seiren_fw_snapshot_kobj, &seiren_fw_dump_attribute.attr))
			pr_err("%s: failed to create sysfs to control PCM dump\n", __func__);
	}

#endif
#ifdef CONFIG_PM_DEVFREQ
	si.mif_qos = 0;
	si.int_qos = 0;
	pm_qos_add_request(&si.ca5_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&si.ca5_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
#endif
	if (!dma_reg)
		dma_reg = ioremap(0x11420000, SZ_4K);
	return 0;

err:
	clk_put(si.clk_ca5);
	return -ENODEV;
}

static int esa_remove(struct platform_device *pdev)
{
	int ret = 0;

	free_irq(si.irq_ca5, 0);
	ret = misc_deregister(&esa_miscdev);
	if (ret)
		esa_err("Cannot deregister miscdev\n");

#ifndef CONFIG_PM_RUNTIME
	clk_disable_unprepare(si.clk_ca5);
	lpass_put_sync(&pdev->dev);
#endif
	clk_put(si.clk_ca5);
	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int esa_runtime_suspend(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_do_suspend(dev);
}

static int esa_runtime_resume(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_do_resume(dev);
}
#endif

static struct platform_device_id esa_driver_ids[] = {
	{
		.name	= "samsung-seiren",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, esa_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id exynos_esa_match[] = {
	{
		.compatible = "samsung,exynos5430-seiren",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_esa_match);
#endif

static const struct dev_pm_ops esa_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		esa_suspend,
		esa_resume
	)
	SET_RUNTIME_PM_OPS(
		esa_runtime_suspend,
		esa_runtime_resume,
		NULL
	)
};

static struct platform_driver esa_driver = {
	.probe		= esa_probe,
	.remove		= esa_remove,
	.id_table	= esa_driver_ids,
	.driver		= {
		.name	= "samsung-seiren",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_esa_match),
		.pm	= &esa_pmops,
	},
};

module_platform_driver(esa_driver);

MODULE_AUTHOR("Yongjin Jo <yjin.jo@samsung.com>, "
              "Yeongman Seo <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Exynos Seiren Audio driver");
MODULE_LICENSE("GPL");
