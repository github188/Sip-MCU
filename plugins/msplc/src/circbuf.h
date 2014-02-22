/* $Id: circbuf.h 2394 2008-12-23 17:27:53Z bennylp $ */
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

#ifndef __MS_CIRC_BUF_H__
#define __MS_CIRC_BUF_H__

#include <assert.h>

#ifdef __cplusplus
extern "C"{
#endif

	/**
	* @defgroup PJMED_CIRCBUF Circular Buffer
	* @ingroup MS_FRAME_OP
	* @brief Circular buffer manages read and write contiguous audio samples in a 
	* non-contiguous buffer as if the buffer were contiguous. This should give
	* better performance than keeping contiguous samples in a contiguous buffer,
	* since read/write operations will only update the pointers, instead of 
	* shifting audio samples.
	*
	* @{
	*
	* This section describes PJMEDIA's implementation of circular buffer.
	*/

	/* Algorithm checkings, for development purpose only */
#if 0
#   define MS_CIRC_BUF_CHECK(x) pj_assert(x)
#else
#   define MS_CIRC_BUF_CHECK(x)
#endif


	/**
	* This is a general purpose function set PCM samples to zero.
	* Since this function is needed by many parts of the library,
	* by putting this functionality in one place, it enables some.
	* clever people to optimize this function.
	*
	* @param samples	The 16bit PCM samples.
	* @param count		Number of samples.
	*/
	inline void  ms_zero_samples(int16_t *samples, unsigned count)
	{
#if 1
		memset(samples, 0, (count<<1));
#elif 0
		unsigned i;
		for (i=0; i<count; ++i) samples[i] = 0;
#else
		unsigned i;
		count >>= 1;
		for (i=0; i<count; ++i) ((int32_t*)samples)[i] = (int32_t)0;
#endif
	}


	/**
	* This is a general purpose function to copy samples from/to buffers with
	* equal size. Since this function is needed by many parts of the library,
	* by putting this functionality in one place, it enables some.
	* clever people to optimize this function.
	*/
	inline void  ms_copy_samples(int16_t *dst, const int16_t *src,
		unsigned count)
	{
#if 1
		memcpy(dst, src, (count<<1));
#elif 0
		unsigned i;
		for (i=0; i<count; ++i) dst[i] = src[i];
#else
		unsigned i;
		count >>= 1;
		for (i=0; i<count; ++i)
			((int32_t*)dst)[i] = ((int32_t*)src)[i];
#endif
	}


	/**
	* This is a general purpose function to copy samples from/to buffers with
	* equal size. Since this function is needed by many parts of the library,
	* by putting this functionality in one place, it enables some.
	* clever people to optimize this function.
	*/
	inline void  ms_move_samples(int16_t *dst, const int16_t *src,
		unsigned count)
	{
#if 1
		memmove(dst, src, (count<<1));
#elif 0
		unsigned i;
		for (i=0; i<count; ++i) dst[i] = src[i];
#else
		unsigned i;
		count >>= 1;
		for (i=0; i<count; ++i)
			((int32_t*)dst)[i] = ((int32_t*)src)[i];
#endif
	}

	/** 
	* Circular buffer structure
	*/
	typedef struct ms_circ_buf {
		int16_t	    *buf;	    /**< The buffer			*/
		unsigned	capacity;	    /**< Buffer capacity, in samples	*/

		int16_t	    *start;	    /**< Pointer to the first sample	*/
		unsigned	len;	    /**< Audio samples length, 
								in samples			*/
	} ms_circ_buf;


	/**
	* Create the circular buffer.
	*
	* @param pool		    Pool where the circular buffer will be allocated
	*			    from.
	* @param capacity	    Capacity of the buffer, in samples.
	* @param p_cb		    Pointer to receive the circular buffer instance.
	*
	* @return		    0 if the circular buffer has been
	*			    created successfully, otherwise the appropriate
	*			    error will be returned.
	*/
	int ms_circ_buf_create(unsigned capacity,ms_circ_buf **p_cb)
	{
		ms_circ_buf *cbuf;

		cbuf = (ms_circ_buf*) ms_new0(ms_circ_buf,1);

		cbuf->buf = (int16_t*) ms_new0(int16_t,capacity);
		cbuf->capacity = capacity;
		cbuf->start = cbuf->buf;
		cbuf->len = 0;

		*p_cb = cbuf;

		return 0;
	}


	/**
	* Reset the circular buffer.
	*
	* @param circbuf	    The circular buffer.
	*
	* @return		    0 when successful.
	*/
	int ms_circ_buf_reset(ms_circ_buf *circbuf)
	{
		circbuf->start = circbuf->buf;
		circbuf->len = 0;
		return 0;
	}


	/**
	* Get the circular buffer length, it is number of samples buffered in the 
	* circular buffer.
	*
	* @param circbuf	    The circular buffer.
	*
	* @return		    The buffer length.
	*/
	inline unsigned ms_circ_buf_get_len(ms_circ_buf *circbuf)
	{
		return circbuf->len;
	}


	/**
	* Set circular buffer length. This is useful when audio buffer is manually 
	* manipulated by the user, e.g: shrinked, expanded.
	*
	* @param circbuf	    The circular buffer.
	* @param len		    The new buffer length.
	*/
	inline void ms_circ_buf_set_len(ms_circ_buf *circbuf,
		unsigned len)
	{
		MS_CIRC_BUF_CHECK(len <= circbuf->capacity);
		circbuf->len = len;
	}


	/**
	* Advance the read pointer of circular buffer. This function will discard
	* the skipped samples while advancing the read pointer, thus reducing 
	* the buffer length.
	*
	* @param circbuf	    The circular buffer.
	* @param count		    Distance from current read pointer, can only be
	*			    possitive number, in samples.
	*
	* @return		    0 when successful, otherwise 
	*			    the appropriate error will be returned.
	*/
	int ms_circ_buf_adv_read_ptr(ms_circ_buf *circbuf, 
		unsigned count)
	{
		if (count >= circbuf->len)
			return ms_circ_buf_reset(circbuf);

		MS_CIRC_BUF_CHECK(count <= circbuf->len);

		circbuf->start += count;
		if (circbuf->start >= circbuf->buf + circbuf->capacity) 
			circbuf->start -= circbuf->capacity;
		circbuf->len -= count;

		return 0;
	}


	/**
	* Advance the write pointer of circular buffer. Since write pointer is always
	* pointing to a sample after the end of sample, so this function also means
	* increasing the buffer length.
	*
	* @param circbuf	    The circular buffer.
	* @param count		    Distance from current write pointer, can only be
	*			    possitive number, in samples.
	*
	* @return		    0 when successful, otherwise 
	*			    the appropriate error will be returned.
	*/
	int ms_circ_buf_adv_write_ptr(ms_circ_buf *circbuf,
		unsigned count)
	{
		if (count + circbuf->len > circbuf->capacity)
			return -1;

		circbuf->len += count;

		return 0;
	}


	/**
	* Get the real buffer addresses containing the audio samples.
	*
	* @param circbuf	    The circular buffer.
	* @param reg1		    Pointer to store the first buffer address.
	* @param reg1_len	    Pointer to store the length of the first buffer, 
	*			    in samples.
	* @param reg2		    Pointer to store the second buffer address.
	* @param reg2_len	    Pointer to store the length of the second buffer, 
	*			    in samples.
	*/
	inline void ms_circ_buf_get_read_regions(ms_circ_buf *circbuf, 
		int16_t **reg1, 
		unsigned *reg1_len, 
		int16_t **reg2, 
		unsigned *reg2_len)
	{
		*reg1 = circbuf->start;
		*reg1_len = circbuf->len;
		if (*reg1 + *reg1_len > circbuf->buf + circbuf->capacity) {
			*reg1_len = circbuf->buf + circbuf->capacity - circbuf->start;
			*reg2 = circbuf->buf;
			*reg2_len = circbuf->len - *reg1_len;
		} else {
			*reg2 = NULL;
			*reg2_len = 0;
		}

		MS_CIRC_BUF_CHECK(*reg1_len != 0 || (*reg1_len == 0 && 
			circbuf->len == 0));
		MS_CIRC_BUF_CHECK(*reg1_len + *reg2_len == circbuf->len);
	}


	/**
	* Get the real buffer addresses that is empty or writeable.
	*
	* @param circbuf	    The circular buffer.
	* @param reg1		    Pointer to store the first buffer address.
	* @param reg1_len	    Pointer to store the length of the first buffer, 
	*			    in samples.
	* @param reg2		    Pointer to store the second buffer address.
	* @param reg2_len	    Pointer to store the length of the second buffer, 
	*			    in samples.
	*/
	inline void ms_circ_buf_get_write_regions(ms_circ_buf *circbuf, 
		int16_t **reg1, 
		unsigned *reg1_len, 
		int16_t **reg2, 
		unsigned *reg2_len)
	{
		*reg1 = circbuf->start + circbuf->len;
		if (*reg1 >= circbuf->buf + circbuf->capacity)
			*reg1 -= circbuf->capacity;
		*reg1_len = circbuf->capacity - circbuf->len;
		if (*reg1 + *reg1_len > circbuf->buf + circbuf->capacity) {
			*reg1_len = circbuf->buf + circbuf->capacity - *reg1;
			*reg2 = circbuf->buf;
			*reg2_len = circbuf->start - circbuf->buf;
		} else {
			*reg2 = NULL;
			*reg2_len = 0;
		}

		MS_CIRC_BUF_CHECK(*reg1_len != 0 || (*reg1_len == 0 && 
			circbuf->len == 0));
		MS_CIRC_BUF_CHECK(*reg1_len + *reg2_len == circbuf->capacity - 
			circbuf->len);
	}


	/**
	* Read audio samples from the circular buffer.
	*
	* @param circbuf	    The circular buffer.
	* @param data		    Buffer to store the read audio samples.
	* @param count		    Number of samples being read.
	*
	* @return		    0 when successful, otherwise 
	*			    the appropriate error will be returned.
	*/
	int ms_circ_buf_read(ms_circ_buf *circbuf, 
		int16_t *data, 
		unsigned count)
	{
		int16_t *reg1, *reg2;
		unsigned reg1cnt, reg2cnt;

		/* Data in the buffer is less than requested */
		if (count > circbuf->len)
			return -1;

		ms_circ_buf_get_read_regions(circbuf, &reg1, &reg1cnt, 
			&reg2, &reg2cnt);
		if (reg1cnt >= count) {
			ms_copy_samples(data, reg1, count);
		} else {
			ms_copy_samples(data, reg1, reg1cnt);
			ms_copy_samples(data + reg1cnt, reg2, count - reg1cnt);
		}

		return ms_circ_buf_adv_read_ptr(circbuf, count);
	}


	/**
	* Write audio samples to the circular buffer.
	*
	* @param circbuf	    The circular buffer.
	* @param data		    Audio samples to be written.
	* @param count		    Number of samples being written.
	*
	* @return		    0 when successful, otherwise
	*			    the appropriate error will be returned.
	*/
	int ms_circ_buf_write(ms_circ_buf *circbuf, 
		int16_t *data, 
		unsigned count)
	{
		int16_t *reg1, *reg2;
		unsigned reg1cnt, reg2cnt;

		/* Data to write is larger than buffer can store */
		if (count > circbuf->capacity - circbuf->len)
			return -1;

		ms_circ_buf_get_write_regions(circbuf, &reg1, &reg1cnt, 
			&reg2, &reg2cnt);
		if (reg1cnt >= count) {
			ms_copy_samples(reg1, data, count);
		} else {
			ms_copy_samples(reg1, data, reg1cnt);
			ms_copy_samples(reg2, data + reg1cnt, count - reg1cnt);
		}

		return ms_circ_buf_adv_write_ptr(circbuf, count);
	}


	/**
	* Copy audio samples from the circular buffer without changing its state. 
	*
	* @param circbuf	    The circular buffer.
	* @param start_idx	    Starting sample index to be copied.
	* @param data		    Buffer to store the read audio samples.
	* @param count		    Number of samples being read.
	*
	* @return		    0 when successful, otherwise 
	*			    the appropriate error will be returned.
	*/
	int ms_circ_buf_copy(ms_circ_buf *circbuf, 
		unsigned start_idx,
		int16_t *data, 
		unsigned count)
	{
		int16_t *reg1, *reg2;
		unsigned reg1cnt, reg2cnt;

		/* Data in the buffer is less than requested */
		if (count + start_idx > circbuf->len)
			return -1;

		ms_circ_buf_get_read_regions(circbuf, &reg1, &reg1cnt, 
			&reg2, &reg2cnt);
		if (reg1cnt > start_idx) {
			unsigned tmp_len;
			tmp_len = reg1cnt - start_idx;
			if (tmp_len > count)
				tmp_len = count;
			ms_copy_samples(data, reg1 + start_idx, tmp_len);
			if (tmp_len < count)
				ms_copy_samples(data + tmp_len, reg2, count - tmp_len);
		} else {
			ms_copy_samples(data, reg2 + start_idx - reg1cnt, count);
		}

		return 0;
	}


	/**
	* Pack the buffer so the first sample will be in the beginning of the buffer.
	* This will also make the buffer contiguous.
	*
	* @param circbuf	    The circular buffer.
	*
	* @return		    0 when successful, otherwise 
	*			    the appropriate error will be returned.
	*/
	int ms_circ_buf_pack_buffer(ms_circ_buf *circbuf)
	{
		int16_t *reg1, *reg2;
		unsigned reg1cnt, reg2cnt;
		unsigned gap;

		ms_circ_buf_get_read_regions(circbuf, &reg1, &reg1cnt, 
			&reg2, &reg2cnt);

		/* Check if not contigue */
		if (reg2cnt != 0) {
			/* Check if no space left to roll the buffer 
			* (or should this function provide temporary buffer?)
			*/
			gap = circbuf->capacity - ms_circ_buf_get_len(circbuf);
			if (gap == 0)
				return -1;

			/* Roll buffer left using the gap until reg2cnt == 0 */
			do {
				if (gap > reg2cnt)
					gap = reg2cnt;
				ms_move_samples(reg1 - gap, reg1, reg1cnt);
				ms_copy_samples(reg1 + reg1cnt - gap, reg2, gap);
				if (gap < reg2cnt)
					ms_move_samples(reg2, reg2 + gap, reg2cnt - gap);
				reg1 -= gap;
				reg1cnt += gap;
				reg2cnt -= gap;
			} while (reg2cnt > 0);
		}

		/* Finally, Shift samples to the left edge */
		if (reg1 != circbuf->buf)
			ms_move_samples(circbuf->buf, reg1, 
			ms_circ_buf_get_len(circbuf));
		circbuf->start = circbuf->buf;

		return 0;
	}


	int ms_circ_buf_destory(ms_circ_buf *p_cb)
	{
		ms_free(p_cb->buf);
		ms_free(p_cb);
		return 0;
	}

#ifdef __cplusplus
}
#endif

#endif
