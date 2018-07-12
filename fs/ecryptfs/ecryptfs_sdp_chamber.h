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

#ifndef ECRYPTFS_SDP_CHAMBER_H_
#define ECRYPTFS_SDP_CHAMBER_H_

#include "ecryptfs_kernel.h"

int add_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		int engine_id, const unsigned char *path);
void del_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
        const unsigned char *path);
int is_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		const unsigned char *path, int *engineid);

void set_chamber_flag(int engineid, struct inode *inode);
void clr_chamber_flag(struct inode *inode);

#define IS_UNDER_ROOT(dentry) (dentry->d_parent->d_inode == dentry->d_sb->s_root->d_inode)
#define IS_CHAMBER_DENTRY(dentry) (ecryptfs_inode_to_private(dentry->d_inode)->crypt_stat.flags & ECRYPTFS_SDP_IS_CHAMBER_DIR)
#define IS_SENSITIVE_DENTRY(dentry) (ecryptfs_inode_to_private(dentry->d_inode)->crypt_stat.flags & ECRYPTFS_DEK_IS_SENSITIVE)

#endif /* ECRYPTFS_SDP_CHAMBER_H_ */
