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
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-fimc-is-sensor.h>
#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-module-4h5yc.h"

#ifdef USE_FF_MODULE
#define SENSOR_NAME "S5K4H5YC_FF"
#else
#define SENSOR_NAME "S5K4H5YC"
#endif

static struct fimc_is_sensor_cfg config_4h5yc[] = {
        /* 3280x2458@30fps */
        FIMC_IS_SENSOR_CFG(3280, 2458, 30, 15, 0, CSI_DATA_LANES_4),
        /* 3280x1846@30fps */
        FIMC_IS_SENSOR_CFG(3280, 1846, 30, 15, 1, CSI_DATA_LANES_4),
        /* 816x604@115fps */
        FIMC_IS_SENSOR_CFG(816, 604, 115, 15, 2, CSI_DATA_LANES_4),
        /* 816x460@120fps */
        FIMC_IS_SENSOR_CFG(816, 460, 120, 15, 3, CSI_DATA_LANES_4),
        /* 1640x1228@60fps */
        FIMC_IS_SENSOR_CFG(1640, 1228, 60, 15, 4, CSI_DATA_LANES_4),
        /* 1640x924@60fps */
        FIMC_IS_SENSOR_CFG(1640, 924, 60, 15, 5, CSI_DATA_LANES_4),
        /* 1640x1228@7fps */
        FIMC_IS_SENSOR_CFG(1640, 1228, 7, 15, 6, CSI_DATA_LANES_4),
        /* 1640x1228@15fps */
        FIMC_IS_SENSOR_CFG(1640, 1228, 15, 15, 7, CSI_DATA_LANES_4),
        /* 1640x1228@30fps */
        FIMC_IS_SENSOR_CFG(1640, 1228, 30, 15, 8, CSI_DATA_LANES_4),
        /* 1640x924@15fps */
        FIMC_IS_SENSOR_CFG(1640, 924, 15, 15, 9, CSI_DATA_LANES_4),
};

static struct fimc_is_vci vci_4h5yc[] = {
        {
                .pixelformat = V4L2_PIX_FMT_SBGGR10,
                .config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_UNKNOWN}, {3, 0}}
        }, {
                .pixelformat = V4L2_PIX_FMT_SBGGR12,
                .config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_UNKNOWN}, {3, 0}}
        }, {
                .pixelformat = V4L2_PIX_FMT_SBGGR16,
                .config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_UNKNOWN}, {3, 0}}
        }
};

static int sensor_4h5yc_init(struct v4l2_subdev *subdev, u32 val)
{
        int ret = 0;
        struct fimc_is_module_enum *module;

        BUG_ON(!subdev);

        module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);

        pr_info("[MOD:D:%d] %s(%d)\n", module->sensor_id, __func__, val);

        return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
        .init = sensor_4h5yc_init
};

static const struct v4l2_subdev_ops subdev_ops = {
        .core = &core_ops,
};

#ifdef CONFIG_OF
static int sensor_4h5yc_power_setpin(struct platform_device *pdev,
	struct exynos_platform_fimc_is_module *pdata)
{
        struct device *dev;
        struct device_node *dnode;
        int gpio_reset = 0;
        int gpio_mclk = 0;
        int gpio_none = 0;

        BUG_ON(!pdev);

        dev = &pdev->dev;
        dnode = dev->of_node;

        dev_info(dev, "%s E\n", __func__);

        gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
        if (!gpio_is_valid(gpio_reset)) {
                dev_err(dev, "failed to get PIN_RESET\n");
                return -EINVAL;
        } else {
                gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
                gpio_free(gpio_reset);
        }

        gpio_mclk = of_get_named_gpio(dnode, "gpio_mclk", 0);
        if (!gpio_is_valid(gpio_mclk)) {
                dev_err(dev, "%s: failed to get mclk\n", __func__);
                return -EINVAL;
        } else {
                if (gpio_request_one(gpio_mclk, GPIOF_OUT_INIT_LOW, "CAM_MCLK_OUTPUT_LOW")) {
                        dev_err(dev, "%s: failed to gpio request mclk\n", __func__);
                        return -ENODEV;
                }
                gpio_free(gpio_mclk);
        }

        SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
        SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);

        SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
        SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);

        /* Normal on */
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "sen_rst low", PIN_OUTPUT, 0, 0);
        SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 1, 0, 1200000);
        SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDA_2.9V_CAM", PIN_REGULATOR, 1, 10, 2800000); /* 2.8V */
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 10);
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "sen_rst high", PIN_OUTPUT, 1, 100);    
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 2, 100);

        /* Normal off */
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 0, 0);
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 1, 0);
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 0, 0);
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_reset, "sen_rst", PIN_OUTPUT, 0, 10);
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDA_2.9V_CAM", PIN_REGULATOR, 0, 0);
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 0, 0);
        SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 0);

        /* READ_ROM - POWER ON */
        SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 0);
        /* READ_ROM - POWER OFF */
        SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 0);

        dev_info(dev, "%s X\n", __func__);

        return 0;
}

#endif

int sensor_4h5yc_probe(struct platform_device *pdev)
{
        int ret = 0;
        struct fimc_is_core *core;
        struct v4l2_subdev *subdev_module;
        struct fimc_is_module_enum *module;
        struct fimc_is_device_sensor *device;
        struct sensor_open_extended *ext;
        struct exynos_platform_fimc_is_module *pdata;
        struct device *dev;

        if (fimc_is_dev == NULL) {
                warn("fimc_is_dev is not yet probed(4h5yc)");
                pdev->dev.init_name = SENSOR_NAME;
                return -EPROBE_DEFER;
        }

        core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
        if (!core) {
                err("core device is not yet probed");
                return -EPROBE_DEFER;
        }

        dev = &pdev->dev;

#ifdef CONFIG_OF
        fimc_is_sensor_module_parse_dt(pdev, sensor_4h5yc_power_setpin);
#endif

        pdata = dev_get_platdata(dev);
        device = &core->sensor[pdata->id];

        subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
        if (!subdev_module) {
                err("subdev_module is NULL");
                ret = -ENOMEM;
                goto p_err;
        }

        module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
        atomic_inc(&core->resourcemgr.rsccount_module);
        clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
        module->pdata = pdata;
        module->dev = dev;
#ifdef USE_FF_MODULE
        module->sensor_id = SENSOR_NAME_S5K4H5YC_FF;
#else
        module->sensor_id = SENSOR_NAME_S5K4H5YC;
#endif
        module->subdev = subdev_module;
        module->device = pdata->id;
        module->client = NULL;
        module->active_width = 3264;
        module->active_height = 2448;
        module->pixel_width = module->active_width + 16;
        module->pixel_height = module->active_height + 10;
        module->max_framerate = 120;
        module->position = pdata->position;
        module->mode = CSI_MODE_CH0_ONLY;
        module->lanes = CSI_DATA_LANES_4;
        module->bitwidth = 10;
        module->vcis = ARRAY_SIZE(vci_4h5yc);
        module->vci = vci_4h5yc;
        module->sensor_maker = "SLSI";
        module->sensor_name = "S5K4H5YC";
        module->setfile_name = "setfile_4h5.bin";
        module->cfgs = ARRAY_SIZE(config_4h5yc);
        module->cfg = config_4h5yc;
        module->private_data = NULL;

        ext = &module->ext;
        ext->mipi_lane_num = module->lanes;
        ext->I2CSclk = 0;

        ext->sensor_con.product_name = SENSOR_S5K4H5YC_NAME;
        ext->sensor_con.peri_type = SE_I2C;
        ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;
        ext->sensor_con.peri_setting.i2c.slave_address = pdata->sensor_i2c_addr;
        ext->sensor_con.peri_setting.i2c.speed = 400000;

        ext->from_con.product_name = FROMDRV_NAME_NOTHING;

        ext->preprocessor_con.product_name = pdata->preprocessor_product_name;
        ext->ois_con.product_name = pdata->ois_product_name;
        ext->actuator_con.product_name = pdata->af_product_name;
        ext->flash_con.product_name = pdata->flash_product_name;

        v4l2_subdev_init(subdev_module, &subdev_ops);

        v4l2_set_subdevdata(subdev_module, module);
        v4l2_set_subdev_hostdata(subdev_module, device);
        snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);

p_err:
        probe_info("%s(%d)\n", __func__, ret);
        return ret;
}

static int sensor_4h5yc_remove(struct platform_device *pdev)
{
        int ret = 0;
        return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_4h5yc_match[] = {
        {
                .compatible = "samsung,exynos5-fimc-is-sensor-4h5yc",
        },
        {},
};

MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_4h5yc_match);

static struct platform_driver sensor_4h5yc_driver = {
        .probe  = sensor_4h5yc_probe,
        .remove = sensor_4h5yc_remove,
        .driver = {
                .name   = SENSOR_NAME,
                .owner  = THIS_MODULE,
                .of_match_table = exynos_fimc_is_sensor_4h5yc_match,
        }
};
static int __init fimc_is_sensor_module_init(void)
{
        int ret = platform_driver_register(&sensor_4h5yc_driver);
        if (ret)
                err("platform_driver_register failed: %d\n", ret);

        return ret;
}
late_initcall(fimc_is_sensor_module_init);

static void __exit fimc_is_sensor_module_exit(void)
{
        platform_driver_unregister(&sensor_4h5yc_driver);
}
module_exit(fimc_is_sensor_module_exit);

MODULE_AUTHOR("Sunggeun Yim");
MODULE_DESCRIPTION("Sensor 4H5YC driver");
MODULE_LICENSE("GPL v2");
