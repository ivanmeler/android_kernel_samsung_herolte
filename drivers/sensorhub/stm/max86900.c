/* 
 * Copyright (c)2013 Maxim Integrated Products, Inc.
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

#include "max86900.h"
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#define MAX86900_I2C_RETRY_DELAY	10
#define MAX86900_I2C_MAX_RETRIES	5
#define MAX86900_COUNT_MAX		65532
#define MAX86900_WHOAMI_REG		0xFE
#define MAX86900_WHOAMI			0x01
#define MAX86900A_WHOAMI		0x11
#define MAX86900A_REV_ID		0x00
#define MAX86900B_REV_ID		0x04
#define MAX86900C_REV_ID		0x05

#define MAX86900_DEFAULT_CURRENT	0x55
#define MAX86900A_DEFAULT_CURRENT	0xFF
#define MAX86900C_DEFAULT_CURRENT	0x0F

#define MODULE_NAME_HRM			"hrm_sensor"
#define CHIP_NAME			"MAX86900"
#define VENDOR				"MAXIM"
#define HRM_LDO_ON	1
#define HRM_LDO_OFF	0
#define MAX_EOL_RESULT 132
#define MAX_LIB_VER 20

#define SAMPLE_RATE 4

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
	int ret;

	data->vdd_1p8 = regulator_get(NULL, data->sub_ldo4);
	if (IS_ERR(data->vdd_1p8)) {
		pr_err("%s - regulator_get fail\n", __func__);
		goto err_1p8;
	}

#if defined(CONFIG_SEC_KACTIVE_PROJECT) || defined(CONFIG_MACH_KSPORTSLTE_SPR)
	data->vdd_3p3 = regulator_get(NULL, data->led_l19);
	if (IS_ERR(data->vdd_3p3)) {
		pr_err("%s - regulator_get fail\n", __func__);
		goto err_3p3;
	}
#endif
	pr_info("%s - onoff = %d\n", __func__, onoff);

	if (onoff == HRM_LDO_ON) {
		regulator_set_voltage(data->vdd_1p8, 1800000, 1800000);
		ret = regulator_enable(data->vdd_1p8);
#if defined(CONFIG_SEC_KACTIVE_PROJECT) || defined(CONFIG_MACH_KSPORTSLTE_SPR)
		regulator_set_voltage(data->vdd_3p3, 3300000, 3300000);
		ret = regulator_enable(data->vdd_3p3);
#endif
	} else {
		regulator_disable(data->vdd_1p8);
#if defined(CONFIG_SEC_KACTIVE_PROJECT) || defined(CONFIG_MACH_KSPORTSLTE_SPR)
		regulator_disable(data->vdd_3p3);
#endif
	}

	regulator_put(data->vdd_1p8);
#if defined(CONFIG_SEC_KACTIVE_PROJECT) || defined(CONFIG_MACH_KSPORTSLTE_SPR)
	regulator_put(data->vdd_3p3);
#endif
	return 0;
#if defined(CONFIG_SEC_KACTIVE_PROJECT) || defined(CONFIG_MACH_KSPORTSLTE_SPR)
err_3p3:
	regulator_put(data->vdd_1p8);
#endif
err_1p8:
	return -ENODEV;
}

static int max86900_init_device(struct max86900_device_data *data)
{
	int err;
	u8 recvData;

	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		return -EIO;
	}

	/* Interrupt Clear */
	recvData = MAX86900_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(data, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x83);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_INTERRUPT_ENABLE, 0x10);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_INTERRUPT_ENABLE!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_SPO2_CONFIGURATION, 0x51); //400hz 400us
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	/* Turn off ALC in case of Max86900A */
	if (data->part_type == PART_TYPE_MAX86900A) {
		/* ALC OFF start */
		err = max86900_write_reg(data, MAX86900_TEST_MODE, 0x54);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_TEST_MODE!\n",
				__func__);
			return -EIO;
		}

		err = max86900_write_reg(data, MAX86900_TEST_MODE, 0x4D);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_TEST_MODE!\n",
				__func__);
			return -EIO;
		}

		err = max86900_write_reg(data, MAX86900_TEST_GTST, 0x02);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_TEST_GTST!\n",
				__func__);
			return -EIO;
		}

		err = max86900_write_reg(data, MAX86900_TEST_ALC, 0x01);
		if (err != 0) {
			pr_err("%s - error initializing MAX86900_TEST_ALC!\n",
				__func__);
			return -EIO;
		}
		err = max86900_write_reg(data, MAX86900_TEST_MODE, 0x00);
		if (err != 0) {
			pr_err("%s - error Exiting MAX86900_TEST_MODE!\n",
				__func__);
			return -EIO;
		}
		/* ALC OFF end */
	}


	pr_info("%s done, part_type = %u\n", __func__, data->part_type);

	return 0;
}

static int max86900_enable(struct max86900_device_data *data)
{
	int err;
	data->led = 0;
	data->sample_cnt = 0;
	data->ir_sum = 0;
	data->r_sum = 0;

	mutex_lock(&data->activelock);
	err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION, data->default_current);
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
		printk("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}
	enable_irq(data->irq);
	mutex_unlock(&data->activelock);
	return 0;
}

static int max86900_disable(struct max86900_device_data *data)
{
	u8 err;

	mutex_lock(&data->activelock);
	disable_irq(data->irq);

	err = max86900_write_reg(data, MAX86900_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
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
	mutex_unlock(&data->activelock);

	return 0;
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

	pr_info("%s - %d(%x, %x)\n", __func__, data->hrm_temp, recvData[0], recvData[1]);

	return 0;
}

static int max86900_eol_test_control(struct max86900_device_data *data)
{
	int err = 0;
	u8 led_current = 0;

	if (data->sample_cnt < data->hr_range2)	{
		data->hr_range = 1;
	} else if (data->sample_cnt < (data->hr_range2 + 297)) {
		/* Fake pulse */
		if (data->sample_cnt % 8 < 4) {
			data->test_current_ir++;
			data->test_current_red++;
		} else {
			data->test_current_ir--;
			data->test_current_red--;
		}

		led_current = (data->test_current_red << 4) | data->test_current_ir;
		err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION,
				led_current);
		if (err != 0) {
			pr_err("%s - error initializing\
					MAX86900_LED_CONFIGURATION!\n", __func__);
			return -EIO;
		}
		data->hr_range = 2;
	} else if (data->sample_cnt == (data->hr_range2 + 297)) {
		/* Measure */
		err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION, data->led_current);
		if (err != 0) {
			pr_err("%s - error initializing\
					MAX86900_LED_CONFIGURATION!\n", __func__);
			return -EIO;
		}
		/* 400Hz setting */
		err = max86900_write_reg(data, MAX86900_SPO2_CONFIGURATION, 0x51);
		if (err != 0) {
			pr_err("%s - error initializing\
					MAX86900_SPO2_CONFIGURATION!\n", __func__);
			return -EIO;
		}
	} else if (data->sample_cnt < ((data->hr_range2 + 297) + 400 * 10)) {
		data->hr_range = 3;
	} else if (data->sample_cnt == ((data->hr_range2 + 297) + 400 * 10)) {
		err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION,
				data->default_current);
		if (err != 0) {
			pr_err("%s - error initializing\
					MAX86900_LED_CONFIGURATION!\n", __func__);
			return -EIO;
		}
		/* 100Hz setting */
		err = max86900_write_reg(data, MAX86900_SPO2_CONFIGURATION, 0x47);
		if (err != 0) {
			pr_err("%s - error initializing\
					MAX86900_SPO2_CONFIGURATION!\n", __func__);
			return -EIO;
		}
	}
	data->sample_cnt++;
	return 0;
}

static int max86900_read_data(struct max86900_device_data *device, u16 *data)
{
	u8 err;
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
		data[i] = ((((u16)recvData[i * 2]) << 8 ) & 0xff00)
			| ((((u16)recvData[i * 2 + 1])) & 0x00ff);
	}

	if ((device->sample_cnt % 4000) == 1)
		pr_info("%s - %u, %u\n", __func__, data[0], data[1]);

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
		if ((device->sample_cnt % SAMPLE_RATE) == SAMPLE_RATE - 1) {
			data[0] = device->ir_sum / SAMPLE_RATE;
			data[1] = device->r_sum / SAMPLE_RATE;
			device->ir_sum = 0;
			device->r_sum = 0;
			ret = 0;
		} else
			ret = 1;

		if (device->sample_cnt++ > 100 && device->led == 0) {
			device->led = 1;
		}
	}

	return ret;
}

void max86900_mode_enable(struct max86900_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		if (data->sub_ldo4 != NULL) {
			err = max86900_regulator_onoff(data, HRM_LDO_ON);
			if (err < 0)
				pr_err("%s max86900_regulator_on fail err = %d\n",
					__func__, err);
			usleep_range(1000, 1100);
			err = max86900_init_device(data);
			if (err)
				pr_err("%s max86900_init device fail err = %d\n",
					__func__, err);
		}
		err = max86900_enable(data);
		if (err != 0)
			pr_err("max86900_enable err : %d\n", err);

		atomic_set(&data->is_enable, 1);
	} else {
		err = max86900_disable(data);
		if (err != 0)
			pr_err("max86900_disable err : %d\n", err);
		if (data->sub_ldo4 != NULL) {
			err = max86900_regulator_onoff(data, HRM_LDO_OFF);
			if (err < 0)
				pr_err("%s max86900_regulator_off fail err = %d\n",
					__func__, err);
		}

		atomic_set(&data->is_enable, 0);
	}
	pr_info("%s - part_type = %u, onoff = %d\n", __func__, data->part_type, onoff);
}

/* sysfs */
static ssize_t max86900_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct max86900_device_data *data = dev_get_drvdata(dev);

    return sprintf(buf, "%d\n", atomic_read(&data->is_enable));
}

static ssize_t max86900_enable_store(struct device *dev,
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

	max86900_mode_enable(data, new_value);
	return count;
}

static ssize_t max86900_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", 10000000LL);
}

static ssize_t max86900_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_info("%s - max86900 hrm sensor delay was fixed as 10ms\n", __func__);
	return size;
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
        max86900_enable_show, max86900_enable_store);
static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
        max86900_poll_delay_show, max86900_poll_delay_store);

static struct attribute *hrm_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group hrm_attribute_group = {
	.attrs = hrm_sysfs_attrs,
};

/* test sysfs */
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

static int max86900_eol_test_enable(struct max86900_device_data *data)
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
	err = max86900_write_reg(data, MAX86900_LED_CONFIGURATION, led_current);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -EIO;
	}

	err = max86900_write_reg(data, MAX86900_SPO2_CONFIGURATION, 0x47); //100Hz 1600us
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
			__func__);
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

static void max86900_eol_test_onoff(struct max86900_device_data *data, int onoff)
{
	int err;

	if (onoff) {
		err = max86900_eol_test_enable(data);
		data->eol_test_is_enable = 1;
		if (err != 0)
			pr_err("max86900_eol_test_enable err : %d\n", err);
	} else {
		pr_info("%s - eol test off\n", __func__);
		err = max86900_disable(data);
		if (err != 0)
			pr_err("max86900_disable err : %d\n", err);

		data->hr_range = 0;
		data->led_current = data->default_current;

		err = max86900_init_device(data);
		if (err)
			pr_err("%s max86900_init device fail err = %d", __func__, err);

		err = max86900_enable(data);
		if (err != 0)
			pr_err("max86900_enable err : %d\n", err);

		data->eol_test_is_enable = 0;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

static ssize_t max86900_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t max86900_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t led_current_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current);
	if (err < 0)
		return err;

	err = max86900_set_led_current(data);
	if (err < 0)
		return err;

	data->default_current = data->led_current;

	return size;
}

static ssize_t led_current_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	pr_info("max86900_%s - led_current = %u\n", __func__, data->led_current);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current);
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

	pr_info("max86900_%s - look_mode_ir = %x\n", __func__, data->look_mode_ir);

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

	pr_info("max86900_%s - look_mode_red = %x\n", __func__, data->look_mode_red);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->look_mode_red);
}

static ssize_t eol_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int test_onoff;
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* eol_test start */
		test_onoff = 1;
	else if (sysfs_streq(buf, "0")) /* eol_test stop */
		test_onoff = 0;
	else {
		pr_debug("max86900_%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	max86900_eol_test_onoff(data, test_onoff);

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
	buf_len = strlen(buf) + 1;
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
	
	pr_info("max86900_%s - result = %s, buf_len(%u)\n", __func__, data->eol_test_result, buf_len);
	data->eol_test_status = 1;
	return size;
}

static ssize_t eol_test_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);

	if (data->eol_test_result == NULL) {
		pr_info("max86900_%s - data->eol_test_result is NULL\n", __func__);
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
		pr_err("max86900_%s - INT pin state is high before INT clear\n", __func__);
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
		pr_err("max86900_%s - INT pin state is low after INT clear\n", __func__);
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
	buf_len = strlen(buf) + 1;
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

static ssize_t max86900_hrm_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	u8 handle = 0;

	if (sysfs_streq(buf, "17")) /* ID_SAM_HRM */
		handle = 17;
	else if (sysfs_streq(buf, "18")) /* ID_AOSP_HRM */
		handle = 18;
	else {
		pr_info("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	input_report_rel(data->hrm_input_dev, REL_MISC, handle);
	return size;
}

static DEVICE_ATTR(name, S_IRUGO, max86900_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, max86900_vendor_show, NULL);
static DEVICE_ATTR(led_current, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current_show, led_current_store);
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
static DEVICE_ATTR(hrm_flush, S_IWUSR | S_IWGRP,
	NULL, max86900_hrm_flush_store);

static struct device_attribute *hrm_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_led_current,
	&dev_attr_hr_range,
	&dev_attr_hr_range2,
	&dev_attr_look_mode_ir,
	&dev_attr_look_mode_red,
	&dev_attr_eol_test,
	&dev_attr_eol_test_result,
	&dev_attr_eol_test_status,
	&dev_attr_int_pin_check,
	&dev_attr_lib_ver,
	&dev_attr_hrm_flush,
	NULL,
};

irqreturn_t max86900_irq_handler(int irq, void *device)
{
	int err;
	struct max86900_device_data *data = device;
	u16 raw_data[2] = {0x00, };

	err = max86900_read_data(data, raw_data);
	if (err < 0)
		pr_err("max86900_read_data err : %d\n", err);

	if (err == 0) {
		input_report_rel(data->hrm_input_dev, REL_X, raw_data[0] + 1); /* IR */
		input_report_rel(data->hrm_input_dev, REL_Y, raw_data[1] + 1); /* RED */
		input_report_rel(data->hrm_input_dev, REL_Z, data->hrm_temp + 1);
		input_sync(data->hrm_input_dev);
	}

	return IRQ_HANDLED;
}


#if defined(CONFIG_MACH_KLTE_KDI)
extern unsigned int system_rev;
#endif


static int max86900_parse_dt(struct max86900_device_data *data,
	struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;
#if defined(CONFIG_MACH_KLTE_KDI)
	if(system_rev==11)
		data->hrm_int = of_get_named_gpio_flags(dNode,
		"max86900,hrm_int-kdi-gpio", 0, &flags);
	else
		data->hrm_int = of_get_named_gpio_flags(dNode,
		"max86900,hrm_int-gpio", 0, &flags);
#else
	data->hrm_int = of_get_named_gpio_flags(dNode,
		"max86900,hrm_int-gpio", 0, &flags);
#endif
	if (data->hrm_int < 0) {
		pr_err("%s - get hrm_int error\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(dNode, "max86900,sub_ldo4", &data->sub_ldo4) < 0)
		pr_err("%s - get sub_ldo4 error\n", __func__);
#if defined(CONFIG_SEC_KACTIVE_PROJECT) || defined(CONFIG_MACH_KSPORTSLTE_SPR)
	if (of_property_read_string(dNode, "max86900,led_l19", &data->led_l19) < 0)
		pr_err("%s - get led_l19 error\n", __func__);
#endif
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

	errorno = request_threaded_irq(data->irq, NULL,
		max86900_irq_handler, IRQF_TRIGGER_FALLING,
		"hrm_sensor_irq", data);

	if (errorno < 0) {
		pr_err("%s - request_irq(%d) failed for gpio %d (%d)\n",
		       __func__, data->irq, data->hrm_int, errorno);
		errorno = -ENODEV;
		goto err_request_irq;
	}

	disable_irq(data->irq);
	goto done;
err_request_irq:
err_gpio_direction_input:
	gpio_free(data->hrm_int);
done:
	return errorno;
}

int max86900_probe(struct i2c_client *client, const struct i2c_device_id *id )
{
	int err = -ENODEV;
	u8 buffer[2] = {0, };

	struct max86900_device_data *data;

	/* check to make sure that the adapter supports I2C */
	if (!i2c_check_functionality(client->adapter,I2C_FUNC_I2C)) {
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
	i2c_set_clientdata(client, data);

	mutex_init(&data->i2clock);
	mutex_init(&data->activelock);
	mutex_init(&data->storelock);

	data->dev = &client->dev;

	pr_info("%s - start \n", __func__);

	data->hr_range = 0;

	err = max86900_parse_dt(data, &client->dev);
	if (err < 0) {
		pr_err("[SENSOR] %s - of_node error\n", __func__);
		err = -ENODEV;
		goto err_of_node;
	}

	if (data->sub_ldo4 != NULL) {
		err = max86900_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0) {
			pr_err("%s max86900_regulator_on fail err = %d\n",
				__func__, err);
			goto err_of_node;
		}
	}
	usleep_range(1000, 1100);

	data->client->addr = MAX86900A_SLAVE_ADDR;

	buffer[0] = MAX86900_WHOAMI_REG;
	err = max86900_read_reg(data, buffer, 2);

	if (buffer[1] == MAX86900A_WHOAMI) {
		/* MAX86900A & MAX86900B */
		switch(buffer[0]) {
			case MAX86900A_REV_ID:
				data->part_type = PART_TYPE_MAX86900A;
				data->default_current = MAX86900A_DEFAULT_CURRENT;
				break;
			case MAX86900B_REV_ID:
				data->part_type = PART_TYPE_MAX86900B;
				data->default_current = MAX86900A_DEFAULT_CURRENT;
				break;
			case MAX86900C_REV_ID:
				data->part_type = PART_TYPE_MAX86900C;
				data->default_current = MAX86900C_DEFAULT_CURRENT;
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

	data->led_current = data->default_current;

	/* allocate input device */
	data->hrm_input_dev = input_allocate_device();
	if (!data->hrm_input_dev) {
		pr_err("%s - could not allocate input device\n",
			__func__);
		goto err_input_allocate_device;
	}

	input_set_drvdata(data->hrm_input_dev, data);
	data->hrm_input_dev->name = MODULE_NAME_HRM;
	input_set_capability(data->hrm_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Z);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_MISC);

	err = input_register_device(data->hrm_input_dev);
	if (err < 0) {
		input_free_device(data->hrm_input_dev);
		pr_err("%s - could not register input device\n", __func__);
		goto err_input_register_device;
	}

	err = sensors_create_symlink(&data->hrm_input_dev->dev.kobj,
					data->hrm_input_dev->name);
	if (err < 0) {
		pr_err("%s - create_symlink error\n", __func__);
		goto err_sensors_create_symlink;
	}

	err = sysfs_create_group(&data->hrm_input_dev->dev.kobj,
				 &hrm_attribute_group);
	if (err) {
		pr_err("[SENSOR] %s - could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group;
	}

	err = max86900_gpio_setup(data);
	if (err) {
		pr_err("[SENSOR] %s - could not setup gpio\n", __func__);
		goto err_setup_gpio;
	}

	/* set sysfs for hrm sensor */
	err = sensors_register(data->dev, data, hrm_sensor_attrs,
			"hrm_sensor");
	if (err) {
		pr_err("[SENSOR] %s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	err = max86900_init_device(data);
	if (err) {
		pr_err("%s max86900_init device fail err = %d", __func__, err);
		goto max86900_init_device_failed;
	}

	err = dev_set_drvdata(data->dev, data);
	if (err) {
		pr_err("%s dev_set_drvdata fail err = %d", __func__, err);
		goto dev_set_drvdata_failed;
	}

	if (data->sub_ldo4 != NULL) {
		err = max86900_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0) {
			pr_err("%s max86900_regulator_off fail(%d, %d)\n",
				__func__, err, HRM_LDO_OFF);
			goto dev_set_drvdata_failed;
		}
	}
	pr_info("%s success\n", __func__);
	goto done;

dev_set_drvdata_failed:
max86900_init_device_failed:
	sensors_unregister(data->dev, hrm_sensor_attrs);
hrm_sensor_register_failed:
	gpio_free(data->hrm_int);
err_setup_gpio:
err_sysfs_create_group:
	sensors_remove_symlink(&data->hrm_input_dev->dev.kobj,
			data->hrm_input_dev->name);
err_sensors_create_symlink:
	input_unregister_device(data->hrm_input_dev);
err_input_register_device:
err_input_allocate_device:
err_of_read_chipid:
	max86900_regulator_onoff(data, HRM_LDO_OFF);
err_of_node:
	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);
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
	if (atomic_read(&data->is_enable)) {
		max86900_mode_enable(data, HRM_LDO_OFF);
		atomic_set(&data->is_suspend, 1);
	}
	pr_info("%s\n", __func__);
	return 0;
}

static int max86900_pm_resume(struct device *dev)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	if (atomic_read(&data->is_suspend)) {
		max86900_mode_enable(data, HRM_LDO_ON);
		atomic_set(&data->is_suspend, 0);
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
static struct i2c_driver max86900_i2c_driver =
{
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
	return i2c_add_driver(&max86900_i2c_driver);
}

static void __exit max86900_exit(void)
{
	i2c_del_driver(&max86900_i2c_driver);
}

module_init(max86900_init);
module_exit(max86900_exit);

MODULE_DESCRIPTION("max86900 Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
