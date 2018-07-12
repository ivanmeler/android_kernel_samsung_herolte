/*
 * Synopsys DesignWare Multimedia Card Interface driver
 *  (Based on NXP driver for lpc 31xx)
 *
 * Copyright (C) 2009 NXP Semiconductors
 * Copyright (C) 2009, 2010 Imagination Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/blkdev.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/smc.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-powermode.h>

#include "dw_mmc.h"
#include "dw_mmc-exynos.h"

/* Common flag combinations */
#define DW_MCI_DATA_ERROR_FLAGS	(SDMMC_INT_DRTO | SDMMC_INT_DCRC | \
				 SDMMC_INT_HTO | SDMMC_INT_SBE  | \
				 SDMMC_INT_EBE)
#define DW_MCI_CMD_ERROR_FLAGS	(SDMMC_INT_RTO | SDMMC_INT_RCRC | \
				 SDMMC_INT_RESP_ERR)
#define DW_MCI_ERROR_FLAGS	(DW_MCI_DATA_ERROR_FLAGS | \
				 DW_MCI_CMD_ERROR_FLAGS  | SDMMC_INT_HLE)
#define DW_MCI_SEND_STATUS	1
#define DW_MCI_RECV_STATUS	2
#define DW_MCI_DMA_THRESHOLD	16

#define DW_MCI_FREQ_MAX	200000000	/* unit: HZ */
#define DW_MCI_FREQ_MIN	300000		/* unit: HZ */

#ifdef CONFIG_MMC_DW_IDMAC
#define IDMAC_INT_CLR		(SDMMC_IDMAC_INT_AI | SDMMC_IDMAC_INT_NI | \
				 SDMMC_IDMAC_INT_CES | SDMMC_IDMAC_INT_DU | \
				 SDMMC_IDMAC_INT_FBE | SDMMC_IDMAC_INT_RI | \
				 SDMMC_IDMAC_INT_TI)

struct idmac_desc_64addr {
	u32		des0;	/* Control Descriptor */

	u32		des1;	/* Reserved */

	u32		des2;	/*Buffer sizes */
#define IDMAC_64ADDR_SET_BUFFER1_SIZE(d, s) \
	((d)->des2 = ((d)->des2 & 0x03ffe000) | ((s) & 0x1fff))

	u32		des3;	/* Reserved */

	u32		des4;	/* Lower 32-bits of Buffer Address Pointer 1*/
	u32		des5;	/* Upper 32-bits of Buffer Address Pointer 1*/

	u32		des6;	/* Lower 32-bits of Next Descriptor Address */
	u32		des7;	/* Upper 32-bits of Next Descriptor Address */
#define IDMAC_64ADDR_SET_DESC_CLEAR(d) \
do {			\
	(d)->des1 = 0;	\
	(d)->des2 = 0;	\
	(d)->des3 = 0;	\
} while(0)
#define IDMAC_64ADDR_SET_DESC_ADDR(d, a) \
do {			\
	(d)->des6 = ((u32)(a) & 0xffffffff); \
	(d)->des7 = ((u32)((a) >> 32));	\
} while(0)
};

struct idmac_desc {
	u32		des0;	/* Control Descriptor */
#define IDMAC_DES0_DIC	BIT(1)
#define IDMAC_DES0_LD	BIT(2)
#define IDMAC_DES0_FD	BIT(3)
#define IDMAC_DES0_CH	BIT(4)
#define IDMAC_DES0_ER	BIT(5)
#define IDMAC_DES0_CES	BIT(30)
#define IDMAC_DES0_OWN	BIT(31)

	u32		des1;	/* Buffer sizes */
#define IDMAC_SET_BUFFER1_SIZE(d, s) \
	((d)->des1 = ((d)->des1 & 0x03ffe000) | ((s) & 0x1fff))

	u32		des2;	/* buffer 1 physical address */

	u32		des3;	/* buffer 2 physical address */
#define IDMAC_SET_DESC_ADDR(d, a) \
do {	\
	(d)->des3 = (u32)(a);	\
} while(0)
};
#endif /* CONFIG_MMC_DW_IDMAC */

#if defined(CONFIG_MMC_DW_DEBUG)
static struct dw_mci_debug_data dw_mci_debug __cacheline_aligned;

unsigned int dw_mci_debug_flag = 0;

#ifdef CONFIG_MMC_SUPPORT_STLOG
#include <linux/stlog.h>
#else
#define ST_LOG(fmt,...)
#endif

/* Add sysfs for read cmd_logs */
static ssize_t dw_mci_debug_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t total_len = 0;
	int j = 0, k = 0;
	struct dw_mci_cmd_log *cmd_log;
	unsigned int offset;

	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct dw_mci *host = dw_mci_debug.host[mmc->index];

	/*
	 * print cmd_log from prev. 14 to last
	 */
	 if (host->debug_info->en_logging & DW_MCI_DEBUG_ON_CMD) {
		 offset = atomic_read(&host->debug_info->cmd_log_count) - 13;
		 offset &= DWMCI_LOG_MAX - 1;
		 total_len += snprintf(buf, PAGE_SIZE, "HOST%1d\n", mmc->index);
		 buf += (sizeof(char) * 6);
		 cmd_log = host->debug_info->cmd_log;
		 for (j = 0; j < 14; j++) {
			 total_len += snprintf(buf+(sizeof(char)*71*j)+
				 (sizeof(char)*(2*k+6*(k+1))), PAGE_SIZE,
				 "%04d:%2d,0x%08x,%04d,%016llu,%016llu,%02x,%04x,%03d.\n",
				 offset,
				 cmd_log[offset].cmd, cmd_log[offset].arg,
				 cmd_log[offset].data_size, cmd_log[offset].send_time,
				 cmd_log[offset].done_time, cmd_log[offset].seq_status,
				 cmd_log[offset].rint_sts, cmd_log[offset].status_count);
			 offset++;
		 }
		 total_len += snprintf(buf + (sizeof(char)*2), PAGE_SIZE, "\n\n");
		 k++;
	 }

	return total_len;
}

static ssize_t dw_mci_debug_log_control(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t len)
{
	int enable = 0;
	int ret = 0;
	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct dw_mci *host = dw_mci_debug.host[mmc->index];

	ret = kstrtoint(buf, 0, &enable);
	if (ret)
		goto out;

	host->debug_info->en_logging = enable;
	printk("%s: en_logging is %d.\n",
			mmc_hostname(host->cur_slot->mmc),
			host->debug_info->en_logging);

 out:
	return len;
}
static DEVICE_ATTR(dwmci_debug, 0644, dw_mci_debug_log_show, dw_mci_debug_log_control);

/*
 * new_cmd : has to be true Only send_command.(except CMD13)
 * flags :
 * 0x1 : send_cmd : start_command(all)
 * 0x2 : resp(CD) : set done_time without data case
 * 0x4 : data_done(DTO) : set done_time with data case
 * 0x8 : error interrupt occurs : set rint_sts read from RINTSTS
 */
static void dw_mci_debug_cmd_log(struct mmc_command *cmd, struct dw_mci *host,
		bool new_cmd, u8 flags, u32 rintsts)
{
	int cpu = raw_smp_processor_id();
	unsigned int count;
	struct dw_mci_cmd_log *cmd_log;

	if (!host->debug_info || !(host->debug_info->en_logging & DW_MCI_DEBUG_ON_CMD))
		return;

	cmd_log = host->debug_info->cmd_log;

	if (!new_cmd) {
		count = atomic_read(&host->debug_info->cmd_log_count) &
							(DWMCI_LOG_MAX - 1);
		if (flags & DW_MCI_FLAG_SEND_CMD)	/* CMD13 */
			cmd_log[count].status_count++;
		if (flags & DW_MCI_FLAG_CD) {
			cmd_log[count].seq_status |= DW_MCI_FLAG_CD;
			cmd_log[count].done_time = cpu_clock(cpu);
		}
		if (flags & DW_MCI_FLAG_DTO) {
			cmd_log[count].seq_status |= DW_MCI_FLAG_DTO;
			cmd_log[count].done_time = cpu_clock(cpu);
		}
		if (flags & DW_MCI_FLAG_ERROR) {
			cmd_log[count].seq_status |= DW_MCI_FLAG_ERROR;
			cmd_log[count].rint_sts |= (rintsts & 0xFFFF);
		}
	} else {
		count = atomic_inc_return(&host->debug_info->cmd_log_count) &
							(DWMCI_LOG_MAX - 1);
		cmd_log[count].cmd = cmd->opcode;
		cmd_log[count].arg = cmd->arg;
		if (cmd->data)
			cmd_log[count].data_size = cmd->data->blocks;
		else
			cmd_log[count].data_size = 0;

		cmd_log[count].send_time = cpu_clock(cpu);

		cmd_log[count].done_time = 0x0;
		cmd_log[count].seq_status = DW_MCI_FLAG_SEND_CMD;
		if (!flags & DW_MCI_FLAG_SEND_CMD)
			cmd_log[count].seq_status |= DW_MCI_FLAG_NEW_CMD_ERR;

		cmd_log[count].rint_sts = 0x0;
		cmd_log[count].status_count = 0;
	}
}

static void dw_mci_debug_req_log(struct dw_mci *host, struct mmc_request *mrq,
		enum dw_mci_req_log_state log_state, enum dw_mci_state state)
{
	int cpu = raw_smp_processor_id();
	unsigned int count;
	struct dw_mci_req_log *req_log;

	if (!host->debug_info || !(host->debug_info->en_logging & DW_MCI_DEBUG_ON_REQ))
		return;

	req_log = host->debug_info->req_log;

	count = atomic_inc_return(&host->debug_info->req_log_count)
					& (DWMCI_REQ_LOG_MAX - 1);
	if (log_state == STATE_REQ_START) {
		req_log[count].info0 = mrq->cmd->opcode;
		req_log[count].info1 = mrq->cmd->arg;
		if (mrq->data) {
			req_log[count].info2 = (u32)mrq->data->blksz;
			req_log[count].info3 = (u32)mrq->data->blocks;
		} else {
			req_log[count].info2 = 0;
			req_log[count].info3 = 0;
		}
	} else {
		req_log[count].info0 = host->cmd_status;
		req_log[count].info1 = host->data_status;
		req_log[count].info2 = 0;
		req_log[count].info3 = 0;
	}
	req_log[count].log_state = log_state;
	req_log[count].pending_events = host->pending_events;
	req_log[count].completed_events = host->completed_events;
	req_log[count].timestamp = cpu_clock(cpu);
	req_log[count].state = state;
}

static void dw_mci_debug_init(struct dw_mci *host)
{
	unsigned int host_index;
	unsigned int info_index;

	host_index = dw_mci_debug.host_count++;
	if (host_index < DWMCI_DBG_NUM_HOST) {
		dw_mci_debug.host[host_index] = host;
		if (DWMCI_DBG_MASK_INFO & DWMCI_DBG_BIT_HOST(host_index)) {
			static atomic_t temp_cmd_log_count = ATOMIC_INIT(-1);
			static atomic_t temp_req_log_count = ATOMIC_INIT(-1);
			int sysfs_err = 0;

			info_index = dw_mci_debug.info_count++;
			dw_mci_debug.info_index[host_index] = info_index;
			host->debug_info = &dw_mci_debug.debug_info[info_index];
			host->debug_info->en_logging = DW_MCI_DEBUG_ON_CMD
					| DW_MCI_DEBUG_ON_REQ;
			host->debug_info->cmd_log_count = temp_cmd_log_count;
			host->debug_info->req_log_count = temp_req_log_count;

			sysfs_err = sysfs_create_file(&(host->slot[0]->mmc->class_dev.kobj),
						&(dev_attr_dwmci_debug.attr));
			pr_info("%s: create debug_log sysfs : %s.....\n", __func__,
					sysfs_err ? "failed" : "successed");
			dev_info(host->dev, "host %d debug On\n", host_index);
		} else {
			dw_mci_debug.info_index[host_index] = 0xFF;
		}
	}
}
#else
static inline int dw_mci_debug_cmd_log(struct mmc_command *cmd,
		struct dw_mci *host, bool new_cmd, u8 flags, u32 rintsts)
{
	return 0;
}

static inline int dw_mci_debug_req_log(struct dw_mci *host,
		struct mmc_request *mrq, enum dw_mci_req_log_state log_state,
		enum dw_mci_state state)
{
	return 0;
}

static inline int dw_mci_debug_init(struct dw_mci *host)
{
	return 0;
}
#endif /* defined (CONFIG_MMC_DW_DEBUG) */

/* Add sysfs for argos */
static ssize_t dw_mci_transferred_cnt_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;

	return sprintf(buf, "%u\n" , host->transferred_cnt);
}

DEVICE_ATTR(trans_count, 0444, dw_mci_transferred_cnt_show, NULL);

static void dw_mci_transferred_cnt_init(struct dw_mci *host, struct mmc_host *mmc)
{
	int sysfs_err = 0;
	sysfs_err = sysfs_create_file(&(mmc->class_dev.kobj),
			&(dev_attr_trans_count.attr));
	pr_info("%s: trans_count: %s.....\n", __func__,
			sysfs_err ? "failed" : "successed");
}

static int dw_mci_ciu_clk_en(struct dw_mci *host, bool force_gating)
{
	int ret = 0;

	if (!host->pdata->use_gate_clock && !force_gating)
		return 0;

	if (!host->ciu_clk) {
		dev_err(host->dev, "no available CIU gating clock\n");
		return 1;
	}

	if (!atomic_cmpxchg(&host->ciu_clk_cnt, 0, 1)) {
		ret = clk_prepare_enable(host->ciu_clk);
		if (ret)
			dev_err(host->dev, "failed to enable ciu clock\n");
	}

	return ret;
}

static void dw_mci_ciu_clk_dis(struct dw_mci *host)
{

	if (!host->pdata->use_gate_clock)
		return;

	if (host->pdata->enable_cclk_on_suspend && host->pdata->on_suspend)
		return;

	if (atomic_read(&host->ciu_en_win)) {
		dev_err(host->dev, "Not available CIU off: %d\n",
				atomic_read(&host->ciu_en_win));
		return;
	}

	if (host->req_state == DW_MMC_REQ_BUSY)
		return;

	if (atomic_cmpxchg(&host->ciu_clk_cnt, 1, 0))
		clk_disable_unprepare(host->ciu_clk);
}

static int dw_mci_biu_clk_en(struct dw_mci *host, bool force_gating)
{
	int ret = 0;

	if (!host->pdata->use_biu_gate_clock && !force_gating)
		return 0;

	if (!atomic_read(&host->biu_clk_cnt)) {
		ret = clk_prepare_enable(host->biu_clk);
		atomic_inc_return(&host->biu_clk_cnt);
		if (ret)
			dev_err(host->dev, "failed to enable biu clock\n");
	}

	return ret;
}

static void dw_mci_biu_clk_dis(struct dw_mci *host)
{
	if (!host->pdata->use_biu_gate_clock)
		return;

	if (host->pdata->enable_cclk_on_suspend && host->pdata->on_suspend)
		return;

	if (atomic_read(&host->biu_en_win)) {
		dev_dbg(host->dev, "Not available BIU off: %d\n",
				atomic_read(&host->biu_en_win));
		return;
	}

	if (host->req_state == DW_MMC_REQ_BUSY)
		return;

	if (atomic_read(&host->biu_clk_cnt)) {
		clk_disable_unprepare(host->biu_clk);
		atomic_dec_return(&host->biu_clk_cnt);
	}
}

bool dw_mci_fifo_reset(struct device *dev, struct dw_mci *host);
void dw_mci_ciu_reset(struct device *dev, struct dw_mci *host);
static bool dw_mci_ctrl_reset(struct dw_mci *host, u32 reset);

#if defined(CONFIG_DEBUG_FS)
static int dw_mci_req_show(struct seq_file *s, void *v)
{
	struct dw_mci_slot *slot = s->private;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_command *stop;
	struct mmc_data	*data;

	/* Make sure we get a consistent snapshot */
	spin_lock_bh(&slot->host->lock);
	mrq = slot->mrq;

	if (mrq) {
		cmd = mrq->cmd;
		data = mrq->data;
		stop = mrq->stop;

		if (cmd)
			seq_printf(s,
				   "CMD%u(0x%x) flg %x rsp %x %x %x %x err %d\n",
				   cmd->opcode, cmd->arg, cmd->flags,
				   cmd->resp[0], cmd->resp[1], cmd->resp[2],
				   cmd->resp[2], cmd->error);
		if (data)
			seq_printf(s, "DATA %u / %u * %u flg %x err %d\n",
				   data->bytes_xfered, data->blocks,
				   data->blksz, data->flags, data->error);
		if (stop)
			seq_printf(s,
				   "CMD%u(0x%x) flg %x rsp %x %x %x %x err %d\n",
				   stop->opcode, stop->arg, stop->flags,
				   stop->resp[0], stop->resp[1], stop->resp[2],
				   stop->resp[2], stop->error);
	}

	spin_unlock_bh(&slot->host->lock);

	return 0;
}

static int dw_mci_req_open(struct inode *inode, struct file *file)
{
	return single_open(file, dw_mci_req_show, inode->i_private);
}

static const struct file_operations dw_mci_req_fops = {
	.owner		= THIS_MODULE,
	.open		= dw_mci_req_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dw_mci_regs_show(struct seq_file *s, void *v)
{
	seq_printf(s, "STATUS:\t0x%08x\n", SDMMC_STATUS);
	seq_printf(s, "RINTSTS:\t0x%08x\n", SDMMC_RINTSTS);
	seq_printf(s, "CMD:\t0x%08x\n", SDMMC_CMD);
	seq_printf(s, "CTRL:\t0x%08x\n", SDMMC_CTRL);
	seq_printf(s, "INTMASK:\t0x%08x\n", SDMMC_INTMASK);
	seq_printf(s, "CLKENA:\t0x%08x\n", SDMMC_CLKENA);

	return 0;
}

static int dw_mci_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, dw_mci_regs_show, inode->i_private);
}

static const struct file_operations dw_mci_regs_fops = {
	.owner		= THIS_MODULE,
	.open		= dw_mci_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void dw_mci_init_debugfs(struct dw_mci_slot *slot)
{
	struct mmc_host	*mmc = slot->mmc;
	struct dw_mci *host = slot->host;
	struct dentry *root;
	struct dentry *node;

	root = mmc->debugfs_root;
	if (!root)
		return;

	node = debugfs_create_file("regs", S_IRUSR, root, host,
				   &dw_mci_regs_fops);
	if (!node)
		goto err;

	node = debugfs_create_file("req", S_IRUSR, root, slot,
				   &dw_mci_req_fops);
	if (!node)
		goto err;

	node = debugfs_create_u32("state", S_IRUSR, root, (u32 *)&host->state);
	if (!node)
		goto err;

	node = debugfs_create_x32("pending_events", S_IRUSR, root,
				  (u32 *)&host->pending_events);
	if (!node)
		goto err;

	node = debugfs_create_x32("completed_events", S_IRUSR, root,
				  (u32 *)&host->completed_events);
	if (!node)
		goto err;

	return;

err:
	dev_err(&mmc->class_dev, "failed to initialize debugfs for slot\n");
}
#endif /* defined(CONFIG_DEBUG_FS) */

static void mci_send_cmd(struct dw_mci_slot *slot, u32 cmd, u32 arg);

u32 dw_mci_disable_interrupt(struct dw_mci *host, unsigned int *int_mask)
{
	u32 ctrl;

	ctrl = mci_readl(host, CTRL);
	ctrl &= ~(SDMMC_CTRL_INT_ENABLE);
	mci_writel(host, CTRL, ctrl);

	*int_mask = mci_readl(host, INTMASK);

	mci_writel(host, INTMASK, 0);

	return ctrl;
}

void dw_mci_enable_interrupt(struct dw_mci *host, unsigned int int_mask)
{
	unsigned int ctrl;
	mci_writel(host, INTMASK, int_mask);

	ctrl = mci_readl(host, CTRL);
	mci_writel(host, CTRL, ctrl | SDMMC_CTRL_INT_ENABLE);
}

static void dw_mci_update_clock(struct dw_mci_slot *slot)
{
	struct dw_mci *host = slot->host;
	unsigned long timeout;
	int retry = 10;
	unsigned int int_mask = 0;
	unsigned int cmd_status = 0;

	atomic_inc_return(&slot->host->ciu_en_win);
	dw_mci_ciu_clk_en(slot->host, false);
	atomic_dec_return(&slot->host->ciu_en_win);

	dw_mci_disable_interrupt(host, &int_mask);

	do {
		wmb();
		mci_writel(host, CMD, SDMMC_CMD_START | SDMMC_CMD_UPD_CLK);

		timeout = jiffies + msecs_to_jiffies(1);
		while (time_before(jiffies, timeout)) {
			cmd_status = mci_readl(host, CMD) & SDMMC_CMD_START;
			if (!cmd_status)
				goto out;

			if (mci_readl(host, RINTSTS) & SDMMC_INT_HLE) {
				mci_writel(host, RINTSTS, SDMMC_INT_HLE);
				break;
				/* reset controller because a command is stuecked */
			}
		}

		dw_mci_ctrl_reset(host, SDMMC_CTRL_RESET);
	} while (--retry);

	dev_err(&slot->mmc->class_dev,
			"Timeout updating command (status %#x)\n", cmd_status);

out:
	/* recover interrupt mask after updating clock */
	dw_mci_enable_interrupt(host, int_mask);
}

static inline bool dw_mci_stop_abort_cmd(struct mmc_command *cmd)
{
	u32 op = cmd->opcode;

	if ((op == MMC_STOP_TRANSMISSION) ||
	    (op == MMC_GO_IDLE_STATE) ||
	    (op == MMC_GO_INACTIVE_STATE) ||
	    ((op == SD_IO_RW_DIRECT) && (cmd->arg & 0x80000000) &&
	     ((cmd->arg >> 9) & 0x1FFFF) == SDIO_CCCR_ABORT))
		return true;
	return false;
}

static u32 dw_mci_prepare_command(struct mmc_host *mmc, struct mmc_command *cmd)
{
	struct mmc_data	*data;
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	const struct dw_mci_drv_data *drv_data = slot->host->drv_data;
	u32 cmdr;
	cmd->error = -EINPROGRESS;

	cmdr = cmd->opcode;

	if (cmd->opcode == MMC_STOP_TRANSMISSION ||
	    cmd->opcode == MMC_GO_IDLE_STATE ||
	    cmd->opcode == MMC_GO_INACTIVE_STATE ||
	    (cmd->opcode == SD_IO_RW_DIRECT &&
	     ((cmd->arg >> 9) & 0x1FFFF) == SDIO_CCCR_ABORT))
		cmdr |= SDMMC_CMD_STOP;
	else if (cmd->opcode != MMC_SEND_STATUS && cmd->data)
		cmdr |= SDMMC_CMD_PRV_DAT_WAIT;

	if (cmd->opcode == SD_SWITCH_VOLTAGE) {
		u32 clk_en_a;

		/* Change state to continue to handle CMD11 weirdness */
		WARN_ON(slot->host->state != STATE_SENDING_CMD);
		slot->host->state = STATE_SENDING_CMD11;

		/*
		 * We need to disable low power mode (automatic clock stop)
		 * while doing voltage switch so we don't confuse the card,
		 * since stopping the clock is a specific part of the UHS
		 * voltage change dance.
		 *
		 * Note that low power mode (SDMMC_CLKEN_LOW_PWR) will be
		 * unconditionally turned back on in dw_mci_setup_bus() if it's
		 * ever called with a non-zero clock.  That shouldn't happen
		 * until the voltage change is all done.
		 */
		clk_en_a = mci_readl(host, CLKENA);
		clk_en_a &= ~(SDMMC_CLKEN_LOW_PWR << slot->id);
		mci_writel(host, CLKENA, clk_en_a);
		dw_mci_update_clock(slot);
	}

	if (cmd->flags & MMC_RSP_PRESENT) {
		/* We expect a response, so set this bit */
		cmdr |= SDMMC_CMD_RESP_EXP;
		if (cmd->flags & MMC_RSP_136)
			cmdr |= SDMMC_CMD_RESP_LONG;
	}

	if (cmd->flags & MMC_RSP_CRC)
		cmdr |= SDMMC_CMD_RESP_CRC;

	data = cmd->data;
	if (data) {
		cmdr |= SDMMC_CMD_DAT_EXP;
		if (data->flags & MMC_DATA_STREAM)
			cmdr |= SDMMC_CMD_STRM_MODE;
		if (data->flags & MMC_DATA_WRITE)
			cmdr |= SDMMC_CMD_DAT_WR;
	}

	if (drv_data && drv_data->prepare_command)
		drv_data->prepare_command(slot->host, &cmdr);

	return cmdr;
}

static u32 dw_mci_prep_stop_abort(struct dw_mci *host, struct mmc_command *cmd)
{
	struct mmc_command *stop;
	u32 cmdr;

	if (!cmd->data)
		return 0;

	stop = &host->stop_abort;
	cmdr = cmd->opcode;
	memset(stop, 0, sizeof(struct mmc_command));

	if (cmdr == MMC_READ_SINGLE_BLOCK ||
	    cmdr == MMC_READ_MULTIPLE_BLOCK ||
	    cmdr == MMC_WRITE_BLOCK ||
	    cmdr == MMC_WRITE_MULTIPLE_BLOCK ||
	    cmdr == MMC_SEND_TUNING_BLOCK ||
	    cmdr == MMC_SEND_TUNING_BLOCK_HS200) {
		stop->opcode = MMC_STOP_TRANSMISSION;
		stop->arg = 0;
		stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	} else if (cmdr == SD_IO_RW_EXTENDED) {
		stop->opcode = SD_IO_RW_DIRECT;
		stop->arg |= (1 << 31) | (0 << 28) | (SDIO_CCCR_ABORT << 9) |
			     ((cmd->arg >> 28) & 0x7);
		stop->flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC;
	} else {
		return 0;
	}

	cmdr = stop->opcode | SDMMC_CMD_STOP |
		SDMMC_CMD_RESP_CRC | SDMMC_CMD_RESP_EXP;

	return cmdr;
}

static void dw_mci_start_command(struct dw_mci *host,
				 struct mmc_command *cmd, u32 cmd_flags)
{
	host->cmd = cmd;

	dev_vdbg(host->dev,
		"start command: ARGR=0x%08x CMDR=0x%08x\n",
		cmd->arg, cmd_flags);

	/* needed to
	 * add get node parse_dt for check to enable logging
	 * if defined(CMD_LOGGING)
	 * set en_logging to true
	 * init cmd_log_count
	 */
	if (cmd->opcode == MMC_SEND_STATUS)
		dw_mci_debug_cmd_log(cmd, host, false, DW_MCI_FLAG_SEND_CMD, 0);
	else
		dw_mci_debug_cmd_log(cmd, host, true, DW_MCI_FLAG_SEND_CMD, 0);

	mci_writel(host, CMDARG, cmd->arg);
	wmb();

	mci_writel(host, CMD, cmd_flags | SDMMC_CMD_START);
}

static inline void send_stop_abort(struct dw_mci *host, struct mmc_data *data)
{
	struct mmc_command *stop = data->stop ? data->stop : &host->stop_abort;
	dw_mci_start_command(host, stop, host->stop_cmdr);
}

/* DMA interface functions */
static void dw_mci_stop_dma(struct dw_mci *host)
{
	if (host->using_dma) {
		host->dma_ops->stop(host);
		host->dma_ops->cleanup(host);
		host->dma_ops->reset(host);
	}

	/* Data transfer was stopped by the interrupt handler */
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static int dw_mci_get_dma_dir(struct mmc_data *data)
{
	if (data->flags & MMC_DATA_WRITE)
		return DMA_TO_DEVICE;
	else
		return DMA_FROM_DEVICE;
}

#ifdef CONFIG_MMC_DW_IDMAC
static void dw_mci_dma_cleanup(struct dw_mci *host)
{
	struct mmc_data *data = host->data;

	if (data)
		if (!data->host_cookie)
			dma_unmap_sg(host->dev,
				     data->sg,
				     data->sg_len,
				     dw_mci_get_dma_dir(data));
}

static void dw_mci_idmac_reset(struct dw_mci *host)
{
	u32 bmod = mci_readl(host, BMOD);
	/* Software reset of DMA */
	bmod |= SDMMC_IDMAC_SWRESET;
	mci_writel(host, BMOD, bmod);
}

static void dw_mci_idmac_stop_dma(struct dw_mci *host)
{
	u32 temp;

	/* Disable and reset the IDMAC interface */
	temp = mci_readl(host, CTRL);
	temp &= ~SDMMC_CTRL_USE_IDMAC;
	mci_writel(host, CTRL, temp);

	/* reset the IDMAC interface */
	dw_mci_ctrl_reset(host, SDMMC_CTRL_DMA_RESET);

	/* Stop the IDMAC running */
	temp = mci_readl(host, BMOD);
	temp &= ~(SDMMC_IDMAC_ENABLE | SDMMC_IDMAC_FB);
	temp |= SDMMC_IDMAC_SWRESET;
	mci_writel(host, BMOD, temp);

}

static void dw_mci_idma_reset_dma(struct dw_mci *host)
{
	u32 temp;

	temp = mci_readl(host, BMOD);
	/* Software reset of DMA */
	temp |= SDMMC_IDMAC_SWRESET;
	mci_writel(host, BMOD, temp);
}

static void dw_mci_idmac_complete_dma(struct dw_mci *host)
{
	struct mmc_data *data = host->data;

	dev_vdbg(host->dev, "DMA complete\n");

	host->dma_ops->cleanup(host);

	/*
	 * If the card was removed, data will be NULL. No point in trying to
	 * send the stop command or waiting for NBUSY in this case.
	 */
	if (data) {
		set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
		tasklet_schedule(&host->tasklet);
	}
}

static void dw_mci_translate_sglist(struct dw_mci *host, struct mmc_data *data,
				    unsigned int sg_len)
{
	int i;
	if (host->dma_64bit_address == true) {
		struct idmac_desc_64addr *desc = host->sg_cpu;

		for (i = 0; i < sg_len; i++, desc++) {
			unsigned int length = sg_dma_len(&data->sg[i]);
			u64 mem_addr = sg_dma_address(&data->sg[i]);

			/*
			 * Set the OWN bit and disable interrupts for this
			 * descriptor
			 */
			desc->des0 = IDMAC_DES0_OWN | IDMAC_DES0_DIC |
						IDMAC_DES0_CH;
			/* Buffer length */
			IDMAC_64ADDR_SET_BUFFER1_SIZE(desc, length);

			/* Physical address to DMA to/from */
			desc->des4 = mem_addr & 0xffffffff;
			desc->des5 = mem_addr >> 32;
		}

		/* Set first descriptor */
		desc = host->sg_cpu;
		desc->des0 |= IDMAC_DES0_FD;

		/* Set last descriptor */
		desc = host->sg_cpu + (i - 1) *
				sizeof(struct idmac_desc_64addr);
		desc->des0 &= ~(IDMAC_DES0_CH | IDMAC_DES0_DIC);
		desc->des0 |= IDMAC_DES0_LD;

	} else {
		struct idmac_desc *desc = host->sg_cpu;

		for (i = 0; i < sg_len; i++, desc++) {
			unsigned int length = sg_dma_len(&data->sg[i]);
			u32 mem_addr = sg_dma_address(&data->sg[i]);

			/*
			 * Set the OWN bit and disable interrupts for this
			 * descriptor
			 */
			desc->des0 = IDMAC_DES0_OWN | IDMAC_DES0_DIC |
						IDMAC_DES0_CH;
			/* Buffer length */
			IDMAC_SET_BUFFER1_SIZE(desc, length);

			/* Physical address to DMA to/from */
			desc->des2 = mem_addr;
		}

		/* Set first descriptor */
		desc = host->sg_cpu;
		desc->des0 |= IDMAC_DES0_FD;

		/* Set last descriptor */
		desc = host->sg_cpu + (i - 1) * sizeof(struct idmac_desc);
		desc->des0 &= ~(IDMAC_DES0_CH | IDMAC_DES0_DIC);
		desc->des0 |= IDMAC_DES0_LD;
	}

	wmb();
}

static void dw_mci_idmac_start_dma(struct dw_mci *host, unsigned int sg_len)
{
	u32 temp;

	dw_mci_translate_sglist(host, host->data, sg_len);

	/* Select IDMAC interface */
	temp = mci_readl(host, CTRL);
	temp |= SDMMC_CTRL_USE_IDMAC;
	mci_writel(host, CTRL, temp);

	wmb();

	/* Enable the IDMAC */
	temp = mci_readl(host, BMOD);
	temp |= SDMMC_IDMAC_ENABLE | SDMMC_IDMAC_FB;
	mci_writel(host, BMOD, temp);

	/* Start it running */
	mci_writel(host, PLDMND, 1);
}

static int dw_mci_idmac_init(struct dw_mci *host)
{
	int i;
	dma_addr_t addr;

	if (host->dma_64bit_address == true) {
		struct idmac_desc_64addr *p;
		/* Number of descriptors in the ring buffer */
		host->ring_size = host->desc_sz * PAGE_SIZE / sizeof(struct idmac_desc_64addr);

		/* Forward link the descriptor list */
		for (i = 0, p = host->sg_cpu; i < host->ring_size - 1;
								i++, p++) {
			addr = host->sg_dma + (sizeof(struct idmac_desc_64addr) *
					(i + 1));
			IDMAC_64ADDR_SET_DESC_ADDR(p,addr);
			IDMAC_64ADDR_SET_DESC_CLEAR(p);
		}

		/* Set the last descriptor as the end-of-ring descriptor */
		IDMAC_64ADDR_SET_DESC_ADDR(p, host->sg_dma);
		p->des0 = IDMAC_DES0_ER;

	} else {
		struct idmac_desc *p;
		/* Number of descriptors in the ring buffer */
		host->ring_size = host->desc_sz * PAGE_SIZE / sizeof(struct idmac_desc);

		/* Forward link the descriptor list */
		for (i = 0, p = host->sg_cpu; i < host->ring_size - 1; i++, p++) {
			addr = host->sg_dma + (sizeof(struct idmac_desc) *
								(i + 1));
			IDMAC_SET_DESC_ADDR(p, addr);
		}

		/* Set the last descriptor as the end-of-ring descriptor */
		IDMAC_SET_DESC_ADDR(p, host->sg_dma);
		p->des0 = IDMAC_DES0_ER;
	}

	dw_mci_idmac_reset(host);

	if (host->dma_64bit_address == true) {
		/* Mask out interrupts - get Tx & Rx complete only */
		mci_writel(host, IDSTS64, IDMAC_INT_CLR);
		mci_writel(host, IDINTEN64, SDMMC_IDMAC_INT_NI |
				SDMMC_IDMAC_INT_RI | SDMMC_IDMAC_INT_TI);

		/* Set the descriptor base address */
		mci_writel(host, DBADDRL, host->sg_dma & 0xffffffff);
		mci_writel(host, DBADDRU, (u64)host->sg_dma >> 32);

	} else {
		/* Mask out interrupts - get Tx & Rx complete only */
		mci_writel(host, IDSTS, IDMAC_INT_CLR);
		mci_writel(host, IDINTEN, SDMMC_IDMAC_INT_NI |
				SDMMC_IDMAC_INT_RI | SDMMC_IDMAC_INT_TI);

		/* Set the descriptor base address */
		mci_writel(host, DBADDR, host->sg_dma);
	}

	return 0;
}

static const struct dw_mci_dma_ops dw_mci_idmac_ops = {
	.init = dw_mci_idmac_init,
	.start = dw_mci_idmac_start_dma,
	.stop = dw_mci_idmac_stop_dma,
	.reset = dw_mci_idma_reset_dma,
	.complete = dw_mci_idmac_complete_dma,
	.cleanup = dw_mci_dma_cleanup,
};
#endif /* CONFIG_MMC_DW_IDMAC */


static void dw_mci_sfr_save(struct dw_mci *host, unsigned int *sfr_backup)
{
	sfr_backup[0] = mci_readl(host, CTRL);
	sfr_backup[1] = mci_readl(host, PWREN);
	sfr_backup[2] = mci_readl(host, CLKDIV);
	sfr_backup[3] = mci_readl(host, CLKSRC);
	sfr_backup[4] = mci_readl(host, CLKENA);
	sfr_backup[5] = mci_readl(host, TMOUT);
	sfr_backup[6] = mci_readl(host, CTYPE);
	sfr_backup[7] = mci_readl(host, BLKSIZ);
	sfr_backup[8] = mci_readl(host, BYTCNT);
	sfr_backup[9] = mci_readl(host, INTMASK);
	sfr_backup[10] = mci_readl(host, FIFOTH);
	sfr_backup[11] = mci_readl(host, UHS_REG);
	sfr_backup[12] = mci_readl(host, BMOD);
	sfr_backup[13] = mci_readl(host, DBADDRL);
	sfr_backup[14] = mci_readl(host, DBADDRU);
	sfr_backup[15] = mci_readl(host, IDINTEN64);
	sfr_backup[16] = mci_readl(host, CLKSEL);
	sfr_backup[17] = mci_readl(host, RESP_TAT);
	sfr_backup[18] = mci_readl(host, FORCE_CLK_STOP);
	sfr_backup[19] = mci_readl(host, AXI_BURST_LEN);
	sfr_backup[20] = mci_readl(host, CDTHRCTL);
	sfr_backup[21] = mci_readl(host, EMMC_DDR_REG);
	sfr_backup[22] = mci_readl(host, DDR200_ENABLE_SHIFT);
	sfr_backup[23] = mci_readl(host, DDR200_RDDQS_EN);
	sfr_backup[24] = mci_readl(host, DDR200_ASYNC_FIFO_CTRL);
	sfr_backup[25] = mci_readl(host, DDR200_DLINE_CTRL);
	sfr_backup[26] = mci_readl(host, SHA_CMD_IE);
	sfr_backup[27] = mci_readl(host, SHA_CMD_IS);
	sfr_backup[28] = mci_readl(host, CMD);
}

static void dw_mci_sfr_restore(struct dw_mci *host, unsigned int *sfr_backup)
{
	struct dw_mci_slot *slot = host->cur_slot;
	const struct dw_mci_drv_data *drv_data = host->drv_data;

	dw_mci_update_clock(slot);
	mci_writel(host, CTRL, sfr_backup[0]);
	mci_writel(host, PWREN, sfr_backup[1]);
	mci_writel(host, CLKDIV, sfr_backup[2]);
	mci_writel(host, CLKSRC, sfr_backup[3]);
	mci_writel(host, CLKENA, sfr_backup[4]);
	mci_writel(host, TMOUT, sfr_backup[5]);
	mci_writel(host, CTYPE, sfr_backup[6]);
	mci_writel(host, BLKSIZ, sfr_backup[7]);
	mci_writel(host, BYTCNT, sfr_backup[8]);
	mci_writel(host, INTMASK, sfr_backup[9]);
	mci_writel(host, FIFOTH, sfr_backup[10]);
	mci_writel(host, UHS_REG, sfr_backup[11]);
	mci_writel(host, BMOD, sfr_backup[12]);
	mci_writel(host, DBADDRL, sfr_backup[13]);
	mci_writel(host, DBADDRU, sfr_backup[14]);
	mci_writel(host, IDINTEN64, sfr_backup[15]);
	mci_writel(host, CLKSEL, sfr_backup[16]);
	mci_writel(host, RESP_TAT, sfr_backup[17]);
	mci_writel(host, FORCE_CLK_STOP, sfr_backup[18]);
	mci_writel(host, AXI_BURST_LEN, sfr_backup[19]);
	mci_writel(host, CDTHRCTL, sfr_backup[20]);
	mci_writel(host, EMMC_DDR_REG, sfr_backup[21]);
	mci_writel(host, DDR200_ENABLE_SHIFT, sfr_backup[22]);
	mci_writel(host, DDR200_RDDQS_EN, sfr_backup[23]);
	mci_writel(host, DDR200_ASYNC_FIFO_CTRL, sfr_backup[24]);
	mci_writel(host, DDR200_DLINE_CTRL, sfr_backup[25]);
	mci_writel(host, SHA_CMD_IE, sfr_backup[26]);
	mci_writel(host, SHA_CMD_IS, sfr_backup[27]);
	mci_writel(host, CMD, sfr_backup[28]);
	if (drv_data && drv_data->cfg_smu)
		drv_data->cfg_smu(host);
}

static void dw_mci_force_reset(struct dw_mci *host)
{
	unsigned int sfr_backup[30] = {0,};

	dw_mci_sfr_save(host, sfr_backup);
	mci_writel(host, AXI_BURST_LEN, FORCE_SWRESET);
	dw_mci_sfr_restore(host, sfr_backup);
	dw_mci_idma_reset_dma(host);
	dw_mci_fifo_reset(host->dev, host);
	dw_mci_ciu_reset(host->dev, host);
}

static int dw_mci_pre_dma_transfer(struct dw_mci *host,
				   struct mmc_data *data,
				   bool next)
{
	struct scatterlist *sg;
	struct dw_mci_slot *slot = host->cur_slot;
	struct mmc_card *card = slot->mmc->card;
	unsigned int i, sg_len;
	unsigned int align_mask = ((host->data_shift == 3) ? 8 : 4) - 1;

	if (!next && data->host_cookie)
		return data->host_cookie;

	/*
	 * We don't do DMA on "complex" transfers, i.e. with
	 * non-word-aligned buffers or lengths. Also, we don't bother
	 * with all the DMA setup overhead for short transfers.
	 */
	if (data->blocks * data->blksz < DW_MCI_DMA_THRESHOLD)
		return -EINVAL;

	if (data->blksz & align_mask)
		return -EINVAL;

	for_each_sg(data->sg, sg, data->sg_len, i) {
		if (sg->offset & align_mask || sg->length & align_mask)
			return -EINVAL;
	}

	if (card && mmc_card_sdio(card)) {
		unsigned int rxwmark_val = 0, txwmark_val = 0, msize_val = 0;

		if (data->blksz >= (4 * (1 << host->data_shift))) {
			msize_val = 1;
			rxwmark_val = 3;
			txwmark_val = 4;
		} else {
			msize_val = 0;
			rxwmark_val = 1;
			txwmark_val = host->fifo_depth / 2;
		}

		host->fifoth_val = ((msize_val << 28) | (rxwmark_val << 16) |
				(txwmark_val << 0));
		dev_dbg(host->dev,
				"data->blksz: %d data->blocks %d Transfer Size %d  "
				"msize_val : %d, rxwmark_val : %d host->fifoth_val: 0x%08x\n",
				data->blksz, data->blocks, (data->blksz * data->blocks),
				msize_val, rxwmark_val, host->fifoth_val);

		mci_writel(host, FIFOTH, host->fifoth_val);

		if (mmc_card_uhs(card)
				&& card->host->caps & MMC_CAP_UHS_SDR104
				&& data->flags & MMC_DATA_READ)
			mci_writel(host, CDTHRCTL, data->blksz << 16 | 1);
	}

	sg_len = dma_map_sg(host->dev,
			    data->sg,
			    data->sg_len,
			    dw_mci_get_dma_dir(data));
	if (sg_len == 0)
		return -EINVAL;

	if (next)
		data->host_cookie = sg_len;

	return sg_len;
}

static void dw_mci_pre_req(struct mmc_host *mmc,
			   struct mmc_request *mrq,
			   bool is_first_req)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!slot->host->use_dma || !data)
		return;

	if (data->host_cookie) {
		data->host_cookie = 0;
		return;
	}

	if (dw_mci_pre_dma_transfer(slot->host, mrq->data, 1) < 0)
		data->host_cookie = 0;
}

static void dw_mci_post_req(struct mmc_host *mmc,
			    struct mmc_request *mrq,
			    int err)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!slot->host->use_dma || !data)
		return;

	if (data->host_cookie)
		dma_unmap_sg(slot->host->dev,
			     data->sg,
			     data->sg_len,
			     dw_mci_get_dma_dir(data));
	data->host_cookie = 0;
}

static void dw_mci_ctrl_rd_thld(struct dw_mci *host, struct mmc_data *data)
{
	unsigned int blksz = data->blksz;
	u32 blksz_depth, fifo_depth;
	u16 thld_size;

	WARN_ON(!(data->flags & MMC_DATA_READ));

	/*
	 * CDTHRCTL doesn't exist prior to 240A (in fact that register offset is
	 * in the FIFO region, so we really shouldn't access it).
	 */
	if (host->verid < DW_MMC_240A)
		return;

	if (host->timing != MMC_TIMING_MMC_HS200 &&
	    host->timing != MMC_TIMING_UHS_SDR104)
		goto disable;

	blksz_depth = blksz / (1 << host->data_shift);
	fifo_depth = host->fifo_depth;

	if (blksz_depth > fifo_depth)
		goto disable;

	/*
	 * If (blksz_depth) >= (fifo_depth >> 1), should be 'thld_size <= blksz'
	 * If (blksz_depth) <  (fifo_depth >> 1), should be thld_size = blksz
	 * Currently just choose blksz.
	 */
	thld_size = blksz;
	mci_writel(host, CDTHRCTL, SDMMC_SET_RD_THLD(thld_size, 1));
	return;

disable:
	mci_writel(host, CDTHRCTL, SDMMC_SET_RD_THLD(0, 0));
}

inline u32 dw_mci_calc_hto_timeout(struct dw_mci *host)
{
	struct dw_mci_slot *slot = host->cur_slot;
	u32 target_timeout, count;
	u32 max_time, max_ext_time;
	u32 host_clock = host->cclk_in;
	u32 tmout_value;
	int ext_cnt = 0;

	if (!host->pdata->hto_timeout)
		return 0xFFFFFFFF; /* timeout maximum */

	target_timeout = host->pdata->data_timeout;

	if (host->timing == MMC_TIMING_MMC_HS400 ||
				host->timing == MMC_TIMING_MMC_HS400_ES) {
		if (host->pdata->quirks & DW_MCI_QUIRK_ENABLE_ULP)
			host_clock *= 2;
	}

	max_time = SDMMC_DATA_TMOUT_MAX_CNT * SDMMC_DATA_TMOUT_CRT / (host_clock / 1000);

	if (target_timeout < max_time) {
		tmout_value = mci_readl(host, TMOUT);
		goto pass;
	} else {
		max_ext_time = SDMMC_DATA_TMOUT_MAX_EXT_CNT / (host_clock / 1000);
		ext_cnt = target_timeout / max_ext_time;
	}

	target_timeout = host->pdata->hto_timeout;

	/* use clkout for sysnopsys divider */
	if (host->timing == MMC_TIMING_MMC_HS400 ||
			host->timing == MMC_TIMING_MMC_HS400_ES ||
			(host->timing == MMC_TIMING_MMC_DDR52 &&
			 slot->ctype == SDMMC_CTYPE_8BIT))
		host_clock /= 2;

	/* Calculating Timeout value */
	count = target_timeout * (host_clock / 1000);

	if (count > 0xFFFFFF)
		count = 0xFFFFFF;

	tmout_value = (count << SDMMC_HTO_TMOUT_SHIFT) | SDMMC_RESP_TMOUT;
	tmout_value &= ~(0x7 << SDMMC_DATA_TMOUT_EXT_SHIFT);
	tmout_value |= ((ext_cnt + 1) << SDMMC_DATA_TMOUT_EXT_SHIFT);
pass:
	/* Set return value */
	return tmout_value;
}

static int dw_mci_submit_data_dma(struct dw_mci *host, struct mmc_data *data)
{
	int sg_len;
	u32 temp;

	host->using_dma = 0;

	/* If we don't have a channel, we can't do DMA */
	if (!host->use_dma)
		return -ENODEV;

	if (host->use_dma && host->dma_ops->init && host->dma_ops->reset) {
		host->dma_ops->init(host);
		host->dma_ops->reset(host);
	}

	sg_len = dw_mci_pre_dma_transfer(host, data, 0);
	if (sg_len < 0) {
		host->dma_ops->stop(host);
		dw_mci_set_timeout(host, dw_mci_calc_hto_timeout(host));
		return sg_len;
	}

	host->using_dma = 1;

	dev_vdbg(host->dev,
		 "sd sg_cpu: %#lx sg_dma: %#lx sg_len: %d\n",
		 (unsigned long)host->sg_cpu, (unsigned long)host->sg_dma,
		 sg_len);

	/* Enable the DMA interface */
	temp = mci_readl(host, CTRL);
	temp |= SDMMC_CTRL_DMA_ENABLE;
	mci_writel(host, CTRL, temp);

	/* Disable RX/TX IRQs, let DMA handle it */
	mci_writel(host, RINTSTS, SDMMC_INT_TXDR | SDMMC_INT_RXDR);
	temp = mci_readl(host, INTMASK);
	temp  &= ~(SDMMC_INT_RXDR | SDMMC_INT_TXDR);
	mci_writel(host, INTMASK, temp);

	host->dma_ops->start(host, sg_len);

	return 0;
}

static void dw_mci_submit_data(struct dw_mci *host, struct mmc_data *data)
{
	u32 temp;

	data->error = -EINPROGRESS;

	WARN_ON(host->data);
	host->sg = NULL;
	host->data = data;

	if (data->flags & MMC_DATA_READ) {
		host->dir_status = DW_MCI_RECV_STATUS;
		dw_mci_ctrl_rd_thld(host, data);
	} else {
		host->dir_status = DW_MCI_SEND_STATUS;
	}

	if (dw_mci_submit_data_dma(host, data)) {
		int flags = SG_MITER_ATOMIC;

		if (SDMMC_GET_FCNT(mci_readl(host, STATUS)))
			dw_mci_ctrl_reset(host, SDMMC_CTRL_FIFO_RESET);

		if (host->data->flags & MMC_DATA_READ)
			flags |= SG_MITER_TO_SG;
		else
			flags |= SG_MITER_FROM_SG;

		sg_miter_start(&host->sg_miter, data->sg, data->sg_len, flags);
		host->sg = data->sg;
		host->part_buf_start = 0;
		host->part_buf_count = 0;

		mci_writel(host, RINTSTS, SDMMC_INT_TXDR | SDMMC_INT_RXDR);
		temp = mci_readl(host, INTMASK);
		temp |= SDMMC_INT_TXDR | SDMMC_INT_RXDR;
		mci_writel(host, INTMASK, temp);

		temp = mci_readl(host, CTRL);
		temp &= ~SDMMC_CTRL_DMA_ENABLE;
		mci_writel(host, CTRL, temp);

	}
}

static void mci_send_cmd(struct dw_mci_slot *slot, u32 cmd, u32 arg)
{
	struct dw_mci *host = slot->host;
	unsigned long timeout = jiffies + msecs_to_jiffies(10);
	unsigned int cmd_status = 0;
	int try = 50;

	atomic_inc_return(&slot->host->ciu_en_win);
	dw_mci_ciu_clk_en(slot->host, false);
	atomic_dec_return(&slot->host->ciu_en_win);

	mci_writel(host, CMDARG, arg);
	wmb();
	mci_writel(host, CMD, SDMMC_CMD_START | cmd);

	do {
		while (time_before(jiffies, timeout)) {
			cmd_status = mci_readl(host, CMD);
			if (!(cmd_status & SDMMC_CMD_START))
				return;
		}

		dw_mci_ctrl_reset(host, SDMMC_CTRL_RESET);
		mci_writel(host, CMD, SDMMC_CMD_START | cmd);
		timeout = jiffies + msecs_to_jiffies(10);
	} while (--try);

	dev_err(&slot->mmc->class_dev,
		"Timeout sending command (cmd %#x arg %#x status %#x)\n",
		cmd, arg, cmd_status);

	/* Debuggin for interrupt storming */
	dw_mci_debug_flag = 1;
	dw_mci_reg_dump(host);
}

static bool dw_mci_wait_data_busy(struct dw_mci *host, struct mmc_request *mrq)
{
	u32 status;
	unsigned long timeout = jiffies + msecs_to_jiffies(100);
	struct dw_mci_slot *slot = host->cur_slot;
	int try = 2;
	u32 clkena;
	bool ret = false;

	do {
		do {
			status = mci_readl(host, STATUS);
			if (!(status & SDMMC_STATUS_BUSY)) {
				ret = true;
				goto out;
			}

			usleep_range(10, 20);
		} while (time_before(jiffies, timeout));

		/* card is checked every 1s by CMD13 at least */
		if (mrq->cmd->opcode == MMC_SEND_STATUS)
			return true;

		dw_mci_ctrl_reset(host, SDMMC_CTRL_FIFO_RESET);
		dw_mci_ctrl_reset(host, SDMMC_CTRL_RESET);
		/* After CTRL Reset, Should be needed clk val to CIU */
		if (host->cur_slot) {
			/* Disable low power mode */
			clkena = mci_readl(host, CLKENA);
			clkena &= ~((SDMMC_CLKEN_LOW_PWR) << slot->id);
			mci_writel(host, CLKENA, clkena);

			mci_send_cmd(host->cur_slot,
				SDMMC_CMD_UPD_CLK | SDMMC_CMD_PRV_DAT_WAIT, 0);
		}
		timeout = jiffies + msecs_to_jiffies(100);
	} while (--try);
out:
	if (host->cur_slot) {
		if (ret == false)
			dev_err(host->dev, "Data[0]: data is busy\n");

		/* enable clock */
		mci_writel(host, CLKENA, ((SDMMC_CLKEN_ENABLE |
					SDMMC_CLKEN_LOW_PWR) << slot->id));

		/* inform CIU */
		mci_send_cmd(slot,
			     SDMMC_CMD_UPD_CLK | SDMMC_CMD_PRV_DAT_WAIT, 0);
	}

	return ret;
}

static void dw_mci_setup_bus(struct dw_mci_slot *slot, bool force_clkinit)
{
	struct dw_mci *host = slot->host;
	unsigned int clock = slot->clock;
	u32 div;
	u32 clk_en_a;
	u32 sdmmc_cmd_bits = SDMMC_CMD_UPD_CLK | SDMMC_CMD_PRV_DAT_WAIT;

	if (!clock) {
		mci_writel(host, CLKENA, 0);
		mci_send_cmd(slot, sdmmc_cmd_bits, 0);
	} else if (clock != host->current_speed || force_clkinit) {
		div = host->bus_hz / clock;
		if (host->bus_hz % clock && host->bus_hz > clock)
			/*
			 * move the + 1 after the divide to prevent
			 * over-clocking the card.
			 */
			div += 1;

		div = (host->bus_hz != clock) ? DIV_ROUND_UP(div, 2) : 0;

		if ((clock << div) != slot->__clk_old || force_clkinit)
			dev_info(&slot->mmc->class_dev,
				 "Bus speed (slot %d) = %dHz (slot req %dHz, actual %dHZ div = %d)\n",
				 slot->id, host->bus_hz, clock,
				 div ? ((host->bus_hz / div) >> 1) :
				 host->bus_hz, div);

		/* disable clock */
		mci_writel(host, CLKENA, 0);
		mci_writel(host, CLKSRC, 0);

		/* inform CIU */
		dw_mci_update_clock(slot);

		/* set clock to desired speed */
		mci_writel(host, CLKDIV, div);

		/* inform CIU */
		dw_mci_update_clock(slot);

		/* enable clock; only low power if no SDIO */
		clk_en_a = SDMMC_CLKEN_ENABLE << slot->id;
		if (!(mci_readl(host, INTMASK) & SDMMC_INT_SDIO(slot->id)))
			clk_en_a |= SDMMC_CLKEN_LOW_PWR << slot->id;

		if (host->current_speed <= 400 * 1000)
			clk_en_a &= ~(SDMMC_CLKEN_LOW_PWR << slot->id);

		mci_writel(host, CLKENA, clk_en_a);

		/* inform CIU */
		dw_mci_update_clock(slot);

		/* keep the clock with reflecting clock dividor */
		slot->__clk_old = clock << div;
	}

	host->current_speed = clock;

	/* Set the current slot bus width */
	mci_writel(host, CTYPE, (slot->ctype << slot->id));
}

inline u32 dw_mci_calc_timeout(struct dw_mci *host)
{
	u32 target_timeout;
	u32 count;
	u32 max_time;
	u32 max_ext_time;
	int ext_cnt = 0;
	u32 host_clock = host->cclk_in;

	if (!host->pdata->data_timeout)
		return 0xFFFFFFFF; /* timeout maximum */

	target_timeout = host->pdata->data_timeout;

	if (host->timing == MMC_TIMING_MMC_HS400 ||
				host->timing == MMC_TIMING_MMC_HS400_ES) {
		if (host->pdata->quirks & DW_MCI_QUIRK_ENABLE_ULP)
			host_clock *= 2;
	}

	max_time = SDMMC_DATA_TMOUT_MAX_CNT * SDMMC_DATA_TMOUT_CRT / (host_clock / 1000);

	if (target_timeout > max_time) {
		max_ext_time = SDMMC_DATA_TMOUT_MAX_EXT_CNT / (host_clock / 1000);
		ext_cnt = target_timeout / max_ext_time;
		target_timeout -= (max_ext_time * ext_cnt);
	}
	count = (target_timeout * (host_clock / 1000)) / SDMMC_DATA_TMOUT_CRT;

	/* Set return value */
	return ((count << SDMMC_DATA_TMOUT_SHIFT)
		| ((ext_cnt + SDMMC_DATA_TMOUT_EXT) << SDMMC_DATA_TMOUT_EXT_SHIFT)
		| SDMMC_RESP_TMOUT);
}

static void __dw_mci_start_request(struct dw_mci *host,
				   struct dw_mci_slot *slot,
				   struct mmc_command *cmd)
{
	struct mmc_request *mrq;
	struct mmc_data	*data;
	u32 cmdflags;

	mrq = slot->mrq;

	if (mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK ||
		mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200)
			mod_timer(&host->timer, jiffies + msecs_to_jiffies(500));
	else if (host->pdata->sw_timeout)
		mod_timer(&host->timer, jiffies + msecs_to_jiffies(host->pdata->sw_timeout));
	else
		mod_timer(&host->timer, jiffies + msecs_to_jiffies(10000));

	host->cur_slot = slot;
	host->mrq = mrq;

	host->pending_events = 0;
	host->completed_events = 0;
	host->cmd_status = 0;
	host->data_status = 0;
	host->dir_status = 0;

	data = cmd->data;
	if (data) {
		if (host->quirks & DW_MCI_SW_TRANS) {
			if (mci_readl(host, MPSTAT) & 0x1)
				dw_mci_force_reset(host);
		}

		dw_mci_set_timeout(host, dw_mci_calc_timeout(host));
		mci_writel(host, BYTCNT, data->blksz*data->blocks);
		mci_writel(host, BLKSIZ, data->blksz);
		host->transferred_cnt += data->blksz * data->blocks;
	}

	cmdflags = dw_mci_prepare_command(slot->mmc, cmd);

	/* this is the first command, send the initialization clock */
	if (test_and_clear_bit(DW_MMC_CARD_NEED_INIT, &slot->flags))
		cmdflags |= SDMMC_CMD_INIT;

	if (data) {
		dw_mci_submit_data(host, data);
		wmb();
	}

	dw_mci_debug_req_log(host, mrq, STATE_REQ_START, 0);

	dw_mci_start_command(host, cmd, cmdflags);

	if (mrq->stop)
		host->stop_cmdr = dw_mci_prepare_command(slot->mmc, mrq->stop);
	else
		host->stop_cmdr = dw_mci_prep_stop_abort(host, cmd);
}

static void dw_mci_start_request(struct dw_mci *host,
				 struct dw_mci_slot *slot)
{
	struct mmc_request *mrq = slot->mrq;
	struct mmc_command *cmd;

	host->req_state = DW_MMC_REQ_BUSY;

	cmd = mrq->sbc ? mrq->sbc : mrq->cmd;
	__dw_mci_start_request(host, slot, cmd);
}

/* must be called with host->lock held */
static void dw_mci_queue_request(struct dw_mci *host, struct dw_mci_slot *slot,
				 struct mmc_request *mrq)
{
	dev_vdbg(&slot->mmc->class_dev, "queue request: state=%d\n",
		 host->state);

	slot->mrq = mrq;

	if (host->state == STATE_WAITING_CMD11_DONE) {
		dev_warn(&slot->mmc->class_dev,
			 "Voltage change didn't complete\n");
		/*
		 * this case isn't expected to happen, so we can
		 * either crash here or just try to continue on
		 * in the closest possible state
		 */
		host->state = STATE_IDLE;
	}

	if (host->state == STATE_IDLE) {
		host->state = STATE_SENDING_CMD;
		dw_mci_start_request(host, slot);
	} else {
		list_add_tail(&slot->queue_node, &host->queue);
	}
}

static void dw_mci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;

	WARN_ON(slot->mrq);

	/*
	 * The check for card presence and queueing of the request must be
	 * atomic, otherwise the card could be removed in between and the
	 * request wouldn't fail until another card was inserted.
	 */

	if (!dw_mci_stop_abort_cmd(mrq->cmd)) {
		if (!dw_mci_wait_data_busy(host, mrq)) {
			mrq->cmd->error = -ENOTRECOVERABLE;
			mmc_request_done(mmc, mrq);
			return;
		}
	}

	spin_lock_bh(&host->lock);

	if (!test_bit(DW_MMC_CARD_PRESENT, &slot->flags)) {
		spin_unlock_bh(&host->lock);
		mrq->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, mrq);
		return;
	}

	dw_mci_queue_request(host, slot, mrq);

	spin_unlock_bh(&host->lock);
}

static void dw_mci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	const struct dw_mci_drv_data *drv_data = slot->host->drv_data;
	u32 regs;
	int ret;
	bool cclk_request_turn_off = 0;

	switch (ios->bus_width) {
	case MMC_BUS_WIDTH_4:
		slot->ctype = SDMMC_CTYPE_4BIT;
		break;
	case MMC_BUS_WIDTH_8:
		slot->ctype = SDMMC_CTYPE_8BIT;
		break;
	default:
		/* set default 1 bit mode */
		slot->ctype = SDMMC_CTYPE_1BIT;
	}

	regs = mci_readl(slot->host, UHS_REG);

	/* DDR mode set */
	if (ios->timing == MMC_TIMING_MMC_DDR52 ||
			ios->timing == MMC_TIMING_MMC_HS400 ||
			ios->timing == MMC_TIMING_MMC_HS400_ES)
		regs |= ((0x1 << slot->id) << 16);
	else
		regs &= ~((0x1 << slot->id) << 16);

	if (slot->host->pdata->caps &
			(MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
			 MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104 |
			 MMC_CAP_UHS_DDR50))
		regs |= (0x1 << slot->id);

	mci_writel(slot->host, UHS_REG, regs);
	slot->host->timing = ios->timing;

	/*
	 * Use mirror of ios->clock to prevent race with mmc
	 * core ios update when finding the minimum.
	 */
	slot->clock = ios->clock;

	if (ios->clock) {
		/*
		 * Use mirror of ios->clock to prevent race with mmc
		 * core ios update when finding the minimum.
		 */
#ifdef CONFIG_CPU_IDLE
		exynos_update_ip_idle_status(slot->host->idle_ip_index, 0);
#endif
		pm_qos_update_request(&slot->host->pm_qos_int,
				slot->host->pdata->qos_int_level);
	} else {
		pm_qos_update_request(&slot->host->pm_qos_int, 0);
#ifdef CONFIG_CPU_IDLE
		exynos_update_ip_idle_status(slot->host->idle_ip_index, 1);
#endif
		cclk_request_turn_off = 1;
	}

	if (drv_data && drv_data->set_ios)
		drv_data->set_ios(slot->host, ios);

	/* Slot specific timing and width adjustment */
	dw_mci_setup_bus(slot, false);

	if (slot->host->state == STATE_WAITING_CMD11_DONE && ios->clock != 0)
		slot->host->state = STATE_IDLE;

	if (!(slot->host->quirks & DW_MMC_QUIRK_FIXED_VOLTAGE)) {
		switch (ios->power_mode) {
		case MMC_POWER_UP:
			if (!IS_ERR(mmc->supply.vmmc)) {
				ret = mmc_regulator_set_ocr(mmc, mmc->supply.vmmc,
							ios->vdd);
			if (ret) {
					dev_err(slot->host->dev,
							"failed to enable vmmc regulator\n");
					/*return, if failed turn on vmmc*/
					return;
				}
			}
			if (!IS_ERR(mmc->supply.vqmmc) && !slot->host->vqmmc_enabled) {
				ret = regulator_enable(mmc->supply.vqmmc);

				if (ret < 0)
					dev_err(slot->host->dev,
							"failed to enable vqmmc regulator\n");
					else
						slot->host->vqmmc_enabled = true;
			}
			set_bit(DW_MMC_CARD_NEED_INIT, &slot->flags);
			regs = mci_readl(slot->host, PWREN);
			regs |= (1 << slot->id);
			mci_writel(slot->host, PWREN, regs);
			break;
		case MMC_POWER_OFF:
			if (!IS_ERR(mmc->supply.vmmc))
				mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

 			if (!IS_ERR(mmc->supply.vqmmc) && slot->host->vqmmc_enabled) {
				regulator_disable(mmc->supply.vqmmc);
				slot->host->vqmmc_enabled = false;
			}

			cclk_request_turn_off = 0;
			regs = mci_readl(slot->host, PWREN);
			regs &= ~(1 << slot->id);
			mci_writel(slot->host, PWREN, regs);

			if (mmc->card)
				mdelay(40);
			break;
		default:
			break;
		}
	}

	if (cclk_request_turn_off) {
		dw_mci_ciu_clk_dis(slot->host);
		if (!IS_ERR(slot->host->biu_clk))
			dw_mci_biu_clk_dis(slot->host);
	}
}

static int dw_mci_card_busy(struct mmc_host *mmc)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	u32 status;

	/*
	 * Check the busy bit which is low when DAT[3:0]
	 * (the data lines) are 0000
	 */
	status = mci_readl(slot->host, STATUS);

	return !!(status & SDMMC_STATUS_BUSY);
}

static int dw_mci_switch_voltage(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	unsigned long timeout = jiffies + msecs_to_jiffies(10);
	u32 uhs;
	u32 v18 = SDMMC_UHS_18V << slot->id;
	int min_uv, max_uv;
	int ret = 0, retry = 10;
	u32 status;

	/*
	 * Program the voltage.  Note that some instances of dw_mmc may use
	 * the UHS_REG for this.  For other instances (like exynos) the UHS_REG
	 * does no harm but you need to set the regulator directly.  Try both.
	 */
	uhs = mci_readl(host, UHS_REG);
	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		min_uv = 2800000;
		max_uv = 2800000;
		uhs &= ~v18;
	} else {
		if (host->pdata->use_vqmmc19 ) {
			min_uv = 1900000;
			max_uv = 1900000;
		} else {	
			min_uv = 1800000;
			max_uv = 1800000;
		}
		uhs |= v18;
	}

	if (!(host->quirks & DW_MMC_QUIRK_FIXED_VOLTAGE)) {
		if (!IS_ERR(mmc->supply.vqmmc)) {
			if (ios->signal_voltage != MMC_SIGNAL_VOLTAGE_330) {
				dw_mci_ctrl_reset(host, SDMMC_CTRL_RESET);
				/* Check For DATA busy */
				do {

					while (time_before(jiffies, timeout)) {
						status = mci_readl(host, STATUS);
						if (!(status & SDMMC_STATUS_BUSY))
							goto out;
					}

					dw_mci_ctrl_reset(host, SDMMC_CTRL_RESET);
					timeout = jiffies + msecs_to_jiffies(10);
				} while (--retry);
			}
out:
			/* waiting for stable */
			mdelay(10);

			ret = regulator_set_voltage(mmc->supply.vqmmc, min_uv, max_uv);

			if (ret) {
				dev_err(&mmc->class_dev,
						"Regulator set error %d: %d - %d\n",
						ret, min_uv, max_uv);
				return ret;
			}
		}
	}
	mci_writel(host, UHS_REG, uhs);

	return 0;
}

static int dw_mci_get_ro(struct mmc_host *mmc)
{
	int read_only;
	struct dw_mci_slot *slot = mmc_priv(mmc);
	int gpio_ro = mmc_gpio_get_ro(mmc);

	/* Use platform get_ro function, else try on board write protect */
	if ((slot->quirks & DW_MCI_SLOT_QUIRK_NO_WRITE_PROTECT) ||
			(slot->host->quirks & DW_MCI_QUIRK_NO_WRITE_PROTECT))
		read_only = 0;
	else if (!IS_ERR_VALUE(gpio_ro))
		read_only = gpio_ro;
	else
		read_only =
			mci_readl(slot->host, WRTPRT) & (1 << slot->id) ? 1 : 0;

	dev_dbg(&mmc->class_dev, "card is %s\n",
		read_only ? "read-only" : "read-write");

	return read_only;
}

static int dw_mci_get_cd(struct mmc_host *mmc)
{
	int present;
	int temp;
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci_board *brd = slot->host->pdata;
	struct dw_mci *host = slot->host;
	int gpio_cd = mmc_gpio_get_cd(mmc);
	const struct dw_mci_drv_data *drv_data = host->drv_data;

	/* Use platform get_cd function, else try onboard card detect */
	if (brd->quirks & DW_MCI_QUIRK_BROKEN_CARD_DETECTION)
		present = 1;
	else if (!IS_ERR_VALUE(gpio_cd))
		present = gpio_cd;
	else
		present = (mci_readl(slot->host, CDETECT) & (1 << slot->id))
			== 0 ? 1 : 0;
	if (drv_data && drv_data->misc_control) {
		temp = drv_data->misc_control(host,
				 CTRL_CHECK_CD, NULL);
		if (temp != -1)
			present = temp;
	}

	spin_lock_bh(&host->lock);
	if (present) {
		set_bit(DW_MMC_CARD_PRESENT, &slot->flags);
		dev_dbg(&mmc->class_dev, "card is present\n");
	} else {
		clear_bit(DW_MMC_CARD_PRESENT, &slot->flags);
		dev_dbg(&mmc->class_dev, "card is not present\n");
	}
	spin_unlock_bh(&host->lock);

	return present;
}

/*
 * Disable lower power mode.
 *
 * Low power mode will stop the card clock when idle.  According to the
 * description of the CLKENA register we should disable low power mode
 * for SDIO cards if we need SDIO interrupts to work.
 *
 * This function is fast if low power mode is already disabled.
 */
static void dw_mci_disable_low_power(struct dw_mci_slot *slot)
{
	struct dw_mci *host = slot->host;
	u32 clk_en_a;
	const u32 clken_low_pwr = SDMMC_CLKEN_LOW_PWR << slot->id;

	clk_en_a = mci_readl(host, CLKENA);

	if (clk_en_a & clken_low_pwr) {
		mci_writel(host, CLKENA, clk_en_a & ~clken_low_pwr);
		dw_mci_update_clock(slot);
	}
}

static void dw_mci_enable_sdio_irq(struct mmc_host *mmc, int enb)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	u32 int_mask;

	/* Enable/disable Slot Specific SDIO interrupt */
	int_mask = mci_readl(host, INTMASK);
	if (enb) {
		/*
		 * Turn off low power mode if it was enabled.  This is a bit of
		 * a heavy operation and we disable / enable IRQs a lot, so
		 * we'll leave low power mode disabled and it will get
		 * re-enabled again in dw_mci_setup_bus().
		 */
		dw_mci_disable_low_power(slot);

		mci_writel(host, INTMASK,
			   (int_mask | SDMMC_INT_SDIO(slot->id)));
	} else {
		mci_writel(host, INTMASK,
			   (int_mask & ~SDMMC_INT_SDIO(slot->id)));
	}
}

static int dw_mci_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	struct dw_mci_tuning_data tuning_data;
	int err = -ENOSYS;

	if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
		if (mmc->ios.bus_width == MMC_BUS_WIDTH_8) {
			tuning_data.blk_pattern = tuning_blk_pattern_8bit;
			tuning_data.blksz = sizeof(tuning_blk_pattern_8bit);
		} else if (mmc->ios.bus_width == MMC_BUS_WIDTH_4) {
			tuning_data.blk_pattern = tuning_blk_pattern_4bit;
			tuning_data.blksz = sizeof(tuning_blk_pattern_4bit);
		} else {
			return -EINVAL;
		}
	} else if (opcode == MMC_SEND_TUNING_BLOCK) {
		tuning_data.blk_pattern = tuning_blk_pattern_4bit;
		tuning_data.blksz = sizeof(tuning_blk_pattern_4bit);
	} else {
		dev_err(host->dev,
			"Undefined command(%d) for tuning\n", opcode);
		return -EINVAL;
	}

	if (drv_data && drv_data->execute_tuning)
		err = drv_data->execute_tuning(slot, opcode, &tuning_data);
	return err;
}

static const struct mmc_host_ops dw_mci_ops = {
	.request		= dw_mci_request,
	.pre_req		= dw_mci_pre_req,
	.post_req		= dw_mci_post_req,
	.set_ios		= dw_mci_set_ios,
	.get_ro			= dw_mci_get_ro,
	.get_cd			= dw_mci_get_cd,
	.enable_sdio_irq	= dw_mci_enable_sdio_irq,
	.execute_tuning		= dw_mci_execute_tuning,
	.card_busy		= dw_mci_card_busy,
	.start_signal_voltage_switch = dw_mci_switch_voltage,

};

static void dw_mci_request_end(struct dw_mci *host, struct mmc_request *mrq)
	__releases(&host->lock)
	__acquires(&host->lock)
{
	struct dw_mci_slot *slot;
	struct mmc_host	*prev_mmc = host->cur_slot->mmc;

	WARN_ON(host->cmd || host->data);

	del_timer(&host->timer);

	host->req_state = DW_MMC_REQ_IDLE;

	dw_mci_debug_req_log(host, mrq, STATE_REQ_END, 0);

	host->cur_slot->mrq = NULL;
	host->mrq = NULL;
	if (!list_empty(&host->queue)) {
		slot = list_entry(host->queue.next,
				  struct dw_mci_slot, queue_node);
		list_del(&slot->queue_node);
		dev_vdbg(host->dev, "list not empty: %s is next\n",
			 mmc_hostname(slot->mmc));
		host->state = STATE_SENDING_CMD;
		dw_mci_start_request(host, slot);
	} else {
		dev_vdbg(host->dev, "list empty\n");

		if (host->state == STATE_SENDING_CMD11)
			host->state = STATE_WAITING_CMD11_DONE;
		else
			host->state = STATE_IDLE;
	}

	spin_unlock(&host->lock);
	mmc_request_done(prev_mmc, mrq);
	spin_lock(&host->lock);
}

static int dw_mci_command_complete(struct dw_mci *host, struct mmc_command *cmd)
{
	u32 status = host->cmd_status;

	host->cmd_status = 0;

	/* Read the response from the card (up to 16 bytes) */
	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[3] = mci_readl(host, RESP0);
			cmd->resp[2] = mci_readl(host, RESP1);
			cmd->resp[1] = mci_readl(host, RESP2);
			cmd->resp[0] = mci_readl(host, RESP3);
		} else {
			cmd->resp[0] = mci_readl(host, RESP0);
			cmd->resp[1] = 0;
			cmd->resp[2] = 0;
			cmd->resp[3] = 0;
		}
	}

	if (status & SDMMC_INT_RTO)
		cmd->error = -ETIMEDOUT;
	else if ((cmd->flags & MMC_RSP_CRC) && (status & SDMMC_INT_RCRC))
		cmd->error = -EILSEQ;
	else if (status & SDMMC_INT_RESP_ERR)
		cmd->error = -EIO;
	else
		cmd->error = 0;

	if (cmd->error) {
		/* newer ip versions need a delay between retries */
		if (host->quirks & DW_MCI_QUIRK_RETRY_DELAY)
			mdelay(20);
	}

	return cmd->error;
}

static int dw_mci_data_complete(struct dw_mci *host, struct mmc_data *data)
{
	u32 status = host->data_status;

	if (status & DW_MCI_DATA_ERROR_FLAGS) {
		if (status & SDMMC_INT_DRTO) {
			data->error = -ETIMEDOUT;
			dev_info(host->dev, "DRTO check TMOUT count 0x%08x\n", mci_readl(host, TMOUT));
		} else if (status & SDMMC_INT_DCRC) {
			data->error = -EILSEQ;
		} else if (status & SDMMC_INT_EBE) {
			if (host->dir_status ==
				DW_MCI_SEND_STATUS) {
				/*
				 * No data CRC status was returned.
				 * The number of bytes transferred
				 * will be exaggerated in PIO mode.
				 */
				data->bytes_xfered = 0;
				data->error = -ETIMEDOUT;
			} else if (host->dir_status ==
					DW_MCI_RECV_STATUS) {
				data->error = -EIO;
			}
		} else {
			/* SDMMC_INT_SBE is included */
			data->error = -EIO;
		}

		printk("%s: DATA command %d : %d, status = %#x, \n",
				mmc_hostname(host->cur_slot->mmc),
				host->cur_slot->mrq->cmd->opcode, data->error, status);

		/*
		 * After an error, there may be data lingering
		 * in the FIFO
		 */
		sg_miter_stop(&host->sg_miter);
		host->sg = NULL;
		dw_mci_fifo_reset(host->dev, host);
		dw_mci_ciu_reset(host->dev, host);
	} else {
		data->bytes_xfered = data->blocks * data->blksz;
		data->error = 0;
	}

	return data->error;
}

static void dw_mci_tasklet_func(unsigned long priv)
{
	struct dw_mci *host = (struct dw_mci *)priv;
	struct mmc_data	*data;
	struct mmc_command *cmd;
	struct mmc_request *mrq;
	enum dw_mci_state state;
	enum dw_mci_state prev_state;
	unsigned int err;

	spin_lock(&host->lock);

	if(host->sw_timeout_chk == true)
		goto unlock;

	state = host->state;
	data = host->data;
	mrq = host->mrq;

	do {
		prev_state = state;

		switch (state) {
		case STATE_IDLE:
		case STATE_WAITING_CMD11_DONE:
			break;

		case STATE_SENDING_CMD11:
		case STATE_SENDING_CMD:
			if (!test_and_clear_bit(EVENT_CMD_COMPLETE,
						&host->pending_events))
				break;

			cmd = host->cmd;
			host->cmd = NULL;
			set_bit(EVENT_CMD_COMPLETE, &host->completed_events);
			err = dw_mci_command_complete(host, cmd);
			if (cmd == mrq->sbc && !err) {
				prev_state = state = STATE_SENDING_CMD;
				__dw_mci_start_request(host, host->cur_slot,
						       mrq->cmd);
				goto unlock;
			}

			if (cmd->data && err) {
				dw_mci_fifo_reset(host->dev, host);
				dw_mci_stop_dma(host);
				send_stop_abort(host, data);
				state = STATE_SENDING_STOP;
				dw_mci_debug_req_log(host,
						host->mrq,
						STATE_REQ_CMD_PROCESS, state);
				break;
			}

			if (!cmd->data || err) {
				if(host->sw_timeout_chk != true)
					dw_mci_request_end(host, mrq);
				goto unlock;
			}

			prev_state = state = STATE_SENDING_DATA;

			dw_mci_debug_req_log(host, host->mrq,
					STATE_REQ_CMD_PROCESS, state);

			/* fall through */

		case STATE_SENDING_DATA:
			/*
			 * We could get a data error and never a transfer
			 * complete so we'd better check for it here.
			 *
			 * Note that we don't really care if we also got a
			 * transfer complete; stopping the DMA and sending an
			 * abort won't hurt.
			 */
			if (test_and_clear_bit(EVENT_DATA_ERROR,
					       &host->pending_events)) {
				dw_mci_fifo_reset(host->dev, host);
				dw_mci_stop_dma(host);
				send_stop_abort(host, data);
				state = STATE_DATA_ERROR;
				dw_mci_debug_req_log(host,
						host->mrq,
						STATE_REQ_DATA_PROCESS, state);
				break;
			}

			if (!test_and_clear_bit(EVENT_XFER_COMPLETE,
						&host->pending_events))
				break;

			set_bit(EVENT_XFER_COMPLETE, &host->completed_events);

			/*
			 * Handle an EVENT_DATA_ERROR that might have shown up
			 * before the transfer completed.  This might not have
			 * been caught by the check above because the interrupt
			 * could have gone off between the previous check and
			 * the check for transfer complete.
			 *
			 * Technically this ought not be needed assuming we
			 * get a DATA_COMPLETE eventually (we'll notice the
			 * error and end the request), but it shouldn't hurt.
			 *
			 * This has the advantage of sending the stop command.
			 */
			if (test_and_clear_bit(EVENT_DATA_ERROR,
					       &host->pending_events)) {
				dw_mci_fifo_reset(host->dev, host);
				dw_mci_stop_dma(host);
				send_stop_abort(host, data);
				state = STATE_DATA_ERROR;
				dw_mci_debug_req_log(host, host->mrq,
						STATE_REQ_DATA_PROCESS, state);
				break;
			}
			prev_state = state = STATE_DATA_BUSY;

			dw_mci_debug_req_log(host, host->mrq,
					STATE_REQ_DATA_PROCESS, state);

			/* fall through */

		case STATE_DATA_BUSY:
			if (!test_and_clear_bit(EVENT_DATA_COMPLETE,
						&host->pending_events))
				break;

			set_bit(EVENT_DATA_COMPLETE, &host->completed_events);
			err = dw_mci_data_complete(host, data);

			host->data = NULL;
			if (!err) {
				if (!data->stop || mrq->sbc) {
					if (mrq->sbc && data->stop)
						data->stop->error = 0;

					if(host->sw_timeout_chk != true)
						dw_mci_request_end(host, mrq);
					goto unlock;
				}

				/* stop command for open-ended transfer*/
				if (data->stop)
					send_stop_abort(host, data);
			} else {
				/*
				 * If we don't have a command complete now we'll
				 * never get one since we just reset everything;
				 * better end the request.
				 *
				 * If we do have a command complete we'll fall
				 * through to the SENDING_STOP command and
				 * everything will be peachy keen.
				 */
				if (!test_bit(EVENT_CMD_COMPLETE,
					      &host->pending_events)) {
					host->cmd = NULL;

					if(host->sw_timeout_chk != true)
						dw_mci_request_end(host, mrq);
					goto unlock;
				}
			}

			/*
			 * If err has non-zero,
			 * stop-abort command has been already issued.
			 */
			prev_state = state = STATE_SENDING_STOP;

			dw_mci_debug_req_log(host, host->mrq,
					STATE_REQ_DATA_PROCESS, state);
			/* fall through */

		case STATE_SENDING_STOP:
			if (!test_and_clear_bit(EVENT_CMD_COMPLETE,
						&host->pending_events))
				break;

			/* CMD error in data command */
			if (mrq->cmd->error && mrq->data) {
				dw_mci_stop_dma(host);
				sg_miter_stop(&host->sg_miter);
				host->sg = NULL;
				dw_mci_fifo_reset(host->dev, host);
			}

			host->cmd = NULL;
			host->data = NULL;

			if (mrq->stop)
				dw_mci_command_complete(host, mrq->stop);
			else
				host->cmd_status = 0;

			if(host->sw_timeout_chk != true)
				dw_mci_request_end(host, mrq);
			dw_mci_debug_req_log(host, host->mrq,
					STATE_REQ_DATA_PROCESS, state);
			goto unlock;

		case STATE_DATA_ERROR:
			if (!test_and_clear_bit(EVENT_XFER_COMPLETE,
						&host->pending_events))
				break;

			set_bit(EVENT_XFER_COMPLETE, &host->completed_events);
			set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
			set_bit(EVENT_DATA_COMPLETE, &host->pending_events);

			state = STATE_DATA_BUSY;
			dw_mci_debug_req_log(host, host->mrq,
					STATE_REQ_DATA_PROCESS, state);
			break;
		}
	} while (state != prev_state);

	host->state = state;
unlock:
	spin_unlock(&host->lock);

}

/* push final bytes to part_buf, only use during push */
static void dw_mci_set_part_bytes(struct dw_mci *host, void *buf, int cnt)
{
	memcpy((void *)&host->part_buf, buf, cnt);
	host->part_buf_count = cnt;
}

/* append bytes to part_buf, only use during push */
static int dw_mci_push_part_bytes(struct dw_mci *host, void *buf, int cnt)
{
	cnt = min(cnt, (1 << host->data_shift) - host->part_buf_count);
	memcpy((void *)&host->part_buf + host->part_buf_count, buf, cnt);
	host->part_buf_count += cnt;
	return cnt;
}

/* pull first bytes from part_buf, only use during pull */
static int dw_mci_pull_part_bytes(struct dw_mci *host, void *buf, int cnt)
{
	cnt = min(cnt, (int)host->part_buf_count);
	if (cnt) {
		memcpy(buf, (void *)&host->part_buf + host->part_buf_start,
		       cnt);
		host->part_buf_count -= cnt;
		host->part_buf_start += cnt;
	}
	return cnt;
}

/* pull final bytes from the part_buf, assuming it's just been filled */
static void dw_mci_pull_final_bytes(struct dw_mci *host, void *buf, int cnt)
{
	memcpy(buf, &host->part_buf, cnt);
	host->part_buf_start = cnt;
	host->part_buf_count = (1 << host->data_shift) - cnt;
}

static void dw_mci_push_data16(struct dw_mci *host, void *buf, int cnt)
{
	struct mmc_data *data = host->data;
	int init_cnt = cnt;

	/* try and push anything in the part_buf */
	if (unlikely(host->part_buf_count)) {
		int len = dw_mci_push_part_bytes(host, buf, cnt);
		buf += len;
		cnt -= len;
		if (host->part_buf_count == 2) {
			mci_writew(host, DATA(host->data_offset),
					host->part_buf16);
			host->part_buf_count = 0;
		}
	}
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x1)) {
		while (cnt >= 2) {
			u16 aligned_buf[64];
			int len = min(cnt & -2, (int)sizeof(aligned_buf));
			int items = len >> 1;
			int i;
			/* memcpy from input buffer into aligned buffer */
			memcpy(aligned_buf, buf, len);
			buf += len;
			cnt -= len;
			/* push data from aligned buffer into fifo */
			for (i = 0; i < items; ++i)
				mci_writew(host, DATA(host->data_offset),
						aligned_buf[i]);
		}
	} else
#endif
	{
		u16 *pdata = buf;
		for (; cnt >= 2; cnt -= 2)
			mci_writew(host, DATA(host->data_offset), *pdata++);
		buf = pdata;
	}
	/* put anything remaining in the part_buf */
	if (cnt) {
		dw_mci_set_part_bytes(host, buf, cnt);
		 /* Push data if we have reached the expected data length */
		if ((data->bytes_xfered + init_cnt) ==
		    (data->blksz * data->blocks))
			mci_writew(host, DATA(host->data_offset),
				   host->part_buf16);
	}
}

static void dw_mci_pull_data16(struct dw_mci *host, void *buf, int cnt)
{
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x1)) {
		while (cnt >= 2) {
			/* pull data from fifo into aligned buffer */
			u16 aligned_buf[64];
			int len = min(cnt & -2, (int)sizeof(aligned_buf));
			int items = len >> 1;
			int i;
			for (i = 0; i < items; ++i)
				aligned_buf[i] = mci_readw(host,
						DATA(host->data_offset));
			/* memcpy from aligned buffer into output buffer */
			memcpy(buf, aligned_buf, len);
			buf += len;
			cnt -= len;
		}
	} else
#endif
	{
		u16 *pdata = buf;
		for (; cnt >= 2; cnt -= 2)
			*pdata++ = mci_readw(host, DATA(host->data_offset));
		buf = pdata;
	}
	if (cnt) {
		host->part_buf16 = mci_readw(host, DATA(host->data_offset));
		dw_mci_pull_final_bytes(host, buf, cnt);
	}
}

static void dw_mci_push_data32(struct dw_mci *host, void *buf, int cnt)
{
	struct mmc_data *data = host->data;
	int init_cnt = cnt;

	/* try and push anything in the part_buf */
	if (unlikely(host->part_buf_count)) {
		int len = dw_mci_push_part_bytes(host, buf, cnt);
		buf += len;
		cnt -= len;
		if (host->part_buf_count == 4) {
			mci_writel(host, DATA(host->data_offset),
					host->part_buf32);
			host->part_buf_count = 0;
		}
	}
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x3)) {
		while (cnt >= 4) {
			u32 aligned_buf[32];
			int len = min(cnt & -4, (int)sizeof(aligned_buf));
			int items = len >> 2;
			int i;
			/* memcpy from input buffer into aligned buffer */
			memcpy(aligned_buf, buf, len);
			buf += len;
			cnt -= len;
			/* push data from aligned buffer into fifo */
			for (i = 0; i < items; ++i)
				mci_writel(host, DATA(host->data_offset),
						aligned_buf[i]);
		}
	} else
#endif
	{
		u32 *pdata = buf;
		for (; cnt >= 4; cnt -= 4)
			mci_writel(host, DATA(host->data_offset), *pdata++);
		buf = pdata;
	}
	/* put anything remaining in the part_buf */
	if (cnt) {
		dw_mci_set_part_bytes(host, buf, cnt);
		 /* Push data if we have reached the expected data length */
		if ((data->bytes_xfered + init_cnt) ==
		    (data->blksz * data->blocks))
			mci_writel(host, DATA(host->data_offset),
				   host->part_buf32);
	}
}

static void dw_mci_pull_data32(struct dw_mci *host, void *buf, int cnt)
{
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x3)) {
		while (cnt >= 4) {
			/* pull data from fifo into aligned buffer */
			u32 aligned_buf[32];
			int len = min(cnt & -4, (int)sizeof(aligned_buf));
			int items = len >> 2;
			int i;
			for (i = 0; i < items; ++i)
				aligned_buf[i] = mci_readl(host,
						DATA(host->data_offset));
			/* memcpy from aligned buffer into output buffer */
			memcpy(buf, aligned_buf, len);
			buf += len;
			cnt -= len;
		}
	} else
#endif
	{
		u32 *pdata = buf;
		for (; cnt >= 4; cnt -= 4)
			*pdata++ = mci_readl(host, DATA(host->data_offset));
		buf = pdata;
	}
	if (cnt) {
		host->part_buf32 = mci_readl(host, DATA(host->data_offset));
		dw_mci_pull_final_bytes(host, buf, cnt);
	}
}

static void dw_mci_push_data64(struct dw_mci *host, void *buf, int cnt)
{
	struct mmc_data *data = host->data;
	int init_cnt = cnt;

	/* try and push anything in the part_buf */
	if (unlikely(host->part_buf_count)) {
		int len = dw_mci_push_part_bytes(host, buf, cnt);
		buf += len;
		cnt -= len;

		if (host->part_buf_count == 8) {
			mci_writeq(host, DATA(host->data_offset),
					host->part_buf);
			host->part_buf_count = 0;
		}
	}
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x7)) {
		while (cnt >= 8) {
			u64 aligned_buf[16];
			int len = min(cnt & -8, (int)sizeof(aligned_buf));
			int items = len >> 3;
			int i;
			/* memcpy from input buffer into aligned buffer */
			memcpy(aligned_buf, buf, len);
			buf += len;
			cnt -= len;
			/* push data from aligned buffer into fifo */
			for (i = 0; i < items; ++i)
				mci_writeq(host, DATA(host->data_offset),
						aligned_buf[i]);
		}
	} else
#endif
	{
		u64 *pdata = buf;
		for (; cnt >= 8; cnt -= 8)
			mci_writeq(host, DATA(host->data_offset), *pdata++);
		buf = pdata;
	}
	/* put anything remaining in the part_buf */
	if (cnt) {
		dw_mci_set_part_bytes(host, buf, cnt);
		/* Push data if we have reached the expected data length */
		if ((data->bytes_xfered + init_cnt) ==
		    (data->blksz * data->blocks))
			mci_writeq(host, DATA(host->data_offset),
				   host->part_buf);
	}
}

static void dw_mci_pull_data64(struct dw_mci *host, void *buf, int cnt)
{
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & 0x7)) {
		while (cnt >= 8) {
			/* pull data from fifo into aligned buffer */
			u64 aligned_buf[16];
			int len = min(cnt & -8, (int)sizeof(aligned_buf));
			int items = len >> 3;
			int i;
			for (i = 0; i < items; ++i)
				aligned_buf[i] = mci_readq(host,
						DATA(host->data_offset));
			/* memcpy from aligned buffer into output buffer */
			memcpy(buf, aligned_buf, len);
			buf += len;
			cnt -= len;
		}
	} else
#endif
	{
		u64 *pdata = buf;
		for (; cnt >= 8; cnt -= 8)
			*pdata++ = mci_readq(host, DATA(host->data_offset));
		buf = pdata;
	}
	if (cnt) {
		host->part_buf = mci_readq(host, DATA(host->data_offset));
		dw_mci_pull_final_bytes(host, buf, cnt);
	}
}

static void dw_mci_pull_data(struct dw_mci *host, void *buf, int cnt)
{
	int len;

	/* get remaining partial bytes */
	len = dw_mci_pull_part_bytes(host, buf, cnt);
	if (unlikely(len == cnt))
		return;
	buf += len;
	cnt -= len;

	/* get the rest of the data */
	host->pull_data(host, buf, cnt);
}

static void dw_mci_read_data_pio(struct dw_mci *host, bool dto)
{
	struct sg_mapping_iter *sg_miter = &host->sg_miter;
	void *buf;
	unsigned int offset;
	struct mmc_data	*data = host->data;
	int shift = host->data_shift;
	u32 status;
	unsigned int len;
	unsigned int remain, fcnt;

	do {
		if (!sg_miter_next(sg_miter))
			goto done;

		host->sg = sg_miter->piter.sg;
		buf = sg_miter->addr;
		remain = sg_miter->length;
		offset = 0;

		do {
			fcnt = (SDMMC_GET_FCNT(mci_readl(host, STATUS))
					<< shift) + host->part_buf_count;
			len = min(remain, fcnt);
			if (!len)
				break;
			dw_mci_pull_data(host, (void *)(buf + offset), len);
			data->bytes_xfered += len;
			offset += len;
			remain -= len;
		} while (remain);

		sg_miter->consumed = offset;
		status = mci_readl(host, MINTSTS);
		mci_writel(host, RINTSTS, SDMMC_INT_RXDR);
	/* if the RXDR is ready read again */
	} while ((status & SDMMC_INT_RXDR) ||
		 (dto && SDMMC_GET_FCNT(mci_readl(host, STATUS))));

	if (!remain) {
		if (!sg_miter_next(sg_miter))
			goto done;
		sg_miter->consumed = 0;
	}
	sg_miter_stop(sg_miter);
	return;

done:
	sg_miter_stop(sg_miter);
	host->sg = NULL;
	smp_wmb();
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static void dw_mci_write_data_pio(struct dw_mci *host)
{
	struct sg_mapping_iter *sg_miter = &host->sg_miter;
	void *buf;
	unsigned int offset;
	struct mmc_data	*data = host->data;
	int shift = host->data_shift;
	u32 status;
	unsigned int len;
	unsigned int fifo_depth = host->fifo_depth;
	unsigned int remain, fcnt;

	do {
		if (!sg_miter_next(sg_miter))
			goto done;

		host->sg = sg_miter->piter.sg;
		buf = sg_miter->addr;
		remain = sg_miter->length;
		offset = 0;

		do {
			fcnt = ((fifo_depth -
				 SDMMC_GET_FCNT(mci_readl(host, STATUS)))
					<< shift) - host->part_buf_count;
			len = min(remain, fcnt);
			if (!len)
				break;
			host->push_data(host, (void *)(buf + offset), len);
			data->bytes_xfered += len;
			offset += len;
			remain -= len;
		} while (remain);

		sg_miter->consumed = offset;
		status = mci_readl(host, MINTSTS);
		mci_writel(host, RINTSTS, SDMMC_INT_TXDR);
	} while (status & SDMMC_INT_TXDR); /* if TXDR write again */

	if (!remain) {
		if (!sg_miter_next(sg_miter))
			goto done;
		sg_miter->consumed = 0;
	}
	sg_miter_stop(sg_miter);
	return;

done:
	sg_miter_stop(sg_miter);
	host->sg = NULL;
	smp_wmb();
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static void dw_mci_cmd_interrupt(struct dw_mci *host, u32 status)
{
	if (!host->cmd_status)
		host->cmd_status = status;

	smp_wmb();

	set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
	tasklet_schedule(&host->tasklet);
}

static irqreturn_t dw_mci_interrupt(int irq, void *dev_id)
{
	struct dw_mci *host = dev_id;
	u32 status, pending;
	int i;

	status = mci_readl(host, RINTSTS);
	pending = mci_readl(host, MINTSTS); /* read-only mask reg */

	if (dw_mci_debug_flag == 1)
		dev_err(host->dev, "## RINTSTS 0x %08x\n", pending);

	/*
	 * DTO fix - version 2.10a and below, and only if internal DMA
	 * is configured.
	 */
	if (host->quirks & DW_MCI_QUIRK_IDMAC_DTO) {
		if (!pending &&
		    ((mci_readl(host, STATUS) >> 17) & 0x1fff))
			pending |= SDMMC_INT_DATA_OVER;
	}

	if (pending) {
		if (pending & SDMMC_INT_HLE) {
			dev_err(host->dev, "hardware locked write error\n");
			dw_mci_reg_dump(host);
			mci_writel(host, RINTSTS, SDMMC_INT_HLE);
			dw_mci_debug_cmd_log(host->cmd, host, false,
					DW_MCI_FLAG_ERROR, status);
			host->cmd_status = pending;
			tasklet_schedule(&host->tasklet);
		}

		if (pending & DW_MCI_CMD_ERROR_FLAGS) {
			mci_writel(host, RINTSTS, DW_MCI_CMD_ERROR_FLAGS);
			host->cmd_status = pending;
			dw_mci_debug_cmd_log(host->cmd, host, false,
					DW_MCI_FLAG_ERROR, status);
			smp_wmb();
			set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
		}

		if (pending & DW_MCI_DATA_ERROR_FLAGS) {
			if (mci_readl(host, RINTSTS) & SDMMC_INT_HTO)
				dw_mci_reg_dump(host);

			/* if there is an error report DATA_ERROR */
			mci_writel(host, RINTSTS, DW_MCI_DATA_ERROR_FLAGS);
			dw_mci_debug_cmd_log(host->cmd, host, false,
					DW_MCI_FLAG_ERROR, status);
			host->data_status = pending;
			smp_wmb();
			set_bit(EVENT_DATA_ERROR, &host->pending_events);
			tasklet_schedule(&host->tasklet);
		}

		if (pending & SDMMC_INT_DATA_OVER) {
			mci_writel(host, RINTSTS, SDMMC_INT_DATA_OVER);
			dw_mci_debug_cmd_log(host->cmd, host, false,
					DW_MCI_FLAG_DTO, 0);
			if (!host->data_status)
				host->data_status = pending;
			smp_wmb();
			if (host->dir_status == DW_MCI_RECV_STATUS) {
				if (host->sg != NULL)
					dw_mci_read_data_pio(host, true);
			}
			set_bit(EVENT_DATA_COMPLETE, &host->pending_events);
			tasklet_schedule(&host->tasklet);
		}

		if (pending & SDMMC_INT_RXDR) {
			mci_writel(host, RINTSTS, SDMMC_INT_RXDR);
			if (host->dir_status == DW_MCI_RECV_STATUS && host->sg)
				dw_mci_read_data_pio(host, false);
		}

		if (pending & SDMMC_INT_TXDR) {
			mci_writel(host, RINTSTS, SDMMC_INT_TXDR);
			if (host->dir_status == DW_MCI_SEND_STATUS && host->sg)
				dw_mci_write_data_pio(host);
		}

		if (pending & SDMMC_INT_CMD_DONE) {
			mci_writel(host, RINTSTS, SDMMC_INT_CMD_DONE);
			dw_mci_debug_cmd_log(host->cmd, host, false,
					DW_MCI_FLAG_CD, 0);
			dw_mci_cmd_interrupt(host, pending);
		}

		if (pending & SDMMC_INT_CD) {
			mci_writel(host, RINTSTS, SDMMC_INT_CD);
			queue_work(host->card_workqueue, &host->card_work);
		}

		/* Handle SDIO Interrupts */
		for (i = 0; i < host->num_slots; i++) {
			struct dw_mci_slot *slot = host->slot[i];
			if (pending & SDMMC_INT_SDIO(i)) {
				mci_writel(host, RINTSTS, SDMMC_INT_SDIO(i));
				mmc_signal_sdio_irq(slot->mmc);
			}
		}

	}

#ifdef CONFIG_MMC_DW_IDMAC
	/* Handle DMA interrupts */
	if (host->dma_64bit_address == true) {
		pending = mci_readl(host, IDSTS64);
		if (pending & (SDMMC_IDMAC_INT_TI | SDMMC_IDMAC_INT_RI)) {
			mci_writel(host, IDSTS64, SDMMC_IDMAC_INT_TI |
							SDMMC_IDMAC_INT_RI);
			mci_writel(host, IDSTS64, SDMMC_IDMAC_INT_NI);
			host->dma_ops->complete(host);
		}
	} else {
		pending = mci_readl(host, IDSTS);
		if (pending & (SDMMC_IDMAC_INT_TI | SDMMC_IDMAC_INT_RI)) {
			mci_writel(host, IDSTS, SDMMC_IDMAC_INT_TI |
							SDMMC_IDMAC_INT_RI);
			mci_writel(host, IDSTS, SDMMC_IDMAC_INT_NI);
			host->dma_ops->complete(host);
		}
	}
#endif

	return IRQ_HANDLED;
}

static void dw_mci_timeout_timer(unsigned long data)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct mmc_request *mrq;

	if (host && host->mrq) {
		host->sw_timeout_chk = true;
		mrq = host->mrq;

		if (!(mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK ||
				mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200)) {
			dev_err(host->dev,
					"Timeout waiting for hardware interrupt."
					" state = %d\n", host->state);
			dw_mci_reg_dump(host);
		}
		spin_lock(&host->lock);

		host->sg = NULL;
		host->data = NULL;
		host->cmd = NULL;

		switch (host->state) {
		case STATE_IDLE:
		case STATE_WAITING_CMD11_DONE:
			break;
		case STATE_SENDING_CMD11:
		case STATE_SENDING_CMD:
			mrq->cmd->error = -ENOMEDIUM;
			if (!mrq->data)
				break;
			/* fall through */
		case STATE_SENDING_DATA:
			mrq->data->error = -ENOMEDIUM;
			dw_mci_stop_dma(host);
			break;
		case STATE_DATA_BUSY:
		case STATE_DATA_ERROR:
			if (mrq->data->error == -EINPROGRESS)
				mrq->data->error = -ENOMEDIUM;
			/* fall through */
		case STATE_SENDING_STOP:
			if (mrq->stop)
				mrq->stop->error = -ENOMEDIUM;
			break;
		}

		spin_unlock(&host->lock);
		dw_mci_ciu_reset(host->dev, host);
		dw_mci_fifo_reset(host->dev, host);
		spin_lock(&host->lock);
		dw_mci_request_end(host, mrq);
		host->state = STATE_IDLE;
		spin_unlock(&host->lock);
		host->sw_timeout_chk = false;
	}
}

static void dw_mci_work_routine_card(struct work_struct *work)
{
	struct dw_mci *host = container_of(work, struct dw_mci, card_work);
	int i;

	for (i = 0; i < host->num_slots; i++) {
		struct dw_mci_slot *slot = host->slot[i];
		struct mmc_host *mmc = slot->mmc;
		struct mmc_request *mrq;
		int present;

		present = dw_mci_get_cd(mmc);
		while (present != slot->last_detect_state) {
			dev_info(&slot->mmc->class_dev, "card %s\n",
				present ? "inserted" : "removed");

			spin_lock_bh(&host->lock);

			/* Card change detected */
			slot->last_detect_state = present;

			/* Clean up queue if present */
			mrq = slot->mrq;
			if (mrq) {
				if (mrq == host->mrq) {
					host->data = NULL;
					host->cmd = NULL;

					switch (host->state) {
					case STATE_IDLE:
					case STATE_WAITING_CMD11_DONE:
						break;
					case STATE_SENDING_CMD11:
					case STATE_SENDING_CMD:
						mrq->cmd->error = -ENOMEDIUM;
						if (!mrq->data)
							break;
						/* fall through */
					case STATE_SENDING_DATA:
						mrq->data->error = -ENOMEDIUM;
						dw_mci_stop_dma(host);
						break;
					case STATE_DATA_BUSY:
					case STATE_DATA_ERROR:
						if (mrq->data->error == -EINPROGRESS)
							mrq->data->error = -ENOMEDIUM;
						/* fall through */
					case STATE_SENDING_STOP:
						if (mrq->stop)
							mrq->stop->error = -ENOMEDIUM;
						break;
					}

					dw_mci_request_end(host, mrq);
				} else {
					list_del(&slot->queue_node);
					mrq->cmd->error = -ENOMEDIUM;
					if (mrq->data)
						mrq->data->error = -ENOMEDIUM;
					if (mrq->stop)
						mrq->stop->error = -ENOMEDIUM;

					del_timer(&host->timer);
					spin_unlock(&host->lock);
					mmc_request_done(slot->mmc, mrq);
					spin_lock(&host->lock);
				}
			}

			/* Power down slot */
			if (present == 0){
				clear_bit(DW_MMC_CARD_PRESENT, &slot->flags);

				/*
				 * Clear down the FIFO - doing so generates a
				 * block interrupt, hence setting the
				 * scatter-gather pointer to NULL.
				 */
				sg_miter_stop(&host->sg_miter);
				host->sg = NULL;
#ifdef CONFIG_MMC_DW_IDMAC
				dw_mci_idma_reset_dma(host);
#endif
				dw_mci_ciu_reset(host->dev, host);
				dw_mci_fifo_reset(host->dev, host);
			} else if (host->cur_slot) {
				dw_mci_ciu_reset(host->dev, host);
				mci_writel(host, RINTSTS, 0xFFFFFFFF);
			}

			spin_unlock_bh(&host->lock);

			present = dw_mci_get_cd(mmc);
		}

		if (present) {
			mmc_detect_change(slot->mmc,
				msecs_to_jiffies(host->pdata->detect_delay_ms));
		} else {
			mmc_detect_change(slot->mmc,0);
			if (host->pdata->only_once_tune)
				host->pdata->tuned = false;
		}
	}
}

#ifdef CONFIG_OF
/* given a slot id, find out the device node representing that slot */
static struct device_node *dw_mci_of_find_slot_node(struct device *dev, u8 slot)
{
	struct device_node *np;
	const __be32 *addr;
	int len;

	if (!dev || !dev->of_node)
		return NULL;

	for_each_child_of_node(dev->of_node, np) {
		addr = of_get_property(np, "reg", &len);
		if (!addr || (len < sizeof(int)))
			continue;
		if (be32_to_cpup(addr) == slot)
			return np;
	}
	return NULL;
}

static struct dw_mci_of_slot_quirks {
	char *quirk;
	int id;
} of_slot_quirks[] = {
	{
		.quirk	= "disable-wp",
		.id	= DW_MCI_SLOT_QUIRK_NO_WRITE_PROTECT,
	},
};

static int dw_mci_of_get_slot_quirks(struct device *dev, u8 slot)
{
	struct device_node *np = dw_mci_of_find_slot_node(dev, slot);
	int quirks = 0;
	int idx;

	/* get quirks */
	for (idx = 0; idx < ARRAY_SIZE(of_slot_quirks); idx++)
		if (of_get_property(np, of_slot_quirks[idx].quirk, NULL)) {
			dev_warn(dev, "Slot quirk %s is deprecated\n",
					of_slot_quirks[idx].quirk);
			quirks |= of_slot_quirks[idx].id;
		}

	return quirks;
}
#else /* CONFIG_OF */
static int dw_mci_of_get_slot_quirks(struct device *dev, u8 slot)
{
	return 0;
}
#endif /* CONFIG_OF */

static irqreturn_t dw_mci_detect_interrupt(int irq, void *dev_id)
{
	struct dw_mci *host = dev_id;

	if (host->card_detect_cnt < 0x7FFFFFF0)
		host->card_detect_cnt++;
	queue_work(host->card_workqueue, &host->card_work);

	return IRQ_HANDLED;
}

static int dw_mci_init_slot(struct dw_mci *host, unsigned int id)
{
	struct mmc_host *mmc;
	struct dw_mci_slot *slot;
	struct dw_mci_sfe_ram_dump *dump;

	const struct dw_mci_drv_data *drv_data = host->drv_data;
	int ctrl_id, ret;
	u32 freq[2];

	mmc = mmc_alloc_host(sizeof(struct dw_mci_slot), host->dev);
	if (!mmc)
		return -ENOMEM;
	dump = devm_kzalloc(host->dev, sizeof(*dump), GFP_KERNEL);
	if (!dump) {
		dev_err(host->dev,"sfr dump memory alloc faile!\n");
		return -ENOMEM;
	}
	host->sfr_dump = dump;
	slot = mmc_priv(mmc);
	slot->id = id;
	slot->mmc = mmc;
	slot->host = host;
	host->slot[id] = slot;
#ifdef CONFIG_MMC_CLKGATE
	mmc->clkgate_delay = 10;
#endif
	slot->quirks = dw_mci_of_get_slot_quirks(host->dev, slot->id);

	mmc->ops = &dw_mci_ops;
	if (of_property_read_u32_array(host->dev->of_node,
				       "clock-freq-min-max", freq, 2)) {
		mmc->f_min = DW_MCI_FREQ_MIN;
		mmc->f_max = DW_MCI_FREQ_MAX;
	} else {
		mmc->f_min = freq[0];
		mmc->f_max = freq[1];
	}

	/*if there are external regulators, get them*/
	if (!(host->quirks & DW_MMC_QUIRK_FIXED_VOLTAGE)) {
		ret = mmc_regulator_get_supply(mmc);

		if (ret == -EPROBE_DEFER)
			goto err_host_allocated;
	}

	if (!mmc->ocr_avail)
		mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	if (host->pdata->caps)
		mmc->caps = host->pdata->caps;

	if (host->pdata->pm_caps)
		mmc->pm_caps = host->pdata->pm_caps;

	if (host->dev->of_node) {
		ctrl_id = of_alias_get_id(host->dev->of_node, "mshc");
		host->channel = ctrl_id;
		if (ctrl_id < 0)
			ctrl_id = 0;
	} else {
		ctrl_id = to_platform_device(host->dev)->id;
	}
	if (drv_data && drv_data->caps)
		mmc->caps |= drv_data->caps[ctrl_id];

	if (host->pdata->caps2)
		mmc->caps2 = host->pdata->caps2;

	ret = mmc_of_parse(mmc);
	if (ret)
		goto err_host_allocated;

	if (host->pdata->blk_settings) {
		mmc->max_segs = host->pdata->blk_settings->max_segs;
		mmc->max_blk_size = host->pdata->blk_settings->max_blk_size;
		mmc->max_blk_count = host->pdata->blk_settings->max_blk_count;
		mmc->max_req_size = host->pdata->blk_settings->max_req_size;
		mmc->max_seg_size = host->pdata->blk_settings->max_seg_size;
	} else {
		/* Useful defaults if platform data is unset. */
#ifdef CONFIG_MMC_DW_IDMAC
		mmc->max_segs = host->ring_size;
		mmc->max_blk_size = 65536;
		mmc->max_seg_size = 0x1000;
		mmc->max_req_size = mmc->max_seg_size * host->ring_size;
		mmc->max_blk_count = mmc->max_req_size / 512;
#else
		mmc->max_segs = 64;
		mmc->max_blk_size = 65536; /* BLKSIZ is 16 bits */
		mmc->max_blk_count = 512;
		mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
		mmc->max_seg_size = mmc->max_req_size;
#endif /* CONFIG_MMC_DW_IDMAC */
	}

	if (dw_mci_get_cd(mmc))
		set_bit(DW_MMC_CARD_PRESENT, &slot->flags);
	else
		clear_bit(DW_MMC_CARD_PRESENT, &slot->flags);
	
	mmc->caps2 |= MMC_CAP2_NO_PRESCAN_POWERUP;

	ret = mmc_add_host(mmc);
	if (ret)
		goto err_host_allocated;

#if defined(CONFIG_DEBUG_FS)
	dw_mci_init_debugfs(slot);
#endif

	/* Card initially undetected */
	slot->last_detect_state = 0;

	/* For argos */
	dw_mci_transferred_cnt_init(host, mmc);

	return 0;

err_host_allocated:
	mmc_free_host(mmc);
	return ret;
}

static void dw_mci_cleanup_slot(struct dw_mci_slot *slot, unsigned int id)
{
	/* Debugfs stuff is cleaned up by mmc core */
	mmc_remove_host(slot->mmc);
	slot->host->slot[id] = NULL;
	mmc_free_host(slot->mmc);
}

static void dw_mci_init_dma(struct dw_mci *host)
{
	int addr_config;
	/* Check ADDR_CONFIG bit in HCON to find IDMAC address bus width */
	addr_config = (mci_readl(host, HCON) >> 27) & 0x01;

	if (addr_config == 1) {
		/* host supports IDMAC in 64-bit address mode */
		host->dma_64bit_address = true;
		dev_info(host->dev, "IDMAC supports 64-bit address mode.\n");
		if (!dma_set_mask(host->dev, DMA_BIT_MASK(64)))
			dma_set_coherent_mask(host->dev, DMA_BIT_MASK(64));
	} else {
		/* host supports IDMAC in 32-bit address mode */
		host->dma_64bit_address = false;
		dev_info(host->dev, "IDMAC supports 32-bit address mode.\n");
	}

	if (host->pdata->desc_sz)
		host->desc_sz = host->pdata->desc_sz;
	 else
		 host->desc_sz = 1;

	/* Alloc memory for sg translation */
	host->sg_cpu = dmam_alloc_coherent(host->dev, host->desc_sz * PAGE_SIZE,
					  &host->sg_dma, GFP_KERNEL);
	if (!host->sg_cpu) {
		dev_err(host->dev, "%s: could not alloc DMA memory\n",
			__func__);
		goto no_dma;
	}

	/* Determine which DMA interface to use */
#ifdef CONFIG_MMC_DW_IDMAC
	host->dma_ops = &dw_mci_idmac_ops;
	dev_info(host->dev, "Using internal DMA controller.\n");
#endif

	if (!host->dma_ops)
		goto no_dma;

	if (host->dma_ops->init && host->dma_ops->start &&
	    host->dma_ops->stop && host->dma_ops->cleanup) {
		if (host->dma_ops->init(host)) {
			dev_err(host->dev, "%s: Unable to initialize "
				"DMA Controller.\n", __func__);
			goto no_dma;
		}
	} else {
		dev_err(host->dev, "DMA initialization not found.\n");
		goto no_dma;
	}

	host->use_dma = 1;
	return;

no_dma:
	dev_info(host->dev, "Using PIO mode.\n");
	host->use_dma = 0;
	return;
}

static bool dw_mci_ctrl_reset(struct dw_mci *host, u32 reset)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(500);
	u32 ctrl;
	unsigned int int_mask = 0;
	u32 clksel_saved = 0x0;
	bool ret = false;

	/* Interrupt disable */
	ctrl = dw_mci_disable_interrupt(host, &int_mask);

	/* set Rx timing to 0 */
	clksel_saved = mci_readl(host, CLKSEL);
	mci_writel(host, CLKSEL, clksel_saved & ~(0x3 << 6 | 0x7));

	/* Reset */
	ctrl |= reset;
	mci_writel(host, CTRL, ctrl);

	/* All interrupt clear */
	mci_writel(host, RINTSTS, 0xFFFFFFFF);

	/* Interrupt enable */
	dw_mci_enable_interrupt(host, int_mask);

	/* wait till resets clear */
	do {
		if (!(mci_readl(host, CTRL) & reset)) {
			ret = true;
			break;
		}
	} while (time_before(jiffies, timeout));

	if (!ret)
		dev_err(host->dev, "Timeout resetting block (ctrl %#x)\n", ctrl);

	/* restore Rx timing */
	mci_writel(host, CLKSEL, clksel_saved);

	return ret;

}

void dw_mci_ciu_reset(struct device *dev, struct dw_mci *host) {
	struct dw_mci_slot *slot = host->cur_slot;
	unsigned long timeout = jiffies + msecs_to_jiffies(10);
	int retry = 10;
	u32 status;
	if (slot) {
		dw_mci_ctrl_reset(host, SDMMC_CTRL_RESET);
		/* Check For DATA busy */
		do {
			while (time_before(jiffies, timeout)) {
				status = mci_readl(host, STATUS);
				if (!(status & SDMMC_STATUS_BUSY))
					goto out;
			}
			dw_mci_ctrl_reset(host, SDMMC_CTRL_RESET);
			timeout = jiffies + msecs_to_jiffies(10);
		} while (--retry);
out:

	/* After a CTRL reset we need to have CIU set clock registers  */
		dw_mci_update_clock(slot);
	}
}

bool dw_mci_fifo_reset(struct device *dev, struct dw_mci *host)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);
	unsigned int ctrl;
	bool result;

	do {
		result = dw_mci_ctrl_reset(host, SDMMC_CTRL_FIFO_RESET);
		if (!result)
			break;

		ctrl = mci_readl(host, STATUS);
		if (!(ctrl & SDMMC_STATUS_DMA_REQ)) {
			result = dw_mci_ctrl_reset(host, SDMMC_CTRL_FIFO_RESET);
			if (result) {
				/* clear exception raw interrupts can not be handled
				   ex) fifo full => RXDR interrupt rising */
				ctrl = mci_readl(host, RINTSTS);
				ctrl = ctrl & ~(mci_readl(host, MINTSTS));
				if (ctrl)
					mci_writel(host, RINTSTS, ctrl);

				return true;
			}
		}
	} while (time_before(jiffies, timeout));

	dev_err(dev, "%s: Timeout while resetting host controller after err\n",
		__func__);

	return false;
}

#ifdef CONFIG_OF
static struct dw_mci_of_quirks {
	char *quirk;
	int id;
} of_quirks[] = {
	{
		.quirk	= "broken-cd",
		.id	= DW_MCI_QUIRK_BROKEN_CARD_DETECTION,
	}, {
		.quirk	= "disable-wp",
		.id	= DW_MCI_QUIRK_NO_WRITE_PROTECT,
	}, {
		.quirk  = "bypass-smu",
		.id	= DW_MCI_QUIRK_BYPASS_SMU,
	}, {
		.quirk  = "fixed_voltage",
		.id	= DW_MMC_QUIRK_FIXED_VOLTAGE,
	}, {
		.quirk  = "card-init-hwacg-ctrl",
		.id	= DW_MCI_QUIRK_HWACG_CTRL,
	}, {
		.quirk  = "enable-ulp-mode",
		.id	= DW_MCI_QUIRK_ENABLE_ULP,
	}, {
		.quirk  = "switch_transfer",
		.id	= DW_MCI_SW_TRANS,
	},
};

static struct dw_mci_board *dw_mci_parse_dt(struct dw_mci *host)
{
	struct dw_mci_board *pdata;
	struct device *dev = host->dev;
	struct device_node *np = dev->of_node;
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	int idx, ret;
	u32 clock_frequency;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for pdata\n");
		return ERR_PTR(-ENOMEM);
	}

	/* find out number of slots supported */
	if (of_property_read_u32(dev->of_node, "num-slots",
				&pdata->num_slots)) {
		dev_info(dev, "num-slots property not found, "
				"assuming 1 slot is available\n");
		pdata->num_slots = 1;
	}

	/* get quirks */
	for (idx = 0; idx < ARRAY_SIZE(of_quirks); idx++)
		if (of_get_property(np, of_quirks[idx].quirk, NULL))
			pdata->quirks |= of_quirks[idx].id;

	if (of_property_read_u32(np, "fifo-depth", &pdata->fifo_depth))
		dev_info(dev, "fifo-depth property not found, using "
				"value of FIFOTH register as default\n");

	of_property_read_u32(np, "card-detect-delay", &pdata->detect_delay_ms);
	of_property_read_u32(np, "qos_int_level", &pdata->qos_int_level);
	of_property_read_u32(np, "data-timeout", &pdata->data_timeout);
	of_property_read_u32(np, "hto-timeout", &pdata->hto_timeout);
	of_property_read_u32(np, "desc-size", &pdata->desc_sz);

	if (!of_property_read_u32(np, "clock-frequency", &clock_frequency))
		pdata->bus_hz = clock_frequency;

	if (of_find_property(np, "only_once_tune", NULL))
		pdata->only_once_tune = true;

	if (of_find_property(np, "supports-vqmmc19", NULL))
		pdata->use_vqmmc19 = true;

	if (drv_data && drv_data->parse_dt) {
		ret = drv_data->parse_dt(host);
		if (ret)
			return ERR_PTR(ret);
	}

	if (of_find_property(np, "supports-highspeed", NULL))
		pdata->caps |= MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED;

	if (of_find_property(np, "clock-gate", NULL))
		pdata->use_gate_clock = true;

	/* caps */

	if (of_find_property(np, "supports-8bit", NULL))
		pdata->caps |= MMC_CAP_8_BIT_DATA;

	if (of_find_property(np, "supports-4bit", NULL))
		pdata->caps |= MMC_CAP_4_BIT_DATA;

	if (of_find_property(np, "supports-cmd23", NULL))
		pdata->caps |= MMC_CAP_CMD23;

	if (of_find_property(np, "supports-erase", NULL))
		pdata->caps |= MMC_CAP_ERASE;

	if (of_find_property(np, "pm-skip-mmc-resume-init", NULL))
		pdata->pm_caps |= MMC_PM_SKIP_MMC_RESUME_INIT;
	if (of_find_property(np, "card-detect-invert-gpio", NULL))
		pdata->caps2 |= MMC_CAP2_CD_ACTIVE_HIGH;

	if (of_find_property(np, "card-detect-gpio", NULL)) {
		pdata->cd_type = DW_MCI_CD_GPIO;
		pdata->caps2 |= MMC_CAP2_DETECT_ON_ERR;
		/* to remove power on period without tray, default enable */
		pdata->caps2 |= MMC_CAP2_NO_PRESCAN_POWERUP;
	}

	return pdata;
}

#else /* CONFIG_OF */
static struct dw_mci_board *dw_mci_parse_dt(struct dw_mci *host)
{
	return ERR_PTR(-EINVAL);
}
#endif /* CONFIG_OF */

int dw_mci_probe(struct dw_mci *host)
{
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	int width, i, ret = 0;
	u32 fifo_size, msize, tx_wmark, rx_wmark;
	int init_slots = 0;

	if (!host->pdata) {
		host->pdata = dw_mci_parse_dt(host);
		if (IS_ERR(host->pdata)) {
			dev_err(host->dev, "platform data not available\n");
			return -EINVAL;
		}
	}

	host->pdata->tuned = false;

	if (host->pdata->num_slots > 1) {
		dev_err(host->dev,
			"Platform data must supply num_slots.\n");
		return -ENODEV;
	}

	host->biu_clk = devm_clk_get(host->dev, "biu");
	if (IS_ERR(host->biu_clk)) {
		dev_dbg(host->dev, "biu clock not available\n");
	} else {
		ret = dw_mci_biu_clk_en(host, true);
		if (ret) {
			dev_err(host->dev, "failed to enable biu clock\n");
			return ret;
		}
	}

	host->ciu_gate = devm_clk_get(host->dev, "ciu_gate");
	if (IS_ERR(host->ciu_gate))
		dev_dbg(host->dev, "not used ciu gate clock or not available\n");

	host->ciu_clk = devm_clk_get(host->dev, "ciu");
	if (IS_ERR(host->ciu_clk)) {
		dev_dbg(host->dev, "ciu clock not available\n");
		host->bus_hz = host->pdata->bus_hz;
	} else {
		ret = dw_mci_ciu_clk_en(host, true);
		if (ret) {
			dev_err(host->dev, "failed to enable ciu clock\n");
			goto err_clk_biu;
		}

		if (host->pdata->bus_hz) {
			if(!IS_ERR(host->ciu_gate))
				ret = clk_set_rate(host->ciu_gate, host->pdata->bus_hz);
			else
				ret = clk_set_rate(host->ciu_clk, host->pdata->bus_hz);
			if (ret)
				dev_warn(host->dev,
					 "Unable to set bus rate to %uHz\n",
					 host->pdata->bus_hz);
		}
		host->bus_hz = clk_get_rate(host->ciu_clk);
	}

	if (!host->bus_hz) {
		dev_err(host->dev,
			"Platform data must supply bus speed\n");
		ret = -ENODEV;
		goto err_clk_ciu;
	}

	if (drv_data && drv_data->init) {
		ret = drv_data->init(host);
		if (ret) {
			dev_err(host->dev,
				"implementation specific init failed\n");
			goto err_clk_ciu;
		}
	}

	if (drv_data && drv_data->setup_clock) {
		ret = drv_data->setup_clock(host);
		if (ret) {
			dev_err(host->dev,
				"implementation specific clock setup failed\n");
			goto err_clk_ciu;
		}
	}

	host->quirks = host->pdata->quirks;

#ifdef CONFIG_CPU_IDLE
	host->idle_ip_index = exynos_get_idle_ip_index(dev_name(host->dev));
	exynos_update_ip_idle_status(host->idle_ip_index, 0);
#endif

	if (host->quirks & DW_MCI_QUIRK_HWACG_CTRL)
		host->qactive_check = HWACG_Q_ACTIVE_EN;
	if ((host->pdata->quirks & DW_MCI_QUIRK_BYPASS_SMU) && drv_data && drv_data->cfg_smu)
		drv_data->cfg_smu(host);

	spin_lock_init(&host->lock);
	INIT_LIST_HEAD(&host->queue);

	/*
	 * Get the host data width - this assumes that HCON has been set with
	 * the correct values.
	 */
	i = (mci_readl(host, HCON) >> 7) & 0x7;
	if (!i) {
		host->push_data = dw_mci_push_data16;
		host->pull_data = dw_mci_pull_data16;
		width = 16;
		host->data_shift = 1;
	} else if (i == 2) {
		host->push_data = dw_mci_push_data64;
		host->pull_data = dw_mci_pull_data64;
		width = 64;
		host->data_shift = 3;
	} else {
		/* Check for a reserved value, and warn if it is */
		WARN((i != 1),
		     "HCON reports a reserved host data width!\n"
		     "Defaulting to 32-bit access.\n");
		host->push_data = dw_mci_push_data32;
		host->pull_data = dw_mci_pull_data32;
		width = 32;
		host->data_shift = 2;
	}

	/* Reset all blocks */
	if (!dw_mci_ctrl_reset(host, SDMMC_CTRL_ALL_RESET_FLAGS))
		return -ENODEV;

	host->dma_ops = host->pdata->dma_ops;
	dw_mci_init_dma(host);

	/* Clear the interrupts for the host controller */
	mci_writel(host, RINTSTS, 0xFFFFFFFF);
	mci_writel(host, INTMASK, 0); /* disable all mmc interrupt first */

	/* Put in max timeout */
	mci_writel(host, TMOUT, 0xFFFFFFFF);

	/*
	 * FIFO threshold settings  RxMark  = fifo_size / 2 - 1,
	 *                          Tx Mark = fifo_size / 2 DMA Size = 8
	 */
	if (!host->pdata->fifo_depth) {
		/*
		 * Power-on value of RX_WMark is FIFO_DEPTH-1, but this may
		 * have been overwritten by the bootloader, just like we're
		 * about to do, so if you know the value for your hardware, you
		 * should put it in the platform data.
		 */
		fifo_size = mci_readl(host, FIFOTH);
		fifo_size = 1 + ((fifo_size >> 16) & 0xfff);
	} else {
		fifo_size = host->pdata->fifo_depth;
	}
	host->fifo_depth = fifo_size;

	WARN_ON(fifo_size < 8);

	/*
	 *	HCON[9:7] -> H_DATA_WIDTH
	 *	000 16 bits
	 *	001 32 bits
	 *	010 64 bits
	 *
	 *	FIFOTH[30:28] -> DW_DMA_Mutiple_Transaction_Size
	 *	msize:
	 *	000  1 transfers
	 *	001  4
	 *	010  8
	 *	011  16
	 *	100  32
	 *	101  64
	 *	110  128
	 *	111  256
	 *
	 *	AHB Master can support 1/4/8/16 burst in DMA.
	 *	So, Max support burst spec is 16 burst.
	 *
	 *	msize <= 011(16 burst)
	 *	Transaction_Size = msize * H_DATA_WIDTH;
	 *	rx_wmark = Transaction_Size - 1;
	 *	tx_wmark = fifo_size - Transaction_Size;
	 */
	msize = host->data_shift;
	msize &= 7;
	rx_wmark = ((1 << (msize + 1)) - 1) & 0xfff;
	tx_wmark = (fifo_size - (1 << (msize + 1))) & 0xfff;

	host->fifoth_val = msize << SDMMC_FIFOTH_DMA_MULTI_TRANS_SIZE;
	host->fifoth_val |= (rx_wmark << SDMMC_FIFOTH_RX_WMARK) | tx_wmark;

	mci_writel(host, FIFOTH, host->fifoth_val);

	dev_info(host->dev, "FIFOTH: 0x %08x", mci_readl(host, FIFOTH));

	/* disable clock to CIU */
	mci_writel(host, CLKENA, 0);
	mci_writel(host, CLKSRC, 0);

	/*
	 * In 2.40a spec, Data offset is changed.
	 * Need to check the version-id and set data-offset for DATA register.
	 */
	host->verid = SDMMC_GET_VERID(mci_readl(host, VERID));
	dev_info(host->dev, "Version ID is %04x\n", host->verid);

	if (host->verid < DW_MMC_240A)
		host->data_offset = DATA_OFFSET;
	else
		host->data_offset = DATA_240A_OFFSET;

	tasklet_init(&host->tasklet, dw_mci_tasklet_func, (unsigned long)host);
	host->card_workqueue = alloc_workqueue("dw-mci-card",
			WQ_MEM_RECLAIM, 1);
	if (!host->card_workqueue) {
		ret = -ENOMEM;
		goto err_dmaunmap;
	}
	INIT_WORK(&host->card_work, dw_mci_work_routine_card);

	/* INT min lock */
	pm_qos_add_request(&host->pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);

	ret = devm_request_irq(host->dev, host->irq, dw_mci_interrupt,
			       host->irq_flags, "dw-mci", host);

	setup_timer(&host->timer, dw_mci_timeout_timer, (unsigned long)host);
	host->sw_timeout_chk = false;

	if (ret)
		goto err_workqueue;

	if (host->pdata->num_slots)
		host->num_slots = host->pdata->num_slots;
	else
		host->num_slots = ((mci_readl(host, HCON) >> 1) & 0x1F) + 1;

	/*
	 * Enable interrupts for command done, data over, data empty, card det,
	 * receive ready and error such as transmit, receive timeout, crc error
	 */
	mci_writel(host, RINTSTS, 0xFFFFFFFF);
	if (host->pdata->cd_type == DW_MCI_CD_INTERNAL)
		mci_writel(host, INTMASK, SDMMC_INT_CMD_DONE | SDMMC_INT_DATA_OVER |
				SDMMC_INT_TXDR | SDMMC_INT_RXDR |
				DW_MCI_ERROR_FLAGS | SDMMC_INT_CD);
	else
		mci_writel(host, INTMASK, SDMMC_INT_CMD_DONE | SDMMC_INT_DATA_OVER |
				SDMMC_INT_TXDR | SDMMC_INT_RXDR |
				DW_MCI_ERROR_FLAGS);
	mci_writel(host, CTRL, SDMMC_CTRL_INT_ENABLE); /* Enable mci interrupt */

	dev_info(host->dev, "DW MMC controller at irq %d, "
		 "%d bit host data width, "
		 "%u deep fifo\n",
		 host->irq, width, fifo_size);

	/* We need at least one slot to succeed */
	for (i = 0; i < host->num_slots; i++) {
		ret = dw_mci_init_slot(host, i);
		if (ret)
			dev_dbg(host->dev, "slot %d init failed\n", i);
		else
			init_slots++;
	}

	dw_mci_debug_init(host);

	if (init_slots) {
		dev_info(host->dev, "%d slots initialized\n", init_slots);
	} else {
		dev_dbg(host->dev, "attempted to initialize %d slots, "
					"but failed on all\n", host->num_slots);
		goto err_workqueue;
	}

	 if (drv_data && drv_data->misc_control
			 && host->pdata->cd_type == DW_MCI_CD_GPIO)
		 drv_data->misc_control(host, CTRL_REQUEST_EXT_IRQ,
				 dw_mci_detect_interrupt);

	if (host->quirks & DW_MCI_QUIRK_IDMAC_DTO)
		dev_info(host->dev, "Internal DMAC interrupt fix enabled.\n");

	host->card_detect_cnt = 0;
	return 0;

err_workqueue:
	destroy_workqueue(host->card_workqueue);
	pm_qos_remove_request(&host->pm_qos_int);

err_dmaunmap:
	if (host->use_dma && host->dma_ops->exit)
		host->dma_ops->exit(host);

err_clk_ciu:
	if (!IS_ERR(host->ciu_clk))
		dw_mci_ciu_clk_dis(host);

err_clk_biu:
	if (!IS_ERR(host->biu_clk))
		dw_mci_biu_clk_dis(host);

	return ret;
}
EXPORT_SYMBOL(dw_mci_probe);

void dw_mci_remove(struct dw_mci *host)
{
	int i;

	mci_writel(host, RINTSTS, 0xFFFFFFFF);
	mci_writel(host, INTMASK, 0); /* disable all mmc interrupt first */

	for (i = 0; i < host->num_slots; i++) {
		dev_dbg(host->dev, "remove slot %d\n", i);
		if (host->slot[i])
			dw_mci_cleanup_slot(host->slot[i], i);
	}

	/* disable clock to CIU */
	mci_writel(host, CLKENA, 0);
	mci_writel(host, CLKSRC, 0);

	del_timer_sync(&host->timer);
	destroy_workqueue(host->card_workqueue);
	pm_qos_remove_request(&host->pm_qos_int);

	if (host->use_dma && host->dma_ops->exit)
		host->dma_ops->exit(host);

	if (!IS_ERR(host->ciu_clk))
		dw_mci_ciu_clk_dis(host);

	if (!IS_ERR(host->biu_clk))
		dw_mci_biu_clk_dis(host);
}
EXPORT_SYMBOL(dw_mci_remove);



#ifdef CONFIG_PM_SLEEP
/*
 * TODO: we should probably disable the clock to the card in the suspend path.
 */
int dw_mci_suspend(struct dw_mci *host)
{
	return 0;
}
EXPORT_SYMBOL(dw_mci_suspend);

int dw_mci_resume(struct dw_mci *host)
{
	const struct dw_mci_drv_data *drv_data = host->drv_data;
	int i, ret;

	ret = dw_mci_ciu_clk_en(host, false);
	if (ret) {
		dev_err(host->dev, "failed to enable ciu clock\n");
		return ret;
	}

	if (!dw_mci_ctrl_reset(host, SDMMC_CTRL_ALL_RESET_FLAGS)) {
		ret = -ENODEV;
		return ret;
	}

	if (host->use_dma && host->dma_ops->init)
		host->dma_ops->init(host);

	if (host->pdata->quirks & DW_MCI_QUIRK_HWACG_CTRL) {
		host->qactive_check = HWACG_Q_ACTIVE_DIS;
		mci_writel(host, FORCE_CLK_STOP, 0);
	}

	if (host->pdata->quirks & DW_MCI_QUIRK_BYPASS_SMU)
		drv_data->cfg_smu(host);

	mci_writel(host, FIFOTH, host->fifoth_val);

	/* Put in max timeout */
	mci_writel(host, TMOUT, 0xFFFFFFFF);

	mci_writel(host, RINTSTS, 0xFFFFFFFF);
	if (host->pdata->cd_type == DW_MCI_CD_INTERNAL)
		mci_writel(host, INTMASK, SDMMC_INT_CMD_DONE | SDMMC_INT_DATA_OVER |
				SDMMC_INT_TXDR | SDMMC_INT_RXDR |
				DW_MCI_ERROR_FLAGS | SDMMC_INT_CD);
	else
		mci_writel(host, INTMASK, SDMMC_INT_CMD_DONE | SDMMC_INT_DATA_OVER |
				SDMMC_INT_TXDR | SDMMC_INT_RXDR |
				DW_MCI_ERROR_FLAGS);

	mci_writel(host, CTRL, SDMMC_CTRL_INT_ENABLE);

	for (i = 0; i < host->num_slots; i++) {
		struct dw_mci_slot *slot = host->slot[i];
		if (!slot)
			continue;
		if (slot->mmc->pm_flags & MMC_PM_KEEP_POWER
				|| slot->mmc->pm_caps & MMC_PM_SKIP_MMC_RESUME_INIT) {
			dw_mci_set_ios(slot->mmc, &slot->mmc->ios);
			dw_mci_setup_bus(slot, true);
		}
	}
	return 0;
}
EXPORT_SYMBOL(dw_mci_resume);
#endif /* CONFIG_PM_SLEEP */

static int __init dw_mci_init(void)
{
	pr_info("Synopsys Designware Multimedia Card Interface Driver\n");
	return 0;
}

static void __exit dw_mci_exit(void)
{
}

module_init(dw_mci_init);
module_exit(dw_mci_exit);

MODULE_DESCRIPTION("DW Multimedia Card Interface driver");
MODULE_AUTHOR("NXP Semiconductor VietNam");
MODULE_AUTHOR("Imagination Technologies Ltd");
MODULE_LICENSE("GPL v2");
