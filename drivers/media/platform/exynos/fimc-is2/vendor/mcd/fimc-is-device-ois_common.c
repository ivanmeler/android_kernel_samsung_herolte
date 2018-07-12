/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <exynos-fimc-is-sensor.h>
#include "fimc-is-core.h"
#include "fimc-is-interface.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-ois.h"
#include "fimc-is-vender-specific.h"
#ifdef CONFIG_AF_HOST_CONTROL
#include "fimc-is-device-af.h"
#endif
#include <linux/pinctrl/pinctrl.h>
#if defined (CONFIG_OIS_USE_RUMBA_S4)
#include "fimc-is-device-ois_s4.h"
#elif defined (CONFIG_OIS_USE_RUMBA_SA)
#include "fimc-is-device-ois_sa.h"
#endif

#define FIMC_IS_OIS_DEV_NAME		"exynos-fimc-is-ois"
#define OIS_GYRO_SCALE_FACTOR_IDG	175
#define OIS_GYRO_SCALE_FACTOR_K2G	131
#define OIS_I2C_RETRY_COUNT	2

 bool not_crc_bin;

struct fimc_is_ois_info ois_minfo;
struct fimc_is_ois_info ois_pinfo;
struct fimc_is_ois_info ois_uinfo;
struct fimc_is_ois_exif ois_exif_data;

void fimc_is_ois_i2c_config(struct i2c_client *client, bool onoff)
{
	struct pinctrl *pinctrl_i2c = NULL;
	struct device *i2c_dev = NULL;
	struct fimc_is_device_ois *ois_device = NULL;

	if (client == NULL) {
		err("client is NULL.");
		return;
	}

	i2c_dev = client->dev.parent->parent;
	ois_device = i2c_get_clientdata(client);

	if (ois_device->ois_hsi2c_status != onoff) {
		info("%s : ois_hsi2c_stauts(%d),onoff(%d)\n",__func__,
			ois_device->ois_hsi2c_status, onoff);

		if (onoff) {
			pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
			if (IS_ERR_OR_NULL(pinctrl_i2c)) {
				printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
			} else {
				devm_pinctrl_put(pinctrl_i2c);
			}
		} else {
			pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "default");
			if (IS_ERR_OR_NULL(pinctrl_i2c)) {
				printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
			} else {
				devm_pinctrl_put(pinctrl_i2c);
			}
		}
		ois_device->ois_hsi2c_status = onoff;
	}

}

int fimc_is_ois_i2c_read(struct i2c_client *client, u16 addr, u8 *data)
{
	int err;
	u8 txbuf[2], rxbuf[1];
	struct i2c_msg msg[2];

	*data = 0;
	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail err = %d\n", __func__, err);
		return -EIO;
	}

	*data = rxbuf[0];
	return 0;
}

int fimc_is_ois_i2c_write(struct i2c_client *client ,u16 addr, u8 data)
{
        int retries = OIS_I2C_RETRY_COUNT;
        int ret = 0, err = 0;
        u8 buf[3] = {0,};
        struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = 3,
                .buf    = buf,
        };

        buf[0] = (addr & 0xff00) >> 8;
        buf[1] = addr & 0xff;
        buf[2] = data;

#if 0
        info("%s : W(0x%02X%02X %02X)\n",__func__, buf[0], buf[1], buf[2]);
#endif

        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < OIS_I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
                        err, addr, data, retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
        }

        return 0;
}

int fimc_is_ois_i2c_write_multi(struct i2c_client *client ,u16 addr, u8 *data, size_t size)
{
	int retries = OIS_I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	ulong i = 0;
	u8 buf[258] = {0,};
	struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = size,
                .buf    = buf,
	};

	buf[0] = (addr & 0xFF00) >> 8;
	buf[1] = addr & 0xFF;

	for (i = 0; i < size - 2; i++) {
	        buf[i + 2] = *(data + i);
	}
#if 0
        info("OISLOG %s : W(0x%02X%02X%02X)\n", __func__, buf[0], buf[1], buf[2]);
#endif
        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < OIS_I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
                        err, addr, *data, retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
	}

        return 0;
}

int fimc_is_ois_i2c_read_multi(struct i2c_client *client, u16 addr, u8 *data, size_t size)
{
	int err;
	u8 rxbuf[256], txbuf[2];
	struct i2c_msg msg[2];

	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail", __func__);
		return -EIO;
	}

	memcpy(data, rxbuf, size);
	return 0;
}

int fimc_is_ois_gpio_on(struct fimc_is_core *core)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
#ifdef CAMERA_USE_OIS_EXT_CLK
	u32 id = core->preproc.pdata->id;
#endif

	sensor_id = specific->rear_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

#ifdef CAMERA_USE_OIS_EXT_CLK
	ret = fimc_is_sensor_mclk_on(&core->sensor[id], SENSOR_SCENARIO_OIS_FACTORY, module->pdata->mclk_ch);
	if (ret) {
		err("fimc_is_sensor_mclk_on is fail(%d)", ret);
		goto p_err;
	}
#endif

	ret = module_pdata->gpio_cfg(module->dev, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ois_gpio_off(struct fimc_is_core *core)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;
#ifdef CAMERA_USE_OIS_EXT_CLK
	u32 id = core->preproc.pdata->id;
#endif

	sensor_id = specific->rear_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module->dev, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

#ifdef CAMERA_USE_OIS_EXT_CLK
	ret = fimc_is_sensor_mclk_off(&core->sensor[id], SENSOR_SCENARIO_OIS_FACTORY, module->pdata->mclk_ch);
	if (ret) {
		err("fimc_is_sensor_mclk_off is fail(%d)", ret);
		goto p_err;
	}
#endif

p_err:
	return ret;
}

void fimc_is_ois_enable(struct fimc_is_core *core)
{
	int ret = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	info("%s : E\n", __FUNCTION__);
	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x02, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x00, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}
	info("%s : X\n", __FUNCTION__);
}

int fimc_is_ois_sine_mode(struct fimc_is_core *core, int mode)
{
	int ret = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	info("%s : E\n", __FUNCTION__);
	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, true);
	}

	if (mode == OPTICAL_STABILIZATION_MODE_SINE_X) {
		ret = fimc_is_ois_i2c_write(core->client1, 0x18, 0x01);
	} else if (mode == OPTICAL_STABILIZATION_MODE_SINE_Y) {
		ret = fimc_is_ois_i2c_write(core->client1, 0x18, 0x02);
	}
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x19, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x1A, 0x1E);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x02, 0x03);
	if (ret) {
		err("i2c write fail\n");
	}

	msleep(20);

	ret = fimc_is_ois_i2c_write(core->client1, 0x00, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}
	info("%s : X\n", __FUNCTION__);

	return ret;
}
EXPORT_SYMBOL(fimc_is_ois_sine_mode);

void fimc_is_ois_version(struct fimc_is_core *core)
{
	int ret = 0;
	u8 val_c = 0, val_d = 0;
	u16 version = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00FC, &val_c);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00FD, &val_d);
	if (ret) {
		err("i2c write fail\n");
	}

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	version = (val_d << 8) | val_c;
	info("OIS version = 0x%04x\n", version);
}

void fimc_is_ois_offset_test(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y)
{
	int ret = 0, i = 0;
	u8 val = 0, x = 0, y = 0;
	int x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 20;
	int scale_factor = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	info("%s : E\n", __FUNCTION__);

	if (ois_minfo.header_ver[0] == '6') {
		scale_factor = OIS_GYRO_SCALE_FACTOR_IDG;
	} else {
		scale_factor = OIS_GYRO_SCALE_FACTOR_K2G;
	}

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x0014, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	retries = avg_count;
	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0014, &val);
		if (ret != 0) {
			break;
		}
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x0248, &val);
		x = val;
		fimc_is_ois_i2c_read(core->client1, 0x0249, &val);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x024A, &val);
		y = val;
		fimc_is_ois_i2c_read(core->client1, 0x024B, &val);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / scale_factor / 10;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	fimc_is_ois_version(core);
	info("%s : X\n", __FUNCTION__);
	return;
}

void fimc_is_ois_get_offset_data(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y)
{
	int i = 0;
	u8 val = 0, x = 0, y = 0;
	int x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 20;
	int scale_factor = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	info("%s : E\n", __FUNCTION__);

	if (ois_minfo.header_ver[0] == '6') {
		scale_factor = OIS_GYRO_SCALE_FACTOR_IDG;
	} else {
		scale_factor = OIS_GYRO_SCALE_FACTOR_K2G;
	}

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x0248, &val);
		x = val;
		fimc_is_ois_i2c_read(core->client1, 0x0249, &val);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x024A, &val);
		y = val;
		fimc_is_ois_i2c_read(core->client1, 0x024B, &val);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / scale_factor / 10;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	fimc_is_ois_version(core);
	info("%s : X\n", __FUNCTION__);
	return;
}

int fimc_is_ois_self_test(struct fimc_is_core *core)
{
	int ret = 0;

	ret = fimc_is_ois_self_test_impl(core);

	return ret;
}

bool fimc_is_ois_diff_test(struct fimc_is_core *core, int *x_diff, int *y_diff)
{
	bool result = false;

	result = fimc_is_ois_diff_test_impl(core, x_diff, y_diff);

	return result;
}

bool fimc_is_ois_auto_test(struct fimc_is_core *core,
		            int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y)

{
	bool result = false;

	result = fimc_is_ois_auto_test_impl(core,
		threshold, x_result, y_result, sin_x, sin_y);

	return result;
}

u16 fimc_is_ois_calc_checksum(u8 *data, int size)
{
	int i = 0;
	u16 result = 0;

	for(i = 0; i < size; i += 2) {
		result = result + (0xFFFF & (((*(data + i + 1)) << 8) | (*(data + i))));
	}

	return result;
}

void fimc_is_ois_gyro_sleep(struct fimc_is_core *core)
{
	int ret = 0;
	u8 val = 0;
	int retries = 20;
	
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x0000, 0x00);
	if (ret) {
		err("i2c read fail\n");
	}

	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0001, &val);
		if (ret != 0) {
			break;
		}

		if (val == 0x01)
			break;

		msleep(1);
	} while (--retries > 0);

	if (retries <= 0) {
		err("Read register failed!!!!, data = 0x%04x\n", val);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x0030, 0x03);
	if (ret) {
		err("i2c read fail\n");
	}
	msleep(1);

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return;
}

void fimc_is_ois_exif_data(struct fimc_is_core *core)
{
	int ret = 0;
	u8 error_reg[2], status_reg;
	u16 error_sum;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0004, &error_reg[0]);
	if (ret) {
		err("i2c read fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0005, &error_reg[1]);
	if (ret) {
		err("i2c read fail\n");
	}

	error_sum = (error_reg[1] << 8) | error_reg[0];

	ret = fimc_is_ois_i2c_read(core->client1, 0x0001, &status_reg);
	if (ret) {
		err("i2c read fail\n");
	}

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	ois_exif_data.error_data = error_sum;
	ois_exif_data.status_data = status_reg;

	return;
}

int fimc_is_ois_get_exif_data(struct fimc_is_ois_exif **exif_info)
{
	*exif_info = &ois_exif_data;
	return 0;
}

int fimc_is_ois_get_module_version(struct fimc_is_ois_info **minfo)
{
	*minfo = &ois_minfo;
	return 0;
}

int fimc_is_ois_get_phone_version(struct fimc_is_ois_info **pinfo)
{
	*pinfo = &ois_pinfo;
	return 0;
}

int fimc_is_ois_get_user_version(struct fimc_is_ois_info **uinfo)
{
	*uinfo = &ois_uinfo;
	return 0;
}

bool fimc_is_ois_version_compare(char *fw_ver1, char *fw_ver2, char *fw_ver3)
{
	if (fw_ver1[FW_GYRO_SENSOR] != fw_ver2[FW_GYRO_SENSOR]
		|| fw_ver1[FW_DRIVER_IC] != fw_ver2[FW_DRIVER_IC]
		|| fw_ver1[FW_CORE_VERSION] != fw_ver2[FW_CORE_VERSION]) {
		return false;
	}

	if (fw_ver2[FW_GYRO_SENSOR] != fw_ver3[FW_GYRO_SENSOR]
		|| fw_ver2[FW_DRIVER_IC] != fw_ver3[FW_DRIVER_IC]
		|| fw_ver2[FW_CORE_VERSION] != fw_ver3[FW_CORE_VERSION]) {
		return false;
	}

	return true;
}

bool fimc_is_ois_version_compare_default(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_GYRO_SENSOR] != fw_ver2[FW_GYRO_SENSOR]
		|| fw_ver1[FW_DRIVER_IC] != fw_ver2[FW_DRIVER_IC]
		|| fw_ver1[FW_CORE_VERSION] != fw_ver2[FW_CORE_VERSION]) {
		return false;
	}

	return true;
}

u8 fimc_is_ois_read_status(struct fimc_is_core *core)
{
	int ret = 0;
	u8 status = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0006, &status);
	if (ret) {
		err("i2c read fail\n");
	}

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return status;
}

u8 fimc_is_ois_read_cal_checksum(struct fimc_is_core *core)
{
	int ret = 0;
	u8 status = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0005, &status);
	if (ret) {
		err("i2c read fail\n");
	}

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return status;
}

void fimc_is_ois_fw_status(struct fimc_is_core *core)
{
	ois_minfo.checksum = fimc_is_ois_read_status(core);
	ois_minfo.caldata = fimc_is_ois_read_cal_checksum(core);

	return;
}

bool fimc_is_ois_crc_check(struct fimc_is_core *core, char *buf)
{
	u8 check_8[4] = {0, };
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_bin;

	if (not_crc_bin) {
		err("ois binary does not conatin crc checksum.\n");
		return false;
	}

	if (buf == NULL) {
		err("buf is NULL. CRC check failed.");
		return false;
	}

	buf32 = (u32 *)buf;

	memcpy(check_8, buf + OIS_BIN_LEN, 4);
	checksum_bin = (check_8[3] << 24) | (check_8[2] << 16) | (check_8[1] << 8) | (check_8[0]);

	checksum = (u32)getCRC((u16 *)&buf32[0], OIS_BIN_LEN, NULL, NULL);
	if (checksum != checksum_bin) {
		return false;
	} else {
		return true;
	}
}

bool fimc_is_ois_check_fw(struct fimc_is_core *core)
{
	bool ret = false;

	ret = fimc_is_ois_check_fw_impl(core);

	return ret;
}

void fimc_is_ois_fw_update(struct fimc_is_core *core)
{
	fimc_is_ois_gpio_on(core);
	msleep(150);
	fimc_is_ois_fw_update_impl(core);
	fimc_is_ois_gpio_off(core);

	return;
}

int fimc_is_ois_parse_dt(struct i2c_client *client)
{
	int ret = 0;

	return ret;
}

static int fimc_is_ois_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_device_ois *device;
	struct fimc_is_core *core;
	struct device *i2c_dev;
	struct pinctrl *pinctrl_i2c = NULL;
	int ret = 0;
	struct fimc_is_vender_specific *specific;

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed");
		client->dev.init_name = FIMC_IS_OIS_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		return -EINVAL;

	specific = core->vender.private_data;
	if (!specific)
		return -EINVAL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err("No I2C functionality found\n");
		return -ENODEV;
	}

	device = kzalloc(sizeof(struct fimc_is_device_ois), GFP_KERNEL);
	if (!device) {
		err("fimc_is_device_companion is NULL");
		return -ENOMEM;
	}

	device->ois_hsi2c_status = false;
	core->client1 = client;
	i2c_set_clientdata(client, device);
	specific->ois_ver_read = false;
	not_crc_bin = false;

	if (client->dev.of_node) {
		ret = fimc_is_ois_parse_dt(client);
		if (ret) {
			err("parsing device tree is fail(%d)", ret);
			return -ENODEV;
		}
	}

	/* Initial i2c pin */
	i2c_dev = client->dev.parent->parent;
	pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "default");
	if (IS_ERR_OR_NULL(pinctrl_i2c)) {
		printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
	} else {
		devm_pinctrl_put(pinctrl_i2c);
	}

	return 0;
}

static int fimc_is_ois_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ois_id[] = {
	{FIMC_IS_OIS_DEV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ois_id);

#ifdef CONFIG_OF
static struct of_device_id ois_dt_ids[] = {
	{ .compatible = "rumba,ois",},
	{},
};
#endif

static struct i2c_driver ois_i2c_driver = {
	.driver = {
		   .name = FIMC_IS_OIS_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ois_dt_ids,
#endif
	},
	.probe = fimc_is_ois_probe,
	.remove = fimc_is_ois_remove,
	.id_table = ois_id,
};
module_i2c_driver(ois_i2c_driver);

MODULE_DESCRIPTION("OIS driver for Rumba");
MODULE_AUTHOR("kyoungho yun <kyoungho.yun@samsung.com>");
MODULE_LICENSE("GPL v2");
