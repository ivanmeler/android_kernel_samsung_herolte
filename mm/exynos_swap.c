/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos SWAP is a backend for frontswap.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>
#include <linux/frontswap.h>
#include <linux/mempool.h>
#include <linux/zpool.h>
#include <linux/compress.h>
#include <linux/console.h>

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#endif

static unsigned int eswap_max_pool_percent = 40;
module_param_named(max_pool_percent,
			eswap_max_pool_percent, uint, 0644);

static unsigned int decomp_verify;
module_param_named(verify_enable,
			decomp_verify, uint, 0644);

enum eswap_entryflags {
	ESWP_QUEUED,
	ESWP_DONE,
	ESWP_PGIN,
	ESWP_PGINV,
};

struct eswap_entry {
	struct rb_node	rbnode;
	pgoff_t		offset;
	unsigned long	handle;
	u16		length;
	u8		flag;
	spinlock_t	lock;
	u32		index;
};

struct eswap_tree {
	struct rb_root	rbroot;
	spinlock_t	lock;
};

struct eswap_data {
	struct compress_ops	*ops;
	struct zpool		*pool;
	struct eswap_tree	tree;
	void 			*priv;
	u32			comp_size;
	u32			comp_unit;
};

struct eswap_verify {
	phys_addr_t	addr;
	u16		size;
};

static struct eswap_data eswap;
static char *eswap_zpool_type = "zsmalloc";
static struct eswap_entry **comp_table;
static u64 eswap_pool_total_size;

static struct eswap_verify eswap_src;
static struct eswap_verify eswap_comp;

/* flag operations need entry->lock */
static inline void eswap_set_flag(struct eswap_entry *entry,
		enum eswap_entryflags flag)
{
	entry->flag |= BIT(flag);
};

static inline void eswap_clear_flag(struct eswap_entry *entry,
		enum eswap_entryflags flag)
{
	entry->flag &= ~BIT(flag);
};

static inline int eswap_test_flag(struct eswap_entry *entry,
		enum eswap_entryflags flag)
{
	return entry->flag & BIT(flag);
};

/*********************************
* statistics
**********************************/
/* Pool limit was hit (see eswap_max_pool_percent) */
static u64 eswap_pool_limit_hit;
/* Store failed because underlying allocator could not get memory */
static u64 eswap_reject_alloc_fail;
/* Store failed because the entry metadata could not be allocated (rare) */
static u64 eswap_reject_kmemcache_fail;
/* Compressed page was too big for the allocator to (optimally) store */
static u64 eswap_reject_compress_pool;
/* Pages written back when pool limit was reached */
static u64 eswap_written_back_pages;
/* Duplicate store was encountered (rare) */
static u64 eswap_duplicate_entry;
/* comp busy was hit */
static u64 eswap_comp_busy;
/* queued page was hit */
static u64 eswap_queued_page_swapin_hit;
/* compressed page was hit */
static u64 eswap_comp_page_swapin_hit;
/* queued page was invalidated */
static u64 eswap_queued_page_inv_hit;
/* The number of compressed pages currently stored in eswap */
static atomic_t eswap_stored_pages = ATOMIC_INIT(0);
/* TODO: add to perf monitor of zsmalloc time */

/*********************************
* eswap entry functions
**********************************/
static struct kmem_cache *eswap_entry_cache;

static bool __init eswap_entry_cache_create(void)
{
	eswap_entry_cache = KMEM_CACHE(eswap_entry, 0);
	return eswap_entry_cache == NULL;
}

static struct eswap_entry *eswap_entry_cache_alloc(gfp_t gfp)
{
	struct eswap_entry *entry;

	entry = kmem_cache_alloc(eswap_entry_cache, gfp);
	if (!entry)
		return NULL;
	entry->flag = 0;
	entry->handle = 0;
	RB_CLEAR_NODE(&entry->rbnode);
	spin_lock_init(&entry->lock);
	return entry;
}

static void eswap_entry_cache_free(struct eswap_entry *entry)
{
	kmem_cache_free(eswap_entry_cache, entry);
}

/*********************************
* rbtree functions
**********************************/
/*
 * In the case that a entry with the same offset is found, a pointer to
 * the existing entry is stored in dupentry and the function returns -EEXIST
 */
static int eswap_rb_insert(struct rb_root *root, struct eswap_entry *entry,
			struct eswap_entry **dupentry)
{
	struct rb_node **link = &root->rb_node, *parent = NULL;
	struct eswap_entry *myentry;

	while (*link) {
		parent = *link;
		myentry = rb_entry(parent, struct eswap_entry, rbnode);
		if (myentry->offset > entry->offset)
			link = &(*link)->rb_left;
		else if (myentry->offset < entry->offset)
			link = &(*link)->rb_right;
		else {
			*dupentry = myentry;
			return -EEXIST;
		}
	}
	rb_link_node(&entry->rbnode, parent, link);
	rb_insert_color(&entry->rbnode, root);
	return 0;
}

static void eswap_rb_erase(struct rb_root *root, struct eswap_entry *entry)
{
	rb_erase(&entry->rbnode, root);
	RB_CLEAR_NODE(&entry->rbnode);
}

static struct eswap_entry *eswap_rb_search_erase(struct rb_root *root,
						pgoff_t offset)
{
	struct rb_node *node = root->rb_node;
	struct eswap_entry *entry;

	while (node) {
		entry = rb_entry(node, struct eswap_entry, rbnode);
		if (entry->offset > offset)
			node = node->rb_left;
		else if (entry->offset < offset)
			node = node->rb_right;
		else {
			eswap_rb_erase(root, entry);
			return entry;
		}
	}
	return NULL;
}

/*
 * Carries out the common pattern of freeing and entry's zpool allocation,
 * freeing the entry itself, and decrementing the number of stored pages.
 */
static void eswap_free_entry(struct eswap_entry *entry)
{
	if (entry->handle)
		zpool_free(eswap.pool, entry->handle);

	eswap_entry_cache_free(entry);
	atomic_dec(&eswap_stored_pages);
	eswap_pool_total_size = zpool_get_total_size(eswap.pool);
}

/*********************************
* eswap debug function
**********************************/
#define ESWP_DATA_MASK	0x3
#define ESWP_ORG_DATA	0x1
#define ESWP_COMP_DATA	0x2
#define ESWP_TYPE_MASK	0xc
#define ESWP_BACKUP	0x4
#define ESWP_VERIFY	0x8
unsigned int eswap_verify_org_cnt;
unsigned int eswap_verify_comp_cnt;
/*
 * This saves original and compressed data to temporarily reserved memory.
 * And check the decompressed data is same with original data and also
 * compressed data.
 */
static int eswap_verify_data(unsigned int idx, u8 *data, unsigned int size,
				int type)
{
	u8 *addr;
	int ret = 0;
	/* TODO: currently idx means comp idx but
	 * verify needs other index.
	 */
	if (!eswap_src.addr || !eswap_comp.addr)
		return ret;

	switch (type & ESWP_DATA_MASK) {
	case ESWP_ORG_DATA:
		if (eswap_src.size < (eswap_verify_org_cnt * PAGE_SIZE))
			return ret;
		addr = (u8 *)eswap_src.addr;
		eswap_verify_org_cnt++;
		break;
	case ESWP_COMP_DATA:
		if (eswap_comp.size < (eswap_verify_comp_cnt * PAGE_SIZE))
			return ret;
		addr = (u8 *)eswap_comp.addr;
		eswap_verify_comp_cnt++;
		break;
	default:
		return ret;
	}

	switch (type & ESWP_TYPE_MASK) {
	case ESWP_BACKUP:
		memcpy(__va(addr + (idx << PAGE_SHIFT)), data, PAGE_SIZE);
		break;
	case ESWP_VERIFY:
		ret = memcmp(__va(addr + (idx << PAGE_SHIFT)), data, size);
		if (ret)
			pr_err("not match (%d) type data\n", type);
		break;
	default:
		return ret;
	};

	return ret;
}

static unsigned int eswap_store_compdata(int idx, u8 *addr, u16 size)
{
	unsigned long handle;
	struct eswap_entry *entry;
	u8 *cmem;
	int ret = 0;

	entry = comp_table[idx];
	if (!entry) {
		pr_err("No entry on comp_table[%d]\n", idx);
		goto out;
	}

	if (idx != entry->index) {
		pr_err("%s: wrong index %d %d\n", __func__, idx, entry->index);
		goto out;
	}

	spin_lock(&entry->lock);
	if (eswap_test_flag(entry, ESWP_PGIN) ||
		       eswap_test_flag(entry, ESWP_PGINV)) {
		spin_unlock(&entry->lock);
		eswap_free_entry(entry);
		goto out;
	}
	spin_unlock(&entry->lock);

	ret = zpool_malloc(eswap.pool, size, GFP_KERNEL, &handle);
	if (ret) {
		eswap_reject_alloc_fail++;
		pr_err("%s: Fail to alloc compress_pool (%d)\n", __func__, ret);
		goto out;
	}

	cmem = (u8 *)zpool_map_handle(eswap.pool, handle, ZPOOL_MM_RW);
	memcpy(cmem, addr, size);
	if (decomp_verify)
		eswap_verify_data(idx, cmem, size,
				ESWP_COMP_DATA | ESWP_BACKUP);
	zpool_unmap_handle(eswap.pool, handle);

	spin_lock(&entry->lock);
	if ((eswap_test_flag(entry, ESWP_PGIN) ||
				eswap_test_flag(entry, ESWP_PGINV))) {
		spin_unlock(&entry->lock);
		zpool_free(eswap.pool, handle);
		eswap_free_entry(entry);
		goto out;
	}
	entry->handle = handle;
	entry->length = size;
	eswap_set_flag(entry, ESWP_DONE);
	eswap_clear_flag(entry, ESWP_QUEUED);
	spin_unlock(&entry->lock);

	comp_table[idx] = NULL;

	pr_debug("--%s: index %d size %d handle %ld offset %ld\n", __func__,
			idx, size, entry->handle, entry->offset);
out:
	return ret;
}

static unsigned int eswap_update_compinfo(struct compress_info *info)
{
	int i, ret;
	u32 idx = info->first_idx;
	u8 *comp_addr = info->comp_addr;
	u16 *comp_size = info->comp_header;

	for (i = 0; i < eswap.comp_unit; i++) {
		do {
			ret = eswap_store_compdata((idx + i),
					comp_addr, comp_size[i]);
			WARN_ON(ret);
		} while (ret);
		comp_addr += PAGE_SIZE;
	}

	eswap_pool_total_size = zpool_get_total_size(eswap.pool);
	return 0;
}

static bool eswap_is_full(void)
{
	return totalram_pages * eswap_max_pool_percent / 100 <
		DIV_ROUND_UP(eswap_pool_total_size, PAGE_SIZE);
}

#define ZS_PREALLOC_THRESHOLD	256
static int eswap_frontswap_store(unsigned type, pgoff_t offset,
				struct page *page)
{
	struct eswap_entry *entry, *dupentry;
	unsigned long handle;
	u8 *src;
	int ret;

	if (eswap_is_full()) {
		eswap_pool_limit_hit++;
		if (zpool_shrink(eswap.pool, 1, NULL)) {
			ret = -ENOMEM;
			goto reject;
		}
	}

	/* allocate entry */
	entry = eswap_entry_cache_alloc(GFP_KERNEL);
	if (!entry) {
		eswap_reject_kmemcache_fail++;
		pr_err("%s: fail to alloc entry\n", __func__);
		ret = -ENOMEM;
		goto reject;
	}

	/* copy page */
	src = kmap_atomic(page);
	ret = eswap.ops->request_compress(eswap.priv, src, &entry->index);
	if (ret < 0) {
		kunmap_atomic(src);
		eswap_entry_cache_free(entry);
		eswap_comp_busy++;
		goto reject;
	}

	/* update entry info */
	comp_table[entry->index] = entry;
	entry->offset = offset;
	entry->length = PAGE_SIZE;
	eswap_set_flag(entry, ESWP_QUEUED);
	kunmap_atomic(src);

	/* zpool pre-allocate */
	if (!(entry->index % ZS_PREALLOC_THRESHOLD))
		zpool_malloc(eswap.pool, 0, 0, &handle);

	if (ret == COMP_READY)
		eswap.ops->start_compress(eswap.priv, entry->index);

	/* add entry to tree */
	spin_lock(&eswap.tree.lock);
	do {
		ret = eswap_rb_insert(&eswap.tree.rbroot, entry, &dupentry);
		if (ret == -EEXIST) {
			pr_err("%s: idx %d/%d length %d flag %d offset %ld\n",
			__func__, dupentry->index, entry->index,
			dupentry->length, dupentry->flag, dupentry->offset);
			eswap_rb_erase(&eswap.tree.rbroot, dupentry);
			eswap_free_entry(dupentry);
			eswap_duplicate_entry++;
		}
	} while (ret == -EEXIST);
	spin_unlock(&eswap.tree.lock);

	if (decomp_verify)
		eswap_verify_data(entry->index, src, PAGE_SIZE,
				ESWP_ORG_DATA | ESWP_BACKUP);

	atomic_inc(&eswap_stored_pages);
	pr_debug("%s: offset %ld index %d\n", __func__, entry->offset,
		       entry->index);
	return 0;

reject:
	return ret;
}

static int eswap_frontswap_load(unsigned type, pgoff_t offset,
				struct page *page)
{
	struct eswap_entry *entry;
	u8 *src, *dst;
	int ret;

	pr_debug("++ %s offset %ld\n", __func__, offset);

	/* find and remove from tree */
	spin_lock(&eswap.tree.lock);
	entry = eswap_rb_search_erase(&eswap.tree.rbroot, offset);
	if (!entry) {
		spin_unlock(&eswap.tree.lock);
		return -EINVAL;
	}
	spin_unlock(&eswap.tree.lock);

	spin_lock(&entry->lock);
	if (eswap_test_flag(entry, ESWP_PGIN) ||
				eswap_test_flag(entry, ESWP_PGINV)) {
		spin_unlock(&entry->lock);
		WARN(1, "%s: This(%ld) is already invalidated page. %d\n",
				__func__, entry->offset, entry->flag);
		return -EINVAL;
	}
	eswap_set_flag(entry, ESWP_PGIN);

	if (eswap_test_flag(entry, ESWP_QUEUED)) {
		dst = kmap_atomic(page);
		eswap.ops->cancel_compress(eswap.priv, dst, entry->index);
		kunmap_atomic(dst);
		spin_unlock(&entry->lock);
		eswap_queued_page_swapin_hit++;

		if (decomp_verify) {
			ret = eswap_verify_data(entry->index, dst,
				PAGE_SIZE, ESWP_ORG_DATA | ESWP_VERIFY);
			if (ret)
				pr_err("wrong data idx %d offset %ld)\n",
					entry->index, offset);
		}
		pr_debug("-- %s: offset %ld index %d\n",
			__func__, offset, entry->index);

	} else if (eswap_test_flag(entry, ESWP_DONE)) {
		spin_unlock(&entry->lock);
		src = (u8 *)zpool_map_handle(eswap.pool, entry->handle,
				ZPOOL_MM_RO);
		dst = kmap_atomic(page);
		eswap.ops->request_decompress(eswap.priv, src, entry->length,
				dst);
		kunmap_atomic(dst);
		zpool_unmap_handle(eswap.pool, entry->handle);

		if (decomp_verify) {
			ret = eswap_verify_data(entry->index, dst, PAGE_SIZE,
					ESWP_ORG_DATA | ESWP_VERIFY);
			if (ret) {
				pr_err("decompressed data is not mismatched");
				pr_err("(%ld, %d/%d), %d\n",
					entry->offset, entry->index,
					entry->index, entry->length);
				ret = eswap_verify_data(entry->index, src,
					entry->length,
					ESWP_COMP_DATA | ESWP_VERIFY);
				if (ret)
					pr_err("compressed data is wrong\n");
			}
		}
		pr_debug("-- %s: offset %ld index %d handle %ld addr %p\n",
			__func__, offset, entry->index, entry->handle, dst);

		eswap_free_entry(entry);
		eswap_comp_page_swapin_hit++;
	}

	return 0;
}

static void eswap_frontswap_invalidate_page(unsigned type, pgoff_t offset)
{
	struct eswap_entry *entry;

	pr_debug("++ %s\n", __func__);
	spin_lock(&eswap.tree.lock);
	entry = eswap_rb_search_erase(&eswap.tree.rbroot, offset);
	if (!entry) {
		spin_unlock(&eswap.tree.lock);
		return;
	}
	spin_unlock(&eswap.tree.lock);

	spin_lock(&entry->lock);
	eswap_set_flag(entry, ESWP_PGINV);
	if (eswap_test_flag(entry, ESWP_QUEUED)) {
		spin_unlock(&entry->lock);
		eswap_queued_page_inv_hit++;
	} else {
		spin_unlock(&entry->lock);
		eswap_free_entry(entry);
	}
	pr_debug("-- %s\n", __func__);
}

static void eswap_frontswap_invalidate_area(unsigned type)
{
	struct eswap_entry *entry, *n;

	spin_lock(&eswap.tree.lock);
	rbtree_postorder_for_each_entry_safe(entry,  n, &eswap.tree.rbroot,
		rbnode) {
		eswap_free_entry(entry);
	}

	eswap.tree.rbroot = RB_ROOT;
	spin_unlock(&eswap.tree.lock);
}

static void eswap_frontswap_init(unsigned type)
{
	eswap.tree.rbroot = RB_ROOT;
	spin_lock_init(&eswap.tree.lock);
}

#ifdef CONFIG_OF_RESERVED_MEM
#define ESWAP_SRC "eswap_src"
#define ESWAP_COMP "eswap_comp"
static int __init eswap_reserved_mem_setup(struct reserved_mem *rmem)
{
	if (!strncmp(rmem->name, ESWAP_SRC, strlen(ESWAP_SRC))) {
		eswap_src.addr = rmem->base;
		eswap_src.size = rmem->size;
	} else if (!strncmp(rmem->name, ESWAP_COMP, strlen(ESWAP_COMP))) {
		eswap_comp.addr = rmem->base;
		eswap_comp.size = rmem->size;
	}
	return 0;
}
RESERVEDMEM_OF_DECLARE(eswap_src, "eswap,src", eswap_reserved_mem_setup);
RESERVEDMEM_OF_DECLARE(eswap_comp, "eswap,comp", eswap_reserved_mem_setup);
#endif

/*********************************
* debugfs functions
**********************************/
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

static struct dentry *eswap_debugfs_root;

static int __init eswap_debugfs_init(void)
{
	if (!debugfs_initialized())
		return -ENODEV;

	eswap_debugfs_root = debugfs_create_dir("eswap", NULL);
	if (!eswap_debugfs_root)
		return -ENOMEM;

	debugfs_create_u64("pool_limit_hit", S_IRUGO,
			eswap_debugfs_root, &eswap_pool_limit_hit);
	debugfs_create_u64("reject_alloc_fail", S_IRUGO,
			eswap_debugfs_root, &eswap_reject_alloc_fail);
	debugfs_create_u64("reject_kmemcache_fail", S_IRUGO,
			eswap_debugfs_root, &eswap_reject_kmemcache_fail);
	debugfs_create_u64("reject_compress_poor", S_IRUGO,
			eswap_debugfs_root, &eswap_reject_compress_pool);
	debugfs_create_u64("written_back_pages", S_IRUGO,
			eswap_debugfs_root, &eswap_written_back_pages);
	debugfs_create_u64("duplicate_entry", S_IRUGO,
			eswap_debugfs_root, &eswap_duplicate_entry);
	debugfs_create_u64("pool_total_size", S_IRUGO,
			eswap_debugfs_root, &eswap_pool_total_size);
	debugfs_create_u64("comp_busy", S_IRUGO,
			eswap_debugfs_root, &eswap_comp_busy);
	debugfs_create_u64("queued_page_swapin", S_IRUGO,
			eswap_debugfs_root, &eswap_queued_page_swapin_hit);
	debugfs_create_u64("comp_page_swapin", S_IRUGO,
			eswap_debugfs_root, &eswap_comp_page_swapin_hit);
	debugfs_create_u64("queued_page_inv", S_IRUGO,
			eswap_debugfs_root, &eswap_queued_page_inv_hit);

	debugfs_create_atomic_t("stored_pages", S_IRUGO,
			eswap_debugfs_root, &eswap_stored_pages);

	return 0;
}

static void __exit eswap_debugfs_exit(void)
{
	debugfs_remove_recursive(eswap_debugfs_root);
}
#else
static int __init eswap_debugfs_init(void)
{
	return 0;
}

static void __exit eswap_debugfs_exit(void) { }
#endif

static struct frontswap_ops eswap_frontswap_ops = {
	.store = eswap_frontswap_store,
	.load = eswap_frontswap_load,
	.invalidate_page = eswap_frontswap_invalidate_page,
	.invalidate_area = eswap_frontswap_invalidate_area,
	.init = eswap_frontswap_init
};

void compress_register_ops(struct compress_ops *ops, void *priv)
{
	if (!ops->device_init || !ops->request_compress ||
			!ops->cancel_compress || !ops->request_decompress) {
		pr_err("fail to install callback\n");
		BUG();
		return;
	}

	eswap.ops = ops;
	eswap.priv = priv;

	if (eswap.ops->device_init(eswap.priv,
				&eswap.comp_size, &eswap.comp_unit,
				&eswap_update_compinfo)) {
		pr_err("failed to initialize comp device\n");
		return;
	}

	if (!eswap.comp_size) {
		pr_err("failed to get comp size\n");
		return;
	}

	comp_table = kzalloc((sizeof(struct eswap_entry *) *
				eswap.comp_size), GFP_KERNEL);
	if (!comp_table)
		return;

	frontswap_register_ops(&eswap_frontswap_ops);
	frontswap_tmem_exclusive_gets(true);
}

static int __init exynos_swap_init(void)
{
	pr_info("initialize exynos swap\n");

	eswap.pool = zpool_create_pool(eswap_zpool_type, NULL, GFP_KERNEL, NULL);
	if (!eswap.pool) {
		pr_info("fail to create %s\n", eswap_zpool_type);
		goto err_pool;
	}

	if (eswap_entry_cache_create()) {
		pr_err("entry cache creation failed\n");
		goto err_cache;
	}

	if (eswap_debugfs_init())
		pr_warn("debugfs initialization failed\n");

	pr_info("Success to create exynos swap\n");
	return 0;

err_cache:
	zpool_destroy_pool(eswap.pool);
err_pool:
	pr_err("failed to initialize exynos swap\n");
	return -ENOMEM;
}

late_initcall(exynos_swap_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sunyoung Kang <sy0816.kang@samsung.com>");
MODULE_DESCRIPTION("Exynos SWAP for frontswap backend");
