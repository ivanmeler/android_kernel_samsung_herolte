#include "../pwrcal.h"
#include "../pwrcal-clk.h"
#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "../pwrcal-pmu.h"
#include "S5E8890-cmusfr.h"
#include "S5E8890-pmusfr.h"
#include "S5E8890-cmu.h"


/*
	PLLs
*/
/* PLL141XX Clock Type */
#define PLL141XX_MDIV_SHIFT		16
#define PLL141XX_PDIV_SHIFT		8
#define PLL141XX_SDIV_SHIFT		0
#define PLL141XX_MDIV_MASK		0x3FF
#define PLL141XX_PDIV_MASK		0x3F
#define PLL141XX_SDIV_MASK		0x7
#define PLL141XX_ENABLE			31
#define PLL141XX_LOCKED			29
#define PLL141XX_BYPASS			22

/* PLL1431X Clock Type */
#define PLL1431X_MDIV_SHIFT		16
#define PLL1431X_PDIV_SHIFT		8
#define PLL1431X_SDIV_SHIFT		0
#define PLL1431X_K_SHIFT		0
#define PLL1431X_MDIV_MASK		0x3FF
#define PLL1431X_PDIV_MASK		0x3F
#define PLL1431X_SDIV_MASK		0x7
#define PLL1431X_K_MASK		0xFFFF
#define PLL1431X_ENABLE			31
#define PLL1431X_LOCKED			29
#define PLL1431X_BYPASS			4

#define FIN_HZ_26M		(26*MHZ)

static int mfc_pll_refcount[2] = {1, 1};

static const struct pwrcal_pll_rate_table *_clk_get_pll_settings(
					struct pwrcal_pll *pll_clk,
					unsigned long long rate)
{
	int i;
	const struct pwrcal_pll_rate_table  *prate_table = pll_clk->rate_table;

	for (i = 0; i < pll_clk->rate_count; i++) {
		if (rate == prate_table[i].rate)
			return &prate_table[i];
	}

	return NULL;
}

static int _clk_pll141xx_find_pms(struct pll_spec *pll_spec,
				struct pwrcal_pll_rate_table *rate_table,
				unsigned long long rate)
{
	unsigned int p, m, s;
	unsigned long long fref, fvco, fout;
	unsigned long long tmprate, tmpfout;
	unsigned long long mindiffrate = 0xFFFFFFFFFFFFFFFF;
	unsigned int min_p, min_m, min_s, min_fout;

	for (p = pll_spec->pdiv_min; p <= pll_spec->pdiv_max; p++) {
		fref = FIN_HZ_26M / p;
		if ((fref < pll_spec->fref_min) || (fref > pll_spec->fref_max))
			continue;

		for (s = pll_spec->sdiv_min; s <= pll_spec->sdiv_max; s++) {
			tmprate = rate;
			do_div(tmprate, KHZ);
			tmprate = tmprate * p * (1 << s);
			do_div(tmprate, (FIN_HZ_26M / KHZ));
			m = (unsigned int)tmprate;

			if ((m < pll_spec->mdiv_min)
					|| (m > pll_spec->mdiv_max))
				continue;

			fvco = ((unsigned long long)FIN_HZ_26M) * m;
			do_div(fvco, p);
			if ((fvco < pll_spec->fvco_min)
					|| (fvco > pll_spec->fvco_max))
				continue;

			fout = fvco >> s;
			if ((fout >= pll_spec->fout_min)
					&& (fout <= pll_spec->fout_max)) {
				tmprate = rate;
				do_div(tmprate, KHZ);
				tmpfout = fout;
				do_div(tmpfout, KHZ);
				if (tmprate == tmpfout) {
					rate_table->rate = fout;
					rate_table->pdiv = p;
					rate_table->mdiv = m;
					rate_table->sdiv = s;
					rate_table->kdiv = 0;
					return 0;
				}
				if (tmpfout < tmprate && mindiffrate > tmprate - tmpfout) {
					mindiffrate = tmprate - tmpfout;
					min_fout = fout;
					min_p = p;
					min_m = m;
					min_s = s;
				}
			}
		}
	}

	if (mindiffrate != 0xFFFFFFFFFFFFFFFF) {
		rate_table->rate = min_fout;
		rate_table->pdiv = min_p;
		rate_table->mdiv = min_m;
		rate_table->sdiv = min_s;
		rate_table->kdiv = 0;
		return 0;
	}
	return -1;
}


static int _clk_pll1419x_find_pms(struct pll_spec *pll_spec,
				struct pwrcal_pll_rate_table *rate_table,
				unsigned long long rate)
{
	unsigned int p, m, s;
	unsigned long long fref, fvco, fout;
	unsigned long long tmprate, tmpfout;
	unsigned long long mindiffrate = 0xFFFFFFFFFFFFFFFF;
	unsigned int min_p, min_m, min_s, min_fout;

	for (p = pll_spec->pdiv_min; p <= pll_spec->pdiv_max; p++) {
		fref = FIN_HZ_26M / p;
		if ((fref < pll_spec->fref_min) || (fref > pll_spec->fref_max))
			continue;

		for (s = pll_spec->sdiv_min; s <= pll_spec->sdiv_max; s++) {
			/*tmprate = rate;*/
			tmprate = rate/2; /*for PLL1419*/
			do_div(tmprate, KHZ);
			tmprate = tmprate * p * (1 << s);
			do_div(tmprate, (FIN_HZ_26M / KHZ));
			m = (unsigned int)tmprate;

			if ((m < pll_spec->mdiv_min)
					|| (m > pll_spec->mdiv_max))
				continue;

			fvco = ((unsigned long long)FIN_HZ_26M) * m;
			do_div(fvco, p);
			if ((fvco < pll_spec->fvco_min)
					|| (fvco > pll_spec->fvco_max))
				continue;

			fout = fvco >> s;
			tmpfout = 0;
			if ((fout >= pll_spec->fout_min)
					&& (fout <= pll_spec->fout_max)) {
				tmprate = rate;
				do_div(tmprate, KHZ);
				tmpfout = fout;
				do_div(tmpfout, KHZ);
				if (tmprate == tmpfout) {
					rate_table->rate = fout;
					rate_table->pdiv = p;
					rate_table->mdiv = m;
					rate_table->sdiv = s;
					rate_table->kdiv = 0;
					return 0;
				}
			}
			if (tmpfout && tmpfout < tmprate && mindiffrate > tmprate - tmpfout) {
				mindiffrate = tmprate - tmpfout;
				min_fout = fout;
				min_p = p;
				min_m = m;
				min_s = s;
			}
		}
	}

	if (mindiffrate != 0xFFFFFFFFFFFFFFFF) {
		rate_table->rate = min_fout;
		rate_table->pdiv = min_p;
		rate_table->mdiv = min_m;
		rate_table->sdiv = min_s;
		rate_table->kdiv = 0;
		return 0;
	}
	return -1;
}

static int _clk_pll141xx_is_enabled(struct pwrcal_clk *clk)
{
	return (int)(pwrcal_getbit(clk->offset, PLL141XX_ENABLE));
}

static int _clk_pll141xx_enable(struct pwrcal_clk *clk)
{
	int timeout;

	if (pwrcal_getbit(clk->offset, PLL141XX_ENABLE))
		return 0;

	pwrcal_setbit(clk->offset, PLL141XX_ENABLE, 1);

	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(clk->offset, PLL141XX_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}

	return 0;
}

static int _clk_pll1419x_enable(struct pwrcal_clk *clk)
{
	int timeout;

	if (pwrcal_getbit(clk->offset, PLL141XX_ENABLE))
		return 0;

	pwrcal_setbit(clk->offset, PLL141XX_ENABLE, 1);

	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(MIF0_PLL_CON0, PLL141XX_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}
	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(MIF1_PLL_CON0, PLL141XX_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}
	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(MIF2_PLL_CON0, PLL141XX_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}
	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(MIF3_PLL_CON0, PLL141XX_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}

	return 0;
}


static int _clk_pll141xx_disable(struct pwrcal_clk *clk)
{
	pwrcal_setbit(clk->offset, PLL141XX_ENABLE, 0);

	return 0;
}

int _clk_pll141xx_is_disabled_bypass(struct pwrcal_clk *clk)
{
	if (pwrcal_getbit(clk->offset + 1, PLL141XX_BYPASS))
		return 0;

	return 1;
}


int _clk_pll141xx_set_bypass(struct pwrcal_clk *clk, int bypass_disable)
{
	if (bypass_disable == 0)
		pwrcal_setbit(clk + 1, PLL141XX_BYPASS, 1);
	else
		pwrcal_setbit(clk + 1, PLL141XX_BYPASS, 0);

	return 0;
}

static int _clk_pll141xx_set_pms(struct pwrcal_clk *clk,
				const struct pwrcal_pll_rate_table  *rate_table)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;

	int timeout;

	pdiv = rate_table->pdiv;
	mdiv = rate_table->mdiv;
	sdiv = rate_table->sdiv;

	pll_con0 = pwrcal_readl(clk->offset);
	pll_con0 &= ~((PLL141XX_MDIV_MASK << PLL141XX_MDIV_SHIFT)
			| (PLL141XX_PDIV_MASK << PLL141XX_PDIV_SHIFT)
			| (PLL141XX_SDIV_MASK << PLL141XX_SDIV_SHIFT));
	pll_con0 |=  (mdiv << PLL141XX_MDIV_SHIFT)
			| (pdiv << PLL141XX_PDIV_SHIFT)
			| (sdiv << PLL141XX_SDIV_SHIFT);
	pll_con0 &= ~(1<<26);
	pll_con0 |= (1<<5);

	pwrcal_writel(clk->status, pdiv*150);
	pwrcal_writel(clk->offset, pll_con0);

	if (pll_con0 & (1 << PLL141XX_ENABLE)) {
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(clk->offset, PLL141XX_LOCKED))
				break;
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
	}

	return 0;

}

static int _clk_pll1419x_set_pms(struct pwrcal_clk *clk,
				const struct pwrcal_pll_rate_table  *rate_table)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;

	int timeout;

	pdiv = rate_table->pdiv;
	mdiv = rate_table->mdiv;
	sdiv = rate_table->sdiv;

	pll_con0 = pwrcal_readl(clk->offset);
	pll_con0 &= ~((PLL141XX_MDIV_MASK << PLL141XX_MDIV_SHIFT)
			| (PLL141XX_PDIV_MASK << PLL141XX_PDIV_SHIFT)
			| (PLL141XX_SDIV_MASK << PLL141XX_SDIV_SHIFT));
	pll_con0 |=  (mdiv << PLL141XX_MDIV_SHIFT)
			| (pdiv << PLL141XX_PDIV_SHIFT)
			| (sdiv << PLL141XX_SDIV_SHIFT);

	pwrcal_writel(MIF0_PLL_LOCK, pdiv*150);
	pwrcal_writel(MIF1_PLL_LOCK, pdiv*150);
	pwrcal_writel(MIF2_PLL_LOCK, pdiv*150);
	pwrcal_writel(MIF3_PLL_LOCK, pdiv*150);
	pwrcal_writel(clk->offset, pll_con0);

	if (pll_con0 & (1 << PLL141XX_ENABLE)) {
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(MIF0_PLL_CON0, PLL141XX_LOCKED))
				break;
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(MIF1_PLL_CON0, PLL141XX_LOCKED))
				break;
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(MIF2_PLL_CON0, PLL141XX_LOCKED))
				break;
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(MIF3_PLL_CON0, PLL141XX_LOCKED))
				break;
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
	}

	return 0;

}


static int _clk_pll141xx_set_rate(struct pwrcal_clk *clk,
					unsigned long long rate)
{
	struct pwrcal_pll *pll = to_pll(clk);
	struct pll_spec *pll_spec;
	struct pwrcal_pll_rate_table  tmp_rate_table;
	const struct pwrcal_pll_rate_table  *rate_table;

	if (rate == 0) {
		if (_clk_pll141xx_is_enabled(clk) != 0)
			if (_clk_pll141xx_disable(clk))
				goto errorout;
		return 0;
	}

	rate_table = _clk_get_pll_settings(pll, rate);
	if (rate_table == NULL) {
		pll_spec = clk_pll_get_spec(clk);
		if (pll_spec == NULL)
			goto errorout;

		if (_clk_pll141xx_find_pms(pll_spec, &tmp_rate_table, rate)) {
			pr_err("can't find pms value for rate(%lldHz) of \'%s\'",
				rate,
				clk->name);
			goto errorout;
		}

		rate_table = &tmp_rate_table;

		pr_warn("not exist in rate table, p(%d), m(%d), s(%d), fout(%lldHz) %s",
				rate_table->pdiv,
				rate_table->mdiv,
				rate_table->sdiv,
				rate,
				clk->name);
	}

	if (_clk_pll141xx_set_pms(clk, rate_table))
		goto errorout;

	if (rate != 0) {
		if (_clk_pll141xx_is_enabled(clk) == 0)
			_clk_pll141xx_enable(clk);
	}
	return 0;

errorout:
	return -1;
}

static int _clk_pll1419x_set_rate(struct pwrcal_clk *clk,
					unsigned long long rate)
{
	struct pwrcal_pll *pll = to_pll(clk);
	struct pll_spec *pll_spec;
	struct pwrcal_pll_rate_table  tmp_rate_table;
	const struct pwrcal_pll_rate_table  *rate_table;

	if (rate == 0) {
		if (_clk_pll141xx_is_enabled(clk) != 0)
			if (_clk_pll141xx_disable(clk))
				goto errorout;
		return 0;
	}

	rate_table = _clk_get_pll_settings(pll, rate);
	if (rate_table == NULL) {
		pll_spec = clk_pll_get_spec(clk);
		if (pll_spec == NULL)
			goto errorout;

		if (_clk_pll1419x_find_pms(pll_spec, &tmp_rate_table, rate)) {
			pr_err("can't find pms value for rate(%lldHz) of \'%s\'",
				rate,
				clk->name);
			goto errorout;
		}

		rate_table = &tmp_rate_table;

		pr_warn("not exist in rate table, p(%d), m(%d), s(%d), fout(%lldHz) %s",
				rate_table->pdiv,
				rate_table->mdiv,
				rate_table->sdiv,
				rate,
				clk->name);
	}

	_clk_pll141xx_disable(clk);

	if (_clk_pll1419x_set_pms(clk, rate_table))
		goto errorout;

	if (rate != 0) {
		if (_clk_pll141xx_is_enabled(clk) == 0)
			_clk_pll1419x_enable(clk);
	}
	return 0;

errorout:
	return -1;
}


static unsigned long long _clk_pll141xx_get_rate(struct pwrcal_clk *clk)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;
	unsigned long long fout;

	if (_clk_pll141xx_is_enabled(clk) == 0)
		return 0;
	pll_con0 = pwrcal_readl(clk->offset);
	mdiv = (pll_con0 >> PLL141XX_MDIV_SHIFT) & PLL141XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL141XX_PDIV_SHIFT) & PLL141XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL141XX_SDIV_SHIFT) & PLL141XX_SDIV_MASK;

	if (pdiv == 0) {
		pr_err("pdiv is 0, id(%s)", clk->name);
		return 0;
	}
	fout = FIN_HZ_26M * mdiv;
	do_div(fout, (pdiv << sdiv));
	return (unsigned long long)fout;
}

static unsigned long long _clk_pll1419x_get_rate(struct pwrcal_clk *clk)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;
	unsigned long long fout;

	if (_clk_pll141xx_is_enabled(clk) == 0)
		return 0;

	pll_con0 = pwrcal_readl(clk->offset);
	mdiv = (pll_con0 >> PLL141XX_MDIV_SHIFT) & PLL141XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL141XX_PDIV_SHIFT) & PLL141XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL141XX_SDIV_SHIFT) & PLL141XX_SDIV_MASK;

	if (pdiv == 0) {
		pr_err("pdiv is 0, id(%s)", clk->name);
		return 0;
	}
	fout = FIN_HZ_26M * 2 * mdiv;
	do_div(fout, (pdiv << sdiv));
	return (unsigned long long)fout;
}

static int _clk_pll141xx_mfc_is_enabled(struct pwrcal_clk *clk)
{
	if (clk->id == MFC_PLL_GATE && mfc_pll_refcount[0])
		return 1;

	if (clk->id == MFC_PLL_DVFS && mfc_pll_refcount[1])
		return 1;

	return 0;
}

static int _clk_pll141xx_mfc_enable(struct pwrcal_clk *clk)
{
	int timeout;

	if (clk->id == MFC_PLL_GATE)
		mfc_pll_refcount[0] = 1;

	if (clk->id == MFC_PLL_DVFS)
		mfc_pll_refcount[1] = 1;

	if (mfc_pll_refcount[0] + mfc_pll_refcount[1] < 2)
		return 0;

	if (pwrcal_getbit(clk->offset, PLL141XX_ENABLE))
		return 0;

	pwrcal_setbit(clk->offset, PLL141XX_ENABLE, 1);

	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(clk->offset, PLL141XX_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}

	if (pwrcal_mux_set_src(CLK(TOP_MUX_MFC_PLL), 1))
		return -1;

	return 0;
}

static int _clk_pll141xx_mfc_disable(struct pwrcal_clk *clk)
{
	if (clk->id == MFC_PLL_GATE)
		mfc_pll_refcount[0] = 0;

	if (clk->id == MFC_PLL_DVFS)
		mfc_pll_refcount[1] = 0;

	if (pwrcal_mux_set_src(CLK(TOP_MUX_MFC_PLL), 0))
		return -1;

	pwrcal_setbit(clk->offset, PLL141XX_ENABLE, 0);

	return 0;
}

static int _clk_pll141xx_mfc_set_rate(struct pwrcal_clk *clk,
					unsigned long long rate)
{
	struct pwrcal_pll *pll = to_pll(clk);
	struct pll_spec *pll_spec;
	struct pwrcal_pll_rate_table  tmp_rate_table;
	const struct pwrcal_pll_rate_table  *rate_table;

	if (rate == 0) {
		if (_clk_pll141xx_mfc_is_enabled(clk) != 0)
			if (_clk_pll141xx_mfc_disable(clk))
				goto errorout;
		return 0;
	}

	rate_table = _clk_get_pll_settings(pll, rate);
	if (rate_table == NULL) {
		pll_spec = clk_pll_get_spec(clk);
		if (pll_spec == NULL)
			goto errorout;

		if (_clk_pll141xx_find_pms(pll_spec, &tmp_rate_table, rate)) {
			pr_err("can't find pms value for rate(%lldHz) of \'%s\'",
				rate,
				clk->name);
			goto errorout;
		}

		rate_table = &tmp_rate_table;

		pr_warn("not exist in rate table, p(%d), m(%d), s(%d), fout(%lldHz) %s",
				rate_table->pdiv,
				rate_table->mdiv,
				rate_table->sdiv,
				rate,
				clk->name);
	}

	if (_clk_pll141xx_set_pms(clk, rate_table))
		goto errorout;

	if (rate != 0) {
		if (_clk_pll141xx_mfc_is_enabled(clk) == 0)
			_clk_pll141xx_mfc_enable(clk);
	}
	return 0;

errorout:
	return -1;
}

static unsigned long long _clk_pll141xx_mfc_get_rate(struct pwrcal_clk *clk)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;
	unsigned long long fout;

	if (_clk_pll141xx_mfc_is_enabled(clk) == 0)
		return 0;
	pll_con0 = pwrcal_readl(clk->offset);
	mdiv = (pll_con0 >> PLL141XX_MDIV_SHIFT) & PLL141XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL141XX_PDIV_SHIFT) & PLL141XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL141XX_SDIV_SHIFT) & PLL141XX_SDIV_MASK;

	if (pdiv == 0) {
		pr_err("pdiv is 0, id(%s)", clk->name);
		return 0;
	}
	fout = FIN_HZ_26M * mdiv;
	do_div(fout, (pdiv << sdiv));
	return (unsigned long long)fout;
}

static int _clk_pll1431x_find_pms(struct pll_spec *pll_spec,
					struct pwrcal_pll_rate_table *rate_table,
					unsigned long long rate)
{
	unsigned int p, m, s;
	signed short k;
	unsigned long long fref, fvco, fout;
	unsigned long long tmprate, tmpfout;

	for (p = pll_spec->pdiv_min; p <= pll_spec->pdiv_max; p++) {
		fref = FIN_HZ_26M / p;
		if ((fref < pll_spec->fref_min) || (fref > pll_spec->fref_max))
			continue;

		for (s = pll_spec->sdiv_min; s <= pll_spec->sdiv_max; s++) {
			tmprate = rate;
			do_div(tmprate, KHZ);
			tmprate = tmprate * p * (1 << s);
			do_div(tmprate, (FIN_HZ_26M / KHZ));
			m = (unsigned int)tmprate;

			if ((m < pll_spec->mdiv_min)
					|| (m > pll_spec->mdiv_max))
				continue;

			tmprate = rate;
			do_div(tmprate, KHZ);
			tmprate = tmprate * p * (1 << s);
			do_div(tmprate, (FIN_HZ_26M / KHZ));
			tmprate = (tmprate - m) * 65536;
			k = (unsigned int)tmprate;
			if ((k < pll_spec->kdiv_min)
					|| (k > pll_spec->kdiv_max))
				continue;

			fvco = FIN_HZ_26M * ((m << 16) + k);
			do_div(fvco, p);
			fvco >>= 16;
			if ((fvco < pll_spec->fvco_min)
					|| (fvco > pll_spec->fvco_max))
				continue;

			fout = fvco >> s;
			if ((fout >= pll_spec->fout_min)
				&& (fout <= pll_spec->fout_max)) {
				tmprate = rate;
				do_div(tmprate, KHZ);
				tmpfout = fout;
				do_div(tmpfout, KHZ);
				if (tmprate == tmpfout) {
					rate_table->rate = fout;
					rate_table->pdiv = p;
					rate_table->mdiv = m;
					rate_table->sdiv = s;
					rate_table->kdiv = k;
					return 0;
				}
			}
		}
	}

	return -1;
}

static int _clk_pll1431x_is_enabled(struct pwrcal_clk *clk)
{
	return (int)(pwrcal_getbit(clk->offset, PLL1431X_ENABLE));
}

static int _clk_pll1431x_enable(struct pwrcal_clk *clk)
{
	int timeout;

	if (pwrcal_getbit(clk->offset, PLL1431X_ENABLE))
		return 0;

	pwrcal_setbit(clk->offset, PLL1431X_ENABLE, 1);

	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(clk->offset, PLL1431X_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}

	return 0;
}

static int _clk_pll1431x_disable(struct pwrcal_clk *clk)
{
	pwrcal_setbit(clk->offset, PLL1431X_ENABLE, 0);

	return 0;
}

int _clk_pll1431x_is_disabled_bypass(struct pwrcal_clk *clk)
{
	if (pwrcal_getbit(clk->offset + 2, PLL1431X_BYPASS))
		return 0;

	return 1;
}

int _clk_pll1431x_set_bypass(struct pwrcal_clk *clk, int bypass_disable)
{
	if (bypass_disable == 0)
		pwrcal_setbit(clk->offset + 2, PLL1431X_BYPASS, 1);
	else
		pwrcal_setbit(clk->offset + 2, PLL1431X_BYPASS, 0);

	return 0;
}

static int _clk_pll1431x_set_pms(struct pwrcal_clk *clk,
			const struct pwrcal_pll_rate_table  *rate_table)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0, pll_con1;
	signed short kdiv;
	int timeout;

	pdiv = rate_table->pdiv;
	mdiv = rate_table->mdiv;
	sdiv = rate_table->sdiv;
	kdiv = rate_table->kdiv;

	pll_con0 = pwrcal_readl(clk->offset);
	pll_con1 = pwrcal_readl(clk->offset + 1);
	pll_con0 &= ~((PLL1431X_MDIV_MASK << PLL1431X_MDIV_SHIFT)
			| (PLL1431X_PDIV_MASK << PLL1431X_PDIV_SHIFT)
			| (PLL1431X_SDIV_MASK << PLL1431X_SDIV_SHIFT));
	pll_con0 |=  (mdiv << PLL1431X_MDIV_SHIFT)
			| (pdiv << PLL1431X_PDIV_SHIFT)
			| (sdiv << PLL1431X_SDIV_SHIFT);
	pll_con0 &= ~(1<<26);
	pll_con0 |= (1<<5);

	pll_con1 &= ~(PLL1431X_K_MASK << PLL1431X_K_SHIFT);
	pll_con1 |=  (kdiv << PLL1431X_K_SHIFT);

	if (kdiv == 0)
		pwrcal_writel(clk->status, pdiv*3000);
	else
		pwrcal_writel(clk->status, pdiv*3000);
	pwrcal_writel(clk->offset, pll_con0);
	pwrcal_writel(clk->offset + 1, pll_con1);

	if (pll_con0 & (1 << PLL1431X_ENABLE)) {
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(clk->offset, PLL1431X_LOCKED))
				break;
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
	}

	return 0;
}

static int _clk_pll1431x_set_rate(struct pwrcal_clk *clk,
					unsigned long long rate)
{
	struct pwrcal_pll *pll = to_pll(clk);
	struct pwrcal_pll_rate_table  tmp_rate_table;
	const struct pwrcal_pll_rate_table  *rate_table;
	struct pll_spec *pll_spec;

	if (rate == 0) {
		if (_clk_pll1431x_is_enabled(clk) != 0)
			if (_clk_pll1431x_disable(clk))
				goto errorout;
		return 0;
	}

	rate_table = _clk_get_pll_settings(pll, rate);
	if (rate_table == NULL) {
		pll_spec = clk_pll_get_spec(clk);
		if (pll_spec == NULL)
			goto errorout;

		if (_clk_pll1431x_find_pms(pll_spec, &tmp_rate_table, rate) < 0) {
			pr_err("can't find pms value for rate(%lldHz) of %s",
				rate,
				clk->name);
			goto errorout;
		}

		rate_table = &tmp_rate_table;
		pr_warn("not exist in rate table, p(%d) m(%d) s(%d) k(%d) fout(%lld Hz) of %s",
				rate_table->pdiv,
				rate_table->mdiv,
				rate_table->sdiv,
				rate_table->kdiv,
				rate,
				clk->name);
	}

	if (_clk_pll1431x_set_pms(clk, rate_table))
		goto errorout;

	if (rate != 0) {
		if (_clk_pll1431x_is_enabled(clk) == 0)
			_clk_pll1431x_enable(clk);
	}

	return 0;

errorout:
	return -1;
}

static unsigned long long _clk_pll1431x_get_rate(struct pwrcal_clk *clk)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0, pll_con1;
	signed short kdiv;
	unsigned long long fout;

	if (_clk_pll1431x_is_enabled(clk) == 0)
		return 0;

	pll_con0 = pwrcal_readl(clk->offset);
	pll_con1 = pwrcal_readl(clk->offset + 1);
	mdiv = (pll_con0 >> PLL1431X_MDIV_SHIFT) & PLL1431X_MDIV_MASK;
	pdiv = (pll_con0 >> PLL1431X_PDIV_SHIFT) & PLL1431X_PDIV_MASK;
	sdiv = (pll_con0 >> PLL1431X_SDIV_SHIFT) & PLL1431X_SDIV_MASK;

	kdiv = (short)(pll_con1 >> PLL1431X_K_SHIFT) & PLL1431X_K_MASK;

	if (pdiv == 0) {
		pr_err("pdiv is 0, id(%s)", clk->name);
		return 0;
	}

	fout = FIN_HZ_26M * ((mdiv << 16) + kdiv);
	do_div(fout, (pdiv << sdiv));
	fout >>= 16;

	return (unsigned long long)fout;
}

struct pwrcal_pll_ops pll141xx_ops = {
	.is_enabled = _clk_pll141xx_is_enabled,
	.enable = _clk_pll141xx_enable,
	.disable = _clk_pll141xx_disable,
	.set_rate = _clk_pll141xx_set_rate,
	.get_rate = _clk_pll141xx_get_rate,
};

struct pwrcal_pll_ops pll141xx_mfc_ops = {
	.is_enabled = _clk_pll141xx_mfc_is_enabled,
	.enable = _clk_pll141xx_mfc_enable,
	.disable = _clk_pll141xx_mfc_disable,
	.set_rate = _clk_pll141xx_mfc_set_rate,
	.get_rate = _clk_pll141xx_mfc_get_rate,
};

struct pwrcal_pll_ops pll1419x_ops = {
	.is_enabled = _clk_pll141xx_is_enabled,
	.enable = _clk_pll1419x_enable,
	.disable = _clk_pll141xx_disable,
	.set_rate = _clk_pll1419x_set_rate,
	.get_rate = _clk_pll1419x_get_rate,
};

struct pwrcal_pll_ops pll1431x_ops = {
	.is_enabled = _clk_pll1431x_is_enabled,
	.enable = _clk_pll1431x_enable,
	.disable = _clk_pll1431x_disable,
	.set_rate = _clk_pll1431x_set_rate,
	.get_rate = _clk_pll1431x_get_rate,
};
