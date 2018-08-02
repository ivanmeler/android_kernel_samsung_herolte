/* sec_bsp.h
 *
 * Copyright (C) 2014 Samsung Electronics
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

#ifndef SEC_BSP_H
#define SEC_BSP_H

extern bool init_command_debug;

extern void sec_boot_stat_get_start_kernel(void);
extern void sec_boot_stat_get_mct(u32);
extern void sec_boot_stat_add_initcall(const char *);
extern void sec_boot_stat_add(const char *);
extern void get_cpuinfo_cur_freq(int *, int *);
extern void get_exynos_thermal_curr_temp(int *, int);
extern void sec_boot_stat_add_init_command(const char *name,
	int duration);

#endif
