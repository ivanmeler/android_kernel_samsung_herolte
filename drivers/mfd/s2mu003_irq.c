/* drivers/mfd/s2mu003_irq.c
 * S2MU003 Multifunction Device Driver
 * Charger / Buck / LDOs / FlashLED
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/mfd/samsung/s2mu003.h>
#include <linux/mfd/samsung/s2mu003_irq.h>
#include <linux/gpio.h>
#include <linux/device.h>

#define ALIAS_NAME "s2mu003_mfd_irq"

#define S2MU003_CHG_IRQ          0x60
#define S2MU003_LED_IRQ          0x66
#define S2MU003_PMIC_IRQ         0x68
#define S2MU003_OFF_EVENT        0x6B

#define S2MU003_CHG_IRQ_CTRL     0x63
#define S2MU003_CHG_IRQ1_CTRL    S2MU003_CHG_IRQ_CTRL
#define S2MU003_CHG_IRQ2_CTRL    (S2MU003_CHG_IRQ_CTRL + 1)
#define S2MU003_CHG_IRQ3_CTRL    (S2MU003_CHG_IRQ_CTRL + 2)
#define S2MU003_LED_IRQ_CTRL     0x67
#define S2MU003_PMIC_IRQ_CTRL    0x69

static const char *s2mu003_irq_names[] = {
	[S2MU003_EOC_IRQ] = S2MU003_EOC_IRQ_NAME,
	[S2MU003_CINIR_IRQ] = S2MU003_CINIR_IRQ_NAME,
	[S2MU003_BATP_IRQ] = S2MU003_BATP_IRQ_NAME,
	[S2MU003_BATLV_IRQ] = S2MU003_BATLV_IRQ_NAME,
	[S2MU003_TOPOFF_IRQ] = S2MU003_TOPOFF_IRQ_NAME,
	[S2MU003_CINOVP_IRQ] = S2MU003_CINOVP_IRQ_NAME,
	[S2MU003_CHGTSD_IRQ] = S2MU003_CHGTSD_IRQ_NAME,
	[S2MU003_CHGVINVR_IRQ] = S2MU003_CHGVINVR_IRQ_NAME,
	[S2MU003_CHGTR_IRQ] = S2MU003_CHGTR_IRQ_NAME,
	[S2MU003_TMROUT_IRQ] = S2MU003_TMROUT_IRQ_NAME,
	[S2MU003_RECHG_IRQ] = S2MU003_RECHG_IRQ_NAME,
	[S2MU003_CHGTERM_IRQ] = S2MU003_CHGTERM_IRQ_NAME,
	[S2MU003_BATOVP_IRQ] = S2MU003_BATOVP_IRQ_NAME,
	[S2MU003_CHGVIN_IRQ] = S2MU003_CHGVIN_IRQ_NAME,
	[S2MU003_CIN2VAT_IRQ] = S2MU003_CIN2VAT_IRQ_NAME,
	[S2MU003_CHGSTS_IRQ] = S2MU003_CHGSTS_IRQ_NAME,
	[S2MU003_OTGILIM_IRQ] = S2MU003_OTGILIM_IRQ_NAME,
	[S2MU003_BSTINLV_IRQ] = S2MU003_BSTINLV_IRQ_NAME,
	[S2MU003_BSTILIM_IRQ] = S2MU003_BSTILIM_IRQ_NAME,
	[S2MU003_VMIDOVP_IRQ] = S2MU003_VMIDOVP_IRQ_NAME,
	[S2MU003_LBPROT_IRQ] = S2MU003_LBPROT_IRQ_NAME,
	[S2MU003_OPEN_CH2_IRQ] = S2MU003_OPEN_CH2_IRQ_NAME,
	[S2MU003_OPEN_CH1_IRQ] = S2MU003_OPEN_CH1_IRQ_NAME,
	[S2MU003_SHORT_CH2_IRQ] = S2MU003_SHORT_CH2_IRQ_NAME,
	[S2MU003_SHORT_CH1_IRQ] = S2MU003_SHORT_CH1_IRQ_NAME,
	[S2MU003_WDT_IRQ] = S2MU003_WDT_IRQ_NAME,
	[S2MU003_TSD_IRQ] = S2MU003_TSD_IRQ_NAME,
	[S2MU003_CPWRLV_IRQ] = S2MU003_CPWRLV_IRQ_NAME,
};

const char *s2mu003_get_irq_name_by_index(int index)
{
	return s2mu003_irq_names[index];
}
EXPORT_SYMBOL(s2mu003_get_irq_name_by_index);

enum S2MU003_IRQ_OFFSET {
	S2MU003_CHG_IRQ_OFFSET = 0,
	S2MU003_CHG_IRQ1_OFFSET = 0,
	S2MU003_CHG_IRQ2_OFFSET,
	S2MU003_CHG_IRQ3_OFFSET,
	S2MU003_LED_IRQ_OFFSET,
	S2MU003_PMIC_IRQ_OFFSET,
};

struct s2mu003_irq_data {
	int mask;
	int offset;
};

static const u8 s2mu003_mask_reg[] = {
	S2MU003_CHG_IRQ1_CTRL,
	S2MU003_CHG_IRQ2_CTRL,
	S2MU003_CHG_IRQ3_CTRL,
	S2MU003_LED_IRQ_CTRL,
	S2MU003_PMIC_IRQ_CTRL,
};

static const struct s2mu003_irq_data s2mu003_irqs[] = {
	[S2MU003_EOC_IRQ] = {
		.offset = 0,
		.mask = 1 << 1,
	},
	[S2MU003_CINIR_IRQ] = {
		.offset = 0,
		.mask = 1 << 2,
	},
	[S2MU003_BATP_IRQ] = {
		.offset = 0,
		.mask = 1 << 3,
	},
	[S2MU003_BATLV_IRQ] = {
		.offset = 0,
		.mask = 1 << 4,
	},
	[S2MU003_TOPOFF_IRQ] = {
		.offset = 0,
		.mask = 1 << 5,
	},
	[S2MU003_CINOVP_IRQ] = {
		.offset = 0,
		.mask = 1 << 6,
	},
	[S2MU003_CHGTSD_IRQ] = {
		.offset = 0,
		.mask = 1 << 7,
	},
	[S2MU003_CHGVINVR_IRQ] = {
		.offset = 1,
		.mask = 1 << 0,
	},
	[S2MU003_CHGTR_IRQ] = {
		.offset = 1,
		.mask = 1 << 1,
	},
	[S2MU003_TMROUT_IRQ] = {
		.offset = 1,
		.mask = 1 << 2,
	},
	[S2MU003_RECHG_IRQ] = {
		.offset = 1,
		.mask = 1 << 3,
	},
	[S2MU003_CHGTERM_IRQ] = {
		.offset = 1,
		.mask = 1 << 4,
	},
	[S2MU003_BATOVP_IRQ] = {
		.offset = 1,
		.mask = 1 << 5,
	},
	[S2MU003_CHGVIN_IRQ] = {
		.offset = 1,
		.mask = 1 << 6,
	},
	[S2MU003_CIN2VAT_IRQ] = {
		.offset = 1,
		.mask = 1 << 7,
	},
	[S2MU003_CHGSTS_IRQ] = {
		.offset = 2,
		.mask = 1 << 1,
	},
	[S2MU003_OTGILIM_IRQ] = {
		.offset = 2,
		.mask = 1 << 4,
	},
	[S2MU003_BSTINLV_IRQ] = {
		.offset = 2,
		.mask = 1 << 5,
	},
	[S2MU003_BSTILIM_IRQ] = {
		.offset = 2,
		.mask = 1 << 6,
	},
	[S2MU003_VMIDOVP_IRQ] = {
		.offset = 2,
		.mask = 1 << 7,
	},
	[S2MU003_LBPROT_IRQ] = {
		.offset = 0,
		.mask = 1 << 2,
	},
	[S2MU003_OPEN_CH2_IRQ] = {
		.offset = 0,
		.mask = 1 << 4,
	},
	[S2MU003_OPEN_CH1_IRQ] = {
		.offset = 0,
		.mask = 1 << 5,
	},
	[S2MU003_SHORT_CH2_IRQ] = {
		.offset = 0,
		.mask = 1 << 6,
	},
	[S2MU003_SHORT_CH1_IRQ] = {
		.offset = 0,
		.mask = 1 << 7,
	},
	[S2MU003_WDT_IRQ] = {
		.offset = 0,
		.mask = 1 << 0,
	},
	[S2MU003_TSD_IRQ] = {
		.offset = 0,
		.mask = 1 << 6,
	},
	[S2MU003_CPWRLV_IRQ] = {
		.offset = 0,
		.mask = 1 << 7,
	},
};


static void s2mu003_irq_lock(struct irq_data *data)
{
	s2mu003_mfd_chip_t *chip = irq_get_chip_data(data->irq);
	mutex_lock(&chip->irq_lock);
}

static void s2mu003_irq_sync_unlock(struct irq_data *data)
{
	s2mu003_mfd_chip_t *chip = irq_get_chip_data(data->irq);
	s2mu003_block_write_device(chip->i2c_client,
			S2MU003_CHG_IRQ_CTRL,
			S2MU003_CHG_IRQ_REGS_NR,
			chip->irq_masks_cache +
			S2MU003_CHG_IRQ_OFFSET);

	s2mu003_block_write_device(chip->i2c_client,
			S2MU003_LED_IRQ_CTRL,
			S2MU003_LED_IRQ_REGS_NR,
			chip->irq_masks_cache +
			S2MU003_LED_IRQ_OFFSET);

	s2mu003_block_write_device(chip->i2c_client,
			S2MU003_PMIC_IRQ_CTRL,
			S2MU003_PMIC_IRQ_REGS_NR,
			chip->irq_masks_cache +
			S2MU003_PMIC_IRQ_OFFSET);

	mutex_unlock(&chip->irq_lock);
}

static const inline struct s2mu003_irq_data *
	irq_to_s2mu003_irq(s2mu003_mfd_chip_t *chip, int irq)
{
	return &s2mu003_irqs[irq - chip->irq_base];
}

static void s2mu003_irq_mask(struct irq_data *data)
{
	s2mu003_mfd_chip_t *chip = irq_get_chip_data(data->irq);
	const struct s2mu003_irq_data *irq_data = irq_to_s2mu003_irq(chip,
			data->irq);

	chip->irq_masks_cache[irq_data->offset] |= irq_data->mask;
}


static void s2mu003_irq_unmask(struct irq_data *data)
{
	s2mu003_mfd_chip_t *chip = irq_get_chip_data(data->irq);
	const struct s2mu003_irq_data *irq_data = irq_to_s2mu003_irq(chip,
			data->irq);

	chip->irq_masks_cache[irq_data->offset] &= ~irq_data->mask;
}

static struct irq_chip s2mu003_irq_chip = {
	.name			= "s2mu003",
	.irq_bus_lock		= s2mu003_irq_lock,
	.irq_bus_sync_unlock	= s2mu003_irq_sync_unlock,
	.irq_mask		= s2mu003_irq_mask,
	.irq_unmask		= s2mu003_irq_unmask,
};

s2mu003_irq_status_t *s2mu003_get_irq_status(s2mu003_mfd_chip_t *mfd_chip,
		s2mu003_irq_status_sel_t sel)
{

	int index;

	switch (sel) {
	case S2MU003_PREV_STATUS:
		index = mfd_chip->irq_status_index^0x01;
		break;
	case S2MU003_NOW_STATUS:
	default:
		index = mfd_chip->irq_status_index;
	}

	return &mfd_chip->irq_status[index];
}
EXPORT_SYMBOL(s2mu003_get_irq_status);

static int s2mu003_read_irq_status(s2mu003_mfd_chip_t *chip)
{
	int ret;
	struct i2c_client *iic = chip->i2c_client;
	s2mu003_irq_status_t *now_irq_status;

	now_irq_status = s2mu003_get_irq_status(chip, S2MU003_NOW_STATUS);

	ret = s2mu003_block_read_device(iic, S2MU003_CHG_IRQ,
			sizeof(now_irq_status->chg_irq_status),
			now_irq_status->chg_irq_status);
	if (ret < 0) {
		dev_err(chip->dev,
				"Failed on reading CHG irq status\n");
		return ret;
	}

	printk("charger irq = 0x%x 0x%x 0x%x\n", (int)now_irq_status->chg_irq_status[0],
					(int)now_irq_status->chg_irq_status[1],
					now_irq_status->chg_irq_status[2]);
	ret = s2mu003_block_read_device(iic, S2MU003_LED_IRQ,
			sizeof(now_irq_status->fled_irq_status),
			now_irq_status->fled_irq_status);
	if (ret < 0) {
		dev_err(chip->dev,
				"Failed on reading FlashLED irq status\n");
		return ret;
	}
	printk("fled irq = 0x%x\n", (int)now_irq_status->fled_irq_status[0]);

	ret = s2mu003_block_read_device(iic, S2MU003_PMIC_IRQ,
			sizeof(now_irq_status->pmic_irq_status),
			now_irq_status->pmic_irq_status);
	if (ret < 0) {
		dev_err(chip->dev,
				"Failed on reading PMIC irq status\n");
		return ret;
	}
	printk("regulator irq = 0x%x\n", (int)now_irq_status->pmic_irq_status[0]);

	return 0;
}

static irqreturn_t s2mu003_irq_handler(int irq, void *data)
{
	int ret;
	int i;
	s2mu003_mfd_chip_t *chip = data;
	s2mu003_irq_status_t *status;

	printk("S2MU003 IRQ triggered\n");
	wake_lock_timeout(&chip->irq_wake_lock, msecs_to_jiffies(500));

	ret = s2mu003_reg_read(chip->i2c_client, 0x64);
	printk("S2MU003 IRQ 0x64 : %d\n", ret);

	ret = s2mu003_read_irq_status(chip);
	if (ret < 0) {
		pr_err("%s :Error : can't read irq status (%d)\n",
				__func__, ret);
		return IRQ_HANDLED;
	}
	status = s2mu003_get_irq_status(chip, S2MU003_NOW_STATUS);

	for (i = 0; i < S2MU003_IRQ_REGS_NR; i++)
		status->regs[i] &= ~chip->irq_masks_cache[i];

	for (i = 0; i < S2MU003_IRQS_NR; i++) {
		if (status->regs[s2mu003_irqs[i].offset] & s2mu003_irqs[i].mask) {
			pr_info("%s : Trigger IRQ %s, irq : %d \n",
				__func__, s2mu003_get_irq_name_by_index(i),
				chip->irq_base + i);

			handle_nested_irq(chip->irq_base + i);
		}
	}

	/* exchange irq index */
	chip->irq_status_index ^= 0x01;

	return IRQ_HANDLED;
}

static int s2mu003_irq_ctrl_regs[] = {
	S2MU003_CHG_IRQ1_CTRL,
	S2MU003_CHG_IRQ2_CTRL,
	S2MU003_CHG_IRQ3_CTRL,
	S2MU003_LED_IRQ_CTRL,
	S2MU003_PMIC_IRQ_CTRL,
};

static uint8_t s2mu003_irqs_ctrl_mask_all_val[] = {
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
};

static int s2mu003_mask_all_irqs(struct i2c_client *iic)
{
	int rc;
	int i;

	s2mu003_mfd_chip_t *chip = i2c_get_clientdata(iic);

	for (i = 0; i < ARRAY_SIZE(s2mu003_irq_ctrl_regs); i++) {
		rc = s2mu003_reg_write(iic, s2mu003_irq_ctrl_regs[i],
				s2mu003_irqs_ctrl_mask_all_val[i]);
		chip->irq_masks_cache[i] = s2mu003_irqs_ctrl_mask_all_val[i];
		if (rc < 0) {
			pr_info("Error : can't write reg[0x%x] = 0x%x\n",
					s2mu003_irq_ctrl_regs[i],
					s2mu003_irqs_ctrl_mask_all_val[i]);
			return rc;
		}
	}

	return 0;
}

static int s2mu003_irq_init_read(s2mu003_mfd_chip_t *chip)
{
	int ret;
	ret = s2mu003_read_irq_status(chip);
	chip->irq_status_index ^= 0x01;

	return ret;
}

int s2mu003_init_irq(s2mu003_mfd_chip_t *chip)
{
	int i, ret, curr_irq;
	ret = s2mu003_mask_all_irqs(chip->i2c_client);

	if (ret < 0) {
		pr_err("%s : Can't mask all irqs(%d)\n", __func__, ret);
		goto err_mask_all_irqs;
	}
	s2mu003_reg_write(chip->i2c_client, 0x64, 0xC3);

	s2mu003_irq_init_read(chip);
	mutex_init(&chip->irq_lock);

	/* Register with genirq */
	for (i = 0; i < S2MU003_IRQS_NR; i++) {
		curr_irq = i + chip->irq_base;
		irq_set_chip_data(curr_irq, chip);
		irq_set_chip_and_handler(curr_irq, &s2mu003_irq_chip,
				handle_simple_irq);
		irq_set_nested_thread(curr_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(curr_irq, IRQF_VALID);
#else
		irq_set_noprobe(curr_irq);
#endif
	}

	ret = gpio_request(chip->pdata->irq_gpio, "s2mu003_mfd_irq");
	if (ret < 0) {
		pr_err("%s : Request GPIO %d failed\n",
			__func__, (int)chip->pdata->irq_gpio);
		goto err_gpio_request;
	}

	ret = gpio_direction_input(chip->pdata->irq_gpio);
	if (ret < 0) {
		pr_err("Set GPIO direction to input : failed\n");
		goto err_set_gpio_input;
	}

	chip->irq = gpio_to_irq(chip->pdata->irq_gpio);
	ret = request_threaded_irq(chip->irq, NULL, s2mu003_irq_handler,
			/* IRQF_TRIGGER_FALLING */
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			"s2mu003", chip);
	if (ret < 0) {
		pr_err("%s : Failed : request IRQ (%d)\n", __func__, ret);
		goto err_request_irq;

	}

	ret = enable_irq_wake(chip->irq);
	if (ret < 0) {
		pr_info("%s : enable_irq_wake(%d) failed for (%d)\n",
				__func__, chip->irq, ret);
	}


	return ret;
err_request_irq:
err_set_gpio_input:
	gpio_free(chip->pdata->irq_gpio);
err_gpio_request:
	for (curr_irq = chip->irq_base;
			curr_irq < chip->irq_base + S2MU003_IRQS_NR;
			curr_irq++) {
#ifdef CONFIG_ARM
		set_irq_flags(curr_irq, 0);
#endif
		irq_set_chip_and_handler(curr_irq, NULL, NULL);
		irq_set_chip_data(curr_irq, NULL);
	}

	mutex_destroy(&chip->irq_lock);
err_mask_all_irqs:
	return ret;

}

int s2mu003_exit_irq(s2mu003_mfd_chip_t *chip)
{
	int curr_irq;

	for (curr_irq = chip->irq_base;
			curr_irq < chip->irq_base + S2MU003_IRQS_NR;
			curr_irq++) {
#ifdef CONFIG_ARM
		set_irq_flags(curr_irq, 0);
#endif
		irq_set_chip_and_handler(curr_irq, NULL, NULL);
		irq_set_chip_data(curr_irq, NULL);
	}

	if (chip->irq)
		free_irq(chip->irq, chip);
	mutex_destroy(&chip->irq_lock);
	return 0;
}
