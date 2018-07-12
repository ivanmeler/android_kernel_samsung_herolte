/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for compress
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _COMPRESS_H_
#define _COMPRESS_H_

/*
 * struct compress_info
 *
 * @comp_header : 	point the address which contains compressed size and
 *			the size value should be in 2bytes per each.
 * @compress_addr : 	base address which stores compressed data and it must
 * 			be contiguous.
 * @first_idx :		the page index of first compressed data.
 */
struct compress_info {
	u16 *comp_header;
	u8 *comp_addr;
	u32 first_idx;
};

#define COMP_MEMCPY 0
#define COMP_READY 1

/*
 * sturct compress_ops - driver-specific callbacks
 *
 * @device_init :		A compress device is initialized by this function
 *				and register callback function which is called
 *				when compress is completed.
 * @request_compress :		Request compress data. According to threshold
 *				of compress device, the compression can be directly,
 * 				or accumulate and later.
 * @cancel_compress :		Remove the data which is not compressed data, but
 * 				accumulated data.
 * @request_decompress :	Request decompress data. this can be
 *				implemented by S/W.
 */
struct compress_ops {
	int (*device_init)(void *priv, u32 *nr_pages,
			u32 *unit, u32 (*cb)(struct compress_info *info));
	int (*request_compress)(void *priv, u8 *src, u32 *page_num);
	void (*start_compress)(void *priv, u32 page_idx);
	int (*cancel_compress)(void *priv, u8 *dst, u32 page_num);
	int (*request_decompress)(void *priv, u8 *in, u32 length, u8 *out);
};

/*
 * compress_register_ops - Register compress_ops
 * The compressor user(e.g. eswap, sswap) should implement this function
 */
void compress_register_ops(struct compress_ops *ops, void *priv);

#endif /* _COMPRESS_H_ */
