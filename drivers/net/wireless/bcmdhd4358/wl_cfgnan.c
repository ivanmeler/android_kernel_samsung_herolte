/*
 * Neighbor Awareness Networking
 *
 * Support NAN (Neighbor Awareness Networking) and RTT (Round Trip Time)
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
 * $Id: wl_cfgnan.c 487532 2014-06-26 05:09:36Z $
 */

/**
 * @file
 * @brief
 * NAN (Neighbor Awareness Networking) is to provide a low-power mechanism
 * that will run in the background to make the devices neighbor aware. NAN
 * enables mobile devices to efficiently discover devices and services in
 * their proximity. The purpose of NAN is only to discover device and its
 * services. Then it is handed over to post NAN discovery and connection
 * is established using the actual service data channel (e.g., WLAN
 * infrastructure, P2P, IBSS, etc.)
 */


#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <proto/nan.h>

#include <wl_cfg80211.h>
#include <wl_android.h>
#include <wl_cfgnan.h>

#ifdef WL_NAN_DEBUG
static u8 g_nan_debug = true;
#else
static u8 g_nan_debug = false;
#endif /* WL_NAN_DEBUG */

static nan_cmd_t nan_cmds [] = {
	{ "NAN_START", wl_cfgnan_start_handler },
	{ "NAN_STOP", wl_cfgnan_stop_handler },
	{ "NAN_SUPPORT", wl_cfgnan_support_handler },
	{ "NAN_STATUS", wl_cfgnan_status_handler },
	{ "NAN_PUBLISH", wl_cfgnan_pub_handler },
	{ "NAN_SUBSCRIBE", wl_cfgnan_sub_handler },
	{ "NAN_CANCEL_PUBLISH", wl_cfgnan_cancel_pub_handler },
	{ "NAN_CANCEL_SUBSCRIBE", wl_cfgnan_cancel_sub_handler },
	{ "NAN_TRANSMIT", wl_cfgnan_transmit_handler },
	{ "NAN_SET_CONFIG", wl_cfgnan_set_config_handler },
	{ "NAN_GET_CONFIG", NULL },
	{ "NAN_RTT_CONFIG", wl_cfgnan_rtt_config_handler },
	{ "NAN_RTT_FIND", wl_cfgnan_rtt_find_handler },
	{ "NAN_DEBUG", wl_cfgnan_debug_handler },
	{ NULL, NULL },
};

static nan_config_attr_t nan_config_attrs [] = {
	{ "ATTR_MASTER", WL_NAN_XTLV_MASTER_PREF },
	{ "ATTR_ID", WL_NAN_XTLV_CLUSTER_ID },
	{ "ATTR_ADDR", WL_NAN_XTLV_IF_ADDR },
	{ "ATTR_ROLE", WL_NAN_XTLV_ROLE },
	{ "ATTR_BCN_INT", WL_NAN_XTLV_BCN_INTERVAL },
	{ "ATTR_CHAN", WL_NAN_XTLV_MAC_CHANSPEC },
	{ "ATTR_TX_RATE", WL_NAN_XTLV_MAC_TXRATE },
	{ "ATTR_DW_LEN", WL_NAN_XTLV_DW_LEN },
	{ {0}, 0 }
};

static char nan_event_str[][NAN_EVENT_STR_MAX_LEN] = {
	"NAN-INVALID",
	"NAN-STARTED",
	"NAN-JOINED",
	"NAN-ROLE-CHANGE",
	"NAN-SCAN-COMPLETE",
	"NAN-SDF-RX",
	"NAN-REPLIED",
	"NAN-TERMINATED",
	"NAN-FOLLOWUP-RX",
	"NAN-STATUS-CHANGE",
	"NAN-MERGED",
	"NAN-STOPPED"
	"NAN-INVALID",
};

static char nan_event_name[][NAN_EVENT_NAME_MAX_LEN] = {
	"WL_NAN_EVENT_INVALID",
	"WL_NAN_EVENT_START",
	"WL_NAN_EVENT_JOIN",
	"WL_NAN_EVENT_ROLE",
	"WL_NAN_EVENT_SCAN_COMPLETE",
	"WL_NAN_EVENT_DISCOVERY_RESULT",
	"WL_NAN_EVENT_REPLIED",
	"WL_NAN_EVENT_TERMINATED",
	"WL_NAN_EVENT_RECEIVE",
	"WL_NAN_EVENT_STATUS_CHG",
	"WL_NAN_EVENT_MERGE",
	"WL_NAN_EVENT_STOP",
	"WL_NAN_EVENT_INVALID"
};

int wl_cfgnan_set_vars_cbfn(void *ctx, void **tlv_buf,
	uint16 type, uint16 len)
{
	wl_nan_tlv_data_t *ndata = ((wl_nan_tlv_data_t *)(ctx));
	bcm_xtlv_t *ptlv;
	struct ether_addr mac;
	int ret = BCME_OK;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint8 uv8;
	char buf[64];

	WL_DBG((" enter, xtlv_type: 0x%x \n", type));

	switch (type) {
	case WL_NAN_XTLV_ENABLE:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_ENABLE,
			sizeof(uv8), &ndata->enabled);
		break;
	case WL_NAN_XTLV_MASTER_PREF:
		/*
		 * master role and preference  mac has them as two u8's,
		 *
		 * masterpref: val & 0x0ff
		 * rnd_factor: val >> 8
		 */
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_MASTER_PREF,
			sizeof(uint16), &ndata->master_pref);
		break;
	case WL_NAN_XTLV_IF_ADDR:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_IF_ADDR,
			ETHER_ADDR_LEN, &mac);
		memcpy(&ndata->mac_addr, &mac, ETHER_ADDR_LEN);
		break;
	case WL_NAN_XTLV_CLUSTER_ID:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_CLUSTER_ID,
			ETHER_ADDR_LEN, &mac);
		memcpy(&ndata->clus_id, &mac, ETHER_ADDR_LEN);
		break;
	case WL_NAN_XTLV_ROLE:
		/*  nan device role, master, master-sync nosync etc  */
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_ROLE, 4,
			&ndata->dev_role);
		break;
	case WL_NAN_XTLV_MAC_CHANSPEC:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_MAC_CHANSPEC,
			sizeof(chanspec_t), &ndata->chanspec);
		if (wf_chspec_valid(ndata->chanspec)) {
			wf_chspec_ntoa(ndata->chanspec, buf);
			WL_DBG((" chanspec: %s 0x%x \n", buf, ndata->chanspec));
		} else {
			WL_DBG((" chanspec: 0x%x is not valid \n", ndata->chanspec));
		}
		break;
	case WL_NAN_XTLV_MAC_AMR:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_MAC_AMR,
			NAN_MASTER_RANK_LEN, buf);
		memcpy(ndata->amr, buf, NAN_MASTER_RANK_LEN);
		break;
	case WL_NAN_XTLV_MAC_AMBTT:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_MAC_AMBTT,
			sizeof(uint32), &ndata->ambtt);
		break;
	case WL_NAN_XTLV_MAC_HOPCNT:
		ret = bcm_unpack_xtlv_entry(tlv_buf,
			WL_NAN_XTLV_MAC_HOPCNT, sizeof(uint8), &ndata->hop_count);
		break;
	case WL_NAN_XTLV_INSTANCE_ID:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_INSTANCE_ID,
			sizeof(wl_nan_instance_id_t), &ndata->inst_id);
		break;
	case WL_NAN_XTLV_SVC_NAME:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_SVC_NAME,
			WL_NAN_SVC_HASH_LEN, ndata->svc_name);
		break;
	case WL_NAN_XTLV_SVC_PARAMS:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_SVC_PARAMS,
			sizeof(wl_nan_disc_params_t), &ndata->params);
		break;
	case WL_NAN_XTLV_MAC_STATUS:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_MAC_STATUS,
			sizeof(wl_nan_status_t), &ndata->nstatus);
		break;
	case WL_NAN_XTLV_PUBLR_ID:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_PUBLR_ID,
			sizeof(uint8), &ndata->pub_id);
		break;
	case WL_NAN_XTLV_SUBSCR_ID:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_SUBSCR_ID,
			sizeof(uint8), &ndata->sub_id);
		break;
	case WL_NAN_XTLV_MAC_ADDR:
		ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_MAC_ADDR,
			ETHER_ADDR_LEN, &mac);
		memcpy(&ndata->mac_addr, &mac, ETHER_ADDR_LEN);
		break;
	case WL_NAN_XTLV_VNDR:
		ptlv = *tlv_buf;
		ndata->vend_info.dlen = BCM_XTLV_LEN(ptlv);
		ndata->vend_info.data = kzalloc(ndata->vend_info.dlen, kflags);
		if (!ndata->vend_info.data) {
			WL_ERR((" memory allocation failed \n"));
			ret = -ENOMEM;
			goto fail;
		}
		if (ndata->vend_info.data && ndata->vend_info.dlen) {
			ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_VNDR,
				ndata->vend_info.dlen, ndata->vend_info.data);
		}
		break;
	case WL_NAN_XTLV_SVC_INFO:
		ptlv = *tlv_buf;
		ndata->svc_info.dlen = BCM_XTLV_LEN(ptlv);
		ndata->svc_info.data = kzalloc(ndata->svc_info.dlen, kflags);
		if (!ndata->svc_info.data) {
			WL_ERR((" memory allocation failed \n"));
			ret = -ENOMEM;
			goto fail;
		}
		if (ndata->svc_info.data && ndata->svc_info.dlen) {
			ret = bcm_unpack_xtlv_entry(tlv_buf, WL_NAN_XTLV_SVC_INFO,
				ndata->svc_info.dlen, ndata->svc_info.data);
		}
		break;
	case WL_NAN_XTLV_ZERO:
		/* don't parse empty space in the buffer */
		ret = BCME_ERROR;
		break;

	default:
		/* skip current tlv, if we don't have a handler */
		ret = bcm_skip_xtlv(tlv_buf);
		break;
	}

fail:
	return ret;
}

int
wl_cfgnan_enable_events(struct net_device *ndev, struct bcm_cfg80211 *cfg)
{
	wl_nan_ioc_t *nanioc = NULL;
	void *pxtlv;
	u32 event_mask = 0;
	s32 ret = BCME_OK;
	u16 start, end;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	ret = wl_add_remove_eventmsg(ndev, WLC_E_NAN, true);
	if (unlikely(ret)) {
		WL_ERR((" nan event enable failed, error = %d \n", ret));
		goto fail;
	}
	if (g_nan_debug) {
		/* enable all nan events */
		event_mask = NAN_EVENT_MASK_ALL;
	} else {
		/* enable only selected nan events to avoid unnecessary host wake up */
		event_mask |= NAN_EVENT_BIT(WL_NAN_EVENT_START);
		event_mask |= NAN_EVENT_BIT(WL_NAN_EVENT_JOIN);
		event_mask |= NAN_EVENT_BIT(WL_NAN_EVENT_DISCOVERY_RESULT);
		event_mask |= NAN_EVENT_BIT(WL_NAN_EVENT_RECEIVE);
		event_mask |= NAN_EVENT_BIT(WL_NAN_EVENT_TERMINATED);
		event_mask |= NAN_EVENT_BIT(WL_NAN_EVENT_STOP);
		event_mask |= NAN_EVENT_BIT(WL_NAN_EVENT_CLEAR_BIT);
		event_mask = htod32(event_mask);
	}

	start = end = NAN_IOCTL_BUF_SIZE;
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_EVENT_MASK);
	pxtlv = nanioc->data;
	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_EVENT_MASK,
		sizeof(uint32), &event_mask);
	if (unlikely(ret)) {
		goto fail;
	}
	nanioc->len = start - end;
	nanioc_size = sizeof(wl_nan_ioc_t) + nanioc->len;
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan event selective enable failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan event selective enable successful \n"));
	}

	ret = wl_add_remove_eventmsg(ndev, WLC_E_PROXD, true);
	if (unlikely(ret)) {
		WL_ERR((" proxd event enable failed, error = %d \n", ret));
		goto fail;
	}

fail:
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_start_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	struct ether_addr cluster_id = ether_null;
	void *pxtlv;
	s32 ret = BCME_OK;
	u16 start, end;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;
	uint8 val;

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan 1
	 *     wl nan join -start
	 *
	 * wpa_cli: DRIVER NAN_START
	 */

	/* nan enable */
	val = 1;
	start = end = NAN_IOCTL_BUF_SIZE;
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_ENABLE);
	pxtlv = nanioc->data;
	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_ENABLE,
		sizeof(uint8), &val);
	if (unlikely(ret)) {
		goto fail;
	}
	nanioc->len = start - end;
	nanioc_size = sizeof(wl_nan_ioc_t) + nanioc->len;
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan enable failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan enable successful \n"));
	}

	/* enable nan events */
	ret = wl_cfgnan_enable_events(ndev, cfg);
	if (unlikely(ret)) {
		goto fail;
	}

	/* nan join */
	val = 1;
	memset(nanioc, 0, nanioc_size);
	start = end = NAN_IOCTL_BUF_SIZE;
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_NAN_JOIN);
	pxtlv = nanioc->data;
	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_CLUSTER_ID,
		ETHER_ADDR_LEN, &cluster_id);
	if (unlikely(ret)) {
		goto fail;
	}
	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_START,
		sizeof(uint8), &val);
	if (unlikely(ret)) {
		goto fail;
	}
	nanioc->len = start - end;
	nanioc_size = sizeof(wl_nan_ioc_t) + nanioc->len;
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan join failed, error = %d \n", ret));
		goto fail;
	}

	WL_DBG((" nan join successful \n"));
	cfg->nan_running = true;

fail:
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_stop_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	struct ether_addr cluster_id = ether_null;
	void *pxtlv;
	s32 ret = BCME_OK;
	u16 start, end;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;
	uint8 nan_enable = FALSE;

	if (cfg->nan_running == false) {
		WL_DBG((" nan not running, do nothing \n"));
		return BCME_OK;
	}

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan stop
	 *     wl nan 0
	 *
	 * wpa_cli: DRIVER NAN_STOP
	 */

	/* nan stop */

	start = end = NAN_IOCTL_BUF_SIZE;
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_STOP);
	pxtlv = nanioc->data;
	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_CLUSTER_ID,
		ETHER_ADDR_LEN, &cluster_id);
	if (unlikely(ret)) {
		goto fail;
	}
	nanioc->len = start - end;
	nanioc_size = sizeof(wl_nan_ioc_t) + nanioc->len;
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan stop failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan stop successful \n"));
	}

	/* nan disable */
	memset(nanioc, 0, nanioc_size);
	start = end = NAN_IOCTL_BUF_SIZE;
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_ENABLE);
	pxtlv = nanioc->data;
	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_ENABLE,
		sizeof(uint8), &nan_enable);
	if (unlikely(ret)) {
		goto fail;
	}
	nanioc->len = start - end;
	nanioc_size = sizeof(wl_nan_ioc_t) + nanioc->len;
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan disable failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan disable successful \n"));
	}

fail:
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_support_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	void *pxtlv;
	s32 ret = BCME_OK;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan
	 *
	 * wpa_cli: DRIVER NAN_SUPPORT
	 */

	/* nan support */
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_ENABLE);
	pxtlv = nanioc->data;
	nanioc->len = htod16(BCM_XTLV_HDR_SIZE + 1);
	nanioc_size = sizeof(wl_nan_ioc_t) + sizeof(bcm_xtlv_t);
	ret = wldev_iovar_getbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_SMLEN, &cfg->ioctl_buf_sync);
	if (unlikely(ret)) {
		WL_ERR((" nan is not supported, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan is supported \n"));
	}

fail:
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_status_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL, *nanioc_res = NULL;
	char *ptr = cmd;
	wl_nan_tlv_data_t tlv_data;
	s32 ret = BCME_OK;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan status
	 *
	 * wpa_cli: DRIVER NAN_STATUS
	 */

	/* nan status */
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_STATUS);
	nanioc->len = NAN_IOCTL_BUF_SIZE;
	nanioc_size = sizeof(wl_nan_ioc_t) + sizeof(bcm_xtlv_t);
	ret = wldev_iovar_getbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan status failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan status successful \n"));
	}

	/* unpack the tlvs */
	memset(&tlv_data, 0, sizeof(tlv_data));
	nanioc_res = (wl_nan_ioc_t *)cfg->ioctl_buf;
	if (!nanioc_res) {
		goto fail;
	}
	if (g_nan_debug) {
		prhex(" nanioc->data: ", (uint8 *)nanioc_res->data, nanioc_res->len);
	}
	bcm_unpack_xtlv_buf(&tlv_data, nanioc_res->data, nanioc_res->len,
		wl_cfgnan_set_vars_cbfn);

	ptr += sprintf(ptr, CLUS_ID_PREFIX MACF, ETHER_TO_MACF(tlv_data.clus_id));
	ptr += sprintf(ptr, " " ROLE_PREFIX"%d", tlv_data.dev_role);
	ptr += sprintf(ptr, " " AMR_PREFIX);
	ptr += bcm_format_hex(ptr, tlv_data.amr, NAN_MASTER_RANK_LEN);
	ptr += sprintf(ptr, " " AMBTT_PREFIX"0x%x", tlv_data.ambtt);
	ptr += sprintf(ptr, " " HOP_COUNT_PREFIX"%d", tlv_data.hop_count);

	WL_DBG((" formatted string for userspace: %s, len: %zu \n",
		cmd, strlen(cmd)));

fail:
	if (nanioc) {
		kfree(nanioc);
	}
	if (tlv_data.svc_info.data) {
		kfree(tlv_data.svc_info.data);
		tlv_data.svc_info.data = NULL;
		tlv_data.svc_info.dlen = 0;
	}
	if (tlv_data.vend_info.data) {
		kfree(tlv_data.vend_info.data);
		tlv_data.vend_info.data = NULL;
		tlv_data.vend_info.dlen = 0;
	}

	return ret;
}

int
wl_cfgnan_pub_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	struct bcm_tlvbuf *tbuf = NULL;
	wl_nan_disc_params_t params;
	s32 ret = BCME_OK;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 tbuf_size = BCM_XTLV_HDR_SIZE + sizeof(params);
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;
	void *pxtlv;
	u16 start, end;

	/*
	 * proceed only if mandatory arguments are present - publisher id,
	 * service hash
	 */
	if ((!cmd_data->pub_id) || (!cmd_data->svc_hash.data) ||
		(!cmd_data->svc_hash.dlen)) {
		WL_ERR((" mandatory arguments are not present \n"));
		return -EINVAL;
	}

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	tbuf = bcm_xtlv_buf_alloc(NULL, tbuf_size);
	if (!tbuf) {
		WL_ERR((" memory allocation failed \n"));
		ret = -ENOMEM;
		goto fail;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan publish 10 NAN123 -info <hex_string
	 *     wl nan publish 10 NAN123 -info <hex_string -period 1 -ttl 0xffffffff
	 *
	 * wpa_cli: DRIVER NAN_PUBLISH PUB_ID=10 SVC_HASH=NAN123
	 *          SVC_INFO=<hex_string>
	 *          DRIVER NAN_PUBLISH PUB_ID=10 SVC_HASH=NAN123
	 *          SVC_INFO=<hex_string> PUB_PR=1 PUB_INT=0xffffffff
	 */

	/* nan publish */
	start = end = NAN_IOCTL_BUF_SIZE;
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_PUBLISH);
	pxtlv = nanioc->data;

	/* disovery parameters */
	if (cmd_data->pub_pr) {
		params.period = cmd_data->pub_pr;
	} else {
		params.period = 1;
	}
	if (cmd_data->pub_int) {
		params.ttl = cmd_data->pub_int;
	} else {
		params.ttl = WL_NAN_TTL_UNTIL_CANCEL;
	}
	params.flags = WL_NAN_PUB_BOTH;
	params.instance_id = (wl_nan_instance_id_t)cmd_data->pub_id;
	memcpy((char *)params.svc_hash, cmd_data->svc_hash.data,
		cmd_data->svc_hash.dlen);
	ret = bcm_pack_xtlv_entry(&pxtlv,
		&end, WL_NAN_XTLV_SVC_PARAMS, sizeof(wl_nan_disc_params_t), &params);
	if (unlikely(ret)) {
		goto fail;
	}
	if (cmd_data->svc_info.data && cmd_data->svc_info.dlen) {
		WL_DBG((" optional svc_info present, pack it \n"));
		ret = bcm_pack_xtlv_entry_from_hex_string(&pxtlv,
			&end, WL_NAN_XTLV_SVC_INFO, cmd_data->svc_info.data);
		if (unlikely(ret)) {
			goto fail;
		}
	}

	nanioc->len = start - end;
	nanioc_size = sizeof(wl_nan_ioc_t) + nanioc->len;
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan publish failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan publish successful \n"));
	}

fail:
	if (tbuf) {
		bcm_xtlv_buf_free(NULL, tbuf);
	}
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_sub_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	struct bcm_tlvbuf *tbuf = NULL;
	wl_nan_disc_params_t params;
	s32 ret = BCME_OK;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;

	/*
	 * proceed only if mandatory arguments are present - subscriber id,
	 * service hash
	 */
	if ((!cmd_data->sub_id) || (!cmd_data->svc_hash.data) ||
		(!cmd_data->svc_hash.dlen)) {
		WL_ERR((" mandatory arguments are not present \n"));
		return -EINVAL;
	}

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	tbuf = bcm_xtlv_buf_alloc(NULL, BCM_XTLV_HDR_SIZE + sizeof(params));
	if (!tbuf) {
		WL_ERR((" memory allocation failed \n"));
		ret = -ENOMEM;
		goto fail;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan subscribe 10 NAN123
	 *
	 * wpa_cli: DRIVER NAN_SUBSCRIBE SUB_ID=10 SVC_HASH=NAN123
	 */

	/* nan subscribe */
	params.period = 1;
	params.ttl = WL_NAN_TTL_UNTIL_CANCEL;
	params.flags = 0;
	params.instance_id = (wl_nan_instance_id_t)cmd_data->sub_id;
	memcpy((char *)params.svc_hash, cmd_data->svc_hash.data,
		cmd_data->svc_hash.dlen);
	bcm_xtlv_put_data(tbuf, WL_NAN_XTLV_SVC_PARAMS, &params, sizeof(params));

	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_SUBSCRIBE);
	nanioc->len = htod16(bcm_xtlv_buf_len(tbuf));
	bcopy(bcm_xtlv_head(tbuf), nanioc->data, bcm_xtlv_buf_len(tbuf));
	nanioc_size = sizeof(wl_nan_ioc_t) + bcm_xtlv_buf_len(tbuf);
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan subscribe failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan subscribe successful \n"));
	}

fail:
	if (tbuf) {
		bcm_xtlv_buf_free(NULL, tbuf);
	}
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_cancel_pub_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	struct bcm_tlvbuf *tbuf = NULL;
	wl_nan_disc_params_t params;
	s32 ret = BCME_OK;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;

	/* proceed only if mandatory argument is present - publisher id */
	if (!cmd_data->pub_id) {
		WL_ERR((" mandatory argument is not present \n"));
		return -EINVAL;
	}

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	tbuf = bcm_xtlv_buf_alloc(NULL, BCM_XTLV_HDR_SIZE + sizeof(params));
	if (!tbuf) {
		WL_ERR((" memory allocation failed \n"));
		ret = -ENOMEM;
		goto fail;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan cancel_publish 10
	 *
	 * wpa_cli: DRIVER NAN_CANCEL_PUBLISH PUB_ID=10
	 */

	bcm_xtlv_put_data(tbuf, WL_NAN_XTLV_INSTANCE_ID, &cmd_data->pub_id,
		sizeof(wl_nan_instance_id_t));

	/* nan cancel publish */
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_CANCEL_PUBLISH);
	nanioc->len = htod16(bcm_xtlv_buf_len(tbuf));
	bcopy(bcm_xtlv_head(tbuf), nanioc->data, bcm_xtlv_buf_len(tbuf));
	nanioc_size = sizeof(wl_nan_ioc_t) + bcm_xtlv_buf_len(tbuf);
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan cancel publish failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan cancel publish successful \n"));
	}

fail:
	if (tbuf) {
		bcm_xtlv_buf_free(NULL, tbuf);
	}
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_cancel_sub_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	struct bcm_tlvbuf *tbuf = NULL;
	wl_nan_disc_params_t params;
	s32 ret = BCME_OK;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;

	/* proceed only if mandatory argument is present - subscriber id */
	if (!cmd_data->sub_id) {
		WL_ERR((" mandatory argument is not present \n"));
		return -EINVAL;
	}

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	tbuf = bcm_xtlv_buf_alloc(NULL, BCM_XTLV_HDR_SIZE + sizeof(params));
	if (!tbuf) {
		WL_ERR((" memory allocation failed \n"));
		ret = -ENOMEM;
		goto fail;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan cancel_subscribe 10
	 *
	 * wpa_cli: DRIVER NAN_CANCEL_SUBSCRIBE PUB_ID=10
	 */

	bcm_xtlv_put_data(tbuf, WL_NAN_XTLV_INSTANCE_ID, &cmd_data->sub_id,
		sizeof(wl_nan_instance_id_t));

	/* nan cancel subscribe */
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_CANCEL_SUBSCRIBE);
	nanioc->len = htod16(bcm_xtlv_buf_len(tbuf));
	bcopy(bcm_xtlv_head(tbuf), nanioc->data, bcm_xtlv_buf_len(tbuf));
	nanioc_size = sizeof(wl_nan_ioc_t) + bcm_xtlv_buf_len(tbuf);
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan cancel subscribe failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan cancel subscribe successful \n"));
	}

fail:
	if (tbuf) {
		bcm_xtlv_buf_free(NULL, tbuf);
	}
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_transmit_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	void *pxtlv;
	s32 ret = BCME_OK;
	u16 start, end;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;

	/*
	 * proceed only if mandatory arguments are present - subscriber id,
	 * publisher id, mac address
	 */
	if ((!cmd_data->sub_id) || (!cmd_data->pub_id) ||
		ETHER_ISNULLADDR(&cmd_data->mac_addr.octet)) {
		WL_ERR((" mandatory arguments are not present \n"));
		return -EINVAL;
	}

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan trasnmit <sub_id> <pub_id> <mac_addr> -info <hex_string>
	 *
	 * wpa_cli: DRIVER NAN_TRANSMIT SUB_ID=<sub_id> PUB_ID=<pub_id>
	 *          MAC_ADDR=<mac_addr> SVC_INFO=<hex_string>
	 */

	/* nan transmit */
	start = end = NAN_IOCTL_BUF_SIZE;
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_TRANSMIT);
	pxtlv = nanioc->data;

	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_INSTANCE_ID,
		sizeof(wl_nan_instance_id_t), &cmd_data->sub_id);
	if (unlikely(ret)) {
		goto fail;
	}
	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_REQUESTOR_ID,
		sizeof(wl_nan_instance_id_t), &cmd_data->pub_id);
	if (unlikely(ret)) {
		goto fail;
	}
	ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_MAC_ADDR,
		ETHER_ADDR_LEN, &cmd_data->mac_addr.octet);
	if (unlikely(ret)) {
		goto fail;
	}
	if (cmd_data->svc_info.data && cmd_data->svc_info.dlen) {
		WL_DBG((" optional svc_info present, pack it \n"));
		ret = bcm_pack_xtlv_entry_from_hex_string(&pxtlv,
			&end, WL_NAN_XTLV_SVC_INFO, cmd_data->svc_info.data);
		if (unlikely(ret)) {
			goto fail;
		}
	}

	nanioc->len = start - end;
	nanioc_size = sizeof(wl_nan_ioc_t) + nanioc->len;
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan transmit failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan transmit successful \n"));
	}

fail:
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_set_config_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ioc_t *nanioc = NULL;
	void *pxtlv;
	s32 ret = BCME_OK;
	u16 start, end;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	uint16 nanioc_size = sizeof(wl_nan_ioc_t) + NAN_IOCTL_BUF_SIZE;

	nanioc = kzalloc(nanioc_size, kflags);
	if (!nanioc) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	/*
	 * command to test
	 *
	 * wl: wl nan <attr> <value> (wl nan role 1)
	 *
	 * wpa_cli: DRIVER NAN_CONFIG_SET ATTR=<attr> <value>...<value>
	 *
	 * wpa_cli: DRIVER NAN_SET_CONFIG ATTR=ATTR_ROLE ROLE=1
	 */

	/* nan set config */
	start = end = NAN_IOCTL_BUF_SIZE;
	nanioc->version = htod16(WL_NAN_IOCTL_VERSION);
	nanioc->id = htod16(WL_NAN_CMD_ATTR);
	pxtlv = nanioc->data;

	switch (cmd_data->attr.type) {
	case WL_NAN_XTLV_ROLE:
		ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_ROLE,
			sizeof(u32), &cmd_data->role);
		break;
	case WL_NAN_XTLV_MASTER_PREF:
		ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_MASTER_PREF,
			sizeof(u16), &cmd_data->master_pref);
		break;
	case WL_NAN_XTLV_DW_LEN:
		ret = bcm_pack_xtlv_entry(&pxtlv, &end, WL_NAN_XTLV_DW_LEN,
			sizeof(u16), &cmd_data->dw_len);
		break;
	case WL_NAN_XTLV_CLUSTER_ID:
	case WL_NAN_XTLV_IF_ADDR:
	case WL_NAN_XTLV_BCN_INTERVAL:
	case WL_NAN_XTLV_MAC_CHANSPEC:
	case WL_NAN_XTLV_MAC_TXRATE:
	default:
		ret = -EINVAL;
		break;
	}
	if (unlikely(ret)) {
		WL_ERR((" unsupported attribute, attr = %s (%d) \n",
			cmd_data->attr.name, cmd_data->attr.type));
		goto fail;
	}

	nanioc->len = start - end;
	nanioc_size = sizeof(wl_nan_ioc_t) + nanioc->len;
	ret = wldev_iovar_setbuf(ndev, "nan", nanioc, nanioc_size,
		cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan set config failed, error = %d \n", ret));
		goto fail;
	} else {
		WL_DBG((" nan set config successful \n"));
	}

fail:
	if (nanioc) {
		kfree(nanioc);
	}

	return ret;
}

int
wl_cfgnan_rtt_config_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	wl_nan_ranging_config_t rtt_config;
	s32 ret = BCME_OK;

	/* proceed only if mandatory argument is present - channel */
	if (!cmd_data->chanspec) {
		WL_ERR((" mandatory argument is not present \n"));
		return -EINVAL;
	}

	/*
	 * command to test
	 *
	 * wl: wl proxd_nancfg 44/80 128 32 ff:ff:ff:ff:ff:ff 1
	 *
	 * wpa_cli: DRIVER NAN_RTT_CONFIG CHAN=44/80
	 */

	memset(&rtt_config, 0, sizeof(wl_nan_ranging_config_t));
	rtt_config.chanspec = cmd_data->chanspec;
	rtt_config.timeslot = 128;
	rtt_config.duration = 32;
	memcpy(&rtt_config.allow_mac, &ether_bcast, ETHER_ADDR_LEN);
	rtt_config.flags = 1;

	ret = wldev_iovar_setbuf(ndev, "proxd_nancfg", &rtt_config,
		sizeof(wl_nan_ranging_config_t), cfg->ioctl_buf,
		WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan rtt config failed, error = %d \n", ret));
	} else {
		WL_DBG((" nan rtt config successful \n"));
	}

	return ret;
}

int
wl_cfgnan_rtt_find_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	void *iovbuf;
	wl_nan_ranging_list_t *rtt_list;
	s32 iovbuf_size = NAN_RTT_IOVAR_BUF_SIZE;
	s32 ret = BCME_OK;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	/*
	 * proceed only if mandatory arguments are present - channel, bitmap,
	 * mac address
	 */
	if ((!cmd_data->chanspec) || (!cmd_data->bmap) ||
		ETHER_ISNULLADDR(&cmd_data->mac_addr.octet)) {
		WL_ERR((" mandatory arguments are not present \n"));
		return -EINVAL;
	}

	iovbuf = kzalloc(iovbuf_size, kflags);
	if (!iovbuf) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	/*
	 * command to test
	 *
	 * wl: wl proxd_nanfind 1 44/80 <mac_addr> 0x300 5 6 1
	 *
	 * wpa_cli: DRIVER NAN_RTT_FIND MAC_ADDR=<mac_addr> CHAN=44/80 BMAP=0x300
	 *
	 */
	rtt_list = (wl_nan_ranging_list_t *)iovbuf;
	rtt_list->count = 1;
	rtt_list->num_peers_done = 0;
	rtt_list->num_dws = 1;
	rtt_list->rp[0].chanspec = cmd_data->chanspec;
	memcpy(&rtt_list->rp[0].ea, &cmd_data->mac_addr,
		sizeof(struct ether_addr));
	rtt_list->rp[0].abitmap = cmd_data->bmap;
	rtt_list->rp[0].frmcnt = 5;
	rtt_list->rp[0].retrycnt = 6;
	rtt_list->rp[0].flags = 1;

	iovbuf_size = sizeof(wl_nan_ranging_list_t) +
		sizeof(wl_nan_ranging_peer_t);
	ret = wldev_iovar_setbuf(ndev, "proxd_nanfind", iovbuf,
		iovbuf_size, cfg->ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (unlikely(ret)) {
		WL_ERR((" nan rtt find failed, error = %d \n", ret));
	} else {
		WL_DBG((" nan rtt find successful \n"));
	}

	if (iovbuf) {
		kfree(iovbuf);
	}

	return ret;
}

int
wl_cfgnan_debug_handler(struct net_device *ndev,
	struct bcm_cfg80211 *cfg, char *cmd, nan_cmd_data_t *cmd_data)
{
	/*
	 * command to test
	 *
	 * wpa_cli: DRIVER NAN_DEBUG DEBUG=1
	 *
	 */

	g_nan_debug = cmd_data->debug_flag;

	/* reconfigure nan events */
	return wl_cfgnan_enable_events(ndev, cfg);
}

static int wl_cfgnan_config_attr(char *buf, nan_config_attr_t *attr)
{
	s32 ret = BCME_OK;
	nan_config_attr_t *nanc = NULL;

	/* only one attribute at a time */
	for (nanc = nan_config_attrs; nanc->name[0]; nanc++) {
		if (!strncmp(nanc->name, buf, strlen(nanc->name))) {
			strncpy((char *)attr->name, buf, strlen(nanc->name));
			attr->type = nanc->type;
			ret = strlen(nanc->name);
			break;
		}
	}

	return ret;
}

static int wl_cfgnan_parse_args(char *buf, nan_cmd_data_t *cmd_data)
{
	s32 ret = BCME_OK;
	char *token = buf;
	char delim[] = " ";

	while ((buf != NULL) && (token != NULL)) {
		if (!strncmp(buf, PUB_ID_PREFIX, strlen(PUB_ID_PREFIX))) {
			buf += strlen(PUB_ID_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->pub_id = simple_strtoul(token, NULL, 10);
			if (NAN_INVALID_ID(cmd_data->pub_id)) {
				WL_ERR((" invalid publisher id, pub_id = %d \n",
					cmd_data->pub_id));
				ret = -EINVAL;
				goto fail;
			}
		} else if (!strncmp(buf, SUB_ID_PREFIX, strlen(SUB_ID_PREFIX))) {
			buf += strlen(SUB_ID_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->sub_id = simple_strtoul(token, NULL, 10);
			if (NAN_INVALID_ID(cmd_data->sub_id)) {
				WL_ERR((" invalid subscriber id, sub_id = %d \n",
					cmd_data->sub_id));
				ret = -EINVAL;
				goto fail;
			}
		} else if (!strncmp(buf, MAC_ADDR_PREFIX, strlen(MAC_ADDR_PREFIX))) {
			buf += strlen(MAC_ADDR_PREFIX);
			token = strsep(&buf, delim);
			if (!wl_cfg80211_ether_atoe(token, &cmd_data->mac_addr)) {
				WL_ERR((" invalid mac address, mac_addr = "MACDBG "\n",
					MAC2STRDBG(cmd_data->mac_addr.octet)));
				ret = -EINVAL;
				goto fail;
			}
		} else if (!strncmp(buf, SVC_HASH_PREFIX, strlen(SVC_HASH_PREFIX))) {
			buf += strlen(SVC_HASH_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->svc_hash.data = token;
			cmd_data->svc_hash.dlen = WL_NAN_SVC_HASH_LEN;
		} else if (!strncmp(buf, SVC_INFO_PREFIX, strlen(SVC_INFO_PREFIX))) {
			buf += strlen(SVC_INFO_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->svc_info.data = token;
			cmd_data->svc_info.dlen = strlen(token);
		} else if (!strncmp(buf, CHAN_PREFIX, strlen(CHAN_PREFIX))) {
			buf += strlen(CHAN_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->chanspec = wf_chspec_aton(token);
			cmd_data->chanspec = wl_chspec_host_to_driver(cmd_data->chanspec);
			if (NAN_INVALID_CHANSPEC(cmd_data->chanspec)) {
				WL_ERR((" invalid chanspec, chanspec = 0x%04x \n",
					cmd_data->chanspec));
				ret = -EINVAL;
				goto fail;
			}
		} else if (!strncmp(buf, BITMAP_PREFIX, strlen(BITMAP_PREFIX))) {
			buf += strlen(BITMAP_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->bmap = simple_strtoul(token, NULL, 16);
		} else if (!strncmp(buf, ATTR_PREFIX, strlen(ATTR_PREFIX))) {
			buf += strlen(ATTR_PREFIX);
			token = strsep(&buf, delim);
			if (!wl_cfgnan_config_attr(token, &cmd_data->attr)) {
				WL_ERR((" invalid attribute, attr = %s \n",
					cmd_data->attr.name));
				ret = -EINVAL;
				goto fail;
			}
		} else if (!strncmp(buf, ROLE_PREFIX, strlen(ROLE_PREFIX))) {
			buf += strlen(ROLE_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->role = simple_strtoul(token, NULL, 10);
			if (NAN_INVALID_ROLE(cmd_data->role)) {
				WL_ERR((" invalid role, role = %d \n", cmd_data->role));
				ret = -EINVAL;
				goto fail;
			}
		} else if (!strncmp(buf, MASTER_PREF_PREFIX,
			strlen(MASTER_PREF_PREFIX))) {
			buf += strlen(MASTER_PREF_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->master_pref = simple_strtoul(token, NULL, 10);
		} else if (!strncmp(buf, PUB_PR_PREFIX, strlen(PUB_PR_PREFIX))) {
			buf += strlen(PUB_PR_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->pub_pr = simple_strtoul(token, NULL, 10);
		} else if (!strncmp(buf, PUB_INT_PREFIX, strlen(PUB_INT_PREFIX))) {
			buf += strlen(PUB_INT_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->pub_int = simple_strtoul(token, NULL, 10);
		} else if (!strncmp(buf, DW_LEN_PREFIX, strlen(DW_LEN_PREFIX))) {
			buf += strlen(DW_LEN_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->dw_len = simple_strtoul(token, NULL, 10);
		} else if (!strncmp(buf, DEBUG_PREFIX, strlen(DEBUG_PREFIX))) {
			buf += strlen(DEBUG_PREFIX);
			token = strsep(&buf, delim);
			cmd_data->debug_flag = simple_strtoul(token, NULL, 10);
		} else {
			WL_ERR((" unknown token, token = %s, buf = %s \n", token, buf));
			ret = -EINVAL;
			goto fail;
		}
	}

fail:
	return ret;
}

int
wl_cfgnan_cmd_handler(struct net_device *ndev, struct bcm_cfg80211 *cfg,
	char *cmd, int cmd_len)
{
	nan_cmd_data_t cmd_data;
	u8 *buf = cmd;
	u8 *cmd_name = NULL;
	nan_cmd_t *nanc = NULL;
	int buf_len = 0;
	int ret = BCME_OK;

	cmd_name = strsep((char **)&buf, " ");
	if (!cmd_name) {
		return BCME_ERROR;
	}
	if (buf) {
		buf_len = strlen(buf);
	}

	WL_DBG((" cmd_name: %s, buf_len: %d, buf: %s \n", cmd_name, buf_len, buf));

	memset(&cmd_data, 0, sizeof(nan_cmd_data_t));
	ret = wl_cfgnan_parse_args(buf, &cmd_data);
	if (unlikely(ret)) {
		WL_ERR((" argument parsing failed with error (%d), buf = %s \n",
			ret, buf));
		goto fail;
	}

	for (nanc = nan_cmds; nanc->name; nanc++) {
		if (strncmp(nanc->name, cmd_name, strlen(nanc->name)) == 0) {
			ret = (*nanc->func)(ndev, cfg, cmd, &cmd_data);
			if (ret < BCME_OK) {
				WL_ERR((" command (%s) failed with error (%d) \n",
					cmd_name, ret));
			}
		}
	}

fail:
	return ret;
}

s32
wl_cfgnan_notify_proxd_status(struct bcm_cfg80211 *cfg,
	bcm_struct_cfgdev *cfgdev, const wl_event_msg_t *event, void *data)
{
	s32 ret = BCME_OK;
	wl_nan_ranging_event_data_t *rdata;
	s32 status;
	u16 data_len;
	s32 event_type;
	s32 event_num;
	u8 *buf = NULL;
	u32 buf_len;
	u8 *ptr;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	s32 i;

	if (!event || !data) {
		WL_ERR((" event data is NULL \n"));
		return -EINVAL;
	}

	status = ntoh32(event->reason);
	event_type = ntoh32(event->event_type);
	event_num = ntoh32(event->reason);
	data_len = ntoh32(event->datalen);

	WL_DBG((" proxd event: type: %d num: %d len: %d \n",
		event_type, event_num, data_len));

	if (NAN_INVALID_PROXD_EVENT(event_num)) {
		WL_ERR((" unsupported event, num: %d \n", event_num));
		return -EINVAL;
	}

	if (g_nan_debug) {
		WL_DBG((" event name: WLC_E_PROXD_NAN_EVENT \n"));
		WL_DBG((" event data: \n"));
		prhex(NULL, data, data_len);
	}

	if (data_len < sizeof(wl_nan_ranging_event_data_t)) {
		WL_ERR((" wrong data len \n"));
		return -EINVAL;
	}

	rdata = (wl_nan_ranging_event_data_t *)data;

	WL_DBG((" proxd event: count:%d success_count:%d mode:%d \n",
		rdata->count, rdata->success_count, rdata->mode));

	if (g_nan_debug) {
		prhex(" event data: ", data, data_len);
	}

	buf_len = NAN_IOCTL_BUF_SIZE;
	buf = kzalloc(buf_len, kflags);
	if (!buf) {
		WL_ERR((" memory allocation failed \n"));
		return -ENOMEM;
	}

	for (i = 0; i < rdata->count; i++) {
		if (&rdata->rr[i] == NULL) {
			ret = -EINVAL;
			goto fail;
		}

		ptr = buf;
		WL_DBG((" ranging data for mac:"MACDBG" \n",
			MAC2STRDBG(rdata->rr[i].ea.octet)));
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " MAC_ADDR_PREFIX MACF
			" "STATUS_PREFIX"%s", EVENT_RTT_STATUS_STR,
			ETHER_TO_MACF(rdata->rr[i].ea), (rdata->rr[i].status == 1) ?
			"success" : "fail");

		if (rdata->rr[i].status == 1) {
			/* add tsf and distance only if status is success */
			ptr += sprintf(ptr, " "TIMESTAMP_PREFIX"0x%x "
				DISTANCE_PREFIX"%d.%04d", rdata->rr[i].timestamp,
				rdata->rr[i].distance >> 4,
				((rdata->rr[i].distance & 0x0f) * 625));
		}

#ifdef WL_GENL
		/* send the preformatted string to the upper layer as event */
		WL_DBG((" formatted string for userspace: %s, len: %zu \n",
			buf, strlen(buf)));
		wl_genl_send_msg(bcmcfg_to_prmry_ndev(cfg), 0, buf, strlen(buf), 0, 0);
#endif /* WL_GENL */
	}

fail:
	if (buf) {
		kfree(buf);
	}

	return ret;
}

s32
wl_cfgnan_notify_nan_status(struct bcm_cfg80211 *cfg,
	bcm_struct_cfgdev *cfgdev, const wl_event_msg_t *event, void *data)
{
	s32 ret = BCME_OK;
	u16 data_len;
	u32 event_num;
	s32 event_type;
	nan_event_hdr_t nan_hdr;
	wl_nan_tlv_data_t tlv_data;
	u8 *buf = NULL;
	u32 buf_len;
	u8 *ptr;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	if (!event || !data) {
		WL_ERR((" event data is NULL \n"));
		return -EINVAL;
	}

	event_type = ntoh32(event->event_type);
	event_num = ntoh32(event->reason);
	data_len = ntoh32(event->datalen);
	memset(&nan_hdr, 0, sizeof(nan_event_hdr_t));
	nan_hdr.event_subtype = event_num;

	WL_DBG((" nan event: type: %d num: %d len: %d \n",
		event_type, event_num, data_len));

	if (NAN_INVALID_EVENT(event_num)) {
		WL_ERR((" unsupported event, num: %d \n", event_num));
		return -EINVAL;
	}

	if (g_nan_debug) {
		WL_DBG((" event name: %s \n", nan_event_name[event_num]));
		WL_DBG((" event data: \n"));
		prhex(NULL, data, data_len);
	}

	/* unpack the tlvs */
	memset(&tlv_data, 0, sizeof(wl_nan_tlv_data_t));
	bcm_unpack_xtlv_buf(&tlv_data, data, data_len,
		wl_cfgnan_set_vars_cbfn);

	/*
	 * send as preformatted hex string
	 *
	 * EVENT_NAN <event_type> <tlv_hex_string>
	 */

	buf_len = NAN_IOCTL_BUF_SIZE;
	buf = ptr = kzalloc(buf_len, kflags);
	if (!buf) {
		WL_ERR((" memory allocation failed \n"));
		ret = -ENOMEM;
		goto fail;
	}

	switch (event_num) {
	case WL_NAN_EVENT_START:
	case WL_NAN_EVENT_JOIN:
	case WL_NAN_EVENT_STOP:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " CLUS_ID_PREFIX MACF,
			nan_event_str[event_num], ETHER_TO_MACF(tlv_data.nstatus.cid));
		break;
	case WL_NAN_EVENT_ROLE:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s "ROLE_PREFIX "%d "
			CLUS_ID_PREFIX MACF, nan_event_str[event_num],
			tlv_data.nstatus.role, ETHER_TO_MACF(tlv_data.nstatus.cid));
		break;
	case WL_NAN_EVENT_DISCOVERY_RESULT:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " PUB_ID_PREFIX"%d "
			SUB_ID_PREFIX"%d " MAC_ADDR_PREFIX MACF,
			nan_event_str[event_num], tlv_data.pub_id, tlv_data.sub_id,
			ETHER_TO_MACF(tlv_data.mac_addr));
		if (tlv_data.svc_info.data && tlv_data.svc_info.dlen) {
			WL_DBG((" service info present \n"));
			if ((strlen(ptr) + tlv_data.svc_info.dlen) >= buf_len) {
				WL_ERR((" service info length = %d\n",
					tlv_data.svc_info.dlen));
				WL_ERR((" insufficent buffer to copy service info \n"));
				ret = -EOVERFLOW;
				goto fail;
			}
			ptr += sprintf(ptr, " %s", SVC_INFO_PREFIX);
			ptr += bcm_format_hex(ptr, tlv_data.svc_info.data,
				tlv_data.svc_info.dlen);
		} else {
			WL_DBG((" service info not present \n"));
		}

		if (tlv_data.vend_info.data && tlv_data.vend_info.dlen) {
			struct ether_addr *ea;
			u8 *temp_data = tlv_data.vend_info.data;
			uint32 bitmap;
			u16 dlen = tlv_data.vend_info.dlen;
			chanspec_t chanspec;
			uint8 mapcontrol;
			uint8 proto;

			WL_DBG((" vendor info present \n"));
			if ((*temp_data != NAN_ATTR_VENDOR_SPECIFIC) ||
				(dlen < NAN_VENDOR_HDR_SIZE)) {
				WL_ERR((" error in vendor info attribute \n"));
				ret = -EINVAL;
				goto fail;
			} else {
				WL_DBG((" vendor info not present \n"));
			}

			if (*(temp_data + 6) == NAN_VENDOR_TYPE_RTT) {
				temp_data += NAN_VENDOR_HDR_SIZE;
				ea = (struct ether_addr *)temp_data;
				temp_data += ETHER_ADDR_LEN;
				mapcontrol = *temp_data++;
				proto = *temp_data++;
				bitmap = *(uint32 *)temp_data;
				temp_data += 4;
				chanspec = *(chanspec_t *)temp_data;
				ptr += sprintf(ptr, " "BITMAP_PREFIX"0x%x "CHAN_PREFIX"%d/%s",
					bitmap, wf_chspec_ctlchan(chanspec),
					wf_chspec_to_bw_str(chanspec));
				WL_DBG((" bitmap: 0x%x channel: %d bandwidth: %s \n", bitmap,
					wf_chspec_ctlchan(chanspec),
					wf_chspec_to_bw_str(chanspec)));
			}
		}
		break;
	case WL_NAN_EVENT_REPLIED:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " PUB_ID_PREFIX"%d "
				MAC_ADDR_PREFIX MACF, nan_event_str[event_num],
				tlv_data.pub_id, ETHER_TO_MACF(tlv_data.mac_addr));
		break;
	case WL_NAN_EVENT_TERMINATED:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " PUB_ID_PREFIX"%d ",
			nan_event_str[event_num], tlv_data.pub_id);
		break;
	case WL_NAN_EVENT_RECEIVE:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " PUB_ID_PREFIX"%d "
				MAC_ADDR_PREFIX MACF, nan_event_str[event_num],
				tlv_data.pub_id, ETHER_TO_MACF(tlv_data.mac_addr));
		if (tlv_data.svc_info.data && tlv_data.svc_info.dlen) {
			WL_DBG((" service info present \n"));
			if ((strlen(ptr) + tlv_data.svc_info.dlen) >= buf_len) {
				WL_ERR((" service info length = %d\n",
					tlv_data.svc_info.dlen));
				WL_ERR((" insufficent buffer to copy service info \n"));
				ret = -EOVERFLOW;
				goto fail;
			}
			ptr += sprintf(ptr, " %s", SVC_INFO_PREFIX);
			ptr += bcm_format_hex(ptr, tlv_data.svc_info.data,
				tlv_data.svc_info.dlen);
		} else {
			WL_DBG((" service info not present \n"));
		}
		break;
	case WL_NAN_EVENT_SCAN_COMPLETE:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " CLUS_ID_PREFIX MACF,
			nan_event_str[event_num], ETHER_TO_MACF(tlv_data.nstatus.cid));
		break;
	case WL_NAN_EVENT_STATUS_CHG:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " CLUS_ID_PREFIX MACF,
			nan_event_str[event_num], ETHER_TO_MACF(tlv_data.nstatus.cid));
		break;
	case WL_NAN_EVENT_MERGE:
		ptr += sprintf(buf, SUPP_EVENT_PREFIX"%s " CLUS_ID_PREFIX MACF,
			nan_event_str[event_num], ETHER_TO_MACF(tlv_data.nstatus.cid));
		break;
	default:
		WL_ERR((" unknown event \n"));
		break;
	}

#ifdef WL_GENL
	/* send the preformatted string to the upper layer as event */
	WL_DBG((" formatted string for userspace: %s, len: %zu \n",
		buf, strlen(buf)));
	wl_genl_send_msg(bcmcfg_to_prmry_ndev(cfg), 0, buf, strlen(buf), 0, 0);
#endif /* WL_GENL */

fail:
	if (buf) {
		kfree(buf);
	}
	if (tlv_data.svc_info.data) {
		kfree(tlv_data.svc_info.data);
		tlv_data.svc_info.data = NULL;
		tlv_data.svc_info.dlen = 0;
	}
	if (tlv_data.vend_info.data) {
		kfree(tlv_data.vend_info.data);
		tlv_data.vend_info.data = NULL;
		tlv_data.vend_info.dlen = 0;
	}

	return ret;
}
