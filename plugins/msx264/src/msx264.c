/*
mediastreamer2 x264 plugin
Copyright (C) 2006-2010 Belledonne Communications SARL (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"

#include <stdint.h>
#include <x264.h>

#define RC_MARGIN 10000 /*bits per sec*/


static int x264_param_apply_preset( x264_param_t *param, const char *preset ) { 

	if( !strcasecmp( preset, "ultrafast" )) { 
		param->i_frame_reference = 1; 
		param->i_scenecut_threshold = 0; 
		param->b_deblocking_filter = 0; 
		param->b_cabac = 0; 
		param->i_bframe = 0; 
		param->analyse.intra = 0; 
		param->analyse.inter = 0; 
		param->analyse.b_transform_8x8 = 0; 
		param->analyse.i_me_method = X264_ME_DIA; 
		param->analyse.i_subpel_refine = 0; 
		param->rc.i_aq_mode = 0; 
		param->analyse.b_mixed_references = 0; 
		param->analyse.i_trellis = 0; 
		param->i_bframe_adaptive = X264_B_ADAPT_NONE; 
		param->rc.b_mb_tree = 0; 
		param->analyse.i_weighted_pred = X264_WEIGHTP_NONE; 
		param->analyse.b_weighted_bipred = 0; 
		param->rc.i_lookahead = 0; 
	}else if( !strcasecmp( preset, "superfast" )) { 
		param->analyse.inter = X264_ANALYSE_I8x8|X264_ANALYSE_I4x4; 
		param->analyse.i_me_method = X264_ME_DIA; 
		param->analyse.i_subpel_refine = 1; 
		param->i_frame_reference = 1; 
		param->analyse.b_mixed_references = 0; 
		param->analyse.i_trellis = 0; 
		param->rc.b_mb_tree = 0; 
		param->analyse.i_weighted_pred = X264_WEIGHTP_NONE; 
		param->rc.i_lookahead = 0; 
	}else if( !strcasecmp( preset, "veryfast" )){ 
		param->analyse.i_me_method = X264_ME_HEX; 
		param->analyse.i_subpel_refine = 2; 
		param->i_frame_reference = 1; 
		param->analyse.b_mixed_references = 0; 
		param->analyse.i_trellis = 0; 
		param->analyse.i_weighted_pred = X264_WEIGHTP_NONE; 
		param->rc.i_lookahead = 10; 
	}else if( !strcasecmp( preset, "faster" )) { 
		param->analyse.b_mixed_references = 0; 
		param->i_frame_reference = 2; 
		param->analyse.i_subpel_refine = 4; 
		param->analyse.i_weighted_pred = X264_WEIGHTP_SMART; 
		param->rc.i_lookahead = 20; 
	}else if( !strcasecmp( preset, "fast" )) { 
		param->i_frame_reference = 2; 
		param->analyse.i_subpel_refine = 6; 
		param->rc.i_lookahead = 30; 
	}else if( !strcasecmp( preset, "medium" )){ 
		/* Default is medium */ 
	}else if( !strcasecmp( preset, "slow" )) { 
		param->analyse.i_me_method = X264_ME_UMH; 
		param->analyse.i_subpel_refine = 8; 
		param->i_frame_reference = 5; 
		param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS; 
		param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO; 
		param->rc.i_lookahead = 50; 
	}else if( !strcasecmp( preset, "slower" )){ 
		param->analyse.i_me_method = X264_ME_UMH; 
		param->analyse.i_subpel_refine = 9; 
		param->i_frame_reference = 8; 
		param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS; 
		param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO; 
		param->analyse.inter |= X264_ANALYSE_PSUB8x8; 
		param->analyse.i_trellis = 2; 
		param->rc.i_lookahead = 60; 
	} else if( !strcasecmp( preset, "veryslow" )){ 
		param->analyse.i_me_method = X264_ME_UMH; 
		param->analyse.i_subpel_refine = 10; 
		param->analyse.i_me_range = 24; 
		param->i_frame_reference = 16; 
		param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS; 
		param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO; 
		param->analyse.inter |= X264_ANALYSE_PSUB8x8; 
		param->analyse.i_trellis = 2; 
		param->i_bframe = 8; 
		param->rc.i_lookahead = 60; 
	}else if( !strcasecmp( preset, "placebo" )){ 
		param->analyse.i_me_method = X264_ME_TESA; 
		param->analyse.i_subpel_refine = 10; 
		param->analyse.i_me_range = 24; 
		param->i_frame_reference = 16; 
		param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS; 
		param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO; 
		param->analyse.inter |= X264_ANALYSE_PSUB8x8; 
		param->analyse.b_fast_pskip = 0; 
		param->analyse.i_trellis = 2; 
		param->i_bframe = 16; 
		param->rc.i_lookahead = 60; 
	}else{ 
		return -1; 
	} 

	return 0; 

} 


static int x264_param_apply_tune( x264_param_t *param, const char *tune ){ 
	if( !strcasecmp( tune, "film", 4 ) ) {
		param->i_deblocking_filter_alphac0 = -1; 
		param->i_deblocking_filter_beta = -1; 
		param->analyse.f_psy_trellis = 0.15; 
	}else if( !strcasecmp( tune, "animation", 9 )){ 
		param->i_frame_reference = param->i_frame_reference > 1 ? param->i_frame_reference*2 : 1; 
		param->i_deblocking_filter_alphac0 = 1; 
		param->i_deblocking_filter_beta = 1; 
		param->analyse.f_psy_rd = 0.4; 
		param->rc.f_aq_strength = 0.6; 
		param->i_bframe += 2; 
	}else if( !strcasecmp( tune, "grain", 5 )){ 
		param->i_deblocking_filter_alphac0 = -2; 
		param->i_deblocking_filter_beta = -2; 
		param->analyse.f_psy_trellis = 0.25; 
		param->analyse.b_dct_decimate = 0; 
		param->rc.f_pb_factor = 1.1; 
		param->rc.f_ip_factor = 1.1; 
		param->rc.f_aq_strength = 0.5; 
		param->analyse.i_luma_deadzone[0] = 6; 
		param->analyse.i_luma_deadzone[1] = 6; 
		param->rc.f_qcompress = 0.8; 
	}else if( !strcasecmp( tune, "stillimage", 5 )){ 
		param->i_deblocking_filter_alphac0 = -3; 
		param->i_deblocking_filter_beta = -3; 
		param->analyse.f_psy_rd = 2.0; 
		param->analyse.f_psy_trellis = 0.7; 
		param->rc.f_aq_strength = 1.2; 
	}else if( !strcasecmp( tune, "psnr", 4 )){ 
		param->rc.i_aq_mode = X264_AQ_NONE; 
		param->analyse.b_psy = 0; 
	}else if( !strcasecmp( tune, "ssim", 4 )){
		param->rc.i_aq_mode = X264_AQ_AUTOVARIANCE; 
		param->analyse.b_psy = 0; 
	}else if( !strcasecmp( tune, "fastdecode", 10 )){ 
		param->b_deblocking_filter = 0; 
		param->b_cabac = 0; 
		param->analyse.b_weighted_bipred = 0; 
		param->analyse.i_weighted_pred = X264_WEIGHTP_NONE; 
	}else if( !strcasecmp( tune, "zerolatency", 11 )){ 
		param->rc.i_lookahead = 0; 
		param->i_sync_lookahead = 0; 
		param->i_bframe = 0; 
		param->b_sliced_threads = 1; 
		param->b_vfr_input = 0; 
		param->rc.b_mb_tree = 0; 
	}else if( !strcasecmp( tune, "touhou", 6 )){ 
		param->i_frame_reference = param->i_frame_reference > 1 ? param->i_frame_reference*2 : 1; 
		param->i_deblocking_filter_alphac0 = -1; 
		param->i_deblocking_filter_beta = -1; 
		param->analyse.f_psy_trellis = 0.2; 
		param->rc.f_aq_strength = 1.3; 

		if( param->analyse.inter & X264_ANALYSE_PSUB16x16 ) 
			param->analyse.inter |= X264_ANALYSE_PSUB8x8; 

	} else { 
		return -1; 
	} 

	return 0; 

} 

/* the goal of this small object is to tell when to send I frames at startup:
at 2 and 4 seconds*/
typedef struct VideoStarter{
	uint64_t next_time;
	int i_frame_count;
}VideoStarter;

static void video_starter_init(VideoStarter *vs){
	vs->next_time=0;
	vs->i_frame_count=0;
}

static void video_starter_first_frame(VideoStarter *vs, uint64_t curtime){
	vs->next_time=curtime+2000;
}

static bool_t video_starter_need_i_frame(VideoStarter *vs, uint64_t curtime){
	if (vs->next_time==0) return FALSE;
	if (curtime>=vs->next_time){
		vs->i_frame_count++;
		if (vs->i_frame_count==1){
			vs->next_time+=2000;
		}else{
			vs->next_time=0;
		}
		return TRUE;
	}
	return FALSE;
}

typedef struct _EncData{
	x264_t *enc;
	MSVideoSize vsize;
	int bitrate;
	float fps;
	int mode;
	uint64_t framenum;
	Rfc3984Context *packer;
	int keyframe_int;
	VideoStarter starter;
	bool_t generate_keyframe;
}EncData;


static void enc_init(MSFilter *f){
	EncData *d=ms_new(EncData,1);
	d->enc=NULL;
	d->bitrate=384000;
	d->vsize.width=MS_VIDEO_SIZE_CIF_W;
	d->vsize.height=MS_VIDEO_SIZE_CIF_H;
	d->fps=30;
	d->keyframe_int=3; /*10 seconds */
	d->mode=1;
	d->framenum=0;
	d->generate_keyframe=FALSE;
	d->packer=NULL;
	f->data=d;
}

static void enc_uninit(MSFilter *f){
	EncData *d=(EncData*)f->data;
	ms_free(d);
}

static void enc_preprocess(MSFilter *f){
	EncData *d=(EncData*)f->data;
	x264_param_t params;
	float bitrate;
	
	d->packer=rfc3984_new();
	rfc3984_set_mode(d->packer,d->mode);
	rfc3984_enable_stap_a(d->packer,FALSE);
	
	x264_param_default(&params);
 	params.i_threads=0;
	params.i_sync_lookahead=0;
	params.i_width=d->vsize.width;
	params.i_height=d->vsize.height;
	params.i_fps_num=(int)d->fps;
	params.i_fps_den=1;
	params.i_slice_max_size=ms_get_payload_max_size()-100; /*-100 security margin*/
	params.i_level_idc=13;

	bitrate=(float)d->bitrate*0.92;
	if (bitrate>RC_MARGIN)
		bitrate-=RC_MARGIN;
	
	params.rc.i_rc_method = X264_RC_ABR;
	params.rc.i_bitrate=(int)(bitrate/1000);
	params.rc.f_rate_tolerance=0.1;
	params.rc.i_vbv_max_bitrate=(int) ((bitrate+RC_MARGIN/2)/1000);
	params.rc.i_vbv_buffer_size=params.rc.i_vbv_max_bitrate;
	params.rc.f_vbv_buffer_init=0.5;
	params.rc.i_lookahead=0;

	/*enable this by config ?*/
	/*
	params.i_keyint_max = (int)d->fps*d->keyframe_int;
	params.i_keyint_min = (int)d->fps;
	*/
	params.b_repeat_headers=1;
	params.b_annexb=0;

	//these parameters must be set so that our stream is baseline
	params.analyse.b_transform_8x8 = 0;
	params.b_cabac = 0;
	params.i_cqm_preset = X264_CQM_FLAT;
	params.i_bframe = 0;
	params.analyse.i_weighted_pred = X264_WEIGHTP_NONE;
	
	x264_param_apply_preset(&params,"faster");//将编码设置成superfast模式【相比其他模式，会有一些花屏】 
	x264_param_apply_tune(&params,"zerolatency");//将延时设置成最短 

	d->enc=x264_encoder_open(&params);

	if (d->enc==NULL) ms_error("Fail to create x264 encoder.");
	d->framenum=0;
	video_starter_init(&d->starter);
}

static void x264_nals_to_msgb(x264_nal_t *xnals, int num_nals, MSQueue * nalus){
	int i;
	mblk_t *m;
	/*int bytes;*/
	for (i=0;i<num_nals;++i){
		m=allocb(xnals[i].i_payload+10,0);
		
		memcpy(m->b_wptr,xnals[i].p_payload+4,xnals[i].i_payload-4);
		m->b_wptr+=xnals[i].i_payload-4;
		if (xnals[i].i_type==NAL_SPS) {
			ms_message("A SPS is being sent.");
		}else if (xnals[i].i_type==NAL_PPS) {
			ms_message("A PPS is being sent.");
		}else if(xnals[i].i_type== NAL_SEI){
			ms_message("A SEI is being sent.");
		}
		ms_queue_put(nalus,m);
	}
}

static void enc_process(MSFilter *f){
	EncData *d=(EncData*)f->data;
	uint32_t ts=(f->ticker->time+10)*90LL;
	mblk_t *im;
	MSPicture pic;
	MSQueue nalus;
	ms_queue_init(&nalus);
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		if (ms_yuv_buf_init_from_mblk(&pic,im)==0){
			x264_picture_t xpic;
			x264_picture_t oxpic;
			x264_nal_t *xnals=NULL;
			int num_nals=0;

			memset(&xpic, 0, sizeof(xpic));
			memset(&oxpic, 0, sizeof(oxpic));

			/*send I frame 2 seconds and 4 seconds after the beginning */
			if (video_starter_need_i_frame(&d->starter,f->ticker->time))
				d->generate_keyframe=TRUE;

			if (d->generate_keyframe){
				xpic.i_type=X264_TYPE_IDR;
				d->generate_keyframe=FALSE;
			}else xpic.i_type=X264_TYPE_AUTO;
			xpic.i_qpplus1=0;
			xpic.i_pts=d->framenum;
			xpic.param=NULL;
			xpic.img.i_csp=X264_CSP_I420;
			xpic.img.i_plane=3;
			xpic.img.i_stride[0]=pic.strides[0];
			xpic.img.i_stride[1]=pic.strides[1];
			xpic.img.i_stride[2]=pic.strides[2];
			xpic.img.i_stride[3]=0;
			xpic.img.plane[0]=pic.planes[0];
			xpic.img.plane[1]=pic.planes[1];
			xpic.img.plane[2]=pic.planes[2];
			xpic.img.plane[3]=0;

			if (x264_encoder_encode(d->enc,&xnals,&num_nals,&xpic,&oxpic)>=0){
				x264_nals_to_msgb(xnals,num_nals,&nalus);
				rfc3984_pack(d->packer,&nalus,f->outputs[0],ts);
				d->framenum++;
				if (d->framenum==0)
					video_starter_first_frame(&d->starter,f->ticker->time);
			}else{
				ms_error("x264_encoder_encode() error.");
			}
		}
		freemsg(im);
	}
}

static void enc_postprocess(MSFilter *f){
	EncData *d=(EncData*)f->data;
	rfc3984_destroy(d->packer);
	d->packer=NULL;
	if (d->enc!=NULL){
		x264_encoder_close(d->enc);
		d->enc=NULL;
	}
}

static int enc_set_br(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	d->bitrate=*(int*)arg;

	if (d->bitrate>=1024000){
		d->vsize.width=MS_VIDEO_SIZE_720P_W;
		d->vsize.height=MS_VIDEO_SIZE_720P_H;
		d->fps=20;
	}else if (d->bitrate>=512000){
		d->vsize.width=MS_VIDEO_SIZE_4CIF_W;
		d->vsize.height=MS_VIDEO_SIZE_4CIF_H;
		d->fps=17;
    }else if (d->bitrate>=384000){
		d->vsize.width=MS_VIDEO_SIZE_VGA_W;
		d->vsize.height=MS_VIDEO_SIZE_VGA_H;
		d->fps=15;
	}else if (d->bitrate>=256000){
		d->vsize.width=MS_VIDEO_SIZE_CIF_W;
		d->vsize.height=MS_VIDEO_SIZE_CIF_H;
		d->fps=15;
	}else if (d->bitrate>=128000){
		d->vsize.width=MS_VIDEO_SIZE_CIF_W;
		d->vsize.height=MS_VIDEO_SIZE_CIF_H;
		d->fps=15;
	}else if (d->bitrate>=64000){
		d->vsize.width=MS_VIDEO_SIZE_CIF_W;
		d->vsize.height=MS_VIDEO_SIZE_CIF_H;
		d->fps=15;
	}else if (d->bitrate>=32000){
		d->vsize.width=MS_VIDEO_SIZE_QCIF_W;
		d->vsize.height=MS_VIDEO_SIZE_QCIF_H;
		d->fps=15;
	}else{
		d->vsize.width=MS_VIDEO_SIZE_QCIF_W;
		d->vsize.height=MS_VIDEO_SIZE_QCIF_H;
		d->fps=15;
	}

	if (d->enc != NULL)
	{
		/*apply new settings dynamically*/
		ms_filter_lock(f);
		enc_postprocess(f);
		enc_preprocess(f);
		ms_filter_unlock(f);
	}

	ms_message("bitrate set to %i",d->bitrate);
	return 0;
}

static int enc_set_fps(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	d->fps=*(float*)arg;
	return 0;
}

static int enc_get_fps(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	*(float*)arg=d->fps;
	return 0;
}

static int enc_get_vsize(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	*(MSVideoSize*)arg=d->vsize;
	return 0;
}

static int enc_set_vsize(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	d->vsize=*(MSVideoSize*)arg;
	return 0;
}

static int enc_add_fmtp(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	const char *fmtp=(const char *)arg;
	char value[12];
	if (fmtp_get_value(fmtp,"packetization-mode",value,sizeof(value))){
		d->mode=atoi(value);
		ms_message("packetization-mode set to %i",d->mode);
	}
	return 0;
}

static int enc_req_vfu(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	d->generate_keyframe=TRUE;
	return 0;
}


static MSFilterMethod enc_methods[]={
	{	MS_FILTER_SET_FPS	,	enc_set_fps	},
	{	MS_FILTER_SET_BITRATE	,	enc_set_br	},
	{	MS_FILTER_GET_FPS	,	enc_get_fps	},
	{	MS_FILTER_GET_VIDEO_SIZE,	enc_get_vsize	},
	{	MS_FILTER_SET_VIDEO_SIZE,	enc_set_vsize	},
	{	MS_FILTER_ADD_FMTP	,	enc_add_fmtp	},
	{	MS_FILTER_REQ_VFU	,	enc_req_vfu	},
	{	0	,			NULL		}
};

#ifdef _MSC_VER
MSFilterDesc x264_enc_desc={
	MS_FILTER_PLUGIN_ID,
	"MSX264Enc",
	"A H264 encoder based on x264 project (with multislicing enabled)",
	MS_FILTER_ENCODER,
	"H264",
	1,
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	enc_methods
};
#else
static MSFilterDesc x264_enc_desc={
	.id=MS_FILTER_PLUGIN_ID,
	.name="MSX264Enc",
	.text="A H264 encoder based on x264 project (with multislicing enabled)",
	.category=MS_FILTER_ENCODER,
	.enc_fmt="H264",
	.ninputs=1,
	.noutputs=1,
	.init=enc_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=enc_methods
};
#endif // _MSC_VER

void libmsx264_init(void){
	ms_filter_register(&x264_enc_desc);
//	ms_message("ms264-" VERSION " plugin registered.");
}

