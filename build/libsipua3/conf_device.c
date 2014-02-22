#include "conference.h"
#include "conf_itc.h"
#include "mediastreamer2/msitc.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/mstee.h"
#include "conf_private.h"

ConfSoundCard *local_sound_card_new()
{
	ConfSoundCard *sndcard=ms_new0(ConfSoundCard,1);
	sndcard->audio_port_index = -1;
	return sndcard;
}

int local_sound_card_start(ConfSoundCard *sndcard, MSSndCard *playcard, MSSndCard *captcard,int sample_rate)
{

	if(!playcard || !captcard) return -1;

	sndcard->capture_card=ms_snd_card_create_reader(captcard);

	if(sndcard->capture_card!=NULL)
		ms_filter_call_method(sndcard->capture_card,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);

	sndcard->playback_card=ms_snd_card_create_writer(playcard);

	if(sndcard->playback_card!=NULL)
		ms_filter_call_method(sndcard->playback_card,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);

	sndcard->itc_source =  ms_filter_new(MS_CONF_ITC_SOURCE_ID);
	sndcard->itc_sink = ms_filter_new(MS_CONF_ITC_SINK_ID);

	ms_filter_link(sndcard->capture_card,0,sndcard->itc_sink,0);
	ms_filter_link(sndcard->itc_source,0,sndcard->playback_card,0);

	sndcard->ticker = ms_ticker_new();
	ms_ticker_set_name(sndcard->ticker,"Local Sound Card MSTicker");

	ms_ticker_attach(sndcard->ticker,sndcard->capture_card);
	ms_ticker_attach(sndcard->ticker,sndcard->itc_source);

	return 0;
}

static void snd_card_free(ConfSoundCard *sndcard)
{
	if (sndcard->ticker!=NULL) ms_ticker_destroy(sndcard->ticker);
	if (sndcard->capture_card!=NULL) ms_filter_destroy(sndcard->capture_card);
	if (sndcard->itc_source!=NULL) ms_filter_destroy(sndcard->itc_source);
	if (sndcard->playback_card!=NULL) ms_filter_destroy(sndcard->playback_card);
	if (sndcard->itc_sink!=NULL) ms_filter_destroy(sndcard->itc_sink);

	ms_free(sndcard);
}

void local_sound_card_stop(ConfSoundCard *sndcard)
{
	if (sndcard->ticker){
		ms_ticker_detach(sndcard->ticker,sndcard->capture_card);
		ms_ticker_detach(sndcard->ticker,sndcard->itc_source);

		ms_filter_unlink(sndcard->capture_card,0,sndcard->itc_sink,0);
		ms_filter_unlink(sndcard->itc_source,0,sndcard->playback_card,0);
	}

	snd_card_free(sndcard);
}


int linphone_conference_add_local_sndcard(LinphoneConference *conf, ConfSoundCard *sndcard)
{
	ConfAudioPort *audio_port = find_free_audio_slot(conf->conf_stream);

	if(conf->local_sndcard!=NULL)
	{
		ms_message("local sound card already added!");
		return -1;
	}

	if(!conf || !sndcard || !audio_port) return -2;

	conf->local_sndcard = sndcard;

	ms_message("Add Local Sound Card: conf_stream [%d] <----> local_sndcard !!",audio_port->index);

	audio_port->sink_connected = TRUE;
	ms_filter_call_method(audio_port->audio_itc_source,MS_CONF_ITC_SOURCE_CONNECT,&audio_port->slotq_in);
	ms_filter_call_method(sndcard->itc_sink,MS_CONF_ITC_SINK_CONNECT,&audio_port->slotq_in);
	audio_port->source_connected = TRUE;
	ms_filter_call_method(sndcard->itc_source,MS_CONF_ITC_SOURCE_CONNECT,&audio_port->slotq_out);
	ms_filter_call_method(audio_port->audio_itc_sink,MS_CONF_ITC_SINK_CONNECT,&audio_port->slotq_out);
	sndcard->audio_port_index = audio_port->index;

}

int linphone_conference_remove_local_sndcard(LinphoneConference *conf)
{
	ConfSoundCard *sndcard=NULL;

	if(conf==NULL || conf->local_sndcard==NULL || conf->local_sndcard->audio_port_index==-1) return -1;

	sndcard = conf->local_sndcard;

	ConfAudioPort *audio_port = find_audio_slot_at_index(conf->conf_stream,conf->local_sndcard->audio_port_index);

	if(sndcard->ticker && audio_port)
	{

		ms_message("Remove Local Sound Card: conf_stream [%d] <----> local_sndcard !!",audio_port->index);

		
		ms_filter_call_method(audio_port->audio_itc_source,MS_CONF_ITC_SOURCE_CONNECT,NULL);
		ms_filter_call_method(sndcard->itc_sink,MS_CONF_ITC_SINK_CONNECT,NULL);
		audio_port->sink_connected = FALSE;

		ms_filter_call_method(sndcard->itc_source,MS_CONF_ITC_SOURCE_CONNECT,NULL);
		ms_filter_call_method(audio_port->audio_itc_sink,MS_CONF_ITC_SINK_CONNECT,NULL);
		audio_port->source_connected = FALSE;

		sndcard->audio_port_index = -1;
	}

	conf->local_sndcard = NULL;
}


ConfWebCam *local_webcam_new(){

	ConfWebCam *webcam=ms_new0(ConfWebCam,1);
	webcam->video_port_index = -1;
	return webcam;
}

int local_webcam_start(ConfWebCam *localcam,MSWebCam *device,MSVideoSize vsize,float fps){

	MSPixFmt format=MS_YUV420P;

	if(!device) return -1;

	if(fps<=0)
		fps=15;

	if (device==NULL){
		device=ms_web_cam_manager_get_default_cam (
			ms_web_cam_manager_get());                                
	}

	localcam->webcam = ms_web_cam_create_reader(device);

	ms_message("Setting sent vsize=%ix%i, fps=%f",vsize.width,vsize.height,fps);
	/* configure the filters */
	if (ms_filter_get_id(localcam->webcam)!=MS_STATIC_IMAGE_ID) {
		ms_filter_call_method(localcam->webcam,MS_FILTER_SET_FPS,&fps);
	}
	ms_filter_call_method(localcam->webcam,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	/* get the output format for webcam reader */
	ms_filter_call_method(localcam->webcam,MS_FILTER_GET_PIX_FMT,&format);
	if (format==MS_MJPEG){
		localcam->pixconv=ms_filter_new(MS_MJPEG_DEC_ID);
	}else{
		localcam->pixconv = ms_filter_new(MS_PIX_CONV_ID);
		/*set it to the pixconv */
		ms_filter_call_method(localcam->pixconv,MS_FILTER_SET_PIX_FMT,&format);
		ms_filter_call_method(localcam->webcam,MS_FILTER_GET_VIDEO_SIZE,&vsize);
		ms_filter_call_method(localcam->pixconv,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	}
	localcam->sizeconv=ms_filter_new(MS_SIZE_CONV_ID);
	ms_filter_call_method(localcam->sizeconv,MS_FILTER_SET_VIDEO_SIZE,&vsize);

	localcam->itc_sink = ms_filter_new(MS_CONF_ITC_SINK_ID);

	ms_filter_link(localcam->webcam,0,localcam->pixconv,0);
	ms_filter_link(localcam->pixconv,0,localcam->sizeconv,0);
	ms_filter_link(localcam->sizeconv,0,localcam->itc_sink,0);


	localcam->ticker = ms_ticker_new();
	ms_ticker_set_name(localcam->ticker,"Local Camera MSTicker");

	ms_ticker_attach(localcam->ticker,localcam->webcam);

	return 0;
}


static void webcam_free(ConfWebCam *localcam){

	if (localcam->ticker!=NULL) ms_ticker_destroy(localcam->ticker);
	if (localcam->webcam!=NULL) ms_filter_destroy(localcam->webcam);
	if (localcam->pixconv!=NULL) ms_filter_destroy(localcam->pixconv);
	if (localcam->sizeconv!=NULL) ms_filter_destroy(localcam->sizeconv);
	if (localcam->itc_sink!=NULL) ms_filter_destroy(localcam->itc_sink);
	ms_free(localcam);
}

void local_webcam_stop(ConfWebCam *localcam){

	if(localcam->ticker!=NULL)
	{

		ms_ticker_detach(localcam->ticker,localcam->webcam);

		ms_filter_unlink(localcam->webcam,0,localcam->pixconv,0);
		ms_filter_unlink(localcam->pixconv,0,localcam->sizeconv,0);
		ms_filter_unlink(localcam->sizeconv,0,localcam->itc_sink,0);

	}
	webcam_free(localcam);
}


int linphone_conference_add_local_webcam(LinphoneConference *conf, ConfWebCam *localcam){

	ConfVideoPort *video_port = find_free_video_slot(conf->conf_stream);


	if(!conf || !localcam || !video_port) return -1;


	int tmp =  video_port->index;

	if(conf->conf_stream->auto_layout)//自动布局模式下只开启 0 号端口的静态图片 
	{
		if(tmp==0) ms_filter_call_method(conf->conf_stream->video_static_image_tee,MS_TEE_MUTE,&tmp);

	}else{
		ms_filter_call_method(conf->conf_stream->video_static_image_tee,MS_TEE_MUTE,&tmp);
	}

	ms_message("Add Local Camera: conf_stream [%d] <----> local_webcam !!",video_port->index);

	video_port->sink_connected = TRUE;
	ms_filter_call_method(video_port->video_itc_source,MS_CONF_ITC_SOURCE_CONNECT,&video_port->slotq_in);
	ms_filter_call_method(localcam->itc_sink,MS_CONF_ITC_SINK_CONNECT,&video_port->slotq_in);
	video_port->source_connected = TRUE;
	localcam->video_port_index = video_port->index;

	conf->local_webcams = ms_list_append(conf->local_webcams,localcam);

}

int linphone_conference_remove_local_all_webcams(LinphoneConference *conf)
{
	if(!conf || conf->local_webcams==NULL) return -1;

	MSList *list =  conf->local_webcams;

	for(;list!=NULL;list=list->next){
		ConfWebCam* cam = (ConfWebCam*) list->data;
		linphone_conference_remove_local_webcam(conf,cam);
		local_webcam_stop(cam);
	}
	ms_list_free(list);

	conf->local_webcams=NULL;
	return 0;
}


ConfWebCam *find_local_webcam(MSList *list,int index)
{
	int i = 0;

	for(;list!=NULL;list=list->next){
		ConfWebCam* cam = (ConfWebCam*) list->data;
		if(i == index) return cam;
		i++;
	}
	return NULL;
}

int linphone_conference_remove_local_webcam_at_index(LinphoneConference *conf, int index)
{
	ConfWebCam *localcam=NULL;

	if(conf==NULL || conf->local_webcams==NULL) return -1;

	localcam = find_local_webcam(conf->local_webcams,index);

	if(localcam==NULL) return -2;

	linphone_conference_remove_local_webcam(conf,localcam);

	conf->local_webcams = ms_list_remove(conf->local_webcams,localcam);
}


int linphone_conference_remove_local_webcam(LinphoneConference *conf, ConfWebCam *localcam)
{
	if(conf==NULL || localcam==NULL) return -1;

	ConfVideoPort *video_port = find_video_slot_at_index(conf->conf_stream,localcam->video_port_index);

	if(localcam->ticker && video_port)
	{

		ms_message("Remove Local Camera: conf_stream [%d] <----> local_webcam !!",video_port->index);

		ms_filter_call_method(video_port->video_itc_source,MS_CONF_ITC_SOURCE_CONNECT,NULL);
		ms_filter_call_method(localcam->itc_sink,MS_CONF_ITC_SINK_CONNECT,NULL);
		video_port->sink_connected = FALSE;
		video_port->source_connected = FALSE;
		localcam->video_port_index = -1;

		int tmp =  video_port->index;

		if(conf->conf_stream->auto_layout)//自动布局模式下只开启 0 号端口的静态图片 
		{
			if(tmp==0) ms_filter_call_method(conf->conf_stream->video_static_image_tee,MS_TEE_UNMUTE,&tmp);

		}else{
			ms_filter_call_method(conf->conf_stream->video_static_image_tee,MS_TEE_UNMUTE,&tmp);
		}

		ms_filter_call_method_noarg(conf->conf_stream->video_mixer,MS_FILTER_REQ_VFU);
	}
}


/*视频录制或发布*/
int linphone_conference_start_video_record(LinphoneConference *conf,const char *filename)
{
	LinphoneConferenceStream *stream = conf->conf_stream;

	if(strlen(filename)<=0 || stream==NULL) return -1;

	return conf_stream_start_record(stream,
		filename,
		conf->hd_audio? 16000:8000,
		1,
		conf->vsize,
		conf->fps,
		conf->bit_rate);
}

int linphone_conference_stop_video_record(LinphoneConference *conf)
{
	LinphoneConferenceStream *stream = conf->conf_stream;

	if(stream==NULL) return -1;

	return conf_stream_stop_record(stream);
}


bool_t linphone_conference_in_video_recording(LinphoneConference *conf)
{
	return conf_stream_in_recording(conf->conf_stream);
}