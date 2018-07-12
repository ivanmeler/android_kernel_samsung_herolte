/**
 * eCryptfs: Linux filesystem encryption layer
 *
 * Copyright (C) 1997-2004 Erez Zadok
 * Copyright (C) 2001-2004 Stony Brook University
 * Copyright (C) 2004-2007 International Business Machines Corp.
 *   Author(s): Michael A. Halcrow <mhalcrow@us.ibm.com>
 *   		Michael C. Thompson <mcthomps@us.ibm.com>
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

#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/security.h>
#include <linux/compat.h>
#include <linux/fs_stack.h>
#include <linux/aio.h>
#include "ecryptfs_kernel.h"

#ifdef CONFIG_WTL_ENCRYPTION_FILTER
#include <linux/ctype.h>
#define ECRYPTFS_IOCTL_GET_ATTRIBUTES	_IOR('l', 0x10, __u32)
#define ECRYPTFS_WAS_ENCRYPTED 0x0080
#define ECRYPTFS_WAS_ENCRYPTED_OTHER_DEVICE 0x0100
#endif
#ifdef CONFIG_SDP
#if 0
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/atomic.h>
#endif
#include "ecryptfs_dek.h"
#include "mm.h"
#endif

#ifdef CONFIG_DLP
#include "ecryptfs_dlp.h"
#include <sdp/sdp_dlp.h>
#include <sdp/fs_request.h>
#endif

/**
 * ecryptfs_read_update_atime
 *
 * generic_file_read updates the atime of upper layer inode.  But, it
 * doesn't give us a chance to update the atime of the lower layer
 * inode.  This function is a wrapper to generic_file_read.  It
 * updates the atime of the lower level inode if generic_file_read
 * returns without any errors. This is to be used only for file reads.
 * The function to be used for directory reads is ecryptfs_read.
 */
static ssize_t ecryptfs_read_update_atime(struct kiocb *iocb,
				struct iov_iter *to)
{
	ssize_t rc;
	struct path *path;
	struct file *file = iocb->ki_filp;

	rc = generic_file_read_iter(iocb, to);
	/*
	 * Even though this is a async interface, we need to wait
	 * for IO to finish to update atime
	 */
	if (-EIOCBQUEUED == rc)
		rc = wait_on_sync_kiocb(iocb);
	if (rc >= 0) {
		path = ecryptfs_dentry_to_lower_path(file->f_path.dentry);
		touch_atime(path);
	}
	return rc;
}

struct ecryptfs_getdents_callback {
	struct dir_context ctx;
	struct dir_context *caller;
	struct super_block *sb;
	int filldir_called;
	int entries_written;
};

/* Inspired by generic filldir in fs/readdir.c */
static int
ecryptfs_filldir(void *dirent, const char *lower_name, int lower_namelen,
		 loff_t offset, u64 ino, unsigned int d_type)
{
	struct ecryptfs_getdents_callback *buf =
	    (struct ecryptfs_getdents_callback *)dirent;
	size_t name_size;
	char *name;
	int rc;

	buf->filldir_called++;
	rc = ecryptfs_decode_and_decrypt_filename(&name, &name_size,
						  buf->sb, lower_name,
						  lower_namelen);
	if (rc) {
		printk(KERN_ERR "%s: Error attempting to decode and decrypt "
		       "filename [%s]; rc = [%d]\n", __func__, lower_name,
		       rc);
		goto out;
	}
	buf->caller->pos = buf->ctx.pos;
	rc = !dir_emit(buf->caller, name, name_size, ino, d_type);
	kfree(name);
	if (!rc)
		buf->entries_written++;
out:
	return rc;
}

/**
 * ecryptfs_readdir
 * @file: The eCryptfs directory file
 * @ctx: The actor to feed the entries to
 */
static int ecryptfs_readdir(struct file *file, struct dir_context *ctx)
{
	int rc;
	struct file *lower_file;
	struct inode *inode = file_inode(file);
	struct ecryptfs_getdents_callback buf = {
		.ctx.actor = ecryptfs_filldir,
		.caller = ctx,
		.sb = inode->i_sb,
	};
	lower_file = ecryptfs_file_to_lower(file);
	lower_file->f_pos = ctx->pos;
	rc = iterate_dir(lower_file, &buf.ctx);
	ctx->pos = buf.ctx.pos;
	if (rc < 0)
		goto out;
	if (buf.filldir_called && !buf.entries_written)
		goto out;
	if (rc >= 0)
		fsstack_copy_attr_atime(inode,
					file_inode(lower_file));
out:
	return rc;
}

struct kmem_cache *ecryptfs_file_info_cache;

static int read_or_initialize_metadata(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat;
	struct ecryptfs_crypt_stat *crypt_stat;
	int rc;

	crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;
	mount_crypt_stat = &ecryptfs_superblock_to_private(
						inode->i_sb)->mount_crypt_stat;

#ifdef CONFIG_WTL_ENCRYPTION_FILTER
	if (crypt_stat->flags & ECRYPTFS_STRUCT_INITIALIZED
		&& crypt_stat->flags & ECRYPTFS_POLICY_APPLIED
		&& crypt_stat->flags & ECRYPTFS_ENCRYPTED
		&& !(crypt_stat->flags & ECRYPTFS_KEY_VALID)
		&& !(crypt_stat->flags & ECRYPTFS_KEY_SET)
		&& crypt_stat->flags & ECRYPTFS_I_SIZE_INITIALIZED) {
		crypt_stat->flags |= ECRYPTFS_ENCRYPTED_OTHER_DEVICE;
	}
	mutex_lock(&crypt_stat->cs_mutex);
	if ((mount_crypt_stat->flags & ECRYPTFS_ENABLE_NEW_PASSTHROUGH)
			&& (crypt_stat->flags & ECRYPTFS_ENCRYPTED)) {
		if (ecryptfs_read_metadata(dentry)) {
			crypt_stat->flags &= ~(ECRYPTFS_I_SIZE_INITIALIZED
					| ECRYPTFS_ENCRYPTED);
			rc = 0;
			goto out;
		}
	} else if ((mount_crypt_stat->flags & ECRYPTFS_ENABLE_FILTERING)
			&& (crypt_stat->flags & ECRYPTFS_ENCRYPTED)) {
		struct dentry *fp_dentry =
			ecryptfs_inode_to_private(inode)->lower_file->f_dentry;
		char filename[NAME_MAX+1] = {0};
		if (fp_dentry->d_name.len <= NAME_MAX)
			memcpy(filename, fp_dentry->d_name.name,
					fp_dentry->d_name.len + 1);

		if (is_file_name_match(mount_crypt_stat, fp_dentry)
			|| is_file_ext_match(mount_crypt_stat, filename)) {
			if (ecryptfs_read_metadata(dentry))
				crypt_stat->flags &=
				~(ECRYPTFS_I_SIZE_INITIALIZED
				| ECRYPTFS_ENCRYPTED);
			rc = 0;
			goto out;
		}
	}
	mutex_unlock(&crypt_stat->cs_mutex);
#endif

	mutex_lock(&crypt_stat->cs_mutex);

	if (crypt_stat->flags & ECRYPTFS_POLICY_APPLIED &&
	    crypt_stat->flags & ECRYPTFS_KEY_VALID) {
		rc = 0;
		goto out;
	}

	rc = ecryptfs_read_metadata(dentry);
	if (!rc)
		goto out;

#ifdef CONFIG_SDP
	/*
	 * no passthrough/xattr for sensitive files
	 */
	if ((rc) && crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE)
		goto out;
#endif

	if (mount_crypt_stat->flags & ECRYPTFS_PLAINTEXT_PASSTHROUGH_ENABLED) {
		crypt_stat->flags &= ~(ECRYPTFS_I_SIZE_INITIALIZED
				       | ECRYPTFS_ENCRYPTED);
		rc = 0;
		goto out;
	}

	if (!(mount_crypt_stat->flags & ECRYPTFS_XATTR_METADATA_ENABLED) &&
	    !i_size_read(ecryptfs_inode_to_lower(inode))) {
		rc = ecryptfs_initialize_file(dentry, inode);
		if (!rc)
			goto out;
	}

	rc = -EIO;
out:
	mutex_unlock(&crypt_stat->cs_mutex);
#ifdef CONFIG_SDP
	if(!rc)
	{
		/*
		 * SDP v2.0 : sensitive directory (SDP vault)
		 * Files under sensitive directory automatically becomes sensitive
		 */
		struct dentry *p = dentry->d_parent;
		struct inode *parent_inode = p->d_inode;
		struct ecryptfs_crypt_stat *parent_crypt_stat =
				&ecryptfs_inode_to_private(parent_inode)->crypt_stat;

		if (!(crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) &&
				((S_ISDIR(parent_inode->i_mode)) &&
						(parent_crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE))) {
			rc = ecryptfs_sdp_set_sensitive(parent_crypt_stat->engine_id, dentry);
		}
	}
#endif
	return rc;
}

#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
static void ecryptfs_set_rapages(struct file *file, unsigned int flag)
{
	if (!flag)
		file->f_ra.ra_pages = 0;
	else
		file->f_ra.ra_pages = (unsigned int)file->f_mapping->backing_dev_info->ra_pages;
}

static int ecryptfs_set_fmpinfo(struct file *file, struct inode *inode, unsigned int set_flag)
{
	struct address_space *mapping = file->f_mapping;

	if (set_flag) {
		struct ecryptfs_crypt_stat *crypt_stat =
			&ecryptfs_inode_to_private(inode)->crypt_stat;
		struct ecryptfs_mount_crypt_stat *mount_crypt_stat =
			&ecryptfs_superblock_to_private(inode->i_sb)->mount_crypt_stat;

		if (strncmp(crypt_stat->cipher, "aesxts", sizeof("aesxts"))
			&& strncmp(crypt_stat->cipher, "aes", sizeof("aes"))) {
			if (!(crypt_stat->flags & ECRYPTFS_ENCRYPTED)) {
				mapping->plain_text = 1;
				return 0;
			} else {
				ecryptfs_printk(KERN_ERR,
						"%s: Error invalid file encryption algorithm, inode %lu, filename %s alg %s\n"
						, __func__, inode->i_ino,  file->f_dentry->d_name.name, crypt_stat->cipher);
				return -EINVAL;
			}
		}
		mapping->iv = crypt_stat->root_iv;
		mapping->key = crypt_stat->key;
		mapping->sensitive_data_index = crypt_stat->metadata_size/4096;
		if (mount_crypt_stat->cipher_code == RFC2440_CIPHER_AES_XTS_256) {
			mapping->key_length = crypt_stat->key_size * 2;
			mapping->alg = "aesxts";
		} else {
			mapping->key_length = crypt_stat->key_size;
			mapping->alg = crypt_stat->cipher;
		}
		mapping->hash_tfm = crypt_stat->hash_tfm;
#ifdef CONFIG_CRYPTO_FIPS
		mapping->cc_enable =
			(mount_crypt_stat->flags & ECRYPTFS_ENABLE_CC)?1:0;
#endif
	} else {
		mapping->iv = NULL;
		mapping->key = NULL;
		mapping->key_length = 0;
		mapping->sensitive_data_index = 0;
		mapping->alg = NULL;
		mapping->hash_tfm = NULL;
#ifdef CONFIG_CRYPTO_FIPS
		mapping->cc_enable = 0;
#endif
		mapping->plain_text = 0;
	}

	return 0;
}

void ecryptfs_propagate_rapages(struct file *file, unsigned int flag)
{
	struct file *f = file;

	do {
		if (!f)
			return;
		ecryptfs_set_rapages(f, flag);
	} while(f->f_op->get_lower_file && (f = f->f_op->get_lower_file(f)));

}

int ecryptfs_propagate_fmpinfo(struct inode *inode, unsigned int flag)
{
	struct file *f = ecryptfs_inode_to_private(inode)->lower_file;

	do {
		if (!f)
			return 0;
		if (ecryptfs_set_fmpinfo(f, inode, flag))
			return -EINVAL;
	} while(f->f_op->get_lower_file && (f = f->f_op->get_lower_file(f)));

	return 0;
}
#endif

static int ecryptfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct file *lower_file = ecryptfs_file_to_lower(file);
	/*
	 * Don't allow mmap on top of file systems that don't support it
	 * natively.  If FILESYSTEM_MAX_STACK_DEPTH > 2 or ecryptfs
	 * allows recursive mounting, this will need to be extended.
	 */
	if (!lower_file->f_op->mmap)
		return -ENODEV;
	return generic_file_mmap(file, vma);
}

/**
 * ecryptfs_open
 * @inode: inode speciying file to open
 * @file: Structure to return filled in
 *
 * Opens the file specified by inode.
 *
 * Returns zero on success; non-zero otherwise
 */
static int ecryptfs_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct ecryptfs_crypt_stat *crypt_stat = NULL;
	struct dentry *ecryptfs_dentry = file->f_path.dentry;
	/* Private value of ecryptfs_dentry allocated in
	 * ecryptfs_lookup() */
	struct ecryptfs_file_info *file_info;
#ifdef CONFIG_DLP
	sdp_fs_command_t *cmd = NULL;

	ssize_t dlp_len = 0;
	struct knox_dlp_data dlp_data;
	struct timespec ts;
#endif

#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS) || defined(CONFIG_SDP)
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat;	
	mount_crypt_stat = &ecryptfs_superblock_to_private(
							inode->i_sb)->mount_crypt_stat;
#endif

	/* Released in ecryptfs_release or end of function if failure */
	file_info = kmem_cache_zalloc(ecryptfs_file_info_cache, GFP_KERNEL);
	ecryptfs_set_file_private(file, file_info);
	if (!file_info) {
		ecryptfs_printk(KERN_ERR,
				"Error attempting to allocate memory\n");
		rc = -ENOMEM;
		goto out;
	}
	crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;
	mutex_lock(&crypt_stat->cs_mutex);
	if (!(crypt_stat->flags & ECRYPTFS_POLICY_APPLIED)) {
		ecryptfs_printk(KERN_DEBUG, "Setting flags for stat...\n");
		/* Policy code enabled in future release */
		crypt_stat->flags |= (ECRYPTFS_POLICY_APPLIED
				      | ECRYPTFS_ENCRYPTED);
	}
	mutex_unlock(&crypt_stat->cs_mutex);
	rc = ecryptfs_get_lower_file(ecryptfs_dentry, inode);
	if (rc) {
		printk(KERN_ERR "%s: Error attempting to initialize "
			"the lower file for the dentry with name "
			"[%pd]; rc = [%d]\n", __func__,
			ecryptfs_dentry, rc);
		goto out_free;
	}
	if ((ecryptfs_inode_to_private(inode)->lower_file->f_flags & O_ACCMODE)
	    == O_RDONLY && (file->f_flags & O_ACCMODE) != O_RDONLY) {
		rc = -EPERM;
		printk(KERN_WARNING "%s: Lower file is RO; eCryptfs "
		       "file must hence be opened RO\n", __func__);
		goto out_put;
	}
	ecryptfs_set_file_lower(
		file, ecryptfs_inode_to_private(inode)->lower_file);
	if (S_ISDIR(ecryptfs_dentry->d_inode->i_mode)) {
#ifdef CONFIG_SDP
		/*
		 * it's possible to have a sensitive directory. (vault)
		 */
		if (mount_crypt_stat->flags & ECRYPTFS_MOUNT_SDP_ENABLED)
			crypt_stat->flags |= ECRYPTFS_DEK_SDP_ENABLED;
#endif
		ecryptfs_printk(KERN_DEBUG, "This is a directory\n");
		mutex_lock(&crypt_stat->cs_mutex);
		crypt_stat->flags &= ~(ECRYPTFS_ENCRYPTED);
		mutex_unlock(&crypt_stat->cs_mutex);
		rc = 0;
		goto out;
	}
	rc = read_or_initialize_metadata(ecryptfs_dentry);
	if (rc) {
#ifdef CONFIG_SDP
		if(file->f_flags & O_SDP){
			printk("Failed to initialize metadata, "
					"but let it continue cause current call is from SDP API\n");
			mutex_lock(&crypt_stat->cs_mutex);
			crypt_stat->flags &= ~(ECRYPTFS_KEY_VALID);
			mutex_unlock(&crypt_stat->cs_mutex);
			rc = 0;
			/*
			 * Letting this continue doesn't mean to allow read/writing. It will anyway fail later.
			 *
			 * 1. In this stage, ecryptfs_stat won't have key/iv and encryption ctx.
			 * 2. ECRYPTFS_KEY_VALID bit is off, next attempt will try reading metadata again.
			 * 3. Skip DEK conversion. it cannot be done anyway.
			 */
			goto out;
		}
#endif
		goto out_put;
	}

#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
	if (mount_crypt_stat->flags & ECRYPTFS_USE_FMP)
		rc = ecryptfs_propagate_fmpinfo(inode, FMPINFO_SET);
	else
		rc = ecryptfs_propagate_fmpinfo(inode, FMPINFO_CLEAR);
#endif
	if (rc)
		goto out_put;
#ifdef CONFIG_SDP
	if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
#ifdef CONFIG_SDP_KEY_DUMP
		if (S_ISREG(ecryptfs_dentry->d_inode->i_mode)) {
			if(get_sdp_sysfs_key_dump()) {
				printk("FEK[%s] : ", ecryptfs_dentry->d_name.name);
				key_dump(crypt_stat->key, 32);
			}
		}
#endif
		/*
		 * Need to update sensitive mapping on file open
		 */
		if (S_ISREG(ecryptfs_dentry->d_inode->i_mode)) {
			ecryptfs_set_mapping_sensitive(inode, mount_crypt_stat->userid, TO_SENSITIVE);
		}
		
		if (ecryptfs_is_sdp_locked(crypt_stat->engine_id)) {
			ecryptfs_printk(KERN_INFO, "ecryptfs_open: persona is locked, rc=%d\n", rc);
		} else {
			int dek_type = crypt_stat->sdp_dek.type;

			ecryptfs_printk(KERN_INFO, "ecryptfs_open: persona is unlocked, rc=%d\n", rc);
			if(dek_type != DEK_TYPE_AES_ENC) {
				ecryptfs_printk(KERN_DEBUG, "converting dek...\n");
				rc = ecryptfs_sdp_convert_dek(ecryptfs_dentry);
				ecryptfs_printk(KERN_DEBUG, "conversion ready, rc=%d\n", rc);
				rc = 0; // TODO: Do we need to return error if conversion fails?
			}
		}
	}
#if ECRYPTFS_DEK_DEBUG
	else {
		ecryptfs_printk(KERN_INFO, "ecryptfs_open: dek_file_type is protected\n");
	}
#endif
#endif

#ifdef CONFIG_DLP
	if(crypt_stat->flags & ECRYPTFS_DLP_ENABLED) {
#if DLP_DEBUG
		printk("DLP %s: try to open %s [%lu] with crypt_stat->flags %d\n",
				__func__, ecryptfs_dentry->d_name.name, inode->i_ino, crypt_stat->flags);
#endif

		dlp_len = ecryptfs_dentry->d_inode->i_op->getxattr(
			ecryptfs_dentry,
			KNOX_DLP_XATTR_NAME,
			&dlp_data, sizeof(dlp_data));

		if(dlp_data.expiry_time.tv_sec <= 0){
#if DLP_DEBUG
			printk("[LOG] %s: DLP flag is set but it is not DLP file -> media created file but not DLP [%s]\n",
				__func__, ecryptfs_dentry->d_name.name);
#endif
			goto dlp_out;
		}

		if (dlp_is_locked(mount_crypt_stat->userid)) {
			printk("%s: DLP locked\n", __func__);
			rc = -EPERM;
			goto out_put;
		}

		if(in_egroup_p(AID_KNOX_DLP) || in_egroup_p(AID_KNOX_DLP_RESTRICTED) || in_egroup_p(AID_KNOX_DLP_MEDIA)) {
			if (dlp_len == sizeof(dlp_data)) {
				getnstimeofday(&ts);
#if DLP_DEBUG
				printk("DLP %s: current time [%ld/%ld] %s\n",
						__func__, (long)ts.tv_sec, (long)dlp_data.expiry_time.tv_sec, ecryptfs_dentry->d_name.name);
#endif
				if ((ts.tv_sec > dlp_data.expiry_time.tv_sec) &&
						dlp_isInterestedFile(mount_crypt_stat->userid, ecryptfs_dentry->d_name.name)==0) {
					
					if(in_egroup_p(AID_KNOX_DLP_MEDIA)) { //ignore media notifications
					/* Command to delete expired file  */
					cmd = sdp_fs_command_alloc(FSOP_DLP_FILE_REMOVE_MEDIA,
							current->tgid, mount_crypt_stat->userid, mount_crypt_stat->partition_id,
							inode->i_ino, GFP_KERNEL);
					} else {
					/* Command to delete expired file  */
					cmd = sdp_fs_command_alloc(FSOP_DLP_FILE_REMOVE,
							current->tgid, mount_crypt_stat->userid, mount_crypt_stat->partition_id,
							inode->i_ino, GFP_KERNEL);
					}
					rc = -ENOENT;
					goto out_put;
				}
			} else if (dlp_len == -ENODATA) {
				/* DLP flag is set, but no DLP data. Let it continue, xattr will be set later */
				printk("DLP %s: normal file [%s]\n",
						__func__, ecryptfs_dentry->d_name.name);
			} else {
				printk("DLP %s: Error, len [%ld], [%s]\n",
						__func__, (long)dlp_len, ecryptfs_dentry->d_name.name);
				rc = -EFAULT;
				goto out_put;
			}

#if DLP_DEBUG
			printk("DLP %s: DLP file [%s] opened with tgid %d, %d\n" ,
					__func__, ecryptfs_dentry->d_name.name, current->tgid, in_egroup_p(AID_KNOX_DLP_RESTRICTED));
#endif
			if(in_egroup_p(AID_KNOX_DLP_RESTRICTED)) {
				cmd = sdp_fs_command_alloc(FSOP_DLP_FILE_OPENED,
						current->tgid, mount_crypt_stat->userid, mount_crypt_stat->partition_id,
						inode->i_ino, GFP_KERNEL);
			} else if(in_egroup_p(AID_KNOX_DLP)) {
				cmd = sdp_fs_command_alloc(FSOP_DLP_FILE_OPENED_CREATOR,
						current->tgid, mount_crypt_stat->userid, mount_crypt_stat->partition_id,
						inode->i_ino, GFP_KERNEL);
			} else {
				printk("DLP %s: DLP open file file from Media process ignoring sending event\n", __func__);
			}
		} else {
			printk("DLP %s: not DLP app [%s]\n", __func__, current->comm);
			printk("DLP %s: DLP open file failed\n", __func__);
			cmd = sdp_fs_command_alloc(FSOP_DLP_FILE_ACCESS_DENIED,
							current->tgid, mount_crypt_stat->userid, mount_crypt_stat->partition_id,
							inode->i_ino, GFP_KERNEL);
							
			rc = -EPERM;
			goto out_put;
		}
	}

dlp_out:
#endif

	ecryptfs_printk(KERN_DEBUG, "inode w/ addr = [0x%p], i_ino = "
			"[0x%.16lx] size: [0x%.16llx]\n", inode, inode->i_ino,
			(unsigned long long)i_size_read(inode));
	goto out;
out_put:
	ecryptfs_put_lower_file(inode);
out_free:
	kmem_cache_free(ecryptfs_file_info_cache,
			ecryptfs_file_to_private(file));
out:
#ifdef CONFIG_DLP
	if(cmd) {
		sdp_fs_request(cmd, NULL);
		sdp_fs_command_free(cmd);
	}
#endif
#ifdef CONFIG_SDP
	if (rc && (rc != -ENOENT)) {
		cmd = sdp_fs_command_alloc(FSOP_AUDIT_FAIL_ACCESS,
				current->tgid, mount_crypt_stat->userid, mount_crypt_stat->partition_id,
				inode->i_ino, GFP_KERNEL);
		if(cmd) {
			sdp_fs_request(cmd, NULL);
			sdp_fs_command_free(cmd);
		}
	}
#endif
	return rc;
}

static int ecryptfs_flush(struct file *file, fl_owner_t td)
{
	struct file *lower_file = ecryptfs_file_to_lower(file);

	if (lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		return lower_file->f_op->flush(lower_file, td);
	}

	return 0;
}

static int ecryptfs_release(struct inode *inode, struct file *file)
{
#ifdef CONFIG_SDP
	struct ecryptfs_crypt_stat *crypt_stat;
	crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;

	mutex_lock(&crypt_stat->cs_mutex);
#endif
	ecryptfs_put_lower_file(inode);
#ifdef CONFIG_SDP
	mutex_unlock(&crypt_stat->cs_mutex);
#endif
	kmem_cache_free(ecryptfs_file_info_cache,
			ecryptfs_file_to_private(file));
	return 0;
}

static int
ecryptfs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	int rc;

	rc = filemap_write_and_wait(file->f_mapping);
	if (rc)
		return rc;

	return vfs_fsync(ecryptfs_file_to_lower(file), datasync);
}

static int ecryptfs_fasync(int fd, struct file *file, int flag)
{
	int rc = 0;
	struct file *lower_file = NULL;

	lower_file = ecryptfs_file_to_lower(file);
	if (lower_file->f_op->fasync)
		rc = lower_file->f_op->fasync(fd, lower_file, flag);
	return rc;
}

static long
ecryptfs_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct file *lower_file = ecryptfs_file_to_lower(file);
	long rc = -ENOTTY;
#ifdef CONFIG_WTL_ENCRYPTION_FILTER
	if (cmd == ECRYPTFS_IOCTL_GET_ATTRIBUTES) {
		u32 __user *user_attr = (u32 __user *)arg;
		u32 attr = 0;
		char filename[NAME_MAX+1] = {0};
		struct dentry *ecryptfs_dentry = file->f_path.dentry;
		struct ecryptfs_mount_crypt_stat *mount_crypt_stat =
			&ecryptfs_superblock_to_private(ecryptfs_dentry->d_sb)
				->mount_crypt_stat;

		struct inode *inode = ecryptfs_dentry->d_inode;
		struct ecryptfs_crypt_stat *crypt_stat =
			&ecryptfs_inode_to_private(inode)->crypt_stat;
		struct dentry *fp_dentry =
			ecryptfs_inode_to_private(inode)->lower_file->f_dentry;
		if (fp_dentry->d_name.len <= NAME_MAX)
			memcpy(filename, fp_dentry->d_name.name,
					fp_dentry->d_name.len + 1);

		mutex_lock(&crypt_stat->cs_mutex);
		if ((crypt_stat->flags & ECRYPTFS_ENCRYPTED
			|| crypt_stat->flags & ECRYPTFS_ENCRYPTED_OTHER_DEVICE)
			|| ((mount_crypt_stat->flags
					& ECRYPTFS_ENABLE_FILTERING)
				&& (is_file_name_match
					(mount_crypt_stat, fp_dentry)
				|| is_file_ext_match
					(mount_crypt_stat, filename)))) {
			if (crypt_stat->flags & ECRYPTFS_KEY_VALID)
				attr = ECRYPTFS_WAS_ENCRYPTED;
			else
				attr = ECRYPTFS_WAS_ENCRYPTED_OTHER_DEVICE;
		}
		mutex_unlock(&crypt_stat->cs_mutex);
		put_user(attr, user_attr);
		return 0;
	}
#endif

#ifdef CONFIG_SDP
	rc = ecryptfs_do_sdp_ioctl(file, cmd, arg);
	if (rc != EOPNOTSUPP)
		return rc;
#else
	printk("%s CONFIG_SDP not enabled \n", __func__);
#endif

	if (!lower_file->f_op->unlocked_ioctl)
		return rc;

	switch (cmd) {
	case FITRIM:
	case FS_IOC_GETFLAGS:
	case FS_IOC_SETFLAGS:
	case FS_IOC_GETVERSION:
	case FS_IOC_SETVERSION:
		rc = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);
		fsstack_copy_attr_all(file_inode(file), file_inode(lower_file));

		return rc;
	default:
		return rc;
	}
}

#ifdef CONFIG_COMPAT
static long
ecryptfs_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct file *lower_file = ecryptfs_file_to_lower(file);
	long rc = -ENOIOCTLCMD;
#ifdef CONFIG_WTL_ENCRYPTION_FILTER
	if (cmd == ECRYPTFS_IOCTL_GET_ATTRIBUTES) {
		u32 __user *user_attr = (u32 __user *)arg;
		u32 attr = 0;
		char filename[NAME_MAX+1] = {0};
		struct dentry *ecryptfs_dentry = file->f_path.dentry;
		struct ecryptfs_mount_crypt_stat *mount_crypt_stat =
			&ecryptfs_superblock_to_private(ecryptfs_dentry->d_sb)
				->mount_crypt_stat;

		struct inode *inode = ecryptfs_dentry->d_inode;
		struct ecryptfs_crypt_stat *crypt_stat =
			&ecryptfs_inode_to_private(inode)->crypt_stat;
		struct dentry *fp_dentry =
			ecryptfs_inode_to_private(inode)->lower_file->f_dentry;
		if (fp_dentry->d_name.len <= NAME_MAX)
			memcpy(filename, fp_dentry->d_name.name,
					fp_dentry->d_name.len + 1);

		mutex_lock(&crypt_stat->cs_mutex);
		if ((crypt_stat->flags & ECRYPTFS_ENCRYPTED
			|| crypt_stat->flags & ECRYPTFS_ENCRYPTED_OTHER_DEVICE)
			|| ((mount_crypt_stat->flags
					& ECRYPTFS_ENABLE_FILTERING)
				&& (is_file_name_match
					(mount_crypt_stat, fp_dentry)
				|| is_file_ext_match
					(mount_crypt_stat, filename)))) {
			if (crypt_stat->flags & ECRYPTFS_KEY_VALID)
				attr = ECRYPTFS_WAS_ENCRYPTED;
			else
				attr = ECRYPTFS_WAS_ENCRYPTED_OTHER_DEVICE;
		}
		mutex_unlock(&crypt_stat->cs_mutex);
		put_user(attr, user_attr);
		return 0;
	}
#endif

#ifdef CONFIG_SDP
	rc = ecryptfs_do_sdp_ioctl(file, cmd, arg);
	if (rc != EOPNOTSUPP)
		return rc;
#else
	printk("%s CONFIG_SDP not enabled \n", __func__);
#endif

	if (!lower_file->f_op->compat_ioctl)
		return rc;

	switch (cmd) {
	case FITRIM:
	case FS_IOC32_GETFLAGS:
	case FS_IOC32_SETFLAGS:
	case FS_IOC32_GETVERSION:
	case FS_IOC32_SETVERSION:
		rc = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);
		fsstack_copy_attr_all(file_inode(file), file_inode(lower_file));

		return rc;
	default:
		return rc;
	}
}
#endif

#ifdef CONFIG_WTL_ENCRYPTION_FILTER
int is_file_name_match(struct ecryptfs_mount_crypt_stat *mcs,
					struct dentry *fp_dentry)
{
	int i;
	char *str = NULL;
	if (!(strcmp("/", fp_dentry->d_name.name))
		|| !(strcmp("", fp_dentry->d_name.name)))
		return 0;
	str = kzalloc(mcs->max_name_filter_len + 1, GFP_KERNEL);
	if (!str) {
		printk(KERN_ERR "%s: Out of memory whilst attempting "
			       "to kzalloc [%d] bytes\n", __func__,
			       (mcs->max_name_filter_len + 1));
		return 0;
	}

	for (i = 0; i < ENC_NAME_FILTER_MAX_INSTANCE; i++) {
		int len = 0;
		struct dentry *p = fp_dentry;
		if (!strlen(mcs->enc_filter_name[i]))
			break;

		while (1) {
			if (len == 0) {
				len = strlen(p->d_name.name);
				if (len > mcs->max_name_filter_len)
					break;
				strcpy(str, p->d_name.name);
			} else {
				len = len + 1 + strlen(p->d_name.name) ;
				if (len > mcs->max_name_filter_len)
					break;
				strcat(str, "/");
				strcat(str, p->d_name.name);
			}

			if (strnicmp(str, mcs->enc_filter_name[i], len))
				break;
			p = p->d_parent;

			if (!(strcmp("/", p->d_name.name))
				|| !(strcmp("", p->d_name.name))) {
				if (len == strlen(mcs->enc_filter_name[i])) {
					kfree(str);
					return 1;
				}
				break;
			}
		}
	}
	kfree(str);
	return 0;
}

int is_file_ext_match(struct ecryptfs_mount_crypt_stat *mcs, char *str)
{
	int i;
	char ext[NAME_MAX + 1] = {0};

	char *token;
	int count = 0;
	while ((token = strsep(&str, ".")) != NULL) {
		strncpy(ext, token, NAME_MAX);
		count++;
	}
	if (count <= 1)
		return 0;

	for (i = 0; i < ENC_EXT_FILTER_MAX_INSTANCE; i++) {
		if (!strlen(mcs->enc_filter_ext[i]))
			return 0;
		if (strlen(ext) != strlen(mcs->enc_filter_ext[i]))
			continue;
		if (!strnicmp(ext, mcs->enc_filter_ext[i], strlen(ext)))
			return 1;
	}
	return 0;
}
#endif

const struct file_operations ecryptfs_dir_fops = {
	.iterate = ecryptfs_readdir,
	.read = generic_read_dir,
	.unlocked_ioctl = ecryptfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ecryptfs_compat_ioctl,
#endif
	.open = ecryptfs_open,
	.flush = ecryptfs_flush,
	.release = ecryptfs_release,
	.fsync = ecryptfs_fsync,
	.fasync = ecryptfs_fasync,
	.splice_read = generic_file_splice_read,
	.llseek = default_llseek,
};

const struct file_operations ecryptfs_main_fops = {
	.llseek = generic_file_llseek,
	.read = new_sync_read,
	.read_iter = ecryptfs_read_update_atime,
	.write = new_sync_write,
	.write_iter = generic_file_write_iter,
	.iterate = ecryptfs_readdir,
	.unlocked_ioctl = ecryptfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ecryptfs_compat_ioctl,
#endif
	.mmap = ecryptfs_mmap,
	.open = ecryptfs_open,
	.flush = ecryptfs_flush,
	.release = ecryptfs_release,
	.fsync = ecryptfs_fsync,
	.fasync = ecryptfs_fasync,
	.splice_read = generic_file_splice_read,
};
