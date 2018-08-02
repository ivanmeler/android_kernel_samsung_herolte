#ifndef __M1120_H__
#define __M1120_H__

#include <linux/ioctl.h>
#include <linux/mutex.h>

/* ********************************************************* */
/* feature of ic revision */
/* ********************************************************* */
#define M1120_REV_0_2						(0x02)
#define M1120_REV_1_0						(0x10)
#define M1120_REV							M1120_REV_1_0
#define M1120_DRIVER_VERSION				"Ver1.04-140226"
/* ********************************************************* */


/* ********************************************************* */
/* property of driver */
/* ********************************************************* */
#define M1120_DRIVER_NAME					"m1120"
#define M1120_IRQ_NAME						"m1120-irq"
#define M1120_PATH							"/dev/m1120"
#define M1120_SLAVE_ADDR					(0x18)
/* ********************************************************* */


/* ********************************************************* */
/* register map */
/* ********************************************************* */
#define M1120_REG_PERSINT					(0x00)
#define M1120_VAL_PERSINT_COUNT(n)			(n<<4)					
#define M1120_VAL_PERSINT_INTCLR			(0x01)

#define M1120_REG_INTSRS					(0x01)
#define M1120_VAL_INTSRS_INT_ON				(0x80)
#define M1120_DETECTION_MODE_INTERRUPT		M1120_VAL_INTSRS_INT_ON
#define M1120_VAL_INTSRS_INT_OFF			(0x00)
#define M1120_DETECTION_MODE_POLLING		M1120_VAL_INTSRS_INT_OFF
#define M1120_VAL_INTSRS_INTTYPE_BESIDE		(0x00)
#define M1120_VAL_INTSRS_INTTYPE_WITHIN		(0x40)
#define M1120_VAL_INTSRS_SRS_10BIT_0_068mT	(0x00)
#define M1120_VAL_INTSRS_SRS_10BIT_0_034mT	(0x01)
#define M1120_VAL_INTSRS_SRS_10BIT_0_017mT	(0x02)
#define M1120_VAL_INTSRS_SRS_10BIT_0_009mT	(0x03)
#define M1120_VAL_INTSRS_SRS_10BIT_0_004mT	(0x04)
#define M1120_VAL_INTSRS_SRS_8BIT_0_272mT	(0x00)
#define M1120_VAL_INTSRS_SRS_8BIT_0_136mT	(0x01)
#define M1120_VAL_INTSRS_SRS_8BIT_0_068mT	(0x02)
#define M1120_VAL_INTSRS_SRS_8BIT_0_036mT	(0x03)
#define M1120_VAL_INTSRS_SRS_8BIT_0_016mT	(0x04)

#define M1120_REG_LTHL						(0x02)

#define M1120_REG_LTHH						(0x03)

#define M1120_REG_HTHL						(0x04)

#define M1120_REG_HTHH						(0x05)

#define M1120_REG_I2CDIS					(0x06)
#define M1120_VAL_I2CDISABLE				(0x37)

#define M1120_REG_SRST						(0x07)
#define M1120_VAL_SRST_RESET				(0x01)

#define M1120_REG_OPF						(0x08)
#define M1120_VAL_OPF_FREQ_20HZ				(0x00)
#define M1120_VAL_OPF_FREQ_10HZ				(0x10)
#define M1120_VAL_OPF_FREQ_6_7HZ			(0x20)
#define M1120_VAL_OPF_FREQ_5HZ				(0x30)
#define M1120_VAL_OPF_FREQ_80HZ				(0x40)
#define M1120_VAL_OPF_FREQ_40HZ				(0x50)
#define M1120_VAL_OPF_FREQ_26_7HZ			(0x60)
#define M1120_VAL_OPF_EFRD_ON				(0x08)
#define M1120_VAL_OPF_BIT_8					(0x02)
#define M1120_VAL_OPF_BIT_10				(0x00)
#define M1120_VAL_OPF_HSSON_ON				(0x01)

#define M1120_REG_DID						(0x09)
#define M1120_VAL_DID						(0x9C)

#define M1120_REG_INFO						(0x0A)

#define M1120_REG_ASA						(0x0B)

#define M1120_REG_ST1						(0x10)
#define M1120_VAL_ST1_DRDY					(0x01)

#define M1120_REG_HSL						(0x11)

#define M1120_REG_HSH						(0x12)

/* ********************************************************* */
/* ioctl command */
/* ********************************************************* */
#define M1120_IOCTL_BASE					(0x80)
#define M1120_IOCTL_SET_ENABLE				_IOW(M1120_IOCTL_BASE, 0x00, int)
#define M1120_IOCTL_GET_ENABLE				_IOR(M1120_IOCTL_BASE, 0x01, int)
#define M1120_IOCTL_SET_DELAY				_IOW(M1120_IOCTL_BASE, 0x02, int)
#define M1120_IOCTL_GET_DELAY				_IOR(M1120_IOCTL_BASE, 0x03, int)
#define M1120_IOCTL_SET_CALIBRATION			_IOW(M1120_IOCTL_BASE, 0x04, int*)
#define M1120_IOCTL_GET_CALIBRATED_DATA		_IOR(M1120_IOCTL_BASE, 0x05, int*)
#define M1120_IOCTL_SET_INTERRUPT			_IOW(M1120_IOCTL_BASE, 0x06, unsigned int)
#define M1120_IOCTL_GET_INTERRUPT			_IOR(M1120_IOCTL_BASE, 0x07, unsigned int*)
#define M1120_IOCTL_SET_THRESHOLD_HIGH		_IOW(M1120_IOCTL_BASE, 0x08, unsigned int)
#define M1120_IOCTL_GET_THRESHOLD_HIGH		_IOR(M1120_IOCTL_BASE, 0x09, unsigned int*)
#define M1120_IOCTL_SET_THRESHOLD_LOW		_IOW(M1120_IOCTL_BASE, 0x0A, unsigned int)
#define M1120_IOCTL_GET_THRESHOLD_LOW		_IOR(M1120_IOCTL_BASE, 0x0B, unsigned int*)
#define M1120_IOCTL_SET_REG					_IOW(M1120_IOCTL_BASE, 0x0C, int)
#define M1120_IOCTL_GET_REG					_IOR(M1120_IOCTL_BASE, 0x0D, int)
/* ********************************************************* */


/* ********************************************************* */
/* event property */
/* ********************************************************* */
#define DEFAULT_EVENT_TYPE					EV_ABS
#define DEFAULT_EVENT_CODE					ABS_X
#define DEFAULT_EVENT_DATA_CAPABILITY_MIN	(-32768)
#define DEFAULT_EVENT_DATA_CAPABILITY_MAX	(32767)
/* ********************************************************* */
/* delay property */
/* ********************************************************* */
#define M1120_DELAY_MAX						(1000)	// ms
#define M1120_DELAY_MIN						(500)	// ms
#define M1120_DELAY_FOR_READY				(10)	// ms
/* ********************************************************* */


/* ********************************************************* */
/* data type for driver */
/* ********************************************************* */

enum {
	OPERATION_MODE_POWERDOWN,
	OPERATION_MODE_MEASUREMENT,
	OPERATION_MODE_FUSEROMACCESS
};

#define M1120_REG_NUM						(15)
typedef union {
	struct {
		unsigned char persint;
		unsigned char intsrs;
		unsigned char lthl;
		unsigned char lthh;
		unsigned char hthl;
		unsigned char hthh;
		unsigned char i2cdis;
		unsigned char srst;
		unsigned char opf;
		unsigned char did;
		unsigned char info;
		unsigned char asa;
		unsigned char st1;
		unsigned char hsl;
		unsigned char hsh;
	} map;
	unsigned char array[M1120_REG_NUM];
} m1120_reg_t;

typedef struct {
	struct mutex	enable;
	struct mutex	data;
} m1120_mutex_t;

typedef struct {
	atomic_t enable;
	atomic_t delay;
	atomic_t debug;
} m1120_atomic_t;

typedef struct {
	int					power_vi2c;
	int					power_vdd;
	int					interrupt_gpio;
	int					interrupt_irq;
} m1120_platform_data_t;

typedef struct {
	struct i2c_client	*client;
	struct input_dev	*input_fold;
	struct input_dev	*input_back;
	m1120_mutex_t		mtx;
	m1120_atomic_t		atm;
	m1120_reg_t			reg;
	bool				irq_enabled;
	int					calibrated_data;
	int					last_data;
	short				thrhigh_on;
	short				thrlow_on;
	short				thrhigh_off;
	short				thrlow_off;
	bool				irq_first;

	struct delayed_work	work;

	int					power_vi2c;
	int					power_vdd;
	int					igpio;
	int					irq;

	u8 event_val_fold;
	u8 event_val_back;
} m1120_data_t;

extern struct device *sec_key;
/* ********************************************************* */

#endif // __M1120_H__

