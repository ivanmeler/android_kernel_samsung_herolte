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
#include <linux/smc.h>
#include <ufshcd.h>
#include <ufs-exynos.h>

#if defined(CONFIG_UFS_FMP_ECRYPT_FS)
#include <linux/pagemap.h>
#include "fmp_derive_iv.h"
#endif

#if defined(CONFIG_FIPS_FMP)
#include "fmpdev_info.h"
#endif

#include "fmpdev_int.h" //For FIPS_FMP_FUNC_TEST macro

extern bool in_fmp_fips_err(void);
extern volatile unsigned int disk_key_flag;
extern spinlock_t disk_key_lock;
extern void exynos_ufs_ctrl_hci_core_clk(struct exynos_ufs *ufs, bool en);

#define byte2word(b0, b1, b2, b3) 	\
		((unsigned int)(b0) << 24) | ((unsigned int)(b1) << 16) | ((unsigned int)(b2) << 8) | (b3)
#define word_in(x, c)           byte2word(((unsigned char *)(x) + 4 * (c))[0], ((unsigned char *)(x) + 4 * (c))[1], \
					((unsigned char *)(x) + 4 * (c))[2], ((unsigned char *)(x) + 4 * (c))[3])

#define FMP_KEY_SIZE	32

static int fmp_xts_check_key(uint8_t *enckey, uint8_t *twkey, uint32_t len)
{
	if (!enckey | !twkey | !len) {
		printk(KERN_ERR "FMP key buffer is NULL or length is 0.\n");
		return -1;
	}

	if (!memcmp(enckey, twkey, len))
		return -1;      /* enckey and twkey are same */
	else
		return 0;       /* enckey and twkey are different */
}

int fmp_map_sg(struct ufshcd_sg_entry *prd_table, struct scatterlist *sg,
					uint32_t sector_key, uint32_t idx,
					uint32_t sector, struct bio *bio)
{
#if defined(CONFIG_FIPS_FMP)
	if (unlikely(in_fmp_fips_err())) {
		printk(KERN_ERR "Fail to work fmp due to fips in error\n");
		return -EPERM;
	}
#endif

#if defined(CONFIG_UFS_FMP_DM_CRYPT)
	/* Disk Encryption */
	if (sector_key & UFS_ENCRYPTION_SECTOR_BEGIN) {
		/* algorithm */
#if defined(CONFIG_FIPS_FMP)
		if (prd_table[idx].size > 0x1000) {
			printk(KERN_ERR "Fail to FMP XTS due to invalid size \
					for Disk Encrytion. size = 0x%x\n", prd_table[idx].size);
			return -EINVAL;
		}
#endif
		SET_DAS(&prd_table[idx], AES_XTS);
		prd_table[idx].size |= DKL;

		/* disk IV */
		prd_table[idx].disk_iv0 = 0;
		prd_table[idx].disk_iv1 = 0;
		prd_table[idx].disk_iv2 = 0;
		prd_table[idx].disk_iv3 = htonl(sector);

		if (disk_key_flag) {
			int ret;

			if (fmp_xts_check_key(bio->disk_key, (uint8_t *)((uint64_t)bio->disk_key + FMP_KEY_SIZE), FMP_KEY_SIZE)) {
				printk(KERN_ERR "Fail to FMP XTS because enckey and twkey is the same\n");
				return -EINVAL;
			}

			if (disk_key_flag == 1)
				printk(KERN_INFO "FMP disk encryption key is set\n");
			else if (disk_key_flag == 2)
				printk(KERN_INFO "FMP disk encryption key is set after clear\n");
			ret = exynos_smc(SMC_CMD_FMP, FMP_KEY_SET, UFS_FMP, 0);
			if (ret < 0)
				panic("Fail to load FMP loadable firmware\n");
			else if (ret) {
				printk(KERN_ERR "Fail to smc call for FMP key setting. ret = 0x%x\n", ret);
				return ret;
			}

			spin_lock(&disk_key_lock);
			disk_key_flag = 0;
			spin_unlock(&disk_key_lock);
		}
	}
#endif

#if defined(CONFIG_UFS_FMP_ECRYPT_FS)
	/* File Encryption */
	if (sector_key & UFS_FILE_ENCRYPTION_SECTOR_BEGIN) {
		int ret;
		unsigned int aes_alg, j;
		loff_t index;
#ifdef CONFIG_CRYPTO_FIPS
		char extent_iv[SHA256_HASH_SIZE];
#else
		char extent_iv[MD5_DIGEST_SIZE];
#endif

		/* algorithm */
		if (!strncmp(sg_page(sg)->mapping->alg, "aes", sizeof("aes")))
			aes_alg = AES_CBC;
		else if (!strncmp(sg_page(sg)->mapping->alg, "aesxts", sizeof("aesxts"))) {
			aes_alg = AES_XTS;
#if defined(CONFIG_FIPS_FMP)
			if (prd_table[idx].size > 0x1000) {
				printk(KERN_ERR "Fail to FMP XTS due to invalid size \
						for File Encrytion. size = 0x%x\n", prd_table[idx].size);
				return -EINVAL;
			}
#endif
		} else {
			printk(KERN_ERR "Invalid file encryption algorithm %s \n", sg_page(sg)->mapping->alg);
			return -EINVAL;
		}
		SET_FAS(&prd_table[idx], aes_alg);

		/* file encryption key size */
		switch (sg_page(sg)->mapping->key_length) {
		case 16:
			prd_table[idx].size &= ~FKL;
			break;
		case 32:
		case 64:
			prd_table[idx].size |= FKL;
			break;
		default:
			printk(KERN_ERR "Invalid file key length %x \n",
							(unsigned int)sg_page(sg)->mapping->key_length);
			return -EINVAL;
		}

		index = sg_page(sg)->index;
		index = index - sg_page(sg)->mapping->sensitive_data_index;
		ret = file_enc_derive_iv(sg_page(sg)->mapping, index, extent_iv);
		if (ret) {
			printk(KERN_ERR "Error attemping to derive IV. ret = %c\n", ret);
			return -EINVAL;
		}

		/* File IV */
		prd_table[idx].file_iv0 = word_in(extent_iv, 3);
		prd_table[idx].file_iv1 = word_in(extent_iv, 2);
		prd_table[idx].file_iv2 = word_in(extent_iv, 1);
		prd_table[idx].file_iv3 = word_in(extent_iv, 0);

		/* File Enc key */
		for (j = 0; j < sg_page(sg)->mapping->key_length >> 2; j++)
			*(&prd_table[idx].file_enckey0 + j) =
				word_in(sg_page(sg)->mapping->key, (sg_page(sg)->mapping->key_length >> 2) - (j + 1));
#if defined(CONFIG_FIPS_FMP)
		if (!strncmp(sg_page(sg)->mapping->alg, "aesxts", sizeof("aesxts"))) {
			if (fmp_xts_check_key((uint8_t *)&prd_table[idx].file_enckey0, (uint8_t *)&prd_table[idx].file_twkey0,
											sg_page(sg)->mapping->key_length)) {
				printk(KERN_ERR "Fail to FMP XTS because enckey and twkey is the same\n");
				return -EINVAL;
			}
		}
#endif
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(fmp_map_sg);

#if defined(CONFIG_FIPS_FMP)
int fmp_map_sg_st(struct ufs_hba *hba, struct ufshcd_sg_entry *prd_table,
					struct scatterlist *sg, uint32_t sector_key,
					uint32_t idx, uint32_t sector)
{
	struct ufshcd_sg_entry *prd_table_st = hba->ucd_prdt_ptr_st;

	if (sector_key == UFS_BYPASS_SECTOR_BEGIN) {
		SET_FAS(&prd_table[idx], CLEAR);
		SET_DAS(&prd_table[idx], CLEAR);
		return 0;
	}

	/* algorithm */
	if (hba->self_test_mode == XTS_MODE) {
		if (prd_table[idx].size > 0x1000) {
			printk(KERN_ERR "Fail to FMP XTS due to invalid size \
					for File Encrytion. size = 0x%x\n", prd_table[idx].size);
			return -EINVAL;
		}

		if (fmp_xts_check_key((uint8_t *)&prd_table_st->file_enckey0,
							(uint8_t *)&prd_table_st->file_twkey0, prd_table_st->size)) {
			printk(KERN_ERR "Fail to FMP XTS because enckey and twkey is the same\n");
			return -EINVAL;
		}

		SET_FAS(&prd_table[idx], AES_XTS);
	} else if (hba->self_test_mode == CBC_MODE)
		SET_FAS(&prd_table[idx], AES_CBC);
	else {
		printk(KERN_ERR "Invalid algorithm for FMP FIPS validation. mode = %d\n", hba->self_test_mode);
		return -EINVAL;
	}

	if (prd_table_st->size == 32)
		prd_table[idx].size |= FKL;

	/* File IV */
	prd_table[idx].file_iv0 = prd_table_st->file_iv0;
	prd_table[idx].file_iv1 = prd_table_st->file_iv1;
	prd_table[idx].file_iv2 = prd_table_st->file_iv2;
	prd_table[idx].file_iv3 = prd_table_st->file_iv3;

	/* enc key */
	prd_table[idx].file_enckey0 = prd_table_st->file_enckey0;
	prd_table[idx].file_enckey1 = prd_table_st->file_enckey1;
	prd_table[idx].file_enckey2 = prd_table_st->file_enckey2;
	prd_table[idx].file_enckey3 = prd_table_st->file_enckey3;
	prd_table[idx].file_enckey4 = prd_table_st->file_enckey4;
	prd_table[idx].file_enckey5 = prd_table_st->file_enckey5;
	prd_table[idx].file_enckey6 = prd_table_st->file_enckey6;
	prd_table[idx].file_enckey7 = prd_table_st->file_enckey7;

	/* tweak key */
	prd_table[idx].file_twkey0 = prd_table_st->file_twkey0;
	prd_table[idx].file_twkey1 = prd_table_st->file_twkey1;
	prd_table[idx].file_twkey2 = prd_table_st->file_twkey2;
	prd_table[idx].file_twkey3 = prd_table_st->file_twkey3;
	prd_table[idx].file_twkey4 = prd_table_st->file_twkey4;
	prd_table[idx].file_twkey5 = prd_table_st->file_twkey5;
	prd_table[idx].file_twkey6 = prd_table_st->file_twkey6;
	prd_table[idx].file_twkey7 = prd_table_st->file_twkey7;

	return 0;
}
EXPORT_SYMBOL_GPL(fmp_map_sg_st);
#endif

#if defined(CONFIG_UFS_FMP_ECRYPT_FS)
void fmp_clear_sg(struct ufshcd_lrb *lrbp)
{
	struct scatterlist *sg;
	int sg_segments;
	struct ufshcd_sg_entry *prd_table;
	struct scsi_cmnd *cmd = lrbp->cmd;
	int i;

	sg_segments = scsi_sg_count(cmd);
	if (sg_segments) {
		prd_table = (struct ufshcd_sg_entry *)lrbp->ucd_prdt_ptr;
		scsi_for_each_sg(cmd, sg, sg_segments, i) {
			if (!((unsigned long)(sg_page(sg)->mapping) & 0x1) && \
					sg_page(sg)->mapping && sg_page(sg)->mapping->key \
					&& ((unsigned int)(sg_page(sg)->index) >= 2)) {
				#if FIPS_FMP_FUNC_TEST == 6 //Key Zeroization
				print_hex_dump (KERN_ERR, "FIPS FMP descriptor before zeroize:", DUMP_PREFIX_NONE, 16, 1, &prd_table[i].file_iv0, sizeof(__le32)*20, false  );
				#endif

				memset(&prd_table[i].file_iv0, 0x0, sizeof(__le32)*20);

				#if FIPS_FMP_FUNC_TEST == 6 //Key Zeroization
				print_hex_dump (KERN_ERR, "FIPS FMP descriptor after zeroize:", DUMP_PREFIX_NONE, 16, 1, &prd_table[i].file_iv0, sizeof(__le32)*20, false  );
				#endif

			}
		}
	}

	return;
}
#endif

#if defined(CONFIG_UFS_FMP_DM_CRYPT) && defined(CONFIG_FIPS_FMP)
int fmp_clear_disk_key(void)
{
	struct device_node *dev_node;
	struct platform_device *pdev;
	struct device *dev;
	struct ufs_hba *hba;

	dev_node = of_find_compatible_node(NULL, NULL, "samsung,exynos-ufs");
	if (!dev_node) {
		printk(KERN_ERR "Fail to find exynos ufs device node\n");
		return -ENODEV;
	}

	pdev = of_find_device_by_node(dev_node);
	if (!pdev) {
		printk(KERN_ERR "Fail to find exynos ufs pdev\n");
		return -ENODEV;
	}

	dev = &pdev->dev;
	hba = dev_get_drvdata(dev);
	if (!hba) {
		printk(KERN_ERR "Fail to find hba from dev\n");
		return -ENODEV;
	}

	spin_lock(&disk_key_lock);
	disk_key_flag = 2;
	spin_unlock(&disk_key_lock);

	printk(KERN_INFO "FMP disk encryption key is clear\n");
	exynos_ufs_ctrl_hci_core_clk(dev_get_platdata(hba->dev), false);
	return exynos_smc(SMC_CMD_FMP, FMP_KEY_CLEAR, 0, 0);
}


#endif
