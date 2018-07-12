/*
 *  max17058_fuelgauge.c
 *  Samsung MAX17058 Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/power/sec_fuelgauge.h>

static int max17058_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);

	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

static int max17058_read_reg(struct i2c_client *client, int reg, u8 *buf)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);

	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

static void max17058_init_regs(struct i2c_client *client)
{
	u8 data[2];

	if (max17058_read_reg(client, MAX17058_REG_STATUS, data) < 0)
		return ;
	data[0] |= (1 << 0);
	max17058_write_reg(client, MAX17058_REG_STATUS, data);

	if (max17058_read_reg(client, MAX17058_REG_STATUS, data) < 0)
		return ;
	data[0] &= ~(1 << 0);
	max17058_write_reg(client, MAX17058_REG_STATUS, data);

	return ;
}

static void max17058_get_version(struct i2c_client *client)
{
	u8 data[2];

	if (max17058_read_reg(client, MAX17058_REG_VERSION, data) < 0)
		return;

	dev_dbg(&client->dev, "MAX17058 Fuel-Gauge Ver %d%d\n",
		data[0], data[1]);
}

/* soc should be 0.01% unit */
static int max17058_get_soc(struct i2c_client *client)
{
	u8 data[2];
	int soc;

	if (max17058_read_reg(client, MAX17058_REG_SOC, data) < 0)
		return -EINVAL;

	soc = ((data[0] * 100) + (data[1] * 100 / 256));

	dev_dbg(&client->dev, "%s: raw capacity (%d)\n", __func__, soc);

	return min(soc, 10000);
}

static int max17058_get_vcell(struct i2c_client *client)
{
	u8 data[2];
	u32 vcell = 0;

	if (max17058_read_reg(client, MAX17058_REG_VCELL, data) < 0)
		return -EINVAL;

	vcell = ((data[0] << 8) | data[1]) * 78 / 1000;

	dev_dbg(&client->dev, "%s: vcell (%d)\n", __func__, vcell);

	return vcell;
}

bool sec_hal_fg_init(struct i2c_client *client)
{
	/* initialize fuel gauge registers */
	max17058_init_regs(client);

	max17058_get_version(client);

	return true;
}

bool sec_hal_fg_suspend(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_resume(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_fuelalert_init(struct i2c_client *client, int soc)
{
	return true;
}

bool sec_hal_fg_is_fuelalerted(struct i2c_client *client)
{
	u8 data[2];

	max17058_read_reg(client, MAX17058_REG_CONFIG, data);
	if (data[1] & (1 << 5))
		return true;
	else
		return false;
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct sec_fuelgauge_info *fuelgauge = irq_data;
	u8 data[2];

	if (is_fuel_alerted) {
		dev_info(&fuelgauge->client->dev,
			"%s: Fuel-alert Alerted!! (%02x%02x)\n",
			__func__, data[1], data[0]);
	} else {
		dev_info(&fuelgauge->client->dev,
			"%s: Fuel-alert Released!! (%02x%02x)\n",
			__func__, data[1], data[0]);
	}

	max17058_read_reg(fuelgauge->client, MAX17058_REG_VCELL, data);
	dev_dbg(&fuelgauge->client->dev,
		"%s: MAX17058_REG_VCELL(%02x%02x)\n",
		 __func__, data[1], data[0]);

	max17058_read_reg(fuelgauge->client, MAX17058_REG_SOC, data);
	dev_dbg(&fuelgauge->client->dev,
		"%s: MAX17058_REG_SOC(%02x%02x)\n",
		 __func__, data[1], data[0]);

	max17058_read_reg(fuelgauge->client, MAX17058_REG_CONFIG, data);
	dev_dbg(&fuelgauge->client->dev,
		"%s: MAX17058_REG_CONFIG(%02x%02x)\n",
		 __func__, data[1], data[0]);

	dev_dbg(&fuelgauge->client->dev,
		"%s: FUEL GAUGE IRQ (%d)\n",
		 __func__,
		 gpio_get_value(fuelgauge->pdata->fg_irq));

	return true;
}

bool sec_hal_fg_full_charged(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_reset(struct i2c_client *client)
{
	u8 data[2];

	if (max17058_read_reg(client, MAX17058_REG_STATUS, data) < 0)
		return false;
	data[0] |= (1 << 0);
	max17058_write_reg(client, MAX17058_REG_STATUS, data);

	if (max17058_read_reg(client, MAX17058_REG_STATUS, data) < 0)
		return false;
	data[0] &= ~(1 << 0);
	max17058_write_reg(client, MAX17058_REG_STATUS, data);

	return true;

}

bool sec_hal_fg_get_property(struct i2c_client *client,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	switch (psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17058_get_vcell(client);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTERY_VOLTAGE_AVERAGE:
			val->intval = 0;
			break;
		case SEC_BATTERY_VOLTAGE_OCV:
			val->intval = 0;
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		val->intval = 0;
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = 0;
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW)
			val->intval = max17058_get_soc(client);
		else
			val->intval = max17058_get_soc(client) / 10;
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = 0;
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_fg_set_property(struct i2c_client *client,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	switch (psp) {
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_fg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_fuelgauge_info *fg =
		container_of(psy, struct sec_fuelgauge_info, psy_fg);
	int i = 0;

	switch (offset) {
/*	case FG_REG: */
/*		break; */
	case FG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%02x%02x\n",
			fg->reg_data[1], fg->reg_data[0]);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sec_hal_fg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_fuelgauge_info *fg =
		container_of(psy, struct sec_fuelgauge_info, psy_fg);
	int ret = 0;
	int x = 0;
	u8 data[2];

	switch (offset) {
	case FG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			fg->reg_addr = x;
			max17058_read_reg(fg->client,
				fg->reg_addr, fg->reg_data);
			dev_dbg(&fg->client->dev,
				"%s: (read) addr = 0x%x, data = 0x%02x%02x\n",
				 __func__, fg->reg_addr,
				 fg->reg_data[1], fg->reg_data[0]);
			ret = count;
		}
		break;
	case FG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data[0] = (x & 0xFF00) >> 8;
			data[1] = (x & 0x00FF);
			dev_dbg(&fg->client->dev,
				"%s: (write) addr = 0x%x, data = 0x%02x%02x\n",
				__func__, fg->reg_addr, data[1], data[0]);
			max17058_write_reg(fg->client,
				fg->reg_addr, data);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
