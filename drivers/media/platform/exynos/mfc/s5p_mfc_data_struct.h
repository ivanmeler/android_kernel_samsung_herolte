/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_data_struct.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This file contains definitions of enums and structs used by the codec
 * driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */

#ifndef __S5P_MFC_DATA_STRUCT_H
#define __S5P_MFC_DATA_STRUCT_H __FILE__

#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-core.h>

/*
 * CONFIG_MFC_USE_BUS_DEVFREQ might be defined in exynos-mfc.h.
 * So pm_qos.h should be checked after exynos-mfc.h
 */
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
#include <linux/pm_qos.h>
#endif

#include <linux/exynos_mfc_media.h>
#include "s5p_mfc_nal_q_struct.h"

#define MFC_NUM_CONTEXTS	32
#define MFC_MAX_PLANES		3
#define MFC_MAX_DPBS		32
#define MFC_MAX_BUFFERS		32
#define MFC_MAX_EXTRA_BUF	10
#define MFC_TIME_INDEX		15

/* Maximum number of temporal layers */
#define VIDEO_MAX_TEMPORAL_LAYERS 7

/*
 *  MFC region id for smc
 */
enum {
	FC_MFC_EXYNOS_ID_MFC_SH        = 0,
	FC_MFC_EXYNOS_ID_VIDEO         = 1,
	FC_MFC_EXYNOS_ID_MFC_FW        = 2,
	FC_MFC_EXYNOS_ID_SECTBL        = 3,
	FC_MFC_EXYNOS_ID_G2D_WFD       = 4,
	FC_MFC_EXYNOS_ID_MFC_NFW       = 5,
	FC_MFC_EXYNOS_ID_VIDEO_EXT     = 6,
};

/**
 * enum s5p_mfc_inst_type - The type of an MFC device node.
 */
enum s5p_mfc_node_type {
	MFCNODE_INVALID = -1,
	MFCNODE_DECODER = 0,
	MFCNODE_ENCODER = 1,
	MFCNODE_DECODER_DRM = 2,
	MFCNODE_ENCODER_DRM = 3,
};

/**
 * enum s5p_mfc_inst_type - The type of an MFC instance.
 */
enum s5p_mfc_inst_type {
	MFCINST_INVALID = 0,
	MFCINST_DECODER = 1,
	MFCINST_ENCODER = 2,
};

/**
 * enum s5p_mfc_inst_state - The state of an MFC instance.
 */
enum s5p_mfc_inst_state {
	MFCINST_FREE = 0,
	MFCINST_INIT = 100,
	MFCINST_GOT_INST,
	MFCINST_HEAD_PARSED,
	MFCINST_RUNNING_BUF_FULL,
	MFCINST_RUNNING,
	MFCINST_FINISHING,
	MFCINST_FINISHED,
	MFCINST_RETURN_INST,
	MFCINST_ERROR,
	MFCINST_ABORT,
	MFCINST_RES_CHANGE_INIT,
	MFCINST_RES_CHANGE_FLUSH,
	MFCINST_RES_CHANGE_END,
	MFCINST_RUNNING_NO_OUTPUT,
	MFCINST_ABORT_INST,
	MFCINST_DPB_FLUSHING,
	MFCINST_VPS_PARSED_ONLY,
	MFCINST_SPECIAL_PARSING,
	MFCINST_SPECIAL_PARSING_NAL,
};

/**
 * enum s5p_mfc_queue_state - The state of buffer queue.
 */
enum s5p_mfc_queue_state {
	QUEUE_FREE = 0,
	QUEUE_BUFS_REQUESTED,
	QUEUE_BUFS_QUERIED,
	QUEUE_BUFS_MMAPED,
};

enum mfc_dec_wait_state {
	WAIT_NONE = 0,
	WAIT_DECODING,
	WAIT_INITBUF_DONE,
};

/**
 * enum s5p_mfc_check_state - The state for user notification
 */
enum s5p_mfc_check_state {
	MFCSTATE_PROCESSING = 0,
	MFCSTATE_DEC_RES_DETECT,
	MFCSTATE_DEC_TERMINATING,
	MFCSTATE_ENC_NO_OUTPUT,
	MFCSTATE_DEC_S3D_REALLOC,
};

/**
 * enum s5p_mfc_buf_cacheable_mask - The mask for cacheble setting
 */
enum s5p_mfc_buf_cacheable_mask {
	MFCMASK_DST_CACHE = (1 << 0),
	MFCMASK_SRC_CACHE = (1 << 1),
};

enum mfc_buf_usage_type {
	MFCBUF_INVALID = 0,
	MFCBUF_NORMAL,
	MFCBUF_DRM,
};

enum mfc_buf_process_type {
	MFCBUFPROC_DEFAULT 		= 0x0,
	MFCBUFPROC_COPY 		= (1 << 0),
	MFCBUFPROC_SHARE 		= (1 << 1),
	MFCBUFPROC_META 		= (1 << 2),
	MFCBUFPROC_ANBSHARE		= (1 << 3),
	MFCBUFPROC_ANBSHARE_NV12L	= (1 << 4),
};

struct s5p_mfc_ctx;
struct s5p_mfc_extra_buf;

/**
 * struct s5p_mfc_buf - MFC buffer
 *
 */
struct s5p_mfc_buf {
	struct vb2_buffer vb;
	struct list_head list;
	union {
		dma_addr_t raw[3];
		dma_addr_t stream;
	} planes;
	int used;
	int already;
	int consumed;
	unsigned char *vir_addr;
};

struct s5p_mfc_pm {
	struct clk	*clock;
	atomic_t	power;
	struct device	*device;
	spinlock_t	clklock;

	int clock_on_steps;
	int clock_off_steps;
	enum mfc_buf_usage_type base_type;
};

struct s5p_mfc_fw {
	const struct firmware	*info;
	int			state;
	int			date;
};

struct s5p_mfc_buf_align {
	unsigned int mfc_base_align;
};

struct s5p_mfc_buf_size_v5 {
	unsigned int h264_ctx_buf;
	unsigned int non_h264_ctx_buf;
	unsigned int desc_buf;
	unsigned int shared_buf;
};

struct s5p_mfc_buf_size_v6 {
	unsigned int dev_ctx;
	unsigned int h264_dec_ctx;
	unsigned int other_dec_ctx;
	unsigned int h264_enc_ctx;
	unsigned int hevc_enc_ctx;
	unsigned int other_enc_ctx;
	unsigned int shared_buf;
};

struct s5p_mfc_buf_size {
	size_t firmware_code;
	unsigned int cpb_buf;
	void *buf;
};

struct s5p_mfc_variant {
	struct s5p_mfc_buf_size *buf_size;
	struct s5p_mfc_buf_align *buf_align;
	int	num_entities;
};

/**
 * struct s5p_mfc_extra_buf - represents internal used buffer
 * @alloc:		allocation-specific contexts for each buffer
 *			(videobuf2 allocator)
 * @ofs:		offset of each buffer, will be used for MFC
 * @virt:		kernel virtual address, only valid when the
 *			buffer accessed by driver
 * @dma:		DMA address, only valid when kernel DMA API used
 *
 * @phys:		physical address for exynos_smc
 */
struct s5p_mfc_extra_buf {
	void		*alloc;
	unsigned long	ofs;
	void		*virt;
	dma_addr_t	dma;
	phys_addr_t	phys;
};

/**
 * struct s5p_mfc_dev - The struct containing driver internal parameters.
 */
struct s5p_mfc_dev {
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd_dec;
	struct video_device	*vfd_enc;
	struct video_device	*vfd_dec_drm;
	struct video_device	*vfd_enc_drm;
	struct device		*device;
#ifdef CONFIG_ION_EXYNOS
	struct ion_client	*mfc_ion_client;
#endif

	void __iomem		*regs_base;
	int			irq;
	struct resource		*mfc_mem;

	struct s5p_mfc_pm	pm;
	struct s5p_mfc_fw	fw;
	struct s5p_mfc_variant	*variant;
	struct s5p_mfc_platdata	*pdata;

	int num_inst;
	spinlock_t irqlock;
	spinlock_t condlock;

	struct mutex mfc_mutex;

	int int_cond;
	int int_type;
	unsigned int int_err;
	wait_queue_head_t queue;

	unsigned long hw_lock;

	/*
	struct clk *clock1;
	struct clk *clock2;
	*/

	/* For 6.x, Added for SYS_INIT context buffer */
	struct s5p_mfc_extra_buf ctx_buf;
	struct s5p_mfc_extra_buf ctx_buf_drm;

	struct s5p_mfc_ctx *ctx[MFC_NUM_CONTEXTS];
	int curr_ctx;
	int preempt_ctx;
	int continue_ctx;
	unsigned long ctx_work_bits;
	unsigned long ctx_stop_bits;

	atomic_t watchdog_cnt;
	atomic_t watchdog_run;
	struct timer_list watchdog_timer;
	struct workqueue_struct *watchdog_wq;
	struct work_struct watchdog_work;

	struct vb2_alloc_ctx *alloc_ctx;

	unsigned long clk_state;

	/* for DRM */
	int curr_ctx_drm;
	int fw_status;
	int num_drm_inst;
	struct s5p_mfc_extra_buf fw_info;
	struct s5p_mfc_extra_buf drm_fw_info;
	struct vb2_alloc_ctx *alloc_ctx_fw;
	struct vb2_alloc_ctx *alloc_ctx_drm_fw;
	struct vb2_alloc_ctx *alloc_ctx_drm;

	struct workqueue_struct *sched_wq;
	struct work_struct sched_work;

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	struct list_head qos_queue;
	atomic_t qos_req_cur;
	atomic_t *qos_req_cnt;
	struct pm_qos_request qos_req_int;
	struct pm_qos_request qos_req_mif;
	struct pm_qos_request qos_req_cluster1;
	struct pm_qos_request qos_req_cluster0;

	/* for direct clock control */
	int min_rate;
#endif
	int id;
	atomic_t clk_ref;
	int fw_size;
	size_t fw_region_size;
	int drm_fw_status;
	int is_support_smc;
	int sys_init_status;
	int wakeup_status;

	atomic_t trace_ref;
	struct _mfc_trace *mfc_trace;

	int has_enc_ctx;

	bool has_job;
	int extra_qos;
	bool is_only_h264_enc;

	nal_queue_handle *nal_q_handle;
};

/**
 *
 */
struct s5p_mfc_h264_enc_params {
	enum v4l2_mpeg_video_h264_profile profile;
	u8 level;
	u8 interlace;
	enum v4l2_mpeg_video_h264_loop_filter_mode loop_filter_mode;
	s8 loop_filter_alpha;
	s8 loop_filter_beta;
	enum v4l2_mpeg_video_h264_entropy_mode entropy_mode;
	u8 num_ref_pic_4p;
	u8 _8x8_transform;
	u32 rc_framerate;
	u8 rc_frame_qp;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_min_qp_p;
	u8 rc_max_qp_p;
	u8 rc_min_qp_b;
	u8 rc_max_qp_b;
	u8 rc_mb_dark;
	u8 rc_mb_smooth;
	u8 rc_mb_static;
	u8 rc_mb_activity;
	u8 rc_p_frame_qp;
	u8 rc_b_frame_qp;
	u8 ar_vui;
	enum v4l2_mpeg_video_h264_vui_sar_idc ar_vui_idc;
	u16 ext_sar_width;
	u16 ext_sar_height;
	u8 open_gop;
	u16 open_gop_size;
	u8 hier_qp_enable;
	enum v4l2_mpeg_video_h264_hierarchical_coding_type hier_qp_type;
	u8 num_hier_layer;
	u8 hier_ref_type;
	u8 hier_qp_layer[7];
	u32 hier_bit_layer[7];
	u8 sei_gen_enable;
	u8 sei_fp_curr_frame_0;
	enum v4l2_mpeg_video_h264_sei_fp_arrangement_type sei_fp_arrangement_type;
	u32 fmo_enable;
	u32 fmo_slice_map_type;
	u32 fmo_slice_num_grp;
	u32 fmo_run_length[4];
	u32 fmo_sg_dir;
	u32 fmo_sg_rate;
	u32 aso_enable;
	u32 aso_slice_order[8];

	u32 prepend_sps_pps_to_idr;
	u8 enable_ltr;
	u8 num_of_ltr;
	u32 set_priority;
	u32 base_priority;
	u32 vui_enable;
};

/**
 *
 */
struct s5p_mfc_mpeg4_enc_params {
	/* MPEG4 Only */
	enum v4l2_mpeg_video_mpeg4_profile profile;
	u8 level;
	u8 quarter_pixel; /* MFC5.x */
	u16 vop_time_res;
	u16 vop_frm_delta;
	u8 rc_b_frame_qp;
	/* Common for MPEG4, H263 */
	u32 rc_framerate;
	u8 rc_frame_qp;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_min_qp_p;
	u8 rc_max_qp_p;
	u8 rc_min_qp_b;
	u8 rc_max_qp_b;
	u8 rc_p_frame_qp;
};

/**
 *
 */
struct s5p_mfc_vp9_enc_params {
	/* VP9 Only */
	u32 rc_framerate;
	u8 vp9_version;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_min_qp_p;
	u8 rc_max_qp_p;
	u8 rc_frame_qp;
	u8 rc_p_frame_qp;
	u8 vp9_goldenframesel;
	u16 vp9_gfrefreshperiod;
	u8 hier_qp_enable;
	u8 hier_qp_layer[3];
	u32 hier_bit_layer[3];
	u8 num_refs_for_p;
	u8 num_hier_layer;
	u8 max_partition_depth;
	u8 intra_pu_split_disable;
	u8 ivf_header;
};

/**
 *
 */
struct s5p_mfc_vp8_enc_params {
	/* VP8 Only */
	u32 rc_framerate;
	u8 vp8_version;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_min_qp_p;
	u8 rc_max_qp_p;
	u8 rc_frame_qp;
	u8 rc_p_frame_qp;
	u8 vp8_numberofpartitions;
	u8 vp8_filterlevel;
	u8 vp8_filtersharpness;
	u8 vp8_goldenframesel;
	u16 vp8_gfrefreshperiod;
	u8 hier_qp_enable;
	u8 hier_qp_layer[3];
	u32 hier_bit_layer[3];
	u8 num_refs_for_p;
	u8 intra_4x4mode_disable;
	u8 num_hier_layer;
};

/**
 *
 */
struct s5p_mfc_hevc_enc_params {
	u8 level;
	u8 tier_flag;
	/* HEVC Only */
	u32 rc_framerate;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_min_qp_p;
	u8 rc_max_qp_p;
	u8 rc_min_qp_b;
	u8 rc_max_qp_b;
	u8 rc_lcu_dark;
	u8 rc_lcu_smooth;
	u8 rc_lcu_static;
	u8 rc_lcu_activity;
	u8 rc_frame_qp;
	u8 rc_p_frame_qp;
	u8 rc_b_frame_qp;
	u8 max_partition_depth;
	u8 num_refs_for_p;
	u8 refreshtype;
	u16 refreshperiod;
	s32 lf_beta_offset_div2;
	s32 lf_tc_offset_div2;
	u8 loopfilter_disable;
	u8 loopfilter_across;
	u8 nal_control_length_filed;
	u8 nal_control_user_ref;
	u8 nal_control_store_ref;
	u8 const_intra_period_enable;
	u8 lossless_cu_enable;
	u8 wavefront_enable;
	u8 enable_ltr;
	u8 hier_qp_enable;
	enum v4l2_mpeg_video_hevc_hierarchical_coding_type hier_qp_type;
	u8 hier_ref_type;
	u8 num_hier_layer;
	u8 hier_qp_layer[7];
	u32 hier_bit_layer[7];
	u8 sign_data_hiding;
	u8 general_pb_enable;
	u8 temporal_id_enable;
	u8 strong_intra_smooth;
	u8 intra_pu_split_disable;
	u8 tmv_prediction_disable;
	u8 max_num_merge_mv;
	u8 eco_mode_enable;
	u8 encoding_nostartcode_enable;
	u8 size_of_length_field;
	u8 user_ref;
	u8 store_ref;
	u8 prepend_sps_pps_to_idr;
	u8 roi_enable;
};

/**
 *
 */
struct s5p_mfc_enc_params {
	u16 width;
	u16 height;

	u32 gop_size;
	enum v4l2_mpeg_video_multi_slice_mode slice_mode;
	u32 slice_mb;
	u32 slice_bit;
	u32 slice_mb_row;
	u32 intra_refresh_mb;
	u8 pad;
	u8 pad_luma;
	u8 pad_cb;
	u8 pad_cr;
	u8 rc_frame;
	u32 rc_bitrate;
	u16 rc_reaction_coeff;
	u32 config_qp;
	u32 dynamic_qp;
	u8 frame_tag;

	u8 num_b_frame;		/* H.264/MPEG4 */
	u8 rc_mb;		/* H.264: MFCv5, MPEG4/H.263: MFCv6 */
	u16 vbv_buf_size;
	enum v4l2_mpeg_video_header_mode seq_hdr_mode;
	enum v4l2_mpeg_mfc51_video_frame_skip_mode frame_skip_mode;
	u8 fixed_target_bit;
	u8 num_hier_max_layer;

	u16 rc_frame_delta;	/* MFC6.1 Only */

	union {
		struct s5p_mfc_h264_enc_params h264;
		struct s5p_mfc_mpeg4_enc_params mpeg4;
		struct s5p_mfc_vp8_enc_params vp8;
		struct s5p_mfc_vp9_enc_params vp9;
		struct s5p_mfc_hevc_enc_params hevc;
	} codec;
};

enum s5p_mfc_ctrl_type {
	MFC_CTRL_TYPE_GET_SRC	= 0x1,
	MFC_CTRL_TYPE_GET_DST	= 0x2,
	MFC_CTRL_TYPE_SET	= 0x4,
};

enum s5p_mfc_ctrl_mode {
	MFC_CTRL_MODE_NONE	= 0x0,
	MFC_CTRL_MODE_SFR	= 0x1,
	MFC_CTRL_MODE_CST	= 0x4,
};

struct s5p_mfc_buf_ctrl;

struct s5p_mfc_ctrl_cfg {
	enum s5p_mfc_ctrl_type type;
	unsigned int id;
	unsigned int is_volatile;	/* only for MFC_CTRL_TYPE_SET */
	unsigned int mode;
	unsigned int addr;
	unsigned int mask;
	unsigned int shft;
	unsigned int flag_mode;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int flag_addr;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int flag_shft;		/* only for MFC_CTRL_TYPE_SET */
	int (*read_cst) (struct s5p_mfc_ctx *ctx,
			struct s5p_mfc_buf_ctrl *buf_ctrl);
	void (*write_cst) (struct s5p_mfc_ctx *ctx,
			struct s5p_mfc_buf_ctrl *buf_ctrl);
};

struct s5p_mfc_ctx_ctrl {
	struct list_head list;
	enum s5p_mfc_ctrl_type type;
	unsigned int id;
	unsigned int addr;
	int has_new;
	int val;
};

struct s5p_mfc_buf_ctrl {
	struct list_head list;
	unsigned int id;
	enum s5p_mfc_ctrl_type type;
	int has_new;
	int val;
	unsigned int old_val;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int old_val2;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int is_volatile;	/* only for MFC_CTRL_TYPE_SET */
	unsigned int updated;
	unsigned int mode;
	unsigned int addr;
	unsigned int mask;
	unsigned int shft;
	unsigned int flag_mode;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int flag_addr;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int flag_shft;		/* only for MFC_CTRL_TYPE_SET */
	int (*read_cst) (struct s5p_mfc_ctx *ctx,
			struct s5p_mfc_buf_ctrl *buf_ctrl);
	void (*write_cst) (struct s5p_mfc_ctx *ctx,
			struct s5p_mfc_buf_ctrl *buf_ctrl);
};

struct s5p_mfc_codec_ops {
	/* initialization routines */
	int (*alloc_ctx_buf) (struct s5p_mfc_ctx *ctx);
	int (*alloc_desc_buf) (struct s5p_mfc_ctx *ctx);
	int (*get_init_arg) (struct s5p_mfc_ctx *ctx, void *arg);
	int (*pre_seq_start) (struct s5p_mfc_ctx *ctx);
	int (*post_seq_start) (struct s5p_mfc_ctx *ctx);
	int (*set_init_arg) (struct s5p_mfc_ctx *ctx, void *arg);
	int (*set_codec_bufs) (struct s5p_mfc_ctx *ctx);
	int (*set_dpbs) (struct s5p_mfc_ctx *ctx);		/* decoder */
	/* execution routines */
	int (*get_exe_arg) (struct s5p_mfc_ctx *ctx, void *arg);
	int (*pre_frame_start) (struct s5p_mfc_ctx *ctx);
	int (*post_frame_start) (struct s5p_mfc_ctx *ctx);
	int (*multi_data_frame) (struct s5p_mfc_ctx *ctx);
	int (*set_exe_arg) (struct s5p_mfc_ctx *ctx, void *arg);
	/* configuration routines */
	int (*get_codec_cfg) (struct s5p_mfc_ctx *ctx, unsigned int type,
			int *value);
	int (*set_codec_cfg) (struct s5p_mfc_ctx *ctx, unsigned int type,
			int *value);
	/* controls per buffer */
	int (*init_ctx_ctrls) (struct s5p_mfc_ctx *ctx);
	int (*cleanup_ctx_ctrls) (struct s5p_mfc_ctx *ctx);
	int (*init_buf_ctrls) (struct s5p_mfc_ctx *ctx,
			enum s5p_mfc_ctrl_type type, unsigned int index);
	int (*cleanup_buf_ctrls) (struct s5p_mfc_ctx *ctx,
			enum s5p_mfc_ctrl_type type, unsigned int index);
	int (*to_buf_ctrls) (struct s5p_mfc_ctx *ctx, struct list_head *head);
	int (*to_ctx_ctrls) (struct s5p_mfc_ctx *ctx, struct list_head *head);
	int (*set_buf_ctrls_val) (struct s5p_mfc_ctx *ctx,
			struct list_head *head);
	int (*get_buf_ctrls_val) (struct s5p_mfc_ctx *ctx,
			struct list_head *head);
	int (*recover_buf_ctrls_val) (struct s5p_mfc_ctx *ctx,
			struct list_head *head);
	int (*get_buf_update_val) (struct s5p_mfc_ctx *ctx,
			struct list_head *head, unsigned int id, int value);
	int (*set_buf_ctrls_val_nal_q) (struct s5p_mfc_ctx *ctx,
			struct list_head *head, EncoderInputStr *pInStr);
	int (*get_buf_ctrls_val_nal_q) (struct s5p_mfc_ctx *ctx,
			struct list_head *head, EncoderOutputStr *pOutStr);
};

struct stored_dpb_info {
	int fd[MFC_MAX_PLANES];
};

struct dec_dpb_ref_info {
	int index;
	struct stored_dpb_info dpb[MFC_MAX_DPBS];
};

struct temporal_layer_info {
	unsigned int temporal_layer_count;
	unsigned int temporal_layer_bitrate[VIDEO_MAX_TEMPORAL_LAYERS];
};

struct mfc_enc_roi_info {
	char *addr;
	int size;
	int upper_qp;
	int lower_qp;
	bool enable;
};

struct mfc_user_shared_handle {
	int fd;
	struct ion_handle *ion_handle;
	void *virt;
};

struct s5p_mfc_raw_info {
	int num_planes;
	int stride[3];
	int plane_size[3];
	int stride_2bits[3];
	int plane_size_2bits[3];
	unsigned int total_plane_size;
};

struct mfc_timestamp {
	struct list_head list;
	struct timeval timestamp;
	int index;
	int interval;
};

struct s5p_mfc_dec {
	int total_dpb_count;

	struct list_head dpb_queue;
	unsigned int dpb_queue_cnt;

	size_t src_buf_size;

	int loop_filter_mpeg4;
	int display_delay;
	int immediate_display;
	int is_packedpb;
	int slice_enable;
	int mv_count;
	int idr_decoding;
	int is_interlaced;
	int is_dts_mode;

	int crc_enable;
	int crc_luma0;
	int crc_chroma0;
	int crc_luma1;
	int crc_chroma1;

	struct s5p_mfc_extra_buf dsc;
	unsigned long consumed;
	unsigned long remained_size;
	unsigned long dpb_status;
	unsigned int dpb_flush;

	enum v4l2_memory dst_memtype;
	int sei_parse;
	int stored_tag;
	dma_addr_t y_addr_for_pb;

	int internal_dpb;
	int cr_left, cr_right, cr_top, cr_bot;

	/* For 6.x */
	int remained;

	/* For 7.x */
	int tiled_buf_cnt;
	struct s5p_mfc_raw_info tiled_ref;

	/* For dynamic DPB */
	int is_dynamic_dpb;
	unsigned int dynamic_set;
	unsigned int dynamic_used;
	struct list_head ref_queue;
	unsigned int ref_queue_cnt;
	struct dec_dpb_ref_info *ref_info;
	int assigned_fd[MFC_MAX_DPBS];
	struct mfc_user_shared_handle sh_handle;
	struct s5p_mfc_buf *assigned_dpb[MFC_MAX_DPBS];

	int dynamic_ref_filled;

	int profile;
	int is_10bit;

	unsigned int err_reuse_flag;
};

struct s5p_mfc_enc {
	struct s5p_mfc_enc_params params;

	size_t dst_buf_size;

	int frame_count;
	enum v4l2_mpeg_mfc51_video_frame_type frame_type;
	enum v4l2_mpeg_mfc51_video_force_frame_type force_frame_type;

	struct list_head ref_queue;
	unsigned int ref_queue_cnt;

	/* For 6.x */
	size_t luma_dpb_size;
	size_t chroma_dpb_size;
	size_t me_buffer_size;
	size_t tmv_buffer_size;

	unsigned int slice_mode;
	union {
		unsigned int mb;
		unsigned int bits;
	} slice_size;
	unsigned int in_slice;
	unsigned int buf_full;

	int stored_tag;
	struct mfc_user_shared_handle sh_handle_svc;
	struct mfc_user_shared_handle sh_handle_roi;
	int roi_index;
	struct s5p_mfc_extra_buf roi_buf[MFC_MAX_EXTRA_BUF];
	struct mfc_enc_roi_info roi_info[MFC_MAX_EXTRA_BUF];
};

struct s5p_mfc_fmt {
	char *name;
	u32 fourcc;
	u32 codec_mode;
	u32 type;
	u32 num_planes;
	u32 mem_planes;
};

/**
 * struct s5p_mfc_ctx - This struct contains the instance context
 */
struct s5p_mfc_ctx {
	struct s5p_mfc_dev *dev;
	struct v4l2_fh fh;
	int num;

	int int_cond;
	int int_type;
	unsigned int int_err;
	wait_queue_head_t queue;

	struct s5p_mfc_fmt *src_fmt;
	struct s5p_mfc_fmt *dst_fmt;

	struct vb2_queue vq_src;
	struct vb2_queue vq_dst;

	struct list_head src_queue;
	struct list_head dst_queue;

	struct list_head src_queue_nal_q;
	struct list_head dst_queue_nal_q;

	unsigned int src_queue_cnt;
	unsigned int dst_queue_cnt;

	unsigned int src_queue_cnt_nal_q;
	unsigned int dst_queue_cnt_nal_q;

	enum s5p_mfc_inst_type type;
	enum s5p_mfc_inst_state state;
	int inst_no;

	int img_width;
	int img_height;
	int buf_width;
	int buf_height;
	int dpb_count;
	int buf_stride;

	int old_img_width;
	int old_img_height;

	unsigned int enc_drc_flag;
	int enc_res_change;
	int enc_res_change_state;
	int enc_res_change_re_input;
	size_t min_scratch_buf_size;

	struct s5p_mfc_raw_info raw_buf;
	int mv_size;

	/* Buffers */
	void *codec_buf;
	size_t codec_buf_phys;
	size_t codec_buf_size;
	void *codec_buf_virt;

	enum s5p_mfc_queue_state capture_state;
	enum s5p_mfc_queue_state output_state;

	struct list_head ctrls;

	struct list_head src_ctrls[MFC_MAX_BUFFERS];
	struct list_head dst_ctrls[MFC_MAX_BUFFERS];

	unsigned long src_ctrls_avail;
	unsigned long dst_ctrls_avail;

	unsigned int sequence;

	/* Control values */
	int codec_mode;
	__u32 pix_format;
	int cacheable;

	/* Extra Buffers */
	unsigned int ctx_buf_size;
	struct s5p_mfc_extra_buf ctx;

	struct s5p_mfc_dec *dec_priv;
	struct s5p_mfc_enc *enc_priv;

	struct s5p_mfc_codec_ops *c_ops;

	/* For 6.x */
	size_t scratch_buf_size;

	/* for DRM */
	int is_drm;

	int is_dpb_realloc;
	enum mfc_dec_wait_state wait_state;

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	int qos_req_step;
	struct list_head qos_list;
#endif
	int qos_ratio;
	int framerate;
	int last_framerate;
	int avg_framerate;
	int frame_count;
	struct timeval last_timestamp;

	int is_max_fps;
	int buf_process_type;

	struct mfc_timestamp ts_array[MFC_TIME_INDEX];
	struct list_head ts_list;
	int ts_count;
	int ts_is_full;

	unsigned long raw_protect_flag;
	unsigned long stream_protect_flag;
};

#endif /* __S5P_MFC_DATA_STRUCT_H */
