#ifndef _MUIC_REGMAP_
#define _MUIC_REGMAP_

#define _MASK0 0x00
#define _MASK1 0x01
#define _MASK2 0x03
#define _MASK3 0x07
#define _MASK4 0x0f
#define _MASK5 0x1f
#define _MASK6 0x3f
#define _MASK7 0x7f
#define _MASK8 0xff
#define MASK(n) _MASK##n

#define _BIT0 0
#define _BIT1 1
#define _BIT2 2
#define _BIT3 3
#define _BIT4 4
#define _BIT5 5
#define _BIT6 6
#define _BIT7 7

#define BITN(nr) (nr)

#define INIT_NONE (-1)
#define INIT_INT_CLR (-2)

#define _REGMAP_TRACE(d, rw, r, a, v) do { \
	pr_info("  -muic_regmap_trace %c %02x=[%02x:%02x+%08x]%s\n", \
			rw, r&0xff, (r>>8)&0xff, v, a, \
			regmap_to_name(d, _ATTR_ADDR(a))); \
} while (0)


struct reg_attr;

#define _REG_ATTR(x, y) do { \
	((struct reg_attr *)x)->value = (y>>24) & 0xff; \
	((struct reg_attr *)x)->bitn = (y>>16) & 0x0f; \
	((struct reg_attr *)x)->mask = (y>>8)  & 0xff; \
	((struct reg_attr *)x)->addr = y & 0xff; \
} while (0)

#define _ATTR_VALUE(x) ((x>>24) & 0xff)
#define _ATTR_BITP(x) ((x>>16) & 0x0f)
#define _ATTR_ATTR(x) ((x>>20) & 0x0f)
#define _ATTR_MASK(x) ((x>>8) & 0xff)
#define _ATTR_ADDR(x) (x & 0xff)

#define _ATTR_VALUE_BITP 24
/*
 * xxxo
 * o : update(0) or overwirte(1)
 */

#define _ATTR_OVERWRITE_M   (1 << 20)

struct reg_attr {
	u8 value;
	u8 bitn;
	u8 mask;
	u8 addr;
};

typedef struct _regmap_t {
	char *name;
	u8 reset;
	u8 curr;
	int init;
} regmap_t;


struct regmap_desc;

struct regmap_ops {
	int (*get_size)(void);
	int (*init)(struct regmap_desc  *);
	int (*revision)(struct regmap_desc  *);
	int (*reset)(struct regmap_desc  *);
	void (*show)(struct regmap_desc  *);
	int (*update)(struct regmap_desc  *);
	int (*ioctl)(struct regmap_desc *pdesc, int arg1, int *arg2, int *arg3);
	void (*get_formatted_dump)(struct regmap_desc  *pdesc, char *mesg);
};

struct vendor_ops {
	int (*attach_ta)(struct regmap_desc  *);
	int (*detach_ta)(struct regmap_desc  *);
	int (*get_switch)(struct regmap_desc  *);
	void (*set_switch)(struct regmap_desc  *, int);
	void (*set_adc_scan_mode)(struct regmap_desc  *, int);
	int (*get_adc_scan_mode)(struct regmap_desc  *);
	int (*set_rustproof)(struct regmap_desc  *, int);
	int (*get_vps_data)(struct regmap_desc *, void *);
	int (*enable_accdet)(struct regmap_desc *, int);
	int (*enable_chgdet)(struct regmap_desc *, int);
	int (*run_chgdet)(struct regmap_desc *, bool);
	int (*start_otg_test)(struct regmap_desc *, int);	
	int (*attach_mmdock)(struct regmap_desc  *, int);
	int (*detach_mmdock)(struct regmap_desc  *);
	int (*get_vbus_value)(struct regmap_desc *, int);
};

struct regmap_desc {
	const char *name;
	regmap_t *regmap;
	int size;
	int trace;
	struct regmap_ops *regmapops;
	struct vendor_ops *vendorops;
	muic_data_t *muic;
};

extern char *regmap_to_name(struct regmap_desc *pdesc, int);
extern int regmap_com_to(struct regmap_desc *pdesc, int);
extern int muic_reg_init(muic_data_t *pmuic);
extern int set_int_mask(muic_data_t *pmuic, bool);
extern int set_manual_sw(muic_data_t *pmuic, bool);
extern int regmap_read_value(struct regmap_desc *pdesc, int);
extern int regmap_read_raw_value(struct regmap_desc *pdesc, int);
extern int regmap_write_value(struct regmap_desc *pdesc, int, int);
extern void muic_register_regmap(struct regmap_desc **pdesc, void *);
#endif
