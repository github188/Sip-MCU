#include "file_stream.h"


static void audio_player_init(MSFilter *f){
	StreamData *d=(StreamData*)ms_new(StreamData,1);
	d->nchannels = 1;
	d->rate = 8000;
	d->is=NULL;
	f->data=d;
}

static void audio_player_uninit(MSFilter *f){
	StreamData *d=(StreamData*)f->data;
	ms_free(d);
}

static void audio_player_preprocess(MSFilter *f){
	StreamData *d=(StreamData*)f->data;
	if(d->is){
		ConfSlotQueue *confq= video_player_get_audio_outq(d->is);
		if(confq) confq->run = TRUE;
	}
}

mblk_t *mono2stereo(mblk_t *im)
{
	int msgsize=msgdsize(im)*2;
	mblk_t *om=allocb(msgsize,0);

	for (;im->b_rptr<im->b_wptr;im->b_rptr+=2,om->b_wptr+=4){
		((int16_t*)om->b_wptr)[0]=*(int16_t*)im->b_rptr;
		((int16_t*)om->b_wptr)[1]=*(int16_t*)im->b_rptr;
	}

	freemsg(im);
	return om;
}

mblk_t *stereo2mono(mblk_t *im)
{
	int msgsize=msgdsize(im)/2;
	mblk_t *om=allocb(msgsize,0);
	for (;im->b_rptr<im->b_wptr;im->b_rptr+=4,om->b_wptr+=2){
		*(int16_t*)om->b_wptr=*(int16_t*)im->b_rptr;
	}
	freemsg(im);
	return om;
}



static void audio_player_process(MSFilter *f){
	StreamData *d=(StreamData*)f->data;
	VideoState *is = d->is;
	mblk_t *om=NULL;
	ConfSlotQueue *confq= video_player_get_audio_outq(d->is);

	ms_mutex_lock(&confq->lock);
	om = ms_queue_get(&confq->q); //每次取出10ms音频采样
	ms_mutex_unlock(&confq->lock);
	
	if(om!=NULL)
	{
		//if(is->audio_outq.nchannels == d->nchannels)
			ms_queue_put(f->outputs[0],om);
		/*else if(is->audio_outq.nchannels == 2) //声道转换 立体声 --> 单声道 
			ms_queue_put(f->outputs[0],stereo2mono(om));
		else if(d->nchannels==2) //声道转换  单声道 ----> 立体声
			ms_queue_put(f->outputs[0],mono2stereo(om));*/
	}
}

static void audio_player_postprocess(MSFilter *f){
	StreamData *d=(StreamData*)f->data;
	ConfSlotQueue *confq= video_player_get_audio_outq(d->is);
	if(d->is)
		confq->run = FALSE;
}

static int audio_player_set_rate(MSFilter *f, void *arg){
	StreamData *d=(StreamData*)f->data;
	ConfSlotQueue *confq= video_player_get_audio_outq(d->is);
	d->rate=*((int*)arg);
	if(d->is) confq->rate = d->rate;
	return 0;
}

static int audio_player_get_rate(MSFilter *f, void *arg){
	StreamData *d=(StreamData*)f->data;
	*((int*)arg) = d->rate;
	return 0;
}

static int audio_player_set_nchannels(MSFilter *f, void* data){
	StreamData *d=(StreamData*)f->data;
	ConfSlotQueue *confq= video_player_get_audio_outq(d->is);
	d->nchannels=*(int*)data;
	if(d->is) confq->nchannels = d->nchannels;
	return 0;
}

static int audio_player_get_nchannels(MSFilter *f, void* data){
	StreamData *d=(StreamData*)f->data;
	*(int*)data=d->nchannels;
	return 0;
}


static MSFilterMethod audio_player_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE,	audio_player_set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	audio_player_get_rate	},
	{	MS_FILTER_SET_NCHANNELS, audio_player_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS, audio_player_get_nchannels },
	{	0,0 }
};

MSFilterDesc filstream_audio_player_desc={
	MS_FILTER_PLUGIN_ID,
	"AudioStreamPlayer",
	N_("A audio player.."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	audio_player_init,
	audio_player_preprocess,
	audio_player_process,
	audio_player_postprocess,
	audio_player_uninit,
	audio_player_methods
};


static void video_player_init(MSFilter *f){
	StreamData *d=(StreamData*)ms_new(StreamData,1);
	d->vsize.width=MS_VIDEO_SIZE_CIF_W;
	d->vsize.height=MS_VIDEO_SIZE_CIF_H;
	d->fps=1;
	d->starttime=0;
	d->index =0;
	d->is=NULL;
	f->data=d;
}

static void video_player_uninit(MSFilter *f){
	StreamData *d=(StreamData*)f->data;
	ms_free(d);
}

static void video_player_preprocess(MSFilter *f){
	StreamData *d=(StreamData*)f->data;
	d->starttime=f->ticker->time;
	ConfSlotQueue *confq= video_player_get_video_outq(d->is);
	if(d->is)
		confq->run = TRUE;
}

static void video_player_process(MSFilter *f){
	StreamData *d=(StreamData*)f->data;
	VideoState *is = d->is;
	mblk_t *om=NULL;
	ConfSlotQueue *confq= video_player_get_video_outq(d->is);

	ms_filter_lock(f);
	float elapsed=(float)(f->ticker->time-d->starttime);

	
	if ((elapsed*d->fps/1000.0)>d->index){

		ms_mutex_lock(&confq->lock);
		om = ms_queue_get(&confq->q);
		ms_mutex_unlock(&confq->lock);

		if(om!=NULL) ms_queue_put(f->outputs[0],om);

		d->index++;
	}
	ms_filter_unlock(f);

	
	
}

static void video_player_postprocess(MSFilter *f){
	StreamData *d=(StreamData*)f->data;
	ConfSlotQueue *confq= video_player_get_video_outq(d->is);
	if(d->is) confq->run = FALSE;
}

static int video_player_set_fps(MSFilter *f, void *arg){
	StreamData *d=(StreamData*)f->data;
	ConfSlotQueue *confq= video_player_get_video_outq(d->is);
	d->fps=*((float*)arg);
	if(d->is) confq->fps = d->fps;
	d->index =0;
	return 0;
}

static int video_player_get_fps(MSFilter *f, void *arg){
	StreamData *d=(StreamData*)f->data;
	*((float*)arg) = d->fps;
	return 0;
}

static int video_player_set_vsize(MSFilter *f, void* data){
	StreamData *d=(StreamData*)f->data;
	ConfSlotQueue *confq= video_player_get_video_outq(d->is);
	d->vsize=*(MSVideoSize*)data;
	if(d->is) confq->vsize = d->vsize;
	return 0;
}

static int video_player_get_vsize(MSFilter *f, void* data){
	StreamData *d=(StreamData*)f->data;
	*(MSVideoSize*)data=d->vsize;
	return 0;
}

static int video_player_get_pix_fmt(MSFilter *f, void *data){
	*(MSPixFmt*)data=MS_YUV420P;
	return 0;
}

static MSFilterMethod video_player_methods[]={
	{	MS_FILTER_SET_FPS,	video_player_set_fps	},
	{	MS_FILTER_GET_FPS,	video_player_get_fps	},
	{	MS_FILTER_SET_VIDEO_SIZE, video_player_set_vsize },
	{	MS_FILTER_GET_VIDEO_SIZE, video_player_get_vsize },
	{	MS_FILTER_GET_PIX_FMT, video_player_get_pix_fmt },
	{	0,0 }
};

MSFilterDesc filstream_video_player_desc={
	MS_FILTER_PLUGIN_ID,
	"VideoStreamPlayer",
	N_("A video player.."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	video_player_init,
	video_player_preprocess,
	video_player_process,
	video_player_postprocess,
	video_player_uninit,
	video_player_methods
};

