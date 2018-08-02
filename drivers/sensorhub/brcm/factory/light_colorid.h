#ifndef __SSP_FACTORY_LIGHT_COLORID_H__

#define __SSP_FACTORY_LIGHT_COLORID_H__

#include "../ssp.h"

#define COLOR_ID_PATH 		"/sys/class/lcd/panel/window_type"

/*

	file name 	: version

	contents	: YYMMDD.ColorIDs 

*/

#define VERSION_FILE_NAME	"/efs/FactoryApp/version"

/* 

	file name : predefine+ColorID
	contents  : r_coef,g_coef,b_coef,c_coef,dgf,cct_coef,cct_offset,threshhigh, threshlow, crc
*/

#define PREDEFINED_FILE_NAME		"/efs/FactoryApp/predefine"

#if defined(CONFIG_SENSORS_SSP_HAECHI_880)
#define COLOR_NUM 			3
#else
#define COLOR_NUM 			6
#endif

#define COLOR_NUM_MAX		16

#define COLOR_ID_VERSION_FILE_LENGTH		30

#define COLOR_ID_PREDEFINED_FILE_LENGTH		60

#define COLOR_ID_IDS_LENGTH					20

#define COLOR_ID_PREDEFINED_FILENAME_LENTH	30

#define WRITE_ALL_DATA_NUMBER				11

ssize_t light_hiddendhole_version_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t light_hiddenhole_version_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
ssize_t light_hiddendhole_hh_write_all_data_show(struct device *dev,	struct device_attribute *attr, char *buf);
ssize_t light_hiddenhole_write_all_data_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
ssize_t light_hiddendhole_is_exist_efs_show(struct device *dev,struct device_attribute *attr, char *buf);
ssize_t light_hiddendhole_check_coef_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t light_hiddendhole_check_coef_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);

#endif

