/*
 * Copyright 2013 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/syscore_ops.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/reboot.h>
#include <linux/fb.h>
#include <linux/mfd/samsung/core.h>
#include <linux/exynos-ss.h>
#include <linux/apm-exynos.h>
#include <linux/mailbox-exynos.h>
#include <asm-generic/checksum.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <soc/samsung/asv-exynos.h>
#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-powermode.h>
#include <soc/samsung/ect_parser.h>
#include "../../soc/samsung/pwrcal/pwrcal.h"

/* firmware file information */
#define fw_checksum	61921
char *firmware_file = "APMFW";
void __iomem *sram_base;
void __iomem *exynos_mailbox_reg;
struct regmap *cortexm3_pmu_regmap;

extern char* protocol_name;

int mbox_irq;
unsigned int sram_status = 0;
unsigned int dram_checksum = 0;
static struct workqueue_struct *mailbox_wq;
static struct cl_init_data cl_init;
static struct class *mailbox_class;
static struct debug_data tx, rx;
static unsigned int firmware_load_mode;

extern u32 mngs_lv;
extern u32 apo_lv;
extern u32 gpu_lv;
extern u32 mif_lv;

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
u32 atl_voltage;
u32 apo_voltage;
u32 g3d_voltage;
u32 mif_voltage;

extern u32 mif_in_voltage;
extern u32 atl_in_voltage;
extern u32 apo_in_voltage;
extern u32 g3d_in_voltage;
#else
u32 default_vol[4] = {100000, 100000, 100000, 100000};
#endif

static bool apm_power_down = false;
static int idle_ip_index;

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
void samsung_mbox_enable_irq(void)
{
	enable_irq(mbox_irq);
}

void samsung_mbox_disable_irq(void)
{
	disable_irq(mbox_irq);
}
#endif

static inline struct samsung_mlink *to_samsung_mchan(struct mbox_chan *pchan)
{
	if (!pchan)
		return NULL;

	return container_of(pchan, struct samsung_mlink, chan);
}

static inline struct samsung_mbox *to_samsung_mbox(struct mbox_chan *pchan)
{
	if (!pchan)
		return NULL;

	return to_samsung_mchan(pchan)->smc;
}

#ifdef CONFIG_ECT
int memcpy_integer(char *src1, char *src2, unsigned int size)
{
	int i, chunk_count, remain_count;
	unsigned int *int_src1 = (unsigned int *)src1;
	unsigned int *int_src2 = (unsigned int *)src2;

	chunk_count = size / sizeof(int);
	remain_count = size % sizeof(int);

	for (i = 0; i < chunk_count; ++i) {
		*int_src1 = *int_src2;
		int_src1++;
		int_src2++;
	}

	src1 += chunk_count * sizeof(int);
	src2 += chunk_count * sizeof(int);

	for (i = 0; i < remain_count; ++i) {
		*src1 = *src2;
		src1++;
		src2++;
	}

	return 0;
}

int memcmp_integer(char *src1, char *src2, unsigned int size)
{
	int i, chunk_count, remain_count;
	unsigned int *int_src1 = (unsigned int *)src1;
	unsigned int *int_src2 = (unsigned int *)src2;

	chunk_count = size / sizeof(int);
	remain_count = size % sizeof(int);

	for (i = 0; i < chunk_count; ++i) {
		if (*int_src1 != *int_src2)
			return *int_src1 - *int_src2;

		int_src1++;
		int_src2++;
	}

	src1 += chunk_count * sizeof(int);
	src2 += chunk_count * sizeof(int);

	for(i = 0; i < remain_count; ++i) {
		if (*src1 != *src2)
			return *src1 - *src2;
		src1++;
		src2++;
	}

	return 0;
}
#endif

static void firmware_load(const char *firmware, int size)
{
	dram_checksum = csum_partial(firmware, size, 0);

	memcpy(sram_base, firmware, size);

	pr_info("OK \n");
}

static int firmware_update(struct device *dev)
{
	const struct firmware *fw_entry = NULL;
#ifdef CONFIG_ECT
	struct ect_bin *fw = NULL;
	void *firmware;
#endif
	int err;

	dev_info(dev, "Loading APM firmware ... ");

#ifdef CONFIG_ECT
	firmware = ect_get_block(BLOCK_BIN);
	if (firmware) {
		fw = ect_binary_get_bin(firmware, firmware_file);
		if (fw)
			firmware_load_mode = 1;
	}
#endif
	if (firmware_load_mode) {
		dram_checksum = csum_partial(fw->ptr, fw->binary_size, 0);
		memcpy_integer(sram_base, fw->ptr, fw->binary_size);
		pr_info("OK : Use ect func\n");
	} else {
#ifdef CONFIG_ECT
		pr_info("ECT firmware file is not exist, so do not turn on APM \n");
		return -1;
#endif
		err = request_firmware(&fw_entry, firmware_file, dev);
		if (err) {
			dev_err(dev, "FAIL \n");
			return err;
		}
		firmware_load(fw_entry->data, fw_entry->size);
		release_firmware(fw_entry);
		pr_info("OK : Use firmware f/w\n");
	}

	return 0;
}

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
static int samsung_mbox_startup(struct mbox_chan *chan)
{
	return 0;
}
#else
static int samsung_mbox_startup(struct mbox_chan *chan) { return 0;}
#endif

static int samsung_mbox_send_data(struct mbox_chan *chan, void *msg)
{
	struct samsung_mlink *samsung_link = to_samsung_mchan(chan);
	u32 *msg_data = (u32 *)msg;
	u32 i, status, limit_cnt = 0;

	samsung_link->data = msg_data;

	/* Check rx interrupt status */
	/* rx interrupt is clear, and then main core send message */
	do {
		status = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0) & CM3_INTERRPUT_SHIFT;
		limit_cnt++;
		if (limit_cnt > CM3_COUNT_MAX)
			return -1;
	} while (status);

	/* Insert apm debug buffer */
	tx.buf[tx.cnt][G3D_STATUS] = (exynos_cortexm3_pmu_read(EXYNOS_PMU_G3D_STATUS) & G3D_STATUS_MASK);
	tx.name[tx.cnt] = protocol_name;

	limit_cnt = 0;
	/* Check Tx interrupt status */
	do {
		status = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_TX_IRQ0) & CM3_INTERRPUT_SHIFT;
		limit_cnt++;
		if (limit_cnt > CM3_COUNT_MAX)
			return -1;
	} while (status);

	/* Save information and data to mailbox SFR */
	for (i = 0; i < MAILBOX_REG_CNT; i++) {
		writel(msg_data[i], exynos_mailbox_reg + EXYNOS_MAILBOX_TX_MSG(i));
		tx.buf[tx.cnt][i] = readl(exynos_mailbox_reg + EXYNOS_MAILBOX_TX_MSG(i));
		/* Save a time and message information */
		tx.time[tx.cnt] = ktime_to_ms(ktime_get());
	}
	/* Send tx interrupt */
	writel(msg_data[4], exynos_mailbox_reg + EXYNOS_MAILBOX_TX_IRQ0);

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	tx.vol[rx.cnt][0] = atl_in_voltage;
	tx.vol[rx.cnt][1] = apo_in_voltage;
	tx.vol[rx.cnt][2] = g3d_in_voltage;
	tx.vol[rx.cnt][3] = mif_in_voltage;
	exynos_ss_mailbox(msg_data, 0, protocol_name, tx.vol[rx.cnt]);
#else
	exynos_ss_mailbox(msg_data, 0, protocol_name, default_vol);
#endif

	tx.cnt++;
	if (tx.cnt == DEBUG_COUNT) tx.cnt = 0;

	return 0;
}

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
static void samsung_mbox_shutdown(struct mbox_chan *chan)
{
}
#else
static void samsung_mbox_shutdown(struct mbox_chan *chan) {}
#endif

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
static irqreturn_t samsung_ipc_handler(int irq, void *p)
{
	struct samsung_mlink *plink = p;
	u32 tmp;
	u32 i;

	mbox_link_txdone(&plink->link, MBOX_OK);

	/* Acknowledge the interrupt by clearing the interrupt register */
	tmp = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0);
	tmp &= ~RX_INT_CLEAR;
	__raw_writel(tmp, exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0);

	/* Debug information */
	rx.buf[rx.cnt][G3D_STATUS] = (exynos_cortexm3_pmu_read(EXYNOS_PMU_G3D_STATUS) & G3D_STATUS_MASK);
	rx.name[rx.cnt] = protocol_name;
	for (i = 0; i < MAILBOX_REG_CNT; i++) {
		rx.time[rx.cnt] = ktime_to_ms(ktime_get());
		rx.buf[rx.cnt][i] = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG(i));
	}
	rx.buf[rx.cnt][INT_STATUS] = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0);

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	/* Debug register clear */
	__raw_writel(0x0, exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG0);

	/* Check firmware setting voltage */
	rx.atl_value = ((rx.buf[rx.cnt][0] >> ATL_VOL_SHIFT) & VOL_MASK);
	rx.apo_value = ((rx.buf[rx.cnt][0] >> APO_VOL_SHIFT) & VOL_MASK);
	rx.g3d_value = ((rx.buf[rx.cnt][0] >> G3D_VOL_SHIFT) & VOL_MASK);
	rx.mif_value = ((rx.buf[rx.cnt][0] >> MIF_VOL_SHIFT) & VOL_MASK);

	atl_voltage = ((rx.atl_value * (u32)PMIC_STEP) + MIN_VOL);
	apo_voltage = ((rx.apo_value * (u32)PMIC_STEP) + MIN_VOL);
	g3d_voltage = ((rx.g3d_value * (u32)PMIC_STEP) + MIN_VOL);
	mif_voltage = ((rx.mif_value * (u32)PMIC_STEP) + MIN_VOL);

	rx.vol[rx.cnt][0] = atl_voltage;
	rx.vol[rx.cnt][1] = apo_voltage;
	rx.vol[rx.cnt][2] = g3d_voltage;
	rx.vol[rx.cnt][3] = mif_voltage;
	exynos_ss_mailbox(rx.buf[rx.cnt], 1, protocol_name, rx.vol[rx.cnt]);
#else
	exynos_ss_mailbox(rx.buf[rx.cnt], 1, protocol_name, default_vol);
#endif

	rx.cnt++;
	if (rx.cnt == DEBUG_COUNT) rx.cnt = 0;

	return IRQ_HANDLED;
}
#endif

int samsung_mbox_last_tx_done(struct mbox_chan *chan)
{
	unsigned int status, limit_cnt = 0, tmp, i;
	ktime_t __start;
	s64 __elapsed;

	__start = ktime_get();
	do {
		if (limit_cnt) {
			cpu_relax();
			usleep_range(28, 30);
		}

		status = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0) & CM3_POLLING_SHIFT;

		limit_cnt++;

		/* RX interrupt timeout check */
		if (limit_cnt > 100) {
			__elapsed = ktime_to_ns(ktime_sub(ktime_get(), __start));
			pr_info("mailbox timeout : %lld ns \n", __elapsed);
			return -EIO;
		}
	} while(!status);

	/* Acknowledge the interrupt by clearing the interrupt register */
	tmp = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0);
	tmp &= ~RX_INT_CLEAR;
	__raw_writel(tmp, exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0);

	/* Debug information */
	rx.buf[rx.cnt][G3D_STATUS] = (exynos_cortexm3_pmu_read(EXYNOS_PMU_G3D_STATUS) & G3D_STATUS_MASK);
	rx.name[rx.cnt] = protocol_name;
	for (i = 0; i < MAILBOX_REG_CNT; i++) {
		rx.time[rx.cnt] = ktime_to_ms(ktime_get());
		rx.buf[rx.cnt][i] = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG(i));
	}
	rx.buf[rx.cnt][INT_STATUS] = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0);

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	/* Debug register clear */
	__raw_writel(0x0, exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG0);

	/* Check firmware setting voltage */
	rx.atl_value = ((rx.buf[rx.cnt][0] >> ATL_VOL_SHIFT) & VOL_MASK);
	rx.apo_value = ((rx.buf[rx.cnt][0] >> APO_VOL_SHIFT) & VOL_MASK);
	rx.g3d_value = ((rx.buf[rx.cnt][0] >> G3D_VOL_SHIFT) & VOL_MASK);
	rx.mif_value = ((rx.buf[rx.cnt][0] >> MIF_VOL_SHIFT) & VOL_MASK);

	atl_voltage = ((rx.atl_value * (u32)PMIC_STEP) + MIN_VOL);
	apo_voltage = ((rx.apo_value * (u32)PMIC_STEP) + MIN_VOL);
	g3d_voltage = ((rx.g3d_value * (u32)PMIC_STEP) + MIN_VOL);
	mif_voltage = ((rx.mif_value * (u32)PMIC_STEP) + MIN_VOL);

	rx.vol[rx.cnt][0] = atl_voltage;
	rx.vol[rx.cnt][1] = apo_voltage;
	rx.vol[rx.cnt][2] = g3d_voltage;
	rx.vol[rx.cnt][3] = mif_voltage;
	exynos_ss_mailbox(rx.buf[rx.cnt], 1, protocol_name, rx.vol[rx.cnt]);
#else
	exynos_ss_mailbox(rx.buf[rx.cnt], 1, protocol_name, default_vol);
#endif

	rx.cnt++;
	if (rx.cnt == DEBUG_COUNT) rx.cnt = 0;

	return 0;
}

static struct mbox_chan_ops samsung_mbox_ops = {
	.send_data = samsung_mbox_send_data,
	.startup = samsung_mbox_startup,
	.shutdown = samsung_mbox_shutdown,
	.last_tx_done = samsung_mbox_last_tx_done,
};

/*
 * [DEBUG] Check mailbox related register and TX/RX data history
 */
int data_history(void) {
	pr_info("EXYNOS_MAILBOX_TX_MSG0 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_TX_MSG0));
	pr_info("EXYNOS_MAILBOX_TX_MSG1 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_TX_MSG1));
	pr_info("EXYNOS_MAILBOX_TX_MSG2 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_TX_MSG2));
	pr_info("EXYNOS_MAILBOX_TX_MSG3 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_TX_MSG3));
	pr_info("EXYNOS_MAILBOX_TX_IRQ0 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_TX_IRQ0));
	pr_info("EXYNOS_MAILBOX_RX_MSG0 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG0));
	pr_info("EXYNOS_MAILBOX_RX_MSG1 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG1));
	pr_info("EXYNOS_MAILBOX_RX_MSG2 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG2));
	pr_info("EXYNOS_MAILBOX_RX_MSG3 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG3));
	pr_info("EXYNOS_MAILBOX_RX_IRQ0 : %8.0x \n", __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_IRQ0));
	pr_info("EXYNOS_CORTEXM3_APM_CONFIGURATION : %8.0x \n",
		exynos_cortexm3_pmu_read(EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION));
	pr_info("EXYNOS_CORTEXM3_APM_STATUS : %8.0x \n",
		exynos_cortexm3_pmu_read(EXYNOS_PMU_CORTEXM3_APM_STATUS));
	pr_info("EXYNOS_CORTEXM3_APM_OPTION : %8.0x \n",
		exynos_cortexm3_pmu_read(EXYNOS_PMU_CORTEXM3_APM_OPTION));

	return 0;
}

static void thread_mailbox_work(struct work_struct *work)
{
	const struct firmware *fw_entry = NULL;
	unsigned int tmp, status, limit_cnt = 0;
	unsigned int sram_checksum = 0;
	struct ect_bin *fw;
	void *firmware;
	int ret, err;

	if (!apm_power_down) {
		if (firmware_load_mode) {
			firmware = ect_get_block(BLOCK_BIN);
			fw = ect_binary_get_bin(firmware, firmware_file);
			sram_checksum = csum_partial(sram_base, fw->binary_size, 0);
			ret = memcmp_integer(sram_base, fw->ptr, fw->binary_size);
		} else {
			err = request_firmware(&fw_entry, firmware_file, NULL);
			if (err) {
				pr_err("mailbox : request firmware fail \n");
			}
			sram_checksum = csum_partial(sram_base, fw_entry->size, 0);
			ret = memcmp(sram_base, fw_entry->data, fw_entry->size);
			release_firmware(fw_entry);
		}
		if (ret) {
			/* Cortex M3 compare error case */
			sram_status = SRAM_UNSTABLE;
			pr_info("mailbox : APM SRAM compare error\n");
		} else {
			/* Cortex M3 compare sucess case */
			sram_status = SRAM_STABLE;
			pr_info("mailbox : APM SRAM stable \n");
		}
		pr_info("mailbox : fw_checksum [%d], DRAM checksum [%d], sram checksum [%d] \n",
								fw_checksum, dram_checksum, sram_checksum);

		/* This condition is lcd on */
		/* Local power up to cortex M3 */
		if (sram_status == SRAM_STABLE) {
			exynos_update_ip_idle_status(idle_ip_index, 0);

			exynos_apm_power_up();

			/* Enable CORTEX M3 */
			exynos_apm_reset_release();

			cl_init.apm_status = APM_ON;
			/* Call apm notifier */
			sec_core_lock();
			cl_dvfs_lock();
			apm_notifier_call_chain(APM_READY);
			cl_dvfs_unlock();
			sec_core_unlock();

			/* Set CL-DVFS voltage margin limit and CL-DVFS period */
			ret = exynos_cl_dvfs_setup(cl_init.atlas_margin, cl_init.apollo_margin,
							cl_init.g3d_margin, cl_init.mif_margin, cl_init.period);
			if (ret)
				pr_warn("mailbox : Do not set margin and period information\n");
		}
	} else {
		/* This condition is lcd off */
		if (sram_status == SRAM_STABLE) {
			if (cl_init.apm_status == APM_OFF) {
				pr_info("APM device already disable staus \n");
				return;
			}

#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
			exynos_cl_dvfs_mode_disable();
			cl_init.apm_status = APM_OFF;
#endif
			sec_core_lock();
			cl_dvfs_lock();
			apm_notifier_call_chain(APM_SLEEP);
			cl_dvfs_unlock();
			sec_core_unlock();

			/* send message go to wfi */
			exynos_apm_enter_wfi();

			/* Check cortex M3 core enter wfi mode */
			do {
				tmp = exynos_cortexm3_pmu_read(EXYNOS_PMU_CORTEXM3_APM_STATUS);
				status = (tmp >> STANDBYWFI) & STANDBYWFI_MASK;
				limit_cnt++;
				if (limit_cnt > CM3_COUNT_MAX) break;
			} while (!status);

			/* local power down(reset) to CORTEX M3 */
			exynos_apm_power_down();

			exynos_update_ip_idle_status(idle_ip_index, 1);
		}
	}
}
static DECLARE_WORK(mailbox_work, thread_mailbox_work);

static int exynos_mailbox_fb_notifier(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *info = evdata->info;
	unsigned int blank;

	if (val != FB_EVENT_BLANK &&
		val != FB_R_EARLY_EVENT_BLANK)
		return 0;
	/*
	 * If FBNODE is not zero, it is not primary display(LCD)
	 * and don't need to process these scheduling.
	 */
	if (info->node)
		return NOTIFY_OK;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		if (mailbox_wq) {
			if (work_pending(&mailbox_work))
				flush_work(&mailbox_work);
			apm_power_down = true;
			queue_work(mailbox_wq, &mailbox_work);
		}
		break;
	case FB_BLANK_UNBLANK:
		if (mailbox_wq) {
			if (work_pending(&mailbox_work))
				flush_work(&mailbox_work);
			apm_power_down = false;
			queue_work(mailbox_wq, &mailbox_work);
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_mailbox_fb_notifier_block = {
	.notifier_call = exynos_mailbox_fb_notifier,
};

static int exynos_mailbox_reboot_notifier_call(struct notifier_block *this,
				   unsigned long code, void *_cmd)
{
	/* This condition is lcd off */
#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
	exynos_cl_dvfs_mode_disable();
#endif
	sec_core_lock();
	cl_dvfs_lock();
	apm_notifier_call_chain(APM_SLEEP);
	cl_dvfs_unlock();
	sec_core_unlock();
	pr_info("APM device go to wfi, change to hsi2c \n");

	/* Reset CORTEX M3 */
	exynos_apm_power_down();

	return NOTIFY_DONE;
}

static struct notifier_block exynos_mailbox_reboot_notifier = {
	.notifier_call = exynos_mailbox_reboot_notifier_call,
	.priority = (INT_MIN + 1),
};

unsigned int exynos_cortexm3_pmu_read(unsigned int offset)
{
	unsigned int val;
	int ret;
	ret = regmap_read(cortexm3_pmu_regmap, offset, &val);
	if (ret)
		pr_err("%s, cannot read cortexm3 pmu register!\n", __func__);

	return val;
}

void exynos_cortexm3_pmu_write(unsigned int val, unsigned int offset)
{
	int ret;
	ret = regmap_write(cortexm3_pmu_regmap, offset, val);
	if (ret)
		pr_err("%s cannot write cortexm3 pmu register!\n", __func__);
}

u32 exynos_mailbox_reg_read(unsigned int offset)
{
	return __raw_readl(exynos_mailbox_reg + offset);
}

void exynos_mailbox_reg_write(unsigned int val, unsigned int offset)
{
	__raw_writel(val, exynos_mailbox_reg+  offset);
}

static int samsung_mbox_probe(struct platform_device *pdev)
{
	struct samsung_mbox *samsung_mbox;
	struct device_node *node = pdev->dev.of_node;
	struct samsung_mlink *mbox_link;
	struct mbox_chan **chan;
	int loop, count, ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;
	int asv_table_ver = 0;

	if (!node) {
		dev_err(&pdev->dev, "driver doesnt support"
				"non-dt devices\n");
		return -ENODEV;
	}

	/* read sub link count */
	count = of_property_count_strings(node,
				"samsung,mbox-names");
	if (count <= 0) {
		dev_err(&pdev->dev, "no mbox devices found\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	exynos_mailbox_reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(exynos_mailbox_reg))
	        return PTR_ERR(exynos_mailbox_reg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	sram_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sram_base))
	        return PTR_ERR(sram_base);

	cortexm3_pmu_regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "samsung,syscon-phandle");

	samsung_mbox = devm_kzalloc(&pdev->dev,
			sizeof(struct samsung_mbox), GFP_KERNEL);

	if (IS_ERR(samsung_mbox))
		return PTR_ERR(samsung_mbox);

	chan = devm_kzalloc(&pdev->dev, (count + 1) * sizeof(*chan),
			GFP_KERNEL);
	if (IS_ERR(chan))
		return PTR_ERR(chan);

	/* copy dev information */
	samsung_mbox->dev = &pdev->dev;

	/* Update firmware */
	ret = firmware_update(samsung_mbox->dev);
	if (ret < 0) {
		dev_err(samsung_mbox->dev, "failed update firmware\n");
		return -ENODEV;
	}

	for (loop = 0; loop < count; loop++) {
		mbox_link = &samsung_mbox->samsung_link[loop];

		/* save interrupt information */
		mbox_irq = irq_of_parse_and_map(node, loop);
		if (mbox_irq < 0) {
			dev_err(&pdev->dev, "Failed get irq map \n");
			return -ENODEV;
		}

		mbox_link->smc = samsung_mbox;
		chan[loop] = &mbox_link->chan;
		if (of_property_read_string_index(node, "samsung,mbox-names",
						  loop, &mbox_link->name)) {
			dev_err(&pdev->dev,
				"mbox_name [%d] read failed\n", loop);
			return -ENODEV;
		}

		/* Get of cl dvfs related information to DT */
		asv_table_ver = cal_asv_get_tablever();
		pr_info("[mailbox] asv_table_ver : %d \n", asv_table_ver);
		switch (asv_table_ver) {
		case 0 :
		case 1 :
		case 2 :
		case 3 :
		case 4 :
			ret = of_property_read_u32_index(node, "asv_v0_mngs_margin", 0, &cl_init.atlas_margin);
			if (ret) {
				dev_err(&pdev->dev, "atlas_margin do not set, Set to default 0mV Value\n");
				cl_init.atlas_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v0_apollo_margin", 0, &cl_init.apollo_margin);
			if (ret) {
				dev_err(&pdev->dev, "apollo_margin do not set, Set to default 0mV Value\n");
				cl_init.apollo_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v0_g3d_margin", 0, &cl_init.g3d_margin);
			if (ret) {
				dev_err(&pdev->dev, "g3d_margin do not set, Set to default 0mV Value\n");
				cl_init.g3d_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v0_mif_margin", 0, &cl_init.mif_margin);
			if (ret) {
				dev_err(&pdev->dev, "mif_margin do not set, Set to default 0mV Value\n");
				cl_init.mif_margin = MARGIN_0MV;
			}
			break;
		}

		ret = of_property_read_u32_index(node, "cl_period", 0, &cl_init.period);
		if (ret) {
			dev_err(&pdev->dev, "cl period do not set, Set to default 1ms\n");
			cl_init.period = PERIOD_1MS;
		}

		dev_info(&pdev->dev, "atlas:%d step, apollo:%d step, g3d:%d step, mif:%d step, period:<%d>\n",
			cl_init.atlas_margin, cl_init.apollo_margin, cl_init.g3d_margin,
			cl_init.mif_margin, cl_init.period);

	}

	chan[loop] = NULL; /* Terminating chan */

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
	/* Request interrupt */
	ret = request_irq(mbox_irq, samsung_ipc_handler, IRQF_SHARED, mbox_link->name,
				mbox_link);
	if (ret) {
		dev_err(&pdev->dev, "failed to register mailbox interrupt:%d\n", ret);
		return ret;
	}

	disable_irq(mbox_irq);
#endif

	mutex_init(&samsung_mbox->lock);
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
	samsung_mbox->mbox_con.txdone_irq = true;
#endif
#ifdef CONFIG_EXYNOS_MBOX_POLLING
	samsung_mbox->mbox_con.txdone_irq = false;
	samsung_mbox->mbox_con.txdone_poll = true;
	samsung_mbox->mbox_con.txpoll_period = POLL_PERIOD;
#endif
	samsung_mbox->mbox_con.ops = &samsung_mbox_ops;
	samsung_mbox->mbox_con.num_chans = count;
	samsung_mbox->mbox_con.dev = &pdev->dev;
	samsung_mbox->mbox_con.chans = &mbox_link->chan;

	ret = mbox_controller_register(&samsung_mbox->mbox_con);
	if (ret) {
		dev_err(&pdev->dev, "%s: MBOX channel register failed\n", __func__);
		return ret;
	}

	platform_set_drvdata(pdev, samsung_mbox);

	register_reboot_notifier(&exynos_mailbox_reboot_notifier);

	mailbox_wq = create_singlethread_workqueue("thred-mailbox");
	if (!mailbox_wq) {
		return -ENOMEM;
	}

	fb_register_client(&exynos_mailbox_fb_notifier_block);

	idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev));
	exynos_update_ip_idle_status(idle_ip_index, 1);

	/* Write rcc table to apm sram area */
	cal_asv_set_rcc_table();
	cal_rcc_print_info();

#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
	cl_init.cl_status = CL_ON;
#endif
	return ret;
}

static int samsung_mbox_remove(struct platform_device *pdev)
{
	struct samsung_mbox *samsung_mbox = platform_get_drvdata(pdev);
	struct samsung_mlink *mbox_link;

	mbox_link = &samsung_mbox->samsung_link[0];

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
	free_irq(mbox_irq, mbox_link);
#endif

	mbox_controller_unregister(&samsung_mbox->mbox_con);
	unregister_reboot_notifier(&exynos_mailbox_reboot_notifier);
	fb_unregister_client(&exynos_mailbox_fb_notifier_block);

	return 0;
}

static int samsung_mailbox_pm_resume_early(struct device *dev)
{
	int ret;

	ret = firmware_update(dev);
	if (ret < 0) {
		dev_err(dev, "failed update firmware\n");
		return -ENODEV;
	}

	/* Write rcc table to apm sram area */
	cal_asv_set_rcc_table();

	return 0;
}

static struct dev_pm_ops samsung_mailbox_pm = {
	.resume_early	= samsung_mailbox_pm_resume_early,
};

/* Debug FS */
static int mailbox_message_open_show(struct seq_file *buf, void *d)
{
	int count;

	/* Print tx message history */
	for (count = 0; count < DEBUG_COUNT; count++) {
		seq_printf(buf, "[TX] [%lld ms]data : %8.x %8.x %8.x %8.x %8.x %s\n", tx.time[count], tx.buf[count][0],
				tx.buf[count][1], tx.buf[count][2], tx.buf[count][3], tx.buf[count][4], tx.name[count]);
		seq_printf(buf, "[RX] [%lld ms]data : %8.x %8.x %8.x %8.x %8.x %s\n", rx.time[count], rx.buf[count][0],
				rx.buf[count][1], rx.buf[count][2], rx.buf[count][3], rx.buf[count][4], rx.name[count]);
	}

	return 0;
}

static int mailbox_send_data_open_show(struct seq_file *buf, void *d)
{
	int count;

	/* Print tx message history */
	for (count = 0; count < DEBUG_COUNT; count++) {
		seq_printf(buf, "[%lld ms]data : %8.x %8.x %8.x %8.x %8.x %s\n", tx.time[count], tx.buf[count][0],
				tx.buf[count][1], tx.buf[count][2], tx.buf[count][3], tx.buf[count][4], tx.name[count]);
	}

	return 0;
}

static int mailbox_receive_data_open_show(struct seq_file *buf, void *d)
{
	int count;

	/* Print tx message history */
	for (count = 0; count < DEBUG_COUNT; count++) {
		seq_printf(buf, "[%lld ms]data : %8.x %8.x %8.x %8.x %8.x %s\n", rx.time[count], rx.buf[count][0],
				rx.buf[count][1], rx.buf[count][2], rx.buf[count][3], rx.buf[count][4], rx.name[count]);
	}

	return 0;
}

static int hpm_read_open_show(struct seq_file *buf, void *d)
{
	unsigned int tmp = 0;
	unsigned int cur_hpm[4] = {0, 0, 0, 0};
	unsigned int tar_hpm[4] = {0, 0, 0, 0};

	tmp = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG3);
	cur_hpm[0] = tmp & 0xFF;
	cur_hpm[1] = (tmp >> 8) & 0xFF;
	cur_hpm[2] = (tmp >> 16) & 0xFF;
	cur_hpm[3] = (tmp >> 24) & 0xFF;

	tmp = __raw_readl(exynos_mailbox_reg + EXYNOS_MAILBOX_RX_MSG7);
	tar_hpm[0] = tmp & 0xFF;
	tar_hpm[1] = (tmp >> 8) & 0xFF;
	tar_hpm[2] = (tmp >> 16) & 0xFF;
	tar_hpm[3] = (tmp >> 24) & 0xFF;

	seq_printf(buf,"mngs[%d][%d], apo[%d][%d], gpu[%d][%d], mif[%d][%d]\n",
			cur_hpm[3], tar_hpm[3], cur_hpm[2], tar_hpm[2],
			cur_hpm[1], tar_hpm[1], cur_hpm[0], tar_hpm[0]);

	return 0;
}

static int cm3_margin_open_show(struct seq_file *buf, void *d)
{
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
	seq_printf(buf, "ATLAS  Limit margin : %d uV \n", cl_init.atlas_margin * PMIC_STEP);
	seq_printf(buf, "APOLLO Limit margin : %d uV \n", cl_init.apollo_margin * PMIC_STEP);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	seq_printf(buf, "G3D    Limit margin : %d uV \n", cl_init.g3d_margin * PMIC_STEP);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	seq_printf(buf, "MIF    Limit margin : %d uV \n", cl_init.mif_margin * PMIC_STEP);
#endif

	return 0;
}

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
static int cl_voltage_open_show(struct seq_file *buf, void *d)
{
	seq_printf(buf, "[Voltage rail][input uV][cl_volt uV]\n");
	seq_printf(buf, "MNGS[L%d] : %d %d APO[L%d] : %d %d GPU[L%d] : %d %d MIF[L%d] : %d %d \n",
		mngs_lv, atl_in_voltage, atl_voltage, apo_lv, apo_in_voltage, apo_voltage,
		gpu_lv, g3d_in_voltage, g3d_voltage, mif_lv, mif_in_voltage, mif_voltage);

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
static int cpu_margin_get(void *data, u64 *val)
{
	pr_info("ATLAS  Limit margin : %d uV \n", cl_init.atlas_margin * PMIC_STEP);
	pr_info("APOLLO Limit margin : %d uV \n", cl_init.apollo_margin * PMIC_STEP);

	return 0;
}

static int cpu_margin_set(void *data, u64 val)
{
	int ret;

	cl_init.atlas_margin = val;
	cl_init.apollo_margin = val;

	ret = exynos_cl_dvfs_setup(cl_init.atlas_margin, cl_init.apollo_margin,
					cl_init.g3d_margin, cl_init.mif_margin, cl_init.period);
	if (ret)
		pr_warn("mailbox : Do not set margin and period information\n");

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
static int g3d_margin_get(void *data, u64 *val)
{
	pr_info("G3D    Limit margin : %d uV \n", cl_init.g3d_margin * PMIC_STEP);

	return 0;
}

static int g3d_margin_set(void *data, u64 val)
{
	int ret;

	cl_init.g3d_margin = val;

	ret = exynos_cl_dvfs_setup(cl_init.atlas_margin, cl_init.apollo_margin,
					cl_init.g3d_margin, cl_init.mif_margin, cl_init.period);
	if (ret)
		pr_warn("mailbox : Do not set margin and period information\n");

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
static int mif_margin_get(void *data, u64 *val)
{
	pr_info("MIF    Limit margin : %d uV \n", cl_init.mif_margin * PMIC_STEP);

	return 0;
}

static int mif_margin_set(void *data, u64 val)
{
	int ret;

	cl_init.mif_margin = val;

	ret = exynos_cl_dvfs_setup(cl_init.atlas_margin, cl_init.apollo_margin,
					cl_init.g3d_margin, cl_init.mif_margin, cl_init.period);
	if (ret)
		pr_warn("mailbox : Do not set margin and period information\n");

	return 0;
}
#endif

#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
static int cl_enable_get(void *data, u64 *val)
{
	if (cl_init.cl_status == CL_OFF && cl_init.apm_status == APM_ON)
		pr_info("CL STATUS : Disable\n");
	else if (cl_init.cl_status == CL_ON && cl_init.apm_status == APM_ON)
		pr_info("CL STATUS : Enable\n");
	else if (cl_init.apm_status == APM_OFF)
		pr_info("CL STATUS : APM Power Off\n");

	return 0;
}

static int cl_enable_set(void *data, u64 val)
{
	if (val) {
		if (cl_init.apm_status == APM_ON) {
			if (cl_init.cl_status == CL_OFF) {
				exynos_cl_dvfs_mode_enable();

				sec_core_lock();
				cl_dvfs_lock();
				apm_notifier_call_chain(CL_ENABLE);
				cl_dvfs_unlock();
				sec_core_unlock();

				cl_init.cl_status = CL_ON;
				pr_info("CL_ENABLE \n");
			} else if (cl_init.cl_status == CL_ON) {
				pr_info("Already turn on CL-DVFS\n");
			}
		} else if (cl_init.apm_status == APM_OFF) {
			pr_info("APM OFF Status \n");
		}
	} else {
		if (cl_init.apm_status == APM_ON) {
			if (cl_init.cl_status == CL_OFF) {
				pr_info("Already turn off CL-DVFS\n");
			} else if (cl_init.cl_status == CL_ON) {
				exynos_cl_dvfs_mode_disable();

				sec_core_lock();
				cl_dvfs_lock();
				apm_notifier_call_chain(CL_DISABLE);
				cl_dvfs_unlock();
				sec_core_unlock();

				cl_init.cl_status = CL_OFF;
				pr_info("CL_DISABLE \n");
			}
		} else if (cl_init.apm_status == APM_OFF) {
			pr_info("APM OFF Status \n");
		}
	}

	return 0;
}
#endif
static int mailbox_message_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, mailbox_message_open_show, inode->i_private);
}

static int mailbox_send_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, mailbox_send_data_open_show, inode->i_private);
}

static int mailbox_receive_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, mailbox_receive_data_open_show, inode->i_private);
}

static int cm3_margin_open(struct inode *inode, struct file *file)
{
	return single_open(file, cm3_margin_open_show, inode->i_private);
}

static int hpm_read_open(struct inode *inode, struct file *file)
{
	return single_open(file, hpm_read_open_show, inode->i_private);
}

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
static int cl_voltage_open(struct inode *inode, struct file *file)
{
	return single_open(file, cl_voltage_open_show, inode->i_private);
}
#endif

static const struct file_operations message_status_fops = {
	.open		= mailbox_message_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations send_status_fops = {
	.open		= mailbox_send_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations receive_status_fops = {
	.open		= mailbox_receive_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations mode_status_fops = {
	.open		= cm3_status_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations mode_margin_fops = {
	.open		= cm3_margin_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations hpm_fops = {
	.open		= hpm_read_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
static const struct file_operations cl_voltage_fops = {
	.open		= cl_voltage_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
DEFINE_SIMPLE_ATTRIBUTE(cpu_margin_fops, cpu_margin_get, cpu_margin_set, "%llx\n");
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
DEFINE_SIMPLE_ATTRIBUTE(g3d_margin_fops, g3d_margin_get, g3d_margin_set, "%llx\n");
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
DEFINE_SIMPLE_ATTRIBUTE(mif_margin_fops, mif_margin_get, mif_margin_set, "%llx\n");
#endif
#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
DEFINE_SIMPLE_ATTRIBUTE(cl_enable_fops, cl_enable_get, cl_enable_set, "%llx\n");
#endif

void mailbox_debugfs(void)
{
	struct dentry *den;

	den = debugfs_create_dir("mailbox", NULL);
	debugfs_create_file("message", 0644, den, NULL, &message_status_fops);
	debugfs_create_file("send_data", 0644, den, NULL, &send_status_fops);
	debugfs_create_file("receive_data", 0644, den, NULL, &receive_status_fops);
	debugfs_create_file("mode", 0644, den, NULL, &mode_status_fops);
	debugfs_create_file("cl_dvs_margin", 0644, den, NULL, &mode_margin_fops);
	debugfs_create_file("hpm", 0644, den, NULL, &hpm_fops);
#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	debugfs_create_file("cl_voltage", 0644, den, NULL, &cl_voltage_fops);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
	debugfs_create_file("cpu_cl_margin", 0644, den, NULL, &cpu_margin_fops);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	debugfs_create_file("g3d_cl_margin", 0644, den, NULL, &g3d_margin_fops);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	debugfs_create_file("mif_cl_margin", 0644, den, NULL, &mif_margin_fops);
#endif
#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
	debugfs_create_file("cl_control", 0644, den, NULL, &cl_enable_fops);
#endif
}

static const struct of_device_id mailbox_smc_match[] = {
	{ .compatible = "samsung,exynos-mailbox" },
	{},
};

static struct platform_driver samsung_mbox_driver = {
	.probe	= samsung_mbox_probe,
	.remove	= samsung_mbox_remove,
	.driver	= {
		.name = "exynos-mailbox",
		.owner	= THIS_MODULE,
		.of_match_table	= mailbox_smc_match,
		.pm	= &samsung_mailbox_pm,
	},
};

static int __init exynos_mailbox_init(void)
{
	mailbox_debugfs();

	return platform_driver_register(&samsung_mbox_driver);
}
fs_initcall(exynos_mailbox_init);

static void __exit exynos_mailbox_exit(void)
{
	class_destroy(mailbox_class);
}
module_exit(exynos_mailbox_exit);
