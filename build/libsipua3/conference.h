#ifndef _CONFERENCE_H
#define _CONFERENCE_H
#include "linphonecore.h"
#include "mediastreamer2/mediastream.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LINPHONE_NAME_SIZE 64

#ifndef NB_MAX_CONF_MEMBERS
#define NB_MAX_CONF_MEMBERS	(16)
#endif

#ifndef VIDEO_MIXER_MAX_OUTPUTS
#define VIDEO_MIXER_MAX_OUTPUTS	(16)
#endif


#ifndef NB_MAX_VIDEO_LAYOUT
#define NB_MAX_VIDEO_LAYOUT (9)
#endif

#ifndef NB_AUDIO_MAX_OUPUT_PINS
#define NB_AUDIO_MAX_OUPUT_PINS (64)
#endif

typedef enum _LinphoneConferenceState {
	LinphoneConferenceNew,
	LinphoneConferenceStart,
	LinphoneConferencePaused,
	LinphoneConferenceResume,
	LinphoneConferenceEmpty,
	LinphoneConferenceFull,
	LinphoneConferenceStop
}LinphoneConferenceState;

const char *conference_state_to_string(LinphoneConferenceState cs);

typedef enum _LinphoneConferenceMemberStatus { 
	LinphoneConferenceMemberJoin, /** 正在加入会议 */
	LinphoneConferenceMemberInTalking,/** 会议中 */
	LinphoneConferenceMemberTalkingOnly, /** 只能发言 */
	LinphoneConferenceMemberListeningOnly, /** 只能收听 */
	LinphoneConferenceMemberMuted, /** 被静音 */
	LinphoneConferenceMemberLeave /** 离开会议 */
} LinphoneConferenceMemberStatus;

struct _LinphoneConference;
typedef struct _LinphoneConference LinphoneConference;

struct _LinphoneConferenceMember;
typedef struct _LinphoneConferenceMember LinphoneConferenceMember;

struct _LinphoneConferenceStream;
typedef struct _LinphoneConferenceStream LinphoneConferenceStream;

//会议事件回调
typedef void (*LinphoneConferenceStateCb)(struct _LinphoneCore *lc,struct _LinphoneConference *conf, LinphoneConferenceState gstate, const char *message);

//成员事件回调
typedef void (*LinphoneConferenceMemberStateCb)(struct _LinphoneCore *lc,struct _LinphoneConference *conf,struct _LinphoneConferenceMember *member, LinphoneConferenceMemberStatus gstate, const char *message);

typedef enum _MemberType{
	MEMBER_TYPE_DEVICE,
	MEMBER_TYPE_CALL,
	MEMBER_TYPE_STREAM_RTSP,
	MEMBER_TYPE_STREAM_UDT,
	MEMBER_TYPE_STREAM_FILE
}MemberType;

struct _ConfSoundCard;
typedef struct _ConfSoundCard ConfSoundCard;

ConfSoundCard *local_sound_card_new();
int local_sound_card_start(ConfSoundCard *sndcard,MSSndCard *playcard, MSSndCard *captcard,int sample_rate);
void local_sound_card_stop(ConfSoundCard *sndcard);

struct _ConfWebCam;
typedef struct _ConfWebCam ConfWebCam;

ConfWebCam *local_webcam_new();
int local_webcam_start(ConfWebCam *localcam,MSWebCam *device,MSVideoSize vsize,float fps);
void local_webcam_stop(ConfWebCam *localcam);

struct _ConfAudioPort;
typedef struct _ConfAudioPort ConfAudioPort;

ConfAudioPort *audio_slot_new();
void audio_slot_init(ConfAudioPort *slot,int index);
bool_t audio_slot_in_used(ConfAudioPort *slot);
void audio_slot_destory(ConfAudioPort *slot);

struct _ConfVideoPort;
typedef struct _ConfVideoPort ConfVideoPort;

ConfVideoPort *video_slot_new();
void video_slot_init(ConfVideoPort *slot,int index);
bool_t video_slot_in_used(ConfVideoPort *slot);
void video_slot_destory(ConfVideoPort *slot);


//Conference Stream 操作函数
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
											int bit_rate);


int linphone_conference_start(struct _LinphoneCore *lc,LinphoneConference *conf);
int linphone_conference_stop(struct _LinphoneCore *lc,LinphoneConference *conf);

int linphone_conference_add_local_sndcard(LinphoneConference *conf, ConfSoundCard *sndcard);
int linphone_conference_remove_local_sndcard(LinphoneConference *conf);

int linphone_conference_add_local_webcam(LinphoneConference *conf, ConfWebCam *localcam);
int linphone_conference_remove_local_webcam(LinphoneConference *conf, ConfWebCam *localcam);
int linphone_conference_remove_local_all_webcams(LinphoneConference *conf);
int linphone_conference_remove_local_webcam_at_index(LinphoneConference *conf, int index);

/*视频录制或发布*/
int linphone_conference_start_video_record(LinphoneConference *conf,const char *filename);
bool_t linphone_conference_in_video_recording(LinphoneConference *conf);
int linphone_conference_stop_video_record(LinphoneConference *conf);

int linphone_conference_mute_ouput(LinphoneConference *conf, int index);
int linphone_conference_unmute_ouput(LinphoneConference *conf,int index);
int linphone_conference_mute_alls_ouput(LinphoneConference *conf);
int linphone_conference_unmute_alls_ouput(LinphoneConference *conf);


void linphone_conference_play_audio(struct _LinphoneCore *lc, LinphoneConference *conf, const char *filename);
void linphone_conference_play_audio_stop(struct _LinphoneCore *lc, LinphoneConference *conf);
void linphone_conference_record_audio(struct _LinphoneCore *lc, LinphoneConference *conf, const char *filename);
void linphone_conference_record_audio_stop(struct _LinphoneCore *lc, LinphoneConference *conf);
void linphone_conference_enable_vad(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled);
void linphone_conference_enable_agc(struct _LinphoneCore *lc, LinphoneConference *conf,float agc_level);
void linphone_conference_set_gain(struct _LinphoneCore *lc, LinphoneConference *conf,float gain);
void linphone_conference_enable_halfduplex(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled);
void linphone_conference_set_vad_prob_start(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled);
void linphone_conference_set_vad_prob_continue(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled);
void linphone_conference_enable_directmode(struct _LinphoneCore *lc, LinphoneConference *conf,bool_t enabled);

int linphone_conference_get_members_nb(LinphoneConference * conf);
MSList *linphone_conference_get_members(LinphoneConference * conf);
bool_t linphone_conference_is_full(LinphoneConference * conf);
bool_t linphone_conference_video_enabled(LinphoneConference * conf);
bool_t linphone_conference_ptt_enabled(LinphoneConference * conf);
int linphone_conference_get_sample_rate(LinphoneConference * conf);
MSVideoSize linphone_conference_get_video_size(LinphoneConference * conf);
float linphone_conference_get_video_fps(LinphoneConference * conf);
void linphone_conference_enable_auto_layout(LinphoneConference *conf,bool_t enabled);

/*只显示发言者视频，0-NB_MAX_VIDEO_LAYOUT, -1 为全部显示*/
void linphone_conference_show_member_at_index(LinphoneConference *conf,int index);

void linphone_conference_send_vfu(LinphoneConference *conf);

LinphoneConferenceMember *linphone_conference_new_mb(struct _LinphoneCore *lc, LinphoneCall *call);
LinphoneCall *linphone_conference_mb_get_call(LinphoneConferenceMember *mb);
void linphone_conference_add_mb(struct _LinphoneCore *lc, LinphoneConference *conf, LinphoneConferenceMember *mb, int pos);
bool_t linphone_core_call_in_conferencing( LinphoneCore *lc, LinphoneConference *conf, LinphoneCall *call);

LinphoneConferenceMember *linphone_conference_find_mb_by_call(struct _LinphoneCore *lc,LinphoneConference *conf, LinphoneCall *call);
void linphone_conference_remove_mb(struct _LinphoneCore *lc, LinphoneConference *conf, LinphoneConferenceMember *mb);
void linphone_conference_remove_all_mbs(struct _LinphoneCore *lc, LinphoneConference *conf);

//private
LinphoneConferenceStream *conf_stream_new(bool_t hd_audio,bool_t has_video,bool_t ptt);
void conf_stream_start(LinphoneConferenceStream *stream,bool_t video_monitor);
void conf_stream_stop(LinphoneConferenceStream *stream);

int conf_stream_start_record(LinphoneConferenceStream *stream,const char *url,int rate, int nchannels, MSVideoSize vsize, float fps, int bit_rate);
bool_t conf_stream_in_recording(LinphoneConferenceStream *stream);
int conf_stream_stop_record(LinphoneConferenceStream *stream);

void conf_stream_set_video_size(LinphoneConferenceStream *stream,MSVideoSize vsize);
void conf_stream_set_native_window_id(LinphoneConferenceStream *stream,unsigned long video_window_id);

int conf_stream_connect_to_audio_stream(LinphoneConferenceStream *conf_stream, AudioStream *audio_stream);
int conf_stream_disconnect_audio_stream(LinphoneConferenceStream *conf_stream, AudioStream *audio_stream, int port_index);
int conf_stream_connect_video_stream_port(LinphoneConferenceStream *conf_stream,  VideoStream *video_stream);
int conf_stream_disconnect_video_stream_port(LinphoneConferenceStream *conf_stream, VideoStream *video_stream, int port_index);

int conf_stream_video_get_max_input(LinphoneConferenceStream *conf_stream);
int conf_stream_video_mute_at_index(LinphoneConferenceStream *conf_stream,int index);
int conf_stream_video_unmute_at_index(LinphoneConferenceStream *conf_stream,int index);

AudioStream *conf_member_get_audio_stream(LinphoneConferenceMember *mb);
VideoStream *conf_member_get_video_stream(LinphoneConferenceMember *mb);
ConfAudioPort *find_free_audio_slot(LinphoneConferenceStream *conf_stream);
ConfAudioPort *find_audio_slot_at_index(LinphoneConferenceStream *conf_stream,int index);
ConfVideoPort *find_free_video_slot(LinphoneConferenceStream *conf_stream);
ConfVideoPort *find_video_slot_at_index(LinphoneConferenceStream *conf_stream,int index);


#ifdef __cplusplus
}
#endif
#endif