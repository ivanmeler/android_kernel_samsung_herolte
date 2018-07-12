/*
 * muic_regmap_sm5703.c
 *
 * Copyright (C) 2014 Samsung Electronics
 * Thomas Ryu <smilesr.ryu@samsung.com>
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

enum sm5703_muic_reg_init_value {
	REG_INTMASK1_VALUE	= (0xDC),
	REG_INTMASK2_VALUE	= (0x00),
	REG_TIMING1_VALUE	= (ADC_DETECT_TIME_200MS |
				KEY_PRESS_TIME_100MS),
};

/* sm5703 I2C registers */
enum sm5703_muic_reg {
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
	REG_CarKit	= 0x0e,

	REG_MANSW1	= 0x13,
	REG_MANSW2	= 0x14,
	REG_DEVT3	= 0x15,
	REG_RESET	= 0x1B,
	REG_RSVDID1	= 0x1D,
	REG_RSVDID2	= 0x20,
	REG_RSVDID3	= 0x21,
	/* 0x22 is reserved. */
	REG_CHGTYPE	= 0x24,
	REG_RSVDID4	= 0x3A,
	REG_END,
};

#define REG_ITEM(addr, bitp, mask) ((bitp<<16) | (mask<<8) | addr)

/* Field */
enum sm5703_muic_reg_item {
	DEVID_VendorID = REG_ITEM(REG_DEVID, _BIT0, _MASK3),

	CTRL_SW_OPEN	= REG_ITEM(REG_CTRL, _BIT4, _MASK1),
	CTRL_RAWDATA	= REG_ITEM(REG_CTRL, _BIT3, _MASK1),
	CTRL_ManualSW	= REG_ITEM(REG_CTRL, _BIT2, _MASK1),
	CTRL_WAIT	= REG_ITEM(REG_CTRL, _BIT1, _MASK1),
	CTRL_MASK_INT	= REG_ITEM(REG_CTRL, _BIT0, _MASK1),

	INT1_OVP_DIS	= REG_ITEM(REG_INT1, _BIT7, _MASK1),
	INT1_OVP_EVENT	= REG_ITEM(REG_INT1, _BIT5, _MASK1),
	INT1_DETACH	= REG_ITEM(REG_INT1, _BIT1, _MASK1),
	INT1_ATTACH	= REG_ITEM(REG_INT1, _BIT0, _MASK1),

	INT2_VBUSDET_ON  = REG_ITEM(REG_INT2, _BIT7, _MASK1),
	INT2_RID_CHARGER = REG_ITEM(REG_INT2, _BIT6, _MASK1),
	INT2_MHL         = REG_ITEM(REG_INT2, _BIT5, _MASK1),
	INT2_ADC_CHG     = REG_ITEM(REG_INT2, _BIT2, _MASK1),
	INT2_VBUS_OFF    = REG_ITEM(REG_INT2, _BIT0, _MASK1),

	INTMASK1_OVP_DIS_M   = REG_ITEM(REG_INTMASK1, _BIT7, _MASK1),
	INTMASK1_OVP_EVENT_M = REG_ITEM(REG_INTMASK1, _BIT5, _MASK1),
	INTMASK1_DETACH_M    = REG_ITEM(REG_INTMASK1, _BIT1, _MASK1),
	INTMASK1_ATTACH_M    = REG_ITEM(REG_INTMASK1, _BIT0, _MASK1),

	INTMASK2_VBUSDET_ON_M = REG_ITEM(REG_INTMASK2, _BIT7, _MASK1),
	INTMASK2_RID_CHARGERM = REG_ITEM(REG_INTMASK2, _BIT6, _MASK1),
	INTMASK2_MHL_M        = REG_ITEM(REG_INTMASK2, _BIT5, _MASK1),
	INTMASK2_ADC_CHG_M    = REG_ITEM(REG_INTMASK2, _BIT2, _MASK1),
	INTMASK2_REV_ACCE_M   = REG_ITEM(REG_INTMASK2, _BIT1, _MASK1),
	INTMASK2_VBUS_OFF_M   = REG_ITEM(REG_INTMASK2, _BIT0, _MASK1),

	ADC_ADC_VALUE  =  REG_ITEM(REG_ADC, _BIT0, _MASK5),

	TIMING1_KEY_PRESS_T = REG_ITEM(REG_TIMING1, _BIT4, _MASK4),
	TIMING1_ADC_DET_T   = REG_ITEM(REG_TIMING1, _BIT0, _MASK4),

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

	MANSW1_DM_CON_SW    =  REG_ITEM(REG_MANSW1, _BIT5, _MASK3),
	MANSW1_DP_CON_SW    =  REG_ITEM(REG_MANSW1, _BIT2, _MASK3),
	MANSW1_VBUS_SW      =  REG_ITEM(REG_MANSW1, _BIT0, _MASK2),

	MANSW2_CHG_DET      =  REG_ITEM(REG_MANSW2, _BIT4, _MASK1),
	MANSW2_BOOT_SW      =  REG_ITEM(REG_MANSW2, _BIT3, _MASK1),
	MANSW2_JIG_ON       =  REG_ITEM(REG_MANSW2, _BIT2, _MASK1),
	MANSW2_SINGLE_MODE  =  REG_ITEM(REG_MANSW2, _BIT1, _MASK1),
	MANSW2_ID_SW        =  REG_ITEM(REG_MANSW2, _BIT0, _MASK1),

	DEVT3_U200_CHG      = REG_ITEM(REG_DEVT3, _BIT6, _MASK1),
	DEVT3_AV_CABLE_VBUS = REG_ITEM(REG_DEVT3, _BIT4, _MASK1),
	DEVT3_DCD_OUT_SDP   = REG_ITEM(REG_DEVT3, _BIT2, _MASK1),
	DEVT3_MHL           = REG_ITEM(REG_DEVT3, _BIT0, _MASK1),

	RESET_RESET = REG_ITEM(REG_RESET, _BIT0, _MASK1),

	RSVDID1_VBUSIN_VALID  = REG_ITEM(REG_RSVDID1, _BIT1, _MASK1),
	RSVDID1_VBUSOUT_VALID = REG_ITEM(REG_RSVDID1, _BIT0, _MASK1),
	RSVDID2_DCD_TIME_EN   = REG_ITEM(REG_RSVDID2, _BIT2, _MASK1),
	RSVDID3_BCD_RESCAN    = REG_ITEM(REG_RSVDID3, _BIT0, _MASK1),
	CHGTYPE_CHG_TYPE      = REG_ITEM(REG_CHGTYPE, _BIT0, _MASK5),
	RSVDID4_CHGPUMP_nEN   = REG_ITEM(REG_RSVDID4, _BIT0, _MASK1),
};

/* sm5703 Control register */
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

struct reg_value_set {
	int value;
	char *alias;
};

/* ADC Scan Mode Values : b'1 */
static struct reg_value_set sm5703_adc_scanmode_tbl[] = {
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
#define _V_CHARGER	(0x1)
#define _V_MIC		(0x2)

/* COM patch Values */
#define COM_VALUE(dm, vb) ((dm<<5) | (dm<<2) | vb)

#define _COM_OPEN		COM_VALUE(_D_OPEN, _V_OPEN)
#define _COM_OPEN_WITH_V_BUS	COM_VALUE(_D_OPEN, _V_CHARGER)
#define _COM_UART_AP		COM_VALUE(_D_UART, _V_CHARGER)
#define _COM_UART_CP		_COM_UART_AP
#define _COM_USB_AP		COM_VALUE(_D_USB, _V_CHARGER)
#define _COM_USB_CP		_COM_USB_AP
#define _COM_AUDIO		COM_VALUE(_D_AUDIO, _V_CHARGER)

static int sm5703_com_value_tbl[] = {
	[COM_OPEN]		= _COM_OPEN,
	[COM_OPEN_WITH_V_BUS]	= _COM_OPEN_WITH_V_BUS,
	[COM_UART_AP]		= _COM_UART_AP,
	[COM_UART_CP]		= _COM_UART_CP,
	[COM_USB_AP]		= _COM_USB_AP,
	[COM_USB_CP]		= _COM_USB_CP,
	[COM_AUDIO]		= _COM_AUDIO,
};

#define REG_CTRL_INITIAL (CTRL_MASK | CTRL_MANUAL_SW_MASK)

static regmap_t sm5703_muic_regmap_table[] = {
	[REG_DEVID]	= {"DeviceID",	0x01, 0x00, INIT_NONE},
	[REG_CTRL]	= {"CONTROL",		0x1F, 0x00, REG_CTRL_INITIAL,},
	[REG_INT1]	= {"INT1",		0x00, 0x00, INIT_NONE,},
	[REG_INT2]	= {"INT2",		0x00, 0x00, INIT_NONE,},
	[REG_INTMASK1]	= {"INTMASK1",	0x00, 0x00, REG_INTMASK1_VALUE,},
	[REG_INTMASK2]	= {"INTMASK2",	0x00, 0x00, REG_INTMASK2_VALUE,},
	[REG_ADC]	= {"ADC",		0x1F, 0x00, INIT_NONE,},
	[REG_TIMING1]	= {"TimingSet1",	0x00, 0x00, REG_TIMING1_VALUE,},
	[REG_TIMING2]	= {"TimingSet2",	0x00, 0x00, INIT_NONE,},
	[REG_DEVT1]	= {"DEVICETYPE1",	0xFF, 0x00, INIT_NONE,},
	[REG_DEVT2]	= {"DEVICETYPE2",	0xFF, 0x00, INIT_NONE,},
	[REG_BTN1]	= {"BUTTON1",		0xFF, 0x00, INIT_NONE,},
	[REG_BTN2]	= {"BUTTON2",		0xFF, 0x00, INIT_NONE,},
	[REG_CarKit]	= {"CarKitStatus",	0x00, 0x00, INIT_NONE,},
	/* 0x0F ~ 0x12: Reserved */
	[REG_MANSW1]	= {"ManualSW1",	0x00, 0x00, INIT_NONE,},
	[REG_MANSW2]	= {"ManualSW2",	0x00, 0x00, INIT_NONE,},
	[REG_DEVT3]	= {"DEVICETYPE3",	0xFF, 0x00, INIT_NONE,},
	/* 0x16 ~ 0x1A: Reserved */
	[REG_RESET]	= {"RESET",		0x00, 0x00, INIT_NONE,},
	/* 0x1C: Reserved */
	[REG_RSVDID1]	= {"Reserved_ID1",	0x00, 0x00, INIT_NONE,},
	/* 0x1E ~ 0x1F: Reserved */
	[REG_RSVDID2]	= {"Reserved_ID2",	0x04, 0x00, INIT_NONE,},
	[REG_RSVDID3]	= {"Reserved_ID3",	0x00, 0x00, INIT_NONE,},
	/* 0x22: Reserved */
	[REG_CHGTYPE]	= {"REG_CHG_TYPE",	0xFF, 0x00, INIT_NONE,},
	[REG_RSVDID4]	= {"Reserved_ID4",	0x01, 0x00, 0x00},
	[REG_END]	= {NULL, 0, 0, INIT_NONE},
};

static int sm5703_muic_ioctl(struct regmap_desc *pdesc,
		int arg1, int *arg2, int *arg3)
{
	int ret = 0;

	switch (arg1) {
	case GET_COM_VAL:
		*arg2 = sm5703_com_value_tbl[*arg2];
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

	case GET_CHGTYPE:
		*arg3 = CHGTYPE_CHG_TYPE;
		break;

	case GET_RESID3:
		*arg3 = RSVDID3_BCD_RESCAN;
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

static int sm5703_attach_ta(struct regmap_desc *pdesc)
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
		value = 0; /* manual switching mode */
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_CTRL write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);
	} while (0);

	return ret;
}

static int sm5703_detach_ta(struct regmap_desc *pdesc)
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

static int sm5703_set_rustproof(struct regmap_desc *pdesc, int op)
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

static int sm5703_get_vps_data(struct regmap_desc *pdesc, void *pbuf)
{
	muic_data_t *pmuic = pdesc->muic;
	vps_data_t *pvps = (vps_data_t *)pbuf;
	int attr;

	if (pdesc->trace)
		pr_info("%s\n", __func__);

	*(u8 *)&pvps->s.val1 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT1);
	*(u8 *)&pvps->s.val2 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT2);
	*(u8 *)&pvps->s.val3 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT3);

	attr = RSVDID1_VBUSIN_VALID;
	*(u8 *)&pvps->s.vbvolt = regmap_read_value(pdesc, attr);

	attr = ADC_ADC_VALUE;
	*(u8 *)&pvps->s.adc = regmap_read_value(pdesc, attr);

	return 0;
}
/*
static int sm5703_muic_enable_accdet(struct regmap_desc *pdesc)
{
	int ret = 0;
	return ret;
}
static int sm5703_muic_disable_accdet(struct regmap_desc *pdesc)
{
	int ret = 0;
	return ret;
}
*/
static int sm5703_get_adc_scan_mode(struct regmap_desc *pdesc)
{
	struct reg_value_set *pvset;
	int attr, value, mode = 0;

	attr = MANSW2_SINGLE_MODE;
	value = regmap_read_value(pdesc, attr);

	for ( ; mode <ARRAY_SIZE(sm5703_adc_scanmode_tbl); mode++) {
		pvset = &sm5703_adc_scanmode_tbl[mode];
		if (pvset->value == value)
			break;
	}

	pr_info("%s: [%2d]=%02x,%02x\n", __func__, mode, value, pvset->value);
	pr_info("%s:       %s\n", __func__, pvset->alias);

	return mode;
}

static void sm5703_set_adc_scan_mode(struct regmap_desc *pdesc,
		const int mode)
{
	struct reg_value_set *pvset;
	int attr, ret, value;

	if (mode > ADC_SCANMODE_PULSE) {
		pr_err("%s Out of range(%d).\n", __func__, mode);
		return;
	}

	pvset = &sm5703_adc_scanmode_tbl[mode];
	pr_info("%s: [%2d] %s\n", __func__, mode, pvset->alias);

	do {
		value = pvset->value;
		attr = MANSW2_SINGLE_MODE;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s MANSW2_SINGLE_MODE write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

#define _ENABLE_PERIODIC_SCAN (0)
#define _DISABLE_PERIODIC_SCAN (1)

		attr = CTRL_RAWDATA;
		if (mode == ADC_SCANMODE_CONTINUOUS)
			value = _ENABLE_PERIODIC_SCAN;
		else
			value = _DISABLE_PERIODIC_SCAN;

		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s CTRL_RAWDATA write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

        } while (0);
}

enum switching_mode_val{
	_SWMODE_AUTO =1,
	_SWMODE_MANUAL =0,
};

static int sm5703_get_switching_mode(struct regmap_desc *pdesc)
{
	int attr, value, mode;

	pr_info("%s\n",__func__);

	attr = CTRL_ManualSW;
	value = regmap_read_value(pdesc, attr);

	mode = (value == _SWMODE_AUTO) ? SWMODE_AUTO : SWMODE_MANUAL;

	return mode;
}

static void sm5703_set_switching_mode(struct regmap_desc *pdesc, int mode)
{
        int attr, value;
	int ret = 0;

	pr_info("%s\n",__func__);

	value = (mode == SWMODE_AUTO) ? _SWMODE_AUTO : _SWMODE_MANUAL;
	attr = CTRL_ManualSW;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s REG_CTRL write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);
}

int sm5703_attach_mmdock(struct regmap_desc *pdesc, int vbus)
{

	int attr = 0, value = 0, ret = 0;

	pr_info("%s: vbus:%d\n", __func__, vbus);

	do {
		/* SW_OPEN: 0 */
		attr = CTRL_SW_OPEN;
		value = 0;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s CTRL_SW_OPEN write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		if (vbus) {
			/* 1. set AP USB path */
			attr = REG_MANSW1 | _ATTR_OVERWRITE_M;
			value = _COM_USB_AP;
			ret = regmap_write_value(pdesc, attr, value);
			if (ret < 0) {
				pr_err("%s REG_MANSW1 write fail.\n", __func__);
				break;
			}

			_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

			/* 2. set JIG_ON high(high) */

			/* 3. set MANUAL SW mode */
			attr = CTRL_ManualSW;
			value = 0; /* manual switching mode */
			ret = regmap_write_value(pdesc, attr, value);
			if (ret < 0) {
				pr_err("%s REG_CTRL write fail.\n", __func__);
				break;
			}

			_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

			/* SW_OPEN: 1 */
			attr = CTRL_SW_OPEN;
			value = 1;
			ret = regmap_write_value(pdesc, attr, value);
			if (ret < 0) {
				pr_err("%s CTRL_SW_OPEN write fail.\n", __func__);
				break;
			}

			_REGMAP_TRACE(pdesc, 'w', ret, attr, value);
		} else {
			/* 3. set AUTO SW mode */
			attr = CTRL_ManualSW;
			value = 1; /* auto switching mode */
			ret = regmap_write_value(pdesc, attr, value);
			if (ret < 0) {
				pr_err("%s CTRL_ManualSW write fail.\n", __func__);
				break;
			}
		}
	} while (0);

	return ret;
}

int sm5703_detach_mmdock(struct regmap_desc *pdesc)
{
	int attr = 0, value = 0, ret = 0;

	pr_info("%s\n", __func__);

	do {
		/* SW_OPEN: 1 */
		attr = CTRL_SW_OPEN;
		value = 1;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s CTRL_SW_OPEN write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		/* Path Open */
		attr = REG_MANSW1 | _ATTR_OVERWRITE_M;
		value = _COM_OPEN_WITH_V_BUS;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_MANSW1 write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		/* set AUTO SW mode */
		attr = CTRL_ManualSW;
		value = 1; /* auto switching mode */
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s REG_CTRL write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	} while (0);

	return ret;
}


static void sm5703_get_fromatted_dump(struct regmap_desc *pdesc, char *mesg)
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
	val = i2c_smbus_read_byte_data(muic->i2c, REG_RSVDID1);
	sprintf(mesg+strlen(mesg), "RS1:%x", val);
}

static int sm5703_get_sizeof_regmap(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return (int)ARRAY_SIZE(sm5703_muic_regmap_table);
}

static struct regmap_ops sm5703_muic_regmap_ops = {
	.get_size = sm5703_get_sizeof_regmap,
	.ioctl = sm5703_muic_ioctl,
	.get_formatted_dump = sm5703_get_fromatted_dump,
};

static struct vendor_ops sm5703_muic_vendor_ops = {
	.attach_ta = sm5703_attach_ta,
	.detach_ta = sm5703_detach_ta,
	.get_switch = sm5703_get_switching_mode,
	.set_switch = sm5703_set_switching_mode,
	.set_adc_scan_mode = sm5703_set_adc_scan_mode,
	.get_adc_scan_mode = sm5703_get_adc_scan_mode,
	.set_rustproof = sm5703_set_rustproof,
	.get_vps_data = sm5703_get_vps_data,
	.attach_mmdock = sm5703_attach_mmdock,
	.detach_mmdock = sm5703_detach_mmdock,
};

static struct regmap_desc sm5703_muic_regmap_desc = {
	.name = "sm5703-MUIC",
	.regmap = sm5703_muic_regmap_table,
	.size = REG_END,
	.regmapops = &sm5703_muic_regmap_ops,
	.vendorops = &sm5703_muic_vendor_ops,
};

void muic_register_sm5703_regmap_desc(struct regmap_desc **pdesc)
{
	*pdesc = &sm5703_muic_regmap_desc;
}
