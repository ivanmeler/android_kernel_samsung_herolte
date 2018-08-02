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
#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
#include <linux/kthread.h>
#endif

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
#include "fimc-is-device-ois_s4.h"

static u8 progCode[OIS_BOOT_FW_SIZE + OIS_PROG_FW_SIZE] = {0,};
static bool fw_sdcard = false;
extern bool not_crc_bin;
extern struct fimc_is_ois_info ois_minfo;
extern struct fimc_is_ois_info ois_pinfo;
extern struct fimc_is_ois_info ois_uinfo;
extern struct fimc_is_ois_exif ois_exif_data;

int fimc_is_ois_self_test_impl(struct fimc_is_core *core)
{
	int ret = 0;
	u8 val = 0;
	u8 reg_val = 0, x = 0, y = 0;
	u16 x_gyro_log = 0, y_gyro_log = 0;
	int retries = 20;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	info("%s : E\n", __FUNCTION__);
	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x0014, 0x08);
	if (ret) {
		err("i2c write fail\n");
	}

	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0014, &val);
		if (ret != 0) {
			val = -EIO;
			break;
		}
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	ret = fimc_is_ois_i2c_read(core->client1, 0x0004, &val);
	if (ret != 0) {
		val = -EIO;
	}

	/* Gyro selfTest result */
	fimc_is_ois_i2c_read(core->client1, 0x00EC, &reg_val);
	x = reg_val;
	fimc_is_ois_i2c_read(core->client1, 0x00ED, &reg_val);
	x_gyro_log = (reg_val << 8) | x;

	fimc_is_ois_i2c_read(core->client1, 0x00EE, &reg_val);
	y = reg_val;
	fimc_is_ois_i2c_read(core->client1, 0x00EF, &reg_val);
	y_gyro_log = (reg_val << 8) | y;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	info("%s(GSTLOG0=%d, GSTLOG1=%d)\n", __FUNCTION__, x_gyro_log, y_gyro_log);

	info("%s(%d) : X\n", __FUNCTION__, val);
	return (int)val;
}

bool fimc_is_ois_diff_test_impl(struct fimc_is_core *core, int *x_diff, int *y_diff)
{
	int ret = 0;
	u8 val = 0, x = 0, y = 0;
	u16 x_min = 0, y_min = 0, x_max = 0, y_max = 0;
	int retries = 20, default_diff = 1100;
	u8 read_x[2], read_y[2];
	struct fimc_is_vender_specific *specific = core->vender.private_data;

#ifdef CONFIG_AF_HOST_CONTROL
	fimc_is_af_move_lens(core);
	msleep(30);
#endif

	info("(%s) : E\n", __FUNCTION__);
	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read_multi(core->client1, 0x021A, read_x, 2);
	ret |= fimc_is_ois_i2c_read_multi(core->client1, 0x021C, read_y, 2);
	if (ret) {
		err("i2c read fail\n");
	}

	ret = fimc_is_ois_i2c_write_multi(core->client1, 0x0022, read_x, 4);
	ret |= fimc_is_ois_i2c_write_multi(core->client1, 0x0024, read_y, 4);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0002, 0x02);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0000, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}
	msleep(400);
	info("(%s) : OIS Position = Center\n", __FUNCTION__);

	ret = fimc_is_ois_i2c_write(core->client1, 0x0000, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0001, &val);
		if (ret != 0) {
			break;
		}
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val != 1);

	ret = fimc_is_ois_i2c_write(core->client1, 0x0230, 0x64);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0231, 0x00);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0232, 0x64);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0233, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x0230, &val);
	info("OIS[read_val_0x0230::0x%04x]\n", val);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0231, &val);
	info("OIS[read_val_0x0231::0x%04x]\n", val);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0232, &val);
	info("OIS[read_val_0x0232::0x%04x]\n", val);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0233, &val);
	info("OIS[read_val_0x0233::0x%04x]\n", val);
	if (ret) {
		err("i2c read fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x020E, 0x6E);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x020F, 0x6E);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0210, 0x1E);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0211, 0x1E);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x0013, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

#if 0
	ret = fimc_is_ois_i2c_write(core->client1, 0x0012, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	retries = 30;
	do { //polarity check
		ret = fimc_is_ois_i2c_read(core->client1, 0x0012, &val);
		if (ret != 0) {
			break;
		}
		msleep(100);
		if (--retries < 0) {
			err("Polarity check is not done or not [read_val_0x0012::0x%04x]\n", val);
			break;
		}
	} while (val);
	fimc_is_ois_i2c_read(core->client1, 0x0200, &val);
	err("OIS[read_val_0x0200::0x%04x]\n", val);
#endif

	retries = 120;
	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0013, &val);
		if (ret != 0) {
			break;
		}
		msleep(100);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	fimc_is_ois_i2c_read(core->client1, 0x0004, &val);
	info("OIS[read_val_0x0004::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0005, &val);
	info("OIS[read_val_0x0005::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0006, &val);
	info("OIS[read_val_0x0006::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0022, &val);
	info("OIS[read_val_0x0022::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0023, &val);
	info("OIS[read_val_0x0023::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0024, &val);
	info("OIS[read_val_0x0024::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0025, &val);
	info("OIS[read_val_0x0025::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0200, &val);
	info("OIS[read_val_0x0200::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x020E, &val);
	info("OIS[read_val_0x020E::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x020F, &val);
	info("OIS[read_val_0x020F::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0210, &val);
	info("OIS[read_val_0x0210::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0211, &val);
	info("OIS[read_val_0x0211::0x%04x]\n", val);

	fimc_is_ois_i2c_read(core->client1, 0x0212, &val);
	x = val;
	info("OIS[read_val_0x0212::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0213, &val);
	x_max = (val << 8) | x;
	info("OIS[read_val_0x0213::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0214, &val);
	x = val;
	info("OIS[read_val_0x0214::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0215, &val);
	x_min = (val << 8) | x;
	info("OIS[read_val_0x0215::0x%04x]\n", val);

	fimc_is_ois_i2c_read(core->client1, 0x0216, &val);
	y = val;
	info("OIS[read_val_0x0216::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0217, &val);
	y_max = (val << 8) | y;
	info("OIS[read_val_0x0217::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0218, &val);
	y = val;
	info("OIS[read_val_0x0218::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x0219, &val);
	y_min = (val << 8) | y;
	info("OIS[read_val_0x0219::0x%04x]\n", val);

	fimc_is_ois_i2c_read(core->client1, 0x021A, &val);
	info("OIS[read_val_0x021A::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x021B, &val);
	info("OIS[read_val_0x021B::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x021C, &val);
	info("OIS[read_val_0x021C::0x%04x]\n", val);
	fimc_is_ois_i2c_read(core->client1, 0x021D, &val);
	info("OIS[read_val_0x021D::0x%04x]\n", val);

	*x_diff = abs(x_max - x_min);
	*y_diff = abs(y_max - y_min);

	info("(%s) : X (default_diff:%d)(%d,%d)\n", __FUNCTION__,
			default_diff, *x_diff, *y_diff);

	ret = fimc_is_ois_i2c_read_multi(core->client1, 0x021A, read_x, 2);
	ret |= fimc_is_ois_i2c_read_multi(core->client1, 0x021C, read_y, 2);
	if (ret) {
		err("i2c read fail\n");
	}

	ret = fimc_is_ois_i2c_write_multi(core->client1, 0x0022, read_x, 4);
	ret |= fimc_is_ois_i2c_write_multi(core->client1, 0x0024, read_y, 4);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0002, 0x02);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0000, 0x01);
	msleep(400);
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0000, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	if (*x_diff > default_diff  && *y_diff > default_diff) {
		return true;
	} else {
		return false;
	}
}

bool fimc_is_ois_sine_wavecheck(struct fimc_is_core *core,
		              int threshold, int *sinx, int *siny, int *result)
{
	u8 buf = 0, val = 0;
	int ret = 0, retries = 10;
	int sinx_count = 0, siny_count = 0;
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x0052, (u8)threshold); /* error threshold level. */
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0053, 0x0); /* count value for error judgement level. */
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0054, 0x05); /* frequency level for measurement. */
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0055, 0x3A); /* amplitude level for measurement. */
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0057, 0x02); /* vyvle level for measurement. */
	ret |= fimc_is_ois_i2c_write(core->client1, 0x0050, 0x01); /* start sine wave check operation */
	if (ret) {
		err("i2c write fail\n");
		goto exit;
	}

	retries = 10;
	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0050, &val);
		if (ret) {
			err("i2c read fail\n");
			goto exit;
		}

		msleep(100);

		if (--retries < 0) {
			err("sine wave operation fail.\n");
			break;
		}
	} while (val);

	ret = fimc_is_ois_i2c_read(core->client1, 0x0051, &buf);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	*result = (int)buf;

	ret = fimc_is_ois_i2c_read_multi(core->client1, 0x00E4, u8_sinx_count, 2);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	ret |= fimc_is_ois_i2c_read_multi(core->client1, 0x00E6, u8_siny_count, 2);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	ret |= fimc_is_ois_i2c_read_multi(core->client1, 0x00E8, u8_sinx, 2);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	ret |= fimc_is_ois_i2c_read_multi(core->client1, 0x00EA, u8_siny, 2);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	info("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d\n",
		threshold, *sinx, *siny, sinx_count, siny_count);

	if (buf == 0x0) {
		return true;
	} else {
		return false;
	}

exit:
	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	*sinx = -1;
	*siny = -1;

	return false;
}

bool fimc_is_ois_auto_test_impl(struct fimc_is_core *core,
		            int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y)
{
	int result = 0;
	bool value = false;

#ifdef CONFIG_AF_HOST_CONTROL
	fimc_is_af_move_lens(core);
	msleep(100);
#endif

	value = fimc_is_ois_sine_wavecheck(core, threshold, sin_x, sin_y, &result);
	if (*sin_x == -1 && *sin_y == -1) {
		err("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;
		*sin_x = 0;
		*sin_y = 0;

		return false;
	}

	if (value == true) {
		*x_result = true;
		*y_result = true;

		return true;
	} else {
		if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}

		return false;
	}
}

int fimc_is_ois_read_manual_cal(struct fimc_is_core *core)
{
	int ret = 0;
	u8 version[20] = {0, };
	u8 read_ver[3] = {0, };
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	version[0] = 0x21;
	version[1] = 0x43;
	version[2] = 0x65;
	version[3] = 0x87;
	version[4] = 0x23;
	version[5] = 0x01;
	version[6] = 0xEF;
	version[7] = 0xCD;
	version[8] = 0x00;
	version[9] = 0x74;
	version[10] = 0x00;
	version[11] = 0x00;
	version[12] = 0x04;
	version[13] = 0x00;
	version[14] = 0x00;
	version[15] = 0x00;
	version[16] = 0x01;
	version[17] = 0x00;
	version[18] = 0x00;
	version[19] = 0x00;

	ret = fimc_is_ois_i2c_write_multi(core->client1, 0x0100, version, 0x16);
	msleep(5);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0118, &read_ver[0]);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x0119, &read_ver[1]);
	ret |= fimc_is_ois_i2c_read(core->client1, 0x011A, &read_ver[2]);
	if (ret) {
		err("i2c cmd fail\n");
		ret = -EINVAL;
		goto exit;
	}

	ois_minfo.header_ver[FW_CORE_VERSION] = read_ver[0];
	ois_minfo.header_ver[FW_GYRO_SENSOR] = read_ver[1];
	ois_minfo.header_ver[FW_DRIVER_IC] = read_ver[2];

exit:
	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return ret;
}

bool fimc_is_ois_fw_version(struct fimc_is_core *core)
{
	int ret = 0;
	char version[7] = {0, };
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00FB, &version[0]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00F9, &version[1]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00F8, &version[2]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007C, &version[3]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007D, &version[4]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007E, &version[5]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007F, &version[6]);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	memcpy(ois_minfo.header_ver, version, 7);
	specific->ois_ver_read = true;

	fimc_is_ois_fw_status(core);

	return true;

exit:
	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return false;
}

int fimc_is_ois_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_RELEASE_YEAR] - 58) * 10000;
	revision = revision + ((int)fw_ver[FW_RELEASE_MONTH] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_RELEASE_COUNT] - 48) * 10;
	revision = revision + (int)fw_ver[FW_RELEASE_COUNT + 1] - 48;

	return revision;
}

bool fimc_is_ois_read_userdata(struct fimc_is_core *core)
{
	u8 SendData[2];
	u8 Read_data[73] = {0, };
	int retries = 0;
	int ret = 0;
	u8 val = 0;
	u8 ois_shift_info = 0;
	int i = 0;

	struct i2c_client *client = core->client1;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	/* OIS servo OFF */
	fimc_is_ois_i2c_write(client ,0x0000, 0x00);
	retries = 50;
	do {
		ret = fimc_is_ois_i2c_read(client, 0x0000, &val);
		if (ret != 0) {
			break;
		}
		msleep(100);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%02x\n", val);
			break;
		}
	} while (val == 0x01);

	/* User Data Area & Address Setting step1 */
	fimc_is_ois_i2c_write(client ,0x000F, 0x12);
	SendData[0] = 0x00;
	SendData[1] = 0x00;
	fimc_is_ois_i2c_write_multi(client, 0x0010, SendData, 4);
	fimc_is_ois_i2c_write(client ,0x000E, 0x04);

	retries = 50;
	do {
		ret = fimc_is_ois_i2c_read(client, 0x000E, &val);
		if (ret != 0) {
			break;
		}
		msleep(100);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%02x\n", val);
			break;
		}
	} while (val != 0x14);

	fimc_is_ois_i2c_read_multi(client, 0x0100, Read_data, USER_FW_SIZE);
	memcpy(ois_uinfo.header_ver, Read_data, 7);
	info("Read register uinfo, data = %s\n", Read_data);
	for (i = 0; i < 72; i = i + 4) {
		info("OIS[user data::0x%02x%02x%02x%02x]\n",
			Read_data[i], Read_data[i + 1], Read_data[i + 2], Read_data[i + 3]);
	}

	/* User Data Area & Address Setting step2 */
	fimc_is_ois_i2c_write(client ,0x000F, 0x0E);
	SendData[0] = 0x00;
	SendData[1] = 0x04;
	fimc_is_ois_i2c_write_multi(client, 0x0010, SendData, 4);
	fimc_is_ois_i2c_write(client ,0x000E, 0x04);

	retries = 50;
	do {
		ret = fimc_is_ois_i2c_read(client, 0x000E, &val);
		if (ret != 0) {
			break;
		}
		msleep(10);

		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%02x\n", val);
			break;
		}
	} while (val != 0x14);

	fimc_is_ois_i2c_read(client, 0x0100, &ois_shift_info);
	info("OIS Shift Info : 0x%x\n", ois_shift_info);

	if (ois_shift_info == 0x11) {
		u16 ois_shift_checksum = 0;
		u16 ois_shift_x_diff = 0;
		u16 ois_shift_y_diff = 0;
		s16 ois_shift_x_cal = 0;
		s16 ois_shift_y_cal = 0;
		u8 calData[2];

		/* OIS Shift CheckSum */
		calData[0] = 0;
		calData[1] = 0;
		fimc_is_ois_i2c_read_multi(client, 0x0102, calData, 2);
		ois_shift_checksum = (calData[1] << 8) | (calData[0]);
		info("OIS Shift CheckSum = 0x%x\n", ois_shift_checksum);

		/* OIS Shift X Diff */
		calData[0] = 0;
		calData[1] = 0;
		fimc_is_ois_i2c_read_multi(client, 0x0104, calData, 2);
		ois_shift_x_diff = (calData[1] << 8) | (calData[0]);
		info("OIS Shift X Diff = 0x%x\n", ois_shift_x_diff);

		/* OIS Shift Y Diff */
		calData[0] = 0;
		calData[1] = 0;
		fimc_is_ois_i2c_read_multi(client, 0x0106, calData, 2);
		ois_shift_y_diff = (calData[1] << 8) | (calData[0]);
		info("OIS Shift Y Diff = 0x%x\n", ois_shift_y_diff);

		/* OIS Shift CAL DATA */
		for (i = 0; i < 9; i++) {
			calData[0] = 0;
			calData[1] = 0;
			fimc_is_ois_i2c_read_multi(client, 0x0108 + i*2, calData, 2);
			ois_shift_x_cal = (calData[1] << 8) | (calData[0]);

			calData[0] = 0;
			calData[1] = 0;
			fimc_is_ois_i2c_read_multi(client, 0x0120 + i*2, calData, 2);
			ois_shift_y_cal = (calData[1] << 8) | (calData[0]);
			info("OIS CAL[%d]:X[%d], Y[%d]\n", i, ois_shift_x_cal, ois_shift_y_cal);
		}
	}

	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	return true;
}

int fimc_is_ois_open_fw(struct fimc_is_core *core, char *name, u8 **buf)
{
	int ret = 0;
	ulong size = 0;
	const struct firmware *fw_blob = NULL;
	static char fw_name[100];
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread;
	int fw_requested = 1;
	int retry_count = 0;

	fw_sdcard = false;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	snprintf(fw_name, sizeof(fw_name), "%s%s", FIMC_IS_OIS_SDCARD_PATH, name);
	fp = filp_open(fw_name, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		info("failed to open SDCARD fw!!!\n");
		goto request_fw;
	}

	fw_requested = 0;
	size = fp->f_path.dentry->d_inode->i_size;
	info("start read sdcard, file path %s, size %lu Bytes\n", fw_name, size);

	*buf = vmalloc(size);
	if (!(*buf)) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	nread = vfs_read(fp, (char __user *)(*buf), size, &fp->f_pos);
	if (nread != size) {
		err("failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto p_err;
	}

	memcpy(ois_pinfo.header_ver, *buf + OIS_BIN_HEADER, 3);
	memcpy(&ois_pinfo.header_ver[3], *buf + OIS_BIN_HEADER - 4, 4);
	memcpy(progCode, *buf, OIS_BOOT_FW_SIZE + OIS_PROG_FW_SIZE);

	fw_sdcard = true;
	if (OIS_BIN_LEN >= nread) {
		not_crc_bin = true;
		err("ois fw binary size = %ld.\n", nread);
	}

request_fw:
	if (fw_requested) {
		snprintf(fw_name, sizeof(fw_name), "%s", name);
		set_fs(old_fs);
		retry_count = 3;
		ret = request_firmware(&fw_blob, fw_name, &core->preproc.pdev->dev);
		while (--retry_count && ret == -EAGAIN) {
			err("request_firmware retry(count:%d)", retry_count);
			ret = request_firmware(&fw_blob, fw_name, &core->preproc.pdev->dev);
		}

		if (ret) {
			err("request_firmware is fail(ret:%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}

		if (!fw_blob) {
			err("fw_blob is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		if (!fw_blob->data) {
			err("fw_blob->data is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		size = fw_blob->size;

		*buf = vmalloc(size);
		if (!(*buf)) {
			err("failed to allocate memory");
			ret = -ENOMEM;
			goto p_err;
		}

		memcpy((void *)(*buf), fw_blob->data, size);
		memcpy(ois_pinfo.header_ver, *buf + OIS_BIN_HEADER, 3);
		memcpy(&ois_pinfo.header_ver[3], *buf + OIS_BIN_HEADER - 4, 4);
		memcpy(progCode, *buf, OIS_BOOT_FW_SIZE + OIS_PROG_FW_SIZE);

		if (OIS_BIN_LEN >= size) {
			not_crc_bin = true;
			err("ois fw binary size = %lu.\n", size);
		}
		info("OIS firmware is loaded from Phone binary.\n");
	}

p_err:
	if (!fw_requested) {
		if (!IS_ERR_OR_NULL(fp)) {
			filp_close(fp, current->files);
		}
		set_fs(old_fs);
	} else {
		if (!IS_ERR_OR_NULL(fw_blob)) {
			release_firmware(fw_blob);
		}
	}

	return ret;
}

bool fimc_is_ois_check_fw_impl(struct fimc_is_core *core)
{
	u8 *buf = NULL;
	int ret = 0;

	ret = fimc_is_ois_fw_version(core);

	if (!ret) {
		err("Failed to read ois fw version.");
		return false;
	}

	fimc_is_ois_read_userdata(core);

	if (ois_minfo.header_ver[FW_CORE_VERSION] == 'A' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'C' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'M' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'O') {
		ret = fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME_DOM, &buf);
	} else if (ois_minfo.header_ver[FW_CORE_VERSION] == 'B' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'D' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'N' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'P') {
		ret |= fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME_SEC, &buf);
	}

	if (ret) {
		err("fimc_is_ois_open_fw failed(%d)\n", ret);
	}

	if (buf) {
		vfree(buf);
	}

	return true;
}

#ifdef CAMERA_USE_OIS_EXT_CLK
void fimc_is_ois_check_extclk(struct fimc_is_core *core)
{
	u8 cur_clk[4] = {0, };
	u8 new_clk[4] = {0, };
	u32 cur_clk_32 = 0;
	u32 new_clk_32 = 0;
	u8 pll_multi = 0;
	u8 pll_divide = 0;
	int ret = 0;
	u8 status = 0;
	int retries = 5;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	do {
		ret = fimc_is_ois_i2c_read(core->client1, 0x0001, &status);
		if (ret != 0) {
			status = -EIO;
			err("i2c read fail\n");
			break;
		}
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!,clk  data = 0x%02x\n", status);
			break;
		}
	} while (status != 0x01);

	ret = fimc_is_ois_i2c_read_multi(core->client1, 0x03F0, cur_clk, 4);
	if (ret) {
		err("i2c read fail\n");
		goto exit;
	}

	cur_clk_32 = (cur_clk[3] << 24)  | (cur_clk[2]  << 16) |
		(cur_clk[1]  << 8) | (cur_clk[0]);

	if (cur_clk_32 != CAMERA_OIS_EXT_CLK) {
		new_clk_32 = CAMERA_OIS_EXT_CLK;
		new_clk[0] = CAMERA_OIS_EXT_CLK & 0xFF;
		new_clk[1] = (CAMERA_OIS_EXT_CLK >> 8) & 0xFF;
		new_clk[2] = (CAMERA_OIS_EXT_CLK >> 16) & 0xFF;
		new_clk[3] = (CAMERA_OIS_EXT_CLK >> 24) & 0xFF;

		switch (new_clk_32) {
		case 0xB71B00:
			pll_multi = 0x08;
			pll_divide = 0x03;
			break;
		case 0x1036640:
			pll_multi = 0x09;
			pll_divide = 0x05;
			break;
		case 0x124F800:
			pll_multi = 0x05;
			pll_divide = 0x03;
			break;
		case 0x16E3600:
			pll_multi = 0x04;
			pll_divide = 0x03;
			break;
		case 0x18CBA80:
			pll_multi = 0x06;
			pll_divide = 0x05;
			break;
		default:
			info("cur_clk: 0x%08x\n", cur_clk_32);
			goto exit;
		}

		/* write EXTCLK */
		ret = fimc_is_ois_i2c_write_multi(core->client1, 0x03F0, new_clk, 6);
		if (ret) {
			err("i2c write fail\n");
			goto exit;
		}

		/* write PLLMULTIPLE */
		ret = fimc_is_ois_i2c_write(core->client1, 0x03F4, pll_multi);
		if (ret) {
			err("i2c write fail\n");
			goto exit;
		}

		/* write PLLDIVIDE */
		ret = fimc_is_ois_i2c_write(core->client1, 0x03F5, pll_divide);
		if (ret) {
			err("i2c write fail\n");
			goto exit;
		}

		/* FLASH TO OIS MODULE */
		ret = fimc_is_ois_i2c_write(core->client1, 0x0003, 0x01);
		if (ret) {
			err("i2c write fail\n");
			msleep(200);
			goto exit;
		}
		msleep(200);

		/* S/W Reset */
		ret = fimc_is_ois_i2c_write(core->client1 ,0x000D, 0x01);
		ret |= fimc_is_ois_i2c_write(core->client1 ,0x000E, 0x06);
		if (ret) {
			err("i2c write fail\n");
			msleep(50);
			goto exit;
		}
		msleep(50);

		info("Apply EXTCLK for ois 0x%08x\n", new_clk_32);
	}else {
		info("Keep current EXTCLK 0x%08x\n", cur_clk_32);
	}

exit:
	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, false);
	}

	return;
}
#endif

void fimc_is_ois_fw_update_impl(struct fimc_is_core *core)
{
	u8 SendData[256], RcvData;
	u16 RcvDataShort = 0;
	int block, position = 0;
	u16 checkSum;
	u8 *buf = NULL;
	int ret = 0, sum_size = 0, retry_count = 3;
	int module_ver = 0, binary_ver = 0;
	bool forced_update = false, crc_result = false;
	bool need_reset = false;
	u8 ois_status[4] = {0, };
	u32 ois_status_32 = 0;
	struct i2c_client *client = core->client1;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	/* OIS Status Check */
	if (specific->use_ois_hsi2c) {
	    fimc_is_ois_i2c_config(core->client1, true);
	}

	ret = fimc_is_ois_i2c_read_multi(core->client1, 0x00FC, ois_status, 4);
	if (ret) {
		err("i2c read fail\n");
		goto p_err;
	}

	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	ois_status_32 = (ois_status[3] << 24)  | (ois_status[2]  << 16) |
		(ois_status[1]  << 8) | (ois_status[0]);

	if (ois_status_32 == OIS_FW_UPDATE_ERROR_STATUS) {
		forced_update = true;
		fimc_is_ois_read_manual_cal(core);
	} else {
#ifdef CAMERA_USE_OIS_EXT_CLK
		/* Check EXTCLK value */
		fimc_is_ois_check_extclk(core);
#endif
	}

	if (!forced_update) {
		ret = fimc_is_ois_fw_version(core);
		if (!ret) {
			err("Failed to read ois fw version.");
			return;
		}		
		fimc_is_ois_read_userdata(core);
	}
	
	if (ois_minfo.header_ver[FW_CORE_VERSION] == 'A' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'C' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'M' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'O') {
		ret = fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME_DOM, &buf);
	} else if (ois_minfo.header_ver[FW_CORE_VERSION] == 'B' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'D' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'N' ||
		ois_minfo.header_ver[FW_CORE_VERSION] == 'P') {
		ret |= fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME_SEC, &buf);
	}

	if (ret) {
		err("fimc_is_ois_open_fw failed(%d)", ret);
	}

	if (ois_minfo.header_ver[FW_CORE_VERSION] != CAMERA_OIS_DOM_UPDATE_VERSION
		&& ois_minfo.header_ver[FW_CORE_VERSION] != CAMERA_OIS_SEC_UPDATE_VERSION) {
		info("Do not update ois Firmware. FW version is low.\n");
		goto p_err;
	}

	if (buf == NULL) {
		err("buf is NULL. OIS FW Update failed.");
		return;
	}

	if (!forced_update) {
		if (ois_uinfo.header_ver[0] == 0xFF && ois_uinfo.header_ver[1] == 0xFF &&
			ois_uinfo.header_ver[2] == 0xFF) {
			err("OIS userdata is not valid.");
			goto p_err;
		}
	}

	/*Update OIS FW when Gyro sensor/Driver IC/ Core version is same*/
	if (!forced_update) {
		if (!fimc_is_ois_version_compare(ois_minfo.header_ver, ois_pinfo.header_ver,
			ois_uinfo.header_ver)) {
			err("Does not update ois fw. OIS module ver = %s, binary ver = %s, userdata ver = %s",
				ois_minfo.header_ver, ois_pinfo.header_ver, ois_uinfo.header_ver);
			goto p_err;
		}
	} else {
		if (!fimc_is_ois_version_compare_default(ois_minfo.header_ver, ois_pinfo.header_ver)) {
			err("Does not update ois fw. OIS module ver = %s, binary ver = %s",
				ois_minfo.header_ver, ois_pinfo.header_ver);
			goto p_err;
		}
	}

	crc_result = fimc_is_ois_crc_check(core, buf);
	if (crc_result == false) {
		err("OIS CRC32 error.\n");
		goto p_err;
	}

	if (!fw_sdcard && !forced_update) {
		module_ver = fimc_is_ois_fw_revision(ois_minfo.header_ver);
		binary_ver = fimc_is_ois_fw_revision(ois_pinfo.header_ver);

		if (binary_ver <= module_ver) {
			err("OIS module ver = %d, binary ver = %d, module ver >= bin ver", module_ver, binary_ver);
			goto p_err;
		}
	}
	info("OIS fw update started. module ver = %s, binary ver = %s, userdata ver = %s\n",
		ois_minfo.header_ver, ois_pinfo.header_ver, ois_uinfo.header_ver);

	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, true);
	}

retry:
	if (need_reset) {
		fimc_is_ois_gpio_off(core);
		msleep(30);
		fimc_is_ois_gpio_on(core);
		msleep(150);
	}

	fimc_is_ois_i2c_read(client, 0x0001, &RcvData); /* OISSTS Read */

	if (RcvData != 0x01) {/* OISSTS != IDLE */
		err("RCV data return!!");
		goto p_err;
	}

	/* Update a User Program */
	/* SET FWUP_CTRL REG */
	position = 0;
	SendData[0] = 0x75;		/* FWUP_DSIZE=256Byte, FWUP_MODE=PROG, FWUPEN=Start */
	fimc_is_ois_i2c_write(client, 0x000C, SendData[0]);		/* FWUPCTRL REG(0x000C) 1Byte Send */
	msleep(55);

	/* Write UserProgram Data */
	for (block = 0; block < 112; block++) {		/* 0x1000 -0x 6FFF (RUMBA-SA)*/
		memcpy(SendData, &progCode[position], (size_t)FW_TRANS_SIZE);
		fimc_is_ois_i2c_write_multi(client, 0x0100, SendData, FW_TRANS_SIZE + 2); /* FLS_DATA REG(0x0100) 256Byte Send */
		position += FW_TRANS_SIZE;
		msleep(10);
	}

	/* CHECKSUM (Program) */
	sum_size = OIS_PROG_FW_SIZE + OIS_BOOT_FW_SIZE;
	checkSum = fimc_is_ois_calc_checksum(&progCode[0], sum_size);
	SendData[0] = (checkSum & 0x00FF);
	SendData[1] = (checkSum & 0xFF00) >> 8;
	SendData[2] = 0;		/* Don't Care */
	SendData[3] = 0x80;		/* Self Reset Request */

	fimc_is_ois_i2c_write_multi(client, 0x08, SendData, 6);  /* FWUP_CHKSUM REG(0x0008) 4Byte Send */
	msleep(190);		/* (Wait RUMBA Self Reset) */
	fimc_is_ois_i2c_read(client, 0x6, (u8*)&RcvDataShort); /* Error Status read */

	if (RcvDataShort == 0x00) {
		/* F/W Update Success Process */
		info("%s: OISLOG OIS program update success\n", __func__);
	} else {
		/* Retry Process */
		if (retry_count > 0) {
			err("OISLOG OIS program update fail, retry update. count = %d", retry_count);
			retry_count--;
			need_reset = true;
			goto retry;
		}

		/* F/W Update Error Process */
		err("OISLOG OIS program update fail");
		goto p_err;
	}

	/* S/W Reset */
	fimc_is_ois_i2c_write(client ,0x000D, 0x01);
	fimc_is_ois_i2c_write(client ,0x000E, 0x06);
	msleep(50);

	/* Param init - Flash to Rumba */
	fimc_is_ois_i2c_write(client ,0x0036, 0x03);
	msleep(200);
	info("%s: OISLOG OIS param init done.\n", __func__);

	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	ret = fimc_is_ois_fw_version(core);
	if (!ret) {
		err("Failed to read ois fw version.");
		goto p_err;
	}

	if (!fimc_is_ois_version_compare_default(ois_minfo.header_ver, ois_pinfo.header_ver)) {
		err("After update ois fw is not correct. OIS module ver = %s, binary ver = %s",
			ois_minfo.header_ver, ois_pinfo.header_ver);
		goto p_err;
	}

p_err:
	info("OIS module ver = %s, binary ver = %s, userdata ver = %s\n",
		ois_minfo.header_ver, ois_pinfo.header_ver, ois_uinfo.header_ver);

	if (specific->use_ois_hsi2c) {
		fimc_is_ois_i2c_config(core->client1, false);
	}

	if (buf) {
		vfree(buf);
	}
	return;
}

MODULE_DESCRIPTION("OIS driver for Rumba S4");
MODULE_AUTHOR("kyoungho yun <kyoungho.yun@samsung.com>");
MODULE_LICENSE("GPL v2");
