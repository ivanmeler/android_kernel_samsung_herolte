#ifndef FIMC_IS_EEPROM_REAR_4E6_V004_H
#define FIMC_IS_EEPROM_REAR_4E6_V004_H

/* Header referenced section */
#define EEP_HEADER_VERSION_START_ADDR      0x30
#define EEP_HEADER_CAL_MAP_VER_START_ADDR  0x40
#define EEP_HEADER_OEM_START_ADDR          0x0
#define EEP_HEADER_OEM_END_ADDR            0x4
#define EEP_HEADER_AWB_START_ADDR          0x8
#define EEP_HEADER_AWB_END_ADDR            0xC
#define EEP_HEADER_AP_SHADING_START_ADDR   0x18
#define EEP_HEADER_AP_SHADING_END_ADDR     0x1C
#define EEP_HEADER_C2_SHADING_START_ADDR   0x10
#define EEP_HEADER_C2_SHADING_END_ADDR     0x14
#define EEP_HEADER_PROJECT_NAME_START_ADDR 0x4C
#define EEP_HEADER_SENSOR_ID_ADDR          0x54

#define EEP_HEADER_CAL_DATA_START_ADDR                           0x0100

/* OEM referenced section */
#define EEP_OEM_VER_START_ADDR         0x150

/* AWB referenced section */
#define EEP_AWB_VER_START_ADDR         0x220

/* AP Shading referenced section */
#define EEP_AP_SHADING_VER_START_ADDR  0X3B00

/* Checksum referenced section */
#define EEP_CHECKSUM_HEADER_ADDR       0xFC
#define EEP_CHECKSUM_OEM_ADDR          0x1FC
#define EEP_CHECKSUM_AWB_ADDR          0x2FC
#define EEP_CHECKSUM_AP_SHADING_ADDR   0x3BFC

/* etc section */
#define FIMC_IS_MAX_CAL_SIZE	(16 * 1024)
#define HEADER_CRC32_LEN		(0x70)

#endif /* FIMC_IS_EEPROM_REAR_4E6_V004_H */
