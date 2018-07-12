#ifndef __PWRCAL_DFS_H__
#define __PWRCAL_DFS_H__

#include "pwrcal-env.h"
#include "pwrcal-clk.h"
#include "pwrcal-vclk.h"


#define TRANS_HIGH		0
#define TRANS_LOW		1
#define TRANS_DIFF		2
#define TRANS_FORCE		3

#define get_value(_table, _level, _member)	\
	(*(_table->rate_table + (_table->num_of_members * _level + _member)))


extern unsigned int dfs_set_rate_switch(unsigned int rate_from,
					unsigned int rate_to,
					struct dfs_table *table);
extern int dfs_enable_switch(struct dfs_table *table);
extern int dfs_disable_switch(struct dfs_table *table);
extern int dfs_use_switch(struct dfs_table *table);
extern int dfs_not_use_switch(struct dfs_table *table);
extern int dfs_trans_div(int lv_from, int lv_to, struct dfs_table *table, int opt);
extern int dfs_trans_pll(int lv_from, int lv_to, struct dfs_table *table, int opt);
extern int dfs_trans_mux(int lv_from, int lv_to, struct dfs_table *table, int opt);
extern int dfs_trans_gate(int lv_from, int lv_to, struct dfs_table *table, int opt);
extern int dfs_get_lv(unsigned int rate, struct dfs_table *table);



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

extern void dfs_init(void);
extern void dfs_dram_init(void);
#endif
