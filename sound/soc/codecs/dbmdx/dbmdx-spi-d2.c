/*
 * DSPG DBMD2 SPI interface driver
 *
 * Copyright (C) 2014 DSP Group
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* #define DEBUG */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/firmware.h>

#include "dbmdx-interface.h"
#include "dbmdx-va-regmap.h"
#include "dbmdx-vqe-regmap.h"
#include "dbmdx-spi.h"

#define DBMD2_MAX_SPI_BOOT_SPEED 8000000
#define DBMD2_VERIFY_BOOT_CHECKSUM 1

static const u8 clr_crc[] = {0x5A, 0x03, 0x52, 0x0a, 0x00,
			     0x00, 0x00, 0x00, 0x00, 0x00};
static const u8 chng_pll_cmd[] = {0x5A, 0x13,
				0x3D, 0x10, 0x00, 0x00,
				0x00, 0x16, 0x00, 0x10,
				0xFF, 0xF1, 0x3F, 0x00};

static int dbmd2_spi_boot(const void *fw_data, size_t fw_size,
		struct dbmdx_private *p, const void *checksum,
		size_t chksum_len, int load_fw)
{
	int retry = RETRY_COUNT;
	int ret = 0;
	ssize_t send_bytes;
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	struct spi_device *spi = spi_p->client;
	u8 sbl_spi[] = {
		0x5A, 0x04, 0x4c, 0x00, 0x00,
		0x03, 0x11, 0x55, 0x05, 0x00};


	dev_dbg(spi_p->dev, "%s\n", __func__);

	while (retry--) {

		if (p->active_fw == DBMDX_FW_PRE_BOOT) {

			/* reset DBMD2 chip */
			p->reset_sequence(p);

			/* delay before sending commands */
			if (p->clk_get_rate(p, DBMDX_CLK_MASTER) <= 32768)
				msleep(DBMDX_MSLEEP_SPI_D2_AFTER_RESET_32K);
			else
				usleep_range(DBMDX_USLEEP_SPI_D2_AFTER_RESET,
					DBMDX_USLEEP_SPI_D2_AFTER_RESET + 5000);

			ret = spi_set_speed(p, DBMDX_VA_SPEED_MAX);
			if (ret < 0) {
				dev_err(spi_p->dev, "%s:failed %x\n",
					__func__, ret);
				continue;
			}

			if (spi->max_speed_hz > DBMD2_MAX_SPI_BOOT_SPEED) {

				ret = spi_set_speed(p, DBMDX_VA_SPEED_NORMAL);
				if (ret < 0) {
					dev_err(spi_p->dev, "%s:failed %x\n",
						__func__, ret);
					continue;
				}

				/* send Change PLL clear command */
				ret = send_spi_data(p, chng_pll_cmd,
					sizeof(chng_pll_cmd));
				if (ret != sizeof(chng_pll_cmd)) {
					dev_err(p->dev,
						"%s: failed to change PLL\n",
						__func__);
					continue;
				}
				msleep(DBMDX_MSLEEP_SPI_D2_AFTER_PLL_CHANGE);

				ret = spi_set_speed(p, DBMDX_VA_SPEED_MAX);
				if (ret < 0) {
					dev_err(spi_p->dev, "%s:failed %x\n",
						__func__, ret);
					continue;
				}
			}

			/* send SBL */
			send_bytes = send_spi_data(p, sbl_spi, sizeof(sbl_spi));
			if (send_bytes != sizeof(sbl_spi)) {
				dev_err(p->dev,
					"%s: -----------> load SBL error\n",
					__func__);
				continue;
			}

			usleep_range(DBMDX_USLEEP_SPI_D2_AFTER_SBL,
				DBMDX_USLEEP_SPI_D2_AFTER_SBL + 1000);

			/* send CRC clear command */
			ret = send_spi_data(p, clr_crc, sizeof(clr_crc));
			if (ret != sizeof(clr_crc)) {
				dev_err(p->dev, "%s: failed to clear CRC\n",
					 __func__);
				continue;
			}
		} else {
			/* delay before sending commands */
			if (p->active_fw == DBMDX_FW_VQE)
				ret = send_spi_cmd_vqe(p,
					DBMDX_VQE_SET_SWITCH_TO_BOOT_CMD,
					NULL);
			else if (p->active_fw == DBMDX_FW_VA)
				ret = send_spi_cmd_va(p,
					DBMDX_VA_SWITCH_TO_BOOT,
					NULL);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to send 'Switch to BOOT' cmd\n",
					 __func__);
				continue;
			}
		}

		if (!load_fw)
			break;
#if 0
		/* Sleep is needed here to ensure that chip is ready */
		msleep(DBMDX_MSLEEP_SPI_D2_AFTER_SBL);
#endif

		/* send firmware */
		send_bytes = send_spi_data(p, fw_data, fw_size - 4);
		if (send_bytes != fw_size - 4) {
			dev_err(p->dev,
				"%s: -----------> load firmware error\n",
				__func__);
			continue;
		}
#if 0
		msleep(DBMDX_MSLEEP_SPI_D2_BEFORE_FW_CHECKSUM);
#endif

#if DBMD2_VERIFY_BOOT_CHECKSUM
		/* verify checksum */
		if (checksum) {
			ret = spi_verify_boot_checksum(p, checksum, chksum_len);
			if (ret < 0) {
				dev_err(spi_p->dev,
					"%s: could not verify checksum\n",
					__func__);
				continue;
			}
		}
#endif
		dev_dbg(p->dev, "%s: ---------> firmware loaded\n",
			__func__);
		break;
	}

	/* no retries left, failed to boot */
	if (retry < 0) {
		dev_err(p->dev, "%s: failed to load firmware\n", __func__);
		return -1;
	}

	/* send boot command */
	ret = send_spi_cmd_boot(p, DBMDX_FIRMWARE_BOOT);
	if (ret < 0) {
		dev_err(p->dev, "%s: booting the firmware failed\n", __func__);
		return -1;
	}

	ret = spi_set_speed(p, DBMDX_VA_SPEED_NORMAL);
	if (ret < 0)
		dev_err(spi_p->dev, "%s:failed %x\n", __func__, ret);

	/* wait some time */
	usleep_range(DBMDX_USLEEP_SPI_D2_AFTER_BOOT,
		DBMDX_USLEEP_SPI_D2_AFTER_BOOT + 1000);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dbmdx_spi_suspend(struct device *dev)
{
	struct chip_interface *ci = spi_get_drvdata(to_spi_device(dev));
	struct dbmdx_spi_private *spi_p = (struct dbmdx_spi_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);

	spi_interface_suspend(spi_p);

	return 0;
}

static int dbmdx_spi_resume(struct device *dev)
{
	struct chip_interface *ci = spi_get_drvdata(to_spi_device(dev));
	struct dbmdx_spi_private *spi_p = (struct dbmdx_spi_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);
	spi_interface_resume(spi_p);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_RUNTIME
static int dbmdx_spi_runtime_suspend(struct device *dev)
{
	struct chip_interface *ci = spi_get_drvdata(to_spi_device(dev));
	struct dbmdx_spi_private *spi_p = (struct dbmdx_spi_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);

	spi_interface_suspend(spi_p);

	return 0;
}

static int dbmdx_spi_runtime_resume(struct device *dev)
{
	struct chip_interface *ci = spi_get_drvdata(to_spi_device(dev));
	struct dbmdx_spi_private *spi_p = (struct dbmdx_spi_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);
	spi_interface_resume(spi_p);

	return 0;
}

#endif /* CONFIG_PM_RUNTIME */

static const struct dev_pm_ops dbmdx_spi_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(dbmdx_spi_suspend, dbmdx_spi_resume)
	SET_RUNTIME_PM_OPS(dbmdx_spi_runtime_suspend,
			   dbmdx_spi_runtime_resume, NULL)
};


static int dbmd2_spi_probe(struct spi_device *client)
{
	int rc;
	struct dbmdx_spi_private *p;
	struct chip_interface *ci;

	rc = spi_common_probe(client);

	if (rc < 0)
		return rc;

	ci = spi_get_drvdata(client);
	p = (struct dbmdx_spi_private *)ci->pdata;

	/* fill in chip interface functions */
	p->chip.boot = dbmd2_spi_boot;

	return rc;
}


static const struct of_device_id dbmd2_spi_of_match[] = {
	{ .compatible = "dspg,dbmd2-spi", },
	{},
};
MODULE_DEVICE_TABLE(of, dbmd2_spi_of_match);

static const struct spi_device_id dbmd2_spi_id[] = {
	{ "dbmdx-spi", 0 },
	{ "dbmd2-spi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, dbmd2_spi_id);

static struct spi_driver dbmd2_spi_driver = {
	.driver = {
		.name = "dbmd2-spi",
		.bus	= &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = dbmd2_spi_of_match,
		.pm = &dbmdx_spi_pm,
	},
	.probe =    dbmd2_spi_probe,
	.remove =   spi_common_remove,
	.id_table = dbmd2_spi_id,
};

static int __init dbmd2_modinit(void)
{
	return spi_register_driver(&dbmd2_spi_driver);
}

module_init(dbmd2_modinit);

static void __exit dbmd2_exit(void)
{
	spi_unregister_driver(&dbmd2_spi_driver);
}
module_exit(dbmd2_exit);

MODULE_DESCRIPTION("DSPG DBMD2 spi interface driver");
MODULE_LICENSE("GPL");
