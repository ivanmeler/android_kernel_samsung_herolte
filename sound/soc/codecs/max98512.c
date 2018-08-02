/*
 * max98512.c -- MAX98512 ALSA Soc Audio driver
 *
 * Copyright (C) 2017 Maxim Integrated Products
 * Author: Ryan Lee <ryans.lee@maximintegrated.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sound/tlv.h>
#include "max98512.h"
#ifdef CONFIG_SND_SOC_MAXIM_DSM
#include <sound/maxim_dsm_cal.h>
#endif /* CONFIG_SND_SOC_MAXIM_DSM_CAL */

#define DEBUG_MAX98512
#ifdef DEBUG_MAX98512
#define msg_maxim(format, args...) \
	pr_info("[MAX98512_DEBUG] %s: " format "\n", __func__, ## args)
#else
#define msg_maxim(format, args...)
#endif /* DEBUG_MAX98512 */

static struct reg_default max98512_reg[] = {
	{MAX98512_R0001_INT_RAW1, 0x00},
	{MAX98512_R0002_INT_RAW2, 0x00},
	{MAX98512_R0003_INT_RAW3, 0x00},
	{MAX98512_R0004_INT_STATE1, 0x00},
	{MAX98512_R0005_INT_STATE2, 0x00},
	{MAX98512_R0006_INT_STATE3, 0x00},
	{MAX98512_R0007_INT_FLAG1, 0x00},
	{MAX98512_R0008_INT_FLAG2, 0x00},
	{MAX98512_R0009_INT_FLAG3, 0x00},
	{MAX98512_R000A_INT_EN1, 0x00},
	{MAX98512_R000B_INT_EN2, 0x00},
	{MAX98512_R000C_INT_EN3, 0x00},
	{MAX98512_R000D_INT_FLAG_CLR1, 0x00},
	{MAX98512_R000E_INT_FLAG_CLR2, 0x00},
	{MAX98512_R000F_INT_FLAG_CLR3, 0x00},
	{MAX98512_R0010_IRQ_CTRL, 0x00},
	{MAX98512_R0011_CLK_MON, 0x00},
	{MAX98512_R0012_WDOG_CTRL, 0x00},
	{MAX98512_R0013_WDOG_RST, 0x00},
	{MAX98512_R0014_MEAS_ADC_THERM_WARN_THRESH, 0x75},
	{MAX98512_R0015_MEAS_ADC_THERM_SHDN_THRESH, 0x8C},
	{MAX98512_R0016_MEAS_ADC_THERM_HYSTERESIS, 0x08},
	{MAX98512_R0017_PIN_CFG, 0x55},
	{MAX98512_R0018_PCM_RX_EN_A, 0x00},
	{MAX98512_R0019_PCM_RX_EN_B, 0x00},
	{MAX98512_R001A_PCM_TX_EN_A, 0x00},
	{MAX98512_R001B_PCM_TX_EN_B, 0x00},
	{MAX98512_R001C_PCM_TX_HIZ_CTRL_A, 0x00},
	{MAX98512_R001D_PCM_TX_HIZ_CTRL_B, 0x00},
	{MAX98512_R001E_PCM_TX_CH_SRC_A, 0x00},
	{MAX98512_R001F_PCM_TX_CH_SRC_B, 0x00},
	{MAX98512_R0020_PCM_MODE_CFG, 0x22},
	{MAX98512_R0021_PCM_MASTER_MODE, 0x00},
	{MAX98512_R0022_PCM_CLK_SETUP, 0x22},
	{MAX98512_R0023_PCM_SR_SETUP1, 0x00},
	{MAX98512_R0024_PCM_SR_SETUP2, 0x00},
	{MAX98512_R0025_PCM_TO_SPK_MONOMIX_A, 0x00},
	{MAX98512_R0026_PCM_TO_SPK_MONOMIX_B, 0x00},
	{MAX98512_R0027_ICC_RX_EN_A, 0x00},
	{MAX98512_R0028_ICC_RX_EN_B, 0x00},
	{MAX98512_R002B_ICC_TX_EN_A, 0x00},
	{MAX98512_R002C_ICC_TX_EN_B, 0x00},
	{MAX98512_R002D_ICC_HIZ_MANUAL_MODE, 0x00},
	{MAX98512_R002E_ICC_TX_HIZ_EN_A, 0x00},
	{MAX98512_R002F_ICC_TX_HIZ_EN_B, 0x00},
	{MAX98512_R0030_ICC_LNK_EN, 0x00},
	{MAX98512_R0031_PDM_TX_EN, 0x00},
	{MAX98512_R0032_PDM_TX_HIZ_CTRL, 0x00},
	{MAX98512_R0033_PDM_TX_CTRL, 0x00},
	{MAX98512_R0034_PDM_RX_CTRL, 0x40},
	{MAX98512_R0035_AMP_VOL_CTRL, 0x00},
	{MAX98512_R0036_AMP_DSP_CFG, 0x02},
	{MAX98512_R0037_TONE_GEN_DC_CFG, 0x00},
	{MAX98512_R0038_AMP_EN, 0x00},
	{MAX98512_R0039_SPK_SRC_SEL, 0x00},
	{MAX98512_R003A_SPK_GAIN, 0x00},
	{MAX98512_R003B_SSM_CFG, 0x01},
	{MAX98512_R003C_MEAS_EN, 0x00},
	{MAX98512_R003D_MEAS_DSP_CFG, 0x04},
	{MAX98512_R003E_BOOST_CTRL0, 0x00},
	{MAX98512_R003F_BOOST_CTRL3, 0x00},
	{MAX98512_R0040_BOOST_CTRL1, 0x00},
	{MAX98512_R0041_MEAS_ADC_CFG, 0x00},
	{MAX98512_R0042_MEAS_ADC_BASE_MSB, 0x00},
	{MAX98512_R0043_MEAS_ADC_BASE_LSB, 0x00},
	{MAX98512_R0044_ADC_CH0_DIVIDE, 0x00},
	{MAX98512_R0045_ADC_CH1_DIVIDE, 0x00},
	{MAX98512_R0046_ADC_CH2_DIVIDE, 0x00},
	{MAX98512_R0047_ADC_CH0_FILT_CFG, 0x00},
	{MAX98512_R0048_ADC_CH1_FILT_CFG, 0x00},
	{MAX98512_R0049_ADC_CH2_FILT_CFG, 0x00},
	{MAX98512_R004A_MEAS_ADC_CH0_READ, 0x00},
	{MAX98512_R004B_MEAS_ADC_CH1_READ, 0x00},
	{MAX98512_R004C_MEAS_ADC_CH2_READ, 0x00},
	{MAX98512_R004E_SQUELCH, 0x15},
	{MAX98512_R004F_BROWNOUT_STATUS, 0x00},
	{MAX98512_R0050_BROWNOUT_EN, 0x00},
	{MAX98512_R0051_BROWNOUT_INFINITE_HOLD, 0x00},
	{MAX98512_R0052_BROWNOUT_INFINITE_HOLD_CLR, 0x00},
	{MAX98512_R0053_BROWNOUT_LVL_HOLD, 0x00},
	{MAX98512_R0058_BROWNOUT_LVL1_THRESH, 0x00},
	{MAX98512_R0059_BROWNOUT_LVL2_THRESH, 0x00},
	{MAX98512_R005A_BROWNOUT_LVL3_THRESH, 0x00},
	{MAX98512_R005B_BROWNOUT_LVL4_THRESH, 0x00},
	{MAX98512_R005C_BROWNOUT_THRESH_HYSTERYSIS, 0x00},
	{MAX98512_R005D_BROWNOUT_AMP_LIMITER_ATK_REL, 0x00},
	{MAX98512_R005E_BROWNOUT_AMP_GAIN_ATK_REL, 0x00},
	{MAX98512_R005F_BROWNOUT_AMP1_CLIP_MODE, 0x00},
	{MAX98512_R0070_BROWNOUT_LVL1_CUR_LIMIT, 0x00},
	{MAX98512_R0071_BROWNOUT_LVL1_AMP1_CTRL1, 0x00},
	{MAX98512_R0072_BROWNOUT_LVL1_AMP1_CTRL2, 0x00},
	{MAX98512_R0073_BROWNOUT_LVL1_AMP1_CTRL3, 0x00},
	{MAX98512_R0074_BROWNOUT_LVL2_CUR_LIMIT, 0x00},
	{MAX98512_R0075_BROWNOUT_LVL2_AMP1_CTRL1, 0x00},
	{MAX98512_R0076_BROWNOUT_LVL2_AMP1_CTRL2, 0x00},
	{MAX98512_R0077_BROWNOUT_LVL2_AMP1_CTRL3, 0x00},
	{MAX98512_R0078_BROWNOUT_LVL3_CUR_LIMIT, 0x00},
	{MAX98512_R0079_BROWNOUT_LVL3_AMP1_CTRL1, 0x00},
	{MAX98512_R007A_BROWNOUT_LVL3_AMP1_CTRL2, 0x00},
	{MAX98512_R007B_BROWNOUT_LVL3_AMP1_CTRL3, 0x00},
	{MAX98512_R007C_BROWNOUT_LVL4_CUR_LIMIT, 0x00},
	{MAX98512_R007D_BROWNOUT_LVL4_AMP1_CTRL1, 0x00},
	{MAX98512_R007E_BROWNOUT_LVL4_AMP1_CTRL2, 0x00},
	{MAX98512_R007F_BROWNOUT_LVL4_AMP1_CTRL3, 0x00},
	{MAX98512_R0080_ENV_TRACK_VOUT_HEADROOM, 0x00},
	{MAX98512_R0081_ENV_TRACK_BOOST_VOUT_DELAY, 0x00},
	{MAX98512_R0082_ENV_TRACK_REL_RATE, 0x00},
	{MAX98512_R0083_ENV_TRACK_HOLD_RATE, 0x00},
	{MAX98512_R0084_ENV_TRACK_CTRL, 0x00},
	{MAX98512_R0085_ENV_TRACK_BOOST_VOUT_READ, 0x00},
	{MAX98512_R0086_BOOST_BYPASS_1,  0x00},
	{MAX98512_R0087_BOOST_BYPASS_2,  0x00},
	{MAX98512_R0088_BOOST_BYPASS_3,  0x00},
	{MAX98512_R0400_GLOBAL_SHDN, 0x00},
	{MAX98512_R0401_SOFT_RESET, 0x00},
	{MAX98512_R0402_REV_ID, 0x40},
};

int max98512_regmap_read(struct regmap *map,
			 unsigned int reg, unsigned int *val)
{
	int ret = 0;

	if (map)
		ret = regmap_read(map, reg, val);

	return ret;
}

int max98512_regmap_write(struct regmap *map,
			  unsigned int reg, unsigned int val)
{
	int ret = 0;

	if (map)
		ret = regmap_write(map, reg, val);

	return ret;
}

int max98512_wrapper_read(struct max98512_priv *max98512,
			  int speaker, unsigned int reg, unsigned int *val)
{
	int ret = -999;
	int count = 0;

	while (count++ < MAX_TRY_COUNT && ret != 0) {
		switch (speaker) {
		case MAX98512L:
			ret = max98512_regmap_read(max98512->regmap_l,
						   reg, val);
			break;
		case MAX98512R:
			if (max98512->pdata->sub_reg)
				ret = max98512_regmap_read(max98512->regmap_r,
							   reg, val);
			break;
		case MAX98512B:
			ret = max98512_regmap_read(max98512->regmap_l,
						   reg, val);
			if (max98512->pdata->sub_reg)
				ret = max98512_regmap_read(max98512->regmap_r,
							   reg, val);
			break;
		case MAX98512T:
			if (max98512->pdata->third_reg)
				ret = max98512_regmap_read(max98512->regmap_third,
							   reg, val);
			break;
		case MAX98512A:
			ret = max98512_regmap_read(max98512->regmap_l,
						   reg, val);
			if (max98512->pdata->sub_reg)
				ret = max98512_regmap_read(max98512->regmap_r,
							   reg, val);
			if (max98512->pdata->third_reg)
				ret = max98512_regmap_read(max98512->regmap_third,
							   reg, val);
			break;
		default:
			msg_maxim("Unknown type %d", speaker);
			break;
		}
		if (!ret)
			return ret;
	}

	dev_err(max98512->i2c_dev, "Failed to read [0x%04x][%d]", reg, ret);

	return ret;
}

int max98512_wrapper_write(struct max98512_priv *max98512,
			   int speaker, unsigned int reg, unsigned int val)
{
	int ret = -999;
	int count = 0;

	while (count++ < MAX_TRY_COUNT && ret != 0) {
		switch (speaker) {
		case MAX98512L:
			ret = max98512_regmap_write(max98512->regmap_l,
						    reg, val);
			break;
		case MAX98512R:
			if (max98512->pdata->sub_reg)
				ret = max98512_regmap_write(max98512->regmap_r,
							    reg, val);
			break;
		case MAX98512B:
			ret = max98512_regmap_write(max98512->regmap_l,
						    reg, val);
			if (max98512->pdata->sub_reg)
				ret = max98512_regmap_write(max98512->regmap_r,
							    reg, val);
			break;
		case MAX98512T:
			if (max98512->pdata->third_reg)
				ret = max98512_regmap_write(max98512->regmap_third,
							    reg, val);
			break;
		case MAX98512A:
			ret = max98512_regmap_write(max98512->regmap_l,
						    reg, val);
			if (max98512->pdata->sub_reg)
				ret = max98512_regmap_write(max98512->regmap_r,
							    reg, val);
			if (max98512->pdata->third_reg)
				ret = max98512_regmap_write(max98512->regmap_third,
							    reg, val);
			break;
		default:
			msg_maxim("Unknown type %d", speaker);
			break;
		}
		if (!ret)
			return ret;
	}

	dev_err(max98512->i2c_dev,
		"Failed to write [0x%04x, 0x%02x][%d]", reg, val, ret);

	return ret;
}

int max98512_wrapper_update(struct max98512_priv *max98512,
			    int speaker, unsigned int reg, unsigned int mask,
			    unsigned int val)
{
	int ret = -999;
	int count = 0;

	while (count++ < MAX_TRY_COUNT && ret != 0) {
		switch (speaker) {
		case MAX98512L:
			ret = regmap_update_bits(max98512->regmap_l,
						 reg, mask, val);
			break;
		case MAX98512R:
			if (max98512->pdata->sub_reg)
				ret = regmap_update_bits(max98512->regmap_r,
							 reg, mask, val);
			break;
		case MAX98512B:
			ret = regmap_update_bits(max98512->regmap_l,
						 reg, mask, val);
			if (max98512->pdata->sub_reg)
				ret = regmap_update_bits(max98512->regmap_r,
							 reg, mask, val);
			break;
		case MAX98512T:
			if (max98512->pdata->third_reg)
				ret = regmap_update_bits(max98512->regmap_third,
							 reg, mask, val);
			break;
		case MAX98512A:
			ret = regmap_update_bits(max98512->regmap_l,
						 reg, mask, val);
			if (max98512->pdata->sub_reg)
				ret = regmap_update_bits(max98512->regmap_r,
							 reg, mask, val);
			if (max98512->pdata->third_reg)
				ret = regmap_update_bits(max98512->regmap_third,
							 reg, mask, val);
			break;
		default:
			msg_maxim("Unknown type %d", speaker);
		}
		if (!ret)
			return ret;
	}

	dev_err(max98512->i2c_dev,
		"Failed to update [0x%04x, 0x%02x][%d]", reg, val, ret);

	return ret;
}

#ifdef CONFIG_SND_SOC_MAXIM_DSM
#ifdef USE_DSM_LOG
static int max98512_get_dump_status(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = maxdsm_get_dump_status();
	return 0;
}

static int max98512_set_dump_status(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);

	int val = 0;

	max98512_wrapper_read(max98512, MAX98512L,
			      MAX98512_R0400_GLOBAL_SHDN, &val);
	msg_maxim("val: %d", val);

	if (val != 0)
		maxdsm_update_param();

	return 0;
}

static ssize_t max98512_log_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return maxdsm_log_prepare(buf, LOG_LEFT);
}
static DEVICE_ATTR(dsm_log, 0444, max98512_log_show, NULL);
#endif /* USE_DSM_LOG */

#ifdef USE_DSM_UPDATE_CAL
static int max98512_get_dsm_param(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = maxdsm_cal_avail();
	return 0;
}

static int max98512_set_dsm_param(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	maxdsm_update_caldata(ucontrol->value.integer.value[0]);
	return 0;
}

static ssize_t max98512_cal_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return maxdsm_cal_prepare(buf);
}
static DEVICE_ATTR(dsm_cal, 0444, max98512_cal_show, NULL);
#endif /* USE_DSM_UPDATE_CAL */

#if defined(USE_DSM_LOG) || defined(USE_DSM_UPDATE_CAL)
#define DEFAULT_LOG_CLASS_NAME "dsm"
static const char *class_name_log = DEFAULT_LOG_CLASS_NAME;

static struct attribute *max98512_attributes[] = {
#ifdef USE_DSM_LOG
	&dev_attr_dsm_log.attr,
#endif /* USE_DSM_LOG */
#ifdef USE_DSM_UPDATE_CAL
	&dev_attr_dsm_cal.attr,
#endif /* USE_DSM_UPDATE_CAL */
	NULL
};

static struct attribute_group max98512_attribute_group = {
	.attrs = max98512_attributes
};
#endif /* USE_DSM_LOG || USE_DSM_UPDATE_CAL */
#endif /* CONFIG_SND_SOC_MAXIM_DSM */

static int max98512_dai_set_fmt(struct snd_soc_dai *codec_dai,
				unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	unsigned int mode = 0;
	unsigned int pcm_fmt = 0;
	bool pdm_fmt = false;
	unsigned int invert = 0;

	msg_maxim("%s: fmt 0x%08X\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		mode = MAX98512_PCM_MASTER_MODE_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		max98512->master = true;
		mode = MAX98512_PCM_MASTER_MODE_MASTER;
		break;
	default:
		dev_err(codec->dev, "DAI clock mode unsupported");
		return -EINVAL;
	}

	max98512_wrapper_update(max98512, MAX98512A,
				MAX98512_R0021_PCM_MASTER_MODE,
				MAX98512_PCM_MASTER_MODE_MASK,
				mode);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		invert = MAX98512_PCM_MODE_CFG_PCM_BCLKEDGE;
		break;
	default:
		dev_err(codec->dev, "DAI invert mode unsupported");
		return -EINVAL;
	}

	max98512_wrapper_update(max98512, MAX98512A,
				MAX98512_R0020_PCM_MODE_CFG,
				MAX98512_PCM_MODE_CFG_PCM_BCLKEDGE,
				invert);

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		pcm_fmt = MAX98512_PCM_FORMAT_I2S;
		pdm_fmt = false;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		pcm_fmt = MAX98512_PCM_FORMAT_LJ;
		pdm_fmt = false;
		break;
	case SND_SOC_DAIFMT_PDM:
		pcm_fmt = 0;
		pdm_fmt = true;
		break;
	default:
		return -EINVAL;
	}

	if (pdm_fmt) {
		/* pdm channel configuration */
		max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0034_PDM_RX_CTRL,
					MAX98512_PDM_RX_EN_MASK, 1);

		max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0039_SPK_SRC_SEL,
					MAX98512_SPK_SRC_MASK,
					MAX98512_SPK_SOURCE_PDM_IN);

		max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0018_PCM_RX_EN_A,
					MAX98512_PCM_RX_CH0_EN |
					MAX98512_PCM_RX_CH1_EN, 0);
	} else {
		max98512_wrapper_update(max98512, MAX98512B,
					MAX98512_R0018_PCM_RX_EN_A,
					MAX98512_PCM_RX_CH0_EN |
					MAX98512_PCM_RX_CH1_EN,
					MAX98512_PCM_RX_CH0_EN |
					MAX98512_PCM_RX_CH1_EN);

		max98512_wrapper_update(max98512, MAX98512B,
					MAX98512_R0020_PCM_MODE_CFG,
					MAX98512_PCM_MODE_CFG_FORMAT_MASK,
					pcm_fmt <<
					MAX98512_PCM_MODE_CFG_FORMAT_SHIFT);

		max98512_wrapper_update(max98512, MAX98512B,
					MAX98512_R0039_SPK_SRC_SEL,
					MAX98512_SPK_SRC_MASK,
					MAX98512_SPK_SOURCE_DIGITAL);

		max98512_wrapper_update(max98512, MAX98512B,
					MAX98512_R0034_PDM_RX_CTRL,
					MAX98512_PDM_RX_EN_MASK, 0);
	}
	return 0;
}

/* codec MCLK rate in master mode */
static const int rate_table[] = {
	5644800, 6000000, 6144000, 6500000,
	9600000, 11289600, 12000000, 12288000,
	13000000, 19200000,
};

static int max98512_set_clock(struct max98512_priv *max98512,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_codec *codec = max98512->codec;
	/* BCLK/LRCLK ratio calculation */
	int blr_clk_ratio = params_channels(params) * max98512->ch_size;
	int value;

	if (max98512->master) {
		int i;
		/* match rate to closest value */
		for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
			if (rate_table[i] >= max98512->sysclk)
				break;
		}
		if (i == ARRAY_SIZE(rate_table)) {
			dev_err(codec->dev,
				"failed to find proper clock rate.\n");
			return -EINVAL;
		}
		max98512_wrapper_update(max98512, MAX98512B,
				MAX98512_R0021_PCM_MASTER_MODE,
				MAX98512_PCM_MASTER_MODE_MCLK_MASK,
				i << MAX98512_PCM_MASTER_MODE_MCLK_RATE_SHIFT);
	}

	switch (blr_clk_ratio) {
	case 32:
		value = 2;
		break;
	case 48:
		value = 3;
		break;
	case 64:
		value = 4;
		break;
	default:
		return -EINVAL;
	}
	max98512_wrapper_update(max98512, MAX98512B,
				MAX98512_R0022_PCM_CLK_SETUP,
				MAX98512_PCM_CLK_SETUP_BSEL_MASK,
				value);
	return 0;
}

static int max98512_dai_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	unsigned int sampling_rate = 0;
	unsigned int chan_sz = 0;

	/* pcm mode configuration */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		chan_sz = MAX98512_PCM_MODE_CFG_CHANSZ_16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		chan_sz = MAX98512_PCM_MODE_CFG_CHANSZ_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		chan_sz = MAX98512_PCM_MODE_CFG_CHANSZ_32;
		break;
	default:
		dev_err(codec->dev, "format unsupported %d",
			params_format(params));
		goto err;
	}

	max98512->ch_size = snd_pcm_format_width(params_format(params));

	msg_maxim("channel size %d, bit width %d", chan_sz, max98512->ch_size);

	max98512_wrapper_update(max98512, MAX98512B,
				MAX98512_R0020_PCM_MODE_CFG,
				MAX98512_PCM_MODE_CFG_CHANSZ_MASK, chan_sz);

	msg_maxim("format supported %d", params_format(params));

	/* sampling rate configuration */
	switch (params_rate(params)) {
	case 8000:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_8000;
		break;
	case 11025:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_11025;
		break;
	case 12000:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_12000;
		break;
	case 16000:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_16000;
		break;
	case 22050:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_22050;
		break;
	case 24000:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_24000;
		break;
	case 32000:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_32000;
		break;
	case 44100:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_44100;
		break;
	case 48000:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_48000;
		break;
	case 88200:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_88200;
		break;
	case 96000:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_96000;
		break;
	case 176400:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_176400;
		break;
	case 192000:
		sampling_rate = MAX98512_PCM_SR_SET1_SR_192000;
		break;
	default:
		dev_err(codec->dev, "rate %d not supported\n",
			params_rate(params));
		goto err;
	}

	msg_maxim("sample rate is %d, apply %d",
		  params_rate(params), sampling_rate);

	/* set DAI_SR to correct LRCLK frequency */
	max98512_wrapper_update(max98512, MAX98512A,
				MAX98512_R0023_PCM_SR_SETUP1,
				MAX98512_PCM_SR_SET1_SR_MASK,
				sampling_rate);
	max98512_wrapper_update(max98512, MAX98512A,
				MAX98512_R0024_PCM_SR_SETUP2,
				MAX98512_PCM_SR_SET2_SR_MASK,
				sampling_rate << MAX98512_PCM_SR_SET2_SR_SHIFT);

	/* set sampling rate of IV */
	max98512_wrapper_update(max98512, MAX98512T,
				MAX98512_R0024_PCM_SR_SETUP2,
				MAX98512_PCM_SR_SET2_IVADC_SR_MASK,
				sampling_rate);
	max98512_wrapper_update(max98512, MAX98512B,
				MAX98512_R0024_PCM_SR_SETUP2,
				MAX98512_PCM_SR_SET2_IVADC_SR_MASK,
				sampling_rate - 3);
	return max98512_set_clock(max98512, params);
err:
	return -EINVAL;
}

#define MAX98512_RATES SNDRV_PCM_RATE_8000_48000

#define MAX98512_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static int max98512_dai_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				   unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);

	msg_maxim("clk_id %d, freq %d, dir %d", clk_id, freq, dir);
	max98512->sysclk = freq;
	return 0;
}

static int __max98512_spk_enable(struct max98512_priv *max98512)
{
	struct max98512_pdata *pdata = max98512->pdata;
	struct max98512_volume_step_info *vstep = &max98512->vstep;
	unsigned int gain_l, gain_r, gain_t;
	unsigned int digital_gain_l, digital_gain_r, digital_gain_t;
	unsigned int enable_l, enable_r, enable_t;
	unsigned int dem_l, dem_r, dem_t;
	unsigned int vimon = 0;
#ifdef CONFIG_SND_SOC_MAXIM_DSM
		unsigned int smode_table[MAX98512_OSM_MAX] = {
			2, /* MAX98512_OSM_MONO_L */
			1, /* MAX98512_OSM_MONO_R */
			2, /* MAX98512_OSM_RCV_L */
			1, /* MAX98512_OSM_RCV_R */
			0, /* MAX98512_OSM_STEREO */
			3, /* MAX98512_OSM_STEREO_MODE2 */
		};
#endif /* CONFIG_SND_SOC_MAXIM_DSM */

	gain_l = gain_r = max98512->spk_gain;
	gain_t = max98512->pdm_gain;

	digital_gain_l = digital_gain_r = digital_gain_t = max98512->digital_gain;

	enable_l = enable_r = enable_t = 0x00;
	dem_l = dem_r = dem_t = 0x80;

	if (vstep->adc_status && !pdata->nodsm)
		vimon = MAX98512_MEAS_VI_EN;

	switch (pdata->osm) {
	case MAX98512_OSM_STEREO_MODE2:
	case MAX98512_OSM_STEREO:
		enable_l = enable_r = enable_t = 1;
		break;
	case MAX98512_OSM_RCV_L:
	case MAX98512_OSM_RCV_R:
	case MAX98512_OSM_MONO_L:
	case MAX98512_OSM_MONO_R:
		digital_gain_t = max98512->digital_gain_rcv;
		vimon = 0; /* turn off VIMON */
		enable_t = 1;
		break;
	default:
		msg_maxim("Invalid one_stop_mode");
		return -EINVAL;
	}

#ifdef CONFIG_SND_SOC_MAXIM_DSM
	maxdsm_set_stereo_mode_configuration(smode_table[pdata->osm]);
#endif /* CONFIG_SND_SOC_MAXIM_DSM */

	msg_maxim("Gain[%d][%d] Enable[%d][%d] OSM[%d]",
		  gain_l, gain_r, enable_l, enable_r, pdata->osm);

	max98512_wrapper_update(max98512, MAX98512L,
				MAX98512_R003A_SPK_GAIN,
				MAX98512_SPK_PCM_GAIN_MASK,
				gain_l);
	max98512_wrapper_update(max98512, MAX98512L,
				MAX98512_R0035_AMP_VOL_CTRL,
				MAX98512_AMP_VOL_MASK,
				digital_gain_l);
	max98512_wrapper_update(max98512, MAX98512R,
				MAX98512_R003A_SPK_GAIN,
				MAX98512_SPK_PCM_GAIN_MASK,
				gain_r);
	max98512_wrapper_update(max98512, MAX98512R,
				MAX98512_R0035_AMP_VOL_CTRL,
				MAX98512_AMP_VOL_MASK,
				digital_gain_r);

	max98512_wrapper_update(max98512, MAX98512T,
				MAX98512_R003A_SPK_GAIN,
				MAX98512_SPK_PDM_GAIN_MASK,
				gain_t << MAX98512_PDM_GAIN_SHIFT);
	max98512_wrapper_update(max98512, MAX98512T,
				MAX98512_R0035_AMP_VOL_CTRL,
				MAX98512_AMP_VOL_MASK,
				digital_gain_t);

	max98512_wrapper_update(max98512, MAX98512A,
				MAX98512_R003C_MEAS_EN,
				MAX98512_MEAS_VI_EN,
				vimon);
	vstep->adc_status = !!vimon;

	max98512_wrapper_update(max98512, MAX98512L,
				MAX98512_R0038_AMP_EN,
				MAX98512_AMP_EN_MASK, enable_l);
	max98512_wrapper_update(max98512, MAX98512L,
				MAX98512_R0400_GLOBAL_SHDN,
				MAX98512_GLOBAL_EN_MASK, enable_l);
	max98512_wrapper_update(max98512, MAX98512R,
				MAX98512_R0038_AMP_EN,
				MAX98512_AMP_EN_MASK, enable_r);
	max98512_wrapper_update(max98512, MAX98512R,
				MAX98512_R0400_GLOBAL_SHDN,
				MAX98512_GLOBAL_EN_MASK, enable_r);
	max98512_wrapper_update(max98512, MAX98512T,
				MAX98512_R0038_AMP_EN,
				MAX98512_AMP_EN_MASK, enable_t);
	max98512_wrapper_update(max98512, MAX98512T,
				MAX98512_R0400_GLOBAL_SHDN,
				MAX98512_GLOBAL_EN_MASK, enable_t);

	return 0;
}

static void max98512_spk_enable(struct snd_soc_dai *codec_dai, struct max98512_priv *max98512, int enable)
{
	if (enable)
		__max98512_spk_enable(max98512);
	else {
		if (strcmp(codec_dai->name, "max98512-aif1") == 0) {
			max98512_wrapper_update(max98512, MAX98512B,
						MAX98512_R0400_GLOBAL_SHDN,
						MAX98512_GLOBAL_EN_MASK,
						0);
			max98512_wrapper_update(max98512, MAX98512B,
						MAX98512_R0038_AMP_EN,
						MAX98512_AMP_EN_MASK,
						0);
			/* disable the v and i for vi feedback */
			max98512_wrapper_update(max98512, MAX98512B,
						MAX98512_R003C_MEAS_EN,
						MAX98512_MEAS_VI_EN,
						0);
		} else if (strcmp(codec_dai->name, "max98512-aif-pdm") == 0) {
			max98512_wrapper_update(max98512, MAX98512T,
						MAX98512_R0400_GLOBAL_SHDN,
						MAX98512_GLOBAL_EN_MASK,
						0);
			max98512_wrapper_update(max98512, MAX98512T,
						MAX98512_R0038_AMP_EN,
						MAX98512_AMP_EN_MASK,
						0);
			/* disable the v and i for vi feedback */
			max98512_wrapper_update(max98512, MAX98512T,
						MAX98512_R003C_MEAS_EN,
						MAX98512_MEAS_VI_EN,
						0);
		}
	}

#ifdef CONFIG_SND_SOC_MAXIM_DSM
	maxdsm_set_spk_state(enable);
#endif /* CONFIG_SND_SOC_MAXIM_DSM */
}

static int max98512_dai_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct max98512_priv *max98512
		= snd_soc_codec_get_drvdata(codec_dai->codec);
	bool action = 1;

	if (max98512->pca.capture_active != codec_dai->capture_active) {
		max98512->pca.capture_active = codec_dai->capture_active;
		action = 0;
	}

	if (max98512->pca.playback_active != codec_dai->playback_active) {
		max98512->pca.playback_active = codec_dai->playback_active;
		action = 1;
	}

	if ((mute && codec_dai->playback_active) && action)
		action = 0;

	msg_maxim("mute=%d playback_active=%d capture_active=%d action=%d",
			mute, max98512->pca.playback_active,
			max98512->pca.capture_active, action);

	if (action)
		max98512_spk_enable(codec_dai, max98512, mute != 0 ? 0 : 1);

#ifdef USE_REG_DUMP
	if (action)
		reg_dump(max98512);
#endif /* USE_REG_DUMP */

	return 0;
}

static const struct snd_soc_dai_ops max98512_dai_ops = {
	.set_sysclk = max98512_dai_set_sysclk,
	.set_fmt = max98512_dai_set_fmt,
	.hw_params = max98512_dai_hw_params,
	.digital_mute = max98512_dai_digital_mute,
};

static DECLARE_TLV_DB_SCALE(max98512_spk_tlv, 300, 300, 0);
static DECLARE_TLV_DB_SCALE(max98512_digital_tlv, -1600, 25, 0);
static DECLARE_TLV_DB_SCALE(max98512_pdm_tlv, 300, 300, 0);

static int pdm_l_zero;
static int pdm_l_one;

int max98512_common_pdm(struct max98512_priv *max98512,
			int channel, int source)
{
	int ret = 0;
	/* enable the pdm receive interface */
	max98512_wrapper_update(max98512, MAX98512T,
				MAX98512_R0034_PDM_RX_CTRL,
				MAX98512_PDM_RX_EN_MASK,
				MAX98512_PDM_RX_EN_MASK);

	/* enable the pdm transmit interface */
	max98512_wrapper_update(max98512, MAX98512T,
				MAX98512_R0031_PDM_TX_EN,
				MAX98512_PDM_TX_ENABLES_PDM_TX_EN,
				MAX98512_PDM_TX_ENABLES_PDM_TX_EN);

	/* enable channel */
	if (channel == 0) {
		max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0034_PDM_RX_CTRL,
					MAX98512_PDM_RX_EN_MASK, 0);
	} else {
		max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0034_PDM_RX_CTRL,
					MAX98512_PDM_RX_EN_MASK,
					MAX98512_PDM_RX_EN_MASK);
	}
	switch (source) {
	case 0:
		/* enable source*/
		if (channel == 1) {
			max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0033_PDM_TX_CTRL,
					MAX98512_PDM_TX_CTRL_PDM_TX_CH1_SRC,
					MAX98512_PDM_TX_CTRL_PDM_TX_CH1_SRC);
		} else {
			max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0033_PDM_TX_CTRL,
					MAX98512_PDM_TX_CTRL_PDM_TX_CH0_SRC,
					MAX98512_PDM_TX_CTRL_PDM_TX_CH0_SRC);
		}
		break;
	case 1:
		/* enable source*/
		if (channel == 1) {
			max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0033_PDM_TX_CTRL,
					MAX98512_PDM_TX_CTRL_PDM_TX_CH1_SRC, 0);
		} else {
			max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R0033_PDM_TX_CTRL,
					MAX98512_PDM_TX_CTRL_PDM_TX_CH0_SRC, 0);
		}
		break;
	default:
		dev_err(max98512->i2c_dev, "%s no source found %d\n",
			__func__, source);
		return -EINVAL;
	}

	return ret;
}

static int max98512_get_pdm_l_zero(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = pdm_l_zero;

	return 0;
}

static int max98512_put_pdm_l_zero(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	int ret;

	pdm_l_zero = ucontrol->value.integer.value[0];
	ret = max98512_common_pdm(max98512, 0, pdm_l_zero);
	if (ret < 0)
		return -EINVAL;

	return 0;
}

static int max98512_get_pdm_l_one(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = pdm_l_one;

	return 0;
}

static int max98512_put_pdm_l_one(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	int ret;

	pdm_l_one = ucontrol->value.integer.value[0];
	ret = max98512_common_pdm(max98512, 1, pdm_l_one);
	if (ret < 0)
		return -EINVAL;

	return 0;
}

static const char *const pdm_1_text[] = {
	"Current", "Voltage",
};

static const char *const pdm_0_text[] = {
	"Current", "Voltage",
};

static const char * const max98512_speaker_source_text[] = {
	"i2s", "reserved", "tone", "pdm"
};

static const char * const max98512_monomix_output_text[] = {
	"ch_0", "ch_1", "ch_1_2_div"
};

static const char * const max98512_one_stop_mode_text[] = {
	"Mono Left", "Mono Right",
	"Receiver Left", "Receiver Right",
	"Stereo",
	"Stereo II",
};

static bool max98512_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98512_R0001_INT_RAW1 ... MAX98512_R0028_ICC_RX_EN_B:
	case MAX98512_R002B_ICC_TX_EN_A ... MAX98512_R002C_ICC_TX_EN_B:
	case MAX98512_R002D_ICC_HIZ_MANUAL_MODE
		... MAX98512_R004C_MEAS_ADC_CH2_READ:
	case MAX98512_R004F_BROWNOUT_STATUS
		... MAX98512_R0053_BROWNOUT_LVL_HOLD:
	case MAX98512_R0058_BROWNOUT_LVL1_THRESH
		... MAX98512_R005F_BROWNOUT_AMP1_CLIP_MODE:
	case MAX98512_R0070_BROWNOUT_LVL1_CUR_LIMIT
		... MAX98512_R0088_BOOST_BYPASS_3:
	case MAX98512_R0400_GLOBAL_SHDN:
	case MAX98512_R0401_SOFT_RESET:
	case MAX98512_R0402_REV_ID:
		return true;
	default:
		return false;
	}
};

static bool max98512_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98512_R0001_INT_RAW1 ... MAX98512_R0009_INT_FLAG3:
		return true;
	default:
		return false;
	}
}

static const char * const max98512_boost_voltage_text[] = {
	"6.5V", "6.625V", "6.75V", "6.875V", "7V", "7.125V", "7.25V", "7.375V",
	"7.5V", "7.625V", "7.75V", "7.875V", "8V", "8.125V", "8.25V", "8.375V",
	"8.5V", "8.625V", "8.75V", "8.875V", "9V", "9.125V", "9.25V", "9.375V",
	"9.5V", "9.625V", "9.75V", "9.875V", "10V"
};

static SOC_ENUM_SINGLE_DECL(max98512_boost_voltage,
			    MAX98512_R003E_BOOST_CTRL0, 0,
			    max98512_boost_voltage_text);

static const char * const max98512_current_limit_text[] = {
	"1.00A", "1.10A", "1.20A", "1.30A", "1.40A", "1.50A", "1.60A", "1.70A",
	"1.80A", "1.90A", "2.00A", "2.10A", "2.20A", "2.30A", "2.40A", "2.50A",
	"2.60A", "2.70A", "2.80A", "2.90A", "3.00A", "3.10A", "3.20A", "3.30A",
	"3.40A", "3.50A", "3.60A", "3.70A", "3.80A", "3.90A", "4.00A", "4.10A"
};

static SOC_ENUM_SINGLE_DECL(max98512_current_limit,
			    MAX98512_R0040_BOOST_CTRL1, 1,
			    max98512_current_limit_text);

static int max98512_pdm_gain_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98512->pdm_gain;
	msg_maxim("pdm_gain setting returned %d",
		  (int) ucontrol->value.integer.value[0]);

	return 0;
}

static int max98512_pdm_gain_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (sel < ((1 << MAX98512_PDM_GAIN_WIDTH) - 1)) {
		max98512_wrapper_update(max98512, MAX98512T,
					MAX98512_R003A_SPK_GAIN,
					MAX98512_SPK_PDM_GAIN_MASK,
					sel << MAX98512_PDM_GAIN_SHIFT);
		max98512->pdm_gain = sel;
	}

	return 0;
}

static int max98512_rcv_digital_gain_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98512->digital_gain_rcv;
	msg_maxim("digital_gain_rcv setting returned %d",
		  (int) ucontrol->value.integer.value[0]);

	return 0;
}

static int max98512_rcv_digital_gain_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (sel < (1 << MAX98512_AMP_VOL_WIDTH) - 1) {
		max98512_wrapper_update(max98512, MAX98512L,
					MAX98512_R0035_AMP_VOL_CTRL,
					MAX98512_AMP_VOL_MASK,
					sel);
		max98512->digital_gain_rcv = sel;
	}

	return 0;
}

static int max98512_one_stop_mode_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	struct max98512_pdata *pdata = max98512->pdata;

	ucontrol->value.integer.value[0] = pdata->osm;

	return 0;
}

static int max98512_one_stop_mode_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	struct max98512_pdata *pdata = max98512->pdata;
	int osm = (int)ucontrol->value.integer.value[0];

	osm = osm < 0 ? 0 : osm;
	if (osm < MAX98512_OSM_MAX && pdata->osm != osm)
		pdata->osm = osm;

	return osm >= MAX98512_OSM_MAX ? -EINVAL : 0;
}

static const struct soc_enum max98512_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pdm_0_text), pdm_0_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pdm_1_text), pdm_1_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98512_one_stop_mode_text),
			    max98512_one_stop_mode_text),
};

static const struct snd_kcontrol_new max98512_snd_controls[] = {
	SOC_SINGLE_TLV("Speaker Volume", MAX98512_R003A_SPK_GAIN,
		       0, 6, 0,
		       max98512_spk_tlv),
	SOC_SINGLE_TLV("Digital Volume", MAX98512_R0035_AMP_VOL_CTRL,
		       0, (1 << MAX98512_AMP_VOL_WIDTH) - 1, 0,
		       max98512_digital_tlv),
	SOC_SINGLE("Amp DSP Switch", MAX98512_R0050_BROWNOUT_EN,
		   MAX98512_BROWNOUT_DSP_SHIFT, 1, 0),
	SOC_SINGLE("Ramp Switch", MAX98512_R0036_AMP_DSP_CFG,
		   MAX98512_AMP_DSP_CFG_RMP_SHIFT, 1, 0),
	SOC_SINGLE("Volume Location Switch", MAX98512_R0035_AMP_VOL_CTRL,
		   MAX98512_AMP_VOL_SEL_SHIFT, 1, 0),
	SOC_ENUM("Boost Output Voltage", max98512_boost_voltage),
	SOC_ENUM("Current Limit", max98512_current_limit),
	SOC_SINGLE_EXT_TLV("Pdm Gain",
			   MAX98512_R003A_SPK_GAIN,
			   MAX98512_PDM_GAIN_SHIFT,
			   (1 << MAX98512_PDM_GAIN_WIDTH) - 1, 0,
			   max98512_pdm_gain_get,
			   max98512_pdm_gain_put,
			   max98512_pdm_tlv),
	SOC_SINGLE_EXT_TLV("Rcv Digital Gain",
			   MAX98512_R0035_AMP_VOL_CTRL,
			   0,
			   (1 << MAX98512_AMP_VOL_WIDTH) - 1, 0,
			   max98512_rcv_digital_gain_get,
			   max98512_rcv_digital_gain_put,
			   max98512_digital_tlv),
	SOC_ENUM_EXT("PDM_L_CH_0", max98512_enum[0],
		     max98512_get_pdm_l_zero, max98512_put_pdm_l_zero),
	SOC_ENUM_EXT("PDM_L_CH_1", max98512_enum[1],
		     max98512_get_pdm_l_one, max98512_put_pdm_l_one),

	SOC_ENUM_EXT("One Stop Mode", max98512_enum[2],
		     max98512_one_stop_mode_get, max98512_one_stop_mode_put),
#ifdef USE_DSM_LOG
	SOC_SINGLE_EXT("DSM LOG", SND_SOC_NOPM, 0, 3, 0,
		       max98512_get_dump_status, max98512_set_dump_status),
#endif /* USE_DSM_LOG */
#ifdef USE_DSM_UPDATE_CAL
	SOC_SINGLE_EXT("DSM SetParam", SND_SOC_NOPM, 0, 1, 0,
		       max98512_get_dsm_param, max98512_set_dsm_param),
#endif /* USE_DSM_UPDATE_CAL */
};

static struct snd_soc_dai_driver max98512_dai[] = {
	{
		.name = "max98512-aif1",
		.playback = {
			.stream_name = "HiFi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98512_RATES,
			.formats = MAX98512_FORMATS,
		},
		.capture = {
			.stream_name = "HiFi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98512_RATES,
			.formats = MAX98512_FORMATS,
		},
		.ops = &max98512_dai_ops,
	},
	{
		.name = "max98512-aif-pdm",
		.playback = {
			.stream_name = "HiFi Playback PDM",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98512_RATES,
			.formats = MAX98512_FORMATS,
		},
		.capture = {
			.stream_name = "HiFi Capture PDM",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98512_RATES,
			.formats = MAX98512_FORMATS,
		},
		.ops = &max98512_dai_ops,
	},
};

static void max98512_adc_config(struct max98512_priv *max98512)
{
	max98512_wrapper_write(max98512, MAX98512T,
				MAX98512_R001A_PCM_TX_EN_A,
				0x03);
	max98512_wrapper_write(max98512, MAX98512L,
				MAX98512_R001A_PCM_TX_EN_A,
				0x01);
	max98512_wrapper_write(max98512, MAX98512R,
				MAX98512_R001A_PCM_TX_EN_A,
				0x02);
	max98512_wrapper_write(max98512, MAX98512T,
				MAX98512_R001C_PCM_TX_HIZ_CTRL_A,
				0xFC);
	max98512_wrapper_write(max98512, MAX98512L,
				MAX98512_R001C_PCM_TX_HIZ_CTRL_A,
				0xFE);
	max98512_wrapper_write(max98512, MAX98512R,
				MAX98512_R001C_PCM_TX_HIZ_CTRL_A,
				0xFD);
	max98512_wrapper_write(max98512, MAX98512T,
				MAX98512_R001E_PCM_TX_CH_SRC_A,
				0x01);
	max98512_wrapper_write(max98512, MAX98512L,
				MAX98512_R001E_PCM_TX_CH_SRC_A,
				0x00);
	max98512_wrapper_write(max98512, MAX98512R,
				MAX98512_R001E_PCM_TX_CH_SRC_A,
				0x11);
	max98512_wrapper_write(max98512, MAX98512B,
				MAX98512_R001F_PCM_TX_CH_SRC_B,
				0x20);
	max98512_wrapper_update(max98512, MAX98512T,
				MAX98512_R0024_PCM_SR_SETUP2,
				MAX98512_PCM_SR_SET2_IVADC_SR_MASK,
				MAX98512_PCM_SR_SET1_SR_48000);
	max98512_wrapper_update(max98512, MAX98512B,
				MAX98512_R0024_PCM_SR_SETUP2,
				MAX98512_PCM_SR_SET2_IVADC_SR_MASK,
				MAX98512_PCM_SR_SET1_SR_24000);
}

static void max98512_bde_config(struct max98512_priv *max98512)
{
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0041_MEAS_ADC_CFG,
			       0x07);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0042_MEAS_ADC_BASE_MSB,
			       0x00);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0043_MEAS_ADC_BASE_LSB,
			       0x24);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0044_ADC_CH0_DIVIDE,
			       0xFF);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0045_ADC_CH1_DIVIDE,
			       0xFF);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0046_ADC_CH2_DIVIDE,
			       0xFF);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0047_ADC_CH0_FILT_CFG,
			       0x0C);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0048_ADC_CH1_FILT_CFG,
			       0x0C);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0049_ADC_CH2_FILT_CFG,
			       0x0C);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0050_BROWNOUT_EN,
			       0x07);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0053_BROWNOUT_LVL_HOLD,
			       0xC8);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0058_BROWNOUT_LVL1_THRESH,
			       0x50);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R005C_BROWNOUT_THRESH_HYSTERYSIS,
			       0x14);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0070_BROWNOUT_LVL1_CUR_LIMIT,
			       0x14);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0071_BROWNOUT_LVL1_AMP1_CTRL1,
			       0x06);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0072_BROWNOUT_LVL1_AMP1_CTRL2,
			       0x00);
	max98512_wrapper_write(max98512, MAX98512A,
			       MAX98512_R0073_BROWNOUT_LVL1_AMP1_CTRL3,
			       0x00);
}

static int max98512_probe(struct snd_soc_codec *codec)
{
	struct max98512_priv *max98512 = snd_soc_codec_get_drvdata(codec);
	struct max98512_pdata *pdata = max98512->pdata;
	struct max98512_volume_step_info *vstep = &max98512->vstep;
	int ret = 0, reg = 0x0;
	unsigned int vimon = pdata->nodsm ? 0 : MAX98512_MEAS_VI_EN;

	max98512->codec = codec;
	codec->control_data = max98512->regmap_l;
	codec->cache_bypass = 1;

	ret = snd_soc_add_codec_controls(codec, max98512_snd_controls,
					 ARRAY_SIZE(max98512_snd_controls));
	if (ret < 0) {
		msg_maxim("Failed to add controls to max98512: %d\n", ret);
		return ret;
	}

	ret = max98512_wrapper_read(max98512, MAX98512L,
				    MAX98512_R0402_REV_ID, &reg);
	msg_maxim("L device version 0x%02X", reg);

	reg = 0x0;
	if (max98512->mono_stereo) {
		ret = max98512_wrapper_read(max98512, MAX98512R,
					    MAX98512_R0402_REV_ID, &reg);
		msg_maxim("R device version 0x%02X", reg);
	}

	reg = 0x0;
	if (pdata->third_reg) {
		ret = max98512_wrapper_read(max98512, MAX98512T,
					    MAX98512_R0402_REV_ID, &reg);
		msg_maxim("Third device version 0x%02X", reg);
	}

	max98512_wrapper_read(max98512, MAX98512L,
			MAX98512_R003E_BOOST_CTRL0, &pdata->boostv);

	/* Software Reset */
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R0401_SOFT_RESET,
			MAX98512_SOFT_RESET);

	max98512_wrapper_write(max98512, MAX98512A,
		MAX98512_R003C_MEAS_EN, vimon);
	vstep->adc_status = !!vimon;
	/* IV default slot configuration */

	max98512_wrapper_write(max98512, MAX98512B,
			MAX98512_R0025_PCM_TO_SPK_MONOMIX_A,
				0x80);
	max98512_wrapper_write(max98512, MAX98512B,
			MAX98512_R0026_PCM_TO_SPK_MONOMIX_B,
				0x1);

	max98512_wrapper_write(max98512, MAX98512T,
			MAX98512_R0031_PDM_TX_EN,
				0x1);
	max98512_wrapper_write(max98512, MAX98512T,
			MAX98512_R0032_PDM_TX_HIZ_CTRL,
				0x0);
	max98512_wrapper_write(max98512, MAX98512T,
			MAX98512_R0033_PDM_TX_CTRL,
				0x01);

	/* Set inital volume (+15dB) */
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R0035_AMP_VOL_CTRL,
				0x40);
	max98512_wrapper_write(max98512, MAX98512B,
			MAX98512_R003A_SPK_GAIN,
				0x05);
	max98512_wrapper_update(max98512, MAX98512T,
			MAX98512_R003A_SPK_GAIN,
			MAX98512_SPK_PDM_GAIN_MASK,
				0x55);
	/* Enable DC blocker */
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R0036_AMP_DSP_CFG,
				0x03);
	/* Enable IMON VMON DC blocker */
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R003D_MEAS_DSP_CFG,
				0xF7);
	/* Boost Output Voltage & Current limit */
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R003E_BOOST_CTRL0,
				0x1C);
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R0040_BOOST_CTRL1,
				0x3E);

	/* ADC setting */
	max98512_adc_config(max98512);

	/* Measurement ADC config */
	max98512_bde_config(max98512);

	/* Brownout Level */
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R007D_BROWNOUT_LVL4_AMP1_CTRL1,
				0x06);
	/* Envelope Tracking configuration */
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R0080_ENV_TRACK_VOUT_HEADROOM,
				0x08);
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R0084_ENV_TRACK_CTRL,
				0x01);
	max98512_wrapper_write(max98512, MAX98512A,
			MAX98512_R0085_ENV_TRACK_BOOST_VOUT_READ,
				0x10);

#if defined(USE_DSM_LOG) || defined(USE_DSM_UPDATE_CAL)
	if (!g_class)
		g_class = class_create(THIS_MODULE, class_name_log);
	max98512->class = g_class;
	if (max98512->class) {
		max98512->dev = device_create(max98512->class,
					      NULL, 1, NULL, "max98512");
		if (IS_ERR(max98512->dev)) {
			ret = sysfs_create_group(&codec->dev->kobj,
						 &max98512_attribute_group);
			if (ret)
				msg_maxim("failed to create sysfs group [%d]",
					  ret);
		} else {
			ret = sysfs_create_group(&max98512->dev->kobj,
						 &max98512_attribute_group);
			if (ret)
				msg_maxim("failed to create sysfs group [%d]",
					  ret);
		}
	}
	msg_maxim("g_class = %p %p", g_class, max98512->class);
#endif /* USE_DSM_LOG */

	msg_maxim("End");

	return 0;
}

static const struct snd_soc_codec_driver soc_codec_dev_max98512 = {
	.probe = max98512_probe,
};

static const struct regmap_config max98512_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = MAX98512_R0402_REV_ID,
	.reg_defaults = max98512_reg,
	.num_reg_defaults = ARRAY_SIZE(max98512_reg),
	.readable_reg = max98512_readable_register,
	.volatile_reg = max98512_volatile_reg,
	.cache_type = REGCACHE_RBTREE,
};

static struct i2c_board_info max98512_i2c_sub_board[] = {
	{
		I2C_BOARD_INFO("max98512_sub", 0x3a),
	}
};

static struct i2c_driver max98512_i2c_sub_driver = {
	.driver = {
		.name = "max98512_sub",
		.owner = THIS_MODULE,
	},
};

struct i2c_client *max98512_add_sub_device(int bus_id, int slave_addr)
{
	struct i2c_client *i2c = NULL;
	struct i2c_adapter *adapter;

	msg_maxim("bus_id:%d, slave_addr:0x%x", bus_id, slave_addr);
	max98512_i2c_sub_board[0].addr = slave_addr;

	adapter = i2c_get_adapter(bus_id);
	if (adapter) {
		i2c = i2c_new_device(adapter, max98512_i2c_sub_board);
		if (i2c)
			i2c->dev.driver = &max98512_i2c_sub_driver.driver;
	}

	return i2c;
}

static struct i2c_board_info max98512_i2c_third_board[] = {
	{
		I2C_BOARD_INFO("max98512_third", 0x3c),
	}
};

static struct i2c_driver max98512_i2c_third_driver = {
	.driver = {
		.name = "max98512_third",
		.owner = THIS_MODULE,
	},
};

struct i2c_client *max98512_add_third_device(int bus_id, int slave_addr)
{
	struct i2c_client *i2c = NULL;
	struct i2c_adapter *adapter;

	msg_maxim("bus_id:%d, slave_addr:0x%x", bus_id, slave_addr);
	max98512_i2c_third_board[0].addr = slave_addr;

	adapter = i2c_get_adapter(bus_id);
	if (adapter) {
		i2c = i2c_new_device(adapter, max98512_i2c_third_board);
		if (i2c)
			i2c->dev.driver = &max98512_i2c_third_driver.driver;
	}

	return i2c;
}

static int max98512_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct max98512_priv *max98512;
	struct max98512_pdata *pdata;
	struct max98512_volume_step_info *vstep;
	int ret = 0, value;

	msg_maxim("Start. driver_data %ld", id->driver_data);

	max98512 = devm_kzalloc(&i2c->dev, sizeof(*max98512), GFP_KERNEL);
	if (!max98512) {
		ret = -ENOMEM;
		goto err_alloc_priv;
	}

	max98512->pdata = devm_kzalloc(&i2c->dev,
				       sizeof(struct max98512_pdata),
				       GFP_KERNEL);
	if (!max98512->pdata) {
		ret = -ENOMEM;
		goto err_alloc_pdata;
	}

	i2c_set_clientdata(i2c, max98512);
	max98512->i2c_dev = &i2c->dev;
	pdata = max98512->pdata;
	vstep = &max98512->vstep;

	if (i2c->dev.of_node) {
		if (of_property_read_u32_array(i2c->dev.of_node,
					       "maxim,platform_info",
					       (u32 *)&pdata->pinfo,
					       sizeof(pdata->pinfo) /
					       sizeof(uint32_t)))
			dev_warn(&i2c->dev, "set platform_info by default.\n");

		if (of_property_read_u32_array(i2c->dev.of_node,
					       "maxim,boost_step",
					       (uint32_t *) &vstep->boost_step,
					       sizeof(vstep->boost_step) /
					       sizeof(uint32_t))) {
			dev_warn(&i2c->dev, "set boost_step by default.\n");
			for (ret = 0; ret < MAX98512_VSTEP_14; ret++)
				vstep->boost_step[ret] = 0x00;
			vstep->boost_step[MAX98512_VSTEP_14] = 0x0C; /* 8V */
			vstep->boost_step[MAX98512_VSTEP_15] = 0x10; /* 8.5V */
		}

		if (of_property_read_u32(i2c->dev.of_node,
					 "maxim,spk-gain",
					 &max98512->spk_gain)) {
			dev_warn(&i2c->dev, "set spk_gain by default.\n");
			max98512->spk_gain = 0x05; /* +15db for PCM */
		}
		max98512->pdm_gain = max98512->spk_gain;

		if (of_property_read_u32(i2c->dev.of_node,
					 "maxim,spk-gain-rcv",
					 &max98512->spk_gain_rcv)) {
			dev_warn(&i2c->dev, "set spk_gain-rcv by default.\n");
			max98512->spk_gain_rcv = 0x01; /* +3db for RCV */
		}

		if (of_property_read_u32(i2c->dev.of_node,
					 "maxim,digital-gain",
					 &max98512->digital_gain)) {
			dev_warn(&i2c->dev, "set digital_gain by default.\n");
			max98512->digital_gain = 0x40; /* 0db */
		}

		if (of_property_read_u32(i2c->dev.of_node,
					 "maxim,digital-gain-rcv",
					 &max98512->digital_gain_rcv)) {
			dev_warn(&i2c->dev, "set digital_gain-rcv by default.\n");
			max98512->digital_gain = 0x34; /* -3db */
		}

		if (of_property_read_u32(i2c->dev.of_node,
					 "maxim,sysclk", &max98512->sysclk)) {
			dev_warn(&i2c->dev, "set sysclk by default value.\n");
			max98512->sysclk = 12288000;
		}

		if (!of_property_read_u32(i2c->dev.of_node,
					  "maxim,mono_stereo", &value)) {
			max98512->mono_stereo = value;
		} else
			max98512->mono_stereo = 0;
		dev_info(&i2c->dev, "mono_stereo %d\n", max98512->mono_stereo);

		/* update interleave mode info */
		if (!of_property_read_u32(i2c->dev.of_node,
					  "interleave_mode", &value)) {
			if (value > 0)
				max98512->interleave_mode = 1;
			else
				max98512->interleave_mode = 0;
		} else
			max98512->interleave_mode = 0;

		if (of_property_read_u32(i2c->dev.of_node,
					 "maxim,adc_threshold",
					 &vstep->adc_thres)) {
			dev_warn(&i2c->dev, "set adc_threshold by default.\n");
			vstep->adc_thres = MAX98512_VSTEP_8;
		}

		pdata->reg_arr = of_get_property(i2c->dev.of_node,
						 "maxim,registers-of-amp",
						 &pdata->reg_arr_len);

		pdata->nodsm = of_property_read_bool(i2c->dev.of_node,
						     "maxim,nodsm");
		msg_maxim("use DSM(%d)", pdata->nodsm);

		/* update sub device info */
		if (of_property_read_u32(i2c->dev.of_node,
					  "maxim,sub_reg", &pdata->sub_reg)) {
			dev_err(&i2c->dev,
				"sub-device register was not found.\n");
			pdata->sub_reg = 0;
		} else
			dev_info(&i2c->dev,
				 "set sub-device address %#x", pdata->sub_reg);

		/* update third device info */
		if (of_property_read_u32(i2c->dev.of_node,
					  "maxim,third_reg", &pdata->third_reg)) {
			dev_err(&i2c->dev,
				"third-device register was not found.\n");
			pdata->third_reg = 0;
		} else
			dev_info(&i2c->dev,
				 "set third-device address %#x", pdata->third_reg);

#ifdef USE_DSM_LOG
		if (of_property_read_string(i2c->dev.of_node,
					    "maxim,log_class",
					    &class_name_log)) {
			dev_warn(&i2c->dev,
				 "There is no log_class property.\n");
			class_name_log = DEFAULT_LOG_CLASS_NAME;
		}
#endif
	} else {
		/* If i2c->dev.of_node is not found */
		max98512->sysclk = 12288000;
		for (ret = 0; ret < MAX98512_VSTEP_14; ret++)
			vstep->boost_step[ret] = 0x00;
		vstep->boost_step[MAX98512_VSTEP_14] = 0x0C; /* 8V */
		vstep->boost_step[MAX98512_VSTEP_15] = 0x10; /* 8.5V */
		max98512->spk_gain = 0x05; /* +15db for PCM */
		max98512->digital_gain = 0x40; /* 0db */
		max98512->mono_stereo = 0;
		max98512->interleave_mode = 0;
		vstep->adc_thres = MAX98512_VSTEP_8;
		pdata->nodsm = 0;
	}

	/* regmap init */
	max98512->regmap_l = regmap_init_i2c(i2c,
						  &max98512_regmap);
	if (IS_ERR(max98512->regmap_l)) {
		ret = PTR_ERR(max98512->regmap_l);
		dev_err(&i2c->dev, "Failed to allocate regmap_l: %d\n",
			ret);
		goto err_regmap;
	} else {
		regcache_cache_bypass(max98512->regmap_l, true);
		msg_maxim("regmap_L %p", max98512->regmap_l);
	}

	/* regmap init for sub-device */
	if (pdata->sub_reg) {
		max98512->sub_i2c = max98512_add_sub_device(i2c->adapter->nr,
							    pdata->sub_reg);
		if (IS_ERR(max98512->sub_i2c)) {
			dev_err(&max98512->sub_i2c->dev,
				"second max98512 was not found\n");
			ret = -ENODEV;
			goto err_regmap;
		} else {
			max98512->regmap_r = regmap_init_i2c(max98512->sub_i2c,
							     &max98512_regmap);
			if (IS_ERR(max98512->regmap_r)) {
				ret = PTR_ERR(max98512->regmap_r);
				dev_err(&max98512->sub_i2c->dev,
					"Failed to allocate regmap_r: %d\n",
					ret);
				goto err_regmap;
			} else {
				regcache_cache_bypass(max98512->regmap_r, true);
				msg_maxim("regmap_R %p", max98512->regmap_r);
			}
		}
	}

	/* regmap init for third-device */
	if (pdata->third_reg) {
		max98512->third_i2c = max98512_add_third_device(i2c->adapter->nr,
							    pdata->third_reg);
		if (IS_ERR(max98512->third_i2c)) {
			dev_err(&max98512->third_i2c->dev,
				"second max98512 was not found\n");
			ret = -ENODEV;
			goto err_regmap;
		} else {
			max98512->regmap_third = regmap_init_i2c(max98512->third_i2c,
							     &max98512_regmap);
			if (IS_ERR(max98512->regmap_third)) {
				ret = PTR_ERR(max98512->regmap_third);
				dev_err(&max98512->third_i2c->dev,
					"Failed to allocate regmap_third: %d\n",
					ret);
				goto err_regmap;
			} else {
				regcache_cache_bypass(max98512->regmap_third, true);
				msg_maxim("regmap_third %p", max98512->regmap_third);
			}
		}
	}

#ifdef CONFIG_SND_SOC_MAXIM_DSM
	/* If maxdsm module was already registerd, will be ignored. */
	maxdsm_init();
	if (pdata->pinfo)
		maxdsm_update_info(pdata->pinfo);
#endif

	/* codec registeration */
	ret = snd_soc_register_codec(&i2c->dev,
				     &soc_codec_dev_max98512,
				     max98512_dai,
				     ARRAY_SIZE(max98512_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);
		goto err_register_codec;
	}

	msg_maxim("End. driver_data %ld", id->driver_data);

	return 0;

err_register_codec:
#ifdef CONFIG_SND_SOC_MAXIM_DSM
	maxdsm_deinit();
#endif
	if (max98512->regmap_l)
		regmap_exit(max98512->regmap_l);
	if (max98512->regmap_r)
		regmap_exit(max98512->regmap_r);
	if (max98512->regmap_third)
		regmap_exit(max98512->regmap_third);
err_regmap:
	devm_kfree(&i2c->dev, max98512->pdata);
err_alloc_pdata:
	devm_kfree(&i2c->dev, max98512);
err_alloc_priv:
	msg_maxim("Failed with %d. driver_data %ld", ret, id->driver_data);

	return ret;
}

static int max98512_i2c_remove(struct i2c_client *client)
{
	struct max98512_priv *max98512 = i2c_get_clientdata(client);
	struct max98512_pdata *pdata = max98512->pdata;

#ifdef CONFIG_SND_SOC_MAXIM_DSM
	maxdsm_deinit();
#endif
	snd_soc_unregister_codec(&client->dev);
	if (max98512->regmap_l)
		regmap_exit(max98512->regmap_l);
	if (max98512->regmap_r)
		regmap_exit(max98512->regmap_r);
	if (max98512->regmap_third)
		regmap_exit(max98512->regmap_third);
	if (pdata->sub_reg != 0)
		i2c_unregister_device(max98512->sub_i2c);
	if (pdata->third_reg != 0)
		i2c_unregister_device(max98512->third_i2c);
	devm_kfree(&client->dev, pdata);
	devm_kfree(&client->dev, max98512);

	return 0;
}

static const struct i2c_device_id max98512_i2c_id[] = {
	{ "max98512", MAX98512},
	{ },
};
MODULE_DEVICE_TABLE(i2c, max98512_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id max98512_of_match[] = {
	{ .compatible = "maxim,max98512", },
	{ }
};
MODULE_DEVICE_TABLE(of, max98512_of_match);
#endif

static struct i2c_driver max98512_i2c_driver = {
	.driver = {
		.name = "max98512",
		.of_match_table = of_match_ptr(max98512_of_match),
		.pm = NULL,
	},
	.probe  = max98512_i2c_probe,
	.remove = max98512_i2c_remove,
	.id_table = max98512_i2c_id,
};
module_i2c_driver(max98512_i2c_driver)

MODULE_DESCRIPTION("ALSA SoC MAX98512 driver");
MODULE_AUTHOR("Ryan Lee <ryans.lee@maximintegrated.com>");
MODULE_LICENSE("GPL");
