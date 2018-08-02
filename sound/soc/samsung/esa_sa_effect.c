#include <sound/soc.h>
#include <sound/tlv.h>

#include <linux/module.h>
#include <linux/io.h>

#include "esa_sa_effect.h"
#include "seiren/seiren.h"

#define COMPRESSED_LR_VOL_MAX_STEPS     0x2000

const DECLARE_TLV_DB_LINEAR(compr_vol_gain,  0, COMPRESSED_LR_VOL_MAX_STEPS);
struct compr_pdata aud_vol;

extern int check_esa_compr_state(void);
extern void esa_compr_save_volume(int left, int right);

static int esa_ctl_sa_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = SA_MAX_COUNT;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 100;
	return 0;
}

static int esa_ctl_my_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = MYSOUND_MAX_COUNT;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 100;
	return 0;
}

static int esa_ctl_ps_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = VSP_MAX_COUNT;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1000;
	return 0;
}

static int esa_ctl_sb_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = LRSM_MAX_COUNT;
	uinfo->value.integer.min = -50;
	uinfo->value.integer.max = 50;
	return 0;
}

static int esa_ctl_bb_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = BB_MAX_COUNT;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 100;
	return 0;
}

static int esa_ctl_eq_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = EQ_MAX_COUNT;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 14000;
	return 0;
}

static int esa_ctl_ms_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = MYSPACE_MAX_COUNT;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 5;
	return 0;
}

static int esa_ctl_sr_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 192000;
	return 0;
}

static int esa_ctl_sa_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[SA_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < SA_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(SOUNDALIVE, effect_val, SA_MAX_COUNT);

	return 0;
}

static int esa_ctl_my_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[MYSOUND_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < MYSOUND_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(MYSOUND, effect_val, MYSOUND_MAX_COUNT);

	return 0;
}

static int esa_ctl_ps_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[VSP_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < VSP_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(PLAYSPEED, effect_val, VSP_MAX_COUNT);

	return 0;
}

static int esa_ctl_sb_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[LRSM_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < LRSM_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(SOUNDBALANCE, effect_val, LRSM_MAX_COUNT);

	return 0;
}

static int esa_ctl_bb_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[BB_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < BB_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(BASSBOOST, effect_val, BB_MAX_COUNT);

	return 0;
}

static int esa_ctl_eq_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[EQ_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < EQ_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(EQUALIZER, effect_val, EQ_MAX_COUNT);

	return 0;
}

static int esa_ctl_ms_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[MYSPACE_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < MYSPACE_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(MYSPACE, effect_val, MYSPACE_MAX_COUNT);

	return 0;
}

static int esa_ctl_sr_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	u32 rate = (u32)ucontrol->value.integer.value[0];

	pr_info("%s set sample rate = %dHz\n", __func__, rate);
	esa_compr_set_sample_rate(rate);

	return 0;
}

static int esa_ctl_sa_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_my_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_ps_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_sb_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_bb_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_eq_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_ms_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_sr_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

#define ESA_CTL_EQ_SWITCH(xname, xval, xinfo, xget_effect, xset_effect) {\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,\
	.info = xinfo, \
	.get = xget_effect, .put = xset_effect, \
	.private_value = xval }

int esa_compr_set_volume(struct audio_processor *ap, int left, int right)
{
	void __iomem *mailbox;

	esa_compr_save_volume(left, right);

	mailbox = esa_compr_get_mem();
	if (check_esa_compr_state()) {
		writel(left, mailbox + COMPR_LEFT_VOL);
		writel(right, mailbox + COMPR_RIGHT_VOL);
	}

	return 0;
}

static int compr_volume_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct audio_processor *ap = aud_vol.ap[mc->reg];
	uint32_t *volume = aud_vol.volume[mc->reg];

	volume[0] = ucontrol->value.integer.value[0];
	volume[1] = ucontrol->value.integer.value[1];
	pr_info("%s: mc->reg %d left_vol %d right_vol %d\n",
			__func__, mc->reg, volume[0], volume[1]);
	if (ap)
		esa_compr_set_volume(ap, volume[0], volume[1]);

	return 0;
}

static int compr_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	uint32_t *volume = aud_vol.volume[mc->reg];

	pr_info("%s: mc->reg %d left_vol %d right_vol %d\n",
		__func__, mc->reg, volume[0], volume[1]);
	ucontrol->value.integer.value[0] = volume[0];
	ucontrol->value.integer.value[1] = volume[1];

	return 0;
}

static const struct snd_kcontrol_new esa_effect_controls[] = {
	ESA_CTL_EQ_SWITCH("SA data", 0, \
		esa_ctl_sa_info, esa_ctl_sa_get, esa_ctl_sa_put),
	ESA_CTL_EQ_SWITCH("Audio DHA data", 0, \
		esa_ctl_my_info, esa_ctl_my_get, esa_ctl_my_put),
	ESA_CTL_EQ_SWITCH("VSP data", 0, \
		esa_ctl_ps_info, esa_ctl_ps_get, esa_ctl_ps_put),
	ESA_CTL_EQ_SWITCH("LRSM data", 0, \
		esa_ctl_sb_info, esa_ctl_sb_get, esa_ctl_sb_put),
	ESA_CTL_EQ_SWITCH("MSP data", 0, \
		esa_ctl_ms_info, esa_ctl_ms_get, esa_ctl_ms_put),
	ESA_CTL_EQ_SWITCH("SR data", 0, \
		esa_ctl_sr_info, esa_ctl_sr_get, esa_ctl_sr_put),
	SOC_DOUBLE_EXT_TLV("Compress Playback 3 Volume", COMPR_DAI_MULTIMEDIA_1,
			0, 8, COMPRESSED_LR_VOL_MAX_STEPS, 0,
			compr_volume_get, compr_volume_put, compr_vol_gain),
	ESA_CTL_EQ_SWITCH("ESA BBoost data", 0, \
		esa_ctl_bb_info, esa_ctl_bb_get, esa_ctl_bb_put),
	ESA_CTL_EQ_SWITCH("ESA EQ data", 0, \
		esa_ctl_eq_info, esa_ctl_eq_get, esa_ctl_eq_put),
};

int esa_effect_register(struct snd_soc_card *card)
{
	int ret;

	ret = snd_soc_add_card_controls(card, esa_effect_controls,
			ARRAY_SIZE(esa_effect_controls));

	if (ret < 0)
		pr_err("Failed to add controls : %d", ret);

	return ret;
}

MODULE_AUTHOR("HaeKwang Park, <haekwang0808.park@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC SEIREN effect Driver");
MODULE_LICENSE("GPL");
