#include "pwrcal-env.h"
#include "pwrcal-vclk.h"
#include "pwrcal-pmu.h"
#include "pwrcal-clk.h"
#include "pwrcal-rae.h"
#include <linux/exynos-ss.h>


#define is_vclk(id)	((id & 0x0F000000) == 0x0A000000)

unsigned int _cal_vclk_get(char *name)
{
	unsigned int id;

	for (id = 0; id < vclk_grpgate_list_size; id++)
		if (!strcmp(vclk_grpgate_list[id]->vclk.name, name))
			return vclk_group_grpgate + id;

	for (id = 0; id < vclk_m1d1g1_list_size; id++)
		if (!strcmp(vclk_m1d1g1_list[id]->vclk.name, name))
			return vclk_group_m1d1g1 + id;

	for (id = 0; id < vclk_p1_list_size; id++)
		if (!strcmp(vclk_p1_list[id]->vclk.name, name))
			return vclk_group_p1 + id;

	for (id = 0; id < vclk_m1_list_size; id++)
		if (!strcmp(vclk_m1_list[id]->vclk.name, name))
			return vclk_group_m1 + id;

	for (id = 0; id < vclk_d1_list_size; id++)
		if (!strcmp(vclk_d1_list[id]->vclk.name, name))
			return vclk_group_d1 + id;

	for (id = 0; id < vclk_pxmxdx_list_size; id++)
		if (!strcmp(vclk_pxmxdx_list[id]->vclk.name, name))
			return vclk_group_pxmxdx + id;

	for (id = 0; id < vclk_umux_list_size; id++)
		if (!strcmp(vclk_umux_list[id]->vclk.name, name))
			return vclk_group_umux + id;

	for (id = 0; id < vclk_dfs_list_size; id++)
		if (!strcmp(vclk_dfs_list[id]->vclk.name, name))
			return vclk_group_dfs + id;

	return 0;
}

struct vclk *cal_get_vclk(unsigned int id)
{
	struct vclk *ret = NULL;

	if (!is_vclk(id))
		return ret;

	switch (id & vclk_group_mask) {
	case vclk_group_grpgate:
		ret = &(vclk_grpgate_list[id & 0x0000FFFF]->vclk);
		break;
	case vclk_group_m1d1g1:
		ret = &(vclk_m1d1g1_list[id & 0x0000FFFF]->vclk);
		break;
	case vclk_group_p1:
		ret = &(vclk_p1_list[id & 0x0000FFFF]->vclk);
		break;
	case vclk_group_m1:
		ret = &(vclk_m1_list[id & 0x0000FFFF]->vclk);
		break;
	case vclk_group_d1:
		ret = &(vclk_d1_list[id & 0x0000FFFF]->vclk);
		break;
	case vclk_group_pxmxdx:
		ret = &(vclk_pxmxdx_list[id & 0x0000FFFF]->vclk);
		break;
	case vclk_group_umux:
		ret = &(vclk_umux_list[id & 0x0000FFFF]->vclk);
		break;
	case vclk_group_dfs:
		ret = &(vclk_dfs_list[id & 0x0000FFFF]->vclk);
		break;
	default:
		break;
	}

	return ret;
}

static int vclk_is_enabled(struct vclk *vclk)
{
	if (vclk->ref_count)
		return 1;
	return 0;
}

/*
 * grpgate functions list
 */
static unsigned long grpgate_get(struct vclk *vclk)
{
	struct pwrcal_vclk_grpgate *grpgate;
	struct pwrcal_clk *first;
	unsigned long ret = 0;

	grpgate = to_grpgate(vclk);

	first = grpgate->gates[0];
	if (first != CLK_NONE)
		ret = (unsigned long)pwrcal_clk_get_rate(first);

	return ret;
}

static int grpgate_enable(struct vclk *vclk)
{
	struct pwrcal_vclk_grpgate *grpgate;
	struct pwrcal_clk *cur;
	struct pwrcal_clk **gates_list;
	void *addr;
	unsigned int value, dirty;
	int i;

	grpgate = to_grpgate(vclk);
	gates_list = grpgate->gates;
	addr = (void *)0xFFFFFFFF;
	value = 0;
	dirty = 0;

	for (i = 0; gates_list[i] != CLK_NONE; i++) {
		cur = gates_list[i];
		if (!is_gate(cur))
			continue;

		if (addr != cur->offset) {
			if (dirty == 1) {
				pwrcal_writel(addr, value);
				dirty = 0;
			}
			addr = cur->offset;
			value = pwrcal_readl(addr);
		}

		if (value & (1 << cur->shift))
			continue;

		value |= (1 << cur->shift);
		dirty = 1;
	}
	if (dirty == 1)
		pwrcal_writel(addr, value);

	return 0;
}

static int grpgate_disable(struct vclk *vclk)
{
	struct pwrcal_vclk_grpgate *grpgate;
	struct pwrcal_clk *cur;
	struct pwrcal_clk **gates_list;
	void *addr;
	unsigned int value, dirty;
	int i;

	grpgate = to_grpgate(vclk);
	gates_list = grpgate->gates;
	addr = (void *)0xFFFFFFFF;
	value = 0;
	dirty = 0;

	for (i = 0; gates_list[i] != CLK_NONE; i++) {
		cur = gates_list[i];
		if (!is_gate(cur))
			continue;

		if (addr != cur->offset) {
			if (dirty == 1) {
				pwrcal_writel(addr, value);
				dirty = 0;
			}
			addr = cur->offset;
			value = pwrcal_readl(addr);
		}

		if (value & (1 << cur->shift)) {
			value &= ~(1 << cur->shift);
			dirty = 1;
		}
	}

	if (dirty == 1)
		pwrcal_writel(addr, value);

	return 0;
}

struct vclk_ops grpgate_ops = {
	.enable		= grpgate_enable,
	.disable	= grpgate_disable,
	.is_enabled = vclk_is_enabled,
	.get_rate	= grpgate_get,
};

/*
 * config related functions list
 */
int is_config(struct pwrcal_clk_set *table, int config)
{
	int lidx;
	unsigned int ratio, src, enable;
	unsigned long long rate;
	int target;
	int diff = 0;
	struct pwrcal_clk *cur;

	for (lidx = 0; table[lidx].clk != CLK_NONE; lidx++) {
		cur = table[lidx].clk;
		target = config ? table[lidx].config1 : table[lidx].config0;
		if (target == -1)
			continue;
		switch (cur->id & mask_of_type) {
		case div_type:
			ratio = pwrcal_div_get_ratio(cur);
			if (ratio !=  target + 1) {
				if (table[lidx].config0 == -1
						|| table[lidx].config1 == -1) {
					pr_warn("check div (%s)[%d][%d]",
							cur->name,
							target + 1,
							ratio);
				}
				diff = 1;
			}
			break;
		case pll_type:
			rate = pwrcal_pll_get_rate(cur);
			if (rate != ((unsigned long long)target) * 1000) {
				if (table[lidx].config0 == -1
						|| table[lidx].config1 == -1) {
					do_div(rate, 1000);
					pr_warn("check pll. (%s)[%dKhz][%lldKhz]",
							cur->name,
							target,
							rate);
				}
				diff = 1;
			}
			break;
		case mux_type:
			src = pwrcal_mux_get_src(cur);
			if (src != target) {
				if (table[lidx].config0 == -1
						|| table[lidx].config1 == -1) {
					pr_warn("check mux. (%s)[%d][%d]",
							cur->name,
							target,
							src);
				}
				diff = 1;
			}
			break;
		case gate_type:
			enable = pwrcal_gate_is_enabled(cur);
			if (enable != target) {
				if (table[lidx].config0 == -1
						|| table[lidx].config1 == -1) {
					pr_warn("check gate. (%s)[%d][%d]",
							cur->name,
							target,
							enable);
				}
				diff = 1;
			}
			break;
		default:
			break;
		}

		if (diff)
			return 0;
	}

	return 1;
}


int set_config(struct pwrcal_clk_set *table, int config)
{
	int lidx;
	int max;
	unsigned long long rate;
	int to;
	struct pwrcal_clk *cur;

	for (lidx = 0; table[lidx].clk != CLK_NONE; lidx++) {
		cur = table[lidx].clk;
		if (is_gate(cur)) {
			to = config ? table[lidx].config1 : table[lidx].config0;
			if (to != 1)
				continue;
			pwrcal_gate_enable(cur);
		}
	}

	for (lidx = 0; table[lidx].clk != CLK_NONE; lidx++) {
		cur = table[lidx].clk;
		if (is_div(cur)) {
			to = config ? table[lidx].config1 : table[lidx].config0;
			if (to == -1)
				continue;

			if (pwrcal_div_get_ratio(cur) <  to + 1)
				if (pwrcal_div_set_ratio(cur, to + 1))
					return -1;
		}
	}

	for (lidx = 0; table[lidx].clk != CLK_NONE; lidx++) {
		cur = table[lidx].clk;
		if (is_pll(cur)) {
			to = config ? table[lidx].config1 : table[lidx].config0;
			if (to == -1)
				continue;

			if (to == 0) {
				if (pwrcal_pll_disable(cur) != 0)
					return -1;
				continue;
			}

			rate = ((unsigned long long)to) * 1000;
			if (pwrcal_pll_get_rate(cur) > rate) {
				if (pwrcal_pll_set_rate(cur, rate))
					return -1;

				if (pwrcal_pll_is_enabled(cur) == 0)
					if (pwrcal_pll_enable(cur))
						return -1;
			}
		}
	}

	for (lidx = 0; table[lidx].clk != CLK_NONE; lidx++) {
		cur = table[lidx].clk;
		if (is_mux(cur)) {
			to = config ? table[lidx].config1 : table[lidx].config0;
			if (to == -1)
				continue;

			if (pwrcal_mux_get_src(cur) != to)
				if (pwrcal_mux_set_src(cur, to))
					return -1;
		}
	}

	max = lidx;

	for (lidx = max - 1; lidx >= 0; lidx--) {
		cur = table[lidx].clk;
		if (is_pll(cur)) {
			to = config ? table[lidx].config1 : table[lidx].config0;
			if (to == -1)
				continue;

			if (to == 0) {
				if (pwrcal_pll_disable(cur))
					return -1;
				continue;
			}

			rate = ((unsigned long long)to) * 1000;
			if (pwrcal_pll_get_rate(cur) < rate) {
				if (pwrcal_pll_set_rate(cur, rate))
					return -1;

				if (pwrcal_pll_is_enabled(cur) == 0)
					if (pwrcal_pll_enable(cur))
						return -1;
			}
		}
	}

	for (lidx = max - 1; lidx >= 0; lidx--) {
		cur = table[lidx].clk;
		if (is_div(cur)) {
			to = config ? table[lidx].config1 : table[lidx].config0;
			if (to == -1)
				continue;

			if (pwrcal_div_get_ratio(cur) >  to + 1)
				if (pwrcal_div_set_ratio(cur, to + 1))
					return -1;
		}
	}

	for (lidx = max - 1; lidx >= 0; lidx--) {
		cur = table[lidx].clk;
		if (is_gate(cur)) {
			to = config ? table[lidx].config1 : table[lidx].config0;
			if (to != 0)
				continue;
			pwrcal_gate_disable(cur);
		}
	}

	return 0;
}

/*
 * pxmxdx functions list
 */
static unsigned long pxmxdx_get(struct vclk *vclk)
{
	struct pwrcal_vclk_pxmxdx *pxmxdx;
	struct pwrcal_clk *first;
	unsigned long ret = 0;

	pxmxdx = to_pxmxdx(vclk);

	first = pxmxdx->clk_list[0].clk;
	if (first != CLK_NONE)
		ret = (unsigned long)pwrcal_clk_get_rate(first);

	return ret;
}

static int pxmxdx_enable(struct vclk *vclk)
{
	struct pwrcal_vclk_pxmxdx *pxmxdx;
	int ret;

	pxmxdx = to_pxmxdx(vclk);
	ret = set_config(pxmxdx->clk_list, 0);

	if (ret)
		pr_err("pxmxdx_enable error (%s)", vclk->name);

	return ret;
}

static int pxmxdx_disable(struct vclk *vclk)
{
	struct pwrcal_vclk_pxmxdx *pxmxdx;
	int ret;

	pxmxdx = to_pxmxdx(vclk);

	ret = set_config(pxmxdx->clk_list, 1);

	if (ret)
		pr_err("pxmxdx_disable error (%s)\n", vclk->name);

	return ret;
}

struct vclk_ops pxmxdx_ops = {
	.enable		= pxmxdx_enable,
	.disable	= pxmxdx_disable,
	.is_enabled = vclk_is_enabled,
	.get_rate	= pxmxdx_get,
};

/*
 * umux functions list
 */
static unsigned long umux_get(struct vclk *vclk)
{
	struct pwrcal_vclk_umux *umux;
	unsigned long ret = 0;

	umux = to_umux(vclk);
	if (umux->umux != CLK_NONE)
		ret = (unsigned long)pwrcal_clk_get_rate(umux->umux);

	return ret;
}

static int umux_enable(struct vclk *vclk)
{
	struct pwrcal_vclk_umux *umux;

	umux = to_umux(vclk);
	if (umux->umux != CLK_NONE)
		pwrcal_mux_set_src(umux->umux, 1);

	return 0;
}

static int umux_disable(struct vclk *vclk)
{
	struct pwrcal_vclk_umux *umux;

	umux = to_umux(vclk);
	if (umux->umux != CLK_NONE)
		pwrcal_mux_set_src(umux->umux, 0);

	return 0;
}

struct vclk_ops umux_ops = {
	.enable		= umux_enable,
	.disable	= umux_disable,
	.is_enabled = vclk_is_enabled,
	.get_rate	= umux_get,
};

/*
 * m1d1g1 functions list
 */
static unsigned long m1d1g1_get(struct vclk *vclk)
{
	struct pwrcal_vclk_m1d1g1 *m1d1g1;
	unsigned long ret = -1;

	m1d1g1 = to_m1d1g1(vclk);

	if (m1d1g1->gate != CLK_NONE)
		ret = (unsigned long)pwrcal_clk_get_rate(m1d1g1->gate);
	else if (m1d1g1->div != CLK_NONE)
		ret = (unsigned long)pwrcal_clk_get_rate(m1d1g1->div);
	else if (m1d1g1->mux != CLK_NONE)
		ret = (unsigned long)pwrcal_clk_get_rate(m1d1g1->mux);

	return ret;
}


static int m1d1g1_set(struct vclk *vclk, unsigned long freq_to)
{
	struct pwrcal_vclk_m1d1g1 *m1d1g1;
	int pidx, didx;
	struct pwrcal_clk *parents[32];
	unsigned long parent_freq[32];
	unsigned long long temp_freq;
	int min_diff = 0x7FFFFFFF;
	int min_diff_mux = 0x7FFFFFFF;
	int min_diff_div = 0x7FFFFFFF;
	unsigned long cur_freq;
	unsigned int num_of_parents;
	unsigned int max_ratio;
	int ret = -1;

	m1d1g1 = to_m1d1g1(vclk);

	if (freq_to == 0 && m1d1g1->extramux != CLK_NONE)
		if (pwrcal_mux_get_src(m1d1g1->extramux) != 0)
			if (pwrcal_mux_set_src(m1d1g1->extramux, 0))
				goto out;

	if (freq_to == 0 && m1d1g1->gate != CLK_NONE) {
		if (pwrcal_gate_is_enabled(m1d1g1->gate) != 0)
			if (pwrcal_gate_disable(m1d1g1->gate))
				goto out;
		goto success;
	}

	num_of_parents = pwrcal_mux_get_parents(m1d1g1->mux, parents);
	max_ratio = pwrcal_div_get_max_ratio(m1d1g1->div);

	for (pidx = 0; pidx < num_of_parents; pidx++) {
		temp_freq = pwrcal_clk_get_rate(parents[pidx]);
		parent_freq[pidx] = (unsigned long)temp_freq;
	}

	if (num_of_parents == 0) {
		temp_freq = pwrcal_clk_get_rate(m1d1g1->div->parent);
		parent_freq[0] = temp_freq;
		num_of_parents = 1;
	}

	if (freq_to == 0) {
		freq_to = parent_freq[0];
		for (pidx = 0; pidx < num_of_parents; pidx++)
			if (freq_to > parent_freq[pidx])
				freq_to = parent_freq[pidx];

		if (freq_to != 0)
			freq_to /= (max_ratio - 1);
	}

	for (pidx = 0; pidx < num_of_parents && min_diff != 0; pidx++) {
		for (didx = 1; didx < max_ratio + 1 && min_diff != 0; didx++) {
			if (parent_freq[pidx])
				cur_freq = parent_freq[pidx] / didx;
			else
				cur_freq = 0;
			if (freq_to >= cur_freq) {
				if (min_diff > freq_to - cur_freq) {
					min_diff = freq_to - cur_freq;
					min_diff_mux = pidx;
					min_diff_div = didx;
					if (min_diff == 0)
						break;
				}
			}
		}
	}

	if (min_diff == 0x7FFFFFFF) {
		if (freq_to == 0)
			goto success;
		goto out;
	}

	if (m1d1g1->div != CLK_NONE)
		if (pwrcal_div_get_ratio(m1d1g1->div) < min_diff_div)
			if (pwrcal_div_set_ratio(m1d1g1->div, min_diff_div))
				goto out;

	if (m1d1g1->mux != CLK_NONE)
		if (pwrcal_mux_get_src(m1d1g1->mux) != min_diff_mux)
			if (pwrcal_mux_set_src(m1d1g1->mux, min_diff_mux))
				goto out;

	if (m1d1g1->div != CLK_NONE)
		if (pwrcal_div_get_ratio(m1d1g1->div) > min_diff_div)
			if (pwrcal_div_set_ratio(m1d1g1->div, min_diff_div))
				goto out;

	if (freq_to != 0 && m1d1g1->gate != CLK_NONE)
		if (pwrcal_gate_is_enabled(m1d1g1->gate) != 1)
			if (pwrcal_gate_enable(m1d1g1->gate))
				goto out;

	if (freq_to != 0 && m1d1g1->extramux != CLK_NONE)
		if (pwrcal_mux_get_src(m1d1g1->extramux) != 1)
			if (pwrcal_mux_set_src(m1d1g1->extramux, 1))
				goto out;

success:
	ret = 0;
out:

	if (ret)
		pr_err("m1d1g1_set error(%s) (%ld)\n", vclk->name, freq_to);

	return ret;
}


static int m1d1g1_enable(struct vclk *vclk)
{
	struct pwrcal_vclk_m1d1g1 *m1d1g1;
	int ret = -1;

	m1d1g1 = to_m1d1g1(vclk);

	if (m1d1g1->mux != CLK_NONE) {
		if (pwrcal_mux_is_enabled(m1d1g1->mux) != 1) {
			ret = pwrcal_mux_enable(m1d1g1->mux);
			if (ret)
				goto out;
		}
	}

	if (m1d1g1->div != CLK_NONE) {
		if (pwrcal_div_is_enabled(m1d1g1->div) != 1) {
			ret = pwrcal_div_enable(m1d1g1->div);
			if (ret)
				goto out;
		}
	}

	if (m1d1g1->gate != CLK_NONE) {
		if (pwrcal_gate_is_enabled(m1d1g1->gate) != 1) {
			ret = pwrcal_gate_enable(m1d1g1->gate);
			if (ret)
				goto out;
		}
	}

	if (m1d1g1->extramux != CLK_NONE) {
		if (pwrcal_mux_get_src(m1d1g1->extramux) != 1) {
			ret = pwrcal_mux_set_src(m1d1g1->extramux, 1);
			if (ret)
				goto out;
		}
	}

	ret = 0;
out:
	if (ret)
		pr_err("m1d1g1_enable error (%s)", vclk->name);

	return ret;
}

static int m1d1g1_disable(struct vclk *vclk)
{
	struct pwrcal_vclk_m1d1g1 *m1d1g1;
	int ret = 0;

	m1d1g1 = to_m1d1g1(vclk);

	if (m1d1g1->extramux != CLK_NONE) {
		if (pwrcal_mux_get_src(m1d1g1->extramux) != 0) {
			ret = pwrcal_mux_set_src(m1d1g1->extramux, 0);
			if (ret)
				goto out;
		}
	}

	if (m1d1g1->gate != CLK_NONE) {
		if (pwrcal_gate_is_enabled(m1d1g1->gate) != 0) {
			ret = pwrcal_gate_disable(m1d1g1->gate);
			if (ret)
				goto out;
		}
	}

	if (m1d1g1->mux != CLK_NONE) {
		if (pwrcal_mux_is_enabled(m1d1g1->mux) != 0) {
			ret = pwrcal_mux_disable(m1d1g1->mux);
			if (ret)
				goto out;
		}
	}

out:
	if (ret)
		pr_err("pxmxdx_disable error (%s)\n", vclk->name);

	return ret;
}

struct vclk_ops m1d1g1_ops = {
	.enable		= m1d1g1_enable,
	.disable	= m1d1g1_disable,
	.is_enabled = vclk_is_enabled,
	.get_rate	= m1d1g1_get,
	.set_rate	= m1d1g1_set,
};

/*
 * p1 functions list
 */
static unsigned long p1_get(struct vclk *vclk)
{
	struct pwrcal_vclk_p1 *p1;
	unsigned long ret = 0;

	p1 = to_p1(vclk);
	ret = (unsigned long)pwrcal_pll_get_rate(p1->pll);

	return ret;
}


static int p1_set(struct vclk *vclk, unsigned long freq_to)
{
	struct pwrcal_vclk_p1 *p1;
	unsigned long long temp_freq;
	unsigned long ret = -1;

	p1 = to_p1(vclk);
	temp_freq = (unsigned long long)freq_to;

	if (pwrcal_pll_get_rate(p1->pll) != temp_freq)
		if (pwrcal_pll_set_rate(p1->pll, temp_freq))
			goto out;

	ret = 0;
out:
	if (ret)
		pr_err("p1_set error(%s) (%ld)\n", vclk->name, freq_to);

	return ret;
}


static int p1_enable(struct vclk *vclk)
{
	struct pwrcal_vclk_p1 *p1;
	int ret = -1;

	p1 = to_p1(vclk);
	ret = pwrcal_pll_enable(p1->pll);
	if (ret)
		goto out;

	ret = 0;
out:
	if (ret)
		pr_err("p1_enable error (%s)\n", vclk->name);

	return ret;
}

static int p1_disable(struct vclk *vclk)
{
	struct pwrcal_vclk_p1 *p1;
	int ret = -1;

	p1 = to_p1(vclk);
	ret = pwrcal_pll_disable(p1->pll);
	if (ret)
		goto out;

	ret = 0;
out:
	if (ret)
		pr_err("pxmxdx_disable error (%s)\n", vclk->name);

	return ret;
}

struct vclk_ops p1_ops = {
	.enable		= p1_enable,
	.disable	= p1_disable,
	.is_enabled = vclk_is_enabled,
	.get_rate	= p1_get,
	.set_rate	= p1_set,
};

/*
 * m1 functions list
 */
static unsigned long m1_get(struct vclk *vclk)
{
	struct pwrcal_vclk_m1 *m1;
	unsigned long ret = 0;

	m1 = to_m1(vclk);
	ret = (unsigned long)pwrcal_pll_get_rate(m1->mux);

	return ret;
}


static int m1_set(struct vclk *vclk, unsigned long freq_to)
{
	struct pwrcal_vclk_m1 *m1;
	unsigned long ret = -1;

	m1 = to_m1(vclk);
	if (pwrcal_mux_get_src(m1->mux) != freq_to)
		if (pwrcal_mux_set_src(m1->mux, freq_to))
			goto out;

	ret = 0;
out:
	if (ret)
		pr_err("m1_set error(%s) (%ld)\n", vclk->name, freq_to);

	return ret;
}


static int m1_enable(struct vclk *vclk)
{
	struct pwrcal_vclk_m1 *m1;
	int ret = -1;

	m1 = to_m1(vclk);
	ret = pwrcal_mux_enable(m1->mux);
	if (ret)
		goto out;

	ret = 0;
out:
	if (ret)
		pr_err("m1_enable error (%s)\n", vclk->name);

	return ret;
}

static int m1_disable(struct vclk *vclk)
{
	struct pwrcal_vclk_m1 *m1 = to_m1(vclk);
	int ret;

	ret = pwrcal_mux_disable(m1->mux);

	if (ret)
		pr_err("m1_disable error (%s)\n", vclk->name);

	return ret;
}

struct vclk_ops m1_ops = {
	.enable		= m1_enable,
	.disable	= m1_disable,
	.is_enabled = vclk_is_enabled,
	.get_rate	= m1_get,
	.set_rate	= m1_set,
};

/*
 * d1 functions list
 */
static unsigned long d1_get(struct vclk *vclk)
{
	struct pwrcal_vclk_d1 *d1;
	unsigned long ret = 0;

	d1 = to_d1(vclk);
	ret = (unsigned long)pwrcal_clk_get_rate(d1->div);

	return ret;
}


static int d1_set(struct vclk *vclk, unsigned long freq_to)
{
	struct pwrcal_vclk_d1 *d1;
	unsigned long long temp_freq;
	unsigned long long parent_freq;
	unsigned long long parent_temp;
	unsigned int ratio;
	struct pwrcal_clk *parent;
	unsigned long ret = -1;

	d1 = to_d1(vclk);
	temp_freq = (unsigned long long)freq_to;

	if (pwrcal_clk_get_rate(d1->div) != temp_freq) {
		parent = pwrcal_clk_get_parent(d1->div);
		parent_freq = pwrcal_clk_get_rate(parent);

		if (parent_freq < temp_freq)
			goto out;

		parent_temp = parent_freq;
		do_div(parent_temp, temp_freq);
		if (parent_freq > parent_temp * temp_freq)
			parent_temp += 1;
		ratio = (unsigned int)parent_temp;
		if (pwrcal_div_set_ratio(d1->div, ratio))
			goto out;
	}

	ret = 0;
out:

	if (ret)
		pr_err("d1_set error(%s) (%ld)\n", vclk->name, freq_to);

	return ret;
}


static int d1_enable(struct vclk *vclk)
{
	struct pwrcal_vclk_d1 *d1;
	int ret = -1;

	d1 = to_d1(vclk);
	ret = pwrcal_div_enable(d1->div);
	if (ret)
		goto out;

	ret = 0;
out:
	if (ret)
		pr_err("d1_enable error (%s)\n", vclk->name);

	return ret;
}

static int d1_disable(struct vclk *vclk)
{
	struct pwrcal_vclk_d1 *d1;
	int ret = -1;

	d1 = to_d1(vclk);

	ret = pwrcal_div_disable(d1->div);
	if (ret)
		goto out;

	ret = 0;
out:
	if (ret)
		pr_err("d1_disable error (%s)\n", vclk->name);

	return ret;
}

struct vclk_ops d1_ops = {
	.enable		= d1_enable,
	.disable	= d1_disable,
	.is_enabled = vclk_is_enabled,
	.get_rate	= d1_get,
	.set_rate	= d1_set,
};



struct pwrcal_vclk_none vclk_0;



int vclk_setrate(struct vclk *vclk, unsigned long rate)
{
	int ret = 0;
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	const char *name = "vclk_setrate";
#endif
	if (!vclk->ref_count && vclk->ops->set_rate) {
		vclk->vfreq = rate;
		goto out;
	}

	exynos_ss_clk(vclk, name, ESS_FLAG_IN);

	if (vclk->ops->set_rate)
		ret = vclk->ops->set_rate(vclk, rate);
	else
		ret = -1;

	if (!ret) {
		vclk->vfreq = rate;
		exynos_ss_clk(vclk, name, ESS_FLAG_OUT);
	} else
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);
out:
	return ret;
}
unsigned long vclk_getrate(struct vclk *vclk)
{
	int ret = 0;
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	const char *name = "vclk_getrate";
#endif

	exynos_ss_clk(vclk, name, ESS_FLAG_IN);
	if (!vclk->ref_count) {
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);
		goto out;
	}

	ret = vclk->ops->get_rate(vclk);

	if (ret > 0) {
		vclk->vfreq = (unsigned long)ret;
		exynos_ss_clk(vclk, name, ESS_FLAG_OUT);
	} else
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);
out:
	return ret;
}
int vclk_enable(struct vclk *vclk)
{
	int ret = 0;
	unsigned int tmp;
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	const char *name = "vclk_enable";
#endif
	if (vclk->ref_count++)
		goto out;

	if (vclk->parent != VCLK_NONE)
		ret = vclk_enable(vclk->parent);

	exynos_ss_clk(vclk, name, ESS_FLAG_IN);
	if (ret)
		goto out;

	ret = vclk->ops->enable(vclk);

	if (ret || !vclk->vfreq || !vclk->ops->set_rate)
		goto out;

	tmp = vclk->vfreq;
	if (vclk->type == vclk_group_dfs)
		vclk->vfreq = 0;

	if (vclk->ops->set_rate(vclk, tmp))
		pr_warn("vclk retenction fail (%s) (%ld)\n",
				vclk->name, vclk->vfreq);

out:
	if (!ret) {
		exynos_ss_clk(vclk, name, ESS_FLAG_OUT);
	} else
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);

	return ret;
}
int vclk_disable(struct vclk *vclk)
{
	int ret = 0;
	int parent_disable = 0;
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	const char *name = "vclk_disable";
#endif
	if (vclk->ref_count)
		parent_disable = 1;

	if (vclk->ref_count)
		vclk->ref_count--;

	if (vclk->ref_count)
		goto out;

	exynos_ss_clk(vclk, name, ESS_FLAG_IN);
	ret = vclk->ops->disable(vclk);

	if (ret) {
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);
		goto out;
	} else
		exynos_ss_clk(vclk, name, ESS_FLAG_OUT);

	if (parent_disable && vclk->parent != VCLK_NONE)
		ret = vclk_disable(vclk->parent);

out:
	return ret;
}
