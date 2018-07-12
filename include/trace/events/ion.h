#undef TRACE_SYSTEM
#define TRACE_SYSTEM ion

#if !defined(_TRACE_ION_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ION_H

#include <linux/types.h>
#include <linux/tracepoint.h>

#define show_buffer_flags(flags)			\
	(flags) ? __print_flags(flags, "|",		\
	{(unsigned long) (1 << 0), "cached"},		\
	{(unsigned long) (1 << 1), "needsync"},		\
	{(unsigned long) (1 << 2), "kmap"},		\
	{(unsigned long) (1 << 3), "nozeroed"},		\
	{(unsigned long) (1 << 11), "shrink"},		\
	{(unsigned long) (1 << 12), "migrated"},	\
	{(unsigned long) (1 << 13), "ready"}		\
	) : "noncached"

#define dir_string(dir)					\
	dir == DMA_BIDIRECTIONAL ? "bidirectional" :	\
	dir == DMA_TO_DEVICE ? "to_device" : 		\
	dir == DMA_FROM_DEVICE ? "from_device" :	\
	dir == DMA_NONE ? "none" : "invalid"

DECLARE_EVENT_CLASS(ion_alloc,

	TP_PROTO(const char *client_name,
			unsigned long buffer_id,
			size_t len,
			size_t align,
			unsigned int heap_id_mask,
			unsigned int flags),

	TP_ARGS(client_name, buffer_id, len, align, heap_id_mask, flags),

	TP_STRUCT__entry(
		__field(	const char *,		client_name	)
		__field(	unsigned long,		buffer_id	)
		__field(	size_t,			len		)
		__field(	size_t,			align		)
		__field(	unsigned int,		heap_id_mask	)
		__field(	unsigned int,		flags		)
	),

	TP_fast_assign(
		__entry->client_name	= client_name;
		__entry->buffer_id	= buffer_id;
		__entry->len		= len;
		__entry->align		= align;
		__entry->heap_id_mask	= heap_id_mask;
		__entry->flags		= flags;
	),

	TP_printk("client=%s, buffer=%08lx, len=%zd, "
			"align=%zd, heap_id_mask=%d, flags=%#x(%s)",
			__entry->client_name,
			__entry->buffer_id,
			__entry->len,
			__entry->align,
			__entry->heap_id_mask,
			__entry->flags,
			show_buffer_flags(__entry->flags)
	)
);

DEFINE_EVENT(ion_alloc, ion_alloc_start,

	TP_PROTO(const char *client_name,
			unsigned long buffer_id,
			size_t len,
			size_t align,
			unsigned int heap_id_mask,
			unsigned int flags),

	TP_ARGS(client_name, buffer_id, len, align, heap_id_mask, flags)
);

DEFINE_EVENT(ion_alloc, ion_alloc_end,

	TP_PROTO(const char *client_name,
			unsigned long buffer_id,
			size_t len,
			size_t align,
			unsigned int heap_id_mask,
			unsigned int flags),

	TP_ARGS(client_name, buffer_id, len, align, heap_id_mask, flags)
);

DEFINE_EVENT(ion_alloc, ion_alloc_fail,

	TP_PROTO(const char *client_name,
			unsigned long buffer_id,
			size_t len,
			size_t align,
			unsigned int heap_id_mask,
			unsigned int flags),

	TP_ARGS(client_name, buffer_id, len, align, heap_id_mask, flags)
);

DECLARE_EVENT_CLASS(ion_free,

	TP_PROTO(unsigned long buffer_id, size_t len, bool shrinker),

	TP_ARGS(buffer_id, len, shrinker),

	TP_STRUCT__entry(
		__field(	unsigned long,		buffer_id	)
		__field(	size_t,			len		)
		__field(	bool,			shrinker	)
	),

	TP_fast_assign(
		__entry->buffer_id	= buffer_id;
		__entry->len		= len;
		__entry->shrinker	= shrinker;
	),

	TP_printk("buffer=%08lx, len=%zd, shrinker=%s",
			__entry->buffer_id,
			__entry->len,
			__entry->shrinker ? "yes" : "no"
	)
);

DEFINE_EVENT(ion_free, ion_free_start,

	TP_PROTO(unsigned long buffer_id, size_t len, bool shrinker),

	TP_ARGS(buffer_id, len, shrinker)
);

DEFINE_EVENT(ion_free, ion_free_end,

	TP_PROTO(unsigned long buffer_id, size_t len, bool shrinker),

	TP_ARGS(buffer_id, len, shrinker)
);

DECLARE_EVENT_CLASS(ion_mmap,

	TP_PROTO(unsigned long buffer_id, size_t len, bool faultmap),

	TP_ARGS(buffer_id, len, faultmap),

	TP_STRUCT__entry(
		__field(	unsigned long,		buffer_id	)
		__field(	size_t,			len		)
		__field(	bool,			faultmap	)
	),

	TP_fast_assign(
		__entry->buffer_id	= buffer_id;
		__entry->len		= len;
		__entry->faultmap	= faultmap;
	),

	TP_printk("buffer=%08lx, len=%zd, faultmap=%s",
			__entry->buffer_id,
			__entry->len,
			__entry->faultmap ? "yes" : "no"
	)
);

DEFINE_EVENT(ion_mmap, ion_mmap_start,

	TP_PROTO(unsigned long buffer_id, size_t len, bool faultmap),

	TP_ARGS(buffer_id, len, faultmap)
);

DEFINE_EVENT(ion_mmap, ion_mmap_end,

	TP_PROTO(unsigned long buffer_id, size_t len, bool faultmap),

	TP_ARGS(buffer_id, len, faultmap)
);

TRACE_EVENT(ion_shrink,

	TP_PROTO(unsigned long nr_to_scan, unsigned long freed),

	TP_ARGS(nr_to_scan, freed),

	TP_STRUCT__entry(
		__field(	unsigned long,		nr_to_scan	)
		__field(	unsigned long,		freed		)
	),

	TP_fast_assign(
		__entry->nr_to_scan	= nr_to_scan;
		__entry->freed		= freed;
	),

	TP_printk("nr_to_scan=%lu, freed=%lu",
			__entry->nr_to_scan,
			__entry->freed
	)
);

DECLARE_EVENT_CLASS(ion_sync,

	TP_PROTO(unsigned long caller,
			struct device *dev,
			enum dma_data_direction dir,
			size_t size,
			void *vaddr,
			off_t offset,
			bool flush_all),

	TP_ARGS(caller, dev, dir, size, vaddr, offset, flush_all),

	TP_STRUCT__entry(
		__field(	unsigned long,		 caller		)
		__field(	struct device *,	 dev		)
		__field(	enum dma_data_direction, dir		)
		__field(	size_t,			 size		)
		__field(	void *,			 vaddr		)
		__field(	off_t,			 offset		)
		__field(	bool,			 flush_all	)
	),

	TP_fast_assign(
		__entry->caller		= caller;
		__entry->dev		= dev;
		__entry->dir		= dir;
		__entry->size		= size;
		__entry->vaddr		= vaddr;
		__entry->offset		= offset;
		__entry->flush_all	= flush_all;
	),

	TP_printk("caller=%ps, dev=%s, dir=%s, size=%zd, "
			"va=%p, offs=%ld, all=%s",
			&__entry->caller,
			dev_name(__entry->dev),
			dir_string(__entry->dir),
			__entry->size,
			__entry->vaddr,
			__entry->offset,
			__entry->flush_all ? "yes" : "no"
	)
);

DEFINE_EVENT(ion_sync, ion_sync_start,

	TP_PROTO(unsigned long caller,
			struct device *dev,
			enum dma_data_direction dir,
			size_t size,
			void *vaddr,
			off_t offset,
			bool flush_all),

	TP_ARGS(caller, dev, dir, size, vaddr, offset, flush_all)
);

DEFINE_EVENT(ion_sync, ion_sync_end,

	TP_PROTO(unsigned long caller,
			struct device *dev,
			enum dma_data_direction dir,
			size_t size,
			void *vaddr,
			off_t offset,
			bool flush_all),

	TP_ARGS(caller, dev, dir, size, vaddr, offset, flush_all)
);

#endif /* _TRACE_ION_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
