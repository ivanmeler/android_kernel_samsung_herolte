/**
 * i2c-exynos5.c - Samsung Exynos5 I2C Controller Driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include "../../pinctrl/core.h"
#include <soc/samsung/exynos-powermode.h>
#include <linux/smc.h>

#ifdef CONFIG_CPU_IDLE
#include <soc/samsung/exynos-pm.h>
#endif
#ifdef CONFIG_EXYNOS_APM
#include <linux/apm-exynos.h>
#endif

#include "i2c-exynos5.h"

#if defined(CONFIG_CPU_IDLE) || \
	defined(CONFIG_EXYNOS_APM)
static LIST_HEAD(drvdata_list);
#endif

/*
 * HSI2C controller from Samsung supports 2 modes of operation
 * 1. Auto mode: Where in master automatically controls the whole transaction
 * 2. Manual mode: Software controls the transaction by issuing commands
 *    START, READ, WRITE, STOP, RESTART in I2C_MANUAL_CMD register.
 *
 * Operation mode can be selected by setting AUTO_MODE bit in I2C_CONF register
 *
 * Special bits are available for both modes of operation to set commands
 * and for checking transfer status
 */

/* Register Map */
#define HSI2C_CTL		0x00
#define HSI2C_FIFO_CTL		0x04
#define HSI2C_TRAILIG_CTL	0x08
#define HSI2C_CLK_CTL		0x0C
#define HSI2C_CLK_SLOT		0x10
#define HSI2C_INT_ENABLE	0x20
#define HSI2C_INT_STATUS	0x24
#define HSI2C_ERR_STATUS	0x2C
#define HSI2C_FIFO_STATUS	0x30
#define HSI2C_TX_DATA		0x34
#define HSI2C_RX_DATA		0x38
#define HSI2C_CONF		0x40
#define HSI2C_AUTO_CONF		0x44
#define HSI2C_TIMEOUT		0x48
#define HSI2C_MANUAL_CMD	0x4C
#define HSI2C_TRANS_STATUS	0x50
#define HSI2C_TIMING_HS1	0x54
#define HSI2C_TIMING_HS2	0x58
#define HSI2C_TIMING_HS3	0x5C
#define HSI2C_TIMING_FS1	0x60
#define HSI2C_TIMING_FS2	0x64
#define HSI2C_TIMING_FS3	0x68
#define HSI2C_TIMING_SLA	0x6C
#define HSI2C_ADDR		0x70

#define HSI2C_SMRelease		0x200

/* HSI2C Batcher Register Map */
#define HSI2C_BATCHER_CON		0x500
#define HSI2C_BATCHER_STATE		0x504
#define HSI2C_BATCHER_INT_EN		0x508
#define HSI2C_BATCHER_FIFO_STATUS	0x50C
#define HSI2C_BATCHER_INT_STATUS	0x510
#define HSI2C_BATCHER_OPCODE		0x600

#define HSI2C_START_PAYLOAD		0x1000
#define HSI2C_END_PAYLOAD		0x1060

/* I2C_CTL Register bits */
#define HSI2C_FUNC_MODE_I2C			(1u << 0)
#define HSI2C_CS_ENB				(1u << 0)
#define HSI2C_MASTER				(1u << 3)
#define HSI2C_RXCHON				(1u << 6)
#define HSI2C_TXCHON				(1u << 7)
#define HSI2C_EXT_MSB				(1u << 29)
#define HSI2C_EXT_ADDR				(1u << 30)
#define HSI2C_SW_RST				(1u << 31)

/* I2C_FIFO_CTL Register bits */
#define HSI2C_RXFIFO_EN				(1u << 0)
#define HSI2C_TXFIFO_EN				(1u << 1)
#define HSI2C_FIFO_MAX				(0x40)
#define HSI2C_RXFIFO_TRIGGER_LEVEL		(0x8 << 4)
#define HSI2C_TXFIFO_TRIGGER_LEVEL		(0x8 << 16)
/* I2C_TRAILING_CTL Register bits */
#define HSI2C_TRAILING_COUNT			(0xf)
#define BATCHER_TRAILING_COUNT			(0x2ff)

/* I2C_INT_EN Register bits */
#define HSI2C_INT_TX_ALMOSTEMPTY_EN		(1u << 0)
#define HSI2C_INT_RX_ALMOSTFULL_EN		(1u << 1)
#define HSI2C_INT_TRAILING_EN			(1u << 6)
#define HSI2C_INT_TRANSFER_DONE			(1u << 7)
#define HSI2C_INT_I2C_EN			(1u << 9)
#define HSI2C_INT_CHK_TRANS_STATE		(0xf << 8)

/* I2C_INT_STAT Register bits */
#define HSI2C_INT_TX_ALMOSTEMPTY		(1u << 0)
#define HSI2C_INT_RX_ALMOSTFULL			(1u << 1)
#define HSI2C_INT_TX_UNDERRUN			(1u << 2)
#define HSI2C_INT_TX_OVERRUN			(1u << 3)
#define HSI2C_INT_RX_UNDERRUN			(1u << 4)
#define HSI2C_INT_RX_OVERRUN			(1u << 5)
#define HSI2C_INT_TRAILING			(1u << 6)
#define HSI2C_INT_I2C				(1u << 9)
#define HSI2C_RX_INT				(HSI2C_INT_RX_ALMOSTFULL | \
						 HSI2C_INT_RX_UNDERRUN | \
						 HSI2C_INT_RX_OVERRUN | \
						 HSI2C_INT_TRAILING)

/* I2C_FIFO_STAT Register bits */
#define HSI2C_RX_FIFO_EMPTY			(1u << 24)
#define HSI2C_RX_FIFO_FULL			(1u << 23)
#define HSI2C_RX_FIFO_LVL(x)			((x >> 16) & 0x7f)
#define HSI2C_RX_FIFO_LVL_MASK			(0x7F << 16)
#define HSI2C_TX_FIFO_EMPTY			(1u << 8)
#define HSI2C_TX_FIFO_FULL			(1u << 7)
#define HSI2C_TX_FIFO_LVL(x)			((x >> 0) & 0x7f)
#define HSI2C_TX_FIFO_LVL_MASK			(0x7F << 0)
#define HSI2C_FIFO_EMPTY			(HSI2C_RX_FIFO_EMPTY |	\
						HSI2C_TX_FIFO_EMPTY)

/* I2C_CONF Register bits */
#define HSI2C_AUTO_MODE				(1u << 31)
#define HSI2C_10BIT_ADDR_MODE			(1u << 30)
#define HSI2C_HS_MODE				(1u << 29)
#define HSI2C_FILTER_EN_SCL			(1u << 28)
#define HSI2C_FILTER_EN_SDA			(1u << 27)
#define HSI2C_FTL_CYCLE_SCL_MASK		(0x7 << 16)
#define HSI2C_FTL_CYCLE_SDA_MASK		(0x7 << 13)

/* I2C_AUTO_CONF Register bits */
#define HSI2C_READ_WRITE			(1u << 16)
#define HSI2C_STOP_AFTER_TRANS			(1u << 17)
#define HSI2C_MASTER_RUN			(1u << 31)

/* I2C_TIMEOUT Register bits */
#define HSI2C_TIMEOUT_EN			(1u << 31)

/* I2C_TRANS_STATUS register bits */
#define HSI2C_MASTER_BUSY			(1u << 17)
#define HSI2C_SLAVE_BUSY			(1u << 16)
#define HSI2C_TIMEOUT_AUTO			(1u << 4)
#define HSI2C_NO_DEV				(1u << 3)
#define HSI2C_NO_DEV_ACK			(1u << 2)
#define HSI2C_TRANS_ABORT			(1u << 1)
#define HSI2C_TRANS_DONE			(1u << 0)
#define HSI2C_MAST_ST_MASK			(0xf << 0)

/* I2C_ADDR register bits */
#define HSI2C_SLV_ADDR_SLV(x)			((x & 0x3ff) << 0)
#define HSI2C_SLV_ADDR_MAS(x)			((x & 0x3ff) << 10)
#define HSI2C_MASTER_ID(x)			((x & 0xff) << 24)
#define MASTER_ID(x)				((x & 0x7) + 0x08)

/* HSI2CBatcer.CON register bits */
#define HSI2C_BATCHER_RESET			(1u << 31)
#define HSI2C_BATCHER_HSI2C_RST			(1u << 6)
#define HSI2C_BATCHER_APB_RSP			(1u << 5)
#define HSI2C_BATCHER_START			(1u << 4)
#define HSI2C_BATCHER_DISABLE_SEMA_REL		(1u << 2)
#define HSI2C_BATCHER_DEDICATE			(1u << 1)
#define HSI2C_BATCHER_ENABLE			(1u << 0)

/* Batcher STATE register bits */
#define UNEXPECTED_HSI2C_INTR			(1u << 1)
#define BATCHER_OPERATION_COMPLETE		(1u << 0)
#define BATCHER_IDLE_STATE			(1u << 3)
#define BATCHER_INIT_STATE			(1u << 4)
#define BATCHER_GET_SEMAPHORE_STATE		(1u << 5)
#define BATCHER_CONFIG_STATE			(1u << 6)
#define BATCHER_CLEAN_INTR_STATE		(1u << 13)
#define BATCHER_REL_SEMAPHORE_STATE		(1u << 14)
#define BATCHER_GEN_INT_STATE			(1u << 15)

/* Batcher INT_EN */
#define HSI2C_BATCHER_INT_ENABLE		(1u << 0)

/*
 * Controller operating frequency, timing values for operation
 * are calculated against this frequency
 */
#define HSI2C_HS_TX_CLOCK	2500000
#define HSI2C_FS_TX_CLOCK	400000
#define HSI2C_HIGH_SPD		1
#define HSI2C_FAST_SPD		0

#define HSI2C_POLLING 0
#define HSI2C_INTERRUPT 1

#define EXYNOS5_I2C_TIMEOUT (msecs_to_jiffies(1000))
#define EXYNOS5_BATCHER_TIMEOUT (msecs_to_jiffies(500))

#define EXYNOS5_FIFO_SIZE		16

#define EXYNOS5_HSI2C_RUNTIME_PM_DELAY	(100)

#define HSI2C_BATCHER_INIT_CMD	0xFFFFFFFF


static const struct of_device_id exynos5_i2c_match[] = {
	{ .compatible = "samsung,exynos5-hsi2c" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_i2c_match);


#ifdef CONFIG_EXYNOS_APM
static struct pinctrl *apm_i2c_pinctrl;
static struct pinctrl_state *default_i2c_gpio, *apm_i2c_gpio;

static int exynos_regulator_apm_notifier(struct notifier_block *notifier,
						unsigned long pm_event, void *v)
{
	int status;
	struct exynos5_i2c *i2c;

	list_for_each_entry(i2c, &drvdata_list, node)
		if (i2c->use_apm_mode == 1)
			break;

	switch (pm_event) {
		case APM_READY:
			if (!IS_ERR_OR_NULL(apm_i2c_gpio)) {
				status = pinctrl_select_state(apm_i2c_pinctrl, apm_i2c_gpio);
				if (status) {
					printk(KERN_ERR "[APM I2C] Can't set APM GPIO\n");
					return NOTIFY_BAD;
				}
			}
			break;
		case APM_SLEEP:
		case APM_TIMEOUT:
			if (!IS_ERR_OR_NULL(default_i2c_gpio)) {
				status = pinctrl_select_state(apm_i2c_pinctrl,
							default_i2c_gpio);
				if (status) {
					printk(KERN_ERR "[APM I2C] Can't set default I2C gpio.\n");
					return NOTIFY_BAD;
				}
			}

			if (i2c->support_hsi2c_batcher) {
				if (readl(i2c->regs + HSI2C_BATCHER_STATE) &
						BATCHER_OPERATION_COMPLETE) {
					writel(BATCHER_OPERATION_COMPLETE,
							i2c->regs + HSI2C_BATCHER_STATE);
				}
			}

			i2c->need_hw_init = 1;
			break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_apm_notifier = {
	.notifier_call = exynos_regulator_apm_notifier,
};
#endif

#ifdef CONFIG_GPIOLIB
static void change_i2c_gpio(struct exynos5_i2c *i2c)
{
	struct pinctrl_state *default_i2c_pins;
	struct pinctrl *default_i2c_pinctrl;
	int status = 0;

	default_i2c_pinctrl = devm_pinctrl_get(i2c->dev);
	if (IS_ERR(default_i2c_pinctrl)) {
		dev_err(i2c->dev, "Can't get i2c pinctrl!!!\n");
		return ;
	}

	default_i2c_pins = pinctrl_lookup_state(default_i2c_pinctrl,
				"default");
	if (!IS_ERR(default_i2c_pins)) {
		default_i2c_pinctrl->state = NULL;
		status = pinctrl_select_state(default_i2c_pinctrl, default_i2c_pins);
		if (status)
			dev_err(i2c->dev, "Can't set default i2c pins!!!\n");
	} else {
		dev_err(i2c->dev, "Can't get default pinstate!!!\n");
	}
}

static void recover_gpio_pins_secure(struct exynos5_i2c *i2c)
{
	dev_err(i2c->dev, "Recover GPIO pins in secure\n");

	if (i2c->bus_id == exynos_smc((0x83000045), i2c->bus_id, 0, 0))
		dev_err(i2c->dev, "SDA line(%d) is recovered in secure!!!\n", i2c->bus_id);
	else
		dev_err(i2c->dev, "SDA line(%d) is not recovered in secure!!!\n", i2c->bus_id);
}

static void recover_gpio_pins(struct exynos5_i2c *i2c)
{
	int gpio_sda, gpio_scl;
	int sda_val, scl_val, clk_cnt;
	unsigned long timeout;
	struct device_node *np = i2c->adap.dev.of_node;

	dev_err(i2c->dev, "Recover GPIO pins\n");

	gpio_sda = of_get_named_gpio(np, "gpio_sda", 0);
	if (!gpio_is_valid(gpio_sda)) {
		dev_err(i2c->dev, "Can't get gpio_sda!!!\n");
		return ;
	}
	gpio_scl = of_get_named_gpio(np, "gpio_scl", 0);
	if (!gpio_is_valid(gpio_scl)) {
		dev_err(i2c->dev, "Can't get gpio_scl!!!\n");
		return ;
	}

	sda_val = gpio_get_value(gpio_sda);
	scl_val = gpio_get_value(gpio_scl);

	dev_err(i2c->dev, "SDA line : %s, SCL line : %s\n",
			sda_val ? "HIGH" : "LOW", scl_val ? "HIGH" : "LOW");

	if (sda_val == 1)
		return ;

	/* Wait for SCL as high for 500msec */
	if (scl_val == 0) {
		timeout = jiffies + msecs_to_jiffies(500);
		while (time_before(jiffies, timeout)) {
			if (gpio_get_value(gpio_scl) != 0) {
				timeout = 0;
				break;
			}
			msleep(10);
		}
		if (timeout)
			dev_err(i2c->dev, "SCL line is still LOW!!!\n");
	}

	sda_val = gpio_get_value(gpio_sda);

	if (sda_val == 0) {
		gpio_direction_output(gpio_scl, 1);
		gpio_direction_input(gpio_sda);

		for (clk_cnt = 0; clk_cnt < 100; clk_cnt++) {
			/* Make clock for slave */
			gpio_set_value(gpio_scl, 0);
			udelay(5);
			gpio_set_value(gpio_scl, 1);
			udelay(5);
			if (gpio_get_value(gpio_sda) == 1) {
				dev_err(i2c->dev, "SDA line is recovered.\n");
				break;
			}
		}
		if (clk_cnt == 100)
			dev_err(i2c->dev, "SDA line is not recovered!!!\n");
	}

	/* Change I2C GPIO as default function */
	change_i2c_gpio(i2c);
}
#endif

static inline void dump_i2c_register(struct exynos5_i2c *i2c)
{
#ifdef CONFIG_EXYNOS_APM
	int buf_index;
#endif

	dev_err(i2c->dev, "Register dump(suspended : %d)\n", i2c->suspended);
	dev_err(i2c->dev, ": CTL	0x%08x\n"
			, readl(i2c->regs + HSI2C_CTL));
	dev_err(i2c->dev, ": FIFO_CTL	0x%08x\n"
			, readl(i2c->regs + HSI2C_FIFO_CTL));
	dev_err(i2c->dev, ": INT_EN	0x%08x\n"
			, readl(i2c->regs + HSI2C_INT_ENABLE));
	dev_err(i2c->dev, ": INT_STAT	0x%08x\n"
			, readl(i2c->regs + HSI2C_INT_STATUS));
	dev_err(i2c->dev, ": FIFO_STAT	0x%08x\n"
			, readl(i2c->regs + HSI2C_FIFO_STATUS));
	dev_err(i2c->dev, ": CONF	0x%08x\n"
			, readl(i2c->regs + HSI2C_CONF));
	dev_err(i2c->dev, ": AUTO_CONF	0x%08x\n"
			, readl(i2c->regs + HSI2C_AUTO_CONF));
	dev_err(i2c->dev, ": TRANS_STAT	0x%08x\n"
			, readl(i2c->regs + HSI2C_TRANS_STATUS));
	dev_err(i2c->dev, ": TIMING_HS1	0x%08x\n"
			, readl(i2c->regs + HSI2C_TIMING_HS1));
	dev_err(i2c->dev, ": TIMING_HS2	0x%08x\n"
			, readl(i2c->regs + HSI2C_TIMING_HS2));
	dev_err(i2c->dev, ": TIMING_HS3	0x%08x\n"
			, readl(i2c->regs + HSI2C_TIMING_HS3));
	dev_err(i2c->dev, ": TIMING_FS1	0x%08x\n"
			, readl(i2c->regs + HSI2C_TIMING_FS1));
	dev_err(i2c->dev, ": TIMING_FS2	0x%08x\n"
			, readl(i2c->regs + HSI2C_TIMING_FS2));
	dev_err(i2c->dev, ": TIMING_FS3	0x%08x\n"
			, readl(i2c->regs + HSI2C_TIMING_FS3));
	dev_err(i2c->dev, ": TIMING_SLA	0x%08x\n"
			, readl(i2c->regs + HSI2C_TIMING_SLA));
	dev_err(i2c->dev, ": ADDR	0x%08x\n"
			, readl(i2c->regs + HSI2C_ADDR));

#ifdef CONFIG_EXYNOS_APM
	if (i2c->use_apm_mode && i2c->msg != NULL) {
		dev_err(i2c->dev, "Print PMIC CMD\n");
		for (buf_index = 0; buf_index < i2c->msg->len; buf_index++) {
			pr_err("[%d] 0x%x\n", buf_index, i2c->msg->buf[buf_index]);
		}
	}
#endif

#ifdef CONFIG_GPIOLIB
	if (i2c->secure_mode) /* this is for secure gpio port recovery (Grace Secure Camera only) */
		recover_gpio_pins_secure(i2c);
	else
		recover_gpio_pins(i2c);
#endif
}

static void write_batcher(struct exynos5_i2c *i2c, unsigned int description,
			unsigned int opcode)
{

	if ((HSI2C_START_PAYLOAD + (i2c->desc_pointer * 4)) <=
		HSI2C_END_PAYLOAD) {

		/* clear cmd_buffer */
		i2c->cmd_buffer &= ~(0xff << (8 * i2c->cmd_index));

		/* write opcode to cmd_buffer */
		i2c->cmd_buffer |= opcode << (8 * i2c->cmd_index);

		writel(i2c->cmd_buffer, i2c->regs + HSI2C_BATCHER_OPCODE +
			(i2c->cmd_pointer * 4));

		writel(description, i2c->regs + HSI2C_START_PAYLOAD +
			(i2c->desc_pointer++ * 4));
	} else {
		/* Error Handling Routine */
		dev_warn(i2c->dev, "fail to write hsi2c batcher\n");
	}

	/* Update cmd_index */
	if (++i2c->cmd_index == 4) {
		i2c->cmd_index = 0;
		i2c->cmd_pointer++;
		i2c->cmd_buffer = HSI2C_BATCHER_INIT_CMD;
	}
}

static void exynos5_i2c_clr_pend_irq(struct exynos5_i2c *i2c)
{
	writel(readl(i2c->regs + HSI2C_INT_STATUS),
				i2c->regs + HSI2C_INT_STATUS);
}

/*
 * exynos5_i2c_set_timing: updates the registers with appropriate
 * timing values calculated
 *
 * Returns 0 on success, -EINVAL if the cycle length cannot
 * be calculated.
 */
static int exynos5_i2c_set_timing(struct exynos5_i2c *i2c, int mode)
{
	u32 i2c_timing_s1;
	u32 i2c_timing_s2;
	u32 i2c_timing_s3;
	u32 i2c_timing_sla;
	unsigned int t_start_su, t_start_hd;
	unsigned int t_stop_su;
	unsigned int t_sda_su;
	unsigned int t_data_su, t_data_hd;
	unsigned int t_scl_l, t_scl_h;
	unsigned int t_sr_release;
	unsigned int t_ftl_cycle;
	unsigned int clkin = clk_get_rate(i2c->rate_clk);
	unsigned int div, utemp0 = 0, utemp1 = 0, clk_cycle = 0;
	unsigned int op_clk = (mode == HSI2C_HIGH_SPD) ?
				i2c->hs_clock : i2c->fs_clock;

	if (i2c->use_old_timing_values == 0) {
		/*
		 * FPCLK / FI2C = (CLK_DIV + 1) * (TSCLK_L + TSCLK_H + 2) +
		 * {(FLT_CYCLE + 3) - (FLT_CYCLE + 3) % (CLK_DIV + 1)} * 2
		 */
		t_ftl_cycle = (readl(i2c->regs + HSI2C_CONF) >> 16) & 0x7;

		/*
		 * utemp0 = (FPCLK / FI2C) - (FLT_CYCLE + 3) * 2
		 */
		utemp0 = (clkin / op_clk) - (t_ftl_cycle + 3) * 2;

		/* CLK_DIV max is 256 */
		for (div = 0; div < 256; div++) {
			/*
			 * utemp1 = (TSCLK_L + TSCLK_H + 2)
			 */
			utemp1 = (utemp0 + ((t_ftl_cycle + 3) % (div + 1)) * 2) / (div + 1);

			if ((utemp1 < 512) && (utemp1 > 4)) {
				clk_cycle = utemp1 - 2;
				break;
			} else if (div == 255) {
				dev_warn(i2c->dev, "Failed to calculate divisor");
				return -EINVAL;
			}
		}
	} else {
		/*
		 * FPCLK / FI2C =
		 * (CLK_DIV + 1) * (TSCLK_L + TSCLK_H + 2) + 8 + 2 * FLT_CYCLE
		 * utemp0 = (CLK_DIV + 1) * (TSCLK_L + TSCLK_H + 2)
		 * utemp1 = (TSCLK_L + TSCLK_H + 2)
		 */
		t_ftl_cycle = (readl(i2c->regs + HSI2C_CONF) >> 16) & 0x7;
		utemp0 = (clkin / op_clk) - 8 - t_ftl_cycle;

		/* CLK_DIV max is 256 */
		for (div = 0; div < 256; div++) {
			utemp1 = utemp0 / (div + 1);

			/*
			 * SCL_L and SCL_H each has max value of 255
			 * Hence, For the clk_cycle to the have right value
			 * utemp1 has to be less then 512 and more than 4.
			 */
			if ((utemp1 < 512) && (utemp1 > 4)) {
				clk_cycle = utemp1 - 2;
				break;
			} else if (div == 255) {
				dev_warn(i2c->dev, "Failed to calculate divisor");
				return -EINVAL;
			}
		}
	}

	if (mode == HSI2C_HIGH_SPD)
		t_scl_h = ((clk_cycle + 10) / 3) - 5;
	else
		t_scl_h = clk_cycle / 2;

	t_scl_l = clk_cycle - t_scl_h;

	t_start_su = t_scl_l;
	t_start_hd = t_scl_l;
	t_stop_su = t_scl_l;
	t_sda_su = t_scl_l;
	t_data_su = t_scl_l / 2;
	t_data_hd = t_scl_l / 2;
	t_sr_release = clk_cycle;

	if (mode == HSI2C_HIGH_SPD)
		i2c_timing_s1 = t_start_su << 24 | t_start_hd << 16 | t_stop_su << 8 | t_sda_su;
	else
		i2c_timing_s1 = t_start_su << 24 | t_start_hd << 16 | t_stop_su << 8;

	i2c_timing_s2 = (0xF << 16) | t_data_su << 24 | t_scl_l << 8 | t_scl_h;
	i2c_timing_s3 = (div << 16) | t_sr_release;
	i2c_timing_sla = t_data_hd;

	dev_dbg(i2c->dev, "tSTART_SU: %X, tSTART_HD: %X, tSTOP_SU: %X\n",
		t_start_su, t_start_hd, t_stop_su);
	dev_dbg(i2c->dev, "tDATA_SU: %X, tSCL_L: %X, tSCL_H: %X\n",
		t_data_su, t_scl_l, t_scl_h);
	dev_dbg(i2c->dev, "nClkDiv: %X, tSR_RELEASE: %X\n",
		div, t_sr_release);
	dev_dbg(i2c->dev, "tDATA_HD: %X\n", t_data_hd);

	if (mode == HSI2C_HIGH_SPD) {
		writel(i2c_timing_s1, i2c->regs + HSI2C_TIMING_HS1);
		writel(i2c_timing_s2, i2c->regs + HSI2C_TIMING_HS2);
		writel(i2c_timing_s3, i2c->regs + HSI2C_TIMING_HS3);
	} else {
		writel(i2c_timing_s1, i2c->regs + HSI2C_TIMING_FS1);
		writel(i2c_timing_s2, i2c->regs + HSI2C_TIMING_FS2);
		writel(i2c_timing_s3, i2c->regs + HSI2C_TIMING_FS3);
	}
	writel(i2c_timing_sla, i2c->regs + HSI2C_TIMING_SLA);

	return 0;
}

static int exynos5_hsi2c_clock_setup(struct exynos5_i2c *i2c)
{
	if (i2c->support_hsi2c_batcher)
		return 0;
	/*
	 * Configure the Fast speed timing values
	 * Even the High Speed mode initially starts with Fast mode
	 */
	if (exynos5_i2c_set_timing(i2c, HSI2C_FAST_SPD)) {
		dev_err(i2c->dev, "HSI2C FS Clock set up failed\n");
		return -EINVAL;
	}

	/* configure the High speed timing values */
	if (i2c->speed_mode == HSI2C_HIGH_SPD) {
		if (exynos5_i2c_set_timing(i2c, HSI2C_HIGH_SPD)) {
			dev_err(i2c->dev, "HSI2C HS Clock set up failed\n");
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * exynos5_i2c_init: configures the controller for I2C functionality
 * Programs I2C controller for Master mode operation
 */
static void exynos5_i2c_init(struct exynos5_i2c *i2c)
{
	u32 i2c_conf = readl(i2c->regs + HSI2C_CONF);

	writel(HSI2C_MASTER, i2c->regs + HSI2C_CTL);
	writel(HSI2C_TRAILING_COUNT, i2c->regs + HSI2C_TRAILIG_CTL);

	if (i2c->speed_mode == HSI2C_HIGH_SPD) {
		writel(HSI2C_MASTER_ID(MASTER_ID(i2c->bus_id)),
					i2c->regs + HSI2C_ADDR);
		i2c_conf |= HSI2C_HS_MODE;
	}

	writel(i2c_conf | HSI2C_AUTO_MODE, i2c->regs + HSI2C_CONF);

	i2c->need_hw_init = 0;
}

static void exynos5_i2c_reset(struct exynos5_i2c *i2c)
{
	u32 i2c_ctl;

	/* Set and clear the bit for reset */
	i2c_ctl = readl(i2c->regs + HSI2C_CTL);
	i2c_ctl |= HSI2C_SW_RST;
	writel(i2c_ctl, i2c->regs + HSI2C_CTL);

	i2c_ctl = readl(i2c->regs + HSI2C_CTL);
	i2c_ctl &= ~HSI2C_SW_RST;
	writel(i2c_ctl, i2c->regs + HSI2C_CTL);

	/* We don't expect calculations to fail during the run */
	exynos5_hsi2c_clock_setup(i2c);
	/* Initialize the configure registers */
	exynos5_i2c_init(i2c);
}

static inline void exynos5_i2c_stop(struct exynos5_i2c *i2c)
{
	writel(0, i2c->regs + HSI2C_INT_ENABLE);

	complete(&i2c->msg_complete);
}

static void set_batcher_enable(struct exynos5_i2c *i2c)
{
	u32 i2c_batcher_con;

	i2c_batcher_con = readl(i2c->regs + HSI2C_BATCHER_CON);

	i2c_batcher_con |= HSI2C_BATCHER_ENABLE;
	i2c_batcher_con |= HSI2C_BATCHER_DEDICATE;

	writel(i2c_batcher_con, i2c->regs + HSI2C_BATCHER_CON);
}

static void start_batcher(struct exynos5_i2c *i2c)
{
	u32 i2c_batcher_con = 0x00;
	u32 i2c_batcher_int = 0x00;

	i2c_batcher_int |= HSI2C_BATCHER_INT_ENABLE;
	writel(i2c_batcher_int, i2c->regs + HSI2C_BATCHER_INT_EN);


	i2c_batcher_con = readl(i2c->regs + HSI2C_BATCHER_CON);
	i2c_batcher_con |= HSI2C_BATCHER_START;
	writel(i2c_batcher_con, i2c->regs + HSI2C_BATCHER_CON);
}

static void stop_batcher(struct exynos5_i2c *i2c)
{
	writel(0x0, i2c->regs + HSI2C_BATCHER_INT_EN);
}

static void set_batcher_idle(struct exynos5_i2c *i2c)
{
	u32 i2c_batcher_con = 0x00;
	writel(i2c_batcher_con, i2c->regs + HSI2C_BATCHER_CON);
}

static void reset_batcher(struct exynos5_i2c *i2c)
{
	u32 i2c_batcher_con = 0x00;

#ifdef CONFIG_GPIOLIB
	if (i2c->secure_mode) /* this is for secure gpio port recovery (Grace Secure Camera only) */
		recover_gpio_pins_secure(i2c);
	else
		recover_gpio_pins(i2c);
#endif

	i2c_batcher_con |= HSI2C_BATCHER_RESET;
	writel(i2c_batcher_con, i2c->regs + HSI2C_BATCHER_CON);

	i2c_batcher_con &= ~HSI2C_BATCHER_RESET;
	writel(i2c_batcher_con, i2c->regs + HSI2C_BATCHER_CON);

	if (readl(i2c->regs + HSI2C_BATCHER_STATE) & BATCHER_OPERATION_COMPLETE)
		writel(BATCHER_OPERATION_COMPLETE, i2c->regs + HSI2C_BATCHER_STATE);

	exynos5_i2c_reset(i2c);
	set_batcher_idle(i2c);
}

static void release_ap_semaphore(struct exynos5_i2c *i2c)
{
	writel(0x01, i2c->regs + HSI2C_SMRelease);
}

static void recover_batcher(struct exynos5_i2c *i2c, u32 batcher_state)
{
	if (batcher_state == BATCHER_IDLE_STATE) {
		dev_warn(i2c->dev, "Batcher State is IDLE\n");
	} else if (batcher_state == BATCHER_INIT_STATE) {
		dev_warn(i2c->dev, "Batcher State is INIT\n");
	} else if (batcher_state == BATCHER_GET_SEMAPHORE_STATE) {
		dev_warn(i2c->dev, "Batcher State is GET_SEMA\n");
	} else if (batcher_state == BATCHER_REL_SEMAPHORE_STATE) {
		dev_warn(i2c->dev, "Batcher State is REL_SEMA\n");
	} else if (batcher_state == BATCHER_GEN_INT_STATE) {
		dev_warn(i2c->dev, "Batcher State is GET_INT\n");
	} else if ((batcher_state >= BATCHER_CONFIG_STATE) &&
			(batcher_state <= BATCHER_CLEAN_INTR_STATE)) {
		dev_warn(i2c->dev, "Batcher recovery is started\n");
		reset_batcher(i2c);
		release_ap_semaphore(i2c);
		dev_warn(i2c->dev, "Batcher recovery was done\n");
		return;
	} else {
		/* BATCHER_UNEXPECTED_INT */
		dev_warn(i2c->dev, "Batcher State is UNEXPECTED_INT\n");
	}

	dev_warn(i2c->dev, "Batcher can't be recovered\n");
}

static void finalize_batcher(struct exynos5_i2c *i2c)
{
	write_batcher(i2c, 0x00, 0xff);

	/* Initialize batcher related variables */
	i2c->desc_pointer = 0;
	i2c->cmd_index = 0;
	i2c->cmd_pointer = 0;
	i2c->cmd_buffer = HSI2C_BATCHER_INIT_CMD;
}

/*
 * exynos5_i2c_irq_batcher: Batcher IRQ servicing routine
 *
 * INT_STATUS registers gives the interrupt details. Further,
 * HSI2C_BATCHER_STATE or FIFO_STATUS registers are to be check for detailed
 * state of the bus.
 */

static irqreturn_t exynos5_i2c_irq_batcher(int irqno, void *dev_id)
{
	struct exynos5_i2c *i2c = dev_id;
	unsigned char byte;
	unsigned char i = 0;
	unsigned int i2c_batcher_state;
	unsigned int i2c_int_state;

	i2c_batcher_state = readl(i2c->regs + HSI2C_BATCHER_STATE);
	i2c_int_state = readl(i2c->regs + HSI2C_BATCHER_INT_STATUS);

	if (i2c_batcher_state & UNEXPECTED_HSI2C_INTR) {
		i2c->trans_done = -ENXIO;
		goto out;
	}

	if (i2c->msg->flags & I2C_M_RD) {
		do {
			byte = (unsigned char)readl(i2c->regs +
				i2c->batcher_read_addr + (i++ * 4));
			i2c->msg->buf[i2c->msg_ptr++] = byte;
		} while (i2c->msg_ptr < i2c->msg->len);
	}

out:
	complete(&i2c->msg_complete);

	i2c_batcher_state |= BATCHER_OPERATION_COMPLETE;

	writel(i2c_batcher_state, i2c->regs + HSI2C_BATCHER_STATE);
	writel(i2c_int_state, i2c->regs + HSI2C_BATCHER_INT_STATUS);

	/* Initialize Batcher */
	set_batcher_idle(i2c);

	return IRQ_HANDLED;
}

static irqreturn_t exynos5_i2c_irq(int irqno, void *dev_id)
{
	struct exynos5_i2c *i2c = dev_id;
	unsigned long reg_val;
	unsigned long trans_status;
	unsigned char byte;

	if (i2c->msg->flags & I2C_M_RD) {
		while ((readl(i2c->regs + HSI2C_FIFO_STATUS) &
			0x1000000) == 0) {
			byte = (unsigned char)readl(i2c->regs + HSI2C_RX_DATA);
			i2c->msg->buf[i2c->msg_ptr++] = byte;
		}

		if (i2c->msg_ptr >= i2c->msg->len) {
			reg_val = readl(i2c->regs + HSI2C_INT_ENABLE);
			reg_val &= ~(HSI2C_INT_RX_ALMOSTFULL_EN);
			writel(reg_val, i2c->regs + HSI2C_INT_ENABLE);
			exynos5_i2c_stop(i2c);
		}
	} else {
		while ((readl(i2c->regs + HSI2C_FIFO_STATUS) &
			0x80) == 0) {
			if (i2c->msg_ptr >= i2c->msg->len) {
				reg_val = readl(i2c->regs + HSI2C_INT_ENABLE);
				reg_val &= ~(HSI2C_INT_TX_ALMOSTEMPTY_EN);
				writel(reg_val, i2c->regs + HSI2C_INT_ENABLE);
				break;
			}
			byte = i2c->msg->buf[i2c->msg_ptr++];
			writel(byte, i2c->regs + HSI2C_TX_DATA);
		}
	}

	reg_val = readl(i2c->regs + HSI2C_INT_STATUS);

	/*
	 * Checking Error State in INT_STATUS register
	 */
	if (reg_val & HSI2C_INT_CHK_TRANS_STATE) {
		trans_status = readl(i2c->regs + HSI2C_TRANS_STATUS);
		dev_err(i2c->dev, "HSI2C Error Interrupt "
				"occurred(IS:0x%08x, TR:0x%08x)\n",
				(unsigned int)reg_val, (unsigned int)trans_status);
		i2c->trans_done = -ENXIO;
		exynos5_i2c_stop(i2c);
		goto out;
	}
	/* Checking INT_TRANSFER_DONE */
	if ((reg_val & HSI2C_INT_TRANSFER_DONE) &&
		(i2c->msg_ptr >= i2c->msg->len) &&
		!(i2c->msg->flags & I2C_M_RD))
		exynos5_i2c_stop(i2c);

out:
	writel(reg_val, i2c->regs +  HSI2C_INT_STATUS);

	return IRQ_HANDLED;
}

static int exynos5_i2c_xfer_msg(struct exynos5_i2c *i2c, struct i2c_msg *msgs, int stop)
{
	unsigned long timeout;
	unsigned long trans_status;
	unsigned long i2c_ctl;
	unsigned long i2c_auto_conf;
	unsigned long i2c_timeout;
	unsigned long i2c_addr;
	unsigned long i2c_int_en;
	unsigned long i2c_fifo_ctl;
	unsigned char byte;
	int ret = 0;
	int operation_mode = i2c->operation_mode;

	i2c->msg = msgs;
	i2c->msg_ptr = 0;
	i2c->trans_done = 0;

	reinit_completion(&i2c->msg_complete);

	i2c_ctl = readl(i2c->regs + HSI2C_CTL);
	i2c_auto_conf = readl(i2c->regs + HSI2C_AUTO_CONF);
	i2c_timeout = readl(i2c->regs + HSI2C_TIMEOUT);
	i2c_timeout &= ~HSI2C_TIMEOUT_EN;
	writel(i2c_timeout, i2c->regs + HSI2C_TIMEOUT);

	i2c_fifo_ctl = HSI2C_RXFIFO_EN | HSI2C_TXFIFO_EN |
		HSI2C_TXFIFO_TRIGGER_LEVEL | HSI2C_RXFIFO_TRIGGER_LEVEL;
	writel(i2c_fifo_ctl, i2c->regs + HSI2C_FIFO_CTL);

	i2c_int_en = 0;
	if (msgs->flags & I2C_M_RD) {
		i2c_ctl &= ~HSI2C_TXCHON;
		i2c_ctl |= HSI2C_RXCHON;

		i2c_auto_conf |= HSI2C_READ_WRITE;

		i2c_int_en |= (HSI2C_INT_RX_ALMOSTFULL_EN |
			HSI2C_INT_TRAILING_EN);
	} else {
		i2c_ctl &= ~HSI2C_RXCHON;
		i2c_ctl |= HSI2C_TXCHON;

		i2c_auto_conf &= ~HSI2C_READ_WRITE;

		i2c_int_en |= HSI2C_INT_TX_ALMOSTEMPTY_EN;
	}

	if (operation_mode == HSI2C_INTERRUPT)
		exynos5_i2c_clr_pend_irq(i2c);

	if ((stop == 1) || (i2c->stop_after_trans == 1))
		i2c_auto_conf |= HSI2C_STOP_AFTER_TRANS;
	else
		i2c_auto_conf &= ~HSI2C_STOP_AFTER_TRANS;

	i2c_addr = readl(i2c->regs + HSI2C_ADDR);
	i2c_addr &= ~(0x3ff << 10);
	i2c_addr &= ~(0x3ff << 0);
	i2c_addr &= ~(0xff << 24);
	i2c_addr |= ((msgs->addr & 0x7f) << 10);
	writel(i2c_addr, i2c->regs + HSI2C_ADDR);

	writel(i2c_ctl, i2c->regs + HSI2C_CTL);

	i2c_auto_conf &= ~(0xffff);
	i2c_auto_conf |= i2c->msg->len;
	writel(i2c_auto_conf, i2c->regs + HSI2C_AUTO_CONF);

	i2c_auto_conf = readl(i2c->regs + HSI2C_AUTO_CONF);
	i2c_auto_conf |= HSI2C_MASTER_RUN;
	writel(i2c_auto_conf, i2c->regs + HSI2C_AUTO_CONF);

	if (operation_mode == HSI2C_INTERRUPT) {
		i2c_int_en |= HSI2C_INT_CHK_TRANS_STATE | HSI2C_INT_TRANSFER_DONE;
		writel(i2c_int_en, i2c->regs + HSI2C_INT_ENABLE);
		enable_irq(i2c->irq);
	} else {
		writel(0x0, i2c->regs + HSI2C_INT_ENABLE);
	}

	ret = -EAGAIN;
	if (msgs->flags & I2C_M_RD) {
		if (operation_mode == HSI2C_POLLING) {
			timeout = jiffies + EXYNOS5_I2C_TIMEOUT;
			while (time_before(jiffies, timeout)){
				if ((readl(i2c->regs + HSI2C_FIFO_STATUS) &
					0x1000000) == 0) {
					byte = (unsigned char)readl
						(i2c->regs + HSI2C_RX_DATA);
					i2c->msg->buf[i2c->msg_ptr++]
						= byte;
				}

				if (i2c->msg_ptr >= i2c->msg->len) {
					ret = 0;
					break;
				}
			}

			if (ret == -EAGAIN) {
				dump_i2c_register(i2c);
				exynos5_i2c_reset(i2c);
				dev_warn(i2c->dev, "rx timeout\n");
				return ret;
			}
		} else {
			timeout = wait_for_completion_timeout
				(&i2c->msg_complete, EXYNOS5_I2C_TIMEOUT);

			ret = 0;
			if (i2c->scl_clk_stretch) {
				unsigned long timeout = jiffies + msecs_to_jiffies(100);

				do {
					trans_status = readl(i2c->regs + HSI2C_TRANS_STATUS);
					if ((!(trans_status & HSI2C_MAST_ST_MASK)) ||
					   ((stop == 0) && (trans_status & HSI2C_MASTER_BUSY))){
						timeout = 0;
						break;
					}
				} while(time_before(jiffies, timeout));

				if (timeout)
					dev_err(i2c->dev, "SDA check timeout!!! = 0x%8lx\n",trans_status);
			}
			disable_irq(i2c->irq);

			if (i2c->trans_done < 0) {
				dev_err(i2c->dev, "ack was not received at read\n");
				ret = i2c->trans_done;
				exynos5_i2c_reset(i2c);
			}

			if (timeout == 0) {
				dump_i2c_register(i2c);
				exynos5_i2c_reset(i2c);
				dev_warn(i2c->dev, "rx timeout\n");
				ret = -EAGAIN;
				return ret;
			}
		}
	} else {
		if (operation_mode == HSI2C_POLLING) {
			timeout = jiffies + EXYNOS5_I2C_TIMEOUT;
			while (time_before(jiffies, timeout) &&
				(i2c->msg_ptr < i2c->msg->len)) {
				if ((readl(i2c->regs + HSI2C_FIFO_STATUS)
					& HSI2C_TX_FIFO_LVL_MASK) < EXYNOS5_FIFO_SIZE) {
					byte = i2c->msg->buf
						[i2c->msg_ptr++];
					writel(byte,
						i2c->regs + HSI2C_TX_DATA);
				}
			}

		} else {
			timeout = wait_for_completion_timeout
				(&i2c->msg_complete, EXYNOS5_I2C_TIMEOUT);
			disable_irq(i2c->irq);

			if (timeout == 0) {
				dump_i2c_register(i2c);
				exynos5_i2c_reset(i2c);
				dev_warn(i2c->dev, "tx timeout\n");
				return ret;
			}

			timeout = jiffies + timeout;
		}

		if (operation_mode == HSI2C_POLLING) {
			while (time_before(jiffies, timeout)) {
				trans_status = readl(i2c->regs + HSI2C_INT_STATUS);
				writel(trans_status, i2c->regs +  HSI2C_INT_STATUS);
				if (trans_status & HSI2C_INT_TRANSFER_DONE) {
					ret = 0;
					break;
				}
				udelay(100);
			}
			if (ret == -EAGAIN) {
				dump_i2c_register(i2c);
				exynos5_i2c_reset(i2c);
				dev_warn(i2c->dev, "tx timeout\n");
				return ret;
			}
		} else {
			if (i2c->trans_done < 0) {
				dev_err(i2c->dev, "ack was not received at write\n");
				ret = i2c->trans_done;
				exynos5_i2c_reset(i2c);
			} else {
				if (i2c->scl_clk_stretch) {
					unsigned long timeout = jiffies + msecs_to_jiffies(100);

					do {
						trans_status = readl(i2c->regs + HSI2C_TRANS_STATUS);
						if ((!(trans_status & HSI2C_MAST_ST_MASK)) ||
						   ((stop == 0) && (trans_status & HSI2C_MASTER_BUSY))){
							timeout = 0;
							break;
						}
					} while(time_before(jiffies, timeout));

					if (timeout)
						dev_err(i2c->dev, "SDA check timeout!!! = 0x%8lx\n",trans_status);
				}

				ret = 0;
			}
		}
	}

	return ret;
}

static int exynos5_i2c_xfer_batcher(struct exynos5_i2c *i2c,
					struct i2c_msg *msgs, int stop)
{
	unsigned long timeout;
	unsigned long i2c_ctl = 0;
	unsigned long i2c_auto_conf = 0;
	unsigned long i2c_timeout = 0x80ff;
	unsigned long i2c_addr = 0;
	unsigned long i2c_int_en = 0;
	unsigned long i2c_fifo_ctl;
	unsigned char byte;

	int ret = 0;

	unsigned int i2c_conf = 0x00;
	unsigned int i2c_read_length;
	unsigned int i2c_batcher_state;
	unsigned char i = 0;

	i2c->msg = msgs;
	i2c->msg_ptr = 0;
	i2c->trans_done = 0;
	i2c->desc_pointer = 0;

	reinit_completion(&i2c->msg_complete);

	/*****************************/
	/* Set Batcher IDLE Status   */
	/*****************************/
	set_batcher_idle(i2c);

	/********************************************/
	/* initialization batcher -> enable batcher */
	/********************************************/
	set_batcher_enable(i2c);

	if (i2c->hs_clock == 3000000) {
		/* Set HSI2C Timing Parameters for 3.0Mhz */
		write_batcher(i2c, ((7 << 24)|(7 << 16)|(7 << 8)|(7)), HSI2C_TIMING_HS1);
		write_batcher(i2c, ((0xF << 16)|(3 << 24)|(7 << 8)|(1)), HSI2C_TIMING_HS2);
		write_batcher(i2c, ((0x0)|(0 << 16)|(8)), HSI2C_TIMING_HS3);
		write_batcher(i2c, ((0x0)|(3)), HSI2C_TIMING_SLA);
	} else if (i2c->hs_clock == 2500000) {
		/* Set HSI2C Timing Parameters for 2.5Mhz */
		write_batcher(i2c, ((10 << 24)|(10 << 16)|(10 << 8)|(10)), HSI2C_TIMING_HS1);
		write_batcher(i2c, ((0xF << 16)|(5 << 24)|(10 << 8)|(2)), HSI2C_TIMING_HS2);
		write_batcher(i2c, ((0x0)|(0 << 16)|(12)), HSI2C_TIMING_HS3);
		write_batcher(i2c, ((0x0)|(5)), HSI2C_TIMING_SLA);
	}

	write_batcher(i2c, ((0x0)|(38 << 24)|(38 << 16)|(38 << 8)), HSI2C_TIMING_FS1);
	write_batcher(i2c, ((0xF << 16)|(19 << 24)|(38 << 8)|(38)), HSI2C_TIMING_FS2);
	write_batcher(i2c, ((0x0)|(1 << 16)|(76)), HSI2C_TIMING_FS3);

	if (i2c->need_cs_enb) {
		/* Set HSI2C CTL[0] CS_ENB as 1 for 2.5Mhz SCL frequency */
		i2c_ctl |= HSI2C_CS_ENB;
	}

	/* Set HSI2C Trailing Register */
	write_batcher(i2c, BATCHER_TRAILING_COUNT, HSI2C_TRAILIG_CTL);

	/* Set HSI2C Configuration Register */
	if (msgs->flags & I2C_M_RD)
		i2c_conf |= HSI2C_10BIT_ADDR_MODE;
	else
		i2c_conf &= ~HSI2C_10BIT_ADDR_MODE;

	i2c_conf |= HSI2C_HS_MODE | HSI2C_AUTO_MODE;

	i2c_conf &= ~HSI2C_FTL_CYCLE_SCL_MASK;
	i2c_conf |= 3 << 16;

	i2c_conf &= ~HSI2C_FTL_CYCLE_SDA_MASK;
	i2c_conf |= 3 << 13;

	i2c_conf |= HSI2C_FILTER_EN_SCL | HSI2C_FILTER_EN_SDA;

	i2c_conf |= 0xff;

	write_batcher(i2c, i2c_conf, HSI2C_CONF);

	/* Set HSI2C Timeout Register */
	i2c_timeout &= ~HSI2C_TIMEOUT_EN;
	i2c_timeout = 0x00;
	write_batcher(i2c, i2c_timeout, HSI2C_TIMEOUT);

	/* Set HSI2C FIFO Control Register */
	i2c_fifo_ctl = HSI2C_RXFIFO_EN | HSI2C_TXFIFO_EN |
			HSI2C_TXFIFO_TRIGGER_LEVEL;

	i2c_fifo_ctl |= i2c->msg->len << 4;
	write_batcher(i2c, i2c_fifo_ctl, HSI2C_FIFO_CTL);

	/* Set HSI2C Control Register */
	i2c_ctl |= HSI2C_MASTER;

	if (msgs->flags & I2C_M_RD) {
		i2c_ctl &= ~HSI2C_TXCHON;
		i2c_ctl |= HSI2C_RXCHON;

		i2c_ctl &= ~HSI2C_EXT_ADDR;
		i2c_ctl |= HSI2C_EXT_ADDR;

		if (msgs->addr & 0x4000) {
			i2c_ctl &= ~HSI2C_EXT_MSB;
			i2c_ctl |= HSI2C_EXT_MSB;
		} else {
			i2c_ctl &= ~HSI2C_EXT_MSB;
		}
	} else {
		i2c_ctl &= ~HSI2C_RXCHON;
		i2c_ctl |= HSI2C_TXCHON;

		i2c_ctl &= ~HSI2C_EXT_ADDR;
		i2c_ctl &= ~HSI2C_EXT_MSB;
	}
	write_batcher(i2c, i2c_ctl, HSI2C_CTL);

	if (msgs->flags & I2C_M_RD)
		i2c_addr |= ((msgs->addr & 0x3fff) << 10);
	else
		i2c_addr |= ((msgs->addr & 0x7f) << 10);

	write_batcher(i2c, i2c_addr, HSI2C_ADDR);

	/* Set HSI2C Auto Configuration Register */
	if (msgs->flags & I2C_M_RD)
		i2c_auto_conf |= HSI2C_READ_WRITE;
	else
		i2c_auto_conf &= ~HSI2C_READ_WRITE;

	if (stop == 1)
		i2c_auto_conf |= HSI2C_STOP_AFTER_TRANS;
	else
		i2c_auto_conf &= ~HSI2C_STOP_AFTER_TRANS;

	i2c_auto_conf |= i2c->msg->len;
	i2c_auto_conf |= HSI2C_MASTER_RUN;
	write_batcher(i2c, i2c_auto_conf, HSI2C_AUTO_CONF);

	/* Enable HSI2C Interrupt */
	if (msgs->flags & I2C_M_RD)
		i2c_int_en |= HSI2C_INT_RX_ALMOSTFULL_EN;
	else
		i2c_int_en |= HSI2C_INT_TX_ALMOSTEMPTY_EN |
				HSI2C_INT_TRANSFER_DONE;

	writel(readl(i2c->regs + HSI2C_BATCHER_INT_STATUS),
		i2c->regs + HSI2C_BATCHER_INT_STATUS);

	write_batcher(i2c, i2c_int_en, HSI2C_INT_ENABLE);
	enable_irq(i2c->irq);

	/* Fill the Batcher with Read and Write Data*/
	if (msgs->flags & I2C_M_RD) {
		i2c->batcher_read_addr = HSI2C_START_PAYLOAD +
					((i2c->desc_pointer) * 4);
		i2c_read_length = i2c->msg->len;

		do {
			write_batcher(i2c, 0x77, HSI2C_RX_DATA);
		} while (--i2c_read_length != 0);
	} else {
		i2c_read_length = i2c->msg->len;
		do {
			byte = i2c->msg->buf[i2c->msg_ptr++];
			write_batcher(i2c, byte, HSI2C_TX_DATA);
		} while (--i2c_read_length != 0);
	}

	/* Finalize Batcher */
	finalize_batcher(i2c);

	/* Batcher enable Interrupt and start to work for execution opcode */
	start_batcher(i2c);

	ret = -EAGAIN;
	if (msgs->flags & I2C_M_RD) {

		timeout = wait_for_completion_timeout
			(&i2c->msg_complete, EXYNOS5_BATCHER_TIMEOUT);


		/* disable batcher interrupt for preventing unintended interrupt */
		stop_batcher(i2c);
		disable_irq(i2c->irq);

		if (i2c->trans_done < 0) {
			dev_warn(i2c->dev, "Unexpected Batcher Interrupt at Read\n");

			dev_warn(i2c->dev, "Batcher State= %x\n",
			readl(i2c->regs + HSI2C_BATCHER_STATE));
			dev_warn(i2c->dev, "Batcher FIFO Status= %x\n",
			readl(i2c->regs + HSI2C_BATCHER_FIFO_STATUS));
			dev_warn(i2c->dev, "Batcher INT Status= %x\n",
			readl(i2c->regs + HSI2C_BATCHER_INT_STATUS));

			ret = i2c->trans_done;
			return ret;
		}

		if (timeout == 0) {
			i2c_batcher_state = readl(i2c->regs + HSI2C_BATCHER_STATE);

			if (i2c_batcher_state & BATCHER_OPERATION_COMPLETE) {
				do {
					byte = (unsigned char)readl(i2c->regs +
						i2c->batcher_read_addr + (i++ * 4));
					i2c->msg->buf[i2c->msg_ptr++] = byte;
				} while (i2c->msg_ptr < i2c->msg->len);

				i2c_batcher_state |= BATCHER_OPERATION_COMPLETE;
				writel(i2c_batcher_state, i2c->regs + HSI2C_BATCHER_STATE);

				/* Initialize Batcher */
				set_batcher_idle(i2c);
			} else {
				/* Read Error handlilng for HSI2C_Batcher */
				dev_warn(i2c->dev, "rx timeout Batcher status= %x\n",
				readl(i2c->regs + HSI2C_BATCHER_STATE));

				dev_warn(i2c->dev, "Batcher FIFO Status= %x\n",
				readl(i2c->regs + HSI2C_BATCHER_FIFO_STATUS));
				dev_warn(i2c->dev, "Batcher INT Status= %x\n",
				readl(i2c->regs + HSI2C_BATCHER_INT_STATUS));

				/* Batcher recovery */
				recover_batcher(i2c, i2c_batcher_state);

				return ret;
			}
		}
		ret = 0;
	} else {
		timeout = wait_for_completion_timeout
			(&i2c->msg_complete, EXYNOS5_I2C_TIMEOUT);

		/* disable batcher interrupt for preventing unintended interrupt */
		stop_batcher(i2c);
		disable_irq(i2c->irq);

		if (i2c->trans_done < 0) {
			dev_warn(i2c->dev, "Unexpected Batcher Interrupt at Write\n");

			dev_warn(i2c->dev, "Batcher State= %x\n",
			readl(i2c->regs + HSI2C_BATCHER_STATE));
			dev_warn(i2c->dev, "Batcher FIFO Status= %x\n",
			readl(i2c->regs + HSI2C_BATCHER_FIFO_STATUS));
			dev_warn(i2c->dev, "Batcher INT Status= %x\n",
			readl(i2c->regs + HSI2C_BATCHER_INT_STATUS));

			ret = i2c->trans_done;
			return ret;
		}

		if (timeout == 0) {
			i2c_batcher_state = readl(i2c->regs + HSI2C_BATCHER_STATE);

			if (i2c_batcher_state & BATCHER_OPERATION_COMPLETE) {
				i2c_batcher_state |= BATCHER_OPERATION_COMPLETE;
				writel(i2c_batcher_state, i2c->regs + HSI2C_BATCHER_STATE);

				/* Initialize Batcher */
				set_batcher_idle(i2c);
			} else {
				/* Write Error handlilng for HSI2C_Batcher */
				dev_warn(i2c->dev, "tx timeout Batcher status= %x\n",
				readl(i2c->regs + HSI2C_BATCHER_STATE));

				dev_warn(i2c->dev, "Batcher FIFO Status= %x\n",
				readl(i2c->regs + HSI2C_BATCHER_FIFO_STATUS));
				dev_warn(i2c->dev, "Batcher INT Status= %x\n",
				readl(i2c->regs + HSI2C_BATCHER_INT_STATUS));

				/* Batcher recovery */
				recover_batcher(i2c, i2c_batcher_state);

				return ret;
			}
		}
		ret = 0;
	}
	return ret;
}

static int exynos5_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	struct exynos5_i2c *i2c = (struct exynos5_i2c *)adap->algo_data;
	struct i2c_msg *msgs_ptr = msgs;
	int retry, i = 0;
	int ret = 0;
	int stop = 0;

#ifdef CONFIG_PM_RUNTIME
	int clk_ret = 0;
#endif

	if (i2c->suspended) {
		dev_err(i2c->dev, "HS-I2C is not initialzed.\n");
		return -EIO;
	}

#ifdef CONFIG_PM_RUNTIME
	clk_ret = pm_runtime_get_sync(i2c->dev);
	if (clk_ret < 0) {
		exynos_update_ip_idle_status(i2c->idle_ip_index, 0);
		clk_prepare_enable(i2c->clk);
	}
#else
	exynos_update_ip_idle_status(i2c->idle_ip_index, 0);
	clk_prepare_enable(i2c->clk);
#endif
	/* If master is in arbitration lost state before transfer */
	/* master should be reset */
	if (i2c->reset_before_trans) {
		if (unlikely((readl(i2c->regs + HSI2C_TRANS_STATUS)
			& HSI2C_MAST_ST_MASK) == 0xC)) {
			i2c->need_hw_init = 1;
		}
	}

	if ((i2c->need_hw_init) && !(i2c->support_hsi2c_batcher))
		exynos5_i2c_reset(i2c);

	if (!(i2c->support_hsi2c_batcher)) {
		if (unlikely(!(readl(i2c->regs + HSI2C_CONF)
			& HSI2C_AUTO_MODE))) {
			dev_err(i2c->dev, "HSI2C should be reconfigured\n");
			exynos5_hsi2c_clock_setup(i2c);
			exynos5_i2c_init(i2c);
		}
	}

	for (retry = 0; retry < adap->retries; retry++) {
		for (i = 0; i < num; i++) {
			stop = (i == num - 1);

			if (i2c->transfer_delay)
				udelay(i2c->transfer_delay);

			if (i2c->support_hsi2c_batcher)
				ret = exynos5_i2c_xfer_batcher(i2c, msgs_ptr, stop);
			else
				ret = exynos5_i2c_xfer_msg(i2c, msgs_ptr, stop);

			msgs_ptr++;

			if (ret == -EAGAIN) {
				msgs_ptr = msgs;
				break;
			} else if (ret < 0) {
				goto out;
			}
		}

		if ((i == num) && (ret != -EAGAIN))
			break;

		dev_dbg(i2c->dev, "retrying transfer (%d)\n", retry);

		udelay(100);
	}

	if (i == num) {
		ret = num;
	} else {
		ret = -EREMOTEIO;
		dev_warn(i2c->dev, "xfer message failed\n");
	}

 out:
#ifdef CONFIG_PM_RUNTIME
	if (clk_ret < 0) {
		clk_disable_unprepare(i2c->clk);
		exynos_update_ip_idle_status(i2c->idle_ip_index, 1);
	} else {
		pm_runtime_mark_last_busy(i2c->dev);
		pm_runtime_put_autosuspend(i2c->dev);
	}
#else
	clk_disable_unprepare(i2c->clk);
	exynos_update_ip_idle_status(i2c->idle_ip_index, 1);
#endif

	return ret;
}

static u32 exynos5_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static const struct i2c_algorithm exynos5_i2c_algorithm = {
	.master_xfer		= exynos5_i2c_xfer,
	.functionality		= exynos5_i2c_func,
};

#ifdef CONFIG_CPU_IDLE
static int exynos5_i2c_notifier(struct notifier_block *self,
				unsigned long cmd, void *v)
{
	struct exynos5_i2c *i2c;

	switch (cmd) {
	case LPA_EXIT:
		list_for_each_entry(i2c, &drvdata_list, node)
			i2c->need_hw_init = 1;
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos5_i2c_notifier_block = {
	.notifier_call = exynos5_i2c_notifier,
};
#endif /* CONFIG_CPU_IDLE */

static int exynos5_i2c_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct exynos5_i2c *i2c;
	struct resource *mem;
	int ret;
	unsigned int tmp;
	void __iomem *pmu_batcher;

	if (!np) {
		dev_err(&pdev->dev, "no device node\n");
		return -ENOENT;
	}

	i2c = devm_kzalloc(&pdev->dev, sizeof(struct exynos5_i2c), GFP_KERNEL);
	if (!i2c) {
		dev_err(&pdev->dev, "no memory for state\n");
		return -ENOMEM;
	}

	/* Mode of operation High/Fast Speed mode */
	if (of_get_property(np, "samsung,hs-mode", NULL)) {
		i2c->speed_mode = HSI2C_HIGH_SPD;
		i2c->fs_clock = HSI2C_FS_TX_CLOCK;
		if (of_property_read_u32(np, "clock-frequency", &i2c->hs_clock))
			i2c->hs_clock = HSI2C_HS_TX_CLOCK;
	} else {
		i2c->speed_mode = HSI2C_FAST_SPD;
		if (of_property_read_u32(np, "clock-frequency", &i2c->fs_clock))
			i2c->fs_clock = HSI2C_FS_TX_CLOCK;
	}

	/* Mode of operation Polling/Interrupt mode */
	if (of_get_property(np, "samsung,polling-mode", NULL)) {
		i2c->operation_mode = HSI2C_POLLING;
	} else {
		i2c->operation_mode = HSI2C_INTERRUPT;
	}

	if (of_get_property(np, "samsung,scl-clk-stretching", NULL))
		i2c->scl_clk_stretch = 1;
	else
		i2c->scl_clk_stretch = 0;

	ret = of_property_read_u32(np, "samsung,transfer_delay", &i2c->transfer_delay);
	if (!ret)
		dev_warn(&pdev->dev, "Transfer delay is not needed.\n");

	if (of_get_property(np, "samsung,stop-after-trans", NULL))
		i2c->stop_after_trans = 1;
	else
		i2c->stop_after_trans = 0;

	if (of_get_property(np, "samsung,use-old-timing-values", NULL))
		i2c->use_old_timing_values = 1;
	else
		i2c->use_old_timing_values = 0;

	if (of_get_property(np, "samsung,hsi2c-batcher", NULL)) {
		i2c->support_hsi2c_batcher = 1;
		i2c->cmd_buffer = HSI2C_BATCHER_INIT_CMD;
	} else
		i2c->support_hsi2c_batcher = 0;

	if (of_get_property(np, "samsung,need-cs-enb", NULL)) {
		i2c->need_cs_enb = 1;
	} else
		i2c->need_cs_enb = 0;

	if (of_get_property(np, "samsung,reset-before-trans", NULL))
		i2c->reset_before_trans = 1;
	else
		i2c->reset_before_trans = 0;

	if (of_get_property(np, "secure-mode", NULL))
		i2c->secure_mode = 1;
	else
		i2c->secure_mode = 0;

	i2c->idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev));

	strlcpy(i2c->adap.name, "exynos5-i2c", sizeof(i2c->adap.name));
	i2c->adap.owner   = THIS_MODULE;
	i2c->adap.algo    = &exynos5_i2c_algorithm;
	i2c->adap.retries = 2;
	i2c->adap.class   = I2C_CLASS_HWMON | I2C_CLASS_SPD;

	i2c->dev = &pdev->dev;
	i2c->clk = devm_clk_get(&pdev->dev, "gate_hsi2c");
	if (IS_ERR(i2c->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		return -ENOENT;
	}

	i2c->rate_clk = devm_clk_get(&pdev->dev, "rate_hsi2c");
	if (IS_ERR(i2c->rate_clk)) {
		dev_err(&pdev->dev, "cannot get rate clock\n");
		return -ENOENT;
	}

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev,
					EXYNOS5_HSI2C_RUNTIME_PM_DELAY);
	pm_runtime_enable(&pdev->dev);
#endif

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i2c->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (i2c->regs == NULL) {
		dev_err(&pdev->dev, "cannot map HS-I2C IO\n");
		ret = PTR_ERR(i2c->regs);
		goto err_clk;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (mem != NULL) {
		i2c->regs_mailbox = devm_ioremap_resource(&pdev->dev, mem);

		if (i2c->regs_mailbox == NULL) {
			dev_err(&pdev->dev, "cannot map MAILBOX IO\n");
			ret = PTR_ERR(i2c->regs_mailbox);
		}

		if (!i2c->support_hsi2c_batcher && i2c->regs_mailbox){
			tmp = readl(i2c->regs_mailbox + 0x40);
			tmp |= 0x1 << 3;
			writel(tmp, i2c->regs_mailbox + 0x40);
		}
	}

	if (i2c->support_hsi2c_batcher) {
		/* for enable Batcher in PMU */
		mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (mem != NULL) {
			pmu_batcher = devm_ioremap_resource(&pdev->dev, mem);

			if (pmu_batcher == NULL) {
				dev_err(&pdev->dev, "cannot map PMIC for batcher enable\n");
				ret = PTR_ERR(pmu_batcher);
			}
			writel(0x3,pmu_batcher);
		}
	}

	i2c->adap.dev.of_node = np;
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;

	init_completion(&i2c->msg_complete);

	if (i2c->operation_mode == HSI2C_INTERRUPT) {
		i2c->irq = ret = irq_of_parse_and_map(np, 0);
		if (ret <= 0) {
			dev_err(&pdev->dev, "cannot find HS-I2C IRQ\n");
			ret = -EINVAL;
			goto err_clk;
		}

		if (i2c->support_hsi2c_batcher) {
			i2c->irq = ret = irq_of_parse_and_map(np, 1);
			if (ret <= 0) {
				dev_err(&pdev->dev, "cannot find BATCHER IRQ\n");
				ret = -EINVAL;
				goto err_clk;
			}

			ret = devm_request_irq(&pdev->dev, i2c->irq,
				exynos5_i2c_irq_batcher, 0, dev_name(&pdev->dev)
				, i2c);
			disable_irq(i2c->irq);
		} else {
			ret = devm_request_irq(&pdev->dev, i2c->irq,
					exynos5_i2c_irq, 0, dev_name(&pdev->dev), i2c);
			disable_irq(i2c->irq);
		}

		if (ret != 0) {
			dev_err(&pdev->dev, "cannot request HS-I2C IRQ %d\n",
					i2c->irq);
			goto err_clk;
		}
	}
	platform_set_drvdata(pdev, i2c);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_get_sync(&pdev->dev);
#else
	exynos_update_ip_idle_status(i2c->idle_ip_index, 0);
	clk_prepare_enable(i2c->clk);
#endif

	/* Clear pending interrupts from u-boot or misc causes */
	exynos5_i2c_clr_pend_irq(i2c);
	/* Reset i2c SFR from u-boot or misc causes */
	exynos5_i2c_reset(i2c);

	if (!(i2c->support_hsi2c_batcher)) {
		ret = exynos5_hsi2c_clock_setup(i2c);
		if (ret)
			goto err_clk;
	}

	i2c->bus_id = of_alias_get_id(i2c->adap.dev.of_node, "hsi2c");

	exynos5_i2c_init(i2c);

	i2c->adap.nr = -1;
	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add bus to i2c core\n");
		goto err_clk;
	}

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);
#else
	clk_disable_unprepare(i2c->clk);
	exynos_update_ip_idle_status(i2c->idle_ip_index, 1);
#endif

#if defined(CONFIG_CPU_IDLE) || \
	defined(CONFIG_EXYNOS_APM)
	list_add_tail(&i2c->node, &drvdata_list);
#endif

#ifdef CONFIG_EXYNOS_APM
	if (of_get_property(np, "samsung,use-apm", NULL)) {
		i2c->use_apm_mode = 1;
		apm_i2c_pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(apm_i2c_pinctrl)) {
			dev_err(&pdev->dev, "Can't get apm i2c pinctrl.\n");
		} else {
			default_i2c_gpio = pinctrl_lookup_state(apm_i2c_pinctrl, "default");
			apm_i2c_gpio = pinctrl_lookup_state(apm_i2c_pinctrl, "apm");
		}

		/* When APM uses HSI2C device, APM can't control HSI2C clock
		 * because of clock synchronization. Therefore we don't disable the clock
		 * by calling clock enable function one more.
		 */
		if (of_get_property(np, "samsung,apm-always-clkon", NULL))
			clk_prepare_enable(i2c->clk);
	} else {
		i2c->use_apm_mode = 0;
	}

#endif
	return 0;

 err_clk:
	clk_disable_unprepare(i2c->clk);
	exynos_update_ip_idle_status(i2c->idle_ip_index, 1);
	return ret;
}

static int exynos5_i2c_remove(struct platform_device *pdev)
{
	struct exynos5_i2c *i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c->adap);

	return 0;
}

#ifdef CONFIG_PM
static int exynos5_i2c_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos5_i2c *i2c = platform_get_drvdata(pdev);

	i2c_lock_adapter(&i2c->adap);
	i2c->suspended = 1;
	i2c_unlock_adapter(&i2c->adap);

	return 0;
}

static int exynos5_i2c_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos5_i2c *i2c = platform_get_drvdata(pdev);

	i2c_lock_adapter(&i2c->adap);
	exynos_update_ip_idle_status(i2c->idle_ip_index, 0);
	clk_prepare_enable(i2c->clk);
	/* I2C for batcher doesn't need reset */
	if(!(i2c->support_hsi2c_batcher))
		exynos5_i2c_reset(i2c);
	clk_disable_unprepare(i2c->clk);
	exynos_update_ip_idle_status(i2c->idle_ip_index, 1);
	i2c->suspended = 0;
	i2c_unlock_adapter(&i2c->adap);

	return 0;
}

#else
static int exynos5_i2c_suspend_noirq(struct device *dev)
{
	return 0;
}

static int exynos5_i2c_resume_noirq(struct device *dev)
{
	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int exynos5_i2c_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos5_i2c *i2c = platform_get_drvdata(pdev);

	clk_disable_unprepare(i2c->clk);
	exynos_update_ip_idle_status(i2c->idle_ip_index, 1);

	return 0;
}

static int exynos5_i2c_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos5_i2c *i2c = platform_get_drvdata(pdev);

	exynos_update_ip_idle_status(i2c->idle_ip_index, 0);
	clk_prepare_enable(i2c->clk);

	return 0;
}
#endif /* CONFIG_PM_RUNTIME */

static const struct dev_pm_ops exynos5_i2c_pm = {
	.suspend_noirq = exynos5_i2c_suspend_noirq,
	.resume_noirq = exynos5_i2c_resume_noirq,
	SET_RUNTIME_PM_OPS(exynos5_i2c_runtime_suspend,
			   exynos5_i2c_runtime_resume, NULL)
};

static struct platform_driver exynos5_i2c_driver = {
	.probe		= exynos5_i2c_probe,
	.remove		= exynos5_i2c_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "exynos5-hsi2c",
		.pm	= &exynos5_i2c_pm,
		.of_match_table = exynos5_i2c_match,
		.suppress_bind_attrs = true,
	},
};

static int __init i2c_adap_exynos5_init(void)
{
#ifdef CONFIG_CPU_IDLE
	exynos_pm_register_notifier(&exynos5_i2c_notifier_block);
#endif
#ifdef CONFIG_EXYNOS_APM
	register_apm_notifier(&exynos_apm_notifier);
#endif
	return platform_driver_register(&exynos5_i2c_driver);
}
subsys_initcall(i2c_adap_exynos5_init);

static void __exit i2c_adap_exynos5_exit(void)
{
	platform_driver_unregister(&exynos5_i2c_driver);
}
module_exit(i2c_adap_exynos5_exit);

MODULE_DESCRIPTION("Exynos5 HS-I2C Bus driver");
MODULE_AUTHOR("Naveen Krishna Chatradhi, <ch.naveen@samsung.com>");
MODULE_AUTHOR("Taekgyun Ko, <taeggyun.ko@samsung.com>");
MODULE_LICENSE("GPL v2");
