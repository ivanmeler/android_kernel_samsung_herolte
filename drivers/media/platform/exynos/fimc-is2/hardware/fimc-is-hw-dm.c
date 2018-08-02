/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/string.h>

#include "fimc-is-hw-dm.h"

#define CAMERA2_CTL(type)	(ctl->type)
#define CAMERA2_DM(type)	(dm->type)
#define CAMERA2_UCTL(type)	(uctl->type##Ud)
#define CAMERA2_UDM(type)	(udm->type)

#define DUP_SHOT_CTL_DM(type)						\
	do {											\
		memset(&CAMERA2_DM(type), 0x00, sizeof(typeof(CAMERA2_DM(type))));		\
		if (sizeof(typeof(CAMERA2_CTL(type))) ==	\
			sizeof(typeof(CAMERA2_DM(type))))		\
			memcpy((void *)&CAMERA2_DM(type),		\
				(void *)&CAMERA2_CTL(type),			\
				sizeof(typeof(CAMERA2_DM(type))));	\
	} while (0)

#define CPY_SHOT_CTL_DM(type, item)		\
	do {								\
		memset(&(CAMERA2_DM(type).item), 0x00, sizeof(typeof(CAMERA2_DM(type).item)));		\
			memcpy((void *)&(CAMERA2_DM(type).item),		\
				(void *)&(CAMERA2_CTL(type).item),			\
				sizeof(typeof(CAMERA2_DM(type).item)));	\
	} while (0)

#define DUP_SHOT_UCTL_UDM(type)						\
	do {											\
		memset(&CAMERA2_UDM(type), 0x00, sizeof(typeof(CAMERA2_UDM(type))));		\
		if (sizeof(typeof(CAMERA2_UCTL(type))) ==	\
			sizeof(typeof(CAMERA2_UDM(type))))		\
			memcpy((void *)&CAMERA2_UDM(type),		\
				(void *)&CAMERA2_UCTL(type),		\
				sizeof(typeof(CAMERA2_UDM(type))));	\
	} while (0)

#define CPY_SHOT_UCTL_UDM(type, item)			\
	do {										\
		memset(&(CAMERA2_UDM(type).item), 0x00, sizeof(typeof(CAMERA2_UDM(type).item)));		\
			memcpy((void *)&(CAMERA2_UDM(type).item),		\
				(void *)&(CAMERA2_UCTL(type).item),			\
				sizeof(typeof(CAMERA2_UDM(type).item)));	\
	} while (0)

#define CPY_COLOR_DM(item)	CPY_SHOT_CTL_DM(color, item)
static inline void copy_color_dm(camera2_ctl_t *ctl, camera2_dm_t *dm)
{
	CPY_COLOR_DM(vendor_hue);
	CPY_COLOR_DM(vendor_saturation);
	CPY_COLOR_DM(vendor_brightness);
	CPY_COLOR_DM(vendor_contrast);
}

#define CPY_AA_DM(item)	CPY_SHOT_CTL_DM(aa, item)
static inline void copy_aa_dm(camera2_ctl_t *ctl, camera2_dm_t *dm)
{
	CPY_AA_DM(aeAntibandingMode);
	CPY_AA_DM(aeExpCompensation);
	CPY_AA_DM(aeLock);
	CPY_AA_DM(aeMode);
	CPY_AA_DM(aeRegions[0]);
	CPY_AA_DM(aeRegions[1]);
	CPY_AA_DM(aeRegions[2]);
	CPY_AA_DM(aeRegions[3]);
	CPY_AA_DM(aeTargetFpsRange[0]);
	CPY_AA_DM(aeTargetFpsRange[1]);
	CPY_AA_DM(afMode);

	/* should be removed when valid regions are avaiable */
	CPY_AA_DM(afRegions[0]);
	CPY_AA_DM(afRegions[1]);
	CPY_AA_DM(afRegions[2]);
	CPY_AA_DM(afRegions[3]);
	
	CPY_AA_DM(afTrigger);
	CPY_AA_DM(awbLock);
	CPY_AA_DM(awbMode);
	CPY_AA_DM(mode);
	CPY_AA_DM(sceneMode);

	/* undocumented purpose */
	CPY_AA_DM(vendor_reserved[0]);

	CPY_AA_DM(vendor_aeExpCompensationStep);
	CPY_AA_DM(vendor_afmode_option);
	CPY_AA_DM(vendor_afmode_ext);
	CPY_AA_DM(vendor_aeflashMode);
	CPY_AA_DM(vendor_awbValue);
	CPY_AA_DM(vendor_afState);
}

#define CPY_EDGE_DM(item)	CPY_SHOT_CTL_DM(edge, item)
static inline void copy_edge_dm(camera2_ctl_t *ctl, camera2_dm_t *dm)
{
	CPY_EDGE_DM(strength);
}

#define CPY_NOISE_DM(item)	CPY_SHOT_CTL_DM(noise, item)
static inline void copy_noise_dm(camera2_ctl_t *ctl, camera2_dm_t *dm)
{
	CPY_NOISE_DM(strength);
}

#define CPY_LENS_DM(item)	CPY_SHOT_CTL_DM(lens, item)
static inline void copy_lens_dm(camera2_ctl_t *ctl, camera2_dm_t *dm)
{
	static float lastfocusd = -1;
	u32 tmp;

	dm->lens.state = LENS_STATE_STATIONARY;

	memcpy((void *)&tmp, (void *)&ctl->lens.focusDistance,
				sizeof(float));

	if ((int)tmp >= 0) {
		CPY_LENS_DM(focusDistance);

		if (memcmp(&ctl->lens.focusDistance, &lastfocusd,
				sizeof(float)))
			dm->lens.state = LENS_STATE_MOVING;
	} else {
		memset(&dm->lens.focusDistance, -1, sizeof(float));
	}

	memcpy((void *)&lastfocusd, (void *)&ctl->lens.focusDistance,
				sizeof(float));
}

static inline void copy_aa_udm(camera2_uctl_t *uctl, camera2_udm_t *udm)
{
	DUP_SHOT_UCTL_UDM(aa);
}

static inline void copy_lens_udm(camera2_uctl_t *uctl, camera2_udm_t *udm)
{
	if (uctl->lensUd.posSize != 0)
		DUP_SHOT_UCTL_UDM(lens);
	else
		memset(&udm->lens, 0x00, sizeof(camera2_lens_udm_t));

}

#define CPY_COMPANION_UDM(item)	CPY_SHOT_UCTL_UDM(companion, item)
static inline void copy_companion_udm(camera2_uctl_t *uctl,
					camera2_udm_t *udm)
{
	CPY_COMPANION_UDM(drc_mode);
	CPY_COMPANION_UDM(wdr_mode);
	CPY_COMPANION_UDM(paf_mode);
	CPY_COMPANION_UDM(lsc_mode);
	CPY_COMPANION_UDM(bpc_mode);
	CPY_COMPANION_UDM(bypass_mode);
}

int copy_ctrl_to_dm(struct camera2_shot *shot)
{
	/* dm */
	/* color correction */
	copy_color_dm(&shot->ctl, &shot->dm);
	/* aa */
	copy_aa_dm(&shot->ctl, &shot->dm);
	/* edge */
	copy_edge_dm(&shot->ctl, &shot->dm);
	/* lens */
	copy_lens_dm(&shot->ctl, &shot->dm);
	/* noise reduction */
	copy_noise_dm(&shot->ctl, &shot->dm);

	/* udm */
	/* aa */
	/*
	 * both af_data, gyroInfo are meaningful
	 * only for a sensor with AF actuator.
	 */
	copy_aa_udm(&shot->uctl, &shot->udm);
	/* lens */
	copy_lens_udm(&shot->uctl, &shot->udm);
	/* companion */
	copy_companion_udm(&shot->uctl, &shot->udm);

	return 0;
}
