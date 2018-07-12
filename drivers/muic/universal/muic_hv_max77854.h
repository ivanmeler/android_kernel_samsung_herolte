/*
 * muic_hv.h
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * Thomas Ryu <smilesr.ryu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *
 */

#ifndef __MAX77854_MUIC_HV_H__
#define __MAX77854_MUIC_HV_H__

#include "muic-internal.h"

/* MAX77854 STATUS1 register */
#define STATUS1_ADC_SHIFT               0
#define STATUS1_ADCERR_SHIFT            6
#define STATUS1_ADC1K_SHIFT             7
#define STATUS1_ADC_MASK                (0x1f << STATUS1_ADC_SHIFT)
#define STATUS1_ADCERR_MASK             (0x1 << STATUS1_ADCERR_SHIFT)
#define STATUS1_ADC1K_MASK              (0x1 << STATUS1_ADC1K_SHIFT)

/* MAX77854 STATUS2 register */
#define STATUS2_CHGTYP_SHIFT            0
#define STATUS2_CHGDETRUN_SHIFT         3
#define STATUS2_DCDTMR_SHIFT            4
#define STATUS2_DXOVP_SHIFT             5
#define STATUS2_VBVOLT_SHIFT            6
#define STATUS2_CHGTYP_MASK             (0x7 << STATUS2_CHGTYP_SHIFT)
#define STATUS2_CHGDETRUN_MASK          (0x1 << STATUS2_CHGDETRUN_SHIFT)
#define STATUS2_DCDTMR_MASK             (0x1 << STATUS2_DCDTMR_SHIFT)
#define STATUS2_DXOVP_MASK              (0x1 << STATUS2_DXOVP_SHIFT)
#define STATUS2_VBVOLT_MASK             (0x1 << STATUS2_VBVOLT_SHIFT)

/* MAX77854 CDETCTRL1 register */
#define CHGDETEN_SHIFT                  0
#define CHGTYPM_SHIFT                   1
#define CDDELAY_SHIFT                   4
#define CHGDETEN_MASK                   (0x1 << CHGDETEN_SHIFT)
#define CHGTYPM_MASK                    (0x1 << CHGTYPM_SHIFT)
#define CDDELAY_MASK                    (0x1 << CDDELAY_SHIFT)

/* MAX77828 INTMASK3 register */
#define INTMASK3_VBADCM_SHIFT		0
#define INTMASK3_VDNMONM_SHIFT		1
#define INTMASK3_DNRESM_SHIFT		2
#define INTMASK3_MPNACKM_SHIFT		3
#define INTMASK3_MRXBUFOWM_SHIFT	4
#define INTMASK3_MRXTRFM_SHIFT		5
#define INTMASK3_MRXPERRM_SHIFT		6
#define INTMASK3_MRXRDYM_SHIFT		7
#define INTMASK3_VBADCM_MASK		(MAX77854_ENABLE_BIT << INTMASK3_VBADCM_SHIFT)
#define INTMASK3_VDNMONM_MASK		(MAX77854_ENABLE_BIT << INTMASK3_VDNMONM_SHIFT)
#define INTMASK3_DNRESM_MASK		(MAX77854_ENABLE_BIT << INTMASK3_DNRESM_SHIFT)
#define INTMASK3_MPNACKM_MASK		(MAX77854_ENABLE_BIT << INTMASK3_MPNACKM_SHIFT)
#define INTMASK3_MRXBUFOWM_MASK		(MAX77854_ENABLE_BIT << INTMASK3_MRXBUFOWM_SHIFT)
#define INTMASK3_MRXTRFM_MASK		(MAX77854_ENABLE_BIT << INTMASK3_MRXTRFM_SHIFT)
#define INTMASK3_MRXPERRM_MASK		(MAX77854_ENABLE_BIT << INTMASK3_MRXPERRM_SHIFT)
#define INTMASK3_MRXRDYM_MASK		(MAX77854_ENABLE_BIT << INTMASK3_MRXRDYM_SHIFT)

/* MAX77854 HVCONTROL1 register */
#define HVCONTROL1_DPDNVDEN_SHIFT	0
#define HVCONTROL1_DNVD_SHIFT		1
#define HVCONTROL1_DPVD_SHIFT		3
#define HVCONTROL1_VBUSADCEN_SHIFT	5
#define HVCONTROL1_DPDNVDEN_MASK	(0x1 << HVCONTROL1_DPDNVDEN_SHIFT)
#define HVCONTROL1_DNVD_MASK		(0x3 << HVCONTROL1_DNVD_SHIFT)
#define HVCONTROL1_DPVD_MASK		(0x3 << HVCONTROL1_DPVD_SHIFT)
#define HVCONTROL1_VBUSADCEN_MASK	(0x1 << HVCONTROL1_VBUSADCEN_SHIFT)

/* MAX77854 STATUS3 register */
#define STATUS3_VBADC_SHIFT		0
#define STATUS3_VDNMON_SHIFT		4
#define STATUS3_DNRES_SHIFT		5
#define STATUS3_MPNACK_SHIFT		6
#define STATUS3_VBADC_MASK		(0xf << STATUS3_VBADC_SHIFT)
#define STATUS3_VDNMON_MASK		(0x1 << STATUS3_VDNMON_SHIFT)
#define STATUS3_DNRES_MASK		(0x1 << STATUS3_DNRES_SHIFT)
#define STATUS3_MPNACK_MASK		(0x1 << STATUS3_MPNACK_SHIFT)

/* MAX77854 HVCONTROL2 register */
#define HVCONTROL2_HVDIGEN_SHIFT	0
#define HVCONTROL2_DP06EN_SHIFT		1
#define HVCONTROL2_DNRESEN_SHIFT	2
#define HVCONTROL2_MPING_SHIFT		3
#define HVCONTROL2_MTXEN_SHIFT		4
#define HVCONTROL2_MTXBUSRES_SHIFT	5
#define HVCONTROL2_MPNGENB_SHIFT	6
#define HVCONTROL2_HVDIGEN_MASK		(0x1 << HVCONTROL2_HVDIGEN_SHIFT)
#define HVCONTROL2_DP06EN_MASK		(0x1 << HVCONTROL2_DP06EN_SHIFT)
#define HVCONTROL2_DNRESEN_MASK		(0x1 << HVCONTROL2_DNRESEN_SHIFT)
#define HVCONTROL2_MPING_MASK		(0x1 << HVCONTROL2_MPING_SHIFT)
#define HVCONTROL2_MTXEN_MASK		(0x1 << HVCONTROL2_MTXEN_SHIFT)
#define HVCONTROL2_MTXBUSRES_MASK	(0x1 << HVCONTROL2_MTXBUSRES_SHIFT)
#define HVCONTROL2_MPNGENB_MASK		(0x1 << HVCONTROL2_MPNGENB_SHIFT)

/* MAX77854 HVRXBYTE register */
#define HVRXBYTE_MAX			16

/* MAX77854 AFC charger W/A Check NUM */
#define AFC_CHARGER_WA_PING		5

/* MAX77854 MPing miss SW Workaround - delay time */
#define MPING_MISS_WA_TIME		2000


/* MAX77854 REGISTER ENABLE or DISABLE bit */
enum max77854_reg_bit_control {
        MAX77854_DISABLE_BIT            = 0,
        MAX77854_ENABLE_BIT,
};

typedef enum {
        CHGDETRUN_FALSE         = 0x00,
        CHGDETRUN_TRUE          = (0x1 << STATUS2_CHGDETRUN_SHIFT),

        CHGDETRUN_DONTCARE      = 0xff,
} chgdetrun_t;

typedef enum {
	DPDNVDEN_DISABLE	= 0x00,
	DPDNVDEN_ENABLE		= (0x1 << HVCONTROL1_DPDNVDEN_SHIFT),

	DPDNVDEN_DONTCARE	= 0xff,
} dpdnvden_t;

typedef enum {
	VDNMON_LOW		= 0x00,
	VDNMON_HIGH		= (0x1 << STATUS3_VDNMON_SHIFT),

	VDNMON_DONTCARE		= 0xff,
} vdnmon_t;

typedef enum {
	VBADC_VBDET		= 0x00,
	VBADC_4V_5V		= (0x1 << STATUS3_VBADC_SHIFT),
	VBADC_5V_6V		= (0x2 << STATUS3_VBADC_SHIFT),
	VBADC_6V_7V		= (0x3 << STATUS3_VBADC_SHIFT),
	VBADC_7V_8V		= (0x4 << STATUS3_VBADC_SHIFT),
	VBADC_8V_9V		= (0x5 << STATUS3_VBADC_SHIFT),
	VBADC_9V_10V		= (0x6 << STATUS3_VBADC_SHIFT),
	VBADC_10V_12V		= (0x7 << STATUS3_VBADC_SHIFT),
	VBADC_12V_13V		= (0x8 << STATUS3_VBADC_SHIFT),
	VBADC_13V_14V		= (0x9 << STATUS3_VBADC_SHIFT),
	VBADC_14V_15V		= (0xA << STATUS3_VBADC_SHIFT),
	VBADC_15V_16V		= (0xB << STATUS3_VBADC_SHIFT),
	VBADC_16V_17V		= (0xC << STATUS3_VBADC_SHIFT),
	VBADC_17V_18V		= (0xD << STATUS3_VBADC_SHIFT),
	VBADC_18V_19V		= (0xE << STATUS3_VBADC_SHIFT),
	VBADC_19V		= (0xF << STATUS3_VBADC_SHIFT),

	VBADC_QC_5V		= 0xeb,
	VBADC_QC_9V		= 0xec,
	VBADC_QC_12V		= 0xed,
	VBADC_QC_20V		= 0xee,

#if defined(CONFIG_MUIC_HV_12V)
	VBADC_AFC_12V		= 0xf9,
#endif	
	VBADC_AFC_5V		= 0xfa,
	VBADC_AFC_9V		= 0xfb,
	VBADC_AFC_ERR_V		= 0xfc,
	VBADC_AFC_ERR_V_NOT_0	= 0xfd,

	VBADC_ANY		= 0xfe,
	VBADC_DONTCARE		= 0xff,
} vbadc_t;

enum {
	HV_SUPPORT_QC_5V	= 5,
	HV_SUPPORT_QC_9V	= 9,
	HV_SUPPORT_QC_12V	= 12,
	HV_SUPPORT_QC_20V	= 20,
};

enum max77854_reg_hv_val {
	MAX77854_MUIC_HVCONTROL1_DPVD_06	= (0x2 << HVCONTROL1_DPVD_SHIFT),
	MAX77854_MUIC_HVCONTROL1_11	= (MAX77854_MUIC_HVCONTROL1_DPVD_06 | \
					HVCONTROL1_DPDNVDEN_MASK),
	MAX77854_MUIC_HVCONTROL1_31	= (HVCONTROL1_VBUSADCEN_MASK | \
					MAX77854_MUIC_HVCONTROL1_DPVD_06 | \
					HVCONTROL1_DPDNVDEN_MASK),
	MAX77854_MUIC_HVCONTROL2_06	= (HVCONTROL2_DP06EN_MASK | HVCONTROL2_DNRESEN_MASK),
	MAX77854_MUIC_HVCONTROL2_13	= (HVCONTROL2_MTXEN_MASK | HVCONTROL2_DP06EN_MASK | \
					HVCONTROL2_HVDIGEN_MASK),
	MAX77854_MUIC_HVCONTROL2_1B	= (HVCONTROL2_HVDIGEN_MASK | HVCONTROL2_DP06EN_MASK | \
					HVCONTROL2_MPING_MASK | HVCONTROL2_MTXEN_MASK),
	MAX77854_MUIC_HVCONTROL2_1F	= (HVCONTROL2_HVDIGEN_MASK | HVCONTROL2_DP06EN_MASK | \
					HVCONTROL2_DNRESEN_MASK | HVCONTROL2_MPING_MASK | HVCONTROL2_MTXEN_MASK),
	MAX77854_MUIC_HVCONTROL2_5B	= (HVCONTROL2_HVDIGEN_MASK | HVCONTROL2_DP06EN_MASK | \
					HVCONTROL2_MPING_MASK | HVCONTROL2_MTXEN_MASK | HVCONTROL2_MPNGENB_MASK),
	MAX77854_MUIC_INTMASK3_FB	= (INTMASK3_MRXRDYM_MASK | INTMASK3_MRXPERRM_MASK | \
					INTMASK3_MRXTRFM_MASK | INTMASK3_MRXBUFOWM_MASK | \
					INTMASK3_MPNACKM_MASK | INTMASK3_VDNMONM_MASK | \
					INTMASK3_VBADCM_MASK),
};

extern muic_attached_dev_t hv_muic_check_id_err
	(struct hv_data *phv, muic_attached_dev_t new_dev);

extern void max77854_hv_muic_reset_hvcontrol_reg(struct hv_data *phv);
#if defined(CONFIG_OF)
extern int of_max77854_hv_muic_dt(struct hv_data *phv);
#endif

extern int max77854_muic_hv_update_reg(struct i2c_client *i2c,
		const u8 reg, const u8 val, const u8 mask, const bool debug_en);

extern void max77854_hv_muic_init_detect(struct hv_data *phv);
extern void max77854_hv_muic_remove(struct hv_data *phv);
extern void max77854_hv_muic_remove_wo_free_irq(struct hv_data *phv);

extern void max77854_muic_set_adcmode_always(struct hv_data *phv);
#if !defined(CONFIG_SEC_FACTORY)
extern void max77854_muic_set_adcmode_oneshot(struct hv_data *phv);
#endif /* !CONFIG_SEC_FACTORY */
extern void max77854_hv_muic_adcmode_oneshot(struct hv_data *phv);

extern void max77854_muic_prepare_afc_charger(struct hv_data *phv);
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
extern void max77854_muic_prepare_afc_pogo_dock(struct hv_data *phv);
#endif
extern bool max77854_muic_check_change_dev_afc_charger
	(struct hv_data *phv, muic_attached_dev_t new_dev);
extern void max77854_hv_muic_connect_start(struct hv_data *phv);
extern void hv_muic_chgdet_ready(struct hv_data *phv);

#endif /* __MAX77854_MUIC_HV_H__ */

