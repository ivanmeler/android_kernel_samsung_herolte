/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Helper file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <asm/cacheflush.h>
#include <asm/page.h>

#include "decon.h"
#include "dsim.h"
#include "./vpp/vpp.h"
#include "decon_helper.h"
#include <video/mipi_display.h>

extern void *return_address(int);

int decon_clk_set_parent(struct device *dev, const char *child, const char *parent)
{
	struct clk *p;
	struct clk *c;

	p = clk_get(dev, parent);
	if (IS_ERR_OR_NULL(p)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, parent);
		return -ENODEV;
	}

	c = clk_get(dev, child);
	if (IS_ERR_OR_NULL(c)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, child);
		return -ENODEV;
	}

	clk_set_parent(c, p);
	clk_put(p);
	clk_put(c);

	return 0;
}

int decon_clk_set_rate(struct device *dev, struct clk *clk,
		const char *conid, unsigned long rate)
{
	if (IS_ERR_OR_NULL(clk)) {
		if (IS_ERR_OR_NULL(conid)) {
			decon_err("%s: couldn't set clock(%ld)\n", __func__, rate);
			return -ENODEV;
		}
		clk = clk_get(dev, conid);
		clk_set_rate(clk, rate);
		clk_put(clk);
	} else {
		clk_set_rate(clk, rate);
	}

	return 0;
}

unsigned long decon_clk_get_rate(struct device *dev, const char *clkid)
{
	struct clk *target;
	unsigned long rate;

	target = clk_get(dev, clkid);
	if (IS_ERR_OR_NULL(target)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, clkid);
		return -ENODEV;
	}

	rate = clk_get_rate(target);
	clk_put(target);

	return rate;
}

void decon_to_psr_info(struct decon_device *decon, struct decon_mode_info *psr)
{
	psr->psr_mode = decon->pdata->psr_mode;
	psr->trig_mode = decon->pdata->trig_mode;
	psr->dsi_mode = decon->pdata->dsi_mode;
	psr->out_type = decon->pdata->out_type;
}

void decon_to_init_param(struct decon_device *decon, struct decon_param *p)
{
	struct decon_lcd *lcd_info = decon->lcd_info;
	struct v4l2_mbus_framefmt mbus_fmt;

	mbus_fmt.width = 0;
	mbus_fmt.height = 0;
	mbus_fmt.code = 0;
	mbus_fmt.field = 0;
	mbus_fmt.colorspace = 0;

	p->lcd_info = lcd_info;
	p->psr.psr_mode = decon->pdata->psr_mode;
	p->psr.trig_mode = decon->pdata->trig_mode;
	p->psr.dsi_mode = decon->pdata->dsi_mode;
	p->psr.out_type = decon->pdata->out_type;
	p->nr_windows = decon->pdata->max_win;
	p->disp_ss_regs = decon->ss_regs;
	decon_dbg("###psr_mode %d trig_mode %d dsi_mode %d out_type %d nr_windows %d LCD[%d %d]\n",
		p->psr.psr_mode, p->psr.trig_mode, p->psr.dsi_mode, p->psr.out_type, p->nr_windows,
		decon->lcd_info->xres, decon->lcd_info->yres);
}

u32 decon_get_bpp(enum decon_pixel_format fmt)
{
	switch (fmt) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 32;

	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_NV16:
	case DECON_PIXEL_FORMAT_NV61:
	case DECON_PIXEL_FORMAT_YVU422_3P:
		return 16;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
	case DECON_PIXEL_FORMAT_YUV420:
	case DECON_PIXEL_FORMAT_YVU420:
	case DECON_PIXEL_FORMAT_YUV420M:
	case DECON_PIXEL_FORMAT_YVU420M:
		return 12;

	default:
		break;
	}

	return 0;
}

int decon_get_plane_cnt(enum decon_pixel_format format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_NV12N:
	case DECON_PIXEL_FORMAT_NV12N_10B:
		return 1;

	case DECON_PIXEL_FORMAT_NV16:
	case DECON_PIXEL_FORMAT_NV61:
	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 2;

	case DECON_PIXEL_FORMAT_YVU422_3P:
	case DECON_PIXEL_FORMAT_YUV420:
	case DECON_PIXEL_FORMAT_YVU420:
	case DECON_PIXEL_FORMAT_YUV420M:
	case DECON_PIXEL_FORMAT_YVU420M:
		return 3;

	default:
		decon_err("invalid format(%d)\n", format);
		return 1;
	}
}

/**
* ----- APIs for DISPLAY_SUBSYSTEM_EVENT_LOG -----
*/
/* ===== STATIC APIs ===== */

#ifdef CONFIG_DECON_EVENT_LOG
/* logging a event related with DECON */
static inline void disp_ss_event_log_decon
	(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];

	if (time.tv64)
		log->time = time;
	else
		log->time = ktime_get();
	log->type = type;

	switch (type) {
	case DISP_EVT_DECON_SUSPEND:
	case DISP_EVT_DECON_RESUME:
	case DISP_EVT_ENTER_LPD:
	case DISP_EVT_EXIT_LPD:
		log->data.pm.pm_status = pm_runtime_active(decon->dev);
		log->data.pm.elapsed = ktime_sub(ktime_get(), log->time);
		break;
	case DISP_EVT_WB_SET_BUFFER:
	case DISP_EVT_WB_SW_TRIGGER:
		break;
	case DISP_EVT_TE_INTERRUPT:
	case DISP_EVT_UNDERRUN:
	case DISP_EVT_LINECNT_ZERO:
		break;
	default:
		/* Any remaining types will be log just time and type */
		break;
	}
}

/* logging a event related with DSIM */
static inline void disp_ss_event_log_dsim
	(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time)
{
	struct dsim_device *dsim = container_of(sd, struct dsim_device, sd);
	struct decon_device *decon = get_decon_drvdata(dsim->id);
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];

	if (time.tv64)
		log->time = time;
	else
		log->time = ktime_get();
	log->type = type;

	switch (type) {
	case DISP_EVT_DSIM_SUSPEND:
	case DISP_EVT_DSIM_RESUME:
	case DISP_EVT_ENTER_ULPS:
	case DISP_EVT_EXIT_ULPS:
		log->data.pm.pm_status = pm_runtime_active(dsim->dev);
		log->data.pm.elapsed = ktime_sub(ktime_get(), log->time);
		break;
	default:
		/* Any remaining types will be log just time and type */
		break;
	}
}

/* get decon's id used by vpp */
static int __get_decon_id_for_vpp(struct v4l2_subdev *sd)
{
	struct decon_device *decon;
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);
	int idx;
	int ret = 0;

	for (idx = 0; idx < NUM_DECON_IPS; idx++) {
		decon = get_decon_drvdata(idx);
		if (!decon || IS_ERR_OR_NULL(decon->debug_event))
			continue;
		if (decon->vpp_used[vpp->id] == true)
			ret = decon->id;
	}

	return ret;
}

/* logging a event related with VPP */
static inline void disp_ss_event_log_vpp
	(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time)
{
	struct decon_device *decon = get_decon_drvdata(__get_decon_id_for_vpp(sd));
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);

	if (time.tv64)
		log->time = time;
	else
		log->time = ktime_get();
	log->type = type;

	switch (type) {
	case DISP_EVT_VPP_SUSPEND:
	case DISP_EVT_VPP_RESUME:
		log->data.pm.pm_status = pm_runtime_active(&vpp->pdev->dev);
		log->data.pm.elapsed = ktime_sub(ktime_get(), log->time);
		break;
	case DISP_EVT_VPP_FRAMEDONE:
	case DISP_EVT_VPP_STOP:
	case DISP_EVT_VPP_WINCON:
		log->data.vpp.id = vpp->id;
		log->data.vpp.start_cnt = vpp->start_count;
		log->data.vpp.done_cnt = vpp->done_count;
		log->data.vpp.width = vpp->config->dst.w;
		log->data.vpp.height = vpp->config->dst.h;
		break;
	default:
		log->data.vpp.id = vpp->id;
		break;
	}

	return;
}

/* If event are happend continuously, then ignore */
static bool disp_ss_event_ignore
	(disp_ss_event_t type, struct decon_device *decon)
{
	int latest = atomic_read(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log;
	int idx;

	/* Seek a oldest from current index */
	idx = (latest + DISP_EVENT_LOG_MAX - DECON_ENTER_LPD_CNT) % DISP_EVENT_LOG_MAX;
	do {
		if (++idx >= DISP_EVENT_LOG_MAX)
			idx = 0;

		log = &decon->disp_ss_log[idx];
		if (log->type != type)
			return false;
	} while (latest != idx);

	return true;
}

/* ===== EXTERN APIs ===== */
/* Common API to log a event related with DECON/DSIM/VPP */
void DISP_SS_EVENT_LOG(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time)
{
	struct decon_device *decon = get_decon_drvdata(0);

	if (!decon || IS_ERR_OR_NULL(decon->debug_event))
		return;

	/* log a eventy softly */
	switch (type) {
	case DISP_EVT_TE_INTERRUPT:
	case DISP_EVT_UNDERRUN:
		/* If occurs continuously, skipped. It is a burden */
		if (disp_ss_event_ignore(type, decon))
			break;
	case DISP_EVT_BLANK:
	case DISP_EVT_UNBLANK:
	case DISP_EVT_ENTER_LPD:
	case DISP_EVT_EXIT_LPD:
	case DISP_EVT_DECON_SUSPEND:
	case DISP_EVT_DECON_RESUME:
	case DISP_EVT_LINECNT_ZERO:
	case DISP_EVT_TRIG_MASK:
	case DISP_EVT_TRIG_UNMASK:
	case DISP_EVT_DECON_FRAMEDONE:
	case DISP_EVT_DECON_FRAMEDONE_WAIT:
	case DISP_EVT_WB_SET_BUFFER:
	case DISP_EVT_WB_SW_TRIGGER:
	case DISP_EVT_DECON_SHUTDOWN:
	case DISP_EVT_RSC_CONFLICT:
		disp_ss_event_log_decon(type, sd, time);
		break;
	case DISP_EVT_DSIM_FRAMEDONE:
	case DISP_EVT_ENTER_ULPS:
	case DISP_EVT_EXIT_ULPS:
	case DISP_EVT_DSIM_SHUTDOWN:
		disp_ss_event_log_dsim(type, sd, time);
		break;
	case DISP_EVT_VPP_FRAMEDONE:
	case DISP_EVT_VPP_STOP:
	case DISP_EVT_VPP_WINCON:
		disp_ss_event_log_vpp(type, sd, time);
		break;
	default:
		break;
	}

	if (decon->disp_ss_log_level == DISP_EVENT_LEVEL_LOW)
		return;

	/* additionally logging hardly */
	switch (type) {
	case DISP_EVT_ACT_VSYNC:
	case DISP_EVT_DEACT_VSYNC:
	case DISP_EVT_WIN_CONFIG:
		disp_ss_event_log_decon(type, sd, time);
		break;
	case DISP_EVT_DSIM_SUSPEND:
	case DISP_EVT_DSIM_RESUME:
		disp_ss_event_log_dsim(type, sd, time);
		break;
	case DISP_EVT_VPP_SUSPEND:
	case DISP_EVT_VPP_RESUME:
	case DISP_EVT_VPP_UPDATE_DONE:
	case DISP_EVT_VPP_SHADOW_UPDATE:
		disp_ss_event_log_vpp(type, sd, time);
	default:
		break;
	}
}
void DISP_SS_EVENT_LOG_WINCON2(struct v4l2_subdev *sd, struct decon_reg_data *regs)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];
	int win = 0;
	bool window_updated = false;

	log->time = ktime_get();
	log->type = DISP_EVT_WIN_CONFIG;

	for (win = 0; win < MAX_DECON_WIN; win++) {
		if (regs->win_regs[win].wincon & WIN_CONTROL_EN_F) {
			memcpy(&log->data.reg.win_regs[win], &regs->win_regs[win],
				sizeof(struct decon_window_regs));
			memcpy(&log->data.reg.win_config[win], &regs->vpp_config[win],
				sizeof(struct decon_win_config));
		} else {
			log->data.reg.win_config[win].state = DECON_WIN_STATE_DISABLED;
		}
	}

	if (decon->pdata->out_type == DECON_OUT_WB)
		memcpy(&log->data.reg.win_config[MAX_DECON_WIN], &regs->vpp_config[MAX_DECON_WIN],
				sizeof(struct decon_win_config));

#ifdef CONFIG_FB_WINDOW_UPDATE
	if ((regs->need_update) ||
		(decon->need_update && regs->update_win.w)) {
		window_updated = true;
		memcpy(&log->data.reg.win, &regs->update_win,
				sizeof(struct decon_rect));
	}
#endif
	if (!window_updated) {
		log->data.reg.win.x = 0;
		log->data.reg.win.y = 0;
		log->data.reg.win.w = decon->lcd_info->xres;
		log->data.reg.win.h = decon->lcd_info->yres;
	}
}

void DISP_SS_EVENT_LOG_WINCON(struct v4l2_subdev *sd, struct decon_reg_data *regs)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];
	int win = 0;
	bool window_updated = false;

	log->time = ktime_get();
	log->type = DISP_EVT_UPDATE_HANDLER;

	for (win = 0; win < MAX_DECON_WIN; win++) {
		if (regs->win_regs[win].wincon & WIN_CONTROL_EN_F) {
			memcpy(&log->data.reg.win_regs[win], &regs->win_regs[win],
				sizeof(struct decon_window_regs));
			memcpy(&log->data.reg.win_config[win], &regs->vpp_config[win],
				sizeof(struct decon_win_config));
		} else {
			log->data.reg.win_config[win].state = DECON_WIN_STATE_DISABLED;
		}
	}

	if (decon->pdata->out_type == DECON_OUT_WB)
		memcpy(&log->data.reg.win_config[MAX_DECON_WIN], &regs->vpp_config[MAX_DECON_WIN],
				sizeof(struct decon_win_config));

#ifdef CONFIG_FB_WINDOW_UPDATE
	if ((regs->need_update) ||
		(decon->need_update && regs->update_win.w)) {
		window_updated = true;
		memcpy(&log->data.reg.win, &regs->update_win,
				sizeof(struct decon_rect));
	}
#endif
	if (!window_updated) {
		log->data.reg.win.x = 0;
		log->data.reg.win.y = 0;
		log->data.reg.win.w = decon->lcd_info->xres;
		log->data.reg.win.h = decon->lcd_info->yres;
	}
}

/* Common API to log a event related with DSIM COMMAND */
void DISP_SS_EVENT_LOG_CMD(struct v4l2_subdev *sd, u32 cmd_id, unsigned long data)
{
	struct dsim_device *dsim = container_of(sd, struct dsim_device, sd);
	struct decon_device *decon = get_decon_drvdata(dsim->id);
	int idx, i;
	struct disp_ss_log *log;

	if (!decon || IS_ERR_OR_NULL(decon->debug_event))
		return;

	idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	log = &decon->disp_ss_log[idx];

	log->time = ktime_get();
	log->type = DISP_EVT_DSIM_COMMAND;
	log->data.cmd_buf.id = cmd_id;
	if (cmd_id == MIPI_DSI_DCS_LONG_WRITE)
		log->data.cmd_buf.buf = *(u8 *)(data);
	else
		log->data.cmd_buf.buf = (u8)data;

	for (i = 0; i < DISP_CALLSTACK_MAX; i++)
		log->data.cmd_buf.caller[i] = (void *)((size_t)return_address(i + 1));
}

/* display logged events related with DECON */
void DISP_SS_EVENT_SHOW(struct seq_file *s, struct decon_device *decon)
{
	int idx = atomic_read(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log;
	int latest = idx;
	struct timeval tv;
	ktime_t prev_ktime;

	/* TITLE */
	seq_printf(s, "-------------------DECON%d EVENT LOGGER ----------------------\n",
			decon->id);
	seq_printf(s, "-- STATUS: LPD(%s) ", IS_ENABLED(CONFIG_DECON_LPD_DISPLAY) ? "on" : "off");
	seq_printf(s, "PKTGO(%s) ", IS_ENABLED(CONFIG_DECON_MIPI_DSI_PKTGO) ? "on" : "off");
	seq_printf(s, "BlockMode(%s) ", IS_ENABLED(CONFIG_DECON_BLOCKING_MODE) ? "on" : "off");
	seq_printf(s, "Window_Update(%s)\n", IS_ENABLED(CONFIG_FB_WINDOW_UPDATE) ? "on" : "off");
	seq_puts(s, "-------------------------------------------------------------\n");
	seq_printf(s, "%14s  %20s  %20s\n",
		"Time", "Event ID", "Remarks");
	seq_puts(s, "-------------------------------------------------------------\n");

	/* return if there is no event log */
	if (idx < 0)
		return;
	/* Seek a oldest from current index */
	idx = (idx + DISP_EVENT_LOG_MAX - DISP_EVENT_PRINT_MAX) % DISP_EVENT_LOG_MAX;
	prev_ktime = ktime_set(0, 0);

	do {
		if (++idx >= DISP_EVENT_LOG_MAX)
			idx = 0;

		/* Seek a index */
		log = &decon->disp_ss_log[idx];

		/* TIME */
		tv = ktime_to_timeval(log->time);
		seq_printf(s, "[%6ld.%06ld] ", tv.tv_sec, tv.tv_usec);

		/* If there is no timestamp, then exit directly */
		if (!tv.tv_sec)
			break;

		/* EVETN ID + Information */
		switch (log->type) {
		case DISP_EVT_BLANK:
			seq_printf(s, "%20s  %20s", "FB_BLANK", "-\n");
			break;
		case DISP_EVT_UNBLANK:
			seq_printf(s, "%20s  %20s", "FB_UNBLANK", "-\n");
			break;
		case DISP_EVT_ACT_VSYNC:
			seq_printf(s, "%20s  %20s", "ACT_VSYNC", "-\n");
			break;
		case DISP_EVT_DEACT_VSYNC:
			seq_printf(s, "%20s  %20s", "DEACT_VSYNC", "-\n");
			break;
		case DISP_EVT_WIN_CONFIG:
			seq_printf(s, "%20s  %20s", "WIN_CONFIG", "-\n");
			break;
		case DISP_EVT_TE_INTERRUPT:
			prev_ktime = ktime_sub(log->time, prev_ktime);
			seq_printf(s, "%20s  ", "TE_INTERRUPT");
			seq_printf(s, "time_diff=[%ld.%04lds]\n",
					ktime_to_timeval(prev_ktime).tv_sec,
					ktime_to_timeval(prev_ktime).tv_usec/100);
			/* Update for latest DISP_EVT_TE time */
			prev_ktime = log->time;
			break;
		case DISP_EVT_UNDERRUN:
			seq_printf(s, "%20s  %20s", "UNDER_RUN", "-\n");
			break;
		case DISP_EVT_DECON_FRAMEDONE:
			seq_printf(s, "%20s  %20s", "DECON_FRAME_DONE", "-\n");
			break;
		case DISP_EVT_UPDATE_HANDLER:
			seq_printf(s, "%20s  ", "UPDATE_HANDLER");
			seq_printf(s, "Partial Size (%d,%d,%d,%d)\n",
					log->data.reg.win.x,
					log->data.reg.win.y,
					log->data.reg.win.w,
					log->data.reg.win.h);
			break;
		case DISP_EVT_DSIM_COMMAND:
			seq_printf(s, "%20s  ", "DSIM_COMMAND");
			seq_printf(s, "id=0x%x, command=0x%x\n",
					log->data.cmd_buf.id,
					log->data.cmd_buf.buf);
			break;
		case DISP_EVT_TRIG_MASK:
			seq_printf(s, "%20s  %20s", "TRIG_MASK", "-\n");
			break;
		case DISP_EVT_TRIG_UNMASK:
			seq_printf(s, "%20s  %20s", "TRIG_UNMASK", "-\n");
			break;
		case DISP_EVT_VPP_WINCON:
			seq_printf(s, "%20s  ", "VPP_WINCON");
			seq_printf(s, "ID:%d, start= %d, done= %d\n",
					log->data.vpp.id,
					log->data.vpp.start_cnt,
					log->data.vpp.done_cnt);
			break;
		case DISP_EVT_VPP_FRAMEDONE:
			seq_printf(s, "%20s  ", "VPP_FRAMEDONE");
			seq_printf(s, "ID:%d, start=%d, done=%d\n",
					log->data.vpp.id,
					log->data.vpp.start_cnt,
					log->data.vpp.done_cnt);
			break;
		case DISP_EVT_VPP_STOP:
			seq_printf(s, "%20s  ", "VPP_STOP");
			seq_printf(s, "(id:%d)\n", log->data.vpp.id);
			break;
		case DISP_EVT_VPP_SUSPEND:
			seq_printf(s, "%20s  %20s", "VPP_SUSPEND", "-\n");
			break;
		case DISP_EVT_VPP_RESUME:
			seq_printf(s, "%20s  %20s", "VPP_RESUME", "-\n");
			break;
		case DISP_EVT_DECON_SUSPEND:
			seq_printf(s, "%20s  %20s", "DECON_SUSPEND", "-\n");
			break;
		case DISP_EVT_DECON_RESUME:
			seq_printf(s, "%20s  %20s", "DECON_RESUME", "-\n");
			break;
		case DISP_EVT_ENTER_LPD:
			seq_printf(s, "%20s  ", "ENTER_LPD");
			tv = ktime_to_timeval(log->data.pm.elapsed);
			seq_printf(s, "pm=%s, elapsed=[%ld.%03lds]\n",
					log->data.pm.pm_status ? "active " : "suspend",
					tv.tv_sec, tv.tv_usec/1000);
			break;
		case DISP_EVT_EXIT_LPD:
			seq_printf(s, "%20s  ", "EXIT_LPD");
			tv = ktime_to_timeval(log->data.pm.elapsed);
			seq_printf(s, "pm=%s, elapsed=[%ld.%03lds]\n",
					log->data.pm.pm_status ? "active " : "suspend",
					tv.tv_sec, tv.tv_usec/1000);
			break;
		case DISP_EVT_DSIM_SUSPEND:
			seq_printf(s, "%20s  %20s", "DSIM_SUSPEND", "-\n");
			break;
		case DISP_EVT_DSIM_RESUME:
			seq_printf(s, "%20s  %20s", "DSIM_RESUME", "-\n");
			break;
		case DISP_EVT_ENTER_ULPS:
			seq_printf(s, "%20s  ", "ENTER_ULPS");
			tv = ktime_to_timeval(log->data.pm.elapsed);
			seq_printf(s, "pm=%s, elapsed=[%ld.%03lds]\n",
					log->data.pm.pm_status ? "active " : "suspend",
					tv.tv_sec, tv.tv_usec/1000);
			break;
		case DISP_EVT_EXIT_ULPS:
			seq_printf(s, "%20s  ", "EXIT_ULPS");
			tv = ktime_to_timeval(log->data.pm.elapsed);
			seq_printf(s, "pm=%s, elapsed=[%ld.%03lds]\n",
					log->data.pm.pm_status ? "active " : "suspend",
					tv.tv_sec, tv.tv_usec/1000);
			break;
		default:
			seq_printf(s, "%20s  (%2d)\n", "NO_DEFINED", log->type);
			break;
		}
	} while (latest != idx);

	seq_puts(s, "-------------------------------------------------------------\n");

	return;
}

void DISP_SS_EVENT_SIZE_ERR_LOG(struct v4l2_subdev *sd, struct disp_ss_size_info *info)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	int idx = (decon->disp_ss_size_log_idx++) % DISP_EVENT_SIZE_ERR_MAX;
	struct disp_ss_size_err_info *log = &decon->disp_ss_size_log[idx];

	if (!decon)
		return;

	log->time = ktime_get();
	memcpy(&log->info, info, sizeof(struct disp_ss_size_info));
}
#endif
