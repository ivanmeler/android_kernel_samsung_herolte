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
#include <linux/init.h>
#include <linux/sched.h>

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "ecryptfs_sdp_chamber.h"

#include <sdp/common.h>

#define CHAMBER_PATH_MAX 512
typedef struct __chamber_info {
	int partition_id;
    int engine_id;

	struct list_head list;
	char path[CHAMBER_PATH_MAX];
}chamber_info_t;

#define NO_DIRECTORY_SEPARATOR_IN_CHAMBER_PATH 1
/* Debug */
#define CHAMBER_DEBUG		0

#if CHAMBER_DEBUG
#define CHAMBER_LOGD(FMT, ...) printk("SDP_CHAMBER[%d] %s :: " FMT , current->pid, __func__, ##__VA_ARGS__)
#else
#define CHAMBER_LOGD(FMT, ...)
#endif /* PUB_CRYPTO_DEBUG */
#define CHAMBER_LOGE(FMT, ...) printk("SDP_CHAMBER[%d] %s :: " FMT , current->pid, __func__, ##__VA_ARGS__)


chamber_info_t *alloc_chamber_info(int partition_id, int engine_id, const unsigned char *path) {
	chamber_info_t *new_chamber = kmalloc(sizeof(chamber_info_t), GFP_KERNEL);

	if(new_chamber == NULL) {
		CHAMBER_LOGE("can't alloc memory for chamber_info\n");
		return NULL;
	}

    new_chamber->partition_id = partition_id;
    new_chamber->engine_id = engine_id;
	snprintf(new_chamber->path, CHAMBER_PATH_MAX, "%s", path);

	return new_chamber;
}

int add_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		int engine_id, const unsigned char *path) {
	chamber_info_t *new_chamber = NULL;

#if NO_DIRECTORY_SEPARATOR_IN_CHAMBER_PATH
	if(strchr(path, '/') != NULL) {
		CHAMBER_LOGE("Chamber directory cannot contain '/'\n");
		return -EINVAL;
	}
#endif

	new_chamber = alloc_chamber_info(mount_crypt_stat->partition_id, engine_id, path);

	if(new_chamber == NULL) {
		return -ENOMEM;
	}

	spin_lock(&(mount_crypt_stat->chamber_dir_list_lock));
	CHAMBER_LOGD("Adding %s into chamber list\n", new_chamber->path);
	list_add_tail(&new_chamber->list, &(mount_crypt_stat->chamber_dir_list));
	spin_unlock(&(mount_crypt_stat->chamber_dir_list_lock));

	return 0;
}

chamber_info_t *find_chamber_info(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		const unsigned char *path) {
	struct list_head *entry;

	spin_lock(&(mount_crypt_stat->chamber_dir_list_lock));

	CHAMBER_LOGD("%s\n", path);

	list_for_each(entry, &mount_crypt_stat->chamber_dir_list) {
		chamber_info_t *info;
		info = list_entry(entry, chamber_info_t, list);

		// Check path
		if(!strncmp(path, info->path, CHAMBER_PATH_MAX)) {
			CHAMBER_LOGD("Found %s from chamber list\n", info->path);

			spin_unlock(&(mount_crypt_stat->chamber_dir_list_lock));
			return info;
		}
	}

	spin_unlock(&(mount_crypt_stat->chamber_dir_list_lock));
	CHAMBER_LOGD("Not found\n");

	return NULL;
}

void del_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
        const unsigned char *path) {
	chamber_info_t *info = find_chamber_info(mount_crypt_stat, path);
	if(info == NULL) {
		CHAMBER_LOGD("nothing to remove\n");
		return;
	}

	spin_lock(&mount_crypt_stat->chamber_dir_list_lock);
	CHAMBER_LOGD("%s removed\n", info->path);
	list_del(&info->list);

	kfree(info);
	spin_unlock(&mount_crypt_stat->chamber_dir_list_lock);
}

int is_chamber_directory(struct ecryptfs_mount_crypt_stat *mount_crypt_stat,
		const unsigned char *path, int *engineid) {
    chamber_info_t *info;
#if NO_DIRECTORY_SEPARATOR_IN_CHAMBER_PATH
	if(strchr(path, '/') != NULL) {
		CHAMBER_LOGD("%s containes '/'\n", path);
		return 0;
	}
#endif

	info = find_chamber_info(mount_crypt_stat, path);
	if(info == NULL)
		return 0;

	if(engineid) *engineid = info->engine_id;

	return 1;
}

void set_chamber_flag(int engineid, struct inode *inode) {
	struct ecryptfs_crypt_stat *crypt_stat;

	if(inode == NULL) {
		CHAMBER_LOGE("invalid inode\n");
		return;
	}

	crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;

    crypt_stat->engine_id = engineid;
    crypt_stat->flags |= ECRYPTFS_SDP_IS_CHAMBER_DIR;
	crypt_stat->flags |= ECRYPTFS_DEK_IS_SENSITIVE;
}

void clr_chamber_flag(struct inode *inode) {
    struct ecryptfs_crypt_stat *crypt_stat;

    if(inode == NULL) {
        CHAMBER_LOGE("invalid inode\n");
        return;
    }

    crypt_stat = &ecryptfs_inode_to_private(inode)->crypt_stat;

    crypt_stat->engine_id = -1;
    crypt_stat->flags &= ~ECRYPTFS_DEK_IS_SENSITIVE;
    crypt_stat->flags &= ~ECRYPTFS_SDP_IS_CHAMBER_DIR;
}
