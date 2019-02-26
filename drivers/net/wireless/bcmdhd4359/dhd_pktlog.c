/*
 * DHD debugability packet logging support
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
 * $Id: dhd_pktlog.c 742382 2018-01-22 01:56:53Z $
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include <dhd_pktlog.h>

#ifdef DHD_PKT_LOGGING
#ifndef strtoul
#define strtoul(nptr, endptr, base) bcm_strtoul((nptr), (endptr), (base))
#endif
extern int wl_pattern_atoh(char *src, char *dst);
extern uint32 __dhd_dbg_pkt_hash(uintptr_t pkt, uint32 pktid);
extern wifi_tx_packet_fate __dhd_dbg_map_tx_status_to_pkt_fate(uint16 status);

int
dhd_os_attach_pktlog(dhd_pub_t *dhdp)
{
	gfp_t kflags;
	uint32 alloc_len;
	dhd_pktlog_t *pktlog;

	if (!dhdp) {
		DHD_ERROR(("%s(): dhdp is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	alloc_len = sizeof(dhd_pktlog_t);
	pktlog = (dhd_pktlog_t *)kzalloc(alloc_len, kflags);
	if (unlikely(!pktlog)) {
		DHD_ERROR(("%s(): could not allocate memory for - "
					"dhd_pktlog_t\n", __FUNCTION__));
		goto fail;
	}

	dhdp->pktlog = pktlog;

	dhdp->pktlog->tx_pktlog_ring = dhd_pktlog_ring_init(dhdp, MIN_PKTLOG_LEN);
	dhdp->pktlog->rx_pktlog_ring = dhd_pktlog_ring_init(dhdp, MIN_PKTLOG_LEN);
	dhdp->pktlog->pktlog_filter = dhd_pktlog_filter_init(MAX_DHD_PKTLOG_FILTER_LEN);

	DHD_ERROR(("%s(): dhd_os_attach_pktlog attach\n", __FUNCTION__));

	return BCME_OK;
fail:
	if (pktlog) {
		kfree(pktlog);
	}

	return BCME_ERROR;
}

int
dhd_os_detach_pktlog(dhd_pub_t *dhdp)
{
	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	dhd_pktlog_ring_deinit(dhdp->pktlog->tx_pktlog_ring);
	dhd_pktlog_ring_deinit(dhdp->pktlog->rx_pktlog_ring);
	dhd_pktlog_filter_deinit(dhdp->pktlog->pktlog_filter);

	DHD_ERROR(("%s(): dhd_os_attach_pktlog detach\n", __FUNCTION__));

	kfree(dhdp->pktlog);

	return BCME_OK;
}

dhd_pktlog_ring_t*
dhd_pktlog_ring_init(dhd_pub_t *dhdp, int size)
{
	gfp_t kflags;
	uint32 alloc_len;
	dhd_pktlog_ring_t *ring;
	dhd_pktlog_ring_info_t *ring_info = NULL;

	if (!dhdp) {
		DHD_ERROR(("%s(): dhdp is NULL\n", __FUNCTION__));
		return NULL;
	}

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	alloc_len = sizeof(dhd_pktlog_ring_t);
	ring = (dhd_pktlog_ring_t *)kzalloc(alloc_len, kflags);
	if (unlikely(!ring)) {
		DHD_ERROR(("%s(): could not allocate memory for - "
					"dhd_pktlog_ring_t\n", __FUNCTION__));
		goto fail;
	}

	alloc_len = (sizeof(dhd_pktlog_ring_info_t) * size);
	ring_info = (dhd_pktlog_ring_info_t *)kzalloc(alloc_len, kflags);
	if (unlikely(!ring_info)) {
		DHD_ERROR(("%s(): could not allocate memory for - "
					"dhd_pktlog_ring_info_t\n", __FUNCTION__));
		goto fail;
	}

	ring->pktlog_ring_info = ring_info;

	ring->front = 0;
	ring->rear = 0;
	ring->prev_pos = 0;
	ring->next_pos = 0;

	ring->start = TRUE;
	ring->pktlog_minmize = FALSE;
	ring->pktlog_len = size;
	ring->dhdp = dhdp;

	DHD_ERROR(("%s(): pktlog ring init success\n", __FUNCTION__));

	return ring;
fail:
	if (ring_info) {
		kfree(ring_info);
	}

	if (ring) {
		kfree(ring);
	}

	return NULL;
}

int
dhd_pktlog_ring_deinit(dhd_pktlog_ring_t *ring)
{
	int ret = BCME_OK;
	void *data = NULL;
	dhd_pktlog_ring_info_t *ring_info;

	if (!ring) {
		DHD_ERROR(("%s(): ring is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	if (!ring->dhdp) {
		DHD_ERROR(("%s(): dhdp is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	/* stop pkt log */
	ring->start = FALSE;

	/* free ring_info->info.pkt */
	if (dhd_pktlog_ring_set_nextpos(ring) == BCME_OK) {
		while (dhd_pktlog_ring_get_nextbuf(ring, &data) == BCME_OK) {
			ring_info = (dhd_pktlog_ring_info_t *)data;

			if (ring_info && ring_info->info.pkt) {
				PKTFREE(ring->dhdp->osh, ring_info->info.pkt, TRUE);
				DHD_PKT_LOG(("%s(): pkt free pos %d\n",
					__FUNCTION__, ring->next_pos));
				ring_info->info.pkt = NULL;
			}
		}
	}

	if (ring->pktlog_ring_info) {
		kfree(ring->pktlog_ring_info);
	}
	kfree(ring);

	DHD_ERROR(("%s(): pktlog ring deinit\n", __FUNCTION__));

	return ret;
}

int
dhd_pktlog_ring_set_nextpos(dhd_pktlog_ring_t *ringbuf)
{
	if  (!ringbuf) {
		DHD_ERROR(("%s(): ringbuf is  NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* ring buffer is empty */
	if (ringbuf->front ==  ringbuf->rear) {
		DHD_ERROR(("%s(): ringbuf empty\n", __FUNCTION__));
		return BCME_ERROR;
	}

	ringbuf->next_pos = ringbuf->front;

	return BCME_OK;
}

int
dhd_pktlog_ring_get_nextbuf(dhd_pktlog_ring_t *ringbuf, void **data)
{
	dhd_pktlog_ring_info_t *ring_info;

	if  (!ringbuf || !data) {
		DHD_PKT_LOG(("%s(): ringbuf=%p data=%p\n",
			__FUNCTION__, ringbuf, data));
		return BCME_ERROR;
	}

	/* ring buffer is empty */
	if (ringbuf->front ==  ringbuf->rear) {
		DHD_PKT_LOG(("%s(): ringbuf empty\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (ringbuf->next_pos == ringbuf->rear) {
		DHD_PKT_LOG(("%s(): ringbuf next pos %d is rear\n",
			__FUNCTION__, ringbuf->next_pos));
		return BCME_ERROR;
	}

	DHD_PKT_LOG(("%s(): ringbuf next pos %d\n", __FUNCTION__, ringbuf->next_pos));

	*data = ((dhd_pktlog_ring_info_t *)ringbuf->pktlog_ring_info) + ringbuf->next_pos;
	ring_info = (dhd_pktlog_ring_info_t *)*data;

	if (*data == NULL) {
		DHD_PKT_LOG(("%s(): next_pos %d data NULL\n",
			__FUNCTION__, ringbuf->next_pos));
		return BCME_ERROR;
	}

	if (!ring_info->info.pkt) {
		DHD_ERROR(("%s(): next_pos %d info.pkt NULL\n",
			__FUNCTION__, ringbuf->next_pos));
		return BCME_ERROR;
	}

	ringbuf->next_pos = (ringbuf->next_pos + 1) % ringbuf->pktlog_len;

	DHD_PKT_LOG(("%s(): ringbuf next next pos %d\n",
		__FUNCTION__, ringbuf->next_pos));

	return BCME_OK;
}

int
dhd_pktlog_ring_set_prevpos(dhd_pktlog_ring_t *ringbuf)
{
	if  (!ringbuf) {
		DHD_PKT_LOG(("%s(): ringbuf is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* ring buffer is empty */
	if (ringbuf->front ==  ringbuf->rear) {
		DHD_PKT_LOG(("%s(): ringbuf empty\n", __FUNCTION__));
		return BCME_ERROR;
	}

	ringbuf->prev_pos = (ringbuf->rear + ringbuf->pktlog_len - 1) % ringbuf->pktlog_len;

	return BCME_OK;
}

int
dhd_pktlog_ring_get_prevbuf(dhd_pktlog_ring_t *ringbuf, void **data)
{
	if  (!ringbuf || !data) {
		DHD_PKT_LOG(("%s(): ringbuf=%p data=%p\n",
			__FUNCTION__, ringbuf, data));
		return BCME_ERROR;
	}

	/* ring buffer is empty */
	if (ringbuf->front ==  ringbuf->rear) {
		DHD_PKT_LOG(("%s(): ringbuf empty\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (ringbuf->prev_pos == ringbuf->front) {
		DHD_PKT_LOG(("%s(): ringbuf prev pos %d is front\n",
			__FUNCTION__, ringbuf->prev_pos));
		if (ringbuf->front == 0 && ringbuf->rear == 1) {
			*data = ((dhd_pktlog_ring_info_t *)ringbuf->pktlog_ring_info)
				+ ringbuf->front;
			if (*data == NULL) {
				DHD_PKT_LOG(("%s(): front %d data NULL\n",
					__FUNCTION__, ringbuf->prev_pos));
				return BCME_ERROR;
			} else {
				DHD_PKT_LOG(("%s(): ringbuf front data\n", __FUNCTION__));
				return BCME_OK;
			}
		} else {
			return BCME_ERROR;
		}
	}

	DHD_PKT_LOG(("%s(): ringbuf prev pos %d\n", __FUNCTION__, ringbuf->prev_pos));

	*data = ((dhd_pktlog_ring_info_t *)ringbuf->pktlog_ring_info) + ringbuf->prev_pos;

	if (*data == NULL) {
		DHD_PKT_LOG(("%s(): prev_pos %d data NULL\n",
			__FUNCTION__, ringbuf->prev_pos));
		return BCME_ERROR;
	}

	ringbuf->prev_pos  = (ringbuf->prev_pos + ringbuf->pktlog_len - 1) % ringbuf->pktlog_len;

	DHD_PKT_LOG(("%s(): ringbuf next prev pos %d\n", __FUNCTION__, ringbuf->prev_pos));

	return BCME_OK;
}

int
dhd_pktlog_ring_get_writebuf(dhd_pktlog_ring_t *ringbuf, void **data)
{
	dhd_pktlog_ring_info_t *ring_info;
	uint16 write_pos = 0;
	uint16 next_write_pos = 0;

	if  (!ringbuf || !data) {
		DHD_PKT_LOG(("%s(): ringbuf=%p data=%p\n",
			__FUNCTION__, ringbuf, data));
		return BCME_ERROR;
	}

	write_pos = ringbuf->rear;
	next_write_pos = (ringbuf->rear + 1) % ringbuf->pktlog_len;
	*data = ((dhd_pktlog_ring_info_t *)ringbuf->pktlog_ring_info) + write_pos;
	if (*data == NULL) {
		DHD_PKT_LOG(("%s(): write_pos %d data NULL\n", __FUNCTION__, write_pos));
		return BCME_ERROR;
	}

	/* ringbuf is Full */
	if (ringbuf->front == next_write_pos) {
		DHD_PKT_LOG(("%s(): ringbuf full next write %d front %d\n",
			__FUNCTION__, next_write_pos, ringbuf->front));
		/* Free next write buffer */
		ring_info = ((dhd_pktlog_ring_info_t *)ringbuf->pktlog_ring_info) + next_write_pos;
		if (ring_info && ring_info->info.pkt) {
			PKTFREE(ringbuf->dhdp->osh, ring_info->info.pkt, TRUE);
			DHD_PKT_LOG(("%s(): ringbuf full next write %d pkt free\n",
				__FUNCTION__, next_write_pos));
		}
		ringbuf->front = (ringbuf->front + 1) % ringbuf->pktlog_len;
	}

	ringbuf->rear = next_write_pos;

	DHD_PKT_LOG(("%s(): next write pos %d front %d \n",
		__FUNCTION__, next_write_pos, ringbuf->front));

	return BCME_OK;
}

int
dhd_pktlog_ring_tx_pkts(dhd_pub_t *dhdp, void *pkt, uint32 pktid)
{
	dhd_pktlog_ring_info_t *tx_pkts;
	uint32 pkt_hash;
	void *data = NULL;
	uint8 *pktdata = NULL;
	dhd_pktlog_ring_t *tx_pktlog_ring;
	dhd_pktlog_filter_t *pktlog_filter;
	u64 ts_nsec;
	unsigned long rem_nsec;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (!dhdp->pktlog->tx_pktlog_ring) {
		DHD_PKT_LOG(("%s(): tx_pktlog_ring is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	tx_pktlog_ring = dhdp->pktlog->tx_pktlog_ring;

	if (!tx_pktlog_ring->start) {
		return BCME_OK;
	}

	pktlog_filter = dhdp->pktlog->pktlog_filter;

	pktdata = (uint8 *)PKTDATA(dhdp->osh, pkt);
	if (dhd_pktlog_filter_matched(pktlog_filter, pktdata, PKTLOG_TXPKT_CASE)
			== FALSE) {
		return BCME_OK;
	}

	tx_pktlog_ring->dhdp = dhdp;
	if (dhd_pktlog_ring_get_writebuf(tx_pktlog_ring, &data)
			== BCME_OK) {
		tx_pkts = (dhd_pktlog_ring_info_t *)data;
		DHD_PKT_LOG(("%s(): write buf %p\n", __FUNCTION__, tx_pkts));
		pkt_hash = __dhd_dbg_pkt_hash((uintptr_t)pkt, pktid);
		ts_nsec = local_clock();
		rem_nsec = do_div(ts_nsec, NSEC_PER_SEC);

		tx_pkts->info.pkt = PKTDUP(dhdp->osh, pkt);
		tx_pkts->info.pkt_len = PKTLEN(dhdp->osh, pkt);
		tx_pkts->info.pkt_hash = pkt_hash;
		tx_pkts->info.driver_ts_sec = (uint32)ts_nsec;
		tx_pkts->info.driver_ts_usec = (uint32)(rem_nsec / NSEC_PER_USEC);
		tx_pkts->info.firmware_ts = 0U;
		tx_pkts->info.payload_type = FRAME_TYPE_ETHERNET_II;
		tx_pkts->tx_fate = TX_PKT_FATE_DRV_QUEUED;
		DHD_PKT_LOG(("%s(): pkt hash %d\n", __FUNCTION__, tx_pkts->info.pkt_hash));
		DHD_PKT_LOG(("%s(): sec %d usec %d\n", __FUNCTION__,
			tx_pkts->info.driver_ts_sec, tx_pkts->info.driver_ts_usec));
	} else {
		DHD_PKT_LOG(("%s(): Can't get write pos\n", __FUNCTION__));
	}

	return BCME_OK;
}

int
dhd_pktlog_ring_tx_status(dhd_pub_t *dhdp, void *pkt, uint32 pktid,
		uint16 status)
{
	dhd_pktlog_ring_info_t *tx_pkt;
	wifi_tx_packet_fate pkt_fate;
	uint32 pkt_hash, temp_hash;
	void *data = NULL;
	uint8 *pktdata = NULL;
	dhd_pktlog_ring_t *tx_pktlog_ring;
	dhd_pktlog_filter_t *pktlog_filter;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (!dhdp->pktlog->tx_pktlog_ring) {
		DHD_PKT_LOG(("%s(): tx_pktlog_ring is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	tx_pktlog_ring = dhdp->pktlog->tx_pktlog_ring;

	if (!tx_pktlog_ring->start) {
		return BCME_OK;
	}

	pktlog_filter = dhdp->pktlog->pktlog_filter;

	pktdata = (uint8 *)PKTDATA(dhdp->osh, pkt);
	if (dhd_pktlog_filter_matched(pktlog_filter, pktdata,
		PKTLOG_TXSTATUS_CASE) == FALSE) {
		return BCME_OK;
	}

	pkt_hash = __dhd_dbg_pkt_hash((uintptr_t)pkt, pktid);
	pkt_fate = __dhd_dbg_map_tx_status_to_pkt_fate(status);

	if (dhd_pktlog_ring_set_prevpos(tx_pktlog_ring) != BCME_OK) {
		DHD_PKT_LOG(("%s(): fail set prev pos\n", __FUNCTION__));
		return BCME_ERROR;
	}

	while (dhd_pktlog_ring_get_prevbuf(tx_pktlog_ring, &data)
			== BCME_OK) {
		DHD_PKT_LOG(("%s(): prev status_pos %p\n", __FUNCTION__, data));
		tx_pkt = (dhd_pktlog_ring_info_t *)data;
		temp_hash = tx_pkt->info.pkt_hash;
		if (temp_hash == pkt_hash) {
			tx_pkt->tx_fate = pkt_fate;
			DHD_PKT_LOG(("%s(): Found pkt hash in prev pos\n", __FUNCTION__));
			break;
		}
	}

	return BCME_OK;
}

int
dhd_pktlog_ring_rx_pkts(dhd_pub_t *dhdp, void *pkt)
{
	dhd_pktlog_ring_info_t *rx_pkts;
	void *data = NULL;
	uint8 *pktdata = NULL;
	dhd_pktlog_ring_t *rx_pktlog_ring;
	dhd_pktlog_filter_t *pktlog_filter;
	u64 ts_nsec;
	unsigned long rem_nsec;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (!dhdp->pktlog->rx_pktlog_ring) {
		DHD_PKT_LOG(("%s(): rx_pktlog_ring is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	rx_pktlog_ring = dhdp->pktlog->rx_pktlog_ring;

	if (!rx_pktlog_ring->start) {
		return BCME_OK;
	}

	pktlog_filter = dhdp->pktlog->pktlog_filter;

	pktdata = (uint8 *)PKTDATA(dhdp->osh, pkt);
	if (dhd_pktlog_filter_matched(pktlog_filter, pktdata, PKTLOG_RXPKT_CASE)
			== FALSE) {
		return BCME_OK;
	}

	rx_pktlog_ring->dhdp = dhdp;
	if (dhd_pktlog_ring_get_writebuf(rx_pktlog_ring, &data)
			== BCME_OK) {
		rx_pkts = (dhd_pktlog_ring_info_t *)data;
		DHD_PKT_LOG(("%s(): write buf %p\n", __FUNCTION__, rx_pkts));
		ts_nsec = local_clock();
		rem_nsec = do_div(ts_nsec, NSEC_PER_SEC);

		rx_pkts->info.pkt = PKTDUP(dhdp->osh, pkt);
		rx_pkts->info.pkt_len = PKTLEN(dhdp->osh, pkt);
		rx_pkts->info.pkt_hash = 0U;
		rx_pkts->info.driver_ts_sec = (uint32)ts_nsec;
		rx_pkts->info.driver_ts_usec = (uint32)(rem_nsec / NSEC_PER_USEC);
		rx_pkts->info.firmware_ts = 0U;
		rx_pkts->info.payload_type = FRAME_TYPE_ETHERNET_II;
		rx_pkts->rx_fate = RX_PKT_FATE_SUCCESS;
		DHD_PKT_LOG(("%s(): sec %d usec %d\n", __FUNCTION__,
			rx_pkts->info.driver_ts_sec, rx_pkts->info.driver_ts_usec));
	} else {
		DHD_PKT_LOG(("%s(): Can't get write pos\n", __FUNCTION__));
	}

	return BCME_OK;
}

dhd_pktlog_filter_t*
dhd_pktlog_filter_init(int size)
{
	int i;
	gfp_t kflags;
	uint32 alloc_len;
	dhd_pktlog_filter_t *filter;
	dhd_pktlog_filter_info_t *filter_info = NULL;

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	/* allocate and initialze pktmon filter */
	alloc_len = sizeof(dhd_pktlog_filter_t);
	filter = (dhd_pktlog_filter_t *)kzalloc(alloc_len, kflags);
	if (unlikely(!filter)) {
		DHD_ERROR(("%s(): could not allocate memory for - "
					"dhd_pktlog_filter_t\n", __FUNCTION__));
		goto fail;
	}

	alloc_len = (sizeof(dhd_pktlog_filter_info_t) * size);
	filter_info = (dhd_pktlog_filter_info_t *)kzalloc(alloc_len, kflags);
	if (unlikely(!filter_info)) {
		DHD_ERROR(("%s(): could not allocate memory for - "
					"dhd_pktlog_filter_info_t\n", __FUNCTION__));
		goto fail;
	}

	filter->info = filter_info;
	filter->list_cnt = 0;

	for (i = 0; i < MAX_DHD_PKTLOG_FILTER_LEN; i++) {
		filter->info[i].id = 0;
	}

	filter->enable = PKTLOG_TXPKT_CASE | PKTLOG_TXSTATUS_CASE | PKTLOG_RXPKT_CASE;

	DHD_ERROR(("%s(): pktlog filter init success\n", __FUNCTION__));

	return filter;
fail:
	if (filter_info) {
		kfree(filter_info);
	}

	if (filter) {
		kfree(filter);
	}

	return NULL;
}

int
dhd_pktlog_filter_deinit(dhd_pktlog_filter_t *filter)
{
	int ret = BCME_OK;

	if (!filter) {
		DHD_ERROR(("%s(): filter is NULL\n", __FUNCTION__));
		return -EINVAL;
	}

	if (filter->info) {
		kfree(filter->info);
	}
	kfree(filter);

	DHD_ERROR(("%s(): pktlog filter deinit\n", __FUNCTION__));

	return ret;
}

bool
dhd_pktlog_filter_existed(dhd_pktlog_filter_t *filter, char *arg, uint32 *id)
{
	char filter_pattern[MAX_FILTER_PATTERN_LEN];
	char *p;
	int i, j;
	int nchar;
	int len;

	if  (!filter || !arg) {
		DHD_ERROR(("%s(): filter=%p arg=%p\n", __FUNCTION__, filter, arg));
		return TRUE;
	}

	for (i = 0; i < filter->list_cnt; i++) {
		p = filter_pattern;
		len = sizeof(filter_pattern);

		nchar = snprintf(p, len, "%d ", filter->info[i].offset);
		p += nchar;
		len -= nchar;

		nchar = snprintf(p, len, "0x");
		p += nchar;
		len -= nchar;

		for (j = 0; j < filter->info[i].size_bytes; j++) {
			nchar = snprintf(p, len, "%02x", filter->info[i].mask[j]);
			p += nchar;
			len -= nchar;
		}

		nchar = snprintf(p, len, " 0x");
		p += nchar;
		len -= nchar;

		for (j = 0; j < filter->info[i].size_bytes; j++) {
			nchar = snprintf(p, len, "%02x", filter->info[i].pattern[j]);
			p += nchar;
			len -= nchar;
		}

		DHD_PKT_LOG(("%s(): Pattern %s\n", __FUNCTION__, filter_pattern));

		if (strncmp(filter_pattern, arg, strlen(filter_pattern)) == 0) {
			*id = filter->info[i].id;
			DHD_ERROR(("%s(): This pattern is existed\n", __FUNCTION__));
			DHD_ERROR(("%s(): arg %s\n", __FUNCTION__, arg));
			return TRUE;
		}
	}

	return FALSE;
}

int
dhd_pktlog_filter_add(dhd_pktlog_filter_t *filter, char *arg)
{
	int32 mask_size, pattern_size;
	char *offset, *bitmask, *pattern;
	uint32 id = 0;

	if  (!filter || !arg) {
		DHD_ERROR(("%s(): pktlog_filter =%p arg =%p\n", __FUNCTION__, filter, arg));
		return BCME_ERROR;
	}

	DHD_PKT_LOG(("%s(): arg %s\n", __FUNCTION__, arg));

	if (dhd_pktlog_filter_existed(filter, arg, &id) == TRUE) {
		DHD_PKT_LOG(("%s(): This pattern id %d is existed\n", __FUNCTION__, id));
		return BCME_OK;
	}

	if (filter->list_cnt >= MAX_DHD_PKTLOG_FILTER_LEN) {
		DHD_ERROR(("%s(): pktlog filter full\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if ((offset = bcmstrtok(&arg, " ", 0)) == NULL) {
		DHD_ERROR(("%s(): offset not found\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if ((bitmask = bcmstrtok(&arg, " ", 0)) == NULL) {
		DHD_ERROR(("%s(): bitmask not found\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if ((pattern = bcmstrtok(&arg, " ", 0)) == NULL) {
		DHD_ERROR(("%s(): pattern not found\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* parse filter bitmask */
	mask_size = wl_pattern_atoh(bitmask,
			(char *) &filter->info[filter->list_cnt].mask[0]);
	if (mask_size == -1) {
		DHD_ERROR(("Rejecting: %s\n", bitmask));
		return BCME_ERROR;
	}

	/* parse filter pattern */
	pattern_size = wl_pattern_atoh(pattern,
			(char *) &filter->info[filter->list_cnt].pattern[0]);
	if (pattern_size == -1) {
		DHD_ERROR(("Rejecting: %s\n", pattern));
		return BCME_ERROR;
	}

	prhex("mask", (char *)&filter->info[filter->list_cnt].mask[0],
			mask_size);
	prhex("pattern", (char *)&filter->info[filter->list_cnt].pattern[0],
			pattern_size);

	if (mask_size != pattern_size) {
		DHD_ERROR(("%s(): Mask and pattern not the same size\n", __FUNCTION__));
		return BCME_ERROR;
	}

	filter->info[filter->list_cnt].offset = strtoul(offset, NULL, 0);
	filter->info[filter->list_cnt].size_bytes = mask_size;
	filter->info[filter->list_cnt].id = filter->list_cnt + 1;
	filter->info[filter->list_cnt].enable = TRUE;

	filter->list_cnt++;

	return BCME_OK;
}

int
dhd_pktlog_filter_enable(dhd_pktlog_filter_t *filter, uint32 pktmon_case, uint32 enable)
{
	if  (!filter) {
		DHD_ERROR(("%s(): filter is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	DHD_PKT_LOG(("%s(): pktlog_case %d enable %d\n", __FUNCTION__, pktmon_case, enable));

	if (enable) {
		filter->enable |=  pktmon_case;
	} else {
		filter->enable &= ~pktmon_case;
	}

	return BCME_OK;
}

int
dhd_pktlog_filter_pattern_enable(dhd_pktlog_filter_t *filter, char *arg, uint32 enable)
{
	uint32 id = 0;

	if  (!filter || !arg) {
		DHD_ERROR(("%s(): pktlog_filter =%p arg =%p\n", __FUNCTION__, filter, arg));
		return BCME_ERROR;
	}

	if (dhd_pktlog_filter_existed(filter, arg, &id) == TRUE) {
		if (id > 0) {
			filter->info[id-1].enable = enable;
			DHD_ERROR(("%s(): This pattern id %d is %s\n",
				__FUNCTION__, id, (enable ? "enabled" : "disabled")));
		}
	} else {
		DHD_ERROR(("%s(): This pattern is not existed\n", __FUNCTION__));
		DHD_ERROR(("%s(): arg %s\n", __FUNCTION__, arg));
	}

	return BCME_OK;
}

int
dhd_pktlog_filter_info(dhd_pktlog_filter_t *filter)
{
	char filter_pattern[MAX_FILTER_PATTERN_LEN];
	char *p;
	int i, j;
	int nchar;
	int len;

	if  (!filter) {
		DHD_ERROR(("%s(): pktlog_filter is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	DHD_ERROR(("---- PKTLOG FILTER INFO ----\n\n"));

	DHD_ERROR(("Filter list cnt %d Filter is %s\n",
		filter->list_cnt, (filter->enable ? "enabled" : "disabled")));

	for (i = 0; i < filter->list_cnt; i++) {
		p = filter_pattern;
		len = sizeof(filter_pattern);

		nchar = snprintf(p, len, "%d ", filter->info[i].offset);
		p += nchar;
		len -= nchar;

		nchar = snprintf(p, len, "0x");
		p += nchar;
		len -= nchar;

		for (j = 0; j < filter->info[i].size_bytes; j++) {
			nchar = snprintf(p, len, "%02x", filter->info[i].mask[j]);
			p += nchar;
			len -= nchar;
		}

		nchar = snprintf(p, len, " 0x");
		p += nchar;
		len -= nchar;

		for (j = 0; j < filter->info[i].size_bytes; j++) {
			nchar = snprintf(p, len, "%02x", filter->info[i].pattern[j]);
			p += nchar;
			len -= nchar;
		}

		DHD_ERROR(("ID:%d is %s\n",
			filter->info[i].id, (filter->info[i].enable ? "enabled" : "disabled")));
		DHD_ERROR(("Pattern %s\n", filter_pattern));
	}

	DHD_ERROR(("---- PKTLOG FILTER END ----\n"));

	return BCME_OK;
}
bool
dhd_pktlog_filter_matched(dhd_pktlog_filter_t *filter, char *data, uint32 pktlog_case)
{
	uint16 szbts;	/* pattern size */
	uint16 offset;	/* pattern offset */
	int i, j;
	uint8 *mask = NULL;		/* bitmask */
	uint8 *pattern = NULL;
	uint8 *pkt_offset = NULL;	/* packet offset */
	bool matched;

	if  (!filter || !data) {
		DHD_PKT_LOG(("%s(): filter=%p data=%p\n",
			__FUNCTION__, filter, data));
		return TRUE;
	}

	if (!(pktlog_case & filter->enable)) {
		DHD_PKT_LOG(("%s(): pktlog_case %d return TRUE filter is disabled\n",
			__FUNCTION__, pktlog_case));
		return TRUE;
	}

	for (i = 0; i < filter->list_cnt; i++) {
		if (&filter->info[i] && filter->info[i].id && filter->info[i].enable) {
			szbts = filter->info[i].size_bytes;
			offset = filter->info[i].offset;
			mask = &filter->info[i].mask[0];
			pkt_offset = &data[offset];
			pattern = &filter->info[i].pattern[0];

			matched = TRUE;
			for (j = 0; j < szbts; j++) {
				if ((mask[j] & pkt_offset[j]) != pattern[j]) {
					matched = FALSE;
					break;
				}
			}

			if (matched) {
				DHD_PKT_LOG(("%s(): pktlog_filter return TRUE id %d\n",
					__FUNCTION__, filter->info[i].id));
				return TRUE;
			}
		} else {
			DHD_PKT_LOG(("%s(): filter ino is null %p\n",
				__FUNCTION__, &filter->info[i]));
		}
	}

	return FALSE;
}

/* Ethernet Type MAC Header 12 bytes + Frame payload 10 bytes */
#define PKTLOG_MINIMIZE_REPORT_LEN 22

static char pktlog_minmize_mask_table[] = {
	0xff, 0x00, 0x00, 0x00, 0xff, 0x0f, /* Ethernet Type MAC Header - Destination MAC Address */
	0xff, 0x00, 0x00, 0x00, 0xff, 0x0f, /* Ethernet Type MAC Header - Source MAC Address */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* Ethernet Type MAC Header - Ether Type - 2 bytes */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* Frame payload */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, /* UDP port number offset - bytes as 0xff */
	0xff, 0xff,
};

static inline void
dhd_pktlog_minimize_report(char *pkt, uint32 frame_len)
{
	int i;
	int table_len;
	int report_len;
	int remain_len;
	char *p_table;

	table_len = sizeof(pktlog_minmize_mask_table);
	report_len =  table_len;
	p_table = &pktlog_minmize_mask_table[0];

	if (frame_len < PKTLOG_MINIMIZE_REPORT_LEN) {
		return;
	}

	if (frame_len < table_len) {
		report_len = PKTLOG_MINIMIZE_REPORT_LEN;
	}

	remain_len = frame_len - report_len;

	for (i = 0; i < report_len; i++) {
		pkt[i] = pkt[i] & p_table[i];
	}

	if (remain_len) {
		memset(&pkt[report_len], 0x00, remain_len);
	}
}

static int
dhd_pktlog_pkts_write_file(dhd_pktlog_ring_t *ringbuf, struct file *w_pcap_fp, int *total_cnt)
{
	dhd_pktlog_ring_info_t *report_ptr;
	void *data = NULL;
	int count = 0;
	uint32 frame_len;
	uint32 write_frame_len;
	int ret = BCME_OK;
	uint32 len, alloc_len, remain_len;
	char *report_buf = NULL;
	char *p = NULL;
	int bytes_user_data = 0;
	char buf[DHD_PKTLOG_FATE_INFO_STR_LEN];

	if (!ringbuf || !w_pcap_fp) {
		DHD_PKT_LOG(("%s(): pktlog_ring=%p w_pcap_fp=%p NULL\n",
			__FUNCTION__, ringbuf, w_pcap_fp));
		return BCME_ERROR;
	}

	if (dhd_pktlog_ring_set_nextpos(ringbuf) != BCME_OK) {
		return BCME_ERROR;
	}

	if (!ringbuf->dhdp) {
		DHD_ERROR(("%s(): dhdp is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	DHD_PKT_LOG(("%s(): BEGIN\n", __FUNCTION__));

	alloc_len = PKTLOG_DUMP_BUF_SIZE;

	report_buf =
		DHD_OS_PREALLOC(ringbuf->dhdp, DHD_PREALLOC_DHD_PKTLOG_DUMP_BUF, alloc_len);
	if (unlikely(!report_buf)) {
		DHD_ERROR(("%s(): could not allocate memory for - "
					"report_buf size %d\n", __FUNCTION__, alloc_len));
		ret = -ENOMEM;
		goto fail;
	}

	p = report_buf;
	len = 0;
	remain_len = 0;

	while (dhd_pktlog_ring_get_nextbuf(ringbuf, &data) == BCME_OK) {
		report_ptr = (dhd_pktlog_ring_info_t *)data;

		memcpy(p, (char*)&report_ptr->info.driver_ts_sec,
				sizeof(report_ptr->info.driver_ts_sec));
		p += sizeof(report_ptr->info.driver_ts_sec);
		len += sizeof(report_ptr->info.driver_ts_sec);

		memcpy(p, (char*)&report_ptr->info.driver_ts_usec,
				sizeof(report_ptr->info.driver_ts_usec));
		p += sizeof(report_ptr->info.driver_ts_usec);
		len += sizeof(report_ptr->info.driver_ts_usec);

		if (report_ptr->info.payload_type == FRAME_TYPE_ETHERNET_II) {
			frame_len = min(report_ptr->info.pkt_len, (size_t)MAX_FRAME_LEN_ETHERNET);
		} else {
			frame_len = min(report_ptr->info.pkt_len, (size_t)MAX_FRAME_LEN_80211_MGMT);
		}

		bytes_user_data = sprintf(buf, "%s:%s:%02d\n", DHD_PKTLOG_FATE_INFO_FORMAT,
				(report_ptr->tx_fate ? "Failure" : "Succeed"), report_ptr->tx_fate);
		write_frame_len = frame_len + bytes_user_data;

		/* pcap pkt head has incl_len and orig_len */
		memcpy(p, (char*)&write_frame_len, sizeof(write_frame_len));
		p += sizeof(write_frame_len);
		len += sizeof(write_frame_len);
		memcpy(p, (char*)&write_frame_len, sizeof(write_frame_len));
		p += sizeof(write_frame_len);
		len += sizeof(write_frame_len);

		memcpy(p, PKTDATA(ringbuf->dhdp->osh, report_ptr->info.pkt), frame_len);
		if (ringbuf->pktlog_minmize) {
			dhd_pktlog_minimize_report(p, frame_len);
		}
		p += frame_len;
		len += frame_len;

		memcpy(p, buf, bytes_user_data);
		p += bytes_user_data;
		len += bytes_user_data;

		count++;

		DHD_PKT_LOG(("%s(): write cnt %d frame_len %d\n", __FUNCTION__, count, frame_len));

		remain_len = len;
		remain_len += (MAX_FRAME_LEN_80211_MGMT + DHD_PKTLOG_FATE_INFO_STR_LEN);

		if (remain_len > alloc_len) {
			ret = vfs_write(w_pcap_fp, report_buf, len, &w_pcap_fp->f_pos);
			if (ret < 0) {
				DHD_ERROR(("%s(): write pkt data error, err = %d\n",
					__FUNCTION__, ret));
				goto fail;
			}

			p = report_buf;
			len = 0;
			remain_len = 0;
		}
	}

	if (len) {
		ret = vfs_write(w_pcap_fp, report_buf, len, &w_pcap_fp->f_pos);
		if (ret < 0) {
			DHD_ERROR(("%s(): write pkt data error, err = %d\n", __FUNCTION__, ret));
			goto fail;
		}
	}

	if (ret < 0) {
		DHD_ERROR(("%s(): write pkt fate error, err = %d\n", __FUNCTION__, ret));
	}

fail:
	*total_cnt = *total_cnt + count;

	if (report_buf) {
		DHD_OS_PREFREE(ringbuf->dhdp, report_buf, alloc_len);
	}

	return ret;
}

int
dhd_pktlog_write_file(dhd_pub_t *dhdp)
{
	struct file *w_pcap_fp = NULL;
	mm_segment_t old_fs;
	uint32 file_mode;
	char dump_path[128];
	struct timeval curtime;
	int ret = BCME_OK;
	dhd_pktlog_ring_t *tx_pktlog_ring;
	dhd_pktlog_ring_t *rx_pktlog_ring;
	dhd_pktlog_pcap_hdr_t pcap_h;
	int total_cnt = 0;

	if (!dhdp || !dhdp->pktlog) {
		DHD_PKT_LOG(("%s(): dhdp=%p pktlog=%p\n",
			__FUNCTION__, dhdp, (dhdp ? dhdp->pktlog : NULL)));
		return -EINVAL;
	}

	if (!dhdp->pktlog->tx_pktlog_ring || !dhdp->pktlog->rx_pktlog_ring) {
		DHD_PKT_LOG(("%s(): tx_pktlog_ring =%p rx_pktlog_ring=%p\n",
			__FUNCTION__, dhdp->pktlog->tx_pktlog_ring,
			dhdp->pktlog->rx_pktlog_ring));
		return -EINVAL;
	}

	pcap_h.magic_number = PKTLOG_PCAP_MAGIC_NUM;
	pcap_h.version_major = PKTLOG_PCAP_MAJOR_VER;
	pcap_h.version_minor = PKTLOG_PCAP_MINOR_VER;
	pcap_h.thiszone = 0x0;
	pcap_h.sigfigs = 0x0;
	pcap_h.snaplen = PKTLOG_PCAP_SNAP_LEN;
	pcap_h.network = PKTLOG_PCAP_NETWORK_TYPE;

	tx_pktlog_ring = dhdp->pktlog->tx_pktlog_ring;
	rx_pktlog_ring = dhdp->pktlog->rx_pktlog_ring;

	tx_pktlog_ring->start = FALSE;
	rx_pktlog_ring->start = FALSE;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	memset(dump_path, 0, sizeof(dump_path));
	do_gettimeofday(&curtime);
	snprintf(dump_path, sizeof(dump_path), "%s_%ld.%ld.pcap",
			DHD_PKTLOG_DUMP_PATH DHD_PKTLOG_DUMP_TYPE,
			(unsigned long)curtime.tv_sec, (unsigned long)curtime.tv_usec);

	file_mode = O_CREAT | O_WRONLY;

	DHD_ERROR(("pktlog_dump_pcap = %s\n", dump_path));

	w_pcap_fp = filp_open(dump_path, file_mode, 0664);
	if (IS_ERR(w_pcap_fp)) {
		DHD_ERROR(("%s: Couldn't open file '%s' err %ld\n",
			__FUNCTION__, dump_path, PTR_ERR(w_pcap_fp)));
		ret = BCME_ERROR;
		goto fail;
	}

	ret = vfs_write(w_pcap_fp, (char*)&pcap_h, sizeof(pcap_h), &w_pcap_fp->f_pos);

	if (ret < 0) {
		DHD_ERROR(("%s(): write pcap head error, err = %d\n", __FUNCTION__, ret));
		goto fail;
	}

	tx_pktlog_ring->dhdp = dhdp;
	ret = dhd_pktlog_pkts_write_file(tx_pktlog_ring, w_pcap_fp, &total_cnt);

	if (ret < 0) {
		DHD_ERROR(("%s(): write tx pkts error, err = %d\n", __FUNCTION__, ret));
		goto fail;
	}
	DHD_ERROR(("pktlog tx pkts write is end, err = %d\n", ret));

	rx_pktlog_ring->dhdp = dhdp;
	ret = dhd_pktlog_pkts_write_file(rx_pktlog_ring, w_pcap_fp, &total_cnt);

	if (ret < 0) {
		DHD_ERROR(("%s(): write rx pkts error, err = %d\n", __FUNCTION__, ret));
		goto fail;
	}
	DHD_ERROR(("pktlog rx pkts write is end, err = %d\n", ret));

	/* Sync file from filesystem to physical media */
	ret = vfs_fsync(w_pcap_fp, 0);
	if (ret < 0) {
		DHD_ERROR(("%s(): sync pcap file error, err = %d\n", __FUNCTION__, ret));
		goto fail;
	}

fail:
	tx_pktlog_ring->start = TRUE;
	rx_pktlog_ring->start = TRUE;

	if (!IS_ERR(w_pcap_fp)) {
		filp_close(w_pcap_fp, NULL);
	}

	set_fs(old_fs);

	DHD_ERROR(("pktlog write file is end, err = %d\n", ret));

#ifdef DHD_DUMP_MNGR
	if (ret >= 0) {
		dhd_dump_file_manage_enqueue(dhdp, dump_path, DHD_PKTLOG_DUMP_TYPE);
	}
#endif /* DHD_DUMP_MNGR */

	return ret;
}

dhd_pktlog_ring_t*
dhd_pktlog_ring_change_size(dhd_pktlog_ring_t *ringbuf, int size)
{
	uint32 alloc_len;
	uint32 pktlog_start;
	uint32 pktlog_minmize;
	dhd_pktlog_ring_t *pktlog_ring;
	dhd_pub_t *dhdp;

	if  (!ringbuf) {
		DHD_ERROR(("%s(): ringbuf is NULL\n", __FUNCTION__));
		return NULL;
	}

	alloc_len = size;
	if (alloc_len < MIN_PKTLOG_LEN) {
		alloc_len = MIN_PKTLOG_LEN;
	}
	if (alloc_len > MAX_PKTLOG_LEN) {
		alloc_len = MAX_PKTLOG_LEN;
	}
	DHD_ERROR(("ring size requested: %d alloc: %d\n", size, alloc_len));

	/* backup variable */
	pktlog_start = ringbuf->start;
	pktlog_minmize = ringbuf->pktlog_minmize;
	dhdp = ringbuf->dhdp;

	/* free ring_info */
	dhd_pktlog_ring_deinit(ringbuf);

	/* alloc ring_info */
	pktlog_ring = dhd_pktlog_ring_init(dhdp, alloc_len);

	/* restore variable */
	if (pktlog_ring) {
		pktlog_ring->start = pktlog_start;
		pktlog_ring->pktlog_minmize = pktlog_minmize;
	}

	return pktlog_ring;
}
#endif /* DHD_PKT_LOGGING */
