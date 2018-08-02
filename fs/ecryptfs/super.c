/**
 * eCryptfs: Linux filesystem encryption layer
 *
 * Copyright (C) 1997-2003 Erez Zadok
 * Copyright (C) 2001-2003 Stony Brook University
 * Copyright (C) 2004-2006 International Business Machines Corp.
 *   Author(s): Michael A. Halcrow <mahalcro@us.ibm.com>
 *              Michael C. Thompson <mcthomps@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/key.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/crypto.h>
#include <linux/statfs.h>
#include <linux/magic.h>
#include "ecryptfs_kernel.h"

#ifdef CONFIG_SDP
#include "ecryptfs_dek.h"
#endif

#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
#include "sdcardfs.h"
#endif

struct kmem_cache *ecryptfs_inode_info_cache;

/**
 * ecryptfs_alloc_inode - allocate an ecryptfs inode
 * @sb: Pointer to the ecryptfs super block
 *
 * Called to bring an inode into existence.
 *
 * Only handle allocation, setting up structures should be done in
 * ecryptfs_read_inode. This is because the kernel, between now and
 * then, will 0 out the private data pointer.
 *
 * Returns a pointer to a newly allocated inode, NULL otherwise
 */
static struct inode *ecryptfs_alloc_inode(struct super_block *sb)
{
	struct ecryptfs_inode_info *inode_info;
	struct inode *inode = NULL;

	inode_info = kmem_cache_alloc(ecryptfs_inode_info_cache, GFP_KERNEL);
	if (unlikely(!inode_info))
		goto out;
	ecryptfs_init_crypt_stat(&inode_info->crypt_stat);
	mutex_init(&inode_info->lower_file_mutex);
	atomic_set(&inode_info->lower_file_count, 0);
	inode_info->lower_file = NULL;
#ifdef CONFIG_SDP
	// get userid from super block
	inode_info->crypt_stat.engine_id = -1;
#endif
	inode = &inode_info->vfs_inode;
out:
	return inode;
}

static void ecryptfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	struct ecryptfs_inode_info *inode_info;
	inode_info = ecryptfs_inode_to_private(inode);

	kmem_cache_free(ecryptfs_inode_info_cache, inode_info);
}

/**
 * ecryptfs_destroy_inode
 * @inode: The ecryptfs inode
 *
 * This is used during the final destruction of the inode.  All
 * allocation of memory related to the inode, including allocated
 * memory in the crypt_stat struct, will be released here.
 * There should be no chance that this deallocation will be missed.
 */
static void ecryptfs_destroy_inode(struct inode *inode)
{
	struct ecryptfs_inode_info *inode_info;

	inode_info = ecryptfs_inode_to_private(inode);
	BUG_ON(inode_info->lower_file);
	ecryptfs_destroy_crypt_stat(&inode_info->crypt_stat);
	call_rcu(&inode->i_rcu, ecryptfs_i_callback);
}

/**
 * ecryptfs_statfs
 * @sb: The ecryptfs super block
 * @buf: The struct kstatfs to fill in with stats
 *
 * Get the filesystem statistics. Currently, we let this pass right through
 * to the lower filesystem and take no action ourselves.
 */
static int ecryptfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct dentry *lower_dentry = ecryptfs_dentry_to_lower(dentry);
	int rc;

	if (!lower_dentry->d_sb->s_op->statfs)
		return -ENOSYS;

	rc = lower_dentry->d_sb->s_op->statfs(lower_dentry, buf);
	if (rc)
		return rc;

	buf->f_type = ECRYPTFS_SUPER_MAGIC;
	rc = ecryptfs_set_f_namelen(&buf->f_namelen, buf->f_namelen,
	       &ecryptfs_superblock_to_private(dentry->d_sb)->mount_crypt_stat);

	return rc;
}

/**
 * ecryptfs_evict_inode
 * @inode - The ecryptfs inode
 *
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list. We use this to drop out reference to the
 * lower inode.
 */
static void ecryptfs_evict_inode(struct inode *inode)
{
	truncate_inode_pages_final(&inode->i_data);
	clear_inode(inode);
	iput(ecryptfs_inode_to_lower(inode));
}

/**
 * ecryptfs_show_options
 *
 * Prints the mount options for a given superblock.
 * Returns zero; does not fail.
 */
static int ecryptfs_show_options(struct seq_file *m, struct dentry *root)
{
	struct super_block *sb = root->d_sb;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat =
		&ecryptfs_superblock_to_private(sb)->mount_crypt_stat;
	struct ecryptfs_propagate_stat *propagate_stat =
		&ecryptfs_superblock_to_private(sb)->propagate_stat;
	struct ecryptfs_global_auth_tok *walker;

	mutex_lock(&mount_crypt_stat->global_auth_tok_list_mutex);
	list_for_each_entry(walker,
			    &mount_crypt_stat->global_auth_tok_list,
			    mount_crypt_stat_list) {
		if (walker->flags & ECRYPTFS_AUTH_TOK_FNEK)
			seq_printf(m, ",ecryptfs_fnek_sig=%s", walker->sig);
		else
			seq_printf(m, ",ecryptfs_sig=%s", walker->sig);
	}
	mutex_unlock(&mount_crypt_stat->global_auth_tok_list_mutex);

#ifdef CONFIG_SDP
	seq_printf(m, ",userid=%d", mount_crypt_stat->userid);

	if (mount_crypt_stat->flags & ECRYPTFS_MOUNT_SDP_ENABLED){
		seq_printf(m, ",sdp_enabled");
	}

	if (mount_crypt_stat->partition_id >= 0){
	    seq_printf(m, ",partition_id=%d", mount_crypt_stat->partition_id);
	}
#endif

#ifdef CONFIG_DLP
	if (mount_crypt_stat->flags & ECRYPTFS_MOUNT_DLP_ENABLED){
		seq_printf(m, ",dlp_enabled");
	}
#endif
	
#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
	if (mount_crypt_stat->cipher_code == RFC2440_CIPHER_AES_XTS_256)
		seq_printf(m, ",ecryptfs_cipher=%s",
			"aesxts");
	else
#endif
		seq_printf(m, ",ecryptfs_cipher=%s",
			mount_crypt_stat->global_default_cipher_name);

	if (mount_crypt_stat->global_default_cipher_key_size)
		seq_printf(m, ",ecryptfs_key_bytes=%zd",
			   mount_crypt_stat->global_default_cipher_key_size);
#ifdef CONFIG_WTL_ENCRYPTION_FILTER
	if (mount_crypt_stat->flags & ECRYPTFS_ENABLE_FILTERING)
		seq_printf(m, ",ecryptfs_enable_filtering");
#endif
#ifdef CONFIG_CRYPTO_FIPS
	if (mount_crypt_stat->flags & ECRYPTFS_ENABLE_CC)
		seq_printf(m, ",ecryptfs_enable_cc");
#endif
	if (mount_crypt_stat->flags & ECRYPTFS_PLAINTEXT_PASSTHROUGH_ENABLED)
		seq_printf(m, ",ecryptfs_passthrough");
	if (mount_crypt_stat->flags & ECRYPTFS_XATTR_METADATA_ENABLED)
		seq_printf(m, ",ecryptfs_xattr_metadata");
	if (mount_crypt_stat->flags & ECRYPTFS_ENCRYPTED_VIEW_ENABLED)
		seq_printf(m, ",ecryptfs_encrypted_view");
	if (mount_crypt_stat->flags & ECRYPTFS_UNLINK_SIGS)
		seq_printf(m, ",ecryptfs_unlink_sigs");
	if (mount_crypt_stat->flags & ECRYPTFS_GLOBAL_MOUNT_AUTH_TOK_ONLY)
		seq_printf(m, ",ecryptfs_mount_auth_tok_only");
#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
	if (mount_crypt_stat->flags & ECRYPTFS_USE_FMP)
		seq_printf(m, ",ecryptfs_use_fmp");
#endif

	seq_printf(m, ",base=%s", propagate_stat->base_path);
	if (propagate_stat->propagate_type == TYPE_E_DEFAULT)
		seq_printf(m, ",type=default");
	else if (propagate_stat->propagate_type == TYPE_E_READ)
		seq_printf(m, ",type=read");
	else if (propagate_stat->propagate_type == TYPE_E_WRITE)
		seq_printf(m, ",type=write");
	seq_printf(m, ",label=%s", propagate_stat->label);	

	return 0;
}

static long ecryptfs_propagate_lookup(struct super_block *sb, char *pathname){
	int ret = 0;
	char *propagate_path;
	struct ecryptfs_sb_info *sbi;
	struct ecryptfs_propagate_stat *stat;
	struct path sibling_path;
	const struct cred *saved_cred = NULL;

	sbi = ecryptfs_superblock_to_private(sb);
	stat = &sbi->propagate_stat;
	ECRYPTFS_OVERRIDE_ROOT_CRED(saved_cred);
	propagate_path = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!propagate_path) {
		ECRYPTFS_REVERT_CRED(saved_cred);
		return -ENOMEM;
	}
	if (stat->propagate_type != TYPE_E_NONE && stat->propagate_type != TYPE_E_DEFAULT) {
		snprintf(propagate_path, PATH_MAX, "%s/%s/%s/%s",
				stat->base_path, "default", stat->label, pathname);
		ret = (long)kern_path(propagate_path, LOOKUP_FOLLOW, &sibling_path);
		if (!ret) {
			path_put(&sibling_path);
		}
	}

	if (stat->propagate_type != TYPE_E_NONE && stat->propagate_type != TYPE_E_READ) {
		snprintf(propagate_path, PATH_MAX, "%s/%s/%s/%s",
				stat->base_path, "read", stat->label, pathname);
		ret = (long)kern_path(propagate_path, LOOKUP_FOLLOW, &sibling_path);
		if (!ret) {
			path_put(&sibling_path);
		}
	}

	if (stat->propagate_type != TYPE_E_NONE && stat->propagate_type != TYPE_E_WRITE) {
		snprintf(propagate_path, PATH_MAX, "%s/%s/%s/%s",
				stat->base_path, "write", stat->label, pathname);
		ret = (long)kern_path(propagate_path, LOOKUP_FOLLOW, &sibling_path);
		if (!ret) {
			path_put(&sibling_path);
		}
	}

	if (stat->propagate_type != TYPE_E_NONE) {
		snprintf(propagate_path, PATH_MAX, "/storage/%s/%s",
				stat->label, pathname);
		ret = (long)kern_path(propagate_path, LOOKUP_FOLLOW, &sibling_path);
		if (!ret) {
			path_put(&sibling_path);
		}
	}
	ECRYPTFS_REVERT_CRED(saved_cred);
	kfree(propagate_path);
	return ret;
}

const struct super_operations ecryptfs_sops = {
	.alloc_inode = ecryptfs_alloc_inode,
	.destroy_inode = ecryptfs_destroy_inode,
	.statfs = ecryptfs_statfs,
	.remount_fs = NULL,
	.evict_inode = ecryptfs_evict_inode,
	.show_options = ecryptfs_show_options,
	.drop_inode = generic_delete_inode,
};

const struct super_operations ecryptfs_multimount_sops = {
	.alloc_inode = ecryptfs_alloc_inode,
	.destroy_inode = ecryptfs_destroy_inode,
	.statfs = ecryptfs_statfs,
	.remount_fs = NULL,
	.evict_inode = ecryptfs_evict_inode,
	.show_options = ecryptfs_show_options,
	.drop_inode = generic_delete_inode,
	.unlink_callback = ecryptfs_propagate_lookup,
};
