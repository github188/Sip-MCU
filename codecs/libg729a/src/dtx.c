/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                          Version 2.1 of October 1999
*/


/*
 File : DTX.C
*/

/* DTX and Comfort Noise Generator - Encoder part */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "typedef.h"
#include "ld8a.h"
#include "tab_ld8a.h"
#include "sid.h"
#include "vad.h"
#include "dtx.h"
#include "tab_dtx.h"

/* Local functions */
static void calc_pastfilt(struct cod_cng_state_t *, GFLOAT *Coeff);
static void calc_RCoeff(GFLOAT *Coeff, GFLOAT *RCoeff);
static int cmp_filt(GFLOAT *RCoeff, GFLOAT *acf, GFLOAT alpha, GFLOAT Thresh);
static void calc_sum_acf(GFLOAT *acf, GFLOAT *sum, int nb);
static void update_sumAcf(struct cod_cng_state_t *);

/*-----------------------------------------------------------*
* procedure init_Cod_cng:                                   *
*           ~~~~~~~~~~~~                                    *
*   Initialize variables used for dtx at the encoder        *
*-----------------------------------------------------------*/
void init_cod_cng(struct cod_cng_state_t * state)
{
	set_zero(state->sumAcf, SIZ_SUMACF);
	set_zero(state->Acf, SIZ_ACF);
	set_zero(state->ener, NB_GAIN);
    
    state->cur_gain = 0;
    state->fr_cur = 0;
    state->flag_chang = 0;
}

/*-----------------------------------------------------------*
* procedure cod_cng:                                        *
*           ~~~~~~~~                                        *
*   computes DTX decision                                   *
*   encodes SID frames                                      *
*   computes CNG excitation for encoder update              *
*-----------------------------------------------------------*/
void cod_cng(struct cod_cng_state_t * state,
    GFLOAT *exc,          /* (i/o) : excitation array                     */
    int pastVad,         /* (i)   : previous VAD decision                */
    GFLOAT *lsp_old_q,    /* (i/o) : previous quantized lsp               */
//    GFLOAT *old_A,          /* (i/o) : last stable filter LPC coefficients  */
//    GFLOAT *old_rc,          /* (i/o) : last stable filter Reflection coefficients.*/
    GFLOAT *Aq,           /* (o)   : set of interpolated LPC coefficients */
    int *ana,            /* (o)   : coded SID parameters                 */
    GFLOAT freq_prev[MA_NP][M], /* (i/o) : previous LPS for quantization        */
    INT16 *seed          /* (i/o) : random generator seed                */
)
{
    int i;
    
    GFLOAT curAcf[MP1];
    GFLOAT bid[MP1];
    GFLOAT curCoeff[MP1];
    GFLOAT lsp_new[M];
    GFLOAT *lpcCoeff;
    int cur_igain;
    GFLOAT energyq;
    
    /* Update Ener */
    for(i = NB_GAIN-1; i>=1; i--) {
        state->ener[i] = state->ener[i-1];
    }
    
    /* Compute current Acfs */
    calc_sum_acf(state->Acf, curAcf, NB_CURACF);
    
    /* Compute LPC coefficients and residual energy */
    if(curAcf[0] == (F)0.) {
        state->ener[0] = (F)0.;                /* should not happen */
    }
    else {
        state->ener[0] = levinson(curAcf, curCoeff, bid);
    }

    /* if first frame of silence => SID frame */
    if(pastVad != 0) {
        ana[0] = 2;
        state->count_fr0 = 0;
        state->nb_ener = 1;
        qua_Sidgain(state->ener, state->nb_ener, &energyq, &cur_igain);
    }
    else {
        state->nb_ener++;
        if(state->nb_ener > NB_GAIN) state->nb_ener = NB_GAIN;
        qua_Sidgain(state->ener, state->nb_ener, &energyq, &cur_igain);

        /* Compute stationarity of current filter   */
        /* versus reference filter                  */
        if(cmp_filt(state->RCoeff, curAcf, state->ener[0], THRESH1) != 0) {
            state->flag_chang = 1;
        }
        
        /* compare energy difference between current frame and last frame */
        if( (GFLOAT)fabs(state->prev_energy - energyq) > (F)2.0) state->flag_chang = 1;
        
        state->count_fr0++;
        if(state->count_fr0 < FR_SID_MIN) {
            ana[0] = 0; /* no transmission */
        }
        else {
            if(state->flag_chang != 0) {
                ana[0] = 2;             /* transmit SID frame */
            }
            else{
                ana[0] = 0;
            }
            state->count_fr0 = FR_SID_MIN;   /* to avoid overflow */
        }
    }

    if(ana[0] == 2) {

        /* Reset frame count and change flag */
        state->count_fr0 = 0;
        state->flag_chang = 0;
        
        /* Compute past average filter */
        calc_pastfilt(state, state->pastCoeff);
        calc_RCoeff(state->pastCoeff, state->RCoeff);
        
        /* Compute stationarity of current filter   */
        /* versus past average filter               */
        
        /* if stationary */
        /* transmit average filter => new ref. filter */
        if(cmp_filt(state->RCoeff, curAcf, state->ener[0], THRESH2) == 0) {
            lpcCoeff = state->pastCoeff;
        }
        
        /* else */
        /* transmit current filter => new ref. filter */
        else {
            lpcCoeff = curCoeff;
            calc_RCoeff(curCoeff, state->RCoeff);
        }
        
        /* Compute SID frame codes */
        az_lsp(lpcCoeff, lsp_new, lsp_old_q); /* From A(z) to lsp */
        
        /* LSP quantization */
        lsfq_noise(&state->lsfq_s, lsp_new, state->lspSid_q, freq_prev, &ana[1]);
        
        state->prev_energy = energyq;
        ana[4] = cur_igain;
        state->sid_gain = tab_Sidgain[cur_igain];
        
    } /* end of SID frame case */
    
    /* Compute new excitation */
    if(pastVad != 0) {
        state->cur_gain = state->sid_gain;
    }
    else {
        state->cur_gain *= A_GAIN0;
        state->cur_gain += A_GAIN1 * state->sid_gain;
    }
    
    calc_exc_rand(state->exc_err, state->cur_gain, exc, seed, FLAG_COD);
    
    int_qlpc(lsp_old_q, state->lspSid_q, Aq);
    /*for(i=0; i<M; i++) lsp_old_q[i] = state->lspSid_q[i];*/
	copy(state->lspSid_q, lsp_old_q, M);
        
    /* Update sumAcf if fr_cur = 0 */
    if(state->fr_cur == 0) {
        update_sumAcf(state);
    }
}

/*-----------------------------------------------------------*
* procedure update_cng:                                     *
*           ~~~~~~~~~~                                      *
*   updates autocorrelation arrays                          *
*   used for DTX/CNG                                        *
*   If Vad=1 : updating of array sumAcf                     *
*-----------------------------------------------------------*/
void update_cng(struct cod_cng_state_t * state,
    GFLOAT *r,         /* (i) :   frame autocorrelation               */
    int Vad           /* (i) :   current Vad decision                */
)
{
    int i;
    GFLOAT *ptr1, *ptr2;
    
    /* Update Acf */
    ptr1 = state->Acf + SIZ_ACF - 1;
    ptr2 = ptr1 - MP1;
    for(i=0; i<(SIZ_ACF-MP1); i++) {
        *ptr1-- = *ptr2--;
    }
    
    /* Save current Acf */
    /*for(i=0; i<MP1; i++) state->Acf[i] = r[i];*/
    copy(r, state->Acf, MP1);
    
    state->fr_cur++;
    if(state->fr_cur == NB_CURACF) {
        state->fr_cur = 0;
        if(Vad != 0) {
            update_sumAcf(state);
        }
    }
}


/*-----------------------------------------------------------*
*         Local procedures                                  *
*         ~~~~~~~~~~~~~~~~                                  *
*-----------------------------------------------------------*/

/* Compute autocorr of LPC coefficients used for Itakura distance */
/******************************************************************/
static void calc_RCoeff(GFLOAT *Coeff, GFLOAT *RCoeff)
{
    int i, j;
    GFLOAT temp;
    
    /* RCoeff[0] = SUM(j=0->M) Coeff[j] ** 2 */
    for(j=0, temp = (F)0.; j <= M; j++) {
        temp += Coeff[j] * Coeff[j];
    }
    RCoeff[0] = temp;
    
    /* RCoeff[i] = SUM(j=0->M-i) Coeff[j] * Coeff[j+i] */
    for(i=1; i<=M; i++) {
        for(j=0, temp=(F)0.; j<=M-i; j++) {
            temp += Coeff[j] * Coeff[j+i];
        }
        RCoeff[i] = (F)2. * temp;
    }
}

/* Compute Itakura distance and compare to threshold */
/*****************************************************/
static int cmp_filt(GFLOAT *RCoeff, GFLOAT *acf, GFLOAT alpha, GFLOAT Thresh)
{
    GFLOAT temp1, temp2;
    int i;
    int diff;
    
    temp1 = (F)0.;
    for(i=0; i <= M; i++) {
        temp1 += RCoeff[i] * acf[i];
    }
    
    temp2 = alpha * Thresh;
    if(temp1 > temp2) diff = 1;
    else diff = 0;
    
    return(diff);
}

/* Compute past average filter */
/*******************************/
static void calc_pastfilt(struct cod_cng_state_t * state, GFLOAT *Coeff)
{
    GFLOAT s_sumAcf[MP1];
    GFLOAT bid[M];

    calc_sum_acf(state->sumAcf, s_sumAcf, NB_SUMACF);
    
    if(s_sumAcf[0] == (F)0.) {
        Coeff[0] = (F)1.;
        /*for(i=1; i<=M; i++) Coeff[i] = (F)0.;*/
		set_zero(Coeff + 1, M);
        return;
    }

    levinson(s_sumAcf, Coeff, bid);
}

/* Update sumAcf */
/*****************/
static void update_sumAcf(struct cod_cng_state_t * state)
{
    GFLOAT *ptr1, *ptr2;
    int i;
    
    /*** Move sumAcf ***/
    ptr1 = state->sumAcf + SIZ_SUMACF - 1;
    ptr2 = ptr1 - MP1;
    for(i=0; i<(SIZ_SUMACF-MP1); i++) {
        *ptr1-- = *ptr2--;
    }
    
    /* Compute new sumAcf */
    calc_sum_acf(state->Acf, state->sumAcf, NB_CURACF);
}

/* Compute sum of acfs (curAcf, sumAcf or s_sumAcf) */
/****************************************************/
static void calc_sum_acf(GFLOAT *acf, GFLOAT *sum, int nb)
{
    
    GFLOAT *ptr1;
    int i, j;
    
    /*for(j=0; j<MP1; j++) sum[j] = (F)0.;*/
	set_zero(sum, MP1);

    ptr1 = acf;
    for(i=0; i<nb; i++) {
        for(j=0; j<MP1; j++) {
            sum[j] += (*ptr1++);
        }
    }
}
