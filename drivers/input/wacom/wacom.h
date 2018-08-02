#ifndef _LINUX_WACOM_H
#define _LINUX_WACOM_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/firmware.h>
#include <linux/pm_qos.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/i2c/wacom_i2c.h>

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

#define USE_OPEN_CLOSE

#ifdef CONFIG_EPEN_WACOM_W9018
#define WACOM_FW_SIZE 131072
#elif defined(CONFIG_EPEN_WACOM_W9014)
#define WACOM_FW_SIZE 131092
#elif defined(CONFIG_EPEN_WACOM_W9012)
#define WACOM_FW_SIZE 131072
#elif defined(CONFIG_EPEN_WACOM_G9PM)
#define WACOM_FW_SIZE 61440
#elif defined(CONFIG_EPEN_WACOM_G9PLL) \
	|| defined(CONFIG_EPEN_WACOM_G10PM)
#define WACOM_FW_SIZE 65535
#else
#define WACOM_FW_SIZE 32768
#endif

/* SURVEY MODE is LPM mode : Only detect grage(pdct) & aop */
#define WACOM_USE_SURVEY_MODE

#define WACOM_NORMAL_MODE	false
#define WACOM_BOOT_MODE		true

/*Wacom Command*/
#define COM_COORD_NUM	12
#define COM_RESERVED_NUM	2
#define COM_QUERY_NUM	14
#define COM_QUERY_POS	(COM_COORD_NUM+COM_RESERVED_NUM)
#define COM_QUERY_BUFFER (COM_QUERY_POS+COM_QUERY_NUM)
#define COM_QUERY_RETRY 3

#define COM_SAMPLERATE_STOP 0x30
#define COM_SAMPLERATE_40  0x33
#define COM_SAMPLERATE_80  0x32
#define COM_SAMPLERATE_133 0x31
#define COM_SAMPLERATE_START COM_SAMPLERATE_133
#define COM_SURVEYSCAN     0x2B
#define COM_QUERY          0x2A
#define COM_SURVEYEXIT     0x2D
#define COM_SURVEYREQUESTDATA	0x2E
#define COM_SURVEY_TARGET_GARAGE_AOP	0x3A
#define COM_SURVEY_TARGET_GARAGEONLY	0x3B
#define COM_FLASH          0xff
#define COM_CHECKSUM       0x63

#define COM_NORMAL_SENSE_MODE	0xDB
#define COM_LOW_SENSE_MODE	0xDC
#define COM_LOW_SENSE_MODE2		0xE7

#define COM_REQUEST_SENSE_MODE	0xDD
#define COM_REQUESTSCANMODE		0xE6

/* data format for SURVEY mode*/
#define EPEN_SURVEY_NONE		0x000
#define EPEN_SURVEY_AOP			0x100
#define EPEN_SURVEY_GARAGE		0x200
#define EPEN_SURVEY_GARAGE_IN	0x1
#define EPEN_SURVEY_GARAGE_OUT	0x0


/*--------------------------------------------------*/
// function setting by user or default
// wac_i2c->function_set
//  7.Garage | 6.Power Save | 5.AOP | 4.reserved || 3.reserved | 2. reserved | 1. reserved | 0. reserved |
//
// 6. Power Save - not imported
//
//
// event 
//wac_i2c->function_result
// 7. ~ 4. reserved || 3. Garage | 2. Power Save | 1. AOP | 0. Pen In/Out |
//
// 0. Pen In/Out ( pen_insert )
// ( 0: IN / 1: OUT )
/*--------------------------------------------------*/
#define EPEN_EVENT_PEN_INOUT		(0x1<<0)
#define EPEN_EVENT_AOP				(0x1<<1)
#define EPEN_EVENT_POWRSAVE			(0x1<<2)
#define EPEN_EVENT_GARAGE			(0x1<<3)

#define EPEN_SETMODE_PEN_INOUT		(0x1<<4)
#define EPEN_SETMODE_AOP			(0x1<<5)
#define EPEN_SETMODE_POWRSAVE		(0x1<<6)
#define EPEN_SETMODE_GARAGE			(0x1<<7)


#define EPEN_SURVEY_MODE_GARAGE_AOP		0x0
#define EPEN_SURVEY_MODE_GARAGE_ONLY	0x1

/* query data format */
#define EPEN_REG_HEADER 0x00
#define EPEN_REG_X1 0x01
#define EPEN_REG_X2 0x02
#define EPEN_REG_Y1 0x03
#define EPEN_REG_Y2 0x04
#define EPEN_REG_PRESSURE1 0x05
#define EPEN_REG_PRESSURE2 0x06
#define EPEN_REG_FWVER1 0X07
#define EPEN_REG_FWVER2 0X08
#define EPEN_REG_MPUVER 0X09
#define EPEN_REG_BLVER 0X0A
#define EPEN_REG_TILT_X 0X0B
#define EPEN_REG_TILT_Y 0X0C
#define EPEN_REG_HEIGHT 0X0D

/*Information for input_dev*/
#define EMR 0
#define WACOM_PKGLEN_I2C_EMR 0

/*Special keys*/
#define EPEN_TOOL_PEN		0x220
#define EPEN_TOOL_RUBBER	0x221
#define EPEN_STYLUS			0x22b
#define EPEN_STYLUS2		0x22c

#define WACOM_DELAY_FOR_RST_RISING 200
/* #define INIT_FIRMWARE_FLASH */

#define WACOM_PDCT_WORK_AROUND
#define WACOM_USE_QUERY_DATA

/*PDCT Signal*/
#define PDCT_NOSIGNAL 1
#define PDCT_DETECT_PEN 0

#define WACOM_PRESSURE_MAX 1023

/*Digitizer Type*/
#define EPEN_DTYPE_B660	1
#define EPEN_DTYPE_B713 2
#define EPEN_DTYPE_B746 3
#define EPEN_DTYPE_B804 4
#define EPEN_DTYPE_B878 5

#define EPEN_DTYPE_B887 6
#define EPEN_DTYPE_B911 7
#define EPEN_DTYPE_B934 8
#define EPEN_DTYPE_B968 9

#define WACOM_I2C_MODE_BOOT 1
#define WACOM_I2C_MODE_NORMAL 0

#ifdef WACOM_USE_SURVEY_MODE
#define EPEN_RESUME_DELAY 20
#else
#define EPEN_RESUME_DELAY 180
#endif
#define EPEN_OFF_TIME_LIMIT 10000	// usec

/* softkey block workaround */
#define WACOM_USE_SOFTKEY_BLOCK
#define SOFTKEY_BLOCK_DURATION (HZ / 10)

/* LCD freq sync */
#ifdef CONFIG_WACOM_LCD_FREQ_COMPENSATE
/* NOISE from LDI. read Vsync at wacom firmware. */
#define LCD_FREQ_SYNC
#endif

#ifdef LCD_FREQ_SYNC
#define LCD_FREQ_BOTTOM 60100
#define LCD_FREQ_TOP 60500
#endif

#define LCD_FREQ_SUPPORT_HWID 8

#define HSYNC_COUNTER_UMAGIC	0x96
#define HSYNC_COUNTER_LMAGIC	0xCA
#define WACOM_NOISE_HIGH		0x11
#define WACOM_NOISE_LOW			0x22


/*IRQ TRIGGER TYPE*/
/*#define EPEN_IRQF_TRIGGER_TYPE IRQF_TRIGGER_FALLING*/

#define WACOM_USE_PDATA

#define WACOM_USE_SOFTKEY

/* For Android origin */
#define WACOM_POSX_MAX WACOM_MAX_COORD_Y
#define WACOM_POSY_MAX WACOM_MAX_COORD_X

#define COOR_WORK_AROUND

//#define WACOM_IMPORT_FW_ALGO
/*#define WACOM_USE_OFFSET_TABLE*/
#if 0
#define WACOM_USE_AVERAGING
#define WACOM_USE_AVE_TRANSITION
#define WACOM_USE_BOX_FILTER
#define WACOM_USE_TILT_OFFSET
#endif

/*Box Filter Parameters*/
//#define  X_INC_S1  1500
//#define  X_INC_E1  (WACOM_MAX_COORD_X - 1500)
//#define  Y_INC_S1  1500
//#define  Y_INC_E1  (WACOM_MAX_COORD_Y - 1500)
//
//#define  Y_INC_S2  500
//#define  Y_INC_E2  (WACOM_MAX_COORD_Y - 500)
//#define  Y_INC_S3  1100
//#define  Y_INC_E3  (WACOM_MAX_COORD_Y - 1100)


/*HWID to distinguish Digitizer*/
//#define WACOM_DTYPE_B934_HWID 4
//#define WACOM_DTYPE_B968_HWID 6


//#define WACOM_USE_I2C_GPIO

#ifdef WACOM_USE_PDATA
#undef WACOM_USE_QUERY_DATA
#endif

#define FW_UPDATE_RUNNING 1
#define FW_UPDATE_PASS 2
#define FW_UPDATE_FAIL -1

/*Parameters for wacom own features*/
struct wacom_features {
	char comstat;
	unsigned int fw_version;
	int update_status;
};

#define WACOM_FW_PATH "epen/W9012_T.bin"

enum {
	FW_NONE = 0,
	FW_BUILT_IN,
	FW_HEADER,
	FW_IN_SDCARD,
	FW_EX_SDCARD,
};

/* header ver 1 */
struct fw_image {
	u8 hdr_ver;
	u8 hdr_len;
	u16 fw_ver1;
	u16 fw_ver2;
	u16 fw_ver3;
	u32 fw_len;
	u8 checksum[5];
	u8 alignment_dummy[3];
	u8 data[0];
} __attribute__ ((packed));

struct wacom_i2c {
	struct i2c_client *client;
	struct i2c_client *client_boot;
	struct completion init_done;
	struct input_dev *input_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct mutex lock;
	struct mutex update_lock;
	struct mutex irq_lock;
	struct wake_lock fw_wakelock;
	struct wake_lock det_wakelock;
	struct device	*dev;
	int irq;
	int irq_pdct;
	int pen_pdct;
	int pen_prox;
	int pen_pressed;
	int side_pressed;
	bool fullscan_mode; 
	int tsp_noise_mode;
	int wacom_noise_state;
	int tool;
	int soft_key_pressed[2];
	struct delayed_work pen_insert_dwork;
#ifdef WACOM_USE_SURVEY_MODE
	struct delayed_work pen_survey_resetdwork;
#endif
#ifdef WACOM_IMPORT_FW_ALGO
	bool use_offset_table;
	bool use_aveTransition;
#endif
	bool checksum_result;
	struct wacom_features *wac_feature;
	struct wacom_g5_platform_data *pdata;
	struct wacom_g5_callbacks callbacks;
	struct delayed_work resume_work;
	struct delayed_work fullscan_check_work;
	bool connection_check;
	int  fail_channel;
	int  min_adc_val;
	bool battery_saving_mode;
	bool pwr_flag;
	bool power_enable;
#ifdef WACOM_USE_SURVEY_MODE
	bool survey_mode;
	bool survey_cmd_state;
	bool epen_blocked;
	u8	function_set;
	u8	function_result;
	bool reset_flag;
#endif
	bool boot_mode;
	bool query_status;
	int wcharging_mode;
#ifdef LCD_FREQ_SYNC
	int lcd_freq;
	bool lcd_freq_wait;
	bool use_lcd_freq_sync;
	struct work_struct lcd_freq_work;
	struct delayed_work lcd_freq_done_work;
	struct mutex freq_write_lock;
#endif
#ifdef WACOM_USE_SOFTKEY_BLOCK
	bool block_softkey;
	struct delayed_work softkey_block_work;
#endif
	struct work_struct update_work;
	const struct firmware *firm_data;
	struct workqueue_struct *fw_wq;
	u8 fw_path;
	struct fw_image *fw_img;
	bool do_crc_check;
	struct pinctrl *pinctrl_irq;
	struct pinctrl_state *pin_state_irq;
#ifdef WACOM_USE_I2C_GPIO
	struct pinctrl *pinctrl_i2c;
	struct pinctrl_state *pin_state_i2c;
	struct pinctrl_state *pin_state_gpio;
#endif
};

#endif /* _LINUX_WACOM_H */
