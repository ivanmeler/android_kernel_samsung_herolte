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

#ifndef __S6E3HF2_WQHD_AID_DIMMING_H__
#define __S6E3HF2_WQHD_AID_DIMMING_H__

// start of rev.D
static signed char RDdrtbl2nit[10] = {0, 20, 24, 24, 20, 17, 15, 11, 7, 0};
static signed char RDdrtbl3nit[10] = {0, 20, 24, 22, 18, 16, 13, 10, 7, 0};
static signed char RDdrtbl4nit[10] = {0, 24, 22, 19, 15, 13, 12, 9, 6, 0};
static signed char RDdrtbl5nit[10] = {0, 20, 21, 18, 15, 13, 12, 9, 6, 0};
static signed char RDdrtbl6nit[10] = {0, 21, 21, 18, 14, 13, 12, 8, 6, 0};
static signed char RDdrtbl7nit[10] = {0, 22, 20, 17, 13, 12, 11, 8, 6, 0};
static signed char RDdrtbl8nit[10] = {0, 19, 19, 16, 12, 11, 10, 7, 5, 0};
static signed char RDdrtbl9nit[10] = {0, 18, 18, 15, 11, 10, 9, 7, 5, 0};
static signed char RDdrtbl10nit[10] = {0, 20, 17, 14, 11, 10, 9, 6, 5, 0};
static signed char RDdrtbl11nit[10] = {0, 17, 16, 13, 10, 9, 8, 6, 4, 0};
static signed char RDdrtbl12nit[10] = {0, 15, 16, 13, 10, 9, 8, 6, 4, 0};
static signed char RDdrtbl13nit[10] = {0, 18, 15, 12, 9, 8, 7, 5, 4, 0};
static signed char RDdrtbl14nit[10] = {0, 15, 15, 11, 9, 8, 7, 5, 4, 0};
static signed char RDdrtbl15nit[10] = {0, 17, 14, 11, 8, 7, 7, 5, 4, 0};
static signed char RDdrtbl16nit[10] = {0, 15, 14, 10, 8, 7, 6, 5, 4, 0};
static signed char RDdrtbl17nit[10] = {0, 17, 13, 10, 7, 6, 6, 5, 4, 0};
static signed char RDdrtbl19nit[10] = {0, 16, 12, 9, 6, 6, 5, 4, 3, 0};
static signed char RDdrtbl20nit[10] = {0, 15, 12, 9, 6, 6, 5, 4, 3, 0};
static signed char RDdrtbl21nit[10] = {0, 11, 12, 8, 6, 6, 5, 4, 3, 0};
static signed char RDdrtbl22nit[10] = {0, 10, 12, 8, 6, 5, 5, 4, 3, 0};
static signed char RDdrtbl24nit[10] = {0, 12, 11, 8, 5, 5, 5, 4, 3, 0};
static signed char RDdrtbl25nit[10] = {0, 14, 10, 7, 5, 4, 4, 4, 3, 0};
static signed char RDdrtbl27nit[10] = {0, 10, 10, 7, 5, 4, 4, 3, 3, 0};
static signed char RDdrtbl29nit[10] = {0, 13, 9, 6, 4, 4, 4, 3, 3, 0};
static signed char RDdrtbl30nit[10] = {0, 10, 9, 6, 4, 4, 4, 3, 3, 0};
static signed char RDdrtbl32nit[10] = {0, 9, 9, 6, 4, 3, 4, 3, 3, 0};
static signed char RDdrtbl34nit[10] = {0, 11, 8, 6, 3, 3, 3, 3, 3, 0};
static signed char RDdrtbl37nit[10] = {0, 9, 8, 6, 3, 3, 3, 3, 3, 0};
static signed char RDdrtbl39nit[10] = {0, 10, 7, 5, 3, 3, 3, 3, 3, 0};
static signed char RDdrtbl41nit[10] = {0, 8, 7, 5, 3, 3, 3, 3, 3, 0};
static signed char RDdrtbl44nit[10] = {0, 7, 7, 5, 3, 3, 3, 3, 3, 0};
static signed char RDdrtbl47nit[10] = {0, 8, 6, 4, 2, 3, 3, 2, 3, 0};
static signed char RDdrtbl50nit[10] = {0, 6, 6, 4, 2, 2, 3, 2, 3, 0};
static signed char RDdrtbl53nit[10] = {0, 9, 5, 4, 2, 2, 3, 2, 3, 0};
static signed char RDdrtbl56nit[10] = {0, 7, 5, 4, 2, 2, 2, 2, 3, 0};
static signed char RDdrtbl60nit[10] = {0, 4, 5, 4, 1, 2, 2, 1, 3, 0};
static signed char RDdrtbl64nit[10] = {0, 6, 4, 3, 1, 2, 2, 1, 2, 0};
static signed char RDdrtbl68nit[10] = {0, 4, 4, 3, 1, 2, 2, 1, 2, 0};
static signed char RDdrtbl72nit[10] = {0, 3, 4, 3, 1, 2, 2, 1, 2, 0};
static signed char RDdrtbl77nit[10] = {0, 3, 4, 2, 1, 2, 2, 2, 2, 0};
static signed char RDdrtbl82nit[10] = {0, 4, 4, 3, 1, 1, 1, 2, 2, 0};
static signed char RDdrtbl87nit[10] = {0, 3, 4, 3, 2, 1, 1, 2, 3, 0};
static signed char RDdrtbl93nit[10] = {0, 4, 3, 2, 1, 1, 2, 3, 1, 0};
static signed char RDdrtbl98nit[10] = {0, 2, 3, 2, 1, 1, 1, 3, 2, 0};
static signed char RDdrtbl105nit[10] = {0, 5, 3, 2, 1, 2, 3, 3, 2, 0};
static signed char RDdrtbl110nit[10] = {0, 4, 3, 2, 1, 1, 2, 4, 2, 0};
static signed char RDdrtbl119nit[10] = {0, 5, 2, 1, 2, 1, 2, 3, 1, 0};
static signed char RDdrtbl126nit[10] = {0, 6, 2, 2, 1, 2, 2, 3, 0, 0};
static signed char RDdrtbl134nit[10] = {0, 4, 2, 2, 1, 1, 1, 3, 0, 0};
static signed char RDdrtbl143nit[10] = {0, 4, 2, 1, 1, 1, 2, 3, 1, 0};
static signed char RDdrtbl152nit[10] = {0, 4, 2, 2, 1, 1, 2, 4, 2, 0};
static signed char RDdrtbl162nit[10] = {0, 3, 1, 1, 0, 1, 2, 4, 1, 0};
static signed char RDdrtbl172nit[10] = {0, 5, 1, 1, 0, 1, 1, 3, 1, 0};
static signed char RDdrtbl183nit[10] = {0, 2, 2, 1, 1, 1, 2, 4, 1, 0};
static signed char RDdrtbl195nit[10] = {0, 5, 1, 1, 1, 1, 1, 3, 1, 0};
static signed char RDdrtbl207nit[10] = {0, 4, 1, 1, 1, 1, 1, 3, 1, 0};
static signed char RDdrtbl220nit[10] = {0, 1, 1, 0, 1, 1, 1, 3, 1, 0};
static signed char RDdrtbl234nit[10] = {0, 0, 1, 0, 1, 1, 1, 3, 1, 0};
static signed char RDdrtbl249nit[10] = {0, 0, 1, 0, 1, 1, 1, 2, 1, 0};
static signed char RDdrtbl265nit[10] = {0, 0, 1, 1, 1, 0, 1, 2, 0, 0};
static signed char RDdrtbl282nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 0, 0};
static signed char RDdrtbl300nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char RDdrtbl316nit[10] = {0, 2, 0, 0, -1, 0, -1, 1, 0, 0};
static signed char RDdrtbl333nit[10] = {0, 2, 0, -1, -1, -1, -1, 0, -1, 0};
static signed char RDdrtbl360nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char RDdctbl2nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -7, -6, 3, -6, -11, 2, -6, -16, 4, -8, -10, 1, -4, -3, 0, -2, -4, 0, -3, -5, 1, -6};
static signed char RDdctbl3nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -7, -6, 3, -6, -10, 3, -7, -12, 3, -7, -9, 1, -4, -3, 0, -2, -3, 0, -3, -4, 1, -5};
static signed char RDdctbl4nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -7, -7, 2, -6, -10, 3, -8, -10, 2, -6, -7, 2, -4, -2, 0, -2, -3, 0, -2, -2, 1, -4};
static signed char RDdctbl5nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -7, -7, 3, -8, -7, 3, -7, -9, 3, -6, -6, 2, -4, -3, 0, -2, -2, 0, -2, -2, 0, -4};
static signed char RDdctbl6nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -7, 3, -7, -5, 3, -8, -7, 2, -6, -6, 2, -4, -2, 0, -2, -1, 0, -1, -2, 0, -4};
static signed char RDdctbl7nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -7, 3, -8, -5, 4, -8, -7, 2, -5, -5, 1, -4, -2, 0, -2, -1, 0, -1, -1, 0, -3};
static signed char RDdctbl8nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -5, 3, -8, -4, 4, -9, -7, 2, -5, -5, 1, -4, -2, 0, -2, -1, 0, -1, 0, 0, -2};
static signed char RDdctbl9nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -4, 4, -8, -4, 4, -9, -5, 2, -4, -4, 1, -3, -1, 0, -1, -1, 0, -1, 0, 0, -2};
static signed char RDdctbl10nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -10, -5, 4, -10, -3, 3, -8, -6, 2, -5, -3, 1, -2, -2, 0, -2, 0, 0, -1, 0, 0, -1};
static signed char RDdctbl11nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -4, 4, -10, -3, 3, -6, -5, 2, -5, -4, 1, -3, -2, 0, -2, 0, 0, -1, 1, 0, 0};
static signed char RDdctbl12nit[30] = {0, 0, 0, 0, 0, 0,-2, 4, -8, -3, 4, -10, -3, 3, -6, -5, 2, -5, -3, 1, -3, -2, 0, -1, 0, 0, -1, 1, 0, 0};
static signed char RDdctbl13nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -11, -4, 4, -10, -2, 3, -6, -5, 2, -5, -3, 1, -3, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char RDdctbl14nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -3, 5, -11, -2, 3, -6, -4, 2, -4, -2, 1, -2, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char RDdctbl15nit[30] = {0, 0, 0, 0, 0, 0,-2, 6, -13, -2, 4, -10, -3, 2, -6, -4, 2, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char RDdctbl16nit[30] = {0, 0, 0, 0, 0, 0,-2, 4, -10, -2, 5, -12, -3, 2, -5, -4, 1, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char RDdctbl17nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -14, -2, 4, -10, -3, 2, -5, -4, 1, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char RDdctbl19nit[30] = {0, 0, 0, 0, 0, 0,0, 7, -16, -3, 4, -10, -3, 1, -4, -3, 1, -3, -2, 1, -2, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl20nit[30] = {0, 0, 0, 0, 0, 0,0, 7, -16, -2, 4, -9, -3, 1, -4, -4, 1, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl21nit[30] = {0, 0, 0, 0, 0, 0,0, 5, -10, -3, 5, -10, -3, 1, -4, -3, 1, -3, -2, 0, -2, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl22nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -10, -2, 5, -10, -3, 1, -4, -3, 1, -3, -2, 0, -2, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl24nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -14, -3, 4, -8, -4, 1, -4, -3, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl25nit[30] = {0, 0, 0, 0, 0, 0,1, 7, -16, -2, 5, -10, -2, 1, -3, -4, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl27nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -14, -3, 4, -9, -2, 1, -3, -3, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl29nit[30] = {0, 0, 0, 0, 0, 0,0, 7, -16, -3, 5, -11, -3, 1, -3, -3, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl30nit[30] = {0, 0, 0, 0, 0, 0,0, 7, -14, -2, 4, -10, -3, 1, -4, -3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl32nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -13, -3, 4, -10, -2, 1, -2, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl34nit[30] = {0, 0, 0, 0, 0, 0,0, 8, -18, -2, 4, -8, -2, 1, -3, -3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1};
static signed char RDdctbl37nit[30] = {0, 0, 0, 0, 0, 0,0, 7, -16, -3, 3, -8, -2, 1, -3, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl39nit[30] = {0, 0, 0, 0, 0, 0,0, 8, -17, -2, 4, -8, -1, 1, -2, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl41nit[30] = {0, 0, 0, 0, 0, 0,1, 7, -14, -3, 4, -8, -1, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl44nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -14, -4, 3, -8, -1, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl47nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -14, -1, 4, -8, -1, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char RDdctbl50nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -12, -2, 3, -8, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char RDdctbl53nit[30] = {0, 0, 0, 0, 0, 0,-1, 8, -17, -2, 3, -8, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static signed char RDdctbl56nit[30] = {0, 0, 0, 0, 0, 0,-1, 6, -14, -1, 3, -7, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0, 2, 2, 0, 2};
static signed char RDdctbl60nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -11, -1, 3, -7, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0, 2, 2, 0, 2};
static signed char RDdctbl64nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -13, -1, 4, -9, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0, 2, 2, 0, 2};
static signed char RDdctbl68nit[30] = {0, 0, 0, 0, 0, 0,0, 5, -10, 0, 4, -8, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0, 2, 2, 0, 2};
static signed char RDdctbl72nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -10, -1, 3, -8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0, 2, 2, 0, 2};
static signed char RDdctbl77nit[30] = {0, 0, 0, 0, 0, 0,0, 4, -9, -1, 3, -8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 2, 0, 2};
static signed char RDdctbl82nit[30] = {0, 0, 0, 0, 0, 0,-2, 4, -10, 0, 2, -6, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 2, 2, 0, 2};
static signed char RDdctbl87nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, 0, 2, -6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 2, 0, 2};
static signed char RDdctbl93nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -8, 0, 2, -6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 2, 0, 2};
static signed char RDdctbl98nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -6, 0, 2, -5, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char RDdctbl105nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -1, 1, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1};
static signed char RDdctbl110nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -8, 1, 2, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char RDdctbl119nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -7, -1, 1, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char RDdctbl126nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -1, 1, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char RDdctbl134nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, 0, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char RDdctbl143nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -6, -1, 1, -4, 0, 0, 0, -1, 0, 0, 0, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char RDdctbl152nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -6, -1, 1, -3, 0, 0, 0, 0, 0, 0, -1, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char RDdctbl162nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -6, 0, 1, -2, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl172nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -6, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl183nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -4, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl195nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -4, 0, 0, -2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl207nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -4, -1, 0, -2, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char RDdctbl220nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -2, 0, 0, -2, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char RDdctbl234nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, -2, 0, 0, -2, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char RDdctbl249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char RDdctbl265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char RDdctbl282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char RDdctbl300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char RDdctbl316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char RDdctbl333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char RDdctbl360nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char aid9798[3] = {0xB2, 0xDC, 0x90};
static signed char aid9720[3] = {0xB2, 0xC8, 0x90};
static signed char aid9612[3] = {0xB2, 0xAC, 0x90};
static signed char aid9530[3] = {0xB2, 0x97, 0x90};
static signed char aid9449[3] = {0xB2, 0x82, 0x90};
static signed char aid9359[3] = {0xB2, 0x6B, 0x90};
static signed char aid9282[3] = {0xB2, 0x57, 0x90};
static signed char aid9208[3] = {0xB2, 0x44, 0x90};
static signed char aid9115[3] = {0xB2, 0x2C, 0x90};
static signed char aid8998[3] = {0xB2, 0x0E, 0x90};
static signed char aid8960[3] = {0xB2, 0x04, 0x90};
static signed char aid8894[3] = {0xB2, 0xF3, 0x80};
static signed char aid8832[3] = {0xB2, 0xE3, 0x80};
static signed char aid8723[3] = {0xB2, 0xC7, 0x80};
static signed char aid8653[3] = {0xB2, 0xB5, 0x80};
static signed char aid8575[3] = {0xB2, 0xA1, 0x80};
static signed char aid8381[3] = {0xB2, 0x6F, 0x80};
static signed char aid8292[3] = {0xB2, 0x58, 0x80};
static signed char aid8218[3] = {0xB2, 0x45, 0x80};
static signed char aid8148[3] = {0xB2, 0x33, 0x80};
static signed char aid7966[3] = {0xB2, 0x04, 0x80};
static signed char aid7865[3] = {0xB2, 0xEA, 0x70};
static signed char aid7710[3] = {0xB2, 0xC2, 0x70};
static signed char aid7512[3] = {0xB2, 0x8F, 0x70};
static signed char aid7384[3] = {0xB2, 0x6E, 0x70};
static signed char aid7267[3] = {0xB2, 0x50, 0x70};
static signed char aid7089[3] = {0xB2, 0x22, 0x70};
static signed char aid6813[3] = {0xB2, 0xDB, 0x60};
static signed char aid6658[3] = {0xB2, 0xB3, 0x60};
static signed char aid6467[3] = {0xB2, 0x82, 0x60};
static signed char aid6188[3] = {0xB2, 0x3A, 0x60};
static signed char aid5920[3] = {0xB2, 0xF5, 0x50};
static signed char aid5637[3] = {0xB2, 0xAC, 0x50};
static signed char aid5365[3] = {0xB2, 0x66, 0x50};
static signed char aid5085[3] = {0xB2, 0x1E, 0x50};
static signed char aid4732[3] = {0xB2, 0xC3, 0x40};
static signed char aid4344[3] = {0xB2, 0x5F, 0x40};
static signed char aid3956[3] = {0xB2, 0xFB, 0x30};
static signed char aid3637[3] = {0xB2, 0xA9, 0x30};
static signed char aid3168[3] = {0xB2, 0x30, 0x30};
static signed char aid2659[3] = {0xB2, 0xAD, 0x20};
static signed char aid2186[3] = {0xB2, 0x33, 0x20};
static signed char aid1580[3] = {0xB2, 0x97, 0x10};
static signed char aid1005[3] = {0xB2, 0x03, 0x10};


static unsigned char delv1 [3] = {
	0xB6, 0x8C, 0x15
};

static unsigned char delv2 [3] = {
	0xB6, 0x8C, 0x14
};

static unsigned char delv3 [3] = {
	0xB6, 0x8C, 0x13
};

static unsigned char delv4 [3] = {
	0xB6, 0x8C, 0x12
};

static unsigned char delv5 [3] = {
	0xB6, 0x8C, 0x11
};

static unsigned char delv6 [3] = {
	0xB6, 0x8C, 0x10
};

static unsigned char delv7 [3] = {
	0xB6, 0x8C, 0x0F
};

static unsigned char delv8 [3] = {
	0xB6, 0x8C, 0x0E
};

static unsigned char delv9 [3] = {
	0xB6, 0x8C, 0x0D
};

static unsigned char delv10 [3] = {
	0xB6, 0x8C, 0x0C
};

static unsigned char delv11 [3] = {
	0xB6, 0x8C, 0x0B
};

static unsigned char delv12 [3] = {
	0xB6, 0x8C, 0x0A
};

// hbm interpolation
static unsigned char delv13 [3] = {
	0xB6, 0x8C, 0x08
};
static unsigned char delv14 [3] = {
	0xB6, 0x8C, 0x06
};
static unsigned char delv15 [3] = {
	0xB6, 0x8C, 0x05
};
static unsigned char delv16 [3] = {
	0xB6, 0x8C, 0x03
};
static unsigned char delv17 [3] = {
	0xB6, 0x8C, 0x01
};



static unsigned char delvCaps1 [3] = {
	0xB6, 0x9C, 0x15
};

static unsigned char delvCaps2 [3] = {
	0xB6, 0x9C, 0x14
};

static unsigned char delvCaps3 [3] = {
	0xB6, 0x9C, 0x13
};

static unsigned char delvCaps4 [3] = {
	0xB6, 0x9C, 0x12
};

static unsigned char delvCaps5 [3] = {
	0xB6, 0x9C, 0x11
};

static unsigned char delvCaps6 [3] = {
	0xB6, 0x9C, 0x10
};

static unsigned char delvCaps7 [3] = {
	0xB6, 0x9C, 0x0F
};

static unsigned char delvCaps8 [3] = {
	0xB6, 0x9C, 0x0E
};
static unsigned char delvCaps9 [3] = {
	0xB6, 0x9C, 0x0D
};

static unsigned char delvCaps10 [3] = {
	0xB6, 0x9C, 0x0C
};

static unsigned char delvCaps11 [3] = {
	0xB6, 0x9C, 0x0B
};

static unsigned char delvCaps12 [3] = {
	0xB6, 0x9C, 0x0A
};

// hbm interpolation
static unsigned char delvCaps13 [3] = {
	0xB6, 0x9C, 0x08
};
static unsigned char delvCaps14 [3] = {
	0xB6, 0x9C, 0x06
};
static unsigned char delvCaps15 [3] = {
	0xB6, 0x9C, 0x05
};
static unsigned char delvCaps16 [3] = {
	0xB6, 0x9C, 0x03
};
static unsigned char delvCaps17 [3] = {
	0xB6, 0x9C, 0x01
};

#endif
