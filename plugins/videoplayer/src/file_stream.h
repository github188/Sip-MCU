#ifndef FILE_STREAM_READER
#define FILE_STREAM_READER

#include <mediastreamer2/mediastream.h>
#include "conf_itc.h"

#ifdef __cplusplus
extern "C"{
#endif


struct _VideoState;
typedef struct _VideoState VideoState;

VideoState * file_stream_new();
int file_stream_init(VideoState *is);
int file_stream_open(VideoState *is, const char *filename);
int file_stream_start(VideoState *is);
int file_stream_stop(VideoState *is);
void file_stream_destory(VideoState *is);

bool_t file_stream_is_playling(VideoState *is);

int file_stream_do_seek(VideoState *is,double pos);
int file_stream_audio_get_rate(VideoState *is);
int file_stream_audio_get_nchannels(VideoState *is);
float file_stream_get_duration(VideoState *is);

//播放函数
int file_stream_play_to_position(VideoState *is,float pos);
int file_stream_play(VideoState *is, bool_t repeat);
int file_stream_pause(VideoState *is);

extern mblk_t *mono2stereo(mblk_t *im);
extern mblk_t *stereo2mono(mblk_t *im);

typedef struct _StreamData{
	MSVideoSize vsize;
	float fps;
	int rate;
	int nchannels;
	VideoState *is;
	int index;
	uint64_t starttime;
}StreamData;

//创建音频/视频源
MSFilter *file_stream_create_audio_filter(VideoState *is);
MSFilter *file_stream_create_video_filter(VideoState *is);

//用于Filter连接成功后输出数据
int file_stream_output_data(VideoState *is,bool_t enabled);

//视频输出设置
MSVideoSize file_stream_video_get_vsize(VideoState *is);
float file_stream_video_get_fps(VideoState *is);

struct _VideoPlayer;
typedef struct _VideoPlayer VideoPlayer;


VideoPlayer *video_player_new();

void video_player_set_rate(VideoPlayer *vp ,int rate);
void video_player_set_nchannels(VideoPlayer *vp,int nchannels);
void video_player_set_fps(VideoPlayer *vp,float fps);
void video_player_set_video_size(VideoPlayer *vp, MSVideoSize size);

void video_player_set_audio_source(VideoPlayer *vp,MSFilter *source);
void video_player_set_video_source(VideoPlayer *vp,MSFilter *source);

void video_player_start(VideoPlayer *vp, MSSndCard *snd_playbackcard, MSSndCard *snd_capcard, MSWebCam *cam, unsigned long video_window_id);

void video_player_stop(VideoPlayer *vp);

//创建虚拟摄像头
MSWebCam *ms_file_player_cam_new(VideoState *is);
//创建只读声卡
MSSndCard *ms_file_player_snd_new(VideoState *is);


//private:
ConfSlotQueue *video_player_get_audio_outq(VideoState *is);
ConfSlotQueue *video_player_get_video_outq(VideoState *is);

#ifdef __cplusplus
}
#endif

#endif