/*
 * max77833-muic.h - MUIC for the Maxim 77833
 *
 * Copyright (C) 2015 Samsung Electrnoics
 * Insun Choi <insun77.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * This driver is based on max14577-muic.h
 *
 */

#ifndef __MAX77833_MUIC_H__
#define __MAX77833_MUIC_H__

#define MUIC_DEV_NAME			"muic-max77833"
#define MUIC_PASS4			(0x05)

enum max77833_muic_command_rw {
	COMMAND_READ = 0,
	COMMAND_WRITE = 1,
};

typedef enum {
	MAX77833_ADC_GND                 = 0x00,
	MAX77833_ADC_1K                  = 0x10, /* 0x010000 1K ohm */
	MAX77833_ADC_SEND_END            = 0x11, /* 0x010001 2K ohm */
	MAX77833_ADC_2_604K              = 0x12, /* 0x010010 2.604K ohm */
	MAX77833_ADC_3_208K              = 0x13, /* 0x010011 3.208K ohm */
	MAX77833_ADC_4_014K              = 0x14, /* 0x010100 4.014K ohm */
	MAX77833_ADC_4_820K              = 0x15, /* 0x010101 4.820K ohm */
	MAX77833_ADC_6_030K              = 0x16, /* 0x010110 6.030K ohm */
	MAX77833_ADC_8_030K              = 0x17, /* 0x010111 8.030K ohm */
	MAX77833_ADC_10_030K             = 0x18, /* 0x011000 10.030K ohm */
	MAX77833_ADC_12_030K             = 0x19, /* 0x011001 12.030K ohm */
	MAX77833_ADC_14_460K             = 0x1a, /* 0x011010 14.460K ohm */
	MAX77833_ADC_17_260K             = 0x1b, /* 0x011011 17.260K ohm */
	MAX77833_ADC_REMOTE_S11          = 0x1c, /* 0x011100 20.5K ohm */
	MAX77833_ADC_REMOTE_S12          = 0x1d, /* 0x011101 24.07K ohm */
	MAX77833_ADC_RESERVED_VZW        = 0x1e, /* 0x011110 28.7K ohm */
	MAX77833_ADC_INCOMPATIBLE_VZW    = 0x1f, /* 0x011111 34K ohm */
	MAX77833_ADC_SMARTDOCK           = 0x20, /* 0x100000 40.2K ohm */
	MAX77833_ADC_HMT                 = 0x21, /* 0x100001 49.9K ohm */
	MAX77833_ADC_AUDIODOCK           = 0x22, /* 0x100010 64.9K ohm */
	MAX77833_ADC_USB_LANHUB          = 0x23, /* 0x100011 80.07K ohm */
	MAX77833_ADC_CHARGING_CABLE      = 0x24, /* 0x100100 102K ohm */
	MAX77833_ADC_UNIVERSAL_MMDOCK    = 0x25, /* 0x100101 121K ohm */
	MAX77833_ADC_UART_CABLE          = 0x26, /* 0x100110 150K ohm */
	MAX77833_ADC_CEA936ATYPE1_CHG    = 0x27, /* 0x100111 200K ohm */
	MAX77833_ADC_JIG_USB_OFF         = 0x28, /* 0x101000 255K ohm */
	MAX77833_ADC_JIG_USB_ON          = 0x29, /* 0x101001 301K ohm */
	MAX77833_ADC_DESKDOCK            = 0x2a, /* 0x101010 365K ohm */
	MAX77833_ADC_CEA936ATYPE2_CHG    = 0x2b, /* 0x101011 442K ohm */
	MAX77833_ADC_JIG_UART_OFF        = 0x2c, /* 0x101100 523K ohm */
	MAX77833_ADC_JIG_UART_ON         = 0x2d, /* 0x101101 619K ohm */
	MAX77833_ADC_AUDIOMODE_W_REMOTE  = 0x2e, /* 0x101110 1000K ohm */
	MAX77833_ADC_OPEN                = 0x2f,

	MAX77833_ADC_OPEN_219            = 0xfb, /* ADC open or 219.3K ohm */
	MAX77833_ADC_219                 = 0xfc, /* ADC open or 219.3K ohm */

	MAX77833_ADC_UNDEFINED           = 0xfd, /* Undefied range */
	MAX77833_ADC_DONTCARE            = 0xfe, /* ADC don't care for MHL */
	MAX77833_ADC_ERROR               = 0xff, /* ADC value read error */
} max77833_adc_t;

typedef enum max77833_muic_command_opcode {
	COMMAND_CONFIG_READ		= 0x01,
	COMMAND_CONFIG_WRITE		= 0x02,
	COMMAND_SWITCH_READ		= 0x03,
	COMMAND_SWITCH_WRITE		= 0x04,
	COMMAND_SYSMSG_READ		= 0x05,
	COMMAND_CHGDET_READ		= 0x12,
	COMMAND_MONITOR_READ		= 0x21,
	COMMAND_MONITOR_WRITE		= 0x22,
#if defined(CONFIG_HV_MUIC_MAX77833_AFC)
	COMMAND_QC_DISABLE_READ		= 0x31,
	COMMAND_QC_ENABLE_READ		= 0x32,
	COMMAND_QC_AUTOSET_WRITE	= 0x34,
	COMMAND_AFC_DISABLE_READ	= 0x41,
	COMMAND_AFC_ENABLE_READ		= 0x42,
	COMMAND_AFC_SET_WRITE		= 0x43,
	COMMAND_AFC_CAPA_READ		= 0x44,
#endif
	COMMAND_CHGIN_READ		= 0x51,

	/* not cmd opcode, for notifier */
	NOTI_ATTACH			= 0xfa,
	NOTI_DETACH			= 0xfb,
	NOTI_LOGICALLY_ATTACH		= 0xfc,
	NOTI_LOGICALLY_DETACH		= 0xfd,

	COMMAND_NONE			= 0xff,
} muic_cmd_opcode;

#define	CMD_Q_SIZE			8

typedef struct max77833_muic_command_data {
	muic_cmd_opcode			opcode;
	u8				response;
	u8				read_data;
	u8				write_data;
	u8				reg;
	u8				val;
	u8				mask;
	muic_attached_dev_t		noti_dev;
} muic_cmd_data;

typedef struct max77833_muic_command_node {
	muic_cmd_data				cmd_data;
	struct max77833_muic_command_node	*next;
} muic_cmd_node;

typedef struct max77833_muic_command_node*	muic_cmd_node_p;

typedef struct max77833_muic_command_queue {
	struct mutex			command_mutex;
	muic_cmd_node			*front;
	muic_cmd_node			*rear;
	muic_cmd_node			tmp_cmd_node;
//	int				count;
} cmd_queue_t;

typedef struct max77833_muic_data muic_data_t;
/* muic chip specific internal data structure */
struct max77833_muic_data {
	struct device			*dev;
	struct i2c_client		*i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex			muic_mutex;
	struct mutex			reset_mutex;
	struct mutex			command_mutex;

	/* muic command data */
	cmd_queue_t			muic_cmd_queue;

	/* model dependant mfd platform data */
	struct max77833_platform_data	*mfd_pdata;

	int				irq_idres;
	int				irq_chgtyp;
//	int				irq_chgtyprun;
	int				irq_sysmsg;
	int				irq_apcmdres;

	/* model dependant muic platform data */
	struct muic_platform_data	*pdata;

	/* muic current attached device */
	muic_attached_dev_t		attached_dev;
	void				*attached_func;

	/* muic support vps list */
	bool muic_support_list[ATTACHED_DEV_NUM];

	bool				is_muic_ready;
	bool				is_muic_reset;

	u8				adcmode;

//	bool				ignore_adcerr;	// CHECK ME!!!

	/* check is otg test */
	bool				is_otg_test;

	/* muic HV charger */
	bool				is_factory_start;
	bool				is_check_hv;
	bool				is_charger_ready;

	u8				is_boot_dpdnvden;

	/* muic status value */
	u8				status1;
	u8				status2;
	u8				status3;
	u8                              status4;
	u8                              status5;
	u8                              status6;

};

/* max77833 muic register read/write related information defines. */

#define REG_NONE			0xff
#define REG_FULL_MASKING		0xff

/* MAX77833 REGISTER ENABLE or DISABLE bit */
enum max77833_reg_bit_control {
	MAX77833_DISABLE_BIT		= 0,
	MAX77833_ENABLE_BIT,
};

/* MAX77833 STATUS1 register */
#define STATUS1_IDRES_SHIFT             0
#define STATUS1_IDRES_MASK              (0xff << STATUS1_IDRES_SHIFT)

/* MAX77833 STATUS2 register */
#define STATUS2_CHGTYP_SHIFT            0
#define STATUS2_SPCHGTYP_SHIFT          3
#define STATUS2_CHGTYPRUN_SHIFT         7
#define STATUS2_CHGTYP_MASK             (0x7 << STATUS2_CHGTYP_SHIFT)
#define STATUS2_SPCHGTYP_MASK           (0x7 << STATUS2_SPCHGTYP_SHIFT)
#define STATUS2_CHGTYPRUN_MASK          (0x1 << STATUS2_CHGTYPRUN_SHIFT)

/* MAX77833 STATUS3 register - include ERROR message. */
#define STATUS3_SYSMSG_SHIFT		0
#define STATUS3_SYSMSG_MASK		(0xff << STATUS3_SYSMSG_SHIFT)

/* MAX77833 DAT_IN register */
#define DAT_IN_SHIFT			0
#define DAT_IN_MASK			(0xff << DAT_IN_SHIFT)

/* MAX77833 DAT_OUT register */
#define DAT_OUT_SHIFT			0
#define DAT_OUT_MASK			(0xff << DAT_OUT_SHIFT)

/* MAX77833 CONFIG COMMAND */
#define JIGSET_SHIFT			0
#define SFOUT_SHIFT			2
#define IDMONEN_SHIFT			6
#define CHGDETEN_SHIFT			7
#define JIGSET_MASK			(0x3 << JIGSET_SHIFT)
#define SFOUT_MASK			(0x3 << SFOUT_SHIFT)
#define IDMONEN_MASK			(0x1 << IDMONEN_SHIFT)
#define CHGDETEN_MASK			(0x1 << CHGDETEN_SHIFT)

/* MAX77833 SWITCH COMMAND */
#define COMN1SW_SHIFT                   0
#define COMP2SW_SHIFT                   3
#define RCPS_SHIFT			6
#define IDBEN_SHIFT                     7
#define COMN1SW_MASK                    (0x7 << COMN1SW_SHIFT)
#define COMP2SW_MASK                    (0x7 << COMP2SW_SHIFT)
#define RCPS_MASK			(0x1 << RCPS_SHIFT)
#define IDBEN_MASK                      (0x1 << IDBEN_SHIFT)
//#define CLEAR_IDBEN_RSVD_MASK           (COMN1SW_MASK | COMP2SW_MASK) // ??

/* MAX77833 ID Monitor Config */
#define MODE_SHIFT			2
#define MODE_MASK			(0x3 << MODE_SHIFT)

typedef enum {
	CHGDET_ENABLE		= 0xfc, 
	CHGDET_DISABLE		= 0x7c,
} chgdetcon_t;

typedef enum {
	CHGDETRUN_FALSE		= 0x00,
	CHGDETRUN_TRUE		= (0x1 << STATUS2_CHGTYPRUN_SHIFT),

	CHGDETRUN_DONTCARE	= 0xff,
} chgdetrun_t;

/* MAX77833 MUIC Charger Type Detection Output Value */
typedef enum {
	/* No Valid voltage at VB (Vvb < Vvbdet) */
	CHGTYP_NO_VOLTAGE		= 0x00,
	/* Unknown (D+/D- does not present a valid USB charger signature) */
	CHGTYP_USB			= 0x01,
	/* Charging Downstream Port */
	CHGTYP_CDP			= 0x02,
	/* Dedicated Charger (D+/D- shorted) */
	CHGTYP_DEDICATED_CHARGER	= 0x03,
	/* DCD Timeout, Open D+/D- */
	CHGTYP_TIMEOUT_OPEN		= 0x04,
	/* Abort Vbus present but Charge Detection halted */
	CHGTYP_HALT			= 0x05,
	/* Reserved for Future Use */
	CHGTYP_RFU_1                    = 0x06,
	CHGTYP_RFU_2                    = 0x07,

	/* Any charger w/o USB */
	CHGTYP_UNOFFICIAL_CHARGER	= 0xfc,
	/* Any charger type */
	CHGTYP_ANY			= 0xfd,
	/* Don't care charger type */
	CHGTYP_DONTCARE			= 0xfe,

	CHGTYP_MAX,
	CHGTYP_INIT,
	CHGTYP_MIN = CHGTYP_NO_VOLTAGE
} chgtyp_t;

/* MAX77833 MUIC Special Charger Type Detection Output value */
typedef enum {
	SPCHGTYP_UNKNOWN		= 0x00,
	SPCHGTYP_SAMSUNG_2A		= 0x01,
	SPCHGTYP_APPLE_500MA		= 0x02,
	SPCHGTYP_APPLE_1A		= 0x03,
	SPCHGTYP_APPLE_2A		= 0x04,
	SPCHGTYP_APPLE_12W		= 0x05,
	SPCHGTYP_GENERIC_500MA		= 0x06,
	SPCHGTYP_RFU			= 0x07,
} spchgtyp_t;

typedef enum {
	PROCESS_ATTACH		= 0,
	PROCESS_LOGICALLY_DETACH,
	PROCESS_NONE,
} process_t;

/* muic register value for COMN1, COMN2 in Switch command  */

/*
 * MAX77833 Switch command
 * ID Bypass [7] / Mic En [6] / D+ [5:3] / D- [2:0]
 * 0: ID Bypass Open / 1: IDB connect to UID
 * 0: Mic En Open / 1: Mic connect to VB
 * 111: Open / 001: USB / 010(enable),011(disable): Audio / 100: UART
 */
enum max77833_switch_command_val {
	MAX77833_MUIC_SWITCH_CMD_ID_OPEN	= 0x0,
	MAX77833_MUIC_SWITCH_CMD_ID_BYPASS	= 0x1,

	MAX77833_MUIC_SWITCH_CMD_RCPS_DIS	= 0x0,
	MAX77833_MUIC_SWITCH_CMD_RCPS_EN	= 0x1,

	MAX77833_MUIC_SWITCH_CMD_COM_USB	= 0x01,
	MAX77833_MUIC_SWITCH_CMD_COM_AUDIO_ON	= 0x02,
	MAX77833_MUIC_SWITCH_CMD_COM_AUDIO_OFF  = 0x03,
	MAX77833_MUIC_SWITCH_CMD_COM_UART	= 0x04,
	MAX77833_MUIC_SWITCH_CMD_COM_USB_CP	= 0x05,
	MAX77833_MUIC_SWITCH_CMD_COM_UART_CP	= 0x06,
	MAX77833_MUIC_SWITCH_CMD_COM_OPEN       = 0x07,
};

typedef enum {
	COM_OPEN	= (MAX77833_MUIC_SWITCH_CMD_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_RCPS_DIS << RCPS_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_OPEN << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_OPEN << COMN1SW_SHIFT),
	COM_USB		= (MAX77833_MUIC_SWITCH_CMD_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_RCPS_DIS << RCPS_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_USB << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_USB << COMN1SW_SHIFT),
#if 0
	COM_AUDIO	= (MAX77833_MUIC_SWITCH_CMD_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_RCPS_DIS << RCPS_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_AUDIO_ON << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_AUDIO_ON << COMN1SW_SHIFT),
#endif
	COM_UART	= (MAX77833_MUIC_SWITCH_CMD_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_RCPS_DIS << RCPS_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_UART << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_UART << COMN1SW_SHIFT),
	COM_USB_CP	= (MAX77833_MUIC_SWITCH_CMD_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_RCPS_DIS << RCPS_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_USB_CP << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_USB_CP << COMN1SW_SHIFT),
	COM_UART_CP	= (MAX77833_MUIC_SWITCH_CMD_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_RCPS_DIS << RCPS_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_UART_CP << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_UART_CP << COMN1SW_SHIFT),
	COM_USB_DOCK  = (MAX77833_MUIC_SWITCH_CMD_ID_OPEN << IDBEN_SHIFT) | \
/*			(MAX77833_MUIC_SWITCH_CMD_RCPS_DIS << RCPS_SHIFT) | \ */
			(MAX77833_MUIC_SWITCH_CMD_COM_USB << COMP2SW_SHIFT) | \
			(MAX77833_MUIC_SWITCH_CMD_COM_USB << COMN1SW_SHIFT),
} max77833_switch_cmd_t;

enum {
	MAX77833_MUIC_IDMODE_CONTINUOUS			= 0x3,
	MAX77833_MUIC_IDMODE_FACTORY_ONE_SHOT		= 0x2,
	MAX77833_MUIC_IDMODE_ONE_SHOT			= 0x1,
	MAX77833_MUIC_IDMODE_PULSE			= 0x0,

	MAX77833_MUIC_IDMODE_NONE			= 0xf,
};

extern struct device *switch_device;
extern void init_muic_cmd_data(muic_cmd_data *cmd_data);
extern void enqueue_muic_cmd(cmd_queue_t *muic_cmd_queue, muic_cmd_data cmd_data);

#endif /* __MAX77833_MUIC_H__ */

