/*
 * Broadcom Dongle Host Driver (DHD), common DHD core.
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
 * $Id: dhd_common.c 707373 2017-06-27 12:09:14Z $
 */
#include <typedefs.h>
#include <osl.h>

#include <epivers.h>
#include <bcmutils.h>

#include <bcmendian.h>
#include <dngl_stats.h>
#include <wlioctl.h>
#include <dhd.h>
#include <dhd_ip.h>
#include <proto/bcmevent.h>

#ifdef SHOW_LOGTRACE
#include <event_log.h>
#endif /* SHOW_LOGTRACE */

#ifdef BCMPCIE
#include <dhd_flowring.h>
#endif

#include <dhd_bus.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <msgtrace.h>

#ifdef WL_CFG80211
#include <wl_cfg80211.h>
#endif
#ifdef WLBTAMP
#include <proto/bt_amp_hci.h>
#include <dhd_bta.h>
#endif
#ifdef PNO_SUPPORT
#include <dhd_pno.h>
#endif
#ifdef RTT_SUPPORT
#include <dhd_rtt.h>
#endif

#define htod32(i) (i)
#define htod16(i) (i)
#define dtoh32(i) (i)
#define dtoh16(i) (i)
#define htodchanspec(i) (i)
#define dtohchanspec(i) (i)

#ifdef PROP_TXSTATUS
#include <wlfc_proto.h>
#include <dhd_wlfc.h>
#endif



int dhd_msg_level = DHD_ERROR_VAL;


#if defined(WL_WIRELESS_EXT)
#include <wl_iw.h>
#endif 

#ifdef SOFTAP
char fw_path2[MOD_PARAM_PATHLEN];
extern bool softap_enabled;
#endif

/* Last connection success/failure status */
uint32 dhd_conn_event;
uint32 dhd_conn_status;
uint32 dhd_conn_reason;

#if defined(SHOW_EVENTS) && defined(SHOW_LOGTRACE)
static int check_event_log_sequence_number(uint32 seq_no);
#endif /* defined(SHOW_EVENTS) && defined(SHOW_LOGTRACE) */
extern int dhd_iscan_request(void * dhdp, uint16 action);
extern void dhd_ind_scan_confirm(void *h, bool status);
extern int dhd_iscan_in_progress(void *h);
void dhd_iscan_lock(void);
void dhd_iscan_unlock(void);
extern int dhd_change_mtu(dhd_pub_t *dhd, int new_mtu, int ifidx);
#if !defined(AP) && defined(WLP2P)
extern int dhd_get_concurrent_capabilites(dhd_pub_t *dhd);
#endif

#define MAX_CHUNK_LEN 1408 /* 8 * 8 * 22 */

bool ap_cfg_running = FALSE;
bool ap_fw_loaded = FALSE;

/* Version string to report */
#ifdef DHD_DEBUG
#ifndef SRCBASE
#define SRCBASE        "drivers/net/wireless/bcmdhd"
#endif
#define DHD_COMPILED "\nCompiled in " SRCBASE
#endif /* DHD_DEBUG */

#define CHIPID_MISMATCH	8

#if defined(DHD_DEBUG)
const char dhd_version[] = "Dongle Host Driver, version " EPI_VERSION_STR
	DHD_COMPILED " on " __DATE__ " at " __TIME__;
#else
const char dhd_version[] = "\nDongle Host Driver, version " EPI_VERSION_STR "\nCompiled from ";
#endif 
char fw_version[FW_VER_STR_LEN] = "\0";
char clm_version[CLM_VER_STR_LEN] = "\0";

void dhd_set_timer(void *bus, uint wdtick);



/* IOVar table */
enum {
	IOV_VERSION = 1,
	IOV_MSGLEVEL,
	IOV_BCMERRORSTR,
	IOV_BCMERROR,
	IOV_WDTICK,
	IOV_DUMP,
	IOV_CLEARCOUNTS,
	IOV_LOGDUMP,
	IOV_LOGCAL,
	IOV_LOGSTAMP,
	IOV_GPIOOB,
	IOV_IOCTLTIMEOUT,
#ifdef WLBTAMP
	IOV_HCI_CMD,		/* HCI command */
	IOV_HCI_ACL_DATA,	/* HCI data packet */
#endif
#if defined(DHD_DEBUG)
	IOV_CONS,
	IOV_DCONSOLE_POLL,
#endif /* defined(DHD_DEBUG) */
#ifdef PROP_TXSTATUS
	IOV_PROPTXSTATUS_ENABLE,
	IOV_PROPTXSTATUS_MODE,
	IOV_PROPTXSTATUS_OPT,
	IOV_PROPTXSTATUS_MODULE_IGNORE,
	IOV_PROPTXSTATUS_CREDIT_IGNORE,
	IOV_PROPTXSTATUS_TXSTATUS_IGNORE,
	IOV_PROPTXSTATUS_RXPKT_CHK,
#endif /* PROP_TXSTATUS */
	IOV_BUS_TYPE,
	IOV_CHANGEMTU,
	IOV_HOSTREORDER_FLOWS,
#ifdef DHDTCPACK_SUPPRESS
	IOV_TCPACK_SUPPRESS,
#endif /* DHDTCPACK_SUPPRESS */
	IOV_AP_ISOLATE,
#ifdef DHD_UNICAST_DHCP
	IOV_DHCP_UNICAST,
#endif /* DHD_UNICAST_DHCP */
#ifdef DHD_L2_FILTER
	IOV_BLOCK_PING,
#endif
	IOV_LAST
};

const bcm_iovar_t dhd_iovars[] = {
	{"version",	IOV_VERSION,	0,	IOVT_BUFFER,	sizeof(dhd_version) },
#ifdef DHD_DEBUG
	{"msglevel",	IOV_MSGLEVEL,	0,	IOVT_UINT32,	0 },
#endif /* DHD_DEBUG */
	{"bcmerrorstr", IOV_BCMERRORSTR, 0, IOVT_BUFFER,	BCME_STRLEN },
	{"bcmerror",	IOV_BCMERROR,	0,	IOVT_INT8,	0 },
	{"wdtick",	IOV_WDTICK, 0,	IOVT_UINT32,	0 },
	{"dump",	IOV_DUMP,	0,	IOVT_BUFFER,	DHD_IOCTL_MAXLEN },
#ifdef DHD_DEBUG
	{"cons",	IOV_CONS,	0,	IOVT_BUFFER,	0 },
	{"dconpoll",	IOV_DCONSOLE_POLL, 0,	IOVT_UINT32,	0 },
#endif
	{"clearcounts", IOV_CLEARCOUNTS, 0, IOVT_VOID,	0 },
	{"gpioob",	IOV_GPIOOB,	0,	IOVT_UINT32,	0 },
	{"ioctl_timeout",	IOV_IOCTLTIMEOUT,	0,	IOVT_UINT32,	0 },
#ifdef WLBTAMP
	{"HCI_cmd",	IOV_HCI_CMD,	0,	IOVT_BUFFER,	0},
	{"HCI_ACL_data", IOV_HCI_ACL_DATA, 0,	IOVT_BUFFER,	0},
#endif
#ifdef PROP_TXSTATUS
	{"proptx",	IOV_PROPTXSTATUS_ENABLE,	0,	IOVT_BOOL,	0 },
	/*
	set the proptxtstatus operation mode:
	0 - Do not do any proptxtstatus flow control
	1 - Use implied credit from a packet status
	2 - Use explicit credit
	*/
	{"ptxmode",	IOV_PROPTXSTATUS_MODE,	0,	IOVT_UINT32,	0 },
	{"proptx_opt", IOV_PROPTXSTATUS_OPT,	0,	IOVT_UINT32,	0 },
	{"pmodule_ignore", IOV_PROPTXSTATUS_MODULE_IGNORE, 0, IOVT_BOOL, 0 },
	{"pcredit_ignore", IOV_PROPTXSTATUS_CREDIT_IGNORE, 0, IOVT_BOOL, 0 },
	{"ptxstatus_ignore", IOV_PROPTXSTATUS_TXSTATUS_IGNORE, 0, IOVT_BOOL, 0 },
	{"rxpkt_chk", IOV_PROPTXSTATUS_RXPKT_CHK, 0, IOVT_BOOL, 0 },
#endif /* PROP_TXSTATUS */
	{"bustype", IOV_BUS_TYPE, 0, IOVT_UINT32, 0},
	{"changemtu", IOV_CHANGEMTU, 0, IOVT_UINT32, 0 },
	{"host_reorder_flows", IOV_HOSTREORDER_FLOWS, 0, IOVT_BUFFER,
	(WLHOST_REORDERDATA_MAXFLOWS + 1) },
#ifdef DHDTCPACK_SUPPRESS
	{"tcpack_suppress",	IOV_TCPACK_SUPPRESS,	0,	IOVT_UINT8,	0 },
#endif /* DHDTCPACK_SUPPRESS */
#ifdef DHD_UNICAST_DHCP
	{"dhcp_unicast", IOV_DHCP_UNICAST, (0), IOVT_BOOL, 0 },
#endif /* DHD_UNICAST_DHCP */
	{"ap_isolate", IOV_AP_ISOLATE, (0), IOVT_BOOL, 0},
#ifdef DHD_L2_FILTER
	{"block_ping", IOV_BLOCK_PING, (0), IOVT_BOOL, 0},
#endif
	{NULL, 0, 0, 0, 0 }
};

#define DHD_IOVAR_BUF_SIZE	128

void dhd_save_fwdump(dhd_pub_t *dhd_pub, void * buffer, uint32 length)
{

	if (dhd_pub->soc_ram == NULL) {
#if defined(CONFIG_DHD_USE_STATIC_BUF) && defined(DHD_USE_STATIC_MEMDUMP)
		dhd_pub->soc_ram = (uint8*)DHD_OS_PREALLOC(dhd_pub,
			DHD_PREALLOC_MEMDUMP_RAM, length);
#else
		dhd_pub->soc_ram = (uint8*) MALLOC(dhd_pub->osh, length);
#endif /* CONFIG_DHD_USE_STATIC_BUF && DHD_USE_STATIC_MEMDUMP */
		if (dhd_pub->soc_ram == NULL) {
			DHD_ERROR(("%s: Failed to allocate memory for fw crash snap shot.\n",
				__FUNCTION__));
			return;
		}
	}
	memset(dhd_pub->soc_ram, 0, length);
	dhd_pub->soc_ram_length = length;
	memcpy(dhd_pub->soc_ram, buffer, length);
}

/* to NDIS developer, the structure dhd_common is redundant,
 * please do NOT merge it back from other branches !!!
 */

static int
dhd_dump(dhd_pub_t *dhdp, char *buf, int buflen)
{
	char eabuf[ETHER_ADDR_STR_LEN];

	struct bcmstrbuf b;
	struct bcmstrbuf *strbuf = &b;

	bcm_binit(strbuf, buf, buflen);

	/* Base DHD info */
	bcm_bprintf(strbuf, "%s\n", dhd_version);
	bcm_bprintf(strbuf, "\n");
	bcm_bprintf(strbuf, "pub.up %d pub.txoff %d pub.busstate %d\n",
	            dhdp->up, dhdp->txoff, dhdp->busstate);
	bcm_bprintf(strbuf, "pub.hdrlen %u pub.maxctl %u pub.rxsz %u\n",
	            dhdp->hdrlen, dhdp->maxctl, dhdp->rxsz);
	bcm_bprintf(strbuf, "pub.iswl %d pub.drv_version %ld pub.mac %s\n",
	            dhdp->iswl, dhdp->drv_version, bcm_ether_ntoa(&dhdp->mac, eabuf));
	bcm_bprintf(strbuf, "pub.bcmerror %d tickcnt %u\n", dhdp->bcmerror, dhdp->tickcnt);

	bcm_bprintf(strbuf, "dongle stats:\n");
	bcm_bprintf(strbuf, "tx_packets %lu tx_bytes %lu tx_errors %lu tx_dropped %lu\n",
	            dhdp->dstats.tx_packets, dhdp->dstats.tx_bytes,
	            dhdp->dstats.tx_errors, dhdp->dstats.tx_dropped);
	bcm_bprintf(strbuf, "rx_packets %lu rx_bytes %lu rx_errors %lu rx_dropped %lu\n",
	            dhdp->dstats.rx_packets, dhdp->dstats.rx_bytes,
	            dhdp->dstats.rx_errors, dhdp->dstats.rx_dropped);
	bcm_bprintf(strbuf, "multicast %lu\n", dhdp->dstats.multicast);

	bcm_bprintf(strbuf, "bus stats:\n");
	bcm_bprintf(strbuf, "tx_packets %lu  tx_dropped %lu tx_multicast %lu tx_errors %lu\n",
	            dhdp->tx_packets, dhdp->tx_dropped, dhdp->tx_multicast, dhdp->tx_errors);
	bcm_bprintf(strbuf, "tx_ctlpkts %lu tx_ctlerrs %lu\n",
	            dhdp->tx_ctlpkts, dhdp->tx_ctlerrs);
	bcm_bprintf(strbuf, "rx_packets %lu rx_multicast %lu rx_errors %lu \n",
	            dhdp->rx_packets, dhdp->rx_multicast, dhdp->rx_errors);
	bcm_bprintf(strbuf, "rx_ctlpkts %lu rx_ctlerrs %lu rx_dropped %lu\n",
	            dhdp->rx_ctlpkts, dhdp->rx_ctlerrs, dhdp->rx_dropped);
	bcm_bprintf(strbuf, "rx_readahead_cnt %lu tx_realloc %lu\n",
	            dhdp->rx_readahead_cnt, dhdp->tx_realloc);
	bcm_bprintf(strbuf, "\n");

	/* Add any prot info */
	dhd_prot_dump(dhdp, strbuf);
	bcm_bprintf(strbuf, "\n");

	/* Add any bus info */
	dhd_bus_dump(dhdp, strbuf);


	return (!strbuf->size ? BCME_BUFTOOSHORT : 0);
}

void
dhd_dump_to_kernelog(dhd_pub_t *dhdp)
{
	char buf[512];

	DHD_ERROR(("F/W version: %s\n", fw_version));
	bcm_bprintf_bypass = TRUE;
	dhd_dump(dhdp, buf, sizeof(buf));
	bcm_bprintf_bypass = FALSE;
}

int
dhd_wl_ioctl_cmd(dhd_pub_t *dhd_pub, int cmd, void *arg, int len, uint8 set, int ifidx)
{
	wl_ioctl_t ioc;

	ioc.cmd = cmd;
	ioc.buf = arg;
	ioc.len = len;
	ioc.set = set;

	return dhd_wl_ioctl(dhd_pub, ifidx, &ioc, arg, len);
}

int
dhd_wl_ioctl_get_intiovar(dhd_pub_t *dhd_pub, char *name, uint *pval,
	int cmd, uint8 set, int ifidx)
{
	char iovbuf[WLC_IOCTL_SMLEN];
	int ret = -1;

	memset(iovbuf, 0, sizeof(iovbuf));
	if (bcm_mkiovar(name, NULL, 0, iovbuf, sizeof(iovbuf))) {
		ret = dhd_wl_ioctl_cmd(dhd_pub, cmd, iovbuf, sizeof(iovbuf), set, ifidx);
		if (!ret) {
			*pval = ltoh32(*((uint*)iovbuf));
		} else {
			DHD_ERROR(("%s: get int iovar %s failed, ERR %d\n",
				__FUNCTION__, name, ret));
		}
	} else {
		DHD_ERROR(("%s: mkiovar %s failed\n",
			__FUNCTION__, name));
	}

	return ret;
}

int
dhd_wl_ioctl_set_intiovar(dhd_pub_t *dhd_pub, char *name, uint val,
	int cmd, uint8 set, int ifidx)
{
	char iovbuf[WLC_IOCTL_SMLEN] = {0};
	int ret = -1;
	int lval = htol32(val);
	uint len;

	len = bcm_mkiovar(name, (char*)&lval, sizeof(lval), iovbuf, sizeof(iovbuf));

	if (len) {
		ret = dhd_wl_ioctl_cmd(dhd_pub, cmd, iovbuf, len, set, ifidx);
		if (ret) {
			DHD_ERROR(("%s: set int iovar %s failed, ERR %d\n",
				__FUNCTION__, name, ret));
		}
	} else {
		DHD_ERROR(("%s: mkiovar %s failed\n",
			__FUNCTION__, name));
	}

	return ret;
}

int
dhd_wl_ioctl(dhd_pub_t *dhd_pub, int ifidx, wl_ioctl_t *ioc, void *buf, int len)
{
	int ret = BCME_ERROR;

	if (dhd_os_proto_block(dhd_pub))
	{
#ifdef DHD_LOG_DUMP
		int slen, i, val, rem, lval, min_len;
		char *pval, *pos, *msg;
		char tmp[64];

		/* WLC_GET_VAR */
		if (ioc->cmd == WLC_GET_VAR) {
			min_len = MIN(sizeof(tmp) - 1, strlen(buf));
			memset(tmp, 0, sizeof(tmp));
			bcopy(buf, tmp, min_len);
			tmp[min_len] = '\0';
		}
#endif /* DHD_LOG_DUMP */
		ret = dhd_prot_ioctl(dhd_pub, ifidx, ioc, buf, len);

		if (ret && dhd_pub->up) {
			/* Send hang event only if dhd_open() was success */
			dhd_os_check_hang(dhd_pub, ifidx, ret);
		}

		if (ret == -ETIMEDOUT && !dhd_pub->up) {
			DHD_ERROR(("%s: 'resumed on timeout' error is "
				"occurred before the interface does not"
				" bring up\n", __FUNCTION__));
			dhd_pub->busstate = DHD_BUS_DOWN;
		}

		dhd_os_proto_unblock(dhd_pub);

#ifdef DHD_LOG_DUMP
		if (ioc->cmd == WLC_GET_VAR || ioc->cmd == WLC_SET_VAR) {
			lval = 0;
			slen = strlen(buf) + 1;
			msg = (char*)buf;
			if (len >= slen + sizeof(lval)) {
				if (ioc->cmd == WLC_GET_VAR) {
					msg = tmp;
					lval = *(int*)buf;
				} else {
					min_len = MIN(ioc->len - slen, sizeof(int));
					bcopy((msg + slen), &lval, min_len);
				}
			}
			DHD_ERROR_MEM(("%s: cmd: %d, msg: %s, val: 0x%x, len: %d, set: %d\n",
				ioc->cmd == WLC_GET_VAR ? "WLC_GET_VAR" : "WLC_SET_VAR",
				ioc->cmd, msg, lval, ioc->len, ioc->set));
		} else {
			slen = ioc->len;
			if (buf != NULL) {
				val = *(int*)buf;
				pval = (char*)buf;
				pos = tmp;
				rem = sizeof(tmp);
				memset(tmp, 0, sizeof(tmp));
				for (i = 0; i < slen; i++) {
					if (rem <= 3) {
						/* At least 2 byte required + 1 byte(NULL) */
						break;
					}
					pos += snprintf(pos, rem, "%02x ", pval[i]);
					rem = sizeof(tmp) - (int)(pos - tmp);
				}
				DHD_ERROR_MEM(("WLC_IOCTL: cmd: %d, val: %d (%s), len: %d, "
					"set: %d\n", ioc->cmd, val, tmp, ioc->len, ioc->set));
			} else {
				DHD_ERROR_MEM(("WLC_IOCTL: cmd: %d, buf is NULL\n", ioc->cmd));
			}
		}
#endif /* DHD_LOG_DUMP */
		if (ret < 0) {
			if (ioc->cmd == WLC_GET_VAR || ioc->cmd == WLC_SET_VAR) {
				if (ret == BCME_UNSUPPORTED || ret == BCME_NOTASSOCIATED) {
					DHD_ERROR(("%s: %s: %s, %s\n",
						__FUNCTION__, ioc->cmd == WLC_GET_VAR ?
						"WLC_GET_VAR" : "WLC_SET_VAR",
						(char *)buf,
						ret == BCME_UNSUPPORTED ? "UNSUPPORTED"
						: "NOT ASSOCIATED"));
				} else {
					DHD_ERROR(("%s: %s: %s, ret = %d\n",
						__FUNCTION__, ioc->cmd == WLC_GET_VAR ?
						"WLC_GET_VAR" : "WLC_SET_VAR",
						(char *)buf, ret));
				}
			} else {
				if (ret == BCME_UNSUPPORTED || ret == BCME_NOTASSOCIATED) {
					DHD_ERROR(("%s: WLC_IOCTL: cmd: %d, %s\n",
						__FUNCTION__, ioc->cmd,
						ret == BCME_UNSUPPORTED ? "UNSUPPORTED" :
						"NOT ASSOCIATED"));
				} else {
					DHD_ERROR(("%s: WLC_IOCTL: cmd: %d, ret = %d\n",
						__FUNCTION__, ioc->cmd, ret));
				}
			}
		}
	}

	return ret;
}

uint wl_get_port_num(wl_io_pport_t *io_pport)
{
	return 0;
}

/* Get bssidx from iovar params
 * Input:   dhd_pub - pointer to dhd_pub_t
 *	    params  - IOVAR params
 * Output:  idx	    - BSS index
 *	    val	    - ponter to the IOVAR arguments
 */
static int
dhd_iovar_parse_bssidx(dhd_pub_t *dhd_pub, char *params, int *idx, char **val)
{
	char *prefix = "bsscfg:";
	uint32	bssidx;

	if (!(strncmp(params, prefix, strlen(prefix)))) {
		/* per bss setting should be prefixed with 'bsscfg:' */
		char *p = (char *)params + strlen(prefix);

		/* Skip Name */
		while (*p != '\0')
			p++;
		/* consider null */
		p = p + 1;
		bcopy(p, &bssidx, sizeof(uint32));
		/* Get corresponding dhd index */
		bssidx = dhd_bssidx2idx(dhd_pub, bssidx);

		if (bssidx >= DHD_MAX_IFS) {
			DHD_ERROR(("%s Wrong bssidx provided\n", __FUNCTION__));
			return BCME_ERROR;
		}

		/* skip bss idx */
		p += sizeof(uint32);
		*val = p;
		*idx = bssidx;
	} else {
		DHD_ERROR(("%s: bad parameter for per bss iovar\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return BCME_OK;
}

static int
dhd_doiovar(dhd_pub_t *dhd_pub, const bcm_iovar_t *vi, uint32 actionid, const char *name,
            void *params, int plen, void *arg, int len, int val_size)
{
	int bcmerror = 0;
	int32 int_val = 0;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));
	DHD_TRACE(("%s: actionid = %d; name %s\n", __FUNCTION__, actionid, name));

	if ((bcmerror = bcm_iovar_lencheck(vi, arg, len, IOV_ISSET(actionid))) != 0)
		goto exit;

	if (plen >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	switch (actionid) {
	case IOV_GVAL(IOV_VERSION):
		/* Need to have checked buffer length */
		bcm_strncpy_s((char*)arg, len, dhd_version, len);
		break;

	case IOV_GVAL(IOV_MSGLEVEL):
		int_val = (int32)dhd_msg_level;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_MSGLEVEL):
#ifdef WL_CFG80211
		/* Enable DHD and WL logs in oneshot */
		if (int_val & DHD_WL_VAL2)
			wl_cfg80211_enable_trace(TRUE, int_val & (~DHD_WL_VAL2));
		else if (int_val & DHD_WL_VAL)
			wl_cfg80211_enable_trace(FALSE, WL_DBG_DBG);
		if (!(int_val & DHD_WL_VAL2))
#endif /* WL_CFG80211 */
		dhd_msg_level = int_val;
		break;
	case IOV_GVAL(IOV_BCMERRORSTR):
		bcm_strncpy_s((char *)arg, len, bcmerrorstr(dhd_pub->bcmerror), BCME_STRLEN);
		((char *)arg)[BCME_STRLEN - 1] = 0x00;
		break;

	case IOV_GVAL(IOV_BCMERROR):
		int_val = (int32)dhd_pub->bcmerror;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_GVAL(IOV_WDTICK):
		int_val = (int32)dhd_watchdog_ms;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_WDTICK):
		if (!dhd_pub->up) {
			bcmerror = BCME_NOTUP;
			break;
		}
		dhd_os_wd_timer(dhd_pub, (uint)int_val);
		break;

	case IOV_GVAL(IOV_DUMP):
		bcmerror = dhd_dump(dhd_pub, arg, len);
		break;

#ifdef DHD_DEBUG
	case IOV_GVAL(IOV_DCONSOLE_POLL):
		int_val = (int32)dhd_console_ms;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_DCONSOLE_POLL):
		dhd_console_ms = (uint)int_val;
		break;

	case IOV_SVAL(IOV_CONS):
		if (len > 0)
			bcmerror = dhd_bus_console_in(dhd_pub, arg, len - 1);
		break;
#endif /* DHD_DEBUG */

	case IOV_SVAL(IOV_CLEARCOUNTS):
		dhd_pub->tx_packets = dhd_pub->rx_packets = 0;
		dhd_pub->tx_errors = dhd_pub->rx_errors = 0;
		dhd_pub->tx_ctlpkts = dhd_pub->rx_ctlpkts = 0;
		dhd_pub->tx_ctlerrs = dhd_pub->rx_ctlerrs = 0;
		dhd_pub->tx_dropped = 0;
		dhd_pub->rx_dropped = 0;
		dhd_pub->rx_readahead_cnt = 0;
		dhd_pub->tx_realloc = 0;
		dhd_pub->wd_dpc_sched = 0;
		memset(&dhd_pub->dstats, 0, sizeof(dhd_pub->dstats));
		dhd_bus_clearcounts(dhd_pub);
#ifdef PROP_TXSTATUS
		/* clear proptxstatus related counters */
		dhd_wlfc_clear_counts(dhd_pub);
#endif /* PROP_TXSTATUS */
		break;


	case IOV_GVAL(IOV_IOCTLTIMEOUT): {
		int_val = (int32)dhd_os_get_ioctl_resp_timeout();
		bcopy(&int_val, arg, sizeof(int_val));
		break;
	}

	case IOV_SVAL(IOV_IOCTLTIMEOUT): {
		if (int_val <= 0)
			bcmerror = BCME_BADARG;
		else
			dhd_os_set_ioctl_resp_timeout((unsigned int)int_val);
		break;
	}

#ifdef WLBTAMP
	case IOV_SVAL(IOV_HCI_CMD): {
		amp_hci_cmd_t *cmd = (amp_hci_cmd_t *)arg;

		/* sanity check: command preamble present */
		if (len < HCI_CMD_PREAMBLE_SIZE)
			return BCME_BUFTOOSHORT;

		/* sanity check: command parameters are present */
		if (len < (int)(HCI_CMD_PREAMBLE_SIZE + cmd->plen))
			return BCME_BUFTOOSHORT;

		dhd_bta_docmd(dhd_pub, cmd, len);
		break;
	}

	case IOV_SVAL(IOV_HCI_ACL_DATA): {
		amp_hci_ACL_data_t *ACL_data = (amp_hci_ACL_data_t *)arg;

		/* sanity check: HCI header present */
		if (len < HCI_ACL_DATA_PREAMBLE_SIZE)
			return BCME_BUFTOOSHORT;

		/* sanity check: ACL data is present */
		if (len < (int)(HCI_ACL_DATA_PREAMBLE_SIZE + ACL_data->dlen))
			return BCME_BUFTOOSHORT;

		dhd_bta_tx_hcidata(dhd_pub, ACL_data, len);
		break;
	}
#endif /* WLBTAMP */

#ifdef PROP_TXSTATUS
	case IOV_GVAL(IOV_PROPTXSTATUS_ENABLE): {
		bool wlfc_enab = FALSE;
		bcmerror = dhd_wlfc_get_enable(dhd_pub, &wlfc_enab);
		if (bcmerror != BCME_OK)
			goto exit;
		int_val = wlfc_enab ? 1 : 0;
		bcopy(&int_val, arg, val_size);
		break;
	}
	case IOV_SVAL(IOV_PROPTXSTATUS_ENABLE): {
		bool wlfc_enab = FALSE;
		bcmerror = dhd_wlfc_get_enable(dhd_pub, &wlfc_enab);
		if (bcmerror != BCME_OK)
			goto exit;

		/* wlfc is already set as desired */
		if (wlfc_enab == (int_val == 0 ? FALSE : TRUE))
			goto exit;

		if (int_val == TRUE)
			bcmerror = dhd_wlfc_init(dhd_pub);
		else
			bcmerror = dhd_wlfc_deinit(dhd_pub);

		break;
	}
	case IOV_GVAL(IOV_PROPTXSTATUS_MODE):
		bcmerror = dhd_wlfc_get_mode(dhd_pub, &int_val);
		if (bcmerror != BCME_OK)
			goto exit;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_PROPTXSTATUS_MODE):
		dhd_wlfc_set_mode(dhd_pub, int_val);
		break;

	case IOV_GVAL(IOV_PROPTXSTATUS_MODULE_IGNORE):
		bcmerror = dhd_wlfc_get_module_ignore(dhd_pub, &int_val);
		if (bcmerror != BCME_OK)
			goto exit;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_PROPTXSTATUS_MODULE_IGNORE):
		dhd_wlfc_set_module_ignore(dhd_pub, int_val);
		break;

	case IOV_GVAL(IOV_PROPTXSTATUS_CREDIT_IGNORE):
		bcmerror = dhd_wlfc_get_credit_ignore(dhd_pub, &int_val);
		if (bcmerror != BCME_OK)
			goto exit;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_PROPTXSTATUS_CREDIT_IGNORE):
		dhd_wlfc_set_credit_ignore(dhd_pub, int_val);
		break;

	case IOV_GVAL(IOV_PROPTXSTATUS_TXSTATUS_IGNORE):
		bcmerror = dhd_wlfc_get_txstatus_ignore(dhd_pub, &int_val);
		if (bcmerror != BCME_OK)
			goto exit;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_PROPTXSTATUS_TXSTATUS_IGNORE):
		dhd_wlfc_set_txstatus_ignore(dhd_pub, int_val);
		break;

	case IOV_GVAL(IOV_PROPTXSTATUS_RXPKT_CHK):
		bcmerror = dhd_wlfc_get_rxpkt_chk(dhd_pub, &int_val);
		if (bcmerror != BCME_OK)
			goto exit;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_PROPTXSTATUS_RXPKT_CHK):
		dhd_wlfc_set_rxpkt_chk(dhd_pub, int_val);
		break;

#endif /* PROP_TXSTATUS */

	case IOV_GVAL(IOV_BUS_TYPE):
		/* The dhd application queries the driver to check if its usb or sdio.  */
#ifdef BCMDHDUSB
		int_val = BUS_TYPE_USB;
#endif
#ifdef BCMSDIO
		int_val = BUS_TYPE_SDIO;
#endif
#ifdef PCIE_FULL_DONGLE
		int_val = BUS_TYPE_PCIE;
#endif
		bcopy(&int_val, arg, val_size);
		break;


	case IOV_SVAL(IOV_CHANGEMTU):
		int_val &= 0xffff;
		bcmerror = dhd_change_mtu(dhd_pub, int_val, 0);
		break;

	case IOV_GVAL(IOV_HOSTREORDER_FLOWS):
	{
		uint i = 0;
		uint8 *ptr = (uint8 *)arg;
		uint8 count = 0;

		ptr++;
		for (i = 0; i < WLHOST_REORDERDATA_MAXFLOWS; i++) {
			if (dhd_pub->reorder_bufs[i] != NULL) {
				*ptr = dhd_pub->reorder_bufs[i]->flow_id;
				ptr++;
				count++;
			}
		}
		ptr = (uint8 *)arg;
		*ptr = count;
		break;
	}
#ifdef DHDTCPACK_SUPPRESS
	case IOV_GVAL(IOV_TCPACK_SUPPRESS): {
		int_val = (uint32)dhd_pub->tcpack_sup_mode;
		bcopy(&int_val, arg, val_size);
		break;
	}
	case IOV_SVAL(IOV_TCPACK_SUPPRESS): {
		bcmerror = dhd_tcpack_suppress_set(dhd_pub, (uint8)int_val);
		break;
	}
#endif /* DHDTCPACK_SUPPRESS */


#ifdef DHD_UNICAST_DHCP
	case IOV_GVAL(IOV_DHCP_UNICAST):
		int_val = dhd_pub->dhcp_unicast;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_DHCP_UNICAST):
		if (dhd_pub->dhcp_unicast == int_val)
			break;

		if (int_val >= OFF || int_val <= ON) {
			dhd_pub->dhcp_unicast = int_val;
		} else {
			bcmerror = BCME_RANGE;
		}
		break;
#endif /* DHD_UNICAST_DHCP */
#ifdef DHD_L2_FILTER
	case IOV_GVAL(IOV_BLOCK_PING):
		int_val = dhd_pub->block_ping;
		bcopy(&int_val, arg, val_size);
		break;
	case IOV_SVAL(IOV_BLOCK_PING):
		if (dhd_pub->block_ping == int_val)
			break;
		if (int_val >= OFF || int_val <= ON) {
			dhd_pub->block_ping = int_val;
		} else {
			bcmerror = BCME_RANGE;
		}
		break;
#endif

	case IOV_GVAL(IOV_AP_ISOLATE): {
		uint32	bssidx;
		char *val;

		if (dhd_iovar_parse_bssidx(dhd_pub, (char *)name, &bssidx, &val) != BCME_OK) {
			DHD_ERROR(("%s: ap isoalate: bad parameter\n", __FUNCTION__));
			bcmerror = BCME_BADARG;
			break;
		}

		int_val = dhd_get_ap_isolate(dhd_pub, bssidx);
		bcopy(&int_val, arg, val_size);
		break;
	}
	case IOV_SVAL(IOV_AP_ISOLATE): {
		uint32	bssidx;
		char *val;

		if (dhd_iovar_parse_bssidx(dhd_pub, (char *)name, &bssidx, &val) != BCME_OK) {
			DHD_ERROR(("%s: ap isolate: bad parameter\n", __FUNCTION__));
			bcmerror = BCME_BADARG;
			break;
		}

		ASSERT(val);
		bcopy(val, &int_val, sizeof(uint32));
		dhd_set_ap_isolate(dhd_pub, bssidx, int_val);
		break;
	}

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

exit:
	DHD_TRACE(("%s: actionid %d, bcmerror %d\n", __FUNCTION__, actionid, bcmerror));
	return bcmerror;
}

/* Store the status of a connection attempt for later retrieval by an iovar */
void
dhd_store_conn_status(uint32 event, uint32 status, uint32 reason)
{
	/* Do not overwrite a WLC_E_PRUNE with a WLC_E_SET_SSID
	 * because an encryption/rsn mismatch results in both events, and
	 * the important information is in the WLC_E_PRUNE.
	 */
	if (!(event == WLC_E_SET_SSID && status == WLC_E_STATUS_FAIL &&
	      dhd_conn_event == WLC_E_PRUNE)) {
		dhd_conn_event = event;
		dhd_conn_status = status;
		dhd_conn_reason = reason;
	}
}

bool
dhd_prec_enq(dhd_pub_t *dhdp, struct pktq *q, void *pkt, int prec)
{
	void *p;
	int eprec = -1;		/* precedence to evict from */
	bool discard_oldest;

	/* Fast case, precedence queue is not full and we are also not
	 * exceeding total queue length
	 */
	if (!pktq_pfull(q, prec) && !pktq_full(q)) {
		pktq_penq(q, prec, pkt);
		return TRUE;
	}

	/* Determine precedence from which to evict packet, if any */
	if (pktq_pfull(q, prec))
		eprec = prec;
	else if (pktq_full(q)) {
		p = pktq_peek_tail(q, &eprec);
		ASSERT(p);
		if (eprec > prec || eprec < 0)
			return FALSE;
	}

	/* Evict if needed */
	if (eprec >= 0) {
		/* Detect queueing to unconfigured precedence */
		ASSERT(!pktq_pempty(q, eprec));
		discard_oldest = AC_BITMAP_TST(dhdp->wme_dp, eprec);
		if (eprec == prec && !discard_oldest)
			return FALSE;		/* refuse newer (incoming) packet */
		/* Evict packet according to discard policy */
		p = discard_oldest ? pktq_pdeq(q, eprec) : pktq_pdeq_tail(q, eprec);
		ASSERT(p);
#ifdef DHDTCPACK_SUPPRESS
		if (dhd_tcpack_check_xmit(dhdp, p) == BCME_ERROR) {
			DHD_ERROR(("%s %d: tcpack_suppress ERROR!!! Stop using it\n",
				__FUNCTION__, __LINE__));
			dhd_tcpack_suppress_set(dhdp, TCPACK_SUP_OFF);
		}
#endif /* DHDTCPACK_SUPPRESS */
		PKTFREE(dhdp->osh, p, TRUE);
	}

	/* Enqueue */
	p = pktq_penq(q, prec, pkt);
	ASSERT(p);

	return TRUE;
}

/*
 * Functions to drop proper pkts from queue:
 *	If one pkt in queue is non-fragmented, drop first non-fragmented pkt only
 *	If all pkts in queue are all fragmented, find and drop one whole set fragmented pkts
 *	If can't find pkts matching upper 2 cases, drop first pkt anyway
 */
bool
dhd_prec_drop_pkts(dhd_pub_t *dhdp, struct pktq *pq, int prec, f_droppkt_t fn)
{
	struct pktq_prec *q = NULL;
	void *p, *prev = NULL, *next = NULL, *first = NULL, *last = NULL, *prev_first = NULL;
	pkt_frag_t frag_info;

	ASSERT(dhdp && pq);
	ASSERT(prec >= 0 && prec < pq->num_prec);

	q = &pq->q[prec];
	p = q->head;

	if (p == NULL)
		return FALSE;

	while (p) {
		frag_info = pkt_frag_info(dhdp->osh, p);
		if (frag_info == DHD_PKT_FRAG_NONE) {
			break;
		} else if (frag_info == DHD_PKT_FRAG_FIRST) {
			if (first) {
				/* No last frag pkt, use prev as last */
				last = prev;
				break;
			} else {
				first = p;
				prev_first = prev;
			}
		} else if (frag_info == DHD_PKT_FRAG_LAST) {
			if (first) {
				last = p;
				break;
			}
		}

		prev = p;
		p = PKTLINK(p);
	}

	if ((p == NULL) || ((frag_info != DHD_PKT_FRAG_NONE) && !(first && last))) {
		/* Not found matching pkts, use oldest */
		prev = NULL;
		p = q->head;
		frag_info = 0;
	}

	if (frag_info == DHD_PKT_FRAG_NONE) {
		first = last = p;
		prev_first = prev;
	}

	p = first;
	while (p) {
		next = PKTLINK(p);
		q->len--;
		pq->len--;

		PKTSETLINK(p, NULL);

		if (fn)
			fn(dhdp, prec, p, TRUE);

		if (p == last)
			break;

		p = next;
	}

	if (prev_first == NULL) {
		if ((q->head = next) == NULL)
			q->tail = NULL;
	} else {
		PKTSETLINK(prev_first, next);
		if (!next)
			q->tail = prev_first;
	}

	return TRUE;
}

static int
dhd_iovar_op(dhd_pub_t *dhd_pub, const char *name,
	void *params, int plen, void *arg, int len, bool set)
{
	int bcmerror = 0;
	int val_size;
	const bcm_iovar_t *vi = NULL;
	uint32 actionid;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	ASSERT(name);
	ASSERT(len >= 0);

	/* Get MUST have return space */
	ASSERT(set || (arg && len));

	/* Set does NOT take qualifiers */
	ASSERT(!set || (!params && !plen));

	if ((vi = bcm_iovar_lookup(dhd_iovars, name)) == NULL) {
		bcmerror = BCME_UNSUPPORTED;
		goto exit;
	}

	DHD_CTL(("%s: %s %s, len %d plen %d\n", __FUNCTION__,
		name, (set ? "set" : "get"), len, plen));

	/* set up 'params' pointer in case this is a set command so that
	 * the convenience int and bool code can be common to set and get
	 */
	if (params == NULL) {
		params = arg;
		plen = len;
	}

	if (vi->type == IOVT_VOID)
		val_size = 0;
	else if (vi->type == IOVT_BUFFER)
		val_size = len;
	else
		/* all other types are integer sized */
		val_size = sizeof(int);

	actionid = set ? IOV_SVAL(vi->varid) : IOV_GVAL(vi->varid);

	bcmerror = dhd_doiovar(dhd_pub, vi, actionid, name, params, plen, arg, len, val_size);

exit:
	return bcmerror;
}

int
dhd_ioctl(dhd_pub_t * dhd_pub, dhd_ioctl_t *ioc, void *buf, uint buflen)
{
	int bcmerror = 0;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	if (!buf) {
		return BCME_BADARG;
	}

	switch (ioc->cmd) {
	case DHD_GET_MAGIC:
		if (buflen < sizeof(int))
			bcmerror = BCME_BUFTOOSHORT;
		else
			*(int*)buf = DHD_IOCTL_MAGIC;
		break;

	case DHD_GET_VERSION:
		if (buflen < sizeof(int))
			bcmerror = BCME_BUFTOOSHORT;
		else
			*(int*)buf = DHD_IOCTL_VERSION;
		break;

	case DHD_GET_VAR:
	case DHD_SET_VAR: {
		char *arg;
		uint arglen;

		/* scan past the name to any arguments */
		for (arg = buf, arglen = buflen; *arg && arglen; arg++, arglen--)
			;

		if (*arg) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		/* account for the NUL terminator */
		arg++, arglen--;

		/* call with the appropriate arguments */
		if (ioc->cmd == DHD_GET_VAR)
			bcmerror = dhd_iovar_op(dhd_pub, buf, arg, arglen,
			buf, buflen, IOV_GET);
		else
			bcmerror = dhd_iovar_op(dhd_pub, buf, NULL, 0, arg, arglen, IOV_SET);
		if (bcmerror != BCME_UNSUPPORTED)
			break;

		/* not in generic table, try protocol module */
		if (ioc->cmd == DHD_GET_VAR)
			bcmerror = dhd_prot_iovar_op(dhd_pub, buf, arg,
				arglen, buf, buflen, IOV_GET);
		else
			bcmerror = dhd_prot_iovar_op(dhd_pub, buf,
				NULL, 0, arg, arglen, IOV_SET);
		if (bcmerror != BCME_UNSUPPORTED)
			break;

		/* if still not found, try bus module */
		if (ioc->cmd == DHD_GET_VAR) {
			bcmerror = dhd_bus_iovar_op(dhd_pub, buf,
				arg, arglen, buf, buflen, IOV_GET);
		} else {
			bcmerror = dhd_bus_iovar_op(dhd_pub, buf,
				NULL, 0, arg, arglen, IOV_SET);
		}

		break;
	}

	default:
		bcmerror = BCME_UNSUPPORTED;
	}

	return bcmerror;
}

#ifdef SHOW_EVENTS
#ifdef SHOW_LOGTRACE

#define AVOID_BYTE 64
#define MAX_NO_OF_ARG 16

static int
check_event_log_sequence_number(uint32 seq_no)
{
	int32 diff;
	uint32 ret;
	static uint32 logtrace_seqnum_prev = 0;

	diff = ntoh32(seq_no)-logtrace_seqnum_prev;
	switch (diff)
	{
		case 0:
			ret = -1; /* duplicate packet . drop */
			break;

		case 1:
			ret =0; /* in order */
			break;

		default:
			if ((ntoh32(seq_no) == 0) &&
				(logtrace_seqnum_prev == 0xFFFFFFFF) ) { /* in-order - Roll over */
					ret = 0;
			} else {

				if (diff > 0) {
					DHD_EVENT(("WLC_E_TRACE:"
						"Event lost (log) seqnum %d nblost %d\n",
						ntoh32(seq_no), (diff-1)));
				} else {
					DHD_EVENT(("WLC_E_TRACE:"
						"Event Packets coming out of order!!\n"));
				}
				ret = 0;
			}
	}

	logtrace_seqnum_prev = ntoh32(seq_no);

	return ret;
}
#endif /* SHOW_LOGTRACE */

static void
wl_show_host_event(dhd_pub_t *dhd_pub, wl_event_msg_t *event, void *event_data,
	void *raw_event_ptr, char *eventmask)
{
	uint i, status, reason;
	bool group = FALSE, flush_txq = FALSE, link = FALSE;
	const char *auth_str;
	const char *event_name;
	uchar *buf;
	char err_msg[256], eabuf[ETHER_ADDR_STR_LEN];
	uint event_type, flags, auth_type, datalen;

	event_type = ntoh32(event->event_type);
	flags = ntoh16(event->flags);
	status = ntoh32(event->status);
	reason = ntoh32(event->reason);
	BCM_REFERENCE(reason);
	auth_type = ntoh32(event->auth_type);
	datalen = ntoh32(event->datalen);

	/* debug dump of event messages */
	snprintf(eabuf, sizeof(eabuf), "%02x:%02x:%02x:%02x:%02x:%02x",
	        (uchar)event->addr.octet[0]&0xff,
	        (uchar)event->addr.octet[1]&0xff,
	        (uchar)event->addr.octet[2]&0xff,
	        (uchar)event->addr.octet[3]&0xff,
	        (uchar)event->addr.octet[4]&0xff,
	        (uchar)event->addr.octet[5]&0xff);

	event_name = bcmevent_get_name(event_type);
	BCM_REFERENCE(event_name);

	if (flags & WLC_EVENT_MSG_LINK)
		link = TRUE;
	if (flags & WLC_EVENT_MSG_GROUP)
		group = TRUE;
	if (flags & WLC_EVENT_MSG_FLUSHTXQ)
		flush_txq = TRUE;

	switch (event_type) {
	case WLC_E_START:
	case WLC_E_DEAUTH:
	case WLC_E_DISASSOC:
		DHD_EVENT(("MACEVENT: %s, MAC %s\n", event_name, eabuf));
		break;

	case WLC_E_ASSOC_IND:
	case WLC_E_REASSOC_IND:

		DHD_EVENT(("MACEVENT: %s, MAC %s\n", event_name, eabuf));
		break;

	case WLC_E_ASSOC:
	case WLC_E_REASSOC:
		if (status == WLC_E_STATUS_SUCCESS) {
			DHD_EVENT(("MACEVENT: %s, MAC %s, SUCCESS\n", event_name, eabuf));
		} else if (status == WLC_E_STATUS_TIMEOUT) {
			DHD_EVENT(("MACEVENT: %s, MAC %s, TIMEOUT\n", event_name, eabuf));
		} else if (status == WLC_E_STATUS_FAIL) {
			DHD_EVENT(("MACEVENT: %s, MAC %s, FAILURE, reason %d\n",
			       event_name, eabuf, (int)reason));
		} else {
			DHD_EVENT(("MACEVENT: %s, MAC %s, unexpected status %d\n",
			       event_name, eabuf, (int)status));
		}
		break;

	case WLC_E_DEAUTH_IND:
	case WLC_E_DISASSOC_IND:
		DHD_EVENT(("MACEVENT: %s, MAC %s, reason %d\n", event_name, eabuf, (int)reason));
		break;

	case WLC_E_AUTH:
	case WLC_E_AUTH_IND:
		if (auth_type == DOT11_OPEN_SYSTEM)
			auth_str = "Open System";
		else if (auth_type == DOT11_SHARED_KEY)
			auth_str = "Shared Key";
		else {
			snprintf(err_msg, sizeof(err_msg), "AUTH unknown: %d", (int)auth_type);
			auth_str = err_msg;
		}
		if (event_type == WLC_E_AUTH_IND) {
			DHD_EVENT(("MACEVENT: %s, MAC %s, %s\n", event_name, eabuf, auth_str));
		} else if (status == WLC_E_STATUS_SUCCESS) {
			DHD_EVENT(("MACEVENT: %s, MAC %s, %s, SUCCESS\n",
				event_name, eabuf, auth_str));
		} else if (status == WLC_E_STATUS_TIMEOUT) {
			DHD_EVENT(("MACEVENT: %s, MAC %s, %s, TIMEOUT\n",
				event_name, eabuf, auth_str));
		} else if (status == WLC_E_STATUS_FAIL) {
			DHD_EVENT(("MACEVENT: %s, MAC %s, %s, FAILURE, reason %d\n",
			       event_name, eabuf, auth_str, (int)reason));
		}
		BCM_REFERENCE(auth_str);

		break;

	case WLC_E_JOIN:
	case WLC_E_ROAM:
	case WLC_E_SET_SSID:
		if (status == WLC_E_STATUS_SUCCESS) {
			DHD_EVENT(("MACEVENT: %s, MAC %s\n", event_name, eabuf));
		} else if (status == WLC_E_STATUS_FAIL) {
			DHD_EVENT(("MACEVENT: %s, failed\n", event_name));
		} else if (status == WLC_E_STATUS_NO_NETWORKS) {
			DHD_EVENT(("MACEVENT: %s, no networks found\n", event_name));
		} else {
			DHD_EVENT(("MACEVENT: %s, unexpected status %d\n",
				event_name, (int)status));
		}
		break;

	case WLC_E_BEACON_RX:
		if (status == WLC_E_STATUS_SUCCESS) {
			DHD_EVENT(("MACEVENT: %s, SUCCESS\n", event_name));
		} else if (status == WLC_E_STATUS_FAIL) {
			DHD_EVENT(("MACEVENT: %s, FAIL\n", event_name));
		} else {
			DHD_EVENT(("MACEVENT: %s, status %d\n", event_name, status));
		}
		break;

	case WLC_E_LINK:
		DHD_EVENT(("MACEVENT: %s %s\n", event_name, link?"UP":"DOWN"));
		BCM_REFERENCE(link);
		break;

	case WLC_E_MIC_ERROR:
		DHD_EVENT(("MACEVENT: %s, MAC %s, Group %d, Flush %d\n",
		       event_name, eabuf, group, flush_txq));
		BCM_REFERENCE(group);
		BCM_REFERENCE(flush_txq);
		break;

	case WLC_E_ICV_ERROR:
	case WLC_E_UNICAST_DECODE_ERROR:
	case WLC_E_MULTICAST_DECODE_ERROR:
		DHD_EVENT(("MACEVENT: %s, MAC %s\n",
		       event_name, eabuf));
		break;

	case WLC_E_TXFAIL:
		DHD_EVENT(("MACEVENT: %s, RA %s\n", event_name, eabuf));
		break;

	case WLC_E_SCAN_COMPLETE:
	case WLC_E_ASSOC_REQ_IE:
	case WLC_E_ASSOC_RESP_IE:
	case WLC_E_PMKID_CACHE:
		DHD_EVENT(("MACEVENT: %s\n", event_name));
		break;

	case WLC_E_PFN_NET_FOUND:
	case WLC_E_PFN_NET_LOST:
	case WLC_E_PFN_SCAN_COMPLETE:
	case WLC_E_PFN_SCAN_NONE:
	case WLC_E_PFN_SCAN_ALLGONE:
	case WLC_E_PFN_GSCAN_FULL_RESULT:
		DHD_EVENT(("PNOEVENT: %s\n", event_name));
		break;

	case WLC_E_PSK_SUP:
	case WLC_E_PRUNE:
		DHD_EVENT(("MACEVENT: %s, status %d, reason %d\n",
		           event_name, (int)status, (int)reason));
		break;

#ifdef WIFI_ACT_FRAME
	case WLC_E_ACTION_FRAME:
		DHD_TRACE(("MACEVENT: %s Bssid %s\n", event_name, eabuf));
		break;
#endif /* WIFI_ACT_FRAME */

#ifdef SHOW_LOGTRACE
	case WLC_E_TRACE:
	{
		msgtrace_hdr_t hdr;
		uint32 nblost;
		uint8 count;
		char *s, *p;
		static uint32 seqnum_prev = 0;
		uint32 *record = NULL;
		uint32 *log_ptr =  NULL;
		uint32 writeindex = 0;
		event_log_hdr_t event_hdr;
		int no_of_fmts = 0;
		char *fmt = NULL;
		dhd_event_log_t *raw_event = (dhd_event_log_t *) raw_event_ptr;

		buf = (uchar *) event_data;
		memcpy(&hdr, buf, MSGTRACE_HDRLEN);

		if (hdr.version != MSGTRACE_VERSION) {
			DHD_EVENT(("\nMACEVENT: %s [unsupported version --> "
				"dhd version:%d dongle version:%d]\n",
				event_name, MSGTRACE_VERSION, hdr.version));
			/* Reset datalen to avoid display below */
			datalen = 0;
			break;
		}

		if (hdr.trace_type == MSGTRACE_HDR_TYPE_MSG) {
			/* There are 2 bytes available at the end of data */
			buf[MSGTRACE_HDRLEN + ntoh16(hdr.len)] = '\0';

			if (ntoh32(hdr.discarded_bytes) || ntoh32(hdr.discarded_printf)) {
				DHD_EVENT(("WLC_E_TRACE: [Discarded traces in dongle -->"
					"discarded_bytes %d discarded_printf %d]\n",
					ntoh32(hdr.discarded_bytes),
					ntoh32(hdr.discarded_printf)));
			}

			nblost = ntoh32(hdr.seqnum) - seqnum_prev - 1;
			if (nblost > 0) {
				DHD_EVENT(("WLC_E_TRACE:"
					"[Event lost (msg) --> seqnum %d nblost %d\n",
					ntoh32(hdr.seqnum), nblost));
			}
			seqnum_prev = ntoh32(hdr.seqnum);

			/* Display the trace buffer. Advance from
			 * \n to \n to avoid display big
			 * printf (issue with Linux printk )
			 */
			p = (char *)&buf[MSGTRACE_HDRLEN];
			while (*p != '\0' && (s = strstr(p, "\n")) != NULL) {
				*s = '\0';
				DHD_EVENT(("%s\n", p));
				p = s+1;
			}
			if (*p)
				DHD_EVENT(("%s", p));

			/* Reset datalen to avoid display below */
			datalen = 0;

		} else if (hdr.trace_type == MSGTRACE_HDR_TYPE_LOG) {
			/* Let the standard event printing work for now */
			uint32 timestamp, w, malloc_len;

			if (check_event_log_sequence_number(hdr.seqnum)) {

				DHD_EVENT(("%s: WLC_E_TRACE:"
					"[Event duplicate (log) %d] dropping!!\n",
					__FUNCTION__, hdr.seqnum));
				return; /* drop duplicate events */
			}

			p = (char *)&buf[MSGTRACE_HDRLEN];
			datalen -= MSGTRACE_HDRLEN;
			w = ntoh32((uint32)*p);
			p += 4;
			datalen -= 4;
			timestamp = ntoh32((uint32)*p);
			BCM_REFERENCE(timestamp);
			BCM_REFERENCE(w);

			DHD_EVENT(("timestamp %x%x\n", timestamp, w));

			if (raw_event->fmts) {
				malloc_len = datalen+ AVOID_BYTE;
				record = (uint32 *)MALLOC(dhd_pub->osh, malloc_len);
				if (record == NULL) {
					DHD_EVENT(("MSGTRACE_HDR_TYPE_LOG:"
						"malloc failed\n"));
					return;
				}
				log_ptr = (uint32 *) (p + datalen);
				writeindex = datalen/4;

				if (record) {
					while (datalen > 4) {
						log_ptr--;
						datalen -= 4;
						event_hdr.t = *log_ptr;
						/*
						 * Check for partially overriten entries
						 */
						if (log_ptr - (uint32 *) p < event_hdr.count) {
								break;
						}
						/*
						* Check for end of the Frame.
						*/
						if (event_hdr.tag ==  EVENT_LOG_TAG_NULL) {
							continue;
						}
						/*
						* Check For Special Time Stamp Packet
						*/
						if (event_hdr.tag == EVENT_LOG_TAG_TS) {
							datalen -= 12;
							log_ptr = log_ptr - 3;
							continue;
						}

						log_ptr[0] = event_hdr.t;
						if (event_hdr.count > MAX_NO_OF_ARG) {
							break;
						}
						/* Now place the header at the front
						* and copy back.
						*/
						log_ptr -= event_hdr.count;

						writeindex = writeindex - event_hdr.count;
						record[writeindex++] = event_hdr.t;
						for (count = 0; count < (event_hdr.count-1);
							count++) {
							record[writeindex++] = log_ptr[count];
						}
						writeindex = writeindex - event_hdr.count;
						datalen = datalen - (event_hdr.count * 4);
						no_of_fmts++;
					}
				}

				while (no_of_fmts--)
				{
					event_log_hdr_t event_hdr;
					event_hdr.t = record[writeindex];

					if ((event_hdr.fmt_num>>2) < raw_event->num_fmts) {
						fmt = raw_event->fmts[event_hdr.fmt_num>>2];
						DHD_EVENT((fmt,
							record[writeindex + 1],
							record[writeindex + 2],
							record[writeindex + 3],
							record[writeindex + 4],
							record[writeindex + 5],
							record[writeindex + 6],
							record[writeindex + 7],
							record[writeindex + 8],
							record[writeindex + 9],
							record[writeindex + 10],
							record[writeindex + 11],
							record[writeindex + 12],
							record[writeindex + 13],
							record[writeindex + 14],
							record[writeindex + 15],
							record[writeindex + 16]));

						if (fmt[strlen(fmt) - 1] != '\n') {
							/* Add newline if missing */
							DHD_EVENT(("\n"));
						}
					}

					writeindex = writeindex + event_hdr.count;
				}

				if (record) {
					MFREE(dhd_pub->osh, record, malloc_len);
					record = NULL;
				}
			} else {
				while (datalen > 4) {
					p += 4;
					datalen -= 4;
					/* Print each word.  DO NOT ntoh it.  */
					DHD_EVENT((" %8.8x", *((uint32 *) p)));
				}
				DHD_EVENT(("\n"));
			}
			datalen = 0;
		}
		break;
	}
#endif /* SHOW_LOGTRACE */

	case WLC_E_RSSI:
		DHD_EVENT(("MACEVENT: %s %d\n", event_name, ntoh32(*((int *)event_data))));
		break;

	case WLC_E_SERVICE_FOUND:
	case WLC_E_P2PO_ADD_DEVICE:
	case WLC_E_P2PO_DEL_DEVICE:
		DHD_EVENT(("MACEVENT: %s, MAC %s\n", event_name, eabuf));
		break;

#ifdef BT_WIFI_HANDOBER
	case WLC_E_BT_WIFI_HANDOVER_REQ:
		DHD_EVENT(("MACEVENT: %s, MAC %s\n", event_name, eabuf));
		break;
#endif

	default:
		DHD_EVENT(("MACEVENT: %s %d, MAC %s, status %d, reason %d, auth %d\n",
		       event_name, event_type, eabuf, (int)status, (int)reason,
		       (int)auth_type));
		break;
	}

	/* show any appended data */
	if (DHD_BYTES_ON() && DHD_EVENT_ON() && datalen) {
		buf = (uchar *) event_data;
		BCM_REFERENCE(buf);
		DHD_EVENT((" data (%d) : ", datalen));
		for (i = 0; i < datalen; i++)
			DHD_EVENT((" 0x%02x ", *buf++));
		DHD_EVENT(("\n"));
	}
}
#endif /* SHOW_EVENTS */

/* Check whether packet is a BRCM event pkt. If it is, record event data. */
int
wl_host_event_get_data(void *pktdata, uint pktlen, bcm_event_msg_u_t *evu)
{
	int ret;

	ret = is_wlc_event_frame(pktdata, pktlen, 0, evu);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s: Invalid event frame, err = %d\n",
			__FUNCTION__, ret));
	}

	return ret;
}

int
wl_process_host_event(dhd_pub_t *dhd_pub, int *ifidx, void *pktdata, uint pktlen,
	wl_event_msg_t *event, void **data_ptr, void *raw_event)
{
	bcm_event_t *pvt_data = (bcm_event_t *)pktdata;
	bcm_event_msg_u_t evu;
	uint8 *event_data;
	uint32 type, status, datalen;
	uint16 flags;
	uint evlen;
	int ret;
	int hostidx;
	uint16 usr_subtype;

	ret = wl_host_event_get_data(pktdata, pktlen, &evu);
	if (ret != BCME_OK) {
		return ret;
	}

	usr_subtype = ntoh16_ua((void *)&pvt_data->bcm_hdr.usr_subtype);
	switch (usr_subtype) {
	case BCMILCP_BCM_SUBTYPE_EVENT:
		memcpy(event, &evu.event, sizeof(wl_event_msg_t));
		*data_ptr = &pvt_data[1];
		break;
	case BCMILCP_BCM_SUBTYPE_DNGLEVENT:
#ifdef DNGL_EVENT_SUPPORT
		/* If it is a DNGL event process it first */
		if (dngl_host_event(dhd_pub, pktdata, &evu.dngl_event, pktlen) == BCME_OK) {
			/*
			 * Return error purposely to prevent DNGL event being processed
			 * as BRCM event
			 */
			return BCME_ERROR;
		}
#endif /* DNGL_EVENT_SUPPORT */
		return BCME_NOTFOUND;
	default:
		return BCME_NOTFOUND;
	}

	/* start wl_event_msg process */
	event_data = *data_ptr;
	type = ntoh32_ua((void *)&event->event_type);
	flags = ntoh16_ua((void *)&event->flags);
	status = ntoh32_ua((void *)&event->status);
	datalen = ntoh32_ua((void *)&event->datalen);
	evlen = datalen + sizeof(bcm_event_t);

	/* find equivalent host index for event ifidx */
	hostidx = dhd_ifidx2hostidx(dhd_pub->info, event->ifidx);

	switch (type) {
#ifdef PROP_TXSTATUS
	case WLC_E_FIFO_CREDIT_MAP:
		dhd_wlfc_enable(dhd_pub);
		dhd_wlfc_FIFOcreditmap_event(dhd_pub, event_data);
		WLFC_DBGMESG(("WLC_E_FIFO_CREDIT_MAP:(AC0,AC1,AC2,AC3),(BC_MC),(OTHER): "
			"(%d,%d,%d,%d),(%d),(%d)\n", event_data[0], event_data[1],
			event_data[2],
			event_data[3], event_data[4], event_data[5]));
		break;

	case WLC_E_BCMC_CREDIT_SUPPORT:
		dhd_wlfc_BCMCCredit_support_event(dhd_pub);
		break;
#endif

	case WLC_E_IF:
		{
		struct wl_event_data_if *ifevent = (struct wl_event_data_if *)event_data;

		/* Ignore the event if NOIF is set */
		if (ifevent->reserved & WLC_E_IF_FLAGS_BSSCFG_NOIF) {
			DHD_ERROR(("WLC_E_IF: NO_IF set, event Ignored\r\n"));
			return (BCME_UNSUPPORTED);
		}
#ifdef PCIE_FULL_DONGLE
		dhd_update_interface_flow_info(dhd_pub, ifevent->ifidx,
			ifevent->opcode, ifevent->role);
#endif
#ifdef PROP_TXSTATUS
		{
			uint8* ea = pvt_data->eth.ether_dhost;
			WLFC_DBGMESG(("WLC_E_IF: idx:%d, action:%s, iftype:%s, "
			              "[%02x:%02x:%02x:%02x:%02x:%02x]\n",
			              ifevent->ifidx,
			              ((ifevent->opcode == WLC_E_IF_ADD) ? "ADD":"DEL"),
			              ((ifevent->role == 0) ? "STA":"AP "),
			              ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]));
			(void)ea;

			if (ifevent->opcode == WLC_E_IF_CHANGE)
				dhd_wlfc_interface_event(dhd_pub,
					eWLFC_MAC_ENTRY_ACTION_UPDATE,
					ifevent->ifidx, ifevent->role, ea);
			else
				dhd_wlfc_interface_event(dhd_pub,
					((ifevent->opcode == WLC_E_IF_ADD) ?
					eWLFC_MAC_ENTRY_ACTION_ADD : eWLFC_MAC_ENTRY_ACTION_DEL),
					ifevent->ifidx, ifevent->role, ea);

			/* dhd already has created an interface by default, for 0 */
			if (ifevent->ifidx == 0)
				break;
		}
#endif /* PROP_TXSTATUS */

		if (ifevent->ifidx > 0 && ifevent->ifidx < DHD_MAX_IFS) {
			if (ifevent->opcode == WLC_E_IF_ADD) {
				if (dhd_event_ifadd(dhd_pub->info, ifevent, event->ifname,
					event->addr.octet)) {

					DHD_ERROR(("%s: dhd_event_ifadd failed ifidx: %d  %s\n",
						__FUNCTION__, ifevent->ifidx, event->ifname));
					return (BCME_ERROR);
				}
			} else if (ifevent->opcode == WLC_E_IF_DEL) {
				dhd_event_ifdel(dhd_pub->info, ifevent, event->ifname,
					event->addr.octet);
				/* Return ifidx (for vitual i/f, it will be > 0)
				 * so that no other operations on deleted interface
				 * are carried out
				 */
				return ifevent->ifidx;
			} else if (ifevent->opcode == WLC_E_IF_CHANGE) {
#ifdef WL_CFG80211
				wl_cfg80211_notify_ifchange(ifevent->ifidx,
					event->ifname, event->addr.octet, ifevent->bssidx);
#endif /* WL_CFG80211 */
			}
		} else {
#if !defined(PROP_TXSTATUS) && !defined(PCIE_FULL_DONGLE) && defined(WL_CFG80211)
			DHD_ERROR(("%s: Invalid ifidx %d for %s\n",
			           __FUNCTION__, ifevent->ifidx, event->ifname));
#endif /* !PROP_TXSTATUS && !PCIE_FULL_DONGLE && WL_CFG80211 */
		}
			/* send up the if event: btamp user needs it */
			*ifidx = hostidx;
			/* push up to external supp/auth */
			dhd_event(dhd_pub->info, (char *)pvt_data, evlen, *ifidx);
		break;
	}

	case WLC_E_NDIS_LINK:
		break;
	case WLC_E_PFN_NET_FOUND:
	case WLC_E_PFN_SCAN_ALLGONE: /* share with WLC_E_PFN_BSSID_NET_LOST */
	case WLC_E_PFN_NET_LOST:
		break;
#if defined(PNO_SUPPORT)
	case WLC_E_PFN_BSSID_NET_FOUND:
	case WLC_E_PFN_BEST_BATCHING:
		dhd_pno_event_handler(dhd_pub, event, (void *)event_data);
		break;
#endif 
#if defined(RTT_SUPPORT)
	case WLC_E_PROXD:
		dhd_rtt_event_handler(dhd_pub, event, (void *)event_data);
		break;
#endif /* RTT_SUPPORT */
		/* These are what external supplicant/authenticator wants */
	case WLC_E_ASSOC_IND:
	case WLC_E_AUTH_IND:
	case WLC_E_REASSOC_IND:
		dhd_findadd_sta(dhd_pub, hostidx, &event->addr.octet);
		break;
	case WLC_E_LINK:
#ifdef PCIE_FULL_DONGLE
		if (dhd_update_interface_link_status(dhd_pub, (uint8)hostidx,
			(uint8)flags) != BCME_OK)
			break;
		if (!flags) {
			dhd_flow_rings_delete(dhd_pub, hostidx);
		}
		/* fall through */
#endif
	case WLC_E_DEAUTH:
	case WLC_E_DEAUTH_IND:
	case WLC_E_DISASSOC:
	case WLC_E_DISASSOC_IND:
		if (type != WLC_E_LINK) {
			dhd_del_sta(dhd_pub, hostidx, &event->addr.octet);
		}
		DHD_EVENT(("%s: Link event %d, flags %x, status %x\n",
		           __FUNCTION__, type, flags, status));
#ifdef PCIE_FULL_DONGLE
		if (type != WLC_E_LINK) {
			uint8 ifindex = (uint8)hostidx;
			uint8 role = dhd_flow_rings_ifindex2role(dhd_pub, ifindex);
			if (DHD_IF_ROLE_STA(role)) {
				dhd_flow_rings_delete(dhd_pub, ifindex);
			} else {
				dhd_flow_rings_delete_for_peer(dhd_pub, ifindex,
					&event->addr.octet[0]);
			}
		}
#endif
		/* fall through */
	default:
		*ifidx = hostidx;
		/* push up to external supp/auth */
		dhd_event(dhd_pub->info, (char *)pvt_data, evlen, *ifidx);
		DHD_TRACE(("%s: MAC event %d, flags %x, status %x\n",
		           __FUNCTION__, type, flags, status));
		BCM_REFERENCE(flags);
		BCM_REFERENCE(status);

		break;
	}

#ifdef SHOW_EVENTS
	wl_show_host_event(dhd_pub, event,
		(void *)event_data, raw_event, dhd_pub->enable_log);
#endif /* SHOW_EVENTS */

	return (BCME_OK);
}

int
wl_host_event(dhd_pub_t *dhd_pub, int *ifidx, void *pktdata, uint pktlen,
	wl_event_msg_t *event, void **data_ptr, void *raw_event)
{
	return wl_process_host_event(dhd_pub, ifidx, pktdata, pktlen, event, data_ptr,
			raw_event);
}

void
wl_event_to_host_order(wl_event_msg_t * evt)
{
	/* Event struct members passed from dongle to host are stored in network
	 * byte order. Convert all members to host-order.
	 */
	evt->event_type = ntoh32(evt->event_type);
	evt->flags = ntoh16(evt->flags);
	evt->status = ntoh32(evt->status);
	evt->reason = ntoh32(evt->reason);
	evt->auth_type = ntoh32(evt->auth_type);
	evt->datalen = ntoh32(evt->datalen);
	evt->version = ntoh16(evt->version);
}

void
dhd_print_buf(void *pbuf, int len, int bytes_per_line)
{
#ifdef DHD_DEBUG
	int i, j = 0;
	unsigned char *buf = pbuf;

	if (bytes_per_line == 0) {
		bytes_per_line = len;
	}

	for (i = 0; i < len; i++) {
		printf("%2.2x", *buf++);
		j++;
		if (j == bytes_per_line) {
			printf("\n");
			j = 0;
		} else {
			printf(":");
		}
	}
	printf("\n");
#endif /* DHD_DEBUG */
}
#ifndef strtoul
#define strtoul(nptr, endptr, base) bcm_strtoul((nptr), (endptr), (base))
#endif

#ifdef PKT_FILTER_SUPPORT
/* Convert user's input in hex pattern to byte-size mask */
static int
wl_pattern_atoh(char *src, char *dst)
{
	int i;
	if (strncmp(src, "0x", 2) != 0 &&
	    strncmp(src, "0X", 2) != 0) {
		DHD_ERROR(("Mask invalid format. Needs to start with 0x\n"));
		return -1;
	}
	src = src + 2; /* Skip past 0x */
	if (strlen(src) % 2 != 0) {
		DHD_ERROR(("Mask invalid format. Needs to be of even length\n"));
		return -1;
	}
	for (i = 0; *src != '\0'; i++) {
		char num[3];
		bcm_strncpy_s(num, sizeof(num), src, 2);
		num[2] = '\0';
		dst[i] = (uint8)strtoul(num, NULL, 16);
		src += 2;
	}
	return i;
}

void
dhd_pktfilter_offload_enable(dhd_pub_t * dhd, char *arg, int enable, int master_mode)
{
	char				*argv[8];
	int					i = 0;
	const char			*str;
	int					buf_len;
	int					str_len;
	char				*arg_save = 0, *arg_org = 0;
	int					rc;
	char				buf[32] = {0};
	wl_pkt_filter_enable_t	enable_parm;
	wl_pkt_filter_enable_t	* pkt_filterp;

	if (!arg)
		return;

	if (!(arg_save = MALLOC(dhd->osh, strlen(arg) + 1))) {
		DHD_ERROR(("%s: malloc failed\n", __FUNCTION__));
		goto fail;
	}
	arg_org = arg_save;
	memcpy(arg_save, arg, strlen(arg) + 1);

	argv[i] = bcmstrtok(&arg_save, " ", 0);

	i = 0;
	if (argv[i] == NULL) {
		DHD_ERROR(("No args provided\n"));
		goto fail;
	}

	str = "pkt_filter_enable";
	str_len = strlen(str);
	bcm_strncpy_s(buf, sizeof(buf) - 1, str, sizeof(buf) - 1);
	buf[ sizeof(buf) - 1 ] = '\0';
	buf_len = str_len + 1;

	pkt_filterp = (wl_pkt_filter_enable_t *)(buf + str_len + 1);

	/* Parse packet filter id. */
	enable_parm.id = htod32(strtoul(argv[i], NULL, 0));

	/* Parse enable/disable value. */
	enable_parm.enable = htod32(enable);

	buf_len += sizeof(enable_parm);
	memcpy((char *)pkt_filterp,
	       &enable_parm,
	       sizeof(enable_parm));

	/* Enable/disable the specified filter. */
	rc = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, buf, buf_len, TRUE, 0);
	rc = rc >= 0 ? 0 : rc;
	if (rc)
		DHD_TRACE(("%s: failed to add pktfilter %s, retcode = %d\n",
		__FUNCTION__, arg, rc));
	else
		DHD_TRACE(("%s: successfully added pktfilter %s\n",
		__FUNCTION__, arg));

	/* Contorl the master mode */
	rc = dhd_wl_ioctl_set_intiovar(dhd, "pkt_filter_mode",
		master_mode, WLC_SET_VAR, TRUE, 0);
	rc = rc >= 0 ? 0 : rc;
	if (rc)
		DHD_TRACE(("%s: failed to add pktfilter %s, retcode = %d\n",
		__FUNCTION__, arg, rc));

fail:
	if (arg_org)
		MFREE(dhd->osh, arg_org, strlen(arg) + 1);
}

void
dhd_pktfilter_offload_set(dhd_pub_t * dhd, char *arg)
{
	const char 			*str;
	wl_pkt_filter_t		pkt_filter;
	wl_pkt_filter_t		*pkt_filterp;
	int					buf_len;
	int					str_len;
	int 				rc;
	uint32				mask_size;
	uint32				pattern_size;
	char				*argv[8], * buf = 0;
	int					i = 0;
	char				*arg_save = 0, *arg_org = 0;
#define BUF_SIZE		2048

	if (!arg)
		return;

	if (!(arg_save = MALLOC(dhd->osh, strlen(arg) + 1))) {
		DHD_ERROR(("%s: malloc failed\n", __FUNCTION__));
		goto fail;
	}

	arg_org = arg_save;

	if (!(buf = MALLOC(dhd->osh, BUF_SIZE))) {
		DHD_ERROR(("%s: malloc failed\n", __FUNCTION__));
		goto fail;
	}
	memset(buf, 0, BUF_SIZE);
	memcpy(arg_save, arg, strlen(arg) + 1);

	if (strlen(arg) > BUF_SIZE) {
		DHD_ERROR(("Not enough buffer %d < %d\n", (int)strlen(arg), (int)sizeof(buf)));
		goto fail;
	}

	argv[i] = bcmstrtok(&arg_save, " ", 0);
	while (argv[i++])
		argv[i] = bcmstrtok(&arg_save, " ", 0);

	i = 0;
	if (argv[i] == NULL) {
		DHD_ERROR(("No args provided\n"));
		goto fail;
	}

	str = "pkt_filter_add";
	str_len = strlen(str);
	bcm_strncpy_s(buf, BUF_SIZE, str, str_len);
	buf[ str_len ] = '\0';
	buf_len = str_len + 1;

	pkt_filterp = (wl_pkt_filter_t *) (buf + str_len + 1);

	/* Parse packet filter id. */
	pkt_filter.id = htod32(strtoul(argv[i], NULL, 0));

	if (argv[++i] == NULL) {
		DHD_ERROR(("Polarity not provided\n"));
		goto fail;
	}

	/* Parse filter polarity. */
	pkt_filter.negate_match = htod32(strtoul(argv[i], NULL, 0));

	if (argv[++i] == NULL) {
		DHD_ERROR(("Filter type not provided\n"));
		goto fail;
	}

	/* Parse filter type. */
	pkt_filter.type = htod32(strtoul(argv[i], NULL, 0));

	if (argv[++i] == NULL) {
		DHD_ERROR(("Offset not provided\n"));
		goto fail;
	}

	/* Parse pattern filter offset. */
	pkt_filter.u.pattern.offset = htod32(strtoul(argv[i], NULL, 0));

	if (argv[++i] == NULL) {
		DHD_ERROR(("Bitmask not provided\n"));
		goto fail;
	}

	/* Parse pattern filter mask. */
	mask_size =
		htod32(wl_pattern_atoh(argv[i], (char *) pkt_filterp->u.pattern.mask_and_pattern));

	if (argv[++i] == NULL) {
		DHD_ERROR(("Pattern not provided\n"));
		goto fail;
	}

	/* Parse pattern filter pattern. */
	pattern_size =
		htod32(wl_pattern_atoh(argv[i],
	         (char *) &pkt_filterp->u.pattern.mask_and_pattern[mask_size]));

	if (mask_size != pattern_size) {
		DHD_ERROR(("Mask and pattern not the same size\n"));
		goto fail;
	}

	pkt_filter.u.pattern.size_bytes = mask_size;
	buf_len += WL_PKT_FILTER_FIXED_LEN;
	buf_len += (WL_PKT_FILTER_PATTERN_FIXED_LEN + 2 * mask_size);

	/* Keep-alive attributes are set in local	variable (keep_alive_pkt), and
	** then memcpy'ed into buffer (keep_alive_pktp) since there is no
	** guarantee that the buffer is properly aligned.
	*/
	memcpy((char *)pkt_filterp,
	       &pkt_filter,
	       WL_PKT_FILTER_FIXED_LEN + WL_PKT_FILTER_PATTERN_FIXED_LEN);

	rc = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, buf, buf_len, TRUE, 0);
	rc = rc >= 0 ? 0 : rc;

	if (rc)
		DHD_TRACE(("%s: failed to add pktfilter %s, retcode = %d\n",
		__FUNCTION__, arg, rc));
	else
		DHD_TRACE(("%s: successfully added pktfilter %s\n",
		__FUNCTION__, arg));

fail:
	if (arg_org)
		MFREE(dhd->osh, arg_org, strlen(arg) + 1);

	if (buf)
		MFREE(dhd->osh, buf, BUF_SIZE);
}

void dhd_pktfilter_offload_delete(dhd_pub_t *dhd, int id)
{
	int ret;

	ret = dhd_wl_ioctl_set_intiovar(dhd, "pkt_filter_delete",
		id, WLC_SET_VAR, TRUE, 0);
	if (ret < 0) {
		DHD_ERROR(("%s: Failed to delete filter ID:%d, ret=%d\n",
			__FUNCTION__, id, ret));
	}
}
#endif /* PKT_FILTER_SUPPORT */

/* ========================== */
/* ==== ARP OFFLOAD SUPPORT = */
/* ========================== */
#ifdef ARP_OFFLOAD_SUPPORT
void
dhd_arp_offload_set(dhd_pub_t * dhd, int arp_mode)
{
	int retcode;

	retcode = dhd_wl_ioctl_set_intiovar(dhd, "arp_ol",
		arp_mode, WLC_SET_VAR, TRUE, 0);

	retcode = retcode >= 0 ? 0 : retcode;
	if (retcode)
		DHD_TRACE(("%s: failed to set ARP offload mode to 0x%x, retcode = %d\n",
			__FUNCTION__, arp_mode, retcode));
	else
		DHD_TRACE(("%s: successfully set ARP offload mode to 0x%x\n",
			__FUNCTION__, arp_mode));
}

void
dhd_arp_offload_enable(dhd_pub_t * dhd, int arp_enable)
{
	int retcode;

	retcode = dhd_wl_ioctl_set_intiovar(dhd, "arpoe",
		arp_enable, WLC_SET_VAR, TRUE, 0);

	retcode = retcode >= 0 ? 0 : retcode;
	if (retcode)
		DHD_TRACE(("%s: failed to enabe ARP offload to %d, retcode = %d\n",
			__FUNCTION__, arp_enable, retcode));
	else
		DHD_TRACE(("%s: successfully enabed ARP offload to %d\n",
			__FUNCTION__, arp_enable));
	if (arp_enable) {
		uint32 version;
		retcode = dhd_wl_ioctl_get_intiovar(dhd, "arp_version",
			&version, WLC_GET_VAR, FALSE, 0);
		if (retcode) {
			DHD_INFO(("%s: fail to get version (maybe version 1:retcode = %d\n",
				__FUNCTION__, retcode));
			dhd->arp_version = 1;
		}
		else {
			DHD_INFO(("%s: ARP Version= %x\n", __FUNCTION__, version));
			dhd->arp_version = version;
		}
	}
}

void
dhd_aoe_arp_clr(dhd_pub_t *dhd, int idx)
{
	int ret = 0;

	if (dhd == NULL) return;
	if (dhd->arp_version == 1)
		idx = 0;

	ret = dhd_iovar(dhd, idx, "arp_table_clear", NULL, 0, NULL, 0, TRUE);
	if (ret < 0)
		DHD_ERROR(("%s failed code %d\n", __FUNCTION__, ret));
}

void
dhd_aoe_hostip_clr(dhd_pub_t *dhd, int idx)
{
	int ret = 0;

	if (dhd == NULL) return;
	if (dhd->arp_version == 1)
		idx = 0;

	ret = dhd_iovar(dhd, idx, "arp_hostip_clear", NULL, 0, NULL, 0, TRUE);
	if (ret < 0)
		DHD_ERROR(("%s failed code %d\n", __FUNCTION__, ret));
}

void
dhd_arp_offload_add_ip(dhd_pub_t *dhd, uint32 ipaddr, int idx)
{
	int ret;

	if (dhd == NULL) return;
	if (dhd->arp_version == 1)
		idx = 0;

	ret = dhd_iovar(dhd, idx, "arp_hostip", (char *)&ipaddr, sizeof(ipaddr),
			NULL, 0, TRUE);
	if (ret)
		DHD_TRACE(("%s: ARP ip addr add failed, ret = %d\n", __FUNCTION__, ret));
	else
		DHD_TRACE(("%s: sARP H ipaddr entry added \n",
		__FUNCTION__));
}

int
dhd_arp_get_arp_hostip_table(dhd_pub_t *dhd, void *buf, int buflen, int idx)
{
	int ret, i;
	uint32 *ptr32 = buf;
	bool clr_bottom = FALSE;

	if (!buf)
		return -1;
	if (dhd == NULL) return -1;
	if (dhd->arp_version == 1)
		idx = 0;

	ret = dhd_iovar(dhd, idx, "arp_hostip", NULL, 0, (char *)buf, buflen,
			FALSE);
	if (ret) {
		DHD_TRACE(("%s: ioctl WLC_GET_VAR error %d\n",
		__FUNCTION__, ret));

		return -1;
	}

	/* clean up the buf, ascii reminder */
	for (i = 0; i < MAX_IPV4_ENTRIES; i++) {
		if (!clr_bottom) {
			if (*ptr32 == 0)
				clr_bottom = TRUE;
		} else {
			*ptr32 = 0;
		}
		ptr32++;
	}

	return 0;
}
#endif /* ARP_OFFLOAD_SUPPORT  */

/*
 * Neighbor Discovery Offload: enable NDO feature
 * Called  by ipv6 event handler when interface comes up/goes down
 */
int
dhd_ndo_enable(dhd_pub_t * dhd, int ndo_enable)
{
	int retcode;

	if (dhd == NULL)
		return -1;

	retcode = dhd_wl_ioctl_set_intiovar(dhd, "ndoe",
		ndo_enable, WLC_SET_VAR, TRUE, 0);
	if (retcode)
		DHD_ERROR(("%s: failed to enabe ndo to %d, retcode = %d\n",
			__FUNCTION__, ndo_enable, retcode));
	else
		DHD_TRACE(("%s: successfully enabed ndo offload to %d\n",
			__FUNCTION__, ndo_enable));

	return retcode;
}

/*
 * Neighbor Discover Offload: enable NDO feature
 * Called  by ipv6 event handler when interface comes up
 */
int
dhd_ndo_add_ip(dhd_pub_t *dhd, char* ipv6addr, int idx)
{
	int iov_len = 0;
	char iovbuf[DHD_IOVAR_BUF_SIZE] = {0};
	int retcode;

	if (dhd == NULL)
		return -1;

	iov_len = bcm_mkiovar("nd_hostip", (char *)ipv6addr,
		IPV6_ADDR_LEN, iovbuf, sizeof(iovbuf));
	if (!iov_len) {
		DHD_ERROR(("%s: Insufficient iovar buffer size %zu \n",
			__FUNCTION__, sizeof(iovbuf)));
		return -1;
	}
	retcode = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, iov_len, TRUE, idx);

	if (retcode)
		DHD_ERROR(("%s: ndo ip addr add failed, retcode = %d\n",
		__FUNCTION__, retcode));
	else
		DHD_TRACE(("%s: ndo ipaddr entry added \n",
		__FUNCTION__));

	return retcode;
}
/*
 * Neighbor Discover Offload: enable NDO feature
 * Called  by ipv6 event handler when interface goes down
 */
int
dhd_ndo_remove_ip(dhd_pub_t *dhd, int idx)
{
	int iov_len = 0;
	char iovbuf[DHD_IOVAR_BUF_SIZE] = {0};
	int retcode;

	if (dhd == NULL)
		return -1;

	iov_len = bcm_mkiovar("nd_hostip_clear", NULL,
		0, iovbuf, sizeof(iovbuf));
	if (!iov_len) {
		DHD_ERROR(("%s: Insufficient iovar buffer size %zu \n",
			__FUNCTION__, sizeof(iovbuf)));
		return -1;
	}
	retcode = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, iov_len, TRUE, idx);

	if (retcode) {
		DHD_ERROR(("%s: ndo ip addr remove returned (%d)\n",
		__FUNCTION__, retcode));
	} else {
		DHD_TRACE(("%s: ndo ipaddr entry removed \n",
		__FUNCTION__));
	}

	return retcode;
}

/* send up locally generated event */
void
dhd_sendup_event_common(dhd_pub_t *dhdp, wl_event_msg_t *event, void *data)
{
	switch (ntoh32(event->event_type)) {
#ifdef WLBTAMP
	case WLC_E_BTA_HCI_EVENT:
		break;
#endif /* WLBTAMP */
	default:
		break;
	}

	/* Call per-port handler. */
	dhd_sendup_event(dhdp, event, data);
}

#ifdef SIMPLE_ISCAN

uint iscan_thread_id = 0;
iscan_buf_t * iscan_chain = 0;

iscan_buf_t *
dhd_iscan_allocate_buf(dhd_pub_t *dhd, iscan_buf_t **iscanbuf)
{
	iscan_buf_t *iscanbuf_alloc = 0;
	iscan_buf_t *iscanbuf_head;

	DHD_ISCAN(("%s: Entered\n", __FUNCTION__));
	dhd_iscan_lock();

	iscanbuf_alloc = (iscan_buf_t*)MALLOC(dhd->osh, sizeof(iscan_buf_t));
	if (iscanbuf_alloc == NULL)
		goto fail;

	iscanbuf_alloc->next = NULL;
	iscanbuf_head = *iscanbuf;

	DHD_ISCAN(("%s: addr of allocated node = 0x%X"
		   "addr of iscanbuf_head = 0x%X dhd = 0x%X\n",
		   __FUNCTION__, iscanbuf_alloc, iscanbuf_head, dhd));

	if (iscanbuf_head == NULL) {
		*iscanbuf = iscanbuf_alloc;
		DHD_ISCAN(("%s: Head is allocated\n", __FUNCTION__));
		goto fail;
	}

	while (iscanbuf_head->next)
		iscanbuf_head = iscanbuf_head->next;

	iscanbuf_head->next = iscanbuf_alloc;

fail:
	dhd_iscan_unlock();
	return iscanbuf_alloc;
}

void
dhd_iscan_free_buf(void *dhdp, iscan_buf_t *iscan_delete)
{
	iscan_buf_t *iscanbuf_free = 0;
	iscan_buf_t *iscanbuf_prv = 0;
	iscan_buf_t *iscanbuf_cur;
	dhd_pub_t *dhd = dhd_bus_pub(dhdp);
	DHD_ISCAN(("%s: Entered\n", __FUNCTION__));

	dhd_iscan_lock();

	iscanbuf_cur = iscan_chain;

	/* If iscan_delete is null then delete the entire
	 * chain or else delete specific one provided
	 */
	if (!iscan_delete) {
		while (iscanbuf_cur) {
			iscanbuf_free = iscanbuf_cur;
			iscanbuf_cur = iscanbuf_cur->next;
			iscanbuf_free->next = 0;
			MFREE(dhd->osh, iscanbuf_free, sizeof(iscan_buf_t));
		}
		iscan_chain = 0;
	} else {
		while (iscanbuf_cur) {
			if (iscanbuf_cur == iscan_delete)
				break;
			iscanbuf_prv = iscanbuf_cur;
			iscanbuf_cur = iscanbuf_cur->next;
		}
		if (iscanbuf_prv)
			iscanbuf_prv->next = iscan_delete->next;

		iscan_delete->next = 0;
		MFREE(dhd->osh, iscan_delete, sizeof(iscan_buf_t));

		if (!iscanbuf_prv)
			iscan_chain = 0;
	}
	dhd_iscan_unlock();
}

iscan_buf_t *
dhd_iscan_result_buf(void)
{
	return iscan_chain;
}

int
dhd_iscan_issue_request(void * dhdp, wl_iscan_params_t *pParams, uint32 size)
{
	int rc = -1;
	dhd_pub_t *dhd = dhd_bus_pub(dhdp);
	char *buf;
	char iovar[] = "iscan";
	uint32 allocSize = 0;
	wl_ioctl_t ioctl;

	if (pParams) {
		allocSize = (size + strlen(iovar) + 1);
		if ((allocSize < size) || (allocSize < strlen(iovar)))
		{
			DHD_ERROR(("%s: overflow - allocation size too large %d < %d + %d!\n",
				__FUNCTION__, allocSize, size, strlen(iovar)));
			goto cleanUp;
		}
		buf = MALLOC(dhd->osh, allocSize);

		if (buf == NULL)
			{
			DHD_ERROR(("%s: malloc of size %d failed!\n", __FUNCTION__, allocSize));
			goto cleanUp;
			}
		ioctl.cmd = WLC_SET_VAR;
		bcm_mkiovar(iovar, (char *)pParams, size, buf, allocSize);
		rc = dhd_wl_ioctl(dhd, 0, &ioctl, buf, allocSize);
	}

cleanUp:
	if (buf) {
		MFREE(dhd->osh, buf, allocSize);
	}

	return rc;
}

static int
dhd_iscan_get_partial_result(void *dhdp, uint *scan_count)
{
	wl_iscan_results_t *list_buf;
	wl_iscan_results_t list;
	wl_scan_results_t *results;
	iscan_buf_t *iscan_cur;
	int status = -1;
	dhd_pub_t *dhd = dhd_bus_pub(dhdp);
	int rc;
	wl_ioctl_t ioctl;

	DHD_ISCAN(("%s: Enter\n", __FUNCTION__));

	iscan_cur = dhd_iscan_allocate_buf(dhd, &iscan_chain);
	if (!iscan_cur) {
		DHD_ERROR(("%s: Failed to allocate node\n", __FUNCTION__));
		dhd_iscan_free_buf(dhdp, 0);
		dhd_iscan_request(dhdp, WL_SCAN_ACTION_ABORT);
		dhd_ind_scan_confirm(dhdp, FALSE);
		goto fail;
	}

	dhd_iscan_lock();

	memset(iscan_cur->iscan_buf, 0, WLC_IW_ISCAN_MAXLEN);
	list_buf = (wl_iscan_results_t*)iscan_cur->iscan_buf;
	results = &list_buf->results;
	results->buflen = WL_ISCAN_RESULTS_FIXED_SIZE;
	results->version = 0;
	results->count = 0;

	memset(&list, 0, sizeof(list));
	list.results.buflen = htod32(WLC_IW_ISCAN_MAXLEN);
	bcm_mkiovar("iscanresults", (char *)&list, WL_ISCAN_RESULTS_FIXED_SIZE,
		iscan_cur->iscan_buf, WLC_IW_ISCAN_MAXLEN);
	ioctl.cmd = WLC_GET_VAR;
	ioctl.set = FALSE;
	rc = dhd_wl_ioctl(dhd, 0, &ioctl, iscan_cur->iscan_buf, WLC_IW_ISCAN_MAXLEN);

	results->buflen = dtoh32(results->buflen);
	results->version = dtoh32(results->version);
	*scan_count = results->count = dtoh32(results->count);
	status = dtoh32(list_buf->status);
	DHD_ISCAN(("%s: Got %d resuls status = (%x)\n", __FUNCTION__, results->count, status));

	dhd_iscan_unlock();

	if (!(*scan_count)) {
		 /* TODO: race condition when FLUSH already called */
		dhd_iscan_free_buf(dhdp, 0);
	}
fail:
	return status;
}

#endif /* SIMPLE_ISCAN */

/*
 * returns = TRUE if associated, FALSE if not associated
 */
bool dhd_is_associated(dhd_pub_t *dhd, uint8 ifidx, int *retval)
{
	char bssid[6], zbuf[6];
	int ret = -1;

	bzero(bssid, 6);
	bzero(zbuf, 6);

	ret  = dhd_wl_ioctl_cmd(dhd, WLC_GET_BSSID, (char *)&bssid,
		ETHER_ADDR_LEN, FALSE, ifidx);
	DHD_TRACE((" %s WLC_GET_BSSID ioctl res = %d\n", __FUNCTION__, ret));

	if (ret == BCME_NOTASSOCIATED) {
		DHD_TRACE(("%s: not associated! res:%d\n", __FUNCTION__, ret));
	}

	if (retval)
		*retval = ret;

	if (ret < 0)
		return FALSE;

	if ((memcmp(bssid, zbuf, ETHER_ADDR_LEN) == 0)) {
		DHD_TRACE(("%s: WLC_GET_BSSID ioctl returned zero bssid\n", __FUNCTION__));
		return FALSE;
	}
	return TRUE;
}

/* Function to estimate possible DTIM_SKIP value */
int
dhd_get_suspend_bcn_li_dtim(dhd_pub_t *dhd)
{
	int bcn_li_dtim = 1; /* deafult no dtim skip setting */
	int ret = -1;
	int dtim_period = 0;
	int ap_beacon = 0;
	int allowed_skip_dtim_cnt = 0;
	/* Check if associated */
	if (dhd_is_associated(dhd, 0, NULL) == FALSE) {
		DHD_TRACE(("%s NOT assoc ret %d\n", __FUNCTION__, ret));
		goto exit;
	}

	/* read associated AP beacon interval */
	if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_GET_BCNPRD,
		&ap_beacon, sizeof(ap_beacon), FALSE, 0)) < 0) {
		DHD_ERROR(("%s get beacon failed code %d\n", __FUNCTION__, ret));
		goto exit;
	}

	/* read associated ap's dtim setup */
	if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_GET_DTIMPRD,
		&dtim_period, sizeof(dtim_period), FALSE, 0)) < 0) {
		DHD_ERROR(("%s failed code %d\n", __FUNCTION__, ret));
		goto exit;
	}

	/* if not assocated just eixt */
	if (dtim_period == 0) {
		goto exit;
	}

	if (dhd->max_dtim_enable) {
		bcn_li_dtim = (int) (MAX_DTIM_ALLOWED_INTERVAL / (ap_beacon * dtim_period));
		if (bcn_li_dtim == 0) {
			bcn_li_dtim = 1;
		}
	} else {
		/* attemp to use platform defined dtim skip interval */
		bcn_li_dtim = dhd->suspend_bcn_li_dtim;

		/* check if sta listen interval fits into AP dtim */
		if (dtim_period > CUSTOM_LISTEN_INTERVAL) {
			/* AP DTIM to big for our Listen Interval : no dtim skiping */
			bcn_li_dtim = NO_DTIM_SKIP;
			DHD_ERROR(("%s DTIM=%d > Listen=%d : too big ...\n",
				__FUNCTION__, dtim_period, CUSTOM_LISTEN_INTERVAL));
			goto exit;
		}

		if ((dtim_period * ap_beacon * bcn_li_dtim) > MAX_DTIM_ALLOWED_INTERVAL) {
			allowed_skip_dtim_cnt =
				MAX_DTIM_ALLOWED_INTERVAL / (dtim_period * ap_beacon);
			bcn_li_dtim =
				(allowed_skip_dtim_cnt != 0) ? allowed_skip_dtim_cnt : NO_DTIM_SKIP;
		}

		if ((bcn_li_dtim * dtim_period) > CUSTOM_LISTEN_INTERVAL) {
			/* Round up dtim_skip to fit into STAs Listen Interval */
			bcn_li_dtim = (int)(CUSTOM_LISTEN_INTERVAL / dtim_period);
			DHD_TRACE(("%s agjust dtim_skip as %d\n", __FUNCTION__, bcn_li_dtim));
		}
	}

	DHD_ERROR(("%s beacon=%d bcn_li_dtim=%d DTIM=%d Listen=%d\n",
		__FUNCTION__, ap_beacon, bcn_li_dtim, dtim_period, CUSTOM_LISTEN_INTERVAL));

exit:
	return bcn_li_dtim;
}

/* Check if the mode supports STA MODE */
bool dhd_support_sta_mode(dhd_pub_t *dhd)
{

#ifdef  WL_CFG80211
	if (!(dhd->op_mode & DHD_FLAG_STA_MODE))
		return FALSE;
	else
#endif /* WL_CFG80211 */
		return TRUE;
}

#if defined(KEEP_ALIVE)
int dhd_keep_alive_onoff(dhd_pub_t *dhd)
{
	wl_mkeep_alive_pkt_t	mkeep_alive_pkt = {0};

	if (!dhd_support_sta_mode(dhd))
		return -1;

	DHD_TRACE(("%s execution\n", __FUNCTION__));

	mkeep_alive_pkt.period_msec = CUSTOM_KEEP_ALIVE_SETTING;
	mkeep_alive_pkt.version = htod16(WL_MKEEP_ALIVE_VERSION);
	mkeep_alive_pkt.length = htod16(WL_MKEEP_ALIVE_FIXED_LEN);
	/* Setup keep alive zero for null packet generation */
	mkeep_alive_pkt.keep_alive_id = 0;
	mkeep_alive_pkt.len_bytes = 0;
	return dhd_iovar(dhd, 0, "mkeep_alive", (char*)&mkeep_alive_pkt, WL_MKEEP_ALIVE_FIXED_LEN,
			NULL, 0, TRUE);
}
#endif /* defined(KEEP_ALIVE) */

#define CSCAN_TLV_TYPE_SSID_IE	'S'
/*
 *  SSIDs list parsing from cscan tlv list
 */
int
wl_parse_ssid_list_tlv(char** list_str, wlc_ssid_ext_t* ssid, int max, int *bytes_left)
{
	char* str;
	int idx = 0;

	if ((list_str == NULL) || (*list_str == NULL) || (*bytes_left < 0)) {
		DHD_ERROR(("%s error paramters\n", __FUNCTION__));
		return -1;
	}
	str = *list_str;
	while (*bytes_left > 0) {

		if (str[0] != CSCAN_TLV_TYPE_SSID_IE) {
			*list_str = str;
			DHD_TRACE(("nssid=%d left_parse=%d %d\n", idx, *bytes_left, str[0]));
			return idx;
		}

		/* Get proper CSCAN_TLV_TYPE_SSID_IE */
		*bytes_left -= 1;
		str += 1;

		if (str[0] == 0) {
			/* Broadcast SSID */
			ssid[idx].SSID_len = 0;
			memset((char*)ssid[idx].SSID, 0x0, DOT11_MAX_SSID_LEN);
			*bytes_left -= 1;
			str += 1;

			DHD_TRACE(("BROADCAST SCAN  left=%d\n", *bytes_left));
		}
		else if (str[0] <= DOT11_MAX_SSID_LEN) {
			/* Get proper SSID size */
			ssid[idx].SSID_len = str[0];
			*bytes_left -= 1;
			str += 1;

			/* Get SSID */
			if (ssid[idx].SSID_len > *bytes_left) {
				DHD_ERROR(("%s out of memory range len=%d but left=%d\n",
				__FUNCTION__, ssid[idx].SSID_len, *bytes_left));
				return -1;
			}

			memcpy((char*)ssid[idx].SSID, str, ssid[idx].SSID_len);

			*bytes_left -= ssid[idx].SSID_len;
			str += ssid[idx].SSID_len;
			ssid[idx].hidden = TRUE;

			DHD_TRACE(("%s :size=%d left=%d\n",
				(char*)ssid[idx].SSID, ssid[idx].SSID_len, *bytes_left));
		}
		else {
			DHD_ERROR(("### SSID size more that %d\n", str[0]));
			return -1;
		}

		if (idx++ >  max) {
			DHD_ERROR(("%s number of SSIDs more that %d\n", __FUNCTION__, idx));
			return -1;
		}
	}

	*list_str = str;
	return idx;
}

#if defined(WL_WIRELESS_EXT)
/* Android ComboSCAN support */

/*
 *  data parsing from ComboScan tlv list
*/
int
wl_iw_parse_data_tlv(char** list_str, void *dst, int dst_size, const char token,
                     int input_size, int *bytes_left)
{
	char* str;
	uint16 short_temp;
	uint32 int_temp;

	if ((list_str == NULL) || (*list_str == NULL) ||(bytes_left == NULL) || (*bytes_left < 0)) {
		DHD_ERROR(("%s error paramters\n", __FUNCTION__));
		return -1;
	}
	str = *list_str;

	/* Clean all dest bytes */
	memset(dst, 0, dst_size);
	while (*bytes_left > 0) {

		if (str[0] != token) {
			DHD_TRACE(("%s NOT Type=%d get=%d left_parse=%d \n",
				__FUNCTION__, token, str[0], *bytes_left));
			return -1;
		}

		*bytes_left -= 1;
		str += 1;

		if (input_size == 1) {
			memcpy(dst, str, input_size);
		}
		else if (input_size == 2) {
			memcpy(dst, (char *)htod16(memcpy(&short_temp, str, input_size)),
				input_size);
		}
		else if (input_size == 4) {
			memcpy(dst, (char *)htod32(memcpy(&int_temp, str, input_size)),
				input_size);
		}

		*bytes_left -= input_size;
		str += input_size;
		*list_str = str;
		return 1;
	}
	return 1;
}

/*
 *  channel list parsing from cscan tlv list
*/
int
wl_iw_parse_channel_list_tlv(char** list_str, uint16* channel_list,
                             int channel_num, int *bytes_left)
{
	char* str;
	int idx = 0;

	if ((list_str == NULL) || (*list_str == NULL) ||(bytes_left == NULL) || (*bytes_left < 0)) {
		DHD_ERROR(("%s error paramters\n", __FUNCTION__));
		return -1;
	}
	str = *list_str;

	while (*bytes_left > 0) {

		if (str[0] != CSCAN_TLV_TYPE_CHANNEL_IE) {
			*list_str = str;
			DHD_TRACE(("End channel=%d left_parse=%d %d\n", idx, *bytes_left, str[0]));
			return idx;
		}
		/* Get proper CSCAN_TLV_TYPE_CHANNEL_IE */
		*bytes_left -= 1;
		str += 1;

		if (str[0] == 0) {
			/* All channels */
			channel_list[idx] = 0x0;
		}
		else {
			channel_list[idx] = (uint16)str[0];
			DHD_TRACE(("%s channel=%d \n", __FUNCTION__,  channel_list[idx]));
		}
		*bytes_left -= 1;
		str += 1;

		if (idx++ > 255) {
			DHD_ERROR(("%s Too many channels \n", __FUNCTION__));
			return -1;
		}
	}

	*list_str = str;
	return idx;
}

/* Parse a comma-separated list from list_str into ssid array, starting
 * at index idx.  Max specifies size of the ssid array.  Parses ssids
 * and returns updated idx; if idx >= max not all fit, the excess have
 * not been copied.  Returns -1 on empty string, or on ssid too long.
 */
int
wl_iw_parse_ssid_list(char** list_str, wlc_ssid_t* ssid, int idx, int max)
{
	char* str, *ptr;

	if ((list_str == NULL) || (*list_str == NULL))
		return -1;

	for (str = *list_str; str != NULL; str = ptr) {

		/* check for next TAG */
		if (!strncmp(str, GET_CHANNEL, strlen(GET_CHANNEL))) {
			*list_str	 = str + strlen(GET_CHANNEL);
			return idx;
		}

		if ((ptr = strchr(str, ',')) != NULL) {
			*ptr++ = '\0';
		}

		if (strlen(str) > DOT11_MAX_SSID_LEN) {
			DHD_ERROR(("ssid <%s> exceeds %d\n", str, DOT11_MAX_SSID_LEN));
			return -1;
		}

		if (strlen(str) == 0)
			ssid[idx].SSID_len = 0;

		if (idx < max) {
			bzero(ssid[idx].SSID, sizeof(ssid[idx].SSID));
			strncpy((char*)ssid[idx].SSID, str, sizeof(ssid[idx].SSID) - 1);
			ssid[idx].SSID_len = strlen(str);
		}
		idx++;
	}
	return idx;
}

/*
 * Parse channel list from iwpriv CSCAN
 */
int
wl_iw_parse_channel_list(char** list_str, uint16* channel_list, int channel_num)
{
	int num;
	int val;
	char* str;
	char* endptr = NULL;

	if ((list_str == NULL)||(*list_str == NULL))
		return -1;

	str = *list_str;
	num = 0;
	while (strncmp(str, GET_NPROBE, strlen(GET_NPROBE))) {
		val = (int)strtoul(str, &endptr, 0);
		if (endptr == str) {
			printf("could not parse channel number starting at"
				" substring \"%s\" in list:\n%s\n",
				str, *list_str);
			return -1;
		}
		str = endptr + strspn(endptr, " ,");

		if (num == channel_num) {
			DHD_ERROR(("too many channels (more than %d) in channel list:\n%s\n",
				channel_num, *list_str));
			return -1;
		}

		channel_list[num++] = (uint16)val;
	}
	*list_str = str;
	return num;
}

#endif 


#if defined(DHD_8021X_DUMP)
/* Parse EAPOL 4 way handshake messages */
void
dhd_dump_eapol_4way_message(char *dump_data, bool direction)
{
	unsigned char type;
	int pair, ack, mic, kerr, req, sec, install;
	unsigned short us_tmp;
	type = dump_data[18];
	if (type == 2 || type == 254) {
		us_tmp = (dump_data[19] << 8) | dump_data[20];
		pair =  0 != (us_tmp & 0x08);
		ack = 0  != (us_tmp & 0x80);
		mic = 0  != (us_tmp & 0x100);
		kerr =  0 != (us_tmp & 0x400);
		req = 0  != (us_tmp & 0x800);
		sec = 0  != (us_tmp & 0x200);
		install  = 0 != (us_tmp & 0x40);
		if (!sec && !mic && ack && !install && pair && !kerr && !req) {
			DHD_ERROR(("ETHER_TYPE_802_1X [%s] : M1 of 4way\n",
				direction ? "TX" : "RX"));
		} else if (pair && !install && !ack && mic && !sec && !kerr && !req) {
			DHD_ERROR(("ETHER_TYPE_802_1X [%s] : M2 of 4way\n",
				direction ? "TX" : "RX"));
		} else if (pair && ack && mic && sec && !kerr && !req) {
			DHD_ERROR(("ETHER_TYPE_802_1X [%s] : M3 of 4way\n",
				direction ? "TX" : "RX"));
		} else if (pair && !install && !ack && mic && sec && !req && !kerr) {
			DHD_ERROR(("ETHER_TYPE_802_1X [%s] : M4 of 4way\n",
				direction ? "TX" : "RX"));
		} else {
			DHD_ERROR(("ETHER_TYPE_802_1X [%s]: ver %d, type %d, replay %d\n",
				direction ? "TX" : "RX",
				dump_data[14], dump_data[15], dump_data[30]));
		}
	} else {
		DHD_ERROR(("ETHER_TYPE_802_1X [%s]: ver %d, type %d, replay %d\n",
				direction ? "TX" : "RX",
				dump_data[14], dump_data[15], dump_data[30]));
	}
}
#endif /* DHD_8021X_DUMP */

int
dhd_download_2_dongle(dhd_pub_t	*dhd, char *iovar, uint16 flag, uint16 dload_type,
	unsigned char *dload_buf, int len)
{
	struct wl_dload_data *dload_ptr = (struct wl_dload_data *)dload_buf;
	int err = 0;
	int dload_data_offset;
	static char iovar_buf[WLC_IOCTL_MEDLEN];
	int iovar_len;

	memset(iovar_buf, 0, sizeof(iovar_buf));

	dload_data_offset = OFFSETOF(wl_dload_data_t, data);
	dload_ptr->flag = (DLOAD_HANDLER_VER << DLOAD_FLAG_VER_SHIFT) | flag;
	dload_ptr->dload_type = dload_type;
	dload_ptr->len = htod32(len - dload_data_offset);
	dload_ptr->crc = 0;
	len = ROUNDUP(len, 8);

	iovar_len = bcm_mkiovar(iovar, dload_buf,
		(uint)len, iovar_buf, sizeof(iovar_buf));
	if (iovar_len == 0) {
		DHD_ERROR(("%s: insufficient buffer space passed to bcm_mkiovar for '%s' \n",
		           __FUNCTION__, iovar));
		return BCME_BUFTOOSHORT;
	}

	err = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovar_buf,
			iovar_len, IOV_SET, 0);

	return err;
}

int
dhd_download_clm_blob(dhd_pub_t *dhd, unsigned char *image, uint32 len)
{
	int chunk_len;
	int size2alloc;
	unsigned char *new_buf;
	int err = 0, data_offset;
	uint16 dl_flag = DL_BEGIN;

	data_offset = OFFSETOF(wl_dload_data_t, data);
	size2alloc = data_offset + MAX_CHUNK_LEN;
	size2alloc = ROUNDUP(size2alloc, 8);

	if ((new_buf = (unsigned char *)MALLOCZ(dhd->osh, size2alloc)) != NULL) {
		do {
			chunk_len = dhd_os_get_image_block((char *)(new_buf + data_offset),
				MAX_CHUNK_LEN, image);
			if (chunk_len < 0) {
				DHD_ERROR(("%s: dhd_os_get_image_block failed (%d)\n",
					__FUNCTION__, chunk_len));
				err = BCME_ERROR;
				goto exit;
			}

			if (len - chunk_len == 0)
				dl_flag |= DL_END;

			err = dhd_download_2_dongle(dhd, "clmload", dl_flag, DL_TYPE_CLM,
				new_buf, data_offset + chunk_len);

			dl_flag &= ~DL_BEGIN;

			len = len - chunk_len;
		} while ((len > 0) && (err == 0));
	} else {
		err = BCME_NOMEM;
	}
exit:
	if (new_buf) {
		MFREE(dhd->osh, new_buf, size2alloc);
	}
	return err;
}

int
dhd_check_current_clm_data(dhd_pub_t *dhd)
{
	char iovbuf[WLC_IOCTL_SMLEN];
	wl_country_t *cspec;
	int err = BCME_OK;

	bcm_mkiovar("country", NULL, 0, iovbuf, sizeof(iovbuf));
	err = dhd_wl_ioctl_cmd(dhd, WLC_GET_VAR, iovbuf, sizeof(iovbuf), FALSE, 0);
	if (err) {
		DHD_ERROR(("%s: country code get failed\n", __FUNCTION__));
		return err;
	}

	cspec = (wl_country_t *)iovbuf;
	if ((strncmp(cspec->ccode, WL_CCODE_NULL_COUNTRY, WLC_CNTRY_BUF_SZ)) == 0) {
		DHD_ERROR(("%s: ----- This FW is not included CLM data -----\n",
			__FUNCTION__));
		return FALSE;
	}
	DHD_ERROR(("%s: ----- This FW is included CLM data -----\n",
		__FUNCTION__));
	return TRUE;
}

int
dhd_apply_default_clm(dhd_pub_t *dhd, char *clm_path)
{
	char *clm_blob_path;
	int len;
	unsigned char *imgbuf = NULL;
	int err = BCME_OK;
	char iovbuf[WLC_IOCTL_SMLEN] = {0};
	int status = FALSE;

	if (clm_path[0] != '\0') {
		if (strlen(clm_path) > MOD_PARAM_PATHLEN) {
			DHD_ERROR(("clm path exceeds max len\n"));
			return BCME_ERROR;
		}
		clm_blob_path = clm_path;
		DHD_TRACE(("clm path from module param:%s\n", clm_path));
	} else {
		clm_blob_path = CONFIG_BCMDHD_CLM_PATH;
	}

	/* If CLM blob file is found on the filesystem, download the file.
	 * After CLM file download or If the blob file is not present,
	 * validate the country code before proceeding with the initialization.
	 * If country code is not valid, fail the initialization.
	 */

	imgbuf = dhd_os_open_image((char *)clm_blob_path);
	if (imgbuf == NULL) {
#if defined(DHD_BLOB_EXISTENCE_CHECK)
		if (dhd->is_blob) {
			err = BCME_ERROR;
		} else {
			status = dhd_check_current_clm_data(dhd);
			if (status == TRUE) {
				err = BCME_OK;
			} else {
				err = BCME_ERROR;
			}
		}
#endif /* DHD_BLOB_EXISTENCE_CHECK */
		goto exit;
	}

	len = dhd_os_get_image_size(imgbuf);

	if ((len > 0) && (len < MAX_CLM_BUF_SIZE) && imgbuf) {
		status = dhd_check_current_clm_data(dhd);
		if (status == TRUE) {
#if defined(DHD_BLOB_EXISTENCE_CHECK)
			if (dhd->op_mode != DHD_FLAG_MFG_MODE) {
				if (dhd->is_blob) {
					err = BCME_ERROR;
				}
				goto exit;
			}
#else
			DHD_ERROR(("%s: CLM already exist in F/W, "
				"new CLM data will be added to the end of existing CLM data!\n",
				__FUNCTION__));
#endif /* DHD_BLOB_EXISTENCE_CHECK */
		}

		/* Found blob file. Download the file */
		DHD_TRACE(("clm file download from %s \n", clm_blob_path));
		err = dhd_download_clm_blob(dhd, imgbuf, len);
		if (err) {
			DHD_ERROR(("%s: CLM download failed err=%d\n", __FUNCTION__, err));
			/* Retrieve clmload_status and print */
			bcm_mkiovar("clmload_status", NULL, 0, iovbuf, sizeof(iovbuf));
			err = dhd_wl_ioctl_cmd(dhd, WLC_GET_VAR, iovbuf, sizeof(iovbuf), FALSE, 0);
			if (err) {
				DHD_ERROR(("%s: clmload_status get failed err=%d \n",
					__FUNCTION__, err));
			} else {
				DHD_ERROR(("%s: clmload_status: %d \n",
					__FUNCTION__, *((int *)iovbuf)));
				if (*((int *)iovbuf) == CHIPID_MISMATCH) {
					DHD_ERROR(("Chip ID mismatch error \n"));
				}
			}
			err = BCME_ERROR;
			goto exit;
		} else {
			DHD_INFO(("%s: CLM download succeeded \n", __FUNCTION__));
		}
	} else {
		DHD_INFO(("Skipping the clm download. len:%d memblk:%p \n", len, imgbuf));
	}

	/* Verify country code */
	status = dhd_check_current_clm_data(dhd);

	if (status != TRUE) {
		/* Country code not initialized or CLM download not proper */
		DHD_ERROR(("country code not initialized\n"));
		err = BCME_ERROR;
	}

exit:

	if (imgbuf) {
		dhd_os_close_image(imgbuf);
	}

	return err;
}
