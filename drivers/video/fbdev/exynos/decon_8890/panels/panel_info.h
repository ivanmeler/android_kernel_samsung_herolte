#ifndef __PANEL_INFO_H__
#define __PANEL_INFO_H__

#if defined(CONFIG_PANEL_S6E3HA3_DYNAMIC)
#include "s6e3ha3_s6e3ha2_wqhd_param.h"

#elif defined(CONFIG_PANEL_S6E3HF4_WQHD)
#include "s6e3ha3_s6e3ha2_wqhd_param.h"

#elif defined(CONFIG_PANEL_S6E3HF4_WQHD_HAECHI)
#include "s6e3hf4_wqhd_haechi_param.h"

#elif defined(CONFIG_PANEL_S6E3HA5_WQHD)
#include "s6e3hf4_s6e3ha5_wqhd_param.h"

#elif defined(CONFIG_PANEL_S6E3FA2_FHD)
#include "s6e3fa2_fhd_param.h"

#elif defined(CONFIG_PANEL_S6E3HF2_WQXGA)
#include "s6e3hf2_wqxga_param.h"

#elif defined(CONFIG_PANEL_S6E3HF4_WQXGA)
#include "s6e3hf4_wqxga_param.h"

#else
#error "ERROR !! Check LCD Panel Header File"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#endif

#endif
