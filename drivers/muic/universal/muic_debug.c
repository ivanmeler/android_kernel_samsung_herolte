/*
 * sm5703-muic.c - SM5703 micro USB switch device driver
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

#include <linux/muic/muic.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include "muic-internal.h"
#include "muic_debug.h"
#include "muic_i2c.h"
#include "muic_regmap.h"

#define MAX_LOG 25

extern bool muic_is_online(void);
static u8 muic_log_cnt;
static u8 muic_log[MAX_LOG][3];

void muic_reg_log(u8 reg, u8 value, u8 rw)
{
	muic_log[muic_log_cnt][0]=reg;
	muic_log[muic_log_cnt][1]=value;
	muic_log[muic_log_cnt][2]=rw;
	muic_log_cnt++;
	if(muic_log_cnt >= MAX_LOG) muic_log_cnt = 0;
}
void muic_print_reg_log(void)
{
	int i;
	u8 reg, value, rw;
	char mesg[256]="";

	for( i = 0 ; i < MAX_LOG ; i++ )
	{
		reg = muic_log[muic_log_cnt][0];
		value = muic_log[muic_log_cnt][1];
		rw = muic_log[muic_log_cnt][2];
		muic_log_cnt++;

		if(muic_log_cnt >= MAX_LOG) muic_log_cnt = 0;
		sprintf(mesg+strlen(mesg),"%x(%x)%x ", reg, value, rw);
	}
	pr_info("%s:%s\n", __func__, mesg);
}
void muic_read_reg_dump(muic_data_t *pmuic, char *mesg)
{
	struct regmap_ops *pops = pmuic->regmapdesc->regmapops;

	pops->get_formatted_dump(pmuic->regmapdesc, mesg);
}
void muic_print_reg_dump(muic_data_t *pmuic)
{
	char mesg[256]="";

	muic_read_reg_dump(pmuic, mesg);

	pr_info("%s:%s\n", __func__, mesg);
}


void muic_show_debug_info(struct work_struct *work)
{
	muic_data_t *pmuic;

	if (!muic_is_online()) {
		pr_err("%s: muic is offline\n", __func__);
		return;
	}

	pmuic = container_of(work, muic_data_t, usb_work.work);

	mutex_lock(&pmuic->muic_mutex);
	muic_print_reg_log();
	muic_print_reg_dump(pmuic);
	mutex_unlock(&pmuic->muic_mutex);

	schedule_delayed_work(&pmuic->usb_work, msecs_to_jiffies(60000));
}
