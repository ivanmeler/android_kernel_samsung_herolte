/*
 * Copyright (C) 2012-2014 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#define DEFAULT_BUFFER_SIZE 258

#define P61_MAGIC 0xEB
#define P61_SET_PWR _IOW(P61_MAGIC, 0x01, unsigned long)
#define P61_SET_DBG _IOW(P61_MAGIC, 0x02, unsigned long)
#define P61_SET_POLL _IOW(P61_MAGIC, 0x03, unsigned long)

/* To set SPI configurations like gpio, clks */
#define P61_SET_SPI_CONFIG _IO(P61_MAGIC, 0x04)
/* Set the baud rate of SPI master clock nonTZ */
#define P61_ENABLE_SPI_CLK _IO(P61_MAGIC, 0x05)
/* To disable spi core clock */
#define P61_DISABLE_SPI_CLK _IO(P61_MAGIC, 0x06)
/* only nonTZ +++++*/
/* Transmit data to the device and retrieve data from it simultaneously.*/
#define P61_RW_SPI_DATA _IOWR(P61_MAGIC, 0x07, unsigned long)
/* only nonTZ -----*/

struct p61_ioctl_transfer {
	unsigned char *rx_buffer;
	unsigned char *tx_buffer;
	unsigned int len;
};

struct p61_spi_platform_data {
	unsigned int irq_gpio;
	unsigned int rst_gpio;
};
