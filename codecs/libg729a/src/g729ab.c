/*-------------------------------------------------------------*
 * g729ab.c - init & line packing for G.729A/B 8.0 kb/s codec  *
 *-------------------------------------------------------------*/

#include <memory.h>
#include "g729ab.h"

#if defined(_MSC_VER)
#	define __inline__	__inline
#endif

/* line packing ops */
__inline__ static void   put32_BE(unsigned char *out, INT32 bits);
__inline__ static INT32 get32_BE(unsigned char *in);
__inline__ static void   put16_BE(unsigned char *out, INT32 bits);
__inline__ static INT32 get16_BE(unsigned char *in);
__inline__ static void float_to_sint16(INT16 *dst, GFLOAT *src, int len);

void g729_line_pack(int* prm, unsigned char *Vout, int* poutlen);
void g729_line_unpack(int* prm, unsigned char *Vinp, int Ftyp);

void
g729_init_coder(struct cod_state *cod, int vad_enable)
{
	cod->frame = 0;
	cod->vad_enable = vad_enable;

	init_pre_process(&cod->preproc_s);
	init_coder_ld8a(&cod->cod_s);

	/* for G.729B */
	if (vad_enable)
		init_cod_cng(&cod->cod_s.cng_s);
}

int
g729_coder(struct cod_state *cod, INT16 *DataBuff, unsigned char *Vout, int* poutlen)
{
	int parm[1 + PRM_SIZE];        /* Analysis parameters + frame type */

	{
		int x;
		for (x = 0; x < L_FRAME; ++x)
			cod->cod_s.new_speech[x] = DataBuff[x];
	}

    if (cod->frame == 32767)
		cod->frame = 256;
    else
		cod->frame++;

    pre_process(&cod->preproc_s, cod->cod_s.new_speech, L_FRAME);
	coder_ld8a(&cod->cod_s, parm, cod->frame, cod->vad_enable);

	if (parm[0] != G729_TYPE_DONTSEND)
		g729_line_pack(parm, Vout, poutlen);
	else
		*poutlen = 0;

	return 0;
}

void
g729_init_decoder(struct dec_state *dec)
{
	init_decod_ld8a(&dec->dec_s);
	init_post_filter(&dec->postfilt_s);
	init_post_process(&dec->postproc_s);

	memset(dec->synth_buf, 0, sizeof(dec->synth_buf[0]) * M);
	dec->synth = dec->synth_buf + M;

	/* for G.729b */
	init_dec_cng(&dec->dec_s.cng_s);
}

int
g729_decoder(struct dec_state *dec, INT16 *DataBuff, unsigned char *Vinp, int inplen)
{
	int    parm[2+PRM_SIZE];            /* Synthesis parameters + BFI */
	GFLOAT  Az_dec[MP1*2];               /* Decoded Az for post-filter */
	int    T2[2];                       /* Decoded Pitch              */
	int    Ftyp;
	int    Vad;

	if (inplen == G729_VOICE_FRAME_SIZE)
		Ftyp = G729_TYPE_VOICE;
	else if (inplen == G729_SID_FRAME_SIZE)
		Ftyp = G729_TYPE_SID;
	else
		return -1;

	g729_line_unpack(parm, Vinp, Ftyp);

	decod_ld8a(&dec->dec_s, parm, dec->synth, Az_dec, T2, &Vad);
	post_filter(&dec->postfilt_s, dec->synth, Az_dec, T2, Vad);
	post_process(&dec->postproc_s, dec->synth, L_FRAME);

	float_to_sint16(DataBuff, dec->synth, L_FRAME);

	return 0;
}

void
g729_line_pack(int* prm, unsigned char *Vout, int* poutlen)
{
	if (prm[0] == G729_TYPE_DONTSEND)
	{
		*poutlen = 0;
		return;
	}
	else if (prm[0] == G729_TYPE_VOICE)
	{
		INT32 bits;

		*poutlen = G729_VOICE_FRAME_SIZE;

		bits =	((prm[1] & 0xff) << 24) | ((prm[2] & 0x3ff) << 14) |
				((prm[3] & 0xff) <<  6) | ((prm[4] & 0x001) <<  5) |
				((prm[5] >> 8) & 0x1f);
		put32_BE(Vout, bits);
		Vout += 4;
		bits =	((prm[5] & 0xff) << 24) | ((prm[6] & 0x0f) << 20) |
				((prm[7] & 0x7f) << 13) | ((prm[8] & 0x1f) <<  8) |
				((prm[9] >> 5) & 0xff);
		put32_BE(Vout, bits);
		Vout += 4;
		bits =	((prm[9] & 0x1f) << 11) | ((prm[10] & 0x0f) << 7) |
				(prm[11] & 0x7f);
		put16_BE(Vout, bits);

	}
	else if (prm[0] == G729_TYPE_SID)
	{
		INT32 bits;

		*poutlen = G729_SID_FRAME_SIZE;

		bits =	((prm[1] << 15) & 0x01) | ((prm[2] << 10) & 0x1f) |
				((prm[3] <<  6) & 0x0f) | ((prm[4] <<  1) & 0x1f);
		put16_BE(Vout, bits);
	}
	else
		*poutlen = 0;
}

void
g729_line_unpack(int* prm, unsigned char *Vinp, int Ftyp)
{
	prm[0] = 0;  /* no frame erasures */
	prm[1] = Ftyp;
	
	if (Ftyp == G729_TYPE_DONTSEND)
	{
		prm[0] = 1;	/* frame erased */
	}
	else if (Ftyp == G729_TYPE_VOICE)
	{
		INT32 bits[3];

		bits[0] = get32_BE(Vinp);
		Vinp += 4;
		bits[1] = get32_BE(Vinp);
		Vinp += 4;
		bits[2] = get16_BE(Vinp);

		prm[ 2] = (bits[0] >> 24) & 0x0ff;
		prm[ 3] = (bits[0] >> 14) & 0x3ff;
		prm[ 4] = (bits[0] >>  6) & 0x0ff;
		prm[ 5] = (bits[0] >>  5) & 0x001;
		prm[ 6] = ((bits[0] & 0x1f) << 8) | ((bits[1] >> 24) & 0xff);
		prm[ 7] = (bits[1] >> 20) & 0x00f;
		prm[ 8] = (bits[1] >> 13) & 0x07f;
		prm[ 9] = (bits[1] >>  8) & 0x01f;
		prm[10] = ((bits[1] & 0xff) << 5) | ((bits[2] >> 11) & 0x1f);
		prm[11] = (bits[2] >>  7) & 0x0f;
		prm[12] = (bits[2] & 0x7f);

		/* check parity and put 1 in parm[5] if parity error */
		prm[5] = check_parity_pitch(prm[4], prm[5]);
		
	}
	else if (Ftyp == G729_TYPE_SID)
	{
		INT32 bits;

		bits = get16_BE(Vinp);

		prm[2] = (bits >> 15) & 0x01;
		prm[3] = (bits >> 10) & 0x1f;
		prm[4] = (bits >>  6) & 0x0f;
		prm[5] = (bits >>  1) & 0x1f;

	}
	else
	{
		prm[0] = 1;	/* frame erased */
		prm[1] = G729_TYPE_DONTSEND;
	}
}

__inline__ static void
put32_BE(unsigned char *out, INT32 bits)
{
	out[0] = bits >> 24;
	out[1] = bits >> 16;
	out[2] = bits >> 8;
	out[3] = bits;
}

__inline__ static INT32
get32_BE(unsigned char *in)
{
	return (in[0] << 24) | (in[1] << 16) | (in[2] << 8) | in[3];
}

__inline__ static void
put16_BE(unsigned char *out, INT32 bits)
{
	out[0] = bits >> 8;
	out[1] = bits;
}

__inline__ static INT32
get16_BE(unsigned char *in)
{
	return (in[0] << 8) | in[1];
}

__inline__ static void
float_to_sint16(INT16 *dst, GFLOAT *src, int len)
{
	for ( ; len; --len, ++dst, ++src)
	{	/* round and convert to int  */
		GFLOAT t = *src;
		if (t >= (F)0.0)
			t += (F)0.5;
		else
			t -= (F)0.5;

		if (t > (F)32767.0)
			*dst = 32767;
		else if (t < (F)-32768.0)
			*dst = -32768;
		else
			*dst = (INT16) t;
	}
}
