/*
 *  Copyright (c) 2012 Samsung Electronics.
 *
 * EXYNOS - SMC Call
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SMC_H__
#define __SMC_H__

/* For Power Management */
#define SMC_CMD_SLEEP			(-3)
#define SMC_CMD_CPU1BOOT		(-4)
#define SMC_CMD_CPU0AFTR		(-5)
#define SMC_CMD_SAVE			(-6)
#define SMC_CMD_SHUTDOWN		(-7)

/* For Accessing CP15/SFR (General) */
#define SMC_CMD_REG			(-101)

/* For setting memory for debug */
#define SMC_CMD_SET_DEBUG_MEM		(-120)
#define SMC_CMD_GET_LOCKUP_REASON	(-121)
#define SMC_CMD_KERNEL_PANIC_NOTICE	(-122)

/* Command ID for smc */
#define SMC_PROTECTION_SET		(0x82002010)
#define SMC_DRM_FW_LOADING		(0x82002011)
#define SMC_DCPP_SUPPORT		(0x82002012)
#define SMC_DRM_SECBUF_PROT		(0x82002020)
#define SMC_DRM_SECBUF_UNPROT		(0x82002021)
#define SMC_DRM_SECBUF_CFW_PROT		(0x82002030)
#define SMC_DRM_SECBUF_CFW_UNPROT	(0x82002031)
#define MC_FC_SET_CFW_PROT		(0x82002040)
#define MC_FC_DRM_SET_CFW_PROT		(0x10000000)
#define SMC_SRPMB_WSM			(0x82003811)

/* Deprecated */
#define SMC_DRM_MAKE_PGTABLE		(0x81000003)
#define SMC_DRM_CLEAR_PGTABLE		(0x81000004)
#define SMC_MEM_PROT_SET		(0x81000005)
#define SMC_DRM_SECMEM_INFO		(0x81000006)
#define SMC_DRM_VIDEO_PROC		(0x81000007)

/* Parameter for smc */
#define SMC_PROTECTION_ENABLE		(1)
#define SMC_PROTECTION_DISABLE		(0)

/* For FMP Ctrl */
#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS8890)
#define SMC_CMD_FMP			(0xC2001810)
#define SMC_CMD_SMU			(0xC2001820)
#define SMC_CMD_RESUME			(0xC2001830)
#else
#define SMC_CMD_FMP			(0x81000020)
#endif

/* For DTRNG Access */
#ifdef CONFIG_EXYRNG_USE_CRYPTOMANAGER
#define SMC_CMD_RANDOM			(0x82001012)
#else
#define SMC_CMD_RANDOM			(0x81000030)
#endif

/* MACRO for SMC_CMD_REG */
#define SMC_REG_CLASS_CP15		(0x0 << 30)
#define SMC_REG_CLASS_SFR_W		(0x1 << 30)
#define SMC_REG_CLASS_SFR_R		(0x3 << 30)
#define SMC_REG_CLASS_MASK		(0x3 << 30)
#define SMC_REG_ID_SFR_W(ADDR)		(SMC_REG_CLASS_SFR_W | ((ADDR) >> 2))
#define SMC_REG_ID_SFR_R(ADDR)		(SMC_REG_CLASS_SFR_R | ((ADDR) >> 2))

/* op type for SMC_CMD_SAVE and SMC_CMD_SHUTDOWN */
#define OP_TYPE_CORE			(0x0)
#define OP_TYPE_CLUSTER			(0x1)

/* Power State required for SMC_CMD_SAVE and SMC_CMD_SHUTDOWN */
#define SMC_POWERSTATE_SLEEP		(0x0)
#define SMC_POWERSTATE_IDLE		(0x1)
#define SMC_POWERSTATE_SWITCH		(0x2)

/*
 * For SMC_CMD_FMP
 * Defined from Exynos7420
 */
#define FMP_KEY_STORE			(0x0)
#define FMP_KEY_SET			(0x1)
#define FMP_KEY_RESUME			(0x2)
#define FMP_FW_INTEGRITY		(0x3)
#define FMP_FW_SELFTEST			(0x4)
#define FMP_FW_SHA2_TEST		(0x5)
#define FMP_FW_HMAC_SHA2_TEST		(0x6)
#define FMP_KEY_CLEAR			(0x7)
#define FMP_SECURITY			(0x8)

#define FMP_DESC_OFF			(0x0)
#define FMP_DESC_ON			(0x1)

#define FMP_SMU_OFF			(0x0)
#define FMP_SMU_ON			(0x1)

#define FMP_SMU_INIT			(0x0)
#define FMP_SMU_SET			(0x1)
#define FMP_SMU_RESUME			(0x2)
#define FMP_SMU_DUMP			(0x3)

#if defined(CONFIG_SOC_EXYNOS7420)
#define UFS_FMP				(0x15572000)
#define EMMC0_FMP			(0x15741000)
#elif defined(CONFIG_SOC_EXYNOS8890)
#define UFS_FMP				(0x155A2000)
#define EMMC0_FMP			(0x15561000)
#define EMMC2_FMP			(0x15741000)
#endif

/* For DTRNG Access */
#define HWRNG_INIT			(0x0)
#define HWRNG_EXIT			(0x1)
#define HWRNG_GET_DATA			(0x2)
#define HWRNG_RESUME			(0x3)

/* For CFW group */
#define CFW_DISP_RW			(3)
#define CFW_VPP0			(5)
#define CFW_VPP1			(6)

#define SMC_TZPC_OK			(2)

#define PROT_MFC			(0)
#define PROT_MSCL0			(1)
#define PROT_MSCL1			(2)
#define PROT_G0				(3)
#define PROT_G1				(4)
#define PROT_VG0			(5)
#define PROT_VG1			(6)
#define PROT_G2				(7)
#define PROT_G3				(8)
#define PROT_VGR0			(9)
#define PROT_VGR1			(10)
#define PROT_WB1			(11)
#define PROT_G3D			(12)
#define PROT_JPEG			(13)
#define PROT_G2D			(14)

#ifndef __ASSEMBLY__
/* Return value from DRM LDFW */
enum drmdrv_result_t {
	DRMDRV_OK				= 0x0000,

	/* Error lists for common driver */
	E_DRMDRV_INVALID			= 0x1001,
	E_DRMDRV_INVALID_ADDR_ALIGN		= 0x1002,
	E_DRMDRV_INVALID_SIZE_ALIGN		= 0x1003,
	E_DRMDRV_INVALID_MEMCPY_LENGTH		= 0x1004,
	E_DRMDRV_ADDR_OUT_OF_DRAM		= 0x1005,
	E_DRMDRV_ADDR_OUT_OF_SECMEM		= 0x1006,
	E_DRMDRV_INVALID_ADDR			= 0x1007,
	E_DRMDRV_INVALID_SIZE			= 0x1008,
	E_DRMDRV_INVALID_CMD			= 0x1009,
	E_DRMDRV_ADDR_OVERFLOWED		= 0x100A,
	E_DRMDRV_ADDR_OVERLAP_SECOS		= 0x100B,

	/* Error lists for TZASC driver */
	E_DRMDRV_TZASC_ALIGN_CHECK		= 0x2001,
	E_DRMDRV_TZASC_CONTIG_CHECK		= 0x2002,
	E_DRMDRV_TZASC_GET_ORDER		= 0x2003,
	E_DRMDRV_TZASC_INVALID_INDEX		= 0x2004,
	E_DRMDRV_TZASC_INVALID_ENABLED		= 0x2005,
	E_DRMDRV_TZASC_NOT_PROTECTED		= 0x2006,

	/* Erorr lists for media driver */
	E_DRMDRV_MEDIA_CHECK_POWER		= 0x3001,
	E_DRMDRV_MEDIA_CHECK_CLOCK		= 0x3002,
	E_DRMDRV_MEDIA_CHECK_SMMU_CLOCK		= 0x3003,
	E_DRMDRV_MEDIA_CHECK_SMMU_ENABLED	= 0x3004,
	E_DRMDRV_WB_CHECK_FAILED		= 0x3005,
	E_DRMDRV_HDMI_WITH_NO_HDCP_FAILED	= 0x3006,
	E_DRMDRV_MSCL_LOCAL_PATH_FAILED		= 0x3007,
	E_DRMDRV_HDMI_POWER_OFF			= 0x3008,
	E_DRMDRV_HDMI_CLOCK_OFF			= 0x3009,
	E_DRMDRV_INVALID_REFCOUNT		= 0x300A,

	/* Error lists for g2d driver */
	E_DRMDRV_G2D_INVALID_PARAM		= 0x4001,
	E_DRMDRV_G2D_BLIT_TIMEOUT		= 0x4002,

	/* Error lists for RTC driver */
	E_DRMDRV_GET_RTC_TIME			= 0x5001,
	E_DRMDRV_SET_RTC_TIME			= 0x5002,
	E_DRMDRV_GET_RTC_TICK_TIME		= 0x5003,
	E_DRMDRV_SET_RTC_TICK_TIME		= 0x5004,

	/* Error lists for CFW driver */
	E_DRMDRV_CFW_ERROR			= 0x6001,
	E_DRMDRV_CFW_BUFFER_LIST_FULL		= 0x6002,
	E_DRMDRV_CFW_NOT_PROTECTED_BUFFER	= 0x6003,
	E_DRMDRV_CFW_INVALID_DEV_ARG		= 0x6004,
	E_DRMDRV_CFW_INIT_FAIL			= 0x6005,
	E_DRMDRV_CFW_PROT_FAIL			= 0x6006,
	E_DRMDRV_CFW_ENABLED_ALREADY		= 0x6007,
	E_DRMDRV_CFW_NOT_EXIST_IN_CFW_BUFF_LIST	= 0x6008,
	E_DRMDRV_CFW_DUPLICATED_BUFFER		= 0x6009,
	E_DRMDRV_CFW_BUFFER_IS_OVERLAPPED	= 0x600A,

	/* Error lists for Secure Buffer */
	E_DRMDRV_DUPLICATED_BUFFER		= 0x7001,
	E_DRMDRV_BUFFER_LIST_FULL		= 0x7002,
	E_DRMDRV_NOT_EXIST_IN_SEC_BUFFER_LIST	= 0x7003,
	E_DRMDRV_OVERLAP_RESERVED_ASP_REGION	= 0x7004,
	E_DRMDRV_INVALID_BUFFER_TYPE		= 0x7005,

	/* Error lists for MFC FW */
	E_DRMDRV_MFC_FW_IS_NOT_PROTECTED	= 0x8001,
	E_DRMDRV_MFC_FW_ARG_IS_NULL		= 0x8002,
	E_DRMDRV_MFC_FW_INVALID_SIZE		= 0x8003,
};

extern int __exynos_smc(unsigned long cmd, unsigned long arg1, unsigned long arg2, unsigned long arg3);
extern int exynos_smc(unsigned long cmd, unsigned long arg1, unsigned long arg2, unsigned long arg3);
extern int exynos_smc_readsfr(unsigned long addr, unsigned long* val);
#endif

#endif	/* __SMC_H__ */
