/*
 * max77854-private.h - Voltage regulator driver for the Maxim 77843
 *
 *  Copyright (C) 2011 Samsung Electrnoics
 *  SangYoung Son <hello.son@samsung.com>
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

#ifndef __LINUX_MFD_MAX77854_PRIV_H
#define __LINUX_MFD_MAX77854_PRIV_H

#include <linux/i2c.h>

#define MAX77854_I2C_ADDR		(0x92)
#define MAX77854_REG_INVALID		(0xff)

#define MAX77854_IRQSRC_CHG		(1 << 0)
#define MAX77854_IRQSRC_TOP		(1 << 1)
#define MAX77854_IRQSRC_FG              (1 << 2)
#define MAX77854_IRQSRC_MUIC		(1 << 3)

enum max77854_reg {
	/* Slave addr = 0xCC */
	/* PMIC Top-Level Registers */
	MAX77854_PMIC_REG_PMICID1			= 0x00,
	MAX77854_PMIC_REG_PMICREV			= 0x01,
	MAX77854_PMIC_REG_INTSRC			= 0x22,
	MAX77854_PMIC_REG_INTSRC_MASK			= 0x23,
	MAX77854_PMIC_REG_SYSTEM_INT			= 0x24,
	MAX77854_PMIC_REG_RESERVED_25			= 0x25,
	MAX77854_PMIC_REG_SYSTEM_INT_MASK		= 0x26,
	MAX77854_PMIC_REG_RESERVED_27			= 0x27,
	MAX77854_PMIC_REG_TOPSYS_STAT			= 0x28,
	MAX77854_PMIC_REG_RESERVED_29			= 0x29,

	MAX77854_PMIC_REG_SAFEOUT_CTRL			= 0xC6,

	MAX77854_PMIC_REG_MAINCTRL1			= 0x02,

//	MAX77854_PMIC_REG_LSCNFG			= 0x2B,
//	MAX77854_PMIC_REG_RESERVED_2C			= 0x2C,
//	MAX77854_PMIC_REG_RESERVED_2D			= 0x2D,

	/* Haptic motor driver Registers */
	MAX77854_PMIC_REG_MCONFIG			= 0x10,

	MAX77854_CHG_REG_INT			= 0xB0,
	MAX77854_CHG_REG_INT_MASK			= 0xB1,
	MAX77854_CHG_REG_INT_OK			= 0xB2,
	MAX77854_CHG_REG_DETAILS_00			= 0xB3,
	MAX77854_CHG_REG_DETAILS_01			= 0xB4,
	MAX77854_CHG_REG_DETAILS_02			= 0xB5,
	MAX77854_CHG_REG_DTLS_03			= 0xB6,
	MAX77854_CHG_REG_CNFG_00			= 0xB7,
	MAX77854_CHG_REG_CNFG_01			= 0xB8,
	MAX77854_CHG_REG_CNFG_02			= 0xB9,
	MAX77854_CHG_REG_CNFG_03			= 0xBA,
	MAX77854_CHG_REG_CNFG_04			= 0xBB,
	MAX77854_CHG_REG_CNFG_05			= 0xBC,
	MAX77854_CHG_REG_CNFG_06			= 0xBD,
	MAX77854_CHG_REG_CNFG_07			= 0xBE,
	MAX77854_CHG_REG_CNFG_08			= 0xBF,
	MAX77854_CHG_REG_CNFG_09			= 0xC0,
	MAX77854_CHG_REG_CNFG_10			= 0xC1,
	MAX77854_CHG_REG_CNFG_11			= 0xC2,
	MAX77854_CHG_REG_CNFG_12			= 0xC3,
	MAX77854_CHG_REG_CNFG_13			= 0xC4,
	MAX77854_CHG_REG_CNFG_14			= 0xC5,
	MAX77854_CHG_REG_SAFEOUT_CTRL			= 0xC6,

	MAX77854_PMIC_REG_END,
};

/* Slave addr = 0x6C : Fuelgauge */
enum max77854_fuelgauge_reg {
	STATUS_REG                                   = 0x00,
	VALRT_THRESHOLD_REG                          = 0x01,
	TALRT_THRESHOLD_REG                          = 0x02,
	SALRT_THRESHOLD_REG                          = 0x03,
	REMCAP_REP_REG                               = 0x05,
	SOCREP_REG                                   = 0x06,
	TEMPERATURE_REG                              = 0x08,
	VCELL_REG                                    = 0x09,
	TIME_TO_EMPTY_REG                            = 0x11,
	FULLSOCTHR_REG                               = 0x13,
	CURRENT_REG                                  = 0x0A,
	AVG_CURRENT_REG                              = 0x0B,
	SOCMIX_REG                                   = 0x0D,
	SOCAV_REG                                    = 0x0E,
	REMCAP_MIX_REG                               = 0x0F,
	FULLCAP_REG                                  = 0x10,
	RFAST_REG                                    = 0x15,
	AVR_TEMPERATURE_REG                          = 0x16,
	CYCLES_REG                                   = 0x17,
	DESIGNCAP_REG                                = 0x18,
	AVR_VCELL_REG                                = 0x19,
	TIME_TO_FULL_REG                             = 0x20,
	CONFIG_REG                                   = 0x1D,
	ICHGTERM_REG                                 = 0x1E,
	REMCAP_AV_REG                                = 0x1F,
	FULLCAP_NOM_REG                              = 0x23,
	FILTER_CFG_REG                               = 0x29,
	MISCCFG_REG                                  = 0x2B,
	COFFSET_REG                                  = 0x2F,
	QRTABLE20_REG                                = 0x32,
	FULLCAP_REP_REG								 = 0x35,
	RCOMP_REG                                    = 0x38,
	VEMPTY_REG				     = 0x3A,
	FSTAT_REG                                    = 0x3D,
	DISCHARGE_THRESHOLD_REG			     = 0x40,
	QRTABLE30_REG                                = 0x42,
	DQACC_REG                                    = 0x45,
	DPACC_REG                                    = 0x46,
	QH_REG                                       = 0x4D,
	CONFIG2_REG                                  = 0xBB,
	OCV_REG                                      = 0xEE,
	VFOCV_REG                                    = 0xFB,
	VFSOC_REG                                    = 0xFF,

	MAX77854_FG_END,
};

#define MAX77854_REG_MAINCTRL1_MRDBTMER_MASK	(0x7)
#define MAX77854_REG_MAINCTRL1_MREN		(1 << 3)
#define MAX77854_REG_MAINCTRL1_BIASEN		(1 << 7)

/* Slave addr = 0x4A: MUIC */
enum max77854_muic_reg {
	MAX77854_MUIC_REG_ID		= 0x00,
	MAX77854_MUIC_REG_INT1		= 0x01,
	MAX77854_MUIC_REG_INT2		= 0x02,
	MAX77854_MUIC_REG_INT3		= 0x03,
	MAX77854_MUIC_REG_STATUS1	= 0x04,
	MAX77854_MUIC_REG_STATUS2	= 0x05,
	MAX77854_MUIC_REG_STATUS3	= 0x06,
	MAX77854_MUIC_REG_INTMASK1	= 0x07,
	MAX77854_MUIC_REG_INTMASK2	= 0x08,
	MAX77854_MUIC_REG_INTMASK3	= 0x09,
	MAX77854_MUIC_REG_CDETCTRL1	= 0x0A,
	MAX77854_MUIC_REG_CDETCTRL2	= 0x0B,
	MAX77854_MUIC_REG_CTRL1		= 0x0C,
	MAX77854_MUIC_REG_CTRL2		= 0x0D,
	MAX77854_MUIC_REG_CTRL3		= 0x0E,
	MAX77854_MUIC_REG_CTRL4		= 0x16,
	MAX77854_MUIC_REG_HVCONTROL1	= 0x17,
	MAX77854_MUIC_REG_HVCONTROL2	= 0x18,
	MAX77854_MUIC_REG_HVTXBYTE	= 0x19,
	MAX77854_MUIC_REG_HVRXBYTE1	= 0x1A,

	MAX77854_MUIC_REG_END,
};

/* Slave addr = 0x94: RGB LED */
enum max77854_led_reg {
	MAX77854_RGBLED_REG_LEDEN			= 0x30,
	MAX77854_RGBLED_REG_LED0BRT			= 0x31,
	MAX77854_RGBLED_REG_LED1BRT			= 0x32,
	MAX77854_RGBLED_REG_LED2BRT			= 0x33,
	MAX77854_RGBLED_REG_LED3BRT			= 0x34,
	MAX77854_RGBLED_REG_LEDRMP			= 0x36,
	MAX77854_RGBLED_REG_LEDBLNK			= 0x38,
	MAX77854_LED_REG_END,
};

enum max77854_irq_source {
	SYS_INT = 0,
	CHG_INT,
	FUEL_INT,
	MUIC_INT1,
	MUIC_INT2,
	MUIC_INT3,

	MAX77854_IRQ_GROUP_NR,
};

#define MUIC_MAX_INT			MUIC_INT3
#define MAX77854_NUM_IRQ_MUIC_REGS	(MUIC_MAX_INT - MUIC_INT1 + 1)

enum max77854_irq {
	/* PMIC; TOPSYS */
	MAX77854_SYSTEM_IRQ_SYSUVLO_INT,
	MAX77854_SYSTEM_IRQ_SYSOVLO_INT,
	MAX77854_SYSTEM_IRQ_TSHDN_INT,
	MAX77854_SYSTEM_IRQ_TM_INT,

	/* PMIC; Charger */
	MAX77854_CHG_IRQ_BYP_I,
	MAX77854_CHG_IRQ_BATP_I,
	MAX77854_CHG_IRQ_BAT_I,
	MAX77854_CHG_IRQ_CHG_I,
	MAX77854_CHG_IRQ_WCIN_I,
	MAX77854_CHG_IRQ_CHGIN_I,
	MAX77854_CHG_IRQ_AICL_I,

	/* MUIC INT1 */
	MAX77854_MUIC_IRQ_INT1_ADC,
	MAX77854_MUIC_IRQ_INT1_ADCERR,
	MAX77854_MUIC_IRQ_INT1_ADC1K,

	/* MUIC INT2 */
	MAX77854_MUIC_IRQ_INT2_CHGTYP,
	MAX77854_MUIC_IRQ_INT2_CHGDETREUN,
	MAX77854_MUIC_IRQ_INT2_DCDTMR,
	MAX77854_MUIC_IRQ_INT2_DXOVP,
	MAX77854_MUIC_IRQ_INT2_VBVOLT,

	/* MUIC INT3 */
	MAX77854_MUIC_IRQ_INT3_VBADC,
	MAX77854_MUIC_IRQ_INT3_VDNMON,
	MAX77854_MUIC_IRQ_INT3_DNRES,
	MAX77854_MUIC_IRQ_INT3_MPNACK,
	MAX77854_MUIC_IRQ_INT3_MRXBUFOW,
	MAX77854_MUIC_IRQ_INT3_MRXTRF,
	MAX77854_MUIC_IRQ_MRXPERR,
	MAX77854_MUIC_IRQ_MRXRDY,

	/* Fuelgauge */
	MAX77854_FG_IRQ_ALERT,

	MAX77854_IRQ_NR,
};

struct max77854_dev {
	struct device *dev;
	struct i2c_client *i2c; /* 0x92; Haptic, PMIC */
	struct i2c_client *charger; /* 0xD2; Charger */
	struct i2c_client *fuelgauge; /* 0x6C; Fuelgauge */
	struct i2c_client *muic; /* 0x4A; MUIC */
#if defined(CONFIG_MAX77854_FG_SENSING_WA)
	struct i2c_client *gtest;
	struct i2c_client *otp;
#endif
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[MAX77854_IRQ_GROUP_NR];
	int irq_masks_cache[MAX77854_IRQ_GROUP_NR];

#ifdef CONFIG_HIBERNATION
	/* For hibernation */
	u8 reg_pmic_dump[MAX77854_PMIC_REG_END];
	u8 reg_muic_dump[MAX77854_MUIC_REG_END];
	u8 reg_led_dump[MAX77854_LED_REG_END];
#endif

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	struct max77854_platform_data *pdata;
};

enum max77854_types {
	TYPE_MAX77854,
};

extern int max77854_irq_init(struct max77854_dev *max77854);
extern void max77854_irq_exit(struct max77854_dev *max77854);

/* MAX77854 shared i2c API function */
extern int max77854_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int max77854_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77854_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77854_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77854_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int max77854_read_word(struct i2c_client *i2c, u8 reg);

extern int max77854_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

/* MAX77854 check muic path fucntion */
extern bool is_muic_usb_path_ap_usb(void);
extern bool is_muic_usb_path_cp_usb(void);

/* for charger api */
extern void max77854_hv_muic_charger_init(void);

#endif /* __LINUX_MFD_MAX77854_PRIV_H */

