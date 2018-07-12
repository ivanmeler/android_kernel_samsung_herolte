/*
 * Symbol helper for FMP
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>

extern void first_fmp_text(void);
extern void first_fmp_init(void);
extern const int first_fmp_rodata;
extern void last_fmp_text(void);
extern const int last_fmp_rodata;
extern void last_fmp_init(void);

unsigned long get_fmp_symbol_address(const char *symname)
{
	if (!strcmp(symname, "first_fmp_text"))
		return (unsigned long)first_fmp_text;
	else if (!strcmp(symname, "last_fmp_text"))
		return (unsigned long)last_fmp_text;
	else if (!strcmp(symname, "first_fmp_rodata"))
		return (unsigned long)&first_fmp_rodata;
	else if (!strcmp(symname, "last_fmp_rodata"))
		return (unsigned long)&last_fmp_rodata;
	else if (!strcmp(symname, "first_fmp_init"))
		return (unsigned long)first_fmp_init;
	else if (!strcmp(symname, "last_fmp_init"))
		return (unsigned long)last_fmp_init;

	return 0;
}

