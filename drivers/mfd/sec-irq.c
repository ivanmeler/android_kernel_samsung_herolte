/*
 * sec-irq.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/regmap.h>

#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/irq.h>
#include <linux/mfd/samsung/s2mpu03.h>
#include <linux/mfd/samsung/s2mps16.h>
#include <linux/mfd/samsung/s2mps15.h>
#include <linux/mfd/samsung/s2mps13.h>
#include <linux/mfd/samsung/s2mps11.h>
#include <linux/mfd/samsung/s5m8763.h>
#include <linux/mfd/samsung/s5m8767.h>

#ifdef CONFIG_EXYNOS_MBOX
#include <soc/samsung/asv-exynos.h>
#endif

static struct regmap_irq s2mps15_irqs[] = {
	[S2MPS15_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S2MPS15_IRQ_PWRONF_MASK,
	},
	[S2MPS15_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S2MPS15_IRQ_PWRONR_MASK,
	},
	[S2MPS15_IRQ_JIGONBF] = {
		.reg_offset = 0,
		.mask = S2MPS15_IRQ_JIGONBF_MASK,
	},
	[S2MPS15_IRQ_JIGONBR] = {
		.reg_offset = 0,
		.mask = S2MPS15_IRQ_JIGONBR_MASK,
	},
	[S2MPS15_IRQ_ACOKBF] = {
		.reg_offset = 0,
		.mask = S2MPS15_IRQ_ACOKBF_MASK,
	},
	[S2MPS15_IRQ_ACOKBR] = {
		.reg_offset = 0,
		.mask = S2MPS15_IRQ_ACOKBR_MASK,
	},
	[S2MPS15_IRQ_PWRON1S] = {
		.reg_offset = 0,
		.mask = S2MPS15_IRQ_PWRON1S_MASK,
	},
	[S2MPS15_IRQ_MRB] = {
		.reg_offset = 0,
		.mask = S2MPS15_IRQ_MRB_MASK,
	},
	[S2MPS15_IRQ_RTC60S] = {
		.reg_offset = 1,
		.mask = S2MPS15_IRQ_RTC60S_MASK,
	},
	[S2MPS15_IRQ_RTCA1] = {
		.reg_offset = 1,
		.mask = S2MPS15_IRQ_RTCA1_MASK,
	},
	[S2MPS15_IRQ_RTCA0] = {
		.reg_offset = 1,
		.mask = S2MPS15_IRQ_RTCA0_MASK,
	},
	[S2MPS15_IRQ_SMPL] = {
		.reg_offset = 1,
		.mask = S2MPS15_IRQ_SMPL_MASK,
	},
	[S2MPS15_IRQ_RTC1S] = {
		.reg_offset = 1,
		.mask = S2MPS15_IRQ_RTC1S_MASK,
	},
	[S2MPS15_IRQ_WTSR] = {
		.reg_offset = 1,
		.mask = S2MPS15_IRQ_WTSR_MASK,
	},
	[S2MPS15_IRQ_WRSTB] = {
		.reg_offset = 1,
		.mask = S2MPS15_IRQ_WRSTB_MASK,
	},
	[S2MPS15_IRQ_INT120C] = {
		.reg_offset = 2,
		.mask = S2MPS15_IRQ_INT120C_MASK,
	},
	[S2MPS15_IRQ_INT140C] = {
		.reg_offset = 2,
		.mask = S2MPS15_IRQ_INT140C_MASK,
	},
	[S2MPS15_IRQ_TSD] = {
		.reg_offset = 2,
		.mask = S2MPS15_IRQ_TSD_MASK,
	},
	[S2MPS15_IRQ_OC0] = {
		.reg_offset = 2,
		.mask = S2MPS15_IRQ_OC0_MASK,
	},
	[S2MPS15_IRQ_OC1] = {
		.reg_offset = 2,
		.mask = S2MPS15_IRQ_OC1_MASK,
	},
	[S2MPS15_IRQ_OC2] = {
		.reg_offset = 2,
		.mask = S2MPS15_IRQ_OC2_MASK,
	},
	[S2MPS15_IRQ_OC3] = {
		.reg_offset = 2,
		.mask = S2MPS15_IRQ_OC3_MASK,
	},
	[S2MPS15_IRQ_ADCDONE] = {
		.reg_offset = 2,
		.mask = S2MPS15_IRQ_ADCDONE_MASK,
	},
};

static struct regmap_irq s2mps16_irqs[] = {
	[S2MPS16_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S2MPS16_IRQ_PWRONF_MASK,
	},
	[S2MPS16_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S2MPS16_IRQ_PWRONR_MASK,
	},
	[S2MPS16_IRQ_JIGONBF] = {
		.reg_offset = 0,
		.mask = S2MPS16_IRQ_JIGONBF_MASK,
	},
	[S2MPS16_IRQ_JIGONBR] = {
		.reg_offset = 0,
		.mask = S2MPS16_IRQ_JIGONBR_MASK,
	},
	[S2MPS16_IRQ_ACOKBF] = {
		.reg_offset = 0,
		.mask = S2MPS16_IRQ_ACOKBF_MASK,
	},
	[S2MPS16_IRQ_ACOKBR] = {
		.reg_offset = 0,
		.mask = S2MPS16_IRQ_ACOKBR_MASK,
	},
	[S2MPS16_IRQ_PWRON1S] = {
		.reg_offset = 0,
		.mask = S2MPS16_IRQ_PWRON1S_MASK,
	},
	[S2MPS16_IRQ_MRB] = {
		.reg_offset = 0,
		.mask = S2MPS16_IRQ_MRB_MASK,
	},
	[S2MPS16_IRQ_RTC60S] = {
		.reg_offset = 1,
		.mask = S2MPS16_IRQ_RTC60S_MASK,
	},
	[S2MPS16_IRQ_RTCA1] = {
		.reg_offset = 1,
		.mask = S2MPS16_IRQ_RTCA1_MASK,
	},
	[S2MPS16_IRQ_RTCA0] = {
		.reg_offset = 1,
		.mask = S2MPS16_IRQ_RTCA0_MASK,
	},
	[S2MPS16_IRQ_SMPL] = {
		.reg_offset = 1,
		.mask = S2MPS16_IRQ_SMPL_MASK,
	},
	[S2MPS16_IRQ_RTC1S] = {
		.reg_offset = 1,
		.mask = S2MPS16_IRQ_RTC1S_MASK,
	},
	[S2MPS16_IRQ_WTSR] = {
		.reg_offset = 1,
		.mask = S2MPS16_IRQ_WTSR_MASK,
	},
	[S2MPS16_IRQ_WRSTB] = {
		.reg_offset = 1,
		.mask = S2MPS16_IRQ_WRSTB_MASK,
	},
	[S2MPS16_IRQ_INT120C] = {
		.reg_offset = 2,
		.mask = S2MPS16_IRQ_INT120C_MASK,
	},
	[S2MPS16_IRQ_INT140C] = {
		.reg_offset = 2,
		.mask = S2MPS16_IRQ_INT140C_MASK,
	},
	[S2MPS16_IRQ_TSD] = {
		.reg_offset = 2,
		.mask = S2MPS16_IRQ_TSD_MASK,
	},
	[S2MPS16_IRQ_ADCDONE] = {
		.reg_offset = 2,
		.mask = S2MPS16_IRQ_ADCDONE_MASK,
	},
	[S2MPS16_IRQ_OC0] = {
		.reg_offset = 3,
		.mask = S2MPS16_IRQ_OC0_MASK,
	},
	[S2MPS16_IRQ_OC1] = {
		.reg_offset = 3,
		.mask = S2MPS16_IRQ_OC1_MASK,
	},
	[S2MPS16_IRQ_OC2] = {
		.reg_offset = 3,
		.mask = S2MPS16_IRQ_OC2_MASK,
	},
	[S2MPS16_IRQ_OC3] = {
		.reg_offset = 3,
		.mask = S2MPS16_IRQ_OC3_MASK,
	},
	[S2MPS16_IRQ_OC4] = {
		.reg_offset = 3,
		.mask = S2MPS16_IRQ_OC4_MASK,
	},
	[S2MPS16_IRQ_OC5] = {
		.reg_offset = 3,
		.mask = S2MPS16_IRQ_OC5_MASK,
	},
	[S2MPS16_IRQ_OC6] = {
		.reg_offset = 3,
		.mask = S2MPS16_IRQ_OC6_MASK,
	},
	[S2MPS16_IRQ_OC7] = {
		.reg_offset = 3,
		.mask = S2MPS16_IRQ_OC7_MASK,
	},
};

#ifndef CONFIG_EXYNOS_MBOX
static struct regmap_irq s2mpu03_irqs[] = {
	[S2MPU03_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S2MPU03_IRQ_PWRONF_MASK,
	},
	[S2MPU03_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S2MPU03_IRQ_PWRONR_MASK,
	},
	[S2MPU03_IRQ_JIGONBF] = {
		.reg_offset = 0,
		.mask = S2MPU03_IRQ_JIGONBF_MASK,
	},
	[S2MPU03_IRQ_JIGONBR] = {
		.reg_offset = 0,
		.mask = S2MPU03_IRQ_JIGONBR_MASK,
	},
	[S2MPU03_IRQ_ACOKF] = {
		.reg_offset = 0,
		.mask = S2MPU03_IRQ_ACOKF_MASK,
	},
	[S2MPU03_IRQ_ACOKR] = {
		.reg_offset = 0,
		.mask = S2MPU03_IRQ_ACOKR_MASK,
	},
	[S2MPU03_IRQ_PWRON1S] = {
		.reg_offset = 0,
		.mask = S2MPU03_IRQ_PWRON1S_MASK,
	},
	[S2MPU03_IRQ_MRB] = {
		.reg_offset = 0,
		.mask = S2MPU03_IRQ_MRB_MASK,
	},
	[S2MPU03_IRQ_RTC60S] = {
		.reg_offset = 1,
		.mask = S2MPU03_IRQ_RTC60S_MASK,
	},
	[S2MPU03_IRQ_RTCA1] = {
		.reg_offset = 1,
		.mask = S2MPU03_IRQ_RTCA1_MASK,
	},
	[S2MPU03_IRQ_RTCA0] = {
		.reg_offset = 1,
		.mask = S2MPU03_IRQ_RTCA0_MASK,
	},
	[S2MPU03_IRQ_SMPL] = {
		.reg_offset = 1,
		.mask = S2MPU03_IRQ_SMPL_MASK,
	},
	[S2MPU03_IRQ_RTC1S] = {
		.reg_offset = 1,
		.mask = S2MPU03_IRQ_RTC1S_MASK,
	},
	[S2MPU03_IRQ_WTSR] = {
		.reg_offset = 1,
		.mask = S2MPU03_IRQ_WTSR_MASK,
	},
	[S2MPU03_IRQ_INT120C] = {
		.reg_offset = 2,
		.mask = S2MPU03_IRQ_INT120C_MASK,
	},
	[S2MPU03_IRQ_INT140C] = {
		.reg_offset = 2,
		.mask = S2MPU03_IRQ_INT140C_MASK,
	},
	[S2MPU03_IRQ_TSD] = {
		.reg_offset = 2,
		.mask = S2MPU03_IRQ_TSD_MASK,
	},
};

static struct regmap_irq s2mps13_irqs[] = {
	[S2MPS13_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S2MPS13_IRQ_PWRONF_MASK,
	},
	[S2MPS13_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S2MPS13_IRQ_PWRONR_MASK,
	},
	[S2MPS13_IRQ_JIGONBF] = {
		.reg_offset = 0,
		.mask = S2MPS13_IRQ_JIGONBF_MASK,
	},
	[S2MPS13_IRQ_JIGONBR] = {
		.reg_offset = 0,
		.mask = S2MPS13_IRQ_JIGONBR_MASK,
	},
	[S2MPS13_IRQ_ACOKBF] = {
		.reg_offset = 0,
		.mask = S2MPS13_IRQ_ACOKBF_MASK,
	},
	[S2MPS13_IRQ_ACOKBR] = {
		.reg_offset = 0,
		.mask = S2MPS13_IRQ_ACOKBR_MASK,
	},
	[S2MPS13_IRQ_PWRON1S] = {
		.reg_offset = 0,
		.mask = S2MPS13_IRQ_PWRON1S_MASK,
	},
	[S2MPS13_IRQ_MRB] = {
		.reg_offset = 0,
		.mask = S2MPS13_IRQ_MRB_MASK,
	},
	[S2MPS13_IRQ_RTC60S] = {
		.reg_offset = 1,
		.mask = S2MPS13_IRQ_RTC60S_MASK,
	},
	[S2MPS13_IRQ_RTCA1] = {
		.reg_offset = 1,
		.mask = S2MPS13_IRQ_RTCA1_MASK,
	},
	[S2MPS13_IRQ_RTCA0] = {
		.reg_offset = 1,
		.mask = S2MPS13_IRQ_RTCA0_MASK,
	},
	[S2MPS13_IRQ_SMPL] = {
		.reg_offset = 1,
		.mask = S2MPS13_IRQ_SMPL_MASK,
	},
	[S2MPS13_IRQ_RTC1S] = {
		.reg_offset = 1,
		.mask = S2MPS13_IRQ_RTC1S_MASK,
	},
	[S2MPS13_IRQ_WTSR] = {
		.reg_offset = 1,
		.mask = S2MPS13_IRQ_WTSR_MASK,
	},
	[S2MPS13_IRQ_INT120C] = {
		.reg_offset = 2,
		.mask = S2MPS13_IRQ_INT120C_MASK,
	},
	[S2MPS13_IRQ_INT140C] = {
		.reg_offset = 2,
		.mask = S2MPS13_IRQ_INT140C_MASK,
	},
	[S2MPS13_IRQ_TSD] = {
		.reg_offset = 2,
		.mask = S2MPS13_IRQ_TSD_MASK,
	},
};

static struct regmap_irq s2mps11_irqs[] = {
	[S2MPS11_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRONF_MASK,
	},
	[S2MPS11_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRONR_MASK,
	},
	[S2MPS11_IRQ_JIGONBF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_JIGONBF_MASK,
	},
	[S2MPS11_IRQ_JIGONBR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_JIGONBR_MASK,
	},
	[S2MPS11_IRQ_ACOKBF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_ACOKBF_MASK,
	},
	[S2MPS11_IRQ_ACOKBR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_ACOKBR_MASK,
	},
	[S2MPS11_IRQ_PWRON1S] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRON1S_MASK,
	},
	[S2MPS11_IRQ_MRB] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_MRB_MASK,
	},
	[S2MPS11_IRQ_RTC60S] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTC60S_MASK,
	},
	[S2MPS11_IRQ_RTCA1] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTCA1_MASK,
	},
	[S2MPS11_IRQ_RTCA0] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTCA0_MASK,
	},
	[S2MPS11_IRQ_SMPL] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_SMPL_MASK,
	},
	[S2MPS11_IRQ_RTC1S] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTC1S_MASK,
	},
	[S2MPS11_IRQ_WTSR] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_WTSR_MASK,
	},
	[S2MPS11_IRQ_INT120C] = {
		.reg_offset = 2,
		.mask = S2MPS11_IRQ_INT120C_MASK,
	},
	[S2MPS11_IRQ_INT140C] = {
		.reg_offset = 2,
		.mask = S2MPS11_IRQ_INT140C_MASK,
	},
};

static struct regmap_irq s5m8767_irqs[] = {
	[S5M8767_IRQ_PWRR] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_PWRR_MASK,
	},
	[S5M8767_IRQ_PWRF] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_PWRF_MASK,
	},
	[S5M8767_IRQ_PWR1S] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_PWR1S_MASK,
	},
	[S5M8767_IRQ_JIGR] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_JIGR_MASK,
	},
	[S5M8767_IRQ_JIGF] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_JIGF_MASK,
	},
	[S5M8767_IRQ_LOWBAT2] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_LOWBAT2_MASK,
	},
	[S5M8767_IRQ_LOWBAT1] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_LOWBAT1_MASK,
	},
	[S5M8767_IRQ_MRB] = {
		.reg_offset = 1,
		.mask = S5M8767_IRQ_MRB_MASK,
	},
	[S5M8767_IRQ_DVSOK2] = {
		.reg_offset = 1,
		.mask = S5M8767_IRQ_DVSOK2_MASK,
	},
	[S5M8767_IRQ_DVSOK3] = {
		.reg_offset = 1,
		.mask = S5M8767_IRQ_DVSOK3_MASK,
	},
	[S5M8767_IRQ_DVSOK4] = {
		.reg_offset = 1,
		.mask = S5M8767_IRQ_DVSOK4_MASK,
	},
	[S5M8767_IRQ_RTC60S] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_RTC60S_MASK,
	},
	[S5M8767_IRQ_RTCA1] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_RTCA1_MASK,
	},
	[S5M8767_IRQ_RTCA2] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_RTCA2_MASK,
	},
	[S5M8767_IRQ_SMPL] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_SMPL_MASK,
	},
	[S5M8767_IRQ_RTC1S] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_RTC1S_MASK,
	},
	[S5M8767_IRQ_WTSR] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_WTSR_MASK,
	},
};

static struct regmap_irq s5m8763_irqs[] = {
	[S5M8763_IRQ_DCINF] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_DCINF_MASK,
	},
	[S5M8763_IRQ_DCINR] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_DCINR_MASK,
	},
	[S5M8763_IRQ_JIGF] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_JIGF_MASK,
	},
	[S5M8763_IRQ_JIGR] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_JIGR_MASK,
	},
	[S5M8763_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_PWRONF_MASK,
	},
	[S5M8763_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_PWRONR_MASK,
	},
	[S5M8763_IRQ_WTSREVNT] = {
		.reg_offset = 1,
		.mask = S5M8763_IRQ_WTSREVNT_MASK,
	},
	[S5M8763_IRQ_SMPLEVNT] = {
		.reg_offset = 1,
		.mask = S5M8763_IRQ_SMPLEVNT_MASK,
	},
	[S5M8763_IRQ_ALARM1] = {
		.reg_offset = 1,
		.mask = S5M8763_IRQ_ALARM1_MASK,
	},
	[S5M8763_IRQ_ALARM0] = {
		.reg_offset = 1,
		.mask = S5M8763_IRQ_ALARM0_MASK,
	},
	[S5M8763_IRQ_ONKEY1S] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_ONKEY1S_MASK,
	},
	[S5M8763_IRQ_TOPOFFR] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_TOPOFFR_MASK,
	},
	[S5M8763_IRQ_DCINOVPR] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_DCINOVPR_MASK,
	},
	[S5M8763_IRQ_CHGRSTF] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_CHGRSTF_MASK,
	},
	[S5M8763_IRQ_DONER] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_DONER_MASK,
	},
	[S5M8763_IRQ_CHGFAULT] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_CHGFAULT_MASK,
	},
	[S5M8763_IRQ_LOBAT1] = {
		.reg_offset = 3,
		.mask = S5M8763_IRQ_LOBAT1_MASK,
	},
	[S5M8763_IRQ_LOBAT2] = {
		.reg_offset = 3,
		.mask = S5M8763_IRQ_LOBAT2_MASK,
	},
};
#endif

#ifdef CONFIG_EXYNOS_MBOX

static inline struct regmap_irq *
irq_to_sec_pmic_irq(struct sec_pmic_dev *sec_pmic, int irq)
{
	switch (sec_pmic->device_type) {
	case S2MPS15X:
		return &s2mps15_irqs[irq - sec_pmic->irq_base];
	case S2MPS16X:
		return &s2mps16_irqs[irq - sec_pmic->irq_base];
	default:
		return NULL;
	}
}

static void sec_pmic_irq_lock(struct irq_data *data)
{
	struct sec_pmic_dev *sec_pmic = irq_data_get_irq_chip_data(data);

	mutex_lock(&sec_pmic->irqlock);
}

static void sec_pmic_irq_sync_unlock(struct irq_data *data)
{
	struct sec_pmic_dev *sec_pmic = irq_data_get_irq_chip_data(data);
	int i;
	int reg_int1m = 0;

	switch (sec_pmic->device_type) {
	case S2MPS15X:
		reg_int1m = S2MPS15_REG_INT1M;
		break;
	case S2MPS16X:
		reg_int1m = S2MPS16_REG_INT1M;
		break;
	default:
		dev_err(sec_pmic->dev,
			"Unknown device type %d\n",
			sec_pmic->device_type);

		mutex_unlock(&sec_pmic->irqlock);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(sec_pmic->irq_masks_cur); i++) {
		if (sec_pmic->irq_masks_cur[i] != sec_pmic->irq_masks_cache[i]) {
			sec_pmic->irq_masks_cache[i] = sec_pmic->irq_masks_cur[i];
			sec_reg_write(sec_pmic, reg_int1m + i,
					sec_pmic->irq_masks_cur[i]);
		}
	}

	mutex_unlock(&sec_pmic->irqlock);
}

static void sec_pmic_irq_unmask(struct irq_data *data)
{
	struct sec_pmic_dev *sec_pmic = irq_data_get_irq_chip_data(data);
	struct regmap_irq *irq_data = irq_to_sec_pmic_irq(sec_pmic,
							       data->irq);

	sec_pmic->irq_masks_cur[irq_data->reg_offset] &= ~irq_data->mask;
}

static void sec_pmic_irq_mask(struct irq_data *data)
{
	struct sec_pmic_dev *sec_pmic = irq_data_get_irq_chip_data(data);
	struct regmap_irq *irq_data = irq_to_sec_pmic_irq(sec_pmic,
							       data->irq);

	sec_pmic->irq_masks_cur[irq_data->reg_offset] |= irq_data->mask;
}

static void sec_pmic_irq_ack(struct irq_data *data)
{

}

static struct irq_chip sec_pmic_irq_chip = {
	.name = "sec-pmic",
	.irq_bus_lock = sec_pmic_irq_lock,
	.irq_bus_sync_unlock = sec_pmic_irq_sync_unlock,
	.irq_mask = sec_pmic_irq_mask,
	.irq_unmask = sec_pmic_irq_unmask,
	.irq_ack = sec_pmic_irq_ack,
};

static irqreturn_t sec_pmic_irq_thread(int irq, void *data)
{

	struct sec_pmic_dev *sec_pmic = data;
	u8 irq_reg[NUM_IRQ_REGS-1];
	int ret;
	int i, reg_int1;

	switch (sec_pmic->device_type) {
	case S2MPS15X:
		reg_int1 = S2MPS15_REG_INT1;
		break;
	case S2MPS16X:
		reg_int1 = S2MPS16_REG_INT1;
		break;
	default:
		dev_err(sec_pmic->dev,
			"Unknown device type %d\n",
			sec_pmic->device_type);
		return -EINVAL;
	}

	ret = sec_bulk_read(sec_pmic, reg_int1,
				NUM_IRQ_REGS - 1, irq_reg);
	if (ret < 0) {
		dev_err(sec_pmic->dev, "Failed to read interrupt register: %d\n",
				ret);
		return IRQ_NONE;
	}

	for (i = 0; i < NUM_IRQ_REGS - 1; i++)
		irq_reg[i] &= ~sec_pmic->irq_masks_cur[i];

	switch (sec_pmic->device_type) {
	case S2MPS15X:
		for (i = 0; i < S2MPS15_IRQ_NR; i++) {
			if (irq_reg[s2mps15_irqs[i].reg_offset] & s2mps15_irqs[i].mask)
				handle_nested_irq(sec_pmic->irq_base + i);
		}
		break;
	case S2MPS16X:
		for (i = 0; i < S2MPS16_IRQ_NR; i++) {
			if (irq_reg[s2mps16_irqs[i].reg_offset] & s2mps16_irqs[i].mask)
				handle_nested_irq(sec_pmic->irq_base + i);
		}
		break;
	default:
		dev_err(sec_pmic->dev,
			"Unknown device type %d\n",
			sec_pmic->device_type);
		return -EINVAL;
	}

	return IRQ_HANDLED;
}

int s5m_irq_resume(struct sec_pmic_dev *sec_pmic)
{
	if (sec_pmic->irq && sec_pmic->irq_base)
			sec_pmic_irq_thread(sec_pmic->irq_base, sec_pmic);
	return 0;
}

int sec_irq_init(struct sec_pmic_dev *sec_pmic)
{
	int i, reg_int1m, reg_irq_nr;
	int cur_irq;
	int ret = 0;
	int type = sec_pmic->device_type;

	if (!sec_pmic->irq) {
		dev_warn(sec_pmic->dev,
			 "No interrupt specified, no interrupts\n");
		sec_pmic->irq_base = 0;
		return 0;
	}

	if (!sec_pmic->irq_base) {
		dev_err(sec_pmic->dev,
			"No interrupt base specified, no interrupts\n");
		return 0;
	}

	mutex_init(&sec_pmic->irqlock);

	switch (type) {
	case S2MPS15X:
		reg_int1m = S2MPS15_REG_INT1M;
		reg_irq_nr = S2MPS15_IRQ_NR;
		break;
	case S2MPS16X:
		reg_int1m = S2MPS16_REG_INT1M;
		reg_irq_nr = S2MPS16_IRQ_NR;
		break;
	default:
		dev_err(sec_pmic->dev,
			"Unknown device type %d\n",
			sec_pmic->device_type);
		return -EINVAL;
	}

	for (i = 0; i < NUM_IRQ_REGS - 1; i++) {
		sec_pmic->irq_masks_cur[i] = 0xff;
		sec_pmic->irq_masks_cache[i] = 0xff;
		sec_reg_write(sec_pmic, reg_int1m + i, 0xff);
	}

	sec_pmic->irq_base = irq_alloc_descs(sec_pmic->irq_base, 0, reg_irq_nr, 0);
	if (sec_pmic->irq_base < 0) {
		dev_warn(sec_pmic->dev, "Failed to allcoate IRQs: %d\n",
						sec_pmic->irq_base);
			return sec_pmic->irq_base;
	}

	for (i = 0; i < reg_irq_nr; i++) {
		cur_irq = i + sec_pmic->irq_base;
		ret = irq_set_chip_data(cur_irq, sec_pmic);
		if (ret) {
			dev_err(sec_pmic->dev,
				"Failed to irq_set_chip_data %d: %d\n",
				sec_pmic->irq, ret);
			return ret;
		}

		irq_set_chip_and_handler(cur_irq, &sec_pmic_irq_chip,
					 handle_edge_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
			set_irq_flags(cur_irq, IRQF_VALID);
#else
			irq_set_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(sec_pmic->irq, NULL,
				   sec_pmic_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "sec-pmic-irq", sec_pmic);
	if (ret) {
		dev_err(sec_pmic->dev, "Failed to request IRQ %d: %d\n",
			sec_pmic->irq, ret);
		return ret;
	}

	return 0;
}

void sec_irq_exit(struct sec_pmic_dev *sec_pmic)
{
	if (sec_pmic->irq)
		free_irq(sec_pmic->irq, sec_pmic);
}

#else /* CONFIG_EXYNOS_MBOX */

static struct regmap_irq_chip s2mps16_irq_chip = {
	.name = "s2mps16",
	.irqs = s2mps16_irqs,
	.num_irqs = ARRAY_SIZE(s2mps16_irqs),
	.num_regs = 4,
	.status_base = S2MPS16_REG_INT1,
	.mask_base = S2MPS16_REG_INT1M,
	.ack_base = S2MPS16_REG_INT1,
};

static struct regmap_irq_chip s2mpu03_irq_chip = {
	.name = "s2mpu03",
	.irqs = s2mpu03_irqs,
	.num_irqs = ARRAY_SIZE(s2mpu03_irqs),
	.num_regs = 3,
	.status_base = S2MPU03_REG_INT1,
	.mask_base = S2MPU03_REG_INT1M,
	.ack_base = S2MPU03_REG_INT1,
};

static struct regmap_irq_chip s2mps15_irq_chip = {
	.name = "s2mps15",
	.irqs = s2mps15_irqs,
	.num_irqs = ARRAY_SIZE(s2mps15_irqs),
	.num_regs = 3,
	.status_base = S2MPS15_REG_INT1,
	.mask_base = S2MPS15_REG_INT1M,
	.ack_base = S2MPS15_REG_INT1,
};

static struct regmap_irq_chip s2mps13_irq_chip = {
	.name = "s2mps13",
	.irqs = s2mps13_irqs,
	.num_irqs = ARRAY_SIZE(s2mps13_irqs),
	.num_regs = 3,
	.status_base = S2MPS13_REG_INT1,
	.mask_base = S2MPS13_REG_INT1M,
	.ack_base = S2MPS13_REG_INT1,
};

static struct regmap_irq_chip s2mps11_irq_chip = {
	.name = "s2mps11",
	.irqs = s2mps11_irqs,
	.num_irqs = ARRAY_SIZE(s2mps11_irqs),
	.num_regs = 3,
	.status_base = S2MPS11_REG_INT1,
	.mask_base = S2MPS11_REG_INT1M,
	.ack_base = S2MPS11_REG_INT1,
};

static struct regmap_irq_chip s5m8767_irq_chip = {
	.name = "s5m8767",
	.irqs = s5m8767_irqs,
	.num_irqs = ARRAY_SIZE(s5m8767_irqs),
	.num_regs = 3,
	.status_base = S5M8767_REG_INT1,
	.mask_base = S5M8767_REG_INT1M,
	.ack_base = S5M8767_REG_INT1,
};

static struct regmap_irq_chip s5m8763_irq_chip = {
	.name = "s5m8763",
	.irqs = s5m8763_irqs,
	.num_irqs = ARRAY_SIZE(s5m8763_irqs),
	.num_regs = 4,
	.status_base = S5M8763_REG_IRQ1,
	.mask_base = S5M8763_REG_IRQM1,
	.ack_base = S5M8763_REG_IRQ1,
};

int sec_irq_init(struct sec_pmic_dev *sec_pmic)
{
	int ret = 0;
	int type = sec_pmic->device_type;

	if (!sec_pmic->irq) {
		dev_warn(sec_pmic->dev,
			 "No interrupt specified, no interrupts\n");
		sec_pmic->irq_base = 0;
		return 0;
	}

	switch (type) {
	case S5M8763X:
		ret = regmap_add_irq_chip(sec_pmic->regmap, sec_pmic->irq,
				  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				  sec_pmic->irq_base, &s5m8763_irq_chip,
				  &sec_pmic->irq_data);
		break;
	case S5M8767X:
		ret = regmap_add_irq_chip(sec_pmic->regmap, sec_pmic->irq,
				  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				  sec_pmic->irq_base, &s5m8767_irq_chip,
				  &sec_pmic->irq_data);
		break;
	case S2MPS11X:
		ret = regmap_add_irq_chip(sec_pmic->regmap, sec_pmic->irq,
				  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				  sec_pmic->irq_base, &s2mps11_irq_chip,
				  &sec_pmic->irq_data);
		break;
	case S2MPS13X:
		ret = regmap_add_irq_chip(sec_pmic->regmap, sec_pmic->irq,
				  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				  sec_pmic->irq_base, &s2mps13_irq_chip,
				  &sec_pmic->irq_data);
		break;
	case S2MPS15X:
		ret = regmap_add_irq_chip(sec_pmic->regmap, sec_pmic->irq,
				  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				  sec_pmic->irq_base, &s2mps15_irq_chip,
				  &sec_pmic->irq_data);
		break;
	case S2MPU03X:
		ret = regmap_add_irq_chip(sec_pmic->regmap, sec_pmic->irq,
				  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				  sec_pmic->irq_base, &s2mpu03_irq_chip,
				  &sec_pmic->irq_data);
		break;
	case S2MPS16X:
		ret = regmap_add_irq_chip(sec_pmic->regmap, sec_pmic->irq,
				  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				  sec_pmic->irq_base, &s2mps16_irq_chip,
				  &sec_pmic->irq_data);
		break;
	default:
		dev_err(sec_pmic->dev, "Unknown device type %d\n",
			sec_pmic->device_type);
		return -EINVAL;
	}

	if (ret != 0) {
		dev_err(sec_pmic->dev, "Failed to register IRQ chip: %d\n", ret);
		return ret;
	}

	return 0;
}

void sec_irq_exit(struct sec_pmic_dev *sec_pmic)
{
	regmap_del_irq_chip(sec_pmic->irq, sec_pmic->irq_data);
}

#endif
