/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _SYNAPTICS_RMI4_H_
#define _SYNAPTICS_RMI4_H_

#define SYNAPTICS_RMI4_DRIVER_VERSION "DS5 1.0"
#include <linux/device.h>
#include <linux/i2c/synaptics_rmi.h>
#include <linux/cdev.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <linux/sec_debug.h>
#endif
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

/**************************************/
/* Define related with driver feature */
#define FACTORY_MODE
#define PROXIMITY_MODE
//#define EDGE_SWIPE
#define USE_SHUTDOWN_CB

#define CHECK_PR_NUMBER
#define REPORT_2D_W

#ifdef CONFIG_SEC_FACTORY
#define REPORT_2D_Z		/* only for CONFIG_SEC_FACTORY */
#endif
#ifdef REPORT_2D_Z
#define DSX_PRESSURE_MAX	255
#endif

#ifdef CONFIG_INPUT_BOOSTER
#define TSP_BOOSTER
#endif
#if defined(CONFIG_GLOVE_TOUCH)
#define GLOVE_MODE
#endif

//#define USE_STYLUS
#define USE_ACTIVE_REPORT_RATE

/* #define SKIP_UPDATE_FW_ON_PROBE */
/* #define REPORT_ORIENTATION */
/* #define USE_SENSOR_SLEEP */

/* "HAS_ALTERNATER_R_R" feature have to check logic for using it. */
/* #define HAS_ALTERNATER_R_R */

/* only for V model feature*/
//#define GESTURE_3FIN	// Disable 3 finger gesture scenario
#define USE_LPGW_MODE

/**************************************/
/**************************************/

/* Show TSP status information for debug */
#define PRINT_DEBUG_INFO
#ifdef PRINT_DEBUG_INFO
#define F51_HAND_EDGE_DATA_SIZE 9
#define F51_HAS_DELTA_INFO (1 << 3)
#define F51_HAS_HAND_EDGE (1 << 4)
#define F54_DATA10_ADDR 0x0108
#define F54_DATA14_ADDR 0X0109
#define F54_DATA16_ADDR 0x010a
#define F54_DATA17_ADDR 0x010bf
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);	\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#define SYNAPTICS_DEVICE_NAME	"SYNAPTICS"
#define DRIVER_NAME "synaptics_rmi4_i2c"

#define SYNAPTICS_HW_RESET_TIME	100
#define SYNAPTICS_REZERO_TIME 100
#define SYNAPTICS_POWER_MARGIN_TIME	150
#define SYNAPTICS_DEEPSLEEP_TIME	20

#define TSP_FACTEST_RESULT_PASS		2
#define TSP_FACTEST_RESULT_FAIL		1
#define TSP_FACTEST_RESULT_NONE		0

#define SYNAPTICS_MAX_FW_PATH	64

#define SYNAPTICS_DEFAULT_UMS_FW "/sdcard/synaptics.fw"

#define DEFAULT_DEVICE_NUM	1

/* Define for Firmware file image format */
#define FIRMWARE_IMG_HEADER_MAJOR_VERSION_OFFSET	(0x07)
#define NEW_IMG_MAJOR_VERSION	(0x10)


/* New firmware image format(PR number is loaded defaultly) for v7 */
#define PR_NUMBER_0TH_BYTE_BIN_OFFSET_V7 (0x84)

#define PDT_PROPS (0X00EF)
#define PDT_START (0x00E9)
#define PDT_END (0x000A)
#define PDT_ENTRY_SIZE (0x0006)
#define PAGES_TO_SERVICE (10)
#define PAGE_SELECT_LEN (2)

#define SYNAPTICS_RMI4_F01 (0x01)
#define SYNAPTICS_RMI4_F11 (0x11)
#define SYNAPTICS_RMI4_F12 (0x12)
#define SYNAPTICS_RMI4_F1A (0x1a)
#define SYNAPTICS_RMI4_F34 (0x34)
#define SYNAPTICS_RMI4_F51 (0x51)
#define SYNAPTICS_RMI4_F54 (0x54)
#define SYNAPTICS_RMI4_F55 (0x55)
#define SYNAPTICS_RMI4_F60 (0x60)
#define SYNAPTICS_RMI4_FDB (0xdb)

#define SYNAPTICS_RMI4_PRODUCT_INFO_SIZE 2
#define SYNAPTICS_RMI4_DATE_CODE_SIZE 3
#define SYNAPTICS_RMI4_PRODUCT_ID_SIZE 10
#define SYNAPTICS_RMI4_BUILD_ID_SIZE 3
#define SYNAPTICS_RMI4_PRODUCT_ID_LENGTH 10
#define SYNAPTICS_RMI4_PACKAGE_ID_SIZE 4

#define MAX_NUMBER_OF_BUTTONS 4
#define MAX_INTR_REGISTERS 4
#define F12_FINGERS_TO_SUPPORT 10
#define MAX_NUMBER_OF_FINGERS (F12_FINGERS_TO_SUPPORT)

#define MASK_16BIT 0xFFFF
#define MASK_8BIT 0xFF
#define MASK_7BIT 0x7F
#define MASK_6BIT 0x3F
#define MASK_5BIT 0x1F
#define MASK_4BIT 0x0F
#define MASK_3BIT 0x07
#define MASK_2BIT 0x03
#define MASK_1BIT 0x01

#define INVALID_X	65535
#define INVALID_Y	65535

/* Define for Object type and status(F12_2D_data(N)/0).
 * Each 3-bit finger status field represents the following:
 * 000 = finger not present
 * 001 = finger present and data accurate
 * 010 = stylus pen (passive pen)
 * 011 = palm touch
 * 100 = not used
 * 101 = hover
 * 110 = glove touch
 */
#define OBJECT_NOT_PRESENT		(0x00)
#define OBJECT_FINGER			(0x01)
#define OBJECT_PASSIVE_STYLUS	(0x02)
#define OBJECT_PALM				(0x03)
#define OBJECT_UNCLASSIFIED		(0x04)
#define OBJECT_HOVER			(0x05)
#define OBJECT_GLOVE			(0x06)

/* Define for object type report enable Mask(F12_2D_CTRL23) */
#define OBJ_TYPE_FINGER			(1 << 0)
#define OBJ_TYPE_PASSIVE_STYLUS	(1 << 1)
#define OBJ_TYPE_PALM			(1 << 2)
#define OBJ_TYPE_UNCLASSIFIED	(1 << 3)
#define OBJ_TYPE_HOVER			(1 << 4)
#define OBJ_TYPE_GLOVE			(1 << 5)
#define OBJ_TYPE_NARROW_SWIPE	(1 << 6)
#define OBJ_TYPE_HANDEDGE		(1 << 7)
#define OBJ_TYPE_DEFAULT		(0x85)
/*OBJ_TYPE_FINGER, OBJ_TYPE_UNCLASSIFIED, OBJ_TYPE_HANDEDGE*/

/* Define for Data report enable Mask(F12_2D_CTRL28) */
#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

/* Define for Feature enable(F12_2D_CTRL26)
 * bit[0] : represent enable or disable glove mode(high sensitivity mode)
 * bit[1] : represent enable or disable cover mode.
 *		(cover is on lcd, change sensitivity to prevent unintended touch)
 * bit[2] : represent enable or disable fast glove mode.
 *		(change glove mode entering condition to be faster)
 */
#define GLOVE_DETECTION_EN (1 << 0)
#define CLOSED_COVER_EN (1 << 1)
#define FAST_GLOVE_DECTION_EN (1 << 2)

#define CLEAR_COVER_MODE_EN	(CLOSED_COVER_EN | GLOVE_DETECTION_EN)
#define FLIP_COVER_MODE_EN	(CLOSED_COVER_EN)

#ifdef PROXIMITY_MODE
#define F51_FINGER_TIMEOUT 50 /* ms */
#define HOVER_Z_MAX (255)

#define F51_PROXIMITY_ENABLES_OFFSET (0)
/* Define for proximity enables(F51_CUSTOM_CTRL00) */
#define FINGER_HOVER_EN (1 << 0)
#define AIR_SWIPE_EN (1 << 1)
#define LARGE_OBJ_EN (1 << 2)
#define HOVER_PINCH_EN (1 << 3)
#define LARGE_OBJ_WAKEUP_GESTURE_EN (1 << 4)
/* Reserved 5 */
#define ENABLE_HANDGRIP_RECOG (1 << 6)
#define SLEEP_PROXIMITY (1 << 7)

#define F51_GENERAL_CONTROL_OFFSET (1)
/* Define for General Control(F51_CUSTOM_CTRL01) */
#define JIG_TEST_EN		(1 << 0)
#define JIG_COMMAND_EN	(1 << 1)
#define DEAD_ZONE_EN	(1 << 2)
#define EN_GHOST_FINGER_STATUS_REPORT	(1 << 3)
//#define RESERVED		(1 << 4)
#define EDGE_SWIPE_EN	(1 << 5)
//#define RESERVED		(1 << 6)
//#define RESERVED		(1 << 7)

#define F51_GENERAL_CONTROL_2_OFFSET (2)
/* Define for General Control(F51_CUSTOM_CTRL02) */
/* Reserved 0 ~ 7 */

/* Define for proximity Controls(F51_CUSTOM_QUERY04) */
//#define HAS_FINGER_HOVER (1 << 0)
//#define HAS_AIR_SWIPE (1 << 1)
//#define HAS_LARGE_OBJ (1 << 2)
//#define HAS_HOVER_PINCH (1 << 3)
#define HAS_EDGE_SWIPE (1 << 4)
//#define HAS_SINGLE_FINGER (1 << 5)
//#define HAS_GRIP_SUPPRESSION (1 << 6)
//#define HAS_PALM_REJECTION (1 << 7)

/* Define for proximity Controls 2(F51_CUSTOM_QUERY05) */
#define HAS_PROFILE_HANDEDNESS (1 << 0)
#define HAS_LOWG (1 << 1)
#define HAS_FACE_DETECTION (1 << 2)
#define HAS_SIDE_BUTTONS (1 << 3)
#define HAS_CAMERA_GRIP_DETECTION (1 << 4)
/* Reserved 5 ~ 7 */

/* Define for Detection flag 2(F51_CUSTOM_DATA06) */
#define HAS_HAND_EDGE_SWIPE_DATA (1 << 0)
#define SIDE_BUTTON_DETECTED (1 << 1)
/* Reserved 2 ~ 7 */

#define F51_DATA_RESERVED_SIZE	(1)
#define F51_DATA_1_SIZE (4)	/* FINGER_HOVER */
#define F51_DATA_2_SIZE (1)	/* HOVER_PINCH */
#define F51_DATA_3_SIZE (1)	/* AIR_SWIPE | LARGE_OBJ */
#define F51_DATA_4_SIZE (2)	/* SIDE_BUTTON */
#define F51_DATA_5_SIZE	(1)	/* CAMERA_GRIP_DETECTION */
#define F51_DATA_6_SIZE (2)	/* DETECTION_FLAG2 */

#ifdef EDGE_SWIPE
#define EDGE_SWIPE_WIDTH_MAX	255
#define EDGE_SWIPE_PALM_MAX		1

#define EDGE_SWIPE_WITDH_X_OFFSET	5
#define EDGE_SWIPE_AREA_OFFSET	7
#endif
#define F51_EDGE_SWIPE_DATA_SIZE 10

#ifdef GESTURE_3FIN
#define	GESTURE_RIGHT	1
#define	GESTURE_LEFT	2
#define	GESTURE_UP		3
#define	GESTURE_DOWN	4
#endif

#ifdef USE_LPGW_MODE
#define	GESTURE_SWIPE			0x07
#endif

#define	ORIENTATION_0	0
#define	ORIENTATION_90	1
#define	ORIENTATION_180	2
#define	ORIENTATION_270	3

#ifdef SIDE_TOUCH
#define MAX_SIDE_BUTTONS	8
#define NUM_OF_ACTIVE_SIDE_BUTTONS	6
#endif
#endif

#define SYN_I2C_RETRY_TIMES 3
#define MAX_F11_TOUCH_WIDTH 15

#define CHECK_STATUS_TIMEOUT_MS 200
#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12
#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

/* Define for Device Control(F01_RMI_CTRL00) */
#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_ON (1 << 2)
/* Reserved 3 ~ 4 */
#define CHARGER_CONNECTED (1 << 5)
#define REPORT_RATE (1 << 6)
#define CONFIGURED (1 << 7)

#define TSP_NEEDTO_REBOOT	(-ECONNREFUSED)
#define MAX_TSP_REBOOT		3

#define SYNAPTICS_BL_ID_0_OFFSET	0
#define SYNAPTICS_BL_ID_1_OFFSET	1
#define SYNAPTICS_BL_MINOR_REV_OFFSET	2
#define SYNAPTICS_BL_MAJOR_REV_OFFSET	3

#define SYNAPTICS_BOOTLOADER_ID_SIZE	4


#define SYNAPTICS_BL_PROPERTIES_V7				0
#define SYNAPTICS_BL_MINOR_REV_OFFSET_V7	1
#define SYNAPTICS_BL_MAJOR_REV_OFFSET_V7	2

#define SYNAPTICS_BOOTLOADER_REVISION	3


/* Below version is represent that it support guest thread functionality. */
#define BL_MAJOR_VER_OF_GUEST_THREAD	0x36	/* '6' */
#define BL_MINOR_VER_OF_GUEST_THREAD	0x34	/* '4' */

#define SYNAPTICS_ACCESS_READ	false
#define SYNAPTICS_ACCESS_WRITE	true

/* Below offsets are defined manually.
 * So please keep in your mind when use this. it can be changed based on
 * firmware version you can check it debug_address sysfs node
 * (sys/class/sec/tsp/cmd).
 * If it is possible to replace that getting address from IC,
 * I recommend the latter than former.
 */
#ifdef PROXIMITY_MODE
#define MANUAL_DEFINED_OFFSET_GRIP_EDGE_EXCLUSION_RX	(32)
#endif
#ifdef SIDE_TOUCH
#define MANUAL_DEFINED_OFFSET_SIDEKEY_THRESHOLD	(47)
#endif
#ifdef USE_STYLUS
#define MANUAL_DEFINED_OFFSET_FORCEFINGER_ON_EDGE	(61)
#endif
/* Enum for each product id */
enum synaptics_product_ids {
	SYNAPTICS_PRODUCT_ID_NONE = 0,
	SYNAPTICS_PRODUCT_ID_S5807,
	SYNAPTICS_PRODUCT_ID_S5806,
	SYNAPTICS_PRODUCT_ID_S5300,
	SYNAPTICS_PRODUCT_ID_MAX
};

/* Define for Revision of IC */
#define SYNAPTICS_IC_REVISION_AA	0xAA

/* Release event type for manual release.. */
#define RELEASE_TYPE_FINGER	(1 << 0)
#define RELEASE_TYPE_SIDEKEY	(1 << 1)

#define RELEASE_TYPE_ALL (RELEASE_TYPE_FINGER | RELEASE_TYPE_SIDEKEY)

#ifdef USE_ACTIVE_REPORT_RATE
#define	SYNAPTICS_RPT_RATE_30HZ_VAL	(0x50)
#define	SYNAPTICS_RPT_RATE_60HZ_VAL	(0x16)
#define	SYNAPTICS_RPT_RATE_90HZ_VAL	(0x04)

enum synaptics_report_rate {
	SYNAPTICS_RPT_RATE_START = 0,
	SYNAPTICS_RPT_RATE_90HZ = SYNAPTICS_RPT_RATE_START,
	SYNAPTICS_RPT_RATE_60HZ,
	SYNAPTICS_RPT_RATE_30HZ,
	SYNAPTICS_RPT_RATE_END
};
#endif

/* load Register map file */
#include "rmi_register_map.h"

struct synaptics_rmi4_f1a_handle {
	int button_bitmask_size;
	unsigned char max_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned char *button_map;
	struct synaptics_rmi4_f1a_query button_query;
	struct synaptics_rmi4_f1a_control button_control;
};

#ifdef PROXIMITY_MODE
#ifdef EDGE_SWIPE
struct synaptics_rmi4_edge_swipe {
	int sumsize;
	int palm;
	int wx;
	int wy;
};
#endif

struct synaptics_rmi4_f51_handle {
/* CTRL */
	unsigned char proximity_enables;		/* F51_CUSTOM_CTRL00 */
	unsigned short proximity_enables_addr;
	unsigned char general_control;			/* F51_CUSTOM_CTRL01 */
	unsigned short general_control_addr;
	unsigned char general_control_2;		/* F51_CUSTOM_CTRL02 */
	unsigned short general_control_2_addr;
#ifdef PROXIMITY_MODE
	unsigned short grip_edge_exclusion_rx_addr;
#endif
#ifdef SIDE_TOUCH
	unsigned short sidebutton_tapthreshold_addr;
#endif
#ifdef USE_STYLUS
	unsigned short forcefinger_onedge_addr;
#endif
/* QUERY */
	unsigned char proximity_controls;		/* F51_CUSTOM_QUERY04 */
	unsigned char proximity_controls_2;		/* F51_CUSTOM_QUERY05 */
/* DATA */
	unsigned short detection_flag_2_addr;	/* F51_CUSTOM_DATA06 */
	unsigned short edge_swipe_data_addr;	/* F51_CUSTOM_DATA07 */
#ifdef GESTURE_3FIN
	unsigned short gesture_3finger_data_addr;	/* F51_CUSTOM_DATA49~50 */
#endif
#ifdef EDGE_SWIPE
	struct synaptics_rmi4_edge_swipe edge_swipe_data;
#endif
	unsigned short side_button_data_addr;	/* F51_CUSTOM_DATA04 */
	bool finger_is_hover;	/* To print hover log */
};
#endif

/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT
 * @query_base_addr: base address for query registers
 * @cmd_base_addr: base address for command registers
 * @ctrl_base_addr: base address for control registers
 * @data_base_addr: base address for data registers
 * @intr_src_count: number of interrupt sources
 * @fn_number: function number
 */
struct synaptics_rmi4_fn_desc {
	unsigned char	query_base_addr;
	unsigned char	cmd_base_addr;
	unsigned char	ctrl_base_addr;
	unsigned char	data_base_addr;
	unsigned char	intr_src_count:3;
	unsigned char	reserved_1:2;
	unsigned char	fn_version:2;
	unsigned char	reserved_2:1;
	unsigned char	fn_number;
};

/*
 * synaptics_rmi4_fn_full_addr - full 16-bit base addresses
 * @query_base: 16-bit base address for query registers
 * @cmd_base: 16-bit base address for data registers
 * @ctrl_base: 16-bit base address for command registers
 * @data_base: 16-bit base address for control registers
 */
struct synaptics_rmi4_fn_full_addr {
	unsigned short query_base;
	unsigned short cmd_base;
	unsigned short ctrl_base;
	unsigned short data_base;
};

/*
 * struct synaptics_rmi4_fn - function handler data structure
 * @fn_number: function number
 * @num_of_data_sources: number of data sources
 * @num_of_data_points: maximum number of fingers supported
 * @size_of_data_register_block: data register block size
 * @data1_offset: offset to data1 register from data base address
 * @intr_reg_num: index to associated interrupt register
 * @intr_mask: interrupt mask
 * @full_addr: full 16-bit base addresses of function registers
 * @link: linked list for function handlers
 * @data_size: size of private data
 * @data: pointer to private data
 */
struct synaptics_rmi4_fn {
	unsigned char fn_number;
	unsigned char num_of_data_sources;
	unsigned char num_of_data_points;
	unsigned char size_of_data_register_block;
	unsigned char intr_reg_num;
	unsigned char intr_mask;
	struct synaptics_rmi4_fn_full_addr full_addr;
	struct list_head link;
	int data_size;
	void *data;
	void *extra;
};

/*
 * struct synaptics_rmi4_device_info - device information
 * @version_major: rmi protocol major version number
 * @version_minor: rmi protocol minor version number
 * @manufacturer_id: manufacturer id
 * @product_props: product properties information
 * @product_info: product info array
 * @date_code: device manufacture date
 * @tester_id: tester id array
 * @serial_number: device serial number
 * @product_id_string: device product id
 * @support_fn_list: linked list for function handlers
 * @exp_fn_list: linked list for expanded function handlers
 */
struct synaptics_rmi4_device_info {
	unsigned int version_major;
	unsigned int version_minor;
	unsigned char manufacturer_id;
	unsigned char product_props;
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	unsigned char date_code[SYNAPTICS_RMI4_DATE_CODE_SIZE];
	unsigned short tester_id;
	unsigned short serial_number;
	unsigned char product_id_string[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned char build_id[SYNAPTICS_RMI4_BUILD_ID_SIZE];
	unsigned int package_id;
	unsigned int package_rev;
	unsigned int pr_number;
	struct list_head support_fn_list;
	struct list_head exp_fn_list;
};

/**
 * struct synaptics_finger - Represents fingers.
 * @ state: finger status.
 * @ mcount: moving counter for debug.
 * @ stylus: represent stylus..
 */
struct synaptics_finger {
	unsigned char state;
	unsigned short mcount;
#ifdef USE_STYLUS
	bool stylus;
#endif

	int x;
	int y;
	int wx;
	int wy;
#ifdef REPORT_2D_Z
	int z;
#endif
	unsigned char tool_type;
	unsigned char finger_status;
	int print_type;

};

enum {
	TOUCH_NONE = 0,
	TOUCH_RELEASE,
	TOUCH_RELEASE_FORCED,
	TOUCH_PRESS,
	TOUCH_CHANGE,
};

struct synaptics_rmi4_f12_handle {
/* CTRL */
	unsigned short ctrl11_addr;		/* F12_2D_CTRL11 : for jitter level*/
	unsigned short ctrl15_addr;		/* F12_2D_CTRL15 : for finger amplitude threshold */
	unsigned short ctrl23_addr;		/* F12_2D_CTRL23 : object report enable */
	unsigned char obj_report_enable;	/* F12_2D_CTRL23 */
	unsigned short ctrl20_addr;		/* F12_2D_CTRL20 : for lpwg mode */
	unsigned short ctrl26_addr;		/* F12_2D_CTRL26 : for glove mode */
	unsigned char feature_enable;	/* F12_2D_CTRL26 */
	unsigned short ctrl27_addr;		/* F12_2D_CTRL26 : for lpwg mode - report rate */
	unsigned short ctrl28_addr;		/* F12_2D_CTRL28 : for report data */
	unsigned char report_enable;	/* F12_2D_CTRL28 */
/* QUERY */
	unsigned char glove_mode_feature;	/* F12_2D_QUERY_10 */
};

enum {
	BUILT_IN = 0,
	UMS,
	BUILT_IN_FAC,
	FFU,
};

enum bl_version {
	V5 = 5,
	V6 = 6,
	BL_V5 = 5,
	BL_V6 = 6,
	BL_V7 = 7,
};

#define IMAGE_HEADER_VERSION_05 0x05
#define IMAGE_HEADER_VERSION_06 0x06
#define IMAGE_HEADER_VERSION_10 0x10

#define IMAGE_AREA_OFFSET 0x100
#define LOCKDOWN_SIZE 0x50

enum v7_flash_command2 {
	CMD_V7_IDLE = 0x00,
	CMD_V7_ENTER_BL,
	CMD_V7_READ,
	CMD_V7_WRITE,
	CMD_V7_ERASE,
	CMD_V7_ERASE_AP,
	CMD_V7_SENSOR_ID,
};

enum v7_flash_command {
	v7_CMD_IDLE = 0,
	v7_CMD_WRITE_FW,
	v7_CMD_WRITE_CONFIG,
	v7_CMD_WRITE_LOCKDOWN,
	v7_CMD_WRITE_GUEST_CODE,
	v7_CMD_READ_CONFIG,
	v7_CMD_ERASE_ALL,
	v7_CMD_ERASE_UI_FIRMWARE,
	v7_CMD_ERASE_UI_CONFIG,
	v7_CMD_ERASE_BL_CONFIG,
	v7_CMD_ERASE_DISP_CONFIG,
	v7_CMD_ERASE_FLASH_CONFIG,
	v7_CMD_ERASE_GUEST_CODE,
	v7_CMD_ENABLE_FLASH_PROG,
};

enum v7_config_area {
	v7_UI_CONFIG_AREA = 0,
	v7_PM_CONFIG_AREA,
	v7_BL_CONFIG_AREA,
	v7_DP_CONFIG_AREA,
	v7_FLASH_CONFIG_AREA,
};

enum v7_partition_id {
	BOOTLOADER_PARTITION = 0x01,
	DEVICE_CONFIG_PARTITION,
	FLASH_CONFIG_PARTITION,
	MANUFACTURING_BLOCK_PARTITION,
	GUEST_SERIALIZATION_PARTITION,
	GLOBAL_PARAMETERS_PARTITION,
	CORE_CODE_PARTITION,
	CORE_CONFIG_PARTITION,
	GUEST_CODE_PARTITION,
	DISPLAY_CONFIG_PARTITION,
};

struct partition_table {
	unsigned char partition_id:5;
	unsigned char byte_0_reserved:3;
	unsigned char byte_1_reserved;
	unsigned char partition_length_7_0;
	unsigned char partition_length_15_8;
	unsigned char start_physical_address_7_0;
	unsigned char start_physical_address_15_8;
	unsigned char partition_properties_7_0;
	unsigned char partition_properties_15_8;
} __packed;

struct physical_address {
	unsigned short ui_firmware;
	unsigned short ui_config;
	unsigned short dp_config;
	unsigned short guest_code;
};

struct block_count {
	unsigned short ui_firmware;
	unsigned short ui_config;
	unsigned short dp_config;
	unsigned short fl_config;
	unsigned short pm_config;
	unsigned short bl_config;
	unsigned short lockdown;
	unsigned short guest_code;
};

struct f34_v7_query_0 {
	union {
		struct {
			unsigned char subpacket_1_size:3;
			unsigned char has_config_id:1;
			unsigned char f34_query0_b4:1;
			unsigned char has_thqa:1;
			unsigned char f34_query0_b6__7:2;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_v7_query_1_7 {
	union {
		struct {
			/* query 1 */
			unsigned char bl_minor_revision;
			unsigned char bl_major_revision;

			/* query 2 */
			unsigned char bl_fw_id_7_0;
			unsigned char bl_fw_id_15_8;
			unsigned char bl_fw_id_23_16;
			unsigned char bl_fw_id_31_24;

			/* query 3 */
			unsigned char minimum_write_size;
			unsigned char block_size_7_0;
			unsigned char block_size_15_8;
			unsigned char flash_page_size_7_0;
			unsigned char flash_page_size_15_8;

			/* query 4 */
			unsigned char adjustable_partition_area_size_7_0;
			unsigned char adjustable_partition_area_size_15_8;

			/* query 5 */
			unsigned char flash_config_length_7_0;
			unsigned char flash_config_length_15_8;

			/* query 6 */
			unsigned char payload_length_7_0;
			unsigned char payload_length_15_8;

			/* query 7 */
			unsigned char f34_query7_b0:1;
			unsigned char has_bootloader:1;
			unsigned char has_device_config:1;
			unsigned char has_flash_config:1;
			unsigned char has_manufacturing_block:1;
			unsigned char has_guest_serialization:1;
			unsigned char has_global_parameters:1;
			unsigned char has_core_code:1;
			unsigned char has_core_config:1;
			unsigned char has_guest_code:1;
			unsigned char has_display_config:1;
			unsigned char f34_query7_b11__15:5;
			unsigned char f34_query7_b16__23;
			unsigned char f34_query7_b24__31;
		} __packed;
		unsigned char data[21];
	};
};

struct f34_v7_data_1_5 {
	union {
		struct {
			unsigned char partition_id:5;
			unsigned char f34_data1_b5__7:3;
			unsigned char block_offset_7_0;
			unsigned char block_offset_15_8;
			unsigned char transfer_length_7_0;
			unsigned char transfer_length_15_8;
			unsigned char command;
			unsigned char payload_0;
			unsigned char payload_1;
		} __packed;
		unsigned char data[8];
	};
};

static inline int secure_memcpy(unsigned char *dest, unsigned int dest_size,
		const unsigned char *src, unsigned int src_size,
		unsigned int count)
{
	if (dest == NULL || src == NULL)
		return -EINVAL;

	if (count > dest_size || count > src_size)
		return -EINVAL;

	memcpy((void *)dest, (const void *)src, count);

	return 0;
}
struct f01_device_control {
	union {
		struct {
			unsigned char sleep_mode:2;
			unsigned char nosleep:1;
			unsigned char reserved:2;
			unsigned char charger_connected:1;
			unsigned char report_rate:1;
			unsigned char configured:1;
		} __packed;
		unsigned char data[1];
	};
};


enum flash_area {
	NONE,
	UI_FIRMWARE,
	CONFIG_AREA,
};

enum update_mode {
	UPDATE_MODE_NORMAL = 1,
	UPDATE_MODE_FORCE = 2,
	UPDATE_MODE_LOCKDOWN = 8,
};

enum flash_command {
	CMD_IDLE = 0x0,
	CMD_WRITE_FW_BLOCK = 0x2,
	CMD_ERASE_ALL = 0x3,
	CMD_WRITE_LOCKDOWN_BLOCK = 0x4,
	CMD_READ_CONFIG_BLOCK = 0x5,
	CMD_WRITE_CONFIG_BLOCK = 0x6,
	CMD_ERASE_CONFIG = 0x7,
	CMD_READ_SENSOR_ID = 0x8,
	CMD_ERASE_BL_CONFIG = 0x9,
	CMD_ERASE_DISP_CONFIG = 0xA,
	CMD_ERASE_GUEST_CODE = 0xB,
	CMD_WRITE_GUEST_CODE = 0xC,
	CMD_ENABLE_FLASH_PROG = 0xF
};

struct img_x0x6_header {
	/* 0x00 - 0x0f */
	unsigned char checksum[4];
	unsigned char reserved_04;
	unsigned char reserved_05;
	unsigned char options_firmware_id:1;
	unsigned char options_contain_bootloader:1;
	unsigned char options_reserved:6;
	unsigned char bootloader_version;
	unsigned char firmware_size[4];
	unsigned char config_size[4];
	/* 0x10 - 0x1f */
	unsigned char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE];
	unsigned char package_id[2];
	unsigned char package_id_revision[2];
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	/* 0x20 - 0x2f */
	unsigned char reserved_20_2f[16];
	/* 0x30 - 0x3f */
	unsigned char ds_firmware_info[16];
	/* 0x40 - 0x4f */
	unsigned char ds_info[10];
	unsigned char reserved_4a_4f[6];
	/* 0x50 - 0x53 */
	unsigned char firmware_id[4];
};

enum img_x10_container_id {
	ID_TOP_LEVEL_CONTAINER = 0,
	ID_UI_CONTAINER,
	ID_UI_CONFIGURATION,
	ID_BOOTLOADER_CONTAINER,
	ID_BOOTLOADER_IMAGE_CONTAINER,
	ID_BOOTLOADER_CONFIGURATION_CONTAINER,
	ID_BOOTLOADER_LOCKDOWN_INFORMATION_CONTAINER,
	ID_PERMANENT_CONFIGURATION_CONTAINER,
	ID_GUEST_CODE_CONTAINER,
	ID_BOOTLOADER_PROTOCOL_DESCRIPTOR_CONTAINER,
	ID_UI_PROTOCOL_DESCRIPTOR_CONTAINER,
	ID_RMI_SELF_DISCOVERY_CONTAINER,
	ID_RMI_PAGE_CONTENT_CONTAINER,
	ID_GENERAL_INFORMATION_CONTAINER,
	RESERVERD
};

struct block_data {
	// for v7
	unsigned char *data;
	int size;
};

#define PRODUCT_INFO_SIZE 2
#define PRODUCT_ID_SIZE 10
#define BUILD_ID_SIZE 3

struct image_header_05_06 {
	/* 0x00 - 0x0f */
	unsigned char checksum[4];
	unsigned char reserved_04;
	unsigned char reserved_05;
	unsigned char options_firmware_id:1;
	unsigned char options_bootloader:1;
	unsigned char options_guest_code:1;
	unsigned char options_tddi:1;
	unsigned char options_reserved:4;
	unsigned char header_version;
	unsigned char firmware_size[4];
	unsigned char config_size[4];
	/* 0x10 - 0x1f */
	unsigned char product_id[PRODUCT_ID_SIZE];
	unsigned char package_id[2];
	unsigned char package_id_revision[2];
	unsigned char product_info[PRODUCT_INFO_SIZE];
	/* 0x20 - 0x2f */
	unsigned char bootloader_addr[4];
	unsigned char bootloader_size[4];
	unsigned char ui_addr[4];
	unsigned char ui_size[4];
	/* 0x30 - 0x3f */
	unsigned char ds_id[16];
	/* 0x40 - 0x4f */
	union {
		struct {
			unsigned char cstmr_product_id[PRODUCT_ID_SIZE];
			unsigned char reserved_4a_4f[6];
		};
		struct {
			unsigned char dsp_cfg_addr[4];
			unsigned char dsp_cfg_size[4];
			unsigned char reserved_48_4f[8];
		};
	};
	/* 0x50 - 0x53 */
	unsigned char firmware_id[4];
};

struct container_descriptor {
	unsigned char content_checksum[4];
	unsigned char container_id[2];
	unsigned char minor_version;
	unsigned char major_version;
	unsigned char reserved_08;
	unsigned char reserved_09;
	unsigned char reserved_0a;
	unsigned char reserved_0b;
	unsigned char container_option_flags[4];
	unsigned char content_options_length[4];
	unsigned char content_options_address[4];
	unsigned char content_length[4];
	unsigned char content_address[4];
};

enum container_id {
	TOP_LEVEL_CONTAINER = 0,
	UI_CONTAINER,
	UI_CONFIG_CONTAINER,
	BL_CONTAINER,
	BL_IMAGE_CONTAINER,
	BL_CONFIG_CONTAINER,
	BL_LOCKDOWN_INFO_CONTAINER,
	PERMANENT_CONFIG_CONTAINER,
	GUEST_CODE_CONTAINER,
	BL_PROTOCOL_DESCRIPTOR_CONTAINER,
	UI_PROTOCOL_DESCRIPTOR_CONTAINER,
	RMI_SELF_DISCOVERY_CONTAINER,
	RMI_PAGE_CONTENT_CONTAINER,
	GENERAL_INFORMATION_CONTAINER,
	DEVICE_CONFIG_CONTAINER,
	FLASH_CONFIG_CONTAINER,
	GUEST_SERIALIZATION_CONTAINER,
	GLOBAL_PARAMETERS_CONTAINER,
	CORE_CODE_CONTAINER,
	CORE_CONFIG_CONTAINER,
	DISPLAY_CONFIG_CONTAINER,
};


struct image_header_10 {
	unsigned char checksum[4];
	unsigned char reserved_04;
	unsigned char reserved_05;
	unsigned char minor_header_version;
	unsigned char major_header_version;
	unsigned char reserved_08;
	unsigned char reserved_09;
	unsigned char reserved_0a;
	unsigned char reserved_0b;
	unsigned char top_level_container_start_addr[4];
};

struct image_metadata {
	bool contains_firmware_id;
	bool contains_bootloader;
	bool contains_disp_config;
	bool contains_guest_code;
	bool contains_flash_config;
	unsigned int firmware_id;
	unsigned int checksum;
	unsigned int bootloader_size;
	unsigned int disp_config_offset;
	unsigned char bl_version;
	unsigned char product_id[PRODUCT_ID_SIZE + 1];
	unsigned char cstmr_product_id[PRODUCT_ID_SIZE + 1];
	struct block_data bootloader;
	struct block_data ui_firmware;
	struct block_data ui_config;
	struct block_data dp_config;
	struct block_data fl_config;
	struct block_data bl_config;
	struct block_data guest_code;
	struct block_data lockdown;
	struct block_count blkcount;
	struct physical_address phyaddr;
};
struct register_offset {
	unsigned char properties;
	unsigned char properties_2;
	unsigned char block_size;
	unsigned char block_count;
	unsigned char gc_block_count;
	unsigned char flash_status;
	unsigned char partition_id;
	unsigned char block_number;
	unsigned char transfer_length;
	unsigned char flash_cmd;
	unsigned char payload;
};

struct img_file_content {
	unsigned char *fw_image;
	unsigned int image_size;
	unsigned char *image_name;
	unsigned char imageFileVersion;
	struct block_data uiFirmware;
	struct block_data uiConfig;
	struct block_data guestCode;
	struct block_data lockdown;
	struct block_data permanent;
	struct block_data bootloaderInfo;
	unsigned char blMajorVersion;
	unsigned char blMinorVersion;
	unsigned char *configId;	/* len 0x4 */
	unsigned char *firmwareId;	/* len 0x4 */
	unsigned char *packageId;		/* len 0x4 */
	unsigned char *dsFirmwareInfo;	/* len 0x10 */
};

struct img_x10_descriptor {
	unsigned char contentChecksum[4];
	unsigned char containerID[2];
	unsigned char minorVersion;
	unsigned char majorVersion;
	unsigned char reserverd[4];
	unsigned char containerOptionFlags[4];
	unsigned char contentOptionLength[4];
	unsigned char contentOptionAddress[4];
	unsigned char contentLength[4];
	unsigned char contentAddress[4];
};

struct img_x10_bl_container {
	unsigned char majorVersion;
	unsigned char minorVersion;
	unsigned char reserved[2];
	unsigned char *subContainer;
};

struct pdt_properties {
	union {
		struct {
			unsigned char reserved_1:6;
			unsigned char has_bsr:1;
			unsigned char reserved_2:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_fwu_handle {
	enum bl_version bl_version;
	bool initialized;
	bool program_enabled;
	bool has_perm_config;
	bool has_bl_config;
	bool has_disp_config;
	bool has_guest_code;
	bool force_update;
	bool in_flash_prog_mode;
	bool do_lockdown;
	bool can_guest_bootloader;
	unsigned int data_pos;
	unsigned char *ext_data_source;
	unsigned char *read_config_buf;
	unsigned char intr_mask;
	unsigned char command;
	unsigned char bootloader_id[4];
	unsigned char bootloader_id_ic[2];
	unsigned char flash_status;
	unsigned char productinfo1;
	unsigned char productinfo2;
	unsigned char properties_off;
	unsigned char blk_size_off;
	unsigned char blk_count_off;
	unsigned char blk_data_off;
	unsigned char properties2_off;
	unsigned char guest_blk_count_off;
	unsigned char flash_cmd_off;
	unsigned char flash_status_off;
	unsigned short block_size;
	unsigned short fw_block_count;
	unsigned short config_block_count;
	unsigned short perm_config_block_count;
	unsigned short bl_config_block_count;
	unsigned short disp_config_block_count;
	unsigned short guest_code_block_count;
	unsigned short config_size;
	unsigned short config_area;
	char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];

	struct synaptics_rmi4_f34_query_01 flash_properties;
	struct workqueue_struct *fwu_workqueue;
	struct delayed_work fwu_work;
	struct synaptics_rmi4_fn_desc f01_fd;
	struct synaptics_rmi4_fn_desc f34_fd;
	struct synaptics_rmi4_data *rmi4_data;
	struct img_file_content img;
	bool polling_mode;
	struct kobject *attr_dir;

	unsigned short flash_config_length;
	unsigned short payload_length;
	struct register_offset off;
	unsigned char partitions;
	unsigned short partition_table_bytes;
	unsigned short read_config_buf_size;
	struct block_count blkcount;
	struct physical_address phyaddr;
	struct image_metadata v7_img;
	bool new_partition_table;
	const unsigned char *config_data;
//	const unsigned char *image;
	unsigned char *image;
	bool in_bl_mode;
};

#ifdef FACTORY_MODE
#include <linux/uaccess.h>

#define CMD_STR_LEN 32
#define CMD_PARAM_NUM 8
#define CMD_RESULT_STR_LEN 768
#define RPT_DATA_STRNCAT_LENGTH 9

#define DEBUG_RESULT_STR_LEN	1024
#define MAX_VAL_OFFSET_AND_LENGTH	10
#define DEBUG_STR_LEN	(CMD_STR_LEN * 2)

#define DEBUG_PRNT_SCREEN(_dest, _temp, _length, fmt, ...)	\
({	\
	snprintf(_temp, _length, fmt, ## __VA_ARGS__);	\
	strcat(_dest, _temp);	\
})

#define FT_CMD(name, func) .cmd_name = name, .cmd_func = func

enum CMD_STATUS {
	CMD_STATUS_WAITING = 0,
	CMD_STATUS_RUNNING,
	CMD_STATUS_OK,
	CMD_STATUS_FAIL,
	CMD_STATUS_NOT_APPLICABLE,
};

struct ft_cmd {
	const char *cmd_name;
	void (*cmd_func)(void *dev_data);
	struct list_head list;
};

struct factory_data {
	struct device *fac_dev_ts;
	short *rawcap_data;
	short *delta_data;
	int *abscap_data;
	int *absdelta_data;
	char *trx_short;
	unsigned int abscap_rx_min;
	unsigned int abscap_rx_max;
	unsigned int abscap_tx_min;
	unsigned int abscap_tx_max;
	bool cmd_is_running;
	unsigned char cmd_state;
	char cmd[CMD_STR_LEN];
	int cmd_param[CMD_PARAM_NUM];
	char cmd_buff[CMD_RESULT_STR_LEN];
	char cmd_result[CMD_RESULT_STR_LEN];
	struct mutex cmd_lock;
	struct list_head cmd_list_head;
};
#endif

enum f54_report_types {
	F54_8BIT_IMAGE = 1,
	F54_16BIT_IMAGE = 2,
	F54_RAW_16BIT_IMAGE = 3,
	F54_HIGH_RESISTANCE = 4,
	F54_TX_TO_TX_SHORT = 5,
	F54_RX_TO_RX1 = 7,
	F54_TRUE_BASELINE = 9,
	F54_FULL_RAW_CAP_MIN_MAX = 13,
	F54_RX_OPENS1 = 14,
	F54_TX_OPEN = 15,
	F54_TX_TO_GROUND = 16,
	F54_RX_TO_RX2 = 17,
	F54_RX_OPENS2 = 18,
	F54_FULL_RAW_CAP = 19,
	F54_FULL_RAW_CAP_RX_COUPLING_COMP = 20,
	F54_SENSOR_SPEED = 22,
	F54_ADC_RANGE = 23,
	F54_TREX_OPENS = 24,
	F54_TREX_TO_GND = 25,
	F54_TREX_SHORTS = 26,
	F54_ABS_CAP = 38,
	F54_ABS_DELTA = 40,
	F54_ABS_ADC = 42,
	INVALID_REPORT_TYPE = -1,
};

struct synaptics_rmi4_f54_handle {
	bool no_auto_cal;
	unsigned char status;
	unsigned char intr_mask;
	unsigned char intr_reg_num;
	unsigned char rx_assigned;
	unsigned char tx_assigned;
	unsigned char *report_data;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	unsigned short fifoindex;
	unsigned int report_size;
	unsigned int data_buffer_size;
	enum f54_report_types report_type;
	struct mutex status_mutex;
	struct mutex data_mutex;
	struct mutex control_mutex;
	struct f54_query query;
	struct f54_query_13 query_13;
	struct f54_query_15 query_15;
	struct f54_query_16 query_16;
	struct f54_query_21 query_21;
	struct f54_query_22 query_22;
	struct f54_query_25 query_25;
	struct f54_query_27 query_27;
	struct f54_query_29 query_29;
	struct f54_query_30 query_30;
	struct f54_query_32 query_32;
	struct f54_query_33 query_33;
	struct f54_query_36 query_36;
	struct f54_query_38 query_38;
	struct f54_query_39 query_39;
	struct f54_query_40 query_40;
	struct f54_query_43 query_43;
	struct f54_query_46 query_46;
	struct f54_query_47 query_47;
	struct f54_query_49 query_49;
	struct f54_query_50 query_50;
	struct f54_query_51 query_51;
	struct f54_query_55 query_55;
	struct f54_control control;
#ifdef FACTORY_MODE
	struct factory_data *factory_data;
#endif
	struct kobject *attr_dir;
	struct hrtimer watchdog;
	struct work_struct timeout_work;
	struct delayed_work status_work;
	struct workqueue_struct *status_workqueue;
	struct synaptics_rmi4_data *rmi4_data;
};

struct rmidev_data {
	int ref_count;
	struct cdev main_dev;
	struct class *device_class;
	struct mutex file_mutex;
};

struct rmidev_handle {
	dev_t dev_no;
	struct device dev;
	struct kobject *attr_dir;
	struct rmidev_data dev_data;
	bool irq_enabled;
	struct synaptics_rmi4_data *rmi4_data;
};

#ifdef PRINT_DEBUG_INFO
struct synaptics_rmi4_f51_delta_info {
	union {
		struct {
			unsigned char deltamin_lsb;
			unsigned char deltamin_msb;
			unsigned char deltamax_lsb;
			unsigned char deltamax_msb;
			unsigned char deltaavg_lsb;
			unsigned char deltaavg_msb;
			unsigned char fast_relax;
		} __packed;
		unsigned char data[7];
	};
};
struct synaptics_rmi4_f54_cid_im {
	union {
		struct {
			unsigned char cid_im_low;
			unsigned char cid_im_high;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f54_freq_im {
	union {
		struct {
			unsigned char freq_im_low;
			unsigned char freq_im_high;
		} __packed;
		unsigned char data[2];
	};
};
#endif

/*
 * struct synaptics_rmi4_data - rmi4 device instance data
 * @i2c_client: pointer to associated i2c client
 * @input_dev: pointer to associated input device
 * @board: constant pointer to platform data
 * @rmi4_mod_info: device information
 * @regulator: pointer to associated regulator
 * @rmi4_io_ctrl_mutex: mutex for i2c i/o control
 * @early_suspend: instance to support early suspend power management
 * @button_0d_enabled: flag for 0d button support
 * @full_pm_cycle: flag for full power management cycle in early suspend stage
 * @num_of_intr_regs: number of interrupt registers
 * @f01_query_base_addr: query base address for f01
 * @f01_cmd_base_addr: command base address for f01
 * @f01_ctrl_base_addr: control base address for f01
 * @f01_data_base_addr: data base address for f01
 * @irq: attention interrupt
 * @sensor_max_x: sensor maximum x value
 * @sensor_max_y: sensor maximum y value
 * @irq_enabled: flag for indicating interrupt enable status
 * @touch_stopped: flag to stop interrupt thread processing
 * @fingers_on_2d: flag to indicate presence of fingers in 2d area
 * @sensor_sleep: flag to indicate sleep state of sensor
 * @wait: wait queue for touch data polling in interrupt thread
 * @i2c_read: pointer to i2c read function
 * @i2c_write: pointer to i2c write function
 * @irq_enable: pointer to irq enable function
 */
struct synaptics_rmi4_data {
	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
	const struct synaptics_rmi4_platform_data *board;
	struct synaptics_rmi4_device_info rmi4_mod_info;
	struct mutex rmi4_reset_mutex;
	struct mutex rmi4_io_ctrl_mutex;
	struct mutex rmi4_reflash_mutex;
	struct timer_list f51_finger_timer;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	const char *firmware_name;

	struct completion init_done;
	struct synaptics_finger finger[MAX_NUMBER_OF_FINGERS];

	unsigned char button_0d_enabled;
	unsigned char full_pm_cycle;
	unsigned char num_of_rx;
	unsigned char num_of_tx;
	unsigned int num_of_node;
	unsigned char num_of_fingers;
	unsigned char max_touch_width;
	unsigned char intr_mask[MAX_INTR_REGISTERS];
	unsigned short num_of_intr_regs;
	unsigned char *button_txrx_mapping;
	unsigned short f01_query_base_addr;
	unsigned short f01_cmd_base_addr;
	unsigned short f01_ctrl_base_addr;
	unsigned short f01_data_base_addr;
	unsigned short f34_ctrl_base_addr;
#ifdef PRINT_DEBUG_INFO
	unsigned short f51_query_base;
	unsigned short f51_data_base;
	unsigned short f51_delta_info_addr;
#endif
	int irq;
	int sensor_max_x;
	int sensor_max_y;
	bool flash_prog_mode;
	bool irq_enabled;
	bool touch_stopped;
	bool fingers_on_2d;
	bool f51_finger;
	bool sensor_sleep;
	bool stay_awake;
	bool staying_awake;
	bool tsp_probe;
	bool tsp_pwr_enabled;
	unsigned char fingers_already_present;
#ifdef USE_ACTIVE_REPORT_RATE
	int tsp_change_report_rate;
#endif

	enum synaptics_product_ids product_id;			/* product id of ic */
	unsigned char product_id_string_of_bin[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	int ic_revision_of_ic;		/* revision of reading from IC */
	int fw_version_of_ic;		/* firmware version of IC */
	int ic_revision_of_bin;		/* revision of reading from binary */
	int fw_version_of_bin;		/* firmware version of binary */
	int fw_release_date_of_ic;	/* Config release data from IC */
	u32 panel_revision;			/* Octa panel revision */
	unsigned char bootloader_id[SYNAPTICS_BOOTLOADER_ID_SIZE];	/* Bootloader ID */
	bool doing_reflash;
	int rebootcount;

	int bootloader_version;	/* Bootloader Version for v7 */
	unsigned char bootloader_revision[SYNAPTICS_BOOTLOADER_REVISION];	/* Bootloader Revision */

#ifdef GLOVE_MODE
	bool fast_glove_state;
	bool touchkey_glove_mode_status;
#endif
	unsigned char ddi_type;

	struct synaptics_rmi4_f12_handle f12;
	struct synaptics_rmi4_fwu_handle *fwu;
#ifdef PROXIMITY_MODE
	struct synaptics_rmi4_f51_handle *f51;
#endif
	struct synaptics_rmi4_f54_handle *f54;
	struct rmidev_handle *rmidev;

	struct delayed_work rezero_work;

	struct mutex rmi4_device_mutex;
#ifdef SIDE_TOUCH
	unsigned char sidekey_data;
#endif
	bool use_stylus;
#ifdef SYNAPTICS_RMI_INFORM_CHARGER
	int ta_status;
	void (*register_cb)(struct synaptics_rmi_callbacks *);
	struct synaptics_rmi_callbacks callbacks;
#endif
	bool use_deepsleep;
#ifdef USE_LPGW_MODE
	bool use_lpwg_sleep;
	struct mutex rmi4_lpgw_mutex;
	unsigned char scrub_id;
#endif
#ifdef GESTURE_3FIN
	int gesture_3fin_code;
#endif
	int orientation;
	struct pinctrl *pinctrl;

	int (*i2c_read)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*i2c_write)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*irq_enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
	int (*reset_device)(struct synaptics_rmi4_data *rmi4_data);
	int (*stop_device)(struct synaptics_rmi4_data *rmi4_data);
	int (*start_device)(struct synaptics_rmi4_data *rmi4_data);
	void (*sleep_device)(struct synaptics_rmi4_data *rmi4_data);
	void (*wake_device)(struct synaptics_rmi4_data *rmi4_data);
};

enum exp_fn {
	RMI_DEV = 0,
	RMI_F54,
	RMI_FW_UPDATER,
	RMI_DB,
	RMI_GUEST,
	RMI_LAST,
};

struct synaptics_rmi4_exp_fn {
	enum exp_fn fn_type;
	bool initialized;
	int (*func_init)(struct synaptics_rmi4_data *rmi4_data);
	int (*func_reinit)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_remove)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask);
	struct list_head link;
};

struct synaptics_rmi4_exp_fn_ptr {
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
};

int synaptics_rmi4_new_function(enum exp_fn fn_type,
		struct synaptics_rmi4_data *rmi4_data,
		int (*func_init)(struct synaptics_rmi4_data *rmi4_data),
		int (*func_reinit)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_remove)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
				unsigned char intr_mask));

int rmidev_module_register(struct synaptics_rmi4_data *rmi4_data);
int rmi4_f54_module_register(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_f54_set_control(struct synaptics_rmi4_data *rmi4_data);
int rmi4_fw_update_module_register(struct synaptics_rmi4_data *rmi4_data);
int rmidb_module_register(struct synaptics_rmi4_data *rmi4_data);
int rmi_guest_module_register(struct synaptics_rmi4_data *rmi4_data);

int synaptics_fw_updater(struct synaptics_rmi4_data *rmi4_data, unsigned char *fw_data);
int synaptics_rmi4_fw_update_on_probe(struct synaptics_rmi4_data *rmi4_data);
void fwu_parse_image_header_10_simple(struct synaptics_rmi4_data *rmi4_data, unsigned char *image);

int synaptics_rmi4_f12_ctrl11_set(struct synaptics_rmi4_data *rmi4_data, unsigned char data);
int synaptics_rmi4_set_tsp_test_result_in_config(struct synaptics_rmi4_data *rmi4_data, int value);
int synaptics_rmi4_read_tsp_test_result(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_access_register(struct synaptics_rmi4_data *rmi4_data,
				bool mode, unsigned short address, int length, unsigned char *value);
void synpatics_rmi4_release_all_event(struct synaptics_rmi4_data *rmi4_data, unsigned char type);

#ifdef PROXIMITY_MODE
int synaptics_rmi4_proximity_enables(struct synaptics_rmi4_data *rmi4_data, unsigned char enables);
#endif
#ifdef GLOVE_MODE
int synaptics_rmi4_glove_mode_enables(struct synaptics_rmi4_data *rmi4_data);
#endif
#ifdef SYNAPTICS_RMI_INFORM_CHARGER
extern void synaptics_tsp_register_callback(struct synaptics_rmi_callbacks *cb);
#endif
#ifdef USE_LPGW_MODE
int set_lpgw_mode(struct synaptics_rmi4_data *rmi4_data, int on);
#endif
#ifdef USE_ACTIVE_REPORT_RATE
int change_report_rate(struct synaptics_rmi4_data *rmi4_data, int mode);
#endif
static inline struct device *rmi_attr_kobj_to_dev(struct kobject *kobj)
{
	return container_of(kobj->parent, struct device, kobj);
}

static inline void *rmi_attr_kobj_to_drvdata(struct kobject *kobj)
{
	return dev_get_drvdata(rmi_attr_kobj_to_dev(kobj));
}

static inline ssize_t synaptics_rmi4_show_error(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct device *dev = rmi_attr_kobj_to_dev(kobj);

	dev_warn(dev, "%s Attempted to read from write-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t synaptics_rmi4_store_error(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct device *dev = rmi_attr_kobj_to_dev(kobj);

	dev_warn(dev, "%s Attempted to write to read-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static inline void hstoba(unsigned char *dest, unsigned short src)
{
	dest[0] = src % 0x100;
	dest[1] = src / 0x100;
}

#define RMI_KOBJ_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute kobj_attr_##_name = __ATTR(_name, _mode, _show, _store)

#endif
