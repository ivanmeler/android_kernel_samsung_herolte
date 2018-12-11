/*
 * Bad AP Manager for ADPS
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
 * $Id: wl_bam.h 763128 2018-05-17 08:38:35Z $
 */
#ifndef _WL_BAM_H_
#define _WL_BAM_H_
#include <typedefs.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>

#include <wl_cfgp2p.h>

typedef struct wl_bad_ap_mngr {
	uint32 num;
	spinlock_t lock;
	struct mutex fs_lock;		/* lock for bad ap file list */
	struct list_head list;

	wl_event_adps_bad_ap_t data;
} wl_bad_ap_mngr_t;

void wl_bad_ap_mngr_init(struct bcm_cfg80211 *cfg);
void wl_bad_ap_mngr_deinit(struct bcm_cfg80211 *cfg);

bool wl_adps_bad_ap_check(struct bcm_cfg80211 *cfg, const struct ether_addr *bssid);
int wl_adps_enabled(struct bcm_cfg80211 *cfg, struct net_device *ndev);
int wl_adps_set_suspend(struct bcm_cfg80211 *cfg, struct net_device *ndev, uint8 suspend);

s32 wl_adps_event_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data);
#endif  /* _WL_BAM_H_ */
