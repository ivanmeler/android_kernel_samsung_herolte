/*
 * s2mps15.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/pinctrl-samsung.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mps15.h>
#include <mach/regs-pmu.h>
#include <mach/asv-exynos.h>
#include <asm/io.h>
#include <linux/mutex.h>

static struct s2mps15_info *static_info;
static struct regulator_desc regulators[][S2MPS15_REGULATOR_MAX];

struct s2mps15_info {
	struct regulator_dev *rdev[S2MPS15_REGULATOR_MAX];
	unsigned int opmode[S2MPS15_REGULATOR_MAX];
	int num_regulators;
	bool dvs_en;
	struct sec_pmic_dev *iodev;
	bool g3d_en;
	int g3d_pin;
	int dvs_pin;
	const char* g3d_en_addr;
	const char* g3d_en_pin;
	struct mutex lock;
	bool g3d_ocp;

	bool buck_dvs_on;
	bool buck2_sync;
	bool buck3_sync;
	int buck2_dvs;
	int buck3_dvs;
	int int_max;
	int int_vthr;
	int int_delta;
	int buck4_sel;
	int buck5_sel;
	unsigned int desc_type;
};

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret, id = rdev_get_id(rdev);

	switch (mode) {
	case SEC_OPMODE_SUSPEND:			/* ON in Standby Mode */
		val = 0x1 << S2MPS15_ENABLE_SHIFT;
		break;
	case SEC_OPMODE_ON:			/* ON in Normal Mode */
		val = 0x3 << S2MPS15_ENABLE_SHIFT;
		break;
	default:
		pr_warn("%s: regulator_suspend_mode : 0x%x not supported\n",
			rdev->desc->name, mode);
		return -EINVAL;
	}

	ret = sec_reg_update(s2mps15->iodev, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
	if (ret)
		return ret;

	s2mps15->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);

	int reg_id = rdev_get_id(rdev);

	/* disregard BUCK6 enable */
	if (reg_id == S2MPS15_BUCK6 && SEC_PMIC_REV(s2mps15->iodev)
		&& s2mps15->g3d_en && gpio_is_valid(s2mps15->g3d_pin))
		return 0;

	return sec_reg_update(s2mps15->iodev, rdev->desc->enable_reg,
				  s2mps15->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int val;

	/* disregard BUCK6 disable */
	if (reg_id == S2MPS15_BUCK6 && SEC_PMIC_REV(s2mps15->iodev)
		&& s2mps15->g3d_en && gpio_is_valid(s2mps15->g3d_pin))
		return 0;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return sec_reg_update(s2mps15->iodev, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int ret, reg_id = rdev_get_id(rdev);
	unsigned int val;
	/* BUCK6 is controlled by g3d gpio */
	if (reg_id == S2MPS15_BUCK6 && SEC_PMIC_REV(s2mps15->iodev)
		&& s2mps15->g3d_en && gpio_is_valid(s2mps15->g3d_pin)) {
		if ((__raw_readl(EXYNOS_PMU_G3D_STATUS) & LOCAL_PWR_CFG) == LOCAL_PWR_CFG)
			return 1;
		else
			return 0;
	} else {
		ret = sec_reg_read(s2mps15->iodev, rdev->desc->enable_reg, &val);
		if (ret)
			return ret;
	}

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
}

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static int s2m_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	int ramp_mask = 0x03;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay/1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPS15_BUCK1:
	case S2MPS15_BUCK2:
	case S2MPS15_BUCK9:
		ramp_shift = 6;
		break;
	case S2MPS15_BUCK3:
	case S2MPS15_BUCK5:
	case S2MPS15_BUCK10:
		ramp_shift = 4;
		break;
	case S2MPS15_BUCK4:
	case S2MPS15_BUCK7:
	case S2MPS15_BB1:
		ramp_shift = 2;
		break;
	case S2MPS15_BUCK6:
	case S2MPS15_BUCK8:
		ramp_shift = 0;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mps15->iodev, S2MPS15_REG_BUCK_RAMP1,
				  ramp_value << ramp_shift, ramp_mask << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int ret, reg_id = rdev_get_id(rdev);
	unsigned int val;
	/* if dvs pin set high, get the voltage on the diffrent register. */
	if (reg_id == S2MPS15_BUCK6 && SEC_PMIC_REV(s2mps15->iodev)
			&& s2mps15->dvs_en && s2m_get_dvs_is_on()) {
		ret = sec_reg_read(s2mps15->iodev, S2MPS15_REG_B6CTRL3, &val);
		if (ret)
			return ret;
	} else {
		ret = sec_reg_read(s2mps15->iodev, rdev->desc->vsel_reg, &val);
		if (ret)
			return ret;
	}

	 /* fix g3d ocp 6A : -31.25mV */
	 if (reg_id == S2MPS15_BUCK6 && s2mps15->g3d_ocp)
		 val -= 5;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int ret;

	ret = sec_reg_update(s2mps15->iodev, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = sec_reg_update(s2mps15->iodev, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_get_max_int_voltage(struct s2mps15_info *s2mps15)
{
	int buck4_val, buck5_val, int_max;

	buck4_val = s2mps15->buck4_sel
			* regulators[s2mps15->desc_type][S2MPS15_BUCK4].uV_step
			+ regulators[s2mps15->desc_type][S2MPS15_BUCK4].min_uV;
	buck5_val = s2mps15->buck5_sel
			* regulators[s2mps15->desc_type][S2MPS15_BUCK5].uV_step
			+ regulators[s2mps15->desc_type][S2MPS15_BUCK5].min_uV;
	int_max = buck4_val >= buck5_val ? buck4_val : buck5_val;
	int_max += s2mps15->int_delta;
	int_max = int_max >= s2mps15->int_vthr ? int_max : s2mps15->int_vthr;

	return int_max;
}

static int s2m_set_max_int_voltage(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int ret;
	int buck_val, int_max = 0;
	int reg_id = rdev_get_id(rdev);
	unsigned int old_sel_buck = 0, old_val_buck, old_val_ldo, buck_delay = 0;

	if (reg_id == S2MPS15_BUCK4) {
		old_sel_buck = s2mps15->buck4_sel;
		s2mps15->buck4_sel = sel;
	} else if (reg_id == S2MPS15_BUCK5) {
		old_sel_buck = s2mps15->buck5_sel;
		s2mps15->buck5_sel = sel;
	}

	old_val_buck = rdev->desc->min_uV + (rdev->desc->uV_step * old_sel_buck);
	old_val_ldo = s2mps15->int_max;

	buck_val = rdev->desc->min_uV + (rdev->desc->uV_step * sel);

	if (buck_val + s2mps15->int_delta > s2mps15->int_max) {
		ret = DIV_ROUND_UP(buck_val + s2mps15->int_delta -
		regulators[s2mps15->desc_type][S2MPS15_LDO8].min_uV,
		regulators[s2mps15->desc_type][S2MPS15_LDO8].uV_step);
		if (ret < 0)
			goto out;

		s2mps15->int_max = buck_val + s2mps15->int_delta;

		ret = sec_reg_update(s2mps15->iodev, S2MPS15_REG_L8CTRL,
								  ret, 0x3f);
		if (ret < 0)
			goto out;

		udelay(DIV_ROUND_UP((s2mps15->int_max - old_val_ldo), 12000));

		ret = sec_reg_write(s2mps15->iodev, rdev->desc->vsel_reg, sel);
		if (ret < 0)
			goto out;

	} else {
		if (buck_val < old_val_buck)
			buck_delay = DIV_ROUND_UP((old_val_buck - buck_val),
					rdev->constraints->ramp_delay);
		else
			buck_delay = DIV_ROUND_UP((buck_val- old_val_buck),
					rdev->constraints->ramp_delay);

		ret = sec_reg_write(s2mps15->iodev, rdev->desc->vsel_reg, sel);
		if (ret < 0)
			goto out;

		udelay(buck_delay);

		int_max = s2m_get_max_int_voltage(s2mps15);
		if (s2mps15->int_max != int_max) {
			s2mps15->int_max = int_max;
			ret = DIV_ROUND_UP(int_max -
			regulators[s2mps15->desc_type][S2MPS15_LDO8].min_uV,
			regulators[s2mps15->desc_type][S2MPS15_LDO8].uV_step);
			if (ret < 0)
				goto out;

			ret = sec_reg_update(s2mps15->iodev, S2MPS15_REG_L8CTRL,
							  ret, 0x3f);
			if (ret < 0)
				goto out;

			udelay(DIV_ROUND_UP((old_val_ldo - s2mps15->int_max), 12000));
		}
	}
	return ret;
out:
	pr_warn("%s: failed to set the max int voltage\n", __func__);
	return ret;
}

static int s2m_set_fix_ldo_voltage(struct regulator_dev *rdev, bool fix_en)
{
	int ret, old_sel, old_val;
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);

	ret = sec_reg_read(s2mps15->iodev, rdev->desc->vsel_reg, &old_sel);
	if (ret < 0)
		goto out;
	old_val = rdev->desc->min_uV + (rdev->desc->uV_step * old_sel);

	ret = sec_reg_write(s2mps15->iodev, rdev->desc->vsel_reg, 0x7C);
	if (ret < 0)
		goto out;

	if (1075000 > old_val)
		udelay(DIV_ROUND_UP((1075000 - old_val), rdev->constraints->ramp_delay));

	if (reg_id == S2MPS15_BUCK2) {
		ret = sec_reg_update(s2mps15->iodev, S2MPS15_REG_LDO_DVS1,
					fix_en << 2, 0x1 << 2);
		if (ret < 0)
			goto out;

		s2mps15->buck2_sync = fix_en ? true : false;
	} else {
		ret = sec_reg_update(s2mps15->iodev, S2MPS15_REG_LDO_DVS3,
					fix_en << 2, 0x1 << 2);

		if (ret < 0)
			goto out;
		s2mps15->buck3_sync = fix_en ? true : false;
	}

	return ret;
out:
	pr_warn("%s: failed to fix_ldo_voltage\n", rdev->desc->name);
	return ret;
}

static int s2m_set_ldo_dvs_control(struct regulator_dev *rdev)
{
	int ret;
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int sram_vthr, sram_delta, asv_info;
	int vthr_mask, delta_mask;
	int vthr_val;
	int dvs_reg;

	asv_info = exynos_get_asv_info(0);
	switch (reg_id) {
	case S2MPS15_BUCK2:
		vthr_mask = 0x3;
		delta_mask = 0x3 << 2;
		sram_vthr = asv_info & vthr_mask;
		sram_delta = (asv_info & delta_mask) >> 2;
		if (sram_delta == 0x0)
			sram_delta = 0x1;
		s2mps15->buck2_dvs = sram_delta;
		dvs_reg = S2MPS15_REG_LDO_DVS1;
		break;
	case S2MPS15_BUCK3:
		vthr_mask = 0x3 << 4;
		delta_mask = 0x3 << 6;
		sram_vthr = (asv_info & vthr_mask) >> 4;
		sram_delta = (asv_info & delta_mask) >> 6;
		if (sram_delta == 0x0)
			sram_delta = 0x1;
		s2mps15->buck3_dvs = sram_delta;
		dvs_reg = S2MPS15_REG_LDO_DVS3;
		break;
	case S2MPS15_BUCK6:
		vthr_mask = 0x3 << 8;
		delta_mask = 0x3 << 10;
		sram_vthr = (asv_info & vthr_mask) >> 8;
		sram_delta = (asv_info & delta_mask) >> 10;
		dvs_reg = S2MPS15_REG_LDO_DVS4;
		break;
	case S2MPS15_LDO8:
		vthr_mask = 0x3 << 12;
		delta_mask = 0x3 << 14;
		sram_vthr = (asv_info & vthr_mask) >> 12;
		sram_delta = (asv_info & delta_mask) >> 14;
		break;
	default:
		dev_err(&rdev->dev, "%s, set invalid reg_id(%d)\n", __func__, reg_id);
		break;
	}

	switch (sram_vthr) {
	case 0x0:
		if (reg_id == S2MPS15_LDO8)
			s2mps15->int_vthr = 750000;
		else if (reg_id == S2MPS15_BUCK6)
			vthr_val = 0x3 << 5;
		else
			vthr_val = 0x4 << 5;
		break;
	case 0x1:
		if (reg_id == S2MPS15_LDO8)
			s2mps15->int_vthr = 800000;
		else if (reg_id == S2MPS15_BUCK6)
			vthr_val = 0x4 << 5;
		else
			vthr_val = 0x3 << 5;
		break;
	case 0x2:
		if (reg_id == S2MPS15_LDO8)
			s2mps15->int_vthr = 700000;
		else
			vthr_val = 0x2 << 5;
		break;
	case 0x3:
		if (reg_id == S2MPS15_LDO8)
			s2mps15->int_vthr = 850000;
		else if (reg_id == S2MPS15_BUCK2)
			vthr_val = 0x6 << 5;
		else if (reg_id == S2MPS15_BUCK3)
			vthr_val = 0x1 << 5;
		else if (reg_id == S2MPS15_BUCK6)
			vthr_val = 0x5 << 5;
		break;
	}

	if (reg_id == S2MPS15_LDO8) {
		if (sram_delta == 0)
			s2mps15->int_delta = 0;
		else
			s2mps15->int_delta = sram_delta * 25000 + 25000;
		return 0;
	}

	ret = sec_reg_update(s2mps15->iodev, dvs_reg,
			vthr_val | (sram_delta << 3), 0xf8);
	if (ret < 0)
		goto out;

	return ret;
out:
	pr_warn("%s: failed to ldo dvs control\n", rdev->desc->name);
	return ret;

}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev, unsigned sel)
{
	int ret;
	struct s2mps15_info *s2mps15 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	int buck_dvs, delta_val, new_val;
	bool buck_sync;

	/* fix g3d ocp 6A : +31.25mV */
	if (reg_id == S2MPS15_BUCK6 && s2mps15->g3d_ocp)
		sel += 5;

	/* If dvs_en = 0, dvs_pin = 1, occur BUG_ON */
	if (reg_id == S2MPS15_BUCK6 && SEC_PMIC_REV(s2mps15->iodev)
		&& !s2mps15->dvs_en && gpio_is_valid(s2mps15->dvs_pin)) {
		BUG_ON(s2m_get_dvs_is_on());
	}

	if ((reg_id == S2MPS15_BUCK2 || reg_id == S2MPS15_BUCK3) && s2mps15->buck_dvs_on) {
		mutex_lock(&s2mps15->lock);
		buck_dvs = (reg_id == S2MPS15_BUCK2) ? s2mps15->buck2_dvs : s2mps15->buck3_dvs;
		buck_sync = (reg_id == S2MPS15_BUCK2) ? s2mps15->buck2_sync : s2mps15->buck3_sync;

		if (buck_dvs == 0)
			delta_val = 0;
		else
			delta_val = (buck_dvs - 1) * 25000 + 50000;

		new_val = rdev->desc->min_uV + (rdev->desc->uV_step * sel);

		if (delta_val + new_val <= 1175000) {
			if (!buck_sync) {
				ret = s2m_set_fix_ldo_voltage(rdev, 1);
				if (ret < 0)
					goto out;
			}
		} else {
			if (buck_sync) {
				ret = s2m_set_fix_ldo_voltage(rdev, 0);
				if (ret < 0)
					goto out;
			}
		}

		ret = sec_reg_write(s2mps15->iodev, rdev->desc->vsel_reg, sel);
		if (ret < 0)
			goto out;

		mutex_unlock(&s2mps15->lock);
		return ret;
	}

	if (reg_id == S2MPS15_BUCK4 || reg_id == S2MPS15_BUCK5) {
		mutex_lock(&s2mps15->lock);
		ret = s2m_set_max_int_voltage(rdev, sel);
		if (ret < 0)
			goto out;
		mutex_unlock(&s2mps15->lock);
		return ret;
	}

	ret = sec_reg_write(s2mps15->iodev, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto i2c_out;

	if (rdev->desc->apply_bit)
		ret = sec_reg_update(s2mps15->iodev, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	mutex_unlock(&s2mps15->lock);
i2c_out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	unsigned int ramp_delay = 0;
	int old_volt, new_volt;

	if (rdev->constraints->ramp_delay)
		ramp_delay = rdev->constraints->ramp_delay;
	else if (rdev->desc->ramp_delay)
		ramp_delay = rdev->desc->ramp_delay;

	if (ramp_delay == 0) {
		pr_warn("%s: ramp_delay not set\n", rdev->desc->name);
		return -EINVAL;
	}

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, ramp_delay);

	return 0;
}

void s2m_init_dvs()
{
	unsigned int temp;

	temp = __raw_readl(EXYNOS_PMU_GPU_DVS_CTRL);
	__raw_writel(temp | G3D_DVS_ENABLE, EXYNOS_PMU_GPU_DVS_CTRL);

	temp = __raw_readl(EXYNOS_PMU_GPU_DVS_CTRL);
	__raw_writel(temp | G3D_DVS_POLARITY, EXYNOS_PMU_GPU_DVS_CTRL);

	temp = __raw_readl(EXYNOS_PMU_GPU_DVS_COUNTER);
	__raw_writel((temp & G3D_DVS_COUNTER_RESET) | G3D_DVS_COUNTER, EXYNOS_PMU_GPU_DVS_COUNTER);

	temp = __raw_readl(EXYNOS_PMU_GPU_DVS_CLK_CTRL);
	__raw_writel(temp | G3D_DVS_CG_ENABLE, EXYNOS_PMU_GPU_DVS_CLK_CTRL);
}

int s2m_get_dvs_is_enable()
{
	return (__raw_readl(EXYNOS_PMU_GPU_DVS_CTRL) & G3D_DVS_CTRL);
}
EXPORT_SYMBOL_GPL(s2m_get_dvs_is_enable);

int s2m_get_dvs_is_on()
{
	return !(__raw_readl(EXYNOS_PMU_GPU_DVS_STATUS) & G3D_DVS_STATUS);
}
EXPORT_SYMBOL_GPL(s2m_get_dvs_is_on);

int s2m_set_dvs_pin(bool gpio_val)
{
	unsigned int temp, status_temp, ret, val, val_dvs, count = 0;
	if (!SEC_PMIC_REV(static_info->iodev) || !static_info->dvs_en
		|| !gpio_is_valid(static_info->dvs_pin)) {
		pr_warn("%s: dvs pin ctrl failed\n", __func__);
		return -EINVAL;
	}

	/* wait for 90us, 130us when dvs pin control */
	if (gpio_val) {
		temp = __raw_readl(EXYNOS_PMU_GPU_DVS_CTRL);
		__raw_writel(temp | G3D_DVS_CTRL_ON, EXYNOS_PMU_GPU_DVS_CTRL);
		udelay(90);
	}
	else {
		do {
			temp = __raw_readl(EXYNOS_PMU_GPU_DVS_CTRL);
			__raw_writel((temp & ~G3D_DVS_CTRL) | G3D_DVS_CTRL_OFF, EXYNOS_PMU_GPU_DVS_CTRL);
			udelay(250);
			if (count > 10) {
				ret = sec_reg_read(static_info->iodev, S2MPS15_REG_B6CTRL2, &val);
				ret = sec_reg_read(static_info->iodev, S2MPS15_REG_B6CTRL3, &val_dvs);
				pr_warn("%s: dvs pin ctrl off failed G3D_DVS_CTRL[%X], GPIO value[%X], Buck6 vol[%d], DVS Voltage[%d]\n", __func__, temp, gpio_get_value(static_info->dvs_pin), val, val_dvs);
				return -EINVAL;
			}
			status_temp = __raw_readl(EXYNOS_PMU_GPU_DVS_STATUS);
			count++;
		} while (!(status_temp & G3D_DVS_STATUS));
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2m_set_dvs_pin);

void g3d_pin_config_set()
{
	if (!SEC_PMIC_REV(static_info->iodev) || !static_info->g3d_en
		|| !gpio_is_valid(static_info->g3d_pin))
		return;

	pin_config_set(static_info->g3d_en_addr, static_info->g3d_en_pin, PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
}
EXPORT_SYMBOL_GPL(g3d_pin_config_set);

static struct regulator_ops s2mps15_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
};

static struct regulator_ops s2mps15_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap_buck,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
	.set_ramp_delay		= s2m_set_ramp_delay,
};

#define _BUCK(macro)	S2MPS15_BUCK##macro
#define _buck_ops(num)	s2mps15_buck_ops##num

#define _LDO(macro)	S2MPS15_LDO##macro
#define _REG(ctrl)	S2MPS15_REG##ctrl
#define _ldo_ops(num)	s2mps15_ldo_ops##num
#define _TIME(macro)	S2MPS15_ENABLE_TIME##macro

#define BUCK_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPS15_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS15_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS15_ENABLE_MASK,			\
	.enable_time	= t					\
}

#define LDO_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPS15_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS15_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS15_ENABLE_MASK,			\
	.enable_time	= t					\
}

enum regulator_desc_type {
	S2MPS15_DESC_TYPE0 = 0,
	S2MPS15_DESC_TYPE1,
};

static struct regulator_desc regulators[][S2MPS15_REGULATOR_MAX] = {
	[S2MPS15_DESC_TYPE0] = {
			/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
		LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP1), _REG(_L1CTRL), _REG(_L1CTRL), _TIME(_LDO)),
		LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L2CTRL), _REG(_L2CTRL), _TIME(_LDO)),
		LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
		LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
		LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L5CTRL), _REG(_L5CTRL), _TIME(_LDO)),
		LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO)),
		LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),
		LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
		LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO)),
		LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), _LDO(_MIN1), _LDO(_STEP2), _REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO)),
		LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L11CTRL), _REG(_L11CTRL), _TIME(_LDO)),
		LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L12CTRL), _REG(_L12CTRL), _TIME(_LDO)),
		LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO)),
		LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO)),
		LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO)),
		LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L16CTRL), _REG(_L16CTRL), _TIME(_LDO)),
		LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L17CTRL), _REG(_L17CTRL), _TIME(_LDO)),
		LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L18CTRL), _REG(_L18CTRL), _TIME(_LDO)),
		LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L19CTRL), _REG(_L19CTRL), _TIME(_LDO)),
		LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L20CTRL), _REG(_L20CTRL), _TIME(_LDO)),
		LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L21CTRL), _REG(_L21CTRL), _TIME(_LDO)),
		LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO)),
		LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO)),
		LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L24CTRL), _REG(_L24CTRL), _TIME(_LDO)),
		LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L25CTRL), _REG(_L25CTRL), _TIME(_LDO)),
		LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L26CTRL), _REG(_L26CTRL), _TIME(_LDO)),
		LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L27CTRL), _REG(_L27CTRL), _TIME(_LDO)),
		BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B1CTRL2), _REG(_B1CTRL1), _TIME(_BUCK1)),
		BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B2CTRL2), _REG(_B2CTRL1), _TIME(_BUCK2)),
		BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B3CTRL2), _REG(_B3CTRL1), _TIME(_BUCK3)),
		BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B4CTRL2), _REG(_B4CTRL1), _TIME(_BUCK4)),
		BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B5CTRL2), _REG(_B5CTRL1), _TIME(_BUCK5)),
		BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B6CTRL2), _REG(_B6CTRL1), _TIME(_BUCK6)),
		BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), _BUCK(_MIN1), _BUCK(_STEP1), _REG(_B7CTRL2), _REG(_B7CTRL1), _TIME(_BUCK7)),
		BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2), _REG(_B8CTRL2), _REG(_B8CTRL1), _TIME(_BUCK8)),
		BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2), _REG(_B9CTRL2), _REG(_B9CTRL1), _TIME(_BUCK9)),
		BUCK_DESC("BUCK10", _BUCK(10), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2), _REG(_B10CTRL2), _REG(_B10CTRL1), _TIME(_BUCK10)),
		BUCK_DESC("BB", S2MPS15_BB1, &_buck_ops(), _BUCK(_MIN3), _BUCK(_STEP2), _REG(_BB1CTRL2), _REG(_BB1CTRL1), _TIME(_BB)),
	},
	[S2MPS15_DESC_TYPE1] = {
			/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
		LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN2), _LDO(_STEP1), _REG(_L1CTRL), _REG(_L1CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L2CTRL), _REG(_L2CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L5CTRL), _REG(_L5CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN1_REV1), _LDO(_STEP2), _REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN1_REV1), _LDO(_STEP2), _REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), _LDO(_MIN1_REV1), _LDO(_STEP2), _REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), _LDO(_MIN1_REV1), _LDO(_STEP2), _REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L11CTRL), _REG(_L11CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L12CTRL), _REG(_L12CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L16CTRL), _REG(_L16CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L17CTRL), _REG(_L17CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L18CTRL), _REG(_L18CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L19CTRL), _REG(_L19CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L20CTRL), _REG(_L20CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L21CTRL), _REG(_L21CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L24CTRL), _REG(_L24CTRL), _TIME(_LDO_REV1)),
		LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), _LDO(_MIN4), _LDO(_STEP2), _REG(_L25CTRL_REV1), _REG(_L25CTRL_REV1), _TIME(_LDO_REV1)),
		LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP1), _REG(_L26CTRL_REV1), _REG(_L26CTRL_REV1), _TIME(_LDO_REV1)),
		LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), _LDO(_MIN3), _LDO(_STEP2), _REG(_L27CTRL), _REG(_L27CTRL), _TIME(_LDO_REV1)),
		BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1_REV1), _BUCK(_STEP1), _REG(_B1CTRL2), _REG(_B1CTRL1), _TIME(_BUCK1_REV1)),
		BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), _BUCK(_MIN1_REV1), _BUCK(_STEP1), _REG(_B2CTRL2), _REG(_B2CTRL1), _TIME(_BUCK2_REV1)),
		BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN1_REV1), _BUCK(_STEP1), _REG(_B3CTRL2), _REG(_B3CTRL1), _TIME(_BUCK3_REV1)),
		BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), _BUCK(_MIN1_REV1), _BUCK(_STEP1), _REG(_B4CTRL2), _REG(_B4CTRL1), _TIME(_BUCK4_REV1)),
		BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), _BUCK(_MIN1_REV1), _BUCK(_STEP1), _REG(_B5CTRL2), _REG(_B5CTRL1), _TIME(_BUCK5_REV1)),
		BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), _BUCK(_MIN1_REV1), _BUCK(_STEP1), _REG(_B6CTRL2), _REG(_B6CTRL1), _TIME(_BUCK6_REV1)),
		BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), _BUCK(_MIN1_REV1), _BUCK(_STEP1), _REG(_B7CTRL2), _REG(_B7CTRL1), _TIME(_BUCK7_REV1)),
		BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2), _REG(_B8CTRL2), _REG(_B8CTRL1), _TIME(_BUCK8_REV1)),
		BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2), _REG(_B9CTRL2), _REG(_B9CTRL1), _TIME(_BUCK9_REV1)),
		BUCK_DESC("BUCK10", _BUCK(10), &_buck_ops(), _BUCK(_MIN2), _BUCK(_STEP2), _REG(_B10CTRL2), _REG(_B10CTRL1), _TIME(_BUCK10_REV1)),
		BUCK_DESC("BB", S2MPS15_BB1, &_buck_ops(), _BUCK(_MIN3), _BUCK(_STEP2), _REG(_BB1CTRL2), _REG(_BB1CTRL1), _TIME(_BB_REV1)),
	},

};

#ifdef CONFIG_OF
static int s2mps15_pmic_dt_parse_pdata(struct sec_pmic_dev *iodev,
					struct sec_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct sec_regulator_data *rdata;
	unsigned int i, s2mps15_desc_type;
	int ret;
	u32 val;
	pdata->smpl_warn_vth = 0;
	pdata->smpl_warn_hys = 0;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
	/* If pmic is revised, get 3 gpio values */
	if (SEC_PMIC_REV(iodev)) {
		if (of_gpio_count(pmic_np) < 3) {
			dev_err(iodev->dev, "could not find pmic gpios\n");
			return -EINVAL;
		}
		pdata->smpl_warn = of_get_gpio(pmic_np, 0);
		pdata->g3d_pin = of_get_gpio(pmic_np, 1);
		pdata->dvs_pin = of_get_gpio(pmic_np, 2);

		ret = of_property_read_u32(pmic_np, "g3d_en", &val);
		if (ret)
			return -EINVAL;
		pdata->g3d_en = !!val;

		ret = of_property_read_u32(pmic_np, "dvs_en", &val);
		if (ret)
			return -EINVAL;
		pdata->dvs_en = !!val;

		ret = of_property_read_u32(pmic_np, "smpl_warn_en", &val);
		if (ret)
			return -EINVAL;
		pdata->smpl_warn_en = !!val;

		ret = of_property_read_u32(pmic_np, "smpl_warn_vth", &val);
		if (ret)
			return -EINVAL;
		pdata->smpl_warn_vth = val;

		ret = of_property_read_u32(pmic_np, "smpl_warn_hys", &val);
		if (ret)
			return -EINVAL;
		pdata->smpl_warn_hys = val;
	}

	pdata->adc_en = false;
	if (of_find_property(pmic_np, "adc-on", NULL))
		pdata->adc_en = true;
	iodev->adc_en = pdata->adc_en;

	pdata->buck_dvs_on = (of_find_property(pmic_np, "buck_dvs_on", NULL))
			? true : false;

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev,
			"could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	s2mps15_desc_type = SEC_PMIC_REV(iodev) ? S2MPS15_DESC_TYPE1 : S2MPS15_DESC_TYPE0;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators[s2mps15_desc_type]); i++)
			if (!of_node_cmp(reg_np->name,
					regulators[s2mps15_desc_type][i].name))
				break;

		if (i == ARRAY_SIZE(regulators[s2mps15_desc_type])) {
			dev_warn(iodev->dev,
			"don't know how to configure regulator %s\n",
			reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(
						iodev->dev, reg_np);
		rdata->reg_node = reg_np;
		rdata++;
	}

	return 0;
}
#else
static int s2mps15_pmic_dt_parse_pdata(struct sec_pmic_dev *iodev,
					struct sec_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static unsigned int s2mps15_pmic_get_low_bat_lv(unsigned int vth, int pmic_rev)
{
	unsigned int low_bat_lv;

	if (pmic_rev == 0) {
		switch (vth) {
		case 2100 :
		case 2300 :
			low_bat_lv = 0x00;
			break;
		case 2500 :
			low_bat_lv = 0x40;
			break;
		case 2700 :
			low_bat_lv = 0x80;
			break;
		case 2900 :
			low_bat_lv = 0xC0;
			break;
		case 3100 :
		case 3300 :
		case 3500 :
			low_bat_lv = 0xE0;
			break;
		default :
			pr_warn("%s: smpl_warn_vth is unvalidated\n", __func__);
			low_bat_lv = 0x00;
			goto err;
		}
	} else {
		switch (vth) {
		case 2100 :
			low_bat_lv = 0x00;
			break;
		case 2300 :
			low_bat_lv = 0x20;
			break;
		case 2500 :
			low_bat_lv = 0x40;
			break;
		case 2700 :
			low_bat_lv = 0x60;
			break;
		case 2900 :
			low_bat_lv = 0x80;
			break;
		case 3100 :
			low_bat_lv = 0xA0;
			break;
		case 3300 :
			low_bat_lv = 0xC0;
			break;
		case 3500 :
			low_bat_lv = 0xE0;
			break;
		default :
			pr_warn("%s: smpl_warn_vth is unvalidated\n", __func__);
			low_bat_lv = 0x00;
			goto err;
		}
	}
	pr_info("%s: pmic_rev(%d), smpl_warn vthreshold is %dmV(0x%x)\n", __func__,
			pmic_rev+1, vth, low_bat_lv);

err:

	return low_bat_lv;
}

static int s2mps15_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps15_info *s2mps15;
	int i, ret;
	unsigned int temp, low_bat_lv;
	unsigned int s2mps15_desc_type;
	int delta_sel;

	ret = sec_reg_read(iodev, S2MPS15_REG_ID, &SEC_PMIC_REV(iodev));
	if (ret < 0)
		return ret;

	SEC_PMIC_REV(iodev) &= 0x0f;
	s2mps15_desc_type = SEC_PMIC_REV(iodev) ? S2MPS15_DESC_TYPE1 : S2MPS15_DESC_TYPE0;
	if (iodev->dev->of_node) {
		ret = s2mps15_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps15 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps15_info),
				GFP_KERNEL);
	if (!s2mps15)
		return -ENOMEM;
	s2mps15->desc_type = s2mps15_desc_type;
	s2mps15->iodev = iodev;
	s2mps15->g3d_en_pin = (const char *)of_get_property(iodev->dev->of_node, "buck6en_pin", NULL);
	s2mps15->g3d_en_addr = (const char *)of_get_property(iodev->dev->of_node, "buck6en_addr", NULL);
	s2mps15->int_max = 0;
	s2mps15->buck_dvs_on = pdata->buck_dvs_on;
	mutex_init(&s2mps15->lock);

	static_info = s2mps15;

	/* fix g3d ocp 6A */
	ret = sec_reg_read(iodev, 0x59, &temp);
	if (ret < 0)
		goto err;
	s2mps15->g3d_ocp = (temp & 0x20) ? false : true;

	if (s2mps15->g3d_ocp) {
		ret = sec_reg_write(iodev, 0xC0, 0x86);
		if (ret) {
			dev_err(&pdev->dev, "fix g3d 6A ocp error\n");
			goto err;
		}
	}

	if (SEC_PMIC_REV(iodev)) {
		s2mps15->dvs_en = pdata->dvs_en;
		s2mps15->g3d_en = pdata->g3d_en;
		if (gpio_is_valid(pdata->dvs_pin)) {
			ret = devm_gpio_request(&pdev->dev, pdata->dvs_pin,
						"S2MPS15 DVS_PIN");
			if (ret < 0)
				return ret;
			if (pdata->dvs_en) {
				/* Set DVS Regulator Voltage 0x20 - 0.5 voltage */
				temp = (s2mps15->g3d_ocp) ? 0x25 : 0x20;
				ret = sec_reg_write(iodev, S2MPS15_REG_B6CTRL3, temp);
				if (ret < 0)
					return ret;
				s2m_init_dvs();
			}
			s2mps15->dvs_pin = pdata->dvs_pin;
		} else
			dev_err(&pdev->dev, "dvs pin is not valid\n");
	}

	platform_set_drvdata(pdev, s2mps15);

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.regmap = iodev->regmap;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mps15;
		config.of_node = pdata->regulators[i].reg_node;
		s2mps15->opmode[id] = regulators[s2mps15_desc_type][id].enable_mask;

		s2mps15->rdev[i] = regulator_register(
				&regulators[s2mps15_desc_type][id], &config);
		if (IS_ERR(s2mps15->rdev[i])) {
			ret = PTR_ERR(s2mps15->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mps15->rdev[i] = NULL;
			goto err;
		}
		if ((id == S2MPS15_BUCK2 || id == S2MPS15_BUCK3
				|| id == S2MPS15_BUCK6 || id == S2MPS15_LDO8)
				&& s2mps15->buck_dvs_on) {
			mutex_lock(&s2mps15->lock);
			ret = s2m_set_ldo_dvs_control(s2mps15->rdev[i]);
			if (ret < 0)
				dev_err(&pdev->dev, "failed vth/delta init, id(%d)\n", id);
			mutex_unlock(&s2mps15->lock);
		}
	}

	if (s2mps15->buck_dvs_on) {
		delta_sel = 0;

		ret = sec_reg_update(iodev, S2MPS15_REG_L7CTRL, 0x23, 0x3f);
		if (ret < 0)
			goto err;

		ret = sec_reg_update(iodev, S2MPS15_REG_L9CTRL, 0x23, 0x3f);
		if (ret < 0)
			goto err;

		if (s2mps15->buck2_dvs)
			delta_sel = (s2mps15->buck2_dvs - 1) * 4 + 8;

		ret = sec_reg_read(iodev, S2MPS15_REG_B2CTRL2, &temp);
		if (ret < 0)
			goto err;

		s2mps15->buck2_sync = (temp + delta_sel > 0x8C) ? false : true;
		if (!s2mps15->buck2_sync)
			ret = sec_reg_update(s2mps15->iodev, S2MPS15_REG_LDO_DVS1,
								0 << 2, 0x1 << 2);
		if (s2mps15->buck3_dvs)
			delta_sel = (s2mps15->buck3_dvs - 1) * 4 + 8;

		ret = sec_reg_read(iodev, S2MPS15_REG_B3CTRL2, &temp);
		if (ret < 0)
			goto err;

		s2mps15->buck3_sync = (temp + delta_sel > 0x8C) ? false : true;
		if (!s2mps15->buck3_sync)
			ret = sec_reg_update(s2mps15->iodev, S2MPS15_REG_LDO_DVS3,
								0 << 2, 0x1 << 2);

		ret = sec_reg_read(iodev, S2MPS15_REG_B4CTRL2, &s2mps15->buck4_sel);
		if (ret < 0)
			goto err;
		ret = sec_reg_read(iodev, S2MPS15_REG_B5CTRL2, &s2mps15->buck5_sel);
		if (ret < 0)
			goto err;

		s2mps15->int_max = s2m_get_max_int_voltage(s2mps15);
	}

	s2mps15->num_regulators = pdata->num_regulators;

	if (SEC_PMIC_REV(iodev)) {
		if (gpio_is_valid(pdata->g3d_pin)) {
			ret = devm_gpio_request(&pdev->dev, pdata->g3d_pin,
					"S2MPS15 G3D_PIN");
			if (ret < 0)
				goto err;

			s2mps15->g3d_pin = pdata->g3d_pin;

			if (pdata->g3d_en) {
				g3d_pin_config_set();
				/* for buck6 gpio control, disable i2c control */
				ret = sec_reg_update(iodev, S2MPS15_REG_B6CTRL1, 0x00, 0xC0);
				if (ret) {
					dev_err(&pdev->dev, "buck6 gpio control error\n");
					goto err;
				}
				/* sync buck6 and ldo10 */
				ret = sec_reg_update(iodev, S2MPS15_REG_LDO_RSVD3, 0x40, 0x40);
				if (ret) {
					dev_err(&pdev->dev, "regulator sync enable error\n");
					goto err;
				}
				ret = sec_reg_update(iodev, S2MPS15_REG_L10CTRL, 0x00, 0xC0);
				if (ret) {
					dev_err(&pdev->dev, "regulator sync enable error\n");
					goto err;
				}
			}
		} else
			dev_err(&pdev->dev, "g3d pin is not valid\n");
	}

	if(SEC_PMIC_REV(iodev) == 0x01){/* to distinguish rev0 and others */
		sec_reg_read(iodev, 0x59, &temp);
		if ((temp & 0x10) == 0x00) {
			/* buck1-7, 10 forced PWM mode for EVT1 PMIC */
			ret = sec_reg_update(iodev, S2MPS15_REG_B1CTRL1, 0x0C, 0x0C);
			if (ret) {
				dev_err(&pdev->dev, "forced pwm i2c write error\n");
				goto err;
			}
			ret = sec_reg_update(iodev, S2MPS15_REG_B2CTRL1, 0x0C, 0x0C);
			if (ret) {
				dev_err(&pdev->dev, "forced pwm i2c write error\n");
				goto err;
			}
			ret = sec_reg_update(iodev, S2MPS15_REG_B3CTRL1, 0x0C, 0x0C);
			if (ret) {
				dev_err(&pdev->dev, "forced pwm i2c write error\n");
				goto err;
			}
			ret = sec_reg_update(iodev, S2MPS15_REG_B4CTRL1, 0x0C, 0x0C);
			if (ret) {
				dev_err(&pdev->dev, "forced pwm i2c write error\n");
				goto err;
			}
			ret = sec_reg_update(iodev, S2MPS15_REG_B5CTRL1, 0x0C, 0x0C);
			if (ret) {
				dev_err(&pdev->dev, "forced pwm i2c write error\n");
				goto err;
			}
			ret = sec_reg_update(iodev, S2MPS15_REG_B6CTRL1, 0x0C, 0x0C);
			if (ret) {
				dev_err(&pdev->dev, "forced pwm i2c write error\n");
				goto err;
			}
			ret = sec_reg_update(iodev, S2MPS15_REG_B7CTRL1, 0x0C, 0x0C);
			if (ret) {
				dev_err(&pdev->dev, "forced pwm i2c write error\n");
				goto err;
			}
			ret = sec_reg_update(iodev, S2MPS15_REG_B10CTRL1, 0x0C, 0x0C);
			if (ret) {
				dev_err(&pdev->dev, "forced pwm i2c write error\n");
				goto err;
			}

			/* changes in buck3 dvs setting due to sudden off of buck3 */
			ret = sec_reg_write(iodev, 0x80, 0x74);
			if (ret) {
				dev_err(&pdev->dev, "dvs configuration i2c write error\n");
				goto err;
			}
			ret = sec_reg_write(iodev, 0x89, 0x34);
			if (ret) {
				dev_err(&pdev->dev, "dvs configuration i2c write error\n");
				goto err;
			}
			ret = sec_reg_write(iodev, 0xF8, 0x51);
			if (ret) {
				dev_err(&pdev->dev, "dvs configuration i2c write error\n");
				goto err;
			}
		}
	}

	if (pdata->smpl_warn_en && SEC_PMIC_REV(iodev)) {	/* to distingusish EVT0 and others */
		sec_reg_read(iodev, 0x59, &temp);
		if ((temp & 0x10) != 0x10) {		/* PMIC EVT1 */
			low_bat_lv = s2mps15_pmic_get_low_bat_lv(pdata->smpl_warn_vth, 0);
		} else {				/* PMIC EVT2 */
			low_bat_lv = s2mps15_pmic_get_low_bat_lv(pdata->smpl_warn_vth, 1);
		}
		ret = sec_reg_update(iodev, S2MPS15_REG_CTRL2, low_bat_lv, 0xe0);
		if (ret) {
			dev_err(&pdev->dev, "set smpl_warn configuration i2c write error\n");
			goto err;
		}
		ret = sec_reg_update(iodev, S2MPS15_REG_CTRL2, pdata->smpl_warn_hys, 0x18);
		if (ret) {
		dev_err(&pdev->dev, "set smpl_warn configuration i2c write error\n");
		goto err;
		}
		pr_info("%s: smpl_warn hysteresis is 0x%x\n", __func__, pdata->smpl_warn_hys);
	}

	/* RTC Low jitter mode */
	ret = sec_reg_update(iodev, S2MPS15_REG_RTC_BUF, 0x10, 0x10);
	if (ret) {
		dev_err(&pdev->dev, "set low jitter mode i2c write error\n");
		goto err;
	}

	/* Buck2/6 phase shedding off configuration for EVT2 and EVT3 */
	sec_reg_read(iodev, 0x59, &temp);
	if (SEC_PMIC_REV(iodev) == 2 || (SEC_PMIC_REV(iodev) == 1
			 && (temp & 0x10))) {
		pr_info("%s: i2c settings..\n", __func__);
		ret = sec_reg_write(iodev, 0x77, 0x6C);
		if (ret) {
			dev_err(&pdev->dev, "fail buck2 phase shedding off\n");
			goto err;
		}
		ret = sec_reg_write(iodev, 0x78, 0x94);
		if (ret) {
			dev_err(&pdev->dev, "fail buck2 phase shedding off\n");
			goto err;
		}
		ret = sec_reg_write(iodev, 0x8A, 0x94);
		if (ret) {
			dev_err(&pdev->dev, "fail buck6 phase shedding off\n");
			goto err;
		}
		ret = sec_reg_write(iodev, 0xF9, 0x11);
		if (ret) {
			dev_err(&pdev->dev, "fail buck6 phase shedding off\n");
			goto err;
		}
	}

	/* wake-up seq. exchange : Buck1 <--> Buck4 */
	ret = sec_reg_write(iodev, 0x5F, 0xEC);
	if (ret) {
		dev_err(&pdev->dev, "fail B1&B4 wake-up seq. exchange\n");
		goto err;
	}
	ret = sec_reg_write(iodev, 0x60, 0xBF);
	if (ret) {
		dev_err(&pdev->dev, "fail B1&B4 wake-up seq. exchange\n");
		goto err;
	}

	return 0;
err:
	for (i = 0; i < S2MPS15_REGULATOR_MAX; i++)
		regulator_unregister(s2mps15->rdev[i]);

	return ret;
}

static int s2mps15_pmic_remove(struct platform_device *pdev)
{
	struct s2mps15_info *s2mps15 = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < S2MPS15_REGULATOR_MAX; i++)
		regulator_unregister(s2mps15->rdev[i]);

	return 0;
}

static const struct platform_device_id s2mps15_pmic_id[] = {
	{ "s2mps15-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps15_pmic_id);

static struct platform_driver s2mps15_pmic_driver = {
	.driver = {
		.name = "s2mps15-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mps15_pmic_probe,
	.remove = s2mps15_pmic_remove,
	.id_table = s2mps15_pmic_id,
};

static int __init s2mps15_pmic_init(void)
{
	return platform_driver_register(&s2mps15_pmic_driver);
}
subsys_initcall(s2mps15_pmic_init);

static void __exit s2mps15_pmic_exit(void)
{
	platform_driver_unregister(&s2mps15_pmic_driver);
}
module_exit(s2mps15_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS15 Regulator Driver");
MODULE_LICENSE("GPL");
