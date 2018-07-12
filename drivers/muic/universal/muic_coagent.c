#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/muic/muic.h>

#include "muic-internal.h"
#include "muic_apis.h"
#include "muic_coagent.h"

struct kfifo fifo;

struct coagent {
	struct mutex		co_mutex;
	struct task_struct	*co_thread;
	int co_number;
	muic_data_t *pmuic; /* context for MUIC */

	struct kfifo fifo;
	struct semaphore read_sem;
	bool is_active;
};

static struct coagent base_coagent;

extern bool muic_is_online(void);

void coagent_update_ctx(muic_data_t *pmuic)
{
	struct coagent *pco = &base_coagent;

	pr_info("%s\n", __func__);

	pco->pmuic = pmuic;
}

static int coagent_cmd_gamepad(struct coagent *pco, int status)
{
	pr_info("%s: status=[%s]\n", __func__,
		(status == COA_STATUS_OK) ? "OK" : "NOK");

	if (!muic_is_online()) {
		pr_info("%s: MUIC is not online.\n", __func__);
		return -1;
	}

	if (!pco->pmuic) {
		pr_info("%s: MUIC ctx is not ready.\n", __func__);
		return -1;
	}

	if ((pco->pmuic->attached_dev != ATTACHED_DEV_GAMEPAD_MUIC) &&
		(pco->pmuic->attached_dev != ATTACHED_DEV_OTG_MUIC)) {
		pr_info("%s: Abnormal state for USB's gampad Noti. [%d]\n",
			__func__, pco->pmuic->attached_dev);
		return -1;
	}

	if (status == COA_STATUS_OK) {
		if (get_adc_scan_mode(pco->pmuic) != ADC_SCANMODE_CONTINUOUS) {
			/* The interrupts occurred during mode change will be discarded. */
			pco->pmuic->discard_interrupt = true;
			set_adc_scan_mode(pco->pmuic, ADC_SCANMODE_CONTINUOUS);
			msleep(200);
			pco->pmuic->discard_interrupt = false;
		}
	} else 
		pr_info("%s: discarded.\n", __func__);

	return 0;
}

static int coagent_cmd_handler(struct coagent *pco, int cmd, int param)
{

	switch (cmd) {
	case COA_GAMEPAD_STATUS:
		coagent_cmd_gamepad(pco, param);
		break;
	default:
		break;
	}

	return 0;
}

int coagent_in(unsigned int *pbuf)
{
	struct coagent *pco = &base_coagent;

	kfifo_in(&(pco->fifo), pbuf, 1);
	up(&(pco->read_sem));

	return 0;
}

int coagent_out(unsigned int *pbuf)
{
	struct coagent *pco = &base_coagent;
	unsigned int ret = 0;

	ret = kfifo_out(&(pco->fifo), pbuf, 1);

	return ret;
}

bool coagent_alive(void)
{
	struct coagent *pco = &base_coagent;

	return pco->is_active;
}

static int __init init_fifo_test(void)
{
	struct coagent *pco = &base_coagent;
	unsigned int i;
	unsigned int val;

	pr_info("%s: fifo module insert\n", __func__);
	if (kfifo_alloc(&(pco->fifo), 1024, GFP_KERNEL)) {
		pr_warn("%s: error kfifo_alloc\n", __func__);
		return -ENOMEM;
	}

	pr_info("%s: queue size:%u\n", __func__, kfifo_size(&(pco->fifo)));
	pr_info("%s: queue_available1: %u\n", __func__, kfifo_avail(&(pco->fifo)));	
	/* enqueue */
	for (i=0; i<2; i++) {
		val = 5 + i;
		val |= (4 + i) << COAGENT_PARAM_BITS;
		coagent_in(&val);
	}

	pr_info("%s: queue len: %u\n", __func__, kfifo_len(&(pco->fifo)));
	pr_info("%s: queue_available: %u\n", __func__, kfifo_avail(&(pco->fifo)));

	return 0;
}

static void __exit exit_fifo_test(void)
{
	struct coagent *pco = &base_coagent;

	kfifo_free(&(pco->fifo));
	pr_info("%s: fifo module removed\n", __func__);
}

static int coagent_thread(void *data)
{
	struct coagent *pco = (struct coagent *)data;
	uint i = 0;
	int r;
	unsigned int rx_data = 0, cmd = 0, param = 0;

	pr_info("%s: %dth thread is running...\n", __func__, pco->co_number);

	sema_init(&(pco->read_sem), 1);

	init_fifo_test();

	pco->is_active = true;

	for(;;) {
		r = down_interruptible(&(pco->read_sem));
		if (r < 0) {
			
			pr_info("%s: down_interruptible error\n", __func__);
			goto out_error;
		}

		r = coagent_out(&rx_data);
		if ( r != 1) {
			pr_info("%s: The copied item is not one(%d)\n", __func__, r);
			continue;
		}

		cmd = COAGENT_CMD(rx_data);
		param = COAGENT_PARAM(rx_data);

		pr_info("%s: [%2d - %d/%d] cmd=%d, param=%d\n", __func__, i++,
			kfifo_len(&(pco->fifo)), kfifo_avail(&(pco->fifo)),
			cmd, param);

		coagent_cmd_handler(pco, cmd, param);
	}

out_error:

	pr_info("%s: End\n", __func__);

	return 0;
}

static int create_coagent_thread(struct coagent *pco)
{
	int error = 0;

	pr_info("%s:\n", __func__);

	pco->co_thread = kthread_create(coagent_thread, pco, "coagent%d",
						++pco->co_number);
	if (IS_ERR(pco->co_thread)) {
		pr_info("%s: Error\n", __func__);
		error = PTR_ERR(pco->co_thread);
		goto out_clr;
	}

	wake_up_process(pco->co_thread);

out_clr:
	
	return error;
}

static int __init init_coagent(void)
{
	create_coagent_thread(&base_coagent);

	return 0;
}

static void __exit exit_coagent(void)
{
	exit_fifo_test();
}

module_init(init_coagent);
module_exit(exit_fifo_test);
MODULE_LICENSE("GPL");
