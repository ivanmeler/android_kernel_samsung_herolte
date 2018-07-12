/*
 * max77833-muic.c - MUIC driver for the Maxim 77833
 *
 *  Copyright (C) 2015 Samsung Electronics
 *  Insun Choi <insun77.choi@samsung.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#include <linux/mfd/max77833.h>
#include <linux/mfd/max77833-private.h>

/* MUIC header file */
#include <linux/muic/muic.h>
#include <linux/muic/max77833-muic.h>
#include <linux/muic/max77833-muic-hv.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

void max77833_muic_hv_qc_autoset(struct max77833_muic_data *muic_data, u8 val, u8 mask)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_QC_AUTOSET_WRITE;
	u8 reg = MAX77833_MUIC_REG_DAT_IN1;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	init_muic_cmd_data(&cmd_data);

	cmd_data.opcode = opcode;
	cmd_data.reg = reg; 
	cmd_data.val = val; 
	cmd_data.mask = mask;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

void max77833_muic_hv_qc_enable(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_QC_ENABLE_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

void max77833_muic_hv_qc_disable(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_QC_DISABLE_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

void max77833_muic_hv_chgin_read(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_CHGIN_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

void max77833_muic_hv_fchv_capa_read(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_AFC_CAPA_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg; 

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

void max77833_muic_hv_fchv_set(struct max77833_muic_data *muic_data, u8 val, u8 mask)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_AFC_SET_WRITE;
	u8 reg = MAX77833_MUIC_REG_DAT_IN1;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;
	cmd_data.reg = reg; 
	cmd_data.val = val;
	cmd_data.mask = mask;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

void max77833_muic_hv_fchv_enable(struct max77833_muic_data *muic_data, u8 val, u8 mask)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_AFC_ENABLE_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_IN1;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;
	cmd_data.reg = reg; 
	cmd_data.val = val;
	cmd_data.mask = mask;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

void max77833_muic_hv_fchv_disable(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_AFC_DISABLE_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg; 

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

void max77833_muic_set_afc_ready(struct max77833_muic_data *muic_data, bool value)
{
	bool before, after;

	before = muic_data->is_check_hv;
	muic_data->is_check_hv = value;
	after = muic_data->is_check_hv;

	pr_info("%s:%s check_hv[%d->%d]\n", MUIC_DEV_NAME, __func__, before, after);
}

void max77833_muic_hv_fchv_disable_set(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	max77833_muic_set_afc_ready(muic_data, false);
	max77833_muic_hv_fchv_disable(muic_data);

	return;
}

void max77833_muic_hv_qc_disable_set(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	max77833_muic_set_afc_ready(muic_data, false);
	max77833_muic_hv_qc_disable(muic_data);

	return;
}
