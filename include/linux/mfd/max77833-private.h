/*
 * max77833-private.h - Voltage regulator driver for the Maxim 77833
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

#ifndef __LINUX_MFD_MAX77833_PRIV_H
#define __LINUX_MFD_MAX77833_PRIV_H

#include <linux/i2c.h>

#include <linux/battery/charger/max77833_charger.h>
#include <linux/battery/fuelgauge/max77833_fuelgauge.h>

#define MAX77833_I2C_ADDR		(0x92)
#define MAX77833_REG_INVALID		(0xff)

#define MAX77833_IRQSRC_CHG		(1 << 0)
#define MAX77833_IRQSRC_TOP		(1 << 1)
#define MAX77833_IRQSRC_HAPTIC		(1 << 2)
#define MAX77833_IRQSRC_MUIC		(1 << 3)
#define MAX77833_IRQSRC_FG              (1 << 4)
#define MAX77833_IRQSRC_GPIO		(1 << 5)

enum max77833_reg {
	/* Slave addr = 0xCC */
	/* PMIC Top-Level Registers */
	MAX77833_PMIC_REG_PMICID			= 0x20,
	MAX77833_PMIC_REG_PMICREV			= 0x21,
	MAX77833_PMIC_REG_INTSRC			= 0x22,
	MAX77833_PMIC_REG_INTSRC_MASK			= 0x23,
	MAX77833_PMIC_REG_SYSTEM_INT			= 0x24,
	MAX77833_PMIC_REG_SYSTEM_INT_MASK               = 0x25,
	MAX77833_PMIC_REG_TOPSYS_STAT                   = 0x26,

	MAX77833_FG_REG_INT                             = 0x27,
	MAX77833_FG_REG_INT_MASK                        = 0x28,
	MAX77833_FG_REG_INT_STAT                        = 0x29,

	MAX77833_PMIC_REG_MAINCTRL1                     = 0x2B,
	MAX77833_PMIC_REG_SAFEOUT_CTRL			= 0x9B,

	/* Haptic motor driver Registers */
	MAX77833_MOTOR_OVERDRIVE_CYCLES		= 0x0061,
	MAX77833_MOTOR_OVERDRIVE_STRENGTH	= 0x0062,
	MAX77833_MOTOR_BRAKING_CYCLES		= 0X0063,
	MAX77833_LRA_ENABLE_1			= 0x006D,
	MAX77833_LRA_ENABLE_2			= 0x006E,
	MAX77833_LRA_ENABLE_3			= 0x006F,
	MAX77833_AUTORES_CONFIG				= 0x0059,
	MAX77833_AUTORES_MIN_FREQ_LOW			= 0x005B,
	MAX77833_AUTORES_MAX_FREQ_LOW			= 0x005D,
	MAX77833_NOMINAL_STRENGTH			= 0x0066,
	MAX77833_RES_MIN_FREQ_HIGH			= 0x005A,
	MAX77833_RES_MAX_FREQ_HIGH			= 0x005C,
	MAX77833_AUTORES_INIT_GUESS_LOW			= 0x005F,
	MAX77833_AUTORES_INIT_GUESS_HIGH		= 0x005E,
	MAX77833_AUTORES_LOCK_WINDOW			= 0X0060,
	MAX77833_OPTION_REG1				= 0X0070,
	MAX77833_AUTORES_UPDATE_FREQ			= 0X0075,

	/* Old Haptic motor driver Register */
	MAX77833_PMIC_REG_MCONFIG			= 0x10,

	MAX77833_CHG_REG_INT			        = 0x80,
	MAX77833_CHG_REG_INT_MASK			= 0x81,
	MAX77833_CHG_REG_INT_OK			        = 0x82,
	MAX77833_CHG_REG_DTLS_00			= 0x83,
	MAX77833_CHG_REG_DTLS_01			= 0x84,
	MAX77833_CHG_REG_DTLS_02			= 0x85,
	MAX77833_CHG_REG_CNFG_00			= 0x88,
	MAX77833_CHG_REG_CNFG_01			= 0x89,
	MAX77833_CHG_REG_CNFG_02			= 0x8A,
	MAX77833_CHG_REG_CNFG_03			= 0x8B,
	MAX77833_CHG_REG_CNFG_04			= 0x8C,
	MAX77833_CHG_REG_CNFG_05			= 0x8D,
	MAX77833_CHG_REG_CNFG_06			= 0x8E,
	MAX77833_CHG_REG_CNFG_07			= 0x8F,
	MAX77833_CHG_REG_CNFG_08			= 0x90,
	MAX77833_CHG_REG_CNFG_09			= 0x91,
	MAX77833_CHG_REG_CNFG_10			= 0x92,
	MAX77833_CHG_REG_CNFG_11			= 0x93,
	MAX77833_CHG_REG_CNFG_12			= 0x94,
	MAX77833_CHG_REG_CNFG_13			= 0x95,
	MAX77833_CHG_REG_CNFG_14			= 0x96,
	MAX77833_CHG_REG_CNFG_15			= 0x97,
	MAX77833_CHG_REG_CNFG_16			= 0x98,
	MAX77833_CHG_REG_CNFG_17			= 0x99,
	MAX77833_CHG_REG_CNFG_18			= 0x9A,
	MAX77833_CHG_REG_PROTECT                        = 0x9C,
	MAX77833_CHG_REG_SEMAPHORE                      = 0x9D,

	MAX77833_PMIC_REG_END,
};

/* Slave addr = 0x6C : Fuelgauge */
enum max77833_fuelgauge_reg {
	STATUS_REG                                   = 0x0000,
	VALRT_THRESHOLD_REG                          = 0x0002,
	TALRT_THRESHOLD_REG                          = 0x0004,
	SALRT_THRESHOLD_REG                          = 0x0006,
	REMCAP_REP_REG                               = 0x000A,
	SOCREP_REG                                   = 0x000C,
	TEMPERATURE_REG                              = 0x0010,
	VCELL_REG                                    = 0x0012,
	TIME_TO_EMPTY_REG                            = 0x0022,
	FULLSOCTHR_REG                               = 0x0026,
	CURRENT_REG                                  = 0x0014,
	AVG_CURRENT_REG                              = 0x0016,
	SOCMIX_REG                                   = 0x001A,
	SOCAV_REG                                    = 0x001C,
	REMCAP_MIX_REG                               = 0x001E,
	FULLCAP_REG                                  = 0x0020,
	AVR_TEMPERATURE_REG                          = 0x002C,
	CYCLES_REG                                   = 0x002E,
	DESIGNCAP_REG                                = 0x0030,
	AVR_VCELL_REG                                = 0x0032,
	TIME_TO_FULL_REG                             = 0x0040,
	CONFIG_REG                                   = 0x003A,
	ICHGTERM_REG                                 = 0x003C,
	REMCAP_AV_REG                                = 0x003E,
	FULLCAP_NOM_REG                              = 0x0046,
	MISCCFG_REG                                  = 0x0056,
	QRTABLE20_REG                                = 0x0064,
	FULLCAPREP_REG                               = 0x006A,
	RCOMP_REG                                    = 0x0070,
	FSTAT_REG                                    = 0x007A,
	QRTABLE30_REG                                = 0x0084,
	DQACC_REG                                    = 0x008A,
	DPACC_REG                                    = 0x008C,
	CONFIG2_REG                                  = 0x0176,
	VFOCV_REG                                    = 0x07DC,
	VFSOC_REG                                    = 0x0090,
	FILTERCFG2_REG                               = 0x0168,
	ISYS_REG                                     = 0x0086,
	AVGISYS_REG                                  = 0x0096,
	ISYSTH_REG                                   = 0x01EA,

	MAX77833_FG_END,
};

#define MAX77833_REG_MAINCTRL1_MRSTB_TIMER_MASK	(0x7)
#define MAX77833_REG_MAINCTRL1_MREN		(1 << 3)
//#define MAX77833_REG_MAINCTRL1_THRM_FORCE_EN	(1 << 4)

/* Slave addr = 0x4A: MUIC */
enum max77833_muic_reg {
	MAX77833_MUIC_REG_HW_REV	= 0x00,
	MAX77833_MUIC_REG_FW_REV1	= 0x01,
	MAX77833_MUIC_REG_FW_REV0       = 0x02,
	MAX77833_MUIC_REG_INT1		= 0x03,
	MAX77833_MUIC_REG_INT2		= 0x04,
	MAX77833_MUIC_REG_INT3		= 0x05,
	MAX77833_MUIC_REG_STATUS1	= 0x06,
	MAX77833_MUIC_REG_STATUS2	= 0x07,
	MAX77833_MUIC_REG_STATUS3	= 0x08,
	MAX77833_MUIC_REG_INTMASK1	= 0x0C,
	MAX77833_MUIC_REG_INTMASK2	= 0x0D,
	MAX77833_MUIC_REG_INTMASK3	= 0x0E,
	MAX77833_MUIC_REG_DAT_IN_OP	= 0x0F,
	MAX77833_MUIC_REG_DAT_IN1	= 0x10,
	MAX77833_MUIC_REG_DAT_IN2       = 0x11,
	MAX77833_MUIC_REG_DAT_IN3       = 0x12,
	MAX77833_MUIC_REG_DAT_IN4       = 0x13,
	MAX77833_MUIC_REG_DAT_IN5       = 0x14,
	MAX77833_MUIC_REG_DAT_IN6       = 0x15,
	MAX77833_MUIC_REG_DAT_IN7       = 0x16,
	MAX77833_MUIC_REG_DAT_IN8       = 0x17,
	MAX77833_MUIC_REG_DAT_OUT_OP	= 0x18,
	MAX77833_MUIC_REG_DAT_OUT1	= 0x19,
	MAX77833_MUIC_REG_DAT_OUT2      = 0x1A,
	MAX77833_MUIC_REG_DAT_OUT3      = 0x1B,
	MAX77833_MUIC_REG_DAT_OUT4      = 0x1C,
	MAX77833_MUIC_REG_DAT_OUT5      = 0x1D,
	MAX77833_MUIC_REG_DAT_OUT6      = 0x1E,

	MAX77833_MUIC_REG_END,
#if 0	/* Not USE */
	MAX77833_MUIC_REG_STATUS4	= 0x09,
	MAX77833_MUIC_REG_STATUS5	= 0x0A,
	MAX77833_MUIC_REG_STATUS6	= 0x0B,
#endif
};

/* Slave addr = 0x94: RGB LED */
enum max77833_led_reg {
	MAX77833_RGBLED_REG_LEDEN			= 0x40,
	MAX77833_RGBLED_REG_LED0BRT			= 0x41,
	MAX77833_RGBLED_REG_LED1BRT			= 0x42,
	MAX77833_RGBLED_REG_LED2BRT			= 0x43,
	MAX77833_RGBLED_REG_LED3BRT			= 0x44,
	MAX77833_RGBLED_REG_LEDRMP			= 0x46,
	MAX77833_RGBLED_REG_LEDBLNK			= 0x45,
	MAX77833_RGBLED_REG_DIMMING			= 0x49,
	MAX77833_LED_REG_END,
};

enum max77833_irq_source {
	TOP_INT = 0,
	CHG_INT,
	FUEL_INT,
	MUIC_INT1,
	MUIC_INT2,
	MUIC_INT3,

	MAX77833_IRQ_GROUP_NR,
};

#define MUIC_MAX_INT			MUIC_INT3
#define MAX77833_NUM_IRQ_MUIC_REGS	(MUIC_MAX_INT - MUIC_INT1 + 1)

enum max77833_irq {
	/* PMIC; TOPSYS */
	MAX77833_SYSTEM_IRQ_T100C_INT,
	MAX77833_SYSTEM_IRQ_T120C_INT,
	MAX77833_SYSTEM_IRQ_T140C_INT,
	MAX77833_SYSTEM_IRQ_I2C_WD_INT,
	MAX77833_SYSTEM_IRQ_SYSUVLO_INT,
	MAX77833_SYSTEM_IRQ_MRSTB_INT,
	MAX77833_SYSTEM_IRQ_TS_INT,

	/* PMIC; Charger */
	MAX77833_CHG_IRQ_BYP_I,
	MAX77833_CHG_IRQ_BATP_I,
	MAX77833_CHG_IRQ_BAT_I,
	MAX77833_CHG_IRQ_CHG_I,
	MAX77833_CHG_IRQ_WCIN_I,
	MAX77833_CHG_IRQ_CHGIN_I,
	MAX77833_CHG_IRQ_AICL_I,

	/* Fuelgauge */
	MAX77833_FG_IRQ_ALERT,

	/* MUIC INT1 */
	MAX77833_MUIC_IRQ_INT1_RESERVED,
	/* MUIC INT2 */
	MAX77833_MUIC_IRQ_INT2_RESERVED,
	/* MUIC INT3 */
	MAX77833_MUIC_IRQ_INT3_IDRES_INT,
	MAX77833_MUIC_IRQ_INT3_CHGTYP_INT,
	MAX77833_MUIC_IRQ_INT3_CHGTYP_RUN_INT,
	MAX77833_MUIC_IRQ_INT3_SYSMSG_INT,
	MAX77833_MUIC_IRQ_INT3_APCMD_RESP_INT,

	MAX77833_IRQ_NR,
};

struct max77833_dev {
	struct device *dev;
	struct i2c_client *i2c; /* 0x92; Haptic, PMIC */
	struct i2c_client *charger; /* 0xD2; Charger */
	struct i2c_client *fuelgauge; /* 0x6C; Fuelgauge */
	struct i2c_client *muic; /* 0x4A; MUIC */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[MAX77833_IRQ_GROUP_NR];
	int irq_masks_cache[MAX77833_IRQ_GROUP_NR];

#ifdef CONFIG_HIBERNATION
	/* For hibernation */
	u8 reg_pmic_dump[MAX77833_PMIC_REG_END];
	u8 reg_muic_dump[MAX77833_MUIC_REG_END];
	u8 reg_led_dump[MAX77833_LED_REG_END];
#endif

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	struct max77833_platform_data *pdata;
};

enum max77833_types {
	TYPE_MAX77833,
};

extern int max77833_irq_init(struct max77833_dev *max77833);
extern void max77833_irq_exit(struct max77833_dev *max77833);

/* MAX77833 shared i2c API function */
extern int max77833_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int max77833_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77833_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77833_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77833_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int max77833_read_word(struct i2c_client *i2c, u8 reg);

extern int max77833_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

extern int max77833_read_fg(struct i2c_client *i2c, u16 reg, u16 *dest);
extern int max77833_write_fg(struct i2c_client *i2c, u16 reg, u16 val);

/* MAX77833 check muic path fucntion */
extern bool is_muic_usb_path_ap_usb(void);
extern bool is_muic_usb_path_cp_usb(void);

/* MAX77833 Debug. ft */
extern void max77833_muic_read_register(struct i2c_client *i2c);

/* for charger api */
extern void max77833_hv_muic_charger_init(void);

#endif /* __LINUX_MFD_MAX77833_PRIV_H */

