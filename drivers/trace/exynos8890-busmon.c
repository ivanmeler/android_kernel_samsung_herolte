/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * BUS Monitor Debugging Driver for Samsung EXYNOS8890 SoC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#define pr_fmt(fmt) "[detect] abnormal access: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/exynos-busmon.h>
#include <linux/exynos-modem-ctrl.h>

#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif

/* S-NODE, M-NODE Common */
#define OFFSET_TIMEOUT_REG		(0x2000)
#define OFFSET_REQ_R			(0x0)
#define OFFSET_REQ_W			(0x20)
#define OFFSET_RESP_R			(0x40)
#define OFFSET_RESP_W			(0x60)
#define OFFSET_ERR_REPT			(0x20)

#define REG_INT_MASK			(0x0)
#define REG_INT_CLR			(0x4)
#define REG_INT_INFO			(0x8)
#define REG_EXT_INFO_0			(0x10)
#define REG_EXT_INFO_1			(0x14)
#define REG_EXT_INFO_2			(0x18)

#define REG_DBG_CTL			(0x10)
#define REG_TIMEOUT_INIT_VAL		(0x14)
#define REG_R_TIMEOUT_MO		(0x20)
#define REG_W_TIMEOUT_MO		(0x30)

#define BIT_ERR_CODE(x)			(((x) & (0xF << 28)) >> 28)
#define BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define BIT_ID_VAL(x)			(((x) & (0xFFFF)))
#define BIT_AXUSER(x)			(((x) & (0xFFFF << 16)) >> 16)

#define S_NODE				(0)
#define M_NODE				(1)
#define T_S_NODE			(2)
#define T_M_NODE			(3)

#define PATH_DATA_CCORE			(0)
#define PATH_DATA_BUS0			(1)
#define PATH_DATA_BUS1			(2)
#define PATH_SFR_CCORE			(3)
#define PATH_SFR_BUS0			(4)
#define PATH_SFR_BUS1			(5)
#define PATH_NUM			(6)

#define ERRCODE_SLVERR			(0)
#define ERRCODE_DECERR			(1)
#define ERRCODE_UNSUPORTED		(2)
#define	ERRCODE_POWER_DOWN		(3)
#define ERRCODE_UNKNOWN_4		(4)
#define	ERRCODE_UNKNOWN_5		(5)
#define	ERRCODE_TIMEOUT			(6)

#define TIMEOUT				(0xFFFFF)
#define TIMEOUT_TEST			(0x1)

#define NEED_TO_CHECK			(0xCAFE)

struct busmon_rpathinfo {
	unsigned int id;
	char *port_name;
	char *dest_name;
	unsigned int bits;
};

struct busmon_masterinfo {
	char *port_name;
	unsigned int user;
	char *master_name;
	unsigned int bits;
};

struct busmon_nodeinfo {
	unsigned int type;
	char *name;
	unsigned int phy_regs;
	void __iomem *regs;
	unsigned int irq;
	unsigned int time_val;
	bool timeout_enabled;
	bool err_rpt_enabled;
	char *need_rpath;
	char *comment;
};

/* Error Code Description */
static char *busmon_errcode[] = {
	"Error Detect by the Slave(SLVERR)",
	"Decode error(DECERR)",
	"Unsupported transaction error",
	"Power Down access error",
	"Unsupported transaction",
	"Unsupported transaction",
	"Timeout error - response timeout",
	"Invalid errorcode",
};

struct busmon_nodegroup {
	int irq;
	char *name;
	struct busmon_nodeinfo *nodeinfo;
	unsigned int nodesize;
	unsigned int irq_occurred;
	bool panic_delayed;
};

struct busmon_platdata {
	struct busmon_rpathinfo		*rpathinfo;
	struct busmon_masterinfo	*masterinfo;
	struct busmon_nodegroup		nodegroup[PATH_NUM];
};

static struct busmon_rpathinfo rpathinfo[] = {
	{0,	"G3D1",		"MEMS_0",	0x7F},
	{65,	"CAM0",		"MEMS_0",	0x7F},
	{81,	"CAM1",		"MEMS_0",	0x7F},
	{1,	"DISP0_0",	"MEMS_0",	0x7F},
	{17,	"DISP0_1",	"MEMS_0",	0x7F},
	{33,	"DISP1_0",	"MEMS_0",	0x7F},
	{49,	"DISP1_1",	"MEMS_0",	0x7F},
	{97,	"ISP0",		"MEMS_0",	0x7F},
	{66,	"CAM0",		"MEMS_0",	0x7F},
	{82,	"CAM1",		"MEMS_0",	0x7F},
	{2,	"DISP0_0",	"MEMS_0",	0x7F},
	{18,	"DISP0_1",	"MEMS_0",	0x7F},
	{34,	"DISP1_0",	"MEMS_0",	0x7F},
	{50,	"DISP1_1",	"MEMS_0",	0x7F},
	{98,	"ISP0",		"MEMS_0",	0x7F},
	{3,	"IMEM",		"MEMS_0",	0x7F},
	{35,	"AUD",		"MEMS_0",	0x7F},
	{67,	"CORESIGHT",	"MEMS_0",	0x7F},
	{11,	"CAM1",		"MEMS_0",	0x7F},
	{75,	"FSYS1",	"MEMS_0",	0x7F},
	{43,	"ISP0",		"MEMS_0",	0x7F},
	{19,	"CP",		"MEMS_0",	0x7F},
	{4,	"FSYS0",	"MEMS_0",	0x7F},
	{52,	"MFC0",		"MEMS_0",	0x7F},
	{68,	"MFC1",		"MEMS_0",	0x7F},
	{20,	"MSCL0",	"MEMS_0",	0x7F},
	{36,	"MSCL1",	"MEMS_0",	0x7F},

	{0,	"G3D1",		"MEMS_1",	0x7F},
	{65,	"CAM0",		"MEMS_1",	0x7F},
	{81,	"CAM1",		"MEMS_1",	0x7F},
	{1,	"DISP0_0",	"MEMS_1",	0x7F},
	{17,	"DISP0_1",	"MEMS_1",	0x7F},
	{33,	"DISP1_0",	"MEMS_1",	0x7F},
	{49,	"DISP1_1",	"MEMS_1",	0x7F},
	{97,	"ISP0",		"MEMS_1",	0x7F},
	{66,	"CAM0",		"MEMS_1",	0x7F},
	{82,	"CAM1",		"MEMS_1",	0x7F},
	{2,	"DISP0_0",	"MEMS_1",	0x7F},
	{18,	"DISP0_1",	"MEMS_1",	0x7F},
	{34,	"DISP1_0",	"MEMS_1",	0x7F},
	{50,	"DISP1_1",	"MEMS_1",	0x7F},
	{98,	"ISP0",		"MEMS_1",	0x7F},
	{3,	"IMEM",		"MEMS_1",	0x7F},
	{35,	"AUD",		"MEMS_1",	0x7F},
	{67,	"CORESIGHT",	"MEMS_1",	0x7F},
	{11,	"CAM1",		"MEMS_1",	0x7F},
	{75,	"FSYS1",	"MEMS_1",	0x7F},
	{43,	"ISP0",		"MEMS_1",	0x7F},
	{19,	"CP",		"MEMS_1",	0x7F},
	{4,	"FSYS0",	"MEMS_1",	0x7F},
	{52,	"MFC0",		"MEMS_1",	0x7F},
	{68,	"MFC1",		"MEMS_1",	0x7F},
	{20,	"MSCL0",	"MEMS_1",	0x7F},
	{36,	"MSCL1",	"MEMS_1",	0x7F},

	{0,	"IMEM",		"PERI",		0x1F},
	{8,	"AUD",		"PERI",		0x1F},
	{16,	"CORESIGHT",	"PERI",		0x1F},
	{1,	"FSYS0",	"PERI",		0x1F},
	{13,	"MFC0",		"PERI",		0x1F},
	{17,	"MFC1",		"PERI",		0x1F},
	{5,	"MSCL0",	"PERI",		0x1F},
	{9,	"MSCL1",	"PERI",		0x1F},
	{2,	"CAM1",		"PERI",		0x1F},
	{18,	"FSYS1",	"PERI",		0x1F},
	{10,	"ISP0",		"PERI",		0x1F},
};

static struct busmon_masterinfo masterinfo[] = {
	/* DISP0_0 */
	{"DISP0_0", 1 << 0, "sysmmu",	0x1},
	{"DISP0_0", 0 << 0, "S-IDMA0",	0x3},
	{"DISP0_0", 1 << 1, "IDMA3",	0x3},

	/* DISP0_1 */
	{"DISP0_1", 1 << 0, "sysmmu",	0x1},
	{"DISP0_1", 0 << 0, "IDMA0",	0x3},
	{"DISP0_1", 1 << 1, "IDMA4",	0x3},

	/* DISP1_0 */
	{"DISP1_0", 1 << 0, "sysmmu",	0x1},
	{"DISP1_0", 0 << 0, "IDMA1",	0x3},
	{"DISP1_0", 1 << 1, "VGR0",	0x3},

	/* DISP1_1 */
	{"DISP1_1", 1 << 0, "sysmmu",	0x1},
	{"DISP1_1", 0 << 0, "IDMA2",	0x7},
	{"DISP1_1", 1 << 1, "VGR1",	0x7},
	{"DISP1_1", 1 << 2, "WDMA",	0x7},

	/* MFC0 */
	{"MFC0", 1 << 0, "sysmmu",	0x1},
	{"MFC0", 0 << 0, "MFC M0",	0x1},

	/* MFC1 */
	{"MFC1", 1 << 0, "sysmmu",	0x1},
	{"MFC1", 0 << 0, "MFC M1",	0x1},

	/* IMEM */
	{"IMEM", 0 << 0, "SSS M0",	0xF},
	{"IMEM", 1 << 2, "RTIC",	0xF},
	{"IMEM", 1 << 3, "SSS M1",	0xF},
	{"IMEM", 1 << 0, "MCOMP",	0x3},
	{"IMEM", 1 << 1, "APM",		0x3},

	/* G3D */
	{"G3D0", 0 << 0, "G3D0",	0x1},
	{"G3D1", 0 << 1, "G3D1",	0x1},

	/* AUD */
	{"AUD", 1 << 0, "sysmmu",	0x1},
	{"AUD", 1 << 1, "DMAC",		0x7},
	{"AUD", 1 << 2, "AUD CA5",	0x7},

	/* MSCL0 */
	{"MSCL0", 1 << 0, "sysmmu",	0x1},
	{"MSCL0", 0 << 0, "JPEG",	0x3},
	{"MSCL0", 1 << 1, "MSCL0",	0x3},

	/* MSCL1 */
	{"MSCL1", 1 << 0, "sysmmu",	0x1},
	{"MSCL1", 0 << 0, "G2D",	0x3},
	{"MSCL1", 1 << 1, "MSCL1",	0x3},

	/* FSYS1 */
	{"FSYS1", 0 << 0, "MMC51",	0x7},
	{"FSYS1", 1 << 2, "UFS",	0x7},
	{"FSYS1", 1 << 0, "PCIE_WIFI0",	0x3},
	{"FSYS1", 1 << 1, "PCIE_WIFI1",	0x3},

	/* FSYS0 */
	{"FSYS0", 0 << 0, "ETR USB",			0x7},
	{"FSYS0", 1 << 2, "USB30",			0x7},
	{"FSYS0", 1 << 0, "UFS",			0x7},
	{"FSYS0", 1 << 0 | 1 << 2, "MMC51",		0x7},
	{"FSYS0", 1 << 1, "PDMA0",			0x7},
	{"FSYS0", 1 << 1 | 1 << 2, "PDMA(secure)",	0x7},
	{"FSYS0", 1 << 0 | 1 << 1, "USB20",		0x3},

	/* CAM0 */
	{"CAM0", 1 << 0, "sysmmu",			0x1},
	{"CAM0", 0 << 0, "MIPI_CSIS0",			0x7},
	{"CAM0", 1 << 1, "MIPI_CSIS1",			0x7},
	{"CAM0", 1 << 2, "FIMC_3AA0",			0x7},
	{"CAM0", 1 << 1 | 1 << 2, "FIMC_3AA1",		0x7},

	/* CAM1 */
	{"CAM1", 1 << 2, "sysmmu_IS_B",			0x7},
	{"CAM1", 0 << 0, "MIPI_CSI2",			0x1F},
	{"CAM1", 1 << 3, "ISP1",			0x1F},
	{"CAM1", 1 << 4, "MIPI_CSI3",			0x1F},
	{"CAM1", 1 << 0 | 1 << 2, "sysmmu_SCL",		0x7},
	{"CAM1", 1 << 0, "MC_SCALER",			0x7},
	{"CAM1", 1 << 0 | 1 << 1 | 1 << 2, "sysmmu_VRA", 0x7},
	{"CAM1", 1 << 0 | 1 << 1, "FIMC_VRA",		0x7},
	{"CAM1", 1 << 1 | 1 << 2, "sysmmu_CA7",		0x7},
	{"CAM1", 1 << 1, "CA7",				0xF},
	{"CAM1", 1 << 1 | 1 << 3, "PDMA_IS",		0xF},

	/* ISP0 */
	{"ISP0", 1 << 0, "sysmmu",			0x1},
	{"ISP0", 0 << 0, "FIMC_ISP",			0x3},
	{"ISP0", 1 << 1, "FIMC_TPU",			0x3},

	/* CP */
	{"CP", 0 << 0, "CR7M",				0x18},
	{"CP", 1 << 3, "TL3MtoL2",			0x18},
	{"CP", 1 << 4, "DMAC",				0x1C},
	{"CP", 1 << 2 | 1 << 4, "MEMtoL2",		0x1C},
	{"CP", 1 << 3 | 1 << 4, "CSXAP",		0x1F},
	{"CP", 1 << 0 | 1 << 3 | 1 << 4, "LMAC",	0x1F},
	{"CP", 1 << 1 | 1 << 3 | 1 << 4, "HMtoL2",	0x1F},
};

static struct busmon_masterinfo masterinfo_sfr[] = {
	{"CPU",		0, "Access from either CPU clusters",		0},
	{"TREX_CCORE",	0, "The PERI S-node from the data backbone",	0},
	{"G3D",		0, "SFR accesses from G3D",			0},
	{"CP",		0, "SFR accesses from CP",			0},
	{"CPU",		0, "IMEM access only",				0},
	{"CPU",		0, "IMEM access only",				0},
	{"Unknown",	0, "Unknown",					0},
	{"Unknown",	0, "Unknown",					0},
};

static struct busmon_nodeinfo ccore_datapath[] = {
	/* Data Path - CCORE */
	{M_NODE, "CCORE_G3D0_M_NODE",		0x10683000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master(G3D0 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "CCORE_G3D1_M_NODE",		0x10693000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master(G3D1 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "CCORE_IMEM_M_NODE",		0x106B3000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master(IMEM Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "CCORE_AUD_M_NODE",		0x106C3000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master(AUD Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "CCORE_CORESIGHT_M_NODE",	0x106D3000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master(Coresight) -> Slave(MEM or PERI) ]"},
	{M_NODE, "CCORE_CP_M_NODE",		0x10733000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master(CP) -> Slave(MEM or PERI) ]"},
	{T_M_NODE, "CCORE_BUS0_0_M_NODE",	0x10603000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master -> CCORE_BUS0_0 -> Slave(MEM or PERI) ]"},
	{T_M_NODE, "CCORE_BUS0_1_M_NODE",	0x10613000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master -> CCORE_BUS0_1 -> Slave(MEM or PERI) ]"},
	{T_M_NODE, "CCORE_BUS1_0_M_NODE",	0x10623000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master -> CCORE_BUS1_0 -> Slave(MEM or PERI) ]"},
	{T_M_NODE, "CCORE_BUS1_1_M_NODE",	0x10633000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master -> CCORE_BUS1_1 -> Slave(MEM or PERI) ]"},
	{T_M_NODE, "CCORE_BUS0_R0_T_M_NODE",	0x10643000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master -> CCORE_BUS0_R0 -> Slave(MEM or PERI) ]"},
	{T_M_NODE, "CCORE_BUS0_R1_T_M_NODE",	0x10653000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master -> CCORE_BUS0_R1 -> Slave(MEM or PERI) ]"},
	{T_M_NODE, "CCORE_BUS0_R2_T_M_NODE",	0x10663000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master -> CCORE_BUS0_R2 -> Slave(MEM or PERI) ]"},
	{T_M_NODE, "CCORE_BUS0_R3_T_M_NODE",	0x10673000, NULL, 322, 0, false, true, NULL, "DATA Path [ Master -> CCORE_BUS0_R3 -> Slave(MEM or PERI) ]"},
	{S_NODE, "CCORE_MEMS_0_S_NODE",		0x10703000, NULL, 322, TIMEOUT, true, true, "MEMS_0", "DATA Path [ Master(Refer Route Information for DATA) -> Slave(MEM0) ]"},
	{S_NODE, "CCORE_MEMS_1_S_NODE",		0x10713000, NULL, 322, TIMEOUT, true, true, "MEMS_1", "DATA Path [ Master(Refer Route Information for DATA) -> Slave(MEM1) ]"},
	{S_NODE, "CCORE_PERI_S_NODE",		0x10723000, NULL, 322, TIMEOUT, true, true, "PERI", "DATA Path [ Master(Refer Route Information for DATA) -> Slave(PERI) ]"},
};

static struct busmon_nodeinfo bus0_datapath[] = {
	/* Data Path BUS0 */
	{M_NODE, "BUS0_FSYS1_M_NODE",		0x11F03000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master(FSYS1 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS0_CAM1_M_NODE",		0X11F13000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master(CAM1 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS0_ISP0_M_NODE",		0X11F23000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master(ISP0 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS0_DISP0_0_M_NODE",		0X11F33000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master(DISP0_0 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS0_DISP0_1_M_NODE",		0X11F43000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master(DISP0_1 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS0_DISP1_0_M_NODE",		0X11F53000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master(DISP1_0 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS0_DISP1_1_M_NODE",		0X11F63000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master(DISP1_1 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS0_CAM0_M_NODE",		0X11F73000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master(CAM0 Block) -> Slave(MEM or PERI) ]"},
	{T_S_NODE, "BUS0_0_T_S_NODE",		0X11F83000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master -> BUS0 -> Slave(MEM or PERI) ]"},
	{T_S_NODE, "BUS0_1_T_S_NODE",		0X11F93000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master -> BUS0 -> Slave(MEM or PERI) ]"},
	{T_S_NODE, "BUS0_R0_T_S_NODE",		0X11FA3000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master -> BUS0 -> Slave(MEM0) ]"},
	{T_S_NODE, "BUS0_R1_T_S_NODE",		0X11FB3000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master -> BUS0 -> Slave(MEM1) ]"},
	{T_S_NODE, "BUS0_R2_T_S_NODE",		0X11FC3000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master -> BUS0 -> Slave(MEM2) ]"},
	{T_S_NODE, "BUS0_R3_T_S_NODE",		0X11FD3000, NULL, 344, 0, false, true, NULL, "DATA Path [ Master -> BUS0 -> Slave(MEM3) ]"},
};

static struct busmon_nodeinfo bus1_datapath[] = {
	/* Data Path - BUS1 */
	{M_NODE, "BUS1_FSYS0_M_NODE",		0x11D03000, NULL, 368, 0, false, true, NULL, "DATA Path [ Master(FSYS0 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS1_MSCL0_M_NODE",		0x11D13000, NULL, 368, 0, false, true, NULL, "DATA Path [ Master(MSCL0 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS1_MSCL1_M_NODE",		0x11D23000, NULL, 368, 0, false, true, NULL, "DATA Path [ Master(MSCL1 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS1_MFC0_M_NODE",		0x11D33000, NULL, 368, 0, false, true, NULL, "DATA Path [ Master(MFC01 Block) -> Slave(MEM or PERI) ]"},
	{M_NODE, "BUS1_MFC1_M_NODE",		0x11D43000, NULL, 368, 0, false, true, NULL, "DATA Path [ Master(MFC1 Block) -> Slave(MEM or PERI) ]"},
	{T_S_NODE, "BUS1_0_T_S_NODE",		0x11D53000, NULL, 368, 0, false, true, NULL, "DATA Path [ Master -> BUS1 -> Slave) ]"},
	{T_S_NODE, "BUS1_1_T_S_NODE",		0x11D63000, NULL, 368, 0, false, true, NULL, "DATA Path [ Master -> BUS1 -> Slave) ]"},
};

static struct busmon_nodeinfo ccore_sfrpath[] = {
	/* SFR Path CCORE */
	{M_NODE, "P_CCORE_BUS_M_NODE",		0x104E3000, NULL, 323, 0, false, true, NULL, "SFR Path - Connected with CCORE"},
	{S_NODE, "P_CCORE_APL_S_NODE",		0x10443000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(APL(Apollo) Block) ]"},
	{S_NODE, "P_CCORE_AUD_S_NODE",		0x10493000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(AUD(Audio) Block) ]"},
	{S_NODE, "P_CCORE_CCORE_SFR_S_NODE",	0x104B3000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(CCORE Block) ]"},
	{S_NODE, "P_CCORE_CORESIGHT_S_NODE",	0x10423000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(Coresight Access) ]"},
	{S_NODE, "P_CCORE_G3D_S_NODE",		0x104A3000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(G3D block) ]"},
	{S_NODE, "P_CCORE_MIF0_S_NODE",		0x10453000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(MIF0 Block) ]"},
	{S_NODE, "P_CCORE_MIF1_S_NODE",		0x10463000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(MIF1 Block) ]"},
	{S_NODE, "P_CCORE_MIF2_S_NODE",		0x10473000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(MIF2 Block) ]"},
	{S_NODE, "P_CCORE_MIF3_S_NODE",		0x10483000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(MIF3 Block) ]"},
	{S_NODE, "P_CCORE_MNGS_NODE",		0x10433000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(MNG(mongoose) Block) ]"},
	{S_NODE, "P_CCORE_TREX_MIF_S_NODE",	0x104D3000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(TREX_MIF Block) ]"},
	{S_NODE, "P_CCORE_TREX_MIF_PERI_S_NODE",0x104C3000, NULL, 323, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(TREX_MIF_PERI Block) ]"},
	{T_S_NODE, "P_CCORE_BUS1_T_S_NODE",	0x10403000, NULL, 323, 0, false, true, NULL, "SFR Path [ (Master(Refer Route Information for SFR) -> BUS1 Block) ]"},
	{T_S_NODE, "P_CCORE_BUS0_T_S_NODE",	0x10413000, NULL, 323, 0, false, true, NULL, "SFR Path [ (Master(Refer Route Information for SFR) -> BUS0 Block) ]"},
};

static struct busmon_nodeinfo bus1_sfrpath[] = {
	/* Path BUS1 */
	{T_M_NODE, "P_BUS1_CCORE_BUS1_T_M_NODE",0x11CE3000, NULL, 373, 0, false, true, NULL, "SFR Path - Connected with BUS1"},
	{S_NODE, "P_BUS1_BUS1_SFR_S_NODE",	0x11C43000, NULL, 373, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(BUS1 Block) ]"},
	{S_NODE, "P_BUS1_FSYS0_S_NODE",		0x11C03000, NULL, 373, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(FSYS0 Block) ]"},
	{S_NODE, "P_BUS1_MFC_S_NODE",		0x11C23000, NULL, 373, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(MFC Block) ]"},
	{S_NODE, "P_BUS1_MSCL_S_NODE",		0x11C13000, NULL, 373, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(MSCL Block) ]"},
	{S_NODE, "P_BUS1_TREX_BUS1_S_NODE",	0x11C63000, NULL, 373, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(TREX_BUS1 Block) ]"},
	{S_NODE, "P_BUS1_TREX_BUS1_PERI_S_NODE",0x11C53000, NULL, 373, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(TREX_BUS1_PERI Block) ]"},
};

static struct busmon_nodeinfo bus0_sfrpath[] = {
	/* Path BUS0 */
	{T_M_NODE, "P_BUS0_CCORE_BUS0_T_M_NODE",0x11EE3000, NULL, 352, 0, false, true, NULL, "SFR Path - Connected with BUS0"},
	{S_NODE, "P_BUS0_BUS0_SFR_S_NODE",	0x11E73000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(BUS0 Block) ]"},
	{S_NODE, "P_BUS0_CAM0_S_NODE",		0x11E63000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(CAM0 Block) ]"},
	{S_NODE, "P_BUS0_CAM1_S_NODE",		0x11E53000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(CAM1 Block) ]"},
	{S_NODE, "P_BUS0_DISP_S_NODE",		0x11E33000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(DISP Block) ]"},
	{S_NODE, "P_BUS0_FSYS1_S_NODE",		0x11E13000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(FSYS1 Block) ]"},
	{S_NODE, "P_BUS0_PERIC0_S_NODE",	0x11E23000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(PERIC0 Block) ]"},
	{S_NODE, "P_BUS0_PERIC1_S_NODE",	0x11EA3000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(PERIC1 Block) ]"},
	{S_NODE, "P_BUS0_PERIS_S_NODE",		0x11E03000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(PERIS Block) ]"},
	{S_NODE, "P_BUS0_TREX_BUS0_S_NODE",	0x11E93000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(TREX_BUS0 Block) ]"},
	{S_NODE, "P_BUS0_TREX_BUS0_PERI_S_NODE",0x11E83000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(TREX BUS0 PERI) ]"},
	{S_NODE, "P_BUS0_VPP_S_NODE",		0x11E43000, NULL, 352, TIMEOUT, true, true, NULL, "SFR Path [ Master(Refer Route Information for SFR) -> Slave(VPP Block) ]"},
};

struct busmon_dev {
	struct device			*dev;
	struct busmon_platdata		*pdata;
	struct of_device_id		*match;
	int				irq;
	int				id;
	void __iomem			*regs;
	spinlock_t			ctrl_lock;
	struct busmon_notifier		notifier_info;
};

struct busmon_panic_block {
	struct notifier_block nb_panic_block;
	struct busmon_dev *pdev;
};

/* declare notifier_list */
static ATOMIC_NOTIFIER_HEAD(busmon_notifier_list);

static const struct of_device_id busmon_dt_match[] = {
	{ .compatible = "samsung,exynos-busmonitor",
	  .data = NULL, },
	{},
};
MODULE_DEVICE_TABLE(of, busmon_dt_match);

static struct busmon_rpathinfo* busmon_get_rpathinfo
					(struct busmon_dev *busmon,
					 unsigned int id,
					 char *dest_name)
{
	struct busmon_platdata *pdata = busmon->pdata;
	struct busmon_rpathinfo *rpath = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(rpathinfo); i++) {
		if (pdata->rpathinfo[i].id == (id & pdata->rpathinfo[i].bits)) {
			if (dest_name && !strncmp(pdata->rpathinfo[i].dest_name,
					dest_name, strlen(pdata->rpathinfo[i].dest_name))) {
				rpath = &pdata->rpathinfo[i];
				break;
			}
		}
	}
	return rpath;
}

static struct busmon_masterinfo* busmon_get_masterinfo
					(struct busmon_dev *busmon,
					 char *port_name,
					 unsigned int user)
{
	struct busmon_platdata *pdata = busmon->pdata;
	struct busmon_masterinfo *master = NULL;
	unsigned int val;
	int i;

	for (i = 0; i < ARRAY_SIZE(masterinfo); i++) {
		if (!strncmp(pdata->masterinfo[i].port_name, port_name, strlen(port_name))) {
			val = user & pdata->masterinfo[i].bits;
			if (val == pdata->masterinfo[i].user) {
				master = &pdata->masterinfo[i];
				break;
			}
		}
	}
	return master;
}

static void busmon_init(struct busmon_dev *busmon, bool enabled)
{
	struct busmon_platdata *pdata = busmon->pdata;
	struct busmon_nodeinfo *node;
	unsigned int offset;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(pdata->nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			if (node[j].type == S_NODE && node[j].timeout_enabled) {
				offset = OFFSET_TIMEOUT_REG;
				/* Enable Timeout setting */
				__raw_writel(enabled, node[j].regs + offset + REG_DBG_CTL);
				/* set timeout interval value */
				__raw_writel(node[j].time_val,
					node[j].regs + offset + REG_TIMEOUT_INIT_VAL);
				pr_debug("Exynos BUS Monitor irq:%u - %s timeout %sabled\n",
					node[j].irq, node[j].name, enabled ? "en" : "dis");
			}
			if (node[j].err_rpt_enabled) {
				/* clear previous interrupt of req_read */
				offset = OFFSET_REQ_R;
				__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of req_write */
				offset = OFFSET_REQ_W;
				__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of response_read */
				offset = OFFSET_RESP_R;
				__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of response_write */
				offset = OFFSET_RESP_W;
				__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);
				pr_debug("Exynos BUS Monitor irq:%u - %s error reporting %sabled\n",
					node[j].irq, node[j].name, enabled ? "en" : "dis");
			}
		}
	}
}

static void busmon_post_handler_by_master(struct busmon_dev *busmon,
					  struct busmon_nodegroup *group,
					  char *port, char *master, bool read)
{
	/* After treatment by master */
	if (!port || strlen(port) < 1)
		return;

	if (!strncmp(port, "CP", strlen(port))) {
		/* if master is DSP and operation is read, we don't care this */
		if (master && !strncmp(master, "TL3MtoL2", strlen(master)) && read == true) {
			group->panic_delayed = true;
			group->irq_occurred = 0;
			pr_info("busmon skips CP's DSP(TL3MtoL2) detected\n");
		} else {
			/* Disable busmon all interrupts */
			busmon_init(busmon, false);
			/* This call is for Exynos8890 only */
			group->panic_delayed = true;
			ss310ap_force_crash_exit_ext();
		}
	} else if (!strncmp(port, "CPU", strlen(port))) {
		pr_info("busmon disabled for CPU exception\n");
		/* Disable busmon all interrupts */
		busmon_init(busmon, false);
		group->panic_delayed = true;
	}
}

static void busmon_report_route(struct busmon_dev *busmon,
			        struct busmon_nodegroup *group,
				struct busmon_nodeinfo *node,
				unsigned int offset)
{
	struct busmon_masterinfo *master = NULL;
	struct busmon_rpathinfo *rpath = NULL;
	unsigned int val, id, user;

	val = __raw_readl(node->regs + offset + REG_INT_INFO);
	id = BIT_ID_VAL(val);

	rpath = busmon_get_rpathinfo(busmon, id, node->need_rpath);
	if (!rpath) {
		pr_info("failed to get route path - %s, id:%x\n",
					node->need_rpath, id);
	} else {
		val = __raw_readl(node->regs + offset + REG_EXT_INFO_2);
		user = BIT_AXUSER(val);

		master = busmon_get_masterinfo(busmon, rpath->port_name, user);
		if (!master) {
			pr_info("failed to get master IP with "
				"port:%s, id:%x, user:%x\n",
				rpath->port_name, id, user);
		} else {

			pr_auto(ASL3, "\n--------------------------------------------------------------------------------\n"
				"Route Information for DATA transaction\n\n"
				"Master IP:%s's %s ---> Target:%s\n",
				master->port_name, master->master_name, rpath->dest_name);

			busmon_post_handler_by_master(busmon, group,
							master->port_name,
							master->master_name,
							(offset == OFFSET_RESP_R) ? true : false);
		}
	}
}

static void busmon_report_info(struct busmon_dev *busmon,
			       struct busmon_nodegroup *group,
			       struct busmon_nodeinfo *node,
			       unsigned int offset)
{
	unsigned int int_info, errcode, info0, info1, info2;
	bool read = false, req = false;
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	char temp_buf[SZ_128];
#endif

	int_info = __raw_readl(node->regs + offset + REG_INT_INFO);
	if (!BIT_ERR_VALID(int_info)) {
		pr_info("no information, %s/offset:%x is stopover, "
			"check other node\n", node->name, offset);
		return;
	}

	errcode = BIT_ERR_CODE(int_info);
	info0 = __raw_readl(node->regs + offset + REG_EXT_INFO_0);
	info1 = __raw_readl(node->regs + offset + REG_EXT_INFO_1);
	info2 = __raw_readl(node->regs + offset + REG_EXT_INFO_2);

	switch(offset) {
	case OFFSET_REQ_R:
		read = true;
		/* fall down */
	case OFFSET_REQ_W:
		req = true;
		if (node->type == S_NODE) {
			/* Only S-Node is able to make log to registers */
			pr_info("invalid logged, see more following information\n");
			break;
		} else {
			if (!strncmp(node->name, "P_CCORE_BUS_M_NODE", strlen(node->name))) {
				/* SFR Path */
				unsigned int master;
				master = BIT_ID_VAL(int_info) & 0x7;

				pr_auto(ASL3, "\n--------------------------------------------------------------------------------\n"
					"Route Information for SFR transaction\n\n"
					"Master IP		 : %s - %s\n"
					"AxID			 : 0x%X\n"
					"--------------------------------------------------------------------------------\n",
					masterinfo_sfr[master].port_name,
					masterinfo_sfr[master].master_name,
					BIT_ID_VAL(int_info));

				busmon_post_handler_by_master(busmon, group,
								masterinfo_sfr[master].port_name,
								masterinfo_sfr[master].master_name, read);
			} else if (!strncmp(node->name, "CCORE_CP_M_NODE", strlen(node->name))){
				struct busmon_masterinfo* master =
					busmon_get_masterinfo(busmon, "CP", BIT_AXUSER(info2));
				if (master)
					busmon_post_handler_by_master(busmon, group,
									master->port_name,
									master->master_name, read);
				else
					pr_info("failed to get CP's master information\n");

			}
		}
		break;
	case OFFSET_RESP_R:
		read = true;
		/* fall down */
	case OFFSET_RESP_W:
		req = false;
		if (node->type != S_NODE) {
			/* Only S-Node is able to make log to registers */
			pr_info("invalid logged, see more following information\n");
			break;
		} else {
			/* 0x6 means timeout */
			if (errcode == 0x6) {
				unsigned int mo_offset = read ? REG_R_TIMEOUT_MO : REG_W_TIMEOUT_MO;
				pr_info("\n--------------------------------------------------------------------------------\n"
					"Additional information for timeout error\n\n"
					"Timeout MO val  : 0x%08X\n",
					__raw_readl(node->regs + mo_offset + OFFSET_TIMEOUT_REG));
			}
		}
		if (node->need_rpath) {
			/* Data Path */
			busmon_report_route(busmon, group, node, offset);
		}
		break;
	default:
		pr_info("Unknown Error - offset:%u\n", offset);
		break;
	}

	pr_auto(ASL3, "\n--------------------------------------------------------------------------------\n"
		"Transaction information => [%s, %s] Fail to access\n\n"
		"Detect reason   : %s\n"
		"Target address  : 0x%llX\n"
		"Error type      : %s\n"
		"Group           : %s\n",
		read ? "READ" : "WRITE",
		req ? "REQUEST" : "RESPONSE",
		node->comment,
		(unsigned long long)((info1 & (GENMASK(3, 0)) << 32) | info0),
		busmon_errcode[errcode],
		group->name);

	pr_auto(ASL3, "\n--------------------------------------------------------------------------------\n"
		"Detail NODE information for debuggging\n\n"
		"Node Name		 : %s(0x%08X)\n"
		"INTERRUPT_INFO  : 0x%08X\n"
		"EXT_INFO_0 	 : 0x%08X\n"
		"EXT_INFO_1 	 : 0x%08X\n"
		"EXT_INFO_2 	 : 0x%08X\n"
		"--------------------------------------------------------------------------------\n",
		node->name, node->phy_regs + offset,
		int_info, info0, info1, info2);

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	snprintf(temp_buf, SZ_128, "%s (0x%08X) / 0x%08X / Target : 0x%08X / 0x%08X / 0x%08X",
		node->name, node->phy_regs + offset,
		int_info, info0, info1, info2);
	sec_debug_set_extra_info_busmon(temp_buf);
#endif

}

static int busmon_parse_info(struct busmon_dev *busmon,
			      struct busmon_nodegroup *group,
			      bool clear)
{
	struct busmon_platdata *pdata = busmon->pdata;
	struct busmon_nodeinfo *node = NULL;
	unsigned int val, offset;
	unsigned long flags;
	int ret = 0;
	int i, j, k;

	spin_lock_irqsave(&busmon->ctrl_lock, flags);
	if (group) {
		/* Processing only this group */
		node = group->nodeinfo;
		for (i = 0; i < group->nodesize; i++) {
			for (j = 0; j < 4; j++) {
				offset = j * OFFSET_ERR_REPT;
				/* Check Request information */
				val = __raw_readl(node[i].regs + offset + REG_INT_INFO);
				if (BIT_ERR_OCCURRED(val)) {
					/* This node occurs the error */
					busmon_report_info(busmon, group, &node[i], offset);
					if (clear)
						__raw_writel(1,
							node[i].regs + offset + REG_INT_CLR);
					ret = 1;
				}
			}
		}
	} else {
		/* Processing all group & nodes */
		for (i = 0; i < ARRAY_SIZE(pdata->nodegroup); i++) {
			group = &pdata->nodegroup[i];
			node = group->nodeinfo;
			for (j = 0; j < group->nodesize; j++) {
				for (k = 0; k < 4; k++) {
					offset = k * OFFSET_ERR_REPT;
					/* Check Request information */
					val = __raw_readl(node[j].regs + offset + REG_INT_INFO);
					if (BIT_ERR_OCCURRED(val)) {
						/* This node occurs the error */
						busmon_report_info(busmon, group, &node[j], offset);
						if (clear)
							__raw_writel(1,
								node[j].regs + offset + REG_INT_CLR);
						ret = 1;
					}
				}
			}
		}
	}
	spin_unlock_irqrestore(&busmon->ctrl_lock, flags);
	return ret;
}

static bool busmon_panic = false;
bool busmon_get_panic(void)
{
       return busmon_panic;
}

static irqreturn_t busmon_irq_handler(int irq, void *data)
{
	struct busmon_dev *busmon = (struct busmon_dev *)data;
	struct busmon_platdata *pdata = busmon->pdata;
	struct busmon_nodegroup *group = NULL;
	int i, ret;

	/* Check error has been logged */
	pr_info("%d interrupt detected\n", (irq - 32));

	/* Search busmon group */
	for (i = 0; i < ARRAY_SIZE(pdata->nodegroup); i++) {
		if ((irq - 32) == pdata->nodegroup[i].irq) {
			pr_info("%s group, irq %d occurrs \n",
				pdata->nodegroup[i].name, irq - 32);
			group = &pdata->nodegroup[i];
			break;
		}
	}

	if (group) {
		ret = busmon_parse_info(busmon, group, true);
		if (!ret) {
			pr_info("%s can't find the error - irq %d\n",
				group->name, irq - 32);
		} else {
			if (group->irq_occurred && !group->panic_delayed) {
				busmon_panic = true;
				panic("STOP infinite output by abnormal access");
			} else {
				group->irq_occurred++;
			}
		}
	} else {
		pr_info("can't find irq %d\n", irq - 32);
	}
#if 0
	/* Disable to call notifier_call_chain of busmon in Exynos8890 EVT0 */
	atomic_notifier_call_chain(&busmon_notifier_list, 0, &busmon->notifier_info);
	panic("Error detected by BUS monitor.");
#endif
	return IRQ_HANDLED;
}

void busmon_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&busmon_notifier_list, block);
}

static int busmon_logging_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct busmon_panic_block *busmon_panic = (struct busmon_panic_block *)nb;
	struct busmon_dev *busmon = busmon_panic->pdev;
	int ret;

	if (!IS_ERR_OR_NULL(busmon)) {
		/* Check error has been logged */
		ret = busmon_parse_info(busmon, NULL, false);
		if (!ret)
			pr_info("No found error in %s\n", __func__);
		else
			pr_info("Found errors in %s\n", __func__);
	}
	return 0;
}

static int busmon_probe(struct platform_device *pdev)
{
	struct busmon_dev *busmon;
	struct busmon_panic_block *busmon_panic = NULL;
	struct busmon_platdata *pdata;
	struct busmon_nodeinfo *node;
	char *dev_name;
	int ret, i, j;

	busmon = devm_kzalloc(&pdev->dev, sizeof(struct busmon_dev), GFP_KERNEL);
	if (!busmon) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"private data\n");
		return -ENOMEM;
	}
	busmon->dev = &pdev->dev;

	spin_lock_init(&busmon->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct busmon_platdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"platform data\n");
		return -ENOMEM;
	}
	busmon->pdata = pdata;
	busmon->pdata->masterinfo = masterinfo;
	busmon->pdata->rpathinfo = rpathinfo;

	for (i = 0; i < ARRAY_SIZE(pdata->nodegroup); i++)
	{
		switch (i) {
		case PATH_DATA_CCORE:
			node = ccore_datapath;
			dev_name = "DATA_PATH_CCORE";
			pdata->nodegroup[i].nodesize = ARRAY_SIZE(ccore_datapath);
			break;
		case PATH_DATA_BUS0:
			node = bus0_datapath;
			dev_name = "DATA_PATH_BUS0";
			pdata->nodegroup[i].nodesize = ARRAY_SIZE(bus0_datapath);
			break;
		case PATH_DATA_BUS1:
			node = bus1_datapath;
			dev_name = "DATA_PATH_BUS1";
			pdata->nodegroup[i].nodesize = ARRAY_SIZE(bus1_datapath);
			break;
		case PATH_SFR_CCORE:
			node = ccore_sfrpath;
			dev_name = "SFR_PATH_CCORE";
			pdata->nodegroup[i].nodesize = ARRAY_SIZE(ccore_sfrpath);
			break;
		case PATH_SFR_BUS0:
			node = bus0_sfrpath;
			dev_name = "SFR_PATH_BUS0";
			pdata->nodegroup[i].nodesize = ARRAY_SIZE(bus0_sfrpath);
			break;
		case PATH_SFR_BUS1:
			node = bus1_sfrpath;
			dev_name = "SFR_PATH_BUS1";
			pdata->nodegroup[i].nodesize = ARRAY_SIZE(bus1_sfrpath);
			break;
		default:
			break;
		};

		pdata->nodegroup[i].nodeinfo = node;
		pdata->nodegroup[i].irq = node[0].irq;
		pdata->nodegroup[i].name = dev_name;
		pdata->nodegroup[i].irq_occurred = 0;
		pdata->nodegroup[i].panic_delayed = false;

		ret = devm_request_irq(&pdev->dev, pdata->nodegroup[i].irq + 32,
					busmon_irq_handler, 0, //IRQF_GIC_MULTI_TARGET,
					dev_name, busmon);

		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			node[j].regs = devm_ioremap_nocache(&pdev->dev, node[j].phy_regs, SZ_16K);
			if (node[j].regs == NULL) {
				dev_err(&pdev->dev, "failed to claim register region - %s\n", dev_name);
				return -ENOENT;
			}
		}
	}

	busmon_panic = devm_kzalloc(&pdev->dev,
			sizeof(struct busmon_panic_block), GFP_KERNEL);

	if (!busmon_panic) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"panic handler data\n");
	} else {
		busmon_panic->nb_panic_block.notifier_call =
					busmon_logging_panic_handler;
		busmon_panic->pdev = busmon;
		atomic_notifier_chain_register(&panic_notifier_list,
					&busmon_panic->nb_panic_block);
	}

	platform_set_drvdata(pdev, busmon);

	busmon_init(busmon, true);
	dev_info(&pdev->dev, "success to probe bus monitor driver\n");

	return 0;
}

static int busmon_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int busmon_suspend(struct device *dev)
{
	return 0;
}

static int busmon_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct busmon_dev *busmon = platform_get_drvdata(pdev);

	busmon_init(busmon, true);

	return 0;
}

static SIMPLE_DEV_PM_OPS(busmon_pm_ops,
			 busmon_suspend,
			 busmon_resume);
#define BUSMON_PM	(busmon_pm_ops)
#else
#define BUSMON_PM	NULL
#endif

static struct platform_driver exynos_busmon_driver = {
	.probe		= busmon_probe,
	.remove		= busmon_remove,
	.driver		= {
		.name		= "exynos-busmon",
		.of_match_table	= busmon_dt_match,
		.pm		= &busmon_pm_ops,
	},
};

module_platform_driver(exynos_busmon_driver);

MODULE_DESCRIPTION("Samsung Exynos8890 BUS MONITOR DRIVER");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-busmon");
