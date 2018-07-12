#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/memblock.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/pm_domain.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include <linux/highmem.h>

#include <asm/cacheflush.h>
#include <asm/pgtable.h>

#include <dt-bindings/sysmmu/sysmmu.h>

#include "exynos-iommu.h"

#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif

#define CFG_MASK	0x01101FBC /* Selecting bit 24, 20, 12-7, 5-2 */

#define PB_INFO_NUM(reg)	((reg) & 0xFF)
#define PB_GRP_NUM(reg)		((reg) >> 20)
#define L1TLB_ATTR_IM	(1 << 16)

#define REG_PT_BASE_PPN		0x00C
#define REG_MMU_FLUSH		0x010
#define REG_MMU_FLUSH_ENTRY	0x014
#define REG_MMU_FLUSH_RANGE	0x018
#define REG_FLUSH_RANGE_START	0x020
#define REG_FLUSH_RANGE_END	0x024
#define REG_MMU_CAPA		0x030
#define REG_MMU_CAPA_1		0x038
#define REG_INT_STATUS		0x060
#define REG_INT_CLEAR		0x064
#define REG_FAULT_AR_ADDR	0x070
#define REG_FAULT_AR_TRANS_INFO	0x078
#define REG_FAULT_AW_ADDR	0x080
#define REG_FAULT_AW_TRANS_INFO	0x088
#define REG_L1TLB_CFG		0x100 /* sysmmu v5.1 only */
#define REG_L1TLB_CTRL		0x108 /* sysmmu v5.1 only */
#define REG_L2TLB_CFG		0x200 /* sysmmu that has L2TLB only*/
#define REG_PB_LMM		0x300
#define REG_PB_GRP_STATE	0x304
#define REG_PB_INDICATE		0x308
#define REG_PB_CFG		0x310
#define REG_PB_START_ADDR	0x320
#define REG_PB_END_ADDR		0x328
#define REG_PB_AXI_ID		0x330
#define REG_PB_INFO		0x350
#define REG_SW_DF_VPN		0x400 /* sysmmu v5.1 only */
#define REG_SW_DF_VPN_CMD_NUM	0x408 /* sysmmu v5.1 only */
#define REG_L1TLB_READ_ENTRY	0x750
#define REG_L1TLB_ENTRY_VPN	0x754
#define REG_L1TLB_ENTRY_PPN	0x75C
#define REG_L1TLB_ENTRY_ATTR	0x764
#define REG_L2TLB_READ_ENTRY	0x770
#define REG_L2TLB_ENTRY_VPN	0x774
#define REG_L2TLB_ENTRY_PPN	0x77C
#define REG_L2TLB_ENTRY_ATTR	0x784
#define REG_PCI_SPB0_SVPN	0x7A0
#define REG_PCI_SPB0_EVPN	0x7A4
#define REG_PCI_SPB0_SLOT_VALID	0x7A8
#define REG_PCI_SPB1_SVPN	0x7B0
#define REG_PCI_SPB1_EVPN	0x7B4
#define REG_PCI_SPB1_SLOT_VALID	0x7B8

/* 'reg' argument must be the value of REG_MMU_CAPA register */
#define MMU_NUM_L1TLB_ENTRIES(reg) (reg & 0xFF)
#define MMU_HAVE_PB(reg)	(!!((reg >> 20) & 0xF))
#define MMU_PB_GRP_NUM(reg)	(((reg >> 20) & 0xF))
#define MMU_IS_TLB_FULL_ASSOCIATIVE(reg)	(!!(reg & 0xFF))
#define MMU_IS_TLB_CONFIGURABLE(reg)	(!!((reg >> 16) & 0xFF))

#define MMU_TLB_WAY(reg)	((reg >> 8) & 0xFF)
#define MMU_MIN_TLB_LINE(reg)	((reg >> 16) & 0xFF)
#define MMU_MAX_TLB_SET(reg)	(reg & 0xFF)
#define MMU_SET_TLB_READ_ENTRY(set, way, line) ((set) | (way << 8) | (line << 16))
#define MMU_TLB_ENTRY_VALID(reg) (reg >> 28)

#define MMU_MAX_DF_CMD		8
#define MAX_NUM_PPC	4

#define SYSMMU_FAULTS_NUM         (SYSMMU_FAULT_UNKNOWN + 1)

/* default burst length is BL4(16 page descriptor, 4set) */
#define MMU_MASK_LINE_SIZE	0x7
#define MMU_DEFAULT_LINE_SIZE	(0x2 << 4)

const char *ppc_event_name[] = {
	"TOTAL",
	"L1TLB MISS",
	"L2TLB MISS",
	"FLPD CACHE MISS",
	"PB LOOK-UP",
	"PB MISS",
	"BLOCK NUM BY PREFETCHING",
	"BLOCK CYCLE BY PREFETCHING",
	"TLB MISS",
	"FLPD MISS ON PREFETCHING",
};

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PTW ACCESS FAULT",
	"PAGE FAULT",
	"L1TLB MULTI-HIT FAULT",
	"ACCESS FAULT",
	"SECURITY FAULT",
	"UNKNOWN FAULT"
};

static char *sysmmu_clock_names[SYSMMU_CLK_NUM] = {"aclk", "pclk"};

static const char * const sysmmu_prop_opts[] = {
	[SYSMMU_PROP_RESERVED]		= "Reserved",
	[SYSMMU_PROP_READ]		= "r",
	[SYSMMU_PROP_WRITE]		= "w",
	[SYSMMU_PROP_READWRITE]		= "rw",	/* default */
};

static int iova_from_sent(sysmmu_pte_t *base, sysmmu_pte_t *sent)
{
	return ((unsigned long)sent - (unsigned long)base) *
					(SECT_SIZE / sizeof(sysmmu_pte_t));
}

struct sysmmu_list_data {
	struct device *sysmmu;
	struct list_head node; /* entry of exynos_iommu_owner.mmu_list */
};

#define has_sysmmu(dev)		(dev->archdata.iommu != NULL)
#define for_each_sysmmu_list(dev, sysmmu_list)			\
	list_for_each_entry(sysmmu_list,				\
		&((struct exynos_iommu_owner *)dev->archdata.iommu)->mmu_list,\
		node)

static struct exynos_iommu_owner *sysmmu_owner_list = NULL;
static struct sysmmu_drvdata *sysmmu_drvdata_list = NULL;

static struct kmem_cache *lv2table_kmem_cache;
static phys_addr_t fault_page;
static struct dentry *exynos_sysmmu_debugfs_root;

#ifdef CONFIG_ARM
static inline void pgtable_flush(void *vastart, void *vaend)
{
	dmac_flush_range(vastart, vaend);
	outer_flush_range(virt_to_phys(vastart),
				virt_to_phys(vaend));
}
#else /* ARM64 */
static inline void pgtable_flush(void *vastart, void *vaend)
{
	dma_sync_single_for_device(NULL,
			virt_to_phys(vastart),
			(size_t)(virt_to_phys(vaend) - virt_to_phys(vastart)),
			DMA_TO_DEVICE);
}
#endif

static bool has_sysmmu_capable_pbuf(void __iomem *sfrbase)
{
	unsigned long cfg = __raw_readl(sfrbase + REG_MMU_CAPA);

	return MMU_HAVE_PB(cfg);
}

static bool has_sysmmu_set_associative_tlb(void __iomem *sfrbase)
{
	u32 cfg = __raw_readl(sfrbase + REG_MMU_CAPA_1);

	return MMU_IS_TLB_CONFIGURABLE(cfg);
}

static bool has_sysmmu_l1_tlb(void __iomem *sfrbase)
{
	u32 cfg = __raw_readl(sfrbase + REG_MMU_CAPA);

	return MMU_IS_TLB_FULL_ASSOCIATIVE(cfg);
}

void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase, dma_addr_t iova)
{
	writel(iova | 0x1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

static void __sysmmu_tlb_invalidate_all(void __iomem *sfrbase)
{
	writel(0x1, sfrbase + REG_MMU_FLUSH);
}

void __sysmmu_tlb_invalidate(struct sysmmu_drvdata *drvdata,
				dma_addr_t iova, size_t size)
{
	void * __iomem sfrbase = drvdata->sfrbase;

	if (__raw_sysmmu_version(sfrbase) >= MAKE_MMU_VER(5, 1)) {
		__raw_writel(iova, sfrbase + REG_FLUSH_RANGE_START);
		__raw_writel(size - 1 + iova, sfrbase + REG_FLUSH_RANGE_END);
		writel(0x1, sfrbase + REG_MMU_FLUSH_RANGE);
		SYSMMU_EVENT_LOG_TLB_INV_RANGE(SYSMMU_DRVDATA_TO_LOG(drvdata),
						iova, iova + size);
	} else {
		if (sysmmu_block(sfrbase)) {
			__sysmmu_tlb_invalidate_all(sfrbase);
			SYSMMU_EVENT_LOG_TLB_INV_ALL(
					SYSMMU_DRVDATA_TO_LOG(drvdata));
		}
		sysmmu_unblock(sfrbase);
	}
}

void __sysmmu_set_ptbase(void __iomem *sfrbase, phys_addr_t pfn_pgtable)
{
	__raw_writel(pfn_pgtable, sfrbase + REG_PT_BASE_PPN);

	__sysmmu_tlb_invalidate_all(sfrbase);
}

static void __sysmmu_disable_pbuf(struct sysmmu_drvdata *drvdata,
				int target_grp)
{
	unsigned int i, num_pb;
	void * __iomem sfrbase = drvdata->sfrbase;

	if (target_grp >= 0)
		__raw_writel(target_grp << 8, sfrbase + REG_PB_INDICATE);

	__raw_writel(0, sfrbase + REG_PB_LMM);

	SYSMMU_EVENT_LOG_PBLMM(SYSMMU_DRVDATA_TO_LOG(drvdata), 0, 0);

	num_pb = PB_INFO_NUM(__raw_readl(sfrbase + REG_PB_INFO));
	for (i = 0; i < num_pb; i++) {
		__raw_writel((target_grp << 8) | i, sfrbase + REG_PB_INDICATE);
		__raw_writel(0, sfrbase + REG_PB_CFG);
		SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata), 0, 0, 0);
	}
}

static unsigned int find_lmm_preset(unsigned int num_pb, unsigned int num_bufs)
{
	static char lmm_preset[4][6] = {  /* [num of PB][num of buffers] */
	/*	  1,  2,  3,  4,  5,  6 */
		{ 1,  1,  0, -1, -1, -1}, /* num of pb: 3 */
		{ 3,  2,  1,  0, -1, -1}, /* num of pb: 4 */
		{-1, -1, -1, -1, -1, -1},
		{ 5,  5,  4,  2,  1,  0}, /* num of pb: 6 */
		};
	unsigned int lmm;

	BUG_ON(num_bufs > 6);
	lmm = lmm_preset[num_pb - 3][num_bufs - 1];
	BUG_ON(lmm == -1);
	return lmm;
}

static unsigned int find_num_pb(unsigned int num_pb, unsigned int lmm)
{
	static char lmm_preset[6][6] = { /* [pb_num - 1][pb_lmm] */
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{3, 2, 0, 0, 0, 0},
		{4, 3, 2, 1, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{6, 5, 4, 3, 3, 2},
	};

	num_pb = lmm_preset[num_pb - 1][lmm];
	BUG_ON(num_pb == 0);
	return num_pb;
}

static void __sysmmu_set_pbuf(struct sysmmu_drvdata *drvdata, int target_grp,
		struct sysmmu_prefbuf prefbuf[], int num_bufs)
{
	unsigned int i, num_pb, lmm;

	__raw_writel(target_grp << 8, drvdata->sfrbase + REG_PB_INDICATE);

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));

	lmm = find_lmm_preset(num_pb, (unsigned int)num_bufs);
	num_pb = find_num_pb(num_pb, lmm);

	__raw_writel(lmm, drvdata->sfrbase + REG_PB_LMM);

	SYSMMU_EVENT_LOG_PBLMM(SYSMMU_DRVDATA_TO_LOG(drvdata), lmm, num_bufs);

	for (i = 0; i < num_pb; i++) {
		__raw_writel(target_grp << 8 | i,
				drvdata->sfrbase + REG_PB_INDICATE);
		__raw_writel(0, drvdata->sfrbase + REG_PB_CFG);
		if ((prefbuf[i].size > 0) && (i < num_bufs)) {
			__raw_writel(prefbuf[i].base,
					drvdata->sfrbase + REG_PB_START_ADDR);
			__raw_writel(prefbuf[i].size - 1 + prefbuf[i].base,
					drvdata->sfrbase + REG_PB_END_ADDR);
			__raw_writel(prefbuf[i].config | 1,
					drvdata->sfrbase + REG_PB_CFG);

			SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata),
					prefbuf[i].config | 1, prefbuf[i].base,
					prefbuf[i].size - 1 + prefbuf[i].base);
		} else {
			if (i < num_bufs)
				dev_err(drvdata->sysmmu,
					"%s: Trying to init PB[%d/%d]with zero-size\n",
					__func__, i, num_bufs);
			SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata),
						0, 0, 0);
		}
	}
}

static void __sysmmu_set_pbuf_axi_id(struct sysmmu_drvdata *drvdata,
				struct pb_info *pb, unsigned int ipoption[],
				unsigned int opoption[])
{
	int i, j, num_pb, lmm;
	int ret_num_pb = 0;
	int total_plane_num = pb->ar_id_num + pb->aw_id_num;
	u32 opt;

	if (total_plane_num <= 0)
		return;

	if (pb->grp_num < 0) {
		pr_err("The group number(%d) is invalid\n", pb->grp_num);
		return;
	}

	__raw_writel(pb->grp_num << 8, drvdata->sfrbase + REG_PB_INDICATE);

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));

	lmm = find_lmm_preset(num_pb, total_plane_num);
	num_pb = find_num_pb(num_pb, lmm);

	__raw_writel(lmm, drvdata->sfrbase + REG_PB_LMM);

	ret_num_pb = min(pb->ar_id_num, num_pb);
	for (i = 0; i < ret_num_pb; i++) {
		__raw_writel((pb->grp_num << 8) | i,
				drvdata->sfrbase + REG_PB_INDICATE);
		__raw_writel(0, drvdata->sfrbase + REG_PB_CFG);
		__raw_writel((0xFFFF << 16) | pb->ar_axi_id[i],
			     drvdata->sfrbase + REG_PB_AXI_ID);
		opt = ipoption ? ipoption[i] : SYSMMU_PBUFCFG_DEFAULT_INPUT;
		__raw_writel(opt | 0x100001,
				drvdata->sfrbase + REG_PB_CFG);
	}

	if ((num_pb > ret_num_pb)) {
		for (i = ret_num_pb, j = 0; i < num_pb; i++, j++) {
			__raw_writel((pb->grp_num << 8) | i,
					drvdata->sfrbase + REG_PB_INDICATE);
			__raw_writel(0, drvdata->sfrbase + REG_PB_CFG);
			__raw_writel((0xFFFF << 16) | pb->aw_axi_id[j],
					drvdata->sfrbase + REG_PB_AXI_ID);
			opt = opoption ? opoption[i] : SYSMMU_PBUFCFG_DEFAULT_OUTPUT;
			__raw_writel(opt | 0x100001,
					drvdata->sfrbase + REG_PB_CFG);
		}
	}
}

static void __sysmmu_set_pbuf_property(struct sysmmu_drvdata *drvdata,
				struct pb_info *pb, unsigned int ipoption[],
				unsigned int opoption[])
{
	int i, num_pb, lmm;
	int ret_num_pb = 0;
	int total_plane_num = pb->ar_id_num + pb->aw_id_num;
	u32 opt;

	if (total_plane_num <= 0)
		return;

	if (pb->grp_num < 0) {
		pr_err("The group number(%d) is invalid\n", pb->grp_num);
		return;
	}

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));
	lmm = find_lmm_preset(num_pb, total_plane_num);
	num_pb = find_num_pb(num_pb, lmm);

	ret_num_pb = min(pb->ar_id_num, num_pb);
	for (i = 0; i < ret_num_pb; i++) {
		__raw_writel((pb->grp_num << 8) | i,
				drvdata->sfrbase + REG_PB_INDICATE);
		opt = ipoption ? ipoption[i] : SYSMMU_PBUFCFG_DEFAULT_INPUT;
		__raw_writel(opt | 0x100001,
				drvdata->sfrbase + REG_PB_CFG);
	}

	if ((num_pb > ret_num_pb)) {
		for (i = ret_num_pb; i < num_pb; i++) {
			__raw_writel((pb->grp_num << 8) | i,
					drvdata->sfrbase + REG_PB_INDICATE);
			opt = opoption ? opoption[i] : SYSMMU_PBUFCFG_DEFAULT_OUTPUT;
			__raw_writel(opt | 0x100001,
					drvdata->sfrbase + REG_PB_CFG);
		}
	}
}

static void __exynos_sysmmu_set_prefbuf_by_region(
			struct sysmmu_drvdata *drvdata, struct device *dev,
			struct sysmmu_prefbuf pb_reg[], unsigned int num_reg)
{
	struct pb_info *pb;
	unsigned int i;
	int orig_num_reg, num_bufs = 0;
	struct sysmmu_prefbuf prefbuf[6];

	if (!has_sysmmu_capable_pbuf(drvdata->sfrbase))
		return;

	if ((num_reg == 0) || (pb_reg == NULL)) {
		/* Disabling prefetch buffers */
		__sysmmu_disable_pbuf(drvdata, -1);
		return;
	}

	orig_num_reg = num_reg;

	list_for_each_entry(pb, &drvdata->pb_grp_list, node) {
		if (pb->master == dev) {
			for (i = 0; i < orig_num_reg; i++) {
				if (((pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
						(pb->prop & SYSMMU_PROP_WRITE)) ||
					(!(pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
						 (pb->prop & SYSMMU_PROP_READ))) {
					if (num_reg > 0)
						num_reg--;
					else if (num_reg == 0)
						break;

					prefbuf[num_bufs++] = pb_reg[i];
				}
			}
			if (num_bufs)
				__sysmmu_set_pbuf(drvdata, pb->grp_num, prefbuf,
						num_bufs);
			num_bufs = 0;
		}
	}
}

static void __exynos_sysmmu_set_prefbuf_axi_id(struct sysmmu_drvdata *drvdata,
			struct device *master, unsigned int inplanes,
			unsigned int onplanes, unsigned int ipoption[],
			unsigned int opoption[])
{
	struct pb_info *pb;

	if (!has_sysmmu_capable_pbuf(drvdata->sfrbase))
		return;

	list_for_each_entry(pb, &drvdata->pb_grp_list, node) {
		if (master) {
			if (pb->master == master) {
				struct pb_info tpb;
				memcpy(&tpb, pb, sizeof(tpb));
				tpb.ar_id_num = inplanes;
				tpb.aw_id_num = onplanes;
				__sysmmu_set_pbuf_axi_id(drvdata, &tpb,
						ipoption, opoption);
			}
			continue;
		}
		__sysmmu_set_pbuf_axi_id(drvdata, pb,
				ipoption, opoption);
	}
}

static void __exynos_sysmmu_set_prefbuf_property(struct sysmmu_drvdata *drvdata,
			struct device *master, unsigned int inplanes,
			unsigned int onplanes, unsigned int ipoption[],
			unsigned int opoption[])
{
	struct pb_info *pb;

	if (!has_sysmmu_capable_pbuf(drvdata->sfrbase))
		return;

	if (!master)
		return;

	list_for_each_entry(pb, &drvdata->pb_grp_list, node) {
		if (pb->master == master) {
			struct pb_info tpb;
			memcpy(&tpb, pb, sizeof(tpb));
			tpb.ar_id_num = inplanes;
			tpb.aw_id_num = onplanes;
			__sysmmu_set_pbuf_property(drvdata, &tpb,
					ipoption, opoption);
		}
	}
}

void dump_l2_tlb(void __iomem *sfrbase, u32 tlb_entry_count)
{
	if (MMU_TLB_ENTRY_VALID(__raw_readl(sfrbase + REG_L2TLB_ENTRY_ATTR))) {
		pr_crit("[%03d] VPN: %#010x, PPN: %#010x, ATTR: %#010x\n",
			tlb_entry_count,
			__raw_readl(sfrbase + REG_L2TLB_ENTRY_VPN),
			__raw_readl(sfrbase + REG_L2TLB_ENTRY_PPN),
			__raw_readl(sfrbase + REG_L2TLB_ENTRY_ATTR));
	}
}

void dump_sysmmu_tlb_pb(void __iomem *sfrbase)
{
	int i, j, k, capa, num_pb, lmm;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	phys_addr_t phys;

	pgd = pgd_offset_k((unsigned long)sfrbase);
	if (!pgd) {
		pr_crit("Invalid virtual address %p\n", sfrbase);
		return;
	}

	pud = pud_offset(pgd, (unsigned long)sfrbase);
	if (!pud) {
		pr_crit("Invalid virtual address %p\n", sfrbase);
		return;
	}

	pmd = pmd_offset(pud, (unsigned long)sfrbase);
	if (!pmd) {
		pr_crit("Invalid virtual address %p\n", sfrbase);
		return;
	}

	pte = pte_offset_kernel(pmd, (unsigned long)sfrbase);
	if (!pte) {
		pr_crit("Invalid virtual address %p\n", sfrbase);
		return;
	}

	capa = __raw_readl(sfrbase + REG_MMU_CAPA);
	lmm = MMU_RAW_VER(__raw_readl(sfrbase + REG_MMU_VERSION));

	phys = pte_pfn(*pte) << PAGE_SHIFT;

	pr_auto(ASL4, "ADDR: %pa(VA: %p), MMU_CTRL: %#010x, PT_BASE: %#010x\n",
		&phys, sfrbase,
		__raw_readl(sfrbase + REG_MMU_CTRL),
		__raw_readl(sfrbase + REG_PT_BASE_PPN));
	pr_auto(ASL4, "VERSION %d.%d.%d, MMU_CFG: %#010x, MMU_STATUS: %#010x\n",
		MMU_MAJ_VER(lmm), MMU_MIN_VER(lmm), MMU_REV_VER(lmm),
		__raw_readl(sfrbase + REG_MMU_CFG),
		__raw_readl(sfrbase + REG_MMU_STATUS));

	pr_crit("---------- TLB -----------------------------------\n");

	for (i = 0; i < MMU_NUM_L1TLB_ENTRIES(capa); i++) {
		__raw_writel(i, sfrbase + REG_L1TLB_READ_ENTRY);
		pr_crit("[%02d] VPN: %#010x, PPN: %#010x, ATTR: %#010x\n",
			i, __raw_readl(sfrbase + REG_L1TLB_ENTRY_VPN),
			__raw_readl(sfrbase + REG_L1TLB_ENTRY_PPN),
			__raw_readl(sfrbase + REG_L1TLB_ENTRY_ATTR));
	}

	/*
	 * 6.0.2 sysmmu has only 1 TLB, the number of entry is 512
	 */
	if (!has_sysmmu_l1_tlb(sfrbase)) {
		u32 capa_1 = __raw_readl(sfrbase + REG_MMU_CAPA_1);
		u32 line_size =
			(__raw_readl(sfrbase + REG_L2TLB_CFG) >> 4) &
				MMU_MASK_LINE_SIZE;
		u32 tlb_set_num = MMU_MAX_TLB_SET(capa_1) >> line_size;
		u32 tlb_line_num = MMU_MIN_TLB_LINE(capa_1) * tlb_set_num;
		u32 tlb_read_entry = 0;
		u32 count = 0;
		for (i = 0; i < tlb_set_num; i++) {
			for (j = 0; j < MMU_TLB_WAY(capa_1); j++) {
				for (k = 0; k < tlb_line_num; k++) {
					tlb_read_entry =
					MMU_SET_TLB_READ_ENTRY(i, j, k);
					__raw_writel(tlb_read_entry,
					sfrbase + REG_L2TLB_READ_ENTRY);
					dump_l2_tlb(sfrbase, count++);
				}
			}
		}
	}

	if (!has_sysmmu_capable_pbuf(sfrbase))
		return;

	pr_crit("---------- Prefetch Buffers ------------------------------\n");

	for (i = 0; i < MMU_PB_GRP_NUM(capa); i++) {
		__raw_writel(i << 8, sfrbase + REG_PB_INDICATE);

		num_pb = PB_INFO_NUM(__raw_readl(sfrbase + REG_PB_INFO));
		lmm = __raw_readl(sfrbase + REG_PB_LMM);
		pr_crit("PB_INFO[%d]: %#010x, PB_LMM: %#010x\n",
				i, num_pb, lmm);

		num_pb = find_num_pb(num_pb, lmm);
		for (j = 0; j < num_pb; j++) {
			__raw_writel((i << 8) | j, sfrbase + REG_PB_INDICATE);
			pr_crit("PB[%d][%d] = CFG: %#010x, AXI ID: %#010x, ", i,
				j, __raw_readl(sfrbase + REG_PB_CFG),
				__raw_readl(sfrbase + REG_PB_AXI_ID));
			pr_crit("PB[%d][%d] START: %#010x, END: %#010x\n", i, j,
				__raw_readl(sfrbase + REG_PB_START_ADDR),
				__raw_readl(sfrbase + REG_PB_END_ADDR));
			pr_crit("SPB0 START: %#010x, END: %#010x, VALID: %#010x\n",
				__raw_readl(sfrbase + REG_PCI_SPB0_SVPN),
				__raw_readl(sfrbase + REG_PCI_SPB0_EVPN),
				__raw_readl(sfrbase + REG_PCI_SPB0_SLOT_VALID));
			pr_crit("SPB1 START: %#010x, END: %#010x, VALID: %#010x\n",
				__raw_readl(sfrbase + REG_PCI_SPB1_SVPN),
				__raw_readl(sfrbase + REG_PCI_SPB1_EVPN),
				__raw_readl(sfrbase + REG_PCI_SPB1_SLOT_VALID));
		}
	}
}

static void show_fault_information(struct sysmmu_drvdata *drvdata,
				   int flags, unsigned long fault_addr)
{
	unsigned int info;
	phys_addr_t pgtable;
	int fault_id = SYSMMU_FAULT_ID(flags);
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	char temp_buf[SZ_128];
#endif

	pgtable = __raw_readl(drvdata->sfrbase + REG_PT_BASE_PPN);
	pgtable <<= PAGE_SHIFT;

	pr_auto_once(4);
	pr_auto(ASL4, "----------------------------------------------------------\n");
	pr_auto(ASL4, "%s %s %s at %#010lx (page table @ %pa)\n",
		dev_name(drvdata->sysmmu),
		(flags & IOMMU_FAULT_WRITE) ? "WRITE" : "READ",
		sysmmu_fault_name[fault_id], fault_addr, &pgtable);

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	snprintf(temp_buf, SZ_128, "%s %s %s at %#010lx (%pa)",
		dev_name(drvdata->sysmmu),
		(flags & IOMMU_FAULT_WRITE) ? "WRITE" : "READ",
		sysmmu_fault_name[fault_id], fault_addr, &pgtable);
	sec_debug_set_extra_info_sysmmu(temp_buf);
#endif

	if (fault_id == SYSMMU_FAULT_UNKNOWN) {
		pr_auto(ASL4, "The fault is not caused by this System MMU.\n");
		pr_auto(ASL4, "Please check IRQ and SFR base address.\n");
		goto finish;
	}

	info = __raw_readl(drvdata->sfrbase +
			((flags & IOMMU_FAULT_WRITE) ?
			REG_FAULT_AW_TRANS_INFO : REG_FAULT_AR_TRANS_INFO));

	pr_auto(ASL4, "AxID: %#x, AxLEN: %#x\n", info & 0xFFFF, (info >> 16) & 0xF);

	if (pgtable != drvdata->pgtable) {
		pr_auto(ASL4, "Page table base of driver: %pa\n",
			&drvdata->pgtable);
	}

	if (fault_id == SYSMMU_FAULT_PTW_ACCESS){
		pr_auto(ASL4, "System MMU has failed to access page table\n");
	}

	if (!pfn_valid(pgtable >> PAGE_SHIFT)) {
		pr_auto(ASL4, "Page table base is not in a valid memory region\n");
	} else {
		sysmmu_pte_t *ent;

		ent = section_entry(phys_to_virt(pgtable), fault_addr);
		pr_auto(ASL4, "Lv1 entry: %#010x\n", *ent);

		if (lv1ent_page(ent)) {
			ent = page_entry(ent, fault_addr);
			pr_auto(ASL4, "Lv2 entry: %#010x\n", *ent);
		}
	}

	dump_sysmmu_tlb_pb(drvdata->sfrbase);

finish:

	pr_auto(ASL4, "----------------------------------------------------------\n");
	pr_auto_disable(4);
}

#define REG_INT_STATUS_WRITE_BIT 16

irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	/* SYSMMU is in blocked when interrupt occurred. */
	struct sysmmu_drvdata *drvdata = dev_id;
	unsigned int itype;
	unsigned long addr = -1;
	int flags = 0;

	WARN(!is_sysmmu_active(drvdata),
		"Fault occurred while System MMU %s is not enabled!\n",
		dev_name(drvdata->sysmmu));

	itype =  __ffs(__raw_readl(drvdata->sfrbase + REG_INT_STATUS));
	if (itype >= REG_INT_STATUS_WRITE_BIT) {
		itype -= REG_INT_STATUS_WRITE_BIT;
		flags = IOMMU_FAULT_WRITE;
	}

	if (WARN_ON(!(itype < SYSMMU_FAULT_UNKNOWN)))
		itype = SYSMMU_FAULT_UNKNOWN;
	else
		addr = __raw_readl(drvdata->sfrbase +
				((flags & IOMMU_FAULT_WRITE) ?
				 REG_FAULT_AW_ADDR : REG_FAULT_AR_ADDR));
	flags |= SYSMMU_FAULT_FLAG(itype);

	show_fault_information(drvdata, flags, addr);

	atomic_notifier_call_chain(&drvdata->fault_notifiers, addr, &flags);

	panic("Unrecoverable System MMU Fault!!");

	return IRQ_HANDLED;
}

void __sysmmu_set_tlb_line_size(void __iomem *sfrbase)
{
	u32 cfg =  __raw_readl(sfrbase + REG_L2TLB_CFG);
	cfg &= ~MMU_MASK_LINE_SIZE;
	cfg |= MMU_DEFAULT_LINE_SIZE;
	__raw_writel(cfg, sfrbase + REG_L2TLB_CFG);
}

void __sysmmu_init_config(struct sysmmu_drvdata *drvdata)
{
	unsigned long cfg;

	__raw_writel(CTRL_BLOCK, drvdata->sfrbase + REG_MMU_CTRL);

	cfg = CFG_FLPDCACHE;

	if (!(drvdata->prop & SYSMMU_PROP_DISABLE_ACG))
		cfg |= CFG_ACGEN;

	if (!(drvdata->qos < 0))
		cfg |= CFG_QOS_OVRRIDE | CFG_QOS(drvdata->qos);

	if (has_sysmmu_capable_pbuf(drvdata->sfrbase))
		__exynos_sysmmu_set_prefbuf_axi_id(drvdata, NULL, 0, 0,
				NULL, NULL);
	if (has_sysmmu_set_associative_tlb(drvdata->sfrbase))
		__sysmmu_set_tlb_line_size(drvdata->sfrbase);

	cfg |= __raw_readl(drvdata->sfrbase + REG_MMU_CFG) & ~CFG_MASK;
	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);
}

static void sysmmu_tlb_invalidate_entry(struct device *dev, dma_addr_t iova,
					bool force)
{
	struct sysmmu_list_data *list;

	for_each_sysmmu_list(dev, list) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		if (!force && !(drvdata->prop & SYSMMU_PROP_NONBLOCK_TLBINV))
			continue;

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) &&
				is_sysmmu_runtime_active(drvdata)) {
			TRACE_LOG_DEV(drvdata->sysmmu,
				"TLB invalidation @ %#x\n", iova);
			__sysmmu_tlb_invalidate_entry(drvdata->sfrbase, iova);
			SYSMMU_EVENT_LOG_TLB_INV_VPN(
					SYSMMU_DRVDATA_TO_LOG(drvdata), iova);
		} else {
			TRACE_LOG_DEV(drvdata->sysmmu,
				"Skip TLB invalidation @ %#x\n", iova);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

void exynos_sysmmu_tlb_invalidate(struct iommu_domain *domain, dma_addr_t start,
				  size_t size)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct exynos_iommu_owner *owner;
	struct sysmmu_list_data *list;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	list_for_each_entry(owner, &priv->clients, client) {
		for_each_sysmmu_list(owner->dev, list) {
			struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

			if (!!(drvdata->prop & SYSMMU_PROP_NONBLOCK_TLBINV))
				continue;

			spin_lock(&drvdata->lock);
			if (!is_sysmmu_active(drvdata) ||
					!is_sysmmu_runtime_active(drvdata)) {
				spin_unlock(&drvdata->lock);
				TRACE_LOG_DEV(drvdata->sysmmu,
					"Skip TLB invalidation %#x@%#x\n", size, start);
				continue;
			}

			TRACE_LOG_DEV(drvdata->sysmmu,
				"TLB invalidation %#x@%#x\n", size, start);

			__sysmmu_tlb_invalidate(drvdata, start, size);

			spin_unlock(&drvdata->lock);
		}
	}
	spin_unlock_irqrestore(&priv->lock, flags);
}

static inline void __sysmmu_disable_nocount(struct sysmmu_drvdata *drvdata)
{
	int disable = (drvdata->prop & SYSMMU_PROP_STOP_BLOCK) ?
					CTRL_BLOCK_DISABLE : CTRL_DISABLE;

	__raw_sysmmu_disable(drvdata->sfrbase, disable);

	__sysmmu_clk_disable(drvdata);

	SYSMMU_EVENT_LOG_DISABLE(SYSMMU_DRVDATA_TO_LOG(drvdata));

	TRACE_LOG("%s(%s)\n", __func__, dev_name(drvdata->sysmmu));
}

static bool __sysmmu_disable(struct sysmmu_drvdata *drvdata)
{
	bool disabled;
	unsigned long flags;

	spin_lock_irqsave(&drvdata->lock, flags);

	disabled = set_sysmmu_inactive(drvdata);

	if (disabled) {
		drvdata->pgtable = 0;
		drvdata->domain = NULL;

		if (is_sysmmu_runtime_active(drvdata))
			__sysmmu_disable_nocount(drvdata);

		TRACE_LOG_DEV(drvdata->sysmmu, "Disabled\n");
	} else  {
		TRACE_LOG_DEV(drvdata->sysmmu, "%d times left to disable\n",
					drvdata->activations);
	}

	spin_unlock_irqrestore(&drvdata->lock, flags);

	return disabled;
}

static void __sysmmu_enable_nocount(struct sysmmu_drvdata *drvdata)
{
	__sysmmu_clk_enable(drvdata);

	__sysmmu_init_config(drvdata);

	__sysmmu_set_ptbase(drvdata->sfrbase, drvdata->pgtable / PAGE_SIZE);

	__raw_sysmmu_enable(drvdata->sfrbase);

	SYSMMU_EVENT_LOG_ENABLE(SYSMMU_DRVDATA_TO_LOG(drvdata));

	TRACE_LOG_DEV(drvdata->sysmmu, "Really enabled\n");
}

static int __sysmmu_enable(struct sysmmu_drvdata *drvdata,
			phys_addr_t pgtable, struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&drvdata->lock, flags);
	if (set_sysmmu_active(drvdata)) {
		drvdata->pgtable = pgtable;
		drvdata->domain = domain;

		if (is_sysmmu_runtime_active(drvdata))
			__sysmmu_enable_nocount(drvdata);

		TRACE_LOG_DEV(drvdata->sysmmu, "Enabled\n");
	} else {
		ret = (pgtable == drvdata->pgtable) ? 1 : -EBUSY;

		TRACE_LOG_DEV(drvdata->sysmmu, "Already enabled (%d)\n", ret);
	}

	if (WARN_ON(ret < 0))
		set_sysmmu_inactive(drvdata); /* decrement count */

	spin_unlock_irqrestore(&drvdata->lock, flags);

	return ret;
}

/* __exynos_sysmmu_enable: Enables System MMU
 *
 * returns -error if an error occurred and System MMU is not enabled,
 * 0 if the System MMU has been just enabled and 1 if System MMU was already
 * enabled before.
 */
static int __exynos_sysmmu_enable(struct device *dev, phys_addr_t pgtable,
				struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;

	BUG_ON(!has_sysmmu(dev));

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);
		ret = __sysmmu_enable(drvdata, pgtable, domain);
		if (ret < 0) {
			struct sysmmu_list_data *iter;
			for_each_sysmmu_list(dev, iter) {
				if (iter == list)
					break;
				__sysmmu_disable(dev_get_drvdata(iter->sysmmu));
			}
			break;
		}
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return ret;
}

int exynos_sysmmu_enable(struct device *dev, unsigned long pgtable)
{
	int ret;

	BUG_ON(!memblock_is_memory(pgtable));

	ret = __exynos_sysmmu_enable(dev, pgtable, NULL);

	return ret;
}

bool exynos_sysmmu_disable(struct device *dev)
{
	unsigned long flags;
	bool disabled = true;
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;

	BUG_ON(!has_sysmmu(dev));

	spin_lock_irqsave(&owner->lock, flags);

	/* Every call to __sysmmu_disable() must return same result */
	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);
		disabled = __sysmmu_disable(drvdata);
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return disabled;
}

void sysmmu_set_prefetch_buffer_by_region(struct device *dev,
			struct sysmmu_prefbuf pb_reg[], unsigned int num_reg)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;
	unsigned long flags;

	if (!dev->archdata.iommu) {
		dev_err(dev, "%s: No System MMU is configured\n", __func__);
		return;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock(&drvdata->lock);

		if (!is_sysmmu_active(drvdata) ||
				!is_sysmmu_runtime_active(drvdata)) {
			spin_unlock(&drvdata->lock);
			continue;
		}

		if (sysmmu_block(drvdata->sfrbase)) {
			__exynos_sysmmu_set_prefbuf_by_region(
					drvdata, dev, pb_reg, num_reg);
			sysmmu_unblock(drvdata->sfrbase);
		}

		spin_unlock(&drvdata->lock);
	}

	spin_unlock_irqrestore(&owner->lock, flags);
}

int sysmmu_set_prefetch_buffer_property(struct device *dev,
			unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption[], unsigned int opoption[])
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;
	unsigned long flags;

	if (!dev->archdata.iommu) {
		dev_err(dev, "%s: No System MMU is configured\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock(&drvdata->lock);

		if (!is_sysmmu_active(drvdata) ||
			!is_sysmmu_runtime_active(drvdata)) {
			spin_unlock(&drvdata->lock);
			continue;
		}

		__exynos_sysmmu_set_prefbuf_property(drvdata, dev,
				inplanes, onplanes, ipoption, opoption);

		spin_unlock(&drvdata->lock);
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return 0;
}

static int __init __sysmmu_init_clock(struct device *sysmmu,
					struct sysmmu_drvdata *drvdata)
{
	int i, ret;

	/* Initialize SYSMMU clocks */
	for (i = 0; i < SYSMMU_CLK_NUM; i++)
		drvdata->clocks[i] = ERR_PTR(-ENOENT);

	for (i = 0; i < SYSMMU_CLK_NUM; i++) {
		drvdata->clocks[i] =
			devm_clk_get(sysmmu, sysmmu_clock_names[i]);
		if (IS_ERR(drvdata->clocks[i]) &&
			!(drvdata->clocks[i] == ERR_PTR(-ENOENT))) {
			dev_err(sysmmu, "Failed to get sysmmu %s clock\n",
				sysmmu_clock_names[i]);
			return PTR_ERR(drvdata->clocks[i]);
		} else if (drvdata->clocks[i] == ERR_PTR(-ENOENT)) {
			continue;
		}

		ret = clk_prepare(drvdata->clocks[i]);
		if (ret) {
			dev_err(sysmmu, "Failed to prepare sysmmu  %s clock\n",
					sysmmu_clock_names[i]);
			while (i-- > 0) {
				if (!IS_ERR(drvdata->clocks[i]))
					clk_unprepare(drvdata->clocks[i]);
			}
			return ret;
		}

	}

	return 0;
}

int __sysmmu_init_pb_info(struct device *sysmmu, struct device *master,
		    struct sysmmu_drvdata *data, struct device_node *pb_node,
		    struct of_phandle_args *pb_args, int grp_num)
{
	struct pb_info *pb;
	const char *s;
	int i, ret;

	pb = devm_kzalloc(sysmmu, sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(sysmmu, "%s: failed to allocate pb_info[%d]\n",
				__func__, grp_num);
		return -ENOMEM;
	}

	pb->master = master;

	pb->grp_num = grp_num;
	for (i = 0; i < MAX_NUM_PBUF; i++) {
		pb->ar_axi_id[i] = -1;
		pb->aw_axi_id[i] = -1;
	}

	INIT_LIST_HEAD(&pb->node);

	for (i = 0; i < pb_args->args_count; i++) {
		if (is_axi_id(pb_args->args[i])) {
			if (is_ar_axi_id(pb_args->args[i])) {
				pb->ar_axi_id[pb->ar_id_num] = pb_args->args[i];
				pb->ar_id_num++;
			} else {
				pb->aw_axi_id[pb->aw_id_num] =
					pb_args->args[i] & AXIID_MASK;
				pb->aw_id_num++;
			}
		}
	}

	ret = of_property_read_string(pb_node, "dir", &s);
	if (!ret) {
		int val;
		for (val = 1; val < ARRAY_SIZE(sysmmu_prop_opts); val++) {
			if (!strcasecmp(s, sysmmu_prop_opts[val])) {
				pb->prop &= ~SYSMMU_PROP_RW_MASK;
				pb->prop |= val;
				break;
			}
		}
	} else if (ret && ret == -EINVAL) {
		pb->prop = SYSMMU_PROP_READWRITE;
	} else {
		dev_err(sysmmu, "%s: failed to get PB Direction of %s\n",
				__func__, pb_args->np->full_name);
		devm_kfree(sysmmu, pb);
		return ret;
	}

	list_add_tail(&pb->node, &data->pb_grp_list);

	dev_info(sysmmu, "device node[%d] : %s\n",
			pb->grp_num, pb_args->np->name);
	dev_info(sysmmu, "ar[%d] = {%d, %d, %d, %d, %d, %d}\n",
			pb->ar_id_num,
			pb->ar_axi_id[0], pb->ar_axi_id[1],
			pb->ar_axi_id[2], pb->ar_axi_id[3],
			pb->ar_axi_id[4], pb->ar_axi_id[5]);
	dev_info(sysmmu, "aw[%d]= {%d, %d, %d, %d, %d, %d}\n",
			pb->aw_id_num,
			pb->aw_axi_id[0], pb->aw_axi_id[1],
			pb->aw_axi_id[2], pb->aw_axi_id[3],
			pb->aw_axi_id[4], pb->aw_axi_id[5]);
	return 0;
}

int __sysmmu_update_owner(struct device *master, struct device *sysmmu)
{
	struct exynos_iommu_owner *owner;
	struct sysmmu_list_data *list_data;

	owner = master->archdata.iommu;
	if (!owner) {
		owner = kzalloc(sizeof(*owner), GFP_KERNEL);
		if (!owner) {
			dev_err(master, "%s: Failed to allocate owner structure\n",
					__func__);
			return -ENOMEM;
		}

		INIT_LIST_HEAD(&owner->mmu_list);
		INIT_LIST_HEAD(&owner->client);
		owner->dev = master;
		spin_lock_init(&owner->lock);

		master->archdata.iommu = owner;
		if (!sysmmu_owner_list) {
			sysmmu_owner_list = owner;
		} else {
			owner->next = sysmmu_owner_list->next;
			sysmmu_owner_list->next = owner;
		}
	}

	list_for_each_entry(list_data, &owner->mmu_list, node)
		if (list_data->sysmmu == sysmmu)
			return 0;

	list_data = devm_kzalloc(sysmmu, sizeof(*list_data), GFP_KERNEL);
	if (!list_data) {
		dev_err(sysmmu, "%s: Failed to allocate sysmmu_list_data\n",
				__func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&list_data->node);
	list_data->sysmmu = sysmmu;

	/*
	 * System MMUs are attached in the order of the presence
	 * in device tree
	 */
	list_add_tail(&list_data->node, &owner->mmu_list);
	dev_info(master, "--> %s\n", dev_name(sysmmu));

	return 0;
}

static struct platform_device * __init __sysmmu_init_owner(struct device *sysmmu,
							struct sysmmu_drvdata *data,
							struct of_phandle_args *pb_args)
{
	struct device_node *master_node = pb_args->np;
	struct platform_device *master;
	int ret;

	master = of_find_device_by_node(master_node);
	if (!master) {
		pr_err("%s: failed to get master device in '%s'\n",
				__func__, master_node->full_name);
		return ERR_PTR(-EINVAL);
	}

	ret = __sysmmu_update_owner(&master->dev, sysmmu);
	if (ret) {
		pr_err("%s: failed to update iommu owner '%s'\n",
				__func__, dev_name(&master->dev));
		of_node_put(master_node);
		return ERR_PTR(-EINVAL);
	}

	of_node_put(master_node);

	return master;
}

static int __init __sysmmu_init_master_info(struct device *sysmmu,
					struct sysmmu_drvdata *data)
{
	struct device_node *node;
	struct device_node *pb_info;
	int grp_num = 0;
	int ret = 0;

	pb_info = of_get_child_by_name(sysmmu->of_node, "pb-info");
	if (!pb_info) {
		pr_info("%s: 'master-info' node not found from '%s' node\n",
			__func__, dev_name(sysmmu));
		return 0;
	}

	INIT_LIST_HEAD(&data->pb_grp_list);

	for_each_child_of_node(pb_info, node) {
		struct of_phandle_args pb_args;
		struct platform_device *master;
		int i, master_cnt = 0;

		master_cnt = of_count_phandle_with_args(node,
				"master_axi_id_list",
				"#pb-id-cells");

		for (i = 0; i < master_cnt; i++) {
			memset(&pb_args, 0x0, sizeof(pb_args));
			ret = of_parse_phandle_with_args(node,
					"master_axi_id_list",
					"#pb-id-cells", i, &pb_args);
			if (ret) {
				of_node_put(node);
				pr_err("%s: failed to get PB info of %s\n",
					__func__, dev_name(data->sysmmu));
				return ret;
			}

			master = __sysmmu_init_owner(sysmmu, data, &pb_args);
			if (IS_ERR(master)) {
				of_node_put(node);
				of_node_put(pb_args.np);
				pr_err("%s: failed to initialize sysmmu(%s)"
					"owner info\n", __func__,
					dev_name(data->sysmmu));
				return PTR_ERR(master);
			}

			ret = __sysmmu_init_pb_info(sysmmu, &master->dev, data,
					node, &pb_args, grp_num);
			if (ret) {
				of_node_put(node);
				of_node_put(pb_args.np);
				pr_err("%s: failed to update pb axi id '%s'\n",
						__func__, dev_name(sysmmu));
				break;
			}

			of_node_put(pb_args.np);
		}

		if (ret) {
			struct pb_info *pb;
			while (!list_empty(&data->pb_grp_list)) {
				pb = list_entry(data->pb_grp_list.next,
						struct pb_info, node);
				list_del(&pb->node);
				kfree(pb);
			}
		}

		of_node_put(node);
		grp_num++;
	}

	return ret;
}

static int __init __sysmmu_init_prop(struct device *sysmmu,
				     struct sysmmu_drvdata *drvdata)
{
	struct device_node *prop_node;
	const char *s;
	unsigned int qos = DEFAULT_QOS_VALUE;
	int ret;

	ret = of_property_read_u32_index(sysmmu->of_node, "qos", 0, &qos);

	if ((ret == 0) && (qos > 15)) {
		dev_err(sysmmu, "%s: Invalid QoS value %d specified\n",
				__func__, qos);
		qos = DEFAULT_QOS_VALUE;
	}

	drvdata->qos = (short)qos;

	/**
	 * Deprecate 'prop-map' child node of System MMU device nodes in FDT.
	 * It is not required to introduce new child node for boolean
	 * properties like 'block-stop' and 'tlbinv-nonblock'.
	 * 'tlbinv-nonblock' is H/W W/A to accellerates master H/W performance
	 * for 5.x and the earlier versions of System MMU.x.
	 * 'sysmmu,tlbinv-nonblock' is introduced, instead for those earlier
	 * versions.
	 * Instead of 'block-stop' in 'prop-map' childe node,
	 * 'sysmmu,block-when-stop' without a value is introduced to simplify
	 * the FDT node definitions.
	 * For the compatibility with the existing FDT files, the 'prop-map'
	 * child node parsing is still kept.
	 */
	prop_node = of_get_child_by_name(sysmmu->of_node, "prop-map");
	if (prop_node) {
		if (!of_property_read_string(prop_node, "tlbinv-nonblock", &s))
			if (strnicmp(s, "yes", 3) == 0)
				drvdata->prop |= SYSMMU_PROP_NONBLOCK_TLBINV;

		if (!of_property_read_string(prop_node, "block-stop", &s))
			if (strnicmp(s, "yes", 3) == 0)
				drvdata->prop |= SYSMMU_PROP_STOP_BLOCK;

		of_node_put(prop_node);
	}

	if (of_find_property(sysmmu->of_node, "sysmmu,block-when-stop", NULL))
		drvdata->prop |= SYSMMU_PROP_STOP_BLOCK;

	if (of_find_property(sysmmu->of_node, "sysmmu,tlbinv-nonblock", NULL))
		drvdata->prop |= SYSMMU_PROP_NONBLOCK_TLBINV;

	if (of_find_property(sysmmu->of_node, "sysmmu,acg_disable", NULL))
		drvdata->prop |= SYSMMU_PROP_DISABLE_ACG;

	return 0;
}

static int __init __sysmmu_setup(struct device *sysmmu,
				struct sysmmu_drvdata *drvdata)
{
	int ret;

	ret = __sysmmu_init_prop(sysmmu, drvdata);
	if (ret) {
		dev_err(sysmmu, "Failed to initialize sysmmu properties\n");
		return ret;
	}

	ret = __sysmmu_init_clock(sysmmu, drvdata);
	if (ret) {
		dev_err(sysmmu, "Failed to initialize gating clocks\n");
		return ret;
	}

	ret = __sysmmu_init_master_info(sysmmu, drvdata);
	if (ret) {
		int i;
		for (i = 0; i < SYSMMU_CLK_NUM; i++) {
			if (!IS_ERR(drvdata->clocks[i]))
				clk_unprepare(drvdata->clocks[i]);
		}
		dev_err(sysmmu, "Failed to initialize master device.\n");
	}

	return ret;
}

static int __init exynos_sysmmu_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct sysmmu_drvdata *data;
	struct resource *res;

	data = devm_kzalloc(dev, sizeof(*data) , GFP_KERNEL);
	if (!data) {
		dev_err(dev, "Not enough memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Unable to find IOMEM region\n");
		return -ENOENT;
	}

	data->sfrbase = devm_ioremap_resource(dev, res);
	if (!data->sfrbase) {
		dev_err(dev, "Unable to map IOMEM @ PA:%pa\n", &res->start);
		return -EBUSY;
	}

	ret = platform_get_irq(pdev, 0);
	if (ret <= 0) {
		dev_err(dev, "Unable to find IRQ resource\n");
		return ret;
	}

	ret = devm_request_irq(dev, ret, exynos_sysmmu_irq, 0,
				dev_name(dev), data);
	if (ret) {
		dev_err(dev, "Unabled to register interrupt handler\n");
		return ret;
	}

	pm_runtime_enable(dev);

	ret = exynos_iommu_init_event_log(SYSMMU_DRVDATA_TO_LOG(data),
					SYSMMU_LOG_LEN);
	if (!ret)
		sysmmu_add_log_to_debugfs(exynos_sysmmu_debugfs_root,
				SYSMMU_DRVDATA_TO_LOG(data), dev_name(dev));
	else
		return ret;

	ret = __sysmmu_setup(dev, data);
	if (!ret) {
		if (!pm_runtime_enabled(dev))
			get_sysmmu_runtime_active(data);
		data->sysmmu = dev;
		ATOMIC_INIT_NOTIFIER_HEAD(&data->fault_notifiers);
		spin_lock_init(&data->lock);
		if (!sysmmu_drvdata_list) {
			sysmmu_drvdata_list = data;
		} else {
			data->next = sysmmu_drvdata_list->next;
			sysmmu_drvdata_list->next = data;
		}

		platform_set_drvdata(pdev, data);

		dev_info(dev, "[OK]\n");
	}

	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id sysmmu_of_match[] __initconst = {
	{ .compatible = "samsung,exynos7420-sysmmu", },
	{ },
};
#endif

static int exynos_iommu_domain_init(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgtable = (sysmmu_pte_t *)__get_free_pages(
					GFP_KERNEL | __GFP_ZERO, 2);
	if (!priv->pgtable)
		goto err_pgtable;

	priv->lv2entcnt = (atomic_t *)__get_free_pages(
					GFP_KERNEL | __GFP_ZERO, 2);
	if (!priv->lv2entcnt)
		goto err_counter;

	if (exynos_iommu_init_event_log(IOMMU_PRIV_TO_LOG(priv), IOMMU_LOG_LEN))
		goto err_init_event_log;

	pgtable_flush(priv->pgtable, priv->pgtable + NUM_LV1ENTRIES);

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->pgtablelock);
	INIT_LIST_HEAD(&priv->clients);

	domain->priv = priv;

	return 0;

err_init_event_log:
	free_pages((unsigned long)priv->lv2entcnt, 2);
err_counter:
	free_pages((unsigned long)priv->pgtable, 2);
err_pgtable:
	kfree(priv);
	return -ENOMEM;
}

static void exynos_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct exynos_iommu_owner *owner;
	unsigned long flags;
	int i;

	WARN_ON(!list_empty(&priv->clients));

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(owner, &priv->clients, client)
		while (!exynos_sysmmu_disable(owner->dev))
			; /* until System MMU is actually disabled */

	while (!list_empty(&priv->clients))
		list_del_init(priv->clients.next);

	spin_unlock_irqrestore(&priv->lock, flags);

	for (i = 0; i < NUM_LV1ENTRIES; i++)
		if (lv1ent_page(priv->pgtable + i))
			kmem_cache_free(lv2table_kmem_cache,
					__va(lv2table_base(priv->pgtable + i)));

	free_pages((unsigned long)priv->pgtable, 2);
	free_pages((unsigned long)priv->lv2entcnt, 2);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int exynos_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct exynos_iommu_domain *priv = domain->priv;
	phys_addr_t pgtable = virt_to_phys(priv->pgtable);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&priv->lock, flags);

	ret = __exynos_sysmmu_enable(dev, __pa(priv->pgtable), domain);

	spin_unlock_irqrestore(&priv->lock, flags);

	if (ret < 0) {
		dev_err(dev, "%s: Failed to attach IOMMU with pgtable %pa\n",
				__func__, &pgtable);
	} else {
		SYSMMU_EVENT_LOG_IOMMU_ATTACH(IOMMU_PRIV_TO_LOG(priv), dev);
		TRACE_LOG_DEV(dev,
			"%s: Attached new IOMMU with pgtable %pa %s\n",
			__func__, &pgtable, (ret == 0) ? "" : ", again");
		ret = 0;
	}

	return ret;
}

static void exynos_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct exynos_iommu_owner *owner;
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(owner, &priv->clients, client) {
		if (owner == dev->archdata.iommu) {
			exynos_sysmmu_disable(owner->dev);
			break;
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (owner == dev->archdata.iommu) {
		SYSMMU_EVENT_LOG_IOMMU_DETACH(IOMMU_PRIV_TO_LOG(priv), dev);
		TRACE_LOG_DEV(dev, "%s: Detached IOMMU with pgtable %#lx\n",
					__func__, __pa(priv->pgtable));
	} else {
		dev_err(dev, "%s: No IOMMU is attached\n", __func__);
	}
}

static sysmmu_pte_t *alloc_lv2entry(struct exynos_iommu_domain *priv,
	sysmmu_pte_t *sent, unsigned long iova, atomic_t *pgcounter)
{
	if (lv1ent_fault(sent)) {
		unsigned long flags;
		spin_lock_irqsave(&priv->pgtablelock, flags);
		if (lv1ent_fault(sent)) {
			sysmmu_pte_t *pent;

			pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
			BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
			if (!pent) {
				spin_unlock_irqrestore(&priv->pgtablelock, flags);
				return ERR_PTR(-ENOMEM);
			}

			*sent = mk_lv1ent_page(__pa(pent));
			kmemleak_ignore(pent);
			atomic_set(pgcounter, NUM_LV2ENTRIES);
			pgtable_flush(pent, pent + NUM_LV2ENTRIES);
			pgtable_flush(sent, sent + 1);
			SYSMMU_EVENT_LOG_IOMMU_ALLOCSLPD(IOMMU_PRIV_TO_LOG(priv),
							iova & SECT_MASK);
		}
		spin_unlock_irqrestore(&priv->pgtablelock, flags);
	} else if (!lv1ent_page(sent)) {
		BUG();
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
}

static int lv1ent_check_page(struct exynos_iommu_domain *priv,
				sysmmu_pte_t *sent, atomic_t *pgcnt)
{
	if (lv1ent_page(sent)) {
		if (WARN_ON(atomic_read(pgcnt) != NUM_LV2ENTRIES))
			return -EADDRINUSE;
	}

	return 0;
}

static void clear_lv1_page_table(sysmmu_pte_t *ent, int n)
{
	if (n > 0)
		memset(ent, 0, sizeof(*ent) * n);
}

static void clear_lv2_page_table(sysmmu_pte_t *ent, int n)
{
	if (n > 0)
		memset(ent, 0, sizeof(*ent) * n);
}

static int lv1set_section(struct exynos_iommu_domain *priv,
			sysmmu_pte_t *sent, phys_addr_t paddr,
			  size_t size, atomic_t *pgcnt)
{
	int ret;

	if (WARN_ON(!lv1ent_fault(sent) && !lv1ent_page(sent)))
		return -EADDRINUSE;

	if (size == SECT_SIZE) {
		ret = lv1ent_check_page(priv, sent, pgcnt);
		if (ret)
			return ret;
		*sent = mk_lv1ent_sect(paddr);
		pgtable_flush(sent, sent + 1);
	} else if (size == DSECT_SIZE) {
		int i;
		for (i = 0; i < SECT_PER_DSECT; i++, sent++, pgcnt++) {
			ret = lv1ent_check_page(priv, sent, pgcnt);
			if (ret) {
				clear_lv1_page_table(sent - i, i);
				return ret;
			}
			*sent = mk_lv1ent_dsect(paddr);
		}
		pgtable_flush(sent - SECT_PER_DSECT, sent);
	} else {
		int i;
		for (i = 0; i < SECT_PER_SPSECT; i++, sent++, pgcnt++) {
			ret = lv1ent_check_page(priv, sent, pgcnt);
			if (ret) {
				clear_lv1_page_table(sent - i, i);
				return ret;
			}
			*sent = mk_lv1ent_spsect(paddr);
		}
		pgtable_flush(sent - SECT_PER_SPSECT, sent);
	}

	return 0;
}

static int lv2set_page(sysmmu_pte_t *pent, phys_addr_t paddr,
		       size_t size, atomic_t *pgcnt)
{
	if (size == SPAGE_SIZE) {
		if (WARN_ON(!lv2ent_fault(pent)))
			return -EADDRINUSE;

		*pent = mk_lv2ent_spage(paddr);
		pgtable_flush(pent, pent + 1);
		atomic_dec(pgcnt);
	} else { /* size == LPAGE_SIZE */
		int i;
		for (i = 0; i < SPAGES_PER_LPAGE; i++, pent++) {
			if (WARN_ON(!lv2ent_fault(pent))) {
				clear_lv2_page_table(pent - i, i);
				return -EADDRINUSE;
			}

			*pent = mk_lv2ent_lpage(paddr);
		}
		pgtable_flush(pent - SPAGES_PER_LPAGE, pent);
		atomic_sub(SPAGES_PER_LPAGE, pgcnt);
	}

	return 0;
}

static int exynos_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct exynos_iommu_domain *priv = domain->priv;
	sysmmu_pte_t *entry;
	int ret = -ENOMEM;

	BUG_ON(priv->pgtable == NULL);

	entry = section_entry(priv->pgtable, iova);

	if (size >= SECT_SIZE) {
		ret = lv1set_section(priv, entry, paddr, size,
				&priv->lv2entcnt[lv1ent_offset(iova)]);
	} else {
		sysmmu_pte_t *pent;
		pent = alloc_lv2entry(priv, entry, iova,
				&priv->lv2entcnt[lv1ent_offset(iova)]);
		if (IS_ERR(pent)) {
			ret = PTR_ERR(pent);
		} else {
			ret = lv2set_page(pent, paddr, size,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
		}
	}

	if (ret)
		pr_err("%s: Failed(%d) to map %#zx bytes @ %pa\n",
			__func__, ret, size, &iova);


	return ret;
}

static void exynos_iommu_tlb_invalidate_entry(struct exynos_iommu_domain *priv,
					unsigned long iova)
{
	struct exynos_iommu_owner *owner;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(owner, &priv->clients, client)
		sysmmu_tlb_invalidate_entry(owner->dev, iova, false);

	spin_unlock_irqrestore(&priv->lock, flags);
}

static size_t exynos_iommu_unmap(struct iommu_domain *domain,
					unsigned long iova, size_t size)
{
	struct exynos_iommu_domain *priv = domain->priv;
	size_t err_pgsize;
	sysmmu_pte_t *sent, *pent;
	atomic_t *lv2entcnt = &priv->lv2entcnt[lv1ent_offset(iova)];

	BUG_ON(priv->pgtable == NULL);

	sent = section_entry(priv->pgtable, iova);

	if (lv1ent_spsection(sent)) {
		if (WARN_ON(size < SPSECT_SIZE)) {
			err_pgsize = SPSECT_SIZE;
			goto err;
		}

		clear_lv1_page_table(sent, SECT_PER_SPSECT);

		pgtable_flush(sent, sent + SECT_PER_SPSECT);
		size = SPSECT_SIZE;
		goto done;
	}

	if (lv1ent_dsection(sent)) {
		if (WARN_ON(size < DSECT_SIZE)) {
			err_pgsize = DSECT_SIZE;
			goto err;
		}

		*sent = 0;
		*(++sent) = 0;
		pgtable_flush(sent, sent + 2);
		size = DSECT_SIZE;
		goto done;
	}

	if (lv1ent_section(sent)) {
		if (WARN_ON(size < SECT_SIZE)) {
			err_pgsize = SECT_SIZE;
			goto err;
		}

		*sent = 0;
		pgtable_flush(sent, sent + 1);
		size = SECT_SIZE;
		goto done;
	}

	if (unlikely(lv1ent_fault(sent))) {
		if (size > SECT_SIZE)
			size = SECT_SIZE;
		goto done;
	}

	/* lv1ent_page(sent) == true here */

	pent = page_entry(sent, iova);

	if (unlikely(lv2ent_fault(pent))) {
		size = SPAGE_SIZE;
		goto done;
	}

	if (lv2ent_small(pent)) {
		*pent = 0;
		size = SPAGE_SIZE;
		pgtable_flush(pent, pent + 1);
		atomic_inc(lv2entcnt);
		goto unmap_flpd;
	}

	/* lv1ent_large(ent) == true here */
	if (WARN_ON(size < LPAGE_SIZE)) {
		err_pgsize = LPAGE_SIZE;
		goto err;
	}

	clear_lv2_page_table(pent, SPAGES_PER_LPAGE);
	pgtable_flush(pent, pent + SPAGES_PER_LPAGE);
	size = LPAGE_SIZE;
	atomic_add(SPAGES_PER_LPAGE, lv2entcnt);

unmap_flpd:
	if (atomic_read(lv2entcnt) == NUM_LV2ENTRIES) {
		unsigned long flags;
		spin_lock_irqsave(&priv->pgtablelock, flags);
		if (atomic_read(lv2entcnt) == NUM_LV2ENTRIES) {
			kmem_cache_free(lv2table_kmem_cache,
					page_entry(sent, 0));
			atomic_set(lv2entcnt, 0);
			*sent = 0;

			SYSMMU_EVENT_LOG_IOMMU_FREESLPD(IOMMU_PRIV_TO_LOG(priv), iova_from_sent(priv->pgtable, sent));
		}
		spin_unlock_irqrestore(&priv->pgtablelock, flags);
	}

done:
	exynos_iommu_tlb_invalidate_entry(priv, iova);

	/* TLB invalidation is performed by IOVMM */
	return size;

err:
	pr_err("%s: Failed: size(%#zx) @ %pa is smaller than page size %#zx\n",
		__func__, size, &iova, err_pgsize);

	return 0;
}

static phys_addr_t exynos_iommu_iova_to_phys(struct iommu_domain *domain,
					     dma_addr_t iova)
{
	struct exynos_iommu_domain *priv = domain->priv;
	sysmmu_pte_t *entry;
	phys_addr_t phys = 0;

	entry = section_entry(priv->pgtable, iova);

	if (lv1ent_spsection(entry)) {
		phys = spsection_phys(entry) + spsection_offs(iova);
	} else if (lv1ent_dsection(entry)) {
		phys = dsection_phys(entry) + dsection_offs(iova);
	} else if (lv1ent_section(entry)) {
		phys = section_phys(entry) + section_offs(iova);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, iova);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry) + lpage_offs(iova);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry) + spage_offs(iova);
	}

	return phys;
}

static int exynos_iommu_add_device(struct device *dev)
{
	/*
	 * implement not yet
	 */
	return 0;
}

static struct iommu_ops exynos_iommu_ops = {
	.domain_init = &exynos_iommu_domain_init,
	.domain_destroy = &exynos_iommu_domain_destroy,
	.attach_dev = &exynos_iommu_attach_device,
	.detach_dev = &exynos_iommu_detach_device,
	.map = &exynos_iommu_map,
	.unmap = &exynos_iommu_unmap,
	.iova_to_phys = &exynos_iommu_iova_to_phys,
	.pgsize_bitmap = PGSIZE_BITMAP,
	.add_device = &exynos_iommu_add_device,
};

static int __sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr,
					exynos_iova_t iova,
					size_t size)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iovmm *vmm = owner->vmm_data;
	struct iommu_domain *domain = vmm->domain;
	struct exynos_iommu_domain *priv = domain->priv;
	struct vm_area_struct *vma;
	unsigned long start = vaddr & PAGE_MASK;
	unsigned long end = PAGE_ALIGN(vaddr + size);
	bool is_pfnmap;
	sysmmu_pte_t *sent, *pent;
	int ret = 0;

	down_read(&mm->mmap_sem);

	BUG_ON((vaddr + size) < vaddr);
	/*
	 * Assumes that the VMA is safe.
	 * The caller must check the range of address space before calling this.
	 */
	vma = find_vma(mm, vaddr);
	if (!vma) {
		pr_err("%s: vma is null\n", __func__);
		ret = -EINVAL;
		goto out_unmap;
	}

	if (vma->vm_end < (vaddr + size)) {
		pr_err("%s: vma overflow: %#lx--%#lx, vaddr: %#lx, size: %zd\n",
			__func__, vma->vm_start, vma->vm_end, vaddr, size);
		ret = -EINVAL;
		goto out_unmap;
	}

	is_pfnmap = vma->vm_flags & VM_PFNMAP;

	TRACE_LOG_DEV(dev, "%s: unmap starts @ %#zx@%#lx\n",
			__func__, size, start);

	do {
		sysmmu_pte_t *pent_first;

		sent = section_entry(priv->pgtable, iova);
		if (lv1ent_fault(sent)) {
			ret = -EFAULT;
			goto out_unmap;
		}

		pent = page_entry(sent, iova);
		if (lv2ent_fault(pent)) {
			ret = -EFAULT;
			goto out_unmap;
		}

		pent_first = pent;

		do {
			if (!lv2ent_fault(pent) && !is_pfnmap)
				put_page(phys_to_page(spage_phys(pent)));

			*pent = 0;
			if (lv2ent_offset(iova) == NUM_LV2ENTRIES - 1) {
				pgtable_flush(pent_first, pent);
				iova += PAGE_SIZE;
				sent = section_entry(priv->pgtable, iova);
				if (lv1ent_fault(sent)) {
					ret = -EFAULT;
					goto out_unmap;
				}

				pent = page_entry(sent, iova);
				if (lv2ent_fault(pent)) {
					ret = -EFAULT;
					goto out_unmap;
				}

				pent_first = pent;
			} else {
				iova += PAGE_SIZE;
				pent++;
			}
		} while (start += PAGE_SIZE, start != end);

		if (pent_first != pent)
			pgtable_flush(pent_first, pent);
	} while (start != end);

	TRACE_LOG_DEV(dev, "%s: unmap done @ %#lx\n", __func__, start);

out_unmap:
	up_read(&mm->mmap_sem);

	if (ret) {
		pr_debug("%s: Ignoring unmapping for %#lx ~ %#lx\n",
					__func__, start, end);
	}

	return ret;
}

static sysmmu_pte_t *alloc_lv2entry_fast(struct exynos_iommu_domain *priv,
		sysmmu_pte_t *sent, unsigned long iova)
{
	if (lv1ent_fault(sent)) {
		sysmmu_pte_t *pent;

		pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return ERR_PTR(-ENOMEM);

		*sent = mk_lv1ent_page(virt_to_phys(pent));
		kmemleak_ignore(pent);
		pgtable_flush(sent, sent + 1);
	} else if (WARN_ON(!lv1ent_page(sent))) {
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
}

static int mm_fault_translate(int fault)
{
	if (fault & VM_FAULT_OOM)
		return -ENOMEM;
	else if (fault & (VM_FAULT_SIGBUS | VM_FAULT_SIGSEGV))
		return -EBUSY;
	else if (fault & (VM_FAULT_HWPOISON | VM_FAULT_HWPOISON_LARGE))
		return -EFAULT;
	else if (fault & VM_FAULT_FALLBACK)
		return -EAGAIN;

	return -EFAULT;
}

int exynos_sysmmu_map_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr,
					exynos_iova_t iova,
					size_t size, bool write,
					bool shareable)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iovmm *vmm = owner->vmm_data;
	struct iommu_domain *domain = vmm->domain;
	struct exynos_iommu_domain *priv = domain->priv;
	exynos_iova_t iova_start = iova;
	struct vm_area_struct *vma;
	unsigned long start, end;
	unsigned long pgd_next;
	int ret = -EINVAL;
	bool is_pfnmap;
	pgd_t *pgd;

	if (WARN_ON(size == 0))
		return 0;

	down_read(&mm->mmap_sem);

	/*
	 * Assumes that the VMA is safe.
	 * The caller must check the range of address space before calling this.
	 */
	vma = find_vma(mm, vaddr);
	if (!vma) {
		pr_err("%s: vma is null\n", __func__);
		up_read(&mm->mmap_sem);
		return -EINVAL;
	}

	if (vma->vm_end < (vaddr + size)) {
		pr_err("%s: vma overflow: %#lx--%#lx, vaddr: %#lx, size: %zd\n",
			__func__, vma->vm_start, vma->vm_end, vaddr, size);
		up_read(&mm->mmap_sem);
		return -EINVAL;
	}

	is_pfnmap = vma->vm_flags & VM_PFNMAP;

	start = vaddr & PAGE_MASK;
	end = PAGE_ALIGN(vaddr + size);

	TRACE_LOG_DEV(dev, "%s: map @ %#lx--%#lx, %zd bytes, vm_flags: %#lx\n",
			__func__, start, end, size, vma->vm_flags);

	pgd = pgd_offset(mm, start);
	do {
		unsigned long pmd_next;
		pmd_t *pmd;
		pud_t *pud;

		pud = pud_offset(pgd, start);
		if (pud_none(*pud)) {
			ret = handle_mm_fault(mm, vma, start,
				write ? FAULT_FLAG_WRITE : 0);
			if (ret & VM_FAULT_ERROR) {
				ret = mm_fault_translate(ret);
				goto out_unmap;
			}
		} else if (pud_bad(*pud)) {
			pud_clear_bad(pud);
			ret = -EBADR;
			goto out_unmap;
		}

		pgd_next = pgd_addr_end(start, end);
		pmd = pmd_offset((pud_t *)pgd, start);

		do {
			pte_t *pte;
			sysmmu_pte_t *pent, *pent_first;
			sysmmu_pte_t *sent;
			spinlock_t *ptl;

			if (pmd_none(*pmd)) {
				ret = handle_mm_fault(mm, vma, start,
						write ? FAULT_FLAG_WRITE : 0);
				if (ret & VM_FAULT_ERROR) {
					ret = mm_fault_translate(ret);
					goto out_unmap;
				}
			} else if (pmd_bad(*pmd)) {
				pr_err("%s: bad pmd value %#lx\n", __func__,
						(unsigned long)pmd_val(*pmd));
				pmd_clear_bad(pmd);
				ret = -EBADR;
				goto out_unmap;
			}

			pmd_next = pmd_addr_end(start, pgd_next);
			pte = pte_offset_map(pmd, start);

			sent = section_entry(priv->pgtable, iova);
			pent = alloc_lv2entry_fast(priv, sent, iova);
			if (IS_ERR(pent)) {
				ret = PTR_ERR(pent); /* ENOMEM or EADDRINUSE */
				goto out_unmap;
			}

			pent_first = pent;
			ptl = pte_lockptr(mm, pmd);

			spin_lock(ptl);
			do {
				WARN_ON(!lv2ent_fault(pent));

				if (!pte_present(*pte) ||
					(write && !pte_write(*pte))) {
					if (pte_present(*pte) || pte_none(*pte)) {
						spin_unlock(ptl);
						ret = handle_mm_fault(mm, vma, start,
							write ? FAULT_FLAG_WRITE : 0);
						if (ret & VM_FAULT_ERROR) {
							ret = mm_fault_translate(ret);
							goto out_unmap;
						}
						spin_lock(ptl);
					}
				}

				if (!pte_present(*pte) ||
					(write && !pte_write(*pte))) {
					ret = -EACCES;
					spin_unlock(ptl);
					goto out_unmap;
				}

				if (!is_pfnmap)
					get_page(pte_page(*pte));
				*pent = mk_lv2ent_spage(__pfn_to_phys(
							pte_pfn(*pte)));
				if (shareable)
					set_lv2ent_shareable(pent);

				if (lv2ent_offset(iova) == (NUM_LV2ENTRIES - 1)) {
					pgtable_flush(pent_first, pent);
					iova += PAGE_SIZE;
					sent = section_entry(priv->pgtable, iova);
					pent = alloc_lv2entry_fast(priv, sent, iova);
					if (IS_ERR(pent)) {
						ret = PTR_ERR(pent);
						spin_unlock(ptl);
						goto out_unmap;
					}
					pent_first = pent;
				} else {
					iova += PAGE_SIZE;
					pent++;
				}
			} while (pte++, start += PAGE_SIZE, start < pmd_next);

			if (pent_first != pent)
				pgtable_flush(pent_first, pent);
			spin_unlock(ptl);
		} while (pmd++, start = pmd_next, start != pgd_next);

	} while (pgd++, start = pgd_next, start != end);

	ret = 0;
out_unmap:
	up_read(&mm->mmap_sem);

	if (ret) {
		pr_debug("%s: Ignoring mapping for %#lx ~ %#lx\n",
					__func__, start, end);
		__sysmmu_unmap_user_pages(dev, mm, vaddr, iova_start,
					start - (vaddr & PAGE_MASK));
	}

	return ret;
}

int exynos_sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr,
					exynos_iova_t iova,
					size_t size)
{
	if (WARN_ON(size == 0))
		return 0;

	return __sysmmu_unmap_user_pages(dev, mm, vaddr, iova, size);
}

static int __init exynos_iommu_create_domain(void)
{
	struct device_node *domain;

	for_each_compatible_node(domain, NULL, "samsung,exynos-iommu-bus") {
		struct device_node *np;
		struct exynos_iovmm *vmm = NULL;
		int i = 0;

		while ((np = of_parse_phandle(domain, "domain-clients", i++))) {
			struct platform_device *master =
						of_find_device_by_node(np);
			struct exynos_iommu_owner *owner;
			struct exynos_iommu_domain *priv;

			if (!master) {
				pr_err("%s: master IP in '%s' not found\n",
						__func__, np->name);
				of_node_put(np);
				of_node_put(domain);
				return -ENOENT;
			}

			owner = (struct exynos_iommu_owner *)
					master->dev.archdata.iommu;
			if (!owner) {
				pr_err("%s: No System MMU attached for %s\n",
						__func__, np->name);
				of_node_put(np);
				continue;
			}

			if (!vmm) {
				vmm = exynos_create_single_iovmm(np->name);
				if (IS_ERR(vmm)) {
					pr_err("%s: Failed to create IOVM space\
							of %s\n",
							__func__, np->name);
					of_node_put(np);
					of_node_put(domain);
					return -ENOMEM;
				}
			}

			priv = (struct exynos_iommu_domain *)vmm->domain->priv;

			owner->vmm_data = vmm;
			spin_lock(&priv->lock);
			list_add_tail(&owner->client, &priv->clients);
			spin_unlock(&priv->lock);

			of_node_put(np);

			dev_err(&master->dev,
				"create IOVMM device node : %s\n", np->name);
		}
		of_node_put(domain);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sysmmu_resume(struct device *dev)
{
	unsigned long flags;
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(dev);

	TRACE_LOG("%s(%s) ----->\n", __func__, dev_name(dev));

	spin_lock_irqsave(&drvdata->lock, flags);
	if (is_sysmmu_active(drvdata) &&
			(!pm_runtime_enabled(dev) ||
			is_sysmmu_runtime_active(drvdata)))
		__sysmmu_enable_nocount(drvdata);
	spin_unlock_irqrestore(&drvdata->lock, flags);

	TRACE_LOG("<----- %s(%s)\n", __func__, dev_name(dev));

	return 0;
}

static int sysmmu_suspend(struct device *dev)
{
	unsigned long flags;
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(dev);

	TRACE_LOG("%s(%s) ----->\n", __func__, dev_name(dev));

	spin_lock_irqsave(&drvdata->lock, flags);
	if (is_sysmmu_active(drvdata) &&
			(!pm_runtime_enabled(dev) ||
			 is_sysmmu_runtime_active(drvdata)))
		__sysmmu_disable_nocount(drvdata);
	spin_unlock_irqrestore(&drvdata->lock, flags);

	TRACE_LOG("<----- %s(%s)\n", __func__, dev_name(dev));

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sysmmu_runtime_resume(struct device *dev)
{
	struct sysmmu_list_data *list;
	int (*cb)(struct device *__dev);

	TRACE_LOG("%s(%s) ----->\n", __func__, dev_name(dev));

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *data = dev_get_drvdata(list->sysmmu);
		unsigned long flags;

		TRACE_LOG("%s(%s)\n", __func__, dev_name(data->sysmmu));

		SYSMMU_EVENT_LOG_POWERON(SYSMMU_DRVDATA_TO_LOG(data));

		spin_lock_irqsave(&data->lock, flags);
		if (get_sysmmu_runtime_active(data) && is_sysmmu_active(data))
			__sysmmu_enable_nocount(data);
		spin_unlock_irqrestore(&data->lock, flags);
	}

	if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_resume;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_resume;

	TRACE_LOG("<----- %s(%s)\n", __func__, dev_name(dev));

	return cb ? cb(dev) : 0;
}

static int sysmmu_runtime_suspend(struct device *dev)
{
	struct sysmmu_list_data *list;
	int (*cb)(struct device *__dev);
	int ret = 0;

	TRACE_LOG("%s(%s) ----->\n", __func__, dev_name(dev));

	if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_suspend;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_suspend;

	if (cb) {
		ret = cb(dev);
		if (ret) {
			dev_err(dev, "failed runtime_suspend cb func\n");
			return ret;
		}
	}

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *data = dev_get_drvdata(list->sysmmu);
		unsigned long flags;

		TRACE_LOG("%s(%s)\n", __func__, dev_name(data->sysmmu));

		SYSMMU_EVENT_LOG_POWEROFF(SYSMMU_DRVDATA_TO_LOG(data));

		spin_lock_irqsave(&data->lock, flags);
		if (put_sysmmu_runtime_active(data) && is_sysmmu_active(data))
			__sysmmu_disable_nocount(data);
		spin_unlock_irqrestore(&data->lock, flags);
	}

	TRACE_LOG("<----- %s(%s)\n", __func__, dev_name(dev));

	return ret;
}
#endif

static const struct dev_pm_ops sysmmu_pm_ops_from_master = {
	SET_RUNTIME_PM_OPS(sysmmu_runtime_suspend, sysmmu_runtime_resume,
			NULL)
};

static const struct dev_pm_ops sysmmu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sysmmu_suspend, sysmmu_resume)
};

struct class exynos_iommu_class = {
	.name	= "exynos_iommu_class",
	.pm	= &sysmmu_pm_ops_from_master,
};

static struct platform_driver exynos_sysmmu_driver __refdata = {
	.probe		= exynos_sysmmu_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= MODULE_NAME,
		.of_match_table = of_match_ptr(sysmmu_of_match),
		.pm		= &sysmmu_pm_ops,
	}
};

static int __init exynos_iommu_init(void)
{
	struct page *page;
	int ret = -ENOMEM;

	lv2table_kmem_cache = kmem_cache_create("exynos-iommu-lv2table",
		LV2TABLE_SIZE, LV2TABLE_SIZE, 0, NULL);
	if (!lv2table_kmem_cache) {
		pr_err("%s: failed to create kmem cache\n", __func__);
		return -ENOMEM;
	}

	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!page) {
		pr_err("%s: failed to allocate fault page\n", __func__);
		goto err_fault_page;
	}
	fault_page = page_to_phys(page);

	ret = bus_set_iommu(&platform_bus_type, &exynos_iommu_ops);
	if (ret) {
		pr_err("%s: Failed to register IOMMU ops\n", __func__);
		goto err_set_iommu;
	}

	exynos_sysmmu_debugfs_root = debugfs_create_dir("sysmmu", NULL);
	if (!exynos_sysmmu_debugfs_root)
		pr_err("%s: Failed to create debugfs entry\n", __func__);

	ret = platform_driver_register(&exynos_sysmmu_driver);
	if (ret) {
		pr_err("%s: Failed to register System MMU driver.\n", __func__);
		goto err_driver_register;
	}

	ret = exynos_iommu_create_domain();
	if (ret && (ret != -ENOENT)) {
		pr_err("%s: Failed to create iommu domain\n", __func__);
		platform_driver_unregister(&exynos_sysmmu_driver);
		goto err_driver_register;
	}

	return 0;
err_driver_register:
	bus_set_iommu(&platform_bus_type, NULL);
err_set_iommu:
	__free_page(page);
err_fault_page:
	kmem_cache_destroy(lv2table_kmem_cache);
	return ret;
}
arch_initcall_sync(exynos_iommu_init);

static int sysmmu_hook_driver_register(struct notifier_block *nb,
					unsigned long val,
					void *p)
{
	struct device *dev = p;

	/*
	 * No System MMU assigned. See exynos_sysmmu_probe().
	 */
	if (dev->archdata.iommu == NULL)
		return 0;

	switch (val) {
	case BUS_NOTIFY_BIND_DRIVER:
	{
		if (dev->class) {
			dev_err(dev, "Already exist dev->class\n");
			return -EBUSY;
		}

		dev->class = &exynos_iommu_class;
		dev_info(dev, "exynos-iommu class is added\n");

		break;
	}
	case BUS_NOTIFY_BOUND_DRIVER:
	{
		struct sysmmu_list_data *list;

		if (pm_runtime_enabled(dev))
			break;

		for_each_sysmmu_list(dev, list) {
			struct sysmmu_drvdata *data =
					dev_get_drvdata(list->sysmmu);
			unsigned long flags;
			spin_lock_irqsave(&data->lock, flags);
			if (is_sysmmu_active(data) &&
					get_sysmmu_runtime_active(data))
				__sysmmu_enable_nocount(data);
			pm_runtime_disable(data->sysmmu);
			spin_unlock_irqrestore(&data->lock, flags);
		}

		break;
	}
	case BUS_NOTIFY_UNBOUND_DRIVER:
	{
		struct exynos_iommu_owner *owner = dev->archdata.iommu;
		WARN_ON(!list_empty(&owner->client));
		if (dev->class)
			dev->class = NULL;
		dev_info(dev, "exynos-iommu class is removed\n");
		break;
	}
	} /* switch (val) */

	return 0;
}

static struct notifier_block sysmmu_notifier = {
	.notifier_call = &sysmmu_hook_driver_register,
};

static int __init exynos_iommu_prepare(void)
{
	return bus_register_notifier(&platform_bus_type, &sysmmu_notifier);
}
subsys_initcall_sync(exynos_iommu_prepare);

static int __init exynos_iommu_dpm_move_to_first(void)
{
	struct sysmmu_drvdata *data = sysmmu_drvdata_list;
	struct device *sysmmu;

	BUG_ON(!data);

	for (; data != NULL; data = data->next) {
		sysmmu = data->sysmmu;
		device_move(sysmmu, NULL, DPM_ORDER_DEV_FIRST);
		dev_info(sysmmu, "dpm order to first\n");
	}

	return 0;
}
late_initcall_sync(exynos_iommu_dpm_move_to_first);

static int sysmmu_fault_notifier(struct notifier_block *nb,
				     unsigned long fault_addr, void *data)
{
	struct exynos_iommu_owner *owner = NULL;
	struct exynos_iovmm *vmm;

	owner = container_of(nb, struct exynos_iommu_owner, nb);

	if (owner && owner->fault_handler) {
		vmm = exynos_get_iovmm(owner->dev);
		if (vmm && vmm->domain)
			owner->fault_handler(vmm->domain, owner->dev,
					fault_addr, (unsigned long)data,
					owner->token);
	}

	return 0;
}

int exynos_sysmmu_add_fault_notifier(struct device *dev,
		iommu_fault_handler_t handler, void *token)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;
	struct sysmmu_drvdata *drvdata;
	unsigned long flags;
	int ret;

	if (!has_sysmmu(dev)) {
		dev_info(dev, "%s doesn't have sysmmu\n", dev_name(dev));
		return -EINVAL;
	}

	spin_lock_irqsave(&owner->lock, flags);

	owner->fault_handler = handler;
	owner->token = token;
	owner->nb.notifier_call = sysmmu_fault_notifier;

	for_each_sysmmu_list(dev, list) {
		drvdata = dev_get_drvdata(list->sysmmu);
		ret = atomic_notifier_chain_register(
				&drvdata->fault_notifiers, &owner->nb);
		if (ret) {
			dev_err(dev,
				"Failed to register %s's fault notifier\n",
				dev_name(dev));
			goto err;
		}

	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return 0;

err:
	for_each_sysmmu_list(dev, list) {
		drvdata = dev_get_drvdata(list->sysmmu);
		atomic_notifier_chain_unregister(
			&drvdata->fault_notifiers, &owner->nb);
	}
	spin_unlock_irqrestore(&owner->lock, flags);

	return ret;
}

void iovmm_set_fault_handler(struct device *dev,
			     iommu_fault_handler_t handler, void *token)
{
	int ret;

	ret = exynos_sysmmu_add_fault_notifier(dev, handler, token);
	if (ret)
		dev_err(dev, "Failed to %s's fault notifier\n", dev_name(dev));
}

static void sysmmu_dump_lv2_page_table(unsigned int lv1idx, sysmmu_pte_t *base)
{
	unsigned int i;
	for (i = 0; i < NUM_LV2ENTRIES; i += 4) {
		if (!base[i] && !base[i + 1] && !base[i + 2] && !base[i + 3])
			continue;
		pr_info("    LV2[%04d][%03d] %08x %08x %08x %08x\n",
			lv1idx, i,
			base[i], base[i + 1], base[i + 2], base[i + 3]);
	}
}

static void sysmmu_dump_page_table(sysmmu_pte_t *base)
{
	unsigned int i;
	phys_addr_t phys_base = virt_to_phys(base);

	pr_info("---- System MMU Page Table @ %pa ----\n", &phys_base);

	for (i = 0; i < NUM_LV1ENTRIES; i += 4) {
		unsigned int j;
		if (!base[i])
			continue;
		pr_info("LV1[%04d] %08x %08x %08x %08x\n",
			i, base[i], base[i + 1], base[i + 2], base[i + 3]);

		for (j = 0; j < 4; j++)
			if (lv1ent_page(&base[i + j]))
				sysmmu_dump_lv2_page_table(i + j,
						page_entry(&base[i + j], 0));
	}
}

void exynos_sysmmu_show_status(struct device *dev)
{
	struct sysmmu_list_data *list;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		if (!is_sysmmu_active(drvdata) ||
				!is_sysmmu_runtime_active(drvdata)) {
			dev_info(drvdata->sysmmu,
				"%s: System MMU is not active\n", __func__);
			continue;
		}

		pr_info("DUMPING SYSTEM MMU: %s\n", dev_name(drvdata->sysmmu));

		if (sysmmu_block(drvdata->sfrbase))
			dump_sysmmu_tlb_pb(drvdata->sfrbase);
		else
			pr_err("!!Failed to block Sytem MMU!\n");
		sysmmu_unblock(drvdata->sfrbase);
	}
}

void exynos_sysmmu_dump_pgtable(struct device *dev)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list =
		list_entry(&owner->mmu_list, struct sysmmu_list_data, node);
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

	sysmmu_dump_page_table(phys_to_virt(drvdata->pgtable));
}

void dump_sysmmu_ppc_cnt(struct sysmmu_drvdata *drvdata)
{
	unsigned int cfg;
	int i;

	pr_crit("------------- System MMU PPC Status --------------\n");
	for (i = 0; i < drvdata->event_cnt; i++) {
		cfg = __raw_readl(drvdata->sfrbase +
				REG_PPC_EVENT_SEL(i));
		pr_crit("%s %s %s CNT : %d", dev_name(drvdata->sysmmu),
			cfg & 0x10 ? "WRITE" : "READ",
			ppc_event_name[cfg & 0x7],
			__raw_readl(drvdata->sfrbase + REG_PPC_PMCNT(i)));
	}
	pr_crit("--------------------------------------------------\n");
}

int sysmmu_set_ppc_event(struct sysmmu_drvdata *drvdata, int event)
{
	unsigned int cfg;

	if (event < 0 || event > TOTAL_ID_NUM ||
	    event == READ_TLB_MISS || event == WRITE_TLB_MISS ||
	    event == READ_FLPD_MISS_PREFETCH ||
	    event == WRITE_FLPD_MISS_PREFETCH)
		return -EINVAL;

	if (!drvdata->event_cnt)
		__raw_writel(0x1, drvdata->sfrbase + REG_PPC_PMNC);

	__raw_writel(event, drvdata->sfrbase +
			REG_PPC_EVENT_SEL(drvdata->event_cnt));
	cfg = __raw_readl(drvdata->sfrbase +
			REG_PPC_CNTENS);
	__raw_writel(cfg | 0x1 << drvdata->event_cnt,
			drvdata->sfrbase + REG_PPC_CNTENS);
	return 0;
}
void exynos_sysmmu_show_ppc_event(struct device *dev)
{
	struct sysmmu_list_data *list;
	unsigned long flags;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			dev_info(drvdata->sysmmu,
				"%s: System MMU is not active\n", __func__);
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		if (sysmmu_block(drvdata->sfrbase))
			dump_sysmmu_ppc_cnt(drvdata);
		else
			pr_err("!!Failed to block Sytem MMU!\n");

		sysmmu_unblock(drvdata->sfrbase);
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

void exynos_sysmmu_clear_ppc_event(struct device *dev)
{
	struct sysmmu_list_data *list;
	unsigned long flags;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			dev_info(drvdata->sysmmu,
				"%s: System MMU is not active\n", __func__);
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		if (sysmmu_block(drvdata->sfrbase)) {
			dump_sysmmu_ppc_cnt(drvdata);
			__raw_writel(0x2, drvdata->sfrbase + REG_PPC_PMNC);
			__raw_writel(0, drvdata->sfrbase + REG_PPC_CNTENS);
			__raw_writel(0, drvdata->sfrbase + REG_PPC_INTENS);
			drvdata->event_cnt = 0;
		} else
			pr_err("!!Failed to block Sytem MMU!\n");

		sysmmu_unblock(drvdata->sfrbase);
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

int exynos_sysmmu_set_ppc_event(struct device *dev, int event)
{
	struct sysmmu_list_data *list;
	unsigned long flags;
	int ret = 0;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			dev_info(drvdata->sysmmu,
				"%s: System MMU is not active\n", __func__);
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		if (sysmmu_block(drvdata->sfrbase)) {
			if (drvdata->event_cnt < MAX_NUM_PPC) {
				ret = sysmmu_set_ppc_event(drvdata, event);
				if (ret)
					pr_err("Not supported Event ID (%d)",
						event);
				else
					drvdata->event_cnt++;
			}
		} else
			pr_err("!!Failed to block Sytem MMU!\n");

		sysmmu_unblock(drvdata->sfrbase);
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	return ret;
}

static sysmmu_pte_t *alloc_lv2entry_userptr(struct exynos_iommu_domain *domain,
					     unsigned long iova)
{
	return alloc_lv2entry(domain, section_entry(domain->pgtable, iova),
				iova, &domain->lv2entcnt[lv1ent_offset(iova)]);
}

static int sysmmu_map_pte(struct mm_struct *mm,
		pmd_t *pmd, unsigned long addr, unsigned long end,
		struct exynos_iommu_domain *domain, dma_addr_t iova, int prot)
{
	pte_t *pte;
	int ret = 0;
	spinlock_t *ptl;
	bool write = !!(prot & IOMMU_WRITE);
	bool pfnmap = !!(prot & IOMMU_PFNMAP);
	bool shareable = !!(prot & IOMMU_CACHE);
	unsigned int fault_flag = write ? FAULT_FLAG_WRITE : 0;
	sysmmu_pte_t *ent, *ent_beg;

	pte = pte_alloc_map_lock(mm, pmd, addr, &ptl);
	if (!pte)
		return -ENOMEM;

	ent = alloc_lv2entry_userptr(domain, iova);
	if (IS_ERR(ent)) {
		ret = PTR_ERR(ent);
		goto err;
	}

	ent_beg = ent;

	do {
		if (pte_none(*pte) || !pte_present(*pte) ||
					(write && !pte_write(*pte))) {
			int cnt = 0;
			int maxcnt = 1;

			if (pfnmap) {
				ret = -EFAULT;
				goto err;
			}

			while (cnt++ < maxcnt) {
				spin_unlock(ptl);
				/* find_vma() always successes */
				ret = handle_mm_fault(mm, find_vma(mm, addr),
						addr, fault_flag);
				spin_lock(ptl);
				if (ret & VM_FAULT_ERROR) {
					ret = mm_fault_translate(ret);
					goto err;
				} else {
					ret = 0;
				}
				/*
				 * the racing between handle_mm_fault() and the
				 * page reclamation may cause handle_mm_fault()
				 * to return 0 even though it failed to page in.
				 * This behavior expect the process to access
				 * the paged out entry again then give
				 * handle_mm_fault() a chance again to page in
				 * the entry.
				 */
				if (is_swap_pte(*pte)) {
					BUG_ON(maxcnt > 8);
					maxcnt++;
				}
			}
		}

		BUG_ON(!lv2ent_fault(ent));

		*ent = mk_lv2ent_spage(pte_pfn(*pte) << PAGE_SHIFT);

		if (!pfnmap)
			get_page(pte_page(*pte));
		else
			mk_lv2ent_pfnmap(ent);

		if (shareable)
			set_lv2ent_shareable(ent);

		ent++;
		iova += PAGE_SIZE;

		if ((iova & SECT_MASK) != ((iova - 1) & SECT_MASK)) {
			pgtable_flush(ent_beg, ent);

			ent = alloc_lv2entry_userptr(domain, iova);
			if (IS_ERR(ent)) {
				ret = PTR_ERR(ent);
				goto err;
			}
			ent_beg = ent;
		}
	} while (pte++, addr += PAGE_SIZE, addr != end);

	pgtable_flush(ent_beg, ent);
err:
	pte_unmap_unlock(pte - 1, ptl);
	return ret;
}

static inline int sysmmu_map_pmd(struct mm_struct *mm,
		pud_t *pud, unsigned long addr, unsigned long end,
		struct exynos_iommu_domain *domain, dma_addr_t iova, int prot)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_alloc(mm, pud, addr);
	if (!pmd)
		return -ENOMEM;

	do {
		next = pmd_addr_end(addr, end);
		if (sysmmu_map_pte(mm, pmd, addr, next, domain, iova, prot))
			return -ENOMEM;
		iova += (next - addr);
	} while (pmd++, addr = next, addr != end);
	return 0;
}
static inline int sysmmu_map_pud(struct mm_struct *mm,
		pgd_t *pgd, unsigned long addr, unsigned long end,
		struct exynos_iommu_domain *domain, dma_addr_t iova, int prot)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_alloc(mm, pgd, addr);
	if (!pud)
		return -ENOMEM;
	do {
		next = pud_addr_end(addr, end);
		if (sysmmu_map_pmd(mm, pud, addr, next, domain, iova, prot))
			return -ENOMEM;
		iova += (next - addr);
	} while (pud++, addr = next, addr != end);
	return 0;
}
int exynos_iommu_map_userptr(struct iommu_domain *dom, unsigned long addr,
			      dma_addr_t iova, size_t size, int prot)
{
	struct exynos_iommu_domain *domain = dom->priv;
	struct mm_struct *mm = current->mm;
	unsigned long end = addr + size;
	dma_addr_t start = iova;
	unsigned long next;
	pgd_t *pgd;
	int ret;

	BUG_ON(!!((iova | addr | size) & ~PAGE_MASK));

	pgd = pgd_offset(mm, addr);

	do {
		next = pgd_addr_end(addr, end);
		ret = sysmmu_map_pud(mm, pgd, addr, next, domain, iova, prot);
		if (ret)
			goto err;
		iova += (next - addr);
	} while (pgd++, addr = next, addr != end);

	return 0;
err:
	/* unroll */
	exynos_iommu_unmap_userptr(dom, start, size);
	return ret;
}

#define sect_offset(iova)	((iova) & ~SECT_MASK)
#define lv2ents_within(iova)	((SECT_SIZE - sect_offset(iova)) >> SPAGE_ORDER)

void exynos_iommu_unmap_userptr(struct iommu_domain *dom,
				dma_addr_t iova, size_t size)
{
	struct exynos_iommu_domain *domain = dom->priv;
	sysmmu_pte_t *sent = section_entry(domain->pgtable, iova);
	unsigned int entries = size >> SPAGE_ORDER;
	dma_addr_t start = iova;

	while (entries > 0) {
		unsigned int lv2ents, i;
		sysmmu_pte_t *pent;

		/* ignore fault entries */
		if (lv1ent_fault(sent)) {
			lv2ents = min_t(unsigned int, entries, NUM_LV1ENTRIES);
			entries -= lv2ents;
			iova += lv2ents << SPAGE_ORDER;
			sent++;
			continue;
		}

		BUG_ON(!lv1ent_page(sent));

		lv2ents = min_t(unsigned int, lv2ents_within(iova), entries);

		pent = page_entry(sent, iova);
		for (i = 0; i < lv2ents; i++, pent++) {
			/* ignore fault entries */
			if (lv2ent_fault(pent))
				continue;

			BUG_ON(!lv2ent_small(pent));

			if (!lv2ent_pfnmap(pent))
				put_page(phys_to_page(spage_phys(pent)));

			*pent = 0;
		}

		pgtable_flush(pent - lv2ents, pent);

		entries -= lv2ents;
		iova += lv2ents << SPAGE_ORDER;
		sent++;
	}

	exynos_sysmmu_tlb_invalidate(dom, start, size);
}

typedef void (*syncop)(const void *, size_t, int);

static size_t sysmmu_dma_sync_page(phys_addr_t phys, off_t off,
				  size_t pgsize, size_t size,
				  syncop op, enum dma_data_direction dir)
{
	size_t len;
	size_t skip_pages = off >> PAGE_SHIFT;
	struct page *page;

	off = off & ~PAGE_MASK;
	page = phys_to_page(phys) + skip_pages;
	len = min(pgsize - off, size);
	size = len;

	while (len > 0) {
		size_t sz;

		sz = min(PAGE_SIZE, len + off) - off;
		op(kmap(page) + off, sz, dir);
		kunmap(page++);
		len -= sz;
		off = 0;
	}

	return size;
}

static void exynos_iommu_sync(sysmmu_pte_t *pgtable, dma_addr_t iova,
			size_t len, syncop op, enum dma_data_direction dir)
{
	while (len > 0) {
		sysmmu_pte_t *entry;
		size_t done;

		entry = section_entry(pgtable, iova);
		switch (*entry & FLPD_FLAG_MASK) {
		case SPSECT_FLAG:
			done = sysmmu_dma_sync_page(spsection_phys(entry),
					spsection_offs(iova), SPSECT_SIZE,
					len, op, dir);

			break;
		case DSECT_FLAG:
			done = sysmmu_dma_sync_page(dsection_phys(entry),
					dsection_offs(iova), DSECT_SIZE,
					len, op, dir);
			break;
		case SECT_FLAG:
			done = sysmmu_dma_sync_page(section_phys(entry),
					section_offs(iova), SECT_SIZE,
					len, op, dir);
			break;
		case SLPD_FLAG:
			entry = page_entry(entry, iova);
			switch (*entry & SLPD_FLAG_MASK) {
			case LPAGE_FLAG:
				done = sysmmu_dma_sync_page(lpage_phys(entry),
						lpage_offs(iova), LPAGE_SIZE,
						len, op, dir);
				break;
			case SPAGE_FLAG:
				done = sysmmu_dma_sync_page(spage_phys(entry),
						spage_offs(iova), SPAGE_SIZE,
						len, op, dir);
				break;
			default: /* fault */
				return;
			}
			break;
		default: /* fault */
			return;
		}

		iova += done;
		len -= done;
	}
}

static sysmmu_pte_t *sysmmu_get_pgtable(struct device *dev)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct device *sysmmu = list_first_entry(&owner->mmu_list,
					struct sysmmu_list_data, node)->sysmmu;
	struct sysmmu_drvdata *data = dev_get_drvdata(sysmmu);
	struct iommu_domain *dom = data->domain;
	struct exynos_iommu_domain *domain = dom->priv;

	return domain->pgtable;
}

void exynos_iommu_sync_for_device(struct device *dev, dma_addr_t iova,
				  size_t len, enum dma_data_direction dir)
{
	exynos_iommu_sync(sysmmu_get_pgtable(dev),
			iova, len, __dma_map_area, dir);
}

void exynos_iommu_sync_for_cpu(struct device *dev, dma_addr_t iova, size_t len,
				enum dma_data_direction dir)
{
	if (dir == DMA_TO_DEVICE)
		return;

	exynos_iommu_sync(sysmmu_get_pgtable(dev),
			iova, len, __dma_unmap_area, dir);
}
