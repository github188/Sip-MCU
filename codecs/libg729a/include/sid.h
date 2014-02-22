/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                         Version 2.1 of October 1999
*/
/*
 File : SID.H
*/

#define         sqr(a)  ((a)*(a))
#define         R_LSFQ 10

struct lsfq_state_t
{
	GFLOAT noise_fg[MODE][MA_NP][M];
};

void init_lsfq_noise(struct lsfq_state_t *);

void lsfq_noise(struct lsfq_state_t *,
				GFLOAT *lsp_new, GFLOAT *lspq,
                GFLOAT freq_prev[MA_NP][M], int *idx);

void sid_lsfq_decode(struct lsfq_state_t *,
					 int *index, GFLOAT *lspq,
                     GFLOAT freq_prev[MA_NP][M]);





