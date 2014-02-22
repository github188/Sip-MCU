/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL <simon.morlat@linphone.org>

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


#include "mediastreamer2/msjpegwriter.h"
#include "mediastreamer2/msvideo.h"
#include "ffmpeg-priv.h"
#include "jpeglib.h"

static bool_t jpeg_enc_yv12(char* buffer, int width, int height, int quality, char* filename)
{
	char fname[128] ;
	static int framenum = 0;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *outfile = NULL;
	bool_t ret = TRUE;
	if(buffer == NULL || width <=0 || height <=0|| filename == NULL)
		return FALSE;

#ifdef __MINGW32__
	//sprintf(fname, "%s-%d.jpg", filename, framenum++);
	sprintf(fname, "%s.jpg", filename);
#else
	sprintf(fname, "%s-%d.jpg", filename, framenum++);
#endif

#ifdef WIN32
	if ((outfile = fopen(fname, "wb")) == NULL) 
	{  
		return FALSE;
	}   
#else
	if ((outfile = fopen(fname, "r+x")) == NULL) 
	{  
		return FALSE;
	} 
#endif // WIN32


	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = width; 
	cinfo.image_height = height;
	cinfo.input_components = 3;        
	cinfo.in_color_space = JCS_YCbCr;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	cinfo.raw_data_in = TRUE;  

	{
		JSAMPARRAY pp[3];
		JSAMPROW *rpY = (JSAMPROW*)malloc(sizeof(JSAMPROW) * height);
		JSAMPROW *rpU = (JSAMPROW*)malloc(sizeof(JSAMPROW) * height);
		JSAMPROW *rpV = (JSAMPROW*)malloc(sizeof(JSAMPROW) * height);
		int k;
		if(rpY == NULL && rpU == NULL && rpV == NULL)
		{
			ret = FALSE;
			goto exit;
		}
		cinfo.comp_info[0].h_samp_factor =
			cinfo.comp_info[0].v_samp_factor = 2;
		cinfo.comp_info[1].h_samp_factor =
			cinfo.comp_info[1].v_samp_factor =
			cinfo.comp_info[2].h_samp_factor =
			cinfo.comp_info[2].v_samp_factor = 1;
		jpeg_start_compress(&cinfo, TRUE);

		for (k = 0; k < height; k+=2) 
		{
			rpY[k]   = buffer + k*width;
			rpY[k+1] = buffer + (k+1)*width;
			rpU[k/2] = buffer+width*height + (k/2)*width/2;
			rpV[k/2] = buffer+width*height*5/4 + (k/2)*width/2;
		}
		for (k = 0; k < height; k+=2*DCTSIZE) 
		{
			pp[0] = &rpY[k];
			pp[1] = &rpU[k/2];
			pp[2] = &rpV[k/2];
			jpeg_write_raw_data(&cinfo, pp, 2*DCTSIZE);
		}
		jpeg_finish_compress(&cinfo);
		free(rpY);
		free(rpU);
		free(rpV);
	}
exit:
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);
	return ret;
}

typedef struct {
	bool_t take_pic;
	char *filename;
}JpegWriter;


static void jpg_init(MSFilter *f){
	JpegWriter *s=ms_new0(JpegWriter,1);
	s->take_pic = FALSE;
	f->data=s;
}

static void jpg_uninit(MSFilter *f){
	JpegWriter *s=(JpegWriter*)f->data;
	if(s->filename!=NULL) ms_free(s->filename);
	ms_free(s);
}

static int take_snapshot(MSFilter *f, void *arg){
	JpegWriter *s=(JpegWriter*)f->data;
	const char *filename=(const char*)arg;
	
	if(filename==NULL) return -1;

	if(s->filename!=NULL) ms_free(s->filename);
	
	s->take_pic = TRUE;
	s->filename = ms_strdup(filename);

	return 0;
}


static void jpg_process(MSFilter *f){
	JpegWriter *s=(JpegWriter*)f->data;
	if (s->take_pic){
		MSPicture yuvbuf;
		mblk_t *m=ms_queue_peek_last(f->inputs[0]);
		if (ms_yuv_buf_init_from_mblk(&yuvbuf,m)==0){
			MSVideoSize dstsize;
			dstsize.height = yuvbuf.h;
			dstsize.width = yuvbuf.w;
			if(jpeg_enc_yv12(m->b_rptr,dstsize.width,dstsize.height,90,s->filename))
				ms_message("Snapshot Ok");
			else
				ms_message("Snapshot failed!");
			s->take_pic = FALSE;
		}
		goto end;
	}
	end:
	ms_queue_flush(f->inputs[0]);
}

static MSFilterMethod jpg_methods[]={
	{	MS_JPEG_WRITER_TAKE_SNAPSHOT, take_snapshot },
	{	0,NULL}
};

#ifndef _MSC_VER

MSFilterDesc ms_jpeg_writer_desc={
	.id=MS_JPEG_WRITER_ID,
	.name="MSJpegWriter",
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=jpg_init,
	.process=jpg_process,
	.uninit=jpg_uninit,
	.methods=jpg_methods
};

#else

MSFilterDesc ms_jpeg_writer_desc={
	MS_JPEG_WRITER_ID,
	"MSJpegWriter",
	N_("The jpeg writer filter"),
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	jpg_init,
	NULL,
	jpg_process,
	NULL,
	jpg_uninit,
	jpg_methods
};


#endif

MS_FILTER_DESC_EXPORT(ms_jpeg_writer_desc)
