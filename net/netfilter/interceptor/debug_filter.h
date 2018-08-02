/**
   @copyright
   Copyright (c) 2011 - 2015, INSIDE Secure Oy. All rights reserved.
*/


#ifndef DEBUG_FILTER_H
#define DEBUG_FILTER_H

#include "public_defs.h"

#define DEBUG_MAX_FILTER_COUNT 64

#define DEBUG_MAX_FILTERSTRING_LEN 32

void
debug_filter_reset(
          void);

void
debug_filter_add(
        bool log,
        const char *level,
        const char *flow,
        const char *module,
        const char *file,
        const char *func);

bool
debug_filter(
        const char *level,
        const char *flow,
        const char *module,
        const char *file,
        const char *func);

void
debug_filter_set_string(
        const char *str);

#endif /* DEBUG_FILTER_H */
