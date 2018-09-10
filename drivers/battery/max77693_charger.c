/*
 *  max77693_charger.c
 *  Samsung max77693 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#define DEBUG

#define ENABLE 1
#define DISABLE 0

static struct dentry    *max77693_dentry;

struct max77693_charger_data {
	struct max77693_dev	*max77693;

	struct power_supply	psy_chg;

	struct workqueue_struct *wqueue;
	struct work_struct	chgin_work;
	struct delayed_work	isr_work;

	/* mutex */
	struct mutex irq_lock;
	struct mutex ops_lock;

	/* wakelock */
	struct wake_lock update_wake_lock;

	unsigned int	is_charging;
	unsigned int	charging_type;
	unsigned int	battery_state;
	unsigned int	battery_present;
	unsigned int	cable_type;
	unsigned int	charging_current_max;
	unsigned int	charging_current;
	unsigned int	input_current_limit;
	unsigned int	vbus_state;
	int		status;
	bool		aicl_on;
	int		siop_level;

	int		irq_bypass;
	int		irq_therm;
	int		irq_battery;
	int		irq_chg;
	int		irq_chgin;

	/* software regulation */
	bool		soft_reg_state;
	int		soft_reg_current;

	/* unsufficient power */
	bool		reg_loop_deted;

#if defined(CONFIG_WIRELESS_CHARGING)
	/* wireless charge, w(wpc), v(vbus) */
	int		wc_w_gpio;
	int		wc_w_irq;
	int		wc_w_state;
	int		wc_v_gpio;
	int		wc_v_irq;
	int		wc_v_state;
	bool		wc_pwr_det;
#endif

	sec_battery_platform_data_t	*pdata;
};

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

/* static void MAX77693_charger_initialize(struct max77693_charger_data *charger); */
static int max77693_get_vbus_state(struct max77693_charger_data *charger);

static void max77693_dump_reg(struct max77693_charger_data *charger)
{
	u8 reg_data;
	u32 reg_addr;
	pr_info("%s\n", __func__);

	for (reg_addr = 0xB0; reg_addr <= 0xC5; reg_addr++) {
		max77693_read_reg(charger->max77693->i2c, reg_addr, &reg_data);
		pr_info("max77693: c: 0x%02x(0x%02x)\n", reg_addr, reg_data);
	}
}

static int max77693_get_battery_present(struct max77693_charger_data *charger)
{
	u8 reg_data;

	if (max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_INT_OK, &reg_data) < 0) {
		/* Eventhough there is an error,
		   don't do power-off */
		return 1;
	}

	pr_debug("%s: CHG_INT_OK(0x%02x)\n", __func__, reg_data);

	reg_data = ((reg_data & MAX77693_BATP_OK) >> MAX77693_BATP_OK_SHIFT);

	return reg_data;
}

static void max77693_set_charger_state(struct max77693_charger_data *charger,
		int enable)
{
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, &reg_data);

	if (enable)
		reg_data |= MAX77693_MODE_CHGR;
	else
		reg_data &= ~MAX77693_MODE_CHGR;

	pr_info("%s: CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
	max77693_write_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, reg_data);
}

static void max77693_set_buck(struct max77693_charger_data *charger,
		int enable)
{
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_00, &reg_data);

	if (enable)
		reg_data |= MAX77693_MODE_BUCK;
	else
		reg_data &= ~MAX77693_MODE_BUCK;

	pr_debug("%s: CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_00, reg_data);
}

static void max77693_set_input_current(struct max77693_charger_data *charger,
		int cur)
{
	int set_current_reg, now_current_reg;
	int vbus_state, curr_step, delay;
	u8 set_reg, reg_data;

	mutex_lock(&charger->ops_lock);
	disable_irq(charger->irq_chgin);

	if (charger->cable_type == POWER_SUPPLY_TYPE_WPC)
		set_reg = MAX77693_CHG_REG_CHG_CNFG_10;
	else
		set_reg = MAX77693_CHG_REG_CHG_CNFG_09;


	if (cur <= 0) {
		max77693_write_reg(charger->max77693->i2c,
			set_reg, 0);
//		max77693_set_buck(charger, DISABLE);
		enable_irq(charger->irq_chgin);
		mutex_unlock(&charger->ops_lock);
		return;
	} else
		max77693_set_buck(charger, ENABLE);

	set_current_reg = cur / 20;
	max77693_read_reg(charger->max77693->i2c,
		set_reg, &reg_data);
	if (reg_data == set_current_reg) {
		/* check uvlo  */
		while(1) {
			vbus_state = max77693_get_vbus_state(charger);
			if (((vbus_state == 0x00) || (vbus_state == 0x01)) &&
				(charger->cable_type != POWER_SUPPLY_TYPE_WPC)) {
				/* UVLO */
				set_current_reg -= 5;
				if (set_current_reg <= 0)
					break;
				max77693_write_reg(charger->max77693->i2c,
						set_reg, set_current_reg);
				pr_info("%s: reg_data(0x%02x)\n", __func__, set_current_reg);
				/* under 1.3A, slow rate */
				if (set_current_reg < (1300 / 20) &&
					(charger->cable_type == POWER_SUPPLY_TYPE_MAINS))
					charger->aicl_on = true;
				msleep(50);
			} else
				break;
		}
		enable_irq(charger->irq_chgin);
		mutex_unlock(&charger->ops_lock);
		return;
	}

	if (reg_data == 0) {
		now_current_reg = SOFT_CHG_START_CURR / 20;
		max77693_write_reg(charger->max77693->i2c,
			set_reg, now_current_reg);
		msleep(SOFT_CHG_START_DUR);
	} else
		now_current_reg = reg_data;

	if (cur < 500) {
		curr_step = 1;
		delay = 50;
	} else {
		curr_step = SOFT_CHG_CURR_STEP / 20;
		delay = SOFT_CHG_STEP_DUR;
	}
	now_current_reg += (curr_step);

	while (now_current_reg < set_current_reg &&
			charger->cable_type != POWER_SUPPLY_TYPE_BATTERY)
	{
		now_current_reg = min(now_current_reg, set_current_reg);
		max77693_write_reg(charger->max77693->i2c,
			set_reg, now_current_reg);
		msleep(delay);

		vbus_state = max77693_get_vbus_state(charger);
		if (((vbus_state == 0x00) || (vbus_state == 0x01)) &&
			!(charger->cable_type == POWER_SUPPLY_TYPE_WPC)) {
			/* UVLO */
			now_current_reg -= (curr_step * 3);
			curr_step /= 2;
			max77693_write_reg(charger->max77693->i2c,
					set_reg, now_current_reg);
			pr_info("%s: reg_data(0x%02x)\n", __func__, now_current_reg);
			if (curr_step < 5) {
				/* under 1.3A, slow rate */
				if (now_current_reg < (1300 / 20) &&
					(charger->cable_type == POWER_SUPPLY_TYPE_MAINS))
					charger->aicl_on = true;
				enable_irq(charger->irq_chgin);
				mutex_unlock(&charger->ops_lock);
				return;
			}
			msleep(50);
		} else
			now_current_reg += (curr_step);
	}

	pr_info("%s: reg_data(0x%02x)\n", __func__, set_current_reg);
	max77693_write_reg(charger->max77693->i2c,
		set_reg, set_current_reg);
	enable_irq(charger->irq_chgin);
	mutex_unlock(&charger->ops_lock);
}

static int max77693_get_input_current(struct max77693_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	if (charger->cable_type == POWER_SUPPLY_TYPE_WPC) {
		max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_10, &reg_data);
		pr_info("%s: CHG_CNFG_10(0x%02x)\n", __func__, reg_data);
	} else {
		max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_09, &reg_data);
		pr_info("%s: CHG_CNFG_09(0x%02x)\n", __func__, reg_data);
	}

	get_current = reg_data * 20;

	pr_debug("%s: get input current: %dmA\n", __func__, get_current);
	return get_current;
}

static void max77693_set_topoff_current(struct max77693_charger_data *charger,
		int cur, int timeout)
{
	u8 reg_data;

	if (cur >= 350)
		reg_data = 0x07;
	else if (cur >= 300)
		reg_data = 0x06;
	else if (cur >= 250)
		reg_data = 0x05;
	else if (cur >= 200)
		reg_data = 0x04;
	else if (cur >= 175)
		reg_data = 0x03;
	else if (cur >= 150)
		reg_data = 0x02;
	else if (cur >= 125)
		reg_data = 0x01;
	else
		reg_data = 0x00;

	/* the unit of timeout is second*/
	timeout = timeout / 60;
	reg_data |= ((timeout / 10) << 3);
	pr_info("%s: reg_data(0x%02x), topoff(%d)\n", __func__, reg_data, cur);

	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_03, reg_data);
}

static void max77693_set_charge_current(struct max77693_charger_data *charger,
		int cur)
{
	u8 reg_data = 0;

	pr_info("%s: set current value : %d\n", __func__, cur);

	if (!cur) {
		/* No charger */
		max77693_write_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_02, 0x0);
	} else {
		reg_data &= ~MAX77693_CHG_CC;
		reg_data |= ((cur * 3 / 100) << 0);
//		reg_data |= MAX77693_OTG_ILIM;

		pr_info("%s: charge current %d mA, reg_data(0x%02x)\n",
				__func__, cur, reg_data);

		max77693_write_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_02, reg_data);
	}
}

static int max77693_get_charge_current(struct max77693_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_02, &reg_data);
	pr_debug("%s: CHG_CNFG_02(0x%02x)\n", __func__, reg_data);

	reg_data &= MAX77693_CHG_CC;
	get_current = reg_data * 333 / 10;

	pr_debug("%s: get charge current: %dmA\n", __func__, get_current);
	return get_current;
}

static void reduce_input_current(struct max77693_charger_data *charger, int cur)
{
	u8 set_reg;
	u8 set_value;

	if ((!charger->is_charging) || mutex_is_locked(&charger->ops_lock) ||
			(charger->cable_type == POWER_SUPPLY_TYPE_WPC))
		return;
	set_reg = MAX77693_CHG_REG_CHG_CNFG_09;
	if (!max77693_read_reg(charger->max77693->i2c,
				set_reg, &set_value)) {
		if (set_value == 0)
			return;
		set_value -= (cur / 20);
		set_value = (set_value < 10) ? 10 : set_value;
		max77693_write_reg(charger->max77693->i2c,
				set_reg, set_value);
		pr_info("%s: set current: reg:(0x%x), val:(0x%x)\n",
				__func__, set_reg, set_value);
		/* under 1.3A, slow rate */
		if (set_value < (1300 / 20) &&
			(charger->cable_type == POWER_SUPPLY_TYPE_MAINS))
			charger->aicl_on = true;
	}
}

static int max77693_get_vbus_state(struct max77693_charger_data *charger)
{
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_DTLS_00, &reg_data);
	if (charger->cable_type == POWER_SUPPLY_TYPE_WPC)
		reg_data = ((reg_data & MAX77693_WCIN_DTLS) >>
			MAX77693_WCIN_DTLS_SHIFT);
	else
		reg_data = ((reg_data & MAX77693_CHGIN_DTLS) >>
			MAX77693_CHGIN_DTLS_SHIFT);

	switch (reg_data) {
	case 0x00:
		pr_info("%s: VBUS is invalid. CHGIN < CHGIN_UVLO\n",
			__func__);
		break;
	case 0x01:
		pr_info("%s: VBUS is invalid. CHGIN < MBAT+CHGIN2SYS" \
			"and CHGIN > CHGIN_UVLO\n", __func__);
		break;
	case 0x02:
		pr_info("%s: VBUS is invalid. CHGIN > CHGIN_OVLO",
			__func__);
		break;
	case 0x03:
		pr_info("%s: VBUS is valid. CHGIN < CHGIN_OVLO", __func__);
		break;
	default:
		break;
	}

	return reg_data;
}

static int max77693_get_charger_state(struct max77693_charger_data *charger)
{
	int state;
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_DTLS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_CHG_DTLS) >> MAX77693_CHG_DTLS_SHIFT);
	pr_info("%s: CHG_DTLS : 0x%2x\n", __func__, reg_data);

	switch (reg_data) {
	case 0x0:
	case 0x1:
	case 0x2:
		state = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x3:
	case 0x4:
		state = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x5:
	case 0x6:
	case 0x7:
		state = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x8:
	case 0xA:
	case 0xB:
		state = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		state = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return state;
}

static int max77693_get_health_state(struct max77693_charger_data *charger)
{
	int state;
	int vbus_state;
	int chg_state;
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_DTLS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_BAT_DTLS) >> MAX77693_BAT_DTLS_SHIFT);

	switch (reg_data) {
	case 0x00:
		pr_info("%s: No battery and the charger is suspended\n",
			__func__);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	case 0x01:
		pr_info("%s: battery unspec failure\n",
			__func__);
		state = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	case 0x02:
		pr_info("%s: battery dead\n", __func__);
		state = POWER_SUPPLY_HEALTH_DEAD;
		break;
	case 0x03:
		pr_info("%s: battery good\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x04:
		pr_info("%s: battery is okay" \
			"but its voltage is low\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x05:
		pr_info("%s: battery ovp\n", __func__);
		state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;
	default:
		pr_info("%s: battery unknown : 0x%d\n", __func__, reg_data);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	if (state == POWER_SUPPLY_HEALTH_GOOD) {
		/* VBUS OVP state return battery OVP state */
		vbus_state = max77693_get_vbus_state(charger);
		if ((vbus_state == 0x00) || (vbus_state == 0x01))
			reduce_input_current(charger, 20);
		/* read CHG_DTLS and detecting battery terminal error */
		chg_state = max77693_get_charger_state(charger);
		/* OVP is higher priority */
		if (vbus_state == 0x02) { /* CHGIN_OVLO */
			pr_info("%s: vbus ovp\n", __func__);
			state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		} else if (reg_data == 0x04 &&
				chg_state == POWER_SUPPLY_STATUS_FULL) {
			pr_info("%s: battery terminal error\n", __func__);
			state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		}
	}

	return state;
}

static bool max77693_charger_unlock(struct max77693_charger_data *chg_data)
{
	struct i2c_client *i2c = chg_data->max77693->i2c;
	u8 reg_data;
	u8 chgprot;
	int retry_cnt = 0;
	bool need_init = false;
	pr_debug("%s\n", __func__);
	pr_info("%s: charger unlock enable \n", __func__);

	max77693_read_reg(i2c, MAX77693_CHG_REG_CHG_CNFG_06, &reg_data);
	chgprot = ((reg_data & 0x0C) >> 2);

	if (chgprot == 0x03) {
		pr_info("%s: unlocked state, return\n", __func__);
		need_init = false;
		goto unlock_finish;
	}

	do {
		max77693_write_reg(i2c, MAX77693_CHG_REG_CHG_CNFG_06,
					(0x03 << 2));

		max77693_read_reg(i2c, MAX77693_CHG_REG_CHG_CNFG_06, &reg_data);
		chgprot = ((reg_data & 0x0C) >> 2);

		if (chgprot != 0x03) {
			pr_err("%s: unlock err, chgprot(0x%x), retry(%d)\n",
					__func__, chgprot, retry_cnt);
			msleep(100);
		} else {
			pr_info("%s: unlock success, chgprot(0x%x)\n",
							__func__, chgprot);
			need_init = true;
			break;
		}
	} while ((chgprot != 0x03) && (++retry_cnt < 10));

unlock_finish:
	return need_init;
}

static void max77693_charger_initialize(struct max77693_charger_data *charger)
{
	u8 reg_data, float_voltage = 0x1D;
	pr_debug("%s\n", __func__);

	max77693_set_buck(charger, ENABLE);

	/* unlock charger setting protect */
	reg_data = (0x03 << 2);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_06, reg_data);

	/*
	 * fast charge timer disable
	 * restart threshold disable
	 * pre-qual charge enable(default)
	 */
	reg_data = (0x0 << 0) | (0x03 << 4);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_01, reg_data);

	/*
	 * confirm whether TA is connected or NOT
	 */
	if (max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_INT_OK, &reg_data) == 0) {
		/* Check the charging status*/
		if ((reg_data & (MAX77693_CHGIN_OK|MAX77693_WCIN_OK)) == 0) {
			max77693_read_reg(charger->max77693->i2c,
				MAX77693_CHG_REG_CHG_CNFG_00, &reg_data);
			reg_data &= ~(CHG_CNFG_00_CHG_MASK
					| CHG_CNFG_00_BUCK_MASK);
			max77693_write_reg(charger->max77693->i2c,
				MAX77693_CHG_REG_CHG_CNFG_00, reg_data);
		}
	}
	/*
	 * charge current 466mA(default)
	 * otg current limit 900mA
	 */
	max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_02, &reg_data);
	reg_data |= (1 << 7);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_02, reg_data);

	/*
	 * top off current 100mA
	 * top off timer 40min
	 */
	reg_data = (0x04 << 3);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_03, reg_data);

	/*
	 * cv voltage 4.2V or 4.35V
	 * MINVSYS 3.6V(default)
	 */
	max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_04, &reg_data);

	reg_data &= ~0x1F;
	if (charger->pdata->chg_float_voltage) {
		float_voltage = charger->pdata->chg_float_voltage <= 4325 ?
			(charger->pdata->chg_float_voltage - 3650) / 25 :
			(charger->pdata->chg_float_voltage - 3650) / 25 + 1;
	}

	reg_data |= float_voltage;

	/*
	pr_info("%s: battery cv voltage %s, (sysrev %d)\n", __func__,
		(((reg_data & max77693_CHG_PRM_MASK) == \
		(0x1D << max77693_CHG_PRM_SHIFT)) ? "4.35V" : "4.2V"),
		system_rev);
	*/
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_04, reg_data);

	/* VBYPSET Default set 3V */
	reg_data = 0x00;
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_11, reg_data);

	max77693_dump_reg(charger);
}

static void check_charger_unlock_state(struct max77693_charger_data *chg_data)
{
	bool need_reg_init = false;
	pr_debug("%s\n", __func__);

	pr_info("%s: unlock state enable\n", __func__);
	need_reg_init = max77693_charger_unlock(chg_data);
	if (need_reg_init) {
		pr_err("%s: charger locked state, reg init\n", __func__);
		max77693_charger_initialize(chg_data);
	}
}

static int sec_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max77693_charger_data *charger =
		container_of(psy, struct max77693_charger_data, psy_chg);
	u8 reg_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_BATTERY;
		if (max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_INT_OK, &reg_data) == 0) {
			if (reg_data & MAX77693_CHGIN_OK)
				val->intval = POWER_SUPPLY_TYPE_MAINS;
			else if (reg_data & MAX77693_WCIN_OK)
				val->intval = POWER_SUPPLY_TYPE_WPC;
		}
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77693_get_charger_state(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77693_get_health_state(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->charging_current_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = min(max77693_get_input_current(charger),
			max77693_get_charge_current(charger));
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging)
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else if (charger->aicl_on)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_SLOW;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77693_get_battery_present(charger);
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
	struct max77693_charger_data *charger =
		container_of(psy, struct max77693_charger_data, psy_chg);
	union power_supply_propval value;
	int set_charging_current, set_charging_current_max;
	const int usb_charging_current = charger->pdata->charging_current[
		POWER_SUPPLY_TYPE_USB].fast_charging_current;
	const int wpc_charging_current = charger->pdata->charging_current[
		POWER_SUPPLY_TYPE_WPC].input_current_limit;

	/* check and unlock */
	check_charger_unlock_state(charger);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_HEALTH, value);
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			charger->is_charging = false;
			charger->aicl_on = false;
			set_charging_current = 0;
			set_charging_current_max = 0;
		} else {
			charger->is_charging = true;
			/* decrease the charging current according to siop level */
			set_charging_current =
				charger->charging_current * charger->siop_level / 100;
			if (set_charging_current > 0 &&
					set_charging_current < usb_charging_current)
				set_charging_current = usb_charging_current;
			if (val->intval == POWER_SUPPLY_TYPE_WPC)
				set_charging_current_max = wpc_charging_current;
			else
				set_charging_current_max =
					charger->charging_current_max;
		}

		max77693_set_charger_state(charger, charger->is_charging);
		/* if battery full, only disable charging  */
		if ((charger->status == POWER_SUPPLY_STATUS_CHARGING) ||
				(charger->status == POWER_SUPPLY_STATUS_DISCHARGING) ||
				(value.intval == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)) {

			/* current setting */
			max77693_set_charge_current(charger,
				set_charging_current);
			max77693_set_input_current(charger,
				set_charging_current_max);
			max77693_set_topoff_current(charger,
				charger->pdata->charging_current[
				val->intval].full_check_current_1st,
				charger->pdata->charging_current[
				val->intval].full_check_current_2nd);
		}
		break;
	/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		charger->charging_current_max = val->intval;
		break;
	/*  val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->siop_level = val->intval;
		if (charger->is_charging) {
			/* decrease the charging current according to siop level */
			int current_now =
				charger->charging_current * val->intval / 100;
			if (current_now > 0 &&
					current_now < usb_charging_current)
				current_now = usb_charging_current;
			max77693_set_charge_current(charger, current_now);
		}
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		max77693_set_charge_current(charger,
				val->intval);
		max77693_set_input_current(charger,
				val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void sec_chg_isr_work(struct work_struct *work)
{
	struct max77693_charger_data *charger =
		container_of(work, struct max77693_charger_data, isr_work.work);

	union power_supply_propval val;

	if (charger->pdata->full_check_type ==
			SEC_BATTERY_FULLCHARGED_CHGINT) {

		val.intval = max77693_get_charger_state(charger);

		switch (val.intval) {
		case POWER_SUPPLY_STATUS_DISCHARGING:
			pr_err("%s: Interrupted but Discharging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_NOT_CHARGING:
			pr_err("%s: Interrupted but NOT Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_FULL:
			pr_info("%s: Interrupted by Full\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_STATUS, val);
			break;

		case POWER_SUPPLY_STATUS_CHARGING:
			pr_err("%s: Interrupted but Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Status\n", __func__);
			break;
		}
	}

	if (charger->pdata->ovp_uvlo_check_type ==
			SEC_BATTERY_OVP_UVLO_CHGINT) {

		val.intval = max77693_get_health_state(charger);

		switch (val.intval) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_COLD:
			pr_err("%s: Interrupted but Hot/Cold\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_DEAD:
			pr_err("%s: Interrupted but Dead\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, val);
			break;

		case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
			pr_err("%s: Interrupted but Unspec\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_GOOD:
			pr_err("%s: Interrupted but Good\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Health\n", __func__);
			break;
		}
	}
}

static int max77693_debugfs_show(struct seq_file *s, void *data)
{
	struct max77693_charger_data *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "MAX77693 CHARGER IC :\n");
	seq_printf(s, "==================\n");
	for (reg = 0xB0; reg <= 0xC5; reg++) {
		max77693_read_reg(charger->max77693->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");

	return 0;
}

static int max77693_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77693_debugfs_show, inode->i_private);
}

static const struct file_operations max77693_debugfs_fops = {
	.open           = max77693_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static irqreturn_t sec_chg_irq_thread(int irq, void *irq_data)
{
	struct max77693_charger_data *charger = irq_data;

	pr_info("%s: Charger interrupt occured\n", __func__);

	if ((charger->pdata->full_check_type ==
				SEC_BATTERY_FULLCHARGED_CHGINT) ||
			(charger->pdata->ovp_uvlo_check_type ==
			 SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

#if defined(CONFIG_WIRELESS_CHARGING)
static irqreturn_t wpc_charger_irq(int irq, void *data)
{
	struct max77693_charger_data *chg_data = data;
	int wc_w_state;
	union power_supply_propval value;
	pr_info("%s: irq(%d)\n", __func__, irq);

	/* check and unlock */
	check_charger_unlock_state(chg_data);

	wc_w_state = 0;

	wc_w_state = !gpio_get_value(chg_data->wc_w_gpio);
	if ((chg_data->wc_w_state == 0) && (wc_w_state == 1)) {
		value.intval = POWER_SUPPLY_TYPE_WPC<<ONLINE_TYPE_MAIN_SHIFT;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: wpc activated, set V_INT as PN\n",
				__func__);
	} else if ((chg_data->wc_w_state == 1) && (wc_w_state == 0)) {
		value.intval =
			POWER_SUPPLY_TYPE_BATTERY<<ONLINE_TYPE_MAIN_SHIFT;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: wpc deactivated, set V_INT as PD\n",
				__func__);
	}
	pr_info("%s: w(%d to %d)\n", __func__,
			chg_data->wc_w_state, wc_w_state);

	chg_data->wc_w_state = wc_w_state;

	return IRQ_HANDLED;
}
#endif

static irqreturn_t max77693_bypass_irq(int irq, void *data)
{
	struct max77693_charger_data *chg_data = data;
	u8 dtls_02;
	u8 byp_dtls;
	u8 chg_cnfg_00;

	pr_info("%s: irq(%d)\n", __func__, irq);

	/* check and unlock */
	check_charger_unlock_state(chg_data);

	max77693_read_reg(chg_data->max77693->i2c,
				MAX77693_CHG_REG_CHG_DTLS_02,
				&dtls_02);

	byp_dtls = ((dtls_02 & MAX77693_BYP_DTLS) >>
				MAX77693_BYP_DTLS_SHIFT);
	pr_info("%s: BYP_DTLS(0x%02x)\n", __func__, byp_dtls);

	if (byp_dtls & 0x1) {
		pr_info("%s: bypass overcurrent limit\n", __func__);
#ifdef CONFIG_USB_HOST_NOTIFY
		/* max77693_muic_host_notify_cb(0); */
#endif
		/* disable the register values just related to OTG and
		   keep the values about the charging */
		max77693_read_reg(chg_data->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, &chg_cnfg_00);
		chg_cnfg_00 &= ~(CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		max77693_write_reg(chg_data->max77693->i2c,
					MAX77693_CHG_REG_CHG_CNFG_00,
					chg_cnfg_00);
	}
	if (byp_dtls & 0x8)
		reduce_input_current(chg_data, 100);

	return IRQ_HANDLED;
}

static void max77693_chgin_isr_work(struct work_struct *work)
{
	struct max77693_charger_data *charger = container_of(work,
				struct max77693_charger_data, chgin_work);
	u8 chgin_dtls;
	u8 prev_chgin_dtls = 0xff;
	int battery_health;
	union power_supply_propval value;
	int stable_count = 0;

	disable_irq(charger->irq_chgin);

	while (1) {
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_HEALTH, value);
		battery_health = value.intval;

		max77693_read_reg(charger->max77693->i2c,
				MAX77693_CHG_REG_CHG_DTLS_00,
				&chgin_dtls);
		chgin_dtls = ((chgin_dtls & MAX77693_CHGIN_DTLS) >>
				MAX77693_CHGIN_DTLS_SHIFT);
		if (prev_chgin_dtls == chgin_dtls)
			stable_count++;
		else
			stable_count = 0;
		if (stable_count > 10) {
			pr_info("%s: irq(%d), chgin(0x%x), prev 0x%x\n",
					__func__, charger->irq_chgin,
					chgin_dtls, prev_chgin_dtls);
			break;
		}

		if (charger->is_charging) {
			if ((chgin_dtls == 0x02) && \
				(battery_health == POWER_SUPPLY_HEALTH_GOOD)) {
				pr_info("%s: charger is over voltage\n",
						__func__);
				value.intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				psy_do_property("battery", set,
					POWER_SUPPLY_PROP_HEALTH, value);
			} else if (battery_health == \
					POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
				pr_info("%s: charger is good\n", __func__);
				value.intval = POWER_SUPPLY_HEALTH_GOOD;
				psy_do_property("battery", set,
					POWER_SUPPLY_PROP_HEALTH, value);
			}
			if ((chgin_dtls == 0x0) || (chgin_dtls == 0x01))
				reduce_input_current(charger, 20);
		}
		prev_chgin_dtls = chgin_dtls;
		msleep(100);
	}
	enable_irq(charger->irq_chgin);
}

static irqreturn_t max77693_chgin_irq(int irq, void *data)
{
	struct max77693_charger_data *charger = data;
	queue_work(charger->wqueue, &charger->chgin_work);

	return IRQ_HANDLED;
}

static __devinit int max77693_charger_probe(struct platform_device *pdev)
{
	struct max77693_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77693_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct max77693_charger_data *charger;
	int ret = 0;

	pr_info("%s: max77693 Charger driver probe\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->max77693 = iodev;
	charger->pdata = pdata->charger_data;
	charger->aicl_on = false;
	charger->siop_level = 100;

	platform_set_drvdata(pdev, charger);

	charger->psy_chg.name           = "sec-charger";
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = sec_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);

	mutex_init(&charger->ops_lock);

	if (charger->pdata->chg_gpio_init) {
		if (!charger->pdata->chg_gpio_init()) {
			pr_err("%s: Failed to Initialize GPIO\n", __func__);
			goto err_free;
		}
	}

	max77693_charger_initialize(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_free;
	}

	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK_DEFERRABLE(
				&charger->isr_work, sec_chg_isr_work);
		ret = request_threaded_irq(charger->pdata->chg_irq,
				NULL, sec_chg_irq_thread,
				charger->pdata->chg_irq_attr,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
			goto err_irq;
		}
	}

#if defined(CONFIG_WIRELESS_CHARGING)
	charger->wc_w_gpio = pdata->wc_irq_gpio;
	if (charger->wc_w_gpio) {
		charger->wc_w_irq = gpio_to_irq(charger->wc_w_gpio);
		ret = gpio_request(charger->wc_w_gpio, "wpc_charger-irq");
		if (ret < 0) {
			pr_err("%s: failed requesting gpio %d\n", __func__,
				charger->wc_w_gpio);
			goto err_wc_irq;
		}
		ret = request_threaded_irq(charger->wc_w_irq,
				NULL, wpc_charger_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT,
				"wpc-int", charger);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
			goto err_wc_irq;
		}
		enable_irq_wake(charger->wc_w_irq);
		charger->wc_w_state = !gpio_get_value(charger->wc_w_gpio);
	}
#endif

	charger->wqueue =
	    create_singlethread_workqueue(dev_name(&pdev->dev));
	INIT_WORK(&charger->chgin_work, max77693_chgin_isr_work);
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_workqueue;
	}

	charger->irq_chgin = pdata->irq_base + MAX77693_CHG_IRQ_CHGIN_I;
	ret = request_threaded_irq(charger->irq_chgin, NULL,
			max77693_chgin_irq, 0, "chgin-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request chgin IRQ: %d: %d\n",
				__func__, charger->irq_chgin, ret);
		goto err_chgin_irq;
	}

	charger->irq_bypass = pdata->irq_base + MAX77693_CHG_IRQ_BYP_I;
	ret = request_threaded_irq(charger->irq_bypass, NULL,
			max77693_bypass_irq, 0, "bypass-irq", charger);
	if (ret < 0)
		pr_err("%s: fail to request bypass IRQ: %d: %d\n",
				__func__, charger->irq_bypass, ret);

	max77693_dentry = debugfs_create_file("max77693-regs",
			S_IRUSR, NULL, charger, &max77693_debugfs_fops);
	return 0;

err_chgin_irq:
	destroy_workqueue(charger->wqueue);
err_workqueue:
#if defined(CONFIG_WIRELESS_CHARGING)
	if (charger->wc_w_irq)
		free_irq(charger->wc_w_irq, NULL);
err_wc_irq:
#endif
	if (charger->pdata->chg_irq)
		free_irq(charger->pdata->chg_irq, NULL);
err_irq:
	power_supply_unregister(&charger->psy_chg);
err_free:
	kfree(charger);

	return ret;

}

static int __devexit max77693_charger_remove(struct platform_device *pdev)
{
	struct max77693_charger_data *charger =
				platform_get_drvdata(pdev);

	if (!IS_ERR_OR_NULL(max77693_dentry))
		debugfs_remove(max77693_dentry);

	destroy_workqueue(charger->wqueue);
#if defined(CONFIG_WIRELESS_CHARGING)
	if (charger->wc_w_irq)
		free_irq(charger->wc_w_irq, NULL);
#endif
	if (charger->pdata->chg_irq)
		free_irq(charger->pdata->chg_irq, NULL);

	power_supply_unregister(&charger->psy_chg);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int max77693_charger_suspend(struct device *dev)
{
	return 0;
}

static int max77693_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define max77693_charger_suspend NULL
#define max77693_charger_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(max77693_charger_pm_ops, max77693_charger_suspend,
		max77693_charger_resume);

void max77693_charger_shutdown(struct device *dev)
{
	struct max77693_charger_data *charger =
				dev_get_drvdata(dev);
	u8 reg_data;

	if (!charger->max77693->i2c) {
		pr_err("%s: no max77693 i2c client\n", __func__);
		return;
	}
	reg_data = 0x04;
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_00, reg_data);
	reg_data = 0x19;
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_09, reg_data);
	reg_data = 0x19;
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_10, reg_data);
	pr_info("func:%s \n", __func__);
}

static struct platform_driver max77693_charger_driver = {
	.driver = {
		.name = "max77693-charger",
		.owner = THIS_MODULE,
		.pm = &max77693_charger_pm_ops,
		.shutdown = max77693_charger_shutdown,
	},
	.probe = max77693_charger_probe,
	.remove = __devexit_p(max77693_charger_remove),
};

static int __init max77693_charger_init(void)
{
	pr_info("func:%s\n", __func__);
	return platform_driver_register(&max77693_charger_driver);
}
module_init(max77693_charger_init);

static void __exit max77693_charger_exit(void)
{
	platform_driver_register(&max77693_charger_driver);
}

module_exit(max77693_charger_exit);

MODULE_DESCRIPTION("max77693 charger driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

