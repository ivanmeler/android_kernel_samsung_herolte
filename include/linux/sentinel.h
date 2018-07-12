/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Memory Protection Watchman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#define SENTINEL_BANK_SIZE	UL(0x40000000)
#define SENTINEL_MEMBLOCK_SIZE UL(SENTINEL_BANK_SIZE * 2)

void __init *sentinel_early_alloc(unsigned long sz, unsigned long addr);
void sentinel_install_cctv(unsigned long where, unsigned long count);
void sentinel_uninstall_cctv(unsigned long where, unsigned long count);

#ifdef SENTINEL_FULL_FUNCTION
bool sentinel_is_installed_cctv(unsigned long addr);
void sentinel_install_slab_cctv(unsigned long where, unsigned long count, const char * name);
void __init sentinel_reserve(void);
int __init sentinel_init(void);
bool sentinel_sanity_check(struct pt_regs *regs, unsigned long addr);
void sentinel_early_init(void);
void sentinel_set_prohibited(void *addr, size_t size);
void sentinel_set_allowed(void *addr, size_t size);
#endif
