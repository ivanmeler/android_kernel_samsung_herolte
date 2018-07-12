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

#include <linux/pagemap.h>
#include <linux/crypto.h>
#include <asm/unaligned.h>

#include <sdp/fs_handler.h>
#include "ecryptfs_dek.h"
#include "ecryptfs_sdp_chamber.h"
#include "mm.h"

extern int dek_encrypt_dek_efs(int engine_id, dek_t *plainDek, dek_t *encDek);
extern int dek_decrypt_dek_efs(int engine_id, dek_t *encDek, dek_t *plainDek);
extern int dek_is_locked(int engine_id);

static int ecryptfs_update_crypt_flag(struct dentry *dentry, enum sdp_op operation);

static const char* get_op_name(enum sdp_op operation) {
   switch (operation)
   {
      case TO_SENSITIVE: return "TO_SENSITIVE";
      case TO_PROTECTED: return "TO_PROTECTED";
      default: return "OP_UNKNOWN";
   }
}

static int ecryptfs_set_key(struct ecryptfs_crypt_stat *crypt_stat) {
	int rc = 0;

	mutex_lock(&crypt_stat->cs_tfm_mutex);
	if (!(crypt_stat->flags & ECRYPTFS_KEY_SET)) {
		rc = crypto_ablkcipher_setkey(crypt_stat->tfm, crypt_stat->key,
				crypt_stat->key_size);
		if (rc) {
			ecryptfs_printk(KERN_ERR,
					"Error setting key; rc = [%d]\n",
					rc);
			rc = -EINVAL;
			goto out;
		}
		crypt_stat->flags |= ECRYPTFS_KEY_SET;
		crypt_stat->flags |= ECRYPTFS_KEY_VALID;
	}
out:
	mutex_unlock(&crypt_stat->cs_tfm_mutex);
	return rc;
}

int ecryptfs_is_sdp_locked(int engine_id)
{
    if(engine_id < 0) {
        DEK_LOGE("invalid engine_id[%d]\n", engine_id);
        return 0;
    }

	return dek_is_locked(engine_id);
}

extern int32_t sdp_mm_set_process_sensitive(unsigned int proc_id);

#define PSEUDO_KEY_LEN 32
const char pseudo_key[PSEUDO_KEY_LEN] = {
        // PSEUDOSDP
        0x50, 0x53, 0x55, 0x45, 0x44, 0x4f, 0x53, 0x44,
        0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void ecryptfs_clean_sdp_dek(struct ecryptfs_crypt_stat *crypt_stat)
{
    int rc = 0;

	printk("%s()\n", __func__);

	if(crypt_stat->tfm) {
	    mutex_lock(&crypt_stat->cs_tfm_mutex);
	    rc = crypto_ablkcipher_setkey(crypt_stat->tfm, pseudo_key,
	            PSEUDO_KEY_LEN);
        if (rc) {
            ecryptfs_printk(KERN_ERR,
                    "Error cleaning tfm rc = [%d]\n",
                    rc);
        }
	    mutex_unlock(&crypt_stat->cs_tfm_mutex);
	}

	memset(crypt_stat->key, 0, ECRYPTFS_MAX_KEY_BYTES);
	crypt_stat->flags &= ~(ECRYPTFS_KEY_SET);
	crypt_stat->flags &= ~(ECRYPTFS_KEY_VALID);
}

int ecryptfs_get_sdp_dek(struct ecryptfs_crypt_stat *crypt_stat)
{
	int rc = 0;

	if((crypt_stat->flags & ECRYPTFS_KEY_SET) && (crypt_stat->flags & ECRYPTFS_POLICY_APPLIED)) {
		DEK_LOGE("get_sdp_dek: key is already set (success)\n");
		return 0;
	}

	if(crypt_stat->flags & ECRYPTFS_DEK_SDP_ENABLED) {
		if(crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
			dek_t DEK;

			if(crypt_stat->engine_id < 0) {
		        DEK_LOGE("get_sdp_dek: invalid engine-id"
		                "(ECRYPTFS_DEK_IS_SENSITIVE:ON, engine_id:%d)\n", crypt_stat->engine_id);
		        goto out;
			}
			memset(crypt_stat->key, 0, ECRYPTFS_MAX_KEY_BYTES);

			if (crypt_stat->sdp_dek.type != DEK_TYPE_PLAIN) {
				rc = dek_decrypt_dek_efs(crypt_stat->engine_id, &crypt_stat->sdp_dek, &DEK);
			} else {
				DEK_LOGE("DEK already plaintext, skip decryption");
				rc = 0;
				goto out;
			}
			if (rc < 0) {
				DEK_LOGE("Error decypting dek; rc = [%d]\n", rc);
				memset(&DEK, 0, sizeof(dek_t));
				goto out;
			}
			memcpy(crypt_stat->key, DEK.buf, DEK.len);
			crypt_stat->key_size = DEK.len;
			memset(&DEK, 0, sizeof(dek_t));
		} else {
#if ECRYPTFS_DEK_DEBUG
			DEK_LOGD("file is not sensitive\n");
#endif
		}
	}
out:
/*
 * Succeeded
 */
	if(!rc) {
		sdp_mm_set_process_sensitive(current->pid);
		rc = ecryptfs_set_key(crypt_stat);
	} else {
	/*
	 * Error
	 */
		ecryptfs_clean_sdp_dek(crypt_stat);
	}

	return rc;
}

int write_dek_packet(char *dest,
		struct ecryptfs_crypt_stat *crypt_stat,
		size_t *written) {
	*written = 0;
	dest[(*written)++] = ECRYPTFS_DEK_PACKET_TYPE;

	memset(dest + *written, 0, PKG_NAME_SIZE);
	memcpy(dest + *written, current->comm, PKG_NAME_SIZE);
	(*written) += PKG_NAME_SIZE;

	put_unaligned_be32(from_kuid(&init_user_ns, current_euid()), dest + *written);
	(*written) += 4;

	memset(dest + *written, 0, DEK_MAXLEN);

	if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
        if(crypt_stat->flags & ECRYPTFS_DEK_MULTI_ENGINE) {
            put_unaligned_be32(crypt_stat->engine_id, dest + *written);
            (*written) += 4;
        }

		put_unaligned_be32(crypt_stat->sdp_dek.type, dest + *written);
		(*written) += 4;
		put_unaligned_be32(crypt_stat->sdp_dek.len, dest + *written);
		(*written) += 4;
		memcpy(dest + *written, crypt_stat->sdp_dek.buf, crypt_stat->sdp_dek.len);
		(*written) += crypt_stat->sdp_dek.len;
	}

	return 0;
}

int parse_dek_packet(char *data,
		struct ecryptfs_crypt_stat *crypt_stat,
		size_t *packet_size) {
	int rc = 0;
	char temp_comm[PKG_NAME_SIZE]; //test
	int temp_euid;

	if (crypt_stat->file_version == 0)
		return -EPERM;

	(*packet_size) = 0;

	if (data[(*packet_size)++] != ECRYPTFS_DEK_PACKET_TYPE) {
		DEK_LOGE("First byte != 0x%.2x; invalid packet\n",
				ECRYPTFS_DEK_PACKET_TYPE);
		rc = -EINVAL;
	}

	memcpy(temp_comm, &data[*packet_size], PKG_NAME_SIZE);
	(*packet_size) += PKG_NAME_SIZE;

	temp_euid = get_unaligned_be32(data + *packet_size);
	(*packet_size) += 4;

	if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
	    if(crypt_stat->flags & ECRYPTFS_DEK_MULTI_ENGINE) {
	        crypt_stat->engine_id = get_unaligned_be32(data + *packet_size);
	        (*packet_size) += 4;
	    } else {
	        /**
	         * If eCryptfs header doesn't have engine-id,
	         * we assign it from mount_crypt_stat->userid
	         * (Fils created in old version)
	         */
	        crypt_stat->engine_id = crypt_stat->mount_crypt_stat->userid;
	    }

		crypt_stat->sdp_dek.type = get_unaligned_be32(data + *packet_size);
		(*packet_size) += 4;
		if(crypt_stat->sdp_dek.type < 0 || crypt_stat->sdp_dek.type > 6)
			return -EINVAL;
		crypt_stat->sdp_dek.len = get_unaligned_be32(data + *packet_size);
		(*packet_size) += 4;
		if(crypt_stat->sdp_dek.len <= 0 || crypt_stat->sdp_dek.len > DEK_MAXLEN)
			return -EFAULT;
		memcpy(crypt_stat->sdp_dek.buf, &data[*packet_size], crypt_stat->sdp_dek.len);
		(*packet_size) += crypt_stat->sdp_dek.len;
	}
	
#if ECRYPTFS_DEK_DEBUG
	DEK_LOGD("%s() : comm : %s [euid:%d]\n",
		__func__, temp_comm, temp_euid);
#endif
	return rc;
}

static int ecryptfs_dek_copy_mount_wide_sigs_to_inode_sigs(
    struct ecryptfs_crypt_stat *crypt_stat,
    struct ecryptfs_mount_crypt_stat *mount_crypt_stat)
{
    struct ecryptfs_global_auth_tok *global_auth_tok;
    int rc = 0;

    mutex_lock(&crypt_stat->keysig_list_mutex);
    mutex_lock(&mount_crypt_stat->global_auth_tok_list_mutex);

    list_for_each_entry(global_auth_tok,
                &mount_crypt_stat->global_auth_tok_list,
                mount_crypt_stat_list) {
        if (global_auth_tok->flags & ECRYPTFS_AUTH_TOK_FNEK)
            continue;
        rc = ecryptfs_add_keysig(crypt_stat, global_auth_tok->sig);
        if (rc) {
            printk(KERN_ERR "Error adding keysig; rc = [%d]\n", rc);
            goto out;
        }
    }

out:
    mutex_unlock(&mount_crypt_stat->global_auth_tok_list_mutex);
    mutex_unlock(&crypt_stat->keysig_list_mutex);
    return rc;
}

#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
static void ecryptfs_propagate_flag(struct file *file, int userid, enum sdp_op operation) {
    struct file *f = file;
    do {
		if(!f)
			return ;

		DEK_LOGD("%s file: %p [%s]\n",__func__, f, f->f_inode->i_sb->s_type->name);
        if (operation == TO_SENSITIVE) {
			mapping_set_sensitive(f->f_mapping);
		} else {
			mapping_clear_sensitive(f->f_mapping);
		}
		f->f_mapping->userid = userid;
    } while (f->f_op->get_lower_file && (f = f->f_op->get_lower_file(f)));
}
#endif

void ecryptfs_set_mapping_sensitive(struct inode *ecryptfs_inode, int userid, enum sdp_op operation) {
#if defined(CONFIG_MMC_DW_FMP_ECRYPT_FS) || defined(CONFIG_UFS_FMP_ECRYPT_FS)
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat = NULL;
	struct ecryptfs_inode_info *inode_info;
	
	inode_info = ecryptfs_inode_to_private(ecryptfs_inode);
	DEK_LOGD("%s inode: %p lower_file_count: %d\n",__func__, ecryptfs_inode,atomic_read(&inode_info->lower_file_count));
	mount_crypt_stat = &ecryptfs_superblock_to_private(ecryptfs_inode->i_sb)->mount_crypt_stat;
	
	if (operation == TO_SENSITIVE) {
		mapping_set_sensitive(ecryptfs_inode->i_mapping);
	} else {
		mapping_clear_sensitive(ecryptfs_inode->i_mapping);
	}
	ecryptfs_inode->i_mapping->userid = userid;
	/*
	 * If FMP is in use, need to set flag to lower filesystems too recursively
	 */
	if (mount_crypt_stat->flags & ECRYPTFS_USE_FMP) {
		if(inode_info->lower_file) {
			ecryptfs_propagate_flag(inode_info->lower_file, userid, operation);
		}
	}
#else
	if (operation == TO_SENSITIVE) {
		mapping_set_sensitive(ecryptfs_inode->i_mapping);
	} else {
		mapping_clear_sensitive(ecryptfs_inode->i_mapping);
	}
	ecryptfs_inode->i_mapping->userid = userid;
#endif
}

/*
 * set sensitive flag, update metadata
 * Set cached inode pages to sensitive
 */
static int ecryptfs_update_crypt_flag(struct dentry *dentry, enum sdp_op operation)
{
	int rc = 0;
	struct inode *inode;
	struct inode *lower_inode;
	struct ecryptfs_crypt_stat *crypt_stat;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat;
	u32 tmp_flags;

	DEK_LOGE("%s(operation:%s) entered\n", __func__, get_op_name(operation));

	crypt_stat = &ecryptfs_inode_to_private(dentry->d_inode)->crypt_stat;
	mount_crypt_stat = &ecryptfs_superblock_to_private(dentry->d_sb)->mount_crypt_stat;
	if (!(crypt_stat->flags & ECRYPTFS_STRUCT_INITIALIZED))
		ecryptfs_init_crypt_stat(crypt_stat);
	inode = dentry->d_inode;
	lower_inode = ecryptfs_inode_to_lower(inode);

    /*
     * To update metadata we need to make sure keysig_list contains fekek.
     * Because our EDEK is stored along with key for protected file.
     */
    if(list_empty(&crypt_stat->keysig_list))
        ecryptfs_dek_copy_mount_wide_sigs_to_inode_sigs(crypt_stat, mount_crypt_stat);

	mutex_lock(&crypt_stat->cs_mutex);
	rc = ecryptfs_get_lower_file(dentry, inode);
	if (rc) {
		mutex_unlock(&crypt_stat->cs_mutex);
		DEK_LOGE("ecryptfs_get_lower_file rc=%d\n", rc);
		return rc;
	}

	tmp_flags = crypt_stat->flags;
	if (operation == TO_SENSITIVE) {
		crypt_stat->flags |= ECRYPTFS_DEK_IS_SENSITIVE;
		crypt_stat->flags |= ECRYPTFS_DEK_MULTI_ENGINE;
		/*
		* Set sensitive to inode mapping
		*/
		ecryptfs_set_mapping_sensitive(inode, mount_crypt_stat->userid, TO_SENSITIVE);
	} else {
		crypt_stat->flags &= ~ECRYPTFS_DEK_IS_SENSITIVE;
        crypt_stat->flags &= ~ECRYPTFS_DEK_MULTI_ENGINE;

		/*
		* Set protected to inode mapping
		*/
		ecryptfs_set_mapping_sensitive(inode, mount_crypt_stat->userid, TO_PROTECTED);
	}

	rc = ecryptfs_write_metadata(dentry, inode);
	if (rc) {
		crypt_stat->flags = tmp_flags;
		DEK_LOGE("ecryptfs_write_metadata rc=%d\n", rc);
		goto out;
	}

	rc = ecryptfs_write_inode_size_to_metadata(inode);
	if (rc) {
		DEK_LOGE("Problem with "
				"ecryptfs_write_inode_size_to_metadata; "
				"rc = [%d]\n", rc);
		goto out;
	}

out:
	mutex_unlock(&crypt_stat->cs_mutex);
	ecryptfs_put_lower_file(inode);
	fsstack_copy_attr_all(inode, lower_inode);
	return rc;
}

void ecryptfs_fs_request_callback(int opcode, int ret, unsigned long ino) {
    DEK_LOGD("%s opcode<%d> ret<%d> ino<%ld>\n", __func__, opcode, ret, ino);
}

int ecryptfs_sdp_set_sensitive(int engine_id, struct dentry *dentry) {
	int rc = 0;
	struct inode *inode = dentry->d_inode;
	struct ecryptfs_crypt_stat *crypt_stat =
			&ecryptfs_inode_to_private(inode)->crypt_stat;
	dek_t DEK;
    int id_bak = crypt_stat->engine_id;

	DEK_LOGD("%s(%s)\n", __func__, dentry->d_name.name);

	if(S_ISDIR(inode->i_mode)) {
        crypt_stat->engine_id = engine_id;
        crypt_stat->flags |= ECRYPTFS_DEK_IS_SENSITIVE;

        rc = 0;
	} else if(S_ISREG(inode->i_mode)) {
	    crypt_stat->engine_id = engine_id;

    	if (crypt_stat->key_size > ECRYPTFS_MAX_KEY_BYTES ||
	    		crypt_stat->key_size > DEK_MAXLEN ||
	    		crypt_stat->key_size > UINT_MAX){
	    	DEK_LOGE("%s Too large key_size\n", __func__);
	    	rc = -EFAULT;
    		goto out;
	    }
	    memcpy(DEK.buf, crypt_stat->key, crypt_stat->key_size);
	    DEK.len = (unsigned int)crypt_stat->key_size;
	    DEK.type = DEK_TYPE_PLAIN;

	    rc = dek_encrypt_dek_efs(crypt_stat->engine_id, &DEK,  &crypt_stat->sdp_dek);
	    if (rc < 0) {
	        DEK_LOGE("Error encrypting dek; rc = [%d]\n", rc);
	        memset(&crypt_stat->sdp_dek, 0, sizeof(dek_t));
	        goto out;
	    }
#if 0
	    /*
	     * We don't have to clear FEK after set-sensitive.
	     * FEK will be closed when the file is closed
	     */
	    memset(crypt_stat->key, 0, crypt_stat->key_size);
	    crypt_stat->flags &= ~(ECRYPTFS_KEY_SET);
#else
	    /*
	     * set-key after set sensitive file.
	     * Well when the file is just created and we do set_sensitive, the key is not set in the
	     * tfm. later SDP code, set-key is done while encryption, trying to decrypt EFEK.
	     *
	     * Here is the case in locked state user process want to create/write a file.
	     * the process open the file, automatically becomes sensitive by vault logic,
	     * and do the encryption, then boom. failed to decrypt EFEK even if FEK is
	     * available
	     */
	    rc = ecryptfs_set_key(crypt_stat);
	    if(rc) goto out;
#endif

	    ecryptfs_update_crypt_flag(dentry, TO_SENSITIVE);
	}

out:
    if(rc) crypt_stat->engine_id = id_bak;
	memset(&DEK, 0, sizeof(dek_t));
	return rc;
}

int ecryptfs_sdp_set_protected(struct dentry *dentry) {
    int rc = 0;
    struct inode *inode = dentry->d_inode;
    struct ecryptfs_crypt_stat *crypt_stat =
            &ecryptfs_inode_to_private(inode)->crypt_stat;

    DEK_LOGD("%s(%s)\n", __func__, dentry->d_name.name);

    if(IS_CHAMBER_DENTRY(dentry)) {
        DEK_LOGE("can't set-protected to chamber directory");
        return -EIO;
    }

    if(crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
        if(crypt_stat->engine_id < 0) {
            DEK_LOGE("%s: invalid engine-id (ECRYPTFS_DEK_IS_SENSITIVE:ON, engine_id:%d)\n",
                    __func__, crypt_stat->engine_id);
            return -EIO;
        }

        if(ecryptfs_is_sdp_locked(crypt_stat->engine_id)) {
            DEK_LOGE("%s: Failed. (engine_id:%d locked)\n",
                    __func__, crypt_stat->engine_id);
            return -EIO;
        }

        if(S_ISDIR(inode->i_mode)) {
            crypt_stat->flags &= ~ECRYPTFS_DEK_IS_SENSITIVE;
            rc = 0;
        } else {
            rc = ecryptfs_get_sdp_dek(crypt_stat);
            if (rc) {
                ecryptfs_printk(KERN_ERR, "%s Get SDP key failed\n", __func__);
                goto out;
            }
            /*
             * TODO : double check if need to compute iv here
             */
            rc = ecryptfs_compute_root_iv(crypt_stat);
            if (rc) {
                ecryptfs_printk(KERN_ERR, "Error computing "
                        "the root IV\n");
                goto out;
            }

            rc = ecryptfs_set_key(crypt_stat);
            if(rc) goto out;

        ecryptfs_update_crypt_flag(dentry, TO_PROTECTED);
        }
    } else {
        rc = 0; //already protected
    }
out:
    return rc;
}

int ecryptfs_sdp_convert_dek(struct dentry *dentry) {
	int rc = 0;
	struct inode *inode = dentry->d_inode;
	struct ecryptfs_crypt_stat *crypt_stat =
			&ecryptfs_inode_to_private(inode)->crypt_stat;
	dek_t DEK;

	rc = dek_decrypt_dek_efs(crypt_stat->engine_id, &crypt_stat->sdp_dek, &DEK);
	if (rc < 0) {
		DEK_LOGE("Error converting dek [DEC]; rc = [%d]\n", rc);
		goto out;
	}

	rc = dek_encrypt_dek_efs(crypt_stat->engine_id, &DEK,  &crypt_stat->sdp_dek);
	if (rc < 0) {
		DEK_LOGE("Error converting dek [ENC]; rc = [%d]\n", rc);
		goto out;
	}

	rc = ecryptfs_update_crypt_flag(dentry, TO_SENSITIVE);
	if (rc < 0) {
		DEK_LOGE("Error converting dek [FLAG]; rc = [%d]\n", rc);
		goto out;
	}
out:
	memset(&DEK, 0, sizeof(dek_t));
	return rc;
}

long ecryptfs_do_sdp_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	char filename[NAME_MAX+1] = {0};
	void __user *ubuf = (void __user *)arg;
	struct dentry *ecryptfs_dentry = file->f_path.dentry;
	struct inode *inode = ecryptfs_dentry->d_inode;
	struct ecryptfs_crypt_stat *crypt_stat =
			&ecryptfs_inode_to_private(inode)->crypt_stat;
	struct dentry *fp_dentry =
			ecryptfs_inode_to_private(inode)->lower_file->f_dentry;
    struct ecryptfs_mount_crypt_stat *mount_crypt_stat  =
            &ecryptfs_superblock_to_private(inode->i_sb)->mount_crypt_stat;
    int rc;

	if (fp_dentry->d_name.len <= NAME_MAX)
			memcpy(filename, fp_dentry->d_name.name,
					fp_dentry->d_name.len + 1);

	DEK_LOGD("%s(%s)\n", __func__, ecryptfs_dentry->d_name.name);

	if (!(crypt_stat->flags & ECRYPTFS_DEK_SDP_ENABLED)) {
		DEK_LOGE("SDP not enabled, skip sdp ioctl\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case ECRYPTFS_IOCTL_GET_SDP_INFO: {
		dek_arg_get_sdp_info req;

		DEK_LOGD("ECRYPTFS_IOCTL_GET_SDP_INFO\n");
	
		memset(&req, 0, sizeof(dek_arg_get_sdp_info));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			DEK_LOGE("can't copy from user\n");
			return -EFAULT;
		} else {
			mutex_lock(&crypt_stat->cs_mutex);

			req.engine_id = crypt_stat->engine_id;

			if (crypt_stat->flags & ECRYPTFS_DEK_SDP_ENABLED) {
				req.sdp_enabled = 1;
			} else {
				req.sdp_enabled = 0;
			}
			if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
				req.is_sensitive = 1;
			} else {
				req.is_sensitive = 0;
			}
            if (crypt_stat->flags & ECRYPTFS_SDP_IS_CHAMBER_DIR) {
                req.is_chamber = 1;
            } else {
                req.is_chamber = 0;
            }
			req.type = crypt_stat->sdp_dek.type;
			mutex_unlock(&crypt_stat->cs_mutex);
		}
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			DEK_LOGE("can't copy to user\n");
			return -EFAULT;
		}

		break;
		}

    case ECRYPTFS_IOCTL_SET_PROTECTED: {
        ecryptfs_printk(KERN_DEBUG, "ECRYPTFS_IOCTL_SET_PROTECTED\n");

        if(!is_current_epmd()) {
            DEK_LOGE("only epmd can call this\n");
            DEK_LOGE("Permission denied\n");
            return -EACCES;
        }

        if (!(crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE)) {
            DEK_LOGE("already protected file\n");
            return 0;
        }

        if (S_ISDIR(ecryptfs_dentry->d_inode->i_mode)) {
            DEK_LOGE("Set protected directory\n");
            crypt_stat->flags &= ~ECRYPTFS_DEK_IS_SENSITIVE;
            break;
        }

        rc = ecryptfs_sdp_set_protected(ecryptfs_dentry);
        if (rc) {
            DEK_LOGE("Failed to set protected rc(%d)\n", rc);
            return rc;
        }
        break;
    }

	case ECRYPTFS_IOCTL_SET_SENSITIVE: {
		dek_arg_set_sensitive req;

		ecryptfs_printk(KERN_DEBUG, "ECRYPTFS_IOCTL_SET_SENSITIVE\n");
		if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
			DEK_LOGE("already sensitive file\n");
			return 0;
		}

		memset(&req, 0, sizeof(dek_arg_set_sensitive));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			DEK_LOGE("can't copy from user\n");
			memset(&req, 0, sizeof(dek_arg_set_sensitive));
			return -EFAULT;
		} else {
		    rc = ecryptfs_sdp_set_sensitive(req.engine_id, ecryptfs_dentry);
			if (rc) {
				DEK_LOGE("failed to set sensitive rc(%d)\n", rc);
				memset(&req, 0, sizeof(dek_arg_set_sensitive));
				return rc;
			}
		}
		memset(&req, 0, sizeof(dek_arg_set_sensitive));
		break;
	}

	case ECRYPTFS_IOCTL_ADD_CHAMBER: {
	    dek_arg_add_chamber req;
	    int engineid;

        if (!S_ISDIR(ecryptfs_dentry->d_inode->i_mode)) {
            DEK_LOGE("Not a directory\n");
            return -ENOTDIR;
        }

	    if(!is_current_epmd()) {
            DEK_LOGE("only epmd can call this\n");
            DEK_LOGE("Permission denied\n");
	        return -EACCES;
	    }

        memset(&req, 0, sizeof(req));
        if(copy_from_user(&req, ubuf, sizeof(req))) {
            DEK_LOGE("can't copy from user\n");
            memset(&req, 0, sizeof(req));
            return -EFAULT;
        }

	    if(!IS_UNDER_ROOT(ecryptfs_dentry)) {
            DEK_LOGE("Chamber has to be under root directory");
            return -EFAULT;
	    }

	    if(is_chamber_directory(mount_crypt_stat, ecryptfs_dentry->d_name.name, &engineid)) {
	        DEK_LOGE("Already chamber directory [%s] engine:%d\n",
	                ecryptfs_dentry->d_name.name, engineid);
	        if(engineid != req.engine_id) {
	            DEK_LOGE("Attemping to change engine-id[%d] -> [%d] : Failed\n",
	                    engineid, req.engine_id);
	            return -EACCES;
	        }

            set_chamber_flag(engineid, inode);
            break;
	    }

	    rc = add_chamber_directory(mount_crypt_stat, req.engine_id,
	            ecryptfs_dentry->d_name.name);
	    if(rc) {
	        DEK_LOGE("add_chamber_directory failed. %d\n", rc);
	        return rc;
	    }

        set_chamber_flag(req.engine_id, inode);
	    break;
	}

	case ECRYPTFS_IOCTL_REMOVE_CHAMBER: {
        if (!S_ISDIR(ecryptfs_dentry->d_inode->i_mode)) {
            DEK_LOGE("Not a directory\n");
            return -ENOTDIR;
        }

        if(!is_current_epmd()) {
            //DEK_LOGE("only epmd can call this");
            DEK_LOGE("Permission denied");
            return -EACCES;
        }

        if(!IS_UNDER_ROOT(ecryptfs_dentry)) {
            DEK_LOGE("Chamber has to be under root directory");
            return -EFAULT;
        }

        if(!is_chamber_directory(mount_crypt_stat, ecryptfs_dentry->d_name.name, NULL)) {
            DEK_LOGE("Not a chamber directory [%s]\n", ecryptfs_dentry->d_name.name);

            clr_chamber_flag(inode);
            return 0;
        }

        del_chamber_directory(mount_crypt_stat, ecryptfs_dentry->d_name.name);
        clr_chamber_flag(inode);
	    break;
	}

	default: {
		return -EOPNOTSUPP;
		break;
		}

	}
	return 0;
}
