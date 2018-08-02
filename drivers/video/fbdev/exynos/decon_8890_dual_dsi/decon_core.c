/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/console.h>
#include <linux/dma-buf.h>
#include <linux/exynos_ion.h>
#include <linux/ion.h>
#include <linux/irq.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/exynos_iovmm.h>
#include <linux/bug.h>
#include <linux/of_address.h>
#include <linux/smc.h>
#include <linux/debugfs.h>
#include <linux/of_gpio.h>

#include <media/exynos_mc.h>
#include <video/mipi_display.h>
#include <media/v4l2-subdev.h>

#include "decon.h"
#include "dsim.h"
#include "decon_helper.h"
#include "../../../../staging/android/sw_sync.h"
#include "vpp/vpp.h"
#include "../../../../soc/samsung/pwrcal/pwrcal.h"
#include "../../../../soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h"
#include "../../../../kernel/irq/internals.h"

#ifdef CONFIG_MACH_VELOCE8890
#define DISP_SYSMMU_DIS
#define DISP_PWR_CLK_CTRL_DIS
#endif

#define SUCCESS_EXYNOS_SMC	2

#ifdef CONFIG_OF
static const struct of_device_id decon_device_table[] = {
	{ .compatible = "samsung,exynos8-decon_driver" },
	{},
};
MODULE_DEVICE_TABLE(of, decon_device_table);
#endif

int decon_log_level = 6;
module_param(decon_log_level, int, 0644);

struct decon_device *decon_f_drvdata;
EXPORT_SYMBOL(decon_f_drvdata);
struct decon_device *decon_s_drvdata;
EXPORT_SYMBOL(decon_s_drvdata);
struct decon_device *decon_t_drvdata;
EXPORT_SYMBOL(decon_t_drvdata);

static int decon_runtime_resume(struct device *dev);
static int decon_runtime_suspend(struct device *dev);
static void decon_set_protected_content(struct decon_device *decon,
		struct decon_reg_data *reg);

#define SYSTRACE_C_BEGIN(a) do { \
	decon->tracing_mark_write( decon->systrace_pid, 'C', a, 1 );	\
	} while(0)

#define SYSTRACE_C_FINISH(a) do { \
	decon->tracing_mark_write( decon->systrace_pid, 'C', a, 0 );	\
	} while(0)

#define SYSTRACE_C_MARK(a,b) do { \
	decon->tracing_mark_write( decon->systrace_pid, 'C', a, (b) );	\
	} while(0)


/*----------------- function for systrace ---------------------------------*/
/* history (1): 15.11.10
* to make stamp in systrace, we can use trace_printk()/trace_puts().
* but, when we tested them, this function-name is inserted in front of all systrace-string.
* it make disable to recognize by systrace.
* example log : decon0-1831  ( 1831) [001] ....   681.732603: decon_update_regs: tracing_mark_write: B|1831|decon_fence_wait
* systrace error : /sys/kernel/debug/tracing/trace_marker: Bad file descriptor (9)
* solution : make function-name to 'tracing_mark_write'
*
* history (2): 15.11.10
* if we make argument to current-pid, systrace-log will be duplicated in Surfaceflinger as systrace-error.
* example : EventControl-3184  ( 3066) [001] ...1    53.870105: tracing_mark_write: B|3066|eventControl\n\
*           EventControl-3184  ( 3066) [001] ...1    53.870120: tracing_mark_write: B|3066|eventControl\n\
*           EventControl-3184  ( 3066) [001] ....    53.870164: tracing_mark_write: B|3184|decon_DEactivate_vsync_0\n\
* solution : store decon0's pid to static-variable.
*
* history (3) : 15.11.11
* all code is registred in decon srtucture.
*/

static void tracing_mark_write( int pid, char id, char* str1, int value )
{
	char buf[80];

	if(!pid) return;
	switch( id ) {
	case 'B':
		sprintf( buf, "B|%d|%s", pid, str1 );
		break;
	case 'E':
		strcpy( buf, "E" );
		break;
	case 'C':
		sprintf( buf, "C|%d|%s|%d", pid, str1, value );
		break;
	default:
		decon_err( "%s:argument fail\n", __func__ );
		return;
	}

	trace_puts(buf);
}
/*-----------------------------------------------------------------*/

static void vpp_dump(struct decon_device *decon);
#ifdef CONFIG_USE_VSYNC_SKIP
static atomic_t extra_vsync_wait;
#endif /* CCONFIG_USE_VSYNC_SKIP */

void decon_dump(struct decon_device *decon)
{
	int acquired = console_trylock();

	decon_info("\n=== DECON%d SFR DUMP ===\n", decon->id);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->regs, 0x2AC, false);

	if (decon->lcd_info->mic_enabled) {
		decon_info("\n=== DECON%d MIC SFR DUMP ===\n", decon->id);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
				decon->regs + 0x1000, 0x20, false);
	} else if (decon->lcd_info->dsc_enabled) {
		decon_info("\n=== DECON%d DSC0 SFR DUMP ===\n", decon->id);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
				decon->regs + 0x2000, 0xA0, false);

		decon_info("\n=== DECON%d DSC1 SFR DUMP ===\n", decon->id);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
				decon->regs + 0x3000, 0xA0, false);
	}

	if (decon->pdata->dsi_mode != DSI_MODE_SINGLE) {
		decon_info("\n=== DECON%d DISPIF2 SFR DUMP ===\n", decon->id);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
				decon->regs + 0x5080, 0x80, false);

		decon_info("\n=== DECON%d DISPIF3 SFR DUMP ===\n", decon->id);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
				decon->regs + 0x6080, 0x80, false);
	}

	decon_info("\n=== DECON%d SHADOW SFR DUMP ===\n", decon->id);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->regs + SHADOW_OFFSET, 0x2AC, false);

	v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_DUMP, NULL);
	vpp_dump(decon);

	if (acquired)
		console_unlock();
}

/* ---------- CHECK FUNCTIONS ----------- */
static void decon_win_conig_to_regs_param
	(int transp_length, struct decon_win_config *win_config,
	 struct decon_window_regs *win_regs, enum decon_idma_type idma_type)
{
	u8 alpha0, alpha1;

	if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
		alpha0 = win_config->plane_alpha;
		alpha1 = 0;
		if (!transp_length && (win_config->blending == DECON_BLENDING_PREMULT))
			win_config->blending = DECON_BLENDING_COVERAGE;
	} else {
		alpha0 = 0xFF;
		alpha1 = 0xff;
	}

	win_regs->wincon = wincon(transp_length, alpha0, alpha1,
			win_config->plane_alpha, win_config->blending);
	win_regs->start_pos = win_start_pos(win_config->dst.x, win_config->dst.y);
	win_regs->end_pos = win_end_pos(win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);
	win_regs->pixel_count = (win_config->dst.w * win_config->dst.h);
	win_regs->whole_w = win_config->dst.f_w;
	win_regs->whole_h = win_config->dst.f_h;
	win_regs->offset_x = win_config->dst.x;
	win_regs->offset_y = win_config->dst.y;
	win_regs->type = idma_type;

	decon_dbg("DMATYPE_%d@ SRC:(%d,%d) %dx%d  DST:(%d,%d) %dx%d\n",
			idma_type,
			win_config->src.x, win_config->src.y,
			win_config->src.f_w, win_config->src.f_h,
			win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);
}

static u16 fb_panstep(u32 res, u32 res_virtual)
{
	return res_virtual > res ? 1 : 0;
}

u32 wincon(u32 transp_len, u32 a0, u32 a1,
	int plane_alpha, enum decon_blending blending)
{
	u32 data = 0;
	int is_plane_alpha = (plane_alpha < 255 && plane_alpha > 0) ? 1 : 0;

	if (transp_len == 1 && blending == DECON_BLENDING_PREMULT)
		blending = DECON_BLENDING_COVERAGE;

	if (is_plane_alpha) {
		if (transp_len) {
			if (blending != DECON_BLENDING_NONE)
				data |= WIN_CONTROL_ALPHA_MUL_F;
		} else {
			if (blending == DECON_BLENDING_PREMULT)
				blending = DECON_BLENDING_COVERAGE;
		}
	}

	if (transp_len > 1)
		data |= WIN_CONTROL_ALPHA_SEL_F(W_ALPHA_SEL_F_BYAEN);

	switch (blending) {
	case DECON_BLENDING_NONE:
		data |= WIN_CONTROL_FUNC_F(PD_FUNC_COPY);
		break;

	case DECON_BLENDING_PREMULT:
		if (!is_plane_alpha) {
			data |= WIN_CONTROL_FUNC_F(PD_FUNC_SOURCE_OVER);
		} else {
			/* need to check the eq: it is SPEC-OUT */
			data |= WIN_CONTROL_FUNC_F(PD_FUNC_LEGACY2);
		}
		break;

	case DECON_BLENDING_COVERAGE:
	default:
		data |= WIN_CONTROL_FUNC_F(PD_FUNC_LEGACY);
		break;
	}

	data |= WIN_CONTROL_ALPHA0_F(a0) | WIN_CONTROL_ALPHA1_F(a1);

	return data;
}

#ifdef CONFIG_USE_VSYNC_SKIP
void decon_extra_vsync_wait_set(int set_count)
{
	atomic_set(&extra_vsync_wait, set_count);
}

int decon_extra_vsync_wait_get(void)
{
	return atomic_read(&extra_vsync_wait);
}

void decon_extra_vsync_wait_add(int skip_count)
{
	atomic_add(skip_count, &extra_vsync_wait);
}
#endif /* CONFIG_USE_VSYNC_SKIP */


static inline bool is_rgb32(int format)
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
		return true;
	default:
		return false;
	}
}

static u32 decon_red_length(int format)
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
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 5;

	default:
		return 0;
	}
}

static u32 decon_red_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 0;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 11;

	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 16;

	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 24;

	default:
		return 0;
	}
}

static u32 decon_green_length(int format)
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
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 6;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_green_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 16;

	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
		return 5;
	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_blue_length(int format)
{
	return decon_red_length(format);
}

static u32 decon_blue_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
		return 16;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 10;

	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 8;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return 24;

	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_transp_length(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 1;

	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 0;

	default:
		return 0;
	}
}

static u32 decon_transp_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 24;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 15;

	case DECON_PIXEL_FORMAT_RGBX_8888:
		return decon_blue_offset(format);

	case DECON_PIXEL_FORMAT_BGRX_8888:
		return decon_red_offset(format);

	case DECON_PIXEL_FORMAT_RGB_565:
		return 0;

	default:
		return 0;
	}
}

static u32 decon_padding(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}

}

bool decon_validate_x_alignment(struct decon_device *decon, int x, u32 w,
		u32 bits_per_pixel)
{
	uint8_t pixel_alignment = 32 / bits_per_pixel;

	if (x % pixel_alignment) {
		decon_err("left X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u)\n",
				pixel_alignment, bits_per_pixel, x);
		return 0;
	}
	if ((x + w) % pixel_alignment) {
		decon_err("right X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u, w = %u)\n",
				pixel_alignment, bits_per_pixel, x, w);
		return 0;
	}

	return 1;
}

static unsigned int decon_calc_bandwidth(u32 w, u32 h, u32 bits_per_pixel, int fps)
{
	unsigned int bw = w * h;

	bw *= DIV_ROUND_UP(bits_per_pixel, 8);
	bw *= fps;

	return bw;
}

/* ---------- OVERLAP COUNT CALCULATION ----------- */
static inline int rect_width(struct decon_rect *r)
{
	return	r->right - r->left;
}

static inline int rect_height(struct decon_rect *r)
{
	return	r->bottom - r->top;
}

static bool decon_intersect(struct decon_rect *r1, struct decon_rect *r2)
{
	return !(r1->left > r2->right || r1->right < r2->left ||
		r1->top > r2->bottom || r1->bottom < r2->top);
}

static int decon_intersection(struct decon_rect *r1,
			struct decon_rect *r2, struct decon_rect *r3)
{
	r3->top = max(r1->top, r2->top);
	r3->bottom = min(r1->bottom, r2->bottom);
	r3->left = max(r1->left, r2->left);
	r3->right = min(r1->right, r2->right);
	return 0;
}

static bool is_decon_rect_differ(struct decon_rect *r1,
		struct decon_rect *r2)
{
	return ((r1->left != r2->left) || (r1->top != r2->top) ||
		(r1->right != r2->right) || (r1->bottom != r2->bottom));
}

static inline bool does_layer_need_scale(struct decon_win_config *config)
{
	return (config->dst.w != config->src.w) || (config->dst.h != config->src.h);
}

#if defined(CONFIG_FB_WINDOW_UPDATE)
static int decon_union(struct decon_rect *r1,
		struct decon_rect *r2, struct decon_rect *r3)
{
	r3->top = min(r1->top, r2->top);
	r3->bottom = max(r1->bottom, r2->bottom);
	r3->left = min(r1->left, r2->left);
	r3->right = max(r1->right, r2->right);
	return 0;
}
#endif

static inline bool is_decon_opaque_format(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
		return false;

	default:
		return true;
	}
}

static int decon_set_win_blocking_mode(struct decon_device *decon, struct decon_win *win,
		struct decon_win_config *win_config, struct decon_reg_data *regs)
{
	struct decon_rect r1, r2, overlap_rect, block_rect;
	unsigned int overlap_size, blocking_size = 0;
	struct decon_win_config *config = &win_config[win->index];
	struct decon_lcd *lcd_info = decon->lcd_info;
	int j;
	bool enabled = false;

	if (config->state != DECON_WIN_STATE_BUFFER)
		return 0;

	if (config->compression)
		return 0;

	/* Blocking mode is supported for only RGB32 color formats */
	if (!is_rgb32(config->format))
		return 0;

	/* Blocking Mode is not supported if there is a rotation */
	if ((is_rotation(config)) || does_layer_need_scale(config))
		return 0;

	r1.left = config->dst.x;
	r1.top = config->dst.y;
	r1.right = r1.left + config->dst.w - 1;
	r1.bottom = r1.top + config->dst.h - 1;

	/* Calculate the window Blocking region from the top windows opaque rect */
	memset(&block_rect, 0, sizeof(struct decon_rect));
	for (j = win->index + 1; j < decon->pdata->max_win; j++) {
		config = &win_config[j];
		if (config->state != DECON_WIN_STATE_BUFFER)
			continue;

		/* If top window needs plane alpha, bottom window can't use blocking mode */
		if ((config->plane_alpha < 255) && (config->plane_alpha > 0))
			continue;

		if (is_decon_opaque_format(config->format)) {
			/* it is just a safety code */
			config->opaque_area.x = config->dst.x;
			config->opaque_area.y = config->dst.y;
			config->opaque_area.w = config->dst.w;
			config->opaque_area.h = config->dst.h;
		}

		if (!(config->opaque_area.w && config->opaque_area.h))
			continue;

		r2.left = config->opaque_area.x;
		r2.top = config->opaque_area.y;
		r2.right = r2.left + config->opaque_area.w - 1;
		r2.bottom = r2.top + config->opaque_area.h - 1;
		/* overlaps or not */
		if (decon_intersect(&r1, &r2)) {
			decon_intersection(&r1, &r2, &overlap_rect);
			if (!is_decon_rect_differ(&r1, &overlap_rect)) {
				/* window rect and blocking rect is same. */
				win_config[win->index].state = DECON_WIN_STATE_DISABLED;
				return 1;
			}

			if (overlap_rect.right - overlap_rect.left + 1 <
					MIN_BLK_MODE_WIDTH ||
				overlap_rect.bottom - overlap_rect.top + 1 <
					MIN_BLK_MODE_HEIGHT)
				continue;

			overlap_size = (overlap_rect.right - overlap_rect.left) *
					(overlap_rect.bottom - overlap_rect.top);

			if (overlap_size > blocking_size) {
				memcpy(&block_rect, &overlap_rect,
						sizeof(struct decon_rect));
				blocking_size = (block_rect.right - block_rect.left) *
						(block_rect.bottom - block_rect.top);
				enabled = true;
			}
		}
	}

	config = &win_config[win->index];
	/* Transparent area means alpha = 0. The region can be used as blocking area. */
        if (config->transparent_area.w && config->transparent_area.h &&
		!decon->need_update && ((config->transparent_area.w * config->transparent_area.h * 100) >=
		(lcd_info->xres * lcd_info->yres * 15))) {
		/* If the transparent region is smaller than 15% of LCD, try to use CRC mode */
                r2.left = config->transparent_area.x;
                r2.top = config->transparent_area.y;
                r2.right = r2.left + config->transparent_area.w - 1;
                r2.bottom = r2.top + config->transparent_area.h - 1;
                if (decon_intersect(&r1, &r2)) {
                        decon_intersection(&r1, &r2, &overlap_rect);
                        /* choose the bigger region between the two possible blocking areas */
                        if ((rect_width(&block_rect) * rect_height(&block_rect)) <
                                        (rect_width(&overlap_rect) * rect_height(&overlap_rect))) {
                                if (rect_width(&overlap_rect) < MIN_BLK_MODE_WIDTH - 1 ||
                                                rect_height(&overlap_rect) < MIN_BLK_MODE_HEIGHT - 1)
                                        return 0;
                                memcpy(&block_rect, &overlap_rect, sizeof(struct decon_rect));
                                if (!is_decon_rect_differ(&r1, &block_rect)) {
                                        /* window rect and blocking rect is same. */
                                        win_config[win->index].state = DECON_WIN_STATE_DISABLED;
                                        return 1;
                                }
                                enabled = true;
                        }
                }
        }

	if (enabled) {
		regs->block_rect[win->index].w = block_rect.right - block_rect.left + 1;
		regs->block_rect[win->index].h = block_rect.bottom - block_rect.top + 1;
		regs->block_rect[win->index].x = block_rect.left - config->dst.x;
		regs->block_rect[win->index].y = block_rect.top -  config->dst.y;
		decon_dbg("win-%d: block_rect[%d %d %d %d]\n", win->index, regs->block_rect[win->index].x,
			regs->block_rect[win->index].y, regs->block_rect[win->index].w,
			regs->block_rect[win->index].h);
		memcpy(&config->block_area, &regs->block_rect[win->index],
				sizeof(struct decon_win_rect));
	}
	return 0;
}

static void vpp_dump(struct decon_device *decon)
{
	int i;

	for (i = 0; i < MAX_VPP_SUBDEV; i++) {
		if (decon->vpp_used[i]) {
			struct v4l2_subdev *sd = NULL;
			sd = decon->mdev->vpp_sd[i];
			BUG_ON(!sd);
			v4l2_subdev_call(sd, core, ioctl, VPP_DUMP, NULL);
		}
	}
}

static void decon_vpp_stop(struct decon_device *decon, bool do_reset)
{
	int i;
	struct vpp_dev *vpp;
	unsigned long state = (unsigned long)do_reset;

	for (i = 0; i < MAX_VPP_SUBDEV; i++) {
		if (decon->vpp_used[i] && (!(decon->vpp_usage_bitmask & (1 << i)))) {
			struct v4l2_subdev *sd = decon->mdev->vpp_sd[i];
			BUG_ON(!sd);
			if (decon->vpp_err_stat[i])
				state = VPP_STOP_ERR;
			v4l2_subdev_call(sd, core, ioctl, VPP_STOP,
						(unsigned long *)state);
			vpp = v4l2_get_subdevdata(sd);
#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
			decon->bts2_ops->bts_release_vpp(vpp);
#endif
			decon->vpp_used[i] = false;
			decon->vpp_err_stat[i] = false;
		}
	}
}

static int cfw_buf_list_create(struct ion_client *client,
		struct ion_handle *handle, struct decon_reg_data *regs, u32 id)
{
	int ret = 0;
	struct decon_sbuf_data *sbuf;

	sbuf = kzalloc(sizeof(struct decon_sbuf_data), GFP_KERNEL);
	if (sbuf == NULL) {
		decon_err("%s: couldn't allocate.\n", __func__);
		return -ENOMEM;
	}

	ret = ion_phys(client, handle,
			(ion_phys_addr_t *)&sbuf->addr, (size_t *)&sbuf->len);
	if (ret < 0) {
		decon_err("%s: couldn't get %d ion_phys(ret=%d).\n",
				__func__, id, ret);
		kfree(sbuf);
		return ret;
	}

	sbuf->id = id + PROT_G0;
	list_add_tail(&sbuf->list, &regs->sbuf_pend_list);

	decon_cfw_dbg("%s, ID=%d, Addr=0x%lx, Lenth=%d\n",
			__func__, sbuf->id, sbuf->addr, sbuf->len);
	return 0;
}

static void cfw_buf_list_to_active(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	if (regs == NULL) return;

	if (list_empty(&regs->sbuf_pend_list)) return;

	list_replace_init(&regs->sbuf_pend_list, &decon->sbuf_active_list);
}

static int decon_cfw_buf_ctrl(struct decon_device *decon,
		struct decon_reg_data *regs, u32 flag)
{
	int ret = 0;
	struct list_head *sbuf_list;
	struct decon_sbuf_data *sbuf, *next;

	/* Set CFW for pending(will be accessing) lists */
	if (flag == SMC_DRM_SECBUF_CFW_PROT)
		sbuf_list = &regs->sbuf_pend_list;
	else /* Unset CFW for active(Already accessed) lists */
		sbuf_list = &decon->sbuf_active_list;

	if (list_empty(sbuf_list)) return 0;

	list_for_each_entry_safe(sbuf, next, sbuf_list, list) {
		decon_cfw_dbg("%s, CFW(0x%x) id=%d, addr=0x%lx, lenth=%d\n",
				__func__, flag, sbuf->id, sbuf->addr, sbuf->len);

		ret = exynos_smc(flag, sbuf->addr, sbuf->len, sbuf->id);
		if (ret != DRMDRV_OK) {
			decon_err("failed to set CFW(0x%x), ID(%d), ret(0x%x)\n",
					flag, sbuf->id, ret);
		}

		if (flag == SMC_DRM_SECBUF_CFW_UNPROT) {
			list_del(&sbuf->list);
			kfree(sbuf);
		}
	}

	return ret;
}

static void decon_cfw_buf_release(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	if (decon->pdata->out_type == DECON_OUT_DSI) {
		decon_cfw_buf_ctrl(decon, regs, SMC_DRM_SECBUF_CFW_UNPROT);
		cfw_buf_list_to_active(decon, regs);
	} else if (decon->pdata->out_type == DECON_OUT_WB) {
		cfw_buf_list_to_active(decon, regs);
		decon_cfw_buf_ctrl(decon, regs, SMC_DRM_SECBUF_CFW_UNPROT);
	} else {
		decon_dbg("%s, Not supported\n", __func__);
	}
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static inline void decon_win_update_rect_reset(struct decon_device *decon)
{
	decon->update_win.x = 0;
	decon->update_win.y = 0;
	decon->update_win.w = 0;
	decon->update_win.h = 0;
	decon->need_update = true;
}

static void decon_wait_for_framedone(struct decon_device *decon)
{
	int ret;
	s64 time_ms = ktime_to_ms(ktime_get()) - ktime_to_ms(decon->trig_mask_timestamp);

	if (time_ms < MAX_FRM_DONE_WAIT) {
		ret = wait_event_interruptible_timeout(decon->wait_frmdone,
				(decon->frame_done_cnt_target <= decon->frame_done_cnt_cur),
				msecs_to_jiffies(MAX_FRM_DONE_WAIT - time_ms));
	}
}

static int decon_reg_ddi_partial_cmd(struct decon_device *decon, struct decon_win_rect *rect)
{
	struct decon_win_rect win_rect;
	int ret;

	/* Guarantee vstatus is IDLE */
	decon_wait_for_framedone(decon);
	decon_reg_wait_linecnt_is_zero_timeout(decon->id, 0, 35 * 1000);

	/* Partial Command */
	win_rect.x = rect->x;
	win_rect.y = rect->y;
	/* w is right & h is bottom */
	win_rect.w = rect->x + rect->w - 1;
	win_rect.h = rect->y + rect->h - 1;
	ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
			DSIM_IOC_PARTIAL_CMD, &win_rect);
	if (ret) {
		decon_win_update_rect_reset(decon);
		decon_err("%s: partial_area CMD is failed  %s [%d %d %d %d]\n",
				__func__, decon->output_sd->name, rect->x,
				rect->y, rect->w, rect->h);
	}

	return ret;
}

static int decon_win_update_disp_config(struct decon_device *decon,
					struct decon_win_rect *win_rect)
{
	struct decon_lcd lcd_info;
	int ret = 0;
	u32 comp_ratio;

	memcpy(&lcd_info, decon->lcd_info, sizeof(struct decon_lcd));
	lcd_info.xres = win_rect->w;
	lcd_info.yres = win_rect->h;

	if (decon->lcd_info->dsc_enabled)
		comp_ratio = 3;
	else if (decon->lcd_info->mic_enabled &&
			decon->lcd_info->mic_ratio == MIC_COMP_RATIO_1_3)
		comp_ratio = 3;
	else if (decon->lcd_info->mic_enabled &&
			decon->lcd_info->mic_ratio == MIC_COMP_RATIO_1_2)
		comp_ratio = 2;
	else
		comp_ratio = 1;

	/* HFP should be aligned multiple of 2 */
	lcd_info.hfp = ALIGN(decon->lcd_info->hfp +
		((decon->lcd_info->xres - win_rect->w) / comp_ratio), 2);

	v4l2_set_subdev_hostdata(decon->output_sd, &lcd_info);
	ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_SET_PORCH, NULL);
	if (ret) {
		decon_win_update_rect_reset(decon);
		decon_err("failed to set porch values of DSIM [%d %d %d %d]\n",
				win_rect->x, win_rect->y,
				win_rect->w, win_rect->h);
	}

	decon_reg_set_partial_update(decon->id, decon->pdata->dsi_mode, &lcd_info);
	decon_win_update_dbg("[WIN_UPDATE]%s : vfp %d vbp %d vsa %d hfp %d hbp %d hsa %d w %d h %d\n",
			__func__,
			lcd_info.vfp, lcd_info.vbp, lcd_info.vsa,
			lcd_info.hfp, lcd_info.hbp, lcd_info.hsa,
			win_rect->w, win_rect->h);

	return ret;
}
#endif

int decon_tui_protection(struct decon_device *decon, bool tui_en)
{
	int ret = 0;
	struct v4l2_subdev *sd = decon->mdev->vpp_sd[IDMA_G0];
	int win_idx;

	decon_warn("%s:state %d: out_type %d:+\n", __func__,
				tui_en, decon->pdata->out_type);
	if (tui_en) {
		decon_lpd_block_exit(decon);

		mutex_lock(&decon->output_lock);
		if (decon->state == DECON_STATE_OFF) {
			decon_warn("%s: already disabled(tui=%d)\n", __func__, tui_en);
			ret = -EBUSY;
			mutex_unlock(&decon->output_lock);
			goto exit;
		}
		flush_kthread_worker(&decon->update_regs_worker);

#ifdef CONFIG_FB_WINDOW_UPDATE
		if (decon->need_update) {
			decon->update_win.x = decon->update_win.y = 0;
			decon->update_win.w = decon->lcd_info->xres;
			decon->update_win.h = decon->lcd_info->yres;
			decon_reg_ddi_partial_cmd(decon, &decon->update_win);
			decon_win_update_disp_config(decon, &decon->update_win);
			decon->need_update = false;
		}
#endif
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

		/* Disable all of normal window before using secure world */
		for (win_idx = 0; win_idx < decon->pdata->max_win; win_idx++)
			decon_write(decon->id, WIN_CONTROL(win_idx), 0x0);
		decon_reg_all_win_shadow_update_req(decon->id);

		/* Decon is shutdown. It will be started in secure world */
		decon_reg_release_resource_instantly(decon->id);
		decon_set_protected_content(decon, NULL);
		decon_cfw_buf_release(decon, NULL);
		decon->vpp_usage_bitmask = 0;
		decon_vpp_stop(decon, false);

		ret = v4l2_subdev_call(sd, core, ioctl, VPP_TUI_PROTECT,
				(unsigned long *)true);

		decon->pdata->out_type = DECON_OUT_TUI;
		mutex_unlock(&decon->output_lock);
	} else {
		mutex_lock(&decon->output_lock);
		ret = v4l2_subdev_call(sd, core, ioctl, VPP_TUI_PROTECT,
				(unsigned long *)false);

		decon->pdata->out_type = DECON_OUT_DSI;
		mutex_unlock(&decon->output_lock);

		decon_lpd_unblock(decon);
	}

exit:
	decon_warn("%s:state %d: out_type %d:-\n", __func__,
				tui_en, decon->pdata->out_type);
	return ret;
}

static void decon_free_dma_buf(struct decon_device *decon,
		struct decon_dma_buf_data *dma)
{
	if (!dma->dma_addr)
		return;

	if (dma->fence)
		sync_fence_put(dma->fence);

	ion_iovmm_unmap(dma->attachment, dma->dma_addr);

	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);

	dma_buf_detach(dma->dma_buf, dma->attachment);
	dma_buf_put(dma->dma_buf);
	ion_free(decon->ion_client, dma->ion_handle);
	memset(dma, 0, sizeof(struct decon_dma_buf_data));
}


static void decon_esd_enable_interrupt(struct decon_device *decon)
{
	struct esd_protect *esd = &decon->esd;

	struct dsim_device *dsim = NULL;

	dsim = container_of(decon->output_sd, struct dsim_device, sd);
	if(dsim->priv.esd_disable){
		decon_info( "%s : esd_disable = %d\n", __func__, dsim->priv.esd_disable );
		return;
	}

	decon_info("%s : \n",__func__);
	if (esd) {
		esd->irq_type = irq_no_esd;
		esd->when_irq_enable = ktime_get();
		if (esd->pcd_irq){
			enable_irq(esd->pcd_irq);
		}

#ifdef CONFIG_DSIM_ESD_RECOVERY_NOPOWER
		if (esd->disp_det_irq) {
			enable_irq(esd->disp_det_irq);
		}
#endif	// CONFIG_DSIM_ESD_RECOVERY_NOPOWER

		if (esd->err_irq) {
			enable_irq(esd->err_irq);
		}
	}
	return;
}


static void decon_esd_disable_interrupt(struct decon_device *decon)
{
	struct esd_protect *esd = &decon->esd;
	struct dsim_device *dsim = NULL;

	dsim = container_of(decon->output_sd, struct dsim_device, sd);
	if(dsim->priv.esd_disable){
		decon_info( "%s : esd_disable = %d\n", __func__, dsim->priv.esd_disable );
		return;
	}

	decon_info("%s : \n",__func__);

	if (esd) {
		if (esd->pcd_irq)
			disable_irq(esd->pcd_irq);
		if (esd->err_irq)
			disable_irq(esd->err_irq);
#ifdef CONFIG_DSIM_ESD_RECOVERY_NOPOWER
		if (esd->disp_det_irq) {
			disable_irq(esd->disp_det_irq);
		}
#endif
	}
	return;
}
/* ---------- FB_BLANK INTERFACE ----------- */
int decon_enable(struct decon_device *decon)
{
	struct decon_mode_info psr;
	struct decon_param p;
	int state = decon->state;
	int ret = 0;

	decon_info("enable decon-%d\n", decon->id);
	exynos_ss_printk("%s:state %d: active %d:+\n", __func__,
				decon->state, pm_runtime_active(decon->dev));

	if (decon->state != DECON_STATE_LPD_EXIT_REQ)
		mutex_lock(&decon->output_lock);

	if (!decon->id && (decon->pdata->out_type == DECON_OUT_DSI) &&
				(decon->state == DECON_STATE_INIT)) {
		decon_info("decon%d init state\n", decon->id);
		decon->state = DECON_STATE_ON;
		goto err;
	}

	if (decon->state == DECON_STATE_ON) {
		decon_warn("decon%d already enabled\n", decon->id);
		goto err;
	}

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_get_sync(decon->dev);
#else
	decon_runtime_resume(decon->dev);
#endif
	SYSTRACE_C_BEGIN("decon_bts");
	call_init_ops(decon, bts_set_init, decon);
	SYSTRACE_C_FINISH("decon_bts");

	if (decon->state == DECON_STATE_LPD_EXIT_REQ) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
				DSIM_IOC_ENTER_ULPS, (unsigned long *)0);
		if (ret) {
			decon_err("%s: failed to exit ULPS state for %s\n",
					__func__, decon->output_sd->name);
			goto err;
		}
#if 1
		ret = v4l2_subdev_call(decon->output_sd1, core, ioctl,
				DSIM_IOC_ENTER_ULPS, (unsigned long *)0);
		if (ret) {
			decon_err("%s: failed to exit ULPS state for %s\n",
					__func__, decon->output_sd1->name);
			goto err;
		}
#endif
	} else {
		if (decon->pdata->psr_mode != DECON_VIDEO_MODE) {
			if (decon->pinctrl && decon->decon_te_on) {
				if (pinctrl_select_state(decon->pinctrl, decon->decon_te_on)) {
					decon_err("failed to turn on Decon_TE\n");
					goto err;
				}
			}
		}
		if (decon->pdata->out_type == DECON_OUT_DSI) {
			pm_stay_awake(decon->dev);
			dev_warn(decon->dev, "pm_stay_awake");

			ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 1);
			if (ret) {
				decon_err("starting stream failed for %s\n",
						decon->output_sd->name);
				goto err;
			}
			decon_info("enabled 2nd DSIM and LCD for dual DSI mode\n");
			ret = v4l2_subdev_call(decon->output_sd1, video, s_stream, 1);
			if (ret) {
				decon_err("starting stream failed for %s\n",
						decon->output_sd1->name);
				goto err;
			}
		}
	}

	decon_to_init_param(decon, &p);
	decon_reg_init(decon->id, decon->pdata->out_idx, &p);

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
	if (!decon->id)
		mdnie_reg_start(decon->lcd_info->xres, decon->lcd_info->yres);
#endif

	decon_to_psr_info(decon, &psr);
	if (decon->state != DECON_STATE_LPD_EXIT_REQ) {
		/* In case of resume*/
		if (decon->pdata->out_type != DECON_OUT_WB) {
			struct decon_window_regs win_regs;
			struct decon_lcd *lcd = decon->lcd_info;

			memset(&win_regs, 0, sizeof(struct decon_window_regs));
			win_regs.wincon = wincon(0x8, 0xFF, 0xFF, 0xFF, DECON_BLENDING_NONE);
			win_regs.wincon |= WIN_CONTROL_EN_F;
			win_regs.start_pos = win_start_pos(0, 0);
			win_regs.end_pos = win_end_pos(0, 0, lcd->xres, lcd->yres);
			win_regs.colormap = 0x000000;
			win_regs.pixel_count = lcd->xres * lcd->yres;
			win_regs.whole_w = lcd->xres;
			win_regs.whole_h = lcd->yres;
			win_regs.offset_x = 0;
			win_regs.offset_y = 0;
			decon_reg_set_window_control(decon->id, 0, &win_regs, true);
			decon_reg_update_req_window(decon->id, 0);
			decon_reg_start(decon->id, &psr);

			msleep(500);
			decon_reg_update_req_and_unmask(decon->id, &psr);
		}
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		if (!decon->id) {
			ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL);
			if (ret)
				decon_err("Failed to call DSIM packet go enable!\n");
		}
#endif
	}

#ifdef CONFIG_FB_WINDOW_UPDATE
	if ((decon->pdata->out_type == DECON_OUT_DSI) && (decon->need_update)) {
		if (decon->state != DECON_STATE_LPD_EXIT_REQ) {
			decon->need_update = false;
			decon->update_win.x = 0;
			decon->update_win.y = 0;
			decon->update_win.w = decon->lcd_info->xres;
			decon->update_win.h = decon->lcd_info->yres;
		} else {
			decon_win_update_disp_config(decon, &decon->update_win);
		}
	}
#endif
	if (!decon->id && state != DECON_STATE_LPD_EXIT_REQ)
		decon_esd_enable_interrupt(decon);

	if (!decon->id && !decon->eint_status) {
		struct irq_desc *desc = irq_to_desc(decon->irq);
		/* Pending IRQ clear */
		if (desc->irq_data.chip->irq_ack) {
			desc->irq_data.chip->irq_ack(&desc->irq_data);
			desc->istate &= ~IRQS_PENDING;
		}
		enable_irq(decon->irq);
		decon->eint_status = 1;
	}

	decon->state = DECON_STATE_ON;
	decon_reg_set_int(decon->id, &psr, 1);

err:
	exynos_ss_printk("%s:state %d: active %d:-\n", __func__,
				decon->state, pm_runtime_active(decon->dev));
	if (state != DECON_STATE_LPD_EXIT_REQ)
		mutex_unlock(&decon->output_lock);
	return ret;
}

int decon_disable(struct decon_device *decon)
{
	struct decon_mode_info psr;
	int ret = 0;
	unsigned long irq_flags;
	int state = decon->state;
	struct esd_protect *esd = &decon->esd;


	exynos_ss_printk("disable decon-%d, state(%d) cnt %d\n", decon->id,
				decon->state, pm_runtime_active(decon->dev));

	if (!decon->id && decon->state != DECON_STATE_LPD_ENT_REQ) {
		if (esd->esd_wq) {
			decon_esd_disable_interrupt(decon);
			flush_workqueue(esd->esd_wq);
		}
	}

	/* Clear TUI state: Case of LCD off without TUI exit */
	if (decon->pdata->out_type == DECON_OUT_TUI)
		decon_tui_protection(decon, false);
	if (decon->state != DECON_STATE_LPD_ENT_REQ)
		mutex_lock(&decon->output_lock);

	if (decon->state == DECON_STATE_OFF) {
		decon_info("decon%d already disabled\n", decon->id);
		goto err;
	} else if (decon->state == DECON_STATE_LPD) {

#ifdef DECON_LPD_OPT
		decon_lcd_off(decon);
		decon_info("decon is LPD state. only lcd is off\n");
#endif
		goto err;
	}

	flush_kthread_worker(&decon->update_regs_worker);

	decon_to_psr_info(decon, &psr);
	decon_reg_set_int(decon->id, &psr, 0);

	if (!decon->id && (decon->vsync_info.irq_refcount <= 0) &&
			decon->eint_status) {
		disable_irq(decon->irq);
		decon->eint_status = 0;
	}

	decon_to_psr_info(decon, &psr);
	decon_reg_stop(decon->id, decon->pdata->out_idx, &psr);
	decon_reg_clear_int_all(decon->id);

	/* DMA protection disable must be happen on vpp domain is alive */
	if (decon->pdata->out_type != DECON_OUT_WB) {
		decon_set_protected_content(decon, NULL);
		decon_cfw_buf_release(decon, NULL);
		decon->vpp_usage_bitmask = 0;
		decon_vpp_stop(decon, true);
	}

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
	if (!decon->id)
		mdnie_reg_stop();
#endif
	/* Synchronize the decon->state with irq_handler */
	spin_lock_irqsave(&decon->slock, irq_flags);
	if (state == DECON_STATE_LPD_ENT_REQ)
		decon->state = DECON_STATE_LPD;
	spin_unlock_irqrestore(&decon->slock, irq_flags);

	SYSTRACE_C_BEGIN("decon_bts");
#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
	decon->bts2_ops->bts_release_bw(decon);
#else
	call_init_ops(decon, bts_release_init, decon);
#endif
	SYSTRACE_C_FINISH("decon_bts");

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_put_sync(decon->dev);
#else
	decon_runtime_suspend(decon->dev);
#endif

	if (state == DECON_STATE_LPD_ENT_REQ) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
				DSIM_IOC_ENTER_ULPS, (unsigned long *)1);
		if (ret) {
			decon_err("%s: failed to enter ULPS state for %s\n",
					__func__, decon->output_sd->name);
			goto err;
		}
#if 1
		ret = v4l2_subdev_call(decon->output_sd1, core, ioctl,
				DSIM_IOC_ENTER_ULPS, (unsigned long *)1);
		if (ret) {
			decon_err("%s: failed to enter ULPS state for %s\n",
					__func__, decon->output_sd1->name);
			goto err;
		}
#endif
		decon->state = DECON_STATE_LPD;
	} else {
		if (decon->pdata->out_type == DECON_OUT_DSI) {
			/* stop output device (mipi-dsi or hdmi) */
			ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 0);
			if (ret) {
				decon_err("stopping stream failed for %s\n",
						decon->output_sd->name);
				goto err;
			}
			ret = v4l2_subdev_call(decon->output_sd1, video, s_stream, 0);
			if (ret) {
				decon_err("stopping stream failed for %s\n",
						decon->output_sd1->name);
				goto err;
			}

			pm_relax(decon->dev);
			dev_warn(decon->dev, "pm_relax");
		}
		if (decon->pdata->psr_mode != DECON_VIDEO_MODE) {
			if (decon->pinctrl && decon->decon_te_off) {
				if (pinctrl_select_state(decon->pinctrl, decon->decon_te_off)) {
					decon_err("failed to turn off Decon_TE\n");
					goto err;
				}
			}
		}

		decon->state = DECON_STATE_OFF;
	}

err:
	exynos_ss_printk("%s:state %d: active%d:-\n", __func__,
				decon->state, pm_runtime_active(decon->dev));
	if (state != DECON_STATE_LPD_ENT_REQ)
		mutex_unlock(&decon->output_lock);
	return ret;
}

static int decon_blank(int blank_mode, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int ret = 0;

	decon_info("%s ++ blank_mode : %d \n", __func__, blank_mode);
	decon_info("decon-%d %s mode: %dtype (0: DSI, 1: HDMI, 2:WB)\n",
			decon->id,
			blank_mode == FB_BLANK_UNBLANK ? "UNBLANK" : "POWERDOWN",
			decon->pdata->out_type);

	decon_lpd_block_exit(decon);

#ifdef CONFIG_USE_VSYNC_SKIP
	decon_extra_vsync_wait_set(ERANGE);
#endif /* CONFIG_USE_VSYNC_SKIP */

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		DISP_SS_EVENT_LOG(DISP_EVT_BLANK, &decon->sd, ktime_set(0, 0));
		ret = decon_disable(decon);
		if (ret) {
			decon_err("failed to disable decon\n");
			goto blank_exit;
		}
		break;
	case FB_BLANK_UNBLANK:
		DISP_SS_EVENT_LOG(DISP_EVT_UNBLANK, &decon->sd, ktime_set(0, 0));
		ret = decon_enable(decon);
		if (ret) {
			decon_err("failed to enable decon\n");
			goto blank_exit;
		}

		if (decon->pdata->out_type == DECON_OUT_DSI) {
			decon->dispon_req = DSI0_REQ_DP_ON(decon->dispon_req);
#if 0 // for LPM,
			decon->dispon_req = DSI1_REQ_DP_ON(decon->dispon_req);
#endif
		}
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	default:
		ret = -EINVAL;
	}

blank_exit:
	decon_lpd_unblock(decon);
	decon_info("%s -- blank_mode : %d \n", __func__, blank_mode);
	return ret;
}

/* ---------- FB_IOCTL INTERFACE ----------- */
static void decon_activate_vsync(struct decon_device *decon)
{
	int prev_refcount;

	decon->tracing_mark_write( decon->systrace_pid, 'C', "decon_activate_vsync", decon->vsync_info.irq_refcount+1 );
	mutex_lock(&decon->vsync_info.irq_lock);

	prev_refcount = decon->vsync_info.irq_refcount++;
	if (!prev_refcount)
		DISP_SS_EVENT_LOG(DISP_EVT_ACT_VSYNC, &decon->sd, ktime_set(0, 0));

	mutex_unlock(&decon->vsync_info.irq_lock);
}

static void decon_deactivate_vsync(struct decon_device *decon)
{
	int new_refcount;

	decon->tracing_mark_write( decon->systrace_pid, 'C', "decon_activate_vsync", decon->vsync_info.irq_refcount-1 );
	mutex_lock(&decon->vsync_info.irq_lock);

	new_refcount = --decon->vsync_info.irq_refcount;
	WARN_ON(new_refcount < 0);
	if (!new_refcount)
		DISP_SS_EVENT_LOG(DISP_EVT_DEACT_VSYNC, &decon->sd, ktime_set(0, 0));

	mutex_unlock(&decon->vsync_info.irq_lock);
}

int decon_wait_for_vsync(struct decon_device *decon, u32 timeout)
{
	ktime_t timestamp;
	int ret;


	if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE) {
		if (decon->ignore_vsync)
			goto wait_exit;
	}

	timestamp = decon->vsync_info.timestamp;
	decon_activate_vsync(decon);

	if (timeout) {
		ret = wait_event_interruptible_timeout(decon->vsync_info.wait,
				!ktime_equal(timestamp,
						decon->vsync_info.timestamp),
				msecs_to_jiffies(timeout));
	} else {
		ret = wait_event_interruptible(decon->vsync_info.wait,
				!ktime_equal(timestamp,
						decon->vsync_info.timestamp));
	}

	decon_deactivate_vsync(decon);

	if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE) {
			if (decon->ignore_vsync)
				goto wait_exit;
	}

	if (timeout && ret == 0) {
		if (decon->eint_pend && decon->eint_mask) {
			decon_err("decon%d wait for vsync timeout(p:0x%x, m:0x%x)\n",
				decon->id, readl(decon->eint_pend),
				readl(decon->eint_mask));
		} else {
			decon_err("decon%d wait for vsync timeout\n", decon->id);
		}

		return -ETIMEDOUT;
	}

wait_exit:
	return 0;
}

int decon_set_window_position(struct fb_info *info,
				struct decon_user_window user_window)
{
	return 0;
}

int decon_set_plane_alpha_blending(struct fb_info *info,
				struct s3c_fb_user_plane_alpha user_alpha)
{
	return 0;
}

int decon_set_chroma_key(struct fb_info *info,
			struct s3c_fb_user_chroma user_chroma)
{
	return 0;
}

int decon_set_vsync_int(struct fb_info *info, bool active)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	bool prev_active = decon->vsync_info.active;

	decon->vsync_info.active = active;
	smp_wmb();

	if (active && !prev_active)
		decon_activate_vsync(decon);
	else if (!active && prev_active)
		decon_deactivate_vsync(decon);

	return 0;
}

static unsigned int decon_map_ion_handle(struct decon_device *decon,
		struct device *dev, struct decon_dma_buf_data *dma,
		struct ion_handle *ion_handle, struct dma_buf *buf, int win_no)
{
	dma->fence = NULL;
	dma->dma_buf = buf;

	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		decon_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		decon_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	dma->dma_addr = ion_iovmm_map(dma->attachment, 0,
			dma->dma_buf->size, DMA_TO_DEVICE, 0);
	if (!dma->dma_addr || IS_ERR_VALUE(dma->dma_addr)) {
		decon_err("iovmm_map() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}

	exynos_ion_sync_dmabuf_for_device(dev, dma->dma_buf, dma->dma_buf->size,
			DMA_TO_DEVICE);

	dma->ion_handle = ion_handle;

	return dma->dma_buf->size;

err_iovmm_map:
	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);
err_buf_map_attachment:
	dma_buf_detach(dma->dma_buf, dma->attachment);
err_buf_map_attach:
	return 0;
}

static u32 get_vpp_src_format(u32 format, int id)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_NV12:
		return DECON_PIXEL_FORMAT_NV21;
	case DECON_PIXEL_FORMAT_NV21:
		return DECON_PIXEL_FORMAT_NV12;
	case DECON_PIXEL_FORMAT_NV12M:
		return DECON_PIXEL_FORMAT_NV21M;
	case DECON_PIXEL_FORMAT_NV21M:
		return DECON_PIXEL_FORMAT_NV12M;
	case DECON_PIXEL_FORMAT_NV12N:
		return DECON_PIXEL_FORMAT_NV12N;
	case DECON_PIXEL_FORMAT_NV12N_10B:
		return DECON_PIXEL_FORMAT_NV12N_10B;
	case DECON_PIXEL_FORMAT_YUV420:
		return DECON_PIXEL_FORMAT_YVU420;
	case DECON_PIXEL_FORMAT_YVU420:
		return DECON_PIXEL_FORMAT_YUV420;
	case DECON_PIXEL_FORMAT_YUV420M:
		return DECON_PIXEL_FORMAT_YVU420M;
	case DECON_PIXEL_FORMAT_YVU420M:
		return DECON_PIXEL_FORMAT_YUV420M;
	case DECON_PIXEL_FORMAT_ARGB_8888:
		return DECON_PIXEL_FORMAT_BGRA_8888;
	case DECON_PIXEL_FORMAT_ABGR_8888:
		return DECON_PIXEL_FORMAT_RGBA_8888;
	case DECON_PIXEL_FORMAT_RGBA_8888:
		return DECON_PIXEL_FORMAT_ABGR_8888;
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return DECON_PIXEL_FORMAT_ARGB_8888;
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return DECON_PIXEL_FORMAT_BGRX_8888;
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return DECON_PIXEL_FORMAT_RGBX_8888;
	case DECON_PIXEL_FORMAT_RGBX_8888:
		return DECON_PIXEL_FORMAT_XBGR_8888;
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return DECON_PIXEL_FORMAT_XRGB_8888;
	default:
		return format;
	}
}

static u32 get_vpp_out_format(u32 format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
	case DECON_PIXEL_FORMAT_NV12N:
	case DECON_PIXEL_FORMAT_NV12N_10B:
	case DECON_PIXEL_FORMAT_YUV420:
	case DECON_PIXEL_FORMAT_YVU420:
	case DECON_PIXEL_FORMAT_YUV420M:
	case DECON_PIXEL_FORMAT_YVU420M:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return DECON_PIXEL_FORMAT_BGRA_8888;
	default:
		return format;
	}
}

static bool decon_get_protected_idma(struct decon_device *decon, u32 prot_bits)
{
	int i;
	u32 used_idma = 0;

	for (i = 0; i < MAX_DMA_TYPE - 1; i++)
		if ((prot_bits & (1 << i)) >> i)
			used_idma++;

	if (used_idma)
		return true;
	else
		return false;
}

static void decon_set_protected_content(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	bool prot;
	int type, i, ret = 0;
	u32 change = 0;
	u32 cur_protect_bits = 0;

	/**
	 * get requested DMA protection configs
	 * 0:G0 1:G1 2:VG0 3:VG1 4:VGR0 5:VGR1 6:G2 7:G3 8:WB1
	 */
	/* IDMA protection configs (G0,G1,VG0,VG1,VGR0,VGR1,G2,G3) */
	for (i = 0; i < decon->pdata->max_win; i++) {
		if (!regs)
			break;

		cur_protect_bits |=
			(regs->protection[i] << regs->vpp_config[i].idma_type);
	}

	/* ODMA(for writeback) protection config (WB1)*/
	if (decon->pdata->out_type == DECON_OUT_WB)
		if (decon_get_protected_idma(decon, cur_protect_bits))
			cur_protect_bits |= (1 << ODMA_WB);

	if (decon->prev_protection_bitmask != cur_protect_bits) {
		decon_reg_wait_linecnt_is_zero_timeout(decon->id, 0, 35 * 1000);

		/* apply protection configs for each DMA */
		for (type = 0; type < MAX_DMA_TYPE; type++) {
			prot = cur_protect_bits & (1 << type);

			change = (cur_protect_bits & (1 << type)) ^
				(decon->prev_protection_bitmask & (1 << type));

			if (change) {
				struct v4l2_subdev *sd = NULL;
				sd = decon->mdev->vpp_sd[type];
				v4l2_subdev_call(sd, core, ioctl,
						VPP_WAIT_IDLE, NULL);
				ret = exynos_smc(SMC_PROTECTION_SET, 0,
						type + DECON_TZPC_OFFSET, prot);
				if (ret != SUCCESS_EXYNOS_SMC)
					WARN(1, "decon%d DMA-%d smc call fail\n",
							decon->id, type);
				else
					decon_dbg("decon%d DMA-%d protection %s\n",
							decon->id, type, prot ? "enabled" : "disabled");
			}
		}
	}

	/* save current portection configs */
	decon->prev_protection_bitmask = cur_protect_bits;
}

static int decon_set_win_buffer(struct decon_device *decon, struct decon_win *win,
		struct decon_win_config *win_config, struct decon_reg_data *regs)
{
	struct ion_handle *handle;
	struct fb_var_screeninfo prev_var = win->fbinfo->var;
	struct dma_buf *buf[MAX_BUF_PLANE_CNT];
	struct decon_dma_buf_data dma_buf_data[MAX_BUF_PLANE_CNT];
	unsigned short win_no = win->index;
	int ret, i;
	size_t buf_size = 0;
	int plane_cnt;
	u32 format;
	int vpp_id;
	struct device *dev;

	for (i = 0; i < MAX_BUF_PLANE_CNT; i++)
		buf[i] = NULL;

	if (win_config->format >= DECON_PIXEL_FORMAT_MAX) {
		decon_err("unknown pixel format %u\n", win_config->format);
		return -EINVAL;
	}

	if (win_config->blending >= DECON_BLENDING_MAX) {
		decon_err("unknown blending %u\n", win_config->blending);
		return -EINVAL;
	}

	if (win_no == 0 && win_config->blending != DECON_BLENDING_NONE) {
		decon_err("blending not allowed on window 0\n");
		return -EINVAL;
	}

	if (win_config->dst.w == 0 || win_config->dst.h == 0 ||
			win_config->dst.x < 0 || win_config->dst.y < 0) {
		decon_err("win[%d] size is abnormal (w:%d, h:%d, x:%d, y:%d)\n",
				win_no, win_config->dst.w, win_config->dst.h,
				win_config->dst.x, win_config->dst.y);
		return -EINVAL;
	}

	format = get_vpp_out_format(win_config->format);

	win->fbinfo->var.red.length = decon_red_length(format);
	win->fbinfo->var.red.offset = decon_red_offset(format);
	win->fbinfo->var.green.length = decon_green_length(format);
	win->fbinfo->var.green.offset = decon_green_offset(format);
	win->fbinfo->var.blue.length = decon_blue_length(format);
	win->fbinfo->var.blue.offset = decon_blue_offset(format);
	win->fbinfo->var.transp.length =
			decon_transp_length(win_config->format);
	win->fbinfo->var.transp.offset =
			decon_transp_offset(win_config->format);
	win->fbinfo->var.bits_per_pixel = win->fbinfo->var.red.length +
			win->fbinfo->var.green.length +
			win->fbinfo->var.blue.length +
			win->fbinfo->var.transp.length +
			decon_padding(format);

	if (win_config->dst.w * win->fbinfo->var.bits_per_pixel / 8 < 128) {
		decon_err("window wide < 128bytes, width = %u, bpp = %u)\n",
				win_config->dst.w,
				win->fbinfo->var.bits_per_pixel);
		ret = -EINVAL;
		goto err_invalid;
	}

	if (!decon_validate_x_alignment(decon, win_config->dst.x,
				win_config->dst.w,
				win->fbinfo->var.bits_per_pixel)) {
		ret = -EINVAL;
		goto err_invalid;
	}

	plane_cnt = decon_get_plane_cnt(win_config->format);
	for (i = 0; i < plane_cnt; ++i) {
		handle = ion_import_dma_buf(decon->ion_client, win_config->fd_idma[i]);
		if (IS_ERR(handle)) {
			decon_err("failed to import fd\n");
			ret = PTR_ERR(handle);
			goto err_invalid;
		}

		buf[i] = dma_buf_get(win_config->fd_idma[i]);
		if (IS_ERR_OR_NULL(buf[i])) {
			decon_err("dma_buf_get() failed: %ld\n", PTR_ERR(buf[i]));
			ret = PTR_ERR(buf[i]);
			goto err_buf_get;
		}

		vpp_id = win_config->idma_type;
		dev = decon->mdev->vpp_dev[vpp_id];
		buf_size = decon_map_ion_handle(decon, dev, &dma_buf_data[i],
				handle, buf[i], win_no);
		if (!buf_size) {
			ret = -ENOMEM;
			goto err_map;
		}
		if (win_config->protection) {
			cfw_buf_list_create(decon->ion_client, handle,
					regs, vpp_id);
		}
		win_config->vpp_parm.addr[i] = dma_buf_data[i].dma_addr;
		handle = NULL;
		buf[i] = NULL;
	}
	if (win_config->fence_fd >= 0) {
		dma_buf_data[0].fence = sync_fence_fdget(win_config->fence_fd);
		if (!dma_buf_data[0].fence) {
			decon_err("failed to import fence fd\n");
			ret = -EINVAL;
			goto err_offset;
		}
		decon_dbg("%s(%d): fence_fd(%d), fence(%lx)\n", __func__, __LINE__,
				win_config->fence_fd, (ulong)dma_buf_data[0].fence);
	}

	win->fbinfo->fix.smem_start = dma_buf_data[0].dma_addr;
	win->fbinfo->fix.smem_len = buf_size;
	win->fbinfo->var.xres = win_config->dst.w;
	win->fbinfo->var.xres_virtual = win_config->dst.f_w;
	win->fbinfo->var.yres = win_config->dst.h;
	win->fbinfo->var.yres_virtual = win_config->dst.f_h;
	win->fbinfo->var.xoffset = win_config->src.x;
	win->fbinfo->var.yoffset = win_config->src.y;
#ifdef CONFIG_EXYNOS_DECON_DUAL_DSI
	if (win_config->dst.x >= decon->lcd_info->xres) {
		regs->which_dsi |= WIN_DSI1_UPDATE;
	} else {
		regs->which_dsi |= WIN_DSI0_UPDATE;
	}
#endif
	win->fbinfo->fix.line_length = win_config->src.f_w *
			win->fbinfo->var.bits_per_pixel / 8;
	win->fbinfo->fix.xpanstep = fb_panstep(win_config->dst.w,
			win->fbinfo->var.xres_virtual);
	win->fbinfo->fix.ypanstep = fb_panstep(win_config->dst.h,
			win->fbinfo->var.yres_virtual);

	plane_cnt = decon_get_plane_cnt(win_config->format);
	for (i = 0; i < plane_cnt; ++i)
		regs->dma_buf_data[win_no][i] = dma_buf_data[i];
	regs->protection[win_no] = win_config->protection;

	decon_win_conig_to_regs_param(win->fbinfo->var.transp.length, win_config,
				&regs->win_regs[win_no], win_config->idma_type);

	return 0;

err_offset:
	for (i = 0; i < plane_cnt; ++i)
		decon_free_dma_buf(decon, &dma_buf_data[i]);
	goto err_invalid;
err_map:
	for (i = 0; i < plane_cnt; ++i)
		if (buf[i])
			dma_buf_put(buf[i]);
err_buf_get:
	if (handle)
		ion_free(decon->ion_client, handle);
err_invalid:
	win->fbinfo->var = prev_var;
	return ret;
}

static int decon_set_wb_buffer(struct decon_device *decon,
		struct decon_win_config *win_config, struct decon_reg_data *regs)
{
	struct ion_handle *handle;
	struct dma_buf *buf;
	struct decon_dma_buf_data dma_buf_data[MAX_BUF_PLANE_CNT];
	int ret = 0, i;
	size_t buf_size = 0;
	int plane_cnt;
	struct device *dev;
	struct decon_win_config *config = &win_config[MAX_DECON_WIN];

	DISP_SS_EVENT_LOG(DISP_EVT_WB_SET_BUFFER, &decon->sd, ktime_set(0, 0));

	plane_cnt = decon_get_plane_cnt(config->format);
	for (i = 0; i < plane_cnt; ++i) {
		handle = ion_import_dma_buf(decon->ion_client, config->fd_idma[i]);
		if (IS_ERR(handle)) {
			decon_err("failed to import fd_odma:%d\n", config->fd_idma[i]);
			ret = PTR_ERR(handle);
			goto fail;
		}

		buf = dma_buf_get(config->fd_idma[i]);
		if (IS_ERR_OR_NULL(buf)) {
			decon_err("dma_buf_get() failed: %ld\n", PTR_ERR(buf));
			ret = PTR_ERR(buf);
			goto fail_buf;
		}

		/* idma_type Should be ODMA_WB */
		dev = decon->mdev->vpp_dev[config->idma_type];
		buf_size = decon_map_ion_handle(decon, dev, &dma_buf_data[i],
				handle, buf, 0);
		if (!buf_size) {
			decon_err("failed to mapping buffer\n");
			ret = -ENOMEM;
			goto fail_map;
		}
		if (win_config->protection) {
			cfw_buf_list_create(decon->ion_client, handle,
					regs, ODMA_WB);
		}
		regs->dma_buf_data[MAX_DECON_WIN][i] = dma_buf_data[i];
		config->vpp_parm.addr[i] = dma_buf_data[i].dma_addr;
	}

	handle = NULL;
	buf = NULL;

	return ret;

fail_map:
	if (buf)
		dma_buf_put(buf);

fail_buf:
	if (handle)
		ion_free(decon->ion_client, handle);

fail:
	decon_err("failed save write back buffer configuration\n");
	return ret;
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static inline void decon_update_2_full(struct decon_device *decon,
			struct decon_reg_data *regs,
			struct decon_lcd *lcd_info,
			int flag)
{
	if (flag)
		regs->need_update = true;

	decon->need_update = false;
	decon->update_win.x = 0;
	decon->update_win.y = 0;
	decon->update_win.w = lcd_info->xres;
	decon->update_win.h = lcd_info->yres;
	regs->update_win.x = 0;
	regs->update_win.y = 0;
	regs->update_win.w = lcd_info->xres;
	regs->update_win.h = lcd_info->yres;
	decon_win_update_dbg("[WIN_UPDATE]update2org: [%d %d %d %d]\n",
			decon->update_win.x, decon->update_win.y, decon->update_win.w, decon->update_win.h);
	return;

}

static void decon_calibrate_win_update_size(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_win_config *update_config)
{
	int i;
	struct decon_rect r1, r2;
	bool rect_changed = false;

	if (update_config->state != DECON_WIN_STATE_UPDATE)
		return;

	if ((update_config->dst.x < 0) ||
			(update_config->dst.y < 0)) {
		update_config->state = DECON_WIN_STATE_DISABLED;
		return;
	}

	r1.left = update_config->dst.x;
	r1.top = update_config->dst.y;
	r1.right = r1.left + update_config->dst.w - 1;
	r1.bottom = r1.top + update_config->dst.h - 1;

	decon_win_update_dbg("[WIN_UPDATE]get_config: [%d %d %d %d]\n",
			update_config->dst.x, update_config->dst.y,
			update_config->dst.w, update_config->dst.h);

	if (decon->lcd_info->dsc_enabled) {
		/* Case of DSC, minimum slice size is xres(full_width) x 64 */
		if ((update_config->dst.h % 64 == 0)
				&& (update_config->dst.y % 64 == 0)) {
			update_config->dst.x = 0;
			update_config->dst.w = decon->lcd_info->xres;
		} else {
			update_config->state = DECON_WIN_STATE_DISABLED;
			return;
		}
	}

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if (config->state != DECON_WIN_STATE_DISABLED) {
			if ((is_rotation(config) || does_layer_need_scale(config))) {
				update_config->state = DECON_WIN_STATE_DISABLED;
				return;
			}
			if ((config->src.w != config->dst.w) ||
					(config->src.h != config->dst.h)) {
				r2.left = config->dst.x;
				r2.top = config->dst.y;
				r2.right = r2.left + config->dst.w - 1;
				r2.bottom = r2.top + config->dst.h - 1;
				if (decon_intersect(&r1, &r2) &&
					is_decon_rect_differ(&r1, &r2)) {
					decon_union(&r1, &r2, &r1);
					rect_changed = true;
				}
			}
		}
	}

	if (rect_changed) {
		decon_win_update_dbg("update [%d %d %d %d] -> [%d %d %d %d]\n",
			update_config->dst.x, update_config->dst.y,
			update_config->dst.w, update_config->dst.h,
			r1.left, r1.top, r1.right - r1.left + 1,
			r1.bottom - r1.top + 1);
		update_config->dst.x = r1.left;
		update_config->dst.y = r1.top;
		update_config->dst.w = r1.right - r1.left + 1;
		update_config->dst.h = r1.bottom - r1.top + 1;
	}

	if (update_config->dst.x & 0x7) {
		update_config->dst.w += update_config->dst.x & 0x7;
		update_config->dst.x = update_config->dst.x & (~0x7);
	}
	update_config->dst.w = ((update_config->dst.w + 7) & (~0x7));
	if (update_config->dst.x + update_config->dst.w > decon->lcd_info->xres) {
		update_config->dst.w = decon->lcd_info->xres;
		update_config->dst.x = 0;
	}

	return;
}

static void decon_set_win_update_config(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	int i;
	struct decon_win_config *update_config = &win_config[DECON_WIN_UPDATE_IDX];
	struct decon_win_config temp_config;
	struct decon_rect r1, r2;
	struct decon_lcd *lcd_info = decon->lcd_info;
	int need_update = decon->need_update;
	bool is_scale_layer;

	memset(update_config, 0, sizeof(struct decon_win_config));

	decon_calibrate_win_update_size(decon, win_config, update_config);

	/* if the current mode is not WINDOW_UPDATE, set the config as WINDOW_UPDATE */
	if ((update_config->state == DECON_WIN_STATE_UPDATE) &&
			((update_config->dst.x != decon->update_win.x) ||
			 (update_config->dst.y != decon->update_win.y) ||
			 (update_config->dst.w != decon->update_win.w) ||
			 (update_config->dst.h != decon->update_win.h))) {
		decon->update_win.x = update_config->dst.x;
		decon->update_win.y = update_config->dst.y;
		decon->update_win.w = update_config->dst.w;
		decon->update_win.h = update_config->dst.h;
		decon->need_update = true;
		regs->need_update = true;
		regs->update_win.x = update_config->dst.x;
		regs->update_win.y = update_config->dst.y;
		regs->update_win.w = update_config->dst.w;
		regs->update_win.h = update_config->dst.h;

		decon_win_update_dbg("[WIN_UPDATE]need_update_1: [%d %d %d %d]\n",
				update_config->dst.x, update_config->dst.y, update_config->dst.w, update_config->dst.h);
	} else if (decon->need_update &&
			(update_config->state != DECON_WIN_STATE_UPDATE)) {
		/* Platform requested for normal mode, switch to normal mode from WINDOW_UPDATE */
		decon_update_2_full(decon, regs, lcd_info, true);
		return;
	} else if (decon->need_update) {
		/* It is just for debugging info */
		regs->update_win.x = update_config->dst.x;
		regs->update_win.y = update_config->dst.y;
		regs->update_win.w = update_config->dst.w;
		regs->update_win.h = update_config->dst.h;
	}

	if (update_config->state != DECON_WIN_STATE_UPDATE)
		return;

	r1.left = update_config->dst.x;
	r1.top = update_config->dst.y;
	r1.right = r1.left + update_config->dst.w - 1;
	r1.bottom = r1.top + update_config->dst.h - 1;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if (config->state == DECON_WIN_STATE_DISABLED)
			continue;
		r2.left = config->dst.x;
		r2.top = config->dst.y;
		r2.right = r2.left + config->dst.w - 1;
		r2.bottom = r2.top + config->dst.h - 1;
		if (!decon_intersect(&r1, &r2))
			continue;
		decon_intersection(&r1, &r2, &r2);
		if (((r2.right - r2.left) != 0) ||
			((r2.bottom - r2.top) != 0)) {
			if (decon_get_plane_cnt(config->format) == 1) {
				/*
				 * Platform requested for win_update mode. But, the win_update is
				 * smaller than the VPP min size. So, change the mode to normal mode
				 */
				if (((r2.right - r2.left) < 32) ||
					((r2.bottom - r2.top) < 16)) {
					decon_update_2_full(decon, regs, lcd_info, need_update);
					return;
				}
			} else {
				if (((r2.right - r2.left) < 64) ||
					((r2.bottom - r2.top) < 32)) {
					decon_update_2_full(decon, regs, lcd_info, need_update);
					return;
				}
			}
		}
	}

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if (config->state == DECON_WIN_STATE_DISABLED)
			continue;
		r2.left = config->dst.x;
		r2.top = config->dst.y;
		r2.right = r2.left + config->dst.w - 1;
		r2.bottom = r2.top + config->dst.h - 1;
		if (!decon_intersect(&r1, &r2)) {
			config->state = DECON_WIN_STATE_DISABLED;
			continue;
		}
		memcpy(&temp_config, config, sizeof(struct decon_win_config));
		is_scale_layer = does_layer_need_scale(config);
		if (update_config->dst.x > config->dst.x)
			config->dst.w = min(update_config->dst.w,
					config->dst.x + config->dst.w - update_config->dst.x);
		else if (update_config->dst.x + update_config->dst.w < config->dst.x + config->dst.w)
			config->dst.w = min(config->dst.w,
					update_config->dst.w + update_config->dst.x - config->dst.x);

		if (update_config->dst.y > config->dst.y)
			config->dst.h = min(update_config->dst.h,
					config->dst.y + config->dst.h - update_config->dst.y);
		else if (update_config->dst.y + update_config->dst.h < config->dst.y + config->dst.h)
			config->dst.h = min(config->dst.h,
					update_config->dst.h + update_config->dst.y - config->dst.y);

		config->dst.x = max(config->dst.x - update_config->dst.x, 0);
		config->dst.y = max(config->dst.y - update_config->dst.y, 0);

		if (!is_scale_layer) {
			if (update_config->dst.y > temp_config.dst.y)
				config->src.y += (update_config->dst.y - temp_config.dst.y);

			if (update_config->dst.x > temp_config.dst.x)
				config->src.x += (update_config->dst.x - temp_config.dst.x);
			config->src.w = config->dst.w;
			config->src.h = config->dst.h;
		}

		if (regs->need_update == true)
			decon_win_update_dbg("[WIN_UPDATE]win_idx %d: idma_type%d:,	\
				dst[%d %d %d %d] -> [%d %d %d %d], src[%d %d %d %d] -> [%d %d %d %d]\n",
				i, temp_config.idma_type,
				temp_config.dst.x, temp_config.dst.y, temp_config.dst.w, temp_config.dst.h,
				config->dst.x, config->dst.y, config->dst.w, config->dst.h,
				temp_config.src.x, temp_config.src.y, temp_config.src.w, temp_config.src.h,
				config->src.x, config->src.y, config->src.w, config->src.h);
	}

	return;
}
#endif

void decon_reg_chmap_validate(struct decon_device *decon, struct decon_reg_data *regs)
{
	unsigned short i, bitmap = 0;

	for (i = 0; i < decon->pdata->max_win; i++) {
		if ((regs->win_regs[i].wincon & WIN_CONTROL_EN_F) &&
			(!regs->win_regs[i].winmap_state)) {
			if (bitmap & (1 << regs->vpp_config[i].idma_type)) {
				decon_warn("Channel-%d is mapped to multiple windows\n",
					regs->vpp_config[i].idma_type);
				regs->win_regs[i].wincon &= (~WIN_CONTROL_EN_F);
			}
			bitmap |= 1 << regs->vpp_config[i].idma_type;
		}
	}
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static int decon_reg_set_win_update_config(struct decon_device *decon, struct decon_reg_data *regs)
{
	int ret = 0;

	if (regs->need_update) {
		decon_reg_ddi_partial_cmd(decon, &regs->update_win);
		ret = decon_win_update_disp_config(decon, &regs->update_win);
	}
	return ret;
}
#endif

static void decon_check_vpp_used(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	decon->vpp_usage_bitmask = 0;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (!regs->win_regs[i].winmap_state)
			win->vpp_id = regs->vpp_config[i].idma_type;
		else
			win->vpp_id = 0xF;
		if ((regs->win_regs[i].wincon & WIN_CONTROL_EN_F) &&
			(!regs->win_regs[i].winmap_state)) {
			decon->vpp_usage_bitmask |= (1 << win->vpp_id);
			decon->vpp_used[win->vpp_id] = true;
		}
	}
}

void decon_vpp_wait_wb_framedone(struct decon_device *decon)
{
	struct v4l2_subdev *sd = NULL;

	sd = decon->mdev->vpp_sd[ODMA_WB];
	if (v4l2_subdev_call(sd, core, ioctl,
			VPP_WAIT_FOR_FRAMEDONE, NULL) < 0) {
		decon_warn("%s: timeout of framedone (%d)\n",
				__func__,
				decon->timeline->value);
		decon_warn("\t\t mif(%lu), int(%lu), disp(%lu), eclk(%lu)\n",
				cal_dfs_get_rate(dvfs_mif),
				cal_dfs_get_rate(dvfs_int),
				cal_dfs_get_rate(dvfs_disp),
				clk_get_rate(decon->res.eclk_leaf) / MHZ);
#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
		decon_warn("\t\t bw(%d) peak_bw(%d) disp_bw(%llu)\n",
				decon->total_bw, decon->max_peak_bw,
				decon->max_disp_ch);
#endif
	}
}

#if !defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
static void decon_get_vpp_min_lock(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	struct v4l2_subdev *sd = NULL;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
			sd = decon->mdev->vpp_sd[win->vpp_id];
			v4l2_subdev_call(sd, core, ioctl,
					VPP_GET_BTS_VAL, &regs->vpp_config[i]);
		}
	}
}

static void decon_set_vpp_min_lock_early(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	struct v4l2_subdev *sd = NULL;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
			struct vpp_dev *vpp;
			sd = decon->mdev->vpp_sd[win->vpp_id];
			vpp = v4l2_get_subdevdata(sd);
			if (vpp->cur_bw > vpp->prev_bw) {
				v4l2_subdev_call(sd, core, ioctl,
						VPP_SET_BW,
						&regs->vpp_config[i]);
			}

			v4l2_subdev_call(sd, core, ioctl,
					VPP_SET_ROT_MIF,
					&regs->vpp_config[i]);

			if (decon->disp_cur > decon->disp_prev) {
				SYSTRACE_C_BEGIN("pm_qos_update_request");
				pm_qos_update_request(&decon->disp_qos, decon->disp_cur);
				pm_qos_update_request(&decon->int_qos, decon->disp_cur);
				SYSTRACE_C_FINISH("pm_qos_update_request");
			}
		}
	}
}

static void decon_set_vpp_min_lock_lately(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	struct v4l2_subdev *sd = NULL;
	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
			struct vpp_dev *vpp;
			sd = decon->mdev->vpp_sd[win->vpp_id];
			vpp = v4l2_get_subdevdata(sd);
			if (vpp->cur_bw < vpp->prev_bw) {
				v4l2_subdev_call(sd, core, ioctl,
						VPP_SET_BW,
						&regs->vpp_config[i]);
			}
			vpp->prev_bw = vpp->cur_bw;

			v4l2_subdev_call(sd, core, ioctl,
					VPP_SET_ROT_MIF,
					&regs->vpp_config[i]);

			if (decon->disp_cur < decon->disp_prev) {
				SYSTRACE_C_BEGIN("pm_qos_update_request");
				pm_qos_update_request(&decon->disp_qos, decon->disp_cur);
				pm_qos_update_request(&decon->int_qos, decon->disp_cur);
				SYSTRACE_C_FINISH("pm_qos_update_request");
			}
			decon->disp_prev = decon->disp_cur;

		}
	}
}

void decon_set_vpp_disp_min_lock(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	u64 disp_bw[4] = {0, 0, 0, 0};
	u64 disp_max_bw = 0;
	struct v4l2_subdev *sd = NULL;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
			struct vpp_dev *vpp;
			sd = decon->mdev->vpp_sd[win->vpp_id];
			vpp = v4l2_get_subdevdata(sd);

			/*
			 *  VPP0(G0), VPP1(G1), VPP2(VG0), VPP3(VG1),
			 *  VPP4(G2), VPP5(G3), VPP6(VGR0), VPP7(VGR1), VPP8(WB)
			 *  vpp_bus_bw[0] = DISP0_0_BW = G0 + VG0;
			 *  vpp_bus_bw[1] = DISP0_1_BW = G1 + VG1;
			 *  vpp_bus_bw[2] = DISP1_0_BW = G2 + VGR0;
			 *  vpp_bus_bw[3] = DISP1_1_BW = G3 + VGR1 + WB;
			 */
			if((vpp->id == 0) || (vpp->id == 2))
				disp_bw[0] = disp_bw[0] + vpp->disp_cur_bw;
			if((vpp->id == 1) || (vpp->id == 3))
				disp_bw[1] = disp_bw[1] + vpp->disp_cur_bw;
			if((vpp->id == 4) || (vpp->id == 6))
				disp_bw[2] = disp_bw[2] + vpp->disp_cur_bw;
			if((vpp->id == 5) || (vpp->id == 7) || (vpp->id == 8))
				disp_bw[3] = disp_bw[3] + vpp->disp_cur_bw;
		}
	}

	disp_max_bw = disp_bw[0];
	for (i = 0; i < 4; i++) {
		if (disp_max_bw < disp_bw[i])
			disp_max_bw = disp_bw[i];
	}
	decon->disp_cur = disp_max_bw;
}
#endif
static void __decon_update_regs(struct decon_device *decon, struct decon_reg_data *regs)
{
	unsigned short i, j;
	struct decon_mode_info psr;
	struct v4l2_subdev *sd = NULL;
	int plane_cnt;
	int vpp_ret = 0;
	int win_bitmap = 0;

	decon_to_psr_info(decon, &psr);
	decon_reg_wait_for_update_timeout(decon->id, SHADOW_UPDATE_TIMEOUT);
	if (psr.trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_DISABLE);

	/* TODO: check and wait until the required IDMA is free */
	decon_reg_chmap_validate(decon, regs);

#ifdef CONFIG_FB_WINDOW_UPDATE
	/* if any error in config, skip the trigger UNAMSK. keep others as it is*/
	if (decon->pdata->out_type == DECON_OUT_DSI)
		decon_reg_set_win_update_config(decon, regs);
#endif

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];

		if (regs->win_regs[i].wincon & WIN_CONTROL_EN_F)
			win_bitmap |= 1 << i;

		/* set decon registers for each window */
		decon_reg_set_window_control(decon->id, i, &regs->win_regs[i],
							regs->win_regs[i].winmap_state);

		/* set plane data */
		plane_cnt = decon_get_plane_cnt(regs->vpp_config[i].format);
		for (j = 0; j < plane_cnt; ++j)
			decon->windows[i]->dma_buf_data[j] = regs->dma_buf_data[i][j];

		decon->windows[i]->plane_cnt = plane_cnt;
		if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
			sd = decon->mdev->vpp_sd[win->vpp_id];
			vpp_ret = v4l2_subdev_call(sd, core, ioctl,
					VPP_WIN_CONFIG, &regs->vpp_config[i]);
			if (vpp_ret) {
				decon_err("Failed to config VPP-%d\n", win->vpp_id);
				regs->win_regs[i].wincon &= (~WIN_CONTROL_EN_F);
				decon_write(decon->id, WIN_CONTROL(i), regs->win_regs[i].wincon);
				decon->vpp_usage_bitmask &= ~(1 << win->vpp_id);
				decon->vpp_err_stat[win->vpp_id] = true;
			}
		}
	}

	if (decon->pdata->out_type == DECON_OUT_WB) {
		/* Resizing related with data path */
		if (decon->lcd_info->xres != regs->vpp_config[ODMA_WB].dst.w
				|| decon->lcd_info->yres != regs->vpp_config[ODMA_WB].dst.h)
		{
			decon->lcd_info->xres = regs->vpp_config[ODMA_WB].dst.w;
			decon->lcd_info->yres = regs->vpp_config[ODMA_WB].dst.h;
			decon_reg_set_partial_update(decon->id, 0, decon->lcd_info);
		}

		decon->vpp_usage_bitmask |= (1 << ODMA_WB);
		decon->vpp_used[ODMA_WB] = true;
		sd = decon->mdev->vpp_sd[ODMA_WB];
		if (v4l2_subdev_call(sd, core, ioctl, VPP_WIN_CONFIG,
				&regs->vpp_config[MAX_DECON_WIN])) {
			decon_err("Failed to config VPP-%d\n", ODMA_WB);
			decon->vpp_usage_bitmask &= ~(1 << ODMA_WB);
			decon->vpp_err_stat[ODMA_WB] = true;
		} else
			DISP_SS_EVENT_LOG(DISP_EVT_WB_SW_TRIGGER, &decon->sd, ktime_set(0, 0));
	}

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
		/* NOTE: Must be called after VPP_WIN_CONFIG
		 * calculate bandwidth and update min lock(MIF, INT, DISP) */
		decon->num_of_win = regs->num_of_window;
		decon->bts2_ops->bts_calc_bw(decon);
		decon->bts2_ops->bts_update_bw(decon, 0);
#else
		decon_get_vpp_min_lock(decon, regs);
		decon_set_vpp_disp_min_lock(decon, regs);
		decon_set_vpp_min_lock_early(decon, regs);
#endif

	/* DMA protection(SFW) enable must be happen on vpp domain is alive */
	decon_set_protected_content(decon, regs);
	decon_cfw_buf_ctrl(decon, regs, SMC_DRM_SECBUF_CFW_PROT);

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
	mdnie_reg_update_frame(decon->lcd_info->xres, decon->lcd_info->yres);
#endif

	decon_reg_all_win_shadow_update_req(decon->id);
	decon_to_psr_info(decon, &psr);
	if (decon_reg_start(decon->id, &psr) < 0)
		BUG();
	DISP_SS_EVENT_LOG(DISP_EVT_TRIG_UNMASK, &decon->sd, ktime_set(0, 0));
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	if (!decon->id) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL);
		if (ret)
			decon_err("Failed to call DSIM packet go enable in %s!\n", __func__);
	}
#endif
}

static void decon_fence_wait(struct sync_fence *fence)
{
	int err = sync_fence_wait(fence, 900);
	if (err >= 0)
		return;

	if (err < 0)
		decon_warn("error waiting on acquire fence: %d\n", err);
}

int decon_wait_until_size_match(struct decon_device *decon,
		int dsi_idx, unsigned long timeout)
{
	unsigned long delay_time = 100;
	unsigned long cnt = timeout / delay_time;
	u32 decon_yres, dsim_yres;
	u32 decon_xres, dsim_xres;
	u32 need_save = true;
	struct disp_ss_size_info info;

	if ((decon->pdata->psr_mode == DECON_VIDEO_MODE) ||
		(decon->pdata->out_type != DECON_OUT_DSI))
		return 0;

	while (--cnt) {
		/* Check a DECON and DSIM size mismatch */
		decon_yres = decon_reg_get_height(decon->id, decon->pdata->dsi_mode);
		dsim_yres = dsim_reg_get_yres(dsi_idx);

		decon_xres = decon_reg_get_width(decon->id, decon->pdata->dsi_mode);
		dsim_xres = dsim_reg_get_xres(dsi_idx);

		if (decon_yres == dsim_yres && decon_xres == dsim_xres)
			goto wait_done;

		if (need_save) {
			/* TODO: Save a err data */
			info.w_in = decon_xres;
			info.h_in = decon_yres;
			info.w_out = dsim_xres;
			info.h_out = dsim_yres;
			DISP_SS_EVENT_SIZE_ERR_LOG(&decon->sd, &info);
			need_save = false;
		}

		udelay(delay_time);
	}

	if (!cnt) {
		decon_err("size mis-match, HW_SW_TRIG_CONTROL:0x%x decon_xres:%d,	\
				dsim_xres:%d, decon_yres:%d, dsim_yres:%d\n",
				decon_read(decon->id, HW_SW_TRIG_CONTROL),
				decon_xres, dsim_xres, decon_yres, dsim_yres);
	}
wait_done:
	return 0;
}

void decon_wait_for_vstatus(struct decon_device *decon, u32 timeout)
{
	int ret;

	if (decon->id)
		return;

	ret = wait_event_timeout(decon->wait_vstatus,
			(decon->frame_start_cnt_target <= decon->frame_start_cnt_cur),
			msecs_to_jiffies(timeout));
	if (!ret)
		decon_warn("%s:timeout\n", __func__);
}

static void __decon_update_clear(struct decon_device *decon, struct decon_reg_data *regs)
{
	unsigned short i, j;
	int plane_cnt;

	for (i = 0; i < decon->pdata->max_win; i++) {
		/* set plane data */
		plane_cnt = decon_get_plane_cnt(regs->vpp_config[i].format);
		for (j = 0; j < plane_cnt; ++j)
			decon->windows[i]->dma_buf_data[j] = regs->dma_buf_data[i][j];

		decon->windows[i]->plane_cnt = plane_cnt;
	}

	return;
}

static void decon_update_regs(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct decon_dma_buf_data old_dma_bufs[decon->pdata->max_win][MAX_BUF_PLANE_CNT];
	int old_plane_cnt[MAX_DECON_WIN];
	struct decon_mode_info psr;
	int i, j;
#ifdef CONFIG_USE_VSYNC_SKIP
	int vsync_wait_cnt = 0;
#endif /* CONFIG_USE_VSYNC_SKIP */
	char strace_str[20];
	struct dsim_device *dsim = NULL;

	if( !decon->systrace_pid ) decon->systrace_pid = current->pid;
	decon->tracing_mark_write( decon->systrace_pid, 'B', "decon_update_regs", 0 );

	decon->cur_frame_has_yuv = 0;

	if (decon->state == DECON_STATE_LPD)
		decon_exit_lpd(decon);

	for (i = 0; i < decon->pdata->max_win; i++) {
		for (j = 0; j < MAX_BUF_PLANE_CNT; ++j)
			memset(&old_dma_bufs[i][j], 0, sizeof(struct decon_dma_buf_data));
		old_plane_cnt[i] = 0;
	}

	decon->tracing_mark_write( decon->systrace_pid, 'B', "decon_fence_wait", 0 );
	for (i = 0; i < decon->pdata->max_win; i++) {
		if (decon->pdata->out_type == DECON_OUT_WB)
			old_plane_cnt[i] = decon_get_plane_cnt(regs->vpp_config[i].format);
		else
			old_plane_cnt[i] = decon->windows[i]->plane_cnt;
		for (j = 0; j < old_plane_cnt[i]; ++j)
			old_dma_bufs[i][j] = decon->windows[i]->dma_buf_data[j];
		if (regs->dma_buf_data[i][0].fence) {
			sprintf( strace_str, "decon_fence_wait%d", i );
			decon->tracing_mark_write( decon->systrace_pid, 'B', strace_str, 0 );

			decon_fence_wait(regs->dma_buf_data[i][0].fence);

			decon->tracing_mark_write( decon->systrace_pid, 'E', strace_str, 0 );
		}
	}
	decon->tracing_mark_write( decon->systrace_pid, 'E', "decon_fence_wait", 0 );

	decon_check_vpp_used(decon, regs);
	DISP_SS_EVENT_LOG_WINCON(&decon->sd, regs);

#ifdef CONFIG_USE_VSYNC_SKIP
	if (decon->pdata->out_type == DECON_OUT_DSI) {
		vsync_wait_cnt = decon_extra_vsync_wait_get();
		decon_extra_vsync_wait_set(0);

		if (vsync_wait_cnt < ERANGE && regs->num_of_window <= 2) {
			while ((vsync_wait_cnt--) > 0) {
				if ((decon_extra_vsync_wait_get() >= ERANGE)) {
					decon_extra_vsync_wait_set(0);
					break;
				}

				decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
			}
		}
	}
#endif /* CONFIG_USE_VSYNC_SKIP */

	decon_to_psr_info(decon, &psr);
	if (regs->num_of_window) {
		__decon_update_regs(decon, regs);
	} else {
		__decon_update_clear(decon, regs);
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		goto end;
	}

	if (decon->pdata->out_type == DECON_OUT_WB) {
		decon_reg_release_resource(decon->id, &psr);
		decon_vpp_wait_wb_framedone(decon);
		/* Stop to prevent resource conflict */
		decon->vpp_usage_bitmask = 0;
		decon_set_protected_content(decon, NULL);
		decon_dbg("write-back timeline:%d, max:%d\n",
				decon->timeline->value, decon->timeline_max);
	} else {
		decon->frame_start_cnt_target = decon->frame_start_cnt_cur + 1;
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		decon->tracing_mark_write( decon->systrace_pid, 'E', "decon_update_regs", 0 );
		decon_wait_for_vstatus(decon, 50);
		if (decon_reg_wait_for_update_timeout(decon->id, SHADOW_UPDATE_TIMEOUT) < 0) {
			decon_dump(decon);
			BUG();
		}

		/* wait until decon & dsim size matches */
		decon_wait_until_size_match(decon, decon->pdata->out_idx, 50 * 1000); /* 50ms */

		if (!decon->id) {
			/* clear I80 Framedone pending interrupt */
			decon_write_mask(decon->id, INTERRUPT_PENDING, ~0,
				INTERRUPT_FRAME_DONE_INT_EN);
			decon->frame_done_cnt_target = decon->frame_done_cnt_cur + 1;
		}
		decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_DISABLE);
	}

end:
	DISP_SS_EVENT_LOG(DISP_EVT_TRIG_MASK, &decon->sd, ktime_set(0, 0));
	decon->trig_mask_timestamp =  ktime_get();
	for (i = 0; i < decon->pdata->max_win; i++) {
		for (j = 0; j < old_plane_cnt[i]; ++j)
			if (decon->pdata->out_type == DECON_OUT_WB)
				decon_free_dma_buf(decon, &regs->dma_buf_data[i][j]);
			else
				decon_free_dma_buf(decon, &old_dma_bufs[i][j]);
	}

	if (decon->pdata->out_type == DECON_OUT_WB) {
		old_plane_cnt[0] = decon_get_plane_cnt(regs->vpp_config[MAX_DECON_WIN].format);
		for (j = 0; j < old_plane_cnt[0]; ++j)
			decon_free_dma_buf(decon, &regs->dma_buf_data[MAX_DECON_WIN][j]);
	}

	sw_sync_timeline_inc(decon->timeline, 1);
	decon_cfw_buf_release(decon, regs);
	decon_vpp_stop(decon, false);


	if (CHECK_DSI0_NEED_DP_ON(decon->dispon_req)) {
		decon_info("%s : decon_req dsi0 display on \n", __func__);
		if (regs->which_dsi & WIN_DSI0_UPDATE) {
			if (decon->output_sd) {
				dsim = container_of(decon->output_sd, struct dsim_device, sd);
				call_panel_ops(dsim, displayon, dsim);
				decon_info("decon update : dsi 0\n");
				decon->dispon_req = DSI0_CLEAR_DP_ON(decon->dispon_req);
			}
		}
	}

	if (CHECK_DSI1_NEED_DP_ON(decon->dispon_req)) {
		decon_info("%s : decon_req dsi1 display on \n", __func__);
		if (regs->which_dsi & WIN_DSI1_UPDATE) {
			if (decon->output_sd1) {
				dsim = container_of(decon->output_sd1, struct dsim_device, sd);
				call_panel_ops(dsim, displayon, dsim);
				decon_info("decon update : dsi 1\n");
				decon->dispon_req = DSI1_CLEAR_DP_ON(decon->dispon_req);
			}
		}
	}

	switch(regs->which_dsi) {
		case WIN_DSI1_UPDATE | WIN_DSI0_UPDATE :
			dsim_info("decon update regs : both side\n");
			break;
		case WIN_DSI1_UPDATE :
			dsim_info("decon update regs : sub side\n");
			break;
		case WIN_DSI0_UPDATE :
			dsim_info("decon update regs : main side\n");
			break;
	}
#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
	/* NOTE: Must be called after VPP_STOP
	 * update min lock(MIF, INT, DISP) */
	decon->prev_num_of_win = regs->num_of_window;
	decon->bts2_ops->bts_calc_bw(decon);
	decon->bts2_ops->bts_update_bw(decon, 1);
#else
	decon_set_vpp_min_lock_lately(decon, regs);
#endif

}

static void decon_update_regs_handler(struct kthread_work *work)
{
	struct decon_device *decon =
			container_of(work, struct decon_device, update_regs_work);
	struct decon_reg_data *data, *next;
	struct list_head saved_list;

	if (decon->state == DECON_STATE_LPD)
		decon_warn("%s: LPD state: %d\n", __func__, decon_get_lpd_block_cnt(decon));

	mutex_lock(&decon->update_regs_list_lock);
	saved_list = decon->update_regs_list;
	list_replace_init(&decon->update_regs_list, &saved_list);
	mutex_unlock(&decon->update_regs_list_lock);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		decon->tracing_mark_write( decon->systrace_pid, 'C', "update_regs_list", decon->update_regs_list_cnt);
		decon_update_regs(decon, data);
		decon_lpd_unblock(decon);
		list_del(&data->list);
		decon->tracing_mark_write( decon->systrace_pid, 'C', "update_regs_list", --decon->update_regs_list_cnt);
		kfree(data);
	}
}

static void decon_win_config_dump(struct decon_device *decon,
	struct decon_win_config *win_config)
{
	int i;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if (config->state != DECON_WIN_STATE_DISABLED)
		decon_dbg("win-%d, state:%d, format:%d, idma_type:%d, src[%d %d %d %d %d %d],"
			"dst[%d %d %d %d %d %d]", i, config->state, config->format, config->idma_type,
			config->src.x, config->src.y, config->src.w, config->src.h, config->src.f_w,
			config->src.f_h, config->dst.x, config->dst.y, config->dst.w, config->dst.h,
			config->dst.f_w, config->dst.f_h);
	}
}

static int decon_set_win_config(struct decon_device *decon,
		struct decon_win_config_data *win_data)
{
	struct decon_win_config *win_config = win_data->config;
	int ret = 0;
	unsigned short i, j;
	struct decon_reg_data *regs;
	struct sync_fence *fence;
	struct sync_pt *pt;
	int fd;
	unsigned int bw = 0;
	int plane_cnt = 0;

	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	mutex_lock(&decon->output_lock);

	if ((decon->state == DECON_STATE_OFF) || (decon->ignore_vsync == true) ||
		(decon->pdata->out_type == DECON_OUT_TUI)) {
		decon->timeline_max++;
		pt = sw_sync_pt_create(decon->timeline, decon->timeline_max);
		fence = sync_fence_create("display", pt);
		sync_fence_install(fence, fd);
		win_data->fence = fd;

		sw_sync_timeline_inc(decon->timeline, 1);
		goto err;
	}

	regs = kzalloc(sizeof(struct decon_reg_data), GFP_KERNEL);
	if (!regs) {
		decon_err("could not allocate decon_reg_data\n");
		ret = -ENOMEM;
		goto err;
	}
	INIT_LIST_HEAD(&regs->sbuf_pend_list);

	for (i = 0; i < decon->pdata->max_win; i++) {
		decon->windows[i]->prev_fix =
			decon->windows[i]->fbinfo->fix;
		decon->windows[i]->prev_var =
			decon->windows[i]->fbinfo->var;

	}

#ifdef CONFIG_FB_WINDOW_UPDATE
	if (decon->pdata->out_type == DECON_OUT_DSI)
		decon_set_win_update_config(decon, win_config, regs);
#endif
	decon_win_config_dump(decon, win_config);
	for (i = 0; i < decon->pdata->max_win && !ret; i++) {
		struct decon_win_config *config = &win_config[i];
		struct decon_win *win = decon->windows[i];
		struct decon_window_regs *win_regs = &regs->win_regs[win->index];

		bool enabled = 0;
		bool color_map = true;
		u32 color = 0;

		switch (config->state) {
		case DECON_WIN_STATE_DISABLED:
			break;
		case DECON_WIN_STATE_COLOR:
			enabled = 1;
			color = config->color;
			decon_win_conig_to_regs_param(0, config, win_regs, IDMA_G0);
			break;
		case DECON_WIN_STATE_BUFFER:
			if (!decon->id && ((config->idma_type == IDMA_G0) &&
					(i != MAX_DECON_WIN - 1))) {
				decon_info("%s: idma_type %d win-id %d\n",
						__func__, config->idma_type, i);
				break;
			}
			if (IS_ENABLED(CONFIG_DECON_BLOCKING_MODE))
				if (decon_set_win_blocking_mode(decon, win, win_config, regs))
					break;

			ret = decon_set_win_buffer(decon, win, config, regs);
			if (!ret) {
				enabled = 1;
				color_map = false;
			}
			break;
		default:
			decon_warn("unrecognized window state %u",
					config->state);
			ret = -EINVAL;
			break;
		}
		if (enabled) {
			win_regs->wincon |= WIN_CONTROL_EN_F;
			/*
			 * Both of DECON_WIN_STATE_BUFFER and DECON_WIN_STATE_COLOR
			 * should count number of windows.
			 */
			regs->num_of_window++;
		} else {
			win_regs->wincon &= ~WIN_CONTROL_EN_F;
		}

		if (color_map) {
			win_regs->colormap = color;
			win_regs->winmap_state = color_map;
		}

		if (enabled && config->state == DECON_WIN_STATE_BUFFER) {
			bw += decon_calc_bandwidth(config->dst.w, config->dst.h,
					win->fbinfo->var.bits_per_pixel,
					win->fps);
		}
	}

	if (decon->pdata->out_type == DECON_OUT_WB)
		ret = decon_set_wb_buffer(decon, win_config, regs);

	for (i = 0; i < MAX_VPP_SUBDEV; i++) {
		memcpy(&regs->vpp_config[i], &win_config[i],
				sizeof(struct decon_win_config));
		regs->vpp_config[i].format =
			get_vpp_src_format(regs->vpp_config[i].format, win_config[i].idma_type);
	}
	regs->bandwidth = bw;

	decon_dbg("Total BW = %d Mbits, Max BW per window = %d Mbits\n",
			bw / (1024 * 1024), MAX_BW_PER_WINDOW / (1024 * 1024));

	if (ret) {
#ifdef CONFIG_FB_WINDOW_UPDATE
		if (regs->need_update)
			decon_win_update_rect_reset(decon);
#endif
		for (i = 0; i < decon->pdata->max_win; i++) {
			decon->windows[i]->fbinfo->fix = decon->windows[i]->prev_fix;
			decon->windows[i]->fbinfo->var = decon->windows[i]->prev_var;

			plane_cnt = decon_get_plane_cnt(regs->vpp_config[i].format);
			for (j = 0; j < plane_cnt; ++j)
				decon_free_dma_buf(decon, &regs->dma_buf_data[i][j]);
		}
		if (decon->pdata->out_type == DECON_OUT_WB) {
			plane_cnt = decon_get_plane_cnt(regs->vpp_config[MAX_DECON_WIN].format);
			for (j = 0; j < plane_cnt; ++j)
				decon_free_dma_buf(decon, &regs->dma_buf_data[MAX_DECON_WIN][j]);
		}
		put_unused_fd(fd);
		kfree(regs);
		win_data->fence = -1;
	} else {
		decon_lpd_block(decon);
		decon->timeline_max++;
		if (regs->num_of_window) {
			pt = sw_sync_pt_create(decon->timeline, decon->timeline_max);
			fence = sync_fence_create("display", pt);
			sync_fence_install(fence, fd);
			win_data->fence = fd;
		} else {
			win_data->fence = -1;
			put_unused_fd(fd);
#ifdef CONFIG_FB_WINDOW_UPDATE
			if (regs->need_update)
				decon_win_update_rect_reset(decon);
#endif
		}
		DISP_SS_EVENT_LOG_WINCON2(&decon->sd, regs);
		mutex_lock(&decon->update_regs_list_lock);
		list_add_tail(&regs->list, &decon->update_regs_list);
		decon->update_regs_list_cnt++;
		mutex_unlock(&decon->update_regs_list_lock);
		queue_kthread_work(&decon->update_regs_worker,
				&decon->update_regs_work);
	}
err:
	mutex_unlock(&decon->output_lock);
	return ret;
}


#ifdef CONFIG_EXYNOS_DECON_DUAL_DSI

#define	REQ_DUAL_DSI0_ON	0x0
#define REQ_DUAL_DSI1_ON	0x1
#define REQ_DUAL_DSI0_OFF	0x2
#define REQ_DUAL_DSI1_OFF	0x3
#define REQ_DUAL_MAX		0x4

#define CUR_DUAL_DSI0_OFF_DSI1_OFF	0x0
#define CUR_DUAL_DSI0_OFF_DSI1_ON	0x1
#define CUR_DUAL_DSI0_ON_DSI1_OFF	0x2
#define CUR_DUAL_DSI0_ON_DSI1_ON	0x3
#define CUR_DUAL_MAX				0x4


unsigned int dual_dsi_fsm[REQ_DUAL_MAX][CUR_DUAL_MAX] = {
	/* Req DSI0 Turn On */
	{
		/* dsi0 off, dsi1 off -> (dsi0 on req)-> dsi0 on, dsi1 off */
		DECON_SET_STAT((DECON_STAT_UPDATE|DECON_STAT_ON)) |
		TE_SET_STAT((TE_STAT_UPDATE|TE_STAT_DSI0)) |
		DSI0_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_ALONE)) |
		DSI1_SET_STAT(DSI_STAT_OFF),
		/* dsi0 off, dsi1 on -> (dsi0 on req)-> dsi0 on, dsi1 on */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT(TE_STAT_DSI1) |
		DSI0_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_SLAVE)) |
		DSI1_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_MASTER)),
		/* dsi0 on, dsi1 off -> (dsi0 on req) -> dsi0 on, dsi1 off */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT(TE_STAT_DSI0) |
		DSI0_SET_STAT(DSI_STAT_ALONE) |
		DSI1_SET_STAT(DSI_STAT_OFF),
		/* dsi0 on, dsi1 on -> (dsi0 on req) -> dsi0 on, dsi1 on */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT(TE_STAT_DSI0) | /* don't care */
		DSI0_SET_STAT(DSI_STAT_MASTER) |
		DSI1_SET_STAT(DSI_STAT_SLAVE),
	},
	/* Req DSI1 Turn On */
	{
		/* dsi0 off, dsi1 off -> (dsi1 on req)-> dsi0 off, dsi1 on */
		DECON_SET_STAT((DECON_STAT_UPDATE | DECON_STAT_ON)) |
		TE_SET_STAT((TE_STAT_UPDATE | TE_STAT_DSI1)) |
		DSI0_SET_STAT(DSI_STAT_OFF) |
		DSI1_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_ALONE)),
		/* dsi0 off, dsi1 on -> (dsi1 on req)-> dsi0 off, dsi1 on */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT(TE_STAT_DSI1) |
		DSI0_SET_STAT(DSI_STAT_OFF) |
		DSI1_SET_STAT(DSI_STAT_ALONE),
		/* dsi0 on, dsi1 off -> (dsi1 on req) -> dsi0 on, dsi1 on */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT(TE_STAT_DSI0) |
		DSI0_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_MASTER)) |
		DSI1_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_SLAVE)),
		/* dsi0 on, dsi1 on -> (dsi1 on req) -> dsi0 on, dsi1 on */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT(TE_STAT_DSI0) | /* don't care */
		DSI0_SET_STAT(DSI_STAT_MASTER) |
		DSI1_SET_STAT(DSI_STAT_SLAVE),
	},
	/* Req DSI0 Turn Off */
	{
	/* dsi0 off, dsi1 off -> (dsi0 Off req)-> dsi0 off, dsi1 off */
		DECON_SET_STAT(DECON_STAT_OFF) |
		TE_SET_STAT(TE_STAT_DSI0) |
		DSI0_SET_STAT(DSI_STAT_OFF) |
		DSI1_SET_STAT(DSI_STAT_OFF),
		/* dsi0 off, dsi1 on -> (dsi0 Off req)-> dsi0 off, dsi1 on */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT(TE_STAT_DSI1) |
		DSI0_SET_STAT(DSI_STAT_OFF) |
		DSI1_SET_STAT(DSI_STAT_ALONE),
		/* dsi0 on, dsi1 off -> (dsi0 Off req) -> dsi0 off, dsi1 off */
		DECON_SET_STAT((DECON_STAT_UPDATE | DECON_STAT_OFF)) |
		TE_SET_STAT(TE_STAT_DSI0) |
		DSI0_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_OFF)) |
		DSI1_SET_STAT(DSI_STAT_OFF),
		/* dsi0 on, dsi1 on -> (dsi0 Off req) -> dsi0 off, dsi1 on */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT((TE_STAT_UPDATE | TE_STAT_DSI1)) |
		DSI0_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_OFF)) |
		DSI1_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_ALONE)),
	},
	/* Req DSI1 Turn Off */
	{
		/* dsi0 off, dsi1 off -> (dsi1 off req)-> dsi0 off, dsi1 off */
		DECON_SET_STAT(DECON_STATE_OFF) |
		TE_SET_STAT(TE_STAT_DSI0) |
		DSI0_SET_STAT(DSI_STAT_OFF) |
		DSI1_SET_STAT(DSI_STAT_OFF),
		/* dsi0 off, dsi1 on -> (dsi1 off req)-> dsi0 off, dsi1 off */
		DECON_SET_STAT((DECON_STAT_UPDATE | DECON_STAT_OFF)) |
		TE_SET_STAT(TE_STAT_DSI1) |
		DSI0_SET_STAT(DSI_STAT_OFF) |
		DSI1_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_OFF)),
		/* dsi0 on, dsi1 off -> (dsi1 off req) -> dsi0 on, dsi1 off */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT(TE_STAT_DSI0) |
		DSI0_SET_STAT(DSI_STAT_ALONE) |
		DSI1_SET_STAT(DSI_STAT_OFF),
		/* dsi0 on, dsi1 on -> (dsi1 off req) -> dsi0 on, dsi1 off */
		DECON_SET_STAT(DECON_STAT_ON) |
		TE_SET_STAT((TE_STAT_UPDATE | TE_STAT_DSI0)) | /* don't care */
		DSI0_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_ALONE)) |
		DSI1_SET_STAT((DSI_STAT_UPDATE | DSI_STAT_OFF)),
	},
};


#define DUAL_DSI_DEBUG

int decon_dual_get_next_stat(unsigned int cur_stat, unsigned int req_idx, unsigned int *result)
{
	unsigned int cur_idx = 0;
	unsigned int next_stat = 0;

	cur_idx = (DSI0_GET_STAT(cur_stat) == DSI_STAT_OFF ? 0 : (0x01 << 1)) |
			(DSI1_GET_STAT(cur_stat) == DSI_STAT_OFF ? 0 : 0x01);

	decon_info("cur_idx : %d, next_idx : %d\n", cur_idx, req_idx);

	if ((req_idx >= REQ_DUAL_MAX) || (cur_idx >= CUR_DUAL_MAX)) {
		decon_err("ERR : %s : invalid input : req : %d, cur : %d\n", __func__, req_idx, cur_idx);
		goto get_next_stat_err;
	}

	next_stat = dual_dsi_fsm[req_idx][cur_idx];

#ifdef DUAL_DSI_DEBUG
	decon_info("=================== DUAL DSI BLANK ===================\n");
	decon_info("Cur Decon(%x) : %s(%d), DSI0 : %s(%d), DSI1 : %s(%d)\n\n", cur_stat,
	DECON_GET_STAT(cur_stat) == DECON_STAT_OFF ? "off" : "on", TE_GET_STAT(cur_stat),
	DSI0_GET_STAT(cur_stat) == DSI_STAT_OFF ? "off" : "on", DSI0_GET_STAT(cur_stat),
	DSI1_GET_STAT(cur_stat) == DSI_STAT_OFF ? "off" : "on", DSI1_GET_STAT(cur_stat));
	decon_info("Next Decon(%x) : %s(%d), DSI0 : %s(%d), DSI1 : %s(%d)\n", next_stat,
	DECON_GET_STAT(next_stat) == DECON_STAT_OFF ? "off" : "on", TE_GET_STAT(next_stat),
	DSI0_GET_STAT(next_stat) == DSI_STAT_OFF ? "off" : "on", DSI0_GET_STAT(next_stat),
	DSI1_GET_STAT(next_stat) == DSI_STAT_OFF ? "off" : "on", DSI1_GET_STAT(next_stat));
	decon_info("======================================================\n");
#endif

	*result = next_stat;
	return 0;

get_next_stat_err:
	decon_err("ERR : %s \n", __func__);
	return -1;
}


int notify_dual_dsi_status(unsigned int status)
{
	int main_status = 0;
	int sub_status = 0;

	int iRet = 0;
	int lcd_state = 1;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	if (DSI0_GET_STAT(status) != DSI_STAT_OFF)
		main_status = 1;

	if (DSI1_GET_STAT(status) != DSI_STAT_OFF)
		sub_status = 1;

	if (main_status == 0 &&  sub_status == 1) {

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		cal_filp = filp_open("sys/class/sensors/ssp_sensor/lcd_check_fold_state",
			O_CREAT | O_TRUNC | O_WRONLY, 0660);
		if (IS_ERR(cal_filp)) {
			decon_err("[SSP]: %s - Can't open lcd_check_fold_state file %d \n",
			__func__, IS_ERR(cal_filp));
			set_fs(old_fs);
			iRet = PTR_ERR(cal_filp);
			return iRet;
		}
		iRet = cal_filp->f_op->write(cal_filp, (char *)&lcd_state,
			sizeof(char), &cal_filp->f_pos);
		if (iRet != sizeof(char)) {
			decon_err("[SSP]: %s - Can't write lcd_check_fold_state file %d\n", __func__, iRet);
			iRet = -EIO;
		}
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
	}
	return iRet;
}


int decon_dual_dsi_blank(struct decon_device *decon, display_type_t type)
{
	int ret;
	unsigned int req_idx;
	unsigned int dsi_stat = 0;
	unsigned int next_stat = 0;

	if (type == DECON_PRIMARY_DISPLAY) {
		decon_info("DECON BLANK : DSI0 OFF Request\n");
		req_idx = REQ_DUAL_DSI0_OFF;
	} else if (type == DECON_SECONDARY_DISPLAY) {
		decon_info("DECON BLANK : DSI1 OFF Request\n");
		req_idx = REQ_DUAL_DSI1_OFF;
	} else {
		decon_err("ERR:%s:undefined blank type : %d\n", __func__, type);
		goto dsi_blank_err;
	}

	ret = decon_dual_get_next_stat(decon->dual_status, req_idx, &next_stat);
	if (ret) {
		decon_err("ERR:%s:failed to get next stat req_idx : %d\n", __func__, req_idx);
		goto dsi_blank_err;
	}

	if (CHECK_DECON_UPDATE(next_stat)) {
		if (DECON_GET_STAT(next_stat) == DECON_STAT_OFF)
			decon_disable(decon);
	}


	if (CHECK_DSI0_UPDATE(next_stat)) {
		dsi_stat = DSI0_GET_STAT(next_stat);
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
					DSIM_SET_POWER_STATE, (unsigned int *)&dsi_stat);
	}
	if (CHECK_DSI1_UPDATE(next_stat)) {
		dsi_stat = DSI1_GET_STAT(next_stat);
		ret = v4l2_subdev_call(decon->output_sd1, core, ioctl,
					DSIM_SET_POWER_STATE, (unsigned int *)&dsi_stat);
	}
	decon->dual_status = next_stat;

	notify_dual_dsi_status(decon->dual_status);

	return ret;

dsi_blank_err:
	return -1;
}

int decon_dual_dsi_unblank(struct decon_device *decon, display_type_t type)
{
	int ret;
	unsigned int req_idx;
	unsigned int dsi_stat = 0;
	unsigned int next_stat = 0;

	if (type == DECON_PRIMARY_DISPLAY) {
		decon_info("DECON BLANK : DSI0 ON Request\n");
		req_idx = REQ_DUAL_DSI0_ON;
	} else if (type == DECON_SECONDARY_DISPLAY) {
		decon_info("DECON BLANK : DSI1 ON Request\n");
		req_idx = REQ_DUAL_DSI1_ON;
	} else {
		decon_err("ERR:%s:undefined blank type : %d\n", __func__, type);
		goto dsi_unblank_err;
	}

	ret = decon_dual_get_next_stat(decon->dual_status, req_idx, &next_stat);
	if (ret) {
		decon_err("ERR:%s:failed to get next stat req_idx : %d\n", __func__, req_idx);
		goto dsi_unblank_err;
	}

	if (CHECK_DECON_UPDATE(next_stat)){
		if (DECON_GET_STAT(next_stat) == DECON_STAT_ON)
			decon_enable(decon);
	}

	if (CHECK_DSI0_UPDATE(next_stat)) {
		dsi_stat = DSI0_GET_STAT(next_stat);
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
					DSIM_SET_POWER_STATE, (unsigned int *)&dsi_stat);
		if (DSI0_GET_STAT(decon->dual_status) == DSI_STAT_OFF) {
			decon->dispon_req = DSI0_REQ_DP_ON(decon->dispon_req);
		}
	}

	if (CHECK_DSI1_UPDATE(next_stat)) {
		dsi_stat = DSI1_GET_STAT(next_stat);
		ret = v4l2_subdev_call(decon->output_sd1, core, ioctl,
					DSIM_SET_POWER_STATE, (unsigned int *)&dsi_stat);
		if (DSI1_GET_STAT(decon->dual_status) == DSI_STAT_OFF) {
			decon->dispon_req = DSI1_REQ_DP_ON(decon->dispon_req);
		}
	}

	decon->dual_status = next_stat;
	return ret;

dsi_unblank_err:
	return -1;
}


int decon_set_dual_blank(struct decon_device *decon, struct dual_blank_data *blank_data)
{
	int ret = 0;
	struct dual_blank_data data;
	//int next_stat;

	decon_info("dual blank : blank : %d, type : %d\n", blank_data->blank, blank_data->type);


	memcpy(&data, blank_data, sizeof (struct dual_blank_data));


	switch (blank_data->blank) {
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_NORMAL:
			DISP_SS_EVENT_LOG(DISP_EVT_BLANK, &decon->sd, ktime_set(0, 0));
			ret = decon_dual_dsi_blank(decon, blank_data->type);
			break;
		case FB_BLANK_UNBLANK:
			DISP_SS_EVENT_LOG(DISP_EVT_UNBLANK, &decon->sd, ktime_set(0, 0));
			ret = decon_dual_dsi_unblank(decon, blank_data->type);
			break;

		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:

		default:
			decon_err("ERR:DECON:%s:undefined blank type : %d\n", __func__, blank_data->blank);
			goto blank_err;
	}
	decon_notifier_call_chain(blank_data->blank, &data);

	return ret;

blank_err:
	return -1;
}
#endif


static int decon_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	int ret;
	u32 crtc;
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
#ifdef CONFIG_EXYNOS_DECON_DUAL_DSI
	struct dual_blank_data blank_data;
#endif
	int systrace_cnt = 0;

	SYSTRACE_C_MARK( "decon_ioctl", ++systrace_cnt );

	SYSTRACE_C_MARK( "decon_ioctl", ++systrace_cnt );
	decon_lpd_block_exit(decon);
	SYSTRACE_C_MARK( "decon_ioctl", --systrace_cnt );

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		if (get_user(crtc, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}
		if (crtc == 0)
			ret = decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		else
			ret = -ENODEV;

		break;

	case S3CFB_WIN_POSITION:
		if (copy_from_user(&decon->ioctl_data.user_window,
				(struct decon_user_window __user *)arg,
				sizeof(decon->ioctl_data.user_window))) {
			ret = -EFAULT;
			break;
		}

		if (decon->ioctl_data.user_window.x < 0)
			decon->ioctl_data.user_window.x = 0;
		if (decon->ioctl_data.user_window.y < 0)
			decon->ioctl_data.user_window.y = 0;

		ret = decon_set_window_position(info, decon->ioctl_data.user_window);
		break;

	case S3CFB_WIN_SET_PLANE_ALPHA:
		if (copy_from_user(&decon->ioctl_data.user_alpha,
				(struct s3c_fb_user_plane_alpha __user *)arg,
				sizeof(decon->ioctl_data.user_alpha))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_plane_alpha_blending(info, decon->ioctl_data.user_alpha);
		break;

	case S3CFB_WIN_SET_CHROMA:
		if (copy_from_user(&decon->ioctl_data.user_chroma,
				   (struct s3c_fb_user_chroma __user *)arg,
				   sizeof(decon->ioctl_data.user_chroma))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_chroma_key(info, decon->ioctl_data.user_chroma);
		break;

	case S3CFB_SET_VSYNC_INT:
		if (get_user(decon->ioctl_data.vsync, (int __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_vsync_int(info, decon->ioctl_data.vsync);
		break;

	case S3CFB_WIN_CONFIG:
		if (copy_from_user(&decon->ioctl_data.win_data,
				   (struct decon_win_config_data __user *)arg,
				   sizeof(decon->ioctl_data.win_data))) {
			ret = -EFAULT;
			break;
		}
		ret = decon_set_win_config(decon, &decon->ioctl_data.win_data);
		if (ret)
			break;

		if (copy_to_user(&((struct decon_win_config_data __user *)arg)->fence,
				 &decon->ioctl_data.win_data.fence,
				 sizeof(decon->ioctl_data.win_data.fence))) {
			ret = -EFAULT;
			break;
		}
		break;

#ifdef CONFIG_EXYNOS_DECON_DUAL_DSI
	case S3CFB_DUAL_DISPLAY_BLANK:
		if (copy_from_user(&blank_data,
				(struct dual_blank_data __user *)arg, sizeof(blank_data))) {
			ret = -EFAULT;
			decon_err("ERR:DECON:%s:failed to copy data from user\n", __func__);
			break;
		}
		ret = decon_set_dual_blank(decon, &blank_data);
		break;
#endif

	default:
		decon_err("%s : invalied command\n", __func__);
		ret = -ENOTTY;
	}
	SYSTRACE_C_MARK( "decon_ioctl", ++systrace_cnt );
	decon_lpd_unblock(decon);
	SYSTRACE_C_MARK( "decon_ioctl", --systrace_cnt );

	SYSTRACE_C_MARK( "decon_ioctl", --systrace_cnt );
	return ret;
}

int decon_release(struct fb_info *info, int user)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;

	decon_info("%s +\n", __func__);

	if (decon->id && decon->pdata->out_type == DECON_OUT_DSI) {
		find_subdev_mipi(decon, decon->pdata->out_idx);
		decon_info("output device of decon%d is changed to %s\n",
				decon->id, decon->output_sd->name);
	}

	decon_info("%s -\n", __func__);

	return 0;
}

#ifdef CONFIG_COMPAT
static int decon_compat_ioctl(struct fb_info *info, unsigned int cmd,
		unsigned long arg)
{
	arg = (unsigned long) compat_ptr(arg);
	return decon_ioctl(info, cmd, arg);
}
#endif

/* ---------- FREAMBUFFER INTERFACE ----------- */
static struct fb_ops decon_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= decon_check_var,
	.fb_set_par	= decon_set_par,
	.fb_blank	= decon_blank,
	.fb_setcolreg	= decon_setcolreg,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = decon_compat_ioctl,
#endif
	.fb_ioctl	= decon_ioctl,
	.fb_pan_display	= decon_pan_display,
	.fb_mmap	= decon_mmap,
	.fb_release	= decon_release,
};

/* ---------- POWER MANAGEMENT ----------- */
void decon_clocks_info(struct decon_device *decon)
{
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.pclk),
				clk_get_rate(decon->res.pclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.eclk_leaf),
				clk_get_rate(decon->res.eclk_leaf) / MHZ);
	if (decon->id != 2) {
		decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.vclk_leaf),
				clk_get_rate(decon->res.vclk_leaf) / MHZ);
	}
}

void decon_put_clocks(struct decon_device *decon)
{
	if (decon->id != 2) {
		clk_put(decon->res.dpll);
		clk_put(decon->res.vclk);
		clk_put(decon->res.vclk_leaf);
	}
	clk_put(decon->res.pclk);
	clk_put(decon->res.eclk);
	clk_put(decon->res.eclk_leaf);
}

static int decon_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct decon_device *decon = platform_get_drvdata(pdev);

	DISP_SS_EVENT_LOG(DISP_EVT_DECON_RESUME, &decon->sd, ktime_set(0, 0));
	decon_dbg("decon%d %s +\n", decon->id, __func__);
	mutex_lock(&decon->mutex);

	if (decon->id != 2) {
		clk_prepare_enable(decon->res.dpll);
		clk_prepare_enable(decon->res.vclk);
		clk_prepare_enable(decon->res.vclk_leaf);
	}
	clk_prepare_enable(decon->res.pclk);
	clk_prepare_enable(decon->res.eclk);
	clk_prepare_enable(decon->res.eclk_leaf);

	if (!decon->id)
		decon_f_set_clocks(decon);
	else if (decon->id == 2)
		decon_t_set_clocks(decon);
	else
		decon_s_set_clocks(decon);

	if (decon->state == DECON_STATE_INIT)
		decon_clocks_info(decon);

	mutex_unlock(&decon->mutex);
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

static int decon_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct decon_device *decon = platform_get_drvdata(pdev);

	DISP_SS_EVENT_LOG(DISP_EVT_DECON_SUSPEND, &decon->sd, ktime_set(0, 0));
	decon_dbg("decon%d %s +\n", decon->id, __func__);
	mutex_lock(&decon->mutex);

	clk_disable_unprepare(decon->res.eclk);
	clk_disable_unprepare(decon->res.eclk_leaf);
	clk_disable_unprepare(decon->res.pclk);
	if (decon->id != 2) {
		clk_disable_unprepare(decon->res.vclk);
		clk_disable_unprepare(decon->res.vclk_leaf);
		clk_disable_unprepare(decon->res.dpll);
	}

	mutex_unlock(&decon->mutex);
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

static const struct dev_pm_ops decon_pm_ops = {
	.runtime_suspend = decon_runtime_suspend,
	.runtime_resume	 = decon_runtime_resume,
};

/* ---------- MEDIA CONTROLLER MANAGEMENT ----------- */
static long decon_sd_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	long ret = 0;

	switch (cmd) {
	case DECON_IOC_LPD_EXIT_LOCK:
		decon_lpd_block_exit(decon);
		break;
	case DECON_IOC_LPD_UNLOCK:
		decon_lpd_unblock(decon);
		break;
	default:
		dev_err(decon->dev, "unsupported ioctl");
		ret = -EINVAL;
	}
	return ret;
}

static int decon_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int decon_s_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *format)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	struct decon_lcd *porch;

	if (format->pad != DECON_PAD_WB) {
		decon_err("it is possible only output format setting\n");
		return -EINVAL;
	}

	if (format->format.width * format->format.height > 3840 * 2160) {
		decon_err("size is bigger than 3840x2160\n");
		return -EINVAL;
	}

	porch = kzalloc(sizeof(struct decon_lcd), GFP_KERNEL);
	if (!porch) {
		decon_err("could not allocate decon_lcd for wb\n");
		return -ENOMEM;
	}

	decon->lcd_info = porch;
	decon->lcd_info->width = format->format.width;
	decon->lcd_info->height = format->format.height;
	decon->lcd_info->xres = format->format.width;
	decon->lcd_info->yres = format->format.height;
	decon->lcd_info->vfp = 2;
	decon->lcd_info->vbp = 20;
	decon->lcd_info->hfp = 20;
	decon->lcd_info->hbp = 20;
	decon->lcd_info->vsa = 2;
	decon->lcd_info->hsa = 20;
	decon->lcd_info->fps = 60;
	decon->pdata->out_type = DECON_OUT_WB;

	decon_info("decon-%d output size for writeback %dx%d\n", decon->id,
			decon->lcd_info->width, decon->lcd_info->height);

	return 0;
}

static const struct v4l2_subdev_core_ops decon_sd_core_ops = {
	.ioctl = decon_sd_ioctl,
};

static const struct v4l2_subdev_video_ops decon_sd_video_ops = {
	.s_stream = decon_s_stream,
};

static const struct v4l2_subdev_pad_ops	decon_sd_pad_ops = {
	.set_fmt = decon_s_fmt,
};

static const struct v4l2_subdev_ops decon_sd_ops = {
	.video = &decon_sd_video_ops,
	.core = &decon_sd_core_ops,
	.pad = &decon_sd_pad_ops,
};

static int decon_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations decon_entity_ops = {
	.link_setup = decon_link_setup,
};

static int decon_register_subdev_nodes(struct decon_device *decon,
					struct exynos_md *md)
{
	int ret = v4l2_device_register_subdev_nodes(&md->v4l2_dev);
	if (ret) {
		decon_err("failed to make nodes for subdev\n");
		return ret;
	}

	decon_dbg("Register V4L2 subdev nodes for DECON\n");

	return 0;

}

static int decon_create_links(struct decon_device *decon,
					struct exynos_md *md)
{
	int ret = 0;
	char err[80];
#ifdef CONFIG_EXYNOS_VPP
	int i, j;
	u32 flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED;
#endif

	decon_dbg("decon%d create links\n", decon->id);
	memset(err, 0, sizeof(err));

	/*
	 * Link creation : vpp -> decon-int and ext.
	 * All windos of decon should be connected to all
	 * VPP each other.
	 */
#ifdef CONFIG_EXYNOS_VPP
	for (i = 0; i < MAX_VPP_SUBDEV; ++i) {
		for (j = 0; j < decon->n_sink_pad; j++) {
			ret = media_entity_create_link(&md->vpp_sd[i]->entity,
					VPP_PAD_SOURCE, &decon->sd.entity,
					j, flags);
			if (ret) {
				snprintf(err, sizeof(err), "%s --> %s",
						md->vpp_sd[i]->entity.name,
						decon->sd.entity.name);
				return ret;
			}
		}
	}
	decon_info("vpp <-> decon links are created successfully\n");
#endif

	if (decon->pdata->out_type == DECON_OUT_DSI)
		ret = create_link_mipi(decon, decon->pdata->out_idx);

	return ret;
}

static void decon_unregister_entity(struct decon_device *decon)
{
	v4l2_device_unregister_subdev(&decon->sd);
}

static int decon_register_entity(struct decon_device *decon)
{
	struct v4l2_subdev *sd = &decon->sd;
	struct media_pad *pads = decon->pads;
	struct media_entity *me = &sd->entity;
	struct exynos_md *md;
	int i, n_pad, ret = 0;

	/* init DECON sub-device */
	v4l2_subdev_init(sd, &decon_sd_ops);
	sd->owner = THIS_MODULE;
	snprintf(sd->name, sizeof(sd->name), "exynos-decon%d", decon->id);

	/* DECON sub-device can be opened in user space */
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* init DECON sub-device as entity */
	n_pad = decon->n_sink_pad + decon->n_src_pad;
	for (i = 0; i < decon->n_sink_pad; i++)
		pads[i].flags = MEDIA_PAD_FL_SINK;
	for (i = decon->n_sink_pad; i < n_pad; i++)
		pads[i].flags = MEDIA_PAD_FL_SOURCE;

	me->ops = &decon_entity_ops;
	ret = media_entity_init(me, n_pad, pads, 0);
	if (ret) {
		decon_err("failed to initialize media entity\n");
		return ret;
	}

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get output media device\n");
		return -ENODEV;
	}

	ret = v4l2_device_register_subdev(&md->v4l2_dev, sd);
	if (ret) {
		decon_err("failed to register DECON subdev\n");
		return ret;
	}
	decon_dbg("%s entity init\n", sd->name);

	switch (decon->id) {
		/* All decons can be LINKED to DSIM0/1/2 */
	case 0:
		/* Fixed to DSIM0 or eDP */
		if (decon->pdata->out_type == DECON_OUT_DSI) {
			ret = find_subdev_mipi(decon, 0);
		} else {
			/* TODO */
			decon_err("Not Supported output media device\n");
			return -ENODEV;
		}

		break;
	case 1:
		/* Fixed to DSIM1 */
		if (decon->pdata->out_type == DECON_OUT_DSI) {
			ret = find_subdev_mipi(decon, decon->pdata->out_idx);
		}

		break;
	case 2:
		break;
	}

	return ret;
}

static void decon_release_windows(struct decon_win *win)
{
	if (win->fbinfo)
		framebuffer_release(win->fbinfo);
}

static int decon_fb_alloc_memory(struct decon_device *decon, struct decon_win *win)
{
	struct decon_fb_pd_win *windata = &win->windata;
	unsigned int real_size, virt_size, size;
	struct fb_info *fbi = win->fbinfo;
	struct ion_handle *handle;
	dma_addr_t map_dma;
	struct dma_buf *buf;
	void *vaddr;
	struct device *dev;
	unsigned int ret;

	dev_info(decon->dev, "allocating memory for display\n");

	real_size = windata->win_mode.videomode.xres * windata->win_mode.videomode.yres;
	virt_size = windata->virtual_x * windata->virtual_y;

	dev_info(decon->dev, "real_size=%u (%u.%u), virt_size=%u (%u.%u)\n",
		real_size, windata->win_mode.videomode.xres, windata->win_mode.videomode.yres,
		virt_size, windata->virtual_x, windata->virtual_y);

	size = (real_size > virt_size) ? real_size : virt_size;
	size *= (windata->max_bpp > 16) ? 32 : windata->max_bpp;
	size /= 8;

	fbi->fix.smem_len = size;
	size = PAGE_ALIGN(size);

	dev_info(decon->dev, "want %u bytes for window[%d]\n", size, win->index);

#if defined(CONFIG_ION_EXYNOS)
	handle = ion_alloc(decon->ion_client, (size_t)size, 0,
					EXYNOS_ION_HEAP_SYSTEM_MASK, 0);
	if (IS_ERR(handle)) {
		dev_err(decon->dev, "failed to ion_alloc\n");
		return -ENOMEM;
	}

	buf = ion_share_dma_buf(decon->ion_client, handle);
	if (IS_ERR_OR_NULL(buf)) {
		dev_err(decon->dev, "ion_share_dma_buf() failed\n");
		goto err_share_dma_buf;
	}

	vaddr = ion_map_kernel(decon->ion_client, handle);

	fbi->screen_base = vaddr;

	win->dma_buf_data[1].fence = NULL;
	win->dma_buf_data[2].fence = NULL;
	win->plane_cnt = 1;
	dev = decon->mdev->vpp_dev[decon->default_idma];
	ret = decon_map_ion_handle(decon, dev, &win->dma_buf_data[0],
			handle, buf, win->index);
	if (!ret)
		goto err_map;
	map_dma = win->dma_buf_data[0].dma_addr;

	dev_info(decon->dev, "alloated memory\n");
#else
	fbi->screen_base = dma_alloc_writecombine(decon->dev, size,
						  &map_dma, GFP_KERNEL);
	if (!fbi->screen_base)
		return -ENOMEM;

	dev_dbg(decon->dev, "mapped %x to %p\n",
		(unsigned int)map_dma, fbi->screen_base);

	memset(fbi->screen_base, 0x0, size);
#endif
	fbi->fix.smem_start = map_dma;

	dev_info(decon->dev, "fb start addr = 0x%x\n", (u32)fbi->fix.smem_start);

	return 0;

#ifdef CONFIG_ION_EXYNOS
err_map:
	dma_buf_put(buf);
err_share_dma_buf:
	ion_free(decon->ion_client, handle);
	return -ENOMEM;
#endif
}

static void decon_missing_pixclock(struct decon_fb_videomode *win_mode)
{
	u64 pixclk = 1000000000000ULL;
	u32 div;
	u32 width, height;

	width = win_mode->videomode.xres;
	height = win_mode->videomode.yres;

	div = width * height * (win_mode->videomode.refresh ? : 60);

	do_div(pixclk, div);
	win_mode->videomode.pixclock = pixclk;
}

static int decon_acquire_windows(struct decon_device *decon, int idx)
{
	struct decon_win *win;
	struct fb_info *fbinfo;
	struct fb_var_screeninfo *var;
	struct decon_lcd *lcd_info = NULL;
	int ret, i;

	decon_dbg("acquire DECON window%d\n", idx);

	fbinfo = framebuffer_alloc(sizeof(struct decon_win), decon->dev);
	if (!fbinfo) {
		decon_err("failed to allocate framebuffer\n");
		return -ENOENT;
	}

	win = fbinfo->par;
	decon->windows[idx] = win;
	var = &fbinfo->var;
	win->fbinfo = fbinfo;
	/*fbinfo->fbops = &decon_fb_ops;*/
	/*fbinfo->flags = FBINFO_FLAG_DEFAULT;*/
	win->decon = decon;
	win->index = idx;

	win->windata.default_bpp = 32;
	win->windata.max_bpp = 32;
	if (decon->pdata->out_type == DECON_OUT_DSI) {
		lcd_info = decon->lcd_info;
		win->windata.virtual_x = lcd_info->xres;
		win->windata.virtual_y = lcd_info->yres * 2;
		win->windata.width = lcd_info->xres;
		win->windata.height = lcd_info->yres;
		win->windata.win_mode.videomode.left_margin = lcd_info->hbp;
		win->windata.win_mode.videomode.right_margin = lcd_info->hfp;
		win->windata.win_mode.videomode.upper_margin = lcd_info->vbp;
		win->windata.win_mode.videomode.lower_margin = lcd_info->vfp;
		win->windata.win_mode.videomode.hsync_len = lcd_info->hsa;
		win->windata.win_mode.videomode.vsync_len = lcd_info->vsa;
		win->windata.win_mode.videomode.xres = lcd_info->xres;
		win->windata.win_mode.videomode.yres = lcd_info->yres;
		decon_missing_pixclock(&win->windata.win_mode);
	}

	for (i = 0; i < MAX_BUF_PLANE_CNT; ++i)
		memset(&win->dma_buf_data[i], 0, sizeof(win->dma_buf_data));

	if ((decon->pdata->out_type == DECON_OUT_DSI)
			&& (win->index == decon->pdata->default_win)) {
		ret = decon_fb_alloc_memory(decon, win);
		if (ret) {
			dev_err(decon->dev, "failed to allocate display memory\n");
			return ret;
		}
	}

	fb_videomode_to_var(&fbinfo->var, &win->windata.win_mode.videomode);

	fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.accel	= FB_ACCEL_NONE;
	fbinfo->var.activate	= FB_ACTIVATE_NOW;
	fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
	fbinfo->var.bits_per_pixel = win->windata.default_bpp;
	fbinfo->var.width	= win->windata.width;
	fbinfo->var.height	= win->windata.height;
	fbinfo->fbops		= &decon_fb_ops;
	fbinfo->flags		= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  = &win->pseudo_palette;
	decon_dbg("default_win %d win_idx %d xres %d yres %d\n", decon->pdata->default_win, idx, fbinfo->var.xres, fbinfo->var.yres);

	ret = decon_check_var(&fbinfo->var, fbinfo);
	if (ret < 0) {
		dev_err(decon->dev, "check_var failed on initial video params\n");
		return ret;
	}

	ret = fb_alloc_cmap(&fbinfo->cmap, 256 /* palette size */, 1);
	if (ret == 0)
		fb_set_cmap(&fbinfo->cmap, fbinfo);
	else
		dev_err(decon->dev, "failed to allocate fb cmap\n");

	decon_dbg("decon%d window[%d] create\n", decon->id, idx);
	return 0;
}

static int decon_acquire_window(struct decon_device *decon)
{
	int i, ret;

	for (i = 0; i < decon->n_sink_pad; i++) {
		ret = decon_acquire_windows(decon, i);
		if (ret < 0) {
			decon_err("failed to create decon-int window[%d]\n", i);
			for (; i >= 0; i--)
				decon_release_windows(decon->windows[i]);
			return ret;
		}
	}

	return 0;
}

int decon_get_disp_ss_addr(struct decon_device *decon)
{
	if (of_have_populated_dt()) {
		struct device_node *nd;

		nd = of_find_compatible_node(NULL, NULL,
				"samsung,exynos8-disp_ss");
		if (!nd) {
			decon_err("failed find compatible node(sysreg-disp)");
			return -ENODEV;
		}

		decon->ss_regs = of_iomap(nd, 0);
		if (!decon->ss_regs) {
			decon_err("Failed to get sysreg-disp address.");
			return -ENOMEM;
		}
	} else {
		decon_err("failed have populated device tree");
		return -EIO;
	}

	return 0;
}

static void decon_parse_pdata(struct decon_device *decon, struct device *dev)
{
	struct device_node *te_eint;

	if (dev->of_node) {
		decon->id = of_alias_get_id(dev->of_node, "decon");
		of_property_read_u32(dev->of_node, "n_sink_pad",
					&decon->n_sink_pad);
		of_property_read_u32(dev->of_node, "n_src_pad",
					&decon->n_src_pad);
		of_property_read_u32(dev->of_node, "max_win",
					&decon->pdata->max_win);
		of_property_read_u32(dev->of_node, "default_win",
					&decon->pdata->default_win);
		/* video mode: 0, dp: 1 mipi command mode: 2 */
		of_property_read_u32(dev->of_node, "psr_mode",
					&decon->pdata->psr_mode);
		/* H/W trigger: 0, S/W trigger: 1 */
		of_property_read_u32(dev->of_node, "trig_mode",
					&decon->pdata->trig_mode);
		decon_info("decon-%s: max win%d, %s mode, %s trigger\n",
			(decon->id == 0) ? "f" : ((decon->id == 1) ? "s" : "t"),
			decon->pdata->max_win,
			decon->pdata->psr_mode ? "command" : "video",
			decon->pdata->trig_mode ? "sw" : "hw");

		/* 0: DSI_MODE_SINGLE, 1: DSI_MODE_DUAL_DSI */
		of_property_read_u32(dev->of_node, "dsi_mode", &decon->pdata->dsi_mode);
		decon_info("dsi mode(%d). 0: SINGLE 1: DUAL\n", decon->pdata->dsi_mode);

		of_property_read_u32(dev->of_node, "out_type", &decon->pdata->out_type);
		decon_info("out type(%d). 0: DSI 1: EDP 2: HDMI 3: WB\n",
				decon->pdata->out_type);

		if (decon->pdata->out_type == DECON_OUT_DSI) {
			of_property_read_u32_index(dev->of_node, "out_idx", 0,
					&decon->pdata->out_idx);
			decon_info("out idx(%d). 0: DSI0 1: DSI1 2: DSI2\n",
					decon->pdata->out_idx);

			if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DSI) {
				of_property_read_u32_index(dev->of_node, "out_idx", 1,
						&decon->pdata->out1_idx);
				decon_info("out1 idx(%d). 0: DSI0 1: DSI1 2: DSI2\n",
						decon->pdata->out1_idx);
			}
		}

		if ((decon->pdata->out_type == DECON_OUT_DSI) ||
			(decon->pdata->out_type == DECON_OUT_EDP)) {
			te_eint = of_get_child_by_name(decon->dev->of_node, "te_eint");
			if (!te_eint) {
				decon_info("No DT node for te_eint\n");
			} else {
				decon->eint_pend = of_iomap(te_eint, 0);
				if (!decon->eint_pend)
					decon_info("Failed to get te eint pend\n");

				decon->eint_mask = of_iomap(te_eint, 1);
				if (!decon->eint_mask)
					decon_info("Failed to get te eint mask\n");
			}
		}

	} else {
		decon_warn("no device tree information\n");
	}
}

static int decon_esd_panel_reset(struct decon_device *decon)
{
	int ret = 0;
	struct esd_protect *esd = &decon->esd;

	decon_info("++ %s\n", __func__);

	if (decon->state == DECON_STATE_OFF) {
		decon_warn("decon%d status is inactive\n", decon->id);
		return ret;
	}

	flush_workqueue(decon->lpd_wq);

	decon_lpd_block_exit(decon);

	mutex_lock(&decon->output_lock);

	if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon->ignore_vsync = true;

	flush_kthread_worker(&decon->update_regs_worker);

	/* stop output device (mipi-dsi or hdmi) */
	ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 0);
	if (ret) {
		decon_err("stopping stream failed for %s\n",
				decon->output_sd->name);
		goto reset_fail;
	}

	msleep(200);

	ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 1);
	if (ret) {
		decon_err("starting stream failed for %s\n",
				decon->output_sd->name);
		goto reset_fail;
	}

	esd->queuework_pending = 0;

	if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon->ignore_vsync = false;

	decon->need_update = true;
	decon->update_win.x = 0;
	decon->update_win.y = 0;
	decon->update_win.w = decon->lcd_info->xres;
	decon->update_win.h = decon->lcd_info->yres;
	decon->force_fullupdate = 1;
#if 0
	if (decon->pdata->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
			decon->pdata->trig_mode, DECON_TRIG_ENABLE);

	ret = decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
	if (ret) {
		decon_err("%s:vsync time over\n", __func__);
		goto reset_exit;
	}

	if (decon->pdata->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
			decon->pdata->trig_mode, DECON_TRIG_DISABLE);
reset_exit:
#endif
	mutex_unlock(&decon->output_lock);

	decon_lpd_unblock(decon);

	decon_info("-- %s\n", __func__);

	return ret;

reset_fail:
	if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon->ignore_vsync = false;

	mutex_unlock(&decon->output_lock);

	decon_lpd_unblock(decon);

	decon_info("--(e) %s\n", __func__);

	return ret;
}

static void decon_esd_handler(struct work_struct *work)
{
	int ret;
	struct esd_protect *esd;
	struct decon_device *decon;

	decon_info("esd : handler was called\n");

	esd = container_of(work, struct esd_protect, esd_work);
	decon = container_of(esd, struct decon_device, esd);

	if (decon->pdata->out_type == DECON_OUT_DSI) {
		ret = decon_esd_panel_reset(decon);
		if (ret) {
			decon_err("%s : failed to panel reset", __func__);
		}
	}

	return;
}

irqreturn_t decon_esd_pcd_handler(int irq, void *dev_id)
{
	struct esd_protect *esd;
	struct decon_device *decon = (struct decon_device *)dev_id;

	if (decon == NULL)
		goto handler_exit;

	if (decon->state == DECON_STATE_OFF)
		goto handler_exit;

	esd = &decon->esd;

	decon_info("Detection panel crack.. from now ignore vsync \n");

	decon->ignore_vsync = true;

handler_exit:
	return IRQ_HANDLED;
}


irqreturn_t decon_esd_err_handler(int irq, void *dev_id)
{
	struct esd_protect *esd;
	struct decon_device *decon = (struct decon_device *)dev_id;
#if 1 //def CONFIG_DSIM_ESD_RECOVERY_NOPOWER
	int timediff;
#endif	// CONFIG_DSIM_ESD_RECOVERY_NOPOWER

	if (decon == NULL)
		goto handler_exit;

	if (decon->state == DECON_STATE_OFF)
		goto handler_exit;

	esd = &decon->esd;

#if 1 //def CONFIG_DSIM_ESD_RECOVERY_NOPOWER
		timediff = (int) ktime_to_ms(ktime_sub(ktime_get(),esd->when_irq_enable));
		if( timediff < 40 ) decon_info( "esd : err_fg : diff=%d ms : RETURN\n", timediff);
		else decon_info( "esd : err_fg : diff=%d\n", timediff);
		if( timediff < 40 ) return IRQ_HANDLED;
#endif	// CONFIG_DSIM_ESD_RECOVERY_NOPOWER

	if ((esd->esd_wq) && (esd->queuework_pending == 0)) {
		esd->queuework_pending = 1;
		esd->irq_type = irq_err_fg;
		queue_work(esd->esd_wq, &esd->esd_work);
	}
handler_exit:
	return IRQ_HANDLED;
}


irqreturn_t decon_disp_det_handler(int irq, void *dev_id)
{
	struct esd_protect *esd;
	struct decon_device *decon = (struct decon_device *)dev_id;
#ifdef CONFIG_DSIM_ESD_RECOVERY_NOPOWER
	int timediff;
#endif	// CONFIG_DSIM_ESD_RECOVERY_NOPOWER

	if (decon == NULL)
		goto handler_exit;

	if (decon->state == DECON_STATE_OFF)
		goto handler_exit;

	esd = &decon->esd;
#ifdef CONFIG_DSIM_ESD_RECOVERY_NOPOWER
	timediff = (int) ktime_to_ms(ktime_sub(ktime_get(),esd->when_irq_enable));
	if( timediff < 30 ) decon_info( "esd : disp_det : diff=%d : RETURN\n", timediff);
	else decon_info( "esd : disp_det : diff=%d\n", timediff);
	if( timediff < 30 ) return IRQ_HANDLED;
#endif	// CONFIG_DSIM_ESD_RECOVERY_NOPOWER

	decon_info("esd : decon_irq : disp_det\n");

	if ((esd->esd_wq)  && (esd->queuework_pending == 0)) {
		esd->queuework_pending = 1;
		esd->irq_type = irq_disp_det;
		queue_work(esd->esd_wq, &esd->esd_work);
	}
handler_exit:
	return IRQ_HANDLED;
}


static int decon_register_esd_funcion(struct decon_device *decon)
{
	int gpio;
	int ret = 0;
	struct esd_protect *esd;
	struct device *dev = decon->dev;
	struct dsim_device *dsim = NULL;

	esd = &decon->esd;

	decon_info(" + %s \n", __func__);

	esd->pcd_irq = 0;
	esd->err_irq = 0;
	esd->pcd_gpio = 0;
	esd->disp_det_gpio = 0;

	dsim = container_of(decon->output_sd, struct dsim_device, sd);
	if(dsim->priv.esd_disable){
		decon_info( "%s : esd_disable = %d\n", __func__, dsim->priv.esd_disable );
		return ret;
	}

	gpio = of_get_named_gpio(dev->of_node, "gpio_pcd", 0);
	if (gpio_is_valid(gpio)) {
		decon_info("esd : found gpio_pcd success\n");
		esd->pcd_irq = gpio_to_irq(gpio);
		esd->pcd_gpio = gpio;
		ret ++;
	}

	gpio = of_get_named_gpio(dev->of_node, "gpio_err", 0);
	if (gpio_is_valid(gpio)) {
		decon_info("esd : found gpio_err success\n");
		esd->err_irq = gpio_to_irq(gpio);
		ret ++;
	}

	gpio = of_get_named_gpio(dev->of_node, "gpio_det", 0);
	if (gpio_is_valid(gpio)) {
		decon_info("esd : found disp_det sueccess\n");
		esd->disp_det_gpio = gpio;
		esd->disp_det_irq = gpio_to_irq(gpio);
		ret ++;
	}

	if (ret == 0)
		goto register_exit;

	esd->when_irq_enable = ktime_get();

	esd->esd_wq = create_singlethread_workqueue("decon_esd");

	if (esd->esd_wq) {
		INIT_WORK(&esd->esd_work, decon_esd_handler);
	}

	if (esd->pcd_irq) {
		if (devm_request_irq(dev, esd->pcd_irq, decon_esd_pcd_handler,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "pcd-irq", decon)) {
			dsim_err("%s : failed to request irq for pcd\n", __func__);
			esd->pcd_irq = 0;
			ret --;
		}
		disable_irq_nosync(esd->pcd_irq);
	}

	if (esd->err_irq) {
		if (devm_request_irq(dev, esd->err_irq, decon_esd_err_handler,
				IRQF_TRIGGER_RISING, "err-irq", decon)) {
			dsim_err("%s : faield to request irq for err_fg\n", __func__);
			esd->err_irq = 0;
			ret --;
		}
		disable_irq_nosync(esd->err_irq);
	}

	if (esd->disp_det_irq) {
		if (devm_request_irq(dev, esd->disp_det_irq, decon_disp_det_handler,
				IRQF_TRIGGER_FALLING, "display-det", decon)) {
			dsim_err("%s : faied to request irq for disp_det\n", __func__);
			esd->disp_det_irq = 0;
			ret --;
		}
		disable_irq_nosync(esd->disp_det_irq);
	}

	esd->queuework_pending = 0;

register_exit:
	dsim_info(" - %s \n", __func__);
	return ret;

}
#ifdef CONFIG_DECON_EVENT_LOG
static int decon_debug_event_show(struct seq_file *s, void *unused)
{
	struct decon_device *decon = s->private;
	DISP_SS_EVENT_SHOW(s, decon);
	return 0;
}

static int decon_debug_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, decon_debug_event_show, inode->i_private);
}

static const struct file_operations decon_event_fops = {
	.open = decon_debug_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/* --------- DRIVER INITIALIZATION ---------- */
static int decon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct decon_device *decon;
	struct resource *res;
	struct fb_info *fbinfo;
	int ret = 0;
	char device_name[MAX_NAME_SIZE], debug_name[MAX_NAME_SIZE];

	struct decon_mode_info psr;
	struct decon_param p;
	struct decon_window_regs win_regs;
	struct dsim_device *dsim = NULL;
	struct dsim_device *dsim1;
	struct panel_private *panel = NULL;
	struct exynos_md *md;
	struct device_node *cam_stat;
	int i = 0;
	int win_idx = 0;
	struct v4l2_subdev *sd = NULL;
	struct decon_win_config config;


	dev_info(dev, "%s start\n", __func__);

	decon = devm_kzalloc(dev, sizeof(struct decon_device), GFP_KERNEL);
	if (!decon) {
		decon_err("no memory for decon device\n");
		return -ENOMEM;
	}

	/* setup pointer to master device */
	decon->dev = dev;
	decon->pdata = devm_kzalloc(dev, sizeof(struct exynos_decon_platdata),
						GFP_KERNEL);
	if (!decon->pdata) {
		decon_err("no memory for DECON platdata\n");
		return -ENOMEM;
	}
	/* parse decon config from the DT */
	decon_parse_pdata(decon, dev);
	win_idx = decon->pdata->default_win;

	/* init clock setting for decon */
	switch (decon->id) {
	case 0:
		decon_f_drvdata = decon;
		decon_f_get_clocks(decon);
		decon->decon[1] = decon_s_drvdata;
		decon->decon[2] = decon_t_drvdata;
		decon->default_idma = IDMA_G1;
		decon->dma_rsm = devm_kzalloc(dev, sizeof(struct dma_rsm), GFP_KERNEL);
		if (!decon->dma_rsm) {
			decon_err("no memory for decon dma_rsm\n");
			return -ENOMEM;
		}
		break;
	case 1:
		decon_s_drvdata = decon;
		decon_s_get_clocks(decon);
		decon->decon[0] = decon_f_drvdata;
		decon->decon[2] = decon_t_drvdata;
		decon->default_idma = IDMA_G2;
		decon->dma_rsm = decon_f_drvdata->dma_rsm;
		break;
	case 2:
		decon_t_drvdata = decon;
		decon_t_get_clocks(decon);
		decon->decon[0] = decon_f_drvdata;
		decon->decon[1] = decon_s_drvdata;
		decon->default_idma = IDMA_G3;
		decon->dma_rsm = decon_f_drvdata->dma_rsm;
		break;
	}

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	decon->regs = devm_ioremap_resource(dev, res);
	if (decon->regs == NULL) {
		decon_err("failed to claim register region\n");
		return -ENOENT;
	}

	spin_lock_init(&decon->slock);
	init_waitqueue_head(&decon->vsync_info.wait);
	init_waitqueue_head(&decon->wait_frmdone);
	init_waitqueue_head(&decon->wait_vstatus);
	mutex_init(&decon->vsync_info.irq_lock);
	snprintf(device_name, MAX_NAME_SIZE, "decon%d", decon->id);
	decon->timeline = sw_sync_timeline_create(device_name);

	if (decon->pdata->out_type == DECON_OUT_WB)
		decon->timeline_max = 0;
	else
		decon->timeline_max = 1;

	/* Get IRQ resource and register IRQ, create thread */
	switch (decon->id) {
	case 0:
		ret = decon_f_register_irq(pdev, decon);
		if (ret)
			goto fail;

		ret = decon_f_create_vsync_thread(decon);
		if (ret)
			goto fail;

		ret = decon_f_create_psr_thread(decon);
		if (ret)
			goto fail_vsync_thread;

		snprintf(debug_name, MAX_NAME_SIZE, "decon");
		decon->debug_root = debugfs_create_dir(debug_name, NULL);
		if (!decon->debug_root) {
			decon_err("failed to create debugfs root directory.\n");
			goto fail_psr_thread;
		}
		break;
	case 1:
		if (decon->decon[0])
			decon->debug_root = decon->decon[0]->debug_root;

		ret = decon_s_register_irq(pdev, decon);
		if (ret)
			goto fail;
		break;
	case 2:
		if (decon->decon[0])
			decon->debug_root = decon->decon[0]->debug_root;

		ret = decon_t_register_irq(pdev, decon);
		if (ret)
			goto fail;

		decon_t_set_lcd_info(decon);
		break;
	}

	if ((decon->pdata->psr_mode != DECON_VIDEO_MODE) &&
		((decon->pdata->out_type == DECON_OUT_DSI) ||
		(decon->pdata->out_type == DECON_OUT_EDP))) {
		ret = decon_config_eint_for_te(pdev, decon);
		if (ret)
			goto fail_psr_thread;

		ret = decon_register_lpd_work(decon);
		if (ret)
			goto fail_psr_thread;

	}

	decon->ion_client = ion_client_create(ion_exynos, device_name);
	if (IS_ERR(decon->ion_client)) {
		decon_err("failed to ion_client_create\n");
		goto fail_psr_thread;
	}

	decon->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(decon->pinctrl)) {
		decon_warn("failed to get decon-%d pinctrl\n", decon->id);
		decon->pinctrl = NULL;
	} else {
		decon->decon_te_on = pinctrl_lookup_state(decon->pinctrl, "decon_te_on");
		if (IS_ERR(decon->decon_te_on)) {
			decon_err("failed to get decon_te_on pin state\n");
			decon->decon_te_on = NULL;
		}
		decon->decon_te_off = pinctrl_lookup_state(decon->pinctrl, "decon_te_off");
		if (IS_ERR(decon->decon_te_off)) {
			decon_err("failed to get decon_te_off pin state\n");
			decon->decon_te_off = NULL;
		}
	}

#ifdef CONFIG_DECON_EVENT_LOG
	snprintf(debug_name, MAX_NAME_SIZE, "event%d", decon->id);
	atomic_set(&decon->disp_ss_log_idx, -1);
	decon->debug_event = debugfs_create_file(debug_name, 0444,
						decon->debug_root,
						decon, &decon_event_fops);
#endif

	/* register internal and external DECON as entity */
	ret = decon_register_entity(decon);
	if (ret)
		goto fail_psr_thread;

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get output media device\n");
		goto fail_entity;
	}

	decon->mdev = md;

	/* mapping SYSTEM registers */
	ret = decon_get_disp_ss_addr(decon);
	if (ret)
		goto fail_entity;

	/* link creation: vpp <-> decon / decon <-> output */
	ret = decon_create_links(decon, md);
	if (ret)
		goto fail_entity;

	ret = decon_register_subdev_nodes(decon, md);
	if (ret)
		goto fail_entity;

	/* configure windows */
	ret = decon_acquire_window(decon);
	if (ret)
		goto fail_entity;

	/* register framebuffer */
	fbinfo = decon->windows[decon->pdata->default_win]->fbinfo;
	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		decon_err("failed to register framebuffer\n");
		goto fail_entity;
	}

	/* mutex mechanism */
	mutex_init(&decon->output_lock);
	mutex_init(&decon->mutex);

	/* systrace */
	decon->systrace_pid = 0;
	decon->tracing_mark_write = tracing_mark_write;

	/* init work thread for update registers */
	mutex_init(&decon->update_regs_list_lock);
	INIT_LIST_HEAD(&decon->update_regs_list);
	INIT_LIST_HEAD(&decon->sbuf_active_list);
	init_kthread_worker(&decon->update_regs_worker);
	decon->update_regs_list_cnt = 0;
	decon->tracing_mark_write( decon->systrace_pid, 'C', "update_regs_list", decon->update_regs_list_cnt);

	decon->update_regs_thread = kthread_run(kthread_worker_fn,
			&decon->update_regs_worker, device_name);
	if (IS_ERR(decon->update_regs_thread)) {
		ret = PTR_ERR(decon->update_regs_thread);
		decon->update_regs_thread = NULL;
		decon_err("failed to run update_regs thread\n");
		goto fail_update_thread;
	}
	init_kthread_work(&decon->update_regs_work, decon_update_regs_handler);

	snprintf(device_name, MAX_NAME_SIZE, "decon%d-wb", decon->id);

	if (decon->pdata->out_type == DECON_OUT_DSI) {
		ret = decon_set_lcd_config(decon);
		if (ret) {
			decon_err("failed to set lcd information\n");
			goto fail_update_thread;
		}
	}
	platform_set_drvdata(pdev, decon);
	pm_runtime_enable(dev);

	decon->bts_init_ops = &decon_init_bts_control;

	call_init_ops(decon, bts_add, decon);

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
	decon->bts2_ops = &decon_bts2_control;
	decon->bts2_ops->bts_init(decon);
#endif

	if (!decon->id && decon->pdata->out_type == DECON_OUT_DSI) {
		/* Enable only Decon_F during probe */
#if defined(CONFIG_PM_RUNTIME)
		pm_runtime_get_sync(decon->dev);
#else
		decon_runtime_resume(decon->dev);
#endif

		if (decon->pdata->psr_mode != DECON_VIDEO_MODE) {
			if (decon->pinctrl && decon->decon_te_on) {
				if (pinctrl_select_state(decon->pinctrl, decon->decon_te_on)) {
					decon_err("failed to turn on Decon_TE\n");
					goto fail_update_thread;
				}
			}
		}

		/* DSIM device will use the decon pointer to call the LPD functions */
		dsim = container_of(decon->output_sd, struct dsim_device, sd);
		dsim->decon = (void *)decon;

		decon_to_init_param(decon, &p);
		if (decon_reg_init(decon->id, decon->pdata->out_idx, &p) < 0)
			goto decon_init_done;

		memset(&win_regs, 0, sizeof(struct decon_window_regs));
		win_regs.wincon = wincon(0x8, 0xFF, 0xFF, 0xFF, DECON_BLENDING_NONE);
		win_regs.wincon |= WIN_CONTROL_EN_F;
		win_regs.start_pos = win_start_pos(0, 0);
		win_regs.end_pos = win_end_pos(0, 0, fbinfo->var.xres * 2, fbinfo->var.yres);
		decon_dbg("xres %d yres %d win_start_pos %x win_end_pos %x\n",
			fbinfo->var.xres, fbinfo->var.yres, win_regs.start_pos,
			win_regs.end_pos);
		win_regs.colormap = 0x00ff00;
		win_regs.pixel_count = fbinfo->var.xres * 2 * fbinfo->var.yres;
		win_regs.whole_w = fbinfo->var.xres_virtual;
		win_regs.whole_h = fbinfo->var.yres_virtual;
		win_regs.offset_x = fbinfo->var.xoffset;
		win_regs.offset_y = fbinfo->var.yoffset;
		win_regs.type = decon->default_idma;
		decon_reg_set_window_control(decon->id, win_idx, &win_regs, false);

		decon->vpp_usage_bitmask |= (1 << decon->default_idma);
		decon->vpp_used[decon->default_idma] = true;
		memset(&config, 0, sizeof(struct decon_win_config));
		config.vpp_parm.addr[0] = fbinfo->fix.smem_start;
		config.format = DECON_PIXEL_FORMAT_BGRA_8888;
		config.src.w = fbinfo->var.xres * 2;
		config.src.h = fbinfo->var.yres;
		config.src.f_w = fbinfo->var.xres * 2;
		config.src.f_h = fbinfo->var.yres;
		config.dst.w = config.src.w;
		config.dst.h = config.src.h;
		config.dst.f_w = config.src.f_w;
		config.dst.f_h = config.src.f_h;
		sd = decon->mdev->vpp_sd[decon->default_idma];
		/* Set a default Bandwidth */
		if (v4l2_subdev_call(sd, core, ioctl, VPP_SET_BW, &config))
			decon_err("Failed to VPP_SET_BW VPP-%d\n", decon->default_idma);

		if (v4l2_subdev_call(sd, core, ioctl, VPP_WIN_CONFIG, &config)) {
			decon_err("Failed to config VPP-%d\n", decon->default_idma);
			decon->vpp_usage_bitmask &= ~(1 << decon->default_idma);
			decon->vpp_err_stat[decon->default_idma] = true;
		}
		decon_reg_update_req_window(decon->id, win_idx);

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
		if (!decon->id)
			mdnie_reg_start(decon->lcd_info->xres, decon->lcd_info->yres);
#endif

		decon_to_psr_info(decon, &psr);
		decon_reg_set_int(decon->id, &psr, 1);
		decon_reg_start(decon->id, &psr);

		/* TODO:
		 * 1. If below code is called after turning on 1st LCD.
		 *    2nd LCD is not turned on
		 * 2. It needs small delay between decon start and LCD on
		 *    for avoiding garbage display when dual dsi mode is used. */
		if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DSI) {
			decon_info("2nd LCD is on\n");
			dsim1 = container_of(decon->output_sd1, struct dsim_device, sd);
			//call_panel_ops(dsim1, resume, dsim1);

			decon->dispon_req = DSI1_REQ_DP_ON(decon->dispon_req);
		}

		dsim = container_of(decon->output_sd, struct dsim_device, sd);
		dsim->decon = (void *)decon;



		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		if (decon_reg_wait_update_done_and_mask(decon->id, &psr, SHADOW_UPDATE_TIMEOUT)
				< 0)
			decon_err("%s: wait_for_update_timeout\n", __func__);
		msleep(500);


		call_panel_ops(dsim, displayon, dsim);
		call_panel_ops(dsim1, displayon, dsim1);

decon_init_done:
		decon->ignore_vsync = false;
		decon->disp_ss_log_level = DISP_EVENT_LEVEL_HIGH;
		if ((decon->id == 0)  && (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE)) {
			if (dsim == NULL) {
				sd = decon->mdev->vpp_sd[decon->default_idma];
				dsim = container_of(decon->output_sd, struct dsim_device, sd);
			}
			if (dsim) {
				panel = &dsim->priv;
				if ((panel) && (!panel->lcdConnected)) {
					decon->ignore_vsync = true;
					decon_info("decon does not found panel activate vsync ignore\n");
					goto decon_rest_init;
				}
				decon_info("panel id : %x : %x : %x\n", panel->id[0], panel->id[1], panel->id[2]);
			}
		}

		ret = decon_register_esd_funcion(decon);

decon_rest_init:
		decon->state = DECON_STATE_INIT;
#ifdef CONFIG_EXYNOS_DECON_DUAL_DSI
		decon->dual_status = DECON_SET_STAT(DECON_STAT_ON) | TE_SET_STAT(TE_STAT_DSI0) |
			DSI0_SET_STAT(DSI_STAT_MASTER) | DSI1_SET_STAT(DSI_STAT_SLAVE);
		decon_info("%s dual status : %x\n", __func__, decon->dual_status);

		decon->dispon_req = DSI1_REQ_DP_ON(decon->dispon_req);
#endif

		/* [W/A] prevent sleep enter during LCD on */
		ret = device_init_wakeup(decon->dev, true);
		if (ret) {
			dev_err(decon->dev, "failed to init wakeup device\n");
			goto fail_update_thread;
		}
		pm_stay_awake(decon->dev);
		dev_warn(decon->dev, "pm_stay_awake");
		cam_stat = of_get_child_by_name(decon->dev->of_node, "cam-stat");
		if (!cam_stat) {
			decon_info("No DT node for cam-stat\n");
		} else {
			decon->cam_status[0] = of_iomap(cam_stat, 0);
			if (!decon->cam_status[0])
				decon_info("Failed to get CAM0-STAT Reg\n");

			decon->cam_status[1] = of_iomap(cam_stat, 1);
			if (!decon->cam_status[1])
				decon_info("Failed to get CAM1-STAT Reg\n");
		}
	} else { /* DECON-EXT(only single-dsi) is off at probe */
		decon->state = DECON_STATE_INIT;
		ret = device_init_wakeup(decon->dev, true);
		if (ret) {
			dev_err(decon->dev, "failed to init wakeup device\n");
			goto fail_update_thread;
		}
	}

	for (i = 0; i < MAX_VPP_SUBDEV; i++)
		decon->vpp_used[i] = false;

	if (decon->id == 0)
		decon_esd_enable_interrupt(decon);

	decon->force_fullupdate = 0;
	decon_info("decon%d registered successfully", decon->id);

	return 0;

fail_update_thread:
	mutex_destroy(&decon->output_lock);
	mutex_destroy(&decon->mutex);
	mutex_destroy(&decon->update_regs_list_lock);

	if (decon->update_regs_thread)
		kthread_stop(decon->update_regs_thread);

fail_entity:
	decon_unregister_entity(decon);

fail_psr_thread:
	decon_f_destroy_psr_thread(decon);

fail_vsync_thread:
	decon_f_destroy_vsync_thread(decon);

fail:
	decon_err("decon probe fail");
	return ret;
}

static int decon_remove(struct platform_device *pdev)
{
	struct decon_device *decon = platform_get_drvdata(pdev);
	int i;

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
	decon->bts2_ops->bts_deinit(decon);
#endif

	pm_runtime_disable(&pdev->dev);
	decon_put_clocks(decon);
	unregister_framebuffer(decon->windows[0]->fbinfo);

	if (decon->update_regs_thread)
		kthread_stop(decon->update_regs_thread);

	for (i = 0; i < decon->pdata->max_win; i++)
		decon_release_windows(decon->windows[i]);

	debugfs_remove_recursive(decon->debug_root);
	kfree(decon);

	decon_info("remove sucessful\n");
	return 0;
}

static void decon_shutdown(struct platform_device *pdev)
{
	struct decon_device *decon = platform_get_drvdata(pdev);

	dev_info(decon->dev, "%s + state:%d\n", __func__, decon->state);
	DISP_SS_EVENT_LOG(DISP_EVT_DECON_SHUTDOWN, &decon->sd, ktime_set(0, 0));

	decon_lpd_block_exit(decon);
	/* Unused DECON state is DECON_STATE_INIT */
	if (decon->state == DECON_STATE_ON)
		decon_disable(decon);

	dev_info(decon->dev, "%s -\n", __func__);
	return;
}

static struct platform_driver decon_driver __refdata = {
	.probe		= decon_probe,
	.remove		= decon_remove,
	.shutdown	= decon_shutdown,
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= &decon_pm_ops,
		.of_match_table = of_match_ptr(decon_device_table),
	}
};

static int exynos_decon_register(void)
{
	platform_driver_register(&decon_driver);

	return 0;
}

static void exynos_decon_unregister(void)
{
	platform_driver_unregister(&decon_driver);
}
late_initcall(exynos_decon_register);
module_exit(exynos_decon_unregister);

MODULE_AUTHOR("meka <v.meka@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS DECON driver");
MODULE_LICENSE("GPL");
