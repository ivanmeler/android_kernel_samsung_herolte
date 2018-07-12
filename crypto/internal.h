/*
 * Cryptographic API.
 *
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 * Copyright (c) 2005 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */
#ifndef _CRYPTO_INTERNAL_H
#define _CRYPTO_INTERNAL_H

#include <crypto/algapi.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/fips.h>

/*
 * Use this only for FIPS Functional Test with CMT Lab.
 * FIPS_FUNC_TEST 1 will make self algorithm test (ecb aes) fail
 * FIPS_FUNC_TEST 12 will make self algorithm test (hmac sha1) fail
 * FIPS_FUNC_TEST 2 will make integrity check fail by corrupting the
 * kernel image
 * FIPS_FUNC_TEST 3 will make sure all the logs needed in no error mode
 * FIPS_FUNC_TEST 4 will make the necessary dumps for zeroization test
 * FIPS_FUNC_TEST 5 will make the continous PRNG test fail
 * FIPS_FUNC_TEST 6 will make the SHA1 test fail
 * FIPS_FUNC_TEST 7 will make the TDES test fail
 * FIPS_FUNC_TEST 8 will make the RNG test fail
 * FIPS_FUNC_TEST 91 will make the drbg_pr_ctr_aes128 test fail
 * FIPS_FUNC_TEST 92 will make the drbg_pr_sha256 test fail
 * FIPS_FUNC_TEST 93 will make the drbg_pr_hmac_sha256 test fail
 * FIPS_FUNC_TEST 94 will make the continous PRNG test fail for DRBG
 * FIPS_FUNC_TEST 100 will make the AES GCM self test fail
 */

#define FIPS_FUNC_TEST 0
/* Crypto notification events. */
enum {
	CRYPTO_MSG_ALG_REQUEST,
	CRYPTO_MSG_ALG_REGISTER,
	CRYPTO_MSG_ALG_UNREGISTER,
	CRYPTO_MSG_TMPL_REGISTER,
	CRYPTO_MSG_TMPL_UNREGISTER,
};

struct crypto_instance;
struct crypto_template;

struct crypto_larval {
	struct crypto_alg alg;
	struct crypto_alg *adult;
	struct completion completion;
	u32 mask;
};

extern struct list_head crypto_alg_list;
extern struct rw_semaphore crypto_alg_sem;
extern struct blocking_notifier_head crypto_chain;

#ifdef CONFIG_PROC_FS

#ifdef CONFIG_CRYPTO_FIPS
bool in_fips_err(void);
void set_in_fips_err(void);
void crypto_init_proc(int *fips_error);
int do_integrity_check(void);
int testmgr_crypto_proc_init(void);
#else
void __init crypto_init_proc(void);
#endif
void __exit crypto_exit_proc(void);
#else
static inline void crypto_init_proc(void)
{ }
static inline void crypto_exit_proc(void)
{ }
#endif

static inline unsigned int crypto_cipher_ctxsize(struct crypto_alg *alg)
{
	return alg->cra_ctxsize;
}

static inline unsigned int crypto_compress_ctxsize(struct crypto_alg *alg)
{
	return alg->cra_ctxsize;
}

struct crypto_alg *crypto_mod_get(struct crypto_alg *alg);
struct crypto_alg *crypto_alg_lookup(const char *name, u32 type, u32 mask);
struct crypto_alg *crypto_alg_mod_lookup(const char *name, u32 type, u32 mask);

int crypto_init_cipher_ops(struct crypto_tfm *tfm);
int crypto_init_compress_ops(struct crypto_tfm *tfm);

void crypto_exit_cipher_ops(struct crypto_tfm *tfm);
void crypto_exit_compress_ops(struct crypto_tfm *tfm);

struct crypto_larval *crypto_larval_alloc(const char *name, u32 type, u32 mask);
void crypto_larval_kill(struct crypto_alg *alg);
struct crypto_alg *crypto_larval_lookup(const char *name, u32 type, u32 mask);
void crypto_alg_tested(const char *name, int err);

void crypto_remove_spawns(struct crypto_alg *alg, struct list_head *list,
			  struct crypto_alg *nalg);
void crypto_remove_final(struct list_head *list);
void crypto_shoot_alg(struct crypto_alg *alg);
struct crypto_tfm *__crypto_alloc_tfm(struct crypto_alg *alg, u32 type,
				      u32 mask);
void *crypto_create_tfm(struct crypto_alg *alg,
			const struct crypto_type *frontend);
struct crypto_alg *crypto_find_alg(const char *alg_name,
				   const struct crypto_type *frontend,
				   u32 type, u32 mask);
void *crypto_alloc_tfm(const char *alg_name,
		       const struct crypto_type *frontend, u32 type, u32 mask);

int crypto_register_notifier(struct notifier_block *nb);
int crypto_unregister_notifier(struct notifier_block *nb);
int crypto_probing_notify(unsigned long val, void *v);

static inline struct crypto_alg *crypto_alg_get(struct crypto_alg *alg)
{
	atomic_inc(&alg->cra_refcnt);
	return alg;
}

static inline void crypto_alg_put(struct crypto_alg *alg)
{
	if (atomic_dec_and_test(&alg->cra_refcnt) && alg->cra_destroy)
		alg->cra_destroy(alg);
}

static inline int crypto_tmpl_get(struct crypto_template *tmpl)
{
	return try_module_get(tmpl->module);
}

static inline void crypto_tmpl_put(struct crypto_template *tmpl)
{
	module_put(tmpl->module);
}

static inline int crypto_is_larval(struct crypto_alg *alg)
{
	return alg->cra_flags & CRYPTO_ALG_LARVAL;
}

static inline int crypto_is_dead(struct crypto_alg *alg)
{
	return alg->cra_flags & CRYPTO_ALG_DEAD;
}

static inline int crypto_is_moribund(struct crypto_alg *alg)
{
	return alg->cra_flags & (CRYPTO_ALG_DEAD | CRYPTO_ALG_DYING);
}

static inline void crypto_notify(unsigned long val, void *v)
{
	blocking_notifier_call_chain(&crypto_chain, val, v);
}

#endif	/* _CRYPTO_INTERNAL_H */

