/*
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors: 	James Gleeson <jagleeso@gmail.com>
 *		Wenbo Shen <wenbo.s@samsung.com>
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
#ifndef __ASM_RKP_CFP_H
#define __ASM_RKP_CFP_H

#ifndef __ASSEMBLY__
#error "Only include this from assembly code"
#endif

#ifdef CONFIG_RKP_CFP_ROPP
#include <linux/rkp_cfp.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>
/*
 * Register aliases.
 */
lr	.req	x30		// link register
#endif

#ifdef CONFIG_RKP_CFP_FIX_SMC_BUG
	/* The smc instruction sets register x17 to zero.
	 * Need to save RRX and RRS, and reload RRX, RRK and RRS
	 * See CONFIG_RKP_CFP_FIX_SMC_BUG in Kconfig for more information 
	 *
	 * NOTE: This macro Must be kept in sync with PRE_SMC_FIX and
 	 * POST_SMC_FIX macro in include/linux/rkp_cfp.h
	 */
#if TI_RRK != TI_RRK_AGAIN
	#error "Need to keep thread_info offsetof(rrk) in sync with TI_RRK_AGAIN (cannot include asm-offsets due to conflicts"
#endif

#ifdef CONFIG_RKP_CFP_ROPP_HYPKEY
	.macro	_smc, imm
	stp	RRX, RRS, [sp, #-16]!
	dsb	sy
	isb
	smc	\imm
	dsb	sy
	isb
	// get_thread_info RRK
	mov	RRX, sp
	and	RRX, RRX, #~(THREAD_SIZE - 1) // top of stack
	// load_key from hypervisor
	stp	x29, x30, [sp, #-16]!
	stp	x0, x1, [sp, #-16]!
	mov	x0, #0x3000
	movk	x0, #0x8389, lsl #16
	mov	x1, RRX
	add	x1, x1, #TI_RRK_AGAIN
	bl	rkp_call
	ldp	x0, x1, [sp], #16
	ldp	x29, x30, [sp], #16
	
	/*load RRX and RRS*/
	ldp	RRX, RRS, [sp], #16
	.endm
#else //CONFIG_RKP_CFP_ROPP_HYPKEY
	.macro	_smc, imm
	stp	RRX, RRS, [sp, #-16]!
	dsb	sy
	isb
	smc	\imm
	dsb	sy
	isb
	// get_thread_info RRK
	mov	RRX, sp
	and	RRX, RRX, #~(THREAD_SIZE - 1) // top of stack
	// load_key RRK
	ldr	RRK, [RRX, #TI_RRK_AGAIN]
	ldp	RRX, RRS, [sp], #16
	.endm
#endif //CONFIG_RKP_CFP_ROPP_HYPKEY


#endif // CONFIG_RKP_CFP_FIX_SMC_BUG

//Use the STP_SPACER macro in assembly files to manually add 2 nop's above stp x29, x30, [...].
#define STP_SPACER \
        nop; \
        nop;

#define LDP_SPACER \
        nop

#endif
