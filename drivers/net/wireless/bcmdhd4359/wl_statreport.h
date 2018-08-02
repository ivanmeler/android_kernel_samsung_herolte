/*
* Wifi dongle status logging and report
*
* Copyright (C) 1999-2018, Broadcom Corporation
* 
*      Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2 (the "GPL"),
* available at http://www.broadcom.com/licenses/GPLv2.php, with the
* following added to such license:
* 
*      As a special exception, the copyright holders of this software give you
* permission to link this software with independent modules, and to copy and
* distribute the resulting executable under terms of your choice, provided that
* you also meet, for each linked independent module, the terms and conditions of
* the license of that module.  An independent module is a module which is not
* derived from this software.  The special exception does not apply to any
* modifications of the software.
* 
*      Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*
* <<Broadcom-WL-IPTag/Open:>>
*
* $Id: wl_statreport.h 707595 2017-06-28 08:28:30Z $
*/
#ifndef wl_stat_report_h
#define wl_stat_report_h

/* WSR module functions */
int wl_attach_stat_report(void *cfg);
void wl_detach_stat_report(void *cfg);
void wl_stat_report_gather(void *cfg, void *cnt);
void wl_stat_report_notify_connected(void *cfg);

/* Private command functions */
int wl_android_stat_report_get_start(void *net, char *cmd, int tot_len);
int wl_android_stat_report_get_next(void *net, char *cmd, int tot_len);

/* File Save functions */
void wl_stat_report_file_save(void *dhdp, void *fp);
#endif
