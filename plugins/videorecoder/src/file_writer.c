#include "file_writer.h"

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

#define NB_SAMPLES 10
static int sws_flags = SWS_BICUBIC;
static bool_t ffmpeg_initialized=FALSE;


#ifdef _DEBUG
#include <stdio.h>
extern "C"{
	FILE *netstackdump = 0;
	FILE *netstackdump_read = 0;
};
#endif // _DEBUG

#define PROFILE_H264_BASELINE 66
#define PROFILE_H264_MAIN 66

struct _VideoRecoder{
	int16_t *samples;
	MSQueue video_queue;
	MSBufferizer *bufferizer;
	uint8_t *audio_outbuf;
	int audio_outbuf_size;
	int audio_input_frame_size;
	int rate;
	int nchannels;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVStream *audio_st, *video_st;
	double audio_pts, video_pts;
	char filename[1024];
	bool_t recording;
	MSVideoSize vsize;
	float fps;
	int audio_bit_rate;
	int video_bit_rate;
	/* video output */
	AVFrame *picture, *tmp_picture;
	uint8_t *video_outbuf;
	int frame_count, video_outbuf_size;

	ConfSlotQueue audio_inq; /*音频输出队列*/
	ConfSlotQueue video_inq; /*视频输出队列*/

	ms_mutex_t mutex;
	ms_cond_t cond;
	ms_thread_t thread_id;
	bool_t run;
	bool_t start_recroding;

	double audio_write_pts;
	double video_write_pts;
	int nb_frames_dup;
	int nb_frames_drop;

	//尺寸变换
	struct ms_SwsContext *sws_ctx;
	MSVideoSize in_vsize;
	mblk_t *om;
	YuvBuf outbuf;

	bool_t is_rtmp;
};

static void ffmpeg_check_init(){
	if(!ffmpeg_initialized){
		avcodec_init();
		avcodec_register_all();
		av_register_all();
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


VideoRecoder * video_recoder_new()
{
	VideoRecoder *os = ms_new0(VideoRecoder,1);
	return os;
}

int video_recoder_init(VideoRecoder *os)
{
	os->video_st = NULL;
	os->audio_st = NULL;
	os->audio_bit_rate = 64000;
	os->nchannels = 2;
	os->rate = 8000;
	os->fps = 15.0;
	os->video_bit_rate = 512000;
	os->vsize.width = MS_VIDEO_SIZE_CIF_W;
	os->vsize.height = MS_VIDEO_SIZE_CIF_H;
	os->recording = FALSE;
	os->audio_pts = 0;
	os->audio_st = 0;
	os->fmt = NULL;
	os->oc = NULL;
	os->samples = NULL;
	os->video_pts = 0;
	os->video_st = 0;
	os->bufferizer=ms_bufferizer_new();
	ms_queue_init(&os->video_queue);
	
	ms_mutex_init(&os->mutex,NULL);
	
	conf_slot_queue_init(&os->audio_inq);

	conf_slot_queue_init(&os->video_inq);

	ms_cond_init(&os->cond,NULL);
	ffmpeg_check_init();
	return 0;
}

#define RC_MARGIN 10000

/* add a video output stream */
static AVStream *add_video_stream(AVFormatContext *oc, enum CodecID codec_id, int bit_rate,float fps, MSVideoSize *vsize)
{
	AVCodecContext *c;
	AVStream *st;

	st = av_new_stream(oc, 0);
	if (!st) {
		ms_error("Could not alloc stream");
		return NULL;
	}

	c = st->codec;
	c->codec_id = codec_id;
	c->codec_type = CODEC_TYPE_VIDEO;

	/* put sample parameters */
	c->bit_rate = bit_rate;
	/* resolution must be a multiple of two */
	c->width = vsize->width;
	c->height = vsize->height;
	/* time base: this is the fundamental unit of time (in seconds) in terms
	of which frame timestamps are represented. for fixed-fps content,
	timebase should be 1/framerate and timestamp increments should be
	identically 1. */
	c->time_base.den = (int)fps;
	c->time_base.num = 1;
	c->gop_size = (int)fps; /* emit one intra frame every twelve frames at most */
	c->pix_fmt = PIX_FMT_YUV420P;
	if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
		/* just for testing, we also add B frames */
		c->max_b_frames = 2;
	}
	if (c->codec_id == CODEC_ID_MPEG1VIDEO){
		/* Needed to avoid using macroblocks in which some coeffs overflow.
		This does not happen with normal video, it just happens here as
		the motion of the chroma plane does not match the luma plane. */
		c->mb_decision=2;
	}
	// some formats want stream headers to be separate
	if(oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	if(c->codec_id ==CODEC_ID_H264)
	{
#ifdef WIN32
		c->dsp_mask = (FF_MM_MMX | FF_MM_MMXEXT | FF_MM_SSE |FF_MM_SSE2 |FF_MM_SSE3);
#endif

//最优画质
/*
		c->pix_fmt		= PIX_FMT_YUV420P;
		c->time_base.num  = 1;
		c->time_base.den  = fps;
		c->width = vsize->width;
		c->height = vsize->height;
		c->rc_max_rate = bit_rate * 0.8f;
		c->rc_min_rate = bit_rate * 0.2f;

		c->rc_lookahead = 0;

		//c->refs = 1;
		c->scenechange_threshold = 0;
		c->me_subpel_quality = 0;
		c->partitions = X264_PART_I4X4 | X264_PART_I8X8 | X264_PART_P8X8 | X264_PART_B8X8;
		c->me_method = ME_EPZS;
		c->trellis = 0;


		c->me_range = 16;
		c->max_qdiff = 4;
		c->mb_qmin = c->qmin = 10;
		c->mb_qmax = c->qmax = 51;
		c->qcompress = 0.6f;
		c->mb_decision = FF_MB_DECISION_SIMPLE;
		c->flags2 |= CODEC_FLAG2_FASTPSKIP;
		c->flags |= CODEC_FLAG_LOOP_FILTER;
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
		c->max_b_frames = 0;
		c->b_frame_strategy = 1;
		c->chromaoffset = 0;

		
	//	c->profile = FF_PROFILE_H264_BASELINE;
	//	c->level = 10;

		c->thread_count = 0;
		c->rtp_payload_size = 1460;
		c->opaque = NULL;
#ifdef ENABLE_X264_VBR
		c->bit_rate = (int) (bit_rate * 0.80f);
		c->bit_rate_tolerance = (int) (bit_rate * 0.20f);
		c->crf = 0;
		c->cqp = 0;
#else
	c->crf = 22;
#endif // ENABLE_X264_VBR
		c->gop_size = fps*4; // Each 4 second(s)
		c->bit_rate_tolerance = 0.1;
*/

//普通画质

//pjmedia

		/*平均码率较低，CPU使用率极低，延迟低 首选设置*/
		c->profile = PROFILE_H264_BASELINE;
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
		c->coder_type = FF_CODER_TYPE_VLC;
		c->max_b_frames = 0;
		c->flags2 &= ~(CODEC_FLAG2_WPRED | CODEC_FLAG2_8X8DCT);
		c->weighted_p_pred = 0;

		c->level = 13;

		c->me_range = 16;
		c->max_qdiff = 4;
		c->qmin = 20;
		c->qmax = 32;
		c->qcompress = 0.6f;


		// msx264 参数 /
		c->bit_rate_tolerance = 0.1;
		c->rc_max_rate = (int) ((bit_rate+RC_MARGIN/2)/1000);
		c->rc_min_rate = bit_rate * 0.2f;

		c->rc_initial_buffer_occupancy = 0.5;
		

		c->gop_size = 3 * (int)fps; //10秒 一个关键帧


		c->max_b_frames = 0;

		c->bit_rate_tolerance = 0.1;

		//优化参数 fastest preset CPU 使用率低
		c->me_range = 16;
		c->me_subpel_quality = 1;
		c->b_quant_factor = 1.5;
		c->bframebias = 1;
		c->me_method = 8;
		c->directpred = 1;

//enf for pjmedia h264 

		/*画质良好,CPU使用量较低*/
/*		c->profile = PROFILE_H264_BASELINE;

		c->coder_type = FF_CODER_TYPE_VLC;
		c->max_b_frames = 0;
		c->flags2 &= ~(CODEC_FLAG2_WPRED | CODEC_FLAG2_8X8DCT);
		c->weighted_p_pred = 0;

		c->level = 13;

		c->me_range = 16;
		c->max_qdiff = 4;
		c->mb_qmin = c->qmin = 20;
		c->mb_qmax = c->qmax = 32;
		c->qcompress = 0.6f;
*/
//ABR 控制 
		/*平均码率在30KB 左右, 运动时骤增*/
/*		c->partitions = X264_PART_I4X4 | X264_PART_I8X8 | X264_PART_P8X8 | X264_PART_B8X8;

		c->me_range = 16;
		c->keyint_min =25;
		c->i_quant_factor = 0.71;
		c->qcompress = 0.6;

		c->bit_rate = bit_rate;
		c->rc_min_rate =bit_rate;
		c->rc_max_rate = bit_rate;  
		c->bit_rate_tolerance = bit_rate;
		c->rc_buffer_size=bit_rate;
		c->rc_initial_buffer_occupancy = c->rc_buffer_size*3/4;
		c->rc_buffer_aggressivity= (float)1.0;
		c->rc_initial_cplx= 0.5; 
		c->gop_size = 3 * fps;
*/

		/*画质好 CPU 使用率较高*/
/*		c->partitions = X264_PART_I4X4 | X264_PART_I8X8 | X264_PART_P8X8 | X264_PART_B8X8;
		c->flags = CODEC_FLAG_LOOP_FILTER | CODEC_FLAG_GLOBAL_HEADER;
		c->flags2 = CODEC_FLAG_PASS1;
		c->me_method = ME_UMH;

		c->me_subpel_quality = 5;
		c->trellis = 1;
		c->refs = 1;
		c->coder_type = FF_CODER_TYPE_VLC;
		c->me_range = 16;
		c->keyint_min =25;
		c->scenechange_threshold = 40;
		c->i_quant_factor = 0.71;
		c->bit_rate_tolerance = fps>1?(float)c->bit_rate/(fps-1):c->bit_rate;
		c->qcompress = 0.6;
		c->qmin = 10;
		c->qmin = 51;
		c->max_qdiff = 4;
		c->level = 30;
		c->gop_size = 30;

		c->rc_min_rate = bit_rate * 0.1;
		c->rc_max_rate = bit_rate * 0.8;

		c->rc_buffer_size = c->bit_rate;

		c->crf = 0;

		c->rc_eq = "blurCplx^(1-qComp)";

		oc->mux_rate = bit_rate;
*/

		c->rc_lookahead = 0; //将编码延迟降低到最小
	}

	if(!strcmp(oc->oformat->name, "mp4") || !strcmp(oc->oformat->name, "mov") || !strcmp(oc->oformat->name, "3gp")) 
		c->flags |= CODEC_FLAG_GLOBAL_HEADER; 

	
	return st;
}

/*
* add an audio output stream
*/
static AVStream *add_audio_stream(AVFormatContext *oc, enum CodecID codec_id,int bit_rate, int rate, int nchannels)
{
	AVCodecContext *c;
	AVStream *st;

	st = av_new_stream(oc, 1);
	if (!st) {
		ms_error("Could not alloc stream");
		return NULL;
	}

	c = st->codec;
	c->codec_id = codec_id;
	c->codec_type = CODEC_TYPE_AUDIO;

	/* put sample parameters */
	c->sample_fmt = SAMPLE_FMT_S16;
	c->bit_rate = bit_rate;
	c->sample_rate = rate;
	c->channels = 2;
	c->time_base.den = rate;
	c->time_base.num = 1;

	// some formats want stream headers to be separate
	if(oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	if(c->codec_id==CODEC_ID_AAC)
	{
		c->rc_lookahead = 0;
	}

	return st;
}

static AVFrame *alloc_picture(VideoRecoder *os, enum PixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	uint8_t *picture_buf;
	int size;

	picture = avcodec_alloc_frame();
	if (!picture)
		return NULL;
	size = avpicture_get_size(pix_fmt, width, height);
	picture_buf = (uint8_t*)av_malloc(size);
	if (!picture_buf) {
		av_free(picture);
		return NULL;
	}
	avpicture_fill((AVPicture *)picture, picture_buf,
		pix_fmt, width, height);
	return picture;
}

static int open_video(VideoRecoder *os)
{
	AVFormatContext *oc = os->oc;
	AVStream *st = os->video_st;
	AVCodec *codec;
	AVCodecContext *c;

	c = st->codec;

	/* find the video encoder */
	codec = avcodec_find_encoder(c->codec_id);
	if (!codec) {
		ms_error("open_video: codec not found");
		return -1;
	}

	/* open the codec */
	if (avcodec_open(c, codec) < 0) {
		ms_error("open_video: could not open codec");
		return -2;
	}	

	if(os->video_outbuf)
	{
		av_free(os->video_outbuf);
		os->video_outbuf =NULL;
	}

	if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) {
		/* allocate output buffer */
		/* XXX: API change will be done */
		/* buffers passed into lav* can be allocated any way you prefer,
		as long as they're aligned enough for the architecture, and
		they're freed appropriately (such as using av_free for buffers
		allocated with av_malloc) */
		int size= os->vsize.width * os->vsize.height;
		os->video_outbuf_size = FFMAX(2000000, 6*size + 1664);;
		os->video_outbuf = (uint8_t*)av_malloc(os->video_outbuf_size);
	}

	/* allocate the encoded raw picture */
	os->picture = alloc_picture(os, c->pix_fmt, c->width, c->height);
	if (!os->picture) {
		ms_error("open_video: Could not allocate picture");
		return  -3;
	}

	/* if the output format is not YUV420P, then a temporary YUV420P
	picture is needed too. It is then converted to the required
	output format */
	os->tmp_picture = NULL;
	if (c->pix_fmt != PIX_FMT_YUV420P) {
		os->tmp_picture = alloc_picture(os,PIX_FMT_YUV420P, c->width, c->height);
		if (!os->tmp_picture) {
			ms_error("open_video: Could not allocate temporary picture");
			return -4;
		}
	}


	return 0;
}

static int open_audio(VideoRecoder *os)
{
	AVFormatContext *oc = os->oc;
	AVStream *st = os->audio_st;
	AVCodecContext *c;
	AVCodec *codec;

	c = st->codec;

	/* find the audio encoder */
	/*if(os->is_rtmp)
		codec = avcodec_find_encoder_by_name("libfaac");
	else
		*/codec = avcodec_find_encoder(c->codec_id);

	if (!codec) {
		ms_error("open_audio: codec not found");
		return -1;
	}

	/* open it */
	if (avcodec_open(c, codec) < 0) {
		ms_error("open_audio: could not open codec");
		return -2;
	}


	os->audio_outbuf_size = 10000;
	os->audio_outbuf = (uint8_t*)av_malloc(os->audio_outbuf_size);

	/* ugly hack for PCM codecs (will be removed ASAP with new PCM
	support to compute the input frame size in samples */
	if (c->frame_size <= 1) {
		os->audio_input_frame_size = os->audio_outbuf_size / c->channels;
		switch(st->codec->codec_id) {
		case CODEC_ID_PCM_S16LE:
		case CODEC_ID_PCM_S16BE:
		case CODEC_ID_PCM_U16LE:
		case CODEC_ID_PCM_U16BE:
			os->audio_input_frame_size >>= 1;
			break;
		default:
			break;
		}
	} else {
		os->audio_input_frame_size = c->frame_size;
	}
	os->samples = (int16_t *)av_malloc(os->audio_input_frame_size * 2 * c->channels);

	return 0;
}

static void close_audio(VideoRecoder *os)
{
	AVFormatContext *oc = os->oc;
	AVStream *st = os->audio_st;
	avcodec_close(st->codec);
	av_free(os->samples);
	av_free(os->audio_outbuf);
}

static void close_video(VideoRecoder *os)
{
	AVFormatContext *oc = os->oc;
	AVStream *st =os->video_st;

	avcodec_close(st->codec);

	av_free(os->picture);

	if (os->tmp_picture) {
		av_free(os->tmp_picture);
	}
	if(os->video_outbuf)
	{
		av_free(os->video_outbuf);
		os->video_outbuf =NULL;
	}
}

void video_recoder_set_vsize(VideoRecoder *os, MSVideoSize vsize)
{
	os->vsize = vsize;
}

void video_recoder_set_rate(VideoRecoder *os, int rate)
{
  os->rate = rate;
}

void video_recoder_set_nbchannels(VideoRecoder *os, int nchannels)
{
	os->nchannels = nchannels;
}

void video_recoder_set_fps(VideoRecoder *os, float fps)
{
	os->fps = fps;
}

void video_recoder_set_audio_bit_rate(VideoRecoder *os, int bitrate)
{
	os->audio_bit_rate = bitrate*1000;
}

void video_recoder_set_video_bit_rate(VideoRecoder *os, int bitrate)
{
	os->video_bit_rate = bitrate*1000;
}
int video_recoder_open_file(VideoRecoder *os, const char *filename)
{
	int ret = -1;

	if(strlen(filename)<=0) return -1;

	os->fmt = av_guess_format(NULL, filename, NULL);

	if (!os->fmt) {
		ms_message("Could not deduce output format from file extension: using FLV.");
		os->fmt = av_guess_format("flv", NULL, NULL);
		os->is_rtmp = TRUE;
	}

	if (!os->fmt) {
		ms_error("Could not find suitable output format");
		return -2;
	}

	/* allocate the output media context */
	os->oc = avformat_alloc_context();
	if (!os->oc) {
		ms_error("Memory error");
		return -3;
	}
	os->oc->oformat = os->fmt;

	strcpy(os->filename, filename);

	if (os->fmt->video_codec != CODEC_ID_NONE) {
		os->fmt->video_codec = CODEC_ID_H264;
		os->video_st = add_video_stream(os->oc, os->fmt->video_codec,os->video_bit_rate,os->fps,&os->vsize);
	}
	if (os->fmt->audio_codec != CODEC_ID_NONE) {
		os->fmt->audio_codec = CODEC_ID_AAC;
		os->audio_st = add_audio_stream(os->oc, os->fmt->audio_codec,os->audio_bit_rate,os->rate,os->nchannels);
	}

	/* set the output parameters (must be done even if no
	parameters). */
	if (av_set_parameters(os->oc, NULL) < 0) {
		ms_error("Invalid output format parameters");
		return -4;
	}

	dump_format(os->oc, 0, os->filename, 1);


	/* now that all the parameters are set, we can open the audio and
	video codecs and allocate the necessary encode buffers */
	if (os->video_st){
		ret = open_video(os);
	}

	if(ret!=0) return -5;

	if (os->audio_st)
		ret = open_audio(os);

	if(ret!=0) return -6;

	/* open the output file, if needed */
	if (!(os->fmt->flags & AVFMT_NOFILE)) {
		if (url_fopen(&os->oc->pb, os->filename, URL_WRONLY) < 0) {
			ms_error("Could not open '%s'\n", os->filename);
			return -7;
		}
	}

	/* write the stream header, if any */
	av_write_header(os->oc);

	return 0;
}

extern mblk_t *mono2stereo(mblk_t *im);

/* prepare a 16 bit dummy audio frame of 'frame_size' samples and
'nb_channels' channels */
static int get_audio_frame(VideoRecoder *os, int16_t *samples, int frame_size){

	int buffer_size = frame_size * 2 * os->nchannels;

	//写入到samples 缓冲区
	if(ms_bufferizer_get_avail(os->bufferizer) >= buffer_size) {
		ms_bufferizer_read(os->bufferizer,(uint8_t*)samples,buffer_size);

		// 时间戳 = 样本长度 / (采样率 * 2字节 *声道数);
		os->audio_write_pts+=(double)(buffer_size)/(double)(os->nchannels*2*os->rate);
		return 0;
	}
	return -1;
}

static int write_audio_frame(VideoRecoder *os)
{
	AVFormatContext *oc = os->oc;
	AVStream *st =os->audio_st;

	AVCodecContext *c;
	AVPacket pkt;
	av_init_packet(&pkt);

	c = st->codec;

	if(get_audio_frame(os,os->samples, os->audio_input_frame_size)<0)
		return -1;

	//写入音频流
	{
		pkt.size= avcodec_encode_audio(c, os->audio_outbuf, os->audio_outbuf_size, os->samples);

		if (c->coded_frame && c->coded_frame->pts != AV_NOPTS_VALUE)
			pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);

		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index= st->index;
		pkt.data= os->audio_outbuf;

		pkt.duration = av_rescale((int64_t)c->frame_size*st->time_base.den,
			st->time_base.num, c->sample_rate);

		/* write the compressed frame in the media file */
		if (av_interleaved_write_frame(oc, &pkt) != 0) {
			ms_error("write_audio_frame: Error while writing audio frame");
		}
	}

	return  0;
}

static mblk_t *size_conv_alloc_mblk(VideoRecoder *s){
	if (s->om!=NULL){
		int ref=s->om->b_datap->db_ref;
		if (ref==1){
			return dupmsg(s->om);
		}else{
			/*the last msg is still referenced by somebody else*/
			ms_message("size_conv_alloc_mblk: Somebody still retaining yuv buffer (ref=%i)",ref);
			freemsg(s->om);
			s->om=NULL;
		}
	}
	s->om=ms_yuv_buf_alloc(&s->outbuf,s->vsize.width,s->vsize.height);
	return dupmsg(s->om);
}

static struct ms_SwsContext * get_resampler(VideoRecoder *s, int w, int h){
	if (s->in_vsize.width!=w ||
		s->in_vsize.height!=h || s->sws_ctx==NULL){
			if (s->sws_ctx!=NULL){
				ms_sws_freeContext(s->sws_ctx);
				s->sws_ctx=NULL;
			}
			s->sws_ctx=ms_sws_getContext(w,h,PIX_FMT_YUV420P,
				s->vsize.width,s->vsize.height,PIX_FMT_YUV420P,
				SWS_FAST_BILINEAR,NULL, NULL, NULL);
		s->in_vsize.width = w;
		s->in_vsize.height = h;
	}
	return s->sws_ctx;
}


static int file_write_encoder_video(VideoRecoder *os, mblk_t *im)
{
	AVFormatContext *oc = os->oc;
	AVStream *st = os->video_st;
	AVCodecContext *c= st->codec;
	int out_size;
	YuvBuf inbuf;
	int ret;
	mblk_t *om=NULL;

	if (ms_yuv_buf_init_from_mblk(&inbuf,im)==0){
		if (inbuf.w==c->width &&
			inbuf.h==c->height){
				//尺寸一致
				avpicture_fill((AVPicture*)os->picture,(uint8_t*)im->b_rptr,c->pix_fmt,c->width,c->height);
		}else{
			struct ms_SwsContext *sws_ctx=get_resampler(os,inbuf.w,inbuf.h);
			om=size_conv_alloc_mblk(os);
			if (ms_sws_scale(sws_ctx,inbuf.planes,inbuf.strides, 0,
				inbuf.h, os->outbuf.planes,os->outbuf.strides)<0){
					ms_error("MSSizeConv: error in ms_sws_scale().");
			}
			avpicture_fill((AVPicture*)os->picture,(uint8_t*)om->b_rptr,c->pix_fmt,c->width,c->height);
		}

	}else {
		return -3;
	}


	if (oc->oformat->flags & AVFMT_RAWPICTURE) {
		/* raw video case. The API will change slightly in the near
		futur for that */
		AVPacket pkt;
		av_init_packet(&pkt);

		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index= st->index;
		pkt.data= (uint8_t *)os->picture;
		pkt.size= sizeof(AVPicture);

		ret = av_interleaved_write_frame(oc, &pkt);
	} else {
		/* encode the image */
		out_size = avcodec_encode_video(c, os->video_outbuf, os->video_outbuf_size, os->picture);
		/* if zero size, it means the image was buffered */
		if (out_size > 0) {
			AVPacket pkt;
			av_init_packet(&pkt);

			if (c->coded_frame->pts != AV_NOPTS_VALUE)
				pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);

			if(c->coded_frame->key_frame)
				pkt.flags |= AV_PKT_FLAG_KEY;

			pkt.stream_index= st->index;
			pkt.data= os->video_outbuf;
			pkt.size= out_size;
			pkt.dts = pkt.pts;


			/* write the compressed frame in the media file */
			ret = av_interleaved_write_frame(oc, &pkt);
		} else {
			ret = 0;
		}
	}
	if (ret != 0) {
		ms_error("Error while writing video frame");
		return -4;
	}

	os->video_write_pts+= (1/os->fps);
	os->frame_count++;
	if(om) freemsg(om);
	return ret;
}

static int write_video_frame(VideoRecoder *os)
{
	mblk_t *im;
	
	if(os->video_queue.q.q_mcount < 0) return -1;

	//读取
	im=ms_queue_get(&os->video_queue);

	if(!im) return -1;

	if(os->nb_frames_drop>0){
		freemsg(im);
		os->nb_frames_drop--;
		return -2;
	}

	/*根据时间戳进行补帧或丢帧处理*/
	{
		int audio_sample_ms = 20; /*每个音频包的长度*/
		int frame_diff =0;

		double audio_time = os->audio_write_pts;/*音轨时间*/
		double video_time = os->video_write_pts;/*视频帧时间*/

		double time_diff = audio_time - video_time;
		double time_per_fps = 1/os->fps;

		frame_diff = time_diff/time_per_fps;

		/*补帧*/
		if(frame_diff>0 /*&& os->is_rtmp==FALSE*/){
			int i;
			for(i=0;i<frame_diff;i++){	
				file_write_encoder_video(os,im);
			}
			if(frame_diff>=2)ms_message("auido time %f,video time %f, need dup frame packets %d",audio_time,video_time,frame_diff);
		}else{
			file_write_encoder_video(os,im);
		}

		/*丢帧*/
		if(frame_diff < 0){
			os->nb_frames_drop = abs(frame_diff);
			if(frame_diff>=2)ms_message("auido time %f,video time %f, need drop frame packets %d",audio_time,video_time,abs(frame_diff));
		}
	}

	freemsg(im);
	return 0;
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

static int file_write_frame(VideoRecoder *os)
{
	int ret = -1;
	int ret2= -1;

	ret =  write_video_frame(os);
	ret2 =  write_audio_frame(os);

	return (ret==0 && ret2==0)? 0 : -1;
}

static mblk_t *mono2stereo(mblk_t *im)
{
	int msgsize=msgdsize(im)*2;
	mblk_t *om=allocb(msgsize,0);

	for (;im->b_rptr<im->b_wptr;im->b_rptr+=2,om->b_wptr+=4){
		((int16_t*)om->b_wptr)[0]=*(int16_t*)im->b_rptr;
		((int16_t*)om->b_wptr)[1]=*(int16_t*)im->b_rptr;
	}

	freemsg(im);
	return om;
}

static void av_read_buffer(VideoRecoder *os){
	mblk_t *aim;
	mblk_t *vim;

	//读取声音样本
	ms_mutex_lock(&os->audio_inq.lock);
	while((aim=ms_queue_get(&os->audio_inq.q))!=NULL){
		ms_mutex_unlock(&os->audio_inq.lock);
		/*BUG 20111121 待解决,CallRecord 采样率不一致*/
		//单声道转2声道;
		//	int buffer_size = (采样率 * 2字节 *声道数)/1000 * 毫秒长度;
		if(os->nchannels==1)
			ms_bufferizer_put(os->bufferizer,mono2stereo(aim));
		else
			ms_bufferizer_put(os->bufferizer,aim);
		ms_mutex_lock(&os->audio_inq.lock);
	}
	ms_mutex_unlock(&os->audio_inq.lock);

	//读取视频帧
	ms_mutex_lock(&os->video_inq.lock);
	while((vim=ms_queue_get(&os->video_inq.q))!=NULL){
		ms_mutex_unlock(&os->video_inq.lock);
		ms_queue_put(&os->video_queue,vim);
		ms_mutex_lock(&os->video_inq.lock);
	}
	ms_mutex_unlock(&os->video_inq.lock);
}

static void av_queue_flush(VideoRecoder *os){
	conf_slot_queue_flush(&os->audio_inq);
	conf_slot_queue_flush(&os->video_inq);
}

void *av_record_run(void *arg){
	VideoRecoder *os = (VideoRecoder*)arg;
	AVFormatContext *oc = os->oc;
	int ret=1;
	int precision=2;

	if(os->is_rtmp) 
		precision = set_high_prio();

	while(os->run){

		if(os->start_recroding == FALSE){
			av_queue_flush(os);	
			sleepMs(10);
			continue;
		}

		av_read_buffer(os);
		ret = file_write_frame(os);

		if(ret < 0){
			sleepMs(10);
			continue;
		}
	}

	if(os->is_rtmp==FALSE)
	{
		while(file_write_frame(os)==0){ 
			sleepMs(10); 
		}
	}else
		av_queue_flush(os);
	
	if(os->is_rtmp) unset_high_prio(precision);

	ms_thread_exit(NULL);
	return NULL;
}

int video_recoder_starting(VideoRecoder *os,bool_t yesno)
{
	os->start_recroding = yesno;
	return 0;
}

int video_recoder_start(VideoRecoder *os)
{

	os->run = TRUE;
	os->nb_frames_drop =0;
	os->nb_frames_dup = 0;
	os->start_recroding = FALSE;

	ms_thread_create(&os->thread_id,NULL,&av_record_run,os);

	if(!os->thread_id) {
		return -1;
	}
	return 0;
}


void video_recoder_free(VideoRecoder *os)
{
	unsigned int  i;
	AVFormatContext *oc = os->oc;

	av_write_trailer(oc);

	/* close each codec */
	if (os->video_st)
		close_video(os);
	if (os->audio_st)
		close_audio(os);

	/* free the streams */
	for(i = 0; i < oc->nb_streams; i++) {
		av_freep(&oc->streams[i]->codec);
		av_freep(&oc->streams[i]);
	}

	if (!(os->fmt->flags & AVFMT_NOFILE)) {
		/* close the output file */
		url_fclose(oc->pb);
	}

	if (os->sws_ctx!=NULL) {
		ms_sws_freeContext(os->sws_ctx);
		os->sws_ctx=NULL;
	}
	if (os->om!=NULL){
		freemsg(os->om);
		os->om=NULL;
	}

	/* free the stream */
	av_free(oc);
}

int video_recoder_stop(VideoRecoder *os)
{
	os->run=FALSE;
	if(os->thread_id) ms_thread_join(os->thread_id,NULL);

	video_recoder_free(os);
	return 0;
}

int video_recoder_destory(VideoRecoder *os)
{
  	if(os->bufferizer) ms_bufferizer_destroy(os->bufferizer);

	conf_slot_queue_destory(&os->audio_inq);
	conf_slot_queue_destory(&os->video_inq);

	ms_queue_flush(&os->video_queue);
	ms_mutex_destroy(&os->mutex);
	ms_cond_destroy(&os->cond);

	ms_free(os);
	return 0;
}



MSFilter *video_recoder_create_video_filter(VideoRecoder *os){
	MSFilter *f = ms_filter_new(MS_CONF_ITC_SINK_ID);
	ms_filter_call_method(f,MS_CONF_ITC_SINK_CONNECT,&os->video_inq);
	return f;
}


MSFilter *video_recoder_create_audio_filter(VideoRecoder *os){
	MSFilter *f = ms_filter_new(MS_CONF_ITC_SINK_ID);
	ms_filter_call_method(f,MS_CONF_ITC_SINK_CONNECT,&os->audio_inq);
	return f;
}

ConfSlotQueue *video_recoder_get_audio_inq(VideoRecoder *os){
	return &os->audio_inq;
}

ConfSlotQueue *video_recoder_get_video_inq(VideoRecoder *os){
	return &os->video_inq;
}

static MSFilter *create_writer(MSSndCard *c){
	MSFilter *f =   ms_filter_new(MS_CONF_ITC_SINK_ID);
	VideoRecoder *os = (VideoRecoder *)c->data;
	if(os)
	{  //设置播放器参数
		ms_filter_call_method(f,MS_CONF_ITC_SINK_CONNECT,&os->audio_inq);
	}
	return f;
}

static MSSndCardDesc file_player_snd_desc={
	"File Record SND",
	/*detect*/ NULL,
	/*init*/ NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	create_writer,
	/*uninit,*/
};

MSSndCard *ms_file_writer_snd_new(VideoRecoder *os)
{
	MSSndCard *snd =ms_snd_card_new(&file_player_snd_desc);
	snd->data = os;
	snd->capabilities = MS_SND_CARD_CAP_PLAYBACK;
	return snd;
}