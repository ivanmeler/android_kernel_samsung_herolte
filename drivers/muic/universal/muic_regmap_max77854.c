/*
 * muic_regmap_max77854.c
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

/*
    Control4 Initialization value : 10_1_1_00_10
    [7:6] ADCMode, 10:One Shot + Low Lp Disconnect Detect
    [5] FacAuto, 1:Factory Auto detectioni Enabled
    [4] USBAuto, 1:USB Auto detection Enabled(valid only if AccDet=1)
    [3:2] RSVD
    [1:0] ADCDbSet, 10:25ms
*/
#define	INIT_CONTROL4 (0xB2)

/* max77854 I2C registers */
enum max77854_muic_reg {
	REG_ID		= 0x00,
	REG_INT1	= 0x01,
	REG_INT2	= 0x02,
	REG_INT3	= 0x03,
	REG_STATUS1	= 0x04,
	REG_STATUS2	= 0x05,
	REG_STATUS3	= 0x06,
	REG_INTMASK1	= 0x07,
	REG_INTMASK2	= 0x08,
	REG_INTMASK3	= 0x09,
	REG_CDETCTRL1	= 0x0A,
	REG_CDETCTRL2	= 0x0B,
	REG_CONTROL1	= 0x0C,
	REG_CONTROL2	= 0x0D,
	REG_CONTROL3	= 0x0E,
	REG_CONTROL4	= 0x16,
	REG_HVCONTROL1	= 0x17,
	REG_HVCONTROL2	= 0x18,

	REG_END,
};

#define REG_ITEM(addr, bitp, mask) ((bitp<<16) | (mask<<8) | addr)

/* Field */
enum max77854_muic_reg_item {
	ID_CHIP_REV	= REG_ITEM(REG_ID, _BIT3, _MASK5),
	ID_VENDOR_ID	= REG_ITEM(REG_ID, _BIT0, _MASK3),

	INT1_ADC1K	= REG_ITEM(REG_INT1, _BIT3, _MASK1),
	INT1_ADCError	= REG_ITEM(REG_INT1, _BIT2, _MASK1),
	INT1_ADC	= REG_ITEM(REG_INT1, _BIT0, _MASK1),

	INT2_VBVolt	= REG_ITEM(REG_INT2, _BIT4, _MASK1),
	INT2_DxOVP	= REG_ITEM(REG_INT2, _BIT3, _MASK1),
	INT2_DCDTmr	= REG_ITEM(REG_INT2, _BIT2, _MASK1),
	INT2_ChgDetRun	= REG_ITEM(REG_INT2, _BIT1, _MASK1),
	INT2_ChgTyp	= REG_ITEM(REG_INT2, _BIT0, _MASK1),

	INT3_MRxRdy	= REG_ITEM(REG_INT3, _BIT7, _MASK1),
	INT3_MRxPerr	= REG_ITEM(REG_INT3, _BIT6, _MASK1),
	INT3_MRxTrf	= REG_ITEM(REG_INT3, _BIT5, _MASK1),
	INT3_MRxBufOw	= REG_ITEM(REG_INT3, _BIT4, _MASK1),
	INT3_MPNack	= REG_ITEM(REG_INT3, _BIT3, _MASK1),
	INT3_DNRes	= REG_ITEM(REG_INT3, _BIT2, _MASK1),
	INT3_VDNMon	= REG_ITEM(REG_INT3, _BIT1, _MASK1),
	INT3_VbADC	= REG_ITEM(REG_INT3, _BIT0, _MASK1),

	STATUS1_ADC1K		= REG_ITEM(REG_STATUS1, _BIT7, _MASK1),
	STATUS1_ADCError	= REG_ITEM(REG_STATUS1, _BIT6, _MASK1),
	STATUS1_ADC		= REG_ITEM(REG_STATUS1, _BIT0, _MASK5),

	STATUS2_VBVolt		= REG_ITEM(REG_STATUS2, _BIT6, _MASK1),
	STATUS2_DxOVP		= REG_ITEM(REG_STATUS2, _BIT5, _MASK1),
	STATUS2_DCDTmr		= REG_ITEM(REG_STATUS2, _BIT4, _MASK1),
	STATUS2_ChgDetrun	= REG_ITEM(REG_STATUS2, _BIT3, _MASK1),
	STATUS2_ChgTyp		= REG_ITEM(REG_STATUS2, _BIT0, _MASK3),

	STATUS3_MPNack		= REG_ITEM(REG_STATUS3, _BIT6, _MASK1),
	STATUS3_DNRes		= REG_ITEM(REG_STATUS3, _BIT5, _MASK1),
	STATUS3_VDNMon		= REG_ITEM(REG_STATUS3, _BIT4, _MASK1),
	STATUS3_VbADC		= REG_ITEM(REG_STATUS3, _BIT0, _MASK4),

	INTMASK1_ADC1K_M	= REG_ITEM(REG_INTMASK1, _BIT3, _MASK1),
	INTMASK1_ADCErrorM	= REG_ITEM(REG_INTMASK1, _BIT2, _MASK1),
	INTMASK1_ADCM		= REG_ITEM(REG_INTMASK1, _BIT0, _MASK1),

	INTMASK2_VBVoltM	= REG_ITEM(REG_INTMASK2, _BIT4, _MASK1),
	INTMASK2_DxOVPM		= REG_ITEM(REG_INTMASK2, _BIT3, _MASK1),
	INTMASK2_DCDTmrM	= REG_ITEM(REG_INTMASK2, _BIT2, _MASK1),
	INTMASK2_ChgDetRunM	= REG_ITEM(REG_INTMASK2, _BIT1, _MASK1),
	INTMASK2_ChgTypM	= REG_ITEM(REG_INTMASK2, _BIT0, _MASK1),

	INTMASK3_MRxRdy		= REG_ITEM(REG_INTMASK3, _BIT7, _MASK1),
	INTMASK3_MRxPerr	= REG_ITEM(REG_INTMASK3, _BIT6, _MASK1),
	INTMASK3_MRxTrf		= REG_ITEM(REG_INTMASK3, _BIT5, _MASK1),
	INTMASK3_MRxBufOw	= REG_ITEM(REG_INTMASK3, _BIT4, _MASK1),
	INTMASK3_MPNack		= REG_ITEM(REG_INTMASK3, _BIT3, _MASK1),
	INTMASK3_DNRes		= REG_ITEM(REG_INTMASK3, _BIT2, _MASK1),
	INTMASK3_VDNMon		= REG_ITEM(REG_INTMASK3, _BIT1, _MASK1),
	INTMASK3_VbADC		= REG_ITEM(REG_INTMASK3, _BIT0, _MASK1),

	CDETCTRL1_CDPDet	=  REG_ITEM(REG_CDETCTRL1, _BIT7, _MASK1),
	CDETCTRL1_DCDCpl	=  REG_ITEM(REG_CDETCTRL1, _BIT5, _MASK1),
	CDETCTRL1_CDDelay	=  REG_ITEM(REG_CDETCTRL1, _BIT4, _MASK1),
	CDETCTRL1_DCD2sCt	=  REG_ITEM(REG_CDETCTRL1, _BIT3, _MASK1),
	CDETCTRL1_DCDEn		=  REG_ITEM(REG_CDETCTRL1, _BIT2, _MASK1),
	CDETCTRL1_ChgTypMan	=  REG_ITEM(REG_CDETCTRL1, _BIT1, _MASK1),
	CDETCTRL1_ChgDetEn	=  REG_ITEM(REG_CDETCTRL1, _BIT0, _MASK1),

	CDETCTRL2_DxOVPEN	=  REG_ITEM(REG_CDETCTRL2, _BIT3, _MASK1),
	CDETCTRL2_FrcChg	=  REG_ITEM(REG_CDETCTRL2, _BIT0, _MASK1),

	CONTROL1_NoBCComp	=  REG_ITEM(REG_CONTROL1, _BIT6, _MASK1),
	CONTROL1_COMP2Sw	=  REG_ITEM(REG_CONTROL1, _BIT3, _MASK3),
	CONTROL1_COMN1Sw	=  REG_ITEM(REG_CONTROL1, _BIT0, _MASK3),

	CONTROL2_RCPS		=  REG_ITEM(REG_CONTROL2, _BIT7, _MASK1),
	CONTROL2_USBCpInt	=  REG_ITEM(REG_CONTROL2, _BIT6, _MASK1),
	CONTROL2_AccDet		=  REG_ITEM(REG_CONTROL2, _BIT5, _MASK1),
	CONTROL2_SFOutOrd	=  REG_ITEM(REG_CONTROL2, _BIT4, _MASK1),
	CONTROL2_SFOUTAsrt	=  REG_ITEM(REG_CONTROL2, _BIT3, _MASK1),
	CONTROL2_CPEn		=  REG_ITEM(REG_CONTROL2, _BIT2, _MASK1),
	CONTROL2_ADCEn		=  REG_ITEM(REG_CONTROL2, _BIT1, _MASK1),
	CONTROL2_ADCLowPwr	=  REG_ITEM(REG_CONTROL2, _BIT0, _MASK1),

	CONTROL3_JIGSet		=  REG_ITEM(REG_CONTROL3, _BIT0, _MASK2),

	CONTROL4_ADCMode	=  REG_ITEM(REG_CONTROL4, _BIT6, _MASK2),
	CONTROL4_FctAuto	=  REG_ITEM(REG_CONTROL4, _BIT5, _MASK1),
	CONTROL4_USBAuto	=  REG_ITEM(REG_CONTROL4, _BIT4, _MASK1),
	CONTROL4_ADCDbSet	=  REG_ITEM(REG_CONTROL4, _BIT0, _MASK2),

	HVCONTROL1_VbusADCEn	=  REG_ITEM(REG_HVCONTROL1, _BIT4, _MASK1),
	HVCONTROL1_DPVd		=  REG_ITEM(REG_HVCONTROL1, _BIT3, _MASK2),
	HVCONTROL1_DNVd		=  REG_ITEM(REG_HVCONTROL1, _BIT1, _MASK2),
	HVCONTROL1_DPDNVdEn	=  REG_ITEM(REG_HVCONTROL1, _BIT0, _MASK1),

	HVCONTROL2_MPngEnb	=  REG_ITEM(REG_HVCONTROL2, _BIT6, _MASK1),
	HVCONTROL2_MTxBusRes	=  REG_ITEM(REG_HVCONTROL2, _BIT5, _MASK1),
	HVCONTROL2_MTxEn	=  REG_ITEM(REG_HVCONTROL2, _BIT4, _MASK1),
	HVCONTROL2_MPing	=  REG_ITEM(REG_HVCONTROL2, _BIT3, _MASK1),
	HVCONTROL2_DNResEn	=  REG_ITEM(REG_HVCONTROL2, _BIT2, _MASK1),
	HVCONTROL2_DP06En	=  REG_ITEM(REG_HVCONTROL2, _BIT1, _MASK1),
	HVCONTROL2_HVDigEn	=  REG_ITEM(REG_HVCONTROL2, _BIT0, _MASK1),
};

struct reg_value_set {
	int value;
	char *alias;
};

/* ADC Scan Mode Values : b'67 */
static struct reg_value_set max77854_adc_scanmode_tbl[] = {
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

#define _COM_OPEN		COM_VALUE(_D_OPEN, _ID_OPEN, _NO_BC_COMP_ON)
#define _COM_OPEN_WITH_V_BUS	_COM_OPEN
#define _COM_UART_AP		COM_VALUE(_D_UART, _ID_OPEN, _NO_BC_COMP_ON)
#define _COM_UART_CP		COM_VALUE(_D_UART_CP, _ID_OPEN, _NO_BC_COMP_ON)
#define _COM_USB_CP		COM_VALUE(_D_USB_CP, _ID_OPEN, _NO_BC_COMP_ON)
#define _COM_USB_AP		COM_VALUE(_D_USB, _ID_OPEN, _NO_BC_COMP_ON)
#define _COM_AUDIO		COM_VALUE(_D_AUDIO, _ID_OPEN, _NO_BC_COMP_OFF)

static int max77854_com_value_tbl[] = {
	[COM_OPEN]		= _COM_OPEN,
	[COM_OPEN_WITH_V_BUS]	= _COM_OPEN_WITH_V_BUS,
	[COM_UART_AP]		= _COM_UART_AP,
	[COM_UART_CP]		= _COM_UART_CP,
	[COM_USB_AP]		= _COM_USB_AP,
	[COM_USB_CP]		= _COM_USB_CP,
	[COM_AUDIO]		= _COM_AUDIO,
};

static regmap_t max77854_muic_regmap_table[] = {
	[REG_ID]	= {"ID",	  0xB5, 0x00, INIT_NONE},
	[REG_INT1]	= {"INT1",	  0x00, 0x00, INIT_INT_CLR,},
	[REG_INT2]	= {"INT2",	  0x00, 0x00, INIT_INT_CLR,},
	[REG_INT3]	= {"INT3",	  0x00, 0x00, INIT_INT_CLR,},
	[REG_STATUS1]	= {"STATUS1",	  0x3F, 0x00, INIT_NONE,},
	[REG_STATUS2]	= {"STATUS2",	  0x00, 0x00, INIT_NONE,},
	[REG_STATUS3]	= {"STATUS3",	  0x00, 0x00, INIT_NONE,},
	[REG_INTMASK1]	= {"INTMASK1",  0x00, 0x00, INIT_NONE,},
	[REG_INTMASK2]	= {"INTMASK2",  0x00, 0x00, INIT_NONE,},
	[REG_INTMASK3]	= {"INTMASK3",  0x00, 0x00, INIT_NONE,},
	[REG_CDETCTRL1]	= {"CDETCTRL1", 0x2D, 0x00, INIT_NONE,},
	[REG_CDETCTRL2]	= {"CDETCTRL2", 0x0C, 0x00, INIT_NONE,},
	[REG_CONTROL1]	= {"CONTROL1",  0x00, 0x00, INIT_NONE,},
	[REG_CONTROL2]	= {"CONTROL2",  0x3B, 0x00, INIT_NONE,},
	[REG_CONTROL3]	= {"CONTROL3",  0x00, 0x00, INIT_NONE,},
	[REG_CONTROL4]	= {"CONTROL4",  0xB2, 0x00, INIT_CONTROL4,},
	[REG_HVCONTROL1]= {"HVCONTROL1",  0x00, 0x00, INIT_NONE,},
	[REG_HVCONTROL2]= {"HVCONTROL2",  0x00, 0x00, INIT_NONE,},
	[REG_END]	= {NULL, 0, 0, INIT_NONE},
};

static int max77854_muic_ioctl(struct regmap_desc *pdesc,
		int arg1, int *arg2, int *arg3)
{
	int ret = 0;

	switch (arg1) {
	case GET_COM_VAL:
		*arg2 = max77854_com_value_tbl[*arg2];
		*arg3 = REG_CONTROL1;
		break;
	case GET_ADC:
		*arg3 = STATUS1_ADC;
		break;
	case GET_REVISION:
		*arg3 = ID_CHIP_REV;
		break;
	case GET_OTG_STATUS:
		*arg3 = INTMASK2_VBVoltM;
		break;

	case GET_CHGTYPE:
		*arg3 = STATUS2_ChgTyp;
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
static int max77854_attach_ta(struct regmap_desc *pdesc)
{
	int attr, value, ret;

	pr_info("%s\n", __func__);

	attr = REG_CONTROL1;
	value = _COM_OPEN;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s CNTR1 reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	return ret;
}

static int max77854_detach_ta(struct regmap_desc *pdesc)
{
	int attr, value, ret;

	pr_info("%s\n", __func__);

	attr = REG_CONTROL1 | _ATTR_OVERWRITE_M;
	value = _COM_OPEN;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s CNTR1 reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	return ret;
}

static int max77854_set_rustproof(struct regmap_desc *pdesc, int op)
{
	pr_info("%s: Not implemented.\n", __func__);

	return 0;
}

static int max77854_get_vps_data(struct regmap_desc *pdesc, void *pbuf)
{
	vps_data_t *pvps = (vps_data_t *)pbuf;
	int attr;

	attr = REG_ITEM(REG_STATUS1, _BIT0, _MASK8);
	*(u8 *)&pvps->t.status[0] = regmap_read_value(pdesc, attr + 0);
	*(u8 *)&pvps->t.status[1] = regmap_read_value(pdesc, attr + 1);
	*(u8 *)&pvps->t.status[2] = regmap_read_value(pdesc, attr + 2);

	attr = REG_ITEM(REG_CONTROL1, _BIT0, _MASK8);
	*(u8 *)&pvps->t.control[0] = regmap_read_value(pdesc, attr + 0);
	*(u8 *)&pvps->t.control[1] = regmap_read_value(pdesc, attr + 1);
	*(u8 *)&pvps->t.control[2] = regmap_read_value(pdesc, attr + 2);

	attr = REG_ITEM(REG_CONTROL4, _BIT0, _MASK8);
	*(u8 *)&pvps->t.control[3] = regmap_read_value(pdesc, attr + 0);

	attr = REG_ITEM(REG_HVCONTROL1, _BIT0, _MASK8);
	*(u8 *)&pvps->t.hvcontrol[0] = regmap_read_value(pdesc, attr + 0);
	*(u8 *)&pvps->t.hvcontrol[1] = regmap_read_value(pdesc, attr + 1);

	attr = STATUS1_ADC1K;
	*(u8 *)&pvps->t.adc1k = (pvps->t.status[0] >> _ATTR_BITP(attr)) & _ATTR_MASK(attr);

	attr = STATUS1_ADCError;
	*(u8 *)&pvps->t.adcerr = (pvps->t.status[0] >> _ATTR_BITP(attr)) & _ATTR_MASK(attr);

	attr = STATUS1_ADC;
	*(u8 *)&pvps->t.adc = (pvps->t.status[0] >> _ATTR_BITP(attr)) & _ATTR_MASK(attr);

	attr = STATUS2_VBVolt;
	*(u8 *)&pvps->t.vbvolt = (pvps->t.status[1] >> _ATTR_BITP(attr)) & _ATTR_MASK(attr);

	attr = STATUS2_ChgDetrun;
	*(u8 *)&pvps->t.chgdetrun = (pvps->t.status[1] >> _ATTR_BITP(attr)) & _ATTR_MASK(attr);

	attr = STATUS2_ChgTyp;
	*(u8 *)&pvps->t.chgtyp = (pvps->t.status[1] >> _ATTR_BITP(attr)) & _ATTR_MASK(attr);

	/* 1: timedout, 0: Not yet expired */
	attr = STATUS2_DCDTmr;
	*(u8 *)&pvps->t.DCDTimedout = (pvps->t.status[1] >> _ATTR_BITP(attr)) & _ATTR_MASK(attr);

	return 0;
}

static int max77854_enable_accdet(struct regmap_desc *pdesc, int enable)
{
	int attr, value, ret;

	pr_info("%s: %s\n", __func__, enable ? "Enable": "Disable");

	attr = CONTROL2_AccDet;
	value = enable ? 1: 0; /* 1: enable, 0: disable */
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s CNTR1 reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	return ret;
}

static int max77854_enable_chgdet(struct regmap_desc *pdesc, int enable)
{
	int attr, value, ret;

	pr_info("%s: %s\n", __func__, enable ? "Enable": "Disable");

	attr = CDETCTRL1_ChgDetEn;
	value = enable ? 1: 0; /* 1: enable, 0: disable */
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s CNTR1 reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	return ret;
}

static int max77854_run_chgdet(struct regmap_desc *pdesc, bool started)
{
	int attr, value, ret;

	pr_info("%s: %s\n", __func__, started ? "started": "disabled");

	attr = CDETCTRL1_ChgTypMan;
	value = started ? 1 : 0; /* 0: Disabled, 1: Force a Manual Charger Detection */
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s CNTR1 reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	return ret;
}


static int max77854_start_otg_test(struct regmap_desc *pdesc, int started)
{
	pr_info("%s: %s\n", __func__, started ? "started": "stopped");

	if (started) {
		max77854_enable_chgdet(pdesc, 0);
		max77854_enable_accdet(pdesc, 1);
	} else
		max77854_enable_chgdet(pdesc, 1);

	return 0;
}

static int max77854_get_adc_scan_mode(struct regmap_desc *pdesc)
{
	struct reg_value_set *pvset;
	int attr, value, mode = 0;

	attr = CONTROL4_ADCMode;
	value = regmap_read_value(pdesc, attr);

	for (; mode < ARRAY_SIZE(max77854_adc_scanmode_tbl); mode++) {
		pvset = &max77854_adc_scanmode_tbl[mode];
		if (pvset->value == value)
			break;
	}

	pr_info("%s: [%2d]=%02x,%02x\n", __func__, mode, value, pvset->value);
	pr_info("%s:       %s\n", __func__, pvset->alias);

	return mode;
}

static void max77854_set_adc_scan_mode(struct regmap_desc *pdesc,
		const int mode)
{
	struct reg_value_set *pvset;
	int attr, ret, value;

	if (mode > ADC_SCANMODE_PULSE) {
		pr_err("%s Out of range(%d).\n", __func__, mode);
		return;
	}

	pvset = &max77854_adc_scanmode_tbl[mode];
	pr_info("%s: [%2d] %s\n", __func__, mode, pvset->alias);

	attr = CONTROL4_ADCMode;
	value = pvset->value;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s ADCMODE reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

}

enum switching_mode_val{
	_SWMODE_AUTO = 0,
	_SWMODE_MANUAL = 1,
};

static int max77854_get_switching_mode(struct regmap_desc *pdesc)
{
	return SWMODE_AUTO;
}

static void max77854_set_switching_mode(struct regmap_desc *pdesc, int mode)
{
        int attr, value;
	int ret = 0;

	pr_info("%s\n",__func__);

	value = (mode == SWMODE_AUTO) ? _SWMODE_AUTO : _SWMODE_MANUAL;
	attr = CONTROL1_NoBCComp;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s REG_CTRL write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);
}

static int max77854_get_vbus_value(struct regmap_desc *pdesc, int type)
{
	muic_data_t *muic = pdesc->muic;
	int vbadc = 0, result = 0;
	int adcmode, adcen;

	/* type 0 : afc charger , type 1 : PD charger */
	pr_info("%s, type %d\n", __func__, type);

	/* for PD charger */
	if (type == 1) {
		adcmode = i2c_smbus_read_byte_data(muic->i2c, REG_CONTROL4);
		adcen = i2c_smbus_read_byte_data(muic->i2c, REG_HVCONTROL1);
		i2c_smbus_write_byte_data(muic->i2c, REG_CONTROL4, adcmode & 0x3F);
		i2c_smbus_write_byte_data(muic->i2c, REG_HVCONTROL1, adcen | 0x20);
		msleep(100);
		vbadc = regmap_read_value(pdesc, STATUS3_VbADC);
		i2c_smbus_write_byte_data(muic->i2c, REG_CONTROL4, adcmode);
		i2c_smbus_write_byte_data(muic->i2c, REG_HVCONTROL1, adcen);
	} else {
		vbadc = regmap_read_value(pdesc, STATUS3_VbADC);
	}

	switch (vbadc) {
	case 0:
		result = 0;
		break;
	case 1:
	case 2:
		result = 5;
		break;
	case 3:
	case 4:
		result = vbadc+3;
		break;
	case 5:
	case 6:
		result = 9;
		break;
	case 7:
	case 8:
		result = 12;
		break;
	case (9)...(15):
		result = vbadc+4;
		break;
	default:
		break;
	}

	return result;
}

static void max77854_get_fromatted_dump(struct regmap_desc *pdesc, char *mesg)
{
	muic_data_t *muic = pdesc->muic;
	int val;

	if (pdesc->trace)
		pr_info("%s\n", __func__);

	val = i2c_smbus_read_byte_data(muic->i2c, REG_STATUS1);
	sprintf(mesg+strlen(mesg), "ST1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_STATUS2);
	sprintf(mesg+strlen(mesg), "ST2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_STATUS3);
	sprintf(mesg+strlen(mesg), "ST3:%x ", val);

	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK1);
	sprintf(mesg+strlen(mesg), "IM1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK2);
	sprintf(mesg+strlen(mesg), "IM2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK3);
	sprintf(mesg+strlen(mesg), "IM3:%x ", val);

	val = i2c_smbus_read_byte_data(muic->i2c, REG_CDETCTRL1);
	sprintf(mesg+strlen(mesg), "CD1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CDETCTRL2);
	sprintf(mesg+strlen(mesg), "CD2:%x ", val);

	val = i2c_smbus_read_byte_data(muic->i2c, REG_CONTROL1);
	sprintf(mesg+strlen(mesg), "CT1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CONTROL2);
	sprintf(mesg+strlen(mesg), "CT2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CONTROL3);
	sprintf(mesg+strlen(mesg), "CT3:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_CONTROL4);
	sprintf(mesg+strlen(mesg), "CT4:%x ", val);

	val = i2c_smbus_read_byte_data(muic->i2c, REG_HVCONTROL1);
	sprintf(mesg+strlen(mesg), "HV1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_HVCONTROL2);
	sprintf(mesg+strlen(mesg), "HV2:%x ", val);
}

static int max77854_get_sizeof_regmap(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return (int)ARRAY_SIZE(max77854_muic_regmap_table);
}

/* Need to implement */
void _max77854_muic_read_register(struct i2c_client *i2c)
{
	pr_info("%s: Not implemented.\n", __func__);
}

int max77854_muic_read_register(struct i2c_client *i2c)
	__attribute__((weak, alias("_max77854_muic_read_register")));

static int max77854_init_reg_func(struct regmap_desc *pdesc)
{
	pr_info("%s\n", __func__);

	return 0;
}

static struct regmap_ops max77854_muic_regmap_ops = {
	.get_size = max77854_get_sizeof_regmap,
	.ioctl = max77854_muic_ioctl,
	.get_formatted_dump = max77854_get_fromatted_dump,
	.init = max77854_init_reg_func,
};

static struct vendor_ops max77854_muic_vendor_ops = {
	.attach_ta = max77854_attach_ta,
	.detach_ta = max77854_detach_ta,
	.get_switch = max77854_get_switching_mode,
	.set_switch = max77854_set_switching_mode,
	.set_adc_scan_mode = max77854_set_adc_scan_mode,
	.get_adc_scan_mode =  max77854_get_adc_scan_mode,
	.set_rustproof = max77854_set_rustproof,
	.enable_accdet = max77854_enable_accdet,
	.enable_chgdet = max77854_enable_chgdet,
	.run_chgdet = max77854_run_chgdet,
	.start_otg_test = max77854_start_otg_test,
	.get_vps_data = max77854_get_vps_data,
	.get_vbus_value = max77854_get_vbus_value,
};

static struct regmap_desc max77854_muic_regmap_desc = {
	.name = "max77854-MUIC",
	.regmap = max77854_muic_regmap_table,
	.size = REG_END,
	.regmapops = &max77854_muic_regmap_ops,
	.vendorops = &max77854_muic_vendor_ops,
};

void muic_register_max77854_regmap_desc(struct regmap_desc **pdesc)
{
	*pdesc = &max77854_muic_regmap_desc;
}
