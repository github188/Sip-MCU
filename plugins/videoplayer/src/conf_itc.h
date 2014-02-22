/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#ifndef CON_ITC_H
#define CON_ITC_H

#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"


typedef struct _ConfSlotQueue{
	MSQueue q;
	ms_mutex_t lock;
	int nchannels;
	int rate;
	float fps;
	MSVideoSize vsize;
	MSPixFmt fmt;
	bool_t run;
}ConfSlotQueue;

#ifdef __cplusplus
extern "C"{
#endif

void conf_slot_queue_init(ConfSlotQueue *q);
void conf_slot_queue_flush(ConfSlotQueue *q);
void conf_slot_queue_destory(ConfSlotQueue *q);

#ifdef __cplusplus
}
#endif

#define MS_CONF_ITC_SINK_CONNECT MS_FILTER_METHOD(MS_CONF_ITC_SINK_ID,0,ConfSlotQueue)

/*mute/unmute some outputs of the MSConfSink */
#define MS_CONF_ITC_SINK_MUTE	MS_FILTER_METHOD(MS_CONF_ITC_SINK_ID,1,int)
#define MS_CONF_ITC_SINK_UNMUTE	MS_FILTER_METHOD(MS_CONF_ITC_SINK_ID,2,int)

#define MS_CONF_ITC_SOURCE_CONNECT MS_FILTER_METHOD(MS_CONF_ITC_SOURCE_ID,0,ConfSlotQueue)

#endif
