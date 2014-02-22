#include <windows.h>
#include "YUVRender.h"

#pragma comment(lib,"ddraw.lib")

static DDPIXELFORMAT ddpfOverlayFormats[] = {
	{sizeof(DDPIXELFORMAT), DDPF_FOURCC, MAKEFOURCC('Y','V','1','2'),0,0,0,0,0}, // YV12
	{sizeof(DDPIXELFORMAT), DDPF_FOURCC, MAKEFOURCC('U','Y','V','Y'),0,0,0,0,0}, // UYVY
	//{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x7C00, 0x03e0, 0x001F, 0}, 
	// 16-bit RGB 5:5:5
	//{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0xF800, 0x07e0, 0x001F, 0} 
	// 16-bit RGB 5:6:5
};

#define PF_TABLE_SIZE (sizeof(ddpfOverlayFormats) / sizeof(ddpfOverlayFormats[0]))


CYUVRender::CYUVRender()
{

}

CYUVRender::~CYUVRender()
{
	DDrawDispDone();
}

BOOL CYUVRender::DDrawDispInit(HWND hWnd, int src_width, int src_height,

									int dst_width, int dst_height)
{
	RECT rect;

	GetClientRect(hWnd, &rect);
	// rect.right = GetSystemMetrics(SM_CXSCREEN);
	// rect.bottom = GetSystemMetrics(SM_CYSCREEN);

	m_src_width = src_width;
	m_src_height = src_height;
	m_hWnd = hWnd;

	if (dst_width > rect.right)
		dst_width = rect.right;
	if (dst_height > rect.bottom)
		dst_height = rect.bottom;

	if (dst_width == -1) // Full Screen
	{
		if (rect.right*src_height/src_width > rect.bottom)
		{
			dst_height = rect.bottom;
			dst_width = rect.bottom * src_width / src_height;
		}
		else
		{
			dst_width = rect.right;
			dst_height = rect.right * src_height / src_width;
		}
	}

	m_dst_left = (rect.right-dst_width)/2;
	m_dst_top = (rect.bottom-dst_height)/2;
	m_dst_width = dst_width;
	m_dst_height = dst_height;

	// 创建DirectDraw对象
	if ( DirectDrawCreate(NULL, &m_lpDD, NULL) != DD_OK )
		return FALSE;

	// 设置协作层
	if ( m_lpDD->SetCooperativeLevel(hWnd, DDSCL_NORMAL) != DD_OK )
		return FALSE;

	DDSURFACEDESC ddsd; // DirectDraw 表面描述

	// 创建主表面
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS ;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	if ( m_lpDD->CreateSurface(&ddsd, &m_lpDDSPrimary, NULL) != DD_OK )
		return FALSE;

	LPDIRECTDRAWCLIPPER pcClipper; // Cliper
	if( m_lpDD->CreateClipper( 0, &pcClipper, NULL ) != DD_OK )
		return FALSE;

	if( pcClipper->SetHWnd( 0, hWnd ) != DD_OK )
	{
		pcClipper->Release();
		return FALSE;
	}

	if( m_lpDDSPrimary->SetClipper( pcClipper ) != DD_OK )
	{
		pcClipper->Release();
		return FALSE;
	}

	// Done with clipper
	pcClipper->Release();

	// 创建离屏表面对象
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY | DDSCAPS_OVERLAY;
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsd.dwWidth = src_width;
	ddsd.dwHeight = src_height;
	ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC | DDPF_YUV ;

	int i = 0;
	HRESULT hRet;
	do 
	{
		ddsd.ddpfPixelFormat = ddpfOverlayFormats[i];
		hRet = m_lpDD->CreateSurface(&ddsd, &m_lpDDSOverlay, NULL);
	} while (hRet != DD_OK && (++i < PF_TABLE_SIZE));

	if (hRet != DD_OK)
		return FALSE;

	return TRUE;
}

void CYUVRender::DDrawDispReinit(HWND hWnd, int src_width, int src_height,

									  int dst_width, int dst_height)
{
	DDrawDispDone();
	DDrawDispInit(hWnd, src_width, src_height, dst_width, dst_height);
}

void CYUVRender::DDrawDispDone()
{
	if (m_lpDDSOverlay != NULL)
	{
		// Use UpdateOverlay() with the DDOVER_HIDE flag to remove an overlay

		// from the display.
		m_lpDDSOverlay->UpdateOverlay(NULL, m_lpDDSPrimary, NULL, DDOVER_HIDE,
			NULL);
		m_lpDDSOverlay->Release();
		m_lpDDSOverlay = NULL;
	}

	if (m_lpDDSPrimary != NULL)
	{
		m_lpDDSPrimary->Release();
		m_lpDDSPrimary = NULL;
	}

	if (m_lpDD != NULL)
	{
		m_lpDD->Release();
		m_lpDD = NULL;
	}
}

void CYUVRender::DDrawUpdateDisp(unsigned char *src[3], int stride[3])
{
	HRESULT hRet; // DirectDraw 函数返回值
	DDSURFACEDESC ddsd; // DirectDraw 表面描述

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	// Lock down the surface so we can modify it's contents.
	hRet = m_lpDDSOverlay->Lock( NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	//if (FAILED(hRet))
	// return ;

	LPBYTE lpSurf = (LPBYTE)ddsd.lpSurface;

	if(lpSurf) 
	{
		int i;

		// fill Y data
		for(i = 0; i < m_src_height; i++)
		{
			memcpy(lpSurf, src[0], m_src_width);
			src[0] += stride[0];
			lpSurf += ddsd.lPitch;
		}

		// fill V data
		for(i = 0; i < m_src_height / 2; i++)
		{
			memcpy(lpSurf, src[2], m_src_width / 2);
			src[2] += stride[2];
			lpSurf += ddsd.lPitch / 2;
		}

		// fill U data
		for(i = 0; i < m_src_height / 2; i++)
		{
			memcpy(lpSurf, src[1], m_src_width / 2);
			src[1] += stride[1];
			lpSurf += ddsd.lPitch / 2;
		}
	}

	m_lpDDSOverlay->Unlock(NULL);

	RECT rs, rd;
	rs.left = rs.top = 0;
	rs.right = m_src_width;
	rs.bottom = m_src_height;

	rd.left = m_dst_left;
	rd.top = m_dst_top;
	rd.right = m_dst_left + m_dst_width;
	rd.bottom = m_dst_top + m_dst_height;

	//hRet = m_lpDDSOverlay->UpdateOverlay(&rs, m_lpDDSPrimary, &rd, DDOVER_SHOW, NULL);

	hRet = m_lpDDSOverlay->Blt(&rs, m_lpDDSPrimary, &rd, DDBLT_WAIT, NULL);


}