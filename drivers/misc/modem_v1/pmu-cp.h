/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __EXYNOS_PMU_CP_H
#define __EXYNOS_PMU_CP_H __FILE__

#include "modem_prj.h"

#ifdef CONFIG_SOC_EXYNOS8890
/* BLK_ALIVE: CP related SFRs */
#define EXYNOS_PMU_CP_CTRL_NS			0x0030
#define EXYNOS_PMU_CP_CTRL_S			0x0034
#define EXYNOS_PMU_CP_STAT			0x0038
#define EXYNOS_PMU_CP_DEBUG			0x003C
#define EXYNOS_PMU_CP_DURATION			0x0040
#define EXYNOS_PMU_CP2AP_MEM_CONFIG		0x0050
#define EXYNOS_PMU_CP2AP_MIF0_PERI_ACCESS_CON	0x0054
#define EXYNOS_PMU_CP2AP_MIF1_PERI_ACCESS_CON	0x0058
#define EXYNOS_PMU_CP2AP_MIF2_PERI_ACCESS_CON	0x005C
#define EXYNOS_PMU_CP2AP_MIF3_PERI_ACCESS_CON	0x0060
#define EXYNOS_PMU_CP2AP_CCORE_PERI_ACCESS_CON	0x0064
#define EXYNOS_PMU_CP_BOOT_TEST_RST_CONFIG	0x0068
#define EXYNOS_PMU_CP2AP_PERI_ACCESS_WIN	0x006C
#define EXYNOS_PMU_MODAPIF_CONFIG		0x0070
#define EXYNOS_PMU_CP_CLK_CTRL			0x0074
#define EXYNOS_PMU_CP_QOS			0x0078
#define EXYNOS_PMU_ERROR_CODE_DATA		0x007C
#define EXYNOS_PMU_ERROR_CODE_PERI		0x0080
#endif

#define SMC_ID		0x82000700
#define READ_CTRL	0x3
#define WRITE_CTRL	0x4

enum cp_mode {
	CP_POWER_ON,
	CP_RESET,
	CP_POWER_OFF,
	NUM_CP_MODE,
};

enum reset_mode {
	CP_HW_RESET,
	CP_SW_RESET,
};

enum cp_control {
	CP_CTRL_S,
	CP_CTRL_NS,
};

#if defined(CONFIG_SOC_EXYNOS8890)
extern int exynos_cp_reset(struct modem_ctl *);
extern int exynos_cp_release(struct modem_ctl *);
extern int exynos_cp_init(struct modem_ctl *);
extern int exynos_cp_active_clear(struct modem_ctl *);
extern int exynos_clear_cp_reset(struct modem_ctl *);
extern int exynos_get_cp_power_status(struct modem_ctl *);
extern int exynos_set_cp_power_onoff(struct modem_ctl *, enum cp_mode mode);
extern void exynos_sys_powerdown_conf_cp(struct modem_ctl *);
extern int exynos_pmu_cp_init(struct modem_ctl *);
#endif

#endif /* __EXYNOS_PMU_CP_H */
