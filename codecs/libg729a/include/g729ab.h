/*-----------------------------------------------------------*
 * g729ab.h - include file for G.729A/B 8.0 kb/s codec
 *-----------------------------------------------------------*/

#ifndef _G729AB_H_INCL_
#define _G729AB_H_INCL_

#include "typedef.h"
#include "cst_ld8a.h"

#define G729_PCM_FRAME_SIZE 80
#define G729_VOICE_FRAME_SIZE 10
#define G729_SID_FRAME_SIZE    2

#define G729_TYPE_DONTSEND	0x0
#define G729_TYPE_VOICE		0x1
#define G729_TYPE_SID		0x2

struct cod_state
{
	struct cod_state_t cod_s;
	struct preproc_state_t preproc_s;

	int frame;          /* frame counter */
	int vad_enable;		/* For G.729B */
};

struct dec_state
{
	struct dec_state_t dec_s;
	struct preproc_state_t postproc_s;
	struct postfilt_state_t postfilt_s;

	GFLOAT  synth_buf[M+L_FRAME];        /* Synthesis  */
	GFLOAT  *synth;
};


void g729_init_coder(struct cod_state *, int vad_enable);
int  g729_coder(struct cod_state *, INT16 *DataBuff, unsigned char *Vout, int* poutlen);

void g729_init_decoder(struct dec_state *);
int  g729_decoder(struct dec_state *, INT16 *DataBuff, unsigned char *Vinp, int inplen);


#endif /* _G729AB_H_INCL_ */
