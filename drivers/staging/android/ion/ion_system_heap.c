/*
 * drivers/staging/android/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
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

#include <asm/compat.h>
#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <asm/tlbflush.h>
#include "ion.h"
#include "ion_priv.h"

#ifdef CONFIG_HIGHMEM
static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_NOWARN |
				    __GFP_NORETRY | __GFP_NO_KSWAPD) & ~__GFP_WAIT;
static gfp_t low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_NOWARN);
#else
static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				    __GFP_NORETRY | __GFP_NO_KSWAPD) & ~__GFP_WAIT;
static gfp_t low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN);
#endif

static const unsigned int orders[] = {8, 4, 0};
static const int num_orders = ARRAY_SIZE(orders);
static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static inline unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool **pools;
};

static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order)
{
	int idx = order_to_index(order);
	struct ion_page_pool *pool;
	struct page *page;

	if (!ion_buffer_cached(buffer))
		idx += num_orders;

	pool = heap->pools[idx];

	page = ion_page_pool_alloc(pool);
	if (!page) {
		/* try with alternative pool */
		if (ion_buffer_cached(buffer))
			pool = heap->pools[idx + num_orders];
		else
			pool = heap->pools[idx - num_orders];

		page = ion_page_pool_alloc(pool);
	}

	if (!page)
		page = ion_page_pool_alloc_pages(pool);

	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page)
{
	unsigned int order = compound_order(page);

	struct ion_page_pool *pool = heap->pools[order_to_index(order)];
	if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) {
		ion_page_pool_free_immediate(pool, page);
	} else {
		int uncached = ion_buffer_cached(buffer) ? 0 : 1;
		int idx = order_to_index(order) + (num_orders * uncached);
		struct ion_page_pool *pool = heap->pools[idx];

		ion_page_pool_free(pool, page);
	}
}


static struct page *alloc_largest_available(struct ion_system_heap *heap,
					    struct ion_buffer *buffer,
					    unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i]);
		if (!page)
			continue;

		return page;
	}

	return NULL;
}

#define should_flush_cache(page, buffer) (!ion_get_page_clean(page) &&	\
		(!ion_buffer_cached(buffer) || ion_buffer_sync_force(buffer)))

static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	bool all_pages_from_pool = true;

	if (align > PAGE_SIZE)
		return -EINVAL;

	if (size / PAGE_SIZE > totalram_pages / 2)
		return -ENOMEM;

	INIT_LIST_HEAD(&pages);
	while (size_remaining > 0) {
		page = alloc_largest_available(sys_heap, buffer, size_remaining,
						max_order);
		if (!page)
			goto free_pages;
		list_add_tail(&page->lru, &pages);
		size_remaining -= PAGE_SIZE << compound_order(page);
		max_order = compound_order(page);
		i++;
	}
	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto free_pages;

	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_table;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		unsigned int len = PAGE_SIZE << compound_order(page);
		sg_set_page(sg, page, len, 0);
		sg = sg_next(sg);
		if (should_flush_cache(page, buffer)) {
			all_pages_from_pool = false;
			if (!IS_ENABLED(CONFIG_HIGHMEM)) {
				__flush_dcache_area(page_address(page), len);
				if (!ion_buffer_cached(buffer))
					ion_set_page_clean(page);
			}
		}
		list_del(&page->lru);
	}

	if (all_pages_from_pool)
		ion_buffer_set_ready(buffer);

	buffer->priv_virt = table;
	return 0;

free_table:
	kfree(table);
free_pages:
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		list_del(&page->lru);
		buffer->private_flags |= ION_PRIV_FLAG_SHRINKER_FREE;
		free_buffer_page(sys_heap, buffer, page);
	}

	return -ENOMEM;
}

static void ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_system_heap *sys_heap = container_of(buffer->heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	int i;

	/* pages come from the page pools, zero them before returning
	   for security purposes (other allocations are zerod at alloc time */
	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE))
		ion_heap_buffer_zero(buffer);

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg));
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_system_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

static void ion_system_heap_unmap_dma(struct ion_heap *heap,
				      struct ion_buffer *buffer)
{
}

struct ion_system_heap_prealod_object {
	size_t len;
	unsigned int count;
};

struct ion_system_heap_preload_data {
	struct ion_system_heap *heap;
	unsigned int flags;
	unsigned int count;
	struct ion_system_heap_prealod_object objs[0];
};

#define ION_FLAG_CACHED_POOL (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC)
static int ion_system_heap_preloader(void *p)
{
	struct ion_system_heap_preload_data *data = p;
	long num_1m = 0, num_64k = 0, num_4k = 0;
	long loaded_pages;
	int idx = 0, alt_idx = num_orders;
	int i;

	for (i = 0; i < data->count; i++) {
		size_t len = data->objs[i].len / PAGE_SIZE;

		num_4k += (len & 0xF) * data->objs[i].count;
		len >>= 4;
		num_64k += (len & 0xF) * data->objs[i].count;
		len >>= 4;
		num_1m += len * data->objs[i].count;
	}

	if ((num_4k + num_64k * 16 + num_1m * 256) > (totalram_pages / 4)) {
		/* too many pages requested */
		long max_pages = totalram_pages / 4;
		long exceeded_pages = num_4k + num_64k * 16 + num_1m * 256;
		exceeded_pages -= max_pages;

		if (num_4k < exceeded_pages) {
			num_4k = 0;
			exceeded_pages -= num_4k;
		} else {
			num_4k -= exceeded_pages;
			exceeded_pages = 0;
		}

		exceeded_pages /= 16;

		if (num_64k < exceeded_pages) {
			num_64k = 0;
			exceeded_pages -= num_64k;
		} else {
			num_64k -= exceeded_pages;
			exceeded_pages = 0;
		}

		exceeded_pages /= 16;

		if (num_1m < exceeded_pages) {
			num_1m = 0;
			exceeded_pages -= num_1m;
		} else {
			num_1m -= exceeded_pages;
			exceeded_pages = 0;
		}
	}

	ion_heap_freelist_drain(&data->heap->heap, 0);

	if ((data->flags & ION_FLAG_CACHED_POOL) != ION_FLAG_CACHED_POOL) {
		idx = num_orders; /* non cached */
		alt_idx = 0;
	}

	ion_page_pool_preload_prepare(data->heap->pools[idx + 2], num_4k);
	/* populates order-0 pages first to invoke page reclamation */
	loaded_pages = ion_page_pool_preload(data->heap->pools[idx + 2],
			data->heap->pools[alt_idx + 2], data->flags, num_4k);
	if (loaded_pages < num_4k)
		/* kernel is really unable to allocate page */
		goto finish;

	loaded_pages = ion_page_pool_preload(data->heap->pools[idx + 1],
			data->heap->pools[alt_idx + 1], data->flags, num_64k);
	num_64k -= loaded_pages;

	loaded_pages = ion_page_pool_preload(data->heap->pools[idx],
			data->heap->pools[alt_idx], data->flags, num_1m);
	num_1m -= loaded_pages;

	if (num_1m || num_64k) {
		/* try again with lower order free list */
		loaded_pages = ion_page_pool_preload(data->heap->pools[idx + 1],
				data->heap->pools[alt_idx + 1], data->flags,
				num_64k + num_1m * 16);
		if (num_1m > (loaded_pages / 16)) {
			num_1m -= loaded_pages / 16;
			loaded_pages &= 0xF; /* remiander of loaded_pages/16 */
		} else {
			loaded_pages -= num_1m * 16;
			num_1m = 0;
		}
		num_64k -= loaded_pages;
		/*
		 * half of order-8 pages won't be tried with order-0 free list
		 * for memory utilization because populating too much low order
		 * pages causes memory fragmentation seriously.
		 */
		num_64k += num_1m * 8;
		num_4k += num_64k * 16;

		loaded_pages = ion_page_pool_preload(data->heap->pools[idx + 2],
				data->heap->pools[alt_idx + 2], data->flags,
				num_4k + num_64k * 16);

		if (((num_4k - loaded_pages) + num_1m) > 0)
			pr_info("%s: %ld pages are not populated to the pool\n",
				__func__, loaded_pages + num_1m * 256);
	}

finish:
	kfree(data); /* allocated in ion_system_heap_preload_allocate() */

	if (!signal_pending(current))
		do_exit(0);
	return 0;
}

static void ion_system_heap_preload_allocate(struct ion_heap *heap,
					     unsigned int flags,
					     unsigned int count,
					     struct ion_preload_object obj[])
{
	struct sched_param param = { .sched_priority = 0 };
	struct ion_system_heap_preload_data *data;
	struct task_struct *ret;

	data = kmalloc(sizeof(*data) + sizeof(data->objs[0]) * count,
			GFP_KERNEL);
	if (!data) {
		pr_info("%s: preload request failed due to nomem\n", __func__);
		return;
	}

	data->heap = container_of(heap, struct ion_system_heap, heap);
	data->flags = flags;
	data->count = count;
	for (count = 0; count < data->count; count++) {
		data->objs[count].count = obj[count].count;
		data->objs[count].len = obj[count].len;
	}

	ret = kthread_run(ion_system_heap_preloader, data,
				"ion_system_heap_preloader_%d", count);
	if (IS_ERR(ret)) {
		pr_info("%s: failed to create preload thread(%ld)\n",
				__func__, PTR_ERR(ret));
	} else {
		sched_setscheduler(ret, SCHED_NORMAL, &param);
	}
}

static int ion_system_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
					int nr_to_scan)
{
	struct ion_system_heap *sys_heap;
	int nr_total = 0;
	int nocached, i;

	sys_heap = container_of(heap, struct ion_system_heap, heap);

	/* cached pools first, low order pages first */
	for (nocached = 0; nocached < 2; nocached++) {
		for (i = 0; i < num_orders; i++) {
			struct ion_page_pool *pool =
				sys_heap->pools[i + num_orders * nocached];
			int nr_pages = ion_page_pool_shrink(pool, gfp_mask, 0);

			if (nr_to_scan > 0) {
				int to_scan = nr_to_scan;
				nr_to_scan -= nr_pages;
				nr_pages = ion_page_pool_shrink(
						pool, gfp_mask, to_scan);
			}
			nr_total += nr_pages;
		}
	}

	return nr_total;
}

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.shrink = ion_system_heap_shrink,
	.preload = ion_system_heap_preload_allocate,
};

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		seq_printf(s, "%d order %u highmem pages in cached pool = %lu total\n",
			   pool->high_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in cached pool = %lu total\n",
			   pool->low_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->low_count);
	}

	for (i = num_orders; i < (num_orders * 2); i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		seq_printf(s, "%d order %u highmem pages in uncached pool = %lu total\n",
			   pool->high_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in uncached pool = %lu total\n",
			   pool->low_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->low_count);
	}

	return 0;
}

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;
	int i;

	heap = kzalloc(sizeof(struct ion_system_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	heap->pools = kzalloc(sizeof(struct ion_page_pool *) * num_orders * 2,
			      GFP_KERNEL);
	if (!heap->pools)
		goto free_heap;
	for (i = 0; i < num_orders * 2; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i % num_orders] > 0)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i % num_orders]);
		if (!pool)
			goto destroy_pools;
		pool->cached = i < num_orders ? true : false;
		heap->pools[i] = pool;
	}

	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;

destroy_pools:
	while (i--)
		ion_page_pool_destroy(heap->pools[i]);
	kfree(heap->pools);
free_heap:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

void ion_system_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	for (i = 0; i < num_orders * 2; i++)
		ion_page_pool_destroy(sys_heap->pools[i]);
	kfree(sys_heap->pools);
	kfree(sys_heap);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	int order = get_order(len);
	struct page *page;
	struct sg_table *table;
	unsigned long i;
	int ret;

	if (align > (PAGE_SIZE << order))
		return -EINVAL;

	page = alloc_pages(low_order_gfp_flags, order);
	if (!page)
		return -ENOMEM;

	split_page(page, order);

	len = PAGE_ALIGN(len);
	for (i = len >> PAGE_SHIFT; i < (1 << order); i++)
		__free_page(page + i);

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto free_pages;
	}

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_table;

	sg_set_page(table->sgl, page, len, 0);

	buffer->priv_virt = table;

	ion_pages_sync_for_device(NULL, page, len, DMA_BIDIRECTIONAL);

	return 0;

free_table:
	kfree(table);
free_pages:
	for (i = 0; i < len >> PAGE_SHIFT; i++)
		__free_page(page + i);

	return ret;
}

static void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	unsigned long pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;
	unsigned long i;

	for (i = 0; i < pages; i++)
		__free_page(page + i);
	sg_free_table(table);
	kfree(table);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       ion_phys_addr_t *addr, size_t *len)
{
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	*addr = page_to_phys(page);
	*len = buffer->size;
	return 0;
}

static struct sg_table *ion_system_contig_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

static void ion_system_contig_heap_unmap_dma(struct ion_heap *heap,
					     struct ion_buffer *buffer)
{
}

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.phys = ion_system_contig_heap_phys,
	.map_dma = ion_system_contig_heap_map_dma,
	.unmap_dma = ion_system_contig_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}
