/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_drv.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <asm/cacheflush.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include <linux/ion.h>
#include <linux/smc.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

/* TODO */
#define fimg2d_pm_qos_add(ctrl)
#define fimg2d_pm_qos_remove(ctrl)
#define fimg2d_pm_qos_update(ctrl, status)

#define POLL_TIMEOUT	2	/* 2 msec */
#define POLL_RETRY	1000
#define CTX_TIMEOUT	msecs_to_jiffies(10000)	/* 10 sec */

#ifdef DEBUG
int g2d_debug = DBG_INFO;
module_param(g2d_debug, int, S_IRUGO | S_IWUSR);
#endif

static struct fimg2d_control *ctrl;
static struct fimg2d_qos g2d_qos_table[G2D_LV_END];

static int fimg2d_do_bitblt(struct fimg2d_control *ctrl)
{
	int ret;

	pm_runtime_get_sync(ctrl->dev);
	fimg2d_debug("Done pm_runtime_get_sync()\n");

	fimg2d_clk_on(ctrl);
	ret = ctrl->blit(ctrl);
	fimg2d_clk_off(ctrl);

	pm_runtime_put_sync(ctrl->dev);
	fimg2d_debug("Done pm_runtime_put_sync()\n");

	return ret;
}

static void fimg2d_job_running(struct work_struct *work)
{
	struct fimg2d_control *ctrl;

	ctrl = container_of(work, struct fimg2d_control, running_work);

	fimg2d_info("Start running thread\n");

	while (1) {
		wait_event(ctrl->running_waitq,
				atomic_read(&ctrl->n_running_q));
		fimg2d_debug("Job running...\n");
		fimg2d_do_bitblt(ctrl);
	}
}

static int fimg2d_fence_wait(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	int i;
	int err;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		if (img->fence) {
			err = sync_fence_wait(img->fence, 900);
			if (err < 0) {
				fimg2d_err("Error(%d) for waiting src[%d]\n",
									err, i);
			}
			sync_fence_put(img->fence);
		}
	}

	img = &cmd->image_dst;
	if (img->fence) {
		err = sync_fence_wait(img->fence, 900);
		if (err < 0)
			fimg2d_err("Error(%d) for waiting dst\n", err);
		sync_fence_put(img->fence);
	}

	return 0;
}

static void fimg2d_job_waiting(struct work_struct *work)
{
	struct fimg2d_control *ctrl;
	struct fimg2d_bltcmd *cmd;
	unsigned long flags;

	ctrl = container_of(work, struct fimg2d_control, waiting_work);

	fimg2d_info("Start waiting thread\n");

	while (1) {
		wait_event(ctrl->waiting_waitq,
					atomic_read(&ctrl->n_waiting_q));
		fimg2d_debug("Job waiting...\n");

		/* Get the waiting command */
		cmd = fimg2d_get_command(ctrl, 1);

		/* Do the fence waiting */
		fimg2d_fence_wait(cmd);

		/* Remove list item from waiting q and add to running q */
		/* FIXME: Need to cleanup to use common API */
		g2d_spin_lock(&ctrl->bltlock, flags);
		list_del(&cmd->node);
		atomic_dec(&ctrl->n_waiting_q);
		list_add_tail(&cmd->node, &ctrl->running_job_q);
		atomic_inc(&ctrl->n_running_q);
		g2d_spin_unlock(&ctrl->bltlock, flags);

		wake_up(&ctrl->running_waitq);
	}
}

static int fimg2d_context_wait(struct fimg2d_context *ctx)
{
	int ret;

	ret = wait_event_timeout(ctx->wait_q, !atomic_read(&ctx->ncmd),
			CTX_TIMEOUT);
	if (!ret) {
		fimg2d_err("ctx %p wait timeout\n", ctx);
		return -ETIME;
	}

	if (ctx->state == CTX_ERROR) {
		ctx->state = CTX_READY;
		fimg2d_err("ctx %p error before blit\n", ctx);
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t fimg2d_irq(int irq, void *dev_id)
{
	fimg2d_debug("irq\n");
	spin_lock(&ctrl->bltlock);
	ctrl->stop(ctrl);
	spin_unlock(&ctrl->bltlock);

	return IRQ_HANDLED;
}

static int fimg2d_request_bitblt(struct fimg2d_control *ctrl,
		struct fimg2d_bltcmd *cmd)
{
	unsigned long flags;
	wait_queue_head_t *waitq;
	struct fimg2d_context *ctx = cmd->ctx;

	fimg2d_debug("Request bitblt\n");

	if (cmd->blt.use_fence)
		waitq = &ctrl->waiting_waitq;
	else
		waitq = &ctrl->running_waitq;

	g2d_spin_lock(&ctrl->bltlock, flags);
	atomic_inc(&ctx->ncmd);
	fimg2d_enqueue(cmd, ctrl);
	fimg2d_debug("ctx %p pgd %p ncmd(%d) seq_no(%u)\n", ctx,
			ctx->mm ? (unsigned long *)ctx->mm->pgd : NULL,
			atomic_read(&ctx->ncmd), cmd->blt.seq_no);
	g2d_spin_unlock(&ctrl->bltlock, flags);

	wake_up(waitq);

	return 0;
}

static int fimg2d_open(struct inode *inode, struct file *file)
{
	struct fimg2d_context *ctx;
	unsigned long flags, count;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		fimg2d_err("not enough memory for ctx\n");
		return -ENOMEM;
	}
	file->private_data = (void *)ctx;

	g2d_spin_lock(&ctrl->bltlock, flags);
	fimg2d_add_context(ctrl, ctx);
	count = atomic_read(&ctrl->nctx);
	g2d_spin_unlock(&ctrl->bltlock, flags);

	return 0;
}

static int fimg2d_release(struct inode *inode, struct file *file)
{
	struct fimg2d_context *ctx = file->private_data;
	int retry = POLL_RETRY;
	unsigned long flags, count;

	fimg2d_debug("ctx %p\n", ctx);
	while (retry--) {
		if (!atomic_read(&ctx->ncmd))
			break;
		mdelay(POLL_TIMEOUT);
	}

	g2d_spin_lock(&ctrl->bltlock, flags);
	fimg2d_del_context(ctrl, ctx);
	count = atomic_read(&ctrl->nctx);
	g2d_spin_unlock(&ctrl->bltlock, flags);

	kfree(ctx);
	return 0;
}

static int fimg2d_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static unsigned int fimg2d_poll(struct file *file, struct poll_table_struct *w)
{
	return 0;
}

static long fimg2d_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct fimg2d_context *ctx;
	struct mm_struct *mm;
	struct fimg2d_bltcmd *bltcmd;
	struct fimg2d_image *img;

	struct fimg2d_blit __user *user_blt;
	struct fimg2d_image __user *user_img;
	int i, err;

	ctx = file->private_data;

	switch (cmd) {
	case FIMG2D_BITBLT_BLIT:
		mm = get_task_mm(current);
		if (!mm) {
			fimg2d_err("no mm for ctx\n");
			return -ENXIO;
		}

		g2d_lock(&ctrl->drvlock);
		ctx->mm = mm;

		if (atomic_read(&ctrl->suspended)) {
			fimg2d_err("driver is unavailable, do sw fallback\n");
			g2d_unlock(&ctrl->drvlock);
			mmput(mm);
			return -EPERM;
		}

		bltcmd = fimg2d_add_command(ctrl, ctx,
					(struct fimg2d_blit __user *)arg, &ret);
		if (ret) {
			fimg2d_err("add command not allowed, ret = %d\n", ret);
			g2d_unlock(&ctrl->drvlock);
			mmput(mm);
			return ret;
		}

		fimg2d_pm_qos_update(ctrl, FIMG2D_QOS_ON);

		if (bltcmd->blt.use_fence) {
			mmput(mm);
			user_blt = (struct fimg2d_blit __user *)arg;
			for (i = 0; i < MAX_SRC; i++) {
				img = &bltcmd->image_src[i];
				if (!img->addr.type)
					continue;

				user_img = (struct fimg2d_image __user *)
							user_blt->src[i];

				err = put_user(img->release_fence_fd,
						&user_img->release_fence_fd);
			}
			img = &bltcmd->image_dst;
			user_img = (struct fimg2d_image __user *)user_blt->dst;
			err = put_user(img->release_fence_fd,
						&user_img->release_fence_fd);
		}

		ret = fimg2d_request_bitblt(ctrl, bltcmd);
		if (ret) {
			/* actually dead code */
			fimg2d_info("request bitblit not allowed\n");
			fimg2d_info("so passing to s/w fallback.\n");
			g2d_unlock(&ctrl->drvlock);
			if (!bltcmd->blt.use_fence)
				mmput(mm);
			return -EBUSY;
		}

		g2d_unlock(&ctrl->drvlock);

		break;

	case FIMG2D_BITBLT_SYNC:
		fimg2d_debug("Begin sync of ctx %p\n", ctx);
		ret = fimg2d_context_wait(ctx);
		if (ret)
			fimg2d_err("Failed to wait, ret = %d\n", ret);
		fimg2d_debug("End of sync ctx %p\n", ctx);

		/* assumes fence user does not call this ioctl */
		mmput(ctx->mm);
		break;

	default:
		fimg2d_err("unknown ioctl\n");
		ret = -EFAULT;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static int compat_get_fimg2d_param(struct fimg2d_param __user *data,
				struct compat_fimg2d_param __user *data32)
{
	int err;
	compat_ulong_t ul;
	compat_int_t i;
	enum rotation rotate_mode;
	enum premultiplied premult;
	enum scaling scaling_mode;
	enum repeat repeat_mode;
	unsigned char g_alpha;
	bool b;

	err = get_user(ul, &data32->solid_color);
	err |= put_user(ul, &data->solid_color);
	err |= get_user(g_alpha, &data32->g_alpha);
	err |= put_user(g_alpha, &data->g_alpha);
	err |= get_user(rotate_mode, &data32->rotate);
	err |= put_user(rotate_mode, &data->rotate);
	err |= get_user(premult, &data32->premult);
	err |= put_user(premult, &data->premult);
	err |= get_user(scaling_mode, &data32->scaling.mode);
	err |= put_user(scaling_mode, &data->scaling.mode);
	err |= get_user(i, &data32->scaling.src_w);
	err |= put_user(i, &data->scaling.src_w);
	err |= get_user(i, &data32->scaling.src_h);
	err |= put_user(i, &data->scaling.src_h);
	err |= get_user(i, &data32->scaling.dst_w);
	err |= put_user(i, &data->scaling.dst_w);
	err |= get_user(i, &data32->scaling.dst_h);
	err |= put_user(i, &data->scaling.dst_h);
	err |= get_user(repeat_mode, &data32->repeat.mode);
	err |= put_user(repeat_mode, &data->repeat.mode);
	err |= get_user(ul, &data32->repeat.pad_color);
	err |= put_user(ul, &data->repeat.pad_color);
	err |= get_user(b, &data32->clipping.enable);
	err |= put_user(b, &data->clipping.enable);
	err |= get_user(i, &data32->clipping.x1);
	err |= put_user(i, &data->clipping.x1);
	err |= get_user(i, &data32->clipping.y1);
	err |= put_user(i, &data->clipping.y1);
	err |= get_user(i, &data32->clipping.x2);
	err |= put_user(i, &data->clipping.x2);
	err |= get_user(i, &data32->clipping.y2);
	err |= put_user(i, &data->clipping.y2);

	return err;
}

static int compat_get_fimg2d_image(struct fimg2d_image __user *data,
							compat_uptr_t uaddr)
{
	struct compat_fimg2d_image __user *data32 = compat_ptr(uaddr);
	compat_int_t i;
	compat_ulong_t ul;
	enum pixel_order order;
	enum color_format fmt;
	enum addr_space addr_type;
	enum blit_op op;
	bool need_cacheopr;
	int err;

	err = get_user(i, &data32->layer_num);
	err |= put_user(i, &data->layer_num);
	err |= get_user(i, &data32->width);
	err |= put_user(i, &data->width);
	err |= get_user(i, &data32->height);
	err |= put_user(i, &data->height);
	err |= get_user(i, &data32->stride);
	err |= put_user(i, &data->stride);
	err |= get_user(op, &data32->op);
	err |= put_user(op, &data->op);
	err |= get_user(order, &data32->order);
	err |= put_user(order, &data->order);
	err |= get_user(fmt, &data32->fmt);
	err |= put_user(fmt, &data->fmt);
	/* struct fimg2d_param is handled in other function. */
	err |= get_user(addr_type, &data32->addr.type);
	err |= put_user(addr_type, &data->addr.type);
	err |= get_user(ul, &data32->addr.start);
	err |= put_user(ul, &data->addr.start);
	err |= get_user(ul, &data32->addr.header);
	err |= put_user(ul, &data->addr.header);
	err |= get_user(i, &data32->rect.x1);
	err |= put_user(i, &data->rect.x1);
	err |= get_user(i, &data32->rect.y1);
	err |= put_user(i, &data->rect.y1);
	err |= get_user(i, &data32->rect.x2);
	err |= put_user(i, &data->rect.x2);
	err |= get_user(i, &data32->rect.y2);
	err |= put_user(i, &data->rect.y2);
	err |= get_user(need_cacheopr, &data32->need_cacheopr);
	err |= put_user(need_cacheopr, &data->need_cacheopr);
	err |= get_user(i, &data32->acquire_fence_fd);
	err |= put_user(i, &data->acquire_fence_fd);

	err |= compat_get_fimg2d_param(&data->param, &data32->param);

	return err;
}

static long compat_fimg2d_ioctl32(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	switch (cmd) {
	case COMPAT_FIMG2D_BITBLT_BLIT:
	{
		struct compat_fimg2d_blit __user *data32;
		struct fimg2d_blit __user *data;
		struct mm_struct *mm;
		bool use_fence, dither;
		enum fimg2d_qos_level qos_lv;
		compat_uint_t seq_no;
		unsigned long stack_cursor = 0;
		int err;
		int i;

		mm = get_task_mm(current);
		if (!mm) {
			fimg2d_err("no mm for ctx\n");
			return -ENXIO;
		}

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data) {
			fimg2d_err("failed to allocate user compat space\n");
			mmput(mm);
			return -ENOMEM;
		}

		stack_cursor += sizeof(*data);
		if (clear_user(data, sizeof(*data))) {
			fimg2d_err("failed to access to userspace\n");
			mmput(mm);
			return -EPERM;
		}

		err = get_user(use_fence, &data32->use_fence);
		err |= put_user(use_fence, &data->use_fence);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		err = get_user(dither, &data32->dither);
		err |= put_user(dither, &data->dither);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		for (i = 0; i < MAX_SRC; i++) {
			if (data32->src[i]) {
				unsigned long size;

				size = sizeof(*data->src[i]) + stack_cursor;
				data->src[i] = compat_alloc_user_space(size);
				if (!data->src[i]) {
					fimg2d_err("user space alloc failed");
					fimg2d_err("src layer[%d]", i);
					mmput(mm);
					return -ENOMEM;
				}

				stack_cursor += sizeof(*data->src[i]);
				err = compat_get_fimg2d_image(data->src[i],
							data32->src[i]);
				if (err) {
					fimg2d_err("failed to get src data\n");
					mmput(mm);
					return err;
				}

			}
		}

		if (data32->dst) {
			data->dst = compat_alloc_user_space(sizeof(*data->dst) +
								stack_cursor);
			if (!data->dst) {
				fimg2d_err("failed to allocate user compat space\n");
				mmput(mm);
				return -ENOMEM;
			}

			stack_cursor += sizeof(*data->dst);
			err = compat_get_fimg2d_image(data->dst, data32->dst);
			if (err) {
				fimg2d_err("failed to get compat data\n");
				mmput(mm);
				return err;
			}
		}

		err = get_user(seq_no, &data32->seq_no);
		err |= put_user(seq_no, &data->seq_no);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		err = get_user(qos_lv, &data32->qos_lv);
		err |= put_user(qos_lv, &data->qos_lv);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		err = file->f_op->unlocked_ioctl(file,
				FIMG2D_BITBLT_BLIT, (unsigned long)data);
		mmput(mm);
		return err;
	}
	case COMPAT_FIMG2D_BITBLT_SYNC:
	{
		return file->f_op->unlocked_ioctl(file,
				FIMG2D_BITBLT_SYNC, arg);
	}
	default:
		fimg2d_err("unknown ioctl\n");
		return -EINVAL;
	}
}
#endif

/* fops */
static const struct file_operations fimg2d_fops = {
	.owner          = THIS_MODULE,
	.open           = fimg2d_open,
	.release        = fimg2d_release,
	.mmap           = fimg2d_mmap,
	.poll           = fimg2d_poll,
	.unlocked_ioctl = fimg2d_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_fimg2d_ioctl32,
#endif
};

/* miscdev */
static struct miscdevice fimg2d_dev = {
	.minor		= FIMG2D_MINOR,
	.name		= "fimg2d",
	.fops		= &fimg2d_fops,
};

static int fimg2d_setup_controller(struct fimg2d_control *ctrl)
{
	atomic_set(&ctrl->suspended, 0);
	atomic_set(&ctrl->clkon, 0);
	atomic_set(&ctrl->busy, 0);
	atomic_set(&ctrl->nctx, 0);

	spin_lock_init(&ctrl->bltlock);
	mutex_init(&ctrl->drvlock);
	ctrl->boost = false;

	INIT_LIST_HEAD(&ctrl->cmd_q);
	init_waitqueue_head(&ctrl->wait_q);

	fimg2d_register_ops(ctrl);

	atomic_set(&ctrl->n_running_q, 0);
	INIT_LIST_HEAD(&ctrl->running_job_q);
	init_waitqueue_head(&ctrl->running_waitq);

	atomic_set(&ctrl->n_waiting_q, 0);
	INIT_LIST_HEAD(&ctrl->waiting_job_q);
	init_waitqueue_head(&ctrl->waiting_waitq);

	ctrl->running_workqueue = create_singlethread_workqueue("kg2d_rund");
	if (!ctrl->running_workqueue)
		return -ENOMEM;
	INIT_WORK(&ctrl->running_work, fimg2d_job_running);
	queue_work(ctrl->running_workqueue, &ctrl->running_work);

	ctrl->waiting_workqueue = create_singlethread_workqueue("kg2d_waitd");
	if (!ctrl->waiting_workqueue)
		return -ENOMEM;
	INIT_WORK(&ctrl->waiting_work, fimg2d_job_waiting);
	queue_work(ctrl->waiting_workqueue, &ctrl->waiting_work);

	ctrl->timeline = sw_sync_timeline_create("fimg2d");
	ctrl->timeline_max = 0;

	return 0;
}

static int parse_g2d_qos_platdata(struct device_node *np,
				char *node_name, struct fimg2d_qos *pdata)
{
	int ret = 0;
	struct device_node *np_qos;

	np_qos = of_find_node_by_name(np, node_name);
	if (!np_qos) {
		pr_err("%s: could not find fimg2d qos platdata node\n",
				node_name);
		return -EINVAL;
	}

	of_property_read_u32(np_qos, "freq_int", &pdata->freq_int);
	of_property_read_u32(np_qos, "freq_mif", &pdata->freq_mif);
	of_property_read_u32(np_qos, "freq_cpu", &pdata->freq_cpu);
	of_property_read_u32(np_qos, "freq_kfc", &pdata->freq_kfc);

	fimg2d_info("cpu_min:%d, kfc_min:%d, mif_min:%d, int_min:%d\n"
			, pdata->freq_cpu, pdata->freq_kfc
			, pdata->freq_mif, pdata->freq_int);

	return ret;
}

static void g2d_parse_dt(struct device_node *np, struct fimg2d_platdata *pdata)
{
	struct device_node *np_qos;

	if (!np)
		return;

	of_property_read_u32(np, "ip_ver", &pdata->ip_ver);

	np_qos = of_get_child_by_name(np, "g2d_qos_table");
	if (!np_qos) {
		struct device_node *np_pdata =
				of_find_node_by_name(NULL, "fimg2d_pdata");
		if (!np_pdata)
			BUG();

		np_qos = of_get_child_by_name(np_pdata, "g2d_qos_table");
		if (!np_qos)
			BUG();
	}

	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_0", &g2d_qos_table[0]);
	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_1", &g2d_qos_table[1]);
	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_2", &g2d_qos_table[2]);
	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_3", &g2d_qos_table[3]);
	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_4", &g2d_qos_table[4]);

}

static int __attribute__((unused)) fimg2d_sysmmu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova, int flags, void *token)
{
	struct fimg2d_bltcmd *cmd;

	cmd = fimg2d_get_command(ctrl, 0);
	if (!cmd) {
		fimg2d_err("no available command\n");
		goto done;
	}

	fimg2d_debug_command(cmd);

	if (atomic_read(&ctrl->busy)) {
		fimg2d_err("dumping g2d registers..\n");
		ctrl->dump(ctrl);
	}
done:
	return 0;
}

static int fimg2d_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct fimg2d_platdata *pdata;
	struct device *dev = &pdev->dev;
	int id = 0;

	dev_info(&pdev->dev, "++%s\n", __func__);

	if (dev->of_node) {
		id = of_alias_get_id(pdev->dev.of_node, "fimg2d");
	} else {
		id = pdev->id;
		pdata = dev->platform_data;
		if (!pdata) {
			dev_err(&pdev->dev, "no platform data\n");
			return -EINVAL;
		}
	}

	/* global structure */
	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl) {
		fimg2d_err("failed to allocate memory for controller\n");
		return -ENOMEM;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		fimg2d_err("failed to allocate memory for controller\n");
		kfree(ctrl);
		return -ENOMEM;
	}
	ctrl->pdata = pdata;
	g2d_parse_dt(dev->of_node, ctrl->pdata);

	/* setup global ctrl */
	ret = fimg2d_setup_controller(ctrl);
	if (ret) {
		fimg2d_err("failed to setup controller\n");
		goto drv_free;
	}
	ctrl->dev = &pdev->dev;

	/* memory region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		fimg2d_err("failed to get resource\n");
		ret = -ENOENT;
		goto drv_free;
	}

	ctrl->mem = request_mem_region(res->start, resource_size(res),
					pdev->name);
	if (!ctrl->mem) {
		fimg2d_err("failed to request memory region\n");
		ret = -ENOMEM;
		goto drv_free;
	}

	/* ioremap */
	ctrl->regs = ioremap(res->start, resource_size(res));
	if (!ctrl->regs) {
		fimg2d_err("failed to ioremap for SFR\n");
		ret = -ENOENT;
		goto mem_free;
	}
	fimg2d_debug("base address: 0x%lx\n", (unsigned long)res->start);

	/* irq */
	ctrl->irq = platform_get_irq(pdev, 0);
	if (!ctrl->irq) {
		fimg2d_err("failed to get irq resource\n");
		ret = -ENOENT;
		goto reg_unmap;
	}
	fimg2d_debug("irq: %d\n", ctrl->irq);

	ret = request_irq(ctrl->irq, fimg2d_irq, IRQF_DISABLED,
			pdev->name, ctrl);
	if (ret) {
		fimg2d_err("failed to request irq\n");
		ret = -ENOENT;
		goto reg_unmap;
	}

	ret = fimg2d_clk_setup(ctrl);
	if (ret) {
		fimg2d_err("failed to setup clk\n");
		ret = -ENOENT;
		goto irq_free;
	}

	spin_lock_init(&ctrl->qoslock);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(ctrl->dev);
	fimg2d_info("enable runtime pm\n");
#else
	fimg2d_clk_on(ctrl);
#endif

#ifdef CONFIG_ION_EXYNOS
	ctrl->g2d_ion_client = ion_client_create(ion_exynos, "g2d");
	if (IS_ERR(ctrl->g2d_ion_client)) {
		fimg2d_err("failed to ion_client_create\n");
		goto clk_release;
	}
#endif

	iovmm_set_fault_handler(dev, fimg2d_sysmmu_fault_handler, ctrl);

	fimg2d_debug("register sysmmu page fault handler\n");

	/* misc register */
	ret = misc_register(&fimg2d_dev);
	if (ret) {
		fimg2d_err("failed to register misc driver\n");
		goto clk_release;
	}

	fimg2d_pm_qos_add(ctrl);

	dev_info(&pdev->dev, "fimg2d registered successfully\n");

	return 0;

clk_release:
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(ctrl->dev);
#else
	fimg2d_clk_off(ctrl);
#endif
	fimg2d_clk_release(ctrl);

irq_free:
	free_irq(ctrl->irq, NULL);
reg_unmap:
	iounmap(ctrl->regs);
mem_free:
	release_mem_region(res->start, resource_size(res));
drv_free:
	if (ctrl->running_workqueue)
		destroy_workqueue(ctrl->running_workqueue);
	if (ctrl->waiting_workqueue)
		destroy_workqueue(ctrl->waiting_workqueue);
	mutex_destroy(&ctrl->drvlock);
	kfree(pdata);
	kfree(ctrl);

	return ret;
}

static int fimg2d_remove(struct platform_device *pdev)
{
	struct fimg2d_platdata *pdata;
	pdata = ctrl->pdata;
	fimg2d_pm_qos_remove(ctrl);

	misc_deregister(&fimg2d_dev);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#else
	fimg2d_clk_off(ctrl);
#endif

	fimg2d_clk_release(ctrl);
	free_irq(ctrl->irq, NULL);

	if (ctrl->mem) {
		iounmap(ctrl->regs);
		release_resource(ctrl->mem);
		kfree(ctrl->mem);
	}

	destroy_workqueue(ctrl->running_workqueue);
	destroy_workqueue(ctrl->waiting_workqueue);

	mutex_destroy(&ctrl->drvlock);
	kfree(ctrl);
	kfree(pdata);
	return 0;
}

static int fimg2d_suspend(struct device *dev)
{
	unsigned long flags;
	int retry = POLL_RETRY;

	g2d_spin_lock(&ctrl->bltlock, flags);
	atomic_set(&ctrl->suspended, 1);
	g2d_spin_unlock(&ctrl->bltlock, flags);
	while (retry--) {
		if (fimg2d_queue_is_empty(&ctrl->running_job_q))
			break;
		mdelay(POLL_TIMEOUT);
	}

	fimg2d_info("suspend... done\n");
	return 0;
}

static int fimg2d_resume(struct device *dev)
{
	unsigned long flags;
	int ret = 0;

	g2d_spin_lock(&ctrl->bltlock, flags);
	atomic_set(&ctrl->suspended, 0);
	g2d_spin_unlock(&ctrl->bltlock, flags);

	fimg2d_info("resume... done\n");
	return ret;
}
#ifdef CONFIG_PM_RUNTIME
static int fimg2d_runtime_suspend(struct device *dev)
{
	fimg2d_debug("runtime suspend... done\n");

	return 0;
}

static int fimg2d_runtime_resume(struct device *dev)
{
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	int ret;

	if (ip_is_g2d_8j()) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT, MC_FC_DRM_SET_CFW_PROT,
				PROT_G2D, 0);
		if (ret != SMC_TZPC_OK)
			fimg2d_err("fail to set cfw protection (%d)\n", ret);
		else
			fimg2d_debug("success to set cfw protection\n");
	}
#endif
	fimg2d_debug("runtime resume... done\n");

	return 0;
}
#endif

static const struct dev_pm_ops fimg2d_pm_ops = {
	.suspend		= fimg2d_suspend,
	.resume			= fimg2d_resume,
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend	= fimg2d_runtime_suspend,
	.runtime_resume		= fimg2d_runtime_resume,
#endif
};

static const struct of_device_id exynos_fimg2d_match[] = {
	{
		.compatible = "samsung,s5p-fimg2d",
	},
	{},
};

MODULE_DEVICE_TABLE(of, exynos_fimg2d_match);

static struct platform_driver fimg2d_driver = {
	.probe		= fimg2d_probe,
	.remove		= fimg2d_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s5p-fimg2d",
		.pm     = &fimg2d_pm_ops,
		.of_match_table = exynos_fimg2d_match,
	},
};

static char banner[] __initdata =
	"Exynos Graphics 2D driver, (c) 2015 Samsung Electronics\n";

static int __init fimg2d_register(void)
{
	pr_info("%s", banner);
	return platform_driver_register(&fimg2d_driver);
}

static void __exit fimg2d_unregister(void)
{
	platform_driver_unregister(&fimg2d_driver);
}

int fimg2d_ip_version_is(void)
{
	struct fimg2d_platdata *pdata;

	pdata = ctrl->pdata;

	return pdata->ip_ver;
}

module_init(fimg2d_register);
module_exit(fimg2d_unregister);

MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_AUTHOR("Janghyuck Kim <janghyuck.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung Graphics 2D driver");
MODULE_LICENSE("GPL");
