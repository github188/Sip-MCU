
// ScreenRecorderDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "ScreenRecorder.h"
#include "ScreenRecorderDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mssndcard.h"


extern "C" void conf_itc_init(void);

#ifdef VIDEO_ENABLED
extern void libmsx264_init(void);
#endif

static int mediastream_init(){
	conf_itc_init();
	ms_init();
	return 0;
}

#include "video_mail_record.h"
static VideoMailRecord *os =NULL;


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CScreenRecorderDlg 对话框




CScreenRecorderDlg::CScreenRecorderDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CScreenRecorderDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CScreenRecorderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_FILE_NAME, mFileName);
	DDX_Control(pDX, IDC_EDIT_PRE_VIEW, mWndPreView);
	DDX_Control(pDX, IDC_BUTTON_CAPTURE, mRecBtn);
}

BEGIN_MESSAGE_MAP(CScreenRecorderDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON_CAPTURE, &CScreenRecorderDlg::OnBnClickedButtonCapture)
END_MESSAGE_MAP()


// CScreenRecorderDlg 消息处理程序

BOOL CScreenRecorderDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	mediastream_init();
	mFileName.SetWindowText("screen_recoder_01.mp4");

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CScreenRecorderDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CScreenRecorderDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CScreenRecorderDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CScreenRecorderDlg::OnBnClickedButtonCapture()
{
	
	if(os==NULL){

		CString device("Screen Capture");

		MSWebCam *cam_dev = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),device.GetString());
		MSSndCard *default_snd = ms_snd_card_manager_get_default_card(ms_snd_card_manager_get());

		CString filename=_T("outputrecord.mp4");
		int ret=-1;
		os = video_mail_record_new();
		video_mail_record_set_rate(os,16000);
		video_mail_record_set_nchannels(os,2);

		mFileName.GetWindowText(filename);

		{
			HWND    window_handle; /**< handle of the window for the grab */
			HDC     source_hdc;    /**< Source  device context */
			MSVideoSize vsize;

			video_mail_record_set_bit_rate(os,768);

			window_handle =  ::GetDesktopWindow();
			source_hdc = ::GetDC(window_handle);
			if (NULL == source_hdc){
				return;
			}

			vsize.width = GetDeviceCaps(source_hdc, HORZRES);
			vsize.height = GetDeviceCaps(source_hdc, VERTRES);

			video_mail_record_set_video_size(os,vsize);
			video_mail_record_set_fps(os,8.0);

		}


		ret = video_mail_record_start(os,default_snd,cam_dev,(unsigned long)mWndPreView.m_hWnd,filename.GetString());

		if(ret<0)
		{
			CString tmp;
			tmp.Format("无法打开文件: %s",filename);
			MessageBox(tmp);

			video_mail_record_stop(os);
			os =NULL;

			return;
		}

		video_mail_record_start_recording(os,TRUE);

		mRecBtn.SetWindowText("停止录制");
	}else{
		video_mail_record_stop(os);
		os =NULL;
		mRecBtn.SetWindowText("开始录制");
	}	
}
