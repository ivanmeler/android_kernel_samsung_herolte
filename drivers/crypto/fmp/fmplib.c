/*
 * Exynos FMP libary for FIPS
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/smc.h>

#include <crypto/hash.h>
#include <crypto/sha.h>

#include <asm/cacheflush.h>

#include "fmpdev_int.h"
#include "fmpdev.h"

#define BYPASS_MODE	0
#define CBC_MODE	1
#define XTS_MODE	2

#define INIT		0
#define UPDATE		1
#define FINAL		2

static void fmpdev_complete(struct crypto_async_request *req, int err)
{
	struct fmpdev_result *res = req->data;

	if (err == -EINPROGRESS)
		return;

	res->err = err;
	complete(&res->completion);
}

static inline int waitfor(struct fmp_info *info, struct fmpdev_result *cr,
			ssize_t ret)
{
	struct device *dev = info->dev;

	switch (ret) {
	case 0:
		break;
	case -EINPROGRESS:
	case -EBUSY:
		wait_for_completion(&cr->completion);
		/* At this point we known for sure the request has finished,
		 * because wait_for_completion above was not interruptible.
		 * This is important because otherwise hardware or driver
		 * might try to access memory which will be freed or reused for
		 * another request. */

		if (unlikely(cr->err)) {
			dev_err(dev, "error from async request: %d\n", cr->err);
			return cr->err;
		}

		break;
	default:
		return ret;
	}

	return 0;
}

/*
 * FMP library to call FMP driver
 *
 * This file is part of linux fmpdev
 */

int fmpdev_cipher_init(struct fmp_info *info, struct cipher_data *out,
			const char *alg_name,
			uint8_t *enckey, uint8_t *twkey, size_t keylen)
{
	int ret;
	struct device *dev = info->dev;

	memset(out, 0, sizeof(*out));

	if (!strcmp(alg_name, "cbc(aes-fmp)"))
		out->mode = CBC_MODE;
	else if (!strcmp(alg_name, "xts(aes-fmp)"))
		out->mode = XTS_MODE;
	else {
		dev_err(dev, "Invalid mode\n");
		return -1;
	}

	out->blocksize = 16;
	out->ivsize = 16;
	ret = fips_fmp_cipher_init(dev, enckey, twkey, keylen, out->mode);
	if (ret) {
		dev_err(dev, "Fail to initialize fmp cipher\n");
		return -1;
	}

	out->init = 1;
	return 0;
}

void fmpdev_cipher_deinit(struct cipher_data *cdata)
{
	if (cdata->init)
		cdata->init = 0;
}

int fmpdev_cipher_set_iv(struct fmp_info *info, struct cipher_data *cdata,
			uint8_t *iv, size_t iv_size)
{
	int ret;
	struct device *dev = info->dev;

	ret = fips_fmp_cipher_set_iv(dev, iv, cdata->mode);
	if (ret) {
		dev_err(dev, "Fail to set fmp iv\n");
		return -1;
	}

	return 0;
}

int fmpdev_cipher_exit(struct fmp_info *info)
{
	int ret;
	struct device *dev = info->dev;

	ret = fips_fmp_cipher_exit(dev);
	if (ret) {
		dev_err(dev, "Fail to exit fmp\n");
		return -1;
	}

	return 0;
}

static int fmpdev_cipher_encrypt(struct fmp_info *info,
		struct cipher_data *cdata,
		struct scatterlist *src,
		struct scatterlist *dst, size_t len)
{
	int ret;
	struct device *dev = info->dev;

	ret = fips_fmp_cipher_run(dev, sg_virt(src), sg_virt(dst),
				len, cdata->mode, ENCRYPT);
	if (ret) {
		dev_err(dev, "Fail to encrypt using fmp\n");
		return -1;
	}

	return 0;
}

static int fmpdev_cipher_decrypt(struct fmp_info *info,
		struct cipher_data *cdata,
		struct scatterlist *src,
		struct scatterlist *dst, size_t len)
{
	int ret;
	struct device *dev = info->dev;

	ret = fips_fmp_cipher_run(dev, sg_virt(src), sg_virt(dst),
				len, cdata->mode, DECRYPT);
	if (ret) {
		dev_err(dev, "Fail to encrypt using fmp\n");
		return -1;
	}

	return 0;
}

/* Hash functions */
int fmpfw_hash_init(struct fmp_info *info, struct hash_data *hdata,
			const char *alg_name, int hmac_mode,
			void *mackey, size_t mackeylen)
{
	int ret;
	unsigned long addr;
	struct device *dev = info->dev;
	struct hmac_sha256_fmpfw_info *fmpfw_info;

	fmpfw_info = (struct hmac_sha256_fmpfw_info *)kzalloc(sizeof(*fmpfw_info), GFP_KERNEL);
	if (unlikely(!fmpfw_info)) {
		dev_err(dev, "Fail to alloc fmpfw info\n");
		ret = ENOMEM;
		goto error_alloc_info;
	}

	if (hmac_mode != 0) {
		fmpfw_info->s.step = INIT;
		fmpfw_info->hmac_mode = 1;
		fmpfw_info->key = (uint32_t)virt_to_phys(mackey);
		__flush_dcache_area(mackey, mackeylen);
		fmpfw_info->key_len = mackeylen;
		__flush_dcache_area(fmpfw_info, sizeof(*fmpfw_info));
		addr = virt_to_phys(fmpfw_info);
		ret = exynos_smc(SMC_CMD_FMP, FMP_FW_HMAC_SHA2_TEST, (uint32_t)addr, 0);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to smc call for FMPFW HMAC SHA256 init. ret = 0x%x\n", ret);
			ret = -EFAULT;
			goto error_hmac_smc;
		}
	} else {
		fmpfw_info->s.step = INIT;
		fmpfw_info->hmac_mode = 0;
		fmpfw_info->key = 0;
		fmpfw_info->key_len = 0;
		__flush_dcache_area(fmpfw_info, sizeof(*fmpfw_info));
		addr = virt_to_phys(fmpfw_info);

		ret = exynos_smc(SMC_CMD_FMP, FMP_FW_SHA2_TEST, (uint32_t)addr, 0);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to smc call for FMPFW SHA256 init. ret = 0x%x\n", ret);
			ret = -EFAULT;
			goto error_sha_smc;
		}
	}

	hdata->digestsize = SHA256_DIGEST_SIZE;
	hdata->alignmask = 0x0;
	hdata->fmpfw_info = fmpfw_info;

	hdata->async.result = kzalloc(sizeof(*hdata->async.result), GFP_KERNEL);
	if (unlikely(!hdata->async.result)) {
		ret = -ENOMEM;
		goto error;
	}

	init_completion(&hdata->async.result->completion);

	hdata->init = 1;

	return 0;

error_hmac_smc:
error_sha_smc:
	kfree(fmpfw_info);
error_alloc_info:
	hdata->fmpfw_info = NULL;
error:
	kfree(hdata->async.result);
	return ret;
}

void fmpfw_hash_deinit(struct hash_data *hdata)
{
	if (hdata->init) {
		kfree(hdata->async.result);
		hdata->init = 0;
	}
}

ssize_t fmpfw_hash_update(struct fmp_info *info, struct hash_data *hdata,
				struct scatterlist *sg, size_t len)
{
	int ret = 0;
	unsigned long addr;
	struct device *dev = info->dev;
	struct hmac_sha256_fmpfw_info *fmpfw_info = hdata->fmpfw_info;

	fmpfw_info->s.step = UPDATE;
	fmpfw_info->s.input = (uint32_t)sg_phys(sg);
	__flush_dcache_area(sg_virt(sg), len);
	fmpfw_info->s.input_len = len;
	__flush_dcache_area(fmpfw_info, sizeof(*fmpfw_info));
	addr = virt_to_phys(fmpfw_info);

	reinit_completion(&hdata->async.result->completion);
	if (fmpfw_info->hmac_mode) {
		ret = exynos_smc(SMC_CMD_FMP, FMP_FW_HMAC_SHA2_TEST, addr, 0);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to smc call for FMPFW HMAC SHA256 update. ret = 0x%x\n", ret);
			ret = -EFAULT;
		}
	} else {
		ret = exynos_smc(SMC_CMD_FMP, FMP_FW_SHA2_TEST, addr, 0);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to smc call for FMPFW SHA256 update. ret = 0x%x\n", ret);
			ret = -EFAULT;
		}
	}

	return waitfor(info, hdata->async.result, ret);
}

int fmpfw_hash_final(struct fmp_info *info, struct hash_data *hdata, void *output)
{
	int ret = 0;
	unsigned long addr;
	struct device *dev = info->dev;
	struct hmac_sha256_fmpfw_info *fmpfw_info = hdata->fmpfw_info;

	memset(output, 0, hdata->digestsize);
	fmpfw_info->s.step = FINAL;
	fmpfw_info->s.output = (uint32_t)virt_to_phys(output);
	__flush_dcache_area(fmpfw_info, sizeof(*fmpfw_info));
	addr = virt_to_phys(fmpfw_info);

	reinit_completion(&hdata->async.result->completion);
	__dma_unmap_area((void *)output, hdata->digestsize, DMA_FROM_DEVICE);
	if (fmpfw_info->hmac_mode) {
		ret = exynos_smc(SMC_CMD_FMP, FMP_FW_HMAC_SHA2_TEST, addr, 0);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to smc call for FMPFW HMAC SHA256 final. ret = 0x%x\n", ret);
			ret = -EFAULT;
		}
	} else {
		ret = exynos_smc(SMC_CMD_FMP, FMP_FW_SHA2_TEST, addr, 0);
		if (unlikely(ret)) {
			dev_err(dev, "Fail to smc call for FMPFW SHA256 final. ret = 0x%x\n", ret);
			ret = -EFAULT;
		}
	}
	__dma_unmap_area((void *)output, hdata->digestsize, DMA_FROM_DEVICE);

	if (fmpfw_info->hmac_mode)
		dev_info(dev, "fmp fw hmac sha256 F/W final is done\n");
	else
		dev_info(dev, "fmp fw sha256 F/W final is done\n");

	kfree(fmpfw_info);
	return waitfor(info, hdata->async.result, ret);
}

int fmpdev_hash_init(struct fmp_info *info, struct hash_data *hdata,
			const char *alg_name,
			int hmac_mode, void *mackey, size_t mackeylen)
{
	int ret;
	struct device *dev = info->dev;

	hdata->fmpfw_info = NULL;
	hdata->async.s = crypto_alloc_ahash(alg_name, 0, 0);
	if (unlikely(IS_ERR(hdata->async.s))) {
		dev_err(dev, "Failed to load transform for %s\n", alg_name);
		return -EINVAL;
	}

	/* Copy the key from user and set to TFM. */
	if (hmac_mode != 0) {
		ret = crypto_ahash_setkey(hdata->async.s, mackey, mackeylen);
		if (unlikely(ret)) {
			dev_err(dev, "Setting hmac key failed for %s-%zu.\n",
					alg_name, mackeylen*8);
			ret = -EINVAL;
			goto error;
		}
	}

	hdata->digestsize = crypto_ahash_digestsize(hdata->async.s);
	hdata->alignmask = crypto_ahash_alignmask(hdata->async.s);

	hdata->async.result = kzalloc(sizeof(*hdata->async.result), GFP_KERNEL);
	if (unlikely(!hdata->async.result)) {
		ret = -ENOMEM;
		goto error;
	}

	init_completion(&hdata->async.result->completion);

	hdata->async.request = ahash_request_alloc(hdata->async.s, GFP_KERNEL);
	if (unlikely(!hdata->async.request)) {
		dev_err(dev, "error allocating async crypto request\n");
		ret = -ENOMEM;
		goto error;
	}

	ahash_request_set_callback(hdata->async.request,
			CRYPTO_TFM_REQ_MAY_BACKLOG,
			fmpdev_complete, hdata->async.result);

	ret = crypto_ahash_init(hdata->async.request);
	if (unlikely(ret)) {
		dev_err(dev, "error in fmpdev_hash_init()\n");
		goto error_request;
	}

	hdata->init = 1;
	return 0;

error_request:
	ahash_request_free(hdata->async.request);
error:
	kfree(hdata->async.result);
	crypto_free_ahash(hdata->async.s);
	return ret;
}


void fmpdev_hash_deinit(struct hash_data *hdata)
{
	if (hdata->init) {
		if (hdata->async.request)
			ahash_request_free(hdata->async.request);
		kfree(hdata->async.result);
		if (hdata->async.s)
			crypto_free_ahash(hdata->async.s);
		hdata->init = 0;
	}
}

int fmpdev_hash_reset(struct fmp_info *info, struct hash_data *hdata)
{
	int ret;
	struct device *dev = info->dev;

	ret = crypto_ahash_init(hdata->async.request);
	if (unlikely(ret)) {
		dev_err(dev, "error in crypto_hash_init()\n");
		return ret;
	}

	return 0;

}

ssize_t fmpdev_hash_update(struct fmp_info *info, struct hash_data *hdata,
				struct scatterlist *sg, size_t len)
{
	int ret;

	reinit_completion(&hdata->async.result->completion);
	ahash_request_set_crypt(hdata->async.request, sg, NULL, len);

	ret = crypto_ahash_update(hdata->async.request);

	return waitfor(info, hdata->async.result, ret);
}

int fmpdev_hash_final(struct fmp_info *info, struct hash_data *hdata, void *output)
{
	int ret;

	reinit_completion(&hdata->async.result->completion);
	ahash_request_set_crypt(hdata->async.request, NULL, output, 0);

	ret = crypto_ahash_final(hdata->async.request);

	return waitfor(info, hdata->async.result, ret);
}

static int fmp_n_crypt(struct fmp_info *info, struct csession *ses_ptr,
		struct crypt_op *cop,
		struct scatterlist *src_sg, struct scatterlist *dst_sg,
		uint32_t len)
{
	int ret;
	struct device *dev = info->dev;

	if (cop->op == COP_ENCRYPT) {
		if (ses_ptr->hdata.init != 0) {
			if (!ses_ptr->hdata.fmpfw_info)
				ret = fmpdev_hash_update(info, &ses_ptr->hdata,
								src_sg, len);
			else
				ret = fmpfw_hash_update(info, &ses_ptr->hdata,
								src_sg, len);
			if (unlikely(ret))
				goto out_err;
		}

		if (ses_ptr->cdata.init != 0) {
			ret = fmpdev_cipher_encrypt(info, &ses_ptr->cdata,
						src_sg, dst_sg, len);
			if (unlikely(ret))
				goto out_err;
		}
	} else {
		if (ses_ptr->cdata.init != 0) {
			ret = fmpdev_cipher_decrypt(info, &ses_ptr->cdata,
						src_sg, dst_sg, len);
			if (unlikely(ret))
				goto out_err;
		}

		if (ses_ptr->hdata.init != 0) {
			if (!ses_ptr->hdata.fmpfw_info)
				ret = fmpdev_hash_update(info, &ses_ptr->hdata,
								dst_sg, len);
			else
				ret = fmpfw_hash_update(info, &ses_ptr->hdata,
								dst_sg, len);
			if (unlikely(ret))
				goto out_err;
		}
	}

	return 0;
out_err:
	dev_err(dev, "FMP crypt failure: %d\n", ret);

	return ret;
}

static int __fmp_run_std(struct fmp_info *info,
		struct csession *ses_ptr, struct crypt_op *cop)
{
	char *data;
	struct device *dev = info->dev;
	char __user *src, *dst;
	size_t nbytes, bufsize;
	struct scatterlist sg;
	int ret = 0;

	nbytes = cop->len;
	data = (char *)__get_free_page(GFP_KERNEL);
	if (unlikely(!data)) {
		dev_err(dev, "Error getting free page.\n");
		return -ENOMEM;
	}

	bufsize = PAGE_SIZE < nbytes ? PAGE_SIZE : nbytes;

	src = cop->src;
	dst = cop->dst;

	while (nbytes > 0) {
		size_t current_len = nbytes > bufsize ? bufsize : nbytes;

		if (unlikely(copy_from_user(data, src, current_len))) {
			dev_err(dev, "Error copying %d bytes from user address %p\n",
						(int)current_len, src);
			ret = -EFAULT;
			break;
		}

		sg_init_one(&sg, data, current_len);
		ret = fmp_n_crypt(info, ses_ptr, cop, &sg, &sg, current_len);
		if (unlikely(ret)) {
			dev_err(dev, "fmp_n_crypt failed\n");
			break;
		}

		if (ses_ptr->cdata.init != 0) {
			if (unlikely(copy_to_user(dst, data, current_len))) {
				dev_err(dev, "could not copy to user\n");
				ret = -EFAULT;
				break;
			}
		}

		dst += current_len;
		nbytes -= current_len;
		src += current_len;
	}
	free_page((unsigned long)data);

	return ret;
}


int fmp_run(struct fmp_info *info, struct fcrypt *fcr, struct kernel_crypt_op *kcop)
{
	struct device *dev = info->dev;
	struct csession *ses_ptr;
	struct crypt_op *cop = &kcop->cop;
	int ret = -EINVAL;

	if (unlikely(cop->op != COP_ENCRYPT && cop->op != COP_DECRYPT)) {
		dev_err(dev, "invalid operation op=%u\n", cop->op);
		return -EINVAL;
	}

	/* this also enters ses_ptr->sem */
	ses_ptr = fmp_get_session_by_sid(fcr, cop->ses);
	if (unlikely(!ses_ptr)) {
		dev_err(dev, "invalid session ID=0x%08X\n", cop->ses);
		return -EINVAL;
	}

	if ((ses_ptr->cdata.init != 0) && (cop->len > PAGE_SIZE)) {
		dev_err(dev, "Invalid input length. len = %d\n", cop->len);
		return -EINVAL;
	}

	if (ses_ptr->hdata.init != 0 && (cop->flags == 0 || cop->flags & COP_FLAG_RESET) && \
				!ses_ptr->hdata.fmpfw_info) {
		ret = fmpdev_hash_reset(info, &ses_ptr->hdata);
		if (unlikely(ret)) {
			dev_err(dev, "error in fmpdev_hash_reset()");
			goto out_unlock;
		}
	}

	if (ses_ptr->cdata.init != 0) {
		int blocksize = ses_ptr->cdata.blocksize;

		if (unlikely(cop->len % blocksize)) {
			dev_err(dev,
				"data size (%u) isn't a multiple "
				"of block size (%u)\n",
				cop->len, blocksize);
			ret = -EINVAL;
			goto out_unlock;
		}

		if (cop->flags == COP_FLAG_AES_CBC)
			fmpdev_cipher_set_iv(info, &ses_ptr->cdata, kcop->iv, 16);
		else if (cop->flags == COP_FLAG_AES_XTS)
			fmpdev_cipher_set_iv(info, &ses_ptr->cdata, (uint8_t *)&cop->data_unit_seqnumber, 16);
		else {
			ret = -EINVAL;
			goto out_unlock;
		}
	}

	if (likely(cop->len)) {
		ret = __fmp_run_std(info, ses_ptr, &kcop->cop);
		if (unlikely(ret))
			goto out_unlock;
	}

	if (ses_ptr->hdata.init != 0 &&
		((cop->flags & COP_FLAG_FINAL) ||
		   (!(cop->flags & COP_FLAG_UPDATE) || cop->len == 0))) {
		if (!ses_ptr->hdata.fmpfw_info)
			ret = fmpdev_hash_final(info, &ses_ptr->hdata, kcop->hash_output);
		else
			ret = fmpfw_hash_final(info, &ses_ptr->hdata, kcop->hash_output);
		if (unlikely(ret)) {
			dev_err(dev, "CryptoAPI failure: %d\n", ret);
			goto out_unlock;
		}
		kcop->digestsize = ses_ptr->hdata.digestsize;
	}

out_unlock:
	fmp_put_session(ses_ptr);
	return ret;
}

int fmp_run_AES_CBC_MCT(struct fmp_info *info, struct fcrypt *fcr,
			struct kernel_crypt_op *kcop)
{
	struct device *dev = info->dev;
	struct csession *ses_ptr;
	struct crypt_op *cop = &kcop->cop;
	char **Ct = 0;
	char **Pt = 0;
	int ret = 0, k = 0;

	if (unlikely(cop->op != COP_ENCRYPT && cop->op != COP_DECRYPT)) {
		dev_err(dev, "invalid operation op=%u\n", cop->op);
		return -EINVAL;
	}

	/* this also enters ses_ptr->sem */
	ses_ptr = fmp_get_session_by_sid(fcr, cop->ses);
	if (unlikely(!ses_ptr)) {
		dev_err(dev, "invalid session ID=0x%08X\n", cop->ses);
		return -EINVAL;
	}

	if (cop->len > PAGE_SIZE) {
		dev_err(dev, "Invalid input length. len = %d\n", cop->len);
		return -EINVAL;
	}

	if (ses_ptr->cdata.init != 0) {
		int blocksize = ses_ptr->cdata.blocksize;

		if (unlikely(cop->len % blocksize)) {
			dev_err(dev,
				"data size (%u) isn't a multiple "
				"of block size (%u)\n",
				cop->len, blocksize);
			ret = -EINVAL;
			goto out_unlock;
		}

		fmpdev_cipher_set_iv(info, &ses_ptr->cdata, kcop->iv, 16);
	}

	if (likely(cop->len)) {
		if (cop->flags & COP_FLAG_AES_CBC_MCT) {
		// do MCT here
		char *data;
	        char __user *src, *dst, *secondLast;
	        struct scatterlist sg;
	        size_t nbytes, bufsize;
	        int ret = 0;
	        int y = 0;

	        nbytes = cop->len;
	        data = (char *)__get_free_page(GFP_KERNEL);
		if (unlikely(!data)) {
			dev_err(dev, "Error getting free page.\n");
			return -ENOMEM;
		}

		Pt = (char**)kmalloc(1000 * sizeof(char*), GFP_KERNEL);
		for (k=0; k<1000; k++)
		       Pt[k]= (char*)kmalloc(nbytes, GFP_KERNEL);

		Ct = (char**)kmalloc(1000 * sizeof(char*), GFP_KERNEL);
		for (k=0; k<1000; k++)
		       Ct[k]= (char*)kmalloc(nbytes, GFP_KERNEL);

	        bufsize = PAGE_SIZE < nbytes ? PAGE_SIZE : nbytes;

	        src = cop->src;
	        dst = cop->dst;
	        secondLast = cop->secondLastEncodedData;

		if (unlikely(copy_from_user(data, src, nbytes))) {
			printk(KERN_ERR "Error copying %d bytes from user address %p.\n", (int)nbytes, src);
		        ret = -EFAULT;
		        goto out_err;
	        }

	        sg_init_one(&sg, data, nbytes);
		for (y = 0; y < 1000; y++) {
			memcpy(Pt[y], data, nbytes);
			ret = fmp_n_crypt(info, ses_ptr, cop, &sg, &sg, nbytes);
			memcpy(Ct[y], data, nbytes);

			if (y == 998) {
				if (unlikely(copy_to_user(secondLast, data, nbytes)))
					printk(KERN_ERR "unable to copy second last data for AES_CBC_MCT\n");
				else
					printk(KERN_ERR "KAMAL copied secondlast data\n");
			}

			if( y == 0) {
				memcpy(data, kcop->iv, kcop->ivlen);
			} else {
				if(y != 999)
					memcpy(data, Ct[y-1], nbytes);
			}

			if (unlikely(ret)) {
				printk(KERN_ERR "fmp_n_crypt failed.\n");
				goto out_err;
			}

			if (cop->op == COP_ENCRYPT)
				fmpdev_cipher_set_iv(info, &ses_ptr->cdata, Ct[y], 16);
			else if (cop->op == COP_DECRYPT)
				fmpdev_cipher_set_iv(info, &ses_ptr->cdata, Pt[y], 16);
		} // for loop

		if (ses_ptr->cdata.init != 0) {
			if (unlikely(copy_to_user(dst, data, nbytes))) {
				printk(KERN_ERR "could not copy to user.\n");
				ret = -EFAULT;
				goto out_err;
			}
		}

		for (k=0; k<1000; k++) {
			kfree(Ct[k]);
			kfree(Pt[k]);
		}

		kfree(Ct);
		kfree(Pt);
		free_page((unsigned long)data);
		} else
			goto out_unlock;
	}

out_unlock:
	fmp_put_session(ses_ptr);
	return ret;

out_err:
	fmp_put_session(ses_ptr);
	dev_info(dev, "CryptoAPI failure: %d\n", ret);

	return ret;
}
