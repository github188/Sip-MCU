/* $Id: plc_common.c 2850 2009-08-01 09:20:59Z bennylp $ */
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
#include "plc.h"
#include "wsola.h"

static void* plc_wsola_create(unsigned c, unsigned f);
static void  plc_wsola_save(void*, int16_t*);
static void  plc_wsola_generate(void*, int16_t*);
static void  plc_wsola_destory(void*);

/**
 * This struct is used internally to represent a PLC backend.
 */
struct plc_alg
{
    void* (*plc_create)(unsigned c, unsigned f);
    void  (*plc_save)(void*, int16_t*);
    void  (*plc_generate)(void*, int16_t*);
	void  (*plc_destory)(void*);
};


static struct plc_alg plc_wsola =
{
    &plc_wsola_create,
    &plc_wsola_save,
    &plc_wsola_generate,
	&plc_wsola_destory
};


struct ms_plc
{
    void	    *obj;
    struct plc_alg  *op;
};


/*
 * Create PLC session. This function will select the PLC algorithm to
 * use based on the arguments.
 */
int ms_plc_create(unsigned clock_rate,
					unsigned samples_per_frame,
					unsigned options,
					ms_plc **p_plc)
{
    ms_plc *plc;

    plc = ms_new0(ms_plc, 1);

    plc->op = &plc_wsola;
    plc->obj = plc->op->plc_create(clock_rate, samples_per_frame);

    *p_plc = plc;

    return 0;
}


/*
 * Save a good frame to PLC.
 */
int ms_plc_save( ms_plc *plc,
				      int16_t *frame )
{
  //  ASSERT_RETURN(plc && frame, EINVAL);
 
    plc->op->plc_save(plc->obj, frame);
    return 0;
}


/*
 * Generate a replacement for lost frame.
 */
int ms_plc_generate( ms_plc *plc,
					  int16_t *frame )
{
//    ASSERT_RETURN(plc && frame, EINVAL);
    
    plc->op->plc_generate(plc->obj, frame);
    return 0;
}


int ms_plc_destory( ms_plc *plc)
{
	plc->op->plc_destory(plc->obj);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
/*
 * Packet loss concealment based on WSOLA
 */
struct wsola_plc
{
    ms_wsola   *wsola;
    bool_t	     prev_lost;
};


static void* plc_wsola_create(unsigned clock_rate, 
							unsigned samples_per_frame)
{
    struct wsola_plc *o;
    unsigned flag;
    int status;

    UNUSED_ARG(clock_rate);

    o = ms_new0(struct wsola_plc, 1);
    o->prev_lost = FALSE;

    flag = MS_WSOLA_NO_DISCARD;
    if (MS_WSOLA_PLC_NO_FADING)
	flag |= MS_WSOLA_NO_FADING;

    status = ms_wsola_create(clock_rate, samples_per_frame, 1,
				  flag, &o->wsola);
    if (status != 0)
	return NULL;

    return o;
}

static void plc_wsola_save(void *plc, int16_t *frame)
{
    struct wsola_plc *o = (struct wsola_plc*) plc;

    ms_wsola_save(o->wsola, frame, o->prev_lost);
    o->prev_lost = FALSE;
}

static void plc_wsola_generate(void *plc, int16_t *frame)
{
    struct wsola_plc *o = (struct wsola_plc*) plc;
    
    ms_wsola_generate(o->wsola, frame);
    o->prev_lost = TRUE;
}

static void  plc_wsola_destory(void*plc)
{
	 struct wsola_plc *o = (struct wsola_plc*) plc;

	 ms_wsola_destroy(o->wsola);
}
