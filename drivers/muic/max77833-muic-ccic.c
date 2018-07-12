/*
 * max77833-ccic.c - MUIC-CCIC driver for the Maxim 77833
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
#include <linux/string.h>
#include <linux/platform_device.h>

#include <linux/mfd/max77833.h>
#include <linux/mfd/max77833-private.h>

/* MUIC header file */
#include <linux/muic/muic.h>
#include <linux/muic/max77833-muic.h>
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
#include <linux/muic/max77833-muic-hv.h>
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/muic/ccic_notifier.h>
#endif /* CONFIG_CCIC_NOTIFIER */

/* Enable this feature not to use cmd_q for a cc notification */
#define MUIC_USE_DIRECT_CMD

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
static int muic_handle_cc_supported_dev(struct max77833_muic_data *pmuic, muic_attached_dev_t new_dev);
#endif

#if !defined(CONFIG_MUIC_DISABLE_INTERRUPT)
static int muic_is_cc_supported_jig(struct max77833_muic_data *pmuic, muic_attached_dev_t new_dev);
#endif

extern struct muic_platform_data muic_pdata;
static bool debug_en_vps = false;
static bool debug_en_cmd = false;
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
u8 vi_val = FCHV_SET_9V;
#endif
u8 pass_rev; // For PASS4/Old rev onebinary

struct max77833_muic_vps_data {
	max77833_adc_t			adc;
	chgdetcon_t			chgdetcon;
	chgdetrun_t			chgdetrun;
	chgtyp_t			chgtyp;
	max77833_switch_cmd_t		muic_switch;
	const char			*vps_name;
	const muic_attached_dev_t	attached_dev;
};

static const struct max77833_muic_vps_data muic_vps_table[] = {
// Default.
	{
		.adc		= MAX77833_ADC_JIG_UART_OFF,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.muic_switch	= COM_UART,
		.vps_name	= "Jig UART Off",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_MUIC,
	},
	{
		.adc		= MAX77833_ADC_JIG_UART_OFF,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_HALT,
		.muic_switch	= COM_UART,
		.vps_name	= "Jig UART Off + VB",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_VB_MUIC,
	},
	{
		.adc		= MAX77833_ADC_JIG_UART_ON,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_UART,
		.vps_name	= "Jig UART On",
		.attached_dev	= ATTACHED_DEV_JIG_UART_ON_MUIC,
	},
	{
		.adc		= MAX77833_ADC_JIG_USB_OFF,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Unofficial ID",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_MUIC,
	},
	{
		.adc		= MAX77833_ADC_JIG_USB_ON,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "Jig USB On",
		.attached_dev	= ATTACHED_DEV_JIG_USB_ON_MUIC,
	},
	{
		.adc		= MAX77833_ADC_OPEN,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "TA",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
	{
		.adc		= MAX77833_ADC_OPEN,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_USB,
		.muic_switch	= COM_USB,
		.vps_name	= "USB",
		.attached_dev	= ATTACHED_DEV_USB_MUIC,
	},
// For TA or Charging etc.
	{
		.adc		= MAX77833_ADC_OPEN,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_UNOFFICIAL_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Unofficial TA",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_TA_MUIC,
	},
	{
		.adc		= MAX77833_ADC_CEA936ATYPE2_CHG,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "TYPE2 Charger",
		.attached_dev	= ATTACHED_DEV_TYPE2_CHG_MUIC,
	},
	{
		.adc		= MAX77833_ADC_UNDEFINED,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Undefined Charging",
		.attached_dev	= ATTACHED_DEV_UNDEFINED_CHARGING_MUIC,
	},
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	{
		.adc            = MAX77833_ADC_OPEN,
		.chgdetrun      = CHGDETRUN_FALSE,
		.chgtyp         = CHGTYP_DEDICATED_CHARGER,
		.muic_switch    = COM_OPEN,
		.vps_name       = "AFC charger",
		.attached_dev   = ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	},
	{
		.adc            = MAX77833_ADC_OPEN,
		.chgdetrun      = CHGDETRUN_FALSE,
		.chgtyp         = CHGTYP_DEDICATED_CHARGER,
		.muic_switch    = COM_OPEN,
		.vps_name       = "QC charger",
		.attached_dev   = ATTACHED_DEV_QC_CHARGER_9V_MUIC,
	},
#endif
// For Accessary.
	{
		.adc		= MAX77833_ADC_OPEN,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_CDP,
		.muic_switch	= COM_USB,
		.vps_name	= "CDP",
		.attached_dev	= ATTACHED_DEV_CDP_MUIC,
	},
	{
		.adc		= MAX77833_ADC_1K,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "MHL",
		.attached_dev	= ATTACHED_DEV_MHL_MUIC,
	},
	{
		.adc		= MAX77833_ADC_GND,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "OTG",
		.attached_dev	= ATTACHED_DEV_OTG_MUIC,
	},
	{
		.adc		= MAX77833_ADC_CHARGING_CABLE,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "PowerSharing Cable",
		.attached_dev	= ATTACHED_DEV_CHARGING_CABLE_MUIC,
	},
	{
		.adc		= MAX77833_ADC_HMT,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "HMT",
		.attached_dev	= ATTACHED_DEV_HMT_MUIC,
	},
	{
		.adc		= MAX77833_ADC_DESKDOCK,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Deskdock",
		.attached_dev	= ATTACHED_DEV_DESKDOCK_MUIC,
	},
	{
		.adc		= MAX77833_ADC_SMARTDOCK,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_NO_VOLTAGE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Smartdock",
		.attached_dev	= ATTACHED_DEV_SMARTDOCK_MUIC,
	},
	{
		.adc		= MAX77833_ADC_SMARTDOCK,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.muic_switch	= COM_USB_DOCK,
		.vps_name	= "Smartdock + TA",
		.attached_dev	= ATTACHED_DEV_SMARTDOCK_TA_MUIC,
	},
	{
		.adc		= MAX77833_ADC_SMARTDOCK,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_USB,
		.muic_switch	= COM_USB_DOCK,
		.vps_name	= "Smartdock + USB",
		.attached_dev	= ATTACHED_DEV_SMARTDOCK_USB_MUIC,
	},
	{
		.adc		= MAX77833_ADC_AUDIODOCK,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "Audiodock",
		.attached_dev	= ATTACHED_DEV_AUDIODOCK_MUIC,
	},
	{
		.adc		= MAX77833_ADC_UNIVERSAL_MMDOCK,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_USB,
		.vps_name	= "Universal Multimedia dock",
		.attached_dev	= ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC,
	},
	{
		.adc		= MAX77833_ADC_USB_LANHUB,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "USB LANHUB",
		.attached_dev	= ATTACHED_DEV_USB_LANHUB_MUIC,
	},
// For support 219Kohm
	{
		.adc		= MAX77833_ADC_219,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Unofficial ID + TA",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC,
	},
	{
		.adc		= MAX77833_ADC_219,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_CDP,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Unofficial ID + CDP",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC,
	},
	{
		.adc		= MAX77833_ADC_219,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_UNOFFICIAL_CHARGER,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Unofficial ID + ANY TA",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC,
	},
	{
		.adc		= MAX77833_ADC_219,
		.chgdetrun	= CHGDETRUN_FALSE,
		.chgtyp		= CHGTYP_USB,
		.muic_switch	= COM_OPEN,
		.vps_name	= "Unofficial ID + USB",
		.attached_dev	= ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC,
	},
// For VZW.
	{
		.adc		= MAX77833_ADC_RESERVED_VZW,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "VZW Accessory",
		.attached_dev	= ATTACHED_DEV_VZW_ACC_MUIC,
	},
	{
		.adc		= MAX77833_ADC_INCOMPATIBLE_VZW,
		.chgdetrun	= CHGDETRUN_DONTCARE,
		.chgtyp		= CHGTYP_DONTCARE,
		.muic_switch	= COM_OPEN,
		.vps_name	= "VZW Incompatible",
		.attached_dev	= ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC,
	},
};

static int muic_lookup_vps_table(muic_attached_dev_t new_dev, struct max77833_muic_data *muic_data)
{
	int i;
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_data;

	max77833_read_reg(i2c, MAX77833_MUIC_REG_STATUS3, &reg_data);
	reg_data = reg_data & STATUS3_SYSMSG_MASK;
	pr_info("%s:%s Last sysmsg = 0x%02x\n", MUIC_DEV_NAME, __func__, reg_data);

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		const struct max77833_muic_vps_data *tmp_vps;
		tmp_vps = &(muic_vps_table[i]);

		if (tmp_vps->attached_dev != new_dev)
			continue;

		pr_info("%s:%s (%d) vps table match found at i(%d), %s\n",
				MUIC_DEV_NAME, __func__, new_dev, i,
				tmp_vps->vps_name);

		return i;
	}

	pr_info("%s:%s can't find (%d) on vps table\n", MUIC_DEV_NAME,
			__func__, new_dev);

	return -ENODEV;
}

static bool muic_check_support_dev(struct max77833_muic_data *muic_data,
			muic_attached_dev_t attached_dev)
{
	bool ret = muic_data->muic_support_list[attached_dev];

	if (debug_en_vps)
		pr_info("%s:%s [%c]\n", MUIC_DEV_NAME, __func__, ret ? 'T':'F');

	return ret;
}

static int max77833_muic_write_reg
		(struct i2c_client *i2c, u8 reg, u8 value, bool debug_en)
{
	int ret = max77833_write_reg(i2c, reg, value);

	if (debug_en)
		pr_info("%s:%s Reg[0x%02x]: 0x%02x\n",
					MUIC_DEV_NAME, __func__, reg, value);

	return ret;
}

void init_muic_cmd_data(muic_cmd_data *cmd_data)
{
	if (debug_en_cmd)
		pr_info("%s:%s \n", MUIC_DEV_NAME, __func__);

	cmd_data->opcode = COMMAND_NONE;
	cmd_data->response = COMMAND_NONE;
	cmd_data->read_data = REG_NONE;
	cmd_data->write_data = REG_NONE;
	cmd_data->val = REG_NONE;
	cmd_data->mask = REG_NONE;
	cmd_data->reg = REG_NONE;
	cmd_data->noti_dev = ATTACHED_DEV_UNKNOWN_MUIC;

	return;
}

static void init_muic_command(muic_cmd_node *muic_cmd_node)
{
	muic_cmd_data *cmd_data = &(muic_cmd_node->cmd_data);

	if (debug_en_cmd)
		pr_info("%s:%s \n", MUIC_DEV_NAME, __func__);

	muic_cmd_node->next = NULL;

	init_muic_cmd_data(cmd_data);
	return;
}

static void copy_muic_cmd_data(muic_cmd_data *from, muic_cmd_data *to)
{
	to->opcode = from->opcode;
	to->response = from->response;
	to->read_data = from->read_data;
	to->write_data = from->write_data;
	to->reg = from->reg;
	to->mask = from->mask;
	to->val = from->val;

	to->noti_dev = from->noti_dev;
}

bool is_empty_muic_cmd_queue(cmd_queue_t *muic_cmd_queue)
{
	bool ret = false;

	if (muic_cmd_queue->front == NULL)
		ret = true;

	if (ret)
		pr_info("%s:%s muic_cmd_queue Empty(%c)\n",
			MUIC_DEV_NAME, __func__, ret ? 'T' : 'F');

	return ret;
}

void enqueue_muic_cmd(cmd_queue_t *muic_cmd_queue, muic_cmd_data cmd_data)
{
	muic_cmd_node	*temp_node = kzalloc(sizeof(muic_cmd_node), GFP_KERNEL);

	if (!temp_node) {
		pr_err("%s: failed to allocate muic command queue\n", __func__);
		return;
	}

	init_muic_command(temp_node);

//	mutex_lock(&muic_cmd_queue->command_mutex);

	copy_muic_cmd_data(&cmd_data, &(temp_node->cmd_data));

	if (is_empty_muic_cmd_queue(muic_cmd_queue)) {
		muic_cmd_queue->front = temp_node;
		muic_cmd_queue->rear = temp_node;
	} else {
		muic_cmd_queue->rear->next = temp_node;
		muic_cmd_queue->rear = temp_node;
	}

//	mutex_unlock(&muic_cmd_queue->command_mutex);
}

static void enqueue_muic_notifier(cmd_queue_t *cmd_queue,
		muic_cmd_opcode opcode, muic_attached_dev_t noti_dev)
{
	muic_cmd_data cmd_data;

	pr_info("%s:%s opcode[0x%02x] noti_dev[%d]\n",
		MUIC_DEV_NAME, __func__, opcode, noti_dev);

	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;
	cmd_data.noti_dev = noti_dev;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

static void dequeue_muic_cmd
	(cmd_queue_t *muic_cmd_queue, muic_cmd_data *cmd_data)
{
	muic_cmd_node *temp_node;

	if (is_empty_muic_cmd_queue(muic_cmd_queue)) {
		pr_err("%s:%s Queue, Empty!\n", MUIC_DEV_NAME, __func__);
		return;
	}

	temp_node = muic_cmd_queue->front;
	copy_muic_cmd_data(&(temp_node->cmd_data), cmd_data);

	pr_info("%s:%s Opcode(0x%02x)\n",
		MUIC_DEV_NAME, __func__, cmd_data->opcode);

	/* debugging */
	if (muic_cmd_queue->front->next == NULL) {
		pr_info("%s:%s front->next = NULL\n", MUIC_DEV_NAME, __func__);
		muic_cmd_queue->front = NULL;
	} else
		muic_cmd_queue->front = muic_cmd_queue->front->next;

	if (is_empty_muic_cmd_queue(muic_cmd_queue))
		muic_cmd_queue->rear = NULL;

	kfree(temp_node);

	return;
}

static bool front_muic_cmd
	(cmd_queue_t *cmd_queue, muic_cmd_data *cmd_data)
{
	if (is_empty_muic_cmd_queue(cmd_queue)) {
		pr_err("%s:%s Queue, Empty!\n", MUIC_DEV_NAME, __func__);
		return false;
	}

	copy_muic_cmd_data(&(cmd_queue->front->cmd_data), cmd_data);
	pr_info("%s:%s Opcode(0x%02x)\n",
		MUIC_DEV_NAME, __func__, cmd_data->opcode);

	return true;
}

static u8 read_muic_cmd_response(struct max77833_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 response, reg;

	reg = MAX77833_MUIC_REG_DAT_OUT_OP;
	max77833_read_reg(i2c, reg, &response);
	pr_info("%s:%s Response[0x%02x] = 0x%02x\n",
		MUIC_DEV_NAME, __func__, reg, response);

	return response;
}

static bool is_read_response(u8 response, u8 *reg)
{
	bool read;

	switch (response) {
	case COMMAND_CONFIG_READ:
	case COMMAND_SWITCH_READ:
	case COMMAND_SYSMSG_READ:
	case COMMAND_CHGDET_READ:
	case COMMAND_MONITOR_READ:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	case COMMAND_QC_DISABLE_READ:
	case COMMAND_QC_ENABLE_READ:
	case COMMAND_QC_AUTOSET_WRITE:
	case COMMAND_AFC_DISABLE_READ:
	case COMMAND_AFC_ENABLE_READ:
	case COMMAND_AFC_CAPA_READ:
	case COMMAND_AFC_SET_WRITE:
#endif
	case COMMAND_CHGIN_READ:
		*reg = MAX77833_MUIC_REG_DAT_OUT1;
		read = true;
		break;
	case COMMAND_CONFIG_WRITE:
	case COMMAND_SWITCH_WRITE:
	case COMMAND_MONITOR_WRITE:
		read = false;
		break;
	default:
		pr_err("%s:%s Invalid CMD response[0x%02x]\n",
			MUIC_DEV_NAME, __func__, response);
		break;
	}

	return read;
}

static bool is_read_opcode(u8 opcode)
{
	bool read;

	switch (opcode) {
	case COMMAND_CONFIG_READ:
	case COMMAND_SWITCH_READ:
	case COMMAND_SYSMSG_READ:
	case COMMAND_CHGDET_READ:
	case COMMAND_MONITOR_READ:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	case COMMAND_QC_DISABLE_READ:
	case COMMAND_QC_ENABLE_READ:
	case COMMAND_AFC_DISABLE_READ:
	case COMMAND_AFC_CAPA_READ:
#endif
	case COMMAND_CHGIN_READ:
		read = true;
		break;
	case COMMAND_CONFIG_WRITE:
	case COMMAND_SWITCH_WRITE:
	case COMMAND_MONITOR_WRITE:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	case COMMAND_AFC_ENABLE_READ:
	case COMMAND_QC_AUTOSET_WRITE:
	case COMMAND_AFC_SET_WRITE:
#endif
		read = false;
		break;
	default:
		pr_err("%s:%s Invalid CMD Opcode[%d]\n",
			MUIC_DEV_NAME, __func__, opcode);
		break;
	}

	return read;
}

static bool is_notifier_opcode(u8 opcode)
{
	bool noti;

	switch (opcode) {
	case NOTI_ATTACH:
	case NOTI_DETACH:
	case NOTI_LOGICALLY_ATTACH:
	case NOTI_LOGICALLY_DETACH:
		noti = true;
		break;
	case COMMAND_CONFIG_READ:
	case COMMAND_CONFIG_WRITE:
	case COMMAND_SWITCH_READ:
	case COMMAND_SWITCH_WRITE:
	case COMMAND_SYSMSG_READ:
	case COMMAND_CHGDET_READ:
	case COMMAND_MONITOR_READ:
	case COMMAND_MONITOR_WRITE:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	case COMMAND_QC_DISABLE_READ:
	case COMMAND_QC_ENABLE_READ:
	case COMMAND_QC_AUTOSET_WRITE:
	case COMMAND_AFC_DISABLE_READ:
	case COMMAND_AFC_ENABLE_READ:
	case COMMAND_AFC_SET_WRITE:
	case COMMAND_AFC_CAPA_READ:
#endif
	case COMMAND_CHGIN_READ:
		noti = false;
		break;
	default:
		pr_err("%s:%s Invalid Opcode[%d]\n",
			MUIC_DEV_NAME, __func__, opcode);
		break;
	}

	return noti;
}

static u8 get_reg_written_opcode(u8 opcode)
{
	u8 reg = REG_NONE;

	switch (opcode) {
	case COMMAND_CONFIG_READ:
	case COMMAND_CONFIG_WRITE:
	case COMMAND_SWITCH_READ:
	case COMMAND_SWITCH_WRITE:
	case COMMAND_MONITOR_READ:
	case COMMAND_MONITOR_WRITE:
	case COMMAND_CHGDET_READ:
	case COMMAND_SYSMSG_READ:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	case COMMAND_QC_DISABLE_READ:
	case COMMAND_QC_ENABLE_READ:
	case COMMAND_QC_AUTOSET_WRITE:
	case COMMAND_AFC_DISABLE_READ:
	case COMMAND_AFC_ENABLE_READ:
	case COMMAND_AFC_SET_WRITE:
	case COMMAND_AFC_CAPA_READ:
#endif
	case COMMAND_CHGIN_READ:
		reg = MAX77833_MUIC_REG_DAT_IN_OP;
		break;
	default:
		pr_err("%s:%s Invalid Opcode[%d]\n",
			MUIC_DEV_NAME, __func__, opcode);
		break;
	}

	return reg;
}

static void max77833_muic_cmd_write_lastopcode
	(struct max77833_muic_data *muic_data, u8 opcode)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg;
	int ret;

	reg = MAX77833_MUIC_REG_DAT_IN8;
	ret = max77833_muic_write_reg(i2c, reg, opcode, REG_NONE);
	if (ret)
		pr_err("%s:%s Cannot write reg[0x%02x], [%d]\n",
			MUIC_DEV_NAME, __func__, reg, opcode);

	return;
}

static void max77833_muic_cmd_write_opcode
	(struct max77833_muic_data *muic_data, u8 opcode, bool read)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg;
	int ret;

	reg = get_reg_written_opcode(opcode);
	ret = max77833_muic_write_reg(i2c, reg, opcode, true);
	if (ret)
		pr_err("%s:%s Cannot write reg[0x%02x], [%d]\n",
			MUIC_DEV_NAME, __func__, reg, opcode);
	if (!!read)
		max77833_muic_cmd_write_lastopcode(muic_data, opcode);

	return;
}

static void calculate_write_data(muic_cmd_data *cmd_data)
{
	cmd_data->write_data = (cmd_data->val & cmd_data->mask) | \
				(cmd_data->read_data & (~cmd_data->mask));

	pr_info("%s:%s R-data[0x%02x] + [0x%02x:0x%02x] = W-data[0x%02x]\n",
		MUIC_DEV_NAME, __func__, cmd_data->read_data,
		cmd_data->val, cmd_data->mask, cmd_data->write_data);

	return;
}

static void max77833_muic_cmd_write_reg
	(struct max77833_muic_data *muic_data, muic_cmd_data *cmd_data)
{
	struct i2c_client *i2c = muic_data->i2c;

	calculate_write_data(cmd_data);

#if 0 /* MAXIM - CHECK ME!!! */
	if (cmd_data->read_data == cmd_data->write_data) {
		pr_info("%s:%s Opcode[0x%02x]:[0x%02x]->[0x%02x], not write\n",
			MUIC_DEV_NAME, __func__, (u8)cmd_data->opcode,
			cmd_data->read_data, cmd_data->write_data);
		return;
	}
#endif

	max77833_muic_write_reg(i2c, cmd_data->reg, cmd_data->write_data, true);

	pr_info("%s:%s Opcode[0x%02x]:[0x%02x]->[0x%02x]\n",
		MUIC_DEV_NAME, __func__, (u8)cmd_data->opcode,
		cmd_data->read_data, cmd_data->write_data);

	return;
}

static bool is_read_write_muic_cmd
	(cmd_queue_t *muic_cmd_queue, u8 response)
{
	muic_cmd_data next_cmd_data;
	u8 next_opcode;
	bool ret = false;

	if (!front_muic_cmd(muic_cmd_queue, &next_cmd_data))
		return ret;

	next_opcode = next_cmd_data.opcode;

	switch (response) {
	case COMMAND_CONFIG_READ:
		if (next_opcode == COMMAND_CONFIG_WRITE)
			ret = true;
		break;
	case COMMAND_SWITCH_READ:
		if (next_opcode == COMMAND_SWITCH_WRITE)
			ret = true;
		break;
	case COMMAND_MONITOR_READ:
		if (next_opcode == COMMAND_MONITOR_WRITE)
			ret = true;
		break;
	case COMMAND_SYSMSG_READ:
	case COMMAND_CHGDET_READ:
		break;
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	case COMMAND_QC_ENABLE_READ:
		if (next_opcode == COMMAND_QC_AUTOSET_WRITE)
			ret = true;
		break;
	case COMMAND_AFC_ENABLE_READ:
	case COMMAND_AFC_CAPA_READ:
		if (next_opcode == COMMAND_AFC_SET_WRITE)
			ret = true;
		break;
#endif
	default:
		pr_err("%s:%s Invalid Res[0x%02x], n-Opcode[0x%02x]\n",
			MUIC_DEV_NAME, __func__, response, next_opcode);
		break;
	}

	pr_info("%s:%s Res[0x%02x]->n-Opcode[0x%02x],RW [%c]\n",
		MUIC_DEV_NAME, __func__, response, next_opcode,
		(ret ? 'T' : 'F'));

	return ret;
}

static int write_vps_regs(struct max77833_muic_data *muic_data, muic_attached_dev_t new_dev);
static int max77833_muic_init_regs(struct max77833_muic_data *muic_data);
static void max77833_muic_free_irqs(struct max77833_muic_data *muic_data);
int max77833_muic_handle_detach(struct max77833_muic_data *muic_data);
void max77833_muic_set_idmode_oneshot(struct max77833_muic_data *muic_data);
void max77833_muic_set_idmode_continuous(struct max77833_muic_data *muic_data);

static void muic_cmd_run(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	struct i2c_client *i2c = muic_data->i2c;
	muic_cmd_node *run_node;
	muic_cmd_data cmd_data;

	run_node = kzalloc(sizeof(muic_cmd_node), GFP_KERNEL);
	if (!run_node) {
		pr_err("%s: failed to allocate muic command queue\n", __func__);
		return;
	}

	init_muic_command(run_node);

	cmd_data = run_node->cmd_data;

	if (is_empty_muic_cmd_queue(cmd_queue)) {
		pr_err("%s:%s Queue, Empty!\n", MUIC_DEV_NAME, __func__);
		kfree(run_node);
		return;
	}

	dequeue_muic_cmd(cmd_queue, &cmd_data);

	if (is_notifier_opcode(cmd_data.opcode)) {
		muic_attached_dev_t noti_dev = cmd_data.noti_dev;

		switch (cmd_data.opcode) {
		case NOTI_ATTACH:
			muic_notifier_attach_attached_dev(noti_dev);
			muic_data->attached_dev = noti_dev;
			break;
		case NOTI_DETACH:
			muic_notifier_detach_attached_dev(noti_dev);
			muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
			break;
		case NOTI_LOGICALLY_ATTACH:
			muic_notifier_logically_attach_attached_dev(noti_dev);
			break;
		case NOTI_LOGICALLY_DETACH:
			muic_notifier_logically_detach_attached_dev(noti_dev);
			break;
		default:
			pr_info("%s:%s Invalid notifier type[%d]\n",
				MUIC_DEV_NAME, __func__, cmd_data.opcode);
			break;
		}

		muic_cmd_run(muic_data);
	} else if (cmd_data.opcode == COMMAND_NONE) {	/* Apcmdres isr */
		u8 response = read_muic_cmd_response(muic_data);
		u8 read_reg, read_data, reg_data[5];
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
		int chgin, val;
#endif
		u8 muic_irq_mask[3] = {};
		u8 reset_val = 0x0;

		if (response != cmd_data.response) {
			pr_err("%s:%s Response [0x%02x] != [0x%02x]\n",
				MUIC_DEV_NAME, __func__,
				response, cmd_data.response);
		}
		if (is_read_response(response, &read_reg)) {
			max77833_read_reg(i2c, read_reg, &read_data);
			pr_info("%s:%s Reg[0x%02x] = [0x%02x]\n",
				MUIC_DEV_NAME, __func__, read_reg, read_data);

			if (is_read_write_muic_cmd(cmd_queue, response)) {
				cmd_queue->front->cmd_data.read_data = read_data;
			}

			switch (response) {
			case COMMAND_MONITOR_READ:	// For ACA issue;
				pr_info("%s: %s ID MONITOR: [0x%02x]\n", MUIC_DEV_NAME, __func__, read_data);
				read_data &= 0x10;
				if (read_data > 0)
					panic("ACA mode panic!!!");
				break;
			case COMMAND_SYSMSG_READ:
				/* For Reset problem */
				if (read_data == 0x05) {
					mutex_lock(&muic_data->muic_mutex);
					max77833_bulk_read(muic_data->i2c, MAX77833_MUIC_REG_INTMASK1, 3, muic_irq_mask);
					if ((reset_val == muic_irq_mask[0])
						&& (reset_val == muic_irq_mask[1])
						&& (reset_val == muic_irq_mask[2])) {
						pr_warn("%s:%s MUIC was reset, try re-write MUIC registers\n",
								MUIC_DEV_NAME, __func__);
						max77833_muic_handle_detach(muic_data);
						mutex_unlock(&muic_data->muic_mutex);
						max77833_muic_free_irqs(muic_data);

						mutex_lock(&muic_data->muic_mutex);
						max77833_muic_init_regs(muic_data);
					}
					else
						pr_info("%s:%s MUIC was not reset, just return\n", MUIC_DEV_NAME, __func__);

					mutex_unlock(&muic_data->muic_mutex);
				}

				max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT2, &reg_data[0]);
				max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT3, &reg_data[1]);
				max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT4, &reg_data[2]);
				max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT5, &reg_data[3]);
				max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT6, &reg_data[4]);

				pr_info("%s:%s Check sysmsg: [6]0x%02x [5]0x%02x [4]0x%02x\n", MUIC_DEV_NAME, __func__,
						read_data, reg_data[0], reg_data[1]);
				pr_info("%s:%s Check sysmsg: [3]0x%02x [2]0x%02x [1]0x%02x\n", MUIC_DEV_NAME, __func__,
						reg_data[2], reg_data[3], reg_data[4]);
				break;
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
			case COMMAND_AFC_ENABLE_READ:
				if (read_data == HV_CMD_PASS) {
					vi_val = FCHV_SET_9V;
					max77833_muic_set_idmode_continuous(muic_data);
					max77833_muic_hv_fchv_set(muic_data, vi_val, 0xff);
				}
				else {
					if (read_data != 0x04)
						max77833_muic_set_afc_ready(muic_data, false);
					max77833_muic_hv_fchv_disable(muic_data);
				}
				break;
			case COMMAND_AFC_DISABLE_READ:
				if ((read_data == HV_CMD_PASS) && (muic_data->is_check_hv == true))
					max77833_muic_hv_qc_enable(muic_data);
				else {
					pr_info("%s:%s It's NOT HV Charger.\n", MUIC_DEV_NAME, __func__);
					max77833_muic_set_idmode_oneshot(muic_data);
				}
				break;
			case COMMAND_AFC_SET_WRITE:
	                        if (read_data == HV_CMD_PASS) {
					if (vi_val == FCHV_SET_POWERPACK)
						pr_info("%s:%s POWERPACK ATTACHED.\n",MUIC_DEV_NAME,__func__);
					else
						max77833_muic_hv_chgin_read(muic_data);
				}
				else if (read_data == 0x02)
					max77833_muic_hv_fchv_capa_read(muic_data);
				else if (read_data == 0x03)
					max77833_muic_hv_chgin_read(muic_data);
				else
					max77833_muic_hv_fchv_disable_set(muic_data);
				break;
			case COMMAND_CHGIN_READ:
				max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT2, &reg_data[0]);
				chgin = (int)(reg_data[0] * 794 / 10000);        // Calculate CHGIN Volt.

				if (read_data == HV_CMD_PASS) {
					if ((chgin >= 8) && (chgin <= 9)) {
						pr_info("%s:%s AFC Charger ATTACHED.\n",MUIC_DEV_NAME,__func__);
						max77833_muic_set_afc_ready(muic_data, false);
						val = write_vps_regs(muic_data, ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
						enqueue_muic_notifier(&(muic_data->muic_cmd_queue),
							NOTI_ATTACH, ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
						max77833_muic_set_idmode_oneshot(muic_data);
					}
					else
						max77833_muic_hv_fchv_disable_set(muic_data);
				}
				else
					max77833_muic_hv_fchv_disable_set(muic_data);
				break;
			case COMMAND_AFC_CAPA_READ:
				if (read_data == HV_CMD_PASS) {
					max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT2, &vi_val);
					max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT3, &reg_data[0]);
					max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT4, &reg_data[1]);
					max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT5, &reg_data[2]);
					max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT6, &reg_data[3]);

					for (val = 0; val < 4; val++) {
						if (vi_val < reg_data[val])
							vi_val = reg_data[val];
					}
					max77833_muic_hv_fchv_set(muic_data, vi_val, 0xff);
				}
				else
					max77833_muic_hv_fchv_disable_set(muic_data);
				break;
			case COMMAND_QC_ENABLE_READ:
				if (read_data == HV_CMD_PASS) {
					vi_val = QC_SET_9V;
					max77833_muic_hv_qc_autoset(muic_data, vi_val, 0xff);
				}
				else
					max77833_muic_hv_qc_disable_set(muic_data);
				break;
			case COMMAND_QC_DISABLE_READ:
				if (read_data == HV_CMD_PASS)
					pr_info("%s:%s It's NOT HV Charger.\n", MUIC_DEV_NAME, __func__);
				else
					pr_err("%s:%s Fail HV Invalid State.\n", MUIC_DEV_NAME, __func__);
				max77833_muic_set_idmode_oneshot(muic_data);
				break;
			case COMMAND_QC_AUTOSET_WRITE:
				max77833_read_reg(i2c, MAX77833_MUIC_REG_DAT_OUT2, &reg_data[0]);
				chgin = (int)(reg_data[0] * 794 / 10000);	// Calculate CHGIN Volt.

				if (read_data == HV_CMD_PASS) {
					if ((chgin >= 8) && (chgin <= 9)) {
						pr_info("%s:%s QC Charger ATTACHED.\n",MUIC_DEV_NAME,__func__);
						max77833_muic_set_afc_ready(muic_data, false);
						val = write_vps_regs(muic_data, ATTACHED_DEV_QC_CHARGER_9V_MUIC);
						enqueue_muic_notifier(&(muic_data->muic_cmd_queue),
							NOTI_ATTACH, ATTACHED_DEV_QC_CHARGER_9V_MUIC);
						max77833_muic_set_idmode_oneshot(muic_data);
					}
					else
						max77833_muic_hv_qc_disable_set(muic_data);
				}
				else
					max77833_muic_hv_qc_disable_set(muic_data);
				break;
#endif
			}
		}

		muic_cmd_run(muic_data);
	} else {				/* No isr */
		u8 opcode = cmd_data.opcode;

		if (is_read_opcode(opcode)) {
			max77833_muic_cmd_write_opcode(muic_data, opcode, true);
		}
		else {
			max77833_muic_cmd_write_opcode(muic_data, opcode, false);
			max77833_muic_cmd_write_reg(muic_data, &cmd_data);
			max77833_muic_cmd_write_lastopcode(muic_data, opcode);
		}
	}

	kfree(run_node);

	return;
}

static void max77833_muic_handle_cmd
	(struct max77833_muic_data *muic_data, int irq)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;

	pr_info("%s:%s irq(%d)\n", MUIC_DEV_NAME, __func__, irq);

//	mutex_lock(&(cmd_queue->command_mutex));

	front_muic_cmd(cmd_queue, &cmd_data);

	if (cmd_data.opcode != COMMAND_NONE)
		pr_err("%s:%s Opcode[%d], CHECK!\n",
			MUIC_DEV_NAME, __func__, cmd_data.opcode);

	muic_cmd_run(muic_data);

//	mutex_unlock(&cmd_queue->command_mutex);
}

static void max77833_muic_read_config(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_CONFIG_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;

	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

static void max77833_muic_write_config
	(struct max77833_muic_data *muic_data, u8 val, u8 mask)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_CONFIG_WRITE;
	u8 reg = MAX77833_MUIC_REG_DAT_IN1;

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

static void max77833_muic_read_switch(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_SWITCH_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;

	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

static void max77833_muic_write_switch
	(struct max77833_muic_data *muic_data, u8 val, u8 mask)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_SWITCH_WRITE;
	u8 reg = MAX77833_MUIC_REG_DAT_IN1;

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

static void max77833_muic_read_idmon_config
	(struct max77833_muic_data *muic_data)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_MONITOR_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;

	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	return;
}

static void max77833_muic_write_idmon_config
	(struct max77833_muic_data *muic_data, u8 val, u8 mask)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);

	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_MONITOR_WRITE;
	u8 reg = MAX77833_MUIC_REG_DAT_IN1;

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

static void max77833_muic_read_sysmsg(struct max77833_muic_data *muic_data, int irq)
{
	cmd_queue_t *cmd_queue = &(muic_data->muic_cmd_queue);
	muic_cmd_data cmd_data;
	u8 opcode = COMMAND_SYSMSG_READ;
	u8 reg = MAX77833_MUIC_REG_DAT_OUT1;
	bool empty_q = is_empty_muic_cmd_queue(&(muic_data->muic_cmd_queue));

	pr_info("%s:%s irq(%d)\n", MUIC_DEV_NAME, __func__, irq);

	init_muic_cmd_data(&cmd_data);
	cmd_data.opcode = opcode;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	init_muic_cmd_data(&cmd_data);
	cmd_data.response = opcode;
	cmd_data.reg = reg;

	enqueue_muic_cmd(cmd_queue, cmd_data);

	if (empty_q)
		max77833_muic_handle_cmd(muic_data, -5);

	return;
}

static void com_to_open(struct max77833_muic_data *muic_data)
{
	u8 reg_val;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg_val = COM_OPEN;

	/* write command - switch */
	max77833_muic_read_switch(muic_data);
	max77833_muic_write_switch(muic_data, reg_val, 0xff);
}

static int com_to_usb_ap(struct max77833_muic_data *muic_data)
{
	u8 reg_val;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg_val = COM_USB;

	/* write command - switch */
	max77833_muic_read_switch(muic_data);
	max77833_muic_write_switch(muic_data, reg_val, 0xff);

	if (muic_data->pdata->set_safeout) {
		ret = muic_data->pdata->set_safeout(MUIC_PATH_USB_AP);
		if (ret)
			pr_err("%s:%s set_safeout err(%d)\n", MUIC_DEV_NAME, __func__, ret);
	}

	return ret;
}

static int com_to_usb_cp(struct max77833_muic_data *muic_data)
{
	u8 reg_val;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg_val = COM_USB_CP;

	/* write command - switch */
	max77833_muic_read_switch(muic_data);
	max77833_muic_write_switch(muic_data, reg_val, 0xff);

	if (muic_data->pdata->set_safeout) {
		ret = muic_data->pdata->set_safeout(MUIC_PATH_USB_CP);
		if (ret)
			pr_err("%s:%s set_safeout err(%d)\n", MUIC_DEV_NAME, __func__, ret);
	}

	return ret;
}

static void com_to_uart_ap(struct max77833_muic_data *muic_data)
{
	u8 reg_val;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg_val = COM_UART;

	/* write command - switch */
	max77833_muic_read_switch(muic_data);
	max77833_muic_write_switch(muic_data, reg_val, 0xff);
}

static void com_to_uart_cp(struct max77833_muic_data *muic_data)
{
	u8 reg_val;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg_val = COM_UART_CP;

	/* write command - switch */
	max77833_muic_read_switch(muic_data);
	max77833_muic_write_switch(muic_data, reg_val, 0xff);
}

static int write_vps_regs(struct max77833_muic_data *muic_data,
			muic_attached_dev_t new_dev)
{
	const struct max77833_muic_vps_data *tmp_vps;
	int vps_index;

	vps_index = muic_lookup_vps_table(new_dev, muic_data);
	if (vps_index < 0)
		return -ENODEV;

	tmp_vps = &(muic_vps_table[vps_index]);

if ((new_dev != ATTACHED_DEV_QC_CHARGER_9V_MUIC) && (new_dev != ATTACHED_DEV_AFC_CHARGER_9V_MUIC)) {
	/* write command - switch */
	max77833_muic_read_switch(muic_data);
	max77833_muic_write_switch(muic_data, tmp_vps->muic_switch, 0xff);
}

	return 0;
}

/* muic uart path control function */
static int switch_to_ap_uart(struct max77833_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	int ret = 0;

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
//	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
//	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		com_to_uart_ap(muic_data);
		break;
	default:
		pr_warn("%s:%s current attached is (%d) not Jig UART Off\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev);
		break;
	}

	return ret;
}

static int switch_to_cp_uart(struct max77833_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	int ret = 0;

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
//	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
//	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		com_to_uart_cp(muic_data);
		break;
	default:
		pr_warn("%s:%s current attached is (%d) not Jig UART Off\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev);
		break;
	}

	return ret;
}

static void max77833_muic_enable_chgdet(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* write command - config */
	max77833_muic_read_config(muic_data);
	max77833_muic_write_config(muic_data, CHGDET_ENABLE, 0xff);
}

static void max77833_muic_disable_chgdet(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* write command - config */
	max77833_muic_read_config(muic_data);
	max77833_muic_write_config(muic_data, CHGDET_DISABLE, 0xff);
}

#if 0
static int max77833_muic_enable_accdet(struct max77833_muic_data *muic_data)
{
	return 0;
}

static int max77833_muic_disable_accdet(struct max77833_muic_data *muic_data)
{
	return 0;
}
#endif

static u8 max77833_muic_get_adc_value(struct max77833_muic_data *muic_data)
{
	u8 status;
	u8 adc = MAX77833_ADC_ERROR;
	int ret;

	ret = max77833_read_reg(muic_data->i2c, MAX77833_MUIC_REG_STATUS1,
			&status);
	if (ret)
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
	else
		adc = status & STATUS1_IDRES_MASK;

	return adc;
}

static ssize_t max77833_muic_show_uart_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		mode = "AP\n";
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP\n";
		break;
	default:
		break;
	}

	pr_info("%s:%s %s", MUIC_DEV_NAME, __func__, mode);
	return sprintf(buf, mode);
}

static ssize_t max77833_muic_set_uart_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	bool empty_q = is_empty_muic_cmd_queue(&(muic_data->muic_cmd_queue));

	if (!strncasecmp(buf, "AP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		switch_to_ap_uart(muic_data, muic_data->attached_dev);
	} else if (!strncasecmp(buf, "CP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		switch_to_cp_uart(muic_data, muic_data->attached_dev);
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s uart_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->uart_path);

	if (empty_q)
		max77833_muic_handle_cmd(muic_data, -3);

	return count;
}

static ssize_t max77833_muic_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		mode = "PDA\n";
		break;
	case MUIC_PATH_USB_CP:
		mode = "MODEM\n";
		break;
	default:
		break;
	}

	pr_info("%s:%s %s", MUIC_DEV_NAME, __func__, mode);
	return sprintf(buf, mode);
}

static ssize_t max77833_muic_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3)) {
		pdata->usb_path = MUIC_PATH_USB_AP;
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		pdata->usb_path = MUIC_PATH_USB_CP;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	return count;
}

static ssize_t max77833_muic_show_uart_en(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!pdata->rustproof_on) {
		pr_info("%s:%s UART ENABLE\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s:%s UART DISABLE", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "0\n");
}

static ssize_t max77833_muic_set_uart_en(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "1", 1)) {
		pdata->rustproof_on = false;
	} else if (!strncasecmp(buf, "0", 1)) {
		pdata->rustproof_on = true;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s uart_en(%d)\n", MUIC_DEV_NAME, __func__,
			!pdata->rustproof_on);

	return count;
}

static ssize_t max77833_muic_show_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	u8 adc;

	adc = max77833_muic_get_adc_value(muic_data);
	pr_info("%s:%s adc(0x%02x)\n", MUIC_DEV_NAME, __func__, adc);

	if (adc == MAX77833_ADC_ERROR) {
		pr_err("%s:%s fail to read adc value\n", MUIC_DEV_NAME,
				__func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", adc);
}

static ssize_t max77833_muic_show_usb_state(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return sprintf(buf, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t max77833_muic_show_attached_dev(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	const struct max77833_muic_vps_data *tmp_vps;
	int vps_index;

	vps_index = muic_lookup_vps_table(muic_data->attached_dev, muic_data);
	if (vps_index < 0)
		return sprintf(buf, "Error No Device\n");

	tmp_vps = &(muic_vps_table[vps_index]);

	return sprintf(buf, "%s\n", tmp_vps->vps_name);
}

static ssize_t max77833_muic_show_otg_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	int ret = -ENODEV;

	if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC)) {
		if (muic_data->is_otg_test == true)
			ret = 0;
		else
			ret = 1;
		pr_info("%s:%s ret:%d buf:%s\n", MUIC_DEV_NAME, __func__, ret, buf);

		return ret;

		// Read chgdet value	- CHECK ME!!!
		//return sprintf(buf, "%x\n", val);
	} else
		return ret;
}

static ssize_t max77833_muic_set_otg_test(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	int ret = -ENODEV;
	bool empty_q = is_empty_muic_cmd_queue(&(muic_data->muic_cmd_queue));

	if (muic_check_support_dev(muic_data, ATTACHED_DEV_OTG_MUIC)) {
		pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);
		if (!strncmp(buf, "0", 1)) {
			muic_data->is_otg_test = true;
			max77833_muic_disable_chgdet(muic_data);
			ret = 0;
		} else if (!strncmp(buf, "1", 1)) {
			muic_data->is_otg_test = false;
			max77833_muic_enable_chgdet(muic_data);
			ret = 1;
		} else {
			pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
			return count;
		}

		pr_info("%s:%s ret: %d\n", MUIC_DEV_NAME, __func__, ret);

		if (empty_q)
			max77833_muic_handle_cmd(muic_data, -6);

		return count;
	} else
		return ret;
}

static ssize_t max77833_muic_show_apo_factory(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t max77833_muic_set_apo_factory(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
#if defined(CONFIG_SEC_FACTORY)
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
#endif /* CONFIG_SEC_FACTORY */
	const char *mode;

	pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
#if defined(CONFIG_SEC_FACTORY)
		muic_data->is_factory_start = true;
#endif /* CONFIG_SEC_FACTORY */
		mode = "FACTORY_MODE";
	} else {
		pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return count;
}

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
static ssize_t max77833_muic_show_afc_disable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (pdata->afc_disable) {
		pr_info("%s:%s AFC DISABLE\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s:%s AFC ENABLE", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "0\n");
}

static ssize_t max77833_muic_set_afc_disable(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "1", 1)) {
		pdata->afc_disable = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		pdata->afc_disable = false;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s afc_disable(%d)\n", MUIC_DEV_NAME, __func__, pdata->afc_disable);

	return count;
}
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
static void muic_set_cc_noti(int attached, int rid);

static ssize_t cc_xx_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	const char *mode = "UNKNOWN\n";

	pr_info("%s:%s %s", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, mode);
}

static ssize_t cc_xx_set(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int attached = 0, rid = 0;
	int ret = 0;

	pr_info("%s:%s buf=%s\n", MUIC_DEV_NAME, __func__, buf);

	if (buf[1] != ':') {
		pr_err("A wrong Input Fomat!\n");
		return count;
	}

	if ((buf[0]=='A') || (buf[0]=='a'))
		attached = 1;

	ret = kstrtoint(buf + 2, 0, &rid);
	if (ret) {
		pr_err("%s: Undefined rid\n", __func__);
		goto err;
	}

	muic_set_cc_noti(attached, rid);

err:
	return count;
}

static DEVICE_ATTR(cc_xx, 0664, cc_xx_show,
		cc_xx_set);
#endif

static DEVICE_ATTR(uart_sel, 0664, max77833_muic_show_uart_sel,
		max77833_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664, max77833_muic_show_usb_sel,
		max77833_muic_set_usb_sel);
static DEVICE_ATTR(uart_en, 0660, max77833_muic_show_uart_en,
		max77833_muic_set_uart_en);
static DEVICE_ATTR(adc, S_IRUGO, max77833_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, S_IRUGO, max77833_muic_show_usb_state, NULL);
static DEVICE_ATTR(attached_dev, S_IRUGO, max77833_muic_show_attached_dev, NULL);
#if defined(CONFIG_MUIC_MAX77833_SUPPROT_AUDIO_LINE_OUT_CONTROL)
static DEVICE_ATTR(audio_path, 0664,
		max77833_muic_show_audio_path, max77833_muic_set_audio_path);
#endif /* CONFIG_MUIC_MAX77833_SUPPROT_AUDIO_LINE_OUT_CONTROL */
static DEVICE_ATTR(otg_test, 0664,
		max77833_muic_show_otg_test, max77833_muic_set_otg_test);
static DEVICE_ATTR(apo_factory, 0664,
		max77833_muic_show_apo_factory, max77833_muic_set_apo_factory);
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
static DEVICE_ATTR(afc_disable, 0664,
		max77833_muic_show_afc_disable, max77833_muic_set_afc_disable);
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */

static struct attribute *max77833_muic_attributes[] = {
#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	&dev_attr_cc_xx.attr,
#endif
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_en.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_attached_dev.attr,
#if defined(CONFIG_MUIC_MAX77833_SUPPROT_AUDIO_LINE_OUT_CONTROL)
	&dev_attr_audio_path.attr,
#endif /* CONFIG_MUIC_MAX77833_SUPPROT_AUDIO_LINE_OUT_CONTROL */
	&dev_attr_otg_test.attr,
	&dev_attr_apo_factory.attr,
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	&dev_attr_afc_disable.attr,
#endif /* CONFIG_HV_MUIC_MAX77833_AFC */
	NULL
};

static const struct attribute_group max77833_muic_group = {
	.attrs = max77833_muic_attributes,
};

void max77833_muic_read_register(struct i2c_client *i2c)
{
	const enum max77833_muic_reg regfile[] = {
		MAX77833_MUIC_REG_HW_REV,
		MAX77833_MUIC_REG_STATUS1,
		MAX77833_MUIC_REG_STATUS2,
		MAX77833_MUIC_REG_STATUS3,
		MAX77833_MUIC_REG_INTMASK1,
		MAX77833_MUIC_REG_INTMASK2,
		MAX77833_MUIC_REG_INTMASK3,
	};
	u8 val;
	int i;

	pr_info("%s:%s read register--------------\n", MUIC_DEV_NAME, __func__);
	for (i = 0; i < ARRAY_SIZE(regfile); i++) {
		int ret = 0;
		ret = max77833_read_reg(i2c, regfile[i], &val);
		if (ret) {
			pr_err("%s:%s fail to read muic reg(0x%02x), ret=%d\n",
					MUIC_DEV_NAME, __func__, regfile[i], ret);
			continue;
		}

		pr_info("%s:%s reg(0x%02x)=[0x%02x]\n", MUIC_DEV_NAME, __func__,
				regfile[i], val);
	}
	pr_info("%s:%s end register---------------\n", MUIC_DEV_NAME, __func__);
}

static void max77833_muic_set_adcmode
		(struct max77833_muic_data *muic_data, u8 val)
{
	const u8 mask = MODE_MASK;

	if (muic_data->adcmode == val) {
		pr_info("%s: %s: ADCMODE Duplicated.\n", MUIC_DEV_NAME, __func__);
		return;
	}
	else {
		muic_data->adcmode = val;
		max77833_muic_read_idmon_config(muic_data);
		val = val << MODE_SHIFT;
		max77833_muic_write_idmon_config(muic_data, val, mask);
		max77833_muic_read_idmon_config(muic_data);	// For ACA issue.
	}

	return;
}

static void max77833_muic_adcmode_switch
	(struct max77833_muic_data *muic_data, const u8 val)
{
	const char *name;

	switch (val) {
	case MAX77833_MUIC_IDMODE_CONTINUOUS:
		name = "Continuous";
		break;
	case MAX77833_MUIC_IDMODE_FACTORY_ONE_SHOT:
		name = "Factory One Shot";
		break;
	case MAX77833_MUIC_IDMODE_ONE_SHOT:
		name = "One Shot";
		break;
	case MAX77833_MUIC_IDMODE_PULSE:
		name = "Pulse";
		break;
	default:
		pr_warn("%s:%s wrong ADCMode val[0x%02x]\n", MUIC_DEV_NAME,
				__func__, val);
		return;
	}

	pr_info("%s:%s %s val[0x%02x]\n", MUIC_DEV_NAME, __func__, name, val);
	max77833_muic_set_adcmode(muic_data, val);

	return;
}

void max77833_muic_set_idmode_continuous(struct max77833_muic_data *muic_data)
{
	const u8 val = MAX77833_MUIC_IDMODE_CONTINUOUS;

	max77833_muic_adcmode_switch(muic_data, val);

	return;
}

void max77833_muic_set_idmode_oneshot(struct max77833_muic_data *muic_data)
{
	const u8 val = MAX77833_MUIC_IDMODE_ONE_SHOT;

	max77833_muic_adcmode_switch(muic_data, val);

	return;
}

void max77833_muic_set_idmode_pulse(struct max77833_muic_data *muic_data)
{
	const u8 val = MAX77833_MUIC_IDMODE_PULSE;

	max77833_muic_adcmode_switch(muic_data, val);

	return;
}

static int max77833_muic_attach_uart_path(struct max77833_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->uart_path == MUIC_PATH_UART_AP) {
		ret = switch_to_ap_uart(muic_data, new_dev);
	} else if (pdata->uart_path == MUIC_PATH_UART_CP) {
		ret = switch_to_cp_uart(muic_data, new_dev);
	}
	else
		pr_warn("%s:%s invalid uart_path\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int max77833_muic_attach_usb_path(struct max77833_muic_data *muic_data,
					muic_attached_dev_t new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s usb_path=%d\n", MUIC_DEV_NAME, __func__, pdata->usb_path);

	if (pdata->usb_path == MUIC_PATH_USB_AP) {
		ret = com_to_usb_ap(muic_data);
	}
	else if (pdata->usb_path == MUIC_PATH_USB_CP) {
		ret = com_to_usb_cp(muic_data);
	}
	else
		pr_warn("%s:%s invalid usb_path\n", MUIC_DEV_NAME, __func__);

	return ret;
}

#if defined(CONFIG_SEC_FACTORY)
static muic_attached_dev_t check_jig_uart_on_factory_test
			(struct max77833_muic_data *muic_data,	muic_attached_dev_t new_dev)
{
	muic_attached_dev_t ret_ndev;

	if (muic_data->is_factory_start &&
	muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) {
		ret_ndev = ATTACHED_DEV_JIG_UART_ON_MUIC;
	} else
		ret_ndev = ATTACHED_DEV_JIG_UART_OFF_MUIC;

	pr_info("%s:%s is_factory_start = %c\n", MUIC_DEV_NAME, __func__,
			(muic_data->is_factory_start) ? 'T' : 'F');

	return ret_ndev;
}
#endif

int max77833_muic_handle_detach(struct max77833_muic_data *muic_data)
{
	int ret = 0;
	bool noti = true;

	if (muic_data->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s:%s Duplicated(%d), just ignore\n", MUIC_DEV_NAME,
				__func__, muic_data->attached_dev);
		goto out_without_noti;
	}

	/* Enable Factory Accessory Detection State Machine */
//	max77833_muic_enable_accdet(muic_data);

	muic_lookup_vps_table(muic_data->attached_dev, muic_data);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
#endif
		break;
	default:
		break;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti)
		enqueue_muic_notifier(&(muic_data->muic_cmd_queue),
			NOTI_DETACH, muic_data->attached_dev);

//	if (logically_noti)
//		enqueue_muic_notifier(&(muic_data->muic_cmd_queue),
//			NOTI_LOGICALLY_DETACH, ATTACHED_DEV_SMARTDOCK_VB_MUIC);
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	max77833_muic_set_afc_ready(muic_data, false);
	max77833_muic_hv_qc_disable(muic_data);
	max77833_muic_hv_fchv_disable(muic_data);
#endif

out_without_noti:
	com_to_open(muic_data);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int max77833_muic_logically_detach(struct max77833_muic_data *muic_data,
						muic_attached_dev_t new_dev)
{
	bool noti = true;
//	bool logically_notify = false;
	bool force_path_open = true;
//	bool enable_accdet = true;
	int ret = 0;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
		max77833_muic_set_afc_ready(muic_data, false);
		max77833_muic_hv_qc_disable(muic_data);
		max77833_muic_hv_fchv_disable(muic_data);
#endif
		break;
	case ATTACHED_DEV_NONE_MUIC:
		force_path_open = false;
		goto out;
	default:
		pr_warn("%s:%s try to attach without logically detach\n",
				MUIC_DEV_NAME, __func__);
		goto out;
	}

	pr_info("%s:%s attached(%d)!=new(%d), assume detach\n", MUIC_DEV_NAME,
			__func__, muic_data->attached_dev, new_dev);

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti)
		enqueue_muic_notifier(&(muic_data->muic_cmd_queue), NOTI_DETACH,
			muic_data->attached_dev);

//	if (logically_notify)
//		enqueue_muic_notifier(&(muic_data->muic_cmd_queue),
//			NOTI_LOGICALLY_DETACH, ATTACHED_DEV_SMARTDOCK_VB_MUIC);
#endif /* CONFIG_MUIC_NOTIFIER */

out:
	if (force_path_open)
		com_to_open(muic_data);

	return ret;
}

int max77833_muic_handle_attach(struct max77833_muic_data *muic_data,
		muic_attached_dev_t new_dev)
{
	bool logically_notify = false;
//	bool noti_smartdock = false;
	int ret = 0;

	pr_info("%s:%s \n", MUIC_DEV_NAME, __func__);

	if (new_dev == muic_data->attached_dev) {
		pr_info("%s:%s Duplicated(%d), just ignore \n",
				MUIC_DEV_NAME, __func__, muic_data->attached_dev);
		return ret;
	}

	ret = max77833_muic_logically_detach(muic_data, new_dev);
	if (ret)
		pr_warn("%s:%s fail to logically detach(%d)\n", MUIC_DEV_NAME,
				__func__, ret);

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		ret = max77833_muic_attach_uart_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_TA_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = max77833_muic_attach_usb_path(muic_data, new_dev);
		break;
	default:
		pr_warn("%s:%s unsupported dev(%d)\n", MUIC_DEV_NAME, __func__,
				new_dev);
		ret = -ENODEV;
		goto out;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
//	if (noti_smartdock)
//		enqueue_muic_notifier(&(muic_data->muic_cmd_queue),
//			NOTI_LOGICALLY_ATTACH, ATTACHED_DEV_SMARTDOCK_VB_MUIC);

	if (logically_notify)
		enqueue_muic_notifier(&(muic_data->muic_cmd_queue),
			NOTI_LOGICALLY_ATTACH, new_dev);
	else
		enqueue_muic_notifier(&(muic_data->muic_cmd_queue),
			NOTI_ATTACH, new_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	if (muic_data->pdata->afc_disable)
		pr_info("%s:%s AFC Disable(%d) by USER!\n", MUIC_DEV_NAME,
			__func__, muic_data->pdata->afc_disable);
	else {
		if (new_dev == ATTACHED_DEV_TA_MUIC) {
			max77833_muic_set_afc_ready(muic_data, true);
			max77833_muic_hv_fchv_enable(muic_data, MPING_5TIMES, 0xff);
		}
	}
#endif

out:
	return ret;
}

static bool muic_check_vps_adc
			(const struct max77833_muic_vps_data *tmp_vps, u8 adc)
{
	bool ret = false;

	if (tmp_vps->adc == adc) {
		ret = true;
		goto out;
	}

	if (tmp_vps->adc == MAX77833_ADC_219) {
		switch(adc) {
		case MAX77833_ADC_CEA936ATYPE1_CHG:
		case MAX77833_ADC_JIG_USB_OFF:
			ret = true;
			goto out;
			break;
		default:
			break;
		}
	}

	if (tmp_vps->adc == MAX77833_ADC_UNDEFINED) {
		switch(adc) {
		case MAX77833_ADC_SEND_END ... MAX77833_ADC_REMOTE_S12:
		case MAX77833_ADC_UART_CABLE:
		case MAX77833_ADC_AUDIOMODE_W_REMOTE:
			ret = true;
			goto out;
			break;
		default:
			break;
		}
	}

	if (tmp_vps->adc == MAX77833_ADC_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_info("%s:%s vps(%s) adc(0x%02x) ret(%c)\n",
				MUIC_DEV_NAME, __func__, tmp_vps->vps_name,
				adc, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_vps_chgdetrun
			(const struct max77833_muic_vps_data *tmp_vps, u8 chgdetrun)
{
	bool ret = false;

	if (tmp_vps->chgdetrun == chgdetrun) {
		ret = true;
		goto out;
	}

	if (tmp_vps->chgdetrun == CHGDETRUN_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_info("%s:%s vps(%s) chgdetrun(0x%02x) ret(%c)\n",
				MUIC_DEV_NAME, __func__, tmp_vps->vps_name,
				chgdetrun, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_vps_chgtyp
			(const struct max77833_muic_vps_data *tmp_vps, u8 chgtyp)
{
	bool ret = false;

	if (tmp_vps->chgtyp == chgtyp) {
		ret = true;
		goto out;
	}

	if (tmp_vps->chgtyp == CHGTYP_UNOFFICIAL_CHARGER) {
		switch (chgtyp) {
//		case CHGTYP_500MA:
//		case CHGTYP_1A:
//			ret = true;
//			goto out;
		default:
			break;
		}
	}

	if (tmp_vps->chgtyp == CHGTYP_ANY) {
		switch (chgtyp) {
		case CHGTYP_USB:
		case CHGTYP_CDP:
		case CHGTYP_DEDICATED_CHARGER:
//		case CHGTYP_500MA:
//		case CHGTYP_1A:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_vps->chgtyp == CHGTYP_DONTCARE)
		ret = true;

out:
	if (debug_en_vps) {
		pr_info("%s:%s vps(%s) chgtyp(0x%02x) ret(%c)\n",
				MUIC_DEV_NAME, __func__, tmp_vps->vps_name,
				chgtyp, ret ? 'T' : 'F');
	}

	return ret;
}

muic_attached_dev_t max77833_muic_check_new_dev(struct max77833_muic_data *muic_data, int *intr)
{
	const struct max77833_muic_vps_data *tmp_vps;
	muic_attached_dev_t new_dev = ATTACHED_DEV_NONE_MUIC;
	u8 adc = muic_data->status1 & STATUS1_IDRES_MASK;
	u8 chgdetrun = muic_data->status2 & STATUS2_CHGTYPRUN_MASK;
	u8 chgtyp = muic_data->status2 & STATUS2_CHGTYP_MASK;
	int i;

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		tmp_vps = &(muic_vps_table[i]);

		if (!(muic_check_vps_adc(tmp_vps, adc)))
			continue;

		if (!(muic_check_vps_chgdetrun(tmp_vps, chgdetrun)))
			continue;

		if (!(muic_check_vps_chgtyp(tmp_vps, chgtyp)))
			continue;

		if (!(muic_check_support_dev(muic_data, tmp_vps->attached_dev))) {
			new_dev = ATTACHED_DEV_UNSUPPORTED_ID_MUIC;
			*intr = MUIC_INTR_ATTACH;
			pr_info("%s:%s unsupported ID\n", MUIC_DEV_NAME, __func__);
			break;
		}

		pr_info("%s:%s vps table match found at i(%d), %s\n",
			MUIC_DEV_NAME, __func__, i, tmp_vps->vps_name);

		new_dev = tmp_vps->attached_dev;
		*intr = MUIC_INTR_ATTACH;
		break;
	}

	pr_info("%s:%s %d->%d\n", MUIC_DEV_NAME, __func__, muic_data->attached_dev, new_dev);

	return new_dev;
}

#if !defined(CONFIG_MUIC_DISABLE_INTERRUPT)
#if !defined(CONFIG_SEC_FACTORY)
static bool is_need_muic_idmode_continuous(muic_attached_dev_t new_dev)
{
	bool ret = false;

	switch (new_dev) {
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		ret = true;
		break;
	default:
		break;
	}

	pr_info("%s:%s (%d)%c\n", MUIC_DEV_NAME, __func__, new_dev,
			ret ? 'T' : 'F');

	return ret;
}

static bool is_need_muic_idmode_pulse(struct max77833_muic_data *muic_data)
{
	u8 adc = muic_data->status1 & STATUS1_IDRES_MASK;
	bool ret = false;

	if (adc == MAX77833_ADC_JIG_UART_OFF)
		ret = true;

	pr_info("%s:%s ID: 0x%02x(%c)\n", MUIC_DEV_NAME, __func__, adc,
			ret ? 'T' : 'F');

	return ret;
}
#endif
#endif

#if !defined(CONFIG_MUIC_DISABLE_INTERRUPT)
static void max77833_muic_detect_dev(struct max77833_muic_data *muic_data, int irq)
{
	struct i2c_client *i2c = muic_data->i2c;
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	u8 status[3];
	u8 adc, chgdetrun, chgtyp, sysmsg;
	int ret;
	bool empty_q = is_empty_muic_cmd_queue(&(muic_data->muic_cmd_queue));

	ret = max77833_bulk_read(i2c, MAX77833_MUIC_REG_STATUS1, 3, status);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
		return;
	}
	pr_info("%s:%s STATUS1:0x%02x, 2:0x%02x, 3:0x%02x\n", MUIC_DEV_NAME,
			__func__, status[0], status[1], status[2]);

	/* attached status */
	muic_data->status1 = status[0];
	muic_data->status2 = status[1];
	muic_data->status3 = status[2];

	adc = status[0] & STATUS1_IDRES_MASK;
	chgdetrun = status[1] & STATUS2_CHGTYPRUN_MASK;
	chgtyp = status[1] & STATUS2_CHGTYP_MASK;
	sysmsg = status[2] & STATUS3_SYSMSG_MASK;

	pr_info("%s:%s adc:0x%x chgdetrun:0x%x chgtyp:0x%x sysmsg:0x%x\n",
		MUIC_DEV_NAME, __func__, adc, chgdetrun, chgtyp, sysmsg);

#ifdef CONFIG_MUIC_MAX77833_SHAKEID_WA
	/* Workaround for Factory mode.
	 * Abandon adc interrupt of approximately +-100K range
	 * if previous cable status was JIG UART BOOT OFF.
	 */
	if ((muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) ||
		(muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC)) {
		if ((adc == (MAX77833_ADC_JIG_UART_OFF + 1)) ||
				(adc == (MAX77833_ADC_JIG_UART_OFF - 1))) {
			if (!muic_data->is_factory_start || adc != MAX77833_ADC_JIG_UART_ON) {
				pr_warn("%s:%s abandon JIG UART\n", MUIC_DEV_NAME, __func__);
				return;
			}
		}
	}
#ifdef CONFIG_SEC_FACTORY
	/* Workaround for JIG Download issue in Factory mode. */
	if (muic_data->attached_dev == ATTACHED_DEV_JIG_USB_ON_MUIC) {
		if (adc == (ADC_JIG_USB_ON - 1)) {
			pr_warn("%s:%s abandon JIG USB\n", MUIC_DEV_NAME, __func__);
			return;
		}
	}
#endif
#endif

	new_dev = max77833_muic_check_new_dev(muic_data, &intr);

	if (pass_rev >= MUIC_PASS4) { // For PASS4/Old rev onebinary
		pr_info("%s: %s: ADCMODE / PASS4\n",MUIC_DEV_NAME,__func__);
#if !defined(CONFIG_SEC_FACTORY)
		if (is_need_muic_idmode_continuous(new_dev)) {
			/* ADC Mode switch to the Continuous Mode */
			max77833_muic_set_idmode_continuous(muic_data);
		} else if (is_need_muic_idmode_pulse(muic_data)) {
			/* ID Mode switch to the Pulse Mode */
			max77833_muic_set_idmode_pulse(muic_data);
		} else {
			/* ADC Mode restore to the One Shot Mode */
			max77833_muic_set_idmode_oneshot(muic_data);
		}
#endif /* CONFIG_SEC_FACTORY */
	}
	else
		pr_info("%s: %s: ADCMODE / Not PASS4\n",MUIC_DEV_NAME,__func__);

	if (muic_is_cc_supported_jig(muic_data, new_dev)) {
		pr_info("%s: %s: MUIC 523K.\n",MUIC_DEV_NAME,__func__);

		if (intr == MUIC_INTR_ATTACH) {
			pr_info("%s:%s ATTACHED\n", MUIC_DEV_NAME, __func__);
			ret = max77833_muic_handle_attach(muic_data, new_dev);
			if (ret)
				pr_err("%s:%s cannot handle attach(%d)\n", MUIC_DEV_NAME, __func__, ret);
		} else {
			pr_info("%s:%s DETACHED\n", MUIC_DEV_NAME, __func__);
			ret = max77833_muic_handle_detach(muic_data);
			if (ret)
				pr_err("%s:%s cannot handle detach(%d)\n", MUIC_DEV_NAME, __func__, ret);
		}

		if (empty_q)
			max77833_muic_handle_cmd(muic_data, -1);
	} else {
		pr_info("%s: %s: MUIC Not 523K.\n",MUIC_DEV_NAME,__func__);
	}

	return;
}
#endif

static irqreturn_t max77833_muic_irq(int irq, void *data)
{
	struct max77833_muic_data *muic_data = data;

	pr_info("%s:%s irq:%d\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
#if defined(CONFIG_MUIC_DISABLE_INTERRUPT)
		pr_info("%s: CCIC is supported. Interrupt discarded.\n", __func__);
#else
		max77833_muic_detect_dev(muic_data, irq);
#endif
	else
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static irqreturn_t max77833_muic_irq_cmd(int irq, void *data)
{
	struct max77833_muic_data *muic_data = data;

	pr_info("%s:%s irq:%d\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
		max77833_muic_handle_cmd(muic_data, irq);
	else
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static irqreturn_t max77833_muic_irq_sysmsg(int irq, void *data)
{
	struct max77833_muic_data *muic_data = data;

	pr_info("%s:%s irq:%d\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
		max77833_muic_read_sysmsg(muic_data, irq);
	else
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

#define REQUEST_IRQ(_irq, _dev_id, _name)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, max77833_muic_irq,	\
				IRQF_NO_SUSPEND, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
				MUIC_DEV_NAME, __func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

#define REQUEST_IRQ_CMD(_irq, _dev_id, _name)				\
({									\
	ret = request_threaded_irq(_irq, NULL, max77833_muic_irq_cmd,	\
				IRQF_NO_SUSPEND, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
				MUIC_DEV_NAME, __func__, _irq, ret);	\
		_irq = 0;						\
	}								\
})

#define REQUEST_IRQ_SYSMSG(_irq, _dev_id, _name)				\
({										\
	ret = request_threaded_irq(_irq, NULL, max77833_muic_irq_sysmsg,	\
				IRQF_NO_SUSPEND, _name, _dev_id);		\
	if (ret < 0) {								\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",			\
				MUIC_DEV_NAME, __func__, _irq, ret);		\
		_irq = 0;							\
	}									\
})

static int max77833_muic_irq_init(struct max77833_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->mfd_pdata && (muic_data->mfd_pdata->irq_base > 0)) {
		int irq_base = muic_data->mfd_pdata->irq_base;

		/* request MUIC IRQ */
		muic_data->irq_idres = irq_base + MAX77833_MUIC_IRQ_INT3_IDRES_INT;
		REQUEST_IRQ(muic_data->irq_idres, muic_data, "muic-idres");

		muic_data->irq_chgtyp = irq_base + MAX77833_MUIC_IRQ_INT3_CHGTYP_INT;
		REQUEST_IRQ(muic_data->irq_chgtyp, muic_data, "muic-chgtyp");

		muic_data->irq_sysmsg = irq_base + MAX77833_MUIC_IRQ_INT3_SYSMSG_INT;
		REQUEST_IRQ_SYSMSG(muic_data->irq_sysmsg, muic_data, "muic-sysmsg");

		muic_data->irq_apcmdres = irq_base + MAX77833_MUIC_IRQ_INT3_APCMD_RESP_INT;
		REQUEST_IRQ_CMD(muic_data->irq_apcmdres, muic_data, "muic-apcmdres");
	}

	pr_info("%s:%s idres(%d), chgtyp(%d), sysmsg(%d), apcmdres(%d)\n",
			MUIC_DEV_NAME, __func__,
			muic_data->irq_idres, muic_data->irq_chgtyp,
			muic_data->irq_sysmsg, muic_data->irq_apcmdres);
	return ret;
}

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s:%s IRQ(%d):%s free done\n", MUIC_DEV_NAME,	\
				__func__, _irq, _name);			\
	}								\
} while (0)

static void max77833_muic_free_irqs(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* free MUIC IRQ */
	FREE_IRQ(muic_data->irq_idres, muic_data, "muic-idres");
	FREE_IRQ(muic_data->irq_chgtyp, muic_data, "muic-chgtyp");
	FREE_IRQ(muic_data->irq_sysmsg, muic_data, "muic-sysmsg");
	FREE_IRQ(muic_data->irq_apcmdres, muic_data, "muic-apcmdres");
}

#define CHECK_GPIO(_gpio, _name)					\
do {									\
	if (!_gpio) {							\
		pr_err("%s:%s " _name " GPIO defined as 0 !\n",		\
				MUIC_DEV_NAME, __func__);		\
		WARN_ON(!_gpio);					\
		ret = -EIO;						\
		goto err_kfree;						\
	}								\
} while (0)

#if !defined(CONFIG_MUIC_DISABLE_INTERRUPT)
static void max77833_muic_init_detect(struct max77833_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);
	muic_data->is_muic_ready = true;

	if(!is_empty_muic_cmd_queue(&(muic_data->muic_cmd_queue)))
		max77833_muic_handle_cmd(muic_data, -2);

	max77833_muic_detect_dev(muic_data, -1);

	mutex_unlock(&muic_data->muic_mutex);
}
#endif

#if defined(CONFIG_OF)
static int of_max77833_muic_dt(struct max77833_muic_data *muic_data)
{
	struct device_node *np_muic;
	const char *prop_support_list;
	int i, j, prop_num;
	int ret = 0;

	np_muic = of_find_node_by_path("/muic");
	if (np_muic == NULL)
		return -EINVAL;

	prop_num = of_property_count_strings(np_muic, "muic,support-list");
	if (prop_num < 0) {
		pr_warn("%s:%s Cannot parse 'muic support list dt node'[%d]\n",
				MUIC_DEV_NAME, __func__, prop_num);
		ret = prop_num;
		goto err;
	}

	/* for debug */
	for (i = 0; i < prop_num; i++) {
		ret = of_property_read_string_index(np_muic, "muic,support-list", i,
							&prop_support_list);
		if (ret) {
			pr_err("%s:%s Cannot find string at [%d], ret[%d]\n",
					MUIC_DEV_NAME, __func__, i, ret);
			goto err;
		}

		pr_debug("%s:%s prop_support_list[%d] is %s\n", MUIC_DEV_NAME, __func__,
				i, prop_support_list);

		for (j = 0; j < ARRAY_SIZE(muic_vps_table); j++) {
			if (!strcmp(muic_vps_table[j].vps_name, prop_support_list)) {
				muic_data->muic_support_list[(muic_vps_table[j].attached_dev)] = true;
				break;
			}
		}
	}

	/* for debug */
	for (i = 0; i < ATTACHED_DEV_NUM; i++) {
		pr_debug("%s:%s pmuic_support_list[%d] = %c\n", MUIC_DEV_NAME, __func__,
				i, (muic_data->muic_support_list[i] ? 'T' : 'F'));
	}

err:
	of_node_put(np_muic);

	return ret;
}
#endif /* CONFIG_OF */

static void max77833_muic_clear_interrupt(struct max77833_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 interrupt1, interrupt2, interrupt3;
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = max77833_read_reg(i2c, MAX77833_MUIC_REG_INT1, &interrupt1);
	if (ret)
		pr_err("%s:%s fail to read muic INT1 reg(%d)\n", MUIC_DEV_NAME, __func__, ret);

	ret = max77833_read_reg(i2c, MAX77833_MUIC_REG_INT2, &interrupt2);
	if (ret)
		pr_err("%s:%s fail to read muic INT2 reg(%d)\n", MUIC_DEV_NAME, __func__, ret);

	ret = max77833_read_reg(i2c, MAX77833_MUIC_REG_INT3, &interrupt3);
	if (ret)
		pr_err("%s:%s fail to read muic INT3 reg(%d)\n", MUIC_DEV_NAME, __func__, ret);

	pr_info("%s:%s CLEAR!! INT1:0x%02x, 2:0x%02x, 3:0x%02x\n", MUIC_DEV_NAME,
				__func__, interrupt1, interrupt2, interrupt3);
}

static int max77833_muic_init_regs(struct max77833_muic_data *muic_data)
{
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	max77833_muic_clear_interrupt(muic_data);

	if (pass_rev >= MUIC_PASS4) { // For PASS4/Old rev onebinary
		pr_info("%s: %s: init ADCMODE / PASS4\n",MUIC_DEV_NAME,__func__);
#if defined(CONFIG_SEC_FACTORY)
		/* ADC Mode switch to the Continuous Mode */
		max77833_muic_set_idmode_continuous(muic_data);
#endif /* CONFIG_SEC_FACTORY */
	}
	else {
		pr_info("%s: %s: init ADCMODE / Not PASS4\n",MUIC_DEV_NAME,__func__);
		pr_info("%s:%s ***** Test pulsemode - HE, advanced HW group *****\n",
			MUIC_DEV_NAME, __func__);
		max77833_muic_set_idmode_pulse(muic_data);
	}

	ret = max77833_muic_irq_init(muic_data);
	if (ret < 0) {
		pr_err("%s:%s Failed to initialize MUIC irq:%d\n", MUIC_DEV_NAME,
				__func__, ret);
		max77833_muic_free_irqs(muic_data);
	}

	return ret;
}

#if defined(CONFIG_CCIC_NOTIFIER)
static int muic_handle_cc_event(struct max77833_muic_data *muic_data, int action, int mdev)
{
	struct i2c_client *i2c = muic_data->i2c;
	muic_attached_dev_t new_dev = ATTACHED_DEV_NONE_MUIC;
	int intr, ret;
	u8 status[3];
	u8 vbvolt = 0;
	u8 chgtyp = 0;
	bool empty_q = is_empty_muic_cmd_queue(&(muic_data->muic_cmd_queue));

	pr_info("%s: action=%d, mdev=%d\n", __func__, action, mdev);

	ret = max77833_bulk_read(i2c, MAX77833_MUIC_REG_STATUS1, 3, status);
	if (ret) {
		pr_err("%s: fail to read muic reg(%d)\n",
				__func__, ret);
		return -1;
	}
	pr_info("%s: STATUS1:0x%02x, 2:0x%02x, 3:0x%02x\n",
			__func__, status[0], status[1], status[2]);

	/* VBUS detection is not supported in MAX77833 */
	/* vbvolt = status[1] & STATUS2_VBVOLT_MASK; */
	chgtyp = status[1] & STATUS2_CHGTYP_MASK;
	vbvolt = chgtyp;
	intr = action;

	pr_info("%s: vbvolt: %d\n", __func__, vbvolt);


	switch (mdev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (vbvolt)
			new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
		break;

	case ATTACHED_DEV_UNKNOWN_MUIC:
		intr = MUIC_INTR_DETACH;
		break;

	default:
		break;
	}

	if (new_dev == ATTACHED_DEV_NONE_MUIC)
		new_dev = mdev;

	pr_info("%s: mdev=%d, new_dev=%d\n", __func__, mdev, new_dev);

	if (intr == MUIC_INTR_ATTACH) {
		pr_info("%s:%s ATTACHED\n", MUIC_DEV_NAME, __func__);
		ret = max77833_muic_handle_attach(muic_data, new_dev);
		if (ret)
			pr_err("%s:%s cannot handle attach(%d)\n", MUIC_DEV_NAME,
								__func__, ret);
	} else {
		pr_info("%s:%s DETACHED\n", MUIC_DEV_NAME, __func__);
		ret = max77833_muic_handle_detach(muic_data);
		if (ret)
			pr_err("%s:%s cannot handle detach(%d)\n", MUIC_DEV_NAME,
								__func__, ret);
	}

	if (empty_q)
		max77833_muic_handle_cmd(muic_data, -10);

	return 0;
}

#ifdef MUIC_USE_DIRECT_CMD
static int max77833_muic_direct_write(struct max77833_muic_data *muic_data, int cmd, int value)
{
	pr_info("%s: Writing (%d,0x%02x)\n", __func__, cmd, value);
	max77833_muic_write_reg(muic_data->i2c, MAX77833_MUIC_REG_DAT_IN_OP, cmd, true);
	max77833_muic_write_reg(muic_data->i2c, MAX77833_MUIC_REG_DAT_IN1, value, true);
	max77833_muic_write_reg(muic_data->i2c, MAX77833_MUIC_REG_DAT_IN8, 0x00, true);
	mdelay(30);
	pr_info("%s: Writing done.\n", __func__);

	return 0;
}

static int max77833_muic_direct_read(struct max77833_muic_data *muic_data, int cmd, int value)
{
	u8 out_op = 0;

	max77833_muic_write_reg(muic_data->i2c, MAX77833_MUIC_REG_DAT_IN_OP, cmd, true);
	max77833_muic_write_reg(muic_data->i2c, MAX77833_MUIC_REG_DAT_IN8, 0x00, true);
	mdelay(30);
	max77833_read_reg(muic_data->i2c, MAX77833_MUIC_REG_DAT_OUT1, &out_op);
	pr_info("%s: Read out_op = 0x%02x (0x%02x)\n", __func__, out_op, value);

	return 0;
}

static int muic_cc_com_to(struct max77833_muic_data *pmuic, int path)
{
	int com_value = 0;

	switch (path) {
	case MUIC_PATH_USB_AP:
		pr_info("%s: MUIC_PATH_USB_AP.\n", __func__);
		com_value = COM_USB;
		break;
	case MUIC_PATH_USB_CP:
		pr_info("%s: MUIC_PATH_USB_CP.\n", __func__);
		com_value = COM_USB_CP;
		break;
	case MUIC_PATH_UART_AP:
		pr_info("%s: MUIC_PATH_UART_AP.\n", __func__);
		com_value = COM_UART;
		break;
	case MUIC_PATH_UART_CP:
		pr_info("%s: MUIC_PATH_UART_CP.\n", __func__);
		com_value = COM_UART_CP;
		break;
	case MUIC_PATH_OPEN:
		pr_info("%s: MUIC_PATH_OPEN.\n", __func__);
		com_value = COM_OPEN;
		break;
	default:
		pr_err("%s:A wrong com path!\n", __func__);
		return -1;
	}

	max77833_muic_direct_write(pmuic, COMMAND_SWITCH_WRITE, com_value);
	max77833_muic_direct_read(pmuic, COMMAND_SWITCH_READ, com_value);

	return 0;
}
#else

#define muic_cc_com_to muic_cc_com_to_cmd
static int muic_cc_com_to_cmd(struct max77833_muic_data *pmuic, int path)
{
	bool empty_q = is_empty_muic_cmd_queue(&(pmuic->muic_cmd_queue));

	switch (path) {
	case MUIC_PATH_USB_AP:
		pr_info("%s: MUIC_PATH_USB_AP.\n", __func__);
		com_to_usb_ap(pmuic);
		break;
	case MUIC_PATH_USB_CP:
		pr_info("%s: MUIC_PATH_USB_CP.\n", __func__);
		com_to_usb_cp(pmuic);
		break;
	case MUIC_PATH_UART_AP:
		pr_info("%s: MUIC_PATH_UART_AP.\n", __func__);
		com_to_uart_ap(pmuic);
		break;
	case MUIC_PATH_UART_CP:
		pr_info("%s: MUIC_PATH_UART_CP.\n", __func__);
		com_to_uart_cp(pmuic);
		break;
	case MUIC_PATH_OPEN:
		pr_info("%s: MUIC_PATH_OPEN.\n", __func__);
		com_to_open(pmuic);
		break;
	default:
		pr_err("%s:A wrong com path!\n", __func__);
		return -1;
	}

	if (empty_q)
		max77833_muic_handle_cmd(pmuic, -11);

	return 0;
}
#endif

static int muic_handle_cc_path_open(struct max77833_muic_data *pmuic)
{
	pr_info("%s\n", __func__);

	muic_cc_com_to(pmuic, MUIC_PATH_OPEN);
	pr_info("%s: done.\n", __func__);

	return 0;
}

static int muic_handle_cc_path(struct max77833_muic_data *pmuic, int intr, int mdev)
{
	pr_info("%s: (%d,%d)\n", __func__, intr, mdev);

	if (intr == MUIC_INTR_DETACH) {
		muic_cc_com_to(pmuic, MUIC_PATH_OPEN);
		return 0;
	}

	switch (mdev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		muic_cc_com_to(pmuic, pmuic->pdata->uart_path);
		break;

	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		muic_cc_com_to(pmuic, pmuic->pdata->usb_path);
		break;
	default:
		pr_info("%s: cannot be handled here.\n", __func__);
		return -1;
	}

	pr_info("%s: done.\n", __func__);

	return 0;

}

static int muic_handle_cc_supported_dev(struct max77833_muic_data *pmuic, muic_attached_dev_t new_dev)
{
	pr_info("%s: new_dev:%d\n", __func__, new_dev);

	switch (new_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		muic_cc_com_to(pmuic, MUIC_PATH_USB_AP);
		muic_notifier_attach_attached_dev(new_dev);
		return 1;
	case ATTACHED_DEV_TA_MUIC:
		muic_cc_com_to(pmuic, MUIC_PATH_OPEN);
		muic_notifier_attach_attached_dev(new_dev);
		return 1;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		muic_cc_com_to(pmuic, MUIC_PATH_UART_AP);
		muic_notifier_attach_attached_dev(new_dev);
		return 1;
	default:
		pr_info("%s: unsupported dev.\n", __func__);
	}

	return 0;
}

#if !defined(CONFIG_MUIC_DISABLE_INTERRUPT)
static int muic_is_cc_supported_jig(struct max77833_muic_data *pmuic, muic_attached_dev_t new_dev)
{
	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		pr_info("%s: new_dev:%d Supported JIG.\n", __func__, new_dev);
		return 1;
	default:
		pr_info("%s: new_dev:%d Unsupported.\n", __func__, new_dev);
	}

	switch (pmuic->attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		pr_info("%s: attached_dev:%d Supported JIG.\n", __func__, pmuic->attached_dev);
		return 1;
	default:
		pr_info("%s: attached_dev:%d Unsupported.\n", __func__, pmuic->attached_dev);
	}

	return 0;
}
#endif

static int muic_cc_is_otg_test;
static int muic_handle_cc_OTG(struct max77833_muic_data *pmuic, bool is_on)
{
	bool empty_q = false;

	pr_info("%s: %d\n", __func__, is_on);

	if (is_on)
		muic_cc_com_to(pmuic, MUIC_PATH_USB_AP);

	empty_q = is_empty_muic_cmd_queue(&(pmuic->muic_cmd_queue));
	if (is_on)
		max77833_muic_disable_chgdet(pmuic);
	else
		max77833_muic_enable_chgdet(pmuic);

	if (empty_q)
		max77833_muic_handle_cmd(pmuic, -11);

	muic_cc_is_otg_test = is_on;

	return 0;
}

static int muic_get_chgtyp_to_mdev(struct max77833_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0, mdev = ATTACHED_DEV_NONE_MUIC;
	u8 status[3];
	u8 chgtyp = 0;

	pr_info("%s\n", __func__);

	ret = max77833_bulk_read(i2c, MAX77833_MUIC_REG_STATUS1, 3, status);
	if (ret) {
		pr_err("%s: fail to read muic reg(%d)\n",
				__func__, ret);
		return -1;
	}
	pr_info("%s: STATUS1:0x%02x, 2:0x%02x, 3:0x%02x\n",
			__func__, status[0], status[1], status[2]);

	/* VBUS detection is not supported in MAX77833 */
	/* vbvolt = status[1] & STATUS2_VBVOLT_MASK; */
	chgtyp = status[1] & STATUS2_CHGTYP_MASK;

	switch (chgtyp) {
	case CHGTYP_USB:
	case CHGTYP_CDP:
		mdev = ATTACHED_DEV_USB_MUIC;
		break;
	case CHGTYP_DEDICATED_CHARGER:
		mdev = ATTACHED_DEV_TA_MUIC;
		break;
	default:
		pr_warn("%s: undefined chgtyp: %d\n", __func__, chgtyp);
	}

	pr_info("%s: chgtyp %d -> mdev %d\n", __func__, chgtyp, mdev);

	return mdev;
}

static void muic_run_chgdet(struct max77833_muic_data *pmuic)
{
	pr_info("%s: Chgdet Re-run\n", __func__);
	max77833_muic_direct_write(pmuic, COMMAND_CHGDET_READ, MAX77833_MUIC_REG_DAT_OUT1);
	/* Max. 90msec for the result*/
	mdelay(100);
	return;
}

static int muic_handle_cc_legacy_TA_USB(struct max77833_muic_data *pmuic, bool attached )
{
	pr_info("%s: No VBUS check. attached:%d\n", __func__, attached);

	if (attached) {
		int mdev = 0;
		/* 1. VBUS check.
		  *    Assumes that VBUS is already connected to mUIC.
		*/
		/* 2. Run a charger detection algorithm manually*/
		muic_run_chgdet(pmuic);
		/* 3. Get the result by polling or via an interrupt */
		mdev = muic_get_chgtyp_to_mdev(pmuic);
		/* 4. Noti. */
		if ( mdev != ATTACHED_DEV_NONE_MUIC)
			muic_handle_cc_supported_dev(pmuic, mdev);
		else
			pr_info("%s: invalid mdev:%d\n", __func__, mdev);
	}

	return 0;
}

static int ccic_handle_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	ccic_notifier_rid_t rid = *(ccic_notifier_rid_t *)data;
	struct max77833_muic_data *muic_data =
		container_of(nb, struct max77833_muic_data, ccic_nb);
	int intr = MUIC_INTR_DETACH;
	int mdev = ATTACHED_DEV_NONE_MUIC;

	pr_info("noti : status:%d rid:%d\n",(int) action, (int)rid);
	switch (action) {
		case CCIC_NOTIFY_ATTACH:
			intr = MUIC_INTR_ATTACH;
			pr_info("%s: action=CCIC_NOTIFY_ATTACH\n", __func__);
			break;
		case CCIC_NOTIFY_DETACH:
			pr_info("%s: action=CCIC_NOTIFY_DETACH\n", __func__);
			break;
		default :
			pr_info("%s: action=Undefined -> discarded.\n", __func__);
			break;
	}

	switch (rid) {
		case RID_UNDEFINED:
			pr_info("%s: RID_UNDEFINED -> continue.\n", __func__);
			break;
		case RID_000K:
			pr_info("%s: RID_000K:OTG\n", __func__);
			mdev = ATTACHED_DEV_OTG_MUIC;
			break;
		case RID_001K:
			pr_info("%s: RID_001K:MHL\n", __func__);
			mdev = ATTACHED_DEV_MHL_MUIC;
			break;
		case RID_255K:
			pr_info("%s: RID_255K:JIG_USB_OFF\n", __func__);
			mdev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
			break;
		case RID_301K:
			pr_info("%s: RID_301K:JIG_USB_ON\n", __func__);
			mdev = ATTACHED_DEV_JIG_USB_ON_MUIC;
			break;
		case RID_523K:
			pr_info("%s: RID_523K:JIG_UART_OFF\n", __func__);
			mdev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
			break;
		case RID_619K:
			pr_info("%s: RID_619K:JIG_UART_ON\n", __func__);
			mdev = ATTACHED_DEV_JIG_UART_ON_MUIC;
			break;
		case RID_OPEN:
			pr_info("%s: RID_OPEN:Detach\n", __func__);
			intr = MUIC_INTR_DETACH;
			mdev = ATTACHED_DEV_NONE_MUIC;
			break;
		default :
			pr_info("%s: rid=Undefined. -> discarded\n", __func__);
			return 0;
	}

	if ((action == CCIC_NOTIFY_ATTACH) && (rid == RID_UNDEFINED)) {
		pr_info("%s: Handling a legacy TA/USB.\n", __func__);
		muic_handle_cc_legacy_TA_USB(muic_data, true);
		return NOTIFY_DONE;
	}

	if (action == CCIC_NOTIFY_DETACH) {
		muic_handle_cc_path_open(muic_data);
		if (muic_cc_is_otg_test) {
			msleep(300);
			muic_handle_cc_OTG(muic_data, false);
		}

		muic_handle_cc_legacy_TA_USB(muic_data, false);

		pr_info("%s: DETACH done.\n", __func__);
		return NOTIFY_DONE;
	}

	//mutex_lock(&muic_data->muic_mutex);
	if (muic_handle_cc_path(muic_data, intr, mdev) < 0) {
		/* Process OTG device */
		if (mdev == ATTACHED_DEV_OTG_MUIC)
			muic_handle_cc_OTG(muic_data, true);
		else
			muic_handle_cc_event(muic_data, intr, mdev);
	}
	//mutex_unlock(&muic_data->muic_mutex);

	return NOTIFY_DONE;
}


static void muic_register_ccic_notifier(struct max77833_muic_data *muic_data)
{
	int use_cc_noti = 1;

	if (use_cc_noti) {
		pr_info("%s: Registering CCIC_NOTIFY_DEV_MUIC.\n", __func__);
		ccic_notifier_register(&muic_data->ccic_nb,
			       ccic_handle_notification, CCIC_NOTIFY_DEV_MUIC);
	} else
		pr_info("%s: Registering CCIC notification skipped.\n", __func__);
}

static void muic_set_cc_noti(int attached, int rid)
{
	pr_info("%s: attached=%d, rid=%d\n", __func__, attached, rid);

	attached = attached ? CCIC_NOTIFY_ATTACH : CCIC_NOTIFY_DETACH;

	ccic_notifier_test(attached, rid);
}
#endif

static int max77833_muic_probe(struct platform_device *pdev)
{
	struct max77833_dev *max77833 = dev_get_drvdata(pdev->dev.parent);
	struct max77833_platform_data *mfd_pdata = dev_get_platdata(max77833->dev);
	struct max77833_muic_data *muic_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_data = kzalloc(sizeof(struct max77833_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	if (!mfd_pdata) {
		pr_err("%s: failed to get max77833 mfd platform data\n", __func__);
		ret = -ENOMEM;
		goto err_kfree;
	}

#if defined(CONFIG_OF)
	ret = of_max77833_muic_dt(muic_data);
	if (ret < 0) {
		pr_err("%s:%s not found muic dt! ret[%d]\n", MUIC_DEV_NAME, __func__, ret);
	}

#endif /* CONFIG_OF */

	muic_data->dev = &pdev->dev;
	mutex_init(&muic_data->muic_mutex);
	mutex_init(&muic_data->reset_mutex);
//	mutex_init(&muic_data->muic_cmd_queue.command_mutex);
	muic_data->i2c = max77833->muic;
	muic_data->mfd_pdata = mfd_pdata;
	muic_data->pdata = &muic_pdata;
	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->is_muic_ready = false;
	muic_data->is_otg_test = false;
	muic_data->is_factory_start = false;
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	max77833_muic_set_afc_ready(muic_data, false);
#endif
	muic_data->adcmode = MAX77833_MUIC_IDMODE_NONE;

	muic_data->muic_cmd_queue.front = NULL;
	muic_data->muic_cmd_queue.rear = NULL;

	platform_set_drvdata(pdev, muic_data);

	// For PASS4/Old rev onebinary
	pass_rev = max77833->pmic_rev;

	if (muic_data->pdata->init_gpio_cb) {
		ret = muic_data->pdata->init_gpio_cb(get_switch_sel());
		if (ret) {
			pr_err("%s: failed to init gpio(%d)\n", __func__, ret);
			goto fail;
		}
	}

	mutex_lock(&muic_data->muic_mutex);

	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &max77833_muic_group);
	if (ret) {
		pr_err("%s: failed to create max77833 muic attribute group\n",
				__func__);
		goto fail_sysfs_create;
	}
	dev_set_drvdata(switch_device, muic_data);

	if (muic_data->pdata->init_switch_dev_cb)
		muic_data->pdata->init_switch_dev_cb();

	ret = max77833_muic_init_regs(muic_data);
	if (ret < 0) {
		pr_err("%s:%s Failed to initialize MUIC irq:%d\n",
				MUIC_DEV_NAME, __func__, ret);
		goto fail_init_irq;
	}

	mutex_unlock(&muic_data->muic_mutex);

#if defined(CONFIG_MUIC_DISABLE_INTERRUPT)
	pr_info("%s:%s CC supported.\n", MUIC_DEV_NAME, __func__);
#else
	/* initial cable detection */
	max77833_muic_init_detect(muic_data);
#endif

#if defined(CONFIG_CCIC_NOTIFIER)
	muic_register_ccic_notifier(muic_data);
#endif /* CONFIG_CCIC_NOTIFIER */

	return 0;

fail_init_irq:
	if (muic_data->pdata->cleanup_switch_dev_cb)
		muic_data->pdata->cleanup_switch_dev_cb();
	sysfs_remove_group(&switch_device->kobj, &max77833_muic_group);
fail_sysfs_create:
	mutex_unlock(&muic_data->muic_mutex);
fail:
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&muic_data->muic_mutex);
	mutex_destroy(&muic_data->reset_mutex);
//	mutex_destroy(&muic_data->muic_cmd_queue.command_mutex);
err_kfree:
	kfree(muic_data);
err_return:
	return ret;
}

static int max77833_muic_remove(struct platform_device *pdev)
{
	struct max77833_muic_data *muic_data = platform_get_drvdata(pdev);

	sysfs_remove_group(&switch_device->kobj, &max77833_muic_group);

	if (muic_data) {
		pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

		max77833_muic_free_irqs(muic_data);

		if (muic_data->pdata->cleanup_switch_dev_cb)
			muic_data->pdata->cleanup_switch_dev_cb();

		platform_set_drvdata(pdev, NULL);
		mutex_destroy(&muic_data->muic_mutex);
		mutex_destroy(&muic_data->reset_mutex);
//		mutex_destroy(&muic_data->muic_cmd_queue.command_mutex);
		kfree(muic_data);
	}

	return 0;
}

void max77833_muic_shutdown(struct device *dev)
{
	struct max77833_muic_data *muic_data = dev_get_drvdata(dev);
	struct i2c_client *i2c;
	struct max77833_dev *max77833;

	pr_info("%s:%s +\n", MUIC_DEV_NAME, __func__);

	sysfs_remove_group(&switch_device->kobj, &max77833_muic_group);

	if (!muic_data) {
		pr_err("%s:%s no drvdata\n", MUIC_DEV_NAME, __func__);
		goto out;
	}

	max77833_muic_free_irqs(muic_data);

	i2c = muic_data->i2c;

	if (!i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		goto out_cleanup;
	}

	max77833 = i2c_get_clientdata(i2c);
	pr_info("%s:%s max77833->i2c_lock.count.counter=%d\n", MUIC_DEV_NAME,
		__func__, max77833->i2c_lock.count.counter);

out_cleanup:
	if (muic_data->pdata && muic_data->pdata->cleanup_switch_dev_cb)
		muic_data->pdata->cleanup_switch_dev_cb();

	mutex_destroy(&muic_data->muic_mutex);
	mutex_destroy(&muic_data->reset_mutex);
//	mutex_destroy(&muic_data->muic_cmd_queue.command_mutex);
	kfree(muic_data);

out:
	pr_info("%s:%s -\n", MUIC_DEV_NAME, __func__);
}

static struct platform_driver max77833_muic_driver = {
	.driver		= {
		.name	= MUIC_DEV_NAME,
		.owner	= THIS_MODULE,
		.shutdown = max77833_muic_shutdown,
	},
	.probe		= max77833_muic_probe,
	.remove		= max77833_muic_remove,
};

static int __init max77833_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return platform_driver_register(&max77833_muic_driver);
}
module_init(max77833_muic_init);

static void __exit max77833_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	platform_driver_unregister(&max77833_muic_driver);
}
module_exit(max77833_muic_exit);

MODULE_DESCRIPTION("Maxim MAX77833 MUIC driver");
MODULE_AUTHOR("<insun77.choi@samsung.com>");
MODULE_LICENSE("GPL");
