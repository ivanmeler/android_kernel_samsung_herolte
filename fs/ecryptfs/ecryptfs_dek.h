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

#ifndef ECRYPTFS_DEK_H
#define ECRYPTFS_DEK_H

#include <linux/fs.h>
#include <sdp/dek_common.h>
#include "ecryptfs_kernel.h"

#define ECRYPTFS_DEK_XATTR_NAME "user.sdp"

#define ECRYPTFS_DEK_DEBUG 0

#define O_SDP    0x10000000

enum sdp_op {
	TO_SENSITIVE = 0,
	TO_PROTECTED
};

int ecryptfs_super_block_get_userid(struct super_block *sb);
int ecryptfs_is_sdp_locked(int engine_id);
void ecryptfs_clean_sdp_dek(struct ecryptfs_crypt_stat *crypt_stat);
int ecryptfs_get_sdp_dek(struct ecryptfs_crypt_stat *crypt_stat);
int ecryptfs_sdp_convert_dek(struct dentry *dentry);
int ecryptfs_parse_xattr_is_sensitive(const void *data, int len);

int write_dek_packet(char *dest, struct ecryptfs_crypt_stat *crypt_stat, size_t *written);
int parse_dek_packet(char *data, struct ecryptfs_crypt_stat *crypt_stat, size_t *packet_size);

long ecryptfs_do_sdp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int ecryptfs_sdp_set_sensitive(int engine_id, struct dentry *dentry);
int ecryptfs_sdp_set_protected(struct dentry *dentry);
void ecryptfs_set_mapping_sensitive(struct inode *ecryptfs_inode, int userid, enum sdp_op operation);

void ecryptfs_fs_request_callback(int opcode, int ret, unsigned long ino);

#define ECRYPTFS_EVT_RENAME_TO_CHAMBER      1
#define ECRYPTFS_EVT_RENAME_OUT_OF_CHAMBER  2

/*
 * ioctl for SDP
 */

typedef struct _dek_arg_sdp_info {
    int engine_id;
	int sdp_enabled;	
	int is_sensitive;
	int is_chamber;
	unsigned int type;
}dek_arg_get_sdp_info;

typedef struct _dek_arg_set_sensitive {
	int engine_id;
}dek_arg_set_sensitive;

typedef struct _dek_arg_add_chamber {
    int engine_id;
}dek_arg_add_chamber;

#define ECRYPTFS_IOCTL_GET_SDP_INFO     _IOR('l', 0x11, __u32)
#define ECRYPTFS_IOCTL_SET_SENSITIVE    _IOW('l', 0x15, __u32)
#define ECRYPTFS_IOCTL_SET_PROTECTED    _IOW('l', 0x16, __u32)
#define ECRYPTFS_IOCTL_ADD_CHAMBER      _IOW('l', 0x17, __u32)
#define ECRYPTFS_IOCTL_REMOVE_CHAMBER   _IOW('l', 0x18, __u32)

#endif /* #ifndef ECRYPTFS_DEK_H */
