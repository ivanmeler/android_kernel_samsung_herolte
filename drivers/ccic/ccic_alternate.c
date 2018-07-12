/*
 * driver/ccic/ccic_alternate.c - S2MM005 USB CCIC Alternate mode driver
 *
 * Copyright (C) 2016 Samsung Electronics
 * Author: Wookwang Lee <wookwang.lee@samsung.com>
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
#include <linux/ccic/s2mm005.h>
#include <linux/ccic/s2mm005_ext.h>
#include <linux/ccic/ccic_alternate.h>
#include <asm/byteorder.h>
#include "ccic_misc.h"
#if defined(CONFIG_USB_HOST_NOTIFY)
#include <linux/usb_notify.h>
#endif
/* switch device header */
#ifndef CONFIG_SWITCH
#error "ERROR: CONFIG_SWITCH is not set."
#endif /* CONFIG_SWITCH */
#include <linux/switch.h>
/********************************************
 *
 * s2mm005_cc.c called s2mm005_alternate.c
 *
 ********************************************
*/
#if defined(CONFIG_CCIC_ALTERNATE_MODE)
extern struct device *ccic_device;
static struct switch_dev switch_dock = {
	.name = "ccic_dock",
};

static char VDM_MSG_IRQ_State_Print[9][40] = {
	{"bFLAG_Vdm_Reserve_b0"},
	{"bFLAG_Vdm_Discover_ID"},
	{"bFLAG_Vdm_Discover_SVIDs"},
	{"bFLAG_Vdm_Discover_MODEs"},
	{"bFLAG_Vdm_Enter_Mode"},
	{"bFLAG_Vdm_Exit_Mode"},
	{"bFLAG_Vdm_Attention"},
	{"bFlag_Vdm_DP_Status_Update"},
	{"bFlag_Vdm_DP_Configure"},
};

static char DP_Pin_Assignment_Print[7][40] = {
	{"DP_Pin_Assignment_None"},
	{"DP_Pin_Assignment_A"},
	{"DP_Pin_Assignment_B"},
	{"DP_Pin_Assignment_C"},
	{"DP_Pin_Assignment_D"},
	{"DP_Pin_Assignment_E"},
	{"DP_Pin_Assignment_F"},
};
/*
*********************************************
*
*  Alternate mode processing
*
*********************************************
*/
int ccic_register_switch_device(int mode)
{
	int ret = 0;

	if (mode) {
		ret = switch_dev_register(&switch_dock);
		if (ret < 0) {
			pr_err("%s: Failed to register dock switch(%d)\n",
			       __func__, ret);
			return -ENODEV;
		}
	} else {
		switch_dev_unregister(&switch_dock);
	}
	return 0;
}

static void ccic_send_dock_intent(int type)
{
	pr_info("%s: CCIC dock type(%d)\n", __func__, type);
	switch_set_state(&switch_dock, type);
}

void ccic_send_dock_uevent(u32 vid, u32 pid, int state)
{
	char switch_string[32];
	char pd_ids_string[32];
	char *envp[3] = { switch_string, pd_ids_string, NULL };

	pr_info("%s: CCIC dock : USBPD_IPS=%04x:%04x SWITCH_STATE=%d\n",
			__func__,
			le16_to_cpu(vid),
			le16_to_cpu(pid),
			state);

	if (IS_ERR(ccic_device)) {
		pr_err("%s CCIC ERROR: Failed to send a dock uevent!\n",
				__func__);
		return;
	}

	snprintf(switch_string, 32, "SWITCH_STATE=%d", state);
	snprintf(pd_ids_string, 32, "USBPD_IDS=%04x:%04x",
			le16_to_cpu(vid),
			le16_to_cpu(pid));
	kobject_uevent_env(&ccic_device->kobj, KOBJ_CHANGE, envp);
}

void acc_detach_check(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct s2mm005_data *usbpd_data =
		container_of(delay_work, struct s2mm005_data, acc_detach_work);

	pr_info("%s: usbpd_data->pd_state : %d\n", __func__,
		usbpd_data->pd_state);
	if (usbpd_data->pd_state == State_PE_Initial_detach) {
		if (usbpd_data->acc_type != CCIC_DOCK_DETACHED) {
			if (usbpd_data->acc_type != CCIC_DOCK_NEW)
				ccic_send_dock_intent(CCIC_DOCK_DETACHED);
			ccic_send_dock_uevent(usbpd_data->Vendor_ID,
					usbpd_data->Product_ID,
					CCIC_DOCK_DETACHED);
			usbpd_data->acc_type = CCIC_DOCK_DETACHED;
			usbpd_data->Vendor_ID = 0;
			usbpd_data->Product_ID = 0;
		}
	}
}

void set_enable_powernego(int mode)
{
	struct s2mm005_data *usbpd_data;
	u8 W_DATA[2];

	if (!ccic_device)
		return;
	usbpd_data = dev_get_drvdata(ccic_device);
	if (!usbpd_data)
		return;

	if (mode) {
		W_DATA[0] = 0x3;
		W_DATA[1] = 0x42;
		s2mm005_write_byte(usbpd_data->i2c, 0x10, &W_DATA[0], 2);
		pr_info("%s : Power nego start\n", __func__);
	} else
		pr_info("%s : Power nego stop\n", __func__);
}

void set_enable_alternate_mode(int mode)
{
	struct s2mm005_data *usbpd_data;
	static int check_is_driver_loaded;
	static int prev_alternate_mode;
	u8 W_DATA[2];

	if (!ccic_device)
		return;
	usbpd_data = dev_get_drvdata(ccic_device);
	if (!usbpd_data)
		return;

#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	store_usblog_notify(NOTIFY_ALTERNATEMODE, (void *)&mode, NULL);
#endif
	if ((mode & ALTERNATE_MODE_NOT_READY) &&
	    (mode & ALTERNATE_MODE_READY)) {
		pr_info("%s : mode is invalid!\n", __func__);
		return;
	}
	if ((mode & ALTERNATE_MODE_START) && (mode & ALTERNATE_MODE_STOP)) {
		pr_info("%s : mode is invalid!\n", __func__);
		return;
	}
	if (mode & ALTERNATE_MODE_RESET) {
		pr_info("%s : mode is reset! check_is_driver_loaded=%d, prev_alternate_mode=%d\n",
			__func__, check_is_driver_loaded, prev_alternate_mode);
		if (check_is_driver_loaded &&
		    (prev_alternate_mode == ALTERNATE_MODE_START)) {
			W_DATA[0] = 0x3;
			W_DATA[1] = 0x31;
			s2mm005_write_byte(usbpd_data->i2c, 0x10, &W_DATA[0], 2);
			pr_info("%s : alternate mode is reset as start!\n",
				__func__);
			prev_alternate_mode = ALTERNATE_MODE_START;
		} else if (check_is_driver_loaded &&
			   (prev_alternate_mode == ALTERNATE_MODE_STOP)) {
			W_DATA[0] = 0x3;
			W_DATA[1] = 0x33;
			s2mm005_write_byte(usbpd_data->i2c, 0x10, &W_DATA[0], 2);
			pr_info("%s : alternate mode is reset as stop!\n",
				__func__);
			prev_alternate_mode = ALTERNATE_MODE_STOP;
		} else
			;
	} else {
		if (mode & ALTERNATE_MODE_NOT_READY) {
			check_is_driver_loaded = 0;
			pr_info("%s : alternate mode is not ready!\n", __func__);
		} else if (mode & ALTERNATE_MODE_READY) {
			check_is_driver_loaded = 1;
			pr_info("%s : alternate mode is ready!\n", __func__);
		} else
			;

		if (check_is_driver_loaded) {
			if (mode & ALTERNATE_MODE_START) {
				W_DATA[0] = 0x3;
				W_DATA[1] = 0x31;
				s2mm005_write_byte(usbpd_data->i2c, 0x10,
						   &W_DATA[0], 2);
				pr_info("%s : alternate mode is started!\n",
					__func__);
				prev_alternate_mode = ALTERNATE_MODE_START;
				set_enable_powernego(1);
			} else if (mode & ALTERNATE_MODE_STOP) {
				W_DATA[0] = 0x3;
				W_DATA[1] = 0x33;
				s2mm005_write_byte(usbpd_data->i2c, 0x10,
						   &W_DATA[0], 2);
				pr_info("%s : alternate mode is stopped!\n",
					__func__);
				prev_alternate_mode = ALTERNATE_MODE_STOP;
			}
		}
	}
}

void set_clear_discover_mode(void)
{
	struct s2mm005_data *usbpd_data;
	u8 W_DATA[2];

	if (!ccic_device)
		return;
	usbpd_data = dev_get_drvdata(ccic_device);
	if (!usbpd_data)
		return;

	W_DATA[0] = 0x3;
	W_DATA[1] = 0x32;

	s2mm005_write_byte(usbpd_data->i2c, 0x10, &W_DATA[0], 2);

	pr_info("%s : clear discover mode!\n", __func__);
}

void set_host_turn_on_event(int mode)
{
	struct s2mm005_data *usbpd_data;

	if (!ccic_device)
		return;
	usbpd_data = dev_get_drvdata(ccic_device);
	if (!usbpd_data)
		return;

	pr_info("%s : current_set is %d!\n", __func__, mode);
	if (mode) {
		usbpd_data->host_turn_on_event = 1;
		wake_up_interruptible(&usbpd_data->host_turn_on_wait_q);
	} else {
		usbpd_data->host_turn_on_event = 0;
	}
}

static int process_check_accessory(void *data)
{
	struct s2mm005_data *usbpd_data = data;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	uint16_t vid = usbpd_data->Vendor_ID;
	uint16_t pid = usbpd_data->Product_ID;
	uint16_t acc_type = CCIC_DOCK_DETACHED;

	/* detect Gear VR */
	if (usbpd_data->acc_type == CCIC_DOCK_DETACHED) {
		if (vid == SAMSUNG_VENDOR_ID) {
			switch (pid) {
			/* GearVR: Reserved GearVR PID+6 */
			case GEARVR_PRODUCT_ID:
			case GEARVR_PRODUCT_ID_1:
			case GEARVR_PRODUCT_ID_2:
			case GEARVR_PRODUCT_ID_3:
			case GEARVR_PRODUCT_ID_4:
			case GEARVR_PRODUCT_ID_5:
				acc_type = CCIC_DOCK_HMT;
				pr_info("%s : Samsung Gear VR connected.\n",
					__func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify,
						     USB_CCIC_VR_USE_COUNT);
#endif
				break;
			case DEXDOCK_PRODUCT_ID:
				acc_type = CCIC_DOCK_DEX;
				pr_info("%s : Samsung DEX connected.\n",
					__func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify,
						     USB_CCIC_DEX_USE_COUNT);
#endif
				break;
			case DEXPAD_PRODUCT_ID:
				acc_type = CCIC_DOCK_DEXPAD;
				pr_info("%s : Samsung DEX PADconnected.\n", __func__);
#if defined(CONFIG_USB_HOST_NOTIFY) && defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_DEX_USE_COUNT);
#endif
				break;
			case HDMI_PRODUCT_ID:
				acc_type = CCIC_DOCK_HDMI;
				pr_info("%s : Samsung HDMI connected.\n",
					__func__);
				break;
			case UVDM_PROTOCOL_ID:
				acc_type = CCIC_DOCK_UVDM;
				pr_info("%s : Samsung UVDM device connected.\n",
					__func__);
			default:
				acc_type = CCIC_DOCK_NEW;
				pr_info("%s : default device connected.\n",
					__func__);
				break;
			}
		} else if (vid == SAMSUNG_MPA_VENDOR_ID) {
			switch (pid) {
			case MPA_PRODUCT_ID:
				acc_type = CCIC_DOCK_MPA;
				pr_info("%s : Samsung MPA connected.\n",
					__func__);
				break;
			default:
				acc_type = CCIC_DOCK_NEW;
				pr_info("%s : default device connected.\n",
					__func__);
				break;
			}
		}
		usbpd_data->acc_type = acc_type;
	} else
		acc_type = usbpd_data->acc_type;

	if (acc_type != CCIC_DOCK_NEW)
		ccic_send_dock_intent(acc_type);
	
	ccic_send_dock_uevent(vid, pid, acc_type);
	return 1;
}

static int process_discover_identity(void *data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_DIS_ID;
	uint8_t ReadMSG[32] = {0,};
	int ret = 0;

	/* Message Type Definition */
	U_DATA_MSG_ID_HEADER_Type *DATA_MSG_ID =
		(U_DATA_MSG_ID_HEADER_Type *)&ReadMSG[8];
	U_PRODUCT_VDO_Type *DATA_MSG_PRODUCT =
		(U_PRODUCT_VDO_Type *)&ReadMSG[16];

	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return ret;
	}

	usbpd_data->is_sent_pin_configuration = 0;
	usbpd_data->is_samsung_accessory_enter_mode = 0;
	usbpd_data->Vendor_ID = DATA_MSG_ID->BITS.USB_Vendor_ID;
	usbpd_data->Product_ID = DATA_MSG_PRODUCT->BITS.Product_ID;
	usbpd_data->Device_Version = DATA_MSG_PRODUCT->BITS.Device_Version;

	dev_info(&i2c->dev, "%s Vendor_ID : 0x%X, Product_ID : 0x%X Device Version 0x%X\n",
		 __func__, usbpd_data->Vendor_ID, usbpd_data->Product_ID,
		 usbpd_data->Device_Version);
	if (process_check_accessory(usbpd_data))
		dev_info(&i2c->dev, "%s : Samsung Accessory Connected.\n",
			 __func__);
	return 0;
}
#define MAX_INPUT_DATA (255)

static int process_discover_svids(void *data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_DIS_SVID;
	uint8_t ReadMSG[32] = {0,};
	int ret = 0;
	int timeleft = 0;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	/* Message Type Definition */
	U_VDO1_Type  *DATA_MSG_VDO1 = (U_VDO1_Type *)&ReadMSG[8];

	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return ret;
	}

	usbpd_data->SVID_0 = DATA_MSG_VDO1->BITS.SVID_0;
	usbpd_data->SVID_1 = DATA_MSG_VDO1->BITS.SVID_1;

	if (usbpd_data->SVID_0 == TypeC_DP_SUPPORT) {
		if (usbpd_data->is_client == CLIENT_ON) {
			ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_MUIC,
				CCIC_NOTIFY_ID_ATTACH, 0, 0, 0);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
			usbpd_data->power_role = DUAL_ROLE_PROP_PR_NONE;
#endif
			ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				0, USB_STATUS_NOTIFY_DETACH, 0);
			usbpd_data->is_client = CLIENT_OFF;
		}

		if (usbpd_data->is_host == HOST_OFF) {
			/* muic */
			ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_MUIC,
				CCIC_NOTIFY_ID_ATTACH, 1, 1, 0);
			/* otg */
			usbpd_data->is_host = HOST_ON_BY_RD;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
			if (usbpd_data->func_state & (0x1 << 25)) {
				usbpd_data->power_role = DUAL_ROLE_PROP_PR_SRC;
				/* add to turn on external 5V */
#if defined(CONFIG_USB_HOST_NOTIFY)
				if (!is_blocked(o_notify,
						NOTIFY_BLOCK_TYPE_HOST))
#endif
					vbus_turn_on_ctrl(1);
			} else {
				usbpd_data->power_role = DUAL_ROLE_PROP_PR_SNK;
			}
#endif
			ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				1, USB_STATUS_NOTIFY_ATTACH_DFP, 0);
		}
		usbpd_data->dp_is_connect = 1;
		/*
		 * If you want to support USB SuperSpeed when you connect
		 * Display port dongle, You should change dp_hs_connect depend
		 * on Pin assignment.If DP use 4lane(Pin Assignment C,E,A),
		 * dp_hs_connect is 1. USB can support HS.If DP use 2lane(Pin Assigment B,D,F), dp_hs_connect is 0. USB
		 * can support SS
		 */
		 usbpd_data->dp_hs_connect = 1;

		ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_DP, CCIC_NOTIFY_ID_DP_CONNECT,
				CCIC_NOTIFY_ATTACH, usbpd_data->Vendor_ID,
				usbpd_data->Product_ID);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_DP_USE_COUNT);
#endif
		timeleft = wait_event_interruptible_timeout(
				usbpd_data->host_turn_on_wait_q,
				usbpd_data->host_turn_on_event,
				(usbpd_data->host_turn_on_wait_time)*HZ);

		dev_info(&i2c->dev, "%s host turn on wait = %d\n",
			 __func__, timeleft);

		ccic_event_work(usbpd_data, CCIC_NOTIFY_DEV_USB_DP,
			CCIC_NOTIFY_ID_USB_DP, usbpd_data->dp_is_connect,
			usbpd_data->dp_hs_connect, 0);
	}

	dev_info(&i2c->dev, "%s SVID_0 : 0x%X, SVID_1 : 0x%X\n",
		__func__, usbpd_data->SVID_0, usbpd_data->SVID_1);
	return 0;
}

static int process_discover_modes(void *data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_MODE;
	uint8_t ReadMSG[32] = {0,};
	uint8_t W_DATA[3] = {0,};
	int ret = 0;
	DIS_MODE_DP_CAPA_Type *pDP_DIS_MODE =
		(DIS_MODE_DP_CAPA_Type *)&ReadMSG[0];
	U_DATA_MSG_VDM_HEADER_Type *DATA_MSG_VDM =
		(U_DATA_MSG_VDM_HEADER_Type *)&ReadMSG[4];

	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return ret;
	}

	dev_info(&i2c->dev, "%s : vendor_id = 0x%04x , svid_1 = 0x%04x\n",
		 __func__, DATA_MSG_VDM->BITS.Standard_Vendor_ID,
		 usbpd_data->SVID_1);
	if (DATA_MSG_VDM->BITS.Standard_Vendor_ID == TypeC_DP_SUPPORT && usbpd_data->SVID_0 == TypeC_DP_SUPPORT) {
		/* pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS. */
		dev_info(&i2c->dev, "pDP_DIS_MODE->MSG_HEADER.DATA = 0x%08X\n\r", pDP_DIS_MODE->MSG_HEADER.DATA);
		dev_info(&i2c->dev, "pDP_DIS_MODE->DATA_MSG_VDM_HEADER.DATA = 0x%08X\n\r", pDP_DIS_MODE->DATA_MSG_VDM_HEADER.DATA);
		dev_info(&i2c->dev, "pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.DATA = 0x%08X\n\r", pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.DATA);

		if (pDP_DIS_MODE->MSG_HEADER.BITS.Number_of_obj > 1) {
			if (((pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_UFP_D_Capable)
			     && (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Receptacle_Indication == num_USB_TYPE_C_Receptacle))
			    || ((pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_DFP_D_Capable)
			       && (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Receptacle_Indication == num_USB_TYPE_C_PLUG))) {
				usbpd_data->pin_assignment =
					pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.UFP_D_Pin_Assignments;
				dev_info(&i2c->dev, "%s support UFP_D 0x%08x\n",
					 __func__, usbpd_data->pin_assignment);
			} else if (((pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_DFP_D_Capable)
				     && (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Receptacle_Indication == num_USB_TYPE_C_Receptacle))
				   || ((pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_UFP_D_Capable)
					 && (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Receptacle_Indication == num_USB_TYPE_C_PLUG))) {

				usbpd_data->pin_assignment = pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.DFP_D_Pin_Assignments;
				dev_info(&i2c->dev, "%s support DFP_D 0x%08x\n",
					 __func__, usbpd_data->pin_assignment);
			} else if (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_DFP_D_and_UFP_D_Capable) {
				if (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Receptacle_Indication == num_USB_TYPE_C_PLUG) {
					usbpd_data->pin_assignment = pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.DFP_D_Pin_Assignments;
					dev_info(&i2c->dev, "%s support DFP_D 0x%08x\n", __func__, usbpd_data->pin_assignment);
				} else {
					usbpd_data->pin_assignment = pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.UFP_D_Pin_Assignments;
					dev_info(&i2c->dev, "%s support UFP_D 0x%08x\n", __func__, usbpd_data->pin_assignment);
				}
			} else {
				usbpd_data->pin_assignment = DP_PIN_ASSIGNMENT_NODE;
				dev_info(&i2c->dev, "%s there is not valid object information!!!\n", __func__);
			}
		}
	}

	if (DATA_MSG_VDM->BITS.Standard_Vendor_ID == SAMSUNG_VENDOR_ID) {
		dev_info(&i2c->dev, "%s : dex mode discover_mode ack status!\n",
			 __func__);
		/* pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.*/
		dev_info(&i2c->dev, "pDP_DIS_MODE->MSG_HEADER.DATA = 0x%08X\n\r", pDP_DIS_MODE->MSG_HEADER.DATA);
		dev_info(&i2c->dev, "pDP_DIS_MODE->DATA_MSG_VDM_HEADER.DATA = 0x%08X\n\r", pDP_DIS_MODE->DATA_MSG_VDM_HEADER.DATA);
		dev_info(&i2c->dev, "pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.DATA = 0x%08X\n\r", pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.DATA);

		REG_ADD = REG_I2C_SLV_CMD;
		W_DATA[0] = MODE_INTERFACE;	/* Mode Interface */
		W_DATA[1] = PD_NEXT_STATE;	/* Select mode as pd next state*/
		W_DATA[2] = 89;			/* PD next state*/
		ret = s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 3);
		if (ret < 0) {
			dev_err(&i2c->dev, "%s has i2c write error.\n",
				__func__);
			return ret;
		}
	}

	dev_info(&i2c->dev, "%s\n", __func__);
	set_clear_discover_mode();
	return 0;
}

static int process_enter_mode(void *data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD;
	uint8_t ReadMSG[32] = {0,};
	int ret = 0;

	U_DATA_MSG_VDM_HEADER_Type *DATA_MSG_VDM =
		(U_DATA_MSG_VDM_HEADER_Type *)&ReadMSG[4];

	dev_info(&i2c->dev, "%s\n", __func__);

	REG_ADD = REG_RX_ENTER_MODE;
	/* Message Type Definition */
	DATA_MSG_VDM = (U_DATA_MSG_VDM_HEADER_Type *)&ReadMSG[4];
	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c read error.\n", __func__);
		return ret;
	}

	if (DATA_MSG_VDM->BITS.VDM_command_type == 1) {
		dev_info(&i2c->dev, "%s : EnterMode ACK.\n", __func__);
		if (DATA_MSG_VDM->BITS.Standard_Vendor_ID == SAMSUNG_VENDOR_ID) {
			usbpd_data->is_samsung_accessory_enter_mode = 1;
			dev_info(&i2c->dev, "%s : dex mode enter_mode ack status!\n", __func__);
		}
	} else
		dev_info(&i2c->dev, "%s : EnterMode NAK.\n", __func__);

	return 0;
}

static int process_attention(void *data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_DIS_ATTENTION;
	uint8_t ReadMSG[32] = {0,};
	uint8_t W_DATA[3] = {0,};
	int ret = 0;
	int i;
	int hpd = 0;
	int hpdirq = 0;
	int pin_assignment = 0;

	VDO_MESSAGE_Type *VDO_MSG;
	DIS_ATTENTION_MESSAGE_DP_STATUS_Type *DP_ATTENTION;
	u8 multi_func_preference = 0;

	pr_info("%s\n", __func__);
	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return ret;
	}

	if (usbpd_data->SVID_0 == TypeC_DP_SUPPORT) {
		DP_ATTENTION = (DIS_ATTENTION_MESSAGE_DP_STATUS_Type *)&ReadMSG[0];

		dev_info(&i2c->dev, "%s DP_ATTENTION = 0x%08X\n", __func__,
			 DP_ATTENTION->DATA_MSG_DP_STATUS.DATA);
		if (usbpd_data->is_sent_pin_configuration == 0) {
			/* 1. Pin Assignment */
			REG_ADD = 0x10;
			W_DATA[0] = MODE_INTERFACE;	/* Mode Interface */
			W_DATA[1] = DP_ALT_MODE_REQ;   /* DP Alternate Mode Request */
			W_DATA[2] = 0;	/* DP Pin Assign select */


			multi_func_preference =
				DP_ATTENTION->DATA_MSG_DP_STATUS.BITS.Multi_Function_Preference;
			if (multi_func_preference == 1) {
				if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_D) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_D;
					pin_assignment = CCIC_NOTIFY_DP_PIN_D;
				} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_B) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_B;
					pin_assignment = CCIC_NOTIFY_DP_PIN_B;
				} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_F) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_F;
					pin_assignment = CCIC_NOTIFY_DP_PIN_F;
				} else {
					pin_assignment = CCIC_NOTIFY_DP_PIN_UNKNOWN;
					pr_info("wrong pin assignment value\n");
				}
			} else {
				if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_C) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_C;
					pin_assignment = CCIC_NOTIFY_DP_PIN_C;
				} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_E) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_E;
					pin_assignment = CCIC_NOTIFY_DP_PIN_E;
				} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_A) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_A;
					pin_assignment = CCIC_NOTIFY_DP_PIN_A;
				} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_D) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_D;
					pin_assignment = CCIC_NOTIFY_DP_PIN_D;
				} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_B) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_B;
					pin_assignment = CCIC_NOTIFY_DP_PIN_B;
				} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_F) {
					W_DATA[2] = DP_PIN_ASSIGNMENT_F;
					pin_assignment = CCIC_NOTIFY_DP_PIN_F;
				} else {
					pin_assignment = CCIC_NOTIFY_DP_PIN_UNKNOWN;
					pr_info("wrong pin assignment value\n");
				}
			}
			usbpd_data->dp_selected_pin = pin_assignment;

			dev_info(&i2c->dev, "%s multi_func_preference %d  %s\n",
				 __func__, multi_func_preference,
				 DP_Pin_Assignment_Print[pin_assignment]);
			ret = s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 3);
			if (ret < 0) {
				dev_err(&i2c->dev, "%s has i2c write error.\n",
					__func__);
						return ret;
					}
		usbpd_data->is_sent_pin_configuration = 1;
		} else
			dev_info(&i2c->dev, "%s : pin configuration is already sent as %s!\n", __func__,
				DP_Pin_Assignment_Print[usbpd_data->dp_selected_pin]);

		if (DP_ATTENTION->DATA_MSG_DP_STATUS.BITS.HPD_State == 1)
			hpd = CCIC_NOTIFY_HIGH;
		else if (DP_ATTENTION->DATA_MSG_DP_STATUS.BITS.HPD_State == 0)
			hpd = CCIC_NOTIFY_LOW;
		if (DP_ATTENTION->DATA_MSG_DP_STATUS.BITS.HPD_Interrupt == 1)
			hpdirq = CCIC_NOTIFY_IRQ;

		ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_DP, CCIC_NOTIFY_ID_DP_HPD,
			hpd, hpdirq, 0);
	} else {
		VDO_MSG = (VDO_MESSAGE_Type *)&ReadMSG[8];

		for (i = 0; i < 6; i++)
			dev_info(&i2c->dev, "%s : VDO_%d : %d\n", __func__,
				 i+1, VDO_MSG->VDO[i]);
	}
	return 0;

}

static int process_dp_status_update(void *data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_DIS_DP_STATUS_UPDATE;
	uint8_t ReadMSG[32] = {0,};
	uint8_t W_DATA[3] = {0,};
	int ret = 0;
	int i;
	u8 multi_func_preference = 0;
	int pin_assignment = 0;
	int hpd = 0;
	int hpdirq = 0;
	VDO_MESSAGE_Type *VDO_MSG;
	DP_STATUS_UPDATE_Type *DP_STATUS;

	pr_info("%s\n", __func__);
	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return ret;
	}

	if (usbpd_data->SVID_0 == TypeC_DP_SUPPORT) {
		DP_STATUS = (DP_STATUS_UPDATE_Type *)&ReadMSG[0];

		dev_info(&i2c->dev, "%s DP_STATUS_UPDATE = 0x%08X\n", __func__,
			 DP_STATUS->DATA_DP_STATUS_UPDATE.DATA);

		if (DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.Port_Connected == 0x00) {
			dev_info(&i2c->dev, "%s : port disconnected!\n",
				 __func__);
		} else {
			if (usbpd_data->is_sent_pin_configuration == 0) {
				/* 1. Pin Assignment */
				REG_ADD = 0x10;
				W_DATA[0] = MODE_INTERFACE;	/* Mode Interface */
				W_DATA[1] = DP_ALT_MODE_REQ;   /* DP Alternate Mode Request */
				W_DATA[2] = 0;	/* DP Pin Assign select */


				multi_func_preference =
					DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.Multi_Function_Preference;
				if (multi_func_preference == 1) {
					if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_D) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_D;
						pin_assignment = CCIC_NOTIFY_DP_PIN_D;
					} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_B) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_B;
						pin_assignment = CCIC_NOTIFY_DP_PIN_B;
					} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_F) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_F;
						pin_assignment = CCIC_NOTIFY_DP_PIN_F;
					} else {
						pr_info("wrong pin assignment value\n");
					}
				} else {
					if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_C) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_C;
						pin_assignment = CCIC_NOTIFY_DP_PIN_C;
					} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_E) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_E;
						pin_assignment = CCIC_NOTIFY_DP_PIN_E;
					} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_A) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_A;
						pin_assignment = CCIC_NOTIFY_DP_PIN_A;
					} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_D) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_D;
						pin_assignment = CCIC_NOTIFY_DP_PIN_D;
					} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_B) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_B;
						pin_assignment = CCIC_NOTIFY_DP_PIN_B;
					} else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_F) {
						W_DATA[2] = DP_PIN_ASSIGNMENT_F;
						pin_assignment = CCIC_NOTIFY_DP_PIN_F;
					} else {
						pr_info("wrong pin assignment value\n");
					}
				}
				usbpd_data->dp_selected_pin = pin_assignment;

				dev_info(&i2c->dev, "%s multi_func_preference %d  %s\n", __func__,
					 multi_func_preference, DP_Pin_Assignment_Print[pin_assignment]);
				ret = s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 3);
				if (ret < 0) {
					dev_err(&i2c->dev, "%s has i2c write error.\n",
						__func__);
					return ret;
				}
				usbpd_data->is_sent_pin_configuration = 1;
			} else {
				dev_info(&i2c->dev, "%s : pin configuration is already sent as %s!\n", __func__,
					 DP_Pin_Assignment_Print[usbpd_data->dp_selected_pin]);
			}
		}

		if (DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.HPD_State == 1)
			hpd = CCIC_NOTIFY_HIGH;
		else if (DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.HPD_State == 0)
			hpd = CCIC_NOTIFY_LOW;

		if (DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.HPD_Interrupt == 1)
			hpdirq = CCIC_NOTIFY_IRQ;

		ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_DP, CCIC_NOTIFY_ID_DP_HPD,
				hpd, hpdirq, 0);
	} else {
		VDO_MSG = (VDO_MESSAGE_Type *)&ReadMSG[8];

		for (i = 0; i < 6; i++)
			dev_info(&i2c->dev, "%s : VDO_%d : %d\n", __func__,
				 i+1, VDO_MSG->VDO[i]);
	}
	return 0;
}

static int process_dp_configure(void *data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_RX_DIS_DP_CONFIGURE;
	uint8_t ReadMSG[32] = {0,};
	uint8_t W_DATA[3] = {0,};
	int ret = 0;
	U_DATA_MSG_VDM_HEADER_Type *DATA_MSG_VDM = (U_DATA_MSG_VDM_HEADER_Type *)&ReadMSG[4];

	dev_info(&i2c->dev, "%s\n", __func__);

	ret = s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s has i2c error.\n", __func__);
		return ret;
	}

	dev_info(&i2c->dev, "%s : vendor_id = 0x%04x , svid_1 = 0x%04x\n",
		 __func__, DATA_MSG_VDM->BITS.Standard_Vendor_ID, usbpd_data->SVID_1);
	if (usbpd_data->SVID_0 == TypeC_DP_SUPPORT) {
		ccic_event_work(usbpd_data, CCIC_NOTIFY_DEV_DP,
			CCIC_NOTIFY_ID_DP_LINK_CONF,
			usbpd_data->dp_selected_pin, 0, 0);
	}
	if (DATA_MSG_VDM->BITS.Standard_Vendor_ID == TypeC_DP_SUPPORT &&
	     usbpd_data->SVID_1 == SAMSUNG_VENDOR_ID) {
		/* write s2mm005 with TypeC_Dex_SUPPORT SVID */
		/* It will start discover mode with that svid */
		dev_info(&i2c->dev, "%s : svid 1 is dex station\n", __func__);
		REG_ADD = REG_I2C_SLV_CMD;
		W_DATA[0] = MODE_INTERFACE;	/* Mode Interface */
		W_DATA[1] = SVID_SELECT;   /* SVID select*/
		W_DATA[2] = 1;	/* SVID select with Samsung vendor ID*/
		ret = s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 3);
		if (ret < 0) {
			dev_err(&i2c->dev, "%s has i2c write error.\n",
				__func__);
			return ret;
		}
	}

	return 0;
}

static void process_alternate_mode(void *data)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint32_t mode = usbpd_data->alternate_state;
	int	ret = 0;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	if (mode) {
		dev_info(&i2c->dev, "%s, mode : 0x%x\n", __func__, mode);

#if defined(CONFIG_USB_HOST_NOTIFY)
		if (o_notify != NULL && is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST)) {
			dev_info(&i2c->dev, "%s, host is blocked, skip all the alternate mode.\n", __func__);
			goto process_error;
		}
#endif
		if (mode & VDM_DISCOVER_ID)
			ret = process_discover_identity(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_DISCOVER_SVIDS)
			ret = process_discover_svids(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_DISCOVER_MODES)
			ret = process_discover_modes(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_ENTER_MODE)
			ret = process_enter_mode(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_DP_STATUS_UPDATE)
			ret = process_dp_status_update(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_DP_CONFIGURE)
			ret = process_dp_configure(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_ATTENTION)
			ret = process_attention(usbpd_data);
process_error:
		usbpd_data->alternate_state = 0;
	}
}

void send_alternate_message(void *data, int cmd)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_VDM_MSG_REQ;
	u8 mode = (u8)cmd;

	dev_info(&i2c->dev, "%s : %s\n", __func__,
		 &VDM_MSG_IRQ_State_Print[cmd][0]);
	s2mm005_write_byte(i2c, REG_ADD, &mode, 1);
}

void receive_alternate_message(void *data, VDM_MSG_IRQ_STATUS_Type *VDM_MSG_IRQ_State)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;

	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_ID) {
		dev_info(&i2c->dev, "%s : %s\n", __func__,
			 &VDM_MSG_IRQ_State_Print[1][0]);
		usbpd_data->alternate_state |= VDM_DISCOVER_ID;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_SVIDs) {
		dev_info(&i2c->dev, "%s : %s\n", __func__,
			 &VDM_MSG_IRQ_State_Print[2][0]);
		usbpd_data->alternate_state |= VDM_DISCOVER_SVIDS;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_MODEs) {
		dev_info(&i2c->dev, "%s : %s\n", __func__,
			 &VDM_MSG_IRQ_State_Print[3][0]);
		usbpd_data->alternate_state |= VDM_DISCOVER_MODES;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Enter_Mode) {
		dev_info(&i2c->dev, "%s : %s\n", __func__,
			 &VDM_MSG_IRQ_State_Print[4][0]);
		usbpd_data->alternate_state |= VDM_ENTER_MODE;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Exit_Mode) {
		dev_info(&i2c->dev, "%s : %s\n", __func__,
			 &VDM_MSG_IRQ_State_Print[5][0]);
		usbpd_data->alternate_state |= VDM_EXIT_MODE;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Attention) {
		dev_info(&i2c->dev, "%s : %s\n", __func__,
			 &VDM_MSG_IRQ_State_Print[6][0]);
		usbpd_data->alternate_state |= VDM_ATTENTION;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_DP_Status_Update) {
		dev_info(&i2c->dev, "%s : %s\n", __func__,
			 &VDM_MSG_IRQ_State_Print[7][0]);
		usbpd_data->alternate_state |= VDM_DP_STATUS_UPDATE;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_DP_Configure) {
		dev_info(&i2c->dev, "%s : %s\n", __func__,
			 &VDM_MSG_IRQ_State_Print[8][0]);
		usbpd_data->alternate_state |= VDM_DP_CONFIGURE;
	}

	process_alternate_mode(usbpd_data);
}

void set_endian(char *src, char *dest, int size)
{
	int i, j;
	int loop;
	int dest_pos;
	int src_pos;

	loop = size / SAMSUNGUVDM_ALIGN;
	loop += (((size % SAMSUNGUVDM_ALIGN) > 0) ? 1:0);

	for (i = 0 ; i < loop ; i++)
		for (j = 0 ; j < SAMSUNGUVDM_ALIGN ; j++) {
			src_pos = SAMSUNGUVDM_ALIGN * i + j;
			dest_pos = SAMSUNGUVDM_ALIGN * i + SAMSUNGUVDM_ALIGN - j - 1;
			dest[dest_pos] = src[src_pos];
		}
}

int get_checksum(char *data, int start_addr, int size)
{
	int checksum = 0;
	int i;

	for (i = 0; i < size; i++)
		checksum += data[start_addr+i];

	printk(" %s \n", __func__);
	return checksum;
}

int set_uvdmset_count(int size)
{
	int ret = 0;

	if (size <= SAMSUNGUVDM_MAXDATA_FIRST_UVDMSET)
		ret = 1;
	else {
		ret = ((size-SAMSUNGUVDM_MAXDATA_FIRST_UVDMSET) / SAMSUNGUVDM_MAXDATA_NORMAL_UVDMSET);
		if (((size-SAMSUNGUVDM_MAXDATA_FIRST_UVDMSET) % SAMSUNGUVDM_MAXDATA_NORMAL_UVDMSET) == 0)
			ret += 1;
		else
			ret += 2;
	}
	return ret;
}

void set_msghedader(void *data, int msg_type, int obj_num)
{
	MSG_HEADER_Type *MSG_HDR;
	uint8_t *SendMSG = (uint8_t *)data;

	MSG_HDR = (MSG_HEADER_Type *)&SendMSG[0];
	MSG_HDR->Message_Type = msg_type;
	MSG_HDR->Number_of_obj = obj_num;
	return;
}
int get_writesize(void *data)
{
	MSG_HEADER_Type *MSG_HDR;
	uint8_t *SendMSG = (uint8_t *)data;

	MSG_HDR = (MSG_HEADER_Type *)&SendMSG[0];
	return ((MSG_HDR->Number_of_obj)*4+4);
}

void set_uvdmheader(void *data, int vendor_id, int vdm_type)
{
	U_UNSTRUCTURED_VDM_HEADER_Type	*UVDM_HEADER;
	U_DATA_MSG_VDM_HEADER_Type *VDM_HEADER;
	uint8_t *SendMSG = (uint8_t *)data;

	UVDM_HEADER = (U_UNSTRUCTURED_VDM_HEADER_Type *)&SendMSG[4];
	UVDM_HEADER->BITS.USB_Vendor_ID = vendor_id;
	UVDM_HEADER->BITS.VDM_TYPE = vdm_type;
	UVDM_HEADER->BITS.VENDOR_DEFINED_MESSAGE = SEC_UVDM_UNSTRUCTURED_VDM;
	VDM_HEADER = (U_DATA_MSG_VDM_HEADER_Type *)&SendMSG[4];
	VDM_HEADER->BITS.VDM_command = 4; //s2mm005 only
	return;
}

void set_sec_uvdmheader(void *data, int pid, bool data_type, int cmd_type,
		bool dir, int total_uvdmset_num, uint8_t received_data)
{
	U_SEC_UVDM_HEADER *SEC_VDM_HEADER;
	uint8_t *SendMSG = (uint8_t *)data;

	SEC_VDM_HEADER = (U_SEC_UVDM_HEADER *)&SendMSG[8];
	SEC_VDM_HEADER->BITS.pid = pid;
	SEC_VDM_HEADER->BITS.data_type = data_type;
	SEC_VDM_HEADER->BITS.command_type = cmd_type;
	SEC_VDM_HEADER->BITS.direction = dir;
	if (dir == DIR_OUT)
		SEC_VDM_HEADER->BITS.total_number_of_uvdm_set = total_uvdmset_num;
	if (data_type == TYPE_SHORT)
		SEC_VDM_HEADER->BITS.data = received_data;

	pr_info("%s pid = %d  data_type=%d ,cmd_type =%d,direction= %d, total_num_of_uvdm_set = %d\n",
		 __func__, SEC_VDM_HEADER->BITS.pid,
		SEC_VDM_HEADER->BITS.data_type,
		SEC_VDM_HEADER->BITS.command_type,
		SEC_VDM_HEADER->BITS.direction,
		SEC_VDM_HEADER->BITS.total_number_of_uvdm_set);
	return;
}

int get_datasize_of_currentset (int first_set, int remained_data_size)
{
	int ret = 0;

	if (first_set)
		ret = (remained_data_size <= SAMSUNGUVDM_MAXDATA_FIRST_UVDMSET) ? \
			remained_data_size : SAMSUNGUVDM_MAXDATA_FIRST_UVDMSET;
	 else
		ret = (remained_data_size <= SAMSUNGUVDM_MAXDATA_NORMAL_UVDMSET) ? \
			remained_data_size : SAMSUNGUVDM_MAXDATA_NORMAL_UVDMSET;

	 return ret;
}

void set_sec_uvdm_txdataheader (void *data, int first_set, int cur_uvdmset,
		int total_data_size, int remained_data_size)
{
	U_SEC_TX_DATA_HEADER *SEC_TX_DATA_HEADER;
	uint8_t *SendMSG = (uint8_t *)data;

	if (first_set)
		SEC_TX_DATA_HEADER = (U_SEC_TX_DATA_HEADER *)&SendMSG[12];
	else
		SEC_TX_DATA_HEADER = (U_SEC_TX_DATA_HEADER *)&SendMSG[8];

	SEC_TX_DATA_HEADER->BITS.data_size_of_current_set =\
		get_datasize_of_currentset(first_set, remained_data_size);
	SEC_TX_DATA_HEADER->BITS.total_data_size = total_data_size;
	SEC_TX_DATA_HEADER->BITS.order_of_current_uvdm_set = cur_uvdmset;

	return;
}

void set_sec_uvdm_txdata_tailer(void *data)
{
	U_SEC_TX_DATA_TAILER *SEC_TX_DATA_TAILER;
	uint8_t *SendMSG = (uint8_t *)data;

	SEC_TX_DATA_TAILER = (U_SEC_TX_DATA_TAILER *)&SendMSG[28];
	SEC_TX_DATA_TAILER->BITS.checksum = get_checksum(SendMSG, S2MM005_SECUVDM_START_ADDR,\
			SAMSUNGUVDM_CHECKSUM_DATA_COUNT);
	return;
}

void set_sec_uvdm_rxdata_header(void *data, int cur_uvdmset_num, int cur_uvdmset_data, int ack)
{
	U_SEC_RX_DATA_HEADER *SEC_UVDM_RX_HEADER;
	uint8_t *SendMSG = (uint8_t *)data;

	SEC_UVDM_RX_HEADER = (U_SEC_RX_DATA_HEADER *)&SendMSG[8];
	SEC_UVDM_RX_HEADER->BITS.order_of_current_uvdm_set = cur_uvdmset_num;
	SEC_UVDM_RX_HEADER->BITS.received_data_size_of_current_set = cur_uvdmset_data;
	SEC_UVDM_RX_HEADER->BITS.result_value = ack;

}

ssize_t send_samsung_unstructured_long_uvdm_message(void *data, void *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_SSM_MSG_SEND;
	uint8_t SendMSG[32] = {0,};
	u8 W_DATA[2];
	uint8_t *SEC_DATA;

	/* Valuable to calc the uvdm set and each uvdm set's data size*/
	int need_uvdmset_count = 0;
	int cur_uvdmset_data = 0;
	int cur_uvdmset_num = 0;
	int accumulated_data_size = 0;
	int remained_data_size = 0;
	uint8_t received_data[MAX_INPUT_DATA] = {0,};
	int time_left;
	int i;
	int received_data_index;
	int write_size = 0;

	/* 1. Calc the receivced data size and determin the uvdm set count and last data of uvdm set. */
	set_endian(buf, received_data, size);

	need_uvdmset_count = set_uvdmset_count(size);
	dev_info(&i2c->dev, "%s need_uvdmset_count = %d \n", __func__, need_uvdmset_count);

	usbpd_data->is_in_first_sec_uvdm_req = true;
	usbpd_data->is_in_sec_uvdm_out = DIR_OUT;
	cur_uvdmset_num = 1;
	accumulated_data_size = 0;
	remained_data_size = size;
	received_data_index = 0;

	/* 2. Common : Fill the MSGHeader */
	set_msghedader(SendMSG, 15, 7);
	/* 3. Common : Fill the UVDMHeader*/
	set_uvdmheader(SendMSG, SAMSUNG_VENDOR_ID, 0);
	/* 4. Common : Fill the First SEC_VDMHeader*/
	if (usbpd_data->is_in_first_sec_uvdm_req)
		set_sec_uvdmheader(SendMSG, usbpd_data->Product_ID, TYPE_LONG,\
				SEC_UVDM_ININIATOR, DIR_OUT, need_uvdmset_count, 0);

	while (cur_uvdmset_num <= need_uvdmset_count) {
		cur_uvdmset_data = 0;
		time_left = 0;

		set_sec_uvdm_txdataheader(SendMSG, usbpd_data->is_in_first_sec_uvdm_req,\
				cur_uvdmset_num, size, remained_data_size);

		cur_uvdmset_data = get_datasize_of_currentset(usbpd_data->is_in_first_sec_uvdm_req, remained_data_size);

		dev_info(&i2c->dev, "%s data_size_of_current_set = %d ,total_data_size = %ld,\
			order_of_current_uvdm_set = %d\n", __func__, cur_uvdmset_data, size, cur_uvdmset_num);
		/* 6. Common : Fill the DATA */
		if (usbpd_data->is_in_first_sec_uvdm_req) {
			SEC_DATA = (uint8_t *)&SendMSG[S2MM005_SECUVDM_START_ADDR+8];
			for (i = 0; i < SAMSUNGUVDM_MAXDATA_FIRST_UVDMSET; i++)
				SEC_DATA[i] = received_data[received_data_index++];
		} else {
			SEC_DATA = (uint8_t *)&SendMSG[S2MM005_SECUVDM_START_ADDR+4];
			for (i = 0; i < SAMSUNGUVDM_MAXDATA_NORMAL_UVDMSET; i++)
				SEC_DATA[i] = received_data[received_data_index++];
		}

		/* 7. Common : Fill the TX_DATA_Tailer */
		set_sec_uvdm_txdata_tailer(SendMSG);

		/* 8. Send data to PDIC */
		REG_ADD = REG_SSM_MSG_SEND;
		write_size = get_writesize(SendMSG);
		s2mm005_write_byte(i2c, REG_ADD, SendMSG, write_size);
		REG_ADD = REG_I2C_SLV_CMD;
		W_DATA[0] = MODE_INTERFACE;
		W_DATA[1] = SEL_SSM_MSG_REQ;
		s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

		/* 9. Wait Response*/
		reinit_completion(&usbpd_data->uvdm_out_wait);
		time_left =
			wait_for_completion_interruptible_timeout(&usbpd_data->uvdm_out_wait,
							  msecs_to_jiffies(SASMSUNGUVDM_WAIT_MS));
		if (time_left <= 0)
			return -ETIME;

		accumulated_data_size += cur_uvdmset_data;
		remained_data_size -= cur_uvdmset_data;
		if (usbpd_data->is_in_first_sec_uvdm_req)
			usbpd_data->is_in_first_sec_uvdm_req = false;
		cur_uvdmset_num++;
	}

	return size;
}

int check_is_wait_ack_accessroy(int vid, int pid, int svid)
{
	int should_wait = true;
	if (vid == SAMSUNG_VENDOR_ID && pid == DEXDOCK_PRODUCT_ID && svid == TypeC_DP_SUPPORT) {
		pr_info("%s : no need to wait ack response!\n", __func__);
		should_wait = false;
	}
	return should_wait;
}

int send_samsung_unstructured_short_vdm_message(void *data, void *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_SSM_MSG_SEND;
	uint8_t SendMSG[32] = {0,};
	u8 W_DATA[2];
	/* Message Type Definition */
	uint8_t received_data = 0;
	int time_left;

	if ((buf == NULL) || size <= 0) {
		dev_info(&i2c->dev, "%s given data is not valid !\n", __func__);
		return -EINVAL;
	}

	if (!usbpd_data->is_samsung_accessory_enter_mode) {
		dev_info(&i2c->dev, "%s - samsung_accessory mode is not ready!\n", __func__);
		return -ENXIO;
	}

	/* 1. Calc the receivced data size and determin the uvdm set count and last data of uvdm set. */
	received_data = *(char *)buf;
	/* 2. Common : Fill the MSGHeader */
	set_msghedader(SendMSG, 15, 2);
	/* 3. Common : Fill the UVDMHeader*/
	set_uvdmheader(SendMSG, SAMSUNG_VENDOR_ID, 0);
	/* 4. Common : Fill the First SEC_VDMHeader*/
	set_sec_uvdmheader(SendMSG, usbpd_data->Product_ID, TYPE_SHORT,\
				SEC_UVDM_ININIATOR, DIR_OUT, 1, received_data);
	usbpd_data->is_in_first_sec_uvdm_req = true;

	dev_info(&i2c->dev, "%s - process short data!\n", __func__);
	s2mm005_write_byte(i2c, REG_ADD, SendMSG, 32);

	/* send uVDM message */
	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = SEL_SSM_MSG_REQ;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

	if (check_is_wait_ack_accessroy(usbpd_data->Vendor_ID, usbpd_data->Product_ID, usbpd_data->SVID_0)) {
		reinit_completion(&usbpd_data->uvdm_out_wait);
		/* Wait Response*/
		time_left =
			wait_for_completion_interruptible_timeout(&usbpd_data->uvdm_out_wait,
							  msecs_to_jiffies(SASMSUNGUVDM_WAIT_MS));
		if (time_left <= 0) {
			usbpd_data->is_in_first_sec_uvdm_req = false;
			return -ETIME;
		}
	}

	dev_info(&i2c->dev, "%s - exit : short data transfer complete!\n", __func__);
	usbpd_data->is_in_first_sec_uvdm_req = false;
	return size;
}

int send_samsung_unstructured_vdm_message(void *data, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_SSM_MSG_SEND;
	uint8_t SendMSG[32] = {0,};
	u8 W_DATA[2];
	/* Message Type Definition */
	MSG_HEADER_Type *MSG_HDR;
	U_UNSTRUCTURED_VDM_HEADER_Type	*UVDM_HEADER;
	U_SEC_UNSTRUCTURED_VDM_HEADER_Type *SEND_SEC_VDM_HEADER;
	U_SEC_UNSTRUCTURED_VDM_HEADER_Type *RECEIVED_VDM_HEADER;
	int received_data;

	if ((buf == NULL) || (size < sizeof(U_SEC_UNSTRUCTURED_VDM_HEADER_Type))) {
		dev_info(&i2c->dev, "%s given data is not valid !\n", __func__);
		return -EINVAL;
	}

	if (!usbpd_data->is_samsung_accessory_enter_mode) {
		dev_info(&i2c->dev, "%s - samsung_accessory mode is not ready!\n", __func__);
		return -ENXIO;
	}
	sscanf(buf, "%d", &received_data);

	RECEIVED_VDM_HEADER = (U_SEC_UNSTRUCTURED_VDM_HEADER_Type *)&received_data;
	MSG_HDR = (MSG_HEADER_Type *)&SendMSG[0];
	UVDM_HEADER = (U_UNSTRUCTURED_VDM_HEADER_Type *)&SendMSG[4];
	SEND_SEC_VDM_HEADER = (U_SEC_UNSTRUCTURED_VDM_HEADER_Type *)&SendMSG[8];

	/* process common data */
	MSG_HDR->Message_Type = 15; /* send VDM message */
	UVDM_HEADER->BITS.USB_Vendor_ID = SAMSUNG_VENDOR_ID; /* VID */
	UVDM_HEADER->BITS.VENDOR_DEFINED_MESSAGE = SEC_UVDM_UNSTRUCTURED_VDM;

	*SEND_SEC_VDM_HEADER = *RECEIVED_VDM_HEADER;
	if (RECEIVED_VDM_HEADER->BITS.DATA_TYPE == SEC_UVDM_SHORT_DATA) {
		dev_info(&i2c->dev, "%s - process short data!\n", __func__);
		/* process short data */
		/* phase 1. fill message header */
		MSG_HDR->Number_of_obj = 2; /* VDM Header + 6 VDOs = MAX 7 */
		/* phase 2. fill uvdm header (already filled) */
		/* phase 3. fill sec uvdm header */
		SEND_SEC_VDM_HEADER->BITS.TOTAL_NUMBER_OF_UVDM_SET = 1;
	} else {
		dev_info(&i2c->dev, "%s - process long data!\n", __func__);
		/*
		* process long data
		* phase 1. fill message header
		* phase 2. fill uvdm header
		* phase 3. fill sec uvdm header
		* phase 4.5.6.7 fill sec data header , data , sec data tail and so on.
		*/
	}

	s2mm005_write_byte(i2c, REG_ADD, SendMSG, 32);

	/* send uVDM message */
	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = SEL_SSM_MSG_REQ;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

	if (RECEIVED_VDM_HEADER->BITS.DATA_TYPE == SEC_UVDM_SHORT_DATA)
		dev_info(&i2c->dev, "%s - exit : short data transfer complete!\n", __func__);

	return 0;
}

void send_unstructured_vdm_message(void *data, int cmd)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_SSM_MSG_SEND;
	uint8_t SendMSG[32] = {0,};
	u8 W_DATA[2];
	uint32_t message = (uint32_t)cmd;
	int i;

	/* Message Type Definition */
	MSG_HEADER_Type *MSG_HDR = (MSG_HEADER_Type *)&SendMSG[0];
	U_UNSTRUCTURED_VDM_HEADER_Type	*DATA_MSG_UVDM = (U_UNSTRUCTURED_VDM_HEADER_Type *)&SendMSG[4];
	VDO_MESSAGE_Type *VDO_MSG = (VDO_MESSAGE_Type *)&SendMSG[8];

	/* fill message */
	MSG_HDR->Message_Type = 15; /* send VDM message */
	MSG_HDR->Number_of_obj = 7; /* VDM Header + 6 VDOs = MAX 7 */
	DATA_MSG_UVDM->BITS.USB_Vendor_ID = SAMSUNG_VENDOR_ID; /* VID */

	for (i = 0; i < 6; i++)
		VDO_MSG->VDO[i] = message; /* VD01~VDO6 : Max 24bytes */

	s2mm005_write_byte(i2c, REG_ADD, SendMSG, 32);

	/* send uVDM message */
	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = SEL_SSM_MSG_REQ;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

	dev_info(&i2c->dev, "%s - message : 0x%x\n", __func__, message);
}

void receive_unstructured_vdm_message(void *data, SSM_MSG_IRQ_STATUS_Type *SSM_MSG_IRQ_State)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_SSM_MSG_READ;
	uint8_t ReadMSG[32] = {0,};
	u8 W_DATA[1];
	U_SEC_UVDM_RESPONSE_HEADER *SEC_UVDM_RESPONSE_HEADER;
	U_SEC_RX_DATA_HEADER *SEC_UVDM_RX_HEADER;

	if (usbpd_data->is_in_sec_uvdm_out == DIR_OUT) {
		s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 16);
		/* first uvdm req for direction out */
		if (usbpd_data->is_in_first_sec_uvdm_req) {
			SEC_UVDM_RESPONSE_HEADER = (U_SEC_UVDM_RESPONSE_HEADER *)&ReadMSG[8];
			if (SEC_UVDM_RESPONSE_HEADER->BITS.data_type == TYPE_LONG) {
				if (SEC_UVDM_RESPONSE_HEADER->BITS.command_type == SEC_UVDM_RESPONDER_ACK) {
					SEC_UVDM_RX_HEADER = (U_SEC_RX_DATA_HEADER *)&ReadMSG[12];
					if (SEC_UVDM_RX_HEADER->BITS.result_value != SEC_UVDM_RESPONDER_ACK) {
						dev_err(&i2c->dev, "%s Busy or Nak received.\n", __func__);
					}
				} else {
					dev_err(&i2c->dev, "%s Response type is wrong.\n", __func__);
				}
			} else {
				if (SEC_UVDM_RESPONSE_HEADER->BITS.command_type == SEC_UVDM_RESPONDER_ACK)
					dev_info(&i2c->dev, "%s Short packet ack is received\n", __func__);
				else {
					dev_err(&i2c->dev, "%s Short packet Response type is wrong.\n", __func__);
				}
			}
		/* uvdm req for direction out */
		} else {
			SEC_UVDM_RX_HEADER = (U_SEC_RX_DATA_HEADER *)&ReadMSG[8];
			if (SEC_UVDM_RX_HEADER->BITS.result_value != SEC_UVDM_RESPONDER_ACK) {
					dev_err(&i2c->dev, "%s Busy or Nak received.\n", __func__);
			}
		}
		REG_ADD = REG_I2C_SLV_CMD;
		W_DATA[0] = MODE_INT_CLEAR;
		s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 1);
		complete(&usbpd_data->uvdm_out_wait);
	/* In uvdm req */
	} else {
		s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);
		/* first uvdm req for direction in */
		if (usbpd_data->is_in_first_sec_uvdm_req) {
			SEC_UVDM_RESPONSE_HEADER = (U_SEC_UVDM_RESPONSE_HEADER *)&ReadMSG[8];
			dev_info(&i2c->dev, "[jj time]%s : data_type = %d , command_type = %d, direction=%d \n", __func__,
				SEC_UVDM_RESPONSE_HEADER->BITS.data_type,
				SEC_UVDM_RESPONSE_HEADER->BITS.command_type,
				SEC_UVDM_RESPONSE_HEADER->BITS.direction);
			/* for long data */
			if (SEC_UVDM_RESPONSE_HEADER->BITS.command_type != SEC_UVDM_RESPONDER_ACK) {
				dev_info(&i2c->dev, "%s :received nak or busy in response  \n", __func__);
				return;
			}
		}
		complete(&usbpd_data->uvdm_longpacket_in_wait);
	}
}

int samsung_uvdm_ready(void)
{
	int uvdm_ready = false;
	struct s2mm005_data *usbpd_data;

	usbpd_data = dev_get_drvdata(ccic_device);
	if (usbpd_data->is_samsung_accessory_enter_mode)
		uvdm_ready =  true;

	pr_info("%s : uvdm ready=%d!\n", __func__, uvdm_ready);
	return uvdm_ready;
}

void samsung_uvdm_close(void)
{
	struct s2mm005_data *usbpd_data;
	pr_info("%s + samsung_uvdm_close success\n", __func__);
	usbpd_data = dev_get_drvdata(ccic_device);
	complete(&usbpd_data->uvdm_out_wait);
	complete(&usbpd_data->uvdm_longpacket_in_wait);
	pr_info("%s - samsung_uvdm_close success\n", __func__);
}

ssize_t samsung_uvdm_out_request_message(void *data, size_t size)
{
	struct s2mm005_data *usbpd_data;
	struct i2c_client *i2c;
	ssize_t ret;
	if (!ccic_device)
		return -ENXIO;

	usbpd_data = dev_get_drvdata(ccic_device);
	if (!usbpd_data)
		return -ENXIO;

	i2c = usbpd_data->i2c;
	if (i2c == NULL) {
		dev_err(&i2c->dev, "%s usbpd_data->i2c is not valid!\n", __func__);
		return -EINVAL;
	}

	if (data == NULL) {
		dev_err(&i2c->dev, "%s given data is not valid !\n", __func__);
		return -EINVAL;
	}

	if (size >= SAMSUNGUVDM_MAX_LONGPACKET_SIZE) {
		dev_err(&i2c->dev, "%s : size %ld is too big to send data\n", __func__, size);
		ret = -EFBIG;
	} else if (size <= SAMSUNGUVDM_MAX_SHORTPACKET_SIZE)
		ret = send_samsung_unstructured_short_vdm_message(usbpd_data, data, size);
	else
		ret = send_samsung_unstructured_long_uvdm_message(usbpd_data, data, size);

	return ret;
}

int samsung_uvdm_in_request_message(void *data)
{
	struct s2mm005_data *usbpd_data;
	struct i2c_client *i2c;
	uint16_t REG_ADD = REG_SSM_MSG_SEND;
	uint8_t SendMSG[32] = {0,};
	uint8_t ReadMSG[32] = {0,};
	u8 W_DATA[2];
	uint8_t IN_DATA[MAX_INPUT_DATA] = {0, };

	/* Send Request */
	/* 1  Message Type Definition */
	U_SEC_UVDM_RESPONSE_HEADER	*SEC_RES_HEADER;
	U_SEC_TX_DATA_HEADER		*SEC_UVDM_TX_HEADER;
	U_SEC_TX_DATA_TAILER		*SEC_UVDM_TX_TAILER;

	int cur_uvdmset_data = 0;
	int cur_uvdmset_num = 0;
	int total_uvdmset_num = 0;
	int received_data_size = 0;
	int total_received_data_size = 0;
	int ack = 0;
	int size = 0;
	int time_left = 0;
	int i;
	int write_size = 0;
	int cal_checksum = 0;

	if (!ccic_device)
		return -ENXIO;

	usbpd_data = dev_get_drvdata(ccic_device);
	if (!usbpd_data)
		return -ENXIO;

	i2c = usbpd_data->i2c;
	if (i2c == NULL) {
		dev_err(&i2c->dev, "%s usbpd_data->i2c is not valid!\n", __func__);
		return -EINVAL;
	}

	dev_info(&i2c->dev, "%s\n", __func__);
	usbpd_data->is_in_sec_uvdm_out = DIR_IN;
	usbpd_data->is_in_first_sec_uvdm_req = true;

	/* 2. Common : Fill the MSGHeader */
	set_msghedader(SendMSG, 15, 2);
	/* 3. Common : Fill the UVDMHeader*/
	set_uvdmheader(SendMSG, SAMSUNG_VENDOR_ID, 0);

	/* 4. Common : Fill the First SEC_VDMHeader*/
	if (usbpd_data->is_in_first_sec_uvdm_req)
		set_sec_uvdmheader(SendMSG, usbpd_data->Product_ID, TYPE_LONG,\
				SEC_UVDM_ININIATOR, DIR_IN, 0, 0);

	/* 5. Send data to PDIC */
	write_size = get_writesize(SendMSG);
	s2mm005_write_byte(usbpd_data->i2c, REG_ADD, SendMSG, write_size);

	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = SEL_SSM_MSG_REQ;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

	cur_uvdmset_num = 0;
	total_uvdmset_num = 1;

	do {
		reinit_completion(&usbpd_data->uvdm_longpacket_in_wait);
		time_left =
			wait_for_completion_interruptible_timeout(&usbpd_data->uvdm_longpacket_in_wait,
					msecs_to_jiffies(SASMSUNGUVDM_WAIT_MS));
		if (time_left <= 0) {
			dev_err(&i2c->dev, "%s timeout\n", __func__);
			return -ETIME;
		}

		/* read data */
		REG_ADD = REG_SSM_MSG_READ;
		s2mm005_read_byte(i2c, REG_ADD, ReadMSG, 32);

		if (usbpd_data->is_in_first_sec_uvdm_req) {
			SEC_RES_HEADER = (U_SEC_UVDM_RESPONSE_HEADER *)&ReadMSG[8];
			SEC_UVDM_TX_HEADER = (U_SEC_TX_DATA_HEADER *)&ReadMSG[12];

			if (SEC_RES_HEADER->BITS.data_type == TYPE_SHORT) {
				IN_DATA[received_data_size++] = SEC_RES_HEADER->BITS.data;
				return received_data_size;
			} else {
				/* 1. check the data size received */
				size = SEC_UVDM_TX_HEADER->BITS.total_data_size;
				cur_uvdmset_data = SEC_UVDM_TX_HEADER->BITS.data_size_of_current_set;
				cur_uvdmset_num = SEC_UVDM_TX_HEADER->BITS.order_of_current_uvdm_set;
				total_uvdmset_num =
					SEC_RES_HEADER->BITS.total_number_of_uvdm_set;

				usbpd_data->is_in_first_sec_uvdm_req = false;
				/* 2. copy data to buffer */
				for (i = 0; i < SAMSUNGUVDM_MAXDATA_FIRST_UVDMSET; i++)
					IN_DATA[received_data_size++] = ReadMSG[16+i];

				total_received_data_size += cur_uvdmset_data;
				usbpd_data->is_in_first_sec_uvdm_req = false;
			}
		} else {
			SEC_UVDM_TX_HEADER = (U_SEC_TX_DATA_HEADER *)&ReadMSG[8];
			cur_uvdmset_data = SEC_UVDM_TX_HEADER->BITS.data_size_of_current_set;
			cur_uvdmset_num = SEC_UVDM_TX_HEADER->BITS.order_of_current_uvdm_set;
			/* 2. copy data to buffer */
			for (i = 0 ; i < SAMSUNGUVDM_MAXDATA_NORMAL_UVDMSET ; i++)
				IN_DATA[received_data_size++] = ReadMSG[12+i];
			total_received_data_size += cur_uvdmset_data;
		}
		/* 3. Check Checksum */
		SEC_UVDM_TX_TAILER = (U_SEC_TX_DATA_TAILER *)&ReadMSG[28];
		cal_checksum = get_checksum(ReadMSG, 8, SAMSUNGUVDM_CHECKSUM_DATA_COUNT);
		ack = (cal_checksum == SEC_UVDM_TX_TAILER->BITS.checksum) ?
			SEC_UVDM_RX_HEADER_ACK : SEC_UVDM_RX_HEADER_NAK;

		///* 4. clear IRQ */
	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INT_CLEAR;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 1);
		/* 5. Send Ack */
		/* 5-1. Common : Fill the MSGHeader */
		set_msghedader(SendMSG, 15, 2);
		/* 5-2. Common : Fill the UVDMHeader*/
		set_uvdmheader(SendMSG, SAMSUNG_VENDOR_ID, 0);
		/* 5-3. Common : Fill the SEC RXHeader */
		set_sec_uvdm_rxdata_header(SendMSG, cur_uvdmset_num, cur_uvdmset_data, ack);
		/* 5-4. Send data to PDIC */
		REG_ADD = REG_SSM_MSG_SEND;
		write_size = get_writesize(SendMSG);
		s2mm005_write_byte(usbpd_data->i2c, REG_ADD, SendMSG, write_size);
		/* send uVDM message */
		REG_ADD = REG_I2C_SLV_CMD;
		W_DATA[0] = MODE_INTERFACE;
		W_DATA[1] = SEL_SSM_MSG_REQ;
		s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

	} while (cur_uvdmset_num < total_uvdmset_num);

	set_endian(IN_DATA, data, size);

	return size;
}

void send_dna_audio_unstructured_vdm_message(void *data, int cmd)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_SSM_MSG_SEND;
	uint8_t SendMSG[32] = {0,};
	u8 W_DATA[2];
	uint32_t message = (uint32_t)cmd;

	/* Message Type Definition */
	MSG_HEADER_Type *MSG_HDR = (MSG_HEADER_Type *)&SendMSG[0];
	U_UNSTRUCTURED_VDM_HEADER_Type	*DATA_MSG_UVDM = (U_UNSTRUCTURED_VDM_HEADER_Type *)&SendMSG[4];
	VDO_MESSAGE_Type *VDO_MSG = (VDO_MESSAGE_Type *)&SendMSG[8];

	/* fill message */
	MSG_HDR->Message_Type = 15; /* send VDM message */
	MSG_HDR->Number_of_obj = 7; /* VDM Header + 6 VDOs = MAX 7 */

	DATA_MSG_UVDM->BITS.USB_Vendor_ID = SAMSUNG_VENDOR_ID; /* VID */

	VDO_MSG->VDO[0] = message;

	s2mm005_write_byte(i2c, REG_ADD, SendMSG, 32);

	/* send uVDM message */
	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = SEL_SSM_MSG_REQ;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

	dev_info(&i2c->dev, "%s - message : 0x%x\n", __func__, message);
}

void send_dex_fan_unstructured_vdm_message(void *data, int cmd)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_SSM_MSG_SEND;
	uint8_t SendMSG[32] = {0,};
	u8 W_DATA[2];
	uint32_t message = (uint32_t)cmd;

	/* Message Type Definitio */
	MSG_HEADER_Type *MSG_HDR = (MSG_HEADER_Type *)&SendMSG[0];
	U_UNSTRUCTURED_VDM_HEADER_Type	*DATA_MSG_UVDM = (U_UNSTRUCTURED_VDM_HEADER_Type *)&SendMSG[4];
	VDO_MESSAGE_Type *VDO_MSG = (VDO_MESSAGE_Type *)&SendMSG[8];

	/* fill message */
	MSG_HDR->Message_Type = 15; /* send VDM message */
	MSG_HDR->Number_of_obj = 2; /* VDM Header + 6 VDOs = MAX 7 */

	DATA_MSG_UVDM->BITS.USB_Vendor_ID = SAMSUNG_VENDOR_ID; /* VID */
	DATA_MSG_UVDM->BITS.VENDOR_DEFINED_MESSAGE = 1;

	VDO_MSG->VDO[0] = message;

	s2mm005_write_byte(i2c, REG_ADD, SendMSG, 32);

	/* send uVDM message */
	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = SEL_SSM_MSG_REQ;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

	dev_info(&i2c->dev, "%s - message : 0x%x\n", __func__, message);
}

/*
* send_role_swap_message
* cmd 0 : PR_SWAP, cmd 1 : DR_SWAP
*/
void send_role_swap_message(void *data, int cmd)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_I2C_SLV_CMD;
	u8 mode = (u8)cmd;
	u8 W_DATA[2];

	/* send uVDM message */
	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = mode ? REQ_DR_SWAP : REQ_PR_SWAP;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 2);

	dev_info(&i2c->dev, "%s : sent %s message\n", __func__, mode ? "DR_SWAP" : "PR_SWAP");
}

void send_attention_message(void *data, int cmd)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = REG_TX_DIS_ATTENTION_RESPONSE;
	uint8_t SendMSG[32] = {0,};
	u8 W_DATA[3];
	uint32_t message = (uint32_t)cmd;
	int i;

	/* Message Type Definition */
	MSG_HEADER_Type *MSG_HDR = (MSG_HEADER_Type *)&SendMSG[0];
	U_DATA_MSG_VDM_HEADER_Type	  *DATA_MSG_VDM = (U_DATA_MSG_VDM_HEADER_Type *)&SendMSG[4];
	VDO_MESSAGE_Type *VDO_MSG = (VDO_MESSAGE_Type *)&SendMSG[8];

	/* fill message */
	DATA_MSG_VDM->BITS.VDM_command = 6;	/* attention*/
	DATA_MSG_VDM->BITS.VDM_Type = 1;	/* structured VDM */
	DATA_MSG_VDM->BITS.Standard_Vendor_ID = SAMSUNG_VENDOR_ID;

	MSG_HDR->Message_Type = 15;		/* send VDM message */
	MSG_HDR->Number_of_obj = 7;		/* VDM Header + 6 VDOs = MAX 7 */

	for (i = 0; i < 6; i++)
		VDO_MSG->VDO[i] = message;	/* VD01~VDO6 : Max 24bytes */

	s2mm005_write_byte(i2c, REG_ADD, SendMSG, 32);

	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = PD_NEXT_STATE;
	W_DATA[2] = 100;
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 3);

	dev_info(&i2c->dev, "%s - message : 0x%x\n", __func__, message);
}

void do_alternate_mode_step_by_step(void *data, int cmd)
{
	struct s2mm005_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	uint16_t REG_ADD = 0;
	u8 W_DATA[3];

	REG_ADD = REG_I2C_SLV_CMD;
	W_DATA[0] = MODE_INTERFACE;
	W_DATA[1] = PD_NEXT_STATE;
	switch (cmd) {
	case VDM_DISCOVER_ID:
		W_DATA[2] = 80;
		break;
	case VDM_DISCOVER_SVIDS:
		W_DATA[2] = 83;
		break;
	case VDM_DISCOVER_MODES:
		W_DATA[2] = 86;
		break;
	case VDM_ENTER_MODE:
		W_DATA[2] = 89;
		break;
	case VDM_EXIT_MODE:
		W_DATA[2] = 92;
		break;
	}
	s2mm005_write_byte(i2c, REG_ADD, &W_DATA[0], 3);

	dev_info(&i2c->dev, "%s\n", __func__);
}
#endif
