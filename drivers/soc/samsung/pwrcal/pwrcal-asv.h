#ifndef __PWRCAL_ASV_H__
#define __PWRCAL_ASV_H__

struct cal_asv_ops {
	void (*print_asv_info)(void);
	void (*print_rcc_info)(void);
	void (*set_grp)(unsigned int id, unsigned int asvgrp);
	int (*get_grp)(unsigned int id, unsigned int lv);
	void (*set_tablever)(unsigned int version);
	int (*get_tablever)(void);
	int (*set_rcc_table)(void);
	int (*asv_init)(void);
	unsigned int (*asv_pmic_info)(void);
	int (*get_ids_info)(unsigned int domain);
	void (*set_ssa0)(unsigned int id, unsigned int ssa0);
};

extern struct cal_asv_ops cal_asv_ops;

#endif
