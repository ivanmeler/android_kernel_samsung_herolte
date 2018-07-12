/*
 * muic_regmap_max77849.c
 *
 * Copyright (C) 2014 Samsung Electronics
 * N S SABIN <n.sabin@samsung.com>
 * THOMAS RYU <smilesr.ryu@samsung.com>
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
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>
#include <linux/string.h>

#include <linux/muic/muic.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include "muic-internal.h"
#include "muic_i2c.h"
#include "muic_regmap.h"

/* Initialization value : 10_1_1_00_10 */
#define	INIT_CTRL4_VALUE (0xB2)

/* max77849 I2C registers */
enum max77849_muic_reg {
	REG_DEVID	= 0x00,
	REG_INT1	= 0x01,
	REG_INT2	= 0x02,
	REG_STATUS1 = 0x04,
	REG_STATUS2 = 0x05,
	REG_INTMASK1	= 0x07,
	REG_INTMASK2	= 0x08,
	REG_CGETCTRL1	= 0x0A,
	REG_CGETCTRL2	= 0x0B,
	REG_CTRL1	= 0x0C,
	REG_CTRL2	= 0x0D,
	REG_CTRL3	= 0x0E,
	REG_CTRL4	= 0x16,
	REG_END,
};

#define REG_ITEM(addr, bitp, mask) ((bitp<<16) | (mask<<8) | addr)

/* Field */
enum max77849_muic_reg_item {
	DEVID_DeviceID = REG_ITEM(REG_DEVID, _BIT3, _MASK5),
	DEVID_VendorID = REG_ITEM(REG_DEVID, _BIT0, _MASK3),

	INT1_ADC1K	= REG_ITEM(REG_INT1, _BIT3, _MASK1),
	INT1_ADCERROR	= REG_ITEM(REG_INT1, _BIT2, _MASK1),
	INT1_ADC	= REG_ITEM(REG_INT1, _BIT0, _MASK1),

	INT2_VBVOLT  = REG_ITEM(REG_INT2, _BIT4, _MASK1),
	INT2_DxOVP = REG_ITEM(REG_INT2, _BIT3, _MASK1),
	INT2_DCDTMR         = REG_ITEM(REG_INT2, _BIT2, _MASK1),
	INT2_CHGDETRUN     = REG_ITEM(REG_INT2, _BIT1, _MASK1),
	INT2_CHGTYP    = REG_ITEM(REG_INT2, _BIT0, _MASK1),

	STATUS1_ADC1K  = REG_ITEM(REG_STATUS1, _BIT7, _MASK1),
	STATUS1_ADCERROR  = REG_ITEM(REG_STATUS1, _BIT6, _MASK1),
	STATUS1_ADC  = REG_ITEM(REG_STATUS1, _BIT0, _MASK5),

	STATUS2_VBVOLT  = REG_ITEM(REG_STATUS2, _BIT6, _MASK1),
	STATUS2_DxOVP  = REG_ITEM(REG_STATUS2, _BIT5, _MASK1),
	STATUS2_DCDTMR  = REG_ITEM(REG_STATUS2, _BIT4, _MASK1),
	STATUS2_CHGDETRUN  = REG_ITEM(REG_STATUS2, _BIT3, _MASK1),
	STATUS2_CHGTYP  = REG_ITEM(REG_STATUS2, _BIT0, _MASK3),

	INTMASK1_ADC1K_M   = REG_ITEM(REG_INTMASK1, _BIT3, _MASK1),
	INTMASK1_ADCERROR_M = REG_ITEM(REG_INTMASK1, _BIT2, _MASK1),
	INTMASK1_ADC_M    = REG_ITEM(REG_INTMASK1, _BIT0, _MASK1),

	INTMASK2_VBVOLT_M = REG_ITEM(REG_INTMASK2, _BIT4, _MASK1),
	INTMASK2_DxOVP_M = REG_ITEM(REG_INTMASK2, _BIT3, _MASK1),
	INTMASK2_DCDTMR_M        = REG_ITEM(REG_INTMASK2, _BIT2, _MASK1),
	INTMASK2_ChgDETRUN_M    = REG_ITEM(REG_INTMASK2, _BIT1, _MASK1),
	INTMASK2_CHGTYP_M   = REG_ITEM(REG_INTMASK2, _BIT0, _MASK1),

	CDETCTRL1_CDPDET  =  REG_ITEM(REG_CGETCTRL1, _BIT7, _MASK1),
	CDETCTRL1_DCDCPL  =  REG_ITEM(REG_CGETCTRL1, _BIT5, _MASK1),
	CDETCTRL1_CDDELAY  =  REG_ITEM(REG_CGETCTRL1, _BIT4, _MASK1),
	CDETCTRL1_DCD2SCT =  REG_ITEM(REG_CGETCTRL1, _BIT3, _MASK1),
	CDETCTRL1_DCDEN =  REG_ITEM(REG_CGETCTRL1, _BIT2, _MASK1),
	CDETCTRL1_CHGTYPMAN =  REG_ITEM(REG_CGETCTRL1, _BIT1, _MASK1),
	CDETCTRL1_CHGDETEN =  REG_ITEM(REG_CGETCTRL1, _BIT0, _MASK1),

	CDETCTRL2_DxOVPEN =  REG_ITEM(REG_CGETCTRL2, _BIT3, _MASK1),
	CDETCTRL2_FRCCHG =  REG_ITEM(REG_CGETCTRL2, _BIT0, _MASK1),

	CTRL1_COMP2SW    =  REG_ITEM(REG_CTRL1, _BIT3, _MASK3),
	CTRL1_COMN1SW    =  REG_ITEM(REG_CTRL1, _BIT0, _MASK3),

	CTRL2_RCPS   =  REG_ITEM(REG_CTRL2, _BIT7, _MASK1),
	CTRL2_USBCPLNT   =  REG_ITEM(REG_CTRL2, _BIT6, _MASK1),
	CTRL2_ACCDET   =  REG_ITEM(REG_CTRL2, _BIT5, _MASK1),
	CTRL2_SFOUTORD   =  REG_ITEM(REG_CTRL2, _BIT4, _MASK1),
	CTRL2_SFOUTASRT   =  REG_ITEM(REG_CTRL2, _BIT3, _MASK1),
	CTRL2_CPEN   =  REG_ITEM(REG_CTRL2, _BIT2, _MASK1),
	CTRL2_ADCEN   =  REG_ITEM(REG_CTRL2, _BIT1, _MASK1),
	CTRL2_ADCLOWPWR   =  REG_ITEM(REG_CTRL2, _BIT0, _MASK1),

	CTRL3_JIGSET   =  REG_ITEM(REG_CTRL3, _BIT0, _MASK2),

	CTRL4_ADCMODE   =  REG_ITEM(REG_CTRL4, _BIT6, _MASK2),
	CTRL4_FCTAUTO   =  REG_ITEM(REG_CTRL4, _BIT5, _MASK1),
	CTRL4_USBAUTO   =  REG_ITEM(REG_CTRL4, _BIT4, _MASK1),
	CTRL4_ADCDbSET   =  REG_ITEM(REG_CTRL4, _BIT0, _MASK2),
};

struct reg_value_set {
	int value;
	char *alias;
};

/* ADC Scan Mode Values : b'67 */
static struct reg_value_set max77849_adc_scanmode_tbl[] = {
	[ADC_SCANMODE_CONTINUOUS] = {0x00, "Always ON"},
	[ADC_SCANMODE_ONESHOT]    = {0x02, "One Shot + Low lp Disconnect Detect"},
	[ADC_SCANMODE_PULSE]      = {0x03, "2s Pulse + Low lp Disconnect Detect"},
};

/*
 * Manual Switch
 * D- [5:3] / D+ [2:0] / Resrv [7:6]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO / 100 : USB_CP / 101 : UART_CP
 */
enum {
	_ID_OPEN	= 0x0,
	_ID_BYPASS      = 0x1,
	_NO_BC_COMP_OFF	= 0x0,
	_NO_BC_COMP_ON	= 0x1,
	_D_OPEN	        = 0x0,
	_D_USB	        = 0x1,
	_D_AUDIO	= 0x2,
	_D_UART	        = 0x3,
	_D_USB_CP	= 0x4,
	_D_UART_CP      = 0x5,
};

/* COM patch Values */
#define COM_VALUE(dm, open, comp_onoff) ((dm<<3)|(dm<<0)|(open<<7)| \
					(comp_onoff<<6))

#define _COM_OPEN		COM_VALUE(_D_OPEN, _ID_OPEN, _NO_BC_COMP_OFF)
#define _COM_OPEN_WITH_V_BUS	_COM_OPEN
#define _COM_UART_AP		COM_VALUE(_D_UART, _ID_OPEN, _NO_BC_COMP_OFF)
#define _COM_UART_CP		COM_VALUE(_D_UART_CP, _ID_OPEN, _NO_BC_COMP_OFF)
#define _COM_USB_CP		COM_VALUE(_D_USB_CP, _ID_OPEN, _NO_BC_COMP_OFF)
#define _COM_USB_AP		COM_VALUE(_D_USB, _ID_OPEN, _NO_BC_COMP_OFF)
#define _COM_AUDIO		COM_VALUE(_D_AUDIO, _ID_OPEN, _NO_BC_COMP_OFF)

static int max77849_com_value_tbl[] = {
	[COM_OPEN]		= _COM_OPEN,
	[COM_OPEN_WITH_V_BUS]	= _COM_OPEN_WITH_V_BUS,
	[COM_UART_AP]		= _COM_UART_AP,
	[COM_UART_CP]		= _COM_UART_CP,
	[COM_USB_AP]		= _COM_USB_AP,
	[COM_USB_CP]		= _COM_USB_CP,
	[COM_AUDIO]		= _COM_AUDIO,
};

static regmap_t max77849_muic_regmap_table[] = {
	[REG_DEVID]	= {"DeviceID",  0x9D, 0x00, INIT_NONE},
	[REG_INT1]	= {"INT1",	  0x00, 0x00, INIT_INT_CLR,},
	[REG_INT2]	= {"INT2",	  0x00, 0x00, INIT_INT_CLR,},
	[REG_STATUS1]	= {"STATUS1",	  0x3F, 0x00, INIT_NONE,},
	[REG_STATUS2]	= {"STATUS2",	  0x00, 0x00, INIT_NONE,},
	[REG_INTMASK1]	= {"INTMASK1",  0x00, 0x00, INIT_NONE,},
	[REG_INTMASK2]	= {"INTMASK2",  0x00, 0x00, INIT_NONE,},
	[REG_CGETCTRL1]	= {"CGETCTRL1", 0x2D, 0x00, INIT_NONE,},
	[REG_CGETCTRL2]	= {"CGETCTRL2", 0x0C, 0x00, INIT_NONE,},
	[REG_CTRL1]	= {"CONTROL1",  0x00, 0x00, INIT_NONE,},
	[REG_CTRL2]	= {"CONTROL2",  0x33, 0x00, INIT_NONE,},
	[REG_CTRL3]	= {"CONTROL3",  0x00, 0x00, INIT_NONE,},
	[REG_CTRL4]	= {"CONTROL4",  0xB2, 0x00, INIT_CTRL4_VALUE,},
	[REG_END]	= {NULL, 0, 0, INIT_NONE},
};

static int max77849_muic_ioctl(struct regmap_desc *pdesc,
		int arg1, int *arg2, int *arg3)
{
	int ret = 0;

	switch (arg1) {
	case GET_COM_VAL:
		*arg2 = max77849_com_value_tbl[*arg2];
		*arg3 = REG_CTRL1;
		break;
	case GET_ADC:
		*arg3 = STATUS1_ADC;
		break;
	case GET_REVISION:
		*arg3 = DEVID_VendorID;
		break;
	case GET_OTG_STATUS:
		*arg3 = INTMASK2_VBVOLT_M;
		break;

	case GET_CHGTYPE:
		*arg3 = STATUS2_CHGTYP;
		break;

	default:
		ret = -1;
		break;
	}

	if (pdesc->trace) {
		pr_info("%s: ret:%d arg1:%x,", __func__, ret, arg1);

		if (arg2)
			pr_info(" arg2:%x,", *arg2);

		if (arg3)
			pr_info(" arg3:%x - %s", *arg3,
				regmap_to_name(pdesc, _ATTR_ADDR(*arg3)));
		pr_info("\n");
	}

	return ret;
}
static int max77849_attach_ta(struct regmap_desc *pdesc)
{
	int attr, value, ret;

	pr_info("%s\n", __func__);

	attr = REG_CTRL1 | _ATTR_OVERWRITE_M;
	value = _COM_OPEN;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s CNTR1 reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	return ret;
}

static int max77849_detach_ta(struct regmap_desc *pdesc)
{
	int attr, value, ret;

	pr_info("%s\n", __func__);

	attr = REG_CTRL1 | _ATTR_OVERWRITE_M;
	value = _COM_OPEN;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s CNTR1 reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	return ret;
}

static int max77849_set_rustproof(struct regmap_desc *pdesc, int op)
{
	pr_info("%s: Not implemented.\n", __func__);

	return 0;
}

static int max77849_get_vps_data(struct regmap_desc *pdesc, void *pbuf)
{
	vps_data_t *pvps = (vps_data_t *)pbuf;
	int attr;

	pr_info("%s\n", __func__);

	attr = STATUS1_ADC1K;
	*(u8 *)&pvps->t.adc1k = regmap_read_value(pdesc, attr);

	attr = STATUS1_ADCERROR;
	*(u8 *)&pvps->t.adcerr = regmap_read_value(pdesc, attr);

	attr = STATUS1_ADC;
	*(u8 *)&pvps->t.adc = regmap_read_value(pdesc, attr);

	attr = STATUS2_VBVOLT;
	*(u8 *)&pvps->t.vbvolt = regmap_read_value(pdesc, attr);

	attr = STATUS2_CHGDETRUN;
	*(u8 *)&pvps->t.chgdetrun = regmap_read_value(pdesc, attr);

	attr = STATUS2_CHGTYP;
	*(u8 *)&pvps->t.chgtyp = regmap_read_value(pdesc, attr);

	return 0;
}

/* To be deprecated by another functions later. */
static int max77849_enable_accdet(struct regmap_desc *pdesc)
{
	pr_info("%s: Not implemented.\n", __func__);
	return 0;
}

static int max77849_disable_accdet(struct regmap_desc *pdesc)
{
	pr_info("%s: Not implemented.\n", __func__);
	return 0;
}

/*
static int max77849_disable_chgdet(struct regmap_desc *pdesc)
{
	pr_info("%s: Not implemented.\n", __func__);
	return 0;
}

static int max77849_enable_chgdet(struct regmap_desc *pdesc)
{
	pr_info("%s: Not implemented.\n", __func__);
	return 0;
}
*/

static int max77849_get_adc_scan_mode(struct regmap_desc *pdesc)
{
	struct reg_value_set *pvset;
	int attr, value, mode = 0;

	attr = CTRL4_ADCMODE;
	value = regmap_read_value(pdesc, attr);

	for (; mode < ARRAY_SIZE(max77849_adc_scanmode_tbl); mode++) {
		pvset = &max77849_adc_scanmode_tbl[mode];
		if (pvset->value == value)
			break;
	}

	pr_info("%s: [%2d]=%02x,%02x\n", __func__, mode, value, pvset->value);
	pr_info("%s:       %s\n", __func__, pvset->alias);

	return mode;
}

static void max77849_set_adc_scan_mode(struct regmap_desc *pdesc,
		const int mode)
{
	struct reg_value_set *pvset;
	int attr, ret, value;

	if (mode > ADC_SCANMODE_PULSE) {
		pr_err("%s Out of range(%d).\n", __func__, mode);
		return;
	}

	pvset = &max77849_adc_scanmode_tbl[mode];
	pr_info("%s: [%2d] %s\n", __func__, mode, pvset->alias);

	attr = CTRL4_ADCMODE;
	value = pvset->value;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s ADCMODE reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

}

static int max77849_get_switching_mode(struct regmap_desc *pdesc)
{
	int mode = 0;

	pr_warn("%s: Not implemented.\n", __func__);
	return mode;
}
static void max77849_set_switching_mode(struct regmap_desc *pdesc, int mode)
{
	pr_warn("%s: Not implemented.\n", __func__);
	return;
}

static void max77849_get_fromatted_dump(struct regmap_desc *pdesc, char *mesg)
{
	muic_data_t *muic = pdesc->muic;
	int val;

	if (pdesc->trace)
		pr_info("%s\n", __func__);

	val = i2c_smbus_read_byte_data(muic->i2c, REG_CTRL1);
	sprintf(mesg, "CT1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK1);
	sprintf(mesg+strlen(mesg), "IM1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK2);
	sprintf(mesg+strlen(mesg), "IM2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CGETCTRL1);
	sprintf(mesg+strlen(mesg), "CDT1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CGETCTRL2);
	sprintf(mesg+strlen(mesg), "CDT2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_STATUS1);
	sprintf(mesg+strlen(mesg), "ST1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_STATUS2);
	sprintf(mesg+strlen(mesg), "ST2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CTRL2);
	sprintf(mesg+strlen(mesg), "CT2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CTRL3);
	sprintf(mesg+strlen(mesg), "CT3:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CTRL4);
	sprintf(mesg+strlen(mesg), "CT4:%x", val);
}

static int max77849_get_sizeof_regmap(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return (int)ARRAY_SIZE(max77849_muic_regmap_table);
}

/* Need to implement */
void _max77849_muic_read_register(struct i2c_client *i2c)
{
	pr_info("%s: Not implemented.\n", __func__);
}

int max77849_muic_read_register(struct i2c_client *i2c)
	__attribute__((weak, alias("_max77849_muic_read_register")));

static struct regmap_ops max77849_muic_regmap_ops = {
	.get_size = max77849_get_sizeof_regmap,
	.ioctl = max77849_muic_ioctl,
	.get_formatted_dump = max77849_get_fromatted_dump,
};

static struct vendor_ops max77849_muic_vendor_ops = {
	.attach_ta = max77849_attach_ta,
	.detach_ta = max77849_detach_ta,
	.get_switch = max77849_get_switching_mode,
	.set_switch = max77849_set_switching_mode,
	.set_adc_scan_mode = max77849_set_adc_scan_mode,
	.get_adc_scan_mode =  max77849_get_adc_scan_mode,
	.set_rustproof = max77849_set_rustproof,
	.enable_accdet = max77849_enable_accdet,
	.disable_accdet = max77849_disable_accdet,
	.get_vps_data = max77849_get_vps_data,
};

static struct regmap_desc max77849_muic_regmap_desc = {
	.name = "max77849-MUIC",
	.regmap = max77849_muic_regmap_table,
	.size = REG_END,
	.regmapops = &max77849_muic_regmap_ops,
	.vendorops = &max77849_muic_vendor_ops,
};

void muic_register_max77849_regmap_desc(struct regmap_desc **pdesc)
{
	*pdesc = &max77849_muic_regmap_desc;
}
