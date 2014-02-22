/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                          Version 2.1 of October 1999
*/

/*
 File : VAD.C
*/

/* Voice Activity Detector functions */
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
static int vad_make_dec(
    GFLOAT dSLE,    /* (i)  : differential low band energy */
    GFLOAT dSE,     /* (i)  : differential full band energy */
    GFLOAT SD,      /* (i)  : differential spectral distortion  */
    GFLOAT dSZC     /* (i)  : differential zero crossing rate */
);

/*---------------------------------------------------------------------------*
 * Function  vad_init                                                                                                            *
 * ~~~~~~~~~~~~~~~~~~                                                                                                            *
 *                                                                                                                                                       *
 * -> Initialization of variables for voice activity detection                           *
 *                                                                                                                                                       *
*---------------------------------------------------------------------------*/
void vad_init(struct vad_state_t * state)
{
    /* Static vectors to zero */
    set_zero(state->MeanLSF, M);
    
    /* Initialize VAD parameters */
    state->MeanSE = (F)0.0;
    state->MeanSLE = (F)0.0;
    state->MeanE = (F)0.0;
    state->MeanSZC = (F)0.0;
    state->count_sil = 0;
    state->count_update = 0;
    state->count_ext = 0;
    state->less_count = 0;
    state->flag = 1;
    state->Min = FLT_MAX_G729;
}

/*-----------------------------------------------------------------*
* Functions vad                                                   *
*           ~~~                                                   *
* Input:                                                          *
*   rc            : reflection coefficient                        *
*   lsf[]         : unquantized lsf vector                        *
*   rxx[]         : autocorrelation vector                        *
*   sigpp[]       : preprocessed input signal                     *
*   frm_count     : frame counter                                 *
*   prev_marker   : VAD decision of the last frame                *
*   pprev_marker  : VAD decision of the frame before last frame   *
*                                                                 *
* Output:                                                         *
*                                                                 *
*   marker        : VAD decision of the current frame             *
*                                                                 *
*-----------------------------------------------------------------*/

void vad(struct vad_state_t * state,
    GFLOAT  rc,
    GFLOAT *lsf,
    GFLOAT *rxx, 
    GFLOAT *sigpp,
    int frm_count,
    int prev_marker,
    int pprev_marker,
    int *marker,
    GFLOAT *Energy_db)
{
    GFLOAT tmp[M];
    GFLOAT SD;
    GFLOAT E_low;
    GFLOAT  dtemp;
    GFLOAT  dSE;
    GFLOAT  dSLE;
    GFLOAT   ZC;
    GFLOAT  COEF;
    GFLOAT COEFZC;
    GFLOAT COEFSD;
    GFLOAT  dSZC;
    GFLOAT norm_energy;
    int i;
    
    /* compute the frame energy */
    norm_energy = (F)10.0*(GFLOAT) log10((GFLOAT)( rxx[0]/(F)240.0 +EPSI));
    *Energy_db = norm_energy ;

    /* compute the low band energy */
    E_low = (F)0.0;
    for( i=1; i<= NP; i++)
        E_low = E_low + rxx[i]*lbf_corr[i];

    E_low= rxx[0]*lbf_corr[0] + (F)2.0*E_low;
    if (E_low < (F)0.0) E_low = (F)0.0;
    E_low= (F)10.0*(GFLOAT) log10((GFLOAT) (E_low/(F)240.0+EPSI));

    /* compute SD */
    /* Normalize lsfs */
    for(i=0; i<M; i++) lsf[i] /= (F)2.*PI;
    dvsub(lsf, state->MeanLSF,tmp,M);
    SD = dvdot(tmp,tmp,M);
    
    /* compute # zero crossing */
    ZC = (F)0.0f;
    dtemp = sigpp[ZC_START];
    for (i=ZC_START+1 ; i <= ZC_END ; i++) {
        if (dtemp*sigpp[i] < (F)0.0) {
            ZC= ZC +(F)1.0;
        }
        dtemp = sigpp[i];
    }
    ZC = ZC/(F)80.0;
    
    /* Initialize and update Mins */
    if( frm_count < 129 ) {
        if( norm_energy < state->Min ){
            state->Min = norm_energy;
            state->Prev_Min = norm_energy;
        }
        if( (frm_count % 8) == 0){
            state->Min_buffer[(int)frm_count/8 -1] = state->Min;
            state->Min = FLT_MAX_G729;
        }
    }
    if( (frm_count % 8) == 0){
        state->Prev_Min = state->Min_buffer[0];
        for ( i =1; i< 15; i++){
            if ( state->Min_buffer[i] <  state->Prev_Min )
                state->Prev_Min = state->Min_buffer[i];
        }
    }
    
    if( frm_count >= 129 ) {
        if( (frm_count % 8 ) == 1) {
            state->Min = state->Prev_Min;
            state->Next_Min = FLT_MAX_G729;
        }
        if( norm_energy < state->Min )
            state->Min = norm_energy;
        if( norm_energy < state->Next_Min )
            state->Next_Min = norm_energy;
        if( (frm_count % 8) == 0){
            for ( i =0; i< 15; i++)
                state->Min_buffer[i] = state->Min_buffer[i+1];
            state->Min_buffer[15]  = state->Next_Min;
            state->Prev_Min = state->Min_buffer[0];
            for ( i =1; i< 16; i++){
                if ( state->Min_buffer[i] <  state->Prev_Min )
                    state->Prev_Min = state->Min_buffer[i];
            }
            
        }
    }
    
    if (frm_count <= INIT_FRAME){
        if( norm_energy < (F)21.0){
            state->less_count++;
            *marker = NOISE;
        }
        else{
            *marker = VOICE;
            state->MeanE = (state->MeanE * ( (GFLOAT)(frm_count - state->less_count -1)) +
                norm_energy)/(GFLOAT) (frm_count - state->less_count);
            state->MeanSZC = (state->MeanSZC * ( (GFLOAT)(frm_count - state->less_count -1)) +
                ZC)/(GFLOAT) (frm_count - state->less_count);
            dvwadd(state->MeanLSF,(GFLOAT) (frm_count - state->less_count -1),lsf,(F)1.0,state->MeanLSF,M);
            dvsmul(state->MeanLSF,(F)1.0/(GFLOAT) (frm_count - state->less_count ), state->MeanLSF,M);
        }
    }

    if (frm_count >= INIT_FRAME ){
        if (frm_count == INIT_FRAME ){
            state->MeanSE = state->MeanE -(F)10.0;
            state->MeanSLE = state->MeanE -(F)12.0;
        }

        dSE = state->MeanSE - norm_energy;
        dSLE = state->MeanSLE - E_low;
        dSZC = state->MeanSZC - ZC;

        if( norm_energy < (F)21.0 ){
            *marker = NOISE;
        }
        else{
            *marker = vad_make_dec(dSLE, dSE, SD, dSZC );
        }
        
        state->v_flag =0;
        if( (prev_marker == VOICE) && (*marker == NOISE) &&
            (norm_energy > state->MeanSE + (F)2.0) && ( norm_energy>(F)21.0)){
            *marker = VOICE;
            state->v_flag=1;
        }
        
        if((state->flag == 1) ){
            if( (pprev_marker == VOICE) && (prev_marker == VOICE) &&
                (*marker == NOISE) && (fabs(state->prev_energy - norm_energy)<= (F)3.0)){
                state->count_ext++;
                *marker = VOICE;
                state->v_flag=1;
                if(state->count_ext <=4)
                    state->flag =1;
                else{
                    state->flag =0;
                    state->count_ext=0;
                }
            }
        }
        else
            state->flag =1;
        
        if(*marker == NOISE)
            state->count_sil++;
        
        if((*marker == VOICE) && (state->count_sil > 10) &&
            ((norm_energy - state->prev_energy) <= (F)3.0)){
            *marker = NOISE;
            state->count_sil=0;
        }
        
        
        if(*marker == VOICE)
            state->count_sil=0;
        
        if ((norm_energy < state->MeanSE + (F)3.0) && ( frm_count >128) &&( !state->v_flag) && (rc <(F)0.6) )
			*marker = NOISE;

        if ((norm_energy < state->MeanSE + (F)3.0) && (rc <(F)0.75) && ( SD<(F)0.002532959)){
            state->count_update++;
            if (state->count_update < INIT_COUNT){
                COEF = (F)0.75;
                COEFZC = (F)0.8;
                COEFSD = (F)0.6;
            }
            else
                if (state->count_update < INIT_COUNT+10){
                    COEF = (F)0.95;
                    COEFZC = (F)0.92;
                    COEFSD = (F)0.65;
                }
                else
                    if (state->count_update < INIT_COUNT+20){
                        COEF = (F)0.97;
                        COEFZC = (F)0.94;
                        COEFSD = (F)0.70;
                    }
                    else
                        if (state->count_update < INIT_COUNT+30){
                            COEF = (F)0.99;
                            COEFZC = (F)0.96;
                            COEFSD = (F)0.75;
                        }
                        else
                            if (state->count_update < INIT_COUNT+40){
                                COEF = (F)0.995;
                                COEFZC = (F)0.99;
                                COEFSD = (F)0.75;
                            }
                            else{
                                COEF = (F)0.995;
                                COEFZC = (F)0.998;
                                COEFSD = (F)0.75;
                            }
            dvwadd(state->MeanLSF,COEFSD,lsf,(F)1.0 -COEFSD,state->MeanLSF,M);
            state->MeanSE = COEF * state->MeanSE+((F)1.0- COEF)*norm_energy;
            state->MeanSLE = COEF * state->MeanSLE+((F)1.0- COEF)*E_low;
            state->MeanSZC = COEFZC * state->MeanSZC+((F)1.0- COEFZC)*ZC;
        }
        
        if(((frm_count > 128) && (state->MeanSE < state->Min) && (SD < (F)0.002532959)) || (state->MeanSE > state->Min + (F)10.0) )
		{
            state->MeanSE = state->Min;
            state->count_update = 0;
        }
    }
  
    state->prev_energy = norm_energy;

    return;
}


static const GFLOAT a[14] = {
 (F)1.750000e-03, (F)-4.545455e-03, (F)-2.500000e+01,( F)2.000000e+01,
 (F)0.000000e+00, (F)8.800000e+03, (F)0.000000e+00, (F)2.5e+01,
 (F)-2.909091e+01, (F)0.000000e+00, (F)1.400000e+04, (F)0.928571,
 (F)-1.500000e+00, (F)0.714285};

static const GFLOAT b[14] = {
 (F)0.00085, (F)0.001159091, (F)-5.0, (F)-6.0, (F)-4.7, (F)-12.2, (F)0.0009,
 (F)-7.0, (F)-4.8182, (F)-5.3, (F)-15.5, (F)1.14285, (F)-9.0, (F)-2.1428571};

static int vad_make_dec(
    GFLOAT dSLE,
    GFLOAT dSE, 
    GFLOAT SD, 
    GFLOAT dSZC
)
{
        
    GFLOAT pars[4];
    
    pars[0] = dSLE;
    pars[1] = dSE;
    pars[2] = SD;
    pars[3] = dSZC;
    
    /* SD vs dSZC */
    if (pars[2] > a[0]*pars[3]+b[0]) {
        return(VOICE);
    }
    if (pars[2] > a[1]*pars[3]+b[1]) {
        return(VOICE);
    }
        
    /*   dE vs dSZC */
    
    if (pars[1] < a[2]*pars[3]+b[2]) {
        return(VOICE);
    }
    if (pars[1] < a[3]*pars[3]+b[3]) {
        return(VOICE);
    }
    if (pars[1] < b[4]) {
        return(VOICE);
    }
        
    /*   dE vs SD */
    if (pars[1] < a[5]*pars[2]+b[5]) {
        return(VOICE);
    }
    if (pars[2] > b[6]) {
        return(VOICE);
    }
        
    /* dEL vs dSZC */
    if (pars[1] < a[7]*pars[3]+b[7]) {
        return(VOICE);
    }
    if (pars[1] < a[8]*pars[3]+b[8]) {
        return(VOICE);
    }
    if (pars[1] < b[9]) {
        return(VOICE);
    }
    
    /* dEL vs SD */
    if (pars[0] < a[10]*pars[2]+b[10]) {
        return(VOICE);
    }
    
    /* dEL vs dE */
    if (pars[0] > a[11]*pars[1]+b[11]) {
        return(VOICE);
    }
    
    if (pars[0] < a[12]*pars[1]+b[12]) {
        return(VOICE);
    }
    if (pars[0] < a[13]*pars[1]+b[13]) {
        return(VOICE);
    }
    
    return(NOISE);
}

