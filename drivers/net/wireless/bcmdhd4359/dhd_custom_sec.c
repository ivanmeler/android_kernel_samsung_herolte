/*
 * Customer HW 4 dependant file
 *
 * Copyright (C) 1999-2018, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: dhd_custom_sec.c 334946 2012-05-24 20:38:00Z $
 */
#if defined(CUSTOMER_HW4) || defined(CUSTOMER_HW40)
#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>

#include <ethernet.h>
#include <dngl_stats.h>
#include <bcmutils.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include <dhd_linux.h>
#include <bcmdevs.h>

#include <linux/fcntl.h>
#include <linux/fs.h>

#if defined(ARGOS_CPU_SCHEDULER) && !defined(DHD_LB_IRQSET)
extern int argos_irq_affinity_setup_label(unsigned int irq, const char *label,
	struct cpumask *affinity_cpu_mask,
	struct cpumask *default_cpu_mask);
#endif /* ARGOS_CPU_SCHEDULER && !DHD_LB_IRQSET */

#if !defined(DHD_USE_CLMINFO_PARSER)
const struct cntry_locales_custom translate_custom_table[] = {
#if defined(BCM4330_CHIP) || defined(BCM4334_CHIP) || defined(BCM43241_CHIP)
	/* 4330/4334/43241 */
	{"AR", "AR", 1},
	{"AT", "AT", 1},
	{"AU", "AU", 2},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"CH", "CH", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DE", "DE", 3},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"ES", "ES", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"GB", "GB", 1},
	{"GR", "GR", 1},
	{"HR", "HR", 1},
	{"HU", "HU", 1},
	{"IE", "IE", 1},
	{"IS", "IS", 1},
	{"IT", "IT", 1},
	{"JP", "JP", 5},
	{"KR", "KR", 24},
	{"KW", "KW", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"LV", "LV", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"PY", "PY", 1},
	{"RO", "RO", 1},
	{"RU", "RU", 13},
	{"SE", "SE", 1},
	{"SI", "SI", 1},
	{"SK", "SK", 1},
	{"TW", "TW", 2},
#ifdef BCM4330_CHIP
	{"",   "XZ", 1},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 1},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 1},	/* Universal if Country code is SUDAN */
	{"GL", "XZ", 1},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 1},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 1},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 1},	/* Universal if Country code is MARSHALL ISLANDS */
	{"JO", "XZ", 1},	/* Universal if Country code is Jordan */
	{"PG", "XZ", 1},	/* Universal if Country code is Papua New Guinea */
	{"SA", "XZ", 1},	/* Universal if Country code is Saudi Arabia */
	{"AF", "XZ", 1},	/* Universal if Country code is Afghanistan */
	{"US", "US", 5},
	{"UA", "UY", 0},
	{"AD", "AL", 0},
	{"CX", "AU", 2},
	{"GE", "GB", 1},
	{"ID", "MW", 0},
	{"KI", "AU", 2},
	{"NP", "SA", 0},
	{"WS", "SA", 0},
	{"LR", "BR", 0},
	{"ZM", "IN", 0},
	{"AN", "AG", 0},
	{"AI", "AS", 0},
	{"BM", "AS", 0},
	{"DZ", "GB", 1},
	{"LC", "AG", 0},
	{"MF", "BY", 0},
	{"GY", "CU", 0},
	{"LA", "GB", 1},
	{"LB", "BR", 0},
	{"MA", "IL", 0},
	{"MO", "BD", 0},
	{"MW", "BD", 0},
	{"QA", "BD", 0},
	{"TR", "GB", 1},
	{"TZ", "BF", 0},
	{"VN", "BR", 0},
	{"AE", "AZ", 0},
	{"IQ", "GB", 1},
	{"CN", "CL", 0},
	{"MX", "MX", 1},
#else
	/* 4334/43241 */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"GL", "XZ", 11},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"US", "US", 46},
	{"UA", "UA", 8},
	{"CO", "CO", 4},
	{"ID", "ID", 1},
	{"LA", "LA", 1},
	{"LB", "LB", 2},
	{"VN", "VN", 4},
	{"MA", "MA", 1},
	{"TR", "TR", 7},
#endif /* defined(BCM4330_CHIP) */
#ifdef BCM4334_CHIP
	{"AE", "AE", 1},
	{"MX", "MX", 1},
#endif /* defined(BCM4334_CHIP) */
#ifdef BCM43241_CHIP
	{"AE", "AE", 6},
	{"BD", "BD", 2},
	{"CN", "CN", 38},
	{"MX", "MX", 20},
#endif /* defined(BCM43241_CHIP) */
#else  /* defined(BCM4330_CHIP) || defined(BCM4334_CHIP) || defined(BCM43241_CHIP) */
	/* default ccode/regrev */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
#if defined(BCM4359_CHIP)
	{"SX", "XZ", 11},	/* Universal if Country code is Sint Maarten */
	{"CC", "XZ", 11},	/* Universal if Country code is COCOS (KEELING) ISLANDS */
	{"HM", "XZ", 11},	/* Universal if Country code is HEARD ISLAND AND MCDONALD ISLANDS */
	{"PN", "XZ", 11},	/* Universal if Country code is PITCAIRN */
	{"AQ", "XZ", 11},	/* Universal if Country code is ANTARCTICA */
	{"AX", "XZ", 11},	/* Universal if Country code is ALAND ISLANDS */
	{"BV", "XZ", 11},	/* Universal if Country code is BOUVET ISLAND */
	{"GS", "XZ", 11},	/* Universal if Country code is
				 * SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS
				 */
	{"SH", "XZ", 11},	/* Universal if Country code is SAINT HELENA */
	{"SJ", "XZ", 11},	/* Universal if Country code is SVALBARD AND JAN MAYEN */
	{"SS", "XZ", 11},	/* Universal if Country code is SOUTH SUDAN */
#endif /* BCM4359_CHIP */
	{"GL", "GP", 2},
	{"AL", "AL", 2},
	{"AS", "AS", 12},
	{"AI", "AI", 1},
	{"AF", "AD", 0},
	{"AG", "AG", 2},
	{"AR", "AU", 6},
	{"AW", "AW", 2},
	{"AU", "AU", 6},
	{"AT", "AT", 4},
	{"AZ", "AZ", 2},
	{"BS", "BS", 2},
	{"BH", "BH", 4},
	{"BD", "BD", 1},
	{"BY", "BY", 3},
	{"BE", "BE", 4},
	{"BM", "BM", 12},
	{"BA", "BA", 2},
	{"BR", "BR", 2},
	{"VG", "VG", 2},
	{"BN", "BN", 4},
	{"BG", "BG", 4},
	{"KH", "KH", 2},
	{"KY", "KY", 3},
	{"CN", "CN", 38},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"HR", "HR", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GF", "GF", 2},
	{"DE", "DE", 7},
	{"GR", "GR", 4},
	{"GD", "GD", 2},
	{"GP", "GP", 2},
	{"GU", "GU", 30},
	{"HK", "HK", 2},
	{"HU", "HU", 4},
	{"IS", "IS", 4},
	{"IN", "IN", 3},
	{"ID", "ID", 1},
	{"IE", "IE", 5},
	{"IL", "IL", 14},
	{"IT", "IT", 4},
	{"JO", "JO", 3},
	{"KE", "SA", 0},
	{"KW", "KW", 5},
	{"LA", "LA", 2},
	{"LV", "LV", 4},
	{"LB", "LB", 5},
	{"LS", "LS", 2},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"MO", "SG", 0},
	{"MK", "MK", 2},
	{"MW", "MW", 1},
#if defined(BCM4359_CHIP)
	{"DZ", "DZ", 2},
#elif defined(DHD_SUPPORT_GB_999)
	{"DZ", "GB", 999},
#else
	{"DZ", "GB", 6},
#endif /* BCM4359_CHIP */
	{"MV", "MV", 3},
	{"MT", "MT", 4},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MU", "MU", 2},
	{"YT", "YT", 2},
	{"MX", "MX", 44},
	{"MD", "MD", 2},
	{"MC", "MC", 1},
	{"ME", "ME", 2},
	{"MA", "MA", 2},
	{"NL", "NL", 4},
	{"AN", "GD", 2},
	{"NZ", "NZ", 4},
	{"NO", "NO", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PG", "AU", 6},
	{"PY", "PY", 2},
	{"PE", "PE", 20},
	{"PH", "PH", 5},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PR", "PR", 38},
	{"RE", "RE", 2},
	{"RO", "RO", 4},
	{"SN", "MA", 2},
	{"RS", "RS", 2},
	{"SK", "SK", 4},
	{"SI", "SI", 4},
	{"ES", "ES", 4},
	{"LK", "LK", 1},
	{"SE", "SE", 4},
	{"CH", "CH", 4},
	{"TH", "TH", 5},
	{"TT", "TT", 3},
	{"TR", "TR", 7},
	{"AE", "AE", 6},
#ifdef DHD_SUPPORT_GB_999
	{"GB", "GB", 999},
#else
	{"GB", "GB", 6},
#endif /* DHD_SUPPORT_GB_999 */
	{"UY", "VE", 3},
	{"VI", "PR", 38},
	{"VA", "VA", 2},
	{"VE", "VE", 3},
	{"VN", "VN", 4},
	{"ZM", "LA", 2},
	{"EC", "EC", 21},
	{"SV", "SV", 25},
#if defined(BCM4358_CHIP) || defined(BCM4359_CHIP)
	{"KR", "KR", 70},
#else
	{"KR", "KR", 48},
#endif
#if defined(BCM4359_CHIP)
	{"TW", "TW", 65},
	{"JP", "JP", 968},
	{"RU", "RU", 986},
	{"UA", "UA", 16},
	{"ZA", "ZA", 19},
	{"AM", "AM", 1},
	{"MY", "MY", 19},
#else
	{"TW", "TW", 1},
	{"JP", "JP", 45},
	{"RU", "RU", 13},
	{"UA", "UA", 8},
	{"ZA", "ZA", 6},
	{"MY", "MY", 3},
#endif /* BCM4359_CHIP */
	{"GT", "GT", 1},
	{"MN", "MN", 1},
	{"NI", "NI", 2},
	{"UZ", "MA", 2},
	{"EG", "EG", 13},
	{"TN", "TN", 1},
	{"AO", "AD", 0},
	{"BT", "BJ", 0},
	{"BW", "BJ", 0},
	{"LY", "LI", 4},
	{"BO", "NG", 0},
	{"UM", "PR", 38},
	/* Support FCC 15.407 (Part 15E) Changes, effective June 2 2014 */
	/* US/988, Q2/993 country codes with higher power on UNII-1 5G band */
#if defined(DHD_SUPPORT_US_949)
	{"US", "US", 949},
#elif defined(DHD_SUPPORT_US_945)
	{"US", "US", 945},
#else
	{"US", "US", 988},
#endif /* DHD_SUPPORT_US_949 */
#if defined(DHD_SUPPORT_QA_6)
	/* Support Qatar 5G band 36-64 100-144 149-165 channels
	 * This QA/6 should be used only HERO project and
	 * FW version should be sync up with 9.96.11 or higher version.
	 * If Use the old FW ver before 9.96.11 in HERO project, Country QA/6 is not supported.
	 * So when changing the country code to QA, It will be returned to fail and
	 * still previous Country code
	 */
	{"QA", "QA", 6},
#endif /* DHD_SUPPORT_QA_6 */
	{"CU", "US", 988},
	{"CA", "Q2", 993},
#endif /* default ccode/regrev */
};
#else
struct cntry_locales_custom translate_custom_table[NUM_OF_COUNTRYS];
#endif /* !DHD_USE_CLMINFO_PARSER */

/* Customized Locale convertor
*  input : ISO 3166-1 country abbreviation
*  output: customized cspec
*/
void get_customized_country_code(void *adapter, char *country_iso_code, wl_country_t *cspec)
{
	int size, i;

	size = ARRAYSIZE(translate_custom_table);

	if (cspec == 0)
		 return;

	if (size == 0)
		 return;

	for (i = 0; i < size; i++) {
		if (strcmp(country_iso_code, translate_custom_table[i].iso_abbrev) == 0) {
			memcpy(cspec->ccode,
				translate_custom_table[i].custom_locale, WLC_CNTRY_BUF_SZ);
			cspec->rev = translate_custom_table[i].custom_locale_rev;
			return;
		}
	}
	return;
}

#define PSMINFO	PLATFORM_PATH".psm.info"
#define	REVINFO	PLATFORM_PATH".rev"
#define ANTINFO PLATFORM_PATH".ant.info"
#define WIFIVERINFO	PLATFORM_PATH".wifiver.info"
#define RSDBINFO	PLATFORM_PATH".rsdb.info"
#define LOGTRACEINFO	PLATFORM_PATH".logtrace.info"
#define ADPSINFO	PLATFORM_PATH".adps.info"
#define SOFTAPINFO	PLATFORM_PATH".softap.info"

#ifdef DHD_PM_CONTROL_FROM_FILE
extern bool g_pm_control;
void sec_control_pm(dhd_pub_t *dhd, uint *power_mode)
{
	struct file *fp = NULL;
	char *filepath = PSMINFO;
	char power_val = 0;
	int ret = 0;
#ifdef DHD_ENABLE_LPC
	uint32 lpc = 0;
#endif /* DHD_ENABLE_LPC */

	g_pm_control = FALSE;
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp) || (fp == NULL)) {
		/* Enable PowerSave Mode */
		dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
			sizeof(uint), TRUE, 0);
		DHD_ERROR(("[WIFI_SEC] %s: %s doesn't exist"
			" so set PM to %d\n",
			__FUNCTION__, filepath, *power_mode));
		return;
	} else {
		kernel_read(fp, fp->f_pos, &power_val, 1);
		DHD_ERROR(("[WIFI_SEC] %s: POWER_VAL = %c \r\n", __FUNCTION__, power_val));

		if (power_val == '0') {
#ifdef ROAM_ENABLE
			uint roamvar = 1;
#endif
			uint32 wl_updown = 1;

			*power_mode = PM_OFF;
			/* Disable PowerSave Mode */
			dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
				sizeof(uint), TRUE, 0);
#ifndef CUSTOM_SET_ANTNPM
			/* Turn off MPC in AP mode */
			dhd_iovar(dhd, 0, "mpc", (char *)power_mode, sizeof(*power_mode), NULL, 0,
					TRUE);
#endif /* !CUSTOM_SET_ANTNPM */
			g_pm_control = TRUE;
#ifdef ROAM_ENABLE
			/* Roaming off of dongle */
			dhd_iovar(dhd, 0, "roam_off", (char *)&roamvar, sizeof(roamvar), NULL, 0,
					TRUE);
#endif
#ifdef DHD_ENABLE_LPC
			/* Set lpc 0 */
			ret = dhd_iovar(dhd, 0, "lpc", (char *)&lpc, sizeof(lpc), NULL, 0, TRUE);
			if (ret < 0) {
				DHD_ERROR(("[WIFI_SEC] %s: Set lpc failed  %d\n",
				__FUNCTION__, ret));
			}
#endif /* DHD_ENABLE_LPC */
#ifdef DHD_PCIE_RUNTIMEPM
			DHD_ERROR(("[WIFI_SEC] %s : Turn Runtime PM off \n", __FUNCTION__));
			/* Turn Runtime PM off */
			dhdpcie_block_runtime_pm(dhd);
#endif /* DHD_PCIE_RUNTIMEPM */
			/* Disable ocl */
			if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_UP, (char *)&wl_updown,
					sizeof(wl_updown), TRUE, 0)) < 0) {
				DHD_ERROR(("[WIFI_SEC] %s: WLC_UP faield %d\n", __FUNCTION__, ret));
			}

#ifndef CUSTOM_SET_OCLOFF
			{
				uint32 ocl_enable = 0;
				ret = dhd_iovar(dhd, 0, "ocl_enable", (char *)&ocl_enable,
						sizeof(ocl_enable), NULL, 0, TRUE);
				if (ret < 0) {
					DHD_ERROR(("[WIFI_SEC] %s: Set ocl_enable %d failed %d\n",
						__FUNCTION__, ocl_enable, ret));
				} else {
					DHD_ERROR(("[WIFI_SEC] %s: Set ocl_enable %d OK %d\n",
						__FUNCTION__, ocl_enable, ret));
				}
			}
#else
			dhd->ocl_off = TRUE;
#endif /* CUSTOM_SET_OCLOFF */
#ifdef WLADPS
			if ((ret = dhd_enable_adps(dhd, ADPS_DISABLE)) < 0) {
				DHD_ERROR(("[WIFI_SEC] %s: dhd_enable_adps failed %d\n",
						__FUNCTION__, ret));
			}
#ifdef WLADPS_SEAK_AP_WAR
			dhd->disabled_adps = TRUE;
#endif /* WLADPS_SEAK_AP_WAR */
#endif /* WLADPS */

			if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_DOWN, (char *)&wl_updown,
					sizeof(wl_updown), TRUE, 0)) < 0) {
				DHD_ERROR(("[WIFI_SEC] %s: WLC_DOWN faield %d\n",
						__FUNCTION__, ret));
			}
		} else {
			dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
				sizeof(uint), TRUE, 0);
		}
	}

	if (fp)
		filp_close(fp, NULL);
}
#endif /* DHD_PM_CONTROL_FROM_FILE */

#ifdef MIMO_ANT_SETTING
int dhd_sel_ant_from_file(dhd_pub_t *dhd)
{
	struct file *fp = NULL;
	int ret = -1;
	uint32 ant_val = 0;
	uint32 btc_mode = 0;
#ifndef CUSTOM_SET_ANTNPM
	wl_config_t rsdb_mode;
#endif /* !CUSTOM_SET_ANTNPM */
	char *filepath = ANTINFO;
	uint chip_id = dhd_bus_chip_id(dhd);
#ifndef CUSTOM_SET_ANTNPM
	 memset(&rsdb_mode, 0, sizeof(rsdb_mode));
#endif /* CUSTOM_SET_ANTNPM */
	/* Check if this chip can support MIMO */
	if (chip_id != BCM4324_CHIP_ID &&
		chip_id != BCM4350_CHIP_ID &&
		chip_id != BCM4354_CHIP_ID &&
		chip_id != BCM43569_CHIP_ID &&
		chip_id != BCM4358_CHIP_ID &&
		chip_id != BCM4359_CHIP_ID &&
		chip_id != BCM4355_CHIP_ID &&
		chip_id != BCM4347_CHIP_ID &&
		chip_id != BCM4361_CHIP_ID) {
		DHD_ERROR(("[WIFI_SEC] %s: This chipset does not support MIMO\n",
			__FUNCTION__));
		return ret;
	}

	/* Read antenna settings from the file */
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist\n", __FUNCTION__, filepath));
#ifdef CUSTOM_SET_ANTNPM
		dhd->mimo_ant_set = 0;
#endif /* !CUSTOM_SET_ANTNPM */
		return ret;
	} else {
		ret = kernel_read(fp, 0, (char *)&ant_val, 4);
		if (ret < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: File read error, ret=%d\n", __FUNCTION__, ret));
			filp_close(fp, NULL);
			return ret;
		}

		ant_val = bcm_atoi((char *)&ant_val);

		DHD_ERROR(("[WIFI_SEC]%s: ANT val = %d\n", __FUNCTION__, ant_val));
		filp_close(fp, NULL);

		/* Check value from the file */
		if (ant_val < 1 || ant_val > 3) {
			DHD_ERROR(("[WIFI_SEC] %s: Invalid value %d read from the file %s\n",
				__FUNCTION__, ant_val, filepath));
			return -1;
		}
	}

	/* bt coex mode off */
	if (dhd_get_fw_mode(dhd->info) == DHD_FLAG_MFG_MODE) {
		ret = dhd_iovar(dhd, 0, "btc_mode", (char *)&btc_mode, sizeof(btc_mode), NULL, 0,
				TRUE);
		if (ret) {
			DHD_ERROR(("[WIFI_SEC] %s: Fail to execute dhd_wl_ioctl_cmd(): "
				"btc_mode, ret=%d\n",
				__FUNCTION__, ret));
			return ret;
		}
	}

#ifndef CUSTOM_SET_ANTNPM
	/* rsdb mode off */
	DHD_ERROR(("[WIFI_SEC] %s: %s the RSDB mode!\n",
		__FUNCTION__, rsdb_mode.config ? "Enable" : "Disable"));
	ret = dhd_iovar(dhd, 0, "rsdb_mode", (char *)&rsdb_mode, sizeof(rsdb_mode), NULL, 0, TRUE);
	if (ret) {
		DHD_ERROR(("[WIFI_SEC] %s: Fail to execute dhd_wl_ioctl_cmd(): "
			"rsdb_mode, ret=%d\n", __FUNCTION__, ret));
		return ret;
	}

	/* Select Antenna */
	ret = dhd_iovar(dhd, 0, "txchain", (char *)&ant_val, sizeof(ant_val), NULL, 0, TRUE);
	if (ret) {
		DHD_ERROR(("[WIFI_SEC] %s: Fail to execute dhd_wl_ioctl_cmd(): txchain, ret=%d\n",
			__FUNCTION__, ret));
		return ret;
	}

	ret = dhd_iovar(dhd, 0, "rxchain", (char *)&ant_val, sizeof(ant_val), NULL, 0, TRUE);
	if (ret) {
		DHD_ERROR(("[WIFI_SEC] %s: Fail to execute dhd_wl_ioctl_cmd(): rxchain, ret=%d\n",
			__FUNCTION__, ret));
		return ret;
	}
#else
	dhd->mimo_ant_set = ant_val;
	DHD_ERROR(("[WIFI_SEC] %s: mimo_ant_set = %d\n", __FUNCTION__, dhd->mimo_ant_set));
#endif /* !CUSTOM_SET_ANTNPM */
	return 0;
}
#endif /* MIMO_ANTENNA_SETTING */

#ifdef RSDB_MODE_FROM_FILE
/*
 * RSDBOFFINFO = .rsdb.info
 *  - rsdb_mode = 1            => Don't change RSDB mode / RSDB stay as turn on
 *  - rsdb_mode = 0            => Trun Off RSDB mode
 *  - file not exist          => Don't change RSDB mode / RSDB stay as turn on
 */
int dhd_rsdb_mode_from_file(dhd_pub_t *dhd)
{
	struct file *fp = NULL;
	int ret = -1;
	wl_config_t  rsdb_mode;
	uint32 rsdb_configuration = 0;
	char *filepath = RSDBINFO;

	memset(&rsdb_mode, 0, sizeof(rsdb_mode));

	/* Read RSDB on/off request from the file */
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist\n", __FUNCTION__, filepath));
		return ret;
	} else {
		ret = kernel_read(fp, 0, (char *)&rsdb_configuration, 4);
		if (ret < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: File read error, ret=%d\n", __FUNCTION__, ret));
			filp_close(fp, NULL);
			return ret;
		}

		rsdb_mode.config = bcm_atoi((char *)&rsdb_configuration);
		DHD_ERROR(("[WIFI_SEC] %s: RSDB mode from file = %d\n",
			__FUNCTION__, rsdb_mode.config));

		filp_close(fp, NULL);

		/* Check value from the file */
		if (rsdb_mode.config > 2) {
			DHD_ERROR(("[WIFI_SEC] %s: Invalid value %d read from the file %s\n",
				__FUNCTION__, rsdb_mode.config, filepath));
			return -1;
		}
	}

	if (rsdb_mode.config == 0) {
		ret = dhd_iovar(dhd, 0, "rsdb_mode", (char *)&rsdb_mode, sizeof(rsdb_mode), NULL, 0,
				TRUE);
		if (ret < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: rsdb_mode ret= %d\n", __FUNCTION__, ret));
		} else {
			DHD_ERROR(("[WIFI_SEC] %s: rsdb_mode to MIMO(RSDB OFF) succeeded\n",
				__FUNCTION__));
		}
	}

	return ret;
}
#endif /* RSDB_MODE_FROM_FILE */

#ifdef LOGTRACE_FROM_FILE
/*
 * LOGTRACEINFO = .logtrace.info
 *  - logtrace = 1            => Enable LOGTRACE Event
 *  - logtrace = 0            => Disable LOGTRACE Event
 *  - file not exist          => Disable LOGTRACE Event
 */
int dhd_logtrace_from_file(dhd_pub_t *dhd)
{
	struct file *fp = NULL;
	int ret = -1;
	uint32 logtrace = 0;
	char *filepath = LOGTRACEINFO;

	/* Read LOGTRACE Event on/off request from the file */
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist\n", __FUNCTION__, filepath));
		return 0;
	} else {
		ret = kernel_read(fp, 0, (char *)&logtrace, 4);
		if (ret < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: File read error, ret=%d\n", __FUNCTION__, ret));
			filp_close(fp, NULL);
			return 0;
		}

		logtrace = bcm_atoi((char *)&logtrace);

		DHD_ERROR(("[WIFI_SEC] %s: LOGTRACE On/Off from file = %d\n",
			__FUNCTION__, logtrace));
		filp_close(fp, NULL);

		/* Check value from the file */
		if (logtrace > 2) {
			DHD_ERROR(("[WIFI_SEC] %s: Invalid value %d read from the file %s\n",
				__FUNCTION__, logtrace, filepath));
			return 0;
		}
	}

	return (int)logtrace;
}
#endif /* LOGTRACE_FROM_FILE */

#ifdef USE_WFA_CERT_CONF
int sec_get_param_wfa_cert(dhd_pub_t *dhd, int mode, uint* read_val)
{
	struct file *fp = NULL;
	char *filepath = NULL;
	int val = 0;

	if (!dhd || (mode < SET_PARAM_BUS_TXGLOM_MODE) ||
		(mode >= PARAM_LAST_VALUE)) {
		DHD_ERROR(("[WIFI_SEC] %s: invalid argument\n", __FUNCTION__));
		return BCME_ERROR;
	}

	switch (mode) {
		case SET_PARAM_BUS_TXGLOM_MODE:
			filepath = PLATFORM_PATH".bustxglom.info";
			break;
		case SET_PARAM_ROAMOFF:
			filepath = PLATFORM_PATH".roamoff.info";
			break;
#ifdef USE_WL_FRAMEBURST
		case SET_PARAM_FRAMEBURST:
			filepath = PLATFORM_PATH".frameburst.info";
			break;
#endif /* USE_WL_FRAMEBURST */
#ifdef USE_WL_TXBF
		case SET_PARAM_TXBF:
			filepath = PLATFORM_PATH".txbf.info";
			break;
#endif /* USE_WL_TXBF */
#ifdef PROP_TXSTATUS
		case SET_PARAM_PROPTX:
			filepath = PLATFORM_PATH".proptx.info";
			break;
#endif /* PROP_TXSTATUS */
		default:
			DHD_ERROR(("[WIFI_SEC] %s: File to find file name for index=%d\n",
				__FUNCTION__, mode));
			return BCME_ERROR;
	}
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp) || (fp == NULL)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist\n",
			__FUNCTION__, filepath));
		return BCME_ERROR;
	} else {
		if (kernel_read(fp, fp->f_pos, (char *)&val, 4) < 0) {
			filp_close(fp, NULL);
			/* File operation is failed so we will return error code */
			DHD_ERROR(("[WIFI_SEC] %s: read failed, file path=%s\n",
				__FUNCTION__, filepath));
			return BCME_ERROR;
		}
		filp_close(fp, NULL);
	}

	val = bcm_atoi((char *)&val);

	switch (mode) {
		case SET_PARAM_ROAMOFF:
#ifdef USE_WL_FRAMEBURST
		case SET_PARAM_FRAMEBURST:
#endif /* USE_WL_FRAMEBURST */
#ifdef USE_WL_TXBF
		case SET_PARAM_TXBF:
#endif /* USE_WL_TXBF */
#ifdef PROP_TXSTATUS
		case SET_PARAM_PROPTX:
#endif /* PROP_TXSTATUS */
		if (val < 0 || val > 1) {
			DHD_ERROR(("[WIFI_SEC] %s: value[%d] is out of range\n",
				__FUNCTION__, *read_val));
			return BCME_ERROR;
		}
			break;
		default:
			return BCME_ERROR;
	}
	*read_val = (uint)val;
	return BCME_OK;
}
#endif /* USE_WFA_CERT_CONF */

#ifdef WRITE_WLANINFO
#define FIRM_PREFIX "Firm_ver:"
#define DHD_PREFIX "DHD_ver:"
#define NV_PREFIX "Nv_info:"
#define CLM_PREFIX "CLM_ver:"
#define max_len(a, b) ((sizeof(a)/(2)) - (strlen(b)) - (3))
#define tstr_len(a, b) ((strlen(a)) + (strlen(b)) + (3))

char version_info[512];
char version_old_info[512];

int write_filesystem(struct file *file, unsigned long long offset,
	unsigned char* data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}

uint32 sec_save_wlinfo(char *firm_ver, char *dhd_ver, char *nvram_p, char *clm_ver)
{
	struct file *fp = NULL;
	struct file *nvfp = NULL;
	char *filepath = WIFIVERINFO;
	int min_len, str_len = 0;
	int ret = 0;
	char* nvram_buf;
	char temp_buf[256];

	DHD_TRACE(("[WIFI_SEC] %s: Entered.\n", __FUNCTION__));

	DHD_INFO(("[WIFI_SEC] firmware version   : %s\n", firm_ver));
	DHD_INFO(("[WIFI_SEC] dhd driver version : %s\n", dhd_ver));
	DHD_INFO(("[WIFI_SEC] nvram path : %s\n", nvram_p));
	DHD_INFO(("[WIFI_SEC] clm version : %s\n", clm_ver));

	memset(version_info, 0, sizeof(version_info));

	if (strlen(dhd_ver)) {
		min_len = min(strlen(dhd_ver), max_len(temp_buf, DHD_PREFIX));
		min_len += strlen(DHD_PREFIX) + 3;
		DHD_INFO(("[WIFI_SEC] DHD ver length : %d\n", min_len));
		snprintf(version_info+str_len, min_len, DHD_PREFIX " %s\n", dhd_ver);
		str_len = strlen(version_info);

		DHD_INFO(("[WIFI_SEC] Driver version_info len : %d\n", str_len));
		DHD_INFO(("[WIFI_SEC] Driver version_info : %s\n", version_info));
	} else {
		DHD_ERROR(("[WIFI_SEC] Driver version is missing.\n"));
	}

	if (strlen(firm_ver)) {
		min_len = min(strlen(firm_ver), max_len(temp_buf, FIRM_PREFIX));
		min_len += strlen(FIRM_PREFIX) + 3;
		DHD_INFO(("[WIFI_SEC] firmware ver length : %d\n", min_len));
		snprintf(version_info+str_len, min_len, FIRM_PREFIX " %s\n", firm_ver);
		str_len = strlen(version_info);

		DHD_INFO(("[WIFI_SEC] Firmware version_info len : %d\n", str_len));
		DHD_INFO(("[WIFI_SEC] Firmware version_info : %s\n", version_info));
	} else {
		DHD_ERROR(("[WIFI_SEC] Firmware version is missing.\n"));
	}

	if (nvram_p) {
		memset(temp_buf, 0, sizeof(temp_buf));
		nvfp = filp_open(nvram_p, O_RDONLY, 0);
		if (IS_ERR(nvfp) || (nvfp == NULL)) {
			DHD_ERROR(("[WIFI_SEC] %s: Nvarm File open failed.\n", __FUNCTION__));
			return -1;
		} else {
			ret = kernel_read(nvfp, nvfp->f_pos, temp_buf, sizeof(temp_buf));
			filp_close(nvfp, NULL);
		}

		if (strlen(temp_buf)) {
			nvram_buf = temp_buf;
			bcmstrtok(&nvram_buf, "\n", 0);
			DHD_INFO(("[WIFI_SEC] nvram tolkening : %s(%zu) \n",
				temp_buf, strlen(temp_buf)));
			snprintf(version_info+str_len, tstr_len(temp_buf, NV_PREFIX),
				NV_PREFIX " %s\n", temp_buf);
			str_len = strlen(version_info);
			DHD_INFO(("[WIFI_SEC] NVRAM version_info : %s\n", version_info));
			DHD_INFO(("[WIFI_SEC] NVRAM version_info len : %d, nvram len : %zu\n",
				str_len, strlen(temp_buf)));
		} else {
			DHD_ERROR(("[WIFI_SEC] NVRAM info is missing.\n"));
		}
	} else {
		DHD_ERROR(("[WIFI_SEC] Not exist nvram path\n"));
	}

	if (strlen(clm_ver)) {
		 min_len = min(strlen(clm_ver), max_len(temp_buf, CLM_PREFIX));
		 min_len += strlen(CLM_PREFIX) + 3;
		 DHD_INFO(("[WIFI_SEC] clm ver length : %d\n", min_len));
		 snprintf(version_info+str_len, min_len, CLM_PREFIX " %s\n", clm_ver);
		 str_len = strlen(version_info);

		 DHD_INFO(("[WIFI_SEC] CLM version_info len : %d\n", str_len));
		 DHD_INFO(("[WIFI_SEC] CLM version_info : %s\n", version_info));
	 } else {
		 DHD_ERROR(("[WIFI_SEC] CLM version is missing.\n"));
	 }

	DHD_INFO(("[WIFI_SEC] version_info : %s, strlen : %zu\n",
		version_info, strlen(version_info)));

	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp) || (fp == NULL)) {
		DHD_ERROR(("[WIFI_SEC] %s: .wifiver.info File open failed.\n", __FUNCTION__));
	} else {
		memset(version_old_info, 0, sizeof(version_old_info));
		ret = kernel_read(fp, fp->f_pos, version_old_info, sizeof(version_info));
		filp_close(fp, NULL);
		DHD_INFO(("[WIFI_SEC] kernel_read ret : %d.\n", ret));
		if (strcmp(version_info, version_old_info) == 0) {
			DHD_ERROR(("[WIFI_SEC] .wifiver.info already saved.\n"));
			return 0;
		}
	}

	fp = filp_open(filepath, O_RDWR | O_CREAT, 0664);
	if (IS_ERR(fp) || (fp == NULL)) {
		DHD_ERROR(("[WIFI_SEC] %s: .wifiver.info File open failed.\n",
			__FUNCTION__));
	} else {
		ret = write_filesystem(fp, fp->f_pos, version_info, sizeof(version_info));
		DHD_INFO(("[WIFI_SEC] sec_save_wlinfo done. ret : %d\n", ret));
		DHD_ERROR(("[WIFI_SEC] save .wifiver.info file.\n"));
		filp_close(fp, NULL);
	}
	return ret;
}
#endif /* WRITE_WLANINFO */

#ifdef SUPPORT_MULTIPLE_BOARD_REV_FROM_HW
unsigned int system_hw_rev;
static int
__init get_hw_rev(char *arg)
{
	get_option(&arg, &system_hw_rev);
	printk("dhd : hw_rev : %d\n", system_hw_rev);
	return 0;
}

early_param("androidboot.hw_rev", get_hw_rev);
#endif /* SUPPORT_MULTIPLE_BOARD_REV_FROM_HW */
#endif /* CUSTOMER_HW4 || CUSTOMER_HW40 */

#if defined(FORCE_DISABLE_SINGLECORE_SCAN)
void
dhd_force_disable_singlcore_scan(dhd_pub_t *dhd)
{
	int ret = 0;
	struct file *fp = NULL;
	char *filepath = PLATFORM_PATH".cid.info";
	s8 iovbuf[WL_EVENTING_MASK_LEN + 12];
	char vender[10] = {0, };
	uint32 pm_bcnrx = 0;
	uint32 scan_ps = 0;

	if (BCM4354_CHIP_ID != dhd_bus_chip_id(dhd))
		return;

	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("%s file open error\n", filepath));
	} else {
		ret = kernel_read(fp, 0, (char *)vender, 5);

		if (ret > 0 && NULL != strstr(vender, "wisol")) {
			DHD_ERROR(("wisol module : set pm_bcnrx=0, set scan_ps=0\n"));

			bcm_mkiovar("pm_bcnrx", (char *)&pm_bcnrx, 4, iovbuf, sizeof(iovbuf));
			ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
			if (ret < 0)
				DHD_ERROR(("Set pm_bcnrx error (%d)\n", ret));

			bcm_mkiovar("scan_ps", (char *)&scan_ps, 4, iovbuf, sizeof(iovbuf));
			ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
			if (ret < 0)
				DHD_ERROR(("Set scan_ps error (%d)\n", ret));
		}
		filp_close(fp, NULL);
	}
}
#endif /* FORCE_DISABLE_SINGLECORE_SCAN */

#if defined(ARGOS_CPU_SCHEDULER) && defined(CONFIG_SCHED_HMP) && \
	!defined(DHD_LB_IRQSET)
void
set_irq_cpucore(unsigned int irq, cpumask_var_t default_cpu_mask,
	cpumask_var_t affinity_cpu_mask)
{
	argos_irq_affinity_setup_label(irq,
		ARGOS_IRQ_WIFI_TABLE_LABEL,
		affinity_cpu_mask, default_cpu_mask);

	argos_irq_affinity_setup_label(irq,
		ARGOS_P2P_TABLE_LABEL,
		affinity_cpu_mask, default_cpu_mask);
}
#elif defined(SET_PCIE_IRQ_CPU_CORE)
void
set_irq_cpucore(unsigned int irq, int set)
{
	if (set < 0 || set > 1) {
		DHD_ERROR(("%s, PCIe CPU core set error\n", __FUNCTION__));
		return;
	}

	if (set) {
		DHD_ERROR(("%s, PCIe IRQ:%u set Core %d\n",
			__FUNCTION__, irq, PCIE_IRQ_BIG_CORE));
		irq_set_affinity(irq, cpumask_of(PCIE_IRQ_BIG_CORE));
	} else {
		DHD_ERROR(("%s, PCIe IRQ:%u set Core %d\n",
			__FUNCTION__, irq, PCIE_IRQ_LITTLE_CORE));
		irq_set_affinity(irq, cpumask_of(PCIE_IRQ_LITTLE_CORE));
	}
}
#else
void
set_irq_cpucore(void)
{
	DHD_ERROR(("Unsupported IRQ affinity\n"));
}
#endif /* SET_PCIE_IRQ_CPU_CORE */

#ifdef ADPS_MODE_FROM_FILE
/*
 * ADPSINFO = .asdp.info
 *  - adps mode = 1 => Enable ADPS mode
 *  - adps mode = 0 => Disalbe ADPS mode
 *  - file not exit => Enable ADPS mode
 */
void dhd_adps_mode_from_file(dhd_pub_t *dhd)
{
	struct file *fp = NULL;
	int ret = 0;
	uint32 adps_mode = 0;
	char *filepath = ADPSINFO;

	/* Read ASDP on/off request from the file */
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist\n", __FUNCTION__, filepath));
		if ((ret = dhd_enable_adps(dhd, ADPS_ENABLE)) < 0) {
			DHD_ERROR(("%s dhd_enable_adps failed %d\n", __FUNCTION__, ret));
		}
		return;
	} else {
		ret = kernel_read(fp, 0, (char *)&adps_mode, 4);
		if (ret < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: File read error, ret=%d\n", __FUNCTION__, ret));
			filp_close(fp, NULL);
			return;
		}

		adps_mode = bcm_atoi((char *)&adps_mode);

		DHD_ERROR(("[WIFI_SEC] %s: ASDP mode from file = %d\n", __FUNCTION__, adps_mode));
		filp_close(fp, NULL);

		/* Check value from the file */
		if (adps_mode > 2) {
			DHD_ERROR(("[WIFI_SEC] %s: Invalid value %d read from the file %s\n",
				__FUNCTION__, adps_mode, filepath));
			return;
		}
	}

	if (adps_mode < 2) {
		adps_mode = adps_mode == OFF ? ADPS_DISABLE : ADPS_ENABLE;

		if ((ret = dhd_enable_adps(dhd, adps_mode)) < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: dhd_enable_adps failed %d\n",
					__FUNCTION__, ret));
			return;
		}
#ifdef WLADPS_SEAK_AP_WAR
		if (adps_mode == ADPS_DISABLE) {
			dhd->disabled_adps = TRUE;
		}
#endif /* WLADPS_SEAK_AP_WAR */
	}

	return;
}
#endif /* ADPS_MODE_FROM_FILE */

#ifdef GEN_SOFTAP_INFO_FILE
#define SOFTAP_INFO_FILE_FIRST_LINE	"#.softap.info"
#define SOFTAP_INFO_BUF_SZ	512
/*
 * # Whether both wifi and hotspot can be turned on at the same time?
 * DualBandConcurrency
 * # 5Ghz band support?
 * 5G
 * # How many clients can be connected?
 * maxClient
 * # Does hotspot support PowerSave mode?
 * PowerSave
 * # Does android_net_wifi_set_Country_Code_Hal feature supported?
 * HalFn_setCountryCodeHal
 * # Does android_net_wifi_getValidChannels supported?
 * HalFn_getValidChannels
 */
const char *softap_info_items[] = {
	"DualBandConcurrency", "5G", "maxClient", "PowerSave",
	"HalFn_setCountryCodeHal", "HalFn_getValidChannels", NULL
};
#if defined(BCM4361_CHIP)
const char *softap_info_values[] = {
	"yes", "yes", "10", "yes", "yes", "yes", NULL
};
#elif defined(BCM43454_CHIP) || defined(BCM43455_CHIP) || defined(BCM43456_CHIP)
const char *softap_info_values[] = {
#ifdef WL_RESTRICTED_APSTA_SCC
	"yes", "yes", "10", "no", "yes", "yes", NULL
#else
	"no", "yes", "10", "no", "yes", "yes", NULL
#endif /* WL_RESTRICTED_APSTA_SCC */
};
#elif defined(BCM43430_CHIP)
const char *softap_info_values[] = {
	"no", "no", "10", "no", "yes", "yes", NULL
};
#else
const char *softap_info_values[] = {
	"UNDEF", "UNDEF", "UNDEF", "UNDEF", "UNDEF", "UNDEF", NULL
};
#endif /* defined(BCM4361_CHIP) */
#endif /* GEN_SOFTAP_INFO_FILE */

#ifdef GEN_SOFTAP_INFO_FILE
uint32 sec_save_softap_info(void)
{
	struct file *fp = NULL;
	char *filepath = SOFTAPINFO;
	char temp_buf[SOFTAP_INFO_BUF_SZ];
	int ret = -1, idx = 0, rem = 0, written = 0;
	char *pos = NULL;

	DHD_TRACE(("[WIFI_SEC] %s: Entered.\n", __FUNCTION__));
	memset(temp_buf, 0, sizeof(temp_buf));

	pos = temp_buf;
	rem = sizeof(temp_buf);
	written = snprintf(pos, sizeof(temp_buf), "%s\n",
		SOFTAP_INFO_FILE_FIRST_LINE);
	do {
		int len = strlen(softap_info_items[idx]) +
			strlen(softap_info_values[idx]) + 2;
		pos += written;
		rem -= written;
		if (len > rem) {
			break;
		}
		written = snprintf(pos, rem, "%s=%s\n",
			softap_info_items[idx], softap_info_values[idx]);
	} while (softap_info_items[++idx] != NULL);

	fp = filp_open(filepath, O_RDWR | O_CREAT, 0664);
	if (IS_ERR(fp) || (fp == NULL)) {
		DHD_ERROR(("[WIFI_SEC] %s: %s File open failed.\n",
			SOFTAPINFO, __FUNCTION__));
	} else {
		ret = write_filesystem(fp, fp->f_pos, temp_buf, strlen(temp_buf));
		DHD_INFO(("[WIFI_SEC] %s done. ret : %d\n", __FUNCTION__, ret));
		DHD_ERROR(("[WIFI_SEC] save %s file.\n", SOFTAPINFO));
		filp_close(fp, NULL);
	}
	return ret;
}
#endif /* GEN_SOFTAP_INFO_FILE */
