/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SSP_PRJ_H__
#define __SSP_PRJ_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/math64.h>
#include <linux/rtc.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/spi/spi.h>
#include <linux/battery/sec_battery.h>
#include "../../staging/android/android_alarm.h"
#include "factory/ssp_factory.h"
#include "factory/ssp_mcu.h"
#include "ssp_sensorlist.h"
#include "ssp_data.h"
#include "ssp_debug.h"
#include "ssp_dev.h"
#include "ssp_firmware.h"
#include "ssp_iio.h"
#include "ssp_misc.h"
#include "ssp_sensorhub.h"
#include "ssp_spi.h"
#include "ssp_sysfs.h"
#include "sensors_core.h"


#undef CONFIG_SEC_DEBUG
#define CONFIG_SEC_DEBUG	0

#define DEFUALT_POLLING_DELAY	(200 * NSEC_PER_MSEC)
#define DATA_PACKET_SIZE	960

/* AP -> SSP Instruction */
#define MSG2SSP_INST_BYPASS_SENSOR_ADD		0xA1
#define MSG2SSP_INST_BYPASS_SENSOR_REMOVE	0xA2
#define MSG2SSP_INST_REMOVE_ALL			0xA3
#define MSG2SSP_INST_CHANGE_DELAY		0xA4
#define MSG2SSP_INST_LIBRARY_ADD		0xB1
#define MSG2SSP_INST_LIBRARY_REMOVE		0xB2
#define MSG2SSP_INST_LIB_NOTI			0xB4
#define MSG2SSP_INST_LIB_DATA			0xC1

#define MSG2SSP_AP_FUSEROM			0X01

#define MSG2SSP_AP_MCU_SET_GYRO_CAL		0xCD
#define MSG2SSP_AP_MCU_SET_ACCEL_CAL		0xCE
#define MSG2SSP_AP_STATUS_SHUTDOWN		0xD0
#define MSG2SSP_AP_STATUS_WAKEUP		0xD1
#define MSG2SSP_AP_STATUS_SLEEP			0xD2
#define MSG2SSP_AP_STATUS_RESUME		0xD3
#define MSG2SSP_AP_STATUS_SUSPEND		0xD4
#define MSG2SSP_AP_STATUS_RESET			0xD5
#define MSG2SSP_AP_STATUS_POW_CONNECTED		0xD6
#define MSG2SSP_AP_STATUS_POW_DISCONNECTED	0xD7
#define MSG2SSP_AP_TEMPHUMIDITY_CAL_DONE	0xDA
#define MSG2SSP_AP_MCU_SET_DUMPMODE		0xDB
#define MSG2SSP_AP_MCU_BATCH_FLUSH		0xDD
#define MSG2SSP_AP_MCU_BATCH_COUNT		0xDF

#ifdef CONFIG_SENSORS_MULTIPLE_GLASS_TYPE
#define MSG2SSP_AP_GLASS_TYPE             0xEC
#endif

#define MSG2SSP_AP_WHOAMI			0x0F
#define MSG2SSP_AP_FIRMWARE_REV			0xF0
#define MSG2SSP_AP_SENSOR_FORMATION		0xF1
#define MSG2SSP_AP_SENSOR_PROXTHRESHOLD		0xF2
#define MSG2SSP_AP_SENSOR_BARCODE_EMUL		0xF3
#define MSG2SSP_AP_SENSOR_SCANNING		0xF4
#define MSG2SSP_AP_SET_MAGNETIC_HWOFFSET	0xF5
#define MSG2SSP_AP_GET_MAGNETIC_HWOFFSET	0xF6
#define MSG2SSP_AP_SENSOR_GESTURE_CURRENT	0xF7
#define MSG2SSP_AP_GET_THERM			0xF8
#define MSG2SSP_AP_GET_BIG_DATA			0xF9
#define MSG2SSP_AP_SET_BIG_DATA			0xFA
#define MSG2SSP_AP_START_BIG_DATA		0xFB
#define MSG2SSP_AP_SET_MAGNETIC_STATIC_MATRIX	0xFD
#define MSG2SSP_AP_SENSOR_TILT			0xEA
#define MSG2SSP_AP_MCU_SET_TIME			0xFE
#define MSG2SSP_AP_MCU_GET_TIME			0xFF
#define MSG2SSP_AP_MOBEAM_DATA_SET		0x31
#define MSG2SSP_AP_MOBEAM_REGISTER_SET		0x32
#define MSG2SSP_AP_MOBEAM_COUNT_SET		0x33
#define MSG2SSP_AP_MOBEAM_START			0x34
#define MSG2SSP_AP_MOBEAM_STOP			0x35
#define MSG2SSP_AP_GEOMAG_LOGGING		0x36
#define MSG2SSP_AP_SENSOR_LPF			0x37
#define MSG2SSP_AP_IRDATA_SEND			0x38
#define MSG2SSP_AP_IRDATA_SEND_RESULT		0x39
#define MSG2SSP_AP_PROX_GET_TRIM		0x40

#define SUCCESS					1
#define FAIL					0
#define ERROR					-1

#define ssp_dbg(format, ...) do { \
	pr_debug("[SSP] " format "\n", ##__VA_ARGS__); \
	} while (0)

#define ssp_info(format, ...) do { \
	pr_info("[SSP] " format "\n", ##__VA_ARGS__); \
	} while (0)

#define ssp_err(format, ...) do { \
	pr_err("[SSP] " format "\n", ##__VA_ARGS__); \
	} while (0)

#define ssp_dbgf(format, ...) do { \
	pr_debug("[SSP] %s: " format "\n", __func__, ##__VA_ARGS__); \
	} while (0)

#define ssp_infof(format, ...) do { \
	pr_info("[SSP] %s: " format "\n", __func__, ##__VA_ARGS__); \
	} while (0)

#define ssp_errf(format, ...) do { \
	pr_err("[SSP] %s: " format "\n", __func__, ##__VA_ARGS__); \
	} while (0)


/* SSP Binary Type */
enum {
	KERNEL_BINARY = 0,
	KERNEL_CRASHED_BINARY,
	UMS_BINARY,
};

/*
 * SENSOR_DELAY_SET_STATE
 * Check delay set to avoid sending ADD instruction twice
 */
enum {
	INITIALIZATION_STATE = 0,
	NO_SENSOR_STATE,
	ADD_SENSOR_STATE,
	RUNNING_SENSOR_STATE,
};

/* Firmware download STATE */
enum {
	FW_DL_STATE_FAIL = -1,
	FW_DL_STATE_NONE = 0,
	FW_DL_STATE_NEED_TO_SCHEDULE,
	FW_DL_STATE_SCHEDULED,
	FW_DL_STATE_DOWNLOADING,
	FW_DL_STATE_SYNC,
	FW_DL_STATE_DONE,
};

/* SSP_INSTRUCTION_CMD */
enum {
	REMOVE_SENSOR = 0,
	ADD_SENSOR,
	CHANGE_DELAY,
	GO_SLEEP,
	REMOVE_LIBRARY,
	ADD_LIBRARY,
};

/* SENSOR_TYPE */
enum {
	ACCELEROMETER_SENSOR = 0,
	GYROSCOPE_SENSOR,
	GEOMAGNETIC_UNCALIB_SENSOR,
	GEOMAGNETIC_RAW,
	GEOMAGNETIC_SENSOR,
	PRESSURE_SENSOR,
	GESTURE_SENSOR,
	PROXIMITY_SENSOR,
	TEMPERATURE_HUMIDITY_SENSOR,
	LIGHT_SENSOR,
	PROXIMITY_RAW,
	ORIENTATION_SENSOR,
	STEP_DETECTOR = 12,
	SIG_MOTION_SENSOR,
	GYRO_UNCALIB_SENSOR,
	GAME_ROTATION_VECTOR = 15,
	ROTATION_VECTOR,
	STEP_COUNTER,
	BIO_HRM_RAW,
	BIO_HRM_RAW_FAC,
	BIO_HRM_LIB,
	SHAKE_CAM = 22,
#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
	LIGHT_IR_SENSOR = 24,
#endif
	INTERRUPT_GYRO_SENSOR,
	META_SENSOR,
	SENSOR_MAX,
};

enum {
	AP2HUB_READ = 0,
	AP2HUB_WRITE,
	HUB2AP_WRITE,
	AP2HUB_READY,
	AP2HUB_RETURN
};

enum {
	BIG_TYPE_DUMP = 0,
	BIG_TYPE_READ_LIB,
	BIG_TYPE_MAX,
};

extern struct class *sensors_event_class;

struct sensor_value {
	union {
		struct { /* accel, gyro, mag */
			s16 x;
			s16 y;
			s16 z;
			u32 gyro_dps;
		} __attribute__((__packed__));
		struct { /*calibrated mag, gyro*/
			s16 cal_x;
			s16 cal_y;
			s16 cal_z;
			u8 accuracy;
		} __attribute__((__packed__));
		struct { /*uncalibrated mag, gyro*/
			s16 uncal_x;
			s16 uncal_y;
			s16 uncal_z;
			s16 offset_x;
			s16 offset_y;
			s16 offset_z;
		} __attribute__((__packed__));
		struct { /* rotation vector */
			s32 quat_a;
			s32 quat_b;
			s32 quat_c;
			s32 quat_d;
			u8 acc_rot;
		} __attribute__((__packed__));
		struct { /* light */
			u16 r;
			u16 g;
			u16 b;
			u16 w;
			u8 a_time;
			u8 a_gain;
		} __attribute__((__packed__));
#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
		struct { /* light ir */
			u16 irdata;
			u16 ir_r;
			u16 ir_g;
			u16 ir_b;
			u16 ir_w;
			u8 ir_a_time;
			u8 ir_a_gain;
		} __attribute__((__packed__));
#endif
		struct { /* pressure */
			s32 pressure;
			s16 temperature;
			s32 pressure_cal;
			s32 pressure_sealevel;
		} __attribute__((__packed__));
		struct { /* step detector */
			u8 step_det;
		};
		struct { /* step counter */
			u32 step_diff;
			u64 step_total;
		} __attribute__((__packed__));
		struct { /* proximity */
			u8 prox;
			u16 prox_ex;
		} __attribute__((__packed__));
		struct { /* proximity raw */
			u16 prox_raw[4];
		};
		struct { /* significant motion */
			u8 sig_motion;
		};
		struct meta_data_event { /* meta data */
			s32 what;
			s32 sensor;
		} __attribute__((__packed__)) meta_data;
		u8 data[20];
	};
	u64 timestamp;
} __attribute__((__packed__));

struct calibraion_data {
	s16 x;
	s16 y;
	s16 z;
};

struct ssp_msg {
	u8 cmd;
	u16 length;
	u16 options;
	u32 data;

	struct list_head list;
	struct completion *done;
	char *buffer;
	u8 free_buffer;
	bool *dead_hook;
	bool dead;
} __attribute__((__packed__));

enum {
	BATCH_MODE_NONE = 0,
	BATCH_MODE_RUN,
};

struct ssp_time_diff {
	u16 batch_count;
	u16 batch_mode;
	u64 time_diff;
	u64 irq_diff;
	u16 batch_count_fixed;
};

struct ssp_data {
	char name[SENSOR_MAX][SENSOR_NAME_MAX_LEN];
	bool enable[SENSOR_MAX];
	int data_len[SENSOR_MAX];
	int report_len[SENSOR_MAX];
	struct sensor_value buf[SENSOR_MAX];
	struct iio_dev *indio_devs[SENSOR_MAX];
	struct iio_chan_spec indio_channels[SENSOR_MAX];
	struct device *devices[SENSOR_MAX];

	struct spi_device *spi;
	struct wake_lock ssp_wake_lock;
	struct timer_list debug_timer;
	struct workqueue_struct *debug_wq;
	struct work_struct work_debug;
	struct calibraion_data accelcal;
	struct calibraion_data gyrocal;
	struct device *mcu_device;
	struct device *prs_device;
	struct device *prox_device;
	struct device *light_device;
	struct device *ges_device;
	struct device *irled_device;
	struct device *mobeam_device;
	struct delayed_work work_firmware;
	struct delayed_work work_refresh;
	struct miscdevice shtc1_device;
	struct miscdevice batch_io_device;

	bool bSspShutdown;
	bool bAccelAlert;
	bool bProximityRawEnabled;
	bool bGeomagneticRawEnabled;
	bool bBarcodeEnabled;
	bool bMcuDumpMode;
	bool bBinaryChashed;
	bool bProbeIsDone;
	bool bDumping;
	bool bTimeSyncing;

	unsigned int uProxCanc;
	unsigned int uCrosstalk;
	unsigned int uProxCalResult;
	unsigned int uProxHiThresh;
	unsigned int uProxLoThresh;
	unsigned int uProxHiThresh_default;
	unsigned int uProxLoThresh_default;
	unsigned int uIr_Current;
	unsigned char uFuseRomData[3];
	unsigned char uMagCntlRegData;
	char *pchLibraryBuf;
	char chLcdLdi[2];
	int iIrq;
	int iLibraryLength;
	int aiCheckStatus[SENSOR_MAX];

	unsigned int uComFailCnt;
	unsigned int uResetCnt;
	unsigned int uTimeOutCnt;
	unsigned int uIrqCnt;
	unsigned int uDumpCnt;

	unsigned int uSensorState;
	unsigned int uCurFirmRev;
	unsigned int uFactoryProxAvg[4];
	char uLastResumeState;
	char uLastAPState;

	atomic_t aSensorEnable;
	int64_t adDelayBuf[SENSOR_MAX];
	s32 batchLatencyBuf[SENSOR_MAX];
	s8 batchOptBuf[SENSOR_MAX];
	u64 lastTimestamp[SENSOR_MAX];
	bool reportedData[SENSOR_MAX];

	struct ssp_sensorhub_data *hub_data;
	int accel_position;
	int mag_position;
	int fw_dl_state;
	unsigned char pdc_matrix[PDC_SIZE];
	struct mutex comm_mutex;
	struct mutex pending_mutex;

	int mcu_int1;
	int mcu_int2;
	int ap_int;
	int rst;

	struct list_head pending_list;
	void (*ssp_big_task[BIG_TYPE_MAX])(struct work_struct *);
	u64 timestamp;

	struct file *realtime_dump_file;
	int total_dump_size;
#ifdef CONFIG_SENSORS_MULTIPLE_GLASS_TYPE
      u32 glass_type;
#endif
#ifdef CONFIG_SENSORS_SSP_IRDATA_FOR_CAMERA
	int light_ir_log_cnt;
#endif
	int acc_type;
	int pressure_type;
	atomic_t int_gyro_enable;
};

struct ssp_big {
	struct ssp_data *data;
	struct work_struct work;
	u32 length;
	u32 addr;
};

#endif
