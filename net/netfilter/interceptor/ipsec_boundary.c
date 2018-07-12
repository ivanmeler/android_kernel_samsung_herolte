/**
   @copyright
   Copyright (c) 2014 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include "ipsec_boundary.h"
#include "implementation_defs.h"

bool
ipsec_boundary_is_valid_spec(
        const char *ipsec_boundary_spec)
{
    bool valid = true;

    const char *p = ipsec_boundary_spec;

    if (p == NULL)
    {
        valid = false;
    }

    while (valid == true && *p != 0)
    {
        const char *end;

        switch (*p)
        {
          case '+':
          case '-':
            break;

          default:
            valid = false;
            break;
        }

        p++;

        for (end = p; *end != 0 && *end != ',' && *end != '*'; end++)
          ;

        if (*end == '*')
        {
            end++;
        }

        if (end - p == 0)
        {
            valid = false;
        }

        p = end;

        if (*p != 0)
        {
            if (*p != ',')
            {
                valid = false;
            }
            p++;
        }
    }

    return valid;
}


bool
ipsec_boundary_is_protected_interface(
        const char *ipsec_boundary_spec,
        const char *interface_name)
{
    const char *p = ipsec_boundary_spec;
    bool is_protected = false;

    while (*p != 0)
    {
        bool is_this_protected = false;
        bool match = true;
        const char *n = interface_name;
        const char *end;

        switch (*p)
        {
          case '+':
            is_this_protected = true;
            break;

          case '-':
            break;

          default:
            ASSERT(0);
        }

        p++;

        for (end = p; *end != 0 && *end != ',' && *end != '*'; end++)
        {
            if (*end != *n)
            {
                match = false;
            }

            if (*n != 0)
            {
                n++;
            }
        }

        if (*n != 0 && *end != '*')
        {
            match = false;
        }

        if (match == true)
        {
            is_protected = is_this_protected;
            break;
        }

        p = end;
        if (*p == '*')
        {
            p++;
        }

        if (*p)
        {
            p++;
        }
    }

    return is_protected;
}
