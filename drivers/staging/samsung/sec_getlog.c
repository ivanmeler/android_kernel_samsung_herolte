/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code - support GetLog
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/memblock.h>

#if 0
static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *p_fb;		/* it must be physical address */
	u32 xres;
	u32 yres;
	u32 bpp;		/* color depth : 16 or 24 */
	u32 frames;		/* frame buffer count : 2 */
} frame_buf_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('f' << 24) | ('b' << 16) | ('u' << 8) | ('f' << 0)),
};

void sec_getlog_supply_fbinfo(void *p_fb, u32 xres, u32 yres, u32 bpp,
			      u32 frames)
{
	if (p_fb) {
		pr_info("%s: 0x%p %d %d %d %d\n", __func__, p_fb, xres, yres,
			bpp, frames);
		frame_buf_mark.p_fb = p_fb;
		frame_buf_mark.xres = xres;
		frame_buf_mark.yres = yres;
		frame_buf_mark.bpp = bpp;
		frame_buf_mark.frames = frames;
	}
}
EXPORT_SYMBOL(sec_getlog_supply_fbinfo);
#endif

static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	u32 log_mark_version;
	u32 framebuffer_mark_version;
	void *this;			/* this is used for addressing
					   log buffer in 2 dump files */
	struct {
		phys_addr_t size;	/* memory block's size */
		phys_addr_t addr;	/* memory block'sPhysical address */
	} mem[2];
} marks_ver_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('v' << 24) | ('e' << 16) | ('r' << 8) | ('s' << 0)),
#ifdef CONFIG_ARM64
	.log_mark_version = 2,
#else
	.log_mark_version = 1,
#endif
	.framebuffer_mark_version = 1,
	.this = &marks_ver_mark,
};

/* mark for GetLog extraction */
static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *p_main;
	void *p_radio;
	void *p_events;
	void *p_system;
} plat_log_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('p' << 24) | ('l' << 16) | ('o' << 8) | ('g' << 0)),
};

void sec_getlog_supply_platform(unsigned char *buffer, char *name)
{
	pr_info("sec_debug: GetLog support (mark:0x%p, %s:\t0x%p)\n", &plat_log_mark, name, buffer);

	if(!strcmp(name, "log_main"))
		plat_log_mark.p_main = buffer;
	else if(!strcmp(name, "log_radio"))
		plat_log_mark.p_radio = buffer;
	else if(!strcmp(name, "log_events"))
		plat_log_mark.p_events = buffer;
	else if(!strcmp(name, "log_system"))
		plat_log_mark.p_system = buffer;
}
EXPORT_SYMBOL(sec_getlog_supply_platform);

static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *klog_buf;
} kernel_log_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('k' << 24) | ('l' << 16) | ('o' << 8) | ('g' << 0)),
};

void sec_getlog_supply_kernel(void *klog_buf)
{
	pr_info("sec_debug: GetLog support (mark:0x%p, klog: 0x%p)\n", &kernel_log_mark, klog_buf);
	kernel_log_mark.klog_buf = klog_buf;
}
EXPORT_SYMBOL(sec_getlog_supply_kernel);

static int __init sec_getlog_init(void)
{
#ifdef CONFIG_ARM64
/*
	marks_ver_mark.mem[0].size = (memblock.memory.regions)->size;
//	marks_ver_mark.mem[0].addr = (memblock.memory.regions)->base;
	marks_ver_mark.mem[0].addr = ((unsigned long)0xFFFFFFC000000000 +
			(memblock.memory.regions)->base);
	
	pr_info("sec_debug: GetLog support (memblock size=0x%lx, addr=0x%lx)\n", 
			(unsigned long)marks_ver_mark.mem[0].size,
			(unsigned long)marks_ver_mark.mem[0].addr);
*/
	pr_info("sec_debug: GetLog support (ver:0x%p)\n", &marks_ver_mark);
#else
	marks_ver_mark.mem[0].size =
		meminfo.bank[0].size + meminfo.bank[1].size;
	marks_ver_mark.mem[0].addr = meminfo.bank[0].start;
	marks_ver_mark.mem[1].size =
		meminfo.bank[2].size + meminfo.bank[3].size;
	marks_ver_mark.mem[1].addr = meminfo.bank[2].start;

	pr_info("sec_debug: GetLog support (mem[0] = 0x%x@0x%x mem[1] = 0x%x@0x%x)\n",
			marks_ver_mark.mem[0].size, marks_ver_mark.mem[0].addr,
			marks_ver_mark.mem[1].size, marks_ver_mark.mem[1].addr);
#endif

	return 0;
}

core_initcall(sec_getlog_init);
