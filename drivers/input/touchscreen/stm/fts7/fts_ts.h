#ifndef _LINUX_FTS_TS_H_
#define _LINUX_FTS_TS_H_

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/i2c/fts.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <linux/sec_debug.h>
#endif
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({											\
	dev_dbg(dev, fmt, ## __VA_ARGS__);		\
	if (mode) {								\
		char msg_buff[10];						\
		snprintf(msg_buff, sizeof(msg_buff), "%s", dev_name(dev));	\
		sec_debug_tsp_log_msg(msg_buff, fmt, ## __VA_ARGS__);	\
	}										\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({											\
	dev_info(dev, fmt, ## __VA_ARGS__);		\
	if (mode) {								\
		char msg_buff[10];						\
		snprintf(msg_buff, sizeof(msg_buff), "%s", dev_name(dev));	\
		sec_debug_tsp_log_msg(msg_buff, fmt, ## __VA_ARGS__);	\
	}										\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({											\
	if (mode) {								\
		char msg_buff[10];						\
		snprintf(msg_buff, sizeof(msg_buff), "%s", dev_name(dev));	\
		sec_debug_tsp_log_msg(msg_buff, fmt, ## __VA_ARGS__);	\
	}										\
	dev_err(dev, fmt, ## __VA_ARGS__);		\
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#define USE_OPEN_CLOSE

#ifdef USE_OPEN_DWORK
#define TOUCH_OPEN_DWORK_TIME 10
#endif

#define FTS_SUPPORT_PARTIAL_DOWNLOAD

#undef FTS_SUPPROT_MULTIMEDIA
#undef TSP_RUN_AUTOTUNE_DEFAULT


/*
#define FTS_SUPPORT_STRINGLIB
*/
#define FIRMWARE_IC					"fts_ic"

#define FTS_MAX_FW_PATH	64

#define FTS_TS_DRV_NAME			"fts_touch"
#define FTS_TS_DRV_VERSION		"0132"

#define STM_DEVICE_NAME	"STM"

#define STM_VER4		0x04
#define STM_VER5		0x05
#define STM_VER7		0x07

/* stm7  */
#define FTS_ID0							0x36
#define FTS_ID1							0x70
/*  stm5
#define FTS_ID0							0x39
#define FTS_ID1							0x80
*/
#define FTS_ID2							0x6C

#define FTS_DIGITAL_REV_1	0x01
#define FTS_DIGITAL_REV_2	0x02
#define FTS_DIGITAL_REV_3	0x03
#define FTS_FIFO_MAX					32
#define FTS_EVENT_SIZE				8

#define PRESSURE_MIN					0
#define PRESSURE_MAX				127
#define P70_PATCH_ADDR_START	0x00420000
#define FINGER_MAX						10
#define AREA_MIN							PRESSURE_MIN
#define AREA_MAX						PRESSURE_MAX

#define EVENTID_NO_EVENT						0x00
#define EVENTID_ENTER_POINTER				0x03
#define EVENTID_LEAVE_POINTER				0x04
#define EVENTID_MOTION_POINTER				0x05
#define EVENTID_HOVER_ENTER_POINTER			0x07
#define EVENTID_HOVER_LEAVE_POINTER			0x08
#define EVENTID_HOVER_MOTION_POINTER		0x09
#define EVENTID_PROXIMITY_IN					0x0B
#define EVENTID_PROXIMITY_OUT				0x0C
#define EVENTID_MSKEY						0x0E
#define EVENTID_ERROR						0x0F
#define EVENTID_CONTROLLER_READY			0x10
#define EVENTID_SLEEPOUT_CONTROLLER_READY	0x11
#define EVENTID_RESULT_READ_REGISTER		0x12
#define	EVENTID_STATUS_REQUEST_COMP			0x13
#define EVENTID_STATUS_EVENT					0x16
#define EVENTID_INTERNAL_RELEASE_INFO		0x19
#define EVENTID_EXTERNAL_RELEASE_INFO		0x1A

#define EVENTID_FROM_STRING					0x80
#define EVENTID_GESTURE						0x20

#define EVENTID_SIDE_SCROLL				0x40
#define EVENTID_SIDE_TOUCH_DEBUG			0xDB	/* side touch event-id for debug, remove after f/w fixed */
#define EVENTID_SIDE_TOUCH					0x0B
#define EVENTID_GESTURE_WAKEUP				0xE2

#define STATUS_EVENT_MUTUAL_AUTOTUNE_DONE	0x01
#define STATUS_EVENT_SELF_AUTOTUNE_DONE	0x42
#define STATUS_EVENT_SELF_AUTOTUNE_DONE_D3	0x02
#define STATUS_EVENT_WATER_SELF_AUTOTUNE_DONE	0x4E
#define STATUS_EVENT_FORCE_CAL_DONE	0x15
#define STATUS_EVENT_FORCE_CAL_DONE_D3	0x06
#ifdef FTS_SUPPORT_WATER_MODE
#define STATUS_EVENT_WATER_SELF_DONE 0x17
#endif
#define STATUS_EVENT_FLASH_WRITE_CONFIG	0x03
#define STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE	0x04
#define STATUS_EVENT_FORCE_CAL_MUTUAL_SELF	0x05
#define STATUS_EVENT_FORCE_CAL_MUTUAL	0x15
#define STATUS_EVENT_FORCE_CAL_SELF	0x06
#define STATUS_EVENT_WATERMODE_ON	0x07
#define STATUS_EVENT_WATERMODE_OFF	0x08
#define STATUS_EVENT_RTUNE_MUTUAL	0x09
#define STATUS_EVENT_RTUNE_SELF	0x0A
#define STATUS_EVENT_PANEL_TEST_RESULT	0x0B
#define STATUS_EVENT_GLOVE_MODE	0x0C
#define STATUS_EVENT_RAW_DATA_READY	0x0D
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
#define STATUS_EVENT_PURE_AUTOTUNE_FLAG_WRITE_FINISH	0x10
#define STATUS_EVENT_PURE_AUTOTUNE_FLAG_ERASE_FINISH	0x11
#endif 
#define STATUS_EVENT_MUTUAL_CAL_FRAME_CHECK	0xC1
#define STATUS_EVENT_SELF_CAL_FRAME_CHECK	0xC2
#define STATUS_EVENT_CHARGER_CONNECTED	0xCC
#define STATUS_EVENT_CHARGER_DISCONNECTED	0xCD

#define INT_ENABLE_D3					0x48
#define INT_DISABLE_D3					0x08

#define INT_ENABLE						0x41
#define INT_DISABLE						0x00

#define READ_STATUS					0x84
#define READ_ONE_EVENT				0x85
#define READ_ALL_EVENT				0x86

#define SLEEPIN						0x90
#define SLEEPOUT					0x91
#define SENSEOFF					0x92
#define SENSEON						0x93
#define FTS_CMD_HOVER_OFF           0x94
#define FTS_CMD_HOVER_ON            0x95

#define FTS_CMD_MSKEY_AUTOTUNE		0x96
#define FTS_CMD_TRIM_LOW_POWER_OSCILLATOR		0x97

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
#define FTS_CMD_FORCE_AUTOTUNE		0x98
#endif


#define FTS_CMD_KEY_SENSE_OFF		0x9A
#define FTS_CMD_KEY_SENSE_ON		0x9B
#define FTS_CMD_SET_FAST_GLOVE_MODE	0x9D

#define FTS_CMD_MSHOVER_OFF         0x9E
#define FTS_CMD_MSHOVER_ON          0x9F
#define FTS_CMD_SET_NOR_GLOVE_MODE	0x9F

#define FLUSHBUFFER					0xA1
#define FORCECALIBRATION			0xA2
#define CX_TUNNING					0xA3
#define SELF_AUTO_TUNE				0xA4

#define FTS_CMD_CHARGER_PLUGGED     0xA8
#define FTS_CMD_CHARGER_UNPLUGGED	0xAB

#define FTS_CMD_RELEASEINFO     0xAA
#define FTS_CMD_STYLUS_OFF          0xAB
#define FTS_CMD_STYLUS_ON           0xAC
#define FTS_CMD_LOWPOWER_MODE		0xAD

#define FTS_CMS_ENABLE_FEATURE		0xC1
#define FTS_CMS_DISABLE_FEATURE		0xC2

#define FTS_CMD_WRITE_PRAM          0xF0
#define FTS_CMD_BURN_PROG_FLASH     0xF2
#define FTS_CMD_ERASE_PROG_FLASH    0xF3
#define FTS_CMD_READ_FLASH_STAT     0xF4
#define FTS_CMD_UNLOCK_FLASH        0xF7
#define FTS_CMD_SAVE_FWCONFIG       0xFB
#define FTS_CMD_SAVE_CX_TUNING      0xFC

#define FTS_CMD_FAST_SCAN			0x01
#define FTS_CMD_SLOW_SCAN			0x02
#define FTS_CMD_USLOW_SCAN			0x03

#define REPORT_RATE_90HZ				0
#define REPORT_RATE_60HZ				1
#define REPORT_RATE_30HZ				2

#define FTS_CMD_STRING_ACCESS	0x3000
#define FTS_CMD_NOTIFY				0xC0

#define FTS_RETRY_COUNT					10

#define FTS_STRING_EVENT_AOD_TRIGGER		(1 << 0)
#define FTS_STRING_EVENT_ONEWORDCALL_TRIGGER		(1 << 1)
#define FTS_STRING_EVENT_WATCH_STATUS		(1 << 2)
#define FTS_STRING_EVENT_FAST_ACCESS		(1 << 3)
#define FTS_STRING_EVENT_DIRECT_INDICATOR	(1 << 3) | (1 << 2)
#define FTS_STRING_EVENT_SPAY			(1 << 4)
#define FTS_STRING_EVENT_SPAY1			(1 << 5)
#define FTS_STRING_EVENT_SPAY2			(1 << 4) | (1 << 5)
#define FTS_STRING_EVENT_EDGE_SWIPE_RIGHT			(1 << 6)
#define FTS_STRING_EVENT_EDGE_SWIPE_LEFT			(1 << 7)

#define FTS_SIDEGESTURE_EVENT_SINGLE_STROKE		0xE0
#define FTS_SIDEGESTURE_EVENT_DOUBLE_STROKE	0xE1
#define FTS_SIDEGESTURE_EVENT_INNER_STROKE	0xE3

#define FTS_SIDETOUCH_EVENT_LONG_PRESS			0xBB
#define FTS_SIDETOUCH_EVENT_REBOOT_BY_ESD		0xED

#define FTS_ENABLE		1
#define FTS_DISABLE		0

#define FTS_MODE_SCRUB					(1 << 0)
#define FTS_MODE_SPAY					(1 << 1)
#define FTS_MODE_QUICK_APP_ACCESS			(1 << 2)
#define FTS_MODE_EDGE_SWIPE				(1 << 2)
#define FTS_MODE_DIRECT_INDICATOR			(1 << 3)

#define TSP_BUF_SIZE 3000
#define CMD_STR_LEN 256
#define CMD_RESULT_STR_LEN 3000
#define CMD_PARAM_NUM 8

#define FTS_LOWP_FLAG_2ND_SCREEN		(1 << 1)
#define FTS_LOWP_FLAG_BLACK_UI			(1 << 2)
#define FTS_LOWP_FLAG_QUICK_APP_ACCESS			(1 << 3)
#define FTS_LOWP_FLAG_DIRECT_INDICATOR			(1 << 4)
#define FTS_LOWP_FLAG_SPAY				(1 << 5)
#define FTS_LOWP_FLAG_TEMP_CMD				(1 << 6)
#define FTS_LOWP_FLAG_GESTURE_WAKEUP		(1 << 7)

#define EDGEWAKE_RIGHT	0
#define EDGEWAKE_LEFT	1

/* refer to lcd driver to support TB UB */
#define S6E3HF2_WQXGA_ID1 0x404013
#define S6E3HF2_WQXGA_ID2 0x404014

/* refer to lcd driver to support ZEROF module at NOBLE/ZEN */
#define S6E3HA2_WQXGA_ZERO	0x410013
#define S6E3HA2_WQXGA_ZERO1	0x400013
#define S6E3HA2_WQXGA_ZERO2	0x410012

// ZEN Temp for UB VZW
#define ZEN_UB_VZW	0x000010

/* LCD ID  0x ID1 ID2 ID3 */
#define LCD_ID2_MODEL_MASK	0x003000	// ID2 - 00110000
#define LCD_ID3_DIC_MASK	0x0000C0	// ID3 - 11000000

#define MODEL_ZERO	0x000000
#define MODEL_HEROPLUS	0x001000
#define MODEL_HEROEDGE	0x002000
#define MODEL_NOBLE	0x001000
#define MODEL_ZEN	0x002000

#define S6E3HF2_ZERO	0x000000
#define S6E3HA3_NOBLE	0x000040
#define S6E3HA3_ZEN 	0x000080

#define SMARTCOVER_COVER	// for  Various Cover
#ifdef SMARTCOVER_COVER
#define MAX_W 16		// zero is 16 x 28
#define MAX_H 32		// byte size to IC
#define MAX_TX MAX_W
#define MAX_BYTE MAX_H
#endif

#ifdef FTS_SUPPROT_MULTIMEDIA
#define MAX_BRUSH_STEP 230
#endif

enum fts_error_return {
	FTS_NOT_ERROR = 0,
	FTS_ERROR_INVALID_CHIP_ID,
	FTS_ERROR_INVALID_CHIP_VERSION_ID,
	FTS_ERROR_INVALID_SW_VERSION,
	FTS_ERROR_EVENT_ID,
	FTS_ERROR_TIMEOUT,
	FTS_ERROR_FW_UPDATE_FAIL,
};
#define RAW_MAX	3750
/**
 * struct fts_finger - Represents fingers.
 * @ state: finger status (Event ID).
 * @ mcount: moving counter for debug.
 */
struct fts_finger {
	unsigned char state;
	unsigned short mcount;
	int lx;
	int ly;
};

enum tsp_power_mode {
	FTS_POWER_STATE_ACTIVE = 0,
	FTS_POWER_STATE_LOWPOWER,
	FTS_POWER_STATE_POWERDOWN,
	FTS_POWER_STATE_DEEPSLEEP,
};

enum fts_cover_id {
	FTS_FLIP_WALLET = 0,
	FTS_VIEW_COVER,
	FTS_COVER_NOTHING1,
	FTS_VIEW_WIRELESS,
	FTS_COVER_NOTHING2,
	FTS_CHARGER_COVER,
	FTS_VIEW_WALLET,
	FTS_LED_COVER,
	FTS_CLEAR_FLIP_COVER,
	FTS_QWERTY_KEYBOARD_EUR,
	FTS_QWERTY_KEYBOARD_KOR,
	FTS_MONTBLANC_COVER = 100,
};

enum fts_customer_feature {
	FTS_FEATURE_ORIENTATION_GESTURE = 1,
	FTS_FEATURE_STYLUS,
	FTS_FEATURE_GESTURE_WAKEUP,
	FTS_FEATURE_SIDE_GUSTURE,
	FTS_FEATURE_COVER_GLASS,
	FTS_FEATURE_COVER_WALLET,
	FTS_FEATURE_COVER_LED,
	FTS_FEATURE_COVER_CLEAR_FLIP,
	FTS_FEATURE_DUAL_SIDE_GUSTURE,
	FTS_FEATURE_CUSTOM_COVER_GLASS_ON,
};

#ifdef FTS_SUPPROT_MULTIMEDIA
enum BRUSH_MODE {
	Brush = 0,
	Velocity,
};
#endif

/* ----------------------------------------
 * write 0xE4 [ 11 | 10 | 01 | 00 ]
 * MSB <-------------------> LSB
 * read 0xE4
 * mapping sequnce : LSB -> MSB
 * struct sec_ts_test_result {
 * * assy : front + OCTA assay
 * * module : only OCTA
 *	 union {
 *		 struct {
 *			 u8 assy_count:2;		-> 00
 *			 u8 assy_result:2;		-> 01
 *			 u8 module_count:2;	-> 10
 *			 u8 module_result:2;	-> 11
 *		 } __attribute__ ((packed));
 *		 unsigned char data[1];
 *	 };
 *};
 * ---------------------------------------- */
struct fts_ts_test_result {
	union {
		struct {
			u8 assy_count:2;
			u8 assy_result:2;
			u8 module_count:2;
			u8 module_result:2;
		} __attribute__ ((packed));
		unsigned char data[1];
	};
};

#define TEST_OCTA_MODULE	1
#define TEST_OCTA_ASSAY		2

#define TEST_OCTA_NONE		0
#define TEST_OCTA_FAIL		1
#define TEST_OCTA_PASS		2


struct fts_ts_info {
	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct timer_list timer_charger;
	struct timer_list timer_firmware;
	struct work_struct work;

	int irq;
	int irq_type;
	bool irq_enabled;
	struct fts_i2c_platform_data *board;
	void (*register_cb) (void *);
	struct fts_callbacks callbacks;
	struct mutex lock;
	bool tsp_enabled;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
#ifdef SEC_TSP_FACTORY_TEST
	struct device *fac_dev_ts;
	struct list_head cmd_list_head;
	u8 cmd_state;
	char cmd[CMD_STR_LEN];
	int cmd_param[CMD_PARAM_NUM];
	char cmd_result[CMD_RESULT_STR_LEN];
	int cmd_buffer_size;
	struct mutex cmd_lock;
	bool cmd_is_running;
	int SenseChannelLength;
	int ForceChannelLength;
	short *pFrame;
	unsigned char *cx_data;
	struct delayed_work cover_cmd_work;
	int delayed_cmd_param[2];
#endif
	struct fts_ts_test_result test_result;

	bool hover_ready;
	bool hover_enabled;
	bool mshover_enabled;
	bool fast_mshover_enabled;
	bool flip_enable;
	bool run_autotune;
	bool mainscr_disable;
	unsigned int cover_type;

	unsigned char lowpower_flag;
	bool lowpower_mode;
	bool deepsleep_mode;
	bool wirelesscharger_mode;
	int fts_power_state;
	int wakeful_edge_side;
#ifdef FTS_SUPPORT_STRINGLIB
	unsigned char fts_mode;
#endif
#ifdef FTS_SUPPORT_TA_MODE
	bool TA_Pluged;
#endif

#ifdef FTS_SUPPORT_2NDSCREEN
	u8 SIDE_Flag;
	u8 previous_SIDE_value;
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	unsigned char tsp_keystatus;
	int touchkey_threshold;
	struct device *fac_dev_tk;
	bool tsk_led_enabled;
#endif

	int digital_rev;
	int touch_count;
	struct fts_finger finger[FINGER_MAX];

	int touch_mode;
	int retry_hover_enable_after_wakeup;

	int ic_product_id;			/* product id of ic */
	int ic_revision_of_ic;		/* revision of reading from IC */
	int fw_version_of_ic;		/* firmware version of IC */
	int ic_revision_of_bin;		/* revision of reading from binary */
	int fw_version_of_bin;		/* firmware version of binary */
	int config_version_of_ic;		/* Config release data from IC */
	int config_version_of_bin;	/* Config release data from IC */
	unsigned short fw_main_version_of_ic;	/* firmware main version of IC */
	unsigned short fw_main_version_of_bin;	/* firmware main version of binary */
	int panel_revision;			/* Octa panel revision */
	int tspid_val;
	int tspid2_val;
	int pat;
	int stm_ver;

#ifdef USE_OPEN_DWORK
	struct delayed_work open_work;
#endif
	struct delayed_work work_read_nv;

#ifdef FTS_SUPPORT_NOISE_PARAM
	struct fts_noise_param noise_param;
	int (*fts_get_noise_param_address) (struct fts_ts_info *info);
#endif
	unsigned int delay_time;
	unsigned int debug_string;
	struct delayed_work reset_work;
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
	struct delayed_work debug_work;
	bool rawdata_read_lock;
#endif

	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

	struct mutex i2c_mutex;
	struct mutex device_mutex;
	bool touch_stopped;
	bool reinit_done;

	unsigned char data[FTS_EVENT_SIZE * FTS_FIFO_MAX];
	unsigned char ddi_type;

	const char *firmware_name;

#ifdef SMARTCOVER_COVER
	bool smart_cover[MAX_BYTE][MAX_TX];
	bool changed_table[MAX_TX][MAX_BYTE];
	u8 send_table[MAX_TX][4];
#endif
#ifdef FTS_SUPPROT_MULTIMEDIA
	bool brush_enable;
	bool velocity_enable;
	int brush_size;
#endif

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
	unsigned char o_afe_ver;
	unsigned char afe_ver;
#endif

	int (*stop_device) (struct fts_ts_info * info);
	int (*start_device) (struct fts_ts_info * info);

	int (*fts_write_reg)(struct fts_ts_info *info, unsigned char *reg, unsigned short num_com);
	int (*fts_read_reg)(struct fts_ts_info *info, unsigned char *reg, int cnum, unsigned char *buf, int num);
	void (*fts_systemreset)(struct fts_ts_info *info);
	int (*fts_wait_for_ready)(struct fts_ts_info *info);
	void (*fts_command)(struct fts_ts_info *info, unsigned char cmd);
	void (*fts_enable_feature)(struct fts_ts_info *info, unsigned char cmd, int enable);
	int (*fts_get_version_info)(struct fts_ts_info *info);
#ifdef FTS_SUPPORT_STRINGLIB
	int (*fts_read_from_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
	int (*fts_write_to_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
#endif
};

int fts_fw_update_on_probe(struct fts_ts_info *info);
int fts_fw_update_on_hidden_menu(struct fts_ts_info *info, int update_type);
void fts_fw_init(struct fts_ts_info *info);
void fts_execute_autotune(struct fts_ts_info *info);
int fts_fw_wait_for_event(struct fts_ts_info *info, unsigned char eid);
int fts_fw_wait_for_specific_event(struct fts_ts_info *info, unsigned char eid0, unsigned char eid1, unsigned char eid2);
void fts_interrupt_set(struct fts_ts_info *info, int enable);
int fts_irq_enable(struct fts_ts_info *info, bool enable);
int fts_fw_wait_for_event_D3(struct fts_ts_info *info, unsigned char eid0, unsigned char eid1);
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
bool get_PureAutotune_status(struct fts_ts_info *info);
#endif
int fts_get_tsp_test_result(struct fts_ts_info *info);

#endif				//_LINUX_FTS_TS_H_
