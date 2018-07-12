/*
 * drivers/media/m2m1shot-testdev.c
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * Contact: Cho KyongHo <pullip.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/highmem.h>

#include <media/m2m1shot2.h>

#include "../iommu/exynos-iommu.h"


struct m2m1shot2_testdev_drvdata {
	struct m2m1shot2_device *m21dev;
	struct task_struct *thread;
	wait_queue_head_t waitqueue;
	struct list_head task_list;
};

struct m2m1shot2_testdev_work {
	struct list_head node;
	struct m2m1shot2_context *ctx;
};

static int m2m1shot2_testdev_init_context(struct m2m1shot2_context *ctx)
{
	struct m2m1shot2_testdev_work *work;

	work = kmalloc(sizeof(*work), GFP_KERNEL);
	if (!work)
		return -ENOMEM;

	INIT_LIST_HEAD(&work->node);
	work->ctx = ctx;

	ctx->priv = work;
	return 0;
}

static int m2m1shot2_testdev_free_context(struct m2m1shot2_context *ctx)
{
	kfree(ctx->priv);
	return 0;
}

static int m2m1shot2_testdev_prepare_format(
			struct m2m1shot2_context_format *fmt,
			unsigned int index, enum dma_data_direction dir,
			size_t payload[], unsigned int *num_planes)
{
	payload[0] = fmt->fmt.width * fmt->fmt.height * 4;
	*num_planes = 1;
	return 0;
}

static int m2m1shot2_testdev_prepare_source(struct m2m1shot2_context *ctx,
			unsigned int index, struct m2m1shot2_source_image *img)
{
	return 0;
}

static int m2m1shot2_testdev_device_run(struct m2m1shot2_context *ctx)
{
	struct m2m1shot2_testdev_work *work = ctx->priv;
	struct device *dev = ctx->m21dev->dev;
	struct m2m1shot2_testdev_drvdata *drvdata = dev_get_drvdata(dev);

	INIT_LIST_HEAD(&work->node);

	BUG_ON(!list_empty(&drvdata->task_list));
	list_add_tail(&work->node, &drvdata->task_list);

	/* emulation of situation that device_run is called in IRQ context */
	if (current != drvdata->thread)
		wake_up(&drvdata->waitqueue);

	return 0;
}

static const struct m2m1shot2_devops m2m1shot2_testdev_ops = {
	.init_context = m2m1shot2_testdev_init_context,
	.free_context = m2m1shot2_testdev_free_context,
	.prepare_format = m2m1shot2_testdev_prepare_format,
	.prepare_source = m2m1shot2_testdev_prepare_source,
	.device_run = m2m1shot2_testdev_device_run,
};

static void m2m1shot2_testdev_process(struct m2m1shot2_testdev_drvdata *drvdata)
{
	struct exynos_iommu_owner *owner = drvdata->m21dev->dev->archdata.iommu;
	struct exynos_iovmm *vmm = owner->vmm_data;
	struct iommu_domain *domain = vmm->domain;
	struct m2m1shot2_context *ctx =
				m2m1shot2_current_context(drvdata->m21dev);
	dma_addr_t iova;
	char *addr;
	struct page *page;
	unsigned int i;
	off_t offset;
	size_t size, len;
	char val = 0;

	for (i = 0; i < ctx->num_sources; i++) {
		iova = m2m1shot2_src_dma_addr(ctx, i, 0);
		page = phys_to_page(iommu_iova_to_phys(domain, iova));
		addr = kmap(page) + offset_in_page(iova);
		val += addr[0];
		kunmap(page);
	}

	iova = m2m1shot2_dst_dma_addr(ctx, 0);
	offset = offset_in_page(iova);
	iova = iova & PAGE_MASK;
	size = ctx->target.fmt.fmt.width * ctx->target.fmt.fmt.height * 4;
	while (size > 0) {
		len = min(size, PAGE_SIZE - offset);
		size -= len;

		page = phys_to_page(iommu_iova_to_phys(domain, iova));
		addr = kmap(page) + offset;
		while (len-- > 0)
			addr[len] += val;
		kunmap(page);

		iova += PAGE_SIZE;
		offset = 0;
	}
}

static void m2m1shot2_testdev_irq(struct m2m1shot2_testdev_drvdata *drvdata)
{
	struct m2m1shot2_testdev_work *work;
	struct m2m1shot2_context *ctx;

	/* IRQ handler */

	work = list_first_entry(&drvdata->task_list,
				struct m2m1shot2_testdev_work, node);
	list_del(&work->node);
	BUG_ON(!list_empty(&drvdata->task_list));

	ctx = m2m1shot2_current_context(drvdata->m21dev);

	BUG_ON(!ctx);
	BUG_ON(ctx != work->ctx);

	m2m1shot2_finish_context(ctx, true);
}

/* emulates H/W's processing */
static int m2m1shot2_testdev_worker(void *data)
{
	struct m2m1shot2_testdev_drvdata *drvdata = data;

	while (true) {
		wait_event_freezable(drvdata->waitqueue,
				     !list_empty(&drvdata->task_list));

		BUG_ON(list_empty(&drvdata->task_list));

		m2m1shot2_testdev_process(drvdata);

		m2m1shot2_testdev_irq(drvdata);
	}

	return 0;

}
static struct platform_device m2m1shot2_testdev_pdev = {
	.name = "m2m1shot2_testdev",
};

static int m2m1shot2_init_iovmm(struct device *dev)
{
	struct exynos_iommu_owner *owner = kzalloc(sizeof(*owner), GFP_KERNEL);
		struct exynos_iommu_domain *priv;
	struct exynos_iovmm *vmm;

	if (!owner) {
		dev_err(dev, "%s: failed to allocate owner data\n", __func__);
		return -ENOMEM;
	}

	vmm = exynos_create_single_iovmm(dev_name(dev));
	if (IS_ERR(vmm)) {
		int ret = PTR_ERR(vmm);

		kfree(owner);
		return ret;
	}

	priv = (struct exynos_iommu_domain *)vmm->domain->priv;

	owner->vmm_data = vmm;
	spin_lock(&priv->lock);
	spin_unlock(&priv->lock);

	dev->archdata.iommu = owner;

	return 0;
}

static void m2m1shot2_destroy_iovmm(struct device *dev)
{
	pr_err("%s: nothing to do\n", __func__);
}

static int m2m1shot2_testdev_init(void)
{
	int ret = 0;
	struct m2m1shot2_device *m21dev;
	struct m2m1shot2_testdev_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	ret = platform_device_register(&m2m1shot2_testdev_pdev);
	if (ret) {
		pr_err("%s: Failed to register platform device\n", __func__);
		goto err_register;
	}

	m21dev = m2m1shot2_create_device(&m2m1shot2_testdev_pdev.dev,
					&m2m1shot2_testdev_ops, "testdev", -1,
					M2M1SHOT2_DEVATTR_COHERENT);
	if (IS_ERR(m21dev)) {
		pr_err("%s: Failed to create m2m1shot device\n", __func__);
		ret = PTR_ERR(m21dev);
		goto err_create;
	}

	ret = m2m1shot2_init_iovmm(&m2m1shot2_testdev_pdev.dev);
	if (ret) {
		pr_err("%s: Failed to initialize dummy iovmm\n", __func__);
		goto err_iovmm;
	}

	drvdata->thread = kthread_run(m2m1shot2_testdev_worker, drvdata,
				"%s", "m2m1shot_tesdev_worker");
	if (IS_ERR(drvdata->thread)) {
		pr_err("%s: Failed create worker thread\n", __func__);
		ret = PTR_ERR(drvdata->thread);
		goto err_worker;
	}

	drvdata->m21dev = m21dev;
	INIT_LIST_HEAD(&drvdata->task_list);
	init_waitqueue_head(&drvdata->waitqueue);

	dev_set_drvdata(&m2m1shot2_testdev_pdev.dev, drvdata);

	dev_info(&m2m1shot2_testdev_pdev.dev,
		"%s: m2m1shot2 test device successfully initialized\n",
		__func__);
	return 0;

err_worker:
	m2m1shot2_destroy_iovmm(&m2m1shot2_testdev_pdev.dev);
err_iovmm:
	m2m1shot2_destroy_device(m21dev);
err_create:
	platform_device_unregister(&m2m1shot2_testdev_pdev);
err_register:
	kfree(drvdata);

	pr_err("%s: Failed to initialize m2m1shot2 test device\n", __func__);
	return ret;
}
module_init(m2m1shot2_testdev_init);
