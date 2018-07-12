/*
 *  bq51221_charger.c
 *  Samsung bq51221 Charger Driver
 *
 *  Copyright (C) 2014 Samsung Electronics
 * Yeongmi Ha <yeongmi86.ha@samsung.com>
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

#include <linux/battery/charger/bq51221_charger.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>

#define ENABLE 1
#define DISABLE 0

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
};

static int bq51221_read_device(struct i2c_client *client,
		u8 reg, u8 bytes, void *dest)
{
	int ret;
	if (bytes > 1) {
		ret = i2c_smbus_read_i2c_block_data(client, reg, bytes, dest);
	} else {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret < 0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}
	return ret;
}

static int bq51221_reg_read(struct i2c_client *client, u8 reg)
{
	struct bq51221_charger_data *charger = i2c_get_clientdata(client);
	u8 data;
	int ret = 0;

	mutex_lock(&charger->io_lock);
	ret = bq51221_read_device(client, reg, 1, &data);
	mutex_unlock(&charger->io_lock);

	if (ret < 0) {
		pr_err("%s: can't read reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	} else {
		pr_err("%s: can read reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return (int)data;
	}
}

static int bq51221_reg_write(struct i2c_client *client, u8 reg, u8 data)
{
	struct bq51221_charger_data *charger = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&charger->io_lock);
	ret = i2c_smbus_write_byte_data(client, reg, data);
	mutex_unlock(&charger->io_lock);

	if (ret < 0)
		pr_err("%s: can't write reg(0x%x), ret(%d)\n", __func__, reg, ret);

	return ret;
}

static int bq51221_get_pad_mode(struct i2c_client *client)
{
	int ret = 0;
	int retry_cnt =0;
	struct bq51221_charger_data *charger = i2c_get_clientdata(client);

	if(charger->pdata->pad_mode != BQ51221_PAD_MODE_NONE) {
		/* read pad mode PMA = 1, WPC = 0 (Status bit)*/
		ret = bq51221_reg_read(client, BQ51221_REG_INDICATOR);
		if(ret < 0) {
			while(retry_cnt++ < 3) {
				msleep(50);
				pr_info("%s retry_cnt = %d, ret =%d \n",__func__, retry_cnt, ret);
				/* read pad mode PMA = 1, WPC = 0 (Status bit)*/
				ret = bq51221_reg_read(client, BQ51221_REG_INDICATOR);
				if(ret >= 0)
					break;
			}
		}
		pr_info("%s pad_mode = %d \n", __func__,ret);

		if(ret >= 0) {
			ret &= BQ51221_POWER_MODE_MASK;

			if(ret == 0)
				charger->pdata->pad_mode = BQ51221_PAD_MODE_WPC;
			else if (ret == 1)
				charger->pdata->pad_mode = BQ51221_PAD_MODE_PMA;
			else
				charger->pdata->pad_mode = BQ51221_PAD_MODE_WPC;
		}
		else
			ret = 0;
	}
	return ret;
}

int bq51221_set_full_charge_info(struct i2c_client *client)
{
	int data = 0;
	int ret = 0, i = 0;
	int retry_cnt =0;

	pr_info("%s\n", __func__);

	for(i=0; i< 3; i++) {
		/* send cs100 */
		ret = bq51221_reg_write(client, BQ51221_REG_USER_HEADER, BQ51221_EPT_HEADER_CS100);
		ret = bq51221_reg_write(client, BQ51221_REG_PROP_PACKET_PAYLOAD, BQ51221_CS100_VALUE);

		if(ret < 0) {
			while(retry_cnt++ < 3) {
				msleep(50);
				pr_info("%s retry_cnt = %d, ret =%d \n",__func__, retry_cnt, ret);
				/* send cs100 */
				ret = bq51221_reg_write(client, BQ51221_REG_USER_HEADER, BQ51221_EPT_HEADER_CS100);
				ret = bq51221_reg_write(client, BQ51221_REG_PROP_PACKET_PAYLOAD, BQ51221_CS100_VALUE);

				if(ret >= 0)
					break;
			}
			return ret;
		}

		/* send end packet */
		data = bq51221_reg_read(client, BQ51221_REG_MAILBOX);

		data &= !BQ51221_SEND_USER_PKT_DONE_MASK;
		ret = bq51221_reg_write(client, BQ51221_REG_MAILBOX, data);

		/* check packet error */
		data = bq51221_reg_read(client, BQ51221_REG_MAILBOX);
		data &= BQ51221_SEND_USER_PKT_ERR_MASK;
		data = data >> 5;

		pr_info("%s error pkt = 0x%x \n",__func__, data);

		if(data == BQ51221_PTK_ERR_NO_ERR) {
			pr_err("%s sent CS100!\n",__func__);
			ret = 1;
		} else {
			pr_err("%s can not send CS100! err pkt = 0x%x\n",__func__, data);
			ret = -1;
		}
		msleep(300);
	}
	return ret;
}

int bq51221_set_voreg(struct i2c_client *client, int default_value)
{
	u8 data = 0;
	int ret = 0;
	int retry_cnt =0;
	union power_supply_propval value;
	struct bq51221_charger_data *charger = i2c_get_clientdata(client);

#if defined(CONFIG_WIRELESS_CHARGER_INBATTERY_5V_FIX)
	return 0;
#endif

	if (charger->pdata->pad_mode == BQ51221_PAD_MODE_PMA) {
		pr_info("%s PMA MODE, do not set Voreg \n", __func__);
		return 0;
	}

	psy_do_property("battery", get,
				POWER_SUPPLY_PROP_CAPACITY, value);

	if ((value.intval >= charger->pdata->wireless_cc_cv) && !default_value)
		default_value = 1;

	if (default_value) {
		/* init VOREG with default value */
		ret = bq51221_reg_write(client, BQ51221_REG_CURRENT_REGISTER, 0x01);
		if(ret < 0) {
			while(retry_cnt++ < 3) {
				msleep(50);
				pr_debug("%s retry_cnt = %d, ret =%d \n",__func__, retry_cnt, ret);
				/* init VOREG with default value */
				ret = bq51221_reg_write(client, BQ51221_REG_CURRENT_REGISTER, 0x01);
				data = bq51221_reg_read(client, BQ51221_REG_CURRENT_REGISTER);
				if(ret >= 0) {
					pr_debug("%s VOREG = 0x%x \n", __func__, data);
					break;
				}
			}
		}
		data = bq51221_reg_read(client, BQ51221_REG_CURRENT_REGISTER);
		pr_info("%s VOREG = 0x%x 5.0V, cnt(%d)\n", __func__, data, retry_cnt);
	} else {
		ret = bq51221_reg_write(client, BQ51221_REG_CURRENT_REGISTER, 0x02);
		if(ret < 0) {
			while(retry_cnt++ < 3) {
				msleep(50);
				pr_debug("%s retry_cnt = %d, ret =%d \n",__func__, retry_cnt, ret);
				/* init VOREG with default value */
				ret = bq51221_reg_write(client, BQ51221_REG_CURRENT_REGISTER, 0x02);
				data = bq51221_reg_read(client, BQ51221_REG_CURRENT_REGISTER);
				if(ret >= 0) {
					pr_debug("%s VOREG = 0x%x \n", __func__, data);
					break;
				}
			}
		}
		data = bq51221_reg_read(client, BQ51221_REG_CURRENT_REGISTER);
		pr_info("%s VOREG = 0x%x 5.5V, cnt(%d)\n", __func__, data, retry_cnt);
	}
	return ret;
}

int bq51221_set_end_power_transfer(struct i2c_client *client, int ept_mode)
{

	int pad_mode = 0;
	int data = 0;
	int ret = 0;

	pr_info("%s\n", __func__);

	switch(ept_mode)
	{
		case END_POWER_TRANSFER_CODE_OVER_TEMPERATURE:
			/* read pad mode PMA = 1, WPC = 0 (Status bit)*/
			pad_mode = bq51221_reg_read(client, BQ51221_REG_INDICATOR);
			pr_info("%s pad_mode = %d \n", __func__,pad_mode);

			if(pad_mode > 0)
				pad_mode &= BQ51221_POWER_MODE_MASK;

			if(pad_mode) {
				pr_info("%s PMA MODE, send EOC \n", __func__);

				data = bq51221_reg_read(client, BQ51221_REG_MAILBOX);
				data |= BQ51221_SEND_EOC_MASK;
				ret = bq51221_reg_write(client, BQ51221_REG_MAILBOX, data);
			} else {
				pr_info("%s WPC MODE, send EPT-OT \n", __func__);

				ret = bq51221_reg_write(client, BQ51221_REG_USER_HEADER, BQ51221_EPT_HEADER_EPT);
				ret = bq51221_reg_write(client, BQ51221_REG_PROP_PACKET_PAYLOAD, BQ51221_EPT_CODE_OVER_TEMPERATURE);

				/* send end packet */
				data = bq51221_reg_read(client, BQ51221_REG_MAILBOX);
				data &= !BQ51221_SEND_USER_PKT_DONE_MASK;
				ret = bq51221_reg_write(client, BQ51221_REG_MAILBOX, data);

				/* check packet error */
				data = bq51221_reg_read(client, BQ51221_REG_MAILBOX);
				data &= BQ51221_SEND_USER_PKT_ERR_MASK;
				data = data >> 5;

				pr_info("%s error pkt = 0x%x \n",__func__, data);

				if(data != BQ51221_PTK_ERR_NO_ERR) {
					pr_err("%s can not send ept! err pkt = 0x%x\n",__func__, data);
					ret = -1;
				}
			}
			break;
		case END_POWER_TRANSFER_CODE_RECONFIGURE:
			pr_info("%s send EPT-Reconfigure \n", __func__);

			ret = bq51221_reg_write(client, BQ51221_REG_USER_HEADER, BQ51221_EPT_HEADER_EPT);
			ret = bq51221_reg_write(client, BQ51221_REG_PROP_PACKET_PAYLOAD, BQ51221_EPT_CODE_RECONFIGURE);

			/* send end packet */
			data = bq51221_reg_read(client, BQ51221_REG_MAILBOX);
			data &= !BQ51221_SEND_USER_PKT_DONE_MASK;
			ret = bq51221_reg_write(client, BQ51221_REG_MAILBOX, data);

			/* check packet error */
			data = bq51221_reg_read(client, BQ51221_REG_MAILBOX);
			data &= BQ51221_SEND_USER_PKT_ERR_MASK;
			data = data >> 5;

			pr_info("%s error pkt = 0x%x \n",__func__, data);

			if(data != BQ51221_PTK_ERR_NO_ERR) {
				pr_err("%s can not send ept! err pkt = 0x%x\n",__func__, data);
				ret = -1;
			}
			break;
		default:
			pr_info("%s this ept mode is not reserved \n",__func__);
			ret = -1;
			break;
	}

	return ret;
}

void bq51221_wireless_chg_init(struct i2c_client *client)
{
	int data = 0;
	union power_supply_propval value;
	struct bq51221_charger_data *charger = i2c_get_clientdata(client);

	pr_info("%s\n", __func__);

	psy_do_property("battery", get,
				POWER_SUPPLY_PROP_CAPACITY, value);
	/* init I limit(IOREG) */
	bq51221_reg_write(client, BQ51221_REG_CURRENT_REGISTER2, BQ51221_IOREG_100_VALUE);
	data = bq51221_reg_read(client, BQ51221_REG_CURRENT_REGISTER2);
	pr_info("%s IOREG = 0x%x \n", __func__, data);

	/* init CEP timing */

	/* init RCVD PWR */

	/* read pad mode */
	bq51221_get_pad_mode(client);

	pr_info("%s siop = %d \n" ,__func__, charger->pdata->siop_level );
	if ((value.intval < charger->pdata->wireless_cc_cv) &&
		(charger->pdata->pad_mode == BQ51221_PAD_MODE_WPC) &&
		(charger->pdata->siop_level == 100) &&
		!charger->pdata->default_voreg) {
		/* set VOREG set 5.5V*/
		bq51221_set_voreg(charger->client, 0);
	} else {
		/* init VOREG with default value */
		bq51221_set_voreg(charger->client, 1);
	}
}

static void bq51221_detect_work(
		struct work_struct *work)
{
	struct bq51221_charger_data *charger =
		container_of(work, struct bq51221_charger_data, wpc_work.work);

	pr_info("%s\n", __func__);

	bq51221_wireless_chg_init(charger->client);
}

static int bq51221_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct bq51221_charger_data *charger =
		container_of(psy, struct bq51221_charger_data, psy_chg);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			pr_info("%s charger->pdata->cs100_status %d \n",__func__,charger->pdata->cs100_status);
			val->intval = charger->pdata->cs100_status;
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			val->intval = bq51221_get_pad_mode(charger->client);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = 1;
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			val->intval = charger->pdata->siop_level;
			break;
		case POWER_SUPPLY_PROP_ONLINE:
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static int bq51221_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct bq51221_charger_data *charger =
		container_of(psy, struct bq51221_charger_data, psy_chg);
	union power_supply_propval value;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			if(val->intval == POWER_SUPPLY_STATUS_FULL) {
				charger->pdata->cs100_status = bq51221_set_full_charge_info(charger->client);
				pr_info("%s charger->pdata->cs100_status %d \n",__func__,charger->pdata->cs100_status);
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			if (!charger->pdata->default_voreg &&
				!delayed_work_pending(&charger->wpc_work))
				bq51221_set_voreg(charger->client, val->intval);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			if(val->intval == POWER_SUPPLY_HEALTH_OVERHEAT ||
				val->intval == POWER_SUPPLY_HEALTH_OVERHEATLIMIT ||
				val->intval == POWER_SUPPLY_HEALTH_COLD)
				bq51221_set_end_power_transfer(charger->client, END_POWER_TRANSFER_CODE_OVER_TEMPERATURE);
			else if(val->intval == POWER_SUPPLY_HEALTH_UNDERVOLTAGE)
				bq51221_set_end_power_transfer(charger->client, END_POWER_TRANSFER_CODE_RECONFIGURE);
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			if(val->intval == POWER_SUPPLY_TYPE_WIRELESS) {
				charger->pdata->pad_mode = BQ51221_PAD_MODE_WPC;
				queue_delayed_work(charger->wqueue, &charger->wpc_work,
					msecs_to_jiffies(5000));
				wake_lock_timeout(&charger->wpc_wake_lock, HZ * 6);
			} else if(val->intval == POWER_SUPPLY_TYPE_BATTERY) {
				bq51221_set_voreg(charger->client, 1);
				charger->pdata->pad_mode = BQ51221_PAD_MODE_NONE;
				cancel_delayed_work(&charger->wpc_work);
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			charger->pdata->siop_level = val->intval;
			pr_info("%s siop = %d \n",__func__, charger->pdata->siop_level);
			break;
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			if (val->intval) {
				charger->pdata->default_voreg = true;
				bq51221_set_voreg(charger->client, val->intval);
			} else {
				charger->pdata->default_voreg = false;
				psy_do_property("battery", get,
						POWER_SUPPLY_PROP_STATUS, value);
				if ((value.intval == POWER_SUPPLY_STATUS_CHARGING) &&
					(charger->pdata->pad_mode == BQ51221_PAD_MODE_WPC)) {
					queue_delayed_work(charger->wqueue, &charger->wpc_work,
						msecs_to_jiffies(5000));
					wake_lock_timeout(&charger->wpc_wake_lock, HZ * 6);
				}
			}
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

#if 0 /* this part is for bq51221s */
static void bq51221_chg_isr_work(struct work_struct *work)
{
	//struct bq51221_charger_data *charger =
	//	container_of(work, struct bq51221_charger_data, isr_work.work);

	pr_info("%s \n",__func__);
}

static irqreturn_t bq51221_chg_irq_thread(int irq, void *irq_data)
{
	struct bq51221_charger_data *charger = irq_data;

	pr_info("%s \n",__func__);
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}
#endif

static int bq51221_chg_parse_dt(struct device *dev,
		bq51221_charger_platform_data_t *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;

	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np,
			"battery,wireless_cc_cv", &pdata->wireless_cc_cv);

		ret = of_property_read_string(np,
			"battery,wirelss_charger_name", (char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: Vendor is Empty\n", __func__);
	}


	return ret;

#if 0 /* this part is for bq51221s */
	ret = pdata->irq_gpio = of_get_named_gpio_flags(np, "bq51221-charger,irq-gpio",
			0, &irq_gpio_flags);
	if (ret < 0) {
		dev_err(dev, "%s : can't get irq-gpio\r\n", __FUNCTION__);
		return ret;
	}
	pr_info("%s irq_gpio = %d \n",__func__, pdata->irq_gpio);
	pdata->irq_base = gpio_to_irq(pdata->irq_gpio);

	return 0;
#endif
}

static int bq51221_charger_probe(
						struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct device_node *of_node = client->dev.of_node;
	struct bq51221_charger_data *charger;
	bq51221_charger_platform_data_t *pdata = client->dev.platform_data;
	int ret = 0;

	dev_info(&client->dev,
		"%s: bq51221 Charger Driver Loading\n", __func__);

	if (of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = bq51221_chg_parse_dt(&client->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		pdata = client->dev.platform_data;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (charger == NULL) {
		dev_err(&client->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_wpc_nomem;
	}
	charger->dev = &client->dev;

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(client->adapter);
		dev_err(charger->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}

	charger->client = client;
	charger->pdata = pdata;

    pr_info("%s: %s\n", __func__, charger->pdata->wireless_charger_name );

	/* if board-init had already assigned irq_base (>=0) ,
	no need to allocate it;
	assign -1 to let this driver allocate resource by itself*/
#if 0 /* this part is for bq51221s */
    if (pdata->irq_base < 0)
        pdata->irq_base = irq_alloc_descs(-1, 0, BQ51221_EVENT_IRQ, 0);
	if (pdata->irq_base < 0) {
		pr_err("%s: irq_alloc_descs Fail! ret(%d)\n",
				__func__, pdata->irq_base);
		ret = -EINVAL;
		goto irq_base_err;
	} else {
		charger->irq_base = pdata->irq_base;
		pr_info("%s: irq_base = %d\n",
			 __func__, charger->irq_base);

#if (LINUX_VERSION_CODE>=KERNEL_VERSION(3,4,0))
		irq_domain_add_legacy(of_node, BQ51221_EVENT_IRQ, charger->irq_base, 0,
				      &irq_domain_simple_ops, NULL);
#endif /*(LINUX_VERSION_CODE>=KERNEL_VERSION(3,4,0))*/
	}
#endif
	i2c_set_clientdata(client, charger);

	charger->psy_chg.name		= pdata->wireless_charger_name;
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= bq51221_chg_get_property;
	charger->psy_chg.set_property	= bq51221_chg_set_property;
	charger->psy_chg.properties	= sec_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(sec_charger_props);

	mutex_init(&charger->io_lock);

#if 0 /* this part is for bq51221s */

	if (charger->chg_irq) {
		INIT_DELAYED_WORK(
			&charger->isr_work, bq51221_chg_isr_work);

		ret = request_threaded_irq(charger->chg_irq,
				NULL, bq51221_chg_irq_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"charger-irq", charger);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_supply_unreg;
		}

		ret = enable_irq_wake(charger->chg_irq);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: Failed to Enable Wakeup Source(%d)\n",
				__func__, ret);
	}
#endif
	charger->pdata->cs100_status = 0;
	charger->pdata->pad_mode = BQ51221_PAD_MODE_NONE;
	charger->pdata->siop_level = 100;
	charger->pdata->default_voreg = false;

	ret = power_supply_register(&client->dev, &charger->psy_chg);
	if (ret) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_chg\n", __func__);
		goto err_supply_unreg;
	}

	charger->wqueue = create_workqueue("bq51221_workqueue");
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}

	wake_lock_init(&(charger->wpc_wake_lock), WAKE_LOCK_SUSPEND,
			"wpc_wakelock");
	INIT_DELAYED_WORK(&charger->wpc_work, bq51221_detect_work);

	dev_info(&client->dev,
		"%s: bq51221 Charger Driver Loaded\n", __func__);

	return 0;

err_pdata_free:
	power_supply_unregister(&charger->psy_chg);
err_supply_unreg:
	mutex_destroy(&charger->io_lock);
err_i2cfunc_not_support:
	kfree(charger);
err_wpc_nomem:
err_parse_dt:
	kfree(pdata);
	return ret;
}

static int bq51221_charger_remove(struct i2c_client *client)
{
	return 0;
}

#if defined CONFIG_PM
static int bq51221_charger_suspend(struct i2c_client *client,
				pm_message_t state)
{


	return 0;
}

static int bq51221_charger_resume(struct i2c_client *client)
{


	return 0;
}
#else
#define p9015_charger_suspend NULL
#define p9015_charger_resume NULL
#endif

static void bq51221_charger_shutdown(struct i2c_client *client)
{
	struct bq51221_charger_data *charger = i2c_get_clientdata(client);
	int data = 0;

	if(charger->pdata->pad_mode != BQ51221_PAD_MODE_NONE) {
		/* init VOREG set 5.0V*/
		bq51221_reg_write(client, BQ51221_REG_CURRENT_REGISTER, 0x01);
		data = bq51221_reg_read(client, BQ51221_REG_CURRENT_REGISTER);
		pr_info("%s VOREG = 0x%x \n", __func__, data);
	}
}

static const struct i2c_device_id bq51221_charger_id_table[] = {
	{ "bq51221-charger", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bq51221_id_table);

#ifdef CONFIG_OF
static struct of_device_id bq51221_charger_match_table[] = {
	{ .compatible = "ti,bq51221-charger",},
	{},
};
#else
#define bq51221_charger_match_table NULL
#endif

static struct i2c_driver bq51221_charger_driver = {
	.driver = {
		.name	= "bq51221-charger",
		.owner	= THIS_MODULE,
		.of_match_table = bq51221_charger_match_table,
	},
	.shutdown	= bq51221_charger_shutdown,
	.suspend	= bq51221_charger_suspend,
	.resume		= bq51221_charger_resume,
	.probe	= bq51221_charger_probe,
	.remove	= bq51221_charger_remove,
	.id_table	= bq51221_charger_id_table,
};

static int __init bq51221_charger_init(void)
{
	pr_info("%s \n",__func__);
	return i2c_add_driver(&bq51221_charger_driver);
}

static void __exit bq51221_charger_exit(void)
{
	pr_info("%s \n",__func__);
	i2c_del_driver(&bq51221_charger_driver);
}

module_init(bq51221_charger_init);
module_exit(bq51221_charger_exit);

MODULE_DESCRIPTION("Samsung bq51221 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
