/*
 * Last file for Exynos FMP FIPS integrity check
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>

const int last_fmp_rodata = 1000;
int last_fmp_data   = 2000;

void last_fmp_text(void) __attribute__((unused));
void last_fmp_text (void)
{
}

void __init last_fmp_init(void) __attribute__((unused));
void __init last_fmp_init(void)
{
}

void __exit last_fmp_exit(void) __attribute__((unused));
void __exit last_fmp_exit(void)
{
}
