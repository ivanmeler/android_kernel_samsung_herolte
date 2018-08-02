/**
   @copyright
   Copyright (c) 2011 - 2015, INSIDE Secure Oy. All rights reserved.
*/


#ifndef DEBUG_STRBUF_H
#define DEBUG_STRBUF_H

#ifndef DEBUG_STRBUF_SIZE
#define DEBUG_STRBUF_SIZE 256
#endif

struct DebugStrbuf
{
    int offset;
    char buffer[DEBUG_STRBUF_SIZE];
};

typedef struct DebugStrbuf DegugStrbuf;

void
debug_strbuf_reset(
        struct DebugStrbuf *buf);


char *
debug_strbuf_get_block(
        struct DebugStrbuf *buf,
        int size);


void
debug_strbuf_buffer_get(
        struct DebugStrbuf *buf,
        char **str_p,
        int *len_p);


void
debug_strbuf_buffer_commit(
        struct DebugStrbuf *buf,
        int len);


const char *
debug_strbuf_sshrender(
        DEBUG_STRBUF_TYPE buf,
        int (*ssh_renderer)(unsigned char *, int, int, void *),
        void *render_datum);

const char *
debug_str_hexbuf(
        DEBUG_STRBUF_TYPE buf,
        const void *data,
        int len);


int
format_ipaddress(
        char *buf,
        int len,
        const unsigned char address[16]);

const char *
debug_strbuf_ipaddress(
        DEBUG_STRBUF_TYPE dsb,
        const unsigned char address[16]);

extern const char debug_strbuf_error[];

#endif /* DEBUG_STRBUF_H */
