/*
 * This header provides constants for most thermal bindings.
 *
 * Copyright (C) 2013 Texas Instruments
 *	Eduardo Valentin <eduardo.valentin@ti.com>
 *
 * GPLv2 only
 */

#ifndef _DT_BINDINGS_THERMAL_THERMAL_H
#define _DT_BINDINGS_THERMAL_THERMAL_H

/* On cooling devices upper and lower limits */
#define THERMAL_NO_LIMIT		(-1UL)
#define TABLE_END			(~1)

#define THROTTLE_ACTIVE			(1)
#define THROTTLE_PASSIVE		(2)
#define SW_TRIP				(3)
#define HW_TRIP				(4)

#define DISABLE				(0)
#define ENABLE				(1)

#define NORMAL_SENSOR	(0)
#define VIRTUAL_SENSOR	(1)

#endif

