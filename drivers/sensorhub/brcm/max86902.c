/*
 * Copyright (c) 2014 JY Kim, jy.kim@maximintegrated.com
 * Copyright (c) 2013 Maxim Integrated Products, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "max86902.h"
#ifdef CONFIG_SENSORS_MAX_NOTCHFILTER
#include "max_notchfilter/downsample.h"
#endif
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#define MAX86900_I2C_RETRY_DELAY	10
#define MAX86900_I2C_MAX_RETRIES	5
#define MAX86900_COUNT_MAX		65532
#define MAX86902_COUNT_MAX		65532

#define MAX86900C_WHOAMI		0x11
#define MAX86900A_REV_ID		0x00
#define MAX86900B_REV_ID		0x04
#define MAX86900C_REV_ID		0x05

#define MAX86902_PART_ID1			0xFF
#define MAX86902_PART_ID2			0x15
#define MAX86902_REV_ID1			0xFE
#define MAX86902_REV_ID2			0x03
#define MAX86906_OTP_ID				0x15
#define MAX86907_OTP_ID				0x01
#define MAX86907A_OTP_ID			0x02

#define MAX86900_DEFAULT_CURRENT	0x55
#define MAX86900A_DEFAULT_CURRENT	0xFF
#define MAX86900C_DEFAULT_CURRENT	0x0F
#define MAX86906_DEFAULT_CURRENT	0x0F

#define MAX86902_DEFAULT_CURRENT1	0x00 //RED
#define MAX86902_DEFAULT_CURRENT2	0xFF //IR
#define MAX86902_DEFAULT_CURRENT3	0x60 //NONE
#define MAX86902_DEFAULT_CURRENT4	0x60 //Violet

#define MAX86902_DEFAULT_UV_SR_INTERVAL	100 /*ms*/
#define MAX86902_RE_ENABLE		200/*ms*/

#define MODULE_NAME_HRM			"hrm_sensor"
#define MODULE_NAME_HRM_LED		"hrmled_sensor"
#define MODULE_NAME_UV			"uv_sensor"
#define CHIP_NAME				"MAX86902"
#define MAX86900_CHIP_NAME		"MAX86900"
#define MAX86902_CHIP_NAME		"MAX86902"
#define MAX86907_CHIP_NAME		"MAX86907"
#define MAX86907A_CHIP_NAME		"MAX86907A"
#define VENDOR					"MAXIM"
#define HRM_LDO_ON	1
#define HRM_LDO_OFF	0
#define MAX_EOL_RESULT 132
#define MAX_LIB_VER 20

#define MAX86900_SAMPLE_RATE	4
#ifdef CONFIG_SENSORS_MAX_NOTCHFILTER
#define MAX86902_SAMPLE_RATE	4
#else
#define MAX86902_SAMPLE_RATE	1
#endif

#define MAX86902_SPO2_ADC_RGE	2

#define MAX86902_ENHANCED_UV_GESTURE_MODE	1
#define MAX86902_ENHANCED_UV_HR_MODE		2
#define MAX86902_ENHANCED_UV_MODE			3
#define MAX86902_ENHANCED_UV_EOL_VB_MODE	4
#define MAX86902_ENHANCED_UV_EOL_SUM_MODE	5
#define MAX86902_ENHANCED_UV_EOL_HR_MODE	6
#define MAX86902_ENHANCED_UV_NONE_MODE		7

#define MAX86902_REENABLE_OFF	0
#define MAX86902_REENABLE_HRM	1
#define MAX86902_REENABLE_UV	2
#define MAX86902_REENABLE_FAIL	3
#define MAX86902_REENABLE_MAX_CNT	10

#define MAX86902_IRQ_ENABLE	1
#define MAX86902_IRQ_DISABLE	0

#define AWB_INTERVAL		12
#define CONFIG_SKIP_CNT		8
#define FLICKER_DATA_CNT	200
#define MAX86900_IOCTL_MAGIC		0xFD
#define MAX86900_IOCTL_READ_FLICKER	_IOR(MAX86900_IOCTL_MAGIC, 0x01, int *)
#define AWB_CONFIG_TH1		2000
#define AWB_CONFIG_TH2		240000
#define AWB_CONFIG_TH3		20000
#define AWB_CONFIG_TH4		90

static int dbg_enable;
#define max86902_uv_log(format, arg...)    {if (dbg_enable)\
				pr_info("max86902 uv : "format, ##arg);\
				}

/* I2C function */
static int max86900_write_reg(struct max86900_device_data *device,
	u8 reg_addr, u8 data)
{
	int err;
	int tries = 0;
	u8 buffer[2] = { reg_addr, data };
	struct i2c_msg msgs[] = {
		{
			.addr = device->client->addr,
			.flags = device->client->flags & I2C_M_TEN,
			.len = 2,
			.buf = buffer,
		},
	};

	do {
		mutex_lock(&device->i2clock);
		err = i2c_transfer(device->client->adapter, msgs, 1);
		mutex_unlock(&device->i2clock);
		if (err != 1)
			if (!(device->dual_hrm && device->skip_i2c_msleep))
				msleep_interruptible(MAX86900_I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < MAX86900_I2C_MAX_RETRIES));

	if (err != 1) {
		pr_err("%s -write transfer error\n", __func__);
		err = -EIO;
		return err;
	}

	return 0;
}

static int max86900_read_reg(struct max86900_device_data *data,
	u8 *buffer, int length)
{
	int err = -1;
	int tries = 0; /* # of attempts to read the device */

	struct i2c_msg msgs[] = {
		{
			.addr = data->client->addr,
			.flags = data->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buffer,
		},
		{
			.addr = data->client->addr,
			.flags = (data->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = length,
			.buf = buffer,
		},
	};

	do {
		mutex_lock(&data->i2clock);
		err = i2c_transfer(data->client->adapter, msgs, 2);
		mutex_unlock(&data->i2clock);
		if (err != 2)
			if (!(data->dual_hrm && data->skip_i2c_msleep))
				msleep_interruptible(MAX86900_I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < MAX86900_I2C_MAX_RETRIES));

	if (err != 2) {
		pr_err("%s -read transfer error\n", __func__);
		err = -EIO;
	} else
		err = 0;

	return err;
}

/* Device Control */
static int max86900_regulator_onoff(struct max86900_device_data *data, int onoff)
{
	int rc = 0;
	struct regulator *regulator_led_3p3;
	struct regulator *regulator_vdd_1p8;

	if (data->regulator_is_enable == onoff) {
		pr_err("%s - duplicate regulator : %d\n", __func__, onoff);
		return 0;
	}

	regulator_vdd_1p8 = regulator_get(NULL, data->vdd_1p8);
	if (IS_ERR(regulator_vdd_1p8) || regulator_vdd_1p8 == NULL) {
		pr_err("%s - vdd_1p8 regulator_get fail\n", __func__);
		return -ENODEV;
	}

	regulator_led_3p3 = regulator_get(NULL, data->led_3p3);
	if (IS_ERR(regulator_led_3p3) || regulator_led_3p3 == NULL) {
		pr_err("%s - vdd_3p3 regulator_get fail\n", __func__);
		regulator_put(regulator_vdd_1p8);
		return -ENODEV;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);

	if (onoff == HRM_LDO_ON) {
		rc = regulator_set_voltage(regulator_vdd_1p8, 1800000, 1800000);
		if (rc < 0) {
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
		if (rc < 0) {
			pr_err("%s - set led_3p3 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
		rc = regulator_enable(regulator_led_3p3);
		if (rc) {
			pr_err("%s - enable led_3p3 failed, rc=%d\n",
				__func__, rc);
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

	data->regulator_is_enable = (u8)onoff;

	done:
	regulator_put(regulator_led_3p3);
	regulator_put(regulator_vdd_1p8);

	return rc;
}

static int max86900_init_device(struct max86900_device_data *data)
{
	int err;
	u8 recvData;

	mutex_lock(&data->activelock);
	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	/* Interrupt Clear */
	recvData = MAX86900_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x80);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_UV_CONFIGURATION, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_UV_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	mutex_unlock(&data->activelock);
	pr_info("%s done, part_type = %u\n", __func__, data->part_type);

	return 0;
}


static int max86902_init_device(struct max86900_device_data *data)
{
	int err = 0;
	u8 recvData;

	mutex_lock(&data->activelock);
	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	/* Interrupt Clear */
	recvData = MAX86902_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	/* Interrupt2 Clear */
	recvData = MAX86902_INTERRUPT_STATUS_2;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	mutex_unlock(&data->activelock);
	pr_info("%s done, part_type = %u\n", __func__, data->part_type);

	return 0;
}

void max86900_pin_control(struct max86900_device_data *data, bool pin_set)
{
	int status = 0;
	data->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(data->pins_idle)) {
			status = pinctrl_select_state(data->p,
				data->pins_idle);
			if (status)
				pr_err("%s: can't set pin default state\n",
					__func__);
			pr_debug("%s idle\n", __func__);
		}
	} else {
		if (!IS_ERR(data->pins_sleep)) {
			status = pinctrl_select_state(data->p,
				data->pins_sleep);
			if (status)
				pr_err("%s: can't set pin sleep state\n",
					__func__);
			pr_debug("%s sleep\n", __func__);
		}
	}
}

static void irq_set_state(struct max86900_device_data *data, int irq_enable)
{
	pr_info("%s - irq_enable : %d, irq_state : %d\n",
		__func__, irq_enable, data->irq_state);

	if (irq_enable) {
		if(!data->irq_state) {
			enable_irq(data->irq);
			data->irq_state++;
		}
	} else {
		if(data->irq_state) {
			disable_irq(data->irq);
			data->irq_state--;
		}
	}
}

static int max86900_hrm_enable(struct max86900_device_data *data)
{
	int err;
	data->led = 0;
	data->sample_cnt = 0;
	data->ir_sum = 0;
	data->r_sum = 0;

	mutex_lock(&data->activelock);

	err = max86900_write_reg(data, MAX86900_INTERRUPT_ENABLE, 0x10);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_INTERRUPT_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

#if MAX86900_SAMPLE_RATE == 1
	err = max86900_write_reg(data, MAX86900_SPO2_CONFIGURATION, 0x47);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
#endif

#if MAX86900_SAMPLE_RATE == 2
	err = max86900_write_reg(data, MAX86900_SPO2_CONFIGURATION, 0x4E);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
#endif

#if MAX86900_SAMPLE_RATE == 4
	err = max86900_write_reg(data, MAX86900_SPO2_CONFIGURATION, 0x51);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
#endif

	err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION,
		data->default_current);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_FIFO_WRITE_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_FIFO_WRITE_POINTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_OVF_COUNTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_OVF_COUNTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_FIFO_READ_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_FIFO_READ_POINTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x0B);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	irq_set_state(data, MAX86902_IRQ_ENABLE);
	mutex_unlock(&data->activelock);

	if (data->reenable_cnt < MAX86902_REENABLE_MAX_CNT) {
		schedule_delayed_work(&data->reenable_work_queue,
			msecs_to_jiffies(MAX86902_RE_ENABLE));
		data->reenable_set = MAX86902_REENABLE_HRM;
	} else {
		pr_err("%s - retry %d times, but fail stil!\n",
			__func__, data->reenable_cnt);
		data->reenable_cnt = 0;
		data->reenable_set = MAX86902_REENABLE_FAIL;
	}

	return 0;
}

static int max86902_hrm_enable(struct max86900_device_data *data)
{
	int err;
	u8 flex_config[2] = {0, };
	data->led = 0;
	data->sample_cnt = 0;
#ifdef CONFIG_SENSORS_MAX_NOTCHFILTER
	downsample_init();
#else
	data->led_sum[0] = 0;
	data->led_sum[1] = 0;
	data->led_sum[2] = 0;
	data->led_sum[3] = 0;
#endif
	data->num_samples = 0;
	data->flex_mode = 0;

	flex_config[0] = (IR_LED_CH << MAX86902_S2_OFFSET) | RED_LED_CH;
	flex_config[1] = 0x00;
	if (flex_config[0] & MAX86902_S1_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 0);
	}
	if (flex_config[0] & MAX86902_S2_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 1);
	}
	if (flex_config[1] & MAX86902_S3_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 2);
	}
	if (flex_config[1] & MAX86902_S4_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 3);
	}

	pr_info("%s - flexmode : 0x%02x, num_samples : %d\n", __func__,
			data->flex_mode, data->num_samples);
	mutex_lock(&data->activelock);

	/*write LED currents ir=1, red=2, violet=4*/
	err = max86900_write_reg(data, MAX86902_LED1_PA,
		data->default_current1);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED1_PA!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	err = max86900_write_reg(data, MAX86902_LED2_PA,
			data->default_current2);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED2_PA!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	atomic_set(&data->alc_is_enable, 1);

	err = max86900_write_reg(data, MAX86902_INTERRUPT_ENABLE, PPG_RDY_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED_FLEX_CONTROL_1,
			flex_config[0]);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED_FLEX_CONTROL_1!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	err = max86900_write_reg(data, MAX86902_LED_FLEX_CONTROL_2,
			flex_config[1]);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED_FLEX_CONTROL_2!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION,
			0x0E | (0x03 << MAX86902_SPO2_ADC_RGE_OFFSET));
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

#if MAX86902_SAMPLE_RATE == 1
	err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
			(0x02 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
#endif

#if MAX86902_SAMPLE_RATE == 2
	err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
			(0x01 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
#endif

#if MAX86902_SAMPLE_RATE == 4
	err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
			(0x00 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
#endif

	err = max86900_write_reg(data, MAX86902_FIFO_WRITE_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_WRITE_POINTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_OVF_COUNTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_OVF_COUNTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_FIFO_READ_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_READ_POINTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	irq_set_state(data, MAX86902_IRQ_ENABLE);
	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, MODE_FLEX);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		irq_set_state(data, MAX86902_IRQ_DISABLE);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	/* Temperature Enable */
	err = max86900_write_reg(data, MAX86902_TEMP_CONFIG, 0x01);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_TEMP_CONFIG!\n",
			__func__);
		irq_set_state(data, MAX86902_IRQ_DISABLE);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	mutex_unlock(&data->activelock);

	if (data->reenable_cnt < MAX86902_REENABLE_MAX_CNT) {
		schedule_delayed_work(&data->reenable_work_queue,
			msecs_to_jiffies(MAX86902_RE_ENABLE));
		data->reenable_set = MAX86902_REENABLE_HRM;
	} else {
		pr_err("%s - retry %d times, but fail stil!\n",
			__func__, data->reenable_cnt);
		data->reenable_cnt = 0;
		data->reenable_set = MAX86902_REENABLE_FAIL;
	}

	return 0;
}

static int max86900_uv_enable(struct max86900_device_data *data)
{
	int err;
	data->led = 0;
	data->sample_cnt = 0;

	mutex_lock(&data->activelock);

	err = max86900_write_reg(data, MAX86900_INTERRUPT_ENABLE, 0x08);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_INTERRUPT_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x09);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	irq_set_state(data, MAX86902_IRQ_ENABLE);
	mutex_unlock(&data->activelock);
	return 0;
}


static int max86902_uv_init_fov_correction(struct max86900_device_data *data)
{
	int err;
	err = max86900_write_reg(data, 0xFF, 0x54);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0xFF, 0x4d);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0x82, 0x04);
	if (err != 0) {
		pr_err("%s - error initializing 0x82!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0x8f, 0x01);
	if (err != 0) {
		pr_err("%s - error initializing 0x8f!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0x8e, 0x40);
	if (err != 0) {
		pr_err("%s - error initializing 0x8e!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0x90, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing 0x90!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0xFF, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST3!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED1_PA, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED1_PA!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED2_PA, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED2_PA!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED3_PA, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED3_PA!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED4_PA, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED4_PA!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED_FLEX_CONTROL_1,
			RED_LED_CH); /* IR/RED SWAPED */
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED_FLEX_CONTROL_1!\n",
			__func__);
		return -EIO;
}

	/* Interrupt 20th(32-20) */
	err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
			(MAX86902_FIFO_ROLLS_ON_MASK | 0x0C));
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
			__func__);
		return -EIO;
	}

	return err;
}

static int max86902_uv_enable_gesture(struct max86900_device_data *data)
{
	int err;
	int retry_cnt = 2;
	u8 recvData;

	data->sample_cnt = 0;
	data->num_samples = 1;
	data->sum_gesture_data = 0;

	do {
		err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x40);
		if (err != 0) {
			pr_err("%s - error sw shutdown device!\n", __func__);
			return -EIO;
		}

		usleep_range(1000, 1100);

		err = max86902_uv_init_fov_correction(data);

		/* 400 hz, PDMUX=1 */
		err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION, 0xEF);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
				__func__);
			return -EIO;
		}

		err = max86900_write_reg(data, MAX86902_INTERRUPT_ENABLE, PPG_RDY_MASK);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
				__func__);
			return -EIO;
		}

		err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION,
			(MODE_GEST | MODE_FLEX));
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
				__func__);
			return -EIO;
		}

		/* Interrupt Clear */
		recvData = MAX86902_INTERRUPT_STATUS;
		if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
			pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData);
			return -EIO;
		}

		/* Interrupt2 Clear */
		recvData = MAX86902_INTERRUPT_STATUS_2;
		if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
			pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData);
			return -EIO;
		}
	} while(retry_cnt > 0 && !gpio_get_value_cansleep(data->hrm_int));

	atomic_set(&data->enhanced_uv_mode, MAX86902_ENHANCED_UV_GESTURE_MODE);

	return err;
}

static int max86902_uv_enable_hr(struct max86900_device_data *data)
{
	int err;
	u8 recvData;

	data->num_samples = 20;

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		return -EIO;
	}

	usleep_range(1000, 1100);
	/* Temperature Enable */
	err = max86900_write_reg(data, MAX86902_TEMP_CONFIG, 0x01);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_TEMP_CONFIG!\n",
			__func__);
		return -EIO;
	}

	err = max86902_uv_init_fov_correction(data);

	/* 400Hz */
	err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION, 0xEF);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_INTERRUPT_ENABLE, A_FULL_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, MODE_FLEX);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	/* Interrupt Clear */
	recvData = MAX86902_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	/* Interrupt2 Clear */
	recvData = MAX86902_INTERRUPT_STATUS_2;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86902_ENHANCED_UV_HR_MODE);

	return err;
}

static int max86902_uv_enable_uv(struct max86900_device_data *data)
{
	int err;
	u8 recvData;

	data->sample_cnt = 0;
	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		return -EIO;
	}

	usleep_range(1000, 1100);
	err = max86900_write_reg(data, MAX86902_INTERRUPT_ENABLE, 0x08);
	if (err != 0) {
		pr_err("%s - error init MAX86902_INTERRUPT_ENABLE!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_UV_CONFIGURATION, 0x05);
	if (err != 0) {
		pr_err("%s - error init MAX86902_UV_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x01);
	if (err != 0) {
		pr_err("%s - error init MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	/* Interrupt Clear */
	recvData = MAX86902_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	/* Interrupt2 Clear */
	recvData = MAX86902_INTERRUPT_STATUS_2;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86902_ENHANCED_UV_MODE);

	return err;
}

static int max86902_uv_enable(struct max86900_device_data *data)
{
	int err;
	err = max86902_uv_enable_gesture(data);

	mutex_lock(&data->activelock);
	irq_set_state(data, MAX86902_IRQ_ENABLE);
	mutex_unlock(&data->activelock);

	if (data->reenable_cnt < MAX86902_REENABLE_MAX_CNT) {
		schedule_delayed_work(&data->reenable_work_queue,
			msecs_to_jiffies(MAX86902_RE_ENABLE));
		data->reenable_set = MAX86902_REENABLE_UV;
	} else {
		pr_err("%s - retry %d times, but fail stil!\n",
			__func__, data->reenable_cnt);
		data->reenable_cnt = 0;
		data->reenable_set = MAX86902_REENABLE_FAIL;
	}

	return err;
}

static int max86902_uv_eol_init_fov_correction(struct max86900_device_data *data)
{
	int err;
	err = max86900_write_reg(data, 0xFF, 0x54);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0xFF, 0x4d);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0x82, 0x04);
	if (err != 0) {
		pr_err("%s - error initializing 0x82!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0x8f, 0x01);
	if (err != 0) {
		pr_err("%s - error initializing 0x8f!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0x8e, 0x40);
	if (err != 0) {
		pr_err("%s - error initializing 0x8e!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0x90, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing 0x90!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0xFF, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST3!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED1_PA, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED1_PA!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED2_PA, data->led_current2);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED2_PA!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED3_PA, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED3_PA!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED4_PA, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED4_PA!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED_FLEX_CONTROL_1,
			RED_LED_CH); /* IR/RED SWAPED */
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED_FLEX_CONTROL_1!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
			(MAX86902_FIFO_ROLLS_ON_MASK | 0x0C)); /* Interrupt 20th(32-20) */
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
			__func__);
		return -EIO;
	}

	return err;
}

static int max86902_uv_eol_enable_vb(struct max86900_device_data *data)
{
	int err;
	u8 recvData;

	data->sample_cnt = 0;
	data->num_samples = 1;

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		return -EIO;
	}

	usleep_range(1000, 1100);

	err = max86902_uv_eol_init_fov_correction(data);

	/* 100hz */
	err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION, 0xE7);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_INTERRUPT_ENABLE, PPG_RDY_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION,
		(MODE_GEST | MODE_FLEX));
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	/* Interrupt Clear */
	recvData = MAX86902_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	/* Interrupt2 Clear */
	recvData = MAX86902_INTERRUPT_STATUS_2;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86902_ENHANCED_UV_EOL_VB_MODE);

	return err;
}

static int max86902_uv_eol_enable_sum(struct max86900_device_data *data)
{
	int err;
	u8 recvData;

	data->sample_cnt = 0;
	data->num_samples = 1;

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		return -EIO;
	}

	usleep_range(1000, 1100);

	err = max86902_uv_eol_init_fov_correction(data);

	/* 400 hz */
	err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION, 0x6F);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_INTERRUPT_ENABLE, PPG_RDY_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION,
		(MODE_GEST | MODE_FLEX));
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	/* Interrupt Clear */
	recvData = MAX86902_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	/* Interrupt2 Clear */
	recvData = MAX86902_INTERRUPT_STATUS_2;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86902_ENHANCED_UV_EOL_SUM_MODE);

	return err;
}

static int max86902_uv_eol_enable_hr(struct max86900_device_data *data)
{
	int err;
	u8 recvData;

	data->num_samples = 20;

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		return -EIO;
	}

	usleep_range(1000, 1100);

	err = max86902_uv_eol_init_fov_correction(data);

	/* 400Hz */
	err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION, 0xEF);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_INTERRUPT_ENABLE, A_FULL_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, MODE_FLEX);
	/* err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x02); */
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	/* Interrupt Clear */
	recvData = MAX86902_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	/* Interrupt2 Clear */
	recvData = MAX86902_INTERRUPT_STATUS_2;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86902_ENHANCED_UV_EOL_HR_MODE);

	return err;
}

static int max86902_uv_eol_test_enable(struct max86900_device_data *data)
{
	int err;
	err = max86902_uv_eol_enable_vb(data);

	return err;
}

static int max86900_disable(struct max86900_device_data *data)
{
	int err;

	mutex_lock(&data->activelock);

	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error init MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		goto i2c_err;
	}
	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x80);
	if (err != 0) {
		pr_err("%s - error init MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		goto i2c_err;
	}

	irq_set_state(data, MAX86902_IRQ_DISABLE);

	if (delayed_work_pending(&data->reenable_work_queue)) {
		cancel_delayed_work(&data->reenable_work_queue);
		data->reenable_set = MAX86902_REENABLE_OFF;
		data->reenable_cnt = 0;
	}

	mutex_unlock(&data->activelock);

	return 0;
i2c_err:
	irq_set_state(data, MAX86902_IRQ_DISABLE);
	mutex_unlock(&data->activelock);
	return -EIO;
}

static int max86902_disable(struct max86900_device_data *data)
{
	int err;

	mutex_lock(&data->activelock);

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error init MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		goto i2c_err;
	}
	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, 0x80);
	if (err != 0) {
		pr_err("%s - error init MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		goto i2c_err;
	}

	irq_set_state(data, MAX86902_IRQ_DISABLE);

	if (delayed_work_pending(&data->reenable_work_queue)) {
		cancel_delayed_work(&data->reenable_work_queue);
		data->reenable_set = MAX86902_REENABLE_OFF;
		data->reenable_cnt = 0;
	}

	mutex_unlock(&data->activelock);

	return 0;

i2c_err:
	irq_set_state(data, MAX86902_IRQ_DISABLE);
	mutex_unlock(&data->activelock);
	return -EIO;
}

static int max86900_read_temperature(struct max86900_device_data *data)
{
	u8 recvData[2] = { 0x00, };
	int err;

	recvData[0] = MAX86900_TEMP_INTEGER;

	err = max86900_read_reg(data, recvData, 2);
	if (err != 0) {
		pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}
	data->hrm_temp = ((char)recvData[0]) * 16 + recvData[1];

	pr_info("%s - %d(%x, %x)\n", __func__,
		data->hrm_temp, recvData[0], recvData[1]);

	return 0;
}

static int max86902_read_temperature(struct max86900_device_data *data)
{
	u8 recvData[2] = { 0x00, };
	int err;

	recvData[0] = MAX86902_TEMP_INTEGER;

	err = max86900_read_reg(data, recvData, 1);
	if (err != 0) {
		pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	recvData[1] = MAX86902_TEMP_FRACTION;
	err = max86900_read_reg(data, &recvData[1], 1);
	if (err != 0) {
		pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	data->hrm_temp = ((char)recvData[0]) * 16 + recvData[1];

	err = max86900_write_reg(data, MAX86902_TEMP_CONFIG, 0x01);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_TEMP_CONFIG!\n",
			__func__);
		return -EIO;
	}

	if (atomic_read(&data->uv_is_enable)){
		max86902_uv_log("%s - %d(%x, %x)\n", __func__,
			data->hrm_temp, recvData[0], recvData[1]);
	} else
		pr_info("%s - %d(%x, %x)\n", __func__,
			data->hrm_temp, recvData[0], recvData[1]);

	return 0;
}

static int max86900_eol_test_control(struct max86900_device_data *data)
{
	int err = 0;
	u8 led_current = 0;

	if (data->sample_cnt < data->hr_range2)	{
		data->hr_range = 1;
	} else if (data->sample_cnt < (data->hr_range2 + 297)) {
		if (data->eol_test_is_enable == 1) {
			/* Fake pulse */
			if (data->sample_cnt % 8 < 4) {
				data->test_current_ir++;
				data->test_current_red++;
			} else {
				data->test_current_ir--;
				data->test_current_red--;
			}

			led_current = (data->test_current_red << 4)
				| data->test_current_ir;
			err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION,
					led_current);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
					__func__);
				return -EIO;
			}
			data->hr_range = 2;
		} else if (data->eol_test_is_enable == 2) {
			data->sample_cnt = data->hr_range2 + 297 - 1;
		}
	} else if (data->sample_cnt == (data->hr_range2 + 297)) {
		/* Measure */
		err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION,
				data->led_current);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
				__func__);
			return -EIO;
		}
		/* 400Hz setting */
		err = max86900_write_reg(data,
				MAX86900_SPO2_CONFIGURATION, 0x51);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
				__func__);
			return -EIO;
		}
	} else if (data->sample_cnt < ((data->hr_range2 + 297) + 400 * 10)) {
		data->hr_range = 3;
	} else if (data->sample_cnt == ((data->hr_range2 + 297) + 400 * 10)) {
		err = max86900_write_reg(data,
				MAX86900_LED_CONFIGURATION, data->default_current);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
				__func__);
			return -EIO;
		}
#if MAX86900_SAMPLE_RATE == 1
		err = max86900_write_reg(data,
				MAX86900_SPO2_CONFIGURATION, 0x47);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
					__func__);
			return -EIO;
		}
#endif

#if MAX86900_SAMPLE_RATE == 2
		err = max86900_write_reg(data,
				MAX86900_SPO2_CONFIGURATION, 0x4E);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
					__func__);
			return -EIO;
		}
#endif

#if MAX86900_SAMPLE_RATE == 4
		err = max86900_write_reg(data,
				MAX86900_SPO2_CONFIGURATION, 0x51);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
					__func__);
			return -EIO;
		}
#endif
	}
	data->sample_cnt++;
	return 0;
}

static int max86902_eol_test_control(struct max86900_device_data *data)
{
	int err = 0;

	if (data->sample_cnt < data->hr_range2)	{
		data->hr_range = 1;
	} else if (data->sample_cnt < (data->hr_range2 + 297)) {
		/* Fake pulse */
		if (data->sample_cnt % 8 < 4) {
			data->test_current_led1 += 0x10;
			data->test_current_led2 += 0x10;
			data->test_current_led3 += 0x10;
			data->test_current_led4 += 0x10;
		} else {
			data->test_current_led1 -= 0x10;
			data->test_current_led2 -= 0x10;
			data->test_current_led3 -= 0x10;
			data->test_current_led4 -= 0x10;
		}

		/*write LED currents ir=2, red=1, violet=4*/
		err = max86900_write_reg(data, MAX86902_LED1_PA,
				data->test_current_led1);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED1_PA!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86902_LED2_PA,
				data->test_current_led2);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED2_PA!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86902_LED4_PA,
				data->test_current_led4);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED4_PA!\n",
				__func__);
			return -EIO;
		}
		data->hr_range = 2;
	} else if (data->sample_cnt == (data->hr_range2 + 297)) {
		/* Measure */
		/*write LED currents ir=1, red=2, violet=4*/
		err = max86900_write_reg(data, MAX86902_LED1_PA,
				data->led_current1);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED1_PA!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86902_LED2_PA,
				data->led_current2);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED2_PA!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86902_LED4_PA,
				data->led_current4);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED4_PA!\n",
				__func__);
			return -EIO;
		}
		/* 400Hz setting */
		err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION,
				0x0E | (MAX86902_SPO2_ADC_RGE << MAX86902_SPO2_ADC_RGE_OFFSET));
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
				(0x00 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
				__func__);
			return -EIO;
		}
	} else if (data->sample_cnt < ((data->hr_range2 + 297) + 400 * 10)) {
		data->hr_range = 3;
	} else if (data->sample_cnt == ((data->hr_range2 + 297) + 400 * 10)) {
		/*write LED currents ir=1, red=2, violet=4*/
		err = max86900_write_reg(data, MAX86902_LED1_PA,
				data->default_current1);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED1_PA!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86902_LED2_PA,
				data->default_current2);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED2_PA!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86902_LED4_PA,
				data->default_current4);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_LED4_PA!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION,
				0x0E | (MAX86902_SPO2_ADC_RGE << MAX86902_SPO2_ADC_RGE_OFFSET));
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
				__func__);
			return -EIO;
		}
#if MAX86902_SAMPLE_RATE == 1
		err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
				(0x02 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
				__func__);
			return -EIO;
		}
#endif

#if MAX86902_SAMPLE_RATE == 2
		err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
				(0x01 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
				__func__);
			return -EIO;
		}
#endif

#if MAX86902_SAMPLE_RATE == 4
		err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
				(0x01 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
		if (err != 0) {
			pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
				__func__);
			return -EIO;
		}
#endif
	}
	data->sample_cnt++;
	return 0;
}

static int max86900_hrm_read_data(struct max86900_device_data *device, u16 *data)
{
	int err;
	u8 recvData[4] = { 0x00, };
	int i;
	int ret = 0;

	if (device->sample_cnt == MAX86900_COUNT_MAX)
		device->sample_cnt = 0;

	recvData[0] = MAX86900_FIFO_DATA;
	if ((err = max86900_read_reg(device, recvData, 4)) != 0) {
		pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	for (i = 0; i < 2; i++)	{
		data[i] = ((((u16)recvData[i*2]) << 8) & 0xff00)
			| (((u16)recvData[i*2+1]) & 0x00ff);
	}

	data[2] = device->led;

	if ((device->sample_cnt % 1000) == 1)
		pr_info("%s - %u, %u, %u, %u\n", __func__,
			data[0], data[1], data[2], data[3]);

	if (device->sample_cnt == 20 && device->led == 0) {
		err = max86900_read_temperature(device);
		if (err < 0) {
			pr_err("%s - max86900_read_temperature err : %d\n",
				__func__, err);
			return -EIO;
		}
	}

	if (device->eol_test_is_enable) {
		err = max86900_eol_test_control(device);
		if (err < 0) {
			pr_err("%s - max86900_eol_test_control err : %d\n",
					__func__, err);
			return -EIO;
		}
	} else {
		device->ir_sum += data[0];
		device->r_sum += data[1];
		if ((device->sample_cnt % MAX86900_SAMPLE_RATE) == MAX86900_SAMPLE_RATE - 1) {
			data[0] = device->ir_sum / MAX86900_SAMPLE_RATE;
			data[1] = device->r_sum / MAX86900_SAMPLE_RATE;
			device->ir_sum = 0;
			device->r_sum = 0;
			ret = 0;
		} else
			ret = 1;

		if (device->sample_cnt++ > 100 && device->led == 0)
			device->led = 1;
	}

	return ret;
}

static int max86902_hrm_read_data(struct max86900_device_data *device, int *data)
{
	int err;
	u8 recvData[MAX_LED_NUM * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int i, j = 0;
	int ret = 0;

	if (device->sample_cnt == MAX86902_COUNT_MAX)
		device->sample_cnt = 0;

	recvData[0] = MAX86902_FIFO_DATA;
	if ((err = max86900_read_reg(device, recvData,
			device->num_samples * NUM_BYTES_PER_SAMPLE)) != 0) {
		pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	for (i = 0; i < MAX_LED_NUM; i++)	{
		if (device->flex_mode | (1 << i)) {
			data[i] =  recvData[j++] << 16 & 0x30000;
			data[i] += recvData[j++] << 8;
			data[i] += recvData[j++] << 0;
		} else
			data[i] = 0;
	}

	data[4] = device->led;

	if ((device->sample_cnt % 1000) == 1)
		pr_info("%s - %u, %u, %u, %u\n", __func__,
			data[0], data[1], data[2], data[3]);

	if (device->sample_cnt == 20 && device->led == 0) {
		err = max86902_read_temperature(device);
		if (err < 0) {
			pr_err("%s - max86900_read_temperature err : %d\n",
				__func__, err);
			return -EIO;
		}
	}

	if (device->eol_test_is_enable) {
		err = max86902_eol_test_control(device);
		if (err < 0) {
			pr_err("%s - max86900_eol_test_control err : %d\n",
					__func__, err);
			return -EIO;
		}
	} else {
		for (i = 0; i < MAX_LED_NUM; i++)
#ifdef CONFIG_SENSORS_MAX_NOTCHFILTER
			data[i] = downsample(data[i] >> 2, i) << 2;
#else
			device->led_sum[i] += data[i];
#endif

		if ((device->sample_cnt % MAX86902_SAMPLE_RATE) == MAX86902_SAMPLE_RATE - 1) {
#ifndef CONFIG_SENSORS_MAX_NOTCHFILTER
			for (i = 0; i < MAX_LED_NUM; i++) {
				data[i] = device->led_sum[i] / MAX86902_SAMPLE_RATE;
				device->led_sum[i] = 0;
			}
#endif
			ret = 0;
		} else
			ret = 1;

		if (device->sample_cnt++ > 100 && device->led == 0)
			device->led = 1;
	}

	return ret;
}

static int max86902_awb_flicker_read_data(struct max86900_device_data *device, int *data)
{
	int err;
	u8 recvData[20 * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int ret = 0;
	int mode_changed = 0;

	recvData[0] = MAX86902_FIFO_DATA;

	if ((err = max86900_read_reg(device, recvData,
			NUM_BYTES_PER_SAMPLE)) != 0) {
		pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	*data = (recvData[0] << 16 & 0x30000)
			+ (recvData[1] << 8)
			+ (recvData[2] << 0);

	if (device->awb_sample_cnt > CONFIG_SKIP_CNT) {
		mutex_lock(&device->flickerdatalock);
		device->flicker_data[device->flicker_data_cnt++] = *data;
		mutex_unlock(&device->flickerdatalock);
	}

	/* Change Configuation */
	if (device->awb_flicker_status == AWB_CONFIG0
			&& device->awb_sample_cnt > CONFIG_SKIP_CNT) {
		if ( *data > device->thres1 
				&& device->previous_awb_data > device->thres1) { /* Change to AWB_CONFIG1 (*/
			err = max86900_write_reg(device, 0xFF, 0x54);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(device, 0xFF, 0x4d);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(device, 0x8f, 0x01); /* PW_EN = 0 */
			if (err != 0) {
				pr_err("%s - error initializing 0x01!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(device, 0xFF, 0x00);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST3!\n",
					__func__);
				return -EIO;
			}
			device->awb_flicker_status = AWB_CONFIG1;
			mode_changed = 1;
		}
	} else if ( device->awb_flicker_status == AWB_CONFIG1 ) {
		if ( *data < device->thres4 
				&& device->previous_awb_data < device->thres4 ) { /* Change to AWB_CONFIG0 */
			err = max86900_write_reg(device, 0xFF, 0x54);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(device, 0xFF, 0x4d);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(device, 0x8f, 0x81); /* PW_EN = 1 */
			if (err != 0) {
				pr_err("%s - error initializing 0x01!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(device, 0xFF, 0x00);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST3!\n",
					__func__);
				return -EIO;
			}
			device->awb_flicker_status = AWB_CONFIG0;
			mode_changed = 1;
		}
		else if ( *data > device->thres2 
					&& device->previous_awb_data > device->thres2 ) { /* Change to AWB_CONFIG2 */
			/* 16384 uA setting */
			err = max86900_write_reg(device,
				MAX86902_SPO2_CONFIGURATION, 0x6F);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
					__func__);
				return -EIO;
			}
			device->awb_flicker_status = AWB_CONFIG2;
			mode_changed = 1;
		}
	} else if ( device->awb_flicker_status == AWB_CONFIG2 ) {
		if ( *data < device->thres3 
				&& device->previous_awb_data < device->thres3 ) { /* Change to AWB_CONFIG1 */
			/* 2048 uA setting */
			err = max86900_write_reg(device,
				MAX86902_SPO2_CONFIGURATION, 0x0F);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
					__func__);
				return -EIO;
			}
			device->awb_flicker_status = AWB_CONFIG1;
			mode_changed = 1;
		}
	}
	device->previous_awb_data = *data;

	if (mode_changed == 1) {
		/* Flush Buffer */
		err = max86900_write_reg(device,
			MAX86902_MODE_CONFIGURATION, 0x02);
		if (err != 0) {
			pr_err("%s - error init MAX86902_MODE_CONFIGURATION!\n",
				__func__);
			return -EIO;
		}
		device->flicker_data_cnt = 0;
		device->awb_sample_cnt = 0;
		pr_info("%s - mode changed to : %d\n",
			__func__, device->awb_flicker_status);
	}

	if ((device->awb_sample_cnt % AWB_INTERVAL == AWB_INTERVAL - 1)
			|| device->flicker_data_cnt == FLICKER_DATA_CNT) {
		if (device->awb_flicker_status < AWB_CONFIG2)
			*data = *data >> 3; /* 2uA setting should devided by 8 */
		ret = 0;
	} else
		ret = 1;

	device->awb_sample_cnt++;

	return ret;
}

static int max86900_uv_read_data(struct max86900_device_data *device, u16 *data)
{
	int err;
	u8 recvData[2] = { 0x00, };
	int ret = 0;

	if (device->sample_cnt == MAX86900_COUNT_MAX)
		device->sample_cnt = 0;

	recvData[0] = MAX86900_UV_DATA;
	if ((err = max86900_read_reg(device, recvData, 2)) != 0) {
		pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	*data = ((((u16)recvData[0]) << 8) & 0x0f00)
		| ((((u16)recvData[1])) & 0x00ff);

	pr_info("%s - %u\n", __func__, *data);

	if (device->sample_cnt == 1 && device->led == 0) {
		err = max86900_read_temperature(device);
		if (err < 0) {
			pr_err("%s - max86900_read_temperature err : %d\n",
				__func__, err);
			return -EIO;
		}
	}

	if (device->sample_cnt++ > 0 && device->led == 0)
		device->led = 1;

	return ret;
}

static int max86902_uv_read_data_gesture(struct max86900_device_data *device, int *data)
{
	int err;
	int ret = MAX86902_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data = 0;

	status = MAX86902_INTERRUPT_STATUS;
	err = max86900_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & PPG_RDY_MASK) {
		recvData[0] = MAX86902_FIFO_DATA;
		if ((err = max86900_read_reg(device, recvData,
				NUM_BYTES_PER_SAMPLE)) != 0) {
			pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		device->sample_cnt++;

		if (device->sample_cnt > 0 && device->sample_cnt <= 4) {
			reg_data =  recvData[0] << 16 & 0x30000;
			reg_data += recvData[1] << 8;
			reg_data += recvData[2] << 0;
			if (((recvData[0] >> 6) == 1) || ((recvData[0] >> 6) == 3)) {
				device->sum_gesture_data += reg_data;
			}
		}

		if (device->sample_cnt >= 4) {
			/* Mode change */
			if (device->uv_eol_test_is_enable)
				err = max86902_uv_eol_enable_vb(device);
			else
				err = max86902_uv_enable_hr(device);

			if (err < 0) {
				pr_err("%s - max86902_uv_enable_hr err : %d\n",
					__func__, err);
				return -EIO;
			}

			*data = device->sum_gesture_data;
			max86902_uv_log("%s - %u\n", __func__, *data);
			ret = MAX86902_ENHANCED_UV_GESTURE_MODE;
		}
	}

	return ret;
}

static int max86902_uv_read_data_hr(struct max86900_device_data *device, int *data)
{
	int err, i;
	int ret = MAX86902_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[20 * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data = 0;
	int sum_data = 0;

    status = MAX86902_INTERRUPT_STATUS;
	err = max86900_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & A_FULL_MASK){
		recvData[0] = MAX86902_FIFO_DATA;
		if ((err = max86900_read_reg(device, recvData,
				device->num_samples * NUM_BYTES_PER_SAMPLE)) != 0) {
			pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		for (i = 16; i < 20; i++) {
			reg_data =  recvData[i*NUM_BYTES_PER_SAMPLE+0] << 16 & 0x30000;
			reg_data += recvData[i*NUM_BYTES_PER_SAMPLE+1] << 8;
			reg_data += recvData[i*NUM_BYTES_PER_SAMPLE+2] << 0;
			sum_data += reg_data;
		}

		/* Read Temperature */
		err = max86902_read_temperature(device);
		if (err < 0) {
			pr_err("%s - max86900_read_temperature err : %d\n",
				__func__, err);
			return -EIO;
		}
		/* Mode change */
		if (device->uv_eol_test_is_enable)
			err = max86902_uv_eol_enable_vb(device);
		else
			err = max86902_uv_enable_uv(device);

		if (err < 0) {
			pr_err("%s - max86902_uv_enable_uv err : %d\n",
				__func__, err);
			return -EIO;
		}

		*data = sum_data;
		max86902_uv_log("%s - %u\n", __func__, *data);
		ret = MAX86902_ENHANCED_UV_HR_MODE;
	}

	return ret;
}

static int max86902_uv_read_data_uv(struct max86900_device_data *device, int *data)
{
	int err;
	u8 recvData[2] = { 0x00, };
	int ret = MAX86902_ENHANCED_UV_NONE_MODE;
	u8 status;

	status = MAX86902_INTERRUPT_STATUS;
	err = max86900_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & UV_RDY_MASK) {
		recvData[0] = MAX86902_UV_DATA_HI;
		if ((err = max86900_read_reg(device, recvData, MAX_UV_DATA)) != 0) {
			pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		*data = ((((int)recvData[0]) << 8) & 0x0f00)
			| ((((int)recvData[1])) & 0x00ff);

		/* Mode change */
		if (device->uv_eol_test_is_enable)
			err = max86902_uv_eol_enable_vb(device);
		else
			err = max86902_uv_enable_gesture(device);

		if (err < 0) {
			pr_err("%s - max86902_uv_enable_gesture err : %d\n",
				__func__, err);
			return -EIO;
		}
		max86902_uv_log("%s - %u\n", __func__, *data);
		ret = MAX86902_ENHANCED_UV_MODE;
	}

	return ret;
}

static int max86902_uv_eol_read_data_vb(struct max86900_device_data *device, int *data)
{
	int err;
	int ret = MAX86902_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[MAX_LED_NUM * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data;

	status = MAX86902_INTERRUPT_STATUS;
	err = max86900_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & PPG_RDY_MASK) {
		recvData[0] = MAX86902_FIFO_DATA;
		if ((err = max86900_read_reg(device, recvData,
				device->num_samples * NUM_BYTES_PER_SAMPLE)) != 0) {
			pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		device->sample_cnt++;

		if (device->sample_cnt > 16 && device->sample_cnt <= 20) {
			/* reg_data =  recvData[0] << 16 & 0x30000; */
			reg_data =  recvData[0] << 16;
			reg_data += recvData[1] << 8;
			reg_data += recvData[2] << 0;
			pr_info("vb:%d,%d\n", (reg_data & 0x3ffff), reg_data >> 22);
			*data = reg_data;
			ret = MAX86902_ENHANCED_UV_EOL_VB_MODE;
		}

		if (device->sample_cnt >= 20) {
			/* Mode change */
			if (device->uv_eol_test_is_enable)
				err = max86902_uv_eol_enable_sum(device);
			else
				err = max86902_uv_enable_gesture(device);

			if (err < 0) {
				pr_err("%s - max86902_uv_eol_enable_sum err : %d\n",
					__func__, err);
				return -EIO;
			}
		}
	}

	return ret;
}

static int max86902_uv_eol_read_data_sum(struct max86900_device_data *device, int *data)
{
	int err;
	int ret = MAX86902_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data = 0;

	status = MAX86902_INTERRUPT_STATUS;
	err = max86900_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & PPG_RDY_MASK) {
		recvData[0] = MAX86902_FIFO_DATA;
		if ((err = max86900_read_reg(device, recvData,
				NUM_BYTES_PER_SAMPLE)) != 0) {
			pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		device->sample_cnt++;

		reg_data =  recvData[0] << 16 & 0x30000;
		reg_data += recvData[1] << 8;
		reg_data += recvData[2] << 0;
		pr_info("sum:%d,%d\n", reg_data, recvData[0] >> 6);

		if (device->sample_cnt >= 4) { /* ODO */
			/* Mode change */
			if (device->uv_eol_test_is_enable)
				err = max86902_uv_eol_enable_hr(device);
			else
				err = max86902_uv_enable_gesture(device);

			if (err < 0) {
				pr_err("%s - max86902_uv_eol_enable_hr err : %d\n",
					__func__, err);
				return -EIO;
			}

			*data = reg_data;
			pr_info("%s - %u\n", __func__, *data);
			ret = MAX86902_ENHANCED_UV_EOL_SUM_MODE;
		}
	}

	return ret;
}

static int max86902_uv_eol_read_data_hr(struct max86900_device_data *device, int *data)
{
	int err;
	int ret = MAX86902_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[20 * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data = 0;
	int sum_data = 0;
	int i;

	status = MAX86902_INTERRUPT_STATUS;
	err = max86900_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & A_FULL_MASK) {
		recvData[0] = MAX86902_FIFO_DATA;
		if ((err = max86900_read_reg(device, recvData,
				device->num_samples * NUM_BYTES_PER_SAMPLE)) != 0) {
			pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		for (i = 16; i < 20; i++) {
			reg_data =  recvData[i*NUM_BYTES_PER_SAMPLE+0] << 16 & 0x30000;
			reg_data += recvData[i*NUM_BYTES_PER_SAMPLE+1] << 8;
			reg_data += recvData[i*NUM_BYTES_PER_SAMPLE+2] << 0;
			pr_info("eh:%d,%d\n", reg_data, recvData[0] >> 6);
			sum_data += reg_data;
		}

		/* Mode change */
		if (device->uv_eol_test_is_enable)
			err = max86902_uv_eol_enable_vb(device);
		else
			err = max86902_uv_enable_gesture(device);

		if (err < 0) {
			pr_err("%s - max86902_uv_eol_enable_vb err : %d\n",
				__func__, err);
			return -EIO;
		}

		*data = sum_data;
		pr_info("%s - %u\n", __func__, *data);
		ret = MAX86902_ENHANCED_UV_EOL_HR_MODE;
	}

	return ret;
}

static int max86902_uv_read_data(struct max86900_device_data *device, int *data)
{
	int err;

	switch (atomic_read(&device->enhanced_uv_mode)) {
	case MAX86902_ENHANCED_UV_GESTURE_MODE:
		err = max86902_uv_read_data_gesture(device, data);
		break;
	case MAX86902_ENHANCED_UV_HR_MODE:
		err = max86902_uv_read_data_hr(device, data);
		break;
	case MAX86902_ENHANCED_UV_MODE:
		err = max86902_uv_read_data_uv(device, data);
		break;
	case MAX86902_ENHANCED_UV_EOL_VB_MODE:
		err = max86902_uv_eol_read_data_vb(device, data);
		break;
	case MAX86902_ENHANCED_UV_EOL_SUM_MODE:
		err = max86902_uv_eol_read_data_sum(device, data);
		break;
	case MAX86902_ENHANCED_UV_EOL_HR_MODE:
		err = max86902_uv_eol_read_data_hr(device, data);
		break;
	default:
		err = -EIO;
	}

	return err;
}

void max86900_hrm_mode_enable(struct max86900_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		err = max86900_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0)
			pr_err("%s max86900_regulator_on fail err = %d\n",
				__func__, err);
		usleep_range(1000, 1100);
		err = max86900_init_device(data);
		if (err)
			pr_err("%s max86900_init device fail err = %d\n",
				__func__, err);
		err = max86900_hrm_enable(data);
		if (err != 0)
			pr_err("max86900_hrm_enable err : %d\n", err);

		atomic_set(&data->hrm_is_enable, 1);
	} else {
		err = max86900_disable(data);
		if (err != 0)
			pr_err("max86900_disable err : %d\n", err);
		err = max86900_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0)
			pr_err("%s max86900_regulator_off fail err = %d\n",
				__func__, err);

		atomic_set(&data->hrm_is_enable, 0);
	}
	pr_info("%s - part_type = %u, onoff = %d\n",
		__func__, data->part_type, onoff);
}

void max86902_hrm_mode_enable(struct max86900_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		err = max86900_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0)
			pr_err("%s max86900_regulator_on fail err = %d\n",
				__func__, err);
		usleep_range(1000, 1100);
		err = max86902_init_device(data);
		if (err)
			pr_err("%s max86900_init device fail err = %d\n",
				__func__, err);
		err = max86902_hrm_enable(data);
		if (err != 0)
			pr_err("max86900_hrm_enable err : %d\n", err);

		atomic_set(&data->hrm_is_enable, 1);
	} else {
		err = max86902_disable(data);
		if (err != 0)
			pr_err("max86900_disable err : %d\n", err);
		err = max86900_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0)
			pr_err("%s max86900_regulator_off fail err = %d\n",
				__func__, err);

		atomic_set(&data->hrm_is_enable, 0);
	}
	pr_info("%s - part_type = %u, onoff = %d\n",
		__func__, data->part_type, onoff);
}

void max86900_uv_mode_enable(struct max86900_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		err = max86900_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0)
			pr_err("%s max86900_regulator_on fail err = %d\n",
				__func__, err);
		usleep_range(1000, 1100);
		err = max86900_init_device(data);
		if (err)
			pr_err("%s max86900_init device fail err = %d\n",
				__func__, err);
		err = max86900_uv_enable(data);
		if (err != 0)
			pr_err("max86900_uv_enable err : %d\n", err);

		atomic_set(&data->uv_is_enable, 1);
	} else {
		err = max86900_disable(data);
		if (err != 0)
			pr_err("max86900_disable err : %d\n", err);
		err = max86900_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0)
			pr_err("%s max86900_regulator_off fail err = %d\n",
				__func__, err);

		atomic_set(&data->uv_is_enable, 0);
	}
	pr_info("%s - part_type = %u, onoff = %d\n",
		__func__, data->part_type, onoff);
}

void max86902_uv_mode_enable(struct max86900_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		err = max86900_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0)
			pr_err("%s max86900_regulator_on fail err = %d\n",
				__func__, err);
		usleep_range(1000, 1100);
		err = max86902_init_device(data);
		if (err)
			pr_err("%s max86900_init device fail err = %d\n",
				__func__, err);
		err = max86902_uv_enable(data);
		if (err != 0)
			pr_err("max86900_uv_enable err : %d\n", err);

		atomic_set(&data->uv_is_enable, 1);
	} else {
		err = max86902_disable(data);
		if (err != 0)
			pr_err("max86900_disable err : %d\n", err);
		err = max86900_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0)
			pr_err("%s max86900_regulator_off fail err = %d\n",
				__func__, err);

		atomic_set(&data->uv_is_enable, 0);
	}
	pr_info("%s - part_type = %u, onoff = %d\n",
		__func__, data->part_type, onoff);
}


/* hrm sysfs */
static ssize_t max86900_hrm_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", atomic_read(&data->hrm_is_enable));
}

static ssize_t max86900_hrm_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
	if (data->part_type < PART_TYPE_MAX86902A) {
		if (atomic_read(&data->uv_is_enable)){
			if (new_value)
				atomic_set(&data->hrm_need_reenable, 1);
			else
				atomic_set(&data->hrm_need_reenable, 0);
		} else {
			atomic_set(&data->hrm_need_reenable, 0);
			max86900_hrm_mode_enable(data, new_value);
		}
	} else {
		if (atomic_read(&data->uv_is_enable)){
			if (new_value)
				atomic_set(&data->hrm_need_reenable, 1);
			else
				atomic_set(&data->hrm_need_reenable, 0);
		} else {
			atomic_set(&data->hrm_need_reenable, 0);
			max86902_hrm_mode_enable(data, new_value);
		}
	}

	return count;
}

static ssize_t max86900_hrm_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", 10000000LL);
}

static ssize_t max86900_hrm_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("%s - hrm sensor delay was fixed as 10ms:%d:%d\n",
		__func__, atomic_read(&data->hrm_is_enable), data->reenable_set);
	return size;
}

static struct device_attribute dev_attr_hrm_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	max86900_hrm_enable_show, max86900_hrm_enable_store);

static struct device_attribute dev_attr_hrm_poll_delay =
	__ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	max86900_hrm_poll_delay_show, max86900_hrm_poll_delay_store);

static struct attribute *hrm_sysfs_attrs[] = {
	&dev_attr_hrm_enable.attr,
	&dev_attr_hrm_poll_delay.attr,
	NULL
};

static struct attribute_group hrm_attribute_group = {
	.attrs = hrm_sysfs_attrs,
};

/* hrm led sysfs */
static ssize_t max86900_hrmled_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", atomic_read(&data->hrm_is_enable));
}

static ssize_t max86900_hrmled_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1")) {
		new_value = 1;
		atomic_set(&data->isEnable_led, 1);
	} else if (sysfs_streq(buf, "0")) {
		new_value = 0;
		atomic_set(&data->isEnable_led, 0);
	} else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("%s - start en : %d\n", __func__, new_value);

	if (data->part_type < PART_TYPE_MAX86902A) {
		if (atomic_read(&data->uv_is_enable)){
			if (new_value)
				atomic_set(&data->hrm_need_reenable, 1);
			else
				atomic_set(&data->hrm_need_reenable, 0);
		} else {
			atomic_set(&data->hrm_need_reenable, 0);
			max86900_hrm_mode_enable(data, new_value);
		}
	} else {
		if (atomic_read(&data->uv_is_enable)){
			if (new_value)
				atomic_set(&data->hrm_need_reenable, 1);
			else
				atomic_set(&data->hrm_need_reenable, 0);
		} else {
			atomic_set(&data->hrm_need_reenable, 0);
			max86902_hrm_mode_enable(data, new_value);
		}
	}

	return count;
}

static ssize_t max86900_hrmled_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", 10000000LL);
}

static ssize_t max86900_hrmled_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("%s - hrm sensor delay was fixed as 10ms:%d:%d\n",
		__func__, atomic_read(&data->hrm_is_enable), data->reenable_set);
	return size;
}

static struct device_attribute dev_attr_hrmled_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	max86900_hrmled_enable_show, max86900_hrmled_enable_store);

static struct device_attribute dev_attr_hrmled_poll_delay =
	__ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	max86900_hrmled_poll_delay_show, max86900_hrmled_poll_delay_store);

static struct attribute *hrmled_sysfs_attrs[] = {
	&dev_attr_hrmled_enable.attr,
	&dev_attr_hrmled_poll_delay.attr,
	NULL
};

static struct attribute_group hrmled_attribute_group = {
	.attrs = hrmled_sysfs_attrs,
};

/* uv sysfs */
static ssize_t max86900_uv_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", atomic_read(&data->uv_is_enable));
}

static ssize_t max86900_uv_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	data->uv_sample_cnt = 0;

	if (data->part_type < PART_TYPE_MAX86902A) {
		if (atomic_read(&data->hrm_is_enable)) {
			atomic_set(&data->hrm_need_reenable, 1);
			max86900_hrm_mode_enable(data, 0);
		}
		max86900_uv_mode_enable(data, new_value);

		if (new_value == 0 && atomic_read(&data->hrm_need_reenable) == 1) {
			atomic_set(&data->hrm_need_reenable, 0);
			max86900_hrm_mode_enable(data, 1);
		}
	} else {
		if (atomic_read(&data->hrm_is_enable)) {
			atomic_set(&data->hrm_need_reenable, 1);
			max86902_hrm_mode_enable(data, 0);
		}
		max86902_uv_mode_enable(data, new_value);

		if (new_value == 0 && atomic_read(&data->hrm_need_reenable) == 1) {
			atomic_set(&data->hrm_need_reenable, 0);
			max86902_hrm_mode_enable(data, 1);
		}
	}

	return count;
}

static ssize_t max86900_uv_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", 100000000LL);
}

static ssize_t max86900_uv_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("%s - uv sensor delay was fixed as 100ms:%d:%d\n",
		__func__, atomic_read(&data->uv_is_enable), data->reenable_set);
	return size;
}

static struct device_attribute dev_attr_uv_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	max86900_uv_enable_show, max86900_uv_enable_store);

static struct device_attribute dev_attr_uv_poll_delay =
	__ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	max86900_uv_poll_delay_show, max86900_uv_poll_delay_store);

static struct attribute *uv_sysfs_attrs[] = {
	&dev_attr_uv_enable.attr,
	&dev_attr_uv_poll_delay.attr,
	NULL
};

static struct attribute_group uv_attribute_group = {
	.attrs = uv_sysfs_attrs,
};

/* hrm test sysfs */
static int max86900_set_led_current(struct max86900_device_data *data)
{
	int err;

	err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION,
		data->led_current);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current);
	return 0;
}

static int max86902_set_led_current1(struct max86900_device_data *data)
{
	int err;

	err = max86900_write_reg(data, MAX86902_LED1_PA,
		data->led_current1);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED1_PA!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current1);
	return 0;
}

static int max86902_set_led_current2(struct max86900_device_data *data)
{
	int err;

	err = max86900_write_reg(data, MAX86902_LED2_PA,
		data->led_current2);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED2_PA!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current2);
	return 0;
}

static int max86902_set_led_current3(struct max86900_device_data *data)
{
	int err;

	err = max86900_write_reg(data, MAX86902_LED3_PA,
		data->led_current3);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED3_PA!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current3);
	return 0;
}

static int max86902_set_led_current4(struct max86900_device_data *data)
{
	int err;

	err = max86900_write_reg(data, MAX86902_LED4_PA,
		data->led_current4);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED4_PA!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current4);
	return 0;
}

static int max86900_set_hr_range(struct max86900_device_data *data)
{
	pr_info("%s - hr_range = %u(0x%x)\n", __func__,
			data->hr_range, data->hr_range);
	return 0;
}

static int max86900_set_hr_range2(struct max86900_device_data *data)
{
	pr_info("%s - hr_range2 = %u\n", __func__, data->hr_range2);
	return 0;
}

static int max86900_set_look_mode_ir(struct max86900_device_data *data)
{
	pr_info("%s - look mode ir = %u\n", __func__, data->look_mode_ir);
	return 0;
}

static int max86900_set_look_mode_red(struct max86900_device_data *data)
{
	pr_info("%s - look mode red = %u\n", __func__, data->look_mode_red);
	return 0;
}

static int max86900_hrm_eol_test_enable(struct max86900_device_data *data)
{
	int err;
	u8 led_current;
	data->led = 1; /* Prevent resetting MAX86900_LED_CONFIGURATION */
	data->sample_cnt = 0;

	pr_info("%s\n", __func__);
	mutex_lock(&data->activelock);
	/* Test Mode Setting Start */
	data->hr_range = 0; /* Set test phase as 0 */
	data->eol_test_status = 0;
	data->test_current_ir = data->look_mode_ir;
	data->test_current_red = data->look_mode_red;
	led_current = (data->test_current_red << 4) | data->test_current_ir;

	err = max86900_write_reg(data, MAX86900_INTERRUPT_ENABLE, 0x10);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_INTERRUPT_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION, led_current);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_SPO2_CONFIGURATION, 0x47);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	/* Clear FIFO */
	err = max86900_write_reg(data, MAX86900_FIFO_WRITE_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_FIFO_WRITE_POINTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_OVF_COUNTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_OVF_COUNTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_FIFO_READ_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_FIFO_READ_POINTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	/* Shutdown Clear */
	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x0B);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	mutex_unlock(&data->activelock);
	return 0;
}

static int max86902_hrm_eol_test_enable(struct max86900_device_data *data)
{
	int err;
	u8 flex_config[2] = {0, };
	data->led = 1; /* Prevent resetting MAX86902_LED_CONFIGURATION */
	data->sample_cnt = 0;
	data->num_samples = 0;
	data->flex_mode = 0;

	flex_config[0] = (IR_LED_CH << MAX86902_S2_OFFSET) | RED_LED_CH;
	flex_config[1] = 0x00;
	if (flex_config[0] & MAX86902_S1_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 0);
	}
	if (flex_config[0] & MAX86902_S2_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 1);
	}
	if (flex_config[1] & MAX86902_S3_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 2);
	}
	if (flex_config[1] & MAX86902_S4_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 3);
	}

	pr_info("%s - flexmode : 0x%02x, num_samples : %d\n", __func__,
			data->flex_mode, data->num_samples);
	mutex_lock(&data->activelock);
	/* Test Mode Setting Start */
	data->hr_range = 0; /* Set test phase as 0 */
	data->eol_test_status = 0;
	data->test_current_led1 = ((data->look_mode_ir >> 4) & 0x0f) << 4;
	data->test_current_led2 = ((data->look_mode_ir) & 0x0f) << 4;
	data->test_current_led3 = ((data->look_mode_red >> 4) & 0x0f) << 4;
	data->test_current_led4 = ((data->look_mode_red) & 0x0f) << 4;

	/*write LED currents ir=1, red=2, violet=4*/
	err = max86900_write_reg(data,
			MAX86902_LED1_PA, data->test_current_led1);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED1_PA!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	err = max86900_write_reg(data,
			MAX86902_LED2_PA, data->test_current_led2);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED2_PA!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	err = max86900_write_reg(data,
			MAX86902_LED4_PA, data->test_current_led4);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED4_PA!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_INTERRUPT_ENABLE, PPG_RDY_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_LED_FLEX_CONTROL_1,
			flex_config[0]);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED_FLEX_CONTROL_1!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	err = max86900_write_reg(data, MAX86902_LED_FLEX_CONTROL_2,
			flex_config[1]);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_LED_FLEX_CONTROL_2!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	/* 100Hz setting no average for calculating  P2P */
	err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION,
			0x07 | (MAX86902_SPO2_ADC_RGE << MAX86902_SPO2_ADC_RGE_OFFSET));
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
			(0x00 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_FIFO_WRITE_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_WRITE_POINTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_OVF_COUNTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_OVF_COUNTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_FIFO_READ_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_FIFO_READ_POINTER!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86902_MODE_CONFIGURATION, MODE_FLEX);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	/* Temperature Enable */
	err = max86900_write_reg(data, MAX86902_TEMP_CONFIG, 0x01);
	if (err != 0) {
		pr_err("%s - error initializing MAX86902_TEMP_CONFIG!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	mutex_unlock(&data->activelock);
	return 0;
}

static void max86900_eol_test_onoff(struct max86900_device_data *data, int onoff)
{
	int err;

	if (onoff) {
		err = max86900_hrm_eol_test_enable(data);
		data->eol_test_is_enable = onoff;
		if (err != 0)
			pr_err("max86900_hrm_eol_test_enable err : %d\n", err);
	} else {
		pr_info("%s - eol test off\n", __func__);
		err = max86900_disable(data);
		if (err != 0)
			pr_err("max86900_disable err : %d\n", err);

		data->hr_range = 0;
		data->led_current = data->default_current;

		err = max86900_init_device(data);
		if (err)
			pr_err("%s max86900_init device fail err = %d\n",
				__func__, err);

		err = max86900_hrm_enable(data);
		if (err != 0)
			pr_err("max86900_enable err : %d\n", err);

		data->eol_test_is_enable = 0;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

static void max86902_eol_test_onoff(struct max86900_device_data *data, int onoff)
{
	int err;

	if (onoff) {
		err = max86902_hrm_eol_test_enable(data);
		data->eol_test_is_enable = 1;
		if (err != 0)
			pr_err("max86902_hrm_eol_test_enable err : %d\n", err);
	} else {
		pr_info("%s - eol test off\n", __func__);
		err = max86902_disable(data);
		if (err != 0)
			pr_err("max86902_disable err : %d\n", err);

		data->hr_range = 0;
		data->led_current1 = data->default_current1;
		data->led_current2 = data->default_current2;
		data->led_current3 = data->default_current3;
		data->led_current4 = data->default_current4;

		err = max86902_init_device(data);
		if (err)
			pr_err("%s max86900_init device fail err = %d\n",
				__func__, err);

		err = max86902_hrm_enable(data);
		if (err != 0)
			pr_err("max86902_enable err : %d\n", err);

		data->eol_test_is_enable = 0;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

static int max86900_get_device_id(struct max86900_device_data *data, unsigned long long *device_id)
{
	u8 recvData;
	u8 reg_0x88;
	u8 reg_0x89;
	u8 reg_0x8A;
	u8 reg_0x90;
	u8 reg_0x98;
	u8 reg_0x99;
	u8 reg_0x9D;
	int err;

	if (!atomic_read(&data->uv_is_enable)
			&& !atomic_read(&data->hrm_is_enable)) {
		pr_info("%s - regulator on\n", __func__);
		err = max86900_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0) {
			pr_err("%s max86900_regulator_on fail err = %d\n",
				__func__, err);
			return -EIO;
		}
		usleep_range(1000, 1100);
	}

	*device_id = 0;

	err = max86900_write_reg(data, 0xFF, 0x54);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0xFF, 0x4d);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
			__func__);
		return -EIO;
	}

	recvData = 0x88;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	reg_0x88 = recvData;

	recvData = 0x89;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	reg_0x89 = recvData;

	recvData = 0x8A;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	reg_0x8A = recvData;

	recvData = 0x90;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	reg_0x90 = recvData;

	recvData = 0x98;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	reg_0x98 = recvData;

	recvData = 0x99;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	reg_0x99 = recvData;

	recvData = 0x9D;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	reg_0x9D = recvData;

	err = max86900_write_reg(data, 0xFF, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	if (!atomic_read(&data->uv_is_enable)
			&& !atomic_read(&data->hrm_is_enable)) {
		pr_info("%s - regulator off\n", __func__);
		err = max86900_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0) {
			pr_err("%s max86900_regulator_off fail err = %d\n",
				__func__, err);
			return -EIO;
		}
	}

	*device_id = reg_0x88 * 16 + (reg_0x89 & 0x0F);
	*device_id = *device_id * 16 + ((reg_0x8A & 0xF0) >> 4);
	*device_id = *device_id * 16 + (reg_0x8A & 0x0F);
	*device_id = *device_id * 128 + reg_0x90;
	*device_id = *device_id * 64 + reg_0x99;
	*device_id = *device_id * 64 + reg_0x98;
	*device_id = *device_id * 16 + reg_0x9D;

	pr_info("%s - Device ID = %lld\n", __func__, *device_id);

	return 0;
}

static int max86902_get_device_id(struct max86900_device_data *data, unsigned long long *device_id)
{
	u8 recvData;
	int err;
	int low = 0;
	int high = 0;
	int clock_code = 0;
	int VREF_trim_code = 0;
	int IREF_trim_code = 0;
	int UVL_trim_code = 0;
	int SPO2_trim_code = 0;
	int ir_led_code = 0;
	int red_led_code = 0;
	int TS_trim_code = 0;

	if (!atomic_read(&data->uv_is_enable)
			&& !atomic_read(&data->hrm_is_enable)) {
		pr_info("%s - regulator on\n", __func__);
		err = max86900_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0) {
			pr_err("%s max86900_regulator_on fail err = %d\n",
				__func__, err);
			return -EIO;
		}
		usleep_range(1000, 1100);
	}

	*device_id = 0;

	err = max86900_write_reg(data, 0xFF, 0x54);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0xFF, 0x4d);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
			__func__);
		return -EIO;
	}

	recvData = 0x8B;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	high = recvData;

	recvData = 0x8C;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	low = recvData;

	recvData = 0x88;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	clock_code = recvData;

	recvData = 0x89;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	VREF_trim_code = recvData & 0x0F;

	recvData = 0x8A;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	IREF_trim_code = (recvData >> 4) & 0x0F;
	UVL_trim_code = recvData & 0x0F;

	recvData = 0x90;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	SPO2_trim_code = recvData & 0x7F;

	recvData = 0x98;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	ir_led_code = (recvData >> 4) & 0x0F;
	red_led_code = recvData & 0x0F;

	recvData = 0x9D;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	TS_trim_code = recvData;

	err = max86900_write_reg(data, 0xFF, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	if (!atomic_read(&data->uv_is_enable)
			&& !atomic_read(&data->hrm_is_enable)) {
		pr_info("%s - regulator off\n", __func__);
		err = max86900_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0) {
			pr_err("%s max86900_regulator_off fail err = %d\n",
				__func__, err);
			return -EIO;
		}
	}

	*device_id = clock_code * 16 + VREF_trim_code;
	*device_id = *device_id * 16 + IREF_trim_code;
	*device_id = *device_id * 16 + UVL_trim_code;
	*device_id = *device_id * 128 + SPO2_trim_code;
	*device_id = *device_id * 64 + ir_led_code;
	*device_id = *device_id * 64 + red_led_code;
	*device_id = *device_id * 16 + TS_trim_code;

	pr_info("%s - Device ID = %lld\n", __func__, *device_id);

	return 0;
}

static int max86900_otp_id(struct max86900_device_data *data)
{
	u8 recvData;
	int err;

	err = max86900_write_reg(data, 0xFF, 0x54);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, 0xFF, 0x4d);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
			__func__);
		return -EIO;
	}

	recvData = 0x8B;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	err = max86900_write_reg(data, 0xFF, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	return recvData;

}
static ssize_t max86900_hrm_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (data->part_type < PART_TYPE_MAX86902A)
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86900_CHIP_NAME);
	else if(data->part_type < PART_TYPE_MAX86907)
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86902_CHIP_NAME);
	else if(data->part_type < PART_TYPE_MAX86907A)
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86907_CHIP_NAME);
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86907A_CHIP_NAME);
}

static ssize_t max86900_hrm_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t thres1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &data->thres1);
	if (err < 0)
		return err;

	return size;
}

static ssize_t thres2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &data->thres2);
	if (err < 0)
		return err;

	return size;
}

static ssize_t thres3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &data->thres3);
	if (err < 0)
		return err;

	return size;
}

static ssize_t thres4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtoint(buf, 10, &data->thres4);
	if (err < 0)
		return err;

	return size;
}

static ssize_t led_current_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (data->part_type < PART_TYPE_MAX86902A) {

		err = kstrtou8(buf, 10, &data->led_current);
		if (err < 0)
			return err;

		err = max86900_set_led_current(data);
		if (err < 0)
			return err;

		data->default_current = data->led_current;
	}

	return size;
}

static ssize_t led_current_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - led_current = %u\n",
		__func__, data->led_current);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current);
}

static ssize_t led_current1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current1);
	if (err < 0)
		return err;

	err = max86902_set_led_current1(data);
	if (err < 0)
		return err;

	data->default_current1 = data->led_current1;

	return size;
}

static ssize_t led_current1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - led_current1 = %u\n", __func__,
		data->led_current1);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current1);
}

static ssize_t led_current2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current2);
	if (err < 0)
		return err;

	err = max86902_set_led_current2(data);
	if (err < 0)
		return err;

	data->default_current2 = data->led_current2;

	return size;
}

static ssize_t led_current2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - led_current2 = %u\n",
		__func__, data->led_current2);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current2);
}

static ssize_t led_current3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current3);
	if (err < 0)
		return err;

	err = max86902_set_led_current3(data);
	if (err < 0)
		return err;

	data->default_current3 = data->led_current3;

	return size;
}

static ssize_t led_current3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - led_current3 = %u\n",
		__func__, data->led_current3);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current3);
}

static ssize_t led_current4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current4);
	if (err < 0)
		return err;

	err = max86902_set_led_current4(data);
	if (err < 0)
		return err;

	data->default_current4 = data->led_current4;

	return size;
}

static ssize_t led_current4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - led_current4 = %u\n",
		__func__, data->led_current4);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current4);
}

static ssize_t hr_range_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->hr_range);
	if (err < 0)
		return err;

	err = max86900_set_hr_range(data);
	if (err < 0)
		return err;

	return size;
}

static ssize_t hr_range_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - hr_range = %x\n", __func__, data->hr_range);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->hr_range);
}

static ssize_t hr_range2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->hr_range2);
	if (err < 0)
		return err;

	err = max86900_set_hr_range2(data);
	if (err < 0)
		return err;

	return size;
}

static ssize_t hr_range2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - hr_range2 = %x\n", __func__, data->hr_range2);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->hr_range2);
}

static ssize_t look_mode_ir_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->look_mode_ir);
	if (err < 0)
		return err;

	err = max86900_set_look_mode_ir(data);
	if (err < 0)
		return err;

	return size;
}

static ssize_t look_mode_ir_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - look_mode_ir = %x\n",
		__func__, data->look_mode_ir);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->look_mode_ir);
}

static ssize_t look_mode_red_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->look_mode_red);
	if (err < 0)
		return err;

	err = max86900_set_look_mode_red(data);
	if (err < 0)
		return err;

	return size;
}

static ssize_t look_mode_red_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - look_mode_red = %x\n",
		__func__, data->look_mode_red);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->look_mode_red);
}

static ssize_t eol_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int test_onoff;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* eol_test start */
		test_onoff = 1;
	else if (sysfs_streq(buf, "2")) /* eol_test for grip sensor */
		test_onoff = 2;
	else if (sysfs_streq(buf, "0")) /* eol_test stop */
		test_onoff = 0;
	else {
		pr_debug("max86900_%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (data->eol_test_is_enable == test_onoff) {
		pr_err("%s: invalid eol status Pre: %d, AF : %d\n", __func__,
			data->eol_test_is_enable, test_onoff);
		return -EINVAL;
	}

	if (data->part_type < PART_TYPE_MAX86902A)
		max86900_eol_test_onoff(data, test_onoff);
	else
		max86902_eol_test_onoff(data, test_onoff);

	return size;
}

static ssize_t eol_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->eol_test_is_enable);
}

static ssize_t eol_test_result_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	unsigned int buf_len;
	buf_len = (unsigned int)strlen(buf) + 1;
	if (buf_len > MAX_EOL_RESULT)
		buf_len = MAX_EOL_RESULT;

	mutex_lock(&data->storelock);	

	if (data->eol_test_result != NULL)
		kfree(data->eol_test_result);

	data->eol_test_result = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (data->eol_test_result == NULL) {
		pr_err("max86900_%s - couldn't allocate memory\n",
			__func__);
		mutex_unlock(&data->storelock);
		return -ENOMEM;
	}
	strncpy(data->eol_test_result, buf, buf_len);

	mutex_unlock(&data->storelock);

	pr_info("max86900_%s - result = %s, buf_len(%u)\n",
		__func__, data->eol_test_result, buf_len);
	data->eol_test_status = 1;
	return size;
}

static ssize_t eol_test_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (data->eol_test_result == NULL) {
		pr_info("max86900_%s - data->eol_test_result is NULL\n",
			__func__);
		data->eol_test_status = 0;
		return snprintf(buf, PAGE_SIZE, "%s\n", "NO_EOL_TEST");
	}
	pr_info("max86900_%s - result = %s\n", __func__, data->eol_test_result);
	data->eol_test_status = 0;
	return snprintf(buf, PAGE_SIZE, "%s\n", data->eol_test_result);
}

static ssize_t eol_test_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->eol_test_status);
}

static ssize_t int_pin_check(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int err = -1;
	int pin_state = -1;
	u8 recvData;

	/* DEVICE Power-up */
	err = max86900_regulator_onoff(data, HRM_LDO_ON);
	if (err < 0) {
		pr_err("max86900_%s - regulator on fail\n", __func__);
		goto exit;
	}

	usleep_range(1000, 1100);
	/* check INT pin state */
	pin_state = gpio_get_value_cansleep(data->hrm_int);

	if (pin_state) {
		pr_err("max86900_%s - INT pin state is high before INT clear\n",
			__func__);
		err = -1;
		max86900_regulator_onoff(data, HRM_LDO_OFF);
		goto exit;
	}

	pr_info("max86900_%s - Before INT clear %d\n", __func__, pin_state);
	/* Interrupt Clear */
	recvData = MAX86900_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("max86900_%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		max86900_regulator_onoff(data, HRM_LDO_OFF);
		goto exit;
	}

	/* check INT pin state */
	pin_state = gpio_get_value_cansleep(data->hrm_int);

	if (!pin_state) {
		pr_err("max86900_%s - INT pin state is low after INT clear\n",
			__func__);
		err = -1;
		max86900_regulator_onoff(data, HRM_LDO_OFF);
		goto exit;
	}
	pr_info("max86900_%s - After INT clear %d\n", __func__, pin_state);

	err = max86900_regulator_onoff(data, HRM_LDO_OFF);
	if (err < 0)
		pr_err("max86900_%s - regulator off fail\n", __func__);

	pr_info("max86900_%s - success\n", __func__);
exit:
	return snprintf(buf, PAGE_SIZE, "%d\n", err);
}

static ssize_t max86900_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	unsigned int buf_len;
	buf_len = (unsigned int)strlen(buf) + 1;
	if (buf_len > MAX_LIB_VER)
		buf_len = MAX_LIB_VER;

	mutex_lock(&data->storelock);	

	if (data->lib_ver != NULL)
		kfree(data->lib_ver);

	data->lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (data->lib_ver == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		mutex_unlock(&data->storelock);
		return -ENOMEM;
	}
	strncpy(data->lib_ver, buf, buf_len);

	mutex_unlock(&data->storelock);
	pr_info("%s - lib_ver = %s\n", __func__, data->lib_ver);
	return size;
}

static ssize_t max86900_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (data->lib_ver == NULL) {
		pr_info("%s - data->lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s - lib_ver = %s\n", __func__, data->lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", data->lib_ver);
}

static ssize_t regulator_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int regulator_onoff;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* Regulator On */
		regulator_onoff = HRM_LDO_ON;
	else if (sysfs_streq(buf, "0")) /* Regulator Off */
		regulator_onoff = HRM_LDO_OFF;
	else {
		pr_debug("max86900_%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (data->regulator_is_enable != regulator_onoff)
		max86900_regulator_onoff(data, regulator_onoff);

	return size;
}

static ssize_t regulator_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->regulator_is_enable);
}

static ssize_t part_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->part_type);
}

static ssize_t device_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	unsigned long long device_id = 0;
	if (data->part_type < PART_TYPE_MAX86902A)
		max86900_get_device_id(data, &device_id);
	else
		max86902_get_device_id(data, &device_id);

	return snprintf(buf, PAGE_SIZE, "%lld\n", device_id);
}

static ssize_t max86900_hrm_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int ret = 0;
	u8 handle = 0;

	ret = kstrtou8(buf, 10, &handle);
	if (ret < 0) {
		pr_err("%s - kstrtou8 failed.(%d)\n", __func__, ret);
		return ret;
	}
	pr_info("%s - handle = %d\n", __func__, handle);

	input_report_rel(data->hrm_input_dev, REL_MISC, handle);
	return size;
}

static ssize_t max86900_hrm_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int iErr = 0;

	iErr = kstrtoint(buf, 10, &data->hrm_threshold);
	if (iErr < 0) {
		pr_err("%s - kstrtoint failed.(%d)\n", __func__, iErr);
		return iErr;
	}

	pr_info("%s - threshold = %d\n", __func__, data->hrm_threshold);
	return size;
}

static ssize_t max86900_hrm_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (data->hrm_threshold) {
		pr_info("%s - threshold = %d\n", __func__, data->hrm_threshold);
		return snprintf(buf, PAGE_SIZE, "%d\n", data->hrm_threshold);
	} else {
		pr_info("%s - threshold = 0\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
}

static ssize_t max86900_hrm_alc_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int err, new_value;
	u8 recvData = 0;

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("%s - en:%d, part:%d\n", __func__, new_value, data->part_type);

	if (data->part_type < PART_TYPE_MAX86902A) { /* 86900 */
		if (new_value) { /* alc on */
			err = max86900_write_reg(data, 0xFF, 0x54);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0xFF, 0x4d);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x82, 0x00);
			if (err != 0) {
				pr_err("%s - error initializing 0x82!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x8f, 0x00);
			if (err != 0) {
				pr_err("%s - error initializing 0x8f!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0xff, 0x00);
			if (err != 0) {
				pr_err("%s - error initializing 0xff!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x07, 0x51);
			if (err != 0) {
				pr_err("%s - error initializing 0xff!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x09, 0x0f);
			if (err != 0) {
				pr_err("%s - error initializing 0xff!\n",
					__func__);
				return -EIO;
			}
		} else { /* alc off */
			err = max86900_write_reg(data, 0xFF, 0x54);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0xFF, 0x4d);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x82, 0x04);
			if (err != 0) {
				pr_err("%s - error initializing 0x80!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x8f, 0x01);
			if (err != 0) {
				pr_err("%s - error initializing 0x8f!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0xff, 0x00);
			if (err != 0) {
				pr_err("%s - error initializing 0xff!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x07, 0x31);
			if (err != 0) {
				pr_err("%s - error initializing 0xff!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x09, 0x00);
			if (err != 0) {
				pr_err("%s - error initializing 0xff!\n",
					__func__);
				return -EIO;
			}
		}
	} else { /* 86902 */
		if (new_value) { /* alc on */
			/* Mode change to AWB */
			err = max86900_write_reg(data,
				MAX86902_MODE_CONFIGURATION, 0x40);
			if (err != 0) {
				pr_err("%s - error sw shutdown data!\n", __func__);
				return -EIO;
			}

			/* Interrupt Clear */
			recvData = MAX86902_INTERRUPT_STATUS;
			if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
				pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
					__func__, err, recvData);
				return -EIO;
			}

			/* Interrupt2 Clear */
			recvData = MAX86902_INTERRUPT_STATUS_2;
			if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
				pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
					__func__, err, recvData);
				return -EIO;
			}
			/*write LED currents ir=1, red=2, violet=4*/
			err = max86900_write_reg(data, MAX86902_LED2_PA, 0xFF);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_LED2_PA!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data,
				MAX86902_INTERRUPT_ENABLE, PPG_RDY_MASK);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data,
				MAX86902_LED_FLEX_CONTROL_1, 0x12);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_LED_FLEX_CONTROL_1!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, MAX86902_SPO2_CONFIGURATION,
					0x0E | (0x03 << MAX86902_SPO2_ADC_RGE_OFFSET));
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, MAX86902_FIFO_CONFIG,
					(0x02 << MAX86902_SMP_AVE_OFFSET) & MAX86902_SMP_AVE_MASK);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_FIFO_CONFIG!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data,
				MAX86902_MODE_CONFIGURATION, MODE_FLEX);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
					__func__);
				return -EIO;
			}
			data->awb_sample_cnt = 0;
			data->flicker_data_cnt = 0;
			atomic_set(&data->alc_is_enable, 1);
		} else { /* alc off */
			/* Mode change to AWB */
			err = max86900_write_reg(data,
				MAX86902_MODE_CONFIGURATION, 0x40);
			if (err != 0) {
				pr_err("%s - error sw shutdown data!\n", __func__);
				return -EIO;
			}

			/* Interrupt Clear */
			recvData = MAX86902_INTERRUPT_STATUS;
			if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
				pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
					__func__, err, recvData);
				return -EIO;
			}

			/* Interrupt2 Clear */
			recvData = MAX86902_INTERRUPT_STATUS_2;
			if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
				pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
					__func__, err, recvData);
				return -EIO;
			}
			err = max86900_write_reg(data, 0xFF, 0x54);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST0!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0xFF, 0x4d);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST1!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x82, 0x04);
			if (err != 0) {
				pr_err("%s - error initializing 0x82!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0x8f, 0x01); //PW_EN = 0
			if (err != 0) {
				pr_err("%s - error initializing 0x8f!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data, 0xFF, 0x00);
			if (err != 0) {
				pr_err("%s - error initializing MAX86900_MODE_TEST3!\n",
					__func__);
				return -EIO;
			}
			/* 400Hz, LED_PW=400us, SPO2_ADC_RANGE=2048nA */
			err = max86900_write_reg(data,
				MAX86902_SPO2_CONFIGURATION, 0x0F);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_SPO2_CONFIGURATION!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data,
				MAX86902_INTERRUPT_ENABLE, PPG_RDY_MASK);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_INTERRUPT_ENABLE!\n",
					__func__);
				return -EIO;
			}
			err = max86900_write_reg(data,
					MAX86902_MODE_CONFIGURATION, 0x02);
			if (err != 0) {
				pr_err("%s - error initializing MAX86902_MODE_CONFIGURATION!\n",
					__func__);
				return -EIO;
			}
			data->awb_sample_cnt = 0;
			data->flicker_data_cnt = 0;
			data->awb_flicker_status = AWB_CONFIG1;
			data->previous_awb_data = 0;
			atomic_set(&data->alc_is_enable, 0);
		}
	}

	return size;
}

static struct device_attribute dev_attr_hrm_name =
		__ATTR(name, S_IRUGO, max86900_hrm_name_show, NULL);
static struct device_attribute dev_attr_hrm_vendor =
		__ATTR(vendor, S_IRUGO, max86900_hrm_vendor_show, NULL);

static DEVICE_ATTR(thres1, S_IWUSR | S_IWGRP,
	NULL, thres1_store);
static DEVICE_ATTR(thres2, S_IWUSR | S_IWGRP,
	NULL, thres2_store);
static DEVICE_ATTR(thres3, S_IWUSR | S_IWGRP,
	NULL, thres3_store);
static DEVICE_ATTR(thres4, S_IWUSR | S_IWGRP,
	NULL, thres4_store);
static DEVICE_ATTR(led_current, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current_show, led_current_store);
static DEVICE_ATTR(led_current1, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current1_show, led_current1_store);
static DEVICE_ATTR(led_current2, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current2_show, led_current2_store);
static DEVICE_ATTR(led_current3, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current3_show, led_current3_store);
static DEVICE_ATTR(led_current4, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current4_show, led_current4_store);
static DEVICE_ATTR(hr_range, S_IRUGO | S_IWUSR | S_IWGRP,
	hr_range_show, hr_range_store);
static DEVICE_ATTR(hr_range2, S_IRUGO | S_IWUSR | S_IWGRP,
	hr_range2_show, hr_range2_store);
static DEVICE_ATTR(look_mode_ir, S_IRUGO | S_IWUSR | S_IWGRP,
	look_mode_ir_show, look_mode_ir_store);
static DEVICE_ATTR(look_mode_red, S_IRUGO | S_IWUSR | S_IWGRP,
	look_mode_red_show, look_mode_red_store);
static DEVICE_ATTR(eol_test, S_IRUGO | S_IWUSR | S_IWGRP,
	eol_test_show, eol_test_store);
static DEVICE_ATTR(eol_test_result, S_IRUGO | S_IWUSR | S_IWGRP,
	eol_test_result_show, eol_test_result_store);
static DEVICE_ATTR(eol_test_status, S_IRUGO, eol_test_status_show, NULL);
static DEVICE_ATTR(int_pin_check, S_IRUGO, int_pin_check, NULL);
static DEVICE_ATTR(lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	max86900_lib_ver_show, max86900_lib_ver_store);
static DEVICE_ATTR(regulator, S_IRUGO | S_IWUSR | S_IWGRP,
	regulator_show, regulator_store);
static DEVICE_ATTR(part_type, S_IRUGO, part_type_show, NULL);
static DEVICE_ATTR(device_id, S_IRUGO, device_id_show, NULL);
static DEVICE_ATTR(hrm_flush, S_IWUSR | S_IWGRP,
	NULL, max86900_hrm_flush_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
	max86900_hrm_threshold_show, max86900_hrm_threshold_store);
static DEVICE_ATTR(alc_enable, S_IWUSR | S_IWGRP,
	NULL, max86900_hrm_alc_enable_store);

static struct device_attribute *hrm_sensor_attrs[] = {
	&dev_attr_hrm_name,
	&dev_attr_hrm_vendor,
	&dev_attr_thres1,
	&dev_attr_thres2,
	&dev_attr_thres3,
	&dev_attr_thres4,
	&dev_attr_led_current,
	&dev_attr_led_current1,
	&dev_attr_led_current2,
	&dev_attr_led_current3,
	&dev_attr_led_current4,
	&dev_attr_hr_range,
	&dev_attr_hr_range2,
	&dev_attr_look_mode_ir,
	&dev_attr_look_mode_red,
	&dev_attr_eol_test,
	&dev_attr_eol_test_result,
	&dev_attr_eol_test_status,
	&dev_attr_int_pin_check,
	&dev_attr_lib_ver,
	&dev_attr_regulator,
	&dev_attr_part_type,
	&dev_attr_device_id,
	&dev_attr_hrm_flush,
	&dev_attr_threshold,
	&dev_attr_alc_enable,
	NULL,
};

static ssize_t max86900_hrmled_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (data->part_type < PART_TYPE_MAX86902A)
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86900_CHIP_NAME);
	else if(data->part_type < PART_TYPE_MAX86907)
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86902_CHIP_NAME);
	else if(data->part_type < PART_TYPE_MAX86907A)
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86907_CHIP_NAME);
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86907A_CHIP_NAME);
}

static ssize_t max86900_hrmled_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t max86900_hrmled_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int ret = 0;
	u8 handle = 0;

	ret = kstrtou8(buf, 10, &handle);
	if (ret < 0) {
		pr_err("%s - kstrtou8 failed.(%d)\n", __func__, ret);
		return ret;
	}
	pr_info("%s - handle = %d\n", __func__, handle);

	input_report_rel(data->hrmled_input_dev, REL_MISC, handle);
	return size;
}

static struct device_attribute dev_attr_hrmled_name =
		__ATTR(name, S_IRUGO, max86900_hrmled_name_show, NULL);
static struct device_attribute dev_attr_hrmled_vendor =
		__ATTR(vendor, S_IRUGO, max86900_hrmled_vendor_show, NULL);
static DEVICE_ATTR(hrmled_flush, S_IWUSR | S_IWGRP,
	NULL, max86900_hrmled_flush_store);

static struct device_attribute *hrmled_sensor_attrs[] = {
	&dev_attr_hrmled_name,
	&dev_attr_hrmled_vendor,
	&dev_attr_hrmled_flush,
	NULL,
};

static void max86902_uv_eol_test_onoff(struct max86900_device_data *data, int onoff)
{
	int err;

	if (onoff) {
		err = max86902_uv_eol_test_enable(data);
		data->uv_eol_test_is_enable = 1;
		if (err != 0)
			pr_err("%s - max86902_uv_eol_test_enable err : %d\n",
			__func__, err);
	} else {
		pr_info("%s - eol test off\n", __func__);
		err = max86902_disable(data);
		if (err != 0)
			pr_err("%s - max86902_disable err : %d\n",
			__func__, err);

		err = max86902_init_device(data);
		if (err)
			pr_err("%s - max86900_init device fail err = %d\n",
				__func__, err);

		err = max86902_uv_enable(data);
		if (err != 0)
			pr_err("%s - max86902_enable err : %d\n",
			__func__, err);

		data->uv_eol_test_is_enable = 0;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

/* uv test sysfs */
static ssize_t max86900_uv_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	if (data->part_type < PART_TYPE_MAX86902A)
		return snprintf(buf, PAGE_SIZE, "%s\n", "MAX86900_UV");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "MAX86902_UV");
}

static ssize_t max86900_uv_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t max86900_uv_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	unsigned int buf_len;
	buf_len = (unsigned int)strlen(buf) + 1;
	if (buf_len > MAX_LIB_VER)
		buf_len = MAX_LIB_VER;

	mutex_lock(&data->storelock);

	if (data->uv_lib_ver != NULL)
		kfree(data->uv_lib_ver);

	data->uv_lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (data->uv_lib_ver == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		mutex_unlock(&data->storelock);
		return -ENOMEM;
	}
	strncpy(data->uv_lib_ver, buf, buf_len);

	mutex_unlock(&data->storelock);

	pr_info("%s - uv_lib_ver = %s\n", __func__, data->uv_lib_ver);
	return size;
}

static ssize_t max86900_uv_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (data->uv_lib_ver == NULL) {
		pr_info("%s - data->uv_lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s - lib_ver = %s\n", __func__, data->uv_lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", data->uv_lib_ver);
}

static ssize_t max86900_uv_sr_interval_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou16(buf, 10, &data->uv_sr_interval);
	if (err < 0)
		return err;

	return size;
}

static ssize_t max86900_uv_sr_interval_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - uv_sr_interval = %u\n",
		__func__, data->uv_sr_interval);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->uv_sr_interval);
}

static ssize_t uv_eol_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int test_onoff;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_err("%s: buf : %s\n", __func__, buf);

	if (sysfs_streq(buf, "1")) /* eol_test start */
		test_onoff = 1;
	else if (sysfs_streq(buf, "0")) /* eol_test stop */
		test_onoff = 0;
	else {
		pr_debug("max86900_%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (data->uv_eol_test_is_enable == test_onoff) {
		pr_err("%s: invalid eol status Pre: %d, AF : %d\n", __func__,
			data->uv_eol_test_is_enable, test_onoff);
		return -EINVAL;
	}

	pr_err("%s: test_onoff : %d\n", __func__, test_onoff);

	max86902_uv_eol_test_onoff(data, test_onoff);

	return size;
}

static ssize_t uv_eol_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->uv_eol_test_is_enable);
}

static ssize_t max86900_uv_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int ret = 0;
	u8 handle = 0;

	ret = kstrtou8(buf, 10, &handle);
	if (ret < 0) {
		pr_err("%s - kstrtou8 failed.(%d)\n", __func__, ret);
		return ret;
	}
	pr_info("%s - handle = %d\n", __func__, handle);

	input_report_rel(data->uv_input_dev, REL_MISC, handle);
	return size;
}

static struct device_attribute dev_attr_uv_name =
		__ATTR(name, S_IRUGO, max86900_uv_name_show, NULL);
static struct device_attribute dev_attr_uv_vendor =
		__ATTR(vendor, S_IRUGO, max86900_uv_vendor_show, NULL);
static DEVICE_ATTR(uv_lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	max86900_uv_lib_ver_show, max86900_uv_lib_ver_store);
static DEVICE_ATTR(uv_sr_interval, S_IRUGO | S_IWUSR | S_IWGRP,
	max86900_uv_sr_interval_show, max86900_uv_sr_interval_store);
static DEVICE_ATTR(uv_eol_test, S_IRUGO | S_IWUSR | S_IWGRP,
	uv_eol_test_show, uv_eol_test_store);
static DEVICE_ATTR(uv_flush, S_IWUSR | S_IWGRP,
	NULL, max86900_uv_flush_store);

static struct device_attribute *uv_sensor_attrs[] = {
	&dev_attr_uv_name,
	&dev_attr_uv_vendor,
	&dev_attr_uv_lib_ver,
	&dev_attr_uv_sr_interval,
	&dev_attr_uv_eol_test,
	&dev_attr_uv_flush,
	NULL,
};

static void max86900_hrm_irq_handler(struct max86900_device_data *data)
{
	int err;
	u16 raw_data[4] = {0x00, };

	err = max86900_hrm_read_data(data, raw_data);
	if (err < 0)
		pr_err("max86900_hrm_read_data err : %d\n", err);

	if (err == 0) {
		if (atomic_read(&data->isEnable_led)) {
			input_report_rel(data->hrmled_input_dev, REL_X,  raw_data[0] + 1); /* IR */
			input_report_rel(data->hrmled_input_dev, REL_Y,  raw_data[1] + 1); /* RED */
			input_sync(data->hrmled_input_dev);
		} else {
			input_report_rel(data->hrm_input_dev, REL_X, raw_data[0] + 1); /* IR */
			input_report_rel(data->hrm_input_dev, REL_Y, raw_data[1] + 1); /* RED */
			input_report_rel(data->hrm_input_dev, REL_Z, data->hrm_temp + 1);
			input_sync(data->hrm_input_dev);
		}
	}

	return;
}

static void max86902_hrm_irq_handler(struct max86900_device_data *data)
{
	int err;
	int raw_data[5] = {0x00, };

	err = max86902_hrm_read_data(data, raw_data);
	if (err < 0)
		pr_err("max86902_hrm_read_data err : %d\n", err);

	if (err == 0) {
		if (atomic_read(&data->isEnable_led)) {
			input_report_rel(data->hrmled_input_dev, REL_X,  raw_data[0] + 1); /* IR */
			input_report_rel(data->hrmled_input_dev, REL_Y,  raw_data[1] + 1); /* RED */
			input_sync(data->hrmled_input_dev);
		} else {
			input_report_rel(data->hrm_input_dev, REL_X,  raw_data[0] + 1); /* IR */
			input_report_rel(data->hrm_input_dev, REL_Y,  raw_data[1] + 1); /* RED */
			input_report_rel(data->hrm_input_dev, REL_Z,  data->hrm_temp + 1);  /* Temperature */
			input_report_rel(data->hrm_input_dev, REL_RX, raw_data[2] + 1);  /* VIOLET */
			input_report_rel(data->hrm_input_dev, REL_RY, raw_data[3] + 1); /* LED4 */
			input_sync(data->hrm_input_dev);
		}
	}

	return;
}

static void max86902_awb_flicker_handler(struct max86900_device_data *data)
{
	int err;
	int raw_data = 0;

	err = max86902_awb_flicker_read_data(data, &raw_data);
	if (err < 0)
		pr_err("max86902_awb_flicker_read_data err : %d\n", err);

	if (err == 0) {
			input_report_rel(data->hrm_input_dev, REL_X,  raw_data + 1); /* IR */
			if (data->flicker_data_cnt == FLICKER_DATA_CNT) {
				input_report_rel(data->hrm_input_dev, REL_Y,  -1); /* Report Flicker */
				data->flicker_data_cnt = 0;
			}
			else
				input_report_rel(data->hrm_input_dev, REL_Y,  1); /* IR */
			input_sync(data->hrm_input_dev);
	}

	return;
}

static void max86900_uv_irq_handler(struct max86900_device_data *data)
{
	int err;
	u16 raw_data = 0;

	err = max86900_uv_read_data(data, &raw_data);
	if (err < 0)
		pr_err("max86900_uv_read_data err : %d\n", err);

	if (err == 0) {
		input_report_rel(data->uv_input_dev, REL_X, raw_data + 1); /* UV Data */
		input_report_rel(data->uv_input_dev, REL_Y, data->hrm_temp + 1);
		input_sync(data->uv_input_dev);
	}

	return;
}

static void max86902_uv_irq_handler(struct max86900_device_data *data)
{
	int err;
	int raw_data = 0;

	err = max86902_uv_read_data(data, &raw_data);
	if (err < 0)
		pr_err("max86900_uv_read_data err : %d\n", err);

	if (data->uv_sample_cnt == MAX86900_COUNT_MAX)
		data->uv_sample_cnt = 0;
	else
		data->uv_sample_cnt++;

	if ((data->uv_sample_cnt % 168) < 7)
		dbg_enable = 1;
	else
		dbg_enable = 0;

	switch(err) {
	case MAX86902_ENHANCED_UV_MODE:
		input_report_rel(data->uv_input_dev, REL_X, raw_data + 1); /* UV Data */
		input_report_rel(data->uv_input_dev, REL_Y, data->hrm_temp + 1);
		input_sync(data->uv_input_dev);
		break;
	case MAX86902_ENHANCED_UV_GESTURE_MODE:
	case MAX86902_ENHANCED_UV_HR_MODE:
	case MAX86902_ENHANCED_UV_EOL_VB_MODE:
	case MAX86902_ENHANCED_UV_EOL_SUM_MODE:
	case MAX86902_ENHANCED_UV_EOL_HR_MODE:
		input_report_rel(data->uv_input_dev, REL_X, raw_data + 1); /* UV Data */
		input_report_rel(data->uv_input_dev, REL_Y, -err);
		input_sync(data->uv_input_dev);
		break;
	default:
		return;
	}
	return;
}

irqreturn_t max86900_irq_handler(int irq, void *device)
{
	struct max86900_device_data *data = device;
	u8 recvData;
	int err;

	if (data->reenable_set || data->reenable_cnt == MAX86902_REENABLE_MAX_CNT) {
		cancel_delayed_work(&data->reenable_work_queue);
		data->reenable_set = MAX86902_REENABLE_OFF;
		data->reenable_cnt = 0;
		pr_info("%s - cancel workqueue for reenable\n",
			__func__);
	}

	if (data->part_type < PART_TYPE_MAX86902A) {
		if (atomic_read(&data->hrm_is_enable))
			max86900_hrm_irq_handler(data);
		else if (atomic_read(&data->uv_is_enable))
			max86900_uv_irq_handler(data);
	} else {
		if (atomic_read(&data->hrm_is_enable)) {
			if (atomic_read(&data->alc_is_enable))
				max86902_hrm_irq_handler(data);
			else
				max86902_awb_flicker_handler(data);
		}
		else if (atomic_read(&data->uv_is_enable))
			max86902_uv_irq_handler(data);
	}

	/* Interrupt Clear */
	recvData = MAX86902_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	/* Interrupt2 Clear */
	recvData = MAX86902_INTERRUPT_STATUS_2;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	return IRQ_HANDLED;
}

static void uv_sr_set(struct work_struct *w)
{
	int err;
	struct delayed_work *work_queue;
	struct max86900_device_data *data;

	work_queue = container_of(w, struct delayed_work, work);
	data = container_of(work_queue,
		struct max86900_device_data, uv_sr_work_queue);

	schedule_delayed_work(work_queue,
		msecs_to_jiffies(data->uv_sr_interval / 2));
	/*Ready to Enable UV ADC convert*/
	err = max86900_write_reg(data, MAX86900_TEST_ENABLE_PLETH, 0x00);
	if (err != 0) {
		printk("%s - error init MAX86900_TEST_ENABLE_PLETH!\n",
			__func__);
		return;
	}
	/*Enable UV ADC convert*/
	err = max86900_write_reg(data, MAX86900_TEST_ENABLE_PLETH, 0x01);
	if (err != 0) {
		printk("%s - error init MAX86900_TEST_ENABLE_PLETH!\n",
			__func__);
		return;
	}

	return;
}

static void max86900_reenable_set(struct work_struct *w)
{
	int err;
	struct delayed_work *work_queue;
	struct max86900_device_data *data;

	work_queue = container_of(w, struct delayed_work, work);
	data = container_of(work_queue,
		struct max86900_device_data, reenable_work_queue);

	pr_info("%s - start. renable cnt : %d\n", __func__,
		data->reenable_cnt);
	data->reenable_cnt++;

	/* first of all, disable irq for reenable of uvsensor */
	mutex_lock(&data->activelock);
	irq_set_state(data, MAX86902_IRQ_DISABLE);
	mutex_unlock(&data->activelock);

	if (data->reenable_set == MAX86902_REENABLE_HRM) {
		err = max86902_init_device(data);
		if (err)
			pr_err("%s max86900_init device fail err = %d\n",
				__func__, err);
		err = max86902_hrm_enable(data);
		if (err != 0)
			pr_err("max86900_hrm_enable err : %d\n", err);

		if (data->eol_test_is_enable) {
			usleep_range(3000, 4000);
			pr_info("%s - now hrm eol mode\n", __func__);
			max86902_eol_test_onoff(data, 1);
		}
	} else if (data->reenable_set == MAX86902_REENABLE_UV) {
		err = max86902_uv_enable(data);
		if (err != 0)
			pr_err("max86902_uv_enable err : %d\n", err);

		if (data->uv_eol_test_is_enable) {
			usleep_range(3000, 4000);
			pr_info("%s - now uv eol mode\n", __func__);
			max86902_uv_eol_test_onoff(data, 1);
		}
	}
}

static int max86900_parse_dt(struct max86900_device_data *data,
	struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;

	data->hrm_int = of_get_named_gpio_flags(dNode,
		"max86900,hrm_int-gpio", 0, &flags);
	if (data->hrm_int < 0) {
		pr_err("%s - get hrm_int error\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(dNode, "max86900,vdd_1p8",
		&data->vdd_1p8) < 0)
		pr_err("%s - get vdd_1p8 error\n", __func__);

	if (of_property_read_string(dNode, "max86900,led_3p3",
		&data->led_3p3) < 0)
		pr_err("%s - get led_3p3 error\n", __func__);

	if (of_property_read_u32(dNode, "max86900,dual-hrm", &data->dual_hrm))
		data->dual_hrm = 0;

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
}

static int max86900_gpio_setup(struct max86900_device_data *data)
{
	int errorno = -EIO;

	errorno = gpio_request(data->hrm_int, "hrm_int");
	if (errorno) {
		pr_err("%s - failed to request hrm_int\n", __func__);
		return errorno;
	}

	errorno = gpio_direction_input(data->hrm_int);
	if (errorno) {
		pr_err("%s - failed to set hrm_int as input\n", __func__);
		goto err_gpio_direction_input;
	}
	data->irq = gpio_to_irq(data->hrm_int);

	goto done;
err_gpio_direction_input:
	gpio_free(data->hrm_int);
done:
	return errorno;
}

static int max86900_setup_irq(struct max86900_device_data *data)
{
	int errorno = -EIO;

	errorno = request_threaded_irq(data->irq, NULL,
		max86900_irq_handler, IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
		"hrm_sensor_irq", data);

	if (errorno < 0) {
		pr_err("%s - failed for setup irq errono= %d\n",
			   __func__, errorno);
		errorno = -ENODEV;
		return errorno;
	}

	disable_irq(data->irq);

	return errorno;
}

static long max86900_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	struct max86900_device_data *data = container_of(file->private_data,
				struct max86900_device_data, miscdev);

	pr_err("%s - ioctl start\n", __func__);
	mutex_lock(&data->flickerdatalock);
	switch (cmd) {
		case MAX86900_IOCTL_READ_FLICKER:
			ret = copy_to_user(argp,
					data->flicker_data,
					sizeof(int)*FLICKER_DATA_CNT);
			if (unlikely(ret)) {
				pr_err("%s - read flicker data err(%d)\n", __func__, ret);
				goto ioctl_error;
			}
			break;
		default:
			pr_err("%s - invalid cmd\n", __func__);
			break;
	}
	mutex_unlock(&data->flickerdatalock);
	return ret;

ioctl_error:
	mutex_unlock(&data->flickerdatalock);
	return -ret;
}

static const struct file_operations max86900_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.unlocked_ioctl = max86900_ioctl,
};

int max86900_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = -ENODEV;
	u8 buffer[2] = {0, };
	struct max86900_device_data *data;

	pr_info("%s - start\n", __func__);
	/* check to make sure that the adapter supports I2C */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s - I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	/* allocate some memory for the device */
	data = kzalloc(sizeof(struct max86900_device_data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		return -ENOMEM;
	}

	data->client = client;
	data->dev = &client->dev;
	i2c_set_clientdata(client, data);

	mutex_init(&data->i2clock);
	mutex_init(&data->activelock);
	mutex_init(&data->flickerdatalock);
	mutex_init(&data->storelock);
	data->irq_state = 0;
	data->hr_range = 0;
	data->skip_i2c_msleep = 1;
	data->regulator_is_enable = 0;

	err = max86900_parse_dt(data, &client->dev);
	if (err < 0) {
		pr_err("[SENSOR] %s - of_node error\n", __func__);
		err = -ENODEV;
		goto err_of_node;
	}

	err = max86900_gpio_setup(data);
	if (err) {
		pr_err("[SENSOR] %s - could not setup gpio\n", __func__);
		goto err_setup_gpio;
	}

	err = max86900_regulator_onoff(data, HRM_LDO_ON);
	if (err < 0) {
		pr_err("%s max86900_regulator_on fail err = %d\n",
			__func__, err);
		goto err_regulator_enable;
	}
	usleep_range(1000, 1100);

	data->client->addr = MAX86902_SLAVE_ADDR;

	buffer[0] = MAX86902_WHOAMI_REG_PART;
	err = max86900_read_reg(data, buffer, 1);

	if (buffer[0] == MAX86902_PART_ID1 ||
		buffer[0] == MAX86902_PART_ID2) {/*Max86902*/
		buffer[0] = MAX86902_WHOAMI_REG_REV;
		err = max86900_read_reg(data, buffer, 1);
		if (err) {
			pr_err("%s Max86902 WHOAMI read fail\n", __func__);
			err = -ENODEV;
			goto err_of_read_chipid;
		}
		if (buffer[0] == MAX86902_REV_ID1)
			data->part_type = PART_TYPE_MAX86902A;
		else if (buffer[0] == MAX86902_REV_ID2) {
			if (max86900_otp_id(data) == MAX86907_OTP_ID)
				data->part_type = PART_TYPE_MAX86907;
			else if (max86900_otp_id(data) == MAX86907A_OTP_ID)
				data->part_type = PART_TYPE_MAX86907A;
			else
				data->part_type = PART_TYPE_MAX86902B;
		} else {
			pr_err("%s Max86902 WHOAMI read error : REV ID : 0x%02x\n",
					__func__, buffer[0]);
					err = -ENODEV;
					goto err_of_read_chipid;
		}
		data->default_current1 = MAX86902_DEFAULT_CURRENT1;
		data->default_current2 = MAX86902_DEFAULT_CURRENT2;
		data->default_current3 = MAX86902_DEFAULT_CURRENT3;
		data->default_current4 = MAX86902_DEFAULT_CURRENT4;
	} else {
		data->client->addr = MAX86900A_SLAVE_ADDR;
		buffer[0] = MAX86900_WHOAMI_REG;
		err = max86900_read_reg(data, buffer, 2);

		if (buffer[1] == MAX86900C_WHOAMI) {
			/* MAX86900A & MAX86900B */
			switch (buffer[0]) {
			case MAX86900A_REV_ID:
				data->part_type = PART_TYPE_MAX86900A;
				data->default_current =	MAX86900A_DEFAULT_CURRENT;
				break;
			case MAX86900B_REV_ID:
				data->part_type = PART_TYPE_MAX86900B;
				data->default_current = MAX86900A_DEFAULT_CURRENT;
				break;
			case MAX86900C_REV_ID:
				if (max86900_otp_id(data) == MAX86906_OTP_ID) {
					data->part_type = PART_TYPE_MAX86906;
					data->default_current = MAX86906_DEFAULT_CURRENT;
				} else {
					data->part_type = PART_TYPE_MAX86900C;
					data->default_current = MAX86900C_DEFAULT_CURRENT;
				}
				break;
			default:
				pr_err("%s WHOAMI read error : REV ID : 0x%02x\n",
				__func__, buffer[0]);
				err = -ENODEV;
				goto err_of_read_chipid;
			}
			pr_info("%s - MAX86900 OS21(0x%X), REV ID : 0x%02x\n",
				__func__, MAX86900A_SLAVE_ADDR, buffer[0]);
		} else {
			/* MAX86900 */
			data->client->addr = MAX86900_SLAVE_ADDR;
			buffer[0] = MAX86900_WHOAMI_REG;
			err = max86900_read_reg(data, buffer, 2);

			if (err) {
				pr_err("%s WHOAMI read fail\n", __func__);
				err = -ENODEV;
				goto err_of_read_chipid;
			}
			data->part_type = PART_TYPE_MAX86900;
			data->default_current = MAX86900_DEFAULT_CURRENT;
			pr_info("%s - MAX86900 OS20 (0x%X)\n", __func__,
					MAX86900_SLAVE_ADDR);
		}
	}

	data->uv_sr_interval = MAX86902_DEFAULT_UV_SR_INTERVAL;

	data->led_current = data->default_current;
	data->led_current1 = data->default_current1;
	data->led_current2 = data->default_current2;
	data->led_current3 = data->default_current3;
	data->led_current4 = data->default_current4;

	/* allocate input device for HRM*/
	data->hrm_input_dev = input_allocate_device();
	if (!data->hrm_input_dev) {
		pr_err("%s - could not allocate input device\n",
			__func__);
		goto err_hrm_input_allocate_device;
	}

	input_set_drvdata(data->hrm_input_dev, data);
	data->hrm_input_dev->name = MODULE_NAME_HRM;
	input_set_capability(data->hrm_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Z);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_RX);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_RY);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_MISC);

	err = input_register_device(data->hrm_input_dev);
	if (err < 0) {
		pr_err("%s - could not register input device\n", __func__);
		goto err_hrm_input_register_device;
	}

	err = sensors_create_symlink(data->hrm_input_dev);
	if (err < 0) {
		pr_err("%s - create_symlink error\n", __func__);
		goto err_hrm_sensors_create_symlink;
	}

	err = sysfs_create_group(&data->hrm_input_dev->dev.kobj,
				 &hrm_attribute_group);
	if (err) {
		pr_err("[SENSOR] %s - could not create sysfs group\n",
			__func__);
		goto err_hrm_sysfs_create_group;
	}

	/* set sysfs for hrm sensor */
	err = sensors_register(data->dev, data, hrm_sensor_attrs,
			MODULE_NAME_HRM);
	if (err) {
		pr_err("[SENSOR] %s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	/* allocate input device for HRM*/
	data->hrmled_input_dev = input_allocate_device();
	if (!data->hrmled_input_dev) {
		pr_err("%s - could not allocate input device\n",
			__func__);
		goto err_hrm_input_allocate_device;
	}

	input_set_drvdata(data->hrmled_input_dev, data);
	data->hrmled_input_dev->name = MODULE_NAME_HRM_LED;
	input_set_capability(data->hrmled_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrmled_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrmled_input_dev, EV_REL, REL_MISC);

	err = input_register_device(data->hrmled_input_dev);
	if (err < 0) {
		input_free_device(data->hrmled_input_dev);
		pr_err("%s - could not register input device\n", __func__);
		goto err_hrm_input_register_device;
	}

	err = sensors_create_symlink(data->hrmled_input_dev);
	if (err < 0) {
		pr_err("%s - create_symlink error\n", __func__);
		goto err_hrm_sensors_create_symlink;
	}

	err = sysfs_create_group(&data->hrmled_input_dev->dev.kobj,
				 &hrmled_attribute_group);
	if (err) {
		pr_err("[SENSOR] %s - could not create sysfs group\n",
			__func__);
		goto err_hrm_sysfs_create_group;
	}

	/* set sysfs for hrm led sensor */
	err = sensors_register(data->dev, data, hrmled_sensor_attrs,
			MODULE_NAME_HRM_LED);
	if (err) {
		pr_err("[SENSOR] %s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	/* set initial AWB_CONFIG threshold */
	data->thres1 = AWB_CONFIG_TH1;
	data->thres2 = AWB_CONFIG_TH2;
	data->thres3 = AWB_CONFIG_TH3;
	data->thres4 = AWB_CONFIG_TH4;

	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name = "max_hrm";
	data->miscdev.fops = &max86900_fops;
	data->miscdev.mode = S_IRUGO;
	err = misc_register(&data->miscdev);
	if (err < 0) {
		pr_err("%s - failed to register Device\n", __func__);
		goto err_flicker_miscdev;
	}
	data->flicker_data = kzalloc(sizeof(int)*FLICKER_DATA_CNT, GFP_KERNEL);
	if (data->flicker_data == NULL) {
		pr_err("%s - couldn't allocate flicker memory\n", __func__);
		goto err_flicker_alloc_data;
	}

	if (data->part_type >= PART_TYPE_MAX86902A) {
		/* allocate input device for UV*/
		data->uv_input_dev = input_allocate_device();
		if (!data->uv_input_dev) {
			pr_err("%s - could not allocate input device\n",
				__func__);
			goto err_uv_input_allocate_device;
		}

		input_set_drvdata(data->uv_input_dev, data);
		data->uv_input_dev->name = MODULE_NAME_UV;
		input_set_capability(data->uv_input_dev, EV_REL, REL_X);
		input_set_capability(data->uv_input_dev, EV_REL, REL_Y);
		input_set_capability(data->uv_input_dev, EV_REL, REL_MISC);

		err = input_register_device(data->uv_input_dev);
		if (err < 0) {
			pr_err("%s - could not register input device\n", __func__);
			goto err_uv_input_register_device;
		}

		err = sensors_create_symlink(data->uv_input_dev);
		if (err < 0) {
			pr_err("%s - create_symlink error\n", __func__);
			goto err_uv_sensors_create_symlink;
		}

		err = sysfs_create_group(&data->uv_input_dev->dev.kobj,
					 &uv_attribute_group);
		if (err) {
			pr_err("[SENSOR] %s - could not create sysfs group\n",
				__func__);
			goto err_uv_sysfs_create_group;
		}

		/* set sysfs for uv sensor */
		err = sensors_register(data->dev, data, uv_sensor_attrs,
				MODULE_NAME_UV);
		if (err) {
			pr_err("[SENSOR] %s - cound not register hrm_sensor(%d).\n",
				__func__, err);
			goto uv_sensor_register_failed;
		}
	}

	err = max86900_setup_irq(data);
	if (err) {
		pr_err("[SENSOR] %s - could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	INIT_DELAYED_WORK(&data->uv_sr_work_queue, uv_sr_set);
	INIT_DELAYED_WORK(&data->reenable_work_queue, max86900_reenable_set);
	data->reenable_set = MAX86902_REENABLE_OFF;
	data->reenable_cnt = 0;
	data->eol_test_is_enable = 0;
	data->uv_eol_test_is_enable = 0;

	/* Init Device */
	err = max86900_init_device(data);
	if (err) {
		pr_err("%s max86900_init device fail err = %d\n", __func__, err);
		goto max86900_init_device_failed;
	}

	dev_set_drvdata(data->dev, data);

	err = max86900_regulator_onoff(data, HRM_LDO_OFF);
	if (err < 0) {
		pr_err("%s max86900_regulator_off fail(%d, %d)\n",
			__func__, err, HRM_LDO_OFF);
		goto dev_set_drvdata_failed;
	}
	data->skip_i2c_msleep = 0;
	pr_info("%s success\n", __func__);
	goto done;

dev_set_drvdata_failed:
max86900_init_device_failed:
err_setup_irq:
	if (data->part_type >= PART_TYPE_MAX86902A)
		sensors_unregister(data->dev, uv_sensor_attrs);
uv_sensor_register_failed:
err_uv_sysfs_create_group:
	sensors_remove_symlink(data->uv_input_dev);
err_uv_sensors_create_symlink:
	input_unregister_device(data->uv_input_dev);
err_uv_input_register_device:
	input_free_device(data->uv_input_dev);
err_uv_input_allocate_device:
	kfree(data->flicker_data);
err_flicker_alloc_data:
	misc_deregister(&data->miscdev);
err_flicker_miscdev:
	sensors_unregister(data->dev, hrm_sensor_attrs);
hrm_sensor_register_failed:
err_hrm_sysfs_create_group:
	sensors_remove_symlink(data->hrm_input_dev);
err_hrm_sensors_create_symlink:
	input_unregister_device(data->hrm_input_dev);
err_hrm_input_register_device:
	input_free_device(data->hrm_input_dev);
err_hrm_input_allocate_device:
err_of_read_chipid:
	max86900_regulator_onoff(data, HRM_LDO_OFF);
err_regulator_enable:
	gpio_free(data->hrm_int);
err_setup_gpio:
err_of_node:
	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);
	mutex_destroy(&data->flickerdatalock);
	kfree(data);
	pr_err("%s failed\n", __func__);
done:
	return err;
}

/*
 *  Remove function for this I2C driver.
 */
int max86900_remove(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
	free_irq(client->irq, NULL);
	return 0;
}

static void max86900_shutdown(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
}

static int max86900_pm_suspend(struct device *dev)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	max86900_pin_control(data, false);
	if (data->part_type < PART_TYPE_MAX86902A) {
		if (atomic_read(&data->hrm_is_enable)) {
			max86900_hrm_mode_enable(data, HRM_LDO_OFF);
			atomic_set(&data->is_suspend, 1);
		} else if (atomic_read(&data->uv_is_enable)) {
			max86900_uv_mode_enable(data, HRM_LDO_OFF);
			atomic_set(&data->is_suspend, 2);
		}
	} else {
		if (atomic_read(&data->hrm_is_enable)) {
			max86902_hrm_mode_enable(data, HRM_LDO_OFF);
			atomic_set(&data->is_suspend, 1);
		} else if (atomic_read(&data->uv_is_enable)) {
			max86902_uv_mode_enable(data, HRM_LDO_OFF);
			atomic_set(&data->is_suspend, 2);
		}
	}
	pr_info("%s\n", __func__);
	return 0;
}

static int max86900_pm_resume(struct device *dev)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	max86900_pin_control(data, true);
	if (data->part_type < PART_TYPE_MAX86902A) {
		if (atomic_read(&data->is_suspend) == 1) {
			max86900_hrm_mode_enable(data, HRM_LDO_ON);
			atomic_set(&data->is_suspend, 0);
		} else if (atomic_read(&data->is_suspend) == 2) {
			max86900_uv_mode_enable(data, HRM_LDO_ON);
			atomic_set(&data->is_suspend, 0);
		}
	} else {
		if (atomic_read(&data->is_suspend) == 1) {
			max86902_hrm_mode_enable(data, HRM_LDO_ON);
			atomic_set(&data->is_suspend, 0);
		} else if (atomic_read(&data->is_suspend) == 2) {
			max86902_uv_mode_enable(data, HRM_LDO_ON);
			atomic_set(&data->is_suspend, 0);
		}
	}
	pr_info("%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops max86900_pm_ops = {
	.suspend = max86900_pm_suspend,
	.resume = max86900_pm_resume
};

static struct of_device_id max86900_match_table[] = {
	{ .compatible = "max86900",},
	{},
};

static const struct i2c_device_id max86900_device_id[] = {
	{ "max86900_match_table", 0 },
	{ }
};

/* descriptor of the max86900 I2C driver */
static struct i2c_driver max86900_i2c_driver = {
	.driver = {
	    .name = CHIP_NAME,
	    .owner = THIS_MODULE,
	    .pm = &max86900_pm_ops,
	    .of_match_table = max86900_match_table,
	},
	.probe = max86900_probe,
	.shutdown = max86900_shutdown,
	.remove = max86900_remove,
	.id_table = max86900_device_id,
};

/* initialization and exit functions */
static int __init max86900_init(void)
{
	if (!lpcharge)
		return i2c_add_driver(&max86900_i2c_driver);
	else
		return 0;
}

static void __exit max86900_exit(void)
{
	i2c_del_driver(&max86900_i2c_driver);
}

module_init(max86900_init);
module_exit(max86900_exit);

MODULE_DESCRIPTION("max86902 Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
