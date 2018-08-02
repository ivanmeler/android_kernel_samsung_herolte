/**
   @copyright
   Copyright (c) 2014, INSIDE Secure Oy. All rights reserved.
*/

#ifndef IPSEC_BOUNDARY_H
#define IPSEC_BOUNDARY_H

#include "public_defs.h"


bool
ipsec_boundary_is_valid_spec(
        const char *ipsec_boundary_spec);

bool
ipsec_boundary_is_protected_interface(
        const char *ipsec_boundary_spec,
        const char *interface_name);

#endif /* IPSEC_BOUNDARY_H */
