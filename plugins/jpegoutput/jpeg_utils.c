#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
#include "mediastreamer2/msvideo.h"

#define OUTPUT_BUF_SIZE  4096

typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */

    JOCTET * buffer;    /* start of buffer */

    unsigned char *outbuffer;
    int outbuffer_size;
    unsigned char *outbuffer_cursor;
    int *written;

} mjpg_destination_mgr;

typedef mjpg_destination_mgr * mjpg_dest_ptr;

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) init_destination(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    /* Allocate the output buffer --- it will be released when done with image */
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

    *(dest->written) = 0;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/******************************************************************************
Description.: called whenever local jpeg buffer fills up
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    memcpy(dest->outbuffer_cursor, dest->buffer, OUTPUT_BUF_SIZE);
    dest->outbuffer_cursor += OUTPUT_BUF_SIZE;
    *(dest->written) += OUTPUT_BUF_SIZE;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

/******************************************************************************
Description.: called by jpeg_finish_compress after all data has been written.
              Usually needs to flush buffer.
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) term_destination(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    memcpy(dest->outbuffer_cursor, dest->buffer, datacount);
    dest->outbuffer_cursor += datacount;
    *(dest->written) += datacount;
}

/******************************************************************************
Description.: Prepare for output to a stdio stream.
Input Value.: buffer is the already allocated buffer memory that will hold
              the compressed picture. "size" is the size in bytes.
Return Value: -
******************************************************************************/
GLOBAL(void) dest_buffer(j_compress_ptr cinfo, unsigned char *buffer, int size, int *written)
{
    mjpg_dest_ptr dest;

    if(cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(mjpg_destination_mgr));
    }

    dest = (mjpg_dest_ptr) cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outbuffer = buffer;
    dest->outbuffer_size = size;
    dest->outbuffer_cursor = buffer;
    dest->written = written;
}

/******************************************************************************
Description.: yuv2jpeg function is based on compress_yuyv_to_jpeg written by
              Gabriel A. Devenyi.
              modified to support other formats like RGB5:6:5 by Miklós Márton
              It uses the destination manager implemented above to compress
              YUYV data to JPEG. Most other implementations use the
              "jpeg_stdio_dest" from libjpeg, which can not store compressed
              pictures to memory instead of a file.
Input Value.: video structure from v4l2uvc.c/h, destination buffer and buffersize
              the buffer must be large enough, no error/size checking is done!
Return Value: the buffer will contain the compressed data
******************************************************************************/
int compress_image_to_jpeg(const unsigned char *yuyvBuffer,MSPixFmt formatIn, int width,int height, unsigned char *outBuffer, int size, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer, *yuyv;
    int z;
    static int written;

    line_buffer = calloc(width * 3, 1);
	yuyv = yuyvBuffer;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    /* jpeg_stdio_dest (&cinfo, file); */
    dest_buffer(&cinfo, outBuffer, size, &written);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;

	if (formatIn == MS_YUYV)
		cinfo.in_color_space = JCS_RGB;
	else if (formatIn == MS_YUV420P)
		cinfo.in_color_space = JCS_YCbCr;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    

    z = 0;
    if (formatIn == MS_YUYV) {
		
		jpeg_start_compress(&cinfo, TRUE);

        while(cinfo.next_scanline < height) {
            int x;
            unsigned char *ptr = line_buffer;


            for(x = 0; x < width; x++) {
                int r, g, b;
                int y, u, v;

                if(!z)
                    y = yuyv[0] << 8;
                else
                    y = yuyv[2] << 8;
                u = yuyv[1] - 128;
                v = yuyv[3] - 128;

                r = (y + (359 * v)) >> 8;
                g = (y - (88 * u) - (183 * v)) >> 8;
                b = (y + (454 * u)) >> 8;

                *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
                *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
                *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

                if(z++) {
                    z = 0;
                    yuyv += 4;
                }
            }

            row_pointer[0] = line_buffer;
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }
    } else if(formatIn == MS_YUV420P)
	{
		JSAMPARRAY pp[3];
		JSAMPROW *rpY = (JSAMPROW*)malloc(sizeof(JSAMPROW) * height);
		JSAMPROW *rpU = (JSAMPROW*)malloc(sizeof(JSAMPROW) * height);
		JSAMPROW *rpV = (JSAMPROW*)malloc(sizeof(JSAMPROW) * height);
		int k;
		if(rpY == NULL && rpU == NULL && rpV == NULL)
		{
			goto end;
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
			rpY[k]   = yuyvBuffer + k*width;
			rpY[k+1] = yuyvBuffer + (k+1)*width;
			rpU[k/2] = yuyvBuffer+width*height + (k/2)*width/2;
			rpV[k/2] = yuyvBuffer+width*height*5/4 + (k/2)*width/2;
		}
		for (k = 0; k < height; k+=2*DCTSIZE) 
		{
			pp[0] = &rpY[k];
			pp[1] = &rpU[k/2];
			pp[2] = &rpV[k/2];
			jpeg_write_scanlines(&cinfo, pp, 2*DCTSIZE);
		}
	}

end:
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    free(line_buffer);

    return (written);
}



int jpeg_enc_yv12(char* buffer, int width, int height, int quality, char* filename)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *outfile = NULL;
	int ret = 1;
	if(buffer == NULL || width <=0 || height <=0|| filename == NULL)
		return 0;

#ifdef WIN32
	if ((outfile = fopen(filename, "wb")) == NULL) 
	{  
		return 0;
	}   
#else
	if ((outfile = fopen(filename, "r+x")) == NULL) 
	{  
		return 0;
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
			ret = 0;
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

/*
int main()
{
	char *buffer = malloc(640*480*3/2);
	jpeg_enc_yv12(buffer, 640, 480, 90, "test.jpg");
	free(buffer);    
}*/


