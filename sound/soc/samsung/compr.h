/*
 *  compr.h --
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  ALSA COMPRESS interface for the Samsung SoC
 */

#ifndef _SAMSUNG_AUDIO_COMPR_H
#define _SAMSUNG_AUDIO_COMPR_H

int asoc_compr_platform_register(struct device *dev);
void asoc_compr_platform_unregister(struct device *dev);

#endif
