/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register map for Power management unit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __REGS_PMU_EXYNOS8890_H__
#define __REGS_PMU_EXYNOS8890_H__ __FILE__

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
#define EXYNOS_PMU_CENTRAL_SEQ_CP_CONFIGURATION	0x0280
#define EXYNOS_PMU_RESET_AHEAD_CP_SYS_PWR_REG	0x1170
#define EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG	0x11CC
#define EXYNOS_PMU_LOGIC_RESET_CP_SYS_PWR_REG	0x11D0
#define EXYNOS_PMU_TCXO_GATE_SYS_PWR_REG	0x11D4
#define EXYNOS_PMU_RESET_ASB_CP_SYS_PWR_REG	0x11D8

/* CP PMU */
/* For EXYNOS_PMU_CP_CTRL Register */
#define CP_PWRON                BIT(1)
#define CP_RESET_SET            BIT(2)
#define CP_START                BIT(3)
#define CP_ACTIVE_REQ_EN        BIT(5)
#define CP_ACTIVE_REQ_CLR       BIT(6)
#define CP_RESET_REQ_EN         BIT(7)
#define CP_RESET_REQ_CLR        BIT(8)
#define MASK_CP_PWRDN_DONE      BIT(9)
#define RTC_OUT_EN              BIT(10)
#define MASK_SLEEP_START_REQ    BIT(12)
#define SET_SW_SLEEP_START_REQ  BIT(13)
#define CLEANY_BYPASS_END       BIT(16)

#endif /* __REGS_PMU_EXYNOS8890_H__ */
