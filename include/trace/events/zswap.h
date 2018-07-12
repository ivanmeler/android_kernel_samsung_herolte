#undef TRACE_SYSTEM
#define TRACE_SYSTEM zswap

#if !defined(_TRACE_ZSWAP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ZSWAP_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <trace/events/gfpflags.h>

DECLARE_EVENT_CLASS(mm_zswap_writebackd_template,

	TP_PROTO(unsigned long pool_pages),

	TP_ARGS(pool_pages),

	TP_STRUCT__entry(
		__field(unsigned long, pool_pages)
	),

	TP_fast_assign(
		__entry->pool_pages = pool_pages;
	),

	TP_printk("zswap_pool_pages=%lu", __entry->pool_pages)
);

DEFINE_EVENT(mm_zswap_writebackd_template, mm_zswap_writebackd_sleep,

	TP_PROTO(unsigned long pool_pages),

	TP_ARGS(pool_pages)
);

DEFINE_EVENT(mm_zswap_writebackd_template, mm_zswap_wakeup_writebackd,

	TP_PROTO(unsigned long pool_pages),

	TP_ARGS(pool_pages)
);

DEFINE_EVENT(mm_zswap_writebackd_template, mm_zswap_writebackd_wake,

	TP_PROTO(unsigned long pool_pages),

	TP_ARGS(pool_pages)
);
#endif /* _TRACE_ZSWAP_H */

#include <trace/define_trace.h>
