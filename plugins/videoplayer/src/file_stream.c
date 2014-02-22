#include "file_stream.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msextdisplay.h"
#include "mediastreamer2/mschanadapter.h"
#include "stdint.h"
#include <speex/speex_resampler.h>

#ifdef __cplusplus
extern "C"{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavcodec/audioconvert.h>
#ifdef __cplusplus
};
#endif

#ifdef TARGET_OS_IPHONE

#define ms_yuv_buf_alloc yuv_buf_alloc

static VideoStreamRenderCallback rendercb=NULL;
static void *render_pointer=NULL;

void set_video_render_callback(void *fuc, void *ptr)
{
    rendercb = fuc;
    render_pointer = ptr;
}

static void ext_display_cb(void *ud, unsigned int event, void *eventdata){
	MSExtDisplayOutput *output=(MSExtDisplayOutput*)eventdata;
	VideoStream *st=(VideoStream*)ud;
	if (rendercb!=NULL){
		rendercb(render_pointer,
                     output->local_view.w!=0 ? &output->local_view : NULL,
                     output->remote_view.w!=0 ? &output->remote_view : NULL);
	}
}

#endif // TARGET_OS_IPHONE

// int buffer_size = (采样率 * 2字节 *声道数)/1000 * 毫秒长度;
// 例如: 160 = (8000 * 2 *1)/1000 * 10;
/*********************************************************************************************************/
/*                                file_stream_reader   结构图

包读取线程------> audio_inq ----> 音频解码线程(synchronize_audio)----> audio_outq ----> 缓冲线程---->
|                                        |                              |
|                                        |                              |
|                      音视频同步--------|         数据输出开关---------|
|                                        |                              |
|                                        |                              |
----> video_inq ----> 视频解码线程(synchronize_video)----> video_outq ----> 缓冲线程 ---->

*/
/********************************************************************************************************/

#define MAX_QUEUE_SIZE (30 * 1024 * 1024)
#define MIN_AUDIOQ_SIZE (20 * 16 * 1024)
#define MIN_FRAMES 5
#define MIN_AUDIO_BUFS 20 //10ms * 30 = 300ms 缓冲时间
#define AUDIO_DIFF_AVG_NB 20
#define NB_SAMPLES 10 /* 每次读取音频样本长度*/


struct _PacketQueue {
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	ms_mutex_t mutex;
	ms_cond_t cond;
	bool_t abort_request;
} ;

typedef struct _PacketQueue PacketQueue;

void packet_queue_init(PacketQueue *q);
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
void packet_queue_flush(PacketQueue *q);


struct _VideoState {
	AVFormatContext *pFormatCtx;
	int             videoStream, audioStream;
	int             seek_req;
	int             seek_flags;
	int64_t         seek_pos;
	int64_t			seek_rel;
	double          audio_clock;
	AVStream        *audio_st;
	PacketQueue     audioq;

	/* samples output by the codec. we reserve more space for avsync
	compensation */
	DECLARE_ALIGNED(16,uint8_t,audio_buf1)[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
	DECLARE_ALIGNED(16,uint8_t,audio_buf2)[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
	uint8_t *audio_buf;
	unsigned int audio_buf_size; /* in bytes */
	int audio_buf_index; /* in bytes */
	AVPacket audio_pkt_temp;
	AVPacket audio_pkt;
	enum SampleFormat audio_src_fmt;
	AVAudioConvert *reformat_ctx;
	int paused;

	AVStream        *video_st;
	PacketQueue     videoq;

	ms_thread_t		packet_read_thread;
	ms_mutex_t		packet_read_mutex;
	ms_thread_t     video_decoder_thread;
	ms_thread_t     audio_decoder_thread;
	char            filename[1024];
	bool_t          run;
	double          pos;

	struct ms_SwsContext *sws_ctx; /*用于尺寸和格式转换*/
	enum PixelFormat output_pix_fmt; /*输出格式 默认YUV420P*/

	YuvBuf outbuf;  /*YUV420P 存放缓冲*/
	mblk_t *yuv_msg;

	bool_t output_date;
	MSBufferizer *bufferizer;
	ConfSlotQueue audio_outq; /*音频输出队列*/
	ConfSlotQueue video_outq; /*视频输出队列*/

	//public:
	MSVideoSize vsize;
	float fps;
	int rate;
	int nchannels;
	uint64_t audio_callback_time;

	//输出格式
	MSVideoSize output_vsize;
	float output_fps;
	struct SpeexResamplerState_ *resample_handle;
	int output_rate;
	int output_nchannels;

	//音视频同步
	uint32_t audio_put_count;
	uint32_t video_get_count;
	uint32_t video_put_count;
	ms_mutex_t	time_sync_mutex;

	//rtmp
	bool_t is_rtmp;
};

struct _VideoPlayer{
	MSFilter *sndread;
	MSFilter *resampler;
	MSFilter *chanadapt;
	MSFilter *sndplayback;
	MSFilter *video_source;
	MSFilter *video_output;
	MSTicker *ticker;
	MSVideoSize vsize;
	float fps;
	int rate;
	int nchannels;
	char *display_name;
	unsigned long video_window_id;
};

static bool_t ffmpeg_initialized=FALSE;
static AVPacket flush_pkt;
static void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes);

static void ffmpeg_check_init(){
	if(!ffmpeg_initialized){
		avcodec_init();
		avcodec_register_all();
		av_register_all();
		av_init_packet(&flush_pkt);
		flush_pkt.data = (uint8_t*)"FLUSH";
		ffmpeg_initialized=TRUE;
	}
}

static int set_high_prio(void){
	int precision=2;
	int result=0;
#ifdef WIN32
	MMRESULT mm;
	TIMECAPS ptc;
	mm=timeGetDevCaps(&ptc,sizeof(ptc));
	if (mm==0){
		if (ptc.wPeriodMin<(UINT)precision)
			ptc.wPeriodMin=precision;
		else
			precision = ptc.wPeriodMin;
		mm=timeBeginPeriod(ptc.wPeriodMin);
		if (mm!=TIMERR_NOERROR){
			ms_warning("timeBeginPeriod failed.");
		}
		ms_message("win32 timer resolution set to %i ms",ptc.wPeriodMin);
	}else{
		ms_warning("timeGetDevCaps failed.");
	}

	if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST)){
		ms_warning("SetThreadPriority() failed (%d)\n", GetLastError());
	}
#else
	struct sched_param param;
	memset(&param,0,sizeof(param));
#ifdef TARGET_OS_MAC
	int policy=SCHED_RR;
#else
	int policy=SCHED_OTHER;
#endif
	param.sched_priority=sched_get_priority_max(policy);
	if((result=pthread_setschedparam(pthread_self(),policy, &param))) {
		ms_warning("Set sched param failed with error code(%i)\n",result);
	} else {
		ms_message("MS ticker priority set to max");
	}
#endif
	return precision;
}

static void unset_high_prio(int precision){
#ifdef WIN32
	MMRESULT mm;

	if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL)){
		ms_warning("SetThreadPriority() failed (%d)\n", GetLastError());
	}

	mm=timeEndPeriod(precision);
#endif
}

static int ms_queue_get_size(MSQueue *q)
{
	return q->q.q_mcount;
}

static void sleepMs(int ms){
#ifdef WIN32
	Sleep(ms);
#else
	struct timespec ts;
	ts.tv_sec=0;
	ts.tv_nsec=ms*1000000LL;
	nanosleep(&ts,NULL);
#endif
}

void packet_queue_init(PacketQueue *q) {
	memset(q, 0, sizeof(PacketQueue));
	ms_mutex_init(&q->mutex,NULL);
	ms_cond_init(&q->cond,NULL);
	q->nb_packets = 0;
	q->size = 0;
	q->abort_request=FALSE;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
	AVPacketList *pkt1;
	if(pkt != &flush_pkt && av_dup_packet(pkt) < 0) {
		return -1;
	}
	pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	ms_mutex_lock(&q->mutex);

	if (!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size;
	ms_cond_signal(&q->cond);
	ms_mutex_unlock(&q->mutex);
	return 0;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
	AVPacketList *pkt1;
	int ret;

	ms_mutex_lock(&q->mutex);

	for(;;) {

		if(q->abort_request==TRUE) {
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			av_free(pkt1);
			ret = 1;
			break;
		} else if (!block) {
			ret = 0;
			break;
		} else {
			ms_cond_wait(&q->cond, &q->mutex);
		}
	}
	ms_mutex_unlock(&q->mutex);
	return ret;
}

void packet_queue_flush(PacketQueue *q) {
	AVPacketList *pkt, *pkt1;

	ms_mutex_lock(&q->mutex);
	ms_cond_signal(&q->cond);
	for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	ms_mutex_unlock(&q->mutex);
}

void packet_queue_destroy(PacketQueue *q){
	q->abort_request=TRUE;
	packet_queue_flush(q);
	ms_mutex_destroy(&q->mutex);
	ms_cond_destroy(&q->cond);
	ms_free(q);
}


int decode_interrupt_cb(void) {
	return (FALSE);
}


/* decode one audio frame and returns its uncompressed size */
static int audio_decode_frame(VideoState *is, double *pts_ptr)
{
	AVPacket *pkt_temp = &is->audio_pkt_temp;
	AVPacket *pkt = &is->audio_pkt;
	AVCodecContext *dec= is->audio_st->codec;
	int n, len1, data_size;
	double pts;

	for(;;) {
		/* NOTE: the audio packet can contain several frames */
		while (pkt_temp->size > 0) {
			data_size = sizeof(is->audio_buf1);
			len1 = avcodec_decode_audio3(dec,
				(int16_t *)is->audio_buf1, &data_size,
				pkt_temp);
			if (len1 < 0) {
				/* if error, we skip the frame */
				pkt_temp->size = 0;
				break;
			}

			pkt_temp->data += len1;
			pkt_temp->size -= len1;
			if (data_size <= 0)
				continue;

			if (dec->sample_fmt != is->audio_src_fmt) {
				if (is->reformat_ctx)
					av_audio_convert_free(is->reformat_ctx);

				is->reformat_ctx= av_audio_convert_alloc(SAMPLE_FMT_S16, 1,dec->sample_fmt, 1, NULL, 0);

				if (!is->reformat_ctx) {
					fprintf(stderr, "Cannot convert %s sample format to %s sample format\n",
						avcodec_get_sample_fmt_name(dec->sample_fmt),
						avcodec_get_sample_fmt_name(SAMPLE_FMT_S16));
					break;
				}
				is->audio_src_fmt= dec->sample_fmt;
			}

			if (is->reformat_ctx) {
				const void *ibuf[6]= {is->audio_buf1};
				void *obuf[6]= {is->audio_buf2};
				int istride[6]= {av_get_bits_per_sample_format(dec->sample_fmt)/8};
				int ostride[6]= {2};
				int len= data_size/istride[0];
				if (av_audio_convert(is->reformat_ctx, obuf, ostride, ibuf, istride, len)<0) {
					printf("av_audio_convert() failed\n");
					break;
				}
				is->audio_buf= is->audio_buf2;
				/* FIXME: existing code assume that data_size equals framesize*channels*2
				remove this legacy cruft */
				data_size= len*2;
			}else{
				is->audio_buf= is->audio_buf1;
			}

			/* if no pts, then compute it */
			pts = is->audio_clock;
			*pts_ptr = pts;
			n = 2 * dec->channels;
			is->audio_clock += (double)data_size /
				(double)(n * dec->sample_rate);

			return data_size;
		}

		/* free the current packet */
		if (pkt->data)
			av_free_packet(pkt);

		if (is->paused || is->audioq.abort_request) {
			return -1;
		}

		/* read next packet */
		if (packet_queue_get(&is->audioq, pkt, 1) < 0)
			return -1;
		if(pkt->data == flush_pkt.data){
			avcodec_flush_buffers(dec);
			continue;
		}

		pkt_temp->data = pkt->data;
		pkt_temp->size = pkt->size;

		/* if update the audio clock with the pts */
		if (pkt->pts != AV_NOPTS_VALUE) {
			is->audio_clock = av_q2d(is->audio_st->time_base)*pkt->pts;
		}
	}
}

static double get_audio_data(VideoState *is, uint8_t *stream, int len) {
	int audio_size, len1;
	double pts = 0.0;

	is->audio_callback_time = av_gettime();

	while (len > 0) {
		if (is->audio_buf_index >= is->audio_buf_size) {
			audio_size = audio_decode_frame(is, &pts);
			if (audio_size < 0) {
				/* if error, just output silence */
				is->audio_buf = is->audio_buf1;
				is->audio_buf_size = 1024;
				memset(is->audio_buf, 0, is->audio_buf_size);
			} else {
				is->audio_buf_size = audio_size;
			}
			is->audio_buf_index = 0;
		}
		len1 = is->audio_buf_size - is->audio_buf_index;
		if (len1 > len)
			len1 = len;
		memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
		len -= len1;
		stream += len1;
		is->audio_buf_index += len1;
	}
	return pts;
}

static mblk_t *fp_audio_resample(VideoState *is, mblk_t *m)
{
	unsigned int inlen=(m->b_wptr-m->b_rptr)/(2*is->output_nchannels);
	unsigned int outlen=((inlen*is->output_rate)/is->rate)+1;
	unsigned int inlen_orig=inlen;
	mblk_t *om=allocb(outlen*2*is->output_nchannels,0);

	if (is->output_nchannels==1){
		speex_resampler_process_int(is->resample_handle, 
			0, 
			(int16_t*)m->b_rptr, 
			&inlen, 
			(int16_t*)om->b_wptr, 
			&outlen);
	}else{
		speex_resampler_process_interleaved_int(is->resample_handle, 
			(int16_t*)m->b_rptr, 
			&inlen, 
			(int16_t*)om->b_wptr, 
			&outlen);
	}
	if (inlen_orig!=inlen){
		ms_error("Bug in resampler ! only %u samples consumed instead of %u, out=%u",
			inlen,inlen_orig,outlen);
	}
	om->b_wptr+=outlen*2*is->output_nchannels;
	freemsg(m);

	return om;
}

void *audio_decoder_run(void *arg)
{	/*
	1,解码音频
	2,拷贝10ms 样本到audio_filter_output
	*/
	VideoState *is = (VideoState *)arg;

	int precision=2;
	precision = set_high_prio();

	int one_frame_size = (is->output_rate * is->output_nchannels * 2) / 1000 * NB_SAMPLES; //(160 = (8000 * 2 *1)/1000 * 10;)

	while(is->run){
		if(ms_queue_get_size(&is->audio_outq.q) >= MIN_AUDIO_BUFS && is->is_rtmp ==FALSE){/*if(QUEUE_SIZE < AUDIO_OUTPUT_PACKET_SIZE) */
			sleepMs(10);
			continue;
		}

		{
			int audio_size;
			double pts = 0.0;
			audio_size = audio_decode_frame(is, &pts);

			if (audio_size < 0) {
				/* if error, just output silence */
				is->audio_buf = is->audio_buf1;
				is->audio_buf_size = 1024;
				memset(is->audio_buf, 0, is->audio_buf_size);
			} else {
				is->audio_buf_size = audio_size;
				mblk_t *new_om=NULL;
				mblk_t *om = allocb(audio_size,0);
				memcpy(om->b_wptr,is->audio_buf,audio_size);
				om->b_wptr+=audio_size;

				if(is->nchannels==is->output_nchannels){
					new_om=om;
				}else if(is->nchannels==2){
					new_om=stereo2mono(om);
				}else if(is->output_nchannels==2){
					new_om=mono2stereo(om);
				}

				if(is->resample_handle)
					ms_bufferizer_put(is->bufferizer,fp_audio_resample(is,new_om));
				else
					ms_bufferizer_put(is->bufferizer,new_om);
				
			}
		}


		//if(is->audio_outq.run)
		{
			while(ms_bufferizer_get_avail(is->bufferizer) >= one_frame_size) {
				mblk_t *om=allocb(one_frame_size,0);
				
				ms_bufferizer_read(is->bufferizer,(uint8_t*)om->b_wptr,one_frame_size);
				om->b_wptr+=one_frame_size;

				ms_mutex_lock(&is->time_sync_mutex); //同步时间

				ms_mutex_lock(&is->audio_outq.lock);
				ms_queue_put(&is->audio_outq.q,om);
				ms_mutex_unlock(&is->audio_outq.lock);

				is->audio_put_count++;

				ms_mutex_unlock(&is->time_sync_mutex);
			}
		}

	}

	unset_high_prio(precision);
	ms_thread_exit(NULL);
	return NULL;
}

static mblk_t *get_as_yuvmsg(VideoState *s, AVFrame *orig){

	AVCodecContext *ctx=s->pFormatCtx->streams[s->videoStream]->codec;

	if (ctx->width==0 || ctx->height==0){
		ms_error("file_stream: wrong image size provided by decoder.");
		return NULL;
	}
	if (orig->data[0]==NULL){
		ms_error("file_stream: no image data.");
		return NULL;
	}
	if (s->outbuf.w!=s->output_vsize.width || s->outbuf.h!=s->output_vsize.height || s->sws_ctx==NULL){
		if (s->sws_ctx!=NULL){
			ms_sws_freeContext(s->sws_ctx);
			s->sws_ctx=NULL;
		}
		s->yuv_msg=ms_yuv_buf_alloc(&s->outbuf,s->output_vsize.width,s->output_vsize.height);
		s->outbuf.w=s->output_vsize.width;
		s->outbuf.h=s->output_vsize.height;
		s->sws_ctx=ms_sws_getContext(ctx->width,ctx->height,ctx->pix_fmt,
			s->output_vsize.width,s->output_vsize.height,s->output_pix_fmt,SWS_FAST_BILINEAR,
			NULL, NULL, NULL);
        
        ms_message("get_as_yuvmsg: create sws_ctx %dx%d --> %dx%d",ctx->width,ctx->height,s->output_vsize.width,s->output_vsize.height );
	}
	if (s->sws_ctx==NULL){
		ms_error("file_stream: missing rescaling context.");
		return NULL;
	}
	if (ms_sws_scale(s->sws_ctx,orig->data,orig->linesize, 0,
		ctx->height, s->outbuf.planes, s->outbuf.strides)<0){
			ms_error("file_stream: error in ms_sws_scale().");
	}
	return copymsg(s->yuv_msg);
}

int frame_to_picture(VideoState *is, AVFrame *pFrame, double pts) {
	/*调整帧速，丢帧或补帧
	*/
	int frame_diff =0;

	ms_mutex_lock(&is->time_sync_mutex);

	is->video_get_count++;

	float audio_time = ((float)is->audio_put_count*NB_SAMPLES)/1000.0; /*音轨时间*/
	float video_time = (is->video_get_count/is->fps);/*视频帧时间*/
	float output_video_time = (is->video_put_count/is->output_fps);/*输出视频时间*/
/*
	ms_message("audio time %f, input: fps %f  video time %f, output fps %f time %f, time diff %f"
		,audio_time
		,is->fps
		,video_time
		,is->output_fps
		,output_video_time
		,output_video_time - video_time);
*/
	/************************************************************************/
	/* 当输入文件帧数高于,输出fps设置时,,按时间戳进行丢帧处理

	原理如下:
	          0.033 = 1/30                   0.066  =1/15           
	frame     video_time(30fps) 	output_video_time(15fps)
	1         0.033                                drop            
	2         0.066                 0.066          send
	3         0.1                                  drop
	4         0.133                 0.133          send
	5         0.166                                drop
	
	当输入文件帧低于输出fps设置,则按输入帧发送数据*/
	/************************************************************************/

	if(video_time >= output_video_time || is->video_put_count == 0)
	{
		ms_mutex_lock(&is->video_outq.lock);
		ms_queue_put(&is->video_outq.q,get_as_yuvmsg(is,pFrame));
		ms_mutex_unlock(&is->video_outq.lock);

		is->video_put_count++;
	}else
	{
		/*skip the frame data!*/
	}
	ms_mutex_unlock(&is->time_sync_mutex);

	return 0;
}


void *video_decoder_run(void *arg){
	VideoState *is = (VideoState *)arg;
	AVPacket pkt1, *packet = &pkt1;
	int len1, frameFinished;
	AVFrame *pFrame;
	double pts = 0.0;
	int precision=2;
	precision = set_high_prio();


	pFrame = avcodec_alloc_frame();

	while(is->run){

		if((ms_queue_get_size(&is->video_outq.q) >= MIN_FRAMES && is->is_rtmp ==FALSE)){
			sleepMs(10);
			continue;
		}

		if(packet_queue_get(&is->videoq, packet, 1) < 0) {
			// means we quit getting packets
			break;
		}

		if(packet->data == flush_pkt.data) {
			avcodec_flush_buffers(is->video_st->codec);
			continue;
		}
		// Decode video frame
		len1 = avcodec_decode_video(is->video_st->codec, pFrame, &frameFinished, 
			packet->data, packet->size);

		if(frameFinished) {
			frame_to_picture(is, pFrame, packet->pts);
		}

		av_free_packet(packet);
	}

	ms_mutex_unlock(&is->video_outq.lock);
	av_free(pFrame);

	unset_high_prio(precision);
	ms_thread_exit(NULL);
	return NULL;
}

int stream_component_open(VideoState *is, int stream_index) {

	AVFormatContext *pFormatCtx = is->pFormatCtx;
	AVCodecContext *codecCtx =  pFormatCtx->streams[stream_index]->codec;
	AVCodec *codec;

	if(stream_index < 0 || stream_index >= pFormatCtx->nb_streams) {
		return -1;
	}

	/* prepare audio output */
	if (codecCtx->codec_type == CODEC_TYPE_AUDIO) {
		if (codecCtx->channels > 0) {
			codecCtx->request_channels = FFMIN(2, codecCtx->channels); //设置只请求2声道以下数据
		} else {
			codecCtx->request_channels = 2;
		}

		is->rate = codecCtx->sample_rate;
		is->nchannels = codecCtx->request_channels;
		is->audio_src_fmt= SAMPLE_FMT_S16;
		ms_message("Audio stream: sample rate=%d, channels=%d ",is->rate,is->nchannels);
	}

	// Get a pointer to the codec context for the video stream
	codecCtx = pFormatCtx->streams[stream_index]->codec;

	codec = avcodec_find_decoder(codecCtx->codec_id);
	if(!codec || (avcodec_open(codecCtx, codec) < 0)) {
		ms_error("Unsupported codec!\n");
		return -1;
	}

	switch(codecCtx->codec_type) {
  case CODEC_TYPE_AUDIO:
	  is->audioStream = stream_index;
	  is->audio_st = pFormatCtx->streams[stream_index];
	  is->audio_buf_size = 0;
	  is->audio_buf_index = 0;

	  /* Correct audio only if larger error than this */

	  memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));

	  is->audioq.abort_request = FALSE;

	  break;
  case CODEC_TYPE_VIDEO:
	  is->videoStream = stream_index;
	  is->video_st = pFormatCtx->streams[stream_index];


	  if (is->video_st->r_frame_rate.den && is->video_st->r_frame_rate.num)
	  {
		  if(is->video_st->r_frame_rate.num == 1000)
			 is->fps = 15.0;
		  else
			  is->fps = av_q2d(is->video_st->r_frame_rate);

	  }
	  else
		  is->fps = 1/av_q2d(codecCtx->time_base);

	  is->vsize.width = is->video_st->codec->width;
	  is->vsize.height = is->video_st->codec->height;

	  ms_message("Video stream: fps=%3.2f, size: %dx%d",is->fps,is->vsize.width,is->vsize.height);

	  is->videoq.abort_request = FALSE;

	  break;
  default:
	  break;
	}


	return 0;
}

void file_stream_seek_frame(VideoState *is,AVFormatContext *pFormatCtx )
{
	int ret,eof;

	if(is->seek_req) {

		int64_t seek_target= is->seek_pos;
		int64_t seek_min= is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
		int64_t seek_max= is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;

		ret = avformat_seek_file(is->pFormatCtx, -1, seek_min, seek_target, seek_max, is->seek_flags);
		if (ret < 0) {
			fprintf(stderr, "%s: error while seeking\n", is->filename);
		}else{
			if (is->audioStream >= 0) {
				packet_queue_flush(&is->audioq);
				packet_queue_put(&is->audioq, &flush_pkt);
			}

			if (is->videoStream >= 0) {
				packet_queue_flush(&is->videoq);
				packet_queue_put(&is->videoq, &flush_pkt);
			}
		}
		is->seek_req = 0;
		eof= 0;
		is->audio_put_count = 0;
		is->video_put_count = 0;
		is->video_get_count = 0;
	}
}

static void flush_packet_queue(AVFormatContext *s)
{
	AVPacketList *pktl;

	for(;;) {
		pktl = s->packet_buffer;
		if (!pktl)
			break;
		s->packet_buffer = pktl->next;
		av_free_packet(&pktl->pkt);
		av_free(pktl);
	}
	while(s->raw_packet_buffer){
		pktl = s->raw_packet_buffer;
		s->raw_packet_buffer = pktl->next;
		av_free_packet(&pktl->pkt);
		av_free(pktl);
	}
	s->packet_buffer_end=
		s->raw_packet_buffer_end= NULL;
	s->raw_packet_buffer_remaining_size = RAW_PACKET_BUFFER_SIZE;
}

void *packet_read_run(void *arg) {

	VideoState *is = (VideoState *)arg;
	AVPacket pkt1, *packet = &pkt1;
	int precision=2;
	int ret;
	int eof=0;


	if(is->audioStream!=-1)
		ms_thread_create(&is->audio_decoder_thread,NULL,&audio_decoder_run,is);

	if(is->videoStream!=-1)
		ms_thread_create(&is->video_decoder_thread,NULL,&video_decoder_run,is);

	precision = set_high_prio();

	ms_mutex_lock(&is->packet_read_mutex);
	
	//if(is->is_rtmp) flush_packet_queue(is->pFormatCtx);

	while(is->run)
	{
		ms_mutex_unlock(&is->packet_read_mutex);

		//调整播放进度	
		if(!is->is_rtmp) file_stream_seek_frame(is,is->pFormatCtx);

		/* if the queue are full, no need to read more */
		if ( is->audioq.size + is->videoq.size > MAX_QUEUE_SIZE
			|| ((is->audioq.size  > MIN_AUDIOQ_SIZE || is->audio_st<0) && (is->videoq.nb_packets > MIN_FRAMES || is->video_st<0))){
				sleepMs(10);
				continue;
		}

		if(is->is_rtmp)
			ret = av_read_packet(is->pFormatCtx, packet);
		else
			ret = av_read_frame(is->pFormatCtx, packet);

		if (ret < 0) {
			if (ret == AVERROR_EOF)
				eof=1;
			if (url_ferror(is->pFormatCtx->pb))
				break;
			sleepMs(100); /* wait for user event */
			continue;
		}

		// Is this a packet from the video stream?
		if(packet->stream_index == is->videoStream) {
			packet_queue_put(&is->videoq, packet);
		} else if(packet->stream_index == is->audioStream) {
			packet_queue_put(&is->audioq, packet);
		} else {
			av_free_packet(packet);
		}

		ms_mutex_lock(&is->packet_read_mutex);
	}

	ms_mutex_unlock(&is->packet_read_mutex);

fail:
	unset_high_prio(precision);

	is->audioq.abort_request=TRUE;
	packet_queue_flush(&is->audioq);
	is->videoq.abort_request=TRUE;
	packet_queue_flush(&is->videoq);
	if(is->video_decoder_thread) ms_thread_join(is->video_decoder_thread,NULL);
	if(is->audio_decoder_thread) ms_thread_join(is->audio_decoder_thread,NULL);

	ms_thread_exit(NULL);
	return NULL;
}

VideoState * file_stream_new()
{
	VideoState *is = ms_new0(VideoState,1);
	return is;
}

int file_stream_init(VideoState *is){

	ffmpeg_check_init();

	ms_mutex_init(&is->packet_read_mutex,NULL);
	ms_mutex_init(&is->time_sync_mutex,NULL);

	packet_queue_init(&is->audioq);
	packet_queue_init(&is->videoq);

	is->audio_buf_index = 0;
	is->audio_buf_size = 0;
	is->audio_clock = 0;
	is->rate = 32000;
	is->nchannels = 2;
	is->fps = 15.00;
	is->vsize.width = MS_VIDEO_SIZE_VGA_W;
	is->vsize.height = MS_VIDEO_SIZE_VGA_H;
	is->audio_decoder_thread = NULL;
	is->packet_read_thread = NULL;
	is->audio_decoder_thread = NULL;
	is->video_decoder_thread = NULL;
	is->bufferizer=ms_bufferizer_new();

	conf_slot_queue_init(&is->audio_outq);
	conf_slot_queue_init(&is->video_outq);
	return 0;
}

bool_t file_stream_is_playling(VideoState *is)
{
	return is->run;
}

int file_stream_open(VideoState *is, const char *filename) {
	if(filename==NULL || strlen(filename)<=0) return -1;

	strcpy(is->filename, filename);

	AVFormatContext *pFormatCtx;

	int video_index = -1;
	int audio_index = -1;
	int i;

	is->videoStream=-1;
	is->audioStream=-1;

	is->audio_outq.run = FALSE;
	is->video_outq.run = FALSE;

	is->output_nchannels = is->audio_outq.nchannels;
	is->output_rate = is->audio_outq.rate;

	is->output_vsize = is->video_outq.vsize;
	is->output_fps = is->video_outq.fps;

	// will interrupt blocking functions if we quit!
	url_set_interrupt_cb(decode_interrupt_cb);

	// Open video file
	if(av_open_input_file(&pFormatCtx, is->filename, NULL, 0, NULL)!=0)
		return -1; // Couldn't open file

	is->is_rtmp = pFormatCtx->pb->is_streamed;

	is->pFormatCtx = pFormatCtx;

	// Retrieve stream information
	int res = av_find_stream_info(pFormatCtx);
	if(res<0)
		return -2; // Couldn't find stream information

	// Dump information about file onto standard error
	dump_format(pFormatCtx, 0, is->filename, 0);

	// Find the first video stream

	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if(pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO &&
			video_index < 0) {
				video_index=i;
		}
		if(pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_AUDIO &&
			audio_index < 0) {
				audio_index=i;
		}
	}
	if(audio_index >= 0) {
		stream_component_open(is, audio_index);
	}
	if(video_index >= 0) {
		stream_component_open(is, video_index);
	}   

	if(is->videoStream < 0 && is->audioStream < 0) {
		ms_error("%s: could not open codecs\n", is->filename);
		return -3;
	}


	if(is->rate!=is->output_rate)
	{
		if (is->resample_handle!=NULL){
			unsigned int inrate=0, outrate=0;
			speex_resampler_get_rate(is->resample_handle,&inrate,&outrate);
			if (inrate!=is->rate || outrate!=is->output_rate){
				speex_resampler_destroy(is->resample_handle);
				is->resample_handle=0;
			}
		}
		if (is->resample_handle==NULL){
			int err=0;
			is->resample_handle=speex_resampler_init(is->output_nchannels, is->rate, is->output_rate, SPEEX_RESAMPLER_QUALITY_DESKTOP, &err);
		}
	}

	return 0;
}

int file_stream_start(VideoState *is)
{
	is->run = TRUE;
	is->audio_put_count = 0;
	is->video_put_count = 0;
	is->video_get_count = 0;
	ms_thread_create(&is->packet_read_thread,NULL,&packet_read_run,is);

	if(!is->packet_read_thread) {
		av_free(is);
		return -1;
	}

	return 0;
}



static void stream_component_close(VideoState *is, int stream_index)
{
	AVFormatContext *ic = is->pFormatCtx;
	AVCodecContext *avctx;

	if (stream_index < 0 || stream_index >= ic->nb_streams)
		return;
	avctx = ic->streams[stream_index]->codec;

	switch(avctx->codec_type) {
	case CODEC_TYPE_AUDIO:
		if (is->reformat_ctx)
			av_audio_convert_free(is->reformat_ctx);
		is->reformat_ctx = NULL;
		break;
	case CODEC_TYPE_VIDEO:

		break;
	case CODEC_TYPE_SUBTITLE:

		break;
	default:
		break;
	}

	ic->streams[stream_index]->discard = AVDISCARD_ALL;
	avcodec_close(avctx);
	switch(avctx->codec_type) {
	case CODEC_TYPE_AUDIO:
		is->audio_st = NULL;
		is->audioStream = -1;
		break;
	case CODEC_TYPE_VIDEO:
		is->video_st = NULL;
		is->videoStream = -1;
		break;
	default:
		break;
	}
}

int file_stream_stop(VideoState *is)
{
	if(!is->run) return -1;

	is->output_date = FALSE;
	is->audioq.abort_request = TRUE;
	is->videoq.abort_request = TRUE;

	ms_mutex_lock(&is->packet_read_mutex);
	is->run=FALSE;
	ms_mutex_unlock(&is->packet_read_mutex);

	ms_thread_join(is->packet_read_thread,NULL);

	ms_queue_flush(&is->audio_outq.q);
	ms_queue_flush(&is->video_outq.q);

	ms_bufferizer_flush(is->bufferizer);

	/* close each stream */
	if (is->audioStream >= 0)
		stream_component_close(is, is->audioStream);
	if (is->videoStream >= 0)
		stream_component_close(is, is->videoStream);

	if (is->pFormatCtx) {
		av_close_input_file(is->pFormatCtx);
		is->pFormatCtx = NULL; /* safety */
	}

	if (is->resample_handle!=NULL){
		speex_resampler_destroy(is->resample_handle);
		is->resample_handle=NULL;
	}

	if(is->yuv_msg)
	{
		freemsg(is->yuv_msg);
		is->yuv_msg=NULL;
	}

	if (is->sws_ctx!=NULL){
		ms_sws_freeContext(is->sws_ctx);
		is->sws_ctx=NULL;
	}

	conf_slot_queue_flush(&is->audio_outq);
	conf_slot_queue_flush(&is->video_outq);

	url_set_interrupt_cb(NULL);

	return 0;
}

void file_stream_destory(VideoState *is)
{
	ms_mutex_destroy(&is->packet_read_mutex);
	ms_mutex_destroy(&is->time_sync_mutex);

	conf_slot_queue_destory(&is->audio_outq);
	conf_slot_queue_destory(&is->video_outq);

	ms_bufferizer_destroy(is->bufferizer);
	ms_free(is);
}

extern MSFilterDesc filstream_audio_player_desc;
extern MSFilterDesc filstream_video_player_desc;

MSFilter *file_stream_create_audio_filter(VideoState *is)
{	
	if(is->audioStream >= 0)
	{
		MSFilter *f = ms_filter_new_from_desc(&filstream_audio_player_desc);
		StreamData *d=(StreamData*)f->data;
		d->rate = is->rate;
		d->nchannels = is->nchannels;
		d->is = is;
		return f;
	}
	return NULL;
}

MSFilter *file_stream_create_video_filter(VideoState *is)
{
	if(is->videoStream >= 0)
	{
		MSFilter *f = ms_filter_new_from_desc(&filstream_video_player_desc);
		StreamData *d=(StreamData*)f->data;
		d->is = is;
		d->fps = is->fps;
		d->vsize = is->vsize;
		return f;
	}
	return NULL;
}

//用于Filter连接成功后输出数据
int file_stream_output_data(VideoState *is,bool_t enabled)
{
	is->output_date = enabled;
	return 0;
}


VideoPlayer *video_player_new(){
	VideoPlayer *vp = ms_new0(VideoPlayer,1);
	vp->nchannels = 1;
	vp->rate = 8000;
	vp->fps = 15.0;
	vp->vsize.width = MS_VIDEO_SIZE_CIF_W;
	vp->vsize.height = MS_VIDEO_SIZE_CIF_H;
	return vp;
}

void video_player_set_audio_source(VideoPlayer *vp,MSFilter *source){
	//vp->sndread = source;
}

void video_player_set_video_source(VideoPlayer *vp,MSFilter *source){
	//vp->video_source = source;
}

ConfSlotQueue *video_player_get_audio_outq(VideoState *is){
	return &is->audio_outq;
}

ConfSlotQueue *video_player_get_video_outq(VideoState *is){
	return &is->video_outq;
}

static void choose_display_name(VideoPlayer *stream){
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

void video_player_start(VideoPlayer *vp, MSSndCard *snd_playbackcard, MSSndCard *snd_capcard, MSWebCam *cam,unsigned long video_window_id){

	int tmp;
	MSConnectionHelper h;

	//if(vp->sndread!=NULL)
	{
		if (snd_playbackcard!=NULL) vp->sndplayback=ms_snd_card_create_writer(snd_playbackcard);
		if(snd_capcard!=NULL) vp->sndread = ms_snd_card_create_reader(snd_capcard);

		ms_filter_call_method(vp->sndread,MS_FILTER_SET_NCHANNELS,&vp->nchannels);
		ms_filter_call_method(vp->sndread,MS_FILTER_SET_SAMPLE_RATE,&vp->rate);

		if(vp->sndplayback){

			ms_filter_call_method(vp->sndread,MS_FILTER_GET_NCHANNELS,&tmp);

			if(tmp != vp->nchannels)
			{
				vp->chanadapt = ms_filter_new(MS_CHANNEL_ADAPTER_ID);

				ms_filter_call_method(vp->chanadapt,MS_FILTER_SET_NCHANNELS,&tmp);
				ms_filter_call_method(vp->chanadapt,MS_CHANNEL_ADAPTER_SET_OUTPUT_NCHANNELS,&vp->nchannels);

				ms_message("Video Player: chanadapt %d to %d",tmp,vp->nchannels);
			}

			ms_filter_call_method(vp->sndread,MS_FILTER_GET_SAMPLE_RATE,&tmp);
			//速率变换
			if(tmp != vp->rate)
			{
				vp->resampler = ms_filter_new(MS_RESAMPLE_ID);

				ms_filter_call_method(vp->resampler,MS_FILTER_SET_SAMPLE_RATE,&tmp);
				ms_filter_call_method(vp->resampler,MS_FILTER_SET_NCHANNELS,&vp->nchannels);
				ms_filter_call_method(vp->resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&vp->rate);

				ms_message("Video Player: resample %dHZ to %dHZ",tmp,vp->rate);
			}

			ms_filter_call_method(vp->sndplayback,MS_FILTER_SET_SAMPLE_RATE,&vp->rate);
			ms_filter_call_method(vp->sndplayback,MS_FILTER_SET_NCHANNELS, &vp->nchannels);

		}
	}

	//if(vp->video_source!=NULL)
	{

#ifndef TARGET_OS_IPHONE
		choose_display_name(vp);
		vp->video_output=ms_filter_new_from_name (vp->display_name);
#else
        vp->video_output=ms_filter_new(MS_EXT_DISPLAY_ID);
		 ms_filter_set_notify_callback (vp->video_output,ext_display_cb,vp);
#endif //endif TARGET_OS_IPHONE
      

		if(cam) vp->video_source = ms_web_cam_create_reader(cam);

		ms_filter_call_method(vp->video_source,MS_FILTER_SET_FPS, &vp->fps);
		ms_filter_call_method(vp->video_source,MS_FILTER_SET_VIDEO_SIZE, &vp->vsize);
        

		if (video_window_id!=0)
			ms_filter_call_method(vp->video_output, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&video_window_id);
        
        ms_filter_call_method(vp->video_output,MS_FILTER_SET_VIDEO_SIZE, &vp->vsize);
	}

	if(vp->sndread!=NULL && vp->sndplayback!=NULL){

		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h,vp->sndread,-1,0);

		if(vp->chanadapt!=NULL)
			ms_connection_helper_link(&h,vp->chanadapt,0,0);

		if(vp->resampler!=NULL)
			ms_connection_helper_link(&h,vp->resampler,0,0);

		ms_connection_helper_link(&h,vp->sndplayback,0,-1);
	}

	if(vp->video_source!=NULL && vp->video_output!=NULL)
		ms_filter_link(vp->video_source,0,vp->video_output,0);

	vp->ticker = ms_ticker_new();
	ms_ticker_set_name(vp->ticker,"VideoPlayer");

	if(vp->sndread!=NULL && vp->sndplayback!=NULL)
		ms_ticker_attach(vp->ticker,vp->sndread);

	if(vp->video_source!=NULL && vp->video_output!=NULL)
		ms_ticker_attach(vp->ticker,vp->video_source);

}


static void video_player_free(VideoPlayer *vp)
{
	if(vp->display_name!=NULL) ms_free(vp->display_name);

	if(vp->ticker!=NULL) ms_ticker_destroy(vp->ticker);
	if(vp->sndread!=NULL) ms_filter_destroy(vp->sndread);
	if(vp->chanadapt!=NULL) ms_filter_destroy(vp->chanadapt);
	if(vp->resampler!=NULL) ms_filter_destroy(vp->resampler);
	if(vp->sndplayback!=NULL) ms_filter_destroy(vp->sndplayback);
	if(vp->video_source!=NULL) ms_filter_destroy(vp->video_source);
	if(vp->video_output!=NULL) ms_filter_destroy(vp->video_output);

	ms_free(vp);
}

void video_player_stop(VideoPlayer *vp)
{
	MSConnectionHelper h;

	if(vp->ticker)
	{
		if(vp->sndread!=NULL && vp->sndplayback!=NULL){
			ms_ticker_detach(vp->ticker,vp->sndread);

			ms_connection_helper_start(&h);
			ms_connection_helper_unlink(&h,vp->sndread,-1,0);

			if(vp->chanadapt!=NULL)
				ms_connection_helper_unlink(&h,vp->chanadapt,0,0);

			if(vp->resampler!=NULL)
				ms_connection_helper_unlink(&h,vp->resampler,0,0);

			ms_connection_helper_unlink(&h,vp->sndplayback,0,-1);
		}

		if(vp->video_source!=NULL && vp->video_output!=NULL){
			ms_ticker_detach(vp->ticker,vp->video_source);
			ms_filter_unlink(vp->video_source,0,vp->video_output,0);
		}
	}

	video_player_free(vp);
}

int file_stream_audio_get_rate(VideoState *is){
	return is->rate;
}

int file_stream_audio_get_nchannels(VideoState *is){
	return is->nchannels;
}

MSVideoSize file_stream_video_get_vsize(VideoState *is){
	return is->vsize;
}

float file_stream_video_get_fps(VideoState *is){
	return is->fps;
}

void video_player_set_rate(VideoPlayer *vp ,int rate){
	vp->rate = rate;
}
void video_player_set_nchannels(VideoPlayer *vp,int nchannels){
	vp->nchannels = nchannels;
}

void video_player_set_fps(VideoPlayer *vp,float fps){
	vp->fps = fps;
}

void video_player_set_video_size(VideoPlayer *vp, MSVideoSize size){
	vp->vsize = size;
}

/* seek in the stream */
static void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes)
{
	if (!is->seek_req) {
		is->seek_pos = pos;
		is->seek_rel = rel;
		is->seek_flags &= ~AVSEEK_FLAG_BYTE;
		if (seek_by_bytes)
			is->seek_flags |= AVSEEK_FLAG_BYTE;
		is->seek_req = 1;
	}
}

int file_stream_do_seek(VideoState *is,double to_pos){
	double incr=0.0, video_pos=0.0,frac;
	if(is) {
		int64_t ts;
		int ns, hh, mm, ss;
		int tns, thh, tmm, tss;

		frac = ((double)to_pos)/100;

		tns = is->pFormatCtx->duration/1000000LL;
		thh = tns/3600;
		tmm = (tns%3600)/60;
		tss = (tns%60);
		ns = frac*tns;
		hh = ns/3600;
		mm = (ns%3600)/60;
		ss = (ns%60);
		ms_message("Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d)", frac*100,
			hh, mm, ss, thh, tmm, tss);
		ts = frac*is->pFormatCtx->duration;
		if (is->pFormatCtx->start_time != AV_NOPTS_VALUE)
			ts += is->pFormatCtx->start_time;
		stream_seek(is, ts, 0, 0);
	}
	return -1;
}

/*
static void file_player_cam_init(MSWebCam *cam){
VideoState *vstate = (VideoState *)cam->data;
if(vstate)
cam->name=ms_strdup_printf("Video Player Souce (play %s)",vstate->filename);
else
cam->name=ms_strdup("Video Player Source");
}*/

static MSFilter * file_player_create_reader(MSWebCam *cam){
	MSFilter *f =  ms_filter_new_from_desc(&filstream_video_player_desc);
	VideoState *vstate = (VideoState *)cam->data;
	if(vstate)
	{  //设置播放器参数
		StreamData *d=(StreamData*)f->data;
		d->is = vstate;
		d->fps = vstate->fps;
		d->vsize = vstate->vsize;
	}

	return f;
}

MSWebCamDesc file_player_cam_desc={
	"File Player Cam",
	NULL,
	NULL,
	&file_player_create_reader,
	NULL
};


MSWebCam *ms_file_player_cam_new(VideoState *is)
{
	MSWebCam *cam =ms_web_cam_new(&file_player_cam_desc);
	cam->data = is;
	return cam;
}

static MSFilter *create_reader(MSSndCard *c){
	MSFilter *f =  ms_filter_new_from_desc(&filstream_audio_player_desc);
	VideoState *is = (VideoState *)c->data;
	if(is)
	{  //设置播放器参数
		StreamData *d=(StreamData*)f->data;
		d->is = is;
	}
	return f;
}

static MSSndCardDesc file_player_snd_desc={
	"File Player SND",
	/*detect*/ NULL,
	/*init*/ NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	create_reader,
	/*create_writer*/NULL,
	/*uninit,*/
};

MSSndCard *ms_file_player_snd_new(VideoState *is)
{
	MSSndCard *snd =ms_snd_card_new(&file_player_snd_desc);
	snd->data = is;
	snd->capabilities = MS_SND_CARD_CAP_CAPTURE;
	return snd;
}
