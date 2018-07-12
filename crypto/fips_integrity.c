/*
 * Perform FIPS Integrity test on Kernel Crypto API
 * 
 * At build time, hmac(sha256) of crypto code, avaiable in different ELF sections 
 * of vmlinux file, is generated. vmlinux file is updated with built-time hmac
 * in a read-only data variable, so that it is available at run-time
 * 
 * At run time, hmac(sha256) is again calculated using crypto bytes of a running 
 * kernel. 
 * Run time hmac is compared to built time hmac to verify the integrity.
 *
 *
 * Author : Rohit Kothari (r.kothari@samsung.com) 
 * Date	  : 11 Feb 2014
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 */

#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include "internal.h" /* For Functional test macros */
#include "fips_helper.h"

static const char *
symtab[][3] = {{".text",      "first_crypto_text",   "last_crypto_text"  },
	      {".rodata",    "first_crypto_rodata", "last_crypto_rodata"},
	      {".init.text", "first_crypto_init",   "last_crypto_init"  },
	      {"asm.text",      "first_crypto_asm_text",   "last_crypto_asm_text"  },
	      {"asm.rodata",    "first_crypto_asm_rodata", "last_crypto_asm_rodata"},
	      {"asm.init.text", "first_crypto_asm_init",   "last_crypto_asm_init"  }};

extern const char * get_builtime_crypto_hmac(void);

#ifdef FIPS_DEBUG
static int
dump_bytes(const char * section_name, const char * first_symbol, const char * last_symbol)
{
	u8 * start_addr = (u8 *) get_fips_symbol_address (first_symbol);
	u8 * end_addr   = (u8 *) get_fips_symbol_address (last_symbol);

	if (!start_addr || !end_addr || start_addr >= end_addr)
	{
		printk(KERN_ERR "FIPS(%s): Error Invalid Addresses in Section : %s, Start_Addr : %p , End_Addr : %p", 
                       __FUNCTION__,section_name, start_addr, end_addr);
		return -1;
	}
	
	printk(KERN_INFO "FIPS CRYPTO RUNTIME : Section - %s, %s : %p, %s : %p \n", section_name, first_symbol, start_addr, last_symbol, end_addr);

	print_hex_dump_bytes ("FIPS CRYPTO RUNTIME : ",DUMP_PREFIX_NONE, start_addr, end_addr - start_addr);

	return 0;
}
#endif


static int
query_symbol_addresses (const char * first_symbol, const char * last_symbol, 
                        unsigned long * start_addr,unsigned long * end_addr)
{
	unsigned long start = get_fips_symbol_address (first_symbol);
	unsigned long end   = get_fips_symbol_address (last_symbol);

#ifdef FIPS_DEBUG
	printk(KERN_INFO "FIPS CRYPTO RUNTIME :  %s : %p, %s : %p\n", first_symbol, (u8*)start, last_symbol, (u8*)end);
#endif

	if (!start || !end || start >= end)
	{
		printk(KERN_ERR "FIPS(%s): Error Invalid Addresses.", __FUNCTION__);
		return -1;
	}

	*start_addr = start;
	*end_addr   = end;
	
	return 0;

}

static int 
init_hash (struct hash_desc * desc) 
{
	struct crypto_hash * tfm = NULL;
	int ret = -1;

	/* Same as build time */
	const unsigned char * key = "The quick brown fox jumps over the lazy dog";

	tfm = crypto_alloc_hash ("hmac(sha256)", 0, 0);

	if (IS_ERR(tfm)) {
		printk(KERN_ERR "FIPS(%s): integ failed to allocate tfm %ld", __FUNCTION__, PTR_ERR(tfm));
		return -1;
	}

	ret = crypto_hash_setkey (tfm, key, strlen(key));

	if (ret) {
		printk(KERN_ERR "FIPS(%s): fail at crypto_hash_setkey", __FUNCTION__);		
		return -1;
	}

	desc->tfm   = tfm;
	desc->flags = 0;

	ret = crypto_hash_init (desc);

	if (ret) {
		printk(KERN_ERR "FIPS(%s): fail at crypto_hash_init", __FUNCTION__);		
		return -1;
	}
	
	return 0;
}

static int
finalize_hash (struct hash_desc *desc, unsigned char * out, unsigned int out_size)
{
	int ret = -1;

	if (!desc || !desc->tfm || !out || !out_size)
	{
		printk(KERN_ERR "FIPS(%s): Invalid args", __FUNCTION__);		
		return ret;
	}

	if (crypto_hash_digestsize(desc->tfm) > out_size)
	{
		printk(KERN_ERR "FIPS(%s): Not enough space for digest", __FUNCTION__);		
		return ret;
	}

	ret = crypto_hash_final (desc, out);

	if (ret)
	{
		printk(KERN_ERR "FIPS(%s): crypto_hash_final failed", __FUNCTION__);		
		return -1;
	}

	return 0;
}

static int
update_hash (struct hash_desc * desc, unsigned char * start_addr, unsigned int size)
{
	struct scatterlist sg;
	unsigned char * buf = NULL;
	unsigned char * cur = NULL;
	unsigned int bytes_remaining;
	unsigned int bytes;
	int ret = -1;
#if FIPS_FUNC_TEST == 2
	static int total = 0;
#endif

	buf = kmalloc (PAGE_SIZE, GFP_KERNEL);

	if (!buf)
	{
		printk(KERN_ERR "FIPS(%s): kmalloc failed", __FUNCTION__);		
		return ret;
	}

	bytes_remaining = size;
	cur = start_addr;

	while (bytes_remaining > 0)
	{
		if (bytes_remaining >= PAGE_SIZE)
			bytes = PAGE_SIZE;
		else
			bytes = bytes_remaining;

		memcpy (buf, cur, bytes);

		sg_init_one (&sg, buf, bytes);

#if FIPS_FUNC_TEST == 2
		if (total == 0)
		{
			printk(KERN_INFO "FIPS : Failing Integrity Test");		
			buf[bytes / 2] += 1;
		}
#endif

		ret = crypto_hash_update (desc, &sg, bytes);

		if (ret)
		{
			printk(KERN_ERR "FIPS(%s): crypto_hash_update failed", __FUNCTION__);		
			kfree(buf);
			buf = 0;
			return -1;
		}

		cur += bytes;

		bytes_remaining -= bytes;

#if FIPS_FUNC_TEST == 2
		total += bytes;
#endif
	}

	//printk(KERN_INFO "FIPS : total bytes = %d\n", total);		

	if (buf)
        {
		kfree(buf);
		buf = 0;
        }

	return 0;
}


int
do_integrity_check (void)
{
	int i,rows, err;
	unsigned long start_addr = 0;
	unsigned long end_addr   = 0;
	unsigned char runtime_hmac[32];
	struct hash_desc desc;
	const char * builtime_hmac = 0;
	unsigned int size = 0;

	err = init_hash (&desc);

	if (err)
	{
		printk (KERN_ERR "FIPS(%s): init_hash failed", __FUNCTION__);
		return -1;
	}

	rows = (unsigned int) sizeof (symtab) / sizeof (symtab[0]);

	for (i = 0; i < rows; i++)
	{
		err = query_symbol_addresses (symtab[i][1], symtab[i][2], &start_addr, &end_addr);

		if (err)
		{
			printk (KERN_ERR "FIPS(%s): Error to get start / end addresses", __FUNCTION__);
			crypto_free_hash (desc.tfm);
			return -1;
		}

#ifdef FIPS_DEBUG
		dump_bytes(symtab[i][0],  symtab[i][1], symtab[i][2]);
#endif

		size = end_addr - start_addr;

		err = update_hash (&desc, (unsigned char *)start_addr, size);	

		if (err)
		{
			printk (KERN_ERR "FIPS(%s): Error to update hash", __FUNCTION__);
			crypto_free_hash (desc.tfm);
			return -1;
		}
	}

	err = finalize_hash (&desc, runtime_hmac, sizeof(runtime_hmac));

	crypto_free_hash (desc.tfm);

	if (err)
	{
		printk (KERN_ERR "FIPS(%s): Error in finalize", __FUNCTION__);
		return -1;
	}


	builtime_hmac =  get_builtime_crypto_hmac();

	if (!builtime_hmac)
	{
		printk (KERN_ERR "FIPS(%s): Unable to retrieve builtime_hmac", __FUNCTION__);
		return -1;
	}

#ifdef FIPS_DEBUG
	print_hex_dump_bytes ("FIPS CRYPTO RUNTIME : runtime hmac  = ",DUMP_PREFIX_NONE, runtime_hmac, sizeof(runtime_hmac));
	print_hex_dump_bytes ("FIPS CRYPTO RUNTIME : builtime_hmac = ",DUMP_PREFIX_NONE, builtime_hmac , sizeof(runtime_hmac));
#endif

	if (!memcmp (builtime_hmac, runtime_hmac, sizeof(runtime_hmac))) 
	{
		printk (KERN_INFO "FIPS: Integrity Check Passed");
		return 0;
	}
	else
	{
		printk (KERN_ERR "FIPS(%s): Integrity Check Failed", __FUNCTION__);
		set_in_fips_err();
		return -1;
	}

	return -1;
}

EXPORT_SYMBOL_GPL(do_integrity_check);
