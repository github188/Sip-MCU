#include "conference.h"
#include "conf_private.h"

LinphoneConferenceMember *linphone_conference_new_mb(struct _LinphoneCore *lc, LinphoneCall *call)
{
	const LinphoneAddress * addr = linphone_call_get_remote_address(call);

	if(call->state != LinphoneCallStreamsRunning) return NULL;

	LinphoneConferenceMember *conf_member = ms_new0(LinphoneConferenceMember,1);
	conf_member->call = call;
	conf_member->conf_admin = FALSE;
	conf_member->muted = call->all_muted;
	conf_member->mic_volume = 1.0;
	conf_member->spk_volume = 1.0;
	conf_member->audio_port_index=-1;
	conf_member->video_port_index=-1;
	conf_member->has_video = (call->videostream != NULL);
	conf_member->name = ms_strdup(linphone_address_get_username(addr));

	return conf_member;
}

LinphoneCall * linphone_conference_mb_get_call(LinphoneConferenceMember *mb)
{
	return mb->call;
}

void linphone_conference_add_mb(struct _LinphoneCore *lc, LinphoneConference *conf, LinphoneConferenceMember *mb, int pos)
{
	LinphoneConferenceStream *conf_stream = conf->conf_stream;
	if(!mb) return;

	if(conf->conf_member_state_cb!=NULL)
		conf->conf_member_state_cb(lc,conf,mb,LinphoneConferenceMemberJoin,"LinphoneConferenceMemberJoin");


	VideoStream *video_stream = conf_member_get_video_stream(mb);
	AudioStream *audio_stream = conf_member_get_audio_stream(mb);

	mb->audio_port_index = conf_stream_connect_to_audio_stream(conf_stream,audio_stream);

	if(mb->audio_port_index == -1)
	{
		if(conf->conf_state_cb!=NULL)
			conf->conf_state_cb(conf->core,conf,LinphoneConferenceFull,"LinphoneConferenceFull");
	}

	mb->video_port_index = conf_stream_connect_video_stream_port(conf_stream,video_stream);


	mb->call->state = LinphoneCallInConferencing;
	mb->conf =conf;
	lc->current_call = NULL;
	mb->call->user_pointer = conf;

	if (lc->vtable.call_state_changed)
		lc->vtable.call_state_changed(lc,mb->call,LinphoneCallInConferencing,linphone_call_state_to_string(LinphoneCallInConferencing),0);

	conf->members = ms_list_append(conf->members,mb);

	if(conf->conf_member_state_cb!=NULL)
		conf->conf_member_state_cb(lc,conf,mb,LinphoneConferenceMemberInTalking,"LinphoneConferenceMemberInTalking");

	lc->current_call = NULL;

	linphone_conference_send_vfu(conf);
}


void linphone_conference_remove_all_mbs(struct _LinphoneCore *lc, LinphoneConference *conf)
{
	while(conf->members)
	{
		LinphoneConferenceMember* mb = (LinphoneConferenceMember*) conf->members->data;
		linphone_conference_remove_mb(lc,conf,mb);
	}
	ms_list_free(conf->members);
}

void linphone_conference_remove_mb(struct _LinphoneCore *lc, LinphoneConference *conf, LinphoneConferenceMember *mb){
	LinphoneConferenceStream *conf_stream = conf->conf_stream;

	if(!mb) return;

	MSList *elem = ms_list_find(conf->members,mb);
	conf->members = ms_list_remove_link(conf->members,elem);

	AudioStream *audio_stream = conf_member_get_audio_stream(mb);
	VideoStream *video_stream = conf_member_get_video_stream(mb);
	//断开会议断开连接
	conf_stream_disconnect_audio_stream(conf_stream,audio_stream,mb->audio_port_index);

	conf_stream_disconnect_video_stream_port(conf_stream,video_stream,mb->video_port_index);

	mb->call->user_pointer = NULL;
	mb->call->state = LinphoneCallStreamsRunning;

	if(conf->conf_member_state_cb!=NULL)
		conf->conf_member_state_cb(lc,conf,mb,LinphoneConferenceMemberLeave,"LinphoneConferenceMemberLeave");	
	//释放连接，保持通话
	linphone_core_pause_call(conf->core,mb->call);

	linphone_conference_send_vfu(conf);
}
