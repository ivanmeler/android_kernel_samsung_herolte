/*
 * Driver O/S-independent utility routines
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
 * $Id: bcmxtlv.c 497218 2014-08-18 13:05:12Z $
 */

#include <bcm_cfg.h>

#include <typedefs.h>
#include <bcmdefs.h>

#include <stdarg.h>

#ifdef BCMDRIVER
#include <osl.h>
#else /* !BCMDRIVER */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef ASSERT
#define ASSERT(exp)
#endif
inline void* MALLOCZ(void *o, size_t s) { BCM_REFERENCE(o); return calloc(1, s); }
inline void MFREE(void *o, void *p, size_t s) { BCM_REFERENCE(o); BCM_REFERENCE(s); free(p); }
#endif /* !BCMDRIVER */

#include <bcmendian.h>
#include <bcmutils.h>

bcm_xtlv_t *
bcm_next_xtlv(bcm_xtlv_t *elt, int *buflen)
{
	int sz;

	/* validate current elt */
	if (!bcm_valid_xtlv(elt, *buflen))
		return NULL;

	/* advance to next elt */
	sz = BCM_XTLV_SIZE(elt);
	elt = (bcm_xtlv_t*)((uint8 *)elt + sz);
	*buflen -= sz;

	/* validate next elt */
	if (!bcm_valid_xtlv(elt, *buflen))
		return NULL;

	return elt;
}

struct bcm_tlvbuf *
bcm_xtlv_buf_alloc(void *osh, uint16 len)
{
	struct bcm_tlvbuf *tbuf = MALLOCZ(osh, sizeof(struct bcm_tlvbuf) + len);
	if (tbuf == NULL) {
		return NULL;
	}
	tbuf->size = len;
	tbuf->head = tbuf->buf = (uint8 *)(tbuf + 1);
	return tbuf;
}
void
bcm_xtlv_buf_free(void *osh, struct bcm_tlvbuf *tbuf)
{
	if (tbuf == NULL)
		return;
	MFREE(osh, tbuf, (tbuf->size + sizeof(struct bcm_tlvbuf)));
}
uint16
bcm_xtlv_buf_len(struct bcm_tlvbuf *tbuf)
{
	if (tbuf == NULL) return 0;
	return (tbuf->buf - tbuf->head);
}
uint16
bcm_xtlv_buf_rlen(struct bcm_tlvbuf *tbuf)
{
	if (tbuf == NULL) return 0;
	return tbuf->size - bcm_xtlv_buf_len(tbuf);
}
uint8 *
bcm_xtlv_buf(struct bcm_tlvbuf *tbuf)
{
	if (tbuf == NULL) return NULL;
	return tbuf->buf;
}
uint8 *
bcm_xtlv_head(struct bcm_tlvbuf *tbuf)
{
	if (tbuf == NULL) return NULL;
	return tbuf->head;
}
int
bcm_xtlv_put_data(struct bcm_tlvbuf *tbuf, uint16 type, const void *data, uint16 dlen)
{
	bcm_xtlv_t *xtlv;
	if (tbuf == NULL || bcm_xtlv_buf_rlen(tbuf) < (dlen + BCM_XTLV_HDR_SIZE))
		return BCME_BADARG;
	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(tbuf);
	xtlv->id = htol16(type);
	xtlv->len = htol16(dlen);
	memcpy(xtlv->data, data, dlen);
	tbuf->buf += BCM_XTLV_SIZE(xtlv);
	return BCME_OK;
}
int
bcm_xtlv_put_8(struct bcm_tlvbuf *tbuf, uint16 type, const int8 data)
{
	bcm_xtlv_t *xtlv;
	if (tbuf == NULL || bcm_xtlv_buf_rlen(tbuf) < (1 + BCM_XTLV_HDR_SIZE))
		return BCME_BADARG;
	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(tbuf);
	xtlv->id = htol16(type);
	xtlv->len = htol16(sizeof(data));
	xtlv->data[0] = data;
	tbuf->buf += BCM_XTLV_SIZE(xtlv);
	return BCME_OK;
}
int
bcm_xtlv_put_16(struct bcm_tlvbuf *tbuf, uint16 type, const int16 data)
{
	bcm_xtlv_t *xtlv;
	if (tbuf == NULL || bcm_xtlv_buf_rlen(tbuf) < (2 + BCM_XTLV_HDR_SIZE))
		return BCME_BADARG;
	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(tbuf);
	xtlv->id = htol16(type);
	xtlv->len = htol16(sizeof(data));
	htol16_ua_store(data, xtlv->data);
	tbuf->buf += BCM_XTLV_SIZE(xtlv);
	return BCME_OK;
}
int
bcm_xtlv_put_32(struct bcm_tlvbuf *tbuf, uint16 type, const int32 data)
{
	bcm_xtlv_t *xtlv;
	if (tbuf == NULL || bcm_xtlv_buf_rlen(tbuf) < (4 + BCM_XTLV_HDR_SIZE))
		return BCME_BADARG;
	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(tbuf);
	xtlv->id = htol16(type);
	xtlv->len = htol16(sizeof(data));
	htol32_ua_store(data, xtlv->data);
	tbuf->buf += BCM_XTLV_SIZE(xtlv);
	return BCME_OK;
}

int
bcm_skip_xtlv(void **tlv_buf)
{
	bcm_xtlv_t *ptlv = *tlv_buf;
	uint16 len;
	uint16 type;

	ASSERT(ptlv);
	/* tlv headr is always packed in LE order */
	len = ltoh16(ptlv->len);
	type = ltoh16(ptlv->id);

	printf("xtlv_skip: Skipping tlv [type:%d,len:%d]\n", type, len);

	*tlv_buf += BCM_XTLV_SIZE(ptlv);
	return BCME_OK;
}

/*
 *  upacks xtlv record from buf checks the type
 *  copies data to callers buffer
 *  advances tlv pointer to next record
 *  caller's resposible for dst space check
 */
int
bcm_unpack_xtlv_entry(void **tlv_buf, uint16 xpct_type, uint16 xpct_len, void *dst)
{
	bcm_xtlv_t *ptlv = *tlv_buf;
	uint16 len;
	uint16 type;

	ASSERT(ptlv);
	/* tlv headr is always packed in LE order */
	len = ltoh16(ptlv->len);
	type = ltoh16(ptlv->id);
	if (len == 0) {
		/* z-len tlv headers: allow, but don't process */
		printf("z-len, skip unpack\n");
	} else  {
		if ((type != xpct_type) ||
			(len > xpct_len)) {
			printf("xtlv_unpack Error: found[type:%d,len:%d] != xpct[type:%d,len:%d]\n",
				type, len, xpct_type, xpct_len);
			return BCME_BADARG;
		}
		/* copy tlv record to caller's buffer */
		memcpy(dst, ptlv->data, ptlv->len);
	}
	*tlv_buf += BCM_XTLV_SIZE(ptlv);
	return BCME_OK;
}

/*
 *  packs user data into tlv record
 *  advances tlv pointer to next xtlv slot
 *  buflen is used for tlv_buf space check
 */
int
bcm_pack_xtlv_entry(void **tlv_buf, uint16 *buflen, uint16 type, uint16 len, void *src)
{
	bcm_xtlv_t *ptlv = *tlv_buf;

	/* copy data from tlv buffer to dst provided by user */

	ASSERT(ptlv);
	ASSERT(src);

	if ((BCM_XTLV_HDR_SIZE + len) > *buflen) {
		printf("bcm_pack_xtlv_entry: no space tlv_buf: requested:%d, available:%d\n",
		 ((int)BCM_XTLV_HDR_SIZE + len), *buflen);
		return BCME_BADLEN;
	}
	ptlv->id = htol16(type);
	ptlv->len = htol16(len);

	/* copy callers data */
	memcpy(ptlv->data, src, len);

	/* advance callers pointer to tlv buff */
	*tlv_buf += BCM_XTLV_SIZE(ptlv);
	/* decrement the len */
	*buflen -=  BCM_XTLV_SIZE(ptlv);
	return BCME_OK;
}

/*
 *  unpack all xtlv records from the issue a callback
 *  to set function one call per found tlv record
 */
int
bcm_unpack_xtlv_buf(void *ctx, void *tlv_buf, uint16 buflen, bcm_set_var_from_tlv_cbfn_t *cbfn)
{
	uint16 len;
	uint16 type;
	int res = 0;
	bcm_xtlv_t *ptlv = tlv_buf;
	int sbuflen = buflen;

	ASSERT(ptlv);
	ASSERT(cbfn);

	while (sbuflen >= 0) {
		ptlv = tlv_buf;

		/* tlv header is always packed in LE order */
		len = ltoh16(ptlv->len);
		if (len == 0)	/* can't be zero */
			break;
		type = ltoh16(ptlv->id);

		sbuflen -= (BCM_XTLV_HDR_SIZE + len);

		/* check for possible buffer overrun */
		if (sbuflen < 0)
			break;

		if ((res = cbfn(ctx, &tlv_buf, type, len)) != BCME_OK)
			break;
	}
	return res;
}

/*
 *  pack xtlv buffer from memory according to xtlv_desc_t
 */
int
bcm_pack_xtlv_buf_from_mem(void **tlv_buf, uint16 *buflen, xtlv_desc_t *items)
{
	int res = 0;
	void *ptlv = *tlv_buf;

	while (items->len != 0) {
		if ((res = bcm_pack_xtlv_entry(&ptlv,
			buflen, items->type,
			items->len, items->ptr) != BCME_OK)) {
			break;
		}
		items++;
	}
	*tlv_buf = ptlv; /* update the external pointer */
	return res;
}

/*
 *  unpack xtlv buffer to memory according to xtlv_desc_t
 *
 */
int
bcm_unpack_xtlv_buf_to_mem(void *tlv_buf, int *buflen, xtlv_desc_t *items)
{
	int res = 0;
	bcm_xtlv_t *elt = tlv_buf;

	/*  iterate through tlvs in the buffer  */
	while  ((elt = bcm_next_xtlv(elt, buflen)) != NULL) {

		/*  find matches in desc_t items  */
		xtlv_desc_t *dst_desc = items;
		while (dst_desc->len != 0) {

			if ((elt->id == dst_desc->type) &&
				(elt->len == dst_desc->len)) {
				/* copy xtlv data to mem dst */
				memcpy(dst_desc->ptr, elt->data, elt->len);
				if ((*buflen -= elt->len) < 0) {
					printf("%s:Error: dst buf overrun\n", __FUNCTION__);
					return BCME_NOMEM;
				}
			}
		}
	}
	return res;
}

/*
 *  packs user data (in hex string) into tlv record
 *  advances tlv pointer to next xtlv slot
 *  buflen is used for tlv_buf space check
 */
static int
get_ie_data(uchar *data_str, uchar *ie_data, int len)
{
	uchar *src, *dest;
	uchar val;
	int idx;
	char hexstr[3];

	src = data_str;
	dest = ie_data;

	for (idx = 0; idx < len; idx++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];
		hexstr[2] = '\0';

#ifdef BCMDRIVER
		val = (uchar) simple_strtoul(hexstr, NULL, 16);
#else
		val = (uchar) strtoul(hexstr, NULL, 16);
#endif

		*dest++ = val;
		src += 2;
	}

	return 0;
}

int
bcm_pack_xtlv_entry_from_hex_string(void **tlv_buf, uint16 *buflen, uint16 type, char *hex)
{
	bcm_xtlv_t *ptlv = *tlv_buf;
	uint16 len = strlen(hex)/2;

	/* copy data from tlv buffer to dst provided by user */

	if ((BCM_XTLV_HDR_SIZE + len) > *buflen) {
		printf("bcm_pack_xtlv_entry: no space tlv_buf: requested:%d, available:%d\n",
			((int)BCM_XTLV_HDR_SIZE + len), *buflen);
		return BCME_BADLEN;
	}
	ptlv->id = htol16(type);
	ptlv->len = htol16(len);

	/* copy callers data */
	if (get_ie_data((uchar*)hex, ptlv->data, len)) {
		return BCME_BADARG;
	}

	/* advance callers pointer to tlv buff */
	*tlv_buf += BCM_XTLV_SIZE(ptlv);
	/* decrement the len */
	*buflen -=  BCM_XTLV_SIZE(ptlv);
	return BCME_OK;
}
