/**
  Copyright (c) 2012-12 Paul Kettle
  Copyright (c) 2012-12 Maxim Integrated Products

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

#include "fixed_fir_filter.h"

void fir_filter_init(fir_taps *taps, fixed val)
{
	int i;

	for (i = 0; i < taps->size; i++) {
		taps->num[i] = val;
	}

	taps->clk = 0;
}

fixed fir_filter(fixed x, fixed b[], fir_taps *taps)
{
	int i;
	register fixed acc;
	register int N = taps->size;
	register int clk = taps->clk;

	/*
	Implementing y(n+2) = - a1 y(n+1) - a2 y(n) + b0 x(n+2) + b1 x(n+1) - b2 x(n)
	taps := [y(n+2), y(n+1), y(n), x(n+2), x(n+1), x(n)]
	b:= [b0, b1, b2, b3]
	*/

	acc = 0;

	taps->num[clk%N] = x;

	for (i = 0; i < N; i++) {
		/* b0 x(n) + b2 x(n-1) + b2 x(n-2) + b3 x(n-3) */
		acc = fixed_axb(b[i], taps->num[(clk-i+N)%(N)], acc);
	}

	(taps->clk) = (++clk)%N;

	return acc;
}
