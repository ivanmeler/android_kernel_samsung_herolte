/*
 * driver/ccic/ccic_misc.h - S2MM005 CCIC MISC driver
 *
 * Copyright (C) 2017 Samsung Electronics
 * Author: Wookwang Lee <wookwang.lee@samsung.com>
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
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 */

enum uvdm_data_type {
	TYPE_SHORT = 0,
	TYPE_LONG,
};

enum uvdm_direction_type {
	DIR_OUT = 0,
	DIR_IN,
};

struct uvdm_data {
	unsigned short pid; /* Product ID */
	char type; /* uvdm_data_type */
	char dir; /* uvdm_direction_type */
	unsigned int size; /* data size */
	void __user *pData; /* data pointer */
};

struct ccic_misc_dev {
	struct uvdm_data u_data;
	atomic_t open_excl;
	atomic_t ioctl_excl;
	int (*uvdm_write)(void *data, int size);
	int (*uvdm_read)(void *data, int size);
};

extern ssize_t samsung_uvdm_out_request_message(void *data, size_t size);
extern int samsung_uvdm_in_request_message(void *data);
extern int samsung_uvdm_ready(void);
extern void samsung_uvdm_close(void);
