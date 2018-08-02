#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/input/sec_cmd.h>
#include <linux/sec_sysfs.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_LEDS_CLASS
#include <linux/leds.h>
#define TOUCHKEY_BACKLIGHT	"button-backlight"
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <linux/sec_debug.h>
#endif


#define	SEC_DND_N_COUNT		10
#define	SEC_DND_FREQUENCY	110

#define MAX_RAW_DATA_SZ		576	/*32x18*/
#define MAX_SUPPORTED_FINGER_NUM 5
#define MAX_TRAW_DATA_SZ	(MAX_RAW_DATA_SZ + 4*MAX_SUPPORTED_FINGER_NUM + 2)
#define TSP_CMD_STR_LEN 32
#define TSP_CMD_RESULT_STR_LEN 512
#define TSP_CMD_PARAM_NUM 8
#define TSP_BUF_SIZE 1024

#define G2TSP_TS_STATE_POWER_ON  1
#define G2TSP_TS_STATE_POWER_OFF 0

#define USE_OPEN_CLOSE
#define SEC_TSP_FACTORY_TEST	1	//for samsung
#define G2TSP_I2C_NAME  "g2touch"

/* model info */
#define TSP_FW_DEFALT_PATH	"/sdcard/Download/g2tsp/g2tsp_7400_fw.bin"

#define G2_DRIVER_VERSION	0x010301

//#define G2_SUPPORT_TOUCH_KEY

#define G2TSP_X_CH_NUM		15
#define G2TSP_Y_CH_NUM		20

#define FEATURE_SHRIMP_G7400
#ifdef FEATURE_SHRIMP_G7400
#define G2TSP_FW_NAME		"g2tsp_7400_fw.bin"
#else
#define G2TSP_FW_NAME		"g2tsp_fw.bin"
#endif
#define G2TSP_TS_MAX_FW_PATH		64
#define G2TSP_TS_DEFAULT_FW_NAME	"tsp_g2tsp/G7400_u.bin"

#ifdef FEATURE_SHRIMP_G7400
#define I2C_WRITE_Q_NUM   128
#else
#define I2C_WRITE_Q_NUM   64
#endif

#ifdef FEATURE_SHRIMP_G7400
#define G7400_BUF_SIZE 128
#endif

#define DEF_TRACKING_ID			10
#define G2TSP_FINGER_MAX		10

#define G2TSP_FINGER_RELEASE	0
#define G2TSP_FINGER_PRESS		1


#define FW_RECOVER_DELAY	msecs_to_jiffies(300)

#define TSP_DAC_FLAG		1
#define TSP_ADC_FLAG		2
#define TSP_AREA_FLAG		4
#define TSP_FACTORY_FLAG	8
#define TSP_GET_FRAME_FLAG	0x80

#define USE_COORD_4K

/* preriod raw data interval*/

#define G2_REG_TOUCH_BASE   0xA3
#define G2_REG_AUX1         0xE1
#define G2_REG_CHECKSUM     0xE2

#define G2_DEBUG_PRINT	1
#define G2_DEBUG_PRINT1	1
#define G2_DEBUG_PRINT2	1

#define printk(fmt, ...) //bringup_porting.add

#ifdef G2_DEBUG_PRINT
#define g2debug(dev, fmt, ...) \
({                              \
    dev_info(dev, fmt, ## __VA_ARGS__);     \
    sec_debug_tsp_log(fmt, ## __VA_ARGS__);     \
})
#else
#define g2debug(fmt, ...) do { } while(0)
#endif

#ifdef G2_DEBUG_PRINT1
#define g2debug1(dev, fmt, ...) \
({                              \
    dev_info(dev, fmt, ## __VA_ARGS__);     \
    sec_debug_tsp_log(fmt, ## __VA_ARGS__);     \
})
#else
#define g2debug1(fmt, ...) do { } while(0)
#endif

#ifdef G2_DEBUG_PRINT2
#define g2debug2(dev, fmt, ...) \
({                              \
    dev_info(dev, fmt, ## __VA_ARGS__);     \
    sec_debug_tsp_log(fmt, ## __VA_ARGS__);     \
})
#else
#define g2debug2(fmt, ...) do { } while(0)
#endif

#define SWAP16(x)	(((x>>8)& 0x00ff) | ((x<<8)& 0xff00))

#define DBG_PACKET_SIZE 16 * 1024   //16KByte

struct tDbgPacket
{
    u16	head;
    u16	tail;
    u8	buf[DBG_PACKET_SIZE];
} __attribute((packed));

typedef enum 
{
    eMode_Normal,
    eMode_FW_Update,
    eMode_Factory_Cal,
    eMode_FactoryTest,
}eWorkMode;

enum {
    BUILT_IN = 0,
    UMS,
    REQ_FW,
};

enum {
    R_READ = 0,
    R_WRITE,
};

struct tsp_callbacks 
{   
    void (*inform_charger)(struct tsp_callbacks *tsp_cb, int mode);
    void (*inform_earjack)(struct tsp_callbacks *tsp_cb, int mode);
    void (*inform_sleep)(struct tsp_callbacks *tsp_cb, int mode);
};

/**
 * struct g2tsp_finger - Represents fingers.
 * @ state: finger status (Event ID).
 * @ mcount: moving counter for debug.
 */
struct g2tsp_finger {
	unsigned char state;
	u32 mcount;
	int lx;
	int ly;
	int lp;
};

struct g2tsp_info
{
	struct device *pdev;
	struct i2c_client *client;
	int irq;
	// options
	int x_rev;
	int y_rev;
	bool power_status;

	struct g2tsp_finger finger[G2TSP_FINGER_MAX];
	u8	total_count;

#if defined(CONFIG_DEBUG_FS)
    struct dentry *debug_dir;
    u8 *raw_buffer;
    size_t raw_bufsize;
#endif	
    u8 auxreg1;
    u8 workMode;
    u8 jigId[16];
    u16 dacBuf[1024];
    int firmware_download;
    int calfinish;
    int factory_cal;
    int factory_flag;				// bit(0) dac, bit(1) adc, bit(2) area
    int fw_ver_ic[2];

    struct input_dev *input;
    struct workqueue_struct	*work_queue;
    struct work_struct irq_work;
    struct workqueue_struct	*watchdog_work_queue;
    struct work_struct watchdog_work;
    struct delayed_work fw_work;
    struct timer_list	timer;
    struct timer_list	watchdog_timer;
    struct mutex mutex;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
#endif
    struct g2tsp_platform_data *platform_data;
    struct tDbgPacket dbgPacket;
    u8 suspend;
    int prev_Area[10];
#ifdef CONFIG_LEDS_CLASS
    struct led_classdev leds;
    bool tkey_led_reserved;
#endif

#ifdef SEC_TSP_FACTORY_TEST
#if 1	// NEW
	struct sec_cmd_data sec;
#else	// OLD

    s16 ref_data[MAX_RAW_DATA_SZ];
    s16 normal_data[MAX_RAW_DATA_SZ];
    s16 delta_data[MAX_RAW_DATA_SZ];	

    struct list_head	cmd_list_head;
    u8	cmd_state;
    char	cmd[TSP_CMD_STR_LEN];
    int		cmd_param[TSP_CMD_PARAM_NUM];
    char	cmd_result[TSP_CMD_RESULT_STR_LEN];
    struct mutex	cmd_lock;
    bool	cmd_is_running;
#endif
#endif		

    struct tsp_callbacks cb;
	u8 tsp_info[30];
	bool sleep;
};

typedef struct 
{
    u8  head;
    u8  tail;
    u8  data[2*I2C_WRITE_Q_NUM];    //[4] -> cmd, index, reg, data
}t_I2cWriteQBuf;


struct g2tsp_i2c 
{
    struct i2c_client *client;
    void *g2tsp_handle;
};


#define G2_XREV			0x01	
#define G2_YREV			0x02
#define G2_FAC_CAL		0x04
#define G2_FWDOWN		0x10
#define G2_OLD_CHIP		0x20

struct g2tsp_keys_button 
{
    unsigned int code; 
    int glue_id;
};


struct g2tsp_platform_data 
{
    struct g2tsp_keys_button *keys;
    int nkeys;
    u32	res_x;
    u32 res_y;
    u32 options;
    void (*init)(void); 
    void (*suspend)(struct device *dev); 
    void (*wakeup)(struct device *dev);
    int (*reset)(void *);
    int (*power)(bool onoff); 

    u32 gpio_scl;
    u32 gpio_sda;
//    void (*i2c_to_gpio)(void *, int enable);

    void (*register_cb)(void *);
    void (*register_earjack_cb)(void *);

    int irq_gpio;
    int irq_flag;

    u32 fw_version[2];
    u32 hw_ver;

    struct pinctrl *pinctrl;
    struct pinctrl_state    *pins_default;
    struct pinctrl_state    *pins_sleep;

    // hw i2c
    struct pinctrl *pinctrl_i2c;
    struct pinctrl_state    *pins_i2c_default;
    struct pinctrl_state    *pins_i2c_on;
    struct pinctrl_state    *pins_i2c_off;

    const char *regulator_dvdd;
    const char *regulator_avdd;
    const char *firmware_name;
};

struct g2tsp_ic_info {
	u8 vender;
	u16 model;
	u8 fw_ver[3];
	u8 ic_name;
	u8 ic_rev;
	u8 tsp_type[3];
	u8 ch_x;
	u8 ch_y;
	u16 inch;
	u16 manufacture;
}  __attribute((packed)); 


extern const u16 Refence_dac[];

void tsp_earjack_infom(int en);


void tsp_reset_for_i2c(int en);
void g2tsp_release(void *handle);

void firmware_request_handler(const struct firmware *fw, void *context);
void g2tsp_dac_write(struct g2tsp_info *ts, u8 *data);

void PushDbgPacket(struct g2tsp_info *ts, u8 *src, u16 len);
int PopDbgPacket(struct g2tsp_info *ts, u8 *src);
void dbgMsgToCheese(const char *fmt, ...);
void TSPFrameCopytoBuf(u16 *dacBuf, u8* data);
void TSPFrameCopytoBuf_swap16(u16 *dacBuf, u8* data);

#ifdef FEATURE_SHRIMP_G7400
void G7400_Rev00_I2C_Write(t_I2cWriteQBuf *i2cWriteQBuf, u8 addr, u8 data);
int G7400_flash_erase_all(struct g2tsp_info *ts);
int G7400_flash_write(struct g2tsp_info *ts, int page, u8 *data);
int G7400_flash_page_read(struct g2tsp_info *ts, int page, u8 *rdBuf);
int G7400_flash_page_erase(struct g2tsp_info *ts, unsigned short page);
int G7400_firmware_update(struct g2tsp_info *ts, const u8 *data, int size);
#endif

int g2tsp_FactoryCal(struct g2tsp_info *ts);
int g2tsp_firmware_from_storage(struct g2tsp_info *ts);
void g2tsp_reset_register(struct g2tsp_info *ts);
void check_firmware_version(struct g2tsp_info *ts, char *buf);
int g2tsp_ts_load_fw_from_ums(struct g2tsp_info *ts);
int g2tsp_ts_firmware_update_on_hidden_menu(struct g2tsp_info *ts, int update_type);

s32 g2tsp_read_reg(struct g2tsp_info *info, u8 addr, u8 *value);
s32 g2tsp_write_reg(struct g2tsp_info *info, u8 addr, u8 value);
s32 g2tsp_read_block_data(struct g2tsp_info *info, u8 addr, u8 length, void *values);
s32 g2tsp_write_block_data(struct g2tsp_info *info, u8 addr, u8 length, const void *values);
