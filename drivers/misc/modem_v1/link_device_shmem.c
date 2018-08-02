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

#include <linux/irq.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/suspend.h>
#include <linux/pm_qos.h>
#if defined(CONFIG_SOC_EXYNOS3475)
#include <mach/smc.h>
#else
#include <linux/smc.h>
#endif

#include <linux/shm_ipc.h>
#include <linux/mcu_ipc.h>
#include <linux/exynos-pci-ctrl.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"
#include "link_ctrlmsg_iosm.h"
#include "pmu-cp.h"

#ifdef GROUP_MEM_LINK_COMMAND

static inline void reset_ipc_map(struct mem_link_device *mld)
{
	int i;

	for (i = 0; i < MAX_SIPC_MAP; i++) {
		struct mem_ipc_device *dev = mld->dev[i];

		set_txq_head(dev, 0);
		set_txq_tail(dev, 0);
		set_rxq_head(dev, 0);
		set_rxq_tail(dev, 0);
	}
}

static int shmem_reset_ipc_link(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	unsigned int magic;
	unsigned int access;
	int i;

	set_access(mld, 0);
	set_magic(mld, 0);

	reset_ipc_map(mld);

	for (i = 0; i < MAX_SIPC_MAP; i++) {
		struct mem_ipc_device *dev = mld->dev[i];

		skb_queue_purge(dev->skb_txq);
		atomic_set(&dev->txq.busy, 0);
		dev->req_ack_cnt[TX] = 0;

		skb_queue_purge(dev->skb_rxq);
		atomic_set(&dev->rxq.busy, 0);
		dev->req_ack_cnt[RX] = 0;
	}

	atomic_set(&ld->netif_stopped, 0);

	set_magic(mld, MEM_IPC_MAGIC);
	set_access(mld, 1);

	magic = get_magic(mld);
	access = get_access(mld);
	if (magic != MEM_IPC_MAGIC || access != 1)
		return -EACCES;

	return 0;
}

#endif

#ifdef GROUP_MEM_LINK_DEVICE

static inline bool ipc_active(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (unlikely(!cp_online(mc))) {
		mif_err("%s<->%s: %s.state %s != ONLINE <%pf>\n",
			ld->name, mc->name, mc->name, mc_state(mc), CALLER);
		return false;
	}

	if (mld->dpram_magic) {
		unsigned int magic = get_magic(mld);
		unsigned int access = get_access(mld);
		if (magic != MEM_IPC_MAGIC || access != 1) {
			mif_err("%s<->%s: ERR! magic:0x%X access:%d <%pf>\n",
				ld->name, mc->name, magic, access, CALLER);
			return false;
		}
	}

	if (atomic_read(&mld->forced_cp_crash)) {
		mif_err("%s<->%s: ERR! forced_cp_crash:%d <%pf>\n",
			ld->name, mc->name, atomic_read(&mld->forced_cp_crash),
			CALLER);
		return false;
	}

	return true;
}

static inline void purge_txq(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	int i;

	/* Purge the skb_txq in every rb */
	if (ld->sbd_ipc) {
		struct sbd_link_device *sl = &mld->sbd_link_dev;

		for (i = 0; i < sl->num_channels; i++) {
			struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, TX);
			skb_queue_purge(&rb->skb_q);
		}
	}

	/* Purge the skb_txq in every IPC device (IPC_MAP_FMT, IPC_MAP_HPRIO_RAW, etc.) */
	for (i = 0; i < MAX_SIPC_MAP; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		skb_queue_purge(dev->skb_txq);
	}
}

#endif

#ifdef GROUP_MEM_CP_CRASH

static void set_modem_state(struct mem_link_device *mld, enum modem_state state)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;
	struct io_device *iod;

	spin_lock_irqsave(&mc->lock, flags);
	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, state);
	}
	spin_unlock_irqrestore(&mc->lock, flags);
}

static void shmem_handle_cp_crash(struct mem_link_device *mld,
		enum modem_state state)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (mld->stop_pm)
		mld->stop_pm(mld);
#endif

	/* Disable normal IPC */
	set_magic(mld, MEM_CRASH_MAGIC);
	set_access(mld, 0);

	stop_net_ifaces(ld);
	purge_txq(mld);

	if (cp_online(mc) || cp_booting(mc))
		set_modem_state(mld, state);

	atomic_set(&mld->forced_cp_crash, 0);
}

static void handle_no_cp_crash_ack(unsigned long arg)
{
	struct mem_link_device *mld = (struct mem_link_device *)arg;
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (cp_crashed(mc)) {
		mif_debug("%s: STATE_CRASH_EXIT without CRASH_ACK\n",
			ld->name);
	} else {
		mif_err("%s: ERR! No CRASH_ACK from CP\n", ld->name);
		shmem_handle_cp_crash(mld, STATE_CRASH_EXIT);
	}
}

static void shmem_forced_cp_crash(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	/* Disable normal IPC */
	set_magic(mld, MEM_CRASH_MAGIC);
	set_access(mld, 0);

	if (atomic_inc_return(&mld->forced_cp_crash) > 1) {
		mif_err("%s: ALREADY in progress <%pf>\n",
			ld->name, CALLER);
		return;
	}

	if (!cp_online(mc) && !cp_booting(mc)) {
		mif_err("%s: %s.state %s != ONLINE <%pf>\n",
			ld->name, mc->name, mc_state(mc), CALLER);
		return;
	}

	/* Disable debug Snapshot */
	mif_set_snapshot(false);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_DUMP)) {
		stop_net_ifaces(ld);

		if (mld->debug_info)
			mld->debug_info();

		/**
		 * If there is no CRASH_ACK from CP in a timeout,
		 * handle_no_cp_crash_ack() will be executed.
		 */
		mif_add_timer(&mld->crash_ack_timer, FORCE_CRASH_ACK_TIMEOUT,
			      handle_no_cp_crash_ack, (unsigned long)mld);

		/* Send CRASH_EXIT command to a CP */
		send_ipc_irq(mld, cmd2int(CMD_CRASH_EXIT));
	} else {
		shmem_forced_cp_crash(mld);
	}

	mif_err("%s->%s: CP_CRASH_REQ <%pf>\n", ld->name, mc->name, CALLER);
}

#endif

static bool rild_ready(struct link_device *ld)
{
	struct io_device *fmt_iod;
	struct io_device *rfs_iod;
	int fmt_opened;
	int rfs_opened;

	fmt_iod = link_get_iod_with_channel(ld, SIPC5_CH_ID_FMT_0);
	if (!fmt_iod) {
		mif_err("%s: No FMT io_device\n", ld->name);
		return false;
	}

	rfs_iod = link_get_iod_with_channel(ld, SIPC5_CH_ID_RFS_0);
	if (!rfs_iod) {
		mif_err("%s: No RFS io_device\n", ld->name);
		return false;
	}

	fmt_opened = atomic_read(&fmt_iod->opened);
	rfs_opened = atomic_read(&rfs_iod->opened);
	mif_err("%s: %s.opened=%d, %s.opened=%d\n", ld->name,
		fmt_iod->name, fmt_opened, rfs_iod->name, rfs_opened);
	if (fmt_opened > 0 && rfs_opened > 0)
		return true;

	return false;
}

static void cmd_init_start_handler(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	int err;

	mif_err("%s: INIT_START <- %s (%s.state:%s cp_boot_done:%d)\n",
		ld->name, mc->name, mc->name, mc_state(mc),
		atomic_read(&mld->cp_boot_done));

	if (!ld->sbd_ipc) {
		mif_err("%s: LINK_ATTR_SBD_IPC is NOT set\n", ld->name);
		return;
	}

	err = init_sbd_link(&mld->sbd_link_dev);
	if (err < 0) {
		mif_err("%s: init_sbd_link fail(%d)\n", ld->name, err);
		return;
	}

	if (mld->attrs & LINK_ATTR(LINK_ATTR_IPC_ALIGNED))
		ld->aligned = true;
	else
		ld->aligned = false;

	sbd_activate(&mld->sbd_link_dev);
	send_ipc_irq(mld, cmd2int(CMD_PIF_INIT_DONE));

	mif_err("%s: PIF_INIT_DONE -> %s\n", ld->name, mc->name);
}

static void cmd_phone_start_handler(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;
	int err;

	mif_err("%s: CP_START <- %s (%s.state:%s cp_boot_done:%d)\n",
		ld->name, mc->name, mc->name, mc_state(mc),
		atomic_read(&mld->cp_boot_done));

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (mld->start_pm)
		mld->start_pm(mld);
#endif

	spin_lock_irqsave(&mld->state_lock, flags);

	if (mld->state == LINK_STATE_IPC) {
		/*
		If there is no INIT_END command from AP, CP sends a CP_START
		command to AP periodically until it receives INIT_END from AP
		even though it has already been in ONLINE state.
		*/
		if (rild_ready(ld)) {
			mif_err("%s: INIT_END -> %s\n", ld->name, mc->name);
			send_ipc_irq(mld, cmd2int(CMD_INIT_END));
		}
		goto exit;
	}

	err = shmem_reset_ipc_link(mld);
	if (err) {
		mif_err("%s: shmem_reset_ipc_link fail(%d)\n", ld->name, err);
		goto exit;
	}

	if (rild_ready(ld)) {
		mif_err("%s: INIT_END -> %s\n", ld->name, mc->name);
		send_ipc_irq(mld, cmd2int(CMD_INIT_END));
		atomic_set(&mld->cp_boot_done, 1);
	}

	mld->state = LINK_STATE_IPC;
	complete_all(&mc->init_cmpl);

exit:
	spin_unlock_irqrestore(&mld->state_lock, flags);
}

static void cmd_crash_reset_handler(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;

	spin_lock_irqsave(&mld->state_lock, flags);
	mld->state = LINK_STATE_OFFLINE;
	spin_unlock_irqrestore(&mld->state_lock, flags);

	mif_err("%s<-%s: ERR! CP_CRASH_RESET\n", ld->name, mc->name);

	shmem_handle_cp_crash(mld, STATE_CRASH_RESET);
}

static void cmd_crash_exit_handler(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;

	/* Disable debug Snapshot */
	mif_set_snapshot(false);

	spin_lock_irqsave(&mld->state_lock, flags);
	mld->state = LINK_STATE_CP_CRASH;
	spin_unlock_irqrestore(&mld->state_lock, flags);

	if (timer_pending(&mld->crash_ack_timer))
		del_timer(&mld->crash_ack_timer);

	if (atomic_read(&mld->forced_cp_crash))
		mif_err("%s<-%s: CP_CRASH_ACK\n", ld->name, mc->name);
	else
		mif_err("%s<-%s: ERR! CP_CRASH_EXIT\n", ld->name, mc->name);

	shmem_handle_cp_crash(mld, STATE_CRASH_EXIT);
}

static void shmem_cmd_handler(struct mem_link_device *mld, u16 cmd)
{
	struct link_device *ld = &mld->link_dev;

	switch (cmd) {
	case CMD_INIT_START:
		cmd_init_start_handler(mld);
		break;

	case CMD_PHONE_START:
		cmd_phone_start_handler(mld);
		break;

	case CMD_CRASH_RESET:
		cmd_crash_reset_handler(mld);
		break;

	case CMD_CRASH_EXIT:
		cmd_crash_exit_handler(mld);
		break;

	default:
		mif_err("%s: Unknown command 0x%04X\n", ld->name, cmd);
		break;
	}
}

#ifdef GROUP_MEM_IPC_TX

static inline int check_txq_space(struct mem_link_device *mld,
				  struct mem_ipc_device *dev,
				  unsigned int qsize, unsigned int in,
				  unsigned int out, unsigned int count)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned int space;

	if (!circ_valid(qsize, in, out)) {
		mif_err("%s: ERR! Invalid %s_TXQ{qsize:%d in:%d out:%d}\n",
			ld->name, dev->name, qsize, in, out);
		return -EIO;
	}

	space = circ_get_space(qsize, in, out);
	if (unlikely(space < count)) {
		if (cp_online(mc)) {
			mif_err("%s: NOSPC %s_TX{qsize:%d in:%d out:%d space:%d len:%d}\n",
				ld->name, dev->name, qsize,
				in, out, space, count);
		}
		return -ENOSPC;
	}

	return space;
}

static int txq_write(struct mem_link_device *mld, struct mem_ipc_device *dev,
		     struct sk_buff *skb)
{
	char *src = skb->data;
	unsigned int count = skb->len;
	char *dst = get_txq_buff(dev);
	unsigned int qsize = get_txq_buff_size(dev);
	unsigned int in = get_txq_head(dev);
	unsigned int out = get_txq_tail(dev);
	int space;

	space = check_txq_space(mld, dev, qsize, in, out, count);
	if (unlikely(space < 0))
		return space;

	barrier();

	circ_write(dst, src, qsize, in, count);

	barrier();

	set_txq_head(dev, circ_new_ptr(qsize, in, count));

	/* Commit the item before incrementing the head */
	smp_mb();

	return count;
}

static int tx_frames_to_dev(struct mem_link_device *mld,
			    struct mem_ipc_device *dev)
{
	struct sk_buff_head *skb_txq = dev->skb_txq;
	int tx_bytes = 0;
	int ret = 0;

	while (1) {
		struct sk_buff *skb;

		skb = skb_dequeue(skb_txq);
		if (unlikely(!skb))
			break;

		ret = txq_write(mld, dev, skb);
		if (unlikely(ret < 0)) {
			/* Take the skb back to the skb_txq */
			skb_queue_head(skb_txq, skb);
			break;
		}
		pktlog_tx_bottom_skb(mld, skb);

		tx_bytes += ret;

#ifdef DEBUG_MODEM_IF_LINK_TX
		mif_pkt(skbpriv(skb)->sipc_ch, "LNK-TX", skb);
#endif

		dev_kfree_skb_any(skb);
	}

	return (ret < 0) ? ret : tx_bytes;
}

static enum hrtimer_restart tx_timer_func(struct hrtimer *timer)
{
	struct mem_link_device *mld;
	struct link_device *ld;
	struct modem_ctl *mc;
	int i;
	bool need_schedule;
	u16 mask;
	unsigned long flags;

	mld = container_of(timer, struct mem_link_device, tx_timer);
	ld = &mld->link_dev;
	mc = ld->mc;

	need_schedule = false;
	mask = 0;

	spin_lock_irqsave(&mc->lock, flags);

	if (unlikely(!ipc_active(mld)))
		goto exit;

#ifdef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
	if (mld->link_active) {
		if (!mld->link_active(mld)) {
			need_schedule = true;
			goto exit;
		}
	}
#endif

	for (i = 0; i < MAX_SIPC_MAP; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		int ret;

		if (unlikely(under_tx_flow_ctrl(mld, dev))) {
			ret = check_tx_flow_ctrl(mld, dev);
			if (ret < 0) {
				if (ret == -EBUSY || ret == -ETIME) {
					need_schedule = true;
					continue;
				} else {
					shmem_forced_cp_crash(mld);
					need_schedule = false;
					goto exit;
				}
			}
		}

		ret = tx_frames_to_dev(mld, dev);
		if (unlikely(ret < 0)) {
			if (ret == -EBUSY || ret == -ENOSPC) {
				need_schedule = true;
				txq_stop(mld, dev);
				/* If txq has 2 or more packet and 2nd packet
				  has -ENOSPC return, It request irq to consume
				  the TX ring-buffer from CP */
				mask |= msg_mask(dev);
				continue;
			} else {
				shmem_forced_cp_crash(mld);
				need_schedule = false;
				goto exit;
			}
		}

		if (ret > 0)
			mask |= msg_mask(dev);

		if (!skb_queue_empty(dev->skb_txq))
			need_schedule = true;
	}

	if (!need_schedule) {
		for (i = 0; i < MAX_SIPC_MAP; i++) {
			if (!txq_empty(mld->dev[i])) {
				need_schedule = true;
				break;
			}
		}
	}

	if (mask)
		send_ipc_irq(mld, mask2int(mask));

exit:
	if (need_schedule) {
		ktime_t ktime = ktime_set(0, ms2ns(TX_PERIOD_MS));
		hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
	}

	spin_unlock_irqrestore(&mc->lock, flags);

	return HRTIMER_NORESTART;
}

static inline void start_tx_timer(struct mem_link_device *mld,
				  struct hrtimer *timer)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;

	spin_lock_irqsave(&mc->lock, flags);

	if (unlikely(cp_offline(mc)))
		goto exit;

	if (!hrtimer_is_queued(timer)) {
		ktime_t ktime = ktime_set(0, ms2ns(TX_PERIOD_MS));
		hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
	}

exit:
	spin_unlock_irqrestore(&mc->lock, flags);
}

static inline void cancel_tx_timer(struct mem_link_device *mld,
				   struct hrtimer *timer)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;

	spin_lock_irqsave(&mc->lock, flags);

	if (hrtimer_active(timer))
		hrtimer_cancel(timer);

	spin_unlock_irqrestore(&mc->lock, flags);
}

static int tx_frames_to_rb(struct sbd_ring_buffer *rb)
{
	struct sk_buff_head *skb_txq = &rb->skb_q;
	int tx_bytes = 0;
	int ret = 0;

	while (1) {
		struct sk_buff *skb;

		skb = skb_dequeue(skb_txq);
		if (unlikely(!skb))
			break;

		ret = sbd_pio_tx(rb, skb);
		if (unlikely(ret < 0)) {
			/* Take the skb back to the skb_txq */
			skb_queue_head(skb_txq, skb);
			break;
		}

		tx_bytes += ret;
#ifdef DEBUG_MODEM_IF_LINK_TX
		mif_pkt(rb->ch, "LNK-TX", skb);
#endif
		dev_kfree_skb_any(skb);
	}

	return (ret < 0) ? ret : tx_bytes;
}

static enum hrtimer_restart sbd_tx_timer_func(struct hrtimer *timer)
{
	struct mem_link_device *mld =
		container_of(timer, struct mem_link_device, sbd_tx_timer);
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	struct sbd_link_device *sl = &mld->sbd_link_dev;
	int i;
	bool need_schedule = false;
	u16 mask = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&mc->lock, flags);
	if (unlikely(!ipc_active(mld))) {
		spin_unlock_irqrestore(&mc->lock, flags);
		goto exit;
	}
	spin_unlock_irqrestore(&mc->lock, flags);

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (mld->link_active) {
		if (!mld->link_active(mld)) {
			need_schedule = true;
			goto exit;
		}
	}
#endif

	for (i = 0; i < sl->num_channels; i++) {
		struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, TX);
		int ret;

		ret = tx_frames_to_rb(rb);

		if (unlikely(ret < 0)) {
			if (ret == -EBUSY || ret == -ENOSPC) {
				need_schedule = true;
				mask = MASK_SEND_DATA;
				continue;
			} else {
				shmem_forced_cp_crash(mld);
				need_schedule = false;
				goto exit;
			}
		}

		if (ret > 0)
			mask = MASK_SEND_DATA;

		if (!skb_queue_empty(&rb->skb_q))
			need_schedule = true;
	}

	if (!need_schedule) {
		for (i = 0; i < sl->num_channels; i++) {
			struct sbd_ring_buffer *rb;

			rb = sbd_id2rb(sl, i, TX);
			if (!rb_empty(rb)) {
				need_schedule = true;
				break;
			}
		}
	}

	if (mask) {
		spin_lock_irqsave(&mc->lock, flags);
		if (unlikely(!ipc_active(mld))) {
			spin_unlock_irqrestore(&mc->lock, flags);
			need_schedule = false;
			goto exit;
		}
		send_ipc_irq(mld, mask2int(mask));
		spin_unlock_irqrestore(&mc->lock, flags);
	}

exit:
	if (need_schedule) {
		ktime_t ktime = ktime_set(0, ms2ns(TX_PERIOD_MS));
		hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
	}

	return HRTIMER_NORESTART;
}

static int xmit_ipc_to_rb(struct mem_link_device *mld, enum sipc_ch_id ch,
			  struct sk_buff *skb)
{
	int ret;
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod = skbpriv(skb)->iod;
	struct modem_ctl *mc = ld->mc;
	struct sbd_ring_buffer *rb = sbd_ch2rb_with_skb(&mld->sbd_link_dev, ch, TX, skb);
	struct sk_buff_head *skb_txq;
	unsigned long flags;

	if (!rb) {
		mif_err("%s: %s->%s: ERR! NO SBD RB {ch:%d}\n",
			ld->name, iod->name, mc->name, ch);
		return -ENODEV;
}

	skb_txq = &rb->skb_q;

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->forbid_cp_sleep)
		mld->forbid_cp_sleep(mld);
#endif

	spin_lock_irqsave(&rb->lock, flags);

	if (unlikely(skb_txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		mif_err_limited("%s: %s->%s: ERR! {ch:%d} "
				"skb_txq.len %d >= limit %d\n",
				ld->name, iod->name, mc->name, ch,
				skb_txq->qlen, MAX_SKB_TXQ_DEPTH);
		ret = -EBUSY;
	} else {
		skb->len = min_t(int, skb->len, rb->buff_size);

		ret = skb->len;
		skb_queue_tail(skb_txq, skb);
		start_tx_timer(mld, &mld->sbd_tx_timer);
	}

	spin_unlock_irqrestore(&rb->lock, flags);

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->permit_cp_sleep)
		mld->permit_cp_sleep(mld);
#endif

	return ret;
}
#endif

static int xmit_ipc_to_dev(struct mem_link_device *mld, enum sipc_ch_id ch,
			   struct sk_buff *skb)
{
	int ret;
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod = skbpriv(skb)->iod;
	struct modem_ctl *mc = ld->mc;
	struct mem_ipc_device *dev = mld->dev[get_mmap_idx(ch, skb)];
	struct sk_buff_head *skb_txq;

	if (!dev) {
		mif_err("%s: %s->%s: ERR! NO IPC DEV {ch:%d}\n",
			ld->name, iod->name, mc->name, ch);
		return -ENODEV;
	}

	skb_txq = dev->skb_txq;

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->forbid_cp_sleep)
		mld->forbid_cp_sleep(mld);
#endif
	if (unlikely(skb_txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		mif_err_limited("%s: %s->%s: ERR! %s TXQ.qlen %d >= limit %d\n",
				ld->name, iod->name, mc->name, dev->name,
				skb_txq->qlen, MAX_SKB_TXQ_DEPTH);
		ret = -EBUSY;
	} else {
		ret = skb->len;
		skb_queue_tail(dev->skb_txq, skb);
		start_tx_timer(mld, &mld->tx_timer);
	}

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->permit_cp_sleep)
		mld->permit_cp_sleep(mld);
#endif

	return ret;
}

static int xmit_ipc(struct mem_link_device *mld, struct io_device *iod,
		    enum sipc_ch_id ch, struct sk_buff *skb)
{
	struct link_device *ld = &mld->link_dev;

	if (unlikely(!ipc_active(mld)))
		return -EIO;

	if (ld->sbd_ipc && iod->sbd_ipc) {
		if (likely(sbd_active(&mld->sbd_link_dev)))
			return xmit_ipc_to_rb(mld, ch, skb);
		else
			return -ENODEV;
	} else {
		return xmit_ipc_to_dev(mld, ch, skb);
	}
}

static inline int check_udl_space(struct mem_link_device *mld,
				  struct mem_ipc_device *dev,
				  unsigned int qsize, unsigned int in,
				  unsigned int out, unsigned int count)
{
	struct link_device *ld = &mld->link_dev;
	unsigned int space;

	if (!circ_valid(qsize, in, out)) {
		mif_err("%s: ERR! Invalid %s_TXQ{qsize:%d in:%d out:%d}\n",
			ld->name, dev->name, qsize, in, out);
		return -EIO;
	}

	space = circ_get_space(qsize, in, out);
	if (unlikely(space < count)) {
		mif_err("%s: NOSPC %s_TX{qsize:%d in:%d out:%d free:%d len:%d}\n",
			ld->name, dev->name, qsize, in, out, space, count);
		return -ENOSPC;
	}

	return 0;
}

static inline int udl_write(struct mem_link_device *mld,
			    struct mem_ipc_device *dev, struct sk_buff *skb)
{
	unsigned int count = skb->len;
	char *src = skb->data;
	char *dst = get_txq_buff(dev);
	unsigned int qsize = get_txq_buff_size(dev);
	unsigned int in = get_txq_head(dev);
	unsigned int out = get_txq_tail(dev);
	int space;

	space = check_udl_space(mld, dev, qsize, in, out, count);
	if (unlikely(space < 0))
		return space;

	barrier();

	circ_write(dst, src, qsize, in, count);

	barrier();

	set_txq_head(dev, circ_new_ptr(qsize, in, count));

	/* Commit the item before incrementing the head */
	smp_mb();

	return count;
}

static int xmit_udl(struct mem_link_device *mld, struct io_device *iod,
		    enum sipc_ch_id ch, struct sk_buff *skb)
{
	int ret;
	struct mem_ipc_device *dev = mld->dev[IPC_MAP_NORM_RAW];
	int count = skb->len;
	int tried = 0;

	while (1) {
		ret = udl_write(mld, dev, skb);
		if (ret == count)
			break;

		if (ret != -ENOSPC)
			goto exit;

		tried++;
		if (tried >= 20)
			goto exit;

		if (in_interrupt())
			mdelay(50);
		else
			msleep(50);
	}

#ifdef DEBUG_MODEM_IF_LINK_TX
	mif_pkt(ch, "LNK-TX", skb);
#endif

	dev_kfree_skb_any(skb);

exit:
	return ret;
}

/*============================================================================*/

#ifdef GROUP_MEM_IPC_RX

static void pass_skb_to_demux(struct mem_link_device *mld, struct sk_buff *skb)
{
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod = skbpriv(skb)->iod;
	int ret;
	u8 ch = skbpriv(skb)->sipc_ch;

	if (unlikely(!iod)) {
		mif_err("%s: ERR! No IOD for CH.%d\n", ld->name, ch);
		dev_kfree_skb_any(skb);
		shmem_forced_cp_crash(mld);
		return;
	}

#ifdef DEBUG_MODEM_IF_LINK_RX
	mif_pkt(ch, "LNK-RX", skb);
#endif

	ret = iod->recv_skb_single(iod, ld, skb);
	if (unlikely(ret < 0)) {
		struct modem_ctl *mc = ld->mc;
		mif_err_limited("%s: %s<-%s: ERR! %s->recv_skb fail (%d)\n",
				ld->name, iod->name, mc->name, iod->name, ret);
		dev_kfree_skb_any(skb);
	}
}

static inline void link_to_demux(struct mem_link_device  *mld)
{
	int i;

	for (i = 0; i < MAX_SIPC_MAP; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		struct sk_buff_head *skb_rxq = dev->skb_rxq;

		while (1) {
			struct sk_buff *skb;

			skb = skb_dequeue(skb_rxq);
			if (!skb)
				break;

			pass_skb_to_demux(mld, skb);
		}
	}
}

static void link_to_demux_work(struct work_struct *ws)
{
	struct link_device *ld;
	struct mem_link_device *mld;

	ld = container_of(ws, struct link_device, rx_delayed_work.work);
	mld = to_mem_link_device(ld);

	link_to_demux(mld);
}

static inline void schedule_link_to_demux(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct delayed_work *dwork = &ld->rx_delayed_work;

	/*queue_delayed_work(ld->rx_wq, dwork, 0);*/
	queue_work_on(7, ld->rx_wq, &dwork->work);
}

static struct sk_buff *rxq_read(struct mem_link_device *mld,
				struct mem_ipc_device *dev,
				unsigned int in)
{
	struct link_device *ld = &mld->link_dev;
	struct sk_buff *skb;
	char *src = get_rxq_buff(dev);
	unsigned int qsize = get_rxq_buff_size(dev);
	unsigned int out = get_rxq_tail(dev);
	unsigned int rest = circ_get_usage(qsize, in, out);
	unsigned int len;
	char hdr[SIPC5_MIN_HEADER_SIZE];

	/* Copy the header in a frame to the header buffer */
	circ_read(hdr, src, qsize, out, SIPC5_MIN_HEADER_SIZE);

	/* Check the config field in the header */
	if (unlikely(!sipc5_start_valid(hdr))) {
		mif_err("%s: ERR! %s BAD CFG 0x%02X (in:%d out:%d rest:%d)\n",
			ld->name, dev->name, hdr[SIPC5_CONFIG_OFFSET],
			in, out, rest);
		goto bad_msg;
	}

	/* Verify the length of the frame (data + padding) */
	len = sipc5_get_total_len(hdr);
	if (unlikely(len > rest)) {
		mif_err("%s: ERR! %s BAD LEN %d > rest %d\n",
			ld->name, dev->name, len, rest);
		goto bad_msg;
	}

	/* Allocate an skb */
	skb = mem_alloc_skb(len);
	if (!skb) {
		mif_err("%s: ERR! %s mem_alloc_skb(%d) fail\n",
			ld->name, dev->name, len);
		goto no_mem;
	}

	/* Read the frame from the RXQ */
	circ_read(skb_put(skb, len), src, qsize, out, len);

	/* Update tail (out) pointer to the frame to be read in the future */
	set_rxq_tail(dev, circ_new_ptr(qsize, out, len));

	/* Finish reading data before incrementing tail */
	smp_mb();

#ifdef DEBUG_MODEM_IF
	/* Record the time-stamp */
	getnstimeofday(&skbpriv(skb)->ts);
#endif

	return skb;

bad_msg:
	mif_err("%s%s%s: ERR! BAD MSG: %02x %02x %02x %02x\n",
		ld->name, arrow(RX), ld->mc->name,
		hdr[0], hdr[1], hdr[2], hdr[3]);
	set_rxq_tail(dev, in);	/* Reset tail (out) pointer */
	shmem_forced_cp_crash(mld);

no_mem:
	return NULL;
}

static int rx_frames_from_dev(struct mem_link_device *mld,
			      struct mem_ipc_device *dev)
{
	struct link_device *ld = &mld->link_dev;
	unsigned int qsize = get_rxq_buff_size(dev);
	unsigned int in = get_rxq_head(dev);
	unsigned int out = get_rxq_tail(dev);
	unsigned int size = circ_get_usage(qsize, in, out);
	int rcvd = 0;

	if (unlikely(circ_empty(in, out)))
		return 0;

	while (rcvd < size) {
		struct sk_buff *skb;
		u8 ch;
		struct io_device *iod;

		skb = rxq_read(mld, dev, in);
		if (!skb)
			return -ENOMEM;

		pktlog_rx_bottom_skb(mld, skb);

		ch = sipc5_get_ch(skb->data);
		iod = link_get_iod_with_channel(ld, ch);
		if (!iod) {
			mif_err("%s: ERR! [%s]No IOD for CH.%d(out:%u)\n",
				ld->name, dev->name, ch, get_rxq_tail(dev));
			pr_skb("CRASH", skb);
			dev_kfree_skb_any(skb);
			shmem_forced_cp_crash(mld);
			break;
		}

		/* Record the IO device and the link device into the &skb->cb */
		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;

		skbpriv(skb)->lnk_hdr = iod->link_header;
		skbpriv(skb)->sipc_ch = ch;

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_demux(). */
		rcvd += skb->len;
		pass_skb_to_demux(mld, skb);
	}

	if (rcvd < size) {
		struct link_device *ld = &mld->link_dev;
		mif_err("%s: WARN! rcvd %d < size %d\n", ld->name, rcvd, size);
	}

	return rcvd;
}

static int recv_ipc_frames(struct mem_link_device *mld,
			    struct mem_snapshot *mst)
{
	int i;
	u16 intr = mst->int2ap;

	for (i = 0; i < MAX_SIPC_MAP; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		int rcvd;

#if 0
		print_dev_snapshot(mld, mst, dev);
#endif

		if (req_ack_valid(dev, intr))
			recv_req_ack(mld, dev, mst);

		rcvd = rx_frames_from_dev(mld, dev);
		if (rcvd < 0)
			return rcvd;

		if (req_ack_valid(dev, intr))
			send_res_ack(mld, dev);

		if (res_ack_valid(dev, intr))
			recv_res_ack(mld, dev, mst);
	}
	return 0;
}

static void pass_skb_to_net(struct mem_link_device *mld, struct sk_buff *skb)
{
	struct link_device *ld = &mld->link_dev;
	struct skbuff_private *priv;
	struct io_device *iod;
	int ret;

	priv = skbpriv(skb);
	if (unlikely(!priv)) {
		mif_err("%s: ERR! No PRIV in skb@%p\n", ld->name, skb);
		dev_kfree_skb_any(skb);
		shmem_forced_cp_crash(mld);
		return;
	}

	iod = priv->iod;
	if (unlikely(!iod)) {
		mif_err("%s: ERR! No IOD in skb@%p\n", ld->name, skb);
		dev_kfree_skb_any(skb);
		shmem_forced_cp_crash(mld);
		return;
	}

#ifdef DEBUG_MODEM_IF_LINK_RX
	mif_pkt(iod->id, "LNK-RX", skb);
#endif

	ret = iod->recv_net_skb(iod, ld, skb);
	if (unlikely(ret < 0)) {
		struct modem_ctl *mc = ld->mc;
		mif_err_limited("%s: %s<-%s: ERR! %s->recv_net_skb fail (%d)\n",
				ld->name, iod->name, mc->name, iod->name, ret);
		dev_kfree_skb_any(skb);
	}
}

static int rx_net_frames_from_rb(struct sbd_ring_buffer *rb, int budget)
{
	int rcvd = 0;
	struct link_device *ld = rb->ld;
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	unsigned int num_frames;

#ifdef CONFIG_LINK_DEVICE_NAPI
	num_frames = min_t(unsigned int, rb_usage(rb), budget);
#else
	num_frames = rb_usage(rb);
#endif

	while (rcvd < num_frames) {
		struct sk_buff *skb;

		skb = sbd_pio_rx(rb);
		if (!skb)
			return -ENOMEM;

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_net(). */
		rcvd++;

		pass_skb_to_net(mld, skb);
	}

	if (rcvd < num_frames) {
		struct io_device *iod = rb->iod;
		struct link_device *ld = rb->ld;
		struct modem_ctl *mc = ld->mc;
		mif_err("%s: %s<-%s: WARN! rcvd %d < num_frames %d\n",
			ld->name, iod->name, mc->name, rcvd, num_frames);
	}

	return rcvd;
}

static int rx_ipc_frames_from_rb(struct sbd_ring_buffer *rb)
{
	int rcvd = 0;
	struct link_device *ld = rb->ld;
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	unsigned int qlen = rb->len;
	unsigned int in = *rb->wp;
	unsigned int out = *rb->rp;
	unsigned int num_frames = circ_get_usage(qlen, in, out);

	while (rcvd < num_frames) {
		struct sk_buff *skb;

		skb = sbd_pio_rx(rb);
		if (!skb)
			return -ENOMEM;

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_demux(). */
		rcvd++;

		if (skbpriv(skb)->lnk_hdr) {
			u8 ch = rb->ch;
			u8 fch = sipc5_get_ch(skb->data);
			if (fch != ch) {
				mif_err("frm.ch:%d != rb.ch:%d\n", fch, ch);
				pr_skb("CRASH", skb);
				dev_kfree_skb_any(skb);
				shmem_forced_cp_crash(mld);
				continue;
			}
		}

		pass_skb_to_demux(mld, skb);
	}

	if (rcvd < num_frames) {
		struct io_device *iod = rb->iod;
		struct modem_ctl *mc = ld->mc;
		mif_err("%s: %s<-%s: WARN! rcvd %d < num_frames %d\n",
			ld->name, iod->name, mc->name, rcvd, num_frames);
	}
	return rcvd;
}

int mem_netdev_poll(struct napi_struct *napi, int budget)
{
	int rcvd;
	struct vnet *vnet = netdev_priv(napi->dev);
	struct mem_link_device *mld =
		container_of(vnet->ld, struct mem_link_device, link_dev);
	struct sbd_ring_buffer *rb =
		sbd_ch2rb(&mld->sbd_link_dev, vnet->iod->id, RX);

	rcvd = rx_net_frames_from_rb(rb, budget);

	/* no more ring buffer to process */
	if (rcvd < budget) {
		napi_complete(napi);
		/* To do: enable mailbox irq */
	}

	mif_debug("%d pkts\n", rcvd);

	return rcvd;
}

static int recv_sbd_ipc_frames(struct mem_link_device *mld,
				struct mem_snapshot *mst)
{
	struct sbd_link_device *sl = &mld->sbd_link_dev;
	int i;

	for (i = 0; i < sl->num_channels; i++) {
		struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, RX);
		int rcvd = 0;

		if (unlikely(rb_empty(rb)))
			continue;

		if (likely(sipc_ps_ch(rb->ch))) {
#ifdef CONFIG_LINK_DEVICE_NAPI
			/* To do: disable mailbox irq */
			if (napi_schedule_prep(&rb->iod->napi))
				__napi_schedule(&rb->iod->napi);
#else
			rcvd = rx_net_frames_from_rb(rb, 0);
#endif
		} else {
			rcvd = rx_ipc_frames_from_rb(rb);
		}

		if (rcvd < 0)
			return rcvd;
	}
	return 0;
}

static void shmem_oom_handler_work(struct work_struct *ws)
{
	struct mem_link_device *mld =
		container_of(ws, struct mem_link_device, page_reclaim_work);
	struct sk_buff *skb;

	/* try to page reclaim with GFP_KERNEL */
	skb = alloc_skb(PAGE_SIZE - 512, GFP_KERNEL);
	if (skb)
		dev_kfree_skb_any(skb);

	/* need to disable the RX irq ?? */
	msleep(200);

	mif_info("trigger the rx task again\n");
	tasklet_schedule(&mld->rx_tsk);
}

static void ipc_rx_func(struct mem_link_device *mld)
{
	u32 qlen = mld->msb_rxq.qlen;

	while (qlen-- > 0) {
		struct mst_buff *msb;
		u16 intr;
		int ret = 0;

		msb = msb_dequeue(&mld->msb_rxq);
		if (!msb)
			break;

		intr = msb->snapshot.int2ap;

		if (cmd_valid(intr))
			mld->cmd_handler(mld, int2cmd(intr));

		if (sbd_active(&mld->sbd_link_dev))
			ret = recv_sbd_ipc_frames(mld, &msb->snapshot);
		else
			ret = recv_ipc_frames(mld, &msb->snapshot);

		if (ret == -ENOMEM) {
			msb_queue_head(&mld->msb_rxq, msb);
			if (!work_pending(&mld->page_reclaim_work)) {
				struct link_device *ld = &mld->link_dev;
				mif_err_limited("Rx ENOMEM, try reclaim work");
				queue_work(ld->rx_wq,
						&mld->page_reclaim_work);
			}
			return;
		}
		msb_free(msb);
	}

	if (mld->msb_rxq.qlen)
		tasklet_schedule(&mld->rx_tsk);
}

static void udl_rx_work(struct work_struct *ws)
{
	struct mem_link_device *mld;

	mld = container_of(ws, struct mem_link_device, udl_rx_dwork.work);

	ipc_rx_func(mld);
}

static void shmem_rx_task(unsigned long data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (likely(cp_online(mc)))
		ipc_rx_func(mld);
	else
		queue_delayed_work(ld->rx_wq, &mld->udl_rx_dwork, 0);
}

#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_METHOD

static int shmem_init_comm(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_ctl *mc = ld->mc;
	struct io_device *check_iod;
	int id = iod->id;
	int fmt2rfs = (SIPC5_CH_ID_RFS_0 - SIPC5_CH_ID_FMT_0);
	int rfs2fmt = (SIPC5_CH_ID_FMT_0 - SIPC5_CH_ID_RFS_0);

	if (atomic_read(&mld->cp_boot_done))
		return 0;

#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	if (mld->iosm) {
		struct sbd_link_device *sl = &mld->sbd_link_dev;
		struct sbd_ipc_device *sid = sbd_ch2dev(sl, iod->id);

		if (atomic_read(&sid->config_done)) {
			tx_iosm_message(mld, IOSM_A2C_OPEN_CH, (u32 *)&id);
			return 0;
		} else {
			mif_err("%s isn't configured channel\n", iod->name);
			return -ENODEV;
		}
	}
#endif

	switch (id) {
	case SIPC5_CH_ID_FMT_0 ... SIPC5_CH_ID_FMT_9:
		check_iod = link_get_iod_with_channel(ld, (id + fmt2rfs));
		if (check_iod ? atomic_read(&check_iod->opened) : true) {
			mif_err("%s: %s->INIT_END->%s\n",
				ld->name, iod->name, mc->name);
			send_ipc_irq(mld, cmd2int(CMD_INIT_END));
			atomic_set(&mld->cp_boot_done, 1);
		} else {
			mif_err("%s is not opened yet\n", check_iod->name);
		}
		break;

	case SIPC5_CH_ID_RFS_0 ... SIPC5_CH_ID_RFS_9:
		check_iod = link_get_iod_with_channel(ld, (id + rfs2fmt));
		if (check_iod) {
			if (atomic_read(&check_iod->opened)) {
				mif_err("%s: %s->INIT_END->%s\n",
					ld->name, iod->name, mc->name);
				send_ipc_irq(mld, cmd2int(CMD_INIT_END));
				atomic_set(&mld->cp_boot_done, 1);
			} else {
				mif_err("%s not opened yet\n", check_iod->name);
			}
		}
		break;

	default:
		break;
	}

	return 0;
}

static void shmem_terminate_comm(struct link_device *ld, struct io_device *iod)
{
#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	struct mem_link_device *mld = to_mem_link_device(ld);

	if (mld->iosm)
		tx_iosm_message(mld, IOSM_A2C_CLOSE_CH, (u32 *)&iod->id);
#endif
}

static int shmem_send(struct link_device *ld, struct io_device *iod,
		    struct sk_buff *skb)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_ctl *mc = ld->mc;
	enum dev_format id = iod->format;
	u8 ch = iod->id;

	switch (id) {
	case IPC_RAW:
		if (unlikely(atomic_read(&ld->netif_stopped) > 0)) {
			if (skb->queue_mapping != 1) {
				if (in_interrupt()) {
					mif_err("raw tx is suspended, drop size=%d\n",
							skb->len);
					return -EBUSY;
				}

				mif_err("wait TX RESUME CMD...\n");
				init_completion(&ld->raw_tx_resumed);
				wait_for_completion(&ld->raw_tx_resumed);
				mif_err("TX resumed done.\n");
			} else {
				mif_err_limited("Tx_flowctrl, but received ack from ch %d\n", ch);
			}
		}

	case IPC_RFS:
	case IPC_FMT:
		if (likely(sipc5_ipc_ch(ch)))
			return xmit_ipc(mld, iod, ch, skb);
		else
			return xmit_udl(mld, iod, ch, skb);

	case IPC_BOOT:
	case IPC_DUMP:
		if (sipc5_udl_ch(ch))
			return xmit_udl(mld, iod, ch, skb);
		break;

	default:
		break;
	}

	mif_err("%s:%s->%s: ERR! Invalid IO device (format:%s id:%d)\n",
		ld->name, iod->name, mc->name, dev_str(id), ch);

	return -ENODEV;
}

static void shmem_boot_on(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	unsigned long flags;

	atomic_set(&mld->cp_boot_done, 0);

	spin_lock_irqsave(&mld->state_lock, flags);
	mld->state = LINK_STATE_OFFLINE;
	spin_unlock_irqrestore(&mld->state_lock, flags);

	cancel_tx_timer(mld, &mld->tx_timer);

	if (ld->sbd_ipc) {
#ifdef CONFIG_LTE_MODEM_XMM7260
		sbd_deactivate(&mld->sbd_link_dev);
#endif
		cancel_tx_timer(mld, &mld->sbd_tx_timer);

		if (mld->iosm) {
			memset(mld->base + CMD_RGN_OFFSET, 0, CMD_RGN_SIZE);
			mif_info("Control message region has been initialized\n");
		}
	}

	purge_txq(mld);
}

static int shmem_xmit_boot(struct link_device *ld, struct io_device *iod,
		     unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	void __iomem *dst;
	void __user *src;
	int err;
	struct modem_firmware mf;
	void __iomem *v_base;
	size_t valid_space;

	/**
	 * Get the information about the boot image
	 */
	memset(&mf, 0, sizeof(struct modem_firmware));

	err = copy_from_user(&mf, (const void __user *)arg, sizeof(mf));
	if (err) {
		mif_err("%s: ERR! INFO copy_from_user fail\n", ld->name);
		return -EFAULT;
	}

	/* Calculate size of valid space which BL will download */
	valid_space = (mf.mode) ? mld->size : mld->boot_size;
	/* Calculate base address (0: BOOT_MODE, 1: DUMP_MODE) */
	v_base = (mf.mode) ? mld->base : mld->boot_base;

	/**
	 * Check the size of the boot image
	 * fix the integer overflow of "mf.m_offset + mf.len" from Jose Duart
	 */
	if (mf.size > valid_space || mf.len > valid_space
			|| mf.m_offset > valid_space - mf.len) {
		mif_err("%s: ERR! Invalid args: size %x, offset %x, len %x\n",
			ld->name, mf.size, mf.m_offset, mf.len);
		return -EINVAL;
	}

	dst = (void __iomem *)(v_base + mf.m_offset);
	src = (void __user *)((unsigned long)mf.binary);
	err = copy_from_user(dst, src, mf.len);
	if (err) {
		mif_err("%s: ERR! BOOT copy_from_user fail\n", ld->name);
		return err;
	}

	return 0;
}

static int shmem_security_request(struct link_device *ld, struct io_device *iod,
				unsigned long arg)
{
	unsigned long param2, param3;
	int err = 0;
	struct modem_sec_req msr;

	err = copy_from_user(&msr, (const void __user *)arg, sizeof(msr));
	if (err) {
		mif_err("%s: ERR! copy_from_user fail\n", ld->name);
		err = -EFAULT;
		goto exit;
	}

	param2 = shm_get_security_param2(msr.mode, msr.size_boot);
	param3 = shm_get_security_param3(msr.mode, msr.size_main);
	mif_err("mode=%u, size=%lu, addr=%lu\n", msr.mode, param2, param3);
	/* exynos_smc(SMC_ID_CLK, SSS_CLK_ENABLE, 0, 0); */
	err = exynos_smc(SMC_ID, msr.mode, param2, param3);
	/* exynos_smc(SMC_ID_CLK, SSS_CLK_DISABLE, 0, 0); */
	mif_info("%s: return_value=%d\n", ld->name, err);
exit:
	return err;
}

static int shmem_start_download(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	if (ld->sbd_ipc && mld->attrs & LINK_ATTR(LINK_ATTR_MEM_DUMP))
		sbd_deactivate(&mld->sbd_link_dev);

	reset_ipc_map(mld);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_BOOT_ALIGNED))
		ld->aligned = true;
	else
		ld->aligned = false;

	if (mld->dpram_magic) {
		unsigned int magic;

		set_magic(mld, MEM_BOOT_MAGIC);
		magic = get_magic(mld);
		if (magic != MEM_BOOT_MAGIC) {
			mif_err("%s: ERR! magic 0x%08X != BOOT_MAGIC 0x%08X\n",
				ld->name, magic, MEM_BOOT_MAGIC);
			return -EFAULT;
		}
		mif_err("%s: magic == 0x%08X\n", ld->name, magic);
	}

	return 0;
}

static int shmem_update_firm_info(struct link_device *ld, struct io_device *iod,
				unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	int ret;

	ret = copy_from_user(&mld->img_info, (void __user *)arg,
			     sizeof(struct std_dload_info));
	if (ret) {
		mif_err("ERR! copy_from_user fail!\n");
		return -EFAULT;
	}

	return 0;
}

static int shmem_force_dump(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	mif_err("+++\n");
	shmem_forced_cp_crash(mld);
	mif_err("---\n");
	return 0;
}

static int shmem_start_upload(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	if (ld->sbd_ipc && mld->attrs & LINK_ATTR(LINK_ATTR_MEM_DUMP))
		sbd_deactivate(&mld->sbd_link_dev);

	reset_ipc_map(mld);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_DUMP_ALIGNED))
		ld->aligned = true;
	else
		ld->aligned = false;

	if (mld->dpram_magic) {
		unsigned int magic;

		set_magic(mld, MEM_DUMP_MAGIC);
		magic = get_magic(mld);
		if (magic != MEM_DUMP_MAGIC) {
			mif_err("%s: ERR! magic 0x%08X != DUMP_MAGIC 0x%08X\n",
				ld->name, magic, MEM_DUMP_MAGIC);
			return -EFAULT;
		}
		mif_err("%s: magic == 0x%08X\n", ld->name, magic);
	}

	return 0;
}

static int shmem_full_dump(struct link_device *ld, struct io_device *iod,
		unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	unsigned long copied = 0;
	struct sk_buff *skb;
	unsigned long alloc_size = 0xE00;
	int ret;

	ret = copy_to_user((void __user *)arg, &mld->size, sizeof(mld->size));
	if (ret) {
		mif_err("ERR! copy_from_user fail!\n");
		return -EFAULT;
	}

	while (copied < mld->size) {
		if (mld->size - copied < alloc_size)
			alloc_size = mld->size - copied;

		skb = alloc_skb(alloc_size, GFP_ATOMIC);
		if (!skb) {
			skb_queue_purge(&iod->sk_rx_q);
			mif_err("ERR! alloc_skb fail, purged skb_rx_q\n");
			return -ENOMEM;
		}

		memcpy(skb_put(skb, alloc_size), mld->base + copied, alloc_size);
		copied += alloc_size;

		/* Record the IO device and the link device into the &skb->cb */
		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;

		skbpriv(skb)->lnk_hdr = false;
		skbpriv(skb)->sipc_ch = iod->id;

		ret = iod->recv_skb_single(iod, ld, skb);
		if (unlikely(ret < 0)) {
			struct modem_ctl *mc = ld->mc;
			mif_err_limited("%s: %s<-%s: %s->recv_skb fail (%d)\n",
				ld->name, iod->name, mc->name, iod->name, ret);
			dev_kfree_skb_any(skb);
			return ret;
		}
	}

	mif_info("Complete! (%lu bytes)\n", copied);
	return 0;
}

static void shmem_close_tx(struct link_device *ld)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	unsigned long flags;

	spin_lock_irqsave(&mld->state_lock, flags);
	mld->state = LINK_STATE_OFFLINE;
	spin_unlock_irqrestore(&mld->state_lock, flags);

	if (timer_pending(&mld->crash_ack_timer))
		del_timer(&mld->crash_ack_timer);

	stop_net_ifaces(ld);
	purge_txq(mld);
}

#endif

/*============================================================================*/


static u16 recv_cp2ap_irq(struct mem_link_device *mld)
{
	return (u16)mbox_get_value(mld->mbx_cp2ap_msg);
}

static u16 recv_cp2ap_status(struct mem_link_device *mld)
{
	return (u16)mbox_get_value(mld->mbx_cp2ap_status);
}

static void send_ap2cp_irq(struct mem_link_device *mld, u16 mask)
{
#if 0
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (!cp_online(mc))
		mif_err("%s: mask 0x%04X (%s.state == %s)\n", ld->name, mask,
			mc->name, mc_state(mc));
#endif
	mbox_set_value(mld->mbx_ap2cp_msg, mask);
	mbox_set_interrupt(mld->int_ap2cp_msg);
}

static inline u16 read_ap2cp_irq(struct mem_link_device *mld)
{
	return mbox_get_value(mld->mbx_ap2cp_msg);
}

#define SHMEM_SRINFO_OFFSET 0x800 /* 4KB - 2KB */
#define SHMEM_SRINFO_SBD_OFFSET 0xF800 /* 64KB - 2KB */
#define SHMEM_SRINFO_SIZE 0x800
#define SHMEM_SRINFO_DATA_STR 64

struct shmem_srinfo {
	unsigned int size;
	char buf[0];
};

static char *shmem_get_srinfo_address(struct link_device *ld)
{
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	unsigned offs = (mld->link_dev.sbd_ipc) ?
		SHMEM_SRINFO_SBD_OFFSET : SHMEM_SRINFO_OFFSET;
	char *base = mld->base + offs;

	return base;
}

/* not in use */
static int shmem_ioctl(struct link_device *ld, struct io_device *iod,
		       unsigned int cmd, unsigned long arg)
{
	mif_err("%s: cmd 0x%08X\n", ld->name, cmd);

	switch (cmd) {
	case IOCTL_MODEM_GET_SHMEM_SRINFO:
	{
		struct shmem_srinfo __user *sr_arg =
			(struct shmem_srinfo __user *)arg;
		unsigned count, size = SHMEM_SRINFO_SIZE;

		if (copy_from_user(&count, &sr_arg->size, sizeof(unsigned)))
			return -EFAULT;

		mif_info("get srinfo:%s, size = %d\n", iod->name, count);

		size = min(size, count);
		if (copy_to_user(&sr_arg->size, &size, sizeof(unsigned)))
			return -EFAULT;

		if (copy_to_user(sr_arg->buf, shmem_get_srinfo_address(ld),
			size))
			return -EFAULT;
		break;
	}

	case IOCTL_MODEM_SET_SHMEM_SRINFO:
	{
		struct shmem_srinfo __user *sr_arg =
			(struct shmem_srinfo __user *)arg;
		unsigned count, size = SHMEM_SRINFO_SIZE;

		if (copy_from_user(&count, &sr_arg->size, sizeof(unsigned)))
			return -EFAULT;

		mif_info("set srinfo:%s, size = %d\n", iod->name, count);

		if (copy_from_user(shmem_get_srinfo_address(ld), sr_arg->buf,
			min(count, size)))
			return -EFAULT;
		break;
	}

	default:
		mif_err("%s: ERR! invalid cmd 0x%08X\n", ld->name, cmd);
		return -EINVAL;
	}

	return 0;
}

static void shmem_tx_state_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	u16 int2ap_status;

	int2ap_status = mld->recv_cp2ap_status(mld);

	switch (int2ap_status & 0x0010) {
	case MASK_TX_FLOWCTL_SUSPEND:
		if (!chk_same_cmd(mld, int2ap_status))
			tx_flowctrl_suspend(mld);
		break;

	case MASK_TX_FLOWCTL_RESUME:
		if (!chk_same_cmd(mld, int2ap_status))
			tx_flowctrl_resume(mld);
		break;

	default:
		break;
	}

	if (unlikely(!rx_possible(mc))) {
		mif_err("%s: ERR! %s.state == %s\n", ld->name, mc->name,
			mc_state(mc));
		return;
	}
}

static void shmem_irq_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct mst_buff *msb;
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	msb = mem_take_snapshot(mld, RX);
	if (!msb)
		return;

	if (unlikely(!int_valid(msb->snapshot.int2ap))) {
		mif_err("%s: ERR! invalid intr 0x%X\n",
				ld->name, msb->snapshot.int2ap);
		msb_free(msb);
		return;
	}

	if (unlikely(!rx_possible(mc))) {
		mif_err("%s: ERR! %s.state == %s\n", ld->name, mc->name,
			mc_state(mc));
		msb_free(msb);
		return;
	}

	msb_queue_tail(&mld->msb_rxq, msb);
	tasklet_schedule(&mld->rx_tsk);
}

static struct pm_qos_request pm_qos_req_mif;
static struct pm_qos_request pm_qos_req_cpu;
static struct pm_qos_request pm_qos_req_int;

static void shmem_qos_work(struct work_struct *work)
{
	struct mem_link_device *mld =
		container_of(work, struct mem_link_device, pm_qos_work);
	unsigned int qos_val;
	unsigned int level;

	qos_val = mbox_get_value(mld->mbx_perf_req);
	mif_err("pm_qos:0x%x requested\n", qos_val);

	level = (qos_val & 0xff);
	if (level > 0 && level <= mld->ap_clk_cnt) {
		mif_err("Lock CPU(%u)\n", mld->ap_clk_table[level - 1]);
		pm_qos_update_request(&pm_qos_req_cpu,
				mld->ap_clk_table[level - 1]);
	} else {
		mif_err("Unlock CPU(%u)\n", level);
		pm_qos_update_request(&pm_qos_req_cpu, 0);
	}

	level = (qos_val >> 8) & 0xff;
	if (level > 0 && level <= mld->mif_clk_cnt) {
		mif_err("Lock MIF(%u)\n", mld->mif_clk_table[level - 1]);
		pm_qos_update_request(&pm_qos_req_mif,
				mld->mif_clk_table[level - 1]);
	} else {
		mif_err("Unlock MIF(%u)\n", level);
		pm_qos_update_request(&pm_qos_req_mif, 0);
	}

	level = (qos_val >> 16) & 0xff;
	if (level > 0 && level <= mld->int_clk_cnt) {
		mif_err("Lock INT(%u)\n", mld->int_clk_table[level - 1]);
		pm_qos_update_request(&pm_qos_req_int,
				mld->int_clk_table[level - 1]);
	} else {
		mif_err("Unlock INT(%u)\n", level);
		pm_qos_update_request(&pm_qos_req_int, 0);
	}
}

static void shmem_qos_work_cpu(struct work_struct *work)
{
	struct mem_link_device *mld =
		container_of(work, struct mem_link_device, pm_qos_work_cpu);
	unsigned int qos_val;
	unsigned int level;

	qos_val = mbox_get_value(mld->mbx_perf_req_cpu);
	mif_err("pm_qos:0x%x requested\n", qos_val);

	level = (qos_val & 0xff);
	if (level > 0 && level <= mld->ap_clk_cnt) {
		mif_err("Lock CPU(%u)\n", mld->ap_clk_table[level - 1]);
		pm_qos_update_request(&pm_qos_req_cpu,
				mld->ap_clk_table[level - 1]);
	} else {
		mif_err("Unlock CPU(%u)\n", level);
		pm_qos_update_request(&pm_qos_req_cpu, 0);
	}
}

static void shmem_qos_work_mif(struct work_struct *work)
{
	struct mem_link_device *mld =
		container_of(work, struct mem_link_device, pm_qos_work_mif);
	unsigned int qos_val;
	unsigned int level;

	qos_val = mbox_get_value(mld->mbx_perf_req_mif);
	mif_err("pm_qos:0x%x requested\n", qos_val);

	level = (qos_val & 0xff);
	if (level > 0 && level <= mld->mif_clk_cnt) {
		mif_err("Lock MIF(%u)\n", mld->mif_clk_table[level - 1]);
		pm_qos_update_request(&pm_qos_req_mif,
				mld->mif_clk_table[level - 1]);
	} else {
		mif_err("Unlock MIF(%u)\n", level);
		pm_qos_update_request(&pm_qos_req_mif, 0);
	}
}

static void shmem_qos_work_int(struct work_struct *work)
{
	struct mem_link_device *mld =
		container_of(work, struct mem_link_device, pm_qos_work_int);
	unsigned int qos_val;
	unsigned int level;

	qos_val = mbox_get_value(mld->mbx_perf_req_int);
	mif_err("pm_qos:0x%x requested\n", qos_val);

	level = (qos_val & 0xff);
	if (level > 0 && level <= mld->int_clk_cnt) {
		mif_err("Lock INT(%u)\n", mld->int_clk_table[level - 1]);
		pm_qos_update_request(&pm_qos_req_int,
				mld->int_clk_table[level - 1]);
	} else {
		mif_err("Unlock INT(%u)\n", level);
		pm_qos_update_request(&pm_qos_req_int, 0);
	}
}

static void shmem_qos_req_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	mif_err("%s\n", __func__);

	schedule_work(&mld->pm_qos_work);
}

static void shmem_qos_cpu_req_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	mif_err("%s\n", __func__);

	schedule_work(&mld->pm_qos_work_cpu);
}

static void shmem_qos_mif_req_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	mif_err("%s\n", __func__);

	schedule_work(&mld->pm_qos_work_mif);
}

static void shmem_qos_int_req_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	mif_err("%s\n", __func__);

	schedule_work(&mld->pm_qos_work_int);
}

static void shmem_cp2ap_wakelock_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	unsigned int req;
	mif_err("%s\n", __func__);

	req = mbox_get_value(mld->mbx_cp2ap_wakelock);

	if (req == 0) {
	       if (wake_lock_active(&mld->cp_wakelock)) {
			wake_unlock(&mld->cp_wakelock);
			mif_err("cp_wakelock unlocked\n");
	       } else {
			mif_err("cp_wakelock already unlocked\n");
	       }
	} else if (req == 1) {
	       if (wake_lock_active(&mld->cp_wakelock)) {
			mif_err("cp_wakelock already unlocked\n");
	       } else {
			wake_lock(&mld->cp_wakelock);
			mif_err("cp_wakelock locked\n");
	       }
	} else {
		mif_err("unsupported request: cp_wakelock\n");
	}
}

static void shmem_cp2ap_pcie_l1ss_disable_handler(void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	unsigned int req;
	mif_err("%s\n", __func__);

	req = mbox_get_value(mld->mbx_cp2ap_pcie_l1ss_disable);

	if (req == 1) {
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_MODEM_IF);
		mif_err("cp requests pcie l1ss disable\n");
	} else if (req == 0) {
		exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_MODEM_IF);
		mif_err("cp requests pcie l1ss enable\n");
	} else {
		mif_err("unsupported request: pcie_l1ss_disable\n");
	}
}

static void remap_4mb_map_to_ipc_dev(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct shmem_4mb_phys_map *map;
	struct mem_ipc_device *dev;

	map = (struct shmem_4mb_phys_map *)mld->base;

	/* magic code and access enable fields */
	mld->magic = (u32 __iomem *)&map->magic;
	mld->access = (u32 __iomem *)&map->access;

	/* IPC_MAP_FMT */
	dev = &mld->ipc_dev[IPC_MAP_FMT];

	dev->id = IPC_MAP_FMT;
	strcpy(dev->name, "FMT");

	spin_lock_init(&dev->txq.lock);
	atomic_set(&dev->txq.busy, 0);
	dev->txq.head = &map->fmt_tx_head;
	dev->txq.tail = &map->fmt_tx_tail;
	dev->txq.buff = &map->fmt_tx_buff[0];
	dev->txq.size = SHM_4M_FMT_TX_BUFF_SZ;

	spin_lock_init(&dev->rxq.lock);
	atomic_set(&dev->rxq.busy, 0);
	dev->rxq.head = &map->fmt_rx_head;
	dev->rxq.tail = &map->fmt_rx_tail;
	dev->rxq.buff = &map->fmt_rx_buff[0];
	dev->rxq.size = SHM_4M_FMT_RX_BUFF_SZ;

	dev->msg_mask = MASK_SEND_FMT;
	dev->req_ack_mask = MASK_REQ_ACK_FMT;
	dev->res_ack_mask = MASK_RES_ACK_FMT;

	dev->skb_txq = &ld->skb_txq[IPC_MAP_FMT];
	dev->skb_rxq = &ld->skb_rxq[IPC_MAP_FMT];
	skb_queue_head_init(dev->skb_txq);
	skb_queue_head_init(dev->skb_rxq);

	dev->req_ack_cnt[TX] = 0;
	dev->req_ack_cnt[RX] = 0;

	mld->dev[IPC_MAP_FMT] = dev;

#ifdef CONFIG_MODEM_IF_LEGACY_QOS
	/* IPC_MAP_HPRIO_RAW */
	dev = &mld->ipc_dev[IPC_MAP_HPRIO_RAW];

	dev->id = IPC_MAP_HPRIO_RAW;
	strcpy(dev->name, "HPRIO_RAW");

	spin_lock_init(&dev->txq.lock);
	atomic_set(&dev->txq.busy, 0);
	dev->txq.head = &map->raw_hprio_tx_head;
	dev->txq.tail = &map->raw_hprio_tx_tail;
	dev->txq.buff = &map->raw_hprio_tx_buff[0];
	dev->txq.size = SHM_4M_RAW_HPRIO_TX_BUFF_SZ;

	spin_lock_init(&dev->rxq.lock);
	atomic_set(&dev->rxq.busy, 0);
	dev->rxq.head = &map->raw_hprio_rx_head;
	dev->rxq.tail = &map->raw_hprio_rx_tail;
	dev->rxq.buff = &map->raw_hprio_rx_buff[0];
	dev->rxq.size = SHM_4M_RAW_HPRIO_RX_BUFF_SZ;

	dev->msg_mask = MASK_SEND_RAW;
	dev->req_ack_mask = MASK_REQ_ACK_RAW;
	dev->res_ack_mask = MASK_RES_ACK_RAW;

	dev->skb_txq = &ld->skb_txq[IPC_MAP_HPRIO_RAW];
	dev->skb_rxq = &ld->skb_rxq[IPC_MAP_HPRIO_RAW];
	skb_queue_head_init(dev->skb_txq);
	skb_queue_head_init(dev->skb_rxq);

	dev->req_ack_cnt[TX] = 0;
	dev->req_ack_cnt[RX] = 0;

	mld->dev[IPC_MAP_HPRIO_RAW] = dev;
#endif

	/* IPC_MAP_NORM_RAW */
	dev = &mld->ipc_dev[IPC_MAP_NORM_RAW];

	dev->id = IPC_MAP_NORM_RAW;
	strcpy(dev->name, "NORM_RAW");

	spin_lock_init(&dev->txq.lock);
	atomic_set(&dev->txq.busy, 0);
	dev->txq.head = &map->raw_tx_head;
	dev->txq.tail = &map->raw_tx_tail;
	dev->txq.buff = &map->raw_tx_buff[0];
	dev->txq.size = SHM_4M_RAW_TX_BUFF_SZ;

	spin_lock_init(&dev->rxq.lock);
	atomic_set(&dev->rxq.busy, 0);
	dev->rxq.head = &map->raw_rx_head;
	dev->rxq.tail = &map->raw_rx_tail;
	dev->rxq.buff = &map->raw_rx_buff[0];
	dev->rxq.size = SHM_4M_RAW_RX_BUFF_SZ;

	dev->msg_mask = MASK_SEND_RAW;
	dev->req_ack_mask = MASK_REQ_ACK_RAW;
	dev->res_ack_mask = MASK_RES_ACK_RAW;

	dev->skb_txq = &ld->skb_txq[IPC_MAP_NORM_RAW];
	dev->skb_rxq = &ld->skb_rxq[IPC_MAP_NORM_RAW];
	skb_queue_head_init(dev->skb_txq);
	skb_queue_head_init(dev->skb_rxq);

	dev->req_ack_cnt[TX] = 0;
	dev->req_ack_cnt[RX] = 0;

	mld->dev[IPC_MAP_NORM_RAW] = dev;
}

static int shmem_rx_setup(struct link_device *ld)
{
	ld->rx_wq = alloc_workqueue(
			"mem_rx_work", WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!ld->rx_wq) {
		mif_err("%s: ERR! fail to create rx_wq\n", ld->name);
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&ld->rx_delayed_work, link_to_demux_work);

	return 0;
}


struct link_device *shmem_create_link_device(struct platform_device *pdev)
{
	struct modem_data *modem;
	struct mem_link_device *mld;
	struct link_device *ld;
	int err;
	mif_err("+++\n");

	/**
	 * Get the modem (platform) data
	 */
	modem = (struct modem_data *)pdev->dev.platform_data;
	if (!modem) {
		mif_err("ERR! modem == NULL\n");
		return NULL;
	}

	if (!modem->mbx) {
		mif_err("%s: ERR! mbx == NULL\n", modem->link_name);
		return NULL;
	}

	if (modem->ipc_version < SIPC_VER_50) {
		mif_err("%s<->%s: ERR! IPC version %d < SIPC_VER_50\n",
			modem->link_name, modem->name, modem->ipc_version);
		return NULL;
	}

	mif_err("MODEM:%s LINK:%s\n", modem->name, modem->link_name);

	/*
	** Alloc an instance of mem_link_device structure
	*/
	mld = kzalloc(sizeof(struct mem_link_device), GFP_KERNEL);
	if (!mld) {
		mif_err("%s<->%s: ERR! mld kzalloc fail\n",
			modem->link_name, modem->name);
		return NULL;
	}

	/*
	** Retrieve modem-specific attributes value
	*/
	mld->attrs = modem->link_attrs;

	/*====================================================================*\
		Initialize "memory snapshot buffer (MSB)" framework
	\*====================================================================*/
	if (msb_init() < 0) {
		mif_err("%s<->%s: ERR! msb_init() fail\n",
			modem->link_name, modem->name);
		goto error;
	}

	/*====================================================================*\
		Set attributes as a "link_device"
	\*====================================================================*/
	ld = &mld->link_dev;

	ld->name = modem->link_name;

	if (mld->attrs & LINK_ATTR(LINK_ATTR_SBD_IPC)) {
		mif_err("%s<->%s: LINK_ATTR_SBD_IPC\n", ld->name, modem->name);
		ld->sbd_ipc = true;
	}

	if (mld->attrs & LINK_ATTR(LINK_ATTR_IPC_ALIGNED)) {
		mif_err("%s<->%s: LINK_ATTR_IPC_ALIGNED\n",
			ld->name, modem->name);
		ld->aligned = true;
	}

	ld->ipc_version = modem->ipc_version;

	ld->mdm_data = modem;

	/*
	Set up link device methods
	*/
	ld->ioctl = shmem_ioctl;

	ld->init_comm = shmem_init_comm;
	ld->terminate_comm = shmem_terminate_comm;
	ld->send = shmem_send;

	ld->boot_on = shmem_boot_on;
	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_BOOT)) {
		if (mld->attrs & LINK_ATTR(LINK_ATTR_XMIT_BTDLR))
			ld->xmit_boot = shmem_xmit_boot;
		ld->dload_start = shmem_start_download;
		ld->firm_update = shmem_update_firm_info;
		ld->security_req = shmem_security_request;
	}

	ld->shmem_dump = shmem_full_dump;
	ld->force_dump = shmem_force_dump;

	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_DUMP))
		ld->dump_start = shmem_start_upload;

	ld->close_tx = shmem_close_tx;

	INIT_LIST_HEAD(&ld->list);

	spin_lock_init(&ld->netif_lock);
	atomic_set(&ld->netif_stopped, 0);
	ld->tx_flowctrl_mask = 0;
	init_completion(&ld->raw_tx_resumed);

	if (shmem_rx_setup(ld) < 0)
		goto error;

	if (mld->attrs & LINK_ATTR(LINK_ATTR_DPRAM_MAGIC)) {
		mif_err("%s<->%s: LINK_ATTR_DPRAM_MAGIC\n",
			ld->name, modem->name);
		mld->dpram_magic = true;
	}
#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	mld->iosm = true;
	mld->cmd_handler = iosm_event_bh;
	INIT_WORK(&mld->iosm_w, iosm_event_work);
#else
	mld->cmd_handler = shmem_cmd_handler;
#endif

	spin_lock_init(&mld->state_lock);
	mld->state = LINK_STATE_OFFLINE;

	/*
	** Initialize variables for TX & RX
	*/
	msb_queue_head_init(&mld->msb_rxq);
	msb_queue_head_init(&mld->msb_log);

	tasklet_init(&mld->rx_tsk, shmem_rx_task, (unsigned long)mld);

	hrtimer_init(&mld->tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mld->tx_timer.function = tx_timer_func;

	INIT_WORK(&mld->page_reclaim_work, shmem_oom_handler_work);

	/*
	** Initialize variables for CP booting and crash dump
	*/
	INIT_DELAYED_WORK(&mld->udl_rx_dwork, udl_rx_work);

	/**
	 * Link local functions to the corresponding function pointers that are
	 * mandatory for all memory-type link devices
	 */
	mld->recv_cp2ap_irq = recv_cp2ap_irq;
	mld->send_ap2cp_irq = send_ap2cp_irq;
	mld->recv_cp2ap_status = recv_cp2ap_status;

	/**
	 * Link local functions to the corresponding function pointers that are
	 * optional for some memory-type link devices
	 */
	mld->read_ap2cp_irq = read_ap2cp_irq;

	/**
	 * Initialize SHMEM maps for BOOT (physical map -> logical map)
	 */
	mld->shm_size = shm_get_phys_size();
	mld->boot_size = shm_get_boot_size();
	mld->boot_base = shm_get_boot_region();
	if (!mld->boot_base) {
		mif_err("Failed to vmap boot_region\n");
		goto error;
	}
	mif_err("boot_base=%p, boot_size=%lu\n",
		mld->boot_base, (unsigned long)mld->boot_size);

	/**
	 * Initialize SHMEM maps for IPC (physical map -> logical map)
	 */
	mld->size = shm_get_ipc_rgn_size();
	mld->base = shm_get_ipc_region();
	if (!mld->base) {
		mif_err("Failed to vmap ipc_region\n");
		shm_release_region();
		goto error;
	}
	mif_err("ipc_base=%p, ipc_size=%lu\n",
		mld->base, (unsigned long)mld->size);

	remap_4mb_map_to_ipc_dev(mld);

	if (ld->sbd_ipc) {
		hrtimer_init(&mld->sbd_tx_timer,
				CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		mld->sbd_tx_timer.function = sbd_tx_timer_func;

		err = create_sbd_link_device(ld,
				&mld->sbd_link_dev, mld->base, mld->size);
		if (err < 0)
			goto error;
	}

	/**
	 * Retrieve SHMEM MBOX#, IRQ#, etc.
	 */
	mld->mbx_cp2ap_msg = modem->mbx->mbx_cp2ap_msg;
	mld->irq_cp2ap_msg = modem->mbx->irq_cp2ap_msg;

	mld->mbx_ap2cp_msg = modem->mbx->mbx_ap2cp_msg;
	mld->int_ap2cp_msg = modem->mbx->int_ap2cp_msg;

	/**
	 * Register interrupt handlers
	 */
	err = mbox_request_irq(mld->irq_cp2ap_msg, shmem_irq_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq(%u) fail (%d)\n",
			ld->name, mld->irq_cp2ap_msg, err);
		goto error;
	}

	mld->mbx_perf_req = modem->mbx->mbx_cp2ap_perf_req;
	mld->mbx_perf_req_cpu = modem->mbx->mbx_cp2ap_perf_req_cpu;
	mld->mbx_perf_req_mif = modem->mbx->mbx_cp2ap_perf_req_mif;
	mld->mbx_perf_req_int = modem->mbx->mbx_cp2ap_perf_req_int;
	mld->irq_perf_req = modem->mbx->irq_cp2ap_perf_req;
	mld->irq_perf_req_cpu = modem->mbx->irq_cp2ap_perf_req_cpu;
	mld->irq_perf_req_mif = modem->mbx->irq_cp2ap_perf_req_mif;
	mld->irq_perf_req_int = modem->mbx->irq_cp2ap_perf_req_int;

	mld->ap_clk_table = modem->mbx->ap_clk_table;
	mld->ap_clk_cnt = modem->mbx->ap_clk_cnt;

	mld->mif_clk_table = modem->mbx->mif_clk_table;
	mld->mif_clk_cnt = modem->mbx->mif_clk_cnt;

	mld->int_clk_table = modem->mbx->int_clk_table;
	mld->int_clk_cnt = modem->mbx->int_clk_cnt;

	pm_qos_add_request(&pm_qos_req_cpu, PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, 0);

	INIT_WORK(&mld->pm_qos_work, shmem_qos_work);
	INIT_WORK(&mld->pm_qos_work_cpu, shmem_qos_work_cpu);
	INIT_WORK(&mld->pm_qos_work_mif, shmem_qos_work_mif);
	INIT_WORK(&mld->pm_qos_work_int, shmem_qos_work_int);

	err = mbox_request_irq(mld->irq_perf_req, shmem_qos_req_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq(%u) fail (%d)\n",
			ld->name, mld->irq_perf_req, err);
		goto error;
	}

	err = mbox_request_irq(mld->irq_perf_req_cpu, shmem_qos_cpu_req_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq(%u) fail (%d)\n",
			ld->name, mld->irq_perf_req_cpu, err);
		goto error;
	}

	err = mbox_request_irq(mld->irq_perf_req_mif, shmem_qos_mif_req_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq(%u) fail (%d)\n",
			ld->name, mld->irq_perf_req_mif, err);
		goto error;
	}

	err = mbox_request_irq(mld->irq_perf_req_int, shmem_qos_int_req_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq(%u) fail (%d)\n",
			ld->name, mld->irq_perf_req_int, err);
		goto error;
	}

	/**
	 * Retrieve SHMEM MBOX# and IRQ# for wakelock
	 */
	mld->mbx_cp2ap_wakelock = modem->mbx->mbx_cp2ap_wakelock;
	mld->irq_cp2ap_wakelock = modem->mbx->irq_cp2ap_wakelock;

	wake_lock_init(&mld->cp_wakelock, WAKE_LOCK_SUSPEND, ld->name);

	err = mbox_request_irq(mld->irq_cp2ap_wakelock,
			shmem_cp2ap_wakelock_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq(%u) fail (%d)\n",
			ld->name, mld->irq_perf_req_int, err);
		goto error;
	}

	mld->mbx_cp2ap_pcie_l1ss_disable = modem->mbx->mbx_cp2ap_pcie_l1ss_disable;
	mld->irq_cp2ap_pcie_l1ss_disable = modem->mbx->irq_cp2ap_pcie_l1ss_disable;

	err = mbox_request_irq(mld->irq_cp2ap_pcie_l1ss_disable,
			shmem_cp2ap_pcie_l1ss_disable_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq(%u) fail (%d)\n",
			ld->name, mld->irq_cp2ap_pcie_l1ss_disable, err);
		goto error;
	}

	/**
	 * For TX Flow-control command from CP
	 */
	mld->mbx_cp2ap_status = modem->mbx->mbx_cp2ap_status;
	mld->irq_cp2ap_status = modem->mbx->irq_cp2ap_status;
	mld->tx_flowctrl_cmd = 0;

	err = mbox_request_irq(mld->irq_cp2ap_status,
				shmem_tx_state_handler, mld);
	if (err) {
		mif_err("%s: ERR! mbox_request_irq(%u) fail (%d)\n",
			ld->name, mld->irq_cp2ap_status, err);
		goto error;
	}

	mld->pktlog = create_pktlog("shmem");
	if (!mld->pktlog)
		mif_err("packet log device create fail\n");

	mif_err("---\n");
	return ld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}
