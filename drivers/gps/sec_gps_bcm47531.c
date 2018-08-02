#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/bug.h>
#include <linux/sec_sysfs.h>

#ifdef CONFIG_SENSORS_SSP_BBD
#include <linux/suspend.h>
#include <linux/notifier.h>
#endif

static struct device *gps_dev;
//static wait_queue_head_t *p_geofence_wait;
//static unsigned int gps_host_wake_up = 0;
static unsigned int gps_pwr_on = 0;

#if 0 //def CONFIG_SENSORS_SSP_BBD
static struct pinctrl *host_wake_pinctrl;
static struct pinctrl_state *host_wake_input;
static struct pinctrl_state *host_wake_irq;
#endif

/* BRCM
 * GPS geofence wakeup device implementation
 * to support waking up gpsd upon arrival of the interrupt
 * gpsd will select on this device to be notified of fence crossing event
 */
#if 0
struct gps_geofence_wake {
	wait_queue_head_t wait;
	int irq;
	int host_req_pin;
	struct miscdevice misc;
};

static int gps_geofence_wake_open(struct inode *inode, struct file *filp)
{
	struct gps_geofence_wake *ac_data = container_of(filp->private_data,
			struct gps_geofence_wake,
			misc);

	filp->private_data = ac_data;
	return 0;
}

static int gps_geofence_wake_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static unsigned int gps_geofence_wake_poll(struct file *file, poll_table *wait)
{
	int gpio_value;
	struct gps_geofence_wake *ac_data = file->private_data;

	BUG_ON(!ac_data);

	poll_wait(file, &ac_data->wait, wait);

	gpio_value = gpio_get_value(ac_data->host_req_pin);

	/*printk(KERN_INFO "gpio geofence wake host_req=%d\n",gpio_value);*/

	if (gpio_value)
		return POLLIN | POLLRDNORM;

	return 0;
}


static long gps_geofence_wake_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	/* TODO
	 * Fill in useful commands
	 * (like test helper)
	 */
	/*
	switch(cmd) {
		default:
	}
	*/
	return 0;
}


static const struct file_operations gps_geofence_wake_fops = {
	.owner = THIS_MODULE,
	.open = gps_geofence_wake_open,
	.release = gps_geofence_wake_release,
	.poll = gps_geofence_wake_poll,
	/*.read = gps_geofence_wake_read,
	.write = gps_geofence_wake_write,*/
	.unlocked_ioctl = gps_geofence_wake_ioctl
};

static struct gps_geofence_wake geofence_wake;

static int gps_geofence_wake_init(int irq, int host_req_pin)
{
	int ret;

	struct gps_geofence_wake *ac_data = &geofence_wake;
	memset(ac_data, 0, sizeof(struct gps_geofence_wake));

	/* misc device setup */
	/* gps daemon will access "/dev/gps_geofence_wake" */
	ac_data->misc.minor = MISC_DYNAMIC_MINOR;
	ac_data->misc.name = "gps_geofence_wake";
	ac_data->misc.fops = &gps_geofence_wake_fops;

	/* information that be used later */
	ac_data->irq = irq;
	ac_data->host_req_pin = host_req_pin;

	/* initialize wait queue : */
	/* which will be used by poll or select system call */
	init_waitqueue_head(&ac_data->wait);

	printk(KERN_INFO "%s misc register, name %s, irq %d, host req pin num %d\n",
		__func__, ac_data->misc.name, irq, host_req_pin);

	ret = misc_register(&ac_data->misc);
	if (ret != 0) {
		printk(KERN_ERR "cannot register gps geofence wake miscdev on minor=%d (%d)\n",
			MISC_DYNAMIC_MINOR, ret);
		return -ENODEV;
	}

	p_geofence_wait = &ac_data->wait;

	return 0;
}

/* --------------- */


/*EXPORT_SYMBOL(p_geofence_wait);*/ /*- BRCM -*/

static irqreturn_t gps_host_wake_isr(int irq, void *dev)
{

	int gps_host_wake;

	gps_host_wake = gpio_get_value(gps_host_wake_up);
	irq_set_irq_type(irq,
		gps_host_wake ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);

	// printk(KERN_ERR "%s GPS pin level[%s]\n", __func__, gps_host_wake ? "High" : "Low");

	if (p_geofence_wait && gps_host_wake) {
		printk(KERN_ERR "%s Wake-up GPS daemon[TRUE]\n", __func__);
		wake_up_interruptible(p_geofence_wait);
	}

	return IRQ_HANDLED;
}
#endif
int check_gps_op(void)
{
	/* This pin is high when gps is working */
	int gps_is_running = gpio_get_value(gps_pwr_on);

	pr_debug("LPA : check_gps_op(%d)\n", gps_is_running);

	return gps_is_running;
}
EXPORT_SYMBOL(check_gps_op);

#if 0 //def CONFIG_SENSORS_SSP_BBD
static int bcm477x_notifier(struct notifier_block *nb, unsigned long event, void * data)
{
	int ret = 0;

	// printk("--> %s(%lu)\n", __func__, event);

	switch (event) {
		case PM_SUSPEND_PREPARE:
			 printk("%s going to sleep.\n", __func__);
			// set pulldown and config interrupt
			ret = pinctrl_select_state(host_wake_pinctrl, host_wake_irq);
			if (ret) {
				dev_err(gps_dev, "Failed to get host_wake_irq pinctrl.\n");
				return ret;
			}
			break;

		case PM_POST_SUSPEND:
			 printk("%s waking up.\n", __func__);
			// set pullup and config input
			ret = pinctrl_select_state(host_wake_pinctrl, host_wake_input);
			if (ret) {
				dev_err(gps_dev, "Failed to get host_wake pinctrl.\n");
				return ret;
			}
			break;
        }
        return NOTIFY_OK;
}

static struct notifier_block bcm477x_notifier_block = {
        .notifier_call = bcm477x_notifier,
};
#endif

static int __init gps_bcm47531_init(void)
{
	//int irq = 0, 
	int ret = 0;
	const char *gps_node = "samsung,exynos54xx-bcm4753";

	struct device_node *root_node = NULL;

	gps_dev = sec_device_create(NULL, "gps");
	BUG_ON(!gps_dev);

	root_node = of_find_compatible_node(NULL, NULL, gps_node);
	if (!root_node) {
		WARN(1, "failed to get device node of bcm4753\n");
		ret = -ENODEV;
		goto err_sec_device_create;
	}

	//========== GPS_PWR_EN ============//
	gps_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(gps_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", gps_pwr_on);
		ret = -ENODEV;
		goto err_sec_device_create;
	}

	if (gpio_request(gps_pwr_on, "GPS_PWR_EN")) {
		WARN(1, "fail to request gpio(GPS_PWR_EN)\n");
		ret = -ENODEV;
		goto err_sec_device_create;
	}
	gpio_direction_output(gps_pwr_on, 0);
	gpio_export(gps_pwr_on, 1);
	gpio_export_link(gps_dev, "GPS_PWR_EN", gps_pwr_on);
#if 0
	//========== GPS_HOST_WAKE ============//
	gps_host_wake_up = of_get_gpio(root_node, 1);
	if (!gpio_is_valid(gps_host_wake_up)) {
		WARN(1, "Invalied gpio pin : %d\n", gps_host_wake_up);
		ret = -ENODEV;
		goto err_sec_device_create;
	}

	if (gpio_request(gps_host_wake_up, "GPS_HOST_WAKE")) {
		WARN(1, "fail to request gpio(GPS_HOST_WAKE)\n");
		ret = -ENODEV;
		goto err_sec_device_create;
	}
	gpio_direction_output(gps_host_wake_up, 0);
	gpio_export(gps_host_wake_up, 1);
	gpio_export_link(gps_dev, "GPS_HOST_WAKE", gps_host_wake_up);
#endif
#if 0 // def CONFIG_SENSORS_SSP_BBD
	gps_dev->of_node = root_node;
	host_wake_pinctrl = devm_pinctrl_get(gps_dev);

	host_wake_input = pinctrl_lookup_state(host_wake_pinctrl, "host_wake");
	if (!host_wake_input)
		pr_err("%s: failed to get host_wake_input pinctrl\n", __func__);

	host_wake_irq = pinctrl_lookup_state(host_wake_pinctrl, "host_wake_irq");
	if (!host_wake_irq)
		pr_err("%s: failed to get host_wake_input pinctrl\n", __func__);

	printk("%s waking up", __func__);
	ret = pinctrl_select_state(host_wake_pinctrl, host_wake_input);
	if (ret) {
		dev_err(gps_dev, "Failed to get host_wake pinctrl.\n");
		ret = -ENODEV;
		goto err_sec_device_create;
	}
#endif

#if 0
	irq = gpio_to_irq(gps_host_wake_up);
	ret = gps_geofence_wake_init(irq, gps_host_wake_up);
	if (ret) {
		pr_err("[GPS] gps_geofence_wake_init failed.\n");
		ret = -ENODEV;
		goto err_sec_device_create;
	}

	ret = request_irq(irq, gps_host_wake_isr,
		IRQF_TRIGGER_RISING, "gps_host_wake", NULL);
	if (ret) {
		pr_err("[GPS] Request_host wake irq failed.\n");
		ret = -ENODEV;
		goto err_free_irq;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		pr_err("[GPS] Set_irq_wake failed.\n");
		ret = -ENODEV;
		goto err_free_irq;
	}
#endif

#if 0 // def CONFIG_SENSORS_SSP_BBD
    ret = register_pm_notifier(&bcm477x_notifier_block);
    if (ret) {
        pr_err("[GPS] register_pm_notifier failed.\n");
        ret = -ENODEV;
        goto err_free_irq;
    }
#endif

	return 0;

//err_free_irq:
//	free_irq(irq, NULL);
err_sec_device_create:
	sec_device_destroy(gps_dev->devt);
	return ret;
}

device_initcall(gps_bcm47531_init);
