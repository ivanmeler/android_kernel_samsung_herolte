/*
 * SAMSUNG NFC Controller
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Woonki Lee <woonki84.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

 /*
 * Change Log: 27-Nov-2014.
 * Implementation of the pinctrl changes - naveen.krish@samsung.com
 */

#ifdef	CONFIG_SEC_NFC_I2C_GPIO
#define CONFIG_SEC_NFC_I2C
#endif

#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#ifdef CONFIG_SEC_NFC_I2C
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#endif

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/nfc/sec_nfc.h>
#include <linux/of_gpio.h>

/*
 * Security START
 *	#include <mach/scm.h>
 *	Needs to be changed as per the qualcomm's recommendation
 * Security END
 */

#include <linux/regulator/consumer.h>

#include <linux/wakelock.h>

#ifndef BU80003GUL

#define BU80003GUL
#define BU80003GUL_DEBUG

#ifdef BU80003GUL

#include "bu80003gul.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i2c.h>

#ifdef BU80003GUL_DEBUG
#define EPC_DEBUG(fmt, args...)		printk(KERN_DEBUG fmt, ## args)
#define EPC_INFO(fmt, args...)		printk(KERN_INFO fmt, ## args)
#define EPC_ERR(fmt, args...)		printk(KERN_ERR fmt, ## args)
#else
#define EPC_DEBUG(fmt, args...)
#define EPC_INFO(fmt, args...)
#define EPC_ERR(fmt, args...)
#endif


#define I2C_ADDR (0x56 >> 1)
#define I2C_LOCK_ADDR 0x02
#define I2C_ANT_ADDR 0x01

extern int poweroff_charging;
/*	extern unsigned int system_rev;  Not using system_rev anymore	*/

static unsigned int tvdd_gpio = -1;

#undef FEATURE_SET_DEFAULT_ANT_VAL

#ifdef CONFIG_NFC_EDC_TUNING
static unsigned char user_ant = 10;
#endif
static int felica_epc_ant_read(unsigned char *read_buff);
static int felica_epc_ant_write(char ant);
int felica_epc_reset(void);

/*
 *	I2C device_id table
*/
static struct i2c_client *bu80003gul_i2c_client;
static const struct i2c_device_id bu80003gul_i2c_idtable[] = {
	{BU80003GUL_I2C_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, bu80003gul_i2c_idtable);

/*
 * I2C match table
 */
static struct of_device_id bu80003gul_i2c_match_table[] = {
	{ .compatible = "felica,felica-i2c",},
	{},
};

/*
 * I2C driver
 */
static struct i2c_driver bu80003gul_i2c_driver = {
	.probe = bu80003gul_i2c_probe,
	.remove = bu80003gul_i2c_remove,
	.id_table = bu80003gul_i2c_idtable,
	.driver = {
				.name = BU80003GUL_I2C_NAME,
				.owner = THIS_MODULE,
				.of_match_table = bu80003gul_i2c_match_table,
			  },
};


/*
 * Name : bu80003gul_i2c_probe
 * Description : Probe the I2C device.
 */
static int bu80003gul_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *devid)
{
	int ret;

	EPC_DEBUG("[MFDD] %s START", __func__);

	bu80003gul_i2c_client = client;
	if (!bu80003gul_i2c_client) {
		EPC_ERR("[MFDD] %s ERROR(bu80003gul_i2c_client==NULL)",
					__func__);
		return -EINVAL;
	}

	ret = felica_epc_register();
	if (ret < 0) {
		EPC_ERR("[MFDD] %s Failed to register the device[ret:%d]\n",
				__func__, ret);
	}

#ifdef FEATURE_SET_DEFAULT_ANT_VAL
	/* set default value temporarily */
	ret = felica_epc_ant_write(FEATURE_SET_DEFAULT_ANT_VAL);
	if (ret < 0) {
		EPC_ERR("[MFDD] %s felica_epc_ant_write fail, ret=[%d]",
				__func__, ret);
		return -EFAULT;
	}
#endif
#ifdef CONFIG_NFC_EDC_TUNING
        ret = felica_epc_reset();
        if (ret < 0) {
                EPC_ERR("[MFDD] %s felica_epc_reset fail, ret=[%d]",
                                __func__, ret);
        }
	ret = felica_epc_ant_read(&user_ant);
	if (ret < 0) {
		EPC_ERR("[MFDD] %s felica_epc_ant_read fail, ret=[%d]",
				__func__, ret);
		user_ant = 10;
		return -EFAULT;
	}
	pr_info("%s : felica_ant : %d\n", __func__, user_ant);
#endif

	EPC_DEBUG("[MFDD] %s END", __func__);

	return 0;
}

/*
 * Name : bu80003gul_i2c_remove
 * Description : Remove the I2C device.
 */
static int bu80003gul_i2c_remove(struct i2c_client *client)
{
	bu80003gul_i2c_client = NULL;
	EPC_DEBUG("[MFDD] %s END", __func__);
	return 0;
}

/* felica_ant device related */

static dev_t dev_id_felica_epc;
static struct cdev cdev_felica_epc;
static const struct file_operations fops_felica_epc = {
	.owner = THIS_MODULE,
	.open = felica_epc_open,
	.read = felica_epc_read,
	.write = felica_epc_write,
	/* .unlocked_ioctl = felica_epc_ioctl, */
	.release = felica_epc_close,
};

int felica_epc_set_lock_state(int state)
{
	int ret;
	unsigned char write_buff[2];
	struct i2c_msg write_msgs[] = {
		{
			.addr	= I2C_ADDR,
			.flags	= 0,
			.len	= 2,
			.buf	= NULL,
		},
	};

	write_buff[0] = 0x02;
	write_buff[1] = state;
	write_msgs[0].buf = &write_buff[0];

	ret = i2c_transfer(bu80003gul_i2c_client->adapter, write_msgs, 1);
	if (ret < 0) {
		EPC_ERR(" %s ERROR(i2c_transfer), ret=[%d]",
				__func__, ret);
		return -EIO;
	}

	return ret;
}

int felica_epc_reset(void)
{
	int ret;
	unsigned char write_buff[2];
	struct i2c_msg write_msgs[] = {
		{
			.addr	= I2C_ADDR,
			.flags	= 0,
			.len	= 2,
			.buf	= NULL,
		},
	};

	write_buff[0] = 0x00;
	write_buff[1] = 1;
	write_msgs[0].buf = &write_buff[0];

	ret = i2c_transfer(bu80003gul_i2c_client->adapter, write_msgs, 1);
	if (ret < 0) {
		EPC_ERR(" %s ERROR(i2c_transfer), ret=[%d]",
				__func__, ret);
		return -EIO;
	}

	return ret;
}
static int felica_epc_register(void)
{
	struct device *device_felica_epc;
	int ret;

	dev_id_felica_epc = MKDEV(FELICA_EPC_MAJOR, FELICA_EPC_MINOR);
	ret = alloc_chrdev_region(&dev_id_felica_epc, FELICA_EPC_BASEMINOR,
				FELICA_EPC_MINOR_COUNT, FELICA_EPC_NAME);
	if (ret < 0) {
		EPC_ERR("[MFDD] %s ERROR(alloc_chrdev_region), ret=[%d]",
			       __func__, ret);
		return ret;
	}

	cdev_init(&cdev_felica_epc, &fops_felica_epc);
	ret = cdev_add(&cdev_felica_epc, dev_id_felica_epc, FELICA_EPC_MINOR_COUNT);
	if (ret < 0) {
		unregister_chrdev_region(dev_id_felica_epc, FELICA_EPC_MINOR_COUNT);
		EPC_ERR("[MFDD] %s ERROR(cdev_add), ret=[%d]", __func__, ret);
		return -EIO;
	}

	eeprom_class = class_create(THIS_MODULE, "felica_eeprom");
	if (IS_ERR(eeprom_class)) {
		EPC_ERR("[MFDD] %s ERROR(class_create)", __func__);
		return PTR_ERR(eeprom_class);
	}

	device_felica_epc = device_create(eeprom_class, NULL,
								dev_id_felica_epc, NULL,
								FELICA_EPC_NAME);
	if (IS_ERR(device_felica_epc)) {
		cdev_del(&cdev_felica_epc);
		unregister_chrdev_region(dev_id_felica_epc, FELICA_EPC_MINOR_COUNT);
		EPC_ERR("[MFDD] %s ERROR(device_create)", __func__);
		return -EINVAL;
	}

	EPC_DEBUG("[MFDD] %s END, major=[%d], minor=[%d]", __func__,
			 MAJOR(dev_id_felica_epc), MINOR(dev_id_felica_epc));

	return 0;
}

static void felica_epc_deregister(void)
{
	EPC_DEBUG("[MFDD] %s START", __func__);
	device_destroy(eeprom_class, dev_id_felica_epc);
	cdev_del(&cdev_felica_epc);
	unregister_chrdev_region(dev_id_felica_epc, FELICA_EPC_MINOR_COUNT);
	EPC_DEBUG("[MFDD] %s END", __func__);
	return;
}


static int felica_epc_open(struct inode *inode, struct file *file)
{
	if (felica_epc_set_lock_state(0x81) < 0)
		EPC_ERR("[MFDD} Failed to UnLock the EEPROM.\n");

	return 0;
}

static int felica_epc_close(struct inode *inode, struct file *file)
{
	if (felica_epc_set_lock_state(0x80) < 0)
		EPC_ERR("[MFDD} Failed to UnLock the EEPROM.\n");

	return 0;
}

static int felica_epc_ant_read(unsigned char *read_buff)
{
	int ret;
	unsigned char address = I2C_ANT_ADDR;
	struct i2c_msg read_msgs[] = {
		{
			.addr	= I2C_ADDR,
			.flags	= 0,
			.len	= 1,
			.buf	= &address,
		},
		{
			.addr	= I2C_ADDR,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= read_buff,
		},
	};

	if (bu80003gul_i2c_client == NULL) {
		EPC_ERR("[MFDD] bu80003gul_i2c_client is NULL %s -EIO", __func__);
		return -EIO;
	}

	*read_buff = 0;
	ret = i2c_transfer(bu80003gul_i2c_client->adapter, &read_msgs[0], 1);
	if (ret < 0) {
		EPC_ERR("[MFDD] %s ERROR(i2c_transfer[0]), ret=[%d]",
				__func__, ret);
		return -EIO;
	}
	ret = i2c_transfer(bu80003gul_i2c_client->adapter, &read_msgs[1], 1);
	if (ret < 0) {
		EPC_ERR("[MFDD] %s ERROR(i2c_transfer[1]), ret=[%d]",
				__func__, ret);
		return -EIO;
	}

	pr_info("%s : ant : %d\n", __func__, *read_buff);

	return 0;

}

static int felica_epc_ant_write(char ant)
{
	int ret = 0;
	char write_buff[2];
	struct i2c_msg write_msgs[] = {
		{
			.addr	= I2C_ADDR,
			.flags	= 0,
			.len	= 2,
			.buf	= NULL,
		},
	};

	if (bu80003gul_i2c_client == NULL) {
		EPC_ERR("[MFDD] bu80003gul_i2c_client is NULL %s", __func__);
		return -EIO;
	}

	/* why is bit 7 set ? */
	pr_info("%s : ant : %d\n", __func__, ant&0x7F);

	write_buff[0] = I2C_ANT_ADDR;
	write_buff[1] = ant;
	write_msgs[0].buf = &write_buff[0];

	ret = i2c_transfer(bu80003gul_i2c_client->adapter, &write_msgs[0], 1);
	if (ret < 0) {
		EPC_ERR("[MFDD] %s ERROR(i2c_transfer), ret=[%d]",
				__func__, ret);
		return -EIO;
	}

	return ret;
}

static ssize_t felica_epc_read(struct file *file, char __user *buf,
		size_t len, loff_t *ppos)
{
	int ret;
	unsigned char read_buff = 0;

	ret = felica_epc_ant_read(&read_buff);
	if (ret < 0) {
		EPC_ERR("[MFDD] %s felica_epc_ant_read fail, ret=[%d]",
				__func__, ret);
		return -EFAULT;
	}

	ret = copy_to_user(buf, &read_buff, len);
	if (ret != 0) {
		EPC_ERR("[MFDD] %s ERROR(copy_to_user), ret=[%d]",
				__func__, ret);
		return -EFAULT;
	}

	*ppos += 1;
	EPC_DEBUG("[MFDD] %s END\n", __func__);

	return 1; /* Only one byte at a time will be read. Hence return 1 on success. */

}

static ssize_t felica_epc_write(struct file *file, const char __user *data,
		size_t len, loff_t *ppos)
{
	int ret;
	char ant;

	ret = copy_from_user(&ant, data, len);
	if (ret != 0) {
		EPC_ERR("[MFDD] %s ERROR(copy_from_user), ret=[%d]",
				__func__, ret);
		return -EFAULT;
	}

	ret = felica_epc_ant_write(ant);
	if (ret < 0) {
		EPC_ERR("[MFDD] %s felica_epc_ant_write fail, ret=[%d]",
				__func__, ret);
		return -EFAULT;
	}

#ifdef CONFIG_NFC_EDC_TUNING
	user_ant = ant & 0x7F;
#endif
	EPC_DEBUG("[MFDD] %s END\n", __func__);

	return 1;
}
#endif /* BU80003GUL */

#ifdef CONFIG_NFC_EDC_TUNING
static int ant_tune_req = 0;
static void felica_ant_tuning_work(struct work_struct *work)
{
	int i, ret;
	char ant;

	ant = user_ant;
	for (i = 0; i < 10; i++) {
		if (ant_tune_req != 1)
			break;
		ant = ant > 2 ? ant - 2 : 1;
		ret = felica_epc_ant_write(ant);
		pr_info("%s : felica_tune_work ant: %d\n", __func__, ant);
		msleep(1000);
	}
	ret = felica_epc_ant_write(user_ant);
	ant_tune_req = 0;
}
static DECLARE_DELAYED_WORK(felica_ant_work, felica_ant_tuning_work);
int felica_ant_tuning(int evt)
{
	pr_info("%s : felica_tune_req : %d, event: %d\n", __func__, ant_tune_req, evt);

	ant_tune_req = evt;
	if (evt == 1) {
		schedule_delayed_work(&felica_ant_work, 0);
	}

	return 1;
}
EXPORT_SYMBOL(felica_ant_tuning);
#endif

#ifdef	CONFIG_SEC_NFC_I2C
enum sec_nfc_irq {
	SEC_NFC_NONE,
	SEC_NFC_INT,
	SEC_NFC_TRY_AGAIN,
};
#endif

struct sec_nfc_info {
	struct miscdevice miscdev;
	struct mutex mutex;
	enum sec_nfc_state state;
	struct device *dev;
	struct sec_nfc_platform_data *pdata;
	struct wake_lock wake_lock;

#ifdef	CONFIG_SEC_NFC_I2C
	struct i2c_client *i2c_dev;
	struct mutex read_mutex;
	enum sec_nfc_irq read_irq;
	wait_queue_head_t read_wait;
	size_t buflen;
	u8 *buf;
#endif
};

#ifdef	CONFIG_SEC_NFC_I2C
static irqreturn_t sec_nfc_irq_thread_fn(int irq, void *dev_id)
{
	struct sec_nfc_info *info = dev_id;

	dev_dbg(info->dev, "IRQ\n");

	mutex_lock(&info->read_mutex);
	info->read_irq = SEC_NFC_INT;
	mutex_unlock(&info->read_mutex);

	wake_up_interruptible(&info->read_wait);

	return IRQ_HANDLED;
}

#endif

static int sec_nfc_set_state(struct sec_nfc_info *info,
					enum sec_nfc_state state)
{
	struct sec_nfc_platform_data *pdata = info->pdata;

	/* intfo lock is aleady getten before calling this function */
	info->state = state;

	gpio_set_value(pdata->ven, 0);
	gpio_set_value(pdata->firm, 0);

	if (state == SEC_NFC_ST_FIRM) {
		gpio_set_value(pdata->firm, 1);
		gpio_set_value(pdata->ven, 0);
	}

	msleep(SEC_NFC_VEN_WAIT_TIME);
	if (state != SEC_NFC_ST_OFF)
		gpio_set_value(pdata->ven, 1);

	msleep(SEC_NFC_VEN_WAIT_TIME);
	dev_dbg(info->dev, "Power state is : %d\n", state);

	return 0;
}

#ifdef CONFIG_SEC_NFC_I2C
static int sec_nfc_reset(struct sec_nfc_info *info)
{
	dev_err(info->dev, "i2c failed. return resatrt to M/W\n");

	sec_nfc_set_state(info, SEC_NFC_ST_NORM);

	return -ERESTART;
}

static ssize_t sec_nfc_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	enum sec_nfc_irq irq;
	int ret = 0;

	dev_dbg(info->dev, "%s: info: %p, count: %zu\n", __func__,
		info, count);

	mutex_lock(&info->mutex);

	if (info->state == SEC_NFC_ST_OFF) {
		dev_err(info->dev, "sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	mutex_lock(&info->read_mutex);
	irq = info->read_irq;
	mutex_unlock(&info->read_mutex);
	if (irq == SEC_NFC_NONE) {
		if (file->f_flags & O_NONBLOCK) {
			dev_err(info->dev, "it is nonblock\n");
			ret = -EAGAIN;
			goto out;
		}
	}
	dev_err(info->dev, "LWK irq %d\n", irq);

	/* i2c recv */
	if (count > info->buflen)
		count = info->buflen;

	if (count < SEC_NFC_MSG_MIN_SIZE || count > SEC_NFC_MSG_MAX_SIZE) {
		dev_err(info->dev, "user required wrong size\n");
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&info->read_mutex);
	ret = i2c_master_recv(info->i2c_dev, info->buf, count);
	dev_err(info->dev, "recv size : %d\n", ret);

	if (ret == -EREMOTEIO) {
		ret = sec_nfc_reset(info);
		goto read_error;
	} else if (ret != count) {
		dev_err(info->dev, "read failed: return: %d count: %d\n",
			ret, count);
		/*	ret = -EREMOTEIO;	*/
		goto read_error;
	}

	info->read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->read_mutex);

	if (copy_to_user(buf, info->buf, ret)) {
		dev_err(info->dev, "copy failed to user\n");
		ret = -EFAULT;
	}

	goto out;

read_error:
	info->read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->read_mutex);
out:
	mutex_unlock(&info->mutex);

	return ret;
}

static ssize_t sec_nfc_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	int ret = 0;

	dev_err(info->dev, "%s: info: %p, count %zu\n", __func__,
		info, count);

	mutex_lock(&info->mutex);

	if (info->state == SEC_NFC_ST_OFF) {
		dev_err(info->dev, "sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	if (count > info->buflen)
		count = info->buflen;

	if (count < SEC_NFC_MSG_MIN_SIZE || count > SEC_NFC_MSG_MAX_SIZE) {
		dev_err(info->dev, "user required wrong size\n");
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(info->buf, buf, count)) {
		dev_err(info->dev, "copy failed from user\n");
		ret = -EFAULT;
		goto out;
	}

/*	usleep_range(6000, 10000);	*/
	usleep_range(600, 1000);

	ret = i2c_master_send(info->i2c_dev, info->buf, count);

	if (ret == -EREMOTEIO) {
		ret = sec_nfc_reset(info);
		goto out;
	}

	if (ret != count) {
		dev_err(info->dev, "send failed: return: %d count: %d\n",
		ret, count);
		ret = -EREMOTEIO;
	}

out:
	mutex_unlock(&info->mutex);

	return ret;
}
#endif

#ifdef CONFIG_SEC_NFC_I2C
static unsigned int sec_nfc_poll(struct file *file, poll_table *wait)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	enum sec_nfc_irq irq;

	int ret = 0;

	dev_dbg(info->dev, "%s: info: %p\n", __func__, info);

	mutex_lock(&info->mutex);

	if (info->state == SEC_NFC_ST_OFF) {
		dev_err(info->dev, "sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	poll_wait(file, &info->read_wait, wait);

	mutex_lock(&info->read_mutex);
	irq = info->read_irq;
	if (irq == SEC_NFC_INT)
		ret = (POLLIN | POLLRDNORM);
	mutex_unlock(&info->read_mutex);

out:
	mutex_unlock(&info->mutex);

	return ret;
}
#endif
#ifdef CONFIG_SEC_NFC_LDO_JPN_CONTROL
static int sec_nfc_regulator_onoff(const char *reg_name, int onoff)
{
	int rc = 0;
	struct regulator *regulator_ldo;

	regulator_ldo = regulator_get(NULL, reg_name);
	if (IS_ERR(regulator_ldo) || regulator_ldo == NULL) {
		pr_err("%s - regulator_get fail\n", __func__);
		return -ENODEV;
	}

	pr_info("%s - onoff = %d\n", __func__, onoff);

	if (onoff == NFC_I2C_LDO_ON) {
		rc = regulator_enable(regulator_ldo);
		if (rc) {
			pr_err("%s - enable regulator_ldo failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_ldo);
		if (rc) {
			pr_err("%s - disable regulator_ldo failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	}

	done:
	regulator_put(regulator_ldo);

	return rc;
}
#endif
static long sec_nfc_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	unsigned int mode = (unsigned int)arg;
	int ret = 0;
	int firm;

	dev_dbg(info->dev, "%s: info: %p, cmd: 0x%x\n",
			__func__, info, cmd);

	mutex_lock(&info->mutex);

	switch (cmd) {
	case SEC_NFC_SET_MODE:
		dev_dbg(info->dev, "%s: SEC_NFC_SET_MODE\n", __func__);

		if (info->state == mode)
			break;

		if (mode >= SEC_NFC_ST_COUNT) {
			dev_err(info->dev, "wrong state (%d)\n", mode);
			ret = -EFAULT;
			break;
		}

		ret = sec_nfc_set_state(info, mode);
		if (ret < 0) {
			dev_err(info->dev, "enable failed\n");
			break;
		}

		break;

	case SEC_NFC_SET_UART_STATE:
		if (mode >= SEC_NFC_ST_COUNT) {
			dev_err(info->dev, "wrong state (%d)\n", mode);
			ret = -EFAULT;
		break;
	}

		firm = gpio_get_value(info->pdata->firm);
		pr_info("%s: [NFC] Firm pin = %d\n", __func__, firm);

		if (mode == SEC_NFC_ST_UART_ON) {
			gpio_set_value(info->pdata->firm, STATE_FIRM_HIGH);
			if (!wake_lock_active(&info->wake_lock)) {
				pr_info("%s: [NFC] wake lock.\n", __func__);
				wake_lock(&info->wake_lock);
			}
		} else if (mode == SEC_NFC_ST_UART_OFF) {
			gpio_set_value(info->pdata->firm, STATE_FIRM_LOW);
			if (wake_lock_active(&info->wake_lock)) {
				pr_info("%s: [NFC] wake unlock after 2 sec.\n", __func__);
				wake_lock_timeout(&info->wake_lock, 2 * HZ);
			}
		}
		else
			ret = -EFAULT;

		firm = gpio_get_value(info->pdata->firm);
		pr_info("%s: [NFC] Mode = %d, Firm pin = %d\n",
				__func__, mode, firm);

		break;
	case SEC_NFC_EDC_SWEEP:
		felica_ant_tuning(1);
		break;

	default:
		dev_err(info->dev, "Unknow ioctl 0x%x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&info->mutex);

	return ret;
}

/*	Security	*/
uint8_t check_custom_kernel(void)
{
#if 0
	uint32_t fuse_id = SEC_NFC_FUSE_ID;
	void *cmd_buf;
	size_t cmd_len;
	size_t resp_len = 0;
	uint8_t resp_buf;

	pr_info(" %s START\n", __func__);

	resp_len = sizeof(resp_buf);
	cmd_buf = (void *)&fuse_id;
	cmd_len = sizeof(fuse_id);

	scm_call(SEC_NFC_SVC_FUSE, SEC_NFC_IS_SW_FUSE_BLOWN_ID, cmd_buf,
						cmd_len, &resp_buf, resp_len);
	pr_info(" %s END resp_buf = %d\n", __func__, resp_buf);

	return resp_buf;
#endif
	return 0;
}
/*	End of Security	*/


static int sec_nfc_parse_dt(struct device *dev,
	struct sec_nfc_platform_data *pdata)
{

	struct device_node *np = dev->of_node;
	pdata->ven = of_get_named_gpio_flags(np, "sec-nfc,pon-gpio",
		0, &pdata->pon_gpio_flags);
	pdata->firm = of_get_named_gpio_flags(np, "sec-nfc,rfs-gpio",
		0, &pdata->rfs_gpio_flags);
	pdata->tvdd = of_get_named_gpio_flags(np, "sec-nfc,tvdd-gpio",
					0, &pdata->tvdd_gpio_flags);
		tvdd_gpio = pdata->tvdd;
#ifdef CONFIG_SEC_NFC_LDO_JPN_CONTROL
	if (of_property_read_string(np, "sec-nfc,i2c_1p8",
		&pdata->i2c_1p8) < 0) {
		pr_err("%s - get i2c_1p8 error\n", __func__);
		pdata->i2c_1p8 = NULL;
	}
	if (of_property_read_string(np, "sec-nfc,regulator_tvdd",
		&pdata->ldo_tvdd) < 0) {
		pr_err("%s - get ldo_tvdd error\n", __func__);
		pdata->ldo_tvdd = NULL;
	}
#endif
	return 0;
}

static int sec_nfc_open(struct inode *inode, struct file *file)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	int ret = 0;
	uid_t uid;
/*	Security	*/
	dev_dbg(info->dev, "%s: info : %p" , __func__, info);

	/*	Check process	*/
	uid = __task_cred(current)->uid;

	if (g_secnfc_uid != uid) {
		dev_err(info->dev,
			"%s: Un-authorized process. No access to device\n", __func__);
		return -EPERM;
	}
/*	End of Security	*/

	mutex_lock(&info->mutex);
	if (info->state != SEC_NFC_ST_OFF) {
		dev_err(info->dev, "sec_nfc is busy\n");
		ret = -EBUSY;
		goto out;
	}

#ifdef CONFIG_SEC_NFC_I2C
	mutex_lock(&info->read_mutex);
	info->read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->read_mutex);
	ret = sec_nfc_set_state(info, SEC_NFC_ST_NORM);
#endif
out:
	mutex_unlock(&info->mutex);
	return ret;
}

static int sec_nfc_close(struct inode *inode, struct file *file)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);

	dev_dbg(info->dev, "%s: info : %p" , __func__, info);

	mutex_lock(&info->mutex);
	sec_nfc_set_state(info, SEC_NFC_ST_OFF);
	if (wake_lock_active(&info->wake_lock)) {
		pr_info("%s: [NFC] wake unlock \n", __func__);
		wake_unlock(&info->wake_lock);
	}
	mutex_unlock(&info->mutex);

	return 0;
}

#if 0
static void sec_nfc_uart_suspend(void)
{
	int ret = 0;
	pr_info("%s: enter  start\n", __func__);
	ret = gpio_tlmm_config(GPIO_CFG(49, GPIOMUX_FUNC_GPIO,
					GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	if (ret)
		pr_err("failed to configure GPIO_23. ret %d\n", ret);

	ret = gpio_tlmm_config(GPIO_CFG(50, GPIOMUX_FUNC_GPIO,
					GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	if (ret)
		pr_err("failed to configure GPIO_24. ret %d\n", ret);

	ret = gpio_tlmm_config(GPIO_CFG(51, GPIOMUX_FUNC_GPIO,
					GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	if (ret)
		pr_err("failed to configure GPIO_25. ret %d\n", ret);

	ret = gpio_tlmm_config(GPIO_CFG(52, GPIOMUX_FUNC_GPIO,
					GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	if (ret)
		pr_err("failed to configure GPIO_26. ret %d\n", ret);

	pr_info("%s: exit\n", __func__);
}
#endif

static const struct file_operations sec_nfc_fops = {
	.owner		= THIS_MODULE,
#ifdef CONFIG_SEC_NFC_I2C
	.read		= sec_nfc_read,
	.write		= sec_nfc_write,
	.poll		= sec_nfc_poll,
#endif
	.open		= sec_nfc_open,
	.release	= sec_nfc_close,
	.unlocked_ioctl	= sec_nfc_ioctl,
#ifdef CONFIG_COMPAT
        .compat_ioctl = sec_nfc_ioctl,
#endif
};

#ifdef CONFIG_PM
static int sec_nfc_suspend(struct device *dev)
{
#ifdef	CONFIG_SEC_NFC_I2C
	struct i2c_client *client = to_i2c_client(dev);
	struct sec_nfc_info *info = i2c_get_clientdata(client);
#else
	struct platform_device *pdev = to_platform_device(dev);
	struct sec_nfc_info *info = platform_get_drvdata(pdev);
#endif
/*	struct sec_nfc_platform_data *pdata = dev->platform_data;	*/

	int ret = 0;

	if (lpcharge)
		return 0;

	mutex_lock(&info->mutex);

	if (info->state == SEC_NFC_ST_FIRM)
		ret = -EPERM;

	mutex_unlock(&info->mutex);

	ret = pinctrl_select_state(info->pdata->nfc_pinctrl,
			info->pdata->nfc_suspend);

	if (ret != 0)
		pr_err("%s: fail to select_state suspend\n", __func__);

	/*	pdata->cfg_gpio();	*/
	return ret;
}

static int sec_nfc_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct sec_nfc_info *info = platform_get_drvdata(pdev);

	ret = pinctrl_select_state(info->pdata->nfc_pinctrl,
			info->pdata->nfc_active);

	if (ret != 0)
		pr_err("%s: fail to select_state Active\n", __func__);

	return ret;
}

static int sec_nfc_pinctrl(struct device *dev,
						struct sec_nfc_platform_data *pdata)
{
	int ret = 0;

	/* get the pinctrl handler for the device */
	pdata->nfc_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->nfc_pinctrl)) {
		pr_debug(" senn3ab does not use pinctrl\n");
		pdata->nfc_pinctrl = NULL;
	} else {

		/* Get the pinctrl for the Active & Suspend states */
		pdata->nfc_suspend = pinctrl_lookup_state(pdata->nfc_pinctrl,
											"sec_nfc_suspend");
		if (IS_ERR(pdata->nfc_suspend)) {
			pr_info("%s fail due to Suspend state not found\n", __func__);
			goto err_exit;
		}

		if (lpcharge) {
			pdata->nfc_active = pinctrl_lookup_state(pdata->nfc_pinctrl,
											"sec_nfc_lpm");
		} else {
		pdata->nfc_active = pinctrl_lookup_state(pdata->nfc_pinctrl,
											"sec_nfc_active");
		}
		if (IS_ERR(pdata->nfc_active)) {
			pr_info("%s fail due to Suspend state not found\n", __func__);
			goto err_exit;
		}

		/* Set the Active State Configuration */
		ret = pinctrl_select_state(pdata->nfc_pinctrl, pdata->nfc_active);
		if (ret != 0) {
			pr_err("%s: fail to select_state active\n", __func__);
			goto err_exit;
		}

		/* Need not call the devm_pinctrl_put() as the handler will be
		automatically freed  when the device is removed */
	}

err_exit:
	return ret;
}


static SIMPLE_DEV_PM_OPS(sec_nfc_pm_ops, sec_nfc_suspend, sec_nfc_resume);
#endif

#ifdef	CONFIG_SEC_NFC_I2C
static int sec_nfc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
#else
static int sec_nfc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
#endif
	struct sec_nfc_info *info = NULL;
	/*	struct sec_nfc_platform_data *pdata;	*/
	struct sec_nfc_platform_data *pdata = NULL;
	int ret = 0;
	int err;

	pr_info("%s: enter - sec-nfc probe start\n", __func__);

	/*	Check tamper	*/
	if (check_custom_kernel() == 1)
		pr_info("%s: The kernel is tampered. Couldn't initialize NFC.\n",
					__func__);

	if (dev) {
		pr_info("%s: alloc for platform data\n", __func__);
		pdata = kzalloc(sizeof(struct sec_nfc_platform_data), GFP_KERNEL);

		if (!pdata) {
			dev_err(dev, "No platform data\n");
			ret = -ENOMEM;
			goto err_pdata;
		}
	} else {
		pr_info("%s: failed alloc platform data\n", __func__);
	}

	err = sec_nfc_parse_dt(dev, pdata);

	/* Naveen : pinctrl changes START */
	ret = sec_nfc_pinctrl(dev, pdata);
	if (ret) {
		dev_err(dev, "sec_nfc_pinctrl failed %d\n", ret);
		kfree(pdata);
		goto err_pinctrl;
	}
	/* Naveen : pinctrl changes END */


		ret = gpio_request(pdata->tvdd, "nfc_tvdd");
		if (ret) {
			dev_err(dev, "failed to get gpio tvdd\n");
			kfree(pdata);
			goto err_pdata;
		}
#if 0
		ret = gpio_tlmm_config(GPIO_CFG(pdata->tvdd, GPIOMUX_FUNC_GPIO,
					GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
		if (ret)
			dev_err(dev, "failed to configure GPIO_41. ret %d\n", ret);
		else {

			if (poweroff_charging) {
				pr_info("%s: [poweroff_charging] Setting the GPIO_41 pin LOW\n", __func__);
				gpio_set_value(pdata->tvdd, 0);
				sec_nfc_uart_suspend();
			} else {
				pr_info("%s: [Normal case] Setting the GPIO_41 pin HIGH\n", __func__);
				gpio_set_value(pdata->tvdd, 1);
			}
			pr_info("%s: Set the GPIO_41 (%d) to HIGH.\n", __func__, pdata->tvdd);
		}
#endif

	pr_info("gpio assign success!\n");

	info = kzalloc(sizeof(struct sec_nfc_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "failed to allocate memory for sec_nfc_info\n");
		ret = -ENOMEM;
		kfree(pdata);
		goto err_info_alloc;
	}
	info->dev = dev;
	info->pdata = pdata;
	info->state = SEC_NFC_ST_OFF;

	mutex_init(&info->mutex);

	dev_set_drvdata(dev, info);

	/*	pdata->cfg_gpio();	*/

#ifdef	CONFIG_SEC_NFC_I2C
	info->buflen = SEC_NFC_MAX_BUFFER_SIZE;
	info->buf = kzalloc(SEC_NFC_MAX_BUFFER_SIZE, GFP_KERNEL);
	if (!info->buf) {
		dev_err(dev,
			"failed to allocate memory for sec_nfc_info->buf\n");
		ret = -ENOMEM;
		kfree(pdata);
		goto err_buf_alloc;
	}
	info->i2c_dev = client;
	info->read_irq = SEC_NFC_NONE;
	mutex_init(&info->read_mutex);
	init_waitqueue_head(&info->read_wait);
	i2c_set_clientdata(client, info);

	ret = request_threaded_irq(pdata->irq, NULL, sec_nfc_irq_thread_fn,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, SEC_NFC_DRIVER_NAME,
			info);
	if (ret < 0) {
		dev_err(dev, "failed to register IRQ handler\n");
		goto err_irq_req;
	}

#endif
#ifdef CONFIG_SEC_NFC_LDO_JPN_CONTROL
	if (pdata->i2c_1p8 != NULL) {
		if(!lpcharge) {
			ret = sec_nfc_regulator_onoff(pdata->i2c_1p8, NFC_I2C_LDO_ON);
			if (ret < 0)
				pr_err("%s max86900_regulator_on fail err = %d\n",
					__func__, ret);
			usleep_range(1000, 1100);
		}
	}

	if (pdata->ldo_tvdd != NULL) {
		if(!lpcharge) {
			ret = sec_nfc_regulator_onoff(pdata->ldo_tvdd, NFC_I2C_LDO_ON);
			if (ret < 0)
			pr_err("%s max86900_regulator_on fail err = %d\n",
					__func__, ret);
			usleep_range(1000, 1100);
		}
	}
#endif
	wake_lock_init(&info->wake_lock, WAKE_LOCK_SUSPEND, "NFCWAKE");
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	info->miscdev.name = SEC_NFC_DRIVER_NAME;
	info->miscdev.fops = &sec_nfc_fops;
	info->miscdev.parent = dev;
	ret = misc_register(&info->miscdev);

	if (ret < 0) {
		dev_err(dev, "failed to register Device\n");
		goto err_dev_reg;
	}

	ret = gpio_request(pdata->ven, "nfc_ven");
	if (ret) {
		dev_err(dev, "failed to get gpio ven\n");
		goto err_gpio_ven;
	}

	ret = gpio_request(pdata->firm, "nfc_firm");
	if (ret) {
		dev_err(dev, "failed to get gpio firm\n");
		goto err_gpio_firm;
	}

	gpio_direction_output(pdata->ven, 0);
	gpio_direction_output(pdata->firm, 0);

	dev_dbg(dev, "%s: info: %p, pdata %p\n", __func__, info, pdata);

#ifdef BU80003GUL
/*	ret = i2c_add_driver(&bu80003gul_i2c_driver);
	if (ret) {
		dev_err(dev,"%s Failed to add the i2c driver. ret:%d\n",
						__func__, ret);
		goto err_gpio_firm;
	}
*/
#endif /* BU80003GUL */
	pr_info("%s: exit - sec-nfc probe finish\n", __func__);

	return 0;

err_gpio_firm:
	gpio_free(pdata->firm);
err_gpio_ven:
	gpio_free(pdata->ven);
err_dev_reg:
#ifdef	CONFIG_SEC_NFC_I2C
err_irq_req:
err_buf_alloc:
#endif
err_info_alloc:
	if (info != NULL) {
		if (info->pdata != NULL)
			kfree(info->pdata);
		wake_lock_destroy(&info->wake_lock);
		kfree(info);
	}
err_pinctrl:
err_pdata:
	pr_info("%s: exit - sec-nfc probe finish with ERROR! - ret = 0x%X\n",
							__func__, ret);
	return ret;
}
static void sec_nfc_lpm_set(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sec_nfc_info *info = platform_get_drvdata(pdev);
	int ret;

	info->pdata->nfc_active = pinctrl_lookup_state(info->pdata->nfc_pinctrl, "sec_nfc_lpm");
	ret = pinctrl_select_state(info->pdata->nfc_pinctrl, info->pdata->nfc_active);
	if (ret != 0)
		pr_err("%s: fail to select_state active\n", __func__);

	if (info->pdata->i2c_1p8 != NULL)
		sec_nfc_regulator_onoff(info->pdata->i2c_1p8, NFC_I2C_LDO_OFF);

}

static void sec_nfc_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

#ifdef CONFIG_NFC_EDC_TUNING
	ant_tune_req = 0;
	flush_delayed_work(&felica_ant_work);
#endif
	sec_nfc_lpm_set(dev);
	gpio_set_value(tvdd_gpio, 0);
}

#ifdef	CONFIG_SEC_NFC_I2C
static int sec_nfc_remove(struct i2c_client *client)
{
	struct sec_nfc_info *info = i2c_get_clientdata(client);
	struct sec_nfc_platform_data *pdata = client->dev.platform_data;
#else
static int sec_nfc_remove(struct platform_device *pdev)
{
	struct sec_nfc_info *info = dev_get_drvdata(&pdev->dev);
	struct sec_nfc_platform_data *pdata = pdev->dev.platform_data;
#endif
	if (info == NULL)
		goto ERR_SEC_NFC_REMOVE_INFO;

	dev_dbg(info->dev, "%s\n", __func__);

	misc_deregister(&info->miscdev);

	if (info->state != SEC_NFC_ST_OFF) {
		gpio_set_value(pdata->firm, 0);
		gpio_set_value(pdata->ven, 0);
	}
	if (info->pdata != NULL)
		kfree(info->pdata);

	kfree(info);
ERR_SEC_NFC_REMOVE_INFO:
	gpio_free(pdata->firm);
	gpio_free(pdata->ven);

#ifdef	CONFIG_SEC_NFC_I2C
	free_irq(pdata->irq, info);
#endif

	wake_lock_destroy(&info->wake_lock);

#ifdef BU80003GUL
	i2c_del_driver(&bu80003gul_i2c_driver);
	felica_epc_deregister();
	class_destroy(eeprom_class);
#endif
	return 0;
}

#ifdef	CONFIG_SEC_NFC_I2C
static struct i2c_device_id sec_nfc_id_table[] = {
#else	/* CONFIG_SEC_NFC_I2C */
static struct platform_device_id sec_nfc_id_table[] = {
#endif
	{ SEC_NFC_DRIVER_NAME, 0 },
	{ }
};

static struct of_device_id nfc_match_table[] = {
	{ .compatible = SEC_NFC_DRIVER_NAME, },
	{},
};

#ifdef	CONFIG_SEC_NFC_I2C
MODULE_DEVICE_TABLE(i2c, sec_nfc_id_table);
static struct i2c_driver sec_nfc_driver = {
#else
MODULE_DEVICE_TABLE(platform, sec_nfc_id_table);
static struct platform_driver sec_nfc_driver = {
#endif
	.probe = sec_nfc_probe,
	.id_table = sec_nfc_id_table,
	.shutdown = sec_nfc_shutdown,
	.remove = sec_nfc_remove,
	.driver = {
		.name = SEC_NFC_DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &sec_nfc_pm_ops,
#endif
		.of_match_table = nfc_match_table,
	},
};

#ifdef	CONFIG_SEC_NFC_I2C
static int __init sec_nfc_init(void)
{
	return i2c_add_driver(&sec_nfc_driver);
}

static void __exit sec_nfc_exit(void)
{
	i2c_del_driver(&sec_nfc_driver);
}
#endif

#ifdef	CONFIG_SEC_NFC_I2C
module_init(sec_nfc_init);
module_exit(sec_nfc_exit);
#else
module_platform_driver(sec_nfc_driver);
module_i2c_driver(bu80003gul_i2c_driver);
#endif


MODULE_DESCRIPTION("Samsung sec_nfc driver");
MODULE_LICENSE("GPL");

#endif /* #ifndef BU80003GUL */
