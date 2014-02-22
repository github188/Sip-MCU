/*
   ITU-T G.729 Annex C - Reference C code for floating point
                         implementation of G.729
                         Version 1.01 of 15.September.98
*/

/*
----------------------------------------------------------------------
                    COPYRIGHT NOTICE
----------------------------------------------------------------------
   ITU-T G.729 Annex C ANSI C source code
   Copyright (C) 1998, AT&T, France Telecom, NTT, University of
   Sherbrooke.  All rights reserved.

----------------------------------------------------------------------
*/

/*
 File : FILTER.C
 Used for the floating point version of both
 G.729 main body and G.729A
*/

/***************************************/
/* General filter routines             */
/***************************************/

#include "typedef.h"
#include "ld8a.h"

/*-----------------------------------------------------------*
 * convolve - convolve vectors x and h and put result in y   *
 *-----------------------------------------------------------*/

void convolve(
 GFLOAT x[],             /* input : input vector x[0:l]                     */
 GFLOAT h[],             /* input : impulse response or second input h[0:l] */
 GFLOAT y[],             /* output: x convolved with h , y[0:l]             */
 int  l                 /* input : dimension of all vectors                */
)
{
   GFLOAT temp;
   int    i, n;

   for (n = 0; n < l; n++)
     {
        temp = (F)0.0;
        for (i = 0; i <= n; i++)
          temp += x[i]*h[n-i];
        y[n] = temp;
     }
}

/*-----------------------------------------------------------*
 * syn_filt - filter with synthesis filter 1/A(z)            *
 *-----------------------------------------------------------*/

void syn_filt(
	GFLOAT a[],     /* input : predictor coefficients a[0:m]    */
	GFLOAT x[],     /* input : excitation signal                */
	GFLOAT y[],     /* output: filtered output signal           */
	int  l,        /* input : vector dimension                 */
	GFLOAT mem[],   /* in/out: filter memory                    */
	int  update    /* input : 0 = no memory update, 1 = update */
)
{
   int  i,j;

   /* This is usually done by memory allocation (l+m) */
   GFLOAT yy_b[L_SUBFR+M];
   GFLOAT s, *yy, *py, *pa;

   /* Copy mem[] to yy[] */

   yy = yy_b;
   for (i = 0; i <M; i++)  *yy++ =  *mem++;

   /* Filtering */

   for (i = 0; i < l; i++)
     {
        py=yy;
        pa=a;
        s = *x++;
        for (j = 0; j <M; j++)  s -= (*++pa) * (*--py);
        *yy++ = s;
        *y++ = s;
     }

   /* Update memory if required */

   if (update != 0)
	   for (i = 0; i <M; i++)  *--mem =*--yy;
}

/*-----------------------------------------------------------*
 * residu - filter input vector with all-zero filter A(Z)    *
 *-----------------------------------------------------------*/

void residu(    /* filter A(z)                                       */
 GFLOAT *a,      /* input : prediction coefficients a[0:m+1], a[0]=1. */
 GFLOAT *x,      /* input : input signal x[0:l-1], x[-1:m] are needed */
 GFLOAT *y,      /* output: output signal y[0:l-1] NOTE: x[] and y[]
                            cannot point to same array               */
 int  l        /* input : dimension of x and y                      */
)
{
  GFLOAT s;
  int  i, j;

  for (i = 0; i < l; i++)
  {
    s = x[i];
    for (j = 1; j <= M; j++) s += a[j]*x[i-j];
    *y++ = s;
  }
}
