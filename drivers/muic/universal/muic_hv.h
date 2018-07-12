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

#ifndef __MUIC_HV_H__
#define __MUIC_HV_H__

#include "muic-internal.h"

#define MUIC_HV_DEV_NAME	"muic-hv"

struct hv_vps {
	u8 vdnmon;
	u8 dpdnvden;
	u8 mpnack;
	u8 vbadc;
	u8 hvcontrol[2];
	u8 status1;
	u8 status2;
	u8 status3;	
};

/* MUIC afc irq type */
typedef enum {
        MUIC_AFC_IRQ_VDNMON = 0,
        MUIC_AFC_IRQ_MRXRDY,
        MUIC_AFC_IRQ_VBADC,
        MUIC_AFC_IRQ_MPNACK,

        MUIC_AFC_IRQ_DONTCARE = 0xff,
} muic_afc_irq_t;

/* muic chip specific internal data structure */
typedef struct max77854_muic_afc_data {
        muic_attached_dev_t             new_dev;
        const char                      *afc_name;
        muic_afc_irq_t                  afc_irq;
        u8                              hvcontrol1_dpdnvden;
        u8                              status3_vbadc;
        u8                              status3_vdnmon;
        int                             function_num;
        struct max77854_muic_afc_data   *next;
} muic_afc_data_t;


struct hv_data {
	//Fixme. Thomas
	muic_data_t			*pmuic;	
	struct i2c_client *i2c; /* i2c addr: 0x4A; MUIC */	
	muic_attached_dev_t 		attached_dev;
	struct mutex 			*pmutex;
	int				irq_gpio;

	bool				is_muic_ready;
	bool				afc_disable;

	struct hv_vps			vps;

        bool                            is_afc_muic_ready;
        bool                            is_afc_handshaking;
        bool                            is_afc_muic_prepare;
        bool                            is_charger_ready;
        bool                            is_qc_vb_settle;

        u8                              is_boot_dpdnvden;
        u8                              tx_data;

        bool                            is_mrxrdy;

        int                             afc_count;
        muic_afc_data_t                 afc_data;
        u8                              qc_hv;
        struct delayed_work             hv_muic_qc_vb_work;
        struct delayed_work             hv_muic_mping_miss_wa;

        int                             irq_vdnmon;
        int                             irq_mrxrdy;
        int                             irq_mpnack;
        int                             irq_vbadc;
};


#define MUIC_HV_5V	0x08
#define MUIC_HV_9V	0x46
#define MUIC_HV_12V	0x79


extern void hv_initialize(muic_data_t *pmuic, struct hv_data **pphv);
extern void hv_configure_AFC(struct hv_data *phv);
extern void hv_update_status(struct hv_data *phv, int mdev);
extern bool hv_is_predetach_required(int mdev);
extern bool hv_do_predetach(struct hv_data *phv, int mdev);
extern bool hv_is_running(struct hv_data *phv);
extern void hv_do_detach(struct hv_data *phv);
extern void hv_set_afc_by_user(struct hv_data *phv, bool onoff);
extern void hv_muic_change_afc_voltage(muic_data_t *pmuic, int tx_data);
extern void hv_clear_hvcontrol(struct hv_data *phv);

#endif /* __MUIC_HV_H__ */

