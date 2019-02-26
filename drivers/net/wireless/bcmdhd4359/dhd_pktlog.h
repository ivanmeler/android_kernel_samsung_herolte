/*
 * DHD debugability packet logging header file
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
 * $Id: dhd_pktlog.h 742382 2018-01-22 01:56:53Z $
 */

#ifndef __DHD_PKTLOG_H_
#define __DHD_PKTLOG_H_

#include <dhd_debug.h>
#include <dhd.h>

#ifdef DHD_PKT_LOGGING
#define DHD_PKT_LOG(args)	DHD_INFO(args)
#define MIN_PKTLOG_LEN			(32 * 10)
#define MAX_PKTLOG_LEN			(32 * 100)
#define MAX_DHD_PKTLOG_FILTER_LEN	10
#define MAX_MASK_PATTERN_FILTER_LEN	64
#define PKTLOG_TXPKT_CASE			0x0001
#define PKTLOG_TXSTATUS_CASE		0x0002
#define PKTLOG_RXPKT_CASE			0x0004
#define MAX_FILTER_PATTERN_LEN		256
#define PKTLOG_DUMP_BUF_SIZE		(64 * 1024)

typedef struct dhd_dbg_pktlog_info {
	frame_type payload_type;
	size_t pkt_len;
	uint32 driver_ts_sec;
	uint32 driver_ts_usec;
	uint32 firmware_ts;
	uint32 pkt_hash;
	void *pkt;
} dhd_dbg_pktlog_info_t;

typedef struct dhd_pktlog_ring_info
{
	union {
		wifi_tx_packet_fate tx_fate;
		wifi_rx_packet_fate rx_fate;
	};
	dhd_dbg_pktlog_info_t info;
} dhd_pktlog_ring_info_t;

typedef struct dhd_pktlog_ring
{
	uint16 front;
	uint16 rear;
	uint16 prev_pos;
	uint16 next_pos;
	uint32 start;
	uint32 pktlog_minmize;
	uint32 pktlog_len;
	dhd_pktlog_ring_info_t *pktlog_ring_info;
	dhd_pub_t *dhdp;
} dhd_pktlog_ring_t;

typedef struct dhd_pktlog_filter_info
{
	uint32 id;
	uint32 offset;
	uint32 size_bytes; /* Size of pattern. */
	uint32 enable;
	uint8 mask[MAX_MASK_PATTERN_FILTER_LEN];
	uint8 pattern[MAX_MASK_PATTERN_FILTER_LEN];
} dhd_pktlog_filter_info_t;

typedef struct dhd_pktlog_filter
{
	dhd_pktlog_filter_info_t *info;
	uint32 list_cnt;
	uint32 enable;
} dhd_pktlog_filter_t;

typedef struct dhd_pktlog
{
	struct dhd_pktlog_ring *tx_pktlog_ring;
	struct dhd_pktlog_ring *rx_pktlog_ring;
	struct dhd_pktlog_filter *pktlog_filter;
} dhd_pktlog_t;

typedef struct dhd_pktlog_pcap_hdr
{
	uint32 magic_number;
	uint16 version_major;
	uint16 version_minor;
	uint16 thiszone;
	uint32 sigfigs;
	uint32 snaplen;
	uint32 network;
} dhd_pktlog_pcap_hdr_t;

#define PKTLOG_PCAP_MAGIC_NUM 0xa1b2c3d4
#define PKTLOG_PCAP_MAJOR_VER 0x02
#define PKTLOG_PCAP_MINOR_VER 0x04
#define PKTLOG_PCAP_SNAP_LEN 0x40000
#define PKTLOG_PCAP_NETWORK_TYPE 147

extern int dhd_os_attach_pktlog(dhd_pub_t *dhdp);
extern int dhd_os_detach_pktlog(dhd_pub_t *dhdp);
extern dhd_pktlog_ring_t* dhd_pktlog_ring_init(dhd_pub_t *dhdp, int size);
extern int dhd_pktlog_ring_deinit(dhd_pktlog_ring_t *ring);
extern int dhd_pktlog_ring_set_nextpos(dhd_pktlog_ring_t *ringbuf);
extern int dhd_pktlog_ring_get_nextbuf(dhd_pktlog_ring_t *ringbuf, void **data);
extern int dhd_pktlog_ring_set_prevpos(dhd_pktlog_ring_t *ringbuf);
extern int dhd_pktlog_ring_get_prevbuf(dhd_pktlog_ring_t *ringbuf, void **data);
extern int dhd_pktlog_ring_get_writebuf(dhd_pktlog_ring_t *ringbuf, void **data);
extern int dhd_pktlog_ring_tx_pkts(dhd_pub_t *dhdp, void *pkt, uint32 pktid);
extern int dhd_pktlog_ring_tx_status(dhd_pub_t *dhdp, void *pkt, uint32 pktid,
		uint16 status);
extern int dhd_pktlog_ring_rx_pkts(dhd_pub_t *dhdp, void *pkt);
extern dhd_pktlog_ring_t* dhd_pktlog_ring_change_size(dhd_pktlog_ring_t *ringbuf, int size);

#define DHD_PKTLOG_TX(dhdp, pkt, pktid) \
{ \
	do { \
		if ((dhdp) && (dhdp)->pktlog && (dhdp)->pktlog->tx_pktlog_ring && (pkt)) { \
			if ((dhdp)->pktlog->tx_pktlog_ring->start) { \
				 dhd_pktlog_ring_tx_pkts(dhdp, pkt, pktid); \
			} \
		} \
	} while (0); \
}

#define DHD_PKTLOG_TXS(dhdp, pkt, pktid, status) \
{ \
	do { \
		if ((dhdp) && (dhdp)->pktlog && (dhdp)->pktlog->tx_pktlog_ring && (pkt)) { \
			if ((dhdp)->pktlog->tx_pktlog_ring->start) { \
				 dhd_pktlog_ring_tx_status(dhdp, pkt, pktid, status); \
			} \
		} \
	} while (0); \
}

#define DHD_PKTLOG_RX(dhdp, pkt) \
{ \
	do { \
		if ((dhdp) && (dhdp)->pktlog && (dhdp)->pktlog->rx_pktlog_ring && (pkt)) { \
			if (ntoh16((pkt)->protocol) != ETHER_TYPE_BRCM) { \
				if ((dhdp)->pktlog->rx_pktlog_ring->start) { \
					dhd_pktlog_ring_rx_pkts(dhdp, pkt); \
				} \
			} \
		} \
	} while (0); \
}

extern dhd_pktlog_filter_t* dhd_pktlog_filter_init(int size);
extern int dhd_pktlog_filter_deinit(dhd_pktlog_filter_t *filter);
extern int dhd_pktlog_filter_add(dhd_pktlog_filter_t *filter, char *arg);
extern int dhd_pktlog_filter_enable(dhd_pktlog_filter_t *filter, uint32 pktlog_case, uint32 enable);
extern int dhd_pktlog_filter_pattern_enable(dhd_pktlog_filter_t *filter, char *arg, uint32 enable);
extern int dhd_pktlog_filter_info(dhd_pktlog_filter_t *filter);
extern bool dhd_pktlog_filter_matched(dhd_pktlog_filter_t *filter, char *data, uint32 pktlog_case);
extern bool dhd_pktlog_filter_existed(dhd_pktlog_filter_t *filter, char *arg, uint32 *id);

#define DHD_PKTLOG_FILTER_ADD(pattern, filter_pattern, dhdp)	\
{	\
	do {	\
		if ((strlen(pattern) + 1) < sizeof(filter_pattern)) {	\
			strncpy(filter_pattern, pattern, sizeof(filter_pattern));	\
			dhd_pktlog_filter_add(dhdp->pktlog->pktlog_filter, filter_pattern);	\
		}	\
	} while (0);	\
}

#define DHD_PKTLOG_DUMP_PATH	DHD_COMMON_DUMP_PATH
extern void dhd_pktlog_dump(void *handle, void *event_info, u8 event);
extern void dhd_schedule_pktlog_dump(dhd_pub_t *dhdp);
extern int dhd_pktlog_write_file(dhd_pub_t *dhdp);

#define DHD_PKTLOG_FATE_INFO_STR_LEN 256
#define DHD_PKTLOG_FATE_INFO_FORMAT	"BRCM_Packet_Fate"
#define DHD_PKTLOG_DUMP_TYPE "pktlog_dump"

#endif /* DHD_PKT_LOGGING */
#endif /* __DHD_PKTLOG_H_ */
