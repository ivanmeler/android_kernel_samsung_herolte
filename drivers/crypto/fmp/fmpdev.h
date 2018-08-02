/*
 * Exynos FMP test driver for FIPS
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _FIPS_FMP_H_
#define _FIPS_FMP_H_

#define FMP_DRV_VERSION "1.2"

int fips_fmp_cipher_init(struct device *dev, uint8_t *enckey, uint8_t *twkey, uint32_t key_len, uint32_t mode);
int fips_fmp_cipher_set_iv(struct device *dev, uint8_t *iv, uint32_t mode);
int fips_fmp_cipher_run(struct device *dev, uint8_t *src, uint8_t *dst, uint32_t len, uint32_t mode, uint32_t enc);
int fips_fmp_cipher_exit(struct device *dev);

#endif
