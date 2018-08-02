#ifndef _CYPRESS_TOUCHKEY_H
#define _CYPRESS_TOUCHKEY_H

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
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sec_sysfs.h>
#include <linux/sec_batt.h>
#include <asm/unaligned.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/vmalloc.h>

extern int get_lcd_attached(char *);

/* LDO Regulator */
#define	TK_LED_REGULATOR_NAME	"vtouch_3.3v"
#define	TK_CORE_REGULATOR_NAME	"vtouch_3.3v"
#define TK_IO_REGULATOR_NAME	"vtouch_1.8v"

/* Touchkey Register */
#define BASE_REG			0x00
#define KEYCODE_REG			0x03
#define TK_FW_VER			0x04
#define TK_STATUS_FLAG			0x07
#define TK_THRESHOLD			0x09
#define TK_IDAC				0x0D
#define TK_COMP_IDAC			0x11
#define TK_DIFF_DATA			0x16
#define TK_RAW_DATA			0x1E
#define TK_BASELINE_DATA		0x26

#define TK_BIT_PRESS_EV			0x08
#define TK_BIT_KEYCODE			0x07

/* New command*/
#define TK_BIT_CMD_LEDCONTROL		0x40    // Owner for LED on/off control (0:host / 1:TouchIC)
#define TK_BIT_CMD_INSPECTION		0x20    // Inspection mode
#define TK_BIT_CMD_1mmSTYLUS		0x10    // 1mm stylus mode
#define TK_BIT_CMD_FLIP			0x08    // flip mode
#define TK_BIT_CMD_GLOVE		0x04    // glove mode
#define TK_BIT_CMD_TA_ON		0x02    // Ta mode
#define TK_BIT_CMD_REGULAR		0x01    // regular mode = normal mode

#define TK_BIT_WRITE_CONFIRM		0xAA
#define TK_BIT_WRITE_WITHOUT_CAL	0xEF
#define TK_BIT_EXIT_CONFIRM		0xBB

#define TK_BIT_LOW_SENSITIVITY		0x02	// high threshold
#define TK_BIT_NORMAL_SENSITIVITY	0xA2	// back to origin threshold

/* Status flag after sending command*/
#define TK_BIT_LEDCONTROL		0x40    // Owner for LED on/off control (0:host / 1:TouchIC)
#define TK_BIT_1mmSTYLUS		0x20    // 1mm stylus mode
#define TK_BIT_FLIP			0x10    // flip mode
#define TK_BIT_GLOVE			0x08    // glove mode
#define TK_BIT_TA_ON			0x04    // Ta mode
#define TK_BIT_REGULAR			0x02    // regular mode = normal mode
#define TK_BIT_LED_STATUS		0x01    // LED status

#define TK_CMD_LED_ON			0x10
#define TK_CMD_LED_OFF			0x20

#define TK_UPDATE_DOWN		1
#define TK_UPDATE_FAIL		-1
#define TK_UPDATE_PASS		0

/* update condition */
#define TK_RUN_UPDATE		1
#define TK_EXIT_UPDATE		2
#define TK_RUN_CHK		3

/* KEY_TYPE*/
#define TK_USE_2KEY_TYPE

/* Flip cover*/
#define TKEY_FLIP_MODE

/* Boot-up Firmware Update */
#define TK_HAS_FIRMWARE_UPDATE
#define TKEY_FW_PATH "/sdcard/Firmware/TOUCHKEY/fw.bin"
#define TKEY_FW_FFU_PATH "ffu_tk.bin"

#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
#define LIGHT_VERSION_PATH		"/efs/FactoryApp/tkey_light_version"
#define LIGHT_TABLE_PATH		"/efs/FactoryApp/tkey_light"
#define LIGHT_CRC_PATH			"/efs/FactoryApp/tkey_light_crc"
#define LIGHT_TABLE_PATH_LEN		50
#define LIGHT_VERSION_LEN		25
#define LIGHT_CRC_SIZE			10
#define LIGHT_DATA_SIZE			5
#define LIGHT_VOLTAGE_MIN_VAL		3000

struct light_info {
	int octa_id;
	int vol_mv;
};

enum WINDOW_COLOR {
	WINDOW_COLOR_BLACK_UTYPE = 0,
	WINDOW_COLOR_BLACK,
	WINDOW_COLOR_WHITE,
	WINDOW_COLOR_GOLD,
	WINDOW_COLOR_SILVER,
	WINDOW_COLOR_GREEN,
	WINDOW_COLOR_BLUE,
	WINDOW_COLOR_PINKGOLD,
};
#define WINDOW_COLOR_DEFAULT		WINDOW_COLOR_BLACK
#endif
#define CRC_CHECK_INTERNAL
#undef TK_USE_FWUPDATE_DWORK

#ifdef TK_USE_OPEN_DWORK
#define	TK_OPEN_DWORK_TIME	10
#endif

#ifdef CONFIG_GLOVE_TOUCH
#define	TK_GLOVE_DWORK_TIME	300
#endif

#if defined(TK_INFORM_CHARGER)
struct touchkey_callbacks {
	void (*inform_charger)(struct touchkey_callbacks *, bool);
};
#endif

enum {
	TK_CMD_READ_THRESHOLD = 0,
	TK_CMD_READ_DIFF,
	TK_CMD_READ_RAW,
	TK_CMD_READ_IDAC,
	TK_CMD_COMP_IDAC,
	TK_CMD_BASELINE,
};

enum {
	I_STATE_ON_IRQ = 0,
	I_STATE_OFF_IRQ,
	I_STATE_ON_I2C,
	I_STATE_OFF_I2C,
};

enum {
	FW_NONE = 0,
	FW_BUILT_IN,
	FW_HEADER,
	FW_IN_SDCARD,
	FW_EX_SDCARD,
	FW_FFU,
};

struct FAC_CMD {
	u8 cmd;
	u8 opt1; // 0, 1, 2, 3
	u16 result;
};

struct fw_image {
	u8 hdr_ver;
	u8 hdr_len;
	u16 first_fw_ver;
	u16 second_fw_ver;
	u16 third_ver;
	u32 fw_len;
	u16 checksum;
	u16 alignment_dummy;
	u8 data[0];
} __attribute__ ((packed));

/* mode change struct */
#define MODE_NONE 0
#define MODE_NORMAL 1
#define MODE_GLOVE 2
#define MODE_FLIP 3

#define CMD_GLOVE_ON 1
#define CMD_GLOVE_OFF 2
#define CMD_FLIP_ON 3
#define CMD_FLIP_OFF 4
#define CMD_GET_LAST_MODE 0xfd
#define CMD_MODE_RESERVED 0xfe
#define CMD_MODE_RESET 0xff

struct cmd_mode_change {
	int mode;
	int cmd;
};
struct mode_change_data {
	bool busy;
	int cur_mode;
	struct cmd_mode_change mtc;	/* mode to change */
	struct cmd_mode_change mtr;	/* mode to reserved */
	/* ctr vars */
	struct mutex mc_lock;
	struct mutex mcf_lock;
};

struct touchkey_devicetree_data {
	int gpio_sda;
	int gpio_scl;
	int gpio_int;
	int sub_det;
	u32 irq_gpio_flags;
	u32 sda_gpio_flags;
	u32 scl_gpio_flags;
	u32 sub_det_flags;
	bool i2c_gpio;
	u32 stabilizing_time;
	bool ap_io_power;
	bool boot_on_ldo;
	char *fw_path;
	const char *regulator_avdd;
};

/*Parameters for i2c driver*/
struct touchkey_i2c {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mutex lock;
	struct mutex i2c_lock;
	struct wake_lock fw_wakelock;
	struct device	*dev;
	int irq;
	int md_ver_ic;
	int fw_ver_ic;
	int device_ver;
#ifdef CRC_CHECK_INTERNAL
	int crc;
#endif
	struct touchkey_devicetree_data *dtdata;
	char *name;
	int update_status;
	bool enabled;
	int	src_fw_ver;
	int	src_md_ver;
#ifdef TK_USE_OPEN_DWORK
	struct delayed_work open_work;
#endif
#ifdef TK_INFORM_CHARGER
	struct touchkey_callbacks callbacks;
	bool charging_mode;
#endif
	bool status_update;
#ifdef TK_USE_FWUPDATE_DWORK
	struct work_struct update_work;
	struct workqueue_struct *fw_wq;
#endif
	u8 fw_path;
	const struct firmware *firm_data;
	struct fw_image *fw_img;
	bool do_checksum;
	struct pinctrl *pinctrl_irq;
	struct pinctrl *pinctrl_i2c;
	struct pinctrl *pinctrl_det;
	struct pinctrl_state *pin_state[4];
	struct pinctrl_state *pins_default;
	struct work_struct mode_change_work;
	struct mode_change_data mc_data;
	int ic_mode;
	int tsk_enable_glove_mode;
	bool glove_mode_keep_status;
	bool write_without_cal;

	int (*power)(void *, bool);
	void (*register_cb)(void *);

#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
	struct delayed_work efs_open_work;
	int light_version_efs;
	char light_version_full_efs[LIGHT_VERSION_LEN];
	char light_version_full_bin[LIGHT_VERSION_LEN];
	int light_table_crc;
#endif

	int thd_changed;
};
extern int lcdtype;

#endif /* _CYPRESS_TOUCHKEY_H */
