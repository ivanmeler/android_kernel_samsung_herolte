/*
 * max77854-irq.c - Interrupt controller support for MAX77854
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Seoyoung Jeong <seo0.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * This driver is based on max77854-irq.c
 */

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/max77854.h>
#include <linux/mfd/max77854-private.h>

static void max77854_read_muic_registers(struct i2c_client *i2c)
{
	const enum max77854_muic_reg regfile[] = {
		MAX77854_MUIC_REG_ID,
		MAX77854_MUIC_REG_STATUS1,
		MAX77854_MUIC_REG_STATUS2,
		MAX77854_MUIC_REG_STATUS3,
		MAX77854_MUIC_REG_INTMASK1,
		MAX77854_MUIC_REG_INTMASK2,
		MAX77854_MUIC_REG_INTMASK3,
		MAX77854_MUIC_REG_CDETCTRL1,
		MAX77854_MUIC_REG_CDETCTRL2,
		MAX77854_MUIC_REG_CTRL1,
		MAX77854_MUIC_REG_CTRL2,
		MAX77854_MUIC_REG_CTRL3,
		MAX77854_MUIC_REG_CTRL4,
		MAX77854_MUIC_REG_HVCONTROL1,
		MAX77854_MUIC_REG_HVCONTROL2,
	};
	u8 val;
	int i;

	pr_info("%s: read register--------------\n", __func__);
	for (i = 0; i < ARRAY_SIZE(regfile); i++) {
		int ret = 0;
		ret = max77854_read_reg(i2c, regfile[i], &val);
		if (ret) {
			pr_err("%s: fail to read muic reg(0x%02x), ret=%d\n",
					__func__, regfile[i], ret);
			continue;
		}

		pr_info("%s: reg(0x%02x)=[0x%02x]\n", __func__,
				regfile[i], val);
	}
	pr_info("%s: end register---------------\n", __func__);
}

static const u8 max77854_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[SYS_INT] = MAX77854_PMIC_REG_SYSTEM_INT_MASK,
	[CHG_INT] = MAX77854_CHG_REG_INT_MASK,
	[MUIC_INT1] = MAX77854_MUIC_REG_INTMASK1,
	[MUIC_INT2] = MAX77854_MUIC_REG_INTMASK2,
	[MUIC_INT3] = MAX77854_MUIC_REG_INTMASK3,
};

static struct i2c_client *get_i2c(struct max77854_dev *max77854,
				enum max77854_irq_source src)
{
	switch (src) {
	case SYS_INT:
		return max77854->i2c;
	case FUEL_INT:
		return max77854->fuelgauge;
	case CHG_INT:
		return max77854->charger;
	case MUIC_INT1 ... MUIC_MAX_INT:
		return max77854->muic;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct max77854_irq_data {
	int mask;
	enum max77854_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct max77854_irq_data max77854_irqs[] = {
	DECLARE_IRQ(MAX77854_SYSTEM_IRQ_SYSUVLO_INT,	SYS_INT, 1 << 4),
	DECLARE_IRQ(MAX77854_SYSTEM_IRQ_SYSOVLO_INT,	SYS_INT, 1 << 5),
	DECLARE_IRQ(MAX77854_SYSTEM_IRQ_TSHDN_INT,	SYS_INT, 1 << 6),
	DECLARE_IRQ(MAX77854_SYSTEM_IRQ_TM_INT,		SYS_INT, 1 << 7),

	DECLARE_IRQ(MAX77854_CHG_IRQ_BYP_I,	CHG_INT, 1 << 0),
	DECLARE_IRQ(MAX77854_CHG_IRQ_BATP_I,	CHG_INT, 1 << 2),
	DECLARE_IRQ(MAX77854_CHG_IRQ_BAT_I,	CHG_INT, 1 << 3),
	DECLARE_IRQ(MAX77854_CHG_IRQ_CHG_I,	CHG_INT, 1 << 4),
	DECLARE_IRQ(MAX77854_CHG_IRQ_WCIN_I,	CHG_INT, 1 << 5),
	DECLARE_IRQ(MAX77854_CHG_IRQ_CHGIN_I,	CHG_INT, 1 << 6),
	DECLARE_IRQ(MAX77854_CHG_IRQ_AICL_I,	CHG_INT, 1 << 7),

	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT1_ADC,		MUIC_INT1, 1 << 0),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT1_ADCERR,	MUIC_INT1, 1 << 2),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT1_ADC1K,	MUIC_INT1, 1 << 3),

	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT2_CHGTYP,	MUIC_INT2, 1 << 0),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT2_CHGDETREUN,	MUIC_INT2, 1 << 1),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT2_DCDTMR,	MUIC_INT2, 1 << 2),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT2_DXOVP,	MUIC_INT2, 1 << 3),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT2_VBVOLT,	MUIC_INT2, 1 << 4),

	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT3_VBADC,	MUIC_INT3, 1 << 0),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT3_VDNMON,	MUIC_INT3, 1 << 1),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT3_DNRES,	MUIC_INT3, 1 << 2),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT3_MPNACK,	MUIC_INT3, 1 << 3),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT3_MRXBUFOW,	MUIC_INT3, 1 << 4),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_INT3_MRXTRF,	MUIC_INT3, 1 << 5),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_MRXPERR,		MUIC_INT3, 1 << 6),
	DECLARE_IRQ(MAX77854_MUIC_IRQ_MRXRDY,		MUIC_INT3, 1 << 7),

	DECLARE_IRQ(MAX77854_FG_IRQ_ALERT, FUEL_INT, 1 << 1),
};

static void max77854_irq_lock(struct irq_data *data)
{
	struct max77854_dev *max77854 = irq_get_chip_data(data->irq);

	mutex_lock(&max77854->irqlock);
}

static void max77854_irq_sync_unlock(struct irq_data *data)
{
	struct max77854_dev *max77854 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < MAX77854_IRQ_GROUP_NR; i++) {
		u8 mask_reg = max77854_mask_reg[i];
		struct i2c_client *i2c = get_i2c(max77854, i);

		if (mask_reg == MAX77854_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		max77854->irq_masks_cache[i] = max77854->irq_masks_cur[i];

		max77854_write_reg(i2c, max77854_mask_reg[i],
				max77854->irq_masks_cur[i]);
	}

	mutex_unlock(&max77854->irqlock);
}

static const inline struct max77854_irq_data *
irq_to_max77854_irq(struct max77854_dev *max77854, int irq)
{
	return &max77854_irqs[irq - max77854->irq_base];
}

static void max77854_irq_mask(struct irq_data *data)
{
	struct max77854_dev *max77854 = irq_get_chip_data(data->irq);
	const struct max77854_irq_data *irq_data =
	    irq_to_max77854_irq(max77854, data->irq);

	if (irq_data->group >= MAX77854_IRQ_GROUP_NR)
		return;

	if (irq_data->group >= MUIC_INT1 && irq_data->group <= MUIC_MAX_INT)
		max77854->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
	else
		max77854->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void max77854_irq_unmask(struct irq_data *data)
{
	struct max77854_dev *max77854 = irq_get_chip_data(data->irq);
	const struct max77854_irq_data *irq_data =
	    irq_to_max77854_irq(max77854, data->irq);

	if (irq_data->group >= MAX77854_IRQ_GROUP_NR)
		return;

	if (irq_data->group >= MUIC_INT1 && irq_data->group <= MUIC_MAX_INT)
		max77854->irq_masks_cur[irq_data->group] |= irq_data->mask;
	else
		max77854->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static void max77854_irq_disable(struct irq_data *data)
{
	max77854_irq_mask(data);
}

static struct irq_chip max77854_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= max77854_irq_lock,
	.irq_bus_sync_unlock	= max77854_irq_sync_unlock,
	.irq_mask		= max77854_irq_mask,
	.irq_unmask		= max77854_irq_unmask,
	.irq_disable	= max77854_irq_disable,
};

static irqreturn_t max77854_irq_thread(int irq, void *data)
{
	struct max77854_dev *max77854 = data;
	u8 irq_reg[MAX77854_IRQ_GROUP_NR] = {0};
	u8 tmp_irq_reg[MAX77854_IRQ_GROUP_NR] = {};
#if defined(CONFIG_MUIC_MAX77854_SUPPORT_AFC_CHARGER)
	u8 tmp_irq_mask_reg[MAX77854_IRQ_GROUP_NR] = {};
#endif /* CONFIG_MUIC_MAX77854_SUPPORT_AFC_CHARGER */
	static int check_num;
	u8 irq_src;
	int i, ret;

	pr_debug("%s: irq gpio pre-state(0x%02x)\n", __func__,
				gpio_get_value(max77854->irq_gpio));

	ret = max77854_read_reg(max77854->i2c,
					MAX77854_PMIC_REG_INTSRC, &irq_src);
	if (ret) {
		pr_err("%s:%s Failed to read interrupt source: %d\n",
			MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}

	pr_info("%s:%s: irq[%d] %d/%d/%d irq_src=0x%02x\n", MFD_DEV_NAME, __func__,
		irq, max77854->irq, max77854->irq_base, max77854->irq_gpio, irq_src);


	if (irq_src & MAX77854_IRQSRC_CHG) {
		/* CHG_INT */
		ret = max77854_read_reg(max77854->charger, MAX77854_CHG_REG_INT,
					&irq_reg[CHG_INT]);
		pr_debug("%s: charger interrupt(0x%02x)\n",
				__func__, irq_reg[CHG_INT]);
		/* mask chgin to prevent chgin infinite interrupt
		 * chgin is unmasked chgin isr
		 */
		if (irq_reg[CHG_INT] &
				max77854_irqs[MAX77854_CHG_IRQ_CHGIN_I].mask) {
			u8 reg_data;
			max77854_read_reg(max77854->charger,
				MAX77854_CHG_REG_INT_MASK, &reg_data);
			reg_data |= (1 << 6);
			max77854_write_reg(max77854->charger,
				MAX77854_CHG_REG_INT_MASK, reg_data);
		}
	}


	if (irq_src & MAX77854_IRQSRC_FG) {
		pr_debug("[%s] fuelgauge interrupt\n", __func__);
		pr_debug("[%s]IRQ_BASE(%d), NESTED_IRQ(%d)\n",
			__func__, max77854->irq_base, max77854->irq_base + MAX77854_FG_IRQ_ALERT);
		handle_nested_irq(max77854->irq_base + MAX77854_FG_IRQ_ALERT);
		return IRQ_HANDLED;
	}

	if (irq_src & MAX77854_IRQSRC_TOP) {
		/* SYS_INT */
		ret = max77854_read_reg(max77854->i2c, MAX77854_PMIC_REG_SYSTEM_INT,
				&irq_reg[SYS_INT]);
		pr_debug("%s: topsys interrupt(0x%02x)\n", __func__, irq_reg[SYS_INT]);
	}

	if (irq_src & MAX77854_IRQSRC_MUIC) {
		/* MUIC INT1 ~ INT3 */
		ret = max77854_bulk_read(max77854->muic, MAX77854_MUIC_REG_INT1,
				MAX77854_NUM_IRQ_MUIC_REGS, &tmp_irq_reg[MUIC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read interrupt source: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}

#if defined(CONFIG_MUIC_MAX77854_SUPPORT_AFC_CHARGER)
		max77854_bulk_read(max77854->muic, MAX77854_MUIC_REG_INTMASK1,
				MAX77854_NUM_IRQ_MUIC_REGS, &tmp_irq_mask_reg[MUIC_INT1]);

		for (i = MUIC_INT1; i < MAX77854_IRQ_GROUP_NR; i++)
			tmp_irq_reg[i] &= tmp_irq_mask_reg[i];
#endif /* CONFIG_MUIC_MAX77854_SUPPORT_AFC_CHARGER */

		/* Or temp irq register to irq register for if it retries */
		for (i = MUIC_INT1; i <= MUIC_MAX_INT; i++)
			irq_reg[i] |= tmp_irq_reg[i];

		pr_debug("%s: muic interrupt(0x%02x, 0x%02x, 0x%02x)\n", __func__,
			irq_reg[MUIC_INT1], irq_reg[MUIC_INT2], irq_reg[MUIC_INT3]);

		/* for debug */
		if ((irq_reg[MUIC_INT1] == 0) && (irq_reg[MUIC_INT2] == 0)
				&& (irq_reg[MUIC_INT3] == 0)) {
			pr_debug("%s: irq gpio post-state(0x%02x)\n", __func__,
				gpio_get_value(max77854->irq_gpio));
			if (check_num >= 15) {
				max77854_read_muic_registers(max77854->muic);
				panic("max77854 muic interrupt gpio Err!\n");
			}
			check_num++;
		} else
			check_num = 0;
	}

	/* Apply masking */
	for (i = 0; i < MAX77854_IRQ_GROUP_NR; i++) {
		if (i >= MUIC_INT1 && i <= MUIC_MAX_INT)
			irq_reg[i] &= max77854->irq_masks_cur[i];
		else
			irq_reg[i] &= ~max77854->irq_masks_cur[i];
	}

	/* Report */
	for (i = 0; i < MAX77854_IRQ_NR; i++) {
		if (irq_reg[max77854_irqs[i].group] & max77854_irqs[i].mask)
			handle_nested_irq(max77854->irq_base + i);
	}

	return IRQ_HANDLED;
}

int max77854_irq_init(struct max77854_dev *max77854)
{
	int i;
	int ret;
	u8 i2c_data;

	if (!max77854->irq_gpio) {
		dev_warn(max77854->dev, "No interrupt specified.\n");
		max77854->irq_base = 0;
		return 0;
	}

	if (!max77854->irq_base) {
		dev_err(max77854->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&max77854->irqlock);

	max77854->irq = gpio_to_irq(max77854->irq_gpio);
	pr_info("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
			max77854->irq, max77854->irq_gpio);

	ret = gpio_request(max77854->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(max77854->dev, "%s: failed requesting gpio %d\n",
			__func__, max77854->irq_gpio);
		return ret;
	}
	gpio_direction_input(max77854->irq_gpio);
	gpio_free(max77854->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < MAX77854_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;
		/* MUIC IRQ  0:MASK 1:NOT MASK */
		/* Other IRQ 1:MASK 0:NOT MASK */
		if (i >= MUIC_INT1 && i <= MUIC_INT3) {
			max77854->irq_masks_cur[i] = 0x00;
			max77854->irq_masks_cache[i] = 0x00;
		} else {
			max77854->irq_masks_cur[i] = 0xff;
			max77854->irq_masks_cache[i] = 0xff;
		}
		i2c = get_i2c(max77854, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (max77854_mask_reg[i] == MAX77854_REG_INVALID)
			continue;
		if (i >= MUIC_INT1 && i <= MUIC_INT3)
			max77854_write_reg(i2c, max77854_mask_reg[i], 0x00);
		else
			max77854_write_reg(i2c, max77854_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < MAX77854_IRQ_NR; i++) {
		int cur_irq;
		cur_irq = i + max77854->irq_base;
		irq_set_chip_data(cur_irq, max77854);
		irq_set_chip_and_handler(cur_irq, &max77854_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(max77854->irq, NULL, max77854_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "max77854-irq", max77854);
	if (ret) {
		dev_err(max77854->dev, "Failed to request IRQ %d: %d\n",
			max77854->irq, ret);
		return ret;
	}


	/* Unmask max77854 interrupt */
	ret = max77854_read_reg(max77854->i2c, MAX77854_PMIC_REG_INTSRC_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read muic reg\n", MFD_DEV_NAME, __func__);
		return ret;
	}

	i2c_data &= ~(MAX77854_IRQSRC_CHG);	/* Unmask charger interrupt */
	i2c_data &= ~(MAX77854_IRQSRC_FG);      /* Unmask fg interrupt */
	i2c_data &= ~(MAX77854_IRQSRC_MUIC);	/* Unmask muic interrupt */

	max77854_write_reg(max77854->i2c, MAX77854_PMIC_REG_INTSRC_MASK,
			   i2c_data);

	pr_info("%s:%s max77854_PMIC_REG_INTSRC_MASK=0x%02x\n",
			MFD_DEV_NAME, __func__, i2c_data);

	return 0;
}

void max77854_irq_exit(struct max77854_dev *max77854)
{
	if (max77854->irq)
		free_irq(max77854->irq, max77854);
}
