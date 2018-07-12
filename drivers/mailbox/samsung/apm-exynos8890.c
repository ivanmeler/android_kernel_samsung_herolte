/* drivers/mailbox/samsung/apm-exynos8890.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS8890 - APM driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/err.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mailbox_client.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/mfd/samsung/core.h>
#include <linux/apm-exynos.h>
#include <linux/mailbox-exynos.h>
#include <asm/io.h>
#include <soc/samsung/asv-exynos.h>

#define PMIC_MIF_OUT			(0x1B)
#define PMIC_ATL_OUT			(0x1D)
#define PMIC_APO_OUT			(0x1F)
#define PMIC_G3D_OUT			(0x26)

extern int apm_wfi_prepare;
static DEFINE_MUTEX(cl_mutex);
char* protocol_name;

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
u32 mif_in_voltage;
u32 atl_in_voltage;
u32 apo_in_voltage;
u32 g3d_in_voltage;
#endif
u32 mngs_lv;
u32 apo_lv;
u32 gpu_lv;
u32 mif_lv;

struct mbox_client cl;

void exynos8890_apm_power_up(void)
{
	u32 tmp;

	tmp = exynos_cortexm3_pmu_read(EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION);
	tmp &= APM_LOCAL_PWR_CFG_RESET;
	tmp |= APM_LOCAL_PWR_CFG_RUN;
	exynos_cortexm3_pmu_write(tmp, EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION);
}

void exynos8890_apm_power_down(void)
{
	u32 tmp;

	/* Reset CORTEX M3 */
	tmp = exynos_cortexm3_pmu_read(EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION);
	tmp &= APM_LOCAL_PWR_CFG_RESET;
	exynos_cortexm3_pmu_write(tmp, EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION);
}

/* exynos8890_apm_reset_release
 * Reset signal release to PMU setting.
 */
void exynos8890_apm_reset_release(void)
{
	unsigned int tmp;

	/* Cortex M3 Interrupt bit clear */
	exynos_mailbox_reg_write(0x0, EXYNOS_MAILBOX_TX_IRQ0);
	exynos_mailbox_reg_write(0x0, EXYNOS_MAILBOX_RX_IRQ0);

	/* Set APM device enable */
	tmp = exynos_cortexm3_pmu_read(EXYNOS_PMU_CORTEXM3_APM_OPTION);
	tmp &= ~ENABLE_APM;
	tmp |= ENABLE_APM;
	exynos_cortexm3_pmu_write(tmp, EXYNOS_PMU_CORTEXM3_APM_OPTION);
}


/* check_rx_data function check return value.
 * CM3 send return value. 0xA is sucess, 0x0 is fail value.
 * check_rx_data function check this value. So sucess return 0.
 */
static int check_rx_data(void *msg)
{
	u8 i;
	u32 buf[MSG_LEN] = {0, 0, 0, 0, 0};

	for (i = 0; i < MBOX_LEN; i++)
		buf[i] = exynos_mailbox_reg_read(EXYNOS_MAILBOX_RX_MSG(i));

	/* Check return command */
	buf[4] = exynos_mailbox_reg_read(EXYNOS_MAILBOX_TX_MSG0);

	/* Check apm device return value */
	if (buf[1] == APM_GPIO_ERR)
		return APM_GPIO_ERR;

	/* PMIC No ACK return value */
	if (buf[1] == PMIC_NO_ACK_ERR)
		return PMIC_NO_ACK_ERR;

	/* Multi byte condition */
	if ((buf[4] >> MULTI_BYTE_SHIFT) & MULTI_BYTE_MASK) {
		return 0;
	}

	/* Normal condition */
	if (((buf[4] >> COMMAND_SHIFT) & COMMAND_MASK) != READ_MODE) {
		if (buf[1] == APM_RET_SUCESS) {
			return 0;
		} else {
			pr_err("mailbox err : return incorrect\n");
			data_history();
			return -1;
		}
	} else if (((buf[4] >> COMMAND_SHIFT) & COMMAND_MASK) == READ_MODE) {
		return buf[1];
	}

	return 0;
}
EXPORT_SYMBOL_GPL(check_rx_data);

/* Setting channel ack_mode condition */
static void channel_ack_mode(struct mbox_client *client)
{
	client->rx_callback = NULL;
	client->tx_done = NULL;
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
	client->tx_block = true;
#endif
#ifdef CONFIG_EXYNOS_MBOX_POLLING
	client->tx_block = NULL;
#endif
	client->tx_tout = TIMEOUT;
	client->knows_txdone = false;
}
EXPORT_SYMBOL_GPL(channel_ack_mode);

static int exynos_send_message(struct mbox_client *mbox_cl, void *msg)
{
	struct mbox_chan *chan;
	int ret;

	chan = mbox_request_channel(mbox_cl, 0);
	if (IS_ERR(chan)) {
		pr_err("mailbox : Did not make a mailbox channel\n");
		return PTR_ERR(chan);
	}

	if (!mbox_send_message(chan, (void *)msg)) {
		ret = check_rx_data((void *)msg);
		if (ret == APM_GPIO_ERR) {
			pr_err("mailbox : gpio not set to gpio-i2c \n");
			apm_wfi_prepare = APM_ON;
			mbox_free_channel(chan);
			return ERR_TIMEOUT;
		} else if (ret < 0) {
			pr_err("[%s] mailbox send error \n", __func__);
			mbox_free_channel(chan);
			return ERR_OUT;
		}
	} else {
		pr_err("%s : Mailbox timeout\n", __func__);
		pr_err("POLLING status: 0x%x\n", exynos_mailbox_reg_read(EXYNOS_MAILBOX_RX_IRQ0));
		apm_wfi_prepare = APM_ON;
		mbox_free_channel(chan);
		return ERR_TIMEOUT;
	}
	mbox_free_channel(chan);

       return 0;
}

static int exynos_send_message_bulk_read(struct mbox_client *mbox_cl, void *msg)
{
	struct mbox_chan *chan;
	int ret;

	chan = mbox_request_channel(mbox_cl, 0);
	if (IS_ERR(chan)) {
		pr_err("mailbox : Did not make a mailbox channel\n");
		return PTR_ERR(chan);
	}

	if (!mbox_send_message(chan, (void *)msg)) {
		ret = check_rx_data((void *)msg);
		if (ret < 0) {
			pr_err("[%s] mailbox send error \n", __func__);
			return ERR_RETRY;
		} else if (ret == APM_GPIO_ERR) {
			apm_wfi_prepare = APM_ON;
			mbox_free_channel(chan);
			return ERR_TIMEOUT;
		}
	} else {
		pr_err("%s : Mailbox timeout error \n", __func__);
		apm_wfi_prepare = APM_ON;
		mbox_free_channel(chan);
		return ERR_TIMEOUT;
	}
	mbox_free_channel(chan);

return 0;
}

static int exynos8890_do_cl_dvfs_setup(unsigned int atlas_cl_limit, unsigned int apollo_cl_limit,
					unsigned int g3d_cl_limit, unsigned int mif_cl_limit, unsigned int cl_period)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	int ret;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "setup";

	msg[0] = (NONE << COMMAND_SHIFT) | (INIT_SET << INIT_MODE_SHIFT);
	msg[1] = (atlas_cl_limit << ATLAS_SHIFT) | (apollo_cl_limit << APOLLO_SHIFT)
		| (g3d_cl_limit << G3D_SHIFT) | (mif_cl_limit << MIF_SHIFT) | (cl_period << PERIOD_SHIFT);
	msg[4] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return 0;
/* out means turn off apm device and then mode change */
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_do_cl_dvfs_setup);

static int exynos8890_do_cl_dvfs_start(unsigned int cl_domain)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	int ret;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);

	/* CL-DVFS[29] start, command mode none */
	msg[0] = CL_DVFS | (NONE << COMMAND_SHIFT);
	msg[3] = ((cl_domain + 1) << CL_DOMAIN_SHIFT);
	msg[4] = TX_INTERRUPT_ENABLE;

	if (cl_domain == ID_CL1)
		protocol_name = "cl_start(ATL)--";
	else if (cl_domain == ID_CL0)
		protocol_name = "cl_start(APO)--";
	else if (cl_domain == ID_MIF)
		protocol_name = "cl_start(MIF)--";
	else if (cl_domain == ID_G3D)
		protocol_name = "cl_start(G3D)--";

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return 0;
/* out means turn off apm device and then mode change */
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_do_cl_dvfs_start);

static int exynos8890_do_cl_dvfs_mode_enable(void)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "cl_mode_enable";

	/* CL-DVFS[29] stop, command mode none */
	msg[0] = (1 << CL_ALL_START_SHIFT);
	msg[4] = (TX_INTERRUPT_ENABLE);

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
/* out means turn off apm device and then mode change */
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_do_cl_dvfs_mode_enable);

static int exynos8890_do_cl_dvfs_mode_disable(void)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "cl_mode_disable";

	/* CL-DVFS[29] stop, command mode none */
	msg[0] = (1 << CL_ALL_STOP_SHIFT);
	msg[4] = (TX_INTERRUPT_ENABLE);

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
/* out means turn off apm device and then mode change */
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_do_cl_dvfs_mode_disable);

static int exynos8890_do_cl_dvfs_stop(unsigned int cl_domain, unsigned int level)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	switch (cl_domain) {
	case ID_CL1 :
		mngs_lv = level;
		break;
	case ID_CL0 :
		apo_lv = level;
		break;
	case ID_MIF :
		mif_lv = level;
		break;
	case ID_G3D :
		level = level + G3D_LV_OFFSET;
		gpu_lv = level;
		break;
	default:
		break;
	}

	channel_ack_mode(&cl);
	protocol_name = "cl_stop";

	/* CL-DVFS[29] stop, command mode none */
	msg[0] = (CL_DVFS_OFF << CL_DVFS_SHIFT) | (NONE << COMMAND_SHIFT);
	msg[1] = level;
	msg[3] = ((cl_domain + 1) << CL_DOMAIN_SHIFT);
	msg[4] = TX_INTERRUPT_ENABLE;

	if (cl_domain == ID_CL1)
		protocol_name = "cl_stop(ATL)++";
	else if (cl_domain == ID_CL0)
		protocol_name = "cl_stop(APO)++";
	else if (cl_domain == ID_MIF)
		protocol_name = "cl_stop(MIF)++";
	else if (cl_domain == ID_G3D)
		protocol_name = "cl_stop(G3D)++";

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
/* out means turn off apm device and then mode change */
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_do_cl_dvfs_stop);

static int exynos8890_do_g3d_power_on_noti_apm(void)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "g3d_power_on";

	/* CL-DVFS[29] stop, command mode none */
	msg[3] = (1 << 13);
	msg[4] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return 0;
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_do_g3d_power_on_noti_apm);

static int exynos8890_do_g3d_power_down_noti_apm(void)
{
	u32 msg[5] = {0, 0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "g3d_power_off";

	/* CL-DVFS[29] stop, command mode none */
	msg[3] = (1 << 12);
	msg[4] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return 0;
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_do_g3d_power_down_noti_apm);

/* exynos8890_apm_enter_wfi();
 * This function send CM3 go to WFI message to CM3.
 */
int exynos8890_apm_enter_wfi(void)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	struct mbox_chan *chan;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare == APM_TIMEOUT) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "enter_wfi";

	/* CL-DVFS[29] stop, command mode none */
	msg[0] = 1 << 23;
	msg[4] = TX_INTERRUPT_ENABLE;

	chan = mbox_request_channel(&cl, 0);
	if (IS_ERR(chan)) {
		pr_err("mailbox : Did not make a mailbox channel\n");
		mutex_unlock(&cl_mutex);
		return PTR_ERR(chan);
	}
	if (!mbox_send_message(chan, (void *)msg)) {
	}
	mbox_free_channel(chan);
	mutex_unlock(&cl_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos8890_apm_enter_wfi);

/**
 * exynos8890_apm_update_bits(): Mask a value, after then write value.
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @mask : masking value
 * @value : Pointer to store write value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos8890_apm_update_bits(unsigned int type, unsigned int reg,
					unsigned int mask, unsigned int value)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	int ret;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "update_bits";

	/* CL-DVFS[29] stop, command mode write(0x0), mask mode enable */
	msg[0] = ((type << PM_SECTION_SHIFT) | (MASK << MASK_SHIFT) | (mask));
	msg[1] = reg;
	msg[2] = value;
	msg[4] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_apm_update_bits);

/**
 * exynos8890_apm_write()
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @value : Pointer to store write value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos8890_apm_write(unsigned int type, unsigned int reg, unsigned int value)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	int ret;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "write";

	/* CL-DVFS[29] stop, command mode write(0x0) */
	msg[0] = (type << PM_SECTION_SHIFT);
	msg[1] = reg;
	msg[2] = value;
	msg[4] = TX_INTERRUPT_ENABLE;

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	if (reg == PMIC_MIF_OUT) {
		mif_in_voltage = ((value * (u32)PMIC_STEP) + MIN_VOL);
	} else if (reg == PMIC_ATL_OUT) {
		atl_in_voltage = ((value * (u32)PMIC_STEP) + MIN_VOL);
	} else if (reg == PMIC_APO_OUT) {
		apo_in_voltage = ((value * (u32)PMIC_STEP) + MIN_VOL);
	} else if (reg == PMIC_G3D_OUT) {
		g3d_in_voltage = ((value * (u32)PMIC_STEP) + MIN_VOL);
	}
#endif

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_apm_write);

/**
 * exynos8890_apm_bulk_write()
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @*buf: write buffer section
 * @value : Pointer to store write value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos8890_apm_bulk_write(unsigned int type, unsigned char reg, unsigned char *buf, unsigned int count)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	unsigned int i;
	int ret;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "bulk_write";

	msg[0] = (type << PM_SECTION_SHIFT) | ((count-1) << MULTI_BYTE_CNT_SHIFT) | reg;

	for (i = 0; i < count; i++) {
		if (i < BYTE_4)
			msg[1] |= buf[i] << BYTE_SHIFT * i;
		else
			msg[2] |= buf[i] << BYTE_SHIFT * (i - BYTE_4);
	}
	msg[4] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_apm_bulk_write);

/**
 * exynos8890_apm_read()
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @*val: store read value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */

int exynos8890_apm_read(unsigned int type, unsigned int reg, unsigned int *val)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	struct mbox_chan *chan;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "read";

	/* CL-DVFS[29] stop, command mode read(0x1) */
	msg[0] = (READ_MODE << COMMAND_SHIFT) | (type << PM_SECTION_SHIFT);
	msg[1] = reg;
	msg[4] = TX_INTERRUPT_ENABLE;

	chan = mbox_request_channel(&cl, 0);
	if (IS_ERR(chan)) {
		pr_err("mailbox : Did not make a mailbox channel\n");
		mutex_unlock(&cl_mutex);
		return PTR_ERR(chan);
	}

	if (!mbox_send_message(chan, (void *)msg)) {
		*val = check_rx_data((void *)msg);
		if (*val == APM_GPIO_ERR) {
			pr_err("%s, gpio error\n", __func__);
			mbox_free_channel(chan);
			data_history();
			goto timeout;
		}
	} else {
		pr_err("%s : Mailbox timeout error \n", __func__);
		apm_wfi_prepare = APM_ON;
		mbox_free_channel(chan);
		data_history();
		goto timeout;
	}

	mbox_free_channel(chan);
	mutex_unlock(&cl_mutex);
	return 0;
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_apm_read);

/**
 * exynos8890_apm_bulk_read()
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @*buf: read buffer section
 * @*count : read count
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos8890_apm_bulk_read(unsigned int type, unsigned char reg, unsigned char *buf, unsigned int count)
{
	u32 msg[MSG_LEN] = {0, 0, 0, 0, 0};
	u32 result[2] = {0, 0};
	unsigned int ret, i;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "bulk_read";

	msg[0] = (READ_MODE << COMMAND_SHIFT) | (type << PM_SECTION_SHIFT)
			| ((count-1) << MULTI_BYTE_CNT_SHIFT) | (reg);
	msg[4] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message_bulk_read(&cl, msg);
	if (ret == ERR_TIMEOUT) {
		data_history();
		goto timeout;
	}

	result[0] = exynos_mailbox_reg_read(EXYNOS_MAILBOX_RX_MSG(1));
	result[1] = exynos_mailbox_reg_read(EXYNOS_MAILBOX_RX_MSG(2));

	for (i = 0; i < count; i++) {
		if (i < BYTE_4)
			buf[i] = (result[0] >> i * BYTE_SHIFT) & BYTE_MASK;
		else
			buf[i] = (result[1] >> (i - BYTE_4) * BYTE_SHIFT) & BYTE_MASK;
	}

	mutex_unlock(&cl_mutex);

	return 0;
timeout :
	exynos8890_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos8890_apm_bulk_read);

void exynos_mbox_client_init(struct device *dev)
{
	cl.dev = dev;
}
EXPORT_SYMBOL_GPL(exynos_mbox_client_init);

struct cl_ops exynos_cl_function_ops = {
	.cl_dvfs_setup			= exynos8890_do_cl_dvfs_setup,
	.cl_dvfs_start			= exynos8890_do_cl_dvfs_start,
	.cl_dvfs_stop			= exynos8890_do_cl_dvfs_stop,
	.cl_dvfs_enable			= exynos8890_do_cl_dvfs_mode_enable,
	.cl_dvfs_disable		= exynos8890_do_cl_dvfs_mode_disable,
	.g3d_power_on			= exynos8890_do_g3d_power_on_noti_apm,
	.g3d_power_down			= exynos8890_do_g3d_power_down_noti_apm,
	.enter_wfi			= exynos8890_apm_enter_wfi,
	.apm_reset			= exynos8890_apm_reset_release,
	.apm_power_up			= exynos8890_apm_power_up,
	.apm_power_down			= exynos8890_apm_power_down,
};

struct apm_ops exynos_apm_function_ops = {
	.apm_update_bits	= exynos8890_apm_update_bits,
	.apm_write		= exynos8890_apm_write,
	.apm_bulk_write		= exynos8890_apm_bulk_write,
	.apm_read		= exynos8890_apm_read,
	.apm_bulk_read		= exynos8890_apm_bulk_read,
};
