/*
 *  sec_multi_charger.c
 *  Samsung Mobile Charger Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/battery/sec_multi_charger.h>

static enum power_supply_property sec_multi_charger_props[] = {
};

static bool sec_multi_chg_check_sub_charging(struct sec_multi_charger_info *charger)
{
	union power_supply_propval value;

	if (!charger->pdata->sub_charger_condition) {
		pr_info("%s: sub charger off(default)\n", __func__);
		return false;
	}

	if (charger->pdata->sub_charger_condition &
		SEC_SUB_CHARGER_CONDITION_CURRENT_MAX) {
		if (charger->total_current.input_current_limit < charger->pdata->sub_charger_condition_current_max) {
			if (charger->sub_is_charging)
				pr_info("%s: sub charger off CURRENT_MAX(%d)\n", __func__,
					charger->total_current.input_current_limit);
			return false;
		}
	}

	if (charger->pdata->sub_charger_condition &
		SEC_SUB_CHARGER_CONDITION_ONLINE) {
		int i = 0;

		for (i = 0; i < charger->pdata->sub_charger_condition_online_size; i++) {
			if (charger->cable_type == charger->pdata->sub_charger_condition_online[i])
				break;
		}

		if (i >= charger->pdata->sub_charger_condition_online_size) {
			if (charger->sub_is_charging)
				pr_info("%s: sub charger off ONLINE(%d)\n", __func__, i);
			return false;
		}
	}

	if (charger->pdata->sub_charger_condition &
		SEC_SUB_CHARGER_CONDITION_CV) {
		psy_do_property(charger->pdata->main_charger_name, get,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, value);

		if (value.intval) {
			if (charger->sub_is_charging)
				pr_info("%s: sub charger off CV(%d)\n", __func__, value.intval);
			return false;
		}
	}

	if (charger->pdata->sub_charger_condition &
		SEC_SUB_CHARGER_CONDITION_CURRENT_NOW) {
		psy_do_property(charger->pdata->battery_name, get,
			POWER_SUPPLY_PROP_CURRENT_NOW, value);

		if (value.intval < charger->pdata->sub_charger_condition_current_now) {
			if (charger->sub_is_charging)
				pr_info("%s: sub charger off CURRENT_NOW(%d)\n", __func__, value.intval);
			return false;
		} else if (value.intval < charger->pdata->sub_charger_enable_current_now) {
			if (!charger->sub_is_charging) {
				return false;
			}			
		}
	}

	return true;
}

static int sec_multi_chg_check_input_current(struct sec_multi_charger_info *charger)
{
	union power_supply_propval value;
	int main_input_current = charger->main_current.input_current_limit,
		sub_input_current = charger->sub_current.input_current_limit;

	if (!charger->pdata->is_serial && charger->sub_is_charging) {
		main_input_current = charger->total_current.input_current_limit / 2;
		sub_input_current = charger->total_current.input_current_limit / 2;
	} else {
		main_input_current = charger->total_current.input_current_limit;
		sub_input_current = 0;
	}

	/* set input current */
	if (main_input_current != charger->main_current.input_current_limit) {
		charger->main_current.input_current_limit = main_input_current;
		value.intval = main_input_current;
		psy_do_property(charger->pdata->main_charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);

		pr_info("%s: set input current - main(%dmA)\n", __func__, value.intval);
	}
	if (sub_input_current != charger->sub_current.input_current_limit) {
		charger->sub_current.input_current_limit = sub_input_current;
		value.intval = sub_input_current;
		psy_do_property(charger->pdata->sub_charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);

		pr_info("%s: set input current - sub(%dmA)\n", __func__, value.intval);
	}

	return 0;
}

static int sec_multi_chg_check_charging_current(struct sec_multi_charger_info *charger)
{
	union power_supply_propval value;
	unsigned int main_charging_current = charger->main_current.fast_charging_current,
		sub_charging_current = charger->sub_current.fast_charging_current;

	if (charger->sub_is_charging) {
		main_charging_current = charger->total_current.fast_charging_current / 2;
		sub_charging_current = charger->total_current.fast_charging_current / 2;

		main_charging_current = (main_charging_current * charger->pdata->main_charger_current_level) / 100;
		sub_charging_current = (sub_charging_current * charger->pdata->sub_charger_current_level) / 100;
		if (sub_charging_current > charger->pdata->sub_charger_current_max)
		{
			sub_charging_current = charger->pdata->sub_charger_current_max;
			main_charging_current = charger->total_current.fast_charging_current - sub_charging_current;
		}
	} else {
		main_charging_current = charger->total_current.fast_charging_current;
		sub_charging_current = 0;
	}

	/* set charging current */
	if (main_charging_current != charger->main_current.fast_charging_current) {
		charger->main_current.fast_charging_current = main_charging_current;
		value.intval = main_charging_current;
		psy_do_property(charger->pdata->main_charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_NOW, value);

		pr_info("%s: set charging current - main(%dmA)\n", __func__, value.intval);
	}
	if (sub_charging_current != charger->sub_current.fast_charging_current) {
		charger->sub_current.fast_charging_current = sub_charging_current;
		value.intval = sub_charging_current;
		psy_do_property(charger->pdata->sub_charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_NOW, value);

		pr_info("%s: set charging current - sub(%dmA)\n", __func__, value.intval);
	}

	return 0;
}

static int sec_multi_chg_check_temp_ctrl(struct charger_temp_control *chg_temp_ctrl)
{
	int level = chg_temp_ctrl->level;

	if (chg_temp_ctrl->temp > chg_temp_ctrl->threshold) {
		int diff = chg_temp_ctrl->temp - chg_temp_ctrl->threshold;
		diff = ((diff / chg_temp_ctrl->step) + 1) * chg_temp_ctrl->drop_level;

		level = 100 - diff;
		if (level == chg_temp_ctrl->level)
			level = -1;
		else if (level < chg_temp_ctrl->drop_level)
			level = chg_temp_ctrl->drop_level;
	} else if (level == 100) {
		level = -1;
	} else {
		level = 100;
	}

	pr_info("%s: temp(%d), threshold(%d), level(%d)\n",
		__func__, chg_temp_ctrl->temp, chg_temp_ctrl->threshold, level);
	return level;
}

static int sec_multi_chg_check_temperature(struct sec_multi_charger_info *charger, int temp)
{
	union power_supply_propval value;
	int chg_temp_level = 0;

	charger->pdata->main_charger_temp.temp = (temp & 0x00FF);
	charger->pdata->sub_charger_temp.temp = (temp & 0xFF00) >> 16;

	if ((charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) ||
		(charger->status != POWER_SUPPLY_STATUS_CHARGING && charger->status != POWER_SUPPLY_STATUS_FULL))
		pr_info("%s: skip multi charging routine\n", __func__);
		return 0;

	if (charger->siop_level >= 100) {
		chg_temp_level = sec_multi_chg_check_temp_ctrl(&charger->pdata->main_charger_temp);
		if (chg_temp_level >= 0) {
			charger->pdata->main_charger_temp.level = chg_temp_level;
			value.intval = charger->main_current.fast_charging_current * chg_temp_level / 100;
			value.intval = value.intval * charger->pdata->main_charger_current_level / 100;
			psy_do_property(charger->pdata->main_charger_name, set,
				POWER_SUPPLY_PROP_CURRENT_NOW, value);

			pr_info("%s: set charging current - main(%dmA)\n", __func__, value.intval);
		}

		if (charger->sub_is_charging) {
			chg_temp_level = sec_multi_chg_check_temp_ctrl(&charger->pdata->sub_charger_temp);
			if (chg_temp_level >= 0) {
				charger->pdata->sub_charger_temp.level = chg_temp_level;
				value.intval = charger->sub_current.fast_charging_current * chg_temp_level / 100;
				value.intval = value.intval * charger->pdata->sub_charger_current_level / 100;
				psy_do_property(charger->pdata->sub_charger_name, set,
					POWER_SUPPLY_PROP_CURRENT_NOW, value);

				pr_info("%s: set charging current - sub(%dmA)\n", __func__, value.intval);
			}
		}
	} else {
		if (charger->pdata->main_charger_temp.level != 100 ||
			charger->pdata->sub_charger_temp.level != 100) {
			/* re-set charging current */
			charger->main_current.fast_charging_current = 0;
			charger->sub_current.fast_charging_current = 0;
			sec_multi_chg_check_charging_current(charger);
		}
	}

	return 0;
}

static int sec_multi_chg_check_current(struct sec_multi_charger_info *charger)
{
	union power_supply_propval value;
	bool sub_is_charging = charger->sub_is_charging;

	if ((charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) ||
		(charger->status == POWER_SUPPLY_STATUS_DISCHARGING) ||
		charger->chg_mode != SEC_BAT_CHG_MODE_CHARGING) {
		pr_info("%s: skip multi charging routine\n", __func__);
		return 0;
	}

	/* check sub charging */
	charger->sub_is_charging = sec_multi_chg_check_sub_charging(charger);

	/* set sub charging */
	if (charger->sub_is_charging != sub_is_charging) {
		if (charger->sub_is_charging)
			value.intval = SEC_BAT_CHG_MODE_CHARGING;
		else
			value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;

		/* set charging current */
		sec_multi_chg_check_input_current(charger);
		sec_multi_chg_check_charging_current(charger);

		psy_do_property(charger->pdata->sub_charger_name, set,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, value);

		pr_info("%s: change sub_is_charging(%d)\n", __func__, charger->sub_is_charging);
	}
	return 0;
}

static int sec_multi_chg_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct sec_multi_charger_info *charger =
		container_of(psy, struct sec_multi_charger_info, psy_chg);
	union power_supply_propval value;

	value.intval = val->intval;

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		psy_do_property("battery", get, POWER_SUPPLY_PROP_HEALTH, value);
		if (charger->cable_type != POWER_SUPPLY_TYPE_BATTERY && 
			value.intval != POWER_SUPPLY_HEALTH_UNDERVOLTAGE)
			psy_do_property(charger->pdata->sub_charger_name, get, psp, value);
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
	case POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		val->intval = value.intval;
		sec_multi_chg_check_current(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = charger->total_current.fast_charging_current;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_multi_chg_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct sec_multi_charger_info *charger =
		container_of(psy, struct sec_multi_charger_info, psy_chg);
	union power_supply_propval value, get_value;

	value.intval = val->intval;
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		charger->chg_mode = val->intval;
		psy_do_property(charger->pdata->main_charger_name, set,
			psp, value);

		psy_do_property(charger->pdata->main_charger_name, get, POWER_SUPPLY_PROP_ONLINE, get_value);
		if (get_value.intval != POWER_SUPPLY_TYPE_BATTERY) {
			if (val->intval != SEC_BAT_CHG_MODE_CHARGING) {
				psy_do_property(charger->pdata->sub_charger_name, set,
						psp, value);
			} else if (charger->sub_is_charging) {
				psy_do_property(charger->pdata->sub_charger_name, set,
						psp, value);
			}
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		psy_do_property(charger->pdata->main_charger_name, set,
			psp, value);
		psy_do_property(charger->pdata->sub_charger_name, set,
			psp, value);

		/* INIT */
		charger->cable_type = val->intval;
		charger->sub_is_charging = false;
		charger->pdata->main_charger_temp.level = 100;
		charger->pdata->sub_charger_temp.level = 100;
		charger->main_current.input_current_limit = 0;
		charger->main_current.fast_charging_current = 0;
		charger->sub_current.input_current_limit = 0;
		charger->sub_current.fast_charging_current = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		break;
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
	case POWER_SUPPLY_PROP_HEALTH:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		psy_do_property(charger->pdata->main_charger_name, set, psp, value);
		psy_do_property(charger->pdata->sub_charger_name, set, psp, value);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		charger->total_current.input_current_limit = val->intval;
		sec_multi_chg_check_input_current(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->total_current.fast_charging_current = val->intval;
		sec_multi_chg_check_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		charger->siop_level = val->intval;
		psy_do_property(charger->pdata->main_charger_name, set, psp, value);
		psy_do_property(charger->pdata->sub_charger_name, set, psp, value);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
	case POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL:
		psy_do_property(charger->pdata->main_charger_name, set, psp, value);
		break;
	case POWER_SUPPLY_PROP_USB_HC:
		break;
#if defined(CONFIG_AFC_CHARGER_MODE)
	case POWER_SUPPLY_PROP_AFC_CHARGER_MODE:
		psy_do_property(charger->pdata->main_charger_name, set, psp, value);
		break;
#endif
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		break;
	case POWER_SUPPLY_PROP_TEMP:
		sec_multi_chg_check_temperature(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		/* AICL Enable */
		if(!charger->pdata->aicl_disable)
			psy_do_property(charger->pdata->main_charger_name, set, psp, value);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_OF
static int sec_multi_charger_parse_dt(struct device *dev,
		struct sec_multi_charger_info *charger)
{
	struct device_node *np = dev->of_node;
	struct sec_multi_charger_platform_data *pdata = charger->pdata;
	int ret = 0;
	int len;
	const u32 *p;
	
	if (!np) {
		pr_err("%s: np NULL\n", __func__);
		return 1;
	} else {
		ret = of_property_read_string(np, "charger,battery_name",
				(char const **)&charger->pdata->battery_name);
		if (ret)
			pr_err("%s: battery_name is Empty\n", __func__);

		ret = of_property_read_string(np, "charger,main_charger",
				(char const **)&charger->pdata->main_charger_name);
		if (ret)
			pr_err("%s: main_charger is Empty\n", __func__);

		ret = of_property_read_string(np, "charger,sub_charger",
				(char const **)&charger->pdata->sub_charger_name);
		if (ret)
			pr_err("%s: sub_charger is Empty\n", __func__);

		pdata->is_serial = of_property_read_bool(np,
			"charger,is_serial");
		pdata->aicl_disable = of_property_read_bool(np,
			"charger,aicl_disable");

		ret = of_property_read_u32(np, "charger,sub_charger_condition",
				&pdata->sub_charger_condition);
		if (ret) {
			pr_err("%s: sub_charger_condition is Empty\n", __func__);
			pdata->sub_charger_condition = 0;
		}

		if (pdata->sub_charger_condition) {
			ret = of_property_read_u32(np, "charger,sub_charger_condition_current_max",
					&pdata->sub_charger_condition_current_max);
			if (ret) {
				pr_err("%s: sub_charger_condition_current_max is Empty\n", __func__);
				pdata->sub_charger_condition &= ~SEC_SUB_CHARGER_CONDITION_CURRENT_MAX;
				pdata->sub_charger_condition_current_max = 0;
			}

			ret = of_property_read_u32(np, "charger,sub_charger_condition_current_now",
					&pdata->sub_charger_condition_current_now);
			if (ret) {
				pr_err("%s: sub_charger_condition_current_now is Empty\n", __func__);
				pdata->sub_charger_condition &= ~SEC_SUB_CHARGER_CONDITION_CURRENT_NOW;
				pdata->sub_charger_condition_current_now = 0;
			}

			ret = of_property_read_u32(np, "charger,sub_charger_enable_current_now",
					&pdata->sub_charger_enable_current_now);
			if (ret) {
				pr_err("%s: sub_charger_enable_current_now is Empty\n", __func__);
				pdata->sub_charger_enable_current_now = 2200;
			}

			p = of_get_property(np, "charger,sub_charger_condition_online", &len);
			if (p) {
				len = len / sizeof(u32);

				pdata->sub_charger_condition_online = kzalloc(sizeof(unsigned int) * len,
								  GFP_KERNEL);
				ret = of_property_read_u32_array(np, "charger,sub_charger_condition_online",
						 pdata->sub_charger_condition_online, len);

				pdata->sub_charger_condition_online_size = len;
			} else {
				pdata->sub_charger_condition &= ~SEC_SUB_CHARGER_CONDITION_ONLINE;
				pdata->sub_charger_condition_online_size = 0;
			}

			pr_info("%s: sub_charger_condition(0x%x)\n", __func__, pdata->sub_charger_condition);
		}

		ret = of_property_read_u32(np, "charger,main_charger_temp_threshold",
				&pdata->main_charger_temp.threshold);
		if (ret)
			pr_err("%s: main_charger_temp_threshold is Empty\n", __func__);

		ret = of_property_read_u32(np, "charger,main_charger_temp_step",
				&pdata->main_charger_temp.step);
		if (ret)
			pr_err("%s: main_charger_temp_step is Empty\n", __func__);

		ret = of_property_read_u32(np, "charger,main_charger_temp_drop_level",
				&pdata->main_charger_temp.drop_level);
		if (ret)
			pr_err("%s: main_charger_temp_drop_level is Empty\n", __func__);

		ret = of_property_read_u32(np, "charger,sub_charger_temp_threshold",
				&pdata->sub_charger_temp.threshold);
		if (ret)
			pr_err("%s: sub_charger_temp_threshold is Empty\n", __func__);

		ret = of_property_read_u32(np, "charger,sub_charger_temp_step",
				&pdata->sub_charger_temp.step);
		if (ret)
			pr_err("%s: sub_charger_temp_step is Empty\n", __func__);

		ret = of_property_read_u32(np, "charger,sub_charger_temp_level",
				&pdata->sub_charger_temp.level);
		if (ret)
			pr_err("%s: sub_charger_temp_level is Empty\n", __func__);

		ret = of_property_read_u32(np, "charger,main_charger_current_level",
				&pdata->main_charger_current_level);
		if (ret) {
			pr_err("%s: main_charger_current_level is Empty\n", __func__);
			pdata->main_charger_current_level = 100;
		}

		ret = of_property_read_u32(np, "charger,sub_charger_current_level",
				&pdata->sub_charger_current_level);
		if (ret) {
			pr_err("%s: sub_charger_current_level is Empty\n", __func__);
			pdata->sub_charger_current_level = 100;
		}

		ret = of_property_read_u32(np, "charger,sub_charger_current_max",
				&pdata->sub_charger_current_max);
		if (ret) {
			pdata->sub_charger_current_max = 100000;
			pr_err("%s: sub_charger_current_max is %d\n", __func__,pdata->sub_charger_current_max);
		}
	}
	return 0;
}
#endif

static int __devinit sec_multi_charger_probe(struct platform_device *pdev)
{
	struct sec_multi_charger_info *charger;
	struct sec_multi_charger_platform_data *pdata = NULL;
	int ret = 0;

	dev_info(&pdev->dev,
		"%s: SEC Multi-Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct sec_multi_charger_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_charger_free;
		}

		charger->pdata = pdata;
		if (sec_multi_charger_parse_dt(&pdev->dev, charger)) {
			dev_err(&pdev->dev,
				"%s: Failed to get sec-multi-charger dt\n", __func__);
			ret = -EINVAL;
			goto err_charger_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		charger->pdata = pdata;
	}

	charger->sub_is_charging = false;

	platform_set_drvdata(pdev, charger);
	charger->dev = &pdev->dev;
	charger->psy_chg.name		= "sec-multi-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= sec_multi_chg_get_property;
	charger->psy_chg.set_property	= sec_multi_chg_set_property;
	charger->psy_chg.properties	= sec_multi_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(sec_multi_charger_props);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		dev_err(charger->dev,
			"%s: Failed to Register psy_chg\n", __func__);
		goto err_pdata_free;
	}

	dev_info(charger->dev,
		"%s: SEC Multi-Charger Driver Loaded\n", __func__);
	return 0;

err_pdata_free:
	kfree(pdata);
err_charger_free:
	kfree(charger);

	return ret;
}

static int __devexit sec_multi_charger_remove(struct platform_device *pdev)
{
	struct sec_multi_charger_info *charger = platform_get_drvdata(pdev);

	power_supply_unregister(&charger->psy_chg);

	dev_dbg(charger->dev, "%s: End\n", __func__);

	kfree(charger->pdata);
	kfree(charger);

	return 0;
}

static int sec_multi_charger_suspend(struct device *dev)
{
	return 0;
}

static int sec_multi_charger_resume(struct device *dev)
{
	return 0;
}

static void sec_multi_charger_shutdown(struct device *dev)
{
}

#ifdef CONFIG_OF
static struct of_device_id sec_multi_charger_dt_ids[] = {
	{ .compatible = "samsung,sec-multi-charger" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_multi_charger_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_multi_charger_pm_ops = {
	.suspend = sec_multi_charger_suspend,
	.resume = sec_multi_charger_resume,
};

static struct platform_driver sec_multi_charger_driver = {
	.driver = {
		.name = "sec-multi-charger",
		.owner = THIS_MODULE,
		.pm = &sec_multi_charger_pm_ops,
		.shutdown = sec_multi_charger_shutdown,
#ifdef CONFIG_OF
		.of_match_table = sec_multi_charger_dt_ids,
#endif
	},
	.probe = sec_multi_charger_probe,
	.remove = __devexit_p(sec_multi_charger_remove),
};

static int __init sec_multi_charger_init(void)
{
	pr_info("%s: \n", __func__);
	return platform_driver_register(&sec_multi_charger_driver);
}

static void __exit sec_multi_charger_exit(void)
{
	platform_driver_unregister(&sec_multi_charger_driver);
}

device_initcall_sync(sec_multi_charger_init);
module_exit(sec_multi_charger_exit);

MODULE_DESCRIPTION("Samsung Multi Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
