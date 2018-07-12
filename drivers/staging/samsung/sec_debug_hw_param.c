/*
 *sec_hw_param.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sec_debug.h>
#include <linux/sec_ext.h>
#include <linux/sec_sysfs.h>
#include <linux/uaccess.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/io.h>

#define MAX_DDR_VENDOR 16
#define LPDDR_BASE 0x16509b00
#define DATA_SIZE 1024
#define LOT_STRING_LEN 5

extern unsigned long long pwrcal_get_dram_manufacturer(void);

enum ids_info {
	tg,
	lg,
	bg,
	g3dg,
	mifg,
	bids,
	gids,
};

extern int asv_ids_information(enum ids_info id);

/*
 * LPDDR4 (JESD209-4) MR5 Manufacturer ID
 * 0000 0000B : Reserved
 * 0000 0001B : Samsung
 * 0000 0101B : Nanya
 * 0000 0110B : SK hynix
 * 0000 1000B : Winbond
 * 0000 1001B : ESMT
 * 1111 1111B : Micron
 * All others : Reserved
 */
static char *lpddr4_manufacture_name[MAX_DDR_VENDOR] = {
	"NA",
	"SEC", /* Samsung */
	"NA",
	"NA",
	"NA",
	"NAN", /* Nanya */
	"HYN", /* SK hynix */
	"NA",
	"WIN", /* Winbond */
	"ESM", /* ESMT */
	"NA",
	"NA",
	"NA",
	"NA",
	"NA",
	"MIC", /* Micron */
};

static unsigned int sec_hw_rev;
static unsigned int chipid_fail_cnt;
static unsigned int lpi_timeout_cnt;
static unsigned int cache_err_cnt;
static unsigned int codediff_cnt;
static unsigned long pcb_offset;
static unsigned long smd_offset;
static unsigned int lpddr4_size;
static char warranty = 'D';

static int __init sec_hw_param_get_hw_rev(char *arg)
{
	get_option(&arg, &sec_hw_rev);
	return 0;
}

early_param("androidboot.hw_rev", sec_hw_param_get_hw_rev);

static int __init sec_hw_param_check_chip_id(char *arg)
{
	get_option(&arg, &chipid_fail_cnt);
	return 0;
}

early_param("sec_debug.chipidfail_cnt", sec_hw_param_check_chip_id);

static int __init sec_hw_param_check_lpi_timeout(char *arg)
{
	get_option(&arg, &lpi_timeout_cnt);
	return 0;
}

early_param("sec_debug.lpitimeout_cnt", sec_hw_param_check_lpi_timeout);

static int __init sec_hw_param_cache_error(char *arg)
{
	get_option(&arg, &cache_err_cnt);
	return 0;
}

early_param("sec_debug.cache_err_cnt", sec_hw_param_cache_error);

static int __init sec_hw_param_code_diff(char *arg)
{
	get_option(&arg, &codediff_cnt);
	return 0;
}

early_param("sec_debug.codediff_cnt", sec_hw_param_code_diff);

static int __init sec_hw_param_pcb_offset(char *arg)
{
	pcb_offset = simple_strtoul(arg, NULL, 10);
	return 0;
}

early_param("sec_debug.pcb_offset", sec_hw_param_pcb_offset);

static int __init sec_hw_param_smd_offset(char *arg)
{
	smd_offset = simple_strtoul(arg, NULL, 10);
	return 0;
}

early_param("sec_debug.smd_offset", sec_hw_param_smd_offset);

static int __init sec_hw_param_lpddr4_size(char *arg)
{
	get_option(&arg, &lpddr4_size);
	return 0;
}

early_param("sec_debug.lpddr4_size", sec_hw_param_lpddr4_size);

static int __init sec_hw_param_bin(char *arg)
{
	warranty = (char)*arg;
	return 0;
}

early_param("sec_debug.bin", sec_hw_param_bin);

static u32 chipid_reverse_value(u32 value, u32 bitcnt)
{
	int tmp, ret = 0;
	int i;

	for (i = 0; i < bitcnt; i++) {
		tmp = (value >> i) & 0x1;
		ret += tmp << ((bitcnt - 1) - i);
	}

	return ret;
}

static void chipid_dec_to_36(u32 in, char *p)
{
	int mod;
	int i;

	for (i = LOT_STRING_LEN - 1; i >= 1; i--) {
		mod = in % 36;
		in /= 36;
		p[i] = (mod < 10) ? (mod + '0') : (mod - 10 + 'A');
	}

	p[0] = 'N';
	p[LOT_STRING_LEN] = '\0';
}

static char *get_dram_manufacturer(void)
{
	unsigned long long ect_key;
	int mr5_vendor_id = 0;

	ect_key = pwrcal_get_dram_manufacturer();
	mr5_vendor_id = 0xf & (ect_key >> 24);
	return lpddr4_manufacture_name[mr5_vendor_id];
}

static ssize_t sec_hw_param_ap_info_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	int reverse_id_0 = 0;
	u32 tmp = 0;
	char lot_id[LOT_STRING_LEN + 1];

	reverse_id_0 = chipid_reverse_value(exynos_soc_info.lot_id, 32);
	tmp = (reverse_id_0 >> 11) & 0x1FFFFF;
	chipid_dec_to_36(tmp, lot_id);

	info_size += snprintf(buf, DATA_SIZE, "\"HW_REV\":\"%d\",", sec_hw_rev);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"BIN\":\"%c\",", warranty);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"LOT_ID\":\"%s\",", lot_id);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"CHIPID_FAIL\":\"%d\",", chipid_fail_cnt);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"LPI_TIMEOUT\":\"%d\",", lpi_timeout_cnt);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"CACHE_ERR\":\"%d\",", cache_err_cnt);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"CODE_DIFF\":\"%d\",", codediff_cnt);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"ASV_BIG\":\"%d\",", asv_ids_information(bg));
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"ASV_LIT\":\"%d\",", asv_ids_information(lg));
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"ASV_MIF\":\"%d\",", asv_ids_information(mifg));
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"IDS_BIG\":\"%d\"", asv_ids_information(bids));

	return info_size;
}

static ssize_t sec_hw_param_ddr_info_show(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  char *buf)
{
	ssize_t info_size = 0;

	info_size +=
	    snprintf((char *)(buf), DATA_SIZE, "\"DDRV\":\"%s\",",
		     get_dram_manufacturer());
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"LPDDR4\":\"%dGB\",", lpddr4_size);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"C2D\":\"\",");
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"D2D\":\"\"");

	return info_size;
}

static ssize_t sec_hw_param_extra_info_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (reset_reason == RR_K || reset_reason == RR_D || 
		reset_reason == RR_P || reset_reason == RR_S) {
		sec_debug_store_extra_info_A();
		strncpy(buf, (char *)SEC_DEBUG_EXTRA_INFO_VA, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static ssize_t sec_hw_param_extrb_info_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (reset_reason == RR_K || reset_reason == RR_D || 
		reset_reason == RR_P || reset_reason == RR_S) {
		sec_debug_store_extra_info_B();
		strncpy(buf, (char *)SEC_DEBUG_EXTRA_INFO_VA, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static ssize_t sec_hw_param_extrc_info_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (reset_reason == RR_K || reset_reason == RR_D || 
		reset_reason == RR_P || reset_reason == RR_S) {
		sec_debug_store_extra_info_C();
		strncpy(buf, (char *)SEC_DEBUG_EXTRA_INFO_VA, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static ssize_t sec_hw_param_extrm_info_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (reset_reason == RR_K || reset_reason == RR_D || 
		reset_reason == RR_P || reset_reason == RR_S) {
		sec_debug_store_extra_info_M();
		strncpy(buf, (char *)SEC_DEBUG_EXTRA_INFO_VA, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static ssize_t sec_hw_param_pcb_info_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned char barcode[6] = {0,};
	int ret = -1;

	strncpy(barcode, buf, 5);

	ret = sec_set_param_str(pcb_offset , barcode, 5);
	if (ret < 0)
		pr_err("%s : Set Param fail. offset (%lu), data (%s)", __func__, pcb_offset, barcode);
	
	return count;
}

static ssize_t sec_hw_param_smd_info_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned char smd_date[9] = {0,};
	int ret = -1;

	strncpy(smd_date, buf, 8);

	ret = sec_set_param_str(smd_offset , smd_date, 8);
	if (ret < 0)
		pr_err("%s : Set Param fail. offset (%lu), data (%s)", __func__, smd_offset, smd_date);
	
	return count;
}

static struct kobj_attribute sec_hw_param_ap_info_attr =
        __ATTR(ap_info, 0440, sec_hw_param_ap_info_show, NULL);

static struct kobj_attribute sec_hw_param_ddr_info_attr =
        __ATTR(ddr_info, 0440, sec_hw_param_ddr_info_show, NULL);

static struct kobj_attribute sec_hw_param_extra_info_attr =
	__ATTR(extra_info, 0440, sec_hw_param_extra_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrb_info_attr =
	__ATTR(extrb_info, 0440, sec_hw_param_extrb_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrc_info_attr =
	__ATTR(extrc_info, 0440, sec_hw_param_extrc_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrm_info_attr =
	__ATTR(extrm_info, 0440, sec_hw_param_extrm_info_show, NULL);
static struct kobj_attribute sec_hw_param_pcb_info_attr =
        __ATTR(pcb_info, 0660, NULL, sec_hw_param_pcb_info_store);

static struct kobj_attribute sec_hw_param_smd_info_attr =
	__ATTR(smd_info, 0660, NULL, sec_hw_param_smd_info_store);

static struct attribute *sec_hw_param_attributes[] = {
	&sec_hw_param_ap_info_attr.attr,
	&sec_hw_param_ddr_info_attr.attr,
	&sec_hw_param_extra_info_attr.attr,
	&sec_hw_param_extrb_info_attr.attr,
	&sec_hw_param_extrc_info_attr.attr,
	&sec_hw_param_extrm_info_attr.attr,
	&sec_hw_param_pcb_info_attr.attr,
	&sec_hw_param_smd_info_attr.attr,
	NULL,
};

static struct attribute_group sec_hw_param_attr_group = {
	.attrs = sec_hw_param_attributes,
};

static int __init sec_hw_param_init(void)
{
	int ret = 0;
	struct device *dev;

	dev = sec_device_create(NULL, "sec_hw_param");

	ret = sysfs_create_group(&dev->kobj, &sec_hw_param_attr_group);
	if (ret)
		pr_err("%s : could not create sysfs noden", __func__);

	return 0;
}

device_initcall(sec_hw_param_init);
