/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/compress.h>

/* MCOMP input & output buffer number */
#define MCOMP_MAX_BUF		 3
/* MCOMP input & output buffer size(4K ~ 8M)*/
#define MCOMP_BUF_SIZE		 SZ_1M
#define MCOMP_THRESHOLD_PAGE_NUM (MCOMP_BUF_SIZE >> PAGE_SHIFT)
#define MCOMP_TOTAL_PAGES_NUM	 (MCOMP_MAX_BUF * MCOMP_THRESHOLD_PAGE_NUM)
#define MCOMP_MAX_TEMP_BUF	8
#define MCOMP_MAX_PAGES		2048
/*
 * This value means the minmum size without compression.
 * e.g) 16byte x (DEFAULT_MEMCPY_THRESHOLD + 1) = 4Kbyte
 */
#define DEFAULT_MEMCPY_THRESHOLD 0xFF
#define MCOMP_EXPIRED_TIME	1000
#define inbuf_is_filled(c) (c == MCOMP_THRESHOLD_PAGE_NUM)

struct buf_addr {
	u8 *inbuf;
	u8 *outbuf;
	u16 *comp_info;
};

struct mcomp_buf_info {
	struct buf_addr addr;
	struct list_head list;
	atomic_t cpy_cnt;
	u32 index;
};

struct mcomp_data {
	struct device *dev;
	void __iomem	*base;
	struct mcomp_buf_info buf[MCOMP_MAX_BUF];
	u32 (*comp_done)(struct compress_info *);
	struct timer_list op_timer;
	struct kthread_worker worker;
	struct kthread_work work;
	struct task_struct *task;
	u32 cnt_bufready;
	u32 cnt_compress;
	u32 cnt_irq;
	u32 cnt_workqueue;
	u32 cnt_update;
	bool is_running;
	u32 cur_idx;
	u8 buf_order;
};

/* regs */
#define CMD			0x0000
#define CMD_START		0x1
#define CMD_PAGES		16

#define IER			0x0004
#define IER_ENABLE		0x1

#define ISR			0x0008
#define ISR_CLEAR		1

#define DISK_ADDR		0x000C
#define COMPBUF_ADDR		0x0010
#define COMPINFO_ADDR		0x0014

#define CONTROL			0x0018
#define CONTROL_DRCG_DISABLE	8
#define CONTROL_ARCACHE		12
#define CONTROL_AWCACHE		16
#define CONTROL_THRESHOLD	20
#define CONTROL_AWUSER		28
#define CONTROL_ARUSER		29

#define BUS_CONFIG		0x001C
#define VERSION			0x0F00

static LIST_HEAD(mcomp_done_list);
static LIST_HEAD(mcomp_ready_list);
static DEFINE_SPINLOCK(buf_lock);
static DEFINE_SPINLOCK(page_index_lock);
static DEFINE_PER_CPU(u8 *, decomp_work_buf);

static void __lib_decomp(u8 *comp_data, u8 *decomp_buf,
		u32 comp_len, u8 *temp_buf)
{
	u8 ch_comp, dae_header, i;
	u8 *start_comp_data;
	u8 *start_temp_buf;
	u8 *start_decomp_buf;
	u16 mask;

	start_comp_data = (u8 *)comp_data;
	start_temp_buf = temp_buf;
	start_decomp_buf = decomp_buf;

	do {
		ch_comp = ~(*comp_data);
		comp_data++;
		for (mask = 0x1; mask <= 0x80; mask <<= 1) {
			if (ch_comp & mask) {
				*((u16 *)temp_buf) = *((u16 *)comp_data);
				temp_buf += 2;
				comp_data += 2;
			} else {
				*((u16 *)temp_buf) =
				*((u16 *)(temp_buf - (*comp_data & 0x7f)));
				temp_buf += 2;

				if ((*comp_data) >> 7) {
					*((u16 *)temp_buf) =
					*((u16 *)(temp_buf -
						(*comp_data & 0x7f)));
					temp_buf += 2;
				}
				comp_data++;
			}
		}
	} while ((unsigned long)comp_data -
		(unsigned long)start_comp_data < comp_len);

	temp_buf = start_temp_buf;

	do {
		dae_header = (*temp_buf);
		for (i = 0; i < 8; i++) {
			temp_buf += ((dae_header >> i) & 0x01);
			*(decomp_buf + i) =
				((dae_header >> i) & 0x01) * (*temp_buf);
		}
		decomp_buf += 8;
		temp_buf += 1;
	} while ((unsigned long)decomp_buf -
		(unsigned long)start_decomp_buf < SZ_4K);
}

static void mcomp_work_thread(struct kthread_work *work)
{
	unsigned long flags;
	struct list_head saved_list;
	struct mcomp_data *mcomp =
		container_of(work, struct mcomp_data, work);

	spin_lock_irqsave(&buf_lock, flags);
	list_replace_init(&mcomp_done_list, &saved_list);
	spin_unlock_irqrestore(&buf_lock, flags);

	while (!list_empty(&saved_list)) {
		struct mcomp_buf_info *buf;
		struct compress_info info;

		buf = list_entry(saved_list.next,
			struct mcomp_buf_info, list);
		list_del_init(&buf->list);
		exynos_ss_printkl(mcomp->cnt_workqueue++, buf->index);

		info.comp_header = buf->addr.comp_info;
		info.comp_addr = buf->addr.outbuf;
		info.first_idx = buf->index * MCOMP_THRESHOLD_PAGE_NUM;
		mcomp->comp_done(&info);
		atomic_set(&buf->cpy_cnt, 0);
		exynos_ss_printkl(mcomp->cnt_update++, buf->index);
	}
}

static void mcomp_compress_page(struct mcomp_data *mcomp)
{
	struct mcomp_buf_info *buf;
	u32 num_pages = MCOMP_THRESHOLD_PAGE_NUM;
	u32 cfg = 0;

	BUG_ON(list_empty(&mcomp_ready_list));
	buf = list_first_entry(&mcomp_ready_list, struct mcomp_buf_info, list);
	exynos_ss_printkl(mcomp->cnt_compress++, buf->index);
	mcomp->is_running = true;

	cfg = (virt_to_phys(buf->addr.inbuf) >> PAGE_SHIFT);
	__raw_writel(cfg, mcomp->base + DISK_ADDR);

	cfg = (virt_to_phys(buf->addr.outbuf) >> PAGE_SHIFT);
	__raw_writel(cfg, mcomp->base + COMPBUF_ADDR);

	cfg = (virt_to_phys(buf->addr.comp_info) >> PAGE_SHIFT);
	__raw_writel(cfg, mcomp->base + COMPINFO_ADDR);

	if (num_pages == MCOMP_MAX_PAGES)
		num_pages = 0;

	writel((num_pages << CMD_PAGES) | CMD_START, mcomp->base + CMD);

	mcomp->op_timer.expires =
		jiffies + msecs_to_jiffies(MCOMP_EXPIRED_TIME);
	mod_timer(&mcomp->op_timer, mcomp->op_timer.expires);
}

/*
 * mem_comp_irq - irq clear and scheduling the sswap thread
 */
static irqreturn_t mcomp_irq_handler(int irq, void *dev_id)
{
	int done_st = 0;
	struct mcomp_buf_info *buf;
	struct mcomp_data *mcomp = dev_id;

	/* check the incorrect access */
	done_st = __raw_readl(mcomp->base + ISR) & (ISR_CLEAR);

	if (!done_st) {
		dev_err(mcomp->dev, "interrupt does not happen\n");
		return IRQ_HANDLED;
	}

	__raw_writel(ISR_CLEAR, mcomp->base + ISR);
	del_timer(&mcomp->op_timer);

	spin_lock(&buf_lock);

	mcomp->is_running = false;
	BUG_ON(list_empty(&mcomp_ready_list));
	buf = list_first_entry(&mcomp_ready_list, struct mcomp_buf_info, list);
	list_move_tail(&buf->list, &mcomp_done_list);

	exynos_ss_printkl(mcomp->cnt_irq++, buf->index);
	queue_kthread_work(&mcomp->worker, &mcomp->work);

	if (!list_empty(&mcomp_ready_list)) {
		dev_dbg(mcomp->dev, "Compress job is pending..\n");
		mcomp_compress_page(mcomp);
	}

	spin_unlock(&buf_lock);

	return IRQ_HANDLED;
}

int mcomp_request_decompress(void *priv, u8 *in, u32 comp_len, u8 *out)
{
	int cpu;
	u8 *temp_buf;

	if (comp_len == SZ_4K) {
		memcpy(out, in, comp_len);
		return 0;
	}

	cpu = get_cpu();
	temp_buf = per_cpu(decomp_work_buf, cpu);
	__lib_decomp(in, out, comp_len, temp_buf);
	put_cpu();

	return 0;
}

static void mcomp_use_cci(struct mcomp_data *mcomp)
{
	u32 cfg = __raw_readl(mcomp->base + CONTROL);

	cfg |= (0x1 << CONTROL_ARUSER);
	cfg |= (0x1 << CONTROL_AWUSER);
	cfg |= (0x2 << CONTROL_ARCACHE);
	cfg |= (0x2 << CONTROL_AWCACHE);

	__raw_writel(cfg, mcomp->base + CONTROL);
}

static void mcomp_set_threshold(struct mcomp_data *mcomp, u8 value)
{
	u32 cfg = __raw_readl(mcomp->base + CONTROL);

	cfg |= (value << CONTROL_THRESHOLD);
	__raw_writel(cfg, mcomp->base + CONTROL);
}

static void mcomp_interrupt_enable(struct mcomp_data *mcomp)
{
	__raw_writel(IER_ENABLE, mcomp->base + IER);
}

static int mcomp_alloc_buffer(struct mcomp_data *mcomp, u32 buf_size)
{
	struct device *dev = mcomp->dev;
	struct buf_addr *addr;
	int i;
	u8 *temp_buf[NR_CPUS] = {0,};

	mcomp->buf_order = get_order(buf_size);

	/*
	 * Allocate H/W read/write(compress) buffer
	 * inbuf size		: MCOMP_BUF_SIZE * MCOMP_MAX_BUF
	 * outbuf size		: MCOMP_BUF_SIZE * MCOMP_MAX_BUF
	 * comp_info size	: MCOMP_TOTAL_PAGES_NUM * 2byte
	 * Because H/W supports only 32 bit address, allocation flag
	 * should be used GFP_DMA.
	 */
	for (i = 0; i < MCOMP_MAX_BUF; i++) {
		addr = &mcomp->buf[i].addr;

		addr->inbuf = (u8 *)__get_free_pages(GFP_DMA | __GFP_ZERO,
				mcomp->buf_order);
		if (!addr->inbuf)
			goto err_comp;

		addr->outbuf = (u8 *)__get_free_pages(GFP_DMA | __GFP_ZERO,
				mcomp->buf_order);

		if (!addr->outbuf)
			goto err_comp;

		addr->comp_info = (u16 *)get_zeroed_page(GFP_DMA);
		if (!addr->comp_info)
			goto err_comp;

		atomic_set(&mcomp->buf[i].cpy_cnt, 0);
		mcomp->buf[i].index = i;
	}

	/*
	 * Allocate S/W read/write(decompress) temporal buffer
	 */
	for_each_possible_cpu(i) {
		temp_buf[i] =
			(u8 *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
		if (!temp_buf[i])
			goto err_decomp;
		per_cpu(decomp_work_buf, i) = temp_buf[i];
	}

	return 0;

err_decomp:
	for_each_possible_cpu(i) {
		if (temp_buf[i])
			free_pages((unsigned long)temp_buf[i], 1);
	}

err_comp:
	for (i = 0; i < MCOMP_MAX_BUF; i++) {
		addr = &mcomp->buf[i].addr;
		if (addr->inbuf)
			free_pages((unsigned long)addr->inbuf,
					mcomp->buf_order);
		if (addr->outbuf)
			free_pages((unsigned long)addr->outbuf,
					mcomp->buf_order);
		if (addr->comp_info)
			free_pages((unsigned long)addr->comp_info, 0);
	}

	dev_err(dev, "failed alloc buffer\n");
	return -ENOMEM;
}

static void mcomp_dump_log(struct mcomp_data *mcomp)
{
	dev_dbg(mcomp->dev, "b(%d),c(%d),i(%d),w(%d),u(%d)\n",
		mcomp->cnt_bufready, mcomp->cnt_compress,
		mcomp->cnt_irq, mcomp->cnt_workqueue, mcomp->cnt_update);
	dev_dbg(mcomp->dev, "isr : %#x, running : %d\n",
		__raw_readl(mcomp->base + ISR), mcomp->is_running);
}

static void mcomp_op_timer_handler(unsigned long arg)
{
	struct mcomp_data *mcomp = (struct mcomp_data *)arg;

	dev_err(mcomp->dev, "MCOMP TIMEOUT OCCURRED\n");
	mcomp_dump_log(mcomp);
	BUG();
}

static int mcomp_device_init(void *priv, u32 *nr_pages, u32 *unit,
		u32 (*cb)(struct compress_info *))
{
	struct mcomp_data *mcomp = (struct mcomp_data *)priv;

	mcomp->comp_done = cb;
	*unit = MCOMP_THRESHOLD_PAGE_NUM;
	*nr_pages = MCOMP_TOTAL_PAGES_NUM;
	mcomp->is_running = false;
	mcomp->cur_idx = 0;

	mcomp_use_cci(mcomp);
	mcomp_set_threshold(mcomp, DEFAULT_MEMCPY_THRESHOLD);
	mcomp_interrupt_enable(mcomp);

	setup_timer(&mcomp->op_timer, mcomp_op_timer_handler,
			(unsigned long)mcomp);

	return mcomp_alloc_buffer(mcomp, MCOMP_BUF_SIZE);
}

static int mcomp_request_compress(void *priv, u8 *src, u32 *page_num)
{
	struct mcomp_data *mcomp = (struct mcomp_data *)priv;
	u32 buf_idx, page_idx;
	struct mcomp_buf_info *buf;

	spin_lock(&page_index_lock);

	page_idx = mcomp->cur_idx;
	buf_idx = page_idx >> mcomp->buf_order;
	buf = &mcomp->buf[buf_idx];
	if (atomic_read(&buf->cpy_cnt) == MCOMP_THRESHOLD_PAGE_NUM) {
		spin_unlock(&page_index_lock);
		mcomp_dump_log(mcomp);
		return -EBUSY;
	}

	if (++mcomp->cur_idx == MCOMP_TOTAL_PAGES_NUM)
		mcomp->cur_idx = 0;

	spin_unlock(&page_index_lock);

	*page_num = page_idx;
	memcpy(buf->addr.inbuf +
		((page_idx & (MCOMP_THRESHOLD_PAGE_NUM - 1)) << PAGE_SHIFT),
		src, SZ_4K);

	if (inbuf_is_filled(atomic_inc_return(&buf->cpy_cnt)))
		return COMP_READY;

	return COMP_MEMCPY;
}

static void mcomp_start_compress(void *priv, u32 page_idx)
{
	struct mcomp_data *mcomp = (struct mcomp_data *)priv;
	u32 buf_idx;
	struct mcomp_buf_info *buf;
	unsigned long flags;

	buf_idx = page_idx >> mcomp->buf_order;
	buf = &mcomp->buf[buf_idx];

	spin_lock_irqsave(&buf_lock, flags);

	list_add_tail(&buf->list, &mcomp_ready_list);
	exynos_ss_printkl(mcomp->cnt_bufready++, buf_idx);

	if (!mcomp->is_running)
		mcomp_compress_page(mcomp);

	spin_unlock_irqrestore(&buf_lock, flags);
}

static int mcomp_cancel_compress(void *priv, u8 *dst, u32 page_num)
{
	struct mcomp_data *mcomp = (struct mcomp_data *)priv;
	u8 *src;
	u32 index = (page_num >> mcomp->buf_order);

	page_num &= (MCOMP_THRESHOLD_PAGE_NUM - 1);
	src = mcomp->buf[index].addr.inbuf + (page_num << PAGE_SHIFT);
	memcpy(dst, src, SZ_4K);

	return 0;
}

static struct compress_ops mcomp_ops = {
	.device_init = mcomp_device_init,
	.request_compress = mcomp_request_compress,
	.start_compress = mcomp_start_compress,
	.cancel_compress = mcomp_cancel_compress,
	.request_decompress = mcomp_request_decompress,
};

static int mcomp_probe(struct platform_device *pdev)
{
	struct mcomp_data *mcomp;
	struct resource *res;
	int ret = 0;
	struct device *dev = &pdev->dev;

	mcomp = devm_kzalloc(dev, sizeof(*mcomp), GFP_KERNEL);
	if (!mcomp)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mcomp->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mcomp->base))
		return PTR_ERR(mcomp->base);

	ret = platform_get_irq(pdev, 0);
	if (ret <= 0) {
		dev_err(dev, "Unable to find IRQ resource\n");
		return ret;
	}

	ret = devm_request_irq(dev, ret, mcomp_irq_handler,
			IRQF_GIC_MULTI_TARGET, dev_name(dev), mcomp);
	if (ret) {
		dev_err(dev, "Unabled to register interrupt handler\n");
		return ret;
	}

	mcomp->dev = &pdev->dev;

	compress_register_ops(&mcomp_ops, mcomp);

	init_kthread_worker(&mcomp->worker);
	mcomp->task = kthread_run(kthread_worker_fn, &mcomp->worker,
			"mcomp-task");

	init_kthread_work(&mcomp->work, mcomp_work_thread);

	dev_info(dev, "Loaded driver for Mcomp\n");

	return ret;
}

static const struct of_device_id mcomp_dt_match[] = {
	{ .compatible = "samsung,exynos-mcomp", },
	{ },
};

static struct platform_driver mcomp_driver = {
	.probe		= mcomp_probe,
	.driver		= {
		.name	= "exynos-mcomp",
		.owner	= THIS_MODULE,
		.of_match_table = mcomp_dt_match,
	}
};

module_platform_driver(mcomp_driver);

MODULE_DESCRIPTION("memory_compressor");
MODULE_AUTHOR("Sungchun Kang <sungchun.kang@samsung.com>");
MODULE_LICENSE("GPL");
