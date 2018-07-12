/**
   @copyright
   Copyright (c) 2011 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include "implementation_defs.h"
#include "debug_filter.h"

#include <linux/module.h>

extern void
assert_outputf(
        const char *condition,
        const char *file,
        int line,
        const char *module,
        const char *func,
        const char *description)
{
    panic(description);
}


static const char *
last_slash(const char *str)
{
    const char *last = str;

    while (*str != 0)
    {
        if (*str == '/')
          last = str;

        str++;
    }

    return last + 1;
}


void
debug_outputf(
        const char *level,
        const char *flow,
        const char *module,
        const char *file,
        int line,
        const char *func,
        const char *format, ...)
{
    if (debug_filter(level, flow, module, file, func))
    {
        va_list args;

        printk("%s %s %s:%d ", level, module, last_slash(file), line);
        va_start(args, format);
        vprintk(format, args);
        va_end(args);
        printk("\n");
    }
}
