#include "mediastreamer2/msfilter.h"

#ifdef __cplusplus
extern "C"{
#endif

#include "broadvoice.h"

#ifdef __cplusplus
}
#endif

#define BV32_FRAME_SIZE 80
#define BV32_CODE_SIZE 10

typedef struct EncState{
	bv32_encode_state_t *enc;
	uint32_t ts;
	MSBufferizer *bufferizer; /* a queue */
} EncState;

typedef struct DecState{
	bv32_decode_state_t *dec;
} DecState;



static void enc_init(MSFilter *f){
	EncState *s=(EncState *)ms_new(EncState, 1); /* alloc EncState object, count = 1 */
	memset(s, 0 , sizeof(*s));
	s->enc = bv32_encode_init(NULL);
	s->ts=0;
	s->bufferizer=ms_bufferizer_new();
	f->data=s;
}

static void enc_uninit(MSFilter *f){
	EncState *s=(EncState*)f->data;
	bv32_encode_release(s->enc);
	bv32_encode_free(s->enc);
	ms_bufferizer_destroy(s->bufferizer);
	ms_free(s);
}

static void enc_process(MSFilter *f){
	EncState *s=(EncState*)f->data;
	mblk_t *im;
	int size = BV32_CODE_SIZE * 2;
	int i,frames;
	uint8_t buf[BV32_FRAME_SIZE * 2 * 4];

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		ms_bufferizer_put(s->bufferizer,im);
	}

	while(ms_bufferizer_read(s->bufferizer,(uint8_t*)buf,sizeof(buf))==sizeof(buf)) {
		mblk_t *om=allocb(BV32_CODE_SIZE*8,0);
		uint8_t *in_buf = (uint8_t *)buf;

		frames = sizeof(buf)/(BV32_FRAME_SIZE  * 2);
		//	ms_error("read pcm data, size %d,frames %d",sizeof(buf),frames);

		for(i=0;i<frames;i++)
		{

			bv32_encode(s->enc,
				(uint8_t *)om->b_wptr,
				(int16_t *) in_buf,
				BV32_FRAME_SIZE);

			//ms_error("BV32_Encode frames %d",i);
			in_buf += 160;
			om->b_wptr += 20;
		}

		mblk_set_timestamp_info(om,s->ts);
		ms_queue_put(f->outputs[0],om);
		s->ts+=sizeof(buf)/2;
	}

}

#ifdef _MSC_VER
MSFilterDesc ms_bv32_enc_desc={
	MS_FILTER_PLUGIN_ID,
	"MSBV32Enc",
	"The BV32 codec",
	MS_FILTER_ENCODER,
	"bv32",
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
MSFilterDesc ms_bv32_enc_desc={
	.id=MS_FILTER_PLUGIN_ID,
	.name="MSBV32Enc",
	.text="The BV32 codec",
	.category=MS_FILTER_ENCODER,
	.enc_fmt="bv32",
	.ninputs=1,
	.noutputs=1,
	.init=enc_init,
	.process=enc_process,
	.uninit=enc_uninit,
};
#endif


static void dec_init(MSFilter *f){
	DecState *s = (DecState*)ms_new(DecState, 1);
	s->dec = bv32_decode_init(NULL);
	f->data= s;
}

static void dec_uninit(MSFilter *f){
	DecState *s=(DecState*)f->data;
	bv32_decode_release(s->dec);
	bv32_decode_free(s->dec);
	ms_free(s);
}

static void dec_process(MSFilter *f){
	DecState *s=(DecState*)f->data;
	int nbytes = 0;
	mblk_t *im;
	mblk_t *om;
	const int frsz=BV32_FRAME_SIZE * 2 * 4;
	int i,frames;

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		om=allocb(frsz,0);

		nbytes=msgdsize(im);
		frames = nbytes/(BV32_CODE_SIZE * 2);

		//ms_error("read bv32 data, size %d, frame %d",msgdsize(im),frames);

		for(i=0;i<frames;i++){

			if (mblk_get_precious_flag(im)) { //¶ª°üÒþ±Î
				bv32_fillin(s->dec, (int16_t *) om->b_wptr, BV32_CODE_SIZE);
			} else {
				bv32_decode(s->dec, (int16_t *) om->b_wptr, (uint8_t *) im->b_rptr, BV32_CODE_SIZE);
			}

			im->b_rptr += 20;
			om->b_wptr += 160;
		}

		ms_queue_put(f->outputs[0],om);
		freemsg(im);
	}

}


#ifdef _MSC_VER
MSFilterDesc ms_bv32_dec_desc={
	MS_FILTER_PLUGIN_ID,
	"MSBV32Dec",
	"The BV32 codec",
	MS_FILTER_DECODER,
	"bv32",
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
MSFilterDesc ms_bv32_dec_desc={
	.id=MS_FILTER_PLUGIN_ID,
	.name="MSBV32Dec",
	.text="The BV32 codec",
	.category=MS_FILTER_DECODER,
	.enc_fmt="bv32",
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
ms_filter_register(&ms_bv32_dec_desc);
ms_filter_register(&ms_bv32_enc_desc);
}else
{
ms_error("You do not have permission to load this Plugin !");
}

}
*/

MS_FILTER_DESC_EXPORT(ms_bv32_dec_desc)
MS_FILTER_DESC_EXPORT(ms_bv32_enc_desc)