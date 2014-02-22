#include "conf_itc.h"

typedef struct ConfSourceState{
	ConfSlotQueue *conf_slot_q;
}ConfSourceState;

void conf_slot_queue_init(ConfSlotQueue *q)
{
	q->fmt = MS_YUV420P;
	q->fps = 15.0;
	q->nchannels = 1;
	q->rate = 8000;
	q->vsize.width = MS_VIDEO_SIZE_CIF_W;
	q->vsize.height = MS_VIDEO_SIZE_CIF_H;
	ms_mutex_init(&q->lock,NULL);
	ms_queue_init(&q->q);
}

void conf_slot_queue_destory(ConfSlotQueue *q)
{
	ms_queue_flush(&q->q);
	ms_queue_flush(&q->q);
	ms_mutex_destroy(&q->lock);
}

void conf_slot_queue_flush(ConfSlotQueue *q)
{
	ms_mutex_lock(&q->lock);
	ms_queue_flush(&q->q);
	ms_mutex_unlock(&q->lock);
}


static void conf_tic_source_init(MSFilter *f){
	ConfSourceState *s=ms_new(ConfSourceState,1);;
	s->conf_slot_q = NULL;
	f->data=s;
}

static void conf_tic_source_uninit(MSFilter *f){
	ConfSourceState *s=(ConfSourceState *)f->data;
	ms_free(s);
}

static void conf_tic_source_process(MSFilter *f){
	ConfSourceState *s=(ConfSourceState *)f->data;
	mblk_t *m;

	if(s->conf_slot_q==NULL) return;

	ms_mutex_lock(&s->conf_slot_q->lock);
	while((m=ms_queue_get(&s->conf_slot_q->q))!=NULL){
		ms_mutex_unlock(&s->conf_slot_q->lock);
		ms_queue_put(f->outputs[0],m);
		ms_mutex_lock(&s->conf_slot_q->lock);
	}
	ms_mutex_unlock(&s->conf_slot_q->lock);
}

static int conf_tic_source_connect(MSFilter *f, void *data){
	ConfSourceState *s=(ConfSourceState *)f->data;
	s->conf_slot_q = data;
	return 0;
}


static int get_vsize(MSFilter *f, void *data){
	ConfSourceState *obj=(ConfSourceState*)f->data;

	if(obj->conf_slot_q!=NULL)
		*(MSVideoSize*)data=obj->conf_slot_q->vsize;
	return 0;
}

static int get_fps(MSFilter *f, void *data){
	ConfSourceState *obj=(ConfSourceState*)f->data;

	if(obj->conf_slot_q!=NULL)
		*(float*)data=obj->conf_slot_q->fps;
	return 0;
}

static int get_rate(MSFilter *f, void *arg){
	ConfSourceState *d=(ConfSourceState*)f->data;
	if(d->conf_slot_q!=NULL)
		*((int*)arg)=d->conf_slot_q->rate;
	return 0;
}

static int get_nchannels(MSFilter *f, void *arg){
	ConfSourceState *d=(ConfSourceState*)f->data;
	if(d->conf_slot_q!=NULL)
		*((int*)arg) = d->conf_slot_q->nchannels;
	return 0;
}


static MSFilterMethod source_methods[]={
	{	MS_CONF_ITC_SOURCE_CONNECT , conf_tic_source_connect },
	{	MS_FILTER_GET_VIDEO_SIZE	, get_vsize	},
	{	MS_FILTER_GET_FPS			, get_fps	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_GET_NCHANNELS		, get_nchannels	},
	{ 0, NULL }
};
#ifdef _MSC_VER

MSFilterDesc ms_conf_tic_source_desc={
	MS_CONF_ITC_SOURCE_ID,
	"MSConfItcSource",
	N_("Inter ticker communication filter."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	conf_tic_source_init,
	NULL,
	conf_tic_source_process,
	NULL,
	conf_tic_source_uninit,
	source_methods
};

#else

MSFilterDesc ms_conf_tic_source_desc={
	.id=MS_CONF_ITC_SOURCE_ID,
	.name="MSConfItcSource",
	.text=N_("Inter ticker communication filter."),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=conf_tic_source_init,
	.process=conf_tic_source_process,
	.uninit=conf_tic_source_uninit,
	.methods=source_methods
};

#endif


typedef struct ConfSinkState{
	ConfSlotQueue *conf_slot_q;
	bool_t muted;
}ConfSinkState;

static void conf_tic_sink_init(MSFilter *f){
	ConfSinkState *s=ms_new(ConfSinkState,1);;
	s->conf_slot_q = NULL;
	s->muted = FALSE;
	f->data=s;
}

static void conf_tic_sink_uninit(MSFilter *f){
	ConfSinkState *s=(ConfSinkState *)f->data;
	ms_free(s);
}

static void conf_tic_sink_process(MSFilter *f){
	ConfSinkState *s=(ConfSinkState *)f->data;
	mblk_t *im;

	if(s->conf_slot_q==NULL || s->muted) {
		ms_queue_flush (f->inputs[0]);
		return;
	}

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		ms_mutex_lock(&s->conf_slot_q->lock);
		ms_queue_put(&s->conf_slot_q->q,copymsg(im));
		freemsg(im);
		ms_mutex_unlock(&s->conf_slot_q->lock);
	}
}

static int conf_tic_sink_connect(MSFilter *f, void *data){
	ConfSinkState *s=(ConfSinkState *)f->data;
	s->conf_slot_q = data;
	return 0;
}


static int set_vsize(MSFilter *f, void *data){
	ConfSinkState *obj=(ConfSinkState*)f->data;
	obj->conf_slot_q->vsize=*(MSVideoSize*)data;
	return 0;
}

static int set_fps(MSFilter *f, void *data){
	ConfSinkState *obj=(ConfSinkState*)f->data;
	obj->conf_slot_q->fps=*(float*)data;
	return 0;
}

static int set_rate(MSFilter *f, void *arg){
	ConfSinkState *d=(ConfSinkState*)f->data;
	d->conf_slot_q->rate=*((int*)arg);
	return 0;
}


static int set_nchannels(MSFilter *f, void *arg){
	ConfSinkState *d=(ConfSinkState*)f->data;
	d->conf_slot_q->nchannels=*((int*)arg);
	return 0;
}

static int sink_mute(MSFilter *f, void *arg){
	ConfSinkState *d=(ConfSinkState*)f->data;
	if(d->conf_slot_q!=NULL){
		d->muted=TRUE;
		return 0;
	}
	return -1;
}

static int sink_unmute(MSFilter *f, void *arg){
	ConfSinkState *d=(ConfSinkState*)f->data;
	if(d->conf_slot_q!=NULL){
		d->muted=FALSE;
		return 0;
	}
	return -1;
}


static MSFilterMethod sink_methods[]={
	{	MS_CONF_ITC_SINK_MUTE	,	sink_mute	},
	{	MS_CONF_ITC_SINK_UNMUTE	,	sink_unmute	},
	{	MS_CONF_ITC_SINK_CONNECT  , conf_tic_sink_connect },
	{	MS_FILTER_SET_VIDEO_SIZE	, set_vsize	},
	{	MS_FILTER_SET_SAMPLE_RATE	, set_rate	},
	{	MS_FILTER_SET_FPS			, set_fps	},
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{ 0,NULL}
};


#ifdef _MSC_VER

MSFilterDesc ms_conf_tic_sink_desc={
	MS_CONF_ITC_SINK_ID,
	"MSConfItcSink",
	N_("Inter ticker communication filter."),
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	conf_tic_sink_init,
	NULL,
	conf_tic_sink_process,
	NULL,
	conf_tic_sink_uninit,
	sink_methods
};

#else

MSFilterDesc ms_conf_tic_sink_desc={
	.id=MS_CONF_ITC_SINK_ID,
	.name="MSConfItcSink",
	.text=N_("Inter ticker communication filter."),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.preprocess=conf_tic_sink_preprocess,
	.process=conf_tic_sink_process,
	.methods=sink_methods
};

#endif


void conf_itc_init(void){
	ms_filter_register(&ms_conf_tic_source_desc);
	ms_filter_register(&ms_conf_tic_sink_desc);
}
