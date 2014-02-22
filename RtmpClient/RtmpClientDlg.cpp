
// RtmpClientDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "RtmpClient.h"
#include "RtmpClientDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define PHONE_CORE_TIMER 100
#define PRINT_LOG 10101
#define FLASHVER "FMLE/3.0\20(compatible;\20FMSc/1.0)"
#define CONFIG_SECTION "rtmp_client"

#define	_RED_		RGB(255,0,0)
#define	_YELLOW_	RGB(255,255,0)
#define _GREEN_		RGB(0,215,0)
#define _BLUE_		RGB(30,30,255)
#define	_WHITE_		RGB(255,255,255)
#define	_BLACK_		RGB(0,0,0)
#define	_GREY_		RGB(130,130,130)

#ifdef __cplusplus
extern "C" {
#endif

#include "linphonecore.h"
#include "linphonecore_utils.h"
#include "video_mail_record.h"

#pragma comment(lib,"libsipua3.lib") 

#include "np_config.h"
#pragma comment(lib,"libconfig.lib")

	static LinphoneCoreVTable vTable;
	static const char *config_file = "rtmp_client.ini";
	static const char *factoryConfig = NULL;//"libsipua3_template.ini";
	static LinphoneCore *the_core=NULL;
	CRtmpClientDlg *the_ui_static;
	static const char *the_cfg_file = "settings.db";
	static NetPhoneConfig *the_cfg=NULL;

	static void linphone_log_handler(OrtpLogLevel lev, const char *fmt, va_list args){
		const char *lname="undef";
		char *msg;
		char *msg_str;
		COLORREF color;

		switch(lev){
			case ORTP_DEBUG:
				color = _BLUE_;
				lname="DEBUG";
				break;
			case ORTP_MESSAGE:
				color = _WHITE_;
				lname="MESSAGE";
				break;
			case ORTP_WARNING:
				lname="WARING";
				color = _YELLOW_;
				break;
			case ORTP_ERROR:
				color = _RED_;
				lname="ERROR";
				break;
			case ORTP_FATAL:
				color = _RED_;
				lname="FATAL";
				break;
			default:
				lname = ("Bad level !");
		}

		msg=ortp_strdup_vprintf(fmt,args);


		msg_str = ortp_strdup_printf("%s: %s",lname,msg);

#ifdef _MSC_VER
		if(the_ui_static)
			the_ui_static->PostMessage(PRINT_LOG,(WPARAM)new CString(msg_str),(WPARAM)color);
		//	the_ui_static->Log(CString(msg_str),color);
#endif

		ms_free(msg);
		ms_free(msg_str);
	}

	/**Call state notification callback prototype*/
	void theGlobalStateCb(struct _LinphoneCore *lc, LinphoneGlobalState gstate, const char *message)
	{

	}
	/**Call state notification callback prototype*/
	void theCallStateCb(struct _LinphoneCore *lc, LinphoneCall *call, LinphoneCallState cstate, const char *message,int code)
	{

		CRtmpClientDlg *the_ui = (CRtmpClientDlg *)linphone_core_get_user_data(lc);

		if(cstate ==LinphoneCallIncomingReceived)
		{
//			linphone_core_terminate_call(the_core,call);
		}
	}
	/** @ingroup Proxies
	* Registration state notification callback prototype
	* */
	void theRegistrationStateCb(struct _LinphoneCore *lc, LinphoneProxyConfig *cfg, LinphoneRegistrationState cstate, const char *message,int code)
	{

	}
	/** Callback prototype */
	void theShowInterfaceCb(struct _LinphoneCore *lc)
	{
		CRtmpClientDlg *the_ui = (CRtmpClientDlg *)linphone_core_get_user_data(lc);

		if(the_ui)
			the_ui->ShowWindow(SW_SHOW);
	}
	/** Callback prototype */
	void theDisplayStatusCb(struct _LinphoneCore *lc, const char *message)
	{
		CRtmpClientDlg *the_ui = (CRtmpClientDlg *)linphone_core_get_user_data(lc);
#ifdef _MSC_VER
		OutputDebugString(message);
#endif
		if(the_ui)
			the_ui->Log(message,_GREEN_);
	}
	/** Callback prototype */
	void theDisplayMessageCb(struct _LinphoneCore *lc, const char *message)
	{

	}
	/** Callback prototype */
	void theDisplayUrlCb(struct _LinphoneCore *lc, const char *message, const char *url)
	{

	}
	/** Callback prototype */
	void theCoreCbFunc(struct _LinphoneCore *lc,void * user_data)
	{

	}
	/** Callback prototype */
	void theNotifyReceivedCb(struct _LinphoneCore *lc, LinphoneCall *call, const char *from, const char *event)
	{

	}
	/**
	* Report status change for a friend previously \link linphone_core_add_friend() added \endlink to #LinphoneCore.
	* @param lc #LinphoneCore object .
	* @param lf Updated #LinphoneFriend .
	*/
	void theNotifyPresenceReceivedCb(struct _LinphoneCore *lc, LinphoneFriend * lf)
	{

	}
	/**
	*  Reports that a new subscription request has been received and wait for a decision.
	*  <br> Status on this subscription request is notified by \link linphone_friend_set_inc_subscribe_policy() changing policy \endlink for this friend
	*	@param lc #LinphoneCore object
	*	@param lf #LinphoneFriend corresponding to the subscriber
	*	@param url of the subscriber
	*  Callback prototype
	*  */
	void theNewSubscribtionRequestCb(struct _LinphoneCore *lc, LinphoneFriend *lf, const char *url)
	{

	}
	/** Callback prototype */
	void theAuthInfoRequested(struct _LinphoneCore *lc, const char *realm, const char *username)
	{

	}
	/** Callback prototype */
	void theCallLogUpdated(struct _LinphoneCore *lc, struct _LinphoneCallLog *newcl)
	{

	}
	/**
	* Callback prototype
	*
	* @param lc #LinphoneCore object
	* @param room #LinphoneChatRoom involved in this conversation. Can be be created by the framework in case \link #LinphoneAddress the from \endlink is not present in any chat room.
	* @param from #LinphoneAddress from
	* @param message incoming message
	*  */
	void theTextMessageReceived(LinphoneCore *lc, LinphoneChatRoom *room, const LinphoneAddress *from, const char *message)
	{

	}
	/** Callback prototype */
	void theDtmfReceived(struct _LinphoneCore* lc, LinphoneCall *call, int dtmf)
	{
		CRtmpClientDlg *the_ui = (CRtmpClientDlg *)linphone_core_get_user_data(lc);
		CString tmp;

		tmp.Format("收到 %s, DTMF 按键 %c",linphone_call_get_remote_address_as_string(call),dtmf);
		if(the_ui)
			the_ui->Log(tmp,_GREEN_);
	}
	/** Callback prototype */
	void theReferReceived(struct _LinphoneCore *lc, const char *refer_to)
	{

	}
	/** Callback prototype */
	void theBuddyInfoUpdated(struct _LinphoneCore *lc, LinphoneFriend *lf)
	{

	}


#ifdef __cplusplus
}
#endif

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


// CRtmpClientDlg 对话框




CRtmpClientDlg::CRtmpClientDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRtmpClientDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRtmpClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_SERVER_IP, mServerIP);
	DDX_Control(pDX, IDC_CHECK_APPEND, mChkAppend);
	DDX_Control(pDX, IDC_CHECK_RECORD, mChkRecord);
	DDX_Control(pDX, IDC_EDIT_PASSWD, mPubPasswd);
	DDX_Control(pDX, IDC_EDIT_USER, mPubUser);
	DDX_Control(pDX, IDC_EDIT_STREAM_NAME, mStreamName);
	DDX_Control(pDX, IDC_CHECK_LIVE, mChkLive);
	DDX_Control(pDX, IDC_EDIT_REVIEW, mVideoView);
	DDX_Control(pDX, IDC_COMBO_CAMERA, mCamera);
	DDX_Control(pDX, IDC_COMBO_SOUNDCARD, mMicphone);
	DDX_Control(pDX, IDC_COMBO_SIZE_BW, mBandWidth);
	DDX_Control(pDX, IDC_CHECK_VIDEO_PREVIEW, mChkPreView);
	DDX_Control(pDX, IDC_BUTTON_PUBLISH, mBntPublish);
	DDX_Control(pDX, IDC_SLIDER1, mInputValue);
	DDX_Control(pDX, IDC_RICHEDIT_DEBUG, m_RichEdit);
	DDX_Control(pDX, IDC_COMBO1, mFpsList);
}

BEGIN_MESSAGE_MAP(CRtmpClientDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_WM_TIMER()
	ON_MESSAGE(PRINT_LOG,OnLogMsgHandler)
	ON_WM_HSCROLL()
	ON_CBN_SELCHANGE(IDC_COMBO_SIZE_BW, &CRtmpClientDlg::OnCbnSelchangeComboSizeBw)
	ON_BN_CLICKED(IDC_CHECK_VIDEO_PREVIEW, &CRtmpClientDlg::OnBnClickedCheckVideoPreview)
	ON_BN_CLICKED(IDC_BUTTON_PUBLISH, &CRtmpClientDlg::OnBnClickedButtonPublish)
	ON_CBN_SELCHANGE(IDC_COMBO_CAMERA, &CRtmpClientDlg::OnCbnSelchangeComboCamera)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDER1, &CRtmpClientDlg::OnNMReleasedcaptureSlider1)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CRtmpClientDlg 消息处理程序

BOOL CRtmpClientDlg::OnInitDialog()
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

	// TODO: 在此添加额外的初始化代码
	m_RichEdit.SetBackgroundColor(FALSE,_BLACK_);

	//第一步打开配置文件
	the_cfg = np_config_new(the_cfg_file);
	
	if(!the_cfg) Log("无法打开配置文件!",_RED_);

	in_pulishing = false;

	the_ui_static = this;

	linphone_core_enable_logs_with_cb(linphone_log_handler);

	memset(&vTable,0,sizeof(vTable));
	vTable.show = theShowInterfaceCb;
	vTable.call_state_changed = theCallStateCb;
	vTable.auth_info_requested = theAuthInfoRequested;
	vTable.display_status = theDisplayStatusCb;
	vTable.display_message = theDisplayMessageCb;
	vTable.display_warning = theDisplayMessageCb;
	vTable.global_state_changed = theGlobalStateCb;
	vTable.new_subscription_request = theNewSubscribtionRequestCb;
	vTable.notify_presence_recv = theNotifyPresenceReceivedCb;
	vTable.buddy_info_updated=theBuddyInfoUpdated;
	vTable.notify_recv=theNotifyReceivedCb;
	vTable.text_received=theTextMessageReceived;
	vTable.dtmf_received=theDtmfReceived;
	vTable.call_log_updated=theCallLogUpdated;
	vTable.refer_received=theReferReceived;
	vTable.registration_state_changed=theRegistrationStateCb;


	the_core = linphone_core_new(&vTable
		,config_file
		,factoryConfig
		,this);

	if (the_core)
	{
		SetTimer(PHONE_CORE_TIMER,30,NULL);
	}

	linphone_core_enable_video(the_core,TRUE,TRUE);
	linphone_core_enable_video_preview(the_core,FALSE);
	linphone_core_enable_self_view(the_core,FALSE);
	linphone_core_set_native_preview_window_id(the_core,(unsigned long)mVideoView.m_hWnd);
	linphone_core_set_native_video_window_id(the_core,(unsigned long)mVideoView.m_hWnd);


	linphone_core_set_sip_port(the_core,6060);
	linphone_core_set_playback_gain_db(the_core,1.0);

	linphone_core_set_download_bandwidth(the_core,135);
	linphone_core_set_upload_bandwidth(the_core,135);
	linphone_core_set_audio_jittcomp(the_core,120);

	//linphone_core_set_preferred_video_size_by_name(the_core,"VGA");

	{//声卡
		const char **audio_devices=linphone_core_get_sound_devices(the_core);
		const char *cur_capdev = np_config_get_string(the_cfg,CONFIG_SECTION,"mic_dev",linphone_core_get_capture_device(the_core));
		const char *dev = NULL;
		int in_index,out_index;
		int i=0;
		in_index=out_index=0;
		for(;dev=audio_devices[i];i++){
			CString tmp = CString(dev);

			if(linphone_core_sound_device_can_capture(the_core,dev))
			{
				mMicphone.AddString(tmp);
				if(cur_capdev && strlen(cur_capdev) && !strcmp(cur_capdev,tmp))
					mMicphone.SetCurSel(in_index);
				in_index++;
			}

		}

		if(cur_capdev==NULL) mMicphone.SetCurSel(0);

		linphone_core_set_capture_device(the_core,linphone_core_get_capture_device(the_core));
		linphone_core_set_playback_device(the_core,linphone_core_get_playback_device(the_core));
	}


	{//视频
		const char **video_devices=linphone_core_get_video_devices(the_core);
		const char *curdev = np_config_get_string(the_cfg,CONFIG_SECTION,"cam_dev",linphone_core_get_video_device(the_core));
		const char *dev = NULL;

		int i=0;
		int selected = -1;
		for(;dev=video_devices[i];i++){
			CString tmp = CString(dev);
			mCamera.AddString(tmp);
			if(curdev && strlen(curdev) && !strcmp(curdev,tmp))
			{
				selected = i;
				linphone_core_set_video_device(the_core,dev);
			}
		}

		mCamera.SetCurSel(selected);
	}

	mBandWidth.AddString("128K / QCIF(176x144)");
	mBandWidth.AddString("256k / CIF(352x288)");
	mBandWidth.AddString("384k / VGA(640x480)");
	mBandWidth.AddString("512k / VGA(640x480)");
	mBandWidth.AddString("768k / 4CIF(704×576)");
	mBandWidth.AddString("768k / D1 PAL(720×576)");
	mBandWidth.AddString("768k / D1 NTSC(720×480)");
	mBandWidth.AddString("1M / 720P(1280×720)");
	mBandWidth.AddString("2M / 1080P(1920×1080)");

	mBandWidth.SetCurSel(np_config_get_int(the_cfg,CONFIG_SECTION,"cam_bw",3));

	mServerIP.SetWindowText(np_config_get_string(the_cfg,CONFIG_SECTION,"server","localhost"));
	mStreamName.SetWindowText(np_config_get_string(the_cfg,CONFIG_SECTION,"stream","live_pub"));

	mPubUser.SetWindowText(np_config_get_string(the_cfg,CONFIG_SECTION,"username",""));
	mPubPasswd.SetWindowText(np_config_get_string(the_cfg,CONFIG_SECTION,"password",""));


	mInputValue.SetRange(0,100);
	mInputValue.SetPos(np_config_get_int(the_cfg,CONFIG_SECTION,"mic_vol",50));

	int rate = np_config_get_int(the_cfg,CONFIG_SECTION,"mic_rate",16000);
	if(rate == 8000)
		((CButton *)GetDlgItem(IDC_RADIO_8KHZ))->SetCheck(1);
	else if(rate == 16000 )
		((CButton *)GetDlgItem(IDC_RADIO_16KHZ))->SetCheck(1);
	else if(rate == 32000)
		((CButton *)GetDlgItem(IDC_RADIO_32KHZ))->SetCheck(1);
	else
		((CButton *)GetDlgItem(IDC_RADIO_441KHZ))->SetCheck(1);


	mChkRecord.SetCheck(np_config_get_int(the_cfg,CONFIG_SECTION,"record",0));
	mChkAppend.SetCheck(np_config_get_int(the_cfg,CONFIG_SECTION,"append",0));
	mChkLive.SetCheck(np_config_get_int(the_cfg,CONFIG_SECTION,"live",0));


	mFpsList.AddString("1 fps");
	mFpsList.AddString("3 fps");
	mFpsList.AddString("5 fps");
	mFpsList.AddString("8 fps");
	mFpsList.AddString("10 fps");
	mFpsList.AddString("12 fps");
	mFpsList.AddString("15 fps");
	mFpsList.AddString("20 fps");
	mFpsList.AddString("25 fps");
	mFpsList.AddString("30 fps");

	mFpsList.SetCurSel(np_config_get_int(the_cfg,CONFIG_SECTION,"cam_fps",6));

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CRtmpClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CRtmpClientDlg::OnPaint()
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

HCURSOR CRtmpClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CRtmpClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	if(nIDEvent ==PHONE_CORE_TIMER)
	{
		linphone_core_iterate(the_core);
	}
	CDialog::OnTimer(nIDEvent);
}

void CRtmpClientDlg::OnCbnSelchangeComboSizeBw()
{
	int cur_sel = mBandWidth.GetCurSel();

	if(cur_sel ==0)
	{
		linphone_core_set_download_bandwidth(the_core,135);
		linphone_core_set_upload_bandwidth(the_core,135);

		linphone_core_set_preferred_video_size_by_name(the_core,"qcif");
	}else if(cur_sel ==1)
	{
		linphone_core_set_download_bandwidth(the_core,256);
		linphone_core_set_upload_bandwidth(the_core,256);

		linphone_core_set_preferred_video_size_by_name(the_core,"cif");
	}else if(cur_sel == 2)
	{
		linphone_core_set_download_bandwidth(the_core,384);
		linphone_core_set_upload_bandwidth(the_core,384);

		linphone_core_set_preferred_video_size_by_name(the_core,"vga");
	}else if(cur_sel == 3)
	{
		linphone_core_set_download_bandwidth(the_core,512);
		linphone_core_set_upload_bandwidth(the_core,512);

		linphone_core_set_preferred_video_size_by_name(the_core,"vga");
	}else if(cur_sel ==4)
	{
		linphone_core_set_download_bandwidth(the_core,768);
		linphone_core_set_upload_bandwidth(the_core,768);

		linphone_core_set_preferred_video_size_by_name(the_core,"4cif");
	}else if(cur_sel ==5)
	{
		linphone_core_set_download_bandwidth(the_core,768);
		linphone_core_set_upload_bandwidth(the_core,768);

		linphone_core_set_preferred_video_size_by_name(the_core,"D1-PAL");
	}else if(cur_sel ==6)
	{
		linphone_core_set_download_bandwidth(the_core,768);
		linphone_core_set_upload_bandwidth(the_core,768);

		linphone_core_set_preferred_video_size_by_name(the_core,"D1-NTSC");
	}else if(cur_sel ==7)
	{
		linphone_core_set_download_bandwidth(the_core,1024);
		linphone_core_set_upload_bandwidth(the_core,1024);

		linphone_core_set_preferred_video_size_by_name(the_core,"720p");
	}else if(cur_sel ==8)
	{
		linphone_core_set_download_bandwidth(the_core,2048);
		linphone_core_set_upload_bandwidth(the_core,2048);

		linphone_core_set_preferred_video_size_by_name(the_core,"1080p");
	}

	CString tmp;
	MSVideoSize vsize;
	vsize = linphone_core_get_preferred_video_size(the_core);
	int bw = linphone_core_get_upload_bandwidth(the_core);

	tmp.Format("设置尺寸: %dx%d ,带宽: %d kbps",vsize.width,vsize.height,bw);

	Log(tmp,_GREEN_);
}

void CRtmpClientDlg::OnBnClickedCheckVideoPreview()
{
	if(mChkPreView.GetCheck()==1)
	{
		linphone_core_enable_video_preview(the_core,TRUE);
		Log("打开视频预览",_GREEN_);
	}else{
		linphone_core_enable_video_preview(the_core,FALSE);
		Log("关闭视频预览",_GREEN_);
	}
}

void CRtmpClientDlg::OnBnClickedButtonPublish()
{
	if(!in_pulishing)
	{
		//开始推送
		if(mChkPreView.GetCheck()==1)
		{
			linphone_core_enable_video_preview(the_core,FALSE);
			Log("关闭视频预览",_GREEN_);
			mChkPreView.SetCheck(0);
		}
		if(PublishVideoStream()==0)
		{
			Log("成功推送视频",_GREEN_);
			in_pulishing = true;
			mBntPublish.SetWindowText("停止推送");
		}
		
	}else{
		//停止推送

		Log("完成视频推送",_GREEN_);
		UnPublishStream();
		in_pulishing =false;
		mBntPublish.SetWindowText("推送视频");
	}


}

void CRtmpClientDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{

	CSliderCtrl *m_bar=(CSliderCtrl *)pScrollBar;

	if(m_bar== &mInputValue){
		linphone_core_set_rec_level(the_core,m_bar->GetPos());
	}

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

static VideoMailRecord *os =NULL;

int CRtmpClientDlg::PublishVideoStream(void)
{

	CString rtmp_url;
	float fps;
	int fpslist_cur_select=mFpsList.GetCurSel();
	//获取摄像头
	CString device;
	MSWebCam *dev=NULL;
	mCamera.GetWindowText(device);

	bool_t is_screen =false;
	dev = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),device.GetString());

	if(device == _T("Screen Capture"))
		is_screen =true;

	if(fpslist_cur_select==0)
		fps= 1.0;
	else if (fpslist_cur_select==1)
		fps= 3.0;
	else if (fpslist_cur_select==2)
		fps= 5.0;
	else if (fpslist_cur_select==3)
		fps= 8.0;
	else if (fpslist_cur_select==4)
		fps= 10.0;
	else if (fpslist_cur_select==5)
		fps= 12.0;
	else if (fpslist_cur_select==6)
		fps= 15.0;
	else if (fpslist_cur_select==7)
		fps= 20.0;
	else if (fpslist_cur_select==8)
		fps= 25.0;
	else if (fpslist_cur_select==9)
		fps= 30.0;

	//获取采样率
	int rate;
	if(((CButton *)GetDlgItem(IDC_RADIO_8KHZ))->GetCheck())
		rate = 8000;
	else if(((CButton *)GetDlgItem(IDC_RADIO_16KHZ))->GetCheck())
		rate = 16000;
	else if(((CButton *)GetDlgItem(IDC_RADIO_32KHZ))->GetCheck())
		rate = 32000;
	else if(((CButton *)GetDlgItem(IDC_RADIO_441KHZ))->GetCheck())
		rate = 44100;


	//创建url
	CString tmp;
	rtmp_url = "rtmp://";

	mServerIP.GetWindowText(tmp);
	if(tmp.GetLength()<=0) return -1;

	rtmp_url+= tmp;
	rtmp_url+="/live/";

	mStreamName.GetWindowText(tmp);
	if(tmp.GetLength()<=0) return -1;

	rtmp_url+=tmp;
	rtmp_url+=" ";

	rtmp_url+="flashver=FMLE/3.0\\20(compatible;\\20FMSc/1.0)";

	mPubUser.GetWindowText(tmp);
	if(tmp.GetLength()>0){
		rtmp_url+=" ";
		rtmp_url+="pubUser="+tmp;
	}
	

	mPubPasswd.GetWindowText(tmp);
	if(tmp.GetLength()>0)
	{
		rtmp_url+=" ";
		rtmp_url+="pubPasswd="+tmp;
	}

	rtmp_url+=" ";

	if(mChkRecord.GetCheck())
		rtmp_url+="record=1 ";

	if(mChkAppend.GetCheck())
		rtmp_url+="append=1 ";

	if(mChkLive.GetCheck())
		rtmp_url+="live=1 ";

	Log("Publish URL: "+rtmp_url,_GREEN_);


	if(os==NULL){
		int ret=-1;
		os = video_mail_record_new();
		video_mail_record_set_rate(os,rate);
		video_mail_record_set_nchannels(os,2);
		if(is_screen)
		{
			HWND    window_handle; /**< handle of the window for the grab */
			HDC     source_hdc;    /**< Source  device context */
			MSVideoSize vsize;

			video_mail_record_set_bit_rate(os,768);

			window_handle =  ::GetDesktopWindow();
			source_hdc = ::GetDC(window_handle);
			if (NULL == source_hdc){
				return -2;
			}

			vsize.width = GetDeviceCaps(source_hdc, HORZRES);
			vsize.height = GetDeviceCaps(source_hdc, VERTRES);

			video_mail_record_set_video_size(os,vsize);
			video_mail_record_set_fps(os,5.0);

		}else{
			video_mail_record_set_bit_rate(os,linphone_core_get_upload_bandwidth(the_core));
			video_mail_record_set_fps(os,fps);
			video_mail_record_set_video_size(os,linphone_core_get_preferred_video_size(the_core));
		}

		ret = video_mail_record_start(os,linphone_core_sound_get_default_capture_device(the_core),dev,(unsigned long)mVideoView.m_hWnd,rtmp_url.GetString());

		if(ret<0)
		{
			CString tmp;
			tmp.Format("无法推送视频: %s, 错误代码 %d",rtmp_url,ret);
			MessageBox(tmp);
			Log(tmp,_RED_);
			video_mail_record_stop(os);
			os =NULL;

			return -3;
		}

		video_mail_record_start_recording(os,TRUE);
	}else{
		return -4;
	}
	return 0;
}

void CRtmpClientDlg::UnPublishStream(void)
{
	if(os!=NULL)
	{
		video_mail_record_stop(os);
		os =NULL;
	}
}



void CRtmpClientDlg::Log(CString szText,COLORREF color)
{	

	szText = GetTime() + szText;
	szText += "\n";
	int nOldLines = 0, nNewLines = 0, nScroll = 0;
	long nInsertionPoint = 0;
	CHARFORMAT cf;
	// Save number of lines before insertion of new text
	nOldLines = m_RichEdit.GetLineCount();
	// Initialize character format structure
	cf.cbSize = sizeof(CHARFORMAT);
	cf.dwMask = CFM_COLOR;
	cf.dwEffects = 0; // To disable CFE_AUTOCOLOR
	cf.crTextColor = color;
	// Set insertion point to end of text
	nInsertionPoint = m_RichEdit.GetWindowTextLength();
	m_RichEdit.SetSel(nInsertionPoint, -1);
	// Set the character format
	m_RichEdit.SetSelectionCharFormat(cf);
	// Replace selection. Because we have nothing 
	// selected, this will simply insert
	// the string at the current caret position.
	m_RichEdit.ReplaceSel(szText);
	// Get new line count
	nNewLines = m_RichEdit.GetLineCount();
	// Scroll by the number of lines just inserted
	nScroll = nNewLines - nOldLines;

	if(m_bAutoLineScroll)
		m_RichEdit.LineScroll(nScroll);
}

CString CRtmpClientDlg::GetTime()
{
	SYSTEMTIME sTime;
	GetLocalTime(&sTime);
	CString szTime;
	//szTime.Format("[%d-%d-%d %d:%d:%d::%d] ",sTime.wYear,sTime.wMonth,sTime.wDay,sTime.wHour,sTime.wMinute,sTime.wSecond,sTime.wMilliseconds);
	szTime.Format("[%d:%02d:%02d] ",sTime.wHour,sTime.wMinute,sTime.wSecond);//
	return szTime;
}

void CRtmpClientDlg::OnCbnSelchangeComboCamera()
{
	CString device;
	mCamera.GetWindowText(device);
	linphone_core_set_video_device(the_core,device.GetString());
}

void CRtmpClientDlg::OnNMReleasedcaptureSlider1(NMHDR *pNMHDR, LRESULT *pResult)
{
	CString tmp;
	tmp.Format("设置麦克风音量: %d%%",mInputValue.GetPos());
	Log(tmp,_GREEN_);
	*pResult = 0;
}


LRESULT CRtmpClientDlg::OnLogMsgHandler(WPARAM w,LPARAM l)
{
	CString *msg = (CString*)w;
	Log(*msg,(COLORREF)l);
	delete msg;
	return 0; 
}
void CRtmpClientDlg::OnDestroy()
{
	CDialog::OnDestroy();

	if(the_core) linphone_core_destroy(the_core);
	if(the_cfg){
		SaveConfigs();
		if(np_config_needs_commit(the_cfg)) np_config_sync(the_cfg);
		np_config_destroy(the_cfg);
	}
}

void CRtmpClientDlg::SaveConfigs(void)
{
	if(the_cfg)
	{
		CString tmp;
		mPubUser.GetWindowText(tmp);
		np_config_set_string(the_cfg,CONFIG_SECTION,"username",tmp.GetString());

		mPubPasswd.GetWindowText(tmp);
		np_config_set_string(the_cfg,CONFIG_SECTION,"password",tmp.GetString());

		mServerIP.GetWindowText(tmp);
		np_config_set_string(the_cfg,CONFIG_SECTION,"server",tmp.GetString());

		mStreamName.GetWindowText(tmp);
		np_config_set_string(the_cfg,CONFIG_SECTION,"stream",tmp.GetString());

		np_config_set_int(the_cfg,CONFIG_SECTION,"record",mChkRecord.GetCheck());
		np_config_set_int(the_cfg,CONFIG_SECTION,"live",mChkLive.GetCheck());
		np_config_set_int(the_cfg,CONFIG_SECTION,"append",mChkAppend.GetCheck());


		mMicphone.GetWindowText(tmp);
		np_config_set_string(the_cfg,CONFIG_SECTION,"mic_dev",tmp.GetString());

		np_config_set_int(the_cfg,CONFIG_SECTION,"mic_vol",mInputValue.GetPos());

		int rate;
		if(((CButton *)GetDlgItem(IDC_RADIO_8KHZ))->GetCheck())
			rate = 8000;
		else if(((CButton *)GetDlgItem(IDC_RADIO_16KHZ))->GetCheck())
			rate = 16000;
		else if(((CButton *)GetDlgItem(IDC_RADIO_32KHZ))->GetCheck())
			rate = 32000;
		else if(((CButton *)GetDlgItem(IDC_RADIO_441KHZ))->GetCheck())
			rate = 44100;
		np_config_set_int(the_cfg,CONFIG_SECTION,"mic_rate",rate);


		mCamera.GetWindowText(tmp);
		np_config_set_string(the_cfg,CONFIG_SECTION,"cam_dev",tmp.GetString());

		np_config_set_int(the_cfg,CONFIG_SECTION,"cam_bw",mBandWidth.GetCurSel());
		np_config_set_int(the_cfg,CONFIG_SECTION,"cam_fps",mFpsList.GetCurSel());
	}
}

void CRtmpClientDlg::LoadConfigs(void)
{

}
