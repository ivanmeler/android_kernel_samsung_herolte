#ifndef __S6E3HF2_WQHD_PARAM_H__
#define __S6E3HF2_WQHD_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>


#define S6E3FA3_MTP_ADDR 			0xC8
#define S6E3FA3_MTP_SIZE 			35

#define S6E3FA3_MTP_DATE_SIZE 		S6E3FA3_MTP_SIZE + 9
#define S6E3FA3_COORDINATE_REG		0xA1
#define S6E3FA3_COORDINATE_LEN		4

#define S6E3FA3_HBMGAMMA_REG		0xB4
#define S6E3FA3_HBMGAMMA_LEN		31



#define S6E3FA3_ID_REG				0x04
#define S6E3FA3_ID_LEN				3
#define S6E3FA3_CODE_REG			0xD6
#define S6E3FA3_CODE_LEN			5
#define TSET_REG			0xB8 /* TSET: Global para 8th */
#define TSET_LEN			9
#define TSET_MINUS_OFFSET			0x03
#define ELVSS_REG			0xB6
#define ELVSS_LEN			23   /* elvss: Global para 22th */
#define VINT_REG					0xF4
#define VINT_LEN					3

#define GAMMA_CMD_CNT 		36
#define AID_CMD_CNT			3
#define ELVSS_CMD_CNT		3


#define S6E3FA3_MAX_BRIGHTNESS	420
#define S6E3FA3_HBM_BRIGHTNESS	600


static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_TEST_KEY_ON_F1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_F1[] = {
	0xF1,
	0xA5, 0xA5,
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_STAND_ALONE_MODE[] = {
	0xF2,
	0x31,
};


static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28
};


static const unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
	0x00, 0x00
};

static const unsigned char SEQ_DEFAULT_AID_SETTING[] = {
	0xB2,
	0x00, 0x12
};

static const unsigned char SEQ_DEFAULT_ELVSS_SET[] = {
	0xB6,
	0xBC, 0x0A
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03
};

static const unsigned char SEQ_GAMMA_UPDATE_L[] = {
	0xF7,
	0x00,
};


static const unsigned char SEQ_TE_ON[] = {
	0x35, 0x00
};

static const unsigned char SEQ_TE_MODE[] = {
	0xB9,
	0x02, 0x07, 0x82, 0x00, 0x09
};

static const unsigned char SEQ_TE_OFF[] = {
	0x34,
};

static const unsigned char SEQ_OPR_ACL_OFF[] = {
	0xB5,
	0x40
};

static const unsigned char SEQ_OPR_ACL_ON[] = {
	0xB5,
	0x50,
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_ON[] = {
	0x55,
	0x02
};

static const unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xC0
};

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00
};


#ifndef CONFIG_PANEL_AID_DIMMING

#define UNDIMMING_SEQ_TEST_KEY_ON_F0 SEQ_TEST_KEY_ON_F0
#define UNDIMMING_SEQ_TEST_KEY_OFF_F0 SEQ_TEST_KEY_OFF_F0
#define UNDIMMING_SEQ_GAMMA_CONDITION_SET SEQ_GAMMA_CONDITION_SET
#define UNDIMMING_SEQ_GAMMA_UPDATE SEQ_GAMMA_UPDATE
#define UNDIMMING_SEQ_GAMMA_UPDATE_L SEQ_GAMMA_UPDATE_L

#endif
#endif /* __S6E3HF2_PARAM_H__ */

