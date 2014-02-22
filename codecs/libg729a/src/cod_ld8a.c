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
 File : COD_LD8A.C
 Used for the floating point version of G.729A only
 (not for G.729 main body)
*/

/*-----------------------------------------------------------------*
 *   Functions coder_ld8a and init_coder_ld8a                      *
 *             ~~~~~~~~~~     ~~~~~~~~~~~~~~~                      *
 *                                                                 *
 *  init_coder_ld8a(void);                                         *
 *                                                                 *
 *   ->Initialization of variables for the coder section.          *
 *                                                                 *
 *                                                                 *
 *  coder_ld8a(Short ana[]);                                       *
 *                                                                 *
 *   ->Main coder function.                                        *
 *                                                                 *
 *                                                                 *
 *  Input:                                                         *
 *                                                                 *
 *    80 speech data should have beee copy to vector new_speech[]. *
 *    This vector is global and is declared in this function.      *
 *                                                                 *
 *  Ouputs:                                                        *
 *                                                                 *
 *    ana[]      ->analysis parameters.                            *
 *                                                                 *
 *-----------------------------------------------------------------*/

#include <math.h>
#include "typedef.h"
#include "ld8a.h"
#include "cst_ld8a.h"
#include "tab_ld8a.h"

/*-----------------------------------------------------------*
 *    Coder constant parameters (defined in "ld8a.h")        *
 *-----------------------------------------------------------*
 *   L_WINDOW    : LPC analysis window size.                 *
 *   L_NEXT      : Samples of next frame needed for autocor. *
 *   L_FRAME     : Frame size.                               *
 *   L_SUBFR     : Sub-frame size.                           *
 *   M           : LPC order.                                *
 *   MP1         : LPC order+1                               *
 *   L_TOTAL     : Total size of speech buffer.              *
 *   PIT_MIN     : Minimum pitch lag.                        *
 *   PIT_MAX     : Maximum pitch lag.                        *
 *   L_INTERPOL  : Length of filter for interpolation        *
 *-----------------------------------------------------------*/

/*-----------------------------------------------------------------*
 *   Function  init_coder_ld8a                                     *
 *            ~~~~~~~~~~~~~~~                                      *
 *                                                                 *
 *  init_coder_ld8a(void);                                         *
 *                                                                 *
 *   ->Initialization of variables for the coder section.          *
 *       - initialize pointers to speech buffer                    *
 *       - initialize static  pointers                             *
 *       - set static vectors to zero                              *
 *-----------------------------------------------------------------*/

void init_coder_ld8a(struct cod_state_t * state)
{

  /*----------------------------------------------------------------------*
  *      Initialize pointers to speech vector.                            *
  *                                                                       *
  *                                                                       *
  *   |--------------------|-------------|-------------|------------|     *
  *     previous speech           sf1           sf2         L_NEXT        *
  *                                                                       *
  *   <----------------  Total speech vector (L_TOTAL)   ----------->     *
  *   <----------------  LPC analysis window (L_WINDOW)  ----------->     *
  *   |                   <-- present frame (L_FRAME) -->                 *
  * old_speech            |              <-- new speech (L_FRAME) -->     *
  * p_window              |              |                                *
  *                     speech           |                                *
  *                             new_speech                                *
  *-----------------------------------------------------------------------*/

	state->new_speech = state->old_speech + L_TOTAL - L_FRAME;         /* New speech     */
	state->speech     = state->new_speech - L_NEXT;                    /* Present frame  */
	state->p_window   = state->old_speech + L_TOTAL - L_WINDOW;        /* For LPC window */

	/* Initialize static pointers */

	state->wsp    = state->old_wsp + PIT_MAX;
	state->exc    = state->old_exc + PIT_MAX + L_INTERPOL;

	/* Static vectors to zero */

	set_zero(state->old_speech, L_TOTAL);
	set_zero(state->old_exc, PIT_MAX+L_INTERPOL);
	set_zero(state->old_wsp, PIT_MAX);
	set_zero(state->mem_w,   M);
	set_zero(state->mem_w0,  M);
	set_zero(state->mem_zero, M);
	state->sharp = SHARPMIN;

	copy(lsp_reset, state->lsp_old, M);
	copy(state->lsp_old, state->lsp_old_q, M);
	lsp_codw_reset(&state->lsp_s);

	init_exc_err(state->cng_s.exc_err); // ?

	/* For G.729B */
	/* Initialize VAD/DTX parameters */
	state->pastVad = 1;
	state->ppastVad = 1;
	state->seed = INIT_SEED;
	vad_init(&state->vad_s);

	init_lsfq_noise(&state->cng_s.lsfq_s); // ?

	gain_past_reset(&state->gain_s);
}

/*-----------------------------------------------------------------*
 *   Function coder_ld8a                                           *
 *            ~~~~~~~~~~                                           *
 *  coder_ld8a(int    ana[]);                                      *
 *                                                                 *
 *   ->Main coder function.                                        *
 *                                                                 *
 *                                                                 *
 *  Input:                                                         *
 *                                                                 *
 *    80 speech data should have beee copy to vector new_speech[]. *
 *    This vector is global and is declared in this function.      *
 *                                                                 *
 *  Ouputs:                                                        *
 *                                                                 *
 *    ana[]      ->analysis parameters.                            *
 *                                                                 *
 *-----------------------------------------------------------------*/

void coder_ld8a(struct cod_state_t * state,
  int ana[],        /* output: analysis parameters */
  int frame,        /* input   : frame counter       */
  int vad_enable    /* input   : VAD enable flag     */
)
{
   /* LPC coefficients */

   GFLOAT Aq_t[(MP1)*2];         /* A(z) quantized for the 2 subframes   */
   GFLOAT Ap_t[(MP1)*2];         /* A(z) with spectral expansion         */
   GFLOAT *Aq, *Ap;              /* Pointer on Aq_t  and Ap_t            */

   /* Other vectors */

   GFLOAT h1[L_SUBFR];           /* Impulse response h1[]              */
   GFLOAT xn[L_SUBFR];           /* Target vector for pitch search     */
   GFLOAT xn2[L_SUBFR];          /* Target vector for codebook search  */
   GFLOAT code[L_SUBFR];         /* Fixed codebook excitation          */
   GFLOAT y1[L_SUBFR];           /* Filtered adaptive excitation       */
   GFLOAT y2[L_SUBFR];           /* Filtered fixed codebook excitation */
   GFLOAT g_coeff[5];            /* Correlations between xn, y1, & y2:
                                   <y1,y1>, <xn,y1>, <y2,y2>, <xn,y2>,<y1,y2>*/

   /* Scalars */

   int i, j, i_subfr;
   int T_op, T0, T0_min, T0_max, T0_frac;
   int index;
   GFLOAT   gain_pit, gain_code;
   int     taming;

/*------------------------------------------------------------------------*
 *  - Perform LPC analysis:                                               *
 *       * autocorrelation + lag windowing                                *
 *       * Levinson-durbin algorithm to find a[]                          *
 *       * convert a[] to lsp[]                                           *
 *       * quantize and code the LSPs                                     *
 *       * find the interpolated LSPs and convert to a[] for the 2        *
 *         subframes (both quantized and unquantized)                     *
 *------------------------------------------------------------------------*/
   {
      /* Temporary vectors */
     GFLOAT r[NP+1];                   /* Autocorrelations       */
     GFLOAT rc[M];                     /* Reflexion coefficients */
     GFLOAT lsp_new[M];                /* lsp coefficients       */
     GFLOAT lsp_new_q[M];              /* Quantized lsp coeff.   */
	 GFLOAT lsf_new[M];
 
	 /* For G.729B */
     GFLOAT r_nbe[MP1];
     GFLOAT lsfq_mem[MA_NP][M];
     int Vad;
     GFLOAT Energy_db;

     /* LP analysis */
     autocorr(state->p_window, NP, r);        /* Autocorrelations */
     copy(r, r_nbe, MP1);
     g729_lag_window(NP, r);                       /* Lag windowing    */
     levinson(r, Ap_t, rc);                   /* Levinson Durbin  */
     az_lsp(Ap_t, lsp_new, state->lsp_old);   /* Convert A(z) to lsp */

	/* For G.729B */
	/* ------ VAD ------- */
	if (vad_enable)
	{
		lsp_lsf(lsp_new, lsf_new, M);
		vad(&state->vad_s, rc[1], lsf_new, r, state->p_window, frame,
			state->pastVad, state->ppastVad, &Vad, &Energy_db);

		update_cng(&state->cng_s, r_nbe, Vad);
	}
	else
		Vad = 1;


	/* ---------------------- */
	/* Case of Inactive frame */
	/* ---------------------- */

	if (!Vad)
	{
	  get_freq_prev((const GFLOAT (*)[M]) state->lsp_s.freq_prev, lsfq_mem);
	  cod_cng(&state->cng_s, state->exc, state->pastVad, state->lsp_old_q, Aq_t, ana, lsfq_mem, &state->seed);
	  update_freq_prev(state->lsp_s.freq_prev, (const GFLOAT (*)[M]) lsfq_mem);
	  state->ppastVad = state->pastVad;
	  state->pastVad = Vad;

	  /* Update wsp, mem_w and mem_w0 */
	  Aq = Aq_t;
	  for (i_subfr=0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
	  {
		/* Residual signal in xn */
		residu(Aq, &state->speech[i_subfr], xn, L_SUBFR);
    
		weight_az(Aq, GAMMA1, M, Ap_t);
    
		/* Compute wsp and mem_w */
		Ap = Ap_t + MP1;
		Ap[0] = 0.125;
		for (i=1; i<=M; i++)    /* Ap[i] = Ap_t[i] - 0.7 * Ap_t[i-1]; */
		  Ap[i] = (F)(Ap_t[i] - 0.7 * Ap_t[i-1]);
		syn_filt(Ap, xn, &state->wsp[i_subfr], L_SUBFR, state->mem_w, 1);
    
		/* Compute mem_w0 */
		for (i=0; i<L_SUBFR; i++) {
		  xn[i] = xn[i] - state->exc[i_subfr+i];  /* residu[] - exc[] */
		}
		syn_filt(Ap_t, xn, xn, L_SUBFR, state->mem_w0, 1);
            
		Aq += MP1;
	  }
  
	  state->sharp = SHARPMIN;
  
	  /* Update memories for next frames */
	  copy(&state->old_speech[L_FRAME], &state->old_speech[0], L_TOTAL-L_FRAME);
	  copy(&state->old_wsp[L_FRAME], &state->old_wsp[0], PIT_MAX);
	  copy(&state->old_exc[L_FRAME], &state->old_exc[0], PIT_MAX+L_INTERPOL);
  
	  return;
	}  /* End of inactive frame case */


	/* -------------------- */
	/* Case of Active frame */
	/* -------------------- */
    
	*ana++ = 1;
	state->seed = INIT_SEED;
	state->ppastVad = state->pastVad;
	state->pastVad = Vad;

     /* LSP quantization */
     qua_lsp(&state->lsp_s, lsp_new, lsp_new_q, ana);
     ana += 2;                        /* Advance analysis parameters pointer */

    /*--------------------------------------------------------------------*
     * Find interpolated LPC parameters in all subframes                  *
     * The interpolated parameters are in array Aq_t[].                   *
     *--------------------------------------------------------------------*/

    int_qlpc(state->lsp_old_q, lsp_new_q, Aq_t);

    /* Compute A(z/gamma) */
    weight_az(&Aq_t[0],   GAMMA1, M, &Ap_t[0]);
    weight_az(&Aq_t[MP1], GAMMA1, M, &Ap_t[MP1]);

    /* update the LSPs for the next frame */
    copy(lsp_new,   state->lsp_old,   M);
    copy(lsp_new_q, state->lsp_old_q, M);
  }

   /*----------------------------------------------------------------------*
    * - Find the weighted input speech w_sp[] for the whole speech frame   *
    * - Find the open-loop pitch delay for the whole speech frame          *
    * - Set the range for searching closed-loop pitch in 1st subframe      *
    *----------------------------------------------------------------------*/

   residu(&Aq_t[0],   &state->speech[0],       &state->exc[0],       L_SUBFR);
   residu(&Aq_t[MP1], &state->speech[L_SUBFR], &state->exc[L_SUBFR], L_SUBFR);

  {
     GFLOAT Ap1[MP1];

     Ap = Ap_t;
     Ap1[0] = (F)1.0;
     for(i=1; i<=M; i++)
       Ap1[i] = Ap[i] - (F)0.7 * Ap[i-1];
     syn_filt(Ap1, &state->exc[0], &state->wsp[0], L_SUBFR, state->mem_w, 1);

     Ap += MP1;
     for(i=1; i<=M; i++)
       Ap1[i] = Ap[i] - (F)0.7 * Ap[i-1];
     syn_filt(Ap1, &state->exc[L_SUBFR], &state->wsp[L_SUBFR], L_SUBFR, state->mem_w, 1);
   }


   /* Find open loop pitch lag for whole speech frame */

   T_op = pitch_ol_fast(state->wsp, L_FRAME);

   /* Range for closed loop pitch search in 1st subframe */

   T0_min = T_op - 3;
   if (T0_min < PIT_MIN) T0_min = PIT_MIN;
   T0_max = T0_min + 6;
   if (T0_max > PIT_MAX)
   {
      T0_max = PIT_MAX;
      T0_min = T0_max - 6;
   }

   /*------------------------------------------------------------------------*
    *          Loop for every subframe in the analysis frame                 *
    *------------------------------------------------------------------------*
    *  To find the pitch and innovation parameters. The subframe size is     *
    *  L_SUBFR and the loop is repeated L_FRAME/L_SUBFR times.               *
    *     - find the weighted LPC coefficients                               *
    *     - find the LPC residual signal                                     *
    *     - compute the target signal for pitch search                       *
    *     - compute impulse response of weighted synthesis filter (h1[])     *
    *     - find the closed-loop pitch parameters                            *
    *     - encode the pitch delay                                           *
    *     - find target vector for codebook search                           *
    *     - codebook search                                                  *
    *     - VQ of pitch and codebook gains                                   *
    *     - update states of weighting filter                                *
    *------------------------------------------------------------------------*/

   Aq = Aq_t;    /* pointer to interpolated quantized LPC parameters */
   Ap = Ap_t;    /* pointer to weighted LPC coefficients             */

   for (i_subfr = 0;  i_subfr < L_FRAME; i_subfr += L_SUBFR)
   {
      /*---------------------------------------------------------------*
       * Compute impulse response, h1[], of weighted synthesis filter  *
       *---------------------------------------------------------------*/

      h1[0] = (F)1.0;
      set_zero(&h1[1], L_SUBFR-1);
      syn_filt(Ap, h1, h1, L_SUBFR, &h1[1], 0);

      /*-----------------------------------------------*
       * Find the target vector for pitch search:      *
       *----------------------------------------------*/

      syn_filt(Ap, &state->exc[i_subfr], xn, L_SUBFR, state->mem_w0, 0);

      /*-----------------------------------------------------------------*
       *    Closed-loop fractional pitch search                          *
       *-----------------------------------------------------------------*/

      T0 = pitch_fr3_fast(&state->exc[i_subfr], xn, h1, L_SUBFR, T0_min, T0_max,
                    i_subfr, &T0_frac);

      index = enc_lag3(T0, T0_frac, &T0_min, &T0_max, PIT_MIN, PIT_MAX,
                            i_subfr);

      *ana++ = index;

      if (i_subfr == 0)
        *ana++ = parity_pitch(index);

      /*-----------------------------------------------------------------*
       *   - find filtered pitch exc                                     *
       *   - compute pitch gain and limit between 0 and 1.2              *
       *   - update target vector for codebook search                    *
       *   - find LTP residual.                                          *
       *-----------------------------------------------------------------*/

      syn_filt(Ap, &state->exc[i_subfr], y1, L_SUBFR, state->mem_zero, 0);

      gain_pit = g_pitch(xn, y1, g_coeff, L_SUBFR);

      /* clip pitch gain if taming is necessary */

      taming = test_err(state->cng_s.exc_err, T0, T0_frac);

      if( taming == 1){
        if (gain_pit > GPCLIP) {
          gain_pit = GPCLIP;
      }
    }

    for (i = 0; i < L_SUBFR; i++)
        xn2[i] = xn[i] - y1[i]*gain_pit;

      /*-----------------------------------------------------*
       * - Innovative codebook search.                       *
       *-----------------------------------------------------*/

      index = ACELP_code_A(xn2, h1, T0, state->sharp, code, y2, &i);

      *ana++ = index;           /* Positions index */
      *ana++ = i;               /* Signs index     */

      /*------------------------------------------------------*
       *  - Compute the correlations <y2,y2>, <xn,y2>, <y1,y2>*
       *  - Vector quantize gains.                            *
       *------------------------------------------------------*/

      corr_xy2(xn, y1, y2, g_coeff);
      *ana++ = qua_gain(&state->gain_s, code, g_coeff, L_SUBFR, &gain_pit, &gain_code,
                                    taming);

      /*------------------------------------------------------------*
       * - Update pitch sharpening "sharp" with quantized gain_pit  *
       *------------------------------------------------------------*/

      state->sharp = gain_pit;
      if (state->sharp > SHARPMAX) state->sharp = SHARPMAX;
      if (state->sharp < SHARPMIN) state->sharp = SHARPMIN;

      /*------------------------------------------------------*
       * - Find the total excitation                          *
       * - update filters' memories for finding the target    *
       *   vector in the next subframe  (mem_w0[])            *
       *------------------------------------------------------*/

      for (i = 0; i < L_SUBFR;  i++)
        state->exc[i+i_subfr] = gain_pit*state->exc[i+i_subfr] + gain_code*code[i];

      update_exc_err(state->cng_s.exc_err, gain_pit, T0);

      for (i = L_SUBFR-M, j = 0; i < L_SUBFR; i++, j++)
        state->mem_w0[j]  = xn[i] - gain_pit*y1[i] - gain_code*y2[i];

      Aq += MP1;           /* interpolated LPC parameters for next subframe */
      Ap += MP1;
   }

   /*--------------------------------------------------*
    * Update signal for next frame.                    *
    * -> shift to the left by L_FRAME:                 *
    *     speech[], wsp[] and  exc[]                   *
    *--------------------------------------------------*/

   copy(&state->old_speech[L_FRAME], &state->old_speech[0], L_TOTAL-L_FRAME);
   copy(&state->old_wsp[L_FRAME], &state->old_wsp[0], PIT_MAX);
   copy(&state->old_exc[L_FRAME], &state->old_exc[0], PIT_MAX+L_INTERPOL);
}
