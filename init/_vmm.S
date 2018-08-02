/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/vmm.h>

#define vmm_ELF_PATH "init/vmm.elf"

#define SMC_64BIT_RET_MAGIC 0xC2000401

.global _vmm_goto_EL2
_vmm_goto_EL2:
	smc #0
	isb
	ret

.global _vmm_disable
_vmm_disable:
	ldr x0, =SMC_64BIT_RET_MAGIC
	smc #0
	isb

.section .vmm, "ax"
.global _svmm
_svmm:
.incbin vmm_ELF_PATH
.global _evmm
_evmm:
.section .text
