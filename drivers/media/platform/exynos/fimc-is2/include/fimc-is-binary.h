/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_BINARY_H
#define FIMC_IS_BINARY_H

#include "fimc-is-config.h"

#ifdef VENDER_PATH
#define FIMC_IS_FW_PATH				"/system/vendor/firmware/"
#define FIMC_IS_FW_DUMP_PATH			"/data/camera/"
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_FW_SDCARD			"/data/media/0/fimc_is_fw.bin"
#define FIMC_IS_FW				"fimc_is_fw.bin"
#define FIMC_IS_ISP_LIB_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_ISP_LIB				"fimc_is_lib_isp.bin"
#define FIMC_IS_VRA_LIB_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_VRA_LIB				"fimc_is_lib_vra.bin"

#else
#define FIMC_IS_FW_PATH 			"/system/vendor/firmware/"
#define FIMC_IS_FW_DUMP_PATH			"/data/"
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/"
#define FIMC_IS_FW_SDCARD			"/data/fimc_is_fw2.bin"
#define FIMC_IS_FW				"fimc_is_fw2.bin"
#define FIMC_IS_ISP_LIB_SDCARD_PATH		"/data/"
#define FIMC_IS_ISP_LIB				"fimc_is_lib_isp.bin"
#define FIMC_IS_VRA_LIB_SDCARD_PATH		"/data/"
#define FIMC_IS_VRA_LIB				"fimc_is_lib_vra.bin"

#endif

#define FD_SW_BIN_NAME				"fimc_is_fd.bin"
#define FD_SW_SDCARD				"/data/fimc_is_fd.bin"

#ifdef ENABLE_IS_CORE
#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890) || defined(CONFIG_SOC_EXYNOS7580)|| defined(CONFIG_SOC_EXYNOS8890)
#define FIMC_IS_A5_MEM_SIZE			0x02000000
#define FIMC_IS_BACKUP_SIZE			0x02000000
#define FIMC_IS_REGION_SIZE			0x00005000
#define FIMC_IS_SHARED_OFFSET			0x01FC0000
#define FIMC_IS_SHARED_SIZE			0x00010000
#define FIMC_IS_TTB_OFFSET			0x01BF8000
#define FIMC_IS_TTB_SIZE			0x00008000
#define FIMC_IS_DEBUG_OFFSET			0x01F40000
#define FIMC_IS_DEBUG_SIZE			0x0007D000 /* 500KB */
#define FIMC_IS_CAL_OFFSET0			0x01FD0000
#define FIMC_IS_CAL_OFFSET1			0x01FE0000
#else
#define FIMC_IS_A5_MEM_SIZE			0x01B00000
#define FIMC_IS_BACKUP_SIZE			0x01B00000
#define FIMC_IS_REGION_SIZE			0x00005000
#define FIMC_IS_SHARED_OFFSET			0x019C0000
#define FIMC_IS_SHARED_SIZE			0x00010000
#define FIMC_IS_TTB_OFFSET			0x01BF8000
#define FIMC_IS_TTB_SIZE			0x00008000
#define FIMC_IS_DEBUG_OFFSET			0x01F40000
#define FIMC_IS_DEBUG_SIZE			0x0007D000 /* 500KB */
#define FIMC_IS_CAL_OFFSET0			0x01FD0000
#define FIMC_IS_CAL_OFFSET1			0x01FE0000
#endif
#else /* #ifdef ENABLE_IS_CORE */
/* static reserved memory for libraries */
#define FIMC_IS_LIB_OFFSET		(VMALLOC_START + 0xF6000000)

#define FIMC_IS_VRA_LIB_ADDR		(FIMC_IS_LIB_OFFSET + 0x04000000)
#define FIMC_IS_VRA_LIB_SIZE		SZ_512K

#define FIMC_IS_SDK_LIB_ADDR		(FIMC_IS_LIB_OFFSET + 0x04080000)
#define FIMC_IS_SDK_LIB_SIZE		(SZ_2M + SZ_1M)

#define FIMC_IS_GUARD_SIZE		(0x00001000)

#define FIMC_IS_SETFILE_OFFSET		(0x00001000)
#define FIMC_IS_SETFILE_SIZE		(0x00100000)

#define FIMC_IS_REAR_CALDATA_OFFSET	(FIMC_IS_SETFILE_OFFSET + \
					FIMC_IS_SETFILE_SIZE + \
					FIMC_IS_GUARD_SIZE)
#define FIMC_IS_REAR_CALDATA_SIZE	(0x00010000)

#define FIMC_IS_FRONT_CALDATA_OFFSET	(FIMC_IS_REAR_CALDATA_OFFSET + \
					FIMC_IS_REAR_CALDATA_SIZE)
#define FIMC_IS_FRONT_CALDATA_SIZE	(0x00010000)

#define FIMC_IS_DEBUG_REGION_OFFSET	(FIMC_IS_FRONT_CALDATA_OFFSET + \
					FIMC_IS_FRONT_CALDATA_SIZE + \
					FIMC_IS_GUARD_SIZE)
#define FIMC_IS_DEBUG_REGION_SIZE	(0x0007D000)

#define FIMC_IS_SHARED_REGION_OFFSET	(FIMC_IS_DEBUG_REGION_OFFSET + \
					FIMC_IS_DEBUG_REGION_SIZE + \
					FIMC_IS_GUARD_SIZE)
#define FIMC_IS_SHARED_REGION_SIZE	(0x00010000)

#define FIMC_IS_DATA_REGION_OFFSET	(FIMC_IS_SHARED_REGION_OFFSET + \
					FIMC_IS_SHARED_REGION_SIZE + \
					FIMC_IS_GUARD_SIZE)
#define FIMC_IS_DATA_REGION_SIZE	(0x00010000)

#define FIMC_IS_PARAM_REGION_OFFSET	(FIMC_IS_DATA_REGION_OFFSET + \
					FIMC_IS_DATA_REGION_SIZE + \
					FIMC_IS_GUARD_SIZE)
#define FIMC_IS_PARAM_REGION_SIZE	(0x00005000)

/* for compatibility */
#define FIMC_IS_A5_MEM_SIZE		(FIMC_IS_PARAM_REGION_OFFSET + \
					(4*FIMC_IS_PARAM_REGION_SIZE))
#define FIMC_IS_REGION_SIZE		(FIMC_IS_PARAM_REGION_SIZE)

#define FIMC_IS_SHARED_OFFSET		(0)
#define FIMC_IS_TTB_OFFSET		(0x01BF8000)
#define FIMC_IS_TTB_SIZE		(0x00008000)
#define FIMC_IS_DEBUG_OFFSET		FIMC_IS_DEBUG_REGION_OFFSET
#define FIMC_IS_DEBUG_SIZE		FIMC_IS_DEBUG_REGION_SIZE /* 500KB */
#define FIMC_IS_CAL_OFFSET0		(FIMC_IS_REAR_CALDATA_OFFSET)
#define FIMC_IS_CAL_OFFSET1		(FIMC_IS_FRONT_CALDATA_OFFSET)

/* for internal DMA buffers */
#define FIMC_IS_THUMBNAIL_SDMA_OFFSET	(FIMC_IS_A5_MEM_SIZE + FIMC_IS_GUARD_SIZE)
#define FIMC_IS_THUMBNAIL_SDMA_SIZE	(0x00400000)

#define FIMC_IS_RESERVE_DMA_OFFSET	(FIMC_IS_THUMBNAIL_SDMA_OFFSET + \
					FIMC_IS_THUMBNAIL_SDMA_SIZE + \
					FIMC_IS_GUARD_SIZE)

/* for reserve DMA buffers */
#define FIMC_IS_LHFD_MAP_OFFSET		FIMC_IS_RESERVE_DMA_OFFSET
#define FIMC_IS_LHFD_MAP_SIZE		(0x009F0000)	/* 9.9375MB */

#define FIMC_IS_VRA_DMA_OFFSET		FIMC_IS_RESERVE_DMA_OFFSET
#define FIMC_IS_VRA_DMA_SIZE		(0x00400000)	/* 4MB */

#if defined (ENABLE_FD_SW)
#define FIMC_IS_RESERVE_DMA_SIZE	FIMC_IS_LHFD_MAP_SIZE
#elif defined (ENABLE_VRA)
#define FIMC_IS_RESERVE_DMA_SIZE	FIMC_IS_VRA_DMA_SIZE
#else
#define FIMC_IS_RESERVE_DMA_SIZE	(0x00400000)
#endif

/* for reserve Memory */
#define FIMC_IS_RESERVE_OFFSET		(FIMC_IS_RESERVE_DMA_OFFSET + \
					FIMC_IS_RESERVE_DMA_SIZE + \
					FIMC_IS_GUARD_SIZE)

#define FIMC_IS_RESERVE_SIZE		(0x00900000)

/* EEPROM offset */
#define EEPROM_HEADER_BASE		(0)
#define EEPROM_OEM_BASE			(0x100)
#define EEPROM_AWB_BASE			(0x200)
#define EEPROM_SHADING_BASE		(0x300)
#define EEPROM_PDAF_BASE		(0x0)

/* FROM offset */
#define FROM_HEADER_BASE		(0)
#define FROM_OEM_BASE			(0x1000)
#define FROM_AWB_BASE			(0x2000)
#define FROM_SHADING_BASE		(0x3000)
#define FROM_PDAF_BASE			(0x5000)

#endif
#define FIMC_IS_DEBUGCTL_OFFSET			(FIMC_IS_DEBUG_OFFSET + FIMC_IS_DEBUG_SIZE)

#define FIMC_IS_FW_BASE_MASK			((1 << 26) - 1)
#define FIMC_IS_VERSION_SIZE			42
#define FIMC_IS_SETFILE_VER_OFFSET		0x40
#define FIMC_IS_SETFILE_VER_SIZE		52

#define FIMC_IS_CAL_SDCARD			"/data/cal_data.bin"
#define FIMC_IS_CAL_RETRY_CNT			(2)
#define FIMC_IS_FW_RETRY_CNT			(2)

enum fimc_is_bin_type {
	FIMC_IS_BIN_FW = 0,
	FIMC_IS_BIN_SETFILE,
	FIMC_IS_BIN_LIBRARY,
};

struct fimc_is_binary {
	void *data;
	size_t size;

	const struct firmware *fw;

	unsigned long customized;

	/* request_firmware retry */
	unsigned int retry_cnt;
	int	retry_err;

	/* custom memory allocation */
	void *(*alloc)(unsigned long size);
	void (*free)(const void *buf);
};

void setup_binary_loader(struct fimc_is_binary *bin,
				unsigned int retry_cnt, int retry_err,
				void *(*alloc)(unsigned long size),
				void (*free)(const void *buf));
int request_binary(struct fimc_is_binary *bin, const char *path,
				const char *name, struct device *device);
void release_binary(struct fimc_is_binary *bin);
int was_loaded_by(struct fimc_is_binary *bin);

#endif
