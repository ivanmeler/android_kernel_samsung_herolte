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
#include <linux/kernel.h>

extern void first_crypto_text(void);
extern void first_crypto_init(void);
extern const int first_crypto_rodata;
extern void last_crypto_text(void);
extern const int last_crypto_rodata;
extern void last_crypto_init(void);
extern const int first_crypto_asm_rodata;
extern void first_crypto_asm_text(void);
extern void first_crypto_asm_init(void);
extern const int last_crypto_asm_rodata;
extern void last_crypto_asm_text(void);
extern void last_crypto_asm_init(void);

unsigned long get_fips_symbol_address(const char *symname)
{
	if (!strcmp(symname, "first_crypto_text"))
		return (unsigned long)first_crypto_text;
	else if (!strcmp(symname, "last_crypto_text"))
		return (unsigned long)last_crypto_text;
	else if (!strcmp(symname, "first_crypto_rodata"))
		return (unsigned long)&first_crypto_rodata;
	else if (!strcmp(symname, "last_crypto_rodata"))
		return (unsigned long)&last_crypto_rodata;
	else if (!strcmp(symname, "first_crypto_init"))
		return (unsigned long)first_crypto_init;
	else if (!strcmp(symname, "last_crypto_init"))
		return (unsigned long)last_crypto_init;
	else if (!strcmp(symname, "first_crypto_asm_text"))
		return (unsigned long)first_crypto_asm_text;
	else if (!strcmp(symname, "last_crypto_asm_text"))
		return (unsigned long)last_crypto_asm_text;
	else if (!strcmp(symname, "first_crypto_asm_rodata"))
		return (unsigned long)&first_crypto_asm_rodata;
	else if (!strcmp(symname, "last_crypto_asm_rodata"))
		return (unsigned long)&last_crypto_asm_rodata;
	else if (!strcmp(symname, "first_crypto_asm_init"))
		return (unsigned long)first_crypto_asm_init;
	else if (!strcmp(symname, "last_crypto_asm_init"))
		return (unsigned long)last_crypto_asm_init;

	return 0;
}

