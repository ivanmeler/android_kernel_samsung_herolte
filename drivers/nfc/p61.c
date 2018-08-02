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

 /**
 * \addtogroup spi_driver
 *
 * @{ */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/nfc/p61.h>
#include <linux/ioctl.h>

#include <linux/platform_data/spi-s3c64xx.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/clk.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include <linux/wakelock.h>

/* Device driver's configuration macro */
/* Macro to configure poll/interrupt based req*/
#undef P61_IRQ_ENABLE
//#define P61_IRQ_ENABLE

/* Macro to configure reset to P61*/
#undef P61_RST_ENABLE
//#define P61_RST_ENABLE

/* Macro to configure Hard/Soft reset to P61 */
//#define P61_HARD_RESET
#undef P61_HARD_RESET

#ifdef P61_HARD_RESET
static struct regulator *p61_regulator = NULL;
#else
#endif

#undef PANIC_DEBUG

#define P61_IRQ   33 /* this is the same used in omap3beagle.c */
#define P61_RST  138

/* Macro to define SPI clock frequency */

#define P61_SPI_CLOCK_7MHZ

#ifdef P61_SPI_CLOCK_7MHZ
#define P61_SPI_CLOCK     7000000L;
#else
#define P61_SPI_CLOCK     4000000L;
#endif

/* size of maximum read/write buffer supported by driver */
#define MAX_BUFFER_SIZE   258U

//static struct class *p61_device_class;

/* Different driver debug lever */
enum P61_DEBUG_LEVEL {
    P61_DEBUG_OFF,
    P61_FULL_DEBUG
};

/* Variable to store current debug level request by ioctl */
static unsigned char debug_level = P61_FULL_DEBUG; /////

#define P61_DBG_MSG(msg...)  \
        switch(debug_level)      \
        {                        \
        case P61_DEBUG_OFF:      \
        break;                 \
        case P61_FULL_DEBUG:     \
        printk(KERN_INFO "[NXP-P61] :  " msg); \
        break; \
        default:                 \
        printk(KERN_ERR "[NXP-P61] :  Wrong debug level %d", debug_level); \
        break; \
        } \

#define P61_ERR_MSG(msg...) printk(KERN_ERR "[NFC-P61] : " msg );

/* Device specific macro and structure */
struct p61_dev {
	wait_queue_head_t read_wq; /* wait queue for read interrupt */
	struct mutex read_mutex; /* read mutex */
	struct mutex write_mutex; /* write mutex */
	struct spi_device *spi;  /* spi device structure */
	struct miscdevice p61_device; /* char device as misc driver */
	unsigned int rst_gpio; /* SW Reset gpio */
	unsigned int irq_gpio; /* P61 will interrupt DH for any ntf */
	bool irq_enabled; /* flag to indicate irq is used */
	unsigned char enable_poll_mode; /* enable the poll mode */
	spinlock_t irq_enabled_lock; /*spin lock for read irq */

	//dev_t devt;		/* Device ID */
	unsigned char *null_buffer;
	unsigned char *buffer;

	bool tz_mode;
	spinlock_t ese_spi_lock;
#if 1//def ENABLE_ESE_SECURE
	unsigned int mosipin;
	unsigned int misopin;
	unsigned int cspin;
	unsigned int clkpin;
	bool isGpio_cfgDone;
	bool enabled_clk;
	struct wake_lock ese_lock;
#endif
};

/* T==1 protocol specific global data */
const unsigned char SOF = 0xA5u;

/* Normal World */
static int p61_ioctl_config_spi_gpio(
	struct p61_dev *p61_device)
{
	struct spi_device *spidev = NULL;
	int ret_val = 0;

	if (!p61_device->isGpio_cfgDone) {
		pr_info("%s SET_SPI_CONFIGURATION\n", __func__);
		spin_lock_irq(&p61_device->ese_spi_lock);
		spidev = spi_dev_get(p61_device->spi);
		spin_unlock_irq(&p61_device->ese_spi_lock);
#if 0
		ret_val = ese_spi_request_gpios(spidev);
		if (ret_val < 0)
			pr_err("%s: couldn't config spi gpio\n", __func__);
#endif
		p61_device->isGpio_cfgDone = true;

		p61_device->null_buffer =
			kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (p61_device->null_buffer == NULL) {
			ret_val = -ENOMEM;
			pr_err("%s null_buffer == NULL, -ENOMEM\n",
				__func__);
			//goto vfsspi_open_out;
		}
		p61_device->buffer =
			kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (p61_device->buffer == NULL) {
			ret_val = -ENOMEM;
			kfree(p61_device->null_buffer);
			pr_err("%s buffer == NULL, -ENOMEM\n",
				__func__);
			//goto vfsspi_open_out;
		}

		spi_dev_put(spidev);
		usleep_range(950, 1000);
	}
	return ret_val;
}

static int p61_set_clk(struct p61_dev *p61_device)
{
	int ret_val = 0;
	struct spi_device *spidev = NULL;
	struct s3c64xx_spi_driver_data *sdd = NULL;

	spin_lock_irq(&p61_device->ese_spi_lock);
	spidev = spi_dev_get(p61_device->spi);
	spin_unlock_irq(&p61_device->ese_spi_lock);
	if (spidev == NULL) {
		pr_err("%s - Failed to get spi dev\n", __func__);
		return -1;
	}

	spidev->max_speed_hz = P61_SPI_CLOCK;
	sdd = spi_master_get_devdata(spidev->master);
	if (!sdd){
		pr_err("%s - Failed to get spi dev.\n", __func__);
		return -1;
	}
	pm_runtime_get_sync(&sdd->pdev->dev); /* Enable clk */

	/* set spi clock rate */
	clk_set_rate(sdd->src_clk, spidev->max_speed_hz * 2);

	p61_device->enabled_clk = true;
	spi_dev_put(spidev);

	//CS enable
	gpio_set_value(p61_device->cspin, 0);
	usleep_range(50, 70);
	if (!wake_lock_active(&p61_device->ese_lock)) {
		pr_info("%s: [NFC-ESE] wake lock.\n", __func__);
		wake_lock(&p61_device->ese_lock);
	}
	return ret_val;
}

static int p61_disable_clk(struct p61_dev *p61_device)
{
	int ret_val = 0;
	struct spi_device *spidev = NULL;
	struct s3c64xx_spi_driver_data *sdd = NULL;

	if (!p61_device->enabled_clk) {
		pr_err("%s - clock was not enabled!\n", __func__);
		return ret_val;
	}

	spin_lock_irq(&p61_device->ese_spi_lock);
	spidev = spi_dev_get(p61_device->spi);
	spin_unlock_irq(&p61_device->ese_spi_lock);
	if (spidev == NULL) {
		pr_err("%s - Failed to get spi dev!\n", __func__);
		return -1;
	}

        sdd = spi_master_get_devdata(spidev->master);
        if (!sdd){
                pr_err("%s - Failed to get spi dev.\n", __func__);
                return -1;
	}

	p61_device->enabled_clk = false;
	pm_runtime_put(&sdd->pdev->dev); /* Disable clock */

	spi_dev_put(spidev);

	//CS disable
	gpio_set_value(p61_device->cspin, 1);
	if (wake_lock_active(&p61_device->ese_lock)) {
		pr_info("%s: [NFC-ESE] wake unlock.\n", __func__);
		wake_unlock(&p61_device->ese_lock);
	}
	return ret_val;
}

static int p61_xfer(struct p61_dev *p61_device,
			struct p61_ioctl_transfer *tr)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("%s\n", __func__);

	if (p61_device == NULL || tr == NULL)
		return -EFAULT;

	if (tr->len > DEFAULT_BUFFER_SIZE || !tr->len)
		return -EMSGSIZE;

	if (tr->tx_buffer != NULL) {
		if (copy_from_user(p61_device->null_buffer,
				tr->tx_buffer, tr->len) != 0)
			return -EFAULT;
	}

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = p61_device->null_buffer;
	t.rx_buf = p61_device->buffer;
	t.len = tr->len;
	//t.speed_hz = vfsspi_device->current_spi_speed;

	spi_message_add_tail(&t, &m);

	status = spi_sync(p61_device->spi, &m);

	if (status == 0) {
		if (tr->rx_buffer != NULL) {
			unsigned missing = 0;

			missing = copy_to_user(tr->rx_buffer,
					       p61_device->buffer, tr->len);

			if (missing != 0)
				tr->len = tr->len - missing;
		}
	}
	pr_debug("%s p61_xfer,length=%d\n", __func__, tr->len);
	return status;

} /* vfsspi_xfer */

static int p61_rw_spi_message(struct p61_dev *p61_device,
				 unsigned long arg)
{
	struct p61_ioctl_transfer   *dup = NULL;
	int err = 0;
#ifdef PANIC_DEBUG
	unsigned int addr_rx = 0;
	unsigned int addr_tx = 0;
	unsigned int addr_len = 0;
#endif

	dup = kmalloc(sizeof(struct p61_ioctl_transfer), GFP_KERNEL);
	if (dup == NULL)
		return -ENOMEM;

#ifdef PANIC_DEBUG
	addr_rx = (unsigned int)(&dup->rx_buffer);
	addr_tx = (unsigned int)(&dup->tx_buffer);
	addr_len = (unsigned int)(&dup->len);
#endif
	if (copy_from_user(dup, (void *)arg,
			   sizeof(struct p61_ioctl_transfer)) != 0) {
		kfree(dup);
		return -EFAULT;
	} else {
#ifdef PANIC_DEBUG
		if ((addr_rx != (unsigned int)(&dup->rx_buffer)) ||
			(addr_tx != (unsigned int)(&dup->tx_buffer)) ||
			(addr_len != (unsigned int)(&dup->len)))
		pr_err("%s invalid addr!!! rx=%x, tx=%x, len=%x\n",
			__func__, (unsigned int)(&dup->rx_buffer),
			(unsigned int)(&dup->tx_buffer),
			(unsigned int)(&dup->len));
#endif
		err = p61_xfer(p61_device, dup);
		if (err != 0) {
			kfree(dup);
			pr_err("%s p61_xfer failed!\n", __func__);
			return err;
		}
	}
	if (copy_to_user((void *)arg, dup,
			 sizeof(struct p61_ioctl_transfer)) != 0)
		return -EFAULT;
	kfree(dup);
	return 0;
}

/**
 * \ingroup spi_driver
 * \brief Called from SPI LibEse to initilaize the P61 device
 *
 * \param[in]       struct inode *
 * \param[in]       struct file *
 *
 * \retval 0 if ok.
 *
*/

static int p61_dev_open(struct inode *inode, struct file *filp)
{
	struct p61_dev
	*p61_dev = container_of(filp->private_data,
			struct p61_dev, p61_device);

	filp->private_data = p61_dev;
	P61_DBG_MSG("%s : Major No: %d, Minor No: %d\n", __func__,
			imajor(inode), iminor(inode));

	return 0;
}

/**
 * \ingroup spi_driver
 * \brief To configure the P61_SET_PWR/P61_SET_DBG/P61_SET_POLL
 * \n		  P61_SET_PWR - hard reset (arg=2), soft reset (arg=1)
 * \n		  P61_SET_DBG - Enable/Disable (based on arg value) the driver logs
 * \n		  P61_SET_POLL - Configure the driver in poll (arg = 1), interrupt (arg = 0) based read operation
 * \param[in]       struct file *
 * \param[in]       unsigned int
 * \param[in]       unsigned long
 *
 * \retval 0 if ok.
 *
*/

static long p61_dev_ioctl(struct file *filp, unsigned int cmd,
        unsigned long arg)
{
    int ret = 0;
    struct p61_dev *p61_dev = NULL;
#ifdef P61_RST_ENABLE
    unsigned char buf[100];
#endif

	if (_IOC_TYPE(cmd) != P61_MAGIC) {
		pr_err("%s invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
			__func__, cmd, _IOC_TYPE(cmd), P61_MAGIC);
		return -ENOTTY;
	}
	P61_DBG_MSG(KERN_ALERT "p61_dev_ioctl-Enter %u arg = %ld\n", cmd, arg);
	p61_dev = filp->private_data;

	switch (cmd) {
	case P61_SET_PWR:
		if (arg == 2)
		{
#ifdef P61_HARD_RESET
			P61_DBG_MSG(KERN_ALERT " Disabling p61_regulator");
			if (p61_regulator != NULL)
			{
			    regulator_disable(p61_regulator);
			    msleep(50);
			    regulator_enable(p61_regulator);
			    P61_DBG_MSG(KERN_ALERT " Enabling p61_regulator");
			}
			else
			{
			    P61_ERR_MSG(KERN_ALERT " ERROR : p61_regulator is not enabled");
			}
#endif
		}
#ifdef P61_RST_ENABLE
		else if (arg == 1)
		{
			P61_DBG_MSG(KERN_ALERT " Soft Reset");
			//gpio_set_value(p61_dev->rst_gpio, 1);
			//msleep(20);
			gpio_set_value(p61_dev->rst_gpio, 0);
			msleep(50);
			ret = spi_read (p61_dev -> spi,(void *) buf, sizeof(buf));
			msleep(50);
			gpio_set_value(p61_dev->rst_gpio, 1);
			msleep(20);
		}
#endif
		break;

	case P61_SET_DBG:
		debug_level = (unsigned char )arg;
		P61_DBG_MSG(KERN_INFO"[NXP-P61] -  Debug level %d", debug_level);
		break;
	case P61_SET_POLL:
		p61_dev-> enable_poll_mode = (unsigned char )arg;
		if (p61_dev-> enable_poll_mode == 0)
		{
			P61_DBG_MSG(KERN_INFO"[NXP-P61] - IRQ Mode is set \n");
		}
		else
		{
			P61_DBG_MSG(KERN_INFO"[NXP-P61] - Poll Mode is set \n");
			p61_dev->enable_poll_mode = 1;
		}
		break;
	/*non TZ */
	case P61_SET_SPI_CONFIG:
		p61_ioctl_config_spi_gpio(p61_dev);
		break;
	case P61_ENABLE_SPI_CLK:
		pr_info("%s P61_ENABLE_SPI_CLK\n", __func__);
		ret = p61_set_clk(p61_dev);
		break;
	case P61_DISABLE_SPI_CLK:
		pr_info("%s P61_DISABLE_SPI_CLK\n", __func__);
		ret = p61_disable_clk(p61_dev);
		break;

	case P61_RW_SPI_DATA:
		ret = p61_rw_spi_message(p61_dev, arg);
		break;

	default:
		pr_info("%s no matching ioctl!\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Called when a process closes the device file.
 */
static int p61_dev_release(struct inode *inode, struct file *file)
{
	struct p61_dev *p61_dev = file->private_data;

	pr_info("[ESE]: %s\n", __FUNCTION__);

	if (p61_dev->enabled_clk) {
		pr_info("%s: [NFC-ESE] disable clock.\n", __func__);
		p61_disable_clk(p61_dev);
	}
	if (wake_lock_active(&p61_dev->ese_lock)) {
		pr_info("%s: [NFC-ESE] wake unlock.\n", __func__);
		wake_unlock(&p61_dev->ese_lock);
	}
	return 0;
}

/**
 * \ingroup spi_driver
 * \brief Write data to P61 on SPI
 *
 * \param[in]       struct file *
 * \param[in]       const char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval data size
 *
*/

static ssize_t p61_dev_write(struct file *filp, const char *buf, size_t count,
        loff_t *offset)
{
	int ret = -1;
	struct p61_dev *p61_dev;
	unsigned char tx_buffer[MAX_BUFFER_SIZE];

	P61_DBG_MSG(KERN_ALERT "p61_dev_write -Enter count %zu\n", count);

	p61_dev = filp->private_data;

	mutex_lock(&p61_dev->write_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(&tx_buffer[0], 0, sizeof(tx_buffer));
	if (copy_from_user(&tx_buffer[0], &buf[0], count))
	{
		P61_ERR_MSG("%s : failed to copy from user space\n", __func__);
		mutex_unlock(&p61_dev->write_mutex);
		return -EFAULT;
	}
	/* Write data */
	ret = spi_write(p61_dev->spi, &tx_buffer[0], count);
	if (ret < 0)
	{
		ret = -EIO;
	}
	else
	{
		ret = count;
	}

	mutex_unlock(&p61_dev->write_mutex);
	P61_DBG_MSG(KERN_ALERT "p61_dev_write ret %d- Exit \n", ret);
	return ret;
}

#ifdef P61_IRQ_ENABLE

/**
 * \ingroup spi_driver
 * \brief To disable IRQ
 *
 * \param[in]       struct p61_dev *
 *
 * \retval void
 *
*/

static void p61_disable_irq(struct p61_dev *p61_dev)
{
	unsigned long flags;

	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);

	spin_lock_irqsave(&p61_dev->irq_enabled_lock, flags);
	if (p61_dev->irq_enabled)
	{
		disable_irq_nosync(p61_dev->spi->irq);
		p61_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&p61_dev->irq_enabled_lock, flags);

	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

/**
 * \ingroup spi_driver
 * \brief Will get called when interrupt line asserted from P61
 *
 * \param[in]       int
 * \param[in]       void *
 *
 * \retval IRQ handle
 *
*/

static irqreturn_t p61_dev_irq_handler(int irq, void *dev_id)
{
	struct p61_dev *p61_dev = dev_id;

	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
	p61_disable_irq(p61_dev);

	/* Wake up waiting readers */
	wake_up(&p61_dev->read_wq);

	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return IRQ_HANDLED;
}
#endif

/**
 * \ingroup spi_driver
 * \brief Used to read data from P61 in Poll/interrupt mode configured using ioctl call
 *
 * \param[in]       struct file *
 * \param[in]       char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval read size
 *
*/

static ssize_t p61_dev_read(struct file *filp, char *buf, size_t count,
        loff_t *offset)
{
	int ret = -EIO;
	struct p61_dev *p61_dev = filp->private_data;
	unsigned char sof = 0x00;
	int total_count = 0;
	unsigned char rx_buffer[MAX_BUFFER_SIZE];

	P61_DBG_MSG("p61_dev_read count %zu - Enter \n", count);

	if (count < MAX_BUFFER_SIZE)
	{
		P61_ERR_MSG(KERN_ALERT "Invalid length (min : 258) [%zu] \n", count);
		return -EINVAL;
	}
	mutex_lock(&p61_dev->read_mutex);
	if (count > MAX_BUFFER_SIZE)
	{
		count = MAX_BUFFER_SIZE;
	}

	memset(&rx_buffer[0], 0x00, sizeof(rx_buffer));

	if (p61_dev->enable_poll_mode)
	{
		P61_DBG_MSG(" %s Poll Mode Enabled \n", __FUNCTION__);
		do
		{
			sof = 0x00;
			P61_DBG_MSG(KERN_INFO"SPI_READ returned 0x%x", sof);
			ret = spi_read(p61_dev->spi, (void *)&sof, 1);
			if (0 > ret)
			{
			P61_ERR_MSG(KERN_ALERT "spi_read failed [SOF] \n");
			goto fail;
			}
			P61_DBG_MSG(KERN_INFO"SPI_READ returned 0x%x", sof);
			/* if SOF not received, give some time to P61 */
			/* RC put the conditional delay only if SOF not received */
			if (sof != SOF)
			usleep_range(5000, 5100);
		}while(sof != SOF);
	}
	else
	{
#ifdef P61_IRQ_ENABLE
		P61_DBG_MSG(" %s Interrrupt Mode Enabled \n", __FUNCTION__);
		if (!gpio_get_value(p61_dev->irq_gpio))
		{
			if (filp->f_flags & O_NONBLOCK)
			{
				ret = -EAGAIN;
				goto fail;
			}
			while (1)
			{
				P61_DBG_MSG(" %s waiting for interrupt \n", __FUNCTION__);
				p61_dev->irq_enabled = true;
				enable_irq(p61_dev->spi->irq);
				ret = wait_event_interruptible(p61_dev->read_wq,!p61_dev->irq_enabled);
				p61_disable_irq(p61_dev);
				if (ret)
				{
					P61_ERR_MSG("wait_event_interruptible() : Failed\n");
					goto fail;
				}

				if (gpio_get_value(p61_dev->irq_gpio))
				break;

				P61_ERR_MSG("%s: spurious interrupt detected\n", __func__);
			}
		}
#else
	P61_ERR_MSG(" %s P61_IRQ_ENABLE not Enabled \n", __FUNCTION__);
#endif
        /* read the SOF */
        sof = 0x00;
        ret = spi_read(p61_dev->spi, (void *)&sof, 1);
        if ((0 > ret) || (sof != SOF))
        {
            P61_DBG_MSG(KERN_INFO"SPI_READ returned 0x%x", sof);
            P61_ERR_MSG(KERN_ALERT "spi_read failed [SOF] 0x%x\n", ret);
            ret = -EIO;
            goto fail;
		}
	}


	total_count = 1;
	rx_buffer[0] = sof;
	/* Read the HEADR of Two bytes*/
	ret = spi_read(p61_dev->spi, (void *)&rx_buffer[1], 2);
	if (ret < 0)
	{
		P61_ERR_MSG(KERN_ALERT "spi_read fails after [PCB] \n");
		ret = -EIO;
		goto fail;
	}

	total_count += 2;
	/* Get the data length */
	count = rx_buffer[2];
	P61_DBG_MSG(KERN_INFO"Data Lenth = %zu", count);
	/* Read the availabe data along with one byte LRC */
	ret = spi_read(p61_dev->spi, (void *)&rx_buffer[3], (count+1));
	if (ret < 0)
	{
		P61_ERR_MSG("spi_read failed \n");
		ret = -EIO;
		goto fail;
	}
	total_count = (total_count + (count+1));
	P61_DBG_MSG(KERN_INFO"total_count = %d", total_count);

	if (copy_to_user(buf, &rx_buffer[0], total_count))
	{
		P61_ERR_MSG("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
		goto fail;
	}
	ret = total_count;
	P61_DBG_MSG("p61_dev_read ret %d Exit\n", ret);

	mutex_unlock(&p61_dev->read_mutex);

	return ret;

	fail:
	P61_ERR_MSG("Error p61_dev_read ret %d Exit\n", ret);
	mutex_unlock(&p61_dev->read_mutex);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief It will configure the GPIOs required for soft reset, read interrupt & regulated power supply to P61.
 *
 * \param[in]       struct p61_spi_platform_data *
 * \param[in]       struct p61_dev *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
*/
#if 0
static int p61_hw_setup(struct p61_spi_platform_data *platform_data,
       struct p61_dev *p61_dev, struct spi_device *spi)
{
    int ret = -1;

    P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
#ifdef P61_IRQ_ENABLE
    ret = gpio_request(platform_data->irq_gpio, "p61 irq");
    if (ret < 0)
    {
        P61_ERR_MSG("gpio request failed gpio = 0x%x\n", platform_data->irq_gpio);
        goto fail;
    }

    ret = gpio_direction_input(platform_data->irq_gpio);
    if (ret < 0)
    {
        P61_ERR_MSG("gpio request failed gpio = 0x%x\n", platform_data->irq_gpio);
        goto fail_irq;
    }
#endif

#ifdef P61_HARD_RESET
    /* RC : platform specific settings need to be declare */
    p61_regulator = regulator_get( &spi->dev, "vaux3");
    if (IS_ERR(p61_regulator))
    {
        ret = PTR_ERR(p61_regulator);
        P61_ERR_MSG(" Error to get vaux3 (error code) = %d\n", ret);
        return  -ENODEV;
    }
    else
    {
        P61_DBG_MSG("successfully got regulator\n");
    }

    ret = regulator_set_voltage(p61_regulator, 1800000, 1800000);
    if (ret != 0)
    {
        P61_ERR_MSG("Error setting the regulator voltage %d\n", ret);
        regulator_put(p61_regulator);
        return ret;
    }
    else
    {
        regulator_enable(p61_regulator);
        P61_DBG_MSG("successfully set regulator voltage\n");

    }
#endif
#ifdef P61_RST_ENABLE
    ret = gpio_request( platform_data->rst_gpio, "p61 reset");
    if (ret < 0)
    {
        P61_ERR_MSG("gpio reset request failed = 0x%x\n", platform_data->rst_gpio);
        goto fail_gpio;
    }

    ret = gpio_direction_output(platform_data->rst_gpio,0);
    if (ret < 0)
    {
        P61_ERR_MSG("gpio rst request failed gpio = 0x%x\n", platform_data->rst_gpio);
        goto fail_gpio;
    }
#endif

    ret = 0;
    P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
    return ret;

#ifdef P61_RST_ENABLE
    fail_gpio:
    gpio_free(platform_data->rst_gpio);
#endif
#ifdef P61_IRQ_ENABLE
    fail_irq:
    gpio_free(platform_data->irq_gpio);
    fail:
    P61_ERR_MSG("p61_hw_setup failed\n");
#endif
    return ret;
}
#endif

/**
 * \ingroup spi_driver
 * \brief Set the P61 device specific context for future use.
 *
 * \param[in]       struct spi_device *
 * \param[in]       void *
 *
 * \retval void
 *
*/

static inline void p61_set_data(struct spi_device *spi, void *data)
{
	dev_set_drvdata(&spi->dev, data);
}

/**
 * \ingroup spi_driver
 * \brief Get the P61 device specific context.
 *
 * \param[in]       const struct spi_device *
 *
 * \retval Device Parameters
 *
*/

static inline void *p61_get_data(const struct spi_device *spi)
{
	return dev_get_drvdata(&spi->dev);
}

/* possible fops on the p61 device */
static const struct file_operations p61_dev_fops = {
        .owner = THIS_MODULE,
        .read = p61_dev_read,
        .write = p61_dev_write,
        .open = p61_dev_open,
        .unlocked_ioctl = p61_dev_ioctl,
	.release = p61_dev_release,
};

#if 0
static ssize_t p61_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	//struct spi_device *spi = to_spi_device(dev);

	//ret = spi_read(p61_dev->spi, (void *)&sof, 1);

	pr_info("%s\n", __func__);
	data='a';
	return sprintf(buf, "%d\n", data);
}

static ssize_t p61_test_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	//struct spi_device *spi = to_spi_device(dev);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	pr_info("%s [%lu]\n", __func__, data);

	return count;
}

static DEVICE_ATTR(test, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		p61_test_show, p61_test_store);
#endif

static int p61_parse_dt(struct device *dev,
	struct p61_dev *data)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int errorno = 0;
	//int gpio;
#if 0
	gpio = of_get_named_gpio_flags(np, "vfsspi-drdyPin",
		0, &flags);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->drdy_pin = gpio;
		pr_info("%s: drdyPin=%d\n",
			__func__, data->drdy_pin);
	}
#endif

#if 1//def ENABLE_SENSORS_FPRINT_SECURE
	if (of_property_read_u32(np, "p61-mosipin", &data->mosipin))
		data->mosipin = 0;
	if (of_property_read_u32(np, "p61-misopin", &data->misopin))
		data->misopin = 0;
#if 0
	if (of_property_read_u32(np, "p61-cspin", &data->cspin))
		data->cspin = 0;
#else
	 data->cspin = of_get_named_gpio_flags(np, "p61-cspin",
		0, &flags);

#endif
	if (of_property_read_u32(np, "p61-clkpin", &data->clkpin))
		data->clkpin = 0;

	//data->tz_mode = true;
#endif

	pr_info("%s: mosipin=%d, %d, %d, %d\n", __func__,
		data->mosipin, data->misopin, data->cspin, data->clkpin);
//dt_exit:
	return errorno;
}

/**
 * \ingroup spi_driver
 * \brief To probe for P61 SPI interface. If found initialize the SPI clock, bit rate & SPI mode. 
		  It will create the dev entry (P61) for user space.
 *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
*/

static int p61_probe(struct spi_device *spi)
{
	int ret = -1;
	//struct p61_spi_platform_data *platform_data = NULL;
	//struct p61_spi_platform_data platform_data1;
	struct p61_dev *p61_dev = NULL;
#ifdef P61_IRQ_ENABLE
	unsigned int irq_flags;
#endif

	P61_DBG_MSG("%s chip select : %d , bus number = %d \n",
		__FUNCTION__, spi->chip_select, spi->master->bus_num);

	//platform_data = spi->dev.platform_data;
#if 0
	if (platform_data == NULL)
	{
		/* RC : rename the platformdata1 name */
/* TBD: This is only for Panda as we are passing NULL for platform data */
		P61_ERR_MSG("%s : p61 probe fail\n", __func__);
		//platform_data1.irq_gpio = P61_IRQ;
		//platform_data1.rst_gpio = P61_RST;
		//platform_data = &platform_data1;
		P61_ERR_MSG("%s : p61 probe fail1\n", __func__);
		return  -ENODEV;
	}
#endif
	p61_dev = kzalloc(sizeof(*p61_dev), GFP_KERNEL);
	if (p61_dev == NULL)
	{
		P61_ERR_MSG("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = p61_parse_dt(&spi->dev, p61_dev);
	if (ret) {
		pr_err("%s - Failed to parse DT\n", __func__);
		goto p61_parse_dt_failed;
	}
	pr_info("%s: tz_mode=%d, isGpio_cfgDone:%d\n", __func__,
			p61_dev->tz_mode, p61_dev->isGpio_cfgDone);

#if 0
	ret = p61_hw_setup (platform_data, p61_dev, spi);
	if (ret < 0)
	{
		P61_ERR_MSG("Failed to p61_enable_P61_IRQ_ENABLE\n");
		goto err_exit0;
	}
#endif

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = P61_SPI_CLOCK;
	//spi->chip_select = SPI_NO_CS;
	ret = spi_setup(spi);
	if (ret < 0)
	{
		P61_ERR_MSG("failed to do spi_setup()\n");
		goto p61_spi_setup_failed;
	}

	p61_dev -> spi = spi;
	p61_dev -> p61_device.minor = MISC_DYNAMIC_MINOR;
	p61_dev -> p61_device.name = "p61";
	p61_dev -> p61_device.fops = &p61_dev_fops;
	p61_dev -> p61_device.parent = &spi->dev;
	// p61_dev->irq_gpio = platform_data->irq_gpio;
	//p61_dev->rst_gpio  = platform_data->rst_gpio;


	dev_set_drvdata(&spi->dev, p61_dev);

	/* init mutex and queues */
	init_waitqueue_head(&p61_dev->read_wq);
	mutex_init(&p61_dev->read_mutex);
	mutex_init(&p61_dev->write_mutex);
	spin_lock_init(&p61_dev->ese_spi_lock);


#ifdef P61_IRQ_ENABLE
	spin_lock_init(&p61_dev->irq_enabled_lock);
#endif

	wake_lock_init(&p61_dev->ese_lock, WAKE_LOCK_SUSPEND, "ese_wake_lock");
	ret = misc_register(&p61_dev->p61_device);
	if (ret < 0)
	{
		P61_ERR_MSG("misc_register failed! %d\n", ret);
		goto err_exit0;
	}

#if 0 //test
{
	struct device *dev;

	p61_device_class = class_create(THIS_MODULE, "ese");
	if (IS_ERR(p61_device_class)) {
		pr_err("%s class_create() is failed:%lu\n",
			__func__,  PTR_ERR(p61_device_class));
		//status = PTR_ERR(p61_device_class);
		//goto vfsspi_probe_class_create_failed;
	}
	dev = device_create(p61_device_class, NULL,
			    0, p61_dev, "p61");
		pr_err("%s device_create() is failed:%lu\n",
			__func__,  PTR_ERR(dev));

	if ((device_create_file(dev, &dev_attr_test)) < 0)
		pr_err("%s device_create_file failed !!!\n", __func__);
	else
		pr_info("%s device_create_file success.\n", __func__);
	//ret = sysfs_create_group(&spi->dev.kobj,
	//		&p61_attribute_group);
	//if (ret < 0)
	//	pr_err("%s class_create() is failed - \n",
	//		__func__,  PTR_ERR(p61_device_class));
}
#endif

#ifdef P61_IRQ_ENABLE
	p61_dev->spi->irq = gpio_to_irq(platform_data->irq_gpio);

	if ( p61_dev->spi->irq < 0)
	{
		P61_ERR_MSG("gpio_to_irq request failed gpio = 0x%x\n",
				platform_data->irq_gpio);
		goto err_exit1;
	}
	/* request irq.  the irq is set whenever the chip has data available
	* for reading.  it is cleared when all data has been read.
	*/
	p61_dev->irq_enabled = true;
	irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;

	ret = request_irq(p61_dev->spi->irq, p61_dev_irq_handler,
			irq_flags, p61_dev -> p61_device.name, p61_dev);
	if (ret)
	{
		P61_ERR_MSG("request_irq failed\n");
		goto err_exit1;
	}
	p61_disable_irq(p61_dev);
#endif

	p61_dev-> enable_poll_mode = 1; /* Default IRQ read mode */
	P61_DBG_MSG("%s finished...\n", __FUNCTION__);
	return ret;

	//err_exit1:
	misc_deregister(&p61_dev->p61_device);
	err_exit0:
	mutex_destroy(&p61_dev->read_mutex);
	mutex_destroy(&p61_dev->write_mutex);
        wake_lock_destroy(&p61_dev->ese_lock);

	p61_spi_setup_failed:
	p61_parse_dt_failed:
	if(p61_dev != NULL)
		kfree(p61_dev);
	err_exit:
	P61_DBG_MSG("ERROR: Exit : %s ret %d\n", __FUNCTION__, ret);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Will get called when the device is removed to release the resources.
 *
 * \param[in]       struct spi_device
 *
 * \retval 0 if ok.
 *
*/

static int p61_remove(struct spi_device *spi)
{
	struct p61_dev *p61_dev = p61_get_data(spi);
	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);

#ifdef P61_HARD_RESET
	if (p61_regulator != NULL)
	{
		regulator_disable(p61_regulator);
		regulator_put(p61_regulator);
	}
	else
	{
		P61_ERR_MSG("ERROR %s p61_regulator not enabled \n", __FUNCTION__);
	}
#endif
#ifdef P61_RST_ENABLE
	gpio_free(p61_dev->rst_gpio);
#endif

#ifdef P61_IRQ_ENABLE
	free_irq(p61_dev->spi->irq, p61_dev);
	gpio_free(p61_dev->irq_gpio);
#endif

	mutex_destroy(&p61_dev->read_mutex);
	misc_deregister(&p61_dev->p61_device);
        wake_lock_destroy(&p61_dev->ese_lock);
	if(p61_dev != NULL)
		kfree(p61_dev);
	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id p61_match_table[] = {
	{ .compatible = "p61",},
	{},
};
#else
#define ese_match_table NULL
#endif

static struct spi_driver p61_driver = {
	.driver = {
		.name = "p61",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = p61_match_table,
#endif
        },
	.probe =  p61_probe,
	.remove = p61_remove,
};

/**
 * \ingroup spi_driver
 * \brief Module init interface
 *
 * \param[in]       void
 *
 * \retval handle
 *
*/

static int __init p61_dev_init(void)
{
	debug_level = P61_FULL_DEBUG;

	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);

	return spi_register_driver(&p61_driver);

	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

/**
 * \ingroup spi_driver
 * \brief Module exit interface
 *
 * \param[in]       void
 *
 * \retval void
 *
*/

static void __exit p61_dev_exit(void)
{
	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);

	spi_unregister_driver(&p61_driver);
	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

module_init( p61_dev_init);
module_exit( p61_dev_exit);

MODULE_AUTHOR("BHUPENDRA PAWAR");
MODULE_DESCRIPTION("NXP P61 SPI driver");
MODULE_LICENSE("GPL");

/** @} */
