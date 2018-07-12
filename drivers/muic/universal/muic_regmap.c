/*
 * muic_regmap.c
 *
 * Copyright (C) 2014 Samsung Electronics
 * Thomas Ryu <smilesr.ryu@samsung.com>
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
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>
#include <linux/string.h>

#include <linux/muic/muic.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include "muic-internal.h"
#include "muic_i2c.h"
#include "muic_regmap.h"

struct vendor_regmap {
	char *name; 
	void (*func)(struct regmap_desc **);
};

char *regmap_to_name(struct regmap_desc *pdesc, int addr)
{
	if (addr >= pdesc->size) {
		pr_err("%s out of addr range:%d\n", __func__, addr);
		return "NULL";
	}

	return pdesc->regmap[addr].name;
}

int regmap_write_value(struct regmap_desc *pdesc, int uattr, int value)
{
	struct i2c_client *i2c = pdesc->muic->i2c;
	struct reg_attr attr;
	int curr, result = 0;
	u8 reg_val;

	_REG_ATTR(&attr, uattr);

	if (pdesc->trace)
		pr_info("%s %s[%02x]:%02x<<%d, %02x\n", __func__,
			regmap_to_name(pdesc, attr.addr),
			attr.addr, attr.mask, attr.bitn, value);

	curr = muic_i2c_read_byte(i2c, attr.addr);
	if (curr < 0)
		goto i2c_read_error;

	if (uattr & _ATTR_OVERWRITE_M) {
		reg_val = value;
	} else {
		reg_val  = curr & ~(attr.mask << attr.bitn);
		reg_val	|= ((value & attr.mask) << attr.bitn);
	}

	if (reg_val ^ curr) {
		if (muic_i2c_guaranteed_wbyte(i2c, attr.addr, reg_val) < 0)
			goto i2c_write_error;

		result = muic_i2c_read_byte(i2c, attr.addr);
		if (result < 0)
			goto i2c_read_error;

		if (pdesc->trace)
			pr_info("  -%s done %02x+%02x->%02x(=%02x)\n",
				(uattr & _ATTR_OVERWRITE_M) ?
				"Overwrite" : "Update",
				curr, value, reg_val, result);
	} else {
		result = reg_val;

		if (pdesc->trace)
			pr_info("  -%s skip %02x+%02x->%02x(=%02x)\n",
				(uattr & _ATTR_OVERWRITE_M) ?
				"Overwrite" : "Update",
				curr, value, reg_val, result);
	}

	return result | ((curr << 8) & 0xff00);

i2c_write_error:
	pr_err("%s i2c write error.\n", __func__);
	return -1;

i2c_read_error:
	pr_err("%s i2c read error.\n", __func__);
	return -1;
}

/* read a shifed masked value */
int regmap_read_value(struct regmap_desc *pdesc, int uattr)
{
	struct reg_attr attr;
	int s_m_value = 0, curr;

	_REG_ATTR(&attr, uattr);
	curr = muic_i2c_read_byte(pdesc->muic->i2c, attr.addr);
	if (curr < 0)
		pr_err("%s err read %s(%d)\n", __func__,
			regmap_to_name(pdesc, attr.addr), curr);

	s_m_value = curr >> attr.bitn;
	s_m_value &= attr.mask;

	if (pdesc->trace)
		pr_info("%s %02x/%02x/%02x %02x->%02x\n", __func__,
			attr.addr, attr.mask, attr.bitn, curr, s_m_value);

	return s_m_value;
}

int regmap_read_raw_value(struct regmap_desc *pdesc, int uattr)
{
	struct reg_attr attr;
	int curr;

	_REG_ATTR(&attr, uattr);
	curr = muic_i2c_read_byte(pdesc->muic->i2c, attr.addr);
	if (curr < 0)
		pr_err("%s err read %s(%d)\n", __func__,
			regmap_to_name(pdesc, attr.addr), curr);

	if (pdesc->trace)
		pr_info("%s %02x/%02x/%02x %02x\n", __func__,
			attr.addr, attr.mask, attr.bitn, curr);

	return curr;
}

int regmap_com_to(struct regmap_desc *pdesc, int port)
{
	struct regmap_ops *pops = pdesc->regmapops;
	int uattr, ret;

	pops->ioctl(pdesc, GET_COM_VAL, &port, &uattr);
#if defined(CONFIG_MUIC_UNIVERSAL_MAX77849)
	 ret = regmap_write_value(pdesc, uattr, port);
#else
	uattr |= _ATTR_OVERWRITE_M;
	ret = regmap_write_value(pdesc, uattr, port);
#endif

	_REGMAP_TRACE(pdesc, 'w', ret, uattr, port);

	return ret;
}

int muic_reg_init(muic_data_t *pmuic)
{
	struct regmap_ops *pops = pmuic->regmapdesc->regmapops;

	return pops->init(pmuic->regmapdesc);
}

int set_int_mask(muic_data_t *pmuic, bool on)
{
	struct regmap_ops *pops = pmuic->regmapdesc->regmapops;
	int uattr, ret;

	pops->ioctl(pmuic->regmapdesc, GET_INT_MASK, NULL, &uattr);
	ret = regmap_write_value(pmuic->regmapdesc, uattr, on);

	_REGMAP_TRACE(pmuic->regmapdesc, 'w', ret, uattr, on);

	return ret;
}

static int muic_init_chip(struct regmap_desc *pdesc)
{
	regmap_t *preg = pdesc->regmap;
	int i;

	pr_info("%s\n", __func__);

	for (i = 0; i < pdesc->size; i++, preg++) {
		if (!preg->name)
			continue;

		if(preg->init == INIT_INT_CLR){
			pr_info(" [%02x] read. \n", i);
			muic_i2c_read_byte(pdesc->muic->i2c, i);
		} else if (preg->init != INIT_NONE) {
			pr_info(" [%02x] : 0x%02x\n", i, preg->init & 0xFF);
			if (muic_i2c_write_byte(pdesc->muic->i2c,
					i, preg->init) < 0)
				goto i2c_write_error;
		}
	}

	return 0;

i2c_write_error:
	pr_err("%s i2c write error.\n", __func__);
	return -1;
}

int muic_rev_info(struct regmap_desc *pdesc)
{
	struct regmap_ops *pops = pdesc->regmapops;
	muic_data_t *pmuic = pdesc->muic;
	struct reg_attr attr;
	int uattr;

	pops->ioctl(pmuic->regmapdesc, GET_REVISION, NULL, &uattr);
	_REG_ATTR(&attr, uattr);

	uattr = muic_i2c_read_byte(pmuic->i2c, attr.addr);
	if (uattr < 0) {
		pr_err("%s i2c io error(%d)\n",  __func__, uattr);
		return -ENODEV;
	} else {
		uattr &= (attr.mask << attr.bitn);
		pmuic->muic_vendor = (uattr & 0x7);
		pmuic->muic_version = ((uattr & 0xF8) >> 3);
		pr_info("%s [%s] vendor=0x%x, ver=0x%x\n",
			__func__, regmap_to_name(pdesc, attr.addr),
			pmuic->muic_vendor, pmuic->muic_version);
	}

	return 0;
}

static int muic_reset_chip(struct regmap_desc *pdesc)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	return 0;
}

static int muic_update_regmapdata(struct regmap_desc *pdesc, int size)
{
	muic_data_t *pmuic = pdesc->muic;
	regmap_t *preg = pdesc->regmap;
	int ret = 0, i;

	pr_info("%s REG_END:%d size:%d\n", __func__, pdesc->size, size);

	for (i = 0; i < pdesc->size; i++, preg++) {
		if (!preg->name)
			continue;

		ret = muic_i2c_read_byte(pmuic->i2c, i);
		if (ret < 0)
			pr_err("%s i2c read error(%d)\n", __func__, ret);
		else
			preg->curr = ret;
	}

	return 0;
}
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static int muic_show_regmapdata(struct regmap_desc *pdesc, regmap_t *p)
{
	return 0;
}
#else
static int muic_show_regmapdata(struct regmap_desc *pdesc, regmap_t *p)
{
	char buf[128];
	int i;

	memset(buf, 0x00, sizeof(buf));

	pr_info("\n");
	sprintf(buf, " %4s %16s %4s %4s %4s",
		"Addr", "Name", " Rst", "Curr", "Init");
	pr_info("%s\n", buf);
	pr_info("----------------------------------\n");
	for (i = 0; i < pdesc->size; i++, p++) {
		if (!p->name)
			continue;

		sprintf(buf, " 0x%02x %16s 0x%02x 0x%02x 0x%02x", i,
				p->name, p->reset, p->curr, p->init & 0xFF);
		pr_info("%s\n", buf);
	}

	pr_info("\n");

	return 0;
}
#endif

static int muic_update_regmap(struct regmap_desc *pdesc)
{
	return muic_update_regmapdata(pdesc,
			pdesc->regmapops->get_size());
}

static void muic_show_regmap(struct regmap_desc *pdesc)
{
	muic_show_regmapdata(pdesc, pdesc->regmap);
}

#if defined(CONFIG_MUIC_UNIVERSAL_SM5703)
extern void muic_register_sm5703_regmap_desc(struct regmap_desc **pdesc);
#endif

#if defined(CONFIG_MUIC_UNIVERSAL_S2MM001)
extern void muic_register_s2mm001_regmap_desc(struct regmap_desc **pdesc);
#endif

#if defined(CONFIG_MUIC_UNIVERSAL_MAX77849)
extern void muic_register_max77849_regmap_desc(struct regmap_desc **pdesc);
#endif

#if defined(CONFIG_MUIC_UNIVERSAL_MAX77854)
extern void muic_register_max77854_regmap_desc(struct regmap_desc **pdesc);
#endif

static struct vendor_regmap vendor_regmap_tbl[] = {
#if defined(CONFIG_MUIC_UNIVERSAL_SM5703)
	{"sm,sm5703", muic_register_sm5703_regmap_desc},
#endif
#if defined(CONFIG_MUIC_UNIVERSAL_S2MM001)
	{"lsi,s2mm001", muic_register_s2mm001_regmap_desc},
#endif
#if defined(CONFIG_MUIC_UNIVERSAL_MAX77849)
	{"max,max77849", muic_register_max77849_regmap_desc},
#endif
#if defined(CONFIG_MUIC_UNIVERSAL_MAX77854)
	{"max,max77854", muic_register_max77854_regmap_desc},
#endif
	{"", NULL},
};
void muic_register_regmap(struct regmap_desc **pdesc, void *pdata)
{
	struct regmap_desc *pdesc_temp = NULL;
	struct regmap_ops *pops;
	struct vendor_regmap *pvtbl = vendor_regmap_tbl;
	muic_data_t *pmuic = (muic_data_t *)pdata;
	int i;

	/* Get a chipset descriptor */
	pr_info("chip_name : %s\n",pmuic->chip_name);
	for (i = 0; pvtbl->name; i++, pvtbl++) {
		pr_info(" [%d] : %s\n", i, pvtbl->name);
		if (!strcmp(pvtbl->name, pmuic->chip_name)) {
			pvtbl->func(&pdesc_temp);
			break;
		}
	}
	if (i == sizeof(vendor_regmap_tbl)/sizeof(struct vendor_regmap)) {
		pr_info("%s: No matched regmap driver.\n", __func__);
	}

	pr_info("%s: %s registered.\n", __func__, pdesc_temp->name);

	*pdesc = pdesc_temp;
	pops = pdesc_temp->regmapops;

	/* Add generic functions */
	pops->init = muic_init_chip;
	pops->revision = muic_rev_info;
	pops->reset = muic_reset_chip;
	pops->show = muic_show_regmap;
	pops->update = muic_update_regmap;
}
