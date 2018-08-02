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

#define MAX86900_DEBUG

#define MAX86900_SLAVE_ADDR			0x51
#define MAX86900A_SLAVE_ADDR		0x57

//MAX86900 Registers
#define MAX86900_INTERRUPT_STATUS	0x00
#define MAX86900_INTERRUPT_ENABLE	0x01

#define MAX86900_FIFO_WRITE_POINTER	0x02
#define MAX86900_OVF_COUNTER		0x03
#define MAX86900_FIFO_READ_POINTER	0x04
#define MAX86900_FIFO_DATA			0x05
#define MAX86900_MODE_CONFIGURATION	0x06
#define MAX86900_SPO2_CONFIGURATION	0x07
#define MAX86900_LED_CONFIGURATION	0x09
#define MAX86900_TEMP_INTEGER		0x16
#define MAX86900_TEMP_FRACTION		0x17


//Self Test
#define MAX86900_TEST_MODE		0xFF
#define MAX86900_TEST_GTST		0x80
#define MAX86900_TEST_ENABLE_IDAC	0x81
#define MAX86900_TEST_ENABLE_PLETH	0x82
#define MAX86900_TEST_ALC			0x8F
#define MAX86900_TEST_ROUTE_MODULATOR	0x97
#define MAX86900_TEST_LOOK_MODE_RED		0x98
#define MAX86900_TEST_LOOK_MODE_IR		0x99
#define MAX86900_TEST_IDAC_GAIN		0x9C


#define MAX86900_FIFO_SIZE		16

typedef enum _PART_TYPE
{
	PART_TYPE_MAX86900 = 0,
	PART_TYPE_MAX86900A,
	PART_TYPE_MAX86900B,
	PART_TYPE_MAX86900C,
} PART_TYPE;

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
	struct mutex i2clock;
	struct mutex activelock;
	struct mutex storelock;
	struct regulator *vdd_1p8;
	struct regulator *vdd_3p3;
	const char *vldo39;
	const char *vldo40;
	bool *bio_status;
	atomic_t is_enable;
	atomic_t is_suspend;
	u8 led_current;
	u8 hr_range;
	u8 hr_range2;
	u8 look_mode_ir;
	u8 look_mode_red;
	u8 eol_test_is_enable;
	u8 part_type;
	u8 default_current;
	u8 test_current_ir;
	u8 test_current_red;
	u8 eol_test_status;
	u16 led;
	u16 sample_cnt;
	int hrm_int;
	int irq;
	int hrm_temp;
	char *eol_test_result;
	char *lib_ver;
	int ir_sum;
	int r_sum;
};

extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device *dev, void * drvdata,
	struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);
#endif

