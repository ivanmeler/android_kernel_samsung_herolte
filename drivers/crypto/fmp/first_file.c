/*
 * First file for Exynos FMP FIPS integrity check
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/printk.h> 
#ifdef CONFIG_RELOCATABLE_KERNEL
#include <asm/page.h>
#endif

/* Keep this on top */
#ifdef CONFIG_RELOCATABLE_KERNEL
static const char
builtime_fmp_hmac[128][32] __attribute__((aligned(PAGE_SIZE))) = {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
			   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}};
#else
static const char
builtime_fmp_hmac[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
			   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
#endif

const int first_fmp_rodata = 10;
int first_fmp_data = 20;


void first_fmp_text(void) __attribute__((unused));
void first_fmp_text(void)
{
}


#ifdef CONFIG_RELOCATABLE_KERNEL


#define KERNEL_KASLR_16K_ALGIN

#ifdef  KERNEL_KASLR_16K_ALGIN
#define KASLR_FIRST_SLOT	(0x80080000)
#define KASLR_ALIGN	(0x4000)
#else 
#define KASLR_FIRST_SLOT	(0x85000000)
#define KASLR_ALIGN	(0x200000)
#endif



const char *get_builtime_fmp_hmac(void)
{
	extern u64 *__boot_kernel_offset;
	u64 *kernel_addr = (u64 *) &__boot_kernel_offset;
	u64 offset = kernel_addr[1]  + kernel_addr[0] - KASLR_FIRST_SLOT;
	u64 idx = (offset / KASLR_ALIGN);

	return builtime_fmp_hmac[idx];
}
#else
const char * get_builtime_fmp_hmac (void)
{
	return builtime_fmp_hmac;
}
#endif 


void __init first_fmp_init(void) __attribute__((unused));
void __init first_fmp_init(void)
{
}

void __exit first_fmp_exit(void) __attribute__((unused));
void __exit first_fmp_exit(void)
{
}
