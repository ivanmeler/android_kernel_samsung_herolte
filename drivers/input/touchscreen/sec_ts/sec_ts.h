/* drivers/input/touchscreen/sec_ts.h
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_TS_H__
#define __SEC_TS_H__

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

#define SEC_TS_I2C_NAME "sec_ts"
#define SEC_TS_DEVICE_NAME "SEC_TS"

#ifdef CONFIG_SEC_FACTORY
#undef CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA
#endif

#define USE_OPEN_CLOSE
#define TOUCH_RESET_DWORK_TIME 10
/* #define USE_OPEN_POWER_RESET */
#define CALIBRATION_BY_FACTORY

#define USE_HW_PARAMETER

/* LCD ID  0x ID1 ID2 ID3 */
#define LCD_ID2_MODEL_MASK	0x003000	// ID2 - 00110000
#define MODEL_HERO	0x000000

#define MASK_1_BITS					0x0001
#define MASK_2_BITS					0x0003
#define MASK_3_BITS					0x0007
#define MASK_4_BITS					0x000F
#define MASK_5_BITS					0x001F
#define MASK_6_BITS					0x003F
#define MASK_7_BITS					0x007F
#define MASK_8_BITS					0x00FF

#define SEC_TS_SUPPORT_SPONGELIB

#define TYPE_STATUS_EVENT_ACK	1
#define TYPE_STATUS_EVENT_ERR	2
#define TYPE_STATUS_EVENT_INFO	3
#define TYPE_STATUS_EVENT_GEST	6
#define TYPE_STATUS_CODE_SPONGE	0x5A

#define SEC_TS_ERR_ESD 0xFF

#define BIT_STATUS_EVENT_ACK(a)		a << TYPE_STATUS_EVENT_ACK
#define BIT_STATUS_EVENT_ERR(a)		a << TYPE_STATUS_EVENT_ERR
#define BIT_STATUS_EVENT_INFO(a)	a << TYPE_STATUS_EVENT_INFO
#define BIT_STATUS_EVENT_GEST(a)	a << TYPE_STATUS_EVENT_GEST

#define DO_FW_CHECKSUM		(0x1 << 0)
#define DO_PARA_CHECKSUM	(0x1 << 1)

#define MAX_SUPPORT_TOUCH_COUNT 10
#define MAX_SUPPORT_HOVER_COUNT 1

#define SEC_TS_EVENTID_HOVER 10

#define SEC_TS_STATE_POWER_ON  1
#define SEC_TS_STATE_POWER_OFF 0

#define I2C_WRITE_BUFFER_SIZE 10

#define SEC_TS_DRV_VERSION "g_zerof.v02"

#define SEC_TS_FW_MAX_BURSTSIZE 256
#define CONFIG_FW_UPDATE_ON_PROBE

#define POR_AFTER_I2C_RETRY
#define SEC_TS_SUPPORT_SEC_SWIPE

#define AMBIENT_CAL 0
#define OFFSET_CAL_SDC		1
#define OFFSET_CAL_SEC		2

#define SEC_TS_SKIPTSP_DUTY	100

#define SEC_TS_NVM_OFFSET_FAC_RESULT	0
#define SEC_TS_NVM_OFFSET_CAL_COUNT	1
#define SEC_TS_NVM_OFFSET_DISASSEMBLE_COUNT	2
#define SEC_TS_NVM_OFFSET_CAL_POS	3
#define SEC_TS_NVM_OFFSET_LENGTH SEC_TS_NVM_OFFSET_CAL_POS + 1

/* SEC_TS READ REGISTER ADDRESS */
#define SEC_TS_READ_FW_STATUS		0x51
#define SEC_TS_READ_DEVICE_ID		0x52
#define SEC_TS_READ_SUB_ID		0x53
#define SEC_TS_READ_BOOT_STATUS		0x55
#define SEC_TS_READ_RAW_CHANNEL		0x58
#define SEC_TS_READ_FLASH_ERASE_STATUS	0x59
#define SEC_TS_READ_SET_TOUCHFUNCTION	0x64
#define SEC_TS_READ_THRESHOLD		0x6D
#define SEC_TS_READ_TS_STATUS		0x70
#define SEC_TS_READ_ONE_EVENT		0x71
#define SEC_TS_READ_CALIBRATION_REPORT	0x73
#define SEC_TS_READ_TOUCH_RAWDATA 0x76
#define SEC_TS_READ_TOUCH_SELF_RAWDATA 0x77
#define SEC_TS_READ_SELFTEST_RESULT   0x80
#define SEC_TS_READ_NVM   0x85
#define SEC_TS_READ_FW_INFO 0xA2
#define SEC_TS_READ_FW_VERSION 0xA3
#define SEC_TS_READ_PARA_VERSION 0xA4
#define SEC_TS_READ_IMG_VERSION 0xA5
#define SEC_TS_READ_LV3 0xD2
#define SEC_TS_READ_BL_UPDATE_STATUS 0xDB

#define SEC_TS_CMD_DEADZONE_RANGE 0x30
#define SEC_TS_CMD_LONGPRESSZONE_RANGE 0x31
#define SEC_TS_CMD_LONGPRESS_DROP_AREA 0x32
#define SEC_TS_CMD_LONGPRESS_DROP_DIFF 0x33
#define SEC_TS_CMD_SENSE_ON 0x40
#define SEC_TS_CMD_SENSE_OFF 0x41
#define SEC_TS_CMD_SW_RESET 0x42
#define SEC_TS_CMD_CALIBRATION_AMBIENT 0x43
#define SEC_TS_CMD_ERASE_FLASH 0x45
#define SEC_TS_CMD_STATEMANAGE_ON 0x48
#define SEC_TS_CMD_CALIBRATION_OFFSET_SDC	0x4C
#define SEC_TS_CMD_MIS_CAL_CHECK	0x4E
#define SEC_TS_CMD_CALIBRATION_OFFSET_SEC	0x4F
#define SEC_TS_CMD_SELFTEST   0x51
#define SEC_TS_CMD_WRITE_FW_BLK 0x53
#define SEC_TS_CMD_WRITE_FW_SHORT 0x54
#define SEC_TS_CMD_ENTER_FW_MODE 0x57
#define SEC_TS_CMD_WRITE_FW_LONG 0x5A
#define SEC_TS_CMD_MIS_CAL_READ		0x5E
#define SEC_TS_CMD_MIS_CAL_SPEC		0x5F
#define SEC_TS_CMD_CLEAR_EVENT_STACK 0x60
#define SEC_TS_CMD_SET_TOUCHFUNCTION 0x63
#define SEC_TS_CMD_SET_POWER_MODE	0x65
#define SEC_TS_CMD_STATUS_EVENT_TYPE	0x6B
#define SEC_TS_CMD_GESTURE_MODE	0x6C
#define SEC_TS_CMD_EDGE_DEADZONE        0x6E
#define SEC_TS_CMD_SET_COVERTYPE	0x6F
#define SEC_TS_CMD_SET_CHARGERTYPE	0x74
#define SEC_TS_CMD_SET_SPENMODE		0x75
#define SEC_TS_CMD_NOISE_MODE 0x77
#define SEC_TS_CMD_NVM    0x85
#define SEC_TS_CMD_WET_MODE	0x9F
#define SEC_TS_CMD_GET_CHECKSUM		0xA6
#define SEC_TS_CMD_CHG_SYSMODE 0xD7

/* SEC TS SPONGE OPCODE COMMAND */
#define SEC_TS_CMD_SPONGE_GET_INFO	0x90
#define SEC_TS_CMD_SPONGE_WRITE_PARAM		0x91
#define SEC_TS_CMD_SPONGE_READ_PARAM		0x92
#define SEC_TS_CMD_SPONGE_NOTIFY_PACKET		0x93

#define SEC_TS_CMD_MUTU_RAW_TYPE	0xF4
#define SEC_TS_CMD_SELF_RAW_TYPE	0xFA

#define SEC_TS_SELFTEST_REPORT_SIZE	80

#define SEC_TS_STATUS_BOOT_MODE 0x10
#define SEC_TS_STATUS_APP_MODE 0x20

#define SEC_TS_Status_Event	0
#define SEC_TS_Coordinate_Event 1
#define SEC_TS_Gesture_Event 2
#define SEC_TS_Event_Buff_Size 8
#define SEC_TS_SID_GESTURE		0x14
#define SEC_TS_GESTURE_CODE_AOD		0x00
#define SEC_TS_GESTURE_CODE_SPAY	0x0A
#define	SEC_TS_GESTURE_CODE_SIDE_GESTURE 0x11

#define SEC_TS_Coordinate_Action_None 0
#define SEC_TS_Coordinate_Action_Press 1
#define SEC_TS_Coordinate_Action_Release 3
#define SEC_TS_Coordinate_Action_Move 2

#define SEC_TS_TOUCHTYPE_NORMAL	0
#define SEC_TS_TOUCHTYPE_PROXIMITY 1
#define SEC_TS_TOUCHTYPE_FLIPCOVER 2
#define SEC_TS_TOUCHTYPE_GLOVE 3
#define SEC_TS_TOUCHTYPE_STYLUS 4
#define SEC_TS_TOUCHTYPE_HOVER 5
#define SEC_TS_TOUCHTYPE_PALM 6

/* SEC_TS_ACK : acknowledge event */
#define SEC_TS_ACK_OFFSET_CAL_DONE	0x01
#define SEC_TS_ACK_SELF_TEST_DONE	0x0A
#define SEC_TS_ACK_BOOT_COMPLETE	0x0C
#define SEC_TS_ACK_EVENT_QUEUE_FULL	0x29
#define SEC_TS_ACK_EVENT_WATER_DIVE_MODE	0x2C
#define SEC_TS_ACK_EVENT_WATER_WET_MODE		0x30

#define SEC_TS_BIT_SETFUNC_TOUCH (0x1<<0)
#define SEC_TS_BIT_SETFUNC_MUTUAL (0x1<<0)
#define SEC_TS_BIT_SETFUNC_HOVER (0x1<<1)
#define SEC_TS_BIT_SETFUNC_COVER (0x1<<2)
#define SEC_TS_BIT_SETFUNC_GLOVE (0x1<<3)
#define SEC_TS_BIT_SETFUNC_CHARGER (0x1<<4)
#define SEC_TS_BIT_SETFUNC_STYLUS (0x1<<5)
#define SEC_TS_BIT_SETFUNC_WIRELESSCHARGER (0x1<<7)


#define SEC_TS_STATUS_NOT_CALIBRATION	0x50
#define SEC_TS_STATUS_CALIBRATION_SDC	0xA1
#define SEC_TS_STATUS_CALIBRATION_SEC	0xA2

#define STATE_MANAGE_ON		1
#define STATE_MANAGE_OFF	0

#define SEC_TS_MAX_FW_PATH	64
#define SEC_TS_FW_BLK_SIZE 256
#define SEC_TS_FW_HEADER_SIGN 0x53494654
#define SEC_TS_FW_CHUNK_SIGN  0x53434654
#define SEC_TS_DEFAULT_FW_NAME	"tsp_sec/hero.bin"
#define SEC_TS_DEFAULT_BL_NAME	"tsp_sec/hero_bl.bin"
#define SEC_TS_DEFAULT_PARA_NAME	"tsp_sec/hero_para.bin"
#define SEC_TS_DEFAULT_UMS_FW	"/sdcard/Firmware/TSP/lsi.bin"
#define SEC_TS_DEFAULT_FFU_FW	"ffu_tsp.bin"

#define TWO_LEVEL_GRIP_CONCEPT
#ifdef TWO_LEVEL_GRIP_CONCEPT
#define SEC_TS_CMD_EDGE_HANDLER 		0x30
#define SEC_TS_CMD_EDGE_AREA			0x31
#define SEC_TS_CMD_DEAD_ZONE			0x32
#define SEC_TS_CMD_LANDSCAPE_MODE		0x33

// dual mode for Hero2
#define SEC_TS_CMD_EDGE_HANDLER_FOR_DUAL		0x34
#define SEC_TS_CMD_EDGE_AREA_FOR_DUAL			0x35
#define SEC_TS_CMD_DEAD_ZONE_FOR_DUAL			0x36
#define SEC_TS_CMD_LANDSCAPE_MODE_FOR_DUAL		0x37

enum grip_write_mode {
	G_NONE				= 0,
	G_SET_EDGE_HANDLER		= 1,
	G_SET_EDGE_ZONE			= 2,
	G_SET_NORMAL_MODE		= 4,
	G_SET_LANDSCAPE_MODE	= 8,
	G_CLR_LANDSCAPE_MODE	= 16,
};
enum grip_set_data {
	ONLY_EDGE_HANDLER		= 0,
	GRIP_ALL_DATA			= 1,
};
#endif

enum {
        BUILT_IN = 0,
        UMS,
	BL,
        FFU,
};

typedef struct
{
	u32 signature;			//signature
	u32 version;			//version
	u32 totalsize;			//total size
	u32 checksum;			//checksum
	u32 img_ver;			//image file version
	u32 img_date;			//image file date
	u32 img_description;	//image file description
	u32 fw_ver;				//firmware version
	u32 fw_date;			//firmware date
	u32 fw_description;		//firmware description
	u32 para_ver;			//parameter version
	u32 para_date;			//parameter date
	u32 para_description;	//parameter description
	u32 num_chunk;			//number of chunk
	u32 reserved1;
	u32 reserved2;
}fw_header;

typedef struct
{
	u32 signature;
	u32 addr;
	u32 size;
	u32 reserved;
}fw_chunk;

typedef enum {
	TOUCH_SYSTEM_MODE_BOOT = 0,
	TOUCH_SYSTEM_MODE_CALIBRATION = 1,
	TOUCH_SYSTEM_MODE_TOUCH = 2,
	TOUCH_SYSTEM_MODE_SELFTEST = 3,
	TOUCH_SYSTEM_MODE_FLASH = 4,
	TOUCH_SYSTEM_MODE_LOWPOWER = 5,
	TOUCH_SYSTEM_MODE_LISTEN
} TOUCH_SYSTEM_MODE;

typedef enum {
	TOUCH_MODE_STATE_IDLE = 0,
	TOUCH_MODE_STATE_HOVER = 1,
	TOUCH_MODE_STATE_TOUCH = 2,
	TOUCH_MODE_STATE_NOISY = 3,
	TOUCH_MODE_STATE_CAL = 4,
	TOUCH_MODE_STATE_CAL2 = 5,
	TOUCH_MODE_STATE_WAKEUP = 10
} TOUCH_MODE_STATE;

enum switch_system_mode {
	TO_TOUCH_MODE = 0,
	TO_LOWPOWER_MODE,
	TO_SELFTEST_MODE,
	TO_FLASH_MODE = 3,
};

typedef enum {
	SPONGE_EVENT_TYPE_SPAY			= 0x04,
	SPONGE_EVENT_TYPE_AOD			= 0x08,
	SPONGE_EVENT_TYPE_AOD_PRESS		= 0x09,
	SPONGE_EVENT_TYPE_AOD_LONGPRESS		= 0x0A,
	SPONGE_EVENT_TYPE_AOD_DOUBLETAB		= 0x0B
} SPONGE_EVENT_TYPE;

#define CMD_STR_LEN 256
#define CMD_PARAM_NUM 8
#define CMD_RESULT_STR_LEN 4095
#define SEC_CMD_BUF_SIZE		4095
#define CMD_RESULT_WORD_LEN		10

#define SEC_TS_I2C_RETRY_CNT 10
#define SEC_TS_WAIT_RETRY_CNT 100

/* #define SMARTCOVER_COVER      */  /*  for  Various Cover */
#undef SMARTCOVER_COVER

#ifdef SMARTCOVER_COVER
#define MAX_W 16                /* zero is 16 x 28 */
#define MAX_H 32                /* byte size to IC */
#define MAX_TX MAX_W
#define MAX_BYTE MAX_H
#endif

#define SEC_TS_MODE_SPONGE_SPAY			(1 << 1)
#define SEC_TS_MODE_SPONGE_AOD			(1 << 2)
#define SEC_TS_MODE_SPONGE_ONEWORDCALL		(1 << 3)

#define SEC_TS_LOWP_FLAG_AOD			(1 << 4)
#define SEC_TS_LOWP_FLAG_SPAY			(1 << 5)
#define SEC_TS_LOWP_FLAG_SIDE_GESTURE		(1 << 6)

#define SEC_TS_AOD_GESTURE_PRESS		(1 << 7)
#define SEC_TS_AOD_GESTURE_LONGPRESS		(1 << 6)
#define SEC_TS_AOD_GESTURE_DOUBLETAB		(1 << 5)

enum sec_ts_cover_id {
        SEC_TS_FLIP_WALLET = 0,
        SEC_TS_VIEW_COVER,
        SEC_TS_COVER_NOTHING1,
        SEC_TS_VIEW_WIRELESS,
        SEC_TS_COVER_NOTHING2,
        SEC_TS_CHARGER_COVER,
        SEC_TS_VIEW_WALLET,
        SEC_TS_LED_COVER,
        SEC_TS_CLEAR_FLIP_COVER,
        SEC_TS_QWERTY_KEYBOARD_EUR,
        SEC_TS_QWERTY_KEYBOARD_KOR,
        SEC_TS_MONTBLANC_COVER = 100,
};


extern struct sec_ts_callbacks *charger_callbacks;
struct sec_ts_callbacks {
	void (*inform_charger)(struct sec_ts_callbacks *, int type);
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
 *			 u8 assy_count:2;		-> 00
 *			 u8 assy_result:2;		-> 01
 *			 u8 module_count:2;	-> 10
 *			 u8 module_result:2;	-> 11
 *		 } __attribute__ ((packed));
 *		 unsigned char data[1];
 *	 };
 *};
 * ---------------------------------------- */
struct sec_ts_test_result {
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

struct sec_ts_coordinate {
	u16 x;
	u16 y;
	u16 mcount;
	u8 id;
	u8 ttype;
	u8 action;
	u8 touch_width;
	u8 hover_flag;
	u8 glove_flag;
	u8 touch_height;
	u8 major;
	u8 minor;
	u8 palm;
};

struct sec_ts_event_coordinate{
	u8 tchsta:3;
	u8 ttype:3;
	u8 eid:2;

	u8 tid:4;
	u8 nt:4;

	u8 x_11_4;

	u8 y_11_4;

	u8 y_3_0:4;
	u8 x_3_0:4;

	u8 z;
	u8 major;
	u8 minor;
//      u8 reserved_7:7;
} __attribute__ ((packed));

#define TEST_MODE_MIN_MAX		false
#define TEST_MODE_ALL_NODE		true
#define TEST_MODE_READ_FRAME		false
#define TEST_MODE_READ_CHANNEL		true

/* factory test mode */
struct sec_ts_test_mode {
	u8 type;
	short min;
	short max;
	bool allnode;
	bool frame_channel;
};

struct sec_ts_data {
	u32 isr_pin;

	u32 crc_addr;
	u32 fw_addr;
	u32 para_addr;

	u8 boot_ver[3];

	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct sec_ts_plat_data *plat_data;
	struct factory_data *f_data;
	struct sec_ts_coordinate coord[MAX_SUPPORT_TOUCH_COUNT + 1];
	struct completion init_done;

	uint32_t flags;
	unsigned char lowpower_flag;
	bool lowpower_mode;
	volatile int lowpower_status;
	int touch_count;
	int tx_count;
	int rx_count;
	int cm_specover;
	int i2c_burstmax;
	int ta_status;
	volatile int power_status;
	int raw_status;
	int touchkey_glove_mode_status;
	u8 touch_functions;
	u8 hover_enables;
	struct sec_ts_event_coordinate touchtype;
	bool touched[11];
	u8 gesture_status[SEC_TS_Event_Buff_Size];
	u8 cal_status;

	struct sec_ts_callbacks callbacks;

	struct mutex lock;
	struct mutex device_mutex;
	struct mutex i2c_mutex;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend *early_suspend;
#endif
	struct delayed_work reset_work;

	int irq;
	bool enabled;

	/* factory_data */
	struct device *fac_dev_ts;
	bool cmd_is_running;
	unsigned char cmd_state;
	char cmd[CMD_STR_LEN];
	int cmd_param[CMD_PARAM_NUM];
	char cmd_buff[CMD_STR_LEN];
	char cmd_result[CMD_RESULT_STR_LEN];
	int cmd_buffer_size;
	struct mutex cmd_lock;
	struct list_head cmd_list_head;
	void (*sec_ts_fn_init)(void *device_data);
	int SenseChannelLength;
	int ForceChannelLength;
	short *pFrame;
	short *sFrame;
	unsigned char *cx_data;
	struct delayed_work cover_cmd_work;
	int delayed_cmd_param[2];
	struct delayed_work set_wirelesscharger_mode_work;
	int wirelesscharger_delayed_cmd_param[1];

	struct delayed_work read_nv_work;

	bool touch_stopped;
	bool reinit_done;
	bool flip_enable;
	int cover_type;
	u8 cover_cmd;
	bool probe_done;
#ifdef FTS_SUPPORT_2NDSCREEN
	u8 SIDE_Flag;
	u8 previous_SIDE_value;
#endif
	bool use_sponge;

	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

#ifdef TWO_LEVEL_GRIP_CONCEPT
	u8	gripreg_edge_handler;
	u8	gripreg_edge_area;
	u8	gripreg_dead_zone;
	u8	gripreg_landscape_mode;
	u8	grip_edgehandler_direction;
	int grip_edgehandler_start_y;
	int grip_edgehandler_end_y;
	u16 grip_edge_range;
	u8 grip_deadzone_up_x;
	u8 grip_deadzone_dn_x;
	int grip_deadzone_y;
	u8 grip_landscape_mode;
	int grip_landscape_edge;
	u16 grip_landscape_deadzone;
#endif
#ifdef CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA
	bool brush_enable;
	bool velocity_enable;
#endif
	int nv;
	int cal_count;
	int cal_pos;
	bool external_factory;

#ifdef SMARTCOVER_COVER
	bool smart_cover[MAX_BYTE][MAX_TX];
	bool changed_table[MAX_TX][MAX_BYTE];
	u8 send_table[MAX_TX][4];
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
	struct delayed_work ghost_check;
	u8 tsp_dump_lock;
#endif

	int tspid_val;
	int tspid2_val;

	int wet_mode;
	int dive_mode;

#ifdef USE_HW_PARAMETER
	struct delayed_work work_read_info;

	unsigned char ito_test[4];
	unsigned char check_multi;
	unsigned int multi_count;
	unsigned int wet_count;
	unsigned int dive_count;
	unsigned int comm_err_count;
	unsigned char module_id[4];
#endif

	int (*sec_ts_i2c_write)(struct sec_ts_data * ts, u8 reg, u8 * data, int len);
	int (*sec_ts_i2c_read)(struct sec_ts_data * ts, u8 reg, u8 * data, int len);
	int (*sec_ts_i2c_read_bulk) (struct sec_ts_data * ts, u8 * data, int len);
	int (*sec_ts_i2c_write_burst)(struct sec_ts_data *ts, u8 *data, int len);
};

struct sec_ts_plat_data {
	int max_x;
	int max_y;
	//int intx_pin;
	int num_tx;
	int num_rx;
	unsigned gpio;
	unsigned gpio_det;
	int irq_type;
	int i2c_burstmax;

	const char *firmware_name;
	const char *parameter_name;
	const char *model_name;
	const char *project_name;
	const char *regulator_dvdd;
	const char *regulator_avdd;

	int panel_revision;
	u8 img_version_of_ic[4];
	u8 img_version_of_bin[4];
	u8 para_version_of_ic[4];
	u8 para_version_of_bin[4];

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_sleep;

	int (*power)(void *data, bool on);
	void (*recovery_mode)(bool on);
	void (*enable_sync)(bool on);
	void (*register_cb)(struct sec_ts_callbacks *);

	int grip_area;
	int grip_concept;
	int mis_cal_check;
	int clear_calnv;
	unsigned tspid;
	unsigned tspid2;
};

int sec_ts_firmware_update_on_probe(struct sec_ts_data *ts, bool force_update);
int sec_ts_firmware_update_on_hidden_menu(struct sec_ts_data *ts,int update_type);
int sec_ts_check_firmware_version(struct sec_ts_data *ts, const u8 *fw_info);
int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode);
int sec_ts_hover_enables(struct sec_ts_data *ts, int enables);
int sec_ts_set_cover_type(struct sec_ts_data *ts, bool enable);
int sec_ts_wait_for_ready(struct sec_ts_data *ts, unsigned int ack);
int sec_ts_function( int (*func_init)(void *device_data),
                void (*func_remove)(void));
int sec_ts_read_calibration_report(struct sec_ts_data *ts);
int sec_ts_execute_force_calibration(struct sec_ts_data *ts, int cal_mode);
int get_tsp_nvm_data(struct sec_ts_data *ts, u8 offset);
void set_tsp_nvm_data_clear(struct sec_ts_data *ts, u8 offset);
void sec_ts_release_all_finger(struct sec_ts_data *ts);

#ifdef USE_HW_PARAMETER
int sec_ts_read_raw_data(struct sec_ts_data *ts, struct sec_ts_test_mode *mode);
int execute_selftest(struct sec_ts_data *ts);
#endif

/* static void clear_cover_cmd_work(struct work_struct *work); */

void sec_ts_delay(unsigned int ms);
int sec_ts_fn_init(struct sec_ts_data *ts);


#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
extern void trustedui_mode_on(void);
extern void trustedui_mode_off(void);
#endif

extern unsigned int lcdtype;
int set_spen_mode(int mode);
extern struct sec_ts_data *ts_dup;

#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif

#ifdef TWO_LEVEL_GRIP_CONCEPT
extern void set_grip_data_to_ic(struct sec_ts_data *ts, u8 flag);
extern void sec_ts_set_grip_type(struct sec_ts_data *ts, u8 set_type);
#endif

#endif
