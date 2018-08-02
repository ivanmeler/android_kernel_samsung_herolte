/*
 * Exynos FMP derive iv for eCryptfs
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _FMP_DERIVE_IV_H_
#define _FMP_DERIVE_IV_H_

#define SHA256_HASH_SIZE	32
#define MD5_DIGEST_SIZE		16

int calculate_sha256(struct crypto_hash *hash_tfm, char *dst, char *src, int len);
int calculate_md5(struct crypto_hash *hash_tfm, char *dst, char *src, int len);
int file_enc_derive_iv(struct address_space *mapping, loff_t offset, char *extent_iv);

#endif
