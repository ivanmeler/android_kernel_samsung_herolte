/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <exynos-fimc-is-sensor.h>

#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"

#include "fimc-is-helper-i2c.h"

#include "fimc-is-cis.h"

u32 sensor_cis_do_div64(u64 num, u32 den) {
	u64 res = 0;

	if (den != 0) {
		res = num;
		do_div(res, den);
	} else {
		err("Divide by zero!!!\n");
		WARN_ON(1);
	}

	return (u32)res;
}

int sensor_cis_set_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	BUG_ON(!subdev);
	BUG_ON(!regs);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	msleep(3);

	for (i = 0; i < size; i += I2C_WRITE) {
		if (regs[i + I2C_BYTE] == 0x1) {
			ret = fimc_is_sensor_write8(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			if (ret < 0) {
				err("fimc_is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			}
		} else if (regs[i + I2C_BYTE] == 0x2) {
			ret = fimc_is_sensor_write16(client, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			if (ret < 0) {
				err("fimc_is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)",
						ret, regs[i + I2C_ADDR], regs[i + I2C_DATA]);
			}
		}
	}

#if (CIS_TEST_PATTERN_MODE != 0)
	ret = fimc_is_sensor_write8(client, 0x0601, CIS_TEST_PATTERN_MODE);
#endif

	dbg_sensor("[%s] sensor setting done\n", __func__);

p_err:
	return ret;
}

int sensor_cis_check_rev(struct fimc_is_cis *cis)
{
	int ret = 0;
	u8 rev = 0;
	struct i2c_client *client;

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_sensor_read8(client, 0x0002, &rev);

	cis->cis_data->cis_rev = rev;

	dbg_sensor("rev: %#x\n", rev);

p_err:
	return ret;
}

u32 sensor_cis_calc_again_code(u32 permile)
{
	return (permile * 32 + 500) / 1000;
}

u32 sensor_cis_calc_again_permile(u32 code)
{
	return (code * 1000 + 16) / 32;
}

u32 sensor_cis_calc_dgain_code(u32 permile)
{
	u8 buf[2] = {0, 0};
	buf[0] = permile / 1000;
	buf[1] = (((permile - (buf[0] * 1000)) * 256) / 1000);

	return (buf[0] << 8 | buf[1]);
}

u32 sensor_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0xFF00) >> 8) * 1000) + ((code & 0xFF) * 1000 / 256);
}

int sensor_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u16 coarse_int = 0;
	u32 compensated_again = 0;

	BUG_ON(!subdev);
	BUG_ON(!again);
	BUG_ON(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	if (line_length_pck <= 0) {
		err("[%s] invalid line_length_pck(%d)\n", __func__, line_length_pck);
		goto p_err;
	}

	coarse_int = ((expo * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;
	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor("[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	if (coarse_int <= 15) {
		compensated_again = (*again * ((expo * vt_pic_clk_freq_mhz) - min_fine_int)) / (line_length_pck * coarse_int);

		if (compensated_again < cis_data->min_analog_gain[1]) {
			*again = cis_data->min_analog_gain[1];
		} else if (*again >= cis_data->max_analog_gain[1]) {
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_mhz) - min_fine_int)) / (line_length_pck * coarse_int);
		} else {
			*again = compensated_again;
		}

		dbg_sensor("[%s] exp(%d), again(%d), dgain(%d), coarse_int(%d), compensated_again(%d)\n", __func__, expo, *again, *dgain, coarse_int, compensated_again);
	}

p_err:
	return ret;
}

int sensor_cis_dump_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	u8 data8 = 0;

	BUG_ON(!subdev);
	BUG_ON(!regs);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < size; i += I2C_WRITE) {
		ret = fimc_is_sensor_read8(client, regs[i + I2C_ADDR], &data8);
		if (ret < 0) {
			err("fimc_is_sensor_write8 fail, ret(%d), addr(%#x)",
					ret, regs[i + I2C_ADDR]);
		}
		pr_err("[SEN:DUMP] [%#x] : %x\n", regs[i + I2C_ADDR], data8);
	}

p_err:
	return ret;
}
