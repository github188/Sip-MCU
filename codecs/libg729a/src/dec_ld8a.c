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

/*-----------------------------------------------------------------*
 *   Functions init_decod_ld8a  and decod_ld8a                     *
 *-----------------------------------------------------------------*/

#include "typedef.h"
#include "ld8a.h"
#include "cst_ld8a.h"
#include "tab_ld8a.h"

/*---------------------------------------------------------------*
 *   Decoder constant parameters (defined in "ld8a.h")           *
 *---------------------------------------------------------------*
 *   L_FRAME     : Frame size.                                   *
 *   L_SUBFR     : Sub-frame size.                               *
 *   M           : LPC order.                                    *
 *   MP1         : LPC order+1                                   *
 *   PIT_MIN     : Minimum pitch lag.                            *
 *   PIT_MAX     : Maximum pitch lag.                            *
 *   L_INTERPOL  : Length of filter for interpolation            *
 *   PRM_SIZE    : Size of vector containing analysis parameters *
 *---------------------------------------------------------------*/


/*-----------------------------------------------------------------*
 *   Function init_decod_ld8a                                      *
 *            ~~~~~~~~~~~~~~~                                      *
 *                                                                 *
 *   ->Initialization of variables for the decoder section.        *
 *                                                                 *
 *-----------------------------------------------------------------*/

void init_decod_ld8a(struct dec_state_t * state)
{
  /* Initialize static pointer */

  state->exc = state->old_exc + PIT_MAX + L_INTERPOL;

  /* Static vectors to zero */

  set_zero(state->old_exc, PIT_MAX+L_INTERPOL);
  set_zero(state->mem_syn, M);

  state->sharp  = SHARPMIN;
  state->old_t0 = 60;
  state->gain_code = (F)0.0;
  state->gain_pitch = (F)0.0;

  lsp_decw_reset(&state->lsp_s);

  init_exc_err(state->cng_s.exc_err); // ?

  copy(lsp_reset, state->lsp_old, M);

  /* for G.729B */
  state->seed_fer = 21845;
  state->past_ftyp = 1;
  state->seed = INIT_SEED;
  state->sid_sav = (F)0.;
  init_lsfq_noise(&state->cng_s.lsfq_s); // ?

  gain_past_reset(&state->gain_s);

  state->bad_lsf = 0;          /* Initialize bad LSF indicator */
}

/*-----------------------------------------------------------------*
 *   Function decod_ld8a                                           *
 *           ~~~~~~~~~~                                            *
 *   ->Main decoder routine.                                       *
 *                                                                 *
 *-----------------------------------------------------------------*/

void decod_ld8a(struct dec_state_t * state,
  int      parm[],      /* (i)   : vector of synthesis parameters
                                  parm[0] = bad frame indicator (bfi)  */
  GFLOAT   synth[],     /* (o)   : synthesis speech                     */
  GFLOAT   A_t[],       /* (o)   : decoded LP filter in 2 subframes     */
  int     *T2,         /* (o)   : decoded pitch lag in 2 subframes     */
  int     *Vad         /* (o)   : decoded frame type     */
)
{
  GFLOAT   *Az;                  /* Pointer on A_t   */
  GFLOAT   lsp_new[M];           /* Decoded LSP's    */
  GFLOAT   code[L_SUBFR];        /* ACELP codevector */

   /* Scalars */

   int i, i_subfr;
   int t0, t0_frac, index;
   int  bfi, bad_pitch;

   /* for G.729B */
   int ftyp;
   GFLOAT lsfq_mem[MA_NP][M];

   /* Test bad frame indicator (bfi) */

   bfi = *parm++;
   
   /* for G.729B */
   ftyp = *parm;

   if (bfi)
   {
     if(state->past_ftyp == 1)
       ftyp = 1;
     else
       ftyp = 0;
     *parm = ftyp;  /* modification introduced in version V1.3 */
   }
  
   *Vad = ftyp;

  /* Processing non active frames (SID & not transmitted) */
  if(ftyp != 1)
  {
    get_freq_prev((const GFLOAT (*)[M]) state->lsp_s.freq_prev, lsfq_mem);
    dec_cng(&state->cng_s, state->past_ftyp, state->sid_sav, parm, state->exc, state->lsp_old,
            A_t, &state->seed, lsfq_mem);
    update_freq_prev(state->lsp_s.freq_prev, (const GFLOAT (*)[M]) lsfq_mem);

    Az = A_t;
    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
	{
      syn_filt(Az, &state->exc[i_subfr], &synth[i_subfr], L_SUBFR, state->mem_syn, 0);
      copy(&synth[i_subfr+L_SUBFR-M], state->mem_syn, M);
      
      Az += MP1;

      *T2++ = state->old_t0;
    }
    state->sharp = SHARPMIN;
    
  }
  else /* Processing active frame */
  {
    
    state->seed = INIT_SEED;
    parm++;

    /* Decode the LSPs */

    d_lsp(&state->lsp_s, parm, lsp_new, bfi+state->bad_lsf );
    parm += 2;                        /* Advance synthesis parameters pointer */

    /*
    Note: "bad_lsf" is introduce in case the standard is used with
           channel protection.
    */

    /* Interpolation of LPC for the 2 subframes */

    int_qlpc(state->lsp_old, lsp_new, A_t);

    /* update the LSFs for the next frame */

    copy(lsp_new, state->lsp_old, M);

    /*------------------------------------------------------------------------*
     *          Loop for every subframe in the analysis frame                 *
     *------------------------------------------------------------------------*
     * The subframe size is L_SUBFR and the loop is repeated L_FRAME/L_SUBFR  *
     *  times                                                                 *
     *     - decode the pitch delay                                           *
     *     - decode algebraic code                                            *
     *     - decode pitch and codebook gains                                  *
     *     - find the excitation and compute synthesis speech                 *
     *------------------------------------------------------------------------*/

    Az = A_t;                     /* pointer to interpolated LPC parameters */

    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
	{
      /*-------------------------------------------------*
       *  - Find the adaptive codebook vector.            *
       *--------------------------------------------------*/

      index = *parm++;            /* pitch index */

      if (i_subfr == 0)
      {

        i = *parm++; /* get parity check result */
        bad_pitch = bfi + i;
        if( bad_pitch == 0)
        {
            dec_lag3(index, PIT_MIN, PIT_MAX, i_subfr, &t0, &t0_frac);
            state->old_t0 = t0;
        }
        else        /* Bad frame, or parity error */
        {
          t0  =  state->old_t0;
          t0_frac = 0;
          state->old_t0++;
          if( (state->old_t0 - PIT_MAX) > 0)
            state->old_t0 = PIT_MAX;
        }

      }
      else                  /* second subframe */
      {
        if( bfi == 0)
        {
          dec_lag3(index, PIT_MIN, PIT_MAX, i_subfr, &t0, &t0_frac);
          state->old_t0 = t0;
        }
        else
        {
          t0  =  state->old_t0;
          t0_frac = 0;
          state->old_t0++;
          if( (state->old_t0 - PIT_MAX) > 0)
            state->old_t0 = PIT_MAX;
        }
      }
      *T2++ = t0;

     /*-------------------------------------------------*
      * - Find the adaptive codebook vector.            *
      *-------------------------------------------------*/

      pred_lt_3(&state->exc[i_subfr], t0, t0_frac, L_SUBFR);

      /*-------------------------------------------------------*
       * - Decode innovative codebook.                         *
       * - Add the fixed-gain pitch contribution to code[].    *
       *-------------------------------------------------------*/

      if(bfi != 0)        /* Bad frame */
      {
        parm[0] = random_g729(&state->seed_fer) & (INT16)0x1fff;     /* 13 bits random */
        parm[1] = random_g729(&state->seed_fer) & (INT16)0x000f;     /*  4 bits random */
      }

      decod_ACELP(parm[1], parm[0], code);
      parm +=2;

      for (i = t0; i < L_SUBFR; i++)   code[i] += state->sharp * code[i-t0];

      /*-------------------------------------------------*
       * - Decode pitch and codebook gains.              *
       *-------------------------------------------------*/

      index = *parm++;          /* index of energy VQ */

      dec_gain(&state->gain_s, index, code, L_SUBFR, bfi, &state->gain_pitch, &state->gain_code);

      /*-------------------------------------------------------------*
       * - Update pitch sharpening "sharp" with quantized gain_pitch *
       *-------------------------------------------------------------*/

      state->sharp = state->gain_pitch;
      if (state->sharp > SHARPMAX) state->sharp = SHARPMAX;
      if (state->sharp < SHARPMIN) state->sharp = SHARPMIN;

      /*-------------------------------------------------------*
       * - Find the total excitation.                          *
       * - Find synthesis speech corresponding to exc[].       *
       *-------------------------------------------------------*/

      for (i = 0; i < L_SUBFR;  i++)
         state->exc[i+i_subfr] = state->gain_pitch*state->exc[i+i_subfr] + state->gain_code*code[i];

      syn_filt(Az, &state->exc[i_subfr], &synth[i_subfr], L_SUBFR, state->mem_syn, 1);

      Az  += MP1;        /* interpolated LPC parameters for next subframe */
    }
  }

  /*------------*
   *  For G729b
   *-----------*/
  if (bfi == 0)
  {
     state->sid_sav = (F)0.0;
     for (i=0; i<L_FRAME; i++)
        state->sid_sav += state->exc[i] * state->exc[i];
  }
  state->past_ftyp = ftyp;

  /*--------------------------------------------------*
   * Update signal for next frame.                    *
   * -> shift to the left by L_FRAME  exc[]           *
   *--------------------------------------------------*/

   copy(&state->old_exc[L_FRAME], &state->old_exc[0], PIT_MAX+L_INTERPOL);
}
