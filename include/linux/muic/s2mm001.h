/*
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

#ifndef __S2MM001_H__
#define __S2MM001_H__

#include <linux/muic/muic.h>

#define MUIC_DEV_NAME	"muic-s2mm001"

/* s2mm001 muic register read/write related information defines. */

/* Slave addr = 0x4A: MUIC */

/* S2MM001 I2C registers */
enum s2mm001_muic_reg {
	S2MM001_MUIC_REG_DEVID		= 0x01,
	S2MM001_MUIC_REG_CTRL		= 0x02,
	S2MM001_MUIC_REG_INT1		= 0x03,
	S2MM001_MUIC_REG_INT2		= 0x04,
	S2MM001_MUIC_REG_INTMASK1	= 0x05,
	S2MM001_MUIC_REG_INTMASK2	= 0x06,
	S2MM001_MUIC_REG_ADC		= 0x07,
	S2MM001_MUIC_REG_TIMING1		= 0x08,
	S2MM001_MUIC_REG_TIMING2	= 0x09,
	/* unused registers */
	S2MM001_MUIC_REG_DEV_T1		= 0x0a,
	S2MM001_MUIC_REG_DEV_T2		= 0x0b,

	S2MM001_MUIC_REG_MANSW1		= 0x13,
	S2MM001_MUIC_REG_MANSW2		= 0x14,
	S2MM001_MUIC_REG_DEV_T3		= 0x15,
	S2MM001_MUIC_REG_RESET		= 0x1B,
	S2MM001_MUIC_REG_TIMING3	= 0x20,
	S2MM001_MUIC_REG_OCP		= 0x22,
	S2MM001_MUIC_REG_CTRL2		= 0x23,
	S2MM001_MUIC_REG_END,
};

/* S2MM001 REGISTER ENABLE or DISABLE bit */
#define S2MM001_ENABLE_BIT 1
#define S2MM001_DISABLE_BIT 0

/* S2MM001 Control register */
#define CTRL_SWITCH_OPEN_SHIFT		4
#define CTRL_RAW_DATA_SHIFT		3
#define CTRL_MANUAL_SW_SHIFT		2
#define CTRL_WAIT_SHIFT			1
#define CTRL_INT_MASK_SHIFT		0
#define CTRL_SWITCH_OPEN_MASK		(0x1 << CTRL_SWITCH_OPEN_SHIFT)
#define CTRL_RAW_DATA_MASK		(0x1 << CTRL_RAW_DATA_SHIFT)
#define CTRL_MANUAL_SW_MASK		(0x1 << CTRL_MANUAL_SW_SHIFT)
#define CTRL_WAIT_MASK			(0x1 << CTRL_WAIT_SHIFT)
#define CTRL_INT_MASK_MASK		(0x1 << CTRL_INT_MASK_SHIFT)
#define CTRL_MASK		(CTRL_SWITCH_OPEN_MASK | CTRL_RAW_DATA_MASK | \
				/*CTRL_MANUAL_SW_MASK |*/ CTRL_WAIT_MASK | \
				CTRL_INT_MASK_MASK)

/* S2MM001 Interrupt 1 register */
#define INT_OVP_EN_SHIFT		5
#define INT_LKR_SHIFT			4
#define INT_LKP_SHIFT			3
#define INT_KP_SHIFT			2
#define INT_DETACH_SHIFT		1
#define INT_ATTACH_SHIFT		0
#define INT_OVP_EN_MASK			(0x1 << INT_OVP_EN_SHIFT)
#define INT_LKR_MASK			(0x1 << INT_LKR_SHIFT)
#define INT_LKP_MASK			(0x1 << INT_LKP_SHIFT)
#define INT_KP_MASK			(0x1 << INT_KP_SHIFT)
#define INT_DETACH_MASK			(0x1 << INT_DETACH_SHIFT)
#define INT_ATTACH_MASK			(0x1 << INT_ATTACH_SHIFT)

/* S2MM001 Interrupt 2 register */
#define INT_ADC_CHANGE_SHIFT		2
#define INT_RSRV_ATTACH_SHIFT		1
#define INT_CHG_DET_SHIFT		0
#define INT_ADC_CHANGE_MASK		(0x1 << INT_ADC_CHANGE_SHIFT)
#define INT_RSRV_ATTACH_MASK		(0x1 << INT_RSRV_ATTACH_SHIFT)
#define INT_CHG_DET_MASK		(0x1 << INT_CHG_DET_SHIFT)

/* S2MM001 ADC register */
#define ADC_ADC_SHIFT			0
#define ADC_ADC_MASK			(0x1f << ADC_ADC_SHIFT)

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

/* S2MM001 Device Type 1 register */
#define DEV_TYPE1_USB_OTG		(0x1 << 7)
#define DEV_TYPE1_DEDICATED_CHG		(0x1 << 6)
#define DEV_TYPE1_CDP			(0x1 << 5)
#define DEV_TYPE1_T1_T2_CHG		(0x1 << 4)
#define DEV_TYPE1_UART			(0x1 << 3)
#define DEV_TYPE1_USB			(0x1 << 2)
#define DEV_TYPE1_AUDIO_2		(0x1 << 1)
#define DEV_TYPE1_AUDIO_1		(0x1 << 0)
#define DEV_TYPE1_USB_TYPES	(DEV_TYPE1_USB_OTG | DEV_TYPE1_CDP | \
				DEV_TYPE1_USB)
#define DEV_TYPE1_CHG_TYPES	(DEV_TYPE1_DEDICATED_CHG | DEV_TYPE1_CDP)

/* S2MM001 Device Type 2 register */
#define DEV_TYPE2_AV			(0x1 << 6)
#define DEV_TYPE2_TTY			(0x1 << 5)
#define DEV_TYPE2_PPD			(0x1 << 4)
#define DEV_TYPE2_JIG_UART_OFF		(0x1 << 3)
#define DEV_TYPE2_JIG_UART_ON		(0x1 << 2)
#define DEV_TYPE2_JIG_USB_OFF		(0x1 << 1)
#define DEV_TYPE2_JIG_USB_ON		(0x1 << 0)
#define DEV_TYPE2_JIG_USB_TYPES		(DEV_TYPE2_JIG_USB_OFF | \
					DEV_TYPE2_JIG_USB_ON)
#define DEV_TYPE2_JIG_UART_TYPES	(DEV_TYPE2_JIG_UART_OFF)
#define DEV_TYPE2_JIG_TYPES		(DEV_TYPE2_JIG_UART_TYPES | \
					DEV_TYPE2_JIG_USB_TYPES)

/* S2MM001 Device Type 3 register */
#define DEV_TYPE3_U200_CHG		(0x1 << 6)
#define DEV_TYPE3_APPLE_CHG		(0x1 << 5)
#define DEV_TYPE3_AV_WITH_VBUS		(0x1 << 4)
#define DEV_TYPE3_NO_STD_CHG		(0x1 << 2)
#define DEV_TYPE3_MHL			(0x1 << 0)
#define DEV_TYPE3_CHG_TYPE	(DEV_TYPE3_U200_CHG | DEV_TYPE3_NO_STD_CHG | \
				DEV_TYPE3_APPLE_CHG)

/* S2MM001_MUIC_REG_DEV_T3 register */
#define RSVD1_VBUS		(0x1 << 1)

/* S2MM001_MUIC_REG_CTRL2 register */
#define RSVD3_CHGPUMP_nEN	(0x1 << 0)

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2] / CHARGER[1] / OTGEN[0]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO
 * 00: Vbus to Open / 01: Vbus to Charger / 10: Vbus to MIC / 11: Vbus to VBout
 */
#define MANUAL_SW1_DM_SHIFT	5
#define MANUAL_SW1_DP_SHIFT	2
#define MANUAL_SW1_VBUS_SHIFT	0
#define MANUAL_SW1_D_OPEN	(0x0)
#define MANUAL_SW1_D_USB	(0x1)
#define MANUAL_SW1_D_AUDIO	(0x2)
#define MANUAL_SW1_D_UART	(0x3)
#define MANUAL_SW1_V_OPEN	(0x0)
#define MANUAL_SW1_V_CHARGER	(0x2)
#define MANUAL_SW1_V_OTGEN	(0x1)

enum s2mm001_switch_sel_val {
	S2MM001_SWITCH_SEL_1st_BIT_USB	= (0x1 << 0),
	S2MM001_SWITCH_SEL_2nd_BIT_UART	= (0x1 << 1),
};

enum s2mm001_reg_manual_sw1_value {
	MANSW1_OPEN =	(MANUAL_SW1_D_OPEN << MANUAL_SW1_DM_SHIFT) |
			(MANUAL_SW1_D_OPEN << MANUAL_SW1_DP_SHIFT) |
			(MANUAL_SW1_V_OPEN << MANUAL_SW1_VBUS_SHIFT),
	MANSW1_OPEN_WITH_V_BUS = (MANUAL_SW1_D_OPEN << MANUAL_SW1_DM_SHIFT) |
			(MANUAL_SW1_D_OPEN << MANUAL_SW1_DP_SHIFT) |
			(MANUAL_SW1_V_CHARGER << MANUAL_SW1_VBUS_SHIFT),
	MANSW1_USB =	(MANUAL_SW1_D_USB << MANUAL_SW1_DM_SHIFT) |
			(MANUAL_SW1_D_USB << MANUAL_SW1_DP_SHIFT) |
			(MANUAL_SW1_V_CHARGER << MANUAL_SW1_VBUS_SHIFT),
	MANSW1_AUDIO =	(MANUAL_SW1_D_AUDIO << MANUAL_SW1_DM_SHIFT) |
			(MANUAL_SW1_D_AUDIO << MANUAL_SW1_DP_SHIFT) |
			(MANUAL_SW1_V_CHARGER << MANUAL_SW1_VBUS_SHIFT),
	MANSW1_UART =	(MANUAL_SW1_D_UART << MANUAL_SW1_DM_SHIFT) |
			(MANUAL_SW1_D_UART << MANUAL_SW1_DP_SHIFT) |
			(MANUAL_SW1_V_CHARGER << MANUAL_SW1_VBUS_SHIFT),
	MANSW1_OPEN_RUSTPROOF = (MANUAL_SW1_D_OPEN << MANUAL_SW1_DM_SHIFT) |
			(MANUAL_SW1_D_UART << MANUAL_SW1_DP_SHIFT) |
			(MANUAL_SW1_V_CHARGER << MANUAL_SW1_VBUS_SHIFT),
};

enum s2mm001_muic_reg_init_value {
	REG_INTMASK1_VALUE		= (0xDC),
	REG_INTMASK2_VALUE		= (0xA0),
	REG_INTMASK2_VBUS		= (0x02),
	REG_TIMING1_VALUE		= (OCP_TIME_DELAY_4MS |
					KEY_PRESS_TIME_100MS),
};

/* muic chip specific internal data structure
 * that setted at muic-xxxx.c file
 */
struct s2mm001_muic_data {
	struct device *dev;
	struct i2c_client *i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex muic_mutex;

	/* muic common callback driver internal data */
	struct sec_switch_data *switch_data;

	/* model dependant muic platform data */
	struct muic_platform_data *pdata;

	/* muic support vps list */
	bool muic_support_list[ATTACHED_DEV_NUM];

	/* muic current attached device */
	muic_attached_dev_t	attached_dev;

	/* muic Device ID */
	u8 muic_vendor;			/* Vendor ID */
	u8 muic_version;		/* Version ID */

	bool			is_usb_ready;
	bool			is_factory_start;
	bool			is_rustproof;

	struct delayed_work	init_work;
	struct delayed_work	usb_work;
};

extern struct device *switch_device;
extern unsigned int system_rev;
extern struct muic_platform_data muic_pdata;

#endif /* __S2MM001_H__ */
