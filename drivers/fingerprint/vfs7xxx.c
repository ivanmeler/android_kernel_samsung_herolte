/*! @file vfs7xxx.c
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

#include "fingerprint.h"
#include "vfs7xxx.h"

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <linux/irq.h>

#include <asm-generic/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/jiffies.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/pinctrl/consumer.h>
#include "../pinctrl/core.h"

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
#include <linux/smc.h>

struct sec_spi_info {
	int		port;
	unsigned long	speed;
};
#endif

#ifdef CONFIG_FB
#include <linux/fb.h>
#endif

#define VALIDITY_PART_NAME "validity_fingerprint"
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_mutex);
static struct class *vfsspi_device_class;
static int gpio_irq;
/* for irq enable, disable count */
static int cnt_irq=0;
#ifdef CONFIG_SENSORS_FP_TKEY_NOTI
static int enable_flag_tkey = 0;
#endif

#ifdef CONFIG_OF
static struct of_device_id vfsspi_match_table[] = {
	{ .compatible = "vfsspi,vfs7xxx",},
	{ },
};
#else
#define vfsspi_match_table NULL
#endif

/*
 * vfsspi_devData - The spi driver private structure
 * @devt:Device ID
 * @vfs_spi_lock:The lock for the spi device
 * @spi:The spi device
 * @device_entry:Device entry list
 * @buffer_mutex:The lock for the transfer buffer
 * @is_opened:Indicates that driver is opened
 * @buffer:buffer for transmitting data
 * @null_buffer:buffer for transmitting zeros
 * @stream_buffer:buffer for transmitting data stream
 * @stream_buffer_size:streaming buffer size
 * @drdy_pin:DRDY GPIO pin number
 * @sleep_pin:Sleep GPIO pin number
 * @user_pid:User process ID, to which the kernel signal
 *	indicating DRDY event is to be sent
 * @signal_id:Signal ID which kernel uses to indicating
 *	user mode driver that DRDY is asserted
 * @current_spi_speed:Current baud rate of SPI master clock
 */
struct vfsspi_device_data {
	dev_t devt;
	struct cdev cdev;
	spinlock_t vfs_spi_lock;
	struct spi_device *spi;
	struct list_head device_entry;
	struct mutex buffer_mutex;
	unsigned int is_opened;
	unsigned char *buffer;
	unsigned char *null_buffer;
	unsigned char *stream_buffer;
	size_t stream_buffer_size;
	unsigned int drdy_pin;
	unsigned int sleep_pin;
	struct task_struct *t;
	int user_pid;
	int signal_id;
	unsigned int current_spi_speed;
	atomic_t irq_enabled;
	struct mutex kernel_lock;
	bool ldo_onoff;
	spinlock_t irq_lock;
	unsigned short drdy_irq_flag;
	unsigned int ldocontrol;
	unsigned int ocp_en;
	unsigned int ldo_pin; /* Ldo 3.3V GPIO pin number */
	unsigned int ldo_pin2; /* Ldo 1.8V GPIO pin number */
#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
	/* set cs pin in fp driver, use only Exynos8890 */
	/* for use auto cs mode with dualization fp sensor */
	unsigned int cs_gpio;
#endif
#endif
#ifdef CONFIG_SENSORS_FINGERPRINT_DUALIZATION
	unsigned int vendor_pin; /* For checking vendor */
#endif
	unsigned int retain_pin;
	unsigned int retain_onoff;
	unsigned int retain_delayset;
	struct work_struct work_debug;
	struct workqueue_struct *wq_dbg;
	struct timer_list dbg_timer;
	struct pinctrl *p;
	struct pinctrl_state *pins_sleep;
	struct pinctrl_state *pins_idle;
	bool tz_mode;
#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
	struct device *fp_device;
#endif
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	bool enabled_clk;
#ifdef FEATURE_SPI_WAKELOCK
	struct wake_lock fp_spi_lock;
	struct wake_lock fp_signal_lock;
#endif
#endif
	int sensortype;
	unsigned int orient;
	unsigned int detect_mode;
#ifdef CONFIG_FB
	struct notifier_block fb_notifier;
#endif
};

#ifdef CONFIG_SENSORS_FINGERPRINT_DUALIZATION
int FP_CHECK = 0; /* extern variable init */
#endif

int vfsspi_goto_suspend = 0;
bool fp_lockscreen_mode = false;

#define VENDOR		"SYNAPTICS"
#define CHIP_ID		"VIPER"

static struct vfsspi_device_data *g_data;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int vfsspi_type_check(struct vfsspi_device_data *vfsspi_device);
#endif

#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
extern int fingerprint_register(struct device *dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
extern void fingerprint_unregister(struct device *dev,
	struct device_attribute *attributes[]);
#endif

#ifdef CONFIG_SENSORS_FP_TKEY_NOTI
extern int change_touchkey_thd_for_fingerprint(int on);
#endif

#ifdef CONFIG_FB
static void vfsspi_set_retain_pin(struct vfsspi_device_data *vfsspi_device, int value, int flush){
	if (vfsspi_device->retain_pin) {
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		if(!vfsspi_device->enabled_clk || flush)
			gpio_set_value(vfsspi_device->retain_pin, value);
		vfsspi_device->retain_delayset = value;
#else
		gpio_set_value(vfsspi_device->retain_pin, value);
		vfsspi_device->retain_delayset = value;
#endif
		pr_info("%s pin %d, dset %d flush %d", __func__, gpio_get_value(vfsspi_device->retain_pin),
			vfsspi_device->retain_delayset, flush);
	}
}

static int vfsspi_fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event  *evdata = data;
	int              *blank;

	if (g_data->retain_pin) {
		if (evdata && evdata->data && event == FB_EVENT_BLANK) {
			blank = evdata->data;
			if (*blank == FB_BLANK_UNBLANK){
				vfsspi_set_retain_pin(g_data, 1, 0);
				pr_info("vfsspi: FB_BLANK_UNBLANK\n");
			} else if (*blank == FB_BLANK_POWERDOWN) {
				vfsspi_set_retain_pin(g_data, 0, 0);
				pr_info("vfsspi: FB_BLANK_POWERDOWN\n");
			}
		}
	} else
		pr_err("%s : not set the retain pin!\n", __func__);

	return 0;
}

void vfsspi_fp_homekey_ev(void){
	if (g_data->retain_pin) {
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		if (gpio_get_value(g_data->retain_pin) == 0)
			return;
#endif
		vfsspi_set_retain_pin(g_data, 1, 0);
	} else
		pr_info("%s : not set the retain pin!\n", __func__);
}
#endif

static int vfsspi_send_drdy_signal(struct vfsspi_device_data *vfsspi_device)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (vfsspi_device->t) {
		/* notify DRDY signal to user process */
		ret = send_sig_info(vfsspi_device->signal_id,
				    (struct siginfo *)1, vfsspi_device->t);
		if (ret < 0)
			pr_err("%s Error sending signal\n", __func__);

	} else
		pr_err("%s task_struct is not received yet\n", __func__);

	return ret;
}

/* Return no. of bytes written to device. Negative number for errors */
static inline ssize_t vfsspi_writeSync(struct vfsspi_device_data *vfsspi_device,
					size_t len)
{
	int    status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("%s\n", __func__);

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));
	if (len != (unsigned int)len)
		pr_info("%s vfsspi_writeSync len casting is failed. t.len=%ld, len=%d",
			__func__, len, (unsigned int)len);

	t.rx_buf = vfsspi_device->null_buffer;
	t.tx_buf = vfsspi_device->buffer;
	t.len = (unsigned int)len;
	t.speed_hz = vfsspi_device->current_spi_speed;

	spi_message_add_tail(&t, &m);

	status = spi_sync(vfsspi_device->spi, &m);

	if (status == 0)
		status = m.actual_length;
	pr_debug("%s vfsspi_writeSync,length=%d\n", __func__, m.actual_length);
	return status;
}

/* Return no. of bytes read > 0. negative integer incase of error. */
static inline ssize_t vfsspi_readSync(struct vfsspi_device_data *vfsspi_device,
					size_t len)
{
	int    status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("%s\n", __func__);

	spi_message_init(&m);
	memset(&t, 0x0, sizeof(t));
	if (len != (unsigned int)len)
		pr_info("%s vfsspi_readSync len casting is failed. t.len=%ld, len=%d",
			__func__, len, (unsigned int)len);

	memset(vfsspi_device->null_buffer, 0x0, len);
	t.tx_buf = vfsspi_device->null_buffer;
	t.rx_buf = vfsspi_device->buffer;
	t.len = (unsigned int)len;
	t.speed_hz = vfsspi_device->current_spi_speed;

	spi_message_add_tail(&t, &m);

	status = spi_sync(vfsspi_device->spi, &m);

	if (status == 0)
		status = len;

	pr_debug("%s vfsspi_readSync,length=%ld\n", __func__,  len);

	return status;
}

static ssize_t vfsspi_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *fPos)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	struct vfsspi_device_data *vfsspi_device = NULL;
	ssize_t               status = 0;

	pr_debug("%s\n", __func__);

	if (count > DEFAULT_BUFFER_SIZE || !count)
		return -EMSGSIZE;

	vfsspi_device = filp->private_data;

	mutex_lock(&vfsspi_device->buffer_mutex);

	if (vfsspi_device->buffer) {
		unsigned long missing = 0;

		missing = copy_from_user(vfsspi_device->buffer, buf, count);

		if (missing == 0)
			status = vfsspi_writeSync(vfsspi_device, count);
		else
			status = -EFAULT;
	}

	mutex_unlock(&vfsspi_device->buffer_mutex);

	return status;
#endif
}

static ssize_t vfsspi_read(struct file *filp, char __user *buf,
			size_t count, loff_t *fPos)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	struct vfsspi_device_data *vfsspi_device = NULL;
	ssize_t                status    = 0;

	pr_debug("%s\n", __func__);

	if (count > DEFAULT_BUFFER_SIZE || !count)
		return -EMSGSIZE;
	if (buf == NULL)
		return -EFAULT;


	vfsspi_device = filp->private_data;

	mutex_lock(&vfsspi_device->buffer_mutex);

	status  = vfsspi_readSync(vfsspi_device, count);


	if (status > 0) {
		unsigned long missing = 0;
		/* data read. Copy to user buffer.*/
		missing = copy_to_user(buf, vfsspi_device->buffer, status);

		if (missing == status) {
			pr_err("%s copy_to_user failed\n", __func__);
			/* Nothing was copied to user space buffer. */
			status = -EFAULT;
		} else {
			status = status - missing;
		}
	}

	mutex_unlock(&vfsspi_device->buffer_mutex);

	return status;
#endif
}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int vfsspi_xfer(struct vfsspi_device_data *vfsspi_device,
			struct vfsspi_ioctl_transfer *tr)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
	u64 tx_buffer = (u64)tr->tx_buffer, rx_buffer = (u64)tr->rx_buffer;
#endif

	pr_debug("%s\n", __func__);

	if (vfsspi_device == NULL || tr == NULL)
		return -EFAULT;

	if (tr->len > DEFAULT_BUFFER_SIZE || !tr->len)
		return -EMSGSIZE;

#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
	if ((unsigned char *)tx_buffer != NULL) {
		if (copy_from_user(vfsspi_device->null_buffer,
				(unsigned char *)tx_buffer, tr->len) != 0)
			return -EFAULT;
	}
#else
	if (tr->tx_buffer != NULL) {

		if (copy_from_user(vfsspi_device->null_buffer,
				tr->tx_buffer, tr->len) != 0)
			return -EFAULT;
	}
#endif

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = vfsspi_device->null_buffer;
	t.rx_buf = vfsspi_device->buffer;
	t.len = tr->len;
	t.speed_hz = vfsspi_device->current_spi_speed;

	spi_message_add_tail(&t, &m);

	status = spi_sync(vfsspi_device->spi, &m);

	if (status == 0) {
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
		if ((unsigned char *)rx_buffer != NULL) {
			unsigned long missing = 0;

			missing = copy_to_user((unsigned char *)rx_buffer,
					       vfsspi_device->buffer, tr->len);

			if (missing != 0)
				tr->len = tr->len - missing;
		}
#else
		if (tr->rx_buffer != NULL) {
			unsigned long missing = 0;

			missing = copy_to_user(tr->rx_buffer,
					       vfsspi_device->buffer, tr->len);

			if (missing != 0)
				tr->len = tr->len - missing;
		}
#endif
	}
	pr_debug("%s vfsspi_xfer,length=%d\n", __func__, tr->len);
	return status;

} /* vfsspi_xfer */
#endif

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int vfsspi_rw_spi_message(struct vfsspi_device_data *vfsspi_device,
				 unsigned long arg)
{
	struct vfsspi_ioctl_transfer   *dup = NULL;
	dup = kmalloc(sizeof(struct vfsspi_ioctl_transfer), GFP_KERNEL);
	if (dup == NULL)
		return -ENOMEM;

	if (copy_from_user(dup, (void *)arg,
			sizeof(struct vfsspi_ioctl_transfer)) != 0) {
		kfree(dup);
		return -EFAULT;
	} else {
		int err = vfsspi_xfer(vfsspi_device, dup);
		if (err != 0) {
			kfree(dup);
			return err;
		}
	}
	if (copy_to_user((void *)arg, dup,
			 sizeof(struct vfsspi_ioctl_transfer)) != 0)
		return -EFAULT;
	kfree(dup);
	return 0;
}
#endif

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int vfsspi_sec_spi_prepare(struct sec_spi_info *spi_info,
	struct spi_device *spi)
{
	struct clk *fp_spi_pclk, *fp_spi_sclk;

	fp_spi_pclk = clk_get(NULL, "fp-spi-pclk");

	if (IS_ERR(fp_spi_pclk)) {
		pr_err("Can't get fp_spi_pclk\n");
		return -1;
	}

	fp_spi_sclk = clk_get(NULL, "fp-spi-sclk");

	if (IS_ERR(fp_spi_sclk)) {
		pr_err("Can't get fp_spi_sclk\n");
		return -1;
	}

	clk_prepare_enable(fp_spi_pclk);
	clk_prepare_enable(fp_spi_sclk);
	clk_set_rate(fp_spi_sclk, spi_info->speed * 2);

	clk_put(fp_spi_pclk);
	clk_put(fp_spi_sclk);

	return 0;
}

static int vfsspi_sec_spi_unprepare(struct sec_spi_info *spi_info,
	struct spi_device *spi)
{
	struct clk *fp_spi_pclk, *fp_spi_sclk;

	fp_spi_pclk = clk_get(NULL, "fp-spi-pclk");
	if (IS_ERR(fp_spi_pclk)) {
		pr_err("Can't get fp_spi_pclk\n");
		return -1;
	}

	fp_spi_sclk = clk_get(NULL, "fp-spi-sclk");

	if (IS_ERR(fp_spi_sclk)) {
		pr_err("Can't get fp_spi_sclk\n");
		return -1;
	}

	clk_disable_unprepare(fp_spi_pclk);
	clk_disable_unprepare(fp_spi_sclk);

	clk_put(fp_spi_pclk);
	clk_put(fp_spi_sclk);

	return 0;
}

#if !defined(CONFIG_SOC_EXYNOS8890)
struct amba_device *adev_dma;
static int vfsspi_sec_dma_prepare(struct sec_spi_info *spi_info)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, "arm,pl330")
	{
		if (!of_device_is_available(np))
			continue;

		if (!of_dma_secure_mode(np))
			continue;

		adev_dma = of_find_amba_device_by_node(np);
		pr_info("[%s]device_name:%s\n", __func__, dev_name(&adev_dma->dev));
		break;
	}

	if (adev_dma == NULL)
		return -1;

#ifdef CONFIG_SOC_EXYNOS5430_REV_1
	set_secure_dma();
#endif
	pm_runtime_get_sync(&adev_dma->dev);

	return 0;
}

static int vfsspi_sec_dma_unprepare(void)
{
	if (adev_dma == NULL)
		return -1;

	pm_runtime_put(&adev_dma->dev);

	return 0;
}
#endif
#endif

static void vfsspi_pin_control(struct vfsspi_device_data *vfsspi_device,
	bool pin_set)
{
	int status = 0;
	vfsspi_device->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(vfsspi_device->pins_idle)) {
			status = pinctrl_select_state(vfsspi_device->p,
				vfsspi_device->pins_idle);
			if (status)
				pr_err("%s: can't set pin default state\n",
					__func__);
			pr_debug("%s idle\n", __func__);
		}
	} else {
		if (!IS_ERR(vfsspi_device->pins_sleep)) {
			status = pinctrl_select_state(vfsspi_device->p,
				vfsspi_device->pins_sleep);
			if (status)
				pr_err("%s: can't set pin sleep state\n",
					__func__);
			pr_debug("%s sleep\n", __func__);
		}
	}
}

static int vfsspi_set_clk(struct vfsspi_device_data *vfsspi_device,
			  unsigned long arg)
{
	int ret_val = 0;
	unsigned short clock = 0;
	struct spi_device *spidev = NULL;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	struct sec_spi_info *spi_info = NULL;
#endif

	if (copy_from_user(&clock, (void *)arg,
			   sizeof(unsigned short)) != 0)
		return -EFAULT;

	spin_lock_irq(&vfsspi_device->vfs_spi_lock);

	spidev = spi_dev_get(vfsspi_device->spi);

	spin_unlock_irq(&vfsspi_device->vfs_spi_lock);
	if (spidev != NULL) {
		switch (clock) {
		case 0:	/* Running baud rate. */
			pr_debug("%s Running baud rate.\n", __func__);
			spidev->max_speed_hz = MAX_BAUD_RATE;
			vfsspi_device->current_spi_speed = MAX_BAUD_RATE;
			break;
		case 0xFFFF: /* Slow baud rate */
			pr_debug("%s slow baud rate.\n", __func__);
			spidev->max_speed_hz = SLOW_BAUD_RATE;
			vfsspi_device->current_spi_speed = SLOW_BAUD_RATE;
			break;
		default:
			pr_debug("%s baud rate is %d.\n", __func__, clock);
#ifdef CONFIG_SOC_EXYNOS8890
			/* exynos8890 evt0 board don't support to SPI clock rate 13MHz under */
			/* vfsspi_device->current_spi_speed = clock * BAUD_RATE_COEF; */
			vfsspi_device->current_spi_speed = MAX_BAUD_RATE;
#else
			vfsspi_device->current_spi_speed =
				clock * BAUD_RATE_COEF;
#endif
			if (vfsspi_device->current_spi_speed > MAX_BAUD_RATE)
				vfsspi_device->current_spi_speed =
					MAX_BAUD_RATE;
			spidev->max_speed_hz = vfsspi_device->current_spi_speed;
			break;
		}

		pr_info("%s, clk speed: %d\n", __func__,
			vfsspi_device->current_spi_speed);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		if (!vfsspi_device->enabled_clk) {
			spi_info = kmalloc(sizeof(struct sec_spi_info),
				GFP_KERNEL);
			if (spi_info != NULL) {
				pr_info("%s ENABLE_SPI_CLOCK\n", __func__);

				spi_info->speed = spidev->max_speed_hz;
				ret_val = vfsspi_sec_spi_prepare(spi_info, spidev);
				if (ret_val < 0)
					pr_err("%s: Unable to enable spi clk\n",
						__func__);
#if !defined(CONFIG_SOC_EXYNOS8890)
				ret_val = vfsspi_sec_dma_prepare(spi_info);
				if (ret_val < 0)
					pr_err("%s: Unable to enable spi dma\n",
						__func__);
#endif
				kfree(spi_info);
#ifdef FEATURE_SPI_WAKELOCK
				wake_lock(&vfsspi_device->fp_spi_lock);
#endif
				vfsspi_device->enabled_clk = true;
			} else
				ret_val = -ENOMEM;
		}
#endif
		spi_dev_put(spidev);
	}
	return ret_val;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int vfsspi_ioctl_disable_spi_clock(
	struct vfsspi_device_data *vfsspi_device)
{
	struct spi_device *spidev = NULL;
	int ret_val = 0;
	struct sec_spi_info *spi_info = NULL;

	if (vfsspi_device->enabled_clk) {
		pr_info("%s DISABLE_SPI_CLOCK\n", __func__);
		spin_lock_irq(&vfsspi_device->vfs_spi_lock);
		spidev = spi_dev_get(vfsspi_device->spi);
		spin_unlock_irq(&vfsspi_device->vfs_spi_lock);

		ret_val = vfsspi_sec_spi_unprepare(spi_info, spidev);
		if (ret_val < 0)
			pr_err("%s: couldn't disable spi clks\n", __func__);
#if !defined(CONFIG_SOC_EXYNOS8890)
		ret_val = vfsspi_sec_dma_unprepare();
		if (ret_val < 0)
			pr_err("%s: couldn't disable spi dma\n", __func__);
#endif
		spi_dev_put(spidev);
#ifdef FEATURE_SPI_WAKELOCK
		wake_unlock(&vfsspi_device->fp_spi_lock);
#endif
		vfsspi_device->enabled_clk = false;
		if (vfsspi_device->retain_pin)
			vfsspi_set_retain_pin(vfsspi_device, vfsspi_device->retain_delayset, 1);
	}
	return ret_val;
}
#endif

static int vfsspi_register_drdy_signal(struct vfsspi_device_data *vfsspi_device,
				       unsigned long arg)
{
	struct vfsspi_ioctl_register_signal usr_signal;
	if (copy_from_user(&usr_signal, (void *)arg, sizeof(usr_signal)) != 0) {
		pr_err("%s Failed copy from user.\n", __func__);
		return -EFAULT;
	} else {
		vfsspi_device->user_pid = usr_signal.user_pid;
		vfsspi_device->signal_id = usr_signal.signal_id;
		rcu_read_lock();
		/* find the task_struct associated with userpid */
		vfsspi_device->t = pid_task(find_pid_ns(vfsspi_device->user_pid, &init_pid_ns),
			     PIDTYPE_PID);
		if (vfsspi_device->t == NULL) {
			pr_debug("%s No such pid\n", __func__);
			rcu_read_unlock();
			return -ENODEV;
		}
		rcu_read_unlock();
		pr_info("%s Searching task with PID=%08x, t = %p\n",
			__func__, vfsspi_device->user_pid, vfsspi_device->t);
	}
	return 0;
}

static int vfsspi_enableIrq(struct vfsspi_device_data *vfsspi_device)
{
	pr_info("%s\n", __func__);
	vfsspi_set_retain_pin(vfsspi_device, vfsspi_device->retain_delayset, 1);
	spin_lock_irq(&vfsspi_device->irq_lock);
	if (atomic_read(&vfsspi_device->irq_enabled)
		== DRDY_IRQ_ENABLE) {
		spin_unlock_irq(&vfsspi_device->irq_lock);
		pr_err("%s DRDY irq already enabled\n", __func__);
		return -EINVAL;
	}
	vfsspi_pin_control(vfsspi_device, true);
	enable_irq(gpio_irq);
	atomic_set(&vfsspi_device->irq_enabled, DRDY_IRQ_ENABLE);
	cnt_irq++;
	spin_unlock_irq(&vfsspi_device->irq_lock);
	return 0;
}

static int vfsspi_disableIrq(struct vfsspi_device_data *vfsspi_device)
{
	pr_info("%s\n", __func__);
	spin_lock_irq(&vfsspi_device->irq_lock);
	if (atomic_read(&vfsspi_device->irq_enabled)
		== DRDY_IRQ_DISABLE) {
		spin_unlock_irq(&vfsspi_device->irq_lock);
		pr_err("%s DRDY irq already disabled\n", __func__);
		return -EINVAL;
	}
	disable_irq_nosync(gpio_irq);
	atomic_set(&vfsspi_device->irq_enabled, DRDY_IRQ_DISABLE);
	vfsspi_pin_control(vfsspi_device, false);
	cnt_irq--;
	spin_unlock_irq(&vfsspi_device->irq_lock);
	return 0;
}

static irqreturn_t vfsspi_irq(int irq, void *context)
{
	struct vfsspi_device_data *vfsspi_device = context;

	/* Linux kernel is designed so that when you disable
	an edge-triggered interrupt, and the edge happens while
	the interrupt is disabled, the system will re-play the
	interrupt at enable time.
	Therefore, we are checking DRDY GPIO pin state to make sure
	if the interrupt handler has been called actually by DRDY
	interrupt and it's not a previous interrupt re-play */
	if (gpio_get_value(vfsspi_device->drdy_pin) == DRDY_ACTIVE_STATUS) {
		spin_lock(&vfsspi_device->irq_lock);
		if (atomic_read(&vfsspi_device->irq_enabled) == DRDY_IRQ_ENABLE) {
			disable_irq_nosync(gpio_irq);
			atomic_set(&vfsspi_device->irq_enabled, DRDY_IRQ_DISABLE);
			vfsspi_pin_control(vfsspi_device, false);
			cnt_irq--;
			spin_unlock(&vfsspi_device->irq_lock);
			vfsspi_send_drdy_signal(vfsspi_device);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#ifdef FEATURE_SPI_WAKELOCK
			wake_lock_timeout(&vfsspi_device->fp_signal_lock, 3 * HZ);
#endif
#endif
			pr_info("%s disableIrq\n", __func__);
		}
		else {
			spin_unlock(&vfsspi_device->irq_lock);
			pr_info("%s irq already diabled\n", __func__);
		}
	}
	return IRQ_HANDLED;
}

static int vfsspi_set_drdy_int(struct vfsspi_device_data *vfsspi_device,
			       unsigned long arg)
{
	unsigned short drdy_enable_flag;
	if (copy_from_user(&drdy_enable_flag, (void *)arg,
			   sizeof(drdy_enable_flag)) != 0) {
		pr_err("%s Failed copy from user.\n", __func__);
		return -EFAULT;
	}
	if (drdy_enable_flag == 0)
			vfsspi_disableIrq(vfsspi_device);
	else {
			vfsspi_enableIrq(vfsspi_device);
			/* Workaround the issue where the system
			  misses DRDY notification to host when
			  DRDY pin was asserted before enabling
			  device.*/
			if (gpio_get_value(vfsspi_device->drdy_pin) ==
				DRDY_ACTIVE_STATUS) {
				pr_info("%s drdy pin is already active atatus\n", __func__);
				vfsspi_send_drdy_signal(vfsspi_device);
			}
	}
	return 0;
}

static void vfsspi_regulator_onoff(struct vfsspi_device_data *vfsspi_device,
	bool onoff)
{
	if (vfsspi_device->ldo_pin) {
		if (vfsspi_device->ldocontrol) {
			if (onoff) {
				if (vfsspi_device->ocp_en) {
					gpio_set_value(vfsspi_device->ocp_en, 1);
					usleep_range(2950, 3000);
				}
				if (vfsspi_device->ldo_pin2)
					gpio_set_value(vfsspi_device->ldo_pin2, 1);

				gpio_set_value(vfsspi_device->ldo_pin, 1);
			} else {
				gpio_set_value(vfsspi_device->ldo_pin, 0);

				if (vfsspi_device->ldo_pin2)
					gpio_set_value(vfsspi_device->ldo_pin2, 0);

				if (vfsspi_device->ocp_en)
					gpio_set_value(vfsspi_device->ocp_en, 0);
			}
			vfsspi_device->ldo_onoff = onoff;

			pr_info("%s: %s \n",
				__func__, onoff ? "on" : "off");
		} else
			pr_info("%s: can't control in this revion\n", __func__);
	}
}

static void vfsspi_hardReset(struct vfsspi_device_data *vfsspi_device)
{
	pr_info("%s\n", __func__);

	if (vfsspi_device != NULL) {
#ifdef CONFIG_SENSORS_FP_TKEY_NOTI
		int tkey_ret = 0;
		if(!enable_flag_tkey){
			tkey_ret = change_touchkey_thd_for_fingerprint(1);
			if (tkey_ret >= 0)
				enable_flag_tkey = 1;
		}
#endif
		gpio_set_value(vfsspi_device->sleep_pin, 0);
		mdelay(1);
		gpio_set_value(vfsspi_device->sleep_pin, 1);
		mdelay(5);
	}
}

static void vfsspi_suspend(struct vfsspi_device_data *vfsspi_device)
{
	pr_info("%s\n", __func__);

	if (vfsspi_device != NULL) {
#ifdef CONFIG_SENSORS_FP_TKEY_NOTI
		int tkey_ret = 0;
		tkey_ret = change_touchkey_thd_for_fingerprint(0);
		if (tkey_ret < 0)
			pr_info("%s tkey thd change failed\n", __func__);
		enable_flag_tkey = 0;
#endif
		spin_lock(&vfsspi_device->vfs_spi_lock);
		gpio_set_value(vfsspi_device->sleep_pin, 0);
		spin_unlock(&vfsspi_device->vfs_spi_lock);
	}
}

static void vfsspi_ioctl_power_on(struct vfsspi_device_data *vfsspi_device)
{
	if (vfsspi_device->ldocontrol && !vfsspi_device->ldo_onoff)
		vfsspi_regulator_onoff(vfsspi_device, true);
	else {
		if (vfsspi_device->ldocontrol)
			pr_info("%s already on\n", __func__);
		else
			pr_info("%s can't turn on ldo in this rev\n", __func__);
	}
}

static void vfsspi_ioctl_power_off(struct vfsspi_device_data *vfsspi_device)
{
	if (vfsspi_device->ldocontrol && vfsspi_device->ldo_onoff) {
		vfsspi_regulator_onoff(vfsspi_device, false);
		/* prevent sleep pin floating */
		if (gpio_get_value(vfsspi_device->sleep_pin))
			gpio_set_value(vfsspi_device->sleep_pin, 0);
	} else {
		if (vfsspi_device->ldocontrol)
			pr_info("%s already off\n", __func__);
		else
			pr_info("%s can't turn off ldo in this rev\n", __func__);
	}
}

static long vfsspi_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	int ret_val = 0;
	struct vfsspi_device_data *vfsspi_device = NULL;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	unsigned int onoff = 0;
	unsigned int type_check = -1;
	unsigned int lockscreen_mode = 0;
#endif
	pr_debug("%s\n", __func__);

	if (_IOC_TYPE(cmd) != VFSSPI_IOCTL_MAGIC) {
		pr_err("%s invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
			__func__, cmd, _IOC_TYPE(cmd), VFSSPI_IOCTL_MAGIC);
		return -ENOTTY;
	}

	vfsspi_device = filp->private_data;
	mutex_lock(&vfsspi_device->buffer_mutex);
	switch (cmd) {
	case VFSSPI_IOCTL_DEVICE_RESET:
		pr_debug("%s VFSSPI_IOCTL_DEVICE_RESET\n", __func__);
		vfsspi_hardReset(vfsspi_device);
		break;
	case VFSSPI_IOCTL_DEVICE_SUSPEND:
		pr_debug("%s VFSSPI_IOCTL_DEVICE_SUSPEND\n", __func__);
		vfsspi_suspend(vfsspi_device);
		break;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	case VFSSPI_IOCTL_RW_SPI_MESSAGE:
		pr_debug("%s VFSSPI_IOCTL_RW_SPI_MESSAGE\n", __func__);
		ret_val = vfsspi_rw_spi_message(vfsspi_device, arg);
		if (ret_val) {
			pr_err("%s : VFSSPI_IOCTL_RW_SPI_MESSAGE error %d\n",
				__func__, ret_val);
		}
		break;
#endif
	case VFSSPI_IOCTL_SET_CLK:
		pr_info("%s VFSSPI_IOCTL_SET_CLK\n", __func__);
		ret_val = vfsspi_set_clk(vfsspi_device, arg);
		break;
	case VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL:
		pr_info("%s VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL\n", __func__);
		ret_val = vfsspi_register_drdy_signal(vfsspi_device, arg);
		break;
	case VFSSPI_IOCTL_SET_DRDY_INT:
		pr_info("%s VFSSPI_IOCTL_SET_DRDY_INT\n", __func__);
		ret_val = vfsspi_set_drdy_int(vfsspi_device, arg);
		break;
	case VFSSPI_IOCTL_POWER_ON:
		pr_info("%s VFSSPI_IOCTL_POWER_ON\n", __func__);
		vfsspi_ioctl_power_on(vfsspi_device);
		break;
	case VFSSPI_IOCTL_POWER_OFF:
		pr_info("%s VFSSPI_IOCTL_POWER_OFF\n", __func__);
		vfsspi_ioctl_power_off(vfsspi_device);
		break;
	case VFSSPI_IOCTL_POWER_CONTROL:
		break;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	case VFSSPI_IOCTL_DISABLE_SPI_CLOCK:
		ret_val = vfsspi_ioctl_disable_spi_clock(vfsspi_device);
		break;

	case VFSSPI_IOCTL_SET_SPI_CONFIGURATION:
		pr_info("%s VFSSPI_IOCTL_SET_SPI_CONFIGURATION\n", __func__);
		break;
	case VFSSPI_IOCTL_RESET_SPI_CONFIGURATION:
		pr_info("%s VFSSPI_IOCTL_RESET_SPI_CONFIGURATION\n", __func__);
		break;
	case VFSSPI_IOCTL_CPU_SPEEDUP:
		if (copy_from_user(&onoff, (void *)arg,
			sizeof(unsigned int)) != 0) {
			pr_err("%s Failed copy from user.(CPU_SPEEDUP)\n", __func__);
			mutex_unlock(&vfsspi_device->buffer_mutex);
			return -EFAULT;
		}
		if (onoff) {
			u8 retry_cnt = 0;
			pr_info("%s VFSSPI_IOCTL_CPU_SPEEDUP ON:%d, retry: %d\n",
				__func__, onoff, retry_cnt);
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
			do {
				ret_val = secos_booster_start(onoff - 1);
				retry_cnt++;
				if (ret_val) {
					pr_err("%s: booster start failed. (%d) retry: %d\n"
						, __func__, ret_val, retry_cnt);
					if (retry_cnt < 7)
						usleep_range(500, 510);
				}
			} while (ret_val && retry_cnt < 7);
#endif
		} else {
			pr_info("%s VFSSPI_IOCTL_CPU_SPEEDUP OFF\n", __func__);
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
			ret_val = secos_booster_stop();
			if (ret_val)
				pr_err("%s: booster stop failed. (%d)\n"
					, __func__, ret_val);
#endif
		}
		break;
	case VFSSPI_IOCTL_SET_SENSOR_TYPE:
		if (copy_from_user(&type_check, (void *)arg,
			sizeof(unsigned int)) != 0) {
			pr_err("%s Failed copy from user.(SET_SENSOR_TYPE)\n", __func__);
			mutex_unlock(&vfsspi_device->buffer_mutex);
			return -EFAULT;
		}
		if ((int)type_check >= SENSOR_UNKNOWN && (int)type_check < (SENSOR_STATUS_SIZE - 1)) {
			vfsspi_device->sensortype = (int)type_check;
			pr_info("%s VFSSPI_IOCTL_SET_SENSOR_TYPE :%s\n",
					__func__, sensor_status[g_data->sensortype + 1]);
		} else {
			pr_err("%sVFSSPI_IOCTL_SET_SENSOR_TYPE : invalid value %d\n",
					__func__, (int)type_check);
			vfsspi_device->sensortype = SENSOR_UNKNOWN;
		}
		break;
	case VFSSPI_IOCTL_SET_LOCKSCREEN:
		if (copy_from_user(&lockscreen_mode,
				(void *)arg,sizeof(unsigned int)) != 0) {
			pr_err("%s Failed copy from user.(SET_LOCKSCREEN_MODE)\n", __func__);
			mutex_unlock(&vfsspi_device->buffer_mutex);
			return -EFAULT;
		}
		lockscreen_mode?(fp_lockscreen_mode=true):(fp_lockscreen_mode=false);
		pr_info("%s VFSSPI_IOCTL_SET_LOCKSCREEN :%s \n",
				__func__, fp_lockscreen_mode?"ON":"OFF");
		break;
#endif
	case VFSSPI_IOCTL_GET_SENSOR_ORIENT:
		pr_info("%s: orient is %d(0: normal, 1: upsidedown)\n",
			__func__, vfsspi_device->orient);
		if (copy_to_user((void *)arg,
			&(vfsspi_device->orient),
			sizeof(vfsspi_device->orient))
			!= 0) {
			ret_val = -EFAULT;
			pr_err("%s cp to user fail\n", __func__);
		}
		break;

	default:
		pr_info("%s default error. %u\n", __func__, cmd);
		ret_val = -EFAULT;
		break;
	}
	mutex_unlock(&vfsspi_device->buffer_mutex);
	return ret_val;
}

static int vfsspi_open(struct inode *inode, struct file *filp)
{
	struct vfsspi_device_data *vfsspi_device = NULL;
	int status = -ENXIO;

	pr_info("%s\n", __func__);

	mutex_lock(&device_list_mutex);

	list_for_each_entry(vfsspi_device, &device_list, device_entry) {
		if (vfsspi_device->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (!vfsspi_device->ldo_onoff) {
		vfsspi_regulator_onoff(vfsspi_device, true);
		msleep(100);
	}

	if (status == 0) {
		mutex_lock(&vfsspi_device->kernel_lock);
		if (vfsspi_device->is_opened != 0) {
			status = -EBUSY;
			pr_err("%s vfsspi_open: is_opened != 0, -EBUSY\n",
				__func__);
			goto vfsspi_open_out;
		}
		vfsspi_device->user_pid = 0;
		if (vfsspi_device->buffer != NULL) {
			pr_err("%s vfsspi_open: buffer != NULL\n", __func__);
			goto vfsspi_open_out;
		}
		vfsspi_device->null_buffer =
			kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (vfsspi_device->null_buffer == NULL) {
			status = -ENOMEM;
			pr_err("%s vfsspi_open: null_buffer == NULL, -ENOMEM\n",
				__func__);
			goto vfsspi_open_out;
		}
		vfsspi_device->buffer =
			kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (vfsspi_device->buffer == NULL) {
			status = -ENOMEM;
			kfree(vfsspi_device->null_buffer);
			pr_err("%s vfsspi_open: buffer == NULL, -ENOMEM\n",
				__func__);
			goto vfsspi_open_out;
		}
		vfsspi_device->is_opened = 1;
		filp->private_data = vfsspi_device;
		nonseekable_open(inode, filp);

vfsspi_open_out:
		mutex_unlock(&vfsspi_device->kernel_lock);
	}
	mutex_unlock(&device_list_mutex);
	return status;
}


static int vfsspi_release(struct inode *inode, struct file *filp)
{
	struct vfsspi_device_data *vfsspi_device = NULL;
	int                   status     = 0;

	pr_info("%s\n", __func__);

	mutex_lock(&device_list_mutex);
	vfsspi_device = filp->private_data;
	filp->private_data = NULL;
	vfsspi_device->is_opened = 0;
	if (vfsspi_device->buffer != NULL) {
		kfree(vfsspi_device->buffer);
		vfsspi_device->buffer = NULL;
	}

	if (vfsspi_device->null_buffer != NULL) {
		kfree(vfsspi_device->null_buffer);
		vfsspi_device->null_buffer = NULL;
	}

	if (vfsspi_device->ldo_onoff)
		vfsspi_regulator_onoff(vfsspi_device, false);

	mutex_unlock(&device_list_mutex);
	return status;
}

/* file operations associated with device */
static const struct file_operations vfsspi_fops = {
	.owner   = THIS_MODULE,
	.write   = vfsspi_write,
	.read    = vfsspi_read,
	.unlocked_ioctl = vfsspi_ioctl,
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
	.compat_ioctl   = vfsspi_ioctl,
#endif
	.open    = vfsspi_open,
	.release = vfsspi_release,
};

static int vfsspi_platformInit(struct vfsspi_device_data *vfsspi_device)
{
	int status = 0;
	pr_info("%s\n", __func__);

	if (vfsspi_device->ocp_en) {
		status = gpio_request(vfsspi_device->ocp_en, "vfsspi_ocp_en");
		if (status < 0) {
			pr_err("%s gpio_request vfsspi_ocp_en failed\n",
				__func__);
			goto vfsspi_platformInit_ocpen_failed;
		}
		gpio_direction_output(vfsspi_device->ocp_en, 0);
		pr_info("%s ocp off\n", __func__);
	}
	if (vfsspi_device->ldo_pin) {
		status = gpio_request(vfsspi_device->ldo_pin, "vfsspi_ldo_en");
		if (status < 0) {
			pr_err("%s gpio_request vfsspi_ldo_en failed\n",
				__func__);
			goto vfsspi_platformInit_ldo_failed;
		}
		gpio_direction_output(vfsspi_device->ldo_pin, 0);
	}
	if (vfsspi_device->ldo_pin2) {
		status =
			gpio_request(vfsspi_device->ldo_pin2, "vfsspi_ldo_en2");
		if (status < 0) {
			pr_err("%s gpio_request vfsspi_ldo_en2 failed\n",
				__func__);
			goto vfsspi_platformInit_ldo2_failed;
		}
		gpio_direction_output(vfsspi_device->ldo_pin2, 0);
	}

	if (gpio_request(vfsspi_device->drdy_pin, "vfsspi_drdy") < 0) {
		status = -EBUSY;
		goto vfsspi_platformInit_drdy_failed;
	}

	if (gpio_request(vfsspi_device->sleep_pin, "vfsspi_sleep")) {
		status = -EBUSY;
		goto vfsspi_platformInit_sleep_failed;
	}

	status = gpio_direction_output(vfsspi_device->sleep_pin, 0);
	if (status < 0) {
		pr_err("%s gpio_direction_output SLEEP failed\n", __func__);
		status = -EBUSY;
		goto vfsspi_platformInit_gpio_init_failed;
	}
	if (vfsspi_device->retain_pin) {
		status = gpio_direction_output(vfsspi_device->retain_pin, 1);
		vfsspi_device->retain_delayset = 1;
		if (status < 0) {
			pr_err("%s gpio_direction_output retainPin failed\n", __func__);
			status = -EBUSY;
			goto vfsspi_platformInit_gpio_init_failed;
		}
	}
	spin_lock_init(&vfsspi_device->irq_lock);

	status = gpio_direction_input(vfsspi_device->drdy_pin);
	if (status < 0) {
		pr_err("%s gpio_direction_input DRDY failed\n", __func__);
		status = -EBUSY;
		goto vfsspi_platformInit_gpio_init_failed;
	}

	gpio_irq = gpio_to_irq(vfsspi_device->drdy_pin);

	if (gpio_irq < 0) {
		pr_err("%s gpio_to_irq failed\n", __func__);
		status = -EBUSY;
		goto vfsspi_platformInit_gpio_init_failed;
	}

	if (request_irq(gpio_irq, vfsspi_irq, IRQF_TRIGGER_RISING,
			"vfsspi_irq", vfsspi_device) < 0) {
		pr_err("%s request_irq failed\n", __func__);
		status = -EBUSY;
		goto vfsspi_platformInit_irq_failed;
	}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#ifdef FEATURE_SPI_WAKELOCK
	wake_lock_init(&vfsspi_device->fp_spi_lock,
		WAKE_LOCK_SUSPEND, "vfsspi_wake_lock");
	wake_lock_init(&vfsspi_device->fp_signal_lock,
		WAKE_LOCK_SUSPEND, "vfsspi_sigwake_lock");
#endif
#endif

	pr_info("%s : platformInit success!\n", __func__);
	return status;

vfsspi_platformInit_irq_failed:
vfsspi_platformInit_gpio_init_failed:
	gpio_free(vfsspi_device->sleep_pin);
vfsspi_platformInit_sleep_failed:
	gpio_free(vfsspi_device->drdy_pin);
vfsspi_platformInit_drdy_failed:
	if (vfsspi_device->ldo_pin2)
		gpio_free(vfsspi_device->ldo_pin2);
vfsspi_platformInit_ldo2_failed:
	gpio_free(vfsspi_device->ldo_pin);
vfsspi_platformInit_ldo_failed:
	if (vfsspi_device->ocp_en)
		gpio_free(vfsspi_device->ocp_en);
vfsspi_platformInit_ocpen_failed:
	pr_info("%s : platformInit failed!\n", __func__);
	return status;
}

static void vfsspi_platformUninit(struct vfsspi_device_data *vfsspi_device)
{
	pr_info("%s\n", __func__);

	if (vfsspi_device != NULL) {
		free_irq(gpio_irq, vfsspi_device);
		vfsspi_device->drdy_irq_flag = DRDY_IRQ_DISABLE;
		if (vfsspi_device->ldo_pin)
			gpio_free(vfsspi_device->ldo_pin);
		if (vfsspi_device->ldo_pin2)
			gpio_free(vfsspi_device->ldo_pin2);
		gpio_free(vfsspi_device->sleep_pin);
		gpio_free(vfsspi_device->drdy_pin);
#ifdef CONFIG_SENSORS_FINGERPRINT_DUALIZATION
		if (vfsspi_device->vendor_pin)
			gpio_free(vfsspi_device->vendor_pin);
#endif
		if (vfsspi_device->retain_pin)
			gpio_free(vfsspi_device->retain_pin);
		if (vfsspi_device->ocp_en)
			gpio_free(vfsspi_device->ocp_en);

#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
		if (vfsspi_device->cs_gpio)
			gpio_free(vfsspi_device->cs_gpio);
#endif
#endif
                
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#ifdef FEATURE_SPI_WAKELOCK
		wake_lock_destroy(&vfsspi_device->fp_spi_lock);
		wake_lock_destroy(&vfsspi_device->fp_signal_lock);
#endif
#endif
	}
}

static int vfsspi_parse_dt(struct device *dev,
	struct vfsspi_device_data *data)
{
	struct device_node *np = dev->of_node;
	int errorno = 0;
	int gpio;

#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
	gpio = of_get_named_gpio(np, "vfsspi-csgpio", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->cs_gpio = gpio;
		pr_info("%s: cs_gpio=%d\n",
			__func__, data->cs_gpio);
	}
#endif
#endif
	gpio = of_get_named_gpio(np, "vfsspi-sleepPin", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->sleep_pin = gpio;
		pr_info("%s: sleepPin=%d\n",
			__func__, data->sleep_pin);
	}
	gpio = of_get_named_gpio(np, "vfsspi-drdyPin", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->drdy_pin = gpio;
		pr_info("%s: drdyPin=%d\n",
			__func__, data->drdy_pin);
	}
	if (!of_find_property(np, "vfsspi-ocpen", NULL)) {
		pr_info("%s: not set ocp_en in dts\n", __func__);
	} else {
		gpio = of_get_named_gpio(np, "vfsspi-ocpen", 0);
		if (gpio < 0)
			pr_err("%s: fail to get ocp_en\n", __func__);
		else {
			data->ocp_en = gpio;
				pr_info("%s: ocp_en=%d\n",
					__func__, data->ocp_en);
		}
	}

	gpio = of_get_named_gpio(np, "vfsspi-ldoPin", 0);
	if (gpio < 0) {
		data->ldo_pin = 0;
		pr_err("%s: fail to get ldo_pin\n", __func__);
	} else {
		data->ldo_pin = gpio;
		pr_info("%s: ldo_pin=%d\n",
			__func__, data->ldo_pin);
	}
	if (!of_find_property(np, "vfsspi-ldoPin2", NULL)) {
		pr_info("%s: not set ldo2 in dts\n", __func__);
		data->ldo_pin2 = 0;
	} else {
		gpio = of_get_named_gpio(np, "vfsspi-ldoPin2", 0);
		if (gpio < 0) {
			data->ldo_pin2 = 0;
			pr_err("%s: fail to get ldo_pin2\n", __func__);
		} else {
			data->ldo_pin2 = gpio;
			pr_info("%s: ldo_pin2=%d\n",
				__func__, data->ldo_pin2);
		}
	}

#ifdef CONFIG_SENSORS_FINGERPRINT_DUALIZATION
	if (!of_find_property(np, "vfsspi-vendorPin", NULL)) {
		pr_err("%s: not set vendorPin in dts\n", __func__);
		data->vendor_pin = 0;
	} else {
		gpio = of_get_named_gpio(np, "vfsspi-vendorPin", 0);
		if (gpio < 0) {
			data->vendor_pin = 0;
			pr_err("%s: fail to get vendorPin\n", __func__);
		} else {
			data->vendor_pin = gpio;
			pr_info("%s: vendorPin=%d\n", __func__, data->vendor_pin);
		}
	}
#endif
	if (!of_find_property(np, "vfsspi-retainPin", NULL)) {
		pr_err("%s: not set retainPin in dts\n", __func__);
		data->retain_pin = 0;
		data->detect_mode = DETECT_NORMAL;
	} else {
		gpio = of_get_named_gpio(np, "vfsspi-retainPin", 0);
		if (gpio < 0) {
			data->retain_pin = 0;
			data->detect_mode = DETECT_NORMAL;
			pr_err("%s: fail to get retainPin\n", __func__);
		} else {
			data->retain_pin = gpio;
			pr_info("%s: retainPin=%d\n", __func__, data->retain_pin);
			gpio_request(data->retain_pin, "vfsspi_retain");
			data->detect_mode = DETECT_ADM;
		}
	}
	if (of_property_read_u32(np, "vfsspi-ldocontrol",
		&data->ldocontrol))
		data->ldocontrol = 0;

	pr_info("%s: ldocontrol=%d\n",
		__func__, data->ldocontrol);

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	data->tz_mode = true;
#endif

	if (of_property_read_u32(np, "vfsspi-orient",
		&data->orient))
		data->orient = 0;

	pr_info("%s: orient=%d\n",
		__func__, data->orient);

	data->p = pinctrl_get_select_default(dev);
	if (IS_ERR(data->p)) {
		errorno = -EINVAL;
		pr_err("%s: failed pinctrl_get\n", __func__);
		goto dt_exit;
	}

	data->pins_sleep = pinctrl_lookup_state(data->p, PINCTRL_STATE_SLEEP);
	if(IS_ERR(data->pins_sleep)) {
		errorno = -EINVAL;
		pr_err("%s : could not get pins sleep_state (%li)\n",
			__func__, PTR_ERR(data->pins_sleep));
		goto fail_pinctrl_get;
	}

	data->pins_idle = pinctrl_lookup_state(data->p, PINCTRL_STATE_IDLE);
	if(IS_ERR(data->pins_idle)) {
		errorno = -EINVAL;
		pr_err("%s : could not get pins idle_state (%li)\n",
			__func__, PTR_ERR(data->pins_idle));
		goto fail_pinctrl_get;
	}
	return 0;
fail_pinctrl_get:
	pinctrl_put(data->p);
dt_exit:
	return errorno;
}

#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
static ssize_t vfsspi_bfs_values_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct vfsspi_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "\"FP_SPICLK\":\"%d\"\n",
			data->current_spi_speed);
}

static ssize_t vfsspi_type_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vfsspi_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->sensortype);
}

static ssize_t vfsspi_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t vfsspi_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}
static ssize_t vfsspi_adm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_data->detect_mode);
}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static ssize_t vfsspi_retain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (g_data->retain_pin)
		return sprintf(buf, "%d\n", gpio_get_value(g_data->retain_pin));
	else
		return sprintf(buf, "-1\n");
}

static ssize_t vfsspi_retain_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s, input parameter count was wrong.\n", __func__);
		return -EINVAL;
	}

	if (val == 1) {
		vfsspi_set_retain_pin(g_data, 1, 1);
	} else if (val == 0) {
		vfsspi_set_retain_pin(g_data, 0, 1);
	} else {
		pr_err("%s, input value was not accepted.\n", __func__);
		return -EINVAL;
	}
	return count;
}
#endif
static DEVICE_ATTR(bfs_values, S_IRUGO,
	vfsspi_bfs_values_show, NULL);
static DEVICE_ATTR(type_check, S_IRUGO,
	vfsspi_type_check_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO,
	vfsspi_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO,
	vfsspi_name_show, NULL);
static DEVICE_ATTR(adm, S_IRUGO,
	vfsspi_adm_show, NULL);
#ifndef ENABLE_SENSORS_FPRINT_SECURE
static DEVICE_ATTR(retain_pin, S_IRUGO | S_IWUSR | S_IWGRP,
	vfsspi_retain_show, vfsspi_retain_store);
#endif

static struct device_attribute *fp_attrs[] = {
	&dev_attr_bfs_values,
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_adm,
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	&dev_attr_retain_pin,
#endif
	NULL,
};
#endif

static void vfsspi_work_func_debug(struct work_struct *work)
{
	u8 ldo_value = 0;

	if (g_data->ldo_pin2 == 0) {
		ldo_value = gpio_get_value(g_data->ldo_pin);
	} else {
		ldo_value = (gpio_get_value(g_data->ldo_pin2) << 1)
					| gpio_get_value(g_data->ldo_pin);
	}
	pr_info("%s ldo:%d,"
		" sleep:%d, irq:%d, tz:%d, type:%s, cnt_irq:%d, adm: %d\n",
		__func__,
		ldo_value, gpio_get_value(g_data->sleep_pin),
		gpio_get_value(g_data->drdy_pin),
		g_data->tz_mode,
		sensor_status[g_data->sensortype + 1],
		cnt_irq, g_data->detect_mode);
}

static void vfsspi_enable_debug_timer(void)
{
	mod_timer(&g_data->dbg_timer,
		round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static void vfsspi_disable_debug_timer(void)
{
	del_timer_sync(&g_data->dbg_timer);
	cancel_work_sync(&g_data->work_debug);
}

static void vfsspi_timer_func(unsigned long ptr)
{
	queue_work(g_data->wq_dbg, &g_data->work_debug);
	mod_timer(&g_data->dbg_timer,
		round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}
#ifdef CONFIG_SENSORS_FINGERPRINT_DUALIZATION
static int vfsspi_vendor_check(struct vfsspi_device_data *vfsspi_device)
{
	int status = 0;
	pr_info("%s\n", __func__);

	vfsspi_regulator_onoff(vfsspi_device, true); /* power on */
	msleep(10);

	status = gpio_request(vfsspi_device->vendor_pin, "vfsspi_vendor");
	if (status < 0) {
		pr_info("%s: gpio_request - vfsspi_vendor failed\n", __func__);
		goto vfsspi_vendor_check_vendor_failed;
	}

	status = gpio_direction_input(vfsspi_device->vendor_pin);
	if (status < 0) {
		pr_err("%s gpio_direction_input VENDOR failed\n", __func__);
		status = -EBUSY;
                gpio_free(vfsspi_device->vendor_pin);
		goto vfsspi_vendor_check_vendor_failed;
	}

	status = gpio_get_value(vfsspi_device->vendor_pin);

	pr_info("%s is success, vendorPin (%d), value (%d)\n"
		, __func__, vfsspi_device->vendor_pin, status);

vfsspi_vendor_check_vendor_failed:
	vfsspi_regulator_onoff(vfsspi_device, false); /* power off */
	return status;
}
#endif
#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int vfsspi_type_check(struct vfsspi_device_data *vfsspi_device)
{
	struct spi_device * spi = NULL;
	char tx_buf[64] = {1};
	char rx_buf[64] = {0};
	int i = 0;
	struct spi_transfer t;
	struct spi_message m;
#ifdef ENABLE_SENSORS_FPRINT_SECURE_
	struct spi_device *spidev = NULL;
	int ret_val = 0;
#endif
	pr_info("%s\n", __func__);
	vfsspi_regulator_onoff(vfsspi_device, true);

	/* check sensor if it is viper */
	vfsspi_hardReset(vfsspi_device);
	msleep(20);

	spi = vfsspi_device->spi;
	tx_buf[0] = 1; /* EP0 Read */
	tx_buf[1] = 0;
	spi->bits_per_word = BITS_PER_WORD;
	spi->mode = SPI_MODE_0;
	memset(&t, 0, sizeof(t));
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	t.len = 6;
	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	pr_info("%s ValiditySensor: spi_sync returned %d\n",
		__func__, spi_sync(spi, &m));

	if (((rx_buf[2] == 0x01) || (rx_buf[2] == 0x41))
		&& (rx_buf[5] == 0x68)) {
		vfsspi_device->sensortype = SENSOR_VIPER;
		gpio_set_value(vfsspi_device->sleep_pin, 0);
		pr_info("%s sensor type is VIPER\n", __func__);
		goto type_check_exit;
	} else {
		pr_info("%s sensor type is not VIPER\n", __func__);
		for (i = 0; i < 6; i++)
			pr_info("%s, %0x\n", __func__, rx_buf[i]);
	}

	/* check sensor if it is raptor */
	gpio_set_value(vfsspi_device->sleep_pin, 0);

	msleep(20);

	tx_buf[0] = 5;
	tx_buf[1] = 0;

	spi->bits_per_word = 16;
	memset(&t, 0, sizeof(t));
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	t.len = 64;
	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	pr_info("ValiditySensor: spi_sync returned %d\n",
		spi_sync(spi, &m));

	if (((rx_buf[0] == 0x98) || (rx_buf[0] == 0xBA))
		&& ((rx_buf[1] == 0x98) || (rx_buf[1] == 0xBA))) {
		vfsspi_device->sensortype = SENSOR_RAPTOR;
		pr_info("%s sensor type is RAPTOR\n", __func__);
	} else {
		vfsspi_device->sensortype = SENSOR_FAILED;
		pr_info("%s sensor type is FAILED\n", __func__);
	}

	spi->bits_per_word = BITS_PER_WORD;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	spi_setup(spi);

type_check_exit:
#ifdef ENABLE_SENSORS_FPRINT_SECURE_
	pr_info("%s ENABLE_SPI_CLOCK\n", __func__);
	spin_lock_irq(&vfsspi_device->vfs_spi_lock);
	spidev = spi_dev_get(vfsspi_device->spi);
	spin_unlock_irq(&vfsspi_device->vfs_spi_lock);

	ret_val = fp_spi_clock_enable(spidev);
	if (ret_val < 0)
		pr_err("%s: Unable to enable spi clk\n",
			__func__);
	else {
		ret_val = fp_spi_clock_set_rate(spidev);
		if (ret_val < 0)
			pr_err("%s: Unable to set spi clk rate\n",
				__func__);
	}
	usleep_range(950, 1000);

	spi_dev_put(spidev);

	pr_info("%s DISABLE_SPI_CLOCK\n", __func__);
	spin_lock_irq(&vfsspi_device->vfs_spi_lock);
	spidev = spi_dev_get(vfsspi_device->spi);
	spin_unlock_irq(&vfsspi_device->vfs_spi_lock);

	ret_val = fp_spi_clock_disable(spidev);
	if (ret_val < 0)
		pr_err("%s: couldn't disable spi clks\n", __func__);

	spi_dev_put(spidev);

	pr_info("%s SET_SPI_CONFIGURATION\n", __func__);
#endif /* ENABLE_SENSORS_FPRINT_SECURE */
	vfsspi_regulator_onoff(vfsspi_device, false);

	return 0;
}
#endif

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int vfsspi_wakeup_daemon(struct vfsspi_device_data *vfsspi_device)
{
#ifdef CONFIG_SENSORS_FP_LOCKSCREEN_MODE
	if (fp_lockscreen_mode) {
		if (vfsspi_device->signal_id) {
			if (wakeup_by_key() == true && 
				atomic_read(&vfsspi_device->irq_enabled) == DRDY_IRQ_DISABLE) {
				vfsspi_send_drdy_signal(vfsspi_device);
				pr_info("%s send signal done!\n", __func__);
			} else {
				pr_err("%s send signal failed by wakeup(%d)\n",
					__func__, wakeup_by_key());
			}
		} else {
			pr_err("%s fingerprint has no signal_id\n", __func__);
		}
	}
#endif
	return 0;
}
#endif

#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
static int vfsspi_set_cs_gpio(struct vfsspi_device_data *vfsspi_device, struct s3c64xx_spi_csinfo *cs)
{
	int status = -1;

	pr_info("%s, spi auto cs mode(%d)\n", __func__, cs->cs_mode);

	if (vfsspi_device->cs_gpio) {
	        cs->line = vfsspi_device->cs_gpio;
	        if (!gpio_is_valid(cs->line))
	                cs->line = 0;
	} else {
	        cs->line = 0;
	}

	if(cs->line != 0) {
	        status = gpio_request_one(cs->line, GPIOF_OUT_INIT_HIGH,
	                               dev_name(&vfsspi_device->spi->dev));
	        if (status) {
	                dev_err(&vfsspi_device->spi->dev,
	                        "Failed to get /CS gpio [%d]: %d\n",
	                        cs->line, status);
	        }
	}

        return status;
}
#endif
#endif

static int vfsspi_probe(struct spi_device *spi)
{
	int status = 0;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
#ifdef CONFIG_SOC_EXYNOS8890
	struct s3c64xx_spi_csinfo *cs;
#endif
#endif
	struct vfsspi_device_data *vfsspi_device;
	struct device *dev;

	pr_info("%s\n", __func__);

	vfsspi_device = kzalloc(sizeof(*vfsspi_device), GFP_KERNEL);

	if (vfsspi_device == NULL)
		return -ENOMEM;

	if (spi->dev.of_node) {
		status = vfsspi_parse_dt(&spi->dev, vfsspi_device);
		if (status) {
			pr_err("%s - Failed to parse DT\n", __func__);
			goto vfsspi_probe_parse_dt_failed;
		}
	}

	/* Initialize driver data. */
	vfsspi_device->current_spi_speed = SLOW_BAUD_RATE;
	vfsspi_device->spi = spi;
	g_data = vfsspi_device;

	spin_lock_init(&vfsspi_device->vfs_spi_lock);
	mutex_init(&vfsspi_device->buffer_mutex);
	mutex_init(&vfsspi_device->kernel_lock);

	INIT_LIST_HEAD(&vfsspi_device->device_entry);

	status = vfsspi_platformInit(vfsspi_device);
	if (status) {
		pr_err("%s - Failed to platformInit\n", __func__);
		goto vfsspi_probe_platformInit_failed;
	}
#ifdef CONFIG_SENSORS_FINGERPRINT_DUALIZATION
	/* vendor check */
	if (vfsspi_device->vendor_pin) {
		status = vfsspi_vendor_check(vfsspi_device);

		if (status) { /* normal = 0, not viper = 1 */
			if (status) {
				pr_info("%s: It is not viper sensor.\n", __func__);
				status = -ENODEV;
				goto vfsspi_vendor_check_failed;
			} else if (status < 0) {
				pr_info("%s: vendor gpio request failed.\n", __func__);
				goto vfsspi_vendor_request_failed;
			}
		} else {
			FP_CHECK = 1; /* It is viper sensor */
		}
	} else
		pr_info("%s: This model has no vender pin dts.\n", __func__);
#endif
	spi->bits_per_word = BITS_PER_WORD;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;

#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SOC_EXYNOS8890
	/* set cs pin in fp driver, use only Exynos8890 */
	/* for use auto cs mode with dualization fp sensor */
	cs = spi->controller_data;

	if (cs->cs_mode == 1) {
		status = vfsspi_set_cs_gpio(vfsspi_device, cs);
	} else {
		pr_info("%s, spi manual mode(%d)\n", __func__, cs->cs_mode);
	}
#endif

	status = spi_setup(spi);
	if (status) {
		pr_err("%s : spi_setup failed\n", __func__);
		goto vfsspi_probe_spi_setup_failed;
	}
#endif

	mutex_lock(&device_list_mutex);
	/* Create device node */
	/* register major number for character device */
	status = alloc_chrdev_region(&(vfsspi_device->devt),
				     0, 1, VALIDITY_PART_NAME);
	if (status < 0) {
		pr_err("%s alloc_chrdev_region failed\n", __func__);
		goto vfsspi_probe_alloc_chardev_failed;
	}

	cdev_init(&(vfsspi_device->cdev), &vfsspi_fops);
	vfsspi_device->cdev.owner = THIS_MODULE;
	status = cdev_add(&(vfsspi_device->cdev), vfsspi_device->devt, 1);
	if (status < 0) {
		pr_err("%s cdev_add failed\n", __func__);
		goto vfsspi_probe_cdev_add_failed;
	}

	vfsspi_device_class = class_create(THIS_MODULE, "validity_fingerprint");

	if (IS_ERR(vfsspi_device_class)) {
		pr_err("%s vfsspi_init: class_create() is failed - unregister chrdev.\n",
			__func__);
		status = PTR_ERR(vfsspi_device_class);
		goto vfsspi_probe_class_create_failed;
	}

	dev = device_create(vfsspi_device_class, &spi->dev,
			    vfsspi_device->devt, vfsspi_device, "vfsspi");
	status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	if (status == 0)
		list_add(&vfsspi_device->device_entry, &device_list);
	mutex_unlock(&device_list_mutex);

	if (status != 0)
		goto vfsspi_probe_failed;

	spi_set_drvdata(spi, vfsspi_device);

#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
	status = fingerprint_register(vfsspi_device->fp_device,
		vfsspi_device, fp_attrs, "fingerprint");
	if (status) {
		pr_err("%s sysfs register failed\n", __func__);
		goto vfsspi_probe_failed;
	}
#endif

	/* debug polling function */
	setup_timer(&vfsspi_device->dbg_timer,
		vfsspi_timer_func, (unsigned long)vfsspi_device);

	vfsspi_device->wq_dbg =
		create_singlethread_workqueue("vfsspi_debug_wq");
	if (!vfsspi_device->wq_dbg) {
		status = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto vfsspi_sysfs_failed;
	}
	INIT_WORK(&vfsspi_device->work_debug, vfsspi_work_func_debug);

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	vfsspi_device->sensortype = SENSOR_UNKNOWN;
#else
	/* sensor hw type check */
	do {
		vfsspi_type_check(vfsspi_device);
		pr_info("%s, type (%u), retry (%d)\n",
			__func__, vfsspi_device->sensortype, retry);
        } while (!vfsspi_device->sensortype && ++retry < 3);
#endif

	disable_irq(gpio_irq);
	vfsspi_pin_control(vfsspi_device, false);
	vfsspi_enable_debug_timer();
#ifdef CONFIG_FB
	if (vfsspi_device->retain_pin) {
		vfsspi_device->fb_notifier.notifier_call = vfsspi_fb_notifier_callback;
		fb_register_client(&vfsspi_device->fb_notifier);
	}
#endif
	pr_info("%s successful\n", __func__);

	return 0;

vfsspi_sysfs_failed:
#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
		fingerprint_unregister(vfsspi_device->fp_device, fp_attrs);
#endif
vfsspi_probe_failed:
	device_destroy(vfsspi_device_class, vfsspi_device->devt);
	class_destroy(vfsspi_device_class);
vfsspi_probe_class_create_failed:
	cdev_del(&(vfsspi_device->cdev));
vfsspi_probe_cdev_add_failed:
	unregister_chrdev_region(vfsspi_device->devt, 1);
vfsspi_probe_alloc_chardev_failed:
#ifndef ENABLE_SENSORS_FPRINT_SECURE
vfsspi_probe_spi_setup_failed:
#endif
#ifdef CONFIG_SENSORS_FINGERPRINT_DUALIZATION
vfsspi_vendor_check_failed:
	pinctrl_put(vfsspi_device->p);
#endif
	vfsspi_platformUninit(vfsspi_device);
#ifdef CONFIG_SENSORS_FINGERPRINT_DUALIZATION
vfsspi_vendor_request_failed:
#endif
vfsspi_probe_platformInit_failed:
	mutex_destroy(&vfsspi_device->buffer_mutex);
	mutex_destroy(&vfsspi_device->kernel_lock);
vfsspi_probe_parse_dt_failed:
	kfree(vfsspi_device);
	pr_err("%s vfsspi_probe failed!!\n", __func__);
	return status;
}

static int vfsspi_remove(struct spi_device *spi)
{
	int status = 0;

	struct vfsspi_device_data *vfsspi_device = NULL;

	pr_info("%s\n", __func__);

	vfsspi_device = spi_get_drvdata(spi);

	if (vfsspi_device != NULL) {
		vfsspi_disable_debug_timer();
		spin_lock_irq(&vfsspi_device->vfs_spi_lock);
		vfsspi_device->spi = NULL;
		spi_set_drvdata(spi, NULL);
		spin_unlock_irq(&vfsspi_device->vfs_spi_lock);

		mutex_lock(&device_list_mutex);

		vfsspi_platformUninit(vfsspi_device);

#ifdef CONFIG_SENSORS_FINGERPRINT_SYSFS
		fingerprint_unregister(vfsspi_device->fp_device, fp_attrs);
#endif
		/* Remove device entry. */
		list_del(&vfsspi_device->device_entry);
		device_destroy(vfsspi_device_class, vfsspi_device->devt);
		class_destroy(vfsspi_device_class);
		cdev_del(&(vfsspi_device->cdev));
		unregister_chrdev_region(vfsspi_device->devt, 1);

		mutex_destroy(&vfsspi_device->buffer_mutex);
		mutex_destroy(&vfsspi_device->kernel_lock);

		kfree(vfsspi_device);
		mutex_unlock(&device_list_mutex);
	}

	return status;
}

static void vfsspi_shutdown(struct spi_device *spi)
{
	if (g_data != NULL)
		vfsspi_disable_debug_timer();

	pr_info("%s\n", __func__);
}

static int vfsspi_pm_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);
	if (g_data != NULL) {
		vfsspi_disable_debug_timer();
		if (g_data->retain_pin) {
			if ((g_data->ldo_onoff) && (atomic_read(&g_data->irq_enabled) == DRDY_IRQ_ENABLE)) {
				g_data->retain_onoff = 1;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
				vfsspi_goto_suspend = 1; /* used by pinctrl_samsung.c */
				pr_info("%s: suspend smc ret(WOG)=%d, en:%d\n", __func__,
						exynos_smc(MC_FC_FP_PM_SUSPEND_RETAIN, 0, 0, 0),
						vfsspi_goto_suspend);
#endif
			} else {
				g_data->retain_onoff = 0;
				vfsspi_ioctl_power_off(g_data);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
				vfsspi_goto_suspend = 1; /* used by pinctrl_samsung.c */
				pr_info("%s: suspend smc ret=%d\n", __func__,
					exynos_smc(MC_FC_FP_PM_SUSPEND, 0, 0, 0));
#endif
			}
			pr_info("%s retain++ %d, retain_en %d\n", __func__,
					gpio_get_value(g_data->retain_pin), g_data->retain_onoff);
		} else {
			vfsspi_ioctl_power_off(g_data);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
			pr_info("%s: suspend smc ret=%d\n", __func__,
					exynos_smc(MC_FC_FP_PM_SUSPEND, 0, 0, 0));
#endif
		}
	}
	return 0;
}

static int vfsspi_pm_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	if (g_data != NULL) {
		if (g_data->retain_pin) {
#ifdef ENABLE_SENSORS_FPRINT_SECURE
			if (vfsspi_goto_suspend) {
				vfsspi_goto_suspend = 0;
				pr_info("%s: resume smc ret=%d, en:%d\n", __func__,
						exynos_smc(MC_FC_FP_PM_RESUME, 0, 0, 0),
						vfsspi_goto_suspend);
			}
#endif
		} else {
#ifdef ENABLE_SENSORS_FPRINT_SECURE
			vfsspi_wakeup_daemon(g_data);
			pr_info("%s: resume smc ret=%d\n", __func__,
					exynos_smc(MC_FC_FP_PM_RESUME, 0, 0, 0));
#endif
		}
		if (!g_data->ldo_onoff)
			vfsspi_ioctl_power_on(g_data);
		vfsspi_enable_debug_timer();
	}
	return 0;
}

static const struct dev_pm_ops vfsspi_pm_ops = {
	.suspend = vfsspi_pm_suspend,
	.resume = vfsspi_pm_resume
};

struct spi_driver vfsspi_spi = {
	.driver = {
		.name  = VALIDITY_PART_NAME,
		.owner = THIS_MODULE,
		.pm = &vfsspi_pm_ops,
		.of_match_table = vfsspi_match_table,
	},
		.probe  = vfsspi_probe,
		.remove = vfsspi_remove,
		.shutdown = vfsspi_shutdown,
};

static int __init vfsspi_init(void)
{
	int status = 0;

	pr_info("%s vfsspi_init\n", __func__);

	status = spi_register_driver(&vfsspi_spi);
	if (status < 0) {
		pr_err("%s spi_register_driver() failed\n", __func__);
		return status;
	}
	pr_info("%s init is successful\n", __func__);

	return status;
}

static void __exit vfsspi_exit(void)
{
	pr_debug("%s vfsspi_exit\n", __func__);
	spi_unregister_driver(&vfsspi_spi);
}

module_init(vfsspi_init);
module_exit(vfsspi_exit);

MODULE_DESCRIPTION("Validity FPS sensor");
MODULE_LICENSE("GPL");
