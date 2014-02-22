#pragma once

#ifndef YUV_RENDER_H
#define YUV_RENDER_H

#include <ddraw.h>

class CYUVRender
{
public:
	CYUVRender();
	virtual ~CYUVRender();
public:
	BOOL DDrawDispInit(HWND hWnd, int src_width, int src_height, int dst_width, int dst_height);
	void DDrawDispReinit(HWND hWnd, int src_width, int src_height, int dst_width, int dst_height);
	void DDrawDispDone();
	void DDrawUpdateDisp(unsigned char *src[3], int stride[3]);

protected:

protected:
	int m_src_width;
	int m_src_height;
	int m_dst_top;
	int m_dst_left;
	int m_dst_width;
	int m_dst_height;

	HWND m_hWnd;
	LPDIRECTDRAW m_lpDD; // DirectDraw 对象指针
	LPDIRECTDRAWSURFACE m_lpDDSPrimary; // DirectDraw 主表面指针
	LPDIRECTDRAWSURFACE m_lpDDSOverlay; // DirectDraw 离屏表面指针

};

#endif