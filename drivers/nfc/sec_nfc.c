/*
 * SAMSUNG NFC Controller
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Woonki Lee <woonki84.lee@samsung.com>
 *         Heejae Kim <heejae12.kim@samsung.com>
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
 *
 * Last update: 2014-07-15
 *
 */

#include <linux/wait.h>
#include <linux/delay.h>

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/nfc/sec_nfc.h>
#include <linux/clk-provider.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/i2c.h>

#define SEC_NFC_GET_INFO(dev) i2c_get_clientdata(to_i2c_client(dev))
enum sec_nfc_irq {
	SEC_NFC_SKIP = -1,
	SEC_NFC_NONE,
	SEC_NFC_INT,
	SEC_NFC_READ_TIMES,
};

struct sec_nfc_i2c_info {
	struct i2c_client *i2c_dev;
	struct mutex read_mutex;
	enum sec_nfc_irq read_irq;
	wait_queue_head_t read_wait;
	size_t buflen;
	u8 *buf;
};

struct sec_nfc_info {
	struct miscdevice miscdev;
	struct mutex mutex;
	enum sec_nfc_mode mode;
	struct device *dev;
	struct sec_nfc_platform_data *pdata;
	struct sec_nfc_i2c_info i2c_info;
	struct wake_lock nfc_wake_lock;
	bool clk_ctl;
	bool clk_state;
	struct platform_device *pdev;
};

#define FEATURE_SEC_NFC_TEST
#ifdef FEATURE_SEC_NFC_TEST
static struct sec_nfc_info *g_nfc_info;
static bool on_nfc_test;
static bool nfc_int_wait;
#endif
static irqreturn_t sec_nfc_irq_thread_fn(int irq, void *dev_id)
{
	struct sec_nfc_info *info = dev_id;
	struct sec_nfc_platform_data *pdata = info->pdata;

	dev_dbg(info->dev, "[NFC] Read Interrupt is occurred!\n");

#ifdef FEATURE_SEC_NFC_TEST
	if (on_nfc_test) {
		nfc_int_wait = true;
		pr_info("NFC_TEST: interrupt is raised\n");
		wake_up_interruptible(&info->i2c_info.read_wait);
		return IRQ_HANDLED;
	}
#endif

	if(gpio_get_value(pdata->irq) == 0) {
		dev_err(info->dev, "[NFC] Warning,irq-gpio state is low!\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&info->i2c_info.read_mutex);
	/* Skip interrupt during power switching
	 * It is released after first write */
	if (info->i2c_info.read_irq == SEC_NFC_SKIP) {
		dev_dbg(info->dev, "%s: Now power swiching. Skip this IRQ\n", __func__);
		mutex_unlock(&info->i2c_info.read_mutex);
		return IRQ_HANDLED;
	}

	info->i2c_info.read_irq += SEC_NFC_READ_TIMES;
	mutex_unlock(&info->i2c_info.read_mutex);

	wake_up_interruptible(&info->i2c_info.read_wait);
	wake_lock_timeout(&info->nfc_wake_lock, 2*HZ);

	return IRQ_HANDLED;
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

#ifdef FEATURE_SEC_NFC_TEST
	if (on_nfc_test)
		return 0;
#endif

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		dev_err(info->dev, "sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	mutex_lock(&info->i2c_info.read_mutex);
	if(count == 0)
	{
		if (info->i2c_info.read_irq >= SEC_NFC_INT)
			info->i2c_info.read_irq--;
		mutex_unlock(&info->i2c_info.read_mutex);
		goto out;
	}

	irq = info->i2c_info.read_irq;
	mutex_unlock(&info->i2c_info.read_mutex);
	if (irq == SEC_NFC_NONE) {
		if (file->f_flags & O_NONBLOCK) {
			dev_err(info->dev, "it is nonblock\n");
			ret = -EAGAIN;
			goto out;
		}
	}

	/* i2c recv */
	if (count > info->i2c_info.buflen)
		count = info->i2c_info.buflen;

	if (count > SEC_NFC_MSG_MAX_SIZE) {
		dev_err(info->dev, "user required wrong size :%d\n", (u32)count);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&info->i2c_info.read_mutex);
	memset(info->i2c_info.buf, 0, count);
	ret = i2c_master_recv(info->i2c_info.i2c_dev, info->i2c_info.buf, (u32)count);
	dev_dbg(info->dev, "recv size : %d\n", ret);

	if (ret == -EREMOTEIO) {
		ret = -ERESTART;
		goto read_error;
	} else if (ret != count) {
		dev_err(info->dev, "read failed: return: %d count: %d\n",
			ret, (u32)count);
		/*ret = -EREMOTEIO;*/
		goto read_error;
	}

	if (info->i2c_info.read_irq >= SEC_NFC_INT)
		info->i2c_info.read_irq--;

	if(info->i2c_info.read_irq == SEC_NFC_READ_TIMES)
		wake_up_interruptible(&info->i2c_info.read_wait);

	mutex_unlock(&info->i2c_info.read_mutex);

	if (copy_to_user(buf, info->i2c_info.buf, ret)) {
		dev_err(info->dev, "copy failed to user\n");
		ret = -EFAULT;
	}

	goto out;

read_error:
	info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->i2c_info.read_mutex);
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

	dev_dbg(info->dev, "%s: info: %p, count %d\n", __func__,
		info, (u32)count);

#ifdef FEATURE_SEC_NFC_TEST
	if (on_nfc_test)
		return 0;
#endif

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		dev_err(info->dev, "sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	if (count > info->i2c_info.buflen)
		count = info->i2c_info.buflen;

	if (count > SEC_NFC_MSG_MAX_SIZE) {
		dev_err(info->dev, "user required wrong size :%d\n", (u32)count);
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(info->i2c_info.buf, buf, count)) {
		dev_err(info->dev, "copy failed from user\n");
		ret = -EFAULT;
		goto out;
	}

	/* Skip interrupt during power switching
	 * It is released after first write */
	mutex_lock(&info->i2c_info.read_mutex);
	ret = i2c_master_send(info->i2c_info.i2c_dev, info->i2c_info.buf, count);
	if (info->i2c_info.read_irq == SEC_NFC_SKIP)
		info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->i2c_info.read_mutex);

	if (ret == -EREMOTEIO) {
		dev_err(info->dev, "send failed: return: %d count: %d\n",
		ret, (u32)count);
		ret = -ERESTART;
		goto out;
	}

	if (ret != count) {
		dev_err(info->dev, "send failed: return: %d count: %d\n",
		ret, (u32)count);
		ret = -EREMOTEIO;
	}

out:
	mutex_unlock(&info->mutex);

	return ret;
}

static unsigned int sec_nfc_poll(struct file *file, poll_table *wait)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	enum sec_nfc_irq irq;

	int ret = 0;

	dev_dbg(info->dev, "%s: info: %p\n", __func__, info);

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		dev_err(info->dev, "sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	poll_wait(file, &info->i2c_info.read_wait, wait);

	mutex_lock(&info->i2c_info.read_mutex);
	irq = info->i2c_info.read_irq;
	if (irq == SEC_NFC_READ_TIMES)
		ret = (POLLIN | POLLRDNORM);
	mutex_unlock(&info->i2c_info.read_mutex);

out:
	mutex_unlock(&info->mutex);

	return ret;
}

static int sec_nfc_regulator_onoff(struct sec_nfc_platform_data *data, int onoff)
{
	int rc = 0;
	struct regulator *regulator_nfc_pvdd;

	regulator_nfc_pvdd = regulator_get(NULL, data->nfc_pvdd);
	if (IS_ERR(regulator_nfc_pvdd) || regulator_nfc_pvdd == NULL) {
		pr_err("%s - nfc_pvdd regulator_get fail\n", __func__);
		return -ENODEV;
	}

	pr_info("%s - onoff = %d\n", __func__, onoff);

	if (onoff == NFC_I2C_LDO_ON) {
		rc = regulator_enable(regulator_nfc_pvdd);
		if (rc) {
			pr_err("%s - enable nfc_pvdd failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_nfc_pvdd);
		if (rc) {
			pr_err("%s - disable nfc_pvdd failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	}

	done:
	regulator_put(regulator_nfc_pvdd);

	return rc;
}

void sec_nfc_i2c_irq_clear(struct sec_nfc_info *info)
{
	/* clear interrupt. Interrupt will be occured at power off */
	mutex_lock(&info->i2c_info.read_mutex);
	info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->i2c_info.read_mutex);
}

int sec_nfc_i2c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sec_nfc_info *info = dev_get_drvdata(dev);
	struct sec_nfc_platform_data *pdata = info->pdata;
	int ret;

	dev_info(info->dev, "%s: start: %p\n", __func__, info);

	info->i2c_info.buflen = SEC_NFC_MAX_BUFFER_SIZE;
	info->i2c_info.buf = kzalloc(SEC_NFC_MAX_BUFFER_SIZE, GFP_KERNEL);
	if (!info->i2c_info.buf) {
		dev_err(dev,
			"failed to allocate memory for sec_nfc_info->buf\n");
		return -ENOMEM;
	}
	info->i2c_info.i2c_dev = client;
	info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_init(&info->i2c_info.read_mutex);
	init_waitqueue_head(&info->i2c_info.read_wait);
	i2c_set_clientdata(client, info);

	ret = gpio_request(pdata->irq, "nfc_int");
	if (ret) {
		dev_err(dev, "GPIO request is failed to register IRQ\n");
                goto err_irq_req;
	}
	gpio_direction_input(pdata->irq);

	ret = request_threaded_irq(client->irq, NULL, sec_nfc_irq_thread_fn,
			IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_ONESHOT,
			SEC_NFC_DRIVER_NAME, info);
	if (ret < 0) {
		dev_err(dev, "failed to register IRQ handler\n");
		kfree(info->i2c_info.buf);
		return ret;
	}

	if (of_get_property(dev->of_node, "sec-nfc,ldo_control", NULL)) {
		if (pdata->nfc_pvdd != NULL) {
			if(!lpcharge) {
				ret = sec_nfc_regulator_onoff(pdata, NFC_I2C_LDO_ON);
				if (ret < 0)
					pr_err("%s max86900_regulator_on fail err = %d\n",
							__func__, ret);
				usleep_range(1000, 1100);
			}
		}
	}

	dev_info(info->dev, "%s: success: %p\n", __func__, info);
	return 0;

err_irq_req:
        return ret;
}

static irqreturn_t sec_nfc_clk_irq_thread(int irq, void *dev_id)
{
	struct sec_nfc_info *info = dev_id;
	struct sec_nfc_platform_data *pdata = info->pdata;
	bool value;

	dev_dbg(info->dev, "[NFC]Clock Interrupt is occurred!\n");

	value = gpio_get_value(pdata->clk_req) > 0 ? true : false;

	if (value == info->clk_state)
		return IRQ_HANDLED;

	if (value)
		clk_prepare_enable(pdata->clk);
	else
		clk_disable_unprepare(pdata->clk);

	info->clk_state = value;

	return IRQ_HANDLED;
}

void sec_nfc_clk_ctl_enable(struct sec_nfc_info *info)
{
	struct sec_nfc_platform_data *pdata = info->pdata;

	if (info->clk_ctl)
		return;

	if (!pdata->clk)
		return;

	info->clk_state = false;
	enable_irq(pdata->clk_irq);
	info->clk_ctl = true;
}

void sec_nfc_clk_ctl_disable(struct sec_nfc_info *info)
{
	struct sec_nfc_platform_data *pdata = info->pdata;

	if (!info->clk_ctl)
		return;

	if (!pdata->clk)
		return;

	disable_irq(pdata->clk_irq);
	if (info->clk_state)
		clk_disable_unprepare(pdata->clk);

	info->clk_state = false;
	info->clk_ctl = false;
}

static void sec_nfc_set_mode(struct sec_nfc_info *info,
					enum sec_nfc_mode mode)
{
	struct sec_nfc_platform_data *pdata = info->pdata;

	/* intfo lock is aleady gotten before calling this function */
	if (info->mode == mode) {
		dev_dbg(info->dev, "Power mode is already %d", mode);
		return;
	}
	info->mode = mode;

	/* Skip interrupt during power switching
	 * It is released after first write */
	mutex_lock(&info->i2c_info.read_mutex);
	info->i2c_info.read_irq = SEC_NFC_SKIP;
	mutex_unlock(&info->i2c_info.read_mutex);

	gpio_set_value(pdata->ven, SEC_NFC_PW_OFF);
	if (pdata->firm) gpio_set_value(pdata->firm, SEC_NFC_FW_OFF);

	if (mode == SEC_NFC_MODE_BOOTLOADER)
		if (pdata->firm) gpio_set_value(pdata->firm, SEC_NFC_FW_ON);

	if (mode != SEC_NFC_MODE_OFF) 
	{
		msleep(SEC_NFC_VEN_WAIT_TIME);
		gpio_set_value(pdata->ven, SEC_NFC_PW_ON);
		sec_nfc_clk_ctl_enable(info);
		enable_irq_wake(info->i2c_info.i2c_dev->irq);
		msleep(SEC_NFC_VEN_WAIT_TIME/2);
	} else {
		sec_nfc_clk_ctl_disable(info);
		disable_irq_wake(info->i2c_info.i2c_dev->irq);
	}

	if(wake_lock_active(&info->nfc_wake_lock))
		wake_unlock(&info->nfc_wake_lock);

	pr_info("%s NFC mode is : %d\n",__func__ , mode);
}

static long sec_nfc_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	struct sec_nfc_platform_data *pdata = info->pdata;
	unsigned int new = (unsigned int)arg;
	int ret = 0;

	dev_dbg(info->dev, "%s: info: %p, cmd: 0x%x\n",
			__func__, info, cmd);

	mutex_lock(&info->mutex);

	switch (cmd) {
	case SEC_NFC_SET_MODE:
		if (info->mode == new)
			break;

		if (new >= SEC_NFC_MODE_COUNT) {
			dev_err(info->dev, "wrong mode (%d)\n", new);
			ret = -EFAULT;
			break;
		}
		sec_nfc_set_mode(info, new);

		break;

	case SEC_NFC_SLEEP:
		if (info->mode != SEC_NFC_MODE_BOOTLOADER) {
			if(wake_lock_active(&info->nfc_wake_lock))
				wake_unlock(&info->nfc_wake_lock);
			gpio_set_value(pdata->wake, SEC_NFC_WAKE_SLEEP);
		}
		break;

	case SEC_NFC_WAKEUP:
		if (info->mode != SEC_NFC_MODE_BOOTLOADER) {
			gpio_set_value(pdata->wake, SEC_NFC_WAKE_UP);
			if(!wake_lock_active(&info->nfc_wake_lock))
				wake_lock(&info->nfc_wake_lock);
		}
		break;

// [START] NPT
	case SEC_NFC_SET_NPT_MODE:
		pr_info("%s: VEN Pin state = %d\n", __func__, gpio_get_value(pdata->ven));
		pr_info("%s: FIRM Pin state = %d\n", __func__, gpio_get_value(pdata->firm));

		if(SEC_NFC_NPT_CMD_ON == new) {
			pr_info("%s: NFC OFF mode NPT - Turn on VEN.\n", __func__);
			info->mode = SEC_NFC_MODE_FIRMWARE;
			mutex_lock(&info->i2c_info.read_mutex);
			info->i2c_info.read_irq = SEC_NFC_SKIP;
			mutex_unlock(&info->i2c_info.read_mutex);
			gpio_set_value(pdata->ven, SEC_NFC_PW_ON);
			sec_nfc_clk_ctl_enable(info);
			msleep(20);
			gpio_set_value(pdata->firm, SEC_NFC_FW_ON);
			enable_irq_wake(info->i2c_info.i2c_dev->irq);
		} else if(SEC_NFC_NPT_CMD_OFF == new) {
			pr_info("%s: NFC OFF mode NPT - Turn off VEN.\n", __func__);
			info->mode = SEC_NFC_MODE_OFF;
			gpio_set_value(pdata->firm, SEC_NFC_FW_OFF);
			gpio_set_value(pdata->ven, SEC_NFC_PW_OFF);
			sec_nfc_clk_ctl_disable(info);
			disable_irq_wake(info->i2c_info.i2c_dev->irq);
		}
		break;
// [END] NPT

	default:
		dev_err(info->dev, "Unknow ioctl 0x%x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&info->mutex);

	return ret;
}

static int nfc_state_print(struct sec_nfc_info *info)
{
	struct sec_nfc_platform_data *pdata = info->pdata;
	struct regulator *regulator_nfc_pvdd;

	int en = gpio_get_value(info->pdata->ven);
	int firm =	gpio_get_value(info->pdata->firm);
	int pvdd = 0;

	regulator_nfc_pvdd = regulator_get(NULL, pdata->nfc_pvdd);
	if (IS_ERR(regulator_nfc_pvdd) || regulator_nfc_pvdd == NULL) {
		pr_err("%s - nfc_pvdd regulator_get fail\n", __func__);
		return -ENODEV;
	}
	pvdd = regulator_is_enabled(regulator_nfc_pvdd);

	pr_err("NFC: %s en: %d, firm: %d power: %d\n",__func__, en, firm, pvdd);
	pr_err("NFC: %s mode %d, clk_state: %d\n",__func__ , info->mode, info->clk_state);

	regulator_put(regulator_nfc_pvdd);

	return 0;
}

static int sec_nfc_open(struct inode *inode, struct file *file)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	int ret = 0;

	dev_info(info->dev, "%s\n", __func__);

	mutex_lock(&info->mutex);
	if (info->mode != SEC_NFC_MODE_OFF) {
		dev_err(info->dev, "sec_nfc is busy\n");
		ret = -EBUSY;
		goto out;
	}

	sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);

out:
	mutex_unlock(&info->mutex);
	return ret;
}

static int sec_nfc_close(struct inode *inode, struct file *file)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);

	nfc_state_print(info);
	
	dev_info(info->dev, "%s\n", __func__);

	mutex_lock(&info->mutex);
	sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);
	mutex_unlock(&info->mutex);

	return 0;
}

static const struct file_operations sec_nfc_fops = {
	.owner		= THIS_MODULE,
	.read		= sec_nfc_read,
	.write		= sec_nfc_write,
	.poll		= sec_nfc_poll,
	.open		= sec_nfc_open,
	.release	= sec_nfc_close,
	.unlocked_ioctl	= sec_nfc_ioctl,
};

#ifdef CONFIG_NFC_CLKREQ_SUSPEND
static int sec_nfc_suspend(struct device *dev)
{
	struct sec_nfc_info *info = SEC_NFC_GET_INFO(dev);
	int req, ret = 0;

	if (info->pdata->clk_req > 0) { /* if clk_req exist! */
		req = gpio_get_value(info->pdata->clk_req);
		pr_info("%s: clk_req:%d wake_lock[%d] read_irq[%d]\n", __func__, req,
				wake_lock_active(&info->nfc_wake_lock), info->i2c_info.read_irq);
		/* If CLK REQ HIGH, AP not allowed to go suspend for HAECHI model */
		if (req) {
			pr_info("%s: kick it out of sleep\n", __func__);
			ret = -EPERM;
		}
	}
	return ret;
}

static int sec_nfc_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(sec_nfc_pm_ops, sec_nfc_suspend, sec_nfc_resume);
#endif

/*device tree parsing*/
static int sec_nfc_parse_dt(struct device *dev,
	struct sec_nfc_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->ven = of_get_named_gpio(np, "sec-nfc,ven-gpio", 0);
	pdata->firm = of_get_named_gpio(np, "sec-nfc,firm-gpio", 0);
	pdata->wake = pdata->firm;
	pdata->irq = of_get_named_gpio(np, "sec-nfc,irq-gpio", 0);

	if (of_get_property(dev->of_node, "sec-nfc,nfc_ap_clk", NULL)) {
		pdata->clk_req = of_get_named_gpio(np, "sec-nfc,clk_req-gpio", 0);
		pr_info("%s: enable nfc_ap_clk : %d\n", __func__, pdata->clk_req);
	}

	if (of_get_property(dev->of_node, "sec-nfc,ldo_control", NULL)) {
		if (of_property_read_string(np, "sec-nfc,nfc_pvdd",
					&pdata->nfc_pvdd) < 0) {
			pr_err("%s - get nfc_pvdd error\n", __func__);
			pdata->nfc_pvdd = NULL;
		}
	}

	pr_info("%s: irq : %d, ven : %d, firm : %d\n",
			__func__, pdata->irq, pdata->ven, pdata->firm);
	return 0;
}

#ifdef FEATURE_SEC_NFC_TEST
static int sec_nfc_i2c_read(char *buf, int count)
{
	struct sec_nfc_info *info = g_nfc_info;
	int ret = 0;

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		pr_err("NFC_TEST: sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	/* i2c recv */
	if (count > info->i2c_info.buflen)
		count = info->i2c_info.buflen;

	if (count > SEC_NFC_MSG_MAX_SIZE) {
		pr_err("NFC_TEST: user required wrong size :%d\n", (u32)count);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&info->i2c_info.read_mutex);
	memset(buf, 0, count);
	ret = i2c_master_recv(info->i2c_info.i2c_dev, buf, (u32)count);
	pr_info("NFC_TEST: recv size : %d\n", ret);

	if (ret == -EREMOTEIO) {
		ret = -ERESTART;
		goto read_error;
	} else if (ret != count) {
		pr_err("NFC_TEST: read failed: return: %d count: %d\n",
			ret, (u32)count);
		goto read_error;
	}

	mutex_unlock(&info->i2c_info.read_mutex);

	goto out;

read_error:
	info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->i2c_info.read_mutex);
out:
	mutex_unlock(&info->mutex);

	return ret;
}

static int sec_nfc_i2c_write(char *buf,	int count)
{
	struct sec_nfc_info *info = g_nfc_info;
	int ret = 0;

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		pr_err("NFC_TEST: sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	if (count > info->i2c_info.buflen)
		count = info->i2c_info.buflen;

	if (count > SEC_NFC_MSG_MAX_SIZE) {
		pr_err("NFC_TEST: user required wrong size :%d\n", (u32)count);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&info->i2c_info.read_mutex);
	ret = i2c_master_send(info->i2c_info.i2c_dev, buf, count);
	mutex_unlock(&info->i2c_info.read_mutex);

	if (ret == -EREMOTEIO) {
		pr_err("NFC_TEST: send failed: return: %d count: %d\n",
		ret, (u32)count);
		ret = -ERESTART;
		goto out;
	}

	if (ret != count) {
		pr_err("NFC_TEST: send failed: return: %d count: %d\n",
		ret, (u32)count);
		ret = -EREMOTEIO;
	}

out:
	mutex_unlock(&info->mutex);

	return ret;
}

static ssize_t sec_nfc_test_show(struct class *class,
					struct class_attribute *attr,
					char *buf)
{
	char cmd[8] = {0x0, 0x1, 0x0, 0x0,}; /*bootloader fw check*/
	enum sec_nfc_mode old_mode = g_nfc_info->mode;
	int size;
	int ret = 0;
	int timeout = 1;

	on_nfc_test = true;
	nfc_int_wait = false;
	pr_info("NFC_TEST: mode = %d\n", old_mode);

	sec_nfc_set_mode(g_nfc_info, SEC_NFC_MODE_BOOTLOADER);
	ret = sec_nfc_i2c_write(cmd, 4);
	if (ret < 0) {
		pr_info("NFC_TEST: i2c write error %d\n", ret);
		size = sprintf(buf, "NFC_TEST: i2c write error %d\n", ret);
		goto exit;
	}

	timeout = wait_event_interruptible_timeout(g_nfc_info->i2c_info.read_wait, nfc_int_wait, 100);
	ret = sec_nfc_i2c_read(buf, 16);
	if (ret < 0) {
		pr_info("NFC_TEST: i2c read error %d\n", ret);
		size = sprintf(buf, "NFC_TEST: i2c read error %d\n", ret);
		goto exit;
	}

	pr_info("NFC_TEST: BL ver: %02X %02X %02X %02X, INT: %s\n", buf[0],
					buf[1],	buf[2], buf[3], timeout ? "OK":"NOK");
	size = sprintf(buf, "BL ver: %02X.%02X.%02X.%02X, INT: %s\n", buf[0],
					buf[1], buf[2],	buf[3], timeout ? "OK":"NOK");

exit:
	sec_nfc_set_mode(g_nfc_info, old_mode);
	on_nfc_test = false;

	return size;
}
static ssize_t sec_nfc_test_store(struct class *dev,
					struct class_attribute *attr,
					const char *buf, size_t size)
{
	return size;
}

static CLASS_ATTR(test, 0664, sec_nfc_test_show, sec_nfc_test_store);
#endif

static int __devinit __sec_nfc_probe(struct device *dev)
{
	struct sec_nfc_info *info;
	struct sec_nfc_platform_data *pdata = NULL;
	int ret = 0;

#ifdef FEATURE_SEC_NFC_TEST
	struct class *nfc_class;
#endif
	dev_info(dev, "[NFC]sec-nfc probe start \n");
	if (dev->of_node) {
		pdata = devm_kzalloc(dev,
			sizeof(struct sec_nfc_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = sec_nfc_parse_dt(dev, pdata);
		if (ret)
			return ret;
	} else {
		pdata = dev->platform_data;
	}

	if (!pdata) {
		dev_err(dev, "No platform data\n");
		ret = -ENOMEM;
		goto err_pdata;
	}

	info = kzalloc(sizeof(struct sec_nfc_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "failed to allocate memory for sec_nfc_info\n");
		ret = -ENOMEM;
		goto err_info_alloc;
	}
	info->dev = dev;
	info->pdata = pdata;
	info->mode = SEC_NFC_MODE_OFF;

	mutex_init(&info->mutex);
	dev_set_drvdata(dev, info);

	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	info->miscdev.name = SEC_NFC_DRIVER_NAME;
	info->miscdev.fops = &sec_nfc_fops;
	info->miscdev.parent = dev;
	ret = misc_register(&info->miscdev);
	if (ret < 0) {
		dev_err(dev, "failed to register Device\n");
		goto err_dev_reg;
	}

	if (of_get_property(dev->of_node, "sec-nfc,nfc_ap_clk", NULL)) {
		pdata->clk = clk_get(dev, "oscclk_nfc");
		if(IS_ERR(pdata->clk)) {
			pr_err("%s: clk not found\n",__func__);
			goto err_gpio_clk_parse;
		}
		pdata->clk_irq = gpio_to_irq(pdata->clk_req);

		ret = request_threaded_irq(pdata->clk_irq, NULL, sec_nfc_clk_irq_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"nfc_clk_req", info);
		if (ret < 0) {
			dev_err(info->dev, "failed to register CLK REQ IRQ handler\n");
		}
		disable_irq(pdata->clk_irq);
	}

	ret = gpio_request(pdata->ven, "nfc_ven");
	if (ret) {
		dev_err(dev, "failed to get gpio ven\n");
		goto err_gpio_ven;
	}
	gpio_direction_output(pdata->ven, SEC_NFC_PW_OFF);

	if (pdata->firm)
	{
		ret = gpio_request(pdata->firm, "nfc_firm");
		if (ret) {
			dev_err(dev, "failed to get gpio firm\n");
			goto err_gpio_firm;
		}
		gpio_direction_output(pdata->firm, SEC_NFC_FW_OFF);
	}

	wake_lock_init(&info->nfc_wake_lock, WAKE_LOCK_SUSPEND, "nfc_wake_lock");

#ifdef FEATURE_SEC_NFC_TEST
	g_nfc_info = info;
	nfc_class = class_create(THIS_MODULE, "nfc_test");
	if (IS_ERR(&nfc_class))
		pr_err("NFC: failed to create nfc class\n");
	else
	{
		ret = class_create_file(nfc_class, &class_attr_test);
		if (ret)
			pr_err("NFC: failed to create attr_test\n");
	}
#endif
	dev_info(dev, "%s: success info: %p, pdata %p\n", __func__, info, pdata);

	return 0;

err_gpio_firm:
	gpio_free(pdata->ven);
err_gpio_ven:
	free_irq(pdata->clk_irq, info);
err_gpio_clk_parse:
	misc_deregister(&info->miscdev);
err_dev_reg:
	mutex_destroy(&info->mutex);
	kfree(info);
err_info_alloc:
	devm_kfree(dev, pdata);
err_pdata:
	return ret;
}

static int __devexit __sec_nfc_remove(struct device *dev)
{
	struct sec_nfc_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->i2c_info.i2c_dev;
	struct sec_nfc_platform_data *pdata = info->pdata;

	dev_dbg(info->dev, "%s\n", __func__);

	misc_deregister(&info->miscdev);
	sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);
	free_irq(client->irq, info);
	free_irq(pdata->clk_irq, info);
	gpio_free(pdata->irq);
	gpio_set_value(pdata->firm, 0);
	gpio_free(pdata->ven);
	if (pdata->firm) gpio_free(pdata->firm);
	wake_lock_destroy(&info->nfc_wake_lock);

	kfree(info);

	return 0;
}

MODULE_DEVICE_TABLE(i2c, sec_nfc_id_table);
typedef struct i2c_driver sec_nfc_driver_type;
#define SEC_NFC_INIT(driver)	i2c_add_driver(driver);
#define SEC_NFC_EXIT(driver)	i2c_del_driver(driver);

static int __devinit sec_nfc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;

	ret = __sec_nfc_probe(&client->dev);
	if (ret)
		return ret;

	if (sec_nfc_i2c_probe(client))
		__sec_nfc_remove(&client->dev);

	return ret;
}

static int __devexit sec_nfc_remove(struct i2c_client *client)
{
	return __sec_nfc_remove(&client->dev);
}

static struct i2c_device_id sec_nfc_id_table[] = {
	{ SEC_NFC_DRIVER_NAME, 0 },
	{ }
};

static struct of_device_id nfc_match_table[] = {
	{ .compatible = SEC_NFC_DRIVER_NAME,},
	{},
};

static sec_nfc_driver_type sec_nfc_driver = {
	.probe = sec_nfc_probe,
	.id_table = sec_nfc_id_table,
	.remove = sec_nfc_remove,
	.driver = {
		.name = SEC_NFC_DRIVER_NAME,
#ifdef CONFIG_NFC_CLKREQ_SUSPEND
		.pm = &sec_nfc_pm_ops,
#endif
		.of_match_table = nfc_match_table,
	},
};

static int __init sec_nfc_init(void)
{
	return SEC_NFC_INIT(&sec_nfc_driver);
}

static void __exit sec_nfc_exit(void)
{
	SEC_NFC_EXIT(&sec_nfc_driver);
}

module_init(sec_nfc_init);
module_exit(sec_nfc_exit);

MODULE_DESCRIPTION("Samsung sec_nfc driver");
MODULE_LICENSE("GPL");
