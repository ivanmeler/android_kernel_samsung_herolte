#ifndef __PANEL_DIMMING__
#define __PANEL_DIMMING__

#include "s6e3ha0_wqhd_param.h"

#define OLED_CMD_GAMMA_CNT		36
#define MAX_VREF_NUM			15
#define MAX_GRADATION			256

enum {
	CI_RED = 0,
	CI_GREEN,
	CI_BLUE,
	CI_MAX,
};

struct v_constant {
	int nu;
	int de;
};

struct dim_data {
	int t_gamma[MAX_VREF_NUM][CI_MAX];
	int mtp[MAX_VREF_NUM][CI_MAX];
	int volt[MAX_GRADATION][CI_MAX];
	int volt_vt[CI_MAX];
	int vt_mtp[CI_MAX];
	int look_volt[MAX_VREF_NUM][CI_MAX];
};

struct SmtDimInfo {
	unsigned int br;
	unsigned int refBr;
	const unsigned int *cGma;
	const signed char *rTbl;
	const signed char *cTbl;
	const unsigned char *aid;
	const unsigned char *elvAcl;
	const unsigned char *elv;
	unsigned char gamma[OLED_CMD_GAMMA_CNT];
};

#endif
