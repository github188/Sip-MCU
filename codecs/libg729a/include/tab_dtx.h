/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                         Version 2.1 of October 1999
*/

/*
 File : TAB_DTX.H
*/

/* VAD constants */
extern const GFLOAT lbf_corr[NP+1];

/* SID LSF quantization */
extern const GFLOAT noise_fg_sum[MODE][M];
extern const GFLOAT noise_fg_sum_inv[MODE][M];
extern const int PtrTab_1[32];
extern const int PtrTab_2[2][16];
extern const GFLOAT Mp[MODE];

/* SID gain quantization */
extern const GFLOAT fact[NB_GAIN+1];
extern const GFLOAT tab_Sidgain[32];



