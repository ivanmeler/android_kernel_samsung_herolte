/*
 * include/linux/mfd/s2mu003.h
 *
 * Driver to Richtek S2MU003
 * Multi function device -- Charger / Battery Gauge / DCDC Converter / LED Flashlight
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef S2MU003_H
#define S2MU003_H
/* delete below header caused it is only for this module*/
/*#include <linux/rtdefs.h> */

#include <linux/mutex.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/kernel.h>

#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <linux/battery/sec_charging_common.h>

#define S2MU003_DRV_VER "1.1.8_S"

#ifdef CONFIG_S2MU003_SADDR
#define S2MU003FG_SLAVE_ADDR_MSB (0x40)
#else
#define S2MU003FG_SLAVE_ADDR_MSB (0x00)
#endif

#define S2MU003FG_SLAVE_ADDR (0x35|S2MU003FG_SLAVE_ADDR_MSB)
#define S2MU003_SLAVE_ADDR   (0x34|S2MU003FG_SLAVE_ADDR_MSB)

#define S2MU003_CHG_IRQ_REGS_NR 3
#define S2MU003_LED_IRQ_REGS_NR 1
#define S2MU003_PMIC_IRQ_REGS_NR 1

#define S2MU003_IRQ_REGS_NR \
    (S2MU003_CHG_IRQ_REGS_NR + \
     S2MU003_LED_IRQ_REGS_NR + \
     S2MU003_PMIC_IRQ_REGS_NR)

#define S2MU003_DECLARE_IRQ(irq) { \
	irq, irq, \
	irq##_NAME, IORESOURCE_IRQ }

#define S2MU003_OF_COMPATIBLE_LDO_SAFE "samsung,s2mu003-safeldo"
#define S2MU003_OF_COMPATIBLE_LDO1 "samsung,s2mu003-ldo1"
#define S2MU003_OF_COMPATIBLE_DCDC1 "samsung,s2mu003-dcdc1"

enum {
	S2MU003_ID_LDO_SAFE = 0,
	S2MU003_ID_LDO1,
	S2MU003_ID_DCDC1,
	S2MU003_MAX_REGULATOR,
};

typedef union s2mu003_irq_status {
	struct {
		uint8_t chg_irq_status[S2MU003_CHG_IRQ_REGS_NR];
		uint8_t fled_irq_status[S2MU003_LED_IRQ_REGS_NR];
		uint8_t pmic_irq_status[S2MU003_PMIC_IRQ_REGS_NR];
	};
	struct {
		uint8_t regs[S2MU003_IRQ_REGS_NR];
	};
} s2mu003_irq_status_t;

typedef union s2mu003_pmic_shdn_ctrl {
	struct {
		uint8_t reserved:2;
		uint8_t buck_ocp_enshdn:1;
		uint8_t buck_lv_enshdn:1;
		uint8_t sldo_lv_enshdn:1;
		uint8_t ldo_lv_enshdn:1;
		uint8_t ot_enshdn:1;
		uint8_t vdda_uv_enshdn:1;
	} shdn_ctrl1;
	uint8_t shdn_ctrl[1];
} s2mu003_pmic_shdn_ctrl_t;


typedef struct s2mu003_regulator_platform_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
} s2mu003_regulator_platform_data_t;


struct s2mu003_fled_platform_data;

typedef struct s2mu003_charger_platform_data {
	sec_charging_current_t *charging_current_table;
	int chg_float_voltage;
	char *charger_name;
	uint32_t is_750kHz_switching:1;
	uint32_t is_fixed_switching:1;
} s2mu003_charger_platform_data_t;

struct s2mu003_mfd_platform_data {
	s2mu003_regulator_platform_data_t *regulator_platform_data;
	struct s2mu003_fled_platform_data *fled_platform_data;
	int irq_gpio;
	int irq_base;
	int num_regulators;
	s2mu003_regulator_platform_data_t *regulators;
#ifdef CONFIG_CHARGER_S2MU003
    s2mu003_charger_platform_data_t *charger_platform_data;
#endif
};

#define s2mu003_mfd_platform_data_t \
	struct s2mu003_mfd_platform_data

struct s2mu003_charger_data;

struct s2mu003_mfd_chip {
	struct i2c_client *i2c_client;
	struct device *dev;
	s2mu003_mfd_platform_data_t *pdata;
    int irq_base;
	struct mutex io_lock;
    struct mutex irq_lock;
	struct wake_lock irq_wake_lock;
	/* prev IRQ status and now IRQ_status*/
	s2mu003_irq_status_t irq_status[2];
	/* irq_status_index ^= 0x01; after access irq*/
	int irq_status_index;
	int irq;
    uint8_t irq_masks_cache[S2MU003_IRQ_REGS_NR];
	int suspend_flag;
	struct s2mu003_charger_data *charger;

#ifdef CONFIG_FLED_S2MU003
	struct s2mu003_fled_info *fled_info;
#endif
#ifdef CONFIG_REGULATOR_S2MU003
	struct s2mu003_regulator_info *regulator_info[S2MU003_MAX_REGULATOR];
#endif
};

#define s2mu003_mfd_chip_t \
	struct s2mu003_mfd_chip

extern int s2mu003_block_read_device(struct i2c_client *i2c,
		int reg, int bytes, void *dest);

extern int s2mu003_block_write_device(struct i2c_client *i2c,
		int reg, int bytes, void *src);

extern int s2mu003_reg_read(struct i2c_client *i2c, int reg_addr);
extern int s2mu003_reg_write(struct i2c_client *i2c, int reg_addr, u8 data);
extern int s2mu003_assign_bits(struct i2c_client *i2c, int reg_addr, u8 mask,
		u8 data);
extern int s2mu003_set_bits(struct i2c_client *i2c, int reg_addr, u8 mask);
extern int s2mu003_clr_bits(struct i2c_client *i2c, int reg_addr, u8 mask);

typedef enum {
	S2MU003_PREV_STATUS = 0,
	S2MU003_NOW_STATUS } s2mu003_irq_status_sel_t;

extern s2mu003_irq_status_t *s2mu003_get_irq_status(s2mu003_mfd_chip_t *mfd_chip,
		s2mu003_irq_status_sel_t sel);
extern int s2mu003_init_irq(s2mu003_mfd_chip_t *chip);
extern int s2mu003_exit_irq(s2mu003_mfd_chip_t *chip);

#endif /* S2MU003_H */
