#ifndef _LINUX_FTS_TS_H_
#define _LINUX_FTS_TS_H_

#include <linux/device.h>
#include <linux/input/sec_cmd.h>

//#define FTS_SUPPORT_TOUCH_KEY
#define FTS_SUPPORT_SEC_SWIPE
//#define FTS_SUPPORT_SIDE_GESTURE
//#define FTS_SUPPORT_PRESSURE_SENSOR
//#define FTS_SUPPORT_STRINGLIB
#define USE_OPEN_CLOSE
#define SEC_TSP_FACTORY_TEST
#define PAT_CONTROL


#undef FTS_SUPPORT_HOVER
#undef FTS_SUPPORT_WATER_MODE
#undef FTS_SUPPORT_TA_MODE
#undef FTS_SUPPORT_NOISE_PARAM
#undef USE_OPEN_DWORK

/* temperary undefined */
#undef CONFIG_SECURE_TOUCH

#ifdef USE_OPEN_DWORK
#define TOUCH_OPEN_DWORK_TIME 10
#endif

#define FIRMWARE_IC					"fts_ic"
#define FTS_MAX_FW_PATH					64
#define FTS_TS_DRV_NAME					"fts_touch"
#define FTS_TS_DRV_VERSION				"0132"

/* Location for config version in binary file with header */
#define CONFIG_OFFSET_BIN_D3				0x3F022

#define FTS_SEC_IX1_TX_MULTIPLIER			(4)
#define FTS_SEC_IX1_RX_MULTIPLIER			(2)

#define PRESSURE_SENSOR_COUNT				3

#define FTS_ID0						0x36
#define FTS_ID1						0x70

#define ANALOG_ID_FTS8					0x47
#define ANALOG_ID_FTS9					0x48

#define FTS_FIFO_MAX					32
#define FTS_EVENT_SIZE					8

#define PRESSURE_MIN					0
#define PRESSURE_MAX					127
#define FINGER_MAX					10
#define AREA_MIN					PRESSURE_MIN
#define AREA_MAX					PRESSURE_MAX

#define EVENTID_NO_EVENT				0x00
#define EVENTID_ENTER_POINTER				0x03
#define EVENTID_LEAVE_POINTER				0x04
#define EVENTID_MOTION_POINTER				0x05
#define EVENTID_HOVER_ENTER_POINTER			0x07
#define EVENTID_HOVER_LEAVE_POINTER			0x08
#define EVENTID_HOVER_MOTION_POINTER			0x09
#define EVENTID_PROXIMITY_IN				0x0B
#define EVENTID_PROXIMITY_OUT				0x0C
#define EVENTID_MSKEY					0x0E
#define EVENTID_ERROR					0x0F
#define EVENTID_CONTROLLER_READY			0x10
#define EVENTID_SLEEPOUT_CONTROLLER_READY		0x11
#define EVENTID_RESULT_READ_REGISTER			0x12
#define EVENTID_STATUS_REQUEST_COMP			0x13
#define EVENTID_STATUS_EVENT				0x16
#define EVENTID_INTERNAL_RELEASE_INFO			0x19
#define EVENTID_EXTERNAL_RELEASE_INFO			0x1A

#define EVENTID_FROM_STRING				0x80
#define EVENTID_GESTURE					0x20

/* side touch event-id for debug, remove after f/w fixed */
#define EVENTID_SIDE_SCROLL				0x40
#define EVENTID_SIDE_TOUCH_DEBUG			0xDB
#define EVENTID_SIDE_TOUCH				0x0B
#define EVENTID_GESTURE_WAKEUP				0xE2
#define EVENTID_GESTURE_HOME				0xE6
#define EVENTID_PRESSURE				0xE7

#define STATUS_EVENT_MUTUAL_AUTOTUNE_DONE		0x01
#define STATUS_EVENT_SELF_AUTOTUNE_DONE			0x42
#define STATUS_EVENT_SELF_AUTOTUNE_DONE_D3		0x02
#define STATUS_EVENT_WATER_SELF_AUTOTUNE_DONE		0x4E
#define STATUS_EVENT_FORCE_CAL_DONE			0x15
#define STATUS_EVENT_FORCE_CAL_DONE_D3			0x06
#ifdef FTS_SUPPORT_WATER_MODE
#define STATUS_EVENT_WATER_SELF_DONE			0x17
#endif
#define STATUS_EVENT_FLASH_WRITE_CONFIG			0x03
#define STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE		0x04
#define STATUS_EVENT_FORCE_CAL_MUTUAL_SELF		0x05
#define STATUS_EVENT_FORCE_CAL_MUTUAL			0x15
#define STATUS_EVENT_FORCE_CAL_SELF			0x06
#define STATUS_EVENT_WATERMODE_ON			0x07
#define STATUS_EVENT_WATERMODE_OFF			0x08
#define STATUS_EVENT_RTUNE_MUTUAL			0x09
#define STATUS_EVENT_RTUNE_SELF				0x0A
#define STATUS_EVENT_PANEL_TEST_RESULT			0x0B
#define STATUS_EVENT_GLOVE_MODE				0x0C
#define STATUS_EVENT_RAW_DATA_READY			0x0D
#define STATUS_EVENT_PURE_AUTOTUNE_FLAG_WRITE_FINISH	0x10
#define STATUS_EVENT_PURE_AUTOTUNE_FLAG_ERASE_FINISH	0x11
#define STATUS_EVENT_MUTUAL_CAL_FRAME_CHECK		0xC1
#define STATUS_EVENT_SELF_CAL_FRAME_CHECK		0xC2
#define STATUS_EVENT_CHARGER_CONNECTED			0xCC
#define STATUS_EVENT_CHARGER_DISCONNECTED		0xCD

#define PURE_AUTOTUNE_FLAG_ID0				0xA5
#define PURE_AUTOTUNE_FLAG_ID1				0x96

#define INT_ENABLE_D3					0x48
#define INT_DISABLE_D3					0x08

#define INT_ENABLE					0x01
#define INT_DISABLE					0x00

#define READ_STATUS					0x84
#define READ_ONE_EVENT					0x85
#define READ_ALL_EVENT					0x86

#define SLEEPIN						0x90
#define SLEEPOUT					0x91
#define SENSEOFF					0x92
#define SENSEON						0x93
#define FTS_CMD_HOVER_OFF				0x94
#define FTS_CMD_HOVER_ON				0x95

#define FTS_CMD_MSKEY_AUTOTUNE				0x96
#define FTS_CMD_TRIM_LOW_POWER_OSCILLATOR		0x97

#define FTS_CMD_FORCE_AUTOTUNE				0x98

#define FTS_CMD_KEY_SENSE_OFF				0x9A
#define FTS_CMD_KEY_SENSE_ON				0x9B
#define FTS_CMD_PRESSURE_SENSE_ON			0x9C
#define FTS_CMD_SET_FAST_GLOVE_MODE			0x9D

#define FTS_CMD_MSHOVER_OFF				0x9E
#define FTS_CMD_MSHOVER_ON				0x9F
#define FTS_CMD_SET_NOR_GLOVE_MODE			0x9F

#define FLUSHBUFFER					0xA1
#define FORCECALIBRATION				0xA2
#define CX_TUNNING					0xA3
#define SELF_AUTO_TUNE					0xA4

#define FTS_CMD_CHARGER_PLUGGED				0xA8
#define FTS_CMD_CHARGER_UNPLUGGED			0xAB

#define FTS_CMD_RELEASEINFO				0xAA
#define FTS_CMD_STYLUS_OFF				0xAB
#define FTS_CMD_STYLUS_ON				0xAC
#define FTS_CMD_LOWPOWER_MODE				0xAD

#define FTS_CMD_ENABLE_FEATURE				0xC1
#define FTS_CMD_DISABLE_FEATURE				0xC2

#define FTS_CMD_WRITE_PRAM				0xF0
#define FTS_CMD_BURN_PROG_FLASH				0xF2
#define FTS_CMD_ERASE_PROG_FLASH			0xF3
#define FTS_CMD_READ_FLASH_STAT				0xF4
#define FTS_CMD_UNLOCK_FLASH				0xF7
#define FTS_CMD_SAVE_FWCONFIG				0xFB
#define FTS_CMD_SAVE_CX_TUNING				0xFC

#define FTS_CMD_FAST_SCAN				0x01
#define FTS_CMD_SLOW_SCAN				0x02
#define FTS_CMD_USLOW_SCAN				0x03

#define REPORT_RATE_90HZ				0
#define REPORT_RATE_60HZ				1
#define REPORT_RATE_30HZ				2

#define FTS_CMD_STRING_ACCESS				0x3000
#define FTS_CMD_NOTIFY					0xC0

#define FTS_RETRY_COUNT					10

#define FTS_STRING_EVENT_AOD_TRIGGER			(1 << 0)
#define FTS_STRING_EVENT_ONEWORDCALL_TRIGGER		(1 << 1)
#define FTS_STRING_EVENT_WATCH_STATUS			(1 << 2)
#define FTS_STRING_EVENT_FAST_ACCESS			(1 << 3)
#define FTS_STRING_EVENT_DIRECT_INDICATOR		((1 << 3) | (1 << 2))
#define FTS_STRING_EVENT_SPAY				(1 << 4)
#define FTS_STRING_EVENT_SPAY1				(1 << 5)
#define FTS_STRING_EVENT_SPAY2				((1 << 4) | (1 << 5))
#define FTS_STRING_EVENT_EDGE_SWIPE_RIGHT		(1 << 6)
#define FTS_STRING_EVENT_EDGE_SWIPE_LEFT		(1 << 7)

#define FTS_SIDEGESTURE_EVENT_SINGLE_STROKE		0xE0
#define FTS_SIDEGESTURE_EVENT_DOUBLE_STROKE		0xE1
#define FTS_SIDEGESTURE_EVENT_INNER_STROKE		0xE3

#define FTS_SIDETOUCH_EVENT_LONG_PRESS			0xBB
#define FTS_SIDETOUCH_EVENT_REBOOT_BY_ESD		0xED

#define FTS_ENABLE					1
#define FTS_DISABLE					0

#define FTS_MODE_SPAY					(1 << 1)
#define FTS_MODE_AOD					(1 << 2)

#define TSP_BUF_SIZE					3000
#define CMD_STR_LEN					256
#define CMD_RESULT_STR_LEN				3000
#define CMD_PARAM_NUM					8

#define FTS_LOWP_FLAG_2ND_SCREEN			(1 << 1)
#define FTS_LOWP_FLAG_BLACK_UI				(1 << 2)
#define FTS_LOWP_FLAG_SPAY				(1 << 5)
#define FTS_LOWP_FLAG_TEMP_CMD				(1 << 6)
#define FTS_LOWP_FLAG_GESTURE_WAKEUP			(1 << 7)

#define EDGEWAKE_RIGHT					0
#define EDGEWAKE_LEFT					1

#ifdef FTS_SUPPORT_TOUCH_KEY
/* TSP Key Feature*/
#define KEY_PRESS       1
#define KEY_RELEASE     0
#define TOUCH_KEY_NULL	0

/* support 2 touch keys */
#define TOUCH_KEY_RECENT		0x01
#define TOUCH_KEY_BACK		0x02


#define FTS_FLASH_DATA_OFFSET_BASE			16

enum fts_nvm_data_type {
	FTS_FLASH_TYPE_FACTORY_TEST = 1,
	FTS_FLASH_TYPE_CAL_INFOMATION,
	FTS_FLASH_TYPE_PRESSURE_STRENGTH,
	FTS_FLASH_TYPE_FORCE_CAL_TEST_COUNT,
};

#define NVM_CMD(mtype, moffset, mlength)		.type = mtype,	.offset = moffset,	.length = mlength

struct fts_nvm_data_map {
	int type;
	int offset;
	int length;
};

#ifdef PAT_CONTROL
/*---------------------------------------
	<<< apply to server >>>
	0x00 : no action
	0x01 : clear nv  
	0x02 : pat magic
	0x03 : rfu

	<<< use for temp bin >>>
	0x05 : forced clear nv & f/w update  before pat magic, eventhough same f/w
	0x06 : rfu
  ---------------------------------------*/
#define PAT_CONTROL_NONE  		0x00
#define PAT_CONTROL_CLEAR_NV 		0x01
#define PAT_CONTROL_PAT_MAGIC 		0x02
#define PAT_CONTROL_FORCE_UPDATE	0x05

#define PAT_COUNT_ZERO			0x00
#define PAT_MAX_LCIA			0x80
#define PAT_MAX_MAGIC			0xF5
#define PAT_MAGIC_NUMBER		0x83
#endif

struct fts_touchkey {
	unsigned int value;
	unsigned int keycode;
	char *name;
};
#endif

#ifdef FTS_SUPPORT_TA_MODE
extern struct fts_callbacks *fts_charger_callbacks;
struct fts_callbacks {
	void (*inform_charger) (struct fts_callbacks *, int);
};
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
struct fts_noise_param {
    unsigned short pAddr;
    unsigned char bestRIdx;
    unsigned char mtNoiseLvl;
    unsigned char sstNoiseLvl;
    unsigned char bestRMutual;
};
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

enum fts_fw_update_status {
	FTS_NOT_UPDATE = 10,
	FTS_NEED_FW_UPDATE,
	FTS_NEED_CALIBRATION_ONLY,
	FTS_NEED_FW_UPDATE_N_CALIBRATION,
};

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
	int lp;
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
	FTS_FEATURE_LPM_FUNCTION = 0x0B,
};

enum fts_system_information_address {
	FTS_SI_FILTERED_RAW_ADDR		= 0x02,
	FTS_SI_STRENGTH_ADDR			= 0x04,
	FTS_SI_SELF_FILTERED_FORCE_RAW_ADDR	= 0x1E,
	FTS_SI_SELF_FILTERED_SENSE_RAW_ADDR	= 0x20,
	FTS_SI_NOISE_PARAM_ADDR			= 0x40,
	FTS_SI_PURE_AUTOTUNE_FLAG		= 0x4E,
	FTS_SI_COMPENSATION_OFFSET_ADDR		= 0x50,
	FTS_SI_PURE_AUTOTUNE_CONFIG		= 0x52,
	FTS_SI_FACTORY_RESULT_FLAG		= 0x56,
	FTS_SI_AUTOTUNE_CNT			= 0x58,
	FTS_SI_SENSE_CH_LENGTH			= 0x5A, /* 2 bytes */
	FTS_SI_FORCE_CH_LENGTH			= 0x5C, /* 2 bytes */
	FTS_SI_FINGER_THRESHOLD			= 0x60, /* 2 bytes */
	FTS_SI_AUTOTUNE_PROTECTION_CONFIG	= 0x62, /* 2 bytes */
	FTS_SI_REPORT_PRESSURE_RAW_DATA		= 0x64, /* 2 bytes */
	FTS_SI_SS_KEY_THRESHOLD			= 0x66, /* 2 bytes */
	FTS_SI_MS_TUNE_VERSION			= 0x68, /* 2 bytes */
	FTS_SI_CONFIG_CHECKSUM			= 0x6A, /* 4 bytes */
	FTS_SI_FORCE_TOUCH_FILTERED_RAW_ADDR	= 0x70,
	FTS_SI_FORCE_TOUCH_STRENGTH_ADDR	= 0x72,
};

enum fts_config_value_feature {
	FTS_CFG_NONE = 0,
	FTS_CFG_APWR = 1,
	FTS_CFG_AUTO_TUNE_PROTECTION = 2,
};

enum {
	SPECIAL_EVENT_TYPE_SPAY			= 0x04,
	SPECIAL_EVENT_TYPE_PRESSURE_TOUCHED	= 0x05,
	SPECIAL_EVENT_TYPE_PRESSURE_RELEASED	= 0x06,
	SPECIAL_EVENT_TYPE_AOD			= 0x08,
	SPECIAL_EVENT_TYPE_AOD_PRESS		= 0x09,
	SPECIAL_EVENT_TYPE_AOD_LONGPRESS	= 0x0A,
	SPECIAL_EVENT_TYPE_AOD_DOUBLETAB	= 0x0B,
	SPECIAL_EVENT_TYPE_AOD_HOMEKEY		= 0x0C
};


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
 *			 u8 assy_count:2;	-> 00
 *			 u8 assy_result:2;	-> 01
 *			 u8 module_count:2;	-> 10
 *			 u8 module_result:2;	-> 11
 *		 } __attribute__ ((packed));
 *		 unsigned char data[1];
 *	 };
 *};
 * ----------------------------------------
 */
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

struct fts_i2c_platform_data {
	bool factory_flatform;
	bool recovery_mode;
	bool support_hover;
	bool support_mshover;
	int max_x;
	int max_y;
	int max_width;
	int grip_area;
	int SenseChannelLength;
	int ForceChannelLength;
	unsigned char panel_revision;	/* to identify panel info */
	int pat_function;	/*  copyed by dt, select function for suitable process  - pat_control */
	int afe_base;		/*  set f/w version when afe is fixed			- pat_control */
	const char *firmware_name;
	const char *project_name;
	const char *model_name;
	const char *regulator_dvdd;
	const char *regulator_avdd;

	struct pinctrl *pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	int (*power)(void *data, bool on);
	void (*register_cb)(void *);
	void (*enable_sync)(bool on);
	unsigned char (*get_ddi_type)(void);	/* to indentify ddi type */

	int tsp_icid;	/* IC Vendor */
	int tsp_id;	/* Panel Vendor */
	int device_id;	/* Device id */
	int tsp_reset;	/* Use RESET pin */

	int gpio;	/* Interrupt GPIO */
	unsigned int irq_type;
	u32	device_num;

#ifdef FTS_SUPPORT_TOUCH_KEY
	bool support_mskey;
	unsigned int num_touchkey;
	struct fts_touchkey *touchkey;
	const char *regulator_tk_led;
	int (*led_power)(void *, bool);
#endif
//#ifdef FTS_SUPPORT_SIDE_GESTURE
	int support_sidegesture;
//#endif
	int gpio_scl;
	int gpio_sda;

	int bringup;
};


struct fts_ts_info {
	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;

	int irq;
	int irq_type;
	bool irq_enabled;
	struct fts_i2c_platform_data *board;
#ifdef FTS_SUPPORT_TA_MODE
	void (*register_cb)(void *);
	struct fts_callbacks callbacks;
#endif
	struct mutex lock;
	bool tsp_enabled;
	bool probe_done;
#ifdef SEC_TSP_FACTORY_TEST
	struct sec_cmd_data sec;
	int SenseChannelLength;
	int ForceChannelLength;
	short *pFrame;
	unsigned char *cx_data;
#endif
	struct fts_ts_test_result test_result;
	unsigned char disassemble_count;

	bool hover_ready;
	bool hover_enabled;
	bool mshover_enabled;
	bool fast_mshover_enabled;
	bool flip_enable;
	bool mainscr_disable;
	bool report_pressure;
	unsigned int cover_type;

	unsigned char lowpower_flag;
	bool lowpower_mode;
	bool deepsleep_mode;
	bool wirelesscharger_mode;
	int fts_power_state;
	int wakeful_edge_side;
	unsigned char fts_mode;

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

	int touch_count;
	struct fts_finger finger[FINGER_MAX];

	int touch_mode;
	int retry_hover_enable_after_wakeup;

	int ic_product_id;			/* product id of ic */
	int ic_revision_of_ic;			/* revision of reading from IC */
	int fw_version_of_ic;			/* firmware version of IC */
	int ic_revision_of_bin;			/* revision of reading from binary */
	int fw_version_of_bin;			/* firmware version of binary */
	int config_version_of_ic;		/* Config release data from IC */
	int config_version_of_bin;		/* Config release data from IC */
	unsigned short fw_main_version_of_ic;	/* firmware main version of IC */
	unsigned short fw_main_version_of_bin;	/* firmware main version of binary */
	int panel_revision;			/* Octa panel revision */
	int tspid_val;
	int tspid2_val;

#ifdef USE_OPEN_DWORK
	struct delayed_work open_work;
#endif
	struct delayed_work work_read_nv;

#ifdef FTS_SUPPORT_NOISE_PARAM
	struct fts_noise_param noise_param;
	int (*fts_get_noise_param_address)(struct fts_ts_info *info);
#endif
	unsigned int delay_time;
	unsigned int debug_string;
	struct delayed_work reset_work;

	struct delayed_work debug_work;
	bool rawdata_read_lock;

	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;
#if defined(CONFIG_SECURE_TOUCH)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_interrupt;
	struct clk *core_clk;
	struct clk *iface_clk;
#endif
	struct mutex i2c_mutex;
	struct mutex device_mutex;
	bool touch_stopped;
	bool reinit_done;

	unsigned char data[FTS_EVENT_SIZE * FTS_FIFO_MAX];
	unsigned char ddi_type;

	const char *firmware_name;

	unsigned char o_afe_ver;
	unsigned char afe_ver;
	int cal_count;		/* calibration count   		- pat_control */
	int tune_fix_ver;	/* calibration version which f/w based on  - pat_control */

	int (*stop_device)(struct fts_ts_info *info);
	int (*start_device)(struct fts_ts_info *info);

	int (*fts_write_reg)(struct fts_ts_info *info, unsigned char *reg, unsigned short num_com);
	int (*fts_read_reg)(struct fts_ts_info *info, unsigned char *reg, int cnum, unsigned char *buf, int num);
	void (*fts_systemreset)(struct fts_ts_info *info);
	int (*fts_wait_for_ready)(struct fts_ts_info *info);
	void (*fts_command)(struct fts_ts_info *info, unsigned char cmd);
	void (*fts_enable_feature)(struct fts_ts_info *info, unsigned char cmd, int enable);
	int (*fts_get_version_info)(struct fts_ts_info *info);
	int (*fts_get_sysinfo_data)(struct fts_ts_info *info, unsigned char sysinfo_addr, unsigned char read_cnt, unsigned char *data);

#ifdef FTS_SUPPORT_STRINGLIB
	int (*fts_read_from_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
	int (*fts_write_to_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
#endif
};

int fts_fw_update_on_probe(struct fts_ts_info *info);
int fts_fw_update_on_hidden_menu(struct fts_ts_info *info, int update_type);
void fts_fw_init(struct fts_ts_info *info, bool magic_cal);
void fts_execute_autotune(struct fts_ts_info *info);
int fts_fw_wait_for_event(struct fts_ts_info *info, unsigned char eid);
int fts_fw_wait_for_event_D3(struct fts_ts_info *info, unsigned char eid0, unsigned char eid1);
int fts_fw_wait_for_specific_event(struct fts_ts_info *info, unsigned char eid0, unsigned char eid1, unsigned char eid2);
int fts_irq_enable(struct fts_ts_info *info, bool enable);
#ifdef PAT_CONTROL
void fts_set_pat_magic_number(struct fts_ts_info *info);
void set_pat_cal_count(struct fts_ts_info *info, unsigned char count);
void set_pat_tune_ver(struct fts_ts_info *info, int ver);
void get_pat_data(struct fts_ts_info *info);
#endif
int fts_get_tsp_test_result(struct fts_ts_info *info);
int fts_read_pressure_data(struct fts_ts_info *info);
void fts_interrupt_set(struct fts_ts_info *info, int enable);
void fts_release_all_finger(struct fts_ts_info *info);
int fts_read_analog_chip_id(struct fts_ts_info *info, unsigned char id);

#ifndef CONFIG_SEC_SYSFS
extern struct class *sec_class;
#endif
#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
extern void trustedui_mode_on(void);
#endif

#endif /* _LINUX_FTS_TS_H_ */
