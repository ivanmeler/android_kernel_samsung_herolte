/*
 * muic_state.c
 *
 * Copyright (C) 2014 Samsung Electronics
 * Thomas Ryu <smilesr.ryu@samsung.com>
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>

#include <linux/muic/muic.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#if defined(CONFIG_MUIC_HV)
#include "muic_hv.h"
#include "muic_hv_max77854.h"
#endif

#include "muic-internal.h"
#include "muic_apis.h"
#include "muic_i2c.h"
#include "muic_vps.h"

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
#include "muic_ccic.h"
#include <linux/ccic/s2mm005.h>
#endif

#include "../../battery_v2/include/sec_charging_common.h"

extern void muic_send_dock_intent(int type);

static void muic_handle_attach(muic_data_t *pmuic,
			muic_attached_dev_t new_dev, int adc, u8 vbvolt)
{
	int ret = 0;
	bool noti_f = true;

	pr_info("%s:%s attached_dev:%d new_dev:%d adc:0x%02x, vbvolt:%02x\n",
		MUIC_DEV_NAME, __func__, pmuic->attached_dev, new_dev, adc, vbvolt);

#if defined(CONFIG_MUIC_HV)
        if (pmuic->attached_dev == ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC ||
                pmuic->attached_dev == ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC ||
                pmuic->attached_dev == ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC)
                return ;
#endif

	if((new_dev == pmuic->attached_dev) &&
		(new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC) &&
		(new_dev != ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC)) {
		pr_info("%s:%s Duplicated device %d just ignore\n",
				MUIC_DEV_NAME, __func__,pmuic->attached_dev);
		return;
	}
	switch (pmuic->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		if (new_dev != pmuic->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			ret = detach_usb(pmuic);
		}
		break;
	case ATTACHED_DEV_OTG_MUIC:
	/* OTG -> LANHUB, meaning TA is attached to LANHUB(OTG)
	   OTG -> GAMEPAD, meaning Earphone is detached to GAMEPAD(OTG) */
		if (new_dev == ATTACHED_DEV_USB_LANHUB_MUIC) {
			pr_info("%s:%s OTG+TA=>LANHUB. Do not detach OTG.\n",
					__func__, MUIC_DEV_NAME);
			noti_f = false;
			break;
		}
		else if (new_dev == ATTACHED_DEV_GAMEPAD_MUIC) {
			pr_info("%s:%s OTG+No Earphone=>GAMEPAD. Do not detach OTG.\n",
					__func__, MUIC_DEV_NAME);
			noti_f = false;
			break;
		}

		if (new_dev == pmuic->attached_dev) {
			noti_f = false;
			break;

		/* Treat unexpected situations on disconnection */
		} else if (pmuic->is_gamepad && (adc == ADC_OPEN)) {
			pmuic->is_gamepad = false;
			muic_send_dock_intent(MUIC_DOCK_DETACHED);
		}
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		if (new_dev != pmuic->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			ret = detach_otg_usb(pmuic);
		}
		break;

	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (new_dev != pmuic->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			ret = detach_audiodock(pmuic);
		}
		break;

	case ATTACHED_DEV_HMT_MUIC:
		if (new_dev != pmuic->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			ret = detach_HMT(pmuic);
		}
		break;

	case ATTACHED_DEV_RDU_TA_MUIC:
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;

	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_MUIC_HV)
        case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
        case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
        case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
        case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
                noti_f = hv_do_predetach(pmuic->phv, new_dev);
#endif /* CONFIG_MUIC_HV */
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			ret = detach_jig_uart_boot_off(pmuic);
		}
		break;
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		if (new_dev != pmuic->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
				MUIC_DEV_NAME, __func__, new_dev,
				pmuic->attached_dev);

			if (pmuic->is_factory_start)
				ret = detach_deskdock(pmuic);
			else
				ret = detach_jig_uart_boot_on(pmuic);
		}
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		if (new_dev == ATTACHED_DEV_DESKDOCK_MUIC || 
				new_dev == ATTACHED_DEV_DESKDOCK_VB_MUIC) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume same device\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			noti_f = false;
		} else if (new_dev != pmuic->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			ret = detach_deskdock(pmuic);
		}
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		if (new_dev != pmuic->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			pr_warn("%s:%s mmdock pre-detach skipped.\n", MUIC_DEV_NAME, __func__);
		}
		break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		if (new_dev != pmuic->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					pmuic->attached_dev);
			ret = detach_ps_cable(pmuic);
		}
		break;
	case ATTACHED_DEV_GAMEPAD_MUIC:
		if (new_dev == ATTACHED_DEV_OTG_MUIC) {
			pr_info("%s:%s GAMEPAD+Earphone=>OTG. Do not detach gamepad.\n",
					__func__, MUIC_DEV_NAME);
			noti_f = false;

			/* We have to inform the framework of this change
			  * if USB does not send a noti of gamepad to muic.
			  */
			muic_send_dock_intent(MUIC_DOCK_GAMEPAD_WITH_EARJACK);
		}
		break;		
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNDEFINED_RANGE_MUIC:
		break;
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	case ATTACHED_DEV_POGO_DOCK_MUIC:
	case ATTACHED_DEV_POGO_DOCK_5V_MUIC:
	case ATTACHED_DEV_POGO_DOCK_9V_MUIC:
		hv_do_detach(pmuic->phv);
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		detach_ta(pmuic);
		break;
#endif

	default:
		noti_f = false;
	}

	if (noti_f)
		muic_notifier_detach_attached_dev(pmuic->attached_dev);

	noti_f = true;
	switch (new_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		ret = attach_usb(pmuic, new_dev);
		break;
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		ret = attach_otg_usb(pmuic, new_dev);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = attach_audiodock(pmuic, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_RDU_TA_MUIC:
		attach_ta(pmuic);
		pmuic->attached_dev = new_dev;
		break;
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_MUIC_HV)
                if (pmuic->pdata->afc_disable)
                        pr_info("%s:%s AFC Disable(%d) by USER!\n", MUIC_DEV_NAME,
                                __func__, pmuic->pdata->afc_disable);
#if defined(CONFIG_MUIC_SUPPORT_CCIC)
		else if (pmuic->afc_water_disable)
                        pr_info("%s:%s AFC Disable(%d) by WATER!\n", MUIC_DEV_NAME,
                                __func__, pmuic->afc_water_disable);
		else if (!pmuic->is_ccic_attach)
			pr_info("%s:%s AFC Disable(%d), not CC attach!\n", MUIC_DEV_NAME,
				__func__, pmuic->is_ccic_attach);
		else if (pmuic->afc_tsub_disable)
			pr_info("%s:%s AFC Disable(%d) by TSUB too hot!\n", MUIC_DEV_NAME,
				__func__, pmuic->afc_tsub_disable);
		else if (pmuic->is_ccic_afc_enable == Rp_Abnormal)
			pr_info("%s:%s AFC Disable(%d) by CC or SBU short!\n", MUIC_DEV_NAME,
				__func__, pmuic->is_ccic_afc_enable);
#endif
                else {
                        if ((pmuic->phv->is_afc_muic_ready == false) &&
				vps_is_hv_ta(&pmuic->vps)) {
                                max77854_muic_prepare_afc_charger(pmuic->phv);
                       	}
                }
#endif /* CONFIG_MUIC_HV */
		attach_ta(pmuic);
		mdelay(150);
		pmuic->attached_dev = new_dev;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		new_dev = attach_jig_uart_boot_off(pmuic, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		/* Keep AP UART path and
		 *  call attach_deskdock to wake up the device in the Facory Build Binary.
		 */
		if (pmuic->is_factory_start)
			ret = attach_deskdock(pmuic, new_dev);
		else
			ret = attach_jig_uart_boot_on(pmuic, new_dev);
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		ret = attach_jig_usb_boot_off(pmuic, vbvolt);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = attach_jig_usb_boot_on(pmuic, vbvolt);
		break;
	case ATTACHED_DEV_MHL_MUIC:
		ret = attach_mhl(pmuic);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		ret = attach_deskdock(pmuic, new_dev);
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		noti_f = vbvolt ? true: false;
		ret = attach_mmdock(pmuic, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_HMT_MUIC:
		ret = attach_HMT(pmuic, new_dev);
		break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		ret = attach_ps_cable(pmuic, new_dev);
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNDEFINED_RANGE_MUIC:
		com_to_open_with_vbus(pmuic);
		pmuic->attached_dev = new_dev;
		break;
	case ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC:
		com_to_open_with_vbus(pmuic);
		pmuic->attached_dev = new_dev;
		break;
	case ATTACHED_DEV_GAMEPAD_MUIC:
		ret = attach_gamepad(pmuic, new_dev);
		break;
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	case ATTACHED_DEV_POGO_DOCK_MUIC:
		max77854_muic_prepare_afc_pogo_dock(pmuic->phv);
		attach_ta(pmuic);
		mdelay(150);
		pmuic->attached_dev = new_dev;
		break;
#endif
	default:
		pr_warn("%s:%s unsupported dev=%d, adc=0x%x, vbus=%c\n",
				MUIC_DEV_NAME, __func__, new_dev, adc,
				(vbvolt ? 'O' : 'X'));
		break;
	}

#if defined(CONFIG_MUIC_HV)
	if (!pmuic->pdata->afc_disable)
		new_dev = hv_muic_check_id_err(pmuic->phv, new_dev);
#endif

	if (noti_f)
		muic_notifier_attach_attached_dev(new_dev);
	else
		pr_info("%s:%s attach Noti. for (%d) discarded.\n",
				MUIC_DEV_NAME, __func__, new_dev);

	if (ret < 0)
		pr_warn("%s:%s something wrong with attaching %d (ERR=%d)\n",
				MUIC_DEV_NAME, __func__, new_dev, ret);
}

static void muic_handle_detach(muic_data_t *pmuic)
{
	int ret = 0;
	bool noti_f = true;

	ret = com_to_open_with_vbus(pmuic);

	enable_chgdet(pmuic, 1);
	if (get_adc_scan_mode(pmuic) != ADC_SCANMODE_ONESHOT)
		set_adc_scan_mode(pmuic, ADC_SCANMODE_ONESHOT);

#if defined(CONFIG_MUIC_HV)
	hv_do_detach(pmuic->phv);
#endif

	switch (pmuic->attached_dev) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		ret = detach_usb(pmuic);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		if (pmuic->is_gamepad) {
			pmuic->is_gamepad = false;
			muic_send_dock_intent(MUIC_DOCK_DETACHED);
		}
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		ret = detach_otg_usb(pmuic);
		break;

	case ATTACHED_DEV_RDU_TA_MUIC:
	case ATTACHED_DEV_TA_MUIC:
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		detach_ta(pmuic);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = detach_jig_uart_boot_off(pmuic);
		break;
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		if (pmuic->is_factory_start)
			ret = detach_deskdock(pmuic);
		else
			ret = detach_jig_uart_boot_on(pmuic);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		ret = detach_deskdock(pmuic);
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		ret = detach_mmdock(pmuic);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = detach_audiodock(pmuic);
		break;
	case ATTACHED_DEV_MHL_MUIC:
		ret = detach_mhl(pmuic);
		break;
	case ATTACHED_DEV_HMT_MUIC:
		ret = detach_HMT(pmuic);
		break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		ret = detach_ps_cable(pmuic);
		break;
	case ATTACHED_DEV_NONE_MUIC:
		pr_info("%s:%s duplicated(NONE)\n", MUIC_DEV_NAME, __func__);
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	case ATTACHED_DEV_UNDEFINED_RANGE_MUIC:		
		pr_info("%s:%s UNDEFINED_RANGE\n", MUIC_DEV_NAME, __func__);
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	case ATTACHED_DEV_GAMEPAD_MUIC:
		ret = detach_gamepad(pmuic);
		break;
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	case ATTACHED_DEV_POGO_DOCK_MUIC:
	case ATTACHED_DEV_POGO_DOCK_5V_MUIC:
	case ATTACHED_DEV_POGO_DOCK_9V_MUIC:
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		detach_ta(pmuic);
		break;
#endif
	default:
		pr_info("%s:%s invalid attached_dev type(%d)\n", MUIC_DEV_NAME,
			__func__, pmuic->attached_dev);
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	}

	if (noti_f)
		muic_notifier_detach_attached_dev(pmuic->attached_dev);

	else
		pr_info("%s:%s detach Noti. for (%d) discarded.\n",
				MUIC_DEV_NAME, __func__, pmuic->attached_dev);
	if (ret < 0)
		pr_warn("%s:%s something wrong with detaching %d (ERR=%d)\n",
				MUIC_DEV_NAME, __func__, pmuic->attached_dev, ret);

}

void muic_detect_dev(muic_data_t *pmuic, int irq)
{
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	u8 adc = 0, vbvolt = 0;
#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_MUIC_SUPPORT_CCIC)
	union power_supply_propval tsub_val;
#endif

	get_vps_data(pmuic, &pmuic->vps);

	if (pmuic->vps_table == VPS_TYPE_SCATTERED) {
		pr_info("%s:%s dev[1:0x%x, 2:0x%x, 3:0x%x], adc:0x%x, vbvolt:0x%x\n",
			MUIC_DEV_NAME, __func__, pmuic->vps.s.val1, pmuic->vps.s.val2,
			pmuic->vps.s.val3, pmuic->vps.s.adc, pmuic->vps.s.vbvolt);
		adc = pmuic->vps.s.adc;
		vbvolt = pmuic->vps.s.vbvolt;
	} else if (pmuic->vps_table == VPS_TYPE_TABLE) {
		pr_info("%s:%s ST[0x%02x 0x%02x 0x%02x] CT[0x%02x 0x%02x 0x%02x 0x%02x] HVCT[0x%02x 0x%02x]\n",
			MUIC_DEV_NAME, __func__,
			pmuic->vps.t.status[0], pmuic->vps.t.status[1], pmuic->vps.t.status[2],
			pmuic->vps.t.control[0], pmuic->vps.t.control[1], pmuic->vps.t.control[2], pmuic->vps.t.control[3],
			pmuic->vps.t.hvcontrol[0], pmuic->vps.t.hvcontrol[1]);
		
		pr_info("%s:%s adc:%02x vbvolt:%02x chgtyp:%02x adcerr:%02x adclow:%02x chgdetrun:%02x\n",
			MUIC_DEV_NAME, __func__,
			pmuic->vps.t.adc, pmuic->vps.t.vbvolt, pmuic->vps.t.chgtyp, pmuic->vps.t.adcerr,
			pmuic->vps.t.adclow, pmuic->vps.t.chgdetrun);

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_MUIC_SUPPORT_CCIC)
		if (pmuic->vps.t.adc == ADC_CEA936ATYPE2_CHG) {
			pr_info("%s:%s W/A for pogo adc 0x%x\n",
					MUIC_DEV_NAME, __func__, pmuic->vps.t.adc);
			pmuic->vps.t.adc = ADC_JIG_UART_OFF;
		}
#endif

#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
		if (gpio_is_valid(pmuic->dock_int_ap))
			pr_info("%s:%s dock_int_ap(%c)\n", MUIC_DEV_NAME, __func__,
				gpio_get_value(pmuic->dock_int_ap) ? 'x' : 'o');
#endif

		adc = pmuic->vps.t.adc;
		vbvolt = pmuic->vps.t.vbvolt;
	}

	if (vps_resolve_dev(pmuic, &new_dev, &intr) < 0) {
		pr_info("%s:%s: discarded.\n",MUIC_DEV_NAME,__func__);
		return;
	}

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	if (new_dev == ATTACHED_DEV_USB_MUIC && pmuic->is_dcdtmr_intr) {
		new_dev = ATTACHED_DEV_TIMEOUT_OPEN_MUIC;
	}
	if (pmuic->opmode & OPMODE_CCIC) {
		if (irq > 0) {
			if (!mdev_continue_for_TA_USB(pmuic, new_dev))
				return;
		} else
			muic_set_legacy_dev(pmuic, new_dev);
	}

#if !defined(CONFIG_SEC_FACTORY)
	if (irq < 0) {
		pmuic->afc_tsub_disable = false;
	} else {
		psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_SUB_PBA_TEMP_REC, tsub_val);
		if (!tsub_val.intval)
			pmuic->afc_tsub_disable = true;
		else
			pmuic->afc_tsub_disable = false;
	}
#endif /* CONFIG_SEC_FACTORY */
#endif /* CONFIG_MUIC_SUPPORT_CCIC */

	if (intr == MUIC_INTR_ATTACH)
		muic_handle_attach(pmuic, new_dev, adc, vbvolt);
	else
		muic_handle_detach(pmuic);

#if defined(CONFIG_MUIC_HV)
	/* AFC should refer to the change of attached_dev */
	hv_update_status(pmuic->phv, pmuic->attached_dev);
#endif
}
