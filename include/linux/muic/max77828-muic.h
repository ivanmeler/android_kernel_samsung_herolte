/*
 * max77828-muic.h - MUIC for the Maxim 77828
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * Seoyoung Jeong <seo0.jeong@samsung.com>
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
 * This driver is based on max14577-muic.h
 *
 */

#ifndef __MAX77828_MUIC_H__
#define __MAX77828_MUIC_H__

#define MUIC_DEV_NAME			"muic-max77828"

/* max77828 muic register read/write related information defines. */

/* MAX77828 REGISTER ENABLE or DISABLE bit */
enum max77828_reg_bit_control {
	MAX77828_DISABLE_BIT		= 0,
	MAX77828_ENABLE_BIT,
};

/* MAX77828 STATUS1 register */
#define STATUS1_ADC_SHIFT		0
#define STATUS1_ADCERR_SHIFT		6
#define STATUS1_ADC1K_SHIFT		7
#define STATUS1_ADC_MASK		(0x1f << STATUS1_ADC_SHIFT)
#define STATUS1_ADCERR_MASK		(0x1 << STATUS1_ADCERR_SHIFT)
#define STATUS1_ADC1K_MASK		(0x1 << STATUS1_ADC1K_SHIFT)

/* MAX77828 STATUS2 register */
#define STATUS2_CHGTYP_SHIFT		0
#define STATUS2_CHGDETRUN_SHIFT		3
#define STATUS2_DCDTMR_SHIFT		4
#define STATUS2_DXOVP_SHIFT		5
#define STATUS2_VBVOLT_SHIFT		6
#define STATUS2_CHGTYP_MASK		(0x7 << STATUS2_CHGTYP_SHIFT)
#define STATUS2_CHGDETRUN_MASK		(0x1 << STATUS2_CHGDETRUN_SHIFT)
#define STATUS2_DCDTMR_MASK		(0x1 << STATUS2_DCDTMR_SHIFT)
#define STATUS2_DXOVP_MASK		(0x1 << STATUS2_DXOVP_SHIFT)
#define STATUS2_VBVOLT_MASK		(0x1 << STATUS2_VBVOLT_SHIFT)

/* MAX77828 CDETCTRL1 register */
#define CHGDETEN_SHIFT			0
#define CHGTYPM_SHIFT			1
#define CDDELAY_SHIFT			4
#define CHGDETEN_MASK			(0x1 << CHGDETEN_SHIFT)
#define CHGTYPM_MASK			(0x1 << CHGTYPM_SHIFT)
#define CDDELAY_MASK			(0x1 << CDDELAY_SHIFT)

/* MAX77828 CONTROL1 register */
#define COMN1SW_SHIFT			0
#define COMP2SW_SHIFT			3
#define IDBEN_SHIFT			7
#define COMN1SW_MASK			(0x7 << COMN1SW_SHIFT)
#define COMP2SW_MASK			(0x7 << COMP2SW_SHIFT)
#define IDBEN_MASK			(0x1 << IDBEN_SHIFT)
#define CLEAR_IDBEN_RSVD_MASK		(COMN1SW_MASK | COMP2SW_MASK)

/* MAX77828 CONTROL2 register */
#define CTRL2_LOWPWR_SHIFT		0
#define	CTRL2_CPEN_SHIFT		2
#define CTRL2_ACCDET_SHIFT		5
#define CTRL2_LOWPWR_MASK		(0x1 << CTRL2_LOWPWR_SHIFT)
#define CTRL2_CPEN_MASK			(0x1 << CTRL2_CPEN_SHIFT)
#define CTRL2_ACCDET_MASK		(0x1 << CTRL2_ACCDET_SHIFT)
#define CTRL2_CPEN1_LOWPWD0	((MAX77828_ENABLE_BIT << CTRL2_CPEN_SHIFT) | \
				(MAX77828_DISABLE_BIT << CTRL2_ADCLOWPWR_SHIFT))
#define CTRL2_CPEN0_LOWPWD1	((MAX77828_DISABLE_BIT << CTRL2_CPEN_SHIFT) | \
				(MAX77828_ENABLE_BIT << CTRL2_ADCLOWPWR_SHIFT))

/* MAX77828 CONTROL3 register */
#define CTRL3_JIGSET_SHIFT		0
#define CTRL3_JIGSET_MASK		(0x3 << CTRL3_JIGSET_SHIFT)

/* MAX77828 CONTROL4 register */
#define CTRL4_ADCDBSET_SHIFT		0
#define CTRL4_ADCMODE_SHIFT		6
#define CTRL4_ADCDBSET_MASK		(0x3 << CTRL4_ADCDBSET_SHIFT)
#define CTRL4_ADCMODE_MASK		(0x3 << CTRL4_ADCMODE_SHIFT)

/* MAX77828 MUIC support for device tree */
struct of_max77828_muic_support {
	bool	notifier;
	bool	otg_dongle;
};

typedef enum {
	VB_LOW			= 0x00,
	VB_HIGH			= (0x1 << STATUS2_VBVOLT_SHIFT),

	VB_DONTCARE		= 0xff,
} vbvolt_t;

typedef enum {
	CHGDETRUN_FALSE		= 0x00,
	CHGDETRUN_TRUE		= (0x1 << STATUS2_CHGDETRUN_SHIFT),

	CHGDETRUN_DONTCARE	= 0xff,
} chgdetrun_t;

/* MAX77804 MUIC Output of USB Charger Detection */
typedef enum {
	/* No Valid voltage at VB (Vvb < Vvbdet) */
	CHGTYP_NO_VOLTAGE		= 0x00,
	/* Unknown (D+/D- does not present a valid USB charger signature) */
	CHGTYP_USB			= 0x01,
	/* Charging Downstream Port */
	CHGTYP_CDP			= 0x02,
	/* Dedicated Charger (D+/D- shorted) */
	CHGTYP_DEDICATED_CHARGER	= 0x03,
	/* Special 500mA charger, max current 500mA */
	CHGTYP_500MA			= 0x04,
	/* Special 1A charger, max current 1A */
	CHGTYP_1A			= 0x05,
	/* Special charger - 3.3V bias on D+/D- */
	CHGTYP_SPECIAL_3_3V_CHARGER	= 0x06,
	/* Reserved */
	CHGTYP_RFU			= 0x07,
	/* Any charger type */
	CHGTYP_ANY			= 0xfd,
	/* Don't care charger type */
	CHGTYP_DONTCARE			= 0xfe,

	CHGTYP_MAX,

	CHGTYP_INIT,
	CHGTYP_MIN = CHGTYP_NO_VOLTAGE
} chgtyp_t;

/* muic register value for COMN1, COMN2 in CTRL1 reg  */

/*
 * MAX77828 CONTROL1 register
 * ID Bypass [7] / Mic En [6] / D+ [5:3] / D- [2:0]
 * 0: ID Bypass Open / 1: IDB connect to UID
 * 0: Mic En Open / 1: Mic connect to VB
 * 000: Open / 001: USB / 010: Audio / 011: UART
 */
enum max77828_reg_ctrl1_val {
	MAX77828_MUIC_CTRL1_ID_OPEN	= 0x0,
	MAX77828_MUIC_CTRL1_ID_BYPASS	= 0x1,
	MAX77828_MUIC_CTRL1_COM_OPEN	= 0x00,
	MAX77828_MUIC_CTRL1_COM_USB	= 0x01,
	MAX77828_MUIC_CTRL1_COM_AUDIO	= 0x02,
	MAX77828_MUIC_CTRL1_COM_UART	= 0x03,
	MAX77828_MUIC_CTRL1_COM_USB_CP	= 0x04,
	MAX77828_MUIC_CTRL1_COM_UART_CP	= 0x05,
};

typedef enum {
	CTRL1_OPEN	= (MAX77828_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_OPEN << COMP2SW_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_OPEN << COMN1SW_SHIFT),
	CTRL1_USB	= (MAX77828_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_USB << COMP2SW_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_USB << COMN1SW_SHIFT),
	CTRL1_AUDIO	= (MAX77828_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_AUDIO << COMP2SW_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_AUDIO << COMN1SW_SHIFT),
	CTRL1_UART	= (MAX77828_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_UART << COMP2SW_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_UART << COMN1SW_SHIFT),
	CTRL1_USB_CP	= (MAX77828_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_USB_CP << COMP2SW_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_USB_CP << COMN1SW_SHIFT),
	CTRL1_UART_CP	= (MAX77828_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_UART_CP << COMP2SW_SHIFT) | \
			(MAX77828_MUIC_CTRL1_COM_UART_CP << COMN1SW_SHIFT),
} max77828_reg_ctrl1_t;

extern struct device *switch_device;

#endif /* __MAX77828_MUIC_H__ */

