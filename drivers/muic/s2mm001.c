/*
 * driver/misc/s2mm001-muic.c - S2MM001 micro USB switch device driver
 *
 * Copyright (C) 2010 Samsung Electronics
 * Hyoyoung Kim <hyway.kim@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <linux/muic/muic.h>
#include <linux/muic/s2mm001.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#define DEBUG_MUIC

#ifdef DEBUG_MUIC

#define MAX_LOG 25
#define READ 0
#define WRITE 1

#define GPIO_LEVEL_HIGH		1
#define GPIO_LEVEL_LOW		0

static u8 s2mm001_log_cnt;
static u8 s2mm001_log[MAX_LOG][3];

static int s2mm001_i2c_read_byte(const struct i2c_client *client, u8 command);
static int s2mm001_i2c_write_byte(const struct i2c_client *client,
			u8 command, u8 value);
static void s2mm001_muic_handle_attach(struct s2mm001_muic_data *muic_data,
			muic_attached_dev_t new_dev, int adc, u8 vbvolt);
static void s2mm001_muic_handle_detach(struct s2mm001_muic_data *muic_data);

static void s2mm001_reg_log(u8 reg, u8 value, u8 rw)
{
	s2mm001_log[s2mm001_log_cnt][0] = reg;
	s2mm001_log[s2mm001_log_cnt][1] = value;
	s2mm001_log[s2mm001_log_cnt][2] = rw;
	s2mm001_log_cnt++;
	if (s2mm001_log_cnt >= MAX_LOG)
		s2mm001_log_cnt = 0;
}
static void s2mm001_print_reg_log(void)
{
	int i;
	u8 reg, value, rw;
	char mesg[256] = "";

	for (i = 0; i < MAX_LOG; i++) {
		reg = s2mm001_log[s2mm001_log_cnt][0];
		value = s2mm001_log[s2mm001_log_cnt][1];
		rw = s2mm001_log[s2mm001_log_cnt][2];
		s2mm001_log_cnt++;

		if (s2mm001_log_cnt >= MAX_LOG)
			s2mm001_log_cnt = 0;
		sprintf(mesg+strlen(mesg), "%x(%x)%x ", reg, value, rw);
	}
	pr_info("%s:%s\n", __func__, mesg);
}
void s2mm001_read_reg_dump(struct s2mm001_muic_data *muic, char *mesg)
{
	int val;
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_CTRL);
	sprintf(mesg, "CT:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_INTMASK1);
	sprintf(mesg+strlen(mesg), "IM1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_INTMASK2);
	sprintf(mesg+strlen(mesg), "IM2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_MANSW1);
	sprintf(mesg+strlen(mesg), "MS1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_MANSW2);
	sprintf(mesg+strlen(mesg), "MS2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_ADC);
	sprintf(mesg+strlen(mesg), "ADC:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_DEV_T1);
	sprintf(mesg+strlen(mesg), "DT1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_DEV_T2);
	sprintf(mesg+strlen(mesg), "DT2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, S2MM001_MUIC_REG_DEV_T3);
	sprintf(mesg+strlen(mesg), "DT3:%x ", val);
}
void s2mm001_print_reg_dump(struct s2mm001_muic_data *muic_data)
{
	char mesg[256] = "";

	s2mm001_read_reg_dump(muic_data, mesg);

	pr_info("%s:%s\n", __func__, mesg);
}
#endif

/* don't access this variable directly!! except get_switch_sel_value function.
 * you must get switch_sel value by using get_switch_sel function. */
static int switch_sel;

/*
 * func : set_switch_sel
 * switch_sel value get from bootloader comand line
 * switch_sel data consist 4 bits
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
static int s2mm001_i2c_read_byte(const struct i2c_client *client, u8 command)
{
	int ret;
	int retry = 0;

	ret = i2c_smbus_read_byte_data(client, command);

	while (ret < 0) {
		pr_err("%s:i2c err on reading reg(0x%x), retrying ...\n",
					__func__, command);
		if (retry > 10) {
			pr_err("%s: retry count > 10 : failed !!\n", __func__);
			break;
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, command);
		retry++;
	}

#ifdef DEBUG_MUIC
	s2mm001_reg_log(command, ret, retry << 1 | READ);
#endif
	return ret;
}
static int s2mm001_i2c_write_byte(const struct i2c_client *client,
			u8 command, u8 value)
{
	int ret;
	int retry = 0;
	int written = 0;

	ret = i2c_smbus_write_byte_data(client, command, value);

	while (ret < 0) {
		written = i2c_smbus_read_byte_data(client, command);
		if (written < 0)
			pr_err("%s:i2c err on reading reg(0x%x)\n",
					__func__, command);
		msleep(100);
		ret = i2c_smbus_write_byte_data(client, command, value);
		retry++;
	}
#ifdef DEBUG_MUIC
	s2mm001_reg_log(command, value, retry << 1 | WRITE);
#endif
	return ret;
}
static int s2mm001_i2c_guaranteed_wbyte(const struct i2c_client *client,
			u8 command, u8 value)
{
	int ret;
	int retry = 0;
	int written;

	ret = s2mm001_i2c_write_byte(client, command, value);
	written = s2mm001_i2c_read_byte(client, command);

	while (written != value) {
		pr_err("%s:reg(0x%x): written(0x%x) != value(0x%x)...\n",
					__func__, command, written, value);
		if (retry > 10) {
			pr_err("%s: retry count > 10 : failed !!\n", __func__);
			break;
		}
		msleep(100);
		retry++;
		ret = s2mm001_i2c_write_byte(client, command, value);
		written = s2mm001_i2c_read_byte(client, command);
	}
	return ret;
}


#if defined(GPIO_USB_SEL)
static int s2mm001_set_gpio_usb_sel(int uart_sel)
{
	return 0;
}
#endif /* GPIO_USB_SEL */

static int s2mm001_set_gpio_uart_sel(int uart_sel)
{
	const char *mode;
	int uart_sel_gpio = muic_pdata.gpio_uart_sel;
	int uart_sel_val;
	int ret;

	ret = gpio_request(uart_sel_gpio, "GPIO_UART_SEL");
	if (ret) {
		pr_err("failed to gpio_request GPIO_UART_SEL\n");
		return ret;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	pr_info("%s: uart_sel(%d), GPIO_UART_SEL(%d)=%c ->", __func__, uart_sel,
			uart_sel_gpio, (uart_sel_val == 0 ? 'L' : 'H'));

	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_direction_output(uart_sel_gpio, GPIO_LEVEL_HIGH);
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_direction_output(uart_sel_gpio, GPIO_LEVEL_LOW);
		break;
	default:
		mode = "Error";
		break;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	gpio_free(uart_sel_gpio);

	pr_info(" %s, GPIO_UART_SEL(%d)=%c\n", mode, uart_sel_gpio,
			(uart_sel_val == 0 ? 'L' : 'H'));

	return 0;
}
#if defined(GPIO_DOC_SWITCH)
static int s2mm001_set_gpio_doc_switch(int val)
{
	int doc_switch_gpio = muic_pdata.gpio_doc_switch;
	int doc_switch_val;
	int ret;

	ret = gpio_request(doc_switch_gpio, "GPIO_UART_SEL");
	if (ret) {
		pr_err("failed to gpio_request GPIO_UART_SEL\n");
		return ret;
	}

	doc_switch_val = gpio_get_value(doc_switch_gpio);

	pr_info("%s: doc_switch(%d) --> (%d)", __func__, doc_switch_val, val);

	if (gpio_is_valid(doc_switch_gpio))
			gpio_set_value(doc_switch_gpio, val);

	doc_switch_val = gpio_get_value(doc_switch_gpio);

	gpio_free(doc_switch_gpio);

	pr_info("%s: GPIO_DOC_SWITCH(%d)=%c\n", __func__, doc_switch_gpio,
			(doc_switch_val == 0 ? 'L' : 'H'));

	return 0;
}
#endif /* GPIO_DOC_SWITCH */

static ssize_t s2mm001_muic_show_uart_en(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);

	if (!muic_data->is_rustproof) {
		pr_info("%s:%s UART ENABLE\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "1\n");
	}
	pr_info("%s:%s UART DISABLE\n", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "0\n");
}

static ssize_t s2mm001_muic_set_uart_en(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1))
		muic_data->is_rustproof = false;
	else if (!strncmp(buf, "0", 1))
		muic_data->is_rustproof = true;
	else
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);

	pr_info("%s:%s uart_en(%d)\n", MUIC_DEV_NAME, __func__,
			!muic_data->is_rustproof);

	return count;
}

static ssize_t s2mm001_muic_show_uart_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		pr_info("%s:%s AP\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "AP\n");
	case MUIC_PATH_UART_CP:
		pr_info("%s:%s CP\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "CP\n");
	default:
		break;
	}

	pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "UNKNOWN\n");
}

static int switch_to_ap_uart(struct s2mm001_muic_data *muic_data);
static int switch_to_cp_uart(struct s2mm001_muic_data *muic_data);

static ssize_t s2mm001_muic_set_uart_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
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

static ssize_t s2mm001_muic_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		return sprintf(buf, "PDA\n");
	case MUIC_PATH_USB_CP:
		return sprintf(buf, "MODEM\n");
	default:
		break;
	}

	pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t s2mm001_muic_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3))
		pdata->usb_path = MUIC_PATH_USB_AP;
	else if (!strncasecmp(buf, "MODEM", 5))
		pdata->usb_path = MUIC_PATH_USB_CP;
	else
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);

	pr_info("%s:%s usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	return count;
}

static ssize_t s2mm001_muic_show_usb_en(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);

	return sprintf(buf, "%s:%s attached_dev = %d\n",
		MUIC_DEV_NAME, __func__, muic_data->attached_dev);
}

static ssize_t s2mm001_muic_set_usb_en(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	muic_attached_dev_t new_dev = ATTACHED_DEV_USB_MUIC;

	if (!strncasecmp(buf, "1", 1))
		s2mm001_muic_handle_attach(muic_data, new_dev, 0, 0);
	else if (!strncasecmp(buf, "0", 1))
		s2mm001_muic_handle_detach(muic_data);
	else
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);

	pr_info("%s:%s attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	return count;
}

static ssize_t s2mm001_muic_show_adc(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&muic_data->muic_mutex);
	ret = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_ADC);
	mutex_unlock(&muic_data->muic_mutex);
	if (ret < 0) {
		pr_err("%s:%s err read adc reg(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", (ret & ADC_ADC_MASK));
}

static ssize_t s2mm001_muic_show_usb_state(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s muic_data->attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return 0;
}

#ifdef DEBUG_MUIC
static ssize_t s2mm001_muic_show_mansw1(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&muic_data->muic_mutex);
	ret = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_MANSW1);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("func:%s ret:%d buf%s\n", __func__, ret, buf);

	if (ret < 0) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	return sprintf(buf, "0x%x\n", ret);
}
static ssize_t s2mm001_muic_show_mansw2(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&muic_data->muic_mutex);
	ret = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_MANSW2);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("func:%s ret:%d buf%s\n", __func__, ret, buf);

	if (ret < 0) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	return sprintf(buf, "0x%x\n", ret);
}
static ssize_t s2mm001_muic_show_interrupt_status(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	int st1, st2;

	mutex_lock(&muic_data->muic_mutex);
	st1 = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_INT1);
	st2 = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_INT2);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("func:%s st1:0x%x st2:0x%x buf%s\n", __func__, st1, st2, buf);

	if (st1 < 0 || st2 < 0) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	return sprintf(buf, "st1:0x%x st2:0x%x\n", st1, st2);
}
static ssize_t s2mm001_muic_show_registers(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	char mesg[256] = "";

	mutex_lock(&muic_data->muic_mutex);
	s2mm001_read_reg_dump(muic_data, mesg);
	mutex_unlock(&muic_data->muic_mutex);
	pr_info("%s:%s\n", __func__, mesg);

	return sprintf(buf, "%s\n", mesg);
}

#endif

#if defined(CONFIG_USB_HOST_NOTIFY)
static ssize_t s2mm001_muic_show_otg_test(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;
	u8 val = 0;

	mutex_lock(&muic_data->muic_mutex);
	ret = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_INTMASK2);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("func:%s ret:%d val:%x buf%s\n", __func__, ret, val, buf);

	if (ret < 0) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	val &= INT_CHG_DET_MASK;
	return sprintf(buf, "%x\n", val);
}

static ssize_t s2mm001_muic_set_otg_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	u8 val;

	pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);
	if (!strncmp(buf, "0", 1)) {
		val = 0;
	} else if (!strncmp(buf, "1", 1)) {
		val = 1;
	} else {
		pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}
	mutex_lock(&muic_data->muic_mutex);
	val = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_INTMASK2);
	val = s2mm001_i2c_write_byte(muic_data->i2c,
			S2MM001_MUIC_REG_INTMASK2, val|INT_CHG_DET_MASK);
	mutex_unlock(&muic_data->muic_mutex);
	if (val < 0) {
		pr_err("%s:%s err writing INTMASK reg(%d)\n",
					MUIC_DEV_NAME, __func__, val);
	}

	val = 0;
	val = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_INTMASK2);
	val &= INT_CHG_DET_MASK;
	pr_info("%s: CHG_DET(0x%02x)\n", __func__, val);

	return count;
}
#endif

static ssize_t s2mm001_muic_show_attached_dev(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s attached_dev:%d\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_NONE_MUIC:
		return sprintf(buf, "No VPS\n");
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB\n");
	case ATTACHED_DEV_CDP_MUIC:
		return sprintf(buf, "CDP\n");
	case ATTACHED_DEV_OTG_MUIC:
		return sprintf(buf, "OTG\n");
	case ATTACHED_DEV_TA_MUIC:
		return sprintf(buf, "TA\n");
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		return sprintf(buf, "JIG UART OFF\n");
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		return sprintf(buf, "JIG UART OFF/VB\n");
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		return sprintf(buf, "JIG UART ON\n");
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		return sprintf(buf, "JIG USB OFF\n");
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "JIG USB ON\n");
	case ATTACHED_DEV_DESKDOCK_MUIC:
		return sprintf(buf, "DESKDOCK\n");
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		return sprintf(buf, "AUDIODOCK\n");
	default:
		break;
	}

	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t s2mm001_muic_show_audio_path(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return 0;
}

static ssize_t s2mm001_muic_set_audio_path(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return 0;
}


static ssize_t s2mm001_muic_show_apo_factory(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t s2mm001_muic_set_apo_factory(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct s2mm001_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
		muic_data->is_factory_start = true;
		mode = "FACTORY_MODE";
	} else {
		pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return count;
}

static DEVICE_ATTR(uart_en, 0664, s2mm001_muic_show_uart_en,
					s2mm001_muic_set_uart_en);
static DEVICE_ATTR(uart_sel, 0664, s2mm001_muic_show_uart_sel,
		s2mm001_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664,
		s2mm001_muic_show_usb_sel, s2mm001_muic_set_usb_sel);
static DEVICE_ATTR(adc, 0664, s2mm001_muic_show_adc, NULL);
#ifdef DEBUG_MUIC
static DEVICE_ATTR(mansw1, 0664, s2mm001_muic_show_mansw1, NULL);
static DEVICE_ATTR(mansw2, 0664, s2mm001_muic_show_mansw2, NULL);
static DEVICE_ATTR(dump_registers, 0664, s2mm001_muic_show_registers, NULL);
static DEVICE_ATTR(int_status, 0664, s2mm001_muic_show_interrupt_status, NULL);
#endif
static DEVICE_ATTR(usb_state, 0664, s2mm001_muic_show_usb_state, NULL);
#if defined(CONFIG_USB_HOST_NOTIFY)
static DEVICE_ATTR(otg_test, 0664,
		s2mm001_muic_show_otg_test, s2mm001_muic_set_otg_test);
#endif
static DEVICE_ATTR(attached_dev, 0664, s2mm001_muic_show_attached_dev, NULL);
static DEVICE_ATTR(audio_path, 0664,
		s2mm001_muic_show_audio_path, s2mm001_muic_set_audio_path);
static DEVICE_ATTR(apo_factory, 0664,
		s2mm001_muic_show_apo_factory,
		s2mm001_muic_set_apo_factory);
static DEVICE_ATTR(usb_en, 0664,
		s2mm001_muic_show_usb_en,
		s2mm001_muic_set_usb_en);

static struct attribute *s2mm001_muic_attributes[] = {
	&dev_attr_uart_en.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_adc.attr,
#ifdef DEBUG_MUIC
	&dev_attr_mansw1.attr,
	&dev_attr_mansw2.attr,
	&dev_attr_dump_registers.attr,
	&dev_attr_int_status.attr,
#endif
	&dev_attr_usb_state.attr,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&dev_attr_otg_test.attr,
#endif
	&dev_attr_attached_dev.attr,
	&dev_attr_audio_path.attr,
	&dev_attr_apo_factory.attr,
	&dev_attr_usb_en.attr,
	NULL
};

static const struct attribute_group s2mm001_muic_group = {
	.attrs = s2mm001_muic_attributes,
};

static int set_ctrl_reg(struct s2mm001_muic_data *muic_data, int shift, bool on)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val;
	int ret = 0;

	ret = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_CTRL);
	if (ret < 0)
		pr_err("%s:%s err read CTRL(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
	if (on)
		reg_val = ret | (0x1 << shift);
	else
		reg_val = ret & ~(0x1 << shift);

	if (reg_val ^ ret) {
		pr_info("%s:%s reg_val(0x%x)!=CTRL reg(0x%x), update reg\n",
				MUIC_DEV_NAME, __func__, reg_val, ret);

		ret = s2mm001_i2c_guaranteed_wbyte(i2c, S2MM001_MUIC_REG_CTRL,
				reg_val);
		if (ret < 0)
			pr_err("%s:%s err write CTRL(%d)\n", MUIC_DEV_NAME,
					__func__, ret);
	} else {
		pr_info("%s:%s reg_val(0x%x)==CTRL reg(0x%x), just return\n",
				MUIC_DEV_NAME, __func__, reg_val, ret);
		return 0;
	}

	ret = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_CTRL);
	if (ret < 0)
		pr_err("%s:%s err read CTRL(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
	else
		pr_info("%s:%s CTRL reg after change(0x%x)\n", MUIC_DEV_NAME,
				__func__, ret);

	return ret;
}

static int set_int_mask(struct s2mm001_muic_data *muic_data, bool on)
{
	int shift = CTRL_INT_MASK_SHIFT;
	int ret = 0;

	ret = set_ctrl_reg(muic_data, shift, on);

	return ret;
}

static int set_manual_sw(struct s2mm001_muic_data *muic_data, bool on)
{
	int shift = CTRL_MANUAL_SW_SHIFT;
	int ret = 0;

	ret = set_ctrl_reg(muic_data, shift, on);

	return ret;
}

static int set_com_sw(struct s2mm001_muic_data *muic_data,
			enum s2mm001_reg_manual_sw1_value reg_val)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	int temp = 0;

	temp = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_MANSW1);
	if (temp < 0)
		pr_err("%s:%s err read MANSW1(%d)\n", MUIC_DEV_NAME, __func__,
				temp);

	if (reg_val != temp) {
		pr_info("%s:%s reg_val(0x%x)!=MANSW1 reg(0x%x), update reg\n",
				MUIC_DEV_NAME, __func__, reg_val, temp);

		ret = s2mm001_i2c_guaranteed_wbyte(i2c, S2MM001_MUIC_REG_MANSW1,
				reg_val);
		if (ret < 0)
			pr_err("%s:%s err write MANSW1(%d)\n", MUIC_DEV_NAME,
					__func__, reg_val);
	} else {
		pr_info("%s:%s MANSW1 reg(0x%x), just return\n",
				MUIC_DEV_NAME, __func__, reg_val);
		return 0;
	}

	return ret;
}

#ifndef com_to_open
static int com_to_open(struct s2mm001_muic_data *muic_data)
{
	enum s2mm001_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_OPEN;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_sw err\n", MUIC_DEV_NAME, __func__);

	return ret;
}
#endif
static int com_to_open_with_vbus(struct s2mm001_muic_data *muic_data)
{
	enum s2mm001_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_OPEN_WITH_V_BUS;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_sw err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_usb(struct s2mm001_muic_data *muic_data)
{
	enum s2mm001_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_USB;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_usb err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_uart(struct s2mm001_muic_data *muic_data)
{
	enum s2mm001_reg_manual_sw1_value reg_val;
	int ret = 0;

	if (muic_data->is_rustproof) {
		pr_info("%s:%s rustproof mode : don't open uart path\n",
			MUIC_DEV_NAME, __func__);
		return ret;
	}
	reg_val = MANSW1_UART;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_uart err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_audio(struct s2mm001_muic_data *muic_data)
{
	enum s2mm001_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_AUDIO;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_audio err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int switch_to_dock_audio(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = com_to_audio(muic_data);
	if (ret) {
		pr_err("%s:%s com->audio set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_system_audio(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	return ret;
}


static int switch_to_ap_usb(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = com_to_usb(muic_data);
	if (ret) {
		pr_err("%s:%s com->usb set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_usb(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = com_to_usb(muic_data);
	if (ret) {
		pr_err("%s:%s com->usb set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_ap_uart(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->pdata->gpio_uart_sel)
		muic_data->pdata->set_gpio_uart_sel(MUIC_PATH_UART_AP);

	ret = com_to_uart(muic_data);
	if (ret) {
		pr_err("%s:%s com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_uart(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->pdata->gpio_uart_sel)
		muic_data->pdata->set_gpio_uart_sel(MUIC_PATH_UART_CP);

	ret = com_to_uart(muic_data);
	if (ret) {
		pr_err("%s:%s com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int attach_charger(struct s2mm001_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	int ret = 0;

	pr_info("%s:%s new_dev(%d)\n", MUIC_DEV_NAME, __func__, new_dev);

	muic_data->attached_dev = new_dev;

	return ret;
}

static int detach_charger(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_usb_util(struct s2mm001_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	int ret = 0;

	ret = attach_charger(muic_data, new_dev);
	if (ret)
		return ret;

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP) {
		ret = switch_to_cp_usb(muic_data);
		return ret;
	}

	ret = switch_to_ap_usb(muic_data);
	return ret;
}

static int attach_usb(struct s2mm001_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	int ret = 0;

	if (muic_data->attached_dev == new_dev) {
		pr_info("%s:%s duplicated(USB)\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, new_dev);
	if (ret)
		return ret;

	return ret;
}

static int attach_otg_usb(struct s2mm001_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	int ret = 0;

	if (muic_data->attached_dev == new_dev) {
		pr_info("%s:%s duplicated(USB)\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

#ifdef CONFIG_MUIC_S2MM001_SUPPORT_LANHUB
	/* LANHUB doesn't work under AUTO switch mode, so turn it off */
	/* set MANUAL SW mode */
	set_manual_sw(muic_data, 0);

	/* enable RAW DATA mode, only for OTG LANHUB */
	set_ctrl_reg(muic_data, CTRL_RAW_DATA_SHIFT , 0);
#endif

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP)
		ret = switch_to_cp_usb(muic_data);
	else
		ret = switch_to_ap_usb(muic_data);

	muic_data->attached_dev = new_dev;

	return ret;
}

static int detach_otg_usb(struct s2mm001_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s attached_dev type(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	ret = com_to_open(muic_data);
	if (ret)
		return ret;

#ifdef CONFIG_MUIC_S2MM001_SUPPORT_LANHUB
	/* disable RAW DATA mode */
	set_ctrl_reg(muic_data, CTRL_RAW_DATA_SHIFT , 1);

#ifdef CONFIG_MACH_DEGAS
	/* System rev less than 0.1 cannot use Auto switch mode in DEGAS */
	if (system_rev >= 0x1)
#endif
		/* set AUTO SW mode */
		set_manual_sw(muic_data, 1);
#endif

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if (pdata->usb_path == MUIC_PATH_USB_CP)
		return ret;

	return ret;
}

static int detach_usb(struct s2mm001_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s attached_dev type(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	ret = detach_charger(muic_data);
	if (ret)
		return ret;

	ret = com_to_open(muic_data);
	if (ret)
		return ret;

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if (pdata->usb_path == MUIC_PATH_USB_CP)
		return ret;

	return ret;
}

static int set_vbus_interrupt(struct s2mm001_muic_data *muic_data, int enable)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;

	if (enable) {
		ret = s2mm001_i2c_write_byte(i2c, S2MM001_MUIC_REG_INTMASK2,
			REG_INTMASK2_VBUS);
		if (ret < 0)
			pr_err("%s: err mask interrupt2(%d)\n", __func__, ret);
	} else {
		ret = s2mm001_i2c_write_byte(i2c, S2MM001_MUIC_REG_INTMASK2,
			REG_INTMASK2_VALUE);
		if (ret < 0)
			pr_err("%s: err mask interrupt2(%d)\n", __func__, ret);
	}
	return ret;
}

static int attach_deskdock(struct s2mm001_muic_data *muic_data,
			muic_attached_dev_t new_dev, u8 vbvolt)
{
	int ret = 0;

	pr_info("%s:%s vbus(%x)\n", MUIC_DEV_NAME, __func__, vbvolt);

	ret = set_vbus_interrupt(muic_data, 1);
	if (ret)
		return ret;

#ifdef CONFIG_MACH_DEGAS
	/* Audio-out doesn't work under AUTO switch mode, so turn it off */
	/* set MANUAL SW mode */
	set_manual_sw(muic_data, 0);
#endif

	ret = switch_to_dock_audio(muic_data);
	if (ret)
		return ret;

	if (vbvolt)
		ret = attach_charger(muic_data, new_dev);
	else
		ret = detach_charger(muic_data);
	if (ret)
		return ret;

	muic_data->attached_dev = new_dev;

	return ret;
}

static int detach_deskdock(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_vbus_interrupt(muic_data, 0);
	if (ret)
		return ret;

#ifdef CONFIG_MACH_DEGAS
	/* System rev less than 0.1 cannot use Auto switch mode in DEGAS */
	if (system_rev >= 0x1)
		set_manual_sw(muic_data, 1); /* set AUTO SW mode */
#endif

	ret = switch_to_system_audio(muic_data);
	if (ret)
		pr_err("%s:%s err changing audio path(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

	ret = detach_charger(muic_data);
	if (ret)
		pr_err("%s:%s err detach_charger(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_audiodock(struct s2mm001_muic_data *muic_data,
			muic_attached_dev_t new_dev, u8 vbus)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_vbus_interrupt(muic_data, 1);
	if (ret)
		return ret;

	if (!vbus) {
		ret = detach_charger(muic_data);
		if (ret)
			pr_err("%s:%s err detach_charger(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

		ret = com_to_open(muic_data);
		if (ret)
			return ret;

		muic_data->attached_dev = new_dev;

		return ret;
	}

	ret = attach_usb_util(muic_data, new_dev);
	if (ret)
		pr_info("%s:%s attach_usb_util(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

	muic_data->attached_dev = new_dev;

	return ret;
}

static int detach_audiodock(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_vbus_interrupt(muic_data, 0);
	if (ret)
		return ret;

	ret = detach_charger(muic_data);
	if (ret)
		pr_err("%s:%s err detach_charger(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

	ret = com_to_open(muic_data);
	if (ret)
		return ret;

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_jig_uart_boot_off(struct s2mm001_muic_data *muic_data,
				u8 vbvolt)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	muic_attached_dev_t new_dev;
	int ret = 0;

	pr_info("%s:%s JIG UART BOOT-OFF(0x%x)\n", MUIC_DEV_NAME, __func__,
			vbvolt);

	ret = set_vbus_interrupt(muic_data, 1);
	if (ret)
		return ret;

	if (pdata->uart_path == MUIC_PATH_UART_AP)
		ret = switch_to_ap_uart(muic_data);
	else
		ret = switch_to_cp_uart(muic_data);

	/* if VBUS is enabled, call host_notify_cb to check if it is OTGTEST*/
	if (vbvolt) {
		/* JIG_UART_OFF_VB */
		new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
		ret = attach_charger(muic_data, new_dev);
	} else {
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		ret = attach_charger(muic_data, new_dev);
	}

	return ret;
}
static int detach_jig_uart_boot_off(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_vbus_interrupt(muic_data, 0);
	if (ret)
		return ret;

	ret = detach_charger(muic_data);
	if (ret)
		pr_err("%s:%s err detach_charger(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_jig_usb_boot_off(struct s2mm001_muic_data *muic_data,
				u8 vbvolt)
{
	int ret = 0;

	if (muic_data->attached_dev == ATTACHED_DEV_JIG_USB_OFF_MUIC) {
		pr_info("%s:%s duplicated(JIG USB OFF)\n", MUIC_DEV_NAME,
				__func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, ATTACHED_DEV_JIG_USB_OFF_MUIC);
	if (ret)
		return ret;

	return ret;
}

static int attach_jig_usb_boot_on(struct s2mm001_muic_data *muic_data,
				u8 vbvolt)
{
	int ret = 0;

	if (muic_data->attached_dev == ATTACHED_DEV_JIG_USB_ON_MUIC) {
		pr_info("%s:%s duplicated(JIG USB ON)\n", MUIC_DEV_NAME,
				__func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, ATTACHED_DEV_JIG_USB_ON_MUIC);
	if (ret)
		return ret;

	return ret;
}

static void s2mm001_muic_handle_attach(struct s2mm001_muic_data *muic_data,
			muic_attached_dev_t new_dev, int adc, u8 vbvolt)
{
	int ret = 0;
	bool noti = true;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_usb(muic_data);
		}
		break;
	case ATTACHED_DEV_OTG_MUIC:
	/* OTG -> LANHUB, meaning TA is attached to LANHUB(OTG) */
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_otg_usb(muic_data);
		}
		break;

	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_audiodock(muic_data);
		}
		break;

	case ATTACHED_DEV_TA_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_charger(muic_data);
		}
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_jig_uart_boot_off(muic_data);
		}
		break;

	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_deskdock(muic_data);
		}
		break;
	default:
		noti = false;
		break;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti)
		muic_notifier_detach_attached_dev(muic_data->attached_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

	noti = true;

	switch (new_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = attach_usb(muic_data, new_dev);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		ret = attach_otg_usb(muic_data, new_dev);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = attach_audiodock(muic_data, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_TA_MUIC:
		com_to_open_with_vbus(muic_data);
		ret = attach_charger(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = attach_jig_uart_boot_off(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		/* call attach_deskdock to wake up the device */
		ret = attach_deskdock(muic_data, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		ret = attach_jig_usb_boot_off(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = attach_jig_usb_boot_on(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		ret = attach_deskdock(muic_data, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		com_to_open_with_vbus(muic_data);
		ret = attach_charger(muic_data, new_dev);
		break;
	default:
		noti = false;
		pr_warn("%s:%s unsupported dev=%d, adc=0x%x, vbus=%c\n",
				MUIC_DEV_NAME, __func__, new_dev, adc,
				(vbvolt ? 'O' : 'X'));
		break;
	}

	if (ret)
		pr_warn("%s:%s something wrong with attaching %d (ERR=%d)\n",
		MUIC_DEV_NAME, __func__, new_dev, ret);

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti)
		muic_notifier_attach_attached_dev(new_dev);
#endif /* CONFIG_MUIC_NOTIFIER */
}

static void s2mm001_muic_handle_detach(struct s2mm001_muic_data *muic_data)
{
	int ret = 0;
	bool noti = true;

	ret = com_to_open(muic_data);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = detach_usb(muic_data);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		ret = detach_otg_usb(muic_data);
		break;
	case ATTACHED_DEV_TA_MUIC:
		ret = detach_charger(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = detach_jig_uart_boot_off(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		ret = detach_deskdock(muic_data);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = detach_audiodock(muic_data);
		break;
	case ATTACHED_DEV_NONE_MUIC:
		pr_info("%s:%s duplicated(NONE)\n", MUIC_DEV_NAME, __func__);
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
		ret = detach_charger(muic_data);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	default:
		noti = false;
		pr_info("%s:%s invalid attached_dev type(%d)\n", MUIC_DEV_NAME,
			__func__, muic_data->attached_dev);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	}
	if (ret)
		pr_warn("%s:%s something wrong with detaching %d (ERR=%d)\n",
		MUIC_DEV_NAME, __func__, muic_data->attached_dev, ret);

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti)
		muic_notifier_detach_attached_dev(muic_data->attached_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

}

static void s2mm001_muic_detect_dev(struct s2mm001_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	int vbvolt = 0;
	int val1, val2, val3, adc;
	u8 chg_en, man_sw;

	val1 = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_DEV_T1);
	if (val1 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val1);

	val2 = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_DEV_T2);
	if (val2 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val2);

	val3 = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_DEV_T3);
	if (val3 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val3);

	vbvolt = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_DEV_T3);
	vbvolt &= RSVD1_VBUS;
	if (vbvolt) {
		mdelay(450);
		chg_en = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_MANSW1);
		man_sw = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_CTRL);
		pr_info("chg_en : 0x%x, man_sw : 0x%x\n", chg_en, man_sw);
		chg_en |= 0x01 << 1;
		s2mm001_i2c_write_byte(i2c, S2MM001_MUIC_REG_MANSW1, chg_en);
	}

	adc = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_ADC);
	pr_info("%s:%s dev[1:0x%x, 2:0x%x, 3:0x%x], adc:0x%x, vbvolt:0x%x\n",
		MUIC_DEV_NAME, __func__, val1, val2, val3, adc, vbvolt);

	/* Attached */
	switch (val1) {
	case DEV_TYPE1_CDP:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_CDP_MUIC;
		pr_info("%s : USB_CDP DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_USB:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_MUIC;
		pr_info("%s : USB DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_DEDICATED_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s : DEDICATED CHARGER DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_USB_OTG:
#ifdef CONFIG_MUIC_S2MM001_SUPPORT_OTG
		intr = MUIC_INTR_ATTACH;
#endif
		new_dev = ATTACHED_DEV_OTG_MUIC;
		pr_info("%s : USB_OTG DETECTED\n", MUIC_DEV_NAME);
		break;
	default:
		break;
	}

	switch (val2) {
	case DEV_TYPE2_JIG_UART_OFF:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		pr_info("%s : JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE2_JIG_USB_OFF:
		if (!vbvolt)
			break;
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		pr_info("%s : JIG_USB_OFF DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE2_JIG_USB_ON:
		if (!vbvolt)
			break;
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		pr_info("%s : JIG_USB_ON DETECTED\n", MUIC_DEV_NAME);
		break;
	default:
		break;
	}

	if (val3 & DEV_TYPE3_CHG_TYPE) {
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s : TYPE3_CHARGER DETECTED\n", MUIC_DEV_NAME);
	}

	if (val2 & DEV_TYPE2_AV || val3 & DEV_TYPE3_AV_WITH_VBUS) {
#ifdef CONFIG_MUIC_S2MM001_SUPPORT_DESKDOCK
		intr = MUIC_INTR_ATTACH;
#endif
		new_dev = ATTACHED_DEV_DESKDOCK_MUIC;
		pr_info("%s : DESKDOCK DETECTED\n", MUIC_DEV_NAME);
	}

	/* If there is no matching device found using device type registers
		use ADC to find the attached device */
	if (new_dev == ATTACHED_DEV_UNKNOWN_MUIC) {
		switch (adc) {
		case ADC_CEA936ATYPE1_CHG: /*200k ohm */
#ifdef CONFIG_MUIC_S2MM001_200K_USB
			intr = MUIC_INTR_ATTACH;
			/* This is workaournd for LG USB cable
					which has 219k ohm ID */
			new_dev = ATTACHED_DEV_USB_MUIC;
			pr_info("%s : TYPE1 CHARGER DETECTED(USB)\n",
								MUIC_DEV_NAME);
			break;
#endif
		case ADC_CEA936ATYPE2_CHG:
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_TA_MUIC;
			pr_info("%s : TYPE1/2 CHARGER DETECTED(TA)\n",
								MUIC_DEV_NAME);
			break;
		case ADC_JIG_USB_OFF: /* 255k */
			if (!vbvolt)
				break;
			if (new_dev != ATTACHED_DEV_JIG_USB_OFF_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
				pr_info("%s : ADC JIG_USB_OFF DETECTED\n",
								MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_USB_ON:
			if (!vbvolt)
				break;
			if (new_dev != ATTACHED_DEV_JIG_USB_ON_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
				pr_info("%s : ADC JIG_USB_ON DETECTED\n",
								MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_UART_OFF:
			if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
				pr_info("%s : ADC JIG_UART_OFF DETECTED\n",
								MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_UART_ON:
			/* This is the mode to wake up device
						during factory mode. */
			if (new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC
					&& muic_data->is_factory_start) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
				pr_info("%s : ADC JIG_UART_ON DETECTED\n",
								MUIC_DEV_NAME);
			}
			break;
		case ADC_SMARTDOCK: /* 0x10000 40.2K ohm */
			/* SMARTDOCK is not supported */
			/* force not to charge the device with SMARTDOCK */
			break;
		case ADC_AUDIODOCK:
#ifdef CONFIG_MUIC_S2MM001_SUPPORT_AUDIODOCK
			intr = MUIC_INTR_ATTACH;
#endif
			new_dev = ATTACHED_DEV_AUDIODOCK_MUIC;
			pr_info("%s : ADC AUDIODOCK DETECTED\n",
							MUIC_DEV_NAME);
			break;
		case ADC_OPEN:
			/* sometimes muic fails to
				catch JIG_UART_OFF detaching */
			/* double check with ADC */
			if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) {
				new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
				intr = MUIC_INTR_DETACH;
				pr_info("%s : ADC OPEN DETECTED\n",
							MUIC_DEV_NAME);
			}
			break;
		default:
			pr_warn("%s:%s unsupported ADC(0x%02x)\n",
					MUIC_DEV_NAME, __func__, adc);
			if (vbvolt) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
				pr_info("%s : UNDEFINED VB DETECTED\n",
							MUIC_DEV_NAME);
			} else {
				intr = MUIC_INTR_DETACH;
			}
			break;
		}
	}

	if (intr == MUIC_INTR_ATTACH)
		s2mm001_muic_handle_attach(muic_data, new_dev, adc, vbvolt);
	else
		s2mm001_muic_handle_detach(muic_data);
}

static int s2mm001_muic_reg_init(struct s2mm001_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret;
	int ctrl = CTRL_MASK;
	u8 chg_en;
	int val1, val2, val3, adc;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = s2mm001_i2c_write_byte(i2c, S2MM001_MUIC_REG_INTMASK1,
			REG_INTMASK1_VALUE);
	if (ret < 0)
		pr_err("%s: err mask interrupt1(%d)\n", __func__, ret);

	ret = s2mm001_i2c_write_byte(i2c, 0x2B, 0x00);

	ret = s2mm001_i2c_write_byte(i2c, S2MM001_MUIC_REG_INTMASK2,
			REG_INTMASK2_VALUE);
	if (ret < 0)
		pr_err("%s: err mask interrupt2(%d)\n", __func__, ret);

#ifdef CONFIG_MUIC_S2MM001_ENABLE_AUTOSW
	/* set AUTO SW mode */
	/* enable AUTO Switch for devices with internal battery */
#ifdef CONFIG_MACH_DEGAS
	/* System rev less than 0.1 cannot use Auto switch mode in DEGAS */
	if (system_rev >= 0x1)
#endif
		ctrl |= CTRL_MANUAL_SW_MASK;
#endif

	ret = s2mm001_i2c_guaranteed_wbyte(i2c, S2MM001_MUIC_REG_CTRL, ctrl);
	if (ret < 0)
		pr_err("%s: err write ctrl(%d)\n", __func__, ret);

	ret = s2mm001_i2c_write_byte(i2c, S2MM001_MUIC_REG_TIMING1,
						REG_TIMING1_VALUE);
	if (ret < 0)
		pr_err("%s: err write timing1(%d)\n", __func__, ret);

	val1 = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_DEV_T1);
	if (val1 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val1);

	val2 = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_DEV_T2);
	if (val2 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val2);

	val3 = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_DEV_T3);
	if (val3 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val3);

	adc = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_ADC);
	if (val3 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, adc);

	pr_info("%s:%s dev[1:0x%x, 2:0x%x, 3:0x%x], adc:0x%x\n",
		MUIC_DEV_NAME, __func__, val1, val2, val3, adc);

	if (!val1 && !val2 && !val3 && (adc == 0x1f)) {
		chg_en = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_MANSW1);
		pr_info("init, chg_en : 0x%x\n", chg_en);
		chg_en &= ~(0x01 << 1);
		s2mm001_i2c_write_byte(i2c, S2MM001_MUIC_REG_MANSW1, chg_en);
	}

	/* enable ChargePump */
	ret = s2mm001_i2c_write_byte(i2c, S2MM001_MUIC_REG_CTRL2,
						RSVD3_CHGPUMP_nEN);
	if (ret < 0)
		pr_err("%s: err write ctrl(%d)\n", __func__, ret);

	return ret;
}

static irqreturn_t s2mm001_muic_irq_thread(int irq, void *data)
{
	struct s2mm001_muic_data *muic_data = data;
	struct i2c_client *i2c = muic_data->i2c;
	int intr1, intr2;

	pr_info("%s:%s irq(%d)\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);

	/* read and clear interrupt status bits */
	intr1 = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_INT1);
	intr2 = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_INT2);

	if ((intr1 < 0) || (intr2 < 0)) {
		pr_err("%s: err read interrupt status [1:0x%x, 2:0x%x]\n",
				__func__, intr1, intr2);
		goto skip_detect_dev;
	}

	if (intr1 & INT_ATTACH_MASK) {
		int intr_tmp;
		intr_tmp = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_INT1);
		if (intr_tmp & 0x2) {
			pr_info("%s:%s attach/detach interrupt occurred\n",
				MUIC_DEV_NAME, __func__);
			intr1 &= 0xFE;
		}
		intr1 |= intr_tmp;
	}

	pr_info("%s:%s intr[1:0x%x, 2:0x%x]\n", MUIC_DEV_NAME, __func__,
			intr1, intr2);

	/* check for muic reset and recover for every interrupt occurred */
	if (intr1 & INT_OVP_EN_MASK) {
		int ctrl;
		ctrl = s2mm001_i2c_read_byte(i2c, S2MM001_MUIC_REG_CTRL);
		if (ctrl == 0x1F) {
			/* CONTROL register is reset to 1F */
			s2mm001_print_reg_log();
			s2mm001_print_reg_dump(muic_data);
			pr_err("%s: err muic could have been reseted. Initilize!!\n",
				__func__);
			s2mm001_muic_reg_init(muic_data);
			s2mm001_print_reg_dump(muic_data);

			/* MUIC Interrupt On */
			set_int_mask(muic_data, false);
		}

		if ((intr1 & INT_ATTACH_MASK) == 0)
			goto skip_detect_dev;
	}

	/* device detection */
	s2mm001_muic_detect_dev(muic_data);

skip_detect_dev:
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static void s2mm001_muic_init_detect(struct work_struct *work)
{
	struct s2mm001_muic_data *muic_data =
		container_of(work, struct s2mm001_muic_data, init_work.work);

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* MUIC Interrupt On */
	set_int_mask(muic_data, false);

	s2mm001_muic_irq_thread(-1, muic_data);
}

static int s2mm001_init_rev_info(struct s2mm001_muic_data *muic_data)
{
	u8 dev_id;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	dev_id = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_DEVID);
	if (dev_id < 0) {
		pr_err("%s:%s i2c io error(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
		ret = -ENODEV;
	} else {
		muic_data->muic_vendor = (dev_id & 0x7);
		muic_data->muic_version = ((dev_id & 0xF8) >> 3);
		pr_info("%s:%s device found: vendor=0x%x, ver=0x%x\n",
				MUIC_DEV_NAME, __func__, muic_data->muic_vendor,
				muic_data->muic_version);
	}

	return ret;
}

static int s2mm001_muic_irq_init(struct s2mm001_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!pdata->irq_gpio) {
		pr_warn("%s:%s No interrupt specified\n", MUIC_DEV_NAME,
				__func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(pdata->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
				s2mm001_muic_irq_thread,
				(IRQF_TRIGGER_FALLING |
				IRQF_NO_SUSPEND | IRQF_ONESHOT),
				"s2mm001-muic", muic_data);
		if (ret < 0) {
			pr_err("%s:%s failed to reqeust IRQ(%d)\n",
					MUIC_DEV_NAME, __func__, i2c->irq);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			pr_err("%s:%s failed to enable wakeup src\n",
					MUIC_DEV_NAME, __func__);
	}

	return ret;
}

#if defined(CONFIG_OF)
static int of_s2mm001_muic_dt(struct device *dev,
			struct s2mm001_muic_data *muic_data)
{
	struct device_node *np_muic = dev->of_node;
	int ret = 0;

	if (np_muic == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -EINVAL;
	} else {
		muic_data->pdata->irq_gpio = of_get_named_gpio(np_muic,
							"muic,muic_int", 0);
		if (muic_data->pdata->irq_gpio < 0) {
			pr_err("%s error reading muic_irq = %d\n", __func__,
						muic_data->pdata->irq_gpio);
			muic_data->pdata->irq_gpio = 0;
		}
		if (of_gpio_count(np_muic) < 1) {
			dev_err(muic_data->dev, "could not find muic gpio\n");
			muic_data->pdata->gpio_uart_sel = 0;
		} else {
			muic_data->pdata->gpio_uart_sel =
					of_get_gpio(np_muic, 0);
		}
	}

	return ret;
}
#endif /* CONFIG_OF */

static int s2mm001_muic_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct s2mm001_muic_data *muic_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_return;
	}

	muic_data = kzalloc(sizeof(struct s2mm001_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	/* save platfom data for gpio control functions */
	muic_data->pdata = &muic_pdata;
	muic_data->dev = &i2c->dev;
	muic_data->i2c = i2c;

#if defined(CONFIG_OF)
	ret = of_s2mm001_muic_dt(&i2c->dev, muic_data);
	if (ret < 0) {
		pr_err("%s:%s not found muic dt! ret[%d]\n",
				MUIC_DEV_NAME, __func__, ret);
	}
#endif /* CONFIG_OF */

	muic_data->i2c = i2c;
	i2c_set_clientdata(i2c, muic_data);

	if (muic_pdata.gpio_uart_sel)
		muic_pdata.set_gpio_uart_sel = s2mm001_set_gpio_uart_sel;

	if (muic_data->pdata->init_gpio_cb)
		ret = muic_data->pdata->init_gpio_cb(get_switch_sel());
	if (ret) {
		pr_err("%s: failed to init gpio(%d)\n", __func__, ret);
		goto fail_init_gpio;
	}

	mutex_init(&muic_data->muic_mutex);

	muic_data->is_factory_start = false;
#if 0
	wake_lock_init(&muic_data->muic_wake_lock, WAKE_LOCK_SUSPEND,
		"muic wake lock");
#endif

	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->is_usb_ready = false;

#ifdef CONFIG_SEC_SYSFS
	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &s2mm001_muic_group);
	if (ret) {
		pr_err("%s: failed to create s2mm001 muic attribute group\n",
			__func__);
		goto fail;
	}
	dev_set_drvdata(switch_device, muic_data);
#endif

	ret = s2mm001_init_rev_info(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic rev register(%d)\n", __func__,
			ret);
		goto fail;
	}

	ret = s2mm001_muic_reg_init(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic register(%d)\n", __func__, ret);
		goto fail;
	}

	ret = s2mm001_i2c_read_byte(muic_data->i2c, S2MM001_MUIC_REG_MANSW1);
	if (ret < 0)
		pr_err("%s: err mansw1 (%d)\n", __func__, ret);

	/* RUSTPROOF : disable UART connection if MANSW1
		from BL is OPEN_RUSTPROOF */
	if (ret == MANSW1_OPEN_RUSTPROOF) {
		muic_data->is_rustproof = true;
		com_to_open_with_vbus(muic_data);
	} else {
		muic_data->is_rustproof = false;
	}

	if (muic_data->pdata->init_switch_dev_cb)
		muic_data->pdata->init_switch_dev_cb();

	ret = s2mm001_muic_irq_init(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	/* initial cable detection */
	INIT_DELAYED_WORK(&muic_data->init_work, s2mm001_muic_init_detect);
	schedule_delayed_work(&muic_data->init_work, msecs_to_jiffies(6000));

	return 0;

fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, muic_data);
fail:
#ifdef CONFIG_SEC_SYSFS
	sysfs_remove_group(&switch_device->kobj, &s2mm001_muic_group);
#endif
	mutex_destroy(&muic_data->muic_mutex);
fail_init_gpio:
	i2c_set_clientdata(i2c, NULL);
	kfree(muic_data);
err_return:
	return ret;
}

static int s2mm001_muic_remove(struct i2c_client *i2c)
{
	struct s2mm001_muic_data *muic_data = i2c_get_clientdata(i2c);
#ifdef CONFIG_SEC_SYSFS
	sysfs_remove_group(&switch_device->kobj, &s2mm001_muic_group);
#endif

	if (muic_data) {
		pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
		cancel_delayed_work(&muic_data->init_work);
		cancel_delayed_work(&muic_data->usb_work);
		disable_irq_wake(muic_data->i2c->irq);
		free_irq(muic_data->i2c->irq, muic_data);
#if 0
		wake_lock_destroy(&info->muic_wake_lock);
#endif
		mutex_destroy(&muic_data->muic_mutex);
		i2c_set_clientdata(muic_data->i2c, NULL);
		kfree(muic_data);
	}
	return 0;
}

static const struct i2c_device_id s2mm001_i2c_id[] = {
	{ MUIC_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, s2mm001_i2c_id);

static void s2mm001_muic_shutdown(struct i2c_client *i2c)
{
	struct s2mm001_muic_data *muic_data = i2c_get_clientdata(i2c);
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	if (!muic_data->i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	pr_info("%s:%s open D+,D-,V_bus line\n", MUIC_DEV_NAME, __func__);
	ret = com_to_open(muic_data);
	if (ret < 0)
		pr_err("%s:%s fail to open mansw1 reg\n", MUIC_DEV_NAME,
				__func__);

	/* set auto sw mode before shutdown to make sure device goes into */
	/* LPM charging when TA or USB is connected during off state */
	pr_info("%s:%s muic auto detection enable\n", MUIC_DEV_NAME, __func__);
	ret = set_manual_sw(muic_data, true);
	if (ret < 0) {
		pr_err("%s:%s fail to update reg\n", MUIC_DEV_NAME, __func__);
		return;
	}
}

#if defined(CONFIG_OF)
static struct of_device_id sec_muic_i2c_dt_ids[] = {
	{ .compatible = "sec-muic,i2c" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_fuelgauge_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2mm001_muic_driver = {
	.driver		= {
		.name	= MUIC_DEV_NAME,
#if defined(CONFIG_OF)
		.of_match_table	= sec_muic_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= s2mm001_muic_probe,
	.remove		= __devexit_p(s2mm001_muic_remove),
	.shutdown	= s2mm001_muic_shutdown,
	.id_table	= s2mm001_i2c_id,
};

static int __init s2mm001_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return i2c_add_driver(&s2mm001_muic_driver);
}
module_init(s2mm001_muic_init);

static void __exit s2mm001_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	i2c_del_driver(&s2mm001_muic_driver);
}
module_exit(s2mm001_muic_exit);

MODULE_AUTHOR("Hyoyoung Kim <hyway.kim@samsung.com>");
MODULE_DESCRIPTION("S2MM001 USB Switch driver");
MODULE_LICENSE("GPL");
