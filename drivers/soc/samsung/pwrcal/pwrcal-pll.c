#include "pwrcal.h"
#include "pwrcal-clk.h"
#include "pwrcal-vclk.h"
#include "pwrcal-env.h"
#include "pwrcal-rae.h"

enum clk_pll_type {
	pll_1416x = 14160,
	pll_1417x = 14170,
	pll_1418x = 14180,
	pll_1419x = 14190,
	pll_1431x = 14310,
	pll_1450x = 14500,
	pll_1451x = 14510,
	pll_1452x = 14520,
	pll_1460x = 14600,
};


static struct pll_spec gpll1416X_spec = {
	1,	63,
	64,	1023,
	0,	6,
	0,	0,
	4*MHZ,	12*MHZ,
	1600*MHZ,	3200*MHZ,
	200*MHZ,	3200*MHZ,
};

static struct pll_spec gpll1417X_spec = {
	1,	63,
	64,	1023,
	0,	5,
	0,	0,
	4*MHZ,	12*MHZ,
	1000*MHZ,	2000*MHZ,
	200*MHZ,	2000*MHZ,
};

static struct pll_spec gpll1418X_spec = {
	1,	63,
	16,	511,
	0,	5,
	0,	0,
	2*MHZ,	8*MHZ,
	600*MHZ,	1200*MHZ,
	100*MHZ,	1200*MHZ,
};

static struct pll_spec gpll1419X_spec = {
	1,	63,
	64,	1023,
	0,	6,
	0,	0,
	4*MHZ,	12*MHZ,
	1900*MHZ,	3800*MHZ,
	200*MHZ,	3800*MHZ,
};

static struct pll_spec gpll1431X_spec = {
	1,	63,
	16,	511,
	0,	5,
	-32767,	32767,
	6*MHZ,	30*MHZ,
	400*MHZ,	800*MHZ,
	50*MHZ,	800*MHZ,
};

static struct pll_spec gpll1450X_spec = {
	1,		63,
	64,	1023,
	0,		6,
	0,		0,
	4*MHZ,	12*MHZ,
	1600*MHZ,	3304*MHZ,
	25*MHZ,		3304*MHZ,
};

static struct pll_spec gpll1451X_spec = {
	1,		63,
	64,	1023,
	0,		5,
	0,		0,
	4*MHZ,		12*MHZ,
	900*MHZ,	1800*MHZ,
	30*MHZ,		1800*MHZ,
};

static struct pll_spec gpll1452X_spec = {
	1,		63,
	16,	511,
	0,		5,
	0,		0,
	4*MHZ,		8*MHZ,
	500*MHZ,	1000*MHZ,
	16*MHZ,		1000*MHZ,
};

static struct pll_spec gpll1460X_spec = {
	1,		63,
	16,	511,
	0,		5,
	-32767,	32767,
	6*MHZ,		30*MHZ,
	400*MHZ,	800*MHZ,
	12.5*MHZ,	800*MHZ,
};

struct pll_spec *clk_pll_get_spec(struct pwrcal_clk *clk)
{
	struct pwrcal_pll *pll = to_pll(clk);
	struct pll_spec *pll_spec;

	switch (pll->type) {
	case pll_1416x:
		pll_spec = &gpll1416X_spec;
		break;
	case pll_1417x:
		pll_spec = &gpll1417X_spec;
		break;
	case pll_1418x:
		pll_spec = &gpll1418X_spec;
		break;
	case pll_1419x:
		pll_spec = &gpll1419X_spec;
		break;
	case pll_1431x:
		pll_spec = &gpll1431X_spec;
		break;
	case pll_1450x:
		pll_spec = &gpll1450X_spec;
		break;
	case pll_1451x:
		pll_spec = &gpll1451X_spec;
		break;
	case pll_1452x:
		pll_spec = &gpll1452X_spec;
		break;
	case pll_1460x:
		pll_spec = &gpll1460X_spec;
		break;
	default:
		return NULL;
	}

	return pll_spec;
}

unsigned long long clk_pll_getmaxfreq(struct pwrcal_clk *clk)
{
	struct pll_spec *pll_spec;

	pll_spec = clk_pll_get_spec(clk);
	if (pll_spec == NULL)
		return 0;

	return pll_spec->fout_max;
}


int pwrcal_pll_is_enabled(struct pwrcal_clk *clk)
{
	struct pwrcal_pll *pll = to_pll(clk);

	if (pll->ops->is_enabled(clk) == 0)
		return 0;

	if (pll->mux != CLK_NONE)
		if (pwrcal_mux_get_src(pll->mux) == 0)
			return 0;

	return 1;
}

int pwrcal_pll_enable(struct pwrcal_clk *clk)
{
	struct pwrcal_pll *pll = to_pll(clk);

	if (pll->ops->enable(clk))
		goto errorout;

	if (pll->mux != CLK_NONE)
		if (pwrcal_mux_set_src(pll->mux, 1))
			goto errorout;

	return 0;

errorout:
	return -1;
}

int pwrcal_pll_disable(struct pwrcal_clk *clk)
{
	struct pwrcal_pll *pll = to_pll(clk);

	if (pll->mux != CLK_NONE)
		if (pwrcal_mux_set_src(pll->mux, 0))
			goto errorout;

	if (pll->ops->disable(clk))
		goto errorout;

	return 0;

errorout:
	return -1;
}

unsigned long long pwrcal_pll_get_rate(struct pwrcal_clk *clk)
{
	struct pwrcal_pll *pll = to_pll(clk);

	if (pll->mux != CLK_NONE)
		if (pwrcal_mux_get_src(pll->mux) == 0)
			return 0;

	return pll->ops->get_rate(clk);
}

int pwrcal_pll_set_rate(struct pwrcal_clk *clk, unsigned long long rate)
{
	struct pwrcal_pll *pll = to_pll(clk);

	if (pll->mux != CLK_NONE && rate == 0) {
		if (pwrcal_mux_get_src(pll->mux) != 0)
			pwrcal_mux_set_src(pll->mux, 0);
		return pll->ops->disable(clk);
	}


	if (pll->ops->set_rate(clk, rate))
		goto errorout;


	if (pll->mux != CLK_NONE && rate != 0)
		if (pwrcal_mux_get_src(pll->mux) != 1)
			return pwrcal_mux_set_src(pll->mux, 1);

	return 0;

errorout:
	return -1;
}
