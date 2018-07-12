/* linux/drivers/video/exynos_decon/panel/dimming_core.c
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

#include "dimming_core.h"

static int calc_vt_volt(int gamma)
{
	int max;

	max = (sizeof(vt_trans_volt) >> 2) - 1;
	if (gamma > max) {
		dimm_err(" %s exceed gamma value\n", __func__);
		gamma = max;
	}

	return (int)vt_trans_volt[gamma];
}

static int calc_v0_volt(struct dim_data *data, int color)
{
	return DOUBLE_MULTIPLE_VREGOUT;
}

static int calc_v1_volt(struct dim_data *data, int color)
{
	int gap;
	int ret, v7, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V1][color];

	if (gamma > vreg_element_max[V1]) {
		dimm_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V1];
	}
	if (gamma < 0) {
		dimm_err(":%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	v7 = data->volt[TBL_INDEX_V7][color];
	gap = (DOUBLE_MULTIPLE_VREGOUT - v7);
	temp = (unsigned long)gap * (unsigned long)v1_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = DOUBLE_MULTIPLE_VREGOUT - (int)temp;

	return ret;
}

static int calc_v7_volt(struct dim_data *data, int color)
{
	int gap;
	int ret, v11, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V7][color];

	if (gamma > vreg_element_max[V7]) {
		dimm_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V7];
	}
	if (gamma < 0) {
		dimm_err(":%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	v11 = data->volt[TBL_INDEX_V11][color];

	gap = (DOUBLE_MULTIPLE_VREGOUT - v11);
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = DOUBLE_MULTIPLE_VREGOUT - (int)temp;

	return ret;
}

static int calc_v11_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v23, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V11][color];

	if (gamma > vreg_element_max[V11]) {
		dimm_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V11];
	}
	if (gamma < 0) {
		dimm_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v23 = data->volt[TBL_INDEX_V23][color];

	gap = vt - v23;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;

	return ret ;
}

static int calc_v23_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v35, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V23][color];

	if (gamma > vreg_element_max[V23]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V23];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}


	vt = data->volt_vt[color];
	v35 = data->volt[TBL_INDEX_V35][color];

	gap = vt - v35;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;

	return ret;

}

static int calc_v35_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v51, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V35][color];

	if (gamma > vreg_element_max[V35]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V35];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v51 = data->volt[TBL_INDEX_V51][color];

	gap = vt - v51;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;

	return ret;
}

static int calc_v51_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v87, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V51][color];

	if (gamma > vreg_element_max[V51]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V51];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v87 = data->volt[TBL_INDEX_V87][color];

	gap = vt - v87;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;

	return ret;
}

static int calc_v87_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v151, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V87][color];

	if (gamma > vreg_element_max[V87]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V87];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v151 = data->volt[TBL_INDEX_V151][color];

	gap = vt - v151;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;

	return ret;
}

static int calc_v151_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v203, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V151][color];

	if (gamma > vreg_element_max[V151]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V151];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v203 = data->volt[TBL_INDEX_V203][color];

	gap = vt - v203;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;
	ret = vt - (int)temp;

	return ret;
}

static int calc_v203_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v255, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V203][color];

	if (gamma > vreg_element_max[V203]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V203];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v255 = data->volt[TBL_INDEX_V255][color];

	gap = vt - v255;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;
	ret = vt - (int)temp;

	return ret;
}

static int calc_v255_volt(struct dim_data *data, int color)
{
	int ret, gamma;

	gamma = data->t_gamma[V255][color];


	if (gamma > vreg_element_max[V255]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int calc_inter_v1_v7(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v1, v7, ratio, temp;

	ratio = (int)int_tbl_v1_v7[gray];

	v1 = data->volt[TBL_INDEX_V1][color];
	v7 = data->volt[TBL_INDEX_V7][color];

	temp = (v1 - v7) * ratio;
	temp >>= 10;
	ret = v1 - temp;

	return ret;
}

static int calc_inter_v7_v11(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v7, v11, ratio, temp;

	ratio = (int)int_tbl_v7_v11[gray];
	v7 = data->volt[TBL_INDEX_V7][color];
	v11 = data->volt[TBL_INDEX_V11][color];

	temp = (v7 - v11) * ratio;
	temp >>= 10;
	ret = v7 - temp;

	return ret;
}

static int calc_inter_v11_v23(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v11, v23, ratio, temp;

	ratio = (int)int_tbl_v11_v23[gray];
	v11 = data->volt[TBL_INDEX_V11][color];
	v23 = data->volt[TBL_INDEX_V23][color];

	temp = (v11 - v23) * ratio;
	temp >>= 10;
	ret = v11 - temp;

	return ret;
}

static int calc_inter_v23_v35(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v23, v35, ratio, temp;

	ratio = (int)int_tbl_v23_v35[gray];
	v23 = data->volt[TBL_INDEX_V23][color];
	v35 = data->volt[TBL_INDEX_V35][color];

	temp = (v23 - v35) * ratio;
	temp >>= 10;
	ret = v23 - temp;

	return ret;
}

static int calc_inter_v35_v51(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v35, v51, ratio, temp;

	ratio = (int)int_tbl_v35_v51[gray];
	v35 = data->volt[TBL_INDEX_V35][color];
	v51 = data->volt[TBL_INDEX_V51][color];

	temp = (v35 - v51) * ratio;
	temp >>= 10;
	ret = v35 - temp;

	return ret;
}

static int calc_inter_v51_v87(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v51, v87, ratio, temp;

	ratio = (int)int_tbl_v51_v87[gray];
	v51 = data->volt[TBL_INDEX_V51][color];
	v87 = data->volt[TBL_INDEX_V87][color];

	temp = (v51 - v87) * ratio;
	temp >>= 10;
	ret = v51 - temp;

	return ret;
}

static int calc_inter_v87_v151(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v87, v151, ratio, temp;

	ratio = (int)int_tbl_v87_v151[gray];
	v87 = data->volt[TBL_INDEX_V87][color];
	v151 = data->volt[TBL_INDEX_V151][color];

	temp = (v87 - v151) * ratio;
	temp >>= 10;
	ret = v87 - temp;
	return ret;
}

static int calc_inter_v151_v203(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v151, v203, ratio, temp;

	ratio = (int)int_tbl_v151_v203[gray];
	v151 = data->volt[TBL_INDEX_V151][color];
	v203 = data->volt[TBL_INDEX_V203][color];

	temp = (v151 - v203) * ratio;
	temp >>= 10;
	ret = v151 - temp;

	return ret;
}

static int calc_inter_v203_v255(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v203, v255, ratio, temp;

	ratio = (int)int_tbl_v203_v255[gray];
	v203 = data->volt[TBL_INDEX_V203][color];
	v255 = data->volt[TBL_INDEX_V255][color];

	temp = (v203 - v255) * ratio;
	temp >>= 10;
	ret = v203 - temp;

	return ret;
}


int generate_volt_table(struct dim_data *data)
{
	int i, j;
	int seq, index, gray;
	int ret = 0;

	int calc_seq[NUM_VREF] = {V255, V203, V151, V87, V51, V35, V23, V11, V7, V1};
	int (*calc_volt_point[NUM_VREF])(struct dim_data *, int) = {
		calc_v1_volt,
		calc_v7_volt,
		calc_v11_volt,
		calc_v23_volt,
		calc_v35_volt,
		calc_v51_volt,
		calc_v87_volt,
		calc_v151_volt,
		calc_v203_volt,
		calc_v255_volt,
	};
	int (*calc_inter_volt[NUM_VREF])(struct dim_data *, int, int)  = {
		NULL,
		calc_inter_v1_v7,
		calc_inter_v7_v11,
		calc_inter_v11_v23,
		calc_inter_v23_v35,
		calc_inter_v35_v51,
		calc_inter_v51_v87,
		calc_inter_v87_v151,
		calc_inter_v151_v203,
		calc_inter_v203_v255,
	};

	for (i = 0; i < CI_MAX; i++) {
		data->volt_vt[i] = calc_vt_volt(data->vt_mtp[i]);
	}

	/* calculate voltage for V0 */
	for (i = 0; i < CI_MAX; i++)
		data->volt[0][i] = calc_v0_volt(data, i);

	/* calculate voltage for every vref point */
	for (j = 0; j < NUM_VREF; j++) {
		seq = calc_seq[j];
 		index = vref_index[seq];
		if (calc_volt_point[seq] != NULL) {
			for (i = 0; i < CI_MAX; i++)
				data->volt[index][i] = calc_volt_point[seq](data ,i);
		}
	}

	index = 0;
	for (i = 0; i < MAX_GRADATION; i++) {
		if (i == vref_index[index]) {
			index++;
			continue;
		}
		gray = (i - vref_index[index - 1]) - 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_inter_volt[index] != NULL) {
				data->volt[i][j] = calc_inter_volt[index](data, gray, j);
			}
		}

	}
#if defined (SMART_DIMMING_DEBUG)
	dsim_info("=========================== VT Voltage ===========================\n");

	dsim_info("R : %05d : G: %05d : B : %05d\n",
					data->volt_vt[0], data->volt_vt[1], data->volt_vt[2]);

	dsim_info("\n=================================================================\n");

	for (i = 0; i < MAX_GRADATION; i++) {
		dsim_info("V%03d R : %05d : G : %05d B : %05d\n", i,
					data->volt[i][CI_RED], data->volt[i][CI_GREEN], data->volt[i][CI_BLUE]);
	}
#endif
	return ret;
}


static int lookup_volt_index(struct dim_data *data, int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray / (MULTIPLY_VALUE * MULTIPLY_VALUE);
	index = (int)lookup_tbl[temp];
	exit = 1;
	i = 0;
	while(exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_BRIGHTNESS)
			index_h = MAX_BRIGHTNESS;
		cnt_l = (int)lookup_tbl[index] - (int)lookup_tbl[index_l];
		cnt_h = (int)lookup_tbl[index_h] - (int)lookup_tbl[index];

		if (cnt_l + cnt_h) {
			exit = 0;
		}
		i++;
	}

	p_delta = 0;
	index = (int)lookup_tbl[index_l];
	ret = index;

	temp = gamma_multi_tbl[index] * MULTIPLY_VALUE;

	if (gray > temp)
		p_delta = gray - temp;
	else
		p_delta = temp - gray;

	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = gamma_multi_tbl[index + i] * MULTIPLY_VALUE;
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

static int calc_reg_v1(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V1][color];
	t2 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V7][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V1].de) / (unsigned long)t2;
	ret =  temp - fix_const[V1].nu;

	return ret;
}


static int calc_reg_v7(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V7][color];
	t2 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V11][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V7].de) / (unsigned long)t2;
	ret =  temp - fix_const[V7].nu;

	return ret;
}


static int calc_reg_v11(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V11][color];
	t2 = data->volt_vt[color] - data->look_volt[V23][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V11].de) / (unsigned long)t2;
	ret =  (int)temp - fix_const[V11].nu;

	return ret;

}

static int calc_reg_v23(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V23][color];
	t2 = data->volt_vt[color] - data->look_volt[V35][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V23].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V23].nu;

	return ret;

}

static int calc_reg_v35(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V35][color];
	t2 = data->volt_vt[color] - data->look_volt[V51][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V35].de)/ (unsigned long)t2;
	ret = (int)temp - fix_const[V35].nu;

	return ret;
}


static int calc_reg_v51(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V51][color];
	t2 = data->volt_vt[color] - data->look_volt[V87][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V51].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V51].nu;

	return ret;
}


static int calc_reg_v87(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V87][color];
	t2 = data->volt_vt[color] - data->look_volt[V151][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V87].de) / (unsigned long)t2;

	ret = (int)temp - fix_const[V87].nu;

	return ret;
}

static int calc_reg_v151(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V151][color];
	t2 = data->volt_vt[color] - data->look_volt[V203][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V151].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V151].nu;

	return ret;
}



static int calc_reg_v203(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V203][color];
	t2 = data->volt_vt[color] - data->look_volt[V255][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V203].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V203].nu;

	return ret;
}

static int calc_reg_v255(struct dim_data *data, int color)
{
	int ret;
	int t1;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT -  data->look_volt[V255][color];
	temp = ((unsigned long)t1 * (unsigned long)fix_const[V255].de) / DOUBLE_MULTIPLE_VREGOUT;
	ret = (int)temp - fix_const[V255].nu;

	return ret;
}

int cal_gamma_from_index(struct dim_data *data, struct SmtDimInfo *brInfo)
{
	int i, j;
	int ret = 0;
	int gray, index;
	signed int shift, c_shift;
	int gamma_int[NUM_VREF][CI_MAX];
	int br, temp;
	unsigned char *result;
	int (*calc_reg[NUM_VREF])(struct dim_data *, int)  = {
		calc_reg_v1,
		calc_reg_v7,
		calc_reg_v11,
		calc_reg_v23,
		calc_reg_v35,
		calc_reg_v51,
		calc_reg_v87,
		calc_reg_v151,
		calc_reg_v203,
		calc_reg_v255,
	};

	br = brInfo->refBr;
	result = brInfo->gamma;

	if (br > MAX_BRIGHTNESS) {
		dsim_err("Warning Exceed Max brightness : %d\n", br);
		br = MAX_BRIGHTNESS;
	}
	for (i = V1; i < NUM_VREF; i++) {
#ifdef CONFIG_REF_SHIFT
		/* get reference shift value */
		if (brInfo->rTbl == NULL) {
			shift = 0;
		}
		else {
			shift = (signed int)brInfo->rTbl[i];
		}
#else
		shift = 0;
#endif
		gray = brInfo->cGma[vref_index[i]] * br;

		index = lookup_volt_index(data, gray);
		index = index + shift;
		if(i == V1)
			index = 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				data->look_volt[i][j] = data->volt[index][j];
			}
		}
	}
	for (i = V1; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				index = (i * CI_MAX) + j;
#ifdef CONFIG_COLOR_SHIFT
				if (brInfo->cTbl == NULL)
					c_shift = 0;
				else
					c_shift = (signed int)brInfo->cTbl[index];

#else
				c_shift = 0;
#endif
				temp = calc_reg[i](data, j);
				gamma_int[i][j] = (temp + c_shift) - data->mtp[i][j];

				if (gamma_int[i][j] >= vreg_element_max[i])
					gamma_int[i][j] = vreg_element_max[i];
				if (gamma_int[i][j] < 0)
					gamma_int[i][j] = 0;
			}
		}
	}
	/*
	for (j = 0; j < CI_MAX; j++) {
		//gamma_int[VT][j] = data->mtp_gamma[VT][j];
		gamma_int[VT][j] = data->t_gamma[VT][j] - data->mtp[VT][j];
	}
	*/
	index = 0;
	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= V1; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] = gamma_int[i][j] > 0xff ? 1 : 0;
				result[index++] = gamma_int[i][j] & 0xff;
			} else {
				result[index++] = (unsigned char)gamma_int[i][j];
			}
		}
	}
	//result[index++] = data->vt_mtp[CI_RED] | (data->vt_mtp[CI_GREEN] << 4);
	//result[index++] = data->vt_mtp[CI_BLUE] & 0x0f;
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}

