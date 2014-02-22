/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                          Version 2.1 of October 1999
*/

/*
 File : QSIDLSF.C
*/

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

/* local functions */
static void qnt_e(
    GFLOAT *errlsf,    /* (i)  : error lsf vector             */
    GFLOAT *weight,    /* (i)  : weighting vector             */
    int DIn,        /* (i)  : number of input candidates   */
    GFLOAT *qlsf,      /* (o)  : quantized error lsf vector   */
    int *Pptr,      /* (o)  : predictor index              */
    int DOut,       /* (i)  : number of quantized vectors  */
    int *cluster,   /* (o)  : quantizer indices            */
    int *MS         /* (i)  : size of the quantizers       */
);

static void new_ML_search_1(
    GFLOAT *d_data,    /* (i) : error vector             */
    int J,          /* (i) : number of input vectors  */
    GFLOAT *new_d_data,/* (o) : output vector            */
    int K,          /* (i) : number of candidates     */
    int *best_indx, /* (o) : best indices             */
    int *ptr_back,  /* (o) : pointer for backtracking */
    const int *PtrTab,    /* (i) : quantizer table          */
    int MQ          /* (i) : size of quantizer        */
);

static void new_ML_search_2(
    GFLOAT *d_data,    /* (i) : error vector             */
    GFLOAT *weight,    /* (i) : weighting vector         */
    int J,          /* (i) : number of input vectors  */
    GFLOAT *new_d_data,/* (o) : output vector            */
    int K,          /* (i) : number of candidates     */
    int *best_indx, /* (o) : best indices             */
    int *ptr_prd,   /* (i) : pointer for backtracking */
    int *ptr_back,  /* (o) : pointer for backtracking */
    const int PtrTab[2][16],/* (i) : quantizer table        */
    int MQ          /* (i) : size of quantizer        */
);


/*-----------------------------------------------------------------*
 * Functions lsfq_noise                                            *
 *           ~~~~~~~~~~                                            *
 * Input:                                                          *
 *   lsp[]         : unquantized lsp vector                        *
 *   freq_prev[][] : memory of the lsf predictor                   *
 *                                                                 *
 * Output:                                                         *
 *                                                                 *
 *   lspq[]        : quantized lsp vector                          * 
 *   ana[]         : indices                                       *
 *                                                                 *
 *-----------------------------------------------------------------*/
void lsfq_noise(struct lsfq_state_t * state,
				GFLOAT *lsp, GFLOAT *lspq, GFLOAT freq_prev[MA_NP][M], int *ana)
{
    int i, MS[MODE]={32, 16}, Clust[MODE], mode;
    GFLOAT  lsf[M], lsfq[M], weight[M], tmpbuf[M], errlsf[M*MODE];
    
    /* convert lsp to lsf */
	lsp_lsf(lsp, lsf, M);
    
    /* spacing to ~100Hz */
    if (lsf[0] < L_LIMIT)
        lsf[0] = L_LIMIT;
    for (i=0 ; i < M-1 ; i++) {
        if (lsf[i+1]- lsf[i] < 2*GAP3) lsf[i+1] = lsf[i]+ 2*GAP3;
    }
    if (lsf[M-1] > M_LIMIT)
        lsf[M-1] = M_LIMIT;
    if (lsf[M-1] < lsf[M-2]) 
        lsf[M-2] = lsf[M-1]- GAP3;
    
    /* get the lsf weighting */
    get_wegt(lsf, weight);
    
    /**********************/
    /* quantize the lsf's */
    /**********************/
    /* get the prediction error vector */
    for (mode=0; mode<MODE; mode++)
        lsp_prev_extract(lsf, errlsf+mode*M, (const GFLOAT (*)[M]) state->noise_fg[mode],
		freq_prev, noise_fg_sum_inv[mode]);
    
    /* quantize the lsf and get the corresponding indices */
    qnt_e(errlsf, weight, MODE, tmpbuf, &mode, 1, Clust, MS);
    ana[0] = (int) mode;
    ana[1] = (int) Clust[0];
    ana[2] = (int) Clust[1];
    
    /* guarantee minimum distance of 0.0012 between tmpbuf[j]
    and tmpbuf[j+1] */
    lsp_expand_1_2(tmpbuf, (F)0.0012);
    
    /* compute the quantized lsf vector */
    lsp_prev_compose(tmpbuf, lsfq, (const GFLOAT (*)[M]) state->noise_fg[mode],
		freq_prev, noise_fg_sum[mode]);
    
    /* update the prediction memory */
    lsp_prev_update(tmpbuf, freq_prev);
    
    /* lsf stability check */
    lsp_stability(lsfq);
    
    /* convert lsf to lsp */
    lsf_lsp(lsfq, lspq, M);
}

static void qnt_e(
    GFLOAT *errlsf,    /* (i)  : error lsf vector             */
    GFLOAT *weight,    /* (i)  : weighting vector             */
    int DIn,        /* (i)  : number of input candidates   */
    GFLOAT *qlsf,      /* (o)  : quantized error lsf vector   */
    int *Pptr,      /* (o)  : predictor index              */
    int DOut,       /* (i)  : number of quantized vectors  */
    int *cluster,   /* (o)  : quantizer indices            */
    int *MS         /* (i)  : size of the quantizers       */
)
{
    int best_indx[2][R_LSFQ];
    int ptr_back[2][R_LSFQ], ptr, i;
    GFLOAT d_data[2][R_LSFQ*M];
    
    new_ML_search_1(errlsf, DIn, d_data[0], 4, best_indx[0], ptr_back[0],
        PtrTab_1, MS[0]);
    new_ML_search_2(d_data[0], weight, 4, d_data[1], DOut, best_indx[1], 
        ptr_back[0], ptr_back[1], PtrTab_2, MS[1]);
    
    /* backward path for the indices */
    cluster[1] = best_indx[1][0];
    ptr = ptr_back[1][0];
    cluster[0] = best_indx[0][ptr];
    
    /* this is the pointer to the best predictor */
    *Pptr = ptr_back[0][ptr];
    
    /* generating the quantized vector */
    copy(lspcb1[PtrTab_1[cluster[0]]], qlsf, M);
    for (i=0; i<M/2; i++)
        qlsf[i] = qlsf[i]+ lspcb2[PtrTab_2[0][cluster[1]]][i];
    for (i=M/2; i<M; i++)
        qlsf[i] = qlsf[i]+ lspcb2[PtrTab_2[1][cluster[1]]][i];
    
}

static void new_ML_search_1(
    GFLOAT *d_data,    /* (i) : error vector             */
    int J,          /* (i) : number of input vectors  */
    GFLOAT *new_d_data,/* (o) : output vector            */
    int K,          /* (i) : number of candidates     */
    int *best_indx, /* (o) : best indices             */
    int *ptr_back,  /* (o) : pointer for backtracking */
    const int *PtrTab,    /* (i) : quantizer table          */
    int MQ          /* (i) : size of quantizer        */
)
{
    int m, l, p, q, min_indx_p[R_LSFQ], min_indx_m[R_LSFQ];
    GFLOAT  sum[R_LSFQ*R_LSFQ], min[R_LSFQ];
    
    for (q=0 ; q < K ; q++) {
        min[q] = FLT_MAX_G729;
    }
    
    /* compute the errors */
    for (p=0 ; p < J ; p++) {
        for(m=0 ; m < MQ ; m++) {
            sum[p*MQ+m] = (F)0.0;
            for (l=0 ; l < M ; l++) {
                sum[p*MQ+m] +=
                    sqr(d_data[p*M+l]-lspcb1[PtrTab[m]][l]);
            }
            sum[p*MQ+m] *= Mp[p];
        }
    }
    
    /* select the candidates */
    for (q=0 ; q < K ; q++) {
        for (p=0 ; p < J ; p++) {
            for (m=0 ; m < MQ ; m++) {
                if (sum[p*MQ+m] < min[q]) {
                    min[q] = sum[p*MQ+m];
                    min_indx_p[q] = p;
                    min_indx_m[q] = m;
                }
            }
        }
        sum[min_indx_p[q]*MQ+min_indx_m[q]] = FLT_MAX_G729;
    }
    
    for (q=0 ; q < K ; q++) {
        for (l=0 ; l < M ; l++) {
            new_d_data[q*M+l] =
                d_data[min_indx_p[q]*M+l]-lspcb1[PtrTab[min_indx_m[q]]][l];
        }
        ptr_back[q] = min_indx_p[q];
        best_indx[q] = min_indx_m[q];
    }
}

static void new_ML_search_2(
    GFLOAT *d_data,    /* (i) : error vector             */
    GFLOAT *weight,    /* (i) : weighting vector         */
    int J,          /* (i) : number of input vectors  */
    GFLOAT *new_d_data,/* (o) : output vector            */
    int K,          /* (i) : number of candidates     */
    int *best_indx, /* (o) : best indices             */
    int *ptr_prd,   /* (i) : pointer for backtracking */
    int *ptr_back,  /* (o) : pointer for backtracking */
    const int PtrTab[2][16],/* (i) : quantizer table        */
    int MQ          /* (i) : size of quantizer        */
)
{
    int   m,l,p,q;
    int   min_indx_p[R_LSFQ]={0};
    int   min_indx_m[R_LSFQ]={0};
    GFLOAT   sum[R_LSFQ*R_LSFQ];
    GFLOAT   min[R_LSFQ];
    
    for (q=0; q<K; q++)
        min[q] = FLT_MAX_G729;
    
    /* compute the errors */
    for (p=0 ; p < J ; p++) {
        for(m=0 ; m < MQ ; m++) {
            sum[p*MQ+m] = (F)0.0;
            for (l=0 ; l < M/2 ; l++) {
                sum[p*MQ+m] +=
                    weight[l]*sqr(noise_fg_sum[ptr_prd[p]][l])*sqr(d_data[p*M+l]-lspcb2[PtrTab[0][m]][l]);
            }
            for (l=M/2 ; l < M ; l++) {
                sum[p*MQ+m] +=
                    weight[l]*sqr(noise_fg_sum[ptr_prd[p]][l])*sqr(d_data[p*M+l]-lspcb2[PtrTab[1][m]][l]);
            }
        }
    }
    
    /* select the candidates */
    for (q=0 ; q < K ; q++) {
        for (p=0 ; p < J ; p++) {
            for (m=0 ; m < MQ ; m++) {
                if (sum[p*MQ+m] < min[q]) {
                    min[q] = sum[p*MQ+m];
                    min_indx_p[q] = p;
                    min_indx_m[q] = m;
                }
            }
        }
        sum[min_indx_p[q]*MQ+min_indx_m[q]] = FLT_MAX_G729;
    }
    
    /* compute the candidates */
    for (q=0 ; q < K ; q++) {
        for (l=0 ; l < M/2 ; l++) {
            new_d_data[q*M+l] =
                d_data[min_indx_p[q]*M+l]-lspcb2[PtrTab[0][min_indx_m[q]]][l];
        }
        for (l=M/2 ; l < M ; l++) {
            new_d_data[q*M+l] = d_data[min_indx_p[q]*M+l]-
                lspcb2[PtrTab[1][min_indx_m[q]]][l];
        }
        ptr_back[q] = min_indx_p[q];
        best_indx[q] = min_indx_m[q];
    }
}
