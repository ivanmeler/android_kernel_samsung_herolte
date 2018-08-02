/*
 * Linux cfg80211 Vendor Extension Code
 *
 * Copyright (C) 1999-2017, Broadcom Corporation
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
 * $Id: wl_cfgvendor.c 455257 2014-02-20 08:10:24Z $
*/

/*
 * New vendor interface additon to nl80211/cfg80211 to allow vendors
 * to implement proprietary features over the cfg80211 stack.
*/

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <linux/kernel.h>

#include <bcmutils.h>
#include <bcmwifi_channels.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/802.11.h>
#include <linux/if_arp.h>
#include <asm/uaccess.h>

#include <dngl_stats.h>
#include <dhd.h>
#include <dhdioctl.h>
#include <wlioctl.h>
#include <dhd_cfg80211.h>
#ifdef PNO_SUPPORT
#include <dhd_pno.h>
#endif /* PNO_SUPPORT */
#ifdef RTT_SUPPORT
#include <dhd_rtt.h>
#endif /* RTT_SUPPORT */
#include <proto/ethernet.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <linux/wait.h>
#include <net/cfg80211.h>
#include <net/rtnetlink.h>

#include <wlioctl.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>
#include <wl_cfgp2p.h>
#include <wl_android.h>
#include <wl_cfgvendor.h>
#ifdef PROP_TXSTATUS
#include <dhd_wlfc.h>
#endif
#include <brcm_nl80211.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 13, 0)) || defined(WL_VENDOR_EXT_SUPPORT)
/*
 * This API is to be used for asynchronous vendor events. This
 * shouldn't be used in response to a vendor command from its
 * do_it handler context (instead wl_cfgvendor_send_cmd_reply should
 * be used).
 */
int
wl_cfgvendor_send_async_event(struct wiphy *wiphy,
	struct net_device *dev, int event_id, const void  *data, int len)
{
	u16 kflags;
	struct sk_buff *skb;

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	skb = cfg80211_vendor_event_alloc(wiphy, NULL, len, event_id, kflags);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, len, event_id, kflags);
#endif /* CONFIG_ARCH_MSM && SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC ||
		* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
		*/
	if (!skb) {
		WL_ERR(("skb alloc failed"));
		return -ENOMEM;
	}

	/* Push the data to the skb */
	nla_put_nohdr(skb, len, data);

	cfg80211_vendor_event(skb, kflags);

	return 0;
}

static int
wl_cfgvendor_send_cmd_reply(struct wiphy *wiphy,
	struct net_device *dev, const void  *data, int len)
{
	struct sk_buff *skb;

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, len);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		return -ENOMEM;
	}

	/* Push the data to the skb */
	nla_put_nohdr(skb, len, data);

	return cfg80211_vendor_cmd_reply(skb);
}

#ifdef RTT_SUPPORT
void
wl_cfgvendor_rtt_evt(void *ctx, void *rtt_data)
{
	struct wireless_dev *wdev = (struct wireless_dev *)ctx;
	struct wiphy *wiphy;
	struct sk_buff *skb;
	uint32 tot_len = NLMSG_DEFAULT_SIZE, entry_len = 0;
	gfp_t kflags;
	rtt_report_t *rtt_report = NULL;
	rtt_result_t *rtt_result = NULL;
	struct list_head *rtt_list;
	wiphy = wdev->wiphy;

	WL_DBG(("In\n"));
	/* Push the data to the skb */
	if (!rtt_data) {
		WL_ERR(("rtt_data is NULL\n"));
		goto exit;
	}
	rtt_list = (struct list_head *)rtt_data;
	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	skb = cfg80211_vendor_event_alloc(wiphy, NULL, tot_len, GOOGLE_RTT_COMPLETE_EVENT, kflags);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, tot_len, GOOGLE_RTT_COMPLETE_EVENT, kflags);
#endif /* CONFIG_ARCH_MSM && SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC ||
		* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
		*/
	if (!skb) {
		WL_ERR(("skb alloc failed"));
		goto exit;
	}
	/* fill in the rtt results on each entry */
	list_for_each_entry(rtt_result, rtt_list, list) {
		entry_len = 0;
		entry_len = sizeof(rtt_report_t);
		rtt_report = kzalloc(entry_len, kflags);
		if (!rtt_report) {
			WL_ERR(("rtt_report alloc failed"));
			kfree_skb(skb);
			goto exit;
		}
		rtt_report->addr = rtt_result->peer_mac;
		rtt_report->num_measurement = 1; /* ONE SHOT */
		rtt_report->status = rtt_result->err_code;
		rtt_report->type =
			(rtt_result->TOF_type == TOF_TYPE_ONE_WAY) ? RTT_ONE_WAY: RTT_TWO_WAY;
		rtt_report->peer = rtt_result->target_info->peer;
		rtt_report->channel = rtt_result->target_info->channel;
		rtt_report->rssi = rtt_result->avg_rssi;
		/* tx_rate */
		rtt_report->tx_rate = rtt_result->tx_rate;
		/* RTT */
		rtt_report->rtt = rtt_result->meanrtt;
		rtt_report->rtt_sd = rtt_result->sdrtt/10;
		/* convert to centi meter */
		if (rtt_result->distance != 0xffffffff)
			rtt_report->distance = (rtt_result->distance >> 2) * 25;
		else /* invalid distance */
			rtt_report->distance = -1;
		rtt_report->ts = rtt_result->ts;
		nla_append(skb, entry_len, rtt_report);
		kfree(rtt_report);
	}
	cfg80211_vendor_event(skb, kflags);
exit:
	return;
}

static int
wl_cfgvendor_rtt_set_config(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len) {
	int err = 0, rem, rem1, rem2, type;
	rtt_config_params_t rtt_param;
	rtt_target_info_t* rtt_target = NULL;
	const struct nlattr *iter, *iter1, *iter2;
	int8 eabuf[ETHER_ADDR_STR_LEN];
	int8 chanbuf[CHANSPEC_STR_LEN];
	int32 feature_set = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	feature_set = dhd_dev_get_feature_set(bcmcfg_to_prmry_ndev(cfg));

	WL_DBG(("In\n"));
	err = dhd_dev_rtt_register_noti_callback(wdev->netdev, wdev, wl_cfgvendor_rtt_evt);
	if (err < 0) {
		WL_ERR(("failed to register rtt_noti_callback\n"));
		goto exit;
	}
	memset(&rtt_param, 0, sizeof(rtt_param));
	if (len <= 0) {
		WL_ERR(("Length of the nlattr is not valid len : %d\n", len));
		err = BCME_ERROR;
		goto exit;
	}
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
		case RTT_ATTRIBUTE_TARGET_CNT:
			rtt_param.rtt_target_cnt = nla_get_u8(iter);
			if ((rtt_param.rtt_target_cnt <= 0) ||
				(rtt_param.rtt_target_cnt > RTT_MAX_TARGET_CNT)) {
				WL_ERR(("target_cnt is not valid : %d\n",
					rtt_param.rtt_target_cnt));
				err = BCME_RANGE;
				goto exit;
			}
			break;
		case RTT_ATTRIBUTE_TARGET_INFO:
			rtt_target = rtt_param.target_info;
			nla_for_each_nested(iter1, iter, rem1) {
				nla_for_each_nested(iter2, iter1, rem2) {
					type = nla_type(iter2);
					switch (type) {
					case RTT_ATTRIBUTE_TARGET_MAC:
						memcpy(&rtt_target->addr, nla_data(iter2),
							ETHER_ADDR_LEN);
						break;
					case RTT_ATTRIBUTE_TARGET_TYPE:
						rtt_target->type = nla_get_u8(iter2);
						if (!(feature_set & WIFI_FEATURE_D2D_RTT)) {
							if (rtt_target->type == RTT_TWO_WAY ||
								rtt_target->type == RTT_INVALID) {
								WL_ERR(("doesn't support RTT type"
									" : %d\n",
									rtt_target->type));
								err = -EINVAL;
								goto exit;
							} else if (rtt_target->type == RTT_AUTO) {
								rtt_target->type = RTT_ONE_WAY;
							}
						} else if (rtt_target->type == RTT_INVALID) {
							WL_ERR(("doesn't support RTT type"
								" : %d\n",
								rtt_target->type));
							err = -EINVAL;
							goto exit;
						}
						break;
					case RTT_ATTRIBUTE_TARGET_PEER:
						rtt_target->peer = nla_get_u8(iter2);
						if (rtt_target->peer != RTT_PEER_AP) {
							WL_ERR(("doesn't support peer type : %d\n",
								rtt_target->peer));
							err = -EINVAL;
							goto exit;
						}
						break;
					case RTT_ATTRIBUTE_TARGET_CHAN:
						memcpy(&rtt_target->channel, nla_data(iter2),
							sizeof(rtt_target->channel));
						break;
					case RTT_ATTRIBUTE_TARGET_MODE:
						rtt_target->continuous = nla_get_u8(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_INTERVAL:
						rtt_target->interval = nla_get_u32(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_NUM_MEASUREMENT:
						rtt_target->measure_cnt = nla_get_u32(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_NUM_PKT:
						rtt_target->ftm_cnt = nla_get_u32(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_NUM_RETRY:
						rtt_target->retry_cnt = nla_get_u32(iter2);
					}
				}
				/* convert to chanspec value */
				rtt_target->chanspec =
					dhd_rtt_convert_to_chspec(rtt_target->channel);
				if (rtt_target->chanspec == 0) {
					WL_ERR(("Channel is not valid \n"));
					goto exit;
				}
				WL_INFORM(("Target addr %s, Channel : %s for RTT \n",
					bcm_ether_ntoa((const struct ether_addr *)&rtt_target->addr,
					eabuf),
					wf_chspec_ntoa(rtt_target->chanspec, chanbuf)));
				rtt_target++;
			}
			break;
		}
	}
	WL_DBG(("leave :target_cnt : %d\n", rtt_param.rtt_target_cnt));
	if (dhd_dev_rtt_set_cfg(bcmcfg_to_prmry_ndev(cfg), &rtt_param) < 0) {
		WL_ERR(("Could not set RTT configuration\n"));
		err = -EINVAL;
	}
exit:
	return err;
}

static int
wl_cfgvendor_rtt_cancel_config(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int err = 0, rem, type, target_cnt = 0;
	int target_cnt_chk = 0;
	const struct nlattr *iter;
	struct ether_addr *mac_list = NULL, *mac_addr = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	if (len <= 0) {
		WL_ERR(("Length of nlattr is not valid len : %d\n", len));
		err = -EINVAL;
		goto exit;
	}
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
		case RTT_ATTRIBUTE_TARGET_CNT:
			if (mac_list != NULL) {
				WL_ERR(("mac_list is not NULL\n"));
				err = -EINVAL;
				goto exit;
			}
			target_cnt = nla_get_u8(iter);
			if ((target_cnt > 0) && (target_cnt < RTT_MAX_TARGET_CNT)) {
				mac_list = (struct ether_addr *)kzalloc(target_cnt * ETHER_ADDR_LEN,
					GFP_KERNEL);
				if (mac_list == NULL) {
					WL_ERR(("failed to allocate mem for mac list\n"));
					err = -EINVAL;
					goto exit;
				}
				mac_addr = &mac_list[0];
			} else {
				goto cancel;
			}
			break;
		case RTT_ATTRIBUTE_TARGET_MAC:
			if (mac_addr) {
				memcpy(mac_addr++, nla_data(iter), ETHER_ADDR_LEN);
				target_cnt_chk++;
				if (target_cnt_chk > target_cnt) {
					WL_ERR(("over target count\n"));
					err = -EINVAL;
					goto exit;
				}
				break;
			} else {
				WL_ERR(("mac_list is NULL\n"));
				err = -EINVAL;
				goto exit;
			}
		}
	}
cancel:
	if (dhd_dev_rtt_cancel_cfg(bcmcfg_to_prmry_ndev(cfg), mac_list, target_cnt) < 0) {
		WL_ERR(("Could not cancel RTT configuration\n"));
		err = -EINVAL;
		goto exit;
	}

exit:
	if (mac_list) {
		kfree(mac_list);
	}
	return err;
}
static int
wl_cfgvendor_rtt_get_capability(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	rtt_capabilities_t capability;

	err = dhd_dev_rtt_capability(bcmcfg_to_prmry_ndev(cfg), &capability);
	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
		goto exit;
	}
	err =  wl_cfgvendor_send_cmd_reply(wiphy, bcmcfg_to_prmry_ndev(cfg),
	        &capability, sizeof(capability));

	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}
exit:
	return err;
}

#endif /* RTT_SUPPORT */

static int
wl_cfgvendor_get_feature_set(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int reply;

	reply = dhd_dev_get_feature_set(bcmcfg_to_prmry_ndev(cfg));

	err = wl_cfgvendor_send_cmd_reply(wiphy, bcmcfg_to_prmry_ndev(cfg),
		&reply, sizeof(int));

	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}

	return err;
}

static int
wl_cfgvendor_get_feature_set_matrix(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct sk_buff *skb;
	int reply;
	int mem_needed, i;

	mem_needed = VENDOR_REPLY_OVERHEAD +
		(ATTRIBUTE_U32_LEN * MAX_FEATURE_SET_CONCURRRENT_GROUPS) + ATTRIBUTE_U32_LEN;

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		err = -ENOMEM;
		goto exit;
	}

	nla_put_u32(skb, ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET, MAX_FEATURE_SET_CONCURRRENT_GROUPS);
	for (i = 0; i < MAX_FEATURE_SET_CONCURRRENT_GROUPS; i++) {
		reply = dhd_dev_get_feature_set_matrix(bcmcfg_to_prmry_ndev(cfg), i);
		if (reply != WIFI_FEATURE_INVALID) {
			nla_put_u32(skb, ANDR_WIFI_ATTRIBUTE_FEATURE_SET, reply);
		}
	}

	err =  cfg80211_vendor_cmd_reply(skb);

	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}
exit:
	return err;
}

static int
wl_cfgvendor_set_pno_mac_oui(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type;
	uint8 pno_random_mac_oui[DOT11_OUI_LEN];

	type = nla_type(data);

	if (type == ANDR_WIFI_ATTRIBUTE_PNO_RANDOM_MAC_OUI) {
		memcpy(pno_random_mac_oui, nla_data(data), DOT11_OUI_LEN);

		err = dhd_dev_pno_set_mac_oui(bcmcfg_to_prmry_ndev(cfg), pno_random_mac_oui);

		if (unlikely(err)) {
			WL_ERR(("Bad OUI, could not set:%d \n", err));
		}

	} else {
		WL_ERR(("type not supported:%d \n", type));
		err = -EINVAL;
	}

	return err;
}

#ifdef DHD_SET_COUNTRY_SUPPORT
static int
wl_cfgvendor_set_country(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int err = BCME_ERROR, rem, type;
	char country_code[WLC_CNTRY_BUF_SZ] = {0};
	const struct nlattr *iter;

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case ANDR_WIFI_ATTRIBUTE_COUNTRY:
				memcpy(country_code, nla_data(iter),
					MIN(nla_len(iter), WLC_CNTRY_BUF_SZ));
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				return err;
		}
	}

	err = wldev_set_country(wdev->netdev, country_code, true, true);
	if (err < 0) {
		WL_ERR(("Set country failed ret:%d\n", err));
	}

	return err;
}
#endif /* DHD_SET_COUNTRY_SUPPORT */

#ifdef GSCAN_SUPPORT
int
wl_cfgvendor_send_hotlist_event(struct wiphy *wiphy,
	struct net_device *dev, void  *data, int len, wl_vendor_event_t event)
{
	u16 kflags;
	const void *ptr;
	struct sk_buff *skb;
	int malloc_len, total, iter_cnt_to_send, cnt;
	gscan_results_cache_t *cache = (gscan_results_cache_t *)data;

	total = len / sizeof(wifi_gscan_result_t);
	while (total > 0) {
		malloc_len = (total * sizeof(wifi_gscan_result_t)) + VENDOR_DATA_OVERHEAD;
		if (malloc_len > NLMSG_DEFAULT_SIZE) {
			malloc_len = NLMSG_DEFAULT_SIZE;
		}
		iter_cnt_to_send =
			(malloc_len - VENDOR_DATA_OVERHEAD)/sizeof(wifi_gscan_result_t);
		total = total - iter_cnt_to_send;

		kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

		/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
		skb = cfg80211_vendor_event_alloc(wiphy, NULL, malloc_len, event, kflags);
#else
		skb = cfg80211_vendor_event_alloc(wiphy, malloc_len, event, kflags);
#endif /* CONFIG_ARCH_MSM && SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC ||
		* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
		*/
		if (!skb) {
			WL_ERR(("skb alloc failed"));
			return -ENOMEM;
		}

		while (cache && iter_cnt_to_send) {
			ptr = (const void *) &cache->results[cache->tot_consumed];

			if (iter_cnt_to_send < (cache->tot_count - cache->tot_consumed)) {
				cnt = iter_cnt_to_send;
			} else {
				cnt = (cache->tot_count - cache->tot_consumed);
			}

			iter_cnt_to_send -= cnt;
			cache->tot_consumed += cnt;
			/* Push the data to the skb */
			nla_append(skb, cnt * sizeof(wifi_gscan_result_t), ptr);
			if (cache->tot_consumed == cache->tot_count)
				cache = cache->next;

		}

		cfg80211_vendor_event(skb, kflags);
	}

	return 0;
}

static int
wl_cfgvendor_gscan_get_capabilities(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pno_gscan_capabilities_t *reply = NULL;
	uint32 reply_len = 0;

	reply = dhd_dev_pno_get_gscan(bcmcfg_to_prmry_ndev(cfg),
		DHD_PNO_GET_CAPABILITIES, NULL, &reply_len);
	if (!reply) {
		WL_ERR(("Could not get capabilities\n"));
		err = -EINVAL;
		return err;
	}

	err =  wl_cfgvendor_send_cmd_reply(wiphy, bcmcfg_to_prmry_ndev(cfg),
		reply, reply_len);

	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}

	if (reply) {
		kfree(reply);
	}
	return err;
}

static int
wl_cfgvendor_gscan_get_channel_list(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0, type, band;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	uint16 *reply = NULL;
	uint32 reply_len = 0, num_channels, mem_needed;
	struct sk_buff *skb;

	type = nla_type(data);

	if (type == GSCAN_ATTRIBUTE_BAND) {
		band = nla_get_u32(data);
	} else {
		return -EINVAL;
	}

	reply = dhd_dev_pno_get_gscan(bcmcfg_to_prmry_ndev(cfg),
		DHD_PNO_GET_CHANNEL_LIST, &band, &reply_len);

	if (!reply) {
		WL_ERR(("Could not get channel list\n"));
		err = -EINVAL;
		return err;
	}
	num_channels =  reply_len/ sizeof(uint32);
	mem_needed = reply_len + VENDOR_REPLY_OVERHEAD + (ATTRIBUTE_U32_LEN * 2);

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		err = -ENOMEM;
		goto exit;
	}

	nla_put_u32(skb, GSCAN_ATTRIBUTE_NUM_CHANNELS, num_channels);
	nla_put(skb, GSCAN_ATTRIBUTE_CHANNEL_LIST, reply_len, reply);

	err =  cfg80211_vendor_cmd_reply(skb);

	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}
exit:
	if (reply) {
		kfree(reply);
	}
	return err;
}

static int
wl_cfgvendor_gscan_get_batch_results(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	gscan_results_cache_t *results, *iter;
	uint32 reply_len, complete = 0;
	int32 mem_needed, num_results_iter;
	wifi_gscan_result_t *ptr;
	uint16 num_scan_ids, num_results;
	struct sk_buff *skb;
	struct nlattr *scan_hdr;

	dhd_dev_wait_batch_results_complete(bcmcfg_to_prmry_ndev(cfg));
	dhd_dev_pno_lock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
	results = dhd_dev_pno_get_gscan(bcmcfg_to_prmry_ndev(cfg),
		DHD_PNO_GET_BATCH_RESULTS, NULL, &reply_len);

	if (!results) {
		WL_ERR(("No results to send %d\n", err));
		err =  wl_cfgvendor_send_cmd_reply(wiphy, bcmcfg_to_prmry_ndev(cfg),
			results, 0);

		if (unlikely(err)) {
			WL_ERR(("Vendor Command reply failed ret:%d \n", err));
		}
		dhd_dev_pno_unlock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
		return err;
	}
	num_scan_ids = reply_len & 0xFFFF;
	num_results = (reply_len & 0xFFFF0000) >> 16;
	mem_needed = (num_results * sizeof(wifi_gscan_result_t)) +
		(num_scan_ids * GSCAN_BATCH_RESULT_HDR_LEN) +
		VENDOR_REPLY_OVERHEAD + SCAN_RESULTS_COMPLETE_FLAG_LEN;

	if (mem_needed > (int32)NLMSG_DEFAULT_SIZE) {
		mem_needed = (int32)NLMSG_DEFAULT_SIZE;
		complete = 0;
	} else {
		complete = 1;
	}

	WL_TRACE(("complete %d mem_needed %d max_mem %d\n", complete, mem_needed,
		(int)NLMSG_DEFAULT_SIZE));
	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		dhd_dev_pno_unlock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
		return -ENOMEM;
	}
	iter = results;

	nla_put_u32(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE, complete);

	mem_needed = mem_needed - (SCAN_RESULTS_COMPLETE_FLAG_LEN + VENDOR_REPLY_OVERHEAD);

	while (iter) {
		num_results_iter =
		(mem_needed - GSCAN_BATCH_RESULT_HDR_LEN)/sizeof(wifi_gscan_result_t);
		if (num_results_iter <= 0 ||
			((iter->tot_count - iter->tot_consumed) > num_results_iter)) {
			break;
		}
		scan_hdr = nla_nest_start(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS);
		if (!scan_hdr) {
			WL_ERR(("scan_hdr is NULL.\n"));
			break;
		}
		nla_put_u32(skb, GSCAN_ATTRIBUTE_SCAN_ID, iter->scan_id);
		nla_put_u8(skb, GSCAN_ATTRIBUTE_SCAN_FLAGS, iter->flag);
		num_results_iter = iter->tot_count - iter->tot_consumed;

		nla_put_u32(skb, GSCAN_ATTRIBUTE_NUM_OF_RESULTS, num_results_iter);
		if (num_results_iter) {
			ptr = &iter->results[iter->tot_consumed];
			iter->tot_consumed += num_results_iter;
			nla_put(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS,
				num_results_iter * sizeof(wifi_gscan_result_t), ptr);
		}
		nla_nest_end(skb, scan_hdr);
		mem_needed -= GSCAN_BATCH_RESULT_HDR_LEN +
			(num_results_iter * sizeof(wifi_gscan_result_t));
		iter = iter->next;
	}

	dhd_dev_gscan_batch_cache_cleanup(bcmcfg_to_prmry_ndev(cfg));
	dhd_dev_pno_unlock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));

	return cfg80211_vendor_cmd_reply(skb);
}

static int
wl_cfgvendor_initiate_gscan(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type, tmp = len;
	int run = 0xFF;
	int flush = 0;
	const struct nlattr *iter;

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		if (type == GSCAN_ATTRIBUTE_ENABLE_FEATURE) {
			run = nla_get_u32(iter);
		} else if (type == GSCAN_ATTRIBUTE_FLUSH_FEATURE) {
			flush = nla_get_u32(iter);
		}
	}

	if (run != 0xFF) {
		err = dhd_dev_pno_run_gscan(bcmcfg_to_prmry_ndev(cfg), run, flush);

		if (unlikely(err)) {
			WL_ERR(("Could not run gscan:%d \n", err));
		}
	} else {
		WL_ERR(("run value was 0xFF\n"));
		err = -EINVAL;
	}

	return err;
}

static int
wl_cfgvendor_enable_full_scan_result(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type;
	bool real_time = FALSE;

	type = nla_type(data);

	if (type == GSCAN_ATTRIBUTE_ENABLE_FULL_SCAN_RESULTS) {
		real_time = nla_get_u32(data);

		err = dhd_dev_pno_enable_full_scan_result(bcmcfg_to_prmry_ndev(cfg), real_time);

		if (unlikely(err)) {
			WL_ERR(("Could not run gscan:%d \n", err));
		}

	} else {
		WL_ERR(("type not supported:%d \n", type));
		err = -EINVAL;
	}

	return err;
}

static int
wl_cfgvendor_set_scan_cfg_bucket(const struct nlattr *prev,
	gscan_scan_params_t *scan_param, int num)
{
	struct dhd_pno_gscan_channel_bucket  *ch_bucket;
	int k = 0;
	int type, err = 0, rem;
	const struct nlattr *cur, *next;

	nla_for_each_nested(cur, prev, rem) {
		type = nla_type(cur);
		ch_bucket = scan_param->channel_bucket;
		switch (type) {
		case GSCAN_ATTRIBUTE_BUCKET_ID:
			break;
		case GSCAN_ATTRIBUTE_BUCKET_PERIOD:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}

			ch_bucket[num].bucket_freq_multiple =
				nla_get_u32(cur) / MSEC_PER_SEC;
			break;
		case GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].num_channels = nla_get_u32(cur);
			if (ch_bucket[num].num_channels >
				GSCAN_MAX_CHANNELS_IN_BUCKET) {
				WL_ERR(("channel range:%d,bucket:%d\n",
					ch_bucket[num].num_channels,
					num));
				err = -EINVAL;
				goto exit;
			}
			break;
		case GSCAN_ATTRIBUTE_BUCKET_CHANNELS:
			nla_for_each_nested(next, cur, rem) {
				if (k >= GSCAN_MAX_CHANNELS_IN_BUCKET)
					break;
				if (nla_len(next) != sizeof(uint32)) {
					err = -EINVAL;
					goto exit;
				}
				ch_bucket[num].chan_list[k] = nla_get_u32(next);
				k++;
			}
			break;
		case GSCAN_ATTRIBUTE_BUCKETS_BAND:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].band = (uint16)nla_get_u32(cur);
			break;
		case GSCAN_ATTRIBUTE_REPORT_EVENTS:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].report_flag = (uint8)nla_get_u32(cur);
			break;
		case GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].repeat = (uint16)nla_get_u32(cur);
			break;
		case GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].bucket_max_multiple =
				nla_get_u32(cur) / MSEC_PER_SEC;
			break;
		default:
			WL_ERR(("unknown attr type:%d\n", type));
			err = -EINVAL;
			goto exit;
		}
	}

exit:
	return err;
}

static int
wl_cfgvendor_set_scan_cfg(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	gscan_scan_params_t *scan_param;
	int j = 0;
	int type, tmp;
	const struct nlattr *iter;

	scan_param = kzalloc(sizeof(gscan_scan_params_t), GFP_KERNEL);
	if (!scan_param) {
		WL_ERR(("Could not set GSCAN scan cfg, mem alloc failure\n"));
		err = -EINVAL;
		return err;

	}

	scan_param->scan_fr = PNO_SCAN_MIN_FW_SEC;
	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);

		if (j >= GSCAN_MAX_CH_BUCKETS) {
			break;
		}

		switch (type) {
			case GSCAN_ATTRIBUTE_BASE_PERIOD:
				if (nla_len(iter) != sizeof(uint32)) {
					err = -EINVAL;
					goto exit;
				}
				scan_param->scan_fr = nla_get_u32(iter) / MSEC_PER_SEC;
				break;
			case GSCAN_ATTRIBUTE_NUM_BUCKETS:
				if (nla_len(iter) != sizeof(uint32)) {
					err = -EINVAL;
					goto exit;
				}
				scan_param->nchannel_buckets = nla_get_u32(iter);
				if (scan_param->nchannel_buckets >=
				    GSCAN_MAX_CH_BUCKETS) {
					WL_ERR(("ncha_buck out of range %d\n",
					scan_param->nchannel_buckets));
					err = -EINVAL;
					goto exit;
				}
				break;
			case GSCAN_ATTRIBUTE_CH_BUCKET_1:
			case GSCAN_ATTRIBUTE_CH_BUCKET_2:
			case GSCAN_ATTRIBUTE_CH_BUCKET_3:
			case GSCAN_ATTRIBUTE_CH_BUCKET_4:
			case GSCAN_ATTRIBUTE_CH_BUCKET_5:
			case GSCAN_ATTRIBUTE_CH_BUCKET_6:
			case GSCAN_ATTRIBUTE_CH_BUCKET_7:
				err = wl_cfgvendor_set_scan_cfg_bucket(iter, scan_param, j);
				if (err < 0) {
					WL_ERR(("set_scan_cfg_buck error:%d\n", err));
					goto exit;
				}
				j++;
				break;
			default:
				WL_ERR(("No GSCAN_ATTR_CH_BUKT_type - %x\n", type));
				err = -EINVAL;
				goto exit;
		}
	}

	err = dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
	     DHD_PNO_SCAN_CFG_ID, scan_param, FALSE);

	if (err < 0) {
		WL_ERR(("Could not set GSCAN scan cfg\n"));
		err = -EINVAL;
	}

exit:
	kfree(scan_param);
	return err;
}

static int
wl_cfgvendor_hotlist_cfg(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	gscan_hotlist_scan_params_t *hotlist_params;
	int tmp, tmp1, tmp2, type, j = 0, dummy;
	const struct nlattr *outer, *inner = NULL, *iter;
	uint8 flush = 0;
	struct bssid_t *pbssid;

	BCM_REFERENCE(dummy);

	if (len < sizeof(*hotlist_params) || len >= WLC_IOCTL_MAXLEN) {
		WL_ERR(("buffer length :%d wrong - bail out.\n", len));
		return -EINVAL;
	}

	hotlist_params = kzalloc(sizeof(*hotlist_params)
		+ (sizeof(struct bssid_t) * (PFN_SWC_MAX_NUM_APS - 1)),
		GFP_KERNEL);

	if (!hotlist_params) {
		WL_ERR(("Cannot Malloc memory.\n"));
		return -1;
	}

	hotlist_params->lost_ap_window = GSCAN_LOST_AP_WINDOW_DEFAULT;

	nla_for_each_attr(iter, data, len, tmp2) {
		type = nla_type(iter);
		switch (type) {
		case GSCAN_ATTRIBUTE_HOTLIST_BSSIDS:
			pbssid = hotlist_params->bssid;
			nla_for_each_nested(outer, iter, tmp) {
				nla_for_each_nested(inner, outer, tmp1) {
					type = nla_type(inner);

					switch (type) {
					case GSCAN_ATTRIBUTE_BSSID:
						if (nla_len(inner) != sizeof(pbssid[j].macaddr)) {
							WL_ERR(("type:%d length:%d not matching.\n",
								type, nla_len(inner)));
							err = -EINVAL;
							goto exit;
						}
						memcpy(
							&(pbssid[j].macaddr),
							nla_data(inner),
							sizeof(pbssid[j].macaddr));
						break;
					case GSCAN_ATTRIBUTE_RSSI_LOW:
						if (nla_len(inner) != sizeof(uint8)) {
							WL_ERR(("type:%d length:%d not matching.\n",
								type, nla_len(inner)));
							err = -EINVAL;
							goto exit;
						}
						pbssid[j].rssi_reporting_threshold =
							(int8)nla_get_u8(inner);
						break;
					case GSCAN_ATTRIBUTE_RSSI_HIGH:
						if (nla_len(inner) != sizeof(uint8)) {
							WL_ERR(("type:%d length:%d not matching.\n",
								type, nla_len(inner)));
							err = -EINVAL;
							goto exit;
						}
						dummy = (int8)nla_get_u8(inner);
						break;
					default:
						WL_ERR(("ATTR unknown %d\n", type));
						err = -EINVAL;
						goto exit;
					}
				}
				if (++j >= PFN_SWC_MAX_NUM_APS) {
					WL_ERR(("cap hotlist max:%d\n", j));
					break;
				}
			}
			hotlist_params->nbssid = j;
			break;
		case GSCAN_ATTRIBUTE_HOTLIST_FLUSH:
			if (nla_len(iter) != sizeof(uint8)) {
				WL_ERR(("type:%d length:%d not matching.\n",
					type, nla_len(inner)));
				err = -EINVAL;
				goto exit;
			}
			flush = nla_get_u8(iter);
			break;
		case GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE:
			if (nla_len(iter) != sizeof(uint32)) {
				WL_ERR(("type:%d length:%d not matching.\n",
					type, nla_len(inner)));
				err = -EINVAL;
				goto exit;
			}
			hotlist_params->lost_ap_window = (uint16)nla_get_u32(iter);
			break;
		default:
			WL_ERR(("Unknown type %d\n", type));
			err = -EINVAL;
			goto exit;
		}

	}
	if (dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
		DHD_PNO_GEOFENCE_SCAN_CFG_ID, hotlist_params, flush) < 0) {
		WL_ERR(("Could not set GSCAN HOTLIST cfg error: %d\n", err));
		err = -EINVAL;
		goto exit;
	}
exit:
	if (hotlist_params) {
		kfree(hotlist_params);
	}
	return err;
}

static int
wl_cfgvendor_set_batch_scan_cfg(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0, tmp, type;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	gscan_batch_params_t batch_param;
	const struct nlattr *iter;

	batch_param.mscan = batch_param.bestn = 0;
	batch_param.buffer_threshold = GSCAN_BATCH_NO_THR_SET;

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);

		switch (type) {
			case GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN:
				batch_param.bestn = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE:
				batch_param.mscan = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_REPORT_THRESHOLD:
				batch_param.buffer_threshold = nla_get_u32(iter);
				break;
			default:
				WL_ERR(("No ATTR_type - %x\n", type));
				break;
		}
	}

	if (dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
		DHD_PNO_BATCH_SCAN_CFG_ID, &batch_param, 0) < 0) {
		WL_ERR(("Could not set batch cfg\n"));
		err = -EINVAL;
		return err;
	}

	return err;
}

#endif /* GSCAN_SUPPORT */

#if defined(KEEP_ALIVE)
static int wl_cfgvendor_start_mkeep_alive(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	/* max size of IP packet for keep alive */
	const int MKEEP_ALIVE_IP_PKT_MAX = 256;

	int ret = BCME_OK, rem, type;
	u8 mkeep_alive_id = 0;
	u8 *ip_pkt = NULL;
	u16 ip_pkt_len = 0;
	u8 src_mac[ETHER_ADDR_LEN];
	u8 dst_mac[ETHER_ADDR_LEN];
	u32 period_msec = 0;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;
	gfp_t kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case MKEEP_ALIVE_ATTRIBUTE_ID:
				mkeep_alive_id = nla_get_u8(iter);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN:
				ip_pkt_len = nla_get_u16(iter);
				if (ip_pkt_len > MKEEP_ALIVE_IP_PKT_MAX) {
					ret = BCME_BADARG;
					goto exit;
				}
				break;
			case MKEEP_ALIVE_ATTRIBUTE_IP_PKT:
				if (!ip_pkt_len) {
					ret = BCME_BADARG;
					WL_ERR(("ip packet length is 0\n"));
					goto exit;
				}
				ip_pkt = (u8 *)kzalloc(ip_pkt_len, kflags);
				if (ip_pkt == NULL) {
					ret = BCME_NOMEM;
					WL_ERR(("Failed to allocate mem for ip packet\n"));
					goto exit;
				}
				memcpy(ip_pkt, (u8*)nla_data(iter), ip_pkt_len);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR:
				memcpy(src_mac, nla_data(iter), ETHER_ADDR_LEN);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR:
				memcpy(dst_mac, nla_data(iter), ETHER_ADDR_LEN);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC:
				period_msec = nla_get_u32(iter);
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_BADARG;
				goto exit;
		}
	}

	if (ip_pkt == NULL) {
		ret = BCME_BADARG;
		WL_ERR(("ip packet is NULL\n"));
		goto exit;
	}

	ret = dhd_dev_start_mkeep_alive(dhd_pub, mkeep_alive_id, ip_pkt, ip_pkt_len, src_mac,
		dst_mac, period_msec);
	if (ret < 0) {
		WL_ERR(("start_mkeep_alive is failed ret: %d\n", ret));
	}

exit:
	if (ip_pkt) {
		kfree(ip_pkt);
	}

	return ret;
}

static int wl_cfgvendor_stop_mkeep_alive(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int ret = BCME_OK, rem, type;
	u8 mkeep_alive_id = 0;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case MKEEP_ALIVE_ATTRIBUTE_ID:
				mkeep_alive_id = nla_get_u8(iter);
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_BADARG;
				break;
		}
	}

	ret = dhd_dev_stop_mkeep_alive(dhd_pub, mkeep_alive_id);
	if (ret < 0) {
		WL_ERR(("stop_mkeep_alive is failed ret: %d\n", ret));
	}

	return ret;
}
#endif /* defined(KEEP_ALIVE) */


#if defined(PKT_FILTER_SUPPORT) && defined(APF)
static int
wl_cfgvendor_apf_get_capabilities(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	struct net_device *ndev = wdev_to_ndev(wdev);
	struct sk_buff *skb;
	int ret, ver, max_len, mem_needed;

	/* APF version */
	ver = 0;
	ret = dhd_dev_apf_get_version(ndev, &ver);
	if (unlikely(ret)) {
		WL_ERR(("APF get version failed, ret=%d\n", ret));
		return ret;
	}

	/* APF memory size limit */
	max_len = 0;
	ret = dhd_dev_apf_get_max_len(ndev, &max_len);
	if (unlikely(ret)) {
		WL_ERR(("APF get maximum length failed, ret=%d\n", ret));
		return ret;
	}

	mem_needed = VENDOR_REPLY_OVERHEAD + (ATTRIBUTE_U32_LEN * 2);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("%s: can't allocate %d bytes\n", __FUNCTION__, mem_needed));
		return -ENOMEM;
	}

	nla_put_u32(skb, APF_ATTRIBUTE_VERSION, ver);
	nla_put_u32(skb, APF_ATTRIBUTE_MAX_LEN, max_len);

	ret = cfg80211_vendor_cmd_reply(skb);
	if (unlikely(ret)) {
		WL_ERR(("vendor command reply failed, ret=%d\n", ret));
	}

	return ret;
}

static int
wl_cfgvendor_apf_set_filter(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	struct net_device *ndev = wdev_to_ndev(wdev);
	const struct nlattr *iter;
	u8 *program = NULL;
	u32 program_len = 0;
	int ret, tmp, type;
	gfp_t kflags;

	if (len <= 0) {
		WL_ERR(("Invalid len: %d\n", len));
		ret = -EINVAL;
		goto exit;
	}
	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		switch (type) {
			case APF_ATTRIBUTE_PROGRAM_LEN:
				/* check if the iter value is valid and program_len
				 * is not already initialized.
				 */
				if (nla_len(iter) == sizeof(uint32) && !program_len) {
					program_len = nla_get_u32(iter);
				} else {
					ret = -EINVAL;
					goto exit;
				}

				if (program_len > WL_APF_PROGRAM_MAX_SIZE) {
					WL_ERR(("program len is more than expected len\n"));
					ret = -EINVAL;
					goto exit;
				}

				if (unlikely(!program_len)) {
					WL_ERR(("zero program length\n"));
					ret = -EINVAL;
					goto exit;
				}
				break;
			case APF_ATTRIBUTE_PROGRAM:
				if (unlikely(!program_len)) {
					WL_ERR(("program len is not set\n"));
					ret = -EINVAL;
					goto exit;
				}
				kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
				program = kzalloc(program_len, kflags);
				if (unlikely(!program)) {
					WL_ERR(("%s: can't allocate %d bytes\n",
						__FUNCTION__, program_len));
					ret = -ENOMEM;
					goto exit;
				}
				memcpy(program, (u8*)nla_data(iter), program_len);
				break;
			default:
				WL_ERR(("%s: no such attribute %d\n", __FUNCTION__, type));
				ret = -EINVAL;
				goto exit;
		}
	}

	if (!program || !program_len) {
		WL_ERR(("program alloc, program len %d set fail\n", program_len));
		ret = -EINVAL;
		goto exit;
	}

	ret = dhd_dev_apf_add_filter(ndev, program, program_len);

exit:
	if (program) {
		kfree(program);
	}
	return ret;
}
#endif /* PKT_FILTER_SUPPORT && APF */

static int
wl_cfgvendor_priv_string_handler(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int err = 0;
	int data_len = 0;

	WL_INFORM(("%s: Enter \n", __func__));

	bzero(cfg->ioctl_buf, WLC_IOCTL_MAXLEN);

	if (strncmp((char *)data, BRCM_VENDOR_SCMD_CAPA, strlen(BRCM_VENDOR_SCMD_CAPA)) == 0) {
		err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "cap", NULL, 0,
			cfg->ioctl_buf, WLC_IOCTL_MAXLEN, &cfg->ioctl_buf_sync);
		if (unlikely(err)) {
			WL_ERR(("error (%d)\n", err));
			return err;
		}
		data_len = strlen(cfg->ioctl_buf);
		cfg->ioctl_buf[data_len] = '\0';
	}

	err =  wl_cfgvendor_send_cmd_reply(wiphy, bcmcfg_to_prmry_ndev(cfg),
		cfg->ioctl_buf, data_len+1);
	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	} else {
		WL_INFORM(("Vendor Command reply sent successfully!\n"));
	}

	return err;
}

#ifdef LINKSTAT_SUPPORT
static int
wl_cfgvendor_lstats_get_info(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	char *iovar_buf = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int err = 0;
	wifi_iface_stat *iface;
	wifi_radio_stat *radio;
	wl_wme_cnt_t *wl_wme_cnt;
	wl_cnt_t *wl_cnt;
	char *output;
	int radstat_buflen = sizeof(wifi_radio_stat) + NUM_CHAN * sizeof(wifi_channel_stat);
	int iface_buflen = sizeof(wifi_iface_stat) + NUM_PEER * sizeof(wifi_peer_info);
	int ratestat_buflen = NUM_RATE * sizeof(wifi_rate_stat);

	WL_INFORM(("%s: Enter \n", __func__));
	RETURN_EIO_IF_NOT_UP(cfg);

	bzero(cfg->ioctl_buf, WLC_IOCTL_MAXLEN);
	iovar_buf = kzalloc(sizeof(char) * WLC_IOCTL_MAXLEN, GFP_KERNEL);
	if (unlikely(!iovar_buf)) {
		WL_ERR(("%s:link stats buf alloc fails\n", __FUNCTION__));
		return -ENOMEM;
	}

	ASSERT((radstat_buflen + iface_buflen + ratestat_buflen) <=
		(sizeof(char) * WLC_IOCTL_MAXLEN));

	output = iovar_buf;
	radio = (wifi_radio_stat *)output;

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "radiostat", NULL, 0,
		cfg->ioctl_buf, WLC_IOCTL_MAXLEN, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		WL_ERR(("error (%d) - size = %zu\n", err, sizeof(wifi_radio_stat)));
		goto exit;
	}
	memcpy(output, cfg->ioctl_buf, sizeof(wifi_radio_stat));

	radio->num_channels = NUM_CHAN;
	output += radstat_buflen;

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "wme_counters", NULL, 0,
		cfg->ioctl_buf, WLC_IOCTL_MAXLEN, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		WL_ERR(("error (%d)\n", err));
		goto exit;
	}
	wl_wme_cnt = (wl_wme_cnt_t *)cfg->ioctl_buf;
	iface = (wifi_iface_stat *)output;

	iface->ac[WIFI_AC_VO].ac = WIFI_AC_VO;
	iface->ac[WIFI_AC_VO].tx_mpdu = wl_wme_cnt->tx[AC_VO].packets;
	iface->ac[WIFI_AC_VO].rx_mpdu = wl_wme_cnt->rx[AC_VO].packets;
	iface->ac[WIFI_AC_VO].mpdu_lost = wl_wme_cnt->tx_failed[WIFI_AC_VO].packets;

	iface->ac[WIFI_AC_VI].ac = WIFI_AC_VI;
	iface->ac[WIFI_AC_VI].tx_mpdu = wl_wme_cnt->tx[AC_VI].packets;
	iface->ac[WIFI_AC_VI].rx_mpdu = wl_wme_cnt->rx[AC_VI].packets;
	iface->ac[WIFI_AC_VI].mpdu_lost = wl_wme_cnt->tx_failed[WIFI_AC_VI].packets;

	iface->ac[WIFI_AC_BE].ac = WIFI_AC_BE;
	iface->ac[WIFI_AC_BE].tx_mpdu = wl_wme_cnt->tx[AC_BE].packets;
	iface->ac[WIFI_AC_BE].rx_mpdu = wl_wme_cnt->rx[AC_BE].packets;
	iface->ac[WIFI_AC_BE].mpdu_lost = wl_wme_cnt->tx_failed[WIFI_AC_BE].packets;

	iface->ac[WIFI_AC_BK].ac = WIFI_AC_BK;
	iface->ac[WIFI_AC_BK].tx_mpdu = wl_wme_cnt->tx[AC_BK].packets;
	iface->ac[WIFI_AC_BK].rx_mpdu = wl_wme_cnt->rx[AC_BK].packets;
	iface->ac[WIFI_AC_BK].mpdu_lost = wl_wme_cnt->tx_failed[WIFI_AC_BK].packets;
	bzero(cfg->ioctl_buf, WLC_IOCTL_MAXLEN);

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "counters", NULL, 0,
		cfg->ioctl_buf, WLC_IOCTL_MAXLEN, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		WL_ERR(("error (%d) - size = %zu\n", err, sizeof(wl_cnt_t)));
		goto exit;
	}
	wl_cnt = (wl_cnt_t *)cfg->ioctl_buf;
	iface->ac[WIFI_AC_BE].retries = wl_cnt->txretry;
	iface->beacon_rx = wl_cnt->rxbeaconmbss;

	err = wldev_get_rssi(bcmcfg_to_prmry_ndev(cfg), &iface->rssi_mgmt);
	if (unlikely(err)) {
		WL_ERR(("get_rssi error (%d)\n", err));
		goto exit;
	}

	iface->num_peers = NUM_PEER;
	iface->peer_info->num_rate = NUM_RATE;

	bzero(cfg->ioctl_buf, WLC_IOCTL_MAXLEN);
	output = (char *)iface + iface_buflen;

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "ratestat", NULL, 0,
		cfg->ioctl_buf, WLC_IOCTL_MAXLEN, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		WL_ERR(("error (%d) - size = %d\n", err, ratestat_buflen));
		goto exit;
	}
	memcpy(output, cfg->ioctl_buf, ratestat_buflen);

	err =  wl_cfgvendor_send_cmd_reply(wiphy, bcmcfg_to_prmry_ndev(cfg),
		iovar_buf, radstat_buflen + iface_buflen + ratestat_buflen);
	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}

exit:

	if (iovar_buf) {
		kfree(iovar_buf);
	}
	return err;
}
#endif /* LINKSTAT_SUPPORT */

static const struct wiphy_vendor_command wl_vendor_cmds [] = {
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_PRIV_STR
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_priv_string_handler
	},
#ifdef GSCAN_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_gscan_get_capabilities
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_SET_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_scan_cfg
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_SET_SCAN_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_batch_scan_cfg
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_ENABLE_GSCAN
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_initiate_gscan
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_ENABLE_FULL_SCAN_RESULTS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_enable_full_scan_result
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_SET_HOTLIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_hotlist_cfg
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_GET_SCAN_RESULTS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_gscan_get_batch_results
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_GET_CHANNEL_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_gscan_get_channel_list
	},
#endif /* GSCAN_SUPPORT */
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_SUBCMD_GET_FEATURE_SET
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_get_feature_set
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_SUBCMD_GET_FEATURE_SET_MATRIX
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_get_feature_set_matrix
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_PNO_RANDOM_MAC_OUI
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_pno_mac_oui
	},
#ifdef RTT_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_SET_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_set_config
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_CANCEL_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_cancel_config
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_GETCAPABILITY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_get_capability
	},
#endif /* RTT_SUPPORT */
#ifdef KEEP_ALIVE
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_OFFLOAD_SUBCMD_START_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_start_mkeep_alive
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_OFFLOAD_SUBCMD_STOP_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_stop_mkeep_alive
	},
#endif /* KEEP_ALIVE */
#ifdef LINKSTAT_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = LSTATS_SUBCMD_GET_INFO
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_lstats_get_info
	},
#endif /* LINKSTAT_SUPPORT */
#ifdef DHD_SET_COUNTRY_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_SET_COUNTRY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_country
	},
#endif /* DHD_SET_COUNTRY_SUPPORT */
#if defined(PKT_FILTER_SUPPORT) && defined(APF)
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = APF_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_apf_get_capabilities
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = APF_SUBCMD_SET_FILTER
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_apf_set_filter
	},
#endif /* PKT_FILTER_SUPPORT && APF */
};

static const struct  nl80211_vendor_cmd_info wl_vendor_events [] = {
		{ OUI_BRCM, BRCM_VENDOR_EVENT_UNSPEC },
		{ OUI_BRCM, BRCM_VENDOR_EVENT_PRIV_STR },
#ifdef GSCAN_SUPPORT
		{ OUI_GOOGLE, GOOGLE_GSCAN_GEOFENCE_FOUND_EVENT },
		{ OUI_GOOGLE, GOOGLE_GSCAN_BATCH_SCAN_EVENT },
		{ OUI_GOOGLE, GOOGLE_SCAN_FULL_RESULTS_EVENT },
#endif /* GSCAN_SUPPORT */
#ifdef RTT_SUPPORT
		{ OUI_GOOGLE, GOOGLE_RTT_COMPLETE_EVENT },
#endif /* RTT_SUPPORT */
#ifdef GSCAN_SUPPORT
		{ OUI_GOOGLE, GOOGLE_SCAN_COMPLETE_EVENT },
		{ OUI_GOOGLE, GOOGLE_GSCAN_GEOFENCE_LOST_EVENT },
#endif /* GSCAN_SUPPORT */
#ifdef KEEP_ALIVE
		{ OUI_GOOGLE, GOOGLE_MKEEP_ALIVE_EVENT },
#endif
};

void
wl_cfgvendor_attach(struct wiphy *wiphy)
{

	WL_INFORM(("Vendor: Register BRCM cfg80211 vendor cmd(0x%x) interface \n",
		NL80211_CMD_VENDOR));

	wiphy->vendor_commands	= wl_vendor_cmds;
	wiphy->n_vendor_commands = ARRAY_SIZE(wl_vendor_cmds);
	wiphy->vendor_events	= wl_vendor_events;
	wiphy->n_vendor_events	= ARRAY_SIZE(wl_vendor_events);

	return;
}

void
wl_cfgvendor_detach(struct wiphy *wiphy)
{
	WL_INFORM(("Vendor: Unregister BRCM cfg80211 vendor interface \n"));

	wiphy->vendor_commands  = NULL;
	wiphy->vendor_events    = NULL;
	wiphy->n_vendor_commands = 0;
	wiphy->n_vendor_events  = 0;

	return;
}
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(3, 13, 0)) || defined(WL_VENDOR_EXT_SUPPORT) */
