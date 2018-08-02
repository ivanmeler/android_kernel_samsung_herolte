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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/crypto.h>
#include <linux/sysfs.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <linux/rtnetlink.h>
#include <linux/smc.h>

#include <asm/byteorder.h>
#include <crypto/aes.h>
#include <crypto/hash.h>
#include <crypto/authenc.h>

#include "fmpdev_int.h"
#include "fmp_integrity.h"
#include "fmplib.h"

static int fips_fmp_result;
static const char pass[] = "passed";
static const char fail[] = "failed";

static const char *ALG_SHA256_FMP = "sha256-fmp";
static const char *ALG_HMAC_SHA256_FMP = "hmac-fmp(sha256-fmp)";

extern int fips_fmp_init(struct device *dev);
extern struct fips_fmp_ops fips_fmp_fops;
static const struct of_device_id exynos_fips_fmp_match[];

struct platform_device *pdev;

#define FMP_FIPS_INIT_STATE	0
#define FMP_FIPS_SELFTEST_STATE	1
#define FMP_FIPS_ERR_STATE	2
#define FMP_FIPS_SUCCESS_STATE	3

static int fmp_fips_state = FMP_FIPS_INIT_STATE;

bool in_fmp_fips_err(void)
{
	if (fmp_fips_state == FMP_FIPS_INIT_STATE || fmp_fips_state == FMP_FIPS_ERR_STATE)
		return true;
	return false;
}
EXPORT_SYMBOL_GPL(in_fmp_fips_err);

static void set_fmp_fips_state(uint32_t val)
{
	fmp_fips_state = val;
}

#define AES_XTS_ENC_TEST_VECTORS 5
#define AES_CBC_ENC_TEST_VECTORS 4

/*
 * Indexes into the xbuf to simulate cross-page access.
 */
#define IDX1            32
#define IDX2            32400
#define IDX3            1
#define IDX4            8193
#define IDX5            22222
#define IDX6            17101
#define IDX7            27333
#define IDX8            3000

static unsigned int IDX[8] = { IDX1, IDX2, IDX3, IDX4, IDX5, IDX6, IDX7, IDX8 };

/* ====== Compile-time config ====== */

/* Default (pre-allocated) and maximum size of the job queue.
 * These are free, pending and done items all together. */
#define DEF_COP_RINGSIZE 16
#define MAX_COP_RINGSIZE 64

static struct cipher_testvec aes_xts_enc_tv_template[] = {
	/* http://grouper.ieee.org/groups/1619/email/pdf00086.pdf */
	{ /* XTS-AES 1 */
#if FIPS_FMP_FUNC_TEST == 1
		.key    = "\xa2\xb9\x0c\xba\x3f\x06\xac\x35"
#else
		.key    = "\xa1\xb9\x0c\xba\x3f\x06\xac\x35"
#endif
			  "\x3b\x2c\x34\x38\x76\x08\x17\x62"
			  "\x09\x09\x23\x02\x6e\x91\x77\x18"
			  "\x15\xf2\x9d\xab\x01\x93\x2f\x2f",
		.klen   = 32,
		.iv     = "\x4f\xae\xf7\x11\x7c\xda\x59\xc6"
			  "\x6e\x4b\x92\x01\x3e\x76\x8a\xd5",
		.input  = "\xeb\xab\xce\x95\xb1\x4d\x3c\x8d"
			  "\x6f\xb3\x50\x39\x07\x90\x31\x1c",
		.ilen   = 16,
		.result = "\x77\x8a\xe8\xb4\x3c\xb9\x8d\x5a"
			  "\x82\x50\x81\xd5\xbe\x47\x1c\x63",
		.rlen   = 16,
	}, { /* XTS-AES 2 */
		.key    = "\x11\x11\x11\x11\x11\x11\x11\x11"
			  "\x11\x11\x11\x11\x11\x11\x11\x11"
			  "\x22\x22\x22\x22\x22\x22\x22\x22"
			  "\x22\x22\x22\x22\x22\x22\x22\x22",
		.klen   = 32,
		.iv     = "\x33\x33\x33\x33\x33\x00\x00\x00"
			  "\x00\x00\x00\x00\x00\x00\x00\x00",
		.input  = "\x44\x44\x44\x44\x44\x44\x44\x44"
			  "\x44\x44\x44\x44\x44\x44\x44\x44"
			  "\x44\x44\x44\x44\x44\x44\x44\x44"
			  "\x44\x44\x44\x44\x44\x44\x44\x44",
		.ilen   = 32,
		.result = "\xc4\x54\x18\x5e\x6a\x16\x93\x6e"
			  "\x39\x33\x40\x38\xac\xef\x83\x8b"
			  "\xfb\x18\x6f\xff\x74\x80\xad\xc4"
			  "\x28\x93\x82\xec\xd6\xd3\x94\xf0",
		.rlen   = 32,
	}, { /* XTS-AES 3 */
		.key    = "\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8"
			  "\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0"
			  "\x22\x22\x22\x22\x22\x22\x22\x22"
			  "\x22\x22\x22\x22\x22\x22\x22\x22",
		.klen   = 32,
		.iv     = "\x33\x33\x33\x33\x33\x00\x00\x00"
			  "\x00\x00\x00\x00\x00\x00\x00\x00",
		.input  = "\x44\x44\x44\x44\x44\x44\x44\x44"
			  "\x44\x44\x44\x44\x44\x44\x44\x44"
			  "\x44\x44\x44\x44\x44\x44\x44\x44"
			  "\x44\x44\x44\x44\x44\x44\x44\x44",
		.ilen   = 32,
		.result = "\xaf\x85\x33\x6b\x59\x7a\xfc\x1a"
			  "\x90\x0b\x2e\xb2\x1e\xc9\x49\xd2"
			  "\x92\xdf\x4c\x04\x7e\x0b\x21\x53"
			  "\x21\x86\xa5\x97\x1a\x22\x7a\x89",
		.rlen   = 32,
	}, { /* XTS-AES 4 */
		.key    = "\x27\x18\x28\x18\x28\x45\x90\x45"
			  "\x23\x53\x60\x28\x74\x71\x35\x26"
			  "\x31\x41\x59\x26\x53\x58\x97\x93"
			  "\x23\x84\x62\x64\x33\x83\x27\x95",
		.klen   = 32,
		.iv     = "\x00\x00\x00\x00\x00\x00\x00\x00"
			  "\x00\x00\x00\x00\x00\x00\x00\x00",
		.input  = "\x00\x01\x02\x03\x04\x05\x06\x07"
			  "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
			  "\x10\x11\x12\x13\x14\x15\x16\x17"
			  "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
			  "\x20\x21\x22\x23\x24\x25\x26\x27"
			  "\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
			  "\x30\x31\x32\x33\x34\x35\x36\x37"
			  "\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
			  "\x40\x41\x42\x43\x44\x45\x46\x47"
			  "\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
			  "\x50\x51\x52\x53\x54\x55\x56\x57"
			  "\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
			  "\x60\x61\x62\x63\x64\x65\x66\x67"
			  "\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
			  "\x70\x71\x72\x73\x74\x75\x76\x77"
			  "\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
			  "\x80\x81\x82\x83\x84\x85\x86\x87"
			  "\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
			  "\x90\x91\x92\x93\x94\x95\x96\x97"
			  "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
			  "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7"
			  "\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
			  "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7"
			  "\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
			  "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7"
			  "\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
			  "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7"
			  "\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
			  "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
			  "\xe8\xe9\xea\xeb\xec\xed\xee\xef"
			  "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
			  "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
			  "\x00\x01\x02\x03\x04\x05\x06\x07"
			  "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
			  "\x10\x11\x12\x13\x14\x15\x16\x17"
			  "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
			  "\x20\x21\x22\x23\x24\x25\x26\x27"
			  "\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
			  "\x30\x31\x32\x33\x34\x35\x36\x37"
			  "\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
			  "\x40\x41\x42\x43\x44\x45\x46\x47"
			  "\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
			  "\x50\x51\x52\x53\x54\x55\x56\x57"
			  "\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
			  "\x60\x61\x62\x63\x64\x65\x66\x67"
			  "\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
			  "\x70\x71\x72\x73\x74\x75\x76\x77"
			  "\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
			  "\x80\x81\x82\x83\x84\x85\x86\x87"
			  "\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
			  "\x90\x91\x92\x93\x94\x95\x96\x97"
			  "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
			  "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7"
			  "\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
			  "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7"
			  "\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
			  "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7"
			  "\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
			  "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7"
			  "\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
			  "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
			  "\xe8\xe9\xea\xeb\xec\xed\xee\xef"
			  "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
			  "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff",
		.ilen   = 512,
		.result = "\x27\xa7\x47\x9b\xef\xa1\xd4\x76"
			  "\x48\x9f\x30\x8c\xd4\xcf\xa6\xe2"
			  "\xa9\x6e\x4b\xbe\x32\x08\xff\x25"
			  "\x28\x7d\xd3\x81\x96\x16\xe8\x9c"
			  "\xc7\x8c\xf7\xf5\xe5\x43\x44\x5f"
			  "\x83\x33\xd8\xfa\x7f\x56\x00\x00"
			  "\x05\x27\x9f\xa5\xd8\xb5\xe4\xad"
			  "\x40\xe7\x36\xdd\xb4\xd3\x54\x12"
			  "\x32\x80\x63\xfd\x2a\xab\x53\xe5"
			  "\xea\x1e\x0a\x9f\x33\x25\x00\xa5"
			  "\xdf\x94\x87\xd0\x7a\x5c\x92\xcc"
			  "\x51\x2c\x88\x66\xc7\xe8\x60\xce"
			  "\x93\xfd\xf1\x66\xa2\x49\x12\xb4"
			  "\x22\x97\x61\x46\xae\x20\xce\x84"
			  "\x6b\xb7\xdc\x9b\xa9\x4a\x76\x7a"
			  "\xae\xf2\x0c\x0d\x61\xad\x02\x65"
			  "\x5e\xa9\x2d\xc4\xc4\xe4\x1a\x89"
			  "\x52\xc6\x51\xd3\x31\x74\xbe\x51"
			  "\xa1\x0c\x42\x11\x10\xe6\xd8\x15"
			  "\x88\xed\xe8\x21\x03\xa2\x52\xd8"
			  "\xa7\x50\xe8\x76\x8d\xef\xff\xed"
			  "\x91\x22\x81\x0a\xae\xb9\x9f\x91"
			  "\x72\xaf\x82\xb6\x04\xdc\x4b\x8e"
			  "\x51\xbc\xb0\x82\x35\xa6\xf4\x34"
			  "\x13\x32\xe4\xca\x60\x48\x2a\x4b"
			  "\xa1\xa0\x3b\x3e\x65\x00\x8f\xc5"
			  "\xda\x76\xb7\x0b\xf1\x69\x0d\xb4"
			  "\xea\xe2\x9c\x5f\x1b\xad\xd0\x3c"
			  "\x5c\xcf\x2a\x55\xd7\x05\xdd\xcd"
			  "\x86\xd4\x49\x51\x1c\xeb\x7e\xc3"
			  "\x0b\xf1\x2b\x1f\xa3\x5b\x91\x3f"
			  "\x9f\x74\x7a\x8a\xfd\x1b\x13\x0e"
			  "\x94\xbf\xf9\x4e\xff\xd0\x1a\x91"
			  "\x73\x5c\xa1\x72\x6a\xcd\x0b\x19"
			  "\x7c\x4e\x5b\x03\x39\x36\x97\xe1"
			  "\x26\x82\x6f\xb6\xbb\xde\x8e\xcc"
			  "\x1e\x08\x29\x85\x16\xe2\xc9\xed"
			  "\x03\xff\x3c\x1b\x78\x60\xf6\xde"
			  "\x76\xd4\xce\xcd\x94\xc8\x11\x98"
			  "\x55\xef\x52\x97\xca\x67\xe9\xf3"
			  "\xe7\xff\x72\xb1\xe9\x97\x85\xca"
			  "\x0a\x7e\x77\x20\xc5\xb3\x6d\xc6"
			  "\xd7\x2c\xac\x95\x74\xc8\xcb\xbc"
			  "\x2f\x80\x1e\x23\xe5\x6f\xd3\x44"
			  "\xb0\x7f\x22\x15\x4b\xeb\xa0\xf0"
			  "\x8c\xe8\x89\x1e\x64\x3e\xd9\x95"
			  "\xc9\x4d\x9a\x69\xc9\xf1\xb5\xf4"
			  "\x99\x02\x7a\x78\x57\x2a\xee\xbd"
			  "\x74\xd2\x0c\xc3\x98\x81\xc2\x13"
			  "\xee\x77\x0b\x10\x10\xe4\xbe\xa7"
			  "\x18\x84\x69\x77\xae\x11\x9f\x7a"
			  "\x02\x3a\xb5\x8c\xca\x0a\xd7\x52"
			  "\xaf\xe6\x56\xbb\x3c\x17\x25\x6a"
			  "\x9f\x6e\x9b\xf1\x9f\xdd\x5a\x38"
			  "\xfc\x82\xbb\xe8\x72\xc5\x53\x9e"
			  "\xdb\x60\x9e\xf4\xf7\x9c\x20\x3e"
			  "\xbb\x14\x0f\x2e\x58\x3c\xb2\xad"
			  "\x15\xb4\xaa\x5b\x65\x50\x16\xa8"
			  "\x44\x92\x77\xdb\xd4\x77\xef\x2c"
			  "\x8d\x6c\x01\x7d\xb7\x38\xb1\x8d"
			  "\xeb\x4a\x42\x7d\x19\x23\xce\x3f"
			  "\xf2\x62\x73\x57\x79\xa4\x18\xf2"
			  "\x0a\x28\x2d\xf9\x20\x14\x7b\xea"
			  "\xbe\x42\x1e\xe5\x31\x9d\x05\x68",
		.rlen   = 512,
	}, { /* XTS-AES 10, XTS-AES-256, data unit 512 bytes */
		.key	= "\x27\x18\x28\x18\x28\x45\x90\x45"
			  "\x23\x53\x60\x28\x74\x71\x35\x26"
			  "\x62\x49\x77\x57\x24\x70\x93\x69"
			  "\x99\x59\x57\x49\x66\x96\x76\x27"
			  "\x31\x41\x59\x26\x53\x58\x97\x93"
			  "\x23\x84\x62\x64\x33\x83\x27\x95"
			  "\x02\x88\x41\x97\x16\x93\x99\x37"
			  "\x51\x05\x82\x09\x74\x94\x45\x92",
		.klen	= 64,
		.iv	= "\xff\x00\x00\x00\x00\x00\x00\x00"
			  "\x00\x00\x00\x00\x00\x00\x00\x00",
		.input	= "\x00\x01\x02\x03\x04\x05\x06\x07"
			  "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
			  "\x10\x11\x12\x13\x14\x15\x16\x17"
			  "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
			  "\x20\x21\x22\x23\x24\x25\x26\x27"
			  "\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
			  "\x30\x31\x32\x33\x34\x35\x36\x37"
			  "\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
			  "\x40\x41\x42\x43\x44\x45\x46\x47"
			  "\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
			  "\x50\x51\x52\x53\x54\x55\x56\x57"
			  "\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
			  "\x60\x61\x62\x63\x64\x65\x66\x67"
			  "\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
			  "\x70\x71\x72\x73\x74\x75\x76\x77"
			  "\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
			  "\x80\x81\x82\x83\x84\x85\x86\x87"
			  "\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
			  "\x90\x91\x92\x93\x94\x95\x96\x97"
			  "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
			  "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7"
			  "\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
			  "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7"
			  "\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
			  "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7"
			  "\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
			  "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7"
			  "\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
			  "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
			  "\xe8\xe9\xea\xeb\xec\xed\xee\xef"
			  "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
			  "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
			  "\x00\x01\x02\x03\x04\x05\x06\x07"
			  "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
			  "\x10\x11\x12\x13\x14\x15\x16\x17"
			  "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
			  "\x20\x21\x22\x23\x24\x25\x26\x27"
			  "\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
			  "\x30\x31\x32\x33\x34\x35\x36\x37"
			  "\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
			  "\x40\x41\x42\x43\x44\x45\x46\x47"
			  "\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
			  "\x50\x51\x52\x53\x54\x55\x56\x57"
			  "\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
			  "\x60\x61\x62\x63\x64\x65\x66\x67"
			  "\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
			  "\x70\x71\x72\x73\x74\x75\x76\x77"
			  "\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
			  "\x80\x81\x82\x83\x84\x85\x86\x87"
			  "\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
			  "\x90\x91\x92\x93\x94\x95\x96\x97"
			  "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
			  "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7"
			  "\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
			  "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7"
			  "\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
			  "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7"
			  "\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
			  "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7"
			  "\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
			  "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
			  "\xe8\xe9\xea\xeb\xec\xed\xee\xef"
			  "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
			  "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff",
		.ilen	= 512,
		.result	= "\x1c\x3b\x3a\x10\x2f\x77\x03\x86"
			  "\xe4\x83\x6c\x99\xe3\x70\xcf\x9b"
			  "\xea\x00\x80\x3f\x5e\x48\x23\x57"
			  "\xa4\xae\x12\xd4\x14\xa3\xe6\x3b"
			  "\x5d\x31\xe2\x76\xf8\xfe\x4a\x8d"
			  "\x66\xb3\x17\xf9\xac\x68\x3f\x44"
			  "\x68\x0a\x86\xac\x35\xad\xfc\x33"
			  "\x45\xbe\xfe\xcb\x4b\xb1\x88\xfd"
			  "\x57\x76\x92\x6c\x49\xa3\x09\x5e"
			  "\xb1\x08\xfd\x10\x98\xba\xec\x70"
			  "\xaa\xa6\x69\x99\xa7\x2a\x82\xf2"
			  "\x7d\x84\x8b\x21\xd4\xa7\x41\xb0"
			  "\xc5\xcd\x4d\x5f\xff\x9d\xac\x89"
			  "\xae\xba\x12\x29\x61\xd0\x3a\x75"
			  "\x71\x23\xe9\x87\x0f\x8a\xcf\x10"
			  "\x00\x02\x08\x87\x89\x14\x29\xca"
			  "\x2a\x3e\x7a\x7d\x7d\xf7\xb1\x03"
			  "\x55\x16\x5c\x8b\x9a\x6d\x0a\x7d"
			  "\xe8\xb0\x62\xc4\x50\x0d\xc4\xcd"
			  "\x12\x0c\x0f\x74\x18\xda\xe3\xd0"
			  "\xb5\x78\x1c\x34\x80\x3f\xa7\x54"
			  "\x21\xc7\x90\xdf\xe1\xde\x18\x34"
			  "\xf2\x80\xd7\x66\x7b\x32\x7f\x6c"
			  "\x8c\xd7\x55\x7e\x12\xac\x3a\x0f"
			  "\x93\xec\x05\xc5\x2e\x04\x93\xef"
			  "\x31\xa1\x2d\x3d\x92\x60\xf7\x9a"
			  "\x28\x9d\x6a\x37\x9b\xc7\x0c\x50"
			  "\x84\x14\x73\xd1\xa8\xcc\x81\xec"
			  "\x58\x3e\x96\x45\xe0\x7b\x8d\x96"
			  "\x70\x65\x5b\xa5\xbb\xcf\xec\xc6"
			  "\xdc\x39\x66\x38\x0a\xd8\xfe\xcb"
			  "\x17\xb6\xba\x02\x46\x9a\x02\x0a"
			  "\x84\xe1\x8e\x8f\x84\x25\x20\x70"
			  "\xc1\x3e\x9f\x1f\x28\x9b\xe5\x4f"
			  "\xbc\x48\x14\x57\x77\x8f\x61\x60"
			  "\x15\xe1\x32\x7a\x02\xb1\x40\xf1"
			  "\x50\x5e\xb3\x09\x32\x6d\x68\x37"
			  "\x8f\x83\x74\x59\x5c\x84\x9d\x84"
			  "\xf4\xc3\x33\xec\x44\x23\x88\x51"
			  "\x43\xcb\x47\xbd\x71\xc5\xed\xae"
			  "\x9b\xe6\x9a\x2f\xfe\xce\xb1\xbe"
			  "\xc9\xde\x24\x4f\xbe\x15\x99\x2b"
			  "\x11\xb7\x7c\x04\x0f\x12\xbd\x8f"
			  "\x6a\x97\x5a\x44\xa0\xf9\x0c\x29"
			  "\xa9\xab\xc3\xd4\xd8\x93\x92\x72"
			  "\x84\xc5\x87\x54\xcc\xe2\x94\x52"
			  "\x9f\x86\x14\xdc\xd2\xab\xa9\x91"
			  "\x92\x5f\xed\xc4\xae\x74\xff\xac"
			  "\x6e\x33\x3b\x93\xeb\x4a\xff\x04"
			  "\x79\xda\x9a\x41\x0e\x44\x50\xe0"
			  "\xdd\x7a\xe4\xc6\xe2\x91\x09\x00"
			  "\x57\x5d\xa4\x01\xfc\x07\x05\x9f"
			  "\x64\x5e\x8b\x7e\x9b\xfd\xef\x33"
			  "\x94\x30\x54\xff\x84\x01\x14\x93"
			  "\xc2\x7b\x34\x29\xea\xed\xb4\xed"
			  "\x53\x76\x44\x1a\x77\xed\x43\x85"
			  "\x1a\xd7\x7f\x16\xf5\x41\xdf\xd2"
			  "\x69\xd5\x0d\x6a\x5f\x14\xfb\x0a"
			  "\xab\x1c\xbb\x4c\x15\x50\xbe\x97"
			  "\xf7\xab\x40\x66\x19\x3c\x4c\xaa"
			  "\x77\x3d\xad\x38\x01\x4b\xd2\x09"
			  "\x2f\xa7\x55\xc8\x24\xbb\x5e\x54"
			  "\xc4\xf3\x6f\xfd\xa9\xfc\xea\x70"
			  "\xb9\xc6\xe6\x93\xe1\x48\xc1\x51",
		.rlen	= 512,
	}
};

static struct cipher_testvec aes_cbc_enc_tv_template[] = {
	{ /* From RFC 3602 */
#if FIPS_FMP_FUNC_TEST == 2
		.key    = "\x07\xa9\x21\x40\x36\xb8\xa1\x5b"
#else
		.key    = "\x06\xa9\x21\x40\x36\xb8\xa1\x5b"
#endif
			  "\x51\x2e\x03\xd5\x34\x12\x00\x06",
		.klen   = 16,
		.iv	= "\x3d\xaf\xba\x42\x9d\x9e\xb4\x30"
			  "\xb4\x22\xda\x80\x2c\x9f\xac\x41",
		.input	= "Single block msg",
		.ilen   = 16,
		.result = "\xe3\x53\x77\x9c\x10\x79\xae\xb8"
			  "\x27\x08\x94\x2d\xbe\x77\x18\x1a",
		.rlen   = 16,
	}, {
		.key    = "\xc2\x86\x69\x6d\x88\x7c\x9a\xa0"
			  "\x61\x1b\xbb\x3e\x20\x25\xa4\x5a",
		.klen   = 16,
		.iv     = "\x56\x2e\x17\x99\x6d\x09\x3d\x28"
			  "\xdd\xb3\xba\x69\x5a\x2e\x6f\x58",
		.input  = "\x00\x01\x02\x03\x04\x05\x06\x07"
			  "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
			  "\x10\x11\x12\x13\x14\x15\x16\x17"
			  "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",
		.ilen   = 32,
		.result = "\xd2\x96\xcd\x94\xc2\xcc\xcf\x8a"
			  "\x3a\x86\x30\x28\xb5\xe1\xdc\x0a"
			  "\x75\x86\x60\x2d\x25\x3c\xff\xf9"
			  "\x1b\x82\x66\xbe\xa6\xd6\x1a\xb1",
		.rlen   = 32,
	}, {
		.key	= "\x60\x3d\xeb\x10\x15\xca\x71\xbe"
			  "\x2b\x73\xae\xf0\x85\x7d\x77\x81"
			  "\x1f\x35\x2c\x07\x3b\x61\x08\xd7"
			  "\x2d\x98\x10\xa3\x09\x14\xdf\xf4",
		.klen	= 32,
		.iv	= "\x00\x01\x02\x03\x04\x05\x06\x07"
			  "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f",
		.input	= "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96"
			  "\xe9\x3d\x7e\x11\x73\x93\x17\x2a"
			  "\xae\x2d\x8a\x57\x1e\x03\xac\x9c"
			  "\x9e\xb7\x6f\xac\x45\xaf\x8e\x51"
			  "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11"
			  "\xe5\xfb\xc1\x19\x1a\x0a\x52\xef"
			  "\xf6\x9f\x24\x45\xdf\x4f\x9b\x17"
			  "\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
		.ilen	= 64,
		.result	= "\xf5\x8c\x4c\x04\xd6\xe5\xf1\xba"
			  "\x77\x9e\xab\xfb\x5f\x7b\xfb\xd6"
			  "\x9c\xfc\x4e\x96\x7e\xdb\x80\x8d"
			  "\x67\x9f\x77\x7b\xc6\x70\x2c\x7d"
			  "\x39\xf2\x33\x69\xa9\xd9\xba\xcf"
			  "\xa5\x30\xe2\x63\x04\x23\x14\x61"
			  "\xb2\xeb\x05\xe2\xc3\x9b\xe9\xfc"
			  "\xda\x6c\x19\x07\x8c\x6a\x9d\x1b",
		.rlen	= 64,
	}, { /* Generated with Crypto++ */
		.key	= "\xC9\x83\xA6\xC9\xEC\x0F\x32\x55"
			  "\x0F\x32\x55\x78\x9B\xBE\x78\x9B"
			  "\xBE\xE1\x04\x27\xE1\x04\x27\x4A"
			  "\x6D\x90\x4A\x6D\x90\xB3\xD6\xF9",
		.klen	= 32,
		.iv	= "\xE7\x82\x1D\xB8\x53\x11\xAC\x47"
			  "\xE2\x7D\x18\xD6\x71\x0C\xA7\x42",
		.input	= "\x50\xB9\x22\xAE\x17\x80\x0C\x75"
			  "\xDE\x47\xD3\x3C\xA5\x0E\x9A\x03"
			  "\x6C\xF8\x61\xCA\x33\xBF\x28\x91"
			  "\x1D\x86\xEF\x58\xE4\x4D\xB6\x1F"
			  "\xAB\x14\x7D\x09\x72\xDB\x44\xD0"
			  "\x39\xA2\x0B\x97\x00\x69\xF5\x5E"
			  "\xC7\x30\xBC\x25\x8E\x1A\x83\xEC"
			  "\x55\xE1\x4A\xB3\x1C\xA8\x11\x7A"
			  "\x06\x6F\xD8\x41\xCD\x36\x9F\x08"
			  "\x94\xFD\x66\xF2\x5B\xC4\x2D\xB9"
			  "\x22\x8B\x17\x80\xE9\x52\xDE\x47"
			  "\xB0\x19\xA5\x0E\x77\x03\x6C\xD5"
			  "\x3E\xCA\x33\x9C\x05\x91\xFA\x63"
			  "\xEF\x58\xC1\x2A\xB6\x1F\x88\x14"
			  "\x7D\xE6\x4F\xDB\x44\xAD\x16\xA2"
			  "\x0B\x74\x00\x69\xD2\x3B\xC7\x30"
			  "\x99\x02\x8E\xF7\x60\xEC\x55\xBE"
			  "\x27\xB3\x1C\x85\x11\x7A\xE3\x4C"
			  "\xD8\x41\xAA\x13\x9F\x08\x71\xFD"
			  "\x66\xCF\x38\xC4\x2D\x96\x22\x8B"
			  "\xF4\x5D\xE9\x52\xBB\x24\xB0\x19"
			  "\x82\x0E\x77\xE0\x49\xD5\x3E\xA7"
			  "\x10\x9C\x05\x6E\xFA\x63\xCC\x35"
			  "\xC1\x2A\x93\x1F\x88\xF1\x5A\xE6"
			  "\x4F\xB8\x21\xAD\x16\x7F\x0B\x74"
			  "\xDD\x46\xD2\x3B\xA4\x0D\x99\x02"
			  "\x6B\xF7\x60\xC9\x32\xBE\x27\x90"
			  "\x1C\x85\xEE\x57\xE3\x4C\xB5\x1E"
			  "\xAA\x13\x7C\x08\x71\xDA\x43\xCF"
			  "\x38\xA1\x0A\x96\xFF\x68\xF4\x5D"
			  "\xC6\x2F\xBB\x24\x8D\x19\x82\xEB"
			  "\x54\xE0\x49\xB2\x1B\xA7\x10\x79"
			  "\x05\x6E\xD7\x40\xCC\x35\x9E\x07"
			  "\x93\xFC\x65\xF1\x5A\xC3\x2C\xB8"
			  "\x21\x8A\x16\x7F\xE8\x51\xDD\x46"
			  "\xAF\x18\xA4\x0D\x76\x02\x6B\xD4"
			  "\x3D\xC9\x32\x9B\x04\x90\xF9\x62"
			  "\xEE\x57\xC0\x29\xB5\x1E\x87\x13"
			  "\x7C\xE5\x4E\xDA\x43\xAC\x15\xA1"
			  "\x0A\x73\xFF\x68\xD1\x3A\xC6\x2F"
			  "\x98\x01\x8D\xF6\x5F\xEB\x54\xBD"
			  "\x26\xB2\x1B\x84\x10\x79\xE2\x4B"
			  "\xD7\x40\xA9\x12\x9E\x07\x70\xFC"
			  "\x65\xCE\x37\xC3\x2C\x95\x21\x8A"
			  "\xF3\x5C\xE8\x51\xBA\x23\xAF\x18"
			  "\x81\x0D\x76\xDF\x48\xD4\x3D\xA6"
			  "\x0F\x9B\x04\x6D\xF9\x62\xCB\x34"
			  "\xC0\x29\x92\x1E\x87\xF0\x59\xE5"
			  "\x4E\xB7\x20\xAC\x15\x7E\x0A\x73"
			  "\xDC\x45\xD1\x3A\xA3\x0C\x98\x01"
			  "\x6A\xF6\x5F\xC8\x31\xBD\x26\x8F"
			  "\x1B\x84\xED\x56\xE2\x4B\xB4\x1D"
			  "\xA9\x12\x7B\x07\x70\xD9\x42\xCE"
			  "\x37\xA0\x09\x95\xFE\x67\xF3\x5C"
			  "\xC5\x2E\xBA\x23\x8C\x18\x81\xEA"
			  "\x53\xDF\x48\xB1\x1A\xA6\x0F\x78"
			  "\x04\x6D\xD6\x3F\xCB\x34\x9D\x06"
			  "\x92\xFB\x64\xF0\x59\xC2\x2B\xB7"
			  "\x20\x89\x15\x7E\xE7\x50\xDC\x45"
			  "\xAE\x17\xA3\x0C\x75\x01\x6A\xD3"
			  "\x3C\xC8\x31\x9A\x03\x8F\xF8\x61"
			  "\xED\x56\xBF\x28\xB4\x1D\x86\x12",
		.ilen	= 496,
		.result	= "\xEA\x65\x8A\x19\xB0\x66\xC1\x3F"
			  "\xCE\xF1\x97\x75\xC1\xFD\xB5\xAF"
			  "\x52\x65\xF7\xFF\xBC\xD8\x2D\x9F"
			  "\x2F\xB9\x26\x9B\x6F\x10\xB7\xB8"
			  "\x26\xA1\x02\x46\xA2\xAD\xC6\xC0"
			  "\x11\x15\xFF\x6D\x1E\x82\x04\xA6"
			  "\xB1\x74\xD1\x08\x13\xFD\x90\x7C"
			  "\xF5\xED\xD3\xDB\x5A\x0A\x0C\x2F"
			  "\x0A\x70\xF1\x88\x07\xCF\x21\x26"
			  "\x40\x40\x8A\xF5\x53\xF7\x24\x4F"
			  "\x83\x38\x43\x5F\x08\x99\xEB\xE3"
			  "\xDC\x02\x64\x67\x50\x6E\x15\xC3"
			  "\x01\x1A\xA0\x81\x13\x65\xA6\x73"
			  "\x71\xA6\x3B\x91\x83\x77\xBE\xFA"
			  "\xDB\x71\x73\xA6\xC1\xAE\x43\xC3"
			  "\x36\xCE\xD6\xEB\xF9\x30\x1C\x4F"
			  "\x80\x38\x5E\x9C\x6E\xAB\x98\x2F"
			  "\x53\xAF\xCF\xC8\x9A\xB8\x86\x43"
			  "\x3E\x86\xE7\xA1\xF4\x2F\x30\x40"
			  "\x03\xA8\x6C\x50\x42\x9F\x77\x59"
			  "\x89\xA0\xC5\xEC\x9A\xB8\xDD\x99"
			  "\x16\x24\x02\x07\x48\xAE\xF2\x31"
			  "\x34\x0E\xC3\x85\xFE\x1C\x95\x99"
			  "\x87\x58\x98\x8B\xE7\xC6\xC5\x70"
			  "\x73\x81\x07\x7C\x56\x2F\xD8\x1B"
			  "\xB7\xB9\x2B\xAB\xE3\x01\x87\x0F"
			  "\xD8\xBB\xC0\x0D\xAC\x2C\x2F\x98"
			  "\x3C\x0B\xA2\x99\x4A\x8C\xF7\x04"
			  "\xE0\xE0\xCF\xD1\x81\x5B\xFE\xF5"
			  "\x24\x04\xFD\xB8\xDF\x13\xD8\xCD"
			  "\xF1\xE3\x3D\x98\x50\x02\x77\x9E"
			  "\xBC\x22\xAB\xFA\xC2\x43\x1F\x66"
			  "\x20\x02\x23\xDA\xDF\xA0\x89\xF6"
			  "\xD8\xF3\x45\x24\x53\x6F\x16\x77"
			  "\x02\x3E\x7B\x36\x5F\xA0\x3B\x78"
			  "\x63\xA2\xBD\xB5\xA4\xCA\x1E\xD3"
			  "\x57\xBC\x0B\x9F\x43\x51\x28\x4F"
			  "\x07\x50\x6C\x68\x12\x07\xCF\xFA"
			  "\x6B\x72\x0B\xEB\xF8\x88\x90\x2C"
			  "\x7E\xF5\x91\xD1\x03\xD8\xD5\xBD"
			  "\x22\x39\x7B\x16\x03\x01\x69\xAF"
			  "\x3D\x38\x66\x28\x0C\xBE\x5B\xC5"
			  "\x03\xB4\x2F\x51\x8A\x56\x17\x2B"
			  "\x88\x42\x6D\x40\x68\x8F\xD0\x11"
			  "\x19\xF9\x1F\x43\x79\x95\x31\xFA"
			  "\x28\x7A\x3D\xF7\x66\xEB\xEF\xAC"
			  "\x06\xB2\x01\xAD\xDB\x68\xDB\xEC"
			  "\x8D\x53\x6E\x72\x68\xA3\xC7\x63"
			  "\x43\x2B\x78\xE0\x04\x29\x8F\x72"
			  "\xB2\x2C\xE6\x84\x03\x30\x6D\xCD"
			  "\x26\x92\x37\xE1\x2F\xBB\x8B\x9D"
			  "\xE4\x4C\xF6\x93\xBC\xD9\xAD\x44"
			  "\x52\x65\xC7\xB0\x0E\x3F\x0E\x61"
			  "\x56\x5D\x1C\x6D\xA7\x05\x2E\xBC"
			  "\x58\x08\x15\xAB\x12\xAB\x17\x4A"
			  "\x5E\x1C\xF2\xCD\xB8\xA2\xAE\xFB"
			  "\x9B\x2E\x0E\x85\x34\x80\x0E\x3F"
			  "\x4C\xB8\xDB\xCE\x1C\x90\xA1\x61"
			  "\x6C\x69\x09\x35\x9E\xD4\xF4\xAD"
			  "\xBC\x06\x41\xE3\x01\xB4\x4E\x0A"
			  "\xE0\x1F\x91\xF8\x82\x96\x2D\x65"
			  "\xA3\xAA\x13\xCC\x50\xFF\x7B\x02",
		.rlen	= 496,
	},
};

/*
 * SHA256 test vectors from from NIST
 */
#define SHA256_TEST_VECTORS     2

static struct hash_testvec sha256_tv_template[] = {
	{
		.plaintext = "abc",
		.psize  = 3,
#if FIPS_FMP_FUNC_TEST == 3
		.digest = "\xbc\x78\x16\xbf\x8f\x01\xcf\xea"
#else
		.digest = "\xba\x78\x16\xbf\x8f\x01\xcf\xea"
#endif
			  "\x41\x41\x40\xde\x5d\xae\x22\x23"
			  "\xb0\x03\x61\xa3\x96\x17\x7a\x9c"
			  "\xb4\x10\xff\x61\xf2\x00\x15\xad",
	}, {
		.plaintext = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		.psize  = 56,
		.digest = "\x24\x8d\x6a\x61\xd2\x06\x38\xb8"
			  "\xe5\xc0\x26\x93\x0c\x3e\x60\x39"
			  "\xa3\x3c\xe4\x59\x64\xff\x21\x67"
			  "\xf6\xec\xed\xd4\x19\xdb\x06\xc1",
		.np	= 2,
		.tap	= { 28, 28 }
	},
};

/*
 * HMAC-SHA256 test vectors from
 * draft-ietf-ipsec-ciph-sha-256-01.txt
 */
#define HMAC_SHA256_TEST_VECTORS	10

static struct hash_testvec hmac_sha256_tv_template[] = {
	{
#if FIPS_FMP_FUNC_TEST == 4
		.key	= "\x02\x02\x03\x04\x05\x06\x07\x08"
#else
		.key	= "\x01\x02\x03\x04\x05\x06\x07\x08"
#endif
			  "\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
			  "\x11\x12\x13\x14\x15\x16\x17\x18"
			  "\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20",
		.ksize	= 32,
		.plaintext = "abc",
		.psize	= 3,
		.digest	= "\xa2\x1b\x1f\x5d\x4c\xf4\xf7\x3a"
			  "\x4d\xd9\x39\x75\x0f\x7a\x06\x6a"
			  "\x7f\x98\xcc\x13\x1c\xb1\x6a\x66"
			  "\x92\x75\x90\x21\xcf\xab\x81\x81",
	}, {
		.key	= "\x01\x02\x03\x04\x05\x06\x07\x08"
			  "\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
			  "\x11\x12\x13\x14\x15\x16\x17\x18"
			  "\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20",
		.ksize	= 32,
		.plaintext = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		.psize	= 56,
		.digest	= "\x10\x4f\xdc\x12\x57\x32\x8f\x08"
			  "\x18\x4b\xa7\x31\x31\xc5\x3c\xae"
			  "\xe6\x98\xe3\x61\x19\x42\x11\x49"
			  "\xea\x8c\x71\x24\x56\x69\x7d\x30",
	}, {
		.key	= "\x01\x02\x03\x04\x05\x06\x07\x08"
			  "\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
			  "\x11\x12\x13\x14\x15\x16\x17\x18"
			  "\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20",
		.ksize	= 32,
		.plaintext = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
			   "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		.psize	= 112,
		.digest	= "\x47\x03\x05\xfc\x7e\x40\xfe\x34"
			  "\xd3\xee\xb3\xe7\x73\xd9\x5a\xab"
			  "\x73\xac\xf0\xfd\x06\x04\x47\xa5"
			  "\xeb\x45\x95\xbf\x33\xa9\xd1\xa3",
	}, {
		.key	= "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
			"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
			"\x0b\x0b\x0b\x0b\x0b\x0b",
		.ksize	= 32,
		.plaintext = "Hi There",
		.psize	= 8,
		.digest	= "\x19\x8a\x60\x7e\xb4\x4b\xfb\xc6"
			  "\x99\x03\xa0\xf1\xcf\x2b\xbd\xc5"
			  "\xba\x0a\xa3\xf3\xd9\xae\x3c\x1c"
			  "\x7a\x3b\x16\x96\xa0\xb6\x8c\xf7",
	}, {
		.key	= "Jefe",
		.ksize	= 4,
		.plaintext = "what do ya want for nothing?",
		.psize	= 28,
		.digest	= "\x5b\xdc\xc1\x46\xbf\x60\x75\x4e"
			  "\x6a\x04\x24\x26\x08\x95\x75\xc7"
			  "\x5a\x00\x3f\x08\x9d\x27\x39\x83"
			  "\x9d\xec\x58\xb9\x64\xec\x38\x43",
		.np	= 2,
		.tap	= { 14, 14 }
	}, {
		.key	= "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		.ksize	= 32,
		.plaintext = "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
		.psize	= 50,
		.digest	= "\xcd\xcb\x12\x20\xd1\xec\xcc\xea"
			  "\x91\xe5\x3a\xba\x30\x92\xf9\x62"
			  "\xe5\x49\xfe\x6c\xe9\xed\x7f\xdc"
			  "\x43\x19\x1f\xbd\xe4\x5c\x30\xb0",
	}, {
		.key	= "\x01\x02\x03\x04\x05\x06\x07\x08"
			  "\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
			  "\x11\x12\x13\x14\x15\x16\x17\x18"
			  "\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20"
			  "\x21\x22\x23\x24\x25",
		.ksize	= 37,
		.plaintext = "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
			"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
			"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
			"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
		.psize	= 50,
		.digest	= "\xd4\x63\x3c\x17\xf6\xfb\x8d\x74"
			  "\x4c\x66\xde\xe0\xf8\xf0\x74\x55"
			  "\x6e\xc4\xaf\x55\xef\x07\x99\x85"
			  "\x41\x46\x8e\xb4\x9b\xd2\xe9\x17",
	}, {
		.key	= "\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
			"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
			"\x0c\x0c\x0c\x0c\x0c\x0c",
		.ksize	= 32,
		.plaintext = "Test With Truncation",
		.psize	= 20,
		.digest	= "\x75\x46\xaf\x01\x84\x1f\xc0\x9b"
			  "\x1a\xb9\xc3\x74\x9a\x5f\x1c\x17"
			  "\xd4\xf5\x89\x66\x8a\x58\x7b\x27"
			  "\x00\xa9\xc9\x7c\x11\x93\xcf\x42",
	}, {
		.key	= "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa",
		.ksize	= 80,
		.plaintext = "Test Using Larger Than Block-Size Key - Hash Key First",
		.psize	= 54,
		.digest	= "\x69\x53\x02\x5e\xd9\x6f\x0c\x09"
			  "\xf8\x0a\x96\xf7\x8e\x65\x38\xdb"
			  "\xe2\xe7\xb8\x20\xe3\xdd\x97\x0e"
			  "\x7d\xdd\x39\x09\x1b\x32\x35\x2f",
	}, {
		.key	= "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
			"\xaa\xaa",
		.ksize	= 80,
		.plaintext = "Test Using Larger Than Block-Size Key and Larger Than "
			   "One Block-Size Data",
		.psize	= 73,
		.digest	= "\x63\x55\xac\x22\xe8\x90\xd0\xa3"
			  "\xc8\x48\x1a\x5c\xa4\x82\x5b\xc8"
			  "\x84\xd3\xe7\xa1\xff\x98\xa2\xfc"
			  "\x2a\xc7\xd8\xe0\x64\xc3\xb2\xe6",
	},
};

static void hexdump(uint8_t *buf, uint32_t len)
{
	print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET,
			16, 1,
			buf, len, false);
}

static int alloc_buf(char *buf[XBUFSIZE])
{
	int i;

	for (i = 0; i < XBUFSIZE; i++) {
		buf[i] = (void *)__get_free_page(GFP_KERNEL);
		if (!buf[i])
			goto err_free_buf;
	}

	return 0;

err_free_buf:
	while (i-- > 0)
		free_page((unsigned long)buf[i]);

	return -ENOMEM;
}

static void free_buf(char *buf[XBUFSIZE])
{
	int i;

	for (i = 0; i < XBUFSIZE; i++)
		free_page((unsigned long)buf[i]);
}

static int aes_init(struct device *dev, int mode)
{
	int ret = 0;
	const struct fips_fmp_ops *ops;
	const struct of_device_id *match;

	match = of_match_node(exynos_fips_fmp_match, dev->of_node);
	ops = (struct fips_fmp_ops *)match->data;

	ret = ops->init(dev, mode);
	if (ret) {
		dev_err(dev, "Fail to init aes. ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int aes_set_iv(struct device *dev, int mode, uint8_t *iv, uint32_t iv_len)
{
	int ret = 0;
	const struct fips_fmp_ops *ops;
	const struct of_device_id *match;

	match = of_match_node(exynos_fips_fmp_match, dev->of_node);
	ops = match->data;

	if (iv_len != 16) {
		dev_err(dev, "Invalied iv length : %d\n", iv_len);
		return -EINVAL;
	}

	ret = ops->set_iv(dev, mode, iv, iv_len);
	if (ret) {
		dev_err(dev, "Fail to set aes iv. ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int aes_set_key(struct device *dev, int mode, uint8_t *in_key, uint32_t key_len)
{
	int ret = 0;
	const struct fips_fmp_ops *ops;
	const struct of_device_id *match;

	match = of_match_node(exynos_fips_fmp_match, dev->of_node);
	ops = match->data;

	ret = ops->set_key(dev, mode, in_key, key_len);
	if (ret) {
		dev_err(dev, "Fail to set aes key, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int aes_run(struct device *dev, uint32_t mode, uint8_t *data, uint32_t len, uint32_t write)
{
	int ret = 0;
	const struct fips_fmp_ops *ops;
	const struct of_device_id *match;

	match = of_match_node(exynos_fips_fmp_match, dev->of_node);
	ops = match->data;

	ret = ops->run(dev, mode, data, len, write);
	if (ret) {
		dev_err(dev, "Fail to encrypt. ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int aes_exit(struct device *dev)
{
	int ret = 0;
	const struct fips_fmp_ops *ops;
	const struct of_device_id *match;

	match = of_match_node(exynos_fips_fmp_match, dev->of_node);
	ops = (struct fips_fmp_ops *)match->data;

	ret = ops->exit();
	if (ret) {
		dev_err(dev, "Fail to exit aes. ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int fmp_aes_run(struct device *dev, struct cipher_testvec *template, const int idx,
				uint8_t *data, uint32_t len, const int mode, uint32_t write)
{
	int ret;

	ret = aes_init(dev, mode);
	if (ret) {
		dev_err(dev, "Fail to init. ret = %d\n", ret);
		goto err_init;
	}

	ret = aes_set_key(dev, mode, template->key, template->klen);
	if (ret) {
		dev_err(dev, "Fail to set aes key. ret = %d\n", ret);
		goto err_set_key;
	}

	ret = aes_set_iv(dev, mode, template->iv, 16);
	if (ret) {
		dev_err(dev, "Fail to set aes iv. ret = %d\n", ret);
		goto err_set_iv;
	}

	ret = aes_run(dev, mode, data, len, write);
	if (ret) {
		dev_err(dev, "Fail to set aes iv. ret = %d\n", ret);
		goto err_aes_set;
	}

	ret = aes_exit(dev);
	if (ret) {
		dev_err(dev, "Fail to exit. ret = %d\n", ret);
		goto err_exit;
	}

	return 0;

err_set_key:
err_set_iv:
err_aes_set:
	aes_exit(dev);
err_init:
err_exit:
	return -1;
}

int fips_fmp_cipher_init(struct device *dev, uint8_t *enckey, uint8_t *twkey, uint32_t key_len, uint32_t mode)
{
	int ret;
	uint8_t *key;

	ret = aes_init(dev, mode);
	if (ret) {
		dev_err(dev, "Fail to init. ret = %d\n", ret);
		goto err_init;
	}

	if (twkey) {
		key = (uint8_t *)kmalloc(key_len * 2, GFP_KERNEL);
		if (!key) {
			dev_err(dev, "Fail to alloc buffer for key with twkey.\n");
			goto err_set_key;
		}
		memcpy(key, enckey, key_len);
		memcpy(key + key_len, twkey, key_len);
		key_len *= 2;
	} else {
		key = (uint8_t *)kmalloc(key_len, GFP_KERNEL);
		if (!key) {
			dev_err(dev, "Fail to alloc buffer for key.\n");
			goto err_set_key;
		}
		memcpy(key, enckey, key_len);
	}

	ret = aes_set_key(dev, mode, key, key_len);
	if (ret) {
		dev_err(dev, "Fail to set aes key. ret = %d\n", ret);
		kfree(key);
		goto err_set_key;
	}

	kfree(key);
	return 0;

err_set_key:
	aes_exit(dev);
err_init:
	return -1;
}
EXPORT_SYMBOL_GPL(fips_fmp_cipher_init);

int fips_fmp_cipher_set_iv(struct device *dev, uint8_t *iv, uint32_t mode)
{
	int ret;

	ret = aes_set_iv(dev, mode, iv, 16);
	if (ret) {
		dev_err(dev, "Fail to set aes iv. ret = %d\n", ret);
		goto err_set_iv;
	}

	return 0;

err_set_iv:
	return -1;
}
EXPORT_SYMBOL_GPL(fips_fmp_cipher_set_iv);

int fips_fmp_cipher_run(struct device *dev, uint8_t *src, uint8_t *dst, uint32_t len, uint32_t mode, uint32_t enc)
{
	int ret;

	if (enc == ENCRYPT) {
		ret = aes_run(dev, mode, src, len, WRITE_MODE);
		if (ret) {
			dev_err(dev, "Fail to set aes iv. ret = %d\n", ret);
			goto err_aes_set;
		}

		ret = aes_run(dev, BYPASS_MODE, dst, len, READ_MODE);
		if (ret) {
			dev_err(dev, "Fail to set aes iv. ret = %d\n", ret);
			goto err_aes_set;
		}
	} else {
		ret = aes_run(dev, BYPASS_MODE, src, len, WRITE_MODE);
		if (ret) {
			dev_err(dev, "Fail to set aes iv. ret = %d\n", ret);
			goto err_aes_set;
		}

		ret = aes_run(dev, mode, dst, len, READ_MODE);
		if (ret) {
			dev_err(dev, "Fail to set aes iv. ret = %d\n", ret);
			goto err_aes_set;
		}
	}

	return 0;

err_aes_set:
	return -1;
}
EXPORT_SYMBOL_GPL(fips_fmp_cipher_run);

int fips_fmp_cipher_exit(struct device *dev)
{
	int ret;

	ret = aes_exit(dev);
	if (ret) {
		dev_err(dev, "Fail to exit. ret = %d\n", ret);
		goto err_exit;
	}

	return 0;
err_exit:
	return -1;
}
EXPORT_SYMBOL_GPL(fips_fmp_cipher_exit);

static int test_cipher(struct device *dev, int mode, struct cipher_testvec *template, uint32_t tcount)
{
	int ret;
	uint32_t idx;
	char *inbuf[XBUFSIZE];
	char *outbuf[XBUFSIZE];
	void *indata, *outdata;

	if (alloc_buf(inbuf)) {
		dev_err(dev, "Fail to alloc input buf.\n");
		goto err_alloc_inbuf;
	}

	if (alloc_buf(outbuf)) {
		dev_err(dev, "Fail to alloc output buf.\n");
		goto err_alloc_outbuf;
	}

	for (idx = 0; idx < tcount; idx++) {
		if (template[idx].np)
			continue;

		if (WARN_ON(template[idx].ilen > PAGE_SIZE)) {
			dev_err(dev, "Invalid input length. ilen = %d\n", template[idx].ilen);
			goto err_ilen;
		}

		indata = inbuf[0];
		outdata = outbuf[0];
		memset(indata, 0, FMP_BLK_SIZE);
		memcpy(indata, template[idx].input, template[idx].ilen);

		if (template[idx].ilen > PAGE_SIZE) {
			dev_err(dev, "Fail to fmp aes cipher due to invalid input length = %d\n", template[idx].ilen);
			goto err_aes_run;
		}

		ret = fmp_aes_run(dev, &template[idx], idx, indata, template[idx].ilen, mode, WRITE_MODE);
		if (ret) {
			dev_err(dev, "Fail to set aes encrypt run. ret = %d\n", ret);
			goto err_aes_run;
		}

		memset(outdata, 0, FMP_BLK_SIZE);
		ret = fmp_aes_run(dev, &template[idx], idx, outdata, template[idx].ilen, BYPASS_MODE, READ_MODE);
		if (ret) {
			dev_err(dev, "Fail to set aes bypass read mode. ret = %d\n", ret);
			goto err_aes_run;
		}

		if (memcmp(outdata, template[idx].result, template[idx].rlen)) {
			dev_err(dev, "Fail to compare encrypted result.\n");
			hexdump(outdata, template[idx].rlen);
			goto err_cmp;
		}

		ret = fmp_aes_run(dev, &template[idx], idx, outdata, template[idx].ilen, BYPASS_MODE, WRITE_MODE);
		if (ret) {
			dev_err(dev, "Fail to set aes bypass write mode. ret = %d\n", ret);
			goto err_aes_run;
		}

		memset(indata, 0, FMP_BLK_SIZE);
		ret = fmp_aes_run(dev, &template[idx], idx, indata, template[idx].ilen, mode, READ_MODE);
		if (ret) {
			dev_err(dev, "Fail to set aes decrypt read mode. ret = %d\n", ret);
			goto err_aes_run;
		}

		if (memcmp(indata, template[idx].input, template[idx].rlen)) {
			dev_err(dev, "Fail to compare decrypted result.\n");
			hexdump(outdata, template[idx].rlen);
			goto err_cmp;
		}
	}

	free_buf(inbuf);
	free_buf(outbuf);

	return 0;

err_ilen:
err_aes_run:
err_cmp:
	free_buf(outbuf);
err_alloc_outbuf:
	free_buf(inbuf);
err_alloc_inbuf:

	return -1;
}

struct fmp_test_result {
	struct completion completion;
	int err;
};

static int do_one_async_hash_op(struct ahash_request *req,
				struct fmp_test_result *tr,
				int ret)
{
	if (ret == -EINPROGRESS || ret == -EBUSY) {
		ret = wait_for_completion_interruptible(&tr->completion);
		if (!ret)
			ret = tr->err;
		reinit_completion(&tr->completion);
	}
	return ret;
}

static void test_complete(struct crypto_async_request *req, int err)
{
	struct fmp_test_result *res = req->data;

	if (err == -EINPROGRESS)
		return;

	res->err = err;
	complete(&res->completion);
}

static int test_hash(struct crypto_ahash *tfm, struct hash_testvec *template,
		     unsigned int tcount, bool use_digest)
{
	const char *algo = crypto_tfm_alg_driver_name(crypto_ahash_tfm(tfm));
	unsigned int i, j, k, temp;
	struct scatterlist sg[8];
	char result[64];
	struct ahash_request *req;
	struct fmp_test_result tresult;
	void *hash_buff;
	char *xbuf[XBUFSIZE];
	int ret = -ENOMEM;

	if (alloc_buf(xbuf))
		goto out_nobuf;

	init_completion(&tresult.completion);

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		printk(KERN_ERR "alg: hash: Failed to allocate request for "
		       "%s\n", algo);
		goto out_noreq;
	}
	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   test_complete, &tresult);

	j = 0;
	for (i = 0; i < tcount; i++) {
		if (template[i].np)
			continue;

		j++;
		memset(result, 0, 64);

		hash_buff = xbuf[0];

		memcpy(hash_buff, template[i].plaintext, template[i].psize);
		sg_init_one(&sg[0], hash_buff, template[i].psize);

		if (template[i].ksize) {
			crypto_ahash_clear_flags(tfm, ~0);
			ret = crypto_ahash_setkey(tfm, template[i].key,
						  template[i].ksize);
			if (ret) {
				printk(KERN_ERR "alg: hash: setkey failed on "
				       "test %d for %s: ret=%d\n", j, algo,
				       -ret);
				goto out;
			}
		}

		ahash_request_set_crypt(req, sg, result, template[i].psize);
		if (use_digest) {
			ret = do_one_async_hash_op(req, &tresult,
						   crypto_ahash_digest(req));
			if (ret) {
				pr_err("alg: hash: digest failed on test %d "
				       "for %s: ret=%d\n", j, algo, -ret);
				goto out;
			}
		} else {
			ret = do_one_async_hash_op(req, &tresult,
						   crypto_ahash_init(req));
			if (ret) {
				pr_err("alt: hash: init failed on test %d "
				       "for %s: ret=%d\n", j, algo, -ret);
				goto out;
			}
			ret = do_one_async_hash_op(req, &tresult,
						   crypto_ahash_update(req));
			if (ret) {
				pr_err("alt: hash: update failed on test %d "
				       "for %s: ret=%d\n", j, algo, -ret);
				goto out;
			}
			ret = do_one_async_hash_op(req, &tresult,
						   crypto_ahash_final(req));
			if (ret) {
				pr_err("alt: hash: final failed on test %d "
				       "for %s: ret=%d\n", j, algo, -ret);
				goto out;
			}
		}

		if (memcmp(result, template[i].digest,
			   crypto_ahash_digestsize(tfm))) {
			printk(KERN_ERR "alg: hash: Test %d failed for %s\n",
			       j, algo);
			hexdump(result, crypto_ahash_digestsize(tfm));
			ret = -EINVAL;
			goto out;
		}
	}

	j = 0;
	for (i = 0; i < tcount; i++) {
		if (template[i].np) {
			j++;
			memset(result, 0, 64);

			temp = 0;
			sg_init_table(sg, template[i].np);
			ret = -EINVAL;
			for (k = 0; k < template[i].np; k++) {
				if (WARN_ON(offset_in_page(IDX[k]) +
					    template[i].tap[k] > PAGE_SIZE))
					goto out;
				sg_set_buf(&sg[k],
					   memcpy(xbuf[IDX[k] >> PAGE_SHIFT] +
						  offset_in_page(IDX[k]),
						  template[i].plaintext + temp,
						  template[i].tap[k]),
					   template[i].tap[k]);
				temp += template[i].tap[k];
			}

			if (template[i].ksize) {
				crypto_ahash_clear_flags(tfm, ~0);
				ret = crypto_ahash_setkey(tfm, template[i].key,
							  template[i].ksize);

				if (ret) {
					printk(KERN_ERR "alg: hash: setkey "
					       "failed on chunking test %d "
					       "for %s: ret=%d\n", j, algo,
					       -ret);
					goto out;
				}
			}

			ahash_request_set_crypt(req, sg, result,
						template[i].psize);
			ret = crypto_ahash_digest(req);
			switch (ret) {
			case 0:
				break;
			case -EINPROGRESS:
			case -EBUSY:
				ret = wait_for_completion_interruptible(
					&tresult.completion);
				if (!ret && !(ret = tresult.err)) {
					reinit_completion(&tresult.completion);
					break;
				}
				/* fall through */
			default:
				printk(KERN_ERR "alg: hash: digest failed "
				       "on chunking test %d for %s: "
				       "ret=%d\n", j, algo, -ret);
				goto out;
			}

			if (memcmp(result, template[i].digest,
				   crypto_ahash_digestsize(tfm))) {
				printk(KERN_ERR "alg: hash: Chunking test %d "
				       "failed for %s\n", j, algo);
				hexdump(result, crypto_ahash_digestsize(tfm));
				ret = -EINVAL;
				goto out;
			}
		}
	}

	ret = 0;

out:
	ahash_request_free(req);
out_noreq:
	free_buf(xbuf);
out_nobuf:
	return ret;
}

static int alg_test_hash(const struct hash_test_suite *suite, const char *driver, u32 type, u32 mask)
{
	struct crypto_ahash *tfm;
	int err;

	tfm = crypto_alloc_ahash(driver, type, mask);
	if (IS_ERR(tfm)) {
		printk(KERN_ERR "alg: hash: Failed to load transform for %s: "
			"%ld\n", driver, PTR_ERR(tfm));
		return PTR_ERR(tfm);
	}

	err = test_hash(tfm, suite->vecs, suite->count, true);
	if (!err)
		err = test_hash(tfm, suite->vecs, suite->count, false);

	crypto_free_ahash(tfm);
	return err;
}

static int do_fmp_fw_selftest(void)
{
	int err = 0;

	err = exynos_smc(SMC_CMD_FMP, FMP_FW_SELFTEST, 0, 0);
	if (err) {
		printk(KERN_ERR "Fail to check selftest for FMP F/W. err = 0x%x\n", err);
		return -1;
	}

	return 0;
}

static int do_fips_fmp_selftest(struct device *dev)
{
	int ret;
	struct cipher_test_suite xts_cipher;
	struct cipher_test_suite cbc_cipher;
	struct hash_test_suite test_hmac_sha256;
	struct hash_test_suite test_sha256;

	/* Self test for AES XTS mode */
	xts_cipher.enc.vecs = aes_xts_enc_tv_template;
	xts_cipher.enc.count = AES_XTS_ENC_TEST_VECTORS;
	ret = test_cipher(dev, XTS_MODE, xts_cipher.enc.vecs, xts_cipher.enc.count);
	if (ret) {
		printk(KERN_ERR "FIPS: self-tests for UFSFMP aes-xts failed\n");
		goto err_xts_cipher;
	}
	printk(KERN_INFO "FIPS: self-tests for UFSFMP aes-xts passed\n");

	/* Self test for AES CBC mode */
	cbc_cipher.enc.vecs = aes_cbc_enc_tv_template;
	cbc_cipher.enc.count = AES_CBC_ENC_TEST_VECTORS;
	ret = test_cipher(dev, CBC_MODE, cbc_cipher.enc.vecs, cbc_cipher.enc.count);
	if (ret) {
		printk(KERN_ERR "FIPS: self-tests for UFSFMP aes-cbc failed\n");
		goto err_cbc_cipher;
	}
	printk(KERN_INFO "FIPS: self-tests for UFSFMP aes-cbc passed\n");

	test_sha256.vecs = sha256_tv_template;
	test_sha256.count = SHA256_TEST_VECTORS;
	ret = alg_test_hash(&test_sha256, ALG_SHA256_FMP, 0, 0);
	if (ret) {
		printk(KERN_ERR "FIPS: self-tests for UFSFMP %s failed\n", ALG_SHA256_FMP);
		goto err_xts_cipher;
	}
	printk(KERN_INFO "FIPS: self-tests for UFSFMP %s passed\n", ALG_SHA256_FMP);

	test_hmac_sha256.vecs = hmac_sha256_tv_template;
	test_hmac_sha256.count = HMAC_SHA256_TEST_VECTORS;
	ret = alg_test_hash(&test_hmac_sha256, ALG_HMAC_SHA256_FMP, 0, 0);
	if (ret) {
		printk(KERN_ERR "FIPS: self-tests for UFSFMP %s failed\n", ALG_HMAC_SHA256_FMP);
		goto err_xts_cipher;
	}
	printk(KERN_INFO "FIPS: self-tests for UFSFMP %s passed\n", ALG_HMAC_SHA256_FMP);

	ret = do_fmp_fw_selftest();
	if (ret) {
		printk(KERN_ERR "FIPS: self-tests for FMP FW failed\n");
		goto err_fw_selftest;
	}
	printk(KERN_INFO "FIPS: self-tests for FMP FW passed\n");

	return 0;

err_cbc_cipher:
err_xts_cipher:
err_fw_selftest:
	return -1;
}

static ssize_t fips_fmp_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(pass), "%s\n", fips_fmp_result ? pass : fail);
}

static DEVICE_ATTR(fips_fmp_status, 0444, fips_fmp_status_show, NULL);

static struct attribute *fips_fmp_attr[] = {
	&dev_attr_fips_fmp_status.attr,
	NULL,
};

static struct attribute_group fips_fmp_attr_group = {
	.name	= "fips-fmp",
	.attrs	= fips_fmp_attr,
};

static int fmp_create_session(struct fmp_info *info,
			struct fcrypt *fcr, struct session_op *sop)
{
	int ret = 0;
	struct device *dev = info->dev;
	struct csession *ses_new = NULL, *ses_ptr;
	const char *alg_name = NULL;
	const char *hash_name = NULL;
	int hmac_mode = 1;
	int fw_mode = 0;

	/*
	 * With composite aead ciphers, only ckey is used and it can cover all the
	 * structure space; otherwise both keys may be used simultaneously but they
	 * are confined to their spaces
	 */
	struct {
		uint8_t mkey[FMP_HMAC_MAX_KEY_LEN];
		/* padding space for aead keys */
		uint8_t pad[RTA_SPACE(sizeof(struct crypto_authenc_key_param))];
	} keys;

	if (unlikely(!sop->cipher && !sop->mac)) {
		dev_err(dev, "Both 'cipher' and 'mac' unset.\n");
		return -EINVAL;
	}

	switch (sop->cipher) {
	case 0:
		break;
	case FMP_AES_CBC:
		alg_name = "cbc(aes-fmp)";
		break;
	case FMP_AES_XTS:
		alg_name = "xts(aes-fmp)";
		break;
	default:
		dev_err(dev, "Invalid cipher : %d\n", sop->cipher);
		return -EINVAL;
	}

	switch (sop->mac) {
	case 0:
		break;
	case FMP_SHA2_256_HMAC:
		hash_name = "hmac-fmp(sha256-fmp)";
		break;
	case FMP_SHA2_256:
		hash_name = "sha256-fmp";
		hmac_mode = 0;
		break;
	case FMPFW_SHA2_256_HMAC:
		hash_name = "hmac-fmpfw(sha256-fmp)";
		fw_mode = 1;
		break;
	case FMPFW_SHA2_256:
		hash_name = "sha256-fmpfw";
		fw_mode = 1;
		hmac_mode = 0;
		break;
	default:
		dev_err(dev, "Invalid mac : %d\n", sop->mac);
		return -EINVAL;
	}

	/* Create a session and put it to the list. */
	ses_new = kzalloc(sizeof(*ses_new), GFP_KERNEL);
	if (!ses_new) {
		dev_err(dev, "Fail to allocate session buffer\n");
		return -ENOMEM;
	}

	/* Set-up crypto transform. */
	if (alg_name) {
		uint8_t keyp[FMP_CIPHER_MAX_KEY_LEN];
		if (unlikely(sop->keylen > FMP_CIPHER_MAX_KEY_LEN)) {
			dev_err(dev, "Setting key failed for %s-%zu.\n",
					alg_name, (size_t)sop->keylen*8);
			ret = -EINVAL;
			goto error_cipher;
		}
		if (unlikely(copy_from_user(keyp, sop->key, sop->keylen))) {
			ret = -EFAULT;
			goto error_cipher;
		}

		if (!strcmp(alg_name, "xts(aes-fmp)"))
			ret = fmpdev_cipher_init(info, &ses_new->cdata, alg_name, \
					keyp, keyp + (sop->keylen >> 1), sop->keylen >> 1);
		else
			ret = fmpdev_cipher_init(info, &ses_new->cdata, alg_name, \
					keyp, NULL, sop->keylen);
		if (ret < 0) {
			dev_err(dev, "%s: Fail to load cipher for %s\n",
					__func__, alg_name);
			ret = -EINVAL;
			goto error_cipher;
		}
		info->init_status = 1;
	}

	if (hash_name) {
		if (unlikely(sop->mackeylen > FMP_HMAC_MAX_KEY_LEN)) {
			dev_err(dev, "Setting key failed for %s-%zu.",
				hash_name, (size_t)sop->mackeylen*8);
			ret = -EINVAL;
			goto error_hash;
		}

		if (sop->mackey && unlikely(copy_from_user(keys.mkey, sop->mackey,
					    sop->mackeylen))) {
			ret = -EFAULT;
			goto error_hash;
		}

		if (fw_mode)
			ret = fmpfw_hash_init(info, &ses_new->hdata, hash_name,
								hmac_mode, keys.mkey,
								sop->mackeylen);
		else
			ret = fmpdev_hash_init(info, &ses_new->hdata, hash_name,
								hmac_mode, keys.mkey,
								sop->mackeylen);
		if (ret != 0) {
			dev_err(dev, "Failed to load hash for %s", hash_name);
			ret = -EINVAL;
			goto error_hash;
		}
	}

	ses_new->alignmask = max(ses_new->cdata.alignmask,
			ses_new->hdata.alignmask);
	ses_new->array_size = DEFAULT_PREALLOC_PAGES;
	ses_new->pages = kzalloc(ses_new->array_size *
			sizeof(struct page *), GFP_KERNEL);
	ses_new->sg = kzalloc(ses_new->array_size *
			sizeof(struct scatterlist), GFP_KERNEL);
	if (ses_new->sg == NULL || ses_new->pages == NULL) {
		dev_err(dev, "Memory error\n");
		ret = -ENOMEM;
		goto error_hash;
	}

	/* put the new session to the list */
	get_random_bytes(&ses_new->sid, sizeof(ses_new->sid));
	mutex_init(&ses_new->sem);

	mutex_lock(&fcr->sem);
restart:
	list_for_each_entry(ses_ptr, &fcr->list, entry) {
		/* Check for duplicate SID */
		if (unlikely(ses_new->sid == ses_ptr->sid)) {
			get_random_bytes(&ses_new->sid, sizeof(ses_new->sid));
			/* Unless we have a broken RNG this
			   shouldn't loop forever... ;-) */
			goto restart;
		}
	}

	list_add(&ses_new->entry, &fcr->list);
	mutex_unlock(&fcr->sem);

	/* Fill in some values for the user. */
	sop->ses = ses_new->sid;

	return 0;

error_hash:
	fmpdev_cipher_deinit(&ses_new->cdata);
	kfree(ses_new->sg);
	kfree(ses_new->pages);
error_cipher:
	kfree(ses_new);
	return ret;
}

static inline void fmp_destroy_session(struct fmp_info *info, struct csession *ses_ptr)
{
	struct device *dev = info->dev;

	if (!mutex_trylock(&ses_ptr->sem)) {
		dev_err(dev, "Waiting for semaphore of sid=0x%08X\n", ses_ptr->sid);
		mutex_lock(&ses_ptr->sem);
	}
	fmpdev_cipher_deinit(&ses_ptr->cdata);
	if (!ses_ptr->hdata.fmpfw_info)
		fmpdev_hash_deinit(&ses_ptr->hdata);
	else
		fmpfw_hash_deinit(&ses_ptr->hdata);
	kfree(ses_ptr->pages);
	kfree(ses_ptr->sg);
	mutex_unlock(&ses_ptr->sem);
	mutex_destroy(&ses_ptr->sem);
	kfree(ses_ptr);
}

/* Look up a session by ID and remove. */
static int fmp_finish_session(struct fmp_info *info, struct fcrypt *fcr, uint32_t sid)
{
	struct device *dev = info->dev;
	struct csession *tmp, *ses_ptr;
	struct list_head *head;
	int ret = 0;

	mutex_lock(&fcr->sem);
	head = &fcr->list;

	if (info->init_status) {
		ret = fmpdev_cipher_exit(info);
		if (ret < 0) {
			dev_err(dev, "Fail to exit fmp dev. sid = %d\n", sid);
			ret = -ENOENT;
		}
		info->init_status = 0;
	}

	list_for_each_entry_safe(ses_ptr, tmp, head, entry) {
		if (ses_ptr->sid == sid) {
			list_del(&ses_ptr->entry);
			fmp_destroy_session(info, ses_ptr);
			break;
		}
	}

	if (unlikely(!ses_ptr)) {
		dev_err(dev, "Session with sid=0x%08X not found!\n", sid);
		ret = -ENOENT;
	}
	mutex_unlock(&fcr->sem);

	return ret;
}

/* Remove all sessions when closing the file */
static int fmp_finish_all_sessions(struct fmp_info *info, struct fcrypt *fcr)
{
	struct csession *tmp, *ses_ptr;
	struct list_head *head;

	mutex_lock(&fcr->sem);

	head = &fcr->list;
	list_for_each_entry_safe(ses_ptr, tmp, head, entry) {
		list_del(&ses_ptr->entry);
		fmp_destroy_session(info, ses_ptr);
	}
	mutex_unlock(&fcr->sem);

	return 0;
}

/* Look up session by session ID. The returned session is locked. */
struct csession *fmp_get_session_by_sid(struct fcrypt *fcr, uint32_t sid)
{
	struct csession *ses_ptr, *retval = NULL;

	if (unlikely(fcr == NULL))
		return NULL;

	mutex_lock(&fcr->sem);
	list_for_each_entry(ses_ptr, &fcr->list, entry) {
		if (ses_ptr->sid == sid) {
			mutex_lock(&ses_ptr->sem);
			retval = ses_ptr;
			break;
		}
	}
	mutex_unlock(&fcr->sem);

	return retval;
}

static void fmptask_routine(struct work_struct *work)
{
	struct fmp_info *info = container_of(work, struct fmp_info, fmptask);
	struct device *dev = info->dev;
	struct todo_list_item *item;
	LIST_HEAD(tmp);

	/* fetch all pending jobs into the temporary list */
	mutex_lock(&info->todo.lock);
	list_cut_position(&tmp, &info->todo.list, info->todo.list.prev);
	mutex_unlock(&info->todo.lock);

	/* handle each job locklessly */
	list_for_each_entry(item, &tmp, __hook) {
		item->result = fmp_run(info, &info->fcrypt, &item->kcop);
		if (unlikely(item->result))
			dev_err(dev, "%s: crypto_run() failed: %d\n",
					__func__, item->result);
	}

	/* push all handled jobs to the done list at once */
	mutex_lock(&info->done.lock);
	list_splice_tail(&tmp, &info->done.list);
	mutex_unlock(&info->done.lock);

	/* wake for POLLIN */
	wake_up_interruptible(&info->user_waiter);
}

static int find_fmp_device_by_node(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos-fips-fmp");
	if (!np) {
		printk("Fail to find device node for fmp\n");
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		printk("Fail to find pdev for fmp\n");
		return -ENODEV;
	}

	return 0;
}

static int fmpdev_open(struct inode *inode, struct file *file)
{
	int i, ret;
	struct fmp_info *info;
	struct todo_list_item *tmp;
	struct device *dev;

	ret = find_fmp_device_by_node();
	if (ret) {
		printk("Fail to find fmp device by node\n");
		return -1;
	}

	dev = &pdev->dev;
	printk("fmp fips driver name : %s\n", dev_name(dev));

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "fail to alloc fmp private data\n");
		return -ENOMEM;
	}
	memset(info, 0, sizeof(*info));

	mutex_init(&info->fcrypt.sem);
	INIT_LIST_HEAD(&info->fcrypt.list);

	INIT_LIST_HEAD(&info->free.list);
	INIT_LIST_HEAD(&info->todo.list);
	INIT_LIST_HEAD(&info->done.list);
	INIT_WORK(&info->fmptask, fmptask_routine);
	mutex_init(&info->free.lock);
	mutex_init(&info->todo.lock);
	mutex_init(&info->done.lock);
	init_waitqueue_head(&info->user_waiter);

	for (i = 0; i < DEF_COP_RINGSIZE; i++) {
		tmp = kzalloc(sizeof(struct todo_list_item), GFP_KERNEL);
		info->itemcount++;
		dev_info(dev, "%s: allocated new item at %lx\n",
				__func__, (unsigned long)tmp);
		list_add(&tmp->__hook, &info->free.list);
	}

	info->dev = dev;
	file->private_data = info;

	dev_info(dev, "%s opened.\n", dev_name(dev));

	return 0;
}

/* this function has to be called from process context */
static int fill_kcop_from_cop(struct fmp_info *info,
		struct kernel_crypt_op *kcop, struct fcrypt *fcr)
{
	struct device *dev = info->dev;
	struct crypt_op *cop = &kcop->cop;
	struct csession *ses_ptr;
	int rc;

	/* this also enters ses_ptr->sem */
	ses_ptr = fmp_get_session_by_sid(fcr, cop->ses);
	if (unlikely(!ses_ptr)) {
		dev_err(dev, "invalid session ID=0x%08X\n", cop->ses);
		return -EINVAL;
	}
	kcop->digestsize = 0; /* will be updated during operation */

	fmp_put_session(ses_ptr);

	kcop->task = current;
	kcop->mm = current->mm;

	if (cop->iv) {
		kcop->ivlen = ses_ptr->cdata.ivsize;
		rc = copy_from_user(kcop->iv, cop->iv, kcop->ivlen);
		if (unlikely(rc)) {
			dev_err(dev,
				"error copying IV (%d bytes), copy_from_user returned %d for address %lx\n",
					kcop->ivlen, rc, (unsigned long)cop->iv);
			return -EFAULT;
		}
	} else {
		kcop->ivlen = 0;
	}

	return 0;
}

/* this function has to be called from process context */
static int fill_cop_from_kcop(struct kernel_crypt_op *kcop, struct fcrypt *fcr)
{
	int ret;

	if (kcop->digestsize) {
		ret = copy_to_user(kcop->cop.mac,
				kcop->hash_output, kcop->digestsize);
		if (unlikely(ret))
			return -EFAULT;
	}
	if (kcop->ivlen && kcop->cop.flags & COP_FLAG_WRITE_IV) {
		ret = copy_to_user(kcop->cop.iv,
				kcop->iv, kcop->ivlen);
		if (unlikely(ret))
			return -EFAULT;
	}
	return 0;
}


static int kcop_from_user(struct fmp_info *info, struct kernel_crypt_op *kcop,
		struct fcrypt *fcr, void __user *arg)
{
	if (unlikely(copy_from_user(&kcop->cop, arg, sizeof(kcop->cop))))
		return -EFAULT;

	return fill_kcop_from_cop(info, kcop, fcr);
}

static int kcop_to_user(struct fmp_info *info, struct kernel_crypt_op *kcop,
		struct fcrypt *fcr, void __user *arg)
{
	int ret;
	struct device *dev = info->dev;

	ret = fill_cop_from_kcop(kcop, fcr);
	if (unlikely(ret)) {
		dev_err(dev, "Error in fill_cop_from_kcop\n");
		return ret;
	}

	if (unlikely(copy_to_user(arg, &kcop->cop, sizeof(kcop->cop)))) {
		dev_err(dev, "Cannot copy to userspace\n");
		return -EFAULT;
	}
	return 0;
}

static inline void tfm_info_to_alg_info(struct alg_info *dst, struct crypto_tfm *tfm)
{
	snprintf(dst->cra_name, FMPDEV_MAX_ALG_NAME,
			"%s", crypto_tfm_alg_name(tfm));
	snprintf(dst->cra_driver_name, FMPDEV_MAX_ALG_NAME,
			"%s", crypto_tfm_alg_driver_name(tfm));
}

static int get_session_info(struct fmp_info *info,
		struct fcrypt *fcr, struct session_info_op *siop)
{
	struct csession *ses_ptr;
	struct device *dev = info->dev;
	struct crypto_tfm *tfm;

	/* this also enters ses_ptr->sem */
	ses_ptr = fmp_get_session_by_sid(fcr, siop->ses);
	if (unlikely(!ses_ptr)) {
		dev_err(dev, "invalid session ID=0x%08X\n", siop->ses);
		return -EINVAL;
	}

	siop->flags = 0;

	if (ses_ptr->hdata.init && !ses_ptr->hdata.fmpfw_info) {
		tfm = crypto_ahash_tfm(ses_ptr->hdata.async.s);
		tfm_info_to_alg_info(&siop->hash_info, tfm);
#ifdef CRYPTO_ALG_KERN_DRIVER_ONLY
		if (tfm->__crt_alg->cra_flags & CRYPTO_ALG_KERN_DRIVER_ONLY)
			siop->flags |= SIOP_FLAG_KERNEL_DRIVER_ONLY;
#else
		if (is_known_accelerated(tfm))
			siop->flags |= SIOP_FLAG_KERNEL_DRIVER_ONLY;
#endif
	}

	siop->alignmask = ses_ptr->alignmask;
	fmp_put_session(ses_ptr);

	return 0;
}

static long fmpdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg_)
{
	int ret;
	struct session_op sop;
	struct kernel_crypt_op kcop;
	struct fmp_info *info = file->private_data;
	struct device *dev = info->dev;
	void __user *arg = (void __user *)arg_;
	struct fcrypt *fcr;
	struct session_info_op siop;
	uint32_t ses;

	if (unlikely(!info))
		BUG();

	fcr = &info->fcrypt;

	switch (cmd) {
	case FMPGSESSION:
		if (unlikely(copy_from_user(&sop, arg, sizeof(sop))))
			return -EFAULT;

		ret = fmp_create_session(info, fcr, &sop);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to create fmp session. ret = %d\n", ret);
			return ret;
		}
		ret = copy_to_user(arg, &sop, sizeof(sop));
		if (unlikely(ret)) {
			fmp_finish_session(info, fcr, sop.ses);
			return -EFAULT;
		}
		return ret;
	case FMPFSESSION:
		ret = get_user(ses, (uint32_t __user *)arg);
		if (unlikely(ret))
			return ret;
		ret = fmp_finish_session(info, fcr, ses);
		if (unlikely(ret))
			dev_err(dev, "Fail to finish fmp session. ret = %d\n", ret);
		return ret;
	case FMPGSESSIONINFO:
		if (unlikely(copy_from_user(&siop, arg, sizeof(siop))))
			return -EFAULT;

		ret = get_session_info(info, fcr, &siop);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to get fmp session. ret = %d\n", ret);
			return ret;
		}
		return copy_to_user(arg, &siop, sizeof(siop));
	case FMPCRYPT:
		if (unlikely(ret = kcop_from_user(info, &kcop, fcr, arg)))
			return ret;

		if (unlikely(in_fmp_fips_err())) {
			dev_err(dev, "Fail to run fmp due to fips in error.");
			return -EPERM;
		}

		ret = fmp_run(info, fcr, &kcop);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to run fmp crypt. ret = %d\n", ret);
			return ret;
		}
		return kcop_to_user(info, &kcop, fcr, arg);
	case FMP_AES_CBC_MCT:
		if (unlikely(ret = kcop_from_user(info, &kcop, fcr, arg))) {
			dev_err(dev, "Error copying from user");
			return ret;
		}

		if (unlikely(in_fmp_fips_err())) {
			dev_err(dev, "Fail to run fmp due to fips in error.");
			return -EPERM;
		}

		ret = fmp_run_AES_CBC_MCT(info, fcr, &kcop);
		if (unlikely(ret)) {
			dev_err(dev, "Error in fmp_run_AES_CBC_MCT");
			return ret;
		}
		return kcop_to_user(info, &kcop, fcr, arg);
	default:
		dev_err(dev, "Invalid command : 0x%x\n", cmd);
		return -EINVAL;
	}
}

/* compatibility code for 32bit userlands */
#ifdef CONFIG_COMPAT
static inline void compat_to_session_op(struct compat_session_op *compat,
					struct session_op *sop)
{
	sop->cipher = compat->cipher;
	sop->mac = compat->mac;
	sop->keylen = compat->keylen;

	sop->key       = compat_ptr(compat->key);
	sop->mackeylen = compat->mackeylen;
	sop->mackey    = compat_ptr(compat->mackey);
	sop->ses       = compat->ses;
}

static inline void session_op_to_compat(struct session_op *sop,
				struct compat_session_op *compat)
{
	compat->cipher = sop->cipher;
	compat->mac = sop->mac;
	compat->keylen = sop->keylen;

	compat->key       = ptr_to_compat(sop->key);
	compat->mackeylen = sop->mackeylen;
	compat->mackey    = ptr_to_compat(sop->mackey);
	compat->ses       = sop->ses;
}

static inline void compat_to_crypt_op(struct compat_crypt_op *compat,
					struct crypt_op *cop)
{
	cop->ses = compat->ses;
	cop->op = compat->op;
	cop->flags = compat->flags;
	cop->len = compat->len;

	cop->src = compat_ptr(compat->src);
	cop->dst = compat_ptr(compat->dst);
	cop->mac = compat_ptr(compat->mac);
	cop->iv  = compat_ptr(compat->iv);
	cop->secondLastEncodedData = compat_ptr(compat->secondLastEncodedData);
	cop->thirdLastEncodedData = compat_ptr(compat->thirdLastEncodedData);
}

static inline void crypt_op_to_compat(struct crypt_op *cop,
				struct compat_crypt_op *compat)
{
	compat->ses = cop->ses;
	compat->op = cop->op;
	compat->flags = cop->flags;
	compat->len = cop->len;

	compat->src = ptr_to_compat(cop->src);
	compat->dst = ptr_to_compat(cop->dst);
	compat->mac = ptr_to_compat(cop->mac);
	compat->iv  = ptr_to_compat(cop->iv);
	compat->secondLastEncodedData  = ptr_to_compat(cop->secondLastEncodedData);
	compat->thirdLastEncodedData  = ptr_to_compat(cop->thirdLastEncodedData);
}

static int compat_kcop_from_user(struct fmp_info *info,
		struct kernel_crypt_op *kcop,
		struct fcrypt *fcr, void __user *arg)
{
	struct compat_crypt_op compat_cop;

	if (unlikely(copy_from_user(&compat_cop, arg, sizeof(compat_cop))))
		return -EFAULT;
	compat_to_crypt_op(&compat_cop, &kcop->cop);

	return fill_kcop_from_cop(info, kcop, fcr);
}

static int compat_kcop_to_user(struct fmp_info *info,
		struct kernel_crypt_op *kcop,
		struct fcrypt *fcr, void __user *arg)
{
	int ret;
	struct device *dev = info->dev;
	struct compat_crypt_op compat_cop;

	ret = fill_cop_from_kcop(kcop, fcr);
	if (unlikely(ret)) {
		dev_err(dev, "Error in fill_cop_from_kcop");
		return ret;
	}
	crypt_op_to_compat(&kcop->cop, &compat_cop);

	if (unlikely(copy_to_user(arg, &compat_cop, sizeof(compat_cop)))) {
		dev_err(dev, "Error copying to user");
		return -EFAULT;
	}
	return 0;
}

static long fmpdev_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg_)
{
	int ret;
	struct session_op sop;
	struct compat_session_op compat_sop;
	struct kernel_crypt_op kcop;
	struct fmp_info *info = file->private_data;
	struct device *dev = info->dev;
	void __user *arg = (void __user *)arg_;
	struct fcrypt *fcr;

	if (unlikely(info))
		BUG();

	fcr = &info->fcrypt;

	switch (cmd) {
	case FMPFSESSION:
	case FMPGSESSIONINFO:
		return fmpdev_ioctl(file, cmd, arg_);

	case COMPAT_FMPGSESSION:
		if (unlikely(copy_from_user(&compat_sop, arg, sizeof(compat_sop))))
			return -EFAULT;
		compat_to_session_op(&compat_sop, &sop);

		ret = fmp_create_session(info, fcr, &sop);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to create fmp session. ret = %d\n", ret);
			return ret;
		}

		session_op_to_compat(&sop, &compat_sop);
		ret = copy_to_user(arg, &compat_sop, sizeof(compat_sop));
		if (unlikely(ret)) {
			fmp_finish_session(info, fcr, sop.ses);
			return -EFAULT;
		}
		return ret;
	case COMPAT_FMPCRYPT:
		if (unlikely(ret = compat_kcop_from_user(info, &kcop, fcr, arg)))
			return ret;

		if (unlikely(in_fmp_fips_err())) {
			dev_err(dev, "Fail to run fmp due to fips in error.");
			return -EPERM;
		}

		ret = fmp_run(info, fcr, &kcop);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to run fmp crypt. ret = %d\n", ret);
			return ret;
		}
		return compat_kcop_to_user(info, &kcop, fcr, arg);
	case COMPAT_FMP_AES_CBC_MCT:
		if (unlikely(ret = compat_kcop_from_user(info, &kcop, fcr, arg))) {
			dev_err(dev, "Error copying from user");
			return ret;
		}

		if (unlikely(in_fmp_fips_err())) {
			dev_err(dev, "Fail to run fmp due to fips in error.");
			return -EPERM;
		}

		ret = fmp_run_AES_CBC_MCT(info, fcr, &kcop);
		if (unlikely(ret)) {
			dev_err(dev, "Error in fmp_run_AES_CBC_MCT");
			return ret;
		}
		return compat_kcop_to_user(info, &kcop, fcr, arg);
	default:
		dev_err(dev, "Invalid command : 0x%x\n", cmd);
		return -EINVAL;
	}
}
#endif /* CONFIG_COMPAT */

static int fmpdev_release(struct inode *inode, struct file *file)
{
	struct fmp_info *info = file->private_data;
	struct device *dev;
	struct todo_list_item *item, *item_safe;
	int items_freed = 0;

	if (!info) {
		printk(KERN_ERR "fmp info is already free.\n");
		return -ENOMEM;
	}

	dev = info->dev;
	cancel_work_sync(&info->fmptask);

	mutex_destroy(&info->todo.lock);
	mutex_destroy(&info->done.lock);
	mutex_destroy(&info->free.lock);

	list_splice_tail(&info->todo.list, &info->free.list);
	list_splice_tail(&info->done.list, &info->free.list);

	list_for_each_entry_safe(item, item_safe, &info->free.list, __hook) {
		dev_err(dev, "%s: freeing item at %lx\n",
				__func__, (unsigned long)item);
		list_del(&item->__hook);
		kfree(item);
		items_freed++;
	}
	if (items_freed != info->itemcount)
		dev_err(dev, "%s: freed %d items, but %d should exist!\n",
				__func__, items_freed, info->itemcount);

	fmp_finish_all_sessions(info, &info->fcrypt);
	kfree(info);
	file->private_data = NULL;

	dev_info(dev, "%s released.\n", dev_name(dev));

	return 0;
}

static const struct file_operations fmpdev_fops = {
	.owner		= THIS_MODULE,
	.open		= fmpdev_open,
	.release	= fmpdev_release,
	.unlocked_ioctl = fmpdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= fmpdev_compat_ioctl,
#endif
};

static struct miscdevice exynos_fmpdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "fmp",
	.fops	= &fmpdev_fops,
};

static int exynos_fips_fmp_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;

	ret = misc_register(&exynos_fmpdev);
	if (ret) {
		dev_err(dev, "Fail to register misc device. ret = %d\n", ret);
		goto misc_err;
	}

	ret = sysfs_create_group(&dev->kobj, &fips_fmp_attr_group);
	if (ret) {
		dev_err(dev, "Fail to create sysfs for fips fmp\n");
		goto sysfs_err;
	}

	match = of_match_node(exynos_fips_fmp_match, dev->of_node);
	if (!match) {
		dev_err(dev, "Fail to get match from device node\n");
		goto err;
	}

	set_fmp_fips_state(FMP_FIPS_SELFTEST_STATE);

	ret = fips_fmp_init(dev);
	if (ret) {
		dev_err(dev, "Fail to initialize fmp driver for selftest. ret = %d\n", ret);
		goto err;
	}

	ret = do_fips_fmp_selftest(dev);
	if (ret) {
		printk(KERN_ERR "FIPS: self-tests for UFSFMP failed\n");
		goto err;
	} else {
		printk(KERN_INFO "FIPS: self-tests for UFSFMP passed\n");
	}

	ret = do_fips_fmp_integrity_check();
	if (ret) {
		printk(KERN_ERR "FIPS: integrity check for UFSFMP failed\n");
		goto err;
	} else {
		printk(KERN_INFO "FIPS: integrity check for UFSFMP passed\n");
	}

	set_fmp_fips_state(FMP_FIPS_SUCCESS_STATE);
	fips_fmp_result = 1;

	return 0;

err:
#if defined(CONFIG_NODE_FOR_SELFTEST_FAIL)
	set_fmp_fips_state(FMP_FIPS_ERR_STATE);
	fips_fmp_result = 0;
	return 0;
#else
	panic("Panic due to FMP self test for FIPS KAT");
#endif

sysfs_err:
misc_err:
	return -1;
}

static int exynos_fips_fmp_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_group(&dev->kobj, &fips_fmp_attr_group);
	misc_deregister(&exynos_fmpdev);

	return 0;
}

static const struct of_device_id exynos_fips_fmp_match[] = {
	{ .compatible = "samsung,exynos-fips-fmp",
			.data = &fips_fmp_fops, },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fips_fmp_match);

static struct platform_driver exynos_fips_fmp_driver = {
	.driver = {
		.name = "exynos-fips-fmp",
		.owner = THIS_MODULE,
		.pm = NULL,
		.of_match_table = exynos_fips_fmp_match,
	},
	.probe = exynos_fips_fmp_probe,
	.remove = exynos_fips_fmp_remove,
};

static int __init exynos_fips_fmp_init(void)
{
	return platform_driver_register(&exynos_fips_fmp_driver);
}
late_initcall(exynos_fips_fmp_init);

static void __exit exynos_fips_fmp_exit(void)
{
	platform_driver_unregister(&exynos_fips_fmp_driver);
}
module_exit(exynos_fips_fmp_exit);

MODULE_DESCRIPTION("FIPS KAT for Exynos FMP AES module");
