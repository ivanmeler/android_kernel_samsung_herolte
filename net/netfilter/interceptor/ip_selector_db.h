/**
   @copyright
   Copyright (c) 2013 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#ifndef IP_SELECTOR_DB_H
#define IP_SELECTOR_DB_H

#include "public_defs.h"
#include "ip_selector.h"
#include "ip_selector_match.h"

struct IPSelectorDbEntry
{
    uint32_t action;
    uint32_t id;
    uint32_t priority;

    struct IPSelectorDbEntry *next;
};

#define IP_SELECTOR_DB_PARTS 2

struct IPSelectorDb
{
    struct IPSelectorDbEntry *heads[IP_SELECTOR_DB_PARTS];
};


int
ip_selector_db_entry_check(
        const struct IPSelectorDbEntry *entry,
        uint32_t length);


void
ip_selector_db_init(
        struct IPSelectorDb *db);


int
ip_selector_db_lookup(
        struct IPSelectorDb *db,
        int part,
        const struct IPSelectorFields *fields);

void
ip_selector_db_entry_add(
        struct IPSelectorDb *db,
        int part,
        struct IPSelectorDbEntry *entry,
        int precedence);


struct IPSelectorDbEntry *
ip_selector_db_entry_remove(
        struct IPSelectorDb *db,
        int part,
        uint32_t id);


struct IPSelectorDbEntry *
ip_selector_db_entry_remove_next(
        struct IPSelectorDb *db);


#endif /* IP_SELECTOR_DB_H */
