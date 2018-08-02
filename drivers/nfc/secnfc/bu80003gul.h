/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _BU80003GUL_H
#define _BU80003GUL_H

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/i2c.h>

/* Common constants definition */
#define BU80003GUL_I2C_NAME "felica_i2c"
#define FELICA_EPC_MAJOR	10 /* same as misc device */
#define FELICA_EPC_BASEMINOR  0
#define FELICA_EPC_MINOR 0
#define FELICA_EPC_MINOR_COUNT 1

/* just to keep the notation same as of previous models */
#define FELICA_EPC_NAME "felica_ant"

static struct class *eeprom_class;

/* Function prototypes */
static int felica_epc_register(void);
static void felica_epc_deregister(void);
static int felica_epc_open(struct inode *inode, struct file *file);
static int felica_epc_close(struct inode *inode, struct file *file);
static ssize_t felica_epc_read(struct file *file,
		char __user *buf, size_t len, loff_t *ppos);
static ssize_t felica_epc_write(struct file *file,
		const char __user *data, size_t len, loff_t *ppos);

/* I2C related function prototypes
static int bu80003gul_i2c_init(void); */
static int bu80003gul_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *devid);
static int bu80003gul_i2c_remove(struct i2c_client *client);
/* static void bu80003gul_i2c_exit(void); */

#endif /* _BU80003GUL_H */
