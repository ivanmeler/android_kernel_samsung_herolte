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

#ifndef FIXED_INCLUDE
#define FIXED_INCLUDE

typedef int fixed;

#define PRECISION 16
#define MAXSCALE ((1 << (PRECISION - 1)) - 1)

#define round(x) ((x) >= 0 ? (long)((x) + 0.5) : (long)((x) - 0.5))
#define float2fixed(X) (fixed)((X*MAXSCALE))
#define fixed2float(X) (float)((float)(X)/MAXSCALE)
#define fixed2int(X) (int)round((float)(X)/MAXSCALE)
#define int2fixed(X) (fixed)((int)(X)*MAXSCALE)

#define fixed_axb(a, x, b) (fixed)(((a * x) >> (PRECISION - 1)) + b)
#define fixed_mac(a, x) fixed_axb(a, x, 0)

#endif
