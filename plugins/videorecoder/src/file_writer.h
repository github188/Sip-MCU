#ifndef FILE_WRITER_H
#define FILE_WRITER_H

#include "mediastreamer2/mediastream.h"
#include "conf_itc.h"

#ifdef __cplusplus
extern "C"{
#endif

struct _VideoRecoder;
typedef struct _VideoRecoder VideoRecoder;

VideoRecoder *video_recoder_new();
int video_recoder_init(VideoRecoder *os);
int video_recoder_open_file(VideoRecoder *os, const char *filename);
void video_recoder_set_rate(VideoRecoder *os, int rate);
void video_recoder_set_nbchannels(VideoRecoder *os, int nchannels);
void video_recoder_set_fps(VideoRecoder *os, float fps);
void video_recoder_set_vsize(VideoRecoder *os, MSVideoSize vsize);
void video_recoder_set_audio_bit_rate(VideoRecoder *os, int bitrate);
void video_recoder_set_video_bit_rate(VideoRecoder *os, int bitrate);
int video_recoder_start(VideoRecoder *os);
int video_recoder_starting(VideoRecoder *os,bool_t yesno);
int video_recoder_stop(VideoRecoder *os);
int video_recoder_destory(VideoRecoder *os);

MSFilter *video_recoder_create_video_filter(VideoRecoder *os);
MSFilter *video_recoder_create_audio_filter(VideoRecoder *os);

ConfSlotQueue *video_recoder_get_audio_inq(VideoRecoder *os);
ConfSlotQueue *video_recoder_get_video_inq(VideoRecoder *os);

MSSndCard *ms_file_writer_snd_new(VideoRecoder *os);
#define  ms_file_write_video_output_new(os) video_recoder_create_video_filter(os)

#ifdef __cplusplus
}
#endif

#endif //#endif FILE_WRITER_H