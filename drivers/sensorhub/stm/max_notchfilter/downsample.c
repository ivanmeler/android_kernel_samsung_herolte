/**
  Copyright (c) 2014 Ashley Baer
  Copyright (c) 2014 Maxim Integrated Products

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "downsample.h"

/*
	fs = 400; bw = 35; n = 16;
	b = fir1(n-1,bw/(fs/2));
	mat2c(b,'downsample_b')
*/
/*
static fixed downsample_b[] ={
	-0.0029825859455169 * 0x7FFF , -0.0025942417834986 * 0x7FFF, 0.0016687578061928 * 0x7FFF, 0.0184070271276648 * 0x7FFF,
	0.0530127006633963 * 0x7FFF, 0.1015880251006320 * 0x7FFF, 0.1501684392341056 * 0x7FFF, 0.1807318777970240 * 0x7FFF,
	0.1807318777970240 * 0x7FFF, 0.1501684392341056 * 0x7FFF, 0.1015880251006320 * 0x7FFF, 0.0530127006633963 * 0x7FFF,
	0.0184070271276648 * 0x7FFF, 0.0016687578061928 * 0x7FFF, -0.0025942417834986 * 0x7FFF, -0.0029825859455169 * 0x7FFF,
};

static fir_taps ir_inst ={{0},0,16};
static fir_taps r_inst ={{0},0,16};
*/

static fixed downsample_b[] = {
	0.25 * 0x7FFF, 0.25 * 0x7FFF, 0.25 * 0x7FFF, 0.25 * 0x7FFF
};

static fir_taps ir_inst = {{0}, 0, 4};
static fir_taps r_inst = {{0}, 0, 4};
static fir_taps g_inst = {{0}, 0, 4};
static fir_taps v_inst = {{0}, 0, 4};

/*
fs = 400;   % sampling freq
f1 = 120;   % notch freq
f2 = 100;   % notch freq
bw = 5;     % 3-dB notch bandwidth
[b,a] = iirnotch(f1/(fs/2),bw/(fs/2));
mat2c(b,'notch1_b'), mat2c(a,'notch1_a')
[b,a] = iirnotch(f2/(fs/2),bw/(fs/2));
mat2c(b,'notch2_b'), mat2c(a,'notch2_a')
*/

static fixed notch1_b[] = {
	0.9621952458291035 * 0x7FFF, 0.5946693657359463 * 0x7FFF, 0.9621952458291035 * 0x7FFF,
	};
static fixed notch1_a[] = {
	1.0000000000000000 * 0x7FFF, 0.5946693657359463 * 0x7FFF, 0.9243904916582071 * 0x7FFF,
	};
static fixed notch2_b[] = {
	0.9621952458291035 * 0x7FFF, -0.0000000000000001 * 0x7FFF, 0.9621952458291035 * 0x7FFF,
	};
static fixed notch2_a[] = {
	1.0000000000000000 * 0x7FFF, -0.0000000000000001 * 0x7FFF, 0.9243904916582071 * 0x7FFF,
	};

static iir_taps ir_notch[2] = {{{0}, {0}, 0, 3}, {{0}, {0}, 0, 3}};
static iir_taps r_notch[2] = {{{0}, {0}, 0, 3}, {{0}, {0}, 0, 3}};
static iir_taps g_notch[2] = {{{0}, {0}, 0, 3}, {{0}, {0}, 0, 3}};
static iir_taps v_notch[2] = {{{0}, {0}, 0, 3}, {{0}, {0}, 0, 3}};


fir_taps *downsample_get_instance(channel n)
{
	switch (n) {
	case CHANNEL_IR:
		return &ir_inst;
	case CHANNEL_R:
		return &r_inst;
	case CHANNEL_G:
		return &g_inst;
	case CHANNEL_V:
		return &v_inst;
	default:
		return 0;
	}
}

iir_taps *notch_get_instance(channel n)
{
	switch (n) {
	case CHANNEL_IR:
		return ir_notch;
	case CHANNEL_R:
		return r_notch;
	case CHANNEL_G:
		return g_notch;
	case CHANNEL_V:
		return v_notch;
	default:
		return 0;
	}
}

void downsample_init()
{
	iir_filter_init(&ir_notch[0], 0);
	iir_filter_init(&r_notch[0], 0);
	iir_filter_init(&g_notch[0], 0);
	iir_filter_init(&v_notch[0], 0);
	iir_filter_init(&ir_notch[1], 0);
	iir_filter_init(&r_notch[1], 0);
	iir_filter_init(&g_notch[1], 0);
	iir_filter_init(&v_notch[1], 0);
	fir_filter_init(&ir_inst, 0);
	fir_filter_init(&r_inst, 0);
	fir_filter_init(&g_inst, 0);
	fir_filter_init(&v_inst, 0);
}

fixed downsample(fixed x, channel n)
{
	fixed temp = iir_filter(x, notch1_b, notch1_a,
		&notch_get_instance(n)[0]);
	temp = iir_filter(temp, notch2_b, notch2_a, &notch_get_instance(n)[1]);
	return fir_filter(temp, downsample_b, downsample_get_instance(n));
}
