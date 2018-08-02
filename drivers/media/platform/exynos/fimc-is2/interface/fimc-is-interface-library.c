/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/random.h>
#include <linux/firmware.h>

#include "fimc-is-interface-library.h"
#include "../fimc-is-device-ischain.h"

struct fimc_is_lib_support gPtr_lib_support;
bool vra_binary_load;

/*
 * Log write
 */

int fimc_is_log_write_console(char *str)
{
	pr_info("[@][LIB] %s", str);
	return 0;
}

int fimc_is_log_write(const char *str, ...)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	int size = 0;
	int cpu = raw_smp_processor_id();
	unsigned long long t;
	unsigned long nanosec_rem;
	va_list args;
	ulong debug_kva = lib->kvaddr + FIMC_IS_DEBUG_OFFSET;
	ulong ctrl_kva  = lib->kvaddr + FIMC_IS_DEBUGCTL_OFFSET;

	va_start(args, str);
	size += vscnprintf(lib->string + size,
			  sizeof(lib->string) - size, str, args);
	va_end(args);

	t = cpu_clock(UINT_MAX);
	nanosec_rem = do_div(t, 1000000000);
	size = snprintf(lib->log_buf, sizeof(lib->log_buf),
		"[%5lu.%06lu] [%d] %s",
		(unsigned long)t, nanosec_rem/1000, cpu, lib->string);

	if (size > 256)
		BUG_ON(1);

	if (lib->log_ptr == 0) {
		lib->log_ptr = debug_kva;
	} else if (lib->log_ptr >= (ctrl_kva - size)) {
		memcpy((void *)lib->log_ptr, (void *)&lib->log_buf,
			(ctrl_kva - lib->log_ptr));
		size -= (debug_kva + FIMC_IS_DEBUG_SIZE - (u32)lib->log_ptr);
		lib->log_ptr = debug_kva;
	}

	memcpy((void *)lib->log_ptr, (void *)&lib->log_buf, size);
	lib->log_ptr += size;

	*((int *)(ctrl_kva)) = (lib->log_ptr - lib->kvaddr);

	return 0;
}

#ifdef LIB_MEM_TRACK
static inline void add_alloc_track(int type, ulong addr, size_t size)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct lib_mem_tracks *tracks = lib->cur_tracks;
	struct lib_mem_track *track;

	if (tracks && (tracks->num_of_track == MEM_TRACK_COUNT)) {
		tracks = kzalloc(sizeof(struct lib_mem_tracks), GFP_KERNEL);
		lib->cur_tracks = tracks;

		if (tracks)
			list_add(&tracks->list, &lib->list_of_tracks);
		else
			err_lib("failed to allocate memory track");
	}

	if (tracks) {
		track = &tracks->track[tracks->num_of_track];
		tracks->num_of_track++;

		track->type = type;
		track->addr = addr;
		track->size = size;
		track->status = MT_STATUS_ALLOC;
		track->alloc.lr = (ulong)__builtin_return_address(0);
		track->alloc.cpu = raw_smp_processor_id();
		track->alloc.pid = current->pid;
		track->alloc.when = cpu_clock(raw_smp_processor_id());
	}
}

static inline void add_free_track(int type, ulong addr)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct lib_mem_tracks *tracks, *temp;
	struct lib_mem_track *track;
	int found = 0;
	int i;

	list_for_each_entry_safe(tracks, temp, &lib->list_of_tracks, list) {
		for (i = 0; i < tracks->num_of_track; i++) {
			track = &tracks->track[i];

			if ((track->addr == addr)
				&& (track->status == MT_STATUS_ALLOC)
				&& (track->type == type)) {
				track->status |= MT_STATUS_FREE;
				track->free.lr = (ulong)__builtin_return_address(0);
				track->free.cpu = raw_smp_processor_id();
				track->free.pid = current->pid;
				track->free.when = cpu_clock(raw_smp_processor_id());

				found = 1;
				break;
			}
		}

		if (found)
			break;
	}

	if (!found)
		err_lib("could not find buffer [%d, 0x%08lx]", type, addr);
}

static void print_track(struct lib_mem_track *track)
{
	unsigned long long when;
	ulong usec;

	pr_info("type: %d, addr: 0x%08lx, size: %zd, status: %d\n",
			track->type, track->addr, track->size, track->status);

	when = track->alloc.when;
	usec = do_div(when, NSEC_PER_SEC);
	pr_info("\talloc: [%5lu.%06lu] from: 0x%08lx, cpu: %d, pid: %d\n",
			(ulong)when, usec / NSEC_PER_USEC,
			track->alloc.lr-1, track->alloc.cpu, track->alloc.pid);

	when = track->free.when;
	usec = do_div(when, NSEC_PER_SEC);
	pr_info("\tfree:  [%5lu.%06lu] from: 0x%08lx, cpu: %d, pid: %d\n",
			(ulong)when, usec / NSEC_PER_USEC,
			track->free.lr-1, track->free.cpu, track->free.pid);
}

static void check_tracks(int status)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct lib_mem_tracks *tracks, *temp;
	struct lib_mem_track *track;
	int i;

	list_for_each_entry_safe(tracks, temp, &lib->list_of_tracks, list) {
		for (i = 0; i < tracks->num_of_track; i++) {
			track = &tracks->track[i];

			if (track->status == status)
				print_track(track);
		}
	}
}

static void free_tracks(void)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct lib_mem_tracks *tracks;

	while (!list_empty(&lib->list_of_tracks)) {
		tracks = list_entry((&lib->list_of_tracks)->next,
						struct lib_mem_tracks, list);

		list_del(&tracks->list);
		kfree(tracks);
	}
}
#endif

/*
 * Memory alloc, free
 */
#ifdef ENABLE_RESERVED_INTERNAL_DMA
void *fimc_is_alloc_reserved_buffer(u32 size)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct fimc_is_lib_dma_buffer *reserved_buf = NULL;
	u32 aligned_size = 0;
	u32 offset = FIMC_IS_RESERVE_OFFSET;

	if (size <= 0) {
		err_lib("invaild size(%d)", size);
		return NULL;
	}

	reserved_buf = kzalloc(sizeof(struct fimc_is_lib_dma_buffer), GFP_KERNEL);
	if (lib->reserved_buf_size + size > FIMC_IS_RESERVE_SIZE) {
		err_lib("Out of reserved memory, use dynamic alloc\n");
		reserved_buf->kvaddr = (ulong)kzalloc(size, GFP_KERNEL);
		reserved_buf->dvaddr = 0; /* dynamic */
		aligned_size = size;
		reserved_buf->size = aligned_size;
	} else {
		reserved_buf->kvaddr = lib->kvaddr + offset + lib->reserved_buf_size;
		reserved_buf->dvaddr = lib->dvaddr + offset + lib->reserved_buf_size;
		aligned_size = ALIGN(size, 0x40);
		reserved_buf->size = aligned_size;
		lib->reserved_buf_size += aligned_size;
	}

	dbg_lib("alloc_reserved_buffer: kva(0x%lx) size(0x%x)\n",
		reserved_buf->kvaddr, aligned_size);

	list_add(&reserved_buf->list, &lib->reserved_buf_list);
#ifdef LIB_MEM_TRACK
	add_alloc_track(MT_TYPE_RESERVED, reserved_buf->kvaddr, aligned_size);
#endif

	return (void *)reserved_buf->kvaddr;
}

void fimc_is_free_reserved_buffer(void *buf)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct fimc_is_lib_dma_buffer *reserved_buf, *temp;

	if (buf == NULL)
		return;

	list_for_each_entry_safe(reserved_buf, temp, &lib->reserved_buf_list, list) {
		if ((void *)reserved_buf->kvaddr == buf) {
#ifdef LIB_MEM_TRACK
			add_free_track(MT_TYPE_RESERVED, (ulong)buf);
#endif
			if (reserved_buf->dvaddr)
				lib->reserved_buf_size -= reserved_buf->size;
			else
				kfree((void *)reserved_buf->kvaddr);

			list_del(&reserved_buf->list);
			kfree(reserved_buf);

			break;
		}
	}
}

void *fimc_is_alloc_dma_buffer(u32 size)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	ulong buf_addr;

	if ((lib->base_addr_size + size) >
			(FIMC_IS_THUMBNAIL_SDMA_SIZE + FIMC_IS_RESERVE_DMA_SIZE)) {
		err_lib("Invalid DMABUF size");
		return NULL;
	}

	buf_addr = lib->kvaddr + FIMC_IS_THUMBNAIL_SDMA_OFFSET + lib->base_addr_size;

#ifdef LIB_MEM_TRACK
	add_alloc_track(MT_TYPE_DMA, (ulong)buf_addr, size);
#endif

	dbg_lib("alloc_dma_buffer: kva(0x%lx), size(0x%x)\n", buf_addr, size);

	lib->base_addr_size += size;

	return (void *)buf_addr;
}

void fimc_is_free_dma_buffer(void *buf)
{
#ifdef LIB_MEM_TRACK
	add_free_track(MT_TYPE_DMA, (ulong)buf);
#endif

	dbg_lib("free_dma_buffer: kva(0x%p)\n", buf);
}

int fimc_is_translate_kva_to_dva(ulong src_addr, u32 *target_addr)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	u32 offset;

	if (src_addr < lib->kvaddr) {
		err_lib("translate_kva_to_dva: Invalid kva(0x%lx)!!", src_addr);
		*target_addr = 0x0;
		return 0;
	}

	offset = src_addr - lib->kvaddr;
	*target_addr = (u32)(lib->dvaddr + offset);

	dbg_lib("translate_kva_to_dva: offset(0x%x), kva(0x%lx), dva(0x%x)\n",
		offset, src_addr, *target_addr);

	return 0;
}

int fimc_is_translate_dva_to_kva(u32 src_addr, ulong *target_addr)
{
	dbg_lib("translate_dva_to_kva: dva(0x%x)\n", src_addr);

	return 0;
}
#else /* #ifdef ENABLE_RESERVED_INTERNAL_DMA */
void *fimc_is_alloc_buffer(u32 size)
{
	char *buf = NULL;

	if (size <= 0) {
		err_lib("invaild size(%d)", size);
		return NULL;
	}
	buf = kzalloc(size, GFP_KERNEL);

#ifdef LIB_MEM_TRACK
	add_alloc_track(MT_TYPE_HEAP, (ulong)buf, size);
#endif

	return (void *)buf;
}

void fimc_is_free_buffer(void *buf)
{
	if (buf != NULL) {
#ifdef LIB_MEM_TRACK
		add_free_track(MT_TYPE_HEAP, (ulong)buf);
#endif
		kfree(buf);
	}
}
void print_dma_buf(void)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct fimc_is_lib_dma_buffer *dma_buf, *temp;

	printk(KERN_INFO "[LIB][DMA] Buffer list :");
	list_for_each_entry_safe(dma_buf, temp, &lib->dma_buf_list, list)
		printk(KERN_INFO "0x%08lx[%d]->", dma_buf->kvaddr, dma_buf->size);

	printk(KERN_INFO "X\n");
}

void *fimc_is_alloc_dma_buffer(u32 size)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct fimc_is_lib_dma_buffer *dma_buf = NULL;
	void *ret_addr = NULL;
	int ret = 0;

	dma_buf = kzalloc(sizeof(struct fimc_is_lib_dma_buffer), GFP_KERNEL);
	dma_buf->dma_cookie = vb2_ion_private_alloc(lib->alloc_ctx, size, 1, 0);
	if (IS_ERR(dma_buf->dma_cookie)) {
		err_lib("DMA buffer allocation is failed!!");
		return ret_addr;
	}

#ifdef LIB_MEM_TRACK
	add_alloc_track(MT_TYPE_DMA, (ulong)dma_buf, sizeof(struct fimc_is_lib_dma_buffer));
#endif

	ret = vb2_ion_dma_address(dma_buf->dma_cookie, &(dma_buf->dvaddr));
	if (ret < 0) {
		err_lib("The base memory is not aligned to correctly");
		vb2_ion_private_free(dma_buf->dma_cookie);
		dma_buf->dvaddr = 0;
		dma_buf->dma_cookie = NULL;
		return ret_addr;
	}

	ret_addr = vb2_ion_private_vaddr(dma_buf->dma_cookie);
	dma_buf->kvaddr = (ulong)ret_addr;
	if (IS_ERR((void *)dma_buf->kvaddr)) {
		err_lib("Bitprocessor memory remap failed");
		vb2_ion_private_free(dma_buf->dma_cookie);
		dma_buf->kvaddr = 0;
		dma_buf->dma_cookie = NULL;
		return ret_addr;
	}

	vb2_ion_sync_for_device(dma_buf->dma_cookie, 0, size, DMA_BIDIRECTIONAL);
	dma_buf->size = size;
	list_add(&dma_buf->list, &lib->dma_buf_list);

	dbg_lib("alloc_dma_buffer: kva(0x%lx), size(0x%x)\n", ret_addr, size);

	return ret_addr;
}

void fimc_is_free_dma_buffer(void *buf)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct fimc_is_lib_dma_buffer *dma_buf, *temp;
	int free_cnt = 0;

	dbg_lib("free_dma_buffer: kva(0x%p)\n", buf);

	list_for_each_entry_safe(dma_buf, temp, &lib->dma_buf_list, list) {
		if ((void *)dma_buf->kvaddr == buf) {
			vb2_ion_private_free(dma_buf->dma_cookie);
			list_del(&dma_buf->list);
			kfree(dma_buf);
			free_cnt++;
		}
	}

	if (free_cnt > 1) {
		print_dma_buf();
		err_lib("Duplicated free occur (%d)", free_cnt);
		BUG_ON(1);
	}

	if (buf != NULL) {
#ifdef LIB_MEM_TRACK
		add_free_track(MT_TYPE_DMA, (ulong)buf);
#endif
		kfree(buf);
	}
}

int fimc_is_translate_kva_to_dva(ulong src_addr, u32 *target_addr)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct fimc_is_lib_dma_buffer *dma_buf, *temp;
	ulong offset = 0;
	int free_cnt = 0;

	list_for_each_entry_safe(dma_buf, temp, &lib->dma_buf_list, list) {
		if ((dma_buf->kvaddr <= src_addr)
			&& (src_addr <= (dma_buf->kvaddr + dma_buf->size))) {
			offset = src_addr - dma_buf->kvaddr;
			*target_addr = dma_buf->dvaddr + offset;
			free_cnt++;
		}
	}

	if (free_cnt > 1) {
		print_dma_buf();
		err_lib("Duplicated addr translation occur!! kva(0x%lx)", src_addr);
		BUG_ON(1);
	}

	dbg_lib("translate_kva_to_dva: offset(0x%x), kva(0x%lx), dva(0x%x)\n",
		offset, src_addr, *target_addr);

	return 0;
}

int fimc_is_translate_dva_to_kva(u32 src_addr, ulong *target_addr)
{
	dbg_lib("translate_dva_to_kva: dva(0x%x)\n", src_addr);

	return 0;
}
#endif /* #ifdef ENABLE_RESERVED_INTERNAL_DMA */

/*
 * Assert
 */
int fimc_is_lib_logdump(void)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct fimc_is_debug *debug = &fimc_is_debug;
	size_t write_vptr, read_vptr;
	size_t read_cnt, read_cnt1, read_cnt2;
	void *read_ptr;
	ulong debug_kva = lib->kvaddr + FIMC_IS_DEBUG_OFFSET;
	ulong ctrl_kva  = lib->kvaddr + FIMC_IS_DEBUGCTL_OFFSET;

	write_vptr = *((int *)(ctrl_kva)) - FIMC_IS_DEBUG_OFFSET;
	read_vptr = debug->read_vptr;

	if (write_vptr >= read_vptr) {
		read_cnt1 = write_vptr - read_vptr;
		read_cnt2 = 0;
	} else {
		read_cnt1 = FIMC_IS_DEBUG_SIZE - read_vptr;
		read_cnt2 = write_vptr;
	}

	read_cnt = read_cnt1 + read_cnt2;
	info_lib("firmware message start(%zd)\n", read_cnt);

	if (read_cnt1) {
		read_ptr = (void *)(debug_kva + debug->read_vptr);

		fimc_is_print_buffer(read_ptr, read_cnt1);
		debug->read_vptr += read_cnt1;
	}

	if (debug->read_vptr >= FIMC_IS_DEBUG_SIZE) {
		if (debug->read_vptr > FIMC_IS_DEBUG_SIZE)
			err_lib("[DBG] read_vptr(%zd) is invalid", debug->read_vptr);
		debug->read_vptr = 0;
	}

	if (read_cnt2) {
		read_ptr = (void *)(debug_kva + debug->read_vptr);

		fimc_is_print_buffer(read_ptr, read_cnt2);
		debug->read_vptr += read_cnt2;
	}

	info_lib("end\n");

	return read_cnt;
}

void fimc_is_assert(void)
{
	BUG_ON(1);
}

/*
 * Semaphore
 */
int fimc_is_sema_init(void **sema, int sema_count)
{
	if (*sema == NULL) {
		dbg_lib("sema_init: kzalloc struct semaphore\n");
		*sema = kzalloc(sizeof(struct semaphore), GFP_KERNEL);
	}

	sema_init((struct semaphore *)*sema, sema_count);

#ifdef LIB_MEM_TRACK
	add_alloc_track(MT_TYPE_SEMA, (ulong)*sema, sizeof(struct semaphore));
#endif
	return 0;
}

int fimc_is_sema_finish(void *sema)
{
	if (sema == NULL) {
		err_lib("invalid sema");
		return -1;
	}

#ifdef LIB_MEM_TRACK
	add_free_track(MT_TYPE_SEMA, (ulong)sema);
#endif
	kfree(sema);
	return 0;
}

int fimc_is_sema_up(void *sema)
{
	if (sema == NULL) {
		err_lib("invalid sema");
		return -1;
	}

	up((struct semaphore *)sema);

	return 0;
}

int fimc_is_sema_down(void *sema)
{
	if (sema == NULL) {
		err_lib("invalid sema");
		return -1;
	}

	down((struct semaphore *)sema);

	return 0;
}

/*
 * Mutex
 */
int fimc_is_mutex_init(void **mutex)
{
	if (*mutex == NULL)
		*mutex = kzalloc(sizeof(struct mutex), GFP_KERNEL);

	mutex_init((struct mutex *)*mutex);

#ifdef LIB_MEM_TRACK
	add_alloc_track(MT_TYPE_MUTEX, (ulong)*mutex, sizeof(struct mutex));
#endif
	return 0;
}

int fimc_is_mutex_finish(void *mutex_lib)
{
	struct mutex *_mutex = NULL;

	if (mutex_lib == NULL) {
		err_lib("invalid mutex");
		return -1;
	}

	_mutex = (struct mutex *)mutex_lib;

	if (atomic_read(&_mutex->count) == 0)
		mutex_unlock(_mutex);

#ifdef LIB_MEM_TRACK
	add_free_track(MT_TYPE_MUTEX, (ulong)mutex_lib);
#endif
	kfree(mutex_lib);
	return 0;
}

int fimc_is_mutex_lock(void *mutex_lib)
{
	struct mutex *_mutex = NULL;

	if (mutex_lib == NULL) {
		err_lib("invalid mutex");
		return -1;
	}

	_mutex = (struct mutex *)mutex_lib;
	mutex_lock(_mutex);

	return 0;
}

bool fimc_is_mutex_tryLock(void *mutex_lib)
{
	struct mutex *_mutex = NULL;
	int locked = 0;

	if (mutex_lib == NULL) {
		err_lib("invalid mutex");
		return -1;
	}

	_mutex = (struct mutex *)mutex_lib;
	locked = mutex_trylock(_mutex);

	return locked == 1 ? true : false;
}

int fimc_is_mutex_unlock(void *mutex_lib)
{
	struct mutex *_mutex = NULL;

	if (mutex_lib == NULL) {
		err_lib("invalid mutex");
		return -1;
	}

	_mutex = (struct mutex *)mutex_lib;
	mutex_unlock(_mutex);

	return 0;
}

/*
 * Timer
 */
int fimc_is_timer_create(void **timer, ulong expires,
				void (*func)(ulong),
				ulong data)
{
	struct timer_list *pTimer = NULL;
	if (*timer == NULL)
		*timer = kzalloc(sizeof(struct timer_list), GFP_KERNEL);

	pTimer = *timer;

	init_timer(pTimer);
	pTimer->expires = jiffies + expires;
	pTimer->data = (ulong)data;
	pTimer->function = func;

#ifdef LIB_MEM_TRACK
	add_alloc_track(MT_TYPE_TIMER, (ulong)*timer, sizeof(struct timer_list));
#endif
	return 0;
}

int fimc_is_timer_delete(void *timer)
{
	if (timer == NULL) {
		err_lib("invalid timer");
		return -1;
	}

	/* Stop timer, when timer is started */
	del_timer((struct timer_list *)timer);

#ifdef LIB_MEM_TRACK
	add_free_track(MT_TYPE_TIMER, (ulong)timer);
#endif
	kfree(timer);

	return 0;
}

int fimc_is_timer_reset(void *timer, ulong expires)
{
	if (timer == NULL) {
		err_lib("invalid timer");
		return -1;
	}

	mod_timer((struct timer_list *)timer, jiffies + expires);

	return 0;
}

int fimc_is_timer_query(void *timer, ulong timerValue)
{
	/* TODO: will be IMPLEMENTED */
	return 0;
}

int fimc_is_timer_enable(void *timer)
{
	if (timer == NULL) {
		err_lib("invalid timer");
		return -1;
	}

	add_timer((struct timer_list *)timer);

	return 0;
}

int fimc_is_timer_disable(void *timer)
{
	if (timer == NULL) {
		err_lib("invalid timer");
		return -1;
	}

	del_timer((struct timer_list *)timer);

	return 0;
}

/*
 * Interrupts
 */
#define INTR_ID_BASE_OFFSET	(3)
#define IRQ_ID_3AA0(x)		(x - (INTR_ID_BASE_OFFSET * 0))
#define IRQ_ID_3AA1(x)		(x - (INTR_ID_BASE_OFFSET * 1))
#define IRQ_ID_ISP0(x)		(x - (INTR_ID_BASE_OFFSET * 2))
#define IRQ_ID_ISP1(x)		(x - (INTR_ID_BASE_OFFSET * 3))
#define valid_3aaisp_intr_index(intr_index) \
	(0 <= intr_index && intr_index < INTR_HWIP_MAX)

int fimc_is_register_interrupt(struct hwip_intr_handler info)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct hwip_intr_handler *pHandler = NULL;
	int intr_index = -1;
	int ret = 0;

	switch (info.chain_id) {
	case ID_3AA_0:
		intr_index = IRQ_ID_3AA0(info.id);
		break;
	case ID_3AA_1:
		intr_index = IRQ_ID_3AA1(info.id);
		break;
	case ID_ISP_0:
		intr_index = IRQ_ID_ISP0(info.id);
		break;
	case ID_ISP_1:
		intr_index = IRQ_ID_ISP1(info.id);
		break;
	default:
		err_lib("invalid chaind_id(%d)", info.chain_id);
		return -EINVAL;
		break;
	}

	if (!valid_3aaisp_intr_index(intr_index)) {
		err_lib("invalid interrupt id chain ID(%d),(%d)(%d)",
			info.chain_id, info.id, intr_index);
		return -EINVAL;
	}
	pHandler = lib->intr_handler_taaisp[info.chain_id][intr_index];

	if (!pHandler) {
		err_lib("Register interrupt error!!:chain ID(%d)(%d)",
			info.chain_id, info.id);
		return 0;
	}

	pHandler->id		= info.id;
	pHandler->priority	= info.priority;
	pHandler->ctx		= info.ctx;
	pHandler->handler	= info.handler;
	pHandler->valid		= true;
	pHandler->chain_id	= info.chain_id;

	fimc_is_log_write("Register interrupt:ID(%d), handler(%p)\n",
		info.id, info.handler);

	return ret;
}

int fimc_is_unregister_interrupt(u32 intr_id, u32 chain_id)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct hwip_intr_handler *pHandler;
	int intr_index = -1;
	int ret = 0;

	switch (chain_id) {
	case ID_3AA_0:
		intr_index = IRQ_ID_3AA0(intr_id);
		break;
	case ID_3AA_1:
		intr_index = IRQ_ID_3AA1(intr_id);
		break;
	case ID_ISP_0:
		intr_index = IRQ_ID_ISP0(intr_id);
		break;
	case ID_ISP_1:
		intr_index = IRQ_ID_ISP1(intr_id);
		break;
	default:
		err_lib("invalid chaind_id(%d)", chain_id);
		return 0;
		break;
	}

	if (!valid_3aaisp_intr_index(intr_index)) {
		err_lib("invalid interrupt id chain ID(%d),(%d)(%d)",
			chain_id, intr_id, intr_index);
		return -EINVAL;
	}

	pHandler = lib->intr_handler_taaisp[chain_id][intr_index];

	pHandler->id		= 0;
	pHandler->priority	= 0;
	pHandler->ctx		= NULL;
	pHandler->handler	= NULL;
	pHandler->valid		= false;
	pHandler->chain_id	= 0;

	fimc_is_log_write("Unregister interrupt:ID(%d)\n", intr_id);

	return ret;
}

int fimc_is_enable_interrupt(u32 interrupt_id)
{
	/* TODO */
	return 0;
}

int fimc_is_disable_interrupt(u32 interrupt_id)
{
	/* TODO */
	return 0;
}

int fimc_is_clear_interrupt(u32 interrupt_id)
{
	/* TODO */
	return 0;
}

/*
 * Spinlock
 */
static spinlock_t svc_slock;
ulong fimc_is_svc_spin_lock_save(void)
{
	ulong flags = 0;

	spin_lock_irqsave(&svc_slock, flags);

	return flags;
}

void fimc_is_svc_spin_unlock_restore(ulong flags)
{
	spin_unlock_irqrestore(&svc_slock, flags);
}

int fimc_is_spin_lock_init(void **slock)
{
	if (*slock == NULL)
		*slock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);

	spin_lock_init((spinlock_t *)*slock);

#ifdef LIB_MEM_TRACK
	add_alloc_track(MT_TYPE_SPINLOCK, (ulong)*slock, sizeof(spinlock_t));
#endif

	return 0;
}

int fimc_is_spin_lock_finish(void *slock_lib)
{
	spinlock_t *slock = NULL;

	if (slock_lib == NULL) {
		err_lib("invalid spinlock");
		return -EINVAL;
	}

	slock = (spinlock_t *)slock_lib;

#ifdef LIB_MEM_TRACK
	add_free_track(MT_TYPE_SPINLOCK, (ulong)slock_lib);
#endif
	kfree(slock_lib);

	return 0;
}

int fimc_is_spin_lock(void *slock_lib)
{
	spinlock_t *slock = NULL;

	BUG_ON(!slock_lib);

	slock = (spinlock_t *)slock_lib;
	spin_lock(slock);

	return 0;
}

int fimc_is_spin_unlock(void *slock_lib)
{
	spinlock_t *slock = NULL;

	BUG_ON(!slock_lib);

	slock = (spinlock_t *)slock_lib;
	spin_unlock(slock);

	return 0;
}

int fimc_is_spin_lock_irq(void *slock_lib)
{
	spinlock_t *slock = NULL;

	BUG_ON(!slock_lib);

	slock = (spinlock_t *)slock_lib;
	spin_lock_irq(slock);

	return 0;
}

int fimc_is_spin_unlock_irq(void *slock_lib)
{
	spinlock_t *slock = NULL;

	BUG_ON(!slock_lib);

	slock = (spinlock_t *)slock_lib;
	spin_unlock_irq(slock);

	return 0;
}

ulong fimc_is_spin_lock_irqsave(void *slock_lib)
{
	spinlock_t *slock = NULL;
	ulong flags = 0;

	BUG_ON(!slock_lib);

	slock = (spinlock_t *)slock_lib;
	spin_lock_irqsave(slock, flags);

	return flags;
}

int fimc_is_spin_unlock_irqrestore(void *slock_lib, ulong flag)
{
	spinlock_t *slock = NULL;

	BUG_ON(!slock_lib);

	slock = (spinlock_t *)slock_lib;
	spin_unlock_irqrestore(slock, flag);

	return 0;
}

int fimc_is_get_nsec(uint64_t *time)
{
	unsigned long long t;
	ulong nanosec_rem;

	t = cpu_clock(UINT_MAX);
	nanosec_rem = do_div(t, 1000000000);
	*time = (uint64_t)nanosec_rem;

	return 0;
}

int fimc_is_get_usec(uint64_t *time)
{
	unsigned long long t;
	ulong nanosec_rem;

	t = cpu_clock(UINT_MAX);
	nanosec_rem = do_div(t, 1000000000);
	*time = (uint64_t)nanosec_rem/1000;

	return 0;
}

/*
 * Sleep
 */
void fimc_is_sleep(u32 msec)
{
	msleep(msec);
}

void fimc_is_res_cache_invalid(ulong kvaddr, u32 size)
{
	vb2_ion_sync_for_device(gPtr_lib_support.fw_cookie,
		kvaddr - gPtr_lib_support.kvaddr,
		size, DMA_FROM_DEVICE);
}

void fimc_is_res_cache_flush(ulong kvaddr, u32 size)
{
	vb2_ion_sync_for_device(gPtr_lib_support.fw_cookie,
		kvaddr - gPtr_lib_support.kvaddr,
		size, DMA_TO_DEVICE);
}

ulong get_reg_addr(u32 id)
{
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	ulong reg_addr = 0;
	u32 hw_id, hw_slot;

	switch(id) {
	case ID_3AA_0:
		hw_id = DEV_HW_3AA0;
		break;
	case ID_3AA_1:
		hw_id = DEV_HW_3AA1;
		break;
	case ID_ISP_0:
		hw_id = DEV_HW_ISP0;
		break;
	case ID_ISP_1:
		hw_id = DEV_HW_ISP1;
		break;
	default:
		warn_lib("get_reg_addr: invalid id(%d)\n", id);
		return 0;
		break;
	}

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_lib("invalid hw_slot (%d) ", hw_slot);
		return 0;
	}

	reg_addr = (ulong)lib->itfc->itf_ip[hw_slot].hw_ip->regs;

	info_lib("get_reg_addr: [ID:%d](0x%lx)\n", hw_id, reg_addr);

	return reg_addr;
}

static void lib_task_work(struct kthread_work *work)
{
	struct fimc_is_task_work *cur_work;

	BUG_ON(!work);

	cur_work = container_of(work, struct fimc_is_task_work, work);

	dbg_lib("do task_work: func(%p), params(%p)\n",
		cur_work->func, cur_work->params);

	cur_work->func(cur_work->params);
}

void lib_task_trigger(struct fimc_is_lib_support *this,
	int priority, void *func, void *params)
{
	struct fimc_is_lib_task *lib_task = NULL;
	u32 index = 0;

	BUG_ON(!this);
	BUG_ON(!func);
	BUG_ON(!params);

	lib_task = &this->task_taaisp[(priority - TASK_PRIORITY_BASE - 1)];
	spin_lock(&lib_task->work_lock);
	lib_task->work[lib_task->work_index % FIMC_IS_MAX_TASK].func = (task_func)func;
	lib_task->work[lib_task->work_index % FIMC_IS_MAX_TASK].params = params;
	lib_task->work_index++;
	index = (lib_task->work_index - 1) % FIMC_IS_MAX_TASK;
	spin_unlock(&lib_task->work_lock);

	queue_kthread_work(&lib_task->worker, &lib_task->work[index].work);
}

void fimc_is_add_task(int priority, void *func, void *param)
{
	BUG_ON(!func);

	dbg_lib("add_task: priority(%d), func(%p), params(%p)\n",
		priority, func, param);

	lib_task_trigger(&gPtr_lib_support, priority, func, param);
}

int lib_get_task_priority(int task_index)
{
	int priority = MAX_RT_PRIO - 1;

	switch (task_index) {
	case TASK_OTF:
		priority = TASK_OTF_PRIORITY;
		break;
	case TASK_AF:
		priority = TASK_AF_PRIORITY;
		break;
	case TASK_ISP_DMA:
		priority = TASK_ISP_DMA_PRIORITY;
		break;
	case TASK_3AA_DMA:
		priority = TASK_3AA_DMA_PRIORITY;
		break;
	case TASK_AA:
		priority = TASK_AA_PRIORITY;
		break;
	default:
		err_lib("Invalid task ID (%d)", task_index);
		break;
	}
	return priority;
}

u32 lib_get_task_affinity(int task_index)
{
	u32 cpu = 0;

	switch (task_index) {
	case TASK_OTF:
		cpu = TASK_OTF_AFFINITY;
		break;
	case TASK_AF:
		cpu = TASK_AF_AFFINITY;
		break;
	case TASK_ISP_DMA:
		cpu = TASK_ISP_DMA_AFFINITY;
		break;
	case TASK_3AA_DMA:
		cpu = TASK_3AA_DMA_AFFINITY;
		break;
	case TASK_AA:
		cpu = TASK_AA_AFFINITY;
		break;
	default:
		err_lib("Invalid task ID (%d)", task_index);
		break;
	}
	return cpu;
}

int lib_task_init(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	char name[30];
	int i, j, ret = 0;
	u32 cpu = 0;

	struct fimc_is_lib_support *lib = &gPtr_lib_support;

	for (i = 0 ; i < FIMC_IS_MAX_TASK_WORKER; i++) {
		spin_lock_init(&lib->task_taaisp[i].work_lock);
		init_kthread_worker(&lib->task_taaisp[i].worker);
		snprintf(name, sizeof(name), "lib_%d_worker", i);
		lib->task_taaisp[i].task = kthread_run(kthread_worker_fn,
							&lib->task_taaisp[i].worker,
							name);
		if (IS_ERR(lib->task_taaisp[i].task)) {
			err_lib("failed to create library task_handler(%d)", i);
			return -ENOMEM;
		}

		/* TODO: consider task priority group worker */
		param.sched_priority = lib_get_task_priority(i);
		ret = sched_setscheduler_nocheck(lib->task_taaisp[i].task, SCHED_FIFO, &param);
		if (ret) {
			err_lib("sched_setscheduler_nocheck(%d) is fail(%d)", i, ret);
			return 0;
		}

		lib->task_taaisp[i].work_index = 0;
		for (j = 0; j < FIMC_IS_MAX_TASK; j++) {
			lib->task_taaisp[i].work[j].func = NULL;
			lib->task_taaisp[i].work[j].params = NULL;
			init_kthread_work(&lib->task_taaisp[i].work[j].work,
					lib_task_work);
		}

#ifdef SET_CPU_AFFINITY
		cpu = lib_get_task_affinity(i);
		ret = set_cpus_allowed_ptr(lib->task_taaisp[i].task, cpumask_of(cpu));
		dbg_lib("lib_task_init: task(%d) affinity cpu(%d) (%d)\n", i, cpu, ret);
#endif
	}

	return ret;
}

int lib_support_init(void)
{
	int ret = 0;
#ifdef LIB_MEM_TRACK
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
#endif

	/* task initialization */
	ret = lib_task_init();
	if (ret) {
		err_lib("lib_support_init: task_init is failed!! (%d)", ret);
		return 0;
	}

#ifdef LIB_MEM_TRACK
	INIT_LIST_HEAD(&lib->list_of_tracks);
	lib->cur_tracks = kzalloc(sizeof(struct lib_mem_tracks), GFP_KERNEL);
	if (lib->cur_tracks)
		list_add(&lib->cur_tracks->list,
			&lib->list_of_tracks);
	else
		err_lib("failed to allocate memory track");
#endif

	return ret;
}

void fimc_is_flush_ddk_thread(void)
{
	int i = 0;
	struct fimc_is_lib_support *lib = &gPtr_lib_support;

	info_lib("%s\n", __func__);

	for (i = 0; i < FIMC_IS_MAX_TASK_WORKER; i++) {
		if (lib->task_taaisp[i].task == NULL)
			err_hw("task is null");
		else
			flush_kthread_worker(&lib->task_taaisp[i].worker);
	}

	return;
}

void fimc_is_load_clear(void)
{
	int i = 0;
	struct fimc_is_lib_support *lib = &gPtr_lib_support;

	lib->binary_load_flg = false;

	if (lib->task_taaisp[0].task == NULL)
		return;

	for (i = 0; i < FIMC_IS_MAX_TASK_WORKER; i++) {
		if (lib->task_taaisp[i].task->state <= 0) {
			warn_lib("kthread state(%ld), exit_code(%d), exit_state(%d)\n",
				lib->task_taaisp[i].task->state,
				lib->task_taaisp[i].task->exit_code,
				lib->task_taaisp[i].task->exit_state);
		}
	}

	return;
}

void check_lib_memory_leak(void)
{
#ifdef LIB_MEM_TRACK
	check_tracks(MT_STATUS_ALLOC);
	free_tracks();
#endif
}

void set_os_system_funcs(os_system_func_t *funcs)
{
	funcs[0] = (os_system_func_t)fimc_is_log_write_console;
#ifdef ENABLE_RESERVED_INTERNAL_DMA
	funcs[1] = (os_system_func_t)fimc_is_alloc_reserved_buffer;
	funcs[2] = (os_system_func_t)fimc_is_free_reserved_buffer;
#else
	funcs[1] = (os_system_func_t)fimc_is_alloc_buffer;
	funcs[2] = (os_system_func_t)fimc_is_free_buffer;
#endif
	funcs[3] = (os_system_func_t)fimc_is_assert;

	funcs[4] = (os_system_func_t)fimc_is_sema_init;
	funcs[5] = (os_system_func_t)fimc_is_sema_finish;
	funcs[6] = (os_system_func_t)fimc_is_sema_up;
	funcs[7] = (os_system_func_t)fimc_is_sema_down;

	funcs[8] = (os_system_func_t)fimc_is_mutex_init;
	funcs[9] = (os_system_func_t)fimc_is_mutex_finish;
	funcs[10] = (os_system_func_t)fimc_is_mutex_lock;
	funcs[11] = (os_system_func_t)fimc_is_mutex_tryLock;
	funcs[12] = (os_system_func_t)fimc_is_mutex_unlock;

	funcs[13] = (os_system_func_t)fimc_is_timer_create;
	funcs[14] = (os_system_func_t)fimc_is_timer_delete;
	funcs[15] = (os_system_func_t)fimc_is_timer_reset;
	funcs[16] = (os_system_func_t)fimc_is_timer_query;
	funcs[17] = (os_system_func_t)fimc_is_timer_enable;
	funcs[18] = (os_system_func_t)fimc_is_timer_disable;

	funcs[19] = (os_system_func_t)fimc_is_register_interrupt;
	funcs[20] = (os_system_func_t)fimc_is_unregister_interrupt;
	funcs[21] = (os_system_func_t)fimc_is_enable_interrupt;
	funcs[22] = (os_system_func_t)fimc_is_disable_interrupt;
	funcs[23] = (os_system_func_t)fimc_is_clear_interrupt;

	funcs[24] = (os_system_func_t)fimc_is_svc_spin_lock_save;
	funcs[25] = (os_system_func_t)fimc_is_svc_spin_unlock_restore;
	funcs[26] = (os_system_func_t)get_random_int;
	funcs[27] = (os_system_func_t)fimc_is_add_task;

	funcs[28] = (os_system_func_t)fimc_is_get_usec;
	funcs[29] = (os_system_func_t)fimc_is_log_write;

	funcs[30] = (os_system_func_t)fimc_is_translate_kva_to_dva;
	funcs[31] = (os_system_func_t)fimc_is_translate_dva_to_kva;
	funcs[32] = (os_system_func_t)fimc_is_sleep;
	funcs[33] = (os_system_func_t)fimc_is_res_cache_invalid;
	funcs[34] = (os_system_func_t)fimc_is_alloc_dma_buffer;
	funcs[35] = (os_system_func_t)fimc_is_free_dma_buffer;

	funcs[36] = (os_system_func_t)fimc_is_spin_lock_init;
	funcs[37] = (os_system_func_t)fimc_is_spin_lock_finish;
	funcs[38] = (os_system_func_t)fimc_is_spin_lock;
	funcs[39] = (os_system_func_t)fimc_is_spin_unlock;
	funcs[40] = (os_system_func_t)fimc_is_spin_lock_irq;
	funcs[41] = (os_system_func_t)fimc_is_spin_unlock_irq;
	funcs[42] = (os_system_func_t)fimc_is_spin_lock_irqsave;
	funcs[43] = (os_system_func_t)fimc_is_spin_unlock_irqrestore;
	funcs[44] = (os_system_func_t)fimc_is_alloc_reserved_buffer;
	funcs[45] = (os_system_func_t)fimc_is_free_reserved_buffer;
	funcs[46] = (os_system_func_t)get_reg_addr;
}

int fimc_is_load_bin(void)
{
	int ret = 0;
	char *lib_isp;
	struct fimc_is_binary bin;
	os_system_func_t os_system_funcs[100];
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
	struct device *device = &gPtr_lib_support.pdev->dev;

	if (lib->binary_load_flg)
		return ret;

	set_os_system_funcs(os_system_funcs);

	/* fixup the memory attribute for every region */
	lib_isp = (char *)LIB_ISP_BASE_ADDR;

#if 0 /* TODO */
	set_memory_xn((ulong)lib_isp, PFN_UP(LIB_ISP_CODE_SIZE));
	set_memory_rw((ulong)lib_isp, PFN_UP(LIB_ISP_CODE_SIZE));
#endif
	memset((void *)lib_isp, 0x00, LIB_ISP_SIZE);

	setup_binary_loader(&bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&bin, FIMC_IS_ISP_LIB_SDCARD_PATH,
						FIMC_IS_ISP_LIB, device);
	if (ret) {
		err_lib("failed to load ISP library (%d)", ret);
		return ret;
	}
	info_lib("binary info - type: C/D, addr: %p, size: 0x%lx from: %s\n",
			lib_isp, bin.size,
			was_loaded_by(&bin) ? "built-in" : "user-provided" );
	memcpy((void *)lib_isp, bin.data, bin.size);
#if 0 /* TODO */
	set_memory_ro((ulong)lib_isp, PFN_UP(LIB_ISP_CODE_SIZE));
	set_memory_x((ulong)lib_isp, PFN_UP(LIB_ISP_CODE_SIZE));
#endif
	fimc_is_ischain_version(FIMC_IS_BIN_LIBRARY, bin.data, bin.size);
	release_binary(&bin);

	((start_up_func_t)lib_isp)((void **)os_system_funcs);

	ret = lib_support_init();
	if (ret < 0) {
		err_lib("lib_support_init failed!! (%d)", ret);
		return ret;
	}
	dbg_lib("lib_support_init success!!\n");

	spin_lock_init(&svc_slock);

	lib->binary_load_flg = true;
	lib->log_ptr = 0;
	lib->base_addr_size = 0;
	if (lib->reserved_buf_size) {
		err_lib("reserved_buf_size is not zero!! (%d)", lib->reserved_buf_size);
		lib->reserved_buf_size = 0;
	}
#ifndef ENABLE_RESERVED_INTERNAL_DMA
	INIT_LIST_HEAD(&lib->dma_buf_list);
#endif
	INIT_LIST_HEAD(&lib->reserved_buf_list);

	info_lib("binary load done\n");

	return 0;
}

int fimc_is_load_vra_bin(void)
{
	int ret = 0;
	char *lib_vra;
	struct fimc_is_binary bin;
	struct device *device = &gPtr_lib_support.pdev->dev;

	if (vra_binary_load)
		return ret;

	/* fixup the memory attribute for every region */
	lib_vra = (char *)LIB_VRA_BINARY_ADDR;

#if 0 /* TODO */
	set_memory_xn((unsigned long)lib_vra, PFN_UP(LIB_VRA_CODE_SIZE));
	set_memory_rw((unsigned long)lib_vra, PFN_UP(LIB_VRA_CODE_SIZE));
#endif

	memset((void *)lib_vra, 0x00, LIB_VRA_SIZE);

	setup_binary_loader(&bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&bin, FIMC_IS_VRA_LIB_SDCARD_PATH,
						FIMC_IS_VRA_LIB, device);
	if (ret) {
		err_lib("failed to load VRA library (%d)", ret);
		ret = -ENOENT;
		return ret;
	}
	info_lib("binary info - type: C/D, addr: %p, size: 0x%lx from: %s\n",
			lib_vra, bin.size,
			was_loaded_by(&bin) ? "built-in" : "user-provided");
	memcpy((void *)lib_vra, bin.data, bin.size);

#if 0 /* TODO */
	set_memory_ro((unsigned long)lib_isp, PFN_UP(LIB_ISP_CODE_SIZE));
	set_memory_x((unsigned long)lib_isp, PFN_UP(LIB_ISP_CODE_SIZE));
#endif

	release_binary(&bin);

	vra_binary_load = true;

	return 0;
}

void fimc_is_load_vra_clear(void)
{
	vra_binary_load = false;
}
