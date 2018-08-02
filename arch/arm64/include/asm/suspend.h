#ifndef __ASM_SUSPEND_H
#define __ASM_SUSPEND_H

#ifdef CONFIG_RKP_CFP
/*  (lr is in struct member pc, not counted in NR_CTX_REGS)
 *
 *  stp	x29, lr, [sp, #-96]!
 *  stp	x19, x20, [sp,#16]
 *  stp	x21, x22, [sp,#32]
 *  stp	x23, x24, [sp,#48]
 *  stp	x25, x26, [sp,#64]
 *  stp	x27, x28, [sp,#80]
 *  stp	RRK, RRS, [sp,#96]
 */
#define NR_CTX_REGS 13
#else
#define NR_CTX_REGS 11
#endif

/*
 * struct cpu_suspend_ctx must be 16-byte aligned since it is allocated on
 * the stack, which must be 16-byte aligned on v8
 */
struct cpu_suspend_ctx {
	/*
	 * This struct must be kept in sync with
	 * cpu_do_{suspend/resume} in mm/proc.S
	 */
	u64 ctx_regs[NR_CTX_REGS];
	u64 sp;
} __aligned(16);

struct sleep_save_sp {
	phys_addr_t *save_ptr_stash;
	phys_addr_t save_ptr_stash_phys;
};

extern int __cpu_suspend(unsigned long arg, int (*fn)(unsigned long));
extern void cpu_resume(void);
extern int cpu_suspend(unsigned long);

#endif
