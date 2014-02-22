#ifndef VIDEO_MAIL_RECORD_H
#define VIDEO_MAIL_RECORD_H

#include "file_writer.h"

#ifdef __cplusplus
extern "C"{
#endif

struct _VideoMailRecord;
typedef struct _VideoMailRecord VideoMailRecord;

VideoMailRecord *video_mail_record_new();

void video_mail_record_set_rate(VideoMailRecord *vp ,int rate);
void video_mail_record_set_nchannels(VideoMailRecord *vp,int nchannels);
void video_mail_record_set_fps(VideoMailRecord *vp,float fps);
void video_mail_record_set_bit_rate(VideoMailRecord *vp,int bitrate);
void video_mail_record_set_video_size(VideoMailRecord *vp, MSVideoSize size);

void video_mail_record_set_audio_sink(VideoMailRecord *vp,MSFilter *sink);
void video_mail_record_set_video_sink(VideoMailRecord *vp,MSFilter *sink);

int video_mail_record_start(VideoMailRecord *vp, MSSndCard *sndcard,MSWebCam *webcam,unsigned long video_window_id,const char *filename);

void video_mail_record_start_recording(VideoMailRecord *vp,bool_t enabled);

void video_mail_record_stop(VideoMailRecord *vp);

#ifdef __cplusplus
}
#endif
#endif //VIDEO_MAIL_RECORD_H