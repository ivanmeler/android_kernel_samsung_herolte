#ifndef __PWRCAL_CLK_H__
#define __PWRCAL_CLK_H__

#ifndef NULL
#define NULL			(0)
#endif

#ifndef MHZ
#define MHZ			((unsigned long long)1000000)
#endif
#ifndef KHZ
#define KHZ			((unsigned int)1000)
#endif

#ifndef FIN_HZ
#if defined(CONFIG_BOARD_FPGA)
	#define FIN_HZ		(5*MHZ)
#else
	#define FIN_HZ		(24*MHZ)
#endif
#endif

#define CLK_WAIT_CNT		1000

#define TO_MASK(_size)	(((unsigned int)1<<(_size))-1)

#define invalid_mux_src_num	0xFF
#define mux_flag_user_mux	(1<<0)
#define mux_flag_alwayson	(1<<1)


#define	mask_of_type		0x0F000000
#define	mask_of_group		0x00FF0000
#define	mask_of_id		0x00000FFF
#define	fixed_rate_type		0x01000000
#define	fixed_factor_type	0x02000000
#define	pll_type			0x05000000
#define	mux_type		0x06000000
#define	user_mux_type		0x06001000
#define	div_type		0x07000000
#define	gate_type		0x08000000
#define	gate_root_type		0x09000000
#define	vclk_type		0x0A000000
#define	invalid_clk_id		mask_of_id
#define	empty_clk_id		mask_of_id

#define is_pll(a)			(a && (a->id & mask_of_type) == pll_type)
#define is_mux(a)		(a && (a->id & mask_of_type) == mux_type)
#define is_div(a)			(a && (a->id & mask_of_type) == div_type)
#define is_gate(a)		(a && (a->id & mask_of_type) == gate_type)

enum clk_id;

struct pwrcal_clk {
	unsigned int	id;
	struct pwrcal_clk	*parent;
	unsigned int	*offset;
	unsigned short	shift;
	unsigned short	width;
	unsigned int	*status;		/* or lock_offset of pll */
	unsigned short	s_shift;
	unsigned short	s_width;
	unsigned int	*enable;
	unsigned short	e_shift;
	unsigned short	e_width;
	const char	*name;
};

struct pwrcal_clk_none {
	struct pwrcal_clk clk;
};
extern struct pwrcal_clk_none clk_0;
#define CLK_NONE	(&(clk_0.clk))
#define CLK(_id)		((struct pwrcal_clk *)(&((clk_##_id).clk)))


struct pll_spec {
	unsigned int pdiv_min;
	unsigned int pdiv_max;
	unsigned int mdiv_min;
	unsigned int mdiv_max;
	unsigned int sdiv_min;
	unsigned int sdiv_max;
	signed short kdiv_min;
	signed short kdiv_max;
	unsigned long long fref_min;
	unsigned long long fref_max;
	unsigned long long fvco_min;
	unsigned long long fvco_max;
	unsigned long long fout_min;
	unsigned long long fout_max;
};

struct pwrcal_pll_rate_table {
	unsigned long long	rate;
	unsigned short	pdiv;
	unsigned short	mdiv;
	unsigned short	sdiv;
	signed short	kdiv;
};

struct pwrcal_pll_ops {
	int (*is_enabled)(struct pwrcal_clk *clk);
	int (*enable)(struct pwrcal_clk *clk);
	int (*disable)(struct pwrcal_clk *clk);
	int (*set_rate)(struct pwrcal_clk *clk, unsigned long long rate);
	unsigned long long (*get_rate)(struct pwrcal_clk *clk);
};

struct pwrcal_pll {
	struct pwrcal_clk	clk;
	unsigned int	type;
	struct pwrcal_pll_rate_table *rate_table;
	unsigned int	rate_count;
	struct pwrcal_clk	*mux;
	struct pwrcal_pll_ops *ops;
};

#define CLK_PLL(_typ, _id, _pid, _lock, _con, _rtable, _mux, _ops)	\
struct pwrcal_pll clk_##_id __attribute__((unused, aligned(8), section(".clk_pll."))) = {	\
	.clk.id		= _id,					\
	.clk.parent	= &((clk_##_pid).clk),			\
	.clk.offset	= _con,					\
	.clk.status	= _lock,					\
	.clk.name	= #_id,					\
	.type		= _typ,					\
	.rate_table	= _rtable,				\
	.rate_count	= (sizeof(_rtable) / sizeof((_rtable)[0])),	\
	.mux		= &((clk_##_mux).clk),			\
	.ops		= _ops					\
}


extern struct pwrcal_clk_ops frate_ops;
struct pwrcal_clk_fixed_rate {
	struct pwrcal_clk	clk;
	unsigned long long	fixed_rate;
	struct pwrcal_clk	*gate;
};

#define FIXEDRATE(_id, _frate, _gate)			\
struct pwrcal_clk_fixed_rate clk_##_id __attribute__((unused, aligned(8), section(".clk_fixed_rate."))) = {	\
	.clk.id		= _id,				\
	.clk.parent	= &(clk_0.clk),			\
	.clk.name	= #_id,				\
	.fixed_rate	= _frate,			\
	.gate		= &((clk_##_gate).clk),		\
}


extern struct pwrcal_clk_ops fdiv_ops;
struct pwrcal_clk_fixed_factor {
	struct pwrcal_clk	clk;
	unsigned short	ratio;
	struct pwrcal_clk	*gate;
};

#define FIXEDFACTOR(_id, _pid, _ratio, _gate)		\
struct pwrcal_clk_fixed_factor clk_##_id __attribute__((unused, aligned(8), section(".clk_fixed_factor."))) = {	\
	.clk.id		= _id,				\
	.clk.parent	= &((clk_##_pid).clk),		\
	.clk.name	= #_id,				\
	.ratio		= _ratio,			\
	.gate		= &((clk_##_gate).clk),		\
}


extern struct pwrcal_clk_ops mux_ops;
struct pwrcal_mux {
	struct pwrcal_clk	clk;
	struct pwrcal_clk **parents;
	unsigned char	num_parents;
	struct pwrcal_clk	*gate;
};

#define CLK_MUX(_id, _pids, _o, _s, _w, _so, _ss, _sw,	\
					_eo, _es, _gate)	\
struct pwrcal_mux clk_##_id __attribute__((unused, aligned(8), section(".clk_mux."))) = {	\
	.clk.id		= _id,				\
	.clk.name	= #_id,				\
	.clk.offset	= _o,				\
	.clk.shift		= _s,				\
	.clk.width		= _w,				\
	.clk.status	= _so,				\
	.clk.s_shift	= _ss,				\
	.clk.s_width	= _sw,				\
	.clk.enable	= _eo,				\
	.clk.e_shift	= _es,				\
	.parents		= _pids,				\
	.num_parents	= (sizeof(_pids) / sizeof((_pids)[0])), \
	.gate		= &((clk_##_gate).clk),		\
}


extern struct pwrcal_clk_ops div_ops;
struct pwrcal_div {
	struct pwrcal_clk	clk;
	struct pwrcal_clk	*gate;
};

#define CLK_DIV(_id, _pid, _o, _s, _w, _so, _ss, _sw, _gate)	\
struct pwrcal_div clk_##_id __attribute__((unused, aligned(8), section(".clk_div."))) = {	\
	.clk.id		= _id,				\
	.clk.name	= #_id,				\
	.clk.parent	= &((clk_##_pid).clk),		\
	.clk.offset	= _o,				\
	.clk.shift		= _s,				\
	.clk.width		= _w,				\
	.clk.status	= _so,				\
	.clk.s_shift	= _ss,				\
	.clk.s_width	= _sw,				\
	.gate		= &((clk_##_gate).clk),		\
}


extern struct pwrcal_clk_ops gate_ops;
struct pwrcal_gate {
	struct pwrcal_clk	clk;
};

#define CLK_GATE(_id, _pid, _o, _s)			\
struct pwrcal_gate clk_##_id __attribute__((unused, aligned(8), section(".clk_gate."))) = {	\
	.clk.id		= _id,				\
	.clk.name	= #_id,				\
	.clk.parent	= &((clk_##_pid).clk),		\
	.clk.offset	= _o,				\
	.clk.shift		= _s,				\
}

#define PLL_EXTERN(_id)		\
	extern struct pwrcal_pll clk_##_id;
#define FIXEDRATE_EXTERN(_id)	\
	extern struct pwrcal_clk_fixed_rate clk_##_id;
#define FIXEDFACTOR_EXTERN(_id)	\
	extern struct pwrcal_clk_fixed_factor clk_##_id;
#define MUX_EXTERN(_id)		\
	extern struct pwrcal_mux clk_##_id;
#define DIV_EXTERN(_id)		\
	extern struct pwrcal_div clk_##_id;
#define GATE_EXTERN(_id)	\
	extern struct pwrcal_gate clk_##_id;


#define to_pll(_clk) container_of(_clk, struct pwrcal_pll, clk)
#define to_frate(_clk) container_of(_clk, struct pwrcal_clk_fixed_rate, clk)
#define to_ffactor(_clk) container_of(_clk, struct pwrcal_clk_fixed_factor, clk)
#define to_mux(_clk) container_of(_clk, struct pwrcal_mux, clk)
#define to_div(_clk) container_of(_clk, struct pwrcal_div, clk)
#define to_gate(_clk) container_of(_clk, struct pwrcal_gate, clk)

struct clk_entry {
	unsigned int id;
	struct pwrcal_clk *clk;
};
extern struct clk_entry pwrcal_clk_list[];

extern int pwrcal_mux_is_enabled(struct pwrcal_clk *clk);
extern int pwrcal_mux_enable(struct pwrcal_clk *clk);
extern int pwrcal_mux_disable(struct pwrcal_clk *clk);
extern int pwrcal_mux_get_src(struct pwrcal_clk *clk);
extern int pwrcal_mux_set_src(struct pwrcal_clk *clk, unsigned int src);
extern int pwrcal_mux_get_parents(struct pwrcal_clk *clk,
					struct pwrcal_clk **parents);
extern struct pwrcal_clk *pwrcal_mux_get_parent(struct pwrcal_clk *clk);
extern int _pwrcal_is_private_mux_set_src(struct pwrcal_clk *clk);
extern int _pwrcal_private_mux_set_src(struct pwrcal_clk *clk,
						unsigned int src);

extern int pwrcal_div_is_enabled(struct pwrcal_clk *clk);
extern int pwrcal_div_enable(struct pwrcal_clk *clk);
extern int pwrcal_div_disable(struct pwrcal_clk *clk);
extern unsigned int pwrcal_div_get_ratio(struct pwrcal_clk *clk);
extern int pwrcal_div_set_ratio(struct pwrcal_clk *clk, unsigned int ratio);
extern unsigned int pwrcal_div_get_max_ratio(struct pwrcal_clk *clk);

extern int pwrcal_pll_is_enabled(struct pwrcal_clk *clk);
extern int pwrcal_pll_enable(struct pwrcal_clk *clk);
extern int pwrcal_pll_disable(struct pwrcal_clk *clk);
extern unsigned long long pwrcal_pll_get_rate(struct pwrcal_clk *clk);
extern int pwrcal_pll_set_rate(struct pwrcal_clk *clk, unsigned long long rate);
extern unsigned long long clk_pll_getmaxfreq(struct pwrcal_clk *clk);
extern struct pll_spec *clk_pll_get_spec(struct pwrcal_clk *clk);

extern int pwrcal_gate_is_enabled(struct pwrcal_clk *clk);
extern int pwrcal_gate_enable(struct pwrcal_clk *clk);
extern int pwrcal_gate_disable(struct pwrcal_clk *clk);

extern struct pwrcal_clk *pwrcal_clk_get_parent(struct pwrcal_clk *clk);
extern unsigned long long pwrcal_clk_get_rate(struct pwrcal_clk *clk);
extern int pwrcal_clk_set_rate(struct pwrcal_clk *clk, unsigned long long rate);
extern int pwrcal_clk_enable(struct pwrcal_clk *clk);
extern int pwrcal_clk_disable(struct pwrcal_clk *clk);

extern unsigned int _cal_clk_get(char *name);
extern struct pwrcal_clk *cal_get_clk(unsigned int id);

extern void clk_init(void);
extern struct pwrcal_clk *clk_find(char *clk_name);
#endif
