#include "pwrcal-env.h"
#include "pwrcal-pmu.h"
#include "pwrcal-clk.h"
#include "pwrcal-vclk.h"
#include "pwrcal-dfs.h"



unsigned int dfs_set_rate_switch(unsigned int rate_from,
					unsigned int rate_to,
					struct dfs_table *table)
{
	unsigned int rate_max;
	int i;

	rate_max = (rate_from > rate_to) ? rate_from : rate_to;

	for (i = 0; i < table->num_of_switches; i++) {
		if (rate_max >= table->switches[i].switch_rate) {
			if (is_div(table->switch_src_div))
				if (pwrcal_div_set_ratio(
					table->switch_src_div,
					table->switches[i].div_value + 1))
					goto errorout;

			if (is_mux(table->switch_src_mux))
				if (pwrcal_mux_set_src(
					table->switch_src_mux,
					table->switches[i].mux_value))
					goto errorout;

			return table->switches[i].switch_rate;
		}
	}

	return table->switches[table->num_of_switches - 1].switch_rate;
errorout:
	return 0;
}

int dfs_enable_switch(struct dfs_table *table)
{
	if (is_gate(table->switch_src_gate))
		if (pwrcal_gate_enable(table->switch_src_gate))
			return -1;

	if (is_mux(table->switch_src_usermux))
		if (pwrcal_mux_set_src(table->switch_src_usermux, 1))
			return -1;

	return 0;
}

int dfs_disable_switch(struct dfs_table *table)
{
	if (is_mux(table->switch_src_usermux))
		if (pwrcal_mux_set_src(table->switch_src_usermux, 0))
			return -1;

	if (is_div(table->switch_src_div))
		if (pwrcal_div_set_ratio(table->switch_src_div, 1))
			return -1;

	if (is_gate(table->switch_src_gate))
		if (pwrcal_gate_disable(table->switch_src_gate))
			return -1;

	return 0;
}

int dfs_use_switch(struct dfs_table *table)
{
	if (is_mux(table->switch_mux))
		if (pwrcal_mux_set_src(table->switch_mux,
					table->switch_use))
			return -1;

	return 0;
}

int dfs_not_use_switch(struct dfs_table *table)
{
	if (is_mux(table->switch_mux))
		if (pwrcal_mux_set_src(table->switch_mux, table->switch_notuse))
			return -1;

	return 0;
}

int dfs_trans_div(int lv_from, int lv_to, struct dfs_table *table, int opt)
{
	unsigned int from;
	unsigned int to;
	int trans;
	int i;
	struct pwrcal_clk *clk;

	for (i = 1; i < table->num_of_members; i++) {
		clk = table->members[i];
		if (is_div(clk)) {
			if (lv_from >= 0)
				from = get_value(table, lv_from, i);
			else
				from = pwrcal_div_get_ratio(clk) - 1;

			to = get_value(table, lv_to, i);

			trans = 0;
			switch (opt) {
			case TRANS_HIGH:
				if (from < to)
					trans = 1;
				break;
			case TRANS_LOW:
				if (from > to)
					trans = 1;
				break;
			case TRANS_DIFF:
				if (from != to)
					trans = 1;
				break;
			case TRANS_FORCE:
				trans = 1;
				break;
			default:
				break;
			}
			if (trans == 0)
				continue;

			if (pwrcal_div_set_ratio(clk, to + 1))
				goto errorout;
		}
	}
	return 0;

errorout:
	pr_err("%s %s %d\n", __func__, clk->name, to + 1);
	return -1;
}

int dfs_trans_pll(int lv_from, int lv_to, struct dfs_table *table, int opt)
{
	unsigned long long rate;
	unsigned int from;
	unsigned int to;
	int trans;
	int i;
	struct pwrcal_clk *clk;

	for (i = 1; i < table->num_of_members; i++) {
		clk = table->members[i];
		if (is_pll(clk)) {
			if (lv_from >= 0) {
				from = get_value(table, lv_from, i);
			} else {
				rate = pwrcal_pll_get_rate(clk);
				do_div(rate, 1000);
				from = (unsigned int)rate;
			}

			to = get_value(table, lv_to, i);

			trans = 0;
			switch (opt) {
			case TRANS_HIGH:
				if (from < to)
					trans = 1;
				if (to == 0)
					trans = 0;
				break;
			case TRANS_LOW:
				if (from > to)
					trans = 1;
				if (from == 0)
					trans = 0;
				break;
			case TRANS_DIFF:
				if (from != to)
					trans = 1;
				break;
			case TRANS_FORCE:
				trans = 1;
				break;
			default:
				break;
			}

			if (trans == 0)
				continue;

			rate = (unsigned long long)to * 1000;
			if (rate != 0) {
				if (pwrcal_pll_set_rate(clk, rate))
					goto errorout;
				if (pwrcal_pll_is_enabled(clk) != 1)
					if (pwrcal_pll_enable(clk))
						goto errorout;
			} else {
				if (pwrcal_pll_is_enabled(clk) != 0)
					if (pwrcal_pll_disable(clk))
						goto errorout;
			}
		}
	}
	return 0;

errorout:
	pr_err("%s %s %d\n", __func__, clk->name, to);
	return -1;
}

int dfs_trans_mux(int lv_from, int lv_to, struct dfs_table *table, int opt)
{
	unsigned int from;
	unsigned int to;
	int trans;
	int i;
	struct pwrcal_clk *clk;

	for (i = 1; i < table->num_of_members; i++) {
		clk = table->members[i];
		if (is_mux(clk)) {
			if (lv_from >= 0)
				from = get_value(table, lv_from, i);
			else
				from = pwrcal_mux_get_src(clk);

			to = get_value(table, lv_to, i);

			trans = 0;
			switch (opt) {
			case TRANS_HIGH:
				if (from < to)
					trans = 1;
				break;
			case TRANS_LOW:
				if (from > to)
					trans = 1;
				break;
			case TRANS_DIFF:
				if (from != to)
					trans = 1;
				break;
			case TRANS_FORCE:
				trans = 1;
				break;
			default:
				break;
			}

			if (trans == 0)
				continue;

			if (pwrcal_mux_set_src(clk, to) != 0)
				goto errorout;
		}
	}

	return 0;

errorout:
	pr_err("%s %s %d\n", __func__, clk->name, to);
	return -1;
}

int dfs_trans_gate(int lv_from, int lv_to, struct dfs_table *table, int opt)
{
	unsigned int from;
	unsigned int to;
	int trans;
	int i;
	struct pwrcal_clk *clk;

	for (i = 1; i < table->num_of_members; i++) {
		clk = table->members[i];
		if (is_gate(clk)) {
			if (lv_from >= 0)
				from = get_value(table, lv_from, i);
			else
				from = pwrcal_gate_is_enabled(clk);

			to = get_value(table, lv_to, i);

			trans = 0;
			switch (opt) {
			case TRANS_HIGH:
				if (from < to)
					trans = 1;
				break;
			case TRANS_LOW:
				if (from > to)
					trans = 1;
				break;
			case TRANS_DIFF:
				if (from != to)
					trans = 1;
				break;
			default:
				break;
			}

			if (trans == 0)
				continue;

			if (to)
				pwrcal_gate_enable(clk);
			else
				pwrcal_gate_disable(clk);
		}
	}
	return 0;
}

int dfs_get_lv(unsigned int rate, struct dfs_table *table)
{
	int i;
	unsigned int rep;

	if (rate == 0)
		return -1;

	for (i = 0; i < table->num_of_lv; i++) {
		rep = *(table->rate_table + (table->num_of_members * i));
		if (0 != rep && rate >= rep)
			return i;
	}

	return i;
}




static int transition(unsigned int rate_from,
			unsigned int rate_to,
			struct dfs_table *table)
{
	int lv_from, lv_to, lv_switch;
	unsigned int rate_switch;

	lv_from = dfs_get_lv(rate_from, table);
	lv_to = dfs_get_lv(rate_to, table);

	if (lv_from == lv_to)
		return 0;

	if (lv_from >= table->num_of_lv || lv_to >= table->num_of_lv)
		goto errorout;

	if (table->trans_pre)
		table->trans_pre(rate_from, rate_to);

	if (table->num_of_switches != 0) {
		rate_switch = dfs_set_rate_switch(rate_from, rate_to, table);
		lv_switch = dfs_get_lv(rate_switch, table);

		if (dfs_enable_switch(table))
			goto errorout;
		if (dfs_trans_div(lv_from, lv_switch, table, TRANS_HIGH))
			goto errorout;
		if (table->switch_pre)
			table->switch_pre(rate_from, rate_switch);
		if (dfs_use_switch(table))
			goto errorout;
		if (table->switch_post)
			table->switch_post(rate_from, rate_switch);
		if (dfs_trans_mux(lv_from, lv_switch, table, TRANS_DIFF))
			goto errorout;
		if (dfs_trans_div(lv_from, lv_switch, table, TRANS_LOW))
			goto errorout;
		if (dfs_trans_pll(lv_from, lv_to, table, TRANS_DIFF))
			goto errorout;
		if (dfs_trans_div(lv_switch, lv_to, table, TRANS_HIGH))
			goto errorout;
		if (table->switch_pre)
			table->switch_pre(rate_switch, rate_to);
		if (dfs_not_use_switch(table))
			goto errorout;
		if (table->switch_post)
			table->switch_post(rate_switch, rate_to);
		if (dfs_trans_mux(lv_switch, lv_to, table, TRANS_DIFF))
			goto errorout;
		if (dfs_trans_div(lv_switch, lv_to, table, TRANS_LOW))
			goto errorout;
		if (dfs_disable_switch(table))
			goto errorout;
	} else {
		if (dfs_trans_gate(lv_from, lv_to, table, TRANS_HIGH))
			goto errorout;
		if (dfs_trans_div(lv_from, lv_to, table, TRANS_HIGH))
			goto errorout;
		if (dfs_trans_pll(lv_from, lv_to, table, TRANS_LOW))
			goto errorout;
		if (dfs_trans_mux(lv_from, lv_to, table, TRANS_DIFF))
			goto errorout;
		if (dfs_trans_pll(lv_from, lv_to, table, TRANS_HIGH))
			goto errorout;
		if (dfs_trans_div(lv_from, lv_to, table, TRANS_LOW))
			goto errorout;
		if (dfs_trans_gate(lv_from, lv_to, table, TRANS_LOW))
			goto errorout;
	}

	if (table->trans_post)
		table->trans_post(rate_from, rate_to);

	return 0;

errorout:
	return -1;
}

static unsigned int get_rate(struct dfs_table *table)
{
	int l, m;
	unsigned int cur[128] = {0, };
	unsigned long long rate;
	struct pwrcal_clk *clk;

	for (m = 1; m < table->num_of_members; m++) {
		clk = table->members[m];
		if (is_pll(clk)) {
			rate = pwrcal_pll_get_rate(clk);
			do_div(rate, 1000);
			cur[m] = (unsigned int)rate;
		}
		if (is_mux(clk))
			cur[m] = pwrcal_mux_get_src(clk);
		if (is_div(clk))
			cur[m] = pwrcal_div_get_ratio(clk) - 1;
		if (is_gate(clk))
			cur[m] = pwrcal_gate_is_enabled(clk);
	}

	for (l = 0; l < table->num_of_lv; l++) {
		for (m = 1; m < table->num_of_members; m++)
			if (cur[m] != get_value(table, l, m))
				break;

		if (m == table->num_of_members)
			return get_value(table, l, 0);
	}

	if (is_pll(table->members[1])) {
		for (l = 0; l < table->num_of_lv; l++)
			if (cur[1] == get_value(table, l, 1))
				return get_value(table, l, 0);
	}


	for (m = 1; m < table->num_of_members; m++) {
		clk = table->members[m];
		pr_err("dfs_get_rate mid : %s : %d\n", clk->name, cur[m]);
	}

	return 0;
}


int dfs_get_rate_table(struct dfs_table *dfs, unsigned long *table)
{
	int i;

	for (i = 0; i < dfs->num_of_lv; i++)
		table[i] = get_value(dfs, i, 0);

	return dfs->num_of_lv;
}


int dfs_get_target_rate_table(struct dfs_table *dfs,
				unsigned int mux,
				unsigned int div,
				unsigned long *table)
{
	int m, d, i;
	int num_of_parent;
	struct pwrcal_clk *parents[32];
	unsigned int parents_rate[32];
	unsigned long long rate;
	unsigned int src, ratio;

	for (m = 0; m < dfs->num_of_members; m++)
		if (dfs->members[m]->id == mux)
			break;

	for (d = 0; d < dfs->num_of_members; d++)
		if (dfs->members[d]->id == div)
			break;


	if (m != dfs->num_of_members) {
		num_of_parent = pwrcal_mux_get_parents(dfs->members[m],
								parents);
		for (i = 0; i < num_of_parent; i++) {
			rate = pwrcal_clk_get_rate(parents[i]);
			do_div(rate, 1000);
			parents_rate[i] = (unsigned int)rate;
		}
	} else {
		parents[0] = pwrcal_clk_get_parent(dfs->members[d]);
		rate = pwrcal_clk_get_rate(parents[0]);
		do_div(rate, 1000);
		parents_rate[0] = (unsigned int)rate;
	}



	for (i = 0; i < dfs->num_of_lv; i++) {
		src = get_value(dfs, i, m);
		ratio = get_value(dfs, i, d) + 1;

		if (m == dfs->num_of_members) {
			table[i] = parents_rate[0] / ratio;
			continue;
		}
		if (d == dfs->num_of_members) {
			table[i] = parents_rate[src];
			continue;
		}
		table[i] = parents_rate[src] / ratio;
	}

	return i;
}


#ifdef CONFIG_SOC_EXYNOS8890
extern spinlock_t dfs_int_spinlock;
DFS_EXTERN(dvfs_int)
#endif
int dfs_set_rate(struct vclk *vclk, unsigned long to)
{
	struct pwrcal_vclk_dfs *dfs;
	unsigned long ret = -1;
#ifdef CONFIG_SOC_EXYNOS8890
	unsigned long flag = 0;

	if (vclk == &vclk_dvfs_int.vclk)
		spin_lock_irqsave(&dfs_int_spinlock, flag);
#endif
	dfs = to_dfs(vclk);

/*	pr_info("(%s) set from (%ldHz) to (%ldHz) start\n",
				vclk->name, vclk->vfreq, to);	*/
	if (transition((unsigned int)(vclk->vfreq),
			(unsigned int)to,
			dfs->table))
		goto out;
/*	pr_info("(%s) set from (%ldHz) to (%ldHz) success\n",
				vclk->name, vclk->vfreq, to);	*/

	ret = 0;
out:
#ifdef CONFIG_SOC_EXYNOS8890
	if (vclk == &vclk_dvfs_int.vclk)
		spin_unlock_irqrestore(&dfs_int_spinlock, flag);
#endif

	if (ret)
		pr_err("dfs_set_rate error (%s) from(%ld) to(%ld)\n",
				vclk->name, vclk->vfreq, to);

	return ret;
}


unsigned long dfs_get_rate(struct vclk *vclk)
{
	struct pwrcal_vclk_dfs *dfs;
	unsigned long ret = 0;
#ifdef CONFIG_SOC_EXYNOS8890
	unsigned long flag = 0;

	if (vclk == &vclk_dvfs_int.vclk)
		spin_lock_irqsave(&dfs_int_spinlock, flag);
#endif
	dfs = to_dfs(vclk);

	ret =  (unsigned long)get_rate(dfs->table);

#ifdef CONFIG_SOC_EXYNOS8890
	if (vclk == &vclk_dvfs_int.vclk)
		spin_unlock_irqrestore(&dfs_int_spinlock, flag);
#endif
	return ret;
}

int dfs_enable(struct vclk *vclk)
{
	struct pwrcal_vclk_dfs *dfs;
	int ret = -1;

	dfs = to_dfs(vclk);

	if (dfs->en_clks)
		if (set_config(dfs->en_clks, 0))
			goto out;

	ret = 0;
out:
	if (ret)
		pr_err("p1_enable error (%s)\n", vclk->name);

	return ret;
}

int dfs_disable(struct vclk *vclk)
{
	struct pwrcal_vclk_dfs *dfs;
	int ret = -1;

	dfs = to_dfs(vclk);

	if (dfs->en_clks != 0)
		if (set_config(dfs->en_clks, 1))
			goto out;

	ret = 0;
out:
	if (ret)
		pr_err("dfs_disable error (%s)\n", vclk->name);

	return ret;
}

int dfs_is_enabled(struct vclk *vclk)
{
	if (vclk->ref_count)
		return 1;

	return 0;
}

struct vclk_ops dfs_ops = {
	.enable = dfs_enable,
	.disable = dfs_disable,
	.is_enabled = dfs_is_enabled,
	.get_rate = dfs_get_rate,
	.set_rate = dfs_set_rate,
};

unsigned long dfs_get_max_freq(struct vclk *vclk)
{
	struct pwrcal_vclk_dfs *dfs = to_dfs(vclk);

	return dfs->table->max_freq;
}

unsigned long dfs_get_min_freq(struct vclk *vclk)
{
	struct pwrcal_vclk_dfs *dfs = to_dfs(vclk);

	return dfs->table->min_freq;
}
