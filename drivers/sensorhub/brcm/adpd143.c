/**
 *\mainpage
 * ADPD143 driver
 * (c) copyright 2014 Analog Devices Inc.
 * Licensed under the GPL version 3 or later.
 * \date    April 2014
 * \version 1.1 Driver
 * \version Linux 3.4.0
*/
#include <linux/init.h>	 /* module initialization api */
#include <linux/module.h>   /* module functionality */
#include <linux/i2c.h>	  /* i2c related functionality */
#include <linux/slab.h>
#include <linux/pm.h>	   /* device Power Management functionality */
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/average.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/moduleparam.h>
#include <linux/regulator/consumer.h>
#include <linux/fs.h>
#include <linux/pinctrl/consumer.h>
#include "../../pinctrl/core.h"
#include "adpd143.h"

static int dbg_enable;

module_param(dbg_enable, int, S_IRUGO | S_IWUSR);

/* ADPD143 driver version*/
#define ADPD143_VERSION			"1.2"
/* ADPD143 release date*/
#define ADPD143_RELEASE_DATE		"Tue Apr 22 12:11:42 IST 2014"


#define ADPD_DEV_NAME			"adpd143"

/*INPUT DEVICE NAME LIST*/
#define MODULE_NAME_HRM			"hrm_sensor"
#define MODULE_NAME_HRMLED		"hrmled_sensor"
#define CHIP_NAME			"ADPD143RI"
#define VENDOR				"ADI"

#define MAX_EOL_RESULT			132

/*I2C RELATED*/
#define I2C_RETRY_DELAY			5
#define I2C_RETRIES			5

/*ADPD143 DRIVER RELATED*/
#define MAX_BUFFER			128
#define GLOBAL_OP_MODE_OFF		0
#define GLOBAL_OP_MODE_IDLE		1
#define GLOBAL_OP_MODE_GO		2

#define SET_GLOBAL_OP_MODE(val, cmd)	((val & 0xFC) | cmd)

#define FRM_FILE				0
#define FRM_ARR				1
#define EN_FIFO_CLK			0x3B5B
#define DS_FIFO_CLK			0x3050

/******************************************************************/
/**************** ADPD143 USER OPERATING MODE ********************/
/*
Operating mode defined to user
*/
#define IDLE_MODE			0x00
#define SAMPLE_XY_A_MODE		0x30
#define SAMPLE_XY_AB_MODE		0x31
#define SAMPLE_XY_B_MODE		0x32
#define GET_USR_MODE(usr_val)	((usr_val & 0xF0) >> 4)
#define GET_USR_SUB_MODE(usr_val)	(usr_val & 0xF)
/******************************************************************/
/**************** ADPD143 OPERATING MODE & DATA MODE *************/
#define R_RDATA_FLAG_EN		 (0x1 << 15)
#define R_PACK_START_EN		 (0x0 << 14)
#define	R_RDOUT_MODE			(0x0 << 13)
#define R_FIFO_PREVENT_EN	   (0x1 << 12)
#define	R_SAMP_OUT_MODE			(0x0 << 9)
#define DEFAULT_OP_MODE_CFG_VAL(cfg_x)  (R_RDATA_FLAG_EN + R_PACK_START_EN +\
					R_RDOUT_MODE + R_FIFO_PREVENT_EN+\
					R_SAMP_OUT_MODE)

#define SET_R_OP_MODE_A(val)		((val & 0xF000) >> 12)
#define SET_R_DATAMODE_A(val)	   ((val & 0x0F00) >> 8)
#define SET_R_OP_MODE_B(val)		((val & 0x00F0) >> 4)
#define SET_R_DATAMODE_B(val)	   ((val & 0x000F))
/**
@brief Used to get the value need to be set for slot-A, B operating mode
and data out mode.
*/
#define SET_MODE_VALUE(val)	 (SET_R_OP_MODE_A(val) |\
					(SET_R_DATAMODE_A(val) << 2) |\
					(SET_R_OP_MODE_B(val) << 5) |\
					(SET_R_DATAMODE_B(val) << 6))
/******************************************************************/
/**************** ADPD143 INTERRUPT MASK *************************/
#define INTR_MASK_IDLE_USR		0x0000
#define INTR_MASK_SAMPLE_USR		0x0020
#define INTR_MASK_S_SAMP_XY_A_USR	0x0020
#define INTR_MASK_S_SAMP_XY_AB_USR	0x0040
#define	INTR_MASK_S_SAMP_XY_B_USR	0x0040
#define SET_INTR_MASK(usr_val)		((~(INTR_MASK_##usr_val)) & 0x1FF)
/******************************************************************/
/***************** ADPD143 PRINT *********************************/
#ifdef ADPD_DBG
#define ADPD143_dbg(format, arg...)	\
				printk(KERN_DEBUG "ADPD143 : "format, ##arg);
#else
#define ADPD143_dbg(format, arg...)    {if (dbg_enable)\
				printk(KERN_DEBUG "ADPD143 : "format, ##arg);\
					}
#endif

#ifdef ADPD_INFO
#define ADPD143_info(format, arg...)	\
				printk(KERN_INFO "ADPD143 : "format, ##arg);
#else
#define ADPD143_info(format, arg...)   {if (0)\
					; }
#endif
/******************************************************************/
/***************** ADPD143 DIFFERENT MODE ************************/
/*
Main mode
*/
#define IDLE_USR			0
#define SAMPLE_USR			3
#define TIA_ADC_USR			5
/*
Sub mode
*/
#define S_SAMP_XY_A			0
#define S_SAMP_XY_AB			1
#define S_SAMP_XY_B			2
/*
OPERATING MODE ==>      MAIN_mode[20:16] |
			OP_mode_A[15:12] |
			DATA_mode_A[11:8]|
			OP_mode_B[7:4]   |
			DATA_mode_B[3:0]
*/
#define IDLE_OFF                        ((IDLE_USR << 16) | (0x0000))

#define SAMPLE_XY_A                     ((SAMPLE_USR << 16) | (0x1400))
#define SAMPLE_XY_AB			((SAMPLE_USR << 16) | (0x1414))
#define SAMPLE_XY_B			((SAMPLE_USR << 16) | (0x0014))

#define MAX_MODE                        6
#define MAX_IDLE_MODE                   1
#define MAX_SAMPLE_MODE                 3

#define MODE(val)                       __arr_mode_##val
#define MODE_SIZE(val)                  MAX_##val##_MODE


#define MAX_LIB_VER		20
#define OSC_TRIM_ADDR26_REG		0x26
#define OSC_TRIM_ADDR28_REG		0x28
#define OSC_TRIM_ADDR29_REG		0x29
#define OSC_TRIM_32K_REG		0x4B
#define OSC_TRIM_32M_REG		0x4D
#define OSC_DEFAULT_32K_SET		0x2695
#define OSC_DEFAULT_32M_SET		0x4272
#define OSCILLATOR_TRIM_FILE_PATH	"/efs/osc_trim"

#define	ALC_OFF_MODE		0
#define	ALC_TIA_MODE		1
#define	ALC_TNF_MODE		2
#define	ALC_FLOAT_MODE		3

#define HRM_LDO_ON			1
#define HRM_LDO_OFF		0

#define IRRED_OFF			0
#define IR_MODE			1
#define RED_MODE			2
#define IRRED_MODE			3

#define HRM_IDLE_MODE		0
#define HRM_SAMPLE_MODE		1
#define HRM_TIA_ADC_MODE		2

#define SAMPLE_MAX_COUNT		65532

#define CH1_FLOAT_STR		1200
#define CH2_FLOAT_STR		1600
#define CH3_FLOAT_STR		2200
#define CH4_FLOAT_STR		4500
/**
mode mapping structure used to hold the number mode actually supported by
driver
*/
struct mode_mapping {
	char *name;
	int *mode_code;
	unsigned int size;
};
int __arr_mode_IDLE[MAX_IDLE_MODE] =
				{ IDLE_OFF };
int __arr_mode_SAMPLE[MAX_SAMPLE_MODE] =
				{ SAMPLE_XY_A, SAMPLE_XY_AB, SAMPLE_XY_B };
int __arr_mode_TIA_ADC_SAMPLE[MAX_SAMPLE_MODE] =
				{ SAMPLE_XY_A, SAMPLE_XY_AB, SAMPLE_XY_B };

struct mode_mapping __mode_recv_frm_usr[MAX_MODE] = {
	{
		.name = "IDLE",
		.mode_code = MODE(IDLE),
		.size = MODE_SIZE(IDLE)
	},
	{	.name = "UNSUPPORTED1",
		.mode_code = 0,
		.size = 0,
	},
	{	.name = "UNSUPPORTED2",
		.mode_code = 0,
		.size = 0,
	},
	{
		.name = "SAMPLE",
		.mode_code = MODE(SAMPLE),
		.size = MODE_SIZE(SAMPLE)
	},
	{	.name = "UNSUPPORTED3",
		.mode_code = 0,
		.size = 0,
	},
	{
		.name = "TIA_ADC_SAMPLE",
		.mode_code = MODE(TIA_ADC_SAMPLE),
		.size = MODE_SIZE(SAMPLE)
	}
};

/**
general data buffer that is required to store the recieved buffer
*/
unsigned short data_buffer[MAX_BUFFER] = { 0 };

struct wrk_q_timestamp {
	struct timeval interrupt_trigger;
};

struct adpd143_stats {
	atomic_t interrupts;
	atomic_t fifo_requires_sync;
	atomic_t fifo_bytes[4];
	atomic_t wq_pending;
	atomic_t wq_schedule_time_peak_usec;
	atomic_t wq_schedule_time_last_usec;
	struct ewma wq_schedule_time_avg_usec;
	atomic_t data_process_time_peak_usec;
	atomic_t data_process_time_last_usec;
	struct ewma data_process_time_avg_usec;
	struct wrk_q_timestamp stamp;
};

/**
structure hold adpd143 chip structure, and parameter used
to control the adpd143 software part.
*/
struct adpd143_data {
	struct i2c_client *client;
	struct mutex mutex;/*for chip structure*/
	struct mutex storelock;
	struct device *dev;
	struct input_dev *hrm_input_dev;
	struct input_dev *hrmled_input_dev;
	struct adpd_platform_data *ptr_config;

	struct work_struct work;
	struct workqueue_struct *ptr_adpd143_wq_st;

	struct pinctrl *p;
	struct pinctrl_state *pins_sleep;
	struct pinctrl_state *pins_idle;

	int irq;
	int hrm_int;
	int led_en;
	int pre_hrmled_enable;
	int sample_cnt;
	int threshold;
	struct regulator *leda;
	unsigned short *ptr_data;

	struct adpd143_stats stats;

	unsigned short intr_mask_val;
	unsigned short intr_status_val;
	unsigned short fifo_size;

	/*sysfs register read write */
	unsigned short sysfs_I2C_regaddr;
	unsigned short sysfs_I2C_regval;
	unsigned short sysfslastreadval;

	atomic_t hrm_enabled;
	atomic_t hrmled_enabled;
	atomic_t adpd_mode;

	int (*read)(struct adpd143_data *, u8 reg_addr, int len, u16 *buf);
	int (*write)(struct adpd143_data *, u8 reg_addr, int len, u16 data);
	u8 eol_test_is_enable;
	u8 eol_test_status;
	char *eol_test_result;
	char *eol_lib_ver;
	char *elf_lib_ver;
	const char *led_3p3;
	const char *vdd_1p8;
	const char *i2c_1p8;

	int osc_trim_32K_value;
	int osc_trim_32M_value;
	int osc_trim_addr26_value;
	int osc_trim_addr28_value;
	int osc_trim_addr29_value;
	u8 osc_trim_open_enable;

	unsigned ch1_f_str;
	unsigned ch2_f_str;
	unsigned ch3_f_str;
	unsigned ch4_f_str;
};

/**
structure hold adpd143 configuration data to be written during probe.
*/
static struct adpd_platform_data adpd143_config_data = {
	.config_size = 54,
	.config_data = {
		0x00100001,
		0x000100ff,
		0x00020005,
		0x00060000,
		0x00111011,
		0x0012000A,
		0x00130320,
		0x00140449,
		0x00150333,
		0x001C4040,
		0x001D4040,
		0x00181400,
		0x00191400,
		0x001a1650,
		0x001b1000,
		0x001e1ff0,
		0x001f1Fc0,
		0x00201ad0,
		0x00211470,
		0x00231032,
		0x00241039,
		0x002502CC,
		0x00340000,
		0x00260000,
		0x00270800,
		0x00280000,
		0x00294003,
		0x00300330,
		0x00310213,
		0x00421d37,
		0x0043ada5,
		0x003924D2,
		0x00350330,
		0x00360813,
		0x00441d37,
		0x0045ada5,
		0x003b24D2,
		0x00590808,
		0x00320320,
		0x00330113,
		0x003a22d4,
		0x003c0006,
		0x00401010,
		0x0041004c,
		0x004c0004,
		0x004f2090,
		0x00500000,
		0x00523000,
		0x0053e400,
		0x00580000,
		0x005b1831,
		0x005d1b31,
		0x005e2808,
		0x004e0040,
	},
};

static struct adpd_platform_data adpd143_tia_adc_config_data = {
	.config_size = 66,
	.config_data = {
		0x00100001, 0x000100ff, 0x00020005, 0x00060000,
		0x00111011, 0x00120020, 0x00130320,
		0x00140000, /* No PD connect first for dark calibration */
		0x00150000,
		0x00233034, 0x0024303A, 0x002502CC,
		0x00340100, /* LED off */
		0x00260000, 0x00270800, 0x00280000,
		0x00300220, 0x00350220, /* Pulse offset */
		0x00310440, 0x00360244, /* Pulse count 0x0813/0120 */
		0x00294003,
		0x003924D2, 0x003b1a70, /* AFE_CTRL */
		0x00421d34, 0x00441d26, /* AFE_TRIM */
		0x0043b065, 0x0045ae65, /* AFE_MUX_SW */
		0x005e0808, 0x00597a08, /* float */
		0x00168080, 0x00178080, 0x001C8080, 0x001D8080, /*AFE ch gain */
		0x005b0000,
		0x00370000, 0x003d0000,
		0x00460000, 0x00470080, 0x00480000,
		0x00490000, 0x004a0000, 0x005d0000,
		0x00320320, 0x00330113, 0x003a22d4,
		0x003c3206, /* 32Mhz/PD cathod to VDD  - hrm : 0x003c0006 */
		0x00401010, 0x0041004c, 0x004c0004,
		0x004d4272, 0x004f2090, 0x00500000, 0x00523000,
		0x0053e400, 0x004e0040,
		0x005a0000,
		/* slot B-float:subtract first, add second  -hrm:0x00580000 */
		0x00580040,
		/* ADC offset start */
		0x00180000,
		0x00190000,
		0x001a0000,
		0x001b0000,
		0x001e0000,
		0x001f0000,
		0x00200000,
		0x00210000,
		/* ADC offset end */
	},
};
static int adpd143_configuration(struct adpd143_data *pst_adpd, unsigned char config);
static int adpd143_TIA_ADC_configuration(struct adpd143_data *pst_adpd, unsigned char config);

/* for TIA_ADC runtime Dark Calibration */
static unsigned short is_TIA_Dark_Cal = 0;
static unsigned short is_Float_Dark_Cal = 0;
static unsigned rawDarkCh[8];
static unsigned rawDataCh[8];

/**
This function is used to parse data from the string "0xXXX 1 0xYYY"
@param buf is pointer point constant address
@param cnt store number value need to parse
@param data parse data are stored in it
@return void
*/
static void
cmd_parsing(const char *buf, unsigned short cnt, unsigned short *data)
{

	char **bp = (char **)&buf;
	char *token, minus, parsing_cnt = 0;
	int val;
	int pos;

	while ((token = strsep(bp, " "))) {
		pos = 0;
		minus = false;
		if ((char)token[pos] == '-') {
			minus = true;
			pos++;
		}
		if ((token[pos] == '0') &&
			(token[pos + 1] == 'x' || token[pos + 1] == 'X')) {
			if (kstrtoul(&token[pos + 2], 16,
				(long unsigned int *)&val))
				val = 0;
			if (minus)
				val *= (-1);
		} else {
			if (kstrtoul(&token[pos], 10,
				(long unsigned int *)&val))
				val = 0;
			if (minus)
				val *= (-1);
		}

		if (parsing_cnt < cnt)
			*(data + parsing_cnt) = (unsigned short)val;
		else
			break;
		parsing_cnt++;
	}
}

/**
This function is a inline function mapped to i2c read functionality
@param pst_adpd the ADPD143 data structure
@param reg is the address from which the value need to be read out
@return u16 value present in the register.
*/
static inline u16
reg_read(struct adpd143_data *pst_adpd, u16 reg)
{
	u16 value;
	if (!pst_adpd->read(pst_adpd, reg, 1, &value))
		return value;
	return 0xFFFF;
}

/**
This function is a inline function mapped to i2c read functionality
@param pst_adpd the ADPD143 data structure
@param reg is the address from which the value need to be read out
@param len number of data need to be readout
@param buf is pointer store the value present in the register.
@return u16 status of i2c read.
*/
static inline u16
multi_reg_read(struct adpd143_data *pst_adpd, u16 reg, u16 len, u16 *buf)
{
	return pst_adpd->read(pst_adpd, reg, len, buf);
}

/**
This function is a inline function mapped to i2c write functionality
@param pst_adpd the ADPD143 data structure
@param reg is the address to which the data need to be written
@param value is the data need to be write.
@return u16 status of i2c write.
*/
static inline u16
reg_write(struct adpd143_data *pst_adpd, u16 reg, u16 value)
{
	return pst_adpd->write(pst_adpd, reg, 1, value);
}

/**
This function is used for I2C read from the sysfs
file system of the ADPD143 Chip
@param pst_adpd the ADPD143 data structure
@return u16 status of i2c read.
*/
static u16
adpd143_sysfs_I2C_read(struct adpd143_data *pst_adpd)
{
	return reg_read(pst_adpd, pst_adpd->sysfs_I2C_regaddr);
}

/**
This function is used for I2C write from the sysfs
file system of the ADPD143 Chip
@param pst_adpd the ADPD143 data structure
@return u16 the bytes of written data.
*/
static u16
adpd143_sysfs_I2C_write(struct adpd143_data *pst_adpd)
{
	u16 err;
	err = reg_write(pst_adpd, pst_adpd->sysfs_I2C_regaddr,
			pst_adpd->sysfs_I2C_regval);
	if (err)
		return 0xFFFF;
	else
		return pst_adpd->sysfs_I2C_regval;
}

/**
This function is used to parse string
@param recv_buf pointer point a string
@return int
*/
static int
parse_data(char *recv_buf)
{
	char **bp = (char **)&recv_buf;
	char *token, parsing_cnt = 0;
	long val;
	int test_data;
	unsigned int data = 0;
	int ret = 0;
	char test[10] = {'\0'};

	while ((token = strsep(bp, " \t"))) {
		memset(test, '\0', 10);
		memcpy(test, token, strlen(token));
		memmove(test+2, test, strlen(test));
		test[0] = '0';
		test[1] = 'x';
		ret = kstrtol(test, 0, &val);
		if (ret) {
			if (ret  == -ERANGE) {
				ADPD143_info("out of range\n");
				val = 0;
			}
			if (ret ==  -EINVAL) {
				ADPD143_info("parsing error\n");
				sscanf(test, "%x", &test_data);
				val = test_data;
			}
		}

		if (parsing_cnt == 0) {
			data = (int) (0xFF & val);
			if (data == 0)
				return -1;
		}
		if (parsing_cnt == 1) {
			data = data << 16;
			data |= (0xFFFF & val);
		}
		parsing_cnt++;
		if (parsing_cnt > 2)
			break;
	}
	return data;
}

/**
This function is used for reading configuration file
@param pst_adpd the ADPD143 data structure
@return int status of the configuration file.
*/
static int
adpd143_read_config_file(struct adpd143_data *pst_adpd)
{
	mm_segment_t old_fs;
	struct file *fpt_adpd = NULL;
	struct adpd_platform_data *ptr_config = NULL;
	int line_cnt = 0;
	int start = 0;
	int cnt = 0;
	int ret = 0;
	int i = 0;
	char get_char;
	char *recieved_buf = NULL;
	loff_t pos = 0;
	ptr_config = pst_adpd->ptr_config;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fpt_adpd = filp_open("/data/misc/adpd143_config.dcfg", O_RDONLY, 0666);
	if (IS_ERR(fpt_adpd)) {
		ADPD143_dbg("unable to find de file %ld\n", PTR_ERR(fpt_adpd));
		set_fs(old_fs);
		ret = /*PTR_ERR(fpt_adpd_filp); */ -1;
		ptr_config->config_size = 0;
		goto err_filp_open;
	}

	recieved_buf = kzalloc(sizeof(char) * 15, GFP_KERNEL);
	memset(recieved_buf, '\0', sizeof(char) * 15);

	while (vfs_read(fpt_adpd, &get_char, sizeof(char), &pos)) {
		if (get_char == '\n') {
			line_cnt++;
			if (cnt > 1) {
				ret = parse_data(recieved_buf);
				if (ret == -1) {
					ADPD143_dbg("Error in reading dcfg\n");
					break;
				}
				ADPD143_info("0x%08x\n", ret);
				ptr_config->config_data[i] = ret;
				i++;
			}
			memset(recieved_buf, '\0', sizeof(char) * 15);
			start = 0;
			cnt = 0;
			ret = 0;
		} else {
			if (get_char != '#') {
				if (start != 0xF) {
					recieved_buf[cnt] = get_char;
					cnt++;
				}
			} else {
				start = 0xF;
			}
		}
	}

	filp_close(fpt_adpd, NULL);

	set_fs(old_fs);
	kfree(recieved_buf);
	ptr_config->config_size = i;
	return 0;
err_filp_open:
	return -1;
}

/**
This function is used for reading interrupt and FIFO status
@param pst_adpd the ADPD143 data structure
@return unsigned short interrupt status value
*/
static unsigned short
adpd143_rd_intr_fifo(struct adpd143_data *pst_adpd)
{
	unsigned short regval = 0;
	unsigned short fifo_size = 0;
	unsigned short usr_mode = 0;
	regval = reg_read(pst_adpd, ADPD_INT_STATUS_ADDR);
	fifo_size = ((regval & 0xFF00) >> 8);
	pst_adpd->fifo_size = ((fifo_size / 2) + (fifo_size & 1));
	pst_adpd->intr_status_val = (regval & 0xFF);

	ADPD143_info("Intr_status 0x%x, FIFO_SIZE 0x%x\n",
			  pst_adpd->intr_status_val, pst_adpd->fifo_size);

	if (fifo_size <= 16)
		atomic_inc(&pst_adpd->stats.fifo_bytes[0]);
	else if (fifo_size <= 32)
		atomic_inc(&pst_adpd->stats.fifo_bytes[1]);
	else if (fifo_size <= 64)
		atomic_inc(&pst_adpd->stats.fifo_bytes[2]);
	else
		atomic_inc(&pst_adpd->stats.fifo_bytes[3]);

	usr_mode = atomic_read(&pst_adpd->adpd_mode);
	if (0 != usr_mode) {
		unsigned int mask = 0;
		unsigned int mod = 0;

		switch (GET_USR_SUB_MODE(usr_mode)) {
		case S_SAMP_XY_AB:
			mask = 0x07;
			break;
		default:
			mask = 0x03;
			break;
		};

		mod = pst_adpd->fifo_size & mask;
		if (mod) {
			ADPD143_info("Keeping %d samples in FIFO from %d\n",
				     mod, pst_adpd->fifo_size);
			pst_adpd->fifo_size &= ~mask;
			atomic_inc(&pst_adpd->stats.fifo_requires_sync);
		}
	}

	return pst_adpd->intr_status_val;
}

/**
This function is used for clearing the Interrupt as well as FIFO
@param pst_adpd the ADPD143 data structure
@return void
*/
static void
adpd143_clr_intr_fifo(struct adpd143_data *pst_adpd)
{
	/*below code is added for verification */
	unsigned short regval;
	regval = reg_read(pst_adpd, ADPD_INT_STATUS_ADDR);
	ADPD143_info("fifo_size: 0x%04x, Status: 0x%x\n",
			  ((regval & 0xFF00) >> 8), (regval & 0xFF));

	reg_write(pst_adpd, ADPD_INT_STATUS_ADDR, 0x80FF);
	regval = reg_read(pst_adpd, ADPD_INT_STATUS_ADDR);
	ADPD143_info("After clear - fifo_size: 0x%04x, Status: 0x%x\n",
			  ((regval & 0xFF00) >> 8), (regval & 0xFF));
}

/**
This function is used for clearing the Interrupt status
@param pst_adpd the ADPD143 data structure
@param mode operating mode of ADPD143
@return void
*/
static void
adpd143_clr_intr_status(struct adpd143_data *pst_adpd, unsigned short mode)
{
	reg_write(pst_adpd, ADPD_INT_STATUS_ADDR,
		  pst_adpd->intr_status_val);
}

/**
This function is used to read out FIFO data from FIFO
@param pst_adpd the ADPD143 data structure
@return unsigned int FIFO size
*/
static unsigned int
adpd143_rd_fifo_data(struct adpd143_data *pst_adpd)
{
	int loop_cnt = 0;

	if (!pst_adpd->fifo_size)
		goto err_rd_fifo_data;

	if (0 != atomic_read(&pst_adpd->adpd_mode) &&
		pst_adpd->fifo_size & 0x3) {
			pr_err("Unexpected FIFO_SIZE=%d,\
			should be multiple of four (channels)!\n",
			pst_adpd->fifo_size);
	}

/*  reg_write(pst_adpd, ADPD_ACCESS_CTRL_ADDR, 0x0001);*/
	reg_write(pst_adpd, ADPD_TEST_PD_ADDR, EN_FIFO_CLK);
	multi_reg_read(pst_adpd, ADPD_DATA_BUFFER_ADDR,
			   pst_adpd->fifo_size, pst_adpd->ptr_data);
/*  reg_write(pst_adpd, ADPD_ACCESS_CTRL_ADDR, 0x0000);*/
	reg_write(pst_adpd, ADPD_TEST_PD_ADDR, DS_FIFO_CLK);

	for (; loop_cnt < pst_adpd->fifo_size; loop_cnt++) {
		ADPD143_info("[0x%04x] 0x%04x\n", loop_cnt,
				  pst_adpd->ptr_data[loop_cnt]);
	}
	return pst_adpd->fifo_size;
err_rd_fifo_data:
	return 0;
}

/**
This function is a thing need to configure before changing the register
@param pst_adpd the ADPD143 data structure
@param global_mode OFF, IDLE, GO are the three mode
@return void
*/
static void
adpd143_config_prerequisite(struct adpd143_data *pst_adpd, u32 global_mode)
{
	unsigned short regval = 0;
	regval = reg_read(pst_adpd, ADPD_OP_MODE_ADDR);
	regval = SET_GLOBAL_OP_MODE(regval, global_mode);
	ADPD143_info("reg 0x%04x,\tafter set 0x%04x\n",
			  regval, SET_GLOBAL_OP_MODE(regval, global_mode));
	reg_write(pst_adpd, ADPD_OP_MODE_ADDR, regval);
}

void adpd143_pin_control(struct adpd143_data *pst_adpd, bool pin_set)
{
	int status = 0;
	pst_adpd->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(pst_adpd->pins_idle)) {
			status = pinctrl_select_state(pst_adpd->p,
				pst_adpd->pins_idle);
			if (status)
				pr_err("%s: can't set pin default state\n",
					__func__);
			pr_debug("%s idle\n", __func__);
		}
	} else {
		if (!IS_ERR(pst_adpd->pins_sleep)) {
			status = pinctrl_select_state(pst_adpd->p,
				pst_adpd->pins_sleep);
			if (status)
				pr_err("%s: can't set pin sleep state\n",
					__func__);
			pr_debug("%s sleep\n", __func__);
		}
	}
}

/**
This function is used for switching the adpd143 mode
@param pst_adpd the ADPD143 data structure
@param usr_mode user mode
@return void
*/
static void
adpd143_mode_switching(struct adpd143_data *pst_adpd, u32 usr_mode)
{
	unsigned int opmode_val = 0;
	unsigned short mode_val = 0;
	unsigned short intr_mask_val = 0;
	int i;

	unsigned short mode = GET_USR_MODE(usr_mode);
	unsigned short sub_mode = GET_USR_SUB_MODE(usr_mode);
	/*disabling further avoid further interrupt to trigger */
	disable_irq(pst_adpd->irq);

	/*stop the pending work */
	/*this function Gurantee that wrk is not pending or executing on CPU */
	cancel_work_sync(&pst_adpd->work);

	atomic_set(&pst_adpd->adpd_mode, 0);
	mutex_lock(&pst_adpd->mutex);

	/*Depending upon the mode get the value need to write Operatin mode*/
	opmode_val = *(__mode_recv_frm_usr[mode].mode_code + (sub_mode));

	adpd143_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_IDLE);

	/*switch to IDLE mode */
	mode_val = DEFAULT_OP_MODE_CFG_VAL(pst_adpd);
	mode_val += SET_MODE_VALUE(IDLE_OFF);
	intr_mask_val = SET_INTR_MASK(IDLE_USR);

	reg_write(pst_adpd, ADPD_OP_MODE_CFG_ADDR, mode_val);
	reg_write(pst_adpd, ADPD_INT_MASK_ADDR, intr_mask_val);

	/*clear FIFO and flush buffer */
	adpd143_clr_intr_fifo(pst_adpd);
	adpd143_rd_intr_fifo(pst_adpd);
	if (pst_adpd->fifo_size != 0) {
		adpd143_clr_intr_status(pst_adpd, IDLE_USR);
		ADPD143_info("Flushing FIFO\n");
		adpd143_rd_fifo_data(pst_adpd);
	}


	adpd143_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_GO);
	msleep(20);
	adpd143_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_IDLE);

	/*Find Interrupt mask value */
	switch (mode) {
	case IDLE_USR:
		ADPD143_info("IDLE MODE\n");
		intr_mask_val = SET_INTR_MASK(IDLE_USR);
		atomic_set(&pst_adpd->hrm_enabled, HRM_IDLE_MODE);
		break;
	case SAMPLE_USR:
		ADPD143_info("SAMPLE MODE\n");
		/*enable interrupt only when data written to FIFO */
		switch (sub_mode) {
		case S_SAMP_XY_A:
			intr_mask_val = SET_INTR_MASK(S_SAMP_XY_A_USR);
			break;
		case S_SAMP_XY_AB:
			intr_mask_val = SET_INTR_MASK(S_SAMP_XY_AB_USR);
			break;
		case S_SAMP_XY_B:
			intr_mask_val = SET_INTR_MASK(S_SAMP_XY_B_USR);
			break;
		default:
			intr_mask_val = SET_INTR_MASK(SAMPLE_USR);
			break;
		};
		intr_mask_val |= 0xC000;
		atomic_set(&pst_adpd->hrm_enabled, HRM_SAMPLE_MODE);
		adpd143_configuration(pst_adpd, 1);
		break;
	case TIA_ADC_USR:
		ADPD143_info("TIA_ADC_USR SAMPLE MODE\n");
		/* for TIA_ADC runtime Dark Calibration */
		pst_adpd->ch1_f_str = CH1_FLOAT_STR;
		pst_adpd->ch2_f_str = CH2_FLOAT_STR;
		pst_adpd->ch3_f_str = CH3_FLOAT_STR;
		pst_adpd->ch4_f_str = CH4_FLOAT_STR;
		is_TIA_Dark_Cal = is_Float_Dark_Cal = 0;
		for(i = 0; i < 8; i++) rawDarkCh[i] = rawDataCh[i] = 0;

		/*enable interrupt only when data written to FIFO */
		switch (sub_mode) {
		case S_SAMP_XY_A:
			intr_mask_val = SET_INTR_MASK(S_SAMP_XY_A_USR);
			break;
		case S_SAMP_XY_AB:
			intr_mask_val = SET_INTR_MASK(S_SAMP_XY_AB_USR);
			break;
		case S_SAMP_XY_B:
			intr_mask_val = SET_INTR_MASK(S_SAMP_XY_B_USR);
		break;
		default:
			intr_mask_val = SET_INTR_MASK(SAMPLE_USR);
			break;
		};
		intr_mask_val |= 0xC000;
		atomic_set(&pst_adpd->hrm_enabled, HRM_TIA_ADC_MODE);
		adpd143_TIA_ADC_configuration(pst_adpd, 1);
		break;
	default:
		/*This case won't occur */
		ADPD143_dbg("invalid mode\n");
		break;
	};

	ADPD143_info("INT_MASK_ADDR[0x%04x] = 0x%04x\n", ADPD_INT_MASK_ADDR,
			  intr_mask_val);
	pst_adpd->intr_mask_val = intr_mask_val;

	/*Fetch Default opmode configuration other than OP mode
	   and DATA_OUT mode */
	mode_val = DEFAULT_OP_MODE_CFG_VAL(pst_adpd);
	/*Get Mode value from the opmode value,
	to hardocde the macro, change SET_MODE_VALUE macro and concern
	mode macro
	*/
	mode_val += SET_MODE_VALUE(opmode_val);

	ADPD143_info("OP_MODE_CFG[0x%04x] = 0x%04x\n",
		ADPD_OP_MODE_CFG_ADDR, mode_val);

	atomic_set(&pst_adpd->adpd_mode, usr_mode);

	reg_write(pst_adpd, ADPD_INT_MASK_ADDR, pst_adpd->intr_mask_val);
	reg_write(pst_adpd, ADPD_OP_MODE_CFG_ADDR, mode_val);

	mutex_unlock(&pst_adpd->mutex);

	enable_irq(pst_adpd->irq);

	if (mode != IDLE_USR)
		adpd143_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_GO);
	else
		adpd143_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_OFF);
}

/**
This function is used for sending Sample event depend upon
the Sample mode
@param pst_adpd the ADPD143 data structure
@return void
*/
/* for making float mode saturated at the point */
#define FLOAT_STR_CONTROL 7200
/* for TIA_ADC runtime Dark Calibration */
#define TIA_DARK_CAL_CNT 5
#define FLOAT_DARK_CAL_CNT 5
static void
adpd143_sample_event(struct adpd143_data *pst_adpd)
{
	unsigned short usr_mode = 0;
	unsigned short sub_mode = 0;
	unsigned short cnt = 0;
	unsigned int ch;
	unsigned short uncoated_ch3 = 0;
	unsigned short coated_ch4 = 0;
	unsigned short k = 0;

	ADPD143_info("%s\n", __func__);

	usr_mode = atomic_read(&pst_adpd->adpd_mode);
	sub_mode = GET_USR_SUB_MODE(usr_mode);

	pst_adpd->sample_cnt++;
	if ((pst_adpd->sample_cnt % 200) == 1) {
		printk("%s: user mode : %d, sub mode : %d\n",
			__func__, GET_USR_MODE(usr_mode),GET_USR_SUB_MODE(usr_mode));
		if (pst_adpd->sample_cnt == SAMPLE_MAX_COUNT)
			pst_adpd->sample_cnt = 0;
	}

	switch (sub_mode) {
	case S_SAMP_XY_A:
		if (pst_adpd->fifo_size < 4 || (pst_adpd->fifo_size & 0x3)) {
			pr_err("Unexpected FIFO_SIZE=%d\n",
				pst_adpd->fifo_size);
			break;
		}

		if(GET_USR_MODE(usr_mode) != TIA_ADC_USR) {
			for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 4) {
				if (atomic_read(&pst_adpd->hrmled_enabled)) {
					input_event(pst_adpd->hrmled_input_dev,
						EV_REL, REL_Z, sub_mode + 1);
					input_event(pst_adpd->hrmled_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt] + 1);
					input_event(pst_adpd->hrmled_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 1] + 1);
					input_event(pst_adpd->hrmled_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 2] + 1);
					input_event(pst_adpd->hrmled_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 3] + 1);
					input_sync(pst_adpd->hrmled_input_dev);
				} else {
					input_event(pst_adpd->hrm_input_dev,
						EV_REL, REL_Z, sub_mode + 1);
					input_event(pst_adpd->hrm_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt] + 1);
					input_event(pst_adpd->hrm_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 1] + 1);
					input_event(pst_adpd->hrm_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 2] + 1);
					input_event(pst_adpd->hrm_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 3] + 1);
					input_sync(pst_adpd->hrm_input_dev);
				}
			}
		} else {
			for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 4) {
				/* for TIA_ADC runtime Dark Calibration */
				if (is_TIA_Dark_Cal < TIA_DARK_CAL_CNT
					&& is_TIA_Dark_Cal > 1) {
					for(k = 0; k < 4; k++) {
						if (rawDarkCh[k] < pst_adpd->ptr_data[cnt+k]
							|| !rawDarkCh[k])
							rawDarkCh[k] = pst_adpd->ptr_data[cnt+k];
					}
					is_TIA_Dark_Cal++;
					continue;
				} else if (is_TIA_Dark_Cal >= TIA_DARK_CAL_CNT
					&& is_TIA_Dark_Cal < 100) {
					rawDarkCh[0] -= 1470;
					rawDarkCh[1] -= 205;
					rawDarkCh[2] -= 210;
					rawDarkCh[3] -= 230;
					is_TIA_Dark_Cal = 100;
						continue;
				} else if (is_TIA_Dark_Cal < TIA_DARK_CAL_CNT) {
					is_TIA_Dark_Cal++;
					continue;
				} else if (is_TIA_Dark_Cal == 100) {
					reg_write(pst_adpd, 0x10,0x0001);
					reg_write(pst_adpd, 0x14,0x0441);
					reg_write(pst_adpd, 0x10,0x0002);
					is_TIA_Dark_Cal++;
					continue;
				}
				/* for making float mode saturated at the point */
				for(ch = 0; ch < 4; ++ch) {
					rawDataCh[ch] =
						rawDarkCh[ch] - pst_adpd->ptr_data[cnt+ch];
					rawDataCh[ch] =
						((int)rawDataCh[ch] < 0)? 0 : rawDataCh[ch];
				}
				input_event(pst_adpd->hrm_input_dev,
					EV_REL, REL_Z, sub_mode + 1);
				input_event(pst_adpd->hrm_input_dev, EV_MSC,
					MSC_RAW, rawDataCh[0]);
				input_event(pst_adpd->hrm_input_dev, EV_MSC,
					MSC_RAW, rawDataCh[1]);
				input_event(pst_adpd->hrm_input_dev, EV_MSC,
					MSC_RAW, rawDataCh[2]);
				uncoated_ch3 = rawDataCh[2];
				input_event(pst_adpd->hrm_input_dev, EV_MSC,
					MSC_RAW, rawDataCh[3]);
				input_sync(pst_adpd->hrm_input_dev);
				coated_ch4 = rawDataCh[3];
				ADPD143_dbg("adpd143 SLOT A(TIA) unch3:%d, ch4=%d\n",
					uncoated_ch3, coated_ch4);
			}
		}
		break;
	case S_SAMP_XY_AB:
		if (pst_adpd->fifo_size < 8 || (pst_adpd->fifo_size & 0x7)) {
			pr_err("Unexpected FIFO_SIZE=%d\n",
				pst_adpd->fifo_size);
			break;
		}

		if (GET_USR_MODE(usr_mode) != TIA_ADC_USR) {
			for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 8) {
				unsigned int sum_slot_a = 0;
				unsigned int sum_slot_b = 0;

				if (atomic_read(&pst_adpd->hrmled_enabled)) {
					input_event(pst_adpd->hrmled_input_dev,
					EV_REL, REL_Z, sub_mode + 1);

					for (ch = 0; ch < 4; ++ch) {
						sum_slot_a +=
						pst_adpd->ptr_data[cnt+ch];
						input_event(pst_adpd->hrmled_input_dev,
						EV_MSC, MSC_RAW,
						pst_adpd->ptr_data[cnt+ch] + 1);
					}
					input_event(pst_adpd->hrmled_input_dev,
						EV_REL, REL_X, sum_slot_a + 1);

					for (ch = 4; ch < 8; ++ch) {
						sum_slot_b +=
						pst_adpd->ptr_data[cnt+ch];
						input_event(pst_adpd->hrmled_input_dev,
						EV_MSC, MSC_RAW,
						pst_adpd->ptr_data[cnt+ch] + 1);
					}
					input_event(pst_adpd->hrmled_input_dev,
						EV_REL, REL_Y, sum_slot_b + 1);
					input_sync(pst_adpd->hrmled_input_dev);
				} else {
					input_event(pst_adpd->hrm_input_dev,
					EV_REL, REL_Z, sub_mode + 1);
					for (ch = 0; ch < 4; ++ch) {
						sum_slot_a +=
						pst_adpd->ptr_data[cnt+ch];
						input_event(pst_adpd->hrm_input_dev,
						EV_MSC, MSC_RAW,
						pst_adpd->ptr_data[cnt+ch] + 1);
					}
					input_event(pst_adpd->hrm_input_dev,
						EV_REL, REL_X, sum_slot_a + 1);

					for (ch = 4; ch < 8; ++ch) {
						sum_slot_b +=
						pst_adpd->ptr_data[cnt+ch];
						input_event(pst_adpd->hrm_input_dev,
						EV_MSC, MSC_RAW,
						pst_adpd->ptr_data[cnt+ch] + 1);
					}
					input_event(pst_adpd->hrm_input_dev,
						EV_REL, REL_Y, sum_slot_b + 1);
					input_sync(pst_adpd->hrm_input_dev);
					ADPD143_dbg("adpd143 sum_slot_a:%d, sum_slot_b:%d\n",
						sum_slot_a, sum_slot_b);
				}
			}
		} else {
			for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 8) {
				input_event(pst_adpd->hrm_input_dev,
					EV_REL, REL_Z, sub_mode + 1);
				/* for TIA_ADC runtime Dark Calibration */
				if (is_TIA_Dark_Cal < TIA_DARK_CAL_CNT
					&& is_TIA_Dark_Cal > 1) {
					for(k = 0; k < 4; k++) {
						if (rawDarkCh[k] < pst_adpd->ptr_data[cnt+k]
							|| !rawDarkCh[k])
							rawDarkCh[k] = pst_adpd->ptr_data[cnt+k];
					}
					is_TIA_Dark_Cal++;
				} else if (is_TIA_Dark_Cal >= TIA_DARK_CAL_CNT
					&& is_TIA_Dark_Cal < 100) {
						rawDarkCh[0] -= 1470;
						rawDarkCh[1] -= 205;
						rawDarkCh[2] -= 210;
						rawDarkCh[3] -= 230;
						is_TIA_Dark_Cal = 100;
				} else if (is_TIA_Dark_Cal < TIA_DARK_CAL_CNT) {
					is_TIA_Dark_Cal++;
				}
				/* for TIA_ADC runtime Dark Calibration */
				if (is_Float_Dark_Cal < TIA_DARK_CAL_CNT &&
					is_Float_Dark_Cal > 1) {
					for(k = 4; k < 8; k++) {
						if (rawDarkCh[k] < pst_adpd->ptr_data[cnt+k]
							|| !rawDarkCh[k])
							rawDarkCh[k] = pst_adpd->ptr_data[cnt+k];
					}
					is_Float_Dark_Cal++;
					continue;
				} else if (is_Float_Dark_Cal >= TIA_DARK_CAL_CNT &&
					is_Float_Dark_Cal < 100) {
						is_Float_Dark_Cal = 100;
						continue;
				} else if (is_Float_Dark_Cal < TIA_DARK_CAL_CNT) {
					is_Float_Dark_Cal++;
					continue;
				} else if (is_Float_Dark_Cal == 100) {
					reg_write(pst_adpd, 0x10,0x0001);
					reg_write(pst_adpd, 0x14,0x0441);
					reg_write(pst_adpd, 0x10,0x0002);
					is_Float_Dark_Cal++;
					continue;
				}
				/* for making float mode saturated at the point */
				for(ch = 0; ch < 4; ++ch) {
					rawDataCh[ch]
						= rawDarkCh[ch] - pst_adpd->ptr_data[cnt+ch];
					rawDataCh[ch]
						= ((int)rawDataCh[ch] < 0)? 0 : rawDataCh[ch];
				}

				for (ch = 0; ch < 4; ++ch) {
						if (ch == 2) uncoated_ch3 = rawDataCh[2];
						if (ch == 3) {
							coated_ch4 = rawDataCh[3];
							/* for approximating the code continuity between float and TIA_ADC */
							input_event(pst_adpd->hrm_input_dev,
							EV_MSC, MSC_RAW, rawDataCh[ch]*2 );
						} else {
							input_event(pst_adpd->hrm_input_dev,
							EV_MSC, MSC_RAW, rawDataCh[ch]);
							ADPD143_dbg("adpd143 SLOT A %d:%d\n",
								ch, rawDataCh[ch]);
						}
				}
				if (uncoated_ch3 == 0) {
					uncoated_ch3++;
					coated_ch4++;
				}
				input_event(pst_adpd->hrm_input_dev, EV_REL,
					REL_X, (((unsigned)coated_ch4) * 100) / uncoated_ch3);

				/* for making float mode saturated at the point */
				for (ch = 4; ch < 8; ++ch) {
					ADPD143_dbg("adpd142 SLOT B(float mode) : rawDataCh%d : %d, rawDarkCh%d : %d\n",
						ch, pst_adpd->ptr_data[cnt+ch], ch, rawDarkCh[ch]);
					rawDataCh[ch] =
						pst_adpd->ptr_data[cnt+ch] - rawDarkCh[ch];
					rawDataCh[ch] =
						((int)rawDataCh[ch] < 0)? 0 : rawDataCh[ch];

					if ((int)rawDataCh[ch] >=
						(FLOAT_STR_CONTROL - rawDarkCh[ch])) {
						rawDataCh[ch] =
							FLOAT_STR_CONTROL - rawDarkCh[ch];
						if (ch == 4 &&
							(((rawDataCh[0] < pst_adpd->ch1_f_str)
							&& (rawDataCh[0] > pst_adpd->ch1_f_str - 20))
							|| (pst_adpd->ch1_f_str == CH1_FLOAT_STR)))
							pst_adpd->ch1_f_str = rawDataCh[0];
						if (ch == 5 &&
							(((rawDataCh[1] < pst_adpd->ch2_f_str)
							&& (rawDataCh[1] > pst_adpd->ch2_f_str - 20))
							|| (pst_adpd->ch2_f_str == CH2_FLOAT_STR)))
							pst_adpd->ch2_f_str = rawDataCh[1];
						if (ch == 6 &&
							(((rawDataCh[2] < pst_adpd->ch3_f_str)
							&& (rawDataCh[2] > pst_adpd->ch3_f_str - 20))
							|| (pst_adpd->ch3_f_str == CH3_FLOAT_STR)))
							pst_adpd->ch3_f_str = rawDataCh[2];
						if (ch == 7 &&
							(((rawDataCh[3] < pst_adpd->ch4_f_str)
							&& (rawDataCh[3] > pst_adpd->ch4_f_str - 10))
							|| (pst_adpd->ch4_f_str == CH4_FLOAT_STR)))
							pst_adpd->ch4_f_str = rawDataCh[3];
					}
				}

				if ((int)rawDataCh[0] >= pst_adpd->ch1_f_str)
					rawDataCh[4] =
						FLOAT_STR_CONTROL - rawDarkCh[4];

				if ((int)rawDataCh[1] >= pst_adpd->ch2_f_str)
					rawDataCh[5] =
						FLOAT_STR_CONTROL - rawDarkCh[5];

				if ((int)rawDataCh[2] >= pst_adpd->ch3_f_str)
					rawDataCh[6] =
						FLOAT_STR_CONTROL - rawDarkCh[6];

				if ((int)rawDataCh[3] >= pst_adpd->ch4_f_str)
					rawDataCh[7] = FLOAT_STR_CONTROL;

				for (ch = 4; ch < 8; ++ch) {
					if (GET_USR_MODE(usr_mode) == TIA_ADC_USR) {
						if (ch == 6)
							uncoated_ch3 = rawDataCh[6];
						if (ch == 7)
							coated_ch4 = rawDataCh[7];
						input_event(pst_adpd->hrm_input_dev,
							EV_MSC, MSC_RAW, rawDataCh[ch]);
						ADPD143_dbg("adpd143 SLOT B(After Dark Cal) %d:%d\n",
							ch, rawDataCh[ch]);
					}
				}
				if (uncoated_ch3 == 0) {
					uncoated_ch3++;
					coated_ch4++;
				}
				input_event(pst_adpd->hrm_input_dev,
					EV_REL, REL_Y,
					(((unsigned)coated_ch4)*100)/uncoated_ch3);
				input_sync(pst_adpd->hrm_input_dev);
			}
		}
		break;
	case S_SAMP_XY_B:
		if (pst_adpd->fifo_size < 4 || (pst_adpd->fifo_size & 0x3)) {
			pr_err("Unexpected FIFO_SIZE=%d\n",
				pst_adpd->fifo_size);
			break;
		}

		if( GET_USR_MODE(usr_mode) != TIA_ADC_USR ) {
			for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 4) {
				if (atomic_read(&pst_adpd->hrmled_enabled)) {
					input_event(pst_adpd->hrmled_input_dev, EV_REL,
						REL_Z, sub_mode + 1);
					input_event(pst_adpd->hrmled_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt] + 1);
					input_event(pst_adpd->hrmled_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 1] + 1);
					input_event(pst_adpd->hrmled_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 2] + 1);
					input_event(pst_adpd->hrmled_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 3] + 1);
					input_sync(pst_adpd->hrmled_input_dev);
				} else {
					input_event(pst_adpd->hrm_input_dev, EV_REL,
						REL_Z, sub_mode + 1);
					input_event(pst_adpd->hrm_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt] + 1);
					input_event(pst_adpd->hrm_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 1] + 1);
					input_event(pst_adpd->hrm_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 2] + 1);
					input_event(pst_adpd->hrm_input_dev, EV_MSC,
						MSC_RAW, pst_adpd->ptr_data[cnt + 3] + 1);
					input_sync(pst_adpd->hrm_input_dev);
				}
			}
		} else {
			for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 4) {
				/* for TIA_ADC runtime Dark Calibration */
				if (is_Float_Dark_Cal < FLOAT_DARK_CAL_CNT &&
					is_Float_Dark_Cal > 1) {
					for(k = 4; k < 8; k++) {
						if (rawDarkCh[k] < pst_adpd->ptr_data[cnt+k-4]
							|| !rawDarkCh[k])
							rawDarkCh[k] = pst_adpd->ptr_data[cnt+k-4];
					}
					is_Float_Dark_Cal++;
					continue;
				} else if (is_Float_Dark_Cal >= FLOAT_DARK_CAL_CNT
					&& is_Float_Dark_Cal < 100) {
						is_Float_Dark_Cal = 100;
						continue;
				} else if (is_Float_Dark_Cal < FLOAT_DARK_CAL_CNT) {
					is_Float_Dark_Cal++;
					continue;
				} else if (is_Float_Dark_Cal == 100) {
					reg_write(pst_adpd, 0x10,0x0001);
					reg_write(pst_adpd, 0x14,0x0441);
					reg_write(pst_adpd, 0x10,0x0002);
					is_Float_Dark_Cal++;
				}

				/* for making float mode saturated at the point */
				for(ch = 4; ch < 8; ++ch) {
					rawDataCh[ch] =
						pst_adpd->ptr_data[cnt+ch] - rawDarkCh[ch];
					rawDataCh[ch] =
						((int)rawDataCh[ch] < 0) ? 0 : rawDataCh[ch];

					if ((int)rawDataCh[ch] >=
						(FLOAT_STR_CONTROL - rawDarkCh[ch]))
						rawDataCh[ch] =
							FLOAT_STR_CONTROL - rawDarkCh[ch];
				}

				input_event(pst_adpd->hrm_input_dev, EV_REL,
					REL_Z, sub_mode + 1);
				input_event(pst_adpd->hrm_input_dev, EV_MSC,
					MSC_RAW, rawDataCh[4]);
				input_event(pst_adpd->hrm_input_dev, EV_MSC,
					MSC_RAW, rawDataCh[5]);
				input_event(pst_adpd->hrm_input_dev, EV_MSC,
					MSC_RAW, rawDataCh[6]);
				uncoated_ch3 = rawDataCh[6];
				input_event(pst_adpd->hrm_input_dev, EV_MSC,
					MSC_RAW, rawDataCh[7]);
				input_sync(pst_adpd->hrm_input_dev);
				coated_ch4 = rawDataCh[7];
			}
		}
		break;
	default:
		break;
	};
}

/**
This function is used for handling FIFO when interrupt occured
@param pst_adpd the ADPD143 data structure
@return void
*/
static void
adpd143_data_handler(struct adpd143_data *pst_adpd)
{
	unsigned short usr_mode = 0;
	unsigned short mode = 0;
	unsigned short sub_mode = 0;
	mutex_lock(&pst_adpd->mutex);
	usr_mode = atomic_read(&pst_adpd->adpd_mode);
	mode = GET_USR_MODE(usr_mode);
	sub_mode = GET_USR_SUB_MODE(usr_mode);

	ADPD143_info("mode - 0x%x,\t sub_mode - 0x%x\n", mode, sub_mode);
	adpd143_rd_intr_fifo(pst_adpd);

	if (pst_adpd->intr_status_val == 0) {
		mutex_unlock(&pst_adpd->mutex);
		return;
	}

	adpd143_clr_intr_status(pst_adpd, mode);

	switch (mode) {
	case IDLE_USR:
		ADPD143_dbg("IDLE_MODE\n");
		adpd143_rd_fifo_data(pst_adpd);
		break;
	case SAMPLE_USR:
		ADPD143_info("SAMPLE MODE\n");
		adpd143_rd_fifo_data(pst_adpd);
		adpd143_sample_event(pst_adpd);
		break;
	/* for supporting TIA_ADC mode */
	case TIA_ADC_USR:
		ADPD143_info("SAMPLE TIA ADC MODE\n");
		adpd143_rd_fifo_data(pst_adpd);
		adpd143_sample_event(pst_adpd);
		break;
	default:
		ADPD143_info("DEFAULT MODE\n");
		adpd143_rd_fifo_data(pst_adpd);
		break;
	};

	mutex_unlock(&pst_adpd->mutex);
}

/**
This function is a handler for WorkQueue
@param ptr_work linux work structure
@return void
*/
static void
adpd143_wq_handler(struct work_struct *ptr_work)
{

	struct adpd143_data *pst_adpd = container_of(ptr_work,
					struct adpd143_data, work);

	struct timeval wkq_start;
	struct timeval wkq_comp;
	int diff_usec = 0;

	do_gettimeofday(&wkq_start);

	diff_usec = timeval_compare(&wkq_start,
			&pst_adpd->stats.stamp.interrupt_trigger);

	if (diff_usec > 1) {
		if (diff_usec >
			atomic_read(&pst_adpd->stats.wq_schedule_time_peak_usec))
			atomic_set(&pst_adpd->stats.wq_schedule_time_peak_usec,
				diff_usec);
		atomic_set(&pst_adpd->stats.wq_schedule_time_last_usec,
			   diff_usec);
		ewma_add(&pst_adpd->stats.wq_schedule_time_avg_usec, diff_usec);
	}

	adpd143_data_handler(pst_adpd);

	do_gettimeofday(&wkq_comp);

	diff_usec = timeval_compare(&wkq_comp, &wkq_start);

	if (diff_usec > 1) {
		if (diff_usec >
		atomic_read(&pst_adpd->stats.data_process_time_peak_usec))
			atomic_set(
				&pst_adpd->stats.data_process_time_peak_usec,
				diff_usec);
		atomic_set(&pst_adpd->stats.data_process_time_last_usec,
			   diff_usec);
		ewma_add(&pst_adpd->stats.data_process_time_avg_usec,
			 diff_usec);
	}

}

/**
This function is used for handling Interrupt from ADPD143
@param irq is Interrupt number
@param dev_id is pointer point to ADPD143 data structure
@return irqreturn_t is a Interrupt flag
*/
static irqreturn_t
adpd143_isr_handler(int irq, void *dev_id)
{
	struct adpd143_data *pst_adpd = dev_id;

	atomic_inc(&pst_adpd->stats.interrupts);

	if (!work_pending(&pst_adpd->work)) {
		do_gettimeofday(&pst_adpd->stats.stamp.interrupt_trigger);
		ADPD143_info("%s\n", __func__);
		if (!queue_work(pst_adpd->ptr_adpd143_wq_st, &pst_adpd->work))
			atomic_inc(&pst_adpd->stats.wq_pending);
	} else {
		atomic_inc(&pst_adpd->stats.wq_pending);
		ADPD143_info("work_pending !!\n");
	}
	return IRQ_HANDLED;
}

/**
This function is used for updating the ADPD143 structure after configuration
@param pst_adpd the ADPD143 data structure
@return void
*/
static void
adpd143_update_config(struct adpd143_data *pst_adpd)
{
	return;
}

/**
This function is used for loading the configuration data to ADPD143 chip
0 - From file "/data/misc/adpd143_configuration.dcfg"
1 - From Static defined Array
@param pst_adpd the ADPD143 data structure
@param config configuration command
@return int status
*/
static int
adpd143_configuration(struct adpd143_data *pst_adpd, unsigned char config)
{
	struct adpd_platform_data *ptr_config;
	unsigned short addr;
	unsigned short data;
	unsigned short cnt = 0;
	int ret = 0;

	/* must be removed in TIA mode */
	/* adpd143_mode_switching(pst_adpd, 0); */
	if (config == FRM_FILE) {
		ret = adpd143_read_config_file(pst_adpd);
		/*	 ADPD143_info("ARRAY_SIZE - %d\n", size);*/
	} else {
		ret = FRM_ARR;
	}
	if (ret == 0)
		ptr_config = pst_adpd->ptr_config;
	else
		ptr_config = &adpd143_config_data;

	for (cnt = 0; cnt < ptr_config->config_size; cnt++) {
		addr = (unsigned short) ((0xFFFF0000 &
					  ptr_config->config_data[cnt]) >> 16);
		data = (unsigned short) (0x0000FFFF &
					 ptr_config->config_data[cnt]);

		ADPD143_dbg("addr[0x%04x] = 0x%04x\n", addr, data);
		reg_write(pst_adpd, addr, data);
	}

	adpd143_update_config(pst_adpd);

	return 0;
}

static int
adpd143_TIA_ADC_configuration(struct adpd143_data *pst_adpd, unsigned char config)
{
	struct adpd_platform_data *ptr_config;
	unsigned short addr;
	unsigned short data;
	unsigned short cnt = 0;
	int ret = 0;

	pr_info("%s start", __func__);
	if (config == FRM_FILE) {
		ret = adpd143_read_config_file(pst_adpd);
		/*	 ADPD143_info("ARRAY_SIZE - %d\n", size);*/
	} else {
		ret = FRM_ARR;
	}
	if (ret == 0)
		ptr_config = pst_adpd->ptr_config;
	else
		ptr_config = &adpd143_tia_adc_config_data;

	for (cnt = 0; cnt < ptr_config->config_size; cnt++) {
		addr = (unsigned short) ((0xFFFF0000 &
					  ptr_config->config_data[cnt]) >> 16);
		data = (unsigned short) (0x0000FFFF &
					 ptr_config->config_data[cnt]);

		ADPD143_dbg("addr[0x%04x] = 0x%04x\n", addr, data);
		reg_write(pst_adpd, addr, data);
	}

	adpd143_update_config(pst_adpd);

	return 0;
}

/* read & write efs for hrm sensor */
static int osc_trim_efs_register_open(struct adpd143_data *pst_adpd)
{
	struct file *osc_filp = NULL;
	char buffer[60] = {0, };
	int err = 0;
	int osc_trim_32k, osc_trim_32m;
	int osc_trim_addr26, osc_trim_addr28, osc_trim_addr29;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	osc_filp = filp_open(OSCILLATOR_TRIM_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(osc_filp)) {
		err = PTR_ERR(osc_filp);
		if (err != -ENOENT)
			pr_err("%s: Can't open oscillator trim file\n",
				__func__);
		set_fs(old_fs);
		return err;
	}

	err = osc_filp->f_op->read(osc_filp,
		(char *)buffer,
		60 * sizeof(char), &osc_filp->f_pos);
	if (err != (60 * sizeof(char))) {
		pr_err("%s: Can't read the oscillator trim data from file\n",
			__func__);
		err = -EIO;
	}
	sscanf(buffer, "%d,%d,%d,%d,%d", &osc_trim_32k, &osc_trim_32m,
		&osc_trim_addr26, &osc_trim_addr28, &osc_trim_addr29);
	pr_info("%s: (%d,%d,%d,%d,%d)\n", __func__, osc_trim_32k,
		osc_trim_32m, osc_trim_addr26, osc_trim_addr28, osc_trim_addr29);

	pst_adpd->osc_trim_32K_value = osc_trim_32k;
	pst_adpd->osc_trim_32M_value = osc_trim_32m;
	pst_adpd->osc_trim_addr26_value = osc_trim_addr26;
	pst_adpd->osc_trim_addr28_value = osc_trim_addr28;
	pst_adpd->osc_trim_addr29_value = osc_trim_addr29;

	filp_close(osc_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int osc_trim_efs_register_save(struct adpd143_data *pst_adpd)
{
	struct file *osc_filp = NULL;
	mm_segment_t old_fs;
	char buffer[60] = {0, };
	int osc_trim_32k, osc_trim_32m;
	int osc_trim_addr26, osc_trim_addr28, osc_trim_addr29;
	int err = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	osc_filp = filp_open(OSCILLATOR_TRIM_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(osc_filp)) {
		pr_err("%s: Can't open oscillator trim file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(osc_filp);
		return err;
	}

	osc_trim_32k = reg_read(pst_adpd, OSC_TRIM_32K_REG);
	osc_trim_32m = reg_read(pst_adpd, OSC_TRIM_32M_REG);
	osc_trim_addr26 = reg_read(pst_adpd, OSC_TRIM_ADDR26_REG);
	osc_trim_addr28 = reg_read(pst_adpd, OSC_TRIM_ADDR28_REG);
	osc_trim_addr29 = reg_read(pst_adpd, OSC_TRIM_ADDR29_REG);

	sprintf(buffer, "%d,%d,%d,%d,%d", osc_trim_32k, osc_trim_32m,
		osc_trim_addr26, osc_trim_addr28, osc_trim_addr29);
	pr_info("%s: (%d,%d,%d,%d,%d)\n", __func__, osc_trim_32k, osc_trim_32m,
		osc_trim_addr26, osc_trim_addr28, osc_trim_addr29);

	err = osc_filp->f_op->write(osc_filp,
		(char *)&buffer,
		60 * sizeof(char), &osc_filp->f_pos);
	if (err != (60 * sizeof(char))) {
		pr_err("%s: Can't write the oscillator trim data to file\n",
			__func__);
		err = -EIO;
	}
	pst_adpd->osc_trim_32K_value = osc_trim_32k;
	pst_adpd->osc_trim_32M_value = osc_trim_32m;
	pst_adpd->osc_trim_addr26_value = osc_trim_addr26;
	pst_adpd->osc_trim_addr28_value = osc_trim_addr28;
	pst_adpd->osc_trim_addr29_value = osc_trim_addr29;

	filp_close(osc_filp, current->files);
	set_fs(old_fs);

	return err;
}

/**
This function clears all the statistic counters.
@param pst_adpd the ADPD143 data structure
@return void
*/
static void
adpd_stat_reset(struct adpd143_data *pst_adpd)
{
	atomic_set(&pst_adpd->stats.interrupts, 0);
	atomic_set(&pst_adpd->stats.wq_pending, 0);
	atomic_set(&pst_adpd->stats.wq_schedule_time_peak_usec, 0);
	atomic_set(&pst_adpd->stats.wq_schedule_time_last_usec, 0);
	atomic_set(&pst_adpd->stats.data_process_time_peak_usec, 0);
	atomic_set(&pst_adpd->stats.data_process_time_last_usec, 0);
	atomic_set(&pst_adpd->stats.fifo_requires_sync, 0);
	atomic_set(&pst_adpd->stats.fifo_bytes[0], 0);
	atomic_set(&pst_adpd->stats.fifo_bytes[1], 0);
	atomic_set(&pst_adpd->stats.fifo_bytes[2], 0);
	atomic_set(&pst_adpd->stats.fifo_bytes[3], 0);
	ewma_init(&pst_adpd->stats.wq_schedule_time_avg_usec, 2048, 128);
	ewma_init(&pst_adpd->stats.data_process_time_avg_usec, 2048, 128);
}

/* SAMPLE - SYSFS ATTRIBUTE*/
/**
This function is used for getting the status of the sample enable bit
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@return ssize_t size of data presnt in the buffer
*/
static ssize_t
hrm_attr_get_enable(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	int val = atomic_read(&pst_adpd->hrm_enabled);
	return sprintf(buf, "%d\n", val);
}

/**
This function is used for enabling the Sample mode
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@param size buffer size
@return ssize_t size of data written to the buffer
*/
static ssize_t
hrm_attr_set_enable(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[2];
	unsigned short mode = 0;
        unsigned short osc_trim_32k, osc_trim_32m;
	unsigned short osc_trim_addr26 = 0, osc_trim_addr28 = 0, osc_trim_addr29 = 0;
	int err;

	int rc = 0;
	int val;
	int n = sscanf(buf, "%d", &val);
	(void)n;
	memset(parse_data, 0, sizeof(parse_data));

	if (pst_adpd->osc_trim_open_enable == 1) {
		err = osc_trim_efs_register_open(pst_adpd);
		if (err < 0) {
			pr_err("%s: osc_trim_efs_register() empty(%d)\n",
				__func__, err);
			osc_trim_32k = OSC_DEFAULT_32K_SET;
			osc_trim_32m = OSC_DEFAULT_32M_SET;
		} else {
			osc_trim_32k = (unsigned short)pst_adpd->osc_trim_32K_value;
			osc_trim_32m = (unsigned short)pst_adpd->osc_trim_32M_value;
			osc_trim_addr26 = (unsigned short)pst_adpd->osc_trim_addr26_value;
			osc_trim_addr28 = (unsigned short)pst_adpd->osc_trim_addr28_value;
			osc_trim_addr29 = (unsigned short)pst_adpd->osc_trim_addr29_value;

			reg_write(pst_adpd, OSC_TRIM_ADDR26_REG, osc_trim_addr26);
			reg_write(pst_adpd, OSC_TRIM_ADDR28_REG, osc_trim_addr28);
			reg_write(pst_adpd, OSC_TRIM_ADDR29_REG, osc_trim_addr29);
		}
		pr_info("%s: 32K[%02x], 32M[%02x], addr26[%02x], addr28[%02x], addr29[%02x]\n",
			__func__, osc_trim_32k, osc_trim_32m, osc_trim_addr26,
			osc_trim_addr28, osc_trim_addr29);
		reg_write(pst_adpd, OSC_TRIM_32K_REG, osc_trim_32k);
		reg_write(pst_adpd, OSC_TRIM_32M_REG, osc_trim_32m);
		pst_adpd->osc_trim_open_enable = 0;
	}

	if (val == 1) {
		pr_info("%s: enable HRM.\n", __func__);
		if (pst_adpd->led_en != -1)
			gpio_set_value(pst_adpd->led_en, 1);
		else if (!IS_ERR(pst_adpd->leda)) {
			rc = regulator_enable(pst_adpd->leda);
			if (rc)
				pr_info(KERN_ERR "%s: adpd enable failed(%d)\n",
					__func__, rc);
		}
		cmd_parsing("0x32", 1, parse_data);
		atomic_set(&pst_adpd->hrm_enabled, HRM_SAMPLE_MODE);
	} else if (val == 2) {
		pr_info("%s: enable Ambient Light Measurement.\n",
			__func__);
		if (pst_adpd->led_en != -1)
			gpio_set_value(pst_adpd->led_en, 1);
		else if (!IS_ERR(pst_adpd->leda))
			rc = regulator_enable(pst_adpd->leda);
			if (rc)
				pr_info(KERN_ERR "%s: adpd enable ALM failed(%d)\n",
					__func__, rc);
			cmd_parsing("0x51", 1, parse_data);
			atomic_set(&pst_adpd->hrm_enabled, HRM_TIA_ADC_MODE);
	} else {
		pr_info("%s: disable.\n", __func__);
		if (pst_adpd->led_en != -1)
			gpio_set_value(pst_adpd->led_en, 0);
		else if (!IS_ERR(pst_adpd->leda)) {
			rc = regulator_disable(pst_adpd->leda);
			if (rc)
				pr_info(KERN_ERR "%s: adpd disable failed(%d)\n",
					__func__, rc);
		}
		cmd_parsing("0x0", 1, parse_data);
		atomic_set(&pst_adpd->hrm_enabled, HRM_IDLE_MODE);
	}
	mode = GET_USR_MODE(parse_data[0]);

	if (GET_USR_MODE(parse_data[0]) < MAX_MODE) {
		if ((GET_USR_SUB_MODE(parse_data[0])) <
			__mode_recv_frm_usr[mode].size) {
			adpd143_mode_switching(pst_adpd, parse_data[0]);
		} else {
			ADPD143_dbg("Sub mode Out of bound\n");
			adpd143_mode_switching(pst_adpd, 0);
		}
	} else {
		ADPD143_dbg("Mode out of bound\n");
		adpd143_mode_switching(pst_adpd, 0);
	}
	return size;
}

/* GENERAL - SYSFS ATTRIBUTE*/
/**
This function is used for getting the status of the adpd_mode
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@return ssize_t size of data presnt in the buffer
*/
static ssize_t
attr_get_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	int val = atomic_read(&pst_adpd->adpd_mode);
	return sprintf(buf, "%d\n", val);
}

/**
This function is used for switching ADPD143 mode
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@param size buffer size
@return ssize_t size of data written to the buffer
*/
static ssize_t
attr_set_mode(struct device *dev, struct device_attribute *attr,
		  const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[2];
	unsigned short mode = 0;

	memset(parse_data, 0, sizeof(parse_data));
	cmd_parsing(buf, 1, parse_data);

	ADPD143_info("Mode requested 0x%02x\n", parse_data[0]);
	pr_info("%s: data0[%02x], data1[%02x]\n",
		__func__, parse_data[0], parse_data[1]);
	mode = GET_USR_MODE(parse_data[0]);
	if (GET_USR_MODE(parse_data[0]) < MAX_MODE) {
		if ((GET_USR_SUB_MODE(parse_data[0])) <
			__mode_recv_frm_usr[mode].size) {
			adpd143_mode_switching(pst_adpd, parse_data[0]);
		} else {
			ADPD143_dbg("Sub mode Out of bound\n");
			adpd143_mode_switching(pst_adpd, 0);
		}
	} else {
		ADPD143_dbg("Mode out of bound\n");
			adpd143_mode_switching(pst_adpd, 0);
		}

	return size;
}

/**
This function is used for reading the register value
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@return ssize_t size of data presnt in the buffer
*/
static ssize_t
attr_reg_read_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	ADPD143_dbg("Regval: 0x%4x\n", pst_adpd->sysfslastreadval);
	return sprintf(buf, "0x%04x\n", pst_adpd->sysfslastreadval);
}

/**
This function is used for writing the register for reading back
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@param size buffer size
@return ssize_t size of data written to the buffer
*/
static ssize_t
attr_reg_read_set(struct device *dev, struct device_attribute *attr,
		  const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short addr, cnt;
	unsigned short parse_data[4];
	unsigned short ret;

	memset(parse_data, 0, sizeof(unsigned short) * 4);
	cmd_parsing(buf, 2, parse_data);
	addr = parse_data[0];
	cnt = parse_data[1];

	mutex_lock(&pst_adpd->mutex);

	pst_adpd->sysfs_I2C_regaddr = addr;

	ret = adpd143_sysfs_I2C_read(pst_adpd);
	if (ret != 0xFFFF) {
		ADPD143_dbg("RegRead_Store: addr = 0x%04X,value = 0x%04X\n",
				 addr, ret);
		pst_adpd->sysfslastreadval = ret;
	} else {
		ADPD143_dbg("%s: Error\n", __func__);
		pst_adpd->sysfslastreadval = (unsigned short) -1;
	}

	mutex_unlock(&pst_adpd->mutex);
	return size;
}

static int adpd_power_enable(struct adpd143_data *data, int en)
{
	int rc = 0;
	struct regulator *regulator_led_3p3;
	struct regulator *regulator_vdd_1p8;

	regulator_vdd_1p8 = regulator_get(NULL, data->vdd_1p8);
	if (IS_ERR(regulator_vdd_1p8) || regulator_vdd_1p8 == NULL) {
		pr_err("%s: vdd_1p8 regulator_get fail\n", __func__);
		return -ENODEV;
	}

	regulator_led_3p3 = regulator_get(NULL, data->led_3p3);
	if (IS_ERR(regulator_led_3p3) || regulator_led_3p3 == NULL) {
		pr_err("%s: vdd_3p3 regulator_get fail\n", __func__);
		regulator_put(regulator_vdd_1p8);
		return -ENODEV;
	}
	pr_info("%s: onoff = %d\n", __func__, en);

	if (en == HRM_LDO_ON) {
		rc = regulator_set_voltage(regulator_vdd_1p8, 1800000, 1800000);
		if (rc < 0) {
			pr_err("%s: set vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}

		rc = regulator_enable(regulator_vdd_1p8);
		if (rc) {
			pr_err("%s: enable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}

		rc = regulator_set_voltage(regulator_led_3p3, 3300000, 3300000);
		if (rc < 0) {
			pr_err("%s: set led_3p3 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
		rc = regulator_enable(regulator_led_3p3);
		if (rc) {
			pr_err("%s: enable led_3p3 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_vdd_1p8);
		if (rc) {
			pr_err("%s: disable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}

		rc = regulator_disable(regulator_led_3p3);
		if (rc) {
			pr_err("%s: disable led_3p3 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	}

	done:
	regulator_put(regulator_led_3p3);
	regulator_put(regulator_vdd_1p8);

	return rc;
}

static int adpd_parse_dt(struct adpd143_data *data, struct device *dev)
{
	struct device_node *this_node= dev->of_node;
	enum of_gpio_flags flags;
	int rc;

	if (this_node == NULL)
		return -ENODEV;

	data->hrm_int = of_get_named_gpio_flags(this_node,"adpd143,irq_gpio", 0, &flags);
	pr_err("%s: get hrm_int(data->hrm_int)(%d) \n", __func__, data->hrm_int);
	if (data->hrm_int < 0) {
		pr_err("%s: get hrm_int(%d) error\n", __func__, data->hrm_int);
		return -ENODEV;
	}

	rc = gpio_request(data->hrm_int, "hrm_int");
	if (rc) {
		pr_err("%s: failed to request hrm_int\n", __func__);
		goto err_int_gpio_req;
	}

	rc = gpio_direction_input(data->hrm_int);
	if (rc) {
		pr_err("%s: failed to set hrm_int as input\n", __func__);
		goto err_int_gpio_direction_input;
	}

	if (of_property_read_string(this_node, "adpd143,i2c_1p8",
		&data->i2c_1p8) < 0)
		pr_err("%s - get i2c_1p8 error\n", __func__);

	if (of_property_read_string(this_node, "adpd143,vdd_1p8",
		&data->vdd_1p8) < 0)
		pr_err("%s: get vdd_1p8 error\n", __func__);

	if (of_property_read_string(this_node, "adpd143,led_3p3",
		&data->led_3p3) < 0)
		pr_err("%s: get led_3p3 error\n", __func__);

	data->led_en =of_get_named_gpio_flags(this_node,"adpd143,led_en", 0, &flags);
	pr_err("%s: get led_en(data->led_en)(%d) \n", __func__, data->led_en);
	if (data->led_en < 0) {
		pr_err("%s: get led_en(%d) error\n", __func__, data->led_en);
		data->led_en = -1;
		data->leda = devm_regulator_get(dev, "reg_vled");
		if (IS_ERR(data->leda)) {
			pr_err("%s: regulator HRM_VLED fail \n",__func__);
		}
	} else {

		rc = gpio_request(data->led_en,"led_en");
		if (unlikely(rc < 0)) {
			pr_err("%s: gpio %d request failed (%d)\n",
				__func__, data->led_en, rc);
			goto err_int_gpio_direction_input;
		}

		rc = gpio_direction_output(data->led_en, 0);
		if (unlikely(rc < 0)) {
			pr_err("%s: failed to set gpio %d as input (%d)\n",
				__func__, data->led_en, rc);
			goto err_gpio_direction_output;
		}
	}

	data->p = pinctrl_get_select_default(dev);
	if (IS_ERR(data->p)) {
		pr_err("%s: failed pinctrl_get\n", __func__);
		return -EINVAL;
	}

	data->pins_sleep = pinctrl_lookup_state(data->p, PINCTRL_STATE_SLEEP);
	if(IS_ERR(data->pins_sleep)) {
		pr_err("%s : could not get pins sleep_state (%li)\n",
			__func__, PTR_ERR(data->pins_sleep));
		pinctrl_put(data->p);
		return -EINVAL;
	}

	data->pins_idle = pinctrl_lookup_state(data->p, PINCTRL_STATE_IDLE);
	if(IS_ERR(data->pins_idle)) {
		pr_err("%s : could not get pins idle_state (%li)\n",
			__func__, PTR_ERR(data->pins_idle));
		pinctrl_put(data->p);
		return -EINVAL;
	}

	return 0;
err_gpio_direction_output:
	gpio_free(data->led_en);
err_int_gpio_direction_input:
	gpio_free(data->hrm_int);
err_int_gpio_req:
	return rc;
}


/**
This function is used for writing a particular data to the register
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@param size buffer size
@return ssize_t size of data written to the buffer
*/
static ssize_t
attr_reg_write_set(struct device *dev, struct device_attribute *attr,
		   const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short cnt;
	unsigned short parse_data[4];
	unsigned short ret;

	memset(parse_data, 0, sizeof(unsigned short) * 4);
	cmd_parsing(buf, 3, parse_data);
	if (parse_data[1] != 1) {
		ADPD143_dbg("few many argument!!\n");
		goto err_reg_write_argument;
	}

	pst_adpd->sysfs_I2C_regaddr = parse_data[0];
	cnt = parse_data[1];
	pst_adpd->sysfs_I2C_regval = parse_data[2];
	mutex_lock(&pst_adpd->mutex);
	ret = adpd143_sysfs_I2C_write(pst_adpd);
	if (ret == pst_adpd->sysfs_I2C_regval) {
		ADPD143_dbg("Reg[0x%04x] = 0x%04x\n",
			pst_adpd->sysfs_I2C_regaddr,
			pst_adpd->sysfs_I2C_regval);
	} else {
		ADPD143_dbg("Reg write error!!\n");
	}

	adpd143_update_config(pst_adpd);

	mutex_unlock(&pst_adpd->mutex);
err_reg_write_argument:
	return size;
}

/**
This function is used for getting the status of configuration
TBD
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@return ssize_t size of data presnt in the buffer
*/
static ssize_t
attr_config_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "status of config thread\n");
}

/**
This function is used for wrting the configuration value to the register
0 - write the configuration data present in file to ADPD143
1 - write the configuration data present in Array to ADPD143
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@param size buffer size
@return ssize_t size of data written to the buffer
*/
static ssize_t
attr_config_set(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[1];
	memset(parse_data, 0, sizeof(unsigned short) * 1);
	cmd_parsing(buf, 1, parse_data);

	if (parse_data[0] == FRM_ARR)
		adpd143_configuration(pst_adpd, FRM_ARR);
	else if (parse_data[0] == FRM_FILE)
		adpd143_configuration(pst_adpd, FRM_FILE);
	else
		ADPD143_dbg("set 1 to config\n");
	return size;
}


/**
This function is used for getting the status of adpd143 and driver
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@return ssize_t size of data presnt in the buffer
*/
static ssize_t
attr_stat_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned int interrupts = atomic_read(&pst_adpd->stats.interrupts);
	unsigned int wq_pending = atomic_read(&pst_adpd->stats.wq_pending);
	unsigned int wq_schedule_time_peak_usec =
		atomic_read(&pst_adpd->stats.wq_schedule_time_peak_usec);
	unsigned int wq_schedule_time_last_usec =
		atomic_read(&pst_adpd->stats.wq_schedule_time_last_usec);
	unsigned int data_process_time_peak_usec =
		atomic_read(&pst_adpd->stats.data_process_time_peak_usec);
	unsigned int data_process_time_last_usec =
		atomic_read(&pst_adpd->stats.data_process_time_last_usec);

	return sprintf(buf, "\
		interrupts                  : %u\n\
		wq_pending                  : %u\n\
		wq_schedule_time_peak_usec  : %u\n\
		wq_schedule_time_avg_usec   : %d\n\
		wq_schedule_time_last_usec  : %u\n\
		data_process_time_peak_usec : %u\n\
		data_process_time_avg_usec  : %d\n\
		data_process_time_last_usec : %u\n\
		fifo_requires_sync          : %d\n\
		fifo bytes history          : [%d %d %d %d]\n\
		ADPD143 driver version      : %s\n\
		ADPD143 Release date        : %s\n",

		interrupts, wq_pending,
		wq_schedule_time_peak_usec,
		(int)ewma_read(&pst_adpd->stats.wq_schedule_time_avg_usec),
		wq_schedule_time_last_usec,
		data_process_time_peak_usec,
		(int)ewma_read(&pst_adpd->stats.data_process_time_avg_usec),
		data_process_time_last_usec,
		atomic_read(&pst_adpd->stats.fifo_requires_sync),
		atomic_read(&pst_adpd->stats.fifo_bytes[0]),
		atomic_read(&pst_adpd->stats.fifo_bytes[1]),
		atomic_read(&pst_adpd->stats.fifo_bytes[2]),
		atomic_read(&pst_adpd->stats.fifo_bytes[3]),
		ADPD143_VERSION,
		ADPD143_RELEASE_DATE);

}

#define ADPD143_STAT_RESET	1

/**
This function is used for wrting the adpd stat value to zero
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@param size buffer size
@return ssize_t size of data written to the buffer
*/
static ssize_t
attr_stat_set(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[1];

	memset(parse_data, 0, sizeof(unsigned short) * 1);
	cmd_parsing(buf, 1, parse_data);

	if (parse_data[0] == ADPD143_STAT_RESET) {
		ADPD143_dbg("Resetting statistics\n");
		adpd_stat_reset(pst_adpd);
	}

	return size;
}

/**
array of hrm attributes
*/
static struct device_attribute hrm_attributes[] = {
	__ATTR(enable, 0664, hrm_attr_get_enable, hrm_attr_set_enable),
};

/**
This function is used for creating sysfs attribute for hrm
@param dev linux device structure
@return int status of attribute creation
*/
static int
create_sysfs_interfaces_hrm(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hrm_attributes); i++)
		if (device_create_file(dev, hrm_attributes + i))
			goto err_sysfs_interface_hrm;
	return 0;
err_sysfs_interface_hrm:
	for (; i >= 0; i--)
		device_remove_file(dev, hrm_attributes + i);
	dev_err(dev, "%s: Unable to create interface\n", __func__);
	return -1;
}

/* SAMPLE - SYSFS ATTRIBUTE*/
/**
This function is used for getting the status of the sample enable bit
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@return ssize_t size of data presnt in the buffer
*/
static ssize_t
hrmled_attr_get_enable(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	int val = atomic_read(&pst_adpd->hrmled_enabled);
	return sprintf(buf, "%d\n", val);
}

/**
This function is used for enabling the Sample mode
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@param size buffer size
@return ssize_t size of data written to the buffer
*/
static ssize_t
hrmled_attr_set_enable(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[2];
	unsigned short mode = 0;
	int val;

	int n = sscanf(buf, "%d", &val);
	(void)n;
	memset(parse_data, 0, sizeof(parse_data));

	pr_err("%s: pst_adpd->pre_hrmled_enable:%d, val:%d\n",
		__func__, pst_adpd->pre_hrmled_enable, val);
	if (pst_adpd->pre_hrmled_enable != val) {
		switch(val) {
			case IRRED_OFF:
				pr_info("%s: IRRED_OFF.\n", __func__);
				cmd_parsing("0x0", 1, parse_data);
				atomic_set(&pst_adpd->hrmled_enabled, IRRED_OFF);
				break;
			case IR_MODE:
				pr_info("%s: IR_MODE.\n", __func__);
				cmd_parsing("0x32", 1, parse_data);
				atomic_set(&pst_adpd->hrmled_enabled, IR_MODE);
				break;
			case RED_MODE:
				pr_info("%s: RED_MODE.\n", __func__);
				cmd_parsing("0x30", 1, parse_data);
				atomic_set(&pst_adpd->hrmled_enabled, RED_MODE);
				break;
			case IRRED_MODE:
				pr_info("%s: IRRED_MODE.\n", __func__);
				cmd_parsing("0x31", 1, parse_data);
				atomic_set(&pst_adpd->hrmled_enabled, IRRED_MODE);
				break;
			default:
				pr_err("%s: invalid value\n", __func__);
				return -EINVAL;
		}
	}
	mode = GET_USR_MODE(parse_data[0]);

	if (GET_USR_MODE(parse_data[0]) < MAX_MODE) {
		if ((GET_USR_SUB_MODE(parse_data[0])) <
			__mode_recv_frm_usr[mode].size) {
			adpd143_mode_switching(pst_adpd, parse_data[0]);
		} else {
			ADPD143_dbg("Sub mode Out of bound\n");
			adpd143_mode_switching(pst_adpd, 0);
		}
	} else {
		ADPD143_dbg("Mode out of bound\n");
		adpd143_mode_switching(pst_adpd, 0);
	}

	pst_adpd->pre_hrmled_enable = val;
	return size;
}
/**
array of hrmled attributes
*/
static struct device_attribute hrmled_attributes[] = {
	__ATTR(enable, 0664, hrmled_attr_get_enable, hrmled_attr_set_enable),
};

/**
This function is used for creating sysfs attribute for hrmled
@param dev linux device structure
@return int status of attribute creation
*/
static int
create_sysfs_interfaces_hrmled(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hrmled_attributes); i++)
		if (device_create_file(dev, hrmled_attributes + i))
			goto err_sysfs_interface_hrmled;
	return 0;
err_sysfs_interface_hrmled:
	for (; i >= 0; i--)
		device_remove_file(dev, hrmled_attributes + i);
	dev_err(dev, "%s: Unable to create interface\n", __func__);
	return -1;
}

/**
This function is used for removing sysfs attribute for sample
@param dev linux device structure
@return void
*/
static void
remove_sysfs_interfaces_hrm(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hrm_attributes); i++)
		device_remove_file(dev, hrm_attributes + i);
}

/**
This function is used for removing sysfs attribute for sample
@param dev linux device structure
@return void
*/
static void
remove_sysfs_interfaces_hrmled(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hrmled_attributes); i++)
		device_remove_file(dev, hrmled_attributes + i);
}

/**
This function is used for creating sysfs attribute
@param dev linux device structure
@return int status of attribute creation
*/
static int
create_sysfs_interfaces(struct device *dev)
{
	return 0;
}

/**
This function is used for removing sysfs attribute
@param dev linux device structure
@return void
*/
static void
remove_sysfs_interfaces(struct device *dev)
{

}

/**
This function is used for registering input event for Sample
@param pst_adpd pointer point ADPD143 data structure
@return s32 status of this function
*/
static s32
adpd143_input_init_sample(struct adpd143_data *pst_adpd)
{
	int err;

	/* for hrm sensor */
	pst_adpd->hrm_input_dev = input_allocate_device();
	if (!pst_adpd->hrm_input_dev) {
		err = -ENOMEM;
		dev_err(&pst_adpd->client->dev,
			"hrm input dev allocation fail\n");
		goto err_hrm_allocate;
	}

	pst_adpd->hrm_input_dev->name = MODULE_NAME_HRM;
	input_set_drvdata(pst_adpd->hrm_input_dev, pst_adpd);

	__set_bit(EV_MSC, pst_adpd->hrm_input_dev->evbit);
	__set_bit(EV_REL, pst_adpd->hrm_input_dev->evbit);
	__set_bit(MSC_RAW, pst_adpd->hrm_input_dev->mscbit);
	__set_bit(REL_X, pst_adpd->hrm_input_dev->relbit);
	__set_bit(REL_Y, pst_adpd->hrm_input_dev->relbit);
	__set_bit(REL_Z, pst_adpd->hrm_input_dev->relbit);
	__set_bit(REL_MISC, pst_adpd->hrm_input_dev->relbit);

	err = input_register_device(pst_adpd->hrm_input_dev);
	if (err) {
		dev_err(&pst_adpd->client->dev,
			"unable to register input dev %s\n",
			pst_adpd->hrm_input_dev->name);
		goto err_hrm_register_failed;
	}

	err = sensors_create_symlink(pst_adpd->hrm_input_dev);
	if (err < 0) {
		pr_err("%s: create_symlink error\n", __func__);
		goto err_hrm_register_failed;
	}

	/* for hrmled sensor */
	pst_adpd->hrmled_input_dev = input_allocate_device();
	if (!pst_adpd->hrmled_input_dev) {
		err = -ENOMEM;
		dev_err(&pst_adpd->client->dev,
			"hrmled input dev allocation fail\n");
		goto err_hrmled_allocate;
	}

	pst_adpd->hrmled_input_dev->name = MODULE_NAME_HRMLED;
	input_set_drvdata(pst_adpd->hrmled_input_dev, pst_adpd);

	__set_bit(EV_MSC, pst_adpd->hrmled_input_dev->evbit);
	__set_bit(EV_REL, pst_adpd->hrmled_input_dev->evbit);
	__set_bit(MSC_RAW, pst_adpd->hrmled_input_dev->mscbit);
	__set_bit(REL_X, pst_adpd->hrmled_input_dev->relbit);
	__set_bit(REL_Y, pst_adpd->hrmled_input_dev->relbit);
	__set_bit(REL_Z, pst_adpd->hrmled_input_dev->relbit);
	__set_bit(REL_MISC, pst_adpd->hrmled_input_dev->relbit);

	err = input_register_device(pst_adpd->hrmled_input_dev);
	if (err) {
		dev_err(&pst_adpd->client->dev,
			"unable to register input dev %s\n",
			pst_adpd->hrmled_input_dev->name);
		goto err_hrmled_register_failed;
	}

	err = sensors_create_symlink(pst_adpd->hrmled_input_dev);
	if (err < 0) {
		pr_err("%s: create_symlink error\n", __func__);
		goto err_hrmled_register_failed;
	}

	return 0;
err_hrmled_register_failed:
	input_free_device(pst_adpd->hrmled_input_dev);
err_hrmled_allocate:
err_hrm_register_failed:
	input_free_device(pst_adpd->hrm_input_dev);
err_hrm_allocate:
	return err;
}

/**
This function is used for unregistering input event for Sample
@param pst_adpd pointer point ADPD143 data structure
@return void
*/
static void
adpd143_input_cleanup_sample(struct adpd143_data *pst_adpd)
{
	input_set_drvdata(pst_adpd->hrm_input_dev, NULL);
	input_unregister_device(pst_adpd->hrm_input_dev);
	input_free_device(pst_adpd->hrm_input_dev);

	input_set_drvdata(pst_adpd->hrmled_input_dev, NULL);
	input_unregister_device(pst_adpd->hrmled_input_dev);
	input_free_device(pst_adpd->hrmled_input_dev);
}

/**
This function is used for registering input event for ADPD143
@param pst_adpd pointer point ADPD143 data structure
@return s32 status of the function
*/
static s32
adpd143_input_init(struct adpd143_data *pst_adpd)
{
	return adpd143_input_init_sample(pst_adpd);
}

/**
This function is used for unregistering input event done for ADPD143
@param pst_adpd pointer point ADPD143 data structure
@return void
*/
static void
adpd143_input_cleanup(struct adpd143_data *pst_adpd)
{
	adpd143_input_cleanup_sample(pst_adpd);
}

/**
This function is used for registering sysfs attribute for ADPD143
@param pst_adpd pointer point ADPD143 data structure
@return s32 status of the called function
*/
static s32
adpd143_sysfs_init(struct adpd143_data *pst_adpd)
{
	if (create_sysfs_interfaces(&pst_adpd->client->dev))
		goto err_sysfs_create_gen;

	if (create_sysfs_interfaces_hrm(&pst_adpd->hrm_input_dev->dev))
		goto err_sysfs_create_sample;

	if (create_sysfs_interfaces_hrmled(&pst_adpd->hrmled_input_dev->dev))
		goto err_sysfs_create_sample;

	return 0;
err_sysfs_create_sample:

	remove_sysfs_interfaces(&pst_adpd->client->dev);
err_sysfs_create_gen:
	return -1;
}

/**
This function is used for unregistering sysfs attribute for ADPD143
@param pst_adpd pointer point ADPD143 data structure
@return void
*/
static void
adpd143_sysfs_cleanup(struct adpd143_data *pst_adpd)
{
	remove_sysfs_interfaces(&pst_adpd->client->dev);
	remove_sysfs_interfaces_hrm(&pst_adpd->hrm_input_dev->dev);
	remove_sysfs_interfaces_hrmled(&pst_adpd->hrmled_input_dev->dev);
}

/**
This function is used for assigning initial assignment value to
ADPD143 data structure
@param pst_adpd pointer point ADPD143 data structure
@return void
*/
static void
adpd143_struct_assign(struct adpd143_data *pst_adpd)
{
	pst_adpd->ptr_data = data_buffer;
}

/**
This function is used for initializing ADPD143
@param pst_adpd pointer point ADPD143 data structure
@param id pointer point i2c device id
@return s32 status of the called function
*/
static s32
adpd143_initialization(struct adpd143_data *pst_adpd,
			const struct i2c_device_id *id)
{
	int err = 0;

	if (adpd143_input_init(pst_adpd)) {
		err = -1;
		pr_err("%s: err line %d\n", __func__, __LINE__);
		goto err_input_init;
	}
	if (adpd143_sysfs_init(pst_adpd)) {
		pr_err("%s: err line %d\n", __func__, __LINE__);
		err = -1;
		goto err_sysfs_init;
	}

	adpd143_struct_assign(pst_adpd);

	memset(&pst_adpd->stats, 0, sizeof(pst_adpd->stats));
	adpd_stat_reset(pst_adpd);

	INIT_WORK(&pst_adpd->work, adpd143_wq_handler);
	pst_adpd->ptr_adpd143_wq_st =
		create_workqueue("adpd143_wq");
	if (!pst_adpd->ptr_adpd143_wq_st) {
		err = -ENOMEM;
		pr_err("%s %d\n", __func__, __LINE__);
		goto err_wq_creation_init;
	}

	if (!pst_adpd->client->irq) {
		pr_err("%s %d\n", __func__, __LINE__);
		goto err_work_queue_init;
	}
	pst_adpd->hrm_int = pst_adpd->client->irq;
	pst_adpd->irq = pst_adpd->hrm_int;

	irq_set_irq_type(pst_adpd->irq, IRQ_TYPE_EDGE_FALLING);
	err = request_irq(pst_adpd->irq, adpd143_isr_handler,
			  IRQF_TRIGGER_FALLING, dev_name(&pst_adpd->client->dev),
			  pst_adpd);
	if (err) {
		ADPD143_dbg("irq %d busy?\n", pst_adpd->irq);
		goto err_work_queue_init;
	}
	disable_irq_nosync(pst_adpd->irq);

	pst_adpd->ptr_config = kzalloc(sizeof(struct adpd_platform_data),
					   GFP_KERNEL);
	if (pst_adpd->ptr_config == NULL) {
		err = -ENOMEM;
		pr_err("%s %d\n", __func__, __LINE__);
		goto err_work_queue_init;
	}

	enable_irq(pst_adpd->irq);
	adpd143_configuration(pst_adpd, 1);
	adpd143_mode_switching(pst_adpd, 0); /* turn off : idle mode, temp*/
	pst_adpd->osc_trim_open_enable = 1;
	pst_adpd->pre_hrmled_enable = 0;
	pst_adpd->sample_cnt = 0;

	return 0;
//err_gpio_direction_input:
	//gpio_free(pst_adpd->hrm_int);
err_work_queue_init:
	destroy_workqueue(pst_adpd->ptr_adpd143_wq_st);
err_wq_creation_init:

err_sysfs_init:
	adpd143_input_cleanup(pst_adpd);
err_input_init:
	return err;
}

/**
This function is used for cleanup ADPD143
@param pst_adpd pointer point ADPD143 data structure
@return void
*/
static void
adpd143_initialization_cleanup(struct adpd143_data *pst_adpd)
{
	adpd143_mode_switching(pst_adpd, 0);

	free_irq(pst_adpd->irq, pst_adpd);

	destroy_workqueue(pst_adpd->ptr_adpd143_wq_st);
	adpd143_sysfs_cleanup(pst_adpd);
	adpd143_input_cleanup(pst_adpd);
	kobject_uevent(&pst_adpd->client->dev.kobj, KOBJ_OFFLINE);
}

#ifdef CONFIG_PM
static s32
adpd143_i2c_suspend(struct device *dev)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[2];
	unsigned short mode = 0;

	pr_info("%s \n", __func__);
	adpd143_pin_control(pst_adpd, false);

	memset(parse_data, 0, sizeof(parse_data));
	if (atomic_read(&pst_adpd->hrmled_enabled) ||
		atomic_read(&pst_adpd->hrm_enabled)) {
		cmd_parsing("0x0", 1, parse_data);
		mode = GET_USR_MODE(parse_data[0]);

		if (GET_USR_MODE(parse_data[0]) < MAX_MODE) {
			if ((GET_USR_SUB_MODE(parse_data[0])) <
				__mode_recv_frm_usr[mode].size) {
				adpd143_mode_switching(pst_adpd, parse_data[0]);
			} else {
				ADPD143_dbg("Sub mode Out of bound\n");
				adpd143_mode_switching(pst_adpd, 0);
			}
		} else {
			ADPD143_dbg("Mode out of bound\n");
			adpd143_mode_switching(pst_adpd, 0);
		}
	}

	return 0;
}

static s32
adpd143_i2c_resume(struct device *dev)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[2];
	unsigned short mode = 0;

	pr_info("%s \n", __func__);
	adpd143_pin_control(pst_adpd, true);

	memset(parse_data, 0, sizeof(parse_data));
	if (atomic_read(&pst_adpd->hrm_enabled)) {
		switch (atomic_read(&pst_adpd->hrm_enabled)) {
		case HRM_SAMPLE_MODE:
			cmd_parsing("0x31", 1, parse_data);
			break;
		case HRM_TIA_ADC_MODE:
			cmd_parsing("0x51", 1, parse_data);
			break;
		default:
			pr_err("%s: invalid value\n", __func__);
			return -EINVAL;
		}
		goto mode_switching;

	}

	if (atomic_read(&pst_adpd->hrmled_enabled)) {
		switch (atomic_read(&pst_adpd->hrmled_enabled)) {
		case IR_MODE:
			pr_info("%s: IR_MODE.\n", __func__);
			cmd_parsing("0x32", 1, parse_data);
			break;
		case RED_MODE:
			pr_info("%s: RED_MODE.\n", __func__);
			cmd_parsing("0x30", 1, parse_data);
			break;
		case IRRED_MODE:
			pr_info("%s: IRRED_MODE.\n", __func__);
			cmd_parsing("0x31", 1, parse_data);
			break;
		default:
			pr_err("%s: invalid value\n", __func__);
			return -EINVAL;
		}
		goto mode_switching;
	}

	ADPD143_dbg("No active hrm sensor\n");
	return 0;
mode_switching:
	mode = GET_USR_MODE(parse_data[0]);

	if (GET_USR_MODE(parse_data[0]) < MAX_MODE) {
		if ((GET_USR_SUB_MODE(parse_data[0])) <
			__mode_recv_frm_usr[mode].size) {
			adpd143_mode_switching(pst_adpd, parse_data[0]);
		} else {
			ADPD143_dbg("Sub mode Out of bound\n");
			adpd143_mode_switching(pst_adpd, 0);
		}
	} else {
		ADPD143_dbg("Mode out of bound\n");
		adpd143_mode_switching(pst_adpd, 0);
	}

	return 0;
}
#else
#define adpd143_i2c_resume  NULL
#define adpd143_i2c_suspend NULL
#endif			  /* CONFIG_PM */


/**
This function is used for i2c read communication between ADPD143 and AP
@param pst_adpd pointer point ADPD143 data structure
@param reg_addr address need to be fetch
@param len number byte to be read
@param buf pointer point the read out data.
@return s32 status of the called function
*/
static int
adpd143_i2c_read(struct adpd143_data *pst_adpd, u8 reg_addr, int len,
		  u16 *buf)
{
	int err;
	int tries = 0;
	int icnt = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = pst_adpd->client->addr,
		 .flags = pst_adpd->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = (s8 *)&reg_addr,
		 },
		{
		 .addr = pst_adpd->client->addr,
			.flags = (pst_adpd->client->flags & I2C_M_TEN) |
				I2C_M_RD,
		 .len = len * sizeof(unsigned short),
		 .buf = (s8 *)buf,
		 },
	};

	do {
		err = i2c_transfer(pst_adpd->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
		if (err < 0)
			pr_err("%s , i2c_transfer error = %d\n", __func__, err);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&pst_adpd->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	for (icnt = 0; icnt < len; icnt++) {
		/*convert big endian to CPU format*/
		buf[icnt] = be16_to_cpu(buf[icnt]);
	}

	return err;
}

/**
This function is used for i2c write communication between ADPD143 and AP
@param pst_adpd pointer point ADPD143 data structure
@param reg_addr address need to be fetch
@param len number byte to be read
@param data value to be written on the register.
@return s32 status of the called function
*/
static int
adpd143_i2c_write(struct adpd143_data *pst_adpd, u8 reg_addr, int len,
		   u16 data)
{
	struct i2c_msg msgs[1];
	int err;
	int tries = 0;
	unsigned short data_to_write = cpu_to_be16(data);
	char buf[4];

	buf[0] = (s8) reg_addr;
	memcpy(buf + 1, &data_to_write, sizeof(unsigned short));
	msgs[0].addr = pst_adpd->client->addr;
	msgs[0].flags = pst_adpd->client->flags & I2C_M_TEN;
	msgs[0].len = 1 + (1 * sizeof(unsigned short));
	msgs[0].buf = buf;

	do {
		err = i2c_transfer(pst_adpd->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
		if (err < 0)
			pr_err("%s , i2c_transfer error = %d\n", __func__, err);

	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&pst_adpd->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

/* set sysfs for hrm sensor */
static ssize_t adpd143_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s: %s \n", __func__, CHIP_NAME);

	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t adpd143_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s: %s \n", __func__, VENDOR);

	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t eol_test_result_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t eol_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* eol_test start */
		pst_adpd->eol_test_is_enable = 1;
	else if (sysfs_streq(buf, "0")) /* eol_test stop */
		pst_adpd->eol_test_is_enable = 0;
	else {
		pr_debug("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pst_adpd->eol_test_status = 0;
	pr_info("%s: (%c), value(%u) \n",
		__func__, *buf, pst_adpd->eol_test_is_enable);

	return size;
}

static ssize_t eol_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", pst_adpd->eol_test_is_enable);
}

static ssize_t eol_test_result_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	int err;
	unsigned int buf_len;

	buf_len = strlen(buf) + 1;
	if (buf_len > MAX_EOL_RESULT)
		buf_len = MAX_EOL_RESULT;

	mutex_lock(&pst_adpd->storelock);

	if (pst_adpd->eol_test_result != NULL)
		kfree(pst_adpd->eol_test_result);

	pst_adpd->eol_test_result = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (pst_adpd->eol_test_result == NULL) {
		pr_err("%s: couldn't allocate memory\n", __func__);
		mutex_unlock(&pst_adpd->storelock);
		return -ENOMEM;
	}
	strncpy(pst_adpd->eol_test_result, buf, buf_len);

	mutex_unlock(&pst_adpd->storelock);

	pr_info("%s: result = %s, buf_len(%u)\n",
		__func__, pst_adpd->eol_test_result, buf_len);
	pst_adpd->eol_test_status = 1;

	err = osc_trim_efs_register_save(pst_adpd);
	if (err < 0) {
		pr_err("%s: osc_trim_efs_register_save() empty(%d)\n",
			__func__, err);
	}

	return size;
}

static ssize_t eol_test_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->eol_test_result == NULL) {
		pr_info("%s: data->eol_test_result is NULL\n",
			__func__);
		pst_adpd->eol_test_status = 0;
		return snprintf(buf, PAGE_SIZE, "%s\n", "NO_EOL_TEST");
	}
	pr_info("%s: result = %s\n",
		__func__, pst_adpd->eol_test_result);
	pst_adpd->eol_test_status = 0;
	return snprintf(buf, PAGE_SIZE, "%s\n", pst_adpd->eol_test_result);
}

static ssize_t eol_test_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", pst_adpd->eol_test_status);
}

static ssize_t adpd143_eol_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	unsigned int buf_len;
	buf_len = strlen(buf) + 1;
	if (buf_len > MAX_LIB_VER)
		buf_len = MAX_LIB_VER;

	mutex_lock(&pst_adpd->storelock);

	if (pst_adpd->eol_lib_ver != NULL)
		kfree(pst_adpd->eol_lib_ver);

	pst_adpd->eol_lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (pst_adpd->eol_lib_ver == NULL) {
		pr_err("%s: couldn't allocate memory\n", __func__);
		mutex_unlock(&pst_adpd->storelock);
		return -ENOMEM;
	}
	strncpy(pst_adpd->eol_lib_ver, buf, buf_len);

	mutex_unlock(&pst_adpd->storelock);
	pr_info("%s: eol_lib_ver = %s\n", __func__, pst_adpd->eol_lib_ver);
	return size;
}

static ssize_t adpd143_eol_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->eol_lib_ver == NULL) {
		pr_info("%s: data->eol_lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s: eol_lib_ver = %s\n", __func__, pst_adpd->eol_lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", pst_adpd->eol_lib_ver);
}

static ssize_t adpd143_elf_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	unsigned int buf_len;
	buf_len = strlen(buf) + 1;
	if (buf_len > MAX_LIB_VER)
		buf_len = MAX_LIB_VER;

	mutex_lock(&pst_adpd->storelock);

	if (pst_adpd->elf_lib_ver != NULL)
		kfree(pst_adpd->elf_lib_ver);

	pst_adpd->elf_lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (pst_adpd->elf_lib_ver == NULL) {
		pr_err("%s: couldn't allocate memory\n", __func__);
		mutex_unlock(&pst_adpd->storelock);
		return -ENOMEM;
	}
	strncpy(pst_adpd->elf_lib_ver, buf, buf_len);

	mutex_unlock(&pst_adpd->storelock);
	return size;
}

static ssize_t adpd143_elf_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->elf_lib_ver == NULL) {
		pr_info("%s: data->elf_lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s: elf_lib_ver = %s\n", __func__, pst_adpd->elf_lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", pst_adpd->elf_lib_ver);
}

static ssize_t adpd143_hrm_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	int ret = 0;
	u8 handle = 0;

	ret = kstrtou8(buf, 10, &handle);
	if (ret < 0) {
		pr_err("%s: kstrtou8 failed.(%d)\n", __func__, ret);
		return ret;
	}
	pr_info("%s: handle = %d\n", __func__, handle);

	input_report_rel(pst_adpd->hrm_input_dev, REL_MISC, handle);
	return size;
}


static ssize_t adpd143_alc_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	u8 val;
	unsigned short mode = 0;
	unsigned short parse_data[2];
	int rc = 0;

	rc = kstrtou8(buf, 10, &val);
	pr_info("%s: enable Ambient Light Measurement val:%d.\n",
		__func__, val);

	if (val >= ALC_TIA_MODE && val <= ALC_FLOAT_MODE) {
		if (pst_adpd->led_en != -1)
			gpio_set_value(pst_adpd->led_en, 1);
		else if (!IS_ERR(pst_adpd->leda))
			rc = regulator_enable(pst_adpd->leda);
			if (rc) {
				pr_info(KERN_ERR "%s: enable ALC fail(%d)\n",
					__func__, rc);
			}
		switch(val) {
		case ALC_TIA_MODE:
			pr_info("%s: mode:%d\n", __func__, ALC_TIA_MODE);
			cmd_parsing("0x50", 1, parse_data);
			break;
		case ALC_TNF_MODE:
			pr_info("%s: mode:%d\n", __func__, ALC_TNF_MODE);
			cmd_parsing("0x51", 1, parse_data);
			break;
		case ALC_FLOAT_MODE:
			pr_info("%s: mode:%d\n", __func__, ALC_FLOAT_MODE);
			cmd_parsing("0x52", 1, parse_data);
			break;
		default:
			pr_info("%s: invalid mode\n", __func__);
			cmd_parsing("0x0", 1, parse_data);
			break;
		}
		atomic_set(&pst_adpd->hrm_enabled, 0);
	} else {
		pr_info("%s: disable.\n", __func__);
		if (pst_adpd->led_en != -1)
			gpio_set_value(pst_adpd->led_en, 0);
		else if (!IS_ERR(pst_adpd->leda))
			rc = regulator_disable(pst_adpd->leda);
			if (rc) {
				pr_info(KERN_ERR "%s: disable failed (%d)\n",
					__func__, rc);
			}
		cmd_parsing("0x0", 1, parse_data);
		atomic_set(&pst_adpd->hrm_enabled, 0);
	}

	mode = GET_USR_MODE(parse_data[0]);

	if (GET_USR_MODE(parse_data[0]) < MAX_MODE) {
		if ((GET_USR_SUB_MODE(parse_data[0])) <
			__mode_recv_frm_usr[mode].size) {
			adpd143_mode_switching(pst_adpd, parse_data[0]);
		} else {
			ADPD143_dbg("Sub mode Out of bound\n");
			adpd143_mode_switching(pst_adpd, 0);
		}
	} else {
		ADPD143_dbg("Mode out of bound\n");
		adpd143_mode_switching(pst_adpd, 0);
	}
	return size;
}

static ssize_t adpd143_alc_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->elf_lib_ver == NULL) {
		pr_info("%s: data->elf_lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s: elf_lib_ver = %s\n", __func__, pst_adpd->elf_lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", pst_adpd->elf_lib_ver);
}

static ssize_t adpd143_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	int err = 0;

	err = kstrtoint(buf, 10, &pst_adpd->threshold);
	if (err < 0) {
		pr_err("%s - kstrtoint failed.(%d)\n", __func__, err);
		return err;
	}

	pr_info("%s - threshold = %d\n", __func__, pst_adpd->threshold);
	return size;
}

static ssize_t adpd143_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->threshold) {
		pr_info("%s - threshold = %d\n", __func__, pst_adpd->threshold);
		return snprintf(buf, PAGE_SIZE, "%d\n", pst_adpd->threshold);
	} else {
		pr_info("%s - threshold = 0\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
}

static DEVICE_ATTR(name, S_IRUGO, adpd143_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, adpd143_vendor_show, NULL);
static DEVICE_ATTR(eol_test, S_IRUGO | S_IWUSR | S_IWGRP,
	eol_test_show, eol_test_store);
static DEVICE_ATTR(eol_test_result, S_IRUGO | S_IWUSR | S_IWGRP,
	eol_test_result_show, eol_test_result_store);
static DEVICE_ATTR(eol_test_status, S_IRUGO, eol_test_status_show, NULL);
static DEVICE_ATTR(eol_lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	adpd143_eol_lib_ver_show, adpd143_eol_lib_ver_store);
static DEVICE_ATTR(elf_lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	adpd143_elf_lib_ver_show, adpd143_elf_lib_ver_store);
static DEVICE_ATTR(adpd_mode, S_IRUGO | S_IWUSR | S_IWGRP,
	attr_get_mode, attr_set_mode);
static DEVICE_ATTR(adpd_reg_read, S_IRUGO | S_IWUSR | S_IWGRP,
	attr_reg_read_get, attr_reg_read_set);
static DEVICE_ATTR(adpd_reg_write, S_IRUGO | S_IWUSR | S_IWGRP,
	NULL, attr_reg_write_set);
static DEVICE_ATTR(adpd_configuration, S_IRUGO | S_IWUSR | S_IWGRP,
	attr_config_get, attr_config_set);
static DEVICE_ATTR(adpd_stat, S_IRUGO | S_IWUSR | S_IWGRP,
	attr_stat_get, attr_stat_set);
static DEVICE_ATTR(hrm_flush, S_IWUSR | S_IWGRP,
	NULL, adpd143_hrm_flush_store);
static DEVICE_ATTR(alc_enable, S_IRUGO | S_IWUSR | S_IWGRP,
	adpd143_alc_enable_show, adpd143_alc_enable_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
	adpd143_threshold_show, adpd143_threshold_store);

static struct device_attribute *hrm_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_eol_test,
	&dev_attr_eol_test_result,
	&dev_attr_eol_test_status,
	&dev_attr_eol_lib_ver,
	&dev_attr_elf_lib_ver,
	&dev_attr_adpd_mode,
	&dev_attr_adpd_reg_read,
	&dev_attr_adpd_reg_write,
	&dev_attr_adpd_configuration,
	&dev_attr_adpd_stat,
	&dev_attr_hrm_flush,
	&dev_attr_alc_enable,
	&dev_attr_threshold,
	NULL,
};

static ssize_t adpd143_hrmled_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t adpd143_hrmled_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t adpd143_hrmled_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd143_data *pst_adpd = dev_get_drvdata(dev);
	int ret = 0;
	u8 handle = 0;

	ret = kstrtou8(buf, 10, &handle);
	if (ret < 0) {
		pr_err("%s: kstrtou8 failed.(%d)\n", __func__, ret);
		return ret;
	}
	pr_info("%s: handle = %d\n", __func__, handle);

	input_report_rel(pst_adpd->hrmled_input_dev, REL_MISC, handle);
	return size;
}

static struct device_attribute dev_attr_hrmled_name =
		__ATTR(name, S_IRUGO, adpd143_hrmled_name_show, NULL);
static struct device_attribute dev_attr_hrmled_vendor =
		__ATTR(vendor, S_IRUGO, adpd143_hrmled_vendor_show, NULL);
static DEVICE_ATTR(hrmled_flush, S_IWUSR | S_IWGRP,
	NULL, adpd143_hrmled_flush_store);

static struct device_attribute *hrmled_sensor_attrs[] = {
	&dev_attr_hrmled_name,
	&dev_attr_hrmled_vendor,
	&dev_attr_hrmled_flush,
	NULL,
};

/**
This function is used for ADPD143 probe function
@param client pointer point to the linux i2c client structure
@param id pointer point to the linux i2c device id
@return s32 status of the probe function
*/
static s32
adpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct adpd143_data *pst_adpd = NULL;
	struct adpd_platform_data *pdata = NULL;
	int err = 0, i = 0;
	unsigned short u16_regval = 0;
	struct regulator *regulator_i2c_1p8;
	
	pr_err("%s: is called Success\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		pr_err("[SENSOR] %s: exit_check_functionality.\n", __func__);
		goto exit_check_functionality_failed;
	}

	pst_adpd = kzalloc(sizeof(struct adpd143_data), GFP_KERNEL);
	if (pst_adpd == NULL) {
		err = -ENOMEM;
		pr_err("[SENSOR] %s: exit_mem_allocate.\n", __func__);
		goto exit_mem_allocate_failed;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc (&client->dev ,
			sizeof(struct adpd_platform_data), GFP_KERNEL);
		if (!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
			if (pst_adpd)
				kfree(pst_adpd);
		return -ENOMEM;
		}
	} else
		pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		if (pst_adpd)
			kfree(pst_adpd);
		return err;
	}

	err = adpd_parse_dt(pst_adpd, &client->dev);
	if (err) {
		pr_err("%s: parse dt fail\n", __func__);
		goto adpd_parse_dt_err;
	}

	if (pst_adpd->i2c_1p8 != NULL) {
		regulator_i2c_1p8 = regulator_get(NULL, pst_adpd->i2c_1p8);
		if (IS_ERR(regulator_i2c_1p8) || regulator_i2c_1p8 == NULL) {
			pr_err("%s - i2c_1p8 regulator_get fail\n", __func__);
			goto err_regulator_enable;
		}

		if (!regulator_is_enabled(regulator_i2c_1p8)) {
			err = regulator_enable(regulator_i2c_1p8);
			if (err) {
				pr_err("enable i2c_1p8 failed, err=%d\n", err);
				goto err_regulator_enable;
			}
		}
	}

	adpd_power_enable(pst_adpd, 1);
	err = adpd_power_enable(pst_adpd, 1);
	if (err < 0)
		pr_err("%s: adpd_power_enable fail err = %d\n", __func__, err);
	msleep(100);

	mutex_init(&pst_adpd->mutex);
	mutex_init(&pst_adpd->storelock);

	pst_adpd->client = client;
	pst_adpd->ptr_config = pdata;
	pst_adpd->read = adpd143_i2c_read;
	pst_adpd->write = adpd143_i2c_write;
	pst_adpd->threshold = 0;
	/*Need to allocate and assign data then use the below function */
	i2c_set_clientdata(client, (struct adpd143_data *)pst_adpd);
	/*chip ID verification */
	u16_regval = reg_read(pst_adpd, ADPD_CHIPID_ADDR);

	switch (u16_regval) {
	case ADPD_CHIPID(3):
	case ADPD_CHIPID(4):
		for(i=0; i<8; i++) rawDarkCh[i] = rawDataCh[i] = 0;
		err = 0;
		ADPD143_dbg("chipID value = 0x%x\n", u16_regval);
	break;
	default:
		err = 1;
	break;
	};
	if (err) {
		ADPD143_dbg("chipID value = 0x%x\n", u16_regval);
		pr_err("[SENSOR] %s: exit_chipid_verification.\n", __func__);
		goto exit_chipid_verification;
	}
	ADPD143_info("chipID value = 0x%x\n", u16_regval);

	//pst_adpd->dev = &client->dev;
	pr_info("%s: start \n", __func__);

	if (adpd143_initialization(pst_adpd, id)) {
		pr_err("[SENSOR] %s: exit_adpd143_init_exit.\n", __func__);
		goto exit_adpd143_init;
	}

	/* set sysfs for hrm sensor */
	err = sensors_register(&pst_adpd->dev, pst_adpd, hrm_sensor_attrs,
		"hrm_sensor");
	if (err) {
		pr_err("[SENSOR] %s: cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	/* set sysfs for hrm sensor */
	err = sensors_register(&pst_adpd->dev, pst_adpd, hrmled_sensor_attrs,
		"hrmled_sensor");
	if (err) {
		pr_err("[SENSOR] %s: cound not register hrmled_sensor(%d).\n",
			__func__, err);
		goto hrmled_sensor_register_failed;
	}

	pr_info("%s: success\n", __func__);

	goto done;
hrmled_sensor_register_failed:
	sensors_unregister(pst_adpd->dev, hrm_sensor_attrs);
hrm_sensor_register_failed:
exit_adpd143_init:
exit_chipid_verification:
	mutex_destroy(&pst_adpd->mutex);
	adpd_power_enable(pst_adpd, 0);
err_regulator_enable:
adpd_parse_dt_err:
exit_mem_allocate_failed:
	kfree(pst_adpd);
exit_check_functionality_failed:
	dev_err(&client->dev, "%s: Driver Init failed\n", ADPD_DEV_NAME);
	pr_err("%s: failed\n", __func__);

done:
	return err;
}

/**
This function is used for ADPD143 remove function
@param client pointer point to the linux i2c client structure
@return s32 status of the remove function
*/
static s32
adpd_i2c_remove(struct i2c_client *client)
{
	struct adpd143_data *pst_adpd = i2c_get_clientdata(client);
	ADPD143_dbg("%s\n", __func__);
	adpd143_initialization_cleanup(pst_adpd);
	gpio_free(pst_adpd->hrm_int);
	kfree(pst_adpd->ptr_config);
	kfree(pst_adpd);
	pst_adpd = NULL;

	i2c_set_clientdata(client, NULL);

	return 0;
}

#ifdef CONFIG_PM
/**
	device power management operation structure
*/
static const struct dev_pm_ops adpd_pm_ops = {
	.resume = adpd143_i2c_resume,
	.suspend = adpd143_i2c_suspend,
};
#endif

/**
  This table tell which framework it supported
  @brief the name has to get matched to the board configuration file setup
 */
/*static struct i2c_device_id adpd_id[] = { {ADPD_DEV_NAME, 0}, {} };

MODULE_DEVICE_TABLE(i2c, adpd_id);*/

static struct of_device_id adpd_match_table[] = {
	{ .compatible = "adpd143",},
	{},
};

MODULE_DEVICE_TABLE(i2c, adpd_match_table);

static const struct i2c_device_id adpd143_device_id[] = {
	{ "adpd143", 0 },
	{ }
};

/**
  i2c operation structure
*/
struct i2c_driver adpd143_i2c_driver = {
	.driver = {
		.name = ADPD_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &adpd_pm_ops,
#endif
			.of_match_table = adpd_match_table,
	},
	.probe = adpd_i2c_probe,
	.remove = adpd_i2c_remove,
	.id_table = adpd143_device_id,
};

static struct i2c_client *i2c_client;

#ifdef ADPD_AUTO_PROBE
static int
i2c_check_dev_attach(char *slave_name, unsigned short *slave_addrs,
			 unsigned short cnt)
{
	struct i2c_board_info info;
	struct i2c_adapter *i2c_adapter = NULL;
	int ret = 0;
	unsigned short *scan_device = NULL;
	unsigned short count = 0;

	/*need to check whether we need to free the memory */
	scan_device = kzalloc(sizeof(unsigned short) * (cnt + 2), GFP_KERNEL);
	if (IS_ERR(scan_device)) {
		ret = -ENOMEM;  /* out of memory */
		goto i2c_check_attach_mem_fail;
	}

	memset(scan_device, '\0', sizeof(unsigned short) * (cnt + 2));
	for (count = 0; count < cnt; count++) {
		*(scan_device + count) = *(slave_addrs + count);
		ADPD143_info("list of slave addr = 0x%x\n",
				  *(scan_device + count));
	}
	*(scan_device + count) = I2C_CLIENT_END;

	count = 0;

	do {
		i2c_adapter = i2c_get_adapter(count);
		if (i2c_adapter != NULL) {
			memset(&info, 0, sizeof(struct i2c_board_info));
			strlcpy(info.type, slave_name /*"adpd143" */ ,
				I2C_NAME_SIZE);
			/*need to check i2c_new_device instead of
			i2c_new_probed_device*/
			i2c_client =
				i2c_new_probed_device(i2c_adapter, &info,
						  (const unsigned short *)
						  scan_device, NULL);
			if (i2c_client != NULL) {
				ADPD143_dbg("I2C busnum - %d\n", count);
				ADPD143_dbg(
				"device is attached to the bus i2c-%d\n",
					 count);
			} else {

			}
			i2c_put_adapter(i2c_adapter);
		} else {
			ADPD143_info("Not valid adapter\n");
		}
		count++;
	} while (i2c_client == NULL && count < 20);

	kfree(scan_device);

	if (i2c_client == NULL) {
		/*No such device or address */
		return -ENXIO;
	} else {
		return 0;
	}

i2c_check_attach_mem_fail:
	return ret;
}
#endif

/**
This function is get called when the module is inserted
@return inti status of the adpd143_multisensor_init
*/
static int __init
adpd143_multisensor_init(void)
{
#ifdef ADPD_AUTO_PROBE
	ADPD143_dbg("%s\n", __func__);
	unsigned short addr[] = { ADPD143_SLAVE_ADDR };
	if (!i2c_check_dev_attach(ADPD_DEV_NAME, addr, 1)) {
		return i2c_add_driver(&adpd143_i2c_driver);
	} else {
		pr_err("i2c bus connect error\n");
		return -1;
	}
#else
	ADPD143_dbg("%s\n", __func__);
	return i2c_add_driver(&adpd143_i2c_driver);
#endif
}

/**
This function is get called when the module is removed
@return void
*/
static void __exit
adpd143_multisensor_exit(void)
{
	ADPD143_dbg("%s\n", __func__);
	i2c_del_driver(&adpd143_i2c_driver);
	if (i2c_client)
		i2c_unregister_device(i2c_client);
}

module_init(adpd143_multisensor_init);
module_exit(adpd143_multisensor_exit);
MODULE_DESCRIPTION();
MODULE_LICENSE("GPL");
MODULE_AUTHOR("");
