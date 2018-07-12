/* linux/arch/arm64/mach-exynos/include/mach/exynos-devfreq.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __EXYNOS_DEVFREQ_H_
#define __EXYNOS_DEVFREQ_H_

#include <linux/devfreq.h>
#include <linux/pm_qos.h>
#include <soc/samsung/exynos-devfreq-dep.h>

#define EXYNOS_DEVFREQ_MODULE_NAME	"exynos-devfreq"
#define VOLT_STEP			25000

enum exynos_devfreq_type {
	DEVFREQ_MIF = 0,
	DEVFREQ_INT,
	DEVFREQ_DISP,
	DEVFREQ_CAM,
	DEVFREQ_TYPE_END
};

enum exynos_devfreq_gov_type {
	SIMPLE_ONDEMAND = 0,
	SIMPLE_EXYNOS,
	GOV_TYPE_END
};

enum PPMU_TYPE {
	PPMU_MIF = 0,
	PPMU_INT,
	NONE_PPMU
};

enum volt_order_type {
	KEEP_SET_VOLT = 0,
	PRE_SET_VOLT,
	POST_SET_VOLT
};

enum exynos_devfreq_lv_index {
	DEV_LV0 = 0,
	DEV_LV1,
	DEV_LV2,
	DEV_LV3,
	DEV_LV4,
	DEV_LV5,
	DEV_LV6,
	DEV_LV7,
	DEV_LV8,
	DEV_LV9,
	DEV_LV10,
	DEV_LV11,
	DEV_LV12,
	DEV_LV13,
	DEV_LV14,
	DEV_LV15,
	DEV_LV16,
	DEV_LV17,
	DEV_LV18,
	DEV_LV19,
	DEV_LV20,
	DEV_LV_END,
};

struct exynos_devfreq_opp_table {
	u32 idx;
	u32 freq;
	u32 volt;
};

struct exynos_devfreq_data;
struct ppmu_exynos;

struct exynos_devfreq_ops {
	int (*init)(struct device *, struct exynos_devfreq_data *);
	int (*exit)(struct device *, struct exynos_devfreq_data *);
	int (*init_freq_table)(struct device *, struct exynos_devfreq_data *);
	int (*get_volt_table)(struct device *, u32 *, struct exynos_devfreq_data *);
	int (*ppmu_register)(struct device *, struct exynos_devfreq_data *);
	int (*ppmu_unregister)(struct device *, struct exynos_devfreq_data *);
	int (*pm_suspend_prepare)(struct device *, struct exynos_devfreq_data *);
	int (*pm_post_suspend)(struct device *, struct exynos_devfreq_data *);
	int (*suspend)(struct device *, struct exynos_devfreq_data *);
	int (*resume)(struct device *, struct exynos_devfreq_data *);
	int (*reboot)(struct device *, struct exynos_devfreq_data *);
	int (*get_switch_voltage)(u32, u32, struct exynos_devfreq_data *);
	int (*set_voltage)(struct device *, u32 *, struct exynos_devfreq_data *);
	void (*set_voltage_prepare)(struct exynos_devfreq_data *);
	void (*set_voltage_post)(struct exynos_devfreq_data *);
	u32 (*get_target_freq)(char *, u32);
	int (*get_switch_freq)(u32, u32, u32 *);
	int (*get_freq)(struct device *, u32 *, struct exynos_devfreq_data *);
	int (*set_freq)(struct device *, u32, u32, struct exynos_devfreq_data *);
	int (*pre_update_target)(struct device *, struct exynos_devfreq_data *);
	int (*set_freq_prepare)(struct device *, struct exynos_devfreq_data *);
	int (*set_freq_post)(struct device *, struct exynos_devfreq_data *);
	int (*change_to_switch_freq)(struct device *, struct exynos_devfreq_data *);
	int (*restore_from_switch_freq)(struct device *, struct exynos_devfreq_data *);
	int (*get_dev_status)(struct device *, struct ppmu_exynos *, struct exynos_devfreq_data *);
	int (*cl_dvfs_start)(struct exynos_devfreq_data *);
	int (*cl_dvfs_stop)(u32, struct exynos_devfreq_data *);
	int (*cmu_dump)(struct device *, struct exynos_devfreq_data *);
};

struct ppmu_addr {
	void __iomem *base;
};

struct ppmu_exynos {
	struct list_head node;
	struct ppmu_addr *ppmu_list;
	unsigned int ppmu_count;
	u64 val_ccnt;
	u64 val_pmcnt;
	enum PPMU_TYPE type;
};

struct exynos_devfreq_data {
	struct device				*dev;
	struct devfreq				*devfreq;
	struct mutex				lock;
	struct clk				*clk;
	struct clk				*sw_clk;

	bool					devfreq_disabled;

	enum exynos_devfreq_type		devfreq_type;

	u32					opp_list_length;
	struct exynos_devfreq_opp_table		opp_list[DEV_LV_END];
	u32					volt_table[DEV_LV_END];

	u32					default_qos;

	bool					use_get_dev;
	u32					max_state;
	struct devfreq_dev_profile		devfreq_profile;

	enum exynos_devfreq_gov_type		gov_type;
	const char				*governor_name;
	u32					cal_qos_max;
	void					*governor_data;
#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SIMPLE_EXYNOS)
	struct devfreq_simple_exynos_data	simple_exynos_data;
#endif
#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
	struct devfreq_simple_ondemand_data	simple_ondemand_data;
#endif

	s32					old_idx;
	s32					new_idx;
	u32					old_freq;
	u32					new_freq;
	u32					min_freq;
	u32					max_freq;

	u32					old_volt;
	u32					new_volt;
	u32					volt_offset;
	u32					cold_volt_offset;
	u32					limit_cold_volt;
	u32					min_cold_volt;
	u32					reg_max_volt;
	bool					use_regulator;
	bool					use_regulator_dummy;
	const char				*regulator_name;
	struct regulator			*vdd;
	struct regulator			*vdd_dummy;
	struct mutex				regulator_lock;
	enum volt_order_type			set_volt_order;

	u32					pm_qos_class;
	u32					pm_qos_class_max;
	struct pm_qos_request			sys_pm_qos;
	struct pm_qos_request			sys_pm_qos_max;
	struct pm_qos_request			default_pm_qos;
	struct pm_qos_request			boot_pm_qos;
	u32					boot_qos_timeout;

	u32					ppmu_base[16];
	struct devfreq_notifier_block		*ppmu_nb;
	struct ppmu_exynos			ppmu_data;
	bool					use_ppmu;
	u32					last_monitor_period;
	u64					last_monitor_jiffies;
	u32					last_ppmu_usage_rate;

	bool					use_tmu;
	struct notifier_block			tmu_notifier;
	struct notifier_block			reboot_notifier;
	struct notifier_block			pm_notifier;

	u32					ess_flag;

	bool					use_cl_dvfs;
	u32					cl_domain;

	s32					target_delay;
	s32					setfreq_delay;

	bool					use_switch_clk;
	u32					switch_freq;
	u32					switch_volt;

	void					*private_data;
	struct exynos_devfreq_ops		ops;
};

int register_exynos_devfreq_init_prepare(enum exynos_devfreq_type type,
				int (*func)(struct exynos_devfreq_data *));
s32 exynos_devfreq_get_opp_idx(struct exynos_devfreq_opp_table *table,
				unsigned int size, u32 freq);
#if defined(CONFIG_ARM_EXYNOS_DEVFREQ)
u32 get_target_devfreq_rate(enum exynos_devfreq_type type, char *name, u32 freq);
int exynos_devfreq_sync_voltage(enum exynos_devfreq_type type, bool turn_on);
#else
static inline
u32 get_target_devfreq_rate(enum exynos_devfreq_type type, char *name, u32 freq)
{
	return 0;
}

static inline
int exynos_devfreq_sync_voltage(enum exynos_devfreq_type type, bool turn_on)
{
	return 0;
}
#endif
#endif	/* __EXYNOS_DEVFREQ_H_ */
