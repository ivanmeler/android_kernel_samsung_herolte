/*

 * drivers/staging/android/ion/ion.c
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

#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/idr.h>
#include <linux/exynos_iovmm.h>
#include <linux/exynos_ion.h>
#include <linux/highmem.h>

#include "ion.h"
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#define CREATE_TRACE_POINTS
#include "ion_priv.h"
#include "compat_ion.h"

/**
 * struct ion_device - the metadata of the ion device node
 * @dev:		the actual misc device
 * @buffers:		an rb tree of all the existing buffers
 * @buffer_lock:	lock protecting the tree of buffers
 * @lock:		rwsem protecting the tree of heaps and clients
 * @heaps:		list of all the heaps in the system
 * @user_clients:	list of all the clients created from userspace
 */
struct ion_device {
	struct miscdevice dev;
	struct rb_root buffers;
	struct mutex buffer_lock;
	struct rw_semaphore lock;
	struct plist_head heaps;
	long (*custom_ioctl)(struct ion_client *client, unsigned int cmd,
			     unsigned long arg);
	struct rb_root clients;
	struct dentry *debug_root;
	struct dentry *heaps_debug_root;
	struct dentry *clients_debug_root;
	struct semaphore vm_sem;
	atomic_t page_idx;
	struct vm_struct *reserved_vm_area;
	pte_t **pte;

#ifdef CONFIG_ION_EXYNOS_STAT_LOG
	/* event log */
	struct dentry *buffer_debug_file;
	struct dentry *event_debug_file;
	struct ion_eventlog eventlog[ION_EVENT_LOG_MAX];
	atomic_t event_idx;
#endif
};

/**
 * struct ion_client - a process/hw block local address space
 * @node:		node in the tree of all clients
 * @dev:		backpointer to ion device
 * @handles:		an rb tree of all the handles in this client
 * @idr:		an idr space for allocating handle ids
 * @lock:		lock protecting the tree of handles
 * @name:		used for debugging
 * @display_name:	used for debugging (unique version of @name)
 * @display_serial:	used for debugging (to make display_name unique)
 * @task:		used for debugging
 *
 * A client represents a list of buffers this client may access.
 * The mutex stored here is used to protect both handles tree
 * as well as the handles themselves, and should be held while modifying either.
 */
struct ion_client {
	struct rb_node node;
	struct ion_device *dev;
	struct rb_root handles;
	struct idr idr;
	struct mutex lock;
	const char *name;
	char *display_name;
	int display_serial;
	struct task_struct *task;
	pid_t pid;
	struct dentry *debug_root;
};

/**
 * ion_handle - a client local reference to a buffer
 * @ref:		reference count
 * @client:		back pointer to the client the buffer resides in
 * @buffer:		pointer to the buffer
 * @node:		node in the client's handle rbtree
 * @kmap_cnt:		count of times this client has mapped to kernel
 * @id:			client-unique id allocated by client->idr
 *
 * Modifications to node, map_cnt or mapping should be protected by the
 * lock in the client.  Other fields are never changed after initialization.
 */
struct ion_handle {
	struct kref ref;
	unsigned int user_ref_count;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct rb_node node;
	unsigned int kmap_cnt;
	int id;
};

struct ion_device *g_idev;

static inline struct page *ion_buffer_page(struct page *page)
{
	return (struct page *)((unsigned long)page & ~(1UL));
}

static inline bool ion_buffer_page_is_dirty(struct page *page)
{
	return !!((unsigned long)page & 1UL);
}

static inline void ion_buffer_page_dirty(struct page **page)
{
	*page = (struct page *)((unsigned long)(*page) | 1UL);
}

static inline void ion_buffer_page_clean(struct page **page)
{
	*page = (struct page *)((unsigned long)(*page) & ~(1UL));
}

void ion_debug_heap_usage_show(struct ion_heap *heap)
{
	struct scatterlist *sg;
	struct sg_table *table;
	struct rb_node *n;
	struct page *page;
	struct ion_device *dev = heap->dev;
	int i;
	ion_phys_addr_t paddr;

	/* show the usage for only contiguous buffer */
	if ((heap->type != ION_HEAP_TYPE_CARVEOUT)
			&& (heap->type != ION_HEAP_TYPE_DMA))
		return;

	pr_err("[HEAP %16s (id %4d) DETAIL USAGE]\n", heap->name, heap->id);

	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buffer = rb_entry(n, struct ion_buffer,
						     node);
		if (buffer->heap->id != heap->id)
			continue;
		table = buffer->sg_table;
		for_each_sg(table->sgl, sg, table->nents, i) {
			page = sg_page(sg);
			paddr = PFN_PHYS(page_to_pfn(page));
			pr_err("[%16lx--%16lx] %16zu\n",
				paddr, paddr + sg->length, buffer->size);
		}
	}
	mutex_unlock(&dev->buffer_lock);
}

#ifdef CONFIG_ION_EXYNOS_STAT_LOG
static inline void ION_EVENT_ALLOC(struct ion_buffer *buffer, ktime_t begin)
{
	struct ion_device *dev = buffer->dev;
	int idx = atomic_inc_return(&dev->event_idx);
	struct ion_eventlog *log = &dev->eventlog[idx % ION_EVENT_LOG_MAX];
	struct ion_event_alloc *data = &log->data.alloc;

	log->type = ION_EVENT_TYPE_ALLOC;
	log->begin = begin;
	log->done = ktime_get();
	data->id = buffer;
	data->heap = buffer->heap;
	data->size = buffer->size;
	data->flags = buffer->flags;
}

static inline void ION_EVENT_FREE(struct ion_buffer *buffer, ktime_t begin)
{
	struct ion_device *dev = buffer->dev;
	int idx = atomic_inc_return(&dev->event_idx) % ION_EVENT_LOG_MAX;
	struct ion_eventlog *log = &dev->eventlog[idx];
	struct ion_event_free *data = &log->data.free;

	log->type = ION_EVENT_TYPE_FREE;
	log->begin = begin;
	log->done = ktime_get();
	data->id = buffer;
	data->heap = buffer->heap;
	data->size = buffer->size;
	data->shrinker = (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE);
}

static inline void ION_EVENT_MMAP(struct ion_buffer *buffer, ktime_t begin)
{
	struct ion_device *dev = buffer->dev;
	int idx = atomic_inc_return(&dev->event_idx) % ION_EVENT_LOG_MAX;
	struct ion_eventlog *log = &dev->eventlog[idx];
	struct ion_event_mmap *data = &log->data.mmap;

	log->type = ION_EVENT_TYPE_MMAP;
	log->begin = begin;
	log->done = ktime_get();
	data->id = buffer;
	data->heap = buffer->heap;
	data->size = buffer->size;
}

void ION_EVENT_SHRINK(struct ion_device *dev, size_t size)
{
	int idx = atomic_inc_return(&dev->event_idx) % ION_EVENT_LOG_MAX;
	struct ion_eventlog *log = &dev->eventlog[idx];

	log->type = ION_EVENT_TYPE_SHRINK;
	log->begin = ktime_get();
	log->done = ktime_set(0, 0);
	log->data.shrink.size = size;
}

void ION_EVENT_CLEAR(struct ion_buffer *buffer, ktime_t begin)
{
	struct ion_device *dev = buffer->dev;
	int idx = atomic_inc_return(&dev->event_idx) % ION_EVENT_LOG_MAX;
	struct ion_eventlog *log = &dev->eventlog[idx];
	struct ion_event_clear *data = &log->data.clear;

	log->type = ION_EVENT_TYPE_CLEAR;
	log->begin = begin;
	log->done = ktime_get();
	data->id = buffer;
	data->heap = buffer->heap;
	data->size = buffer->size;
	data->flags = buffer->flags;
}

static struct ion_task *ion_buffer_task_lookup(struct ion_buffer *buffer,
							struct device *master)
{
	bool found = false;
	struct ion_task *task;

	list_for_each_entry(task, &buffer->master_list, list) {
		if (task->master == master) {
			found = true;
			break;
		}
	}

	return found ? task : NULL;
}

static void ion_buffer_set_task_info(struct ion_buffer *buffer)
{
	INIT_LIST_HEAD(&buffer->master_list);
	get_task_comm(buffer->task_comm, current->group_leader);
	get_task_comm(buffer->thread_comm, current);
	buffer->pid = task_pid_nr(current->group_leader);
	buffer->tid = task_pid_nr(current);
}

static void ion_buffer_task_add(struct ion_buffer *buffer,
					struct device *master)
{
	struct ion_task *task;

	task = ion_buffer_task_lookup(buffer, master);
	if (!task) {
		task = kzalloc(sizeof(*task), GFP_KERNEL);
		if (task) {
			task->master = master;
			kref_init(&task->ref);
			list_add_tail(&task->list, &buffer->master_list);
		}
	} else {
		kref_get(&task->ref);
	}
}

static void ion_buffer_task_add_lock(struct ion_buffer *buffer,
					struct device *master)
{
	mutex_lock(&buffer->lock);
	ion_buffer_task_add(buffer, master);
	mutex_unlock(&buffer->lock);
}

static void __ion_buffer_task_remove(struct kref *kref)
{
	struct ion_task *task = container_of(kref, struct ion_task, ref);

	list_del(&task->list);
	kfree(task);
}

static void ion_buffer_task_remove(struct ion_buffer *buffer,
					struct device *master)
{
	struct ion_task *task, *tmp;

	list_for_each_entry_safe(task, tmp, &buffer->master_list, list) {
		if (task->master == master) {
			kref_put(&task->ref, __ion_buffer_task_remove);
			break;
		}
	}
}

static void ion_buffer_task_remove_lock(struct ion_buffer *buffer,
					struct device *master)
{
	mutex_lock(&buffer->lock);
	ion_buffer_task_remove(buffer, master);
	mutex_unlock(&buffer->lock);
}

static void ion_buffer_task_remove_all(struct ion_buffer *buffer)
{
	struct ion_task *task, *tmp;

	mutex_lock(&buffer->lock);
	list_for_each_entry_safe(task, tmp, &buffer->master_list, list) {
		list_del(&task->list);
		kfree(task);
	}
	mutex_unlock(&buffer->lock);
}
#else
#define ION_EVENT_ALLOC(buffer, begin)			do { } while (0)
#define ION_EVENT_FREE(buffer, begin)			do { } while (0)
#define ION_EVENT_MMAP(buffer, begin)			do { } while (0)
#define ion_buffer_set_task_info(buffer)		do { } while (0)
#define ion_buffer_task_add(buffer, master)		do { } while (0)
#define ion_buffer_task_add_lock(buffer, master)	do { } while (0)
#define ion_buffer_task_remove(buffer, master)		do { } while (0)
#define ion_buffer_task_remove_lock(buffer, master)	do { } while (0)
#define ion_buffer_task_remove_all(buffer)		do { } while (0)
#endif

/* this function should only be called while dev->lock is held */
static void ion_buffer_add(struct ion_device *dev,
			   struct ion_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct ion_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_buffer, node);

		if (buffer < entry) {
			p = &(*p)->rb_left;
		} else if (buffer > entry) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);

	ion_buffer_set_task_info(buffer);
	ion_buffer_task_add(buffer, dev->dev.this_device);
}

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
				     struct ion_device *dev,
				     unsigned long len,
				     unsigned long align,
				     unsigned long flags)
{
	struct ion_buffer *buffer;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, ret;

	buffer = kzalloc(sizeof(struct ion_buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	buffer->flags = flags;
	buffer->size = len;
	kref_init(&buffer->ref);

	ret = heap->ops->allocate(heap, buffer, len, align, flags);

	if (ret) {
		if (!(heap->flags & ION_HEAP_FLAG_DEFER_FREE))
			goto err2;

		ion_heap_freelist_drain(heap, 0);
		ret = heap->ops->allocate(heap, buffer, len, align,
					  flags);
		if (ret)
			goto err2;
	}

	buffer->dev = dev;

	table = heap->ops->map_dma(heap, buffer);
	if (WARN_ONCE(table == NULL,
			"heap->ops->map_dma should return ERR_PTR on error"))
		table = ERR_PTR(-EINVAL);
	if (IS_ERR(table)) {
		ret = -EINVAL;
		goto err1;
	}

	buffer->sg_table = table;
	if (ion_buffer_fault_user_mappings(buffer)) {
		int num_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
		struct scatterlist *sg;
		int i, j, k = 0;

		buffer->pages = vmalloc(sizeof(struct page *) * num_pages);
		if (!buffer->pages) {
			ret = -ENOMEM;
			goto err;
		}

		for_each_sg(table->sgl, sg, table->nents, i) {
			struct page *page = sg_page(sg);

			for (j = 0; j < sg->length / PAGE_SIZE; j++)
				buffer->pages[k++] = page++;
		}
	}

	buffer->dev = dev;
	buffer->size = len;
	INIT_LIST_HEAD(&buffer->vmas);
	INIT_LIST_HEAD(&buffer->iovas);
	mutex_init(&buffer->lock);
	/* this will set up dma addresses for the sglist -- it is not
	   technically correct as per the dma api -- a specific
	   device isn't really taking ownership here.  However, in practice on
	   our systems the only dma_address space is physical addresses.
	   Additionally, we can't afford the overhead of invalidating every
	   allocation via dma_map_sg. The implicit contract here is that
	   memory coming from the heaps is ready for dma, ie if it has a
	   cached mapping that mapping has been invalidated */
	for_each_sg(buffer->sg_table->sgl, sg, buffer->sg_table->nents, i)
		sg_dma_address(sg) = sg_phys(sg);
	mutex_lock(&dev->buffer_lock);
	ion_buffer_add(dev, buffer);
	mutex_unlock(&dev->buffer_lock);
	return buffer;

err:
	heap->ops->unmap_dma(heap, buffer);
err1:
	heap->ops->free(buffer);
err2:
	kfree(buffer);
	return ERR_PTR(ret);
}

void ion_buffer_destroy(struct ion_buffer *buffer)
{
	struct ion_iovm_map *iovm_map;
	struct ion_iovm_map *tmp;

	ION_EVENT_BEGIN();
	trace_ion_free_start((unsigned long) buffer, buffer->size,
				buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE);

	if (WARN_ON(buffer->kmap_cnt > 0))
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);

	list_for_each_entry_safe(iovm_map, tmp, &buffer->iovas, list) {
		iovmm_unmap(iovm_map->dev, iovm_map->iova);
		list_del(&iovm_map->list);
		kfree(iovm_map);
	}

	buffer->heap->ops->unmap_dma(buffer->heap, buffer);
	buffer->heap->ops->free(buffer);
	if (buffer->pages)
		vfree(buffer->pages);

	ion_buffer_task_remove_all(buffer);
	ION_EVENT_FREE(buffer, ION_EVENT_DONE());
	trace_ion_free_end((unsigned long) buffer, buffer->size,
				buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE);
	kfree(buffer);
}

static void _ion_buffer_destroy(struct kref *kref)
{
	struct ion_buffer *buffer = container_of(kref, struct ion_buffer, ref);
	struct ion_heap *heap = buffer->heap;
	struct ion_device *dev = buffer->dev;

	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->buffer_lock);

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_freelist_add(heap, buffer);
	else
		ion_buffer_destroy(buffer);
}

static void ion_buffer_get(struct ion_buffer *buffer)
{
	kref_get(&buffer->ref);
}

static int ion_buffer_put(struct ion_buffer *buffer)
{
	return kref_put(&buffer->ref, _ion_buffer_destroy);
}

static void ion_buffer_add_to_handle(struct ion_buffer *buffer)
{
	mutex_lock(&buffer->lock);
	buffer->handle_count++;
	mutex_unlock(&buffer->lock);
}

static void ion_buffer_remove_from_handle(struct ion_buffer *buffer)
{
	/*
	 * when a buffer is removed from a handle, if it is not in
	 * any other handles, copy the taskcomm and the pid of the
	 * process it's being removed from into the buffer.  At this
	 * point there will be no way to track what processes this buffer is
	 * being used by, it only exists as a dma_buf file descriptor.
	 * The taskcomm and pid can provide a debug hint as to where this fd
	 * is in the system
	 */
	mutex_lock(&buffer->lock);
	buffer->handle_count--;
	BUG_ON(buffer->handle_count < 0);
	if (!buffer->handle_count) {
		struct task_struct *task;

		task = current->group_leader;
		get_task_comm(buffer->task_comm, task);
		buffer->pid = task_pid_nr(task);
	}
	mutex_unlock(&buffer->lock);
}

static bool ion_handle_validate(struct ion_client *client,
				struct ion_handle *handle)
{
	WARN_ON(!mutex_is_locked(&client->lock));
	return idr_find(&client->idr, handle->id) == handle;
}

static struct ion_handle *ion_handle_create(struct ion_client *client,
				     struct ion_buffer *buffer)
{
	struct ion_handle *handle;

	handle = kzalloc(sizeof(struct ion_handle), GFP_KERNEL);
	if (!handle)
		return ERR_PTR(-ENOMEM);
	kref_init(&handle->ref);
	RB_CLEAR_NODE(&handle->node);
	handle->client = client;
	ion_buffer_get(buffer);
	ion_buffer_add_to_handle(buffer);
	handle->buffer = buffer;

	return handle;
}

static void ion_handle_kmap_put(struct ion_handle *);

static void ion_handle_destroy(struct kref *kref)
{
	struct ion_handle *handle = container_of(kref, struct ion_handle, ref);
	struct ion_client *client = handle->client;
	struct ion_buffer *buffer = handle->buffer;

	mutex_lock(&buffer->lock);
	while (handle->kmap_cnt)
		ion_handle_kmap_put(handle);
	mutex_unlock(&buffer->lock);

	idr_remove(&client->idr, handle->id);
	if (!RB_EMPTY_NODE(&handle->node))
		rb_erase(&handle->node, &client->handles);

	ion_buffer_remove_from_handle(buffer);
	ion_buffer_put(buffer);

	kfree(handle);
}

struct ion_buffer *ion_handle_buffer(struct ion_handle *handle)
{
	return handle->buffer;
}

static void ion_handle_get(struct ion_handle *handle)
{
	kref_get(&handle->ref);
}

/* Must hold the client lock */
static struct ion_handle *ion_handle_get_check_overflow(
					struct ion_handle *handle)
{
	if (atomic_read(&handle->ref.refcount) + 1 == 0)
		return ERR_PTR(-EOVERFLOW);
	ion_handle_get(handle);
	return handle;
}

static int ion_handle_put_nolock(struct ion_handle *handle)
{
	int ret;

	ret = kref_put(&handle->ref, ion_handle_destroy);

	return ret;
}

int ion_handle_put(struct ion_client *client, struct ion_handle *handle)
{
	bool valid_handle;
	int ret;

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);

	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to free.\n", __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}
	ret = ion_handle_put_nolock(handle);
	mutex_unlock(&client->lock);

	return ret;
}

/* Must hold the client lock */
static void user_ion_handle_get(struct ion_handle *handle)
{
	if (handle->user_ref_count++ == 0) {
		kref_get(&handle->ref);
	}
}

/* Must hold the client lock */
static struct ion_handle* user_ion_handle_get_check_overflow(struct ion_handle *handle)
{
	if (handle->user_ref_count + 1 == 0)
		return ERR_PTR(-EOVERFLOW);
	user_ion_handle_get(handle);
	return handle;
}

/* passes a kref to the user ref count.
 * We know we're holding a kref to the object before and
 * after this call, so no need to reverify handle. */
static struct ion_handle* pass_to_user(struct ion_handle *handle)
{
	struct ion_client *client = handle->client;
	struct ion_handle *ret;

	mutex_lock(&client->lock);
	ret = user_ion_handle_get_check_overflow(handle);
	ion_handle_put_nolock(handle);
	mutex_unlock(&client->lock);
	return ret;
}

/* Must hold the client lock */
static int user_ion_handle_put_nolock(struct ion_handle *handle)
{
	int ret = 0;

	if (--handle->user_ref_count == 0) {
		ret = ion_handle_put_nolock(handle);
	}

	return ret;
}

static struct ion_handle *ion_handle_lookup(struct ion_client *client,
					    struct ion_buffer *buffer)
{
	struct rb_node *n = client->handles.rb_node;

	while (n) {
		struct ion_handle *entry = rb_entry(n, struct ion_handle, node);

		if (buffer < entry->buffer)
			n = n->rb_left;
		else if (buffer > entry->buffer)
			n = n->rb_right;
		else
			return entry;
	}
	return ERR_PTR(-EINVAL);
}

static struct ion_handle *ion_handle_get_by_id_nolock(struct ion_client *client,
						int id)
{
	struct ion_handle *handle;

	handle = idr_find(&client->idr, id);
	if (handle)
		return ion_handle_get_check_overflow(handle);

	return ERR_PTR(-EINVAL);
}

struct ion_handle *ion_handle_get_by_id(struct ion_client *client,
						int id)
{
	struct ion_handle *handle;

	mutex_lock(&client->lock);
	handle = ion_handle_get_by_id_nolock(client, id);
	mutex_unlock(&client->lock);

	return handle;
}

static int ion_handle_add(struct ion_client *client, struct ion_handle *handle)
{
	int id;
	struct rb_node **p = &client->handles.rb_node;
	struct rb_node *parent = NULL;
	struct ion_handle *entry;

	id = idr_alloc(&client->idr, handle, 1, 0, GFP_KERNEL);
	if (id < 0) {
		pr_err("%s: Fail to get bad id (ret %d)\n", __func__, id);
		return id;
	}

	handle->id = id;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_handle, node);

		if (handle->buffer < entry->buffer)
			p = &(*p)->rb_left;
		else if (handle->buffer > entry->buffer)
			p = &(*p)->rb_right;
		else
			WARN(1, "%s: buffer already found.", __func__);
	}

	rb_link_node(&handle->node, parent, p);
	rb_insert_color(&handle->node, &client->handles);

	return 0;
}

unsigned int ion_parse_heap_id(unsigned int heap_id_mask, unsigned int flags);

static struct ion_handle *__ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int heap_id_mask,
			     unsigned int flags, bool grab_handle)
{
	struct ion_handle *handle;
	struct ion_device *dev = client->dev;
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;
	int ret;

	ION_EVENT_BEGIN();
	trace_ion_alloc_start(client->name, 0, len, align, heap_id_mask, flags);

	pr_debug("%s: len %zu align %zu heap_id_mask %u flags %x\n", __func__,
		 len, align, heap_id_mask, flags);
	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);
	if (WARN_ON(!len)) {
		trace_ion_alloc_fail(client->name, EINVAL, len,
				align, heap_id_mask, flags);
		return ERR_PTR(-EINVAL);
	}

	heap_id_mask = ion_parse_heap_id(heap_id_mask, flags);
	if (heap_id_mask == 0) {
		trace_ion_alloc_fail(client->name, EINVAL, len,
				align, heap_id_mask, flags);
		return ERR_PTR(-EINVAL);
	}

	down_read(&dev->lock);
	plist_for_each_entry(heap, &dev->heaps, node) {
		/* if the caller didn't specify this heap id */
		if (!((1 << heap->id) & heap_id_mask))
			continue;
		buffer = ion_buffer_create(heap, dev, len, align, flags);
		if (!IS_ERR(buffer))
			break;
	}
	up_read(&dev->lock);

	if (buffer == NULL) {
		trace_ion_alloc_fail(client->name, ENODEV, len,
				align, heap_id_mask, flags);
		return ERR_PTR(-ENODEV);
	}

	if (IS_ERR(buffer)) {
		trace_ion_alloc_fail(client->name, PTR_ERR(buffer),
					len, align, heap_id_mask, flags);
		return ERR_CAST(buffer);
	}

	handle = ion_handle_create(client, buffer);

	/*
	 * ion_buffer_create will create a buffer with a ref_cnt of 1,
	 * and ion_handle_create will take a second reference, drop one here
	 */
	ion_buffer_put(buffer);

	if (IS_ERR(handle)) {
		trace_ion_alloc_fail(client->name, (unsigned long) buffer,
					len, align, heap_id_mask, flags);
		return handle;
	}

	mutex_lock(&client->lock);
	if (grab_handle)
		ion_handle_get(handle);
	ret = ion_handle_add(client, handle);
	mutex_unlock(&client->lock);
	if (ret) {
		ion_handle_put(client, handle);
		handle = ERR_PTR(ret);
		trace_ion_alloc_fail(client->name, (unsigned long ) buffer,
					len, align, heap_id_mask, flags);
	}

	ION_EVENT_ALLOC(buffer, ION_EVENT_DONE());
	trace_ion_alloc_end(client->name, (unsigned long) buffer,
					len, align, heap_id_mask, flags);

	return handle;
}

struct ion_handle *ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int heap_id_mask,
			     unsigned int flags)
{
	return __ion_alloc(client, len, align, heap_id_mask, flags, false);
}
EXPORT_SYMBOL(ion_alloc);

static void ion_free_nolock(struct ion_client *client, struct ion_handle *handle)
{
	bool valid_handle;

	BUG_ON(client != handle->client);

	valid_handle = ion_handle_validate(client, handle);

	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to free.\n", __func__);
		return;
	}
	ion_handle_put_nolock(handle);
}

static void user_ion_free_nolock(struct ion_client *client, struct ion_handle *handle)
{
	bool valid_handle;

	BUG_ON(client != handle->client);

	valid_handle = ion_handle_validate(client, handle);
	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to free.\n", __func__);
		return;
	}
	if (!handle->user_ref_count > 0) {
		WARN(1, "%s: User does not have access!\n", __func__);
		return;
	}
	user_ion_handle_put_nolock(handle);
}

void ion_free(struct ion_client *client, struct ion_handle *handle)
{
	BUG_ON(client != handle->client);

	mutex_lock(&client->lock);
	ion_free_nolock(client, handle);
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(ion_free);

int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len)
{
	struct ion_buffer *buffer;
	int ret;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		mutex_unlock(&client->lock);
		return -EINVAL;
	}

	buffer = handle->buffer;

	if (!buffer->heap->ops->phys) {
		pr_err("%s: ion_phys is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return -ENODEV;
	}
	mutex_unlock(&client->lock);
	ret = buffer->heap->ops->phys(buffer->heap, buffer, addr, len);
	return ret;
}
EXPORT_SYMBOL(ion_phys);

static void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = buffer->heap->ops->map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(vaddr == NULL,
			"heap->ops->map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;

	ion_buffer_make_ready(buffer);

	return vaddr;
}

static void *ion_handle_kmap_get(struct ion_handle *handle)
{
	struct ion_buffer *buffer = handle->buffer;
	void *vaddr;

	if (handle->kmap_cnt) {
		handle->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = ion_buffer_kmap_get(buffer);
	if (IS_ERR(vaddr))
		return vaddr;
	handle->kmap_cnt++;
	return vaddr;
}

static void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

static void ion_handle_kmap_put(struct ion_handle *handle)
{
	struct ion_buffer *buffer = handle->buffer;

	if (!handle->kmap_cnt) {
		WARN(1, "%s: Double unmap detected! bailing...\n", __func__);
		return;
	}
	handle->kmap_cnt--;
	if (!handle->kmap_cnt)
		ion_buffer_kmap_put(buffer);
}

void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	void *vaddr;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_kernel.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}

	buffer = handle->buffer;

	if (!handle->buffer->heap->ops->map_kernel) {
		pr_err("%s: map_kernel is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-ENODEV);
	}

	mutex_lock(&buffer->lock);
	vaddr = ion_handle_kmap_get(handle);
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return vaddr;
}
EXPORT_SYMBOL(ion_map_kernel);

void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	ion_handle_kmap_put(handle);
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(ion_unmap_kernel);

static int ion_debug_client_show(struct seq_file *s, void *unused)
{
	struct ion_client *client = s->private;
	struct rb_node *n;
	size_t sizes[ION_NUM_HEAP_IDS] = {0};
	size_t sizes_pss[ION_NUM_HEAP_IDS] = {0};
	const char *names[ION_NUM_HEAP_IDS] = {NULL};
	int i;

	down_read(&g_idev->lock);

	/* check validity of the client */
	for (n = rb_first(&g_idev->clients); n; n = rb_next(n)) {
		struct ion_client *c = rb_entry(n, struct ion_client, node);
		if (client == c)
			break;
	}

	if (IS_ERR_OR_NULL(n)) {
		pr_err("%s: invalid client %p\n", __func__, client);
		up_read(&g_idev->lock);
		return -EINVAL;
	}

	seq_printf(s, "%16.s %4.s %16.s %4.s %10.s %8.s %9.s\n",
		   "task", "pid", "thread", "tid", "size", "# procs", "flag");
	seq_printf(s, "----------------------------------------------"
			"--------------------------------------------\n");

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		struct ion_buffer *buffer = handle->buffer;
		unsigned int id = buffer->heap->id;

		if (!names[id])
			names[id] = buffer->heap->name;
		sizes[id] += buffer->size;
		sizes_pss[id] += (buffer->size / buffer->handle_count);
		seq_printf(s, "%16.s %4u %16.s %4u %10zu %8d %9lx\n",
			   buffer->task_comm, buffer->pid,
				buffer->thread_comm, buffer->tid, buffer->size,
				buffer->handle_count, buffer->flags);
	}
	mutex_unlock(&client->lock);
	up_read(&g_idev->lock);

	seq_printf(s, "----------------------------------------------"
			"--------------------------------------------\n");
	seq_printf(s, "%16.16s: %16.16s %18.18s\n", "heap_name",
				"size_in_bytes", "size_in_bytes(pss)");
	for (i = 0; i < ION_NUM_HEAP_IDS; i++) {
		if (!names[i])
			continue;
		seq_printf(s, "%16.16s: %16zu %18zu\n",
				names[i], sizes[i], sizes_pss[i]);
	}
	return 0;
}

static int ion_debug_client_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_client_show, inode->i_private);
}

static const struct file_operations debug_client_fops = {
	.open = ion_debug_client_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ion_get_client_serial(const struct rb_root *root,
					const unsigned char *name)
{
	int serial = -1;
	struct rb_node *node;

	for (node = rb_first(root); node; node = rb_next(node)) {
		struct ion_client *client = rb_entry(node, struct ion_client,
						node);

		if (strcmp(client->name, name))
			continue;
		serial = max(serial, client->display_serial);
	}
	return serial + 1;
}

struct ion_client *ion_client_create(struct ion_device *dev,
				     const char *name)
{
	struct ion_client *client;
	struct task_struct *task;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct ion_client *entry;
	pid_t pid;

	if (!name) {
		pr_err("%s: Name cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);
	pid = task_pid_nr(current->group_leader);
	/* don't bother to store task struct for kernel threads,
	   they can't be killed anyway */
	if (current->group_leader->flags & PF_KTHREAD) {
		put_task_struct(current->group_leader);
		task = NULL;
	} else {
		task = current->group_leader;
	}
	task_unlock(current->group_leader);

	client = kzalloc(sizeof(struct ion_client), GFP_KERNEL);
	if (!client)
		goto err_put_task_struct;

	client->dev = dev;
	client->handles = RB_ROOT;
	idr_init(&client->idr);
	mutex_init(&client->lock);
	client->task = task;
	client->pid = pid;
	client->name = kstrdup(name, GFP_KERNEL);
	if (!client->name)
		goto err_free_client;

	down_write(&dev->lock);
	client->display_serial = ion_get_client_serial(&dev->clients, name);
	client->display_name = kasprintf(
		GFP_KERNEL, "%s-%d", name, client->display_serial);
	if (!client->display_name) {
		up_write(&dev->lock);
		goto err_free_client_name;
	}
	p = &dev->clients.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_client, node);

		if (client < entry)
			p = &(*p)->rb_left;
		else if (client > entry)
			p = &(*p)->rb_right;
	}
	rb_link_node(&client->node, parent, p);
	rb_insert_color(&client->node, &dev->clients);

	client->debug_root = debugfs_create_file(client->display_name, 0664,
						dev->clients_debug_root,
						client, &debug_client_fops);
	if (!client->debug_root) {
		char buf[256], *path;

		path = dentry_path(dev->clients_debug_root, buf, 256);
		pr_err("Failed to create client debugfs at %s/%s\n",
			path, client->display_name);
	}

	up_write(&dev->lock);

	return client;

err_free_client_name:
	kfree(client->name);
err_free_client:
	kfree(client);
err_put_task_struct:
	if (task)
		put_task_struct(current->group_leader);
	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL(ion_client_create);

void ion_client_destroy(struct ion_client *client)
{
	struct ion_device *dev = client->dev;
	struct rb_node *n;

	pr_debug("%s: %d\n", __func__, __LINE__);

	mutex_lock(&client->lock);
	while ((n = rb_first(&client->handles))) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		ion_handle_destroy(&handle->ref);
	}

	mutex_unlock(&client->lock);
	idr_destroy(&client->idr);

	down_write(&dev->lock);
	if (client->task)
		put_task_struct(client->task);
	rb_erase(&client->node, &dev->clients);
	debugfs_remove_recursive(client->debug_root);
	up_write(&dev->lock);

	kfree(client->display_name);
	kfree(client->name);
	kfree(client);
}
EXPORT_SYMBOL(ion_client_destroy);

struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	struct sg_table *table;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_dma.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}
	buffer = handle->buffer;
	table = buffer->sg_table;
	mutex_unlock(&client->lock);
	return table;
}
EXPORT_SYMBOL(ion_sg_table);

static void ion_buffer_sync_for_device(struct ion_buffer *buffer,
				       struct device *dev,
				       enum dma_data_direction direction);

static struct sg_table *ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct ion_buffer *buffer = dmabuf->priv;

	ion_buffer_sync_for_device(buffer, attachment->dev, direction);

	ion_buffer_task_add_lock(buffer, attachment->dev);

	return buffer->sg_table;
}

static void ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	ion_buffer_task_remove_lock(attachment->dmabuf->priv, attachment->dev);
}

void ion_pages_sync_for_device(struct device *dev, struct page *page,
		size_t size, enum dma_data_direction dir)
{
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, size, 0);
	/*
	 * This is not correct - sg_dma_address needs a dma_addr_t that is valid
	 * for the targeted device, but this works on the currently targeted
	 * hardware.
	 */
	sg_dma_address(&sg) = page_to_phys(page);
	dma_sync_sg_for_device(dev, &sg, 1, dir);
}

struct ion_vma_list {
	struct list_head list;
	struct vm_area_struct *vma;
};

static void ion_buffer_sync_for_device(struct ion_buffer *buffer,
				       struct device *dev,
				       enum dma_data_direction dir)
{
	struct ion_vma_list *vma_list;
	int pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	int i;

	ion_buffer_make_ready_lock(buffer);

	if (!ion_buffer_cached(buffer))
		return;

	pr_debug("%s: syncing for device %s\n", __func__,
		 dev ? dev_name(dev) : "null");

	if (!ion_buffer_fault_user_mappings(buffer))
		return;

	mutex_lock(&buffer->lock);
	for (i = 0; i < pages; i++) {
		struct page *page = buffer->pages[i];

		if (ion_buffer_page_is_dirty(page))
			ion_pages_sync_for_device(dev, ion_buffer_page(page),
							PAGE_SIZE, dir);

		ion_buffer_page_clean(buffer->pages + i);
	}
	list_for_each_entry(vma_list, &buffer->vmas, list) {
		struct vm_area_struct *vma = vma_list->vma;

		zap_page_range(vma, vma->vm_start, vma->vm_end - vma->vm_start,
			       NULL);
	}
	mutex_unlock(&buffer->lock);
}

static int ion_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct ion_buffer *buffer = vma->vm_private_data;
	unsigned long pfn;
	int ret;

	mutex_lock(&buffer->lock);
	ion_buffer_page_dirty(buffer->pages + vmf->pgoff);
	BUG_ON(!buffer->pages || !buffer->pages[vmf->pgoff]);

	pfn = page_to_pfn(ion_buffer_page(buffer->pages[vmf->pgoff]));
	ret = vm_insert_pfn(vma, (unsigned long)vmf->virtual_address, pfn);
	mutex_unlock(&buffer->lock);
	if (ret)
		return VM_FAULT_ERROR;

	return VM_FAULT_NOPAGE;
}

static void ion_vm_open(struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = vma->vm_private_data;
	struct ion_vma_list *vma_list;

	vma_list = kmalloc(sizeof(struct ion_vma_list), GFP_KERNEL);
	if (!vma_list)
		return;
	vma_list->vma = vma;
	mutex_lock(&buffer->lock);
	list_add(&vma_list->list, &buffer->vmas);
	mutex_unlock(&buffer->lock);
	pr_debug("%s: adding %pK\n", __func__, vma);
}

static void ion_vm_close(struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = vma->vm_private_data;
	struct ion_vma_list *vma_list, *tmp;

	pr_debug("%s\n", __func__);
	mutex_lock(&buffer->lock);
	list_for_each_entry_safe(vma_list, tmp, &buffer->vmas, list) {
		if (vma_list->vma != vma)
			continue;
		list_del(&vma_list->list);
		kfree(vma_list);
		pr_debug("%s: deleting %pK\n", __func__, vma);
		break;
	}
	mutex_unlock(&buffer->lock);
}

static struct vm_operations_struct ion_vma_ops = {
	.open = ion_vm_open,
	.close = ion_vm_close,
	.fault = ion_vm_fault,
};

static int ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = dmabuf->priv;
	int ret = 0;

	ION_EVENT_BEGIN();

	if (buffer->flags & ION_FLAG_NOZEROED) {
		pr_err("%s: mmap non-zeroed buffer to user is prohibited!\n",
			__func__);
		return -EINVAL;
	}

	if (buffer->flags & ION_FLAG_PROTECTED) {
		pr_err("%s: mmap protected buffer to user is prohibited!\n",
			__func__);
		return -EPERM;
	}

	if ((((vma->vm_pgoff << PAGE_SHIFT) >= buffer->size)) ||
		((vma->vm_end - vma->vm_start) >
			 (buffer->size - (vma->vm_pgoff << PAGE_SHIFT)))) {
		pr_err("%s: trying to map outside of buffer.\n", __func__);
		return -EINVAL;
	}

	if (!buffer->heap->ops->map_user) {
		pr_err("%s: this heap does not define a method for mapping to userspace\n",
			__func__);
		return -EINVAL;
	}

	trace_ion_mmap_start((unsigned long) buffer, buffer->size,
			!(buffer->flags & ION_FLAG_CACHED_NEEDS_SYNC));

	ion_buffer_make_ready_lock(buffer);

	if (ion_buffer_fault_user_mappings(buffer)) {
		vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND |
							VM_DONTDUMP;
		vma->vm_private_data = buffer;
		vma->vm_ops = &ion_vma_ops;
		ion_vm_open(vma);
		ION_EVENT_MMAP(buffer, ION_EVENT_DONE());
		trace_ion_mmap_end((unsigned long) buffer, buffer->size,
				!(buffer->flags & ION_FLAG_CACHED_NEEDS_SYNC));
		return 0;
	}

	if (!(buffer->flags & ION_FLAG_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* Default writeable */
	vma->vm_page_prot = pte_mkdirty(vma->vm_page_prot);

	mutex_lock(&buffer->lock);
	/* now map it to userspace */
	ret = buffer->heap->ops->map_user(buffer->heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		pr_err("%s: failure mapping buffer to userspace\n",
		       __func__);

	ION_EVENT_MMAP(buffer, ION_EVENT_DONE());
	trace_ion_mmap_end((unsigned long) buffer, buffer->size,
			!(buffer->flags & ION_FLAG_CACHED_NEEDS_SYNC));

	return ret;
}

static void ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;

	ion_buffer_put(buffer);
}

static void *ion_dma_buf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
	struct ion_buffer *buffer = dmabuf->priv;

	return buffer->vaddr + offset * PAGE_SIZE;
}

static void ion_dma_buf_kunmap(struct dma_buf *dmabuf, unsigned long offset,
			       void *ptr)
{
}

static int ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf, size_t start,
					size_t len,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	void *vaddr;

	if (!buffer->heap->ops->map_kernel) {
		pr_err("%s: map kernel is not implemented by this heap.\n",
		       __func__);
		return -ENODEV;
	}

	mutex_lock(&buffer->lock);
	vaddr = ion_buffer_kmap_get(buffer);
	mutex_unlock(&buffer->lock);
	return PTR_ERR_OR_ZERO(vaddr);
}

static void ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf, size_t start,
				       size_t len,
				       enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	ion_buffer_kmap_put(buffer);
	mutex_unlock(&buffer->lock);
}

static struct dma_buf_ops dma_buf_ops = {
	.map_dma_buf = ion_map_dma_buf,
	.unmap_dma_buf = ion_unmap_dma_buf,
	.mmap = ion_mmap,
	.release = ion_dma_buf_release,
	.begin_cpu_access = ion_dma_buf_begin_cpu_access,
	.end_cpu_access = ion_dma_buf_end_cpu_access,
	.kmap_atomic = ion_dma_buf_kmap,
	.kunmap_atomic = ion_dma_buf_kunmap,
	.kmap = ion_dma_buf_kmap,
	.kunmap = ion_dma_buf_kunmap,
};

struct dma_buf *ion_share_dma_buf(struct ion_client *client,
						struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	struct dma_buf *dmabuf;
	bool valid_handle;

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);
	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to share.\n", __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}
	buffer = handle->buffer;
	ion_buffer_get(buffer);
	mutex_unlock(&client->lock);

	dmabuf = dma_buf_export(buffer, &dma_buf_ops, buffer->size, O_RDWR,
				NULL);
	if (IS_ERR(dmabuf)) {
		ion_buffer_put(buffer);
		return dmabuf;
	}

	return dmabuf;
}
EXPORT_SYMBOL(ion_share_dma_buf);

int ion_share_dma_buf_fd(struct ion_client *client, struct ion_handle *handle)
{
	struct dma_buf *dmabuf;
	int fd;

	dmabuf = ion_share_dma_buf(client, handle);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0)
		dma_buf_put(dmabuf);

	return fd;
}
EXPORT_SYMBOL(ion_share_dma_buf_fd);

struct ion_handle *ion_import_dma_buf(struct ion_client *client, int fd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;
	struct ion_handle *handle;
	int ret;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return ERR_CAST(dmabuf);
	/* if this memory came from ion */

	if (dmabuf->ops != &dma_buf_ops) {
		pr_err("%s: can not import dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return ERR_PTR(-EINVAL);
	}
	buffer = dmabuf->priv;

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (!IS_ERR(handle)) {
		handle = ion_handle_get_check_overflow(handle);
		mutex_unlock(&client->lock);
		goto end;
	}

	handle = ion_handle_create(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		goto end;
	}

	ret = ion_handle_add(client, handle);
	mutex_unlock(&client->lock);
	if (ret) {
		ion_handle_put(client, handle);
		handle = ERR_PTR(ret);
	}

end:
	dma_buf_put(dmabuf);
	return handle;
}
EXPORT_SYMBOL(ion_import_dma_buf);

int ion_cached_needsync_dmabuf(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	unsigned long cacheflag = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;

	if (dmabuf->ops != &dma_buf_ops)
		return -EINVAL;

	return ((buffer->flags & cacheflag) == cacheflag) ? 1 : 0;
}
EXPORT_SYMBOL(ion_cached_needsync_dmabuf);

static int ion_sync_for_device(struct ion_client *client, int fd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	/* if this memory came from ion */
	if (dmabuf->ops != &dma_buf_ops) {
		pr_err("%s: can not sync dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}
	buffer = dmabuf->priv;

	if (!ion_buffer_cached(buffer) ||
			ion_buffer_fault_user_mappings(buffer)) {
		dma_buf_put(dmabuf);
		return 0;
	}

	trace_ion_sync_start(_RET_IP_, buffer->dev->dev.this_device,
				DMA_BIDIRECTIONAL, buffer->size,
				buffer->vaddr, 0, false);

	if (IS_ENABLED(CONFIG_HIGHMEM)) {
		ion_device_sync(buffer->dev, buffer->sg_table->sgl,
				buffer->sg_table->nents, DMA_BIDIRECTIONAL,
				ion_buffer_flush, false);
	} else {
		struct scatterlist *sg, *sgl;
		int nelems;
		void *vaddr;
		int i = 0;

		sgl = buffer->sg_table->sgl;
		nelems = buffer->sg_table->nents;

		for_each_sg(sgl, sg, nelems, i) {
			vaddr = phys_to_virt(sg_phys(sg));
			__dma_flush_range(vaddr, vaddr + sg->length);
		}
	}

	trace_ion_sync_end(_RET_IP_, buffer->dev->dev.this_device,
				DMA_BIDIRECTIONAL, buffer->size,
				buffer->vaddr, 0, false);

	dma_buf_put(dmabuf);
	return 0;
}

static int ion_sync_partial_for_device(struct ion_client *client, int fd,
					off_t offset, size_t len)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;
	struct scatterlist *sg, *sgl;
	size_t remained = len;
	int nelems;
	int i;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	/* if this memory came from ion */
	if (dmabuf->ops != &dma_buf_ops) {
		pr_err("%s: can not sync dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}
	buffer = dmabuf->priv;

	if (!ion_buffer_cached(buffer) ||
			ion_buffer_fault_user_mappings(buffer)) {
		dma_buf_put(dmabuf);
		return 0;
	}

	trace_ion_sync_start(_RET_IP_, buffer->dev->dev.this_device,
				DMA_BIDIRECTIONAL, buffer->size,
				buffer->vaddr, 0, false);

	sgl = buffer->sg_table->sgl;
	nelems = buffer->sg_table->nents;

	for_each_sg(sgl, sg, nelems, i) {
		size_t len_to_flush;
		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		}

		len_to_flush = sg->length - offset;
		if (remained < len_to_flush) {
			len_to_flush = remained;
			remained = 0;
		} else {
			remained -= len_to_flush;
		}

		__dma_map_area(phys_to_virt(sg_phys(sg)) + offset,
				len_to_flush, DMA_TO_DEVICE);

		if (remained == 0)
			break;
		offset = 0;
	}

	trace_ion_sync_end(_RET_IP_, buffer->dev->dev.this_device,
				DMA_BIDIRECTIONAL, buffer->size,
				buffer->vaddr, 0, false);

	dma_buf_put(dmabuf);

	return 0;
}

static long ion_alloc_preload(struct ion_client *client,
				unsigned int heap_id_mask,
				unsigned int flags,
				unsigned int count,
				struct ion_preload_object obj[])
{
	struct ion_device *dev = client->dev;
	struct ion_heap *heap;
	bool found = false;
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (obj[i].len > SZ_32M) {
			pr_warn("%s: too big buffer %#zx\n",
				__func__, obj[i].len);
			return -EPERM;
		}

		if (obj[i].count > 8) {
			pr_warn("%s: number of buffer exceeds 8 (%d)\n",
				__func__, obj[i].count);
			return -EPERM;
		}
	}


	down_read(&dev->lock);
	heap_id_mask = MAKE_NEW_HEAP_MASK(heap_id_mask, flags);
	plist_for_each_entry(heap, &dev->heaps, node) {
		if ((1 << heap->id) & heap_id_mask) {
			found = true;
			break;
		}
	}
	up_read(&dev->lock);

	if (!found) {
		pr_err("%s: no such heap\n", __func__);
		return -ENODEV;
	}

	if (!heap->ops->preload) {
		pr_err("%s: %s does not support preload allocation\n",
				__func__, heap->name);
		return -EPERM;
	}

	heap->ops->preload(heap, flags, count, obj);

	return 0;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int ion_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
	case ION_IOC_SYNC:
	case ION_IOC_SYNC_PARTIAL:
	case ION_IOC_FREE:
	case ION_IOC_CUSTOM:
		return _IOC_WRITE;
	default:
		return _IOC_DIR(cmd);
	}
}

static long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ion_client *client = filp->private_data;
	struct ion_device *dev = client->dev;
	struct ion_handle *cleanup_handle = NULL;
	int ret = 0;
	unsigned int dir;

	union {
		struct ion_fd_data fd;
		struct ion_fd_partial_data fd_partial;
		struct ion_allocation_data allocation;
		struct ion_handle_data handle;
		struct ion_custom_data custom;
		struct ion_preload_data preload;
	} data;

	dir = ion_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (dir & _IOC_WRITE)
		if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;

	switch (cmd) {
	case ION_IOC_ALLOC:
	{
		struct ion_handle *handle;

		handle = __ion_alloc(client, data.allocation.len,
						data.allocation.align,
						data.allocation.heap_id_mask,
						data.allocation.flags, true);
		if (IS_ERR(handle)) {
			pr_err("%s: len %zu align %zu heap_id_mask %u flags %x (ret %ld)\n",
				__func__, data.allocation.len,
				data.allocation.align,
				data.allocation.heap_id_mask,
				data.allocation.flags, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		pass_to_user(handle);
		data.allocation.handle = handle->id;

		cleanup_handle = handle;
		break;
	}
	case ION_IOC_FREE:
	{
		struct ion_handle *handle;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, data.handle.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			return PTR_ERR(handle);
		}
		user_ion_free_nolock(client, handle);
		ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		break;
	}
	case ION_IOC_SHARE:
	case ION_IOC_MAP:
	{
		struct ion_handle *handle;

		handle = ion_handle_get_by_id(client, data.handle.handle);
		if (IS_ERR(handle))
			return PTR_ERR(handle);
		data.fd.fd = ion_share_dma_buf_fd(client, handle);
		ion_handle_put(client, handle);
		if (data.fd.fd < 0)
			ret = data.fd.fd;
		break;
	}
	case ION_IOC_IMPORT:
	{
		struct ion_handle *handle;

		handle = ion_import_dma_buf(client, data.fd.fd);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
		} else {
			handle = pass_to_user(handle);
			if (IS_ERR(handle))
				ret = PTR_ERR(handle);
			else
				data.handle.handle = handle->id;
		}
		break;
	}
	case ION_IOC_SYNC:
	{
		ret = ion_sync_for_device(client, data.fd.fd);
		break;
	}
	case ION_IOC_SYNC_PARTIAL:
	{
		ret = ion_sync_partial_for_device(client, data.fd_partial.fd,
			data.fd_partial.offset, data.fd_partial.len);
		break;
	}
	case ION_IOC_CUSTOM:
	{
		if (!dev->custom_ioctl)
			return -ENOTTY;
		ret = dev->custom_ioctl(client, data.custom.cmd,
						data.custom.arg);
		break;
	}
	case ION_IOC_PRELOAD_ALLOC:
	{
		struct ion_preload_object *obj;
		int ret;

		if (data.preload.count == 0)
			return 0;

		if (data.preload.count > 8) {
			pr_warn("%s: number of object types should be < 9\n",
				__func__);
			return -EPERM;
		}

		obj = kmalloc(sizeof(*obj) * data.preload.count, GFP_KERNEL);
		if (!obj)
			return -ENOMEM;

		if (copy_from_user(obj, (void __user *)data.preload.obj,
					sizeof(*obj) * data.preload.count)) {
			kfree(obj);
			return -EFAULT;
		}

		ret = ion_alloc_preload(client, data.preload.heap_id_mask,
						data.preload.flags,
						data.preload.count, obj);
		kfree(obj);
		return ret;
	}
	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd))) {
			if (cleanup_handle) {
				mutex_lock(&client->lock);
				user_ion_free_nolock(client, cleanup_handle);
				ion_handle_put_nolock(cleanup_handle);
				mutex_unlock(&client->lock);
			}
			return -EFAULT;
		}
	}
	if (cleanup_handle)
		ion_handle_put(client,cleanup_handle);
	return ret;
}

static int ion_release(struct inode *inode, struct file *file)
{
	struct ion_client *client = file->private_data;

	pr_debug("%s: %d\n", __func__, __LINE__);
	ion_client_destroy(client);
	return 0;
}

static int ion_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct ion_device *dev = container_of(miscdev, struct ion_device, dev);
	struct ion_client *client;
	char debug_name[64];

	pr_debug("%s: %d\n", __func__, __LINE__);
	snprintf(debug_name, 64, "%u", task_pid_nr(current->group_leader));
	client = ion_client_create(dev, debug_name);
	if (IS_ERR(client))
		return PTR_ERR(client);
	file->private_data = client;

	return 0;
}

static const struct file_operations ion_fops = {
	.owner          = THIS_MODULE,
	.open           = ion_open,
	.release        = ion_release,
	.unlocked_ioctl = ion_ioctl,
	.compat_ioctl   = compat_ion_ioctl,
};

static size_t ion_debug_heap_total(struct ion_client *client,
				   unsigned int id)
{
	size_t size = 0;
	struct rb_node *n;

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n,
						     struct ion_handle,
						     node);
		if (handle->buffer->heap->id == id)
			size += handle->buffer->size;
	}
	mutex_unlock(&client->lock);
	return size;
}

static int ion_debug_heap_show(struct seq_file *s, void *unused)
{
	struct ion_heap *heap = s->private;
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
	size_t total_size = 0;
	size_t total_orphaned_size = 0;

	seq_printf(s, "%16.s %16.s %16.s\n", "client", "pid", "size");
	seq_puts(s, "----------------------------------------------------\n");

	down_read(&dev->lock);

	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client,
						     node);
		size_t size = ion_debug_heap_total(client, heap->id);

		if (!size)
			continue;
		if (client->task) {
			char task_comm[TASK_COMM_LEN];

			get_task_comm(task_comm, client->task);
			seq_printf(s, "%16.s %16u %16zu\n", task_comm,
				   client->pid, size);
		} else {
			seq_printf(s, "%16.s %16u %16zu\n", client->name,
				   client->pid, size);
		}
	}
	seq_puts(s, "----------------------------------------------------\n");
	seq_puts(s, "orphaned allocations (info is from last known client):\n");
	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buffer = rb_entry(n, struct ion_buffer,
						     node);
		if (buffer->heap->id != heap->id)
			continue;
		total_size += buffer->size;
		if (!buffer->handle_count) {
			seq_printf(s, "%16.s %16u %16zu %d %d\n",
				   buffer->task_comm, buffer->pid,
				   buffer->size, buffer->kmap_cnt,
				   atomic_read(&buffer->ref.refcount));
			total_orphaned_size += buffer->size;
		}
	}
	mutex_unlock(&dev->buffer_lock);
	seq_puts(s, "----------------------------------------------------\n");
	seq_printf(s, "%16.s %16zu\n", "total orphaned",
		   total_orphaned_size);
	seq_printf(s, "%16.s %16zu\n", "total ", total_size);
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		seq_printf(s, "%16.s %16zu\n", "deferred free",
				heap->free_list_size);
	seq_puts(s, "----------------------------------------------------\n");

	if (heap->debug_show)
		heap->debug_show(heap, s, unused);

	up_read(&dev->lock);

	return 0;
}

static int ion_debug_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_heap_show, inode->i_private);
}

static const struct file_operations debug_heap_fops = {
	.open = ion_debug_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int debug_shrink_set(void *data, u64 val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = -1;
	sc.nr_to_scan = val;

	if (!val) {
		objs = heap->shrinker.count_objects(&heap->shrinker, &sc);
		sc.nr_to_scan = objs;
	}

	heap->shrinker.scan_objects(&heap->shrinker, &sc);
	return 0;
}

static int debug_shrink_get(void *data, u64 *val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = -1;
	sc.nr_to_scan = 0;

	objs = heap->shrinker.count_objects(&heap->shrinker, &sc);
	*val = objs;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_shrink_fops, debug_shrink_get,
			debug_shrink_set, "%llu\n");

void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap)
{
	struct dentry *debug_file;

	if (!heap->ops->allocate || !heap->ops->free || !heap->ops->map_dma ||
	    !heap->ops->unmap_dma)
		pr_err("%s: can not add heap with invalid ops struct.\n",
		       __func__);

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_init_deferred_free(heap);

	if ((heap->flags & ION_HEAP_FLAG_DEFER_FREE) || heap->ops->shrink)
		ion_heap_init_shrinker(heap);

	heap->dev = dev;
	down_write(&dev->lock);
	/* use negative heap->id to reverse the priority -- when traversing
	   the list later attempt higher id numbers first */
	plist_node_init(&heap->node, -heap->id);
	plist_add(&heap->node, &dev->heaps);
	debug_file = debugfs_create_file(heap->name, 0664,
					dev->heaps_debug_root, heap,
					&debug_heap_fops);

	if (!debug_file) {
		char buf[256], *path;

		path = dentry_path(dev->heaps_debug_root, buf, 256);
		pr_err("Failed to create heap debugfs at %s/%s\n",
			path, heap->name);
	}

	if (heap->shrinker.count_objects && heap->shrinker.scan_objects) {
		char debug_name[64];

		snprintf(debug_name, 64, "%s_shrink", heap->name);
		debug_file = debugfs_create_file(
			debug_name, 0644, dev->heaps_debug_root, heap,
			&debug_shrink_fops);
		if (!debug_file) {
			char buf[256], *path;

			path = dentry_path(dev->heaps_debug_root, buf, 256);
			pr_err("Failed to create heap shrinker debugfs at %s/%s\n",
				path, debug_name);
		}
	}

	up_write(&dev->lock);
}

#define VM_PAGE_COUNT_WIDTH 4
#define VM_PAGE_COUNT 4

static void ion_device_sync_and_unmap(unsigned long vaddr,
					pte_t *ptep, size_t size,
					enum dma_data_direction dir,
					ion_device_sync_func sync, bool memzero)
{
	int i;

	flush_cache_vmap(vaddr, vaddr + size);

	if (memzero)
		memset((void *) vaddr, 0, size);

	if (sync)
		sync((void *) vaddr, size, dir);

	for (i = 0; i < (size / PAGE_SIZE); i++)
		pte_clear(&init_mm, (void *) vaddr + (i * PAGE_SIZE), ptep + i);

	flush_cache_vunmap(vaddr, vaddr + size);
	flush_tlb_kernel_range(vaddr, vaddr + size);
}

void ion_device_sync(struct ion_device *dev, struct scatterlist *sgl,
			int nents, enum dma_data_direction dir,
			ion_device_sync_func sync, bool memzero)
{
	struct scatterlist *sg;
	int page_idx, pte_idx, i;
	unsigned long vaddr;
	size_t sum = 0;
	pte_t *ptep;

	if (!memzero && !sync)
		return;

	down(&dev->vm_sem);

	page_idx = atomic_pop(&dev->page_idx, VM_PAGE_COUNT_WIDTH);
	BUG_ON((page_idx < 0) || (page_idx >= VM_PAGE_COUNT));

	pte_idx = page_idx * (SZ_1M / PAGE_SIZE);
	ptep = dev->pte[pte_idx];
	vaddr = (unsigned long) dev->reserved_vm_area->addr +
				(SZ_1M * page_idx);

	for_each_sg(sgl, sg, nents, i) {
		int j;

		if (!PageHighMem(sg_page(sg))) {
			if (memzero)
				memset(page_address(sg_page(sg)),
							0, sg->length);
			if (sync)
				sync(page_address(sg_page(sg)) + sg->offset,
							sg->length, dir);
			continue;
		}

		if (!IS_ALIGNED(sg->length | sg->offset, PAGE_SIZE)) {
			void *va;
			size_t len = sg->length + sg->offset;
			size_t orglen = len;
			off_t off = sg->offset;

			BUG_ON(memzero);

			for (j = 0; j < (PAGE_ALIGN(orglen) / PAGE_SIZE); j++) {
				size_t pagelen;
				pagelen = (len > PAGE_SIZE) ?
						PAGE_SIZE : len;
				va = kmap(sg_page(sg) + j);
				sync(va + off, pagelen - off, dir);
				kunmap(sg_page(sg) + j);
				off = 0;
				len -= pagelen;
			}
			continue;
		}

		for (j = 0; j < (sg->length / PAGE_SIZE); j++) {
			set_pte_at(&init_mm, vaddr, ptep,
					mk_pte(sg_page(sg) + j, PAGE_KERNEL));
			ptep++;
			vaddr += PAGE_SIZE;
			sum += PAGE_SIZE;

			if (sum == SZ_1M) {
				ptep = dev->pte[pte_idx];
				vaddr =
				(unsigned long) dev->reserved_vm_area->addr
					+ (SZ_1M * page_idx);

				ion_device_sync_and_unmap(vaddr,
					ptep, sum, dir, sync, memzero);
				sum = 0;
			}
		}
	}

	if (sum != 0) {
		ion_device_sync_and_unmap(
			(unsigned long) dev->reserved_vm_area->addr +
				(SZ_1M * page_idx),
			dev->pte[pte_idx], sum, dir, sync, memzero);
	}

	atomic_push(&dev->page_idx, page_idx, VM_PAGE_COUNT_WIDTH);

	up(&dev->vm_sem);
}

static int ion_device_reserve_vm(struct ion_device *dev)
{
	int i;

	atomic_set(&dev->page_idx, -1);

	for (i = VM_PAGE_COUNT - 1; i >= 0; i--) {
		BUG_ON(i >= (1 << VM_PAGE_COUNT_WIDTH));
		atomic_push(&dev->page_idx, i, VM_PAGE_COUNT_WIDTH);
	}

	sema_init(&dev->vm_sem, VM_PAGE_COUNT);
	dev->pte = page_address(
			alloc_pages(GFP_KERNEL,
				get_order(((SZ_1M / PAGE_SIZE) *
						VM_PAGE_COUNT) *
						sizeof(*dev->pte))));
	dev->reserved_vm_area = alloc_vm_area(SZ_1M *
						VM_PAGE_COUNT, dev->pte);
	if (!dev->reserved_vm_area) {
		pr_err("%s: Failed to allocate vm area\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

#ifdef CONFIG_ION_EXYNOS_STAT_LOG

#define MAX_DUMP_TASKS		8
#define MAX_DUMP_NAME_LEN	32
#define MAX_DUMP_BUFF_LEN	512

static void ion_buffer_dump_flags(struct seq_file *s, unsigned long flags)
{
	if ((flags & ION_FLAG_CACHED) && !(flags & ION_FLAG_CACHED_NEEDS_SYNC))
		seq_printf(s, "cached|faultmap");
	else if (flags & ION_FLAG_CACHED)
		seq_printf(s, "cached|needsync");
	else
		seq_printf(s, "noncached");

	if (flags & ION_FLAG_NOZEROED)
		seq_printf(s, "|nozeroed");

	if (flags & ION_FLAG_PROTECTED)
		seq_printf(s, "|protected");
}

static void ion_buffer_dump_tasks(struct ion_buffer *buffer, char *str)
{
	struct ion_task *task, *tmp;
	const char *delim = "|";
	size_t total_len = 0;
	int count = 0;

	list_for_each_entry_safe(task, tmp, &buffer->master_list, list) {
		const char *name;
		size_t len = strlen(dev_name(task->master));

		if (len > MAX_DUMP_NAME_LEN)
			len = MAX_DUMP_NAME_LEN;
		if (!strncmp(dev_name(task->master), "ion", len)) {
			continue;
		} else {
			name = dev_name(task->master) + 9;
			len -= 9;
		}
		if (total_len + len + 1 > MAX_DUMP_BUFF_LEN)
			break;

		strncat((char *)(str + total_len), name, len);
		total_len += len;
		if (!list_is_last(&task->list, &buffer->master_list))
			str[total_len++] = *delim;

		if (++count > MAX_DUMP_TASKS)
			break;
	}
}

static int ion_debug_buffer_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = s->private;
	struct rb_node *n;
	char *master_name;
	size_t total_size = 0;

	master_name = kzalloc(MAX_DUMP_BUFF_LEN, GFP_KERNEL);
	if (!master_name) {
		pr_err("%s: no memory for client string buffer\n", __func__);
		return -ENOMEM;
	}

	seq_printf(s, "%20.s %16.s %4.s %16.s %4.s %10.s %4.s %3.s %6.s "
			"%24.s %9.s\n",
			"heap", "task", "pid", "thread", "tid",
			"size", "kmap", "ref", "handle",
			"master", "flag");
	seq_printf(s, "------------------------------------------"
			"----------------------------------------"
			"----------------------------------------"
			"--------------------------------------\n");

	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buffer = rb_entry(n, struct ion_buffer,
						     node);
		mutex_lock(&buffer->lock);
		ion_buffer_dump_tasks(buffer, master_name);
		total_size += buffer->size;
		seq_printf(s, "%20.s %16.s %4u %16.s %4u %10zu %4d %3d %6d "
				"%24.s %9lx", buffer->heap->name,
				buffer->task_comm, buffer->pid,
				buffer->thread_comm,
				buffer->tid, buffer->size, buffer->kmap_cnt,
				atomic_read(&buffer->ref.refcount),
				buffer->handle_count, master_name,
				buffer->flags);
		seq_printf(s, "(");
		ion_buffer_dump_flags(s, buffer->flags);
		seq_printf(s, ")\n");
		mutex_unlock(&buffer->lock);

		memset(master_name, 0, MAX_DUMP_BUFF_LEN);
	}
	mutex_unlock(&dev->buffer_lock);

	seq_printf(s, "------------------------------------------"
			"----------------------------------------"
			"----------------------------------------"
			"--------------------------------------\n");
	seq_printf(s, "%16.s %16zu\n", "total ", total_size);
	seq_printf(s, "------------------------------------------"
			"----------------------------------------"
			"----------------------------------------"
			"--------------------------------------\n");

	kfree(master_name);

	return 0;
}

static int ion_debug_buffer_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_buffer_show, inode->i_private);
}

static const struct file_operations debug_buffer_fops = {
	.open = ion_debug_buffer_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void ion_debug_event_show_one(struct seq_file *s,
					struct ion_eventlog *log)
{
	struct timeval tv = ktime_to_timeval(log->begin);
	long elapsed = ktime_us_delta(log->done, log->begin);

	if (elapsed == 0)
		return;

	seq_printf(s, "[%06ld.%06ld] ", tv.tv_sec, tv.tv_usec);

	switch (log->type) {
	case ION_EVENT_TYPE_ALLOC:
		{
		struct ion_event_alloc *data = &log->data.alloc;
		seq_printf(s, "%8s  %p  %18s  %11zd  ", "alloc",
				data->id, data->heap->name, data->size);
		break;
		}
	case ION_EVENT_TYPE_FREE:
		{
		struct ion_event_free *data = &log->data.free;
		seq_printf(s, "%8s  %p  %18s  %11zd  ", "free",
				data->id, data->heap->name, data->size);
		break;
		}
	case ION_EVENT_TYPE_MMAP:
		{
		struct ion_event_mmap *data = &log->data.mmap;
		seq_printf(s, "%8s  %p  %18s  %11zd  ", "mmap",
				data->id, data->heap->name, data->size);
		break;
		}
	case ION_EVENT_TYPE_SHRINK:
		{
		struct ion_event_shrink *data = &log->data.shrink;
		seq_printf(s, "%8s  %16lx  %18s  %11zd  ", "shrink",
				0l, "ion_noncontig_heap", data->size);
		elapsed = 0;
		break;
		}
	case ION_EVENT_TYPE_CLEAR:
		{
		struct ion_event_clear *data = &log->data.clear;
		seq_printf(s, "%8s  %p  %18s  %11zd  ", "clear",
				data->id, data->heap->name, data->size);
		break;
		}
	}

	seq_printf(s, "%9ld", elapsed);

	if (elapsed > 100 * USEC_PER_MSEC)
		seq_printf(s, " *");

	if (log->type == ION_EVENT_TYPE_ALLOC) {
		seq_printf(s, "  ");
		ion_buffer_dump_flags(s, log->data.alloc.flags);
	} else if (log->type == ION_EVENT_TYPE_CLEAR) {
		seq_printf(s, "  ");
		ion_buffer_dump_flags(s, log->data.clear.flags);
	}

	if (log->type == ION_EVENT_TYPE_FREE && log->data.free.shrinker)
		seq_printf(s, " shrinker");

	seq_printf(s, "\n");
}

static int ion_debug_event_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = s->private;
	int index = atomic_read(&dev->event_idx) % ION_EVENT_LOG_MAX;
	int last = index;

	seq_printf(s, "%13s %10s  %8s  %18s  %11s %10s %24s\n", "timestamp",
			"type", "id", "heap", "size", "time (us)", "remarks");
	seq_printf(s, "-------------------------------------------");
	seq_printf(s, "-------------------------------------------");
	seq_printf(s, "-----------------------------------------\n");

	do {
		if (++index >= ION_EVENT_LOG_MAX)
			index = 0;
		ion_debug_event_show_one(s, &dev->eventlog[index]);
	} while (index != last);

	return 0;
}

static int ion_debug_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_event_show, inode->i_private);
}

static const struct file_operations debug_event_fops = {
	.open = ion_debug_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

struct ion_device *ion_device_create(long (*custom_ioctl)
				     (struct ion_client *client,
				      unsigned int cmd,
				      unsigned long arg))
{
	struct ion_device *idev;
	int ret;

	idev = kzalloc(sizeof(struct ion_device), GFP_KERNEL);
	if (!idev)
		return ERR_PTR(-ENOMEM);

	idev->dev.minor = MISC_DYNAMIC_MINOR;
	idev->dev.name = "ion";
	idev->dev.fops = &ion_fops;
	idev->dev.parent = NULL;
	ret = misc_register(&idev->dev);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		kfree(idev);
		return ERR_PTR(ret);
	}

	idev->debug_root = debugfs_create_dir("ion", NULL);
	if (!idev->debug_root) {
		pr_err("ion: failed to create debugfs root directory.\n");
		goto debugfs_done;
	}
	idev->heaps_debug_root = debugfs_create_dir("heaps", idev->debug_root);
	if (!idev->heaps_debug_root) {
		pr_err("ion: failed to create debugfs heaps directory.\n");
		goto debugfs_done;
	}
	idev->clients_debug_root = debugfs_create_dir("clients",
						idev->debug_root);
	if (!idev->clients_debug_root) {
		pr_err("ion: failed to create debugfs clients directory.\n");
		goto debugfs_done;
	}

#ifdef CONFIG_ION_EXYNOS_STAT_LOG
	atomic_set(&idev->event_idx, -1);
	idev->buffer_debug_file = debugfs_create_file("buffer", 0444,
						 idev->debug_root, idev,
						 &debug_buffer_fops);
	if (!idev->buffer_debug_file) {
		pr_err("%s: failed to create buffer debug file\n", __func__);
		goto debugfs_done;
	}

	idev->event_debug_file = debugfs_create_file("event", 0444,
						 idev->debug_root, idev,
						 &debug_event_fops);
	if (!idev->event_debug_file)
		pr_err("%s: failed to create event debug file\n", __func__);
#endif

debugfs_done:

	idev->custom_ioctl = custom_ioctl;
	idev->buffers = RB_ROOT;
	mutex_init(&idev->buffer_lock);
	init_rwsem(&idev->lock);
	plist_head_init(&idev->heaps);
	idev->clients = RB_ROOT;

	if (IS_ENABLED(CONFIG_HIGHMEM)) {
		ret = ion_device_reserve_vm(idev);
		if (ret)
			panic("ion: failed to reserve vm area\n");
	}

	/* backup of ion device: assumes there is only one ion device */
	g_idev = idev;

	return idev;
}

void ion_device_destroy(struct ion_device *dev)
{
	misc_deregister(&dev->dev);
	debugfs_remove_recursive(dev->debug_root);
	/* XXX need to free the heaps and clients ? */

	if (IS_ENABLED(CONFIG_HIGHMEM))
		free_vm_area(dev->reserved_vm_area);

	kfree(dev);
}

void __init ion_reserve(struct ion_platform_data *data)
{
	int i;

	for (i = 0; i < data->nr; i++) {
		if (data->heaps[i].size == 0)
			continue;

		if (data->heaps[i].base == 0) {
			phys_addr_t paddr;

			paddr = memblock_alloc_base(data->heaps[i].size,
						    data->heaps[i].align,
						    MEMBLOCK_ALLOC_ANYWHERE);
			if (!paddr) {
				pr_err("%s: error allocating memblock for heap %d\n",
					__func__, i);
				continue;
			}
			data->heaps[i].base = paddr;
		} else {
			int ret = memblock_reserve(data->heaps[i].base,
					       data->heaps[i].size);
			if (ret)
				pr_err("memblock reserve of %zx@%lx failed\n",
				       data->heaps[i].size,
				       data->heaps[i].base);
		}
		pr_info("%s: %s reserved base %lx size %zu\n", __func__,
			data->heaps[i].name,
			data->heaps[i].base,
			data->heaps[i].size);
	}
}

static struct ion_iovm_map *ion_buffer_iova_create(struct ion_buffer *buffer,
		struct device *dev, enum dma_data_direction dir, int iommu_prot)
{
	/* Must be called under buffer->lock held */
	struct ion_iovm_map *iovm_map;
	int ret = 0;

	iovm_map = kzalloc(sizeof(struct ion_iovm_map), GFP_KERNEL);
	if (!iovm_map) {
		pr_err("%s: Failed to allocate ion_iovm_map for %s\n",
			__func__, dev_name(dev));
		return ERR_PTR(-ENOMEM);
	}

	iovm_map->iova = iovmm_map(dev, buffer->sg_table->sgl,
					0, buffer->size, dir,
					iommu_prot);

	if (iovm_map->iova == (dma_addr_t)-ENOSYS) {
		size_t len;
		ion_phys_addr_t addr;

		BUG_ON(!buffer->heap->ops->phys);
		ret = buffer->heap->ops->phys(buffer->heap, buffer,
						&addr, &len);
		if (ret)
			pr_err("%s: Unable to get PA for %s\n",
					__func__, dev_name(dev));
	} else if (IS_ERR_VALUE(iovm_map->iova)) {
		ret = iovm_map->iova;
		pr_err("%s: Unable to allocate IOVA for %s\n",
			__func__, dev_name(dev));
	}

	if (ret) {
		kfree(iovm_map);
		return ERR_PTR(ret);
	}

	iovm_map->dev = dev;
	iovm_map->domain = get_domain_from_dev(dev);
	iovm_map->map_cnt = 1;

	pr_debug("%s: new map added for dev %s, iova %pa\n", __func__,
			dev_name(dev), &iovm_map->iova);

	return iovm_map;
}

dma_addr_t ion_iovmm_map(struct dma_buf_attachment *attachment,
			 off_t offset, size_t size,
			 enum dma_data_direction direction,
			 int iommu_prot)
{
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_iovm_map *iovm_map;
	struct iommu_domain *domain;

	BUG_ON(dmabuf->ops != &dma_buf_ops);

	domain = get_domain_from_dev(attachment->dev);
	if (!domain) {
		pr_err("%s: invalid iommu device\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&buffer->lock);
	list_for_each_entry(iovm_map, &buffer->iovas, list) {
		if (domain == iovm_map->domain) {
			pr_debug("%s: reusable map found: dev %s, domain %p\n",
				__func__, dev_name(iovm_map->dev),
				iovm_map->domain);
			iovm_map->map_cnt++;
			mutex_unlock(&buffer->lock);
			return iovm_map->iova;
		}
	}

	iovm_map = ion_buffer_iova_create(buffer, attachment->dev,
						direction, iommu_prot);
	if (IS_ERR(iovm_map)) {
		mutex_unlock(&buffer->lock);
		return PTR_ERR(iovm_map);
	}

	list_add_tail(&iovm_map->list, &buffer->iovas);
	mutex_unlock(&buffer->lock);

	return iovm_map->iova;
}

void ion_iovmm_unmap(struct dma_buf_attachment *attachment, dma_addr_t iova)
{
	struct ion_iovm_map *this_map = NULL;
	struct ion_iovm_map *iovm_map;
	struct dma_buf * dmabuf = attachment->dmabuf;
	struct device *dev = attachment->dev;
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct iommu_domain *domain;
	bool found = false;

	BUG_ON(dmabuf->ops != &dma_buf_ops);

	domain = get_domain_from_dev(attachment->dev);
	if (!domain) {
		pr_err("%s: invalid iommu device\n", __func__);
		return;
	}

	mutex_lock(&buffer->lock);
	list_for_each_entry(iovm_map, &buffer->iovas, list) {
		if (domain == iovm_map->domain) {
			if (iova == iovm_map->iova) {
				if (WARN_ON(iovm_map->map_cnt-- == 0))
					iovm_map->map_cnt = 0;
				this_map = iovm_map;
			} else {
				found = true;
				pr_debug("%s: found new map %pa for dev %s.\n",
					__func__, &iovm_map->iova,
					dev_name(iovm_map->dev));
			}
		}
	}

	if (!this_map) {
		pr_warn("%s: IOVA %pa is not found for %s\n",
			__func__, &iova, dev_name(dev));
	} else if (found && !this_map->map_cnt) {
		pr_debug("%s: unmap previous %pa for dev %s\n",
			__func__, &this_map->iova, dev_name(this_map->dev));
		iovmm_unmap(this_map->dev, this_map->iova);
		list_del(&this_map->list);
		kfree(this_map);
	}

	mutex_unlock(&buffer->lock);
}
