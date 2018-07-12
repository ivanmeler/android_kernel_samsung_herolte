/*! @file vfsSpiDrv.h
*******************************************************************************
**  SPI Driver Interface Functions
**
**  This file contains the SPI driver interface functions.
**
**  Copyright (C) 2011-2013 Validity Sensors, Inc.
**  This program is free software; you can redistribute it and/or
**  modify it under the terms of the GNU General Public License
**  as published by the Free Software Foundation; either version 2
**  of the License, or (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 51 Franklin Street,
**  Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef VFS7XXX_H_
#define VFS7XXX_H_

#if defined(CONFIG_SOC_EXYNOS8890)
/* exynos8890 evt0 board don't support to SPI clock rate 13MHz under */
#define SLOW_BAUD_RATE      13000000
#define MAX_BAUD_RATE       13000000
#else
#define SLOW_BAUD_RATE      4800000
#define MAX_BAUD_RATE       9600000
#endif
#define BAUD_RATE_COEF      1000
#define DRDY_TIMEOUT_MS     40
#define DRDY_ACTIVE_STATUS  1
#define BITS_PER_WORD       8
#define DRDY_IRQ_FLAG       IRQF_TRIGGER_RISING
#define DEFAULT_BUFFER_SIZE (4096 * 6)
#define DRDY_IRQ_ENABLE			1
#define DRDY_IRQ_DISABLE		0

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#define FEATURE_SPI_WAKELOCK
#endif /* CONFIG_SEC_FACTORY */

/* IOCTL commands definitions */

/*
 * Magic number of IOCTL command
 */
#define VFSSPI_IOCTL_MAGIC    'k'

#ifndef ENABLE_SENSORS_FPRINT_SECURE
/*
 * Transmit data to the device and retrieve data from it simultaneously
 */
#define VFSSPI_IOCTL_RW_SPI_MESSAGE         _IOWR(VFSSPI_IOCTL_MAGIC,	\
							 1, unsigned int)
#endif

/*
 * Hard reset the device
 */
#define VFSSPI_IOCTL_DEVICE_RESET           _IO(VFSSPI_IOCTL_MAGIC,   2)
/*
 * Set the baud rate of SPI master clock
 */
#define VFSSPI_IOCTL_SET_CLK                _IOW(VFSSPI_IOCTL_MAGIC,	\
							 3, unsigned int)
#ifndef ENABLE_SENSORS_FPRINT_SECURE
/*
 * Get level state of DRDY GPIO
 */
#define VFSSPI_IOCTL_CHECK_DRDY             _IO(VFSSPI_IOCTL_MAGIC,   4)
#endif

/*
 * Register DRDY signal. It is used by SPI driver for indicating host that
 * DRDY signal is asserted.
 */
#define VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL   _IOW(VFSSPI_IOCTL_MAGIC,	\
							 5, unsigned int)
/*
 * Store the user data into the SPI driver. Currently user data is a
 * device info data, which is obtained from announce packet.

#define VFSSPI_IOCTL_SET_USER_DATA          _IOW(VFSSPI_IOCTL_MAGIC,	\
							 6, unsigned int)
*/

/*
 * Retrieve user data from the SPI driver

#define VFSSPI_IOCTL_GET_USER_DATA          _IOWR(VFSSPI_IOCTL_MAGIC,	\
							 7, unsigned int)
*/

/*
 * Enable/disable DRDY interrupt handling in the SPI driver
 */
#define VFSSPI_IOCTL_SET_DRDY_INT           _IOW(VFSSPI_IOCTL_MAGIC,	\
							8, unsigned int)
/*
 * Put device in suspend state
 */
#define VFSSPI_IOCTL_DEVICE_SUSPEND         _IO(VFSSPI_IOCTL_MAGIC,   9)

#ifndef ENABLE_SENSORS_FPRINT_SECURE
/*
 * Indicate the fingerprint buffer size for read
 */
#define VFSSPI_IOCTL_STREAM_READ_START      _IOW(VFSSPI_IOCTL_MAGIC,	\
						 10, unsigned int)
/*
 * Indicate that fingerprint acquisition is completed
 */
#define VFSSPI_IOCTL_STREAM_READ_STOP       _IO(VFSSPI_IOCTL_MAGIC,   11)
#endif
/* Turn on the power to the sensor */
#define VFSSPI_IOCTL_POWER_ON               _IO(VFSSPI_IOCTL_MAGIC,   13)
/* Turn off the power to the sensor */
#define VFSSPI_IOCTL_POWER_OFF              _IO(VFSSPI_IOCTL_MAGIC,   14)
#ifdef ENABLE_SENSORS_FPRINT_SECURE
/* To disable spi core clock */
#define VFSSPI_IOCTL_DISABLE_SPI_CLOCK     _IO(VFSSPI_IOCTL_MAGIC,    15)
/* To set SPI configurations like gpio, clks */
#define VFSSPI_IOCTL_SET_SPI_CONFIGURATION _IO(VFSSPI_IOCTL_MAGIC,    16)
/* To reset SPI configurations */
#define VFSSPI_IOCTL_RESET_SPI_CONFIGURATION _IO(VFSSPI_IOCTL_MAGIC,  17)
/* To switch core */
#define VFSSPI_IOCTL_CPU_SPEEDUP     _IOW(VFSSPI_IOCTL_MAGIC,	\
						19, unsigned int)
#define VFSSPI_IOCTL_SET_SENSOR_TYPE     _IOW(VFSSPI_IOCTL_MAGIC,	\
							20, unsigned int)
/* IOCTL #21 was already used Synaptics service. Do not use #21 */
#define VFSSPI_IOCTL_SET_LOCKSCREEN     _IOW(VFSSPI_IOCTL_MAGIC,	\
							22, unsigned int)
#endif
/* To control the power */
#define VFSSPI_IOCTL_POWER_CONTROL     _IOW(VFSSPI_IOCTL_MAGIC,	\
							23, unsigned int)
/* get sensor orienation from the SPI driver*/
#define VFSSPI_IOCTL_GET_SENSOR_ORIENT	\
	_IOR(VFSSPI_IOCTL_MAGIC, 18, unsigned int)

#ifndef ENABLE_SENSORS_FPRINT_SECURE
/*
 * Used by IOCTL command:
 *         VFSSPI_IOCTL_RW_SPI_MESSAGE
 *
 * @rx_buffer:pointer to retrieved data
 * @tx_buffer:pointer to transmitted data
 * @len:transmitted/retrieved data size
 */
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
/*
* Platform supports only 32 bits.
*/
struct vfsspi_ioctl_transfer {
	u32 rx_buffer;
	u32 tx_buffer;
	u32 len;
};
#else
 struct vfsspi_ioctl_transfer {
	unsigned char *rx_buffer;
	unsigned char *tx_buffer;
	unsigned int len;
};
#endif

/* used for WoG mode */
extern void vfsspi_fp_homekey_ev(void);
/* export variable for signaling */
EXPORT_SYMBOL(vfsspi_fp_homekey_ev);
extern int vfsspi_goto_suspend;
EXPORT_SYMBOL(vfsspi_goto_suspend);
#endif

/*
 * Used by IOCTL command:
 *         VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL
 *
 * @user_pid:Process ID to which SPI driver sends signal indicating that DRDY
 *			is asserted
 * @signal_id:signal_id
*/
struct vfsspi_ioctl_register_signal {
	int user_pid;
	int signal_id;
};

#endif /* VFS7XXX_H_ */
