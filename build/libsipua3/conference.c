#include "conference.h"
#include "mediastreamer2/mstee.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "conf_private.h"

const char *conference_state_to_string(LinphoneConferenceState cs)
{
	switch (cs){
		case LinphoneConferenceNew:
			return "ConferenceNew";
		case LinphoneConferenceStart:
			return "ConferenceStart";
		case LinphoneConferencePaused:
			return "ConferencePaused";
		case LinphoneConferenceResume:
			return "ConferenceResume";
		case LinphoneConferenceEmpty:
			return "ConferenceEmpty";
		case LinphoneConferenceFull:
			return "ConferenceFull";
		case LinphoneConferenceStop:
			return "ConferenceStop";
	}

	return "undefined state";
}

//创建新会议
LinphoneConference *linphone_conference_new(struct _LinphoneCore *lc,
											const char *name, 
											int max_user, 
											bool_t has_video, 
											bool_t ptt_mode, 
											bool_t hd_audio, 
											bool_t local_mode,
											bool_t video_monitor,
											bool_t auto_layout,
											MSVideoSize vsize,
											unsigned long video_window_id,
											int bit_rate)
{
	LinphoneConference *conf=ms_new0(LinphoneConference,1);
	conf->name = ms_strdup(name);
	conf->hd_audio = hd_audio;
	conf->ptt_mode = ptt_mode;
	conf->start_time=time(NULL);
	
	conf->auto_layout = auto_layout;
	conf->video_monitor = video_monitor;
	conf->video_enabled = has_video;
	conf->vsize = vsize;
	conf->local_mode = local_mode;
	conf->video_window_id = video_window_id;

	conf->max_users = max_user;
	conf->fps = 15.0;
	conf->core = lc;

	conf->conf_stream=conf_stream_new(conf->hd_audio,conf->video_enabled,conf->ptt_mode);
	
	conf->conf_stream->auto_layout = conf->auto_layout;
	conf->conf_stream->fps = conf->fps;
	conf->conf_stream->max_ports = conf->max_users;

	conf->bit_rate = bit_rate;

	conf_stream_set_video_size(conf->conf_stream,conf->vsize);

	if(conf->video_monitor) conf_stream_set_native_window_id(conf->conf_stream,conf->video_window_id);


	if(conf->conf_state_cb!=NULL)
		conf->conf_state_cb(conf->core,conf,LinphoneConferenceNew,conference_state_to_string(LinphoneConferenceNew));

	ms_message("Create Conference: [%s], max user %d",conf->name,conf->max_users);

	return conf;
}

int linphone_conference_start(struct _LinphoneCore *lc,LinphoneConference *conf)
{

	conf_stream_start(conf->conf_stream,conf->video_monitor);

	if(conf->conf_state_cb!=NULL)
		conf->conf_state_cb(conf->core,conf,LinphoneConferenceStart,conference_state_to_string(LinphoneConferenceStart));

	ms_message("Start Conference: %s",conf->name);

	return 0;
}

static void conference_free(LinphoneConference *conf)
{
	if(conf->name!=NULL) ms_free(conf->name);
	if(ms_list_size(conf->members)!=0) ms_message("free members");
	ms_free(conf);
}


int linphone_conference_stop(struct _LinphoneCore *lc,LinphoneConference *conf)
{
	if(conf_stream_in_recording(conf->conf_stream)) 
		conf_stream_stop_record(conf->conf_stream);

	/*断开已连接的声卡*/
	if(conf->local_sndcard!=NULL){
		ConfSoundCard *sndcard = conf->local_sndcard;
		linphone_conference_remove_local_sndcard(conf);
		local_sound_card_stop(sndcard);
	}

	/*断开已连接的摄像头*/
	if(ms_list_size(conf->local_webcams) > 0){
		linphone_conference_remove_local_all_webcams(conf);
	}

	/*断开所有参加会议的呼叫*/
	linphone_conference_remove_all_mbs(lc,conf);
	
	/*停止会议*/
	conf_stream_stop(conf->conf_stream);

	if(conf->conf_state_cb!=NULL)
		conf->conf_state_cb(conf->core,conf,LinphoneConferenceStop,conference_state_to_string(LinphoneConferenceStop));

	ms_message("Stop Conference: %s",conf->name);

	/*释放*/
	conference_free(conf);

	return 0;
}





//为会议播放文件
void linphone_conference_play_audio(struct _LinphoneCore *lc, LinphoneConference *conf, const char *filename){
	LinphoneConferenceStream *conf_stream = conf->conf_stream;

	if(conf==NULL && conf_stream==NULL) return;

	if (conf_stream->audio_fileplayer != NULL && ms_filter_get_id(conf_stream->audio_fileplayer)==MS_FILE_PLAYER_ID){
		ms_filter_call_method_noarg(conf_stream->audio_fileplayer,MS_FILE_PLAYER_CLOSE);
		ms_filter_call_method(conf_stream->audio_fileplayer,MS_FILE_PLAYER_OPEN,(void*)filename);
		ms_filter_call_method_noarg(conf_stream->audio_fileplayer,MS_FILE_PLAYER_START);
	}else{
		ms_error("Cannot record file: not inited conf->audio_fileplayer Filter !");
	}
}

void linphone_conference_play_audio_stop(struct _LinphoneCore *lc, LinphoneConference *conf){
	LinphoneConferenceStream *conf_stream = conf->conf_stream;

	if(conf==NULL && conf_stream==NULL) return;

	if (conf_stream->audio_fileplayer != NULL && ms_filter_get_id(conf_stream->audio_fileplayer)==MS_FILE_PLAYER_ID){
		ms_filter_call_method_noarg(conf_stream->audio_fileplayer,MS_FILE_PLAYER_STOP);
		ms_filter_call_method_noarg(conf_stream->audio_fileplayer,MS_FILE_PLAYER_CLOSE);
	}else{
		ms_error("Cannot record file: not inited conf->audio_fileplayer Filter !");
	}
}


//录制当前会议
void linphone_conference_record_audio(struct _LinphoneCore *lc, LinphoneConference *conf, const char *filename){
	LinphoneConferenceStream *conf_stream = conf->conf_stream;

	if(conf==NULL && conf_stream==NULL) return;

	if (conf_stream->audio_filerecorder != NULL && ms_filter_get_id(conf_stream->audio_filerecorder)==MS_FILE_REC_ID){
		ms_filter_call_method_noarg(conf_stream->audio_filerecorder,MS_FILE_REC_CLOSE);
		ms_filter_call_method(conf_stream->audio_filerecorder,MS_FILE_REC_OPEN,(void*)filename);
		ms_filter_call_method_noarg(conf_stream->audio_filerecorder,MS_FILE_REC_START);
	}else{
		ms_error("Cannot record file: not conf->audio_filerecorder st->filewrite Filter !");
	}
}

void linphone_conference_record_audio_stop(struct _LinphoneCore *lc, LinphoneConference *conf){
	LinphoneConferenceStream *conf_stream = conf->conf_stream;

	if(conf==NULL && conf_stream==NULL) return;

	if (conf_stream->audio_filerecorder != NULL && ms_filter_get_id(conf_stream->audio_filerecorder)==MS_FILE_REC_ID){
		ms_filter_call_method_noarg(conf_stream->audio_filerecorder,MS_FILE_REC_STOP);
		ms_filter_call_method_noarg(conf_stream->audio_filerecorder,MS_FILE_REC_CLOSE);
	}else{
		ms_error("Cannot stop conference record !");
	}
}


bool_t linphone_core_call_in_conferencing( LinphoneCore *lc, LinphoneConference *conf, LinphoneCall *call){
	MSList *it;
	MSList *list = conf->members;

	for(;list!=NULL;list=list->next){
		LinphoneConferenceMember *mb = (LinphoneConferenceMember*) list->data;
		if(mb->call == call) return TRUE;
	}

	return FALSE;
}


LinphoneConferenceMember *linphone_conference_find_mb_by_call(LinphoneCore *lc,LinphoneConference *conf, LinphoneCall *call){
	MSList *it;
	MSList *list = conf->members;

	for(;list!=NULL;list=list->next){
		LinphoneConferenceMember *mb = (LinphoneConferenceMember*) list->data;
		if(mb->call == call) return mb;
	}

	return NULL;
}

void linphone_conference_enable_vad(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled)
{
	LinphoneConferenceStream *stream = conf->conf_stream;
	int tmp;

	if(!conf || !conf->conf_stream) return;

	tmp = enabled? 1:0;

	if(ms_filter_call_method(stream->audio_mixer,MS_FILTER_ENABLE_VAD,&tmp)!=0)
		ms_warning("AudioMixer: set MS_FILTER_ENABLE_VAD error!");

	ms_message("AudioMixer: VAD %s !!",enabled? "enabled":"disabled");

}


void linphone_conference_enable_agc(struct _LinphoneCore *lc, LinphoneConference *conf,float agc_level)
{
	LinphoneConferenceStream *stream = conf->conf_stream;

	if(!conf || !conf->conf_stream) return;

	if(ms_filter_call_method(stream->audio_mixer,MS_FILTER_ENABLE_AGC,&agc_level)!=0)
		ms_warning("AudioMixer: set MS_FILTER_ENABLE_AGC error!");

	ms_message("AudioMixer: AGC %f !!",agc_level);

}

void linphone_conference_set_gain(struct _LinphoneCore *lc, LinphoneConference *conf,float gain)
{
	LinphoneConferenceStream *stream = conf->conf_stream;

	if(!conf || !conf->conf_stream) return;

	if(ms_filter_call_method(stream->audio_mixer,MS_FILTER_SET_MAX_GAIN,&gain)!=0)
		ms_warning("AudioMixer: set MS_FILTER_SET_MAX_GAIN error!");

	ms_message("AudioMixer: set GAIN %f !!",gain);
}

void linphone_conference_enable_halfduplex(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled)
{
	LinphoneConferenceStream *stream = conf->conf_stream;
	int tmp;

	if(!conf || !conf->conf_stream) return;

	tmp = enabled? 1:0;

	if(ms_filter_call_method(stream->audio_mixer,MS_FILTER_ENABLE_HALFDUPLEX,&tmp)!=0)
		ms_warning("AudioMixer: set MS_FILTER_ENABLE_HALFDUPLEX error!");

	ms_message("AudioMixer: HALF DUPLEX %s !!",enabled? "enabled":"disabled");
}

void linphone_conference_set_vad_prob_start(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled)
{
	LinphoneConferenceStream *stream = conf->conf_stream;
	int tmp;

	if(!conf || !conf->conf_stream) return;

	tmp = enabled? 100:0;

	if(ms_filter_call_method(stream->audio_mixer,MS_FILTER_SET_VAD_PROB_START,&tmp)!=0)
		ms_warning("AudioMixer: set MS_FILTER_SET_VAD_PROB_START error!");

	ms_message("AudioMixer:VAD_PROB_START %s !!",enabled? "on":"off");
}


void linphone_conference_set_vad_prob_continue(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled)
{
	LinphoneConferenceStream *stream = conf->conf_stream;
	int tmp;

	if(!conf || !conf->conf_stream) return;

	tmp = enabled? 100:0;

	if(ms_filter_call_method(stream->audio_mixer,MS_FILTER_SET_VAD_PROB_CONTINUE,&tmp)!=0)
		ms_warning("AudioMixer: set MS_FILTER_SET_VAD_PROB_CONTINUE error!");

	ms_message("AudioMixer:VAD_PROB_CONTINUE %s !!",enabled? "on":"off");

}

void linphone_conference_enable_directmode(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled)
{

	LinphoneConferenceStream *stream = conf->conf_stream;
	int tmp;

	if(!conf || !conf->conf_stream) return;

	tmp = enabled? 1:0;

	if(ms_filter_call_method(stream->audio_mixer,MS_FILTER_ENABLE_DIRECTMODE,&tmp)!=0)
		ms_warning("AudioMixer: set MS_FILTER_ENABLE_DIRECTMODE error!");

	ms_message("AudioMixer:DIRECTMODE %s !!",enabled? "on":"off");
	
}


int linphone_conference_get_members_nb(LinphoneConference * conf)
{
	return ms_list_size(conf->members);
}

MSList *linphone_conference_get_members(LinphoneConference * conf)
{
	return conf->members;
}

bool_t linphone_conference_is_full(LinphoneConference * conf)
{
	return (linphone_conference_get_members_nb(conf) >= conf->max_users);
}

bool_t linphone_conference_video_enabled(LinphoneConference * conf)
{
	return conf->video_enabled;
}

bool_t linphone_conference_ptt_enabled(LinphoneConference * conf)
{
	return conf->ptt_mode;
}


int linphone_conference_get_sample_rate(LinphoneConference * conf){
	return conf->hd_audio? 16000 : 8000;
}

MSVideoSize linphone_conference_get_video_size(LinphoneConference * conf){
	return conf->vsize;
}

float linphone_conference_get_video_fps(LinphoneConference * conf)
{
	return conf->fps;
}

void linphone_conference_enable_auto_layout(LinphoneConference *conf,bool_t enabled){
	LinphoneConferenceStream *stream = conf->conf_stream;

	if(!conf || !conf->conf_stream) return;

	stream->auto_layout = enabled;
	MSList *list = stream->video_ports;
	int port_index =0;

	for(;list!=NULL;list=list->next){
		ConfVideoPort* port = (ConfVideoPort*) list->data;
	
		if(stream->auto_layout){
			/*自动布局模式下，关闭所有视频端口的 no signal 图片输出，0 号通道除外*/
			if((port->sink_connected==FALSE && port->source_connected==FALSE) && port_index !=0 )
				ms_filter_call_method(stream->video_static_image_tee,MS_TEE_MUTE,&port_index);
			}else{
			/*如视频端口为连接call或camera,输出静态图片 0号通道除外*/
			if((port->sink_connected==FALSE && port->source_connected==FALSE) && port_index !=0 )
				ms_filter_call_method(stream->video_static_image_tee,MS_TEE_UNMUTE,&port_index);			
		}

		port_index++;
	}
	/*请求视频Mixer刷新布局*/
	ms_filter_call_method_noarg(stream->video_mixer,MS_FILTER_REQ_VFU);

	linphone_conference_send_vfu(conf);
}


void linphone_conference_show_member_at_index(LinphoneConference *conf,int index){
	int i;
	LinphoneCore *lc = conf->core;
	int nb_max = conf_stream_video_get_max_input(conf->conf_stream);
	char *msg=NULL;

	if(index >= 0){
		for(i=0;i<nb_max;i++){
			if(i==index)
				conf_stream_video_unmute_at_index(conf->conf_stream,i);
			else
				conf_stream_video_mute_at_index(conf->conf_stream,i);
		}

		msg = ms_strdup_printf("Show video index %d",index+1);
		ms_message(msg);

		if(lc && lc->vtable.display_status)
			lc->vtable.display_status(lc,msg);

	}else{
		for(i=0;i<nb_max;i++)
			conf_stream_video_unmute_at_index(conf->conf_stream,i);

		msg = ms_strdup_printf("Show  all video party");
		ms_message(msg);

		if(lc && lc->vtable.display_status)
			lc->vtable.display_status(lc,msg);
	}

	linphone_conference_send_vfu(conf);

	if(msg) ms_free(msg);
}


void linphone_core_set_mcu_mode(LinphoneCore *lc, bool_t yesno)
{
	lc->mcu_mode = yesno;
}


bool_t linphone_core_get_mcu_mode(LinphoneCore *lc)
{
	return lc->mcu_mode;
}

void linphone_conference_send_vfu(LinphoneConference *conf)
{
	/*刷新I帧*/
	MSList *list = conf->members;

	for(;list!=NULL;list=list->next){
		LinphoneCall *call = ((LinphoneConferenceMember*) list->data)->call;
		if(call->videostream) video_stream_send_vfu(call->videostream);
	}

	ms_message("Conference [%s]: video stream send vfu",conf->name);
}

int linphone_conference_mute_ouput(LinphoneConference *conf, int index)
{
	LinphoneConferenceStream *stream = conf->conf_stream;
	bool_t set_all = (index==-1)? TRUE:FALSE;
	int i;

	if(stream==NULL) return -1;

	{
		ConfAudioPort *aport;
		ConfVideoPort *vport;
		int i;
		int ap_count = ms_list_size(stream->audio_ports);
		int vp_count = ms_list_size(stream->video_ports);

		ms_message("Mute conf_stream ouput: stream->conf_ports[%d]",index);

		if(set_all)
		{

			for(i=0;i<ap_count;i++){
				aport=find_audio_slot_at_index(stream,i);
				aport->muted = TRUE;
				ms_filter_call_method(aport->audio_itc_sink,MS_CONF_ITC_SINK_MUTE,NULL);
			}

			for(i=0;i<vp_count;i++){
				vport=find_video_slot_at_index(stream,i);
				vport->muted =TRUE;
				ms_filter_call_method(vport->video_itc_sink,MS_CONF_ITC_SINK_MUTE,NULL);
			}
		}else
		{
			aport=find_audio_slot_at_index(stream,index);
			aport->muted = TRUE;
			if(aport) ms_filter_call_method(aport->audio_itc_sink,MS_CONF_ITC_SINK_MUTE,NULL);
			
			vport=find_video_slot_at_index(stream,index);
			vport->muted = TRUE;
			if(vport) ms_filter_call_method(vport->video_itc_sink,MS_CONF_ITC_SINK_MUTE,NULL);
		}

	}

	return 0;
}

int linphone_conference_unmute_ouput(LinphoneConference *conf,int index)
{
	LinphoneConferenceStream *stream = conf->conf_stream;
	bool_t set_all = (index==-1)? TRUE:FALSE;
	int i;

	if(stream==NULL) return -1;

	{
		ConfAudioPort *aport;
		ConfVideoPort *vport;
		int i;
		int ap_count = ms_list_size(stream->audio_ports);
		int vp_count = ms_list_size(stream->video_ports);

		ms_message("Unmute conf_stream ouput: stream->conf_ports[%d]",index);

		if(set_all)
		{

			for(i=0;i<ap_count;i++){
				aport=find_audio_slot_at_index(stream,i);
				aport->muted = FALSE;
				ms_filter_call_method(aport->audio_itc_sink,MS_CONF_ITC_SINK_UNMUTE,NULL);
			}

			for(i=0;i<vp_count;i++){
				vport=find_video_slot_at_index(stream,i);
				vport->muted =FALSE;
				ms_filter_call_method(vport->video_itc_sink,MS_CONF_ITC_SINK_UNMUTE,NULL);
			}
		}else
		{
			aport=find_audio_slot_at_index(stream,index);
			aport->muted = FALSE;
			if(aport) ms_filter_call_method(aport->audio_itc_sink,MS_CONF_ITC_SINK_UNMUTE,NULL);

			vport=find_video_slot_at_index(stream,index);
			vport->muted = FALSE;
			if(vport) ms_filter_call_method(vport->video_itc_sink,MS_CONF_ITC_SINK_UNMUTE,NULL);
		}

	}


}

int linphone_conference_mute_alls_ouput(LinphoneConference *conf)
{
	return linphone_conference_mute_ouput(conf,-1);
}

int linphone_conference_unmute_alls_ouput(LinphoneConference *conf)
{
	return linphone_conference_unmute_ouput(conf,-1);
}