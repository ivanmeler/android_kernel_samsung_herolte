extern void __init bootmem_init(void);
extern void __init arm64_swiotlb_init(void);

/* For exynos compatible */

#include <linux/list.h>
#include <linux/vmalloc.h>

/*
 * ARM specific vm_struct->flags bits.
 */

/* (super)section-mapped I/O regions used by ioremap()/iounmap() */
#define VM_ARM_SECTION_MAPPING	0x80000000

/* permanent static mappings from iotable_init() */
#define VM_ARM_STATIC_MAPPING	0x40000000

/* empty mapping */
#define VM_ARM_EMPTY_MAPPING	0x20000000

/* mapping type (attributes) for permanent static mappings */
#define VM_ARM_MTYPE(mt)		((mt) << 20)
#define VM_ARM_MTYPE_MASK	(0x1f << 20)

/* consistent regions used by dma_alloc_attrs() */
#define VM_ARM_DMA_CONSISTENT	0x20000000


struct static_vm {
	struct vm_struct vm;
	struct list_head list;
};
extern struct list_head static_vmlist;

void fixup_init(void);
