#include "pwrcal-env.h"
#include "pwrcal-clk.h"
#include "pwrcal-rae.h"

struct pwrcal_clk_none clk_0;

struct pwrcal_pll clk_pll_start, clk_pll_end;
struct pwrcal_clk_fixed_rate clk_fixed_rate_start, clk_fixed_rate_end;
struct pwrcal_clk_fixed_factor clk_fixed_factor_start, clk_fixed_factor_end;
struct pwrcal_mux clk_mux_start, clk_mux_end;
struct pwrcal_div clk_div_start, clk_div_end;
struct pwrcal_gate clk_gate_start, clk_gate_end;

unsigned int _cal_clk_get(char *name)
{
	int id;
	struct pwrcal_pll *pll;
	struct pwrcal_clk_fixed_rate *fixed_rate;
	struct pwrcal_clk_fixed_factor *fixed_factor;
	struct pwrcal_mux *mux;
	struct pwrcal_div *div;
	struct pwrcal_gate *gate;

	for (pll = &clk_pll_start, id = 0; pll < &clk_pll_end; pll++, id++)
		if (!strcmp(pll->clk.name, name))
			return pll_type + id;
	for (fixed_rate = &clk_fixed_rate_start, id = 0; fixed_rate < &clk_fixed_rate_end; fixed_rate++, id++)
		if (!strcmp(fixed_rate->clk.name, name))
			return fixed_rate_type + id;
	for (fixed_factor = &clk_fixed_factor_start, id = 0; fixed_factor < &clk_fixed_factor_end; fixed_factor++, id++)
		if (!strcmp(fixed_factor->clk.name, name))
			return fixed_factor_type + id;
	for (mux = &clk_mux_start, id = 0; mux < &clk_mux_end; mux++, id++)
		if (!strcmp(mux->clk.name, name))
			return mux_type + id;
	for (div = &clk_div_start, id = 0; div < &clk_div_end; div++, id++)
		if (!strcmp(div->clk.name, name))
			return div_type + id;
	for (gate = &clk_gate_start, id = 0; gate < &clk_gate_end; gate++, id++)
		if (!strcmp(gate->clk.name, name))
			return gate_type + id;

	return 0;
}

struct pwrcal_clk *cal_get_clk(unsigned int id)
{
	struct pwrcal_clk *ret = NULL;

	switch (id & mask_of_type) {
	case pll_type:
		ret = &((&clk_pll_start)[id & 0x00000FFF].clk);
		break;
	case fixed_rate_type:
		ret = &((&clk_fixed_rate_start)[id & 0x00000FFF].clk);
		break;
	case fixed_factor_type:
		ret = &((&clk_fixed_factor_start)[id & 0x00000FFF].clk);
		break;
	case mux_type:
		ret = &((&clk_mux_start)[id & 0x00000FFF].clk);
		break;
	case div_type:
		ret = &((&clk_div_start)[id & 0x00000FFF].clk);
		break;
	case gate_type:
		ret = &((&clk_gate_start)[id & 0x00000FFF].clk);
		break;
	default:
		break;
	}

	return ret;
}


int pwrcal_gate_is_enabled(struct pwrcal_clk *clk)
{
	return pwrcal_getbit(clk->offset, clk->shift);
}

int pwrcal_gate_enable(struct pwrcal_clk *clk)
{
	pwrcal_setbit(clk->offset, clk->shift, 1);
	return 0;
}

int pwrcal_gate_disable(struct pwrcal_clk *clk)
{
	pwrcal_setbit(clk->offset, clk->shift, 0);
	return 0;
}

int pwrcal_mux_is_enabled(struct pwrcal_clk *clk)
{
	if (clk->enable)
		return pwrcal_getbit(clk->enable, clk->e_shift);
	return 1;
}

int pwrcal_mux_enable(struct pwrcal_clk *clk)
{
	if (clk->enable)
		pwrcal_setbit(clk->enable, clk->e_shift, 1);
	return 0;
}

int pwrcal_mux_disable(struct pwrcal_clk *clk)
{
	if (clk->enable)
		pwrcal_setbit(clk->enable, clk->e_shift, 0);
	return 0;
}

int pwrcal_mux_get_src(struct pwrcal_clk *clk)
{
	return pwrcal_getf(clk->offset, clk->shift, TO_MASK(clk->width));
}

int pwrcal_mux_set_src(struct pwrcal_clk *clk, unsigned int src)
{
	struct pwrcal_mux *mux = to_mux(clk);
	int timeout;
	unsigned int mux_stat;
	int muxgate = 1;

	if (src >= (unsigned int)(mux->num_parents))
		return -1;

	if (_pwrcal_is_private_mux_set_src(clk))
		return _pwrcal_private_mux_set_src(clk, src);

	if (mux->gate != CLK_NONE) {
		muxgate = pwrcal_gate_is_enabled(mux->gate);
		if (!muxgate)
			pwrcal_gate_enable(mux->gate);
	}

	if ((clk->id & user_mux_type) == user_mux_type && src == 1)
		pwrcal_setbit(clk->offset, 27, 0);

	pwrcal_setf(clk->offset, clk->shift, TO_MASK(clk->width), src);

	if ((clk->id & user_mux_type) == user_mux_type && src == 0)
		pwrcal_setbit(clk->offset, 27, 1);

	if ((clk->id & user_mux_type) == user_mux_type)
		return 0;

	if (clk->status) {
		for (timeout = 0;; timeout++) {
			mux_stat = pwrcal_getf(clk->status, clk->s_shift, TO_MASK(clk->s_width));
			if (mux_stat == (1 << src))
				break;

			if (timeout > CLK_WAIT_CNT)
				goto timeout_error;
			cpu_relax();
		}
	}

	if (!muxgate)
		pwrcal_gate_disable(mux->gate);

	return 0;

timeout_error:
	pr_err("stat(=%d) check time out, \'%s\', src_num(=%d)",
		   mux_stat, clk->name, src);
	return -1;
}

int pwrcal_mux_get_parents(struct pwrcal_clk *clk, struct pwrcal_clk **parents)
{
	struct pwrcal_mux *mux = to_mux(clk);
	int i;

	for (i = 0; i < mux->num_parents; i++)
		parents[i] = mux->parents[i];
	return mux->num_parents;
}

struct pwrcal_clk *pwrcal_mux_get_parent(struct pwrcal_clk *clk)
{
	struct pwrcal_mux *mux = to_mux(clk);
	int src;

	src = pwrcal_mux_get_src(clk);
	return mux->parents[src];
}

int pwrcal_div_is_enabled(struct pwrcal_clk *clk)
{
	return 1;
}

int pwrcal_div_enable(struct pwrcal_clk *clk)
{
	return 0;
}

int pwrcal_div_disable(struct pwrcal_clk *clk)
{
	return 0;
}

unsigned int pwrcal_div_get_ratio(struct pwrcal_clk *clk)
{
	return pwrcal_getf(clk->offset, clk->shift, TO_MASK(clk->width)) + 1;
}

int pwrcal_div_set_ratio(struct pwrcal_clk *clk, unsigned int ratio)
{
	int timeout;
	unsigned int div_stat_val;
	struct pwrcal_div *div = to_div(clk);
	int divgate = 1;

	if (ratio == 0) {
		pr_err("ratio is 0. \'%s\'", clk->name);
		return -1;
	}

	if (ratio > (TO_MASK(clk->width) + 1)) {
		pr_err("ratio is bigger than max. (%d) > of (%d) \'%s\'",
				ratio,
				TO_MASK(clk->width) + 1,
				clk->name);
		return -1;
	}

	if (div->gate != CLK_NONE) {
		divgate = pwrcal_gate_is_enabled(div->gate);
		if (!divgate)
			pwrcal_gate_enable(div->gate);
	}

	pwrcal_setf(clk->offset, clk->shift, TO_MASK(clk->width), ratio - 1);

	if (clk->status != 0) {
		for (timeout = 0;; timeout++) {
			div_stat_val = pwrcal_getf(clk->status,
									   clk->s_shift,
									   TO_MASK(clk->s_width));
			if (0 == div_stat_val)
				break;

			if (timeout > CLK_WAIT_CNT)
				goto timeout_error;
			cpu_relax();
		}
	}

	if (!divgate)
		pwrcal_gate_disable(div->gate);

	return 0;

timeout_error:
	pr_err("stat(=%d) check time out, \'%s\'", div_stat_val, clk->name);
	return -1;
}

unsigned int pwrcal_div_get_max_ratio(struct pwrcal_clk *clk)
{
	return TO_MASK(clk->width) + 1;
}

struct pwrcal_clk *pwrcal_clk_get_parent(struct pwrcal_clk *clk)
{
	struct pwrcal_mux *mux;
	unsigned int src;

	switch (clk->id & mask_of_type) {
	case fixed_rate_type:
	case fixed_factor_type:
	case pll_type:
	case div_type:
	case gate_type:
		return clk->parent;
		break;
	case mux_type:
		mux = to_mux(clk);
		src = pwrcal_mux_get_src(clk);
		return mux->parents[src];
		break;
	default:
		break;
	}
	return CLK_NONE;
}

unsigned long long pwrcal_clk_get_rate(struct pwrcal_clk *clk)
{
	struct pwrcal_clk *cur = clk;
	struct pwrcal_clk_fixed_rate *frate;
	struct pwrcal_clk_fixed_factor *ffacor;
	struct pwrcal_mux *mux;
	unsigned int src;
	struct pwrcal_clk *clk_stack[32];
	unsigned int p = 0;
	unsigned long long rate = 0;
	unsigned int ratio;

	while (cur != CLK_NONE) {
		clk_stack[p++] = cur;
		switch (cur->id & mask_of_type) {
		case fixed_rate_type:
		case fixed_factor_type:
		case pll_type:
		case div_type:
		case gate_type:
			cur = cur->parent;
			break;
		case mux_type:
			mux = to_mux(cur);
			src = pwrcal_mux_get_src(cur);
			cur = mux->parents[src];
			break;
		default:
			return 0;
		}
	}

	/* calc rate */
	while (p != 0) {
		cur = clk_stack[--p];

		switch (cur->id & mask_of_type) {
		case fixed_rate_type:
			frate = to_frate(cur);
			rate = frate->fixed_rate;
			break;
		case fixed_factor_type:
			ffacor = to_ffactor(cur);
			ratio = (unsigned int)(ffacor->ratio);
			do_div(rate, ratio);
			break;
		case pll_type:
			if (pwrcal_pll_is_enabled(cur) == 0)
				return 0;
			rate = pwrcal_pll_get_rate(cur);
			break;
		case mux_type:
			if (pwrcal_mux_is_enabled(cur) == 0)
				return 0;
			break;
		case div_type:
			if (pwrcal_div_is_enabled(cur) == 0)
				return 0;
			ratio = pwrcal_div_get_ratio(cur);
			do_div(rate, ratio);
			break;
		case gate_type:
			if (pwrcal_gate_is_enabled(cur) == 0)
				return 0;
			break;
		default:
			return 0;
		}
	}

	return rate;
}

int pwrcal_mux_set_rate(struct pwrcal_clk *clk, unsigned long long rate)
{
	struct pwrcal_clk *parents[48];
	int num_of_parents, i, min_diff_parent = -1;
	unsigned long long parents_rate;
	unsigned long long diff = 0xFFFFFFFFFFFFFFFF;

	num_of_parents = pwrcal_mux_get_parents(clk, parents);
	for (i = 0; i < num_of_parents; i++) {
		parents_rate = pwrcal_clk_get_rate(parents[i]);
		if (parents_rate == rate) {
			min_diff_parent = i;
			break;
		}
		if (rate > parents_rate && diff > rate - parents_rate) {
			diff = rate - parents_rate;
			min_diff_parent = i;
		}
	}

	if (min_diff_parent == -1)
		return -1;

	return pwrcal_mux_set_src(clk, min_diff_parent);

}

int pwrcal_div_set_rate(struct pwrcal_clk *clk, unsigned long long rate)
{
	unsigned long long parents_rate;
	unsigned long long ratio;

	if (clk->parent == CLK_NONE)
		return -1;

	parents_rate = pwrcal_clk_get_rate(clk->parent);
	ratio = parents_rate / rate;
	if (rate < parents_rate / ratio)
		ratio += 1;
	return pwrcal_div_set_ratio(clk, ratio);
}


int pwrcal_clk_set_rate(struct pwrcal_clk *clk, unsigned long long rate)
{
	int ret = -1;

	switch (clk->id & mask_of_type) {
	case pll_type:
		ret = pwrcal_pll_set_rate(clk, rate);
		break;
	case mux_type:
		ret = pwrcal_mux_set_rate(clk, rate);
		break;
	case div_type:
		ret = pwrcal_div_set_rate(clk, rate);
		break;
	default:
		break;
	}
	return ret;
}

int pwrcal_clk_enable(struct pwrcal_clk *clk)
{
	int ret = -1;

	switch (clk->id & mask_of_type) {
	case pll_type:
		ret = pwrcal_pll_enable(clk);
		break;
	case mux_type:
		ret = pwrcal_mux_enable(clk);
		break;
	case div_type:
		ret = pwrcal_div_enable(clk);
		break;
	case gate_type:
		ret = pwrcal_gate_enable(clk);
		break;
	default:
		break;
	}
	return ret;
}

int pwrcal_clk_disable(struct pwrcal_clk *clk)
{
	int ret = -1;

	switch (clk->id & mask_of_type) {
	case pll_type:
		ret = pwrcal_pll_disable(clk);
		break;
	case mux_type:
		ret = pwrcal_mux_disable(clk);
		break;
	case div_type:
		ret = pwrcal_div_disable(clk);
		break;
	case gate_type:
		ret = pwrcal_gate_disable(clk);
		break;
	default:
		break;
	}
	return ret;
}
