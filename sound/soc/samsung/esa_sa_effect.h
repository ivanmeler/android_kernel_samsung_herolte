#ifndef _ESA_EFFECT_AUDIO_H
#define _ESA_EFFECT_AUDIO_H

enum {
	SOUNDALIVE = 0,
	MYSOUND,
	PLAYSPEED,
	SOUNDBALANCE,
	MYSPACE,
	BASSBOOST,
	EQUALIZER,
};

/* Effect offset */
#define EFFECT_BASE		(0x1A000)

#define SA_BASE			(0x0)
#define SA_CHANGE_BIT		(0x0)
#define SA_OUT_DEVICE		(0x10)
#define	SA_PRESET 		(0x14)
#define SA_EQ_BEGIN		(0x18)
#define SA_EQ_END		(0x30)
#define SA_3D_LEVEL		(0x34)
#define SA_BE_LEVEL		(0x38)
#define SA_REVERB		(0x3C)
#define SA_ROOMSIZE		(0x40)
#define SA_CLA_LEVEL		(0x44)
#define SA_VOLUME_LEVEL		(0x48)
#define SA_SQUARE_ROW		(0x4C)
#define SA_SQUARE_COLUMN	(0x50)
#define SA_TAB_INFO		(0x54)
#define SA_NEW_UI		(0x58)
#define SA_3D_ON		(0x5C)
#define SA_3D_ANGLE_L		(0x60)
#define SA_3D_ANGLE_R		(0x64)
#define SA_3D_GAIN_L		(0x68)
#define SA_3D_GAIN_R		(0x6C)
#define SA_MAX_COUNT		(24)

#define MYSOUND_BASE		(0x100)
#define MYSOUND_CHANGE_BIT	(0x0)
#define MYSOUND_DHA_ENABLE	(0x10)
#define MYSOUND_GAIN_BEGIN	(0x14)
#define MYSOUND_OUT_DEVICE	(0x48)
#define MYSOUND_MAX_COUNT	(14)

#define VSP_BASE		(0x200)
#define VSP_CHANGE_BIT		(0x0)
#define VSP_INDEX		(0x10)
#define VSP_MAX_COUNT		(2)

#define LRSM_BASE		(0x300)
#define LRSM_CHANGE_BIT		(0x0)
#define LRSM_INDEX0		(0x10)
#define LRSM_INDEX1		(0x20)
#define LRSM_MAX_COUNT		(3)

#define MYSPACE_BASE		(0x340)
#define MYSPACE_CHANGE_BIT	(0x0)
#define MYSPACE_PRESET		(0x10)
#define MYSPACE_MAX_COUNT	(2)

#define BB_BASE		(0x400)
#define BB_CHANGE_BIT		(0x0)
#define BB_STATUS		(0x10)
#define BB_STRENGTH		(0x14)
#define BB_MAX_COUNT		(2)

#define EQ_BASE		(0x500)
#define EQ_CHANGE_BIT		(0x0)
#define EQ_STATUS		(0x10)
#define EQ_PRESET		(0x14)
#define EQ_NBAND		(0x18)
#define EQ_NBAND_LEVEL		(0x1c)	/* 10 nband levels */
#define EQ_NBAND_FREQ		(0x44)	/* 10 nband frequencies */
#define EQ_MAX_COUNT		(23)

/* CommBox ELPE Parameter */
#define ELPE_BASE		(0x600)
#define ELPE_CMD		(0x0)
#define ELPE_ARG0		(0x4)
#define ELPE_ARG1		(0x8)
#define ELPE_ARG2		(0xC)
#define ELPE_ARG3		(0x10)
#define ELPE_ARG4		(0x14)
#define ELPE_ARG5		(0x18)
#define ELPE_ARG6		(0x1C)
#define ELPE_ARG7		(0x20)
#define ELPE_ARG8		(0x24)
#define ELPE_ARG9		(0x28)
#define ELPE_ARG10		(0x2C)
#define ELPE_ARG11		(0x30)
#define ELPE_ARG12		(0x34)
#define ELPE_RET		(0x38)
#define ELPE_DONE		(0x3C)

#define CHANGE_BIT		(1)

int esa_effect_register(struct snd_soc_card *card);

enum {
	COMPR_DAI_MULTIMEDIA_1 = 0,
	COMPR_DAI_MAX,
};

struct compr_pdata {
	struct audio_processor *ap[COMPR_DAI_MAX];
	uint32_t volume[COMPR_DAI_MAX][2]; /* Left & Right */
};
extern struct compr_pdata aud_vol;
#endif
