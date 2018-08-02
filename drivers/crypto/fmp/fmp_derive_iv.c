/*
 * Exynos FMP derive iv for eCryptfs
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/fs.h>

#define FMP_MAX_IV_BYTES		16
#define FMP_MAX_OFFSET_BYTES		16
#define DEFAULT_HASH			"md5"
#define SHA256_HASH			"sha256-fmp"

static DEFINE_SPINLOCK(fmp_tfm_lock);

int calculate_sha256(struct crypto_hash *hash_tfm, char *dst, char *src, int len)
{
	int ret = -1;
	unsigned long flag;
	struct scatterlist sg;
	struct hash_desc desc = {
		.tfm = hash_tfm,
		.flags = 0
	};

	spin_lock_irqsave(&fmp_tfm_lock, flag);
	sg_init_one(&sg, (u8 *)src, len);

	if (!desc.tfm) {
		desc.tfm = crypto_alloc_hash(SHA256_HASH, 0,
					     CRYPTO_ALG_ASYNC);
		if (IS_ERR(desc.tfm)) {
			printk(KERN_ERR "%s: Error attemping to allocate crypto context\n", __func__);
			goto out;
		}
		hash_tfm = desc.tfm;
	}

	ret = crypto_hash_init(&desc);
	if (ret) {
		printk(KERN_ERR
		       "%s: Error initializing crypto hash; ret = [%d]\n",
		       __func__, ret);
		goto out;
	}
	ret = crypto_hash_update(&desc, &sg, len);
	if (ret) {
		printk(KERN_ERR
		       "%s: Error updating crypto hash; ret = [%d]\n",
		       __func__, ret);
		goto out;
	}
	ret = crypto_hash_final(&desc, dst);
	if (ret) {
		printk(KERN_ERR
		       "%s: Error finalizing crypto hash; ret = [%d]\n",
		       __func__, ret);
		goto out;
	}

out:
	spin_unlock_irqrestore(&fmp_tfm_lock, flag);
	return ret;
}

int calculate_md5(struct crypto_hash *hash_tfm, char *dst, char *src, int len)
{
	int ret = -1;
	unsigned long flag;
	struct scatterlist sg;
	struct hash_desc desc = {
		.tfm = hash_tfm,
		.flags = 0
	};

	spin_lock_irqsave(&fmp_tfm_lock, flag);
	sg_init_one(&sg, (u8 *)src, len);

	if (!desc.tfm) {
		desc.tfm = crypto_alloc_hash(DEFAULT_HASH, 0,
					     CRYPTO_ALG_ASYNC);
		if (IS_ERR(desc.tfm)) {
			printk(KERN_ERR "%s: Error attemping to allocate crypto context\n", __func__);
			goto out;
		}
		hash_tfm = desc.tfm;
	}

	ret = crypto_hash_init(&desc);
	if (ret) {
		printk(KERN_ERR
		       "%s: Error initializing crypto hash; ret = [%d]\n",
		       __func__, ret);
		goto out;
	}
	ret = crypto_hash_update(&desc, &sg, len);
	if (ret) {
		printk(KERN_ERR
		       "%s: Error updating crypto hash; ret = [%d]\n",
		       __func__, ret);
		goto out;
	}
	ret = crypto_hash_final(&desc, dst);
	if (ret) {
		printk(KERN_ERR
		       "%s: Error finalizing crypto hash; ret = [%d]\n",
		       __func__, ret);
		goto out;
	}

out:
	spin_unlock_irqrestore(&fmp_tfm_lock, flag);
	return ret;
}

int file_enc_derive_iv(struct address_space *mapping, loff_t offset, char *extent_iv)
{
	char src[FMP_MAX_IV_BYTES + FMP_MAX_OFFSET_BYTES];
	int ret;

	memcpy(src, mapping->iv, FMP_MAX_IV_BYTES);
	memset(src + FMP_MAX_IV_BYTES, 0, FMP_MAX_OFFSET_BYTES);
	snprintf(src + FMP_MAX_IV_BYTES, FMP_MAX_OFFSET_BYTES, "%lld", offset);

#ifdef CONFIG_CRYPTO_FIPS
	if (mapping->cc_enable)
		ret = calculate_sha256(mapping->hash_tfm, extent_iv, src, FMP_MAX_IV_BYTES + FMP_MAX_OFFSET_BYTES);
	else
#endif
		ret = calculate_md5(mapping->hash_tfm, extent_iv, src, FMP_MAX_IV_BYTES + FMP_MAX_OFFSET_BYTES);
	if (ret) {
		printk(KERN_ERR "%s: Error attempting to compute generating IV for a page; ret = [%d]\n", __func__, ret);
		goto out;
	}

out:
	return ret;
}

