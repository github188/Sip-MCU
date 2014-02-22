#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include "g729ab.h"

#include "codec_g729.h"
#include "iaxclient_lib.h"


/* include by g729ab.h
* #define G729_VOICE_FRAME_SIZE 10
* #define G729_PCM_FRAME_SIZE   80
*/



static void destroy ( struct iaxc_audio_codec *c) 
{
	free(c->encstate);
	free(c->decstate);
	free(c);
}



//Add by B-52-1 2008-08-22, all ok
static int encode ( struct iaxc_audio_codec *c, 
    int *inlen, short *in, int *outlen, unsigned char *out ) 
{
	int size = 10; 

	while( (*inlen >= 80) && (*outlen >= 10) ) {

			g729_coder(c->encstate,in,out, &size);
			/* we used 160 bytes of input, and 10 bytes of output */
			*inlen -= 80;
			in += 80;
			*outlen -= 10;
			out += 10;
		}

	return 0;
}



static int decode ( struct iaxc_audio_codec *c, 
    int *inlen, unsigned char *in, int *outlen, short *out ) 
{
	
	while( (*inlen >= 10) && (*outlen >= 80) ) {
		
		g729_decoder(c->decstate, out,in, G729_VOICE_FRAME_SIZE);

		/* we used 10 bytes of input, and 160 bytes of output */
		*inlen -= 10;
		in += 10;
		*outlen -= 80;
		out += 80;
    }

	return 0;
}



struct iaxc_audio_codec *codec_audio_g729_new() 
{

  struct iaxc_audio_codec *c = calloc(sizeof(struct iaxc_audio_codec),1);
  if(!c) return c;

  strcpy(c->name,"g729a ITU");

  c->format = IAXC_FORMAT_G729A;

  c->encode = encode;
  c->decode = decode;

  c->destroy = destroy;

  c->minimum_frame_size = 80;

  c->encstate = calloc(sizeof(struct cod_state),1);
  c->decstate = calloc(sizeof(struct dec_state),1);

    /* leaks a bit on no-memory */
  if(!(c->encstate && c->decstate))
      return NULL;
	
  g729_init_coder(c->encstate,0);
  g729_init_decoder(c->decstate);

  return c;
}


/*
struct state {
  cod_state g729_en;
  dec_state g729_de;
};
*/

