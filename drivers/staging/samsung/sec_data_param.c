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
#include <linux/sec_sysfs.h>
#include <linux/soc/samsung/exynos-soc.h>

#define MAX_DDR_VENDOR 16
#define MAX_RANK_NUM 2
#define LPDDR4_CH_NUM 4
#define NUM_DQS2DQ_ID 18 // DQ0 ~ DQ7, DM0, DQ8 ~ DQ15, DM1
#define DATA_SIZE 700

enum ids_info {
	tg,
	lg,
	bg,
	g3dg,
	mifg,
	bids,
	gids,
};

/*
   LPDDR4 (JESD209-4) MR5 Manufacturer ID
   0000 0000B : Reserved
   0000 0001B : Samsung
   0000 0101B : Nanya
   0000 0110B : SK hynix
   0000 1000B : Winbond
   0000 1001B : ESMT
   1111 1111B : Micron
   All others : Reserved
 */

static char* lpddr4_manufacture_name[MAX_DDR_VENDOR] =
	{"NA",
	"SEC"/* Samsung */,
	"NA",
	"NA",
	"NA",
	"NAN" /* Nanya */,
	"HYN" /* SK hynix */,
	"NA",
	"WIN" /* Winbond */,
	"ESM" /* ESMT */,
	"NA",
	"NA",
	"NA",
	"NA",
	"NA",
	"MIC" /* Micron */,};

extern int asv_ids_information(enum ids_info id);
extern int pwrcal_get_dram_tdqsck(int ch, int rank, int byte, unsigned int *tdqsck);
extern int pwrcal_get_dram_tdqs2dq(int ch, int rank, int idx, unsigned int *tdqs2dq);
extern unsigned long long pwrcal_get_dram_manufacturer(void);
extern struct exynos_chipid_info exynos_soc_info;
static unsigned int sec_hw_rev;

static int __init sec_hw_param_get_hw_rev(char *arg)
{
    get_option(&arg, &sec_hw_rev);
    return 0;
}
early_param("androidboot.hw_rev", sec_hw_param_get_hw_rev);

static u32 chipid_reverse_value(u32 value, u32 bitcnt)
{
	int tmp, ret = 0;
	int i;

	for (i = 0; i < bitcnt; i++)
	{
		tmp = (value >> i) & 0x1;
		ret += tmp << ((bitcnt - 1) - i);
	}

	return ret;
}

static void chipid_dec_to_36(u32 in, char *p)
{
	int mod;
	int i;

	for (i = 4; i >= 1; i--) {
		mod = in % 36;
		in /= 36;
		p[i] = (mod < 10) ? (mod + '0') : (mod-10 + 'A');
	}

	p[0] = 'N';
	p[5] = 0;
}

static void get_dram_param(int *mr5_vendor_id, int *rank_cnt)
{
	unsigned long long ect_key;
	
	ect_key = pwrcal_get_dram_manufacturer();

	*rank_cnt = 0xf & (ect_key >> 8);
	*mr5_vendor_id = 0xf & (ect_key >> 24);
}

static void get_pwrcal_dram_info(char *tdqs2dq, char *tdqsck, int rank_cnt)
{
	int ch;
	int rank;
	int byte;
	int idx;
	int offset = 0;

	unsigned int dqs2dq;
	unsigned int clk2dqs[2];

	for (ch = 0; ch < LPDDR4_CH_NUM; ch++) {
		offset += sprintf((char*)(tdqsck+offset), "ch%d", ch);
		for (rank = 0; rank < rank_cnt; rank++) {

			for (byte = 0; byte < 2; byte++)
				pwrcal_get_dram_tdqsck(ch, rank, byte, &clk2dqs[byte]);

			offset += sprintf((char*)(tdqsck+offset), "(%x:%x)", clk2dqs[0], clk2dqs[1]);
		}
	}

	offset = 0;

	for (rank = 0; rank < rank_cnt; rank++) {
		for (ch = 0; ch < LPDDR4_CH_NUM; ch++) {
			offset += sprintf((char*)(tdqs2dq+offset), "rank%dch%d(", rank, ch);

			// get tDQS2DQ data(DQ0 ~ DQ7, DM0, DQ8 ~ DQ15, DM1)
			for (idx = 0; idx < NUM_DQS2DQ_ID; idx++) {
				pwrcal_get_dram_tdqs2dq(ch, rank, idx, &dqs2dq);
				offset += sprintf((char*)(tdqs2dq+offset), "%02x", dqs2dq);
			}
			offset += sprintf((char*)(tdqs2dq+offset), ")");
		}
	}
}

static ssize_t sec_hw_param_ap_info_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	/* store ap info
		"LOT_ID":""     : lot id
		"ASV_CL0":""    : ASV for apl
		"ASV_CL1":""    : ASV for mgs
		"ASV_MIF":""    : ASV for mif
		"IDS_CL1":""    : IDS value for mgs
	*/
	
	ssize_t info_size = 0;
	int reverse_id_0 = 0;
	int tmp = 0;
	char lot_id[6];

	reverse_id_0 = chipid_reverse_value(exynos_soc_info.lot_id, 32);
	tmp = (reverse_id_0 >> 11) & 0x1FFFFF;
	chipid_dec_to_36(tmp, lot_id);

	info_size += snprintf(buf, DATA_SIZE, "\"HW_REV\":\"%d\",", sec_hw_rev);
	info_size += snprintf((char*)(buf+info_size), DATA_SIZE - info_size, "\"LOT_ID\":\"%s\",", lot_id);
	info_size += snprintf((char*)(buf+info_size), DATA_SIZE - info_size, "\"ASV_CL0\":\"%d\",", asv_ids_information(lg));
	info_size += snprintf((char*)(buf+info_size), DATA_SIZE - info_size, "\"ASV_CL1\":\"%d\",", asv_ids_information(bg));
	info_size += snprintf((char*)(buf+info_size), DATA_SIZE - info_size, "\"ASV_MIF\":\"%d\",", asv_ids_information(mifg));
	info_size += snprintf((char*)(buf+info_size), DATA_SIZE - info_size, "\"IDS_CL1\":\"%d\",", asv_ids_information(bids));
	info_size += snprintf((char*)(buf+info_size), DATA_SIZE - info_size, "\"PARAM0\":\"\"");
	
	return info_size;
}

static ssize_t sec_hw_param_ddr_info_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	/* store dram info
		"DDRV":""    : dram vendor
		"C2D":""     : tDOSCK - Clk-to-DQS delay, Gate Training Result		
		"D2D":""     : tDQS2DQ - DQS-to-DQ delay, DQ-DQS Training Result 

		The tDOSCK lookup value for each channel, rank, byte, and unit is step.
		The tDQS2DQ lookup value for each channel, rank, dq, dm, and unit is step.
		The tDQS2DQ data is stored in the following order.
		Total idx : 18(dq0 ~ dq7, dm0, dq8 ~ dq15, dm1)
	*/

	char tdqsck[SZ_128] = {0,};
	char tdqs2dq[SZ_512] = {0,};
	int rank_cnt, mr5_vendor_id;
	ssize_t info_size = 0;

	get_dram_param(&mr5_vendor_id, &rank_cnt);

	if(rank_cnt <= MAX_RANK_NUM)
		get_pwrcal_dram_info(tdqs2dq, tdqsck, rank_cnt);

	info_size += snprintf((char*)(buf), DATA_SIZE, "\"DDRV\":\"%s\",", lpddr4_manufacture_name[mr5_vendor_id]);
	info_size += snprintf((char*)(buf+info_size), DATA_SIZE - info_size, "\"C2D\":\"%s\",", tdqsck);
	info_size += snprintf((char*)(buf+info_size), DATA_SIZE - info_size, "\"D2D\":\"%s\"", tdqs2dq);
	
	return info_size;
}


static struct kobj_attribute sec_hw_param_ap_Info_attr =
        __ATTR(ap_info, 0440, sec_hw_param_ap_info_show, NULL);

static struct kobj_attribute sec_hw_param_ddr_Info_attr =
        __ATTR(ddr_info, 0440, sec_hw_param_ddr_info_show, NULL);

static struct attribute *sec_hw_param_attributes[] = {
	&sec_hw_param_ap_Info_attr.attr,	
	&sec_hw_param_ddr_Info_attr.attr,
	NULL,
};

static struct attribute_group sec_hw_param_attr_group = {
	.attrs = sec_hw_param_attributes,
};

static int __init sec_hw_param_init(void)
{
	int ret=0;
	struct device* dev;

	dev = sec_device_create(NULL, "sec_hw_param");
	ret = sysfs_create_group(&dev->kobj, &sec_hw_param_attr_group);

	if (ret)
		printk("%s : could not create sysfs noden", __func__);
	
	return 0;
}
device_initcall(sec_hw_param_init);
