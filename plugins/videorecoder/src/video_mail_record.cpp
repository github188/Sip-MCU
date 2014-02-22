#include "video_mail_record.h"
#include "mediastreamer2/msfilter.h"

struct _VideoMailRecord{
	MSFilter *snd_read;
	MSFilter *snd_sink;
	MSFilter *video_source;
	MSFilter *sizeconv;/*摄像头尺寸转换*/
	MSFilter *pixconv;/*摄像头格式转换*/
	MSFilter *video_tee;
	MSFilter *video_output;
	MSFilter *video_sink;
	MSTicker *ticker;
	MSVideoSize vsize;
	float fps;
	int rate;
	int nchannels;
	char *display_name;
	unsigned long video_window_id;
	char filename[1024];
	VideoRecoder *os;
	int bit_rate;
};


VideoMailRecord *video_mail_record_new(){
	VideoMailRecord *vp = ms_new0(VideoMailRecord,1);
	vp->nchannels = 1;
	vp->rate = 8000;
	vp->fps = 15.0;
	vp->vsize.width = MS_VIDEO_SIZE_CIF_W;
	vp->vsize.height = MS_VIDEO_SIZE_CIF_H;
	return vp;
}

void video_mail_record_set_audio_sink(VideoMailRecord *vp,MSFilter *sink){
	vp->snd_sink = sink;
}

void video_mail_record_set_video_sink(VideoMailRecord *vp,MSFilter *sink){
	vp->video_sink = sink;
}


static void choose_display_name(VideoMailRecord *stream){
#ifdef WIN32
	stream->display_name=ms_strdup("MSDrawDibDisplay");
#elif defined(ANDROID)
	stream->display_name=ms_strdup("MSAndroidDisplay");
#elif defined (HAVE_X11_EXTENSIONS_XV_H)
	stream->display_name=ms_strdup("MSX11Video");
#else
	stream->display_name=ms_strdup("MSVideoOut");
#endif
}


void video_mail_record_set_rate(VideoMailRecord *vp ,int rate)
{
	vp->rate = rate;
}

void video_mail_record_set_nchannels(VideoMailRecord *vp,int nchannels)
{
	vp->nchannels = nchannels;
}

void video_mail_record_set_fps(VideoMailRecord *vp,float fps)
{
	vp->fps = fps;
}
void video_mail_record_set_bit_rate(VideoMailRecord *vp,int bitrate)
{
	vp->bit_rate = bitrate;
}

void video_mail_record_set_video_size(VideoMailRecord *vp, MSVideoSize size)
{
	vp->vsize = size;
}

void video_mail_record_start_recording(VideoMailRecord *vp,bool_t enabled)
{
	if(vp->os)
		video_recoder_starting(vp->os,enabled);

	ms_message("%s video recording!!!!",enabled?"Start":"Stop");
}

int get_bit_rate_by_samples_rate(int rate)
{
	if(rate == 8000)
		return 32;

	if(rate == 16000)
		return 64;

	if(rate == 32000)
		return 96;

	if(rate >32000)
		return 128;
}

int  video_mail_record_start(VideoMailRecord *vp, MSSndCard *sndcard,MSWebCam *webcam,unsigned long video_window_id,const char *filename){
	MSConnectionHelper h;
	MSPixFmt format=MS_YUV420P;

	{
		strcpy(vp->filename, filename);

		vp->os = video_recoder_new();
		video_recoder_init(vp->os);
		video_recoder_set_vsize(vp->os,vp->vsize);
		video_recoder_set_rate(vp->os,vp->rate);
		video_recoder_set_audio_bit_rate(vp->os,  get_bit_rate_by_samples_rate(vp->rate));
		video_recoder_set_video_bit_rate(vp->os, vp->bit_rate - get_bit_rate_by_samples_rate(vp->rate));
		video_recoder_set_nbchannels(vp->os,vp->nchannels);

		if(video_recoder_open_file(vp->os,vp->filename)<0){

			video_recoder_destory(vp->os);
			vp->os=NULL;
			return -1;
		}

		video_mail_record_set_audio_sink(vp,video_recoder_create_audio_filter(vp->os));
		video_mail_record_set_video_sink(vp,video_recoder_create_video_filter(vp->os));

		video_recoder_start(vp->os);
	}


	{
		if (sndcard!=NULL) vp->snd_read=ms_snd_card_create_reader(sndcard);

		if(vp->snd_read){
			ms_filter_call_method(vp->snd_read,MS_FILTER_SET_NCHANNELS,&vp->nchannels);
			ms_filter_call_method(vp->snd_read,MS_FILTER_SET_SAMPLE_RATE,&vp->rate);
		}
	}

	{
		if (webcam==NULL){
			webcam=ms_web_cam_manager_get_default_cam (
				ms_web_cam_manager_get());                                
		}

		vp->video_source = ms_web_cam_create_reader(webcam);

		vp->video_tee = ms_filter_new(MS_TEE_ID);


		ms_message("Setting sent vsize=%ix%i, fps=%f",vp->vsize.width,vp->vsize.height,vp->fps);
		/* configure the filters */
		if (ms_filter_get_id(vp->video_source)!=MS_STATIC_IMAGE_ID) {
			ms_filter_call_method(vp->video_source,MS_FILTER_SET_FPS,&vp->fps);
		}
		ms_filter_call_method(vp->video_source,MS_FILTER_SET_VIDEO_SIZE,&vp->vsize);

		ms_filter_call_method(vp->video_source,MS_FILTER_GET_PIX_FMT,&format);

		if (format==MS_MJPEG){
			vp->pixconv=ms_filter_new(MS_MJPEG_DEC_ID);
		}else{
			vp->pixconv = ms_filter_new(MS_PIX_CONV_ID);
			/*set it to the pixconv */
			ms_filter_call_method(vp->pixconv,MS_FILTER_SET_PIX_FMT,&format);
			ms_filter_call_method(vp->video_source,MS_FILTER_GET_VIDEO_SIZE,&vp->vsize);
			ms_filter_call_method(vp->pixconv,MS_FILTER_SET_VIDEO_SIZE,&vp->vsize);
		}
		vp->sizeconv=ms_filter_new(MS_SIZE_CONV_ID);
		ms_filter_call_method(vp->sizeconv,MS_FILTER_SET_VIDEO_SIZE,&vp->vsize);

		choose_display_name(vp);
		vp->video_output=ms_filter_new_from_name (vp->display_name);

		if (video_window_id!=0)
			ms_filter_call_method(vp->video_output, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&video_window_id);
	}

	if(vp->snd_read!=NULL && vp->snd_sink!=NULL){

		ms_filter_link(vp->snd_read,0,vp->snd_sink,0);
	}

	if(vp->video_source!=NULL && vp->video_output!=NULL)
	{
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h,vp->video_source,-1,0);
		ms_connection_helper_link(&h,vp->pixconv,0,0);
		ms_connection_helper_link(&h,vp->sizeconv,0,0);
		ms_connection_helper_link(&h,vp->video_tee,0,0);

		ms_filter_link(vp->video_tee,1,vp->video_output,0);

		ms_connection_helper_link(&h,vp->video_sink,0,-1);

	}

	vp->ticker = ms_ticker_new();
	ms_ticker_set_name(vp->ticker,"VideoMailRecord");

	if(vp->snd_read!=NULL && vp->snd_sink!=NULL)
		ms_ticker_attach(vp->ticker,vp->snd_read);

	if(vp->video_source!=NULL && vp->video_output!=NULL)
		ms_ticker_attach(vp->ticker,vp->video_source);


	return 0;
}


static void video_mail_record_free(VideoMailRecord *vp)
{
	if(vp->display_name!=NULL) ms_free(vp->display_name);

	if(vp->ticker!=NULL) ms_ticker_destroy(vp->ticker);
	if(vp->snd_read!=NULL) ms_filter_destroy(vp->snd_read);
	if(vp->snd_sink!=NULL) ms_filter_destroy(vp->snd_sink);
	if(vp->sizeconv!=NULL) ms_filter_destroy(vp->sizeconv);
	if(vp->pixconv!=NULL) ms_filter_destroy(vp->pixconv);
	if(vp->video_tee!=NULL) ms_filter_destroy(vp->video_tee);
	if(vp->video_sink!=NULL) ms_filter_destroy(vp->video_sink);
	if(vp->video_source!=NULL) ms_filter_destroy(vp->video_source);
	if(vp->video_output!=NULL) ms_filter_destroy(vp->video_output);

	ms_free(vp);
}

void video_mail_record_stop(VideoMailRecord *vp)
{
	MSConnectionHelper h;
	VideoRecoder *os= vp->os;

	if(vp->ticker)
	{
		if(vp->snd_read!=NULL && vp->snd_sink!=NULL){
			ms_ticker_detach(vp->ticker,vp->snd_read);
			ms_filter_unlink(vp->snd_read,0,vp->snd_sink,0);
		}

		if(vp->video_source!=NULL && vp->video_output!=NULL){
			ms_ticker_detach(vp->ticker,vp->video_source);

			ms_connection_helper_start(&h);
			ms_connection_helper_unlink(&h,vp->video_source,-1,0);
			ms_connection_helper_unlink(&h,vp->pixconv,0,0);
			ms_connection_helper_unlink(&h,vp->sizeconv,0,0);
			ms_connection_helper_unlink(&h,vp->video_tee,0,0);

			ms_filter_unlink(vp->video_tee,1,vp->video_output,0);

			ms_connection_helper_unlink(&h,vp->video_sink,0,-1);


		}
	}

	if(vp->os) {
		video_recoder_stop(os);
		video_recoder_destory(os);
	}
	video_mail_record_free(vp);
	
}