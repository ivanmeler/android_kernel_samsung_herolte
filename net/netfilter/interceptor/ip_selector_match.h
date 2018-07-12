/**
   @copyright
   Copyright (c) 2013 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#ifndef IP_SELECTOR_MATCH_H
#define IP_SELECTOR_MATCH_H

#include "public_defs.h"
#include "ip_selector.h"

struct IPSelectorFields
{
    uint8_t source_address[16];
    uint8_t destination_address[16];

    int32_t source_port;
    int32_t destination_port;

    int32_t ip_version;
    int32_t ip_protocol;
};


int
ip_selector_match_validate_selector_group(
        const struct IPSelectorGroup *selector_group,
        int bytecount);


int
ip_selector_match_fields_to_group(
        const struct IPSelectorGroup *selector_group,
        const struct IPSelectorFields *selector_fields);


const char *
debug_str_ip_selector_fields(
        void *buf,
        const void *data,
        int len);

void
debug_dump_ip_selector_fields(
        const void *context,
        const void *data,
        unsigned bytecount);

void
debug_dump_ip_selector_group(
        void *context,
        const void *data,
        unsigned bytecount);

void
ip_selector_match_debug_log_fields(
        const struct IPSelectorFields *fields);

void
ip_selector_match_debug_log_group(
        const struct IPSelectorGroup *selector_group);

void
ip_selector_match_debug_log_selector(
        const struct IPSelector *selector);

#endif /* IP_SELECTOR_MATCH_H */
