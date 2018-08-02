/* linux/drivers/video/exynos_decon/panel/ddi_spi.c
 *
 * Header file for Samsung spi driver for panel.
 *
 * Copyright (c) 2013 Samsung Electronics
 * JiHoon Kim <jihoonn.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */


#define DRIVER_NAME "ddi_spi_driver"

struct spi_device *ddi_spi;
static int ddi_spi_exit_flag;
struct workqueue_struct *ddi_spi_workq;
struct delayed_work	fw_dl;


//static u8 wdata_buf[32] __cacheline_aligned;
static u16 rdata_buf[60] __cacheline_aligned;
int rx[10];

u8 tx_cmd[] = {0xF0, 0x2E, 0xAD};
u8 rx_cmd[] = {0xB2};

int ddi_spi_sck;
int ddi_spi_miso;
int ddi_spi_mosi;
int ddi_spi_cs;

static DEFINE_MUTEX(ddi_spi_lock);


/////////
/*
static int ddi_spi_write_byte(struct spi_device *spi, int addr, int data)
{
	u16 buf[1];
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len		= 2,
		.tx_buf		= buf,
	};
	pr_info("ddi_spi_write_byte\n");

	buf[0] = (addr << 8) | data;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	return spi_sync(spi, &msg);
}
*/
#if 0
static int ddi_spi_write(struct spi_device *spi, u8 *data)
{
	u16 buf[3];
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len		= 6,
		.tx_buf		= buf,
	};
	pr_info("ddi_spi_write_byte\n");

	buf[0] = (0x00 << 8) | data[0];
	buf[1] = (0x01 << 8) | data[1];
	buf[2] = (0x01 << 8) | data[2];

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	return spi_sync(spi, &msg);
}

static int ddi_spi_read(struct spi_device *spi, int addr, int data, u16 *rbuf)
{
	u16 buf[1];
	struct spi_message msg;

	struct spi_transfer xfer_tx = {
		.len		= 2,
		.tx_buf		= buf,

	};
	struct spi_transfer xfer_rx = {
		.len		= 24,
		.tx_buf		= rbuf,

	};
	pr_info("ddi_spi_read\n");

	buf[0] = (addr << 8) | data;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer_tx, &msg);
	spi_message_add_tail(&xfer_rx, &msg);

	return spi_sync(spi, &msg);
}
#endif
static int ddi_spi_write_read(struct spi_device *spi, int addr, u8 *tdata, int rdata, u16 *rbuf)
{
	u16 buf[1];
	u16 tbuf[3];
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len		= 6,
		.tx_buf		= tbuf,
	};
	struct spi_transfer xfer_tx = {
		.len		= 2,
		.tx_buf		= buf,

	};
	struct spi_transfer xfer_rx = {
		.len		= 24,
		.rx_buf		= rbuf,

	};
	pr_info("ddi_spi_write_read\n");

	tbuf[0] = (0x00 << 8) | tdata[0];
	tbuf[1] = (0x01 << 8) | tdata[1];
	tbuf[2] = (0x01 << 8) | tdata[2];

	buf[0] = (addr << 8) | rdata;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	spi_message_add_tail(&xfer_tx, &msg);
	spi_message_add_tail(&xfer_rx, &msg);

	return spi_sync(spi, &msg);
}

#if 0
static int ddi_spi_write(struct spi_device *spi, unsigned char address,
	unsigned char command)
{
	int ret = 0;

	if (address != DATA_ONLY)
		ret = ddi_spi_write_byte(spi, 0x0, address);
	if (command != COMMAND_ONLY)
		ret = ddi_spi_write_byte(spi, 0x1, command);

	return ret;
}
#endif

int* ddi_spi_read_opr(void)
{
	int	status;
	int	i;

	status = ddi_spi_write_read(ddi_spi, 0x0, tx_cmd, rx_cmd[0], rdata_buf);

	for(i=0;i<10;i++)
		rx[i] = rdata_buf[i] & 0xff;

	pr_info("ddi_spi_read_opr(%d) : 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		status, rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7], rx[8], rx[9]);
    return rx;
}

void ddi_spi_read_thread(struct work_struct *work)
{
	if(ddi_spi_exit_flag)
		return;
	ddi_spi_read_opr();
	queue_delayed_work(ddi_spi_workq, &fw_dl, msecs_to_jiffies(1000));
}

void ddi_spi_read_opr_start(void)
{
	ddi_spi_exit_flag = 0;

	if(ddi_spi_workq==NULL)
	{
	printk("ddi_spi_read_opr_start\n");
//		ddi_spi_workq =	create_singlethread_workqueue("ddi_spi_workqueue");
//		INIT_DELAYED_WORK(&fw_dl, ddi_spi_read_thread);
	}
	queue_delayed_work(ddi_spi_workq, &fw_dl, msecs_to_jiffies(1000));

	return;
}

void ddi_spi_read_opr_end(void)
{
	ddi_spi_exit_flag = 1;
//	if(ddi_spi_workq)
//	{
//		destroy_workqueue(ddi_spi_workq);
//		ddi_spi_workq = NULL;
//	}
	return;
}


//////////
static int ddi_spi_gpio_probe_dt(struct spi_device *spi)
{
	int ret;
	return 0;

	ret = of_get_named_gpio(spi->dev.of_node, "ddi-gpio-sck", 0);
	if (ret < 0) {
		pr_info("%s : gpio-sck property not found\n",__func__);
	}
	ddi_spi_sck = ret;

	ret = of_get_named_gpio(spi->dev.of_node, "ddi-gpio-miso", 0);
	if (ret < 0) {
		pr_info( "%s : gpio-miso property not found\n",__func__);
	} else
		ddi_spi_miso = ret;

	ret = of_get_named_gpio(spi->dev.of_node, "ddi-gpio-mosi", 0);
	if (ret < 0) {
		pr_info("%s : gpio-mosi property not found\n",__func__);
	} else
		ddi_spi_mosi = ret;

	ret = of_get_named_gpio(spi->dev.of_node, "ddi-cs-gpios", 0);
	if (ret < 0) {
		pr_info("%s : gpio-mosi property not found\n",__func__);
	} else
		ddi_spi_cs = ret;

	pr_info("%s : sck %d\n",__func__, ddi_spi_sck);
	pr_info("%s : miso %d\n",__func__, ddi_spi_miso);
	pr_info("%s : mosi %d\n",__func__, ddi_spi_mosi);
	pr_info("%s : cs %d\n",__func__, ddi_spi_cs);
	return 0;
}

static int ddi_spi_probe(struct spi_device *spi)
{
	s32 ret;

	pr_info("ddi_spi_probe spi : 0x%p\n", spi);

	if (spi == NULL) {
		pr_err("ddi_spi_probe spi == NULL\n");
		return -1;
	}
	ddi_spi_gpio_probe_dt(spi);

	spi->max_speed_hz = 1200000; /* 52000000 */
	spi->bits_per_word = 9;
	spi->mode =  SPI_MODE_0;

	ret = spi_setup(spi);

	if (ret < 0) {
		pr_err("fc8300_spi_probe ERROR ret =%d\n", ret);
		return ret;
	}

	ddi_spi = spi;
//	ddi_spi_workq = NULL;

	ddi_spi_workq = create_singlethread_workqueue("ddi_spi_workqueue");
	INIT_DELAYED_WORK(&fw_dl, ddi_spi_read_thread);

	return ret;
}

static int ddi_spi_remove(struct spi_device *spi)
{
	pr_info("ddi_spi_remove\n");
	return 0;
}

static const struct of_device_id ddi_spi_match_table[] = {
	{   .compatible = "ddi_spi",},
	{}
};

static struct spi_driver ddi_spi_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = ddi_spi_match_table,
	},
	.probe		= ddi_spi_probe,
	.remove		= ddi_spi_remove,
};


static int __init ddi_spi_init(void)
{
	int ret = 0;

	pr_info("ddi_spi_init\n");

	ret = spi_register_driver(&ddi_spi_driver);

	if (ret) {
		pr_err("fc8300_spi register fail : %d\n", ret);
		return ret;
	}

	return ret;
}
subsys_initcall(ddi_spi_init);


static void __exit ddi_spi_exit(void)
{
	spi_unregister_driver(&ddi_spi_driver);
}
 module_exit(ddi_spi_exit);

MODULE_DESCRIPTION("SPI driver for ddi");
MODULE_LICENSE("GPL");
