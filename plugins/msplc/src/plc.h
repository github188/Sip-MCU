/* $Id: plc.h 2394 2008-12-23 17:27:53Z bennylp $ */
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
#ifndef __MS_PLC_H__
#define __MS_PLC_H__


/**
 * @file plc.h
 * @brief Packet Lost Concealment (PLC) API.
 */
#include <mediastreamer2/mscommon.h>

/**
 * Opaque declaration for PLC.
 */
typedef struct ms_plc ms_plc;

#ifdef __cplusplus
extern "C"{
#endif


/**
 * Create PLC session. This function will select the PLC algorithm to
 * use based on the arguments.
 *
 * @param pool		    Pool to allocate memory for the PLC.
 * @param clock_rate	    Media sampling rate.
 * @param samples_per_frame Number of samples per frame.
 * @param options	    Must be zero for now.
 * @param p_plc		    Pointer to receive the PLC instance.
 *
 * @return		    SUCCESS on success.
 */
int ms_plc_create(unsigned clock_rate,
					 unsigned samples_per_frame,
					 unsigned options,
					 ms_plc **p_plc);


/**
 * Save a good frame to PLC.
 *
 * @param plc		    The PLC session.
 * @param frame		    The good frame to be stored to PLC. This frame
 *			    must have the same length as the configured
 *			    samples per frame.
 *
 * @return		    SUCCESS on success.
 */
int ms_plc_save( ms_plc *plc,
				       int16_t *frame );


/**
 * Generate a replacement for lost frame.
 *
 * @param plc		    The PLC session.
 * @param frame		    Buffer to receive the generated frame. This buffer
 *			    must be able to store the frame.
 *
 * @return		    SUCCESS on success.
 */
int ms_plc_generate( ms_plc *plc,
					   int16_t *frame );


int ms_plc_destory( ms_plc *plc);

#ifdef __cplusplus
}
#endif

#endif	/* __MS_PLC_H__ */

