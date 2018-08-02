/*
 *  max77843_fuelgauge.c
 *  Samsung MAX77843 Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG
/* #define BATTERY_LOG_MESSAGE */

#include <linux/mfd/max77843-private.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static enum power_supply_property max77843_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
#if defined(CONFIG_EN_OOPS)
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
#endif
};

#if !defined(CONFIG_SEC_FACTORY)
static void max77843_fg_read_time(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int lsb, msb, time;

	if (max77843_bulk_read(fuelgauge->i2c,
		TIME_TO_FULL_REG, 2, data) < 0) {
		pr_err("%s: Failed to read TTF\n", __func__);
		return;
	}

	lsb = data[0] & 0x1f;
	msb = ((data[1] << 3) + ((data[0] & 0xe0) >> 5));
	time = msb * 180 + (lsb * 5625) / 1000;

	pr_info("[Time-to-FULL] %d(secs), %d(mins)\n", time, time / 60);

	if (max77843_bulk_read(fuelgauge->i2c,
		TIME_TO_EMPTY_REG, 2, data) < 0) {
		pr_err("%s: Failed to read TTE\n", __func__);
		return;
	}

	lsb = data[0] & 0x1f;
	msb = ((data[1] << 3) + ((data[0] & 0xe0) >> 5));
	time = msb * 180 + (lsb * 5625) / 1000;

	pr_info("[Time-to-EMPTY] %d(secs), %d(mins)\n", time, time / 60);
}

static void max77843_fg_test_print(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 average_vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;
#ifdef BATTERY_LOG_MESSAGE
	u16 reg_data;
#endif

	if (max77843_bulk_read(fuelgauge->i2c,
				AVR_VCELL_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	average_vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	average_vcell += (temp2 << 4);

#ifdef BATTERY_LOG_MESSAGE
	pr_info("%s: AVG_VCELL(%d), data(0x%04x)\n", __func__,
		average_vcell, (data[1]<<8) | data[0]);

	reg_data = max77843_read_word(fuelgauge->i2c, FULLCAP_REG);
	pr_info("%s: FULLCAP(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = max77843_read_word(fuelgauge->i2c, REMCAP_REP_REG);
	pr_info("%s: REMCAP_REP(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = max77843_read_word(fuelgauge->i2c, REMCAP_MIX_REG);
	pr_info("%s: REMCAP_MIX(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = max77843_read_word(fuelgauge->i2c, REMCAP_AV_REG);
	pr_info("%s: REMCAP_AV(%d), data(0x%04x)\n", __func__,
		reg_data/2, reg_data);

	reg_data = max77843_read_word(fuelgauge->i2c, CONFIG_REG);
	pr_info("%s: CONFIG_REG(0x%02x), data(0x%04x)\n", __func__,
		CONFIG_REG, reg_data);
#endif

	max77843_fg_read_time(fuelgauge);
}

static void max77843_fg_periodic_read(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 reg;
	int i;
	int data[0x10];
	char *str = NULL;

	str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
	if (!str)
		return;

	for (i = 0; i < 16; i++) {
		for (reg = 0; reg < 0x10; reg++) {
			data[reg] = max77843_read_word(fuelgauge->i2c, reg + i * 0x10);
			if (data[reg] < 0) {
				kfree(str);
				return;
			}
		}
		if (i == 12)
			continue;
		sprintf(str+strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x00], data[0x01], data[0x02], data[0x03],
			data[0x04], data[0x05], data[0x06], data[0x07]);
		sprintf(str+strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x08], data[0x09], data[0x0a], data[0x0b],
			data[0x0c], data[0x0d], data[0x0e], data[0x0f]);
		if (i == 4)
			i = 10;
	}

	pr_info("[FG] %s\n", str);

	kfree(str);
}
#endif

static int max77843_fg_read_vcell(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (max77843_bulk_read(fuelgauge->i2c, VCELL_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vcell += (temp2 << 4);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: VCELL(%d), data(0x%04x)\n",
			__func__, vcell, (data[1]<<8) | data[0]);

	return vcell;
}

static int max77843_fg_read_vfocv(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vfocv = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (max77843_bulk_read(fuelgauge->i2c, VFOCV_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VFOCV\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vfocv = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vfocv += (temp2 << 4);

	return vfocv;
}

static int max77843_fg_read_avg_vcell(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 avg_vcell = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (max77843_bulk_read(fuelgauge->i2c, AVR_VCELL_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read AVG_VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	avg_vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	avg_vcell += (temp2 << 4);

	return avg_vcell;
}

static int max77843_fg_check_battery_present(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 status_data[2];
	int ret = 1;

	/* 1. Check Bst bit */
	if (max77843_bulk_read(fuelgauge->i2c, STATUS_REG,
			       2, status_data) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return 0;
	}

	if (status_data[0] & (0x1 << 3)) {
		pr_info("%s: addr(0x01), data(0x%04x)\n", __func__,
			(status_data[1]<<8) | status_data[0]);
		pr_info("%s: battery is absent!!\n", __func__);
		ret = 0;
	}

	return ret;
}

static int max77843_fg_write_temp(struct max77843_fuelgauge_data *fuelgauge,
			 int temperature)
{
	u8 data[2];

	data[0] = (temperature%10) * 1000 / 39;
	data[1] = temperature / 10;
	max77843_bulk_write(fuelgauge->i2c, TEMPERATURE_REG, 2, data);

	pr_debug("%s: temperature to (%d, 0x%02x%02x)\n",
		__func__, temperature, data[1], data[0]);

	return temperature;
}

static int max77843_fg_read_temp(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2] = {0, 0};
	int temper = 0;

	if (max77843_fg_check_battery_present(fuelgauge)) {
		if (max77843_bulk_read(fuelgauge->i2c,
				       TEMPERATURE_REG, 2, data) < 0) {
			pr_err("%s: Failed to read TEMPERATURE_REG\n",
				__func__);
			return -1;
		}

		if (data[1]&(0x1 << 7)) {
			temper = ((~(data[1]))&0xFF)+1;
			temper *= (-1000);
			temper -= ((~((int)data[0]))+1) * 39 / 10;
		} else {
			temper = data[1] & 0x7f;
			temper *= 1000;
			temper += data[0] * 39 / 10;
		}
	} else
		temper = 20000;

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: TEMPERATURE(%d), data(0x%04x)\n",
			__func__, temper, (data[1]<<8) | data[0]);

	return temper/100;
}

/* soc should be 0.1% unit */
static int max77843_fg_read_vfsoc(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77843_bulk_read(fuelgauge->i2c, VFSOC_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read VFSOC\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	return min(soc, 1000);
}

/* soc should be 0.1% unit */
static int max77843_fg_read_avsoc(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77843_bulk_read(fuelgauge->i2c, SOCAV_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read AVSOC\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	return min(soc, 1000);
}

/* soc should be 0.1% unit */
static int max77843_fg_read_soc(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc, vf_soc;

	if (max77843_bulk_read(fuelgauge->i2c, SOCREP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;
	vf_soc = max77843_fg_read_vfsoc(fuelgauge);

#ifdef BATTERY_LOG_MESSAGE
	pr_debug("%s: raw capacity (%d)\n", __func__, soc);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT)) {
		pr_debug("%s: raw capacity (%d), data(0x%04x)\n",
			 __func__, soc, (data[1]<<8) | data[0]);
		pr_debug("%s: REPSOC (%d), VFSOC (%d), data(0x%04x)\n",
				__func__, soc/10, vf_soc/10, (data[1]<<8) | data[0]);
	}
#endif

	return min(soc, 1000);
}

/* soc should be 0.01% unit */
static int max77843_fg_read_rawsoc(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77843_bulk_read(fuelgauge->i2c, SOCREP_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	soc = (data[1] * 100) + (data[0] * 100 / 256);

	pr_debug("%s: raw capacity (0.01%%) (%d)\n",
		 __func__, soc);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_debug("%s: raw capacity (%d), data(0x%04x)\n",
			 __func__, soc, (data[1]<<8) | data[0]);

	return min(soc, 10000);
}

static int max77843_fg_read_fullcap(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77843_bulk_read(fuelgauge->i2c, FULLCAP_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read FULLCAP\n", __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int max77843_fg_read_mixcap(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77843_bulk_read(fuelgauge->i2c, REMCAP_MIX_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_MIX_REG\n",
		       __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int max77843_fg_read_avcap(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77843_bulk_read(fuelgauge->i2c, REMCAP_AV_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_AV_REG\n",
		       __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int max77843_fg_read_repcap(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77843_bulk_read(fuelgauge->i2c, REMCAP_REP_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_REP_REG\n",
		       __func__);
		return -1;
	}

	ret = (data[1] << 8) + data[0];

	return ret;
}

static int max77843_fg_read_current(struct max77843_fuelgauge_data *fuelgauge, int unit)
{
	u8 data1[2], data2[2];
	u32 temp, sign;
	s32 i_current;
	s32 avg_current;
	int vcell;
	static int cnt;

	if (max77843_bulk_read(fuelgauge->i2c, CURRENT_REG,
			      2, data1) < 0) {
		pr_err("%s: Failed to read CURRENT\n", __func__);
		return -1;
	}

	if (max77843_bulk_read(fuelgauge->i2c, AVG_CURRENT_REG,
			       2, data2) < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n", __func__);
		return -1;
	}

	temp = ((data1[1]<<8) | data1[0]) & 0xFFFF;
	/* Debug log for abnormal current case */
	pr_info("%s: CURRENT_REG(0x%04x)\n", __func__, temp);
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	switch (unit) {
	case SEC_BATTEY_CURRENT_UA:
		i_current = temp * 15625 / 100;
		break;
	case SEC_BATTEY_CURRENT_MA:
	default:
		i_current = temp * 15625 / 100000;
	}

	if (sign)
		i_current *= -1;

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	avg_current = temp * 15625 / 100000;
	if (sign)
		avg_current *= -1;

	vcell = max77843_fg_read_vcell(fuelgauge);
	if ((vcell < 3500) && (cnt < 10) && (i_current < 0) &&
			fuelgauge->is_charging) {
		i_current = 1;
		cnt++;
	}

#if !defined(CONFIG_SEC_FACTORY)
	max77843_fg_test_print(fuelgauge);
	max77843_fg_periodic_read(fuelgauge);
#endif
	return i_current;
}

static int max77843_fg_read_avg_current(struct max77843_fuelgauge_data *fuelgauge, int unit)
{
	u8  data2[2];
	u32 temp, sign;
	s32 avg_current;
	int vcell;
	static int cnt;

	if (max77843_bulk_read(fuelgauge->i2c, AVG_CURRENT_REG,
			       2, data2) < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n",
		       __func__);
		return -1;
	}

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	switch (unit) {
	case SEC_BATTEY_CURRENT_UA:
		avg_current = temp * 15625 / 100;
		break;
	case SEC_BATTEY_CURRENT_MA:
	default:
		avg_current = temp * 15625 / 100000;
	}

	if (sign)
		avg_current *= -1;

	vcell = max77843_fg_read_vcell(fuelgauge);
	if ((vcell < 3500) && (cnt < 10) && (avg_current < 0) &&
	    fuelgauge->is_charging) {
		avg_current = 1;
		cnt++;
	}

	return avg_current;
}

int max77843_fg_reset_soc(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int vfocv, fullcap;

	/* delay for current stablization */
	msleep(500);

	pr_info("%s: Before quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77843_fg_read_vcell(fuelgauge), max77843_fg_read_vfocv(fuelgauge),
		max77843_fg_read_vfsoc(fuelgauge), max77843_fg_read_soc(fuelgauge));
	pr_info("%s: Before quick-start - current(%d), avg current(%d)\n",
		__func__, max77843_fg_read_current(fuelgauge, SEC_BATTEY_CURRENT_MA),
		max77843_fg_read_avg_current(fuelgauge, SEC_BATTEY_CURRENT_MA));

	if (fuelgauge->pdata->check_jig_status &&
	    !fuelgauge->pdata->check_jig_status()) {
		pr_info("%s : Return by No JIG_ON signal\n", __func__);
		return 0;
	}

	max77843_write_word(fuelgauge->i2c, CYCLES_REG, 0);

	if (max77843_bulk_read(fuelgauge->i2c, MISCCFG_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read MiscCFG\n", __func__);
		return -1;
	}

	data[1] |= (0x1 << 2);
	if (max77843_bulk_write(fuelgauge->i2c, MISCCFG_REG,
				2, data) < 0) {
		pr_err("%s: Failed to write MiscCFG\n", __func__);
		return -1;
	}

	msleep(250);
	max77843_write_word(fuelgauge->i2c, FULLCAP_REG,
			    fuelgauge->battery_data->Capacity);
	msleep(500);

	pr_info("%s: After quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77843_fg_read_vcell(fuelgauge), max77843_fg_read_vfocv(fuelgauge),
		max77843_fg_read_vfsoc(fuelgauge), max77843_fg_read_soc(fuelgauge));
	pr_info("%s: After quick-start - current(%d), avg current(%d)\n",
		__func__, max77843_fg_read_current(fuelgauge, SEC_BATTEY_CURRENT_MA),
		max77843_fg_read_avg_current(fuelgauge, SEC_BATTEY_CURRENT_MA));

	max77843_write_word(fuelgauge->i2c, CYCLES_REG, 0x00a0);

/* P8 is not turned off by Quickstart @3.4V
 * (It's not a problem, depend on mode data)
 * Power off for factory test(File system, etc..) */
	vfocv = max77843_fg_read_vfocv(fuelgauge);
	if (vfocv < POWER_OFF_VOLTAGE_LOW_MARGIN) {
		pr_info("%s: Power off condition(%d)\n", __func__, vfocv);

		fullcap = max77843_read_word(fuelgauge->i2c, FULLCAP_REG);

		/* FullCAP * 0.009 */
		max77843_write_word(fuelgauge->i2c, REMCAP_REP_REG,
				    (u16)(fullcap * 9 / 1000));
		msleep(200);
		pr_info("%s: new soc=%d, vfocv=%d\n", __func__,
			max77843_fg_read_soc(fuelgauge), vfocv);
	}

	pr_info("%s: Additional step - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77843_fg_read_vfocv(fuelgauge),
		max77843_fg_read_vfsoc(fuelgauge), max77843_fg_read_soc(fuelgauge));

	return 0;
}

int max77843_fg_reset_capacity_by_jig_connection(struct max77843_fuelgauge_data *fuelgauge)
{

	pr_info("%s: DesignCap = Capacity - 1 (Jig Connection)\n", __func__);

	return max77843_write_word(fuelgauge->i2c, DESIGNCAP_REG,
				   fuelgauge->battery_data->Capacity-1);
}

void max77843_fg_low_batt_compensation(struct max77843_fuelgauge_data *fuelgauge,
			      u32 level)
{
	int read_val;
	u32 temp;

	pr_info("%s: Adjust SOCrep to %d!!\n", __func__, level);

	read_val = max77843_read_word(fuelgauge->i2c, FULLCAP_REG);
	/* RemCapREP (05h) = FullCap(10h) x 0.0090 */
	temp = read_val * (level*90) / 10000;
	max77843_write_word(fuelgauge->i2c, REMCAP_REP_REG,
			    (u16)temp);
}

static int max77843_fg_check_status_reg(struct max77843_fuelgauge_data *fuelgauge)
{
	u8 status_data[2];
	int ret = 0;

	/* 1. Check Smn was generatedread */
	if (max77843_bulk_read(fuelgauge->i2c, STATUS_REG,
			       2, status_data) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return -1;
	}

#ifdef BATTERY_LOG_MESSAGE
	pr_info("%s: addr(0x00), data(0x%04x)\n", __func__,
		(status_data[1]<<8) | status_data[0]);
#endif

	if (status_data[1] & (0x1 << 2))
		ret = 1;

	/* 2. clear Status reg */
	status_data[1] = 0;
	if (max77843_bulk_write(fuelgauge->i2c, STATUS_REG,
				2, status_data) < 0) {
		pr_info("%s: Failed to write STATUS_REG\n", __func__);
		return -1;
	}

	return ret;
}

int max77843_get_fuelgauge_value(struct max77843_fuelgauge_data *fuelgauge, int data)
{
	int ret;

	switch (data) {
	case FG_LEVEL:
		ret = max77843_fg_read_soc(fuelgauge);
		break;

	case FG_TEMPERATURE:
		ret = max77843_fg_read_temp(fuelgauge);
		break;

	case FG_VOLTAGE:
		ret = max77843_fg_read_vcell(fuelgauge);
		break;

	case FG_CURRENT:
		ret = max77843_fg_read_current(fuelgauge, SEC_BATTEY_CURRENT_MA);
		break;

	case FG_CURRENT_AVG:
		ret = max77843_fg_read_avg_current(fuelgauge, SEC_BATTEY_CURRENT_MA);
		break;

	case FG_CHECK_STATUS:
		ret = max77843_fg_check_status_reg(fuelgauge);
		break;

	case FG_RAW_SOC:
		ret = max77843_fg_read_rawsoc(fuelgauge);
		break;

	case FG_VF_SOC:
		ret = max77843_fg_read_vfsoc(fuelgauge);
		break;

	case FG_AV_SOC:
		ret = max77843_fg_read_avsoc(fuelgauge);
		break;

	case FG_FULLCAP:
		ret = max77843_fg_read_fullcap(fuelgauge);
		break;

	case FG_MIXCAP:
		ret = max77843_fg_read_mixcap(fuelgauge);
		break;

	case FG_AVCAP:
		ret = max77843_fg_read_avcap(fuelgauge);
		break;

	case FG_REPCAP:
		ret = max77843_fg_read_repcap(fuelgauge);
		break;

	default:
		ret = -1;
		break;
	}

	return ret;
}

int max77843_fg_alert_init(struct max77843_fuelgauge_data *fuelgauge, int soc)
{
	u8 misccgf_data[2];
	u8 salrt_data[2];
	u8 config_data[2];
	u8 valrt_data[2];
	u8 talrt_data[2];
	u16 read_data = 0;

	fuelgauge->is_fuel_alerted = false;

	/* Using RepSOC */
	if (max77843_bulk_read(fuelgauge->i2c, MISCCFG_REG, 2,
			       misccgf_data) < 0) {
		pr_err("%s: Failed to read MISCCFG_REG\n", __func__);
		return -1;
	}
	misccgf_data[0] = misccgf_data[0] & ~(0x03);

	if (max77843_bulk_write(fuelgauge->i2c, MISCCFG_REG,
				2, misccgf_data) < 0) {
		pr_info("%s: Failed to write MISCCFG_REG\n", __func__);
		return -1;
	}

	/* SALRT Threshold setting */
	salrt_data[1] = 0xff;
	salrt_data[0] = soc;
	if (max77843_bulk_write(fuelgauge->i2c, SALRT_THRESHOLD_REG,
				2, salrt_data) < 0) {
		pr_info("%s: Failed to write SALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	/* Reset VALRT Threshold setting (disable) */
	valrt_data[1] = 0xFF;
	valrt_data[0] = 0x00;
	if (max77843_bulk_write(fuelgauge->i2c, VALRT_THRESHOLD_REG,
				2, valrt_data) < 0) {
		pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = max77843_read_word(fuelgauge->i2c, (u8)VALRT_THRESHOLD_REG);
	if (read_data != 0xff00)
		pr_err("%s: VALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	/* Reset TALRT Threshold setting (disable) */
	talrt_data[1] = 0x7F;
	talrt_data[0] = 0x80;
	if (max77843_bulk_write(fuelgauge->i2c, TALRT_THRESHOLD_REG,
				2, talrt_data) < 0) {
		pr_info("%s: Failed to write TALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = max77843_read_word(fuelgauge->i2c, (u8)TALRT_THRESHOLD_REG);
	if (read_data != 0x7f80)
		pr_err("%s: TALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	/*mdelay(100);*/

	/* Enable SOC alerts */
	if (max77843_bulk_read(fuelgauge->i2c, CONFIG_REG,
			       2, config_data) < 0) {
		pr_err("%s: Failed to read CONFIG_REG\n", __func__);
		return -1;
	}
	config_data[0] = config_data[0] | (0x1 << 2);

	if (max77843_bulk_write(fuelgauge->i2c, CONFIG_REG,
				2, config_data) < 0) {
		pr_info("%s: Failed to write CONFIG_REG\n", __func__);
		return -1;
	}

	max77843_update_reg(fuelgauge->pmic,
			    MAX77843_PMIC_REG_INTSRC_MASK,
			    ~MAX77843_IRQSRC_FG,
			    MAX77843_IRQSRC_FG);

	pr_info("[%s] SALRT(0x%02x%02x), VALRT(0x%02x%02x), CONFIG(0x%02x%02x)\n",
		__func__,
		salrt_data[1], salrt_data[0],
		valrt_data[1], valrt_data[0],
		config_data[1], config_data[0]);

	return 1;
}

static void max77843_display_low_batt_comp_cnt(struct max77843_fuelgauge_data *fuelgauge)
{
	pr_info("Check Array(%s): [%d, %d], [%d, %d], ",
			fuelgauge->battery_data->type_str,
			fuelgauge->info.low_batt_comp_cnt[0][0],
			fuelgauge->info.low_batt_comp_cnt[0][1],
			fuelgauge->info.low_batt_comp_cnt[1][0],
			fuelgauge->info.low_batt_comp_cnt[1][1]);
	pr_info("[%d, %d], [%d, %d], [%d, %d]\n",
			fuelgauge->info.low_batt_comp_cnt[2][0],
			fuelgauge->info.low_batt_comp_cnt[2][1],
			fuelgauge->info.low_batt_comp_cnt[3][0],
			fuelgauge->info.low_batt_comp_cnt[3][1],
			fuelgauge->info.low_batt_comp_cnt[4][0],
			fuelgauge->info.low_batt_comp_cnt[4][1]);
}

static void max77843_add_low_batt_comp_cnt(struct max77843_fuelgauge_data *fuelgauge,
				int range, int level)
{
	int i;
	int j;

	/* Increase the requested count value, and reset others. */
	fuelgauge->info.low_batt_comp_cnt[range-1][level/2]++;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (i == range-1 && j == level/2)
				continue;
			else
				fuelgauge->info.low_batt_comp_cnt[i][j] = 0;
		}
	}
}

void max77843_prevent_early_poweroff(struct max77843_fuelgauge_data *fuelgauge,
	int vcell, int *fg_soc)
{
	int soc = 0;
	int read_val;

	soc = max77843_fg_read_soc(fuelgauge);

	/* No need to write REMCAP_REP in below normal cases */
	if (soc > POWER_OFF_SOC_HIGH_MARGIN ||
	    vcell > fuelgauge->battery_data->low_battery_comp_voltage)
		return;

	pr_info("%s: soc=%d, vcell=%d\n", __func__, soc, vcell);

	if (vcell > POWER_OFF_VOLTAGE_HIGH_MARGIN) {
		read_val = max77843_read_word(fuelgauge->i2c, FULLCAP_REG);
		/* FullCAP * 0.013 */
		max77843_write_word(fuelgauge->i2c, REMCAP_REP_REG,
		(u16)(read_val * 13 / 1000));
		msleep(200);
		*fg_soc = max77843_fg_read_soc(fuelgauge);
		pr_info("%s: new soc=%d, vcell=%d\n", __func__, *fg_soc, vcell);
	}
}

void max77843_reset_low_batt_comp_cnt(struct max77843_fuelgauge_data *fuelgauge)
{
	memset(fuelgauge->info.low_batt_comp_cnt, 0,
		sizeof(fuelgauge->info.low_batt_comp_cnt));
}

static int max77843_check_low_batt_comp_condition(
	struct max77843_fuelgauge_data *fuelgauge,
	int *nLevel)
{
	int i;
	int j;
	int ret = 0;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (fuelgauge->info.low_batt_comp_cnt[i][j] >=
				MAX_LOW_BATT_CHECK_CNT) {
				max77843_display_low_batt_comp_cnt(fuelgauge);
				ret = 1;
				*nLevel = j*2 + 1;
				break;
			}
		}
	}

	return ret;
}

static int max77843_get_low_batt_threshold(struct max77843_fuelgauge_data *fuelgauge,
				int range, int nCurrent, int level)
{
	int ret = 0;

	ret = fuelgauge->battery_data->low_battery_table[range][OFFSET] +
		((nCurrent *
		fuelgauge->battery_data->low_battery_table[range][SLOPE]) /
		1000);

	return ret;
}

int max77843_low_batt_compensation(struct max77843_fuelgauge_data *fuelgauge,
		int fg_soc, int fg_vcell, int fg_current)
{
	int fg_avg_current = 0;
	int fg_min_current = 0;
	int new_level = 0;
	int i, table_size;

	/* Not charging, Under low battery comp voltage */
	if (fg_vcell <= fuelgauge->battery_data->low_battery_comp_voltage) {
		fg_avg_current = max77843_fg_read_avg_current(fuelgauge,
			SEC_BATTEY_CURRENT_MA);
		fg_min_current = min(fg_avg_current, fg_current);

		table_size =
			sizeof(fuelgauge->battery_data->low_battery_table) /
			(sizeof(s16)*TABLE_MAX);

		for (i = 1; i < CURRENT_RANGE_MAX_NUM; i++) {
			if ((fg_min_current >= fuelgauge->battery_data->
				low_battery_table[i-1][RANGE]) &&
				(fg_min_current < fuelgauge->battery_data->
				low_battery_table[i][RANGE])) {
				if (fg_soc >= 10 && fg_vcell <
					max77843_get_low_batt_threshold(fuelgauge,
					i, fg_min_current, 1)) {
					max77843_add_low_batt_comp_cnt(
						fuelgauge, i, 1);
				} else {
					max77843_reset_low_batt_comp_cnt(fuelgauge);
				}
			}
		}

		if (max77843_check_low_batt_comp_condition(fuelgauge, &new_level)) {
			max77843_fg_low_batt_compensation(fuelgauge, new_level);
			max77843_reset_low_batt_comp_cnt(fuelgauge);

			/* Do not update soc right after
			 * low battery compensation
			 * to prevent from powering-off suddenly
			 */
			pr_info("%s: SOC is set to %d by low compensation!!\n",
				__func__, max77843_fg_read_soc(fuelgauge));
		}
	}

	/* Prevent power off over 3500mV */
	max77843_prevent_early_poweroff(fuelgauge, fg_vcell, &fg_soc);

	return fg_soc;
}

static bool max77843_fuelgauge_recovery_handler(struct max77843_fuelgauge_data *fuelgauge)
{
	if (fuelgauge->info.soc < LOW_BATTERY_SOC_REDUCE_UNIT) {
		fuelgauge->info.is_low_batt_alarm = false;
	} else {
		pr_err("%s: Reduce the Reported SOC by 1%%\n",
			__func__);

		fuelgauge->info.soc -=
			LOW_BATTERY_SOC_REDUCE_UNIT;
		pr_err("%s: New Reduced RepSOC (%d)\n",
			__func__, fuelgauge->info.soc);
	}

	return fuelgauge->info.is_low_batt_alarm;
}

static int max77843_get_fuelgauge_soc(struct max77843_fuelgauge_data *fuelgauge)
{
	union power_supply_propval value;
	int fg_soc = 0;
	int fg_vfsoc;
	int fg_vcell;
	int fg_current;
	int avg_current;

	if (fuelgauge->info.is_low_batt_alarm)
		if (max77843_fuelgauge_recovery_handler(fuelgauge)) {
			fg_soc = fuelgauge->info.soc;
			goto return_soc;
		}

	fg_soc = max77843_get_fuelgauge_value(fuelgauge, FG_LEVEL);
	if (fg_soc < 0) {
		pr_info("Can't read soc!!!");
		fg_soc = fuelgauge->info.soc;
	}

	fg_vcell = max77843_get_fuelgauge_value(fuelgauge, FG_VOLTAGE);
	fg_current = max77843_get_fuelgauge_value(fuelgauge, FG_CURRENT);
	avg_current = max77843_get_fuelgauge_value(fuelgauge, FG_CURRENT_AVG);
	fg_vfsoc = max77843_get_fuelgauge_value(fuelgauge, FG_VF_SOC);

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);

	/*  Checks vcell level and tries to compensate SOC if needed.*/
	/*  If jig cable is connected, then skip low batt compensation check. */
	if (fuelgauge->pdata->check_jig_status &&
	    !fuelgauge->pdata->check_jig_status() &&
		value.intval == POWER_SUPPLY_STATUS_DISCHARGING)
		fg_soc = max77843_low_batt_compensation(
			fuelgauge, fg_soc, fg_vcell, fg_current);

	if (fuelgauge->info.is_first_check)
		fuelgauge->info.is_first_check = false;

	if ((fg_vcell < 3400) && (avg_current < 0) && (fg_soc <= 10))
		fg_soc = 0;

	fuelgauge->info.soc = fg_soc;

return_soc:
	pr_debug("%s: soc(%d), low_batt_alarm(%d)\n",
		__func__, fuelgauge->info.soc,
		fuelgauge->info.is_low_batt_alarm);

	return fg_soc;
}

static irqreturn_t max77843_jig_irq_thread(int irq, void *irq_data)
{
	struct max77843_fuelgauge_data *fuelgauge = irq_data;

	if (fuelgauge->pdata->check_jig_status &&
	    fuelgauge->pdata->check_jig_status())
		max77843_fg_reset_capacity_by_jig_connection(fuelgauge);
	else
		pr_info("%s: jig removed\n", __func__);
	return IRQ_HANDLED;
}

bool max77843_fg_init(struct max77843_fuelgauge_data *fuelgauge)
{
	ktime_t	current_time;
	struct timespec ts;
	u8 data[2] = {0, 0};

#if defined(ANDROID_ALARM_ACTIVATED)
	current_time = alarm_get_elapsed_realtime();
	ts = ktime_to_timespec(current_time);
#else
	current_time = ktime_get_boottime();
	ts = ktime_to_timespec(current_time);
#endif

	fuelgauge->info.fullcap_check_interval = ts.tv_sec;

	fuelgauge->info.is_low_batt_alarm = false;
	fuelgauge->info.is_first_check = true;

	/* Init parameters to prevent wrong compensation. */
	fuelgauge->info.previous_fullcap =
		max77843_read_word(fuelgauge->i2c, FULLCAP_REG);
	fuelgauge->info.previous_vffullcap =
		max77843_read_word(fuelgauge->i2c, FULLCAP_NOM_REG);

	if (fuelgauge->pdata->check_jig_status &&
	    fuelgauge->pdata->check_jig_status())
		max77843_fg_reset_capacity_by_jig_connection(fuelgauge);
	else {
		if (fuelgauge->pdata->jig_irq) {
			int ret;
			ret = request_threaded_irq(fuelgauge->pdata->jig_irq,
					NULL, max77843_jig_irq_thread,
					fuelgauge->pdata->jig_irq_attr,
					"jig-irq", fuelgauge);
			if (ret) {
				pr_info("%s: Failed to Request IRQ\n",
					__func__);
			}
		}
	}

	/* NOT using FG for temperature */
	if (fuelgauge->pdata->thermal_source != SEC_BATTERY_THERMAL_SOURCE_FG) {
		if (max77843_bulk_read(fuelgauge->i2c, CONFIG_REG,
				       2, data) < 0) {
			pr_err ("%s : Failed to read CONFIG_REG\n", __func__);
			return false;
		}
		data[1] |= 0x1;

		if (max77843_bulk_write(fuelgauge->i2c, CONFIG_REG,
					2, data) < 0) {
			pr_info("%s : Failed to write CONFIG_REG\n", __func__);
			return false;
		}
	}

	return true;
}

bool max77843_fg_fuelalert_init(struct max77843_fuelgauge_data *fuelgauge,
				int soc)
{
	/* 1. Set max77843 alert configuration. */
	if (max77843_fg_alert_init(fuelgauge, soc) > 0)
		return true;
	else
		return false;
}

void max77843_fg_fuelalert_set(struct max77843_fuelgauge_data *fuelgauge,
			       int enable)
{
	u8 config_data[2];

	if (max77843_bulk_read(fuelgauge->i2c, CONFIG_REG,
			       2, config_data) < 0)
		pr_err("%s: Failed to read CONFIG_REG\n", __func__);

	if (enable)
		config_data[0] |= ALERT_EN;
	else
		config_data[0] &= ~ALERT_EN;

	pr_info("%s : CONIFG(0x%02x%02x)\n", __func__, config_data[1], config_data[0]);

	if (max77843_bulk_write(fuelgauge->i2c, CONFIG_REG,
				2, config_data) < 0)
		pr_info("%s: Failed to write CONFIG_REG\n", __func__);

}


bool max77843_fg_fuelalert_process(void *irq_data)
{
	struct max77843_fuelgauge_data *fuelgauge =
		(struct max77843_fuelgauge_data *)irq_data;

	max77843_fg_fuelalert_set(fuelgauge, 0);

	return true;
}

bool max77843_fg_reset(struct max77843_fuelgauge_data *fuelgauge)
{
	if (!max77843_fg_reset_soc(fuelgauge))
		return true;
	else
		return false;
}

#define CAPACITY_MAX_CONTROL_THRESHOLD 300

static void max77843_fg_get_scaled_capacity(
	struct max77843_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{
	u16 reg_data;
	union power_supply_propval value;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, value);
	pr_info("%s : CABLE TYPE(%d)\n", __func__, value.intval);

	if (value.intval != POWER_SUPPLY_TYPE_BATTERY) {
		int temp, sample;
		int curr;
		int topoff;
		int capacity_threshold;
		static int cnt;

		curr = max77843_get_fuelgauge_value(fuelgauge, FG_CURRENT_AVG);
		topoff = fuelgauge->pdata->charging_current[value.intval].full_check_current_1st;
		capacity_threshold = topoff + CAPACITY_MAX_CONTROL_THRESHOLD;

		pr_info("%s : curr(%d) topoff(%d)\n", __func__, curr, topoff);

		if ((curr < capacity_threshold) && (curr > topoff)) {
			if (!cnt) {
				cnt = 1;
				val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
					0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
					     (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));
				fuelgauge->standard_capacity = val->intval;
			} else {
				temp = (val->intval < fuelgauge->pdata->capacity_min) ?
					0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
					     (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

				sample = ((capacity_threshold - curr) * (1000 - fuelgauge->standard_capacity)) /
					(capacity_threshold - topoff);

				pr_info("%s : %d = ((%d - %d) * (1000 - %d)) / (%d - %d)\n",
					__func__,
					sample, capacity_threshold, curr, fuelgauge->standard_capacity,
					capacity_threshold, topoff);

				if ((temp - fuelgauge->standard_capacity) >= sample) {
					pr_info("%s : TEMP > SAMPLE\n", __func__);
					val->intval = temp;
				} else if ((sample - (temp - fuelgauge->standard_capacity)) < 5) {
					pr_info("%s : TEMP < SAMPLE && GAP UNDER 5\n", __func__);
					fuelgauge->capacity_max -= (sample - (temp - fuelgauge->standard_capacity));
					val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
						0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
						     (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));
				} else {
					pr_info("%s : TEMP > SAMPLE && GAP OVER 5\n", __func__);
					fuelgauge->capacity_max -= 5;
					val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
						0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
						     (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));
				}

				pr_info("%s : TEMP(%d) SAMPLE(%d) CPAPCITY_MAX(%d)\n",
					__func__, temp, sample, fuelgauge->capacity_max);
			}
		} else {
			cnt = 0;
			val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
				0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
				     (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));
		}
	} else {
		val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
			0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
			     (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));
	}

	reg_data = max77843_read_word(fuelgauge->i2c, 0xD0);
	if (reg_data != fuelgauge->capacity_max) {
		pr_info("%s : 0xD0 Register Update (%d) -> (%d)\n",
			__func__, reg_data, fuelgauge->capacity_max);
		reg_data = fuelgauge->capacity_max;
		max77843_write_word(fuelgauge->i2c, 0xD0, reg_data);
	}

	pr_info("%s : PRE SOC(%d), SOC(%d)\n",
		__func__, fuelgauge->pre_soc, val->intval);

	fuelgauge->pre_soc = val->intval;

	pr_info("%s: scaled capacity (%d.%d)\n",
		 __func__, val->intval/10, val->intval%10);
}

/* capacity is integer */
static void max77843_fg_get_atomic_capacity(
	struct max77843_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{

	pr_info("%s : NOW(%d), OLD(%d)\n",
		__func__, val->intval, fuelgauge->capacity_old);

	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
	if (fuelgauge->capacity_old < val->intval)
		val->intval = fuelgauge->capacity_old + 1;
	else if (fuelgauge->capacity_old > val->intval)
		val->intval = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
			fuelgauge->capacity_old < val->intval) {
			pr_err("%s: capacity (old %d : new %d)\n",
				__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int max77843_fg_calculate_dynamic_scale(
	struct max77843_fuelgauge_data *fuelgauge, int capacity)
{
	union power_supply_propval raw_soc_val;
	raw_soc_val.intval = max77843_get_fuelgauge_value(fuelgauge,
						 FG_RAW_SOC) / 10;

	if (raw_soc_val.intval <
		fuelgauge->pdata->capacity_max -
		fuelgauge->pdata->capacity_max_margin) {
		fuelgauge->capacity_max =
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin;
		pr_debug("%s: capacity_max (%d)", __func__,
			 fuelgauge->capacity_max);
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval >
			fuelgauge->pdata->capacity_max +
			fuelgauge->pdata->capacity_max_margin) ?
			(fuelgauge->pdata->capacity_max +
			fuelgauge->pdata->capacity_max_margin) :
			raw_soc_val.intval;
		pr_debug("%s: raw soc (%d)", __func__,
			 fuelgauge->capacity_max);
	}

	if (capacity != 100) {
		fuelgauge->capacity_max =
			(fuelgauge->capacity_max * 100 / capacity);
		fuelgauge->capacity_old = capacity;
	} else {
		fuelgauge->capacity_max =
			(fuelgauge->capacity_max * 99 / 100);
		fuelgauge->capacity_old = 100;
	}

	pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
		__func__, fuelgauge->capacity_max, capacity);

	return fuelgauge->capacity_max;
}

static void max77843_fg_check_qrtable(struct max77843_fuelgauge_data *fuelgauge)
{
	u16 qrtable20, qrtable30;

	qrtable20 = max77843_read_word(fuelgauge->i2c, QRTABLE20_REG);
	if (qrtable20 != fuelgauge->battery_data->QResidual20) {
		if (max77843_write_word(fuelgauge->i2c, QRTABLE20_REG,
			fuelgauge->battery_data->QResidual20) < 0)
			pr_err("%s: Failed to write QRTABLE20\n", __func__);
	}
	qrtable30 = max77843_read_word(fuelgauge->i2c, QRTABLE30_REG);
	if (qrtable30 != fuelgauge->battery_data->QResidual30) {
		if (max77843_write_word(fuelgauge->i2c, QRTABLE30_REG,
			fuelgauge->battery_data->QResidual30) < 0)
			pr_err("%s: Failed to write QRTABLE30\n", __func__);
	}
	pr_info("%s: QRTABLE20_REG(0x%04x), QRTABLE30_REG(0x%04x)\n", __func__,
		qrtable20, qrtable30);
}

#if defined(CONFIG_EN_OOPS)
static void max77843_set_full_value(struct max77843_fuelgauge_data *fuelgauge,
				    int cable_type)
{
	u16 ichgterm, misccfg, fullsocthr;

	if ((cable_type == POWER_SUPPLY_TYPE_HV_MAINS) ||
	    (cable_type == POWER_SUPPLY_TYPE_HV_ERR)) {
		ichgterm = fuelgauge->battery_data->ichgterm_2nd;
		misccfg = fuelgauge->battery_data->misccfg_2nd;
		fullsocthr = fuelgauge->battery_data->fullsocthr_2nd;
	} else {
		ichgterm = fuelgauge->battery_data->ichgterm;
		misccfg = fuelgauge->battery_data->misccfg;
		fullsocthr = fuelgauge->battery_data->fullsocthr;
	}

	max77843_write_word(fuelgauge->i2c, ICHGTERM_REG, ichgterm);
	max77843_write_word(fuelgauge->i2c, MISCCFG_REG, misccfg);
	max77843_write_word(fuelgauge->i2c, FULLSOCTHR_REG, fullsocthr);

	pr_info("%s : ICHGTERM(0x%04x) FULLSOCTHR(0x%04x), MISCCFG(0x%04x)\n",
		__func__, ichgterm, misccfg, fullsocthr);
}
#endif

static int max77843_fg_get_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	struct max77843_fuelgauge_data *fuelgauge =
		container_of(psy, struct max77843_fuelgauge_data, psy_fg);
	static int abnormal_current_cnt = 0;
	union power_supply_propval value;

	pr_info("%s : %d(%d)\n", __func__, psp, val->intval);

	switch (psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max77843_get_fuelgauge_value(fuelgauge, FG_VOLTAGE);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_OCV:
			val->intval = max77843_fg_read_vfocv(fuelgauge);
			break;
		case SEC_BATTEY_VOLTAGE_AVERAGE:
		default:
			val->intval = max77843_fg_read_avg_vcell(fuelgauge);
			break;
		}
		break;
		/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		switch (val->intval) {
		case SEC_BATTEY_CURRENT_UA:
			val->intval =
				max77843_fg_read_current(fuelgauge,
						SEC_BATTEY_CURRENT_UA);
			break;
		case SEC_BATTEY_CURRENT_MA:
		default:
			val->intval = max77843_get_fuelgauge_value(fuelgauge,
							  FG_CURRENT);
			psy_do_property("battery", get,
					POWER_SUPPLY_PROP_STATUS, value);
			/* To save log for abnormal case */
			if (value.intval == POWER_SUPPLY_STATUS_DISCHARGING && val->intval > 0) {
				abnormal_current_cnt++;
				if (abnormal_current_cnt >= 5) {
					pr_info("%s : Inow is increasing in not charging status\n",
						__func__);
					value.intval = fuelgauge->capacity_old + 15;
					psy_do_property("battery", set,
							POWER_SUPPLY_PROP_CAPACITY, value);
					abnormal_current_cnt = 0;
					value.intval = fuelgauge->capacity_old;
					psy_do_property("battery", set,
							POWER_SUPPLY_PROP_CAPACITY, value);
				}
			} else {
				abnormal_current_cnt = 0;
			}
			break;
		}
		break;
		/* Average Current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		switch (val->intval) {
		case SEC_BATTEY_CURRENT_UA:
			val->intval =
				max77843_fg_read_avg_current(fuelgauge,
						    SEC_BATTEY_CURRENT_UA);
			break;
		case SEC_BATTEY_CURRENT_MA:
		default:
			val->intval =
				max77843_get_fuelgauge_value(fuelgauge,
						    FG_CURRENT_AVG);
			break;
		}
		break;
		/* Full Capacity */
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTEY_CAPACITY_DESIGNED:
			val->intval = max77843_get_fuelgauge_value(fuelgauge,
							  FG_FULLCAP);
			break;
		case SEC_BATTEY_CAPACITY_ABSOLUTE:
			val->intval = max77843_get_fuelgauge_value(fuelgauge,
							  FG_MIXCAP);
			break;
		case SEC_BATTEY_CAPACITY_TEMPERARY:
			val->intval = max77843_get_fuelgauge_value(fuelgauge,
							  FG_AVCAP);
			break;
		case SEC_BATTEY_CAPACITY_CURRENT:
			val->intval = max77843_get_fuelgauge_value(fuelgauge,
							  FG_REPCAP);
			break;
		}
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = max77843_get_fuelgauge_value(fuelgauge,
							  FG_RAW_SOC);
		} else {
			val->intval = max77843_get_fuelgauge_soc(fuelgauge);

			if (fuelgauge->pdata->capacity_calculation_type &
			    (SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
			     SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
				max77843_fg_get_scaled_capacity(fuelgauge, val);

			/* capacity should be between 0% and 100%
			 * (0.1% degree)
			 */
			if (val->intval > 1000)
				val->intval = 1000;
			if (val->intval < 0)
				val->intval = 0;

			/* get only integer part */
			val->intval /= 10;

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
			    fuelgauge->is_fuel_alerted) {
				wake_unlock(&fuelgauge->fuel_alert_wake_lock);
				max77843_fg_fuelalert_init(fuelgauge,
					  fuelgauge->pdata->fuel_alert_soc);
			}

			/* (Only for atomic capacity)
			 * In initial time, capacity_old is 0.
			 * and in resume from sleep,
			 * capacity_old is too different from actual soc.
			 * should update capacity_old
			 * by val->intval in booting or resume.
			 */
			if (fuelgauge->initial_update_of_soc) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;
				fuelgauge->initial_update_of_soc = false;
				break;
			}

			if (fuelgauge->pdata->capacity_calculation_type &
			    (SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
			     SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
				max77843_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = max77843_get_fuelgauge_value(fuelgauge,
						  FG_TEMPERATURE);
		break;
#if defined(CONFIG_EN_OOPS)
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		return -ENODATA;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77843_fg_set_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	struct max77843_fuelgauge_data *fuelgauge =
		container_of(psy, struct max77843_fuelgauge_data, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
#if defined(CONFIG_AFC_CHARGER_MODE)
		max77843_fg_calculate_dynamic_scale(fuelgauge, val->intval);
#else
		max77843_fg_calculate_dynamic_scale(fuelgauge, 100);
#endif
		}
		break;
#if defined(CONFIG_EN_OOPS)
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		max77843_set_full_value(fuelgauge, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			fuelgauge->is_charging = false;
		} else {
			fuelgauge->is_charging = true;

			if (fuelgauge->info.is_low_batt_alarm) {
				pr_info("%s: Reset low_batt_alarm\n",
					 __func__);
				fuelgauge->info.is_low_batt_alarm = false;
			}

			max77843_reset_low_batt_comp_cnt(fuelgauge);
		}
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			fuelgauge->initial_update_of_soc = true;
			if (!max77843_fg_reset(fuelgauge))
				return -EINVAL;
			else
				break;
		}
	case POWER_SUPPLY_PROP_TEMP:
		max77843_fg_write_temp(fuelgauge, val->intval);
		max77843_fg_check_qrtable(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		max77843_fg_reset_capacity_by_jig_connection(fuelgauge);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void max77843_fg_isr_work(struct work_struct *work)
{
	struct max77843_fuelgauge_data *fuelgauge =
		container_of(work, struct max77843_fuelgauge_data, isr_work.work);

	/* process for fuel gauge chip */
	max77843_fg_fuelalert_process(fuelgauge);

	wake_unlock(&fuelgauge->fuel_alert_wake_lock);
}

static irqreturn_t max77843_fg_irq_thread(int irq, void *irq_data)
{
	struct max77843_fuelgauge_data *fuelgauge = irq_data;

	max77843_update_reg(fuelgauge->pmic,
			    MAX77843_PMIC_REG_INTSRC_MASK,
			    MAX77843_IRQSRC_FG,
			    MAX77843_IRQSRC_FG);

	pr_info("%s\n", __func__);

	if (fuelgauge->is_fuel_alerted) {
		return IRQ_HANDLED;
	} else {
		wake_lock(&fuelgauge->fuel_alert_wake_lock);
		fuelgauge->is_fuel_alerted = true;
		schedule_delayed_work(&fuelgauge->isr_work, 0);
	}

	return IRQ_HANDLED;
}

static int max77843_fuelgauge_debugfs_show(struct seq_file *s, void *data)
{
	struct max77843_fuelgauge_data *fuelgauge = s->private;
	int i;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "MAX77843 FUELGAUGE IC :\n");
	seq_printf(s, "===================\n");
	for (i = 0; i < 16; i++) {
		if (i == 12)
			continue;
		for (reg = 0; reg < 0x10; reg++) {
			reg_data = max77843_read_word(fuelgauge->i2c, reg + i * 0x10);
			seq_printf(s, "0x%02x:\t0x%04x\n", reg + i * 0x10, reg_data);
		}
		if (i == 4)
			i = 10;
	}
	seq_printf(s, "\n");
	return 0;
}

static int max77843_fuelgauge_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77843_fuelgauge_debugfs_show, inode->i_private);
}

static const struct file_operations max77843_fuelgauge_debugfs_fops = {
	.open           = max77843_fuelgauge_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#ifdef CONFIG_OF
static int max77843_fuelgauge_parse_dt(struct max77843_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "max77843-fuelgauge");
	sec_battery_platform_data_t *pdata = fuelgauge->pdata;
	int ret;
	int i, len;
	const u32 *p;

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_hv",
				&pdata->capacity_max_hv);
		if (ret < 0) {
			pr_err("%s error reading capacity_max_hv %d\n", __func__, ret);
			fuelgauge->pdata->capacity_max_hv = fuelgauge->pdata->capacity_max;
		}

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);
		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		pdata->repeated_fuelalert = of_property_read_bool(np,
				"fuelgauge,repeated_fuelalert");

		ret = of_property_read_u32(np, "fuelgauge,qrtable20",
					   &fuelgauge->battery_data->QResidual20);
		if (ret < 0)
			pr_err("%s error reading qrtable20 %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,qrtable30",
					   &fuelgauge->battery_data->QResidual30);
		if (ret < 0)
			pr_err("%s error reading qrtabel30 %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,ichgterm",
					   &fuelgauge->battery_data->ichgterm);
		if (ret < 0)
			pr_err("%s error reading ichgterm %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,ichgterm_2nd",
					   &fuelgauge->battery_data->ichgterm_2nd);
		if (ret < 0)
			pr_err("%s error reading ichgterm_2nd %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,misccfg",
					   &fuelgauge->battery_data->misccfg);
		if (ret < 0)
			pr_err("%s error reading misccfg %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,misccfg_2nd",
					   &fuelgauge->battery_data->misccfg_2nd);
		if (ret < 0)
			pr_err("%s error reading misccfg_2nd %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fullsocthr",
					   &fuelgauge->battery_data->fullsocthr);
		if (ret < 0)
			pr_err("%s error reading fullsocthr %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fullsocthr_2nd",
					   &fuelgauge->battery_data->fullsocthr_2nd);
		if (ret < 0)
			pr_err("%s error reading fullsocthr_2nd %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity",
					   &fuelgauge->battery_data->Capacity);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,low_battery_comp_voltage",
			   &fuelgauge->battery_data->low_battery_comp_voltage);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);

		for(i = 0; i < (CURRENT_RANGE_MAX_NUM * TABLE_MAX); i++) {
			ret = of_property_read_u32_index(np,
					 "fuelgauge,low_battery_table",
					 i,
					 &fuelgauge->battery_data->low_battery_table[i/3][i%3]);
			pr_info("[%d]",
				fuelgauge->battery_data->low_battery_table[i/3][i%3]);
			if ((i%3) == 2)
				pr_info("\n");
		}

		np = of_find_node_by_name(NULL, "battery");
		ret = of_property_read_u32(np, "battery,thermal_source",
					   &pdata->thermal_source);
		if (ret < 0) {
			pr_err("%s error reading pdata->thermal_source %d\n",
			       __func__, ret);
		}

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;
		len = len / sizeof(u32);

		pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
						  GFP_KERNEL);

		for(i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				 "battery,input_current_limit", i,
				 &pdata->charging_current[i].input_current_limit);
			ret = of_property_read_u32_index(np,
				 "battery,fast_charging_current", i,
				 &pdata->charging_current[i].fast_charging_current);
			ret = of_property_read_u32_index(np,
				 "battery,full_check_current_1st", i,
				 &pdata->charging_current[i].full_check_current_1st);
			ret = of_property_read_u32_index(np,
				 "battery,full_check_current_2nd", i,
				 &pdata->charging_current[i].full_check_current_2nd);
		}

		pr_info("%s fg_irq: %d, capacity_max: %d\n"
			"qrtable20: 0x%x, qrtable30 : 0x%x\n"
			"cpacity_max_margin: %d, capacity_min: %d\n"
			"calculation_type: 0x%x, fuel_alert_soc: %d,\n"
			"repeated_fuelalert: %d\n",
			__func__, pdata->fg_irq, pdata->capacity_max,
			fuelgauge->battery_data->QResidual20,
			fuelgauge->battery_data->QResidual30,
			pdata->capacity_max_margin, pdata->capacity_min,
			pdata->capacity_calculation_type, pdata->fuel_alert_soc,
			pdata->repeated_fuelalert);
	}

	pr_info("[%s][%d][%d]\n",
		__func__, fuelgauge->battery_data->Capacity,
	        fuelgauge->battery_data->low_battery_comp_voltage);

	return 0;
}
#endif

static int __devinit max77843_fuelgauge_probe(struct platform_device *pdev)
{
	struct max77843_dev *max77843 = dev_get_drvdata(pdev->dev.parent);
	struct max77843_platform_data *pdata = dev_get_platdata(max77843->dev);
	struct max77843_fuelgauge_data *fuelgauge;
	int ret = 0;
	union power_supply_propval raw_soc_val;
	u16 reg_data;

	pr_info("%s: MAX77843 Fuelgauge Driver Loading\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	pdata->fuelgauge_data = kzalloc(sizeof(sec_battery_platform_data_t), GFP_KERNEL);
	if (!pdata->fuelgauge_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->dev = &pdev->dev;
	fuelgauge->pdata = pdata->fuelgauge_data;
	fuelgauge->i2c = max77843->fuelgauge;
	fuelgauge->pmic = max77843->i2c;
	fuelgauge->max77843_pdata = pdata;

#if defined(CONFIG_OF)
	fuelgauge->battery_data = kzalloc(sizeof(struct battery_data_t),
					  GFP_KERNEL);
	if(!fuelgauge->battery_data) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_pdata_free;
	}
	ret = max77843_fuelgauge_parse_dt(fuelgauge);
	if (ret < 0) {
		pr_err("%s not found charger dt! ret[%d]\n",
		       __func__, ret);
	}
#endif

	platform_set_drvdata(pdev, fuelgauge);

	fuelgauge->psy_fg.name		= "max77843-fuelgauge";
	fuelgauge->psy_fg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.get_property	= max77843_fg_get_property;
	fuelgauge->psy_fg.set_property	= max77843_fg_set_property;
	fuelgauge->psy_fg.properties	= max77843_fuelgauge_props;
	fuelgauge->psy_fg.num_properties =
		ARRAY_SIZE(max77843_fuelgauge_props);
	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	raw_soc_val.intval = max77843_get_fuelgauge_value(fuelgauge, FG_RAW_SOC) / 10;

	reg_data = max77843_read_word(fuelgauge->i2c, 0xD0);

	if (reg_data >= 900 && reg_data <= 1000 && reg_data != fuelgauge->capacity_max) {
		pr_info("%s : Capacity Max Update (%d) -> (%d)\n",
			__func__, fuelgauge->capacity_max, reg_data);
		fuelgauge->capacity_max = reg_data;
	} else {
		pr_info("%s : 0xD0 Register Update (%d) -> (%d)\n",
			__func__, reg_data, fuelgauge->capacity_max);
		reg_data = fuelgauge->capacity_max;
		max77843_write_word(fuelgauge->i2c, 0xD0, reg_data);
	}

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		max77843_fg_calculate_dynamic_scale(fuelgauge, 100);

	(void) debugfs_create_file("max77843-fuelgauge-regs",
		S_IRUGO, NULL, (void *)fuelgauge, &max77843_fuelgauge_debugfs_fops);

	if (!max77843_fg_init(fuelgauge)) {
		pr_err("%s: Failed to Initialize Fuelgauge\n", __func__);
		goto err_data_free;
	}

	ret = power_supply_register(&pdev->dev, &fuelgauge->psy_fg);
	if (ret) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		goto err_data_free;
	}

	fuelgauge->fg_irq = pdata->irq_base + MAX77843_FG_IRQ_ALERT;
	pr_info("[%s]IRQ_BASE(%d) FG_IRQ(%d)\n",
		__func__, pdata->irq_base, fuelgauge->fg_irq);

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		if (max77843_fg_fuelalert_init(fuelgauge,
				       fuelgauge->pdata->fuel_alert_soc)) {
			wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
				       WAKE_LOCK_SUSPEND, "fuel_alerted");
			if (fuelgauge->fg_irq) {
				INIT_DELAYED_WORK(&fuelgauge->isr_work, max77843_fg_isr_work);

				ret = request_threaded_irq(fuelgauge->fg_irq,
					   NULL, max77843_fg_irq_thread,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					   "fuelgauge-irq", fuelgauge);
				if (ret) {
					pr_err("%s: Failed to Request IRQ\n", __func__);
					goto err_supply_unreg;
				}
			}
		} else {
			pr_err("%s: Failed to Initialize Fuel-alert\n",
			       __func__);
			goto err_supply_unreg;
		}
	}

	fuelgauge->initial_update_of_soc = true;

	pr_info("%s: MAX77843 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_supply_unreg:
	power_supply_unregister(&fuelgauge->psy_fg);
err_data_free:
#if defined(CONFIG_OF)
	kfree(fuelgauge->battery_data);
#endif
err_pdata_free:
	kfree(pdata->fuelgauge_data);
	mutex_destroy(&fuelgauge->fg_lock);
err_free:
	kfree(fuelgauge);

	return ret;
}

static int __devexit max77843_fuelgauge_remove(struct platform_device *pdev)
{
	struct max77843_fuelgauge_data *fuelgauge =
		platform_get_drvdata(pdev);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

static int max77843_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int max77843_fuelgauge_resume(struct device *dev)
{
	struct max77843_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	fuelgauge->initial_update_of_soc = true;

	return 0;
}

static void max77843_fuelgauge_shutdown(struct device *dev)
{
}

static SIMPLE_DEV_PM_OPS(max77843_fuelgauge_pm_ops, max77843_fuelgauge_suspend,
			 max77843_fuelgauge_resume);

static struct platform_driver max77843_fuelgauge_driver = {
	.driver = {
		   .name = "max77843-fuelgauge",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &max77843_fuelgauge_pm_ops,
#endif
		.shutdown = max77843_fuelgauge_shutdown,
	},
	.probe	= max77843_fuelgauge_probe,
	.remove	= __devexit_p(max77843_fuelgauge_remove),
};

static int __init max77843_fuelgauge_init(void)
{
	pr_info("%s: \n", __func__);
	return platform_driver_register(&max77843_fuelgauge_driver);
}

static void __exit max77843_fuelgauge_exit(void)
{
	platform_driver_unregister(&max77843_fuelgauge_driver);
}
module_init(max77843_fuelgauge_init);
module_exit(max77843_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung MAX77843 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
