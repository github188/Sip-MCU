/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat-kPC/bLVnM4tg9hUCZPvPmw@xxxxxxxxxxxxxxxx)

enable linphone to using ADI's libbfgdots g729 lib. 

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


/**
 * bitstream format of libbfgdots - g729a/g729ab lib
 * 
 * using 0x6b21 as SYNC word for each frame
 *   [SYNCWORD][BIT_NUM][bits]
 * 
 * Packed mode:
 * 
 * - g729a:
 *   [0x6b21][0x0050][80-bit]
 *
 * - g729ab:
 *   three kinds of frames
 *   -- BIT_NUM = 0x0050
 *   -- BIT_NUM = 0x0010
 *   -- BIT_NUM = 0x0000
 *
 * Unpacked mode:
 *   each bit is encoded using a 16-bit word:
 *   - 0x007F for 0
 *   - 0x0081 for 1
 */

#include "mediastreamer2/msfilter.h"

#include "EasyG729A.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#pragma comment(lib, "EasyG729A.lib")

typedef struct EncState{
	CODER_HANDLE state;
	uint32_t ts;
	MSBufferizer *bufferizer; /* a queue */
} EncState;

#define G729_PCM_FRAME_SIZE 80
#define G729_VOICE_FRAME_SIZE 10

/*
#define  L_G729A_FRAME_COMPRESSED 10
#define  L_G729A_FRAME            80

#define  CODER_HANDLE  unsigned long

extern CODER_HANDLE EasyG729A_init_encoder();
extern bool   EasyG729A_encoder(CODER_HANDLE hEncoder, short *speech, unsigned char *bitstream);
extern bool   EasyG729A_release_encoder(CODER_HANDLE hEncoder);

extern CODER_HANDLE EasyG729A_init_decoder();
extern bool   EasyG729A_decoder(CODER_HANDLE hDecoder, unsigned char *bitstream, short *synth_short);
extern bool   EasyG729A_release_decoder(CODER_HANDLE hDecoder);

*/
#define G729_FRAME_SAMPLES 160
#define G729_ENCODED_FRAME_SIZE (20)
#define PCM_FRAME 80
#define G729_FRAME_COMPRESSED 10


static void enc_init(MSFilter *f){
	EncState *s=(EncState *)ms_new(EncState, 1); /* alloc EncState object, count = 1 */
	memset(s, 0 , sizeof(*s));
	s->state = EasyG729A_init_encoder();
	s->ts = 0;
	s->bufferizer=ms_bufferizer_new();
	f->data=s;
}

static void enc_uninit(MSFilter *f){
	EncState *s=(EncState*)f->data;
	EasyG729A_release_encoder(s->state);
	s->state = NULL;
	ms_bufferizer_destroy(s->bufferizer);
	ms_free(s);
}

static void enc_process(MSFilter *f){
	EncState *s=(EncState*)f->data;
	mblk_t *im;
	int size = 10;
	int i,frames;
	int16_t buf[G729_PCM_FRAME_SIZE * 2];
	int16_t *p;

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		ms_bufferizer_put(s->bufferizer,im);
	}

	while(ms_bufferizer_read(s->bufferizer,(uint8_t*)buf,sizeof(buf))==sizeof(buf)) {
		mblk_t *om=allocb(20,0);
		frames = sizeof(buf)/(G729_PCM_FRAME_SIZE * 2);
		//ms_error("read pcm data, size %d,frames %d",sizeof(buf),frames);
		p = buf;
		//intput 160 * sizeof(int)   ==>    10 * sizeof(char) * 2 frames 
		for(i=0;i<frames;i++)
		{
			//EasyG729A_encoder(CODER_HANDLE hEncoder, short *speech, unsigned char *bitstream);
			EasyG729A_encoder(s->state, p,om->b_wptr);
			om->b_wptr+=10;
			p += 80;
		}
		mblk_set_timestamp_info(om,s->ts);
		ms_queue_put(f->outputs[0],om);
		s->ts+=sizeof(buf)/2;
	}
}

#ifdef _MSC_VER
MSFilterDesc ms_g729_enc_desc={
	MS_FILTER_PLUGIN_ID,
	"MSG729Enc",
	"The G729a(b) codec",
	MS_FILTER_ENCODER,
	"g729",
	1,
	1,
	enc_init,
	NULL,
	enc_process,
	NULL,
	enc_uninit,
	NULL
};
#else
MSFilterDesc ms_g729_enc_desc={
	.id=MS_G729_ENC_ID,
	.name="MSG729Enc",
	.text="The G729a(b) codec",
	.category=MS_FILTER_ENCODER,
	.enc_fmt="g729",
	.ninputs=1,
	.noutputs=1,
	.init=enc_init,
	.process=enc_process,
	.uninit=enc_uninit,
};
#endif

typedef EncState DecState;
static void dec_init(MSFilter *f){
	DecState *s = (DecState*)ms_new(DecState, 1);
	s->state = EasyG729A_init_decoder();
	f->data= s;
}

static void dec_uninit(MSFilter *f){
	DecState *s=(DecState*)f->data;
	EasyG729A_release_decoder(s->state);
	s->state = NULL;
	ms_free(s);
}

static void dec_process(MSFilter *f){
	DecState *s=(DecState*)f->data;
	int nbytes = 0;
	mblk_t *im;
	mblk_t *om;
	const int frsz=160*2;
	int i,frames;


	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		nbytes=msgdsize(im);
		//ms_error("read g729 data, size %d",msgdsize(im));
		frames = nbytes/G729_VOICE_FRAME_SIZE;

		//intput  10 * sizeof(char) * 2 frames   ==>  160 * sizeof(int)
		for(i=0;i<frames;i++){
			om=allocb(frsz,0);
			EasyG729A_decoder(s->state,(unsigned char *)im->b_rptr,(short *)om->b_wptr);
			im->b_rptr+=10;
			om->b_wptr+=160;
			ms_queue_put(f->outputs[0],om);
		}

		freemsg(im);
	}
	

		
}


#ifdef _MSC_VER
MSFilterDesc ms_g729_dec_desc={
	MS_FILTER_PLUGIN_ID,
	"MSG729Dec",
	"The G729 codec",
	MS_FILTER_DECODER,
	"g729",
	1,
	1,
	dec_init,
	NULL,
	dec_process,
	NULL,
	dec_uninit,
	NULL
};
#else
MSFilterDesc ms_g729_dec_desc={
	.id=MS_G729_DEC_ID,
	.name="MSG729Dec",
	.text="The G729 codec",
	.category=MS_FILTER_DECODER,
	.enc_fmt="g729",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.process=dec_process,
	.uninit=dec_uninit
};
#endif

/*
#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) type
#endif

MS_PLUGIN_DECLARE(void) qfish_plugin_init(){
	if(qf_init(plugin_key))
	{
		ms_filter_register(&ms_g729_dec_desc);
		ms_filter_register(&ms_g729_enc_desc);
	}else
	{
		ms_error("You do not have permission to load this Plugin !");
	}

}
*/
extern "C" void g729_plugin_init(){

	ms_filter_register(&ms_g729_dec_desc);
	ms_filter_register(&ms_g729_enc_desc);


}