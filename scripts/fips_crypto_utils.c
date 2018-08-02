/*
 * Utility functions called from fips_crypto_hmac.sh.
 *
 * executed during Kernel build
 * 
 *
 * Author : Rohit Kothari (r.kothari@samsung.com) 
 * Date	  : 11 Feb 2014
 *
 * Copyright (c) 2014 Samsung Electronics
 * 
 */

#include <stdio.h>
#include <stdlib.h>
 
int main (int argc, char **argv)
{
	if (argc < 2)
	{
		printf ("\nUsage : \n");
		printf ("fips_crypto_utils -u vmlinux_file hmac_file offset");
		printf ("fips_crypto_utils -g vmlinux_file section_name offset size out_file");
		printf ("\n");
		return -1;
	}

	if (!strcmp ("-u", argv[1]))
	{
		unsigned long offset = 0;
		unsigned char * vmlinux_file = NULL;
		unsigned char * hmac_file    = NULL;

		if (argc != 5)
		{
			printf ("\nUsage : \n");
			printf ("fips_crypto_utils -u vmlinux_file hmac_file offset");
			printf ("\n");
			return -1;
		}
		
		vmlinux_file = argv[2];
		hmac_file    = argv[3];
		offset       = atol(argv[4]);
		
		if (!vmlinux_file || !hmac_file || !offset)
		{
			printf ("./fips_crypto_utils -u vmlinux_file hmac_file offset");
			return -1;
		}

		return update_crypto_hmac (vmlinux_file, hmac_file, offset);
	}
	else if (!strcmp ("-g", argv[1]))
	{
		const char * in_file      = NULL;
		const char * section_name = NULL;
		unsigned long offset      = 0;
		unsigned long size        = 0;
		const char * out_file     = NULL;

		if (argc != 7)
		{
			printf ("\nUsage : \n");
			printf ("./fips_crypto_utils -g vmlinux_file section_name offset size out_file");
			printf ("\n");
			return -1;
		}

		in_file      = argv[2];
		section_name = argv[3];
		offset       = atol(argv[4]);
		size         = atol(argv[5]);
		out_file     = argv[6];

		if (!in_file || !section_name || !offset || !size || !out_file)
		{
			printf ("./fips_crypto_utils -g vmlinux_file section_name offset size out_file");
			return -1;
		}

		return collect_crypto_bytes (in_file, section_name, offset, size, out_file);
	}
	else
	{
		printf ("\nUsage : \n");
		printf ("fips_crypto_utils -u vmlinux_file hmac_file offset");
		printf ("fips_crypto_utils -g vmlinux_file section_name offset size out_file");
		printf ("\n");
	}

	return -1;
}

/*
 * Given a vmlinux file, dumps "size" bytes from given "offset" to output file
 * in_file      : absolute path to vmlinux file
 * section_name : Used only for printing / debugging 
 * offset       : offset in file from where to dump bytes
 * size         : how many bytes to dump
 * out_file     : Output file, where to dump bytes.
 *                Open in append mode, to keep previous bytes, if present
 *                Caller need to clean up before 1st call
 *
 * Returns 0, if success
 *        -1, if error
 */

int 
collect_crypto_bytes (const char * in_file, const char * section_name, unsigned long offset, 
                      unsigned long size, const char * out_file)
{
	FILE * in_fp  = NULL;
	FILE * out_fp = NULL;
	unsigned int i     = 0;
	unsigned char data = 0;

	if (!in_file || !section_name || !offset || !size || !out_file)
	{
		printf ("collect_crypto_bytes : Invalid arguments");
		return -1;
	}

	printf ("Section : %s\n", section_name);

	in_fp = fopen (in_file, "r");
	if (!in_fp)
	{
		printf ("Unable to open file : %s", in_file);
		return -1;
	}	

	if (fseek (in_fp, offset, SEEK_SET) != 0 )
	{
		printf ("Unable to seek file : %s", in_file);
		fclose (in_fp);
		return -1;
	}

	out_fp = fopen (out_file, "ab");
	if (!out_fp)
	{
		printf ("Unable to open file : %s", out_file);
		fclose(in_fp);
		return -1;
	}	

	for (i = 1; i <= size; i++)
	{
		if ( 1 != fread (&data, sizeof(unsigned char), 1, in_fp))
		{
			printf ("Unable to read 1 byte from  file : %s", in_file);
			fclose (in_fp);
			fclose (out_fp);
			return -1;
		}

		printf ("%02x ", data);

		if (1 != fwrite (&data, 1, 1, out_fp))
		{
			printf ("Unable to write 1 byte to file : %s", out_file);
			fclose (in_fp);
			fclose (out_fp);
			return -1;
		}

		if ( !(i % 16))
			printf ("\n");
	}

	fclose (in_fp);
	fclose (out_fp);

	return 0;
}


#define SHA256_DIGEST_SIZE 32

/*  
 * Given a vmlinux file, overwrites bytes at given offset with hmac bytes, available in 
 * hmac file.
 * Return 0, if Success
 *       -1, if Error
 */
int
update_crypto_hmac (const char * vmlinux_path, const char * hmac_path, unsigned long offset)
{
	FILE * vmlinux_fp = NULL;
	FILE * hmac_fp = NULL;
	int i = 0, j = 0;
	unsigned char hmac[SHA256_DIGEST_SIZE];

	if (!vmlinux_path || !hmac_path || !offset)
	{
		printf ("FIPS update_crypto_hmac : Invalid Params");
		return -1;
	}

	vmlinux_fp  = fopen (vmlinux_path, "r+b");
	if (!vmlinux_fp)
	{
		printf ("Unable to open vmlinux file ");
		return -1;
	}	

	hmac_fp = fopen (hmac_path, "rb"); 

	if (!hmac_fp)
	{
		printf ("Unable to open hmac file ");
		fclose (vmlinux_fp);
		return -1;
	}	

	if (SHA256_DIGEST_SIZE != fread (&hmac, sizeof(unsigned char), SHA256_DIGEST_SIZE, hmac_fp)) 
	{
		printf ("Unable to read %d bytes from hmac file", SHA256_DIGEST_SIZE);
		fclose (hmac_fp);
		fclose (vmlinux_fp);
		return -1;
	}

#if 0
	printf ("Hash : ");
	for (i = 0; i < sizeof(hmac); i++)
		printf ("%02x ", hmac[i]);
	printf ("\n");

	printf ("Offset : %ld", offset);
#endif

	if (fseek (vmlinux_fp, offset, SEEK_SET) != 0 )
	{
		printf ("Unable to seek into vmlinux file.");
		fclose (hmac_fp);
		fclose (vmlinux_fp);
		return -1;
	}

	if (SHA256_DIGEST_SIZE !=  fwrite (hmac, sizeof(unsigned char), SHA256_DIGEST_SIZE, vmlinux_fp))
	{
		printf ("Unable to write %d byte into vmlinux", SHA256_DIGEST_SIZE);
		fclose (hmac_fp);
		fclose (vmlinux_fp);
		return -1;
	}

	fclose (vmlinux_fp);
	fclose (hmac_fp);

	return 0;
}