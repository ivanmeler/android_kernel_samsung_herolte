/*
* Wifi dongle status logging and report
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
* <<Broadcom-WL-IPTag/Open:>>
*
* $Id: wl_statreport.c 735359 2017-12-08 10:56:04Z $
*/
#include <wlc_types.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <wl_statreport.h>
#include <wl_cfg80211.h>
#include <wldev_common.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_linux.h>
#include <linux/fs.h>

#ifndef LINKSTAT_SUPPORT
#error "LINK STAT is NOT supported"
#endif /* LINKSTAT_SUPPORT */
#ifndef DHD_LOG_DUMP
#error "DHD_LOG_DUMP is not supported"
#endif /* DHD_LOG_DUMP */

#define WSR_REPORT_VERSION	20170620
#define WSR_REPORT_MAX_DATA	64
#define WSR_REPORT_MAX_WPA_CNT	5
#define WSR_INVALID	(-1)

#define WSR_CNT_SIZE	(1300 + 256)

/* restrict maximum value less than 100M to reduce string length */
#define WSR_CNT_VALUE_MAX	100000000

#define WSR_NONSEC_TO_SEC		1000000000
#define WSR_REPORT_YEAR_MUL	10000
#define WSR_REPORT_MON_MUL		100
#define WSR_REPORT_HOUR_MUL	10000
#define WSR_REPORT_MIN_MUL		100
#define WSR_REPORT_MINUTES		60
#define WSR_REPORT_YEAR_BASE	1900

#define WSR_MAX_OUI	3
#define OUI_PAD_LEN (4 - (WSR_MAX_OUI * DOT11_OUI_LEN) % 4)

#define WSR_MINIMIZE_REPORT

/* logging connection info to follow up connection changes */
typedef struct stat_report_conn {
	uint8 bssid[ETHER_ADDR_LEN]; /* connected bssid */
	uint8 oui_cnt;				/* number of vndr OUI of connected AP */
	uint8 pad1;
	uint32 dhd_opmode;			/* dhd operation mode : dhd_op_flags */
	uint8 oui_list[WSR_MAX_OUI][DOT11_OUI_LEN]; /* vndr OUI list */
	uint8 pad2[OUI_PAD_LEN];
} wsr_conn_t;

/* Kernel local clock is not elapsed at suspend mode
 * but kernel logs show this value at the logs
 * to avoid calculation print local clock
 * and boot up time(of date/time format)
 */
typedef struct stat_report_clock {
	uint32 kernel_clock;		/* kernel local clock */
	uint32 date;				/* date of WSR format */
	uint32 time;				/* time of WSR format */
} wsr_clock_t;

typedef struct stat_report_adps {
	uint8 mode;				/* ADPS mdoe 0=off, 1 = on */
	uint8 flags;			/* ADPS restrict flags 0 = adps work, not 0 = paused */
	uint8 current_step;		/* ADPS step 0 = LOW PM STEP, 1 = HIGH PM STEP */
	uint8 pad;
	uint32 duration_high;	/* total duration in HIGH PM STEP in mili-second unit */
	uint32 counts_high;		/* total number of HIGH PM STEP */
	uint32 duration_low;	/* total duration in LOW PM STEP in mili-second unit */
	uint32 counts_low;		/* total inumber of LOW PM STEP */
} wsr_adps_t;

typedef struct stat_report_link {
	int8 lqcm;				/* lqcm value -1 : INVALID, 0~8 : LQCM step */
	int8 rssi;				/* RSSI : -128 ~ 0 */
	uint8 snr;				/* SNR : 0 ~ 100 */
	uint8 pad;
} wsr_link_t;

typedef struct stat_report_phy_tx_power {
	uint8 sar_enable;			/* SAR Enable/Disable */
	uint8 phy_rsdb_mode;		/* PHY RSDB mode 0-MIMO, 1-RSDB */

	/* 2.4G/5G SAR power capping value per core[0/1], Unit [qdBm] */
	uint8 sar_limit_2g_core0;
	uint8 sar_limit_2g_core1;
	uint8 sar_limit_5g_core0;
	uint8 sar_limit_5g_core1;
	uint8 fcc_enable;			/* FCC Enable/Disable */
	uint8 fccpwrch12;			/* FCC power capping value of 2.4G 12ch [qdBm] */
	uint8 fccpwrch13;			/* FCC power capping value of 2.4G 13ch [qdBm] */
	uint8 bss_local_max;		/* BSS Local Max [qdBm] */
	uint8 bss_local_constraint;	/* BSS Local Constraint [qdBm] */
	uint8 dsss_1mbps_2g;		/* Target power - 2.4G DSSS 1Mbps rate [qdBm] */
	uint8 ofdm_6mbps_2g;		/* Target power - 2.4G OFDM 6Mbps rate [qdBm] */
	uint8 mcs_0_2g;				/* Target power - 2.4G MCS 0 rate [qdBm] */
	uint8 ofdm_6mbps_5g;		/* Target power - 5G OFDM 6Mbps rate [qdBm] */
	uint8 mcs_0_5g;				/* Target power - 5G MCS 0 rate [qdBm] */
} wsr_tx_power_t;

typedef struct stat_report_elem {
	wsr_clock_t clock;
	wsr_conn_t conn;
	char cntinfo[WSR_CNT_SIZE];
	wsr_link_t link_quality;
	wsr_adps_t adps;
	wsr_tx_power_t tx_power;
} wsr_elem_t;

typedef struct wl_stat_report_info {
	uint32 version; /* WSR version */

	int enabled;	/* enabled/disabled */

	/* ring buf variables */
	struct mutex ring_sync; /* mutex for critical section of ring buff */
	int write_idx;	/* next write index, -1 : not started */
	int read_idx;	/* next read index, -1 : not start */
	/* conn idx : first connection of this connection */
	/* -1 : not started if read = -1, same to read if read is not -1 */
	/* counts between conn & write shall be less than MAX_REPORT */
	int conn_idx;

	/* protected elements during seiralization */
	int lock_idx;	/* start index of locked, element will not be overried */
	int lock_count; /* number of locked, from lock idx */

	/* serialzation variables */
	int seral_mode;	/* serialize mode */
	int trans_buf_len;	/* configed buffer length per serialize transaction */
	int seral_state;	/* state of serialize */
	int serial_sub_state;	/* sub state of each state, currently idx only */

	/* saved data elements */
	wsr_elem_t elem[WSR_REPORT_MAX_DATA];
} wsr_info_t;

/* ring buf control functions */
static inline int wsr_get_write_idx(wsr_info_t *wsr_info);
static inline int wsr_get_first_idx(wsr_info_t *wsr_info);
static inline int wsr_get_last_idx(wsr_info_t *wsr_info);
static inline int wsr_get_next_idx(wsr_info_t *wsr_info, int cur_idx);
static inline void wsr_set_conn_idx(wsr_info_t *wsr_info);
static inline int wsr_get_conn_idx(wsr_info_t *wsr_info, int max_cnt);
static inline int wsr_get_count(int start_idx, int end_idx);

/* ring buf serializing record protection lock */
static inline void wsr_set_lock_idx(wsr_info_t *wsr_info, int max_cnt);
static inline void wsr_free_lock_idx(wsr_info_t *wsr_info);
static inline int wsr_get_lock_idx(wsr_info_t *wsr_info);
static inline void wsr_decr_lock_count(wsr_info_t *wsr_info);
static inline int wsr_get_lock_cnt(wsr_info_t *wsr_info);
/* ring buff internal functions , no mutex lock */
static int __wsr_get_write_idx(wsr_info_t *wsr_info);
static inline int __wsr_get_first_idx(wsr_info_t *wsr_info);
static int __wsr_get_next_idx(wsr_info_t *wsr_info, int cur_idx);
static int __wsr_get_conn_idx(wsr_info_t *wsr_info, int max_cnt);

/* status gathering functions */
static inline void wsr_copy_cntinfo(wsr_elem_t *elem, wl_cnt_info_t *cntinfo);
static void wsr_get_conn_info(struct bcm_cfg80211 *cfg, wsr_elem_t *elem);
static void wsr_get_clock_info(wsr_elem_t *elem);
static void wsr_gather_link_quality(struct bcm_cfg80211 *cfg, wsr_elem_t *elem);
static void wsr_gather_adps(struct bcm_cfg80211 *cfg, wsr_elem_t *elem);

#if defined(WSR_DEBUG)

#define WSR_DEBUG_PRD	5	/* print debug every 5 recrod */
typedef struct wpa_stat_report_elem {
	wsr_clock_t clock;
	wsr_conn_t conn;

	/* Counters */
	uint32 txframe;
	uint32 rxmgocast;

	/* Other Values */
	wsr_link_t link_quality;
	wsr_adps_t adps;
} wsr_dbg_elem_t;

static void wsr_dbg_main(wsr_info_t *wsr_info);
static void wsr_dbg_copy_elem(wsr_dbg_elem_t *dest, wsr_elem_t *src);
static void wsr_dbg_copy_cnts(wsr_dbg_elem_t *dest, wsr_elem_t *src);
static inline void wsr_dbg_copy_conn(wsr_dbg_elem_t *dest, wsr_elem_t *src);
static inline void wsr_dbg_copy_clock(wsr_dbg_elem_t *dest, wsr_elem_t *src);
static inline void wsr_dbg_copy_link(wsr_dbg_elem_t *dest, wsr_elem_t *src);
static inline void wsr_dbg_copy_adps(wsr_dbg_elem_t *dest, wsr_elem_t *src);
static void wsr_dbg_print_elem(wsr_dbg_elem_t *elem);
#endif /* WSR_DEBUG */

#ifdef STAT_REPORT_TEMP_STATIC
#undef DHD_OS_PREALLOC
#undef DHD_OS_PREFREE

#define DHD_OS_PREALLOC(dhdpub, section, size) ({\
		int kflags; \
		void *__ret; \
		kflags = in_atomic() ?GFP_ATOMIC : GFP_KERNEL; \
		__ret = kzalloc(size, kflags); \
		__ret; \
})

#define DHD_OS_PREFREE(dhdpub, addr, size) kfree(addr)
#endif /* STAT_REPORT_TEMP_STATIC */

/* ========= Module functions : exposed to others ============= */
int
wl_attach_stat_report(void *cfg)
{
	struct bcm_cfg80211 *pcfg = (struct bcm_cfg80211 *)cfg;
	wsr_info_t *wsr_info;
	dhd_pub_t *dhdp = pcfg->pub;

	if (!pcfg) {
		WL_ERR(("FAIL to ATTACH STAT REPORT\n"));
		return BCME_ERROR;
	}

	BCM_REFERENCE(dhdp);
	wsr_info = (wsr_info_t *)DHD_OS_PREALLOC(dhdp,
		DHD_PREALLOC_STAT_REPORT_BUF, sizeof(wsr_info_t));
	if (unlikely(!wsr_info)) {
		WL_ERR(("FAIL to alloc for STAT REPORT INFO\n"));
		return BCME_ERROR;
	}

	/* Initialize control block */
	mutex_init(&wsr_info->ring_sync);
	wsr_info->version = WSR_REPORT_VERSION;
	wsr_info->enabled = TRUE;
	wsr_info->read_idx = WSR_INVALID;
	wsr_info->write_idx = WSR_INVALID;
	wsr_info->conn_idx = WSR_INVALID;
	wsr_info->lock_idx = WSR_INVALID;
	pcfg->stat_report_info = wsr_info;
	((dhd_pub_t *)(pcfg->pub))->stat_report_info = wsr_info;
	return BCME_OK;
}

void
wl_detach_stat_report(void *cfg)
{
	struct bcm_cfg80211 *pcfg = (struct bcm_cfg80211 *)cfg;
	wsr_info_t *wsr_info;
	dhd_pub_t *dhdp = pcfg->pub;

	if (!pcfg) {
		return;
	}
	BCM_REFERENCE(dhdp);
	if (pcfg->stat_report_info) {
		wsr_info = (wsr_info_t *)pcfg->stat_report_info;
		mutex_destroy(&wsr_info->ring_sync);
		DHD_OS_PREFREE(dhdp, pcfg->stat_report_info,
			sizeof(wsr_info_t));
		pcfg->stat_report_info = NULL;
	}
}

/* query dongle status data */
void
wl_stat_report_gather(void *cfg, void *cnt)
{
	int write_idx;
	wl_cnt_info_t *cntinfo = (wl_cnt_info_t *)cnt;
	struct bcm_cfg80211 *pcfg = (struct bcm_cfg80211 *)cfg;
	wsr_info_t *wsr_info = pcfg->stat_report_info;
	wsr_elem_t *elem;

	/* Check Validation of counters */
	if (cntinfo->version < WL_CNT_VERSION_XTLV) {
		WL_ERR(("NOT supported CNT version\n"));
		return;
	}
	if (wsr_info == NULL) {
		WL_INFORM(("NOT enabled\n"));
		return;
	}

	/* no available slot, due to oldest slot is locked */
	write_idx = wsr_get_write_idx(wsr_info);
	if (write_idx == WSR_INVALID) {
		WL_ERR(("SKIP to logging due to locking\n"));
		return;
	}

	elem = &wsr_info->elem[write_idx];
	wsr_get_conn_info(pcfg, elem);
	wsr_get_clock_info(elem);
	wsr_copy_cntinfo(elem, cntinfo);
	wsr_gather_link_quality(pcfg, elem);
	wsr_gather_adps(pcfg, elem);

#if defined(WSR_DEBUG)
	{
		static int pr_cnt = 0;
		if ((pr_cnt++) % WSR_DEBUG_PRD == 0) {
			wsr_dbg_main(wsr_info);
		}
	}
#endif /* WSR_DEBUG */
}

void
wl_stat_report_notify_connected(void *cfg)
{
	struct bcm_cfg80211 *pcfg = (struct bcm_cfg80211 *)cfg;
	wsr_info_t *wsr_info = pcfg->stat_report_info;

	/* Set conn idx to next write idx */
	wsr_set_conn_idx(wsr_info);
}

/* ========= Ring buffer management ============= */
/* Get next index can be written
 * will overwrite which doesn't read
 * will return -1 if next pointer is locked
 */
static int
__wsr_get_write_idx(wsr_info_t *wsr_info)
{
	int tmp_idx;

	if (wsr_info->read_idx == WSR_INVALID) {
		wsr_info->read_idx = wsr_info->write_idx = 0;
		return wsr_info->write_idx;
	}

	/* check next index is not locked */
	tmp_idx = (wsr_info->write_idx + 1) % WSR_REPORT_MAX_DATA;
	if (wsr_info->lock_idx == tmp_idx) {
		return WSR_INVALID;
	}

	wsr_info->write_idx = tmp_idx;
	if (wsr_info->write_idx == wsr_info->read_idx) {
		/* record is full, drop oldest one */
		wsr_info->read_idx = (wsr_info->read_idx + 1) % WSR_REPORT_MAX_DATA;

		/* set all data is for this connection */
		if (wsr_info->read_idx == wsr_info->conn_idx) {
			wsr_info->conn_idx = WSR_INVALID;
		}
	}
	return wsr_info->write_idx;

}

static inline int
wsr_get_write_idx(wsr_info_t *wsr_info)
{
	int ret_idx;
	mutex_lock(&wsr_info->ring_sync);
	ret_idx = __wsr_get_write_idx(wsr_info);
	mutex_unlock(&wsr_info->ring_sync);
	return ret_idx;
}

/* Get read index : oldest element */
static inline int
__wsr_get_first_idx(wsr_info_t *wsr_info)
{
	if (wsr_info->read_idx == WSR_INVALID) {
		return WSR_INVALID;
	}

	return wsr_info->read_idx;
}

static inline int
wsr_get_first_idx(wsr_info_t *wsr_info)
{
	int ret_idx;
	mutex_lock(&wsr_info->ring_sync);
	ret_idx = __wsr_get_first_idx(wsr_info);
	mutex_unlock(&wsr_info->ring_sync);
	return ret_idx;
}

/* Get latest element */
static inline int
wsr_get_last_idx(wsr_info_t *wsr_info)
{
	int ret_idx;
	mutex_lock(&wsr_info->ring_sync);
	ret_idx = wsr_info->write_idx;
	mutex_unlock(&wsr_info->ring_sync);
	return ret_idx;
}

/* get counts between two indexes of ring buffer */
static inline int
wsr_get_count(int start_idx, int end_idx)
{
	if (start_idx == WSR_INVALID || end_idx == WSR_INVALID) {
		return 0;
	}

	return (WSR_REPORT_MAX_DATA + end_idx - start_idx) % WSR_REPORT_MAX_DATA + 1;
}

/* Set conn idx to next write idx */
static inline void
wsr_set_conn_idx(wsr_info_t *wsr_info)
{
	mutex_lock(&wsr_info->ring_sync);
	wsr_info->conn_idx = __wsr_get_next_idx(wsr_info, wsr_info->write_idx);
	mutex_unlock(&wsr_info->ring_sync);
}

/* Get first record of this connection
 * if conn_idx = -1, all record is for this connection, return first
 * if counts btwn. conn and write, then return (write - max)
 * else return conn idx
 */
static int
__wsr_get_conn_idx(wsr_info_t *wsr_info, int max_cnt)
{
	int counts;

	if (wsr_info->read_idx == WSR_INVALID) {
		return WSR_INVALID;
	}

	if (wsr_info->conn_idx == WSR_INVALID) {
		counts = wsr_get_count(wsr_info->read_idx, wsr_info->write_idx);
	} else {
		counts = wsr_get_count(wsr_info->conn_idx, wsr_info->write_idx);
	}

	counts = MIN(counts, max_cnt);
	return (WSR_REPORT_MAX_DATA + wsr_info->write_idx - counts + 1) % WSR_REPORT_MAX_DATA;
}

static inline int
wsr_get_conn_idx(wsr_info_t *wsr_info, int max_cnt)
{
	int con_idx;
	mutex_lock(&wsr_info->ring_sync);
	con_idx = __wsr_get_conn_idx(wsr_info, max_cnt);
	mutex_unlock(&wsr_info->ring_sync);
	return con_idx;
}

static int
__wsr_get_next_idx(wsr_info_t *wsr_info, int cur_idx)
{
	if (wsr_info->read_idx == WSR_INVALID) {
		return WSR_INVALID;
	}

	if (cur_idx == wsr_info->write_idx) {
		/* no more new record */
		return WSR_INVALID;
	}

	return ((cur_idx +1) % WSR_REPORT_MAX_DATA);
}

static inline int
wsr_get_next_idx(wsr_info_t *wsr_info, int cur_idx)
{
	int next_idx;
	mutex_lock(&wsr_info->ring_sync);
	next_idx = __wsr_get_next_idx(wsr_info, cur_idx);
	mutex_unlock(&wsr_info->ring_sync);
	return next_idx;
}

static inline void
wsr_set_lock_idx(wsr_info_t *wsr_info, int max_cnt)
{
	mutex_lock(&wsr_info->ring_sync);
	wsr_info->lock_idx = __wsr_get_conn_idx(wsr_info, max_cnt);
	wsr_info->lock_count = wsr_get_count(wsr_info->lock_idx, wsr_info->write_idx);
	mutex_unlock(&wsr_info->ring_sync);
}

static inline void
wsr_free_lock_idx(wsr_info_t *wsr_info)
{
	mutex_lock(&wsr_info->ring_sync);
	wsr_info->lock_idx = WSR_INVALID;
	wsr_info->lock_count = 0;
	mutex_unlock(&wsr_info->ring_sync);
}

static inline int
wsr_get_lock_idx(wsr_info_t *wsr_info)
{
	int ret_idx;
	mutex_lock(&wsr_info->ring_sync);
	ret_idx = wsr_info->lock_idx;
	mutex_unlock(&wsr_info->ring_sync);
	return ret_idx;
}

static inline void
wsr_decr_lock_count(wsr_info_t *wsr_info)
{

	mutex_lock(&wsr_info->ring_sync);
	wsr_info->lock_count--;
	if (wsr_info->lock_count <= 0) {
		wsr_info->lock_idx = WSR_INVALID;
	} else {
		wsr_info->lock_idx = __wsr_get_next_idx(wsr_info, wsr_info->lock_idx);
	}
	mutex_unlock(&wsr_info->ring_sync);
}

static inline int
wsr_get_lock_cnt(wsr_info_t *wsr_info)
{
	int cnt;
	mutex_lock(&wsr_info->ring_sync);
	cnt = wsr_info->lock_count;
	mutex_unlock(&wsr_info->ring_sync);
	return cnt;
}

/* ========= Status Query Functions ============= */
static inline void
wsr_get_op_mode(struct bcm_cfg80211 *cfg, wsr_elem_t *elem)
{
	dhd_pub_t *dhd = (dhd_pub_t *)(cfg->pub);

	elem->conn.dhd_opmode = dhd->op_mode;
}

static void
wsr_get_conn_info(struct bcm_cfg80211 *cfg, wsr_elem_t *elem)
{
	struct wl_profile *profile = wl_get_profile_by_netdev(cfg, bcmcfg_to_prmry_ndev(cfg));

	memset(&elem->conn, 0, sizeof(elem->conn));
	if (!profile) {
		return;
	}

	memcpy(elem->conn.bssid, profile->bssid, ETHER_ADDR_LEN);
	elem->conn.oui_cnt = wl_cfg80211_get_vndr_ouilist(cfg,
		(uint8 *)&elem->conn.oui_list, WSR_MAX_OUI);

	wsr_get_op_mode(cfg, elem);
	return;
}

static void
wsr_get_clock_info(wsr_elem_t *elem)
{
	struct timespec ts;
	struct tm tm;
	u64 tv_kernel =  local_clock();
	wsr_clock_t *clock = &elem->clock;

	getnstimeofday(&ts);
	time_to_tm((ts.tv_sec - (sys_tz.tz_minuteswest*WSR_REPORT_MINUTES)), 0, &tm);

	/* save seconds only */
	clock->kernel_clock = (uint32)(tv_kernel/WSR_NONSEC_TO_SEC);
	clock->date = (tm.tm_year + WSR_REPORT_YEAR_BASE) * WSR_REPORT_YEAR_MUL;
	clock->date += (tm.tm_mon +1) * WSR_REPORT_MON_MUL;
	clock->date += tm.tm_mday;
	clock->time = tm.tm_hour * WSR_REPORT_HOUR_MUL;
	clock->time += tm.tm_min * WSR_REPORT_MIN_MUL;
	clock->time += tm.tm_sec;
}

static inline void
wsr_copy_cntinfo(wsr_elem_t *elem, wl_cnt_info_t *cntinfo)
{
	uint32 copy_len;
	copy_len = MIN((uint32)OFFSETOF(wl_cnt_info_t, data)+ cntinfo->datalen, WSR_CNT_SIZE);

	memcpy(elem->cntinfo, cntinfo, copy_len);
}

#ifndef LQCM_ENAB_MASK
#define LQCM_ENAB_MASK			0x000000FF	/* LQCM enable flag mask */
#define LQCM_TX_INDEX_MASK		0x0000FF00	/* LQCM tx index mask */
#define LQCM_RX_INDEX_MASK		0x00FF0000	/* LQCM rx index mask */

#define LQCM_TX_INDEX_SHIFT		8	/* LQCM tx index shift */
#define LQCM_RX_INDEX_SHIFT		16	/* LQCM rx index shift */
#endif /* LQCM_ENAB_MASK */

static void
wsr_gather_link_quality(struct bcm_cfg80211 *cfg, wsr_elem_t *elem)
{
	int err;
	uint32 val;
	uint8 uint8_v;
	scb_val_t scbval;

	memset(&elem->link_quality, 0, sizeof(elem->link_quality));

	err = wldev_iovar_getint(bcmcfg_to_prmry_ndev(cfg), "lqcm", &val);
	if (err != BCME_OK) {
		WL_ERR(("failed to get lqcm report, error = %d\n", err));
		return;
	}
	if (!(val & LQCM_ENAB_MASK)) {
		elem->link_quality.lqcm = -1;
	} else {
		uint8_v = (val & LQCM_TX_INDEX_MASK) >> LQCM_TX_INDEX_SHIFT;
		elem->link_quality.lqcm = (int8)uint8_v;
	}

	err = wldev_iovar_getint(bcmcfg_to_prmry_ndev(cfg), "snr", &val);
	if (err != BCME_OK) {
		WL_ERR((" Failed to get SNR %d, error = %d\n", val, err));
		return;
	}
	elem->link_quality.snr = *(uint8 *)&val;

	err = wldev_get_rssi(bcmcfg_to_prmry_ndev(cfg), &scbval);
	if (err != BCME_OK) {
		WL_ERR(("failed to get rssi, error = %d\n", err));
		return;
	}
	elem->link_quality.rssi = *((int8 *)((int32 *)&scbval.val));

}

#if defined(WLADPS) || defined(WLADPS_PRIVATE_CMD)
#ifndef ADPS_MODE_OFF
#define ADPS_MODE_OFF	0
#define ADPS_STEP_HIGH	1
#define ADPS_STEP_LOW	0
#endif /* ADPS_MODE_OFF */

static void
wsr_gather_adps(struct bcm_cfg80211 *cfg, wsr_elem_t *elem)
{
	int err;
	uint8 dump_sub_cmd = 0;
	uint8 *data;
	bcm_iov_buf_t iov_buf;
	bcm_iov_buf_t *resp;
	bcm_tlv_t *tlv;
	char adps_buf[WLC_IOCTL_MEDLEN];
	wl_adps_dump_summary_v1_t *summary;
	wsr_adps_t *adps = &elem->adps;

	memset(&elem->adps, 0, sizeof(elem->adps));

	iov_buf.version = WL_ADPS_IOV_VER;
	iov_buf.len = sizeof(dump_sub_cmd);
	iov_buf.id = WL_ADPS_IOV_DUMP;
	data = (uint8 *)iov_buf.data;
	data[0] = dump_sub_cmd;

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "adps",
		&iov_buf, sizeof(bcm_iov_buf_t), adps_buf, WLC_IOCTL_MEDLEN, NULL);
	if (err) {
		WL_ERR(("failed to get ADPS dump error = %d\n", err));
		return;
	}

	resp = (bcm_iov_buf_t *)adps_buf;
	tlv = (bcm_tlv_t *)resp->data;

	if (!bcm_valid_tlv(tlv, resp->len)) {
		WL_ERR(("Invalid TLV\n"));
		return;
	}

	summary = (wl_adps_dump_summary_v1_t *)tlv->data;
	if (tlv->len < OFFSETOF(wl_adps_dump_summary_v1_t, flags)) {
		WL_ERR(("Id: %x - invaild length (%d)\n", tlv->id, tlv->len));
		return;
	}
	if (summary->mode == ADPS_MODE_OFF) {
		return;
	}

	adps->mode = summary->mode;
	adps->flags = summary->flags;
	adps->current_step = summary->current_step;
	adps->duration_high = summary->stat[ADPS_STEP_HIGH].duration;
	adps->duration_low = summary->stat[ADPS_STEP_LOW].duration;
	adps->counts_high = summary->stat[ADPS_STEP_HIGH].counts;
	adps->counts_low = summary->stat[ADPS_STEP_LOW].counts;
}
#else
static void
wsr_gather_adps(struct bcm_cfg80211 *cfg, wsr_elem_t *elem)
{
	WL_ERR(("ADPS is not enabled\n"));
}
#endif /* defined(WLADPS) || defined(WLADPS_PRIVATE_CMD) */
/* ========= Debug Functions ============= */
#if defined(WSR_DEBUG)
static void
wsr_dbg_main(wsr_info_t *wsr_info)
{
	int read_idx;
	wsr_dbg_elem_t report_elem;

	read_idx = wsr_get_conn_idx(wsr_info, WSR_REPORT_MAX_WPA_CNT);
	WL_ERR(("WSR_DBG\n"));
	WL_ERR(("WSR_DBG\n"));
	WL_ERR(("WSR_DBG\n"));
	WL_ERR(("WSR_DBG : counts: %d %d\n",
		wsr_get_count(read_idx, wsr_info->write_idx), read_idx));
	WL_ERR(("WSR_DBG : r:%d w:%d c:%d\n",
		wsr_info->read_idx, wsr_info->write_idx, wsr_info->conn_idx));
	while (read_idx != WSR_INVALID) {
		wsr_elem_t *elem = &wsr_info->elem[read_idx];
		wsr_dbg_copy_elem(&report_elem, elem);
		wsr_dbg_print_elem(&report_elem);
		read_idx = wsr_get_next_ptr(wsr_info, read_idx);
	}

}

static void
wsr_dbg_copy_elem(wsr_dbg_elem_t *dest, wsr_elem_t *src)
{
	wsr_dbg_copy_conn(dest, src);
	wsr_dbg_copy_cnts(dest, src);
	wsr_dbg_copy_clock(dest, src);
	wsr_dbg_copy_link(dest, src);
	wsr_dbg_copy_adps(dest, src);
}

static void
wsr_dbg_copy_cnts(wsr_dbg_elem_t *dest, wsr_elem_t *src)
{
	wl_cnt_info_t *cntinfo = (wl_cnt_info_t *)src->cntinfo;
	wl_cnt_wlc_t *wlc_cnt;
	wl_cnt_ge40mcst_v1_t *macstat_cnt;

	/* COPY WLC counts */
	wlc_cnt = bcm_get_data_from_xtlv_buf(cntinfo->data, cntinfo->datalen,
		WL_CNT_XTLV_WLC, NULL, BCM_XTLV_OPTION_ALIGN32);
	if (wlc_cnt == NULL) {
		WL_ERR(("WSR_DBG : NO CNT\n"));
		return;
	}
	dest->txframe = wlc_cnt->txframe;

	/* COPY MAC STAT counts */
	macstat_cnt = bcm_get_data_from_xtlv_buf(cntinfo->data, cntinfo->datalen,
		WL_CNT_XTLV_GE40_UCODE_V1, NULL, BCM_XTLV_OPTION_ALIGN32);
	if (macstat_cnt == NULL) {
		WL_ERR(("WSR_DBG : NO MAC STAT\n"));
		return;
	}
	dest->rxmgocast = macstat_cnt->rxmgocast;
}

static inline void
wsr_dbg_copy_conn(wsr_dbg_elem_t *dest, wsr_elem_t *src)
{
	memcpy(&dest->conn, &src->conn, sizeof(src->conn));
}
static inline void
wsr_dbg_copy_clock(wsr_dbg_elem_t *dest, wsr_elem_t *src)
{
	memcpy(&dest->clock, &src->clock, sizeof(src->clock));
}

static inline void
wsr_dbg_copy_link(wsr_dbg_elem_t *dest, wsr_elem_t *src)
{
	memcpy(&dest->link_quality, &src->link_quality, sizeof(src->link_quality));
}

static inline void
wsr_dbg_copy_adps(wsr_dbg_elem_t *dest, wsr_elem_t *src)
{
	memcpy(&dest->adps, &src->adps, sizeof(src->adps));
}

static void
wsr_dbg_print_elem(wsr_dbg_elem_t *elem)
{
	wsr_link_t *link = &elem->link_quality;
	wsr_adps_t *adps = &elem->adps;
	wsr_clock_t *clock = &elem->clock;
	wsr_conn_t *conn = &elem->conn;
	int idx;

	WL_ERR(("WSR_DBG: DATE [%u] %d-%02d-%02d %02d:%02d:%02d\n", clock->kernel_clock,
		clock->date/WSR_REPORT_YEAR_MUL,
		(clock->date%WSR_REPORT_YEAR_MUL)/WSR_REPORT_MON_MUL,
		(clock->date%WSR_REPORT_YEAR_MUL)%WSR_REPORT_MON_MUL,
		clock->time/WSR_REPORT_HOUR_MUL,
		(clock->time%WSR_REPORT_HOUR_MUL)/WSR_REPORT_MIN_MUL,
		(clock->time%WSR_REPORT_HOUR_MUL)%WSR_REPORT_MIN_MUL));
	WL_ERR(("WSR_DBG: CONN bssid: "MACDBG"\n",
		MAC2STRDBG(conn->bssid)));
	WL_ERR(("WSR_DBG: CONN  op_mode:%x vndr oui: %d", conn->dhd_opmode, conn->oui_cnt));
	for (idx = 0; idx < conn->oui_cnt; idx++) {
		WL_ERR((" "MACOUIDBG,
			MACOUI2STRDBG(conn->oui_list[idx])));
	}
	WL_ERR(("WSR_DBG\n"));
	WL_ERR(("WSR_DBG: CNT txframe:%d rxmgocast:%d\n", elem->txframe, elem->rxmgocast));
	WL_ERR(("WSR_DBG: LINK LQCM %d SNR %d RSSI %d\n", link->lqcm, link->snr, link->rssi));
	WL_ERR(("WSR_DBG: ADPS M:%d F:%d S:%d H:(%d %d) L:(%d %d)\n", adps->mode, adps->flags,
		adps->current_step, adps->duration_high, adps->counts_high,
		adps->duration_low, adps->counts_low));
}
#endif /* WSR_DEBUG */

/* ========= File Save ============= */
#define WSR_TYPE_MAGIC	"WIFI_STAT_REPORT"
void wl_stat_report_file_save(void *dhdp, void *fp)
{
	struct file *pfp = (struct file *)fp;
	dhd_pub_t *ppub = (dhd_pub_t *)dhdp;
	wsr_info_t *wsr_info;
	int first_idx, last_idx, count, wr_count;
	int wr_size;
	int err;

	if (!pfp || !ppub) {
		WL_ERR(("pfp and/or dhdp is NULL\n"));
		return;
	}

	wsr_info =  (wsr_info_t *)ppub->stat_report_info;
	if (!wsr_info) {
		WL_ERR(("stat control block is not exist\n"));
		return;
	}

	err = generic_file_llseek(pfp, 0, SEEK_END);
	if (err < 0) {
		WL_ERR(("WRITE ERROR!!!! err = %d\n", err));
		return;
	}

	/* WRITE MAGICS TYPE + TYPE + VER */
	vfs_write(pfp, WSR_TYPE_MAGIC, strlen(WSR_TYPE_MAGIC), &pfp->f_pos);
	vfs_write(pfp, WSR_TYPE_MAGIC, strlen(WSR_TYPE_MAGIC), &pfp->f_pos);
	vfs_write(pfp, (const char *)&wsr_info->version, sizeof(wsr_info->version), &pfp->f_pos);

	/* WRITE total data size */
	first_idx = wsr_get_first_idx(wsr_info);
	last_idx = wsr_get_last_idx(wsr_info);
	count = wsr_get_count(first_idx, last_idx);
	wr_size = count * sizeof(wsr_elem_t) + strlen(WSR_TYPE_MAGIC);
	vfs_write(pfp, (const char *)&wr_size, sizeof(wr_size), &pfp->f_pos);
	if (count <= 0) {
		goto final_magic;
	}

	/* WRITE TO THE END OF ARRAY */
	wr_count = (WSR_REPORT_MAX_DATA - first_idx);
	vfs_write(pfp, (const char *)&wsr_info->elem[first_idx],
		sizeof(wsr_elem_t) * wr_count, &pfp->f_pos);

	/* WRITE WRAP UP */
	count = count - wr_count;
	if (count <= 0) {
		goto final_magic;
	}
	vfs_write(pfp, (const char *)&wsr_info->elem[0], sizeof(wsr_elem_t) * count, &pfp->f_pos);

final_magic:
	/* WRITE MAIC */
	vfs_write(pfp, WSR_TYPE_MAGIC, strlen(WSR_TYPE_MAGIC), &pfp->f_pos);

}

/* ========= Private Command ============= */
#define WSR_REPORT_ELEM_PRINT_BUF	256
#define WSR_REPORT_STATE_NORMAL 0
#define WSR_REPORT_STATE_WLC	1
#define WSR_REPORT_STATE_LAST	0xffff
#define WSR_REPORT_NAME_MAX	20

#define WSR_DEC	1
#define WSR_HEX	2

#define WSR_UINT8	1
#define WSR_UINT32	2
#define WSR_DATE	3
#define WSR_TIME	4
#define WSR_BSSID	5
#define WSR_OUI	6
#define WSR_CNT_WLC	7
#define WSR_CNT_MAC	8

#define WSR_SERIAL_MODE_RECORD_FIRST	1
#define WSR_SERIAL_MODE_ITEM_FIRST		2
typedef struct {
	char name[WSR_REPORT_NAME_MAX];
	uint32 offset;
	int format;
	int type;
	int f_signed;
} wsr_serial_info_normal_t;

typedef int (*wsr_serial_sub_func)
	(wsr_serial_info_normal_t *info, char *buf, int buf_len, wsr_elem_t *elem);

#define WSR_SERIAL_CLOCK(name, value, fmt, type, f_s) \
{#name, (uint32)(OFFSETOF(wsr_elem_t, clock) + \
		OFFSETOF(wsr_clock_t, value)), fmt, type, f_s}

#define WSR_SERIAL_CONN(name, value, fmt, type, f_s) \
{#name, (uint32)(OFFSETOF(wsr_elem_t, conn) + \
		OFFSETOF(wsr_conn_t, value)), fmt, type, f_s}

#define WSR_SERIAL_LINK(name, value, fmt, type, f_s) \
{#name, (uint32)(OFFSETOF(wsr_elem_t, link_quality) + \
		OFFSETOF(wsr_link_t, value)), fmt, type, f_s}

#define WSR_SERIAL_ADPS(name, value, fmt, type, f_s) \
{#name, (uint32)(OFFSETOF(wsr_elem_t, adps) + \
		OFFSETOF(wsr_adps_t, value)), fmt, type, f_s}

#define WSR_SERIAL_TXP(name, value, fmt, type, f_s) \
{#name, (uint32)(OFFSETOF(wsr_elem_t, tx_power) + \
		OFFSETOF(wsr_tx_power_t, value)), fmt, type, f_s}

static wsr_serial_info_normal_t
wsr_serial_info_normal_tbl[] = {
	/* clock */
	WSR_SERIAL_CLOCK(CLOCK, kernel_clock, WSR_DEC, WSR_UINT32, FALSE),
	WSR_SERIAL_CLOCK(DATE, date, WSR_DEC, WSR_DATE, FALSE),
	WSR_SERIAL_CLOCK(TIME, time, WSR_DEC, WSR_TIME, FALSE),

	/* connection */
	WSR_SERIAL_CONN(ADDR, bssid, WSR_HEX, WSR_BSSID, FALSE),
	WSR_SERIAL_CONN(OPMODE, dhd_opmode, WSR_HEX, WSR_UINT32, FALSE),
	WSR_SERIAL_CONN(OUI, oui_list, WSR_HEX, WSR_OUI, FALSE),

	/* link quality */
	WSR_SERIAL_LINK(LQCM, lqcm, WSR_DEC, WSR_UINT8, TRUE),
	WSR_SERIAL_LINK(RSSI, rssi, WSR_DEC, WSR_UINT8, TRUE),
	WSR_SERIAL_LINK(SNR, snr, WSR_DEC, WSR_UINT8, FALSE),

	/* adps */
	WSR_SERIAL_ADPS(ADPS_MODE, mode, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_ADPS(ADPS_LFAG, flags, WSR_HEX, WSR_UINT8, FALSE),
	WSR_SERIAL_ADPS(ADPS_STEP, current_step, WSR_DEC, WSR_UINT8, FALSE),
#ifndef WSR_MINIMIZE_REPORT
	WSR_SERIAL_ADPS(ADPS_DH, duration_high, WSR_DEC, WSR_UINT32, FALSE),
	WSR_SERIAL_ADPS(ADPS_DL, duration_low, WSR_DEC, WSR_UINT32, FALSE),
	WSR_SERIAL_ADPS(ADPS_CH, counts_high, WSR_DEC, WSR_UINT32, FALSE),
	WSR_SERIAL_ADPS(ADPS_CL, counts_low, WSR_DEC, WSR_UINT32, FALSE),
#endif /* WSR_MINIMIZE_REPORT */

	/* TX PWR */
	WSR_SERIAL_TXP(SAR_ENAB, sar_enable, WSR_DEC, WSR_UINT8, FALSE),
#ifndef WSR_MINIMIZE_REPORT
	WSR_SERIAL_TXP(RSDB_MOD, phy_rsdb_mode, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(SAR_2G_C0, sar_limit_2g_core0, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(SAR_2G_C1, sar_limit_2g_core1, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(SAR_5G_C0, sar_limit_5g_core0, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(SAR_5G_C1, sar_limit_5g_core1, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(FCC, fcc_enable, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(FCC_CH12, fccpwrch12, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(FCC_CH13, fccpwrch13, WSR_DEC, WSR_UINT8, FALSE),
#endif /* WSR_MINIMIZE_REPORT */
	WSR_SERIAL_TXP(BSS_L_MAX, bss_local_max, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(BSS_L_CST, bss_local_constraint, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(DSSS_1M, dsss_1mbps_2g, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(OFDM_6M_2G, ofdm_6mbps_2g, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(MCS0_2G, mcs_0_2g, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(OFDM_6M_5G, ofdm_6mbps_5g, WSR_DEC, WSR_UINT8, FALSE),
	WSR_SERIAL_TXP(MCS0_5G, mcs_0_5g, WSR_DEC, WSR_UINT8, FALSE),
	{"", 0, 0, 0, 0}
};

/* Serialize 1 byte variable with requested format/signed */
static int
wsr_serial_uint8(wsr_serial_info_normal_t *info, char *buf, int buf_len, wsr_elem_t *elem)
{
	int bytes_written = 0;
	uint8 *ptr;
	int8 *ptr2;
	char *fmt;
	char *null_fmt = " -255";

	if (buf_len <= strlen(null_fmt)) {
		return -1;
	}

	if (elem == NULL) {
		bytes_written = scnprintf(buf, buf_len, " 0");
		return bytes_written;
	}

	ptr = ptr2 = (uint8 *)((char *)elem + info->offset);
	switch (info->format) {
		case WSR_DEC:
			if (info->f_signed) {
				fmt = " %d";
			} else {
				fmt = " %u";
			}
			break;
		case WSR_HEX:
			fmt = " 0x%02x";
			break;
		default:
			WL_ERR(("UKNOWN FMT\n"));
			return -1;
	}

	bytes_written = scnprintf(buf, buf_len, fmt, info->f_signed?(*ptr2) :(*ptr));
	return bytes_written;
}

/* Serialize 4 byte variable with requested format/signed */
static int
wsr_serial_uint32(wsr_serial_info_normal_t *info, char *buf, int buf_len, wsr_elem_t *elem)
{
	int bytes_written = 0;
	uint32 *ptr;
	int32 *ptr2;
	char *fmt;
	char *null_fmt = " -4294967295";

	if (buf_len <= strlen(null_fmt)) {
		return -1;
	}

	if (elem == NULL) {
		bytes_written = scnprintf(buf, buf_len, " 0");
		return bytes_written;
	}

	ptr = ptr2 = (uint32 *)((char *)elem + info->offset);
	switch (info->format) {
		case WSR_DEC:
			if (info->f_signed) {
				fmt = " %d";
			} else {
				fmt = " %u";
			}
			break;
		case WSR_HEX:
			fmt = " 0x%08x";
			break;
		default:
			WL_ERR(("UKNOWN FMT\n"));
			return -1;
	}

	bytes_written = scnprintf(buf, buf_len, fmt, info->f_signed?(*ptr2) :(*ptr));
	return bytes_written;
}

/* serialize WSR date variable to date format */
static int
wsr_serial_date(wsr_serial_info_normal_t *info, char *buf, int buf_len, wsr_elem_t *elem)
{
	int bytes_written = 0;
	uint32 *ptr;
	char *null_fmt = " 0-0-0";
	char *norm_fmt = " 0000-00-00";

	if (buf_len <= strlen(norm_fmt)) {
		return -1;
	}

	if (elem == NULL) {
		bytes_written = scnprintf(buf, buf_len, null_fmt);
		return bytes_written;
	}

	ptr = (uint32 *)((char *)elem + info->offset);
	bytes_written = scnprintf(buf, buf_len, " %d-%02d-%02d",
			(*ptr)/WSR_REPORT_YEAR_MUL,
			((*ptr)%WSR_REPORT_YEAR_MUL)/WSR_REPORT_MON_MUL,
			((*ptr)%WSR_REPORT_YEAR_MUL)%WSR_REPORT_MON_MUL);

	return bytes_written;
}

/* serialize WSR time variable to time format */
static int
wsr_serial_time(wsr_serial_info_normal_t *info, char *buf, int buf_len, wsr_elem_t *elem)
{
	int bytes_written = 0;
	uint32 *ptr;
	char *null_fmt = " 00:00:00";

	if (buf_len <= strlen(null_fmt)) {
		return -1;
	}

	if (elem == NULL) {
		bytes_written = scnprintf(buf, buf_len, null_fmt);
		return bytes_written;
	}

	ptr = (uint32 *)((char *)elem + info->offset);
	bytes_written = scnprintf(buf, buf_len, " %02d:%02d:%02d",
			(*ptr)/WSR_REPORT_HOUR_MUL,
			((*ptr)%WSR_REPORT_HOUR_MUL)/WSR_REPORT_MIN_MUL,
			((*ptr)%WSR_REPORT_HOUR_MUL)%WSR_REPORT_MIN_MUL);

	return bytes_written;
}

/* serialize BSSID type */
static int
wsr_serial_bssid(wsr_serial_info_normal_t *info, char *buf, int buf_len, wsr_elem_t *elem)
{
	int bytes_written = 0;
	int idx;
	uint8 *ptr;
	char *null_fmt = " 00:00:00:00:00:00";


	if (buf_len <= strlen(null_fmt)) {
		return -1;
	}

	if (elem == NULL) {
		bytes_written = scnprintf(buf, buf_len, null_fmt);
		return bytes_written;
	}

	ptr = (uint8 *)((char *)elem + info->offset);

	for (idx = 0; idx < ETHER_ADDR_LEN; idx++) {
		bytes_written += scnprintf(&buf[bytes_written],
			buf_len - bytes_written, ":%02x", *ptr);
		ptr++;
	}

	/* start with white space */
	buf[0] = ' ';

	return bytes_written;
}

/* Serialize OUI type */
static int
wsr_serial_oui(wsr_serial_info_normal_t *info, char *buf, int buf_len, wsr_elem_t *elem)
{
	int bytes_written = 0;
	int idx;
	uint8 *ptr;
	char *null_fmt = " 00-00-00";


	if (buf_len <= strlen(null_fmt) * WSR_MAX_OUI) {
		return -1;
	}

	if (elem == NULL) {
		for (idx = 0; idx < WSR_MAX_OUI; idx++) {
			bytes_written += scnprintf(&buf[bytes_written],
				buf_len - bytes_written, null_fmt);
		}
		goto serial_oui_finish;
	}

	ptr = (uint8 *)((char *)elem + info->offset);

	for (idx = 0; idx < DOT11_OUI_LEN * WSR_MAX_OUI; idx++) {
		bytes_written += scnprintf(&buf[bytes_written],
			buf_len - bytes_written, "-%02x", *ptr);
		ptr++;
	}

serial_oui_finish:
	/* start with white space */
	for (idx = 0; idx < WSR_MAX_OUI; idx++) {
		ptr = &buf[idx * strlen(null_fmt)];
		*ptr = ' ';
	}

	return bytes_written;
}

static int
wsr_serial_normal(char *buf, int *sub_idx, wsr_elem_t **list, int list_cnt)
{
	int bytes_written = 0;
	int cur_written = 0;
	wsr_serial_info_normal_t *info;
	char tmp_buf[WSR_REPORT_ELEM_PRINT_BUF] = {0, };
	wsr_serial_sub_func func;
	int idx;

	if (strlen(wsr_serial_info_normal_tbl[*sub_idx].name) == 0) {
		/* Serialize normal is finished */
		return 0;
	}

	info = &wsr_serial_info_normal_tbl[*sub_idx];
	/* set sub function */
	switch (info->type) {
		case WSR_UINT8:
			func = wsr_serial_uint8;
			break;
		case WSR_UINT32:
			func = wsr_serial_uint32;
			break;
		case WSR_BSSID:
			func = wsr_serial_bssid;
			break;
		case WSR_OUI:
			func = wsr_serial_oui;
			break;
		case WSR_DATE:
			func = wsr_serial_date;
			break;
		case WSR_TIME:
			func = wsr_serial_time;
			break;
		default:
			WL_ERR(("UNKNOWN SERIALIZE TYPE\n"));
			return -1;
	}

	for (idx = 0; idx < list_cnt; idx++) {
		cur_written = func(info, &tmp_buf[bytes_written],
			sizeof(tmp_buf) - bytes_written, list[idx]);
		if (cur_written <= 0) {
			/* need bigger buffer */
			return -1;
		}

		bytes_written += cur_written;
	}

	memcpy(buf, tmp_buf, bytes_written);
	(*sub_idx)++;
	return bytes_written;
}

#define WSR_SERIAL_WLC(a) {#a, OFFSETOF(wl_cnt_wlc_t, a), WSR_CNT_WLC}
#define WSR_SERIAL_MAC(a) {#a, OFFSETOF(wl_cnt_ge40mcst_v1_t, a), WSR_CNT_MAC}
typedef struct {
	char name[WSR_REPORT_NAME_MAX];
	uint32 offset;
	int type;
} wsr_serial_info_count_t;

static wsr_serial_info_count_t
wsr_serial_info_wlc_tbl[] = {
	/* counts of wlc */
#ifndef WSR_MINIMIZE_REPORT
	WSR_SERIAL_WLC(txframe),
#endif /* WSR_MINIMIZE_REPORT */
	WSR_SERIAL_WLC(txretrans),
	WSR_SERIAL_WLC(txfail),
#ifndef WSR_MINIMIZE_REPORT
	WSR_SERIAL_WLC(rxframe),
	WSR_SERIAL_WLC(txphyerr),
	WSR_SERIAL_WLC(rxnobuf),
	WSR_SERIAL_WLC(rxrtry),
#endif /* WSR_MINIMIZE_REPORT */
	WSR_SERIAL_WLC(txprobereq),
	WSR_SERIAL_WLC(rxprobersp),
	WSR_SERIAL_MAC(txallfrm),
	WSR_SERIAL_MAC(txrtsfrm),
	WSR_SERIAL_MAC(txctsfrm),
	WSR_SERIAL_WLC(txback),
	WSR_SERIAL_MAC(txucast),
	WSR_SERIAL_MAC(rxrsptmout),
	WSR_SERIAL_MAC(txrtsfail),
#ifndef WSR_MINIMIZE_REPORT
	WSR_SERIAL_MAC(txphyerror),
#endif /* WSR_MINIMIZE_REPORT */
	WSR_SERIAL_MAC(rxstrt),
	WSR_SERIAL_MAC(rxbadplcp),
	WSR_SERIAL_MAC(rxcrsglitch),
	WSR_SERIAL_MAC(rxnodelim),
	WSR_SERIAL_MAC(bphy_badplcp),
	WSR_SERIAL_MAC(bphy_rxcrsglitch),
	WSR_SERIAL_MAC(rxbadfcs),
	WSR_SERIAL_MAC(rxf0ovfl),
	WSR_SERIAL_MAC(rxf1ovfl),
	WSR_SERIAL_MAC(rxhlovfl),
	WSR_SERIAL_MAC(rxrtsucast),
	WSR_SERIAL_MAC(rxctsucast),
	WSR_SERIAL_MAC(rxackucast),
	WSR_SERIAL_WLC(rxback),
	WSR_SERIAL_MAC(rxbeaconmbss),
	WSR_SERIAL_MAC(rxdtucastmbss),
	WSR_SERIAL_MAC(rxbeaconobss),
	WSR_SERIAL_MAC(rxdtucastobss),
	WSR_SERIAL_MAC(rxdtocast),
	WSR_SERIAL_MAC(rxrtsocast),
	WSR_SERIAL_MAC(rxctsocast),
	WSR_SERIAL_MAC(rxdtmcast),
#ifndef WSR_MINIMIZE_REPORT
	WSR_SERIAL_WLC(rxmpdu_mu),
	WSR_SERIAL_WLC(txassocreq),
	WSR_SERIAL_WLC(txreassocreq),
	WSR_SERIAL_WLC(txdisassoc),
	WSR_SERIAL_WLC(txassocrsp),
	WSR_SERIAL_WLC(txreassocrsp),
	WSR_SERIAL_WLC(txauth),
	WSR_SERIAL_WLC(txdeauth),
	WSR_SERIAL_WLC(rxassocreq),
	WSR_SERIAL_WLC(rxreassocreq),
	WSR_SERIAL_WLC(rxdisassoc),
	WSR_SERIAL_WLC(rxassocrsp),
	WSR_SERIAL_WLC(rxreassocrsp),
	WSR_SERIAL_WLC(rxauth),
	WSR_SERIAL_WLC(rxdeauth),
	WSR_SERIAL_MAC(txackfrm),
	WSR_SERIAL_MAC(txampdu),
	WSR_SERIAL_MAC(txmpdu),
#endif /* WSR_MINIMIZE_REPORT */
	{"", 0, 0}
};

static int
wsr_serial_wlc(char *buf, int *sub_idx, wsr_elem_t **list, int list_cnt)
{
	int bytes_written = 0;
	wsr_serial_info_count_t *info;
	char tmp_buf[WSR_REPORT_ELEM_PRINT_BUF] = {0, };
	int idx;
	uint32 *ptr;
	wl_cnt_info_t *cntinfo;
	char *cntptr;

	if (strlen(wsr_serial_info_wlc_tbl[*sub_idx].name) == 0) {
		/* Serialize normal is finished */
		return 0;
	}

	info = &wsr_serial_info_wlc_tbl[*sub_idx];

	for (idx = 0; idx < list_cnt; idx++) {
		if (bytes_written >= WSR_REPORT_ELEM_PRINT_BUF) {
			return -1;
		}
		if (list[idx] == NULL) {
			bytes_written += scnprintf(&tmp_buf[bytes_written],
				WSR_REPORT_ELEM_PRINT_BUF - bytes_written, " 0");
			continue;
		}

		/* find base PTR for WLC or MACSTAT */
		cntinfo = (wl_cnt_info_t *)list[idx]->cntinfo;
		if (info->type == WSR_CNT_WLC) {
			cntptr = (char *)bcm_get_data_from_xtlv_buf(cntinfo->data, cntinfo->datalen,
				WL_CNT_XTLV_WLC, NULL, BCM_XTLV_OPTION_ALIGN32);
		} else if (info->type == WSR_CNT_MAC) {
			cntptr =  (char *)bcm_get_data_from_xtlv_buf(cntinfo->data,
				cntinfo->datalen,
				WL_CNT_XTLV_GE40_UCODE_V1, NULL, BCM_XTLV_OPTION_ALIGN32);
		} else {
			WL_ERR(("INVALID TYPE\n"));
			return -1;
		}

		if (cntptr == NULL) {
			bytes_written += scnprintf(&tmp_buf[bytes_written],
				WSR_REPORT_ELEM_PRINT_BUF - bytes_written, " 0");
		} else {
			ptr = (uint32 *)(cntptr + info->offset);
			bytes_written += scnprintf(&tmp_buf[bytes_written],
				WSR_REPORT_ELEM_PRINT_BUF - bytes_written, " %u",
				(*ptr) % WSR_CNT_VALUE_MAX);
		}
	}

	memcpy(buf, tmp_buf, bytes_written);
	(*sub_idx)++;
	return bytes_written;
}

int
wl_android_stat_report_get_start(void *net, char *cmd, int tot_len)
{
	struct net_device *dev = (struct net_device *)net;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	wsr_info_t *wsr_info = cfg->stat_report_info;
	char *pcmd = cmd;
	char *str_cmd;
	char *str;
	int mode, max_len;
	int lock_count;
	int bytes_written = 0;

	/* SKIP command */
	str_cmd = bcmstrtok(&pcmd, " ", NULL);

	/* get MODE */
	str = bcmstrtok(&pcmd, " ", NULL);
	if (!str) {
		WL_ERR(("NO mode for %s\n", str_cmd));
		return BCME_ERROR;
	}
	mode = bcm_atoi(str);
	if (mode != WSR_SERIAL_MODE_RECORD_FIRST &&
		mode != WSR_SERIAL_MODE_ITEM_FIRST) {
		WL_ERR(("NOT SUPPORTED MODE \n"));
		return BCME_ERROR;
	}


	/* get max length */
	str = bcmstrtok(&pcmd, " ", NULL);
	if (!str) {
		WL_ERR(("NO length for %s\n", str_cmd));
		return BCME_ERROR;
	}
	max_len = bcm_atoi(str);
	if (max_len <= WSR_REPORT_ELEM_PRINT_BUF) {
		WL_ERR(("TO short buffer length (min=%d)\n", WSR_REPORT_ELEM_PRINT_BUF));
		return BCME_ERROR;
	}

	/* Set seiralize info and lock record till serialzation is finished */
	wsr_info->seral_mode = mode;
	wsr_info->trans_buf_len = max_len;
	wsr_set_lock_idx(wsr_info, WSR_REPORT_MAX_WPA_CNT);
	wsr_info->seral_state = WSR_REPORT_STATE_NORMAL;
	wsr_info->serial_sub_state = 0;

	if (mode == WSR_SERIAL_MODE_RECORD_FIRST) {
		lock_count = wsr_get_lock_cnt(wsr_info);
		WL_ERR(("Lock WSR_SERIAL_MODE_RECORD_FIRST is set count= %d\n",
			lock_count));
		bytes_written = scnprintf(cmd, tot_len, "%d", lock_count);
		return bytes_written;
	}

	WL_ERR(("Lock WSR_SERIAL_MODE_ITEM_FIRST is set\n"));
	return BCME_OK;
}

int
wl_android_stat_report_get_next_mode_record(void *net, char *cmd, int tot_len)
{
	struct net_device *dev = (struct net_device *)net;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	wsr_info_t *wsr_info = cfg->stat_report_info;
	wsr_elem_t *elem;
	int bytes_written = 0;
	int iter_written;
	int lock_idx;

	lock_idx = wsr_get_lock_idx(wsr_info);
	if (lock_idx == WSR_INVALID) {
		WL_ERR(("Lock pointer is not set\n"));
		return BCME_ERROR;
	}

	elem = &wsr_info->elem[lock_idx];

	memset(cmd, 0, wsr_info->trans_buf_len);
	wsr_info->seral_state = WSR_REPORT_STATE_NORMAL;
	wsr_info->serial_sub_state = 0;

	while (bytes_written + WSR_REPORT_ELEM_PRINT_BUF < wsr_info->trans_buf_len) {
		switch (wsr_info->seral_state) {
			case WSR_REPORT_STATE_NORMAL:
				iter_written = wsr_serial_normal(&cmd[bytes_written],
					&wsr_info->serial_sub_state, &elem, 1);
				break;
			case WSR_REPORT_STATE_WLC:
				iter_written = wsr_serial_wlc(&cmd[bytes_written],
					&wsr_info->serial_sub_state, &elem, 1);
				break;
			default:
				goto finished;
		}

		if (iter_written == 0) {
			wsr_info->seral_state++;
			wsr_info->serial_sub_state = 0;
			continue;
		}
		if (iter_written < 0) {
			WL_ERR(("At list one element is string lenght is too big(%d %d)\n",
				wsr_info->seral_state, wsr_info->serial_sub_state));
			wsr_free_lock_idx(wsr_info);
			return -1;
		}
		bytes_written += iter_written;
	}

	cmd[bytes_written] = '\0';
	WL_ERR(("(len=%d) %s\n", bytes_written, cmd));
	return bytes_written;

finished:
	cmd[bytes_written] = '\0';
	wsr_decr_lock_count(wsr_info);

	WL_ERR(("(len=%d) %s\n", bytes_written, cmd));
	return bytes_written;
}

int
wl_android_stat_report_get_next_mode_item(void *net, char *cmd, int tot_len)
{
	struct net_device *dev = (struct net_device *)net;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	wsr_info_t *wsr_info = cfg->stat_report_info;
	wsr_elem_t *elem[WSR_REPORT_MAX_WPA_CNT] = {NULL, };
	int idx;
	int next_idx;
	int bytes_written = 0;
	int iter_written;
	int lock_idx, lock_count;

	lock_idx = wsr_get_lock_idx(wsr_info);
	lock_count = wsr_get_lock_cnt(wsr_info);

	if (lock_idx == WSR_INVALID) {
		WL_ERR(("Lock pointer is not set\n"));
		return BCME_ERROR;
	}

/* Init seiralze records of WSR_REPORT_MAX_WPA_CNT
 * if locked record is less than WSR_REPORT_MAX_WPA_CNT
 * fill NULL from first to WSR_REPORT_MAX_WPA_CNT - count
 */
	next_idx = lock_idx;
	for (idx = WSR_REPORT_MAX_WPA_CNT - lock_count;
		idx < WSR_REPORT_MAX_WPA_CNT; idx++) {
		elem[idx] = &wsr_info->elem[next_idx];
		next_idx = wsr_get_next_idx(wsr_info, next_idx);
	}

	memset(cmd, 0, wsr_info->trans_buf_len);
	bytes_written = scnprintf(cmd, tot_len, "MORE");

	while (bytes_written + WSR_REPORT_ELEM_PRINT_BUF < wsr_info->trans_buf_len) {
		switch (wsr_info->seral_state) {
			case WSR_REPORT_STATE_NORMAL:
				iter_written = wsr_serial_normal(&cmd[bytes_written],
					&wsr_info->serial_sub_state, elem, WSR_REPORT_MAX_WPA_CNT);
				break;
			case WSR_REPORT_STATE_WLC:
				iter_written = wsr_serial_wlc(&cmd[bytes_written],
					&wsr_info->serial_sub_state, elem, WSR_REPORT_MAX_WPA_CNT);
				break;
			default:
				goto finished;
		}

		if (iter_written == 0) {
			wsr_info->seral_state++;
			wsr_info->serial_sub_state = 0;
			continue;
		}
		if (iter_written < 0) {
			WL_ERR(("At list one element is string lenght is too big(%d %d)\n",
				wsr_info->seral_state, wsr_info->serial_sub_state));
			goto finished;
		}
		bytes_written += iter_written;
	}

	cmd[bytes_written] = '\0';
	WL_ERR(("(len=%d) %s\n", bytes_written, cmd));
	return bytes_written;

finished:
	cmd[bytes_written] = '\0';
	memcpy(cmd, "LAST", strlen("LAST"));
	wsr_free_lock_idx(wsr_info);
	WL_ERR(("(len=%d) %s\n", bytes_written, cmd));
	return bytes_written;
}

int
wl_android_stat_report_get_next(void *net, char *cmd, int tot_len)
{
	struct net_device *dev = (struct net_device *)net;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	wsr_info_t *wsr_info = cfg->stat_report_info;

	if (wsr_info == NULL) {
		WL_ERR(("REPORT STAT is NOT enabled\n"));
		return BCME_ERROR;
	}

	if (tot_len < wsr_info->trans_buf_len) {
		WL_ERR(("To short buffer length (config:%d buf len:%d\n",
			wsr_info->trans_buf_len, tot_len));
		wsr_free_lock_idx(wsr_info);
		return BCME_ERROR;
	}

	if (wsr_info->seral_mode == WSR_SERIAL_MODE_RECORD_FIRST) {
		return wl_android_stat_report_get_next_mode_record(net, cmd, tot_len);
	} else {
		return wl_android_stat_report_get_next_mode_item(net, cmd, tot_len);
	}

	return BCME_ERROR;
}
