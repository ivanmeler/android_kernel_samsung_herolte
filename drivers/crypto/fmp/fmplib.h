/*
 * Exynos FMP library header
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __FMPLIB_H__
#define __FMPLIB_H__

int fmpdev_cipher_init(struct fmp_info *info, struct cipher_data *out,
			const char *alg_name,
			uint8_t *enckey, uint8_t *twkey, size_t keylen);
void fmpdev_cipher_deinit(struct cipher_data *cdata);
int fmpfw_hash_init(struct fmp_info *info, struct hash_data *hdata,
			const char *alg_name, int hmac_mode,
			void *mackey, size_t mackeylen);
void fmpfw_hash_deinit(struct hash_data *hdata);
int fmpdev_hash_init(struct fmp_info *info, struct hash_data *hdata,
			const char *alg_name,
			int hmac_mode, void *mackey, size_t mackeylen);
void fmpdev_hash_deinit(struct hash_data *hdata);
int fmpdev_hash_reset(struct fmp_info *info, struct hash_data *hdata);
ssize_t fmpdev_hash_update(struct hash_data *hdata,
				struct scatterlist *sg, size_t len);
int fmpdev_hash_final(struct hash_data *hdata, void *output);
int fmp_run(struct fmp_info *info, struct fcrypt *fcr, struct kernel_crypt_op *kcop);
int fmpdev_cipher_exit(struct fmp_info *info);
int fmp_run_AES_CBC_MCT(struct fmp_info *info, struct fcrypt *fcr,
				struct kernel_crypt_op *kcop);

#endif
