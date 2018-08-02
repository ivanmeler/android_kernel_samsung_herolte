/* linux/drivers/video/exynos_decon/panel/smart_dimming_core.h
 *
 * Header file for Samsung AID Dimming Driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include "../dsim.h"
#include "smart_dimming_core.h"
#include "s6e3fa3_fhd_aid_dimming.h"
//#define TEST

#ifdef TEST

int e_center_gamma[VREF_POINT_CNT][CI_MAX] = {
	{0, 	0, 		0},
	{0x80,	0x80,	0x80},
	{0x80,	0x80,	0x80},
	{0x80,	0x80,	0x80},
	{0x80,	0x80,	0x80},
	{0x80,	0x80,	0x80},
	{0x80, 	0x80,	0x80},
	{0x80,	0x80,	0x80},
	{0x80, 	0x80,	0x80},
	{0x100, 0x100,	0x100},
};

int e_mtp[VREF_POINT_CNT][CI_MAX] = {
	{0, 0, 0},
	{0,	0, 0},
	{0,	0, 0},
	{0,	0, 0},
	{0,	0, 0},
	{0,	0, 0},
	{0, 0, 0},
	{0,	0, 0},
	{0, 0, 0},
	{0, 0, 0},
};

int e_vt_mtp[CI_MAX] = {
	2, 3, 2
};

int pre_condition(struct dimming_param *dim_param)
{
	int i, j;
	int ret = 0;

	for (i = 0; i < VREF_POINT_CNT; i++) {
		for (j = 0; j < CI_MAX; j++) {
			dim_param->center_gma[i][j] = e_center_gamma[i][j];
			dim_param->mtp[i][j] = e_mtp[i][j];
		}
	}

	for (j = 0; j < CI_MAX; j++) {
		dim_param->vt_mtp[j] = e_vt_mtp[j];
	}
	return ret;
}
#endif

char *color_str[CI_MAX] = {
	"R",
 	"G",
	"B",
};


int print_voltage_table(struct volt_info *voltage)
{
	int i, j;

#ifdef TEST
	float volt;
	LOG("\n====================== VT TABLE ======================\n");
	for (j = 0; j < CI_MAX; j++) {
		LOG("<%s : %d:%f>, ", color_str[j], voltage->vt[j], (float)voltage->vt[j]/(MULTIPLY_VALUE*MULTIPLY_VALUE));
	}

	LOG("\n\n===================== VREG TABLE =====================\n");
	for (i = 0; i < MAX_GRAYSCALE; i++) {
		LOG("VREG : %3d : ", i);
		for (j = 0; j < CI_MAX; j++) {
			LOG("< %s : %d : %f >, ", color_str[j], voltage->volt[i][j], (float)voltage->volt[i][j]/(MULTIPLY_VALUE*MULTIPLY_VALUE));
		}
		LOG("\n");
	}
#else

	LOG("\n====================== VT TABLE ======================\n");
	for (j = 0; j < CI_MAX; j++) {
		printk("%s : %d", color_str[j], voltage->vt[j]);
	}

	LOG("\n\n===================== VREG TABLE =====================\n");
	for (i = 0; i < MAX_GRAYSCALE; i++) {
		LOG("VREG : %3d : ", i);
		for (j = 0; j < CI_MAX; j++) {
			printk("<%s:%d>, ", color_str[j], voltage->volt[i][j]);
		}
		printk("\n");
	}
#endif

	return 0;
}


int vref_calc_method_direct(struct dimming_param *dim_param, struct volt_info *voltage, int index)
{
	int i,tmp;
	int r_index = vref_index_list[index];
	unsigned int *volt_tbl = (unsigned int*)tbl_tranform_volt_list[index];

	if (volt_tbl == NULL) {
		LOG("ERR:%s:failed to assign volt_tbl : NULL\n", __func__);
		goto method_direct_err;
	}

	for (i = 0; i < CI_MAX; i++) {
		tmp = dim_param->ref_gma[index][i];
		voltage->volt[r_index][i] = volt_tbl[tmp];
	}
	return 0;

method_direct_err:
	return -1;
}

int vref_calc_mehtod_indirect(struct dimming_param *dim_param, struct volt_info *voltage, int index)
{
	int i;
	int gamma;
	unsigned int ref, idx;
	int ref_idx, ref_volt;
	unsigned long volt;
	unsigned int *volt_tbl = (unsigned int *)tbl_tranform_volt_list[index];

	idx = vref_index_list[index];
	ref_idx = vref_index_list[index+1];

	if (volt_tbl == NULL) {
		LOG("ERR:%s:failed to assign volt_tbl : NULL\n", __func__);
		goto method_direct_err;
	}

	for (i = 0; i < CI_MAX; i++) {
		gamma = dim_param->ref_gma[index][i];

		if (vref_cal_ref[index] == VREF_CAL_REF_VT)
			ref = voltage->vt[i];
		else if (vref_cal_ref[index] == VREF_CAL_REF_VREG)
			ref = MULTIPLE_VREGOUT * MULTIPLY_VALUE;

		ref_volt = voltage->volt[ref_idx][i];


		volt = (unsigned long)(ref - ref_volt) * (unsigned long)volt_tbl[gamma];

		voltage->volt[idx][i] = ref - (unsigned int)(volt >> 10);
	}
	return 0;

method_direct_err:
	return -1;
}

int vref_calc_method_fix(struct dimming_param *dim_param, struct volt_info *voltage, int index)
{
	int i;
	int r_index = vref_index_list[index];
	unsigned int *volt_tbl = (unsigned int *)tbl_tranform_volt_list[index];

	if (volt_tbl == NULL) {
		LOG("ERR:%s:failed to assign volt_tbl : NULL\n", __func__);
		goto method_fix_err;
	}

	for (i = 0; i < CI_MAX; i++) {
		voltage->volt[r_index][i] = volt_tbl[0];
	}

	return 0;

method_fix_err:
	return -1;
}


int calculate_voltage_tbl(struct dimming_param *dim_param, struct volt_info *voltage)
{
	int ret;
	int i, j, seq;

	int vref_idx = 0;
	int preidx, nextidx, refidx;
	unsigned int prevolt, nextvolt;
	unsigned long tmp;
	int method = 0;
	unsigned char cal_seq[VREF_POINT_CNT] = {0, };
	int (*calc_vref_fn[VREF_CALC_METHOD_CNT])(struct dimming_param *, struct volt_info *, int) = {
		vref_calc_method_direct,
		vref_calc_mehtod_indirect,
		vref_calc_method_fix,
	};

	LOG("%s\n", __func__);

	/*make tbl for calcualte sequnece*/
	cal_seq[0] = 0;
	for(i = 1; i < VREF_POINT_CNT; i++)
		cal_seq[i] = VREF_POINT_CNT - i;

	/* get vt voltage */
	for (i = 0 ; i < CI_MAX; i++) {
		voltage->vt[i] = tbl_vt_transform_volt[dim_param->vt_mtp[i]];
		LOG("VT[%d] : %d\n", i, voltage->vt[i]);
	}
	/* get vt reference point */
	for (i = 0; i < VREF_POINT_CNT; i++) {
		seq = cal_seq[i];
		method = vref_calc_method[seq];

		if (method >= VREF_CALC_METHOD_CNT) {
			LOG("ERR:%s:undefined vref calcualte method\n", __func__);
			goto calculate_fail;
		}
		ret = calc_vref_fn[method](dim_param, voltage, seq);
	}
	/*get voltage value using interpoaltion */
	for (i = 0; i < MAX_GRAYSCALE; i++) {
		if (i == vref_index_list[vref_idx]) {
			vref_idx++;
			continue;
		}
		nextidx = vref_index_list[vref_idx];
		preidx = vref_index_list[vref_idx - 1];

		refidx = i - preidx - 1;
		for (j = 0; j < CI_MAX; j++) {
			prevolt = voltage->volt[preidx][j];
			nextvolt = voltage->volt[nextidx][j];
			tmp = (unsigned long)(prevolt - nextvolt) * (unsigned long)(tbl_vref0_vref255_volt[vref_idx-1][refidx]);
			voltage->volt[i][j] = (unsigned int)nextvolt + (unsigned int)(tmp >> 10);
		}
	}
	return 0;

calculate_fail:
	return -1;
}

static int lookup_volt_index(int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray >> 20;
	index = (int)voltage_lookup_tbl[temp];
	exit = 1;
	i = 0;
	while(exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_BRIGHTNESS)
			index_h = MAX_BRIGHTNESS;
		cnt_l = (int)voltage_lookup_tbl[index] - (int)voltage_lookup_tbl[index_l];
		cnt_h = (int)voltage_lookup_tbl[index_h] - (int)voltage_lookup_tbl[index];

		if (cnt_l + cnt_h) {
			exit = 0;
		}
		i++;
	}

	p_delta = 0;
	index = (int)voltage_lookup_tbl[index_l];
	ret = index;

	temp = lookup_gamma_tbl[index] << 10;

	if (gray > temp)
		p_delta = gray - temp;
	else
		p_delta = temp - gray;

	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = lookup_gamma_tbl[index + i] << 10;
		if (gray > temp)
			delta = gray - temp;
		else
			delta = temp - gray;

		if (delta < p_delta) {
			p_delta = delta;
			ret = index + i;
		}
	}
	return ret;
}


int gamma_transform_reg_fix(struct volt_info *voltage, int idx, int ci)
{
	int ret = 0;

	return ret;
}

int gamma_transform_reg_indirect(struct volt_info *voltage, int idx, int ci)
{
	int ref_volt;
	int gap1, gap2;
	unsigned long tmp;

	if (vref_cal_ref[idx] == VREF_CAL_REF_VREG)
		ref_volt = MULTIPLE_VREGOUT * MULTIPLY_VALUE;
	else
		ref_volt = voltage->vt[ci];

	gap1 = ref_volt - voltage->tmpv[idx][ci];
	gap2 = ref_volt - voltage->tmpv[idx+1][ci];

	tmp = ((unsigned long)gap1 * (unsigned long)formula_info[idx].deno) / (unsigned long)gap2;

	return (unsigned int)tmp - formula_info[idx].num1;

}

int gamma_transform_reg_direct(struct volt_info *voltage, int idx, int ci)
{
	int gap;
	int ref_volt;
	unsigned long tmp;

	ref_volt = MULTIPLE_VREGOUT * MULTIPLY_VALUE;

	gap = ref_volt - voltage->tmpv[idx][ci];

	tmp = ((unsigned long)gap * (unsigned long)formula_info[idx].deno) / (unsigned long)ref_volt;

	return (unsigned int)tmp - formula_info[idx].num1;
}


int calculate_gamma_tbl(struct dimming_param *dim_param, struct volt_info *voltage, int ddx, char *result)
{
	int i, j, k;
	//int ret = 0;
	int br, gray;
	int method, tmp;
	int vref, idx, r_shift, c_shift;
	unsigned int *ref_gamma = NULL;
	unsigned int *ref_shift = NULL;
	unsigned int *clr_shift = NULL;
	//int volt_list[VREF_POINT_CNT][CI_MAX] = {0, ..};
	int gamma_reg[VREF_POINT_CNT][CI_MAX];
	int (*calc_gamma_reg[VREF_CALC_METHOD_CNT])(struct volt_info *, int, int) = {
		gamma_transform_reg_direct,
		gamma_transform_reg_indirect,
		gamma_transform_reg_fix,
	};

	if (ddx == MAX_DIMMING_INFO_COUNT - 1) {
		for (j = 0; j < VREF_POINT_CNT; j++) {
			for (k = 0 ; k < CI_MAX; k++) {
				gamma_reg[j][k] = dim_param->center_gma[j][k];
			}
		}
		goto format_gamma;
	}

	br = aid_info[ddx].ref_br;
	ref_gamma = (unsigned int *)aid_info[ddx].ref_gma;
	ref_shift = (unsigned int *)ref_shift_tbl[ddx];
	clr_shift = (unsigned int *)color_shift_tbl[ddx];

	for (j = 1; j < VREF_POINT_CNT; j++) {
		r_shift = 0;
		vref = vref_index_list[j];
		gray = ref_gamma[vref] * br;

		if (ref_shift != NULL)
			r_shift = ref_shift[j];

		idx = lookup_volt_index(gray) + r_shift;
		for (k = 0 ; k < CI_MAX; k++) {
			voltage->tmpv[j][k] = voltage->volt[idx][k];
		}
	}

	for(j = 0; j < VREF_POINT_CNT; j++) {
		method = vref_calc_method[j];
		for (k = 0; k < CI_MAX; k++) {
			c_shift = 0;
			if (clr_shift != NULL) {
				idx = (j * CI_MAX) + k;
				c_shift = clr_shift[idx];
			}
			tmp = calc_gamma_reg[method](voltage, j, k);
			gamma_reg[j][k] = (tmp + c_shift) - dim_param->mtp[j][k];
		}
	}
format_gamma:
	idx = 0;
	result[idx++] = GAMMA_COMMAND;
	for (i = VREF_POINT_CNT-1; i >= 0; i--) {
		for (j = 0 ; j < CI_MAX; j++) {
			if (vref_element_max[i] > 255) {
				result[idx++] = (gamma_reg[i][j] & 0xff00) >> 8;
				result[idx++] = (gamma_reg[i][j] & 0x00ff);
			} else {
				result[idx++] = gamma_reg[i][j] & 0xff;
			}
		}
	}
	result[idx++] = ((dim_param->vt_mtp[CI_RED] & 0x0f) << 4) |
			(dim_param->vt_mtp[CI_GREEN] & 0x0f);
	result[idx++] = (dim_param->vt_mtp[CI_BLUE] & 0x0f);

	return 0;
}

#ifdef TEST
int main(int argc, char *argv[])
{
	int i, j;
	int ret = 0;

	struct volt_info voltage;
	struct dimming_param dim_param;
	struct gamma_cmd *cmd;

	cmd = malloc(sizeof(struct gamma_cmd) * MAX_DIMMING_INFO_COUNT);
	if (cmd == NULL) {
		LOG("%s:failed to malloc for struct gamma_cmd\n", __func__);
		return -1;
	}

	memset(&dim_param, 0, sizeof(struct dimming_param));
	memset(&voltage, 0, sizeof(struct volt_info));

	/*pre_condition function - need to replace to real function */
	pre_condition(&dim_param);

	for (i = 0; i < VREF_POINT_CNT; i++) {
		for (j = 0; j < CI_MAX; j++) {
			dim_param.ref_gma[i][j] = dim_param.center_gma[i][j] + dim_param.mtp[i][j];
		}
	}

	for (j = 0; j < CI_MAX; j++) {
		dim_param.vt_mtp[j] = e_vt_mtp[j];
	}

	calculate_voltage_tbl(&dim_param, &voltage);

	print_voltage_table(&voltage);

	for (i = 0; i < MAX_DIMMING_INFO_COUNT; i ++)
		calculate_gamma_tbl(&dim_param, &voltage, i, cmd[i].gamma);

	for (i = 0; i < MAX_DIMMING_INFO_COUNT; i ++) {
		for (j = 0; j < GAMMA_COMMAND_CNT; j++) {
			LOG("%x ", cmd[i].gamma[j]);
		}
		LOG("\n");
	}

	return ret;

}

#else

struct dimming_param dim_param;

int init_smart_dimming(struct panel_info *command, char *refgamma, char *mtp, struct aid_dimming_info *aid)
{
	int i, j;
	int idx = 0;
	int ret = 0;
	struct gamma_cmd *cmd;
	struct volt_info *voltage;

	LOG("%s\n", __func__);

	cmd = kmalloc(sizeof(struct gamma_cmd) * MAX_DIMMING_INFO_COUNT, GFP_KERNEL);
	if (cmd == NULL) {
		LOG("%s:failed to malloc for struct gamma_cmd\n", __func__);
		return -1;
	}

	voltage = kmalloc(sizeof(struct volt_info), GFP_KERNEL);
	if (voltage == NULL) {
		LOG("%s:failed to malloc for struct gamma_cmd\n", __func__);
		return -1;
	}
	command->gamma_cmd = cmd;
	command->aid_info = (struct aid_dimming_info *)aid_info;


	memset(&dim_param, 0, sizeof(struct dimming_param));
	memset(voltage, 0, sizeof(struct volt_info));

	idx = 0;
	for (i = VREF_POINT_CNT-1; i >= 0 ; i--) {
		for (j = 0 ; j < CI_MAX; j++) {
			if (vref_element_max[i] > 255) {
				dim_param.center_gma[i][j] = (int)(refgamma[idx] << 8) | refgamma[idx+1];
				//printk("(%d)i:%d,j:%d %d, %d, %d\n", idx, i, j,dim_param.center_gma[i][j], refgamma[idx], refgamma[idx+1]);
				if (mtp[idx] & 0x01)
					dim_param.mtp[i][j] = (mtp[idx+1] * -1);
				else
					dim_param.mtp[i][j] = mtp[idx+1];
				idx += 2;
			} else {
				dim_param.center_gma[i][j] = (int)refgamma[idx];
				if (dim_param.mtp[i][j] & 0x80)
					dim_param.mtp[i][j] = (int)((mtp[idx] & 0x7f) * -1);
				else
					dim_param.mtp[i][j] = (int)(mtp[idx] & 0x7f);
				idx++;
			}
			dim_param.ref_gma[i][j] = dim_param.center_gma[i][j] + dim_param.mtp[i][j];
		}
	}
	dim_param.vt_mtp[CI_RED] = mtp[idx] >> 4;
	dim_param.vt_mtp[CI_GREEN] = mtp[idx++] & 0x0f;
	dim_param.vt_mtp[CI_BLUE] = mtp[idx] & 0x0f;

	memcpy(command->vt_mtp, dim_param.vt_mtp, sizeof(command->vt_mtp));
	memcpy(command->mtp, dim_param.mtp, sizeof(command->mtp));

	for(i=0;i<VREF_POINT_CNT;i++)
		printk("<%d %d %d>\n", dim_param.center_gma[i][0], dim_param.center_gma[i][1], dim_param.center_gma[i][2]);


	for(i=0;i<VREF_POINT_CNT;i++)
		printk("<%d %d %d>\n", dim_param.mtp[i][0], dim_param.mtp[i][1], dim_param.mtp[i][2]);

	printk("================== VT MT ==================\n");
	printk("<%d %d %d>\n", dim_param.vt_mtp[0], dim_param.vt_mtp[1], dim_param.vt_mtp[2]);

	calculate_voltage_tbl(&dim_param, voltage);

	//print_voltage_table(voltage);

	for (i = 0; i < MAX_DIMMING_INFO_COUNT; i ++)
		calculate_gamma_tbl(&dim_param, voltage, i, cmd[i].gamma);

	for (i = 0; i < MAX_DIMMING_INFO_COUNT; i ++) {
		for (j = 0; j < GAMMA_COMMAND_CNT; j++) {
			printk("%x ", cmd[i].gamma[j]);
		}
		printk("\n");
	}

	return ret;
}


#endif



