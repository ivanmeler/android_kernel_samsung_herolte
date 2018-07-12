/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors: 	James Gleeson <jagleeso@gmail.com>
 *		Wenbo Shen <wenbo.s@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <linux/module.h>
#include <asm/stacktrace.h>
#include <asm/insn.h>
#include <linux/rkp_cfp.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/kallsyms.h>

unsigned long dump_stack_dec=0x1;
EXPORT_SYMBOL(dump_stack_dec); //remove this line after LKM test

/*#if defined(CONFIG_BPF) && defined(CONFIG_RKP_CFP_JOPP) */
/*#error "Cannot enable CONFIG_BPF when CONFIG_RKP_CFP_JOPP is on x(see CONFIG_RKP_CFP_JOPP for details))"*/
/*#endif*/

#ifdef CONFIG_RKP_CFP_ROPP_HYPKEY //function define
static void hyp_change_keys(struct task_struct *p){
	/*
	 * Hypervisor generate new key, load key to RRK, 
	 * store enc_key to thread_info
	 */
	unsigned long va, is_current=0x0;

	if (!p) {
		p = current;
		is_current = 0x1;
	}
	va = (unsigned long) &(task_thread_info(p)->rrk);

	rkp_call(CFP_ROPP_NEW_KEY_REENC, va, is_current, 0, 0, 0);

}

#else //function define
static void non_hyp_change_keys(struct task_struct *p){
	unsigned long old_key = 0x0, new_key = 0x0;

	if (!p) {
		p = current;
	}

	old_key = task_thread_info(p)->rrk;
#ifdef CONFIG_RKP_CFP_ROPP_RANDOM_KEY
	asm("mrs %0, cntpct_el0" : "=r" (new_key));
#else
	new_key = 0x0;
#endif
	
	//we can update key now
	task_thread_info(p)->rrk = new_key;
	if (p == current)
		asm("mov x17,  %0" : "=r" (new_key));
}
#endif //function define

void rkp_cfp_ropp_change_keys(struct task_struct *p){
#ifdef CONFIG_RKP_CFP_ROPP_HYPKEY
	hyp_change_keys(p);
#else
	non_hyp_change_keys(p);
#endif
}
