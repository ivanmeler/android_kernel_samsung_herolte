/*
 * max77843-muic.c - MUIC driver for the Maxim 77843
 *
 *  Copyright (C) 2012 Samsung Electronics
 *  Seoyoung Jeong <seo0.jeong@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#include <linux/mfd/max77843.h>
#include <linux/mfd/max77843-private.h>

/* MUIC header file */
#include <linux/muic/muic.h>
#include <linux/muic/max77843-muic-hv-typedef.h>
#include <linux/muic/max77843-muic.h>
#include <linux/muic/max77843-muic-hv.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if !defined(CONFIG_SEC_FACTORY)
#if defined(CONFIG_MUIC_ADCMODE_SWITCH_WA)
#include <linux/delay.h>
#endif /* CONFIG_MUIC_ADCMODE_SWITCH_WA */
#endif /* !CONFIG_SEC_FACTORY */

static bool debug_en_checklist = false;

/* temp function for function pointer (TODO) */
enum act_function_num {
	FUNC_TA_TO_PREPARE		= 0,
	FUNC_PREPARE_TO_PREPARE_DUPLI,
	FUNC_PREPARE_TO_AFC_5V,
	FUNC_PREPARE_TO_QC_PREPARE,
	FUNC_PREPARE_DUPLI_TO_PREPARE_DUPLI,
	FUNC_PREPARE_DUPLI_TO_AFC_5V,
	FUNC_PREPARE_DUPLI_TO_AFC_ERR_V,
	FUNC_PREPARE_DUPLI_TO_AFC_9V,
	FUNC_PREPARE_DUPLI_TO_QC_PREPARE,
	FUNC_AFC_5V_TO_AFC_5V_DUPLI,
	FUNC_AFC_5V_TO_AFC_ERR_V,
	FUNC_AFC_5V_TO_AFC_9V,
	FUNC_AFC_5V_TO_QC_PREPARE,
	FUNC_AFC_5V_DUPLI_TO_AFC_5V_DUPLI,
	FUNC_AFC_5V_DUPLI_TO_AFC_ERR_V,
	FUNC_AFC_5V_DUPLI_TO_AFC_9V,
	FUNC_AFC_5V_DUPLI_TO_QC_PREPARE,
	FUNC_AFC_ERR_V_TO_AFC_ERR_V_DUPLI,
	FUNC_AFC_ERR_V_TO_AFC_9V,
	FUNC_AFC_ERR_V_TO_QC_PREPARE,
	FUNC_AFC_ERR_V_DUPLI_TO_AFC_ERR_V_DUPLI,
	FUNC_AFC_ERR_V_DUPLI_TO_AFC_9V,
	FUNC_AFC_ERR_V_DUPLI_TO_QC_PREPARE,
	FUNC_AFC_9V_TO_AFC_9V,
	FUNC_QC_PREPARE_TO_QC_5V,
	FUNC_QC_PREPARE_TO_QC_9V,
	FUNC_QC_5V_TO_QC_5V,
	FUNC_QC_5V_TO_QC_9V,
	FUNC_QC_9V_TO_QC_9V,
};

/* afc_condition_checklist[ATTACHED_DEV_TA_MUIC] */
muic_afc_data_t ta_to_prepare = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC,
	.afc_name		= "AFC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_VDNMON,
	.hvcontrol1_dpdnvden	= DPDNVDEN_ENABLE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_LOW,
	.function_num		= FUNC_TA_TO_PREPARE,
	.next			= &ta_to_prepare,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC] */
muic_afc_data_t prepare_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DONTCARE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_TO_QC_PREPARE,
	.next			= &prepare_to_qc_prepare,
};

muic_afc_data_t prepare_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_5V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_TO_AFC_5V,
	.next			= &prepare_to_qc_prepare,
};

muic_afc_data_t prepare_to_prepare_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC,
	.afc_name		= "AFC charger prepare (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_TO_PREPARE_DUPLI,
	.next			= &prepare_to_afc_5v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC] */
muic_afc_data_t prepare_dupli_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DONTCARE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_QC_PREPARE,
	.next			= &prepare_dupli_to_qc_prepare,
};

muic_afc_data_t prepare_dupli_to_prepare_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC,
	.afc_name		= "AFC charger prepare (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_PREPARE_DUPLI,
	.next			= &prepare_dupli_to_qc_prepare,
};

muic_afc_data_t prepare_dupli_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_AFC_9V,
	.next			= &prepare_dupli_to_prepare_dupli,
};

muic_afc_data_t prepare_dupli_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_ERR_V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_AFC_ERR_V,
	.next			= &prepare_dupli_to_afc_9v,
};

muic_afc_data_t prepare_dupli_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_5V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_AFC_5V,
	.next			= &prepare_dupli_to_afc_err_v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_5V_MUIC] */
muic_afc_data_t afc_5v_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DONTCARE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_QC_PREPARE,
	.next			= &afc_5v_to_qc_prepare,
};

muic_afc_data_t afc_5v_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_AFC_9V,
	.next			= &afc_5v_to_qc_prepare,
};

muic_afc_data_t afc_5v_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_ERR_V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_AFC_ERR_V,
	.next			= &afc_5v_to_afc_9v,
};

muic_afc_data_t afc_5v_to_afc_5v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC,
	.afc_name		= "AFC charger 5V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_5V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_AFC_5V_DUPLI,
	.next			= &afc_5v_to_afc_err_v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC] */
muic_afc_data_t afc_5v_dupli_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DONTCARE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_QC_PREPARE,
	.next			= &afc_5v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_5v_dupli_to_afc_5v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC,
	.afc_name		= "AFC charger 5V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_5V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_AFC_5V_DUPLI,
	.next			= &afc_5v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_5v_dupli_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_AFC_9V,
	.next			= &afc_5v_dupli_to_afc_5v_dupli,
};

muic_afc_data_t afc_5v_dupli_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_ERR_V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_AFC_ERR_V,
	.next			= &afc_5v_dupli_to_afc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC] */
muic_afc_data_t afc_err_v_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DONTCARE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_TO_QC_PREPARE,
	.next			= &afc_err_v_to_qc_prepare,
};

muic_afc_data_t afc_err_v_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_TO_AFC_9V,
	.next			= &afc_err_v_to_qc_prepare,
};

muic_afc_data_t afc_err_v_to_afc_err_v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC,
	.afc_name		= "AFC charger ERR V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_ERR_V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_TO_AFC_ERR_V_DUPLI,
	.next			= &afc_err_v_to_afc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC] */
muic_afc_data_t afc_err_v_dupli_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DONTCARE,
	.status3_vbadc		= VBADC_DONTCARE,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_DUPLI_TO_QC_PREPARE,
	.next			= &afc_err_v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_err_v_dupli_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_DUPLI_TO_AFC_9V,
	.next			= &afc_err_v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_err_v_dupli_to_afc_err_v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC,
	.afc_name		= "AFC charger ERR V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_ERR_V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_DUPLI_TO_AFC_ERR_V_DUPLI,
	.next			= &afc_err_v_dupli_to_afc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_9V_MUIC] */
muic_afc_data_t afc_9v_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_DONTCARE,
	.hvcontrol1_dpdnvden	= DPDNVDEN_DISABLE,
	.status3_vbadc		= VBADC_AFC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_TO_AFC_9V,
	.next			= &afc_9v_to_afc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC] */
muic_afc_data_t qc_prepare_to_qc_9v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_9V_MUIC,
	.afc_name		= "QC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_ENABLE,
	.status3_vbadc		= VBADC_QC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_PREPARE_TO_QC_9V,
	.next			= &qc_prepare_to_qc_9v,
};

muic_afc_data_t qc_prepare_to_qc_5v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_5V_MUIC,
	.afc_name		= "QC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_ENABLE,
	.status3_vbadc		= VBADC_QC_5V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_PREPARE_TO_QC_5V,
	.next			= &qc_prepare_to_qc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_QC_CHARGER_5V_MUIC] */
muic_afc_data_t qc_5v_to_qc_5v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_5V_MUIC,
	.afc_name		= "QC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_DONTCARE,
	.hvcontrol1_dpdnvden	= DPDNVDEN_ENABLE,
	.status3_vbadc		= VBADC_QC_5V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_5V_TO_QC_5V,
	.next			= &qc_5v_to_qc_5v,
};

muic_afc_data_t qc_5v_to_qc_9v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_9V_MUIC,
	.afc_name		= "QC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.hvcontrol1_dpdnvden	= DPDNVDEN_ENABLE,
	.status3_vbadc		= VBADC_QC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_5V_TO_QC_9V,
	.next			= &qc_5v_to_qc_5v,
};

/* afc_condition_checklist[ATTACHED_DEV_QC_CHARGER_9V_MUIC] */
muic_afc_data_t qc_9v_to_qc_9v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_9V_MUIC,
	.afc_name		= "QC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_DONTCARE,
	.hvcontrol1_dpdnvden	= DPDNVDEN_ENABLE,
	.status3_vbadc		= VBADC_QC_9V,
	.status3_vdnmon		= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_9V_TO_QC_9V,
	.next			= &qc_9v_to_qc_9v,
};

muic_afc_data_t		*afc_condition_checklist[ATTACHED_DEV_NUM] = {
	[ATTACHED_DEV_TA_MUIC]			= &ta_to_prepare,
	[ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC]	= &prepare_to_prepare_dupli,
	[ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC] = &prepare_dupli_to_afc_5v,
	[ATTACHED_DEV_AFC_CHARGER_5V_MUIC]	= &afc_5v_to_afc_5v_dupli,
	[ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC]= &afc_5v_dupli_to_afc_err_v,
	[ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC]	= &afc_err_v_to_afc_err_v_dupli,
	[ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC] = &afc_err_v_dupli_to_afc_err_v_dupli,
	[ATTACHED_DEV_AFC_CHARGER_9V_MUIC]	= &afc_9v_to_afc_9v,
	[ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC]	= &qc_prepare_to_qc_9v,
	[ATTACHED_DEV_QC_CHARGER_5V_MUIC]	= &qc_5v_to_qc_5v,
	[ATTACHED_DEV_QC_CHARGER_9V_MUIC]	= &qc_9v_to_qc_9v,
};

struct afc_init_data_s {
	struct work_struct muic_afc_init_work;
	struct max77843_muic_data *muic_data;
};
struct afc_init_data_s afc_init_data;

bool muic_check_is_hv_dev(struct max77843_muic_data *muic_data)
{
	bool ret = false;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	if (debug_en_checklist)
		pr_info("%s:%s attached_dev(%d)[%c]\n", MUIC_HV_DEV_NAME,
			__func__, muic_data->attached_dev, (ret ? 'T' : 'F'));

	return ret;
}

muic_attached_dev_t hv_muic_check_id_err
	(struct max77843_muic_data *muic_data, muic_attached_dev_t new_dev)
{
	muic_attached_dev_t after_new_dev = new_dev;

	if (!muic_check_is_hv_dev(muic_data))
		goto out;

	switch(new_dev) {
	case ATTACHED_DEV_TA_MUIC:
		pr_info("%s:%s cannot change HV(%d)->TA(%d)!\n", MUIC_DEV_NAME,
			__func__, muic_data->attached_dev, new_dev);
		after_new_dev = muic_data->attached_dev;
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		pr_info("%s:%s HV ID Err - Undefined\n", MUIC_DEV_NAME, __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC;
		break;
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		pr_info("%s:%s HV ID Err - Unsupported\n", MUIC_DEV_NAME, __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC;
		break;
	default:
		pr_info("%s:%s HV ID Err - Supported\n", MUIC_DEV_NAME, __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC;
		break;
	}
out:
	return after_new_dev;
}

static int max77843_hv_muic_read_reg(struct i2c_client *i2c, u8 reg, u8 *value)
{
	int ret;

	ret = max77843_read_reg(i2c, reg, value);
	if (ret < 0)
		pr_err("%s:%s err read REG(0x%02x) [%d]\n", MUIC_HV_DEV_NAME,
				__func__, reg, ret);
	return ret;
}

static int max77843_hv_muic_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	u8 before_val, after_val;
	int ret;

	max77843_read_reg(i2c, reg, &before_val);
	ret = max77843_write_reg(i2c, reg, value);
	max77843_read_reg(i2c, reg, &after_val);

	pr_info("%s:%s reg[0x%02x] = [0x%02x] + [0x%02x] -> [0x%02x]\n",
		MUIC_HV_DEV_NAME, __func__, reg, before_val, value, after_val);
	return ret;
}

int max77843_muic_hv_update_reg(struct i2c_client *i2c,
	const u8 reg, const u8 val, const u8 mask, const bool debug_en)
{
	u8 before_val, new_val, after_val;
	int ret = 0;

	ret = max77843_read_reg(i2c, reg, &before_val);
	if (ret)
		pr_err("%s:%s err read REG(0x%02x) [%d] \n", MUIC_DEV_NAME,
				__func__, reg, ret);

	new_val = (val & mask) | (before_val & (~mask));

	if (before_val ^ new_val) {
		ret = max77843_hv_muic_write_reg(i2c, reg, new_val);
		if (ret)
			pr_err("%s:%s err write REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);
	} else if (debug_en) {
		pr_info("%s:%s REG(0x%02x): already [0x%02x], don't write reg\n",
				MUIC_DEV_NAME, __func__, reg, before_val);
		goto out;
	}

	if (debug_en) {
		ret = max77843_read_reg(i2c, reg, &after_val);
		if (ret < 0)
			pr_err("%s:%s err read REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);

		pr_info("%s:%s REG(0x%02x): [0x%02x]+[0x%02x:0x%02x]=[0x%02x]\n",
				MUIC_DEV_NAME, __func__, reg, before_val,
				val, mask, after_val);
	}

out:
	return ret;
}

void max77843_hv_muic_reset_hvcontrol_reg(struct max77843_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;

	max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1, 0x00);
	max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL2, 0x00);
}

void max77843_muic_set_afc_ready(struct max77843_muic_data *muic_data, bool value)
{
	bool before, after;

	before = muic_data->is_afc_muic_ready;
	muic_data->is_afc_muic_ready = value;
	after = muic_data->is_afc_muic_ready;

	pr_info("%s:%s afc_muic_ready[%d->%d]\n", MUIC_DEV_NAME, __func__, before, after);
}

static int max77843_hv_muic_state_maintain(struct max77843_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s \n", MUIC_HV_DEV_NAME, __func__);

	if (muic_data->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s:%s Detached(%d), need to check after\n", MUIC_HV_DEV_NAME,
				__func__, muic_data->attached_dev);
		return ret;
	}

	return ret;
}

static void max77843_hv_muic_set_afc_after_prepare
					(struct max77843_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 value;

	pr_info("%s:%s HV charger is detected\n", MUIC_HV_DEV_NAME, __func__);

	/* Set HVCONTROL2 = 0x02 */
	max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL2,
						HVCONTROL2_DP06EN_MASK);

	/* Set HVCONTROL1 - disable DPVD, DPDNVDEN */
	max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1, 0x20);

	/* Set TX DATA */
	value = muic_data->tx_data;

	max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVTXBYTE, value);

	/* Set HVCONTROL2 = 0x1B */
	max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL2, MAX77843_MUIC_HVCONTROL2_1B);

}

static void max77843_hv_muic_set_afc_charger_handshaking
			(struct max77843_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 hvtxbyte;
	u8 hvrxbyte[HVRXBYTE_MAX];
	u8 selecthvtxbyte=0;
	int i, ret;

	pr_info("%s:%s \n", MUIC_HV_DEV_NAME, __func__);

	ret = max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL2, 0x13);
	if (IS_ERR_VALUE(ret))
		pr_err("%s:%s cannot write hvcontrol2(%d)\n", MUIC_HV_DEV_NAME, __func__, ret);

	max77843_hv_muic_read_reg(i2c, MAX77843_MUIC_REG_HVTXBYTE, &hvtxbyte);

	for(i = 0; i < HVRXBYTE_MAX; i++) {
		max77843_hv_muic_read_reg(i2c, (MAX77843_MUIC_REG_HVRXBYTE1+i), &hvrxbyte[i]);
		if(hvrxbyte[i] == 0)
			break;
	}

	if(hvrxbyte[0] != hvtxbyte) {
		for(i = 0; (i < HVRXBYTE_MAX) && (hvrxbyte[i] != 0); i++) {
			if(((hvrxbyte[i] & 0xF0) == 0x40) && (hvtxbyte > selecthvtxbyte))
				selecthvtxbyte = hvrxbyte[i];
		}
		if(selecthvtxbyte != 0)
			max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVTXBYTE, selecthvtxbyte);
	}

	return;
}

static void max77843_hv_muic_afc_control_ping
		(struct max77843_muic_data *muic_data, bool ping_continue)
{
	int ret;

	pr_info("%s:%s control ping[%d, %c]\n", MUIC_HV_DEV_NAME, __func__,
				muic_data->afc_count, ping_continue ? 'T' : 'F');

	if (ping_continue)
		ret = max77843_hv_muic_write_reg(muic_data->i2c, MAX77843_MUIC_REG_HVCONTROL2, 0x5B);
	else
		ret = max77843_hv_muic_write_reg(muic_data->i2c, MAX77843_MUIC_REG_HVCONTROL2, 0x03);

	if (ret) {
		pr_err("%s:%s cannot writing HVCONTROL2 reg(%d)\n",
				MUIC_HV_DEV_NAME, __func__, ret);
	}
}

static void max77843_hv_muic_qc_charger(struct max77843_muic_data *muic_data)
{
	struct i2c_client	*i2c = muic_data->i2c;
	int ret;
	u8 status3;

	ret = max77843_hv_muic_read_reg(i2c, MAX77843_MUIC_REG_STATUS3, &status3);
	if (ret) {
		pr_err("%s:%s cannot read STATUS3 reg(%d)\n", MUIC_HV_DEV_NAME,
						__func__, ret);
	}

	pr_info("%s:%s STATUS3:0x%02x qc_hv:%x\n", MUIC_HV_DEV_NAME, __func__,
						status3, muic_data->qc_hv);

	switch (muic_data->qc_hv) {
	case HV_SUPPORT_QC_9V:
		ret = max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1, 0x3D);
		break;
	case HV_SUPPORT_QC_12V:
		ret = max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1, 0x35);
		break;
	case HV_SUPPORT_QC_20V:
		ret = max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1, 0x3F);
		break;
	case HV_SUPPORT_QC_5V:
		ret = max77843_hv_muic_write_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1, 0x33);
		break;
	default:
		pr_err("%s:%s not support QC Charger\n", MUIC_HV_DEV_NAME, __func__);
		break;
	}

	if (ret) {
		pr_err("%s:%s cannot writing HVCONTROL2 reg(%d)\n", MUIC_HV_DEV_NAME,
			__func__, ret);
	}
}

static void max77843_hv_muic_after_qc_prepare(struct max77843_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	muic_data->is_qc_vb_settle = false;

	schedule_delayed_work(&muic_data->hv_muic_qc_vb_work, msecs_to_jiffies(300));
}

static void max77843_hv_muic_adcmode_switch
		(struct max77843_muic_data *muic_data, bool always_on)
{
	struct i2c_client	*i2c = muic_data->i2c;
	int ret;

	pr_info("%s:%s always_on:%c\n", MUIC_HV_DEV_NAME, __func__, (always_on ? 'T' : 'F'));

	if (always_on) {
#if !defined(CONFIG_SEC_FACTORY)
		max77843_muic_set_adcmode_always(muic_data);
#endif /* CONFIG_SEC_FACTORY */
		ret = max77843_muic_hv_update_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1,
					(MAX77843_ENABLE_BIT << HVCONTROL1_VBUSADCEN_SHIFT),
					HVCONTROL1_VBUSADCEN_MASK, true);
	} else {
#if !defined(CONFIG_SEC_FACTORY)
		max77843_muic_set_adcmode_oneshot(muic_data);
#endif /* CONFIG_SEC_FACTORY */
		/* non MAXIM */
		ret = max77843_muic_hv_update_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1,
					(MAX77843_DISABLE_BIT << HVCONTROL1_VBUSADCEN_SHIFT),
					HVCONTROL1_VBUSADCEN_MASK, true);
	}

	if (ret)
		pr_err("%s:%s cannot switch adcmode(%d)\n", MUIC_HV_DEV_NAME, __func__, ret);
}

static void max77843_hv_muic_adcmode_always_on(struct max77843_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	max77843_hv_muic_adcmode_switch(muic_data, true);
}

void max77843_hv_muic_adcmode_oneshot(struct max77843_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	max77843_hv_muic_adcmode_switch(muic_data, false);
}

static int max77843_hv_muic_handle_attach
		(struct max77843_muic_data *muic_data, const muic_afc_data_t *new_afc_data)
{
	int ret = 0;
	bool noti = true;
	muic_attached_dev_t	new_dev	= new_afc_data->new_dev;

	pr_info("%s:%s \n", MUIC_HV_DEV_NAME, __func__);

	if (muic_data->is_charger_ready == false) {
		if (new_afc_data->new_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC) {
			muic_data->is_afc_muic_prepare = true;
			pr_info("%s:%s is_charger_ready[%c], is_afc_muic_prepare[%c]\n",
				MUIC_HV_DEV_NAME, __func__,
				(muic_data->is_charger_ready ? 'T' : 'F'),
				(muic_data->is_afc_muic_prepare ? 'T' : 'F'));

			return ret;
		}
		pr_info("%s:%s is_charger_ready[%c], just return\n", MUIC_HV_DEV_NAME,
			__func__, (muic_data->is_charger_ready ? 'T' : 'F'));
		return ret;
	}

	switch (new_afc_data->function_num) {
	case FUNC_TA_TO_PREPARE:
		max77843_hv_muic_adcmode_always_on(muic_data);
		max77843_hv_muic_set_afc_after_prepare(muic_data);
		muic_data->afc_count = 0;
		muic_data->is_afc_handshaking = false;
		break;
	case FUNC_PREPARE_TO_PREPARE_DUPLI:
		muic_data->afc_count++;
		max77843_hv_muic_set_afc_charger_handshaking(muic_data);
		muic_data->is_afc_handshaking = true;
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_TO_AFC_5V:
		/* NEED ?? */
		if (!muic_data->is_afc_handshaking) {
			max77843_hv_muic_set_afc_charger_handshaking(muic_data);
			muic_data->is_afc_handshaking = true;
		}
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_TO_QC_PREPARE:
		/* ping STOP */
		ret = max77843_hv_muic_write_reg(muic_data->i2c, MAX77843_MUIC_REG_HVCONTROL2, 0x03);
		if (ret) {
			pr_err("%s:%s cannot writing HVCONTROL2 reg(%d)\n",
					MUIC_HV_DEV_NAME, __func__, ret);
		}
		max77843_hv_muic_qc_charger(muic_data);
		max77843_hv_muic_after_qc_prepare(muic_data);
		break;
	case FUNC_PREPARE_DUPLI_TO_PREPARE_DUPLI:
		muic_data->afc_count++;
		if (!muic_data->is_afc_handshaking) {
			max77843_hv_muic_set_afc_charger_handshaking(muic_data);
			muic_data->is_afc_handshaking = true;
		}
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_DUPLI_TO_AFC_5V:
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_DUPLI_TO_AFC_ERR_V:
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_DUPLI_TO_AFC_9V:
		max77843_hv_muic_afc_control_ping(muic_data, false);
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	case FUNC_PREPARE_DUPLI_TO_QC_PREPARE:
		max77843_hv_muic_qc_charger(muic_data);
		max77843_hv_muic_after_qc_prepare(muic_data);
		break;
	case FUNC_AFC_5V_TO_AFC_5V_DUPLI:
		muic_data->afc_count++;
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
			max77843_hv_muic_adcmode_always_on(muic_data);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_AFC_5V_TO_AFC_ERR_V:
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_AFC_5V_TO_AFC_9V:
		max77843_hv_muic_afc_control_ping(muic_data, false);
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	case FUNC_AFC_5V_TO_QC_PREPARE:
		max77843_hv_muic_qc_charger(muic_data);
		max77843_hv_muic_after_qc_prepare(muic_data);
		break;
	case FUNC_AFC_5V_DUPLI_TO_AFC_5V_DUPLI:
		muic_data->afc_count++;
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
			max77843_hv_muic_adcmode_always_on(muic_data);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_AFC_5V_DUPLI_TO_AFC_ERR_V:
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_AFC_5V_DUPLI_TO_AFC_9V:
		max77843_hv_muic_afc_control_ping(muic_data, false);
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	case FUNC_AFC_5V_DUPLI_TO_QC_PREPARE:
		max77843_hv_muic_qc_charger(muic_data);
		max77843_hv_muic_after_qc_prepare(muic_data);
		break;
	case FUNC_AFC_ERR_V_TO_AFC_ERR_V_DUPLI:
		muic_data->afc_count++;
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
			max77843_hv_muic_adcmode_always_on(muic_data);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_AFC_ERR_V_TO_AFC_9V:
		max77843_hv_muic_afc_control_ping(muic_data, false);
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	case FUNC_AFC_ERR_V_TO_QC_PREPARE:
		max77843_hv_muic_qc_charger(muic_data);
		max77843_hv_muic_after_qc_prepare(muic_data);
		break;
	case FUNC_AFC_ERR_V_DUPLI_TO_AFC_ERR_V_DUPLI:
		muic_data->afc_count++;
		if (muic_data->afc_count > AFC_CHARGER_WA_PING) {
			max77843_hv_muic_afc_control_ping(muic_data, false);
			max77843_hv_muic_adcmode_always_on(muic_data);
		} else {
			max77843_hv_muic_afc_control_ping(muic_data, true);
			noti = false;
		}
		break;
	case FUNC_AFC_ERR_V_DUPLI_TO_AFC_9V:
		max77843_hv_muic_afc_control_ping(muic_data, false);
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	case FUNC_AFC_ERR_V_DUPLI_TO_QC_PREPARE:
		max77843_hv_muic_qc_charger(muic_data);
		max77843_hv_muic_after_qc_prepare(muic_data);
		break;
	case FUNC_AFC_9V_TO_AFC_9V:
		max77843_hv_muic_afc_control_ping(muic_data, false);
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	case FUNC_QC_PREPARE_TO_QC_5V:
		if (muic_data->is_qc_vb_settle == true)
			max77843_hv_muic_adcmode_oneshot(muic_data);
		else
			noti = false;
		break;
	case FUNC_QC_PREPARE_TO_QC_9V:
		muic_data->is_qc_vb_settle = true;
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	case FUNC_QC_5V_TO_QC_5V:
		if (muic_data->is_qc_vb_settle == true)
			max77843_hv_muic_adcmode_oneshot(muic_data);
		else
			noti = false;
		break;
	case FUNC_QC_5V_TO_QC_9V:
		muic_data->is_qc_vb_settle = true;
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	case FUNC_QC_9V_TO_QC_9V:
		max77843_hv_muic_adcmode_oneshot(muic_data);
		break;
	default:
		pr_warn("%s:%s undefinded hv function num(%d)\n", MUIC_HV_DEV_NAME,
					__func__, new_afc_data->function_num);
		ret = -ESRCH;
		goto out;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (muic_data->attached_dev == new_dev)
		noti = false;
	else if (new_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC)
		noti = false;

	if (noti)
		muic_notifier_attach_attached_dev(new_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

	muic_data->attached_dev = new_dev;
out:
	return ret;
}

static bool muic_check_hv_irq
			(struct max77843_muic_data *muic_data,
			const muic_afc_data_t *tmp_afc_data, int irq)
{
	int afc_irq = 0;
	bool ret = false;

	/* change irq num to muic_afc_irq_t */
	if(irq == muic_data->irq_vdnmon)
		afc_irq = MUIC_AFC_IRQ_VDNMON;
	else if (irq == muic_data->irq_mrxrdy)
		afc_irq = MUIC_AFC_IRQ_MRXRDY;
	else if (irq == muic_data->irq_mpnack)
		afc_irq = MUIC_AFC_IRQ_MPNACK;
	else if (irq == muic_data->irq_vbadc)
		afc_irq = MUIC_AFC_IRQ_VBADC;
	else {
		pr_err("%s:%s cannot find irq #(%d)\n", MUIC_HV_DEV_NAME, __func__, irq);
		ret = false;
		goto out;
	}

	if (tmp_afc_data->afc_irq == afc_irq) {
		ret = true;
		goto out;
	}

	if (tmp_afc_data->afc_irq == MUIC_AFC_IRQ_DONTCARE) {
		ret = true;
		goto out;
	}

out:
	if (debug_en_checklist) {
		pr_info("%s:%s check_data dev(%d) irq(%d:%d) ret(%c)\n",
				MUIC_HV_DEV_NAME, __func__, tmp_afc_data->new_dev,
				tmp_afc_data->afc_irq, afc_irq, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_hvcontrol1_dpdnvden
			(const muic_afc_data_t *tmp_afc_data, u8 dpdnvden)
{
	bool ret = false;

	if (tmp_afc_data->hvcontrol1_dpdnvden == dpdnvden) {
		ret = true;
		goto out;
	}

	if (tmp_afc_data->hvcontrol1_dpdnvden == DPDNVDEN_DONTCARE) {
		ret = true;
		goto out;
	}

out:
	if (debug_en_checklist) {
		pr_info("%s:%s check_data dev(%d) dpdnvden(0x%x:0x%x) ret(%c)\n",
				MUIC_HV_DEV_NAME, __func__, tmp_afc_data->new_dev,
				tmp_afc_data->hvcontrol1_dpdnvden, dpdnvden,
				ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_status3_vbadc
			(const muic_afc_data_t *tmp_afc_data, u8 vbadc)
{
	bool ret = false;

	if (tmp_afc_data->status3_vbadc == vbadc) {
		ret = true;
		goto out;
	}

	if (tmp_afc_data->status3_vbadc == VBADC_AFC_5V) {
		switch (vbadc) {
		case VBADC_4V_5V:
		case VBADC_5V_6V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->status3_vbadc == VBADC_AFC_9V) {
		switch (vbadc) {
		case VBADC_8V_9V:
		case VBADC_9V_10V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->status3_vbadc == VBADC_AFC_ERR_V_NOT_0) {
		switch (vbadc) {
		case VBADC_6V_7V:
		case VBADC_7V_8V:
		case VBADC_10V_12V:
		case VBADC_12V_13V:
		case VBADC_13V_14V:
		case VBADC_14V_15V:
		case VBADC_15V_16V:
		case VBADC_16V_17V:
		case VBADC_17V_18V:
		case VBADC_18V_19V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->status3_vbadc == VBADC_AFC_ERR_V) {
		switch (vbadc) {
		case VBADC_VBDET:
		case VBADC_6V_7V:
		case VBADC_7V_8V:
		case VBADC_10V_12V:
		case VBADC_12V_13V:
		case VBADC_13V_14V:
		case VBADC_14V_15V:
		case VBADC_15V_16V:
		case VBADC_16V_17V:
		case VBADC_17V_18V:
		case VBADC_18V_19V:
		case VBADC_19V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->status3_vbadc == VBADC_QC_5V) {
		switch (vbadc) {
		case VBADC_4V_5V:
		case VBADC_5V_6V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->status3_vbadc == VBADC_QC_9V) {
		switch (vbadc) {
		case VBADC_6V_7V:
		case VBADC_7V_8V:
		case VBADC_8V_9V:
		case VBADC_9V_10V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->status3_vbadc == VBADC_ANY) {
		switch (vbadc) {
		case VBADC_4V_5V:
		case VBADC_5V_6V:
		case VBADC_6V_7V:
		case VBADC_7V_8V:
		case VBADC_8V_9V:
		case VBADC_9V_10V:
		case VBADC_10V_12V:
		case VBADC_12V_13V:
		case VBADC_13V_14V:
		case VBADC_14V_15V:
		case VBADC_15V_16V:
		case VBADC_16V_17V:
		case VBADC_17V_18V:
		case VBADC_18V_19V:
		case VBADC_19V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->status3_vbadc == VBADC_DONTCARE) {
		ret = true;
		goto out;
	}

out:
	if (debug_en_checklist) {
		pr_info("%s:%s check_data dev(%d) vbadc(0x%x:0x%x) ret(%c)\n",
				MUIC_HV_DEV_NAME, __func__, tmp_afc_data->new_dev,
				tmp_afc_data->status3_vbadc, vbadc, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_status3_vdnmon
			(const muic_afc_data_t *tmp_afc_data, u8 vdnmon)
{
	bool ret = false;

	if (tmp_afc_data->status3_vdnmon == vdnmon) {
		ret = true;
		goto out;
	}

	if (tmp_afc_data->status3_vdnmon == VDNMON_DONTCARE) {
		ret = true;
		goto out;
	}

out:
	if (debug_en_checklist) {
		pr_info("%s:%s check_data dev(%d) vdnmon(0x%x:0x%x) ret(%c)\n",
				MUIC_HV_DEV_NAME, __func__, tmp_afc_data->new_dev,
				tmp_afc_data->status3_vdnmon, vdnmon, ret ? 'T' : 'F');
	}

	return ret;
}

bool muic_check_dev_ta(struct max77843_muic_data *muic_data)
{
	u8 status1 = muic_data->status1;
	u8 status2 = muic_data->status2;
	u8 adc, vbvolt, chgdetrun, chgtyp;

	adc = status1 & STATUS1_ADC_MASK;
	vbvolt = status2 & STATUS2_VBVOLT_MASK;
	chgdetrun = status2 & STATUS2_CHGDETRUN_MASK;
	chgtyp = status2 & STATUS2_CHGTYP_MASK;

	if (adc != ADC_OPEN) {
		max77843_muic_set_afc_ready(muic_data, false);
		return false;
	}
	if (vbvolt == VB_LOW || chgdetrun == CHGDETRUN_TRUE || chgtyp != CHGTYP_DEDICATED_CHARGER) {
		max77843_muic_set_afc_ready(muic_data, false);
#if defined(CONFIG_MUIC_NOTIFIER)
		muic_notifier_detach_attached_dev(muic_data->attached_dev);
#endif
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		return false;
	}

	return true;
}

static void max77843_hv_muic_detect_dev(struct max77843_muic_data *muic_data, int irq)
{
	struct i2c_client *i2c = muic_data->i2c;
	const muic_afc_data_t *tmp_afc_data = afc_condition_checklist[muic_data->attached_dev];

	int intr = MUIC_INTR_DETACH;
	int ret;
	int i;
	u8 status[3];
	u8 hvcontrol[2];
	u8 vdnmon, dpdnvden, mpnack, vbadc;
	bool flag_next = true;
	bool muic_dev_ta = false;

	pr_info("%s:%s irq(%d)\n", MUIC_HV_DEV_NAME, __func__, irq);

	if (tmp_afc_data == NULL) {
		pr_info("%s:%s non AFC Charger, just return!\n", MUIC_HV_DEV_NAME, __func__);
		return;
	}

	ret = max77843_bulk_read(muic_data->i2c, MAX77843_MUIC_REG_STATUS1, 3, status);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME, __func__, ret);
		return;
	}

	pr_info("%s:%s STATUS1:0x%02x, 2:0x%02x, 3:0x%02x\n", MUIC_DEV_NAME, __func__,
					status[0], status[1], status[2]);

	/* attached status */
	muic_data->status1 = status[0];
	muic_data->status2 = status[1];
	muic_data->status3 = status[2];

	/* check TA type */
	muic_dev_ta = muic_check_dev_ta(muic_data);
	if (!muic_dev_ta) {
		pr_err("%s:%s device type is not TA!\n", MUIC_HV_DEV_NAME, __func__);
		return;
	}

	/* attached status */
	mpnack = status[2] & STATUS3_MPNACK_MASK;
	vdnmon = status[2] & STATUS3_VDNMON_MASK;
	vbadc = status[2] & STATUS3_VBADC_MASK;

	ret = max77843_bulk_read(i2c, MAX77843_MUIC_REG_HVCONTROL1, 2, hvcontrol);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_HV_DEV_NAME,
				__func__, ret);
		return;
	}

	pr_info("%s:%s HVCONTROL1:0x%02x, 2:0x%02x\n", MUIC_HV_DEV_NAME, __func__,
			hvcontrol[0], hvcontrol[1]);

	/* attached - control */
	muic_data->hvcontrol1 = hvcontrol[0];
	muic_data->hvcontrol2 = hvcontrol[1];

	dpdnvden = hvcontrol[0] & HVCONTROL1_DPDNVDEN_MASK;

	pr_info("%s:%s vdnmon:0x%x mpnack:0x%x vbadc:0x%x dpdnvden:0x%x\n",
		MUIC_HV_DEV_NAME, __func__, vdnmon, mpnack, vbadc, dpdnvden);

	for (i = 0; i < ATTACHED_DEV_NUM; i++, tmp_afc_data = tmp_afc_data->next) {

		if (!flag_next) {
			pr_info("%s:%s not found new_dev in afc_condition_checklist\n",
				MUIC_HV_DEV_NAME, __func__);
			break;
		}

		if (tmp_afc_data->next == tmp_afc_data)
			flag_next = false;

		if (!(muic_check_hv_irq(muic_data, tmp_afc_data, irq)))
			continue;

		if (!(muic_check_hvcontrol1_dpdnvden(tmp_afc_data, dpdnvden)))
			continue;

		if (!(muic_check_status3_vbadc(tmp_afc_data, vbadc)))
			continue;

		if(!(muic_check_status3_vdnmon(tmp_afc_data, vdnmon)))
			continue;

		pr_info("%s:%s checklist match found at i(%d), %s(%d)\n",
			MUIC_HV_DEV_NAME, __func__, i, tmp_afc_data->afc_name,
			tmp_afc_data->new_dev);

		intr = MUIC_INTR_ATTACH;

		break;
	}

	if (intr == MUIC_INTR_ATTACH) {
		pr_info("%s:%s AFC ATTACHED\n", MUIC_HV_DEV_NAME, __func__);
		pr_info("%s:%s %d->%d\n", MUIC_HV_DEV_NAME, __func__,
				muic_data->attached_dev, tmp_afc_data->new_dev);
		ret = max77843_hv_muic_handle_attach(muic_data, tmp_afc_data);
		if (ret)
			pr_err("%s:%s cannot handle attach(%d)\n", MUIC_HV_DEV_NAME,
				__func__, ret);
	} else {
		pr_info("%s:%s AFC MAINTAIN (%d)\n", MUIC_HV_DEV_NAME, __func__,
				muic_data->attached_dev);
		ret = max77843_hv_muic_state_maintain(muic_data);
		if (ret)
			pr_err("%s:%s cannot maintain state(%d)\n", MUIC_HV_DEV_NAME,
				__func__, ret);
		goto out;
	}

out:
	return;
}

/* TA setting in max77843-muic.c */
void max77843_muic_prepare_afc_charger(struct max77843_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret;

	pr_info("%s:%s \n", MUIC_DEV_NAME, __func__);

	max77843_hv_muic_adcmode_oneshot(muic_data);

	/* set HVCONTROL1=0x11 */
	ret = max77843_muic_hv_update_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1,
			(0x2 << HVCONTROL1_DPVD_SHIFT), HVCONTROL1_DPVD_MASK, true);
	if (ret)
		goto err_write;

	ret = max77843_muic_hv_update_reg(i2c, MAX77843_MUIC_REG_HVCONTROL1,
			(MAX77843_ENABLE_BIT << HVCONTROL1_DPDNVDEN_SHIFT),
			HVCONTROL1_DPDNVDEN_MASK, true);
	if (ret)
		goto err_write;

	/* Set VBusADCEn = 1 after the time of changing adcmode*/

	max77843_muic_set_afc_ready(muic_data, true);

	return;

err_write:
	pr_err("%s:%s fail to write muic reg(%d)\n", MUIC_DEV_NAME, __func__, ret);
	return;
}

/* TA setting in max77843-muic.c */
bool max77843_muic_check_change_dev_afc_charger
		(struct max77843_muic_data *muic_data, muic_attached_dev_t new_dev)
{
	bool ret = true;

	if (new_dev == ATTACHED_DEV_TA_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_5V_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_9V_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC || \
		new_dev == ATTACHED_DEV_QC_CHARGER_5V_MUIC || \
		new_dev == ATTACHED_DEV_QC_CHARGER_9V_MUIC) {
		if(muic_check_dev_ta(muic_data)) {
			ret = false;
		}
	}

	return ret;
}

static void max77843_hv_muic_detect_after_charger_init(struct work_struct *work)
{
	struct afc_init_data_s *init_data =
	    container_of(work, struct afc_init_data_s, muic_afc_init_work);
	struct max77843_muic_data *muic_data = init_data->muic_data;
	int ret;
	u8 status3;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);

	/* check vdnmon status value */
	ret = max77843_read_reg(muic_data->i2c, MAX77843_MUIC_REG_STATUS3, &status3);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_HV_DEV_NAME,
				__func__, ret);
		return;
	}
	pr_info("%s:%s STATUS3:0x%02x\n", MUIC_HV_DEV_NAME, __func__, status3);

	if (muic_data->is_afc_muic_ready) {
		if (muic_data->is_afc_muic_prepare)
			max77843_hv_muic_detect_dev(muic_data, muic_data->irq_vdnmon);
		else
			max77843_hv_muic_detect_dev(muic_data, -1);
	}

	mutex_unlock(&muic_data->muic_mutex);
}

void max77843_hv_muic_charger_init(void)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	if(afc_init_data.muic_data) {
		afc_init_data.muic_data->is_charger_ready = true;
		schedule_work(&afc_init_data.muic_afc_init_work);
	}
}

static void max77843_hv_muic_check_qc_vb(struct work_struct *work)
{
	struct max77843_muic_data *muic_data =
	    container_of(work, struct max77843_muic_data, hv_muic_qc_vb_work.work);
	u8 status3, vbadc;

	if (!muic_data) {
		pr_err("%s:%s cannot read muic_data!\n", MUIC_HV_DEV_NAME, __func__);
		return;
	}

	mutex_lock(&muic_data->muic_mutex);

	if (muic_data->is_qc_vb_settle == true) {
		pr_info("%s:%s already qc vb settled\n", MUIC_HV_DEV_NAME, __func__);
		goto out;
	}

	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	max77843_hv_muic_read_reg(muic_data->i2c, MAX77843_MUIC_REG_STATUS3, &status3);
	vbadc = status3 & STATUS3_VBADC_MASK;

	if (vbadc == VBADC_4V_5V || vbadc == VBADC_5V_6V) {
		muic_data->is_qc_vb_settle = true;
		max77843_hv_muic_detect_dev(muic_data, muic_data->irq_vbadc);
	}

out:
	mutex_unlock(&muic_data->muic_mutex);
	return;
}

void max77843_hv_muic_init_detect(struct max77843_muic_data *muic_data)
{
	int ret;
	u8 status3, vdnmon;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);

	if (muic_data->is_boot_dpdnvden == DPDNVDEN_ENABLE)
		pr_info("%s:%s dpdnvden already ENABLE\n", MUIC_HV_DEV_NAME, __func__);
	else if (muic_data->is_boot_dpdnvden == DPDNVDEN_DISABLE) {
		mdelay(30);
		pr_info("%s:%s dpdnvden == DISABLE, 30ms delay\n", MUIC_HV_DEV_NAME, __func__);
	} else {
		pr_err("%s:%s dpdnvden is not correct(0x%x)!\n", MUIC_HV_DEV_NAME,
			__func__, muic_data->is_boot_dpdnvden);
		goto out;
	}

	ret = max77843_read_reg(muic_data->i2c, MAX77843_MUIC_REG_STATUS3, &status3);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME, __func__, ret);
		vdnmon = VDNMON_DONTCARE;
	} else
		vdnmon = status3 & STATUS3_VDNMON_MASK;

	if (vdnmon == VDNMON_LOW)
		max77843_hv_muic_detect_dev(muic_data, muic_data->irq_vdnmon);
	else
		pr_info("%s:%s vdnmon != LOW(0x%x)\n", MUIC_HV_DEV_NAME, __func__, vdnmon);

out:
	mutex_unlock(&muic_data->muic_mutex);
}

void max77843_hv_muic_init_check_dpdnvden(struct max77843_muic_data *muic_data)
{
	u8 hvcontrol1;
	int ret;

	mutex_lock(&muic_data->muic_mutex);

	ret = max77843_hv_muic_read_reg(muic_data->i2c, MAX77843_MUIC_REG_HVCONTROL1, &hvcontrol1);
	if (ret) {
		pr_err("%s:%s cannot read HVCONTROL1 reg!\n", MUIC_HV_DEV_NAME, __func__);
		muic_data->is_boot_dpdnvden = DPDNVDEN_DONTCARE;
	} else
		muic_data->is_boot_dpdnvden = hvcontrol1 & HVCONTROL1_DPDNVDEN_MASK;

	mutex_unlock(&muic_data->muic_mutex);
}

static irqreturn_t max77843_muic_hv_irq(int irq, void *data)
{
	struct max77843_muic_data *muic_data = data;
	pr_info("%s:%s irq:%d\n", MUIC_HV_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == false)
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_HV_DEV_NAME,
			__func__);
	else if (muic_data->is_afc_muic_ready == false)
		pr_info("%s:%s not ready yet(afc_muic_ready[%c])\n", MUIC_HV_DEV_NAME,
			 __func__, (muic_data->is_afc_muic_ready ? 'T' : 'F'));
	else if (muic_data->is_charger_ready == false && irq != muic_data->irq_vdnmon)
		pr_info("%s:%s not ready yet(charger_ready[%c])\n", MUIC_HV_DEV_NAME,
			__func__, (muic_data->is_charger_ready ? 'T' : 'F'));
	else if (muic_data->pdata->afc_disable)
		pr_info("%s:%s AFC disable by USER (afc_disable[%c]\n", MUIC_HV_DEV_NAME,
			__func__, (muic_data->pdata->afc_disable ? 'T' : 'F'));
	else
		max77843_hv_muic_detect_dev(muic_data, irq);

	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

#define REQUEST_HV_IRQ(_irq, _dev_id, _name)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, max77843_muic_hv_irq,	\
				IRQF_NO_SUSPEND, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
				MUIC_HV_DEV_NAME, __func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

int max77843_afc_muic_irq_init(struct max77843_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	if (muic_data->mfd_pdata && (muic_data->mfd_pdata->irq_base > 0)) {
		int irq_base = muic_data->mfd_pdata->irq_base;

		/* request AFC MUIC IRQ */
		muic_data->irq_vdnmon = irq_base + MAX77843_MUIC_IRQ_INT3_VDNMON;
		REQUEST_HV_IRQ(muic_data->irq_vdnmon, muic_data, "muic-vdnmon");
		muic_data->irq_mrxrdy = irq_base + MAX77843_MUIC_IRQ_MRXRDY;
		REQUEST_HV_IRQ(muic_data->irq_mrxrdy, muic_data, "muic-mrxrdy");
		muic_data->irq_mpnack = irq_base + MAX77843_MUIC_IRQ_INT3_MPNACK;
		REQUEST_HV_IRQ(muic_data->irq_mpnack, muic_data, "muic-mpnack");
		muic_data->irq_vbadc = irq_base + MAX77843_MUIC_IRQ_INT3_VBADC;
		REQUEST_HV_IRQ(muic_data->irq_vbadc, muic_data, "muic-vbadc");

		pr_info("%s:%s vdnmon(%d), mrxrdy(%d), mpnack(%d), vbadc(%d)\n",
				MUIC_HV_DEV_NAME, __func__,
				muic_data->irq_vdnmon, muic_data->irq_mrxrdy,
				muic_data->irq_mpnack, muic_data->irq_vbadc);
	}

	return ret;
}

#define FREE_HV_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s:%s IRQ(%d):%s free done\n", MUIC_HV_DEV_NAME,	\
				__func__, _irq, _name);			\
	}								\
} while (0)

void max77843_hv_muic_free_irqs(struct max77843_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	/* free MUIC IRQ */
	FREE_HV_IRQ(muic_data->irq_vdnmon, muic_data, "muic-vdnmon");
	FREE_HV_IRQ(muic_data->irq_mrxrdy, muic_data, "muic-mrxrdy");
	FREE_HV_IRQ(muic_data->irq_mpnack, muic_data, "muic-mpnack");
	FREE_HV_IRQ(muic_data->irq_vbadc, muic_data, "muic-vbadc");
}

#if defined(CONFIG_OF)
int of_max77843_hv_muic_dt(struct max77843_muic_data *muic_data)
{
	struct device_node *np_muic;
	int ret = 0;

	np_muic = of_find_node_by_path("/muic");
	if (np_muic == NULL)
		return -EINVAL;

	ret = of_property_read_u8(np_muic, "muic,qc-hv", &muic_data->qc_hv);
	if (ret) {
		pr_err("%s:%s There is no Property of muic,qc-hv\n",
				MUIC_DEV_NAME, __func__);
		goto err;
	}

	pr_info("%s:%s muic_data->qc-hv:0x%02x\n", MUIC_DEV_NAME, __func__,
				muic_data->qc_hv);

err:
	of_node_put(np_muic);

	return ret;
}
#endif /* CONFIG_OF */

void max77843_hv_muic_initialize(struct max77843_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	muic_data->is_afc_handshaking = false;
	muic_data->is_afc_muic_prepare = false;
	muic_data->is_charger_ready = false;
	muic_data->is_boot_dpdnvden = DPDNVDEN_DONTCARE;

	afc_init_data.muic_data = muic_data;
	INIT_WORK(&afc_init_data.muic_afc_init_work, max77843_hv_muic_detect_after_charger_init);

	INIT_DELAYED_WORK(&muic_data->hv_muic_qc_vb_work, max77843_hv_muic_check_qc_vb);
}

void max77843_hv_muic_remove(struct max77843_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	cancel_work_sync(&afc_init_data.muic_afc_init_work);
	cancel_delayed_work_sync(&muic_data->hv_muic_qc_vb_work);

	max77843_hv_muic_free_irqs(muic_data);
}

void max77843_hv_muic_remove_wo_free_irq(struct max77843_muic_data *muic_data)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	cancel_work_sync(&afc_init_data.muic_afc_init_work);
	cancel_delayed_work_sync(&muic_data->hv_muic_qc_vb_work);
}

