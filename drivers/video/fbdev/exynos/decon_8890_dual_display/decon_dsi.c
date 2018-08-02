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

#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include <linux/of_address.h>
#include <linux/clk-private.h>

#include <media/v4l2-subdev.h>

#include "decon.h"
#include "dsim.h"
#include "decon_helper.h"

extern unsigned int lpcharge;

unsigned int decon_bootmode;
EXPORT_SYMBOL(decon_bootmode);
static int __init decon_bootmode_setup(char *str)
{
	get_option(&str, &decon_bootmode);
	return 1;
}
__setup("bootmode=", decon_bootmode_setup);

int create_link_mipi(struct decon_device *decon, int id)
{
	int i, ret = 0;
	int n_pad = decon->n_sink_pad + decon->n_src_pad;
	int flags = 0;
	char err[80];
	struct exynos_md *md = decon->mdev;

	if (IS_ERR_OR_NULL(md->dsim_sd[id])) {
		decon_err("failed to get subdev of dsim%d\n", decon->id);
		return -EINVAL;
	}

	flags = MEDIA_LNK_FL_ENABLED;
	for (i = decon->n_sink_pad; i < n_pad; i++) {
		ret = media_entity_create_link(&decon->sd.entity, i,
				&md->dsim_sd[id]->entity, 0, flags);
		if (ret) {
			snprintf(err, sizeof(err), "%s --> %s",
					decon->sd.entity.name,
					decon->output_sd->entity.name);
			return ret;
		}

		decon_info("%s[%d] --> [0]%s link is created successfully\n",
				decon->sd.entity.name, i,
				decon->output_sd->entity.name);
	}

	return ret;
}

int find_subdev_mipi(struct decon_device *decon, int id)
{
	struct exynos_md *md;

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get mdev device(%d)\n", decon->id);
		return -ENODEV;
	}

	decon->output_sd = md->dsim_sd[id];

	if (IS_ERR_OR_NULL(decon->output_sd)) {
		decon_warn("couldn't find dsim%d subdev\n", id);
		return -ENOMEM;
	}

	v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_GET_LCD_INFO, NULL);
	decon->lcd_info = (struct decon_lcd *)v4l2_get_subdev_hostdata(decon->output_sd);
	if (IS_ERR_OR_NULL(decon->lcd_info)) {
		decon_err("failed to get lcd information\n");
		return -EINVAL;
	}
	decon_dbg("###lcd_info[hfp %d hbp %d hsa %d vfp %d vbp %d vsa %d xres %d yres %d\n",
		decon->lcd_info->hfp,  decon->lcd_info->hbp,  decon->lcd_info->hsa,
		 decon->lcd_info->vfp,  decon->lcd_info->vbp,  decon->lcd_info->vsa,
		 decon->lcd_info->xres,  decon->lcd_info->yres);

	if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DSI) {
		decon->output_sd1 = md->dsim_sd[decon->pdata->out1_idx];
		if (IS_ERR_OR_NULL(decon->output_sd1)) {
			decon_warn("couldn't find 2nd DSIM subdev\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static u32 fb_visual(u32 bits_per_pixel, unsigned short palette_sz)
{
	switch (bits_per_pixel) {
	case 32:
	case 24:
	case 16:
	case 12:
		return FB_VISUAL_TRUECOLOR;
	case 8:
		if (palette_sz >= 256)
			return FB_VISUAL_PSEUDOCOLOR;
		else
			return FB_VISUAL_TRUECOLOR;
	case 1:
		return FB_VISUAL_MONO01;
	default:
		return FB_VISUAL_PSEUDOCOLOR;
	}
}

static inline u32 fb_linelength(u32 xres_virtual, u32 bits_per_pixel)
{
	return (xres_virtual * bits_per_pixel) / 8;
}

static u16 fb_panstep(u32 res, u32 res_virtual)
{
	return res_virtual > res ? 1 : 0;
}

int decon_set_par(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	struct decon_window_regs win_regs;
	int win_no = win->index;

	dev_info(decon->dev, "%s: state %d\n", __func__, decon->state);

	if ((decon->pdata->out_type == DECON_OUT_DSI &&
			decon->state == DECON_STATE_INIT) ||
			decon->state == DECON_STATE_OFF)
		return 0;

	memset(&win_regs, 0, sizeof(struct decon_window_regs));
	decon_lpd_block_exit(decon);

	decon_reg_wait_for_update_timeout(decon->id, SHADOW_UPDATE_TIMEOUT);
	info->fix.visual = fb_visual(var->bits_per_pixel, 0);

	info->fix.line_length = fb_linelength(var->xres_virtual,
			var->bits_per_pixel);
	info->fix.xpanstep = fb_panstep(var->xres, var->xres_virtual);
	info->fix.ypanstep = fb_panstep(var->yres, var->yres_virtual);
	win_regs.wincon = WIN_CONTROL_EN_F;

	win_regs.wincon |= wincon(var->transp.length, 0, 0xFF,
				0xFF, DECON_BLENDING_NONE);
	win_regs.start_pos = win_start_pos(0, 0);
	win_regs.end_pos = win_end_pos(0, 0, var->xres, var->yres);
	win_regs.pixel_count = (var->xres * var->yres);
	win_regs.whole_w = var->xoffset + var->xres;
	win_regs.whole_h = var->yoffset + var->yres;
	win_regs.offset_x = var->xoffset;
	win_regs.offset_y = var->yoffset;
	win_regs.type = decon->default_idma;
	decon_reg_set_window_control(decon->id, win_no, &win_regs, false);

	decon_lpd_unblock(decon);

	return 0;
}
EXPORT_SYMBOL(decon_set_par);

int decon_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int x, y;
	unsigned long long hz;

	var->xres_virtual = max(var->xres_virtual, var->xres);
	var->yres_virtual = max(var->yres_virtual, var->yres);

	if (!decon_validate_x_alignment(decon, 0, var->xres,
			var->bits_per_pixel))
		return -EINVAL;

	/* always ensure these are zero, for drop through cases below */
	var->transp.offset = 0;
	var->transp.length = 0;

	switch (var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
		var->red.offset		= 4;
		var->green.offset	= 2;
		var->blue.offset	= 0;
		var->red.length		= 5;
		var->green.length	= 3;
		var->blue.length	= 2;
		var->transp.offset	= 7;
		var->transp.length	= 1;
		break;

	case 19:
		/* 666 with one bit alpha/transparency */
		var->transp.offset	= 18;
		var->transp.length	= 1;
	case 18:
		var->bits_per_pixel	= 32;

		/* 666 format */
		var->red.offset		= 12;
		var->green.offset	= 6;
		var->blue.offset	= 0;
		var->red.length		= 6;
		var->green.length	= 6;
		var->blue.length	= 6;
		break;

	case 16:
		/* 16 bpp, 565 format */
		var->red.offset		= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->red.length		= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		break;

	case 32:
	case 28:
	case 25:
		var->transp.length	= var->bits_per_pixel - 24;
		var->transp.offset	= 24;
		/* drop through */
	case 24:
		/* our 24bpp is unpacked, so 32bpp */
		var->bits_per_pixel	= 32;
		var->red.offset		= 16;
		var->red.length		= 8;
		var->green.offset	= 8;
		var->green.length	= 8;
		var->blue.offset	= 0;
		var->blue.length	= 8;
		break;

	default:
		decon_err("invalid bpp %d\n", var->bits_per_pixel);
		return -EINVAL;
	}

	if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE) {
		x = var->xres;
		y = var->yres;
	} else {
		x = var->xres + var->left_margin + var->right_margin + var->hsync_len;
		y = var->yres + var->upper_margin + var->lower_margin + var->vsync_len;
	}
	hz = 1000000000000ULL;		/* 1e12 picoseconds per second */

	hz += (x * y) / 2;
	do_div(hz, x * y);		/* divide by x * y with rounding */

	hz += var->pixclock / 2;
	do_div(hz, var->pixclock);	/* divide by pixclock with rounding */

	win->fps = hz;
	decon_dbg("xres:%d, yres:%d, v_xres:%d, v_yres:%d, bpp:%d, %lldhz\n",
			var->xres, var->yres, var->xres_virtual,
			var->yres_virtual, var->bits_per_pixel, hz);

	return 0;
}

static inline unsigned int chan_to_field(unsigned int chan,
					 struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

int decon_setcolreg(unsigned regno,
			    unsigned red, unsigned green, unsigned blue,
			    unsigned transp, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	unsigned int val;

	dev_dbg(decon->dev, "%s: win %d: %d => rgb=%d/%d/%d\n",
		__func__, win->index, regno, red, green, blue);

	if (decon->state == DECON_STATE_OFF)
		return 0;

	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseudo-palette */

		if (regno < 16) {
			u32 *pal = info->pseudo_palette;

			val  = chan_to_field(red,   &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue,  &info->var.blue);

			pal[regno] = val;
		}
		break;
	default:
		return 1;	/* unknown type */
	}

	return 0;
}
EXPORT_SYMBOL(decon_setcolreg);

int decon_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	struct v4l2_subdev *sd = NULL;
	struct decon_win_config config;
	int ret = 0;
	int shift = 0;
	struct decon_mode_info psr;

	if ((decon->pdata->out_type == DECON_OUT_DSI &&
			decon->state == DECON_STATE_INIT) ||
			decon->state == DECON_STATE_OFF) {
		decon_warn("%s: decon%d state(%d), UNBLANK missed\n",
				__func__, decon->id, decon->state);
		return 0;
	}

	decon_set_par(info);

	decon_lpd_block_exit(decon);
	decon->vpp_usage_bitmask |= (1 << decon->default_idma);
	decon->vpp_used[decon->default_idma] = true;
	memset(&config, 0, sizeof(struct decon_win_config));
	switch (var->bits_per_pixel) {
	case 16:
		config.format = DECON_PIXEL_FORMAT_RGB_565;
		shift = 2;
		break;
	case 24:
	case 32:
		config.format = DECON_PIXEL_FORMAT_ABGR_8888;
		shift = 4;
		break;
	default:
		decon_err("%s: bits_per_pixel %d\n", __func__, var->bits_per_pixel);
	}

	config.vpp_parm.addr[0] = info->fix.smem_start;
	config.src.x =  var->xoffset >> shift;
	config.src.y =  var->yoffset;
	config.src.w = var->xres;
	config.src.h = var->yres;
	config.src.f_w = var->xres;
	config.src.f_h = var->yres;
	config.dst.w = config.src.w;
	config.dst.h = config.src.h;
	config.dst.f_w = config.src.f_w;
	config.dst.f_h = config.src.f_h;
	sd = decon->mdev->vpp_sd[decon->default_idma];
	/* Set a default Bandwidth */
	if (v4l2_subdev_call(sd, core, ioctl, VPP_SET_BW, &config))
			decon_err("Failed to VPP_SET_BW VPP-%d\n", decon->default_idma);

	if (v4l2_subdev_call(sd, core, ioctl, VPP_WIN_CONFIG, &config)) {
		decon_err("%s: Failed to config VPP-%d\n", __func__, win->vpp_id);
		decon->vpp_usage_bitmask &= ~(1 << win->vpp_id);
		decon->vpp_err_stat[win->vpp_id] = true;
	}

	decon_reg_update_req_window(decon->id, win->index);

	decon_to_psr_info(decon, &psr);
	decon_reg_start(decon->id, &psr);
	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

	if (decon_reg_wait_update_done_and_mask(decon->id, &psr, SHADOW_UPDATE_TIMEOUT)
			< 0)
		decon_err("%s: wait_for_update_timeout\n", __func__);

	decon_lpd_unblock(decon);
	return ret;
}
EXPORT_SYMBOL(decon_pan_display);

int decon_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
#ifdef CONFIG_ION_EXYNOS
	int ret;
	struct decon_win *win = info->par;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	ret = dma_buf_mmap(win->dma_buf_data[0].dma_buf, vma, 0);

	return ret;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(decon_mmap);

static void decon_fb_missing_pixclock(struct decon_fb_videomode *win_mode,
		enum decon_psr_mode mode)
{
	u64 pixclk = 1000000000000ULL;
	u32 div;
	u32 width, height;

	if (mode == DECON_MIPI_COMMAND_MODE) {
		width = win_mode->videomode.xres;
		height = win_mode->videomode.yres;
	} else {
		width  = win_mode->videomode.left_margin + win_mode->videomode.hsync_len +
			win_mode->videomode.right_margin + win_mode->videomode.xres;
		height = win_mode->videomode.upper_margin + win_mode->videomode.vsync_len +
			win_mode->videomode.lower_margin + win_mode->videomode.yres;
	}

	div = width * height * (win_mode->videomode.refresh ? : 60);

	do_div(pixclk, div);
	win_mode->videomode.pixclock = pixclk;
}

static void decon_parse_lcd_info(struct decon_device *decon)
{
	int i;
	struct decon_lcd *lcd_info = decon->lcd_info;

	for (i = 0; i < decon->pdata->max_win; i++) {
		decon->windows[i]->win_mode.videomode.left_margin = lcd_info->hbp;
		decon->windows[i]->win_mode.videomode.right_margin = lcd_info->hfp;
		decon->windows[i]->win_mode.videomode.upper_margin = lcd_info->vbp;
		decon->windows[i]->win_mode.videomode.lower_margin = lcd_info->vfp;
		decon->windows[i]->win_mode.videomode.hsync_len = lcd_info->hsa;
		decon->windows[i]->win_mode.videomode.vsync_len = lcd_info->vsa;
		decon->windows[i]->win_mode.videomode.xres = lcd_info->xres;
		decon->windows[i]->win_mode.videomode.yres = lcd_info->yres;
		decon->windows[i]->win_mode.width = lcd_info->width;
		decon->windows[i]->win_mode.height = lcd_info->height;
	}
}

int decon_set_lcd_config(struct decon_device *decon)
{
	struct fb_info *fbinfo;
	struct decon_fb_videomode *win_mode;
	int i, ret = 0;

	decon_parse_lcd_info(decon);
	for (i = 0; i < decon->pdata->max_win; i++) {
		if (!decon->windows[i])
			continue;

		win_mode = &decon->windows[i]->win_mode;
		if (!win_mode->videomode.pixclock)
			decon_fb_missing_pixclock(win_mode, decon->pdata->psr_mode);

		fbinfo = decon->windows[i]->fbinfo;
		fb_videomode_to_var(&fbinfo->var, &win_mode->videomode);

		fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
		fbinfo->fix.accel	= FB_ACCEL_NONE;
		fbinfo->fix.line_length = fb_linelength(fbinfo->var.xres_virtual,
					fbinfo->var.bits_per_pixel);
		fbinfo->var.activate	= FB_ACTIVATE_NOW;
		fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
		fbinfo->var.bits_per_pixel = LCD_DEFAULT_BPP;
		fbinfo->var.width	= win_mode->width;
		fbinfo->var.height	= win_mode->height;

		ret = decon_check_var(&fbinfo->var, fbinfo);
		if (ret)
			return ret;
		decon_dbg("window[%d] verified parameters\n", i);
	}

	return ret;
}

irqreturn_t decon_fb_isr_for_eint(int irq, void *dev_id)
{
	struct decon_device *decon = dev_id;
	struct decon_mode_info psr;
	ktime_t timestamp = ktime_get();

	DISP_SS_EVENT_LOG(DISP_EVT_TE_INTERRUPT, &decon->sd, timestamp);

	spin_lock(&decon->slock);

	if (decon->pdata->trig_mode == DECON_SW_TRIG) {
		decon_to_psr_info(decon, &psr);
		decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_ENABLE);
	}

#ifdef CONFIG_DECON_LPD_DISPLAY
	if ((decon->state == DECON_STATE_ON) && (decon->pdata->out_type == DECON_OUT_DSI)) {
		if (decon_min_lock_cond(decon))
			queue_work(decon->lpd_wq, &decon->lpd_work);
	}
#endif
	decon->vsync_info.timestamp = timestamp;
	wake_up_interruptible_all(&decon->vsync_info.wait);

	spin_unlock(&decon->slock);

	return IRQ_HANDLED;
}

int decon_config_eint_for_te(struct platform_device *pdev, struct decon_device *decon)
{
	struct device *dev = decon->dev;
	int gpio, gpio1;
	int ret = 0;

	/* Get IRQ resource and register IRQ handler. */
	if (of_get_property(dev->of_node, "gpios", NULL) != NULL) {
		gpio = of_get_gpio(dev->of_node, 0);
		if (gpio < 0) {
			decon_err("failed to get proper gpio number\n");
			return -EINVAL;
		}

		gpio1 = of_get_gpio(dev->of_node, 1);
		if (gpio1 < 0)
			decon_info("This board doesn't support TE GPIO of 2nd LCD\n");
	}

	gpio = gpio_to_irq(gpio);
	decon->irq = gpio;

	decon_dbg("%s: gpio(%d)\n", __func__, gpio);
	ret = devm_request_irq(dev, gpio, decon_fb_isr_for_eint,
			IRQF_TRIGGER_RISING, pdev->name, decon);

	decon->eint_status = 1;

	return ret;
}

static int decon_wait_for_vsync_thread(void *data)
{
	struct decon_device *decon = data;

	while (!kthread_should_stop()) {
		ktime_t timestamp = decon->vsync_info.timestamp;
		int ret = wait_event_interruptible(decon->vsync_info.wait,
			!ktime_equal(timestamp, decon->vsync_info.timestamp) &&
			decon->vsync_info.active);

		if (!ret)
			sysfs_notify(&decon->dev->kobj, NULL, "vsync");
	}

	return 0;
}

static ssize_t decon_vsync_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%llu\n",
			ktime_to_ns(decon->vsync_info.timestamp));
}

static DEVICE_ATTR(vsync, S_IRUGO, decon_vsync_show, NULL);

static ssize_t decon_psr_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	struct decon_lcd *lcd_info = decon->lcd_info;
	int dsc_offset = (lcd_info->dsc_enabled)? 4: 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", decon->pdata->psr_mode + dsc_offset);
}

static DEVICE_ATTR(psr_info, S_IRUGO, decon_psr_info, NULL);

int decon_f_create_vsync_thread(struct decon_device *decon)
{
	int ret = 0;

	ret = device_create_file(decon->dev, &dev_attr_vsync);
	if (ret) {
		decon_err("failed to create vsync file\n");
		return ret;
	}

	decon->vsync_info.thread = kthread_run(decon_wait_for_vsync_thread,
			decon, "s3c-fb-vsync");
	if (decon->vsync_info.thread == ERR_PTR(-ENOMEM)) {
		decon_err("failed to run vsync thread\n");
		decon->vsync_info.thread = NULL;
	}

	return ret;
}

int decon_f_create_psr_thread(struct decon_device *decon)
{
	int ret = 0;

	ret = device_create_file(decon->dev, &dev_attr_psr_info);
	if (ret) {
		decon_err("failed to create psr info file\n");
		return ret;
	}

	return ret;
}

void decon_f_destroy_vsync_thread(struct decon_device *decon)
{
	device_remove_file(decon->dev, &dev_attr_vsync);
}

void decon_f_destroy_psr_thread(struct decon_device *decon)
{
	device_remove_file(decon->dev, &dev_attr_psr_info);
}

/************* LPD funtions ************************/
u32 decon_reg_get_cam_status(void __iomem *cam_status)
{
	if (cam_status)
		return readl(cam_status);
	else
		return 0xF;
}

static int decon_enter_lpd(struct decon_device *decon)
{
	int ret = 0;

#ifdef CONFIG_DECON_LPD_DISPLAY
	DISP_SS_EVENT_START();

	mutex_lock(&decon->lpd_lock);

	if (decon_is_lpd_blocked(decon))
		goto err2;

	decon_lpd_block(decon);
	if ((decon->state == DECON_STATE_LPD) ||
		(decon->state != DECON_STATE_ON)) {
		goto err;
	}

	exynos_ss_printk("%s +\n", __func__);
	decon_lpd_trig_reset(decon);
	decon->state = DECON_STATE_LPD_ENT_REQ;
	decon_disable(decon);
	decon->state = DECON_STATE_LPD;
	decon->tracing_mark_write( decon->systrace_pid, 'C', "decon_LPD", 1 );
	exynos_ss_printk("%s -\n", __func__);

	DISP_SS_EVENT_LOG(DISP_EVT_ENTER_LPD, &decon->sd, start);
err:
	decon_lpd_unblock(decon);
err2:
	mutex_unlock(&decon->lpd_lock);
#endif
	return ret;
}

int decon_exit_lpd(struct decon_device *decon)
{
	int ret = 0;

#ifdef CONFIG_DECON_LPD_DISPLAY
	DISP_SS_EVENT_START();

	decon_lpd_block(decon);
	flush_workqueue(decon->lpd_wq);
	mutex_lock(&decon->lpd_lock);

	if (decon->state != DECON_STATE_LPD)
		goto err;

	exynos_ss_printk("%s +\n", __func__);
	decon->state = DECON_STATE_LPD_EXIT_REQ;
	decon_enable(decon);
	decon_lpd_trig_reset(decon);
	decon->state = DECON_STATE_ON;
	decon->tracing_mark_write( decon->systrace_pid, 'C', "decon_LPD", 0 );
	exynos_ss_printk("%s -\n", __func__);

	DISP_SS_EVENT_LOG(DISP_EVT_EXIT_LPD, &decon->sd, start);
err:
	decon_lpd_unblock(decon);
	mutex_unlock(&decon->lpd_lock);
#endif
	return ret;
}

int decon_lpd_block_exit(struct decon_device *decon)
{
	int ret = 0;

	if (!decon || !decon->lpd_init_status)
		return 0;

	decon_lpd_block(decon);
	ret = decon_exit_lpd(decon);

	return ret;
}

#ifdef DECON_LPD_OPT
int decon_lcd_off(struct decon_device *decon)
{
	/* It cann't be used incase of PACKET_GO mode */
	int ret;

	decon_info("%s +\n", __func__);

	decon_lpd_block(decon);
	flush_workqueue(decon->lpd_wq);

	mutex_lock(&decon->lpd_lock);

	ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_LCD_OFF, NULL);
	if (ret < 0)
		decon_err("failed to turn off LCD\n");

	decon->state = DECON_STATE_OFF;

	mutex_unlock(&decon->lpd_lock);
	decon_lpd_unblock(decon);

	decon_info("%s -\n", __func__);

	return 0;
}
#endif

static void decon_lpd_handler(struct work_struct *work)
{
	struct decon_device *decon =
			container_of(work, struct decon_device, lpd_work);

	if (!decon || !decon->lpd_init_status)
		return;

	if (decon_lpd_enter_cond(decon))
		decon_enter_lpd(decon);
}

int decon_register_lpd_work(struct decon_device *decon)
{
	mutex_init(&decon->lpd_lock);

	atomic_set(&decon->lpd_trig_cnt, 0);
	atomic_set(&decon->lpd_block_cnt, 0);

	decon->lpd_wq = create_singlethread_workqueue("decon_lpd");
	if (decon->lpd_wq == NULL) {
		decon_err("%s:failed to create workqueue for LPD\n", __func__);
		return -ENOMEM;
	}

	INIT_WORK(&decon->lpd_work, decon_lpd_handler);
	decon->lpd_init_status = true;

	// recovery mode -> skip LPD
	if(decon_bootmode == 2)
		decon->lpd_init_status = false;

	if (lpcharge == 1)
		decon->lpd_init_status = false;

	return 0;
}
