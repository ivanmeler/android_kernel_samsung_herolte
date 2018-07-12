/* linux/drivers/video/exynos_decon/panel/s6e3hf2_wqhd_aid_dimming.h
 *
 * Header file for Samsung AID Dimming Driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __S6E3FA2_WQHD_AID_DIMMING_H__
#define __S6E3FA2_WQHD_AID_DIMMING_H__


static signed char rtbl2nit[10] = {0, 50, 50, 47, 46, 42, 32, 23, 12, 0};
static signed char rtbl3nit[10] = {0, 46, 42, 38, 37, 36, 27, 19, 9, 0};
static signed char rtbl4nit[10] = {0, 39, 37, 34, 32, 30, 24, 17, 8, 0};
static signed char rtbl5nit[10] = {0, 39, 36, 33, 31, 29, 22, 15, 8, 0};
static signed char rtbl6nit[10] = {0, 33, 31, 29, 27, 25, 20, 14, 7, 0};
static signed char rtbl7nit[10] = {0, 32, 29, 27, 25, 23, 18, 12, 7, 0};
static signed char rtbl8nit[10] = {0, 30, 27, 25, 23, 20, 16, 12, 7, 0};
static signed char rtbl9nit[10] = {0, 27, 26, 24, 22, 19, 15, 11, 7, 0};
static signed char rtbl10nit[10] = {0, 26, 25, 23, 21, 18, 15, 11, 6, 0};
static signed char rtbl11nit[10] = {0, 24, 23, 21, 19, 17, 12, 10, 6, 0};
static signed char rtbl12nit[10] = {0, 26, 22, 20, 18, 16, 12, 10, 6, 0};
static signed char rtbl13nit[10] = {0, 22, 21, 19, 17, 15, 11, 9, 5, 0};
static signed char rtbl14nit[10] = {0, 21, 20, 18, 16, 14, 10, 8, 5, 0};
static signed char rtbl15nit[10] = {0, 21, 18, 16, 14, 14, 10, 8, 5, 0};
static signed char rtbl16nit[10] = {0, 19, 17, 16, 14, 13, 10, 8, 5, 0};
static signed char rtbl17nit[10] = {0, 19, 16, 15, 13, 12, 9, 7, 4, 0};
static signed char rtbl19nit[10] = {0, 19, 16, 14, 12, 11, 8, 7, 4, 0};
static signed char rtbl20nit[10] = {0, 17, 15, 13, 11, 10, 7, 6, 4, 0};
static signed char rtbl21nit[10] = {0, 17, 14, 13, 11, 10, 7, 7, 4, 0};
static signed char rtbl22nit[10] = {0, 16, 14, 13, 11, 10, 7, 7, 4, 0};
static signed char rtbl24nit[10] = {0, 17, 14, 12, 10, 10, 7, 7, 4, 0};
static signed char rtbl25nit[10] = {0, 16, 14, 12, 10, 9, 6, 6, 4, 0};
static signed char rtbl27nit[10] = {0, 14, 13, 11, 10, 9, 6, 6, 4, 0};
static signed char rtbl29nit[10] = {0, 13, 12, 10, 9, 8, 6, 6, 4, 0};
static signed char rtbl30nit[10] = {0, 12, 12, 10, 9, 8, 6, 6, 4, 0};
static signed char rtbl32nit[10] = {0, 13, 11, 9, 8, 7, 6, 6, 4, 0};
static signed char rtbl34nit[10] = {0, 11, 10, 8, 7, 7, 5, 5, 4, 0};
static signed char rtbl37nit[10] = {0, 11, 9, 8, 7, 6, 5, 5, 3, 0};
static signed char rtbl39nit[10] = {0, 12, 9, 8, 6, 6, 4, 5, 3, 0};
static signed char rtbl41nit[10] = {0, 11, 9, 8, 6, 6, 4, 5, 3, 0};
static signed char rtbl44nit[10] = {0, 9, 8, 7, 6, 6, 5, 5, 3, 0};
static signed char rtbl47nit[10] = {0, 8, 9, 7, 6, 5, 3, 5, 3, 0};
static signed char rtbl50nit[10] = {0, 10, 8, 6, 5, 5, 3, 5, 3, 0};
static signed char rtbl53nit[10] = {0, 10, 7, 6, 5, 4, 4, 5, 3, 0};
static signed char rtbl56nit[10] = {0, 8, 8, 6, 5, 5, 3, 5, 3, 0};
static signed char rtbl60nit[10] = {0, 9, 6, 5, 5, 4, 3, 5, 3, 0};
static signed char rtbl64nit[10] = {0, 7, 7, 5, 5, 4, 4, 4, 3, 0};
static signed char rtbl68nit[10] = {0, 7, 7, 5, 4, 4, 4, 5, 3, 0};
static signed char rtbl72nit[10] = {0, 7, 7, 5, 5, 4, 4, 3, 3, 0};
static signed char rtbl77nit[10] = {0, 7, 6, 4, 4, 4, 4, 5, 2, 0};
static signed char rtbl82nit[10] = {0, 10, 7, 5, 5, 4, 5, 5, 3, 0};
static signed char rtbl87nit[10] = {0, 5, 6, 4, 5, 4, 4, 5, 3, 0};
static signed char rtbl93nit[10] = {0, 3, 6, 4, 4, 4, 3, 5, 2, 0};
static signed char rtbl98nit[10] = {0, 6, 7, 5, 5, 5, 5, 6, 2, 0};
static signed char rtbl105nit[10] = {0, 5, 6, 4, 4, 4, 4, 4, 2, 0};
static signed char rtbl111nit[10] = {0, 3, 5, 4, 3, 4, 5, 6, 3, 0};
static signed char rtbl119nit[10] = {0, 5, 5, 4, 5, 4, 4, 5, 3, 0};
static signed char rtbl126nit[10] = {0, 4, 5, 4, 4, 4, 5, 5, 2, 0};
static signed char rtbl134nit[10] = {0, 2, 5, 3, 4, 4, 4, 5, 2, 0};
static signed char rtbl143nit[10] = {0, 3, 5, 4, 3, 4, 4, 4, 2, 0};
static signed char rtbl152nit[10] = {0, 2, 5, 3, 4, 4, 5, 5, 2, 0};
static signed char rtbl162nit[10] = {0, 2, 3, 3, 3, 3, 3, 5, 3, 0};
static signed char rtbl172nit[10] = {0, 0, 3, 2, 2, 2, 3, 5, 3, 0};
static signed char rtbl183nit[10] = {0, 4, 3, 2, 3, 3, 4, 4, 1, 0};
static signed char rtbl195nit[10] = {0, 0, 3, 2, 2, 2, 3, 4, 1, 0};
static signed char rtbl207nit[10] = {0, 0, 3, 1, 2, 2, 3, 3, 1, 0};
static signed char rtbl220nit[10] = {0, 0, 2, 1, 2, 1, 2, 2, 1, 0};
static signed char rtbl234nit[10] = {0, 0, 2, 1, 1, 1, 1, 2, 1, 0};
static signed char rtbl249nit[10] = {0, 0, 2, 1, 1, 2, 3, 4, 3, 3};
static signed char rtbl265nit[10] = {0, 0, 2, 1, 1, 1, 1, 3, 2, 1};
static signed char rtbl282nit[10] = {0, 0, 1, 1, 1, 1, 1, 2, 1, 1};
static signed char rtbl300nit[10] = {0, 0, 1, 0, 0, 0, 0, 2, 1, 0};
static signed char rtbl316nit[10] = {0, 1, 1, 1, 1, 1, 2, 2, 2, 2};
static signed char rtbl333nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1};
static signed char rtbl350nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};



static signed char ctbl2nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -5, -5, 0, -6, -5, 1, -10, -10, 2, -15, -11, 1, -9, -6, 0, -7, -5, 0, -5, -6, 0, -8};
static signed char ctbl3nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -6, -5, 0, -7, -6, 2, -10, -11, 2, -12, -10, 1, -9, -5, 0, -7, -3, 0, -4, -3, 0, -4};
static signed char ctbl4nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -6, -6, 1, -8, -7, 1, -10, -10, 2, -10, -9, 0, -9, -5, 0, -5, -3, 0, -4, -2, 0, -3};
static signed char ctbl5nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -9, -6, 0, -11, -9, 0, -9, -10, 0, -11, -9, 0, -9, -5, 0, -5, -3, 0, -3, -1, 0, -2};
static signed char ctbl6nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -9, -5, 1, -8, -9, 0, -10, -9, 1, -9, -7, 0, -7, -5, 0, -5, -2, 0, -3, -1, 0, -1};
static signed char ctbl7nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -9, -7, 1, -9, -9, 1, -10, -9, 0, -9, -7, 0, -7, -3, 0, -4, -3, 0, -2, 0, 0, -1};
static signed char ctbl8nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -13, -7, 1, -7, -8, 0, -10, -8, 0, -9, -6, 0, -7, -4, 0, -3, -2, 0, -3, 0, 0, 0};
static signed char ctbl9nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -12, -8, 0, -8, -8, 0, -9, -8, 0, -8, -5, 0, -6, -3, 0, -4, -3, 0, -2, 0, 0, 0};
static signed char ctbl10nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -13, -7, 0, -7, -8, 0, -9, -8, 0, -8, -5, 0, -6, -4, 0, -4, -2, 0, -2, 0, 0, 0};
static signed char ctbl11nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -15, -7, 0, -7, -8, 0, -8, -8, 0, -8, -4, 0, -4, -2, 0, -3, -2, 0, -2, 0, 0, 0};
static signed char ctbl12nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -17, -7, 0, -7, -8, 0, -8, -8, 0, -8, -3, 0, -4, -3, 0, -3, -2, 0, -2, 1, 0, 1};
static signed char ctbl13nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -15, -7, 0, -7, -9, 0, -9, -7, 0, -8, -4, 0, -3, -2, 0, -2, -2, 0, -2, 1, 0, 1};
static signed char ctbl14nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -15, -7, 0, -7, -9, 0, -9, -7, 0, -7, -2, 0, -3, -2, 0, -2, -2, 0, -2, 1, 0, 1};
static signed char ctbl15nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -14, -7, 0, -9, -7, 1, -7, -6, 0, -7, -3, 0, -3, -2, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl16nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -16, -5, 1, -6, -7, 1, -7, -7, 0, -7, -3, 0, -3, -1, 0, -2, -2, 0, -1, 1, 0, 1};
static signed char ctbl17nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -16, -7, 1, -7, -7, 1, -7, -6, 0, -7, -2, 0, -2, -2, 0, -3, -1, 0, -1, 1, 0, 1};
static signed char ctbl19nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -15, -7, 0, -9, -7, 0, -7, -6, 0, -7, -1, 0, 0, -2, 0, -3, -1, 0, -1, 1, 0, 1};
static signed char ctbl20nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -18, -4, 1, -6, -7, 0, -8, -6, 0, -5, -2, 0, -2, -2, 0, -2, 0, 0, -1, 0, 0, 0};
static signed char ctbl21nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -16, -5, 1, -7, -6, 1, -6, -5, 0, -5, -1, 0, -2, -2, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl22nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -16, -5, 1, -7, -6, 0, -6, -6, 0, -6, -1, 0, -1, -1, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl24nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -17, -6, 0, -8, -7, 0, -7, -5, 0, -5, -1, 0, -1, -1, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl25nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -19, -6, 0, -8, -6, 0, -6, -5, 0, -6, -1, 0, -1, -1, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl27nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -17, -5, 0, -8, -6, 0, -6, -4, 0, -5, -1, 0, -1, -2, 0, -2, 0, 0, -1, 1, 0, 1};
static signed char ctbl29nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -15, -4, 0, -7, -5, 0, -6, -5, 0, -6, -1, 0, 0, -1, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl30nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -17, -5, 0, -7, -5, 0, -6, -4, 0, -5, -1, 0, 0, -1, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl32nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -17, -4, 0, -6, -5, 0, -7, -4, 0, -4, 0, 0, 0, -1, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl34nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -16, -4, 0, -7, -4, 0, -6, -4, 0, -3, 0, 0, 1, -1, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl37nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -16, -3, 0, -6, -4, 0, -5, -3, 0, -3, 0, 0, 1, -2, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl39nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -17, -4, 0, -6, -4, 0, -7, -3, 0, -4, 1, 0, 1, -2, 0, -1, -1, 0, -1, 1, 0, 1};
static signed char ctbl41nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -16, -3, 0, -5, -4, 0, -7, -3, 0, -4, 1, 0, 2, -2, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl44nit[30] = {0, 0, 0, 0, 0, 0,-2, 4, -15, -3, 0, -6, -3, 1, -5, -3, 0, -3, 1, 0, 1, -2, 0, -1, -1, 0, -1, 1, 0, 1};
static signed char ctbl47nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -16, -3, 0, -5, -3, 0, -6, -4, 0, -4, 2, 0, 1, -2, 0, -1, -1, 0, -1, 1, 0, 1};
static signed char ctbl50nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -17, -2, 0, -6, -3, 0, -6, -3, 0, -3, 1, 0, 1, -1, 0, -1, -1, 0, -1, 1, 0, 1};
static signed char ctbl53nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -17, -1, 0, -5, -2, 0, -6, -3, 0, -2, 1, 0, 1, -1, 0, -1, -1, 0, -1, 1, 0, 1};
static signed char ctbl56nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -15, -2, 0, -5, -2, 0, -6, -3, 0, -3, 2, 0, 2, -2, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl60nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -15, -1, 0, -5, -2, 0, -6, -2, 0, -2, 2, 0, 2, -2, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl64nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -15, -1, 0, -4, -1, 0, -6, -2, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char ctbl68nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -15, -1, 0, -4, -2, 0, -6, -2, 0, -1, 2, 0, 1, -2, 0, -2, -1, 0, 0, 1, 0, 1};
static signed char ctbl72nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -16, -2, 0, -4, -3, 0, -6, -1, 0, -1, 0, 0, 1, -1, 0, -2, 0, 0, 0, 0, 0, 0};
static signed char ctbl77nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -15, -1, 0, -4, -2, 0, -5, -2, 0, -1, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char ctbl82nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -13, -2, 0, -5, -3, 0, -5, 0, 0, -1, 0, 0, 0, -2, 0, -2, 0, 0, 0, 0, 0, 0};
static signed char ctbl87nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -12, -2, 0, -6, -2, 0, -3, -2, 0, -1, 0, 0, 1, -1, 0, -2, 0, 0, 0, 1, 0, 1};
static signed char ctbl93nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -10, -1, 1, -4, -2, 0, -4, -1, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 1};
static signed char ctbl98nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -12, -2, 0, -5, -3, 0, -4, 0, 0, -1, 0, 0, 1, -2, 0, -2, -1, 0, -1, 0, 0, 1};
static signed char ctbl105nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -12, -2, 0, -4, -3, 0, -4, 0, 0, -1, 1, 0, 1, -2, 0, -2, 0, 0, 0, 1, 0, 1};
static signed char ctbl111nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -10, 0, 1, -3, -2, 0, -3, 0, 0, 1, 0, 0, 0, -1, 0, -1, -1, 0, 0, 1, 0, 1};
static signed char ctbl119nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -10, -1, 0, -5, -3, 0, -2, 0, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, -1, 1, 0, 2};
static signed char ctbl126nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -11, -1, 0, -4, -1, 0, -1, 1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char ctbl134nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -9, -1, 0, -5, -3, 0, -2, 0, 0, -1, -1, 0, 0, -1, 0, -2, 0, 0, 0, 0, 0, 1};
static signed char ctbl143nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -10, -2, 0, -4, -2, 0, -2, 1, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char ctbl152nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -9, -2, 0, -5, -2, 0, -1, 0, 0, 0, -1, 0, -1, -2, 0, -1, 1, 0, 0, -1, 0, 1};
static signed char ctbl162nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -8, -1, 0, -4, -2, 0, -2, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char ctbl172nit[30] = {0, 0, 0, 0, 0, 0,0, 2, -8, 0, 0, -4, -2, 0, -2, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char ctbl183nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -10, 0, 0, -4, -1, 0, -1, 1, 0, 1, 0, 0, 1, -1, 0, -1, 1, 0, 0, 0, 0, 0};
static signed char ctbl195nit[30] = {0, 0, 0, 0, 0, 0,-2, 0, -9, 0, 0, -4, -2, 0, -1, 1, 0, 1, 0, 0, 1, 0, 0, -2, 1, 0, 1, -1, 0, 0};
static signed char ctbl207nit[30] = {0, 0, 0, 0, 0, 0,-2, 0, -7, 1, 0, -4, -2, 0, -1, 1, 0, 1, -1, 0, 0, -1, 0, -1, 1, 0, 0, -1, 0, 0};
static signed char ctbl220nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, -7, 2, 0, -3, -2, 0, -3, 2, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0};
static signed char ctbl234nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -6, 0, 0, -4, -1, 0, -1, 2, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0};
static signed char ctbl249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl350nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char aid9793[5] = {0xB2, 0x07, 0x68, 0x07, 0x68};
static signed char aid9711[5] = {0xB2, 0x07, 0x58, 0x07, 0x58};
static signed char aid9633[5] = {0xB2, 0x07, 0x49, 0x07, 0x49};
static signed char aid9551[5] = {0xB2, 0x07, 0x39, 0x07, 0x39};
static signed char aid9468[5] = {0xB2, 0x07, 0x29, 0x07, 0x29};
static signed char aid9385[5] = {0xB2, 0x07, 0x19, 0x07, 0x19};
static signed char aid9303[5] = {0xB2, 0x07, 0x09, 0x07, 0x09};
static signed char aid9215[5] = {0xB2, 0x06, 0xF8, 0x06, 0xF8};
static signed char aid9127[5] = {0xB2, 0x06, 0xE7, 0x06, 0xE7};
static signed char aid9044[5] = {0xB2, 0x06, 0xD7, 0x06, 0xD7};
static signed char aid8967[5] = {0xB2, 0x06, 0xC8, 0x06, 0xC8};
static signed char aid8879[5] = {0xB2, 0x06, 0xB7, 0x06, 0xB7};
static signed char aid8786[5] = {0xB2, 0x06, 0xA5, 0x06, 0xA5};
static signed char aid8704[5] = {0xB2, 0x06, 0x95, 0x06, 0x95};
static signed char aid8611[5] = {0xB2, 0x06, 0x83, 0x06, 0x83};
static signed char aid8528[5] = {0xB2, 0x06, 0x73, 0x06, 0x73};
static signed char aid8363[5] = {0xB2, 0x06, 0x53, 0x06, 0x53};
static signed char aid8177[5] = {0xB2, 0x06, 0x2F, 0x06, 0x2F};
static signed char aid8135[5] = {0xB2, 0x06, 0x27, 0x06, 0x27};
static signed char aid8053[5] = {0xB2, 0x06, 0x17, 0x06, 0x17};
static signed char aid7929[5] = {0xB2, 0x05, 0xFF, 0x05, 0xFF};
static signed char aid7846[5] = {0xB2, 0x05, 0xEF, 0x05, 0xEF};
static signed char aid7665[5] = {0xB2, 0x05, 0xCC, 0x05, 0xCC};
static signed char aid7495[5] = {0xB2, 0x05, 0xAB, 0x05, 0xAB};
static signed char aid7397[5] = {0xB2, 0x05, 0x98, 0x05, 0x98};
static signed char aid7221[5] = {0xB2, 0x05, 0x76, 0x05, 0x76};
static signed char aid7040[5] = {0xB2, 0x05, 0x53, 0x05, 0x53};
static signed char aid6761[5] = {0xB2, 0x05, 0x1D, 0x05, 0x1D};
static signed char aid6586[5] = {0xB2, 0x04, 0xFB, 0x04, 0xFB};
static signed char aid6374[5] = {0xB2, 0x04, 0xD2, 0x04, 0xD2};
static signed char aid6105[5] = {0xB2, 0x04, 0x9E, 0x04, 0x9E};
static signed char aid5795[5] = {0xB2, 0x04, 0x62, 0x04, 0x62};
static signed char aid5511[5] = {0xB2, 0x04, 0x2B, 0x04, 0x2B};
static signed char aid5191[5] = {0xB2, 0x03, 0xED, 0x03, 0xED};
static signed char aid4861[5] = {0xB2, 0x03, 0xAD, 0x03, 0xAD};
static signed char aid4411[5] = {0xB2, 0x03, 0x56, 0x03, 0x56};
static signed char aid4179[5] = {0xB2, 0x03, 0x29, 0x03, 0x29};
static signed char aid3626[5] = {0xB2, 0x02, 0xBE, 0x02, 0xBE};
static signed char aid3177[5] = {0xB2, 0x02, 0x67, 0x02, 0x67};
static signed char aid2794[5] = {0xB2, 0x02, 0x1D, 0x02, 0x1D};
static signed char aid2299[5] = {0xB2, 0x01, 0xBD, 0x01, 0xBD};
static signed char aid1725[5] = {0xB2, 0x01, 0x4E, 0x01, 0x4E};
static signed char aid1105[5] = {0xB2, 0x00, 0xD6, 0x00, 0xD6};
static signed char aid0429[5] = {0xB2, 0x00, 0x53, 0x00, 0x53};
static signed char aid0072[5] = {0xB2, 0x00, 0x0E, 0x00, 0x0E};

static unsigned char elv10 [3] = {0xB6, 0x98, 0x10};
static unsigned char elv0F [3] = {0xB6, 0x98, 0x0F};
static unsigned char elv0E [3] = {0xB6, 0x98, 0x0E};
static unsigned char elv0D [3] = {0xB6, 0x98, 0x0D};
static unsigned char elv0C [3] = {0xB6, 0x98, 0x0C};
static unsigned char elv0B [3] = {0xB6, 0x98, 0x0B};
static unsigned char elv0A [3] = {0xB6, 0x98, 0x0A};
static unsigned char elv09 [3] = {0xB6, 0x98, 0x09};
static unsigned char elv08 [3] = {0xB6, 0x98, 0x08};
static unsigned char elv07 [3] = {0xB6, 0x98, 0x07};
static unsigned char elv06 [3] = {0xB6, 0x98, 0x06};
static unsigned char elv05 [3] = {0xB6, 0x98, 0x05};
static unsigned char elv04 [3] = {0xB6, 0x98, 0x04};


#if 0
static unsigned char elvAcl10 [3] = {0xB6, 0x88, 0x10};
static unsigned char elvAcl0F [3] = {0xB6, 0x88, 0x0F};
static unsigned char elvAcl0E [3] = {0xB6, 0x88, 0x0E};
static unsigned char elvAcl0D [3] = {0xB6, 0x88, 0x0D};
static unsigned char elvAcl0C [3] = {0xB6, 0x88, 0x0C};
static unsigned char elvAcl0B [3] = {0xB6, 0x88, 0x0B};
static unsigned char elvAcl0A [3] = {0xB6, 0x88, 0x0A};
static unsigned char elvAcl09 [3] = {0xB6, 0x88, 0x09};
#endif                                                           
static unsigned char elvAcl08 [3] = {0xB6, 0x88, 0x08};
#if 0 
static unsigned char elvAcl07 [3] = {0xB6, 0x88, 0x07};
static unsigned char elvAcl06 [3] = {0xB6, 0x88, 0x06};
static unsigned char elvAcl05 [3] = {0xB6, 0x88, 0x05};
static unsigned char elvAcl04 [3] = {0xB6, 0x88, 0x04};
#endif
                                    
#endif
