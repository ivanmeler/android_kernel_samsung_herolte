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

#include "fimc-is-core.h"
#include "fimc-is-interface.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-af.h"
#include "fimc-is-vender-specific.h"

#define FIMC_IS_AF_DEV_NAME "exynos-fimc-is-af"
static int af_noise_count;

static struct remove_af_noise af_sensor_interface = {
	.af_pdata = NULL,
	.af_func = NULL,
};

static void fimc_is_af_i2c_config(struct i2c_client *client, bool onoff)
{
	struct device *i2c_dev = client->dev.parent->parent;
	struct pinctrl *pinctrl_i2c = NULL;

	info("(%s):onoff(%d)\n", __func__, onoff);
	if (onoff) {
		/* ON */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
	} else {
		/* OFF */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "default");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
    }

}

int fimc_is_af_i2c_read(struct i2c_client *client, u16 addr, u16 *data)
{
	int err;
	u8 rxbuf[2], txbuf[2];
	struct i2c_msg msg[2];

	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail err = %d\n", __func__, err);
		return -EIO;
	}

	*data = ((rxbuf[0] << 8) | rxbuf[1]);
	return 0;
}

int fimc_is_af_i2c_write(struct i2c_client *client ,u8 addr, u8 data)
{
        int retries = I2C_RETRY_COUNT;
        int ret = 0, err = 0;
        u8 buf[2] = {0,};
        struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = 2,
                .buf    = buf,
        };

        buf[0] = addr;
        buf[1] = data;

#if 0
        pr_info("%s : W(0x%02X%02X)\n",__func__, buf[0], buf[1]);
#endif

        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%02X, %02X), retry %d\n",
                        err, addr, data, I2C_RETRY_COUNT - retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
        }

        return 0;
}

int fimc_is_af_ldo_enable(char *name, bool on)
{
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct regulator *regulator = NULL;
	struct platform_device *pdev = NULL;
	int ret = 0;

	BUG_ON(!core);
	BUG_ON(!core->pdev);

	pdev = core->pdev;

	regulator = regulator_get(&pdev->dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail\n", __func__, name);
		regulator_put(regulator);
		return -EINVAL;
	}

	if (on) {
		if (regulator_is_enabled(regulator)) {
			pr_info("%s: regulator is already enabled\n", name);
			goto exit;
		}

		ret = regulator_enable(regulator);
		if (ret) {
			err("%s : regulator_enable(%s) fail\n", __func__, name);
			goto exit;
		}
	} else {
		if (!regulator_is_enabled(regulator)) {
			pr_info("%s: regulator is already disabled\n", name);
			goto exit;
		}

		ret = regulator_disable(regulator);
		if (ret) {
			err("%s : regulator_disable(%s) fail\n", __func__, name);
			goto exit;
		}
	}
exit:
	regulator_put(regulator);

	return ret;
}

int fimc_is_af_power(struct fimc_is_device_af *af_device, bool onoff)
{
	int ret = 0;

	/*VDDAF_2.8V_CAM*/
	ret = fimc_is_af_ldo_enable("VDDAF_2.8V_CAM", onoff);
	if (ret) {
		err("failed to power control VDDAF_2.8V_CAM, onoff = %d", onoff);
		return -EINVAL;
	}

#ifdef CONFIG_OIS_USE
	/* OIS_VDD_2.8V */
	ret = fimc_is_af_ldo_enable("OIS_VDD_2.8V", onoff);
	if (ret) {
		err("failed to power control OIS_VDD_2.8V, onoff = %d", onoff);
		return -EINVAL;
	}

	/* OIS_VM_2.8V */
	ret = fimc_is_af_ldo_enable("OIS_VM_2.8V", onoff);
	if (ret) {
		err("failed to power control OIS_VM_2.8V, onoff = %d", onoff);
		return -EINVAL;
	}
#ifdef CAMERA_USE_OIS_VDD_1_8V
	/* OIS_VDD_1.8V */
	ret = fimc_is_af_ldo_enable("OIS_VDD_1.8V", onoff);
	if (ret) {
		err("failed to power control OIS_VDD_1.8V, onoff = %d", onoff);
		return -EINVAL;
	}
#endif /* CAMERA_USE_OIS_VDD_1_8V */
#endif

	/*VDDIO_1.8V_CAM*/
	ret = fimc_is_af_ldo_enable("VDDIO_1.8V_CAM", onoff);
	if (ret) {
		err("failed to power control VDDIO_1.8V_CAM, onoff = %d", onoff);
		return -EINVAL;
	}

	usleep_range(5000,5000);
	return ret;
}

bool fimc_is_check_regulator_status(char *name)
{
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct regulator *regulator = NULL;
	struct platform_device *pdev = NULL;
	int ret = 0;

	BUG_ON(!core);
	BUG_ON(!core->pdev);

	pdev = core->pdev;

	regulator = regulator_get(&pdev->dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail\n", __func__, name);
		regulator_put(regulator);
		return false;
	}
	if (regulator_is_enabled(regulator)) {
		ret = true;
	} else {
		ret = false;
	}

	regulator_put(regulator);
	return ret;
}

int16_t fimc_is_af_enable(void *device, bool onoff)
{
	int ret = 0;
	struct fimc_is_device_af *af_device = (struct fimc_is_device_af *)device;
	struct fimc_is_core *core;
	bool af_regulator = false, io_regulator = false;
	struct fimc_is_vender_specific *specific;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -ENODEV;
	}

	specific = core->vender.private_data;

	pr_info("af_noise : running_rear_camera = %d, onoff = %d\n", specific->running_rear_camera, onoff);
	if (!specific->running_rear_camera) {
		if (specific->use_ois_hsi2c) {
			fimc_is_af_i2c_config(af_device->client, true);
		}

		if (onoff) {
			fimc_is_af_power(af_device, true);
			ret = fimc_is_af_i2c_write(af_device->client, 0x02, 0x00);
			if (ret) {
				err("i2c write fail\n");
				goto power_off;
			}

			ret = fimc_is_af_i2c_write(af_device->client, 0x00, 0x00);
			if (ret) {
				err("i2c write fail\n");
				goto power_off;
			}

			ret = fimc_is_af_i2c_write(af_device->client, 0x01, 0x00);
			if (ret) {
				err("i2c write fail\n");
				goto power_off;
			}
			af_noise_count++;
			pr_info("af_noise : count = %d\n", af_noise_count);
		} else {
			/* Check the Power Pins */
			af_regulator = fimc_is_check_regulator_status("VDDAF_2.8V_CAM");
			io_regulator = fimc_is_check_regulator_status("VDDIO_1.8V_CAM");

			if (af_regulator && io_regulator) {
				ret = fimc_is_af_i2c_write(af_device->client, 0x02, 0x40);
				if (ret) {
					err("i2c write fail\n");
				}
				fimc_is_af_power(af_device, false);
			} else {
				pr_info("already power off.(%d)\n", __LINE__);
			}
		}

		if (specific->use_ois_hsi2c) {
			fimc_is_af_i2c_config(af_device->client, false);
		}
	}

	return ret;

power_off:
	if (!specific->running_rear_camera) {
		if (specific->use_ois_hsi2c) {
			fimc_is_af_i2c_config(af_device->client, false);
		}

		af_regulator = fimc_is_check_regulator_status("VDDAF_2.8V_CAM");
		io_regulator = fimc_is_check_regulator_status("VDDIO_1.8V_CAM");
		if (af_regulator && io_regulator) {
			fimc_is_af_power(af_device, false);
		} else {
			pr_info("already power off.(%d)\n", __LINE__);
		}
	}
	return ret;
}

int16_t fimc_is_af_move_lens(struct fimc_is_core *core)
{
	int ret = 0;
	struct i2c_client *client = core->client2;
	struct fimc_is_vender_specific *specific = core->vender.private_data;

	pr_info("fimc_is_af_move_lens : running_rear_camera = %d\n", specific->running_rear_camera);
	if (!specific->running_rear_camera) {
		if (specific->use_ois_hsi2c) {
			fimc_is_af_i2c_config(client, true);
		}

		ret = fimc_is_af_i2c_write(client, 0x00, 0x80);
		if (ret) {
			err("i2c write fail\n");
		}

		ret = fimc_is_af_i2c_write(client, 0x01, 0x00);
		if (ret) {
			err("i2c write fail\n");
		}

		ret = fimc_is_af_i2c_write(client, 0x02, 0x00);
		if (ret) {
			err("i2c write fail\n");
		}

		if (specific->use_ois_hsi2c) {
			fimc_is_af_i2c_config(client, false);
		}
	}

	return ret;
}

int fimc_is_af_parse_dt(struct i2c_client *client)
{
	int ret = 0;

	return ret;
}

#ifdef CONFIG_SENSORS_SSP_BBD
extern int remove_af_noise_register(struct remove_af_noise *af_cam);
extern void remove_af_noise_unregister(struct remove_af_noise *af_cam);
#endif
static int fimc_is_af_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_device_af *device;
	struct fimc_is_core *core;
	int ret;

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed");
		client->dev.init_name = FIMC_IS_AF_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		return -EINVAL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err("No I2C functionality found\n");
		return -ENODEV;
	}

	device = kzalloc(sizeof(struct fimc_is_device_af), GFP_KERNEL);
	if (!device) {
		err("fimc_is_device_companion is NULL");
		return -ENOMEM;
	}

	core->client2 = client;
	device->client = client;
	device->core = core;
	af_noise_count = 0;

	af_sensor_interface.af_pdata = device;
	af_sensor_interface.af_func = &fimc_is_af_enable;
#ifdef CONFIG_SENSORS_SSP_BBD
	ret = remove_af_noise_register(&af_sensor_interface);
	if (ret)
		err("reduce_af_noise_register failed: %d\n", ret);
#endif
	i2c_set_clientdata(client, device);

	if (client->dev.of_node) {
		ret = fimc_is_af_parse_dt(client);
		if (ret) {
			err("parsing device tree is fail(%d)", ret);
			return -ENODEV;
		}
	}

	return 0;
}

static int fimc_is_af_remove(struct i2c_client *client)
{
#ifdef CONFIG_SENSORS_SSP_BBD
	remove_af_noise_unregister(&af_sensor_interface);
#endif
	return 0;
}

static const struct i2c_device_id af_id[] = {
	{FIMC_IS_AF_DEV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, af_id);

#ifdef CONFIG_OF
static struct of_device_id af_dt_ids[] = {
	{ .compatible = "samsung,af",},
	{},
};
#endif

static struct i2c_driver af_i2c_driver = {
	.driver = {
		   .name = FIMC_IS_AF_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = af_dt_ids,
#endif
	},
	.probe = fimc_is_af_probe,
	.remove = fimc_is_af_remove,
	.id_table = af_id,
};

module_i2c_driver(af_i2c_driver);

MODULE_DESCRIPTION("AF driver for remove noise");
MODULE_AUTHOR("kyoungho yun <kyoungho.yun@samsung.com>");
MODULE_LICENSE("GPL v2");
