/**
   @copyright
   Copyright (c) 2011 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include "implementation_defs.h"

#include "debug_filter.h"

static int
matches(const char *str, const char *expr)
{
    int wild_head = 0;
    int wild_tail = 0;
    int str_len = strlen(str);
    const char *body_str = NULL;
    int body_len = 0;

    const char *p;

    p = expr;
    while (*p == '*')
    {
        wild_head = 1;

        p++;
    }

    body_str = p;
    while (*p && *p != '*')
    {
        body_len++;
        p++;
    }

    while (*p == '*')
    {
        wild_tail = 1;
        p++;
    }


    if (body_len == 0)
      return 1;

    for (p = str; p < str + str_len; p++)
    {
        int i;

        for (i = 0; i < body_len && p[i] == body_str[i]; i++)
          ;

        if (i == body_len)
          break;
    }

    if (p == str + str_len)
    {
        return 0;
    }

    if (p > str && !wild_head)
    {
        return 0;
    }

    if (p[body_len] != 0 && !wild_tail)
    {
        return 0;
    }

    return 1;
}



struct DebugFilter
{
    bool log;
    char level[DEBUG_MAX_FILTERSTRING_LEN];
    char flow[DEBUG_MAX_FILTERSTRING_LEN];
    char module[DEBUG_MAX_FILTERSTRING_LEN];
    char file[DEBUG_MAX_FILTERSTRING_LEN];
    char func[DEBUG_MAX_FILTERSTRING_LEN];
};

static struct DebugFilter filters[DEBUG_MAX_FILTER_COUNT];
static int filter_count = 0;


void
debug_filter_reset(
        void)
{
    filter_count = 0;
}

void
debug_filter_add(
        bool log,
        const char *level,
        const char *flow,
        const char *module,
        const char *file,
        const char *func)
{
    if (filter_count < DEBUG_MAX_FILTER_COUNT)
    {
        struct DebugFilter *filter = &filters[filter_count];

        memset(filter, 0, sizeof *filter);

        strncpy(filter->level, level, DEBUG_MAX_FILTERSTRING_LEN - 1);
        strncpy(filter->flow, flow, DEBUG_MAX_FILTERSTRING_LEN - 1);
        strncpy(filter->module, module, DEBUG_MAX_FILTERSTRING_LEN - 1);
        strncpy(filter->file, file, DEBUG_MAX_FILTERSTRING_LEN - 1);
        strncpy(filter->func, func, DEBUG_MAX_FILTERSTRING_LEN - 1);
        filter->log = log;

        filter_count++;
    }
}


bool
debug_filter(
        const char *level,
        const char *flow,
        const char *module,
        const char *file,
        const char *func)
{
    int i;

    for (i = 0; i < filter_count; i++)
    {
        const struct DebugFilter *filter = &filters[i];

        if (matches(level, filter->level) &&
            matches(flow, filter->flow) &&
            matches(module, filter->module) &&
            matches(file, filter->file) &&
            matches(func, filter->func))
          return filter->log;
    }

    return false;
}


static int
get_next(const char **pp)
{
    while (**pp == ' ' || **pp == '\t')
      (*pp)++;

    return **pp;
}


void
debug_filter_set_string(
        const char *str)
{
    const char *p = str;

    if (str == NULL)
    {
        return;
    }

    for (filter_count = 0;
         filter_count < DEBUG_MAX_FILTER_COUNT;
         filter_count++)
    {
        struct DebugFilter *filter = &filters[filter_count];

        get_next(&p);
        while (*p == ';')
        {
            p++;
            get_next(&p);
        }

        if (*p == 0)
        {
            break;
        }
        if (*p == '+')
        {
            filter->log = true;
        }
        else
        if (*p == '-')
        {
            filter->log = false;
        }
#if 0
        else
        if (*p == '=')
        {
            p++;
            get_next(&p);
            if (*p == '"')
            {
                char *d = debug_prefix;
                p++;

                while (*p && *p != '"')
                {
                    *d++ = *p++;
                }

                *d = 0;
                if (*p != '"')
                {
                    strcpy(debug_prefix, DEBUG_PREFIX_DEFAULT);
                }
            }
            continue;
        }
#endif
        else
          break;

        p++;

#define READ_STRING(which)                                              \
        { int i = 0;                                                      \
        get_next(&p);                                                     \
        while(i < DEBUG_MAX_FILTERSTRING_LEN - 1 &&                       \
              *p != ',' && *p && *p != ';' && *p != ' ')                  \
          {                                                               \
            filter->which[i++] = *p++;                                    \
          }                                                               \
        filter->which[i] = 0;                                             \
        get_next(&p);                                                     \
        if (*p && *p != ',')                                              \
          continue;                                                       \
        p++; }

        READ_STRING(level);
        READ_STRING(flow);
        READ_STRING(module);
        READ_STRING(file);
        READ_STRING(func);

#undef READ_STRING

        while (*p && *p != ';')
          p++;
    }
}
