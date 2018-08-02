/* drivers/battery/s2mu003_charger.c
 * S2MU003 Charger Driver
 *
 * Copyright (C) 2013
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/mfd/samsung/s2mu003.h>
#include <linux/battery/charger/s2mu003_charger.h>
#include <linux/version.h>

#define ENABLE_MIVR 0

#define EN_OVP_IRQ 1
#define EN_IEOC_IRQ 1
#define EN_RECHG_REQ_IRQ 0
#define EN_TR_IRQ 0
#define EN_MIVR_SW_REGULATION 0
#define EN_BST_IRQ 0
#define MINVAL(a, b) ((a <= b) ? a : b)

#define EOC_DEBOUNCE_CNT 2
#define HEALTH_DEBOUNCE_CNT 3

#define EOC_SLEEP 200
#define EOC_TIMEOUT (EOC_SLEEP * 6)
#ifndef EN_TEST_READ
#define EN_TEST_READ 0
#endif

static int s2mu003_reg_map[] = {
	S2MU003_CHG_STATUS1,
	S2MU003_CHG_CTRL1,
	S2MU003_CHG_CTRL2,
	S2MU003_CHG_CTRL3,
	S2MU003_CHG_CTRL4,
	S2MU003_CHG_CTRL5,
	S2MU003_SOFTRESET,
	S2MU003_CHG_CTRL6,
	S2MU003_CHG_CTRL7,
	S2MU003_CHG_CTRL8,
	S2MU003_CHG_STATUS2,
	S2MU003_CHG_STATUS3,
	S2MU003_CHG_STATUS4,
	S2MU003_CHG_CTRL9,
};

struct s2mu003_charger_data {
	struct i2c_client       *client;
	s2mu003_mfd_chip_t	*s2mu003;
	struct power_supply	psy_chg;
	s2mu003_charger_platform_data_t *pdata;
	int charging_current;
	int siop_level;
	int cable_type;
	bool is_charging;
	struct mutex io_lock;

	/* register programming */
	int reg_addr;
	int reg_data;

	bool full_charged;
	bool ovp;
	int unhealth_cnt;
	int status;
};

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
};

static int s2mu003_get_charging_health(struct s2mu003_charger_data *charger);

static void s2mu003_read_regs(struct i2c_client *i2c, char *str)
{
	u8 data = 0;
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(s2mu003_reg_map); i++) {
		data = s2mu003_reg_read(i2c, s2mu003_reg_map[i]);
		sprintf(str+strlen(str), "0x%02x, ", data);
	}
}

static void s2mu003_test_read(struct i2c_client *i2c)
{
	int data;
	char str[1024] = {0,};
	int i;

	/* S2MU003 REG: 0x00 ~ 0x08 */
	for (i = 0x0; i <= 0x0E; i++) {
		data = s2mu003_reg_read(i2c, i);
		sprintf(str+strlen(str), "0x%02x, ", data);
	}

	pr_info("%s: %s\n", __func__, str);
}

static void s2mu003_charger_otg_control(struct s2mu003_charger_data *charger,
		bool enable)
{
	pr_info("%s: called charger otg control : %s\n", __func__,
			enable ? "on" : "off");

	if (!enable) {
		/* turn off OTG */
		s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL1, S2MU003_OPAMODE_MASK);
		s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL8, 0x80);
	} else {
		/* Set OTG boost vout = 5V, turn on OTG */
		s2mu003_assign_bits(charger->client,
				S2MU003_CHG_CTRL2, S2MU003_VOREG_MASK,
				0x23 << S2MU003_VOREG_SHIFT);
		s2mu003_set_bits(charger->client,
				S2MU003_CHG_CTRL1, S2MU003_OPAMODE_MASK);
		s2mu003_set_bits(charger->client, S2MU003_CHG_CTRL8, 0x80);
		charger->cable_type = POWER_SUPPLY_TYPE_OTG;
	}
}

/* this function will work well on CHIP_REV = 3 or later */
static void s2mu003_enable_charger_switch(struct s2mu003_charger_data *charger,
		int onoff)
{
	int prev_charging_status = charger->is_charging;
	union power_supply_propval val;

	charger->is_charging = onoff ? true : false;
	if ((onoff > 0) && (prev_charging_status == false)) {
		pr_info("%s: turn on charger\n", __func__);
		s2mu003_set_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
	} else if (onoff == 0) {
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_STATUS, val);
		if (val.intval != POWER_SUPPLY_STATUS_FULL)
			charger->full_charged = false;
		pr_info("%s: turn off charger\n", __func__);
		charger->charging_current = 0;
		s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
	} else {
		pr_info("%s: repeated to set charger switch(%d), prev stat = %d\n",
				__func__, onoff, prev_charging_status ? 1 : 0);
	}
}

static void s2mu003_enable_charging_termination(struct i2c_client *i2c,
		int onoff)
{
	pr_info("%s:[BATT] Do termination set(%d)\n", __func__, onoff);
	if (onoff)
		s2mu003_set_bits(i2c, S2MU003_CHG_CTRL1, S2MU003_TEEN_MASK);
	else
		s2mu003_clr_bits(i2c, S2MU003_CHG_CTRL1, S2MU003_TEEN_MASK);
}

static int s2mu003_input_current_limit[] = {
	100,
	500,
	700,
	900,
	1000,
	1500,
	2000,
};

static void s2mu003_set_input_current_limit(struct s2mu003_charger_data *charger,
		int current_limit)
{
	int i = 0, curr_reg = 0;

	for (i = 0; i < ARRAY_SIZE(s2mu003_input_current_limit); i++) {
		if (current_limit <= s2mu003_input_current_limit[i])
			curr_reg = i+1;
	}

	if (curr_reg < 0)
		curr_reg = 0;
	mutex_lock(&charger->io_lock);
	s2mu003_assign_bits(charger->client, S2MU003_CHG_CTRL1, S2MU003_AICR_LIMIT_MASK,
			curr_reg << S2MU003_AICR_LIMIT_SHIFT);
	mutex_unlock(&charger->io_lock);
}

static int s2mu003_get_input_current_limit(struct i2c_client *i2c)
{
	int ret;
	ret = s2mu003_reg_read(i2c, S2MU003_CHG_CTRL1);
	if (ret < 0)
		return ret;
	ret &= S2MU003_AICR_LIMIT_MASK;
	ret >>= S2MU003_AICR_LIMIT_SHIFT;
	if (ret == 0)
		return 2000 + 1; /* no limitation */

	return s2mu003_input_current_limit[ret - 1];
}

static void s2mu003_set_regulation_voltage(struct s2mu003_charger_data *charger,
		int float_voltage)
{
	int data;

	if (float_voltage < 3650)
		data = 0;
	else if (float_voltage >= 3650 && float_voltage <= 4375)
		data = (float_voltage - 3650) / 25;
	else
		data = 0x23;

	mutex_lock(&charger->io_lock);
	s2mu003_assign_bits(charger->client,
			S2MU003_CHG_CTRL2, S2MU003_VOREG_MASK,
			data << S2MU003_VOREG_SHIFT);
	mutex_unlock(&charger->io_lock);
}

static void s2mu003_set_fast_charging_current(struct i2c_client *i2c,
		int charging_current)
{
	int data;

	if (charging_current < 700)
		data = 0;
	else if (charging_current >= 700 && charging_current <= 2000)
		data = (charging_current - 700) / 100;
	else
		data = 0xd;

	s2mu003_assign_bits(i2c, S2MU003_CHG_CTRL5, S2MU003_ICHRG_MASK,
			data << S2MU003_ICHRG_SHIFT);
}

static int s2mu003_eoc_level[] = {
	0,
	150,
	200,
	250,
	300,
	400,
	500,
	600,
};

static int s2mu003_get_eoc_level(int eoc_current)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(s2mu003_eoc_level); i++) {
		if (eoc_current < s2mu003_eoc_level[i]) {
			if (i == 0)
				return 0;
			return i - 1;
		}
	}

	return ARRAY_SIZE(s2mu003_eoc_level) - 1;
}

static int s2mu003_get_current_eoc_setting(struct s2mu003_charger_data *charger)
{
	int ret;
	mutex_lock(&charger->io_lock);
	ret = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL4);
	mutex_unlock(&charger->io_lock);
	if (ret < 0) {
		pr_info("%s: warning --> fail to read i2c register(%d)\n", __func__, ret);
		return ret;
	}
	return s2mu003_eoc_level[(S2MU003_IEOC_MASK & ret) >> S2MU003_IEOC_SHIFT];
}

static int s2mu003_get_fast_charging_current(struct i2c_client *i2c)
{
	int data = s2mu003_reg_read(i2c, S2MU003_CHG_CTRL5);
	if (data < 0)
		return data;

	data = (data >> 4) & 0x0f;

	if (data > 0xd)
		data = 0xd;
	return data * 100 + 700;
}

static void s2mu003_set_termination_current_limit(struct i2c_client *i2c,
		int current_limit)
{
	int data = s2mu003_get_eoc_level(current_limit);
	int ret;

	pr_info("%s : Set Termination\n", __func__);

	ret = s2mu003_assign_bits(i2c, S2MU003_CHG_CTRL4, S2MU003_IEOC_MASK,
			data << S2MU003_IEOC_SHIFT);
	/* reset chg_en */
	s2mu003_clr_bits(i2c, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
	s2mu003_set_bits(i2c, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
}
/* eoc re set */
static void s2mu003_set_charging_current(struct s2mu003_charger_data *charger,
		int eoc)
{
	int adj_current = 0;

	adj_current = charger->charging_current * charger->siop_level / 100;
	mutex_lock(&charger->io_lock);
	s2mu003_set_fast_charging_current(charger->client,
			adj_current);
	if (eoc) {
		/* set EOC RESET */
		s2mu003_set_termination_current_limit(charger->client, eoc);
	}
	mutex_unlock(&charger->io_lock);
}

enum {
	S2MU003_MIVR_DISABLE = 0,
	S2MU003_MIVR_4200MV,
	S2MU003_MIVR_4300MV,
	S2MU003_MIVR_4400MV,
	S2MU003_MIVR_4500MV,
	S2MU003_MIVR_4600MV,
	S2MU003_MIVR_4700MV,
	S2MU003_MIVR_4800MV,
};

#if ENABLE_MIVR
/* charger input regulation voltage setting */
static void s2mu003_set_mivr_level(struct s2mu003_charger_data *charger)
{
	int mivr;

	switch (charger->cable_type) {
	case POWER_SUPPLY_TYPE_USB ... POWER_SUPPLY_TYPE_USB_ACA:
		mivr = S2MU003_MIVR_4600MV;
		break;
	default:
		mivr = S2MU003_MIVR_DISABLE;
	}
	mutex_lock(&charger->io_lock);
	s2mu003_assign_bits(charger->i2c_client,
			S2MU003_CHG_CTRL4, S2MU003_MIVR_MASK, mivr << S2MU003_MIVR_SHIFT);
	mutex_unlock(&charger->io_lock);
}
#endif /*ENABLE_MIVR*/

static void s2mu003_configure_charger(struct s2mu003_charger_data *charger)
{

	int eoc;
	union power_supply_propval val;

	pr_info("%s : Set config charging\n", __func__);
	if (charger->charging_current < 0) {
		pr_info("%s : OTG is activated. Ignore command!\n",
				__func__);
		return;
	}

#if ENABLE_MIVR
	s2mu003_set_mivr_level(charger);
#endif /*DISABLE_MIVR*/
	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, val);

	/* TEMP_TEST : disable charging current termination for 2nd charging */
	/* s2mu003_enable_charging_termination(charger->s2mu003->i2c_client, 1);*/

	/* Input current limit */
	pr_info("%s : input current (%dmA)\n",
			__func__, charger->pdata->charging_current_table
			[charger->cable_type].input_current_limit);

	s2mu003_set_input_current_limit(charger,
			charger->pdata->charging_current_table
			[charger->cable_type].input_current_limit);

	/* Float voltage */
	pr_info("%s : float voltage (%dmV)\n",
			__func__, charger->pdata->chg_float_voltage);

	s2mu003_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);

	charger->charging_current = charger->pdata->charging_current_table
		[charger->cable_type].fast_charging_current;
	eoc = charger->pdata->charging_current_table
		[charger->cable_type].full_check_current_1st;
	/* Fast charge and Termination current */
	pr_info("%s : fast charging current (%dmA)\n",
			__func__, charger->charging_current);

	pr_info("%s : termination current (%dmA)\n",
			__func__, eoc);

	s2mu003_set_charging_current(charger, eoc);
	s2mu003_enable_charger_switch(charger, 1);
}

/* here is set init charger data */
static bool s2mu003_chg_init(struct s2mu003_charger_data *charger)
{
	int ret = 0, rev_id;

	s2mu003_set_bits(charger->client, 0x6b, 0x01);
	/* delay 100 us to wait for normal read (from e-fuse) */
	msleep(20);
	ret = s2mu003_reg_read(charger->client, 0x03);
	s2mu003_clr_bits(charger->client, 0x6b, 0x01);
	if (ret < 0)
		pr_err("%s : failed to read revision ID\n", __func__);

	rev_id = ret & 0x0f;

	if (charger->pdata->is_750kHz_switching)
		ret = s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL1,
				S2MU003_SEL_SWFREQ_MASK);
	else
		ret = s2mu003_set_bits(charger->client, S2MU003_CHG_CTRL1,
				S2MU003_SEL_SWFREQ_MASK);

	/* Disable Timer function (Charging timeout fault) */
	s2mu003_clr_bits(charger->client,
			S2MU003_CHG_CTRL3, S2MU003_TIMEREN_MASK);
	s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
	s2mu003_set_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);

	/* Disable TE */
	s2mu003_enable_charging_termination(charger->client, 0);

	/* EMI improvement , let reg0x18 bit2~5 be 1100*/
	/* s2mu003_assign_bits(charger->s2mu003->i2c_client, 0x18, 0x3C, 0x30); */

	/* MUST set correct regulation voltage first
	 * Before MUIC pass cable type information to charger
	 * charger would be already enabled (default setting)
	 * it might cause EOC event by incorrect regulation voltage */
	s2mu003_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);
#if !(ENABLE_MIVR)
	s2mu003_assign_bits(charger->client,
			S2MU003_CHG_CTRL4, S2MU003_MIVR_MASK,
			S2MU003_MIVR_DISABLE << S2MU003_MIVR_SHIFT);
#endif
	return true;
}

static int s2mu003_get_charging_status(struct s2mu003_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;

	ret = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS1);
	if (ret < 0) {
		pr_info("Error : can't get charging status (%d)\n", ret);

	}

	if (charger->full_charged)
		return POWER_SUPPLY_STATUS_FULL;

	switch (ret & 0x30) {
	case 0x00:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 0x20:
	case 0x10:
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x30:
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	}

	/* TEMP_TEST : when OTG is enabled(charging_current -1), handle OTG func. */
	if (charger->charging_current < 0) {
		/* For OTG mode, S2MU003 would still report "charging" */
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		ret = s2mu003_reg_read(charger->client, S2MU003_CHG_IRQ3);
		if (ret & 0x80) {
			pr_info("%s: otg overcurrent limit\n", __func__);
			s2mu003_charger_otg_control(charger, false);
		}

	}

	return status;
}

static int s2mu003_get_charge_type(struct i2c_client *iic)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;

	ret = s2mu003_reg_read(iic, S2MU003_CHG_STATUS1);
	if (ret < 0)
		dev_err(&iic->dev, "%s fail\n", __func__);

	switch (ret&0x40) {
	case 0x40:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		/* pre-charge mode */
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	}

	return status;
}

static bool s2mu003_get_batt_present(struct i2c_client *iic)
{
	int ret = s2mu003_reg_read(iic, S2MU003_CHG_STATUS2);
	if (ret < 0)
		return false;
	return (ret & 0x08) ? false : true;
}

static int s2mu003_get_charging_health(struct s2mu003_charger_data *charger)
{
	int ret = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS1);

	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (ret & (0x03 << 2)) {
		charger->ovp = false;
		charger->unhealth_cnt = 0;
		return POWER_SUPPLY_HEALTH_GOOD;
	}
	charger->unhealth_cnt++;
	if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT)
		return POWER_SUPPLY_HEALTH_GOOD;

	charger->unhealth_cnt = HEALTH_DEBOUNCE_CNT;
	if (charger->ovp)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	return POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
}

static int sec_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int chg_curr, aicr;
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mu003_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mu003_get_charging_health(charger);
		s2mu003_test_read(charger->client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 2000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			aicr = s2mu003_get_input_current_limit(charger->client);
			chg_curr = s2mu003_get_fast_charging_current(charger->client);
			val->intval = MINVAL(aicr, chg_curr);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mu003_get_charge_type(charger->client);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mu003_get_batt_present(charger->client);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);

	int eoc;
	int previous_cable_type = charger->cable_type;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
				charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			pr_info("%s:[BATT] Type Battery\n", __func__);
			s2mu003_enable_charger_switch(charger, 0);

			if (previous_cable_type == POWER_SUPPLY_TYPE_OTG)
				s2mu003_charger_otg_control(charger, false);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_OTG) {
			pr_info("%s: OTG mode\n", __func__);
			s2mu003_charger_otg_control(charger, true);
		} else {
			pr_info("%s:[BATT] Set charging"
					", Cable type = %d\n", __func__, charger->cable_type);
			/* Enable charger */
			s2mu003_configure_charger(charger);
		}
#if EN_TEST_READ
		msleep(100);
		s2mu003_test_read(charger->s2mu003->i2c_client);
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* set charging current */
		if (charger->is_charging) {
			/* decrease the charging current according to siop level */
			charger->siop_level = val->intval;
			pr_info("%s:SIOP level = %d, chg current = %d\n", __func__,
					val->intval, charger->charging_current);
			eoc = s2mu003_get_current_eoc_setting(charger);
			s2mu003_set_charging_current(charger, 0);
		}
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		eoc = s2mu003_get_current_eoc_setting(charger);
		pr_info("%s:Set Power Now -> chg current = %d mA, eoc = %d mA\n", __func__,
				val->intval, eoc);
		s2mu003_set_charging_current(charger, 0);
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		s2mu003_charger_otg_control(charger, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

ssize_t s2mu003_chg_show_attrs(struct device *dev,
		const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);
	int i = 0;
	char *str = NULL;

	switch (offset) {
	case CHG_REG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
				charger->reg_addr);
		break;
	case CHG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
				charger->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char) * 256, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		s2mu003_read_regs(charger->client, str);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				str);

		kfree(str);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t s2mu003_chg_store_attrs(struct device *dev,
		const ptrdiff_t offset,
		const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);

	int ret = 0;
	int x = 0;
	uint8_t data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			charger->reg_addr = x;
			data = s2mu003_reg_read(charger->client,
					charger->reg_addr);
			charger->reg_data = data;
			dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
					__func__, charger->reg_addr, charger->reg_data);
			ret = count;
		}
		break;
	case CHG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data = (u8)x;

			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
					__func__, charger->reg_addr, data);
			ret = s2mu003_reg_write(charger->client,
					charger->reg_addr, data);
			if (ret < 0) {
				dev_dbg(dev, "I2C write fail Reg0x%x = 0x%x\n",
						(int)charger->reg_addr, (int)data);
			}
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

struct s2mu003_chg_irq_handler {
	char *name;
	int irq_index;
	irqreturn_t (*handler)(int irq, void *data);
};

#if EN_OVP_IRQ
static irqreturn_t s2mu003_chg_vin_ovp_irq_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;
	union power_supply_propval value;
	int status;

	/* Delay 100ms for debounce */
	msleep(100);
	status = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS1);

	/* PWR ready = 0*/
	if ((status & (0x04)) == 0) {
		/* No need to disable charger,
		 * H/W will do it automatically */
		charger->unhealth_cnt = HEALTH_DEBOUNCE_CNT;
		charger->ovp = true;
		pr_info("%s: OVP triggered\n", __func__);
		value.intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, value);
	} else {
		charger->unhealth_cnt = 0;
		charger->ovp = false;
	}

	return IRQ_HANDLED;
}
#endif /* EN_OVP_IRQ */

#if EN_IEOC_IRQ
static irqreturn_t s2mu003_chg_ieoc_irq_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;

	pr_info("%s : Full charged\n", __func__);
	charger->full_charged = true;
	return IRQ_HANDLED;
}
#endif /* EN_IEOC_IRQ */

#if EN_RECHG_REQ_IRQ
static irqreturn_t s2mu003_chg_rechg_request_irq_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;
	pr_info("%s: Recharging requesting\n", __func__);

	charger->full_charged = false;

	return IRQ_HANDLED;
}
#endif /* EN_RECHG_REQ_IRQ */

#if EN_TR_IRQ
static irqreturn_t s2mu003_chg_otp_tr_irq_handler(int irq, void *data)
{
	pr_info("%s : Over temperature : thermal regulation loop active\n",
			__func__);
	/* if needs callback, do it here */
	return IRQ_HANDLED;
}
#endif

const struct s2mu003_chg_irq_handler s2mu003_chg_irq_handlers[] = {
#if EN_OVP_IRQ
	{
		.name = "chg_cinovp",
		.handler = s2mu003_chg_vin_ovp_irq_handler,
		.irq_index = S2MU003_CINOVP_IRQ,
	},
#endif /* EN_OVP_IRQ */
#if EN_IEOC_IRQ
	{
		.name = "chg_eoc",
		.handler = s2mu003_chg_ieoc_irq_handler,
		.irq_index = S2MU003_EOC_IRQ,
	},
#endif /* EN_IEOC_IRQ */
#if EN_RECHG_REQ_IRQ
	{
		.name = "chg_rechg",
		.handler = s2mu003_chg_rechg_request_irq_handler,
		.irq_index = S2MU003_RECHG_IRQ,
	},
#endif /* EN_RECHG_REQ_IRQ*/
#if EN_TR_IRQ
	{
		.name = "chg_chgtr",
		.handler = s2mu003_chg_otp_tr_irq_handler,
		.irq_index = S2MU003_CHGTR_IRQ,
	},
#endif /* EN_TR_IRQ */

#if EN_MIVR_SW_REGULATION
	{
		.name = "chg_chgvinvr",
		.handler = s2mu003_chg_mivr_irq_handler,
		.irq_index = S2MU003_CHGVINVR_IRQ,
	},
#endif /* EN_MIVR_SW_REGULATION */
#if EN_BST_IRQ
	{
		.name = "chg_bstinlv",
		.handler = s2mu003_chg_otg_fail_irq_handler,
		.irq_index = S2MU003_BSTINLV_IRQ,
	},
	{
		.name = "chg_bstilim",
		.handler = s2mu003_chg_otg_fail_irq_handler,
		.irq_index = S2MU003_BSTILIM_IRQ,
	},
	{
		.name = "chg_vmidovp",
		.handler = s2mu003_chg_otg_fail_irq_handler,
		.irq_index = S2MU003_VMIDOVP_IRQ,
	},
#endif /* EN_BST_IRQ */
};

static int register_irq(struct platform_device *pdev,
		struct s2mu003_charger_data *charger)
{
	int irq;
	int i, j;
	int ret;
	const struct s2mu003_chg_irq_handler *irq_handler = s2mu003_chg_irq_handlers;
	const char *irq_name;
	for (i = 0; i < ARRAY_SIZE(s2mu003_chg_irq_handlers); i++) {
		irq_name = s2mu003_get_irq_name_by_index(irq_handler[i].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		ret = request_threaded_irq(irq, NULL, irq_handler[i].handler,
				IRQF_ONESHOT | IRQF_TRIGGER_RISING |
				IRQF_NO_SUSPEND, irq_name, charger);
		if (ret < 0) {
			pr_err("%s : Failed to request IRQ (%s): #%d: %d\n",
					__func__, irq_name, irq, ret);
			goto err_irq;
		}

		pr_info("%s : Register IRQ%d(%s) successfully\n",
				__func__, irq, irq_name);
	}

	return 0;
err_irq:
	for (j = 0; j < i; j++) {
		irq_name = s2mu003_get_irq_name_by_index(irq_handler[j].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		free_irq(irq, charger);
	}

	return ret;
}

static void unregister_irq(struct platform_device *pdev,
		struct s2mu003_charger_data *charger)
{
	int irq;
	int i;
	const char *irq_name;
	const struct s2mu003_chg_irq_handler *irq_handler = s2mu003_chg_irq_handlers;

	for (i = 0; i < ARRAY_SIZE(s2mu003_chg_irq_handlers); i++) {
		irq_name = s2mu003_get_irq_name_by_index(irq_handler[i].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		free_irq(irq, charger);
	}
}


#ifdef CONFIG_OF
static int s2mu003_charger_parse_dt(struct device *dev,
		struct s2mu003_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu003-charger");
	const u32 *p;
	int ret, i, len;

	if (of_find_property(np, "is_750kHz_switching", NULL))
		pdata->is_750kHz_switching = 1;
	if (of_find_property(np, "is_fixed_switching", NULL))
		pdata->is_fixed_switching = 1;
	pr_info("%s : is_750kHz_switching = %d\n", __func__,
			pdata->is_750kHz_switching);
	pr_info("%s : is_fixed_switching = %d\n", __func__,
			pdata->is_fixed_switching);

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
				&pdata->chg_float_voltage);
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np,
				"battery,charger_name", (char const **)&pdata->charger_name);

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->charging_current_table = kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
					"battery,input_current_limit", i,
					&pdata->charging_current_table[i].input_current_limit);
			ret = of_property_read_u32_index(np,
					"battery,fast_charging_current", i,
					&pdata->charging_current_table[i].fast_charging_current);
			ret = of_property_read_u32_index(np,
					"battery,full_check_current_1st", i,
					&pdata->charging_current_table[i].full_check_current_1st);
			ret = of_property_read_u32_index(np,
					"battery,full_check_current_2nd", i,
					&pdata->charging_current_table[i].full_check_current_2nd);
		}
	}

	dev_info(dev, "s2mu003 charger parse dt retval = %d\n", ret);
	return ret;
}

static struct of_device_id s2mu003_charger_match_table[] = {
	{ .compatible = "samsung,s2mu003-charger",},
	{},
};
#else
static int s2mu003_charger_parse_dt(struct device *dev,
		struct s2mu003_charger_platform_data *pdata)
{
	return -ENOSYS;
}

#define s2mu003_charger_match_table NULL
#endif /* CONFIG_OF */

static int s2mu003_charger_probe(struct platform_device *pdev)
{
	s2mu003_mfd_chip_t *chip = dev_get_drvdata(pdev->dev.parent);
#ifndef CONFIG_OF
	struct s2mu003_mfd_platform_data *mfd_pdata = dev_get_platdata(chip->dev);
#endif
	struct s2mu003_charger_data *charger;
	int ret = 0;

	pr_info("%s:[BATT] S2MU003 Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->io_lock);

	charger->s2mu003 = chip;
	charger->client = chip->i2c_client;

#ifdef CONFIG_OF
	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mu003_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;
#else
	charger->pdata = mfd_pdata->charger_platform_data;
#endif

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "sec-charger";

	charger->psy_chg.name           = charger->pdata->charger_name;
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = sec_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);

	charger->siop_level = 100;
	s2mu003_chg_init(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}
	ret = register_irq(pdev, charger);
	if (ret < 0)
		goto err_reg_irq;

	s2mu003_test_read(charger->client);
	pr_info("%s:[BATT] S2MU003 charger driver loaded OK\n", __func__);

	return 0;
err_reg_irq:
	power_supply_unregister(&charger->psy_chg);
err_power_supply_register:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return ret;
}

static int s2mu003_charger_remove(struct platform_device *pdev)
{
	struct s2mu003_charger_data *charger =
		platform_get_drvdata(pdev);

	unregister_irq(pdev, charger);
	power_supply_unregister(&charger->psy_chg);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mu003_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mu003_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mu003_charger_suspend NULL
#define s2mu003_charger_resume NULL
#endif

static void s2mu003_charger_shutdown(struct device *dev)
{
	pr_info("%s: S2MU003 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mu003_charger_pm_ops, s2mu003_charger_suspend,
		s2mu003_charger_resume);

static struct platform_driver s2mu003_charger_driver = {
	.driver         = {
		.name   = "s2mu003-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mu003_charger_match_table,
		.pm     = &s2mu003_charger_pm_ops,
		.shutdown = s2mu003_charger_shutdown,
	},
	.probe          = s2mu003_charger_probe,
	.remove		= s2mu003_charger_remove,
};

static int __init s2mu003_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mu003_charger_driver);

	return ret;
}
subsys_initcall(s2mu003_charger_init);

static void __exit s2mu003_charger_exit(void)
{
	platform_driver_unregister(&s2mu003_charger_driver);
}
module_exit(s2mu003_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com");
MODULE_DESCRIPTION("Charger driver for S2MU003");
