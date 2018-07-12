/**
 *\mainpage
 * ADPD142 driver
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
#include <linux/sensor/adpd142.h>
#include <linux/regulator/consumer.h>
#include <linux/fs.h>
#include "ssp.h"

static int dbg_enable;

module_param(dbg_enable, int, S_IRUGO | S_IWUSR);

/* ADPD142 driver version*/
#define ADPD142_VERSION			"1.2"
/* ADPD142 release date*/
#define ADPD142_RELEASE_DATE		"Tue Apr 22 12:11:42 IST 2014"


#define ADPD_DEV_NAME			"adpd142"

/*INPUT DEVICE NAME LIST*/
#define MODULE_NAME_HRM			"hrm_sensor"
#define CHIP_NAME				"ADPD142RI"
#define VENDOR				"ADI"

#define MAX_EOL_RESULT			132

/*I2C RELATED*/
#define I2C_RETRY_DELAY			5
#define I2C_RETRIES			5

/*ADPD142 DRIVER RELATED*/
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
/**************** ADPD142 USER OPERATING MODE ********************/
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
/**************** ADPD142 OPERATING MODE & DATA MODE *************/
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
/**************** ADPD142 INTERRUPT MASK *************************/
#define INTR_MASK_IDLE_USR		0x0000
#define INTR_MASK_SAMPLE_USR		0x0020
#define INTR_MASK_S_SAMP_XY_A_USR	0x0020
#define INTR_MASK_S_SAMP_XY_AB_USR	0x0040
#define	INTR_MASK_S_SAMP_XY_B_USR	0x0040
#define SET_INTR_MASK(usr_val)		((~(INTR_MASK_##usr_val)) & 0x1FF)
/******************************************************************/
/***************** ADPD142 PRINT *********************************/
#ifdef ADPD_DBG
#define ADPD142_dbg(format, arg...)	\
				printk(KERN_DEBUG "ADPD142 : "format, ##arg);
#else
#define ADPD142_dbg(format, arg...)    {if (dbg_enable)\
				printk(KERN_DEBUG "ADPD142 : "format, ##arg);\
					}
#endif

#ifdef ADPD_INFO
#define ADPD142_info(format, arg...)	\
				printk(KERN_INFO "ADPD142 : "format, ##arg);
#else
#define ADPD142_info(format, arg...)   {if (0)\
					; }
#endif
/******************************************************************/
/***************** ADPD142 DIFFERENT MODE ************************/
/*
Main mode
*/
#define IDLE_USR			0
#define SAMPLE_USR			3

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

#define MAX_MODE                        4

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
#define OSCILLATOR_TRIM_FILE_PATH	"/efs/FactoryApp/osc_trim"



/**
mode mapping structure used to hold the number mode actually supported by
driver
*/
struct mode_mapping {
	char *name;
	int *mode_code;
	unsigned int size;
};
int __arr_mode_IDLE[MAX_IDLE_MODE] = { IDLE_OFF };
int __arr_mode_SAMPLE[MAX_SAMPLE_MODE]	= {	SAMPLE_XY_A,
						SAMPLE_XY_AB,
						SAMPLE_XY_B };

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
	}
};

/**
general data buffer that is required to store the recieved buffer
*/
unsigned short data_buffer[MAX_BUFFER] = { 0 };

struct wrk_q_timestamp {
	struct timeval interrupt_trigger;
};

struct adpd142_stats {
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
structure hold adpd142 chip structure, and parameter used
to control the adpd142 software part.
*/
struct adpd142_data {
	struct i2c_client *client;
	struct mutex mutex;/*for chip structure*/
	struct mutex storelock;
	struct device *dev;
	struct input_dev *ptr_sample_inputdev;
	struct adpd_platform_data *ptr_config;

	struct work_struct work;
	struct workqueue_struct *ptr_adpd142_wq_st;
	int irq;
	int hrm_int;
	int led_en;
	const char *led_3p3;
	const char *vdd_1p8;
	struct regulator *leda;
	unsigned short *ptr_data_buffer;

	struct adpd142_stats stats;

	unsigned short intr_mask_val;
	unsigned short intr_status_val;
	unsigned short fifo_size;

	/*sysfs register read write */
	unsigned short sysfs_I2C_regaddr;
	unsigned short sysfs_I2C_regval;
	unsigned short sysfslastreadval;

	atomic_t sample_enabled;
	atomic_t adpd_mode;

	int (*read)(struct adpd142_data *, u8 reg_addr, int len, u16 *buf);
	int (*write)(struct adpd142_data *, u8 reg_addr, int len, u16 data);
	u8 eol_test_is_enable;
	u8 eol_test_status;
	char *eol_test_result;
	char *eol_lib_ver;
	char *elf_lib_ver;

	int osc_trim_32K_value;
	int osc_trim_32M_value;
	int osc_trim_addr26_value;
	int osc_trim_addr28_value;
	int osc_trim_addr29_value;
	int dual_hrm;
	int threshold;
	u8 osc_trim_open_enable;
	u8 skip_i2c_msleep;
};

/**
structure hold adpd142 configuration data to be written during probe.
*/
static struct adpd_platform_data adpd142_config_data = {
	.config_size = 53,
	.config_data = {
		0x00100001,
		0x000100ff,
		0x00020004,
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
		0x005b1831,
		0x005d1b31,
		0x005e2808,
		0x004e0040,
	},
};

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
@param pst_adpd the ADPD142 data structure
@param reg is the address from which the value need to be read out
@return u16 value present in the register.
*/
static inline u16
reg_read(struct adpd142_data *pst_adpd, u16 reg)
{
	u16 value;
	if (!pst_adpd->read(pst_adpd, reg, 1, &value))
		return value;
	return 0xFFFF;
}

/**
This function is a inline function mapped to i2c read functionality
@param pst_adpd the ADPD142 data structure
@param reg is the address from which the value need to be read out
@param len number of data need to be readout
@param buf is pointer store the value present in the register.
@return u16 status of i2c read.
*/
static inline u16
multi_reg_read(struct adpd142_data *pst_adpd, u16 reg, u16 len, u16 *buf)
{
	return pst_adpd->read(pst_adpd, reg, len, buf);
}

/**
This function is a inline function mapped to i2c write functionality
@param pst_adpd the ADPD142 data structure
@param reg is the address to which the data need to be written
@param value is the data need to be write.
@return u16 status of i2c write.
*/
static inline u16
reg_write(struct adpd142_data *pst_adpd, u16 reg, u16 value)
{
	return pst_adpd->write(pst_adpd, reg, 1, value);
}

/**
This function is used for I2C read from the sysfs
file system of the ADPD142 Chip
@param pst_adpd the ADPD142 data structure
@return u16 status of i2c read.
*/
static u16
adpd142_sysfs_I2C_read(struct adpd142_data *pst_adpd)
{
	return reg_read(pst_adpd, pst_adpd->sysfs_I2C_regaddr);
}

/**
This function is used for I2C write from the sysfs
file system of the ADPD142 Chip
@param pst_adpd the ADPD142 data structure
@return u16 the bytes of written data.
*/
static u16
adpd142_sysfs_I2C_write(struct adpd142_data *pst_adpd)
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
				ADPD142_info("out of range\n");
				val = 0;
			}
			if (ret ==  -EINVAL) {
				ADPD142_info("parsing error\n");
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
@param pst_adpd the ADPD142 data structure
@return int status of the configuration file.
*/
static int
adpd142_read_config_file(struct adpd142_data *pst_adpd)
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

	fpt_adpd = filp_open("/data/misc/adpd142_config.dcfg", O_RDONLY, 0666);
	if (IS_ERR(fpt_adpd)) {
		ADPD142_dbg("unable to find de file %ld\n", PTR_ERR(fpt_adpd));
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
					ADPD142_dbg("Error in reading dcfg\n");
					break;
				}
				ADPD142_info("0x%08x\n", ret);
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
@param pst_adpd the ADPD142 data structure
@return unsigned short interrupt status value
*/
static unsigned short
adpd142_rd_intr_fifo(struct adpd142_data *pst_adpd)
{
	unsigned short regval = 0;
	unsigned short fifo_size = 0;
	unsigned short usr_mode = 0;
	regval = reg_read(pst_adpd, ADPD_INT_STATUS_ADDR);
	fifo_size = ((regval & 0xFF00) >> 8);
	pst_adpd->fifo_size = ((fifo_size / 2) + (fifo_size & 1));
	pst_adpd->intr_status_val = (regval & 0xFF);

	ADPD142_info("Intr_status 0x%x, FIFO_SIZE 0x%x\n",
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
			ADPD142_info("Keeping %d samples in FIFO from %d\n",
				     mod, pst_adpd->fifo_size);
			pst_adpd->fifo_size &= ~mask;
			atomic_inc(&pst_adpd->stats.fifo_requires_sync);
		}
	}

	return pst_adpd->intr_status_val;
}

/**
This function is used for clearing the Interrupt as well as FIFO
@param pst_adpd the ADPD142 data structure
@return void
*/
static void
adpd142_clr_intr_fifo(struct adpd142_data *pst_adpd)
{
	/*below code is added for verification */
	unsigned short regval;
	regval = reg_read(pst_adpd, ADPD_INT_STATUS_ADDR);
	ADPD142_info("fifo_size: 0x%04x, Status: 0x%x\n",
			  ((regval & 0xFF00) >> 8), (regval & 0xFF));

	reg_write(pst_adpd, ADPD_INT_STATUS_ADDR, 0x80FF);
	regval = reg_read(pst_adpd, ADPD_INT_STATUS_ADDR);
	ADPD142_info("After clear - fifo_size: 0x%04x, Status: 0x%x\n",
			  ((regval & 0xFF00) >> 8), (regval & 0xFF));
}

/**
This function is used for clearing the Interrupt status
@param pst_adpd the ADPD142 data structure
@param mode operating mode of ADPD142
@return void
*/
static void
adpd142_clr_intr_status(struct adpd142_data *pst_adpd, unsigned short mode)
{
	reg_write(pst_adpd, ADPD_INT_STATUS_ADDR,
		  pst_adpd->intr_status_val);
}

/**
This function is used to read out FIFO data from FIFO
@param pst_adpd the ADPD142 data structure
@return unsigned int FIFO size
*/
static unsigned int
adpd142_rd_fifo_data(struct adpd142_data *pst_adpd)
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
			   pst_adpd->fifo_size, pst_adpd->ptr_data_buffer);
/*  reg_write(pst_adpd, ADPD_ACCESS_CTRL_ADDR, 0x0000);*/
	reg_write(pst_adpd, ADPD_TEST_PD_ADDR, DS_FIFO_CLK);

	for (; loop_cnt < pst_adpd->fifo_size; loop_cnt++) {
		ADPD142_info("[0x%04x] 0x%04x\n", loop_cnt,
				  pst_adpd->ptr_data_buffer[loop_cnt]);
	}
	return pst_adpd->fifo_size;
err_rd_fifo_data:
	return 0;
}

/**
This function is a thing need to configure before changing the register
@param pst_adpd the ADPD142 data structure
@param global_mode OFF, IDLE, GO are the three mode
@return void
*/
static void
adpd142_config_prerequisite(struct adpd142_data *pst_adpd, u32 global_mode)
{
	unsigned short regval = 0;
	regval = reg_read(pst_adpd, ADPD_OP_MODE_ADDR);
	regval = SET_GLOBAL_OP_MODE(regval, global_mode);
	ADPD142_info("reg 0x%04x,\tafter set 0x%04x\n",
			  regval, SET_GLOBAL_OP_MODE(regval, global_mode));
	reg_write(pst_adpd, ADPD_OP_MODE_ADDR, regval);
}

/**
This function is used for switching the adpd142 mode
@param pst_adpd the ADPD142 data structure
@param usr_mode user mode
@return void
*/
static void
adpd142_mode_switching(struct adpd142_data *pst_adpd, u32 usr_mode)
{
	unsigned int opmode_val = 0;
	unsigned short mode_val = 0;
	unsigned short intr_mask_val = 0;

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

	adpd142_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_IDLE);

	/*switch to IDLE mode */
	mode_val = DEFAULT_OP_MODE_CFG_VAL(pst_adpd);
	mode_val += SET_MODE_VALUE(IDLE_OFF);
	intr_mask_val = SET_INTR_MASK(IDLE_USR);

	reg_write(pst_adpd, ADPD_OP_MODE_CFG_ADDR, mode_val);
	reg_write(pst_adpd, ADPD_INT_MASK_ADDR, intr_mask_val);

	/*clear FIFO and flush buffer */
	adpd142_clr_intr_fifo(pst_adpd);
	adpd142_rd_intr_fifo(pst_adpd);
	if (pst_adpd->fifo_size != 0) {
		adpd142_clr_intr_status(pst_adpd, IDLE_USR);
		ADPD142_info("Flushing FIFO\n");
		adpd142_rd_fifo_data(pst_adpd);
	}


	adpd142_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_GO);
	msleep(20);
	adpd142_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_IDLE);

	/*Find Interrupt mask value */
	switch (mode) {
	case IDLE_USR:
		ADPD142_info("IDLE MODE\n");
		intr_mask_val = SET_INTR_MASK(IDLE_USR);
		atomic_set(&pst_adpd->sample_enabled, 0);
		break;
	case SAMPLE_USR:
		ADPD142_info("SAMPLE MODE\n");
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
		atomic_set(&pst_adpd->sample_enabled, 1);
		break;
	default:
		/*This case won't occur */
		ADPD142_dbg("invalid mode\n");
		break;
	};

	ADPD142_info("INT_MASK_ADDR[0x%04x] = 0x%04x\n", ADPD_INT_MASK_ADDR,
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

	ADPD142_info("OP_MODE_CFG[0x%04x] = 0x%04x\n",
		ADPD_OP_MODE_CFG_ADDR, mode_val);

	atomic_set(&pst_adpd->adpd_mode, usr_mode);

	reg_write(pst_adpd, ADPD_INT_MASK_ADDR, pst_adpd->intr_mask_val);
	reg_write(pst_adpd, ADPD_OP_MODE_CFG_ADDR, mode_val);

	mutex_unlock(&pst_adpd->mutex);

	enable_irq(pst_adpd->irq);

	if (mode != IDLE_USR)
		adpd142_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_GO);
	else
		adpd142_config_prerequisite(pst_adpd, GLOBAL_OP_MODE_OFF);
}

/**
This function is used for sending Sample event depend upon
the Sample mode
@param pst_adpd the ADPD142 data structure
@return void
*/
static void
adpd142_sample_event(struct adpd142_data *pst_adpd)
{
	unsigned short usr_mode = 0;
	unsigned short sub_mode = 0;
	unsigned short cnt = 0;

	ADPD142_info("%s\n", __func__);

	usr_mode = atomic_read(&pst_adpd->adpd_mode);
	sub_mode = GET_USR_SUB_MODE(usr_mode);

	switch (sub_mode) {
	case S_SAMP_XY_A:
		if (pst_adpd->fifo_size < 4 || (pst_adpd->fifo_size & 0x3)) {
			pr_err("Unexpected FIFO_SIZE=%d\n",
				pst_adpd->fifo_size);
			break;
		}

		for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 4) {
			input_event(pst_adpd->ptr_sample_inputdev, EV_REL,
				REL_Z, sub_mode + 1);

			input_event(pst_adpd->ptr_sample_inputdev, EV_MSC,
				MSC_RAW, pst_adpd->ptr_data_buffer[cnt] + 1);
			input_event(pst_adpd->ptr_sample_inputdev, EV_MSC,
				MSC_RAW, pst_adpd->ptr_data_buffer[cnt + 1] + 1);
			input_event(pst_adpd->ptr_sample_inputdev, EV_MSC,
				MSC_RAW, pst_adpd->ptr_data_buffer[cnt + 2] + 1);
			input_event(pst_adpd->ptr_sample_inputdev, EV_MSC,
				MSC_RAW, pst_adpd->ptr_data_buffer[cnt + 3] + 1);

			input_sync(pst_adpd->ptr_sample_inputdev);
		}
		break;

	case S_SAMP_XY_AB:
		if (pst_adpd->fifo_size < 8 || (pst_adpd->fifo_size & 0x7)) {
			pr_err("Unexpected FIFO_SIZE=%d\n",
				pst_adpd->fifo_size);
			break;
		}

		for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 8) {
			unsigned int channel;
			unsigned int sum_slot_a = 0;
			unsigned int sum_slot_b = 0;

			input_event(pst_adpd->ptr_sample_inputdev,
				EV_REL, REL_Z, sub_mode + 1);

			for (channel = 0; channel < 4; ++channel) {
				sum_slot_a +=
				pst_adpd->ptr_data_buffer[cnt+channel];
				input_event(pst_adpd->ptr_sample_inputdev,
				EV_MSC, MSC_RAW, pst_adpd->ptr_data_buffer[cnt+channel] + 1);
			}
			input_event(pst_adpd->ptr_sample_inputdev,
				EV_REL, REL_X, sum_slot_a + 1);

			for (channel = 4; channel < 8; ++channel) {
				sum_slot_b +=
				pst_adpd->ptr_data_buffer[cnt+channel];
				input_event(pst_adpd->ptr_sample_inputdev,
				EV_MSC, MSC_RAW, pst_adpd->ptr_data_buffer[cnt+channel] + 1);
			}
			input_event(pst_adpd->ptr_sample_inputdev,
				EV_REL, REL_Y, sum_slot_b + 1);
			input_sync(pst_adpd->ptr_sample_inputdev);
		}
		break;
	case S_SAMP_XY_B:
		if (pst_adpd->fifo_size < 4 || (pst_adpd->fifo_size & 0x3)) {
			pr_err("Unexpected FIFO_SIZE=%d\n",
				pst_adpd->fifo_size);
			break;
		}

		for (cnt = 0; cnt < pst_adpd->fifo_size; cnt += 4) {
			input_event(pst_adpd->ptr_sample_inputdev, EV_REL,
				REL_Z,
				sub_mode + 1);

			input_event(pst_adpd->ptr_sample_inputdev, EV_MSC,
				MSC_RAW,
				pst_adpd->ptr_data_buffer[cnt] + 1);
			input_event(pst_adpd->ptr_sample_inputdev, EV_MSC,
				MSC_RAW,
				pst_adpd->ptr_data_buffer[cnt + 1] + 1);
			input_event(pst_adpd->ptr_sample_inputdev, EV_MSC,
				MSC_RAW,
				pst_adpd->ptr_data_buffer[cnt + 2] + 1);
			input_event(pst_adpd->ptr_sample_inputdev, EV_MSC,
				MSC_RAW,
				pst_adpd->ptr_data_buffer[cnt + 3] + 1);

			input_sync(pst_adpd->ptr_sample_inputdev);
		}
		break;
	default:
		break;
	};
}

/**
This function is used for handling FIFO when interrupt occured
@param pst_adpd the ADPD142 data structure
@return void
*/
static void
adpd142_data_handler(struct adpd142_data *pst_adpd)
{
	unsigned short usr_mode = 0;
	unsigned short mode = 0;
	unsigned short sub_mode = 0;
	mutex_lock(&pst_adpd->mutex);
	usr_mode = atomic_read(&pst_adpd->adpd_mode);
	mode = GET_USR_MODE(usr_mode);
	sub_mode = GET_USR_SUB_MODE(usr_mode);

	ADPD142_info("mode - 0x%x,\t sub_mode - 0x%x\n", mode, sub_mode);
	adpd142_rd_intr_fifo(pst_adpd);

	if (pst_adpd->intr_status_val == 0) {
		mutex_unlock(&pst_adpd->mutex);
		return;
	}

	adpd142_clr_intr_status(pst_adpd, mode);

	switch (mode) {
	case IDLE_USR:
		ADPD142_dbg("IDLE_MODE\n");
		adpd142_rd_fifo_data(pst_adpd);
		break;
	case SAMPLE_USR:
		ADPD142_info("SAMPLE MODE\n");
		adpd142_rd_fifo_data(pst_adpd);
		adpd142_sample_event(pst_adpd);
		break;
	default:
		ADPD142_info("DEFAULT MODE\n");
		adpd142_rd_fifo_data(pst_adpd);
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
adpd142_wq_handler(struct work_struct *ptr_work)
{

	struct adpd142_data *pst_adpd = container_of(ptr_work,
					struct adpd142_data, work);

	struct timeval wkq_start;
	struct timeval wkq_comp;
	int diff_usec = 0;

	do_gettimeofday(&wkq_start);

	diff_usec = timeval_compare(&wkq_start,
			&pst_adpd->stats.stamp.interrupt_trigger);

	if (diff_usec > 1) {
		if (diff_usec >
			atomic_read(
				&pst_adpd->stats.wq_schedule_time_peak_usec))
			atomic_set(
				&pst_adpd->stats.wq_schedule_time_peak_usec,
				diff_usec);
		atomic_set(&pst_adpd->stats.wq_schedule_time_last_usec,
			   diff_usec);
		ewma_add(&pst_adpd->stats.wq_schedule_time_avg_usec, diff_usec);
	}

	adpd142_data_handler(pst_adpd);

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
This function is used for handling Interrupt from ADPD142
@param irq is Interrupt number
@param dev_id is pointer point to ADPD142 data structure
@return irqreturn_t is a Interrupt flag
*/
static irqreturn_t
adpd142_isr_handler(int irq, void *dev_id)
{
	struct adpd142_data *pst_adpd = dev_id;

	atomic_inc(&pst_adpd->stats.interrupts);

	if (!work_pending(&pst_adpd->work)) {
		do_gettimeofday(&pst_adpd->stats.stamp.interrupt_trigger);
		ADPD142_info("%s\n", __func__);
		if (!queue_work(pst_adpd->ptr_adpd142_wq_st, &pst_adpd->work))
			atomic_inc(&pst_adpd->stats.wq_pending);
	} else {
		atomic_inc(&pst_adpd->stats.wq_pending);
		ADPD142_info("work_pending !!\n");
	}
	return IRQ_HANDLED;
}

/**
This function is used for updating the ADPD142 structure after configuration
@param pst_adpd the ADPD142 data structure
@return void
*/
static void
adpd142_update_config(struct adpd142_data *pst_adpd)
{
	return;
}

/**
This function is used for loading the configuration data to ADPD142 chip
0 - From file "/data/misc/adpd142_configuration.dcfg"
1 - From Static defined Array
@param pst_adpd the ADPD142 data structure
@param config configuration command
@return int status
*/
static int
adpd142_configuration(struct adpd142_data *pst_adpd, unsigned char config)
{
	struct adpd_platform_data *ptr_config;
	unsigned short addr;
	unsigned short data;
	unsigned short cnt = 0;
	int ret = 0;
	adpd142_mode_switching(pst_adpd, 0);
	if (config == FRM_FILE) {
		ret = adpd142_read_config_file(pst_adpd);
		/*	 ADPD142_info("ARRAY_SIZE - %d\n", size);*/
	} else {
		ret = FRM_ARR;
	}
	if (ret == 0)
		ptr_config = pst_adpd->ptr_config;
	else
		ptr_config = &adpd142_config_data;

	for (cnt = 0; cnt < ptr_config->config_size; cnt++) {
		addr = (unsigned short) ((0xFFFF0000 &
					  ptr_config->config_data[cnt]) >> 16);
		data = (unsigned short) (0x0000FFFF &
					 ptr_config->config_data[cnt]);

		ADPD142_dbg("addr[0x%04x] = 0x%04x\n", addr, data);
		reg_write(pst_adpd, addr, data);
	}

	adpd142_update_config(pst_adpd);

	return 0;
}

/* read & write efs for hrm sensor */

static int osc_trim_efs_register_open(struct adpd142_data *pst_adpd)
{
	struct file *osc_filp = NULL;
	char buffer[60] = {0, };
	int err = 0;
	int osc_trim_32k, osc_trim_32m, osc_trim_addr26, osc_trim_addr28, osc_trim_addr29;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	osc_filp = filp_open(OSCILLATOR_TRIM_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(osc_filp)) {
		err = PTR_ERR(osc_filp);
		if (err != -ENOENT)
			pr_err("adpd142_%s: Can't open oscillator trim file\n", __func__);
		set_fs(old_fs);
		return err;
	}

	err = osc_filp->f_op->read(osc_filp,
		(char *)buffer,
		60 * sizeof(char), &osc_filp->f_pos);
	if (err != (60 * sizeof(char))) {
		pr_err("adpd142_%s: Can't read the oscillator trim data from file\n", __func__);
		err = -EIO;
	}
	sscanf(buffer, "%d,%d,%d,%d,%d",
		&osc_trim_32k, &osc_trim_32m, &osc_trim_addr26, &osc_trim_addr28, &osc_trim_addr29);
	pr_info("adpd142_%s = (%d,%d,%d,%d,%d)\n",
		__func__, osc_trim_32k, osc_trim_32m, osc_trim_addr26, osc_trim_addr28, osc_trim_addr29);

	pst_adpd->osc_trim_32K_value = osc_trim_32k;
	pst_adpd->osc_trim_32M_value = osc_trim_32m;
	pst_adpd->osc_trim_addr26_value = osc_trim_addr26;
	pst_adpd->osc_trim_addr28_value = osc_trim_addr28;
	pst_adpd->osc_trim_addr29_value = osc_trim_addr29;

	filp_close(osc_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int osc_trim_efs_register_save(struct adpd142_data *pst_adpd)
{
	struct file *osc_filp = NULL;
	mm_segment_t old_fs;
	char buffer[60] = {0, };
	int osc_trim_32k, osc_trim_32m, osc_trim_addr26, osc_trim_addr28, osc_trim_addr29;
	int err = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	osc_filp = filp_open(OSCILLATOR_TRIM_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(osc_filp)) {
		pr_err("adpd142_%s: Can't open oscillator trim file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(osc_filp);
		return err;
	}

	osc_trim_32k = reg_read(pst_adpd, OSC_TRIM_32K_REG);
	osc_trim_32m = reg_read(pst_adpd, OSC_TRIM_32M_REG);
	osc_trim_addr26 = reg_read(pst_adpd, OSC_TRIM_ADDR26_REG);
	osc_trim_addr28 = reg_read(pst_adpd, OSC_TRIM_ADDR28_REG);
	osc_trim_addr29 = reg_read(pst_adpd, OSC_TRIM_ADDR29_REG);

	sprintf(buffer, "%d,%d,%d,%d,%d",
		osc_trim_32k, osc_trim_32m, osc_trim_addr26, osc_trim_addr28, osc_trim_addr29);
	pr_info("adpd142_%s = (%d,%d,%d,%d,%d)\n",
		__func__, osc_trim_32k, osc_trim_32m, osc_trim_addr26, osc_trim_addr28, osc_trim_addr29);

	err = osc_filp->f_op->write(osc_filp,
		(char *)&buffer,
		60 * sizeof(char), &osc_filp->f_pos);
	if (err != (60 * sizeof(char))) {
		pr_err("adpd142_%s: Can't write the oscillator trim data to file\n", __func__);
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
@param pst_adpd the ADPD142 data structure
@return void
*/
static void
adpd_stat_reset(struct adpd142_data *pst_adpd)
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
sample_attr_get_enable(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	int val = atomic_read(&pst_adpd->sample_enabled);
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
sample_attr_set_enable(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
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
			pr_err("adpd142_%s: osc_trim_efs_register_open() empty(%d)\n", __func__, err);
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
		pr_info("adpd142_%s = 32K[%02x], 32M[%02x], addr26[%02x], addr28[%02x], addr29[%02x]\n",
			__func__, osc_trim_32k, osc_trim_32m, osc_trim_addr26, osc_trim_addr28, osc_trim_addr29);
		reg_write(pst_adpd, OSC_TRIM_32K_REG, osc_trim_32k);
		reg_write(pst_adpd, OSC_TRIM_32M_REG, osc_trim_32m);
		pst_adpd->osc_trim_open_enable = 0;
	}

	if (val == 1) {
		pr_info("adpd142_%s_enable.\n", __func__);
		if (pst_adpd->led_en != -1)
			gpio_set_value(pst_adpd->led_en, 1);
		else if (!IS_ERR(pst_adpd->leda))
			rc = regulator_enable(pst_adpd->leda);
			if (rc){
				pr_info(KERN_ERR "%s: adpd enable failed (%d)\n",__func__, rc);
			}
			cmd_parsing("0x31", 1, parse_data);
			atomic_set(&pst_adpd->sample_enabled, 1);
	} else {
		pr_info("adpd142_%s_disable.\n", __func__);
		if (pst_adpd->led_en != -1)
			gpio_set_value(pst_adpd->led_en, 0);
		else if (!IS_ERR(pst_adpd->leda))
			rc = regulator_disable(pst_adpd->leda);
			if (rc){
				pr_info(KERN_ERR "%s: adpd disable failed (%d)\n",__func__, rc);
			}
			cmd_parsing("0x0", 1, parse_data);
			atomic_set(&pst_adpd->sample_enabled, 0);
	}
	mode = GET_USR_MODE(parse_data[0]);

	if (GET_USR_MODE(parse_data[0]) < MAX_MODE) {
		if ((GET_USR_SUB_MODE(parse_data[0])) <
			__mode_recv_frm_usr[mode].size) {
			adpd142_mode_switching(pst_adpd, parse_data[0]);
		} else {
			ADPD142_dbg("Sub mode Out of bound\n");
			adpd142_mode_switching(pst_adpd, 0);
		}
	} else {
		ADPD142_dbg("Mode out of bound\n");
		adpd142_mode_switching(pst_adpd, 0);
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
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	int val = atomic_read(&pst_adpd->adpd_mode);
	return sprintf(buf, "%d\n", val);
}

/**
This function is used for switching ADPD142 mode
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
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[2];
	unsigned short mode = 0;

	memset(parse_data, 0, sizeof(parse_data));
	cmd_parsing(buf, 1, parse_data);

	ADPD142_info("Mode requested 0x%02x\n", parse_data[0]);
	pr_info("adpd142_%s = data0[%02x], data1[%02x]\n", __func__, parse_data[0], parse_data[1]);
	mode = GET_USR_MODE(parse_data[0]);
	if (GET_USR_MODE(parse_data[0]) < MAX_MODE) {
		if ((GET_USR_SUB_MODE(parse_data[0])) <
			__mode_recv_frm_usr[mode].size) {
			adpd142_mode_switching(pst_adpd, parse_data[0]);
		} else {
			ADPD142_dbg("Sub mode Out of bound\n");
			adpd142_mode_switching(pst_adpd, 0);
		}
	} else {
		ADPD142_dbg("Mode out of bound\n");
			adpd142_mode_switching(pst_adpd, 0);
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
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	ADPD142_dbg("Regval: 0x%4x\n", pst_adpd->sysfslastreadval);
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
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short addr, cnt;
	unsigned short parse_data[4];
	unsigned short ret;

	memset(parse_data, 0, sizeof(unsigned short) * 4);
	cmd_parsing(buf, 2, parse_data);
	addr = parse_data[0];
	cnt = parse_data[1];

	mutex_lock(&pst_adpd->mutex);

	pst_adpd->sysfs_I2C_regaddr = addr;

	ret = adpd142_sysfs_I2C_read(pst_adpd);
	if (ret != 0xFFFF) {
		ADPD142_dbg("RegRead_Store: addr = 0x%04X,value = 0x%04X\n",
				 addr, ret);
		pst_adpd->sysfslastreadval = ret;
	} else {
		ADPD142_dbg("%s Error\n", __func__);
		pst_adpd->sysfslastreadval = (unsigned short) -1;
	}

	mutex_unlock(&pst_adpd->mutex);
	return size;
}

static int adpd_power_enable(struct adpd142_data *data, int en)
{
	int rc=0;
	struct regulator *regulator_led_3p3;
	struct regulator *regulator_vdd_1p8;

	regulator_vdd_1p8 = regulator_get(NULL, data->vdd_1p8);
	if (IS_ERR(regulator_vdd_1p8) || regulator_vdd_1p8 == NULL) {
		pr_err("%s - vdd_1p8 regulator_get fail\n", __func__);
		return -ENODEV;
	}

	regulator_led_3p3 = regulator_get(NULL, data->led_3p3);
	if (IS_ERR(regulator_led_3p3) || regulator_led_3p3 == NULL) {
		pr_err("%s - led_3p3 regulator_get fail\n", __func__);
		regulator_put(regulator_vdd_1p8);
		return -ENODEV;
	}

	pr_info("%s - onoff = %d\n", __func__, en);

	if (en) {
		rc = regulator_set_voltage(regulator_vdd_1p8, 1800000, 1800000);
		if (rc) {
			pr_err("%s - set vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
		rc = regulator_enable(regulator_vdd_1p8);
		if (rc) {
			pr_err("%s - enable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
		rc = regulator_set_voltage(regulator_led_3p3, 3300000, 3300000);
		if (rc) {
			pr_err("%s - set led_3p3 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
		rc = regulator_enable(regulator_led_3p3);
		if (rc) {
			pr_err("enable led_3p3 failed, rc=%d\n", rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_vdd_1p8);
		if (rc) {
			pr_err("%s - disable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
		rc = regulator_disable(regulator_led_3p3);
		if (rc) {
			pr_err("%s - disable led_3p3 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	}

	done:
	 regulator_put(regulator_vdd_1p8);
	 regulator_put(regulator_led_3p3);

	msleep(10);

	return rc;
}

static int adpd_parse_dt(struct adpd142_data *data, struct device *dev)
{
	struct device_node *this_node= dev->of_node;
	enum of_gpio_flags flags;
	int rc;

	if (this_node == NULL)
		return -ENODEV;

	data->hrm_int = of_get_named_gpio_flags(this_node,"adpd142,irq_gpio", 0, &flags);
	pr_err("%s : get hrm_int(data->hrm_int)(%d) \n", __func__, data->hrm_int);
	if (data->hrm_int < 0) {
		pr_err("%s : get hrm_int(%d) error\n", __func__, data->hrm_int);
		return -ENODEV;
	}

	rc = gpio_request(data->hrm_int, "hrm_int");
	if (rc) {
		pr_err("%s - failed to request hrm_int\n", __func__);
		goto err_int_gpio_req;
	}

	rc = gpio_direction_input(data->hrm_int);
	if (rc) {
		pr_err("%s - failed to set hrm_int as input\n", __func__);
		goto err_int_gpio_direction_input;
	}

	if (of_property_read_string(this_node, "adpd142,vdd_1p8",
		&data->vdd_1p8) < 0)
		pr_err("%s - get vdd_1p8 error\n", __func__);

	if (of_property_read_string(this_node, "adpd142,led_3p3",
		&data->led_3p3) < 0)
		pr_err("%s - get led_3p3 error\n", __func__);

	if (of_property_read_u32(this_node, "adpd142,dual-hrm",
		&data->dual_hrm))
		data->dual_hrm = 0;

	return 0;
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
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short cnt;
	unsigned short parse_data[4];
	unsigned short ret;

	memset(parse_data, 0, sizeof(unsigned short) * 4);
	cmd_parsing(buf, 3, parse_data);
	if (parse_data[1] != 1) {
		ADPD142_dbg("few many argument!!\n");
		goto err_reg_write_argument;
	}

	pst_adpd->sysfs_I2C_regaddr = parse_data[0];
	cnt = parse_data[1];
	pst_adpd->sysfs_I2C_regval = parse_data[2];
	mutex_lock(&pst_adpd->mutex);
	ret = adpd142_sysfs_I2C_write(pst_adpd);
	if (ret == pst_adpd->sysfs_I2C_regval) {
		ADPD142_dbg("Reg[0x%04x] = 0x%04x\n",
			pst_adpd->sysfs_I2C_regaddr,
			pst_adpd->sysfs_I2C_regval);
	} else {
		ADPD142_dbg("Reg write error!!\n");
	}

	adpd142_update_config(pst_adpd);

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
0 - write the configuration data present in file to ADPD142
1 - write the configuration data present in Array to ADPD142
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
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[1];
	memset(parse_data, 0, sizeof(unsigned short) * 1);
	cmd_parsing(buf, 1, parse_data);

	if (parse_data[0] == FRM_ARR)
		adpd142_configuration(pst_adpd, FRM_ARR);
	else if (parse_data[0] == FRM_FILE)
		adpd142_configuration(pst_adpd, FRM_FILE);
	else
		ADPD142_dbg("set 1 to config\n");
	return size;
}


/**
This function is used for getting the status of adpd142 and driver
@param dev linux device structure
@param attr pointer point linux device_attribute structure
@param buf pointer point the buffer
@return ssize_t size of data presnt in the buffer
*/
static ssize_t
attr_stat_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
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
		ADPD142 driver version      : %s\n\
		ADPD142 Release date        : %s\n",

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
		ADPD142_VERSION,
		ADPD142_RELEASE_DATE);

}

#define ADPD142_STAT_RESET	1

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
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	unsigned short parse_data[1];

	memset(parse_data, 0, sizeof(unsigned short) * 1);
	cmd_parsing(buf, 1, parse_data);

	if (parse_data[0] == ADPD142_STAT_RESET) {
		ADPD142_dbg("Resetting statistics\n");
		adpd_stat_reset(pst_adpd);
	}

	return size;
}

/**
array of sample attributes
*/
static struct device_attribute sample_attributes[] = {
	__ATTR(enable, 0664, sample_attr_get_enable, sample_attr_set_enable),
};


/**
This function is used for creating sysfs attribute for sample
@param dev linux device structure
@return int status of attribute creation
*/
static int
create_sysfs_interfaces_sample(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(sample_attributes); i++)
		if (device_create_file(dev, sample_attributes + i))
			goto err_sysfs_interface_sample;
	return 0;
err_sysfs_interface_sample:
	for (; i >= 0; i--)
		device_remove_file(dev, sample_attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

/**
This function is used for removing sysfs attribute for sample
@param dev linux device structure
@return void
*/
static void
remove_sysfs_interfaces_sample(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(sample_attributes); i++)
		device_remove_file(dev, sample_attributes + i);
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
@param pst_adpd pointer point ADPD142 data structure
@return s32 status of this function
*/
static s32
adpd142_input_init_sample(struct adpd142_data *pst_adpd)
{
	int err;
	pst_adpd->ptr_sample_inputdev = input_allocate_device();
	if (!pst_adpd->ptr_sample_inputdev) {
		err = -ENOMEM;
		dev_err(&pst_adpd->client->dev, "input dev allocation fail\n");
		goto err_sample_allocate;
	}

	pst_adpd->ptr_sample_inputdev->name = MODULE_NAME_HRM;
	input_set_drvdata(pst_adpd->ptr_sample_inputdev, pst_adpd);

	__set_bit(EV_MSC, pst_adpd->ptr_sample_inputdev->evbit);
	__set_bit(EV_REL, pst_adpd->ptr_sample_inputdev->evbit);
	__set_bit(MSC_RAW, pst_adpd->ptr_sample_inputdev->mscbit);
	__set_bit(REL_X, pst_adpd->ptr_sample_inputdev->relbit);
	__set_bit(REL_Y, pst_adpd->ptr_sample_inputdev->relbit);
	__set_bit(REL_Z, pst_adpd->ptr_sample_inputdev->relbit);

	err = input_register_device(pst_adpd->ptr_sample_inputdev);
	if (err) {
		dev_err(&pst_adpd->client->dev,
			"unable to register input dev %s\n",
			pst_adpd->ptr_sample_inputdev->name);
		goto err_sample_register_failed;
	}

	err = sensors_create_symlink(pst_adpd->ptr_sample_inputdev);
	if (err < 0) {
		pr_err("[HRM] %s: create_symlink error\n", __func__);
		goto err_sample_register_failed;
	}

	return 0;
err_sample_register_failed:
	input_free_device(pst_adpd->ptr_sample_inputdev);
err_sample_allocate:
	return err;
}

/**
This function is used for unregistering input event for Sample
@param pst_adpd pointer point ADPD142 data structure
@return void
*/
static void
adpd142_input_cleanup_sample(struct adpd142_data *pst_adpd)
{
	input_set_drvdata(pst_adpd->ptr_sample_inputdev, NULL);
	input_unregister_device(pst_adpd->ptr_sample_inputdev);
	input_free_device(pst_adpd->ptr_sample_inputdev);
}

/**
This function is used for registering input event for ADPD142
@param pst_adpd pointer point ADPD142 data structure
@return s32 status of the function
*/
static s32
adpd142_input_init(struct adpd142_data *pst_adpd)
{
	return adpd142_input_init_sample(pst_adpd);
}

/**
This function is used for unregistering input event done for ADPD142
@param pst_adpd pointer point ADPD142 data structure
@return void
*/
static void
adpd142_input_cleanup(struct adpd142_data *pst_adpd)
{
	adpd142_input_cleanup_sample(pst_adpd);
}

/**
This function is used for registering sysfs attribute for ADPD142
@param pst_adpd pointer point ADPD142 data structure
@return s32 status of the called function
*/
static s32
adpd142_sysfs_init(struct adpd142_data *pst_adpd)
{
	if (create_sysfs_interfaces(&pst_adpd->client->dev))
		goto err_sysfs_create_gen;

	if (create_sysfs_interfaces_sample(&pst_adpd->ptr_sample_inputdev->dev))
		goto err_sysfs_create_sample;

	return 0;
err_sysfs_create_sample:

	remove_sysfs_interfaces(&pst_adpd->client->dev);
err_sysfs_create_gen:
	return -1;
}

/**
This function is used for unregistering sysfs attribute for ADPD142
@param pst_adpd pointer point ADPD142 data structure
@return void
*/
static void
adpd142_sysfs_cleanup(struct adpd142_data *pst_adpd)
{
	remove_sysfs_interfaces(&pst_adpd->client->dev);
	remove_sysfs_interfaces_sample(&pst_adpd->ptr_sample_inputdev->dev);
}

/**
This function is used for assigning initial assignment value to
ADPD142 data structure
@param pst_adpd pointer point ADPD142 data structure
@return void
*/
static void
adpd142_struct_assign(struct adpd142_data *pst_adpd)
{
	pst_adpd->ptr_data_buffer = data_buffer;
}

/**
This function is used for initializing ADPD142
@param pst_adpd pointer point ADPD142 data structure
@param id pointer point i2c device id
@return s32 status of the called function
*/
static s32
adpd142_initialization(struct adpd142_data *pst_adpd,
			const struct i2c_device_id *id)
{
	int err = 0;

	if (adpd142_input_init(pst_adpd)) {
		err = -1;
		pr_err("%s err line %d\n", __func__, __LINE__);
		goto err_input_init;
	}
	if (adpd142_sysfs_init(pst_adpd)) {
		pr_err("%s err line %d\n", __func__, __LINE__);
		err = -1;
		goto err_sysfs_init;
	}

	adpd142_struct_assign(pst_adpd);

	memset(&pst_adpd->stats, 0, sizeof(pst_adpd->stats));
	adpd_stat_reset(pst_adpd);

	INIT_WORK(&pst_adpd->work, adpd142_wq_handler);
	pst_adpd->ptr_adpd142_wq_st =
		create_workqueue("adpd142_wq");
	if (!pst_adpd->ptr_adpd142_wq_st) {
		err = -ENOMEM;
		pr_err("%s %d\n", __func__, __LINE__);
		goto err_wq_creation_init;
	}

	if (!pst_adpd->client->irq){
		pr_err("%s %d\n", __func__, __LINE__);
		goto err_work_queue_init;
	}
	pst_adpd->hrm_int = pst_adpd->client->irq;
	pst_adpd->irq = pst_adpd->hrm_int;

	irq_set_irq_type(pst_adpd->irq, IRQ_TYPE_EDGE_RISING);
	err = request_irq(pst_adpd->irq, adpd142_isr_handler,
			  IRQF_TRIGGER_RISING, dev_name(&pst_adpd->client->dev),
			  pst_adpd);
	if (err) {
		ADPD142_dbg("irq %d busy?\n", pst_adpd->irq);
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
	adpd142_configuration(pst_adpd, 1);
	adpd142_mode_switching(pst_adpd, 0); /* turn off : idle mode, temp*/
	pst_adpd->osc_trim_open_enable = 1;

	return 0;
//err_gpio_direction_input:
	//gpio_free(pst_adpd->hrm_int);
err_work_queue_init:
	destroy_workqueue(pst_adpd->ptr_adpd142_wq_st);
err_wq_creation_init:

err_sysfs_init:
	adpd142_input_cleanup(pst_adpd);
err_input_init:
	return err;
}

/**
This function is used for cleanup ADPD142
@param pst_adpd pointer point ADPD142 data structure
@return void
*/
static void
adpd142_initialization_cleanup(struct adpd142_data *pst_adpd)
{
	adpd142_mode_switching(pst_adpd, 0);

	free_irq(pst_adpd->irq, pst_adpd);

	destroy_workqueue(pst_adpd->ptr_adpd142_wq_st);
	adpd142_sysfs_cleanup(pst_adpd);
	adpd142_input_cleanup(pst_adpd);
	kobject_uevent(&pst_adpd->client->dev.kobj, KOBJ_OFFLINE);
}

#ifdef CONFIG_PM
static s32
adpd142_i2c_suspend(struct device *dev)
{
	return 0;
}

static s32
adpd142_i2c_resume(struct device *dev)
{
	return 0;
}
#else
#define adpd142_i2c_resume  NULL
#define adpd142_i2c_suspend NULL
#endif			  /* CONFIG_PM */


/**
This function is used for i2c read communication between ADPD142 and AP
@param pst_adpd pointer point ADPD142 data structure
@param reg_addr address need to be fetch
@param len number byte to be read
@param buf pointer point the read out data.
@return s32 status of the called function
*/
static int
adpd142_i2c_read(struct adpd142_data *pst_adpd, u8 reg_addr, int len,
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
			if (!(pst_adpd->dual_hrm && pst_adpd->skip_i2c_msleep))
				msleep_interruptible(I2C_RETRY_DELAY);
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
This function is used for i2c write communication between ADPD142 and AP
@param pst_adpd pointer point ADPD142 data structure
@param reg_addr address need to be fetch
@param len number byte to be read
@param data value to be written on the register.
@return s32 status of the called function
*/
static int
adpd142_i2c_write(struct adpd142_data *pst_adpd, u8 reg_addr, int len,
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
			if (!(pst_adpd->dual_hrm && pst_adpd->skip_i2c_msleep))
				msleep_interruptible(I2C_RETRY_DELAY);
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
static ssize_t adpd142_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s = %s \n", __func__, CHIP_NAME);

	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t adpd142_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s = %s \n", __func__, VENDOR);

	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t eol_test_result_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t eol_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* eol_test start */
		pst_adpd->eol_test_is_enable = 1;
	else if (sysfs_streq(buf, "0")) /* eol_test stop */
		pst_adpd->eol_test_is_enable = 0;
	else {
		pr_debug("adpd142_%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pst_adpd->eol_test_status = 0;
	pr_info("adpd142_%s =  (%c), value(%u) \n", __func__, *buf, pst_adpd->eol_test_is_enable);

	return size;
}

static ssize_t eol_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", pst_adpd->eol_test_is_enable);
}

static ssize_t eol_test_result_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
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
		pr_err("adpd142_%s - couldn't allocate memory\n", __func__);
		mutex_unlock(&pst_adpd->storelock);
		return -ENOMEM;
	}
	strncpy(pst_adpd->eol_test_result, buf, buf_len);

	mutex_unlock(&pst_adpd->storelock);
	
	pr_info("adpd142_%s - result = %s, buf_len(%u)\n", __func__, pst_adpd->eol_test_result, buf_len);
	pst_adpd->eol_test_status = 1;

	err = osc_trim_efs_register_save(pst_adpd);
	if (err < 0) {
		pr_err("adpd142_%s: osc_trim_efs_register_save() empty(%d)\n", __func__, err);
	}

	return size;
}

static ssize_t eol_test_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->eol_test_result == NULL) {
		pr_info("adpd142_%s - data->eol_test_result is NULL\n", __func__);
		pst_adpd->eol_test_status = 0;
		return snprintf(buf, PAGE_SIZE, "%s\n", "NO_EOL_TEST");
	}
	pr_info("adpd142_%s - result = %s\n", __func__, pst_adpd->eol_test_result);
	pst_adpd->eol_test_status = 0;
	return snprintf(buf, PAGE_SIZE, "%s\n", pst_adpd->eol_test_result);
}

static ssize_t eol_test_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", pst_adpd->eol_test_status);
}

static ssize_t adpd142_eol_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);

	unsigned int buf_len;
	buf_len = strlen(buf) + 1;
	if (buf_len > MAX_LIB_VER)
		buf_len = MAX_LIB_VER;

	mutex_lock(&pst_adpd->storelock);

	if (pst_adpd->eol_lib_ver != NULL)
		kfree(pst_adpd->eol_lib_ver);

	pst_adpd->eol_lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (pst_adpd->eol_lib_ver == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		mutex_unlock(&pst_adpd->storelock);
		return -ENOMEM;
	}
	strncpy(pst_adpd->eol_lib_ver, buf, buf_len);

	mutex_unlock(&pst_adpd->storelock);
	pr_info("%s - eol_lib_ver = %s\n", __func__, pst_adpd->eol_lib_ver);
	return size;
}

static ssize_t adpd142_eol_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->eol_lib_ver == NULL) {
		pr_info("%s - data->eol_lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s - eol_lib_ver = %s\n", __func__, pst_adpd->eol_lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", pst_adpd->eol_lib_ver);
}

static ssize_t adpd142_elf_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);

	unsigned int buf_len;
	buf_len = strlen(buf) + 1;
	if (buf_len > MAX_LIB_VER)
		buf_len = MAX_LIB_VER;

	mutex_lock(&pst_adpd->storelock);

	if (pst_adpd->elf_lib_ver != NULL)
		kfree(pst_adpd->elf_lib_ver);

	pst_adpd->elf_lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (pst_adpd->elf_lib_ver == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		mutex_unlock(&pst_adpd->storelock);
		return -ENOMEM;
	}
	strncpy(pst_adpd->elf_lib_ver, buf, buf_len);

	mutex_unlock(&pst_adpd->storelock);

	return size;
}

static ssize_t adpd142_elf_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->elf_lib_ver == NULL) {
		pr_info("%s - data->elf_lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s - elf_lib_ver = %s\n", __func__, pst_adpd->elf_lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", pst_adpd->elf_lib_ver);
}

static ssize_t adpd142_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);
	int err = 0;

	err = kstrtoint(buf, 10, &pst_adpd->threshold);
	if (err < 0) {
		pr_err("%s - kstrtoint failed.(%d)\n", __func__, err);
		return err;
	}

	pr_info("%s - threshold = %d\n", __func__, pst_adpd->threshold);
	return size;
}

static ssize_t adpd142_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adpd142_data *pst_adpd = dev_get_drvdata(dev);

	if (pst_adpd->threshold) {
		pr_info("%s - threshold = %d\n", __func__, pst_adpd->threshold);
		return snprintf(buf, PAGE_SIZE, "%d\n", pst_adpd->threshold);
	} else {
		pr_info("%s - threshold = 0\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
}

static DEVICE_ATTR(name, S_IRUGO, adpd142_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, adpd142_vendor_show, NULL);
static DEVICE_ATTR(eol_test, S_IRUGO | S_IWUSR | S_IWGRP,
	eol_test_show, eol_test_store);
static DEVICE_ATTR(eol_test_result, S_IRUGO | S_IWUSR | S_IWGRP,
	eol_test_result_show, eol_test_result_store);
static DEVICE_ATTR(eol_test_status, S_IRUGO, eol_test_status_show, NULL);
static DEVICE_ATTR(eol_lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	adpd142_eol_lib_ver_show, adpd142_eol_lib_ver_store);
static DEVICE_ATTR(elf_lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	adpd142_elf_lib_ver_show, adpd142_elf_lib_ver_store);
static DEVICE_ATTR(adpd_mode, S_IRUGO | S_IWUSR | S_IWGRP,
	attr_get_mode, attr_set_mode);
static DEVICE_ATTR(adpd_reg_read, S_IRUGO | S_IWUSR | S_IWGRP,
	attr_reg_read_get, attr_reg_read_set);
static DEVICE_ATTR(adpd_reg_write, S_IWUSR | S_IWGRP,
	NULL, attr_reg_write_set);
static DEVICE_ATTR(adpd_configuration, S_IRUGO | S_IWUSR | S_IWGRP,
	attr_config_get, attr_config_set);
static DEVICE_ATTR(adpd_stat, S_IRUGO | S_IWUSR | S_IWGRP,
	attr_stat_get, attr_stat_set);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
	adpd142_threshold_show, adpd142_threshold_store);

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
	&dev_attr_threshold,
	NULL,
};

/**
This function is used for ADPD142 probe function
@param client pointer point to the linux i2c client structure
@param id pointer point to the linux i2c device id
@return s32 status of the probe function
*/
static s32
adpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct adpd142_data *pst_adpd = NULL;
	struct adpd_platform_data *pdata = NULL;
	int err = 0;
	unsigned short u16_regval = 0;
	pr_err("%s, is called Success\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		pr_err("[SENSOR] %s - exit_check_functionality.\n", __func__);
		goto exit_check_functionality_failed;
	}

	pst_adpd = kzalloc(sizeof(struct adpd142_data), GFP_KERNEL);
	if (pst_adpd == NULL) {
		err = -ENOMEM;
		pr_err("[SENSOR] %s - exit_mem_allocate.\n", __func__);
		goto exit_mem_allocate_failed;
	}

	if(client->dev.of_node) {
		pdata = devm_kzalloc (&client->dev ,
			sizeof(struct adpd_platform_data), GFP_KERNEL);
		if(!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
			if(pst_adpd)
				kfree(pst_adpd);
		return -ENOMEM;
		}
	} else
		pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		if(pst_adpd)
			kfree(pst_adpd);
		return err;
	}

	err = adpd_parse_dt(pst_adpd, &client->dev);
	if (err) {
		pr_err("%s, parse dt fail\n", __func__);
		goto adpd_parse_dt_err;
	}

	err = adpd_power_enable(pst_adpd, 1);
	if (err < 0) {
		pr_err("%s adpd142 regulator on fail err = %d\n",
			__func__, err);
		goto err_regulator_enable;
	}

	mutex_init(&pst_adpd->mutex);
	mutex_init(&pst_adpd->storelock);

	pst_adpd->skip_i2c_msleep = 1;
	pst_adpd->client = client;
	pst_adpd->ptr_config = pdata;
	pst_adpd->read = adpd142_i2c_read;
	pst_adpd->write = adpd142_i2c_write;
	pst_adpd->threshold = 0;
	/*Need to allocate and assign data then use the below function */
	i2c_set_clientdata(client, (struct adpd142_data *)pst_adpd);
	/*chip ID verification */
	u16_regval = reg_read(pst_adpd, ADPD_CHIPID_ADDR);

	switch (u16_regval) {
	case ADPD_CHIPID(0):
	case ADPD_CHIPID(1):
	case ADPD_CHIPID(2):
		err = 0;
		ADPD142_dbg("chipID value = 0x%x\n", u16_regval);
	break;
	default:
		err = 1;
	break;
	};
	if (err) {
		ADPD142_dbg("chipID value = 0x%x\n", u16_regval);
		pr_err("[SENSOR] %s - exit_chipid_verification.\n", __func__);
		goto exit_chipid_verification;
	}
	ADPD142_info("chipID value = 0x%x\n", u16_regval);

	//pst_adpd->dev = &client->dev;
	pr_info("%s - start \n", __func__);

	if (adpd142_initialization(pst_adpd, id)){
		pr_err("[SENSOR] %s - exit_adpd142_init_exit.\n", __func__);
		goto exit_adpd142_init;
	}

	/* set sysfs for hrm sensor */
	err = sensors_register(pst_adpd->dev, pst_adpd, hrm_sensor_attrs,
		"hrm_sensor");
	if (err) {
		pr_err("[SENSOR] %s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	pst_adpd->skip_i2c_msleep = 0;
	pr_info("%s success\n", __func__);

	goto done;
hrm_sensor_register_failed:
exit_adpd142_init:
exit_chipid_verification:
	mutex_destroy(&pst_adpd->mutex);
	adpd_power_enable(pst_adpd, 0);
err_regulator_enable:
adpd_parse_dt_err:
	kfree(pst_adpd);
exit_mem_allocate_failed:
exit_check_functionality_failed:
	dev_err(&client->dev, "%s: Driver Init failed\n", ADPD_DEV_NAME);
	pr_err("%s failed\n", __func__);

done:
	return err;
}

/**
This function is used for ADPD142 remove function
@param client pointer point to the linux i2c client structure
@return s32 status of the remove function
*/
static s32
adpd_i2c_remove(struct i2c_client *client)
{
	struct adpd142_data *pst_adpd = i2c_get_clientdata(client);
	ADPD142_dbg("%s\n", __func__);
	adpd142_initialization_cleanup(pst_adpd);
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
	.resume = adpd142_i2c_resume,
	.suspend = adpd142_i2c_suspend,
};
#endif

/**
  This table tell which framework it supported
  @brief the name has to get matched to the board configuration file setup
 */
/*static struct i2c_device_id adpd_id[] = { {ADPD_DEV_NAME, 0}, {} };

MODULE_DEVICE_TABLE(i2c, adpd_id);*/

static struct of_device_id adpd_match_table[] = {
	{ .compatible = "adpd142",},
	{},
};

MODULE_DEVICE_TABLE(i2c, adpd_match_table);

static const struct i2c_device_id adpd142_device_id[] = {
	{ "adpd142", 0 },
	{ }
};

/**
  i2c operation structure
*/
struct i2c_driver adpd142_i2c_driver = {
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
	.id_table = adpd142_device_id,
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
		ADPD142_info("list of slave addr = 0x%x\n",
				  *(scan_device + count));
	}
	*(scan_device + count) = I2C_CLIENT_END;

	count = 0;

	do {
		i2c_adapter = i2c_get_adapter(count);
		if (i2c_adapter != NULL) {
			memset(&info, 0, sizeof(struct i2c_board_info));
			strlcpy(info.type, slave_name /*"adpd142" */ ,
				I2C_NAME_SIZE);
			/*need to check i2c_new_device instead of
			i2c_new_probed_device*/
			i2c_client =
				i2c_new_probed_device(i2c_adapter, &info,
						  (const unsigned short *)
						  scan_device, NULL);
			if (i2c_client != NULL) {
				ADPD142_dbg("I2C busnum - %d\n", count);
				ADPD142_dbg(
				"device is attached to the bus i2c-%d\n",
					 count);
			} else {

			}
			i2c_put_adapter(i2c_adapter);
		} else {
			ADPD142_info("Not valid adapter\n");
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
@return inti status of the adpd142_multisensor_init
*/
static int __init
adpd142_multisensor_init(void)
{
#ifdef ADPD_AUTO_PROBE
	ADPD142_dbg("%s\n", __func__);
	unsigned short addr[] = { ADPD142_SLAVE_ADDR };
	if (!i2c_check_dev_attach(ADPD_DEV_NAME, addr, 1)) {
		return i2c_add_driver(&adpd142_i2c_driver);
	} else {
		pr_err("i2c bus connect error\n");
		return -1;
	}
#else
	ADPD142_dbg("%s\n", __func__);
	return i2c_add_driver(&adpd142_i2c_driver);
#endif
}

/**
This function is get called when the module is removed
@return void
*/
static void __exit
adpd142_multisensor_exit(void)
{
	ADPD142_dbg("%s\n", __func__);
	i2c_del_driver(&adpd142_i2c_driver);
	if (i2c_client)
		i2c_unregister_device(i2c_client);
}

module_init(adpd142_multisensor_init);
module_exit(adpd142_multisensor_exit);
MODULE_DESCRIPTION();
MODULE_LICENSE("GPL");
MODULE_AUTHOR("");
