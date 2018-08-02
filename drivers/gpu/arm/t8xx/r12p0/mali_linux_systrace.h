#if !defined(_MALI_SYSTRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MALI_SYSTRACE_H

#include <linux/stringify.h>
#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mali-systrace
#define TRACE_SYSTEM_STRING __stringify(TRACE_SYSTEM)
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mali_linux_systrace


/**
 * mali_job_slots_event - called from mali_kbase_core_linux.c
 * @event_id: ORed together bitfields representing a type of event, made with the GATOR_MAKE_EVENT() macro.
 */
TRACE_EVENT(mali_job_systrace_event_start,

        TP_PROTO(char *ev, unsigned int tgid, unsigned int pid, unsigned char job_id, unsigned int ctx_id,
                 unsigned long cookies, unsigned long long start_timestamp, unsigned int dep_0_id, unsigned int dep_0_type,
                 unsigned int dep_1_id , unsigned int dep_1_type, unsigned int gles_ctx_handle, unsigned int frame_number,
                 void* surfacep),
        TP_ARGS(ev, tgid, pid, job_id, ctx_id, cookies, start_timestamp, dep_0_id, dep_0_type, dep_1_id, dep_1_type, gles_ctx_handle, frame_number, surfacep),
        TP_STRUCT__entry(
                            __string(job_name, ev)
                            __field(unsigned int, tgid)
                            __field(unsigned int, pid)
                            __field(unsigned char, job_id)
                            __field(unsigned int, ctx_id)
                            __field(unsigned long, cookies)
                            __field(unsigned long long, start_timestamp)
                            __field(unsigned int, dep_0_id)
                            __field(unsigned int, dep_0_type)
                            __field(unsigned int, dep_1_id)
                            __field(unsigned int, dep_1_type)
                            __field(unsigned int, gles_ctx_handle)
                            __field(unsigned int, frame_number)
                            __field(void*, surfacep)
        ),
        TP_fast_assign(
                            __assign_str(job_name, ev);
                            __entry->tgid = tgid;
                            __entry->pid = pid;
                            __entry->job_id = job_id;
                            __entry->ctx_id = ctx_id;
                            __entry->cookies = cookies;
                            __entry->start_timestamp = start_timestamp;
                            __entry->dep_0_id = dep_0_id;
                            __entry->dep_0_type = dep_0_type;
                            __entry->dep_1_id = dep_1_id;
                            __entry->dep_1_type = dep_1_type;
                            __entry->gles_ctx_handle = gles_ctx_handle;
                            __entry->frame_number = frame_number;
                            __entry->surfacep = surfacep;
        ),

        TP_printk("tracing_mark_write: B|%u|%s Job Queue START|JobType=%s;UniqueID=%llu;FrameNo=%u;Surface=%p;GLContext=%x;JobId=%u;Dep0ID=%u;Dep0Type=%u;Dep1ID=%u;Dep1Type=%u\n",
                            __entry->tgid, __get_str(job_name),__get_str(job_name),
                            __entry->start_timestamp, __entry->frame_number, __entry->surfacep, __entry->gles_ctx_handle,
                            __entry->job_id, __entry->dep_0_id, __entry->dep_0_type, __entry->dep_1_id, __entry->dep_1_type)
    );


TRACE_EVENT(mali_job_systrace_event_end,

        TP_PROTO(char *ev, unsigned int tgid),
        TP_ARGS(ev, tgid),
        TP_STRUCT__entry(
                            __string(job_name, ev)
                            __field(unsigned int, tgid)
        ),
        TP_fast_assign(
                            __assign_str(job_name, ev);
                            __entry->tgid = tgid;
        ),

        TP_printk("tracing_mark_write: E|%u|%s\n", __entry->tgid, __get_str(job_name) )
    );


TRACE_EVENT(mali_job_systrace_event_stop,

        TP_PROTO(char *ev, unsigned int tgid, unsigned int pid, unsigned char job_id, unsigned int ctx_id,
                 unsigned long cookies, unsigned long long start_timestamp, unsigned int dep_0_id, unsigned int dep_0_type,
                 unsigned int dep_1_id , unsigned int dep_1_type, unsigned int gles_ctx_handle, unsigned int frame_number,
                 void* surfacep),
        TP_ARGS(ev, tgid, pid, job_id, ctx_id, cookies, start_timestamp, dep_0_id, dep_0_type, dep_1_id, dep_1_type, gles_ctx_handle, frame_number, surfacep),
        TP_STRUCT__entry(
                            __string(job_name, ev)
                            __field(unsigned int, tgid)
                            __field(unsigned int, pid)
                            __field(unsigned char, job_id)
                            __field(unsigned int, ctx_id)
                            __field(unsigned long, cookies)
                            __field(unsigned long long, start_timestamp)
                            __field(unsigned int, dep_0_id)
                            __field(unsigned int, dep_0_type)
                            __field(unsigned int, dep_1_id)
                            __field(unsigned int, dep_1_type)
                            __field(unsigned int, gles_ctx_handle)
                            __field(unsigned int, frame_number)
                            __field(void*, surfacep)
        ),
        TP_fast_assign(
                            __assign_str(job_name, ev);
                            __entry->tgid = tgid;
                            __entry->pid = pid;
                            __entry->job_id = job_id;
                            __entry->ctx_id = ctx_id;
                            __entry->cookies = cookies;
                            __entry->start_timestamp = start_timestamp;
                            __entry->dep_0_id = dep_0_id;
                            __entry->dep_0_type = dep_0_type;
                            __entry->dep_1_id = dep_1_id;
                            __entry->dep_1_type = dep_1_type;
                            __entry->gles_ctx_handle = gles_ctx_handle;
                            __entry->frame_number = frame_number;
                            __entry->surfacep = surfacep;
        ),

        TP_printk("tracing_mark_write: B|%u|%s Job Queue END|JobType=%s;UniqueID=%llu;FrameNo=%u;Surface=%p;GLContext=%x;JobId=%u;Dep0ID=%u;Dep0Type=%u;Dep1ID=%u;Dep1Type=%u\n",
                            __entry->tgid, __get_str(job_name),__get_str(job_name),
                            __entry->start_timestamp, __entry->frame_number, __entry->surfacep, __entry->gles_ctx_handle,
                            __entry->job_id, __entry->dep_0_id, __entry->dep_0_type, __entry->dep_1_id, __entry->dep_1_type)


    );

#endif				/*  _MALI_SYSTRACE_H */

#undef TRACE_INCLUDE_PATH
#undef linux
#define TRACE_INCLUDE_PATH .

/* This part must be outside protection */
#include <trace/define_trace.h>
