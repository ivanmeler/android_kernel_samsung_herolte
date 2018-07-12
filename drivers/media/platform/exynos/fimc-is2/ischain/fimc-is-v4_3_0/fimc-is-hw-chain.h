/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_CHAIN_V4_0_H
#define FIMC_IS_HW_CHAIN_V4_0_H

#include "fimc-is-hw-api-common.h"

/* the total count */
#define HW_MCUCTL_REG_CNT 2
enum fimc_is_hw_chain_reg_name {
	MCUCTRLR,
	MCUCTRLR2,
};

static struct fimc_is_reg mcuctl_regs[HW_MCUCTL_REG_CNT] = {
	{0x0000, "MCUCTRLR"},
	{0x0060, "MCUCTRLR2"},
};

/* the total count of the fields */
#define HW_MCUCTL_REG_FIELD_CNT 21
enum fimc_is_hw_mcuctl_reg_field {
	MCUCTRLR2_IN_PATH_SEL_MCS1,
	MCUCTRLR2_IN_PATH_SEL_TPU,
	MCUCTRLR2_OUT_PATH_SEL_TPU,
	MCUCTRLR2_ISPCPU_RT_INFO,
	MCUCTRLR2_FIMC_VRA_RT_INFO,
	MCUCTRLR2_CSIS_3_RT_INFO,
	MCUCTRLR2_CSIS_2_RT_INFO,
	MCUCTRLR2_SHARED_BUFFER_SEL,
	MCUCTRLR2_IN_PATH_SEL_BNS,
	MCUCTRLR2_IN_PATH_SEL_3AA0,
	MCUCTRLR2_IN_PATH_SEL_3AA1,
	MCUCTRLR2_IN_PATH_SEL_ISP0,
	MCUCTRLR2_IN_PATH_SEL_ISP1,
	MCUCTRLR2_FIMC_3AA0_RT_INFO,
	MCUCTRLR2_FIMC_3AA1_RT_INFO,
	MCUCTRLR2_FIMC_ISP0_RT_INFO,
	MCUCTRLR2_FIMC_ISP1_RT_INFO,
	MCUCTRLR2_MC_SCALER_RT_INFO,
	MCUCTRLR2_FIMC_TPU_RT_INFO,
	MCUCTRLR2_AXI_TREX_C_AWCACHE,
	MCUCTRLR2_AXI_TREX_C_ARCACHE
};

static struct fimc_is_field mcuctl_fields[HW_MCUCTL_REG_FIELD_CNT] = {
	{"IN_PATH_SEL_MCS1"   , 31, 1, RW, 0},
	{"IN_PATH_SEL_TPU"    , 30, 1, RW, 0},
	{"OUT_PATH_SEL_TPU"   , 29, 1, RW, 0},
	{"ISPCPU_RT_INFO"     , 27, 1, RW, 0x1},
	{"FIMC_VRA_RT_INFO"   , 26, 1, RW, 0  },
	{"CSIS_3_RT_INFO"     , 25, 1, RW, 0x1},
	{"CSIS_2_RT_INFO"     , 24, 1, RW, 0x1},
	{"SHARED_BUFFER_SEL"  , 23, 1, RW, 0  },
	{"IN_PATH_SEL_BNS"    , 22, 1, RW, 0  },
	{"IN_PATH_SEL_3AAA0"  , 20, 2, RW, 0  },
	{"IN_PATH_SEL_3AA1"   , 18, 2, RW, 0  },
	{"IN_PATH_SEL_ISP0"   , 17, 1, RW, 0  },
	{"IN_PATH_SEL_ISP1"   , 16, 1, RW, 0  },
	{"FIMC_3AA0_RT_INFO"  , 13, 1, RW, 0  },
	{"FIMC_3AA1_RT_INFO"  , 12, 1, RW, 0  },
	{"FIMC_ISP0_RT_INFO"  , 11, 1, RW, 0  },
	{"FIMC_ISP1_RT_INFO"  , 10, 1, RW, 0  },
	{"MC_SCALER_RT_INFO"  , 9 , 1, RW, 0  },
	{"FIMC_TPU_RT_INFO"   , 8 , 1, RW, 0  },
	{"AXI_TREX_C_AWCACHE" , 4 , 4, RW, 0x2},
	{"AXI_TREX_C_ARCACHE" , 0 , 4, RW, 0x2},
};
#endif
