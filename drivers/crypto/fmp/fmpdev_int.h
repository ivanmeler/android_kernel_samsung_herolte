/*
 * Exynos FMP device header for FIPS
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __FMPDEV_INT_H__
#define __FMPDEV_INT_H__

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/scatterlist.h>
#include <crypto/aead.h>

#include "fmpdev_info.h"

/* Used during FIPS Functional test with CMT Lab
 * FIPS_FMP_FUNC_TEST 0 - Normal operation
 * FIPS_FMP_FUNC_TEST 1 - Fail FMP H/W AES XTS Self test
 * FIPS_FMP_FUNC_TEST 2 - Fail FMP H/W AES CBC Self test
 * FIPS_FMP_FUNC_TEST 3 - Fail FMP DRV SHA256 Self test
 * FIPS_FMP_FUNC_TEST 4 - Fail FMP DRV HMAC Self test
 * FIPS_FMP_FUNC_TEST 5 - Fail FMP DRV Integrity test
 * FIPS_FMP_FUNC_TEST 6 - Enable FMP Key zeroization logs
 */
#define FIPS_FMP_FUNC_TEST 0

struct fcrypt {
	struct list_head list;
	struct mutex sem;
};

/* kernel-internal extension to struct crypt_op */
struct kernel_crypt_op {
	struct crypt_op cop;

	int ivlen;
	__u8 iv[EALG_MAX_BLOCK_LEN];

	int digestsize;
	uint8_t hash_output[AALG_MAX_RESULT_LEN];

	struct task_struct *task;
	struct mm_struct *mm;
};

struct todo_list_item {
	struct list_head __hook;
	struct kernel_crypt_op kcop;
	int result;
};

struct locked_list {
	struct list_head list;
	struct mutex lock;
};

struct fmp_info {
	int init_status;
	struct device *dev;
	struct fcrypt fcrypt;
	struct locked_list free, todo, done;
	int itemcount;
	struct work_struct fmptask;
	wait_queue_head_t user_waiter;
};

/* compatibility stuff */
#ifdef CONFIG_COMPAT
#include <linux/compat.h>

/* input of FMPGSESSION */
struct compat_session_op {
	/* Specify either cipher or mac
	 */
	uint32_t	cipher;		/* cryptodev_crypto_op_t */
	uint32_t	mac;		/* cryptodev_crypto_op_t */

	uint32_t	keylen;
	compat_uptr_t	key;		/* pointer to key data */
	uint32_t	mackeylen;
	compat_uptr_t	mackey;		/* pointer to mac key data */

	uint32_t	ses;		/* session identifier */
};

/* input of FMPCRYPT */
 struct compat_crypt_op {
	uint32_t	ses;		/* session identifier */
	uint16_t	op;		/* COP_ENCRYPT or COP_DECRYPT */
	uint16_t	flags;		/* see COP_FLAG_* */
	uint32_t	len;		/* length of source data */
	compat_uptr_t	src;		/* source data */
	compat_uptr_t	dst;		/* pointer to output data */
	compat_uptr_t	mac;/* pointer to output data for hash/MAC operations */
	compat_uptr_t	iv;/* initialization vector for encryption operations */
	compat_uptr_t secondLastEncodedData;
	compat_uptr_t thirdLastEncodedData;
};

#define COMPAT_FMPGSESSION    _IOWR('c', 100, struct compat_session_op)
#define COMPAT_FMPCRYPT       _IOWR('c', 101, struct compat_crypt_op)
#define COMPAT_FMP_AES_CBC_MCT	_IOWR('c', 102, struct compat_crypt_op)
#endif

/* the maximum of the above */
#define EALG_MAX_BLOCK_LEN	16

struct cipher_data {
	int init; /* 0 uninitialized */
	int blocksize;
	int aead;
	int stream;
	int ivsize;
	int alignmask;
	struct {
		/* block ciphers */
		struct crypto_ablkcipher *s;
		struct ablkcipher_request *request;

		/* AEAD ciphers */
		struct crypto_aead *as;
		struct aead_request *arequest;

		struct fmpdev_result *result;
		uint8_t iv[EALG_MAX_BLOCK_LEN];
	} async;
	int mode;
};

/* Hash */
struct sha256_fmpfw_info {
	uint32_t input;
	size_t input_len;
	uint32_t output;
	uint32_t step;
};

struct hmac_sha256_fmpfw_info {
	struct sha256_fmpfw_info s;
	uint32_t key;
	size_t key_len;
	uint32_t hmac_mode;
	uint32_t dummy;
};

struct hash_data {
	int init; /* 0 uninitialized */
	int digestsize;
	int alignmask;
	struct {
		struct crypto_ahash *s;
		struct fmpdev_result *result;
		struct ahash_request *request;
	} async;
	struct hmac_sha256_fmpfw_info *fmpfw_info;
};

struct fmpdev_result {
	struct completion completion;
	int err;
};

/* other internal structs */
struct csession {
	struct list_head entry;
	struct mutex sem;
	struct cipher_data cdata;
	struct hash_data hdata;
	uint32_t sid;
	uint32_t alignmask;

	unsigned int array_size;
	unsigned int used_pages; /* the number of pages that are used */
	/* the number of pages marked as writable (first are the readable) */
	unsigned int readable_pages;
	struct page **pages;
	struct scatterlist *sg;
};

struct csession *fmp_get_session_by_sid(struct fcrypt *fcr, uint32_t sid);

inline static void fmp_put_session(struct csession *ses_ptr)
{
	mutex_unlock(&ses_ptr->sem);
}
int adjust_sg_array(struct csession *ses, int pagecount);

#define MAX_TAP		8
#define XBUFSIZE	8

struct cipher_testvec {
	char *key;
	char *iv;
	char *input;
	char *result;
	unsigned short tap[MAX_TAP];
	int np;
	unsigned char also_non_np;
	unsigned char klen;
	unsigned short ilen;
	unsigned short rlen;
};

struct hash_testvec {
	/* only used with keyed hash algorithms */
	char *key;
	char *plaintext;
	char *digest;
	unsigned char tap[MAX_TAP];
	unsigned short psize;
	unsigned char np;
	unsigned char ksize;
};

struct cipher_test_suite {
	struct {
		struct cipher_testvec *vecs;
		unsigned int count;
	} enc;
};

struct hash_test_suite {
	struct hash_testvec *vecs;
	unsigned int count;
};

struct fips_fmp_ops {
	int (*init)(struct device *dev, uint32_t mode);
	int (*set_key)(struct device *dev, uint32_t mode, uint8_t *key, uint32_t key_len);
	int (*set_iv)(struct device *dev, uint32_t mode, uint8_t *iv, uint32_t iv_len);
	int (*run)(struct device *dev, uint32_t mode, uint8_t *data, uint32_t len, uint32_t write);
	int (*exit)(void);
};

#endif
