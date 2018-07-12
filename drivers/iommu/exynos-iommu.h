/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Data structure definition for Exynos IOMMU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _EXYNOS_IOMMU_H_
#define _EXYNOS_IOMMU_H_

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/iommu.h>
#include <linux/irq.h>
#include <linux/clk.h>

#include <linux/exynos_iovmm.h>

#include "exynos-iommu-log.h"

#define TRACE_LOG(...) do { } while (0) /* trace_printk */
#define TRACE_LOG_DEV(dev, fmt, args...)  \
		TRACE_LOG("%s: " fmt, dev_name(dev), ##args)

#define MODULE_NAME "exynos-sysmmu"

#define IOVA_START 0x10000000
#define IOVM_SIZE (SZ_2G + SZ_1G + SZ_256M) /* last 4K is for error values */

#define IOVM_NUM_PAGES(vmsize) (vmsize / PAGE_SIZE)
#define IOVM_BITMAP_SIZE(vmsize) \
		((IOVM_NUM_PAGES(vmsize) + BITS_PER_BYTE) / BITS_PER_BYTE)

#define SPSECT_ORDER	24
#define DSECT_ORDER	21
#define SECT_ORDER	20
#define LPAGE_ORDER	16
#define SPAGE_ORDER	12

#define SPSECT_SIZE	(1 << SPSECT_ORDER)
#define DSECT_SIZE	(1 << DSECT_ORDER)
#define SECT_SIZE	(1 << SECT_ORDER)
#define LPAGE_SIZE	(1 << LPAGE_ORDER)
#define SPAGE_SIZE	(1 << SPAGE_ORDER)

#define SPSECT_MASK	~(SPSECT_SIZE - 1)
#define DSECT_MASK	~(DSECT_SIZE - 1)
#define SECT_MASK	~(SECT_SIZE - 1)
#define LPAGE_MASK	~(LPAGE_SIZE - 1)
#define SPAGE_MASK	~(SPAGE_SIZE - 1)

#define SPSECT_ENT_MASK	~((SPSECT_SIZE >> PG_ENT_SHIFT) - 1)
#define DSECT_ENT_MASK	~((DSECT_SIZE >> PG_ENT_SHIFT) - 1)
#define SECT_ENT_MASK	~((SECT_SIZE >> PG_ENT_SHIFT) - 1)
#define LPAGE_ENT_MASK	~((LPAGE_SIZE >> PG_ENT_SHIFT) - 1)
#define SPAGE_ENT_MASK	~((SPAGE_SIZE >> PG_ENT_SHIFT) - 1)

#define SECT_PER_SPSECT		(SPSECT_SIZE / SECT_SIZE)
#define SECT_PER_DSECT		(DSECT_SIZE / SECT_SIZE)
#define SPAGES_PER_LPAGE	(LPAGE_SIZE / SPAGE_SIZE)

#define PGBASE_TO_PHYS(pgent)	((phys_addr_t)(pgent) << PG_ENT_SHIFT)

#define MAX_NUM_PBUF	6
#define MAX_NUM_PLANE	6

#define NUM_LV1ENTRIES 4096
#define NUM_LV2ENTRIES (SECT_SIZE / SPAGE_SIZE)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) ((iova & ~SECT_MASK) >> SPAGE_ORDER)

typedef u32 sysmmu_pte_t;

#define LV2TABLE_SIZE (NUM_LV2ENTRIES * sizeof(sysmmu_pte_t))

#define ENT_TO_PHYS(ent) (phys_addr_t)(*(ent))
#define spsection_phys(sent) PGBASE_TO_PHYS(ENT_TO_PHYS(sent) & SPSECT_ENT_MASK)
#define spsection_offs(iova) ((iova) & (SPSECT_SIZE - 1))
#define section_phys(sent) PGBASE_TO_PHYS(ENT_TO_PHYS(sent) & SECT_ENT_MASK)
#define section_offs(iova) ((iova) & (SECT_SIZE - 1))
#define lpage_phys(pent) PGBASE_TO_PHYS(ENT_TO_PHYS(pent) & LPAGE_ENT_MASK)
#define lpage_offs(iova) ((iova) & (LPAGE_SIZE - 1))
#define spage_phys(pent) PGBASE_TO_PHYS(ENT_TO_PHYS(pent) & SPAGE_ENT_MASK)
#define spage_offs(iova) ((iova) & (SPAGE_SIZE - 1))

#define lv2table_base(sent) ((phys_addr_t)(*(sent) & ~0x3F) << PG_ENT_SHIFT)

#define SYSMMU_BLOCK_POLLING_COUNT 4096

#define REG_MMU_CTRL		0x000
#define REG_MMU_CFG		0x004
#define REG_MMU_STATUS		0x008
#define REG_MMU_VERSION		0x034

#define CTRL_ENABLE	0x5
#define CTRL_BLOCK	0x7
#define CTRL_DISABLE	0x0
#define CTRL_BLOCK_DISABLE 0x3

#define CFG_ACGEN	(1 << 24) /* System MMU 3.3+ */
#define CFG_FLPDCACHE	(1 << 20) /* System MMU 3.2+ */
#define CFG_SHAREABLE	(1 << 12) /* System MMU 3.0+ */
#define CFG_QOS_OVRRIDE (1 << 11) /* System MMU 3.3+ */
#define CFG_QOS(n)	(((n) & 0xF) << 7)

/*
 * Metadata attached to the owner of a group of System MMUs that belong
 * to the same owner device.
 */
struct exynos_iommu_owner {
	struct list_head client; /* entry of exynos_iommu_domain.clients */
	struct device *dev;
	struct exynos_iommu_owner *next; /* linked list of Owners */
	void *vmm_data;         /* IO virtual memory manager's data */
	spinlock_t lock;        /* Lock to preserve consistency of System MMU */
	struct list_head mmu_list; /* head of sysmmu_list_data.node */
	struct notifier_block nb;
	iommu_fault_handler_t fault_handler;
	void *token;
};

struct exynos_vm_region {
	struct list_head node;
	u32 start;
	u32 size;
	u32 section_off;
	u32 dummy_size;
};

struct exynos_iovmm {
	struct iommu_domain *domain; /* iommu domain for this iovmm */
	size_t iovm_size[MAX_NUM_PLANE]; /* iovm bitmap size per plane */
	u32 iova_start[MAX_NUM_PLANE]; /* iovm start address per plane */
	unsigned long *vm_map[MAX_NUM_PLANE]; /* iovm biatmap per plane */
	struct list_head regions_list;	/* list of exynos_vm_region */
	spinlock_t vmlist_lock; /* lock for updating regions_list */
	spinlock_t bitmap_lock; /* lock for manipulating bitmaps */
	struct device *dev; /* peripheral device that has this iovmm */
	size_t allocated_size[MAX_NUM_PLANE];
	int num_areas[MAX_NUM_PLANE];
	int inplanes;
	int onplanes;
	unsigned int num_map;
	unsigned int num_unmap;
	const char *domain_name;
#ifdef CONFIG_EXYNOS_IOMMU_EVENT_LOG
	struct exynos_iommu_event_log log;
#endif
};

void exynos_sysmmu_tlb_invalidate(struct iommu_domain *domain, dma_addr_t start,
				  size_t size);

#define SYSMMU_FAULT_WRITE	(1 << SYSMMU_FAULTS_NUM)

enum sysmmu_property {
	SYSMMU_PROP_RESERVED,
	SYSMMU_PROP_READ,
	SYSMMU_PROP_WRITE,
	SYSMMU_PROP_READWRITE = SYSMMU_PROP_READ | SYSMMU_PROP_WRITE,
	SYSMMU_PROP_RW_MASK = SYSMMU_PROP_READWRITE,
	SYSMMU_PROP_NONBLOCK_TLBINV = 0x10,
	SYSMMU_PROP_STOP_BLOCK = 0x20,
	SYSMMU_PROP_DISABLE_ACG = 0x40,
	SYSMMU_PROP_WINDOW_SHIFT = 16,
	SYSMMU_PROP_WINDOW_MASK = 0x1F << SYSMMU_PROP_WINDOW_SHIFT,
};

enum sysmmu_clock_ids {
	SYSMMU_ACLK,
	SYSMMU_PCLK,
	SYSMMU_CLK_NUM,
};

/*
 * Metadata attached to each System MMU devices.
 */
struct sysmmu_drvdata {
	struct list_head node;	/* entry of exynos_iommu_owner.mmu_list */
	struct list_head pb_grp_list;	/* list of pb groups */
	struct sysmmu_drvdata *next; /* linked list of System MMU */
	struct device *sysmmu;	/* System MMU's device descriptor */
	struct device *master;	/* Client device that needs System MMU */
	void __iomem *sfrbase;
	struct clk *clocks[SYSMMU_CLK_NUM];
	int activations;
	struct iommu_domain *domain; /* domain given to iommu_attach_device() */
	phys_addr_t pgtable;
	spinlock_t lock;
	struct sysmmu_prefbuf pbufs[MAX_NUM_PBUF];
	short qos;
	short num_pbufs;
	int runtime_active;
	enum sysmmu_property prop; /* mach/sysmmu.h */
#ifdef CONFIG_EXYNOS_IOMMU_EVENT_LOG
	struct exynos_iommu_event_log log;
#endif
	struct atomic_notifier_head fault_notifiers;
	unsigned char event_cnt;
};

struct exynos_iommu_domain {
	struct list_head clients; /* list of sysmmu_drvdata.node */
	sysmmu_pte_t *pgtable; /* lv1 page table, 16KB */
	atomic_t *lv2entcnt; /* free lv2 entry counter for each section */
	spinlock_t lock; /* lock for this structure */
	spinlock_t pgtablelock; /* lock for modifying page table @ pgtable */
#ifdef CONFIG_EXYNOS_IOMMU_EVENT_LOG
	struct exynos_iommu_event_log log;
#endif
};

struct pb_info {
	struct list_head node;
	int ar_id_num;
	int aw_id_num;
	int grp_num;
	int ar_axi_id[MAX_NUM_PBUF];
	int aw_axi_id[MAX_NUM_PBUF];
	struct device *master;
	enum sysmmu_property prop;
};

int sysmmu_set_ppc_event(struct sysmmu_drvdata *drvdata, int event);
void dump_sysmmu_ppc_cnt(struct sysmmu_drvdata *drvdata);
extern const char *ppc_event_name[];

#define REG_PPC_EVENT_SEL(x)	(0x600 + 0x4 * (x))
#define REG_PPC_PMNC		0x620
#define REG_PPC_CNTENS		0x624
#define REG_PPC_CNTENC		0x628
#define REG_PPC_INTENS		0x62C
#define REG_PPC_INTENC		0x630
#define REG_PPC_FLAG		0x634
#define REG_PPC_CCNT		0x640
#define REG_PPC_PMCNT(x)	(0x644 + 0x4 * (x))

#define SYSMMU_OF_COMPAT_STRING "samsung,exynos5430-sysmmu"
#define DEFAULT_QOS_VALUE	-1 /* Inherited from master */
#define PG_ENT_SHIFT 4 /* 36bit PA, 32bit VA */
#define lv1ent_fault(sent) ((*(sent) & 7) == 0)
#define lv1ent_page(sent) ((*(sent) & 7) == 1)

#define FLPD_FLAG_MASK	7
#define SLPD_FLAG_MASK	3

#define SPSECT_FLAG	6
#define DSECT_FLAG	4
#define SECT_FLAG	2
#define SLPD_FLAG	1

#define LPAGE_FLAG	1
#define SPAGE_FLAG	2

#define lv1ent_section(sent) ((*(sent) & FLPD_FLAG_MASK) == SECT_FLAG)
#define lv1ent_dsection(sent) ((*(sent) & FLPD_FLAG_MASK) == DSECT_FLAG)
#define lv1ent_spsection(sent) ((*(sent) & FLPD_FLAG_MASK) == SPSECT_FLAG)
#define lv2ent_fault(pent) ((*(pent) & SLPD_FLAG_MASK) == 0 || \
			   (PGBASE_TO_PHYS(*(pent) & SPAGE_ENT_MASK) == fault_page))
#define lv2ent_small(pent) ((*(pent) & SLPD_FLAG_MASK) == SPAGE_FLAG)
#define lv2ent_large(pent) ((*(pent) & SLPD_FLAG_MASK) == LPAGE_FLAG)
#define dsection_phys(sent) PGBASE_TO_PHYS(*(sent) & DSECT_ENT_MASK)
#define dsection_offs(iova) ((iova) & (DSECT_SIZE - 1))
#define mk_lv1ent_spsect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 6)
#define mk_lv1ent_dsect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 4)
#define mk_lv1ent_sect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 2)
#define mk_lv1ent_page(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 1)
#define mk_lv2ent_lpage(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 1)
#define mk_lv2ent_spage(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 2)
#define set_lv1ent_shareable(sent) (*(sent) |= (1 << 6))
#define set_lv2ent_shareable(pent) (*(pent) |= (1 << 4))

#define mk_lv2ent_pfnmap(pent) (*(pent) |= (1 << 5)) /* unused field */
#define lv2ent_pfnmap(pent) ((*(pent) & (1 << 5)) == (1 << 5))

#define PGSIZE_BITMAP (DSECT_SIZE | SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE)

void __sysmmu_show_status(struct sysmmu_drvdata *drvdata);

static inline void __sysmmu_clk_enable(struct sysmmu_drvdata *data)
{
	if (!IS_ERR(data->clocks[SYSMMU_ACLK]))
		clk_enable(data->clocks[SYSMMU_ACLK]);

	if (!IS_ERR(data->clocks[SYSMMU_PCLK]))
		clk_enable(data->clocks[SYSMMU_PCLK]);
}

static inline void __sysmmu_clk_disable(struct sysmmu_drvdata *data)
{
	if (!IS_ERR(data->clocks[SYSMMU_ACLK]))
		clk_disable(data->clocks[SYSMMU_ACLK]);

	if (!IS_ERR(data->clocks[SYSMMU_PCLK]))
		clk_disable(data->clocks[SYSMMU_PCLK]);
}

static inline bool get_sysmmu_runtime_active(struct sysmmu_drvdata *data)
{
	return ++data->runtime_active == 1;
}

static inline bool put_sysmmu_runtime_active(struct sysmmu_drvdata *data)
{
	BUG_ON(data->runtime_active < 1);
	return --data->runtime_active == 0;
}

static inline bool is_sysmmu_runtime_active(struct sysmmu_drvdata *data)
{
	return data->runtime_active > 0;
}

static inline bool set_sysmmu_active(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU was not active previously
	   and it needs to be initialized */
	return ++data->activations == 1;
}

static inline bool set_sysmmu_inactive(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU is needed to be disabled */
	BUG_ON(data->activations < 1);
	return --data->activations == 0;
}

static inline bool is_sysmmu_active(struct sysmmu_drvdata *data)
{
	return data->activations > 0;
}

static inline bool is_sysmmu_really_enabled(struct sysmmu_drvdata *data)
{
	return is_sysmmu_active(data) && data->runtime_active;
}

#define MMU_MAJ_VER(val)	((val) >> 11)
#define MMU_MIN_VER(val)	((val >> 4) & 0x7F)
#define MMU_REV_VER(val)	((val) & 0xF)
#define MMU_RAW_VER(reg)	(((reg) >> 17) & ((1 << 15) - 1)) /* 11 bits */

#define MAKE_MMU_VER(maj, min)	((((maj) & 0xF) << 7) | ((min) & 0x7F))

static inline unsigned int __raw_sysmmu_version(void __iomem *sfrbase)
{
	return MMU_RAW_VER(__raw_readl(sfrbase + REG_MMU_VERSION));
}

static inline void __raw_sysmmu_disable(void __iomem *sfrbase, int disable)
{
	__raw_writel(0, sfrbase + REG_MMU_CFG);
	__raw_writel(disable, sfrbase + REG_MMU_CTRL);
	BUG_ON(__raw_readl(sfrbase + REG_MMU_CTRL) != disable);
}

static inline void __raw_sysmmu_enable(void __iomem *sfrbase)
{
	__raw_writel(CTRL_ENABLE, sfrbase + REG_MMU_CTRL);
}

#define sysmmu_unblock __raw_sysmmu_enable

void dump_sysmmu_tlb_pb(void __iomem *sfrbase);

static inline bool sysmmu_block(void __iomem *sfrbase)
{
	int i = SYSMMU_BLOCK_POLLING_COUNT;

	__raw_writel(CTRL_BLOCK, sfrbase + REG_MMU_CTRL);
	while ((i > 0) && !(__raw_readl(sfrbase + REG_MMU_STATUS) & 1))
		--i;

	if (!(__raw_readl(sfrbase + REG_MMU_STATUS) & 1)) {
		dump_sysmmu_tlb_pb(sfrbase);
		panic("Failed to block System MMU!");
		sysmmu_unblock(sfrbase);
		return false;
	}

	return true;
}

void __sysmmu_init_config(struct sysmmu_drvdata *drvdata);
void __sysmmu_set_ptbase(void __iomem *sfrbase, phys_addr_t pfn_pgtable);

extern sysmmu_pte_t *zero_lv2_table;

static inline sysmmu_pte_t *page_entry(sysmmu_pte_t *sent, unsigned long iova)
{
	return (sysmmu_pte_t *)(phys_to_virt(lv2table_base(sent))) +
				lv2ent_offset(iova);
}

static inline sysmmu_pte_t *section_entry(
				sysmmu_pte_t *pgtable, unsigned long iova)
{
	return (sysmmu_pte_t *)(pgtable + lv1ent_offset(iova));
}

irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id);
void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase, dma_addr_t iova);
void __sysmmu_tlb_invalidate(struct sysmmu_drvdata *drvdata,
				dma_addr_t iova, size_t size);

int exynos_iommu_map_userptr(struct iommu_domain *dom, unsigned long addr,
			      dma_addr_t iova, size_t size, int prot);
void exynos_iommu_unmap_userptr(struct iommu_domain *dom,
				dma_addr_t iova, size_t size);

void dump_sysmmu_tlb_pb(void __iomem *sfrbase);

#if defined(CONFIG_EXYNOS_IOVMM)
static inline struct exynos_iovmm *exynos_get_iovmm(struct device *dev)
{
	if (!dev->archdata.iommu) {
		dev_err(dev, "%s: System MMU is not configured\n", __func__);
		return NULL;
	}

	return ((struct exynos_iommu_owner *)dev->archdata.iommu)->vmm_data;
}

struct exynos_vm_region *find_iovm_region(struct exynos_iovmm *vmm,
						dma_addr_t iova);

static inline int find_iovmm_plane(struct exynos_iovmm *vmm, dma_addr_t iova)
{
	int i;

	for (i = 0; i < (vmm->inplanes + vmm->onplanes); i++)
		if ((iova >= vmm->iova_start[i]) &&
			(iova < (vmm->iova_start[i] + vmm->iovm_size[i])))
			return i;
	return -1;
}

struct exynos_iovmm *exynos_create_single_iovmm(const char *name);
int exynos_sysmmu_add_fault_notifier(struct device *dev,
		iommu_fault_handler_t handler, void *token);
#else
static inline struct exynos_iovmm *exynos_get_iovmm(struct device *dev)
{
	return NULL;
}

struct exynos_vm_region *find_iovm_region(struct exynos_iovmm *vmm,
						dma_addr_t iova)
{
	return NULL;
}

static inline int find_iovmm_plane(struct exynos_iovmm *vmm, dma_addr_t iova)
{
	return -1;
}

static inline struct exynos_iovmm *exynos_create_single_iovmm(const char *name)
{
	return NULL;
}
#endif /* CONFIG_EXYNOS_IOVMM */

#endif /* _EXYNOS_IOMMU_H_ */
