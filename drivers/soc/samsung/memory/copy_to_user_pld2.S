/*
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

#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 * Copy to user space from a kernel buffer (alignment handled by the hardware)
 *
 * Parameters:
 *	x0 - to
 *	x1 - from
 *	x2 - n
 * Returns:
 *	x0 - bytes not copied
 */

#include <asm/cache.h>

dst	.req	x0
src	.req	x1
count	.req	x2
reg1	.req	x3
reg2	.req	x4
end	.req	x5

ENTRY(__copy_to_user_pld2)
	add	x5, x0, x2		// upper user buffer boundary

	mrs	x3, midr_el1
	ubfx	x3, x3, #24, #8
	cmp	x3, #0x53
	b.ne	.Loriginal_copy		// jump to original copy if not mngs

	cmp	count, #64
	b.ge	.Lcpy_over_64

.Ltail63:
	ands	x3, count, #0x30
	b.eq	.Ltiny15
	cmp	w3, #0x20
	b.eq	1f
	b.lt	2f
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
1:
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
2:
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)

.Ltiny15:
	tbz	count, #3, 1f
	ldr	x3, [src], #8
USER(9f,str	x3, [dst], #8)
1:
	tbz	count, #2, 2f
	ldr	w3, [src], #4
USER(9f,str	w3, [dst], #4)
2:
	tbz	count, #1, 3f
	ldrh	w3, [src], #2
USER(9f,strh	w3, [dst], #2)
3:
	tbz	count, #0, .Lexitfunc
	ldrb	w3, [src]
USER(9f,strb	w3, [dst])

.Lexitfunc:
	mov	x0, #0
	ret

.Lcpy_over_64:
	subs	count, count, #128
	b.ge	.Lcpy_body_large
1:
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
	subs	count, count, #64
	b.ge	1b
	tst	count, #0x3f
	b.ne	.Ltail63
	mov	x0, #0
	ret

	/*
	* Critical loop.  Start at a new cache line boundary.  Assuming
	* 64 bytes per line this ensures the entire loop is in one line.
	*/
	.p2align	L1_CACHE_SHIFT
.Lcpy_body_large:
	add	count, count, #64
1:
	prfm	pldl1strm, [src, #(64*4)]
	prfm	pldl2keep, [src, #(64*24)]
	prfm	pldl2keep, [src, #(64*36)]
	prfm	pldl2keep, [src, #(64*48)]
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
	ldp	x3, x4, [src], #16
USER(9f,stp	x3, x4, [dst], #16)
	subs	count, count, #64
	b.ge	1b
	tst	count, #0x3f
	b.ne	.Ltail63
	mov	x0, #0
	ret

.Loriginal_copy:
	subs	x2, x2, #8
	b.mi	2f
1:
	ldr	x3, [x1], #8
	subs	x2, x2, #8
USER(9f, str	x3, [x0], #8	)
	b.pl	1b
2:	adds	x2, x2, #4
	b.mi	3f
	ldr	w3, [x1], #4
	sub	x2, x2, #4
USER(9f, str	w3, [x0], #4	)
3:	adds	x2, x2, #2
	b.mi	4f
	ldrh	w3, [x1], #2
	sub	x2, x2, #2
USER(9f, strh	w3, [x0], #2	)
4:	adds	x2, x2, #1
	b.mi	5f
	ldrb	w3, [x1]
USER(9f, strb	w3, [x0]	)
5:	mov	x0, #0
	ret
ENDPROC(__copy_to_user_pld2)

	.section .fixup,"ax"
	.align	2
9:	sub	x0, x5, x0			// bytes not copied
	ret
	.previous
