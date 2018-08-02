/**
   @copyright
   Copyright (c) 2013 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include "implementation_defs.h"
#include "ip_selector_db.h"
#include "ip_selector_match.h"

#define __DEBUG_MODULE__ ipselectordb

void
ip_selector_db_init(
        struct IPSelectorDb *db)
{
    memset(db, 0, sizeof *db);
}


int
ip_selector_db_entry_check(
        const struct IPSelectorDbEntry *entry,
        uint32_t length)
{
    const struct IPSelectorGroup *selector_group = (const void *)(entry + 1);
    const int bytecount = length - sizeof *entry;

    return
        ip_selector_match_validate_selector_group(
                selector_group,
                bytecount);
}


void
ip_selector_db_entry_add(
        struct IPSelectorDb *db,
        int part,
        struct IPSelectorDbEntry *entry,
        int precedence)
{
    struct IPSelectorDbEntry **loc;
    const int insertion_priority =
        entry->priority + (precedence != 0 ? 0 : 1);

    loc = &db->heads[part];
    while (*loc != NULL)
    {
        struct IPSelectorDbEntry *tmp = *loc;

        if (tmp->priority >= insertion_priority)
        {
            break;
        }

        loc = &tmp->next;
    }

    entry->next = *loc;
    *loc = entry;

    DEBUG_DUMP(
            db,
            debug_dump_ip_selector_group,
            (struct IPSelectorGroup *)(entry + 1),
            ((struct IPSelectorGroup *)(entry + 1))->bytecount,
            "Inserted to db %p part %d; Entry %d "
            "action %d priority %d precedence %d:",
            db,
            part,
            entry->id,
            entry->action,
            entry->priority,
            precedence);
}


struct IPSelectorDbEntry *
ip_selector_db_entry_remove(
        struct IPSelectorDb *db,
        int part,
        uint32_t id)
{
    struct IPSelectorDbEntry *removed = NULL;
    struct IPSelectorDbEntry **loc;

    loc = &db->heads[part];

    while (*loc != NULL)
    {
        struct IPSelectorDbEntry *tmp = *loc;
        if (tmp->id == id)
        {
            removed = tmp;
            *loc = tmp->next;
            break;
        }

        loc = &tmp->next;
    }


    if (removed != NULL)
    {
        DEBUG_DUMP(
                db,
                debug_dump_ip_selector_group,
                (struct IPSelectorGroup *)(removed + 1),
                ((struct IPSelectorGroup *)(removed + 1))->bytecount,
                "Removed from db %p part %d; Entry %d "
                "action %d priority %d:",
                db,
                part,
                removed->id,
                removed->action,
                removed->priority);
    }
    else
    {
        DEBUG_FAIL(
                db,
                "Remove failed for db part %d id %d.",
                part,
                id);
    }

    return removed;
}


struct IPSelectorDbEntry *
ip_selector_db_entry_remove_next(
        struct IPSelectorDb *db)
{
    struct IPSelectorDbEntry *removed = NULL;
    struct IPSelectorDbEntry **loc = NULL;

    int i;

    for (i = 0; i < IP_SELECTOR_DB_PARTS; i++)
    {
        if (db->heads[i] != NULL)
        {
            loc = &db->heads[i];
            break;
        }
    }

    if (loc != NULL)
    {
        removed = *loc;
        *loc = removed->next;
    }

    return removed;
}


int
ip_selector_db_lookup(
        struct IPSelectorDb *db,
        int part,
        const struct IPSelectorFields *fields)
{
    struct IPSelectorDbEntry *entry = db->heads[part];
    int action = -1;

    DEBUG_LOW(
            lookup,
            "Fields %p: Lookup db part %d: %s",
            fields,
            part,
            debug_str_ip_selector_fields(
                    DEBUG_STRBUF_GET(),
                    fields,
                    sizeof *fields));

    if (entry == NULL)
    {
        DEBUG_LOW(
                lookup,
                "Fields %p: Lookup db part %d: no entries; no match",
                fields,
                part);
    }

    while (entry != NULL)
    {
        struct IPSelectorGroup *selector_group = (void *)(entry + 1);

        if (ip_selector_match_fields_to_group(selector_group, fields))
        {
            action = entry->action;

            DEBUG_LOW(lookup, "Fields %p matched entry %p.", fields, entry);
            break;
        }

        entry = entry->next;
    }

    DEBUG_LOW(lookup, "Fields %p action is %d.", fields, action);

    return action;
}

