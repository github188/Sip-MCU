/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                         Version 2.1 of October 1999
*/

/*
 File : VAD.H
*/

#define     NP            12                  /* Increased LPC order */
#define     NOISE         0
#define     VOICE         1
#define     INIT_FRAME    32
#define     INIT_COUNT    20
#define     ZC_START      120
#define     ZC_END        200

/* VAD static variables */
struct vad_state_t
{
	GFLOAT MeanLSF[M];
	GFLOAT Min_buffer[16];
	GFLOAT Prev_Min, Next_Min, Min;
	GFLOAT MeanE, MeanSE, MeanSLE, MeanSZC;
	GFLOAT prev_energy;
	int count_sil, count_update, count_ext;
	int flag, v_flag, less_count;
};

void vad_init(struct vad_state_t *);

void vad(struct vad_state_t *, GFLOAT  rc, GFLOAT *lsf, GFLOAT *rxx, GFLOAT *sigpp, int frm_count,
    int prev_marker, int pprev_marker, int *marker, GFLOAT *Energy_db);

