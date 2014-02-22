/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                          Version 2.1 of October 1999
*/


/*
 File : DEC_SID.C
*/

/*
**
**
** Description:     Comfort noise generation
**                  performed at the decoder part
**
*/
/**** Floatting point version ***/

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

/*
**
** Function:        init_dec_cng()
**
** Description:     Initialize dec_cng static variables
**
**
*/
void init_dec_cng(struct dec_cng_state_t * state)
{
    /*for (i=0; i<M; i++) state->lspSid[i] = (GFLOAT)cos(freq_prev_reset[i]);*/
	lsf_lsp(freq_prev_reset, state->lspSid, M);
    
    state->sid_gain = tab_Sidgain[0];
}

/*-----------------------------------------------------------*
* procedure dec_cng:                                        *
*           ~~~~~~~~                                        *
*                     Receives frame type                   *
*                     0  :  for untransmitted frames        *
*                     2  :  for SID frames                  *
*                     Decodes SID frames                    *
*                     Computes current frame excitation     *
*                     Computes current frame LSPs
*-----------------------------------------------------------*/
void dec_cng(struct dec_cng_state_t * state,
    int past_ftyp,     /* (i)   : past frame type                      */
    GFLOAT sid_sav,     /* (i)   : energy to recover SID gain           */
    int *parm,         /* (i)   : coded SID parameters                 */
    GFLOAT *exc,        /* (i/o) : excitation array                     */
    GFLOAT *lsp_old,    /* (i/o) : previous lsp                         */
    GFLOAT *A_t,        /* (o)   : set of interpolated LPC coefficients */
    INT16 *seed,       /* (i/o) : random generator seed                */
    GFLOAT freq_prev[MA_NP][M] /* (i/o) : previous LPS for quantization        */
)
{
    GFLOAT temp;
    int ind;
    
    /* SID Frame */
    /*************/
    if(parm[0] != 0) {
        
        state->sid_gain = tab_Sidgain[parm[4]];
        
        /* Inverse quantization of the LSP */
        sid_lsfq_decode(&state->lsfq_s, &parm[1], state->lspSid, freq_prev);
        
    }
    
    /* non SID Frame */
    /*****************/
    else {
        
        /* Case of 1st SID frame erased : quantize-decode   */
        /* energy estimate stored in sid_gain         */
        if(past_ftyp == 1) {
            qua_Sidgain(&sid_sav, 0, &temp, &ind);
            state->sid_gain = tab_Sidgain[ind];
        }
        
    }
    
    if(past_ftyp == 1) {
        state->cur_gain = state->sid_gain;
    }
    else {
        state->cur_gain *= A_GAIN0;
        state->cur_gain += A_GAIN1 * state->sid_gain;
    }
    
    calc_exc_rand(state->exc_err, state->cur_gain, exc, seed, FLAG_DEC);
    
    /* Interpolate the Lsp vectors */
    int_qlpc(lsp_old, state->lspSid, A_t);
    copy(state->lspSid, lsp_old, M);
}

/*---------------------------------------------------------------------------*
 * Function  Init_lsfq_noise                                                 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~                                                 *
 *                                                                           *
 * -> Initialization of variables for the lsf quantization in the SID        *
 *                                                                           *
 *---------------------------------------------------------------------------*/
void init_lsfq_noise(struct lsfq_state_t * state)
{
    int i, j;
    
    /* initialize the noise_fg */
    for (i=0; i<4; i++)
        copy(fg[0][i], state->noise_fg[0][i], M);
    
    for (i=0; i<4; i++)
        for (j=0; j<M; j++){
            state->noise_fg[1][i][j] = fg[0][i][j]*(F)0.6 + fg[1][i][j]* (F)0.4;
        }
}

void sid_lsfq_decode(struct lsfq_state_t * state,
    int *index,             /* (i) : quantized indices    */
    GFLOAT *lspq,              /* (o) : quantized lsp vector */
    GFLOAT freq_prev[MA_NP][M] /* (i) : memory of predictor  */
)
{
    int i;
    GFLOAT lsfq[M], tmpbuf[M];
    
    /* get the lsf error vector */
    copy(lspcb1[PtrTab_1[index[1]]], tmpbuf, M);
    for (i=0; i<M/2; i++)
        tmpbuf[i] = tmpbuf[i]+ lspcb2[PtrTab_2[0][index[2]]][i];
    for (i=M/2; i<M; i++)
        tmpbuf[i] = tmpbuf[i]+ lspcb2[PtrTab_2[1][index[2]]][i];
    
        /* guarantee minimum distance of 0.0012 between tmpbuf[j]
    and tmpbuf[j+1] */
    lsp_expand_1_2(tmpbuf, (GFLOAT)0.0012);
    
    /* compute the quantized lsf vector */
    lsp_prev_compose(tmpbuf, lsfq, (const GFLOAT (*)[M]) state->noise_fg[index[0]],
		freq_prev, noise_fg_sum[index[0]]);
    
    /* update the prediction memory */
    lsp_prev_update(tmpbuf, freq_prev);
    
    /* lsf stability check */
    lsp_stability(lsfq);
    
    /* convert lsf to lsp */
    lsf_lsp(lsfq, lspq, M);
}
