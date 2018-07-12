/**
   @copyright
   Copyright (c) 2011 - 2015, INSIDE Secure Oy. All rights reserved.
*/


#ifndef DEBUG_IMPLEMENTATION_H
#define DEBUG_IMPLEMENTATION_H

/*
  The header debug_implementation.h must define following macros.

  DEBUG_IMPLEMENTATION(level, flow, module, ...)

  May produce a debug log message based on the arguments and
  definition of __DEBUG_MODULE__. Additionally can use __FILE__,
  __LINE__, and __func__.

  The arguments are

  level    a preprocessing token defining debug level and
  is one of FAIL, HIGH, MEDIUM, LOW

  flow     a preprocessing token defining debug flow and
  may be any valid identifier string

  ...      includes a printf format string and possible
  arguments


  ASSERT_IMPLEMENTATION(condition, description)

  May abort program execution if condition evaluates to false.
  May optionally produce a log message based on arguments.
  The arguments are:

  condition      expression that is asserted to be true
  description    a string describing reason for program abort.


  PANIC_IMPLEMENTATION(...)

  Must always abort program execution. Optionally may produce a log
  message based on arguments.
  The arguments are:
  ...     contains printf style format string and arguments.


  TODO_IMPLEMENTATION(todo)

  Must not produce run time behaviour. May produce a compile warning.
  The arguments are:
  todo     a string describing what needs to be done


  COMPILE_GLOBAL_ASSERT_IMPLEMENTATION(condition)

  Should produce a compile time error if condition evaluates to false
  in global context i.e. not inside a function body.  The arguments are:

  condition     expression that is always based on compile time
  constant values.

  COMPILE_STATIC_ASSERT_IMPLEMENTATION(condition)

  Should produce a compile time error if condition evaluates to false
  in "static" context i.e. within a function body.  The arguments are:

  condition     expression that is always based on compile time
  constant values.
*/


#define STR_BOOL_IMPLEMENTATION(b) ((bool)(b) ? "true" : "false")

#define STR_HEXBUF_IMPLEMENTATION(buf, len)             \
    debug_str_hexbuf(DEBUG_STRBUF_GET(), (buf), (len))


/* __FILELINE__

   Macro __FILELINE__ combines __FILE__ and __LINE__ preprocessor
   macros into a single macro with a string value.
   As an example: If __FILE__ would have value "file.c" and
   __LINE__ value 24, the __FILELINE__ would become "file.c:24".
*/
#define __FILELINE__ __FILE__ ":" L_TOSTRING(__LINE__)


#define __DEBUG_CONCAT(x, y) x ## y
#define DEBUG_CONCAT(x, y) __DEBUG_CONCAT(x,y)

#define DEBUG_OUTPUT_FORMAT_WRAP(arg) DEBUG_OUTPUT_FORMAT arg

#define DEBUG_OUTPUT_FORMAT(func, format, ...)  \
    format "%s\n", func, __VA_ARGS__

#define DEBUG_STRINGIFY(x) #x

#define DEBUG_TOSTRING(x) DEBUG_STRINGIFY(x)

#define DEBUG_FULL_FORMAT_STRING(level, flow, module, file, line, func, ...) \
    DEBUG_OUTPUT_FORMAT_WRAP(                                           \
            (                                                           \
                    func,                                               \
                    DEBUG_STRINGIFY(level) ", "                         \
                    DEBUG_STRINGIFY(flow) ", "                          \
                    DEBUG_STRINGIFY(module) ", "                        \
                    file  ":"                                           \
                    DEBUG_STRINGIFY(line) ": "                          \
                    "%s: "                                              \
                    __VA_ARGS__, ""))


#define DEBUG_STRBUF_TYPE_IMPLEMENTATION        \
    struct DebugStrbuf *


#define DEBUG_STRBUF_GET_IMPLEMENTATION()       \
    &local_debug_str_buf


#define DEBUG_STRBUF_ALLOC_IMPLEMENTATION(dsb, n)       \
    debug_strbuf_get_block((dsb), (n))

#define DEBUG_STRBUF_BUFFER_GET_IMPLEMENTATION(dsb, buf_p, len_p)       \
    debug_strbuf_buffer_get(dsb, buf_p, len_p)

#define DEBUG_STRBUF_BUFFER_COMMIT_IMPLEMENTATION(dsb, len)     \
    debug_strbuf_buffer_commit(dsb, len)

#define DEBUG_STRBUF_ERROR_IMPLEMENTATION       \
    debug_strbuf_error


#ifdef DEBUG_LIGHT

#define ENABLE_ASSERT

#define DEBUG_IMPLEMENTATION(level, flow, ...)          \
    ({                                                  \
        struct DebugStrbuf local_debug_str_buf;         \
                                                        \
        debug_strbuf_reset(&local_debug_str_buf);       \
                                                        \
        debug_outputf(                                  \
                DEBUG_STRINGIFY(level),                 \
                DEBUG_STRINGIFY(flow),                  \
                DEBUG_TOSTRING(__DEBUG_MODULE__),       \
                __FILE__,                               \
                __LINE__,                               \
                __func__,                               \
                __VA_ARGS__);                           \
    })

#define DEBUG_DUMP_IMPLEMENTATION(__flow, __dumper, __data, __len, ...) \
    ({                                                                  \
        struct DebugDumpContext __debug_dump_context;                   \
        __debug_dump_context.level = "DUMP";                            \
        __debug_dump_context.flow = DEBUG_STRINGIFY(__flow);            \
        __debug_dump_context.module = DEBUG_TOSTRING(__DEBUG_MODULE__); \
        __debug_dump_context.file = __FILE__ ;                          \
        __debug_dump_context.line = __LINE__ ;                          \
        __debug_dump_context.func = __func__ ;                          \
                                                                        \
        DEBUG_IMPLEMENTATION(DUMP,__flow, __VA_ARGS__);                 \
        (__dumper)(&__debug_dump_context, __data, __len);               \
    })

#define DEBUG_DUMP_LINE_IMPLEMENTATION(context, ...)            \
    ({                                                          \
        const struct DebugDumpContext *__debug_dump_context_p = \
            context;                                            \
        struct DebugStrbuf local_debug_str_buf;                 \
                                                                \
        debug_strbuf_reset(&local_debug_str_buf);               \
                                                                \
        debug_outputf(                                          \
                __debug_dump_context_p->level,                  \
                __debug_dump_context_p->flow,                   \
                __debug_dump_context_p->module,                 \
                __debug_dump_context_p->file,                   \
                __debug_dump_context_p->line,                   \
                __debug_dump_context_p->func,                   \
                __VA_ARGS__);                                   \
    })

#else

#define DEBUG_IMPLEMENTATION(level, flow, ...)
#define DEBUG_DUMP_IMPLEMENTATION(flow, dumper, data, len, ...)
#define DEBUG_DUMP_LINE_IMPLEMENTATION(context, ...)

#ifdef __COVERITY__
#define ENABLE_ASSERT
#endif

#endif

#ifdef ENABLE_ASSERT
#define ASSERT_IMPLEMENTATION(condition, description)   \
    ((void) ((condition) ? 0 :                          \
             (assert_outputf(                           \
                     DEBUG_STRINGIFY(condition),        \
                     __FILE__,                          \
                     __LINE__,                          \
                     DEBUG_TOSTRING(__DEBUG_MODULE__),  \
                     __func__,                          \
                     description))))

#else
#define ASSERT_IMPLEMENTATION(condition, description)
#endif

#define COMPILE_STATIC_ASSERT_IMPLEMENTATION(condition)         \
    do {                                                        \
        int DEBUG_CONCAT(static_assertion_,__LINE__)            \
            [1-2*(!(condition))] = { 1 };                       \
        if (DEBUG_CONCAT(static_assertion_,__LINE__)[0])        \
            ;                                                   \
    } while (0)


#define COMPILE_GLOBAL_ASSERT_IMPLEMENTATION(condition) \
    extern int DEBUG_CONCAT(global_assert_,__LINE__)[   \
            1 - 2*(!(condition))]


#include "debug_strbuf.h"
#include "debug_outputf.h"


struct DebugDumpContext
{
    const char *level;
    const char *flow;
    const char *module;
    const char *file;
    int line;
    const char *func;
};

typedef void
DebugDumper(
        const struct DebugDumpContext *,
        const void *data,
        unsigned bytecount);

DebugDumper debug_dump_hex_bytes;

void
debug_disable(
        void);

void
debug_enable(
        void);

void
debug_set_filename(
        const char *filename);

void
debug_set_filter_filename(
        const char *filename);

void
debug_set_filter_string(
        const char *debug_string);

void
debug_set_prefix(
        const char *prefix);

#endif /* DEBUG_IMPLEMENTATION_H */
