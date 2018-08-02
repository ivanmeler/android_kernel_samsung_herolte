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

#ifndef ___SAMSUNG_DECON_H__
#define ___SAMSUNG_DECON_H__

#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <media/exynos_mc.h>

#include "regs-decon.h"
#include "decon_common.h"
#include "./panels/decon_lcd.h"
#include "dsim.h"

extern struct ion_device *ion_exynos;
extern struct decon_device *decon_f_drvdata;
extern struct decon_device *decon_s_drvdata;
extern struct decon_device *decon_t_drvdata;
extern int decon_log_level;

extern struct decon_bts decon_bts_control;
extern struct decon_init_bts decon_init_bts_control;
extern struct decon_bts2 decon_bts2_control;

#define NUM_DECON_IPS		(3)
#define DRIVER_NAME		"decon"
#define MAX_NAME_SIZE		32

#define MAX_DECON_PADS		9
#define DECON_PAD_WB		8

#define MAX_BUF_PLANE_CNT	3
#define DECON_ENTER_LPD_CNT	3
#define MIN_BLK_MODE_WIDTH	144
#define MIN_BLK_MODE_HEIGHT	16

#define VSYNC_TIMEOUT_MSEC	200
#define MAX_BW_PER_WINDOW	(2560 * 1600 * 4 * 60)
#define LCD_DEFAULT_BPP		24

#define DECON_TZPC_OFFSET	3
#define MAX_DMA_TYPE		10
#define MAX_FRM_DONE_WAIT	34

#define DISP_UTIL		70
#define DECON_INT_UTIL		65
#ifndef NO_CNT_TH
/* BTS driver defines the Threshold */
#define	NO_CNT_TH		10
#endif

#define DECON_PIX_PER_CLK	2

#define UNDERRUN_FILTER_INTERVAL_MS    100
#define UNDERRUN_FILTER_INIT           0
#define UNDERRUN_FILTER_IDLE           1
#define DECON_UNDERRUN_THRESHOLD	0
#ifdef CONFIG_FB_WINDOW_UPDATE
#define DECON_WIN_UPDATE_IDX	(8)
#define decon_win_update_dbg(fmt, ...)					\
	do {								\
		if (decon_log_level >= 7)				\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)
#else
#define decon_win_update_dbg(fmt, ...) while (0)
#endif

#ifndef KHZ
#define KHZ (1000)
#endif

#ifndef MHZ
#define MHZ (1000*1000)
#endif

#define decon_err(fmt, ...)							\
	do {									\
		if (decon_log_level >= 3) {					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
			exynos_ss_printk(fmt, ##__VA_ARGS__);			\
		}								\
	} while (0)

#define decon_warn(fmt, ...)							\
	do {									\
		if (decon_log_level >= 4) {					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
			exynos_ss_printk(fmt, ##__VA_ARGS__);			\
		}								\
	} while (0)

#define decon_info(fmt, ...)							\
	do {									\
		if (decon_log_level >= 6)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define decon_dbg(fmt, ...)							\
	do {									\
		if (decon_log_level >= 8)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define decon_bts(fmt, ...)							\
	do {									\
		if (decon_log_level >= 7)					\
			pr_info("[BTS]"pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define decon_cfw_dbg(fmt, ...)							\
	do {									\
		if (decon_log_level >= 7)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define call_bts_ops(q, op, args...)				\
	(((q)->bts_ops->op) ? ((q)->bts_ops->op(args)) : 0)

#define call_init_ops(q, op, args...)				\
		(((q)->bts_init_ops->op) ? ((q)->bts_init_ops->op(args)) : 0)


/*
 * DECON_STATE_ON : disp power on, decon/dsim clock on & lcd on
 * DECON_STATE_LPD_ENT_REQ : disp power on, decon/dsim clock on, lcd on & request for LPD
 * DECON_STATE_LPD_EXIT_REQ : disp power off, decon/dsim clock off, lcd on & request for LPD exit.
 * DECON_STATE_LPD : disp power off, decon/dsim clock off & lcd on
 * DECON_STATE_OFF : disp power off, decon/dsim clock off & lcd off
 */
enum decon_state {
	DECON_STATE_INIT = 0,
	DECON_STATE_ON,
	DECON_STATE_LPD_ENT_REQ,
	DECON_STATE_LPD_EXIT_REQ,
	DECON_STATE_LPD,
	DECON_STATE_OFF
};

struct exynos_decon_platdata {
	enum decon_psr_mode	psr_mode;
	enum decon_trig_mode	trig_mode;
	enum decon_dsi_mode	dsi_mode;
	enum decon_output_type	out_type;
	int	out_idx;	/* dsim index */
	int	out1_idx;	/* dsim index for dual DSI */
	int	max_win;
	int	default_win;
};

struct decon_vsync {
	wait_queue_head_t	wait;
	ktime_t			timestamp;
	bool			active;
	int			irq_refcount;
	struct mutex		irq_lock;
	struct task_struct	*thread;
};

/*
 * @width: The width of display in mm
 * @height: The height of display in mm
 */
struct decon_fb_videomode {
	struct fb_videomode videomode;
	unsigned short width;
	unsigned short height;

	u8 cs_setup_time;
	u8 wr_setup_time;
	u8 wr_act_time;
	u8 wr_hold_time;
	u8 auto_cmd_rate;
	u8 frame_skip:2;
	u8 rs_pol:1;
};

struct decon_fb_pd_win {
	struct decon_fb_videomode win_mode;

	unsigned short		default_bpp;
	unsigned short		max_bpp;
	unsigned short		virtual_x;
	unsigned short		virtual_y;
	unsigned short		width;
	unsigned short		height;
};

struct decon_dma_buf_data {
	struct ion_handle		*ion_handle;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sg_table;
	dma_addr_t			dma_addr;
	struct sync_fence		*fence;
};

struct decon_win_rect {
	int x;
	int y;
	u32 w;
	u32 h;
};

struct decon_resources {
	struct clk *dpll;
	struct clk *pclk;
	struct clk *eclk;
	struct clk *eclk_leaf;
	struct clk *vclk;
	struct clk *vclk_leaf;
};

struct decon_rect {
	int	left;
	int	top;
	int	right;
	int	bottom;
};

struct decon_win {
	struct decon_fb_pd_win		windata;
	struct decon_device		*decon;
	struct fb_info			*fbinfo;
	struct media_pad		pad;

	struct decon_fb_videomode	win_mode;
	struct decon_dma_buf_data	dma_buf_data[MAX_BUF_PLANE_CNT];
	int				plane_cnt;
	struct fb_var_screeninfo	prev_var;
	struct fb_fix_screeninfo	prev_fix;

	int	fps;
	int	index;
	int	use;
	int	local;
	unsigned long	state;
	int	vpp_id;
	u32	pseudo_palette[16];
};

struct decon_user_window {
	int x;
	int y;
};

struct s3c_fb_user_plane_alpha {
	int		channel;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3c_fb_user_chroma {
	int		enabled;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3c_fb_user_ion_client {
	int	fd[MAX_BUF_PLANE_CNT];
	int	offset;
};

enum decon_pixel_format {
	/* RGB 32bit */
	DECON_PIXEL_FORMAT_ARGB_8888 = 0,
	DECON_PIXEL_FORMAT_ABGR_8888,
	DECON_PIXEL_FORMAT_RGBA_8888,
	DECON_PIXEL_FORMAT_BGRA_8888,
	DECON_PIXEL_FORMAT_XRGB_8888,
	DECON_PIXEL_FORMAT_XBGR_8888,
	DECON_PIXEL_FORMAT_RGBX_8888,
	DECON_PIXEL_FORMAT_BGRX_8888,
	/* RGB 16 bit */
	DECON_PIXEL_FORMAT_RGBA_5551,
	DECON_PIXEL_FORMAT_RGB_565,
	/* YUV422 2P */
	DECON_PIXEL_FORMAT_NV16,
	DECON_PIXEL_FORMAT_NV61,
	/* YUV422 3P */
	DECON_PIXEL_FORMAT_YVU422_3P,
	/* YUV420 2P */
	DECON_PIXEL_FORMAT_NV12,
	DECON_PIXEL_FORMAT_NV21,
	DECON_PIXEL_FORMAT_NV12M,
	DECON_PIXEL_FORMAT_NV21M,
	/* YUV420 3P */
	DECON_PIXEL_FORMAT_YUV420,
	DECON_PIXEL_FORMAT_YVU420,
	DECON_PIXEL_FORMAT_YUV420M,
	DECON_PIXEL_FORMAT_YVU420M,
	DECON_PIXEL_FORMAT_NV12N,
	DECON_PIXEL_FORMAT_NV12N_10B,

	DECON_PIXEL_FORMAT_MAX,
};

enum decon_blending {
	DECON_BLENDING_NONE = 0,
	DECON_BLENDING_PREMULT = 1,
	DECON_BLENDING_COVERAGE = 2,
	DECON_BLENDING_MAX = 3,
};

enum vpp_rotate {
	VPP_ROT_NORMAL = 0x0,
	VPP_ROT_XFLIP,
	VPP_ROT_YFLIP,
	VPP_ROT_180,
	VPP_ROT_90,
	VPP_ROT_90_XFLIP,
	VPP_ROT_90_YFLIP,
	VPP_ROT_270,
};

enum vpp_csc_eq {
	BT_601_NARROW = 0x0,
	BT_601_WIDE,
	BT_709_NARROW,
	BT_709_WIDE,
};

enum vpp_stop_status {
	VPP_STOP_NORMAL = 0x0,
	VPP_STOP_LPD,
	VPP_STOP_ERR,
};

enum vpp_port_num {
	VPP_PORT_NUM0 = 0,
	VPP_PORT_NUM1,
	VPP_PORT_NUM2,
	VPP_PORT_NUM3,
	VPP_PORT_MAX,
};
struct vpp_params {
	dma_addr_t addr[MAX_BUF_PLANE_CNT];
	enum vpp_rotate rot;
	enum vpp_csc_eq eq_mode;
};

struct decon_frame {
	int x;
	int y;
	u32 w;
	u32 h;
	u32 f_w;
	u32 f_h;
};

struct decon_win_config {
	enum {
		DECON_WIN_STATE_DISABLED = 0,
		DECON_WIN_STATE_COLOR,
		DECON_WIN_STATE_BUFFER,
		DECON_WIN_STATE_UPDATE,
	} state;

	/* Reusability:This struct is used for IDMA and ODMA */
	union {
		__u32 color;
		struct {
			int				fd_idma[3];
			int				fence_fd;
			int				plane_alpha;
			enum decon_blending		blending;
			enum decon_idma_type		idma_type;
			enum decon_pixel_format		format;
			struct vpp_params		vpp_parm;
			/* no read area of IDMA */
			struct decon_win_rect		block_area;
			struct decon_win_rect		transparent_area;
			struct decon_win_rect		opaque_area;
			/* source framebuffer coordinates */
			struct decon_frame		src;
		};
	};

	/* destination OSD coordinates */
	struct decon_frame dst;
	bool protection;
	bool compression;
};

struct decon_sbuf_data {
	struct list_head	list;
	unsigned long		addr;
	unsigned int		len;
	unsigned int		id;
};
struct decon_reg_data {
	struct list_head		list;
	struct decon_window_regs	win_regs[MAX_DECON_WIN];
	struct decon_dma_buf_data	dma_buf_data[MAX_DECON_WIN + 1][MAX_BUF_PLANE_CNT];
	unsigned int			bandwidth;
	unsigned int			num_of_window;
	u32				win_overlap_cnt;
	u32				bw;
	u32				disp_bw;
	u32                             int_bw;
	struct decon_rect		overlap_rect;
	struct decon_win_config		vpp_config[MAX_DECON_WIN + 1];
	struct decon_win_rect		block_rect[MAX_DECON_WIN];
#ifdef CONFIG_FB_WINDOW_UPDATE
	struct decon_win_rect		update_win;
	bool				need_update;
#endif
	bool				protection[MAX_DECON_WIN];
	struct list_head		sbuf_pend_list;
	u32 which_dsi;
};


#define WIN_DSI0_UPDATE 0x8000
#define WIN_DSI1_UPDATE 0x0080
#define WIN_BOTH_UPATE (WIN_DSI0_UPDATE | WIN_DSI1_UPDATE)

struct decon_win_config_data {
	int	fence;
	int	fd_odma;
	struct decon_win_config config[MAX_DECON_WIN + 1];
};

union decon_ioctl_data {
	struct decon_user_window user_window;
	struct s3c_fb_user_plane_alpha user_alpha;
	struct s3c_fb_user_chroma user_chroma;
	struct decon_win_config_data win_data;
	u32 vsync;
};

struct decon_underrun_stat {
	int	prev_bw;
	int	prev_int_bw;
	int	prev_disp_bw;
	int	chmap;
	int	fifo_level;
	int	underrun_cnt;
	int	total_underrun_cnt;
	unsigned long aclk;
	unsigned long lh_disp0;
	unsigned long mif_pll;
	unsigned long used_windows;
};

struct disp_ss_size_info {
	u32 w_in;
	u32 h_in;
	u32 w_out;
	u32 h_out;
};

struct decon_init_bts {
	void	(*bts_add)(struct decon_device *decon);
	void	(*bts_set_init)(struct decon_device *decon);
	void	(*bts_release_init)(struct decon_device *decon);
	void	(*bts_remove)(struct decon_device *decon);
};

struct vpp_dev;

struct decon_bts {
	void	(*bts_get_bw)(struct vpp_dev *vpp);
	void	(*bts_set_calc_bw)(struct vpp_dev *vpp);
	void	(*bts_set_zero_bw)(struct vpp_dev *vpp);
	void	(*bts_set_rot_mif)(struct vpp_dev *vpp);
};

struct decon_bts2 {
	void (*bts_init)(struct decon_device *decon);
	void (*bts_calc_bw)(struct decon_device *decon);
	void (*bts_update_bw)(struct decon_device *decon, u32 is_after);
	void (*bts_release_bw)(struct decon_device *decon);
	void (*bts_release_vpp)(struct vpp_dev *vpp);
	void (*bts_deinit)(struct decon_device *decon);
};

#ifdef CONFIG_DECON_EVENT_LOG
/**
 * Display Subsystem event management status.
 *
 * These status labels are used internally by the DECON to indicate the
 * current status of a device with operations.
 */
typedef enum disp_ss_event_type {
	/* Related with FB interface */
	DISP_EVT_BLANK = 0,
	DISP_EVT_UNBLANK,
	DISP_EVT_ACT_VSYNC,
	DISP_EVT_DEACT_VSYNC,
	DISP_EVT_WIN_CONFIG,

	/* Related with interrupt */
	DISP_EVT_TE_INTERRUPT,
	DISP_EVT_UNDERRUN,
	DISP_EVT_DECON_FRAMEDONE,
	DISP_EVT_DSIM_FRAMEDONE,
	DISP_EVT_RSC_CONFLICT,

	/* Related with async event */
	DISP_EVT_UPDATE_HANDLER,
	DISP_EVT_DSIM_COMMAND,
	DISP_EVT_TRIG_MASK,
	DISP_EVT_TRIG_UNMASK,
	DISP_EVT_DECON_FRAMEDONE_WAIT,
	DISP_EVT_DECON_SHUTDOWN,
	DISP_EVT_DSIM_SHUTDOWN,

	/* Related with VPP */
	DISP_EVT_VPP_WINCON,
	DISP_EVT_VPP_FRAMEDONE,
	DISP_EVT_VPP_STOP,
	DISP_EVT_VPP_UPDATE_DONE,
	DISP_EVT_VPP_SHADOW_UPDATE,
	DISP_EVT_VPP_SUSPEND,
	DISP_EVT_VPP_RESUME,

	/* Related with PM */
	DISP_EVT_DECON_SUSPEND,
	DISP_EVT_DECON_RESUME,
	DISP_EVT_ENTER_LPD,
	DISP_EVT_EXIT_LPD,
	DISP_EVT_DSIM_SUSPEND,
	DISP_EVT_DSIM_RESUME,
	DISP_EVT_ENTER_ULPS,
	DISP_EVT_EXIT_ULPS,

	DISP_EVT_LINECNT_ZERO,

	/* write-back events */
	DISP_EVT_WB_SET_BUFFER,
	DISP_EVT_WB_SW_TRIGGER,

	DISP_EVT_MAX, /* End of EVENT */
} disp_ss_event_t;

/* Related with PM */
struct disp_log_pm {
	u32 pm_status;		/* ACTIVE(1) or SUSPENDED(0) */
	ktime_t elapsed;	/* End time - Start time */
};

/* Related with S3CFB_WIN_CONFIG */
struct decon_update_reg_data {
	struct decon_window_regs 	win_regs[MAX_DECON_WIN];
	struct decon_win_config 	win_config[MAX_DECON_WIN + 1];
	struct decon_win_rect 		win;
};

/* Related with MIPI COMMAND read/write */
#define DISP_CALLSTACK_MAX 4
struct dsim_log_cmd_buf {
	u32 id;
	u8 buf;
	void *caller[DISP_CALLSTACK_MAX];
};

/* Related with VPP */
struct disp_log_vpp {
	u32 id;
	u32 start_cnt;
	u32 done_cnt;
	u32 width;
	u32 height;
};

typedef enum disp_esd_irq {
	irq_no_esd = 0,
	irq_pcd_det,
	irq_err_fg,
	irq_disp_det
} disp_esd_irq_t;

struct esd_protect {
	u32 pcd_irq;
	u32 err_irq;
	u32 disp_det_irq;
	u32 pcd_gpio;
	u32 disp_det_gpio;
	struct workqueue_struct *esd_wq;
	struct work_struct esd_work;
	u32	queuework_pending;
	int irq_disable;
	disp_esd_irq_t irq_type;
	ktime_t when_irq_enable;
};

/**
 * struct disp_ss_log - Display Subsystem Log
 * This struct includes DECON/DSIM/VPP
 */
struct disp_ss_log {
	ktime_t time;
	disp_ss_event_t type;
	union {
		struct disp_log_vpp vpp;
		struct decon_update_reg_data reg;
		struct dsim_log_cmd_buf cmd_buf;
		struct disp_log_pm pm;
	} data;
};

struct disp_ss_size_err_info {
	ktime_t time;
	struct disp_ss_size_info info;
};

/* Definitions below are used in the DECON */
#define	DISP_EVENT_LOG_MAX	SZ_1K
#define	DISP_EVENT_PRINT_MAX	512
#define	DISP_EVENT_SIZE_ERR_MAX	16
typedef enum disp_ss_event_log_level_type {
	DISP_EVENT_LEVEL_LOW = 0,
	DISP_EVENT_LEVEL_HIGH,
} disp_ss_log_level_t;

/* APIs below are used in the DECON/DSIM/VPP driver */
#define DISP_SS_EVENT_START() ktime_t start = ktime_get()
void DISP_SS_EVENT_LOG(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time);
void DISP_SS_EVENT_LOG_WINCON(struct v4l2_subdev *sd, struct decon_reg_data *regs);
void DISP_SS_EVENT_LOG_WINCON2(struct v4l2_subdev *sd, struct decon_reg_data *regs);
void DISP_SS_EVENT_LOG_CMD(struct v4l2_subdev *sd, u32 cmd_id, unsigned long data);
void DISP_SS_EVENT_SHOW(struct seq_file *s, struct decon_device *decon);
void DISP_SS_EVENT_SIZE_ERR_LOG(struct v4l2_subdev *sd, struct disp_ss_size_info *info);
#else /*!*/
#define DISP_SS_EVENT_START(...) do { } while(0)
#define DISP_SS_EVENT_LOG(...) do { } while(0)
#define DISP_SS_EVENT_LOG_WINCON(...) do { } while(0)
#define DISP_SS_EVENT_LOG_CMD(...) do { } while(0)
#define DISP_SS_EVENT_SHOW(...) do { } while(0)
#define DISP_SS_EVENT_SIZE_ERR_LOG(...) do { } while(0)
#endif

/**
* END of CONFIG_DECON_EVENT_LOG
*/

#define MAX_VPP_LOG	10

struct vpp_drm_log {
	unsigned long long time;
	int decon_id;
	int line;
	bool protection;
};

/* VPP resource management */
enum decon_dma_state {
	DMA_IDLE = 0,
	DMA_RSVD,
};

struct dma_rsm {
/* TODO: 1) BLK power & Clk management. 2) locking mechanism */
	u32	dma_state[MAX_DMA_TYPE-1];
	u32	decon_dma_map[MAX_DMA_TYPE-1];
};

typedef struct decon_device decon_dev;
struct decon_device {
	decon_dev			*decon[NUM_DECON_IPS];
	struct dma_rsm			*dma_rsm;
	u32				default_idma;
	void __iomem			*regs;
	void __iomem			*ss_regs;
	void __iomem			*eint_pend;
	void __iomem			*eint_mask;
	struct device			*dev;
	struct exynos_decon_platdata	*pdata;
	struct media_pad		pads[MAX_DECON_PADS];
	struct v4l2_subdev		sd;
	struct decon_win		*windows[MAX_DECON_WIN];
	struct decon_resources		res;
	struct v4l2_subdev		*output_sd;
	/* 2nd DSIM sub-device ptr for dual DSI mode */
	struct v4l2_subdev		*output_sd1;
	struct exynos_md		*mdev;

	struct mutex			update_regs_list_lock;
	struct list_head		update_regs_list;
	int				update_regs_list_cnt;
	struct task_struct		*update_regs_thread;
	struct kthread_worker		update_regs_worker;
	struct kthread_work		update_regs_work;
	struct mutex			lpd_lock;
	struct work_struct		lpd_work;
	struct workqueue_struct		*lpd_wq;
	atomic_t			lpd_trig_cnt;
	atomic_t			lpd_block_cnt;
	bool				lpd_init_status;

	struct ion_client		*ion_client;
	struct sw_sync_timeline		*timeline;
	int				timeline_max;

	struct mutex			output_lock;
	struct mutex			mutex;
	spinlock_t			slock;
	struct decon_vsync		vsync_info;
	enum decon_state		state;

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
	u32				total_bw;
	u32				max_peak_bw;
	u32				prev_total_bw;
	u32				prev_max_peak_bw;
	u32				num_of_win;
	u32				prev_num_of_win;

	/*
	 * max current DISP INT channel
	 *
	 * ACLK_DISP0_0_400 : G0 + VG0
	 * ACLK_DISP0_1_400 : G1 + VG1
	 * ACLK_DISP1_0_400 : G2 + VGR0
	 * ACLK_DISP1_1_400 : G3 + VGR1
	 */
	u64				max_disp_ch;
	u64				prev_max_disp_ch;

	u32				mic_factor;
	u32				vclk_factor;
	struct decon_bts2		*bts2_ops;
#endif

	u32				prev_bw;
	u32				prev_disp_bw;
	u32				prev_int_bw;
	u32				prev_frame_has_yuv;
	u32				cur_frame_has_yuv;
	int				default_bw;
	int				cur_overlap_cnt;
	int				id;
	int				n_sink_pad;
	int				n_src_pad;
	union decon_ioctl_data		ioctl_data;
	bool				vpp_used[MAX_VPP_SUBDEV];
	bool				vpp_err_stat[MAX_VPP_SUBDEV];
	u32				vpp_usage_bitmask;
	struct decon_lcd		*lcd_info;

	struct decon_win_rect		update_win;
	bool				need_update;

	struct decon_underrun_stat	underrun_stat;
	void __iomem			*cam_status[2];
	u32				prev_protection_bitmask;
	u32				cur_protection_bitmask;
	struct list_head		sbuf_active_list;

	unsigned int			irq;
	struct dentry			*debug_root;
#ifdef CONFIG_DECON_EVENT_LOG
	struct dentry			*debug_event;
	struct disp_ss_log		disp_ss_log[DISP_EVENT_LOG_MAX];
	atomic_t			disp_ss_log_idx;
	disp_ss_log_level_t		disp_ss_log_level;
	u32				disp_ss_size_log_idx;
	struct disp_ss_size_err_info	disp_ss_size_log[DISP_EVENT_SIZE_ERR_MAX];
#endif
	struct pinctrl			*pinctrl;
    struct pinctrl_state 		*decon_te_on;
    struct pinctrl_state		*decon_te_off;
	struct pm_qos_request		mif_qos;
	struct pm_qos_request		int_qos;
	struct pm_qos_request		disp_qos;
	u32				disp_cur;
	u32				disp_prev;
	struct decon_init_bts		*bts_init_ops;

	int				frame_done_cnt_cur;
	int				frame_done_cnt_target;
	int				frame_start_cnt_cur;
	int				frame_start_cnt_target;
	wait_queue_head_t		wait_frmdone;
	wait_queue_head_t		wait_vstatus;
	ktime_t				trig_mask_timestamp;
	int                             frame_idle;
	int				eint_status;
	struct work_struct		fifo_irq_work;
	struct workqueue_struct		*fifo_irq_wq;
	int				fifo_irq_status;
	bool	ignore_vsync;
	struct esd_protect esd;
	unsigned int			force_fullupdate;
	int	systrace_pid;
	void	(*tracing_mark_write)( int pid, char id, char* str1, int value );
#ifdef CONFIG_EXYNOS_DECON_DUAL_DSI
 	int dual_status;
	int dispon_req;
#endif

};

static inline struct decon_device *get_decon_drvdata(u32 id)
{
        if (id == 0)
                return decon_f_drvdata;
        else if (id == 1)
                return decon_s_drvdata;
	else
		return decon_t_drvdata;
}

/* register access subroutines */
static inline u32 decon_read(u32 id, u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(id);
	return readl(decon->regs + reg_id);
}

static inline u32 decon_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = decon_read(id, reg_id);
	val &= (mask);
	return val;
}

static inline void decon_write(u32 id, u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(id);
	writel(val, decon->regs + reg_id);
}

static inline void decon_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	u32 old = decon_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	decon_write(id, reg_id, val);
}

static inline u32 dsc_read(u32 dsc_id, u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u32 dsc_offset = dsc_id ? DSC1_OFFSET : DSC0_OFFSET;

	return readl(decon->regs + dsc_offset + reg_id);
}

static inline void dsc_write(u32 dsc_id, u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u32 dsc_offset = dsc_id ? DSC1_OFFSET : DSC0_OFFSET;

	writel(val, decon->regs + dsc_offset + reg_id);
}

static inline void dsc_write_mask(u32 dsc_id, u32 reg_id, u32 val, u32 mask)
{
	u32 old = dsc_read(dsc_id, reg_id);

	val = (val & mask) | (old & ~mask);
	dsc_write(dsc_id, reg_id, val);
}

static inline u32 dsc_round_up(u32 x, u32 a)
{
	u32 remained = x % a;

	if (!remained)
		return x;

	return x + a - remained;
}

static inline void dsc_set_mask(u32 *reg, u32 val, u32 mask)
{
	*reg &= ~mask;
	*reg |= val & mask;
}

static inline u32 dsc_pps_index(u32 offset)
{
	return ((offset - DSC_PPS00_03) >> 2);
}

static inline u32 CEIL_DIV(u32 a, u32 b)
{
	return (a + (b - 1)) / b;
}

/* common function API */
bool decon_validate_x_alignment(struct decon_device *decon, int x, u32 w,
		u32 bits_per_pixel);
int create_link_mipi(struct decon_device *decon, int id);
int decon_int_remap_eint(struct decon_device *decon);
int decon_f_register_irq(struct platform_device *pdev, struct decon_device *decon);
int decon_s_register_irq(struct platform_device *pdev, struct decon_device *decon);
int decon_t_register_irq(struct platform_device *pdev, struct decon_device *decon);
irqreturn_t decon_f_irq_handler(int irq, void *dev_data);
irqreturn_t decon_s_irq_handler(int irq, void *dev_data);
irqreturn_t decon_t_irq_handler(int irq, void *dev_data);
int decon_f_get_clocks(struct decon_device *decon);
int decon_s_get_clocks(struct decon_device *decon);
int decon_t_get_clocks(struct decon_device *decon);
void decon_f_set_clocks(struct decon_device *decon);
void decon_s_set_clocks(struct decon_device *decon);
void decon_t_set_clocks(struct decon_device *decon);
int decon_t_set_lcd_info(struct decon_device *decon);
int decon_register_lpd_work(struct decon_device *decon);
int decon_exit_lpd(struct decon_device *decon);
int decon_lpd_block_exit(struct decon_device *decon);
int decon_lcd_off(struct decon_device *decon);
int decon_enable(struct decon_device *decon);
int decon_disable(struct decon_device *decon);
int decon_tui_protection(struct decon_device *decon, bool tui_en);
int decon_wait_for_vsync(struct decon_device *decon, u32 timeout);

/* internal only function API */
int decon_config_eint_for_te(struct platform_device *pdev, struct decon_device *decon);
int decon_f_create_vsync_thread(struct decon_device *decon);
int decon_f_create_psr_thread(struct decon_device *decon);
void decon_f_destroy_vsync_thread(struct decon_device *decon);
void decon_f_destroy_psr_thread(struct decon_device *decon);
int decon_set_lcd_config(struct decon_device *decon);
int decon_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
int decon_set_par(struct fb_info *info);
int decon_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
int decon_setcolreg(unsigned regno,
			    unsigned red, unsigned green, unsigned blue,
			    unsigned transp, struct fb_info *info);
int decon_mmap(struct fb_info *info, struct vm_area_struct *vma);

/* POWER and ClOCK API */
int init_display_decon_clocks(struct device *dev);
int disable_display_decon_clocks(struct device *dev);
void decon_clock_on(struct decon_device *decon);
void decon_clock_off(struct decon_device *decon);
u32 decon_reg_get_cam_status(void __iomem *);
void decon_reg_set_block_mode(u32 id, u32 win_idx, u32 x, u32 y, u32 h, u32 w, u32 enable);
void decon_reg_set_tui_va(u32 id, u32 va);
void decon_vpp_min_lock_release(struct decon_device *decon);
u32 wincon(u32 transp_len, u32 a0, u32 a1, int plane_alpha, enum decon_blending blending);
int find_subdev_mipi(struct decon_device *decon, int id);
#define is_rotation(config) (config->vpp_parm.rot >= VPP_ROT_90)

//support decon notify
extern int decon_register_client(struct notifier_block *nb);
extern int decon_unregister_client(struct notifier_block *nb);
extern int decon_notifier_call_chain(unsigned long val, void *v);
/* LPD related */
static inline void decon_lpd_block(struct decon_device *decon)
{
	if (decon)
		atomic_inc(&decon->lpd_block_cnt);
}

static inline bool decon_is_lpd_blocked(struct decon_device *decon)
{
	return (atomic_read(&decon->lpd_block_cnt) > 0);
}

static inline int decon_get_lpd_block_cnt(struct decon_device *decon)
{
	return (atomic_read(&decon->lpd_block_cnt));
}

static inline void decon_lpd_unblock(struct decon_device *decon)
{
	if (decon) {
		if (decon_is_lpd_blocked(decon))
			atomic_dec(&decon->lpd_block_cnt);
	}
}

static inline void decon_lpd_block_reset(struct decon_device *decon)
{
	if (decon)
		atomic_set(&decon->lpd_block_cnt, 0);
}

static inline void decon_lpd_trig_reset(struct decon_device *decon)
{
	if (decon)
		atomic_set(&decon->lpd_trig_cnt, 0);
}

static inline bool is_cam_not_running(struct decon_device *decon)
{
	if (!decon->id)
		return (!((decon_reg_get_cam_status(decon->cam_status[0]) & 0xF) ||
		(decon_reg_get_cam_status(decon->cam_status[1]) & 0xF)));
	else
		return true;
}
static inline bool decon_lpd_enter_cond(struct decon_device *decon)
{
	return ((atomic_read(&decon->lpd_block_cnt) <= 0) && is_cam_not_running(decon)
		&& (atomic_inc_return(&decon->lpd_trig_cnt) >= DECON_ENTER_LPD_CNT));
}

static inline bool decon_min_lock_cond(struct decon_device *decon)
{
	return (atomic_read(&decon->lpd_block_cnt) <= 0);
}

static inline u32 win_start_pos(int x, int y)
{
	return (WIN_STRPTR_Y_F(y) | WIN_STRPTR_X_F(x));
}

static inline u32 win_end_pos(int x, int y,  u32 xres, u32 yres)
{
	return (WIN_ENDPTR_Y_F(y + yres - 1) | WIN_ENDPTR_X_F(x + xres - 1));
}

/* IOCTL commands */
#define S3CFB_WIN_POSITION		_IOW('F', 203, \
						struct decon_user_window)
#define S3CFB_WIN_SET_PLANE_ALPHA	_IOW('F', 204, \
						struct s3c_fb_user_plane_alpha)
#define S3CFB_WIN_SET_CHROMA		_IOW('F', 205, \
						struct s3c_fb_user_chroma)
#define S3CFB_SET_VSYNC_INT		_IOW('F', 206, __u32)

#define S3CFB_GET_ION_USER_HANDLE	_IOWR('F', 208, \
						struct s3c_fb_user_ion_client)
#define S3CFB_WIN_CONFIG		_IOW('F', 209, \
						struct decon_win_config_data)
#define S3CFB_WIN_PSR_EXIT		_IOW('F', 210, int)

#define DECON_IOC_LPD_EXIT_LOCK		_IOW('L', 0, u32)
#define DECON_IOC_LPD_UNLOCK		_IOW('L', 1, u32)

#ifdef CONFIG_EXYNOS_DECON_DUAL_DSI

typedef enum display_type {
	DECON_PRIMARY_DISPLAY = 0,
	DECON_SECONDARY_DISPLAY,
} display_type_t;

struct dual_blank_data {
	display_type_t type;
	int blank;
};

#define S3CFB_DUAL_DISPLAY_BLANK _IOW('F', 300, \
				struct dual_blank_data)

#define TE_STAT_UPDATE		0x80
#define TE_STAT_DSI0		0x01
#define TE_STAT_DSI1		0x02

#define DECON_STAT_UPDATE	0x80
#define DECON_STAT_OFF		0x00
#define DECON_STAT_ON		0x01

#define DSI_STAT_UPDATE		0x80
#define DSI_STAT_OFF		0x00
#define DSI_STAT_MASTER		0x01
#define DSI_STAT_SLAVE		0x02
#define DSI_STAT_ALONE		0x03

#define TE_SET_STAT(x) ((x & 0x000000ff) << 24)
#define TE_GET_STAT(x) ((x & 0x0f000000) >> 24)

#define DECON_SET_STAT(x) ((x & 0x000000ff) << 16)
#define DECON_GET_STAT(x) ((x & 0x000f0000) >> 16)

#define CHECK_DECON_UPDATE(x) (x & (DECON_STAT_UPDATE << 16))

#define DSI0_SET_STAT(x) ((x & 0x000000ff) << 8)
#define DSI0_GET_STAT(x) ((x & 0x00000f00) >> 8)

#define CHECK_DSI0_UPDATE(x) (x & (DSI_STAT_UPDATE << 8))
#define CHECK_DSI1_UPDATE(x) (x & (DSI_STAT_UPDATE))

#define DSI1_SET_STAT(x) (x & 0x000000ff)
#define DSI1_GET_STAT(x) (x & 0x0000000f)


#define DSI_STAT_NEEE_DISPLAY_ON 	0x40

#define CHECK_DSI0_NEED_DP_ON(x) (x & (DSI_STAT_NEEE_DISPLAY_ON << 8))
#define CHECK_DSI1_NEED_DP_ON(x) (x & DSI_STAT_NEEE_DISPLAY_ON)

#define DSI0_REQ_DP_ON(x) (x | (DSI_STAT_NEEE_DISPLAY_ON << 8))
#define DSI1_REQ_DP_ON(x) (x | DSI_STAT_NEEE_DISPLAY_ON)

#define DSI0_CLEAR_DP_ON(x) (x & ~(DSI_STAT_NEEE_DISPLAY_ON << 8))
#define DSI1_CLEAR_DP_ON(x) (x & ~(DSI_STAT_NEEE_DISPLAY_ON))
#endif

#endif /* ___SAMSUNG_DECON_H__ */
