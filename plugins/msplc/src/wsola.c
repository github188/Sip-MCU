/* $Id: wsola.c 3255 2010-08-06 07:18:08Z nanang $ */
/* 
* Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
* Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/
#include <math.h>

#include "wsola.h"
#include "circbuf.h"
/*
* This file contains implementation of WSOLA using MS_WSOLA_IMP_WSOLA
* or MS_WSOLA_IMP_NULL
*/
#define THIS_FILE   "wsola.c"

/*
* http://trac.pjsip.org/repos/ticket/683:
*  Workaround for segfault problem in the fixed point version of create_win()
*  on ARM9 platform, possibly due to gcc optimization bug.
*
*  For now, we will use linear window when floating point is disabled.
*/
#ifndef MS_WSOLA_LINEAR_WIN
#   define MS_WSOLA_LINEAR_WIN    (!HAS_FLOATING_POINT)
#endif


#if 0
#   define TRACE_(x)	LOG(4,x)
#else
#   define TRACE_(x)
#endif

#if 0
#   define CHECK_(x)	pj_assert(x)
#else
#   define CHECK_(x)
#endif


#if (MS_WSOLA_IMP==MS_WSOLA_IMP_WSOLA) || \
	(MS_WSOLA_IMP==MS_WSOLA_IMP_WSOLA_LITE)

/*
* WSOLA implementation using WSOLA
*/

/* Buffer size including history, in frames */
#define FRAME_CNT	6

/* Number of history frames in buffer */
#define HIST_CNT	1.5

/* Template size, in msec */
#define TEMPLATE_PTIME	MS_WSOLA_TEMPLATE_LENGTH_MSEC

/* Hanning window size, in msec */
#define HANNING_PTIME	MS_WSOLA_DELAY_MSEC

/* Number of frames in erase buffer */
#define ERASE_CNT	((unsigned)3)

/* Minimum distance from template for find_pitch() of expansion, in frames */
#define EXP_MIN_DIST	0.5

/* Maximum distance from template for find_pitch() of expansion, in frames */
#define EXP_MAX_DIST	HIST_CNT

/* Duration of a continuous synthetic frames after which the volume 
* of the synthetic frame will be set to zero with fading-out effect.
*/
#define MAX_EXPAND_MSEC	MS_WSOLA_MAX_EXPAND_MSEC


/* Buffer content:
*
*  +---------+-----------+--------------------+
*  | history | min_extra | more extra / empty |
*  +---------+-----------+--------------------+
*  ^         ^           ^                    ^
* buf    hist_size   min_extra            buf_size
* 
* History size (hist_size) is a constant value, initialized upon creation.
*
* min_extra size is equal to HANNING_PTIME, this samples is useful for 
* smoothening samples transition between generated frame & history 
* (when PLC is invoked), or between generated samples & normal frame 
* (after lost/PLC). Since min_extra samples need to be available at 
* any time, this will introduce delay of HANNING_PTIME ms.
*
* More extra is excess samples produced by PLC (PLC frame generation may 
* produce more than exact one frame).
*
* At any particular time, the buffer will contain at least (hist_size + 
* min_extra) samples.
*
* A "save" operation will append the new frame to the end of the buffer,
* return the frame from samples right after history and shift the buffer
* by one frame.
*
*/

/* WSOLA structure */
struct ms_wsola
{
	unsigned		 clock_rate;	    /* Sampling rate.		    */
	uint16_t		 samples_per_frame; /* Samples per frame (const)    */
	uint16_t		 channel_count;	    /* Channel countt (const)	    */
	uint16_t		 options;	    /* Options.			    */

	ms_circ_buf	*buf;		    /* The buffer.		    */
	int16_t		*erase_buf;	    /* Temporary erase buffer.	    */
	int16_t		*merge_buf;	    /* Temporary merge buffer.	    */

	uint16_t		 buf_size;	    /* Total buffer size (const)    */
	uint16_t		 hanning_size;	    /* Hanning window size (const)  */
	uint16_t		 templ_size;	    /* Template size (const)	    */
	uint16_t		 hist_size;	    /* History size (const)	    */

	uint16_t		 min_extra;	    /* Minimum extra (const)	    */
	unsigned		 max_expand_cnt;    /* Max # of synthetic samples   */
	unsigned		 fade_out_pos;	    /* Last fade-out position	    */
	uint16_t		 expand_sr_min_dist;/* Minimum distance from template 
										for find_pitch() on expansion
										(const)			    */
	uint16_t		 expand_sr_max_dist;/* Maximum distance from template 
										for find_pitch() on expansion
										(const)			    */

#if defined(HAS_FLOATING_POINT) && HAS_FLOATING_POINT!=0
	float		*hanning;	    /* Hanning window.		    */
#else
	uint16_t		*hanning;	    /* Hanning window.		    */
#endif

	uint32_t	 ts;		    /* Running timestamp.	    */

};

#if (MS_WSOLA_IMP==MS_WSOLA_IMP_WSOLA_LITE)

/* In this implementation, waveform similarity comparison is done by calculating
* the difference of total level between template frame and the target buffer 
* for each template_cnt samples. The smallest difference value assumed to be 
* the most similar block. This seems to be naive, however some tests show
* acceptable results and the processing speed is amazing.
*
* diff level = (template[1]+..+template[n]) - (target[1]+..+target[n])
*/
static int16_t *find_pitch(int16_t *frm, int16_t *beg, int16_t *end, 
						   unsigned template_cnt, int first)
{
	int16_t *sr, *best=beg;
	int best_corr = 0x7FFFFFFF;
	int frm_sum = 0;
	unsigned i;

	for (i = 0; i<template_cnt; ++i)
		frm_sum += frm[i];

	for (sr=beg; sr!=end; ++sr) {
		int corr = frm_sum;
		int abs_corr = 0;

		/* Do calculation on 8 samples at once */
		for (i = 0; i<template_cnt-8; i+=8) {
			corr -= (int)sr[i+0] +
				(int)sr[i+1] +
				(int)sr[i+2] +
				(int)sr[i+3] +
				(int)sr[i+4] +
				(int)sr[i+5] +
				(int)sr[i+6] +
				(int)sr[i+7];
		}

		/* Process remaining samples */
		for (; i<template_cnt; ++i)
			corr -= (int)sr[i];

		abs_corr = corr > 0? corr : -corr;

		if (first) {
			if (abs_corr < best_corr) {
				best_corr = abs_corr;
				best = sr;
			}
		} else {
			if (abs_corr <= best_corr) {
				best_corr = abs_corr;
				best = sr;
			}
		}
	}

	/*TRACE_((THIS_FILE, "found pitch at %u", best-beg));*/
	return best;
}

#endif

#if defined(HAS_FLOATING_POINT) && HAS_FLOATING_POINT!=0
/*
* Floating point version.
*/

#if (MS_WSOLA_IMP==MS_WSOLA_IMP_WSOLA)

static int16_t *find_pitch(int16_t *frm, int16_t *beg, int16_t *end, 
						   unsigned template_cnt, int first)
{
	int16_t *sr, *best=beg;
	double best_corr = 0;

	for (sr=beg; sr!=end; ++sr) {
		double corr = 0;
		unsigned i;

		/* Do calculation on 8 samples at once */
		for (i=0; i<template_cnt-8; i += 8) {
			corr += ((float)frm[i+0]) * ((float)sr[i+0]) + 
				((float)frm[i+1]) * ((float)sr[i+1]) + 
				((float)frm[i+2]) * ((float)sr[i+2]) + 
				((float)frm[i+3]) * ((float)sr[i+3]) + 
				((float)frm[i+4]) * ((float)sr[i+4]) + 
				((float)frm[i+5]) * ((float)sr[i+5]) + 
				((float)frm[i+6]) * ((float)sr[i+6]) + 
				((float)frm[i+7]) * ((float)sr[i+7]);
		}

		/* Process remaining samples. */
		for (; i<template_cnt; ++i) {
			corr += ((float)frm[i]) * ((float)sr[i]);
		}

		if (first) {
			if (corr > best_corr) {
				best_corr = corr;
				best = sr;
			}
		} else {
			if (corr >= best_corr) {
				best_corr = corr;
				best = sr;
			}
		}
	}

	/*TRACE_((THIS_FILE, "found pitch at %u", best-beg));*/
	return best;
}

#endif

static void overlapp_add(int16_t dst[], unsigned count,
						 int16_t l[], int16_t r[],
						 float w[])
{
	unsigned i;

	for (i=0; i<count; ++i) {
		dst[i] = (int16_t)(l[i] * w[count-1-i] + r[i] * w[i]);
	}
}

static void overlapp_add_simple(int16_t dst[], unsigned count,
								int16_t l[], int16_t r[])
{
	float step = (float)(1.0 / count), stepdown = 1.0;
	unsigned i;

	for (i=0; i<count; ++i) {
		dst[i] = (int16_t)(l[i] * stepdown + r[i] * (1-stepdown));
		stepdown -= step;
	}
}

static void create_win(float **pw, unsigned count)
{
	unsigned i;
	float *w = (float*)ms_new0(float, count);

	*pw = w;

	for (i=0;i<count; i++) {
		w[i] = (float)(0.5 - 0.5 * cos(2.0 * PI * i / (count*2-1)) );
	}
}

#else	/* HAS_FLOATING_POINT */
/*
* Fixed point version.
*/
#define WINDOW_BITS	15
enum { WINDOW_MAX_VAL = (1 << WINDOW_BITS)-1 };

#if (MS_WSOLA_IMP==MS_WSOLA_IMP_WSOLA)

static int16_t *find_pitch(int16_t *frm, int16_t *beg, int16_t *end, 
						   unsigned template_cnt, int first)
{
	int16_t *sr, *best=beg;
	int32_t best_corr = 0;


	for (sr=beg; sr!=end; ++sr) {
		int32_t corr = 0;
		unsigned i;

		/* Do calculation on 8 samples at once */
		for (i=0; i<template_cnt-8; i+=8) {
			corr += ((int)frm[i+0]) * ((int)sr[i+0]) + 
				((int)frm[i+1]) * ((int)sr[i+1]) + 
				((int)frm[i+2]) * ((int)sr[i+2]) +
				((int)frm[i+3]) * ((int)sr[i+3]) +
				((int)frm[i+4]) * ((int)sr[i+4]) +
				((int)frm[i+5]) * ((int)sr[i+5]) +
				((int)frm[i+6]) * ((int)sr[i+6]) +
				((int)frm[i+7]) * ((int)sr[i+7]);
		}

		/* Process remaining samples. */
		for (; i<template_cnt; ++i) {
			corr += ((int)frm[i]) * ((int)sr[i]);
		}

		if (first) {
			if (corr > best_corr) {
				best_corr = corr;
				best = sr;
			}
		} else {
			if (corr >= best_corr) {
				best_corr = corr;
				best = sr;
			}
		}
	}

	/*TRACE_((THIS_FILE, "found pitch at %u", best-beg));*/
	return best;
}

#endif


static void overlapp_add(int16_t dst[], unsigned count,
						 int16_t l[], int16_t r[],
						 uint16_t w[])
{
	unsigned i;

	for (i=0; i<count; ++i) {
		dst[i] = (int16_t)(((int)(l[i]) * (int)(w[count-1-i]) + 
			(int)(r[i]) * (int)(w[i])) >> WINDOW_BITS);
	}
}

static void overlapp_add_simple(int16_t dst[], unsigned count,
								int16_t l[], int16_t r[])
{
	int step = ((WINDOW_MAX_VAL+1) / count), 
		stepdown = WINDOW_MAX_VAL;
	unsigned i;

	for (i=0; i<count; ++i) {
		dst[i]=(int16_t)((l[i] * stepdown + r[i] * (1-stepdown)) >> WINDOW_BITS);
		stepdown -= step;
	}
}

#if HAS_INT64 && !MS_WSOLA_LINEAR_WIN
/* approx_cos():
*   see: http://www.audiomulch.com/~rossb/code/sinusoids/ 
*/
static pj_uint32_t approx_cos( pj_uint32_t x )
{
	pj_uint32_t i,j,k;

	if( x == 0 )
		return 0xFFFFFFFF;

	i = x << 1;
	k = ((x + 0xBFFFFFFD) & 0x80000000) >> 30;
	j = i - i * ((i & 0x80000000)>>30);
	j = j >> 15;
	j = (j * j + j) >> 1;
	j = j - j * k;

	return j;
}
#endif	/* HAS_INT64 && .. */

static void create_win(pj_pool_t *pool, uint16_t **pw, unsigned count)
{

	unsigned i;
	uint16_t *w = (uint16_t*)ms_new0(uint16_t,count);

	*pw = w;

	for (i=0; i<count; i++) {
#if HAS_INT64 && !MS_WSOLA_LINEAR_WIN
		pj_uint32_t phase;
		pj_uint64_t cos_val;

		/* w[i] = (float)(0.5 - 0.5 * cos(2.0 * PI * i / (count*2-1)) ); */

		phase = (pj_uint32_t)(INT64(0xFFFFFFFF) * i / (count*2-1));
		cos_val = approx_cos(phase);

		w[i] = (uint16_t)(WINDOW_MAX_VAL - 
			(WINDOW_MAX_VAL * cos_val) / 0xFFFFFFFF);
#else
		/* Revert to linear */
		w[i] = (uint16_t)(i * WINDOW_MAX_VAL / count);
#endif
	}
}

#endif	/* HAS_FLOATING_POINT */

/* Apply fade-in to the buffer.
*  - fade_cnt is the number of samples on which the volume
*       will go from zero to 100%
*  - fade_pos is current sample position within fade_cnt range.
*       It is zero for the first sample, so the first sample will
*	 have zero volume. This value is increasing.
*/
static void fade_in(int16_t buf[], int count,
					int fade_in_pos, int fade_cnt)
{
#if defined(HAS_FLOATING_POINT) && HAS_FLOATING_POINT!=0
	float fade_pos = (float)fade_in_pos;
#else
	int fade_pos = fade_in_pos;
#endif

	if (fade_cnt - fade_pos < count) {
		for (; fade_pos < fade_cnt; ++fade_pos, ++buf) {
			*buf = (int16_t)(*buf * fade_pos / fade_cnt);
		}
		/* Leave the remaining samples as is */
	} else {
		int16_t *end = buf + count;
		for (; buf != end; ++fade_pos, ++buf) {
			*buf = (int16_t)(*buf * fade_pos / fade_cnt);
		}
	}
}

/* Apply fade-out to the buffer. */
static void wsola_fade_out(ms_wsola *wsola, 
						   int16_t buf[], int count)
{
	int16_t *end = buf + count;
	int fade_cnt = wsola->max_expand_cnt;
#if defined(HAS_FLOATING_POINT) && HAS_FLOATING_POINT!=0
	float fade_pos = (float)wsola->fade_out_pos;
#else
	int fade_pos = wsola->fade_out_pos;
#endif

	if (wsola->fade_out_pos == 0) {
		ms_zero_samples(buf, count);
	} else if (fade_pos < count) {
		for (; fade_pos; --fade_pos, ++buf) {
			*buf = (int16_t)(*buf * fade_pos / fade_cnt);
		}
		if (buf != end)
			ms_zero_samples(buf, end - buf);
		wsola->fade_out_pos = 0;
	} else {
		for (; buf != end; --fade_pos, ++buf) {
			*buf = (int16_t)(*buf * fade_pos / fade_cnt);
		}
		wsola->fade_out_pos -= count;
	}
}


int  ms_wsola_create(unsigned clock_rate,
						  unsigned samples_per_frame,
						  unsigned channel_count,
						  unsigned options,
						  ms_wsola **p_wsola)
{
	ms_wsola *wsola;
	int  status;

	/* Allocate wsola and initialize vars */
	wsola = (ms_wsola *)ms_new0(ms_wsola, 1);
	wsola->clock_rate= (uint16_t) clock_rate;
	wsola->samples_per_frame = (uint16_t) samples_per_frame;
	wsola->channel_count = (uint16_t) channel_count;
	wsola->options = (uint16_t) options;
	wsola->max_expand_cnt = clock_rate * MAX_EXPAND_MSEC / 1000;
	wsola->fade_out_pos = wsola->max_expand_cnt;

	/* Create circular buffer */
	wsola->buf_size = (uint16_t) (samples_per_frame * FRAME_CNT);
	status = ms_circ_buf_create(wsola->buf_size, &wsola->buf);
	if (status != 0) {
		ms_message("Failed to create circular buf");
		return status;
	}

	/* Calculate history size */
	wsola->hist_size = (uint16_t)(HIST_CNT * samples_per_frame);

	/* Calculate template size */
	wsola->templ_size = (uint16_t)(TEMPLATE_PTIME * clock_rate * 
		channel_count / 1000);
	if (wsola->templ_size > samples_per_frame)
		wsola->templ_size = wsola->samples_per_frame;

	/* Calculate hanning window size */
	wsola->hanning_size = (uint16_t)(HANNING_PTIME * clock_rate * 
		channel_count / 1000);
	if (wsola->hanning_size > wsola->samples_per_frame)
		wsola->hanning_size = wsola->samples_per_frame;

	pj_assert(wsola->templ_size <= wsola->hanning_size);

	/* Create merge buffer */
	wsola->merge_buf = (int16_t*) ms_new0(int16_t, 
		wsola->hanning_size);

	/* Setup with PLC */
	if ((options & MS_WSOLA_NO_PLC) == 0) {
		wsola->min_extra = wsola->hanning_size;
		wsola->expand_sr_min_dist = (uint16_t)
			(EXP_MIN_DIST * wsola->samples_per_frame);
		wsola->expand_sr_max_dist = (uint16_t)
			(EXP_MAX_DIST * wsola->samples_per_frame);
	}

	/* Setup with hanning */
	if ((options & MS_WSOLA_NO_HANNING) == 0) {
		create_win(&wsola->hanning, wsola->hanning_size);
	}

	/* Setup with discard */
	if ((options & MS_WSOLA_NO_DISCARD) == 0) {
		wsola->erase_buf = (int16_t*)ms_new0(int16_t, samples_per_frame *
			ERASE_CNT);
	}

	/* Generate dummy extra */
	ms_circ_buf_set_len(wsola->buf, wsola->hist_size + wsola->min_extra);

	*p_wsola = wsola;
	return 0;

}


int  ms_wsola_set_max_expand(ms_wsola *wsola, unsigned msec)
{
	ASSERT_RETURN(wsola, EINVAL);
	wsola->max_expand_cnt = msec * wsola->clock_rate / 1000;
	return 0;
}

int  ms_wsola_reset( ms_wsola *wsola,
						 unsigned options)
{
	ASSERT_RETURN(wsola && options==0, EINVAL);
	UNUSED_ARG(options);

	ms_circ_buf_reset(wsola->buf);
	ms_circ_buf_set_len(wsola->buf, wsola->hist_size + wsola->min_extra);
	ms_zero_samples(wsola->buf->start, wsola->buf->len); 
	wsola->fade_out_pos = wsola->max_expand_cnt;

	return 0;
}


static void expand(ms_wsola *wsola, unsigned needed)
{
	unsigned generated = 0;
	unsigned rep;

	int16_t *reg1, *reg2;
	unsigned reg1_len, reg2_len;

	ms_circ_buf_pack_buffer(wsola->buf);
	ms_circ_buf_get_read_regions(wsola->buf, &reg1, &reg1_len, 
		&reg2, &reg2_len);
	CHECK_(reg2_len == 0);

	for (rep=1;; ++rep) {
		int16_t *start, *templ;
		unsigned dist;

		templ = reg1 + reg1_len - wsola->hanning_size;
		CHECK_(templ - reg1 >= wsola->hist_size);

		start = find_pitch(templ, 
			templ - wsola->expand_sr_max_dist, 
			templ - wsola->expand_sr_min_dist,
			wsola->templ_size, 
			1);

		/* Should we make sure that "start" is really aligned to
		* channel #0, in case of stereo? Probably not necessary, as
		* find_pitch() should have found the best match anyway.
		*/

		if (wsola->options & MS_WSOLA_NO_HANNING) {
			overlapp_add_simple(wsola->merge_buf, wsola->hanning_size, 
				templ, start);
		} else {
			/* Check if pointers are in the valid range */
			CHECK_(templ >= wsola->buf->buf &&
				templ + wsola->hanning_size <= 
				wsola->buf->buf + wsola->buf->capacity);
			CHECK_(start >= wsola->buf->buf &&
				start + wsola->hanning_size <= 
				wsola->buf->buf + wsola->buf->capacity);

			overlapp_add(wsola->merge_buf, wsola->hanning_size, templ, 
				start, wsola->hanning);
		}

		/* How many new samples do we have */
		dist = templ - start;

		/* Not enough buffer to hold the result */
		if (reg1_len + dist > wsola->buf_size) {
			pj_assert(!"WSOLA buffer size may be to small!");
			break;
		}

		/* Copy the "tail" (excess frame) to the end */
		ms_move_samples(templ + wsola->hanning_size, 
			start + wsola->hanning_size,
			dist);

		/* Copy the merged frame */
		ms_copy_samples(templ, wsola->merge_buf, wsola->hanning_size);

		/* We have new samples */
		reg1_len += dist;
		ms_circ_buf_set_len(wsola->buf, reg1_len);

		generated += dist;

		if (generated >= needed) {
			TRACE_((THIS_FILE, "WSOLA frame expanded after %d iterations", 
				rep));
			break;
		}
	}
}


static unsigned compress(ms_wsola *wsola, int16_t *buf, unsigned count,
						 unsigned del_cnt)
{
	unsigned samples_del = 0, rep;

	for (rep=1; ; ++rep) {
		int16_t *start, *end;
		unsigned dist;

		if (count <= wsola->hanning_size + del_cnt) {
			TRACE_((THIS_FILE, "Not enough samples to compress!"));
			return samples_del;
		}

		// Make start distance to del_cnt, so discard will be performed in
		// only one iteration.
		//start = buf + (frmsz >> 1);
		start = buf + del_cnt - samples_del;
		end = start + wsola->samples_per_frame;

		if (end + wsola->hanning_size > buf + count) {
			end = buf+count-wsola->hanning_size;
		}

		CHECK_(start < end);

		start = find_pitch(buf, start, end, wsola->templ_size, 0);
		dist = start - buf;

		if (wsola->options & MS_WSOLA_NO_HANNING) {
			overlapp_add_simple(buf, wsola->hanning_size, buf, start);
		} else {
			overlapp_add(buf, wsola->hanning_size, buf, start, wsola->hanning);
		}

		ms_move_samples(buf + wsola->hanning_size, 
			buf + wsola->hanning_size + dist,
			count - wsola->hanning_size - dist);

		count -= dist;
		samples_del += dist;

		if (samples_del >= del_cnt) {
			TRACE_((THIS_FILE, 
				"Erased %d of %d requested after %d iteration(s)",
				samples_del, del_cnt, rep));
			break;
		}
	}

	return samples_del;
}



int  ms_wsola_save( ms_wsola *wsola, 
						int16_t frm[], 
						bool_t prev_lost)
{
	unsigned buf_len;
	int  status;

	buf_len = ms_circ_buf_get_len(wsola->buf);

	/* Update vars */
	wsola->ts += wsola->samples_per_frame;

	/* If previous frame was lost, smoothen this frame with the generated one */
	if (prev_lost) {
		int16_t *reg1, *reg2;
		unsigned reg1_len, reg2_len;
		int16_t *ola_left;

		/* Trim excessive len */
		if ((int)buf_len > wsola->hist_size + (wsola->min_extra<<1)) {
			buf_len = wsola->hist_size + (wsola->min_extra<<1);
			ms_circ_buf_set_len(wsola->buf, buf_len);
		}

		ms_circ_buf_get_read_regions(wsola->buf, &reg1, &reg1_len, 
			&reg2, &reg2_len);

		CHECK_(ms_circ_buf_get_len(wsola->buf) >= 
			(unsigned)(wsola->hist_size + (wsola->min_extra<<1)));

		/* Continue applying fade out to the extra samples */
		if ((wsola->options & MS_WSOLA_NO_FADING)==0) {
			if (reg2_len == 0) {
				wsola_fade_out(wsola, reg1 + reg1_len - (wsola->min_extra<<1),
					(wsola->min_extra<<1));
			} else if ((int)reg2_len >= (wsola->min_extra<<1)) {
				wsola_fade_out(wsola, reg2 + reg2_len - (wsola->min_extra<<1),
					(wsola->min_extra<<1));
			} else {
				unsigned tmp = (wsola->min_extra<<1) - reg2_len;
				wsola_fade_out(wsola, reg1 + reg1_len - tmp, tmp);
				wsola_fade_out(wsola, reg2, reg2_len);
			}
		}

		/* Get the region in buffer to be merged with the frame */
		if (reg2_len == 0) {
			ola_left = reg1 + reg1_len - wsola->min_extra;
		} else if (reg2_len >= wsola->min_extra) {
			ola_left = reg2 + reg2_len - wsola->min_extra;
		} else {
			unsigned tmp;

			tmp = wsola->min_extra - reg2_len;
			ms_copy_samples(wsola->merge_buf, reg1 + reg1_len - tmp, tmp);
			ms_copy_samples(wsola->merge_buf + tmp, reg2, reg2_len);
			ola_left = wsola->merge_buf;
		}

		/* Apply fade-in to the frame before merging */
		if ((wsola->options & MS_WSOLA_NO_FADING)==0) {
			unsigned count = wsola->min_extra;
			int fade_in_pos;

			/* Scale fade_in position based on last fade-out */
			fade_in_pos = wsola->fade_out_pos * count /
				wsola->max_expand_cnt;

			/* Fade-in it */
			fade_in(frm, wsola->samples_per_frame,
				fade_in_pos, count);
		}

		/* Merge it */
		overlapp_add_simple(frm, wsola->min_extra, ola_left, frm);

		/* Trim len */
		buf_len -= wsola->min_extra;
		ms_circ_buf_set_len(wsola->buf, buf_len);

	} else if ((wsola->options & MS_WSOLA_NO_FADING)==0 &&
		wsola->fade_out_pos != wsola->max_expand_cnt) 
	{
		unsigned count = wsola->min_extra;
		int fade_in_pos;

		/* Fade out the remaining synthetic samples */
		if (buf_len > wsola->hist_size) {
			int16_t *reg1, *reg2;
			unsigned reg1_len, reg2_len;

			/* Number of samples to fade out */
			count = buf_len - wsola->hist_size;

			ms_circ_buf_get_read_regions(wsola->buf, &reg1, &reg1_len, 
				&reg2, &reg2_len);

			CHECK_(ms_circ_buf_get_len(wsola->buf) >= 
				(unsigned)(wsola->hist_size + (wsola->min_extra<<1)));

			/* Continue applying fade out to the extra samples */
			if (reg2_len == 0) {
				wsola_fade_out(wsola, reg1 + reg1_len - count, count);
			} else if (reg2_len >= count) {
				wsola_fade_out(wsola, reg2 + reg2_len - count, count);
			} else {
				unsigned tmp = count - reg2_len;
				wsola_fade_out(wsola, reg1 + reg1_len - tmp, tmp);
				wsola_fade_out(wsola, reg2, reg2_len);
			}
		}

		/* Apply fade-in to the frame */
		count = wsola->min_extra;

		/* Scale fade_in position based on last fade-out */
		fade_in_pos = wsola->fade_out_pos * count /
			wsola->max_expand_cnt;

		/* Fade it in */
		fade_in(frm, wsola->samples_per_frame,
			fade_in_pos, count);

	}

	wsola->fade_out_pos = wsola->max_expand_cnt;

	status = ms_circ_buf_write(wsola->buf, frm, wsola->samples_per_frame);
	if (status != 0) {
		TRACE_((THIS_FILE, "Failed writing to circbuf [err=%d]", status));
		return status;
	}

	status = ms_circ_buf_copy(wsola->buf, wsola->hist_size, frm, 
		wsola->samples_per_frame);
	if (status != 0) {
		TRACE_((THIS_FILE, "Failed copying from circbuf [err=%d]", status));
		return status;
	}

	return ms_circ_buf_adv_read_ptr(wsola->buf, wsola->samples_per_frame);
}


int  ms_wsola_generate( ms_wsola *wsola, 
							int16_t frm[])
{
	unsigned samples_len, samples_req;
	int  status = 0;

	CHECK_(ms_circ_buf_get_len(wsola->buf) >= wsola->hist_size + 
		wsola->min_extra);

	/* Calculate how many samples in the buffer */
	samples_len = ms_circ_buf_get_len(wsola->buf) - wsola->hist_size;

	/* Calculate how many samples are required to be available in the buffer */
	samples_req = wsola->samples_per_frame + (wsola->min_extra << 1);

	wsola->ts += wsola->samples_per_frame;

	if (samples_len < samples_req) {
		/* Expand buffer */
		expand(wsola, samples_req - samples_len);
		TRACE_((THIS_FILE, "Buf size after expanded = %d", 
			ms_circ_buf_get_len(wsola->buf)));
	}

	status = ms_circ_buf_copy(wsola->buf, wsola->hist_size, frm, 
		wsola->samples_per_frame);
	if (status != 0) {
		TRACE_((THIS_FILE, "Failed copying from circbuf [err=%d]", status));
		return status;
	}

	ms_circ_buf_adv_read_ptr(wsola->buf, wsola->samples_per_frame);

	/* Apply fade-out to the frame */
	if ((wsola->options & MS_WSOLA_NO_FADING)==0) {
		wsola_fade_out(wsola, frm, wsola->samples_per_frame);
	}

	return 0;
}

int ms_wsola_destroy(ms_wsola *wsola)
{
	ms_circ_buf_destory(wsola->buf);
	if(wsola->hanning) ms_free(wsola->hanning);
	if(wsola->merge_buf) ms_free(wsola->merge_buf);
	ms_free(wsola);
	return 0;
}

int  ms_wsola_discard( ms_wsola *wsola, 
						   int16_t buf1[],
						   unsigned buf1_cnt, 
						   int16_t buf2[],
						   unsigned buf2_cnt,
						   unsigned *del_cnt)
{
//	ASSERT_RETURN(wsola && buf1 && buf1_cnt && del_cnt, EINVAL);
//	ASSERT_RETURN(*del_cnt, EINVAL);

	if (buf2_cnt == 0) {
		/* The whole buffer is contiguous space, straight away. */
		*del_cnt = compress(wsola, buf1, buf1_cnt, *del_cnt);
	} else {
	//	ASSERT_RETURN(buf2, EINVAL);

		if (buf1_cnt < ERASE_CNT * wsola->samples_per_frame &&
			buf2_cnt < ERASE_CNT * wsola->samples_per_frame &&
			wsola->erase_buf == NULL)
		{
			/* We need erase_buf but WSOLA was created with 
			* MS_WSOLA_NO_DISCARD flag.
			*/
			pj_assert(!"WSOLA need erase buffer!");
			return EINVALIDOP;
		}

		if (buf2_cnt >= ERASE_CNT * wsola->samples_per_frame) {
			/* Enough space to perform compress in the second buffer. */
			*del_cnt = compress(wsola, buf2, buf2_cnt, *del_cnt);
		} else if (buf1_cnt >= ERASE_CNT * wsola->samples_per_frame) {
			/* Enough space to perform compress in the first buffer, but then
			* we need to re-arrange the buffers so there is no gap between 
			* buffers.
			*/
			unsigned max;

			*del_cnt = compress(wsola, buf1, buf1_cnt, *del_cnt);

			max = *del_cnt;
			if (max > buf2_cnt)
				max = buf2_cnt;

			ms_move_samples(buf1 + buf1_cnt - (*del_cnt), buf2, max);

			if (max < buf2_cnt) {
				ms_move_samples(buf2, buf2+(*del_cnt), 
					buf2_cnt-max);
			}
		} else {
			/* Not enough samples in either buffers to perform compress. 
			* Need to combine the buffers in a contiguous space, the erase_buf.
			*/
			unsigned buf_size = buf1_cnt + buf2_cnt;
			int16_t *rem;	/* remainder */
			unsigned rem_cnt;

			if (buf_size > ERASE_CNT * wsola->samples_per_frame) {
				buf_size = ERASE_CNT * wsola->samples_per_frame;

				rem_cnt = buf1_cnt + buf2_cnt - buf_size;
				rem = buf2 + buf2_cnt - rem_cnt;

			} else {
				rem = NULL;
				rem_cnt = 0;
			}

			ms_copy_samples(wsola->erase_buf, buf1, buf1_cnt);
			ms_copy_samples(wsola->erase_buf+buf1_cnt, buf2, 
				buf_size-buf1_cnt);

			*del_cnt = compress(wsola, wsola->erase_buf, buf_size, *del_cnt);

			buf_size -= (*del_cnt);

			/* Copy back to buffers */
			if (buf_size == buf1_cnt) {
				ms_copy_samples(buf1, wsola->erase_buf, buf_size);
				if (rem_cnt) {
					ms_move_samples(buf2, rem, rem_cnt);
				}
			} else if (buf_size < buf1_cnt) {
				ms_copy_samples(buf1, wsola->erase_buf, buf_size);
				if (rem_cnt) {
					unsigned c = rem_cnt;
					if (c > buf1_cnt-buf_size) {
						c = buf1_cnt-buf_size;
					}
					ms_copy_samples(buf1+buf_size, rem, c);
					rem += c;
					rem_cnt -= c;
					if (rem_cnt)
						ms_move_samples(buf2, rem, rem_cnt);
				}
			} else {
				ms_copy_samples(buf1, wsola->erase_buf, buf1_cnt);
				ms_copy_samples(buf2, wsola->erase_buf+buf1_cnt, 
					buf_size-buf1_cnt);
				if (rem_cnt) {
					ms_move_samples(buf2+buf_size-buf1_cnt, rem,
						rem_cnt);
				}
			}

		}
	}

	return (*del_cnt) > 0 ? 0 : ETOOSMALL;
}


#elif MS_WSOLA_IMP==MS_WSOLA_IMP_NULL
/*
* WSOLA implementation using NULL
*/

struct ms_wsola
{
	unsigned samples_per_frame;
};


int  ms_wsola_create( pj_pool_t *pool, 
						  unsigned clock_rate,
						  unsigned samples_per_frame,
						  unsigned channel_count,
						  unsigned options,
						  ms_wsola **p_wsola)
{
	ms_wsola *wsola;

	wsola = POOL_ZALLOC_T(pool, struct ms_wsola);
	wsola->samples_per_frame = samples_per_frame;

	UNUSED_ARG(clock_rate);
	UNUSED_ARG(channel_count);
	UNUSED_ARG(options);

	*p_wsola = wsola;

	return 0;
}


int  ms_wsola_destroy(ms_wsola *wsola)
{
	UNUSED_ARG(wsola);
	return 0;
}


int  ms_wsola_reset( ms_wsola *wsola,
						 unsigned options)
{
	UNUSED_ARG(wsola);
	UNUSED_ARG(options);

	return 0;
}


int  ms_wsola_save( ms_wsola *wsola, 
						int16_t frm[], 
						bool_t prev_lost)
{
	UNUSED_ARG(wsola);
	UNUSED_ARG(frm);
	UNUSED_ARG(prev_lost);

	return 0;
}


int  ms_wsola_generate( ms_wsola *wsola, 
							int16_t frm[])
{
	ms_zero_samples(frm, wsola->samples_per_frame);
	return 0;
}


int  ms_wsola_discard( ms_wsola *wsola, 
						   int16_t buf1[],
						   unsigned buf1_cnt, 
						   int16_t buf2[],
						   unsigned buf2_cnt,
						   unsigned *del_cnt)
{
	CHECK_(buf1_cnt + buf2_cnt >= wsola->samples_per_frame);

	UNUSED_ARG(buf1);
	UNUSED_ARG(buf1_cnt);
	UNUSED_ARG(buf2);
	UNUSED_ARG(buf2_cnt);

	*del_cnt = wsola->samples_per_frame;

	return 0;
}

#endif	/* #if MS_WSOLA_IMP.. */


