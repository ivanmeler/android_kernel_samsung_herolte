/*
 * Based on arch/arm/include/asm/uaccess.h
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __EXYNOS_MEMCPY_H
#define __EXYNOS_MEMCPY_H

#include <linux/compiler.h>

extern void *memcpy_pld2(void *, const void *, __kernel_size_t);
extern unsigned long __must_check __copy_to_user_pld2(void __user *to, const void *from, unsigned long n);

#endif /* __EXYNOS_MEMCPY_H */
