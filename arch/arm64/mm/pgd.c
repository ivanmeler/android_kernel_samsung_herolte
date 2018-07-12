/*
 * PGD allocation/freeing
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
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

#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/slab.h>

#include <asm/pgalloc.h>
#include <asm/page.h>
#include <asm/tlbflush.h>

#include "mm.h"

#ifdef CONFIG_TIMA_RKP
#include <linux/rkp_entry.h>
extern u8 rkp_started;
#endif /* CONFIG_TIMA_RKP */
#define PGD_SIZE	(PTRS_PER_PGD * sizeof(pgd_t))

static struct kmem_cache *pgd_cache;
#ifndef CONFIG_TIMA_RKP
pgd_t *pgd_alloc(struct mm_struct *mm)
{
	if (PGD_SIZE == PAGE_SIZE)
		return (pgd_t *)get_zeroed_page(GFP_KERNEL);
	else
		return kmem_cache_zalloc(pgd_cache, GFP_KERNEL);
}
#else
pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *ret = NULL;
	ret = (pgd_t *) rkp_ro_alloc();
	if (!ret) {
		if (PGD_SIZE == PAGE_SIZE)
			ret = (pgd_t *)get_zeroed_page(GFP_KERNEL);
		else
			ret = kmem_cache_zalloc(pgd_cache, GFP_KERNEL);
	}

	if(unlikely(!ret)) {
		pr_warn("%s: pgd alloc is failed\n", __func__);
		return ret;
	}

	if (rkp_started)
		rkp_call(RKP_PGD_NEW, (unsigned long)ret, 0, 0, 0, 0);
	return ret;
}
#endif
#ifndef  CONFIG_TIMA_RKP
void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	if (PGD_SIZE == PAGE_SIZE)
		free_page((unsigned long)pgd);
	else
		kmem_cache_free(pgd_cache, pgd);
}
#else
void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{

	if (rkp_started)
		rkp_call(RKP_PGD_FREE, (unsigned long)pgd, 0, 0, 0, 0);
	/* if pgd memory come from read only buffer, the put it back */
	/*TODO: use a macro*/
	if((unsigned long)pgd >= (unsigned long)RKP_RBUF_VA && (unsigned long)pgd < ((unsigned long)RKP_RBUF_VA +  TIMA_ROBUF_SIZE))
		rkp_ro_free((void*)pgd);
	else
	{
		if (PGD_SIZE == PAGE_SIZE)
			free_page((unsigned long)pgd);
		else
			kmem_cache_free(pgd_cache, pgd);
	}
}
#endif
static int __init pgd_cache_init(void)
{
	/*
	 * Naturally aligned pgds required by the architecture.
	 */
	if (PGD_SIZE != PAGE_SIZE)
		pgd_cache = kmem_cache_create("pgd_cache", PGD_SIZE, PGD_SIZE,
					      SLAB_PANIC, NULL);
	return 0;
}
core_initcall(pgd_cache_init);
