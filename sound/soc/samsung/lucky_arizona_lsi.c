/*
 *  lucky_arizona.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/tlv.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#if (defined CONFIG_SWITCH_ANTENNA_EARJACK \
	 || defined CONFIG_SWITCH_ANTENNA_EARJACK_IF) \
	 && (!defined CONFIG_SEC_FACTORY)
#include <linux/antenna_switch.h>
#endif
#include <linux/mfd/arizona/registers.h>
#include <linux/mfd/arizona/core.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/florida.h"
#include "../codecs/clearwater.h"
#include "../codecs/moon.h"

#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
#include <sound/maxim_dsm_cal.h>
#endif

#include <sound/samsung_audio_debugfs.h>

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
#include "./seiren/seiren.h"
#endif
#ifdef CONFIG_SND_ESA_SA_EFFECT
#include "esa_sa_effect.h"
#endif


#define LUCKY_32K_MCLK2	32768

#define LUCKY_RUN_MAINMIC	2
#define LUCKY_RUN_EARMIC	1

/* XXX_FLLn is CODEC_FLLn */
#define LUCKY_FLL1        1
#define LUCKY_FLL2        2
#define LUCKY_FLL1_REFCLK 3
#define LUCKY_FLL2_REFCLK 4
#define LUCKY_FLL3        5
#define LUCKY_FLL3_REFCLK 6
#define LUCKY_FLLAO      5

#define DSP6_XM_BASE 0x320000
#define DSP6_ZM_BASE 0x360000
#define EZ2CTRL_WORDID_OFF 0x4A
#define SENSORY_PID_SVSCORE 0x4F
#define SENSORY_PID_FINALSCORE 0x50
#define PID_NOISEFLOOR 0x54

#if defined(CONFIG_MFD_CLEARWATER) || defined(CONFIG_MFD_MOON)
#define LUCKY_AIF_MAX	4
#else
#define LUCKY_AIF_MAX	3
#endif

static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);

enum {
	LUCKY_PLAYBACK_DAI,
	LUCKY_VOICECALL_DAI,
	LUCKY_BT_SCO_DAI,
	LUCKY_MULTIMEDIA_DAI,
};

struct snd_mclk_info {
	int id;
	int rate;

	struct clk *clk;
	int gpio;
};

struct snd_sysclk_info {
	struct snd_mclk_info mclk;

	int clk_id;
	int src_id;
	int rate;

	int fll_id;
	int fll_src;
	int fll_in;
	int fll_out;

	int fll_ref_id;
	int fll_ref_src;
	int fll_ref_in;
	int fll_ref_out;
};

struct arizona_machine_priv {
	int mic_bias_gpio;
	int clk_sel_gpio;
	struct snd_soc_codec *codec;
	struct snd_soc_dai *aif[LUCKY_AIF_MAX];
	int voice_uevent;
	int seamless_voicewakeup;
	int voicetriginfo;
	struct input_dev *input;
	int voice_key_event;
	struct clk *mclk;
	struct wake_lock wake_lock;

	unsigned int hp_impedance_step;
	bool ear_mic;

	struct snd_sysclk_info sysclk;
	struct snd_sysclk_info asyncclk;
	struct snd_sysclk_info dspclk;

	int word_id_addr;
	int sv_score_addr;
	int final_score_addr;
	int noise_floor_addr;
	int energy_l_addr;
	int energy_r_addr;

	struct regmap *regmap_dsp;

	u32 aif_format[3];
	u32 aif_format_tdm[3];

	int (*external_amp)(int onoff);

#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
	int dsm_cal_read_cnt;
	int dsm_cal_rdc;
	int dsm_cal_temp;
#endif
};

extern void update_cp_available(bool cpen);

static struct class *svoice_class;
static struct device *keyword_dev;
static unsigned int keyword_type;

static struct snd_mclk_info lucky_mclk_data[] = {
	{
		.rate = 32768,
		.id = ARIZONA_CLK_SRC_MCLK2,
	},
	{
		.rate = 26000000,
		.id = ARIZONA_CLK_SRC_MCLK1,
	},
	{
		.rate = 22579200,
		.id = ARIZONA_CLK_SRC_MCLK1,
	},
};

static struct arizona_machine_priv lucky_cs47l91_priv = {
	.word_id_addr = DSP6_XM_BASE + (EZ2CTRL_WORDID_OFF * 2),
	.sv_score_addr = DSP6_XM_BASE + (SENSORY_PID_SVSCORE * 2),
	.final_score_addr = DSP6_XM_BASE + (SENSORY_PID_FINALSCORE * 2),
	.noise_floor_addr = DSP6_XM_BASE + (PID_NOISEFLOOR * 2),
	.energy_l_addr = CLEARWATER_DSP3_SCRATCH_0_1,
	.energy_r_addr = CLEARWATER_DSP3_SCRATCH_2_3,

	.sysclk = {
		.clk_id = ARIZONA_CLK_SYSCLK,
		.src_id = ARIZONA_CLK_SRC_FLL1,
		.rate = 98304000,

		.fll_id = LUCKY_FLL1,
		.fll_out = 98304000,
		.fll_ref_id = LUCKY_FLL1_REFCLK,
		.fll_ref_src = ARIZONA_FLL_SRC_NONE,
	},
	.asyncclk = {
		.clk_id = ARIZONA_CLK_ASYNCCLK,
		.src_id = ARIZONA_CLK_SRC_FLL2,
		.rate = 98304000,

		.fll_id = LUCKY_FLL2,
		.fll_out = 98304000,
		.fll_ref_id = LUCKY_FLL2_REFCLK,
		.fll_ref_src = ARIZONA_FLL_SRC_NONE,
	},
	.dspclk = {
		.clk_id = ARIZONA_CLK_DSPCLK,
		.src_id = ARIZONA_CLK_SRC_FLLAO_HI,
		.rate = 147456000,

		.fll_id = LUCKY_FLLAO,
		.fll_out = 49152000,
	},
};

static const char * const voicecontrol_mode_text[] = {
	"Normal", "LPSD"
};

static const struct soc_enum voicecontrol_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(voicecontrol_mode_text),
			voicecontrol_mode_text),
};

static const struct snd_soc_component_driver lucky_cmpnt = {
	.name	= "lucky-audio",
};

static struct {
	int min;           /* Minimum impedance */
	int max;           /* Maximum impedance */
	unsigned int gain; /* Register value to set for this measurement */
} hp_gain_table[] = {
	{    0,      13, 0 },
	{   14,      42, 0 },
	{   43,     100, 4 },
	{  101,     200, 8 },
	{  201,     450, 12 },
	{  451,    1000, 14 },
	{ 1001, INT_MAX, 0 },
};

enum mclk_src {
	CLK_26M = 1 << 0,
	CLK_22M = 1 << 1,
	CLK_32K = 1 << 2,
};


static struct snd_soc_codec *the_codec;

static int clk_cnt[2];

static void enable_mclk(struct snd_soc_card *card, struct snd_mclk_info *mclk)
{
	struct arizona_machine_priv *priv = card->drvdata;

	if (mclk->clk) {
		clk_enable(mclk->clk);
		clk_cnt[0]++;
	}

	if (mclk->gpio) {
		if (gpio_is_valid(mclk->gpio)) {
			gpio_set_value(mclk->gpio, 1);
			clk_cnt[1]++;
		}
	}

	dev_info(priv->codec->dev, "26MHz[%d], 22.576MHz[%d]\n",
						clk_cnt[0], clk_cnt[1]);
}

static void disable_mclk(struct snd_soc_card *card, struct snd_mclk_info *mclk)
{
	struct arizona_machine_priv *priv = card->drvdata;

	if (mclk->clk) {
		clk_disable(mclk->clk);
		clk_cnt[0]--;
	}

	if (mclk->gpio) {
		if (gpio_is_valid(mclk->gpio)) {
			gpio_set_value(mclk->gpio, 0);
			clk_cnt[1]--;
		}
	}

	dev_info(priv->codec->dev, "26MHz[%d], 22.576MHz[%d]\n",
						clk_cnt[0], clk_cnt[1]);
}

static void lucky_change_mclk(struct snd_soc_card *card, int rate)
{
	struct arizona_machine_priv *priv = card->drvdata;
	struct snd_mclk_info *mclk;
	int sysclk_rate;

	if (rate == 0) {
		mclk = &lucky_mclk_data[0];
		sysclk_rate = 98304000;
	} else if (rate % 8000) {
		mclk = &lucky_mclk_data[2];
		sysclk_rate = 90316800;
	} else {
		mclk = &lucky_mclk_data[1];
		sysclk_rate = 98304000;
	}

	if (priv->sysclk.mclk.id != mclk->id ||
	    priv->sysclk.mclk.rate != mclk->rate ||
	    priv->sysclk.rate != sysclk_rate) {
		disable_mclk(card, &priv->sysclk.mclk);
		enable_mclk(card, mclk);

		memcpy(&priv->sysclk.mclk, mclk, sizeof(struct snd_mclk_info));
		priv->sysclk.rate = sysclk_rate;
		priv->sysclk.fll_out = sysclk_rate;

		if (priv->asyncclk.fll_ref_src == ARIZONA_FLL_SRC_NONE)
			memcpy(&priv->asyncclk.mclk, mclk,
					sizeof(struct snd_mclk_info));
	} else {
		dev_dbg(priv->codec->dev,
				"%s: skipping to change mclk\n", __func__);
	}

	dev_info(priv->codec->dev, "%s: %d [%d]\n",
					__func__, rate, mclk->rate);

	return;
}

void lucky_arizona_hpdet_cb(unsigned int meas)
{
	struct arizona_machine_priv *priv;
	int jack_det;
	int i, num_hp_gain_table;

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	priv = the_codec->component.card->drvdata;

	if (meas == ARIZONA_HP_Z_OPEN) {
		jack_det = 0;
	} else {
		jack_det = 1;
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		/* ToDo
		esa_compr_hpdet_notifier(true);
		*/
#endif
	}

	dev_info(the_codec->dev, "%s(%d) meas(%d)\n", __func__, jack_det, meas);

#if (defined CONFIG_SWITCH_ANTENNA_EARJACK \
	 || defined CONFIG_SWITCH_ANTENNA_EARJACK_IF) \
	 && (!defined CONFIG_SEC_FACTORY)
	/* Notify jack condition to other devices */
	antenna_switch_work_earjack(jack_det);
#endif

	num_hp_gain_table = (int) ARRAY_SIZE(hp_gain_table);
	for (i = 0; i < num_hp_gain_table; i++) {
		if (meas < hp_gain_table[i].min || meas > hp_gain_table[i].max)
			continue;

		dev_info(the_codec->dev, "SET GAIN %d step for %d ohms\n",
			 hp_gain_table[i].gain, meas);
		priv->hp_impedance_step = hp_gain_table[i].gain;
	}
}

void lucky_arizona_micd_cb(bool mic)
{
	struct arizona_machine_priv *priv;

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	priv = the_codec->component.card->drvdata;

	priv->ear_mic = mic;
	dev_info(the_codec->dev, "%s: ear_mic = %d\n", __func__, priv->ear_mic);
}

void lucky_update_impedance_table(struct device_node *np)
{
	int len = ARRAY_SIZE(hp_gain_table);
	u32 data[len * 3];
	int i, shift;

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	if (!of_property_read_u32_array(np, "imp_table", data, (len * 3))) {
		dev_info(the_codec->dev, "%s: data from DT\n", __func__);

		for (i = 0; i < len; i++) {
			hp_gain_table[i].min = data[i * 3];
			hp_gain_table[i].max = data[(i * 3) + 1];
			hp_gain_table[i].gain = data[(i * 3) + 2];
		}
	}

	if (!of_property_read_u32(np, "imp_shift", &shift)) {
		dev_info(the_codec->dev, "%s: shift = %d\n", __func__, shift);

		for (i = 0; i < len; i++) {
			if (hp_gain_table[i].min != 0)
				hp_gain_table[i].min += shift;
			if ((hp_gain_table[i].max + shift) < INT_MAX)
				hp_gain_table[i].max += shift;
		}
	}
}

static int arizona_put_impedance_volsw(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona_machine_priv *priv = codec->component.card->drvdata;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int err;
	unsigned int val, val_mask;

	val = (ucontrol->value.integer.value[0] & mask);
	val += priv->hp_impedance_step;
	dev_info(codec->dev,
			 "SET GAIN %d according to impedance, moved %d step\n",
			 val, priv->hp_impedance_step);

	if (invert)
		val = max - val;
	val_mask = mask << shift;
	val = val << shift;

	err = snd_soc_update_bits(codec, reg, val_mask, val);
	if (err < 0)
		return err;

	return err;
}

#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
#define DSM_RDC_ROOM_TEMP	0x2A005C
#define DSM_AMBIENT_TEMP	0x2A0182
static int lucky_get_dsm_cal_info(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	ret = maxdsm_cal_get_temp(&priv->dsm_cal_temp);
	dev_info(card->dev, "temp = 0x%x, ret = %d\n", priv->dsm_cal_temp, ret);
	if (ret < 0)
		return ret;

	ret = maxdsm_cal_get_rdc(&priv->dsm_cal_rdc);
	dev_info(card->dev, "rdc = 0x%x, ret = %d\n", priv->dsm_cal_rdc, ret);
	if (ret < 0)
		return ret;

	if (ret == 0) {
		priv->dsm_cal_rdc = -1;
		priv->dsm_cal_temp = -1;
	}
	priv->dsm_cal_read_cnt = 1;

	return ret;
}

static int lucky_set_dsm_cal_info(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	int ret = 0;

	dev_info(card->dev, "rdc=%d, temp = %d\n",
					priv->dsm_cal_rdc, priv->dsm_cal_temp);

	if (priv->dsm_cal_temp >= 0 && priv->dsm_cal_rdc >= 0) {
		dev_info(card->dev, "write dsm_cal values properly\n");
		regmap_write(priv->regmap_dsp, DSM_AMBIENT_TEMP,
				(unsigned int)priv->dsm_cal_temp);
		regmap_write(priv->regmap_dsp, DSM_RDC_ROOM_TEMP,
				(unsigned int)priv->dsm_cal_rdc);
	}

	return ret;
}
#endif

static int lucky_external_amp(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct arizona_machine_priv *priv = card->drvdata;
	struct snd_soc_codec *codec = priv->codec;

	dev_dbg(codec->dev, "%s: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (priv->external_amp)
			priv->external_amp(1);
#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
		if (priv->dsm_cal_read_cnt == 0)
			lucky_get_dsm_cal_info(card);
		if (priv->dsm_cal_read_cnt == 1)
			lucky_set_dsm_cal_info(card);
#endif
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (priv->external_amp)
			priv->external_amp(0);
		break;
	}

	return 0;
}

static int lucky_ext_mainmicbias(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct arizona_machine_priv *priv = card->drvdata;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (gpio_is_valid(priv->mic_bias_gpio))
			gpio_set_value(priv->mic_bias_gpio, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (gpio_is_valid(priv->mic_bias_gpio))
			gpio_set_value(priv->mic_bias_gpio, 0);
		break;
	}

	dev_err(w->dapm->dev, "Main Mic BIAS: %d\n", event);

	return 0;
}

static const struct snd_kcontrol_new lucky_codec_controls[] = {
	SOC_SINGLE_EXT_TLV("HPOUT1L Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1L,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, arizona_put_impedance_volsw,
		digital_tlv),

	SOC_SINGLE_EXT_TLV("HPOUT1R Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1R,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, arizona_put_impedance_volsw,
		digital_tlv),
};

static const struct snd_kcontrol_new lucky_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Third Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("VI SENSING"),
	SOC_DAPM_PIN_SWITCH("FM In"),
};

const struct snd_soc_dapm_widget lucky_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", lucky_external_amp),
	SND_SOC_DAPM_SPK("RCV", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", lucky_ext_mainmicbias),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Third Mic", NULL),
	SND_SOC_DAPM_MIC("VI SENSING", NULL),
	SND_SOC_DAPM_MIC("FM In", NULL),
};

const struct snd_soc_dapm_route lucky_cs47l91_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

#if 0
	{ "SPK", NULL, "HiFi Playback" },
#endif

	{ "RCV", NULL, "HPOUT3L" },
	{ "RCV", NULL, "HPOUT3R" },

	{ "Main Mic", NULL, "MICBIAS2A" },
	{ "IN1AR", NULL, "Main Mic" },

	{ "Headset Mic", NULL, "MICBIAS1A" },
	{ "IN2BL", NULL, "Headset Mic" },

	{ "Sub Mic", NULL, "MICBIAS2B" },
	{ "IN3L", NULL, "Sub Mic" },

#if 0
	/* "HiFi Capture" is capture stream of maxim_amp dai */
	{ "HiFi Capture", NULL, "VI SENSING" },
#endif

	{ "IN3L", NULL, "FM In" },
	{ "IN3R", NULL, "FM In" },
};

static int lucky_aif_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;

	dev_info(card->dev, "%s\n", __func__);
	dev_info(card->dev, "codec_dai: %s\n", codec_dai->name);
	dev_info(card->dev, "%s-%d playback: %d, capture: %d, active: %d",
			rtd->dai_link->name, substream->stream,
			codec_dai->playback_active, codec_dai->capture_active,
			codec->component.active);

	return 0;
}

static void lucky_aif_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	dev_info(card->dev, "%s\n", __func__);
	dev_info(card->dev, "codec_dai: %s\n", codec_dai->name);
	dev_info(card->dev, "%s-%d playback: %d, capture: %d, active: %d",
			rtd->dai_link->name, substream->stream,
			codec_dai->playback_active, codec_dai->capture_active,
			codec->component.active);

	if (!codec_dai->playback_active && !codec_dai->capture_active &&
	    !codec->component.active) {
		lucky_change_mclk(card, 0);

		/* Set SYSCLK FLL */
		ret = snd_soc_codec_set_pll(priv->codec, priv->sysclk.fll_id,
				    priv->sysclk.mclk.id,
				    priv->sysclk.mclk.rate,
				    priv->sysclk.fll_out);
		if (ret != 0)
			dev_err(card->dev,
				"Failed to start FLL for SYSCLK: %d\n", ret);

		/* Set SYSCLK from FLL*/
		ret = snd_soc_codec_set_sysclk(priv->codec, priv->sysclk.clk_id,
				       priv->sysclk.src_id,
				       priv->sysclk.rate,
				       SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev,
				"Failed to set SYSCLK to FLL: %d\n", ret);
	}

	return;
}

static int lucky_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	dev_info(card->dev, "%s-%d %dch, %dHz, %dbytes\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params),
			params_buffer_bytes(params));

	lucky_change_mclk(card, params_rate(params));

	/* Set SYSCLK FLL */
	ret = snd_soc_codec_set_pll(priv->codec, priv->sysclk.fll_ref_id,
				    priv->sysclk.fll_ref_src,
				    priv->sysclk.fll_ref_in,
				    priv->sysclk.fll_ref_out);
	if (ret != 0)
		dev_err(card->dev, "Failed to start SYSCLK FLL REF: %d\n", ret);

	ret = snd_soc_codec_set_pll(priv->codec, priv->sysclk.fll_id,
				    priv->sysclk.mclk.id,
				    priv->sysclk.mclk.rate,
				    priv->sysclk.fll_out);
	if (ret != 0)
		dev_err(card->dev, "Failed to start FLL for SYSCLK: %d\n", ret);

	/* Set SYSCLK from FLL*/
	ret = snd_soc_codec_set_sysclk(priv->codec, priv->sysclk.clk_id,
				       priv->sysclk.src_id,
				       priv->sysclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL3: %d\n", ret);

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, priv->aif_format[0]);
	if (ret < 0)
		dev_err(card->dev, "Failed to set aif1 codec fmt: %d\n", ret);

	if (priv->aif_format_tdm[0]) {
		ret = snd_soc_dai_set_tdm_slot(codec_dai,
				priv->aif_format_tdm[0] & 0x000F,
				priv->aif_format_tdm[0] & 0x000F,
				(priv->aif_format_tdm[0] & 0x00F0) >> 4,
				(priv->aif_format_tdm[0] & 0xFF00) >> 8);
		if (ret < 0) {
			dev_err(card->dev,
				"Failed to set aif1 tdm slot: %d\n", ret);
			return ret;
		}
	}

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, priv->aif_format[0]);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 cpu fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_CDCL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_OPCL: %d\n", ret);
		return ret;
	}

	/* Set ASYNCCLK FLL  */
	ret = snd_soc_codec_set_pll(priv->codec, priv->asyncclk.fll_ref_id,
				    priv->asyncclk.fll_ref_src,
				    priv->asyncclk.fll_ref_in,
				    priv->asyncclk.fll_ref_out);
	if (ret != 0)
		dev_err(card->dev,
			 "Failed to start ASYNCCLK FLL REF: %d\n", ret);

	ret = snd_soc_codec_set_pll(priv->codec, priv->asyncclk.fll_id,
				    priv->asyncclk.mclk.id,
				    priv->asyncclk.mclk.rate,
				    priv->asyncclk.fll_out);
	if (ret != 0)
		dev_err(card->dev, "Failed to start ASYNCCLK FLL: %d\n", ret);

	/* Set ASYNCCLK from FLL */
	ret = snd_soc_codec_set_sysclk(priv->codec, priv->asyncclk.clk_id,
				       priv->asyncclk.src_id,
				       priv->asyncclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev,
				 "Unable to set ASYNCCLK to FLL: %d\n", ret);

	return ret;
}

static int lucky_aif1_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d\n hw_free",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static int lucky_aif1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d prepare\n",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static int lucky_aif1_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d trigger\n",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static struct snd_soc_ops lucky_aif1_ops = {
	.startup = lucky_aif_startup,
	.shutdown = lucky_aif_shutdown,
	.hw_params = lucky_aif1_hw_params,
	.hw_free = lucky_aif1_hw_free,
	.prepare = lucky_aif1_prepare,
	.trigger = lucky_aif1_trigger,
};

#ifdef CONFIG_SND_SAMSUNG_COMPR
static void lucky_compress_shutdown(struct snd_compr_stream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;

	dev_info(card->dev, "%s shutdown++ codec_dai[%s]:playback=%d codec_active=%d\n",
			rtd->dai_link->name, codec_dai->name,
			codec_dai->playback_active, codec->component.active);

	if (!codec_dai->playback_active && !codec_dai->capture_active &&
	    !codec->component.active)
		lucky_change_mclk(card, 0);

	return;
}

static struct snd_soc_compr_ops lucky_compress_ops = {
	.shutdown = lucky_compress_shutdown,
};
#endif

static int lucky_aif2_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct arizona_machine_priv *priv = rtd->card->drvdata;
	int ret;
	int prate, bclk;

	dev_info(card->dev, "%s-%d %dch, %dHz\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params));

	prate = params_rate(params);
	switch (prate) {
	case 8000:
		bclk = 256000;
		break;
	case 16000:
		bclk = 512000;
		break;
	default:
		dev_warn(card->dev,
			"Unsupported LRCLK %d, falling back to 8000Hz\n",
			(int)params_rate(params));
		bclk = 256000;
	}

	lucky_change_mclk(card, params_rate(params));

	/* Set SYSCLK FLL */
	ret = snd_soc_codec_set_pll(priv->codec, priv->sysclk.fll_ref_id,
				    priv->sysclk.fll_ref_src,
				    priv->sysclk.fll_ref_in,
				    priv->sysclk.fll_ref_out);
	if (ret != 0)
		dev_err(card->dev, "Failed to start SYSCLK FLL REF: %d\n", ret);

	ret = snd_soc_codec_set_pll(priv->codec, priv->sysclk.fll_id,
				    priv->sysclk.mclk.id,
				    priv->sysclk.mclk.rate,
				    priv->sysclk.fll_out);
	if (ret != 0)
		dev_err(card->dev, "Failed to start FLL for SYSCLK: %d\n", ret);

	/* Set SYSCLK from FLL*/
	ret = snd_soc_codec_set_sysclk(priv->codec, priv->sysclk.clk_id,
				       priv->sysclk.src_id,
				       priv->sysclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL3: %d\n", ret);

	/* Set FLL2 */
	if (priv->aif_format[1] & SND_SOC_DAIFMT_CBS_CFS) {
		priv->asyncclk.fll_ref_src = ARIZONA_FLL_SRC_MCLK1;
		priv->asyncclk.fll_ref_in = priv->sysclk.mclk.rate;
		priv->asyncclk.fll_ref_out = priv->sysclk.rate;
		priv->asyncclk.mclk.id = ARIZONA_FLL_SRC_AIF2BCLK;
		priv->asyncclk.mclk.rate = bclk;
	} else {
		priv->asyncclk.fll_ref_src = ARIZONA_FLL_SRC_NONE;
		priv->asyncclk.fll_ref_in = 0;
		priv->asyncclk.fll_ref_out = 0;
		memcpy(&priv->asyncclk.mclk, &priv->sysclk.mclk,
					sizeof(struct snd_mclk_info));
	}

	ret = snd_soc_codec_set_pll(priv->codec, priv->asyncclk.fll_ref_id,
				    priv->asyncclk.fll_ref_src,
				    priv->asyncclk.fll_ref_in,
				    priv->asyncclk.fll_ref_out);
	if (ret != 0)
		dev_err(card->dev,
				"Failed to start ASYNCCLK FLL REF: %d\n", ret);

	ret = snd_soc_codec_set_pll(priv->codec, priv->asyncclk.fll_id,
				    priv->asyncclk.mclk.id,
				    priv->asyncclk.mclk.rate,
				    priv->asyncclk.fll_out);
	if (ret != 0)
		dev_err(card->dev, "Failed to start ASYNCCLK FLL: %d\n", ret);

	/* Set ASYNCCLK from FLL */
	ret = snd_soc_codec_set_sysclk(priv->codec, priv->asyncclk.clk_id,
				       priv->asyncclk.src_id,
				       priv->asyncclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev,
				 "Unable to set ASYNCCLK to FLL: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_ASYNCCLK, 0, 0);
	if (ret < 0)
		dev_err(card->dev, "Failed to enable asyncclk (%d)\n", ret);

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, priv->aif_format[1]);
	if (ret < 0)
		dev_err(card->dev, "Failed to set aif2 codec fmt: %d\n", ret);

	if (priv->aif_format_tdm[1]) {
		ret = snd_soc_dai_set_tdm_slot(codec_dai,
				priv->aif_format_tdm[1] & 0x000F,
				priv->aif_format_tdm[1] & 0x000F,
				(priv->aif_format_tdm[1] & 0x00F0) >> 4,
				(priv->aif_format_tdm[1] & 0xFF00) >> 8);
		if (ret < 0) {
			dev_err(card->dev,
				"Failed to set aif2 tdm slot: %d\n", ret);
			return ret;
		}
	}

	update_cp_available(true);

	return 0;
}

static int lucky_aif2_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct arizona_machine_priv *priv = rtd->card->drvdata;

	dev_dbg(card->dev, "%s-%d\n hw_free",
			rtd->dai_link->name, substream->stream);

	priv->asyncclk.fll_ref_src = ARIZONA_FLL_SRC_NONE;
	priv->asyncclk.fll_ref_in = 0;
	priv->asyncclk.fll_ref_out = 0;
	memcpy(&priv->asyncclk.mclk, &lucky_mclk_data[0],
					sizeof(struct snd_mclk_info));

	update_cp_available(false);

	return 0;
}

static struct snd_soc_ops lucky_aif2_ops = {
	.shutdown = lucky_aif_shutdown,
	.hw_params = lucky_aif2_hw_params,
	.hw_free = lucky_aif2_hw_free,
};

#if 0
static int lucky_aif3_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct arizona_machine_priv *priv = rtd->card->drvdata;
	int ret;

	dev_info(card->dev, "%s-%d %dch, %dHz\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params));

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, priv->aif_format[2]);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif3 codec fmt: %d\n", ret);
		return ret;
	}

	if (priv->aif_format_tdm[2]) {
		ret = snd_soc_dai_set_tdm_slot(codec_dai,
				priv->aif_format_tdm[2] & 0x000F,
				priv->aif_format_tdm[2] & 0x000F,
				(priv->aif_format_tdm[2] & 0x00F0) >> 4,
				(priv->aif_format_tdm[2] & 0xFF00) >> 8);
		if (ret < 0) {
			dev_err(card->dev,
				"Failed to set aif3 tdm slot: %d\n", ret);
			return ret;
		}
	}

	return 0;
}
#endif

/*
static struct snd_soc_ops lucky_aif3_ops = {
	.shutdown = lucky_aif_shutdown,
	.hw_params = lucky_aif3_hw_params,
};
*/

static struct snd_soc_dai_driver lucky_ext_dai[] = {
	{
		.name = "lucky-ext voice call",
		.playback = {
			.channels_min = 1,
			.channels_max = 4,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 4,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "lucky-ext bluetooth sco",
		.playback = {
			.channels_min = 1,
			.channels_max = 4,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
#if defined(CONFIG_SND_SOC_MAX98505) || defined(CONFIG_SND_SOC_MAX98506)
	{
		.name = "lucky-ext dsm",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
#endif
};

#if defined(CONFIG_SND_SOC_MAX98505) || defined(CONFIG_SND_SOC_MAX98506)
static const struct snd_soc_pcm_stream dsm_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};
#endif

static struct snd_soc_dai_link lucky_cs47l91_dai[] = {
	{ /* playback & recording */
		.name = "playback-pri",
		.stream_name = "i2s0-pri",
		.codec_dai_name = "moon-aif1",
		.ops = &lucky_aif1_ops,
	},
#if 0
	{ /* bluetooth sco */
		.name = "bluetooth sco",
		.stream_name = "lucky-ext bluetooth sco",
		.cpu_dai_name = "lucky-ext bluetooth sco",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "moon-aif3",
		.ops = &lucky_aif3_ops,
		.ignore_suspend = 1,
	},
#endif
	{ /* deep buffer playback */
		.name = "playback-sec",
		.stream_name = "i2s0-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "moon-aif1",
		.ops = &lucky_aif1_ops,
	},
	{ /* voice call */
		.name = "baseband",
		.stream_name = "lucky-ext voice call",
		.cpu_dai_name = "lucky-ext voice call",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "moon-aif2",
		.ops = &lucky_aif2_ops,
		.ignore_suspend = 1,
	},
#if 0
	{
		.name = "CPU-DSP Voice Control",
		.stream_name = "CPU-DSP Voice Control",
		.cpu_dai_name = "moon-cpu6-voicectrl",
		.platform_name = "moon-codec",
		.codec_dai_name = "moon-dsp6-voicectrl",
		.codec_name = "moon-codec",
	},
	{ /* pcm dump interface */
		.name = "CPU-DSP trace",
		.stream_name = "CPU-DSP trace",
		.cpu_dai_name = "moon-cpu-trace",
		.platform_name = "moon-codec",
		.codec_dai_name = "moon-dsp-trace",
		.codec_name = "moon-codec",
	},
#endif
	{ /* eax0 playback */
		.name = "playback-eax0",
		.stream_name = "eax0",
		.cpu_dai_name = "samsung-eax.0",
		.platform_name = "samsung-eax.0",
		.codec_dai_name = "moon-aif1",
		.ops = &lucky_aif1_ops,
	},
	{ /* eax1 playback */
		.name = "playback-eax1",
		.stream_name = "eax1",
		.cpu_dai_name = "samsung-eax.1",
		.platform_name = "samsung-eax.1",
		.codec_dai_name = "moon-aif1",
		.ops = &lucky_aif1_ops,
	},
	{ /* eax2 playback */
		.name = "playback-eax2",
		.stream_name = "eax2",
		.cpu_dai_name = "samsung-eax.2",
		.platform_name = "samsung-eax.2",
		.codec_dai_name = "moon-aif1",
		.ops = &lucky_aif1_ops,
	},
	{ /* eax3 playback */
		.name = "playback-eax3",
		.stream_name = "eax3",
		.cpu_dai_name = "samsung-eax.3",
		.platform_name = "samsung-eax.3",
		.codec_dai_name = "moon-aif1",
		.ops = &lucky_aif1_ops,
	},
#if defined(CONFIG_SND_SOC_MAX98505) || defined(CONFIG_SND_SOC_MAX98506)
	{ /* dsm */
		.name = "codec-dsm",
		.stream_name = "lucky-ext dsm",
		.cpu_dai_name = "moon-aif4",
		.codec_dai_name = "max98506-aif1",
		.codec_name = "max98506.0-0031",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
		.params = &dsm_params,
		.ignore_suspend = 1,
	},
#endif
#ifdef CONFIG_SND_SAMSUNG_COMPR
	{ /* compress playback */
		.name = "playback offload",
		.stream_name = "i2s0-compr",
		.cpu_dai_name = "samsung-i2s-compr",
		.platform_name = "samsung-i2s-compr",
		.codec_dai_name = "moon-aif1",
		.ops = &lucky_aif1_ops,
		.compr_ops = &lucky_compress_ops,
	},
#endif
};

static int lucky_of_get_pdata(struct snd_soc_card *card)
{
	struct device_node *pdata_np;
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	pdata_np = of_find_node_by_path("/audio_pdata");
	if (!pdata_np) {
		dev_err(card->dev,
			"Property 'samsung,audio-pdata' missing or invalid\n");
		return -EINVAL;
	}

	priv->mic_bias_gpio = of_get_named_gpio(pdata_np, "mic_bias_gpio", 0);

	if (gpio_is_valid(priv->mic_bias_gpio)) {
		ret = gpio_request(priv->mic_bias_gpio, "MICBIAS_EN_AP");
		if (ret)
			dev_err(card->dev, "Failed to request gpio: %d\n", ret);

		gpio_direction_output(priv->mic_bias_gpio, 0);
	}

	priv->clk_sel_gpio = of_get_named_gpio(pdata_np, "clk_sel_gpio", 0);
	if (gpio_is_valid(priv->clk_sel_gpio)) {
		ret = gpio_request(priv->clk_sel_gpio, "SEL_44.1K");
		if (ret) {
			dev_err(card->dev,
				"Failed to request clk_sel_gpio: %d\n", ret);
		}
	}

	lucky_mclk_data[2].gpio = priv->clk_sel_gpio;

	lucky_update_impedance_table(pdata_np);

	priv->seamless_voicewakeup =
		of_property_read_bool(pdata_np, "seamless_voicewakeup");

	ret = of_property_read_u32_array(pdata_np, "aif_format",
			priv->aif_format, ARRAY_SIZE(priv->aif_format));
	if (ret == -EINVAL) {
		priv->aif_format[0] =  SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM;
		priv->aif_format[1] =  SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS;
		priv->aif_format[2] =  SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM;
	}

	of_property_read_u32_array(pdata_np, "aif_format_tdm",
			priv->aif_format_tdm, ARRAY_SIZE(priv->aif_format_tdm));

	return 0;
}

static void get_voicetrigger_dump(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	unsigned int sv_score, final_score, noise_floor;

	regmap_read(priv->regmap_dsp, priv->sv_score_addr, &sv_score);
	regmap_read(priv->regmap_dsp, priv->final_score_addr, &final_score);
	regmap_read(priv->regmap_dsp, priv->noise_floor_addr, &noise_floor);

	dev_info(card->dev,
			"sv_score=0x%x, final_score=0x%x, noise-floor=0x%x\n",
			sv_score, final_score, noise_floor);
}

static void ez2ctrl_voicewakeup_cb(void)
{
	struct arizona_machine_priv *priv;
	char keyword_buf[100];
	char *envp[2];

	memset(keyword_buf, 0, sizeof(keyword_buf));

	dev_info(the_codec->dev, "Voice Control Callback invoked!\n");

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	priv = the_codec->component.card->drvdata;
	if (!priv)
		return;

	if (priv->voice_uevent == 0) {
		regmap_read(priv->regmap_dsp,
					 priv->word_id_addr, &keyword_type);

		snprintf(keyword_buf, sizeof(keyword_buf),
			"VOICE_WAKEUP_WORD_ID=%x", keyword_type);
	} else if (priv->voice_uevent == 1) {
		snprintf(keyword_buf, sizeof(keyword_buf),
			"VOICE_WAKEUP_WORD_ID=LPSD");
	} else {
		dev_err(the_codec->dev, "Invalid voice control mode =%d",
				priv->voice_uevent);
		return;
	}
	get_voicetrigger_dump(the_codec->component.card);

	envp[0] = keyword_buf;
	envp[1] = NULL;

	dev_info(the_codec->dev, "%s : raise the uevent, string = %s\n",
			__func__, keyword_buf);

	wake_lock_timeout(&priv->wake_lock, 5000);
	kobject_uevent_env(&(the_codec->dev->kobj), KOBJ_CHANGE, envp);

	return;
}

static ssize_t svoice_keyword_type_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret;
	ret = snprintf(buf, PAGE_SIZE, "%x\n", keyword_type);

	return ret;
}

static ssize_t svoice_keyword_type_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	int ret;

	ret = kstrtoint(buf, 0, &keyword_type);
	if (ret)
		dev_err(codec->dev,
			"Failed to convert a string to an int: %d\n", ret);

	dev_info(codec->dev, "%s(): keyword_type = %x\n",
			__func__, keyword_type);

	return count;
}

static DEVICE_ATTR(keyword_type, S_IRUGO | S_IWUSR | S_IWGRP,
	svoice_keyword_type_show, svoice_keyword_type_set);

static void ez2ctrl_debug_cb(void)
{
	struct arizona_machine_priv *priv;

	pr_info("Voice Control Callback invoked!\n");

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	priv = the_codec->component.card->drvdata;
	if (!priv)
		return;

	input_report_key(priv->input, priv->voice_key_event, 1);
	input_sync(priv->input);

	usleep_range(10000, 20000);

	input_report_key(priv->input, priv->voice_key_event, 0);
	input_sync(priv->input);

	return;
}

static int lucky_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct arizona_machine_priv *priv = card->drvdata;
	int i, ret;

	priv->codec = codec;
	the_codec = codec;
#ifdef CONFIG_SND_SAMSUNG_DEBUGFS
	dump_codec = codec;
#endif

	lucky_of_get_pdata(card);

	for (i = 0; i < LUCKY_AIF_MAX; i++)
		priv->aif[i] = card->rtd[i].codec_dai;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	/* close codec device immediately when pcm is closed */
	codec->component.ignore_pmdown_time = true;

	ret = snd_soc_add_codec_controls(codec, lucky_codec_controls,
					ARRAY_SIZE(lucky_codec_controls));
	if (ret < 0) {
		dev_err(codec->dev,
				"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	snd_soc_dapm_ignore_suspend(&card->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Sub Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Third Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "FM In");
	snd_soc_dapm_ignore_suspend(&card->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HP");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "DSP Virtual Output");
	snd_soc_dapm_sync(&codec->dapm);

	memcpy(&priv->sysclk.mclk, &lucky_mclk_data[0],
					sizeof(struct snd_mclk_info));
	memcpy(&priv->asyncclk.mclk, &lucky_mclk_data[0],
					sizeof(struct snd_mclk_info));
	memcpy(&priv->dspclk.mclk, &lucky_mclk_data[0],
					sizeof(struct snd_mclk_info));

	/* Set FLL3 */
	ret = snd_soc_codec_set_pll(codec, priv->sysclk.fll_ref_id,
				    priv->sysclk.fll_ref_src, 0, 0);
	if (ret != 0)
		dev_err(card->dev,
				"Failed to start FLL3 REF: %d\n", ret);

	ret = snd_soc_codec_set_pll(codec, priv->sysclk.fll_id,
				    priv->sysclk.mclk.id,
				    priv->sysclk.mclk.rate,
				    priv->sysclk.fll_out);
	if (ret != 0)
		dev_err(card->dev, "Failed to start FLL3: %d\n", ret);

	/* Set SYSCLK from FLL3 */
	ret = snd_soc_codec_set_sysclk(codec, priv->sysclk.clk_id,
				       priv->sysclk.src_id,
				       priv->sysclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev,
				 "Failed to set SYSCLK to FLL3: %d\n", ret);

	/* Set DAI Clock Source */
	snd_soc_dai_set_sysclk(card->rtd[0].codec_dai,
			       ARIZONA_CLK_SYSCLK, 0, 0);
	snd_soc_dai_set_sysclk(card->rtd[1].codec_dai,
			       ARIZONA_CLK_ASYNCCLK, 0, 0);
	snd_soc_dai_set_sysclk(card->rtd[2].codec_dai,
			       ARIZONA_CLK_SYSCLK_2, 0, 0);
	if (1)
		pr_err("[ERROR] Temporarily skip offload set_sysclk\n");
	else
		snd_soc_dai_set_sysclk(card->rtd[10].cpu_dai,
				       ARIZONA_CLK_ASYNCCLK_2, 0, 0);

	wake_lock_init(&priv->wake_lock, WAKE_LOCK_SUSPEND,
				"lucky-voicewakeup");

	if (priv->seamless_voicewakeup) {
		arizona_set_ez2ctrl_cb(codec, ez2ctrl_voicewakeup_cb);

		svoice_class = class_create(THIS_MODULE, "svoice");
		ret = IS_ERR(svoice_class);
		if (ret) {
			pr_err("Failed to create class(svoice)!\n");
			goto err_class_create;
		}

		keyword_dev = device_create(svoice_class,
					NULL, 0, NULL, "keyword");
		ret = IS_ERR(keyword_dev);
		if (ret) {
			pr_err("Failed to create device(keyword)!\n");
			goto err_device_create;
		}

		ret = device_create_file(keyword_dev, &dev_attr_keyword_type);
		if (ret < 0) {
			pr_err("Failed to create device file in sysfs entries(%s)!\n",
				dev_attr_keyword_type.attr.name);
			goto err_device_create_file;
		}
	} else {
		priv->input = devm_input_allocate_device(card->dev);
		if (!priv->input) {
			dev_err(card->dev, "Can't allocate input dev\n");
			ret = -ENOMEM;
			goto err_register;
		}

		priv->input->name = "voicecontrol";
		priv->input->dev.parent = card->dev;

		input_set_capability(priv->input, EV_KEY,
			KEY_VOICE_WAKEUP);
		input_set_capability(priv->input, EV_KEY,
			KEY_LPSD_WAKEUP);

		ret = input_register_device(priv->input);
		if (ret) {
			dev_err(card->dev,
				"Can't register input device: %d\n", ret);
			goto err_register;
		}

		arizona_set_ez2ctrl_cb(codec, ez2ctrl_debug_cb);
		priv->voice_key_event = KEY_VOICE_WAKEUP;
	}

	priv->regmap_dsp = arizona_get_regmap_dsp(codec);

	arizona_set_hpdet_cb(codec, lucky_arizona_hpdet_cb);
	arizona_set_micd_cb(codec, lucky_arizona_micd_cb);

#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
	if (priv->regmap_dsp)
		maxdsm_cal_set_regmap(priv->regmap_dsp);
#endif

#ifdef CONFIG_SND_ESA_SA_EFFECT
	ret = esa_effect_register(card);
	if (ret < 0) {
		dev_err(codec->dev,
			"Failed to add esa controls to codec: %d\n", ret);
	}
#endif

	return 0;

err_device_create_file:
	device_destroy(svoice_class, 0);
err_device_create:
	class_destroy(svoice_class);
err_class_create:
err_register:
	wake_lock_destroy(&priv->wake_lock);
	return ret;
}

static int lucky_start_sysclk(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	struct snd_soc_codec *codec = priv->codec;
	int ret;

	dev_dbg(card->dev, "%s\n", __func__);

	/* Set SYSCLK FLL */
	ret = snd_soc_codec_set_pll(priv->codec, priv->sysclk.fll_ref_id,
				    priv->sysclk.fll_ref_src, 0, 0);
	if (ret != 0)
		dev_err(card->dev, "Failed to start SYSCLK FLL REF: %d\n", ret);

	ret = snd_soc_codec_set_pll(priv->codec, priv->sysclk.fll_id,
				    priv->sysclk.mclk.id,
				    priv->sysclk.mclk.rate,
				    priv->sysclk.fll_out);
	if (ret != 0)
		dev_err(card->dev, "Failed to start FLL for SYSCLK: %d\n", ret);

	if (priv->dspclk.rate > 0) {
		/* Set DSPCLK FLL */
		if (priv->dspclk.fll_ref_id > 0) {
			ret = snd_soc_codec_set_pll(codec,
				priv->dspclk.fll_ref_id,
				priv->dspclk.fll_ref_src, 0, 0);
			if (ret != 0)
				dev_err(card->dev,
					"Failed to start DSPCLK FLL REF: %d\n",
					ret);
		}

		ret = snd_soc_codec_set_pll(codec, priv->dspclk.fll_id,
					    priv->dspclk.mclk.id,
					    priv->dspclk.mclk.rate,
					    priv->dspclk.fll_out);
		if (ret != 0) {
			dev_err(card->dev,
				"Failed to start DSPCLK FLL: %d\n", ret);
			return ret;
		}

		/* Set DSPCLK from FLL */
		ret = snd_soc_codec_set_sysclk(codec, priv->dspclk.clk_id,
					       priv->dspclk.src_id,
					       priv->dspclk.rate,
					       SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev,
				 "Failed to set DSPCLK to FLL: %d\n", ret);
	}

	return ret;
}

static int lucky_stop_sysclk(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	dev_dbg(card->dev, "%s\n", __func__);

	/* Clear SYSCLK */
	ret = snd_soc_codec_set_sysclk(priv->codec, priv->sysclk.clk_id,
				       0, 0, 0);

	/* Clear ASYNCCLK */
	ret = snd_soc_codec_set_sysclk(priv->codec, priv->asyncclk.clk_id,
				       0, 0, 0);

	/* Power down FLLAO */
	ret = snd_soc_codec_set_pll(priv->codec, LUCKY_FLLAO, 0, 0, 0);
	if (ret != 0)
		dev_err(card->dev, "Failed to stop FLLAO: %d\n", ret);

	/* Power down FLL2 */
	ret = snd_soc_codec_set_pll(priv->codec, LUCKY_FLL2, 0, 0, 0);
	if (ret != 0)
		dev_err(card->dev, "Failed to stop FLL2: %d\n", ret);

	/* Power down FLL1 */
	ret = snd_soc_codec_set_pll(priv->codec, LUCKY_FLL1, 0, 0, 0);
	if (ret != 0)
		dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);

	return ret;
}

static int lucky_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct arizona_machine_priv *priv = card->drvdata;

	if (!priv->codec || dapm != &priv->codec->dapm)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (card->dapm.bias_level == SND_SOC_BIAS_OFF)
			lucky_start_sysclk(card);
		break;
	case SND_SOC_BIAS_OFF:
		lucky_stop_sysclk(card);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	default:
	break;
	}

	card->dapm.bias_level = level;
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int lucky_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

int lucky_change_sysclk(struct snd_soc_card *card, int source)
{
	struct arizona_machine_priv *priv = card->drvdata;
	struct snd_soc_codec *codec = priv->codec;
	int ret;

	dev_info(card->dev, "%s: source = %d\n", __func__, source);

	if (source) {
		/* Set SYSCLK from FLL3 */
		ret = snd_soc_codec_set_sysclk(codec, priv->sysclk.clk_id,
						   priv->sysclk.fll_id,
						   priv->sysclk.rate,
						   SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev,
				"Failed to set SYSCLK to FLL3: %d\n", ret);
	} else {
		/* Set SYSCLK from FLL1 */
		ret = snd_soc_codec_set_sysclk(codec, priv->sysclk.clk_id,
					       priv->sysclk.fll_id,
					       priv->sysclk.rate,
					       SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev,
				"Failed to set SYSCLK to FLL3: %d\n", ret);

		/* Clear ASYNCCLK */
		ret = snd_soc_codec_set_sysclk(priv->codec, priv->dspclk.clk_id,
					       0, 0, 0);

		/* Power down FLLAO */
		ret = snd_soc_codec_set_pll(priv->codec, LUCKY_FLLAO, 0, 0, 0);
		if (ret != 0)
			dev_err(card->dev, "Failed to stop FLLAO: %d\n", ret);

		/* Power down FLL2 */
		ret = snd_soc_codec_set_pll(priv->codec, LUCKY_FLL2, 0, 0, 0);
		if (ret != 0)
			dev_err(card->dev, "Failed to stop FLL2: %d\n", ret);
	}

	return ret;
}

static int lucky_check_clock_conditions(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct arizona_machine_priv *priv = card->drvdata;
	int mainmic_state = 0;

#if defined(CONFIG_MFD_FLORIDA) || defined(CONFIG_MFD_CLEARWATER) \
	|| defined(CONFIG_MFD_MOON)
	/* Check status of the Main Mic for ez2control
	 * Because when the phone goes to suspend mode,
	 * Enabling case of Main mic is only ez2control mode */
	mainmic_state = snd_soc_dapm_get_pin_status(&card->dapm, "Main Mic");
#endif

	if (!codec->component.active && mainmic_state) {
		dev_info(card->dev, "MAIN_MIC is running without input stream\n");
		return LUCKY_RUN_MAINMIC;
	}

	if (!codec->component.active && priv->ear_mic && !mainmic_state) {
		dev_info(card->dev, "EAR_MIC is running without input stream\n");
		return LUCKY_RUN_EARMIC;
	}

	return 0;
}

static int lucky_suspend_post(struct snd_soc_card *card)
{
//	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	/* When the card goes to suspend state, If codec is not active,
	 * the micbias of headset is enable and the ez2control is not running,
	 * The MCLK and the FLL3 should be disable to reduce the sleep current.
	 * In the other cases, these should keep previous status */
	ret = lucky_check_clock_conditions(card);
/*
	if (ret == LUCKY_RUN_EARMIC) {
		arizona_enable_force_bypass(priv->codec);
		lucky_stop_sysclk(card);
		dev_info(card->dev, "%s: stop_sysclk\n", __func__);
	} else if (ret == LUCKY_RUN_MAINMIC) {
		arizona_enable_force_bypass(priv->codec);
		lucky_change_sysclk(card, 0);
		dev_info(card->dev, "%s: change_sysclk\n", __func__);
	}

	if (!priv->codec->component.active) {
		dev_info(card->dev, "%s : set AIF1 port slave\n", __func__);
		snd_soc_update_bits(priv->codec, ARIZONA_AIF1_BCLK_CTRL,
				ARIZONA_AIF1_BCLK_MSTR_MASK, 0);
		snd_soc_update_bits(priv->codec, ARIZONA_AIF1_TX_PIN_CTRL,
				ARIZONA_AIF1TX_LRCLK_MSTR_MASK, 0);
		snd_soc_update_bits(priv->codec, ARIZONA_AIF1_RX_PIN_CTRL,
				ARIZONA_AIF1RX_LRCLK_MSTR_MASK, 0);
	}
*/
	return 0;
}

static int lucky_resume_pre(struct snd_soc_card *card)
{
//	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	/* When the card goes to resume state, If codec is not active,
	 * the micbias of headset is enable and the ez2control is not running,
	 * The MCLK and the FLL3 should be enable.
	 * In the other cases, these should keep previous status */
	ret = lucky_check_clock_conditions(card);
/*
	if (ret == LUCKY_RUN_EARMIC) {
		lucky_start_sysclk(card);
		arizona_disable_force_bypass(priv->codec);
		dev_info(card->dev, "%s: start_sysclk\n", __func__);
	} else if (ret == LUCKY_RUN_MAINMIC) {
		arizona_disable_force_bypass(priv->codec);
	}
*/
	return 0;
}

static struct snd_soc_card lucky_cards[] = {
	{
		.name = "Lucky CS47L91 Sound",
		.owner = THIS_MODULE,

		.dai_link = lucky_cs47l91_dai,
		.num_links = ARRAY_SIZE(lucky_cs47l91_dai),

		.controls = lucky_controls,
		.num_controls = ARRAY_SIZE(lucky_controls),
		.dapm_widgets = lucky_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(lucky_dapm_widgets),
		.dapm_routes = lucky_cs47l91_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(lucky_cs47l91_dapm_routes),

		.late_probe = lucky_late_probe,

		.suspend_post = lucky_suspend_post,
		.resume_pre = lucky_resume_pre,

		.set_bias_level = lucky_set_bias_level,
		.set_bias_level_post = lucky_set_bias_level_post,

		.drvdata = &lucky_cs47l91_priv,
	},
};

static const char * const card_ids[] = {
	"cs47l91"
};

static int lucky_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *codec_np, *cpu_np;
	struct snd_soc_card *card;
	struct snd_soc_dai_link *dai_link;
	struct arizona_machine_priv *priv;
	int of_route, num_cards;

	num_cards = (int) ARRAY_SIZE(card_ids);
	for (n = 0; n < num_cards; n++) {
		if (NULL != of_find_node_by_name(NULL, card_ids[n])) {
			card = &lucky_cards[n];
			dai_link = card->dai_link;
			break;
		}
	}

	if (n == num_cards) {
		dev_err(&pdev->dev, "couldn't find card\n");
		return -EINVAL;
	}

	card->dev = &pdev->dev;
	priv = card->drvdata;

	priv->mclk = devm_clk_get(card->dev, "mclk");
	if (IS_ERR(priv->mclk)) {
		dev_dbg(card->dev, "Device tree node not found for mclk");
		priv->mclk = NULL;
	} else
		clk_prepare(priv->mclk);

	lucky_mclk_data[1].clk = priv->mclk;

	ret = snd_soc_register_component(card->dev, &lucky_cmpnt,
				lucky_ext_dai, ARRAY_SIZE(lucky_ext_dai));
	if (ret != 0)
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);

	of_route = of_property_read_bool(card->dev->of_node,
						"samsung,audio-routing");
	if (!card->dapm_routes || !card->num_dapm_routes || of_route) {
		ret = snd_soc_of_parse_audio_routing(card,
				"samsung,audio-routing");
		if (ret != 0) {
			dev_err(&pdev->dev,
				"Failed to parse audio routing: %d\n",
				ret);
			ret = -EINVAL;
			goto out;
		}
	}

	if (np) {
		for (n = 0; n < card->num_links; n++) {
			/* Skip parsing DT for fully formed dai links */
			if (dai_link[n].platform_name &&
			    dai_link[n].codec_name) {
				dev_dbg(card->dev,
					"Skipping dt for populated dai link %s\n",
					dai_link[n].name);
				continue;
			}

			if (dai_link[n].platform_name == NULL &&
			    dai_link[n].codec_name) {
				dev_dbg(card->dev,
					"Skipping dt for populated dai link(codec to codec) %s\n",
					dai_link[n].name);
				continue;
			}

			cpu_np = of_parse_phandle(np,
					"samsung,audio-cpu", n);
			if (!cpu_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-cpu'"
					" missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			codec_np = of_parse_phandle(np,
					"samsung,audio-codec", n);
			if (!codec_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-codec'"
					" missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			if (!dai_link[n].cpu_dai_name)
				dai_link[n].cpu_of_node = cpu_np;

			if (!dai_link[n].platform_name)
				dai_link[n].platform_of_node = cpu_np;

			dai_link[n].codec_of_node = codec_np;
		}
	} else
		dev_err(&pdev->dev, "Failed to get device node\n");

#if defined(CONFIG_SND_SOC_MAX98505)
	if (of_find_node_by_name(NULL, "max98505") != NULL) {
		for (n = 0; n < card->num_links; n++) {
			if (!strcmp(dai_link[n].name, "codec-dsm")) {
				dai_link[n].codec_dai_name = "max98505-aif1";
				dai_link[n].codec_name = "max98505.0-0031";
				break;
			}
		}
	}
#endif

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);
		goto out;
	}

	return ret;

out:
	snd_soc_unregister_component(card->dev);
	return ret;
}

static int lucky_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct arizona_machine_priv *priv = card->drvdata;

	device_remove_file(keyword_dev, &dev_attr_keyword_type);
	device_destroy(svoice_class, 0);
	class_destroy(svoice_class);

	snd_soc_unregister_component(card->dev);
	snd_soc_unregister_card(card);
	wake_lock_destroy(&priv->wake_lock);

	if (priv->mclk) {
		clk_unprepare(priv->mclk);
		devm_clk_put(card->dev, priv->mclk);
	}

	return 0;
}

static const struct of_device_id lucky_arizona_of_match[] = {
	{ .compatible = "samsung,lucky-arizona", },
	{ },
};
MODULE_DEVICE_TABLE(of, lucky_arizona_of_match);

static struct platform_driver lucky_audio_driver = {
	.driver	= {
		.name	= "lucky-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(lucky_arizona_of_match),
	},
	.probe	= lucky_audio_probe,
	.remove	= lucky_audio_remove,
};

module_platform_driver(lucky_audio_driver);

MODULE_DESCRIPTION("ALSA SoC LUCKY ARIZONA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lucky-audio");
