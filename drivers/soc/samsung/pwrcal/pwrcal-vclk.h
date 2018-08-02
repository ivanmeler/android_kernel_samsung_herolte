#ifndef __PWRCAL_VCLK_H__
#define __PWRCAL_VCLK_H__

#include "pwrcal-env.h"
#include "pwrcal-clk.h"

#define	vclk_group_grpgate	(0x0A000000 | 0x00000000)
#define	vclk_group_m1d1g1	(0x0A000000 | 0x00010000)
#define	vclk_group_p1	(0x0A000000 | 0x00020000)
#define	vclk_group_m1	(0x0A000000 | 0x00030000)
#define	vclk_group_d1	(0x0A000000 | 0x00040000)
#define	vclk_group_pxmxdx	(0x0A000000 | 0x00050000)
#define	vclk_group_umux	(0x0A000000 | 0x00060000)
#define	vclk_group_dfs	(0x0A000000 | 0x00070000)
#define	vclk_group_mask	(0x0A000000 | 0x000F0000)

struct vclk {
	unsigned int type;
	struct vclk *parent;
	int ref_count;
	unsigned long vfreq;
	char *name;
	struct vclk_ops *ops;
};

struct vclk_ops {
	int (*enable)(struct vclk *vclk);
	int (*disable)(struct vclk *vclk);
	int (*is_enabled)(struct vclk *vclk);
	unsigned long (*get_rate)(struct vclk *vclk);
	int (*set_rate)(struct vclk *vclk, unsigned long rate);
};

extern struct vclk_ops grpgate_ops;
extern struct vclk_ops m1d1g1_ops;
extern struct vclk_ops p1_ops;
extern struct vclk_ops m1_ops;
extern struct vclk_ops d1_ops;
extern struct vclk_ops pxmxdx_ops;
extern struct vclk_ops umux_ops;
extern struct vclk_ops dfs_ops;

struct pwrcal_vclk_grpgate {
	struct vclk vclk;
	struct pwrcal_clk **gates;
};

struct pwrcal_vclk_m1d1g1 {
	struct vclk vclk;
	struct pwrcal_clk *mux;
	struct pwrcal_clk *div;
	struct pwrcal_clk *gate;
	struct pwrcal_clk *extramux;
};

struct pwrcal_vclk_umux {
	struct vclk vclk;
	struct pwrcal_clk *umux;
};

struct pwrcal_vclk_p1 {
	struct vclk vclk;
	struct pwrcal_clk *pll;
};

struct pwrcal_vclk_m1 {
	struct vclk vclk;
	struct pwrcal_clk *mux;
};

struct pwrcal_vclk_d1 {
	struct vclk vclk;
	struct pwrcal_clk *div;
};

struct pwrcal_clk_set {
	struct pwrcal_clk *clk;
	int config0;
	int config1;
};

extern int is_config(struct pwrcal_clk_set *table, int config);
extern int set_config(struct pwrcal_clk_set *table, int config);

struct pwrcal_vclk_pxmxdx {
	struct vclk vclk;
	struct pwrcal_clk_set *clk_list;
};
extern unsigned int _cal_vclk_get(char *name);
extern struct vclk *cal_get_vclk(unsigned int id);


#define REPRESENT_RATE		CLK_NONE

struct dfs_switch {
	unsigned int switch_rate;
	unsigned int mux_value;
	unsigned int div_value;
};

struct dfs_table {
	struct pwrcal_clk **members;
	unsigned int *rate_table;
	int num_of_members;
	int num_of_lv;
	unsigned long max_freq;	/* KHZ */
	unsigned long min_freq;	/* KHZ */

	struct dfs_switch *switches;
	int num_of_switches;
	struct pwrcal_clk *switch_mux;
	unsigned int switch_use;
	unsigned int switch_notuse;
	struct pwrcal_clk *switch_src_mux;
	struct pwrcal_clk *switch_src_div;
	struct pwrcal_clk *switch_src_gate;
	struct pwrcal_clk *switch_src_usermux;

	void (*trans_pre)(unsigned int rate_from, unsigned int rate_to);
	void (*trans_post)(unsigned int rate_from, unsigned int rate_to);
	void (*switch_pre)(unsigned int rate_from, unsigned int rate_to);
	void (*switch_post)(unsigned int rate_from, unsigned int rate_to);
	int (*private_trans)(unsigned int rate_from, unsigned int rate_to,
					struct dfs_table *table);
	int (*private_switch)(unsigned int rate_from, unsigned int rate_switch,
					struct dfs_table *table);
	unsigned long (*private_getrate)(struct dfs_table *table);
};

/* dfs ops */
struct vclk_dfs_ops {
	int (*set_ema)(unsigned int volt);
	int (*init_smpl)(void);
	int (*deinit_smpl)(void);
	int (*set_smpl)(void);
	int (*get_smpl)(void);
	int (*dvs)(int command);	/* 0: DVS on  1: DVS off   2: DVS init */
	int (*is_dll_on)(void);
	int (*get_target_rate)(char *member, unsigned long rate);
	int (*get_rate_table)(unsigned long *table);
	int (*get_asv_table)(unsigned int *table);
	int (*set_voltage)(unsigned int uv);
	int (*cpu_idle_clock_down)(unsigned int enable);
	int (*ctrl_clk_gate)(unsigned int enable);
	int (*get_margin_param)(unsigned int id);
	int (*rate_lock)(unsigned int para);
};

struct pwrcal_vclk_dfs {
	struct vclk vclk;
	struct pwrcal_clk **clks;
	struct pwrcal_clk_set *en_clks;
	struct dfs_table *table;
	struct vclk_dfs_ops *dfsops;
	int volt_margin;
	spinlock_t	*lock;

};
extern struct vclk_ops dfs_ops;


struct pwrcal_vclk_none {
	struct vclk vclk;
};

#define VCLK(_id)		((struct vclk *)(&((vclk_##_id).vclk)))

extern struct pwrcal_vclk_none vclk_0;
#define VCLK_NONE	(&(vclk_0.vclk))


#define GRPGATE_EXTERN(_id)	\
	extern struct pwrcal_vclk_grpgate vclk_##_id;
#define M1D1G1_EXTERN(_id)	\
	extern struct pwrcal_vclk_m1d1g1 vclk_##_id;
#define P1_EXTERN(_id)		\
	extern struct pwrcal_vclk_p1 vclk_##_id;
#define M1_EXTERN(_id)		\
	extern struct pwrcal_vclk_m1 vclk_##_id;
#define D1_EXTERN(_id)		\
	extern struct pwrcal_vclk_d1 vclk_##_id;
#define PXMXDX_EXTERN(_id)	\
	extern struct pwrcal_vclk_pxmxdx vclk_##_id;
#define UMUX_EXTERN(_id)	\
	extern struct pwrcal_vclk_umux vclk_##_id;
#define DFS_EXTERN(_id)		\
	extern struct pwrcal_vclk_dfs vclk_##_id;


/* Macro to define GRPGATE */
#define GRPGATE(_id, _parent, _gates)		\
struct pwrcal_vclk_grpgate vclk_##_id __attribute__((unused, aligned(8), section(".vclk_grpgate."))) = {	\
	.vclk.type	= vclk_group_grpgate,				\
	.vclk.parent	= &((vclk_##_parent).vclk),	\
	.vclk.ref_count	= 0,				\
	.vclk.name	= #_id,				\
	.vclk.ops	= &grpgate_ops,				\
	.gates		= _gates,		\
}

/* Macro to define PXMXDX */
#define PXMXDX(_id, _parent, _clk_list)		\
struct pwrcal_vclk_pxmxdx vclk_##_id __attribute__((unused, aligned(8), section(".vclk_pxmxdx."))) = {	\
	.vclk.type	= vclk_group_pxmxdx,				\
	.vclk.parent	= &((vclk_##_parent).vclk),	\
	.vclk.ref_count	= 0,				\
	.vclk.name	= #_id,				\
	.vclk.ops	= &pxmxdx_ops,			\
	.clk_list		= _clk_list,			\
}

/* Macro to define UMUX */
#define UMUX(_id, _parent, _umux)		\
struct pwrcal_vclk_umux vclk_##_id __attribute__((unused, aligned(8), section(".vclk_umux."))) = {	\
	.vclk.type	= vclk_group_umux,				\
	.vclk.parent	= &((vclk_##_parent).vclk),	\
	.vclk.ref_count	= 0,				\
	.vclk.name	= #_id,				\
	.vclk.ops	= &umux_ops,			\
	.umux		= &((clk_##_umux).clk),		\
}

/* Macro to define M1D1G1 */
#define M1D1G1(_id, _parent,			\
		_mux, _div, _gate, _extramux)		\
struct pwrcal_vclk_m1d1g1 vclk_##_id __attribute__((unused, aligned(8), section(".vclk_m1d1g1."))) = {	\
	.vclk.type	= vclk_group_m1d1g1,				\
	.vclk.parent	= &((vclk_##_parent).vclk),	\
	.vclk.ref_count	= 0,				\
	.vclk.name	= #_id,				\
	.vclk.ops	= &m1d1g1_ops,			\
	.mux		= &((clk_##_mux).clk),		\
	.div		= &((clk_##_div).clk),		\
	.gate		= &((clk_##_gate).clk),		\
	.extramux	= &((clk_##_extramux).clk),	\
}

/* Macro to define P1 */
#define P1(_id, _parent, _pll)			\
struct pwrcal_vclk_p1 vclk_##_id __attribute__((unused, aligned(8), section(".vclk_p1."))) = {	\
	.vclk.type	= vclk_group_p1,				\
	.vclk.parent	= &((vclk_##_parent).vclk),	\
	.vclk.ref_count	= 0,				\
	.vclk.name	= #_id,				\
	.vclk.ops	= &p1_ops,			\
	.pll		= &((clk_##_pll).clk),		\
}

/* Macro to define M1 */
#define M1(_id, _parent, _mux)			\
struct pwrcal_vclk_m1 vclk_##_id __attribute__((unused, aligned(8), section(".vclk_m1."))) = {	\
	.vclk.type	= vclk_group_m1,				\
	.vclk.parent	= &((vclk_##_parent).vclk),	\
	.vclk.ref_count	= 0,				\
	.vclk.name	= #_id,				\
	.vclk.ops	= &m1_ops,			\
	.mux		= &((clk_##_mux).clk),		\
}

/* Macro to define D1 */
#define D1(_id, _parent, _div)			\
struct pwrcal_vclk_d1 vclk_##_id __attribute__((unused, aligned(8), section(".vclk_d1."))) = {	\
	.vclk.type	= vclk_group_d1,				\
	.vclk.parent	= &((vclk_##_parent).vclk),	\
	.vclk.ref_count	= 0,				\
	.vclk.name	= #_id,				\
	.vclk.ops	= &d1_ops,			\
	.div		= &((clk_##_div).clk),		\
}

#define DFS(_id)	struct pwrcal_vclk_dfs vclk_##_id __attribute__((unused, aligned(8), section(".vclk_dfs.")))


extern struct pwrcal_vclk_grpgate *vclk_grpgate_list[];
extern struct pwrcal_vclk_m1d1g1 *vclk_m1d1g1_list[];
extern struct pwrcal_vclk_p1 *vclk_p1_list[];
extern struct pwrcal_vclk_m1 *vclk_m1_list[];
extern struct pwrcal_vclk_d1 *vclk_d1_list[];
extern struct pwrcal_vclk_pxmxdx *vclk_pxmxdx_list[];
extern struct pwrcal_vclk_umux *vclk_umux_list[];
extern struct pwrcal_vclk_dfs *vclk_dfs_list[];
extern unsigned int vclk_grpgate_list_size;
extern unsigned int vclk_m1d1g1_list_size;
extern unsigned int vclk_p1_list_size;
extern unsigned int vclk_m1_list_size;
extern unsigned int vclk_d1_list_size;
extern unsigned int vclk_pxmxdx_list_size;
extern unsigned int vclk_umux_list_size;
extern unsigned int vclk_dfs_list_size;


#define to_grpgate(_vclk) container_of(_vclk, struct pwrcal_vclk_grpgate, vclk)
#define to_pxmxdx(_vclk) container_of(_vclk, struct pwrcal_vclk_pxmxdx, vclk)
#define to_umux(_vclk) container_of(_vclk, struct pwrcal_vclk_umux, vclk)
#define to_m1d1g1(_vclk) container_of(_vclk, struct pwrcal_vclk_m1d1g1, vclk)
#define to_p1(_vclk) container_of(_vclk, struct pwrcal_vclk_p1, vclk)
#define to_m1(_vclk) container_of(_vclk, struct pwrcal_vclk_m1, vclk)
#define to_d1(_vclk) container_of(_vclk, struct pwrcal_vclk_d1, vclk)
#define to_dfs(_vclk) container_of(_vclk, struct pwrcal_vclk_dfs, vclk)

extern int vclk_setrate(struct vclk *vclk, unsigned long rate);
extern unsigned long vclk_getrate(struct vclk *vclk);
extern int vclk_enable(struct vclk *vclk);
extern int vclk_disable(struct vclk *vclk);

extern int dfs_get_rate_table(struct dfs_table *dfs, unsigned long *table);
extern int dfs_get_target_rate_table(struct dfs_table *dfs,
				unsigned int mux,
				unsigned int div,
				unsigned long *table);
extern int dfs_set_rate(struct vclk *vclk, unsigned long to);
extern unsigned long dfs_get_rate(struct vclk *vclk);
extern int dfs_enable(struct vclk *vclk);
extern int dfs_disable(struct vclk *vclk);
extern int dfs_is_enabled(struct vclk *vclk);
extern unsigned long dfs_get_max_freq(struct vclk *vclk);
extern unsigned long dfs_get_min_freq(struct vclk *vclk);
extern void vclk_unused_disable(void);
extern void vclk_init(void);

/* for debug */
#define NUM_OF_DEBUG_CLKS		64

struct cal_vclk_debug_info {
	unsigned int type;
	unsigned int *parent;
	unsigned int *blkpwr;
	int state;
	int ref_count;
	int dtci;
	unsigned int clks[NUM_OF_DEBUG_CLKS];
	unsigned long rates[NUM_OF_DEBUG_CLKS];
	int num_of_clks;
	char *name;
};

extern int cal_vclk_get_debug(unsigned int id,
				struct cal_vclk_debug_info *info);

struct cal_clk_debug_info {
	unsigned int type;
	unsigned int config;
	unsigned int addr;
	unsigned int shift;
	unsigned int mask;
	unsigned int stat;
	unsigned int stat_addr;
	unsigned int stat_shift;
	unsigned int stat_mask;
	unsigned int parent;
	char *name;
};

extern int cal_clk_get_debug(unsigned int id, struct cal_clk_debug_info *info);

#define NUM_OF_DEBUG_CLKPATH		16
struct cal_clk_debug_clkpath {
	unsigned int clks[NUM_OF_DEBUG_CLKPATH];
	unsigned long rates[NUM_OF_DEBUG_CLKPATH];		/* KHZ */
	const char *name[NUM_OF_DEBUG_CLKPATH];
};

extern unsigned long cal_clk_get_path_debug(unsigned int id,
				struct cal_clk_debug_clkpath *clkpath);
extern void cal_vclk_dbg_print_all(void);

#endif
