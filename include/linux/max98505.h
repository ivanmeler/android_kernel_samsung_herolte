/*
 * Platform data for MAX98505
 *
 * Copyright 2011-2012 Maxim Integrated Products
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __SOUND_MAX98505_PDATA_H__
#define __SOUND_MAX98505_PDATA_H__

#define MAX98505_I2C_ADDRESS	(0x62 >> 1)

struct max98505_dsp_cfg {
	const char *name;
	u8 tx_dither_en;
	u8 rx_dither_en;
	u8 meas_dc_block_en;
	u8 rx_flt_mode;
};

/* MAX98505 volume step */
#define MAX98505_VSTEP_0		0
#define MAX98505_VSTEP_1		1
#define MAX98505_VSTEP_2		2
#define MAX98505_VSTEP_3		3
#define MAX98505_VSTEP_4		4
#define MAX98505_VSTEP_5		5
#define MAX98505_VSTEP_6		6
#define MAX98505_VSTEP_7		7
#define MAX98505_VSTEP_8		8
#define MAX98505_VSTEP_9		9
#define MAX98505_VSTEP_10		10
#define MAX98505_VSTEP_11		11
#define MAX98505_VSTEP_12		12
#define MAX98505_VSTEP_13		13
#define MAX98505_VSTEP_14		14
#define MAX98505_VSTEP_15		15
#define MAX98505_VSTEP_MAX		MAX98505_VSTEP_15

#ifdef CONFIG_SND_SOC_MAXIM_DSM_CAL
extern struct class *g_class;
#else
struct class *g_class;
#endif /* CONFIG_SND_SOC_MAXIM_DSM_CAL */

struct max98505_volume_step_info {
	int length;
	int vol_step;
	int adc_thres;
	int boost_step[MAX98505_VSTEP_MAX + 1];
	bool adc_status;
};

/*
 * codec platform data.
 * This definition should be changed,
 * if platform_info of device tree is changed.
 */
#define MAX98505_PINFO_SZ	6

struct max98505_pdata {
	int sysclk;
	u32 spk_vol;
	u32 vmon_slot;
	u32 capture_active;
	u32 playback_active:1;
	bool i2c_pull_up;
#ifdef USE_MAX98505_IRQ
	int irq;
#endif
	uint32_t pinfo[MAX98505_PINFO_SZ];
	struct max98505_volume_step_info vstep;
	const uint32_t *reg_arr;
	uint32_t reg_arr_len;
};
#endif
