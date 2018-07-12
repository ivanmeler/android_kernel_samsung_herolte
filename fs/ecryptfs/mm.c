/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Sensitive Data Protection
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/swap.h>
#include <linux/pagemap.h>

#include "ecryptfs_kernel.h"
#include "ecryptfs_dek.h"


extern spinlock_t inode_sb_list_lock;
static int ecryptfs_mm_debug = 0;
DEFINE_MUTEX(ecryptfs_mm_mutex);

struct ecryptfs_mm_drop_cache_param {
	int user_id;
    int engine_id;
};

#define INVALIDATE_MAPPING_RETRY_CNT 3

static unsigned long invalidate_mapping_pages_retry(struct address_space *mapping,
		pgoff_t start, pgoff_t end, int retries) {
	unsigned long ret;

	if(ecryptfs_mm_debug)
		printk("freeing [%s] sensitive inode[mapped pagenum = %lu]\n",
				mapping->host->i_sb->s_type->name,
				mapping->nrpages);
retry:
	ret = invalidate_mapping_pages(mapping, start, end);
	if(ecryptfs_mm_debug)
		printk("invalidate_mapping_pages ret = %lu, [%lu] remained\n",
				ret, mapping->nrpages);

	if(mapping->nrpages != 0) {
		if(retries > 0) {
			printk("[%lu] mapped pages remained in sensitive inode, retry..\n",
					mapping->nrpages);
			retries--;
			msleep(100);
			goto retry;
		}
	}

	return ret;
}

#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
static unsigned long invalidate_lower_mapping_pages_retry(struct file *lower_file, int retries) {

	unsigned long ret, invalidated = 0;
	struct address_space *mapping;

	mapping = lower_file->f_mapping;

	if(ecryptfs_mm_debug)
		printk("%s:freeing [%s] sensitive inode[mapped pagenum = %lu]\n",__func__,
				mapping->host->i_sb->s_type->name,mapping->nrpages);

	for (; retries > 0; retries--) {

		// !! TODO !!
		//if (lower_file && lower_file->f_op && lower_file->f_op->unlocked_ioctl)
		//	ret = lower_file->f_op->unlocked_ioctl(lower_file, FS_IOC_INVAL_MAPPING, 0);
		ret = do_vfs_ioctl(lower_file,0, FS_IOC_INVAL_MAPPING, 0); // lower_file is sdcardfs file
		invalidated += ret;

		if(ecryptfs_mm_debug)
			printk("invalidate_mapping_pages ret = %lu, [%lu] remained\n",
					ret, mapping->nrpages);

		if (mapping->nrpages == 0)
			break;

		printk("[%lu] mapped pages remained in sensitive inode, retry..\n",
				mapping->nrpages);
		msleep(100);
	} 
	return invalidated;
}
#endif

void ecryptfs_mm_do_sdp_cleanup(struct inode *inode) {
	struct ecryptfs_crypt_stat *crypt_stat;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat = NULL;
	struct ecryptfs_inode_info *inode_info;

	crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;
	mount_crypt_stat = &ecryptfs_superblock_to_private(inode->i_sb)->mount_crypt_stat;
	inode_info = ecryptfs_inode_to_private(inode);

	if(crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
		int rc;
		if(S_ISDIR(inode->i_mode)) {
			DEK_LOGD("%s: inode: %p is dir, return\n",__func__, inode);
			return;
		}

		DEK_LOGD("%s: inode: %p  clean up start\n",__func__, inode);
		rc = vfs_fsync(inode_info->lower_file, 0);
		if(rc)
			DEK_LOGE("%s: vfs_sync returned error rc: %d\n", __func__, rc);

		if(ecryptfs_is_sdp_locked(crypt_stat->engine_id)) {
			DEK_LOGD("%s: persona locked inode: %lu useid: %d\n",
			        __func__, inode->i_ino, crypt_stat->engine_id);
			invalidate_mapping_pages_retry(inode->i_mapping, 0, -1, 3);
		}
#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
		if (mount_crypt_stat->flags & ECRYPTFS_USE_FMP) {
			DEK_LOGD("%s inode: %p calling invalidate_lower_mapping_pages_retry\n",__func__, inode);
			invalidate_lower_mapping_pages_retry(inode_info->lower_file, 3);
		}
#endif
		if(ecryptfs_is_sdp_locked(crypt_stat->engine_id)) {
			ecryptfs_clean_sdp_dek(crypt_stat);
		}
		DEK_LOGD("%s: inode: %p clean up stop\n",__func__, inode);
	}
	return;
}

static unsigned long drop_inode_pagecache(struct inode *inode) {
	int rc = 0;

	spin_lock(&inode->i_lock);
	
	if(ecryptfs_mm_debug)
		printk("%s() cleaning [%s] pages: %lu\n", __func__,
				inode->i_sb->s_type->name,inode->i_mapping->nrpages);

	if ((inode->i_mapping->nrpages == 0)) {
		spin_unlock(&inode->i_lock);
		printk("%s inode having zero nrpages\n", __func__);
		return 0;
	}

	spin_unlock(&inode->i_lock);

	/*
	 * flush mapped dirty pages.
	 */
	rc = filemap_write_and_wait(inode->i_mapping);
	if(rc)
		printk("filemap_flush failed, rc=%d\n", rc);

	if (inode->i_mapping->nrpages != 0)
		lru_add_drain_all();

	rc = invalidate_mapping_pages_retry(inode->i_mapping, 0, -1,
			INVALIDATE_MAPPING_RETRY_CNT);

	if(inode->i_mapping->nrpages)
			printk("%s() uncleaned [%s] pages: %lu\n", __func__,
					inode->i_sb->s_type->name,inode->i_mapping->nrpages);

	return rc;
}


static void ecryptfs_mm_drop_pagecache(struct super_block *sb, void *arg)
{
	struct inode *inode;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat;
	struct ecryptfs_mm_drop_cache_param *param = arg;
	
	if(strcmp("ecryptfs", sb->s_type->name)) {
		printk("%s sb:%s is not ecryptfs superblock\n", __func__,
				sb->s_type->name);
		return;
	}
	
	mount_crypt_stat = &ecryptfs_superblock_to_private(sb)->mount_crypt_stat;
	
	printk("%s start() sb:%s [%d], userid:%d\n", __func__,
			sb->s_type->name, mount_crypt_stat->userid, param->user_id);
	
	if (param->user_id >= 100 && param->user_id < 200) {
		if(mount_crypt_stat->userid != param->user_id)
			return;
	}
	
	spin_lock(&inode_sb_list_lock);
	list_for_each_entry(inode, &sb->s_inodes, i_sb_list)
	{	
        struct ecryptfs_crypt_stat *crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;

        if(crypt_stat == NULL)
            continue;

        if(crypt_stat->engine_id != param->engine_id) {
			continue;
		}

		if (!inode->i_mapping) {
			continue;
		}
		
		spin_lock(&inode->i_lock);
		if (inode->i_mapping->nrpages == 0) {
			spin_unlock(&inode->i_lock);
			spin_unlock(&inode_sb_list_lock);
			
			if(ecryptfs_mm_debug)
				printk("%s() ecryptfs inode [ino:%lu]\n",__func__, inode->i_ino);
				
			if((crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) &&
					!atomic_read(&ecryptfs_inode_to_private(inode)->lower_file_count))
				ecryptfs_clean_sdp_dek(crypt_stat);

			spin_lock(&inode_sb_list_lock);
			continue;
		}
		spin_unlock(&inode->i_lock);

		spin_unlock(&inode_sb_list_lock);

		if(ecryptfs_mm_debug)
			printk(KERN_ERR "inode number: %lu i_mapping: %p [%s] userid:%d\n",inode->i_ino,
					inode->i_mapping,inode->i_sb->s_type->name,inode->i_mapping->userid);

		if(mapping_sensitive(inode->i_mapping) &&
				!atomic_read(&ecryptfs_inode_to_private(inode)->lower_file_count)) {
			drop_inode_pagecache(inode);
				
			if(ecryptfs_mm_debug)
					printk(KERN_ERR "lower inode: %p lower inode: %p nrpages: %lu\n",ecryptfs_inode_to_lower(inode),
							ecryptfs_inode_to_private(inode), ecryptfs_inode_to_lower(inode)->i_mapping->nrpages);
			
			if(crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE)
				ecryptfs_clean_sdp_dek(crypt_stat);	
		}
		spin_lock(&inode_sb_list_lock);
	}
	spin_unlock(&inode_sb_list_lock);
}

static int ecryptfs_mm_task(void *arg)
{
	struct file_system_type *type;
	struct ecryptfs_mm_drop_cache_param *param = arg;
	
	type = get_fs_type("ecryptfs");
	
	if(type) {
		if(ecryptfs_mm_debug)
			printk("%s type name: %s flags: %d\n", __func__, type->name, type->fs_flags);
		
		mutex_lock(&ecryptfs_mm_mutex);
		iterate_supers_type(type,ecryptfs_mm_drop_pagecache, param);
		mutex_unlock(&ecryptfs_mm_mutex);
		
		put_filesystem(type);
	}
	
	kfree(param);
	return 0;
}

void ecryptfs_mm_drop_cache(int userid, int engineid) {
#if 1
	struct task_struct *task;
	struct ecryptfs_mm_drop_cache_param *param =
			kzalloc(sizeof(*param), GFP_KERNEL);

	if (!param) {
		printk("%s :: skip. no memory to alloc param\n", __func__);
		return;
	}
    param->user_id = userid;
    param->engine_id = engineid;

	printk("running cache cleanup thread - sdp-id : %d\n", userid);
	task = kthread_run(ecryptfs_mm_task, param, "sdp_cached");

	if (IS_ERR(task)) {
		printk(KERN_ERR "SDP : unable to create kernel thread: %ld\n",
				PTR_ERR(task));
	}
#else
	ecryptfs_mm_task(&userid);
#endif
}

#include <linux/pagevec.h>
#include <linux/pagemap.h>
#include <linux/memcontrol.h>
#include <linux/atomic.h>

static void __page_dump(unsigned char *buf, int len, const char* str)
{
	unsigned int     i;
	char	s[512];

	s[0] = 0;
	for(i=0;i<len && i<16;++i) {
		char tmp[8];
		sprintf(tmp, " %02x", buf[i]);
		strcat(s, tmp);
	}

	if (len > 16) {
		char tmp[8];
		sprintf(tmp, " ...");
		strcat(s, tmp);
	}

	DEK_LOGD("%s [%s len=%d]\n", s, str, len);
}

#ifdef DEK_DEBUG
/*
 * This dump will appear in ramdump
 */
void page_dump (struct page *p) {
	void *d;
	d = kmap_atomic(p);
	if(d) {
		__page_dump((unsigned char *)d, PAGE_SIZE, "freeing");
		kunmap_atomic(d);
	}
}
#else
void page_dump (struct page *p) {
	// Do nothing
}
#endif
