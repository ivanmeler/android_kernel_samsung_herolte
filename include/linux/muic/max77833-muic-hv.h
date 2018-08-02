/*
 * max77833-muic-hv.h - MUIC for the Maxim 77833
 *
 * Copyright (C) 2015 Samsung Electrnoics
 * Insun Choi <insun77.choi@samsung.com>
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
 * This driver is based on max77833-muic.h
 *
 */

#ifndef __MAX77833_MUIC_HV_H__
#define __MAX77833_MUIC_HV_H__

#define MUIC_HV_DEV_NAME		"muic-max77833-hv"

#define HV_CMD_PASS	0

typedef enum max77833_muic_hv_set_value {
	QC_SET_9V		= 0x09,

	FCHV_SET_9V		= 0x46,
	FCHV_SET_POWERPACK	= 0x24,
} muic_hv_set_val;

typedef enum max77833_muic_hv_enable_value {
	MPING_5TIMES		= 0x00,
	MPING_ALWAYS		= 0x02,
} muic_hv_enable_val;

extern void max77833_muic_set_afc_ready(struct max77833_muic_data *muic_data, bool value);
extern void max77833_muic_hv_fchv_disable_set(struct max77833_muic_data *muic_data);
extern void max77833_muic_hv_qc_disable_set(struct max77833_muic_data *muic_data);

extern void max77833_muic_hv_qc_enable(struct max77833_muic_data *muic_data);
extern void max77833_muic_hv_qc_disable(struct max77833_muic_data *muic_data);
extern void max77833_muic_hv_qc_autoset(struct max77833_muic_data *muic_data, u8 val, u8 mask);
extern void max77833_muic_hv_fchv_enable(struct max77833_muic_data *muic_data, u8 val, u8 mask);
extern void max77833_muic_hv_fchv_disable(struct max77833_muic_data *muic_data);
extern void max77833_muic_hv_fchv_set(struct max77833_muic_data *muic_data, u8 val, u8 mask);
extern void max77833_muic_hv_fchv_capa_read(struct max77833_muic_data *muic_data);
extern void max77833_muic_hv_chgin_read(struct max77833_muic_data *muic_data);

#endif /* __MAX77833_MUIC_HV_H__ */
