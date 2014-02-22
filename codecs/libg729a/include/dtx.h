/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                         Version 2.1 of October 1999
*/
/*
 File : DTX.H
*/

/*--------------------------------------------------------------------------*
 * Constants for DTX/CNG                                                    *
 *--------------------------------------------------------------------------*/

/* DTX constants */
#define FLAG_COD        1
#define FLAG_DEC        0
#define INIT_SEED       (INT16)11111
#define FR_SID_MIN      3
#define NB_SUMACF       3
#define NB_CURACF       2
#define NB_GAIN         2
#define THRESH1         (GFLOAT)1.1481628
#define THRESH2         (GFLOAT)1.0966466
#define A_GAIN0         (GFLOAT)0.875

#define SIZ_SUMACF      (NB_SUMACF * MP1)
#define SIZ_ACF         (NB_CURACF * MP1)
#define A_GAIN1         ((GFLOAT)1. - A_GAIN0)

#define MIN_ENER        (GFLOAT)0.1588489319   /* <=> - 8 dB      */

#define RATE_8000       80      /* Full rate  (8000 bit/s)       */
#define RATE_SID        15      /* SID                           */
#define RATE_0           0      /* 0 bit/s rate                  */

/* CNG excitation generation constant */
                                           /* alpha = 0.5 */
#define NORM_GAUSS      (GFLOAT)3.16227766  /* sqrt(40)xalpha */
#define K0              (GFLOAT)3.          /* 4 x (1 - alpha ** 2) */
#define G_MAX           (GFLOAT)5000.


/*--------------------------------------------------------------------------*
 * Prototypes for DTX/CNG                                                   *
 *--------------------------------------------------------------------------*/

struct cod_cng_state_t
{
	/* Static Variables */
	GFLOAT lspSid_q[M] ;
	GFLOAT pastCoeff[MP1];
	GFLOAT RCoeff[MP1];
	GFLOAT Acf[SIZ_ACF];
	GFLOAT sumAcf[SIZ_SUMACF];
	GFLOAT ener[NB_GAIN];
	int fr_cur;
	GFLOAT cur_gain;
	int nb_ener;
	GFLOAT sid_gain;
	int flag_chang;
	GFLOAT prev_energy;
	int count_fr0;

	GFLOAT exc_err[4]; /* taming state */

	struct lsfq_state_t lsfq_s;
	//struct taming_state_t taming_s;
};

/* Encoder DTX/CNG functions */
void init_cod_cng(struct cod_cng_state_t *);
void cod_cng(struct cod_cng_state_t *,
  GFLOAT *exc,          /* (i/o) : excitation array                     */
  int pastVad,         /* (i)   : previous VAD decision                */
  GFLOAT *lsp_old_q,    /* (i/o) : previous quantized lsp               */
//    GFLOAT *old_A,          /* (i/o) : last stable filter LPC coefficients  */
//    GFLOAT *old_rc,         /* (i/o) : last stable filter Reflection coefficients.*/
  GFLOAT *Aq,           /* (o)   : set of interpolated LPC coefficients */
  int *ana,            /* (o)   : coded SID parameters                 */
  GFLOAT freq_prev[MA_NP][M],
                       /* (i/o) : previous LPS for quantization        */
  INT16 *seed          /* (i/o) : random generator seed                */
);
void update_cng(struct cod_cng_state_t *,
  GFLOAT *r,         /* (i) :   frame autocorrelation               */
  int Vad           /* (i) :   current Vad decision                */
);

/* SID gain Quantization */
void qua_Sidgain(
  GFLOAT *ener,     /* (i)   array of energies                   */
  int nb_ener,     /* (i)   number of energies or               */
  GFLOAT *enerq,    /* (o)   decoded energies in dB              */
  int *idx         /* (o)   SID gain quantization index         */
);

/* CNG excitation generation */
void calc_exc_rand(GFLOAT exc_err[4],
  GFLOAT cur_gain,      /* (i)   :   target sample gain                 */
  GFLOAT *exc,          /* (i/o) :   excitation array                   */
  INT16 *seed,         /* (i)   :   current Vad decision               */
  int flag_cod         /* (i)   :   encoder/decoder flag               */
);

struct dec_cng_state_t
{
	GFLOAT cur_gain;
	GFLOAT lspSid[M];
	GFLOAT sid_gain;
	
	GFLOAT exc_err[4]; /* taming state */

	struct lsfq_state_t lsfq_s;
	//struct taming_state_t taming_s;
};

/* Decoder CNG generation */
void init_dec_cng(struct dec_cng_state_t *);
void dec_cng(struct dec_cng_state_t *,
  int past_ftyp,       /* (i)   : past frame type                      */
  GFLOAT sid_sav,       /* (i)   : energy to recover SID gain           */
  int *parm,           /* (i)   : coded SID parameters                 */
  GFLOAT *exc,          /* (i/o) : excitation array                     */
  GFLOAT *lsp_old,      /* (i/o) : previous lsp                         */
  GFLOAT *A_t,          /* (o)   : set of interpolated LPC coefficients */
  INT16 *seed,         /* (i/o) : random generator seed                */
  GFLOAT freq_prev[MA_NP][M]
                        /* (i/o) : previous LPS for quantization        */
);
