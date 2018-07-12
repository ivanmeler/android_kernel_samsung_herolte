/*
 * /include/media/exynos_fimc_is_sensor.h
 *
 * Copyright (C) 2012 Samsung Electronics, Co. Ltd
 *
 * Exynos series exynos_fimc_is_sensor device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MEDIA_EXYNOS_SENSOR_H
#define MEDIA_EXYNOS_SENSOR_H

#include <linux/platform_device.h>

#define FIMC_IS_SENSOR_DEV_NAME "exynos-fimc-is-sensor"
#define FIMC_IS_PINNAME_LEN 32
#define FIMC_IS_MAX_NAME_LEN 32

enum exynos_csi_id {
	CSI_ID_A = 0,
	CSI_ID_B = 1,
	CSI_ID_C = 2,
	CSI_ID_D = 3,
	CSI_ID_MAX
};

enum exynos_flite_id {
	FLITE_ID_A = 0,
	FLITE_ID_B = 1,
	FLITE_ID_C = 2,
	FLITE_ID_D = 3,
	FLITE_ID_END,
	FLITE_ID_NOTHING = 100,
};

enum exynos_sensor_channel {
	SENSOR_CONTROL_I2C0	 = 0,
	SENSOR_CONTROL_I2C1	 = 1,
	SENSOR_CONTROL_I2C2	 = 2
};

enum exynos_sensor_position {
	SENSOR_POSITION_REAR = 0,
	SENSOR_POSITION_FRONT = 1,
	SENSOR_POSITION_SECURE = 2,
};

enum exynos_sensor_id {
	SENSOR_NAME_NOTHING		 = 0,
	SENSOR_NAME_S5K3H2		 = 1,
	SENSOR_NAME_S5K6A3		 = 2,
	SENSOR_NAME_S5K3H5		 = 3,
	SENSOR_NAME_S5K3H7		 = 4,
	SENSOR_NAME_S5K3H7_SUNNY	 = 5,
	SENSOR_NAME_S5K3H7_SUNNY_2M	 = 6,
	SENSOR_NAME_S5K6B2		 = 7,
	SENSOR_NAME_S5K3L2		 = 8,
	SENSOR_NAME_S5K4E5		 = 9,
	SENSOR_NAME_S5K2P2		 = 10,
	SENSOR_NAME_S5K8B1		 = 11,
	SENSOR_NAME_S5K1P2		 = 12,
	SENSOR_NAME_S5K4H5		 = 13,
	SENSOR_NAME_S5K3M2		 = 14,
	SENSOR_NAME_S5K2P2_12M		 = 15,
	SENSOR_NAME_S5K6D1		 = 16,
	SENSOR_NAME_S5K5E3		 = 17,
	SENSOR_NAME_S5K2T2		 = 18,
	SENSOR_NAME_S5K2P3		 = 19,
	SENSOR_NAME_S5K2P8		 = 20,
	SENSOR_NAME_S5K4E6		 = 21,
	SENSOR_NAME_S5K5E2		 = 22,
	SENSOR_NAME_S5K3P3		 = 23,
	SENSOR_NAME_S5K4H5YC		 = 24,
	SENSOR_NAME_S5K2X8		 = 28,
	SENSOR_NAME_S5K2L1		 = 29,
	SENSOR_NAME_S5K4H5YC_FF          = 34,
	SENSOR_NAME_S5K4EC		 = 57,

	SENSOR_NAME_IMX135		 = 101, /* 101 ~ 200 Sony sensors */
	SENSOR_NAME_IMX134		 = 102,
	SENSOR_NAME_IMX175		 = 103,
	SENSOR_NAME_IMX240		 = 104,
	SENSOR_NAME_IMX220		 = 105,
	SENSOR_NAME_IMX228		 = 106,
	SENSOR_NAME_IMX219		 = 107,
	SENSOR_NAME_IMX230		 = 108,
	SENSOR_NAME_IMX260		 = 109,

	SENSOR_NAME_SR261		 = 201, /* 201 ~ 300 Other vendor sensors */
	SENSOR_NAME_OV5693		 = 202,
	SENSOR_NAME_SR544		 = 203,
	SENSOR_NAME_OV5670		 = 204,

	SENSOR_NAME_CUSTOM		 = 301,
	SENSOR_NAME_SR200		 = 302, // SoC Module
	SENSOR_NAME_SR352		 = 303,
	SENSOR_NAME_SR130PC20	 = 304,
	SENSOR_NAME_S5K5E6		 = 305,
	SENSOR_NAME_VIRTUAL_ZEBU = 901,
	SENSOR_NAME_END,
};

enum actuator_name {
	ACTUATOR_NAME_AD5823	= 1,
	ACTUATOR_NAME_DWXXXX	= 2,
	ACTUATOR_NAME_AK7343	= 3,
	ACTUATOR_NAME_HYBRIDVCA	= 4,
	ACTUATOR_NAME_LC898212	= 5,
	ACTUATOR_NAME_WV560	= 6,
	ACTUATOR_NAME_AK7345	= 7,
	ACTUATOR_NAME_DW9804	= 8,
	ACTUATOR_NAME_AK7348	= 9,
	ACTUATOR_NAME_SHAF3301	= 10,
	ACTUATOR_NAME_BU64241GWZ = 11,
	ACTUATOR_NAME_AK7371	= 12,
	ACTUATOR_NAME_DW9807	= 13,
	ACTUATOR_NAME_ZC533	= 14,
	ACTUATOR_NAME_BU63165	= 15,
	ACTUATOR_NAME_END,
	ACTUATOR_NAME_NOTHING	= 100,
};

enum flash_drv_name {
	FLADRV_NAME_KTD267	= 1,	/* Gpio type(Flash mode, Movie/torch mode) */
	FLADRV_NAME_AAT1290A	= 2,
	FLADRV_NAME_MAX77693	= 3,
	FLADRV_NAME_AS3643	= 4,
	FLADRV_NAME_KTD2692	= 5,
	FLADRV_NAME_LM3560	= 6,
	FLADRV_NAME_SKY81296	= 7,
	FLADRV_NAME_RT5033	= 8,
	FLADRV_NAME_AS3647	= 9,
	FLADRV_NAME_LM3646	= 10,
	FLADRV_NAME_DRV_FLASH_GPIO = 11, /* Common Gpio type(Flash mode, Movie/torch mode) */
	FLADRV_NAME_LM3644	= 12,
	FLADRV_NAME_END,
	FLADRV_NAME_NOTHING	= 100,
};

enum from_name {
	FROMDRV_NAME_W25Q80BW	= 1,	/* Spi type */
	FROMDRV_NAME_FM25M16A	= 2,	/* Spi type */
	FROMDRV_NAME_FM25M32A	= 3,
	FROMDRV_NAME_NOTHING	= 100,
};

enum preprocessor_name {
	PREPROCESSOR_NAME_73C1 = 1,	/* SPI, I2C, FIMC Lite */
	PREPROCESSOR_NAME_73C2 = 2,
	PREPROCESSOR_NAME_73C3 = 3,
	PREPROCESSOR_NAME_END,
	PREPROCESSOR_NAME_NOTHING = 100,
};

enum ois_name {
	OIS_NAME_IDG2030	= 1,
	OIS_NAME_END,
	OIS_NAME_NOTHING	= 100,
};

enum sensor_peri_type {
	SE_NULL		= 0,
	SE_I2C		= 1,
	SE_SPI		= 2,
	SE_GPIO		= 3,
	SE_MPWM		= 4,
	SE_ADC		= 5,
	SE_DMA		= 6,
};

enum sensor_dma_channel_type {
	DMA_CH_NOT_DEFINED	= 100,
};

enum sensor_retention_state
{
	SENSOR_RETENTION_DISABLE = 0, /* disable */
	SENSOR_RETENTION_READY  = 1,  /* cold boot */
	SENSOR_RETENTION_USE = 2,     /* warm boot */
};

struct i2c_type {
	u32 channel;
	u32 slave_address;
	u32 speed;
};

struct spi_type {
	u32 channel;
};

struct gpio_type {
	u32 first_gpio_port_no;
	u32 second_gpio_port_no;
};

struct dma_type {
	/*
	 * This value has FIMC_LITE CH or CSIS virtual CH.
	 * If it's not defined in some SOC, it should be DMA_CH_NOT_DEFINED
	 */
	u32 channel;
};

union sensor_peri_format {
	struct i2c_type i2c;
	struct spi_type spi;
	struct gpio_type gpio;
	struct dma_type dma;
};

struct sensor_protocol1 {
	u32 product_name;
	enum sensor_peri_type peri_type;
	union sensor_peri_format peri_setting;
	u32 csi_ch;
	u32 cal_address;
	u32 reserved[2];
};

struct sensor_peri_info {
	bool valid;
	enum sensor_peri_type peri_type;
	union sensor_peri_format peri_setting;
};

struct sensor_protocol2 {
	u32 product_name; /* enum preprocessor_name */
	struct sensor_peri_info peri_info0;
	struct sensor_peri_info peri_info1;
	struct sensor_peri_info peri_info2;
	struct sensor_peri_info reserved[2];
};

struct sensor_open_extended {
	struct sensor_protocol1 sensor_con;
	struct sensor_protocol1 actuator_con;
	struct sensor_protocol1 flash_con;
	struct sensor_protocol1 from_con;
	struct sensor_protocol2 preprocessor_con;
	struct sensor_protocol1 ois_con;
	u32 mclk;
	u32 mipi_lane_num;
	u32 mipi_settle_line;
	u32 mipi_speed;
	/* Use sensor retention mode */
	u32 use_retention_mode;
	/* Activatiing sensor self calibration mode (6A3) */
	u32 self_calibration_mode;
	/* This field is to adjust I2c clock based on ACLK200 */
	/* This value is varied in case of rev 0.2 */
	u32 I2CSclk;
	struct sensor_protocol1 reserved[4];
};

struct exynos_platform_fimc_is_sensor {
	int (*iclk_get)(struct device *dev, u32 scenario, u32 channel);
	int (*iclk_cfg)(struct device *dev, u32 scenario, u32 channel);
	int (*iclk_on)(struct device *dev,u32 scenario, u32 channel);
	int (*iclk_off)(struct device *dev, u32 scenario, u32 channel);
	int (*mclk_on)(struct device *dev, u32 scenario, u32 channel);
	int (*mclk_off)(struct device *dev, u32 scenario, u32 channel);
	u32 id;
	u32 scenario;
	u32 csi_ch;
	u32 flite_ch;
	u32 is_bns;
};

int exynos_fimc_is_sensor_iclk_cfg(struct device *dev,
	u32 scenario,
	u32 channel);
int exynos_fimc_is_sensor_iclk_on(struct device *dev,
	u32 scenario,
	u32 channel);
int exynos_fimc_is_sensor_iclk_off(struct device *dev,
	u32 scenario,
	u32 channel);
int exynos_fimc_is_sensor_mclk_on(struct device *dev,
	u32 scenario,
	u32 channel);
int exynos_fimc_is_sensor_mclk_off(struct device *dev,
	u32 scenario,
	u32 channel);

#endif /* MEDIA_EXYNOS_SENSOR_H */
