#ifndef _MAX86900_H_
#define _MAX86900_H_

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>

#include <linux/pinctrl/consumer.h>
#include "../../pinctrl/core.h"

#define MAX86902_DEBUG

#define MAX86900_SLAVE_ADDR			0x51
#define MAX86900A_SLAVE_ADDR		0x57

#define MAX86902_SLAVE_ADDR			0x57

//MAX86900 Registers
#define MAX86900_INTERRUPT_STATUS	0x00
#define MAX86900_INTERRUPT_ENABLE	0x01

#define MAX86900_FIFO_WRITE_POINTER	0x02
#define MAX86900_OVF_COUNTER		0x03
#define MAX86900_FIFO_READ_POINTER	0x04
#define MAX86900_FIFO_DATA			0x05
#define MAX86900_MODE_CONFIGURATION	0x06
#define MAX86900_SPO2_CONFIGURATION	0x07
#define MAX86900_UV_CONFIGURATION	0x08
#define MAX86900_LED_CONFIGURATION	0x09
#define MAX86900_UV_DATA			0x0F
#define MAX86900_TEMP_INTEGER		0x16
#define MAX86900_TEMP_FRACTION		0x17

#define MAX86900_WHOAMI_REG			0xFE

#define MAX86900_FIFO_SIZE				16

//MAX86902 Registers

#define MAX_UV_DATA			2

#define MODE_UV				1
#define MODE_HR				2
#define MODE_SPO2			3
#define MODE_FLEX			7
#define MODE_GEST			8

#define IR_LED_CH			1
#define RED_LED_CH			2
#define NA_LED_CH			3
#define VIOLET_LED_CH		4

#define NUM_BYTES_PER_SAMPLE			3
#define MAX_LED_NUM						4

#define MAX86902_INTERRUPT_STATUS			0x00
#define PWR_RDY_MASK					0x01
#define UV_INST_OVF_MASK				0x02
#define UV_ACCUM_OVF_MASK				0x04
#define UV_RDY_MASK						0x08
#define UV_RDY_OFFSET					0x03
#define PROX_INT_MASK					0x10
#define ALC_OVF_MASK					0x20
#define PPG_RDY_MASK					0x40
#define A_FULL_MASK						0x80

#define MAX86902_INTERRUPT_STATUS_2			0x01
#define THERM_RDY						0x01
#define DIE_TEMP_RDY					0x02

#define MAX86902_INTERRUPT_ENABLE			0x02
#define MAX86902_INTERRUPT_ENABLE_2			0x03

#define MAX86902_FIFO_WRITE_POINTER			0x04
#define MAX86902_OVF_COUNTER				0x05
#define MAX86902_FIFO_READ_POINTER			0x06
#define MAX86902_FIFO_DATA					0x07

#define MAX86902_FIFO_CONFIG				0x08
#define MAX86902_FIFO_A_FULL_MASK		0x0F
#define MAX86902_FIFO_A_FULL_OFFSET		0x00
#define MAX86902_FIFO_ROLLS_ON_MASK		0x10
#define MAX86902_FIFO_ROLLS_ON_OFFSET	0x04
#define MAX86902_SMP_AVE_MASK			0xE0
#define MAX86902_SMP_AVE_OFFSET			0x05


#define MAX86902_MODE_CONFIGURATION			0x09
#define MAX86902_MODE_MASK				0x07
#define MAX86902_MODE_OFFSET			0x00
#define MAX86902_GESTURE_EN_MASK		0x08
#define MAX86902_GESTURE_EN_OFFSET		0x03
#define MAX86902_RESET_MASK				0x40
#define MAX86902_RESET_OFFSET			0x06
#define MAX86902_SHDN_MASK				0x80
#define MAX86902_SHDN_OFFSET			0x07

#define MAX86902_SPO2_CONFIGURATION			0x0A
#define MAX86902_LED_PW_MASK			0x03
#define MAX86902_LED_PW_OFFSET			0x00
#define MAX86902_SPO2_SR_MASK			0x1C
#define MAX86902_SPO2_SR_OFFSET			0x02
#define MAX86902_SPO2_ADC_RGE_MASK		0x60
#define MAX86902_SPO2_ADC_RGE_OFFSET	0x05
#define MAX86902_SPO2_EN_DACX_MASK		0x80
#define MAX86902_SPO2_EN_DACX_OFFSET	0x07

#define MAX86902_UV_CONFIGURATION			0x0B
#define MAX86902_UV_ADC_RGE_MASK		0x03
#define MAX86902_UV_ADC_RGE_OFFSET		0x00
#define MAX86902_UV_SR_MASK				0x04
#define MAX86902_UV_SR_OFFSET			0x02
#define MAX86902_UV_TC_ON_MASK			0x08
#define MAX86902_UV_TC_ON_OFFSET		0x03
#define MAX86902_UV_ACC_CLR_MASK		0x80
#define MAX86902_UV_ACC_CLR_OFFSET		0x07

#define MAX86902_LED1_PA					0x0C
#define MAX86902_LED2_PA					0x0D
#define MAX86902_LED3_PA					0x0E
#define MAX86902_LED4_PA					0x0F
#define MAX86902_PILOT_PA					0x10

/*THIS IS A BS ENTRY. KEPT HERE TO KEEP LEGACY CODE COMPUILING*/
#define MAX86902_LED_CONFIGURATION			0x10

#define MAX86902_LED_FLEX_CONTROL_1			0x11
#define MAX86902_S1_MASK				0x07
#define MAX86902_S1_OFFSET				0x00
#define MAX86902_S2_MASK				0x70
#define MAX86902_S2_OFFSET				0x04

#define MAX86902_LED_FLEX_CONTROL_2			0x12
#define MAX86902_S3_MASK				0x07
#define MAX86902_S3_OFFSET				0x00
#define MAX86902_S4_MASK				0x70
#define MAX86902_S4_OFFSET				0x04

#define MAX86902_UV_HI_THRESH				0x13
#define MAX86902_UV_LOW_THRESH			0x14
#define MAX86902_UV_ACC_THRESH_HI		0x15
#define MAX86902_UV_ACC_THRESH_MID		0x16
#define MAX86902_UV_ACC_THRESH_LOW		0x17

#define MAX86902_UV_DATA_HI					0x18
#define MAX86902_UV_DATA_LOW				0x19
#define MAX86902_UV_ACC_DATA_HI				0x1A
#define MAX86902_UV_ACC_DATA_MID			0x1B
#define MAX86902_UV_ACC_DATA_LOW			0x1C

#define MAX86902_UV_COUNTER_HI				0x1D
#define MAX86902_UV_COUNTER_LOW				0x1E

#define MAX86902_TEMP_INTEGER				0x1F
#define MAX86902_TEMP_FRACTION				0x20

#define MAX86902_TEMP_CONFIG				0x21
#define MAX86902_TEMP_EN_MASK			0x01
#define MAX86902_TEMP_EN_OFFSET			0x00

#define MAX86902_WHOAMI_REG_REV		0xFE
#define MAX86902_WHOAMI_REG_PART	0xFF

#define MAX86902_FIFO_SIZE				64

//Self Test
#define MAX86900_TEST_MODE					0xFF
#define MAX86900_TEST_GTST					0x80
#define MAX86900_TEST_ENABLE_IDAC			0x81
#define MAX86900_TEST_ENABLE_PLETH			0x82
#define MAX86900_TEST_ALC					0x8F
#define MAX86900_TEST_ROUTE_MODULATOR		0x97
#define MAX86900_TEST_LOOK_MODE_RED			0x98
#define MAX86900_TEST_LOOK_MODE_IR			0x99
#define MAX86900_TEST_IDAC_GAIN				0x9C

typedef enum _PART_TYPE
{
	PART_TYPE_MAX86900 = 0,
	PART_TYPE_MAX86900A,
	PART_TYPE_MAX86900B,
	PART_TYPE_MAX86900C,
	PART_TYPE_MAX86906,
	PART_TYPE_MAX86902A = 10,
	PART_TYPE_MAX86902B,
	PART_TYPE_MAX86907,
	PART_TYPE_MAX86907A,
} PART_TYPE;

typedef enum _AWB_CONFIG
{
	AWB_CONFIG0 = 0,
	AWB_CONFIG1,
	AWB_CONFIG2,
} AWB_CONFIG;

struct max86900_platform_data
{
	int (*init)(void);
	int (*deinit)(void);
};

struct max86900_device_data
{
	struct i2c_client *client;          // represents the slave device
	struct device *dev;
	struct input_dev *hrm_input_dev;
	struct input_dev *hrmled_input_dev;
	struct input_dev *uv_input_dev;
	struct mutex i2clock;
	struct mutex activelock;
	struct mutex flickerdatalock;
	struct mutex storelock;
	struct delayed_work uv_sr_work_queue;
	struct delayed_work reenable_work_queue;
	struct pinctrl *p;
	struct pinctrl_state *pins_sleep;
	struct pinctrl_state *pins_idle;
	struct miscdevice miscdev;
	const char *led_3p3;
	const char *vdd_1p8;
	bool *bio_status;
	atomic_t hrm_is_enable;
	atomic_t uv_is_enable;
	atomic_t enhanced_uv_mode;
	atomic_t is_suspend;
	atomic_t hrm_need_reenable;
	atomic_t isEnable_led;
	atomic_t alc_is_enable;
	u8 led_current;
	u8 led_current1;
	u8 led_current2;
	u8 led_current3;
	u8 led_current4;
	u16 uv_sr_interval;
	u8 hr_range;
	u8 hr_range2;
	u8 look_mode_ir;
	u8 look_mode_red;
	u8 eol_test_is_enable;
	u8 uv_eol_test_is_enable;
	u8 part_type;
	u8 default_current;
	u8 default_current1;
	u8 default_current2;
	u8 default_current3;
	u8 default_current4;
	u8 test_current_ir;
	u8 test_current_red;
	u8 test_current_led1;
	u8 test_current_led2;
	u8 test_current_led3;
	u8 test_current_led4;
	u8 eol_test_status;
	u8 regulator_is_enable;
	u8 skip_i2c_msleep;
	u16 led;
	u16 sample_cnt;
	u16 uv_sample_cnt;
	u16 awb_sample_cnt;
	int dual_hrm;
	int hrm_int;
	int hrm_en;
	int irq;
	int hrm_temp;
	char *eol_test_result;
	char *lib_ver;
	char *uv_lib_ver;
	int ir_sum;
	int r_sum;
	int led_sum[4];
	int spo2_mode;
	u8 flex_mode;
	int num_samples;
	int sum_gesture_data;
	int hrm_threshold;
	u8 reenable_set;
	u8 reenable_cnt;
	u8 irq_state;
	int *flicker_data;
	int flicker_data_cnt;
	int previous_awb_data;
	int thres1;
	int thres2;
	int thres3;
	int thres4;
	u8 awb_flicker_status;
};

extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device *dev, void * drvdata,
	struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);

extern unsigned int lpcharge;
#endif

