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


/*-----------------------------------------------------------*
 * ld8a.h - include file for G.729a 8.0 kb/s codec           *
 *-----------------------------------------------------------*/

#ifndef _LD8A_H_INCL
#define _LD8A_H_INCL

#include <stdio.h>
#include <stdlib.h>

#ifdef INLINE_FUNCS
#ifdef _MSC_VER
#	define __inline__ __inline
#endif
#define INLINE_PREFIX static __inline__
#else /* INLINE_FUNCS */
#define INLINE_PREFIX
#endif /* INLINE_FUNCS */

#ifdef PI
#undef PI
#endif
#ifdef PI2
#undef PI2
#endif
#define PI              (F)3.141592654
#define PI2             (F)6.283185307
#define FLT_MAX_G729    (F)1.0e38    /* largest floating point number      */
#define FLT_MIN_G729    (F)-1.0e38   /* largest floating point number      */

/*--------------------------------------------------------------------------*
 *       Codec constant parameters (coder, decoder, and postfilter)         *
 *--------------------------------------------------------------------------*/

#define L_TOTAL         240     /* Total size of speech buffer              */
#define L_WINDOW        240     /* LPC analysis window size                 */
#define L_NEXT          40      /* Samples of next frame needed for LPC ana.*/
#define L_FRAME         80      /* LPC update frame size                    */
#define L_SUBFR         40      /* Sub-frame size                           */

#define PIT_MIN         20      /* Minimum pitch lag in samples             */
#define PIT_MAX         143     /* Maximum pitch lag in samples             */
#define L_INTERPOL      (10+1)  /* Length of filter for interpolation.      */
#define GAMMA1       (F)0.75    /* Bandwitdh expansion for W(z)             */


/*--------------------------------------------------------------------------*
 * constants for lpc analysis and lsp quantizer                             *
 *--------------------------------------------------------------------------*/

#define M               10      /* LPC order                                */
#define MP1             (M+1)   /* LPC order+1                              */
#define NC              (M/2)   /* LPC order / 2                            */
#define WNC          (F)1.0001  /* white noise correction factor            */
#define GRID_POINTS      50     /* resolution of lsp search                 */

#define MA_NP           4       /* MA prediction order for LSP              */
#define MODE            2       /* number of modes for MA prediction        */
#define NC0_B           7       /* number of bits in first stage            */
#define NC0          (1<<NC0_B) /* number of entries in first stage         */
#define NC1_B           5       /* number of bits in second stage           */
#define NC1          (1<<NC1_B) /* number of entries in second stage        */

#define L_LIMIT         (F)0.005   /*  */
#define M_LIMIT         (F)3.135   /*  */
#define GAP1            (F)0.0012  /*  */
#define GAP2            (F)0.0006  /*  */
#define GAP3            (F)0.0392  /*  */
#define PI04            PI*(F)0.04   /* pi*0.04 */
#define PI92            PI*(F)0.92   /* pi*0.92 */
#define CONST12         (F)1.2

/*------------------------------------------------------------------*
 *  Constants for long-term predictor                               *
 *------------------------------------------------------------------*/

#define  SHARPMAX       (F)0.7945  /* Maximum value of pitch sharpening */
#define  SHARPMIN       (F)0.2     /* Minimum value of pitch sharpening */
#define  GAIN_PIT_MAX   (F)1.2     /* Maximum adaptive codebook gain    */

#define  UP_SAMP         3         /* Resolution of fractional delays   */
#define  L_INTER10       10        /* Length for pitch interpolation    */
#define  FIR_SIZE_SYN    (UP_SAMP*L_INTER10+1)

/*-----------------------*
 * Innovative codebook.  *
 *-----------------------*/

#define DIM_RR  616 /* size of correlation matrix                            */
#define NB_POS  8   /* Number of positions for each pulse                    */
#define STEP    5   /* Step betweem position of the same pulse.              */
#define MSIZE   64  /* Size of vectors for cross-correlation between 2 pulses*/

/*------------------------------------------------------------------*
 *  gain quantizer                                                  *
 *------------------------------------------------------------------*/

#define MEAN_ENER     (F)36.0   /* Average innovation energy */
#define NCODE1           8      /* Codebook 1 size */
#define NCODE2           16     /* Codebook 2 size */
#define NCAN1            4      /* Pre-selecting order for #1 */
#define NCAN2            8      /* Pre-selecting order for #2 */
#define INV_COEF      (F)-0.032623

/*------------------------------------------------------------------*
 *  Constant for postfilter                                         *
 *------------------------------------------------------------------*/

#define  GAMMA2_PST  (F)0.55       /* Formant postfilt factor (numerator)   */
#define  GAMMA1_PST  (F)0.70       /* Formant postfilt factor (denominator) */
#define  GAMMAP      (F)0.50       /* Harmonic postfilt factor              */
#define  INV_GAMMAP  ((F)1.0/((F)1.0+GAMMAP))
#define  GAMMAP_2    (GAMMAP/((F)1.0+GAMMAP))

#define  MU          (F)0.8        /* Factor for tilt compensation filter   */
#define  AGC_FAC     (F)0.9        /* Factor for automatic gain control     */
#define  AGC_FAC1     ((F)1.-AGC_FAC)
#define  L_H 22   /* size of truncated impulse response of A(z/g1)/A(z/g2) */

/*--------------------------------------------------------------------------*
 * Constants for taming procedure.                           *
 *--------------------------------------------------------------------------*/

#define GPCLIP      (F)0.95     /* Maximum pitch gain if taming is needed */
#define GPCLIP2     (F)0.94     /* Maximum pitch gain if taming is needed */
#define GP0999      (F)0.9999   /* Maximum pitch gain if taming is needed    */
#define THRESH_ERR  (F)60000.   /* Error threshold taming    */
#define INV_L_SUBFR (GFLOAT) ((F)1./(GFLOAT)L_SUBFR) /* =0.025 */

/*-----------------------*
 * Bitstream constants   *
 *-----------------------*/

#define BIT_0     (INT16)0x007f /* definition of zero-bit in bit-stream     */
#define BIT_1     (INT16)0x0081 /* definition of one-bit in bit-stream      */
#define SYNC_WORD (INT16)0x6b21 /* definition of frame erasure flag         */
#define PRM_SIZE        11      /* number of parameters per 10 ms frame     */
#define SERIAL_SIZE     82      /* bits per frame                           */
#define SIZE_WORD (INT16)80     /* number of speech bits                     */

/*-------------------------------*
 * Pre and post-process functions*
 *-------------------------------*/
struct preproc_state_t
{
	GFLOAT x0, x1;         /* high-pass fir memory          */
	GFLOAT y1, y2;         /* high-pass iir memory          */
};

void init_post_process(struct preproc_state_t *);

void post_process(struct preproc_state_t *,
   GFLOAT signal[],      /* (i/o)  : signal           */
   int lg               /* (i)    : lenght of signal */
);

void init_pre_process(struct preproc_state_t *);

void pre_process(struct preproc_state_t *,
   GFLOAT signal[],      /* (i/o)  : signal           */
   int lg               /* (i)    : lenght of signal */
);

/*--------------------------------------------------------------------------*
 *       VAD                                                                *
 *--------------------------------------------------------------------------*/
#define EPSI            (F)1.0e-38   /* very small positive floating point number      */

/*----------------------------------*
 * Main coder and decoder functions *
 *----------------------------------*/

struct cod_state_t;
struct dec_state_t;

void  init_coder_ld8a(struct cod_state_t *);

void coder_ld8a(struct cod_state_t *,
  int ana[],        /* output: analysis parameters */
  int frame,        /* input   : frame counter       */
  int vad_enable    /* input   : VAD enable flag     */
);

void  init_decod_ld8a(struct dec_state_t *);

void  decod_ld8a(struct dec_state_t *,
  int parm[],          /* (i)   : vector of synthesis parameters
                                  parm[0] = bad frame indicator (bfi)  */
  GFLOAT   synth[],     /* (o)   : synthesis speech                     */
  GFLOAT   A_t[],       /* (o)   : decoded LP filter in 2 subframes     */
  int *T2,             /* (o)   : decoded pitch lag in 2 subframes     */
  int *Vad             /* (o)   : decoded frame type     */
);

/*-------------------------------*
 * LPC analysis and filtering.   *
 *-------------------------------*/
void  autocorr(GFLOAT *x, int m, GFLOAT *r);

void  g729_lag_window(int m, GFLOAT r[]);

GFLOAT levinson(GFLOAT *a, GFLOAT *r, GFLOAT *r_c);

void  az_lsp(GFLOAT *a, GFLOAT *lsp, GFLOAT *old_lsp);

void  get_lsp_pol(GFLOAT lsf[],GFLOAT f[]);

void  lsp_az(GFLOAT *lsp, GFLOAT *a);

void  int_qlpc(GFLOAT lsp_new[], GFLOAT lsp_old[], GFLOAT a[]);

void  weight_az(GFLOAT *a,  GFLOAT gamma, int m,  GFLOAT *ap);

INLINE_PREFIX void lsf_lsp(const GFLOAT lsf[], GFLOAT lsp[], int m);
INLINE_PREFIX void lsp_lsf(const GFLOAT lsp[], GFLOAT lsf[], int m);

void residu(    /* filter A(z)                                       */
 GFLOAT *a,      /* input : prediction coefficients a[0:m+1], a[0]=1. */
 GFLOAT *x,      /* input : input signal x[0:l-1], x[-1:m] are needed */
 GFLOAT *y,      /* output: output signal y[0:l-1] NOTE: x[] and y[]
                            cannot point to same array               */
 int l          /* input : dimension of x and y                      */
);

void syn_filt(
 GFLOAT a[],     /* input : predictor coefficients a[0:m]    */
 GFLOAT x[],     /* input : excitation signal                */
 GFLOAT y[],     /* output: filtered output signal           */
 int l,         /* input : vector dimension                 */
 GFLOAT mem[],   /* in/out: filter memory                    */
 int update_m   /* input : 0 = no memory update, 1 = update */
);

void convolve(
 GFLOAT x[],             /* input : input vector x[0:l]                     */
 GFLOAT h[],             /* input : impulse response or second input h[0:l] */
 GFLOAT y[],             /* output: x convolved with h , y[0:l]             */
 int l                  /* input : dimension of all vectors                */
);

/*-------------------------------------------------------------*
 * Prototypes of pitch functions                               *
 *-------------------------------------------------------------*/

int pitch_ol_fast(  /* output: open loop pitch lag                        */
   GFLOAT signal[],  /* input : signal used to compute the open loop pitch */
                    /*     signal[-pit_max] to signal[-1] should be known */
   int L_frame      /* input : length of frame to compute pitch           */
);

int pitch_fr3_fast(     /* output: integer part of pitch period */
 GFLOAT exc[],           /* input : excitation buffer            */
 GFLOAT xn[],            /* input : target vector                */
 GFLOAT h[],             /* input : impulse response.            */
 int L_subfr,           /* input : Length of subframe           */
 int t0_min,            /* input : minimum value in the searched range */
 int t0_max,            /* input : maximum value in the searched range */
 int i_subfr,           /* input : indicator for first subframe        */
 int *pit_frac          /* output: chosen fraction                     */
);

GFLOAT g_pitch(GFLOAT xn[], GFLOAT y1[], GFLOAT g_coeff[], int l);

int enc_lag3(     /* output: Return index of encoding */
  int T0,         /* input : Pitch delay              */
  int T0_frac,    /* input : Fractional pitch delay   */
  int *T0_min,    /* in/out: Minimum search delay     */
  int *T0_max,    /* in/out: Maximum search delay     */
  int pit_min,    /* input : Minimum pitch delay      */
  int pit_max,    /* input : Maximum pitch delay      */
  int pit_flag    /* input : Flag for 1st subframe    */
);

void dec_lag3(     /* Decode the pitch lag                   */
  int index,       /* input : received pitch index           */
  int pit_min,     /* input : minimum pitch lag              */
  int pit_max,     /* input : maximum pitch lag              */
  int i_subfr,     /* input : subframe flag                  */
  int *T0,         /* output: integer part of pitch lag      */
  int *T0_frac     /* output: fractional part of pitch lag   */
);

void pred_lt_3(       /* Compute adaptive codebook                       */
 GFLOAT exc[],         /* in/out: excitation vector, exc[0:l_sub-1] = out */
 int t0,              /* input : pitch lag                               */
 int frac,            /* input : Fraction of pitch lag (-1, 0, 1)  / 3   */
 int l_sub            /* input : length of subframe.                     */
);

int parity_pitch(int pitch_i);

int check_parity_pitch(int pitch_i, int parity);

void cor_h_x(
     GFLOAT h[],         /* (i) :Impulse response of filters      */
     GFLOAT X[],         /* (i) :Target vector                    */
     GFLOAT D[]          /* (o) :Correlations between h[] and D[] */
);

/*-----------------------*
 * Innovative codebook.  *
 *-----------------------*/

int ACELP_code_A(       /* (o) :index of pulses positions    */
  GFLOAT x[],            /* (i) :Target vector                */
  GFLOAT h[],            /* (i) :Inpulse response of filters  */
  int T0,               /* (i) :Pitch lag                    */
  GFLOAT pitch_sharp,    /* (i) :Last quantized pitch gain    */
  GFLOAT code[],         /* (o) :Innovative codebook          */
  GFLOAT y[],            /* (o) :Filtered innovative codebook */
  int *sign             /* (o) :Signs of 4 pulses            */
);

void  decod_ACELP(int signs, int positions, GFLOAT cod[]);

/*-----------------------------------------------------------*
 * Prototypes of LSP VQ functions                            *
 *-----------------------------------------------------------*/

struct lsp_cod_state_t
{
	GFLOAT freq_prev[MA_NP][M];    /* previous LSP vector       */
};

void lsp_codw_reset(struct lsp_cod_state_t *);

void qua_lsp(struct lsp_cod_state_t *,
  GFLOAT lsp[],       /* (i) : Unquantized LSP            */
  GFLOAT lsp_q[],     /* (o) : Quantized LSP              */
  int ana[]          /* (o) : indexes                    */
);

void lsp_expand_1( GFLOAT buf[], GFLOAT c);

void lsp_expand_2( GFLOAT buf[], GFLOAT c);

void lsp_expand_1_2( GFLOAT buf[], GFLOAT c);

void lsp_get_quant(
  const GFLOAT lspcb1[][M],
  const GFLOAT lspcb2[][M],
  int code0,
  int code1,
  int code2,
  const GFLOAT fg[][M],
  GFLOAT freq_prev[][M],
  GFLOAT lspq[],
  const GFLOAT fg_sum[]
);

struct lsp_dec_state_t
{
	/* memory */
	GFLOAT freq_prev[MA_NP][M];    /* previous LSP vector       */

	/* memory for frame erase operation */
	int prev_ma;                  /* previous MA prediction coef.*/
	GFLOAT prev_lsp[M];            /* previous LSP vector         */
};

void lsp_decw_reset(struct lsp_dec_state_t *);

void d_lsp(struct lsp_dec_state_t *,
int index[],           /* input : indexes                 */
GFLOAT lsp_new[],       /* output: decoded lsp             */
int bfi                /* input : frame erase information */
);

void lsp_prev_extract(
  GFLOAT lsp[M],
  GFLOAT lsp_ele[M],
  const GFLOAT fg[MA_NP][M],
  GFLOAT freq_prev[MA_NP][M],
  const GFLOAT fg_sum_inv[M]
);

void lsp_prev_update(
  GFLOAT lsp_ele[M],
  GFLOAT freq_prev[MA_NP][M]
);

void lsp_stability(
 GFLOAT  buf[]           /*in/out: LSP parameters  */
);

void lsp_prev_compose(
  GFLOAT lsp_ele[],
  GFLOAT lsp[],
  const GFLOAT fg[][M],
  GFLOAT freq_prev[][M],
  const GFLOAT fg_sum[]
);

void get_wegt( GFLOAT     flsp[], GFLOAT   wegt[] );

void get_freq_prev(const GFLOAT freq_prev[MA_NP][M], GFLOAT x[MA_NP][M]);
void update_freq_prev(GFLOAT freq_prev[MA_NP][M], const GFLOAT x[MA_NP][M]);

/*--------------------------------------------------------------------------*
 * gain VQ functions.                                                       *
 *--------------------------------------------------------------------------*/

struct gain_state_t
{
	GFLOAT past_qua_en[4];
};

void gain_past_reset(struct gain_state_t *);

int qua_gain(struct gain_state_t *,
		GFLOAT code[], GFLOAT *coeff, int lcode, GFLOAT *gain_pit,
        GFLOAT *gain_code, int tameflag   );

void  dec_gain(struct gain_state_t *,
			   int indice, GFLOAT code[], int lcode, int bfi, GFLOAT *gain_pit,
               GFLOAT *gain_code);

void gain_predict(
  GFLOAT past_qua_en[],  /* input :Past quantized energies       */
  GFLOAT code[],         /* input: Innovative vector.            */
  int l_subfr,          /* input : Subframe length.             */
  GFLOAT *gcode0         /* output : Predicted codebook gain     */
);

void gain_update(
  GFLOAT past_qua_en[], /* input/output :Past quantized energies  */
  GFLOAT g_code         /* input        : quantized gain          */
);

void gain_update_erasure(GFLOAT *past_qua_en);

void  corr_xy2(GFLOAT xn[], GFLOAT y1[], GFLOAT y2[], GFLOAT g_coeff[]);

/*-----------------------*
 * Bitstream function    *
 *-----------------------*/
void prm2bits_ld8k(int prm[], INT16 bits[]);
void bits2prm_ld8k(INT16 bits[], int prm[]);
int  read_frame(FILE *f_serial, int *parm);

/*-----------------------------------------------------------*
 * Prototypes for the post filtering                         *
 *-----------------------------------------------------------*/

struct postfilt_state_t
{
	/* inverse filtered synthesis (with A(z/GAMMA2_PST))   */
	GFLOAT res2_buf[PIT_MAX+L_SUBFR];
	GFLOAT *res2;

	/* memory of filter 1/A(z/GAMMA1_PST) */
	GFLOAT mem_syn_pst[M];

	GFLOAT mem_pre;
    GFLOAT past_gain;
};

void init_post_filter(struct postfilt_state_t *);

void post_filter(struct postfilt_state_t *,
  GFLOAT *syn,     /* in/out: synthesis speech (postfiltered is output)    */
  GFLOAT *a_t,     /* input : interpolated LPC parameters in all subframes */
  int *T,         /* input : decoded pitch lags in all subframes          */
  int Vad         /* input : decoded frame type          */
);

/*------------------------------------------------------------*
 * prototypes for taming procedure.                           *
 *------------------------------------------------------------*/

void init_exc_err(GFLOAT exc_err[4]);
void update_exc_err(GFLOAT exc_err[4], GFLOAT gain_pit, int t0);
int test_err(GFLOAT exc_err[4], int t0, int t0_frac);

/*-----------------------------------------------------------*
 * Prototypes for auxiliary functions                        *
 *-----------------------------------------------------------*/

INLINE_PREFIX void set_zero(GFLOAT  dst[], int L);
INLINE_PREFIX void copy(const GFLOAT src[], GFLOAT dst[], int L);
INLINE_PREFIX INT16 random_g729(INT16 *seed);

void fwrite16(
 GFLOAT *data,           /* input: inputdata            */
 int length,            /* input: length of data array */
 FILE *fp               /* input: file pointer         */
);

void dvsub(GFLOAT *in1, GFLOAT *in2, GFLOAT *out, int npts);

GFLOAT dvdot(GFLOAT *in1, GFLOAT *in2, int npts);

void dvwadd(GFLOAT *in1, GFLOAT scalar1, GFLOAT *in2, GFLOAT scalar2,
             GFLOAT *out, int npts);

void dvsmul(GFLOAT *in, GFLOAT scalar, GFLOAT *out, int npts);

#ifdef INLINE_FUNCS
#	include "utilinl.c"
#	include "lpcfinl.c"
#endif

#endif /* _LD8A_H_INCL */
