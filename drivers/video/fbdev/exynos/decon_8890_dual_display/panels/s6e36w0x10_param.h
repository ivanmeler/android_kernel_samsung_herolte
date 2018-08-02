#ifndef __AMB184BW01_PARAM_H__
#define __AMB184BW01_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>


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
#define EXTEND_BRIGHTNESS	365
#define UI_MAX_BRIGHTNESS 	255
#define UI_MIN_BRIGHTNESS 	0
#define UI_DEFAULT_BRIGHTNESS 134


#define S6E36W0X10_MTP_ADDR 			0xC8
#define S6E36W0X10_MTP_SIZE 			35

#define S6E36W0X10_MTP_DATE_SIZE 		S6E36W0X10_MTP_SIZE + 9
#define S6E36W0X10_COORDINATE_REG		0xA1
#define S6E36W0X10_COORDINATE_LEN		4

#define S6E36W0X10_HBMGAMMA_REG		0xB4
#define S6E36W0X10_HBMGAMMA_LEN		31



#define S6E36W0X10_ID_REG				0x04
#define S6E36W0X10_ID_LEN				3
#define S6E36W0X10_CODE_REG			0xD6
#define S6E36W0X10_CODE_LEN			5
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


#define S6E36W0X10_MAX_BRIGHTNESS	420
#define S6E36W0X10_HBM_BRIGHTNESS	600


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

static const unsigned char SEQ_ALC_SETTING[] = {
	0xB5, 
	0x51, 0x3C, 0x00, 0x95, 
	0x00
};

static const unsigned char SEQ_TEMP_OFFSET_1[] = {
	0xB0, 
	0x0B,
};

static const unsigned char SEQ_TEMP_OFFSET_2[] = {
	0xB6,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x08, 0x08,
	0x08
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
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, /* V255 R,G,B*/
	0x80, 0x80, 0x80, /* V203 R,G,B*/
	0x80, 0x80, 0x80, /* V151 R,G,B*/
	0x80, 0x80, 0x80, /* V87 R,G,B*/
	0x80, 0x80, 0x80, /* V51 R,G,B*/
	0x80, 0x80, 0x80, /* V35 R,G,B*/
	0x80, 0x80, 0x80, /* V23 R,G,B*/
	0x80, 0x80, 0x80, /* V11 R,G,B*/
	0x80, 0x80, 0x80, /* V3 R,G,B*/
	0x00, 0x00, 0x00, /* V0 R,G,B*/
	0x00, 0x00
};


static const unsigned char SEQ_DEFAULT_AID_SETTING[] = {
	0xB2,
	0x00, 0x12
};

static const unsigned char SEQ_AID_SETTING[] = {
	0xB2,			/*	AOR 10%	*/
	0xE0, 0x06
};

static const unsigned char SEQ_DEFAULT_ELVSS_SET[] = {
	0xB6,
	0xBC, 0x0A
};

/*set elvss to 183nit */
static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x88, 0x1B,
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03
};

static const unsigned char SEQ_TE_ON[] = {
	0x35, 0x00
};

static const unsigned char SEQ_TE_OFF[] = {
	0x34,
};


static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_ON[] = {
	0x55,
	0x02
};


#endif /* __AMB184BW01_PARAM_H__ */


