/**
   @copyright
   Copyright (c) 2011 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#ifndef DEBUG_OUTPUTF_H
#define DEBUG_OUTPUTF_H

extern void
assert_outputf(
        const char *condition,
        const char *file,
        int line,
        const char *module,
        const char *func,
        const char *description)
#ifdef __GNUC__
    __attribute__ ((noreturn))
#endif
    ;

extern void
debug_outputf(
        const char *level,
        const char *flow,
        const char *module,
        const char *file,
        int line,
        const char *func,
        const char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 7, 8)))
#endif
    ;

#endif /* DEBUG_OUTPUTF_H */
