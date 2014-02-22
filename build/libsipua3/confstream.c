#include "conference.h"
#include "conf_itc.h"
#include "mediastreamer2/msitc.h"
#include "mediastreamer2/mstee.h"
#include "conf_private.h"

static MSWebCam *get_image_webcam_device(){
	return ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),"Static picture");
}

static void choose_display_name(LinphoneConferenceStream *stream){
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
/*查找空闲的对接端口*/
ConfAudioPort *find_free_audio_slot(LinphoneConferenceStream *conf_stream)
{
	MSList *list = conf_stream->audio_ports;

	for(;list!=NULL;list=list->next){
		ConfAudioPort* port = (ConfAudioPort*) list->data;
		if(!audio_slot_in_used(port)) return port;
	}
	return NULL;
}
/*按端口号取出对接端口*/
ConfAudioPort *find_audio_slot_at_index(LinphoneConferenceStream *conf_stream,int index)
{
	MSList *list = conf_stream->audio_ports;

	for(;list!=NULL;list=list->next){
		ConfAudioPort* port = (ConfAudioPort*) list->data;
		if(port->index == index) return port;
	}
	return NULL;
}


ConfVideoPort *find_free_video_slot(LinphoneConferenceStream *conf_stream)
{
	MSList *list = conf_stream->video_ports;

	for(;list!=NULL;list=list->next){
		ConfVideoPort* port = (ConfVideoPort*) list->data;
		if(!video_slot_in_used(port)) return port;
	}
	return NULL;
}

ConfVideoPort *find_video_slot_at_index(LinphoneConferenceStream *conf_stream,int index)
{
	MSList *list = conf_stream->video_ports;

	for(;list!=NULL;list=list->next){
		ConfVideoPort* port = (ConfVideoPort*) list->data;
		if(port->index == index) return port;
	}
	return NULL;
}

void conf_stream_set_video_size(LinphoneConferenceStream *stream,MSVideoSize vsize)
{
	stream->sent_vsize.height = vsize.height;
	stream->sent_vsize.width = vsize.width;
}

void conf_stream_set_native_window_id(LinphoneConferenceStream *stream,unsigned long video_window_id)
{
	stream->video_window_id = video_window_id;
}

LinphoneConferenceStream *conf_stream_new(bool_t hd_audio,bool_t has_video,bool_t ptt)
{
	LinphoneConferenceStream *stream = ms_new0(LinphoneConferenceStream,1);
	stream->sample_rate = hd_audio ? 16000 : 8000;

	stream->sent_vsize.width=MS_VIDEO_SIZE_CIF_W;
	stream->sent_vsize.height=MS_VIDEO_SIZE_CIF_H;

	stream->has_video = has_video;

	choose_display_name(stream);
	stream->ptt_mode = ptt;

	return stream;
}

void conf_stream_start(LinphoneConferenceStream *stream,bool_t video_monitor)
{
	bool_t ptt = stream->ptt_mode;
	int tmp;

	{
		stream->audio_ticker = ms_ticker_new();
		ms_cond_init(&stream->audio_ticker->cond,NULL);
		ms_ticker_set_name(stream->audio_ticker,"Audio Mixer MSTicker");

		stream->audio_mixer = ms_filter_new(MS_CONF_ID);
		stream->audio_filerecorder = ms_filter_new(MS_FILE_REC_ID);
		stream->audio_fileplayer = ms_filter_new(MS_FILE_PLAYER_ID);


		ms_filter_call_method(stream->audio_mixer,MS_FILTER_SET_SAMPLE_RATE,&stream->sample_rate);
		ms_filter_call_method (stream->audio_fileplayer , MS_FILTER_SET_SAMPLE_RATE,&stream->sample_rate);
		tmp = 1; /*默认采用单声道会议*/
		ms_filter_call_method (stream->audio_fileplayer , MS_FILTER_SET_NCHANNELS,&tmp);
		/*设置会议音频采样率，8k,16k,32k*/
		ms_filter_call_method(stream->audio_filerecorder,MS_FILTER_SET_SAMPLE_RATE,&stream->sample_rate);

		ms_message("Link audio filter list!!!");

		/*创建录制端口*/
		{
			int audio_mixer_pin = NB_AUDIO_MAX_OUPUT_PINS-2;
			stream->audio_record_sink = ms_filter_new(MS_CONF_ITC_SINK_ID);

			ms_filter_link(stream->audio_mixer,audio_mixer_pin,stream->audio_record_sink,0); 
		}

		{/*创建音频对接端口*/
			int max_ports = stream->max_ports;
			for(int i=0; i< max_ports; i++)
			{
				ConfAudioPort *port = audio_slot_new();
				audio_slot_init(port,i);

				stream->audio_ports = ms_list_append(stream->audio_ports, port);

				/*顺序连接音频对接端口*/
				ms_filter_link(port->audio_itc_source,0,stream->audio_mixer,i);// <--- call[i].sink <--- call[i].decode <--- call[i].rtp_recv
				ms_filter_link(stream->audio_mixer,i,port->audio_itc_sink,0);  // ---> call[i].itc_source --->call[i].encode --->call[i].rtp_send

			}

			/*将音频播放，及录制连接至最末端端口*/
			ms_filter_link(stream->audio_fileplayer,0,stream->audio_mixer,stream->max_ports);
			ms_filter_link(stream->audio_mixer,stream->max_ports,stream->audio_filerecorder,0);
		}

		/*启动音频mixer线程*/
		ms_ticker_attach(stream->audio_ticker,stream->audio_mixer);
	}


	if(stream->has_video)
	{
		ms_message("Link video filter list!!!");

		MSPixFmt format=MS_YUV420P;
		MSVideoSize vsize,cam_vsize,disp_size;
		stream->video_mixer=ms_filter_new_from_name("MSVideoMixer");

		if(stream->video_mixer==NULL){
			ms_error("Not have MSVideoStitcher, Can't to Video Conference!!!");
			return;
		}

		stream->video_ticker = ms_ticker_new();
		ms_cond_init(&stream->video_ticker->cond,NULL);
		
		ms_ticker_set_name(stream->video_ticker,"Video Mixer MSTicker");
		/*设置视频会议的输出尺寸*/
		ms_filter_call_method(stream->video_mixer,MS_FILTER_SET_VIDEO_SIZE,&stream->sent_vsize);
		/*添加服务器端视频监视,可添加jpeg画面截取filer，用于web端实时会议的画面截取*/
		if(video_monitor) stream->video_output=ms_filter_new_from_name (stream->display_name);

		/*配置监视器*/
		if(stream->video_output!=NULL){
			disp_size = stream->sent_vsize;
			tmp=1;
			ms_filter_call_method(stream->video_output,MS_FILTER_SET_VIDEO_SIZE,&disp_size);
			ms_filter_call_method(stream->video_output,MS_VIDEO_DISPLAY_ENABLE_AUTOFIT,&tmp);
			ms_filter_call_method(stream->video_output,MS_FILTER_SET_PIX_FMT,&format);

			/*在已有窗口上绘图，或创建新window*/
			if (stream->video_window_id!=0)
				ms_filter_call_method(stream->video_output, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&stream->video_window_id);
		
			/*设置服务器端 视频监视filter，将其连接至视频Mixer输出最后的端口上*/
			ms_filter_link (stream->video_mixer,VIDEO_MIXER_MAX_OUTPUTS-1, stream->video_output, 0);
		}

		/*输出录制端口*/
		{
			int video_mixer_pin = VIDEO_MIXER_MAX_OUTPUTS-2;
			stream->video_record_sink = ms_filter_new(MS_CONF_ITC_SINK_ID);
			ms_filter_link(stream->video_mixer,video_mixer_pin,stream->video_record_sink,0); 
		}

		/*创建无信号输出时，显示的图片*/
		stream->video_static_image = ms_web_cam_create_reader(get_image_webcam_device());
		
		if(stream->static_image_path!=NULL)
			ms_filter_call_method(stream->video_static_image, MS_STATIC_IMAGE_SET_IMAGE,(void *)stream->static_image_path);

		ms_filter_call_method(stream->video_static_image, MS_FILTER_SET_FPS,&stream->fps);
		ms_filter_call_method(stream->video_static_image,MS_FILTER_SET_VIDEO_SIZE,&stream->sent_vsize);

		/*并行输出至所有视频端口*/
		stream->video_static_image_tee = ms_filter_new(MS_TEE_ID);

		ms_filter_link (stream->video_static_image, 0, stream->video_static_image_tee, 0);

		/*最大视频端口，需小于视频Mixer最大布局数，当前最大为 9 画面*/
		int max_ports = (stream->max_ports > NB_MAX_VIDEO_LAYOUT)? NB_MAX_VIDEO_LAYOUT:stream->max_ports;
		int static_pic_index=0;

		/*创建端口连接插座*/
		for(int i=0; i<max_ports; i++)
		{
			ConfVideoPort *port = video_slot_new();
			video_slot_init(port,i);
			stream->video_ports = ms_list_append(stream->video_ports, port);

			/**
			* 视频采集输入源 和 静态图片 同时 join 至mixer的输入端口，
			* 当视频流有效时需MUTE 静态图片的输入，反之则输入静态图片
			*
			* video_input -------> join ------> mixer ----> video_output
			*                       |  
			* static_pic  ----------+
			*
			**/

			ms_filter_link (port->video_itc_source, 0, port->video_input_join,0);
			ms_filter_link (stream->video_static_image_tee, static_pic_index++, port->video_input_join,1);

			/*自动布局模式下除了0号端口外，均不输出静态图片*/
			if(stream->auto_layout && (static_pic_index > 0)){
				ms_filter_call_method(stream->video_static_image_tee,MS_TEE_MUTE,&static_pic_index);
			}

			ms_filter_link (port->video_input_join, 0, stream->video_mixer,i);
			ms_filter_link (stream->video_mixer,i, port->video_itc_sink,0);
		}

		ms_ticker_attach(stream->video_ticker,stream->video_mixer);
	}

}


AudioStream *conf_member_get_audio_stream(LinphoneConferenceMember *mb){
	if(mb->call!=NULL && mb->call->audiostream!=NULL){
		return  mb->call->audiostream;
	}
	return NULL;
}

VideoStream *conf_member_get_video_stream(LinphoneConferenceMember *mb){
	if(mb->call!=NULL && mb->call->videostream!=NULL && mb->call->videostream->ticker!=NULL){
		return  mb->call->videostream;
	}
	return NULL;
}


int conf_stream_connect_to_audio_stream(LinphoneConferenceStream *conf_stream, AudioStream *audio_stream){

	int connect_port = -1;

	if(conf_stream==NULL || audio_stream == NULL) return connect_port;

	ConfAudioPort *audio_port = find_free_audio_slot(conf_stream);

	if(audio_port==NULL){
		ms_message("no more audio port to conf");
		return -2;
	}

	/*连接呼叫音频部分 audio_stream <--->  audio_port*/
	if(audio_stream->ticker!=NULL){
		MSConnectionHelper h;

		ms_ticker_detach(audio_stream->ticker,audio_stream->soundread);
		ms_ticker_detach(audio_stream->ticker,audio_stream->rtprecv);

		ms_message("connect: conf_stream [%d] <----> audio_stream !!",audio_port->index);

		/*dismantle the outgoing graph*/
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h,audio_stream->soundread,-1,0);
		if (audio_stream->read_resampler!=NULL)
			ms_connection_helper_unlink(&h,audio_stream->read_resampler,0,0);
#ifndef ENABLED_MCU_MEDIA_SERVER
		if (audio_stream->volsend!=NULL)
			ms_connection_helper_unlink(&h,audio_stream->volsend,0,0);
#endif // ENABLED_MCU_MEDIA_SERVER
		if (audio_stream->dtmfgen_rtp)
			ms_connection_helper_unlink(&h,audio_stream->dtmfgen_rtp,0,0);
		ms_connection_helper_unlink(&h,audio_stream->encoder,0,0);
		ms_connection_helper_unlink(&h,audio_stream->rtpsend,0,-1);

	
		/*dismantle the receiving graph*/
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h,audio_stream->rtprecv,-1,0);
		ms_connection_helper_unlink(&h,audio_stream->decoder,0,0);
		ms_connection_helper_unlink(&h,audio_stream->dtmfgen,0,0);
#ifndef ENABLED_MCU_MEDIA_SERVER
		if (audio_stream->equalizer)
			ms_connection_helper_unlink(&h,audio_stream->equalizer,0,0);
		if (audio_stream->volrecv)
			ms_connection_helper_unlink(&h,audio_stream->volrecv,0,0);
		if (audio_stream->ec)
			ms_connection_helper_unlink(&h,audio_stream->ec,0,0);
#endif // ENABLED_MCU_MEDIA_SERVER
		if (audio_stream->write_resampler)
			ms_connection_helper_unlink(&h,audio_stream->write_resampler,0,0);
		ms_connection_helper_unlink(&h,audio_stream->soundwrite,0,-1);

		/*销毁原声卡filter*/
		ms_filter_destroy(audio_stream->soundread);
		ms_filter_destroy(audio_stream->soundwrite);

		audio_stream->soundread =  ms_filter_new(MS_CONF_ITC_SOURCE_ID);
		audio_stream->soundwrite = ms_filter_new(MS_CONF_ITC_SINK_ID);


		/*处理采样率变换*/

		/*outgoing graph*/
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h,audio_stream->soundread,-1,0);
		if (audio_stream->read_resampler!=NULL)
			ms_connection_helper_link(&h,audio_stream->read_resampler,0,0);
#ifndef ENABLED_MCU_MEDIA_SERVER
		if (audio_stream->volsend!=NULL)
			ms_connection_helper_link(&h,audio_stream->volsend,0,0);
#endif // ENABLED_MCU_MEDIA_SERVER
		if (audio_stream->dtmfgen_rtp)
			ms_connection_helper_link(&h,audio_stream->dtmfgen_rtp,0,0);
		ms_connection_helper_link(&h,audio_stream->encoder,0,0);
		ms_connection_helper_link(&h,audio_stream->rtpsend,0,-1);


		/*receiving graph*/
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h,audio_stream->rtprecv,-1,0);
		ms_connection_helper_link(&h,audio_stream->decoder,0,0);
		ms_connection_helper_link(&h,audio_stream->dtmfgen,0,0);
#ifndef ENABLED_MCU_MEDIA_SERVER
		if (audio_stream->equalizer)
			ms_connection_helper_link(&h,audio_stream->equalizer,0,0);
		if (audio_stream->volrecv)
			ms_connection_helper_link(&h,audio_stream->volrecv,0,0);
		if (audio_stream->ec)
			ms_connection_helper_link(&h,audio_stream->ec,0,0);
#endif // ENABLED_MCU_MEDIA_SERVER
		if (audio_stream->write_resampler)
			ms_connection_helper_link(&h,audio_stream->write_resampler,0,0);

		ms_connection_helper_link(&h,audio_stream->soundwrite,0,-1);

		ms_ticker_attach(audio_stream->ticker,audio_stream->soundread);
		ms_ticker_attach(audio_stream->ticker,audio_stream->rtprecv);

		/*连接Call声卡I/O至conf_stream*/
		audio_port->source_connected = TRUE;
		ms_filter_call_method(audio_port->audio_itc_source,MS_CONF_ITC_SOURCE_CONNECT,&audio_port->slotq_in);
		ms_filter_call_method(audio_stream->soundwrite,MS_CONF_ITC_SINK_CONNECT,&audio_port->slotq_in);
		audio_port->sink_connected = TRUE;
		ms_filter_call_method(audio_stream->soundread,MS_CONF_ITC_SOURCE_CONNECT,&audio_port->slotq_out);
		ms_filter_call_method(audio_port->audio_itc_sink,MS_CONF_ITC_SINK_CONNECT,&audio_port->slotq_out);
		if(audio_port->muted) ms_filter_call_method(audio_port->audio_itc_sink,MS_CONF_ITC_SINK_MUTE,NULL);
		else  ms_filter_call_method(audio_port->audio_itc_sink,MS_CONF_ITC_SINK_UNMUTE,NULL);
		connect_port = audio_port->index;
	}

	return connect_port;
}


int conf_stream_disconnect_audio_stream(LinphoneConferenceStream *conf_stream, AudioStream *audio_stream, int port_index){
	if(conf_stream==NULL || audio_stream == NULL) return -1;

	ConfAudioPort *audio_port = find_audio_slot_at_index(conf_stream,port_index);

	/*audio_stream <--->  audio_port*/
	if(audio_port!=NULL && audio_stream->ticker!=NULL){
		ms_message("disconnect: conf_stream [%d] <----> audio_stream !!",audio_port->index);
		ms_filter_call_method(audio_stream->soundwrite,MS_CONF_ITC_SINK_CONNECT,NULL);
		ms_filter_call_method(audio_port->audio_itc_source,MS_CONF_ITC_SOURCE_CONNECT,NULL);
		audio_port->source_connected = FALSE;
		ms_filter_call_method(audio_port->audio_itc_sink,MS_CONF_ITC_SINK_CONNECT,NULL);
		ms_filter_call_method(audio_stream->soundread,MS_CONF_ITC_SOURCE_CONNECT,NULL);
		audio_port->sink_connected = FALSE;
	}
	return 0;
}



int conf_stream_connect_video_stream_port(LinphoneConferenceStream *conf_stream, VideoStream *video_stream){
	int connected_port = -1;

	if(conf_stream==NULL || video_stream == NULL) return -1;

	ConfVideoPort *video_port = find_free_video_slot(conf_stream);
	MSPixFmt format=MS_YUV420P;
	if(video_port==NULL){
		ms_message("no more video port to conf");
		return -2;
	}

	if(video_stream->ticker!=NULL && video_stream->output!=NULL){
		MSConnectionHelper ch;
		if (video_stream->source)
			ms_ticker_detach (video_stream->ticker, video_stream->source);
		if (video_stream->rtprecv)
			ms_ticker_detach (video_stream->ticker, video_stream->rtprecv);

		ms_message("connect: conf_stream [%d] <----> video_port !!",video_port->index);
		ms_filter_unlink(video_stream->source,0,video_stream->pixconv,0);

		if (video_stream->tee!=NULL){
			//断开本地视频预览小窗口输出
			ms_filter_unlink(video_stream->tee,1,video_stream->output,1);
		}

		/*断开解码器*/
		/* and connect the filters */
		ms_connection_helper_start (&ch);
		ms_connection_helper_unlink (&ch,video_stream->rtprecv,-1,0);
		ms_connection_helper_unlink (&ch,video_stream->decoder,0,0);
		if (video_stream->tee2){
			ms_connection_helper_unlink (&ch,video_stream->tee2,0,0);
			ms_filter_unlink(video_stream->tee2,1,video_stream->jpegwriter,0);
			//输出视频录制帧.
			if(video_stream->video_record){
				ms_filter_unlink(video_stream->tee2,2,video_stream->video_record,0);
			}

		}
		ms_connection_helper_unlink (&ch,video_stream->output,0,-1);

		//销毁旧的I/O
		ms_filter_destroy(video_stream->source);
		ms_filter_destroy(video_stream->output);

		//创建会议槽I/O
		video_stream->source =  ms_filter_new(MS_CONF_ITC_SOURCE_ID); //output = 1
		video_stream->output = ms_filter_new(MS_CONF_ITC_SINK_ID); //input = 1

		//会议参数设置
		int tmp = video_port->index;
		if(conf_stream->auto_layout)//自动布局模式下只开启 0 号端口的静态图片 
		{
			if(tmp==0) ms_filter_call_method(conf_stream->video_static_image_tee,MS_TEE_MUTE,&tmp);

		}else{
			ms_filter_call_method(conf_stream->video_static_image_tee,MS_TEE_MUTE,&tmp);
		}

		MSVideoSize sent_size;
		ms_filter_call_method(video_stream->encoder,MS_FILTER_GET_VIDEO_SIZE,&sent_size);
		ms_filter_call_method(video_stream->pixconv,MS_FILTER_SET_PIX_FMT,&format);
		ms_filter_call_method(video_stream->sizeconv,MS_FILTER_SET_VIDEO_SIZE,&sent_size);

		ms_filter_link(video_stream->source,0,video_stream->pixconv,0);

		{//重连解码器
			/* and connect the filters */
			ms_connection_helper_start (&ch);
			ms_connection_helper_link (&ch,video_stream->rtprecv,-1,0);
			ms_connection_helper_link (&ch,video_stream->decoder,0,0);
			if (video_stream->tee2){
				ms_connection_helper_link (&ch,video_stream->tee2,0,0);
				ms_filter_link(video_stream->tee2,1,video_stream->jpegwriter,0);
				//输出视频录制帧.
				if(video_stream->video_record)
					ms_filter_link(video_stream->tee2,2,video_stream->video_record,0);

			}
			ms_connection_helper_link (&ch,video_stream->output,0,-1);
		}		

		if (video_stream->source)
			ms_ticker_attach(video_stream->ticker, video_stream->source);
		if (video_stream->rtprecv)
			ms_ticker_attach (video_stream->ticker, video_stream->rtprecv);

		video_port->source_connected = TRUE;
		ms_filter_call_method(video_port->video_itc_source,MS_CONF_ITC_SOURCE_CONNECT,&video_port->slotq_in);
		ms_filter_call_method(video_stream->output,MS_CONF_ITC_SINK_CONNECT,&video_port->slotq_in);
		video_port->sink_connected = TRUE;
		ms_filter_call_method(video_stream->source,MS_CONF_ITC_SOURCE_CONNECT,&video_port->slotq_out);
		ms_filter_call_method(video_port->video_itc_sink,MS_CONF_ITC_SINK_CONNECT,&video_port->slotq_out);

		if(video_port->muted) ms_filter_call_method(video_port->video_itc_sink,MS_CONF_ITC_SINK_MUTE,NULL);
		else  ms_filter_call_method(video_port->video_itc_sink,MS_CONF_ITC_SINK_UNMUTE,NULL);

		connected_port = video_port->index;
	}

	return connected_port;
}

int conf_stream_disconnect_video_stream_port(LinphoneConferenceStream *conf_stream, VideoStream *video_stream, int port_index){

	if(conf_stream==NULL || video_stream == NULL) return -1;

	ConfVideoPort *video_port = find_video_slot_at_index(conf_stream,port_index);

	/*audio_stream <--->  audio_port*/

	if(video_stream->ticker!=NULL && video_port!=NULL){
		ms_message("disconnect: conf_stream [%d] <----> video_stream !!",video_port->index);

		/*将itc_source itc_sink 链接至 NULL 队列，分离链接*/
		video_port->source_connected = FALSE;
		ms_filter_call_method(video_port->video_itc_source,MS_CONF_ITC_SOURCE_CONNECT,NULL);
		ms_filter_call_method(video_stream->output,MS_CONF_ITC_SINK_CONNECT,NULL);

		video_port->sink_connected = FALSE;
		ms_filter_call_method(video_stream->source,MS_CONF_ITC_SOURCE_CONNECT,NULL);
		ms_filter_call_method(video_port->video_itc_sink,MS_CONF_ITC_SINK_CONNECT,NULL);

		int tmp = video_port->index;

		if(conf_stream->auto_layout){//自动布局模式下只开启 0 号端口的静态图片
			if(tmp==0) ms_filter_call_method(conf_stream->video_static_image_tee,MS_TEE_UNMUTE,&tmp);
		}else{
			ms_filter_call_method(conf_stream->video_static_image_tee,MS_TEE_UNMUTE,&tmp);
		}
	}

	return 0;
}


static void conf_stream_free(LinphoneConferenceStream *stream)
{
	if(stream->audio_ticker!=NULL)ms_ticker_destroy(stream->audio_ticker);
	if(stream->audio_fileplayer!=NULL) ms_filter_destroy(stream->audio_fileplayer);
	if(stream->audio_filerecorder!=NULL) ms_filter_destroy(stream->audio_filerecorder);
	if(stream->audio_mixer!=NULL) ms_filter_destroy(stream->audio_mixer);
	if(stream->audio_record_sink!=NULL) ms_filter_destroy(stream->audio_record_sink);
	if(stream->video_record_sink!=NULL) ms_filter_destroy(stream->video_record_sink);

	MSList *list = stream->audio_ports;

	for(;list!=NULL;list=list->next){
		ConfAudioPort* port = (ConfAudioPort*) list->data;
		audio_slot_destory(port);
	}

	ms_list_free(stream->audio_ports);

	if(stream->has_video){
		if(stream->video_ticker!=NULL){
			ms_cond_destroy(&stream->video_ticker->cond);
			ms_ticker_destroy(stream->video_ticker);
		}

		if(stream->video_mixer!=NULL) ms_filter_destroy(stream->video_mixer);
		if(stream->video_output!=NULL) ms_filter_destroy(stream->video_output);
		if(stream->video_static_image!=NULL) ms_filter_destroy(stream->video_static_image);
		if(stream->video_static_image_tee!=NULL) ms_filter_destroy(stream->video_static_image_tee);
		MSList *list = stream->video_ports;

		for(;list!=NULL;list=list->next){
			ConfVideoPort* port = (ConfVideoPort*) list->data;
			video_slot_destory(port);
		}
		ms_list_free(stream->video_ports);
	}

	if(stream->display_name!=NULL) ms_free(stream->display_name);
	if(stream->static_image_path!=NULL) ms_free(stream->static_image_path);

	ms_free(stream);
}

void conf_stream_stop(LinphoneConferenceStream *stream){

	
	if(stream->audio_ticker!=NULL && stream->audio_mixer!=NULL){
		ms_message("Detach Mixer Ticker: %s",stream->audio_ticker->name);
		ms_ticker_detach(stream->audio_ticker,stream->audio_mixer);
	}

	if(stream->video_ticker!=NULL && stream->video_mixer!=NULL){
		ms_message("Detach Mixer Ticker: %s",stream->video_ticker->name);
		ms_ticker_detach(stream->video_ticker,stream->video_mixer);
	}

	ms_message("Unlink Audio Filters!!!");
	
	if(stream->video_record_sink!=NULL)
	{
		int audio_mixer_pin = NB_AUDIO_MAX_OUPUT_PINS-2;
		ms_filter_unlink(stream->audio_mixer,audio_mixer_pin,stream->audio_record_sink,0); 
	}

	if(stream->video_record_sink!=NULL)
	{
		int video_mixer_pin = VIDEO_MIXER_MAX_OUTPUTS-2;
		ms_filter_unlink(stream->video_mixer,video_mixer_pin,stream->video_record_sink,0); 
	}

	{
		int j = 0;
		MSList *list = stream->audio_ports;

		for(;list!=NULL;list=list->next){
			ConfAudioPort* port = (ConfAudioPort*) list->data;
			/*断开混音器输入接口，与Call音频输出接口的连接*/
			ms_filter_unlink(port->audio_itc_source,0,stream->audio_mixer,j);// <--- call[i].sink <--- call[i].decode <--- call[i].rtp_recv
			/*断开混音器输出接口与Call的音频输入接口连接*/
			ms_filter_unlink(stream->audio_mixer,j,port->audio_itc_sink,0);  // ---> call[i].itc_source --->call[i].encode --->call[i].rtp_send
			j++;
		}
		/*文件播放以及录音接口 放在最后的端口*/
		ms_filter_unlink(stream->audio_fileplayer,0,stream->audio_mixer,j);
		ms_filter_unlink(stream->audio_mixer,j,stream->audio_filerecorder,0);
	}

	if (stream->video_ticker!=NULL){

		ms_message("Unlink Video Filters!!!");

		if(stream->video_output!=NULL){
			/*端口本机视频监视filter*/
			ms_filter_unlink (stream->video_mixer,VIDEO_MIXER_MAX_OUTPUTS-1, stream->video_output, 0);
		}

		ms_filter_unlink (stream->video_static_image, 0, stream->video_static_image_tee, 0);

		MSList *list = stream->video_ports;
		int i =0;
		for(;list!=NULL;list=list->next){
			ConfVideoPort* port = (ConfVideoPort*) list->data;
			/*断开会议槽链接*/
			ms_filter_unlink (port->video_itc_source, 0, port->video_input_join,0);
			ms_filter_unlink (stream->video_static_image_tee, i, port->video_input_join,1);

			ms_filter_unlink (port->video_input_join, 0, stream->video_mixer,i);
			ms_filter_unlink (stream->video_mixer, i, port->video_itc_sink,0);
			i++;
		}
	}

	conf_stream_free(stream);
}


ConfAudioPort *audio_slot_new(){

	ConfAudioPort *port = ms_new0(ConfAudioPort,1);
	port->index = -1;

	ms_mutex_init(&port->slotq_in.lock,NULL);
	ms_mutex_init(&port->slotq_out.lock,NULL);

	port->sink_connected = FALSE;
	port->source_connected = FALSE;

	return port;
}

void audio_slot_init(ConfAudioPort *slot, int index){
	slot->audio_itc_sink = ms_filter_new(MS_CONF_ITC_SINK_ID);
	slot->audio_itc_source = ms_filter_new(MS_CONF_ITC_SOURCE_ID);
	ms_queue_init(&slot->slotq_in.q);
	ms_queue_init(&slot->slotq_out.q);
	slot->index = index;
}

bool_t audio_slot_in_used(ConfAudioPort *slot){
	return (slot->sink_connected == TRUE || slot->source_connected==TRUE);
}

void audio_slot_destory(ConfAudioPort *slot){

	if(slot->audio_itc_source!=NULL) ms_filter_destroy(slot->audio_itc_source);
	if(slot->audio_itc_sink!=NULL) ms_filter_destroy(slot->audio_itc_sink);
	ms_mutex_destroy(&slot->slotq_in.lock);
	ms_queue_flush (&slot->slotq_in.q);
	ms_mutex_destroy(&slot->slotq_out.lock);
	ms_queue_flush (&slot->slotq_out.q);
	ms_free(slot);
}


ConfVideoPort *video_slot_new(){
	ConfVideoPort *port = ms_new0(ConfVideoPort,1);
	port->index = -1;
	port->sink_connected = FALSE;
	port->source_connected = FALSE;
	return port;
}

/*初始化视频端口连接插座*/
void video_slot_init(ConfVideoPort *slot,int index){

	slot->video_itc_sink = ms_filter_new(MS_CONF_ITC_SINK_ID);
	slot->video_itc_source = ms_filter_new(MS_CONF_ITC_SOURCE_ID);
	slot->video_input_join = ms_filter_new(MS_JOIN_ID);
	slot->index = index;
	ms_mutex_init(&slot->slotq_in.lock,NULL);
	ms_queue_init(&slot->slotq_in.q);
	ms_mutex_init(&slot->slotq_out.lock,NULL);
	ms_queue_init(&slot->slotq_out.q);

}

bool_t video_slot_in_used(ConfVideoPort *slot)
{
	return (slot->sink_connected == TRUE || slot->source_connected==TRUE);
}

void video_slot_destory(ConfVideoPort *slot){
	if(slot->video_itc_source!=NULL) ms_filter_destroy(slot->video_itc_source);
	if(slot->video_itc_sink!=NULL) ms_filter_destroy(slot->video_itc_sink);
	if(slot->video_input_join!=NULL) ms_filter_destroy(slot->video_input_join);
	ms_mutex_destroy(&slot->slotq_in.lock);
	ms_queue_flush (&slot->slotq_in.q);
	ms_mutex_destroy(&slot->slotq_out.lock);
	ms_queue_flush (&slot->slotq_out.q);
	ms_free(slot);
}


int conf_stream_video_get_max_input(LinphoneConferenceStream *conf_stream){
	int nb_mb=0;
	if(conf_stream->video_mixer){
		ms_filter_call_method(conf_stream->video_mixer,MS_FILTER_GET_NCHANNELS,&nb_mb);
	}
	return nb_mb;
}

int conf_stream_video_mute_at_index(LinphoneConferenceStream *conf_stream,int index){
	if(conf_stream->video_mixer){
		ms_filter_call_method(conf_stream->video_mixer,MS_VIDEO_CONF_MUTE_INPUT,&index);
		return 0;
	}
	return -1;
}

int conf_stream_video_unmute_at_index(LinphoneConferenceStream *conf_stream,int index){
	if(conf_stream->video_mixer){
		ms_filter_call_method(conf_stream->video_mixer,MS_VIDEO_CONF_UNMUTE_INPUT,&index);
		return 0;
	}
	return -1;
}


int conf_stream_start_record(LinphoneConferenceStream *stream,const char *url,int rate, int nchannels, MSVideoSize vsize, float fps, int bit_rate)
{
	int audio_bit_rate=32;
	int video_bit_rate=bit_rate-audio_bit_rate;

	if(stream->os!=NULL)
	{
		video_recoder_stop(stream->os);
	}

	if(stream->os==NULL)
	{
		stream->os = video_recoder_new();
		video_recoder_init(stream->os);
	}

	/*设置参数*/
	video_recoder_set_rate(stream->os,rate);
	video_recoder_set_vsize(stream->os, vsize);
	video_recoder_set_fps(stream->os,fps);
	video_recoder_set_audio_bit_rate(stream->os,audio_bit_rate);
	video_recoder_set_video_bit_rate(stream->os,video_bit_rate);


	if(video_recoder_open_file(stream->os,url)==0){
		video_recoder_start(stream->os);
		ms_filter_call_method(stream->audio_record_sink,MS_CONF_ITC_SINK_CONNECT,video_recoder_get_audio_inq(stream->os));
		ms_filter_call_method(stream->video_record_sink,MS_CONF_ITC_SINK_CONNECT,video_recoder_get_video_inq(stream->os));
		video_recoder_starting(stream->os,TRUE);
	}else{
		video_recoder_destory(stream->os);
		stream->os=NULL;
		return -1;
	}

	return 0;
}


int conf_stream_stop_record(LinphoneConferenceStream *stream)
{
	if(stream->os)
	{
		ms_filter_call_method(stream->audio_record_sink,MS_CONF_ITC_SINK_CONNECT,NULL);
		ms_filter_call_method(stream->video_record_sink,MS_CONF_ITC_SINK_CONNECT,NULL);

		video_recoder_stop(stream->os);
		stream->os=NULL;

		return 0;
	}

	return -1;
}


bool_t conf_stream_in_recording(LinphoneConferenceStream *stream)
{
	return (stream->os!=NULL);
}