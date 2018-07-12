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

#ifndef _RKP_ENTRY_H
#define _RKP_ENTRY_H

#ifndef __ASSEMBLY__
#ifdef CONFIG_TIMA_RKP
#define RKP_PREFIX  UL(0x83800000)
#define RKP_CMDID(CMD_ID)  ((UL(CMD_ID) << 12 ) | RKP_PREFIX)

#define RKP_PGD_SET  RKP_CMDID(0x21)
#define RKP_PMD_SET  RKP_CMDID(0x22)
#define RKP_PTE_SET  RKP_CMDID(0x23)
#define RKP_PGD_FREE RKP_CMDID(0x24)
#define RKP_PGD_NEW  RKP_CMDID(0x25)

#define CFP_ROPP_INIT		RKP_CMDID(0x90)
#define CFP_ROPP_NEW_KEY	RKP_CMDID(0x91)
#define CFP_ROPP_NEW_KEY_REENC	RKP_CMDID(0x92)
#define CFP_ROPP_KEY_DEC	RKP_CMDID(0x93)
#define CFP_ROPP_RET_KEY	RKP_CMDID(0x94)
#define CFP_JOPP_INIT		RKP_CMDID(0x98)

#define RKP_INIT	 RKP_CMDID(0)
#define RKP_DEF_INIT	 RKP_CMDID(1)
#define RKP_DEBUG	 RKP_CMDID(2)

#define RKP_INIT_MAGIC 0x5afe0001
#define RKP_VMM_BUFFER 0x600000
#define RKP_RO_BUFFER  UL(0x800000)


#define KASLR_MEM_RESERVE  RKP_CMDID(0x70)

/*** TODO: We need to export this so it is hard coded 
     at one place*/

#define RKP_PGT_BITMAP_LEN 0x20000

#define   TIMA_VMM_START        0xB0500000
#define   TIMA_VMM_SIZE         1<<20

#define   TIMA_DEBUG_LOG_START  0xB0600000
#define   TIMA_DEBUG_LOG_SIZE   1<<18

#define   TIMA_SEC_LOG          0xB0800000
#define   TIMA_SEC_LOG_SIZE     0x7000 

#define   TIMA_PHYS_MAP         0xB0000000
#define   TIMA_PHYS_MAP_SIZE    0x500000

#define   TIMA_DASHBOARD_START  0xB0807000
#define   TIMA_DASHBOARD_SIZE    0x1000

#define   TIMA_ROBUF_START      0xB0808000
#define   TIMA_ROBUF_SIZE       0x7f8000 /* 8MB - RKP_SEC_LOG_SIZE - RKP_DASHBOARD_SIZE)*/

#define RKP_RBUF_VA      (phys_to_virt(TIMA_ROBUF_START))
#define RO_PAGES  (TIMA_ROBUF_SIZE >> PAGE_SHIFT) // (TIMA_ROBUF_SIZE/PAGE_SIZE)
#define CRED_JAR_RO "cred_jar_ro"
#define TSEC_JAR	"tsec_jar"
#define VFSMNT_JAR	"vfsmnt_cache"

extern u8 rkp_pgt_bitmap[];
extern u8 rkp_map_bitmap[];

typedef struct rkp_init rkp_init_t;
extern u8 rkp_started;
extern void *rkp_ro_alloc(void);
extern void rkp_ro_free(void *free_addr);
extern unsigned int is_rkp_ro_page(u64 addr);
extern int rkp_support_large_memory;

struct rkp_init {
	u32 magic;
	u64 vmalloc_start;
	u64 vmalloc_end;
	u64 init_mm_pgd;
	u64 id_map_pgd;
	u64 zero_pg_addr;
	u64 rkp_pgt_bitmap;
	u64 rkp_map_bitmap;
	u32 rkp_pgt_bitmap_size;
	u64 _text;
	u64 _etext;
	u64 extra_memory_addr;
	u32 extra_memory_size;
	u64 physmap_addr;
	u64 _srodata;
	u64 _erodata;
	u32 large_memory;
#ifdef CONFIG_UNMAP_KERNEL_AT_EL0
	u64 tramp_pgd;
#endif
} __attribute__((packed));

#ifdef CONFIG_RKP_KDP
typedef struct kdp_init
{
	u32 credSize;
	u32 cred_task;
	u32 mm_task;
	u32 uid_cred;
	u32 euid_cred;
	u32 gid_cred;
	u32 egid_cred;
	u32 bp_pgd_cred;
	u32 bp_task_cred;
	u32 type_cred;
	u32 security_cred;
	u32 pid_task;
	u32 rp_task;
	u32 comm_task;
	u32 pgd_mm;
	u32 usage_cred;
	u32 task_threadinfo;
	u32 sp_size;
	u32 bp_cred_secptr;
} kdp_init_t;
#endif  /* CONFIG_RKP_KDP */

void rkp_call(unsigned long long cmd, unsigned long long arg0, unsigned long long arg1, unsigned long long arg2, unsigned long long arg3, unsigned long long arg4);


#define rkp_is_pg_protected(va)	rkp_is_protected(va,__pa(va),(u64 *)rkp_pgt_bitmap,1)
#define rkp_is_pg_dbl_mapped(pa) rkp_is_protected((u64)__va(pa),pa,(u64 *)rkp_map_bitmap,0)


#define	RKP_PHYS_OFFSET_MAX		(0x900000ULL << PAGE_SHIFT)
#define RKP_PHYS_ADDR_MASK		((1ULL << 40)-1)

#define	PHYS_PFN_OFFSET_MIN_DRAM1	(0x80000ULL)
#define	PHYS_PFN_OFFSET_MAX_DRAM1	(0xFE400ULL)
#define	PHYS_PFN_OFFSET_MIN_DRAM2	(0x880000ULL)
#define	PHYS_PFN_OFFSET_MAX_DRAM2	(0x900000ULL)

#define DRAM1_SIZE	(PHYS_PFN_OFFSET_MAX_DRAM1 - PHYS_PFN_OFFSET_MIN_DRAM1)
#define DRAM2_SIZE	(PHYS_PFN_OFFSET_MAX_DRAM2 - PHYS_PFN_OFFSET_MIN_DRAM2)
#define DRAM_GAP	(PHYS_PFN_OFFSET_MIN_DRAM2 - PHYS_PFN_OFFSET_MAX_DRAM1)	

	
static inline u64 rkp_get_sys_index(u64 pfn) {
	if (pfn >= PHYS_PFN_OFFSET_MIN_DRAM1 
		&& pfn < PHYS_PFN_OFFSET_MAX_DRAM1) {
		return ((pfn) - PHYS_PFN_OFFSET);
	}
	if (pfn >= PHYS_PFN_OFFSET_MIN_DRAM2 
		&& pfn < PHYS_PFN_OFFSET_MAX_DRAM2) {
		return ((pfn) - PHYS_PFN_OFFSET - DRAM_GAP);
	}
	return (~0ULL);
}

extern  int printk(const char *s, ...);
extern void dump_stack(void);
static inline u8 rkp_is_protected(u64 va,u64 pa, u64 *base_addr,u64 type)
{
	u64 phys_addr = pa & (RKP_PHYS_ADDR_MASK);
	u64 index = rkp_get_sys_index((phys_addr>>PAGE_SHIFT));
	u64 *p = base_addr;
	u64 tmp = (index>>6);
	u64 rindex;
	u8 val;
	/* We are going to ignore if input address NOT belong to DRAM area */
	if((phys_addr < PHYS_OFFSET) || (index ==(~0ULL)) ||
		(phys_addr  > RKP_PHYS_OFFSET_MAX)) {
		return 0;	
	}

	p += (tmp);
	rindex = index % 64;
	val = (((*p) & (1ULL<<rindex))?1:0);
	return val;
}
#endif // CONFIG_TIMA_RKP
#endif //__ASSEMBLY__

#endif //_RKP_ENTRY_H
