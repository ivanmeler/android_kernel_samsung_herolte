/* dsim_common.h
 *
 * Header file for Samsung MIPI-DSI lowlevel driver.
 *
 * Copyright (c) 2014 Samsung Electronics
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _DSIM_COMMON_H_
#define _DSIM_COMMON_H_

#include "./panels/decon_lcd.h"

#define DSIM_PIXEL_FORMAT_RGB24		0x7
#define DSIM_PIXEL_FORMAT_RGB18		0x6
#define DSIM_PIXEL_FORMAT_RGB18_PACKED	0x5
#define DSIM_RX_FIFO_MAX_DEPTH		64

/* define DSI lane types. */
enum {
	DSIM_LANE_CLOCK	= (1 << 0),
	DSIM_LANE_DATA0	= (1 << 1),
	DSIM_LANE_DATA1	= (1 << 2),
	DSIM_LANE_DATA2	= (1 << 3),
	DSIM_LANE_DATA3	= (1 << 4),
};

/* DSI Error report bit definitions */
enum {
	MIPI_DSI_ERR_SOT			= (1 << 0),
	MIPI_DSI_ERR_SOT_SYNC			= (1 << 1),
	MIPI_DSI_ERR_EOT_SYNC			= (1 << 2),
	MIPI_DSI_ERR_ESCAPE_MODE_ENTRY_CMD	= (1 << 3),
	MIPI_DSI_ERR_LOW_POWER_TRANSMIT_SYNC	= (1 << 4),
	MIPI_DSI_ERR_HS_RECEIVE_TIMEOUT		= (1 << 5),
	MIPI_DSI_ERR_FALSE_CONTROL		= (1 << 6),
	/* Bit 7 is reserved */
	MIPI_DSI_ERR_ECC_SINGLE_BIT		= (1 << 8),
	MIPI_DSI_ERR_ECC_MULTI_BIT		= (1 << 9),
	MIPI_DSI_ERR_CHECKSUM			= (1 << 10),
	MIPI_DSI_ERR_DATA_TYPE_NOT_RECOGNIZED	= (1 << 11),
	MIPI_DSI_ERR_VCHANNEL_ID_INVALID	= (1 << 12),
	MIPI_DSI_ERR_INVALID_TRANSMIT_LENGTH	= (1 << 13),
	/* Bit 14 is reserved */
	MIPI_DSI_ERR_PROTOCAL_VIOLATION		= (1 << 15),
	/* DSI_PROTOCAL_VIOLATION[15] is for protocol violation that is caused EoTp
	 * missing So this bit is egnored because of not supportung @S.LSI AP */
	/* FALSE_ERROR_CONTROL[6] is for detect invalid escape or turnaround sequence.
	 * This bit is not supporting @S.LSI AP because of non standard
	 * ULPS enter/exit sequence during power-gating */
	/* Bit [14],[7] is reserved */
	MIPI_DSI_ERR_BIT_MASK			= (0x3f3f), /* Error_Range[13:0] */
};

struct dsim_pll_param {
	u32 p;
	u32 m;
	u32 s;
	u32 pll_freq; /* in/out parameter: Mhz */
};

struct dsim_clks {
	u32 hs_clk;
	u32 esc_clk;
	u32 byte_clk;
};

struct dphy_timing_value {
	u32 bps;
	u32 clk_prepare;
	u32 clk_zero;
	u32 clk_post;
	u32 clk_trail;
	u32 hs_prepare;
	u32 hs_zero;
	u32 hs_trail;
	u32 lpx;
	u32 hs_exit;
	u32 b_dphyctl;
};

struct dsim_clks_param {
	struct dsim_clks clks;
	struct dsim_pll_param pll;
	struct dphy_timing_value t;

	u32 esc_div;
};

/* CAL APIs list */
int dsim_reg_init(u32 id, struct decon_lcd *lcd_info,
			u32 data_lane_cnt, struct dsim_clks *clks);
void dsim_reg_init_probe(u32 id, struct decon_lcd *lcd_info,
			u32 data_lane_cnt, struct dsim_clks *clks);
int dsim_reg_set_clocks(u32 id, struct dsim_clks *clks, struct stdphy_pms *dphy_pms, u32 en);
int dsim_reg_set_lanes(u32 id, u32 lanes, u32 en);
int dsim_reg_set_hs_clock(u32 id, u32 en);
void dsim_reg_set_int(u32 id, u32 en);
int dsim_reg_set_ulps(u32 id, u32 en, u32 lanes);
int dsim_reg_set_smddi_ulps(u32 id, u32 en, u32 lanes);
/* RX related APIs list */
u32 dsim_reg_rx_fifo_is_empty(u32 id);
u32 dsim_reg_get_rx_fifo(u32 id);
int dsim_reg_rx_err_handler(u32 id, u32 rx_fifo);

/* CAL raw functions list */
void dsim_reg_set_phy_otp_config(u32 id);
void dsim_reg_sw_reset(u32 id);
void dsim_reg_dphy_reset(u32 id);
void dsim_reg_funtion_reset(u32 id);
void dsim_reg_dp_dn_swap(u32 id, u32 en);
void dsim_reg_set_num_of_lane(u32 id, u32 lane);
void dsim_reg_enable_lane(u32 id, u32 lane, u32 en);
void dsim_reg_set_pll_freq(u32 id, u32 p, u32 m, u32 s);
void dsim_reg_pll_stable_time(u32 id);
void dsim_reg_set_dphy_timing_values(u32 id, struct dphy_timing_value *t);
void dsim_reg_clear_int(u32 id, u32 int_src);
void dsim_reg_clear_int_all(u32 id);
void dsim_reg_set_pll(u32 id, u32 en);
u32 dsim_reg_is_pll_stable(u32 id);
int dsim_reg_enable_pll(u32 id, u32 en);
void dsim_reg_set_byte_clock(u32 id, u32 en);
void dsim_reg_set_esc_clk_prescaler(u32 id, u32 en, u32 p);
void dsim_reg_set_esc_clk_on_lane(u32 id, u32 en, u32 lane);
u32 dsim_reg_wait_lane_stop_state(u32 id);
void dsim_reg_set_stop_state_cnt(u32 id);
void dsim_reg_set_bta_timeout(u32 id);
void dsim_reg_set_lpdr_timeout(u32 id);
void dsim_reg_set_porch(u32 id, struct decon_lcd *lcd);
void dsim_reg_set_pixel_format(u32 id, u32 pixformat);
void dsim_reg_set_config(u32 id, struct decon_lcd *lcd_info, u32 data_lane_cnt);
void dsim_reg_set_cmd_transfer_mode(u32 id, u32 lp);
void dsim_reg_set_multipix(u32 id, u32 multipix);
void dsim_reg_set_vc_id(u32 id, u32 vcid);
void dsim_reg_set_video_mode(u32 id, u32 mode);
void dsim_reg_enable_dsc(u32 id, u32 en);
void dsim_reg_disable_hsa(u32 id, u32 en);
void dsim_reg_disable_hbp(u32 id, u32 en);
void dsim_reg_disable_hfp(u32 id, u32 en);
void dsim_reg_disable_hse(u32 id, u32 en);
void dsim_reg_set_hsync_preserve(u32 id, u32 en);
void dsim_reg_set_burst_mode(u32 id, u32 burst);
void dsim_reg_set_sync_inform(u32 id, u32 inform);
void dsim_reg_set_cmdallow(u32 id, u32 cmdallow);
void dsim_reg_set_stable_vfp(u32 id, u32 stablevfp);
void dsim_reg_set_vbp(u32 id, u32 vbp);
void dsim_reg_set_hfp(u32 id, u32 hfp);
void dsim_reg_set_hbp(u32 id, u32 hbp);
void dsim_reg_set_vsa(u32 id, u32 vsa);
void dsim_reg_set_hsa(u32 id, u32 hsa);
void dsim_reg_set_vresol(u32 id, u32 vresol);
void dsim_reg_set_hresol(u32 id, u32 hresol, struct decon_lcd *lcd);
void dsim_reg_set_multi_packet_count(u32 id, u32 multipacketcnt);
void dsim_reg_set_command_control(u32 id, u32 cmdcontrol);
void dsim_reg_set_time_stable_vfp(u32 id, u32 stablevfp);
void dsim_reg_set_time_vsync_timeout(u32 id, u32 vsynctout);
void dsim_reg_set_time_te_protect_on(u32 id, u32 teprotecton);
void dsim_reg_set_time_te_timeout(u32 id, u32 tetout);
void dsim_reg_set_hsync_timeout(u32 id, u32 hsynctout);
void dsim_reg_enable_mflush(u32 id, u32 en);
void dsim_reg_enable_noncontinuous_clock(u32 id, u32 en);
void dsim_reg_enable_clocklane_stop_start(u32 id, u32 en);
void dsim_reg_enable_packetgo(u32 id, u32 en);
void dsim_reg_set_packetgo_ready(u32 id);
void dsim_reg_enable_multi_command_packet(u32 id, u32 en);
void dsim_reg_enable_shadow(u32 id, u32 en);
void dsim_reg_enable_hs_clock(u32 id, u32 en);
void dsim_reg_enable_byte_clock(u32 id, u32 en);
u32 dsim_reg_is_hs_clk_ready(u32 id);
void dsim_reg_enable_per_frame_read(u32 id, u32 en);
void dsim_reg_enable_qchannel(u32 id, u32 en);
int dsim_reg_wait_hs_clk_ready(u32 id);
u32 dsim_reg_is_writable_fifo_state(u32 id);
u32 dsim_reg_header_fifo_is_empty(u32 id);
void dsim_reg_set_fifo_ctrl(u32 id, u32 cfg);
void dsim_reg_force_dphy_stop_state(u32 id, u32 en);
void dsim_reg_wr_tx_header(u32 id, u32 data_id, unsigned long data0, u32 data1);
void dsim_reg_wr_tx_payload(u32 id, u32 payload);
void dsim_reg_enter_ulps(u32 id, u32 enter);
void dsim_reg_exit_ulps(u32 id, u32 exit);
int dsim_reg_set_ulps_by_ddi(u32 id, u32 ddi_type, u32 lanes, u32 en);
int dsim_reg_wait_enter_ulps_state(u32 id, u32 lanes);
int dsim_reg_wait_exit_ulps_state(u32 id);
void dsim_reg_set_standby(u32 id, u32 en);
void dsim_reg_set_bist(u32 id, u32 en, u32 vfp, u32 format, u32 type);
void dsim_reg_set_packet_ctrl(u32 id);
void dsim_reg_enable_loopback(u32 id, u32 en);
void dsim_reg_set_loopback_id(u32 id, u32 en);
void dsim_reg_set_pkt_go_enable(u32 id, bool en);
void dsim_reg_set_pkt_go_ready(u32 id);
void dsim_reg_set_pkt_go_cnt(u32 id, unsigned int count);
void dsim_reg_set_shadow(u32 id, u32 en);
void dsim_reg_shadow_update(u32 id);
int dsim_reg_exit_ulps_and_start(u32 id, u32 ddi_type, u32 lanes);
int dsim_reg_stop_and_enter_ulps(u32 id, u32 ddi_type, u32 lanes);
void dsim_reg_start(u32 id, struct dsim_clks *clks, u32 lanes);
void dsim_reg_stop(u32 id, u32 lanes);
void dsim_reg_set_partial_update(u32 id, struct decon_lcd *lcd_info);

#endif /* _DSIM_COMMON_H_ */
