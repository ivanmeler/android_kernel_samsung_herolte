/*
 * =====================================================================================
 *
 *       Filename:  Iccc_Interface.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/20/2015 12:13:42 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  SRI-N
 *        Company:  Samsung Electronics
 *
 *        Copyright (c) 2015 by Samsung Electronics, All rights reserved. 
 *
 * =====================================================================================
 */
#ifndef Iccc_Interface_H_
#define Iccc_Interface_H_

/*  struct for ICCC information from memory */

#define MAX_IMAGES 6
#define RESERVED_BYTES 96
#define BL_MAGIC_STR 0xFFFA
#define TA_MAGIC_STR 0xFFFB
#define KERN_MAGIC_STR 0xFFFC
#define SYS_MAGIC_STR 0xFFFD

typedef struct {
	uint16_t magic_str;
	uint16_t used_size;
}secure_param_header_t;

typedef struct {
	secure_param_header_t header;
	uint32_t rp_ver;
	uint32_t kernel_rp;
	uint32_t system_rp;
	uint32_t test_bit;
	uint32_t sec_boot;
	uint32_t react_lock;
	uint32_t kiwi_lock;
	uint32_t frp_lock;
	uint32_t cc_mode;
	uint32_t mdm_mode;
	uint32_t curr_bin_status;
	uint32_t afw_value;
	uint32_t warranty_bit;
	uint32_t kap_status;
	uint32_t image_status[MAX_IMAGES];
	uint8_t reserved[RESERVED_BYTES];
}bl_secure_info_t;

typedef struct {
	secure_param_header_t header;
	uint32_t  pkm_text;
	uint32_t  pkm_ro;
	uint32_t  selinux_status;
	uint8_t reserved[RESERVED_BYTES];
}ta_secure_info_t;

typedef struct {
	secure_param_header_t header;
	uint32_t dmv_status;
	uint8_t reserved[RESERVED_BYTES];
}kern_secure_info_t;

#if ANDROID_VERSION >= 70000
typedef struct {
	secure_param_header_t header;
	uint32_t sysscope_flag;
	uint32_t trustboot_flag;
	uint32_t tima_version_flag;
	uint8_t reserved[RESERVED_BYTES - 4];
}sys_secure_info_t;
#else
typedef struct {
	secure_param_header_t header;
	uint32_t sysscope_flag;
	uint32_t trustboot_flag;
	uint8_t reserved[RESERVED_BYTES];
}sys_secure_info_t;
#endif

typedef struct {
	bl_secure_info_t  bl_secure_info;
	ta_secure_info_t  ta_secure_info;
	kern_secure_info_t kern_secure_info;
	sys_secure_info_t sys_secure_info;
}iccc_secure_pamameters_info_t;


#define ICCC_SECURE_PARAMETERS_LENGTH			((uint32_t)sizeof(iccc_secure_pamameters_info_t))
#define ICCC_SECURE_PARAMETERS_READING_LENGTH	((uint32_t)0x4)

/* ICCC section types are defined */

#define ICCC_BL_SECURE_PARAMETERS_OFFSET		((uint32_t)0x164)

#define BL_ICCC_TYPE_START		0xFFF00000
#define TA_ICCC_TYPE_START		0xFF000000
#define KERN_ICCC_TYPE_START		0xFF100000
#define SYS_ICCC_TYPE_START		0xFF200000

#define ICCC_SECTION_MASK			0xFFF00000
#define ICCC_SECTION_TYPE(type)		(ICCC_SECTION_MASK & (type))

/*  BL Secure Parameters */
#define RP_VER				(BL_ICCC_TYPE_START+0x00000)
#define KERNEL_RP			(BL_ICCC_TYPE_START+0x00001)
#define SYSTEM_RP			(BL_ICCC_TYPE_START+0x00002)
#define TEST_BIT			(BL_ICCC_TYPE_START+0x00003)
#define SEC_BOOT			(BL_ICCC_TYPE_START+0x00004)
#define REACT_LOCK			(BL_ICCC_TYPE_START+0x00005)
#define KIWI_LOCK			(BL_ICCC_TYPE_START+0x00006)
#define FRP_LOCK			(BL_ICCC_TYPE_START+0x00007)
#define CC_MODE				(BL_ICCC_TYPE_START+0x00008)
#define MDM_MODE			(BL_ICCC_TYPE_START+0x00009)
#define CURR_BIN_STATUS		(BL_ICCC_TYPE_START+0x0000A)
#define AFW_VALUE			(BL_ICCC_TYPE_START+0x0000B)
#define WARRANTY_BIT		(BL_ICCC_TYPE_START+0x0000C)
#define KAP_STATUS			(BL_ICCC_TYPE_START+0x0000D)
#define IMAGE_STATUS1		(BL_ICCC_TYPE_START+0x0000E)
#define IMAGE_STATUS2		(BL_ICCC_TYPE_START+0x0000F)
#define IMAGE_STATUS3		(BL_ICCC_TYPE_START+0x00010)
#define IMAGE_STATUS4		(BL_ICCC_TYPE_START+0x00011)
#define IMAGE_STATUS5		(BL_ICCC_TYPE_START+0x00012)
#define IMAGE_STATUS6		(BL_ICCC_TYPE_START+0x00013)

/* end BL Secure Parameters */

/* TA Secure Parameters */

#define ICCC_TA_SECURE_PARAMETERS_OFFSET	((uint32_t)ICCC_BL_SECURE_PARAMETERS_OFFSET + sizeof (bl_secure_info_t))

#define PKM_TEXT			(TA_ICCC_TYPE_START+0x00000)
#define PKM_RO				(TA_ICCC_TYPE_START+0x00001)
#define SELINUX_STATUS		(TA_ICCC_TYPE_START+0x00002)

/* end TA Secure Parameters */

/* KERNEL  Parameter */
#define ICCC_KERN_SECURE_PARAMETERS_OFFSET	((uint32_t)ICCC_TA_SECURE_PARAMETERS_OFFSET + sizeof (ta_secure_info_t))

#define DMV_STATUS			(KERN_ICCC_TYPE_START+0x00000)

/* end KERNEL Hash Parameter */

/* System  Parameter */
#define ICCC_SYS_SECURE_PARAMETERS_OFFSET	((uint32_t)ICCC_KERN_SECURE_PARAMETERS_OFFSET + sizeof (kern_secure_info_t))

#define SYSSCOPE_FLAG		(SYS_ICCC_TYPE_START+0x00000)
#define TRUSTBOOT_FLAG		(SYS_ICCC_TYPE_START+0x00001)
#define TIMA_VERSION_FLAG	(SYS_ICCC_TYPE_START+0x00002)

/* end System Parameter */

uint32_t Iccc_SaveData_Kernel(uint32_t type, uint32_t value);
uint32_t Iccc_ReadData_Kernel(uint32_t type, uint32_t *value);

#endif
/*  struct for TLC&TA communication */