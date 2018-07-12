/*
 * core.h
 *
 * copyright (c) 2011 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_MFD_SEC_CORE_H
#define __LINUX_MFD_SEC_CORE_H

#include <linux/regulator/consumer.h>

#define NUM_IRQ_REGS	4

#define SEC_PMIC_REV(iodev)	(iodev)->rev_num

enum sec_device_type {
	S5M8751X,
	S5M8763X,
	S5M8767X,
	S2MPS11X,
	S2MPS13X,
	S2MPS15X,
	S2MPS16X,
	S2MPU03X,
};

/**
 * struct sec_pmic_dev - s5m87xx master device for sub-drivers
 * @dev: master device of the chip (can be used to access platform data)
 * @pdata: pointer to private data used to pass platform data to child
 * @i2c: i2c client private data for regulator
 * @rtc: i2c client private data for rtc
 * @iolock: mutex for serializing io access
 * @irqlock: mutex for buslock
 * @irq_base: base IRQ number for sec-pmic, required for IRQs
 * @irq: generic IRQ number for s5m87xx
 * @ono: power onoff IRQ number for s5m87xx
 * @irq_masks_cur: currently active value
 * @irq_masks_cache: cached hardware value
 * @type: indicate which s5m87xx "variant" is used
 */
struct sec_pmic_dev {
	struct device *dev;
	struct sec_platform_data *pdata;
	struct regmap *regmap;
	struct regmap *rtc_regmap;
	struct i2c_client *i2c;
	struct i2c_client *rtc;
	struct mutex sec_lock;
	struct mutex iolock;
	struct mutex irqlock;
	struct apm_ops *ops;

	int device_type;
	int rev_num;
	int irq_base;
	int irq;
	struct regmap_irq_chip_data *irq_data;

	int ono;
	u8 irq_masks_cur[NUM_IRQ_REGS];
	u8 irq_masks_cache[NUM_IRQ_REGS];
	int type;
	bool wakeup;
	bool adc_en;
};

/**
 * struct sec_wtsr_smpl - settings for WTSR/SMPL
 * @wtsr_en:		WTSR Function Enable Control
 * @smpl_en:		SMPL Function Enable Control
 * @wtsr_timer_val:	Set the WTSR timer Threshold
 * @smpl_timer_val:	Set the SMPL timer Threshold
 * @check_jigon:	if this value is true, do not enable SMPL function when
 *			JIGONB is low(JIG cable is attached)
 */
struct sec_wtsr_smpl {
	bool wtsr_en;
	bool smpl_en;
	int wtsr_timer_val;
	int smpl_timer_val;
	bool check_jigon;
};

struct sec_platform_data {
	struct sec_regulator_data	*regulators;
	struct sec_opmode_data		*opmode;
	int				device_type;
	int				num_regulators;

	int				irq_base;
	int				(*cfg_pmic_irq)(void);

	int				ono;
	bool				wakeup;
	bool				buck_voltage_lock;

	int				buck_gpios[3];
	int				buck_ds[3];
	unsigned int			buck2_voltage[8];
	bool				buck2_gpiodvs;
	unsigned int			buck3_voltage[8];
	bool				buck3_gpiodvs;
	unsigned int			buck4_voltage[8];
	bool				buck4_gpiodvs;

	int				buck_set1;
	int				buck_set2;
	int				buck_set3;
	int				buck2_enable;
	int				buck3_enable;
	int				buck4_enable;
	int				buck_default_idx;
	int				buck2_default_idx;
	int				buck3_default_idx;
	int				buck4_default_idx;

	int                             buck_ramp_delay;

	int				buck2_ramp_delay;
	int				buck3_ramp_delay;
	int				buck4_ramp_delay;
	int				buck6_ramp_delay;
	int				buck710_ramp_delay;
	int				buck89_ramp_delay;
	int				buck15_ramp_delay;
	int				buck34_ramp_delay;
	int				buck5_ramp_delay;
	int				buck16_ramp_delay;
	int				buck7810_ramp_delay;
	int				buck9_ramp_delay;
	int				bb1_ramp_delay;

	bool                            buck2_ramp_enable;
	bool                            buck3_ramp_enable;
	bool                            buck4_ramp_enable;
	bool				buck6_ramp_enable;

	int				buck2_init;
	int				buck3_init;
	int				buck4_init;

	int				smpl_warn;
	int				g3d_pin;
	int				dvs_pin;
	bool				g3d_en;
	bool				cache_data;
	bool				smpl_warn_en;
	bool				dvs_en;
	bool				buck_dvs_on;
	bool				adc_en;

	bool				ap_buck_avp_en;
	bool				sub_buck_avp_en;
	unsigned int                    smpl_warn_vth;
	unsigned int                    smpl_warn_hys;
	unsigned int			ldo8_7_seq;
	unsigned int			ldo10_9_seq;

	bool				ten_bit_address;

	/* ---- RTC ---- */
	struct sec_wtsr_smpl *wtsr_smpl;
	struct rtc_time *init_time;
};

int sec_irq_init(struct sec_pmic_dev *sec_pmic);
void sec_irq_exit(struct sec_pmic_dev *sec_pmic);
int sec_irq_resume(struct sec_pmic_dev *sec_pmic);

extern int sec_reg_read(struct sec_pmic_dev *sec_pmic, u32 reg, void *dest);
extern int sec_bulk_read(struct sec_pmic_dev *sec_pmic, u32 reg, int count, u8 *buf);
extern int sec_reg_write(struct sec_pmic_dev *sec_pmic, u32 reg, u32 value);
extern int sec_bulk_write(struct sec_pmic_dev *sec_pmic, u32 reg, int count, u8 *buf);
extern int sec_reg_update(struct sec_pmic_dev *sec_pmic, u32 reg, u32 val, u32 mask);


extern int sec_rtc_read(struct sec_pmic_dev *sec_pmic, u32 reg, void *dest);
extern int sec_rtc_bulk_read(struct sec_pmic_dev *sec_pmic, u32 reg, int count,
				u8 *buf);
extern int sec_rtc_write(struct sec_pmic_dev *sec_pmic, u32 reg, u32 value);
extern int sec_rtc_bulk_write(struct sec_pmic_dev *sec_pmic, u32 reg, int count,
				u8 *buf);
extern int sec_rtc_update(struct sec_pmic_dev *sec_pmic, u32 reg, u32 val,
				u32 mask);

extern int s2m_set_vth(struct regulator *reg, bool enable);
extern void s2m_init_dvs(void);
extern int s2m_get_dvs_is_enable(void);
extern int s2m_get_dvs_is_on(void);
extern int s2m_set_dvs_pin(bool gpio_val);
extern int s2m_set_g3d_pin(bool gpio_val);
extern void sec_core_lock(void);
extern void sec_core_unlock(void);
void g3d_pin_config_set(void);

/**
 * sec_regulator_data - regulator data
 * @id: regulator id
 * @initdata: regulator init data (contraints, supplies, ...)
 */
struct sec_regulator_data {
	int				id;
	struct regulator_init_data	*initdata;
	struct device_node *reg_node;
};

/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */
struct sec_opmode_data {
	int id;
	unsigned int mode;
};

/*
 * samsung regulator operation mode
 * SEC_OPMODE_OFF	Regulator always OFF
 * SEC_OPMODE_ON	Regulator always ON
 * SEC_OPMODE_LOWPOWER  Regulator is on in low-power mode
 * SEC_OPMODE_SUSPEND   Regulator is changed by PWREN pin
 *			If PWREN is high, regulator is on
 *			If PWREN is low, regulator is off
 */

enum sec_opmode {
	SEC_OPMODE_OFF,
	SEC_OPMODE_SUSPEND,
	SEC_OPMODE_LOWPOWER,
	SEC_OPMODE_ON,
	SEC_OPMODE_MIF = 0x2,
};

#endif /*  __LINUX_MFD_SEC_CORE_H */
