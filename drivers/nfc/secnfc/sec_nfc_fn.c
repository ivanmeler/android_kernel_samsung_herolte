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
#include <linux/wait.h>
#include <linux/delay.h>

#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>

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

#include <linux/wakelock.h>

enum push_state {
	PUSH_NONE,
	PUSH_ON,
};

struct sec_nfc_fn_info {
	struct miscdevice miscdev;
	struct device *dev;
	struct sec_nfc_fn_platform_data *pdata;

	struct mutex push_mutex;
	enum push_state push_irq;
	wait_queue_head_t push_wait;

	struct mutex confirm_mutex;
	enum readable_state readable;
	struct wake_lock wake_lock;
};

static irqreturn_t sec_nfc_fn_push_thread_fn(int irq, void *dev_id)
{
	struct sec_nfc_fn_info *info = dev_id;

	dev_dbg(info->dev, "PUSH\n");

	mutex_lock(&info->push_mutex);
	info->push_irq = PUSH_ON;
	mutex_unlock(&info->push_mutex);

	wake_up_interruptible(&info->push_wait);

	if (!wake_lock_active(&info->wake_lock)) {
		pr_err("\n %s: Set wake_lock_timeout for 2 sec. !!!\n", __func__);
		wake_lock_timeout(&info->wake_lock, 2 * HZ);
	}

	return IRQ_HANDLED;
}

static unsigned int sec_nfc_fn_poll(struct file *file, poll_table *wait)
{
	struct sec_nfc_fn_info *info = container_of(file->private_data,
						struct sec_nfc_fn_info, miscdev);
	enum push_state push;

	int ret = 0;

	dev_dbg(info->dev, "%s: info: %p\n", __func__, info);

	poll_wait(file, &info->push_wait, wait);

	mutex_lock(&info->push_mutex);

	push = info->push_irq;
	if (push == PUSH_ON)
		ret = (POLLIN | POLLRDNORM);

	mutex_unlock(&info->push_mutex);

	return ret;
}

static long sec_nfc_fn_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct sec_nfc_fn_info *info = container_of(file->private_data,
						struct sec_nfc_fn_info, miscdev);

	void __user *argp = (void __user *)arg;
	int ret = 0;
	int state = 0;

	dev_dbg(info->dev, "%s: info: %p, cmd: 0x%x\n",
			__func__, info, cmd);

	switch (cmd) {
	case SEC_NFC_GET_PUSH:
		mutex_lock(&info->push_mutex);
		state = gpio_get_value(of_get_named_gpio(info->dev->of_node,
									"sec-nfc-fn,int-gpio", 0));

		pr_info("%s: copy push pin value-state :%d\n", __func__, state);
		if (copy_to_user(argp, &state,
			sizeof(state)) != 0) {
			dev_err(info->dev, "copy failed to user\n");
		}
		info->push_irq = PUSH_NONE;
		mutex_unlock(&info->push_mutex);

		break;
	default:
		dev_err(info->dev, "Unknow ioctl 0x%x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static int sec_nfc_fn_parse_dt(struct device *dev,
	struct sec_nfc_fn_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->push = of_get_named_gpio_flags(np, "sec-nfc-fn,int-gpio",
		0, &pdata->int_gpio_flags);

	return 0;
}


static int sec_nfc_fn_open(struct inode *inode, struct file *file)
{
	struct sec_nfc_fn_info *info = container_of(file->private_data,
						struct sec_nfc_fn_info, miscdev);
	int ret = 0;
	uid_t uid;

	dev_dbg(info->dev, "%s: info : %p" , __func__, info);

	uid = __task_cred(current)->uid;
	if (g_secnfc_uid != uid) {
		dev_err(info->dev, "%s:Un-authorized process.Access denied\n",
				__func__);
		return -EPERM;
	}

	mutex_lock(&info->push_mutex);
	info->push_irq = PUSH_NONE;
	mutex_unlock(&info->push_mutex);

	mutex_lock(&info->confirm_mutex);
	info->readable = RDABLE_YES;
	mutex_unlock(&info->confirm_mutex);

	return ret;
}

static int sec_nfc_fn_close(struct inode *inode, struct file *file)
{
	struct sec_nfc_fn_info *info = container_of(file->private_data,
						struct sec_nfc_fn_info, miscdev);

	dev_dbg(info->dev, "%s: info : %p" , __func__, info);

	mutex_lock(&info->confirm_mutex);
	info->readable = RDABLE_NO;
	mutex_unlock(&info->confirm_mutex);

	return 0;
}

static const struct file_operations sec_nfc_fn_fops = {
	.owner		= THIS_MODULE,
	.poll		= sec_nfc_fn_poll,
	.open		= sec_nfc_fn_open,
	.release	= sec_nfc_fn_close,
	.unlocked_ioctl	= sec_nfc_fn_ioctl,
#ifdef CONFIG_COMPAT
        .compat_ioctl = sec_nfc_fn_ioctl,
#endif
};

#ifdef CONFIG_PM
static int sec_nfc_fn_suspend(struct device *dev)
{
/*	struct sec_nfc_fn_platform_data *pdata = dev->platform_data;	*/
	struct sec_nfc_fn_info *info = dev_get_drvdata(dev);

/*	pdata->cfg_gpio();	*/

	mutex_lock(&info->confirm_mutex);
	info->readable = RDABLE_NO;
	mutex_unlock(&info->confirm_mutex);

	return 0;
}

static int sec_nfc_fn_resume(struct device *dev)
{
/*	struct sec_nfc_fn_platform_data *pdata = dev->platform_data;	*/
/*	pdata->cfg_gpio();	*/
	return 0;
}

static SIMPLE_DEV_PM_OPS(sec_nfc_fn_pm_ops, sec_nfc_fn_suspend,
							sec_nfc_fn_resume);
#endif

static int sec_nfc_fn_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sec_nfc_fn_info *info;
/*	struct sec_nfc_fn_platform_data *pdata = dev->platform_data;	*/
	struct sec_nfc_fn_platform_data *pdata;
	int ret = 0;
	int err;

	pr_info("sec-nfc-fn probe start\n");

	/*	Check tamper	*/
	if (check_custom_kernel() == 1)
		pr_info("%s: The kernel is tampered. Couldn't initialize NFC.\n",
			__func__);

	if (dev) {
		pr_info("%s: alloc for fn platform data\n", __func__);
		pdata = kzalloc(sizeof(struct sec_nfc_fn_platform_data), GFP_KERNEL);

		if (!pdata) {
			dev_err(dev, "No platform data\n");
			ret = -ENOMEM;
			goto err_pdata;
		}
	} else {
		pr_info("%s: failed alloc platform data", __func__);
	}

	err = sec_nfc_fn_parse_dt(dev, pdata);

	info = kzalloc(sizeof(struct sec_nfc_fn_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "failed to allocate memory for sec_nfc_fn_info\n");
		ret = -ENOMEM;
		kfree(pdata);
		goto err_info_alloc;
	}

	info->dev = dev;
	info->pdata = pdata;

	dev_set_drvdata(dev, info);

	/*	pdata->cfg_gpio();	*/

	wake_lock_init(&info->wake_lock, WAKE_LOCK_SUSPEND, "NFCWAKE_FN");

	mutex_init(&info->push_mutex);
	mutex_init(&info->confirm_mutex);
	init_waitqueue_head(&info->push_wait);
	info->push_irq = PUSH_NONE;

	ret = request_threaded_irq(gpio_to_irq(pdata->push), NULL,
			sec_nfc_fn_push_thread_fn,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, SEC_NFC_FN_DRIVER_NAME,
			info);
	if (ret < 0) {
		dev_err(dev, "failed to register PUSH handler\n");
		goto err_push_req;
	}

	ret = enable_irq_wake(gpio_to_irq(pdata->push));
	if (ret) {
		dev_err(dev, "failed to register wakeup irq (0x%02x), ret: %d\n",
			pdata->push, ret);
		goto err_push_wake;
	}

	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	info->miscdev.name = SEC_NFC_FN_DRIVER_NAME;
	info->miscdev.fops = &sec_nfc_fn_fops;
	info->miscdev.parent = dev;
	ret = misc_register(&info->miscdev);
	if (ret < 0) {
		dev_err(dev, "failed to register Device\n");
		goto err_dev_reg;
	}

	pr_info("sec-nfc-fn probe finish\n");

	return 0;

err_dev_reg:
err_push_wake:
err_push_req:
err_info_alloc:
	wake_lock_destroy(&info->wake_lock);
	kfree(info);
err_pdata:

	return ret;
}

static int sec_nfc_fn_remove(struct platform_device *pdev)
{
	struct sec_nfc_fn_info *info = dev_get_drvdata(&pdev->dev);
	struct sec_nfc_fn_platform_data *pdata = pdev->dev.platform_data;

	dev_dbg(info->dev, "%s\n", __func__);

	misc_deregister(&info->miscdev);
	free_irq(pdata->push, info);
	wake_lock_destroy(&info->wake_lock);
	kfree(info->pdata);
	kfree(info);

	return 0;
}

static struct platform_device_id sec_nfc_fn_id_table[] = {
	{ SEC_NFC_FN_DRIVER_NAME, 0 },
	{ }
};

static struct of_device_id nfc_match_table[] = {
	{ .compatible = SEC_NFC_FN_DRIVER_NAME,},
	{},
};

MODULE_DEVICE_TABLE(platform, sec_nfc_fn_id_table);
static struct platform_driver sec_nfc_fn_driver = {
	.probe = sec_nfc_fn_probe,
	.id_table = sec_nfc_fn_id_table,
	.remove = sec_nfc_fn_remove,
	.driver = {
		.name = SEC_NFC_FN_DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &sec_nfc_fn_pm_ops,
#endif
	.of_match_table = nfc_match_table,
	},
};

module_platform_driver(sec_nfc_fn_driver);


MODULE_DESCRIPTION("Samsung sec_nfc_fn driver");
MODULE_LICENSE("GPL");
