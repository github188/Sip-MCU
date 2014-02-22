/** Copyright 2007 Simon Morlat, all rights reserved **/

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/msv4l.h>
#include <mediastreamer2/mstee.h>
#include <mediastreamer2/msvideoout.h>
#include <mediastreamer2/mswebcam.h>
int main(int argc, char *argv[]){
#if defined(WIN32)
	int count;
#endif
	MSFilter *source,*pixconv,*sizeconv,*tee,*stitcher,*output;
	MSVideoSize vsize;
	MSTicker *ticker;
	int format;
	int i;
	int tmp;

	ms_init();
	source=ms_web_cam_create_reader(ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),"Directshow capture: Built-in iSight"));//ms_filter_new(MS_DSCAP_ID);
	tee=ms_filter_new(MS_TEE_ID);
	stitcher=ms_filter_new_from_name("MSVideoMixer");
	if (stitcher==NULL){
		ms_error("Fail to create stitcher: plugin not loaded ?");
		return -1;
	}
	output=ms_filter_new(MS_DRAWDIB_DISPLAY_ID);
	//output1=ms_filter_new(MS_VIDEO_OUT_ID);
	//output2=ms_filter_new(MS_VIDEO_OUT_ID);
	sizeconv=ms_filter_new(MS_SIZE_CONV_ID);

	vsize.width=MS_VIDEO_SIZE_VGA_W;
	vsize.height=MS_VIDEO_SIZE_VGA_H;
	ms_filter_call_method(source,MS_FILTER_SET_VIDEO_SIZE,&vsize);

	vsize.width=MS_VIDEO_SIZE_1024_W;
	vsize.height=MS_VIDEO_SIZE_1024_H;
	ms_filter_call_method(sizeconv,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	
	ms_filter_call_method(stitcher,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	ms_filter_call_method(output,MS_FILTER_SET_VIDEO_SIZE,&vsize);
//	ms_filter_call_method(output1,MS_FILTER_SET_VIDEO_SIZE,&vsize);
//	ms_filter_call_method(output2,MS_FILTER_SET_VIDEO_SIZE,&vsize);
//	ms_filter_call_method_noarg(source,MS_V4L_START);
	/* get the output format for webcam reader */
	ms_filter_call_method(source,MS_FILTER_GET_PIX_FMT,&format);
	if (format==MS_MJPEG){
		pixconv=ms_filter_new(MS_MJPEG_DEC_ID);
	}else{
		pixconv = ms_filter_new(MS_PIX_CONV_ID);
		/*set it to the pixconv */
		vsize.width=MS_VIDEO_SIZE_VGA_W;
		vsize.height=MS_VIDEO_SIZE_VGA_H;
		ms_filter_call_method(pixconv,MS_FILTER_SET_PIX_FMT,&format);
		ms_filter_call_method(pixconv,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	}

	tmp = -1;
//	ms_filter_call_method(output,MS_VIDEO_OUT_SET_CORNER,&tmp);
//	ms_filter_call_method(output1,MS_VIDEO_OUT_SET_CORNER,&tmp);
//	ms_filter_call_method(output2,MS_VIDEO_OUT_SET_CORNER,&tmp);

	
	ms_filter_link(source,0,pixconv,0);
	ms_filter_link(pixconv,0,sizeconv,0);
	ms_filter_link(sizeconv,0,tee,0);
	ms_filter_link(tee,0,stitcher,0);
	ms_filter_link(tee,1,stitcher,1);
	ms_filter_link(tee,2,stitcher,2);
	ms_filter_link(tee,3,stitcher,3);
	ms_filter_link(tee,4,stitcher,4);
	ms_filter_link(tee,5,stitcher,5);
	ms_filter_link(tee,6,stitcher,6);
	ms_filter_link(tee,7,stitcher,7);
	ms_filter_link(tee,8,stitcher,8);
	ms_filter_link(stitcher,0,output,0);
//	ms_filter_link(stitcher,1,output1,0);
 	//ms_filter_link(stitcher,2,output2,0);
	//mute everything but 0:
	for(i=1;i<9;++i)
		ms_filter_call_method(tee,MS_TEE_MUTE,&i);

	ticker=ms_ticker_new();
	ms_ticker_attach(ticker,source);

	/*add new region every five seconds*/
	
#if defined(WIN32)
  count=0;
  while (count<250)
  {
	  MSG msg;
	  BOOL fGotMessage;
	  if((fGotMessage = PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) != 0)
		{
		  TranslateMessage(&msg); 
		  DispatchMessage(&msg);
		}
	  count++;
	  Sleep(20);
  }
#else
	ms_sleep(5);
#endif

	i=1;
	ms_filter_call_method(tee,MS_TEE_UNMUTE,&i);

	i=2;
	ms_filter_call_method(tee,MS_TEE_UNMUTE,&i);
#if defined(WIN32)
  count=0;
  while (count<250)
  {
	  MSG msg;
	  BOOL fGotMessage;
	  if((fGotMessage = PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) != 0)
		{
		  TranslateMessage(&msg); 
		  DispatchMessage(&msg);
		}
	  count++;
	  Sleep(20);
  }
#else
	ms_sleep(5);
#endif

	i=3;
	ms_filter_call_method(tee,MS_TEE_UNMUTE,&i);
#if defined(WIN32)
	count=0;
	while (count<250)
	{
		MSG msg;
		BOOL fGotMessage;
		if((fGotMessage = PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) != 0)
		{
			TranslateMessage(&msg); 
			DispatchMessage(&msg);
		}
		count++;
		Sleep(20);
	}
#else
	ms_sleep(5);
#endif
	i=4;
	ms_filter_call_method(tee,MS_TEE_UNMUTE,&i);

#if defined(WIN32)
  count=0;
  while (count<250)
  {
	  MSG msg;
	  BOOL fGotMessage;
	  if((fGotMessage = PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) != 0)
		{
		  TranslateMessage(&msg); 
		  DispatchMessage(&msg);
		}
	  count++;
	  Sleep(20);
  }
#else
	ms_sleep(5);
#endif

	i=5;
	ms_filter_call_method(tee,MS_TEE_UNMUTE,&i);
#if defined(WIN32)
	count=0;
	while (count<250)
	{
		MSG msg;
		BOOL fGotMessage;
		if((fGotMessage = PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) != 0)
		{
			TranslateMessage(&msg); 
			DispatchMessage(&msg);
		}
		count++;
		Sleep(20);
	}
#else
	ms_sleep(5);
#endif
	i=6;
	ms_filter_call_method(tee,MS_TEE_UNMUTE,&i);

#if defined(WIN32)
  count=0;
  while (count<250)
  {
	  MSG msg;
	  BOOL fGotMessage;
	  if((fGotMessage = PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) != 0)
		{
		  TranslateMessage(&msg); 
		  DispatchMessage(&msg);
		}
	  count++;
	  Sleep(20);
  }
#else
	ms_sleep(5);
#endif

	i=7;
	ms_filter_call_method(tee,MS_TEE_UNMUTE,&i);
#if defined(WIN32)
	count=0;
	while (count<250)
	{
		MSG msg;
		BOOL fGotMessage;
		if((fGotMessage = PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) != 0)
		{
			TranslateMessage(&msg); 
			DispatchMessage(&msg);
		}
		count++;
		Sleep(20);
	}
#else
	ms_sleep(5);
#endif
	i=8;
	ms_filter_call_method(tee,MS_TEE_UNMUTE,&i);

#if defined(WIN32)
	count=0;
	while (count<250)
	{
		MSG msg;
		BOOL fGotMessage;
		if((fGotMessage = PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) != 0)
		{
			TranslateMessage(&msg); 
			DispatchMessage(&msg);
		}
		count++;
		Sleep(20);
	}
#else
	ms_sleep(5);
#endif
	ms_ticker_detach(ticker,source);
	ms_ticker_destroy(ticker);
	ms_filter_unlink(source,0,pixconv,0);
	ms_filter_unlink(pixconv,0,tee,0);
	ms_filter_unlink(tee,0,stitcher,0);
	ms_filter_unlink(tee,1,stitcher,1);
	ms_filter_unlink(tee,2,stitcher,2);
	ms_filter_unlink(tee,3,stitcher,3);
	ms_filter_unlink(stitcher,0,output,0);
	
	ms_filter_destroy(source);
	ms_filter_destroy(pixconv);
	ms_filter_destroy(tee);
	ms_filter_destroy(stitcher);
	ms_filter_destroy(output);
	return 0;
}
