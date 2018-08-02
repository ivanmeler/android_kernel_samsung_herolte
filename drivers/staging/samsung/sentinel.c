/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Memory Protection Watchman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/memblock.h>
#include <linux/sentinel.h>
#include <asm/cacheflush.h>
#include <asm/debug-monitors.h>

#ifdef SENTINEL_FULL_FUNCTION
#undef USE_MARK_4BITS	/* otherwise, use 1bit */

#define SENTINEL_UNIT_SHIFT	3
#define SENTINEL_UNIT_SIZE	(1 << SENTINEL_UNIT_SHIFT)
#ifdef USE_MARK_4BITS
#define SMARK_SHIFT	1
#define SMARK_BITS	4
#else /* 1BIT */
#define SMARK_SHIFT	3
#define SMARK_BITS	1
#endif
#define SMARK_MASK	((1<<SMARK_SHIFT)-1)
#define SMARK_BIT_MASK	((1<<SMARK_BITS)-1)
#define SMARK_PER_BYTE	(8/SMARK_BITS)

#define SENTINEL_TOTAL_SIZE	((u64)4 * 1024 * 1024 * 1024)
#define SENTINEL_RESERVE_SIZE	(SENTINEL_TOTAL_SIZE / SENTINEL_UNIT_SIZE / SMARK_PER_BYTE)
static void *sentinel_sanity_mark[2];

#define SENTINEL_AREA_NUM(a)	(((unsigned long)(a) >> 35) & 0x1)
#define SENTINEL_INDEX(a)	(((unsigned long)(a) & 0xffffffff) >> SENTINEL_UNIT_SHIFT)
#define SENTINEL_LEN(s)		((unsigned long)(s) >> SENTINEL_UNIT_SHIFT)

#ifdef USE_MARK_4BITS
#define SENTINEL_MARK_SANE	0x0
#define SENTINEL_MARK_INSANE	0xf
#define SENTINEL_MARK_DEFAULT	0xff
#else
#define SENTINEL_MARK_SANE	0x0
#define SENTINEL_MARK_INSANE	0x1
#define SENTINEL_MARK_DEFAULT	0xff
#endif

static unsigned long sentinel_invoked_count;
static bool sentinel_initialized = false;
static bool sentinel_enable = false;
static char sentinel_vip[100];
#endif

void __init *sentinel_early_alloc(unsigned long sz, unsigned long addr)
{
	void *ptr;
	unsigned long start_addr = PHYS_OFFSET;
	int i = 1;

	addr = __virt_to_phys(addr);

	if (addr < PHYS_OFFSET + SENTINEL_BANK_SIZE) {
		while(i) {
			if(addr < (start_addr + (i * SENTINEL_BANK_SIZE)))
				break;
			i++;
		}

		start_addr += (i * SENTINEL_BANK_SIZE);
	}

	ptr = __va(memblock_alloc_range(sz, sz, start_addr, start_addr + (SENTINEL_BANK_SIZE / 2)));
	memset(ptr, 0, sz);

	return ptr;
}

void sentinel_install_cctv(unsigned long where, unsigned long count)
{
	int ret;

#ifdef SENTINEL_FULL_FUNCTION
	if(!sentinel_initialized)
		return;
#endif

	ret = set_memory_ro(where & PAGE_MASK, count);
	if (ret)
			pr_err("%s: install failed by %d\n", __func__, ret);
}

void sentinel_uninstall_cctv(unsigned long where, unsigned long count)
{
	int ret;

	ret = set_memory_rw(where & PAGE_MASK, count);
	if (ret)
			pr_err("%s: uninstall failed by %d\n", __func__, ret);
}

#ifdef SENTINEL_FULL_FUNCTION
bool sentinel_is_installed_cctv(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	unsigned long target_addr = addr & PAGE_MASK;

	pgd = pgd_offset_k(target_addr);
	if (pgd_none(*pgd))
		return false;

	pud = pud_offset(pgd, target_addr);
	if (pud_none(*pud))
		return false;

	pmd = pmd_offset(pud, target_addr);
	if (pmd_none(*pmd))
		return false;

	pte = pte_offset_kernel(pmd, target_addr);
	if (pte_none(*pte))
		return false;

	if (pte_write(*pte))
		return false;

	return true;
}

void sentinel_install_slab_cctv(unsigned long where, unsigned long count, const char * name)
{
	if(name == NULL)
		return;
#if 0
	if(!strcmp(sentinel_vip, name) || !strcmp(sentinel_vip, "all")) {
		if(strcmp("names_cache", name))
			sentinel_install_cctv(where, count);
	}
#else
	if(!strncmp("kmalloc", name, 7)) {
		sentinel_install_cctv(where, count);
	}
#endif
}

static bool __sentinel_is_sane(unsigned long addr)
{
	int area;
	int off;
	unsigned long idx;
	u8 mark;
	u8 sign_insane = SENTINEL_MARK_INSANE;
	u8 sign_sane = SENTINEL_MARK_SANE;

	area = SENTINEL_AREA_NUM(addr);
	idx = SENTINEL_INDEX(addr);

	off = idx & SMARK_MASK;
	mark = *(u8*)(sentinel_sanity_mark[area]+idx/SMARK_PER_BYTE);
	mark >>= off*SMARK_BITS;
	mark &= SMARK_BIT_MASK;

	if (mark == sign_insane)
		return false;
	else if (mark == sign_sane)
		return true;
	else {
		WARN(1, "%s: invalid mark(%u)", __func__, mark);
		return false;
	}
}

static bool sentinel_is_addr_sane(unsigned long addr)
{
	/* TODO implement */
	if (!sentinel_initialized)
		return false;

	if (!virt_addr_valid(addr))
		return false;

	if(__sentinel_is_sane(addr))
		return true;
	else
		return false;
}

#ifdef USE_MARK_4BITS
static void __sentinel_set_sane(unsigned long addr, unsigned long size, bool insane)
{
	int area;
	unsigned long idx, len;
	u8 mark;
	u8 sign = insane ? SENTINEL_MARK_INSANE : SENTINEL_MARK_SANE;

	area = SENTINEL_AREA_NUM(addr);
	idx = SENTINEL_INDEX(addr);
	len = SENTINEL_LEN(size);

	if (idx & 0x1 && len > 0) {
		mark = *(u8*)(sentinel_sanity_mark[area]+idx/2) & 0x0f;
		*(u8*)(sentinel_sanity_mark[area]+idx/2) = mark | (sign << 4);
		idx += 1;
		len -= 1;
	}

	if (len >= 2) {
		memset(sentinel_sanity_mark[area]+idx/2, sign | (sign << 4), len / 2);
		idx += len & ~0x1;
		len &= 0x1;
	}

	if (len > 0) {
		mark = *(u8*)(sentinel_sanity_mark[area]+idx/2) & 0xf0;
		*(u8*)(sentinel_sanity_mark[area]+idx/2) = mark | sign;
	}	
}
#else
static void __sentinel_set_sane(unsigned long addr, unsigned long size, bool insane)
{
	int area;
	unsigned long off;
	unsigned long idx, len;
	u8 mark, mask;
	u8 sign = insane ? SENTINEL_MARK_INSANE : SENTINEL_MARK_SANE;

	area = SENTINEL_AREA_NUM(addr);
	idx = SENTINEL_INDEX(addr);
	len = SENTINEL_LEN(size);

	off = idx & 0x7;
	if (off > 0 && len > 0) {
		if (len < 8)
			mask = (1<<len)-1;
		else
			mask = 0xff;
		mask <<= off;
		mark = *(u8*)(sentinel_sanity_mark[area]+idx/8);
		if (sign)
			mark |= mask;
		else
			mark &= ~mask;
		*(u8*)(sentinel_sanity_mark[area]+idx/8) = mark;

		idx += min(len, 8-off);
		len -= min(len, 8-off);
	}

	if (len >= 8) {
		memset(sentinel_sanity_mark[area]+idx/8, sign?0xff:0x0, len/8);
		idx += len & ~0x7;
		len &= 0x7;
	}

	if (len > 0) {
		mask = ~(u8)(0xff << len);
		mark = *(u8*)(sentinel_sanity_mark[area]+idx/8);
		if (sign)
			mark |= mask;
		else
			mark &= ~mask;
		*(u8*)(sentinel_sanity_mark[area]+idx/8) = mark;
	}	
}
#endif /* USE_MARK_4BITS */

static void sentinel_set_sane(unsigned long addr, unsigned long size, bool insane)
{
	__sentinel_set_sane(addr, size, insane);
}

void sentinel_set_prohibited(void *addr, size_t size)
{
	if (!sentinel_initialized)
		return;

	sentinel_set_sane((unsigned long)addr, (unsigned long)size, true);
}

void sentinel_set_allowed(void *addr, size_t size)
{
	if (!sentinel_initialized)
		return;

	sentinel_set_sane((unsigned long)addr, (unsigned long)size, false);
}

bool sentinel_sanity_check(struct pt_regs *regs, unsigned long addr)
{
	unsigned int cnt = 1;
	/* Verify addr*/
	if (!sentinel_is_addr_sane(addr))
		return false;

	local_dbg_disable();
	kernel_enable_single_step(regs);

	current_thread_info()->sentinel_target_addr = addr;

	/*Change target addr writable*/
	if (((addr & PAGE_MASK) != ((addr + 15) & PAGE_MASK)) &&
			(sentinel_is_addr_sane(addr + 15)))
		cnt += 1;

	sentinel_uninstall_cctv(addr, cnt);

	return true;
}

static int sentinel_step_brk_fn(struct pt_regs *regs, unsigned int esr)
{
	unsigned int *pc = (unsigned int *)regs->pc - 1;
	unsigned long addr = current_thread_info()->sentinel_target_addr;
	unsigned int cnt = 1;

	sentinel_invoked_count++;

	/*Restore target addr readonly */
	if((*pc & 0x3FC00000) == 0x08000000) {
		/*
		unsigned long spsr;

		spsr = regs->pstate;
		spsr &= ~DBG_SPSR_SS;
		spsr |= DBG_SPSR_SS;
		regs->pstate = spsr;
		*/
	} else {
		if (((addr & PAGE_MASK) != ((addr + 15) & PAGE_MASK)) &&
				(sentinel_is_addr_sane(addr + 15)))
			cnt += 1;

		sentinel_install_cctv(addr, cnt);
	}

	/*Disable single stepping*/
	kernel_disable_single_step();

	return 0;
}

void __init sentinel_reserve(void)
{
	phys_addr_t phys;

	phys = memblock_alloc_nid(SENTINEL_RESERVE_SIZE, sizeof(void*), NUMA_NO_NODE);
	if (!phys) {
		pr_err("%s: failed to allocate %llu\n", __func__, SENTINEL_RESERVE_SIZE);
		return;
	}

	sentinel_sanity_mark[0] = __va(phys);
	pr_info("%s: %llu@%p\n", __func__, SENTINEL_RESERVE_SIZE, sentinel_sanity_mark);

	return;
}

static void __init sentinel_mem_init(void)
{
	if (!sentinel_sanity_mark[0])
		return;

	memset(sentinel_sanity_mark[0], SENTINEL_MARK_DEFAULT, SENTINEL_RESERVE_SIZE);
	sentinel_sanity_mark[1] = sentinel_sanity_mark[0] + SENTINEL_RESERVE_SIZE / 2;
}
static ssize_t show_debug(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t count = 0;

	return count;
}

static ssize_t store_debug(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	static char *buff;
	static int alloced = 0;
	int value;

	if (!sscanf(buf, "%d", &value))
		return -EINVAL;

	if (value > 0) {
		int i;

		sentinel_initialized = true;

		if (alloced == 0) {
			buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
			alloced = 1;
		}

		for(i = 0; i < value; i++){
			*buff = 0x55;
			buff++;
		}
	} else if (value == 0) {
		char *tmp;

		tmp = kmalloc(PAGE_SIZE, GFP_KERNEL);
		kfree(tmp);
		*(tmp + 200) = 0x55;
	}

	return count;
}

static ssize_t invoked_count_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", sentinel_invoked_count);
}

static struct kobj_attribute debug =
__ATTR(debug, 0644, show_debug, store_debug);

static struct kobj_attribute invoked_count =
__ATTR_RO(invoked_count);

static struct attribute *sentinel_attrs[] = {
	&debug.attr,
	&invoked_count.attr,
	NULL,
};

static struct attribute_group sentinel_attr_group = {
	.attrs = sentinel_attrs,
	.name = "sentinel",
};

static struct step_hook sentinel_step_hook = {
	.fn		= sentinel_step_brk_fn
};

int __init sentinel_init(void)
{
	register_step_hook(&sentinel_step_hook);
	sentinel_mem_init();

	return 0;
}

static int __init sentinel_setup(char *buf)
{
	strcpy(sentinel_vip, buf);
	printk("%s: %s\n", __func__, sentinel_vip);
	sentinel_enable = true;
	return 0;
}
early_param("sentinel_enable", sentinel_setup);

static int __init sentinel_sysfs_init(void)
{
	int err;

#ifdef CONFIG_SYSFS
	/*creat sysfs node /sys/kernel/mm/sentinel*/
	err = sysfs_create_group(mm_kobj, &sentinel_attr_group);
	if (err) {
		pr_err("sentinel: register sysfs failed\n");
	}
#endif
	if(sentinel_enable)
		sentinel_initialized = true;
	return 0;
}
arch_initcall(sentinel_sysfs_init);
#endif
