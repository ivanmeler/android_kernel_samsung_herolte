/* linux/drivers/video/exynos_decon/dsim.h
 *
 * Header file for Samsung MIPI-DSI common driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Haowei Li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DSIM_H__
#define __DSIM_H__

#include <linux/device.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/io.h>
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>
#endif

#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <linux/debugfs.h>

#include "./panels/decon_lcd.h"
#include "regs-dsim.h"
#include "dsim_common.h"
#include "./panels/poc.h"
#include "./panels/mdnie.h"

#define DSIM_PAD_SINK		0
#define DSIM_PADS_NUM		1
#define DSIM_DDI_ID_LEN		3

#define DSIM_RX_FIFO_READ_DONE	(0x30800002)
#define DSIM_MAX_RX_FIFO	(64)

#define dsim_err(fmt, ...)					\
	do {							\
		pr_err(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define dsim_info(fmt, ...)					\
	do {							\
		pr_info(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define dsim_dbg(fmt, ...)					\
	do {							\
		pr_debug(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define call_panel_ops(q, op, args...)				\
	(((q)->panel_ops->op) ? ((q)->panel_ops->op(args)) : 0)

extern struct dsim_device *dsim0_for_decon;
extern struct dsim_device *dsim1_for_decon;

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED		1
#define PANEL_STATE_SUSPENDING	2

#define PANEL_DISCONNECTED		0
#define PANEL_CONNECTED			1

extern struct mipi_dsim_lcd_driver s6e3hf4_mipi_lcd_driver;

enum mipi_dsim_pktgo_state {
	DSIM_PKTGO_DISABLED,
	DSIM_PKTGO_STANDBY,
	DSIM_PKTGO_ENABLED
};

/* operation state of dsim driver */
enum dsim_state {
	DSIM_STATE_HSCLKEN,	/* HS clock was enabled. */
	DSIM_STATE_ULPS,	/* DSIM was entered ULPS state */
	DSIM_STATE_SUSPEND	/* DSIM is suspend state */
};

#ifdef CONFIG_LCD_DOZE_MODE
enum dsim_doze_mode {
	DSIM_DOZE_STATE_NORMAL = 0,
	DSIM_DOZE_STATE_DOZE,
	DSIM_DOZE_STATE_SUSPEND,
	DSIM_DOZE_STATE_DOZE_SUSPEND,
};
#endif
struct dsim_resources {
	struct clk *pclk;
	struct clk *dphy_esc;
	struct clk *dphy_byte;
	struct clk *rgb_vclk0;
	struct clk *pclk_disp;
	int lcd_power[2];
	int lcd_reset;
	struct regulator *regulator_30V;
	struct regulator *regulator_18V;
	struct regulator *regulator_16V;
};

#ifdef CONFIG_LCD_RES
typedef enum lcd_res_type {
	LCD_RES_DEFAULT = 0,
	LCD_RES_FHD = 1920,
	LCD_RES_HD = 1280,
	LCD_RES_MAX
} lcd_res_t;
#endif

struct panel_private {
	struct backlight_device *bd;
	unsigned char id[3];
	unsigned char code[5];

	// HF4 : 1st tset, 2 ~ 23 elvss, HA3 : 1 ~ 22 elvss, 30th tset, HA2 1 ~ 22 elvss
	unsigned char elvss_set[30];
	unsigned char elvss_len;
	unsigned char elvss_start_offset;
	unsigned char elvss_temperature_offset;
	unsigned char elvss_tset_offset;

	// only HA2 use(this code doesnot use)
	unsigned char tset_set[9];
	unsigned char tset_len;

	// ha3 1 ~ 10,
	unsigned char aid_set[11];
	unsigned char aid_len;
	unsigned char aid_reg_offset;

	unsigned char vint_set[3];
	unsigned char vint_len;
	unsigned char vint_table[20];
	unsigned int vint_dim_table[20];
	unsigned char vint_table_len;

	unsigned char **acl_cutoff_tbl;
	unsigned char **acl_opr_tbl;
	unsigned char irc_table[366][21];

	int	temperature;
	unsigned int coordinate[2];
	unsigned char date[7];
	unsigned int lcdConnected;
	unsigned int state;
	unsigned int br_index;
	unsigned int acl_enable;
	unsigned int caps_enable;
	unsigned int current_acl;
	unsigned int current_hbm;
	unsigned int current_vint;
	unsigned int siop_enable;

#ifdef CONFIG_CHECK_OCTA_CHIP_ID
	unsigned char octa_id[25];
#endif

	void *dim_data;
	void *dim_info;
	unsigned char *inter_aor_tbl;
	unsigned int *br_tbl;
	struct mutex lock;
	struct dsim_panel_ops *ops;
	unsigned int panel_type;
	unsigned char panel_rev;
	unsigned char panel_line;
	unsigned char panel_material;
	unsigned char current_model;
	unsigned int disp_type_gpio;
#ifdef CONFIG_LCD_WEAKNESS_CCB
	unsigned char current_ccb;
	unsigned int ccb_support;
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	unsigned int mdnie_support;
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_MCD
	unsigned int mcd_on;
#endif

#ifdef CONFIG_PANEL_GRAY_SPOT
	unsigned int gray_spot;
#endif

#ifdef CONFIG_PANEL_SMART_DIMMING
	unsigned int smart_on;
	void *smart_dim_data;
	void *smart_dim_info;
#endif

#ifdef CONFIG_LCD_HMT
	unsigned int hmt_on;
	unsigned int hmt_prev_status;
	unsigned int hmt_br_index;
	unsigned int hmt_brightness;
	void *hmt_dim_data;
	void *hmt_dim_info;
	unsigned int *hmt_br_tbl;
	unsigned int hmt_support;
#endif

#ifdef CONFIG_LCD_ALPM
	unsigned int 	alpm;
	unsigned int 	current_alpm;
	struct mutex	alpm_lock;
#endif
	unsigned int 	alpm_support;	// 0 : unsupport, 1 : 30hz, 2 : 1hz
	unsigned int	hlpm_support;	// 0 : unsupport, 1 : 30hz

#ifdef CONFIG_LCD_DOZE_MODE
	unsigned int alpm_mode;
	unsigned int curr_alpm_mode;
#endif

	int esd_disable;

#ifdef	CONFIG_LCD_RES
	lcd_res_t lcd_res;
#endif

	unsigned int adaptive_control;
	int lux;
	struct class *mdnie_class;

#ifdef CONFIG_DISPLAY_USE_INFO
	struct notifier_block dpui_notif;
#endif

#ifdef CONFIG_SUPPORT_POC_FLASH
	struct panel_poc_device poc_dev;
	unsigned int poc_op;
	unsigned int poc_allow_chechsum_read;
	unsigned char poc_ctrl_set[4];
	unsigned char poc_checksum_set[5];
#endif
};

struct dsim_panel_ops {
	int (*early_probe)(struct dsim_device *dsim);
	int	(*probe)(struct dsim_device *dsim);
	int	(*displayon)(struct dsim_device *dsim);
	int	(*exit)(struct dsim_device *dsim);
	int	(*init)(struct dsim_device *dsim);
	int (*dump)(struct dsim_device *dsim);
#ifdef CONFIG_LCD_DOZE_MODE
	int (*enteralpm)(struct dsim_device *dsim);
	int (*exitalpm)(struct dsim_device *dsim);
#endif
#ifdef CONFIG_FB_DSU
	int (*dsu_cmd)(struct dsim_device *dsim);
#endif
};

#define MAX_UNDERRUN_LIST		10
struct dsim_underrun_info {
	ktime_t time;
	unsigned long mif_freq;
	unsigned long int_freq;
	unsigned long disp_freq;
	unsigned int prev_bw;
	unsigned int cur_bw;
};

struct dsim_device {
	struct device *dev;
	void *decon;
	struct dsim_resources res;
	unsigned int irq;
	void __iomem *reg_base;

	enum dsim_state state;

	unsigned int data_lane;
	unsigned long hs_clk;
	unsigned long byte_clk;
	unsigned long escape_clk;
	unsigned char freq_band;
	struct notifier_block fb_notif;

	struct lcd_device	*lcd;
	unsigned int enabled;
	struct decon_lcd lcd_info;
	struct dphy_timing_value	timing;
	int	pktgo;
	int	glide_display_size;

	int id;
	u32 data_lane_cnt;
	struct mipi_dsim_lcd_driver *panel_ops;

	spinlock_t slock;
	struct mutex lock;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct panel_private priv;
	struct dsim_clks_param clks_param;
	struct timer_list		cmd_timer;
	struct phy *phy;

#ifdef CONFIG_LCD_ALPM
	int 			alpm;
#endif
#ifdef CONFIG_LCD_DOZE_MODE
	unsigned int dsim_doze;
#endif
#ifdef CONFIG_FB_DSU
	int dsu_xres;
	int dsu_yres;
	struct workqueue_struct *dsu_sysfs_wq;
	struct delayed_work dsu_sysfs_work;
	unsigned int	dsu_param_offset;
	unsigned int	dsu_param_value;
#endif
	bool req_display_on;

#ifdef CONFIG_DUMPSTATE_LOGGING
	struct dentry *debug_root;
	struct dentry *debug_info;
#endif
	int under_list_idx;
	struct dsim_underrun_info under_list[MAX_UNDERRUN_LIST];
	int total_underrun_cnt;
};

/**
 * driver structure for mipi-dsi based lcd panel.
 *
 * this structure should be registered by lcd panel driver.
 * mipi-dsi driver seeks lcd panel registered through name field
 * and calls these callback functions in appropriate time.
 */

struct mipi_dsim_lcd_driver {
	int (*early_probe)(struct dsim_device *dsim);
	int	(*probe)(struct dsim_device *dsim);
	int	(*suspend)(struct dsim_device *dsim);
	int	(*displayon)(struct dsim_device *dsim);
	int	(*resume)(struct dsim_device *dsim);
	int	(*dump)(struct dsim_device *dsim);
#ifdef CONFIG_LCD_DOZE_MODE
	int (*enteralpm)(struct dsim_device *dsim);
	int (*exitalpm)(struct dsim_device *dsim);
#endif
#ifdef CONFIG_FB_DSU
	int (*dsu_cmd)(struct dsim_device *dsim);
	int (*init)(struct dsim_device *dsim);
	int (*dsu_sysfs) (struct dsim_device *dsim);
#endif
};

int dsim_write_data(struct dsim_device *dsim, unsigned int data_id,
		unsigned long data0, unsigned int data1);
int dsim_read_data(struct dsim_device *dsim, u32 data_id, u32 addr,
		u32 count, u8 *buf);
int dsim_wait_for_cmd_done(struct dsim_device *dsim);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
void dsim_pkt_go_ready(struct dsim_device *dsim);
void dsim_pkt_go_enable(struct dsim_device *dsim, bool enable);
#endif

static inline struct dsim_device *get_dsim_drvdata(u32 id)
{
	if (id)
		return dsim1_for_decon;
	else
		return dsim0_for_decon;
}

static inline int dsim_rd_data(u32 id, u32 cmd_id,
	 u32 addr, u32 size, u8 *buf)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_read_data(dsim, cmd_id, addr, size, buf);
	if (ret)
		return ret;

	return 0;
}

static inline int dsim_wr_data(u32 id, u32 cmd_id, unsigned long d0, u32 d1)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_write_data(dsim, cmd_id, d0, d1);
	if (ret)
		return ret;

	return 0;
}

static inline int dsim_wait_for_cmd_completion(u32 id)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_wait_for_cmd_done(dsim);

	return ret;
}

/* register access subroutines */
static inline u32 dsim_read(u32 id, u32 reg_id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	return readl(dsim->reg_base + reg_id);
}

static inline u32 dsim_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dsim_read(id, reg_id);
	val &= (mask);
	return val;
}

static inline void dsim_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	writel(val, dsim->reg_base + reg_id);
}

static inline void dsim_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	u32 old = dsim_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->reg_base + reg_id);
}

u32 dsim_reg_get_yres(u32 id);
u32 dsim_reg_get_xres(u32 id);

#define DSIM_IOC_ENTER_ULPS		_IOW('D', 0, u32)
#define DSIM_IOC_LCD_OFF		_IOW('D', 1, u32)
#define DSIM_IOC_PKT_GO_ENABLE		_IOW('D', 2, u32)
#define DSIM_IOC_PKT_GO_DISABLE		_IOW('D', 3, u32)
#define DSIM_IOC_PKT_GO_READY		_IOW('D', 4, u32)
#define DSIM_IOC_GET_LCD_INFO		_IOW('D', 5, struct decon_lcd *)
#define DSIM_IOC_PARTIAL_CMD		_IOW('D', 6, u32)
#define DSIM_IOC_SET_PORCH		_IOW('D', 7, struct decon_lcd *)
#define DSIM_IOC_DUMP			_IOW('D', 8, u32)
#ifdef CONFIG_DSIM_ESD_REMOVE_DISP_DET
#define DSIM_IOC_GET_DDI_STATUS			_IOW('D', 10, u32)
#endif
#ifdef CONFIG_LCD_ESD_IDLE_MODE
#define DSIM_IOC_IDLE_MODE_CMD			_IOW('D', 11, u32)
#endif

#ifdef CONFIG_FB_DSU
#define DSIM_IOC_DSU_CMD            _IOW('D', 12, u32)
#define DSIM_IOC_DSU_DSC            _IOW('D', 13, u32)
#define DSIM_IOC_TE_ONOFF           _IOW('D', 14, u32)
#define DSIM_IOC_DSU_RECONFIG   _IOW('D', 15, u32)
#define DSIM_IOC_DISPLAY_ONOFF	    _IOW('D', 16, u32)
#define DSIM_IOC_REG_LOCK	_IOW('D', 17, u32)
#endif

#define DSIM_REQ_POWER_OFF		0
#define DSIM_REQ_POWER_ON		1
#ifdef CONFIG_LCD_DOZE_MODE
#define DSIM_REQ_DOZE_MODE		2
#define DSIM_REQ_DOZE_SUSPEND 	3
#endif
int dsim_write_hl_data(struct dsim_device *dsim, const u8 *cmd, u32 cmdSize);
int dsim_read_hl_data(struct dsim_device *dsim, u8 addr, u32 size, u8 *buf);

#ifdef CONFIG_LCD_HMT
void display_off_for_VR(struct dsim_device *dsim);
#endif

#endif /* __DSIM_H__ */
