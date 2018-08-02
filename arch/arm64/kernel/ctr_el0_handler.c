/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/cpu.h>
#include <asm/cpufeature.h>
#include <asm/traps.h>

static int mrs_ctr_el0_handler(struct pt_regs *regs, u32 instr)
{
	u64 val;

	//register index in instruction is last 5 bits
	int reg = instr & 0x1f;

	pr_debug("mrs_ctr_el0_handler: instr %08x reg x%d\n", instr, reg);

	asm volatile("mrs %0, ctr_el0" : "=r" (val));
	val = val & ~(1 << 0); //clear bit 0 to change CTR_EL0[IminLine] to 0x4
	regs->regs[reg] = val;
	regs->pc += 4;

	return 0;
}

/*
 * Hook to handle ctr_el0 reads from EL0
 */
static struct undef_hook mrs_ctr_el0_hook = {
        .instr_mask = 0xd53b0020,
        .instr_val  = 0xd53b0020,
        .pstate_mask = CPSR_MODE_MASK,
        .pstate_val = CPSR_MODE_USR,
        .fn     = mrs_ctr_el0_handler
};

static int __init ctr_el0_instr_emulator_init(void)
{
	register_undef_hook(&mrs_ctr_el0_hook);
	return 0;
}

late_initcall(ctr_el0_instr_emulator_init);
