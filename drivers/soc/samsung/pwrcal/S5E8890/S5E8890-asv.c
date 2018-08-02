#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "../pwrcal-asv.h"
#include "S5E8890-cmusfr.h"
#include "S5E8890-cmu.h"
#include "S5E8890-vclk.h"
#include "S5E8890-vclk-internal.h"

#ifdef PWRCAL_TARGET_LINUX
#include <linux/io.h>
#include <asm/map.h>
#include <soc/samsung/ect_parser.h>
#endif
#ifdef PWRCAL_TARGET_FW
#include <mach/ect_parser.h>
#endif

#define MIN(x, y)   ((x) < (y) ? (x) : (y))

enum dvfs_id {
	cal_asv_dvfs_big,
	cal_asv_dvfs_little,
	cal_asv_dvfs_g3d,
	cal_asv_dvfs_mif,
	cal_asv_dvfs_int,
	cal_asv_dvfs_cam,
	cal_asv_dvfs_disp,
	cal_asv_dvs_g3dm,
	num_of_dvfs,
};

#define MAX_ASV_GROUP	16
#define NUM_OF_ASVTABLE	1
#define PWRCAL_ASV_LIST(table)	{table, sizeof(table) / sizeof(table[0])}

#define MAX_VOLT_OF_MIF	1000000

struct asv_table_entry {
	unsigned int index;
	unsigned int *voltage;
};

struct asv_table_list {
	struct asv_table_entry *table;
	unsigned int table_size;
	unsigned int voltage_count;
};

#define FORCE_ASV_MAGIC		0x57E90000
static unsigned int force_asv_group[num_of_dvfs];

struct asv_tbl_info {
	unsigned mngs_asv_group:4;
	int mngs_modified_group:4;
	unsigned mngs_ssa10:2;
	unsigned mngs_ssa11:2;
	unsigned mngs_ssa0:4;
	unsigned apollo_asv_group:4;
	int apollo_modified_group:4;
	unsigned apollo_ssa10:2;
	unsigned apollo_ssa11:2;
	unsigned apollo_ssa0:4;

	unsigned g3d_asv_group:4;
	int g3d_modified_group:4;
	unsigned g3d_ssa10:2;
	unsigned g3d_ssa11:2;
	unsigned g3d_ssa0:4;
	unsigned mif_asv_group:4;
	int mif_modified_group:4;
	unsigned mif_ssa10:2;
	unsigned mif_ssa11:2;
	unsigned mif_ssa0:4;

	unsigned int_asv_group:4;
	int int_modified_group:4;
	unsigned int_ssa10:2;
	unsigned int_ssa11:2;
	unsigned int_ssa0:4;
	unsigned disp_asv_group:4;
	int disp_modified_group:4;
	unsigned disp_ssa10:2;
	unsigned disp_ssa11:2;
	unsigned disp_ssa0:4;

	unsigned asv_table_ver:7;
	unsigned fused_grp:1;
	unsigned reserved_0:8;
	unsigned cp_asv_group:4;
	int cp_modified_group:4;
	unsigned cp_ssa10:2;
	unsigned cp_ssa11:2;
	unsigned cp_ssa0:4;

	unsigned mngs_vthr:2;
	unsigned mngs_delta:2;
	unsigned apollo_vthr:2;
	unsigned apollo_delta:2;
	unsigned g3dm_vthr1:2;
	unsigned g3dm_vthr2:2;
	unsigned int_vthr:2;
	unsigned int_delta:2;
	unsigned mif_vthr:2;
	unsigned mif_delta:2;
	unsigned g3d_mcs0:4;
	unsigned g3d_mcs1:4;
	unsigned reserved_2:4;
};
#define ASV_INFO_ADDR_BASE	(0x101E9000)
#define ASV_INFO_ADDR_CNT	(sizeof(struct asv_tbl_info) / 4)

static struct asv_tbl_info asv_tbl_info;

static struct asv_table_list *pwrcal_big_asv_table;
static struct asv_table_list *pwrcal_little_asv_table;
static struct asv_table_list *pwrcal_g3d_asv_table;
static struct asv_table_list *pwrcal_mif_asv_table;
static struct asv_table_list *pwrcal_int_asv_table;
static struct asv_table_list *pwrcal_cam_asv_table;
static struct asv_table_list *pwrcal_disp_asv_table;
static struct asv_table_list *pwrcal_g3dm_asv_table;

static struct pwrcal_vclk_dfs *asv_dvfs_big;
static struct pwrcal_vclk_dfs *asv_dvfs_little;
static struct pwrcal_vclk_dfs *asv_dvfs_g3d;
static struct pwrcal_vclk_dfs *asv_dvfs_mif;
static struct pwrcal_vclk_dfs *asv_dvfs_int;
static struct pwrcal_vclk_dfs *asv_dvfs_cam;
static struct pwrcal_vclk_dfs *asv_dvfs_disp;
static struct pwrcal_vclk_dfs *asv_dvs_g3dm;

static unsigned int big_subgrp_index = 256;
static unsigned int little_subgrp_index = 256;
static unsigned int g3d_subgrp_index = 256;
static unsigned int mif_subgrp_index = 256;
static unsigned int int_subgrp_index = 256;
static unsigned int cam_subgrp_index = 256;
static unsigned int disp_subgrp_index = 256;
static unsigned int g3dm_subgrp_index = 256;

static unsigned int big_ssa1_table[8];
static unsigned int little_ssa1_table[8];
static unsigned int g3d_ssa1_table[8];
static unsigned int mif_ssa1_table[8];
static unsigned int int_ssa1_table[8];
static unsigned int cam_ssa1_table[8];
static unsigned int disp_ssa1_table[8];

static unsigned int big_ssa0_base;
static unsigned int little_ssa0_base;
static unsigned int g3d_ssa0_base;
static unsigned int mif_ssa0_base;
static unsigned int int_ssa0_base;
static unsigned int cam_ssa0_base;
static unsigned int disp_ssa0_base;

static unsigned int big_ssa0_offset;
static unsigned int little_ssa0_offset;
static unsigned int g3d_ssa0_offset;
static unsigned int mif_ssa0_offset;
static unsigned int int_ssa0_offset;
static unsigned int cam_ssa0_offset;
static unsigned int disp_ssa0_offset;

static void asv_set_grp(unsigned int id, unsigned int asvgrp)
{
	force_asv_group[id & 0x0000FFFF] = FORCE_ASV_MAGIC | asvgrp;
}

static void asv_set_tablever(unsigned int version)
{
	asv_tbl_info.asv_table_ver = version;

	return;
}

static void asv_set_ssa0(unsigned int id, unsigned int ssa0)
{
	switch (id & 0x0000FFFF) {
	case cal_asv_dvfs_big:
		asv_tbl_info.mngs_ssa0 = ssa0;
		break;
	case cal_asv_dvfs_little:
		asv_tbl_info.apollo_ssa0 = ssa0;
		break;
	case cal_asv_dvfs_g3d:
		asv_tbl_info.g3d_ssa0 = ssa0;
		break;
	case cal_asv_dvfs_mif:
		asv_tbl_info.mif_ssa0 = ssa0;
		break;
	case cal_asv_dvfs_int:
		asv_tbl_info.int_ssa0 = ssa0;
		break;
	case cal_asv_dvfs_cam:
		asv_tbl_info.disp_ssa0 = ssa0;
		break;
	case cal_asv_dvfs_disp:
		asv_tbl_info.disp_ssa0 = ssa0;
		break;
	default:
		break;
	}

}

static void get_max_min_freq_lv(struct ect_voltage_domain *domain, unsigned int version, int *max_lv, int *min_lv)
{
	int i;
	unsigned int max_asv_version = 0;
	struct ect_voltage_table *table = NULL;

	for (i = 0; i < domain->num_of_table; i++) {
		table = &domain->table_list[i];

		if (version == table->table_version)
			break;

		if (table->table_version > max_asv_version)
			max_asv_version = table->table_version;
	}

	if (i == domain->num_of_table) {
		pr_err("There is no voltage table at ECT, PWRCAL force change table version from %d to %d\n", asv_tbl_info.asv_table_ver, max_asv_version);
		asv_tbl_info.asv_table_ver = max_asv_version;
	}

	*max_lv = -1;
	*min_lv = domain->num_of_level - 1;
	for (i = 0; i < domain->num_of_level; i++) {
		if (*max_lv == -1 && table->level_en[i])
			*max_lv = i;
		if (*max_lv != -1 && !table->level_en[i]) {
			*min_lv = i - 1;
			break;
		}
	}
}

static void asv_set_freq_limit(void)
{
	void *asv_block;
	struct ect_voltage_domain *domain;
	int max_lv = 0, min_lv = 0;
	unsigned int asv_table_ver;
	int retry_done = 1;

	asv_block = ect_get_block("ASV");
	if (asv_block == NULL)
		BUG();

retry:

	asv_table_ver = asv_tbl_info.asv_table_ver;

	domain = ect_asv_get_domain(asv_block, "dvfs_big");
	if (!domain)
		BUG();
	get_max_min_freq_lv(domain, asv_tbl_info.asv_table_ver, &max_lv, &min_lv);
	if (max_lv >= 0)
		asv_dvfs_big->table->max_freq = domain->level_list[max_lv] * 1000;
	if (min_lv >= 0)
		asv_dvfs_big->table->min_freq = domain->level_list[min_lv] * 1000;

	domain = ect_asv_get_domain(asv_block, "dvfs_little");
	if (!domain)
		BUG();
	get_max_min_freq_lv(domain, asv_tbl_info.asv_table_ver, &max_lv, &min_lv);
	if (max_lv >= 0)
		asv_dvfs_little->table->max_freq = domain->level_list[max_lv] * 1000;
	if (min_lv >= 0)
		asv_dvfs_little->table->min_freq = domain->level_list[min_lv] * 1000;

	domain = ect_asv_get_domain(asv_block, "dvfs_g3d");
	if (!domain)
		BUG();
	get_max_min_freq_lv(domain, asv_tbl_info.asv_table_ver, &max_lv, &min_lv);
	if (max_lv >= 0)
		asv_dvfs_g3d->table->max_freq = domain->level_list[max_lv] * 1000;
	if (min_lv >= 0)
		asv_dvfs_g3d->table->min_freq = domain->level_list[min_lv] * 1000;

	domain = ect_asv_get_domain(asv_block, "dvfs_mif");
	if (!domain)
		BUG();
	get_max_min_freq_lv(domain, asv_tbl_info.asv_table_ver, &max_lv, &min_lv);
	if (max_lv >= 0)
		asv_dvfs_mif->table->max_freq = domain->level_list[max_lv] * 1000;
	if (min_lv >= 0)
		asv_dvfs_mif->table->min_freq = domain->level_list[min_lv] * 1000;

	domain = ect_asv_get_domain(asv_block, "dvfs_int");
	if (!domain)
		BUG();
	get_max_min_freq_lv(domain, asv_tbl_info.asv_table_ver, &max_lv, &min_lv);
	if (max_lv >= 0)
		asv_dvfs_int->table->max_freq = domain->level_list[max_lv] * 1000;
	if (min_lv >= 0)
		asv_dvfs_int->table->min_freq = domain->level_list[min_lv] * 1000;

	domain = ect_asv_get_domain(asv_block, "dvfs_disp");
	if (!domain)
		BUG();
	get_max_min_freq_lv(domain, asv_tbl_info.asv_table_ver, &max_lv, &min_lv);
	if (max_lv >= 0)
		asv_dvfs_disp->table->max_freq = domain->level_list[max_lv] * 1000;
	if (min_lv >= 0)
		asv_dvfs_disp->table->min_freq = domain->level_list[min_lv] * 1000;

	domain = ect_asv_get_domain(asv_block, "dvfs_cam");
	if (!domain)
		BUG();
	get_max_min_freq_lv(domain, asv_tbl_info.asv_table_ver, &max_lv, &min_lv);
	if (max_lv >= 0)
		asv_dvfs_cam->table->max_freq = domain->level_list[max_lv] * 1000;
	if (min_lv >= 0)
		asv_dvfs_cam->table->min_freq = domain->level_list[min_lv] * 1000;

	if (asv_table_ver != asv_tbl_info.asv_table_ver && retry_done) {
		retry_done = 0;
		goto retry;
	}

	return;
}

static unsigned int asv_get_pmic_info(void)
{
	unsigned int temp = 0;

	temp = asv_tbl_info.mngs_vthr |
		asv_tbl_info.mngs_delta << 2 |
		asv_tbl_info.apollo_vthr << 4 |
		asv_tbl_info.apollo_delta << 6 |
		asv_tbl_info.int_vthr << 12 |
		asv_tbl_info.int_delta << 14 |
		asv_tbl_info.mif_vthr << 16 |
		asv_tbl_info.mif_delta << 18;

	return temp;
}

static void asv_get_asvinfo(void)
{
	int i;
	unsigned int *pasv_table;
	unsigned long tmp;

	pasv_table = (unsigned int *)&asv_tbl_info;
	for (i = 0; i < ASV_INFO_ADDR_CNT; i++) {
#ifdef PWRCAL_TARGET_LINUX
		exynos_smc_readsfr((unsigned long)(ASV_INFO_ADDR_BASE + 0x4 * i), &tmp);
#else
#if (CONFIG_STARTUP_EL_MODE == STARTUP_EL3)
		tmp = *((volatile unsigned int *)(unsigned long)(ASV_INFO_ADDR_BASE + 0x4 * i));
#else
		smc_readsfr((unsigned long)(ASV_INFO_ADDR_BASE + 0x4 * i), &tmp);
#endif
#endif
		*(pasv_table + i) = (unsigned int)tmp;
	}

	if (!asv_tbl_info.fused_grp) {
		asv_tbl_info.asv_table_ver = 3;
		asv_tbl_info.fused_grp = 1;
	}

	if (asv_tbl_info.g3d_mcs0 == 0)
		asv_tbl_info.g3d_mcs0 = 0x4;
	if (asv_tbl_info.g3d_mcs1 == 0)
		asv_tbl_info.g3d_mcs1 = 0x5;

	asv_set_freq_limit();
}

static int get_asv_group(enum dvfs_id domain, unsigned int lv)
{
	int asv = 0;
	int mod = 0;

	switch (domain) {
	case cal_asv_dvfs_big:
		asv = asv_tbl_info.mngs_asv_group;
		mod = asv_tbl_info.mngs_modified_group;
		break;
	case cal_asv_dvfs_little:
		asv = asv_tbl_info.apollo_asv_group;
		mod = asv_tbl_info.apollo_modified_group;
		break;
	case cal_asv_dvfs_g3d:
		asv = asv_tbl_info.g3d_asv_group;
		mod = asv_tbl_info.g3d_modified_group;
		break;
	case cal_asv_dvfs_mif:
		asv = asv_tbl_info.mif_asv_group;
		mod = asv_tbl_info.mif_modified_group;
		break;
	case cal_asv_dvfs_int:
		asv = asv_tbl_info.int_asv_group;
		mod = asv_tbl_info.int_modified_group;
		break;
	case cal_asv_dvfs_cam:
		asv = asv_tbl_info.disp_asv_group;
		mod = asv_tbl_info.disp_modified_group;
		break;
	case cal_asv_dvfs_disp:
		asv = asv_tbl_info.disp_asv_group;
		mod = asv_tbl_info.disp_modified_group;
		break;
	case cal_asv_dvs_g3dm:
		asv = asv_tbl_info.g3d_asv_group;
		mod = asv_tbl_info.g3d_modified_group;
		break;
	default:
		BUG();	/* Never reach */
		break;
	}

	if ((force_asv_group[domain & 0x0000FFFF] & 0xFFFF0000) != FORCE_ASV_MAGIC) {
		if (mod)
			asv += mod;
	} else {
		asv = force_asv_group[domain & 0x0000FFFF] & 0x0000FFFF;
	}

	if (asv < 0 || asv >= MAX_ASV_GROUP)
		BUG();	/* Never reach */

	return asv;
}

static unsigned int get_asv_voltage(enum dvfs_id domain, unsigned int lv)
{
	int asv;
	unsigned int ssa10, ssa11;
	unsigned int ssa0;
	unsigned int subgrp_index;
	const unsigned int *table;
	unsigned int volt;
	unsigned int *ssa1_table = NULL;
	unsigned int ssa0_base = 0, ssa0_offset = 0;

	switch (domain) {
	case cal_asv_dvfs_big:
		asv = get_asv_group(cal_asv_dvfs_big, lv);
		ssa10 = asv_tbl_info.mngs_ssa10;
		ssa11 = asv_tbl_info.mngs_ssa11;
		ssa0 = asv_tbl_info.mngs_ssa0;
		subgrp_index = big_subgrp_index;
		ssa0_base = big_ssa0_base;
		ssa0_offset = big_ssa0_offset;
		ssa1_table = big_ssa1_table;
		table = pwrcal_big_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_little:
		asv = get_asv_group(cal_asv_dvfs_little, lv);
		ssa10 = asv_tbl_info.apollo_ssa10;
		ssa11 = asv_tbl_info.apollo_ssa11;
		ssa0 = asv_tbl_info.apollo_ssa0;
		subgrp_index = little_subgrp_index;
		ssa0_base = little_ssa0_base;
		ssa0_offset = little_ssa0_offset;
		ssa1_table = little_ssa1_table;
		table = pwrcal_little_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_g3d:
		asv = get_asv_group(cal_asv_dvfs_g3d, lv);
		ssa10 = asv_tbl_info.g3d_ssa10;
		ssa11 = asv_tbl_info.g3d_ssa11;
		ssa0 = asv_tbl_info.g3d_ssa0;
		subgrp_index = g3d_subgrp_index;
		ssa0_base = g3d_ssa0_base;
		ssa0_offset = g3d_ssa0_offset;
		ssa1_table = g3d_ssa1_table;
		table = pwrcal_g3d_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_mif:
		asv = get_asv_group(cal_asv_dvfs_mif, lv);
		ssa10 = asv_tbl_info.mif_ssa10;
		ssa11 = asv_tbl_info.mif_ssa11;
		ssa0 = asv_tbl_info.mif_ssa0;
		subgrp_index = mif_subgrp_index;
		ssa0_base = mif_ssa0_base;
		ssa0_offset = mif_ssa0_offset;
		ssa1_table = mif_ssa1_table;
		table = pwrcal_mif_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_int:
		asv = get_asv_group(cal_asv_dvfs_int, lv);
		ssa10 = asv_tbl_info.int_ssa10;
		ssa11 = asv_tbl_info.int_ssa11;
		ssa0 = asv_tbl_info.int_ssa0;
		subgrp_index = int_subgrp_index;
		ssa0_base = int_ssa0_base;
		ssa0_offset = int_ssa0_offset;
		ssa1_table = int_ssa1_table;
		table = pwrcal_int_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_cam:
		asv = get_asv_group(cal_asv_dvfs_cam, lv);
		ssa10 = asv_tbl_info.disp_ssa10;
		ssa11 = asv_tbl_info.disp_ssa11;
		ssa0 = asv_tbl_info.disp_ssa0;
		subgrp_index = cam_subgrp_index;
		ssa0_base = cam_ssa0_base;
		ssa0_offset = cam_ssa0_offset;
		ssa1_table = cam_ssa1_table;
		table = pwrcal_cam_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_disp:
		asv = get_asv_group(cal_asv_dvfs_disp, lv);
		ssa10 = asv_tbl_info.disp_ssa10;
		ssa11 = asv_tbl_info.disp_ssa11;
		ssa0 = asv_tbl_info.disp_ssa0;
		subgrp_index = disp_subgrp_index;
		ssa0_base = disp_ssa0_base;
		ssa0_offset = disp_ssa0_offset;
		ssa1_table = disp_ssa1_table;
		table = pwrcal_disp_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvs_g3dm:
		asv = get_asv_group(cal_asv_dvs_g3dm, lv);
		ssa10 = asv_tbl_info.g3d_ssa10;
		ssa11 = asv_tbl_info.g3d_ssa11;
		ssa0 = asv_tbl_info.g3d_ssa0;
		ssa1_table = g3d_ssa1_table;
		subgrp_index = g3dm_subgrp_index;
		table = pwrcal_g3dm_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	default:
		BUG();	/* Never reach */
		break;
	}

	volt = table[asv];

	if (ssa1_table) {
		if (lv < subgrp_index)
			volt += ssa1_table[ssa10];
		else
			volt += ssa1_table[ssa11 + 4];
	}
	if (ssa0_base)
		if (volt < ssa0_base + ssa0 * ssa0_offset)
			volt = ssa0_base + ssa0 * ssa0_offset;

	if (domain == cal_asv_dvs_g3dm) {
		if (volt < (asv_tbl_info.g3dm_vthr1 * 50000 + 750000))
			volt = asv_tbl_info.g3dm_vthr1 * 50000 + 750000;

		if (volt < (asv_tbl_info.g3dm_vthr2 * 50000 + 750000))
			volt = asv_tbl_info.g3dm_vthr2 * 50000 + 750000;
	}

	return volt;
}

static int dvfsbig_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_big->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_big, lv);

	return max_lv;
}

static int dvfslittle_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_little->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_little, lv);

	return max_lv;
}

static int dvfsg3d_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_g3d->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_g3d, lv);

	return max_lv;
}

static int dvfsmif_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_mif->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_mif, lv);

	return max_lv;
}

static int dvfsint_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_int->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_int, lv);

	return max_lv;
}

static int dvfscam_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_cam->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_cam, lv);

	return max_lv;
}

static int dvfsdisp_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_disp->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_disp, lv);

	return max_lv;
}

static int dvsg3dm_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvs_g3dm->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvs_g3dm, lv);

	return max_lv;
}

static int dfsg3d_set_ema(unsigned int volt)
{
	if (volt > 750000)
		pwrcal_writel(EMA_RF2_UHD_CON, asv_tbl_info.g3d_mcs0);
	else
		pwrcal_writel(EMA_RF2_UHD_CON, asv_tbl_info.g3d_mcs1);

	return 0;
}

unsigned long long pwrcal_get_dram_manufacturer(void);
static void asv_voltage_init_table(struct asv_table_list **asv_table, struct pwrcal_vclk_dfs *dfs)
{
	int i, j, k;
	unsigned long long ect_query_key;
	void *asv_block, *margin_block, *tim_block;
	struct ect_timing_param_size *ect_mif;
	struct ect_voltage_domain *domain;
	struct ect_voltage_table *table;
	struct asv_table_entry *asv_entry;
	struct ect_margin_domain *margin_domain = NULL;
	unsigned int max_asv_version = 0;
	unsigned int *mif_volt_margin = NULL;

	asv_block = ect_get_block("ASV");
	if (asv_block == NULL)
		return;

	margin_block = ect_get_block("MARGIN");

	domain = ect_asv_get_domain(asv_block, dfs->vclk.name);
	if (domain == NULL)
		return;

	for (i = 0; i < domain->num_of_table; i++) {
		table = &domain->table_list[i];
		if (table->table_version > max_asv_version)
			max_asv_version = table->table_version;
	}

	if (asv_tbl_info.asv_table_ver > max_asv_version) {
		pr_err("There is no voltage table at ECT, PWRCAL force change table version from %d to %d\n", asv_tbl_info.asv_table_ver, max_asv_version);
		asv_tbl_info.asv_table_ver = max_asv_version;
	}

	tim_block = ect_get_block(BLOCK_TIMING_PARAM);
	if ((strcmp("dvfs_mif", dfs->vclk.name) == 0) && (tim_block != 0)) {
		ect_query_key = (pwrcal_get_dram_manufacturer() & 0xffffffffffffff00) | 0x3;
		ect_mif = ect_timing_param_get_key(tim_block, ect_query_key);
		if (ect_mif)
			mif_volt_margin = ect_mif->timing_parameter;
	}

	if (margin_block)
		margin_domain = ect_margin_get_domain(margin_block, dfs->vclk.name);

	*asv_table = kzalloc(sizeof(struct asv_table_list) * (max_asv_version + 1), GFP_KERNEL);
	if (*asv_table == NULL)
		return;

	for (i = 0; i <= max_asv_version; ++i) {
		for (j = 0; j < domain->num_of_table; j++) {
			table = &domain->table_list[j];
			if (table->table_version == i)
				break;
		}

		(*asv_table)[i].table_size = domain->num_of_table;
		(*asv_table)[i].table = kzalloc(sizeof(struct asv_table_entry) * domain->num_of_level, GFP_KERNEL);
		if ((*asv_table)[i].table == NULL)
			return;

		for (j = 0; j < domain->num_of_level; ++j) {
			asv_entry = &(*asv_table)[i].table[j];

			asv_entry->index = domain->level_list[j];
			asv_entry->voltage = kzalloc(sizeof(unsigned int) * domain->num_of_group, GFP_KERNEL);

			for (k = 0; k < domain->num_of_group; ++k) {
				if (table->voltages != NULL)
					asv_entry->voltage[k] = table->voltages[j * domain->num_of_group + k];
				else if (table->voltages_step != NULL)
					asv_entry->voltage[k] = table->voltages_step[j * domain->num_of_group + k] * table->volt_step;

				if (margin_domain != NULL) {
					if (margin_domain->offset != NULL)
						asv_entry->voltage[k] += margin_domain->offset[j * margin_domain->num_of_group + k];
					else
						asv_entry->voltage[k] += margin_domain->offset_compact[j * margin_domain->num_of_group + k] * margin_domain->volt_step;
				}

				if (mif_volt_margin != NULL) {
					asv_entry->voltage[k] = MIN((asv_entry->voltage[k] + mif_volt_margin[j]), MAX_VOLT_OF_MIF);
				}
			}
		}

		/* voltage clipping : VDD_MIF@L(i+1) = min(VDD_MIF@L(i), VDD_MIF@L(i+1)) */
		if (mif_volt_margin != NULL) {
			for (j = 1; j < domain->num_of_level; ++j) {
				for (k = 0; k < domain->num_of_group; ++k) {
					(*asv_table)[i].table[j].voltage[k] = MIN(((*asv_table)[i].table[j-1].voltage[k]), ((*asv_table)[i].table[j].voltage[k]));
				}
			}
		}
	}
}

static void asv_voltage_table_init(void)
{
	asv_voltage_init_table(&pwrcal_big_asv_table, asv_dvfs_big);
	asv_voltage_init_table(&pwrcal_little_asv_table, asv_dvfs_little);
	asv_voltage_init_table(&pwrcal_g3d_asv_table, asv_dvfs_g3d);
	asv_voltage_init_table(&pwrcal_mif_asv_table, asv_dvfs_mif);
	asv_voltage_init_table(&pwrcal_int_asv_table, asv_dvfs_int);
	asv_voltage_init_table(&pwrcal_cam_asv_table, asv_dvfs_cam);
	asv_voltage_init_table(&pwrcal_disp_asv_table, asv_dvfs_disp);
	asv_voltage_init_table(&pwrcal_g3dm_asv_table, asv_dvs_g3dm);
}

static void asv_ssa_init(void)
{
	int i;
	void *gen_block;
	struct ect_gen_param_table *param;
	unsigned int asv_table_version = asv_tbl_info.asv_table_ver;

	gen_block = ect_get_block("GEN");
	if (gen_block == NULL)
		return;

	param = ect_gen_param_get_table(gen_block, "SSA_BIG");
	if (param) {
		asv_table_version = asv_tbl_info.asv_table_ver;
		if (asv_table_version >= param->num_of_row)
			asv_table_version = param->num_of_row - 1;
		big_subgrp_index = param->parameter[asv_table_version * param->num_of_col + 1];
		big_ssa0_base = param->parameter[asv_table_version * param->num_of_col + 2];
		big_ssa0_offset = param->parameter[asv_table_version * param->num_of_col + 3];
		for (i = 0; i < 8; i++)
			big_ssa1_table[i] = param->parameter[asv_table_version * param->num_of_col + 4 + i];
	}
	param = ect_gen_param_get_table(gen_block, "SSA_LITTLE");
	if (param) {
		asv_table_version = asv_tbl_info.asv_table_ver;
		if (asv_table_version >= param->num_of_row)
			asv_table_version = param->num_of_row - 1;
		little_subgrp_index = param->parameter[asv_table_version * param->num_of_col + 1];
		little_ssa0_base = param->parameter[asv_table_version * param->num_of_col + 2];
		little_ssa0_offset = param->parameter[asv_table_version * param->num_of_col + 3];
		for (i = 0; i < 8; i++)
			little_ssa1_table[i] = param->parameter[asv_table_version * param->num_of_col + 4 + i];
	}
	param = ect_gen_param_get_table(gen_block, "SSA_G3D");
	if (param) {
		asv_table_version = asv_tbl_info.asv_table_ver;
		if (asv_table_version >= param->num_of_row)
			asv_table_version = param->num_of_row - 1;
		g3d_subgrp_index = param->parameter[asv_table_version * param->num_of_col + 1];
		g3d_ssa0_base = param->parameter[asv_table_version * param->num_of_col + 2];
		g3d_ssa0_offset = param->parameter[asv_table_version * param->num_of_col + 3];
		for (i = 0; i < 8; i++)
			g3d_ssa1_table[i] = param->parameter[asv_table_version * param->num_of_col + 4 + i];
	}
	param = ect_gen_param_get_table(gen_block, "SSA_MIF");
	if (param) {
		asv_table_version = asv_tbl_info.asv_table_ver;
		if (asv_table_version >= param->num_of_row)
			asv_table_version = param->num_of_row - 1;
		mif_subgrp_index = param->parameter[asv_table_version * param->num_of_col + 1];
		mif_ssa0_base = param->parameter[asv_table_version * param->num_of_col + 2];
		mif_ssa0_offset = param->parameter[asv_table_version * param->num_of_col + 3];
		for (i = 0; i < 8; i++)
			mif_ssa1_table[i] = param->parameter[asv_table_version * param->num_of_col + 4 + i];
	}
	param = ect_gen_param_get_table(gen_block, "SSA_INT");
	if (param) {
		asv_table_version = asv_tbl_info.asv_table_ver;
		if (asv_table_version >= param->num_of_row)
			asv_table_version = param->num_of_row - 1;
		int_subgrp_index = param->parameter[asv_table_version * param->num_of_col + 1];
		int_ssa0_base = param->parameter[asv_table_version * param->num_of_col + 2];
		int_ssa0_offset = param->parameter[asv_table_version * param->num_of_col + 3];
		for (i = 0; i < 8; i++)
			int_ssa1_table[i] = param->parameter[asv_table_version * param->num_of_col + 4 + i];
	}
	param = ect_gen_param_get_table(gen_block, "SSA_CAM");
	if (param) {
		asv_table_version = asv_tbl_info.asv_table_ver;
		if (asv_table_version >= param->num_of_row)
			asv_table_version = param->num_of_row - 1;
		cam_subgrp_index = param->parameter[asv_table_version * param->num_of_col + 1];
		cam_ssa0_base = param->parameter[asv_table_version * param->num_of_col + 2];
		cam_ssa0_offset = param->parameter[asv_table_version * param->num_of_col + 3];
		for (i = 0; i < 8; i++)
			cam_ssa1_table[i] = param->parameter[asv_table_version * param->num_of_col + 4 + i];
	}
	param = ect_gen_param_get_table(gen_block, "SSA_DISP");
	if (param) {
		asv_table_version = asv_tbl_info.asv_table_ver;
		if (asv_table_version >= param->num_of_row)
			asv_table_version = param->num_of_row - 1;
		disp_subgrp_index = param->parameter[asv_table_version * param->num_of_col + 1];
		disp_ssa0_base = param->parameter[asv_table_version * param->num_of_col + 2];
		disp_ssa0_offset = param->parameter[asv_table_version * param->num_of_col + 3];
		for (i = 0; i < 8; i++)
			disp_ssa1_table[i] = param->parameter[asv_table_version * param->num_of_col + 4 + i];
	}
	g3dm_subgrp_index = g3d_subgrp_index;
}

static int asv_init(void)
{
	struct vclk *vclk;

	vclk = cal_get_vclk(dvfs_big);
	asv_dvfs_big = to_dfs(vclk);
	asv_dvfs_big->dfsops->get_asv_table = dvfsbig_get_asv_table;
/*	asv_dvfs_big->dfsops->set_ema = dvfsbig_set_ema;
*/
	vclk = cal_get_vclk(dvfs_little);
	asv_dvfs_little = to_dfs(vclk);
	asv_dvfs_little->dfsops->get_asv_table = dvfslittle_get_asv_table;
/*	asv_dvfs_little->dfsops->set_ema = dvfslittle_set_ema;
*/
	vclk = cal_get_vclk(dvfs_g3d);
	asv_dvfs_g3d = to_dfs(vclk);
	asv_dvfs_g3d->dfsops->get_asv_table = dvfsg3d_get_asv_table;
	asv_dvfs_g3d->dfsops->set_ema = dfsg3d_set_ema;

	vclk = cal_get_vclk(dvfs_mif);
	asv_dvfs_mif = to_dfs(vclk);
	asv_dvfs_mif->dfsops->get_asv_table = dvfsmif_get_asv_table;

	vclk = cal_get_vclk(dvfs_int);
	asv_dvfs_int = to_dfs(vclk);
	asv_dvfs_int->dfsops->get_asv_table = dvfsint_get_asv_table;
/*	asv_dvfs_int->dfsops->set_ema = dvfsint_set_ema;
*/
	vclk = cal_get_vclk(dvfs_cam);
	asv_dvfs_cam = to_dfs(vclk);
	asv_dvfs_cam->dfsops->get_asv_table = dvfscam_get_asv_table;

	vclk = cal_get_vclk(dvfs_disp);
	asv_dvfs_disp = to_dfs(vclk);
	asv_dvfs_disp->dfsops->get_asv_table = dvfsdisp_get_asv_table;

	vclk = cal_get_vclk(dvs_g3dm);
	asv_dvs_g3dm = to_dfs(vclk);
	asv_dvs_g3dm->dfsops->get_asv_table = dvsg3dm_get_asv_table;

	pwrcal_big_asv_table = NULL;
	pwrcal_little_asv_table = NULL;
	pwrcal_g3d_asv_table = NULL;
	pwrcal_mif_asv_table = NULL;
	pwrcal_int_asv_table = NULL;
	pwrcal_cam_asv_table = NULL;
	pwrcal_disp_asv_table = NULL;
	pwrcal_g3dm_asv_table = NULL;

	asv_get_asvinfo();
	asv_voltage_table_init();
	asv_ssa_init();

	return 0;
}

static void asv_print_info(void)
{
	pr_info("asv_table_ver : %d\n", asv_tbl_info.asv_table_ver);
	pr_info("fused_grp : %d\n", asv_tbl_info.fused_grp);

	pr_info("mngs_asv_group : %d\n", asv_tbl_info.mngs_asv_group);
	pr_info("mngs_modified_group : %d\n", asv_tbl_info.mngs_modified_group);
	pr_info("apollo_asv_group : %d\n", asv_tbl_info.apollo_asv_group);
	pr_info("apollo_modified_group : %d\n", asv_tbl_info.apollo_modified_group);

	pr_info("g3d_asv_group : %d\n", asv_tbl_info.g3d_asv_group);
	pr_info("g3d_modified_group : %d\n", asv_tbl_info.g3d_modified_group);
	pr_info("mif_asv_group : %d\n", asv_tbl_info.mif_asv_group);
	pr_info("mif_modified_group : %d\n", asv_tbl_info.mif_modified_group);

	pr_info("int_asv_group : %d\n", asv_tbl_info.int_asv_group);
	pr_info("int_modified_group : %d\n", asv_tbl_info.int_modified_group);
	pr_info("disp_asv_group : %d\n", asv_tbl_info.disp_asv_group);
	pr_info("disp_modified_group : %d\n", asv_tbl_info.disp_modified_group);

	pr_info("g3d_mcs0 : %d\n", asv_tbl_info.g3d_mcs0);
	pr_info("g3d_mcs1 : %d\n", asv_tbl_info.g3d_mcs1);
}

static int asv_get_grp(unsigned int id, unsigned int lv)
{
	return get_asv_group((enum dvfs_id)id, lv);
}

static int asv_get_tablever(void)
{
	return (int)(asv_tbl_info.asv_table_ver);
}

static int asv_get_ids_info(unsigned int domain)
{
#ifdef PWRCAL_TARGET_LINUX
	int ret;
	register unsigned long long reg0 __asm__("x0");
	register unsigned long long reg1 __asm__("x1");
	register unsigned long long reg2 __asm__("x2");
	register unsigned long long reg3 __asm__("x3");

	if (domain >= 2) {
		pr_err("[GET_IDS_INFO] Domain is invalid (domain : %d)\n", domain);
		return -1;
	}

	reg0 = 0;
	reg1 = 0;
	reg2 = 0;
	reg3 = 0;

	/* SMC_ID = 0x82001014, CMD_ID = 0x2002, read_idx=0x18 */
	ret = exynos_smc(0x82001014, 0, 0x2002, 0x18);
	__asm__ volatile(
		"\t"
		: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	);

	if (ret) {
		pr_err("[GET_IDS_INFO] SMC error (0x%LX)\n", reg0);
		return -1;
	}

	if (domain == 0)	/* MNGS */
		ret = (int)(reg2 & 0xFF);
	else if (domain == 1)	/* G3D */
		ret = (int)((reg2 >> 8) & 0xFF);

	return ret;
#else
	return 0;
#endif
}

#if defined(CONFIG_SEC_FACTORY) || defined(CONFIG_SEC_DEBUG)
enum ids_info {
	tg,
	lg,
	bg,
	g3dg,
	mifg,
	bids,
	gids,
};

int asv_ids_information(enum ids_info id) {

	int res;

	switch (id) {
		case tg:
			res = asv_tbl_info.asv_table_ver;
			break;
		case lg:
			res = asv_tbl_info.apollo_asv_group;
			break;
		case bg:
			res = asv_tbl_info.mngs_asv_group;
			break;
		case g3dg:
			res = asv_tbl_info.g3d_asv_group;
			break;
		case mifg:
			res = asv_tbl_info.mif_asv_group;
			break;
		case bids:
			res = asv_get_ids_info(0);
			break;
		case gids:
			res = asv_get_ids_info(1);
			break;
		default:
			res = 0;
			break;
		};
	return res;

}

#endif


#if defined(CONFIG_SEC_PM_DEBUG)
enum asv_group {
	asv_max_lv,
	dvfs_freq,
	dvfs_voltage,
	dvfs_group,
	table_group,
	num_of_asc,
};

int asv_get_information(enum dvfs_id id, enum asv_group grp, unsigned int lv) {

	int max_lv, volt, group, asv;
	void *asv_block;
	struct ect_voltage_domain *domain;

	asv_block = ect_get_block("ASV");
	if (asv_block == NULL)
		BUG();

	if (!asv_tbl_info.fused_grp)
		goto notfused;

	switch (id) {
		case cal_asv_dvfs_big:
			max_lv = asv_dvfs_big->table->num_of_lv;
			domain = ect_asv_get_domain(asv_block, "dvfs_big");
			volt = get_asv_voltage(cal_asv_dvfs_big, lv);
			group = asv_tbl_info.mngs_asv_group;
			asv = get_asv_group(cal_asv_dvfs_big, lv);
			break;
		case cal_asv_dvfs_little:
			max_lv = asv_dvfs_little->table->num_of_lv;
			domain = ect_asv_get_domain(asv_block, "dvfs_little");
			volt = get_asv_voltage(cal_asv_dvfs_little, lv);
			group = asv_tbl_info.apollo_asv_group;
			asv = get_asv_group(cal_asv_dvfs_little, lv);
			break;
		case cal_asv_dvfs_g3d:
			max_lv = asv_dvfs_g3d->table->num_of_lv;
			domain = ect_asv_get_domain(asv_block, "dvfs_g3d");
			volt = get_asv_voltage(cal_asv_dvfs_little, lv);
			group = asv_tbl_info.g3d_asv_group;
			asv = get_asv_group(cal_asv_dvfs_g3d, lv);
			break;
		case cal_asv_dvfs_mif:
			max_lv = asv_dvfs_mif->table->num_of_lv;
			domain = ect_asv_get_domain(asv_block, "dvfs_mif");
			volt = get_asv_voltage(cal_asv_dvfs_little, lv);
			group = asv_tbl_info.mif_asv_group;
			asv = get_asv_group(cal_asv_dvfs_mif, lv);
			break;
		case cal_asv_dvfs_int:
			max_lv = asv_dvfs_int->table->num_of_lv;
			domain = ect_asv_get_domain(asv_block, "dvfs_int");
			volt = get_asv_voltage(cal_asv_dvfs_little, lv);
			group = asv_tbl_info.int_asv_group;
			break;
		case cal_asv_dvfs_cam:
			max_lv = asv_dvfs_cam->table->num_of_lv;
			domain = ect_asv_get_domain(asv_block, "dvfs_cam");
			volt = get_asv_voltage(cal_asv_dvfs_little, lv);
			group = asv_tbl_info.disp_asv_group;
			break;
		case cal_asv_dvfs_disp:
			max_lv = asv_dvfs_disp->table->num_of_lv;
			domain = ect_asv_get_domain(asv_block, "dvfs_disp");
			volt = get_asv_voltage(cal_asv_dvfs_little, lv);
			group = asv_tbl_info.disp_asv_group;
			break;
		case cal_asv_dvs_g3dm:
			max_lv = asv_dvs_g3dm->table->num_of_lv;
			domain = ect_asv_get_domain(asv_block, "dvs_g3dm");
			volt = get_asv_voltage(cal_asv_dvs_g3dm, lv);
			group = 0;
			break;
		default:
			break;
	}

	if (grp == asv_max_lv)
		return max_lv;
	else if (grp == dvfs_freq)
		return domain->level_list[lv] * 1000;
	else if (grp == dvfs_voltage)
		return volt;
	else if (grp == dvfs_group)
		return group;
	else if (grp == table_group)
		return asv_tbl_info.asv_table_ver;
	else
		return 0;

notfused:

	return 0;

}
#endif /* CONFIG_SEC_PM_DEBUG */


struct cal_asv_ops cal_asv_ops = {
	.print_asv_info = asv_print_info,
	.set_grp = asv_set_grp,
	.get_grp = asv_get_grp,
	.set_tablever = asv_set_tablever,
	.get_tablever = asv_get_tablever,
	.asv_init = asv_init,
	.asv_pmic_info = asv_get_pmic_info,
	.get_ids_info = asv_get_ids_info,
	.set_ssa0 = asv_set_ssa0,
};
