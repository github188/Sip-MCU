#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/msticker.h>

#ifdef __cplusplus
extern "C"{
#endif

#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#include <libswscale/swscale.h>
#else
#include <ffmpeg/swscale.h>
#ifdef _MSC_VER /* missing on windows? */
#include <ffmpeg/avutil.h>
#endif
#endif

#ifdef __cplusplus
}
#endif

#define MIXER_MAX_INPUTS 9
#define MIXER_MAX_OUTPUTS 16
#define MIXER_INPUT_TIMEOUT 5000

#define AMD_MODE 1

#include "video_mixer.h"

extern Layout video_mixer_default_layout[];


typedef struct _InputInfo{
	uint64_t last_frame_time;
	struct SwsContext *sws_ctx;
	MSVideoSize vsize;
	bool_t active;
	bool_t muted;
	int counter;
} InputInfo;

static void input_info_reset_sws(InputInfo *info){
	if (info->sws_ctx!=NULL){
		sws_freeContext(info->sws_ctx);
		info->sws_ctx=NULL;
	}
}

static void input_info_update(InputInfo *info, uint64_t curtime, bool_t has_data, bool_t force_update){
	if (has_data){
		info->active=TRUE;
		info->last_frame_time=curtime;
	}else if (curtime-info->last_frame_time>MIXER_INPUT_TIMEOUT || force_update){	
		input_info_reset_sws(info);
		info->active=FALSE;
	}
}

static void input_info_process(InputInfo *info, mblk_t *im, YuvBuf *dstframe, Region *pos){
	YuvBuf inbuf,dest;
	MSVideoSize im_size;
	int x,y,w,h;
	//AMD float ratio;
	if (ms_yuv_buf_init_from_mblk(&inbuf,im)!=0) return;

	im_size.width = inbuf.w;
	im_size.height = inbuf.h;

	w=(float)dstframe->w*pos->w;
	x=((float)dstframe->w*pos->x)-((float)w/2);

	//AMDratio=(float)w/(float)inbuf.w;
	//AMDh=(float)inbuf.h*ratio;
	//AMD y=((float)dstframe->h*pos->y)-((float)h/2);
	h=(float)dstframe->h*pos->w;
	y=((float)dstframe->h*pos->y)-((float)h/2);

	dest.w=w;
	dest.h=h;
	dest.planes[0]=dstframe->planes[0]+(y*dstframe->strides[0])+x;
	dest.planes[1]=dstframe->planes[1]+((y/2)*dstframe->strides[1])+(x/2);
	dest.planes[2]=dstframe->planes[2]+((y/2)*dstframe->strides[2])+(x/2);


	if (info->sws_ctx==NULL){
		info->sws_ctx=sws_getContext(inbuf.w,inbuf.h,PIX_FMT_YUV420P,
			dest.w,dest.h,PIX_FMT_YUV420P,SWS_FAST_BILINEAR,
			NULL, NULL, NULL);

		info->vsize.width = inbuf.w;
		info->vsize.height = inbuf.h;
	}

	if(!ms_video_size_equal(info->vsize,im_size)) //输入视频尺寸 != 分屏器预设，则需改变尺寸
	{
		ms_message("VideMixer: input video size changed, port [%d], %dx%d -> %dx%d",im_size.width,im_size.height,info->counter,info->vsize.width,info->vsize.height);

		if (info->sws_ctx!=NULL){

			sws_freeContext(info->sws_ctx);

			info->sws_ctx=sws_getContext(inbuf.w,inbuf.h,PIX_FMT_YUV420P,
				dest.w,dest.h,PIX_FMT_YUV420P,SWS_FAST_BILINEAR,
				NULL, NULL, NULL);

			info->vsize.width = inbuf.w;
			info->vsize.height = inbuf.h;
		}
	}

	if (sws_scale(info->sws_ctx,inbuf.planes,inbuf.strides, 0,
			inbuf.h, dest.planes, dstframe->strides)!=0){
				//		ms_error("Error in sws_scale().");
	}
}

typedef struct _VideoMixerData{
	Layout *layout_table;
	Region *regions;
	InputInfo inputinfo[MIXER_MAX_INPUTS];
	int counter;
	int nregions;
	MSVideoSize vsize;
	mblk_t *frame_msg;
	YuvBuf frame;
#ifdef AMD_MODE
	mblk_t *frame_msg_amd_mode;
	YuvBuf frame_amd_mode;
#endif
	int pin_controller;
	bool_t force_update;
	ms_mutex_t lock;
	mblk_t *output_msg[MIXER_MAX_OUTPUTS];
	YuvBuf output_buf[MIXER_MAX_OUTPUTS];
}VideoMixerData;

static void mixer_init(MSFilter *f){
	VideoMixerData *s=(VideoMixerData*)ms_new0(VideoMixerData,1);
	s->vsize.width=MS_VIDEO_SIZE_CIF_W;
	s->vsize.height=MS_VIDEO_SIZE_CIF_H;
	s->layout_table=video_mixer_default_layout;
	ms_mutex_init(&s->lock,NULL);
	s->force_update = FALSE;
	s->counter = 0;
	f->data=s;
}

static void mixer_reset_all(MSFilter *f, VideoMixerData *d){
	int i;
	for (i=0;i<f->desc->ninputs;++i){
		input_info_reset_sws(&d->inputinfo[i]);
		/* ms_message("nb of incoming image on pin %i: %i", i, d->inputinfo[i].counter); */
		d->inputinfo[i].counter=0;
	}
	d->pin_controller=0;
	ms_message("nb of outgoing images %i", d->counter);
	d->counter=0;
	/* put frame black */
	if (d->frame_msg!=NULL){
		int ysize=d->frame.strides[0]*d->frame.h;
		memset(d->frame.planes[0],16,ysize);
		memset(d->frame.planes[1],128,ysize/4);
		memset(d->frame.planes[2],128,ysize/4);
	}
#ifdef AMD_MODE
	/* put frame black */
	if (d->frame_msg_amd_mode!=NULL){
		int ysize=d->frame_amd_mode.strides[0]*d->frame_amd_mode.h;
		memset(d->frame_amd_mode.planes[0],16,ysize);
		memset(d->frame_amd_mode.planes[1],128,ysize/4);
		memset(d->frame_amd_mode.planes[2],128,ysize/4);
	}
#endif
}

static void mixer_uninit(MSFilter *f){
	VideoMixerData *d=(VideoMixerData*)f->data;
	mixer_reset_all(f,d);
	ms_mutex_destroy(&d->lock);
	ms_free(f->data);
}

static int check_inputs(MSFilter *f, VideoMixerData *d){
	int i;
	int active=0;
	uint64_t curtime=f->ticker->time;
	for (i=0;i<f->desc->ninputs;++i){
		if (f->inputs[i]!=NULL){
			input_info_update(&d->inputinfo[i],
				curtime,
				!ms_queue_empty(f->inputs[i]),
				d->force_update
				);
			if(d->inputinfo[i].muted){
				ms_queue_flush(f->inputs[i]);
				d->inputinfo[i].active = FALSE;
			}

			if (d->inputinfo[i].active)
				++active;
		}
	}
	d->force_update = FALSE;
	return active;
}

static void mixer_update_layout(VideoMixerData *d, int nregions){
	Layout *it;
	for(it=d->layout_table;it->regions!=NULL;++it){
		if (it->nregions==nregions){
			/*found a layout for this number of regions*/
			d->nregions=nregions;
			d->regions=it->regions;
			return;	
		}
	}
	ms_error("No layout defined for %i regions",nregions);
}

static void mixer_preprocess(MSFilter *f){
	VideoMixerData *d=(VideoMixerData*)f->data;
	int i;
	d->frame_msg=ms_yuv_buf_alloc(&d->frame,d->vsize.width,d->vsize.height);
#ifdef AMD_MODE
	d->frame_msg_amd_mode=ms_yuv_buf_alloc(&d->frame_amd_mode,d->vsize.width,d->vsize.height);	
#endif
	for(i=0;i<f->desc->noutputs;i++)
	{
		d->output_msg[i]=ms_yuv_buf_alloc(&d->output_buf[i],d->vsize.width,d->vsize.height);
	}
}

static void mixer_process(MSFilter *f){
#ifdef AMD_MODE
	int update_pin0=-1;
	int update_pin1=-1;
#endif
	int i,found;
	mblk_t *im;
	int nregions;
	VideoMixerData *d=(VideoMixerData*)f->data;

	ms_mutex_lock(&d->lock);
	nregions=check_inputs(f,d);

	if (d->nregions!=nregions){
		mixer_update_layout(d,nregions);
		mixer_reset_all(f,d);
		d->pin_controller=0;
	}

#ifdef AMD_MODE
	if (d->nregions==2)
	{
		/* special mode to enable video conference with 1 guy:
		DO NOT send him his own picture. */
		/* First INPUT has local SOURCE connected */
		if (f->inputs[0]!=NULL && (im=ms_queue_peek_last(f->inputs[0]))!=NULL ){
			Region *pos=&video_mixer_default_layout->regions[0];
			input_info_process(&d->inputinfo[0],im,&d->frame,pos);
			ms_queue_flush(f->inputs[0]);
			// ms_message("WINDS: new image on pin0");
			update_pin0=1;
		}

		/* second INPUT is some RTP connected */
		for (i=1,found=0;i<f->desc->ninputs;++i){
			if (f->inputs[i]!=NULL && (im=ms_queue_peek_last(f->inputs[i]))!=NULL ){
				Region *pos=&video_mixer_default_layout->regions[0];
				input_info_process(&d->inputinfo[i],im,&d->frame_amd_mode,pos);
				ms_queue_flush(f->inputs[i]);
				update_pin1=1;
				break; /* only one in this case */
			}
		}
	}
	else
	{
		int max=0;
		update_pin0=1;
		update_pin1=1;

		/* select the fastest pin */
		d->pin_controller=0;
		for (i=0,found=0;i<f->desc->ninputs;++i){
			if (f->inputs[i]!=NULL && (im=ms_queue_peek_last(f->inputs[i]))!=NULL ){
				Region *pos=&d->regions[found];
				input_info_process(&d->inputinfo[i],im,&d->frame,pos); //依次布局
				ms_queue_flush(f->inputs[i]);
				d->inputinfo[i].counter++;
			}
			if (d->inputinfo[i].counter>max){
				max=d->inputinfo[i].counter;
				d->pin_controller=i;
			}
			if (f->inputs[i]!=NULL && d->inputinfo[i].active)
				++found;
		}

		if (max>20) /* reset */
		{
			for (i=0,found=0;i<f->desc->ninputs;++i){
				/* ms_message("nb of incoming image on pin %i: %i", i, d->inputinfo[i].counter); */
				if (i==d->pin_controller)
					d->inputinfo[i].counter=3; /* keep this one advanced to avoid changing too often */
				else
					d->inputinfo[i].counter=0;
			}
			/* ms_message("nb of outgoing images %i", d->counter); */
			d->counter=0;
		}
	}
#else
	for (i=0,found=0;i<f->desc->ninputs;++i){
		if (f->inputs[i]!=NULL && (im=ms_queue_peek_last(f->inputs[i]))!=NULL ){
			Region *pos=&d->regions[found];
			input_info_process(&d->inputinfo[i],im,&d->frame,pos);
			ms_queue_flush(f->inputs[i]);
		}
		if (f->inputs[i]!=NULL && d->inputinfo[i].active)
			++found;
	}
#endif
#ifdef AMD_MODE
	if (d->nregions==2)
	{
		if (update_pin1==1) /* new frame_msg_amd_mode */
		{
			if (f->outputs[0]!=NULL){
				//ms_queue_put(f->outputs[0],copyb(d->frame_msg_amd_mode));
				/*拷贝mixer帧到输出缓冲*/
				memcpy(d->output_msg[0]->b_rptr,
					d->frame_msg_amd_mode->b_rptr,
					d->frame_msg_amd_mode->b_wptr-d->frame_msg_amd_mode->b_rptr);

				ms_queue_put(f->outputs[0],dupb(d->output_msg[0]));
			}
		}
		if (update_pin0==1) /* new frame_msg */
		{
			for(i=1;i<f->desc->noutputs;++i){
				if (f->outputs[i]!=NULL){
					/*拷贝mixer帧到输出缓冲*/
					memcpy(d->output_msg[i]->b_rptr,
						d->frame_msg->b_rptr,
						d->frame_msg->b_wptr-d->frame_msg->b_rptr);
					ms_queue_put(f->outputs[i],dupb(d->output_msg[i]));
				}
			}
		}
	}
	else
	{
		if (d->inputinfo[d->pin_controller].last_frame_time==f->ticker->time) /* the pin controller was updated */
		{
			d->counter++;
			for(i=0;i<f->desc->noutputs;++i){
				if (f->outputs[i]!=NULL){
					memcpy(d->output_msg[i]->b_rptr,
						d->frame_msg->b_rptr,
						d->frame_msg->b_wptr-d->frame_msg->b_rptr);
					ms_queue_put(f->outputs[i],dupb(d->output_msg[i]));
				}
			}
		}
	}
#else
	for(i=0;i<f->desc->noutputs;++i){
		if (f->outputs[i]!=NULL){
			ms_queue_put(f->outputs[i],copyb(d->frame_msg));
		}
	}
#endif

	ms_mutex_unlock(&d->lock);
}

static void mixer_postprocess(MSFilter *f){
	VideoMixerData *d=(VideoMixerData*)f->data;
	int i;
	mixer_reset_all(f,d);
	freemsg(d->frame_msg);
	d->frame_msg=NULL;
#ifdef AMD_MODE
	freemsg(d->frame_msg_amd_mode);
	d->frame_msg_amd_mode=NULL;
#endif

	for(i=0;i<f->desc->noutputs;i++)
	{
		freemsg(d->output_msg[i]);
		d->output_msg[i]=NULL;
	}

}

static int mixer_set_vsize(MSFilter *f, void *data){
	VideoMixerData *d=(VideoMixerData*)f->data;
	MSVideoSize *sz=(MSVideoSize*)data;
	d->vsize=*sz;

	if (d->counter > 0){
		/*apply new settings dynamically*/
		ms_filter_lock(f);
		mixer_postprocess(f);
		mixer_preprocess(f);
		ms_filter_unlock(f);
	}

	return 0;
}

static int mixer_req_vfu(MSFilter *f, void *arg){
	VideoMixerData *d=(VideoMixerData*)f->data;
	ms_mutex_lock(&d->lock);
	d->force_update=TRUE;
	ms_mutex_unlock(&d->lock);
	return 0;
}

static int mixer_mute_input(MSFilter *f, void *arg){
	VideoMixerData *d=(VideoMixerData*)f->data;
	int index=*((int*)arg);;
	ms_mutex_lock(&d->lock);
	if(index < MIXER_MAX_INPUTS && index >= 0)
		d->inputinfo[index].muted = TRUE;
	ms_mutex_unlock(&d->lock);
	return 0;
}

static int mixer_unmute_input(MSFilter *f, void *arg){
	VideoMixerData *d=(VideoMixerData*)f->data;
	int index=*((int*)arg);;
	ms_mutex_lock(&d->lock);
	if(index < MIXER_MAX_INPUTS && index >= 0)
		d->inputinfo[index].muted = FALSE;
	ms_mutex_unlock(&d->lock);
	return 0;
}

static int get_nchannels(MSFilter *f, void *arg){
	VideoMixerData *d=(VideoMixerData*)f->data;
	int i;
	int counter=0;
	uint64_t curtime=f->ticker->time;
	for (i=0;i<f->desc->ninputs;++i){
		if (f->inputs[i]!=NULL){
			counter++;
		}
	}
	*((int*)arg) =counter;
	return 0;
}


static MSFilterMethod methods[]={
	{MS_FILTER_SET_VIDEO_SIZE, mixer_set_vsize},
	{MS_FILTER_REQ_VFU,	mixer_req_vfu},
	{MS_VIDEO_CONF_MUTE_INPUT, mixer_mute_input},
	{MS_VIDEO_CONF_UNMUTE_INPUT, mixer_unmute_input},
	{MS_FILTER_GET_NCHANNELS, get_nchannels	},
	{0,NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_video_mixer_desc={
	MS_FILTER_PLUGIN_ID,
	"MSVideoMixer",
	"A video mixing filter",
	MS_FILTER_OTHER,
	NULL,
	MIXER_MAX_INPUTS,
	MIXER_MAX_OUTPUTS,
	mixer_init,
	mixer_preprocess,
	mixer_process,
	mixer_postprocess,
	mixer_uninit,
	methods
};

#else

MSFilterDesc ms_video_mixer_desc={
	MS_FILTER_PLUGIN_ID,
	"MSVideoMixer",
	"A video mixing filter",
	MS_FILTER_OTHER,
	NULL,
	MIXER_MAX_INPUTS,
	MIXER_MAX_OUTPUTS,
	mixer_init,
	mixer_preprocess,
	mixer_process,
	mixer_postprocess,
	mixer_uninit,
	methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_video_mixer_desc)