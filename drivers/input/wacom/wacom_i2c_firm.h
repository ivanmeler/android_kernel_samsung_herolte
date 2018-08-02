/*
 *  wacom_i2c_firm.h - Wacom G5 Digitizer Controller (I2C bus)
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

#ifndef _LINUX_WACOM_I2C_FIRM_H
#define _LINUX_WACOM_I2C_FIRM_H

extern const unsigned char mpu_type;
extern unsigned int fw_ver_file;
extern unsigned char *fw_data;
extern bool ums_binary;
extern char fw_chksum[];
void wacom_i2c_init_firm_data(void);
int wacom_i2c_get_digitizer_type(void);

#endif /* _LINUX_WACOM_I2C_FIRM_H */