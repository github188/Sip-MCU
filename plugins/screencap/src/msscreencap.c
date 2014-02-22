/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mswebcam.h"

#ifdef __cplusplus
extern "C"{
#endif
#include "ffmpeg-priv.h"
#ifdef __cplusplus
}
#endif

typedef struct _ScreenCapData{
	HWND    window_handle; /**< handle of the window for the grab */
	HDC     source_hdc;    /**< Source  device context */
	HDC     window_hdc;    /**< Destination, source-compatible device context */
	HBITMAP hbmp;          /**< Information on the bitmap captured */
	int     cursor;        /**< Also capture cursor */
	int     size;          /**< Size in bytes of the grab frame */
	int     bpp;           /**< Bits per pixel of the grab frame */
	int     x_off;         /**< Horizontal top-left corner coordinate */
	int     y_off;         /**< Vertical top-left corner coordinate */
	MSVideoSize vsize;
	uint8_t *buf;
	int index;
	uint64_t starttime;
	float fps;
	YuvBuf outbuf;
	int iVideoStream;
	mblk_t *yuv_msg;
	bool_t loop_after;
	struct ms_SwsContext *sws_ctx;
	enum PixelFormat input_pix_fmt;
	enum PixelFormat output_pix_fmt;
}ScreenCapData;


static int
win32grab_init(ScreenCapData *s)
{
	int          input_pixfmt;
	long int     screenwidth;
	long int     screenheight;
	int          width;
	int          height;
	BITMAP       bmp;
	const char   *param       = NULL;
	const char   *name        = NULL;

	s->x_off = 0;
	s->y_off = 0;
	/*    if (!ap->device) {
	ms_message("AVParameters don't specify any window. Use -vd.");
	return AVERROR_IO;
	}*/

	s->cursor = 1;
	/*
	param = ap->device; // Skip gdi: 
	while ((param = strchr(param, ':')) != NULL)
	{
	param++;
	if (!strncmp(param, "offset=", 7))
	sscanf(param+7, "%i,%i", &s->x_off, &s->y_off);
	else if (!strncmp(param, "cursor", 6))
	s->cursor = 1;
	else if (!strncmp(param, "title=", 6))
	{
	name = param+6;
	s->window_handle = FindWindow(NULL, name);
	break;
	}
	else break;
	}*/

	//s->window_handle = FindWindow(NULL, "ÊÓÆµ»áÒé");
	s->window_handle =  GetDesktopWindow( );

	s->source_hdc = GetDC(s->window_handle);
	if (NULL == s->source_hdc)
	{
		ms_error("Couldn't get window DC (error %li)",
			GetLastError());
		return -1;
	}

	screenwidth = GetDeviceCaps(s->source_hdc, HORZRES);
	screenheight = GetDeviceCaps(s->source_hdc, VERTRES);
	if (s->window_handle)
	{
		RECT dim;

		GetClientRect(s->window_handle, &dim);
		s->vsize.width = dim.right-dim.left;
		s->vsize.height = dim.bottom-dim.top;
	}
	else
	{
		s->vsize.width = screenwidth;
		s->vsize.height = screenheight;
	}

	s->bpp = GetDeviceCaps(s->source_hdc, BITSPIXEL);

	ms_message("Found window %s, capturing %ix%ix%i at (%i,%i)",
		(name) ? name : "NONE", s->vsize.width, s->vsize.height, s->bpp, s->x_off, s->y_off);

	if (s->vsize.width<0 || s->vsize.height<0 || s->bpp%8)
	{
		ms_error("Invalid properties, aborting");
		return -2;
	}

	s->window_hdc = CreateCompatibleDC(s->source_hdc);
	if (!s->window_hdc)
	{
		ms_message("Screen DC CreateCompatibleDC (error %li)",
			GetLastError());
		return -3;
	}
	s->hbmp = CreateCompatibleBitmap(s->source_hdc, s->vsize.width, s->vsize.height);
	if (!s->hbmp)
	{
		ms_message("Screen DC CreateCompatibleBitmap (error %li)",
			GetLastError());
		return -4;
	}

	/* Get info from the bitmap */
	GetObject(s->hbmp, sizeof(BITMAP), &bmp);
	ms_debug("Using Bitmap type %li, size %lix%lix%i,%i planes of width %li bytes",
		bmp.bmType, bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel,
		bmp.bmPlanes, bmp.bmWidthBytes);

	if (!SelectObject(s->window_hdc, s->hbmp))
	{
		ms_message("SelectObject (error %li)", GetLastError());
		return -5;
	}

	switch (s->bpp)
	{
	case 8: input_pixfmt = PIX_FMT_PAL8; break;
	case 16: input_pixfmt = PIX_FMT_RGB555; break;
	case 24: input_pixfmt = PIX_FMT_BGR24; break;
	case 32: input_pixfmt = PIX_FMT_RGB32; break;
	default:
		ms_message("image depth %i not supported ... aborting",
			s->bpp);
		return -1;
	}

	s->input_pix_fmt = (PixelFormat)input_pixfmt;

	s->size = bmp.bmWidthBytes*bmp.bmHeight*bmp.bmPlanes;

	s->buf = ms_new0(uint8_t,s->size);

	return 0;
}

static int
win32grab_read_packet(ScreenCapData *s, uint8_t *data_buf)
{

	/* Blit screen grab */
	if (!BitBlt(s->window_hdc, 0, 0, s->vsize.width, s->vsize.height,
		s->source_hdc, s->x_off, s->y_off, SRCCOPY))
	{
		ms_error("Failed to capture image (error %li)\n",
			GetLastError());
		return -1;
	}

	if (s->cursor)
	{
		/*
		http://www.codeproject.com/csharp/DesktopCaptureWithMouse.asp?df=100&forumid=261533&exp=0&select=1442638
		*/
		CURSORINFO ci;

		ci.cbSize = sizeof(ci);

		if (GetCursorInfo(&ci) && ci.flags == CURSOR_SHOWING)
		{
			HICON     icon = CopyIcon(ci.hCursor);
			ICONINFO  info;
			if(GetIconInfo(icon, &info))
			{
				long int x = ci.ptScreenPos.x - info.xHotspot;
				long int y = ci.ptScreenPos.y - info.yHotspot;

				if (s->window_handle)
				{
					RECT rect;

					if (GetWindowRect(s->window_handle, &rect))
					{
						ms_debug("Pos(%li,%li) -> (%li,%li)\n",
							x, y, x - rect.left, y - rect.top);
						x -= rect.left;
						y -= rect.top;
					}
					else
					{
						ms_error("Couldn't draw icon %li\n",
							GetLastError());
					}
				}

				if (!DrawIcon(s->window_hdc, x, y, icon))
				{
					ms_error("Couldn't draw icon %li\n",
						GetLastError());
				}
			}
			else
			{
				ms_error("Couldn't get cursor info: error %li\n",
					GetLastError());
			}

			DestroyIcon(icon);
		}
		else
		{
			ms_error("Cursor not showing? Error %li\n",
				GetLastError());
		}
	}
	/* Get bits */
	if (!GetBitmapBits(s->hbmp, s->size, data_buf))
	{
		ms_error("GetBitmapBits failed (error %li)\n",
			GetLastError());
		return -1;
	}

	return 0;
}

/**
* Closes win32 frame grabber (public device demuxer API).
*
* @param s1 Context from avformat core
* @return 0 success, !0 failure
*/
static int
win32grab_read_close(ScreenCapData *s)
{
	if (s->source_hdc)
		ReleaseDC(s->window_handle, s->source_hdc);
	if (s->window_hdc)
		DeleteDC(s->window_hdc);
	if (s->hbmp)
		DeleteObject(s->hbmp);
	if (s->source_hdc)
		DeleteDC(s->source_hdc);

	return 0;
}

static void reset_data(ScreenCapData *d);

static void scrren_capture_init(MSFilter *f){
	ScreenCapData *d=(ScreenCapData*)ms_new(ScreenCapData,1);
	d->vsize.width=MS_VIDEO_SIZE_CIF_W;
	d->vsize.height=MS_VIDEO_SIZE_CIF_H;
	d->fps=25;
	d->index=0;
	d->starttime=0;
	d->iVideoStream=-1;
	d->loop_after=FALSE;
	d->sws_ctx=NULL;
	d->outbuf.w=0;
	d->outbuf.h=0;
	d->buf=NULL;
	d->yuv_msg=NULL;
	d->output_pix_fmt=PIX_FMT_YUV420P;
	f->data=d;
}

static void scrren_capture_uninit(MSFilter *f){
	ScreenCapData *d=(ScreenCapData*)f->data;
	reset_data(d);

	win32grab_read_close(d);

	if (d->yuv_msg!=NULL) freemsg(d->yuv_msg);
	if (d->sws_ctx!=NULL){
		ms_sws_freeContext(d->sws_ctx);
		d->sws_ctx=NULL;
	}
	if(d->buf!=NULL) ms_free(d->buf);

	ms_free(f->data);
}

static void scrren_capture_preprocess(MSFilter *f){
	ScreenCapData *d=(ScreenCapData*)f->data;
	win32grab_init(d);
	d->starttime=f->ticker->time;
}

static mblk_t *get_as_yuvmsg(MSFilter *f, ScreenCapData *s, AVFrame *orig){
	if (s->vsize.width==0 || s->vsize.height==0){
		ms_error("%s: wrong image size provided by decoder.",f->desc->name);
		return NULL;
	}
	if (orig->data[0]==NULL){
		ms_error("%s: no image data.",f->desc->name);
		return NULL;
	}
	if (s->outbuf.w!=s->vsize.width || s->outbuf.h!=s->vsize.height){
		if (s->sws_ctx!=NULL){
			ms_sws_freeContext(s->sws_ctx);
			s->sws_ctx=NULL;
		}
		s->yuv_msg=ms_yuv_buf_alloc(&s->outbuf,s->vsize.width,s->vsize.height);
		s->outbuf.w=s->vsize.width;
		s->outbuf.h=s->vsize.height;
		s->sws_ctx=ms_sws_getContext(s->vsize.width,s->vsize.height,s->input_pix_fmt,
			s->vsize.width,s->vsize.height,s->output_pix_fmt,SWS_FAST_BILINEAR,
			NULL, NULL, NULL);
	}
	if (s->sws_ctx==NULL){
		ms_error("%s: missing rescaling context.",f->desc->name);
		return NULL;
	}
	if (ms_sws_scale(s->sws_ctx,orig->data,orig->linesize, 0,
		s->vsize.height, s->outbuf.planes, s->outbuf.strides)<0){
			ms_error("%s: error in ms_sws_scale().",f->desc->name);
	}
	return dupmsg(s->yuv_msg);
}

static void scrren_capture_process(MSFilter *f){
	ScreenCapData *d=(ScreenCapData*)f->data;
	mblk_t *om=NULL;

	float elapsed=(float)(f->ticker->time-d->starttime);

	if ((elapsed*d->fps/1000.0)>d->index){

		AVFrame pict;
		int got_picture=0;
		int frame_len=0;
		int iPacketNum = 0;

		int ret = win32grab_read_packet(d,d->buf);

		avcodec_get_frame_defaults(&pict);
		avpicture_fill((AVPicture*)&pict,(uint8_t*)d->buf,d->input_pix_fmt,d->vsize.width,d->vsize.height);
		
		if (ret==0) {
			om = get_as_yuvmsg(f,d,&pict);
		}

		if(om!=NULL) ms_queue_put(f->outputs[0],om);
		d->index++;
	}

}

static void scrren_capture_postprocess(MSFilter *f){
	ScreenCapData *d=(ScreenCapData*)f->data;
}

static void reset_data(ScreenCapData *d)
{
	d->iVideoStream = -1;
}

static int scrren_capture_set_vsize(MSFilter *f, void* data){
	ScreenCapData *d=(ScreenCapData*)f->data;
	d->vsize=*(MSVideoSize*)data;
	return 0;
}

static int scrren_capture_set_fps(MSFilter *f, void* data){
	ScreenCapData *d=(ScreenCapData*)f->data;
	d->fps=*(float*)data;
	return 0;
}

static int scrren_capture_get_fmt(MSFilter *f, void* data){
	ScreenCapData *d=(ScreenCapData*)f->data;
	*(MSPixFmt*)data=ffmpeg_pix_fmt_to_ms(d->output_pix_fmt);
	return 0;
}

static MSFilterMethod scrren_capture_methods[]={
	{	MS_FILTER_SET_VIDEO_SIZE, scrren_capture_set_vsize },
	{	MS_FILTER_SET_FPS	, scrren_capture_set_fps	},
	{	MS_FILTER_GET_PIX_FMT	, scrren_capture_get_fmt	},
	{	0,0 }
};

MSFilterDesc ms_scrren_capture_desc={
	MS_VIDEO_FILE_PLAYER_ID,
	"MSVideoPlayer",
	"MP4, MPEG, AVI Video player",
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	scrren_capture_init,
	scrren_capture_preprocess,
	scrren_capture_process,
	scrren_capture_postprocess,
	scrren_capture_uninit,
	scrren_capture_methods
};

MS_FILTER_DESC_EXPORT(ms_scrren_capture_desc)

static void scrren_capture_detect(MSWebCamManager *obj);

static void scrren_capture_cam_init(MSWebCam *cam){
	cam->name=ms_strdup("Screen Capture");
}


static MSFilter *scrren_capture_create_reader(MSWebCam *obj){
	return ms_filter_new_from_desc(&ms_scrren_capture_desc);
}

extern "C" MSWebCamDesc scrren_capture_desc={
	"ScreenCapture",
	&scrren_capture_detect,
	&scrren_capture_cam_init,
	&scrren_capture_create_reader,
	NULL
};

static void scrren_capture_detect(MSWebCamManager *obj){
	MSWebCam *cam=ms_web_cam_new(&scrren_capture_desc);
	ms_web_cam_manager_add_cam(obj,cam);
}

