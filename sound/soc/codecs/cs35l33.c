/*
 * cs35l33.c -- CS35L33 ALSA SoC audio driver
 *
 * Copyright 2013 Cirrus Logic, Inc.
 *
 * Author: Paul Handrigan <paul.handrigan@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/gpio.h>
#include <sound/cs35l33.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>

#include "cs35l33.h"

struct  cs35l33_private {
	struct device *dev;
	struct snd_soc_codec *codec;
	struct cs35l33_pdata pdata;
	struct regmap *regmap;
	bool amp_cal;
	int mclk_int;
	struct regulator_bulk_data core_supplies[2];
	int num_core_supplies;
	bool is_tdm_mode;
	bool enable_soft_ramp;
};

static const struct reg_default cs35l33_reg[] = {
	{0x6, 0x85}, /* PWR CTL 1 */
	{0x7, 0xFE}, /* PWR CTL 2 */
	{0x8, 0x0C}, /* CLK CTL */
	{0x9, 0x90}, /* BST PEAK */
	{0xA, 0x55}, /* PROTECTION CTL */
	{0xB, 0x00}, /* BST CTL 1 */
	{0xC, 0x01}, /* BST CTL 2 */
	{0xD, 0x00}, /* ADSP CTL */
	{0xE, 0xC8}, /* ADC CTL */
	{0xF, 0x14}, /* DAC CTL */
	{0x10, 0x00}, /* DAC VOL */
	{0x11, 0x04}, /* AMP CTL */
	{0x12, 0x90}, /* AMP GAIN CTL */
	{0x13, 0xFF}, /* INT MASK 1 */
	{0x14, 0xFF}, /* INT MASK 2 */
	{0x17, 0x00}, /* Diagnostic Mode Register Lock */
	{0x18, 0x40}, /* Diagnostic Mode Register Control */
	{0x19, 0x00}, /* Diagnostic Mode Register Control 2 */
	{0x23, 0x62}, /* HG MEM/LDO CTL */
	{0x24, 0x03}, /* HG RELEASE RATE */
	{0x25, 0x12}, /* LDO ENTRY DELAY */
	{0x29, 0x0A}, /* HG HEADROOM */
	{0x2A, 0x05}, /* HG ENABLE/VP CTL2 */
	{0x2D, 0x00}, /* TDM TX Control 1 (VMON) */
	{0x2E, 0x03}, /* TDM TX Control 2 (IMON) */
	{0x2F, 0x02}, /* TDM TX Control 3 (VPMON) */
	{0x30, 0x05}, /* TDM TX Control 4 (VBSTMON) */
	{0x31, 0x06}, /* TDM TX Control 5 (FLAG) */
	{0x32, 0x00}, /* TDM TX Enable 1 */
	{0x33, 0x00}, /* TDM TX Enable 2 */
	{0x34, 0x00}, /* TDM TX Enable 3 */
	{0x35, 0x00}, /* TDM TX Enable 4 */
	{0x36, 0x40}, /* TDM RX Control 1 */
	{0x37, 0x03}, /* TDM RX Control 2 */
	{0x38, 0x04}, /* TDM RX Control 3 */
	{0x39, 0x45}, /* Boost Converter Control 4 */
};

static const struct reg_default cs35l33_patch[] = {
	{ 0x00,  0x99 },
	{ 0x59,  0x02 },
	{ 0x52,  0x30 },
	{ 0x39,  0x45 },
	{ 0x57,  0x30 },
	{ 0x2C,  0x68 },
	{ 0x00,  0x00 },
};

static bool cs35l33_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L33_DEVID_AB:
	case CS35L33_DEVID_CD:
	case CS35L33_DEVID_E:
	case CS35L33_REV_ID:
	case CS35L33_INT_STATUS_1:
	case CS35L33_INT_STATUS_2:
	case CS35L33_HG_STATUS:
		return true;
	default:
		return false;
	}
}

static bool cs35l33_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	/* these are read only registers */
	case CS35L33_DEVID_AB:
	case CS35L33_DEVID_CD:
	case CS35L33_DEVID_E:
	case CS35L33_REV_ID:
	case CS35L33_INT_STATUS_1:
	case CS35L33_INT_STATUS_2:
	case CS35L33_HG_STATUS:
		return false;
	default:
		return true;
	}
}

static bool cs35l33_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L33_DEVID_AB:
	case CS35L33_DEVID_CD:
	case CS35L33_DEVID_E:
	case CS35L33_REV_ID:
	case CS35L33_PWRCTL1:
	case CS35L33_PWRCTL2:
	case CS35L33_CLK_CTL:
	case CS35L33_BST_PEAK_CTL:
	case CS35L33_PROTECT_CTL:
	case CS35L33_BST_CTL1:
	case CS35L33_BST_CTL2:
	case CS35L33_ADSP_CTL:
	case CS35L33_ADC_CTL:
	case CS35L33_DAC_CTL:
	case CS35L33_DIG_VOL_CTL:
	case CS35L33_CLASSD_CTL:
	case CS35L33_AMP_CTL:
	case CS35L33_INT_MASK_1:
	case CS35L33_INT_MASK_2:
	case CS35L33_INT_STATUS_1:
	case CS35L33_INT_STATUS_2:
	case CS35L33_DIAG_LOCK:
	case CS35L33_DIAG_CTRL_1:
	case CS35L33_DIAG_CTRL_2:
	case CS35L33_HG_MEMLDO_CTL:
	case CS35L33_HG_REL_RATE:
	case CS35L33_LDO_DEL:
	case CS35L33_HG_HEAD:
	case CS35L33_HG_EN:
	case CS35L33_TX_VMON:
	case CS35L33_TX_IMON:
	case CS35L33_TX_VPMON:
	case CS35L33_TX_VBSTMON:
	case CS35L33_TX_FLAG:
	case CS35L33_TX_EN1:
	case CS35L33_TX_EN2:
	case CS35L33_TX_EN3:
	case CS35L33_TX_EN4:
	case CS35L33_RX_AUD:
	case CS35L33_RX_SPLY:
	case CS35L33_RX_ALIVE:
	case CS35L33_BST_CTL4:
		return true;
	default:
		return false;
	}
}

static DECLARE_TLV_DB_SCALE(classd_ctl_tlv, 900, 100, 0);
static DECLARE_TLV_DB_SCALE(dac_tlv, -10200, 50, 0);

static const struct snd_kcontrol_new cs35l33_snd_controls[] = {

	SOC_SINGLE_TLV("SPK Amp Volume", CS35L33_AMP_CTL,
		       4, 0x09, 0, classd_ctl_tlv),
	SOC_SINGLE_SX_TLV("DAC Volume", CS35L33_DIG_VOL_CTL,
			0, 0x34, 0xE4, dac_tlv),
};

static int cs35l33_spkrdrv_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct cs35l33_private *priv = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!priv->amp_cal)
			usleep_range(9999, 10000);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (!priv->amp_cal) {
			usleep_range(7999, 8000);
			priv->amp_cal = true;
			snd_soc_update_bits(codec, CS35L33_CLASSD_CTL,
				    AMP_CAL, 0);
			dev_dbg(w->codec->dev, "amp calibration done\n");
		}
		dev_info(w->codec->dev, "amp turned On\n");
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_info(w->codec->dev, "amp turned Off\n");
		break;
	default:
		pr_err("Invalid event = 0x%x\n", event);
		break;
	}

	return 0;
}

static int cs35l33_sdin_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct cs35l33_private *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, CS35L33_PWRCTL1,
				    PDN_BST, 0);
		val = priv->is_tdm_mode ? 0 : PDN_TDM;
		snd_soc_update_bits(codec, CS35L33_PWRCTL2,
				    PDN_TDM, val);
		dev_info(w->codec->dev, "bst turned On\n");
		break;
	case SND_SOC_DAPM_POST_PMU:
		dev_info(w->codec->dev, "sdin turned On\n");
		if (!priv->amp_cal) {
			snd_soc_update_bits(codec, CS35L33_CLASSD_CTL,
				    AMP_CAL, AMP_CAL);
			dev_dbg(w->codec->dev, "amp calibration started\n");
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, CS35L33_PWRCTL1,
				    PDN_BST, PDN_BST);
		snd_soc_update_bits(codec, CS35L33_PWRCTL2,
				    PDN_TDM, PDN_TDM);
		dev_info(w->codec->dev, "bst and sdin turned Off\n");
		break;
	default:
		pr_err("Invalid event = 0x%x\n", event);

	}

	return 0;
}

static int cs35l33_sdout_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct cs35l33_private *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int mask, val, mask2, val2;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->is_tdm_mode) {
			/* set sdout_3st_i2s and reset pdn_tdm */
			mask = (SDOUT_3ST_I2S | PDN_TDM);
			val = SDOUT_3ST_I2S;
			/* reset sdout_3st_tdm */
			mask2 = SDOUT_3ST_TDM;
			val2 = 0;
		} else {
			/* reset sdout_3st_i2s and set pdn_tdm */
			mask = (SDOUT_3ST_I2S | PDN_TDM);
			val = PDN_TDM;
			/* set sdout_3st_tdm */
			mask2 = val2 = SDOUT_3ST_TDM;
		}
		dev_info(w->codec->dev, "sdout turned On\n");
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mask = val = (SDOUT_3ST_I2S | PDN_TDM);
		mask2 = val2 = SDOUT_3ST_TDM;
		dev_info(w->codec->dev, "sdout turned Off\n");
		break;
	default:
		pr_err("Invalid event = 0x%x\n", event);
		return 0;
	}

	snd_soc_update_bits(codec, CS35L33_PWRCTL2,
		mask, val);
	snd_soc_update_bits(codec, CS35L33_CLK_CTL,
		mask2, val2);

	return 0;
}

static const struct snd_soc_dapm_widget cs35l33_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("SPEAKER"),
	SND_SOC_DAPM_OUT_DRV_E("SPKDRV", CS35L33_PWRCTL1, 7, 1, NULL, 0,
		cs35l33_spkrdrv_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SDIN", NULL, 0, CS35L33_PWRCTL2,
		2, 1, cs35l33_sdin_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("MON"),

	SND_SOC_DAPM_ADC("VMON", NULL,
		CS35L33_PWRCTL2, PDN_VMON_SHIFT, 1),
	SND_SOC_DAPM_ADC("IMON", NULL,
		CS35L33_PWRCTL2, PDN_IMON_SHIFT, 1),
	SND_SOC_DAPM_ADC("VPMON", NULL,
		CS35L33_PWRCTL2, PDN_VPMON_SHIFT, 1),
	SND_SOC_DAPM_ADC("VBSTMON", NULL,
		CS35L33_PWRCTL2, PDN_VBSTMON_SHIFT, 1),

	SND_SOC_DAPM_AIF_OUT_E("SDOUT", NULL, 0, SND_SOC_NOPM, 0, 0,
		cs35l33_sdout_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_PRE_PMD),
};

static const struct snd_soc_dapm_route cs35l33_audio_map[] = {
	{"SDIN", NULL, "CS35L33 Playback"},
	{"SPKDRV", NULL, "SDIN"},
	{"SPEAKER", NULL, "SPKDRV"},

	{"VMON", NULL, "MON"},
	{"IMON", NULL, "MON"},

	{"SDOUT", NULL, "VMON"},
	{"SDOUT", NULL, "IMON"},
	{"CS35L33 Capture", NULL, "SDOUT"},
};

static const struct snd_soc_dapm_route cs35l33_vphg_auto_route[] = {
	{"SPKDRV", NULL, "VPMON"},
	{"VPMON", NULL, "CS35L33 Playback"},
};

static const struct snd_soc_dapm_route cs35l33_vp_vbst_mon_route[] = {
	{"SDOUT", NULL, "VPMON"},
	{"VPMON", NULL, "MON"},
	{"SDOUT", NULL, "VBSTMON"},
	{"VBSTMON", NULL, "MON"},
};

static int cs35l33_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	unsigned int val;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		snd_soc_update_bits(codec, CS35L33_PWRCTL1,
				    PDN_ALL, 0);
		snd_soc_update_bits(codec, CS35L33_CLK_CTL,
				    MCLKDIS, 0);
		dev_dbg(codec->dev, "take amp out of standby\n");
		break;
	case SND_SOC_BIAS_STANDBY:
		snd_soc_update_bits(codec, CS35L33_PWRCTL1,
				    PDN_ALL, PDN_ALL);
		val = snd_soc_read(codec, CS35L33_INT_STATUS_2);
		if (val & PDN_DONE) {
			snd_soc_update_bits(codec, CS35L33_CLK_CTL,
					    MCLKDIS, MCLKDIS);
		}
		break;
	case SND_SOC_BIAS_OFF:
		break;
	default:
		return -EINVAL;
	}

	codec->dapm.bias_level = level;

	return 0;
}

struct cs35l33_mclk_div {
	int mclk;
	int srate;
	u8 adsp_rate;
	u8 int_fs_ratio;
};

static struct cs35l33_mclk_div cs35l33_mclk_coeffs[] = {

	/* MCLK, Sample Rate, adsp_rate, int_fs_ratio */
	{5644800, 11025, 0x4, INT_FS_RATE},
	{5644800, 22050, 0x8, INT_FS_RATE},
	{5644800, 44100, 0xC, INT_FS_RATE},

	{6000000,  8000, 0x1, 0},
	{6000000, 11025, 0x2, 0},
	{6000000, 11029, 0x3, 0},
	{6000000, 12000, 0x4, 0},
	{6000000, 16000, 0x5, 0},
	{6000000, 22050, 0x6, 0},
	{6000000, 22059, 0x7, 0},
	{6000000, 24000, 0x8, 0},
	{6000000, 32000, 0x9, 0},
	{6000000, 44100, 0xA, 0},
	{6000000, 44118, 0xB, 0},
	{6000000, 48000, 0xC, 0},

	{6144000,  8000, 0x1, INT_FS_RATE},
	{6144000, 12000, 0x4, INT_FS_RATE},
	{6144000, 16000, 0x5, INT_FS_RATE},
	{6144000, 24000, 0x8, INT_FS_RATE},
	{6144000, 32000, 0x9, INT_FS_RATE},
	{6144000, 48000, 0xC, INT_FS_RATE},
};

static int cs35l33_get_mclk_coeff(int mclk, int srate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l33_mclk_coeffs); i++) {
		if (cs35l33_mclk_coeffs[i].mclk == mclk &&
			cs35l33_mclk_coeffs[i].srate == srate)
			return i;
	}
	return -EINVAL;
}

static int cs35l33_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs35l33_private *priv = snd_soc_codec_get_drvdata(codec);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		snd_soc_update_bits(codec, CS35L33_ADSP_CTL,
				    MS_MASK, MS_MASK);
		dev_dbg(codec->dev, "audio port in master mode\n");
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		snd_soc_update_bits(codec, CS35L33_ADSP_CTL,
				    MS_MASK, 0);
		dev_dbg(codec->dev, "audio port in slave mode\n");
		break;
	default:
		dev_dbg(codec->dev, "Failed to set master/slave\n");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		/* tdm mode in cs35l33 resembles dsp-a mode very
		closely, it is dsp-a with fsync shifted left by half bclk */
		priv->is_tdm_mode = true;
		dev_dbg(codec->dev, "audio port in tdm mode\n");
		break;
	case SND_SOC_DAIFMT_I2S:
		priv->is_tdm_mode = false;
		dev_dbg(codec->dev, "audio port in i2s mode\n");
		break;
	default:
		dev_dbg(codec->dev, "Failed to set pcm data format\n");
		return -EINVAL;
	}

	return 0;
}

static int cs35l33_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs35l33_private *priv = snd_soc_codec_get_drvdata(codec);
	int sample_size = params_width(params);
	int coeff = cs35l33_get_mclk_coeff(priv->mclk_int, params_rate(params));

	if (coeff < 0)
		return coeff;

	snd_soc_update_bits(codec, CS35L33_CLK_CTL,
		INT_FS_RATE | ADSP_FS,
		cs35l33_mclk_coeffs[coeff].int_fs_ratio |
		cs35l33_mclk_coeffs[coeff].adsp_rate);

	if (priv->is_tdm_mode) {
		sample_size = (sample_size / 8) - 1;
		if (sample_size > 2)
			sample_size = 2;
		snd_soc_update_bits(codec, CS35L33_RX_AUD,
			AUDIN_RX_DEPTH,
			sample_size << AUDIN_RX_DEPTH_SHIFT);
	}

	dev_dbg(codec->dev, "sample rate=%d, bits per sample=%d\n",
		params_rate(params), params_width(params));

	return 0;
}

static int cs35l33_src_rates[] = {
	8000, 11025, 11029, 12000, 16000, 22050,
	22059, 24000, 32000, 44100, 44118, 48000
};


static struct snd_pcm_hw_constraint_list cs35l33_constraints = {
	.count  = ARRAY_SIZE(cs35l33_src_rates),
	.list   = cs35l33_src_rates,
};

static int cs35l33_pcm_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	snd_pcm_hw_constraint_list(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_RATE,
					&cs35l33_constraints);
	return 0;
}

static int cs35l33_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;

	if (tristate) {
		snd_soc_update_bits(codec, CS35L33_PWRCTL2,
			SDOUT_3ST_I2S, SDOUT_3ST_I2S);
		snd_soc_update_bits(codec, CS35L33_CLK_CTL,
			SDOUT_3ST_TDM, SDOUT_3ST_TDM);
	} else {
		snd_soc_update_bits(codec, CS35L33_PWRCTL2,
			SDOUT_3ST_I2S, 0);
		snd_soc_update_bits(codec, CS35L33_CLK_CTL,
			SDOUT_3ST_TDM, 0);
	}

	return 0;
}

static int cs35l33_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
				unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int reg, bit_pos, i;
	int slot, slot_num;

	if (slot_width != 8)
		return -EINVAL;

	/* scan rx_mask for aud slot */
	slot = ffs(rx_mask) - 1;
	if (slot >= 0) {
		snd_soc_update_bits(codec, CS35L33_RX_AUD,
			X_LOC, slot);
		dev_dbg(codec->dev, "Audio starts from slots %d", slot);
	}

	/* scan tx_mask: vmon(2 slots); imon (2 slots);
	vpmon (1 slot) vbstmon (1 slot) */

	slot = ffs(tx_mask) - 1;
	slot_num = 0;

	for (i = 0; i < 2 ; i++) {
		/* disable vpmon/vbstmon: enable later if set in tx_mask */
		snd_soc_update_bits(codec, CS35L33_TX_VPMON + i,
			X_STATE | X_LOC, X_STATE | X_LOC);
	}

	/* disconnect {vp,vbst}_mon routes: eanble later if set in tx_mask*/
	snd_soc_dapm_del_routes(&codec->dapm,
		cs35l33_vp_vbst_mon_route,
		ARRAY_SIZE(cs35l33_vp_vbst_mon_route));

	while (slot >= 0) {
		/* configure VMON_TX_LOC */
		if (slot_num == 0) {
			snd_soc_update_bits(codec, CS35L33_TX_VMON,
				X_STATE | X_LOC, slot);
			dev_dbg(codec->dev, "VMON enabled in slots %d-%d",
				slot, slot + 1);
		}

		/* configure IMON_TX_LOC */
		if (slot_num == 2) {
			snd_soc_update_bits(codec, CS35L33_TX_IMON,
				X_STATE | X_LOC, slot);
			dev_dbg(codec->dev, "IMON enabled in slots %d-%d",
				slot, slot + 1);
		}

		/* configure VPMON_TX_LOC */
		if (slot_num == 4) {
			snd_soc_update_bits(codec, CS35L33_TX_VPMON,
				X_STATE | X_LOC, slot);
			snd_soc_dapm_add_routes(&codec->dapm,
				&cs35l33_vp_vbst_mon_route[0], 2);
			dev_dbg(codec->dev, "VPMON enabled in slots %d", slot);
		}

		/* configure VBSTMON_TX_LOC */
		if (slot_num == 5) {
			snd_soc_update_bits(codec, CS35L33_TX_VBSTMON,
				X_STATE | X_LOC, slot);
			snd_soc_dapm_add_routes(&codec->dapm,
				&cs35l33_vp_vbst_mon_route[2], 2);
			dev_dbg(codec->dev,
				"VBSTMON enabled in slots %d", slot);
		}

		/* Enable the relevant tx slot */
		reg = CS35L33_TX_EN4 - (slot/8);
		bit_pos = slot - ((slot / 8) * (8));
		snd_soc_update_bits(codec, reg,
			1 << bit_pos, 1 << bit_pos);

		tx_mask &= ~(1 << slot);
		slot = ffs(tx_mask) - 1;
		slot_num++;
	}

	return 0;
}

static int cs35l33_codec_set_sysclk(struct snd_soc_codec *codec,
		int clk_id, int source, unsigned int freq, int dir)
{
	struct cs35l33_private *cs35l33 = snd_soc_codec_get_drvdata(codec);

	switch (freq) {
	case CS35L33_MCLK_5644:
	case CS35L33_MCLK_6:
	case CS35L33_MCLK_6144:
		snd_soc_update_bits(codec, CS35L33_CLK_CTL,
			MCLKDIV2, 0);
		cs35l33->mclk_int = freq;
		break;
	case CS35L33_MCLK_11289:
	case CS35L33_MCLK_12:
	case CS35L33_MCLK_12288:
		snd_soc_update_bits(codec, CS35L33_CLK_CTL,
			MCLKDIV2, MCLKDIV2);
			cs35l33->mclk_int = freq/2;
		break;
	default:
		cs35l33->mclk_int = 0;
		return -EINVAL;
	}

	dev_dbg(codec->dev,
		"external mclk freq=%d, internal mclk freq=%d\n",
		freq, cs35l33->mclk_int);

	return 0;
}

static const struct snd_soc_dai_ops cs35l33_ops = {
	.startup = cs35l33_pcm_startup,
	.set_tristate = cs35l33_set_tristate,
	.set_fmt = cs35l33_set_dai_fmt,
	.hw_params = cs35l33_pcm_hw_params,
	.set_tdm_slot = cs35l33_set_tdm_slot,
};

static struct snd_soc_dai_driver cs35l33_dai = {
		.name = "cs35l33",
		.id = 0,
		.playback = {
			.stream_name = "CS35L33 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS35L33_RATES,
			.formats = CS35L33_FORMATS,
		},
		.capture = {
			.stream_name = "CS35L33 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS35L33_RATES,
			.formats = CS35L33_FORMATS,
		},
		.ops = &cs35l33_ops,
		.symmetric_rates = 1,
};

int cs35l33_set_hg_data(struct snd_soc_codec *codec,
			struct cs35l33_pdata *pdata)
{
	struct cs35l33_hg *hg_config = &pdata->hg_config;

	if (hg_config->enable_hg_algo) {
		snd_soc_update_bits(codec, CS35L33_HG_MEMLDO_CTL,
			MEM_DEPTH_MASK,
			hg_config->mem_depth << MEM_DEPTH_SHIFT);
		snd_soc_write(codec, CS35L33_HG_REL_RATE,
			hg_config->release_rate);
		snd_soc_update_bits(codec, CS35L33_HG_HEAD,
			HD_RM_MASK,
			hg_config->hd_rm << HD_RM_SHIFT);
		snd_soc_update_bits(codec, CS35L33_HG_MEMLDO_CTL,
			LDO_THLD_MASK,
			hg_config->ldo_thld << LDO_THLD_SHIFT);
		snd_soc_update_bits(codec, CS35L33_HG_MEMLDO_CTL,
			LDO_DISABLE_MASK,
			hg_config->ldo_path_disable << LDO_DISABLE_SHIFT);
		snd_soc_update_bits(codec, CS35L33_LDO_DEL,
			LDO_ENTRY_DELAY_MASK,
			hg_config->ldo_entry_delay << LDO_ENTRY_DELAY_SHIFT);
		if (hg_config->vp_hg_auto) {
			dev_info(codec->dev,
				"H/G algorithm (with auto vp tracking) controls the amplifier voltage\n");
			snd_soc_update_bits(codec, CS35L33_HG_EN,
				VP_HG_AUTO_MASK,
				VP_HG_AUTO_MASK);
			snd_soc_dapm_add_routes(&codec->dapm,
				cs35l33_vphg_auto_route,
				ARRAY_SIZE(cs35l33_vphg_auto_route));
		} else {
			dev_info(codec->dev, "H/G algorithm controls the amplifier voltage\n");
		}
		snd_soc_update_bits(codec, CS35L33_HG_EN,
			VP_HG_MASK,
			hg_config->vp_hg << VP_HG_SHIFT);
		snd_soc_update_bits(codec, CS35L33_LDO_DEL,
			VP_HG_RATE_MASK,
			hg_config->vp_hg_rate << VP_HG_RATE_SHIFT);
		snd_soc_update_bits(codec, CS35L33_LDO_DEL,
			VP_HG_VA_MASK,
			hg_config->vp_hg_va << VP_HG_VA_SHIFT);
		snd_soc_update_bits(codec, CS35L33_HG_EN,
			CLASS_HG_EN_MASK, CLASS_HG_EN_MASK);
	} else {
		dev_info(codec->dev, "Amplifier voltage controlled via the I2C port\n");
	}

	return 0;
}

static int cs35l33_probe(struct snd_soc_codec *codec)
{
	struct cs35l33_private *cs35l33 = snd_soc_codec_get_drvdata(codec);
	u8 reg;

	cs35l33->codec = codec;

	pm_runtime_get_sync(codec->dev);

	reg = snd_soc_read(codec, CS35L33_PROTECT_CTL);
	reg &= ~(1 << 2);
	reg |= 1 << 3;
	snd_soc_write(codec, CS35L33_PROTECT_CTL, reg);
	snd_soc_update_bits(codec, CS35L33_BST_CTL2,
				    ALIVE_WD_DIS2, ALIVE_WD_DIS2);


	/* Set Platform Data */
	snd_soc_update_bits(codec, CS35L33_BST_CTL1,
		CS35L33_BST_CTL_MASK,
		cs35l33->pdata.boost_ctl);
	snd_soc_update_bits(codec, CS35L33_CLASSD_CTL,
		AMP_DRV_SEL_MASK,
		cs35l33->pdata.amp_drv_sel << AMP_DRV_SEL_SHIFT);

	if (cs35l33->pdata.boost_ipk)
		snd_soc_write(codec, CS35L33_BST_PEAK_CTL,
			cs35l33->pdata.boost_ipk);

	if (cs35l33->enable_soft_ramp) {
		snd_soc_update_bits(codec, CS35L33_DAC_CTL,
			DIGSFT, DIGSFT);
		snd_soc_update_bits(codec, CS35L33_DAC_CTL,
			DSR_RATE, cs35l33->pdata.ramp_rate);
	} else {
		snd_soc_update_bits(codec, CS35L33_DAC_CTL,
			DIGSFT, 0);
		snd_soc_update_bits(codec, CS35L33_DAC_CTL,
			DSR_RATE, 0);
	}

	cs35l33_set_hg_data(codec, &(cs35l33->pdata));

	/* unmask important interrupts that causes the chip to enter
	speaker safe mode and hence deserves user attention */
	snd_soc_update_bits(codec, CS35L33_INT_MASK_1,
		M_OTE | M_OTW | M_AMP_SHORT |
		M_CAL_ERR, 0);

	pm_runtime_put_sync(codec->dev);

	return 0;
}

static int cs35l33_remove(struct snd_soc_codec *codec)
{
	cs35l33_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static struct regmap *cs35l33_get_regmap(struct device *dev)
{
	struct cs35l33_private *cs35l33 = dev_get_drvdata(dev);

	return cs35l33->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_cs35l33 = {
	.probe = cs35l33_probe,
	.remove = cs35l33_remove,

	.get_regmap = cs35l33_get_regmap,
	.set_bias_level = cs35l33_set_bias_level,
	.set_sysclk = cs35l33_codec_set_sysclk,

	.dapm_widgets = cs35l33_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs35l33_dapm_widgets),
	.dapm_routes = cs35l33_audio_map,
	.num_dapm_routes = ARRAY_SIZE(cs35l33_audio_map),
	.controls = cs35l33_snd_controls,
	.num_controls = ARRAY_SIZE(cs35l33_snd_controls),

	.idle_bias_off = true,
};

static struct regmap_config cs35l33_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = CS35L33_MAX_REGISTER,
	.reg_defaults = cs35l33_reg,
	.num_reg_defaults = ARRAY_SIZE(cs35l33_reg),
	.volatile_reg = cs35l33_volatile_register,
	.readable_reg = cs35l33_readable_register,
	.writeable_reg = cs35l33_writeable_register,
	.cache_type = REGCACHE_RBTREE,
	.use_single_rw = true,
};

static inline void cs35l33_wait_for_boot(void)
{
	msleep(50);
}

static int cs35l33_runtime_resume(struct device *dev)
{
	struct cs35l33_private *cs35l33 = dev_get_drvdata(dev);
	int ret;

	dev_dbg(cs35l33->codec->dev,
		"turn On regulator supplies and disable cache only mode\n");

	if (cs35l33->pdata.gpio_nreset > 0)
		gpio_set_value_cansleep(cs35l33->pdata.gpio_nreset, 0);

	ret = regulator_bulk_enable(cs35l33->num_core_supplies,
		cs35l33->core_supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to enable core supplies: %d\n",
			ret);
		return ret;
	}

	regcache_cache_only(cs35l33->regmap, false);

	if (cs35l33->pdata.gpio_nreset > 0)
		gpio_set_value_cansleep(cs35l33->pdata.gpio_nreset, 1);

	cs35l33_wait_for_boot();

	ret = regcache_sync(cs35l33->regmap);
	if (ret != 0) {
		dev_err(dev, "Failed to restore register cache\n");
		goto err;
	}

	return 0;

err:
	regcache_cache_only(cs35l33->regmap, true);
	regulator_bulk_disable(cs35l33->num_core_supplies,
		cs35l33->core_supplies);

	return ret;
}

static int cs35l33_runtime_suspend(struct device *dev)
{
	struct cs35l33_private *cs35l33 = dev_get_drvdata(dev);

	dev_dbg(cs35l33->dev,
		"turn Off regulator supplies and enable cache only mode\n");

	/* redo the calibration in next power up */
	cs35l33->amp_cal = false;

	regcache_cache_only(cs35l33->regmap, true);
	regcache_mark_dirty(cs35l33->regmap);
	regulator_bulk_disable(cs35l33->num_core_supplies,
		cs35l33->core_supplies);

	return 0;
}

const struct dev_pm_ops cs35l33_pm_ops = {
	SET_RUNTIME_PM_OPS(cs35l33_runtime_suspend,
			   cs35l33_runtime_resume,
			   NULL)
};

int cs35l33_get_hg_data(const struct device_node *np,
			struct cs35l33_pdata *pdata)
{
	struct device_node *hg;
	struct cs35l33_hg *hg_config = &pdata->hg_config;
	u32 val32;

	hg = of_get_child_by_name(np, "hg-algo");
	hg_config->enable_hg_algo = hg ? true : false;

	if (hg_config->enable_hg_algo) {
		if (of_property_read_u32(hg, "mem-depth", &val32) >= 0)
			hg_config->mem_depth = val32;
		if (of_property_read_u32(hg, "release-rate", &val32) >= 0)
			hg_config->release_rate = val32;
		if (of_property_read_u32(hg, "hd-rm", &val32) >= 0)
			hg_config->hd_rm = val32;
		if (of_property_read_u32(hg, "ldo-thld", &val32) >= 0)
			hg_config->ldo_thld = val32;
		if (of_property_read_u32(hg, "ldo-path-disable", &val32) >= 0)
			hg_config->ldo_path_disable = val32;
		if (of_property_read_u32(hg, "ldo-entry-delay", &val32) >= 0)
			hg_config->ldo_entry_delay = val32;
		hg_config->vp_hg_auto = of_property_read_bool(hg, "vp-hg-auto");
		if (of_property_read_u32(hg, "vp-hg", &val32) >= 0)
			hg_config->vp_hg = val32;
		if (of_property_read_u32(hg, "vp-hg-rate", &val32) >= 0)
			hg_config->vp_hg_rate = val32;
		if (of_property_read_u32(hg, "vp-hg-va", &val32) >= 0)
			hg_config->vp_hg_va = val32;
	}

	of_node_put(hg);

	return 0;
}

static irqreturn_t cs35l33_irq_thread(int irq, void *data)
{
	struct cs35l33_private *cs35l33 = data;
	struct snd_soc_codec *codec = cs35l33->codec;
	unsigned int sticky_val, current_val;
	bool poll = false;

	do {
		dev_dbg(codec->dev, "t-bone irq received\n");

		if (cs35l33->pdata.gpio_irq > 0) {
			if (gpio_get_value_cansleep(cs35l33->pdata.gpio_irq))
				break;
			poll = true;
		}

		/* ack the irq by reading both status registers */
		regmap_read(cs35l33->regmap, CS35L33_INT_STATUS_2, &sticky_val);
		regmap_read(cs35l33->regmap, CS35L33_INT_STATUS_1, &sticky_val);

		/* read the current value */
		regmap_read(cs35l33->regmap, CS35L33_INT_STATUS_1,
			&current_val);

		/* handle the interrupts */

		if (sticky_val & AMP_SHORT) {
			dev_err(codec->dev,
				"t-bone entering speaker safe mode on amp short error\n");
			if (!(current_val & AMP_SHORT)) {
				dev_dbg(codec->dev,
					"t-bone - executing amp short error release sequence\n");
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, AMP_SHORT_RLS, 0);
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, AMP_SHORT_RLS,
					AMP_SHORT_RLS);
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, AMP_SHORT_RLS, 0);
			}
		}

		if (sticky_val & CAL_ERR) {
			dev_err(codec->dev,
				"t-bone entering speaker safe mode on cal error\n");

			/* redo the calibration in next power up */
			cs35l33->amp_cal = false;

			if (!(current_val & CAL_ERR)) {
				dev_dbg(codec->dev,
					"t-bone - executing cal error release sequence\n");
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, CAL_ERR_RLS, 0);
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, CAL_ERR_RLS,
					CAL_ERR_RLS);
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, CAL_ERR_RLS, 0);
			}
		}

		if (sticky_val & OTE) {
			dev_err(codec->dev,
				"t-bone entering speaker safe mode on over tempreature error\n");
			if (!(current_val & OTE)) {
				dev_dbg(codec->dev,
					"t-bone - executing over tempreature release sequence\n");
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, OTE_RLS, 0);
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, OTE_RLS, OTE_RLS);
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, OTE_RLS, 0);
			}
		}

		if (sticky_val & OTW) {
			dev_err(codec->dev,
				"t-bone entering speaker safe mode on over tempreature warning\n");
			if (!(current_val & OTW)) {
				dev_dbg(codec->dev,
					"t-bone - executing over tempreature warning sequence\n");
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, OTW_RLS, 0);
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, OTW_RLS, OTW_RLS);
				regmap_update_bits(cs35l33->regmap,
					CS35L33_AMP_CTL, OTW_RLS, 0);
			}
		}

		if (sticky_val & ALIVE_ERR)
			dev_err(codec->dev,
				"t-bone entering speaker safe mode on alive error\n");
	} while (poll);

	return IRQ_HANDLED;
}

static const char * const cs35l33_core_supplies[] = {
	"VA",
	"VP",
};

static int cs35l33_i2c_probe(struct i2c_client *i2c_client,
				       const struct i2c_device_id *id)
{
	struct cs35l33_private *cs35l33;
	struct cs35l33_pdata *pdata = dev_get_platdata(&i2c_client->dev);
	int ret, devid, i;
	unsigned int reg;
	u32 val32;

	dev_err(&i2c_client->dev, "%s enter\n", __func__);

	cs35l33 = kzalloc(sizeof(struct cs35l33_private), GFP_KERNEL);
	if (!cs35l33)
		return -ENOMEM;

	cs35l33->dev = &i2c_client->dev;
	i2c_set_clientdata(i2c_client, cs35l33);
	cs35l33->regmap = regmap_init_i2c(i2c_client, &cs35l33_regmap);
	if (IS_ERR(cs35l33->regmap)) {
		ret = PTR_ERR(cs35l33->regmap);
		dev_err(&i2c_client->dev, "regmap_init() failed: %d\n", ret);
		goto err;
	}

	regcache_cache_only(cs35l33->regmap, true);

	for (i = 0; i < ARRAY_SIZE(cs35l33_core_supplies); i++)
		cs35l33->core_supplies[i].supply
			= cs35l33_core_supplies[i];
	cs35l33->num_core_supplies = ARRAY_SIZE(cs35l33_core_supplies);

	ret = devm_regulator_bulk_get(&i2c_client->dev,
			cs35l33->num_core_supplies,
			cs35l33->core_supplies);
	if (ret != 0) {
		dev_err(&i2c_client->dev, "Failed to request core supplies: %d\n",
			ret);
		goto err_regmap;
	}

	if (pdata) {
		cs35l33->pdata = *pdata;
	} else {
		pdata = devm_kzalloc(&i2c_client->dev,
				     sizeof(struct cs35l33_pdata),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c_client->dev, "could not allocate pdata\n");
			return -ENOMEM;
		}
		if (i2c_client->dev.of_node) {
			if (of_property_read_u32(i2c_client->dev.of_node,
				"boost-ctl", &val32) >= 0) {
				pdata->boost_ctl = val32;
				pdata->amp_drv_sel = 1;
			} else {
				pdata->boost_ctl = 0;
				pdata->amp_drv_sel = 0;
			}

			if (of_property_read_u32(i2c_client->dev.of_node,
				"ramp-rate", &val32) >= 0) {
				pdata->ramp_rate = val32;
				cs35l33->enable_soft_ramp = true;
			} else
				cs35l33->enable_soft_ramp = false;

			pdata->gpio_nreset = of_get_named_gpio(
						i2c_client->dev.of_node,
						"reset-gpios", 0);

			if (of_property_read_u32(i2c_client->dev.of_node,
				"boost-ipk", &val32) >= 0)
				pdata->boost_ipk = val32;

			pdata->irq = irq_of_parse_and_map(
					i2c_client->dev.of_node, 0);
			pdata->gpio_irq = of_get_named_gpio(
						i2c_client->dev.of_node,
						"irq-gpios", 0);

			cs35l33_get_hg_data(i2c_client->dev.of_node, pdata);
		}
		cs35l33->pdata = *pdata;
	}

	if (pdata->gpio_irq > 0) {
		if (gpio_to_irq(pdata->gpio_irq) != pdata->irq) {
			dev_err(&i2c_client->dev, "IRQ %d is not GPIO %d (%d)\n",
				 pdata->irq, pdata->gpio_irq,
				 gpio_to_irq(pdata->gpio_irq));
			pdata->irq = gpio_to_irq(pdata->gpio_irq);
		}

		ret = devm_gpio_request_one(&i2c_client->dev,
						pdata->gpio_irq,
						GPIOF_IN, "cs35l33 IRQ");
		if (ret != 0) {
			dev_err(&i2c_client->dev, "Failed to request IRQ GPIO %d:: %d\n",
				pdata->gpio_irq, ret);
			pdata->gpio_irq = 0;
		}
	}

	ret = request_threaded_irq(pdata->irq, NULL, cs35l33_irq_thread,
			IRQF_ONESHOT | IRQF_TRIGGER_LOW, "cs35l33", cs35l33);
	if (ret != 0)
		dev_err(&i2c_client->dev, "Failed to request irq\n");

	/* We could issue !RST or skip it based on AMP topology */
	if (cs35l33->pdata.gpio_nreset > 0) {
		ret = devm_gpio_request_one(&i2c_client->dev,
				cs35l33->pdata.gpio_nreset,
				GPIOF_DIR_OUT | GPIOF_INIT_LOW,
				"CS35L33 /RESET");
		if (ret != 0) {
			dev_err(&i2c_client->dev,
				"Failed to request /RESET: %d\n", ret);
			goto err_regmap;
		} else
			dev_info(&i2c_client->dev, "Success to request RESET\n");
	}

	ret = regulator_bulk_enable(cs35l33->num_core_supplies,
					cs35l33->core_supplies);
	if (ret != 0) {
		dev_err(&i2c_client->dev, "Failed to enable core supplies: %d\n",
			ret);
		goto err_regmap;
	}

	if (cs35l33->pdata.gpio_nreset > 0)
		gpio_set_value_cansleep(cs35l33->pdata.gpio_nreset, 1);

	cs35l33_wait_for_boot();
	regcache_cache_only(cs35l33->regmap, false);

	/* initialize codec */
	ret = regmap_read(cs35l33->regmap, CS35L33_DEVID_AB, &reg);
	devid = (reg & 0xFF) << 12;
	ret = regmap_read(cs35l33->regmap, CS35L33_DEVID_CD, &reg);
	devid |= (reg & 0xFF) << 4;
	ret = regmap_read(cs35l33->regmap, CS35L33_DEVID_E, &reg);
	devid |= (reg & 0xF0) >> 4;

	if (devid != CS35L33_CHIP_ID) {
		dev_err(&i2c_client->dev,
			"CS35L33 Device ID (%X). Expected ID %X\n",
			devid, CS35L33_CHIP_ID);
		goto err_enable;
	}

	ret = regmap_read(cs35l33->regmap, CS35L33_REV_ID, &reg);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "Get Revision ID failed\n");
		goto err_enable;
	}

	dev_info(&i2c_client->dev,
		 "Cirrus Logic CS35L33, Revision: %02X\n", ret & 0xFF);

	ret = regmap_register_patch(cs35l33->regmap,
			cs35l33_patch, ARRAY_SIZE(cs35l33_patch));
	if (ret < 0) {
		dev_err(&i2c_client->dev,
			"Error in applying regmap patch: %d\n", ret);
		goto err_enable;
	}

	/* disable mclk and tdm */
	regmap_update_bits(cs35l33->regmap, CS35L33_CLK_CTL,
		MCLKDIS |SDOUT_3ST_TDM,
		MCLKDIS |SDOUT_3ST_TDM);

	pm_runtime_set_autosuspend_delay(&i2c_client->dev, 100);
	pm_runtime_use_autosuspend(&i2c_client->dev);
	pm_runtime_set_active(&i2c_client->dev);
	pm_runtime_enable(&i2c_client->dev);

	ret =  snd_soc_register_codec(&i2c_client->dev,
			&soc_codec_dev_cs35l33, &cs35l33_dai, 1);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "%s: Register codec failed\n",
			__func__);
		goto err_regmap;
	}


	return 0;

err_enable:
	regulator_bulk_disable(cs35l33->num_core_supplies,
			       cs35l33->core_supplies);
err_regmap:
	regmap_exit(cs35l33->regmap);
err:
	return ret;
}

static int cs35l33_i2c_remove(struct i2c_client *client)
{
	struct cs35l33_private *cs35l33 = i2c_get_clientdata(client);

	snd_soc_unregister_codec(&client->dev);
	if (cs35l33->pdata.gpio_nreset > 0)
		gpio_set_value_cansleep(cs35l33->pdata.gpio_nreset, 0);
	pm_runtime_disable(&client->dev);
	regulator_bulk_disable(cs35l33->num_core_supplies,
		cs35l33->core_supplies);
	free_irq(cs35l33->pdata.irq, cs35l33);
	kfree(i2c_get_clientdata(client));

	return 0;
}

static const struct of_device_id cs35l33_of_match[] = {
	{ .compatible = "cirrus,cs35l33", },
	{},
};
MODULE_DEVICE_TABLE(of, cs35l33_of_match);

static const struct i2c_device_id cs35l33_id[] = {
	{"cs35l33", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cs35l33_id);

static struct i2c_driver cs35l33_i2c_driver = {
	.driver = {
		.name = "cs35l33",
		.owner = THIS_MODULE,
		.pm = &cs35l33_pm_ops,
		.of_match_table = cs35l33_of_match,

		},
	.id_table = cs35l33_id,
	.probe = cs35l33_i2c_probe,
	.remove = cs35l33_i2c_remove,

};
module_i2c_driver(cs35l33_i2c_driver);

MODULE_DESCRIPTION("ASoC CS35L33 driver");
MODULE_AUTHOR("Paul Handrigan, Cirrus Logic Inc, <paul.handrigan@cirrus.com>");
MODULE_LICENSE("GPL");
