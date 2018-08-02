/*
 *  bq24260_charger.c
 *  Samsung bq24260 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG

#include <linux/battery/sec_charger.h>

static int bq24260_i2c_write(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	return ret;
}

static int bq24260_i2c_read(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	return ret;
}

static void bq24260_i2c_write_array(struct i2c_client *client,
				u8 *buf, int size)
{
	int i;
	for (i = 0; i < size; i += 3)
		bq24260_i2c_write(client, (u8) (*(buf + i)), (buf + i) + 1);
}

static void bq24260_set_command(struct i2c_client *client,
				int reg, int datum)
{
	int val;
	u8 data = 0;
	val = bq24260_i2c_read(client, reg, &data);
	if (val >= 0) {
		dev_dbg(&client->dev, "%s : reg(0x%02x): 0x%02x(0x%02x)",
			__func__, reg, data, datum);
		if (data != datum) {
			data = datum;
			if (bq24260_i2c_write(client, reg, &data) < 0)
				dev_err(&client->dev,
					"%s : error!\n", __func__);
			val = bq24260_i2c_read(client, reg, &data);
			if (val >= 0)
				dev_dbg(&client->dev, " => 0x%02x\n", data);
		}
	}
}

static void bq24260_test_read(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;
	for (addr = 0; addr <= 0x06; addr++) {
		bq24260_i2c_read(client, addr, &data);
		dev_dbg(&client->dev,
			"bq24260 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}

static void bq24260_read_regs(struct i2c_client *client, char *str)
{
	u8 data = 0;
	u32 addr = 0;

	for (addr = 0; addr <= 0x06; addr++) {
		bq24260_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}
}


static int bq24260_get_charging_status(struct i2c_client *client)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 data = 0;

	bq24260_i2c_read(client, BQ24260_STATUS, &data);
	dev_info(&client->dev,
		"%s : charger status(0x%02x)\n", __func__, data);

	data = (data & 0x30);

	switch (data) {
	case 0x00:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 0x10:
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x20:
		status = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x30:
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	}

	return (int)status;
}

static int bq24260_get_charging_health(struct i2c_client *client)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 data = 0;

	bq24260_i2c_read(client, BQ24260_STATUS, &data);
	dev_info(&client->dev,
		"%s : charger status(0x%02x)\n", __func__, data);

	if ((data & 0x30) == 0x30) {	/* check for fault */
		data = (data & 0x07);

		switch (data) {
		case 0x01:
			health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			break;
		case 0x02:
			health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
			break;
		}
	}

	return (int)health;
}

static u8 bq24260_get_float_voltage_data(
			int float_voltage)
{
	u8 data;

	if (float_voltage < 3500)
		float_voltage = 3500;

	data = (float_voltage - 3500) / 20;

	return data << 2;
}

static u8 bq24260_get_input_current_limit_data(
			int input_current)
{
	u8 data = 0x00;

	if (input_current <= 100)
		data = 0x00;
	else if (input_current <= 150)
		data = 0x01;
	else if (input_current <= 500)
		data = 0x02;
	else if (input_current <= 900)
		data = 0x03;
	else if (input_current <= 1000)
		data = 0x04;
	else if (input_current <= 2000)/* will be set as 1950mA */
		data = 0x06;
	else	/* No limit */
		data = 0x07;

	return data << 4;
}

static u8 bq24260_get_termination_current_limit_data(
			int termination_current)
{
	u8 data;

	/* default offset 50mA, max 300mA */
	data = (termination_current - 50) / 50;

	return data;
}

static u8 bq24260_get_fast_charging_current_data(
			int fast_charging_current)
{
	u8 data;

	/* default offset 500mA */
	if (fast_charging_current < 500)
		fast_charging_current = 500;

	data = (fast_charging_current - 500) / 100;

	return data << 3;
}

static void bq24260_charger_function_conrol(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	union power_supply_propval val;
	int full_check_type;
	u8 data;
	if (charger->charging_current < 0) {
		dev_dbg(&client->dev,
			"%s : OTG is activated. Ignore command!\n", __func__);
		return;
	}

	if (charger->cable_type ==
		POWER_SUPPLY_TYPE_BATTERY) {
		data = 0x00;
		bq24260_i2c_read(client, BQ24260_CONTROL, &data);
		data |= 0x2;
		data &= 0x7f; /* Prevent register reset */
		bq24260_set_command(client,
			BQ24260_CONTROL, data);
	} else {
		data = 0x00;
		bq24260_i2c_read(client, BQ24260_CONTROL, &data);
		/* Enable charging */
		data &= 0x7d; /*default enabled*/
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, val);
		if (val.intval == SEC_BATTERY_CHARGING_1ST)
			full_check_type = charger->pdata->full_check_type;
		else
			full_check_type = charger->pdata->full_check_type_2nd;
		/* Termination setting */
		switch (full_check_type) {
		case SEC_BATTERY_FULLCHARGED_CHGGPIO:
		case SEC_BATTERY_FULLCHARGED_CHGINT:
		case SEC_BATTERY_FULLCHARGED_CHGPSY:
			/* Enable Current Termination */
			data |= 0x04;
			break;
		default:
			data &= 0x7b;
			break;
		}
		/* Input current limit */
		dev_dbg(&client->dev, "%s : input current (%dmA)\n",
			__func__, charger->pdata->charging_current
			[charger->cable_type].input_current_limit);
		data &= 0x0F;
		data |= bq24260_get_input_current_limit_data(
			charger->pdata->charging_current
			[charger->cable_type].input_current_limit);
		bq24260_set_command(client,
			BQ24260_CONTROL, data);

		data = 0x00;
		/* Float voltage */
		dev_dbg(&client->dev, "%s : float voltage (%dmV)\n",
			__func__, charger->pdata->chg_float_voltage);
		data |= bq24260_get_float_voltage_data(
			charger->pdata->chg_float_voltage);
		bq24260_set_command(client,
			BQ24260_VOLTAGE, data);

		data = 0x00;
		/* Fast charge and Termination current */
		dev_dbg(&client->dev, "%s : fast charging current (%dmA)\n",
				__func__, charger->charging_current);
		data |= bq24260_get_fast_charging_current_data(
			charger->charging_current);
		dev_dbg(&client->dev, "%s : termination current (%dmA)\n",
			__func__, charger->pdata->charging_current[
			charger->cable_type].full_check_current_1st >= 300 ?
			300 : charger->pdata->charging_current[
			charger->cable_type].full_check_current_1st);
		data |= bq24260_get_termination_current_limit_data(
			charger->pdata->charging_current[
			charger->cable_type].full_check_current_1st);
		bq24260_set_command(client,
			BQ24260_CURRENT, data);

		/* Special Charger Voltage
		 * Normal charge current
		 */
		bq24260_i2c_read(client, BQ24260_SPECIAL, &data);
		data &= 0xdf;
		bq24260_set_command(client,
			BQ24260_SPECIAL, data);
	}
}

static void bq24260_charger_otg_conrol(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
	if (charger->cable_type ==
		POWER_SUPPLY_TYPE_BATTERY) {
		dev_info(&client->dev, "%s : turn off OTG\n", __func__);
		/* turn off OTG */
		bq24260_i2c_read(client, BQ24260_STATUS, &data);
		data &= 0xbf;
		bq24260_set_command(client,
			BQ24260_STATUS, data);
	} else {
		dev_info(&client->dev, "%s : turn on OTG\n", __func__);
		/* turn on OTG */
		bq24260_i2c_read(client, BQ24260_STATUS, &data);
		data |= 0x40;
		bq24260_set_command(client,
			BQ24260_STATUS, data);
	}
}

static int bq24260_get_charge_type(struct i2c_client *client)
{
	int ret;
	u8 data;

	bq24260_i2c_read(client, BQ24260_STATUS, &data);
	data = (data & 0x30)>>4;

	switch (data) {
	case 0x01:
		ret = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		ret = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	return ret;
}

bool sec_hal_chg_init(struct i2c_client *client)
{
	bq24260_test_read(client);
	return true;
}

bool sec_hal_chg_suspend(struct i2c_client *client)
{
	return true;
}

bool sec_hal_chg_resume(struct i2c_client *client)
{
	return true;
}

bool sec_hal_chg_get_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq24260_get_charging_status(client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq24260_get_charge_type(client);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq24260_get_charging_health(client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			/* Rsns 0.068 Ohm */
			bq24260_i2c_read(client, BQ24260_CURRENT, &data);
			val->intval = (data >> 3) * 100 + 500;
		} else
			val->intval = 0;
		dev_dbg(&client->dev,
			"%s : set-current(%dmA), current now(%dmA)\n",
			__func__, charger->charging_current, val->intval);
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_chg_set_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		if (charger->pdata->chg_gpio_en) {
			if (gpio_request(charger->pdata->chg_gpio_en,
				"CHG_EN") < 0) {
				dev_err(&client->dev,
					"failed to request vbus_in gpio\n");
				break;
			}
			if (charger->cable_type ==
				POWER_SUPPLY_TYPE_BATTERY)
				gpio_set_value_cansleep(
					charger->pdata->chg_gpio_en,
					charger->pdata->chg_polarity_en ?
					0 : 1);
			else
				gpio_set_value_cansleep(
					charger->pdata->chg_gpio_en,
					charger->pdata->chg_polarity_en ?
					1 : 0);
			gpio_free(charger->pdata->chg_gpio_en);
		}
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current < 0)
			bq24260_charger_otg_conrol(client);
		else if (charger->charging_current > 0)
			bq24260_charger_function_conrol(client);
		else {
			bq24260_charger_function_conrol(client);
			bq24260_charger_otg_conrol(client);
		}
		bq24260_test_read(client);
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_chg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_charger_info *chg =
		container_of(psy, struct sec_charger_info, psy_chg);
	int i = 0;
	char *str = NULL;

	switch (offset) {
	case CHG_REG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
			chg->reg_addr);
		break;
	case CHG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
			chg->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		bq24260_read_regs(chg->client, str);
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

ssize_t sec_hal_chg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_charger_info *chg =
		container_of(psy, struct sec_charger_info, psy_chg);
	int ret = 0;
	int x = 0;
	u8 data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			chg->reg_addr = x;
			bq24260_i2c_read(chg->client,
				chg->reg_addr, &data);
			chg->reg_data = data;
			dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, chg->reg_data);
			ret = count;
		}
		break;
	case CHG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data = (u8)x;
			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, data);
			bq24260_i2c_write(chg->client,
				chg->reg_addr, &data);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
