/*
 * TZ ICCC Support
 *
 */
 
#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include "tz_iccc.h"

/* ICCC implementation for kernel */
int is_iccc_ready;
#define DRIVER_DESC "A kernel module to read boot_completed status"

uint8_t *iccc_tci = NULL;
struct mc_session_handle iccc_mchandle;

uint32_t Iccc_SaveData_Kernel(uint32_t type, uint32_t value)
{
	enum mc_result mc_ret;
	struct mc_uuid_t uuid = TL_TZ_ICCC_UUID;
	int ret = 0;
	tciMessage_t *msg;

	printk(KERN_ERR "ICCC : Iccc_SaveData_Kernel \n");

	if (!is_iccc_ready) {
		ret = RET_ICCC_FAIL;
		pr_err("%s: Not ready! type:%#x, ret:%d\n", __func__, type, ret);
		goto iccc_ret;
	}

	if(ICCC_SECTION_TYPE(type) != KERN_ICCC_TYPE_START) {
		if (type != SELINUX_STATUS) {
			ret = RET_ICCC_FAIL;
			pr_err("iccc permission denied! ret = 0x%x", ret);
			ret = RET_ICCC_FAIL;
			goto iccc_ret;
		}
	}

	/* open device for iccc trustlet */
	mc_ret = mc_open_device(MC_DEVICE_ID_DEFAULT);
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC --cannot get mobicore handle from kernel. %d\n",mc_ret);
		ret = RET_ICCC_FAIL;
		goto iccc_ret;
	}

	/* alloc world shared memory for iccc trustlet */
	mc_ret = mc_malloc_wsm(MC_DEVICE_ID_DEFAULT, 0, sizeof(tciMessage_t),&iccc_tci, 0);
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC --cannot alloc world shared memory.%d\n",mc_ret);
		ret = RET_ICCC_FAIL;
		goto iccc_close_device;
	}

	memset(&iccc_mchandle, 0, sizeof(struct mc_session_handle));
	iccc_mchandle.device_id = MC_DEVICE_ID_DEFAULT;

	/* open session for iccc trustlet */
	mc_ret = mc_open_session(&iccc_mchandle, &uuid, iccc_tci,sizeof(tciMessage_t));
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC : --cannot open mobicore session from kernel. %d\n",mc_ret);
		ret = RET_ICCC_FAIL;
		goto iccc_free_wsm;
	}

	msg = (tciMessage_t *)iccc_tci;

	msg->header.id = CMD_ICCC_SAVEDATA;
	msg->payload.generic.content.iccc_req.cmd_id = CMD_ICCC_SAVEDATA;
	msg->payload.generic.content.iccc_req.type = type;
	msg->payload.generic.content.iccc_req.value = value;

	/* Send the command to the tl. */
	mc_ret = mc_notify(&iccc_mchandle);
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC--mc_notify failed.\n");
		ret = RET_ICCC_FAIL;
		goto iccc_close_session;
	}

retry1:
	mc_ret = mc_wait_notification(&iccc_mchandle, -1);
	if (MC_DRV_ERR_INTERRUPTED_BY_SIGNAL == mc_ret) {
		usleep_range(1000, 5000);
		goto retry1;
	}

	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC--wait_notify failed.\n");
		ret = RET_ICCC_FAIL;
		goto iccc_close_session;
	}

	pr_warn("ICCC--wait_notify completed.\n");

	if (msg->payload.generic.content.iccc_rsp.ret == RET_ICCC_SUCCESS) {
		pr_info("ICCC : Successfully write\n");
		ret = RET_ICCC_SUCCESS;
	}
	else {
		pr_err("ICCC : write failed with error (%d)\n",msg->payload.generic.content.iccc_rsp.ret);
		ret = RET_ICCC_FAIL;
	}

iccc_close_session:
	if (mc_close_session(&iccc_mchandle) != MC_DRV_OK) {
		pr_err("ICCC--failed to close mobicore session.\n");
	}

iccc_free_wsm:
	if (mc_free_wsm(MC_DEVICE_ID_DEFAULT, iccc_tci) != MC_DRV_OK) {
		pr_err("ICCC--failed to free wsm.\n");
	}

iccc_close_device:
	if (mc_close_device(MC_DEVICE_ID_DEFAULT) != MC_DRV_OK) {
		pr_err("ICCC--failed to shutdown mobicore instance.\n");
	}

iccc_ret:
	return ret;
}

uint32_t Iccc_ReadData_Kernel(uint32_t type, uint32_t *value)
{
	enum mc_result mc_ret;
	struct mc_uuid_t uuid = TL_TZ_ICCC_UUID;
	int ret = 0;
	tciMessage_t *msg;

	printk(KERN_ERR "ICCC Iccc_ReadData_Kernel \n");

	if (!is_iccc_ready) {
		ret = RET_ICCC_FAIL;
		pr_err("%s: Not ready! type:%#x, ret:%d\n", __func__, type, ret);
		goto iccc_ret;
	}

	/* open device for iccc trustlet */
	mc_ret = mc_open_device(MC_DEVICE_ID_DEFAULT);
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC --cannot get mobicore handle from kernel. %d\n",mc_ret);
		ret = RET_ICCC_FAIL;
		goto iccc_ret;
	}

	/* alloc world shared memory for iccc trustlet */
	mc_ret = mc_malloc_wsm(MC_DEVICE_ID_DEFAULT, 0, sizeof(tciMessage_t),&iccc_tci, 0);
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC--cannot alloc world shared memory.%d\n",mc_ret);
		ret = RET_ICCC_FAIL;
		goto iccc_close_device;
	}
	memset(&iccc_mchandle, 0, sizeof(struct mc_session_handle));
	iccc_mchandle.device_id = MC_DEVICE_ID_DEFAULT;

	/* open session for iccc trustlet */
	mc_ret = mc_open_session(&iccc_mchandle, &uuid, iccc_tci,sizeof(tciMessage_t));
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC--cannot open mobicore session from kernel. %d\n",mc_ret);
		ret = RET_ICCC_FAIL;
		goto iccc_free_wsm;
	}

	/* Load message */
	msg = (tciMessage_t *)iccc_tci;
	msg->header.id = CMD_ICCC_READDATA;
	msg->payload.generic.content.iccc_req.cmd_id = CMD_ICCC_READDATA;
	msg->payload.generic.content.iccc_req.type = type;

	/* Send the command to the tl. */
	mc_ret = mc_notify(&iccc_mchandle);
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC mc_notify failed.\n");
		ret = RET_ICCC_FAIL;
		goto iccc_close_session;
	}

retry2:
	mc_ret = mc_wait_notification(&iccc_mchandle, -1);
	if (MC_DRV_ERR_INTERRUPTED_BY_SIGNAL == mc_ret) {
		usleep_range(1000, 5000);
		goto retry2;
	}
	if (mc_ret != MC_DRV_OK) {
		pr_err("ICCC--wait_notify failed.\n");
		ret = RET_ICCC_FAIL;
		goto iccc_close_session;
	}

	pr_warn("ICCC--wait_notify completed.\n");

	if (msg->payload.generic.content.iccc_rsp.ret == RET_ICCC_SUCCESS) {
		pr_info("ICCC : Iccc_ReadData_Kernel successful\n");
		ret = RET_ICCC_SUCCESS;
		*value = msg->payload.generic.content.iccc_rsp.value;
	}
	else {
		pr_err("ICCC : Iccc_ReadData_Kernel failed with error (%d)\n",msg->payload.generic.content.iccc_rsp.ret);
		ret = RET_ICCC_FAIL;
	}

iccc_close_session:
	if (mc_close_session(&iccc_mchandle) != MC_DRV_OK) {
		pr_err("ICCC--failed to close mobicore session.\n");
	}

iccc_free_wsm:
	if (mc_free_wsm(MC_DEVICE_ID_DEFAULT, iccc_tci) != MC_DRV_OK) {
		pr_err("ICCC--failed to free wsm.\n");
	}

iccc_close_device:
	if (mc_close_device(MC_DEVICE_ID_DEFAULT) != MC_DRV_OK) {
		pr_err("ICCC--failed to shutdown mobicore instance.\n");
	}

iccc_ret:
	return ret;
}

static ssize_t iccc_write(struct file *fp, const char __user *buf, size_t len, loff_t *off)
{
	uint32_t ret;
	printk(KERN_ERR "%s:\n", __func__);
	is_iccc_ready = 1;

#if defined(CONFIG_SECURITY_SELINUX)
	printk(KERN_INFO "%s: selinux_enabled:%d, selinux_enforcing:%d\n",__func__, selinux_is_enabled(), selinux_is_enforcing());
	if (selinux_is_enabled() && selinux_is_enforcing()) {
		if (0 != (ret = Iccc_SaveData_Kernel(SELINUX_STATUS,0x0))) {
			printk(KERN_ERR "%s: Iccc_SaveData_Kernel failed, type = %x, value =%x\n", __func__,SELINUX_STATUS,0x0);
		}
	}
	else {
		if (0 != (ret = Iccc_SaveData_Kernel(SELINUX_STATUS,0x1))) {
			printk(KERN_ERR "%s: Iccc_SaveData_Kernel failed, type = %x, value =%x\n", __func__,SELINUX_STATUS,0x1);
		}
	}
#endif

	// len bytes successfully written 
	return len;
}

static const struct file_operations iccc_proc_fops = {
	.write = iccc_write,
};

static int __init iccc_init(void)
{
	printk(KERN_INFO"%s:\n", __func__);

	if (proc_create("iccc_ready", 0644, NULL, &iccc_proc_fops) == NULL) {
		printk(KERN_ERR "%s: proc_create() failed\n",__func__);
		return -1;
	}
	printk(KERN_INFO"%s: registered /proc/iccc_boot_completed interface\n", __func__);
	return 0;
}

static void __exit iccc_exit(void)
{
	printk(KERN_INFO"deregistering /proc/iccc_boot_completed interface\n");
	remove_proc_entry("iccc_ready", NULL);
}

module_init(iccc_init);
module_exit(iccc_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
/* END ICCC implementation for kernel */