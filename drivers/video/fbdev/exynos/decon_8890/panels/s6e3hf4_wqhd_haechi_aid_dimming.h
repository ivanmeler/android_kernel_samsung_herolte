/* linux/drivers/video/exynos_decon/panel/s6e3ha2_wqhd_aid_dimming.h
*
* Header file for Samsung aid Dimming Driver.
*
* Copyright (c) 2013 Samsung Electronics
* Minwoo Kim <minwoo7945.kim@samsung.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef __S6E3HA2_WQHD_AID_DIMMING_H__
#define __S6E3HA2_WQHD_AID_DIMMING_H__
/// hf4

static signed char rtbl2nit[10] = {0, 16, 22, 25, 31, 28, 22, 14, 8, 0};
static signed char rtbl3nit[10] = {0, 17, 21, 29, 26, 23, 19, 12, 7, 0};
static signed char rtbl4nit[10] = {0, 18, 21, 27, 23, 20, 17, 11, 7, 0};
static signed char rtbl5nit[10] = {0, 18, 22, 26, 23, 19, 17, 10, 6, 0};
static signed char rtbl6nit[10] = {0, 22, 28, 25, 21, 18, 15, 10, 6, 0};
static signed char rtbl7nit[10] = {0, 18, 25, 21, 18, 16, 14, 9, 6, 0};
static signed char rtbl8nit[10] = {0, 19, 22, 21, 18, 15, 13, 7, 4, 0};
static signed char rtbl9nit[10] = {0, 21, 22, 19, 16, 14, 12, 6, 4, 0};
static signed char rtbl10nit[10] = {0, 20, 23, 19, 15, 13, 11, 6, 4, 0};
static signed char rtbl11nit[10] = {0, 16, 20, 17, 14, 12, 10, 5, 4, 0};
static signed char rtbl12nit[10] = {0, 18, 21, 17, 14, 12, 10, 5, 4, 0};
static signed char rtbl13nit[10] = {0, 22, 20, 16, 13, 11, 9, 5, 4, 0};
static signed char rtbl14nit[10] = {0, 20, 20, 16, 13, 11, 9, 4, 3, 0};
static signed char rtbl15nit[10] = {0, 19, 18, 14, 11, 10, 8, 5, 4, 0};
static signed char rtbl16nit[10] = {0, 20, 18, 14, 11, 10, 8, 4, 4, 0};
static signed char rtbl17nit[10] = {0, 20, 18, 14, 11, 9, 7, 4, 4, 0};
static signed char rtbl19nit[10] = {0, 18, 16, 12, 9, 8, 7, 4, 4, 0};
static signed char rtbl20nit[10] = {0, 17, 15, 11, 9, 7, 6, 4, 4, 0};
static signed char rtbl21nit[10] = {0, 16, 14, 10, 8, 7, 6, 4, 3, 0};
static signed char rtbl22nit[10] = {0, 16, 14, 10, 8, 7, 6, 3, 3, 0};
static signed char rtbl24nit[10] = {0, 15, 13, 9, 7, 6, 6, 3, 3, 0};
static signed char rtbl25nit[10] = {0, 15, 13, 9, 7, 6, 6, 3, 4, 0};
static signed char rtbl27nit[10] = {0, 14, 12, 8, 6, 5, 5, 3, 3, 0};
static signed char rtbl29nit[10] = {0, 14, 12, 8, 6, 5, 4, 3, 3, 0};
static signed char rtbl30nit[10] = {0, 14, 12, 8, 6, 5, 5, 2, 3, 0};
static signed char rtbl32nit[10] = {0, 13, 11, 7, 5, 5, 5, 2, 3, 0};
static signed char rtbl34nit[10] = {0, 13, 11, 7, 5, 4, 4, 2, 3, 0};
static signed char rtbl37nit[10] = {0, 11, 9, 6, 4, 3, 3, 2, 2, 0};
static signed char rtbl39nit[10] = {0, 10, 8, 5, 3, 3, 3, 1, 2, 0};
static signed char rtbl41nit[10] = {0, 10, 8, 5, 3, 3, 3, 1, 2, 0};
static signed char rtbl44nit[10] = {0, 10, 8, 5, 3, 3, 3, 1, 2, 0};
static signed char rtbl47nit[10] = {0, 10, 8, 5, 3, 3, 3, 1, 3, 0};
static signed char rtbl50nit[10] = {0, 9, 7, 4, 3, 3, 3, 1, 2, 0};
static signed char rtbl53nit[10] = {0, 8, 6, 3, 2, 2, 2, 0, 1, 0};
static signed char rtbl56nit[10] = {0, 8, 6, 4, 2, 2, 1, 0, 1, 0};
static signed char rtbl60nit[10] = {0, 7, 5, 3, 1, 1, 1, 0, 1, 0};
static signed char rtbl64nit[10] = {0, 6, 4, 2, 1, 1, 1, 0, 2, 0};
static signed char rtbl68nit[10] = {0, 6, 4, 2, 1, 2, 2, 2, 3, 0};
static signed char rtbl72nit[10] = {0, 7, 5, 3, 1, 2, 3, 2, 3, 0};
static signed char rtbl77nit[10] = {0, 6, 5, 3, 2, 2, 2, 4, 3, 0};
static signed char rtbl82nit[10] = {0, 6, 5, 3, 2, 2, 3, 3, 3, 0};
static signed char rtbl87nit[10] = {0, 6, 5, 3, 2, 2, 3, 2, 2, 0};
static signed char rtbl93nit[10] = {0, 6, 4, 2, 1, 1, 3, 2, 2, 0};
static signed char rtbl98nit[10] = {0, 7, 5, 3, 2, 3, 4, 5, 3, 0};
static signed char rtbl105nit[10] = {0, 7, 5, 3, 3, 3, 4, 4, 3, 0};
static signed char rtbl111nit[10] = {0, 7, 5, 3, 2, 2, 3, 3, 0, 0};
static signed char rtbl119nit[10] = {0, 6, 4, 2, 2, 3, 4, 5, 2, 0};
static signed char rtbl126nit[10] = {0, 5, 4, 2, 2, 3, 4, 4, 2, 0};
static signed char rtbl134nit[10] = {0, 5, 4, 2, 2, 3, 4, 4, 2, 0};
static signed char rtbl143nit[10] = {0, 5, 4, 2, 2, 3, 3, 3, 2, 0};
static signed char rtbl152nit[10] = {0, 5, 4, 2, 2, 3, 4, 5, 2, 0};
static signed char rtbl162nit[10] = {0, 5, 3, 2, 2, 2, 3, 4, 2, 0};
static signed char rtbl172nit[10] = {0, 5, 3, 2, 2, 2, 4, 5, 3, 0};
static signed char rtbl183nit[10] = {0, 5, 3, 1, 2, 2, 4, 5, 2, 0};
static signed char rtbl195nit[10] = {0, 4, 2, 1, 2, 3, 5, 5, 3, 0};
static signed char rtbl207nit[10] = {0, 4, 2, 1, 2, 3, 4, 5, 0, 0};
static signed char rtbl220nit[10] = {0, 4, 2, 1, 2, 2, 4, 4, 3, 0};
static signed char rtbl234nit[10] = {0, 4, 2, 1, 1, 2, 3, 4, 0, 0};
static signed char rtbl249nit[10] = {0, 2, 1, 0, 0, 1, 2, 3, 0, 0};
static signed char rtbl265nit[10] = {0, 1, 1, 0, 0, 0, 1, 2, 1, 0};
static signed char rtbl282nit[10] = {0, 1, 0, 0, 0, 0, 1, 2, 1, 0};
static signed char rtbl300nit[10] = {0, 1, 0, 0, 0, 0, 0, 2, 1, 0};
static signed char rtbl316nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char rtbl333nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char rtbl350nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char rtbl357nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char rtbl365nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char rtbl372nit[10] = {0, 0, 0, 0, 0, 0, 1, 2, 1, 0};
static signed char rtbl380nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char rtbl387nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char rtbl395nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char rtbl403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char rtbl412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char ctbl2nit[30] = {0, 0, 0, -4, -1, -17, -11, -1, -1, -4, 1, -3, -7, 0, -9, -15, 1, -12, -13, 0, -8, -5, 0, -5, -4, 0, -3, -7, 3, -5};
static signed char ctbl3nit[30] = {0, 0, 0, -4, 1, -17, -11, 1, 1, -4, 1, -9, -7, 1, -8, -12, 0, -12, -10, 1, -7, -4, 0, -5, -3, 0, -2, -5, 2, -4};
static signed char ctbl4nit[30] = {0, 0, 0, -4, 1, -16, -18, 0, 5, -9, 0, -12, -8, 2, -8, -14, 1, -11, -9, 0, -7, -4, 0, -4, -3, 0, -3, -4, 1, -3};
static signed char ctbl5nit[30] = {0, 0, 0, -9, 1, -19, -21, 3, 3, -9, 1, -12, -11, 0, -10, -12, 0, -11, -9, 0, -8, -3, 0, -4, -3, 0, -3, -3, 1, -3};
static signed char ctbl6nit[30] = {0, 0, 0, -9, 1, -18, -31, 0, -16, -11, 0, -13, -7, 1, -9, -12, 0, -12, -8, 0, -7, -3, 0, -4, -2, 0, -2, -2, 1, -2};
static signed char ctbl7nit[30] = {0, 0, 0, -8, 3, -17, -26, 2, -13, -9, 2, -12, -6, 2, -8, -10, 1, -10, -6, 1, -5, -2, 0, -3, -3, 0, -3, -1, 1, -1};
static signed char ctbl8nit[30] = {0, 0, 0, -13, 0, -20, -34, -1, -15, -9, 1, -12, -8, 0, -11, -8, 1, -9, -8, 0, -7, -2, 0, -3, -2, 0, -2, 0, 1, 0};
static signed char ctbl9nit[30] = {0, 0, 0, -9, 0, -19, -11, -1, -15, -9, 1, -12, -5, 1, -10, -8, 0, -8, -6, 0, -6, -2, 0, -3, -1, 0, -1, 0, 1, 0};
static signed char ctbl10nit[30] = {0, 0, 0, -64, 0, 10, -11, 0, -16, -12, 0, -13, -6, 0, -10, -7, 0, -10, -6, 0, -5, -2, 0, -3, -1, 0, -1, 0, 0, 0};
static signed char ctbl11nit[30] = {0, 0, 0, -60, 3, 16, -11, 2, -16, -7, 3, -11, -6, 0, -11, -7, 1, -8, -5, 0, -5, -1, 0, -2, -1, 0, -1, 0, 0, 0};
static signed char ctbl12nit[30] = {0, 0, 0, -64, 2, 12, -12, 2, -17, -7, 2, -11, -7, 0, -11, -7, 0, -9, -5, 0, -5, -1, 0, -2, 0, 0, -1, 0, 0, 1};
static signed char ctbl13nit[30] = {0, 0, 0, -22, 0, -3, -12, 0, -19, -9, 2, -12, -5, 0, -10, -7, 1, -8, -4, 0, -4, -1, 0, -2, -1, 0, -1, 1, 0, 1};
static signed char ctbl14nit[30] = {0, 0, 0, -11, 1, -2, -12, 0, -19, -8, 1, -13, -6, 0, -11, -7, 0, -8, -5, 0, -5, -1, 0, -2, -1, 0, -1, 1, 0, 2};
static signed char ctbl15nit[30] = {0, 0, 0, -11, 2, -2, -15, 0, -20, -8, 2, -13, -3, 2, -7, -6, 1, -7, -4, 1, -4, -1, 0, -2, 0, 0, -1, 0, 0, 1};
static signed char ctbl16nit[30] = {0, 0, 0, -11, 1, -3, -17, 0, -22, -7, 0, -14, -4, 1, -8, -6, 1, -7, -5, 0, -5, 0, 0, -1, 0, 0, -1, 0, 0, 1};
static signed char ctbl17nit[30] = {0, 0, 0, -14, 0, -6, -17, 0, -21, -9, 0, -15, -7, 0, -9, -5, 0, -8, -4, 0, -3, 0, 0, -1, 0, 0, -1, 0, 0, 1};
static signed char ctbl19nit[30] = {0, 0, 0, -16, 3, -4, -12, -1, -20, -10, 0, -15, -4, 2, -6, -4, 2, -6, -3, 0, -3, 0, 0, -1, -1, 0, -1, 1, 0, 1};
static signed char ctbl20nit[30] = {0, 0, 0, -10, 3, -4, -14, 0, -21, -6, 2, -13, -5, 0, -7, -4, 1, -6, -3, 1, -2, 0, 0, -1, -1, 0, -2, 1, 0, 2};
static signed char ctbl21nit[30] = {0, 0, 0, -8, 2, -4, -12, 0, -19, -7, 1, -14, -3, 2, -4, -4, 1, -6, -2, 1, -2, -1, 0, -2, -1, 0, 0, 1, -1, 1};
static signed char ctbl22nit[30] = {0, 0, 0, -8, 1, -4, -13, 0, -20, -7, 2, -13, -4, 1, -5, -3, 1, -6, -4, 0, -3, 0, 0, -1, -1, 0, 0, 1, -1, 1};
static signed char ctbl24nit[30] = {0, 0, 0, -7, 2, -4, -11, 1, -20, -9, 1, -14, -2, 1, -4, -3, 2, -5, -4, 0, -3, 0, 0, -1, 0, 0, 0, 0, -1, 1};
static signed char ctbl25nit[30] = {0, 0, 0, -4, 1, -5, -12, 0, -21, -9, 0, -15, -3, 1, -4, -2, 2, -4, -4, 0, -3, 1, 0, 0, -1, 0, -2, 1, -1, 2};
static signed char ctbl27nit[30] = {0, 0, 0, -5, 1, -5, -11, 0, -21, -7, 2, -12, -3, 1, -4, -2, 2, -4, -2, 0, -1, 0, 0, -1, -1, 0, -1, 1, -1, 2};
static signed char ctbl29nit[30] = {0, 0, 0, -7, 2, -5, -13, 0, -23, -8, 0, -13, -2, 1, -3, -3, 0, -5, -2, 1, -1, 0, 0, -1, -1, 0, -1, 1, -1, 2};
static signed char ctbl30nit[30] = {0, 0, 0, -4, 0, -6, -12, -1, -22, -8, 0, -13, -3, 1, -4, -2, 1, -4, -3, 0, -2, 0, 0, -1, -1, 0, 0, 1, -1, 1};
static signed char ctbl32nit[30] = {0, 0, 0, -8, 2, -6, -12, -1, -23, -7, 2, -10, -1, 2, -2, -3, 1, -5, -2, 0, -2, 0, 0, 0, -1, 0, -1, 1, -1, 2};
static signed char ctbl34nit[30] = {0, 0, 0, -8, 2, -6, -14, -1, -24, -8, 0, -12, -3, 1, -3, -1, 1, -3, -2, 0, -2, 0, 0, 0, -1, 0, 0, 1, -1, 1};
static signed char ctbl37nit[30] = {0, 0, 0, -6, 2, -6, -10, 3, -19, -6, 0, -10, -3, 1, -3, 0, 2, -2, 0, 0, 0, 0, 0, -1, -1, 0, 0, 1, -1, 2};
static signed char ctbl39nit[30] = {0, 0, 0, -5, 1, -7, -9, 3, -20, -4, 2, -8, -1, 2, -1, 0, 2, -2, -1, 1, 0, 1, 0, 0, -1, 0, 0, 1, -1, 1};
static signed char ctbl41nit[30] = {0, 0, 0, -6, 2, -7, -9, 3, -21, -5, 1, -8, -1, 2, -1, 0, 1, -2, -1, 1, 0, 1, 0, 0, -1, 0, -1, 1, -1, 2};
static signed char ctbl44nit[30] = {0, 0, 0, -7, 2, -8, -10, 2, -22, -6, 0, -9, -1, 1, -1, 0, 1, -2, -1, 1, -1, 1, 0, 1, -1, 0, 0, 1, -1, 1};
static signed char ctbl47nit[30] = {0, 0, 0, -6, 2, -8, -11, 1, -23, -6, 0, -9, -2, 0, -2, 0, 1, -2, -1, 0, 0, 1, 0, -1, -1, 0, 1, 1, -1, 1};
static signed char ctbl50nit[30] = {0, 0, 0, -6, 1, -9, -12, 0, -24, -2, 3, -4, -1, 1, -1, -1, 0, -3, -1, 0, 0, 0, 0, -1, 0, 0, 1, 1, -1, 1};
static signed char ctbl53nit[30] = {0, 0, 0, -7, 1, -9, -13, 0, -23, -1, 3, -3, -2, 0, -2, 2, 2, 0, -1, 0, -1, 0, 0, 1, 0, 0, 0, 2, 0, 3};
static signed char ctbl56nit[30] = {0, 0, 0, -7, 2, -8, -8, 4, -19, -6, -1, -7, -2, -1, -2, 0, 0, -2, -1, 0, -1, 0, 0, 1, 1, 1, 1, 2, 0, 3};
static signed char ctbl60nit[30] = {0, 0, 0, -9, 2, -9, -8, 2, -20, -6, -1, -8, -1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, -1, 3};
static signed char ctbl64nit[30] = {0, 0, 0, -5, 2, -7, -9, 4, -18, -1, 3, -3, 0, 1, 1, 2, 2, 0, -1, 0, -1, 1, 0, 2, 0, 0, 0, 1, -1, 2};
static signed char ctbl68nit[30] = {0, 0, 0, -5, 2, -7, -10, 2, -21, -1, 2, -3, 3, 3, 3, 3, 2, 1, -1, 0, -1, 1, 0, 1, -1, 0, -1, 1, -1, 2};
static signed char ctbl72nit[30] = {0, 0, 0, -6, 0, -10, -12, 0, -22, -2, 1, -3, 1, 2, 2, 2, 1, 0, -1, 0, 0, 1, 0, 0, -1, 0, 0, 1, -1, 0};
static signed char ctbl77nit[30] = {0, 0, 0, -7, 1, -9, -8, 4, -17, -4, 0, -5, 1, 1, 1, 0, 0, -1, -1, 0, 0, 0, 0, 1, 0, 0, 0, 1, -1, 1};
static signed char ctbl82nit[30] = {0, 0, 0, -9, 1, -11, -9, 3, -17, 0, 2, -2, -1, 0, 0, 0, 0, -2, 1, 1, 2, 0, 0, 0, -1, 0, -1, 1, -1, 1};
static signed char ctbl87nit[30] = {0, 0, 0, -7, 0, -11, -9, 2, -17, -1, 2, -2, 2, 2, 3, 1, 1, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, -1, 1};
static signed char ctbl93nit[30] = {0, 0, 0, -9, 0, -12, -11, -1, -19, 0, 2, -2, 3, 2, 3, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, -1, 1, 0, 2};
static signed char ctbl98nit[30] = {0, 0, 0, -8, -1, -12, -12, 0, -19, -1, 2, -2, 1, 0, 1, 2, 2, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0};
static signed char ctbl105nit[30] = {0, 0, 0, -9, 0, -12, -8, 3, -15, -3, 0, -4, 0, 0, 0, 0, 0, -1, 1, 1, 1, 0, 0, 1, -1, 0, -1, 1, -2, 1};
static signed char ctbl111nit[30] = {0, 0, 0, -9, -1, -13, -9, 2, -15, -2, 0, -4, -1, 0, 0, 2, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, -1, 1};
static signed char ctbl119nit[30] = {0, 0, 0, -4, 3, -9, -11, 0, -16, 1, 3, 0, 1, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, -1, -1, 0, 1, 2, -1, 1};
static signed char ctbl126nit[30] = {0, 0, 0, -5, 3, -9, -12, -1, -17, 1, 3, 0, 3, 2, 2, 0, 0, -1, 1, 1, 2, 0, 0, 0, -1, 0, -1, 1, -1, 1};
static signed char ctbl134nit[30] = {0, 0, 0, -5, 2, -10, -6, 3, -11, -2, 0, -3, 3, 2, 2, 1, 1, 1, -1, 0, 0, 0, 0, 0, -1, 0, 0, 2, -1, 2};
static signed char ctbl143nit[30] = {0, 0, 0, -6, 3, -9, -9, 0, -13, 1, 3, 0, 1, 0, 0, 1, 0, -1, 0, 0, 1, 0, 0, 0, -1, 0, 1, 2, -1, 0};
static signed char ctbl152nit[30] = {0, 0, 0, -6, 1, -11, -9, 0, -12, 2, 3, 0, 2, 2, 2, 1, 0, 0, 0, 0, 1, 0, 0, -2, 0, 0, 2, 1, -1, 0};
static signed char ctbl162nit[30] = {0, 0, 0, -8, 1, -11, -5, 3, -8, -2, 0, -3, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 2, -1, 1};
static signed char ctbl172nit[30] = {0, 0, 0, -8, 0, -12, -8, 2, -9, -2, -1, -3, 2, 1, 2, 1, 1, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 1};
static signed char ctbl183nit[30] = {0, 0, 0, -8, 0, -12, -7, 1, -9, 2, 3, 1, 1, 0, 1, 1, 2, 1, 1, 1, 1, 0, 0, 0, -1, 0, 0, 1, 0, 1};
static signed char ctbl195nit[30] = {0, 0, 0, -8, 1, -11, -3, 4, -5, 2, 3, 1, 2, 1, 2, 1, 1, 1, 0, 0, 0, -1, 0, 0, 0, 0, -1, 1, -1, 1};
static signed char ctbl207nit[30] = {0, 0, 0, -7, 0, -11, -4, 3, -6, 2, 2, 1, 3, 2, 3, -1, 0, -1, 1, 0, 1, -1, 0, 0, 0, 0, -1, 1, -1, 1};
static signed char ctbl220nit[30] = {0, 0, 0, -9, 0, -12, -4, 2, -6, 2, 2, 1, 0, 0, 0, 2, 1, 2, 0, 0, -1, 0, 0, 0, -1, 0, -1, 1, -1, 1};
static signed char ctbl234nit[30] = {0, 0, 0, -9, 0, -12, -5, 1, -6, -1, -1, -3, 2, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char ctbl249nit[30] = {0, 0, 0, -8, -1, -10, -4, 1, -5, 3, 3, 1, 3, 2, 3, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 2, 1, 0, 0};
static signed char ctbl265nit[30] = {0, 0, 0, -2, 2, -4, -5, 0, -6, 3, 3, 2, 3, 2, 3, 2, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1};
static signed char ctbl282nit[30] = {0, 0, 0, -5, 1, -5, -1, 3, -1, 0, 0, -1, 3, 1, 2, 0, 0, 0, 1, 1, 1, 0, 0, 1, -1, 0, 0, 1, 0, 2};
static signed char ctbl300nit[30] = {0, 0, 0, -5, 0, -5, -3, 1, -2, 4, 3, 2, 1, 0, 1, -2, -2, -2, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1};
static signed char ctbl316nit[30] = {0, 0, 0, -7, -1, -5, 0, 4, 1, -2, -1, -2, 3, 1, 3, -2, -1, -1, 1, 0, 0, -1, 0, 0, 0, 1, 1, 1, 0, 1};
static signed char ctbl333nit[30] = {0, 0, 0, -1, 3, 0, 0, 4, 1, 0, 0, 0, 1, 0, 1, -1, -1, -1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, 0};
static signed char ctbl350nit[30] = {0, 0, 0, -4, 3, -1, -4, 0, -2, -1, 0, -1, 1, 0, 1, 0, -1, 0, 0, 1, 0, -1, 0, 0, 1, 0, 1, -1, 0, 0};
static signed char ctbl357nit[30] = {0, 0, 0, -5, 1, -3, -4, -1, -2, -2, 0, -1, 2, 0, 1, -1, -1, 0, -1, 0, -1, 1, 1, 1, 0, 0, 0, -1, -1, 0};
static signed char ctbl365nit[30] = {0, 0, 0, -4, 2, -2, -4, -1, -2, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 2, -2, -1, -2};
static signed char ctbl372nit[30] = {0, 0, 0, -6, 1, -3, -4, -1, -2, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, -1, 0};
static signed char ctbl380nit[30] = {0, 0, 0, -6, 0, -3, -4, -1, -2, -1, 0, -1, 1, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -2, -1, -1};
static signed char ctbl387nit[30] = {0, 0, 0, -6, 1, -2, -5, -2, -3, -2, 0, -1, 2, 0, 1, 1, 1, 2, -1, 0, -1, -1, 0, -1, 1, 0, 2, -2, -1, -2};
static signed char ctbl395nit[30] = {0, 0, 0, -7, 1, -3, -3, -1, -1, -2, 0, -2, 1, -1, 1, 0, 1, 0, -1, -1, -1, 1, 1, 1, 0, 0, 1, -1, 0, 0};
static signed char ctbl403nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl412nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl420nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char aid9799[3] = {0xB1, 0x90, 0xE4};
static signed char aid9717[3] = {0xB1, 0x90, 0xCF};
static signed char aid9601[3] = {0xB1, 0x90, 0xB1};
static signed char aid9485[3] = {0xB1, 0x90, 0x93};
static signed char aid9365[3] = {0xB1, 0x90, 0x74};
static signed char aid9288[3] = {0xB1, 0x90, 0x60};
static signed char aid9180[3] = {0xB1, 0x90, 0x44};
static signed char aid9102[3] = {0xB1, 0x90, 0x30};
static signed char aid8986[3] = {0xB1, 0x90, 0x12};
static signed char aid8909[3] = {0xB1, 0x80, 0xFE};
static signed char aid8808[3] = {0xB1, 0x80, 0xE4};
static signed char aid8735[3] = {0xB1, 0x80, 0xD1};
static signed char aid8622[3] = {0xB1, 0x80, 0xB4};
static signed char aid8541[3] = {0xB1, 0x80, 0x9F};
static signed char aid8425[3] = {0xB1, 0x80, 0x81};
static signed char aid8313[3] = {0xB1, 0x80, 0x64};
static signed char aid8158[3] = {0xB1, 0x80, 0x3C};
static signed char aid8042[3] = {0xB1, 0x80, 0x1E};
static signed char aid7937[3] = {0xB1, 0x80, 0x03};
static signed char aid7821[3] = {0xB1, 0x70, 0xE5};
static signed char aid7616[3] = {0xB1, 0x70, 0xB0};
static signed char aid7546[3] = {0xB1, 0x70, 0x9E};
static signed char aid7314[3] = {0xB1, 0x70, 0x62};
static signed char aid7125[3] = {0xB1, 0x70, 0x31};
static signed char aid7005[3] = {0xB1, 0x70, 0x12};
static signed char aid6811[3] = {0xB1, 0x60, 0xE0};
static signed char aid6622[3] = {0xB1, 0x60, 0xAF};
static signed char aid6273[3] = {0xB1, 0x60, 0x55};
static signed char aid6072[3] = {0xB1, 0x60, 0x21};
static signed char aid5878[3] = {0xB1, 0x50, 0xEF};
static signed char aid5573[3] = {0xB1, 0x50, 0xA0};
static signed char aid5279[3] = {0xB1, 0x50, 0x54};
static signed char aid4954[3] = {0xB1, 0x50, 0x00};
static signed char aid4652[3] = {0xB1, 0x40, 0xB2};
static signed char aid4412[3] = {0xB1, 0x40, 0x74};
static signed char aid3905[3] = {0xB1, 0x30, 0xF1};
static signed char aid3464[3] = {0xB1, 0x30, 0x7F};
static signed char aid3336[3] = {0xB1, 0x30, 0x5E};
static signed char aid3289[3] = {0xB1, 0x30, 0x52};
static signed char aid2848[3] = {0xB1, 0x20, 0xE0};
static signed char aid2349[3] = {0xB1, 0x20, 0x5F};
static signed char aid1753[3] = {0xB1, 0x10, 0xC5};
static signed char aid1262[3] = {0xB1, 0x10, 0x46};
static signed char aid1118[3] = {0xB1, 0x10, 0x21};
static signed char aid1002[3] = {0xB1, 0x10, 0x03};
static signed char aid805[3] = {0xB1, 0x00, 0xD0};
static signed char aid681[3] = {0xB1, 0x00, 0xB0};
static signed char aid441[3] = {0xB1, 0x00, 0x72};
static signed char aid294[3] = {0xB1, 0x00, 0x4C};
static signed char aid46[3] = {0xB1, 0x00, 0x0C};


static unsigned char elv1[3] = {
	0xAC, 0x14
};
static unsigned char elv2[3] = {
	0xAC, 0x13
};
static unsigned char elv3[3] = {
	0xAC, 0x12
};
static unsigned char elv4[3] = {
	0xAC, 0x11
};
static unsigned char elv5[3] = {
	0xAC, 0x10
};
static unsigned char elv6[3] = {
	0xAC, 0x0F
};
static unsigned char elv7[3] = {
	0xAC, 0x0E
};
static unsigned char elv8[3] = {
	0xAC, 0x0D
};
static unsigned char elv9[3] = {
	0xAC, 0x0C
};
static unsigned char elv10[3] = {
	0xAC, 0x0B
};
static unsigned char elv11[3] = {
	0xAC, 0x0A
};
static unsigned char elv12[3] = {
	0xAC, 0x09
};
/*
static unsigned char elv13[3] = {
	0xAC, 0x08
};
*/
static unsigned char elv14[3] = {
	0xAC, 0x07
};
static unsigned char elv15[3] = {
	0xAC, 0x06
};
static unsigned char elv16[3] = {
	0xAC, 0x05
};
static unsigned char elv17[3] = {
	0xAC, 0x04
};
/*
static unsigned char elv18[3] = {
	0xAC, 0x03
};
*/
static unsigned char elv19[3] = {
	0xAC, 0x02
};
static unsigned char elv20[3] = {
	0xAC, 0x01
};
static unsigned char elv21[3] = {
	0xAC, 0x00
};


static unsigned char elvCaps1[3]  = {
	0xBC, 0x14
};
static unsigned char elvCaps2[3]  = {
	0xBC, 0x13
};
static unsigned char elvCaps3[3]  = {
	0xBC, 0x12
};
static unsigned char elvCaps4[3]  = {
	0xBC, 0x11
};
static unsigned char elvCaps5[3]  = {
	0xBC, 0x10
};
static unsigned char elvCaps6[3]  = {
	0xBC, 0x0F
};
static unsigned char elvCaps7[3]  = {
	0xBC, 0x0E
};
static unsigned char elvCaps8[3]  = {
	0xBC, 0x0D
};
static unsigned char elvCaps9[3]  = {
	0xBC, 0x0C
};
static unsigned char elvCaps10[3]  = {
	0xBC, 0x0B
};
static unsigned char elvCaps11[3]  = {
	0xBC, 0x0A
};

static unsigned char elvCaps12[3] = {
	0xBC, 0x09
};
/*
static unsigned char elvCaps13[3] = {
	0xBC, 0x08
};
*/
static unsigned char elvCaps14[3] = {
	0xBC, 0x07
};
static unsigned char elvCaps15[3] = {
	0xBC, 0x06
};
static unsigned char elvCaps16[3] = {
	0xBC, 0x05
};
static unsigned char elvCaps17[3] = {
	0xBC, 0x04
};
/*
static unsigned char elvCaps18[3] = {
	0xBC, 0x03
};
*/
static unsigned char elvCaps19[3] = {
	0xBC, 0x02
};
static unsigned char elvCaps20[3] = {
	0xBC, 0x01
};
static unsigned char elvCaps21[3] = {
	0xBC, 0x00
};


static char elvss_offset1[3] = {0x00, 0x04, 0x00};
static char elvss_offset2[3] = {0x00, 0x03, -0x01};
static char elvss_offset3[3] = {0x00, 0x02, -0x02};
static char elvss_offset4[3] = {0x00, 0x01, -0x03};
static char elvss_offset5[3] = {0x00, 0x00, -0x04};
static char elvss_offset6[3] = {-0x02, -0x02, -0x07};
static char elvss_offset7[3] = {-0x04, -0x04, -0x09};
static char elvss_offset8[3] = {-0x05, -0x05, -0x0A};

#endif

