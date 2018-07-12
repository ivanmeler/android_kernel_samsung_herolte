/*
 * muic_vps.c
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

#include "muic-internal.h"
#include "muic_apis.h"
#include "muic_i2c.h"
#include "muic_vps.h"

/* Device Type 1 register */
#define DEV_TYPE1_USB_OTG		(0x1 << 7)
#define DEV_TYPE1_DEDICATED_CHG		(0x1 << 6)
#define DEV_TYPE1_CDP			(0x1 << 5)
#define DEV_TYPE1_T1_T2_CHG		(0x1 << 4)
#define DEV_TYPE1_UART			(0x1 << 3)
#define DEV_TYPE1_USB			(0x1 << 2)
#define DEV_TYPE1_AUDIO_2		(0x1 << 1)
#define DEV_TYPE1_AUDIO_1		(0x1 << 0)
#define DEV_TYPE1_USB_TYPES	(DEV_TYPE1_USB_OTG | DEV_TYPE1_CDP | \
				DEV_TYPE1_USB)
#define DEV_TYPE1_CHG_TYPES	(DEV_TYPE1_DEDICATED_CHG | DEV_TYPE1_CDP)

/* Device Type 2 register */
#define DEV_TYPE2_AV			(0x1 << 6)
#define DEV_TYPE2_TTY			(0x1 << 5)
#define DEV_TYPE2_PPD			(0x1 << 4)
#define DEV_TYPE2_JIG_UART_OFF		(0x1 << 3)
#define DEV_TYPE2_JIG_UART_ON		(0x1 << 2)
#define DEV_TYPE2_JIG_USB_OFF		(0x1 << 1)
#define DEV_TYPE2_JIG_USB_ON		(0x1 << 0)
#define DEV_TYPE2_JIG_USB_TYPES		(DEV_TYPE2_JIG_USB_OFF | \
					DEV_TYPE2_JIG_USB_ON)
#define DEV_TYPE2_JIG_UART_TYPES	(DEV_TYPE2_JIG_UART_OFF)
#define DEV_TYPE2_JIG_TYPES		(DEV_TYPE2_JIG_UART_TYPES | \
					DEV_TYPE2_JIG_USB_TYPES)

/* Device Type 3 register */
#define DEV_TYPE3_U200_CHG		(0x1 << 6)
#define DEV_TYPE3_AV_WITH_VBUS		(0x1 << 4)
#define DEV_TYPE3_NO_STD_CHG		(0x1 << 2)
#define DEV_TYPE3_MHL			(0x1 << 0)
#define DEV_TYPE3_CHG_TYPE	(DEV_TYPE3_U200_CHG | DEV_TYPE3_NO_STD_CHG)

static struct vps_cfg cfg_MHL = {
	.name = "MHL",
	.attr = MATTR(VCOM_OPEN, VB_ANY)
};
static struct vps_cfg cfg_OTG = {
	.name = "OTG",
	.attr = MATTR(VCOM_USB, VB_ANY),
};
static struct vps_cfg cfg_VZW_ACC = {
	.name = "VZW Accessory",
	.attr = MATTR(VCOM_OPEN, VB_ANY),
};
static struct vps_cfg cfg_VZW_INCOMPATIBLE = {
	.name = "VZW Incompatible",
	.attr = MATTR(VCOM_OPEN, VB_ANY),
};
static struct vps_cfg cfg_RDU_TA = {
	.name = "RDU TA",
	.attr = MATTR(VCOM_OPEN, VB_HIGH) | MATTR_CDET_SUPP,
};
static struct vps_cfg cfg_HMT = {
	.name = "HMT",
	.attr = MATTR(VCOM_USB, VB_ANY),
};
static struct vps_cfg cfg_AUDIODOCK = {
	.name = "Audiodock",
	.attr = MATTR(VCOM_USB, VB_HIGH),
};
static struct vps_cfg cfg_USB_LANHUB = {
	.name = "USB LANHUB",
	.attr = MATTR(VCOM_OPEN, VB_ANY),
};
static struct vps_cfg cfg_CHARGING_CABLE = {
	.name = "Charging Cable",
	.attr = MATTR(VCOM_OPEN, VB_ANY),
};
static struct vps_cfg cfg_GAMEPAD = {
	.name = "Game Pad",
	.attr = MATTR(VCOM_USB, VB_ANY),
};
static struct vps_cfg cfg_TYPE1_CHG = {
	.name = "TYPE1 Charger",
	.attr = MATTR(VCOM_OPEN, VB_HIGH) | MATTR_CDET_SUPP,
};
static struct vps_cfg cfg_JIG_USB_OFF = {
	.name = "Jig USB Off",
	.attr = MATTR(VCOM_USB, VB_HIGH) | MATTR_FACT_SUPP,
};
static struct vps_cfg cfg_JIG_USB_ON = {
	.name = "Jig USB On",
	.attr = MATTR(VCOM_USB, VB_HIGH) | MATTR_FACT_SUPP,
};
static struct vps_cfg cfg_DESKDOCK = {
	.name = "Deskdock",
	.attr = MATTR(VCOM_OPEN, VB_ANY),
};
static struct vps_cfg cfg_TYPE2_CHG = {
	.name = "TYPE2 Charger",
	.attr = MATTR(VCOM_OPEN, VB_HIGH),
};
static struct vps_cfg cfg_JIG_UART_OFF = {
	.name = "Jig UART Off",
	.attr = MATTR(VCOM_UART, VB_ANY) | MATTR_FACT_SUPP,
};
//CONFIG_SEC_FACTORY
static struct vps_cfg cfg_JIG_UART_ON = {
	.name = "Jig UART On",
	.attr = MATTR(VCOM_UART, VB_ANY) | MATTR_FACT_SUPP,
};
static struct vps_cfg cfg_TA = {
	.name = "TA",
	.attr = MATTR(VCOM_OPEN, VB_HIGH) | MATTR_CDET_SUPP,
};
static struct vps_cfg cfg_USB = {
	.name = "USB",
	.attr = MATTR(VCOM_OPEN, VB_HIGH) | MATTR_CDET_SUPP,
};
static struct vps_cfg cfg_CDP = {
	.name = "CDP",
	.attr = MATTR(VCOM_OPEN, VB_HIGH) | MATTR_CDET_SUPP,
};
static struct vps_cfg cfg_UNDEFINED_CHARGING = {
	.name = "Undefined Charging",
	.attr = MATTR(VCOM_OPEN, VB_HIGH) | MATTR_SUPP,
};
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
static struct vps_cfg cfg_POGO_DOCK = {
	.name = "Pogo dock",
	.attr = MATTR(VCOM_OPEN, VB_HIGH) | MATTR_SUPP,
};
#endif

static struct vps_tbl_data vps_table[] = {
	[MDEV(OTG)]			= {0x00, "GND",	&cfg_OTG,},
	[MDEV(MHL)]			= {0xfe, "1K",		&cfg_MHL,},
	/* 0x01 ~ 0x0D : Remote Sx Button */
	[MDEV(VZW_ACC)]		= {0x0e, "28.7K",	&cfg_VZW_ACC,},
	[MDEV(VZW_INCOMPATIBLE)]	= {0x0f, "34K",	&cfg_VZW_INCOMPATIBLE,},
	[MDEV(RDU_TA)]		= {0x10, "40.2K",	&cfg_RDU_TA,},
	[MDEV(HMT)]			= {0x11, "49.9K",	&cfg_HMT,},
	[MDEV(AUDIODOCK)]		= {0x12, "64.9K",	&cfg_AUDIODOCK,},
	[MDEV(USB_LANHUB)]		= {0x13, "80.07K",	&cfg_USB_LANHUB,},
	[MDEV(CHARGING_CABLE)]	= {0x14, "102K",	&cfg_CHARGING_CABLE,},
	[MDEV(GAMEPAD)]		= {0x15, "121K",	&cfg_GAMEPAD,},
	/* 0x16: UART Cable */
	[MDEV(TYPE1_CHG)]		= {0x17, "200K",	&cfg_TYPE1_CHG,},
	[MDEV(JIG_USB_OFF)]		= {0x18, "255K",	&cfg_JIG_USB_OFF,},
	[MDEV(JIG_USB_ON)]		= {0x19, "301K",	&cfg_JIG_USB_ON,},
	[MDEV(DESKDOCK)]		= {0x1a, "365K",	&cfg_DESKDOCK,},
	[MDEV(TYPE2_CHG)]		= {0x1b, "442K",	&cfg_TYPE2_CHG,},
	[MDEV(JIG_UART_OFF)]		= {0x1c, "523K",	&cfg_JIG_UART_OFF,},
	[MDEV(JIG_UART_ON)]		= {0x1d, "619K",	&cfg_JIG_UART_ON,},
	/* 0x1e: Audio Mode with Remote */
	[MDEV(TA)]			= {0x1f, "OPEN",	&cfg_TA,},
	[MDEV(USB)]			= {0x1f, "OPEN",	&cfg_USB,},
	[MDEV(CDP)]			= {0x1f, "OPEN",	&cfg_CDP,},
	[MDEV(UNDEFINED_CHARGING)]	= {0xfe, "UNDEFINED",	&cfg_UNDEFINED_CHARGING,},
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	[MDEV(POGO_DOCK)]		= {0x1f, "OPEN",	&cfg_POGO_DOCK,},
#endif
	[ATTACHED_DEV_NUM]		= {0x00, "NUM", NULL,},
};

static bool mdev_undefined_range(int adc)
{
	switch (adc) {
	case ADC_SEND_END ... ADC_REMOTE_S12:
	case ADC_UART_CABLE:
	case ADC_AUDIOMODE_W_REMOTE:
		return true;
	default:
		break;
	}

	return false;
}

static bool vps_is_acceptable(muic_data_t *pmuic, int adc)
{
	if (pmuic->attached_dev == ATTACHED_DEV_HMT_MUIC) {
		if ((adc == ADC_OPEN) || (adc == ADC_HMT))
			return true;
		else
			return false;
	} else if (pmuic->attached_dev == ATTACHED_DEV_GAMEPAD_MUIC) {
		if ((adc == ADC_OPEN) || (adc == ADC_GAMEPAD) || (adc == ADC_GND))
			return true;
		else
			return false;
	} else
		return true;
}

struct vps_tbl_data * mdev_to_vps(muic_attached_dev_t mdev)
{
	if (mdev >= ATTACHED_DEV_NUM) {
		pr_err("%s Out of range mdev=%d\n", __func__, mdev);
		return NULL;
	}

	return &vps_table[mdev];
}

bool vps_name_to_mdev(const char *name, int *sdev)
{
	struct vps_tbl_data *pvps;
	int mdev;

	for (mdev = MDEV(NONE); mdev < ATTACHED_DEV_NUM; mdev++) {
		pvps = &vps_table[mdev];
		if (!pvps->cfg)
			continue;

		if (!strcmp(pvps->cfg->name, name)){
			break;
		}
	}

	if (mdev >= ATTACHED_DEV_NUM) {
		pr_err("%s Out of range mdev=%d, %s\n", __func__,
						mdev, name);
		return false;
	}

	pr_debug("%s:%s->[%2d]\n", __func__, pvps->cfg->name, mdev);

	*sdev = mdev;

	return true;
}

bool vps_is_supported_dev(muic_attached_dev_t mdev)
{
	struct vps_tbl_data *pvps = mdev_to_vps(mdev);
	int attr;

	if (!pvps || !pvps->cfg)
		return false;

	attr = pvps->cfg->attr;

	if (MATTR_TO_SUPP(attr))
		return true;

	return false;
}

void vps_update_supported_attr(muic_attached_dev_t mdev, bool supported)
{
	struct vps_tbl_data *pvps = mdev_to_vps(mdev);

	if (!pvps || !pvps->cfg)
		return;

	if (supported)
		pvps->cfg->attr |= MATTR_SUPP;
	else
		pvps->cfg->attr &= (~MATTR_SUPP) & 0xFFFFFFFF;
}

bool vps_is_factory_dev(muic_attached_dev_t mdev)
{
	struct vps_tbl_data *pvps = mdev_to_vps(mdev);
	int attr;

	if (!pvps || !pvps->cfg)
		return false;

	attr = pvps->cfg->attr;

	if (MATTR_TO_FACT(attr))
		return true;

	return false;
}

static bool vps_is_1k_mhl_cable(vps_data_t *pmsr)
{
	return pmsr->t.adc1k ? true : false;
}

static bool vps_is_adc(vps_data_t *pmsr, struct vps_tbl_data *pvps)
{
	if (pmsr->t.adc == pvps->adc)
		return true;

	 return false;
}


static bool vps_is_vbvolt(vps_data_t *pmsr, struct vps_tbl_data *pvps)
{
	int attr = pvps->cfg->attr;

	if (pmsr->t.vbvolt == MATTR_TO_VBUS(attr))
		return true;

	if (MATTR_TO_VBUS(attr) == VB_ANY)
		return true;

	 return false;
}

/* Check it the resolved device type is treated as a different one
  * when VBUS also comes along.
  */
int resolve_twin_mdev(int mdev, bool vbus)
{
	if (vbus) {
		if (mdev == MDEV(DESKDOCK)) {
			pr_info("%s: mdev:%d vbus:%d\n",__func__, mdev, vbus);
			return MDEV(DESKDOCK_VB);
		}
		else if (mdev == MDEV(JIG_UART_ON)) {
			pr_info("%s: mdev:%d vbus:%d\n",__func__, mdev, vbus);
			return MDEV(JIG_UART_ON_VB);
		}
	}

	return 0;
}

/* 
 * To filter out the chargers which don't support AFC,
 * such as LO.
 */
bool vps_is_hv_ta(vps_data_t *pvps)
{
	if (pvps->t.chgtyp == CHGTYP_DEDICATED_CHARGER)
		return true;

	return false;
}

int vps_chgtyp_to_dev(int chgtyp)
{
	int ret = -1;

	switch (chgtyp) {
	case CHGTYP_USB:
		ret = ATTACHED_DEV_USB_MUIC;
		break;
	case CHGTYP_CDP:
		ret = ATTACHED_DEV_CDP_MUIC;
		break;
	case CHGTYP_DEDICATED_CHARGER:
		ret = ATTACHED_DEV_TA_MUIC;
		break;
	case CHGTYP_500MA:
	case CHGTYP_1A:
	case CHGTYP_SPECIAL_3_3V_CHARGER:
		ret = ATTACHED_DEV_TA_MUIC;
		break;
	case CHGTYP_NO_VOLTAGE:
	default:
		pr_err("%s: Undefined chgtyp %d\n",__func__, chgtyp);
	}

	return ret;
}

int resolve_dev_based_on_adc_chgtype(muic_data_t *pmuic, vps_data_t *pmsr)
{
	int dev_type;

	pr_info("%s: adc=%02x, chgtyp=%02x\n",__func__, pmsr->t.adc, pmsr->t.chgtyp);

	switch(pmsr->t.adc){
	case ADC_OPEN:
		dev_type = vps_chgtyp_to_dev(pmsr->t.chgtyp);
		if (dev_type < 0) {
			pr_info("%s not able to resolve using ADC and CHGTYPE[OPEN]\n",__func__);
	                return -1;
		}
		break;
	case ADC_CEA936ATYPE1_CHG:
	case ADC_219:
		if(pmsr->t.chgtyp == CHGTYP_DEDICATED_CHARGER)
                        dev_type = ATTACHED_DEV_TA_MUIC;
                else if(pmsr->t.chgtyp == CHGTYP_UNOFFICIAL_CHARGER)
                        dev_type = ATTACHED_DEV_TA_MUIC;
                else if(pmsr->t.chgtyp == CHGTYP_CDP)
                        dev_type = ATTACHED_DEV_CDP_MUIC;
                else if(pmsr->t.chgtyp == CHGTYP_USB)
                        dev_type = ATTACHED_DEV_USB_MUIC;
                else {
                        pr_info("%s not able to resolve using ADC and CHGTYPE[219K]\n",__func__);
                        return -1;
                }
		break;
	case ADC_RDU_TA:
		if (pmsr->t.chgtyp == CHGTYP_DEDICATED_CHARGER)
			dev_type = ATTACHED_DEV_RDU_TA_MUIC;
		else {
			pr_info("%s abnormal RDU_TA\n",__func__);
	                return -1;
		}
		break;
	default:
		pr_info("%s not able to resolve using ADC and CHGTYPE\n",__func__);
		return -1;
	}

	return dev_type;
}

int vps_find_attached_dev(muic_data_t *pmuic, muic_attached_dev_t *pdev, int *pintr)
{
	struct vps_tbl_data *pvps;
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC, mdev;
	vps_data_t *pmsr = &pmuic->vps;
	int attr, ret = 0;
	int intr = MUIC_INTR_ATTACH;
	int chgdet_dev = 0;

	pr_debug("%s\n",__func__);


	if (pmuic->discard_interrupt) {
		pr_info("%s:%s Under ADC mode change.\n", MUIC_DEV_NAME, __func__);
		return -1;
	}

	if ((pmsr->t.vbvolt == VB_HIGH) && pmsr->t.chgdetrun &&
		(pmsr->t.adc != ADC_DESKDOCK)) {
		pr_info("%s:%s chgdet is running.\n", MUIC_DEV_NAME, __func__);
		return -1;
	}

	for (mdev = MDEV(NONE); mdev < ATTACHED_DEV_NUM; mdev++) {
		pvps = &vps_table[mdev];
		if (!pvps->cfg)
			continue;

		attr = pvps->cfg->attr;

		if (vps_is_1k_mhl_cable(pmsr)) {
			new_dev = mdev = MDEV(MHL);
			pr_info("%s:%s MHL found at mdev:%d(%s)\n",
					MUIC_DEV_NAME, __func__, mdev, pvps->cfg->name);
			break;
		}

#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
		if (gpio_is_valid(pmuic->dock_int_ap) && gpio_get_value(pmuic->dock_int_ap) == 0
				&& pmsr->t.vbvolt == VB_HIGH) {
			new_dev = MDEV(POGO_DOCK);
			pr_info("%s:%s POGO DOCK found at mdev:%d\n",
					MUIC_DEV_NAME, __func__, mdev);
			break;
		}
#endif

		if (!vps_is_adc(pmsr, pvps))
			continue;

		if (!vps_is_vbvolt(pmsr, pvps))
			continue;

		if(MATTR_TO_CDET(attr)){
			if (!pmsr->t.chgtyp) {
				intr = MUIC_INTR_DETACH;
				pr_info("%s:%s No chgtyp. Assumes detach.\n", MUIC_DEV_NAME, __func__);
				break;
			}

			if (pmsr->t.DCDTimedout) {
				pr_info("%s:%s DCDTimedout. Forced To USB\n", MUIC_DEV_NAME, __func__);
				new_dev = mdev = MDEV(USB);
				break;
			}

			/* some function for cdeten check */
			chgdet_dev = resolve_dev_based_on_adc_chgtype(pmuic,pmsr);
			if (chgdet_dev < 0) {
				chgdet_dev = 0;
				continue;
			}
		}

		pr_info("%s:%s vps table match found at [1]mdev:%d(%s)\n",
				MUIC_DEV_NAME, __func__, mdev, pvps->cfg->name);

		new_dev = chgdet_dev ? chgdet_dev : mdev;

		break;
	}

	/* do nothing */
	if (ret < 0)
		return -1;

	/* Check Undefined range */
	if (pmuic->undefined_range) {
		if ((pmsr->t.vbvolt == VB_HIGH) &&
			mdev_undefined_range(pmsr->t.adc)) {
			pr_info("%s:%s undefined range adc:%02x\n",
						MUIC_DEV_NAME, __func__, pmsr->t.adc);

			*pintr = intr = MUIC_INTR_ATTACH;
			*pdev = new_dev = ATTACHED_DEV_UNDEFINED_RANGE_MUIC;
			return 0;
		}
	}

	if (!vps_is_acceptable(pmuic, pmsr->t.adc)) {
		pr_info("%s:%s Unacceptable adc(0x%02x) for HMT -> discarded.\n",
			MUIC_DEV_NAME, __func__, pmsr->t.adc);

		com_to_open_with_vbus(pmuic);
		return -1;
	}

	if (mdev == ATTACHED_DEV_NUM) {
		/* Check the cable types which are not listed in the vps table if vbus is. */
		if (pmsr->t.vbvolt == VB_HIGH) {
			new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
			pr_info("%s:%s unsupported ID + VB [%2d]\n",
						MUIC_DEV_NAME, __func__, new_dev);
		}
		else
		{
			intr = MUIC_INTR_DETACH;
			new_dev = ATTACHED_DEV_NONE_MUIC;
			if(pvps->cfg)
				pr_info("%s:%s vps table match found at [2]mdev:%d(%s)\n",
						MUIC_DEV_NAME, __func__, mdev, pvps->cfg->name);
		}
	} else {
		/* Check if the cable type is supported with the attr of the cable.
		  */

		int twin_mdev = 0;

		if (vps_is_supported_dev(new_dev)) {
			if ((twin_mdev = resolve_twin_mdev(new_dev, pmsr->t.vbvolt))) {
				new_dev = twin_mdev;
				pr_info("%s:Supported twin mdev-> %d\n", __func__, twin_mdev);
			} else
				pr_info("%s:Supported.\n", __func__);

		} else if(pmsr->t.vbvolt && (intr == MUIC_INTR_ATTACH) &&
			!MATTR_TO_NOCHG(attr)) {
			new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
			pr_info("%s:Unsupported->UNDEFINED_CHARGING\n", __func__);
		} else {
			intr = MUIC_INTR_DETACH;
			new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
			pr_info("%s:Unsupported->Discarded.\n", __func__);
		}
	}

	*pintr = intr;
	*pdev = new_dev;

	return ret;
}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
void vps_show_table(void)
{
	struct vps_tbl_data *pvps;
	int mdev, attr;

	pr_info(" %4s%6s%10s %12s %29s\n", "dev", "ADC", "(RID)", "attr", "vps_name");
	for (mdev = MDEV(NONE); mdev < ATTACHED_DEV_NUM; mdev++) {

		pvps = &vps_table[mdev];

		if (!pvps->cfg)
			continue;

		attr = pvps->cfg->attr;

		pr_info(" [%2d] = %02x(%10s) %08x:%c:%c %28s\n", mdev,
			pvps->adc, pvps->rid, pvps->cfg->attr,
			MATTR_TO_SUPP(attr) ? 'S' : '_',
			MATTR_TO_FACT(attr) ? 'F' : '_',
			pvps->cfg->name);
	}

	pr_info("done.\n");

}
#endif

void vps_show_supported_list(void)
{
	struct vps_tbl_data *pvps;
	int mdev, attr;

	pr_info(" %4s%6s%10s %12s %30s\n", "dev", "ADC", "(RID)", "attr", "vps_name");
	for (mdev = MDEV(NONE); mdev < ATTACHED_DEV_NUM; mdev++) {

		pvps = &vps_table[mdev];

		if (!pvps->cfg)
			continue;

		attr = pvps->cfg->attr;

		if (!MATTR_TO_SUPP(attr))
			continue;

		pr_info(" [%2d] = %02x(%10s) %08x:%c:%c %28s\n", mdev,
			pvps->adc, pvps->rid, pvps->cfg->attr,
			MATTR_TO_SUPP(attr) ? 'S' : '_',
			MATTR_TO_FACT(attr) ? 'F' : '_',
			pvps->cfg->name);


	}

	pr_info("done.\n");

}


static int resolve_dedicated_dev(muic_data_t *pmuic, muic_attached_dev_t *pdev, int *pintr)
{
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	int vbvolt = 0;
	int val1, val2, val3, adc;

	val1 = pmuic->vps.s.val1;
	val2 = pmuic->vps.s.val2;
	val3 = pmuic->vps.s.val3;
	adc = pmuic->vps.s.adc;
	vbvolt = pmuic->vps.s.vbvolt;

	/* Attached */
	switch (val1) {
	case DEV_TYPE1_CDP:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_CDP_MUIC;
		pr_info("%s : USB_CDP DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_USB:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_MUIC;
		pr_info("%s : USB DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_DEDICATED_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s : DEDICATED CHARGER DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_USB_OTG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_OTG_MUIC;
		pr_info("%s : USB_OTG DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_AUDIO_2:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_LANHUB_MUIC;
		pr_info("%s : LANHUB DETECTED\n", MUIC_DEV_NAME);
		break;

	default:
		break;
	}

	switch (val2) {
	case DEV_TYPE2_JIG_UART_OFF:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		pr_info("%s : JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE2_JIG_USB_OFF:
		if (!vbvolt) break;
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		pr_info("%s : JIG_USB_OFF DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE2_JIG_USB_ON:
		if (!vbvolt) break;
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		pr_info("%s : JIG_USB_ON DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE2_TTY:
		/* MM-dock is handled in an attachment function with vbus */
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC;
		pr_info("%s : UNIVERSAL_MMDOCK DETECTED(%d)\n", MUIC_DEV_NAME, vbvolt);
		break;

	default:
		break;
	}

	if (val3 & DEV_TYPE3_CHG_TYPE)
	{
		intr = MUIC_INTR_ATTACH;

		if (val3 & DEV_TYPE3_NO_STD_CHG) {
			new_dev = ATTACHED_DEV_USB_MUIC;
			pr_info("%s : TYPE3 DCD_OUT_TIMEOUT DETECTED\n", MUIC_DEV_NAME);

		} else {
			new_dev = ATTACHED_DEV_TA_MUIC;
			pr_info("%s : TYPE3_CHARGER DETECTED\n", MUIC_DEV_NAME);
		}
	}

	if (val2 & DEV_TYPE2_AV || val3 & DEV_TYPE3_AV_WITH_VBUS)
	{
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_DESKDOCK_MUIC;
		pr_info("%s : DESKDOCK DETECTED\n", MUIC_DEV_NAME);
	}

	if (val3 & DEV_TYPE3_MHL)
	{
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_MHL_MUIC;
		pr_info("%s : MHL DETECTED\n", MUIC_DEV_NAME);
	}

	/* If there is no matching device found using device type registers
		use ADC to find the attached device */
	if(new_dev == ATTACHED_DEV_UNKNOWN_MUIC) {
		switch (adc) {
		case ADC_CEA936ATYPE1_CHG : /*200k ohm */
		{
			/* For LG USB cable which has 219k ohm ID */
			int rescanned_dev = do_BCD_rescan(pmuic);

			if (rescanned_dev > 0) {
				pr_info("%s : TYPE1 CHARGER DETECTED(USB)\n", MUIC_DEV_NAME);
				intr = MUIC_INTR_ATTACH;
				new_dev = rescanned_dev;
			}
			break;
		}
		case ADC_CEA936ATYPE2_CHG:
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_TA_MUIC;
			pr_info("%s : TYPE1/2 CHARGER DETECTED(TA)\n", MUIC_DEV_NAME);
			break;
		case ADC_JIG_USB_OFF: /* 255k */
			if (!vbvolt) break;
			if (new_dev != ATTACHED_DEV_JIG_USB_OFF_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
				pr_info("%s : ADC JIG_USB_OFF DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_USB_ON:
			if (!vbvolt) break;
			if (new_dev != ATTACHED_DEV_JIG_USB_ON_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
				pr_info("%s : ADC JIG_USB_ON DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_UART_OFF:
			if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
				pr_info("%s : ADC JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_UART_ON:
			/* This is the mode to wake up device during factory mode.
			 *  This device type SHOULD be handled in muic_state.c to
			 *  support both factory & rustproof mode.
			 */
			if (new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
				pr_info("%s : ADC JIG_UART_ON DETECTED\n", MUIC_DEV_NAME);
			}
			break;
#ifdef CONFIG_MUIC_SM5703_SUPPORT_AUDIODOCK
		case ADC_AUDIODOCK:
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_AUDIODOCK_MUIC;
			pr_info("%s : ADC AUDIODOCK DETECTED\n", MUIC_DEV_NAME);
			break;
#endif
		case ADC_CHARGING_CABLE:
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_CHARGING_CABLE_MUIC;
			pr_info("%s : PS_CABLE DETECTED\n", MUIC_DEV_NAME);
			break;
		case ADC_OPEN:
			/* sometimes muic fails to catch JIG_UART_OFF detaching */
			/* double check with ADC */
			if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) {
				new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
				intr = MUIC_INTR_DETACH;
				pr_info("%s : ADC OPEN DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		case ADC_GAMEPAD:
			pr_info("%s : ADC GAMEPAD Discarded\n", MUIC_DEV_NAME);
			break;

		case ADC_RESERVED_VZW:
			new_dev = ATTACHED_DEV_VZW_ACC_MUIC;
			intr = MUIC_INTR_ATTACH;
			pr_info("%s : ADC VZW_ACC DETECTED\n", MUIC_DEV_NAME);
			break;

		case ADC_INCOMPATIBLE_VZW:
			new_dev = ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC;
			intr = MUIC_INTR_ATTACH;
			pr_info("%s : ADC INCOMPATIBLE_VZW DETECTED\n", MUIC_DEV_NAME);
			break;

		default:
			pr_warn("%s:%s unsupported ADC(0x%02x)\n", MUIC_DEV_NAME,
				__func__, adc);
			if(vbvolt) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
				pr_info("%s : UNDEFINED VB DETECTED\n", MUIC_DEV_NAME);
			} else
				intr = MUIC_INTR_DETACH;
			break;
		}
	}

	/* Check if the cable type is supported.
	  */
	if (vps_is_supported_dev(new_dev))
		pr_info("%s:Supported.\n", __func__);
	else if(vbvolt && (intr == MUIC_INTR_ATTACH)) {
		new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
		pr_info("%s:Unsupported->UNDEFINED_CHARGING\n", __func__);
	} else {
		intr = MUIC_INTR_DETACH;
		new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
		pr_info("%s:Unsupported->Discarded.\n", __func__);
	}

	*pintr = intr;
	*pdev = new_dev;

	return 0;
}

int vps_resolve_dev(muic_data_t *pmuic, muic_attached_dev_t *pdev, int *pintr)
{
	if(pmuic->vps_table == VPS_TYPE_TABLE)
		return vps_find_attached_dev(pmuic,pdev,pintr);
	else
		return resolve_dedicated_dev(pmuic, pdev, pintr);
}
