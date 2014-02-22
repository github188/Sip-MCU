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

extern const GFLOAT hamwindow[L_WINDOW];
extern const GFLOAT lwindow[M+3];
extern const GFLOAT lspcb1[NC0][M];
extern const GFLOAT lspcb2[NC1][M];
extern const GFLOAT fg[2][MA_NP][M];
extern const GFLOAT fg_sum[2][M];
extern const GFLOAT fg_sum_inv[2][M];
extern const GFLOAT grid[GRID_POINTS+1];
extern const GFLOAT inter_3l[FIR_SIZE_SYN];
extern const GFLOAT pred[4];
extern const GFLOAT gbk1[NCODE1][2];
extern const GFLOAT gbk2[NCODE2][2];
extern const int map1[NCODE1];
extern const int map2[NCODE2];
extern const GFLOAT coef[2][2];
extern const GFLOAT thr1[NCODE1-NCAN1];
extern const GFLOAT thr2[NCODE2-NCAN2];
extern const int imap1[NCODE1];
extern const int imap2[NCODE2];
extern const GFLOAT b100[3];
extern const GFLOAT a100[3];
extern const GFLOAT b140[3];
extern const GFLOAT a140[3];
extern const int  bitsno[PRM_SIZE];
extern const int bitsno2[4]; 

extern const GFLOAT past_qua_en_reset[4];
extern const GFLOAT lsp_reset[M];

const GFLOAT freq_prev_reset[M];
