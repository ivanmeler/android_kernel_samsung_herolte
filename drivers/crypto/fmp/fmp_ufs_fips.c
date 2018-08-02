/*
 * Exynos FMP UFS driver for FIPS
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/buffer_head.h>
#include <linux/mtd/mtd.h>
#include <linux/kdev_t.h>
#include <linux/smc.h>

#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>

#include <ufshcd.h>

#include "fmpdev_int.h"
#include "../scsi_priv.h"

#if defined(CONFIG_UFS_FMP_ECRYPT_FS)
#include "fmp_derive_iv.h"
#endif
#if defined(CONFIG_FIPS_FMP)
#include "fmpdev_info.h"
#endif

#define byte2word(b0, b1, b2, b3) 	\
		((unsigned int)(b0) << 24) | ((unsigned int)(b1) << 16) | ((unsigned int)(b2) << 8) | (b3)
#define get_word(x, c)	byte2word(((unsigned char *)(x) + 4 * (c))[0], ((unsigned char *)(x) + 4 * (c))[1], \
			((unsigned char *)(x) + 4 * (c))[2], ((unsigned char *)(x) + 4 * (c))[3])

#define SF_BLK_OFFSET	(5)
#define MAX_SCAN_PART	(50)

struct ufs_fmp_work {
	struct Scsi_Host *host;
	struct scsi_device *sdev;
	struct block_device *bdev;
	sector_t sector;
	dev_t devt;
};

struct ufshcd_sg_entry *prd_table;
struct ufshcd_sg_entry *ucd_prdt_ptr_st;

static int ufs_fmp_init(struct device *dev, uint32_t mode)
{
	struct ufs_hba *hba;
	struct ufs_fmp_work *work;
	struct Scsi_Host *host;

	work = dev_get_drvdata(dev);
	if (!work) {
		dev_err(dev, "Fail to get work from platform device\n");
		return -ENODEV;
	}
	host = work->host;
	hba = shost_priv(host);

	ucd_prdt_ptr_st = kmalloc(sizeof(struct ufshcd_sg_entry), GFP_KERNEL);
	if (!ucd_prdt_ptr_st) {
		dev_err(dev, "Fail to alloc prdt ptr for self test\n");
		return -ENOMEM;
	}
	hba->ucd_prdt_ptr_st = ucd_prdt_ptr_st;

	return 0;
}

static int ufs_fmp_set_key(struct device *dev, uint32_t mode, uint8_t *key, uint32_t key_len)
{
	struct ufs_hba *hba;
	struct ufs_fmp_work *work;
	struct Scsi_Host *host;
	struct ufshcd_sg_entry *prd_table;

	work = dev_get_drvdata(dev);
	if (!work) {
		dev_err(dev, "Fail to get work from platform device\n");
		return -ENODEV;
	}

	host = work->host;
	hba = shost_priv(host);
	if (!hba->ucd_prdt_ptr_st) {
		dev_err(dev, "prdt ptr for fips is not allocated\n");
		return -ENOMEM;
	}

	prd_table = hba->ucd_prdt_ptr_st;

	if (mode == CBC_MODE) {
		SET_FAS(prd_table, CBC_MODE);

		switch (key_len) {
		case 32:
			prd_table->size = 32;
			/* encrypt key */
			prd_table->file_enckey0 = get_word(key, 7);
			prd_table->file_enckey1 = get_word(key, 6);
			prd_table->file_enckey2 = get_word(key, 5);
			prd_table->file_enckey3 = get_word(key, 4);
			prd_table->file_enckey4 = get_word(key, 3);
			prd_table->file_enckey5 = get_word(key, 2);
			prd_table->file_enckey6 = get_word(key, 1);
			prd_table->file_enckey7 = get_word(key, 0);
			break;
		case 16:
			prd_table->size = 16;
			/* encrypt key */
			prd_table->file_enckey0 = get_word(key, 3);
			prd_table->file_enckey1 = get_word(key, 2);
			prd_table->file_enckey2 = get_word(key, 1);
			prd_table->file_enckey3 = get_word(key, 0);
			prd_table->file_enckey4 = 0;
			prd_table->file_enckey5 = 0;
			prd_table->file_enckey6 = 0;
			prd_table->file_enckey7 = 0;
			break;
		default:
			dev_err(dev, "Invalid key length : %d\n", key_len);
			return -EINVAL;
		}
	} else if (mode == XTS_MODE) {
		SET_FAS(prd_table, XTS_MODE);

		switch (key_len) {
		case 64:
			prd_table->size = 32;
			/* encrypt key */
			prd_table->file_enckey0 = get_word(key, 7);
			prd_table->file_enckey1 = get_word(key, 6);
			prd_table->file_enckey2 = get_word(key, 5);
			prd_table->file_enckey3 = get_word(key, 4);
			prd_table->file_enckey4 = get_word(key, 3);
			prd_table->file_enckey5 = get_word(key, 2);
			prd_table->file_enckey6 = get_word(key, 1);
			prd_table->file_enckey7 = get_word(key, 0);

			/* tweak key */
			prd_table->file_twkey0 = get_word(key, 15);
			prd_table->file_twkey1 = get_word(key, 14);
			prd_table->file_twkey2 = get_word(key, 13);
			prd_table->file_twkey3 = get_word(key, 12);
			prd_table->file_twkey4 = get_word(key, 11);
			prd_table->file_twkey5 = get_word(key, 10);
			prd_table->file_twkey6 = get_word(key, 9);
			prd_table->file_twkey7 = get_word(key, 8);
			break;
		case 32:
			prd_table->size = 16;
			/* encrypt key */
			prd_table->file_enckey0 = get_word(key, 3);
			prd_table->file_enckey1 = get_word(key, 2);
			prd_table->file_enckey2 = get_word(key, 1);
			prd_table->file_enckey3 = get_word(key, 0);
			prd_table->file_enckey4 = 0;
			prd_table->file_enckey5 = 0;
			prd_table->file_enckey6 = 0;
			prd_table->file_enckey7 = 0;

			/* tweak key */
			prd_table->file_twkey0 = get_word(key, 7);
			prd_table->file_twkey1 = get_word(key, 6);
			prd_table->file_twkey2 = get_word(key, 5);
			prd_table->file_twkey3 = get_word(key, 4);
			prd_table->file_twkey4 = 0;
			prd_table->file_twkey5 = 0;
			prd_table->file_twkey6 = 0;
			prd_table->file_twkey7 = 0;
			break;
		default:
			dev_err(dev, "Invalid key length : %d\n", key_len);
			return -EINVAL;
		}
	} else if (mode == BYPASS_MODE) {
		SET_FAS(prd_table, BYPASS_MODE);

		/* enc key */
		prd_table->file_enckey0 = 0;
		prd_table->file_enckey1 = 0;
		prd_table->file_enckey2 = 0;
		prd_table->file_enckey3 = 0;
		prd_table->file_enckey4 = 0;
		prd_table->file_enckey5 = 0;
		prd_table->file_enckey6 = 0;
		prd_table->file_enckey7 = 0;

		/* tweak key */
		prd_table->file_twkey0 = 0;
		prd_table->file_twkey1 = 0;
		prd_table->file_twkey2 = 0;
		prd_table->file_twkey3 = 0;
		prd_table->file_twkey4 = 0;
		prd_table->file_twkey5 = 0;
		prd_table->file_twkey6 = 0;
		prd_table->file_twkey7 = 0;
	} else {
		dev_err(dev, "Invalid mode : %d\n", mode);
		return -EINVAL;
	}

	dev_info(dev, "%s: ufs fmp key set is done.\n", __func__);
	return 0;
}

static int ufs_fmp_set_iv(struct device *dev, uint32_t mode, uint8_t *iv, uint32_t iv_len)
{
	struct ufs_hba *hba;
	struct ufs_fmp_work *work;
	struct Scsi_Host *host;
	struct ufshcd_sg_entry *prd_table;

	work = dev_get_drvdata(dev);
	if (!work) {
		dev_err(dev, "Fail to get work from platform device\n");
		return -ENODEV;
	}

	host = work->host;
	hba = shost_priv(host);
	if (!hba->ucd_prdt_ptr_st) {
		dev_err(dev, "prdt ptr for fips is not allocated\n");
		return -ENOMEM;
	}

	prd_table = hba->ucd_prdt_ptr_st;

	if (mode == CBC_MODE || mode == XTS_MODE) {
		prd_table->file_iv0 = get_word(iv, 3);
		prd_table->file_iv1 = get_word(iv, 2);
		prd_table->file_iv2 = get_word(iv, 1);
		prd_table->file_iv3 = get_word(iv, 0);
	} else if (mode == BYPASS_MODE) {
		prd_table->file_iv0 = 0;
		prd_table->file_iv1 = 0;
		prd_table->file_iv2 = 0;
		prd_table->file_iv3 = 0;
	} else {
		dev_err(dev, "Invalid mode : %d\n", mode);
		return -EINVAL;
	}

	return 0;
}

static dev_t find_devt_for_selftest(struct device *dev, struct Scsi_Host *host)
{
	int i, idx = 0;
	uint32_t count = 0;
	uint64_t size;
	uint64_t size_list[MAX_SCAN_PART];
	dev_t devt_list[MAX_SCAN_PART];
	dev_t devt_scan, devt;
	struct block_device *bdev;
	fmode_t fmode = FMODE_WRITE | FMODE_READ;

	memset(size_list, 0, sizeof(size_list));
	memset(devt_list, 0, sizeof(devt_list));

	do {
		if (!host->async_scan) {
			dev_info(dev, "scsi scan is already done for FMP self-test\n");
			break;
		} else {
			mdelay(100);
			count++;
		}
	} while (count < 1000);

	if (count == 1000) {
		dev_err(dev, "scsi scan is not done yet. FMP goes to error state\n");
		return 0;
	}
	count = 0;

	do {
		for (i = 1; i < MAX_SCAN_PART; i++) {
			devt_scan = blk_lookup_devt("sda", i);
			bdev = blkdev_get_by_dev(devt_scan, fmode, NULL);
			if (IS_ERR(bdev))
				continue;
			else {
				size_list[idx] = (uint64_t)i_size_read(bdev->bd_inode);
				devt_list[idx++] = devt_scan;
			}
		}

		if (!idx) {
			mdelay(100);
			count++;
			continue;
		}

		for (i = 0; i < idx; i++) {
			if (i == 0) {
				size = size_list[i];
				devt = devt_list[i];
			} else {
				if (size < size_list[i])
					devt = devt_list[i];
			}
		}

		bdev = blkdev_get_by_dev(devt, fmode, NULL);
		dev_info(dev, "FMP fips driver found sda%d for self-test\n", bdev->bd_part->partno);

		return devt;
	} while (count < 100);

	dev_err(dev, "SCSI disk isn't initialized yet. It makes to fail FMP selftest\n");
	return (dev_t)0;
}

static int ufs_fmp_run(struct device *dev, uint32_t mode, uint8_t *data,
			uint32_t len, uint32_t write)
{
	int ret = 0;
	struct ufs_hba *hba;
	struct ufs_fmp_work *work;
	struct Scsi_Host *host;
	static struct buffer_head *bh;

	work = dev_get_drvdata(dev);
	if (!work) {
		dev_err(dev, "Fail to get work from platform device\n");
		return -ENODEV;
	}
	host = work->host;
	hba = shost_priv(host);
	hba->self_test_mode = mode;

	bh = __getblk(work->bdev, work->sector, FMP_BLK_SIZE);
	if (!bh) {
		dev_err(dev, "Fail to get block from bdev\n");
		return -ENODEV;
	}
	hba->self_test_bh = bh;

	get_bh(bh);
	if (write == WRITE_MODE) {
		memcpy(bh->b_data, data, len);
		set_buffer_dirty(bh);
		sync_dirty_buffer(bh);
		if (buffer_req(bh) && !buffer_uptodate(bh)) {
			dev_err(dev, "IO error syncing for FMP fips write\n");
			ret = -EIO;
			goto out;
		}
		memset(bh->b_data, 0, FMP_BLK_SIZE);
	} else {
		lock_buffer(bh);
		bh->b_end_io = end_buffer_read_sync;
		submit_bh(READ_SYNC, bh);
		wait_on_buffer(bh);
		if (unlikely(!buffer_uptodate(bh))) {
			ret = -EIO;
			goto out;
		}
		memcpy(data, bh->b_data, len);
	}
out:
	hba->self_test_mode = 0;
	hba->self_test_bh = NULL;
	put_bh(bh);

	return ret;
}

static int ufs_fmp_exit(void)
{
	if (ucd_prdt_ptr_st)
		kfree(ucd_prdt_ptr_st);

	return 0;
}

struct fips_fmp_ops fips_fmp_fops = {
	.init = ufs_fmp_init,
	.set_key = ufs_fmp_set_key,
	.set_iv = ufs_fmp_set_iv,
	.run = ufs_fmp_run,
	.exit = ufs_fmp_exit,
};

int fips_fmp_init(struct device *dev)
{
	struct ufs_fmp_work *work;
	struct device_node *dev_node;
	struct platform_device *pdev_ufs;
	struct device *dev_ufs;
	struct ufs_hba *hba;
	struct Scsi_Host *host;
	struct inode *inode;
	struct scsi_device *sdev;
	struct super_block *sb;
	unsigned long blocksize;
	unsigned char blocksize_bits;

	sector_t self_test_block;
	fmode_t fmode = FMODE_WRITE | FMODE_READ;

	work = kmalloc(sizeof(*work), GFP_KERNEL);
	if (!work) {
		dev_err(dev, "Fail to alloc fmp work buffer\n");
		return -ENOMEM;
	}

	dev_node = of_find_compatible_node(NULL, NULL, "samsung,exynos-ufs");
	if (!dev_node) {
		dev_err(dev, "Fail to find exynos ufs device node\n");
		goto out;
	}

	pdev_ufs = of_find_device_by_node(dev_node);
	if (!pdev_ufs) {
		dev_err(dev, "Fail to find exynos ufs pdev\n");
		goto out;
	}

	dev_ufs = &pdev_ufs->dev;
	hba = dev_get_drvdata(dev_ufs);
	if (!hba) {
		dev_err(dev, "Fail to find hba from dev\n");
		goto out;
	}

	host = hba->host;
	sdev = to_scsi_device(dev_ufs);
	work->host = host;
	work->sdev = sdev;

	work->devt = find_devt_for_selftest(dev, host);
	if (!work->devt) {
		dev_err(dev, "Fail to find devt for self test\n");
		return -ENODEV;
	}

	work->bdev = blkdev_get_by_dev(work->devt, fmode, NULL);
	if (IS_ERR(work->bdev)) {
		dev_err(dev, "Fail to open block device\n");
		return -ENODEV;
	}
	inode = work->bdev->bd_inode;
	sb = inode->i_sb;
	blocksize = sb->s_blocksize;
	blocksize_bits = sb->s_blocksize_bits;
	self_test_block = (i_size_read(inode) - (blocksize * SF_BLK_OFFSET)) >> blocksize_bits;
	work->sector = self_test_block;

	dev_set_drvdata(dev, work);

	return 0;

out:
	if (work)
		kfree(work);

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(fips_fmp_init);
