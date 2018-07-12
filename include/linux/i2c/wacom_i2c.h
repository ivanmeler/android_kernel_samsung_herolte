#ifndef _LINUX_WACOM_I2C_H
#define _LINUX_WACOM_I2C_H

#include <linux/types.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/*I2C address for digitizer and its boot loader*/
#define WACOM_I2C_ADDR 0x56
#if defined(CONFIG_EPEN_WACOM_G9PL) \
	|| defined(CONFIG_EPEN_WACOM_G9PLL) \
	|| defined(CONFIG_EPEN_WACOM_G10PM) \
	|| defined(CONFIG_EPEN_WACOM_W9012) \
	|| defined(CONFIG_EPEN_WACOM_W9014) \
	|| defined(CONFIG_EPEN_WACOM_W9018)
#define WACOM_I2C_BOOT 0x09
#else
#define WACOM_I2C_BOOT 0x57
#endif

#define WACOM_MAX_COORD_X 12576
#define WACOM_MAX_COORD_Y 7074
#define WACOM_MAX_PRESSURE 1023

#define WACOM_GPIO_CNT 4

#define WACOM_IC_9001 9001
#define WACOM_IC_9002 9002
#define WACOM_IC_9007 9007
#define WACOM_IC_9010 9010
#define WACOM_IC_9012 9012

/*sec_class sysfs*/
extern struct class *sec_class;

struct wacom_g5_callbacks {
	int (*check_prox)(struct wacom_g5_callbacks *);
};

struct wacom_g5_platform_data {
	int gpios[WACOM_GPIO_CNT];
	u32 flag_gpio[WACOM_GPIO_CNT];
	int irq_type;
	char *name;
	int x_invert;
	int y_invert;
	int xy_switch;
	int min_x;
	int max_x;
	int min_y;
	int max_y;
	u32 origin[2];
	int max_pressure;
	int min_pressure;
	char *fw_path;
	u32 ic_type;
	u32 boot_on_ldo;
	bool use_query_cmd;
	char *project_name;
	char *model_name;
	void (*compulsory_flash_mode)(bool);
	int (*init_platform_hw)(void);
	int (*exit_platform_hw)(void);
	/*int (*suspend_platform_hw)(void);
	int (*resume_platform_hw)(void);*/
	int (*suspend_platform_hw)(void);
	int (*resume_platform_hw)(void);
#ifdef CONFIG_HAS_EARLYSUSPEND
	int (*early_suspend_platform_hw)(void);
	int (*late_resume_platform_hw)(void);
#endif
	int (*reset_platform_hw)(void);
	void (*register_cb)(struct wacom_g5_callbacks *);
	int (*get_irq_state)(void);
};

#endif /* _LINUX_WACOM_I2C_H */
