/**
   @copyright
   Copyright (c) 2011 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include "implementation_defs.h"

const char *
debug_str_hexbuf(
        DEBUG_STRBUF_TYPE buf,
        const void *data,
        int len)
{
    int p_len;
    char *p;
    const unsigned char *u_data = data;
    int offset = 0;
    int i;

    DEBUG_STRBUF_BUFFER_GET(buf, &p, &p_len);
    for (i = 0; i < len && offset + 3 < p_len; i++)
    {
        offset += snprintf(p + offset, len - offset, "%02x", u_data[i]);
    }

    if (i < len && offset >= 4)
    {
        offset -= 4;
        offset += snprintf(p + offset, 4,  "...");
    }
    DEBUG_STRBUF_BUFFER_COMMIT(buf, offset);

    return p;
}


const char *
debug_strbuf_sshrender(
        DEBUG_STRBUF_TYPE dsb,
        int (*ssh_renderer)(unsigned char *, int, int, void *),
        void *render_datum)
{
    char *p = DEBUG_STRBUF_ALLOC(dsb, 32);

    if (p != NULL)
    {
        ssh_renderer((unsigned char *) p, 32, -1, render_datum);

        return p;
    }

    return DEBUG_STRBUF_ERROR;
}




static void
longest_zero_pairs(
        const unsigned char address[16],
        int *start_p,
        int *length_p)
{
    int longest_length = 0;
    int longest_start = 0;
    int start = -1;
    int length = 0;
    int i;

    for (i = 0; i < 16; i += 2)
    {
        const bool is_zero = (address[i] == 0 && address[i + 1] == 0);

        if (is_zero)
        {
            if (start < 0)
            {
                start = i;
            }

            length += 2;
        }

        if (start >= 0)
        {
            if (length > longest_length)
            {
                longest_length = length;
                longest_start = start;
            }
        }

        if (is_zero == false)
        {
            start = -1;
            length = 0;
        }
    }

    *start_p = longest_start;
    *length_p = longest_length;
}


static int
format_ip6_address(
        const unsigned char address[16],
        int zero_start,
        int zero_length,
        char *p,
        int p_len)
{
    int address_index;
    int i = 0;

    for (address_index = 0; address_index <= 14; )
    {
        if (address_index != 0)
        {
            i += snprintf(p + i, p_len - i, ":");
        }

        if (address_index == zero_start && zero_length != 0)
        {
            if (address_index == 0)
            {
                i += snprintf(p + i, p_len - i, ":");
            }

            address_index += zero_length;

            if (address_index == 16)
            {
                i += snprintf(p + i, p_len - i, ":");
            }
        }
        else
        {
            const int a1 = address[address_index];
            const int a2 = address[address_index + 1];

            if (a1 != 0)
            {
                i += snprintf(p + i, p_len - 1, "%x%.2x", a1, a2);
            }
            else
            {
                i += snprintf(p + i, p_len - 1, "%x", a2);
            }

            address_index += 2;
        }
    }

    return i;
}


const char *
debug_strbuf_ipaddress(
        DEBUG_STRBUF_TYPE buf,
        const unsigned char address[16])
{
    int used;
    char *p;
    int p_len;

    DEBUG_STRBUF_BUFFER_GET(buf, &p, &p_len);

    used = format_ipaddress(p, p_len, address);

    DEBUG_STRBUF_BUFFER_COMMIT(buf, used + 1);

    return p;
}


int
format_ipaddress(
        char *buf,
        int len,
        const unsigned char address[16])
{
    int zero_start;
    int zero_length;
    int used;

    longest_zero_pairs(address, &zero_start, &zero_length);

    if (zero_start == 0 && zero_length == 12)
    {
        const int a1 = address[12 + 0];
        const int a2 = address[12 + 1];
        const int a3 = address[12 + 2];
        const int a4 = address[12 + 3];

        used = snprintf(buf, len, "::%d.%d.%d.%d", a1, a2, a3, a4);
    }
    else
    {
        used = format_ip6_address(address, zero_start, zero_length, buf, len);
    }

    return used;
}

