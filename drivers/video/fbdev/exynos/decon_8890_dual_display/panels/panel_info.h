#ifndef __PANEL_INFO_H__
#define __PANEL_INFO_H__

#if defined(CONFIG_PANEL_S6E3HF2_WQXGA)
#include "s6e3hf2_wqxga_param.h"

#else
#error "ERROR !! Check LCD Panel Header File"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
#include "smart_dimming_core.h"
#endif

#endif

