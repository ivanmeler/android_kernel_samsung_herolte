#undef TRACE_SYSTEM
#define TRACE_SYSTEM hotplug_governor

#if !defined(_TRACE_HOTPLUG_GOVERNOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOTPLUG_GOVERNOR_H

#include <linux/tracepoint.h>

TRACE_EVENT(exynos_hpgov_governor_update,
	    TP_PROTO(int event, int req_cpu_max, int req_cpu_min),
	    TP_ARGS(event, req_cpu_max, req_cpu_min),
	    TP_STRUCT__entry(
		    __field(int, event)
		    __field(int, req_cpu_max)
		    __field(int, req_cpu_min)
	    ),
	    TP_fast_assign(
		    __entry->event = event;
		    __entry->req_cpu_max = req_cpu_max;
		    __entry->req_cpu_min = req_cpu_min;
	    ),
	    TP_printk("event=%d req_cpu_max=%d req_cpu_min=%d",
		    __entry->event, __entry->req_cpu_max, __entry->req_cpu_min)
);

#endif /* _TRACE_HOTPLUG_GOVERNOR_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
