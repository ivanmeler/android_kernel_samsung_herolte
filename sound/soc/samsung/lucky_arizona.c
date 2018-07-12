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
#include <sound/soc-dapm.h>
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
#if defined(CONFIG_SND_SOC_CIRRUS_AMP_CAL)
#include "../codecs/cirrus-amp-cal.h"
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
#define LUCKY_EZ2CTRL_NUM	5

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

enum {
	AMP_TYPE_MAX98505,
	AMP_TYPE_MAX98506,
	AMP_TYPE_MAX98512,
	AMP_TYPE_CS35L33,
	AMP_TYPE_MAX,
};

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
	struct snd_sysclk_info opclk;

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
	u32 amp_type;

#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
	bool dual_spk;
	int dsm_cal_rdc;
	int dsm_cal_temp;
	int dsm_cal_rdc_r;
#endif
};

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
	.opclk = {
		.clk_id = ARIZONA_CLK_OPCLK,
		.src_id = ARIZONA_CLK_SRC_MCLK1,
		.rate = 12288000,
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

struct gain_table {
	int min;           /* Minimum impedance */
	int max;           /* Maximum impedance */
	unsigned int gain; /* Register value to set for this measurement */
};

static struct impedance_table {
	struct gain_table hp_gain_table[7];
	char imp_region[4];	/* impedance region */
} imp_table = {
	.hp_gain_table = {
		{    0,      13, 0 },
		{   14,      42, 8 },
		{   43,     100, 14 },
		{  101,     200, 20 },
		{  201,     450, 22 },
		{  451,    1000, 22 },
		{ 1001, INT_MAX, 18 },
	},
};

enum mclk_src {
	CLK_26M = 1 << 0,
	CLK_22M = 1 << 1,
	CLK_32K = 1 << 2,
};


static struct snd_soc_card *lucky_card;


static int clk_cnt[2];

static void enable_mclk(struct snd_soc_card *card, struct snd_mclk_info *mclk)
{
	void __iomem *reg_pmu;
	u32 pmu_debug;

	if (mclk->clk) {
		clk_enable(mclk->clk);
		clk_cnt[0]++;

		reg_pmu = ioremap(0x105C0000, SZ_4K);
		pmu_debug = readl(reg_pmu + 0xA00);
		iounmap(reg_pmu);
		dev_info(card->dev, "PMU_DEBUG [0x%08X]\n", pmu_debug);
	}

	if (mclk->gpio) {
		if (gpio_is_valid(mclk->gpio)) {
			gpio_set_value(mclk->gpio, 1);
			clk_cnt[1]++;
		}
	}

	dev_info(card->dev, "26MHz[%d], 22.576MHz[%d]\n",
						clk_cnt[0], clk_cnt[1]);
}

static void disable_mclk(struct snd_soc_card *card, struct snd_mclk_info *mclk)
{
	void __iomem *reg_pmu;
	u32 pmu_debug;

	if (mclk->clk) {
		clk_disable(mclk->clk);
		clk_cnt[0]--;

		reg_pmu = ioremap(0x105C0000, SZ_4K);
		pmu_debug = readl(reg_pmu + 0xA00);
		iounmap(reg_pmu);
		dev_info(card->dev, "PMU_DEBUG [0x%08X]\n", pmu_debug);
	}

	if (mclk->gpio) {
		if (gpio_is_valid(mclk->gpio)) {
			gpio_set_value(mclk->gpio, 0);
			clk_cnt[1]--;
		}
	}

	dev_info(card->dev, "26MHz[%d], 22.576MHz[%d]\n",
						clk_cnt[0], clk_cnt[1]);
}

static void lucky_change_mclk(struct snd_soc_card *card, int rate)
{
	struct arizona_machine_priv *priv = card->drvdata;
	struct snd_mclk_info *mclk;
	int sysclk_rate, sysclk_src_id, sysclk_fll_id, sysclk_fll_out;

	if (rate == 0) {
		mclk = &lucky_mclk_data[0];
		sysclk_rate = 98304000;
		sysclk_src_id = ARIZONA_CLK_SRC_FLLAO_HI;
		sysclk_fll_id = LUCKY_FLLAO;
		sysclk_fll_out = 49152000;
	} else if (rate % 8000) {
		if (lucky_mclk_data[2].gpio > 0)
			mclk = &lucky_mclk_data[2];
		else
			mclk = &lucky_mclk_data[1];
		sysclk_rate = 90316800;
		sysclk_src_id = ARIZONA_CLK_SRC_FLL1;
		sysclk_fll_id = LUCKY_FLL1;
		sysclk_fll_out = 90316800;
	} else {
		mclk = &lucky_mclk_data[1];
		sysclk_rate = 98304000;
		sysclk_src_id = ARIZONA_CLK_SRC_FLL1;
		sysclk_fll_id = LUCKY_FLL1;
		sysclk_fll_out = 98304000;
	}

	if (priv->sysclk.mclk.id != mclk->id ||
	    priv->sysclk.mclk.rate != mclk->rate ||
	    priv->sysclk.rate != sysclk_rate) {
		disable_mclk(card, &priv->sysclk.mclk);
		enable_mclk(card, mclk);

		memcpy(&priv->sysclk.mclk, mclk, sizeof(struct snd_mclk_info));
		priv->sysclk.rate = sysclk_rate;
		priv->sysclk.fll_out = sysclk_fll_out;
		priv->sysclk.src_id = sysclk_src_id;
		priv->sysclk.fll_id = sysclk_fll_id;

		if (priv->asyncclk.fll_ref_src == ARIZONA_FLL_SRC_NONE)
			memcpy(&priv->asyncclk.mclk, mclk,
					sizeof(struct snd_mclk_info));
	} else {
		dev_dbg(card->dev,
				"%s: skipping to change mclk\n", __func__);
	}

	dev_info(card->dev, "%s: %d [%d], sysclk_fll(id: %d, out: %d)\n",
					__func__, rate, mclk->rate,
					priv->sysclk.src_id,
					priv->sysclk.fll_out);
}

void lucky_arizona_hpdet_cb(unsigned int meas)
{
	struct snd_soc_card *card = lucky_card;
	struct arizona_machine_priv *priv = card->drvdata;
	int jack_det;
	int i, num_hp_gain_table;

	if (meas == ARIZONA_HP_Z_OPEN) {
		jack_det = 0;
	} else {
		jack_det = 1;
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		esa_compr_hpdet_notifier(true);
#endif
	}

	dev_info(card->dev, "%s(%d) meas(%d)\n", __func__, jack_det, meas);

#if (defined CONFIG_SWITCH_ANTENNA_EARJACK \
	 || defined CONFIG_SWITCH_ANTENNA_EARJACK_IF) \
	 && (!defined CONFIG_SEC_FACTORY)
	/* Notify jack condition to other devices */
	antenna_switch_work_earjack(jack_det);
#endif

	num_hp_gain_table = (int) ARRAY_SIZE(imp_table.hp_gain_table);
	for (i = 0; i < num_hp_gain_table; i++) {
		if (meas < imp_table.hp_gain_table[i].min
				|| meas > imp_table.hp_gain_table[i].max)
			continue;

		dev_info(card->dev, "SET GAIN %d step for %d ohms\n",
			 imp_table.hp_gain_table[i].gain, meas);
		priv->hp_impedance_step = imp_table.hp_gain_table[i].gain;
	}
}

void lucky_arizona_micd_cb(bool mic)
{
	struct snd_soc_card *card = lucky_card;
	struct arizona_machine_priv *priv = card->drvdata;

	priv->ear_mic = mic;
	dev_info(card->dev, "%s: ear_mic = %d\n", __func__, priv->ear_mic);
}

void lucky_update_impedance_table(struct device_node *np)
{
	struct snd_soc_card *card = lucky_card;
	int len = ARRAY_SIZE(imp_table.hp_gain_table);
	u32 data[len * 3];
	int i, ret;
	char imp_str[14] = "imp_table";

	if (strlen(imp_table.imp_region) == 3) {
		strcat(imp_str, "_");
		strcat(imp_str, imp_table.imp_region);
	}

	if (of_find_property(np, imp_str, NULL))
		ret = of_property_read_u32_array(np, imp_str, data, (len * 3));
	else
		ret = of_property_read_u32_array(np, "imp_table", data,
								(len * 3));

	if (!ret) {
		dev_info(card->dev, "%s: data from DT\n", __func__);

		for (i = 0; i < len; i++) {
			imp_table.hp_gain_table[i].min = data[i * 3];
			imp_table.hp_gain_table[i].max = data[(i * 3) + 1];
			imp_table.hp_gain_table[i].gain = data[(i * 3) + 2];
		}
	}
}

static int __init get_impedance_region(char *str)
{
	/* Read model region */
	strncat(imp_table.imp_region, str,
				sizeof(imp_table.imp_region) - 1);

	return 0;
}
early_param("region1", get_impedance_region);

static int arizona_put_impedance_volsw(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	struct arizona_machine_priv *priv = card->drvdata;
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
	dev_info(card->dev,
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

static int get_voicecontrol_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona_machine_priv *priv = codec->component.card->drvdata;

	if (priv->seamless_voicewakeup) {
		ucontrol->value.integer.value[0] = priv->voice_uevent;
	} else {
		if (priv->voice_key_event == KEY_VOICE_WAKEUP)
			ucontrol->value.integer.value[0] = 0;
		else
			ucontrol->value.integer.value[0] = 1;
	}

	return 0;
}

static int set_voicecontrol_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	struct arizona_machine_priv *priv = card->drvdata;
	int voicecontrol_mode;

	voicecontrol_mode = ucontrol->value.integer.value[0];

	if (priv->seamless_voicewakeup) {
		priv->voice_uevent = voicecontrol_mode;
		dev_info(card->dev, "set voice control mode: %s\n",
			voicecontrol_mode_text[priv->voice_uevent]);
	} else {
		if (voicecontrol_mode == 0) {
			priv->voice_key_event = KEY_VOICE_WAKEUP;
		} else if (voicecontrol_mode == 1) {
			priv->voice_key_event = KEY_LPSD_WAKEUP;
		} else {
			dev_err(card->dev,
			"Invalid voice control mode =%d", voicecontrol_mode);
			return 0;
		}

		dev_info(card->dev, "set voice control mode: %s key_event = %d\n",
				voicecontrol_mode_text[voicecontrol_mode],
				priv->voice_key_event);
	}

	return  0;
}

static int get_voice_tracking_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	struct arizona_machine_priv *priv = card->drvdata;
	unsigned int energy_l, energy_r;

	regmap_read(priv->regmap_dsp, priv->energy_l_addr, &energy_l);
	energy_l &= 0xFFFF0000;
	regmap_read(priv->regmap_dsp, priv->energy_r_addr, &energy_r);
	energy_r = (energy_r >> 16);

	ucontrol->value.enumerated.item[0] = (energy_l | energy_r);

	dev_dbg(card->dev, "get voice tracking info :0x%08x\n",
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int set_voice_tracking_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;

	dev_dbg(card->dev, "set voice tracking info\n");
	return  0;
}

static int get_voice_trigger_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	struct arizona_machine_priv *priv = card->drvdata;

	ucontrol->value.integer.value[0] = priv->voicetriginfo;
	return 0;
}

static int set_voice_trigger_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	struct arizona_machine_priv *priv = card->drvdata;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *) kcontrol->private_value;
	unsigned int reg = mc->reg;
	int offset_value;
	unsigned int val, val1, val2;

	priv->voicetriginfo = ucontrol->value.integer.value[0];

	offset_value = priv->voicetriginfo - 250;

	if (offset_value == 0)
		val = 0;
	else if (offset_value > 0)
		val = (offset_value / 15) + 1;
	else
		val = 0xFFFFFF + (offset_value / 15);

	val1 = val & 0xFFFF;
	val2 = (val >> 16) & 0xFFFF;

	snd_soc_write(codec, reg + 1, val1);
	snd_soc_write(codec, reg, val2);

	dev_info(card->dev, "set voice trigger info: %d, val=0x%x\n",
			priv->voicetriginfo, val);
	return  0;
}

#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
#define DSM_RDC_ROOM_TEMP	0x2A005C
#define DSM_AMBIENT_TEMP	0x2A0182
#define DSM_4_0_LSI_STEREO_OFFSET	410
static int lucky_dsm_cal_apply(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	static bool is_get_temp;
	static bool is_get_rdc;	
	static bool is_get_rdc_r;
	static int get_temp_try_cnt = 3;
	static int get_rdc_try_cnt = 3;
	static int get_rdc_r_try_cnt = 3;
	int ret;

	if (get_temp_try_cnt > 0) {
		ret = maxdsm_cal_get_temp(&priv->dsm_cal_temp);
		if (ret == 0 && priv->dsm_cal_temp > 0) {
			is_get_temp = true;
			get_temp_try_cnt = 0;
		} else {
			if (ret == -ENOENT) {
				dev_dbg(card->dev,
					"No such file or directory\n");
				get_temp_try_cnt = 0;
				return -1;
			} else if (ret == -EBUSY) {
				dev_dbg(card->dev,
					"Device or resource busy\n");
			}
			get_temp_try_cnt--;
			return -1;
		}
	}

	if (is_get_temp && get_rdc_try_cnt > 0) {
		ret = maxdsm_cal_get_rdc(&priv->dsm_cal_rdc);
		if (ret == 0 && priv->dsm_cal_rdc > 0) {
			is_get_rdc = true;
			get_rdc_try_cnt = 0;
		} else {
			if (ret == -ENOENT) {
				dev_dbg(card->dev,
					"No such file or directory\n");
				get_rdc_try_cnt = 0;
				return -1;
			} else if (ret == -EBUSY) {
				dev_dbg(card->dev,
					"Device or resource busy\n");
			}
			get_rdc_try_cnt--;
			return -1;
		}
	}

	if (priv->dual_spk) {
		if (is_get_temp && is_get_rdc && get_rdc_r_try_cnt > 0) {
			ret = maxdsm_cal_get_rdc_r(&priv->dsm_cal_rdc_r);
			if (ret == 0 && priv->dsm_cal_rdc_r > 0) {
				is_get_rdc_r = true;
				get_rdc_r_try_cnt = 0;
			} else {
				if (ret == -ENOENT) {
					dev_dbg(card->dev,
						"No such file or directory\n");
					get_rdc_r_try_cnt = 0;
					return -1;
				} else if (ret == -EBUSY) {
					dev_dbg(card->dev,
						"Device or resource busy\n");
				}
				get_rdc_r_try_cnt--;
				return -1;
			}
		}
	}

	if (is_get_temp && is_get_rdc &&
		(((priv->dual_spk) == 0) || ((priv->dual_spk) && is_get_rdc_r))) {
		regmap_write(priv->regmap_dsp, DSM_AMBIENT_TEMP,
					(unsigned int)priv->dsm_cal_temp);
		regmap_write(priv->regmap_dsp, DSM_RDC_ROOM_TEMP,
					(unsigned int)priv->dsm_cal_rdc);
		if(priv->dual_spk) {
			regmap_write(priv->regmap_dsp, (DSM_AMBIENT_TEMP+DSM_4_0_LSI_STEREO_OFFSET),
						(unsigned int)priv->dsm_cal_temp);
			regmap_write(priv->regmap_dsp, (DSM_RDC_ROOM_TEMP+DSM_4_0_LSI_STEREO_OFFSET),
						(unsigned int)priv->dsm_cal_rdc_r);
			dev_info(card->dev, "set rdc_r = %d\n",
						priv->dsm_cal_rdc_r);
		}
		dev_info(card->dev, "set rdc = %d, temperature = %d\n",
					priv->dsm_cal_rdc, priv->dsm_cal_temp);
	} else
		dev_info(card->dev, "dsm works with default calibration\n");

	return 0;
}
#endif

static int lucky_external_amp(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct arizona_machine_priv *priv = card->drvdata;

	dev_dbg(card->dev, "%s: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (priv->external_amp)
			priv->external_amp(1);
		if (priv->amp_type == AMP_TYPE_MAX98505 ||
			priv->amp_type == AMP_TYPE_MAX98506 ||
			priv->amp_type == AMP_TYPE_MAX98512) {
#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
			lucky_dsm_cal_apply(card);
#endif
		}
		if (priv->amp_type == AMP_TYPE_CS35L33) {
#if defined(CONFIG_SND_SOC_CIRRUS_AMP_CAL)
			cirrus_amp_calib_apply();
#endif
		}
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

	SOC_ENUM_EXT("VoiceControl Mode", voicecontrol_mode_enum[0],
		get_voicecontrol_mode, set_voicecontrol_mode),

	SOC_SINGLE_EXT("VoiceTrackInfo",
		SND_SOC_NOPM,
		0, 0xffffffff, 0,
		get_voice_tracking_info, set_voice_tracking_info),

	SOC_SINGLE_EXT("VoiceTrigInfo",
		0x380006, 0, 0xffff, 0,
		get_voice_trigger_info, set_voice_trigger_info),
};

#if !defined (CONFIG_SND_SAMSUNG_PDM_EXT_AMP)
static int lucky_spk_set_asyncclk(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	if (priv->codec->component.active)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		dev_dbg(card->dev, "%s\n", __func__);

		/* Set ASYNCCLK FLL  */
		ret = snd_soc_codec_set_pll(priv->codec,
						priv->asyncclk.fll_ref_id,
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
			dev_err(card->dev,
				"Failed to start ASYNCCLK FLL: %d\n", ret);

		/* Set ASYNCCLK from FLL */
		ret = snd_soc_codec_set_sysclk(priv->codec,
						priv->asyncclk.clk_id,
						priv->asyncclk.src_id,
						priv->asyncclk.rate,
						SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev,
				"Unable to set ASYNCCLK to FLL: %d\n", ret);
		break;
	default:
		break;
	}

	return 0;
}
#endif
static struct snd_soc_dapm_widget *lucky_dapm_find_widget(
			struct snd_soc_dapm_context *dapm, const char *pin,
			bool search_other_contexts)
{
	struct snd_soc_dapm_widget *w;
	struct snd_soc_dapm_widget *fallback = NULL;

	list_for_each_entry(w, &dapm->card->widgets, list) {
		if (!strcmp(w->name, pin)) {
			if (w->dapm == dapm)
				return w;

			fallback = w;
		}
	}

	if (search_other_contexts)
		return fallback;

	return NULL;
}

static int snd_soc_dapm_set_subseq(struct snd_soc_dapm_context *dapm,
				const char *pin, int subseq)
{
	struct snd_soc_dapm_widget *w =
				lucky_dapm_find_widget(dapm, pin, false);

	if (!w) {
		dev_err(dapm->dev, "%s: unknown pin %s\n", __func__, pin);
		return -EINVAL;
	}

	w->subseq = subseq;

	return 0;
}

static const struct snd_kcontrol_new lucky_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Third Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("VI SENSING"),
	SOC_DAPM_PIN_SWITCH("VI SENSING PDM"),
	SOC_DAPM_PIN_SWITCH("FM In"),
};

const struct snd_soc_dapm_widget lucky_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", lucky_external_amp),
#if defined (CONFIG_SND_SAMSUNG_PDM_EXT_AMP)
	SND_SOC_DAPM_SUPPLY("ASYNC_AMP", SND_SOC_NOPM,
			0, 0, NULL,
			SND_SOC_DAPM_PRE_PMU),
#else
	SND_SOC_DAPM_SUPPLY("ASYNC_AMP", SND_SOC_NOPM,
			0, 0, lucky_spk_set_asyncclk,
			SND_SOC_DAPM_PRE_PMU),
#endif
	SND_SOC_DAPM_SPK("RCV", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", lucky_ext_mainmicbias),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Third Mic", NULL),
	SND_SOC_DAPM_MIC("VI SENSING", NULL),
	SND_SOC_DAPM_MIC("VI SENSING PDM", NULL),
	SND_SOC_DAPM_MIC("FM In", NULL),
};

const struct snd_soc_dapm_route lucky_cs47l91_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "HiFi Playback" },

	{ "RCV", NULL, "HPOUT3L" },
	{ "RCV", NULL, "HPOUT3R" },

	{ "Main Mic", NULL, "MICBIAS2A" },
	{ "IN1AR", NULL, "Main Mic" },

	{ "Headset Mic", NULL, "MICBIAS1A" },
	{ "IN2BL", NULL, "Headset Mic" },

	{ "Sub Mic", NULL, "MICBIAS2B" },
	{ "IN3L", NULL, "Sub Mic" },

	/* "HiFi Capture" is capture stream of maxim_amp dai */
	{ "HiFi Capture", NULL, "VI SENSING" },
	{ "VI SENSING", NULL, "ASYNC_AMP" },

	{ "IN1BL", NULL, "FM In" },
	{ "IN1BR", NULL, "FM In" },
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

		/* Clear ASYNCCLK */
		ret = snd_soc_codec_set_sysclk(priv->codec,
				       priv->asyncclk.clk_id, 0, 0, 0);

		/* Power down FLL2 */
		ret = snd_soc_codec_set_pll(priv->codec, priv->asyncclk.fll_id,
				       0, 0, 0);
		if (ret != 0)
			dev_err(card->dev, "Failed to stop FLL2: %d\n", ret);

		/* Power down FLL1 */
		ret = snd_soc_codec_set_pll(priv->codec, LUCKY_FLL1,
				       0, 0, 0);
		if (ret != 0)
			dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);
	}
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

	dev_dbg(card->dev, "%s-%d hw_free",
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
	case 32000:
		bclk = 1024000;
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

	/* Set FLL2: If AIF2 is slave mode, FLL_REF clock should be same with
	 * SYSCLK and FLL should be set according to AIF2BCLK */
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

	/* Set Sample rate 1 as 48k */
	snd_soc_update_bits(priv->codec, ARIZONA_SAMPLE_RATE_1,
			    ARIZONA_SAMPLE_RATE_1_MASK, 0x3);

	return 0;
}

static int lucky_aif2_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct arizona_machine_priv *priv = rtd->card->drvdata;

	dev_dbg(card->dev, "%s-%d hw_free",
			rtd->dai_link->name, substream->stream);

	priv->asyncclk.fll_ref_src = ARIZONA_FLL_SRC_NONE;
	priv->asyncclk.fll_ref_in = 0;
	priv->asyncclk.fll_ref_out = 0;
	memcpy(&priv->asyncclk.mclk, &lucky_mclk_data[0],
					sizeof(struct snd_mclk_info));

	return 0;
}

static struct snd_soc_ops lucky_aif2_ops = {
	.shutdown = lucky_aif_shutdown,
	.hw_params = lucky_aif2_hw_params,
	.hw_free = lucky_aif2_hw_free,
};

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

#if defined (CONFIG_SEC_FM_RADIO)
	lucky_change_mclk(card, params_rate(params));
	
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
#endif
	return 0;
}

static struct snd_soc_ops lucky_aif3_ops = {
	.shutdown = lucky_aif_shutdown,
	.hw_params = lucky_aif3_hw_params,
};

static struct snd_soc_dai_driver lucky_ext_dai[] = {
	{
		.name = "lucky-ext voice call",
		.playback = {
			.channels_min = 1,
			.channels_max = 4,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 4,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "lucky-ext bluetooth sco",
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
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
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
	{
		.name = "lucky-ext pdm",
		.playback = {
			.stream_name ="PDM Playback stream",
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name ="PDM Capture stream",
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static const struct snd_soc_pcm_stream dsm_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static struct snd_soc_dai_link lucky_cs47l91_dai[] = {
	{ /* playback & recording */
		.name = "playback-pri",
		.stream_name = "i2s0-pri",
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
	{ /* bluetooth sco */
		.name = "bluetooth sco",
		.stream_name = "lucky-ext bluetooth sco",
		.cpu_dai_name = "lucky-ext bluetooth sco",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "moon-aif3",
		.ops = &lucky_aif3_ops,
		.ignore_suspend = 1,
	},
	{ /* deep buffer playback */
		.name = "playback-sec",
		.stream_name = "i2s0-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "moon-aif1",
		.ops = &lucky_aif1_ops,
	},
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
#ifdef CONFIG_SND_SAMSUNG_PDM_EXT_AMP
	{ /* ext pdm dsm */
		.name = "codec-pdm-dsm",
		.stream_name = "lucky-ext pdm",
		.cpu_dai_name = "lucky-ext pdm",
		.codec_dai_name = "max98506-aif1",
		.codec_name = "max98506.0-0031",
		.dai_fmt = SND_SOC_DAIFMT_PDM | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
		.params = &dsm_params,
		.ignore_suspend = 1,
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
	struct snd_soc_card *card = lucky_card;
	struct arizona_machine_priv *priv = card->drvdata;
	char keyword_buf[100];
	char *envp[2];

	memset(keyword_buf, 0, sizeof(keyword_buf));

	dev_info(card->dev, "Voice Control Callback invoked!\n");

	if (priv->voice_uevent == 0) {
		regmap_read(priv->regmap_dsp,
					 priv->word_id_addr, &keyword_type);

		snprintf(keyword_buf, sizeof(keyword_buf),
			"VOICE_WAKEUP_WORD_ID=%x", keyword_type);
	} else if (priv->voice_uevent == 1) {
		snprintf(keyword_buf, sizeof(keyword_buf),
			"VOICE_WAKEUP_WORD_ID=LPSD");
	} else {
		dev_err(card->dev, "Invalid voice control mode =%d",
				priv->voice_uevent);
		return;
	}
	get_voicetrigger_dump(card);

	envp[0] = keyword_buf;
	envp[1] = NULL;

	dev_info(card->dev, "%s : raise the uevent, string = %s\n",
			__func__, keyword_buf);

	wake_lock_timeout(&priv->wake_lock, 5000);
	kobject_uevent_env(&(card->dev->kobj), KOBJ_CHANGE, envp);
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
	struct snd_soc_card *card = lucky_card;
	int ret;

	ret = kstrtoint(buf, 0, &keyword_type);
	if (ret)
		dev_err(card->dev,
			"Failed to convert a string to an int: %d\n", ret);

	dev_info(card->dev, "%s(): keyword_type = %x\n",
			__func__, keyword_type);

	return count;
}

static DEVICE_ATTR(keyword_type, S_IRUGO | S_IWUSR | S_IWGRP,
	svoice_keyword_type_show, svoice_keyword_type_set);

static void ez2ctrl_debug_cb(void)
{
	struct snd_soc_card *card = lucky_card;
	struct arizona_machine_priv *priv = card->drvdata;

	dev_info(card->dev, "Voice Control Callback invoked!\n");

	input_report_key(priv->input, priv->voice_key_event, 1);
	input_sync(priv->input);

	usleep_range(10000, 20000);

	input_report_key(priv->input, priv->voice_key_event, 0);
	input_sync(priv->input);
}

static int lucky_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct arizona_machine_priv *priv = card->drvdata;
	int i, ret;

	priv->codec = codec;
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
		dev_err(card->dev,
				"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	snd_soc_dapm_set_subseq(&card->dapm, "SPK", 1);

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
#if defined (CONFIG_SND_SAMSUNG_PDM_EXT_AMP)
	snd_soc_dapm_ignore_suspend(&codec->dapm, "SPKDAT1R");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "SPKDAT1L");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "IN4L");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "IN4R");
#endif
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

	if (priv->amp_type == AMP_TYPE_CS35L33) {
		/* Set OPCLK from MCLK1 */
		ret = snd_soc_codec_set_sysclk(codec, priv->opclk.clk_id,
					       priv->opclk.src_id,
					       priv->opclk.rate,
					       SND_SOC_CLOCK_OUT);
		if (ret < 0)
			dev_err(card->dev,
				 "Failed to set OPCLK from MCLK1: %d\n", ret);

		/* Set OPCLK to CS35L33 */
		ret = snd_soc_codec_set_sysclk(card->rtd[10].codec, 0,
					       0,
					       priv->opclk.rate,
					       SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev,
				 "Failed to set OPCLK to CS35L33: %d\n", ret);
	}

	/* Set DAI Clock Source */
	snd_soc_dai_set_sysclk(card->rtd[0].codec_dai,
			       ARIZONA_CLK_SYSCLK, 0, 0);
	snd_soc_dai_set_sysclk(card->rtd[1].codec_dai,
			       ARIZONA_CLK_ASYNCCLK, 0, 0);
	snd_soc_dai_set_sysclk(card->rtd[2].codec_dai,
			       ARIZONA_CLK_SYSCLK_2, 0, 0);
#if defined (CONFIG_SND_SAMSUNG_PDM_EXT_AMP)
	snd_soc_dai_set_sysclk(card->rtd[10].cpu_dai,
			       ARIZONA_CLK_SYSCLK, 0, 0);
#else
	snd_soc_dai_set_sysclk(card->rtd[10].cpu_dai,
			       ARIZONA_CLK_ASYNCCLK_2, 0, 0);
#endif
	wake_lock_init(&priv->wake_lock, WAKE_LOCK_SUSPEND,
				"lucky-voicewakeup");

	if (priv->seamless_voicewakeup) {
		arizona_set_ez2ctrl_cb(codec, ez2ctrl_voicewakeup_cb);

		svoice_class = class_create(THIS_MODULE, "svoice");
		ret = IS_ERR(svoice_class);
		if (ret) {
			dev_err(card->dev, "Failed to create class(svoice)!\n");
			goto err_class_create;
		}

		keyword_dev = device_create(svoice_class,
					NULL, 0, NULL, "keyword");
		ret = IS_ERR(keyword_dev);
		if (ret) {
			dev_err(card->dev,
					"Failed to create device(keyword)!\n");
			goto err_device_create;
		}

		ret = device_create_file(keyword_dev, &dev_attr_keyword_type);
		if (ret < 0) {
			dev_err(card->dev,
			"Failed to create device file in sysfs entries(%s)!\n",
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

	if (priv->regmap_dsp) {
#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
		if (priv->amp_type == AMP_TYPE_MAX98505 ||
			priv->amp_type == AMP_TYPE_MAX98506 ||
			priv->amp_type == AMP_TYPE_MAX98512)
			maxdsm_cal_set_regmap(priv->regmap_dsp);
#endif
#if defined(CONFIG_SND_SOC_CIRRUS_AMP_CAL)
		if (priv->amp_type == AMP_TYPE_CS35L33)
			cirrus_amp_calib_set_regmap(priv->regmap_dsp);
#endif
	}

#ifdef CONFIG_SND_ESA_SA_EFFECT
	ret = esa_effect_register(card);
	if (ret < 0) {
		dev_err(card->dev,
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

	/* Set SYSCLK from FLL*/
	ret = snd_soc_codec_set_sysclk(priv->codec, priv->sysclk.clk_id,
				       priv->sysclk.src_id,
				       priv->sysclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL: %d\n", ret);

	if (priv->dspclk.rate > 0) {
		/* Set DSPCLK FLL */
		if (priv->dspclk.fll_ref_id > 0) {
			ret = snd_soc_codec_set_pll(priv->codec,
				priv->dspclk.fll_ref_id,
				priv->dspclk.fll_ref_src, 0, 0);
			if (ret != 0)
				dev_err(card->dev,
					"Failed to start DSPCLK FLL REF: %d\n",
					ret);
		}

		ret = snd_soc_codec_set_pll(priv->codec, priv->dspclk.fll_id,
					    priv->dspclk.mclk.id,
					    priv->dspclk.mclk.rate,
					    priv->dspclk.fll_out);
		if (ret != 0) {
			dev_err(card->dev,
				"Failed to start DSPCLK FLL: %d\n", ret);
			return ret;
		}

		/* Set DSPCLK from FLL */
		ret = snd_soc_codec_set_sysclk(priv->codec, priv->dspclk.clk_id,
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

static int lucky_suspend_post(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	int dsp_state;

	/* When the card goes to suspend state, If codec is not active,
	 * the micbias of headset is enable and the ez2control is not running,
	 * The MCLK and the FLL3 should be disable to reduce the sleep current.
	 * In the other cases, these should keep previous status */

	dsp_state = arizona_dsp_status(priv->codec, LUCKY_EZ2CTRL_NUM);
	if (!priv->codec->component.active && dsp_state) {
		arizona_enable_force_bypass(priv->codec);
	} else if (!priv->codec->component.active && priv->ear_mic) {
		arizona_enable_force_bypass(priv->codec);
		lucky_stop_sysclk(card);
		dev_info(card->dev, "%s: stop_sysclk\n", __func__);
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

	return 0;
}

static int lucky_resume_pre(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	int dsp_state;

	/* When the card goes to resume state, If codec is not active,
	 * the micbias of headset is enable and the ez2control is not running,
	 * The MCLK and the FLL3 should be enable.
	 * In the other cases, these should keep previous status */

	dsp_state = arizona_dsp_status(priv->codec, LUCKY_EZ2CTRL_NUM);
	if (!priv->codec->component.active && dsp_state) {
		arizona_disable_force_bypass(priv->codec);
	} else if (!priv->codec->component.active && priv->ear_mic) {
		lucky_start_sysclk(card);
		arizona_disable_force_bypass(priv->codec);
		dev_info(card->dev, "%s: start_sysclk\n", __func__);
	}

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
	int m;
	char *amp_name, *codec_dai_name, *codec_name;

	num_cards = (int) ARRAY_SIZE(card_ids);
	for (n = 0; n < num_cards; n++) {
		if (NULL != of_find_node_by_name(NULL, card_ids[n])) {
			lucky_card = &lucky_cards[n];
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
		dev_err(card->dev, "Failed to register component: %d\n", ret);

	of_route = of_property_read_bool(card->dev->of_node,
						"samsung,audio-routing");
	if (!card->dapm_routes || !card->num_dapm_routes || of_route) {
		ret = snd_soc_of_parse_audio_routing(card,
				"samsung,audio-routing");
		if (ret != 0) {
			dev_err(card->dev,
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
				dev_err(card->dev,
					"Property 'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			codec_np = of_parse_phandle(np,
					"samsung,audio-codec", n);
			if (!codec_np) {
				dev_err(card->dev,
					"Property 'samsung,audio-codec' missing or invalid\n");
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
		dev_err(card->dev, "Failed to get device node\n");

	/* Find amp device with amp name */
	for (m = 0; m < AMP_TYPE_MAX; m++) {
		switch (m) {
		case AMP_TYPE_MAX98505:
			amp_name = "max98505";
			codec_dai_name = "max98505-aif1";
			codec_name = "max98505.0-0031";
			priv->amp_type = AMP_TYPE_MAX98505;
			break;
		case AMP_TYPE_MAX98506:
			amp_name = "max98506";
			codec_dai_name = "max98506-aif1";
			codec_name = "max98506.0-0031";
			priv->amp_type = AMP_TYPE_MAX98506;
			break;
		case AMP_TYPE_MAX98512:
			amp_name = "max98512";
			codec_dai_name = "max98512-aif1";
			codec_name = "max98512.0-0039";
			priv->amp_type = AMP_TYPE_MAX98512;
			break;
		case AMP_TYPE_CS35L33:
			amp_name = "cs35l33";
			codec_dai_name = "cs35l33";
			codec_name = "cs35l33.0-0040";
			priv->amp_type = AMP_TYPE_CS35L33;
			break;
		default:
			dev_err(card->dev, "There is no external amp.\n");
			break;
		}

		if (of_find_node_by_name(NULL, amp_name) != NULL) {
			for (n = 0; n < card->num_links; n++) {
				if (!strcmp(dai_link[n].name, "codec-dsm")) {
					dai_link[n].codec_dai_name =
								codec_dai_name;
					dai_link[n].codec_name = codec_name;
				} else if(!strcmp(dai_link[n].name, "codec-pdm-dsm")) {
					codec_dai_name = "max98512-aif-pdm";
					codec_name = "max98512.0-0039";
					priv->amp_type = AMP_TYPE_MAX98512;
					dai_link[n].codec_dai_name =
								codec_dai_name;
					dai_link[n].codec_name = codec_name;
				} else {
					dev_err(card->dev, "not support c2c(%s)\n", dai_link[n].name);
				}
			}
			dev_info(card->dev, "Found amp (%s)\n", amp_name);
			break;
		}
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(card->dev, "Failed to register card:%d\n", ret);
		goto out;
	}

#if defined(CONFIG_SND_SOC_MAXIM_DSM_CAL)
	priv->dual_spk =
		of_property_read_bool(card->dev->of_node, "dual_spk");
#endif
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
