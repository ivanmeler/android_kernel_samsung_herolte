#ifndef __PANEL_INFO_H__
#define __PANEL_INFO_H__


#if defined(CONFIG_PANEL_S6E3FA3_FHD)
#include "s6e3fa3_fhd_param.h"
#endif

#if defined(CONFIG_PANEL_S6E3HF4_FHD)
#include "s6e3hf4_fhd_param.h"
#endif

#if defined(CONFIG_PANEL_S6E3HF3_FHD)
#include "s6e3hf3_fhd_param.h"
#endif

#include "smart_dimming_core.h"


struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

#define POWER_IS_ON(pwr)			(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(brightness)		(brightness == EXTEND_BRIGHTNESS)
#define UNDER_MINUS_20(temperature)	(temperature <= -20)
#define UNDER_0(temperature)	(temperature <= 0)

enum {
	OVER_ZERO,
	UNDER_ZERO,
	UNDER_MINUS_TWENTY,
	TEMP_MAX
};

#define ACL_IS_ON(nit) 				(nit != 420)
#define CAPS_IS_ON(nit)				(nit >= 41)

#define NORMAL_TEMPERATURE			25	/* 25 degrees Celsius */
#define EXTEND_BRIGHTNESS 	365
#define UI_MAX_BRIGHTNESS 	255
#define UI_MIN_BRIGHTNESS 	0
#define UI_DEFAULT_BRIGHTNESS 134

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED		1
#define PANEL_STATE_SUSPENDING	2

#define PANEL_DISCONNECTED		0
#define PANEL_CONNECTED			1

#define CUR_MAIN_LCD_ON	 0x00
#define CUR_MAIN_LCD_OFF 0x02
#define CUR_SUB_LCD_ON	0x00
#define CUR_SUB_LCD_OFF	0x01
#define CUR_MAX			4

#define REQ_ON_MAIN_LCD		0x00
#define REQ_ON_SUB_LCD		0x01
#define REQ_OFF_MAIN_LCD	0x02
#define REQ_OFF_SUB_LCD		0x03
#define REQ_MAX				4


#define SKIP_UPDATE		0x00000000
#define NEED_TO_UPDATE	0x80000000
#define FULL_UPDATE		0x40000000


#define PANEL_STAT_DISPLAY_ON		0x00001000

#define PANEL_STAT_MASTER			0x00000000
#define PANEL_STAT_SLAVE			0x00000001
#define PANEL_STAT_STANDALONE		0x00000002
#define PANEL_STAT_OFF				0x00000003
#define GET_PANEL_STAT(x) 		(x & 0x000000ff)
#define MASK_PANEL_STAT			~(0x000000ff)


#define CHECK_NEED_DISPLAY_ON(x) ((GET_PANEL_STAT(x) != PANEL_STAT_OFF) && (!(x & PANEL_STAT_DISPLAY_ON)))
#define CHECK_ALREADY_DISPLAY_ON(x) ((GET_PANEL_STAT(x) != PANEL_STAT_OFF) && (x & PANEL_STAT_DISPLAY_ON))

#define REF_TE_NONE	0x00
#define REF_TE_MAIN	0x01
#define REF_TE_SUB	0x02
struct panel_info {
	struct aid_dimming_info *aid_info;
	const char *CMD_L1_TEST_KEY_ON;
	const char *CMD_L1_TEST_KEY_OFF;
	const char *CMD_L2_TEST_KEY_ON;
	const char *CMD_L2_TEST_KEY_OFF;
	const char *CMD_L3_TEST_KEY_ON;
	const char *CMD_L3_TEST_KEY_OFF;
	const char *CMD_GAMMA_UPDATE_1;
	const char *CMD_GAMMA_UPDATE_2;
	const char *CMD_ACL_ON_OPR;
	const char *CMD_ACL_ON;
	const char *CMD_ACL_OFF_OPR;
	const char *CMD_ACL_OFF;
	struct gamma_cmd *gamma_cmd;
	int mtp[VREF_POINT_CNT][CI_MAX];
	int vt_mtp[CI_MAX];
};


struct br_mapping_tbl {
	const unsigned int *idx_br_tbl;
	const unsigned int *ref_br_tbl;
	const unsigned int *phy_br_tbl;
};

struct panel_private {

	struct backlight_device *bd;
	unsigned char id[3];
	unsigned char code[5];
	unsigned char elvss_set[22];
	unsigned char tset[8];
	int	temperature;
	unsigned int coordinate[2];
	unsigned char date[4];
	unsigned int lcdConnected;
	unsigned int state;
	unsigned int br_index;
	unsigned int acl_enable;
	unsigned int caps_enable;
	unsigned int current_acl;
	unsigned int current_hbm;
	unsigned int current_vint;
	unsigned int siop_enable;

	void *dim_data;
	void *dim_info;
	unsigned int *br_tbl;
	unsigned char **hbm_tbl;
	unsigned char **acl_cutoff_tbl;
	unsigned char **acl_opr_tbl;
	struct mutex lock;
	struct dsim_panel_ops *ops;
	unsigned int panel_type;
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	unsigned int mdnie_support;
#endif
#ifdef CONFIG_EXYNOS_DECON_LCD_MCD
	unsigned int mcd_on;
#endif

	unsigned int adaptive_control;
	int lux;
	struct class *mdnie_class;

	int panel_pos;

/*new feature */
	struct panel_info command;
	/*dimming t*/
	struct br_mapping_tbl mapping_tbl;
	int cur_br_idx;
	int cur_ref_br;
	int cur_phy_br;
	int cur_acl;

	int op_state;
	unsigned char syncReg[10];
	unsigned int pm_state;
	int esd_disable;
};


#endif

