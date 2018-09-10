/**
@file adpd142.h
@brief ADPD142  - Low level driver  Header 'H' File
*/
#ifndef _ADPD143_H_
#define _ADPD143_H_

/**
	@brief ADPD143 Slave address
*/
#define ADPD143_SLAVE_ADDR		0x64
/**
        @brief ADPD143 ChipID
*/
#define ADPD_CHIPID_0			0x0016
/**
	@brief ADPD143 ChipID 1
*/
#define ADPD_CHIPID_1			0x0116
/**
	@brief ADPD143 ChipID 2
*/
#define ADPD_CHIPID_2			0x0216
/**
	@brief ADPD143 ChipID 2
*/
#define ADPD_CHIPID_3			0x0316
/**
	@brief ADPD143 ChipID 2
*/
#define ADPD_CHIPID_4			0x0416
/**
	@brief ADPD143 ChipID
*/
#define ADPD_CHIPID(id)			ADPD_CHIPID_##id

/*ADPD143 REGISTER ADDRESS*/
/**
        @brief ADPD143 Interrupt Status Register
*/
#define ADPD_INT_STATUS_ADDR            0x0
/**
        @brief ADPD143 Interrupt Mask Register
*/
#define ADPD_INT_MASK_ADDR              0x1
/**
        @brief ADPD143 ChipID register
*/
#define ADPD_CHIPID_ADDR                0x8
/**
        @brief ADPD143 Operating mode Register
*/
#define ADPD_OP_MODE_ADDR               0x10
/**
        @brief ADPD143 Operating Mode Configuration register
*/
#define ADPD_OP_MODE_CFG_ADDR           0x11
/**
        @brief ADPD143 Gesture Control Register
*/
#define ADPD_GEST_CTRL_ADDR             0x27
/**
        @brief ADPD143 Gesture Threshold Register
*/
#define ADPD_GEST_THRESH_ADDR           0x28
/**
        @brief ADPD143 Gesture Size Register
*/
#define ADPD_GEST_SIZE_ADDR             0x29
/**
        @brief ADPD143 Proximity ON1 Threshold register
*/
#define ADPD_PROX_ON_TH1_ADDR           0x2A
/**
        @brief ADPD143 proximity OFF1 Threshold register
*/
#define ADPD_PROX_OFF_TH1_ADDR          0x2B
/**
        @brief ADPD143 Proximity ON2 Threshold register
*/
#define ADPD_PROX_ON_TH2_ADDR           0x2C
/**
        @brief ADPD143 Proximity OFF2 Threshold register
*/
#define ADPD_PROX_OFF_TH2_ADDR          0x2D
/**
        @brief ADPD143 Test PD register
*/
#define ADPD_TEST_PD_ADDR               0x52
/**
        @brief ADPD143 Access control register
*/
#define ADPD_ACCESS_CTRL_ADDR           0x5F
/**
        @brief ADPD143 FIFO register
*/
#define ADPD_DATA_BUFFER_ADDR           0x60


/**
	@brief ADPD143 maximum array size of Platform data
*/
#define MAX_CONFIG_REG_CNT              72
/**
	@brief ADPD143 Platform Data
*/
struct adpd_platform_data {
        unsigned short config_size;
        unsigned int config_data[MAX_CONFIG_REG_CNT];
};

extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device **dev, void * drvdata,
	struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);
#endif
