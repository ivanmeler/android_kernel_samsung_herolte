/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * Header file of MobiCore Driver Kernel Module Platform
 * specific structures
 *
 * Internal structures of the McDrvModule
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 */
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

#define IRQ_SPI(x)      (x + 32)

/* MobiCore Interrupt. */
#if defined(CONFIG_SOC_EXYNOS3250) || defined(CONFIG_SOC_EXYNOS3472)
#define MC_INTR_SSIQ	254
#elif defined(CONFIG_SOC_EXYNOS3475) || defined(CONFIG_SOC_EXYNOS5430) || \
	defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS8890) || defined(CONFIG_SOC_EXYNOS7880)
#define MC_INTR_SSIQ	255
#elif defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7580)
#define MC_INTR_SSIQ	246
#endif

/* Enable Runtime Power Management */
#if defined(CONFIG_SOC_EXYNOS3472)
#ifdef CONFIG_PM_RUNTIME
#define MC_PM_RUNTIME
#endif
#endif /* CONFIG_SOC_EXYNOS3472 */

#if !defined(CONFIG_SOC_EXYNOS3472)

#define TBASE_CORE_SWITCHER

#if defined(CONFIG_SOC_EXYNOS3250)
#define COUNT_OF_CPUS 2
#elif defined(CONFIG_SOC_EXYNOS3475)
#define COUNT_OF_CPUS 4
#else
#define COUNT_OF_CPUS 8
#endif

/* Values of MPIDR regs */
#if defined(CONFIG_SOC_EXYNOS3250) || defined(CONFIG_SOC_EXYNOS3475)
#define CPU_IDS {0x0000, 0x0001, 0x0002, 0x0003}
#elif defined(CONFIG_SOC_EXYNOS7580) || defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
#define CPU_IDS {0x0000, 0x0001, 0x0002, 0x0003, 0x0100, 0x0101, 0x0102, 0x0103}
#else
#define CPU_IDS {0x0100, 0x0101, 0x0102, 0x0103, 0x0000, 0x0001, 0x0002, 0x0003}
#endif

#endif /* !CONFIG_SOC_EXYNOS3472 */

/* uidgid.h does not exist in kernels before 3.5 */
#if defined(CONFIG_SOC_EXYNOS3250) || defined(CONFIG_SOC_EXYNOS3472) || \
	defined(CONFIG_SOC_EXYNOS3475)
#define MC_NO_UIDGIT_H
#endif /* CONFIG_SOC_EXYNOS3250|CONFIG_SOC_EXYNOS3472|CONFIG_SOC_EXYNOS3475 */

/* SWd LPAE */
#if defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7420) || \
	defined(CONFIG_SOC_EXYNOS7580) || defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS8890) || defined(CONFIG_SOC_EXYNOS7880)
#ifndef CONFIG_TRUSTONIC_TEE_LPAE
#define CONFIG_TRUSTONIC_TEE_LPAE
#endif
#endif /* CONFIG_SOC_EXYNOS5433 */

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

/* Set Parameters for Secure OS Boosting */
#define DEFAULT_LITTLE_CORE		0
#define NONBOOT_LITTLE_CORE		1
#define DEFAULT_BIG_CORE		4
#define MIGRATE_TARGET_CORE     DEFAULT_BIG_CORE

#define MC_INTR_LOCAL_TIMER            (IRQ_SPI(85) + DEFAULT_BIG_CORE)

#define LOCAL_TIMER_PERIOD             50

#define DEFAULT_SECOS_BOOST_TIME       5000
#define MAX_SECOS_BOOST_TIME           600000  /* 600 sec */

#define DUMP_TBASE_HALT_STATUS

#endif /* _MC_DRV_PLATFORM_H_ */
