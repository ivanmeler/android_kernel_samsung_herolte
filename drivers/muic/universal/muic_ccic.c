/*
 * muic_ccic.c
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
#include <linux/string.h>
#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#include <linux/muic/muic.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif
#include "muic-internal.h"
#include "muic_apis.h"
#include "muic_debug.h"
#include "muic_regmap.h"
#include "muic_state.h"
#include "muic_vps.h"

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
#include <linux/ccic/ccic_notifier.h>
#include <linux/ccic/s2mm005.h>
#endif
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/manager/usb_typec_manager_notifier.h>
#endif

#if defined(CONFIG_MUIC_HV)
#include "muic_hv.h"
#include "muic_hv_max77854.h"
#endif

#define MUIC_CCIC_NOTI_ATTACH (1)
#define MUIC_CCIC_NOTI_DETACH (-1)
#define MUIC_CCIC_NOTI_UNDEFINED (0)

struct mdev_rid_desc_t {
	int mdev;
};

static struct mdev_desc_t {
	int ccic_evt_attached; /* 1: attached, -1: detached, 0: undefined */
	int ccic_evt_rid; /* the last rid */
	int ccic_evt_rprd; /*rprd */
	int ccic_evt_roleswap; /* check rprd role swap event */
	int ccic_evt_dcdcnt; /* count dcd timeout */

	int mdev; /* attached dev */
}mdev_desc;

static int __ccic_info = 0;

static struct mdev_rid_desc_t mdev_rid_tbl[] = {
	[RID_UNDEFINED] = {ATTACHED_DEV_NONE_MUIC},
	[RID_000K] = {ATTACHED_DEV_OTG_MUIC},
	[RID_001K] = {ATTACHED_DEV_MHL_MUIC},
	[RID_255K] = {ATTACHED_DEV_JIG_USB_OFF_MUIC},
	[RID_301K] = {ATTACHED_DEV_JIG_USB_ON_MUIC},
	[RID_523K] = {ATTACHED_DEV_JIG_UART_OFF_MUIC},
	[RID_619K] = {ATTACHED_DEV_JIG_UART_ON_MUIC},
	[RID_OPEN] = {ATTACHED_DEV_NONE_MUIC},
};

/*
 * __ccic_info :
 * b'0: 1 if an active ccic is present,
 *        0 when muic works without ccic chip or
 *              no ccic Noti. registration is needed
 *              even though a ccic chip is present.
 */
static int set_ccic_info(char *str)
{
	get_option(&str, &__ccic_info);

	pr_info("%s: ccic_info: 0x%04x\n", __func__, __ccic_info);

	return __ccic_info;
}
__setup("ccic_info=", set_ccic_info);

int get_ccic_info(void)
{
	return __ccic_info;
}

int muic_is_ccic_supported_jig(muic_data_t *pmuic, muic_attached_dev_t mdev)
{
	switch (mdev) {
	/* JIG */
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		pr_info("%s: Supported JIG(%d).\n", __func__, mdev);
		return 1;
	default:
		pr_info("%s: mdev:%d Unsupported.\n", __func__, mdev);
	}

	return 0;
}

int muic_is_ccic_supported_dev(muic_data_t *pmuic, muic_attached_dev_t new_dev)
{
	switch (new_dev) {
	/* Legacy TA/USB. Noti. will be sent when ATTACH is received from CCIC. */
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	case ATTACHED_DEV_POGO_DOCK_MUIC:
#endif
		return 1;
	default:
		break;
	}

	return 0;
}

static bool mdev_is_supported(int mdev)
{
	switch (mdev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		return true;
	default:
		break;
	}

	return false;
}

static int mdev_com_to(muic_data_t *pmuic, int path)
{
#if defined(CONFIG_HICCUP_CHARGER)
	if (pmuic->afc_water_disable)
		return 0;
#endif
#if defined(CONFIG_MUIC_HV)
	hv_clear_hvcontrol(pmuic->phv);
#endif
	switch (path) {
	case MUIC_PATH_OPEN:
		com_to_open_with_vbus(pmuic);
		break;

	case MUIC_PATH_USB_AP:
	case MUIC_PATH_USB_CP:
		switch_to_ap_usb(pmuic);
		break;
	case MUIC_PATH_UART_AP:
	case MUIC_PATH_UART_CP:
		if (pmuic->pdata->uart_path == MUIC_PATH_UART_AP)
			switch_to_ap_uart(pmuic);
		else
			switch_to_cp_uart(pmuic);
		break;

	default:
		pr_err("%s:A wrong com path!\n", __func__);
		return -1;
	}

	return 0;
}

static int mdev_get_vbus(muic_data_t *pmuic)
{
	return pmuic->vps.t.vbvolt;
}

int mdev_noti_attached(int mdev)
{
	muic_notifier_attach_attached_dev(mdev);
	return 0;
}

int mdev_noti_detached(int mdev)
{
	muic_notifier_detach_attached_dev(mdev);
	return 0;
}

static void mdev_handle_ccic_detach(muic_data_t *pmuic)
{
	struct mdev_desc_t *pdesc = &mdev_desc;
	struct vendor_ops *pvendor = pmuic->regmapdesc->vendorops;

#if defined(CONFIG_MUIC_HV)
	hv_do_detach(pmuic->phv);
#endif
	pmuic->is_ccic_attach = false;
	pmuic->is_ccic_afc_enable = 0;
	pmuic->is_ccic_rp56_enable = false;

	if (pdesc->ccic_evt_rprd) {
		if (pvendor && pvendor->enable_chgdet)
			pvendor->enable_chgdet(pmuic->regmapdesc, 1);
	}
	mdev_com_to(pmuic, MUIC_PATH_OPEN);
	if (mdev_is_supported(pdesc->mdev))
		mdev_noti_detached(pdesc->mdev);
	else if (pmuic->legacy_dev != ATTACHED_DEV_NONE_MUIC)
		mdev_noti_detached(pmuic->legacy_dev);

	/* Reset status & flags */
	pdesc->mdev = 0;
	pdesc->ccic_evt_rid = 0;
	pdesc->ccic_evt_rprd = 0;
	pdesc->ccic_evt_roleswap = 0;
	pdesc->ccic_evt_dcdcnt = 0;
	pdesc->ccic_evt_attached = MUIC_CCIC_NOTI_UNDEFINED;

	pmuic->legacy_dev = 0;
	pmuic->attached_dev = 0;
#if defined(CONFIG_MUIC_HV)
	pmuic->phv->attached_dev = 0;
#endif
	pmuic->is_dcdtmr_intr = false;

	return;
}

static int mdev_handle_factory_jig(muic_data_t *pmuic, int rid, int vbus);

int mdev_continue_for_TA_USB(muic_data_t *pmuic, int mdev)
{
	struct mdev_desc_t *pdesc = &mdev_desc;
	struct vendor_ops *pvendor = pmuic->regmapdesc->vendorops;
	int i;
	int vbus = mdev_get_vbus(pmuic);

	/* W/A for Incomplete insertion case */
	if (pdesc->ccic_evt_attached == MUIC_CCIC_NOTI_ATTACH &&
			pmuic->is_dcdtmr_intr && vbus && pdesc->ccic_evt_dcdcnt < 1) {
		pr_info("%s: Incomplete insertion. Do chgdet again\n", __func__);
		pmuic->is_dcdtmr_intr = false;
		pdesc->ccic_evt_dcdcnt++;
		if (pvendor && pvendor->run_chgdet)
			pvendor->run_chgdet(pmuic->regmapdesc, 1);
		return 0;
	}
	if (vbus == 0) {
		pdesc->ccic_evt_dcdcnt = 0;
		pmuic->is_dcdtmr_intr = false;
	}

	if (!muic_is_ccic_supported_dev(pmuic, mdev)) {
		pr_info("%s:%s: NOT supported(%d).\n", __func__, MUIC_DEV_NAME, mdev);
		
		if (pdesc->ccic_evt_attached == MUIC_CCIC_NOTI_DETACH) {
			pr_info("%s:%s: detach event is occurred\n", __func__, MUIC_DEV_NAME);
			mdev_handle_ccic_detach(pmuic);
			return 0;
		}
		if (pdesc->ccic_evt_rprd && vbus) {
			pr_info("%s:%s:RPRD detected. set path to USB\n",
					__func__, MUIC_DEV_NAME);
			mdev_com_to(pmuic, MUIC_PATH_USB_AP);
		}
#if defined(CONFIG_HICCUP_CHARGER)
		if (pmuic->afc_water_disable) {
			pr_info("%s:%s:HICCUP MODE. set path to Aux USB\n",
					__func__, MUIC_DEV_NAME);
			com_to_usb_cp(pmuic);

			if (vbus) {
				if (pmuic->legacy_dev != ATTACHED_DEV_TA_MUIC)
					mdev_noti_detached(pmuic->legacy_dev);

				pmuic->legacy_dev = ATTACHED_DEV_TA_MUIC;
				mdev_noti_attached(pmuic->legacy_dev);
			}

			return 0;
		}
#endif
		if (pdesc->ccic_evt_rid == 0) {
			pr_info("%s:%s: No rid\n", __func__, MUIC_DEV_NAME);
			return 0;
		}
	}

	/* Some delays for CCIC's Noti. When VBUS comes in to MUIC */
	for (i = 0; i < 4; i++) {
		pr_info("%s:%s: Checking RID (%dth)....\n",
				MUIC_DEV_NAME,__func__, i + 1);
		
		/* Do not continue if this is an RID */
		if (pdesc->ccic_evt_rid || pdesc->ccic_evt_rprd) {
			pr_info("%s:%s: Not a TA or USB -> discarded.\n",
					MUIC_DEV_NAME,__func__);
			if (pdesc->ccic_evt_rid) {
				vbus = mdev_get_vbus(pmuic);
				mdev_handle_factory_jig(pmuic, pdesc->ccic_evt_rid, vbus);
			}
			pmuic->legacy_dev = 0;

			return 0;
		}

		msleep(50);
	}

	pmuic->legacy_dev = mdev;
	pr_info("%s:%s: A legacy TA or USB updated(%d).\n",
				MUIC_DEV_NAME,__func__, mdev);

	return 1;
}

void muic_set_legacy_dev(muic_data_t *pmuic, int new_dev)
{
	pr_info("%s:%s: %d->%d\n", MUIC_DEV_NAME, __func__, pmuic->legacy_dev, new_dev);

	pmuic->legacy_dev = new_dev;
}

static void mdev_show_status(muic_data_t *pmuic)
{
	struct mdev_desc_t *pdesc = &mdev_desc;

	pr_info("%s: mdev:%d rid:%d rprd:%d attached:%d legacy_dev:%d\n", __func__,
			pdesc->mdev, pdesc->ccic_evt_rid, pdesc->ccic_evt_rprd,
			pdesc->ccic_evt_attached, pmuic->legacy_dev);
}

/* Get the charger type from muic interrupt or by reading the register directly */
static int muic_get_chgtyp_to_mdev(muic_data_t *pmuic)
{
	return pmuic->legacy_dev;
}

int muic_get_current_legacy_dev(muic_data_t *pmuic)
{
	struct mdev_desc_t *pdesc = &mdev_desc;

	pr_info("%s: mdev:%d legacy_dev:%d\n", __func__, pdesc->mdev, pmuic->legacy_dev);

	if (pdesc->mdev)
		return pdesc->mdev;
	else if (pmuic->legacy_dev)
		return pmuic->legacy_dev;

	return 0;
}

static int mdev_handle_legacy_TA_USB(muic_data_t *pmuic)
{
	struct mdev_desc_t *pdesc = &mdev_desc;
	int mdev = 0;

	pr_info("%s: vbvolt:%d legacy_dev:%d\n", __func__,
			pmuic->vps.t.vbvolt, pmuic->legacy_dev);

	/* 1. Run a charger detection algorithm manually if necessary. */
	msleep(200);

	/* 2. Get the result by polling or via an interrupt */
	mdev = muic_get_chgtyp_to_mdev(pmuic);
	pr_info("%s: detected legacy_dev=%d\n", __func__, mdev);

	/* 3. Noti. if supported. */
	if (!muic_is_ccic_supported_dev(pmuic, mdev)) {
		pr_info("%s: Unsupported legacy_dev=%d\n", __func__, mdev);
		return 0;
	}

	if (mdev_is_supported(pdesc->mdev)) {
		mdev_noti_detached(pdesc->mdev);
		pdesc->mdev = 0;
	}
	else if (pmuic->legacy_dev != ATTACHED_DEV_NONE_MUIC) {
		mdev_noti_detached(pmuic->legacy_dev);
		pmuic->legacy_dev = 0;
	}

	pdesc->mdev = mdev;
	mdev_noti_attached(mdev);

	return 0;
}


void init_mdev_desc(muic_data_t *pmuic)
{
	struct mdev_desc_t *pdesc = &mdev_desc;

	pr_info("%s\n", __func__);
	pdesc->mdev = 0;
	pdesc->ccic_evt_rid = 0;
	pdesc->ccic_evt_rprd = 0;
	pdesc->ccic_evt_roleswap = 0;
	pdesc->ccic_evt_dcdcnt = 0;
	pdesc->ccic_evt_attached = MUIC_CCIC_NOTI_UNDEFINED;
}


static int rid_to_mdev_with_vbus(muic_data_t *pmuic, int rid, int vbus)
{
	int mdev = 0;

	if (rid < 0 || rid >  RID_OPEN) {
		pr_err("%s:Out of RID range: %d\n", __func__, rid);
		return 0;
	}

	if ((rid == RID_619K) && vbus)
		mdev = ATTACHED_DEV_JIG_UART_ON_VB_MUIC;
	else
		mdev = mdev_rid_tbl[rid].mdev;

	return mdev;
}

static bool mdev_is_valid_RID_OPEN(muic_data_t *pmuic, int vbus)
{
	int i, retry = 5;

	if (vbus)
		return true;

	for (i = 0; i < retry; i++) {
		pr_info("%s: %dth ...\n", __func__, i);
		msleep(10);
		if (mdev_get_vbus(pmuic))
			return 1;
	}

	return 0;
}

static int muic_handle_ccic_ATTACH(muic_data_t *pmuic, CC_NOTI_ATTACH_TYPEDEF *pnoti)
{
	struct mdev_desc_t *pdesc = &mdev_desc;
	int vbus = mdev_get_vbus(pmuic);
	struct vendor_ops *pvendor = pmuic->regmapdesc->vendorops;
	int prev_status = pdesc->ccic_evt_attached;

	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->attach, pnoti->cable_type, pnoti->rprd);

	pdesc->ccic_evt_attached = pnoti->attach ? 
		MUIC_CCIC_NOTI_ATTACH : MUIC_CCIC_NOTI_DETACH;

	/* Attached */
	if (pdesc->ccic_evt_attached == MUIC_CCIC_NOTI_ATTACH) {
		pr_info("%s: Attach\n", __func__);
		pmuic->is_ccic_attach = true;
		pmuic->is_ccic_afc_enable = pnoti->cable_type;

		switch (pnoti->cable_type) {
		case Rp_56K:
			if (!pmuic->is_ccic_rp56_enable) {
				switch (pmuic->attached_dev) {
				case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
					pr_info("%s: 9V HV Charging Start!\n", __func__);
					pmuic->phv->tx_data = MUIC_HV_9V;
					max77854_hv_muic_connect_start(pmuic->phv);
					break;
				default:
					break;
				}
				pmuic->is_ccic_rp56_enable = true;
			}
			break;
		case Rp_Abnormal:
			/* Chgtype re-detect for AFC detach -> TA attach */
			switch (pmuic->attached_dev) {
			case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
			case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
			case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
			case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
				pr_info("%s: CC or SBU short. Chgdet Re-run.\n", __func__);
				hv_muic_chgdet_ready(pmuic->phv);
				if (pvendor && pvendor->run_chgdet)
					pvendor->run_chgdet(pmuic->regmapdesc, 1);
				break;
			default:
				break;
			}
		default:
			break;
		}

		if (pdesc->ccic_evt_roleswap) {
			pr_info("%s: roleswap event, attach USB\n", __func__);
			pdesc->ccic_evt_roleswap = 0;
			if (mdev_get_vbus(pmuic)) {
				pdesc->mdev = ATTACHED_DEV_USB_MUIC;
				mdev_noti_attached(pdesc->mdev);
			}
			return 0;
		}

		if (pnoti->rprd) {
			pr_info("%s: RPRD\n", __func__);
			pdesc->ccic_evt_rprd = 1;
			if (pvendor && pvendor->enable_chgdet)
				pvendor->enable_chgdet(pmuic->regmapdesc, 0);
			pdesc->mdev = ATTACHED_DEV_OTG_MUIC;
			mdev_com_to(pmuic, MUIC_PATH_USB_AP);
			mdev_noti_attached(pdesc->mdev);
			return 0;
		}

		if (mdev_is_valid_RID_OPEN(pmuic, vbus))
			pr_info("%s: Valid VBUS-> handled in irq handler\n", __func__);
		else
			pr_info("%s: No VBUS-> doing nothing.\n", __func__);

		/* CCIC ATTACH means NO WATER */
		if (pmuic->afc_water_disable) {
#if defined(CONFIG_HICCUP_CHARGER)
			enable_chgdet(pmuic, 1);
#endif
			pr_info("%s: Water is not detected, AFC Enable\n", __func__);
			pmuic->afc_water_disable = false;
		}

#if defined(CONFIG_MUIC_HV)
		/* For dry case after water */
		if (pmuic->attached_dev == ATTACHED_DEV_TA_MUIC) {
			if (pmuic->pdata->afc_disable)
				pr_info("%s: AFC Disabled(%d) by USER\n", __func__, pmuic->pdata->afc_disable);
			else if (pmuic->attached_dev == ATTACHED_DEV_TA_MUIC) {
				if ((pmuic->phv->is_afc_muic_ready == false) &&
						vps_is_hv_ta(&pmuic->vps)) {
					if (pmuic->is_ccic_afc_enable == Rp_56K) {
						pmuic->phv->is_afc_muic_prepare = true;
						max77854_muic_prepare_afc_charger(pmuic->phv);
					}
				}
			}
		}
#endif

		/* W/A for Incomplete insertion case */
		pdesc->ccic_evt_dcdcnt = 0;
		if (prev_status != MUIC_CCIC_NOTI_ATTACH &&
				pmuic->is_dcdtmr_intr && vbus) {
			if (pmuic->vps.t.chgdetrun) {
				pr_info("%s: Incomplete insertion. Chgdet runnung\n", __func__);
				return 0;
			}
			pr_info("%s: Incomplete insertion. Do chgdet again\n", __func__);
			pmuic->is_dcdtmr_intr = false;
			if (pvendor && pvendor->run_chgdet)
				pvendor->run_chgdet(pmuic->regmapdesc, 1);
		}

	} else {
		if (pnoti->rprd) {
			/* Role swap detach: attached=0, rprd=1 */
			pr_info("%s: role swap event\n", __func__);
			pdesc->ccic_evt_roleswap = 1;
		} else if (vbus) {
			pr_info("%s: Valid VBUS, return\n", __func__);
		} else {
			/* Detached */
			mdev_handle_ccic_detach(pmuic);
		}
	}

	return 0;
}

static int mdev_handle_factory_jig(muic_data_t *pmuic, int rid, int vbus)
{
	struct mdev_desc_t *pdesc = &mdev_desc;
	int mdev = 0;

	pr_info("%s: rid:%d vbus:%d\n", __func__, rid, vbus);

	switch (rid) {
	case RID_255K:
	case RID_301K:
		mdev_com_to(pmuic, MUIC_PATH_USB_AP);
		break;
	case RID_523K:
	case RID_619K:
		mdev_com_to(pmuic, MUIC_PATH_UART_AP);
		break;
	default:
		pr_info("%s: Unsupported rid\n", __func__);
		return 0;
	}

	mdev = rid_to_mdev_with_vbus(pmuic, rid, vbus);

	if (mdev != pdesc->mdev) {
		if (mdev_is_supported(pdesc->mdev)) {
			mdev_noti_detached(pdesc->mdev);
			pdesc->mdev = 0;
		}
		else if (pmuic->legacy_dev != ATTACHED_DEV_NONE_MUIC) {
			mdev_noti_detached(pmuic->legacy_dev);
			pmuic->legacy_dev = 0;
		}

		pdesc->mdev = mdev;
		mdev_noti_attached(mdev);
	}

	return 0;
}

static int muic_handle_ccic_RID(muic_data_t *pmuic, CC_NOTI_RID_TYPEDEF *pnoti)
{
	struct mdev_desc_t *pdesc = &mdev_desc;
	int rid, vbus;

	pr_info("%s: src:%d dest:%d id:%d rid:%d sub2:%d sub3:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->rid, pnoti->sub2, pnoti->sub3);

	rid = pnoti->rid;

	if (rid > RID_OPEN) {
		pr_info("%s: Out of range of RID\n", __func__);
		return 0;
	}

	if (pdesc->ccic_evt_attached != MUIC_CCIC_NOTI_ATTACH) {
		pr_info("%s: RID but No ATTACH->discarded\n", __func__);
		return 0;
	}

	pdesc->ccic_evt_rid = rid;

	switch (rid) {
	case RID_000K:
		pr_info("%s: OTG -> RID000K\n", __func__);
		mdev_com_to(pmuic, MUIC_PATH_USB_AP);
		vbus = mdev_get_vbus(pmuic);
		pdesc->mdev = rid_to_mdev_with_vbus(pmuic, rid, vbus);
		return 0;
	case RID_001K:
		pr_info("%s: MHL -> discarded.\n", __func__);
		return 0;
	case RID_255K:
	case RID_301K:
	case RID_523K:
	case RID_619K:
		vbus = mdev_get_vbus(pmuic);
		mdev_handle_factory_jig(pmuic, rid, vbus);
		break;
	case RID_OPEN:
	case RID_UNDEFINED:
		vbus = mdev_get_vbus(pmuic);
		if (pdesc->ccic_evt_attached == MUIC_CCIC_NOTI_ATTACH &&
			mdev_is_valid_RID_OPEN(pmuic, vbus)) {
				/*
				 * USB team's requirement.
				 * Set AP USB for enumerations.
				 */
				mdev_com_to(pmuic, MUIC_PATH_USB_AP);

				mdev_handle_legacy_TA_USB(pmuic);
		} else {
			/* RID OPEN + No VBUS = Assume detach */
			mdev_handle_ccic_detach(pmuic);
		}
		break;
	default:
		pr_err("%s:Undefined RID\n", __func__);
		return 0;
	}

	return 0;
}

static int muic_handle_ccic_WATER(muic_data_t *pmuic, CC_NOTI_ATTACH_TYPEDEF *pnoti)
{
	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->attach, pnoti->cable_type, pnoti->rprd);

	if (pnoti->attach == CCIC_NOTIFY_ATTACH) {
		pr_info("%s: Water detect\n", __func__);
#if defined(CONFIG_HICCUP_CHARGER)
		/* set path open initially at WATER state */
		enable_chgdet(pmuic, 0);
		mdev_com_to(pmuic, MUIC_PATH_OPEN);
#endif
		pmuic->afc_water_disable = true;
	} else {
		pr_info("%s: Undefined notification, Discard\n", __func__);
	}

	return 0;
}

static int muic_handle_ccic_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	CC_NOTI_TYPEDEF *pnoti = (CC_NOTI_TYPEDEF *)data;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
		muic_data_t *pmuic =
			container_of(nb, muic_data_t, manager_nb);
#else
		muic_data_t *pmuic =
			container_of(nb, muic_data_t, ccic_nb);
#endif

	pr_info("%s: Rcvd Noti=> action: %d src:%d dest:%d id:%d sub[%d %d %d]\n", __func__,
		(int)action, pnoti->src, pnoti->dest, pnoti->id, pnoti->sub1, pnoti->sub2, pnoti->sub3);

#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	if(pnoti->dest != CCIC_NOTIFY_DEV_MUIC) {
		pr_info("%s destination id is invalid\n", __func__);
		return 0;
	}
#endif

	mdev_show_status(pmuic);

	switch (pnoti->id) {
	case CCIC_NOTIFY_ID_ATTACH:
		pr_info("%s: CCIC_NOTIFY_ID_ATTACH: %s\n", __func__,
				pnoti->sub1 ? "Attached": "Detached");
		muic_handle_ccic_ATTACH(pmuic, (CC_NOTI_ATTACH_TYPEDEF *)pnoti);
		break;
	case CCIC_NOTIFY_ID_RID:
		pr_info("%s: CCIC_NOTIFY_ID_RID\n", __func__);
		muic_handle_ccic_RID(pmuic, (CC_NOTI_RID_TYPEDEF *)pnoti);
		break;
	case CCIC_NOTIFY_ID_WATER:
		pr_info("%s: CCIC_NOTIFY_ID_WATER\n", __func__);
		muic_handle_ccic_WATER(pmuic, (CC_NOTI_ATTACH_TYPEDEF *)pnoti);
		break;
	default:
		pr_info("%s: Undefined Noti. ID\n", __func__);
		return NOTIFY_DONE;
	}

	mdev_show_status(pmuic);

	muic_print_reg_dump(pmuic);

	return NOTIFY_DONE;
}


void __delayed_ccic_notifier(struct work_struct *work)
{
	muic_data_t *pmuic;
	int ret = 0;

	pr_info("%s\n", __func__);

	pmuic = container_of(work, muic_data_t, ccic_work.work);
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	ret = manager_notifier_register(&pmuic->manager_nb,
		muic_handle_ccic_notification, MANAGER_NOTIFY_CCIC_MUIC);
#else
	ret = ccic_notifier_register(&pmuic->ccic_nb,
		muic_handle_ccic_notification, CCIC_NOTIFY_DEV_MUIC);
#endif
	if (ret < 0) {
		pr_info("%s: CCIC Noti. is not ready. Try again in 4sec...\n", __func__);
		schedule_delayed_work(&pmuic->ccic_work, msecs_to_jiffies(4000));
		return;
	}

	pr_info("%s: done.\n", __func__);
}

void muic_register_ccic_notifier(muic_data_t *pmuic)
{
	int ret = 0;

	pr_info("%s: Registering CCIC_NOTIFY_DEV_MUIC.\n", __func__);

	init_mdev_desc(pmuic);
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	ret = manager_notifier_register(&pmuic->manager_nb,
		muic_handle_ccic_notification, MANAGER_NOTIFY_CCIC_MUIC);
#else
	ret = ccic_notifier_register(&pmuic->ccic_nb,
		muic_handle_ccic_notification, CCIC_NOTIFY_DEV_MUIC);
#endif
	if (ret < 0) {
		pr_info("%s: CCIC Noti. is not ready. Try again in 8sec...\n", __func__);
		INIT_DELAYED_WORK(&pmuic->ccic_work, __delayed_ccic_notifier);
		schedule_delayed_work(&pmuic->ccic_work, msecs_to_jiffies(8000));
		return;
	}

	pr_info("%s: done.\n", __func__);
}
