/* linux/arch/arm64/mach-exynos/include/mach/asv.h
 *
 * copyright (c) 2014 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - Adaptive Support Voltage Source File
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
*/

#ifndef __ASM_ARCH_NEW_ASV_H
#define __ASM_ARCH_NEW_ASV_H __FILE__

#include <linux/io.h>

#define ASV_GRP_NR(_id)		_id##_ASV_GRP_NR
#define DVFS_LEVEL_NR(_id)	_id##_DVFS_LEVEL_NR
#define MAX_VOLT(_id)		_id##_MAX_VOLT
#define MAX_VOLT_VER(_id, _ver)	_id##_MAX_VOLT_##_ver

#define ABB_X060		0
#define ABB_X065		1
#define ABB_X070		2
#define ABB_X075		3
#define ABB_X080		4
#define ABB_X085		5
#define ABB_X090		6
#define ABB_X095		7
#define ABB_X100		8
#define ABB_X105		9
#define ABB_X110		10
#define ABB_X115		11
#define ABB_X120		12
#define ABB_X125		13
#define ABB_X130		14
#define ABB_X135		15
#define ABB_X140		16
#define ABB_X145		17
#define ABB_X150		18
#define ABB_X155		19
#define ABB_X160		20
#define ABB_BYPASS		255

#define ABB_INIT		(0x80000080)
#define ABB_INIT_BYPASS		(0x80000000)

#define MAX_ASV_GRP_NR 		16
#define MAX_ASV_SUB_GRP_NR	3

static inline void set_abb(void __iomem *target_reg, unsigned int target_value)
{
	unsigned int tmp;

	if (target_value == ABB_BYPASS)
		tmp = ABB_INIT_BYPASS;
	else
		tmp = (ABB_INIT | target_value);
	__raw_writel(tmp , target_reg);
}

enum asv_type_id {
	ID_CL1,
	ID_CL0,
	ID_INT,
	ID_MIF,
	ID_G3D,
	ID_ISP,
	ASV_TYPE_END,
};

/* define Struct for ASV common */
struct asv_common {
	char		lot_name[5];
	unsigned int	ids_value;
	unsigned int	hpm_value;
	unsigned int	(*init)(void);
	unsigned int	(*regist_asv_member)(void);
	struct asv_common_ops_cal	*ops_cal;
};

/* operation for CAL */
struct asv_ops_cal {
	u32		(*get_vol)(u32, s32 eLvl);
	u32		(*get_freq)(u32 id, s32 eLvl);
	u32 		(*get_abb)(u32 id, s32 eLvl);
	u32 		(*get_rcc)(u32 id, s32 eLvl);
	bool		(*get_use_abb)(u32 id);
	void		(*set_abb)(u32 id, u32 eAbb);
	u32		(*set_rcc)(u32 id, s32 level, u32 rcc);
	u32		(*get_group)(u32 id, s32 level);
	u32		(*get_sub_grp_idx)(u32 id, s32 level);
};

struct asv_common_ops_cal {
	u32	(*get_max_volt)(u32 id);
	s32	(*get_min_lv)(u32 id);
	void	(*init)(void);
	u32	(*get_table_ver)(void);
	bool	(*is_fused_sp_gr)(void);
	u32	(*get_asv_gr)(void);
	u32	(*get_ids)(void);
	u32	(*get_hpm)(void);
	void	(*set_rcc_limit_info)(void);
};

struct asv_freq_table {
	unsigned int	asv_freq;
	unsigned int	asv_value;
};

struct asv_grp_table {
	unsigned int	asv_sub_idx;
	unsigned int	asv_grp;
};

/* define struct for information of each ASV type */
struct asv_info {
	struct list_head	node;
	enum asv_type_id	asv_type;
	const char		*name;
	struct asv_ops		*ops;
	unsigned int		asv_group_nr;
	unsigned int		dvfs_level_nr;
	unsigned int		result_asv_grp;
	unsigned int		max_volt_value;
	struct asv_freq_table	*asv_volt;
	struct asv_freq_table	*asv_abb;
	struct asv_freq_table	*asv_rcc;
	struct abb_common	*abb_info;
	struct asv_ops_cal	*ops_cal;
	struct asv_grp_table	*asv_sub_grp;
};

/* Struct for ABB function */
struct abb_common {
	unsigned int	target_abb;
	void		(*set_target_abb)(struct asv_info *asv_inform);
};

/* Operation for ASV*/
struct asv_ops {
	unsigned int	(*get_asv_group)(struct asv_common *asv_comm);
	void		(*set_asv_info)(struct asv_info *asv_inform,
					bool show_value);
	unsigned int	(*get_asv_sub_group)(struct asv_info *asv_inform,
					unsigned int lv);
	void		(*set_rcc_info)(struct asv_info *asv_inform);
};

/* define function for common asv */
extern void add_asv_member(struct asv_info *exynos_asv_info);
extern struct asv_info *asv_get(enum asv_type_id exynos_asv_type_id);
#if defined (CONFIG_EXYNOS_ASV)
extern unsigned int get_match_volt(enum asv_type_id target_type, unsigned int target_freq);
/* define function for initialize of SoC */
extern int exynos_init_asv(struct asv_common *asv_info);
extern unsigned int exynos_get_table_ver(void);
extern void exynos_set_ema(enum asv_type_id type, unsigned int volt);
extern unsigned int exynos_get_asv_info(int id);
extern unsigned int get_sub_grp_match_asv_grp(enum asv_type_id target_type, unsigned int lv);
#else
static inline int get_match_volt(enum asv_type_id target_type, unsigned int target_freq){return 0;}
static inline int exynos_init_asv(struct asv_common *asv_info){return 0;}
static inline unsigned int exynos_get_table_ver(void){return 0;}
static inline void exynos_set_ema(enum asv_type_id type, unsigned int volt){};
static inline unsigned int exynos_get_asv_info(int id){return 0;};
extern inline unsigned int get_sub_grp_match_asv_grp(enum asv_type_id target_type, unsigned int lv){return 0;}
#endif
#if defined (CONFIG_EXYNOS_ASV_DYNAMIC_ABB)
extern unsigned int get_match_abb(enum asv_type_id target_type, unsigned int target_freq);
extern unsigned int set_match_abb(enum asv_type_id target_type, unsigned int target_abb);
extern bool is_set_abb_first(enum asv_type_id target_type, unsigned int old_freq, unsigned int target_freq);
#else
static inline unsigned int get_match_abb(enum asv_type_id target_type, unsigned int target_freq)
{return ABB_BYPASS;}
static inline unsigned int set_match_abb(enum asv_type_id target_type, unsigned int target_abb)
{return 0;}
static inline bool is_set_abb_first(enum asv_type_id target_type, unsigned int old_freq, unsigned int target_freq)
{return false;}
#endif
#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
extern void set_rcc_info(void);
#else
static inline void set_rcc_info(void){};
#endif

#endif /* __ASM_ARCH_NEW_ASV_H */
