/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/smc.h>
#include <linux/dma-direction.h>
#include <asm/cacheflush.h>
#include <linux/exynos_otp.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/vmalloc.h>


#ifdef OTP_TEST
static unsigned char ref_data[256] = {	/* guide 141117 v17 */
	0x30, 0x44, 0x01, 0x01, 0x29, 0xf4, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x44,		/*   0...15 */
	0x01, 0x01, 0x29, 0xf4, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x43, 0x01, 0x00,		/*  16...31 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x43, 0x01, 0x00, 0x0c, 0x0f,		/*  32...47 */
	0x0f, 0xff, 0xff, 0x0d, 0x0f, 0x0f, 0xff, 0xff, 0x00, 0x00, 0x01, 0x02, 0x0c, 0x0f, 0x0f, 0xff,		/*  48...63 */
	0xff, 0x0d, 0x0f, 0x0f, 0xff, 0xff, 0x33, 0x43, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/*  64...79 */
	0x00, 0x00, 0x00, 0x00, 0x42, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/*  80...95 */
	0x4c, 0x4c, 0x00, 0x02, 0x12, 0x08, 0x17, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/*  96..111 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 112..127 */
	0x57, 0x50, 0x00, 0x02, 0x12, 0x08, 0x17, 0x14, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00,		/* 128..143 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 144..159 */
	0x4d, 0x50, 0x00, 0x02, 0x12, 0x08, 0x17, 0x14, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00,		/* 160..175 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 176..191 */
	0x44, 0x48, 0x00, 0x02, 0x17, 0x1f, 0x18, 0x7f, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00,		/* 192..207 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 208..223 */
	0x50, 0x44, 0x00, 0x03, 0x07, 0x0f, 0x08, 0x0f, 0x09, 0x0f, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00,		/* 224..239 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 240..255 */
};
#endif

#define magic_offset(m, o)	\
	{			\
		.magic	= m,	\
		.offset	= o,	\
	}

#define MAX_TUNEBITS_SIZE	16
struct otp_struct {
	const u16		magic;
	u8			dt_type;	/* 0: byte, 1: word */
	u8			nr_data;	/* index count */
	const unsigned int	offset;		/* byte offset */
	bool			tuned;
	struct tune_bits	data[MAX_TUNEBITS_SIZE];
};

static bool otp_read_done;
static unsigned int nr_ipblk;
static unsigned int tune_bits_size;
static unsigned char *otp_data;
static struct otp_struct *exynos_otp_struct;

static void otp_parse_tune_bits(void)
{
	int i, j, offset;
	struct otp_struct *ipblk;
	unsigned char *ipblk_offset;

	for (i = 0; i < nr_ipblk; i++) {
		ipblk = &exynos_otp_struct[i];
		ipblk_offset = &otp_data[ipblk->offset];

		if (*((u16 *)ipblk_offset) == ipblk->magic)
			ipblk->tuned = true;
		else {
			pr_info("OTP magic 0x%x not found\n", ipblk->magic);
			ipblk->tuned = false;
			continue;
		}

		ipblk->dt_type = *(ipblk_offset + OTP_OFFSET_TYPE);
		ipblk->nr_data = *(ipblk_offset + OTP_OFFSET_COUNT);
		if (!ipblk->nr_data) {
			ipblk->tuned = false;
			pr_err("OTP magic 0x%x found, but index count is zero\n", ipblk->magic);
			continue;
		}

		/* offset = index(1byte) + data(1 or 4bytes) */
		offset = (ipblk->dt_type == OTP_TYPE_BYTE)? 2 : 5;
		for (j = 0; j < ipblk->nr_data; j++) {
			struct tune_bits *data = &ipblk->data[j];
			data->index = *(ipblk_offset + OTP_OFFSET_INDEX + j * offset);
			if (ipblk->dt_type == OTP_TYPE_BYTE)
				data->value = *(ipblk_offset + OTP_OFFSET_DATA + j * offset);
			else
				data->value = *((u32 *)(ipblk_offset + OTP_OFFSET_DATA + j * offset));
		}
	}

#ifdef OTP_DEBUG
	for (i = 0; i < nr_ipblk; i++) {
		ipblk = &exynos_otp_struct[i];
		printk("[0x%lx] OTP magic 0x%x type 0x%x nr_data %d\n",
				(unsigned long)ipblk, ipblk->magic, ipblk->dt_type, ipblk->nr_data);
		if (ipblk->tuned == false) {
			printk("\tNo tune bits in magic 0x%x\n", ipblk->magic);
			continue;
		}
		for (j = 0; j < ipblk->nr_data; j++) {
			struct tune_bits *data = &ipblk->data[j];
			printk("\t[%d] 0x%x 0x%x\n", j, data->index, data->value);
		}
	}
#endif
}

int otp_tune_bits_parsed(u16 magic, u8 *dt_type, u8 *nr_data, struct tune_bits **data)
{
	int i;

	if (!otp_read_done) {
		pr_err("OTP data is not ready\n");
		return -1;
	}

	for (i = 0; i < nr_ipblk; i++) {
		struct otp_struct *ipblk = &exynos_otp_struct[i];
		if (ipblk->magic == magic && ipblk->tuned == true) {
			*dt_type = ipblk->dt_type;
			*nr_data = ipblk->nr_data;
			*data = ipblk->data;
			pr_info("OTP tune bits are found in magic 0x%x\n", magic);
			return 0;
		}
	}

	/* not found */
	pr_err("OTP tune bits are not found in magic 0x%x\n", magic);
	return -1;
}
EXPORT_SYMBOL(otp_tune_bits_parsed);

int otp_tune_bits_offset(u16 magic, unsigned long *addr)
{
	int i;

	if (!otp_read_done) {
		pr_err("OTP data is not ready\n");
		return -1;
	}

	for (i = 0; i < nr_ipblk; i++) {
		struct otp_struct *ipblk = &exynos_otp_struct[i];
		if (ipblk->magic == magic) {
			*addr = (unsigned long)&otp_data[ipblk->offset];
			pr_info("OTP magic 0x%x offset %u tune bits addr 0x%lx\n",
					magic, ipblk->offset, *addr);
			return 0;
		}
	}

	/* invalid magic */
	pr_err("OTP magic 0x%x is unknown\n", magic);
	return -1;
}
EXPORT_SYMBOL(otp_tune_bits_offset);

static const struct otp_struct exynos8890_otp_struct[] = {
	magic_offset(OTP_MAGIC_MIPI_DSI0_M4S4,  0),
	magic_offset(OTP_MAGIC_MIPI_DSI1_M4S0, (112 >> 3)),
	magic_offset(OTP_MAGIC_MIPI_DSI2_M1S0, (224 >> 3)),
	magic_offset(OTP_MAGIC_MIPI_CSI0, (336 >> 3)),
	magic_offset(OTP_MAGIC_MIPI_CSI1, (448 >> 3)),
	magic_offset(OTP_MAGIC_MIPI_CSI2, (560 >> 3)),
	magic_offset(OTP_MAGIC_MIPI_CSI3, (672 >> 3)),
	magic_offset(OTP_MAGIC_USB3, (784 >> 3)),
	magic_offset(OTP_MAGIC_USB2, (991 >> 3)),
	magic_offset(OTP_MAGIC_PCIE_WIFI0, (OTP_BANK12_SIZE + 0)),
	magic_offset(OTP_MAGIC_PCIE_WIFI1, (OTP_BANK12_SIZE + (256 >> 3))),
	magic_offset(OTP_MAGIC_HDMI, (OTP_BANK12_SIZE + (512 >> 3))),
	magic_offset(OTP_MAGIC_EDP, (OTP_BANK12_SIZE + (768 >> 3))),
};

static int __init otp_early_init(void)
{
#if defined(OTP_DEBUG) || defined(OTP_TEST)
	int i;
#endif
	int ret;
	unsigned long phys_addr;
	u32 soc_id;

	otp_read_done = false;

	soc_id = exynos_soc_info.product_id & EXYNOS_SOC_MASK;
	switch(soc_id) {
	case EXYNOS8890_SOC_ID:
			tune_bits_size = OTP_BANK12_SIZE + OTP_BANK19_SIZE;
			nr_ipblk = ARRAY_SIZE(exynos8890_otp_struct);
			exynos_otp_struct = (struct otp_struct *)exynos8890_otp_struct;
			break;
	default:
			goto out;
	}

	otp_data = kzalloc(tune_bits_size, GFP_KERNEL);
	if (!otp_data)
		return -ENOMEM;

	phys_addr = (unsigned long)virt_to_phys(otp_data);
	pr_info("OTP tune bits loc. VA 0x%lx PA 0x%lx\n", (unsigned long)otp_data, phys_addr);

	/* cache invalidate */
	__dma_map_area(otp_data, tune_bits_size, DMA_FROM_DEVICE);

	ret = exynos_smc(0xC2001014, 0, 0x2001, phys_addr);

	__dma_unmap_area(otp_data, tune_bits_size, DMA_FROM_DEVICE);

	if (ret) {
		kfree(otp_data);
		pr_err("OTP tune bits read error %d\n", ret);
		goto out;
	}

#ifdef OTP_DEBUG
	printk("OTP tune bits size %d\n\n", tune_bits_size);
	for (i = 0; i < tune_bits_size; i++) {
		unsigned char *p = otp_data;
		printk("%2x ", *(p + i));
		if (!((i + 1) % 16))
			printk("\n");
		if (!((i + 1) % 128))
			printk("\n");
	}
#endif

#ifdef OTP_TEST
	for (i = 0; i < tune_bits_size; i++) {
		unsigned char *p = otp_data;
		unsigned char *f = ref_data;
		if (*(p+i) != *(f+i))
			printk("OTP index %d is mismatch. OTP:0x%x Ref:0x%x \n", i, *(p+i), *(f+i));
	}
#endif

	otp_parse_tune_bits();
	otp_read_done = true;

	pr_info("OTP tune bits read done\n");
out:
	return 0;
}
fs_initcall(otp_early_init);
