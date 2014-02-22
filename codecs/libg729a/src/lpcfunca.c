/*
   ITU-T G.729 Annex C - Reference C code for floating point
                         implementation of G.729 Annex A
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
 File : LPCFUNCA.C
 Used for the floating point version of G.729A only
 (not for G.729 main body)
*/

/*-------------------------------------------------------------*
 *  Procedure lsp_az:                                          *
 *            ~~~~~~                                           *
 *   Compute the LPC coefficients from lsp (order=10)          *
 *-------------------------------------------------------------*/

#include <math.h>
#include "typedef.h"
#include "ld8a.h"
#include "tab_ld8a.h"


/*-----------------------------------------------------------------------------
 * lsp_az - convert LSPs to predictor coefficients a[]
 *-----------------------------------------------------------------------------
 */
void lsp_az(
 GFLOAT *lsp,            /* input : lsp[0:M-1] */
 GFLOAT *a               /* output: predictor coeffs a[0:M], a[0] = 1. */
)
{

  GFLOAT f1[M], f2[M];
  int i,j;


  get_lsp_pol(&lsp[0],f1);
  get_lsp_pol(&lsp[1],f2);

  for (i = NC; i > 0; i--)
  {
    f1[i] += f1[i-1];
    f2[i] -= f2[i-1];
  }
  a[0] = (F)1.0;
  for (i = 1, j = M; i <= NC; i++, j--)
  {
    a[i] = (F)0.5*(f1[i] + f2[i]);
    a[j] = (F)0.5*(f1[i] - f2[i]);
  }
}


/*----------------------------------------------------------------------------
 * get_lsp_pol - find the polynomial F1(z) or F2(z) from the LSFs
 *----------------------------------------------------------------------------
 */
void get_lsp_pol(
   GFLOAT lsp[],           /* input : line spectral freq. (cosine domain)  */
   GFLOAT f[]              /* output: the coefficients of F1 or F2 */
)
{
  GFLOAT b;
  int   i,j;

  f[0] = (F)1.0;
  b = (F)-2.0*lsp[0];
  f[1] = b;
  for (i = 2; i <= NC; i++)
  {
    b = (F)-2.0*lsp[2*i-2];
    f[i] = b*f[i-1] + (F)2.0*f[i-2];
    for (j = i-1; j > 1; j--)
      f[j] += b*f[j-1] + f[j-2];
    f[1] += b;
  }
}
/*---------------------------------------------------------------------------
 * weight_az:  Weighting of LPC coefficients  ap[i]  =  a[i] * (gamma ** i)
 *---------------------------------------------------------------------------
 */
void weight_az(
 GFLOAT *a,              /* input : lpc coefficients a[0:m]       */
 GFLOAT gamma,           /* input : weighting factor              */
 int m,                 /* input : filter order                  */
 GFLOAT *ap              /* output: weighted coefficients ap[0:m] */
)
{
  GFLOAT fac;
  int i;

  ap[0]=a[0];
  fac=gamma;
  for (i = 1; i < m; i++)
  {
    ap[i] = fac*a[i];
    fac *= gamma;
  }
  ap[m] = fac*a[m];
}

/*-----------------------------------------------------------------------------
 * int_qlpc -  interpolated M LSP parameters and convert to M+1 LPC coeffs
 *-----------------------------------------------------------------------------
 */
void int_qlpc(
 GFLOAT lsp_old[],       /* input : LSPs for past frame (0:M-1) */
 GFLOAT lsp_new[],       /* input : LSPs for present frame (0:M-1) */
 GFLOAT az[]             /* output: filter parameters in 2 subfr (dim 2(m+1)) */
)
{
  int i;
  GFLOAT lsp[M];

  for (i = 0; i < M; i++)
    lsp[i] = lsp_old[i]*(F)0.5 + lsp_new[i]*(F)0.5;

  lsp_az(lsp, az);
  lsp_az(lsp_new, &az[M+1]);
}

/* inline versions of the following funcs are in lpcfinl.c */
#ifndef INLINE_FUNCS

/*----------------------------------------------------------------------------
 * lsf_lsp - convert from lsf[0..M-1 to lsp[0..M-1]
 *----------------------------------------------------------------------------
 */
void lsf_lsp(
    const GFLOAT lsf[],          /* input :  lsf */
    GFLOAT lsp[],          /* output: lsp */
    int m
)
{
    int i;
    for ( i = 0; i < m; i++ )
        lsp[i] = (GFLOAT)cos(lsf[i]);
}

/*----------------------------------------------------------------------------
 * lsp_lsf - convert from lsp[0..M-1 to lsf[0..M-1]
 *----------------------------------------------------------------------------
 */
void lsp_lsf(
    const GFLOAT lsp[],          /* input :  lsp coefficients */
    GFLOAT lsf[],          /* output:  lsf (normalized frequencies */
    int m
)
{
    int i;
    
    for ( i = 0; i < m; i++ )
        lsf[i] = (GFLOAT)acos(lsp[i]);
}

#endif /* INLINE_FUNCS */
