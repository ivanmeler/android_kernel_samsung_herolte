/*
 * max77804-muic.c - MUIC driver for the Maxim 77804
 *
 *  Copyright (C) 2012 Samsung Electronics
 *  <seo0.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <linux/mfd/max77804.h>
#include <linux/mfd/max77804-private.h>

/* MUIC header file */
#include <linux/muic/max77804-muic.h>
#include <linux/muic/muic.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

extern struct muic_platform_data muic_pdata;

/* muic chip specific internal data structure */
struct max77804_muic_data {
	struct device			*dev;
	struct i2c_client		*i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex			muic_mutex;

	/* model dependant mfd platform data */
	struct max77804_platform_data	*mfd_pdata;

	int				irq_adcerr;
	int				irq_adc;
	int				irq_chgtyp;
	int				irq_vbvolt;

	/* model dependant muic platform data */
	struct muic_platform_data	*pdata;

	/* muic current attached device */
	muic_attached_dev_t		attached_dev;

	/* muic support vps list */
	bool muic_support_list[ATTACHED_DEV_NUM];

	bool				is_muic_ready;

	/* check is otg test for jig uart off + vb */
	bool				is_otg_test;
};

/* don't access this variable directly!! except get_switch_sel_value function.
 * you must get switch_sel value by using get_switch_sel function. */
static int switch_sel;

/* func : set_switch_sel
 * switch_sel value get from bootloader comand line
 * switch_sel data consist 8 bits (xxxxzzzz)
 * first 4bits(zzzz) mean path infomation.
 * next 4bits(xxxx) mean if pmic version info
 */
static int set_switch_sel(char *str)
{
	get_option(&str, &switch_sel);
	switch_sel = switch_sel & 0x0f;
	pr_info("%s:%s switch_sel: 0x%x\n", MUIC_DEV_NAME, __func__,
			switch_sel);

	return switch_sel;
}
__setup("pmic_info=", set_switch_sel);

static int get_switch_sel(void)
{
	return switch_sel;
}

struct max77804_muic_vps_data {
	u8				adc1k;
	u8				adcerr;
	u8				adclow;
	muic_adc_t			adc;
	vbvolt_t			vbvolt;
	chgdetrun_t			chgdetrun;
	chgtyp_t			chgtyp;
	max77804_reg_ctrl1_t		control1;
	const char			*vps_name;
	const muic_attached_dev_t	attached_dev;
};

static const struct max77804_muic_vps_data muic_vps_table[] = {
	{
		.adc1k		= (0x1 << STATUS1_ADC1K_SHIFT),
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_DONTCARE,
		.vbvolt		= VB_DONTCARE,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_OPEN,
		.vps_name	= "MHL",
		.attached_dev	= ATTACHED_DEV_MHL_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x0 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_GND,
		.vbvolt		= VB_LOW,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "OTG",
		.attached_dev	= ATTACHED_DEV_OTG_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x0 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_GND,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "OTG charging pump (vbvolt)",
		.attached_dev	= ATTACHED_DEV_OTG_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_JIG_USB_ON,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_USB,
		.vps_name	= "Jig USB On",
		.attached_dev	= ATTACHED_DEV_JIG_USB_ON_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_JIG_UART_OFF,
		.vbvolt		= VB_LOW,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART Off",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_JIG_UART_OFF,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART Off + VB",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_VB_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "TA",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_USB,
		.control1	= CTRL1_USB,
		.vps_name	= "USB",
		.attached_dev	= ATTACHED_DEV_USB_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_OPEN,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_CDP,
		.control1	= CTRL1_USB,
		.vps_name	= "CDP",
		.attached_dev	= ATTACHED_DEV_CDP_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_DESKDOCK,
		.vbvolt		= VB_LOW,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.control1	= CTRL1_AUDIO,
		.vps_name	= "Deskdock",
		.attached_dev	= ATTACHED_DEV_DESKDOCK_MUIC,
	},
	{
		.adc1k		= 0x00,
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_DESKDOCK,
		.vbvolt		= VB_HIGH,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.control1	= CTRL1_AUDIO,
		.vps_name	= "Deskdock + VB",
		.attached_dev	= ATTACHED_DEV_DESKDOCK_VB_MUIC,
	},
};

static int muic_lookup_vps_table(muic_attached_dev_t new_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		const struct max77804_muic_vps_data *tmp_vps;
		tmp_vps = &(muic_vps_table[i]);

		if (tmp_vps->attached_dev != new_dev)
			continue;

		pr_info("%s:%s (%d) vps table match found at i(%d), %s\n",
				MUIC_DEV_NAME, __func__, new_dev, i,
				tmp_vps->vps_name);

		return i;
	}

	pr_info("%s:%s can't find (%d) on vps table\n", MUIC_DEV_NAME,
			__func__, new_dev);

	return -ENODEV;
}

static bool muic_check_support_dev(struct max77804_muic_data *muic_data,
			muic_attached_dev_t attached_dev)
{
	bool ret = muic_data->muic_support_list[attached_dev];

	pr_info("%s:%s [%c]\n", MUIC_DEV_NAME, __func__, ret ? 'T':'F');

	return ret;
}

static int max77804_muic_update_reg(struct i2c_client *i2c, const u8 reg,
		const u8 val, const u8 mask, const bool debug_en)
{
	int ret = 0;
	u8 before_val, new_val, after_val;

	ret = max77804_read_reg(i2c, reg, &before_val);
	if (ret < 0)
		pr_err("%s:%s err read REG(0x%02x) [%d]\n", MUIC_DEV_NAME,
				__func__, reg, ret);

	new_val = (val & mask) | (before_val & (~mask));

	if (before_val ^ new_val) {
		ret = max77804_write_reg(i2c, reg, new_val);
		if (ret < 0)
			pr_err("%s:%s err write REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);
	} else if (debug_en) {
		pr_info("%s:%s REG(0x%02x): already [0x%02x], don't write reg\n",
				MUIC_DEV_NAME, __func__, reg, before_val);
		goto out;
	}

	if (debug_en) {
		ret = max77804_read_reg(i2c, reg, &after_val);
		if (ret < 0)
			pr_err("%s:%s err read REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);

		pr_info("%s:%s REG(0x%02x): [0x%02x]+[0x%02x:0x%02x]=[0x%02x]\n",
				MUIC_DEV_NAME, __func__, reg, before_val,
				val, mask, after_val);
	}

out:
	return ret;
}

static int write_muic_ctrl_reg(struct max77804_muic_data *muic_data,
				const u8 reg, const u8 val)
{
	return max77804_muic_update_reg(muic_data->i2c, reg, val, 0xff, true);
}

static int com_to_open(struct max77804_muic_data *muic_data)
{
	max77804_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_OPEN;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77804_MUIC_REG_CTRL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME,
				__func__);

	return ret;
}

static int com_to_usb_ap(struct max77804_muic_data *muic_data)
{
	max77804_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_USB;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77804_MUIC_REG_CTRL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME,
				__func__);

	if (muic_data->pdata->set_safeout) {
		ret = muic_data->pdata->set_safeout(MUIC_PATH_USB_AP);
		if (ret)
			pr_err("%s:%s set_safeout err(%d)\n", MUIC_DEV_NAME,
			__func__, ret);
	}

	return ret;
}

static int com_to_usb_cp(struct max77804_muic_data *muic_data)
{
	max77804_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_USB_CP;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77804_MUIC_REG_CTRL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME,
				__func__);

	if (muic_data->pdata->set_safeout) {
		ret = muic_data->pdata->set_safeout(MUIC_PATH_USB_CP);
		if (ret)
			pr_err("%s:%s set_safeout err(%d)\n", MUIC_DEV_NAME,
			__func__, ret);
	}

	return ret;
}

static int com_to_uart_ap(struct max77804_muic_data *muic_data)
{
	max77804_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_UART;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77804_MUIC_REG_CTRL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME,
				__func__);

	return ret;
}

static int com_to_uart_cp(struct max77804_muic_data *muic_data)
{
	max77804_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_UART_CP;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77804_MUIC_REG_CTRL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME,
				__func__);

	return ret;
}

static int com_to_audio(struct max77804_muic_data *muic_data)
{
	max77804_reg_ctrl1_t reg_val;
	int ret;

	reg_val = CTRL1_AUDIO;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77804_MUIC_REG_CTRL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME,
				__func__);

	return ret;
}

static int write_vps_regs(struct max77804_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	const struct max77804_muic_vps_data *tmp_vps;
	int vps_index;
	int ret = 0;

	vps_index = muic_lookup_vps_table(new_dev);
	if (vps_index < 0)
		return -ENODEV;

	tmp_vps = &(muic_vps_table[vps_index]);

	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77804_MUIC_REG_CTRL1,
			tmp_vps->control1);

	return ret;
}

static int max77804_muic_enable_chgdet(struct max77804_muic_data *muic_data)
{
	return max77804_muic_update_reg(muic_data->i2c, MAX77804_MUIC_REG_CDETCTRL1,
				(MAX77804_ENABLE_BIT << CHGDETEN_SHIFT),
				CHGDETEN_MASK, true);
}

static int max77804_muic_disable_chgdet(struct max77804_muic_data *muic_data)
{
	return max77804_muic_update_reg(muic_data->i2c, MAX77804_MUIC_REG_CDETCTRL1,
				(MAX77804_DISABLE_BIT << CHGDETEN_SHIFT),
				CHGDETEN_MASK, true);
}

static int max77804_muic_enable_accdet(struct max77804_muic_data *muic_data)
{
	return max77804_muic_update_reg(muic_data->i2c, MAX77804_MUIC_REG_CTRL2,
				(MAX77804_ENABLE_BIT << CTRL2_ACCDET_SHIFT),
				CTRL2_ACCDET_MASK, true);
}

#if defined(CONFIG_TEMP_FUELGAUGE_TEST)
static int max77804_muic_disable_accdet(struct max77804_muic_data *muic_data)
{
	return max77804_muic_update_reg(muic_data->i2c, MAX77804_MUIC_REG_CTRL2,
				(MAX77804_DISABLE_BIT << CTRL2_ACCDET_SHIFT),
				CTRL2_ACCDET_MASK, true);
}
#endif /* CONFIG_TEMP_FUELGAUGE_TEST */

static u8 max77804_muic_get_adc_value(struct max77804_muic_data *muic_data)
{
	u8 status;
	u8 adc = ADC_ERROR;
	int ret = 0;

	ret = max77804_read_reg(muic_data->i2c, MAX77804_MUIC_REG_STATUS1,
			&status);
	if (ret)
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
	else
		adc = status & STATUS1_ADC_MASK;

	return adc;
}

static ssize_t max77804_muic_show_uart_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		mode = "AP\n";
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP\n";
		break;
	default:
		break;
	}

	pr_info("%s:%s %s", MUIC_DEV_NAME, __func__, mode);
	return sprintf(buf, mode);
}

/* muic uart path control function */
static int switch_to_ap_uart(struct max77804_muic_data *muic_data)
{
	int ret = 0;

	if (muic_data->attached_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC)
		pr_warn("%s:%s current attached is (%d) not Jig UART Off\n",
				MUIC_DEV_NAME, __func__,
				muic_data->attached_dev);
	else
		ret = com_to_uart_ap(muic_data);

	return ret;
}

static int switch_to_cp_uart(struct max77804_muic_data *muic_data)
{
	int ret = 0;

	if (muic_data->attached_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC)
		pr_warn("%s:%s current attached is (%d) not Jig UART Off\n",
				MUIC_DEV_NAME, __func__,
				muic_data->attached_dev);
	else
		ret = com_to_uart_cp(muic_data);

	return ret;
}

static ssize_t max77804_muic_set_uart_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "AP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		switch_to_ap_uart(muic_data);
	} else if (!strncasecmp(buf, "CP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		switch_to_cp_uart(muic_data);
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s uart_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->uart_path);

	return count;
}

static ssize_t max77804_muic_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		mode = "PDA\n";
		break;
	case MUIC_PATH_USB_CP:
		mode = "MODEM\n";
		break;
	default:
		break;
	}

	pr_info("%s:%s %s", MUIC_DEV_NAME, __func__, mode);
	return sprintf(buf, mode);
}

static ssize_t max77804_muic_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3)) {
		pdata->usb_path = MUIC_PATH_USB_AP;
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		pdata->usb_path = MUIC_PATH_USB_CP;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	return count;
}

static ssize_t max77804_muic_show_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	u8 adc;

	adc = max77804_muic_get_adc_value(muic_data);
	pr_info("%s:%s adc(0x%02x)\n", MUIC_DEV_NAME, __func__, adc);

	if (adc == ADC_ERROR) {
		pr_err("%s:%s fail to read adc value\n", MUIC_DEV_NAME,
				__func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", adc);
}

static ssize_t max77804_muic_show_usb_state(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return sprintf(buf, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t max77804_muic_show_attached_dev(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	const struct max77804_muic_vps_data *tmp_vps;
	int vps_index;

	vps_index = muic_lookup_vps_table(muic_data->attached_dev);
	if (vps_index < 0)
		return sprintf(buf, "Error No Device\n");

	tmp_vps = &(muic_vps_table[vps_index]);

	return sprintf(buf, "%s\n", tmp_vps->vps_name);
}

#if defined(CONFIG_MUIC_MAX77804_SUPPROT_AUDIO_FACTORY)
static ssize_t max77804_muic_show_audio_path(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = max77804_read_reg(muic_data->i2c, MAX77804_MUIC_REG_CTRL1, &val);
	if (ret) {
		pr_err("%s:%s fail to read muic reg\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", val);
}

static ssize_t max77804_muic_set_audio_path(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);

	if (!strncmp(buf, "0", 1))
		com_to_open(muic_data);
	else if (!strncmp(buf, "1", 1))
		com_to_audio(muic_data);
	else
		pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);

	return count;
}
#endif /* CONFIG_MUIC_MAX77804_SUPPROT_AUDIO_FACTORY */

static ssize_t max77804_muic_show_otg_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	struct i2c_client *i2c = muic_data->i2c;
	int ret = -ENODEV;
	u8 val;

	if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC)) {
		ret = max77804_read_reg(i2c, MAX77804_MUIC_REG_CDETCTRL1, &val);
		pr_info("%s:%s ret:%d val:%x buf:%s\n", MUIC_DEV_NAME, __func__, ret, val, buf);
		if (ret) {
			pr_err("%s:%s: fail to read muic reg\n", MUIC_DEV_NAME, __func__);
			return sprintf(buf, "UNKNOWN\n");
		}
		val &= CHGDETEN_MASK;
		return sprintf(buf, "%x\n", val);
	} else
		return ret;

}

static ssize_t max77804_muic_set_otg_test(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	struct i2c_client *i2c = muic_data->i2c;
	u8 val = 0;
	int ret = -ENODEV;

	if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC)) {
		pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);
		if (!strncmp(buf, "0", 1)) {
			muic_data->is_otg_test = true;
			ret = max77804_muic_disable_chgdet(muic_data);
			if (ret)
				goto err_chgdet;
			ret = max77804_muic_enable_accdet(muic_data);
			if (ret)
				goto err_accdet;
		} else if (!strncmp(buf, "1", 1)) {
			muic_data->is_otg_test = false;
			ret = max77804_muic_enable_chgdet(muic_data);
			if (ret)
				goto err_chgdet;
		} else {
			pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
			return count;
		}

		max77804_read_reg(i2c, MAX77804_MUIC_REG_CDETCTRL1, &val);
		pr_info("%s:%s CDETCTRL(0x%02x)\n", MUIC_DEV_NAME, __func__, val);

		return count;
	} else
		return ret;

err_chgdet:
	pr_err("%s:%s cannot change chgdet!\n", MUIC_DEV_NAME, __func__);
	return ret;

err_accdet:
	pr_err("%s:%s cannot change accdet!\n", MUIC_DEV_NAME, __func__);
	return ret;
}

static void max77804_muic_set_adcdbset
			(struct max77804_muic_data *muic_data, int value)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret;
	u8 val;
	u8 cntl3_before, cntl3_after;

	if (value > 3) {
		pr_err("%s:%s invalid value(%d)\n", MUIC_DEV_NAME, __func__,
				value);
		return;
	}

	if (!i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	val = (value << CTRL3_ADCDBSET_SHIFT);
	max77804_read_reg(i2c, MAX77804_MUIC_REG_CTRL3, &cntl3_before);

	/*
	 * Warning: When changing CONTROL3 register's ADCDBSET value,
	 *         we should overwrite the value never update value.
	 */
	ret = max77804_write_reg(i2c, MAX77804_MUIC_REG_CTRL3, val);
	if (ret < 0)
		pr_err("%s: fail to write reg\n", __func__);

	max77804_read_reg(i2c, MAX77804_MUIC_REG_CTRL3, &cntl3_after);

	pr_info("%s:%s CNTL3: before(0x%02x), value(0x%02x), after(0x%02x)\n",
			MUIC_DEV_NAME, __func__, cntl3_before, val, cntl3_after);
}

static DEVICE_ATTR(uart_sel, 0664, max77804_muic_show_uart_sel,
		max77804_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664, max77804_muic_show_usb_sel,
		max77804_muic_set_usb_sel);
static DEVICE_ATTR(adc, S_IRUGO, max77804_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, S_IRUGO, max77804_muic_show_usb_state, NULL);
static DEVICE_ATTR(attached_dev, S_IRUGO, max77804_muic_show_attached_dev, NULL);
#if defined(CONFIG_MUIC_MAX77804_SUPPROT_AUDIO_FACTORY)
static DEVICE_ATTR(audio_path, 0664,
		max77804_muic_show_audio_path, max77804_muic_set_audio_path);
#endif /* CONFIG_MUIC_MAX77804_SUPPORT_AUDIO_FACTORY */
static DEVICE_ATTR(otg_test, 0664,
		max77804_muic_show_otg_test, max77804_muic_set_otg_test);

static struct attribute *max77804_muic_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_attached_dev.attr,
#if defined(CONFIG_MUIC_MAX77804_SUPPROT_AUDIO_FACTORY)
	&dev_attr_audio_path.attr,
#endif /* CONFIG_MUIC_MAX77804_SUPPORT_AUDIO_FACTORY */
	&dev_attr_otg_test.attr,
	NULL
};

static const struct attribute_group max77804_muic_group = {
	.attrs = max77804_muic_attributes,
};

static int max77804_muic_attach_uart_path(struct max77804_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->uart_path == MUIC_PATH_UART_AP) {
		ret = write_vps_regs(muic_data, new_dev);
	} else if (pdata->uart_path == MUIC_PATH_UART_CP) {
		ret = com_to_uart_cp(muic_data);
	}
	else
		pr_warn("%s:%s invalid uart_path\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int max77804_muic_attach_usb_path(struct max77804_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s usb_path=%d\n", MUIC_DEV_NAME, __func__, pdata->usb_path);

	if (pdata->usb_path == MUIC_PATH_USB_AP) {
		ret = com_to_usb_ap(muic_data);
	}
	else if (pdata->usb_path == MUIC_PATH_USB_CP) {
		ret = com_to_usb_cp(muic_data);
	}
	else
		pr_warn("%s:%s invalid usb_path\n", MUIC_DEV_NAME, __func__);

	return ret;
}

#if defined(CONFIG_MUIC_NOTIFIER)
static int muic_notifier_detach_dev(struct max77804_muic_data *muic_data)
{
	pr_info("%s:%s (%d)\n", MUIC_DEV_NAME, __func__, muic_data->attached_dev);

	muic_notifier_detach_attached_dev(muic_data->attached_dev);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return 0;
}

static int muic_notifier_attach_dev(struct max77804_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	pr_info("%s:%s from (%d) to (%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev, new_dev);

	muic_data->attached_dev = new_dev;

	muic_notifier_attach_attached_dev(muic_data->attached_dev);

	return 0;
}

static int muic_notifier_logically_detach_dev(struct max77804_muic_data *muic_data)
{
	pr_info("%s:%s (%d)\n", MUIC_DEV_NAME, __func__, muic_data->attached_dev);

	muic_notifier_logically_detach_attached_dev(muic_data->attached_dev);

	return 0;
}

static int muic_notifier_logically_attach_dev(struct max77804_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	pr_info("%s:%s from (%d) to (%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev, new_dev);

	muic_data->attached_dev = new_dev;

	muic_notifier_logically_attach_attached_dev(muic_data->attached_dev);

	return 0;
}

#endif /* CONFIG_MUIC_NOTIFIER */

static muic_attached_dev_t check_jig_uart_off_vb_test_type
			(struct max77804_muic_data *muic_data,	muic_attached_dev_t new_dev)
{
	muic_attached_dev_t ret_ndev = new_dev;

	if (muic_data->is_otg_test) {
		if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC))
			ret_ndev = ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC;
		else
			pr_info("%s:%s Not support 'OTG'\n", MUIC_DEV_NAME, __func__);
	} else {
		ret_ndev = ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC;
#if defined(CONFIG_TEMP_FUELGAUGE_TEST)
		ret = max77804_muic_disable_accdet(muic_data);
		if (ret) {
			pr_err("%s:%s cannot change accdet\n", MUIC_DEV_NAME, __func__);
			return ret;
		}
#endif /* CONFIG_TEMP_FUELGAUGE_TEST */
	}
	pr_info("%s:%s is_otg_test = %c\n", MUIC_DEV_NAME, __func__,
			(muic_data->is_otg_test) ? 'T' : 'F');

	return ret_ndev;
}

static int max77804_muic_handle_detach(struct max77804_muic_data *muic_data)
{
	int ret = 0;

	if (muic_data->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s:%s Duplicated(%d), just ignore\n", MUIC_DEV_NAME,
				__func__, muic_data->attached_dev);
		return ret;
	}

	/* Enable Factory Accessory Detection State Machine */
	max77804_muic_enable_accdet(muic_data);

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_lookup_vps_table(muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
		/* Enable Charger Detection */
		max77804_muic_enable_chgdet(muic_data);
		break;
	default:
		break;
	}

	muic_notifier_detach_dev(muic_data);
#endif /* CONFIG_MUIC_NOTIFIER */

	ret = com_to_open(muic_data);

	return ret;
}

static int max77804_muic_logically_detach(struct max77804_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	bool force_path_open = true;
	bool logically_notify = false;
	int ret = 0;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_MHL_MUIC:
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (new_dev == ATTACHED_DEV_DESKDOCK_VB_MUIC)
			goto out;
		break;
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		if(new_dev == ATTACHED_DEV_DESKDOCK_MUIC) {
			logically_notify = true;
			force_path_open = false;
		}
		break;
	case ATTACHED_DEV_NONE_MUIC:
		goto out;
	default:
		pr_warn("%s:%s try to attach without logically detach\n",
				MUIC_DEV_NAME, __func__);
		goto out;
	}

	pr_info("%s:%s attached(%d)!=new(%d), assume detach\n", MUIC_DEV_NAME,
			__func__, muic_data->attached_dev, new_dev);

#if defined(CONFIG_MUIC_NOTIFIER)
	if (logically_notify)
		muic_notifier_logically_detach_dev(muic_data);
	else
		muic_notifier_detach_dev(muic_data);
#endif /* CONFIG_MUIC_NOTIFIER */

	if (force_path_open)
		ret = com_to_open(muic_data);

out:
	return ret;
}

static int max77804_muic_handle_attach(struct max77804_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	bool logically_notify = false;
	int ret = 0;

	if (new_dev == muic_data->attached_dev) {
		pr_info("%s:%s Duplicated(%d), just ignore\n", MUIC_DEV_NAME,
				__func__, muic_data->attached_dev);
		return ret;
	}

	ret = max77804_muic_logically_detach(muic_data, new_dev);
	if (ret)
		pr_warn("%s:%s fail to logically detach(%d)\n", MUIC_DEV_NAME,
				__func__, ret);

	switch (new_dev) {
	case ATTACHED_DEV_OTG_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		/* Disable Charger Detection */
		max77804_muic_disable_chgdet(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = max77804_muic_attach_uart_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
#if defined(CONFIG_MUIC_WR_VB_ATTACH_BEFORE_ADC_ATTACHED)
		ret = write_vps_regs(muic_data, new_dev);
#endif /* CONFIG_MUIC_WR_VB_ATTACH_BEFORE_ADC_ATTACHED */
		new_dev = check_jig_uart_off_vb_test_type(muic_data, new_dev);
		break;
	case ATTACHED_DEV_TA_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = max77804_muic_attach_usb_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_MHL_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (muic_data->attached_dev == ATTACHED_DEV_DESKDOCK_VB_MUIC)
			logically_notify = true;
		else
			ret = com_to_audio(muic_data);
		break;
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		if (muic_data->attached_dev == ATTACHED_DEV_DESKDOCK_MUIC)
			logically_notify = true;
		else
			ret = com_to_audio(muic_data);
		break;
	default:
		pr_warn("%s:%s unsupported dev(%d)\n", MUIC_DEV_NAME, __func__,
				new_dev);
		ret = -ENODEV;
		goto out;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (logically_notify)
		ret = muic_notifier_logically_attach_dev(muic_data, new_dev);
	else
		ret = muic_notifier_attach_dev(muic_data, new_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

out:
	return ret;
}

static bool muic_check_vps_adc
			(const struct max77804_muic_vps_data *tmp_vps, u8 adc)
{
	bool ret = false;

	if (tmp_vps->adc == adc) {
		ret = true;
		goto out;
	}

	if (tmp_vps->adc == ADC_DONTCARE)
		ret = true;

out:
	pr_info("%s:%s vps(%s) adc(0x%02x) ret(%c)\n", MUIC_DEV_NAME, __func__,
						tmp_vps->vps_name, adc,
						ret ? 'T' : 'F');
	return ret;
}

static bool muic_check_vps_vbvolt
			(const struct max77804_muic_vps_data *tmp_vps, u8 vbvolt)
{
	bool ret = false;

	if (tmp_vps->vbvolt == vbvolt) {
		ret = true;
		goto out;
	}

	if (tmp_vps->vbvolt == VB_DONTCARE)
		ret = true;

out:
	pr_info("%s:%s vps(%s) vbvolt(0x%02x) ret(%c)\n", MUIC_DEV_NAME, __func__,
						tmp_vps->vps_name, vbvolt,
						ret ? 'T' : 'F');
	return ret;
}

static bool muic_check_vps_chgdetrun
			(const struct max77804_muic_vps_data *tmp_vps, u8 chgdetrun)
{
	bool ret = false;

	if (tmp_vps->chgdetrun == chgdetrun) {
		ret = true;
		goto out;
	}

	if (tmp_vps->chgdetrun == CHGDETRUN_DONTCARE)
		ret = true;

out:
	pr_info("%s:%s vps(%s) chgdetrun(0x%02x) ret(%c)\n", MUIC_DEV_NAME, __func__,
						tmp_vps->vps_name, chgdetrun,
						ret ? 'T' : 'F');
	return ret;
}

static bool muic_check_vps_chgtyp
			(const struct max77804_muic_vps_data *tmp_vps, u8 chgtyp)
{
	bool ret = false;

	if (tmp_vps->chgtyp == chgtyp) {
		ret = true;
		goto out;
	}

	if (tmp_vps->chgtyp == CHGTYP_ANY) {
		switch (chgtyp) {
		case CHGTYP_USB:
		case CHGTYP_CDP:
		case CHGTYP_DEDICATED_CHARGER:
		case CHGTYP_500MA:
		case CHGTYP_1A:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_vps->chgtyp == CHGTYP_DONTCARE)
		ret = true;

out:
	pr_info("%s:%s vps(%s) chgtyp(0x%02x) ret(%c)\n", MUIC_DEV_NAME, __func__,
						tmp_vps->vps_name, chgtyp,
						ret ? 'T' : 'F');
	return ret;
}

static void max77804_muic_detect_dev(struct max77804_muic_data *muic_data, int irq)
{
	struct i2c_client *i2c = muic_data->i2c;
	const struct max77804_muic_vps_data *tmp_vps;
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	u8 status[2];
	u8 adc1k, adcerr, adclow, adc, vbvolt, chgdetrun, chgtyp;
	int ret;
	int i;

	ret = max77804_bulk_read(i2c, MAX77804_MUIC_REG_STATUS1, 2, status);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
		return;
	}

	pr_info("%s:%s STATUS1:0x%02x, 2:0x%02x\n", MUIC_DEV_NAME, __func__,
			status[0], status[1]);

	adc1k = status[0] & STATUS1_ADC1K_MASK;
	adcerr = status[0] & STATUS1_ADCERR_MASK;
	adclow = status[0] & STATUS1_ADCLOW_MASK;
	adc = status[0] & STATUS1_ADC_MASK;
	vbvolt = status[1] & STATUS2_VBVOLT_MASK;
	chgdetrun = status[1] & STATUS2_CHGDETRUN_MASK;
	chgtyp = status[1] & STATUS2_CHGTYP_MASK;

	pr_info("%s:%s adcerr:0x%x adclow:0x%x adc:0x%x vb:0x%x chgdetrun:0x%x"
			" chgtyp:0x%x\n", MUIC_DEV_NAME, __func__, adcerr,
			adclow, adc, vbvolt, chgdetrun, chgtyp);

	/* Workaround for Factory mode.
	 * Abandon adc interrupt of approximately +-100K range
	 * if previous cable status was JIG UART BOOT OFF.
	 */
	if (muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) {
		if ((adc == (ADC_JIG_UART_OFF + 1)) ||
				(adc == (ADC_JIG_UART_OFF - 1))) {
			pr_warn("%s:%s abandon ADC\n", MUIC_DEV_NAME, __func__);
			return;
		}
	}

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		tmp_vps = &(muic_vps_table[i]);

		if (tmp_vps->adcerr != adcerr)
			continue;

		if (tmp_vps->adclow != adclow)
			continue;

		if (tmp_vps->adc1k != adc1k)
			continue;

		if (!(muic_check_vps_adc(tmp_vps, adc)))
			continue;

		if (!(muic_check_vps_vbvolt(tmp_vps, vbvolt)))
			continue;

		if (!(muic_check_vps_chgdetrun(tmp_vps, chgdetrun)))
			continue;

		if (!(muic_check_vps_chgtyp(tmp_vps, chgtyp)))
			continue;

		if (!(muic_check_support_dev(muic_data, tmp_vps->attached_dev)))
			break;

		pr_info("%s:%s vps table match found at i(%d), %s\n",
				MUIC_DEV_NAME, __func__, i, tmp_vps->vps_name);

		new_dev = tmp_vps->attached_dev;

		intr = MUIC_INTR_ATTACH;
		break;
	}

	if (intr == MUIC_INTR_ATTACH) {
		pr_info("%s:%s ATTACHED\n", MUIC_DEV_NAME, __func__);
		max77804_muic_handle_attach(muic_data, new_dev);
	} else {
		pr_info("%s:%s DETACHED\n", MUIC_DEV_NAME, __func__);
		max77804_muic_handle_detach(muic_data);
	}

	return;
}

static irqreturn_t max77804_muic_irq(int irq, void *data)
{
	struct max77804_muic_data *muic_data = data;
	pr_info("%s:%s irq:%d\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
		max77804_muic_detect_dev(muic_data, irq);
	else
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

#define REQUEST_IRQ(_irq, _dev_id, _name)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, max77804_muic_irq,	\
				IRQF_NO_SUSPEND, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
				MUIC_DEV_NAME, __func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

static int max77804_muic_irq_init(struct max77804_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->mfd_pdata && (muic_data->mfd_pdata->irq_base > 0)) {
		int irq_base = muic_data->mfd_pdata->irq_base;

		/* request MUIC IRQ */
		muic_data->irq_adcerr = irq_base + MAX77804_MUIC_IRQ_INT1_ADCERR;
		REQUEST_IRQ(muic_data->irq_adcerr, muic_data, "muic-adcerr");

		muic_data->irq_adc = irq_base + MAX77804_MUIC_IRQ_INT1_ADC;
		REQUEST_IRQ(muic_data->irq_adc, muic_data, "muic-adc");

		muic_data->irq_chgtyp = irq_base + MAX77804_MUIC_IRQ_INT2_CHGTYP;
		REQUEST_IRQ(muic_data->irq_chgtyp, muic_data, "muic-chgtyp");

		muic_data->irq_vbvolt = irq_base + MAX77804_MUIC_IRQ_INT2_VBVOLT;
		REQUEST_IRQ(muic_data->irq_vbvolt, muic_data, "muic-vbvolt");
	}

	pr_info("%s:%s adcerr(%d), adc(%d), chgtyp(%d), vbvolt(%d)\n",
			MUIC_DEV_NAME, __func__, muic_data->irq_adcerr,
			muic_data->irq_adc, muic_data->irq_chgtyp,
			muic_data->irq_vbvolt);

	return ret;
}

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s:%s IRQ(%d):%s free done\n", MUIC_DEV_NAME,	\
				__func__, _irq, _name);			\
	}								\
} while (0)

static void max77804_muic_free_irqs(struct max77804_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* free MUIC IRQ */
	FREE_IRQ(muic_data->irq_vbvolt, muic_data, "muic-vbvolt");
	FREE_IRQ(muic_data->irq_chgtyp, muic_data, "muic-chgtyp");
	FREE_IRQ(muic_data->irq_adc, muic_data, "muic-adc");
	FREE_IRQ(muic_data->irq_adcerr, muic_data, "muic-adcerr");
}

#define CHECK_GPIO(_gpio, _name)					\
do {									\
	if (!_gpio) {							\
		pr_err("%s:%s " _name " GPIO defined as 0 !\n",		\
				MUIC_DEV_NAME, __func__);		\
		WARN_ON(!_gpio);					\
		ret = -EIO;						\
		goto err_kfree;						\
	}								\
} while (0)

static void max77804_muic_init_detect(struct max77804_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);
	muic_data->is_muic_ready = true;

	max77804_muic_detect_dev(muic_data, -1);
	mutex_unlock(&muic_data->muic_mutex);
}

#if defined(CONFIG_OF)
static int of_max77804_muic_dt(struct max77804_muic_data *muic_data)
{
	struct device_node *np_muic;
	const char *prop_support_list;
	int i, j, prop_num;
	int ret = 0;

	np_muic = of_find_node_by_path("/muic");
	if (np_muic == NULL)
		return -EINVAL;

	prop_num = of_property_count_strings(np_muic, "muic,support-list");
	if (prop_num < 0) {
		pr_warn("%s:%s Cannot parse 'muic support list dt node'[%d]\n",
				MUIC_DEV_NAME, __func__, prop_num);
		ret = prop_num;
		goto err;
	}

	/* for debug */
	for (i = 0; i < prop_num; i++) {
		ret = of_property_read_string_index(np_muic, "muic,support-list", i,
							&prop_support_list);
		if (ret) {
			pr_err("%s:%s Cannot find string at [%d], ret[%d]\n",
					MUIC_DEV_NAME, __func__, i, ret);
			goto err;
		}

		pr_info("%s:%s prop_support_list[%d] is %s\n", MUIC_DEV_NAME, __func__,
				i, prop_support_list);

		for (j = 0; j < ARRAY_SIZE(muic_vps_table); j++) {
			if (!strcmp(muic_vps_table[j].vps_name, prop_support_list)) {
				muic_data->muic_support_list[(muic_vps_table[j].attached_dev)] = true;
				break;
			}
		}
	}

	/* for debug */
	for (i = 0; i < ATTACHED_DEV_NUM; i++) {
		pr_debug("%s:%s pmuic_support_list[%d] = %c\n", MUIC_DEV_NAME, __func__,
				i, (muic_data->muic_support_list[i] ? 'T' : 'F'));
	}

err:
	of_node_put(np_muic);

	return ret;
}
#endif /* CONFIG_OF */

static int max77804_muic_init_regs(struct max77804_muic_data *muic_data)
{
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* Set ADC debounce time: 25ms */
	max77804_muic_set_adcdbset(muic_data, 2);

	ret = max77804_muic_irq_init(muic_data);
	if (ret < 0) {
		pr_err("%s:%s Failed to initialize MUIC irq:%d\n", MUIC_DEV_NAME,
				__func__, ret);
		max77804_muic_free_irqs(muic_data);
	}

	return ret;
}

static int max77804_muic_probe(struct platform_device *pdev)
{
	struct max77804_dev *max77804 = dev_get_drvdata(pdev->dev.parent);
	struct max77804_platform_data *mfd_pdata = dev_get_platdata(max77804->dev);
	struct max77804_muic_data *muic_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_data = kzalloc(sizeof(struct max77804_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	if (!mfd_pdata) {
		pr_err("%s: failed to get max77804 mfd platform data\n", __func__);
		ret = -ENOMEM;
		goto err_kfree;
	}

#if defined(CONFIG_OF)
	ret = of_max77804_muic_dt(muic_data);
	if (ret < 0) {
		pr_err("%s:%s not found muic dt! ret[%d]\n",
				MUIC_DEV_NAME, __func__, ret);
	}
#endif /* CONFIG_OF */

	muic_data->dev = &pdev->dev;
	mutex_init(&muic_data->muic_mutex);
	muic_data->i2c = max77804->muic;
	muic_data->mfd_pdata = mfd_pdata;
	muic_data->pdata = &muic_pdata;
	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->is_muic_ready = false;

	platform_set_drvdata(pdev, muic_data);

	if (muic_data->pdata->init_gpio_cb) {
		ret = muic_data->pdata->init_gpio_cb(get_switch_sel());
		if (ret) {
			pr_err("%s: failed to init gpio(%d)\n", __func__, ret);
			goto fail;
		}
	}

	mutex_lock(&muic_data->muic_mutex);

	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &max77804_muic_group);
	if (ret) {
		pr_err("%s: failed to create max77804 muic attribute group\n",
				__func__);
		goto fail_sysfs_create;
	}
	dev_set_drvdata(switch_device, muic_data);

	if (muic_data->pdata->init_switch_dev_cb)
		muic_data->pdata->init_switch_dev_cb();

	ret = max77804_muic_init_regs(muic_data);
	if (ret < 0) {
		pr_err("%s:%s Failed to initialize MUIC irq:%d\n",
				MUIC_DEV_NAME, __func__, ret);
		goto fail_init_irq;
	}
	mutex_unlock(&muic_data->muic_mutex);

	/* initial cable detection */
	max77804_muic_init_detect(muic_data);

	return 0;

fail_init_irq:
	if (muic_data->pdata->cleanup_switch_dev_cb)
		muic_data->pdata->cleanup_switch_dev_cb();
	sysfs_remove_group(&switch_device->kobj, &max77804_muic_group);
fail_sysfs_create:
	mutex_unlock(&muic_data->muic_mutex);
fail:
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&muic_data->muic_mutex);
err_kfree:
	kfree(muic_data);
err_return:
	return ret;
}

static int max77804_muic_remove(struct platform_device *pdev)
{
	struct max77804_muic_data *muic_data = platform_get_drvdata(pdev);

	sysfs_remove_group(&switch_device->kobj, &max77804_muic_group);

	if (muic_data) {
		pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

		max77804_muic_free_irqs(muic_data);

		if (muic_data->pdata->cleanup_switch_dev_cb)
			muic_data->pdata->cleanup_switch_dev_cb();

		platform_set_drvdata(pdev, NULL);
		mutex_destroy(&muic_data->muic_mutex);
		kfree(muic_data);
	}

	return 0;
}

void max77804_muic_shutdown(struct device *dev)
{
	struct max77804_muic_data *muic_data = dev_get_drvdata(dev);
	struct i2c_client *i2c;
	struct max77804_dev *max77804;
	int ret;
	u8 val;

	pr_info("%s:%s +\n", MUIC_DEV_NAME, __func__);

	sysfs_remove_group(&switch_device->kobj, &max77804_muic_group);

	if (!muic_data) {
		pr_err("%s:%s no drvdata\n", MUIC_DEV_NAME, __func__);
		goto out;
	}

	max77804_muic_free_irqs(muic_data);

	i2c = muic_data->i2c;

	if (!i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		goto out_cleanup;
	}

	max77804 = i2c_get_clientdata(i2c);
	pr_info("%s:%s max77804->i2c_lock.count.counter=%d\n", MUIC_DEV_NAME,
		__func__, max77804->i2c_lock.count.counter);

	pr_info("%s:%s JIGSet: auto detection\n", MUIC_DEV_NAME, __func__);
	val = (0 << CTRL3_JIGSET_SHIFT) | (0 << CTRL3_BTLDSET_SHIFT);

	ret = max77804_muic_update_reg(i2c, MAX77804_MUIC_REG_CTRL3, val,
			CTRL3_JIGSET_MASK | CTRL3_BTLDSET_MASK, true);
	if (ret < 0)
		pr_err("%s:%s fail to update reg\n", MUIC_DEV_NAME, __func__);

out_cleanup:
	if (muic_data->pdata && muic_data->pdata->cleanup_switch_dev_cb)
		muic_data->pdata->cleanup_switch_dev_cb();

	mutex_destroy(&muic_data->muic_mutex);
	kfree(muic_data);

out:
	pr_info("%s:%s -\n", MUIC_DEV_NAME, __func__);
}

static struct platform_driver max77804_muic_driver = {
	.driver		= {
		.name	= MUIC_DEV_NAME,
		.owner	= THIS_MODULE,
		.shutdown = max77804_muic_shutdown,
	},
	.probe		= max77804_muic_probe,
	.remove		= max77804_muic_remove,
};

static int __init max77804_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return platform_driver_register(&max77804_muic_driver);
}
module_init(max77804_muic_init);

static void __exit max77804_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	platform_driver_unregister(&max77804_muic_driver);
}
module_exit(max77804_muic_exit);

MODULE_DESCRIPTION("Maxim MAX77804 MUIC driver");
MODULE_AUTHOR("<seo0.jeong@samsung.com>");
MODULE_LICENSE("GPL");
