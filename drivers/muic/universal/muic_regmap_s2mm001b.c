/*
 * muic_regmap_s2mm001.c
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

#define ADC_DETECT_TIME_200MS (0x03)
#define KEY_PRESS_TIME_100MS  (0x00)

/* S2MM001 Timing Set 1 & 2 register Timing table */
#define OCP_TIME_DELAY_1MS		(0x00)
#define OCP_TIME_DELAY_2MS		(0x01)
#define OCP_TIME_DELAY_4MS		(0x02)
#define OCP_TIME_DELAY_8MS		(0x03)
#define OCP_TIME_DELAY_12MS		(0x04)
#define OCP_TIME_DELAY_16MS		(0x05)

#define KEY_PRESS_TIME_100MS		(0x00)
#define KEY_PRESS_TIME_200MS		(0x10)
#define KEY_PRESS_TIME_300MS		(0x20)
#define KEY_PRESS_TIME_700MS		(0x60)


#define LONGKEY_PRESS_TIME_300MS	(0x00)
#define LONGKEY_PRESS_TIME_500MS	(0x02)
#define LONGKEY_PRESS_TIME_1000MS	(0x07)
#define LONGKEY_PRESS_TIME_1500MS	(0x0C)

#define SWITCHING_WAIT_TIME_10MS	(0x00)
#define SWITCHING_WAIT_TIME_210MS	(0xa0)

enum s2mm001_muic_reg_init_value {
	REG_INTMASK1_VALUE	= (0xDC),
	REG_INTMASK2_VALUE	= (0x20),
	REG_INTMASK2_VBUS	= (0x02),
	REG_TIMING1_VALUE	= (OCP_TIME_DELAY_4MS |
				KEY_PRESS_TIME_200MS),
};

/* s2mm001 I2C registers */
enum s2mm001_muic_reg {
	REG_DEVID	= 0x01,
	REG_CTRL	= 0x02,
	REG_INT1	= 0x03,
	REG_INT2	= 0x04,
	REG_INTMASK1	= 0x05,
	REG_INTMASK2	= 0x06,
	REG_ADC		= 0x07,
	REG_TIMING1	= 0x08,
	REG_TIMING2	= 0x09,
	/* unused registers */
	REG_DEVT1	= 0x0a,
	REG_DEVT2	= 0x0b,

	REG_BTN1	= 0x0c,
	REG_BTN2	= 0x0d,
	REG_MUIC_STATUS	= 0x0e,

	REG_MANSW1	= 0x13,
	REG_MANSW2	= 0x14,
	REG_DEVT3	= 0x15,
	REG_RESET	= 0x1B,
	REG_TIMING3	= 0x20,
	REG_OCP_SET	= 0x22,
	REG_MUIC_CTRL	= 0x23,
	REG_END,
};

#define REG_ITEM(addr, bitp, mask) ((bitp<<16) | (mask<<8) | addr)

/* Field */
enum s2mm001_muic_reg_item {
	DEVID_VERSION = REG_ITEM(REG_DEVID, _BIT3, _MASK5),
	DEVID_VendorID = REG_ITEM(REG_DEVID, _BIT0, _MASK3),

	CTRL_SW_OPEN	= REG_ITEM(REG_CTRL, _BIT4, _MASK1),
	CTRL_RAWDATA	= REG_ITEM(REG_CTRL, _BIT3, _MASK1),
	CTRL_ManualSW	= REG_ITEM(REG_CTRL, _BIT2, _MASK1),
	CTRL_WAIT	= REG_ITEM(REG_CTRL, _BIT1, _MASK1),
	CTRL_MASK_INT	= REG_ITEM(REG_CTRL, _BIT0, _MASK1),

	INT1_OVP_OCP_DIS	= REG_ITEM(REG_INT1, _BIT7, _MASK1),
	INT1_OCP_EVENT		= REG_ITEM(REG_INT1, _BIT6, _MASK1),
	INT1_OVP_EVENT		= REG_ITEM(REG_INT1, _BIT5, _MASK1),
	INT1_DETACH		= REG_ITEM(REG_INT1, _BIT1, _MASK1),
	INT1_ATTACH		= REG_ITEM(REG_INT1, _BIT0, _MASK1),

	INT2_RID_CHARGER = REG_ITEM(REG_INT2, _BIT6, _MASK1),
	INT2_MHL         = REG_ITEM(REG_INT2, _BIT5, _MASK1),
	INT2_ADC_CHG     = REG_ITEM(REG_INT2, _BIT2, _MASK1),
	INT2_VBUS_OFF    = REG_ITEM(REG_INT2, _BIT0, _MASK1),

	INTMASK1_OVP_OCP_DIS_M	= REG_ITEM(REG_INTMASK1, _BIT7, _MASK1),
	INTMASK1_OCP_EVENT_M	= REG_ITEM(REG_INTMASK1, _BIT5, _MASK1),
	INTMASK1_OVP_EVENT_M	= REG_ITEM(REG_INTMASK1, _BIT5, _MASK1),
	INTMASK1_DETACH_M	= REG_ITEM(REG_INTMASK1, _BIT1, _MASK1),
	INTMASK1_ATTACH_M	= REG_ITEM(REG_INTMASK1, _BIT0, _MASK1),

	INTMASK2_RID_CHARGERM = REG_ITEM(REG_INTMASK2, _BIT6, _MASK1),
	INTMASK2_MHL_M        = REG_ITEM(REG_INTMASK2, _BIT5, _MASK1),
	INTMASK2_ADC_CHG_M    = REG_ITEM(REG_INTMASK2, _BIT2, _MASK1),
	INTMASK2_REV_ACCE_M   = REG_ITEM(REG_INTMASK2, _BIT1, _MASK1),
	INTMASK2_VBUS_OFF_M   = REG_ITEM(REG_INTMASK2, _BIT0, _MASK1),

	ADC_ADCERR	=  REG_ITEM(REG_ADC, _BIT7, _MASK1),
	ADC_ADC_VALUE	=  REG_ITEM(REG_ADC, _BIT0, _MASK5),

	TIMING1_KEY_PRESS_T	= REG_ITEM(REG_TIMING1, _BIT4, _MASK4),
	TIMING1_OCP_TIME_DELAY	= REG_ITEM(REG_TIMING1, _BIT0, _MASK3),

	TIMING2_SW_WAIT_T  = REG_ITEM(REG_TIMING2, _BIT4, _MASK4),
	TIMING2_LONG_KEY_T = REG_ITEM(REG_TIMING2, _BIT0, _MASK4),

	DEVT1_USB_OTG         = REG_ITEM(REG_DEVT1, _BIT7, _MASK1),
	DEVT1_DEDICATED_CHG   = REG_ITEM(REG_DEVT1, _BIT6, _MASK1),
	DEVT1_USB_CHG         = REG_ITEM(REG_DEVT1, _BIT5, _MASK1),
	DEVT1_CAR_KIT_CHARGER = REG_ITEM(REG_DEVT1, _BIT4, _MASK1),
	DEVT1_UART            = REG_ITEM(REG_DEVT1, _BIT3, _MASK1),
	DEVT1_USB             = REG_ITEM(REG_DEVT1, _BIT2, _MASK1),
	DEVT1_AUDIO_TYPE2     = REG_ITEM(REG_DEVT1, _BIT1, _MASK1),
	DEVT1_AUDIO_TYPE1     = REG_ITEM(REG_DEVT1, _BIT0, _MASK1),

	DEVT2_AV           = REG_ITEM(REG_DEVT2, _BIT6, _MASK1),
	DEVT2_TTY          = REG_ITEM(REG_DEVT2, _BIT5, _MASK1),
	DEVT2_PPD          = REG_ITEM(REG_DEVT2, _BIT4, _MASK1),
	DEVT2_JIG_UART_OFF = REG_ITEM(REG_DEVT2, _BIT3, _MASK1),
	DEVT2_JIG_UART_ON  = REG_ITEM(REG_DEVT2, _BIT2, _MASK1),
	DEVT2_JIG_USB_OFF  = REG_ITEM(REG_DEVT2, _BIT1, _MASK1),
	DEVT2_JIG_USB_ON   = REG_ITEM(REG_DEVT2, _BIT0, _MASK1),

	STATUS_DCDTMR_OUT	= REG_ITEM(REG_MUIC_STATUS, _BIT5, _MASK1),
	STATUS_VBUSOVP		= REG_ITEM(REG_MUIC_STATUS, _BIT4, _MASK1),
	STATUS_CARKIT_TYPE	= REG_ITEM(REG_MUIC_STATUS, _BIT0, _MASK2),

	MANSW1_DM_CON_SW    =  REG_ITEM(REG_MANSW1, _BIT5, _MASK3),
	MANSW1_DP_CON_SW    =  REG_ITEM(REG_MANSW1, _BIT2, _MASK3),
	MANSW1_CHGIN_EN     =  REG_ITEM(REG_MANSW1, _BIT1, _MASK1),
	MANSW1_OTG_EN      =  REG_ITEM(REG_MANSW1, _BIT0, _MASK1),

	MANSW2_CHG_DET      =  REG_ITEM(REG_MANSW2, _BIT4, _MASK1),
	MANSW2_BOOT_SW      =  REG_ITEM(REG_MANSW2, _BIT3, _MASK1),
	MANSW2_JIG_ON       =  REG_ITEM(REG_MANSW2, _BIT2, _MASK1),
	MANSW2_IDBSW        =  REG_ITEM(REG_MANSW2, _BIT1, _MASK1),

	DEVT3_AV75      = REG_ITEM(REG_DEVT3, _BIT7, _MASK1),
	DEVT3_U200_CHG      = REG_ITEM(REG_DEVT3, _BIT6, _MASK1),
	DEVT3_APPLECHG      = REG_ITEM(REG_DEVT3, _BIT5, _MASK1),
	DEVT3_AV_CABLE_VBUS = REG_ITEM(REG_DEVT3, _BIT4, _MASK1),
	DEVT3_VBUS_NONSTD   = REG_ITEM(REG_DEVT3, _BIT2, _MASK1),
	DEVT3_VBUS_VALID     = REG_ITEM(REG_DEVT3, _BIT1, _MASK1),
	DEVT3_MHL           = REG_ITEM(REG_DEVT3, _BIT0, _MASK1),

	RESET_RESET = REG_ITEM(REG_RESET, _BIT0, _MASK1),

	TIMING3_ADC_PERIOD	= REG_ITEM(REG_TIMING3, _BIT4, _MASK2),
	TIMING3_LOADSW_ENTIME	= REG_ITEM(REG_TIMING3, _BIT3, _MASK1),
	TIMING3_DCDTMRSET	= REG_ITEM(REG_TIMING3, _BIT1, _MASK2),

	OCPSET_OCP_SET  = REG_ITEM(REG_OCP_SET, _BIT0, _MASK6),

	MUIC_CTRL_VBUS_DEC	=  REG_ITEM(REG_MUIC_CTRL, _BIT5, _MASK1),
	MUIC_ADC_OFF		=  REG_ITEM(REG_MUIC_CTRL, _BIT1, _MASK1),
	MUIC_CPEN		=  REG_ITEM(REG_MUIC_CTRL, _BIT1, _MASK1),
};

/* s2mm001 Control register */
#define CTRL_SWITCH_OPEN_SHIFT	4
#define CTRL_RAW_DATA_SHIFT		3
#define CTRL_MANUAL_SW_SHIFT		2
#define CTRL_WAIT_SHIFT		1
#define CTRL_INT_MASK_SHIFT		0

#define CTRL_SWITCH_OPEN_MASK	(0x1 << CTRL_SWITCH_OPEN_SHIFT)
#define CTRL_RAW_DATA_MASK		(0x1 << CTRL_RAW_DATA_SHIFT)
#define CTRL_MANUAL_SW_MASK		(0x1 << CTRL_MANUAL_SW_SHIFT)
#define CTRL_WAIT_MASK		(0x1 << CTRL_WAIT_SHIFT)
#define CTRL_INT_MASK_MASK		(0x1 << CTRL_INT_MASK_SHIFT)
#define CTRL_MASK		(CTRL_SWITCH_OPEN_MASK | CTRL_RAW_DATA_MASK | \
				/*CTRL_MANUAL_SW_MASK |*/ CTRL_WAIT_MASK | \
				CTRL_INT_MASK_MASK)

/* S2MM001_MUIC_REG_CTRL2 register */
#define MUIC_CTRL_CHGPUMP_nEN (0x1 << 0)

struct reg_value_set {
	int value;
	char *alias;
};

/* ADC Scan Mode Values : b'1 */
static struct reg_value_set s2mm001_adc_scanmode_tbl[] = {
	[ADC_SCANMODE_CONTINUOUS] = {0x01, "Periodic"},
	[ADC_SCANMODE_ONESHOT]    = {0x00, "Oneshot."},
	[ADC_SCANMODE_PULSE]      = {0x00, "Oneshot.."},
};

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2] / Vbus [1:0]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO
 * 00: Vbus to Open / 01: Vbus to Charger / 10: Vbus to MIC / 11: Vbus to VBout
 */
#define _D_OPEN	(0x0)
#define _D_USB		(0x1)
#define _D_AUDIO	(0x2)
#define _D_UART	(0x3)
#define _V_OPEN	(0x0)
#define _V_CHARGER	(0x2)
#define _V_OTG		(0x1)

/* COM patch Values */
#define COM_VALUE(dm, vb) ((dm<<5) | (dm<<2) | vb)

#define _COM_OPEN		COM_VALUE(_D_OPEN, _V_OPEN)
#define _COM_OPEN_WITH_V_BUS	COM_VALUE(_D_OPEN, _V_CHARGER)
#define _COM_UART_AP		COM_VALUE(_D_UART, _V_CHARGER)
#define _COM_UART_CP		_COM_UART_AP
#define _COM_USB_AP		COM_VALUE(_D_USB, _V_CHARGER)
#define _COM_USB_CP		_COM_USB_AP
#define _COM_AUDIO		COM_VALUE(_D_AUDIO, _V_CHARGER)

static int s2mm001_com_value_tbl[] = {
	[COM_OPEN]		= _COM_OPEN,
	[COM_OPEN_WITH_V_BUS]	= _COM_OPEN_WITH_V_BUS,
	[COM_UART_AP]		= _COM_UART_AP,
	[COM_UART_CP]		= _COM_UART_CP,
	[COM_USB_AP]		= _COM_USB_AP,
	[COM_USB_CP]		= _COM_USB_CP,
	[COM_AUDIO]		= _COM_AUDIO,
};

#define REG_CTRL_INITIAL (CTRL_MASK | CTRL_MANUAL_SW_MASK)

static regmap_t s2mm001_muic_regmap_table[] = {
	[REG_DEVID]    = {"DeviceID",	0x00, 0x00, INIT_NONE},
	[REG_CTRL]     = {"CONTROL",	0x1F, 0x00, REG_CTRL_INITIAL,},
	[REG_INT1]     = {"INT1",	0x00, 0x00, INIT_NONE,},
	[REG_INT2]     = {"INT2",	0x00, 0x00, INIT_NONE,},
	[REG_INTMASK1] = {"INTMASK1",	0x00, 0x00, REG_INTMASK1_VALUE,},
	[REG_INTMASK2] = {"INTMASK2",	0x00, 0x00, REG_INTMASK2_VALUE,},
	[REG_ADC]      = {"ADC",		0x1F, 0x00, INIT_NONE,},
	[REG_TIMING1]  = {"TimingSet1",	0x02, 0x00, REG_TIMING1_VALUE,},
	[REG_TIMING2]  = {"TimingSet2",	0x00, 0x00, INIT_NONE,},
	[REG_DEVT1]    = {"DEVICETYPE1",	0x00, 0x00, INIT_NONE,},
	[REG_DEVT2]    = {"DEVICETYPE2",	0x00, 0x00, INIT_NONE,},
	[REG_BTN1]     = {"BUTTON1",		0x00, 0x00, INIT_NONE,},
	[REG_BTN2]     = {"BUTTON2",		0x00, 0x00, INIT_NONE,},
	[REG_MUIC_STATUS] = {"MUIC_STATUS",	0x00, 0x00, INIT_NONE,},
	/* 0x0F ~ 0x12: Reserved */
	[REG_MANSW1]   = {"ManualSW1",	0x00, 0x00, INIT_NONE,},
	[REG_MANSW2]   = {"ManualSW2",	0x00, 0x00, INIT_NONE,},
	[REG_DEVT3]    = {"DEVICETYPE3",	0x00, 0x00, INIT_NONE,},
	/* 0x16 ~ 0x1A: Reserved */
	[REG_RESET]    = {"RESET",		0x00, 0x00, INIT_NONE,},
	/* 0x1C: Reserved */
	[REG_TIMING3]  = {"TimingSet3",	0x14, 0x00, INIT_NONE,},
	/* 0x1E ~ 0x1F: Reserved */
	[REG_OCP_SET]  = {"OCP_SET",		0x29, 0x00, INIT_NONE,},
	[REG_MUIC_CTRL]	= {"MUIC_CTRL",	0x00, 0x00, MUIC_CTRL_CHGPUMP_nEN,},

	[REG_END]      = {NULL, 0, 0, INIT_NONE},
};

static int s2mm001_muic_ioctl(struct regmap_desc *pdesc,
	int arg1, int *arg2, int *arg3)
{
	int ret = 0;

	switch (arg1) {
	case GET_COM_VAL:
		*arg2 = s2mm001_com_value_tbl[*arg2];
		*arg3 = REG_MANSW1;
		break;
	case GET_CTLREG:
		*arg3 = REG_CTRL;
		break;

	case GET_ADC:
		*arg3 = ADC_ADC_VALUE;
		break;

	case GET_SWITCHING_MODE:
		*arg3 = CTRL_ManualSW;
		break;

	case GET_INT_MASK:
		*arg3 = CTRL_MASK_INT;
		break;

	case GET_REVISION:
		*arg3 = DEVID_VendorID;
		break;

	case GET_OTG_STATUS:
		*arg3 = INTMASK2_VBUS_OFF_M;
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

static int s2mm001_attach_ta(struct regmap_desc *pdesc)
{
	int attr = 0, value = 0, ret = 0;

	pr_info("%s\n", __func__);

	do {
		attr = REG_MANSW1 | _ATTR_OVERWRITE_M;
		value = _COM_OPEN_WITH_V_BUS;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_MANSW1 write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		attr = REG_MANSW2 | _ATTR_OVERWRITE_M;
		value = 0x10; /* CHG_DET low */
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_MANSW2 write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		attr = CTRL_ManualSW;
		value = 0; /* manual switching mode  */
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_CTRL write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);
	} while (0);

	return ret;
}

static int s2mm001_detach_ta(struct regmap_desc *pdesc)
{
	int attr = 0, value = 0, ret = 0;

	pr_info("%s\n", __func__);

	do {
		attr = REG_MANSW1 | _ATTR_OVERWRITE_M;
		value = _COM_OPEN;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_MANSW1 write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		attr = REG_MANSW2 | _ATTR_OVERWRITE_M;
		value = 0x00;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_MANSW2 write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		attr = CTRL_ManualSW;
		value = 1;  /* auto switching mode */
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_CTRL write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	} while (0);

	return ret;
}

static int s2mm001_set_rustproof(struct regmap_desc *pdesc, int op)
{
	int attr = 0, value = 0, ret = 0;

	pr_info("%s\n", __func__);

	do {
		attr = MANSW2_JIG_ON;
		value = op ? 1 : 0;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s MANSW2_JIG_ON write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		attr = CTRL_ManualSW;
		value = op ? 0 : 1;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s CTRL_ManualSW write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	} while (0);

	return ret;
}

static int s2mm001_get_vps_data(struct regmap_desc *pdesc, void *pbuf)
{
	muic_data_t *pmuic = pdesc->muic;
	vps_data_t *pvps = (vps_data_t *)pbuf;
	int attr;

	if (pdesc->trace)
		pr_info("%s\n", __func__);

	*(u8 *)&pvps->s.val1 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT1);
	*(u8 *)&pvps->s.val2 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT2);
	*(u8 *)&pvps->s.val3 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT3);

	attr = DEVT3_VBUS_VALID;
	*(u8 *)&pvps->s.vbvolt = regmap_read_value(pdesc, attr);

	attr = ADC_ADC_VALUE;
	*(u8 *)&pvps->s.adc = regmap_read_value(pdesc, attr);

	return 0;
}
/*
static int s2mm001_muic_enable_accdet(struct regmap_desc *pdesc)
{
	int ret = 0;
	return ret;
}
static int s2mm001_muic_disable_accdet(struct regmap_desc *pdesc)
{
	int ret = 0;
	return ret;
}
*/
static int s2mm001_get_adc_scan_mode(struct regmap_desc *pdesc)
{
	pr_info("%s: Not implemented.\n", __func__);
	return 0;
}

static void s2mm001_set_adc_scan_mode(struct regmap_desc *pdesc, const int mode)
{
	pr_info("%s: Not implemented.\n", __func__);
	return 0;
}

enum switching_mode_val{
	_SWMODE_AUTO =1,
	_SWMODE_MANUAL =0,
};

static int s2mm001_get_switching_mode(struct regmap_desc *pdesc)
{
	pr_info("%s: Not implemented.\n", __func__);
	return 0;
}

static void s2mm001_set_switching_mode(struct regmap_desc *pdesc, int mode)
{
	pr_info("%s: Not implemented.\n", __func__);
	return 0;
}
static void s2mm001_get_fromatted_dump(struct regmap_desc *pdesc, char *mesg)
{
	muic_data_t *muic = pdesc->muic;
	int val;

	if (pdesc->trace)
		pr_info("%s\n", __func__);

	val = i2c_smbus_read_byte_data(muic->i2c, REG_CTRL);
	sprintf(mesg, "CT:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK1);
	sprintf(mesg+strlen(mesg), "IM1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK2);
	sprintf(mesg+strlen(mesg), "IM2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_MANSW1);
	sprintf(mesg+strlen(mesg), "MS1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_MANSW2);
	sprintf(mesg+strlen(mesg), "MS2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_ADC);
	sprintf(mesg+strlen(mesg), "ADC:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_DEVT1);
	sprintf(mesg+strlen(mesg), "DT1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_DEVT2);
	sprintf(mesg+strlen(mesg), "DT2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_DEVT3);
	sprintf(mesg+strlen(mesg), "DT3:%x ", val);
}

static int s2mm001_get_sizeof_regmap(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return (int)ARRAY_SIZE(s2mm001_muic_regmap_table);
}

static struct regmap_ops s2mm001_muic_regmap_ops = {
	.get_size = s2mm001_get_sizeof_regmap,
	.ioctl = s2mm001_muic_ioctl,
	.get_formatted_dump = s2mm001_get_fromatted_dump,
};

static struct vendor_ops s2mm001_muic_vendor_ops = {
	.attach_ta = s2mm001_attach_ta,
	.detach_ta = s2mm001_detach_ta,
	.get_switch = sm5703_get_switching_mode,
	.set_switch = sm5703_set_switching_mode,
	.set_adc_scan_mode = sm5703_set_adc_scan_mode,
	.get_adc_scan_mode = sm5703_get_adc_scan_mode,
	.set_rustproof = s2mm001_set_rustproof,
	.get_vps_data = s2mm001_get_vps_data,
};

static struct regmap_desc s2mm001_muic_regmap_desc = {
	.name = "s2mm001-MUIC",
	.regmap = s2mm001_muic_regmap_table,
	.size = REG_END,
	.regmapops = &s2mm001_muic_regmap_ops,
	.vendorops = &s2mm001_muic_vendor_ops,
};

void muic_register_s2mm001_regmap_desc(struct regmap_desc **pdesc)
{
	*pdesc = &s2mm001_muic_regmap_desc;
}
