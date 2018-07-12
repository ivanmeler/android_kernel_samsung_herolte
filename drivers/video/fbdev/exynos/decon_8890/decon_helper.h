/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_DECON_HELPER_H__
#define __SAMSUNG_DECON_HELPER_H__

#include <linux/device.h>

#include "decon.h"

int decon_clk_set_parent(struct device *dev, const char *c, const char *p);
int decon_clk_set_rate(struct device *dev, struct clk *clk,
		const char *conid, unsigned long rate);
unsigned long decon_clk_get_rate(struct device *dev, const char *clkid);
void decon_to_psr_info(struct decon_device *decon, struct decon_mode_info *psr);
void decon_to_init_param(struct decon_device *decon, struct decon_param *p);
u32 decon_get_bpp(enum decon_pixel_format fmt);
int decon_get_plane_cnt(enum decon_pixel_format format);

#ifdef CONFIG_FB_DSU
void decon_get_window_rect_log( char* buffer, struct decon_device *decon, struct decon_win_config_data *win_data );
void decon_store_window_rect_log( struct decon_device *decon, struct decon_win_config_data *win_data );
char* decon_last_window_rect_log( void );
void decon_print_bufered_window_rect_log( void );
#endif

#ifdef CONFIG_CHECK_DECON_TIME

enum TIME_TABLE_UPDATE_SEQ {
	TIME_ENTER_UPDATE_TH = 0,
	TIME_FINISH_FENCE_WAIT,
	TIME_ENTER_VPP_SET,
	TIME_FINISH_VPP_SET,
	TIME_FINISH_UPDATE_WAIT,
	TIME_FINISH_UPDATE_TH,
	TIME_TABLE_SEQ_MAX
};

#define UPDATE_DEBUG_BUFFER_MAX	10
#define UPDATE_END_TIME_LIMIT	50
#define UPDATE_MID_TIME_LIMIT	10


struct updatereg_time {
	struct timeval time_table[TIME_TABLE_SEQ_MAX];
	ktime_t total_diff;
	ktime_t mid_diff;
};

struct time_buffer {
	struct updatereg_time time_Q[UPDATE_DEBUG_BUFFER_MAX];
	unsigned int overtime_count;
	ktime_t start_time;
	ktime_t mid_time;		// finish vpp set
	ktime_t end_time;
	ktime_t latest_end_diff;
	ktime_t latest_mid_diff; // finish vpp set diff
	int qIndex;
};

void init_debug_buffer(struct time_buffer* debug_buf);
void set_time_to_buffer(struct time_buffer* debug_buf, enum TIME_TABLE_UPDATE_SEQ update_seq);
int check_diff_time(struct time_buffer* debug_buf);
void show_debug_time(struct time_buffer* debug_buf, struct seq_file *s);
#endif

#endif /* __SAMSUNG_DECON_HELPER_H__ */
