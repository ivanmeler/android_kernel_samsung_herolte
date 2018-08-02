/*
 *  wacom_i2c_firm.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/kernel.h>
#include "wacom.h"

unsigned char *fw_data;
bool ums_binary;
extern unsigned int system_rev;

const unsigned char mpu_type = 0x2C;

unsigned int fw_ver_file = 0x0000;
char fw_chksum[] = { 0x1F, 0x2d, 0x83, 0xed, 0xec};

#if 0
void wacom_i2c_set_firm_data(struct wacom_i2c *wac_i2c)
{
	if (wac_i2c->fw_path == FW_IN_SDCARD)
		fw_data = (unsigned char *)wac_i2c->fw_data;
	else
		fw_data = (unsigned char *)wac_i2c->firm_data->data;

	ums_binary = true;
}
#endif

/*Return digitizer type according to board rev*/
int wacom_i2c_get_digitizer_type(void)
{
#if defined(CONFIG_V1A) || defined(CONFIG_CHAGALL)
	return EPEN_DTYPE_B878;
#elif defined(CONFIG_HA)
	//return EPEN_DTYPE_B968;
	//if (system_rev >= WACOM_DTYPE_B968_HWID)
	//	return EPEN_DTYPE_B968;
	//else
	//	return EPEN_DTYPE_B934;
#endif
	return 0;
}

void wacom_i2c_init_firm_data(void)
{
	int type;
	type = wacom_i2c_get_digitizer_type();

#if defined(CONFIG_V1A) || defined(CONFIG_CHAGALL)
	if (type == EPEN_DTYPE_B878) {
		printk(KERN_DEBUG
			"%s epen:Digitizer type is B878\n", SECLOG);
	}
#elif defined(CONFIG_HA)
	/*if (likely(type == EPEN_DTYPE_B968)) {
		printk(KERN_DEBUG
			"epen:Digitizer type is B968\n");
	} else if (type == EPEN_DTYPE_B934) {
		printk(KERN_DEBUG
			"epen:Digitizer type is B934\n");
		fw_name = "epen/W9010_B934.bin";
		fw_ver_file = 0x0076;
		memcpy(fw_chksum, B934_chksum,
			sizeof(fw_chksum));
	}*/
#endif
}
