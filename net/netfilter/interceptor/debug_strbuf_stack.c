/**
   @copyright
   Copyright (c) 2012 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include "implementation_defs.h"
#include "debug_strbuf.h"

const char debug_strbuf_error[] = " [ERROR: DebugStrbuf out of memory!] ";

void
debug_strbuf_reset(
        struct DebugStrbuf *buf)
{
    buf->offset = 0;
    buf->buffer[0] = 0;
}


char *
debug_strbuf_get_block(
        struct DebugStrbuf *buf,
        int size)
{
    char *p = NULL;

    if (size + 1 < (sizeof buf->buffer) - buf->offset)
    {
        p = buf->buffer + buf->offset;
        buf->offset += size + 1;
    }

    return p;
}


void
debug_strbuf_buffer_get(
        struct DebugStrbuf *buf,
        char **str_p,
        int *len_p)
{
    *str_p = buf->buffer + buf->offset;
    *len_p = (sizeof buf->buffer) - buf->offset;
}


void
debug_strbuf_buffer_commit(
        struct DebugStrbuf *buf,
        int len)
{
    ASSERT(buf->offset + len <= sizeof buf->buffer);

    buf->offset += len;
}
