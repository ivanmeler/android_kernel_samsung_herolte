/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SENSOR_INTERFACE_H
#define FIMC_IS_SENSOR_INTERFACE_H

#include "fimc-is-mem.h"
#include "fimc-is-config.h"
#include "exynos-fimc-is-sensor.h"
#include "fimc-is-metadata.h"
#include "fimc-is-regs.h"
#include "fimc-is-binary.h"

#define SENSOR_INTERFACE_MAGIC 0xFEDCBA98

#define NUM_OF_DUMMY_FRAME		(3) /* N + 2 Frame delay */
#define NUM_FRAMES_DMA			(NUM_OF_DUMMY_FRAME + 2 + 1)
#define NEXT_NEXT_FRAME_DMA		(NUM_OF_DUMMY_FRAME + 2)

/* This OFFSET is set to 0 when enable 3AA 3FRAME DELAY, set to 1 in other cases */
#define OTF_VSYNC_3AA_FRAME_DELAY_OFFSET	0

#define NUM_FRAMES_OTF			(NEXT_NEXT_FRAME_OTF + 1)
#define NEXT_NEXT_FRAME_OTF		(2 + OTF_VSYNC_3AA_FRAME_DELAY_OFFSET)

#define NEXT_FRAME			1
#define CURRENT_FRAME			0

#define NUM_FRAMES			(NUM_OF_DUMMY_FRAME + 2 + 1)

#define EXPOSURE_GAIN_INDEX		0
#define LONG_EXPOSURE_GAIN_INDEX	1
#define SHORT_EXPOSURE_GAIN_INDEX	2
#define MAX_EXPOSURE_GAIN_PER_FRAME	3

#define ACTUATOR_MAX_SOFT_LANDING_NUM	32 /* Actuator interface */
#define ACTUATOR_MAX_FOCUS_POSITIONS	1024

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define FPS_TO_DURATION_US(x)  ((x == 0) ? (0) : ((1000 * 1000) / x))
#define DURATION_US_TO_FPS(x)  ((x == 0) ? (0) : ((1000 * 1000) / x))

#define DEBUG_SENSOR_INTERFACE (0)
#if (DEBUG_SENSOR_INTERFACE == 1)
#define dbg_sen_itf(fmt, args...) \
	printk(KERN_DEBUG "[@][SEN_ITF] " fmt, ##args)
#else
#define dbg_sen_itf dbg_sensor
#endif

#define dbg_fls_itf(fmt, args...) \
	dbg_sen_itf(fmt, ##args)

enum DIFF_BET_SEN_ISP { /* Set to 0: 3AA 3frame delay, 1: 3AA 4frame delay, 3: M2M */
	DIFF_OTF_DELAY	= 0,
	DIFF_M2M_DELAY	= 3
};

/* DEVICE SENSOR INTERFACE */
#define SENSOR_REGISTER_FUNC_ADDR	(FIMC_IS_SDK_LIB_ADDR + 0x28)
typedef int (*register_sensor_interface)(void *itf);
static const register_sensor_interface register_sensor_itf = (register_sensor_interface)SENSOR_REGISTER_FUNC_ADDR;

struct ae_param {
	union {
		u32 val;
		u32 long_val;
	};
	u32 short_val;
};

typedef struct {
	/* Normal parameter */
	/* Each CB has a specific parameter type.(Ex. SetSize Cb has size info param.) */
	void *param;
	unsigned int return_value;
} cis_setting_info;

typedef enum _PdafParameter {
	PDAF_PARAMETERS_API_AFSTAT_NUMWINDOWS,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_0__TOPLEFT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_0__TOPLEFT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_0__BOTTOMRIGHT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_0__BOTTOMRIGHT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_1__TOPLEFT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_1__TOPLEFT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_1__BOTTOMRIGHT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_1__BOTTOMRIGHT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_2__TOPLEFT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_2__TOPLEFT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_2__BOTTOMRIGHT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_2__BOTTOMRIGHT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_3__TOPLEFT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_3__TOPLEFT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_3__BOTTOMRIGHT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_3__BOTTOMRIGHT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_4__TOPLEFT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_4__TOPLEFT_Y,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_4__BOTTOMRIGHT_X,
	PDAF_PARAMETERS_API_AFSTAT_WINDOWS_4__BOTTOMRIGHT_Y,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWSENABLE,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWS_TOPLEFT_X,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWS_TOPLEFT_Y,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWS_BOTTOMRIGHT_X,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWS_BOTTOMRIGHT_Y,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWSREAL_TOPLEFT_X,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWSREAL_TOPLEFT_Y,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWSREAL_BOTTOMRIGHT_X,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWSREAL_BOTTOMRIGHT_Y,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWSNUMX,
	PDAF_PARAMETERS_API_AFSTAT_MULTIWINDOWSNUMY,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_AF_FILTER_DELAY_CONTROL,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_AF_COMPENSATION,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_AF_SHADING_ON,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_AF_MULTI_FRAME_CONTROL,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_BITCUTNO,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_SATURATION_CONTROL,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_CORRELATION_CONTROL0,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_CORRELATION_CONTROL1,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__COEF_0_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__COEF_1_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__COEF_2_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__COEF_3_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__COEF_4_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__COEF_5_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__COMP_0_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__COMP_1_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__SHIFT,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_0__CSHIFT,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__COEF_0_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__COEF_1_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__COEF_2_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__COEF_3_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__COEF_4_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__COEF_5_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__COMP_0_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__COMP_1_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__SHIFT,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_1__CSHIFT,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__COEF_0_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__COEF_1_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__COEF_2_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__COEF_3_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__COEF_4_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__COEF_5_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__COMP_0_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__COMP_1_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__SHIFT,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_2__CSHIFT,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__COEF_0_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__COEF_1_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__COEF_2_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__COEF_3_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__COEF_4_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__COEF_5_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__COMP_0_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__COMP_1_,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__SHIFT,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTERS_3__CSHIFT,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTER1234_MUX,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTER1234_OUTMUX,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTER1234_TYPE,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTER12_CORING,
	PDAF_PARAMETERS_AFSTATTUNINGPARAMS_FILTER34_CORING,
	NUM_OF_PDAF_PARAMETERS
} pdaf_parameter;

typedef struct {
	unsigned long long exposure;
	unsigned int analog_gain;
	unsigned int digital_gain;
	unsigned long long long_exposure;
	unsigned int long_analog_gain;
	unsigned int long_digital_gain;
	unsigned long long short_exposure;
	unsigned int short_analog_gain;
	unsigned int short_digital_gain;
} ae_setting;

typedef struct {
	unsigned int long_exposure_coarse;
	unsigned int long_exposure_fine;
	unsigned int long_exposure_analog_gain;
	unsigned int long_exposure_digital_gain;
	unsigned int long_exposure_companion_digital_gain;
	unsigned int short_exposure_coarse;
	unsigned int short_exposure_fine;
	unsigned int short_exposure_analog_gain;
	unsigned int short_exposure_digital_gain;
	unsigned int short_exposure_companion_digital_gain;
} companion_ae_setting;


typedef struct {
/* #if(USING_CONCORD_SENSOR_EXTINFO == 1)//Jong 20131106 */
	bool companion_available;
/* #endif */

	bool stream_on;
	unsigned int comp_config_index_cur;
	unsigned int comp_config_Index_Pre;

	unsigned int Config_Idx;
	unsigned int Stat_Method_Addr;
	unsigned int Stat_Enable_Addr;
	unsigned short Stat_Enable_Val;

	bool bypass;
	bool stat_enable;
	bool enable_lsc;
	bool enable_wdr;
	bool enable_wdr_pre;
	bool enable_pdaf;
	bool enable_drc;
	bool enable_dynamic_bpc;
	bool enable_tango_bpc;
	bool enable_pdaf_bpc;
	bool enable_xtalk;

	bool change_config;

	unsigned int pre_pdaf_setting[NUM_OF_PDAF_PARAMETERS];
	unsigned int cur_pdaf_setting[NUM_OF_PDAF_PARAMETERS];
} companion_shared_data;

typedef struct {
	bool ois_available;
	unsigned int mode;
	unsigned int factory_step;
} ois_shared_data;

typedef struct {
	/** The length of a frame is specified as a number of lines, frame_length_lines.
	  @remarks
	  'frame length lines' is changed in dynamic AE mode.
	 */
	unsigned int frame_length_lines;
	unsigned int frame_time; // unit: ms

	/** The length of a line is specified as a number of pixel clocks, line_length_pck. */
	unsigned int line_length_pck;
	unsigned int line_readOut_time; // unit: ns
	unsigned long long rolling_shutter_skew;

	/** Video Timing Pixel Clock, vt_pix_clk_freq. */
	unsigned int pclk;
	unsigned int min_frame_us_time;

	/** Frame valid time */
	unsigned int frame_valid_us_time;

	unsigned int min_coarse_integration_time;
	unsigned int max_coarse_integration_time;
	unsigned int min_fine_integration_time;
	unsigned int max_fine_integration_time;
	unsigned int max_margin_coarse_integration_time;
	unsigned int max_margin_fine_integration_time;
	unsigned int min_analog_gain[2]; // 0: code, 1: times
	unsigned int max_analog_gain[2];
	unsigned int min_digital_gain[2];
	unsigned int max_digital_gain[2];

	unsigned int cur_frame_us_time;
	unsigned int cur_width;
	unsigned int cur_height;
	unsigned int pre_width;
	unsigned int pre_height;

	/** Current analogue_gain_code_global */
	unsigned int cur_exposure_coarse;
	unsigned int cur_exposure_fine;
	unsigned int cur_analog_gain;
	unsigned int cur_analog_gain_permille;
	unsigned int cur_digital_gain;
	unsigned int cur_digital_gain_permille;
	unsigned int cur_long_exposure_coarse;
	unsigned int cur_long_exposure_fine;
	unsigned int cur_long_analog_gain;
	unsigned int cur_long_analog_gain_permille;
	unsigned int cur_long_digital_gain;
	unsigned int cur_long_digital_gain_permille;
	unsigned int cur_short_exposure_coarse;
	unsigned int cur_short_exposure_fine;
	unsigned int cur_short_analog_gain;
	unsigned int cur_short_analog_gain_permille;
	unsigned int cur_short_digital_gain;
	unsigned int cur_short_digital_gain_permille;

	unsigned int stream_on;

	/* Moved from SensorEntry.cpp Jong 20121008 */
	unsigned int sen_vsync_count;

	unsigned int stream_id;
	unsigned int product_name; /* sensor names such as IMX134, IMX135, and S5K3L2 */
	unsigned int sens_config_index_cur;
	unsigned int sens_config_index_pre;
	unsigned int cur_frame_rate;
	unsigned int pre_frame_rate;
	bool is_active_area;

	/* bool bFirstRegSet; */
	unsigned int low_expo_start;

	/* To deal with N + 1 or N +2 setting timing, 0: Previous input, 1: Current input */
	unsigned int analog_gain[2];
	unsigned int digital_gain[2];

	unsigned int max_fps;

	unsigned int mipi_err_int_cnt[10];
	unsigned int mipi_err_print_out_cnt[10];

	bool called_common_sensor_setting; /* [hc0105.kim, 2013/09/13] Added to avoid calling common sensor register setting again. */
/* #ifdef C1_LSC_CHANGE // [ist.song 2014.08.19] Added to inform videomode to sensor. */
	bool video_mode;
/* #endif */
	unsigned int actuator_position;

	companion_shared_data companion_data;
	ois_shared_data ois_data;

	ae_setting auto_exposure[2];
	companion_ae_setting companion_auto_exposure[2];

/*	SysSema_t pFlashCtrl_IsrSema; //[2014.08.13, kh14.koo] to add for sema of KTD2692 (flash dirver) */

	bool binning; /* If binning is set, sensor should binning for size */

	u32 cis_rev;
	u32 group_param_hold;
} cis_shared_data;

struct v4l2_subdev;
typedef int (*cis_func_type)(struct v4l2_subdev *subdev, cis_setting_info *info);
struct fimc_is_cis_ops {
        int (*cis_init)(struct v4l2_subdev *subdev);
        int (*cis_log_status)(struct v4l2_subdev *subdev);
        int (*cis_group_param_hold)(struct v4l2_subdev *subdev, bool hold);
        int (*cis_mode_change)(struct v4l2_subdev *subdev, u32 mode);
        int (*cis_set_size)(struct v4l2_subdev *subdev, cis_shared_data *cis_data);
        int (*cis_stream_on)(struct v4l2_subdev *subdev);
        int (*cis_stream_off)(struct v4l2_subdev *subdev);
        int (*cis_adjust_frame_duration)(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration);
	/* Set dynamic frame duration value */
        int (*cis_set_frame_duration)(struct v4l2_subdev *subdev, u32 frame_duration);
	/* Set min fps value */
        int (*cis_set_frame_rate)(struct v4l2_subdev *subdev, u32 min_fps);
        int (*cis_get_min_exposure_time)(struct v4l2_subdev *subdev, u32 *min_expo);
        int (*cis_get_max_exposure_time)(struct v4l2_subdev *subdev, u32 *max_expo);
	cis_func_type cis_adjust_expoure_time; /* TBD */
        int (*cis_set_exposure_time)(struct v4l2_subdev *subdev, struct ae_param *target_exposure);
        int (*cis_get_min_analog_gain)(struct v4l2_subdev *subdev, u32 *min_again);
        int (*cis_get_max_analog_gain)(struct v4l2_subdev *subdev, u32 *max_again);
        int (*cis_adjust_analog_gain)(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile);
        int (*cis_set_analog_gain)(struct v4l2_subdev *subdev, struct ae_param *again);
        int (*cis_get_analog_gain)(struct v4l2_subdev *subdev, u32 *again);
        int (*cis_get_min_digital_gain)(struct v4l2_subdev *subdev, u32 *min_dgain);
        int (*cis_get_max_digital_gain)(struct v4l2_subdev *subdev, u32 *max_dgain);
	cis_func_type cis_adjust_digital_gain; /* TBD */
        int (*cis_set_digital_gain)(struct v4l2_subdev *subdev, struct ae_param *dgain);
        int (*cis_get_digital_gain)(struct v4l2_subdev *subdev, u32 *dgain);
	int (*cis_compensate_gain_for_extremely_br)(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain);
	cis_func_type cis_get_line_readout_time_ns; /* TBD */
	cis_func_type cis_read_sysreg; /* TBD */
	cis_func_type cis_read_userreg; /* TBD */
};

struct fimc_is_sensor_ctl
{
	//===================================================================================//
	// Ctl updtate timing
	// (01) ~ (04) are updated by "SensorEntry::CopyCam2P0Ctl" when "SIRC_ISP_CAMERA_EVENT_FRAME_START" interrupt is occured.

	/* (01) */  u32 ctl_frame_number;
	/* (02) */  camera2_sensor_ctl_t cur_cam20_sensor_ctrl;
	bool valid_sensor_ctrl;
	/* (03) */  camera2_flash_ctl_t cur_cam20_flash_ctrl;
	bool valid_flash_ctrl;
	/* (04) */  camera2_lens_ctl_t cur_cam20_lens_ctrl;
	bool is_valid_lens_ctrl;
	//===================================================================================//

	//===================================================================================//
	// UCtl update timing

	// Sensor
	// (05) ~ (07) are updated by "SensorEntry::ReportCam2P0SensorDone" when "SIRC_ISP_CAMERA_EVENT_FRAME_END" interrupt is occured.
	/* (05) */  u32 sensor_frame_number;
	/* (06) */  bool is_sensor_request;
	/* (07) */  camera2_sensor_uctl_t cur_cam20_sensor_udctrl;
	bool is_valid_sensor_udctrl;

	// Flash
	// (08) ~ (10) are updated by "SensorEntry::ReportCam2P0FlashDone" when "SircSenHal_RequestFlash" function is called.
	/* (08) */  u32 flash_frame_number;
	/* (09) */  bool is_flash_request;
	/* (10) */  camera2_flash_uctl_t cur_cam20_flash_udctrl;
	bool valid_flash_udctrl;

	// Lens
	// (11) ~ (13) are updated by "SensorEntry::ReportCam2P0LensDone" when "SircSenHal_SetActuatorPosition" function is called
	// or "SIRC_ISP_CAMERA_EVENT_FRAME_END" interrupt is occured.
	/* (11) */  u32 lens_frame_number;
	/* (12) */  bool is_lens_request;
	/* (13) */  camera2_lens_uctl_t cur_cam20_lens_udctrl;
	bool is_valid_lens_udctrl;
	//===================================================================================//

	struct camera2_companion_uctl cur_cam20_companion_udctrl;
	bool is_valid_companion_udctrl;

	bool alg_reset_flag;

	// Frame number that indicating shot. Currntly, it is not used.
	/* (14) */  bool shot_frame_number;
};

typedef enum fimc_is_sensor_adjust_direction_ {
	SENSOR_ADJUST_TO_SHORT	= 1,
	SENSOR_ADJUST_TO_LONG	= 2,
} fimc_is_sensor_adjust_direction;

enum itf_cis_interface {
	ITF_CIS_SMIA = 0,
	ITF_CIS_SMIA_WDR,
	ITF_CIS_MAX,
};

enum itf_param_type {
	ITF_CIS_PARAM_TOTAL_GAIN,
	ITF_CIS_PARAM_ANALOG_GAIN,
	ITF_CIS_PARAM_DIGITAL_GAIN,
	ITF_CIS_PARAM_EXPOSURE,
	ITF_CIS_PARAM_FLASH_INTENSITY,
	ITF_CIS_PARAM_MAX,
};

/* This peri state is check available each sensor device. */
enum fimc_is_sensor_peri_state {
	FIMC_IS_SENSOR_ACTUATOR_AVAILABLE,
	FIMC_IS_SENSOR_FLASH_AVAILABLE,
	FIMC_IS_SENSOR_COMPANION_AVAILABLE,
	FIMC_IS_SENSOR_OIS_AVAILABLE
};

enum fimc_is_actuator_pos_size_bit {
	ACTUATOR_POS_SIZE_8BIT = 8,
	ACTUATOR_POS_SIZE_9BIT = 9,
	ACTUATOR_POS_SIZE_10BIT = 10,
};

enum fimc_is_actuator_direction {
	ACTUATOR_RANGE_INF_TO_MAC = 0,
	ACTUATOR_RANGE_MAC_TO_INF,
};

enum fimc_is_actuator_status {
	ACTUATOR_STATUS_NO_BUSY = 0,
	ACTUATOR_STATUS_BUSY
};

typedef int (*actuator_func_type)(struct v4l2_subdev *subdev, u32 *info);
struct fimc_is_actuator_ops {
	actuator_func_type actuator_init;
	actuator_func_type actuator_get_status;
	actuator_func_type actuator_set_pos;
	actuator_func_type actuator_cal_data;
};

/* Flash */
struct fimc_is_flash_ops {
	int (*flash_control)(struct v4l2_subdev *subdev, enum flash_mode mode, u32 intensity);
};

struct fimc_is_sensor_interface;

/* new APIs */
struct fimc_is_cis_interface_ops {
	int (*get_)(struct fimc_is_sensor_interface *itf);

	int (*request_reset_interface)(struct fimc_is_sensor_interface *itf,
					u32 exposure,
					u32 total_gain,
					u32 analog_gain,
					u32 digital_gain);

	int (*get_calibrated_size)(struct fimc_is_sensor_interface *itf,
					u32 *width,
					u32 *height);

	int (*get_bayer_order)(struct fimc_is_sensor_interface *itf,
					u32 *bayer_order);

	u32 (*get_min_exposure_time)(struct fimc_is_sensor_interface *itf);
	u32 (*get_max_exposure_time)(struct fimc_is_sensor_interface *itf);
	u32 (*get_min_analog_gain)(struct fimc_is_sensor_interface *itf);
	u32 (*get_max_analog_gain)(struct fimc_is_sensor_interface *itf);
	u32 (*get_min_digital_gain)(struct fimc_is_sensor_interface *itf);
	u32 (*get_max_digital_gain)(struct fimc_is_sensor_interface *itf);

	u32 (*get_vsync_count)(struct fimc_is_sensor_interface *itf);
	u32 (*get_vblank_count)(struct fimc_is_sensor_interface *itf);
	bool (*is_vvalid_period)(struct fimc_is_sensor_interface *itf);

	int (*request_exposure)(struct fimc_is_sensor_interface *itf,
				u32 long_exposure,
				u32 short_exposure);

	int (*adjust_exposure)(struct fimc_is_sensor_interface *itf,
				u32 long_exposure,
				u32 short_exposure,
				u32 *available_long_exposure,
				u32 *available_short_exposure,
				fimc_is_sensor_adjust_direction adjust_direction);

	int (*get_next_frame_timing)(struct fimc_is_sensor_interface *itf,
					u32 *long_exposure,
					u32 *short_exposure,
					u32 *frame_period,
					u64 *line_period);

	int (*get_frame_timing)(struct fimc_is_sensor_interface *itf,
					u32 *long_exposure,
					u32 *short_exposure,
					u32 *frame_period,
					u64 *line_period);

	int (*request_analog_gain)(struct fimc_is_sensor_interface *itf,
					u32 long_analog_gain,
					u32 short_analog_gain);

	int (*request_gain)(struct fimc_is_sensor_interface *itf,
				u32 long_total_gain,
				u32 long_analog_gain,
				u32 long_digital_gain,
				u32 short_total_gain,
				u32 short_analog_gain,
				u32 short_digital_gain);

	int (*adjust_analog_gain)(struct fimc_is_sensor_interface *itf,
					u32 desired_long_analog_gain,
					u32 desired_short_analog_gain,
					u32 *actual_long_gain,
					u32 *actual_short_gain,
					fimc_is_sensor_adjust_direction adjust_direction);

	int (*get_next_analog_gain)(struct fimc_is_sensor_interface *itf,
				u32 *long_analog_gain,
				u32 *short_analog_gain);

	int (*get_analog_gain)(struct fimc_is_sensor_interface *itf,
				u32 *long_analog_gain,
				u32 *short_analog_gain);

	int (*get_next_digital_gain)(struct fimc_is_sensor_interface *itf,
				u32 *long_digital_gain,
				u32 *short_digital_gain);

	int (*get_digital_gain)(struct fimc_is_sensor_interface *itf,
				u32 *long_digital_gain,
				u32 *short_digital_gain);

	bool (*is_actuator_available)(struct fimc_is_sensor_interface *itf);
	bool (*is_flash_available)(struct fimc_is_sensor_interface *itf);
	bool (*is_companion_available)(struct fimc_is_sensor_interface *itf);
	bool (*is_ois_available)(struct fimc_is_sensor_interface *itf);

	int (*get_sensor_frame_timing)(struct fimc_is_sensor_interface *itf,
				u32 *pclk,
				u32 *line_length_pck,
				u32 *frame_length_lines);

	int (*get_sensor_cur_size)(struct fimc_is_sensor_interface *itf,
				u32 *cur_width,
				u32 *cur_height);

	int (*get_sensor_max_fps)(struct fimc_is_sensor_interface *itf,
				u32 *max_fps);

	int (*get_sensor_cur_fps)(struct fimc_is_sensor_interface *itf,
				u32 *cur_fps);

	int (*get_hdr_ratio_ctl_by_again)(struct fimc_is_sensor_interface *itf,
				u32 *ctrl_by_again);

	int (*get_sensor_use_dgain)(struct fimc_is_sensor_interface *itf,
				u32 *use_dgain);

	int (*set_alg_reset_flag)(struct fimc_is_sensor_interface *itf,
				bool executed);

	int (*get_sensor_fnum)(struct fimc_is_sensor_interface *itf,
				u32 *fnum);

	int (*set_initial_exposure_of_setfile)(struct fimc_is_sensor_interface *itf,
				u32 expo);

	int (*set_video_mode_of_setfile)(struct fimc_is_sensor_interface *itf,
				bool video_mode);

	int (*get_num_of_frame_per_one_3aa)(struct fimc_is_sensor_interface *itf,
				u32 *num_of_frame);

	int (*get_offset_from_cur_result)(struct fimc_is_sensor_interface *itf,
				u32 *offset);

	int (*set_cur_uctl_list)(struct fimc_is_sensor_interface *itf);

	int (*apply_sensor_setting)(struct fimc_is_sensor_interface *itf);

	/* reset exposure and gain for Flash */
	int (*request_reset_expo_gain)(struct fimc_is_sensor_interface *itf,
					u32 long_expo,
					u32 long_tgain,
					u32 long_again,
					u32 long_dgain,
					u32 short_expo,
					u32 short_tgain,
					u32 short_again,
					u32 short_dgain);
	int (*set_sensor_info_mode_change)(struct fimc_is_sensor_interface *itf,
					u32 expo,
					u32 again,
					u32 dgain);
	int (*update_sensor_dynamic_meta)(struct fimc_is_sensor_interface *itf,
					u32 frame_count,
					camera2_ctl_t *ctrl,
					camera2_dm_t *dm,
					camera2_udm_t *udm);
	int (*copy_sensor_ctl)(struct fimc_is_sensor_interface *itf,
					u32 frame_count,
					camera2_shot_t *shot);

	/* Get sensor module id */
	int (*get_module_id)(struct fimc_is_sensor_interface *itf,
					u32 *module_id);

	/* Get sensor module position */
	int (*get_module_position)(struct fimc_is_sensor_interface *itf,
					enum exynos_sensor_position *real_module);

	/* Set sensor 3a mode - OTF/M2M */
	int (*set_sensor_3a_mode)(struct fimc_is_sensor_interface *itf,
					u32 mode);
};

struct fimc_is_cis_event_ops {
	int (*start_of_frame)(struct fimc_is_sensor_interface *itf);
	int (*end_of_frame)(struct fimc_is_sensor_interface *itf);
	int (*apply_frame_settings)(struct fimc_is_sensor_interface *itf);
};

/* end of new APIs */

/* Actuator interface */
struct fimc_is_actuator_softlanding_table {
	bool enable;
	u32 step_delay;
	u32 position_num;
	u32 virtual_table[ACTUATOR_MAX_SOFT_LANDING_NUM];
	u32 hw_table[ACTUATOR_MAX_SOFT_LANDING_NUM];
};

struct fimc_is_actuator_position_table {
	bool enable;
	u32 hw_table[ACTUATOR_MAX_FOCUS_POSITIONS];
};

struct fimc_is_actuator_interface {
	/* ToDo: consider M2M scenario */
	u32 virtual_pos;
	u32 hw_pos;

	/*
	 * This values are specific information for AF Tick noise
	 * when turning on a camera.
	 */
	bool initialized;

	struct fimc_is_actuator_position_table position_table;
	struct fimc_is_actuator_softlanding_table soft_landing_table;
};

struct fimc_is_actuator_interface_ops {
	int (*set_actuator_position_table) (struct fimc_is_sensor_interface *itf,
					u32 *position_table);
	int (*set_soft_landing_config) (struct fimc_is_sensor_interface *itf,
					u32 step_delay,
					u32 position_num,
					u32 *position_table);
	int (*set_position) (struct fimc_is_sensor_interface *itf, u32 position);

	int (*get_cur_frame_position) (struct fimc_is_sensor_interface *itf, u32 *position);
	int (*get_applied_actual_position) (struct fimc_is_sensor_interface *itf, u32 *position);
	int (*get_prev_frame_position) (struct fimc_is_sensor_interface *itf,
					u32 *position, u32 frame_diff);

	int (*set_af_window_position) (struct fimc_is_sensor_interface *itf,
					u32 left_x, u32 left_y,
					u32 right_x, u32 right_y);

	int (*get_status) (struct fimc_is_sensor_interface *itf, u32 *status);
	int (*copy_lens_ctl)(struct fimc_is_sensor_interface *itf,
					u32 frame_count,
					camera2_shot_t *shot);
};

/* Flash interface */
struct fimc_is_flash_expo_gain {
	u32 expo[2];
	u32 tgain[2];
	u32 again[2];
	u32 dgain[2];
	u32 long_expo[2];
	u32 long_tgain[2];
	u32 long_again[2];
	u32 long_dgain[2];
	u32 short_expo[2];
	u32 short_tgain[2];
	u32 short_again[2];
	u32 short_dgain[2];
	bool pre_fls_ae_reset; /* true: Pre-flash off */
	u32 frm_num_pre_fls; /* If it is set to 100, then Pre-flash is automatically turned off at 100-th frame. */
	bool main_fls_ae_reset; /* true: Main-flash on/off */
	/* If they are set to 200 and 201, then Main-flash sensor settings are applied to a sensor, and the flash is fired at 200-th frame.
	   After the Main-flash, ambient exposure and gains are set to the sensor at 201-th frame. */
	u32 frm_num_main_fls[2];
	u32 main_fls_strm_on_off_step; /* 0: main/pre-flash exposure and gains, 1: ambient exposure and gains */
};

struct fimc_is_flash_interface_ops {
	int (*request_flash)(struct fimc_is_sensor_interface *itf,
				u32 mode,
				bool on,
				u32 intensity,
				u32 time);
	int (*request_flash_expo_gain)(struct fimc_is_sensor_interface *itf,
				struct fimc_is_flash_expo_gain *flash_ae);
	int (*update_flash_dynamic_meta)(struct fimc_is_sensor_interface *itf,
					u32 frame_count,
					camera2_ctl_t *ctrl,
					camera2_dm_t *dm,
					camera2_udm_t *udm);
	int (*copy_flash_ctl)(struct fimc_is_sensor_interface *itf,
					u32 frame_count,
					camera2_shot_t *shot);
};

struct fimc_is_sensor_interface {
	u32					magic;
	struct fimc_is_cis_interface_ops	cis_itf_ops;
	struct fimc_is_cis_event_ops		cis_evt_ops;
	struct fimc_is_actuator_interface	actuator_itf;
	struct fimc_is_actuator_interface_ops	actuator_itf_ops;
	struct fimc_is_flash_interface_ops	flash_itf_ops;

	bool			vsync_flag;
	bool			otf_flag_3aa;
	/* Different frame count between sensor and ISP */
	enum DIFF_BET_SEN_ISP	diff_bet_sen_isp;
	enum itf_cis_interface	cis_mode;

	u32			total_gain[MAX_EXPOSURE_GAIN_PER_FRAME][NUM_FRAMES];
	u32			analog_gain[MAX_EXPOSURE_GAIN_PER_FRAME][NUM_FRAMES];
	u32			digital_gain[MAX_EXPOSURE_GAIN_PER_FRAME][NUM_FRAMES];
	u32			exposure[MAX_EXPOSURE_GAIN_PER_FRAME][NUM_FRAMES];

	u32			flash_mode[NUM_FRAMES];
	u32			flash_intensity[NUM_FRAMES];
	u32			flash_firing_duration[NUM_FRAMES];
};

int init_sensor_interface(struct fimc_is_sensor_interface *itf);

/* Sensor interface helper function */
struct fimc_is_module_enum *get_subdev_module_enum(struct fimc_is_sensor_interface *itf);
struct fimc_is_device_csi *get_subdev_csi(struct fimc_is_sensor_interface *itf);
struct fimc_is_actuator *get_subdev_actuator(struct fimc_is_sensor_interface *itf);
u32 get_frame_count(struct fimc_is_sensor_interface *itf);

int sensor_get_ctrl(struct fimc_is_sensor_interface *itf, u32 ctrl_id, u32 *val);

/* Actuator interface function */
int set_actuator_position_table(struct fimc_is_sensor_interface *itf,
			u32 *position_table);
int set_soft_landing_config(struct fimc_is_sensor_interface *itf,
			u32 step_delay, u32 position_num, u32 *hw_pos_table);
int set_position(struct fimc_is_sensor_interface *itf, u32 position);
int get_cur_frame_position(struct fimc_is_sensor_interface *itf, u32 *position);
int get_applied_actual_position(struct fimc_is_sensor_interface *itf, u32 *position);
int get_prev_frame_position(struct fimc_is_sensor_interface *itf,
			u32 *position, u32 frame_diff);
int set_af_window_position(struct fimc_is_sensor_interface *itf,
					u32 left_x, u32 left_y,
					u32 right_x, u32 right_y);
int get_status(struct fimc_is_sensor_interface *itf, u32 *status);

#endif
