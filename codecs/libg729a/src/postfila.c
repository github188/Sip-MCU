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

/*------------------------------------------------------------------------*
 *                         POSTFILTER.C                                   *
 *------------------------------------------------------------------------*
 * Performs adaptive postfiltering on the synthesis speech                *
 * This file contains all functions related to the post filter.           *
 *------------------------------------------------------------------------*/

#include <math.h>
#include "typedef.h"
#include "ld8a.h"

/* prototype of local functions */

static void pit_pst_filt(
  GFLOAT *signal,        /* input : input signal                        */
  int   t0_min,         /* input : minimum value in the searched range */
  int   t0_max,         /* input : maximum value in the searched range */
  int   L_subfr,        /* input : size of filtering                   */
  GFLOAT *signal_pst     /* output: harmonically postfiltered signal    */
);
static void agc(struct postfilt_state_t *,
  GFLOAT *sig_in,   /* input : postfilter input signal  */
  GFLOAT *sig_out,  /* in/out: postfilter output signal */
  int l_trm        /* input : subframe size            */
);
static void preemphasis(struct postfilt_state_t *,
  GFLOAT *signal,   /* in/out: input signal overwritten by the output */
  GFLOAT g,         /* input : preemphasis coefficient                */
  int L            /* input : size of filtering                      */
);

/*---------------------------------------------------------------*
 *    Postfilter constant parameters (defined in "ld8a.h")       *
 *---------------------------------------------------------------*
 *   L_FRAME     : Frame size.                                   *
 *   L_SUBFR     : Sub-frame size.                               *
 *   M           : LPC order.                                    *
 *   MP1         : LPC order+1                                   *
 *   PIT_MAX     : Maximum pitch lag.                            *
 *   GAMMA2_PST  : Formant postfiltering factor (numerator)      *
 *   GAMMA1_PST  : Formant postfiltering factor (denominator)    *
 *   GAMMAP      : Harmonic postfiltering factor                 *
 *   MU          : Factor for tilt compensation filter           *
 *   AGC_FAC     : Factor for automatic gain control             *
 *---------------------------------------------------------------*/


/*---------------------------------------------------------------*
 * Procedure    init_post_filter:                                 *
 *              ~~~~~~~~~~~~~                                    *
 *  Initializes the postfilter parameters:                       *
 *---------------------------------------------------------------*/

void init_post_filter(struct postfilt_state_t * state)
{
  state->res2  = state->res2_buf + PIT_MAX;

  set_zero(state->mem_syn_pst, M);
  set_zero(state->res2_buf, PIT_MAX+L_SUBFR);

  state->mem_pre = (F)0.;
  state->past_gain = (F)1.0;
}


/*------------------------------------------------------------------------*
 *  Procedure     post_filter:                                            *
 *                ~~~~~~~~~~~                                             *
 *------------------------------------------------------------------------*
 *  The postfiltering process is described as follows:                    *
 *                                                                        *
 *  - inverse filtering of syn[] through A(z/GAMMA2_PST) to get res2[]    *
 *  - use res2[] to compute pitch parameters                              *
 *  - perform pitch postfiltering                                         *
 *  - tilt compensation filtering; 1 - MU*k*z^-1                          *
 *  - synthesis filtering through 1/A(z/GAMMA1_PST)                       *
 *  - adaptive gain control                                               *
 *------------------------------------------------------------------------*/

void post_filter(struct postfilt_state_t * state,
  GFLOAT *syn,     /* in/out: synthesis speech (postfiltered is output)    */
  GFLOAT *az_4,    /* input : interpolated LPC parameters in all subframes */
  int *T,         /* input : decoded pitch lags in all subframes          */
  int Vad
)
{
  /*-------------------------------------------------------------------*
   *           Declaration of parameters                               *
   *-------------------------------------------------------------------*/

  GFLOAT res2_pst[L_SUBFR];  /* res2[] after pitch postfiltering */
  GFLOAT syn_pst[L_FRAME];   /* post filtered synthesis speech   */

  GFLOAT ap3[MP1], ap4[MP1]; /* bandwidth expanded LP parameters */

  GFLOAT *az;                /* pointer to Az_4:
                               LPC parameters in each subframe   */
  int   t0_max, t0_min;     /* closed-loop pitch search range  */
  int   i_subfr;            /* index for beginning of subframe */

  GFLOAT temp1, temp2;
  GFLOAT h[L_H];

  int   i;

  az = az_4;

  for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
  {
    /* Find pitch range t0_min - t0_max */

    t0_min = *T++ - 3;
    t0_max = t0_min+6;
    if (t0_max > PIT_MAX) {
      t0_max = PIT_MAX;
      t0_min = t0_max-6;
    }

    /* Find weighted filter coefficients ap3[] and ap[4] */

    weight_az(az, GAMMA2_PST, M, ap3);
    weight_az(az, GAMMA1_PST, M, ap4 );

    /* filtering of synthesis speech by A(z/GAMMA2_PST) to find res2[] */

    residu(ap3, &syn[i_subfr], state->res2, L_SUBFR);

    /* pitch postfiltering */
	if (Vad == 1)
		pit_pst_filt(state->res2, t0_min, t0_max, L_SUBFR, res2_pst);
	else
		for (i=0; i<L_SUBFR; i++)
			res2_pst[i] = state->res2[i];

    /* tilt compensation filter */

    /* impulse response of A(z/GAMMA2_PST)/A(z/GAMMA1_PST) */

    copy(ap3, h, MP1);
    set_zero(&h[MP1],L_H-MP1);
    syn_filt(ap4, h, h, L_H, &h[M+1], 0);

    /* 1st correlation of h[] */
    temp1 = (F)0.0;
    for (i=0; i<L_H; i++) temp1 += h[i]*h[i];
    temp2 = (F)0.0;
    for (i=0; i<L_H-1; i++) temp2 += h[i]*h[i+1];
    if(temp2 <= (F)0.0) {
      temp2 = (F)0.0;
    }
    else {
       temp2 = temp2*MU/temp1;
    }
    preemphasis(state, res2_pst, temp2, L_SUBFR);

    /* filtering through  1/A(z/GAMMA1_PST) */

    syn_filt(ap4, res2_pst, &syn_pst[i_subfr], L_SUBFR, state->mem_syn_pst, 1);

    /* scale output to input */

    agc(state, &syn[i_subfr], &syn_pst[i_subfr], L_SUBFR);

    /* update res2[] buffer;  shift by L_SUBFR */

    copy(&state->res2[L_SUBFR-PIT_MAX], &state->res2[-PIT_MAX], PIT_MAX);

    az += MP1;
  }


  /* update syn[] buffer */

  copy(&syn[L_FRAME-M], &syn[-M], M);

  /* overwrite synthesis speech by postfiltered synthesis speech */

  copy(syn_pst, syn, L_FRAME);
}

/*---------------------------------------------------------------------------*
 * procedure pit_pst_filt                                                    *
 * ~~~~~~~~~~~~~~~~~~~~~~                                                    *
 * Find the pitch period  around the transmitted pitch and perform           *
 * harmonic postfiltering.                                                   *
 * Filtering through   (1 + g z^-T) / (1+g) ;   g = min(pit_gain*gammap, 1)  *
 *--------------------------------------------------------------------------*/

static void pit_pst_filt(
  GFLOAT *signal,        /* input : input signal                        */
  int   t0_min,         /* input : minimum value in the searched range */
  int   t0_max,         /* input : maximum value in the searched range */
  int   L_subfr,        /* input : size of filtering                   */
  GFLOAT *signal_pst     /* output: harmonically postfiltered signal    */
)
{
  int    i, j;
  int    t0=0;
  GFLOAT  cor_max;
  GFLOAT  temp, g0, gain;
  GFLOAT  ener, corr, ener0;
  GFLOAT  *p, *p1, *deb_sig;

/*---------------------------------------------------------------------------*
 * Compute the correlations for all delays                                   *
 * and select the delay which maximizes the correlation                      *
 *---------------------------------------------------------------------------*/

  deb_sig = &signal[-t0_min];
  cor_max = FLT_MIN_G729;

  for (i=t0_min; i<=t0_max; i++)
  {
    corr = (F)0.0;
    p    = signal;
    p1   = deb_sig;
    for (j=0; j<L_subfr; j++)
       corr += (*p++) * (*p1++);
    if (corr>cor_max)
    {
      cor_max = corr;
      t0 = i;
    }
    deb_sig--;
  }

  /* Compute the energy of the signal delayed by t0 */

  ener = (F)0.5;
  p = signal - t0;
  for ( i=0; i<L_subfr ;i++, p++)
    ener += (*p) * (*p);

  /* Compute the signal energy in the present subframe */

  ener0 = (F)0.5;
  p = signal;

  for ( i=0; i<L_subfr; i++, p++)
    ener0 += (*p) * (*p);

  if (cor_max < (F)0.0) cor_max = (F)0.0;

  /* prediction gain (dB)= -10 log(1-cor_max*cor_max/(ener*ener0)) */

  temp = cor_max*cor_max;
  if (temp < ener*ener0*0.5)        /* if prediction gain < 3 dB   */
  {                                 /* switch off pitch postfilter */
     for (i = 0; i < L_subfr; i++)
               signal_pst[i] = signal[i];
     return;
  }

  if (cor_max > ener)     /* if pitch gain > 1 */
  {
     g0 = INV_GAMMAP;
     gain = GAMMAP_2;
  }
  else
  {
    cor_max *= GAMMAP;
    temp = (F)1.0/(cor_max+ener);
    gain = temp * cor_max;
    g0   = (F)1.0 - gain;
  }


  for (i = 0; i < L_subfr; i++)
    signal_pst[i] = g0*signal[i] + gain*signal[i-t0];
}


/*---------------------------------------------------------------------*
 * routine preemphasis()                                               *
 * ~~~~~~~~~~~~~~~~~~~~~                                               *
 * Preemphasis: filtering through 1 - g z^-1                           *
 *---------------------------------------------------------------------*/

static void preemphasis(struct postfilt_state_t * state,
  GFLOAT *signal,    /* in/out: input signal overwritten by the output */
  GFLOAT g,          /* input : preemphasis coefficient                */
  int L             /* input : size of filtering                      */
)
{
  GFLOAT *p1, *p2, temp;
  int   i;

  p1 = signal + L - 1;
  p2 = p1 - 1;
  temp = *p1;

  for (i = 0; i <= L-2; i++) {
    *p1 -= g * (*p2--); p1--; }

  *p1 = *p1 - g * state->mem_pre;

  state->mem_pre = temp;
}

/*----------------------------------------------------------------------*
 *   routine agc()                                                      *
 *   ~~~~~~~~~~~~~                                                      *
 * Scale the postfilter output on a subframe basis by automatic control *
 * of the subframe gain.                                                *
 *  gain[n] = AGC_FAC * gain[n-1] + (1 - AGC_FAC) g_in/g_out            *
 *----------------------------------------------------------------------*/

static void agc(struct postfilt_state_t * state,
  GFLOAT *sig_in,    /* input : postfilter input signal  */
  GFLOAT *sig_out,   /* in/out: postfilter output signal */
  int l_trm         /* input : subframe size            */
)
{
    int i;
    GFLOAT gain_in, gain_out;
    GFLOAT g0, gain;

    gain_out = (F)0.;
    for(i=0; i<l_trm; i++) {
            gain_out += sig_out[i]*sig_out[i];
    }
    if(gain_out == (F)0.) {
            state->past_gain = (F)0.;
            return;
    }

    gain_in = (F)0.;
    for(i=0; i<l_trm; i++) {
            gain_in += sig_in[i]*sig_in[i];
    }
    if(gain_in == (F)0.) {
            g0 = (F)0.;
    }
    else {
            g0 = (GFLOAT)sqrt(gain_in/ gain_out);
            g0 *=  AGC_FAC1;
    }

    /* compute gain(n) = AGC_FAC gain(n-1) + (1-AGC_FAC)gain_in/gain_out */
    /* sig_out(n) = gain(n) sig_out(n)                         */

    gain = state->past_gain;
    for(i=0; i<l_trm; i++) {
            gain *= AGC_FAC;
            gain += g0;
            sig_out[i] *= gain;
    }
    state->past_gain = gain;
}
