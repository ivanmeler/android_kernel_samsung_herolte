/*
 * Symbol helper for FIPS
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _CRYPTO_FIPS_HELPER_H
#define _CRYPTO_FIPS_HELPER_H

unsigned long get_fips_symbol_address(const char *symname);

#endif	/* _CRYPTO_FIPS_HELPER_H */

