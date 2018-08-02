/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef _ET320_LINUX_DIRVER_H_
#define _ET320_LINUX_DIRVER_H_

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#define FEATURE_SPI_WAKELOCK
#endif /* CONFIG_SEC_FACTORY */

#include <linux/module.h>
#include <linux/spi/spi.h>

#include <linux/platform_data/spi-s3c64xx.h>
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#include <linux/wakelock.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl330.h>
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
#if defined(CONFIG_SOC_EXYNOS8890)
#include <soc/samsung/secos_booster.h>
#else
#include <mach/secos_booster.h>
#endif
#endif

struct sec_spi_info {
	int		port;
	unsigned long	speed;
};
#endif

/*#define ET320_SPI_DEBUG*/

#ifdef ET320_SPI_DEBUG
#define DEBUG_PRINT(fmt, args...) pr_err(fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

#define VENDOR						"EGISTEC"
#define CHIP_ID						"ET320"

/* assigned */
#define ET320_MAJOR					153
/* ... up to 256 */
#define N_SPI_MINORS					32

#define ET320_ADDRESS_0					0x00
#define ET320_WRITE_ADDRESS				0xAC
#define ET320_READ_DATA					0xAF
#define ET320_WRITE_DATA				0xAE
#define FP_EEPROM_WREN_OP				0x06
#define FP_EEPROM_WRDI_OP				0x04
#define FP_EEPROM_RDSR_OP				0x05
#define FP_EEPROM_WRSR_OP				0x01
#define FP_EEPROM_READ_OP				0x03
#define FP_EEPROM_WRITE_OP				0x02
#define BITS_PER_WORD					8

#define SLOW_BAUD_RATE					16000000

#define DRDY_IRQ_ENABLE					1
#define DRDY_IRQ_DISABLE				0

#define ET320_INT_DETECTION_PERIOD			10
#define ET320_DETECTION_THRESHOLD			10

#define FP_REGISTER_READ				0x01
#define FP_REGISTER_WRITE				0x02
#define FP_GET_ONE_IMG					0x03
#define FP_SENSOR_RESET					0x04
#define FP_POWER_CONTROL				0x05
#define FP_SET_SPI_CLOCK				0x06
#define FP_RESET_SET					0x07

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#define FP_DIABLE_SPI_CLOCK				0x10
#define FP_CPU_SPEEDUP					0x11
#define FP_SET_SENSOR_TYPE				0x14
/* Do not use ioctl number 0x15 */
#define FP_SET_LOCKSCREEN				0x16
#define FP_SET_WAKE_UP_SIGNAL				0x17
#endif

#define FP_EEPROM_WREN					0x90
#define FP_EEPROM_WRDI					0x91
#define FP_EEPROM_RDSR					0x92
#define FP_EEPROM_WRSR					0x93
#define FP_EEPROM_READ					0x94
#define FP_EEPROM_WRITE					0x95

/* trigger signal initial routine */
#define INT_TRIGGER_INIT				0xa4
/* trigger signal close routine */
#define INT_TRIGGER_CLOSE				0xa5
/* read trigger status */
#define INT_TRIGGER_READ				0xa6
/* polling trigger status */
#define INT_TRIGGER_POLLING				0xa7
/* polling abort */
#define INT_TRIGGER_ABORT				0xa8
/* Sensor Registers */
#define FDATA_ET320_ADDR				0x00
#define FSTATUS_ET320_ADDR				0x01
/* Detect Define */
#define FRAME_READY_MASK				0x01

#define SHIFT_BYTE_OF_IMAGE 3
#define DIVISION_OF_IMAGE 4

struct egis_ioc_transfer {
	u8 *tx_buf;
	u8 *rx_buf;
	u32 len;
	u32 speed_hz;
	u16 delay_usecs;
	u8 bits_per_word;
	u8 cs_change;
	u8 opcode;
	u8 pad[3];
};

/*
 *	If platform is 32bit and kernel is 64bit
 *	We will alloc egis_ioc_transfer for 64bit and 32bit
 *	We use ioc_32(32bit) to get data from user mode.
 *	Then copy the ioc_32 to ioc(64bit).
 */
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
struct egis_ioc_transfer_32 {
	u32 tx_buf;
	u32 rx_buf;
	u32 len;
	u32 speed_hz;
	u16 delay_usecs;
	u8 bits_per_word;
	u8 cs_change;
	u8 opcode;
	u8 pad[3];
};
#endif

#define EGIS_IOC_MAGIC			'k'
#define EGIS_MSGSIZE(N) \
	((((N)*(sizeof(struct egis_ioc_transfer))) < (1 << _IOC_SIZEBITS)) \
		? ((N)*(sizeof(struct egis_ioc_transfer))) : 0)
#define EGIS_IOC_MESSAGE(N) _IOW(EGIS_IOC_MAGIC, 0, char[EGIS_MSGSIZE(N)])

struct etspi_data {
	dev_t devt;
	spinlock_t spi_lock;
	struct spi_device *spi;
	struct list_head device_entry;

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex buf_lock;
	unsigned users;
	u8 *buffer;
	unsigned int ocp_en;	/* ocp enable GPIO pin number */
	unsigned int drdyPin;	/* DRDY GPIO pin number */
	unsigned int sleepPin;	/* Sleep GPIO pin number */
	unsigned int ldo_pin;	/* Ldo GPIO pin number */
	unsigned int ldo_pin2;	/* Ldo2 GPIO pin number */
#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
	/* set cs pin in fp driver, use only Exynos8890 */
	/* for use auto cs mode with dualization fp sensor */
	unsigned int cs_gpio;
#endif
#endif
	unsigned int spi_cs;	/* spi cs pin <temporary gpio setting> */

	unsigned int drdy_irq_flag;	/* irq flag */
	bool ldo_onoff;

	/* For polling interrupt */
	int int_count;
	struct timer_list timer;
	struct work_struct work_debug;
	struct workqueue_struct *wq_dbg;
	struct timer_list dbg_timer;
	int sensortype;
#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
	struct device *fp_device;
#endif
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	bool enabled_clk;
#ifdef FEATURE_SPI_WAKELOCK
	struct wake_lock fp_spi_lock;
#endif
	struct task_struct *t;
	int user_pid;
	int signal_id;
#endif
	bool tz_mode;
	int detect_period;
	int detect_threshold;
	bool finger_on;
};

#ifdef ENABLE_SENSORS_FPRINT_SECURE
/*
 * Used by IOCTL command:
 *         ETSPI_IOCTL_REGISTER_LOCK_SCREEN_WAKE_UP_SIGNAL
 *
 * @user_pid:Process ID to which SPI driver sends signal indicating that LOCK SCREEN
 *			is asserted
 * @signal_id:signal_id
*/
struct etspi_ioctl_register_signal {
	int user_pid;
	int signal_id;
};
#endif

int etspi_io_read_register(struct etspi_data *etspi, u8 *addr, u8 *buf);
int etspi_io_write_register(struct etspi_data *etspi, u8 *buf);
int etspi_io_get_one_image(struct etspi_data *etspi, u8 *buf, u8 *image_buf);
int etspi_read_register(struct etspi_data *etspi, u8 addr, u8 *buf);
int etspi_mass_read(struct etspi_data *etspi, u8 addr, u8 *buf, int read_len);
int etspi_eeprom_wren(struct etspi_data *etspi);
int etspi_eeprom_wrdi(struct etspi_data *etspi);
int etspi_eeprom_rdsr(struct etspi_data *etspi, u8 *buf);
int etspi_eeprom_wrsr(struct etspi_data *etspi, u8 *buf);
int etspi_eeprom_read(struct etspi_data *etspi,
		u8 *addr, u8 *buf, int read_len);
int etspi_eeprom_write(struct etspi_data *etspi, u8 *buf, int write_len);

#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
extern int fingerprint_register(struct device *dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
extern void fingerprint_unregister(struct device *dev,
	struct device_attribute *attributes[]);
#endif

#endif
