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
 File : UTIL.C
 Used for the floating point version of both
 G.729 main body and G.729A
*/

/*****************************************************************************/
/* auxiliary functions                                                       */
/*****************************************************************************/

#include "typedef.h"
#include "ld8a.h"

#ifndef INLINE_FUNCS

/*-------------------------------------------------------------------*
 * Function  set zero()                                              *
 *           ~~~~~~~~~~                                              *
 * Set vector x[] to zero                                            *
 *-------------------------------------------------------------------*/

void set_zero(GFLOAT dst[], int L)
{
   int i;

   for (i = 0; i < L; i++)
     dst[i] = (F)0.0;
}

/*-------------------------------------------------------------------*
 * Function  copy:                                                   *
 *           ~~~~~                                                   *
 * Copy vector x[] to y[]                                            *
 *-------------------------------------------------------------------*/

void copy(const GFLOAT src[], GFLOAT dst[], int L)
{
   int i;

   for (i = 0; i < L; i++)
     dst[i] = src[i];
}

/* Random generator  */
INT16 random_g729(INT16 *seed)
{
	return *seed = (INT16) ((*seed) * 31821L + 13849L);
}

#endif /* INLINE_FUNCS */

void get_freq_prev(const GFLOAT freq_prev[MA_NP][M], GFLOAT x[MA_NP][M])
{
	int i;

	for (i=0; i<MA_NP; i++)
		copy(&freq_prev[i][0], &x[i][0], M);
}
  
void update_freq_prev(GFLOAT freq_prev[MA_NP][M], const GFLOAT x[MA_NP][M])
{
	int i;

	for (i=0; i<MA_NP; i++)
		copy(&x[i][0], &freq_prev[i][0], M);
}

/*-----------------------------------------------------------*
 * fwrite16 - writes a GFLOAT array as a Short to a a file    *
 *-----------------------------------------------------------*/

void fwrite16(
 GFLOAT *data,           /* input: inputdata */
 int length,          /* input: length of data array */
 FILE *fp               /* input: file pointer */
)
{
   int  i;
   INT16 sp16[L_FRAME];
   GFLOAT temp;

   if (length > L_FRAME) {
      printf("error in fwrite16\n");
      exit(16);
   }

   for(i=0; i<length; i++)
    {
      /* round and convert to int  */
      temp = data[i];
      if (temp >= (F)0.0)
            temp += (F)0.5;
      else  temp -= (F)0.5;
      if (temp >  (F)32767.0 ) temp =  (F)32767.0;
      if (temp < (F)-32768.0 ) temp = (F)-32768.0;
      sp16[i] = (INT16) temp;
    }
    fwrite( sp16, sizeof(INT16), length, fp);
}

/*****************************************************************************/
/* Functions used by VAD.C                                                   */
/*****************************************************************************/

void dvsub(GFLOAT *in1, GFLOAT *in2, GFLOAT *out, int npts)
{
    while (npts--)  *(out++) = *(in1++) - *(in2++);
}

GFLOAT dvdot(GFLOAT *in1, GFLOAT *in2, int npts)
{
    GFLOAT accum;
    
    accum = (F)0.0;
    while (npts--)  accum += *(in1++) * *(in2++);
    return(accum);
}

void dvwadd(GFLOAT *in1, GFLOAT scalar1, GFLOAT *in2, GFLOAT scalar2,
                        GFLOAT *out, int npts)
{
    while (npts--)  *(out++) = *(in1++) * scalar1 + *(in2++) * scalar2;
}

void dvsmul(GFLOAT *in, GFLOAT scalar, GFLOAT *out, int npts)
{
    while (npts--)  *(out++) = *(in++) * scalar;
}
