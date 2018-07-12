#ifndef _MUIC_VPS_H_
#define _MUIC_VPS_H_

/* MUIC Output of USB Charger Detection */
typedef enum {
	/* No Valid voltage at VB (Vvb < Vvbdet) */
	CHGTYP_NO_VOLTAGE		= 0x00,
	/* Unknown (D+/D- does not present a valid USB charger signature) */
	CHGTYP_USB			= 0x01,
	/* Charging Downstream Port */
	CHGTYP_CDP			= 0x02,
	/* Dedicated Charger (D+/D- shorted) */
	CHGTYP_DEDICATED_CHARGER	= 0x03,
	/* Special 500mA charger, max current 500mA */
	CHGTYP_500MA			= 0x04,
	/* Special 1A charger, max current 1A */
	CHGTYP_1A			= 0x05,
	/* Special charger - 3.3V bias on D+/D- */
	CHGTYP_SPECIAL_3_3V_CHARGER	= 0x06,
	/* Reserved */
	CHGTYP_RFU			= 0x07,
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

#define MDEV(name) ATTACHED_DEV_##name##_MUIC
/*
 * VPS attribute field.

   b'xxxxxxxx_xxxxxxx_xxxx_bdfs_cccc_vvvv
     x: undefined
     b: No battery charing if not supported (1: no charging, 0: charging)
     d: charger detection
     f: factory device
     s: supported
     c: com port
     v: vbus
*/
#define VPS_NOCHG_BITN 11
#define VPS_CHGDET_BITN 10
#define VPS_FAC_BITN 9
#define VPS_SUP_BITN 8
#define VPS_COM_BITN 4
#define VPS_VBUS_BITN 0
#define VPS_NOCHG_MASK 0x1
#define VPS_CHGDET_MASK 0x1
#define VPS_FAC_MASK 0x1
#define VPS_SUP_MASK 0x1
#define VPS_COM_MASK 0xF
#define VPS_VBUS_MASK 0xF

#define MATTR(com,vbus) \
	(com << VPS_COM_BITN) | \
	(vbus << VPS_VBUS_BITN)

#define MATTR_TO_VBUS(a) ((a >> VPS_VBUS_BITN) & VPS_VBUS_MASK)
#define MATTR_TO_COM(a) ((a >> VPS_COM_BITN) & VPS_COM_MASK)
#define MATTR_TO_FACT(a) ((a >> VPS_FAC_BITN) & VPS_FAC_MASK)
#define MATTR_TO_SUPP(a) ((a >> VPS_SUP_BITN) & VPS_SUP_MASK)
#define MATTR_TO_CDET(a) ((a >> VPS_CHGDET_BITN) & VPS_CHGDET_MASK)
#define MATTR_TO_NOCHG(a) ((a >> VPS_NOCHG_BITN) & VPS_NOCHG_MASK)
#define MATTR_NOCHG (1 << VPS_NOCHG_BITN)
#define MATTR_CDET (1 << VPS_CHGDET_BITN)
#define MATTR_SUPP (1 << VPS_SUP_BITN)
#define MATTR_FACT (1 << VPS_FAC_BITN)
#define MATTR_CDET_SUPP ((1 << VPS_CHGDET_BITN) | MATTR_SUPP)
#define MATTR_FACT_SUPP ((1 << VPS_FAC_BITN) | MATTR_SUPP)

enum vps_vbvolt{
	VB_LOW	= 0,
	VB_HIGH	= 1,
	VB_CHK	= 2,
	VB_ANY	= 3,
};

enum vps_com{
	VCOM_OPEN	= COM_OPEN_WITH_V_BUS,
	VCOM_USB	= COM_USB_AP,
	VCOM_AUDIO	= COM_AUDIO,
	VCOM_UART	= COM_UART_AP,
	VCOM_USB_CP	= COM_USB_CP,
	VCOM_UART_CP	= COM_UART_CP,
};

struct vps_cfg {
	char *name;
	int attr;
};

struct vps_tbl_data {
	u8 adc;
	char *rid;
	struct vps_cfg *cfg;
};
extern bool vps_name_to_mdev(const char *name, int *sdev);
extern void vps_update_supported_attr(muic_attached_dev_t mdev, bool supported);
extern bool vps_is_supported_dev(muic_attached_dev_t mdev);
extern int vps_find_attached_dev(muic_data_t *pmuic, muic_attached_dev_t *pdev, int *pintr);
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static inline void vps_show_table(void){}
#else
extern void vps_show_table(void);
#endif
extern void vps_show_supported_list(void);
extern int vps_resolve_dev(muic_data_t *pmuic, muic_attached_dev_t *pbuf, int *pintr);
extern bool vps_is_hv_ta(vps_data_t *pvps);

#endif
