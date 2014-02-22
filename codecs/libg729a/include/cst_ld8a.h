/*
**
** File:        "cst_ld8a.h"
**
** Description:  This file contains global definition of the
**    CS-ACELP Coder for 8 kbps.
**
*/

#ifndef _CST_LD8A_H_INCL_
#define _CST_LD8A_H_INCL_

#include "ld8a.h"
#include "vad.h"
#include "sid.h"
#include "dtx.h"

/*
   Used structures
*/
struct cod_state_t
{
	/* Speech vector */
	GFLOAT old_speech[L_TOTAL];
	GFLOAT *speech, *p_window;
	GFLOAT *new_speech;                    /* Global variable */

	/* Weighted speech vector */
	GFLOAT old_wsp[L_FRAME+PIT_MAX];
	GFLOAT *wsp;

	/* Excitation vector */
	GFLOAT old_exc[L_FRAME+PIT_MAX+L_INTERPOL];
	GFLOAT *exc;

	/* LSP Line spectral frequencies */
	GFLOAT lsp_old[M];
	GFLOAT lsp_old_q[M];

	/* Filter's memory */
	GFLOAT  mem_w0[M], mem_w[M], mem_zero[M];
	GFLOAT  sharp;

	/* For G.729B */
	/* DTX variables */
	int pastVad;   
	int ppastVad;
	INT16 seed;

	struct lsp_cod_state_t lsp_s;
	struct cod_cng_state_t cng_s;
	struct vad_state_t vad_s;
	struct gain_state_t gain_s;
};

struct dec_state_t
{
	/* Excitation vector */
	GFLOAT old_exc[L_FRAME+PIT_MAX+L_INTERPOL];
	GFLOAT *exc;

	/* Lsp (Line spectral pairs) */
	GFLOAT lsp_old[M];

	GFLOAT mem_syn[M];       /* Synthesis filter's memory          */
	GFLOAT sharp;            /* pitch sharpening of previous frame */
	int old_t0;             /* integer delay of previous frame    */
	GFLOAT gain_code;        /* Code gain                          */
	GFLOAT gain_pitch;       /* Pitch gain                         */

	/* for G.729B */
	INT16 seed_fer;
	/* CNG variables */
	int past_ftyp;
	INT16 seed;
	GFLOAT sid_sav;

	int bad_lsf;
		/*
		   This variable should be always set to zero unless transmission errors
		   in LSP indices are detected.
		   This variable is useful if the channel coding designer decides to
		   perform error checking on these important parameters. If an error is
		   detected on the  LSP indices, the corresponding flag is
		   set to 1 signalling to the decoder to perform parameter substitution.
		   (The flags should be set back to 0 for correct transmission).
		*/

	struct lsp_dec_state_t lsp_s;
	struct dec_cng_state_t cng_s;
	struct gain_state_t gain_s;
};

#endif /* _CST_LD8A_H_INCL_ */
