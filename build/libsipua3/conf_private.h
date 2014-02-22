#ifndef _CONF_PRIVATE_H
#define _CONF_PRIVATE_H

#include "private.h"
#include "conf_itc.h"
#include "file_writer.h"

struct _LinphoneConferenceStream{
	int max_ports;
	int sample_rate;
	bool_t ptt_mode;
	bool_t has_video;
	MSTicker *audio_ticker;/*音频线程*/
	MSFilter *audio_mixer;/*混音器*/
	MSFilter *audio_filerecorder;/*录制会议*/
	MSFilter *audio_fileplayer;/*播放录音*/
	MSList *audio_ports;

	bool_t video_monitor; /* 本地监视*/
	bool_t auto_layout; /* 自动布局或固定布局*/
	char *display_name;
	char *static_image_path;
	MSVideoSize sent_vsize; /*会议输出尺寸*/
	float fps;
	unsigned long video_window_id;/*视频输出窗口*/
	MSList *video_ports;
	MSTicker *video_ticker; /*视频线程*/
	MSFilter *video_output;/*本地视频输出*/
	MSFilter *video_mixer;/*视频分屏*/
	MSFilter *video_static_image;/*成员无信号时输出静态图片占位*/
	MSFilter *video_static_image_tee;/* 静态图片并发器*/

	/*录制*/
	VideoRecoder *os;
	int record_count;
	MSFilter *audio_record_sink;
	MSFilter *video_record_sink;
};

struct _LinphoneConference
{
	char *name;
	struct _LinphoneCore *core;
	MSList *members; /* 参与成员列表 */
	time_t start_time; /*会议开始时间*/
	int max_users;   /*会议参与人数*/

	int layout_index; /*布局索引*/
	int bit_rate;
	float fps;
	MSVideoSize vsize; /*会议输出尺寸*/
	unsigned long video_window_id;/*视频输出窗口*/
	bool_t video_enabled; /* 视频启用视频 */
	bool_t video_monitor; /* 本地监视*/
	bool_t auto_layout; /* 自动布局或固定布局*/
	bool_t ptt_mode; /* PTT模式 .*/
	bool_t hd_audio; /* 8K 或 16K 高清音频模式*/
	bool_t local_mode; /*服务器或端点模式*/
	struct _LinphoneConferenceStream *conf_stream;/*会议媒体核心*/
	ConfSoundCard *local_sndcard;
	MSList *local_webcams; /*本地摄像头*/
	LinphoneConferenceStateCb conf_state_cb;
	LinphoneConferenceMemberStateCb conf_member_state_cb;
};



struct _LinphoneConferenceMember{
	const char *name;
	struct _LinphoneConference *conf; /* 会议指针*/
	struct _LinphoneCall *call; /*参与会议的呼叫*/
	bool_t has_video; /* 是否有视频 */
	bool_t muted; /*是否静音*/
	bool_t conf_admin;/*是否为会议主持人*/
	float mic_volume; /*音量*/
	float spk_volume;
	int audio_port_index;
	int video_port_index;/*在会议中的位置*/
};

struct _ConfAudioPort{
	int index;
	bool_t muted;
	ConfSlotQueue slotq_in;
	ConfSlotQueue slotq_out;
	bool_t sink_connected;
	bool_t source_connected;
	MSFilter *audio_itc_sink;
	MSFilter *audio_itc_source;
};

//本地摄像头
struct _ConfWebCam{
	int video_port_index;/*ConfVideoPort 对应端口号*/
	MSTicker *ticker;
	MSFilter *webcam;/*摄像头*/
	MSFilter *sizeconv;/*摄像头尺寸转换*/
	MSFilter *pixconv;/*摄像头格式转换*/
	MSFilter *itc_sink;
};


//本地声卡
struct _ConfSoundCard{
	int audio_port_index;/*ConfAudioPort 对应端口号*/
	MSTicker *ticker;
	MSFilter *capture_card;/*麦克风*/
	MSFilter *playback_card;/*/声卡输出*/
	MSFilter *itc_sink;
	MSFilter *itc_source;
};


struct _ConfVideoPort{
	int index;
	bool_t muted;
	ConfSlotQueue slotq_in;
	ConfSlotQueue slotq_out;
	bool_t sink_connected;
	bool_t source_connected;
	MSFilter *video_itc_sink;
	MSFilter *video_itc_source;
	MSFilter *video_input_join;
};

#endif