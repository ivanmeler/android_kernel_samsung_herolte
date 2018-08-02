/*
 * exynos_tmu.c - Samsung EXYNOS TMU (Thermal Management Unit)
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *  Amit Daniel Kachhap <amit.kachhap@linaro.org>
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
 */

#include <linux/cpufreq.h>
#include <linux/isp_cooling.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/ipa.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/exynos-ss.h>
#include <linux/pm_qos.h>
#include <linux/cpu.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/cpufreq.h>

#if defined(CONFIG_ECT)
#include <soc/samsung/ect_parser.h>
#endif

#include "exynos_thermal_common.h"
#include "exynos_tmu.h"
#include "exynos_tmu_data.h"

#if defined(CONFIG_CPU_THERMAL_IPA)
static unsigned int sensor_count = 0;
#endif
struct cpufreq_frequency_table gpu_freq_table[10];
struct isp_fps_table isp_fps_table[10];
unsigned long base_addr[10];

/**
 * struct exynos_tmu_data : A structure to hold the private data of the TMU
	driver
 * @id: identifier of the one instance of the TMU controller.
 * @pdata: pointer to the tmu platform/configuration data
 * @base: base address of the single instance of the TMU controller.
 * @base_second: base address of the common registers of the TMU controller.
 * @irq: irq number of the TMU controller.
 * @soc: id of the SOC type.
 * @irq_work: pointer to the irq work structure.
 * @lock: lock to implement synchronization.
 * @temp_error1: fused value of the first point trim.
 * @temp_error2: fused value of the second point trim.
 * @regulator: pointer to the TMU regulator structure.
 * @reg_conf: pointer to structure to register with core thermal.
 */
struct exynos_tmu_data {
	int id;
	bool initialized;
	struct exynos_tmu_platform_data *pdata;
	void __iomem *base;
	void __iomem *base_second;
	int irq;
	enum soc_type soc;
	struct work_struct irq_work;
	struct notifier_block nb;
	struct mutex lock;
	u16 temp_error1, temp_error2;
	struct regulator *regulator;
	struct thermal_sensor_conf *reg_conf;
	struct list_head node;
	int temp;
	int vaild;
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	int cpu_num;
#endif
};

/* list of multiple instance for each thermal sensor */
static LIST_HEAD(dtm_dev_list);

/*
 * TMU treats temperature as a mapped temperature code.
 * The temperature is converted differently depending on the calibration type.
 */
static int temp_to_code(struct exynos_tmu_data *data, u16 temp)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	int temp_code = -EINVAL;
	u16 rtemp;

	if (temp > EXYNOS_MAX_TEMP)
		rtemp = EXYNOS_MAX_TEMP;
	else if (temp < EXYNOS_MIN_TEMP)
		rtemp = EXYNOS_MIN_TEMP;
	else
		rtemp = temp;

	switch (pdata->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp_code = (rtemp - pdata->first_point_trim) *
			(data->temp_error2 - data->temp_error1) /
			(pdata->second_point_trim - pdata->first_point_trim) +
			data->temp_error1;
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp_code = rtemp + data->temp_error1 - pdata->first_point_trim;
		break;
	default:
		temp_code = rtemp + pdata->default_temp_offset;
		break;
	}
	return temp_code;
}

/*
 * Calculate a temperature value from a temperature code.
 * The unit of the temperature is degree Celsius.
 */
static int code_to_temp(struct exynos_tmu_data *data, u16 temp_code)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	int temp;

	switch (pdata->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp = (temp_code - data->temp_error1) *
			(pdata->second_point_trim - pdata->first_point_trim) /
			(data->temp_error2 - data->temp_error1) +
			pdata->first_point_trim;
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp = temp_code - data->temp_error1 + pdata->first_point_trim;
		break;
	default:
		temp = temp_code - pdata->default_temp_offset;
		break;
	}

	/* temperature should range between minimum and maximum */
	if (temp > EXYNOS_MAX_TEMP)
		temp = EXYNOS_MAX_TEMP;
	else if (temp < EXYNOS_MIN_TEMP)
		temp = EXYNOS_MIN_TEMP;

	return temp;
}

static void exynos_tmu_clear_irqs(struct exynos_tmu_data *data)
{
	const struct exynos_tmu_registers *reg = data->pdata->registers;
	unsigned int val_irq;

	val_irq = readl(data->base + reg->tmu_intstat);
	/*
	 * Clear the interrupts.  Please note that the documentation for
	 * Exynos3250, Exynos4412, Exynos5250 and Exynos5260 incorrectly
	 * states that INTCLEAR register has a different placing of bits
	 * responsible for FALL IRQs than INTSTAT register.  Exynos5420
	 * and Exynos5440 documentation is correct (Exynos4210 doesn't
	 * support FALL IRQs at all).
	 */
	writel(val_irq, data->base + reg->tmu_intclear);
}

static int exynos_tmu_initialize(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	const struct exynos_tmu_registers *reg = pdata->registers;
	unsigned int status, trim_info = 0, con, ctrl;
	unsigned int rising_threshold = 0, falling_threshold = 0;
	unsigned int rising7_4_threshold = 0, falling7_4_threshold = 0;
	unsigned int rising_threshold0 = 0, rising_threshold1 = 0;
	unsigned int rising_threshold2 = 0, rising_threshold3 = 0;
	unsigned int falling_threshold0 = 0, falling_threshold1 = 0;
	unsigned int falling_threshold2 = 0, falling_threshold3 = 0;
	int ret = 0, threshold_code, i;
	int timeout, shift;
	unsigned int temp_mask = TYPE_8BIT_MASK;

	mutex_lock(&data->lock);
	if (pdata->sensor_type == VIRTUAL_SENSOR) {
		if (!data->initialized)
			list_add_tail(&data->node, &dtm_dev_list);

		data->initialized = true;
		mutex_unlock(&data->lock);

		return 0;
	}

	if (reg->calib_sel_shift) {
		status = (readl(data->base + reg->triminfo_data) >> reg->calib_sel_shift) \
				& reg->calib_sel_mask;

		if (status)
			pdata->cal_type = TYPE_TWO_POINT_TRIMMING;
		else
			pdata->cal_type = TYPE_ONE_POINT_TRIMMING;

		dev_info(&pdev->dev, "cal type : %d \n", pdata->cal_type);
	}

	if (TMU_SUPPORTS(pdata, READY_STATUS)) {
		timeout = 10;
		while (1) {
			status = readl(data->base + reg->tmu_status);
			if (status & 0x1)
				break;

			timeout--;
			if (!timeout) {
				pr_err("TMU is busy.\n");
				break;
			}
			udelay(5);
		}
	}

	if (TMU_SUPPORTS(pdata, TRIM_RELOAD)) {
		for (i = 0; i < reg->triminfo_ctrl_count; i++) {
			if (pdata->triminfo_reload[i]) {
				ctrl = readl(data->base +
						reg->triminfo_ctrl[i]);
				ctrl |= pdata->triminfo_reload[i];
				writel(ctrl, data->base +
						reg->triminfo_ctrl[i]);
			}
		}
	}

	if (!data->initialized)
		list_add_tail(&data->node, &dtm_dev_list);

	/* Save trimming info in order to perform calibration */
	if (data->soc == SOC_ARCH_EXYNOS5440) {
		/*
		 * For exynos5440 soc triminfo value is swapped between TMU0 and
		 * TMU2, so the below logic is needed.
		 */
		switch (data->id) {
		case 0:
			trim_info = readl(data->base +
			EXYNOS5440_EFUSE_SWAP_OFFSET + reg->triminfo_data);
			break;
		case 1:
			trim_info = readl(data->base + reg->triminfo_data);
			break;
		case 2:
			trim_info = readl(data->base -
			EXYNOS5440_EFUSE_SWAP_OFFSET + reg->triminfo_data);
		}
	} else {
		/* On exynos5420 the triminfo register is in the shared space */
		if (data->soc == SOC_ARCH_EXYNOS5420_TRIMINFO)
			trim_info = readl(data->base_second +
							reg->triminfo_data);
		else
			trim_info = readl(data->base + reg->triminfo_data);
	}
	/* If temp_mask variables exist, temp bit is 9 bit. */
	if (pdata->temp_mask)
		temp_mask = pdata->temp_mask;

	data->temp_error1 = trim_info & temp_mask;
	data->temp_error2 = ((trim_info >> reg->triminfo_85_shift) & temp_mask);

	if (!data->temp_error1)
		data->temp_error1 = pdata->efuse_value & temp_mask;

	if (!data->temp_error2)
		data->temp_error2 =
			(pdata->efuse_value >> reg->triminfo_85_shift) & temp_mask;

	rising_threshold = readl(data->base + reg->threshold_th0);

	if (data->soc == SOC_ARCH_EXYNOS4210) {
		/* Write temperature code for threshold */
		threshold_code = temp_to_code(data, pdata->threshold);
		writeb(threshold_code,
			data->base + reg->threshold_temp);
		for (i = 0; i < pdata->non_hw_trigger_levels; i++)
			writeb(pdata->trigger_levels[i], data->base +
			reg->threshold_th0 + i * sizeof(reg->threshold_th0));

		exynos_tmu_clear_irqs(data);
	} else {
		/* EXYNOS8890 add threshold_th4~7 register, so if threshold_th4~7 register existed call this code */
		if (reg->threshold_th4 && reg->threshold_th5 && reg->threshold_th6 && reg->threshold_th7) {
			for (i = 0; i < pdata->non_hw_trigger_levels; i++) {
				threshold_code = temp_to_code(data, pdata->trigger_levels[i]);

				if (i < 2) {
					shift = 16 * i;
					rising_threshold0 &= ~(0x1ff << shift);
					rising_threshold0 |= (threshold_code << shift);
				} else if (i >= 2 && i < 4) {
					shift = 16 * (i - 2);
					rising_threshold1 &= ~(0x1ff << shift);
					rising_threshold1 |= (threshold_code << shift);
				} else if (i >= 4 && i < 6) {
					shift = 16 * (i - 4);
					rising_threshold2 &= ~(0x1ff << shift);
					rising_threshold2 |= (threshold_code << shift);
				} else if (i >= 6 && i < 8) {
					shift = 16 * (i - 6);
					rising_threshold3 &= ~(0x1ff << shift);
					rising_threshold3 |= (threshold_code << shift);
				}

				if (pdata->threshold_falling) {
					threshold_code = temp_to_code(data,
							pdata->trigger_levels[i] -
							pdata->threshold_falling);
					if (threshold_code > 0) {
						if (i < 2) {
							shift = (16 * i);
							falling_threshold0 |= (threshold_code << shift);
						} else if (i >= 2 && i < 4) {
							shift = (16 * (i - 2));
							falling_threshold1 |= (threshold_code << shift);
						} else if (i >= 4 && i < 6) {
							shift = (16 * (i - 4));
							falling_threshold2 |= (threshold_code << shift);
						} else if (i >= 6 && i < 8) {
							shift = (16 * (i - 6));
							falling_threshold3 |= (threshold_code << shift);
						}
					}
				}
			}

			writel(rising_threshold0, data->base + reg->threshold_th0);
			writel(falling_threshold0, data->base + reg->threshold_th1);
			writel(rising_threshold1, data->base + reg->threshold_th2);
			writel(falling_threshold1, data->base + reg->threshold_th3);
			writel(rising_threshold2, data->base + reg->threshold_th4);
			writel(falling_threshold2, data->base + reg->threshold_th5);
			writel(rising_threshold3, data->base + reg->threshold_th6);
			writel(falling_threshold3, data->base + reg->threshold_th7);
		} else {
			/* Write temperature code for rising and falling threshold */
			for (i = 0; i < pdata->non_hw_trigger_levels; i++) {
				threshold_code = temp_to_code(data, pdata->trigger_levels[i]);
				if (i < 4) {
					rising_threshold &= ~(0xff << 8 * i);
					rising_threshold |= threshold_code << 8 * i;
				} else {
					rising7_4_threshold &= ~(0xff << 8 * (i - 4));
					rising7_4_threshold |= threshold_code << 8 * (i - 4);
				}
				if (pdata->threshold_falling) {
					threshold_code = temp_to_code(data,
							pdata->trigger_levels[i] -
							pdata->threshold_falling);
					if (i < 4)
						falling_threshold |= threshold_code << 8 * i;
					else
						falling7_4_threshold |= threshold_code << 8 * (i - 4);
				}
			}
			writel(rising_threshold,
					data->base + reg->threshold_th0);
			writel(falling_threshold,
					data->base + reg->threshold_th1);
			writel(rising7_4_threshold,
					data->base + reg->threshold_th2);
			writel(falling7_4_threshold,
					data->base + reg->threshold_th3);
		}

		exynos_tmu_clear_irqs(data);

		/* if last threshold limit is also present */
		i = pdata->max_trigger_level - 1;
		if (pdata->trigger_levels[i] &&
				(pdata->trigger_type[i] == HW_TRIP)) {
			threshold_code = temp_to_code(data,
						pdata->trigger_levels[i]);
			/* EXYNOS8890 add threshold_th4~7 register, so if threshold_th4~7 register existed call this code */
			if (reg->threshold_th4 && reg->threshold_th5 && reg->threshold_th6 && reg->threshold_th7) {
				if (i == EXYNOS_MAX_TRIGGER_PER_REG - 1) {
					/* 1-4 level to be assigned in th0 reg */
					shift = 16 * (i - 6);
					rising_threshold3 &= ~(0x1ff << shift);
					rising_threshold3 |= (threshold_code << shift);
					writel(rising_threshold3,
							data->base + reg->threshold_th6);
				} else if (i == EXYNOS_MAX_TRIGGER_PER_REG) {
					/* 5th level to be assigned in th2 reg */
					shift = 16 * (i - 6);
					rising_threshold3 &= ~(0x1ff << shift);
					rising_threshold3 |= (threshold_code << shift);
					writel(rising_threshold3,
							data->base + reg->threshold_th6);
				}
			} else {
				if (i == EXYNOS_MAX_TRIGGER_PER_REG - 1) {
					/* 1-4 level to be assigned in th0 reg */
					rising_threshold &= ~(0xff << 8 * i);
					rising_threshold |= threshold_code << 8 * i;
					writel(rising_threshold,
							data->base + reg->threshold_th0);
				} else if (i == EXYNOS_MAX_TRIGGER_PER_REG) {
					/* 5th level to be assigned in th2 reg */
					rising_threshold =
					threshold_code << reg->threshold_th3_l0_shift;
					writel(rising_threshold,
						data->base + reg->threshold_th2);
				}
			}
			con = readl(data->base + reg->tmu_ctrl);
			con |= (1 << reg->therm_trip_en_shift);
			writel(con, data->base + reg->tmu_ctrl);

			timeout = 10;
			while (1) {
				status = readl(data->base + reg->tmu_status);
				if (status & 0x1)
					break;

				timeout--;
				if (!timeout) {
					pr_err("TMU is busy.\n");
					break;
				}
				udelay(5);
			}

		}
	}
	/*Clear the PMIN in the common TMU register*/
	if (reg->tmu_pmin && !data->id)
		writel(0, data->base_second + reg->tmu_pmin);

	data->initialized = true;
	mutex_unlock(&data->lock);

	return ret;
}

static int exynos_tmu_control(struct platform_device *pdev, bool on)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	const struct exynos_tmu_registers *reg = pdata->registers;
	unsigned int con, interrupt_en, otp_fuse;
	int timeout, status;

	if (pdata->sensor_type == VIRTUAL_SENSOR)
		return 0;

	mutex_lock(&data->lock);

	con = readl(data->base + reg->tmu_ctrl);

	if (pdata->test_mux)
		con |= (pdata->test_mux << reg->test_mux_addr_shift);

	/* Add reg->buf_vref_otp_reg register condition at EXYNOS8890 */
	/* Beacuse we read otp data and write this value to control register */
	if (!reg->buf_vref_otp_mask) {
		con &= ~(EXYNOS_TMU_REF_VOLTAGE_MASK << EXYNOS_TMU_REF_VOLTAGE_SHIFT);
		con |= pdata->reference_voltage << EXYNOS_TMU_REF_VOLTAGE_SHIFT;
	} else {
		/* Write to otp save data */
		con &= ~(EXYNOS_TMU_REF_VOLTAGE_MASK << EXYNOS_TMU_REF_VOLTAGE_SHIFT);
		otp_fuse = readl(data->base + reg->buf_vref_otp_reg);
		otp_fuse = (otp_fuse >> reg->buf_vref_otp_shift) & reg->buf_vref_otp_mask;
		con |= (otp_fuse << EXYNOS_TMU_REF_VOLTAGE_SHIFT);
	}

	/* Add reg->buf_vref_otp_reg register condition at EXYNOS8890 */
	/* Beacuse we read otp data and write this value to control register */
	if (!reg->buf_slope_otp_mask) {
		con &= ~(EXYNOS_TMU_BUF_SLOPE_SEL_MASK << EXYNOS_TMU_BUF_SLOPE_SEL_SHIFT);
		con |= (pdata->gain << EXYNOS_TMU_BUF_SLOPE_SEL_SHIFT);
	} else {
		/* Write to otp save data */
		con &= ~(EXYNOS_TMU_BUF_SLOPE_SEL_MASK << EXYNOS_TMU_BUF_SLOPE_SEL_SHIFT);
		otp_fuse = readl(data->base + reg->buf_slope_otp_reg);
		otp_fuse = (otp_fuse >> reg->buf_slope_otp_shift) & reg->buf_slope_otp_mask;
		con |= (otp_fuse << EXYNOS_TMU_BUF_SLOPE_SEL_SHIFT);
	}

	if (pdata->noise_cancel_mode) {
		con &= ~(reg->therm_trip_mode_mask <<
					reg->therm_trip_mode_shift);
		con |= (pdata->noise_cancel_mode << reg->therm_trip_mode_shift);
	}

	if (on) {
		con |= (1 << EXYNOS_TMU_CORE_EN_SHIFT);
		con |= (1 << reg->therm_trip_en_shift);
		interrupt_en =
			pdata->trigger_enable[7] << reg->inten_rise7_shift |
			pdata->trigger_enable[6] << reg->inten_rise6_shift |
			pdata->trigger_enable[5] << reg->inten_rise5_shift |
			pdata->trigger_enable[4] << reg->inten_rise4_shift |
			pdata->trigger_enable[3] << reg->inten_rise3_shift |
			pdata->trigger_enable[2] << reg->inten_rise2_shift |
			pdata->trigger_enable[1] << reg->inten_rise1_shift |
			pdata->trigger_enable[0] << reg->inten_rise0_shift;
		if (TMU_SUPPORTS(pdata, FALLING_TRIP))
			interrupt_en |=
				pdata->trigger_enable[7] << reg->inten_fall7_shift |
				pdata->trigger_enable[6] << reg->inten_fall6_shift |
				pdata->trigger_enable[5] << reg->inten_fall5_shift |
				pdata->trigger_enable[4] << reg->inten_fall4_shift |
				pdata->trigger_enable[3] << reg->inten_fall3_shift |
				pdata->trigger_enable[2] << reg->inten_fall2_shift |
				pdata->trigger_enable[1] << reg->inten_fall1_shift |
				pdata->trigger_enable[0] << reg->inten_fall0_shift;
	} else {
		con &= ~(1 << EXYNOS_TMU_CORE_EN_SHIFT);
		con &= ~(1 << reg->therm_trip_en_shift);
		interrupt_en = 0; /* Disable all interrupts */
	}
	writel(interrupt_en, data->base + reg->tmu_inten);
	writel(con, data->base + reg->tmu_ctrl);

	timeout = 10;
	while (1) {
		status = readl(data->base + reg->tmu_status);
		if (status & 0x1)
			break;

		timeout--;
		if (!timeout) {
			pr_err("TMU is busy.\n");
			break;
		}
		udelay(5);
	}

	if (reg->tmu_ctrl1 && reg->lpi0_mode_en_shift) {
		con = readl(data->base + reg->tmu_ctrl1);
		con |= 1 << reg->lpi0_mode_en_shift;
		writel(con, data->base + reg->tmu_ctrl1);
	}

	mutex_unlock(&data->lock);

	return 0;
}

#if defined(CONFIG_CPU_THERMAL_IPA)
struct pm_qos_request ipa_cpu_hotplug_request;
int ipa_hotplug(bool removecores)
{
	if (removecores)
		pm_qos_update_request(&ipa_cpu_hotplug_request, NR_CLUST1_CPUS);
	else
		pm_qos_update_request(&ipa_cpu_hotplug_request, NR_CPUS);

	return 0;
}

static int exynos_tmu_ipa_temp_read(struct exynos_tmu_data *data)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	const struct exynos_tmu_registers *reg = pdata->registers;
	int temp = 0;
	u32 temp_code;

	mutex_lock(&data->lock);
	temp_code = readl(data->base + reg->tmu_cur_temp);
	temp = code_to_temp(data, temp_code);
	mutex_unlock(&data->lock);

	return temp;
}

static struct ipa_sensor_conf ipa_sensor_conf = {
	.read_soc_temperature	= (int (*)(void *))exynos_tmu_ipa_temp_read,
};
#endif

static int exynos_tmu_read(struct exynos_tmu_data *data)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	const struct exynos_tmu_registers *reg = pdata->registers;
	u32 temp_code;
	int temp;
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	char *cool_device_name = "NONE";
#endif
	mutex_lock(&data->lock);

	/* Check to the vaild data */
	if (((pdata->d_type == ISP) || (pdata->d_type == GPU)) && reg->valid_mask) {
		data->vaild = readl(data->base + reg->tmu_status);
		data->vaild = ((data->vaild >> reg->valid_p0_shift) & reg->valid_mask);
#if !defined(CONFIG_SEC_FACTORY)
		if (data->vaild) {
#endif
			temp_code = readl(data->base + reg->tmu_cur_temp);
			temp = code_to_temp(data, temp_code);
#if !defined(CONFIG_SEC_FACTORY)
		} else
			temp = 15;
#endif
	} else {
		temp_code = readl(data->base + reg->tmu_cur_temp);
		temp = code_to_temp(data, temp_code);
	}

	exynos_ss_thermal(pdata, temp, cool_device_name, 0);
	mutex_unlock(&data->lock);

	return temp;
}

#ifdef CONFIG_THERMAL_EMULATION
static int exynos_tmu_set_emulation(void *drv_data, unsigned long temp)
{
	struct exynos_tmu_data *data = drv_data;
	struct exynos_tmu_platform_data *pdata = data->pdata;
	const struct exynos_tmu_registers *reg = pdata->registers;
	unsigned int val;
	int ret = -EINVAL;

	if (!TMU_SUPPORTS(pdata, EMULATION))
		goto out;

	if (temp && temp < MCELSIUS)
		goto out;

	mutex_lock(&data->lock);

	val = readl(data->base + reg->emul_con);

	if (temp) {
		temp /= MCELSIUS;

		if (TMU_SUPPORTS(pdata, EMUL_TIME)) {
			val &= ~(EXYNOS_EMUL_TIME_MASK << reg->emul_time_shift);
			val |= (EXYNOS_EMUL_TIME << reg->emul_time_shift);
		}
		val &= ~(EXYNOS_EMUL_DATA_MASK << reg->emul_temp_shift);
		val |= (temp_to_code(data, temp) << reg->emul_temp_shift) |
			EXYNOS_EMUL_ENABLE;
	} else {
		val &= ~EXYNOS_EMUL_ENABLE;
	}

	writel(val, data->base + reg->emul_con);

	mutex_unlock(&data->lock);
	return 0;
out:
	return ret;
}
#else
static int exynos_tmu_set_emulation(void *drv_data,	unsigned long temp)
	{ return -EINVAL; }
#endif/*CONFIG_THERMAL_EMULATION*/

void exynos_tmu_core_control(bool on, int id)
{
	int count;
	unsigned int con, status;

	struct exynos_tmu_data *devnode;

	list_for_each_entry(devnode, &dtm_dev_list, node) {
		if (devnode->base && devnode->pdata->d_type == id) {
			con = readl(devnode->base + devnode->pdata->registers->tmu_ctrl);
			con &= ~(1 << EXYNOS_TMU_CORE_EN_SHIFT);
			if (on)
				con |= (1 << EXYNOS_TMU_CORE_EN_SHIFT);
			writel(con, devnode->base + devnode->pdata->registers->tmu_ctrl);

			if (on)
				continue;

			for (count = 0; count < 10; count++) {
				status = readl(devnode->base + devnode->pdata->registers->tmu_status);
				if (status & 0x1)
					break;

				udelay(5);
			}

			if (count == 10)
				pr_err("TMU is busy.\n");
		}

	}
}

#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
static DEFINE_MUTEX(boost_lock);
extern struct cpumask hmp_fast_cpu_mask;
static int big_cpu_cnt;

void core_boost_lock(void)
{
	mutex_lock(&boost_lock);
}

void core_boost_unlock(void)
{
	mutex_unlock(&boost_lock);
}

static int exynos_tmu_cpus_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct cpumask mask;
	struct exynos_tmu_data *devnode;
	struct exynos_tmu_data *quad_data = NULL, *dual_data = NULL;
	struct exynos_tmu_platform_data *quad_pdata, *dual_pdata;
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	char *cool_device_name = "CHANGE_MODE";
#endif

	switch (event) {
	case CPUS_DOWN_COMPLETE:
		cpumask_copy(&mask, data);
		cpumask_and(&mask, &mask, &hmp_fast_cpu_mask);
		big_cpu_cnt = cpumask_weight(&mask);

		list_for_each_entry(devnode, &dtm_dev_list, node) {
			if (devnode->pdata->d_type == CLUSTER1 && devnode->pdata->sensor_type == NORMAL_SENSOR) {
				quad_data = devnode;
				quad_pdata = quad_data->pdata;
			} else if (devnode->pdata->d_type == CLUSTER1 && devnode->pdata->sensor_type == VIRTUAL_SENSOR) {
				dual_data = devnode;
				dual_pdata = dual_data->pdata;
			}
		}

		if ((quad_data == NULL) || (dual_data == NULL))
			return NOTIFY_OK;

		if (big_cpu_cnt == DUAL_CPU) {
			/* changed to dual */
			mutex_lock(&boost_lock);
			dual_data->cpu_num = quad_data->cpu_num = big_cpu_cnt;

			/* Snap shot logging */
			exynos_ss_thermal(quad_pdata, 0, cool_device_name, 0);
			exynos_ss_thermal(dual_pdata, 0, cool_device_name, 1);

			change_core_boost_thermal(quad_data->reg_conf, dual_data->reg_conf, DUAL_MODE);
			mutex_unlock(&boost_lock);
		}
		break;
	case CPUS_UP_PREPARE:
		cpumask_copy(&mask, data);
		cpumask_and(&mask, &mask, &hmp_fast_cpu_mask);
		big_cpu_cnt = cpumask_weight(&mask);

		list_for_each_entry(devnode, &dtm_dev_list, node) {
			if (devnode->pdata->d_type == CLUSTER1 && devnode->pdata->sensor_type == NORMAL_SENSOR) {
				quad_data = devnode;
				quad_pdata = quad_data->pdata;
			} else if (devnode->pdata->d_type == CLUSTER1 && devnode->pdata->sensor_type == VIRTUAL_SENSOR) {
				dual_data = devnode;
				dual_pdata = dual_data->pdata;
			}
		}

		if ((quad_data == NULL) || (dual_data == NULL))
			return NOTIFY_OK;

		if (big_cpu_cnt == QUAD_CPU){
			/* changed to quad */
			mutex_lock(&boost_lock);
			dual_data->cpu_num = quad_data->cpu_num = big_cpu_cnt;

			/* Snap shot logging */
			exynos_ss_thermal(quad_pdata, 0, cool_device_name, 1);
			exynos_ss_thermal(dual_pdata, 0, cool_device_name, 0);

			change_core_boost_thermal(quad_data->reg_conf, dual_data->reg_conf, QUAD_MODE);
			mutex_unlock(&boost_lock);
		} else if (big_cpu_cnt == DUAL_CPU) {
			mutex_lock(&boost_lock);
			dual_data->cpu_num = quad_data->cpu_num = big_cpu_cnt;

			/* Snap shot logging */
			exynos_ss_thermal(quad_pdata, 0, cool_device_name, 0);
			exynos_ss_thermal(dual_pdata, 0, cool_device_name, 1);

			change_core_boost_thermal(quad_data->reg_conf, dual_data->reg_conf, DUAL_MODE);
			mutex_unlock(&boost_lock);
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_tmu_cpus_nb = {
	.notifier_call = exynos_tmu_cpus_notifier,
};
#endif

#ifdef CONFIG_CPU_PM
#ifdef CONFIG_CPU_IDLE
static int exynos_low_pwr_notifier(struct notifier_block *notifier,
		unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case LPA_ENTER:
		exynos_tmu_core_control(false, CLUSTER0);
		exynos_tmu_core_control(false, CLUSTER1);
		exynos_tmu_core_control(false, GPU);
		exynos_tmu_core_control(false, ISP);
		break;
	case LPA_ENTER_FAIL:
	case LPA_EXIT:
		exynos_tmu_core_control(true, CLUSTER0);
		exynos_tmu_core_control(true, CLUSTER1);
		exynos_tmu_core_control(true, GPU);
		exynos_tmu_core_control(true, ISP);
		break;
	}

	return NOTIFY_OK;
}
#else
static int exynos_low_pwr_notifier(struct notifier_block *notifier,
		unsigned long pm_event, void *v)
{
	return NOTIFY_OK;
}
#endif /* end of CONFIG_CPU_IDLE */

static struct notifier_block exynos_low_pwr_nb = {
	.notifier_call = exynos_low_pwr_notifier,
};
#endif /* end of CONFIG_CPU_PM */

static void exynos_tmu_work(struct work_struct *work)
{
	struct exynos_tmu_data *data = container_of(work,
			struct exynos_tmu_data, irq_work);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	const struct exynos_tmu_registers *reg = pdata->registers;
	unsigned int val_type;
	/* Find which sensor generated this interrupt */
	if (reg->tmu_irqstatus) {
		val_type = readl(data->base_second + reg->tmu_irqstatus);
		if (!((val_type >> data->id) & 0x1))
			goto out;
	}
	exynos_report_trigger(data->reg_conf);
	mutex_lock(&data->lock);

	/* TODO: take action based on particular interrupt */
	exynos_tmu_clear_irqs(data);

	mutex_unlock(&data->lock);
out:
	enable_irq(data->irq);
}

static ssize_t
exynos_thermal_sensor_temp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_tmu_data *devnode;
	int i = 0, len = 0;
	char *name;

	list_for_each_entry(devnode, &dtm_dev_list, node) {
		if(devnode->pdata->d_type == CLUSTER0)
			name = "CLUSTER0";
		else if(devnode->pdata->d_type == CLUSTER1)
			name = "CLUSTER1";
		else if(devnode->pdata->d_type == GPU)
			name = "GPU";
		else if(devnode->pdata->d_type == ISP)
			name = "ISP";
		len += snprintf(&buf[len], PAGE_SIZE,"sensor%d[%s] : %d\n",
			i, name, exynos_tmu_read(devnode) * MCELSIUS);
		i++;
	}

	return len;
}

static DEVICE_ATTR(temp, S_IRUSR | S_IRGRP, exynos_thermal_sensor_temp, NULL);

#ifdef CONFIG_SEC_BSP
void get_exynos_thermal_curr_temp(int *temp, int size)
{
	struct exynos_tmu_data *devnode;
	int i = 0;

	list_for_each_entry(devnode, &dtm_dev_list, node) {
		temp[i] = exynos_tmu_read(devnode);
		i++;
		if(i == size)
			break;
	}
}
#endif

static ssize_t
exynos_thermal_curr_temp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_tmu_data *devnode;
	unsigned long temp[MAX_TMU_COUNT];
	int i = 0, len = 0;

	list_for_each_entry(devnode, &dtm_dev_list, node) {
		temp[i] = exynos_tmu_read(devnode);
		i++;
	}

	len += snprintf(&buf[len], PAGE_SIZE,"%lu,", temp[0]);
	len += snprintf(&buf[len], PAGE_SIZE,"%lu,", temp[1]);
	len += snprintf(&buf[len], PAGE_SIZE,"%lu,", temp[2]);
	len += snprintf(&buf[len], PAGE_SIZE,"%lu", temp[3]);
	len += snprintf(&buf[len], PAGE_SIZE,"\n");

	return len;
}

static DEVICE_ATTR(curr_temp, S_IRUGO, exynos_thermal_curr_temp, NULL);


static struct attribute *exynos_thermal_sensor_attributes[] = {
	&dev_attr_temp.attr,
	&dev_attr_curr_temp.attr,
	NULL
};

static const struct attribute_group exynos_thermal_sensor_attr_group = {
	.attrs = exynos_thermal_sensor_attributes,
};


static irqreturn_t exynos_tmu_irq(int irq, void *id)
{
	struct exynos_tmu_data *data = id;
	struct exynos_tmu_platform_data *pdata = data->pdata;
	char *name = "interrupt";

	disable_irq_nosync(irq);

	exynos_ss_thermal(pdata, 0, name, 0);
	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}

static const struct of_device_id exynos_tmu_match[] = {
	{
		.compatible = "samsung,exynos3250-tmu",
		.data = (void *)EXYNOS3250_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos4210-tmu",
		.data = (void *)EXYNOS4210_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos4412-tmu",
		.data = (void *)EXYNOS4412_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos5250-tmu",
		.data = (void *)EXYNOS5250_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos5260-tmu",
		.data = (void *)EXYNOS5260_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos5420-tmu",
		.data = (void *)EXYNOS5420_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos5420-tmu-ext-triminfo",
		.data = (void *)EXYNOS5420_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos5440-tmu",
		.data = (void *)EXYNOS5440_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos7580-tmu",
		.data = (void *)EXYNOS7580_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos8890-tmu",
		.data = (void *)EXYNOS8890_TMU_DRV_DATA,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_tmu_match);

static inline struct  exynos_tmu_platform_data *exynos_get_driver_data(
			struct platform_device *pdev, int id)
{
	struct  exynos_tmu_init_data *data_table;
	struct exynos_tmu_platform_data *tmu_data;
	const struct of_device_id *match;

	match = of_match_node(exynos_tmu_match, pdev->dev.of_node);
	if (!match)
		return NULL;
	data_table = (struct exynos_tmu_init_data *) match->data;
	if (!data_table || id >= data_table->tmu_count)
		return NULL;
	tmu_data = data_table->tmu_data;
	return (struct exynos_tmu_platform_data *) (tmu_data + id);
}

static int exynos_map_dt_data(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata;
	struct resource res;
	int ret;
	unsigned int i;
	u32 value;
	char node_name[20];
	struct cpufreq_frequency_table *table_ptr;
	struct isp_fps_table *isp_table_ptr;
	unsigned int table_size;
	u32 gpu_idx_num = 0, isp_idx_num = 0;
	int real_tmuctrl_id = -1;
	int sensor_type = -1;

	if (!data || !pdev->dev.of_node)
		return -ENODEV;

	/*
	 * Try enabling the regulator if found
	 * TODO: Add regulator as an SOC feature, so that regulator enable
	 * is a compulsory call.
	 */
	data->regulator = devm_regulator_get(&pdev->dev, "vtmu");
	if (!IS_ERR(data->regulator)) {
		ret = regulator_enable(data->regulator);
		if (ret) {
			dev_err(&pdev->dev, "failed to enable vtmu\n");
			return ret;
		}
	} else {
		dev_info(&pdev->dev, "Regulator node (vtmu) not found\n");
	}

	data->id = of_alias_get_id(pdev->dev.of_node, "tmuctrl");
	if (data->id < 0)
		data->id = 0;

	data->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (data->irq <= 0) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		return -ENODEV;
	}

	if (of_address_to_resource(pdev->dev.of_node, 0, &res)) {
		dev_err(&pdev->dev, "failed to get Resource 0\n");
		return -ENODEV;
	}
	/* Check senor type (0 : Normal, 1 : Virtual) */
	of_property_read_u32(pdev->dev.of_node, "sensor_type", &sensor_type);
	if (sensor_type < 0)
		dev_err(&pdev->dev, "NO checked sensor type\n");

	if (sensor_type == NORMAL_SENSOR) {
		data->base = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
		if (!data->base) {
			dev_err(&pdev->dev, "Failed to ioremap memory\n");
			return -EADDRNOTAVAIL;
		}
		base_addr[data->id] = (unsigned long)data->base;
	}
	else {
		of_property_read_u32(pdev->dev.of_node, "real_tmuctrl_id", &real_tmuctrl_id);
		data->base = (void __iomem *) base_addr[real_tmuctrl_id];
	}

	pdata = exynos_get_driver_data(pdev, data->id);
	if (!pdata) {
		dev_err(&pdev->dev, "No platform init data supplied.\n");
		return -ENODEV;
	}
	/* Parsing throttling data */
	of_property_read_u32(pdev->dev.of_node, "max_trigger_level", &value);
	if (!value)
		dev_err(&pdev->dev, "No max_trigger_level data\n");
	pdata->max_trigger_level = value;

	of_property_read_u32(pdev->dev.of_node, "non_hw_trigger_levels", &value);
	if (!value)
		dev_err(&pdev->dev, "No non_hw_trigger_levels data\n");
	pdata->non_hw_trigger_levels = value;

	for (i = 0; i < pdata->max_trigger_level; i++) {
		snprintf(node_name, sizeof(node_name), "trigger_levels_%d", i);
		of_property_read_u32(pdev->dev.of_node, node_name, &value);
		if (!value)
			dev_err(&pdev->dev, "No trigger_level data\n");
		pdata->trigger_levels[i] = value;

		snprintf(node_name, sizeof(node_name), "trigger_enable_%d", i);
		of_property_read_u32(pdev->dev.of_node, node_name, &value);
		if (!value)
			dev_err(&pdev->dev, "No trigger_enable data\n");
		pdata->trigger_enable[i] = value;

		snprintf(node_name, sizeof(node_name), "trigger_type_%d", i);
		of_property_read_u32(pdev->dev.of_node, node_name, &value);
		if (!value)
			dev_err(&pdev->dev, "No trigger_type data\n");
		pdata->trigger_type[i] = value;
	}

	/* Parsing cooling data */
	of_property_read_u32(pdev->dev.of_node, "freq_tab_count", &pdata->freq_tab_count);
	for (i =0; i < pdata->freq_tab_count; i++) {
		snprintf(node_name, sizeof(node_name), "freq_clip_max_%d", i);
		of_property_read_u32(pdev->dev.of_node, node_name,
						&pdata->freq_tab[i].freq_clip_max);
		if (!pdata->freq_tab[i].freq_clip_max)
			dev_err(&pdev->dev, "No cooling max freq\n");

		snprintf(node_name, sizeof(node_name), "cooling_level_%d", i);
		of_property_read_u32(pdev->dev.of_node, node_name,
						&pdata->freq_tab[i].temp_level);
		if (!pdata->freq_tab[i].temp_level)
			dev_err(&pdev->dev, "No cooling temp level data\n");
	}

	pdata->hotplug_enable = of_property_read_bool(pdev->dev.of_node, "hotplug_enable");
	if (pdata->hotplug_enable) {
		dev_info(&pdev->dev, "thermal zone use hotplug function \n");
		of_property_read_u32(pdev->dev.of_node, "hotplug_in_threshold", &pdata->hotplug_in_threshold);
		if (!pdata->hotplug_in_threshold)
			dev_err(&pdev->dev, "No input hotplug_in_threshold data\n");
		of_property_read_u32(pdev->dev.of_node, "hotplug_out_threshold", &pdata->hotplug_out_threshold);
		if (!pdata->hotplug_out_threshold)
			dev_err(&pdev->dev, "No input hotplug_out_threshold data\n");
	}

	/* gpu cooling frequency table parse */
	ret = of_property_read_u32(pdev->dev.of_node, "gpu_idx_num", &gpu_idx_num);
	if (gpu_idx_num) {
		table_ptr = kzalloc(sizeof(struct cpufreq_frequency_table)
							* gpu_idx_num, GFP_KERNEL);
		if (table_ptr == NULL) {
			pr_err("%s: failed to allocate for cpufreq_dvfs_table\n", __func__);
			return -ENODEV;
		}

		table_size = sizeof(struct cpufreq_frequency_table) / sizeof(unsigned int);
		ret = of_property_read_u32_array(pdev->dev.of_node, "gpu_cooling_table",
			(unsigned int *)table_ptr, table_size * gpu_idx_num);

		for (i = 0; i < gpu_idx_num; i++) {
			gpu_freq_table[i].flags = table_ptr[i].flags;
			gpu_freq_table[i].driver_data = table_ptr[i].driver_data;
			gpu_freq_table[i].frequency = table_ptr[i].frequency;
			pr_info("[GPU TMU] index : %d, frequency : %d \n", gpu_freq_table[i].driver_data, gpu_freq_table[i].frequency);
		}

		kfree(table_ptr);
	}

	/* isp cooling frequency table parse */
	ret = of_property_read_u32(pdev->dev.of_node, "isp_idx_num", &isp_idx_num);
	if (isp_idx_num) {
		isp_table_ptr = kzalloc(sizeof(struct isp_fps_table) * isp_idx_num, GFP_KERNEL);
		if (isp_table_ptr == NULL) {
			pr_err("%s: failed to allocate for isp_fps_table\n", __func__);
			return -ENODEV;
		}

		table_size = sizeof(struct isp_fps_table) / sizeof(unsigned int);
		ret = of_property_read_u32_array(pdev->dev.of_node, "isp_cooling_table",
			(unsigned int *)isp_table_ptr, table_size * isp_idx_num);

		for (i = 0; i < isp_idx_num; i++) {
			isp_fps_table[i].flags = isp_table_ptr[i].flags;
			isp_fps_table[i].driver_data = isp_table_ptr[i].driver_data;
			isp_fps_table[i].fps = isp_table_ptr[i].fps;
			pr_info("[ISP TMU] index : %d, fps : %d \n", isp_fps_table[i].driver_data, isp_fps_table[i].fps);
		}

		kfree(isp_table_ptr);
	}

	data->pdata = pdata;

	/*
	 * Check if the TMU shares some registers and then try to map the
	 * memory of common registers.
	 */
	if (!TMU_SUPPORTS(pdata, ADDRESS_MULTIPLE))
		return 0;

	if (of_address_to_resource(pdev->dev.of_node, 1, &res)) {
		dev_err(&pdev->dev, "failed to get Resource 1\n");
		return -ENODEV;
	}

	data->base_second = devm_ioremap(&pdev->dev, res.start,
					resource_size(&res));
	if (!data->base_second) {
		dev_err(&pdev->dev, "Failed to ioremap memory\n");
		return -ENOMEM;
	}

	return 0;
}

#if defined(CONFIG_ECT)
static int exynos_tmu_ect_set_information(struct platform_device *pdev)
{
	int i;
	void *thermal_block;
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	struct ect_ap_thermal_function *function;
	int hotplug_threshold = 0, hotplug_flag = 0;

	if (pdata->tmu_name == NULL)
		return 0;

	thermal_block = ect_get_block(BLOCK_AP_THERMAL);
	if (thermal_block == NULL) {
		dev_err(&pdev->dev, "Failed to get thermal block");
		return 0;
	}

	function = ect_ap_thermal_get_function(thermal_block, pdata->tmu_name);
	if (function == NULL) {
		dev_err(&pdev->dev, "Failed to get thermal block %s", pdata->tmu_name);
		return 0;
	}

	/* setting trigger */
	for (i = 0; i < function->num_of_range; ++i) {
		pdata->trigger_levels[i] = function->range_list[i].lower_bound_temperature;
		pdata->trigger_enable[i] = true;

		if (function->range_list[i].sw_trip)
			pdata->trigger_type[i] = SW_TRIP;
		else
			pdata->trigger_type[i] = (i == function->num_of_range - 1 ? HW_TRIP : THROTTLE_ACTIVE);

		pdata->freq_tab[i].temp_level = function->range_list[i].lower_bound_temperature;
		pdata->freq_tab[i].freq_clip_max = function->range_list[i].max_frequency;
		dev_info(&pdev->dev, "[%d] Temp_level : %d, freq_clip_max: %d \n",
			i, pdata->freq_tab[i].temp_level, pdata->freq_tab[i].freq_clip_max);

		if (function->range_list[i].flag != hotplug_flag) {
			hotplug_threshold = pdata->freq_tab[i].temp_level;
			hotplug_flag = function->range_list[i].flag;
			dev_info(&pdev->dev, "[ECT]hotplug_threshold : %d \n", hotplug_threshold);
		}
	}
	pdata->freq_tab_count = function->num_of_range;

	if (pdata->ect_hotplug_flag && hotplug_threshold != 0) {
		pdata->hotplug_enable = true;
		pdata->hotplug_in_threshold = hotplug_threshold
						- pdata->ect_hotplug_interval;
		pdata->hotplug_out_threshold = hotplug_threshold;
		dev_info(&pdev->dev, "[ECT]hotplug_in_threshold : %d \n", pdata->hotplug_in_threshold);
		dev_info(&pdev->dev, "[ECT]hotplug_out_threshold : %d \n", pdata->hotplug_out_threshold);
	} else
		pdata->hotplug_enable = false;

	return 0;
}
#endif
static int exynos_tmu_probe(struct platform_device *pdev)
{
	struct exynos_tmu_data *data;
	struct exynos_tmu_platform_data *pdata;
	struct thermal_sensor_conf *sensor_conf;
	struct exynos_tmu_data *devnode;
	u16 p_temp_error1 = 0, p_temp_error2 = 0;
	u16 p_threshold_falling = 0, p_cal_type = 0;
	int ret, i;

	/* make sure cpufreq driver has been initialized */
	if (!cpufreq_frequency_get_table(0))
		return -EPROBE_DEFER;

	data = devm_kzalloc(&pdev->dev, sizeof(struct exynos_tmu_data),
					GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	ret = exynos_map_dt_data(pdev);
	if (ret)
		return ret;

	pdata = data->pdata;

	INIT_WORK(&data->irq_work, exynos_tmu_work);

	if (pdata->type == SOC_ARCH_EXYNOS3250 ||
	    pdata->type == SOC_ARCH_EXYNOS4210 ||
	    pdata->type == SOC_ARCH_EXYNOS4412 ||
	    pdata->type == SOC_ARCH_EXYNOS5250 ||
	    pdata->type == SOC_ARCH_EXYNOS5260 ||
	    pdata->type == SOC_ARCH_EXYNOS5420_TRIMINFO ||
	    pdata->type == SOC_ARCH_EXYNOS5440 ||
	    pdata->type == SOC_ARCH_EXYNOS7580 ||
	    pdata->type == SOC_ARCH_EXYNOS8890)
		data->soc = pdata->type;
	else {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Platform not supported\n");
		goto err;
	}

#if defined(CONFIG_ECT)
	ret = exynos_tmu_ect_set_information(pdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to ect\n");
		goto err;
	}
#endif

	ret = exynos_tmu_initialize(pdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize TMU\n");
		goto err;
	}

	exynos_tmu_control(pdev, true);

	list_for_each_entry(devnode, &dtm_dev_list, node) {
		if (devnode->pdata->d_type == CLUSTER1) {
			if (devnode->pdata->sensor_type == NORMAL_SENSOR) {
				p_temp_error1 = devnode->temp_error1;
				p_temp_error2 = devnode->temp_error2;
				p_cal_type = devnode->pdata->cal_type;
				p_threshold_falling = devnode->pdata->threshold_falling;
			} else if (devnode->pdata->sensor_type == VIRTUAL_SENSOR) {
				devnode->temp_error1 = p_temp_error1;
				devnode->temp_error2 = p_temp_error2;
				devnode->pdata->cal_type = p_cal_type;
				devnode->pdata->threshold_falling = p_threshold_falling;
			}
		}
	}

	/* Allocate a structure to register with the exynos core thermal */
	sensor_conf = devm_kzalloc(&pdev->dev,
				sizeof(struct thermal_sensor_conf), GFP_KERNEL);
	if (!sensor_conf) {
		ret = -ENOMEM;
		goto err;
	}

	sprintf(sensor_conf->name, "therm_zone%d", data->id);
	/* We currently regard data->id as a cluster id */
	sensor_conf->id = data->id;
	sensor_conf->d_type = pdata->d_type;
	sensor_conf->read_temperature = (int (*)(void *))exynos_tmu_read;
	sensor_conf->write_emul_temp =
		(int (*)(void *, unsigned long))exynos_tmu_set_emulation;
	sensor_conf->driver_data = data;
	sensor_conf->trip_data.trip_count = pdata->trigger_enable[0] +
			pdata->trigger_enable[1] + pdata->trigger_enable[2]+
			pdata->trigger_enable[3] + pdata->trigger_enable[4]+
			pdata->trigger_enable[5] + pdata->trigger_enable[6]+
			pdata->trigger_enable[7];

	/* trip old val mean previous trigger_level index, this value used uevent message */
	sensor_conf->trip_data.trip_old_val = -1;

	for (i = 0; i < sensor_conf->trip_data.trip_count; i++) {
		sensor_conf->trip_data.trip_val[i] =
			pdata->threshold + pdata->trigger_levels[i];
		sensor_conf->trip_data.trip_type[i] =
					pdata->trigger_type[i];
	}

	sensor_conf->trip_data.trigger_falling = pdata->threshold_falling;

	sensor_conf->cooling_data.freq_clip_count = pdata->freq_tab_count;
	for (i = 0; i < pdata->freq_tab_count; i++) {
		sensor_conf->cooling_data.freq_data[i].freq_clip_max =
					pdata->freq_tab[i].freq_clip_max;
		sensor_conf->cooling_data.freq_data[i].temp_level =
					pdata->freq_tab[i].temp_level;
	}

	sensor_conf->hotplug_enable = pdata->hotplug_enable;
	sensor_conf->hotplug_in_threshold = pdata->hotplug_in_threshold;
	sensor_conf->hotplug_out_threshold = pdata->hotplug_out_threshold;

	sensor_conf->dev = &pdev->dev;

#if defined(CONFIG_CPU_THERMAL_IPA)
	if (sensor_count++ == FIRST_SENSOR) {
		ipa_sensor_conf.private_data = sensor_conf->driver_data;
		ipa_register_thermal_sensor(&ipa_sensor_conf);
	}
#endif
	/* Register the sensor with thermal management interface */
	ret = exynos_register_thermal(sensor_conf);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register thermal interface\n");
		goto err;
	}
	data->reg_conf = sensor_conf;

	ret = devm_request_irq(&pdev->dev, data->irq, exynos_tmu_irq,
				IRQF_SHARED, dev_name(&pdev->dev), data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq: %d\n", data->irq);
		goto err;
	}

#ifdef CONFIG_CPU_PM
	if (list_is_singular(&dtm_dev_list))
		exynos_pm_register_notifier(&exynos_low_pwr_nb);
#endif
	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_thermal_sensor_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create thermal sensor attributes\n");
#if defined(CONFIG_CPU_THERMAL_IPA)
	if (pdata->d_type == CLUSTER1 && pdata->sensor_type == NORMAL_SENSOR) {
		pm_qos_add_request(&ipa_cpu_hotplug_request, PM_QOS_CPU_ONLINE_MAX,
					PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
	}
#endif
#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
	if (pdata->d_type == CLUSTER1 && pdata->sensor_type == VIRTUAL_SENSOR) {
		register_cpus_notifier(&exynos_tmu_cpus_nb);
		data->nb.notifier_call = exynos_tmu_cpus_notifier;
		dev_info(&pdev->dev, "Regist exynos tmu notifier\n");
	}
#endif
	return 0;
err:
	return ret;
}

static int exynos_tmu_remove(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_data *devnode;
#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
	struct exynos_tmu_platform_data *pdata = data->pdata;

	if (pdata->d_type == CLUSTER1 && pdata->sensor_type == NORMAL_SENSOR)
		unregister_cpus_notifier(&exynos_tmu_cpus_nb);
#endif

	exynos_unregister_thermal(data->reg_conf);

	exynos_tmu_control(pdev, false);

	if (!IS_ERR(data->regulator))
		regulator_disable(data->regulator);

#ifdef CONFIG_CPU_PM
	if (list_is_singular(&dtm_dev_list))
		exynos_pm_unregister_notifier(&exynos_low_pwr_nb);
#endif

	sysfs_remove_group(&pdev->dev.kobj, &exynos_thermal_sensor_attr_group);

	mutex_lock(&data->lock);
	list_for_each_entry(devnode, &dtm_dev_list, node) {
		if (devnode->id == data->id) {
			list_del(&devnode->node);
			break;
		}
	}
	mutex_unlock(&data->lock);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_tmu_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	disable_irq(data->irq);

	exynos_tmu_control(to_platform_device(dev), false);

	return 0;
}

static int exynos_tmu_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	exynos_tmu_initialize(pdev);
	exynos_tmu_control(pdev, true);

	enable_irq(data->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(exynos_tmu_pm,
			 exynos_tmu_suspend, exynos_tmu_resume);
#define EXYNOS_TMU_PM	(&exynos_tmu_pm)
#else
#define EXYNOS_TMU_PM	NULL
#endif

static struct platform_driver exynos_tmu_driver = {
	.driver = {
		.name   = "exynos-tmu",
		.owner  = THIS_MODULE,
		.pm     = EXYNOS_TMU_PM,
		.of_match_table = exynos_tmu_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_tmu_probe,
	.remove	= exynos_tmu_remove,
};

module_platform_driver(exynos_tmu_driver);

MODULE_DESCRIPTION("EXYNOS TMU Driver");
MODULE_AUTHOR("Donggeun Kim <dg77.kim@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:exynos-tmu");
