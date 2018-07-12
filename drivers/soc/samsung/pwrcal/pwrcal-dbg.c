
#include "pwrcal-env.h"
#include "pwrcal-clk.h"
#include "pwrcal-vclk.h"
#include "pwrcal-pmu.h"
#include "pwrcal-rae.h"


static void cal_vclk_grpgate_info(struct vclk *vclk)
{
	struct pwrcal_vclk_grpgate *grpgate = to_grpgate(vclk);
	int i;

	pr_info("GRPGATE : %s\n", vclk->name);
	pr_info("- ref : %d\n", vclk->ref_count);

	if (!vclk->ref_count)
		goto out;

	for (i = 0; grpgate->gates[i] != CLK_NONE; i++)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				grpgate->gates[i]->name,
				pwrcal_clk_get_rate(grpgate->gates[i]),
				(unsigned int)(unsigned long)(grpgate->gates[i]->offset),
				grpgate->gates[i]->shift,
				grpgate->gates[i]->id);
out:
	return;
}

static void cal_vclk_m1d1g1_info(struct vclk *vclk)
{
	struct pwrcal_vclk_m1d1g1 *m1d1g1 = to_m1d1g1(vclk);

	pr_info("M1D1G1 : %s\n", vclk->name);
	pr_info("- ref : %d vfreq : %ld]\n", vclk->ref_count, vclk->vfreq);
	if (!vclk->ref_count)
		goto out;

	if (m1d1g1->mux != CLK_NONE)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				m1d1g1->mux->name,
				pwrcal_clk_get_rate(m1d1g1->mux),
				(unsigned int)(unsigned long)(m1d1g1->mux->offset),
				m1d1g1->mux->shift,
				m1d1g1->mux->id);

	if (m1d1g1->div != CLK_NONE)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				m1d1g1->div->name,
				pwrcal_clk_get_rate(m1d1g1->div),
				(unsigned int)(unsigned long)(m1d1g1->div->offset),
				m1d1g1->div->shift,
				m1d1g1->div->id);

	if (m1d1g1->gate != CLK_NONE)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				m1d1g1->gate->name,
				pwrcal_clk_get_rate(m1d1g1->gate),
				(unsigned int)(unsigned long)(m1d1g1->gate->offset),
				m1d1g1->gate->shift,
				m1d1g1->gate->id);

	if (m1d1g1->extramux != CLK_NONE)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				m1d1g1->extramux->name,
				pwrcal_clk_get_rate(m1d1g1->extramux),
				(unsigned int)(unsigned long)(m1d1g1->extramux->offset),
				m1d1g1->extramux->shift,
				m1d1g1->extramux->id);

out:
	return;
}

static void cal_vclk_p1_info(struct vclk *vclk)
{
	struct pwrcal_vclk_p1 *p1 = to_p1(vclk);

	pr_info("P1 : %s\n", vclk->name);
	pr_info("- ref : %d vfreq : %ld]\n", vclk->ref_count, vclk->vfreq);

	if (!vclk->ref_count)
		goto out;

	if (p1->pll != CLK_NONE)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				p1->pll->name,
				pwrcal_clk_get_rate(p1->pll),
				(unsigned int)(unsigned long)(p1->pll->offset),
				p1->pll->shift,
				p1->pll->id);

out:
	return;
}

static void cal_vclk_m1_info(struct vclk *vclk)
{
	struct pwrcal_vclk_m1 *m1 = to_m1(vclk);

	pr_info("M1 : %s\n", vclk->name);
	pr_info("- ref : %d vfreq : %ld]\n", vclk->ref_count, vclk->vfreq);

	if (!vclk->ref_count)
		goto out;

	if (m1->mux != CLK_NONE)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				m1->mux->name,
				pwrcal_clk_get_rate(m1->mux),
				(unsigned int)(unsigned long)(m1->mux->offset),
				m1->mux->shift,
				m1->mux->id);

out:
	return;
}

static void cal_vclk_d1_info(struct vclk *vclk)
{
	struct pwrcal_vclk_d1 *d1 = to_d1(vclk);

	pr_info("M1 : %s\n", vclk->name);
	pr_info("- ref : %d vfreq : %ld]\n", vclk->ref_count, vclk->vfreq);

	if (!vclk->ref_count)
		goto out;

	if (d1->div != CLK_NONE)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				d1->div->name,
				pwrcal_clk_get_rate(d1->div),
				(unsigned int)(unsigned long)(d1->div->offset),
				d1->div->shift,
				d1->div->id);

out:
	return;
}

static void cal_vclk_umux_info(struct vclk *vclk)
{
	struct pwrcal_vclk_umux *umux = to_umux(vclk);

	pr_info("M1 : %s\n", vclk->name);
	pr_info("- ref : %d vfreq : %ld]\n", vclk->ref_count, vclk->vfreq);

	if (!vclk->ref_count)
		goto out;

	if (umux->umux != CLK_NONE)
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				umux->umux->name,
				pwrcal_clk_get_rate(umux->umux),
				(unsigned int)(unsigned long)(umux->umux->offset),
				umux->umux->shift,
				umux->umux->id);

out:
	return;
}

static void cal_vclk_pxmxdx_info(struct vclk *vclk)
{
	struct pwrcal_vclk_pxmxdx *pxmxdx = to_pxmxdx(vclk);
	struct pwrcal_clk *clk;
	int i;

	pr_info("M1 : %s\n", vclk->name);
	pr_info("- ref : %d vfreq : %ld]\n", vclk->ref_count, vclk->vfreq);

	if (!vclk->ref_count)
		goto out;

	for (i = 0; pxmxdx->clk_list[i].clk != CLK_NONE; i++) {
		clk = pxmxdx->clk_list[i].clk;
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				clk->name,
				pwrcal_clk_get_rate(clk),
				(unsigned int)(unsigned long)(clk->offset),
				clk->shift,
				clk->id);
	}

out:
	return;
}

static void cal_vclk_dfs_info(struct vclk *vclk)
{
	struct pwrcal_vclk_dfs *dfs = to_dfs(vclk);
	struct pwrcal_clk *clk;
	int i;

	pr_info("DFS : %s\n", vclk->name);
	pr_info("- ref : %d vfreq : %ld]\n", vclk->ref_count, vclk->vfreq);

	if (!vclk->ref_count)
		goto out;

	for (i = 1; i < dfs->table->num_of_members; i++) {
		clk = dfs->table->members[i];
		pr_info("- %-40s\t%12lldHz\tSFR: 0x%08X[%d]\tID[0x%08X]\n",
				clk->name,
				pwrcal_clk_get_rate(clk),
				(unsigned int)(unsigned long)(clk->offset),
				clk->shift,
				clk->id);
	}

out:
	return;
}

void cal_vclk_dbg_info(unsigned int id)
{
	struct vclk *vclk = cal_get_vclk(id);

	if ((id & mask_of_type) != vclk_type) {
		pr_err("cal_vclk_dbg_info : invalid id (0x%08X)\n", id);
		return;
	}

	switch (id & (mask_of_type | mask_of_group)) {
	case vclk_group_grpgate:
		cal_vclk_grpgate_info(vclk);
		break;
	case vclk_group_m1d1g1:
		cal_vclk_m1d1g1_info(vclk);
		break;
	case vclk_group_p1:
		cal_vclk_p1_info(vclk);
		break;
	case vclk_group_m1:
		cal_vclk_m1_info(vclk);
		break;
	case vclk_group_d1:
		cal_vclk_d1_info(vclk);
		break;
	case vclk_group_pxmxdx:
		cal_vclk_pxmxdx_info(vclk);
		break;
	case vclk_group_umux:
		cal_vclk_umux_info(vclk);
		break;
	case vclk_group_dfs:
		cal_vclk_dfs_info(vclk);
		break;
	default:
		break;
	}

	return;
}
