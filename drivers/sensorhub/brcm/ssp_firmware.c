/*
*  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*/
#include "ssp.h"

#if defined (CONFIG_SENSORS_SSP_GRACE)
#undef SSP_FIRMWARE_REVISION_BCM
	#if ANDROID_VERSION < 70000
		#define SSP_FIRMWARE_REVISION_BCM	16100701
	#elif ANDROID_VERSION >= 80000
		#define SSP_FIRMWARE_REVISION_BCM	18032000
	#else
		#define SSP_FIRMWARE_REVISION_BCM	17090700
	#endif
#elif defined (CONFIG_SENSORS_SSP_HAECHI)
#undef SSP_FIRMWARE_REVISION_BCM
#define SSP_FIRMWARE_REVISION_BCM	17031600
#elif defined (CONFIG_SENSORS_SSP_HAECHI_880)
#undef SSP_FIRMWARE_REVISION_BCM
#define SSP_FIRMWARE_REVISION_BCM   17092001
#elif defined (CONFIG_SENSORS_SSP_HAECHI_888)
#undef SSP_FIRMWARE_REVISION_BCM
#define SSP_FIRMWARE_REVISION_BCM   18020800
#elif defined (CONFIG_SENSORS_SSP_LUCKY)
#undef SSP_FIRMWARE_REVISION_BCM
	#if ANDROID_VERSION < 70000
		#define SSP_FIRMWARE_REVISION_BCM	16122900
	#elif ANDROID_VERSION >= 80000
		#define SSP_FIRMWARE_REVISION_BCM	18031500
	#else
		#define SSP_FIRMWARE_REVISION_BCM	17060200
	#endif
#elif defined (CONFIG_SENSORS_SSP_VLTE)
#undef SSP_FIRMWARE_REVISION_BCM
#define SSP_FIRMWARE_REVISION_BCM	17011100
#else
#define SSP_FIRMWARE_REVISION_BCM	15080000
#endif

unsigned int get_module_rev(struct ssp_data *data)
{
	return SSP_FIRMWARE_REVISION_BCM;
}
