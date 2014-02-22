
// VideoConfClientDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "VideoConfClient.h"
#include "VideoConfClientDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define PHONE_CORE_TIMER 100
#define PHONE_CALL_TIMER 101
#ifdef __cplusplus
extern "C" {
#endif

#include "linphonecore.h"
#include "linphonecore_utils.h"

#pragma comment(lib,"libsipua3.lib") 

	static LinphoneCoreVTable vTable;
	static const char *config_file = "libsipua3_client.ini";
	static const char *factoryConfig = NULL;//"libsipua3_template.ini";
	static LinphoneCore *the_core=NULL;

	static void linphone_log_handler(OrtpLogLevel lev, const char *fmt, va_list args){
		const char *lname="undef";
		char *msg;
		char *msg_str;

		switch(lev){
			case ORTP_DEBUG:
				lname="DEBUG";
				break;
			case ORTP_MESSAGE:
				lname="MESSAGE";
				break;
			case ORTP_WARNING:
				lname="WARING";
				break;
			case ORTP_ERROR:
				lname="ERROR";
				break;
			case ORTP_FATAL:
				lname="FATAL";
				break;
			default:
				lname = ("Bad level !");
		}


		msg=ortp_strdup_vprintf(fmt,args);


		msg_str = ortp_strdup_printf("%s: %s\r\n",lname,msg);

#ifdef _MSC_VER
		OutputDebugString(msg_str);
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

		CVideoConfClientDlg *the_ui = (CVideoConfClientDlg *)linphone_core_get_user_data(lc);

		if(cstate ==LinphoneCallIncomingReceived)

		{
			linphone_core_accept_call(the_core,linphone_core_get_current_call(the_core));
			//linphone_core_call_enable_udt_video(the_core,linphone_core_get_current_call(the_core),TRUE);
		}
		if(the_ui)
		{ //通话列表操作

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
		CVideoConfClientDlg *the_ui = (CVideoConfClientDlg *)linphone_core_get_user_data(lc);

		if(the_ui)
			the_ui->ShowWindow(SW_SHOW);
	}
	/** Callback prototype */
	void theDisplayStatusCb(struct _LinphoneCore *lc, const char *message)
	{
		CVideoConfClientDlg *the_ui = (CVideoConfClientDlg *)linphone_core_get_user_data(lc);
#ifdef _MSC_VER
		OutputDebugString(message);
#endif
		if(the_ui)
			the_ui->mStatus.SetWindowText(CString(message));
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
		CVideoConfClientDlg *the_ui = (CVideoConfClientDlg *)linphone_core_get_user_data(lc);
		CString tmp;

		tmp.Format("收到 %s, DTMF 按键 %c",linphone_call_get_remote_address_as_string(call),dtmf);
		if(the_ui)
			the_ui->mStatus.SetWindowText(tmp);
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


// CVideoConfClientDlg 对话框




CVideoConfClientDlg::CVideoConfClientDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CVideoConfClientDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CVideoConfClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_INFO, mStatus);
	DDX_Control(pDX, IDC_EDIT_VIDEO_VIEW, mVideoView);
	DDX_Control(pDX, IDC_EDIT_HOST, mHost);
	DDX_Control(pDX, IDC_EDIT_PASSWORD, mPassword);
	DDX_Control(pDX, IDC_EDIT_USERNAME, mUsername);
	DDX_Control(pDX, IDC_COMBO_INPUT, mMicphone);
	DDX_Control(pDX, IDC_COMBO_OUTPUT, mSpeaker);
	DDX_Control(pDX, IDC_COMBO_CAMERA, mCamera);
	DDX_Control(pDX, IDC_CHECK_AEC, mEchoCancel);
	DDX_Control(pDX, IDC_CHECK_AGC2, mAutomicGainControl);
	DDX_Control(pDX, IDC_EDIT1, mDialStr);
	DDX_Control(pDX, IDC_CHECK_URL, mURImode);
	DDX_Control(pDX, IDC_CHECK_SELF_VIEW, mSelfView);
	DDX_Control(pDX, IDC_SLIDER1, mInputValue);
	DDX_Control(pDX, IDC_SLIDER2, mOutputValue);
	DDX_Control(pDX, IDC_COMBO_BANDWITH, mBandWidth);
	DDX_Control(pDX, IDC_CHECK_VIDEO_MODE, mVideoViewMode);
	DDX_Control(pDX, IDC_CHECK_VIDEO_PREVIEW, mVideoPreViewChk);
	DDX_Control(pDX, IDC_CHECK1, mRtmpPublishChk);
	DDX_Control(pDX, IDC_BUTTON_VIDEO_RECORD, mRecrodBnt);
	DDX_Control(pDX, IDC_EDIT2, mRTMP_URL);
}

BEGIN_MESSAGE_MAP(CVideoConfClientDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_BUTTON_REG, &CVideoConfClientDlg::OnBnClickedButtonReg)
	ON_BN_CLICKED(IDC_BUTTON_UNREG, &CVideoConfClientDlg::OnBnClickedButtonUnreg)
	ON_CBN_SELCHANGE(IDC_COMBO_INPUT, &CVideoConfClientDlg::OnCbnSelchangeComboInput)
	ON_CBN_SELCHANGE(IDC_COMBO_OUTPUT, &CVideoConfClientDlg::OnCbnSelchangeComboOutput)
	ON_CBN_SELCHANGE(IDC_COMBO_CAMERA, &CVideoConfClientDlg::OnCbnSelchangeComboCamera)
	ON_BN_CLICKED(IDC_CHECK_AEC, &CVideoConfClientDlg::OnBnClickedCheckAec)
	ON_BN_CLICKED(IDC_CHECK_AGC2, &CVideoConfClientDlg::OnBnClickedCheckAgc2)
	ON_BN_CLICKED(IDC_BUTTON_DIAL, &CVideoConfClientDlg::OnBnClickedButtonDial)
	ON_BN_CLICKED(IDC_BUTTON_HANGUP, &CVideoConfClientDlg::OnBnClickedButtonHangup)
	ON_BN_CLICKED(IDC_CHECK_SELF_VIEW, &CVideoConfClientDlg::OnBnClickedCheckSelfView)
	ON_BN_CLICKED(IDC_BUTTON_CAPTURE_IMAGE, &CVideoConfClientDlg::OnBnClickedButtonCaptureImage)
	ON_BN_CLICKED(IDC_BUTTON_UDT, &CVideoConfClientDlg::OnBnClickedButtonUdt)
	ON_CBN_SELCHANGE(IDC_COMBO_BANDWITH, &CVideoConfClientDlg::OnCbnSelchangeComboBandwith)
	ON_BN_CLICKED(IDC_CHECK_VIDEO_MODE, &CVideoConfClientDlg::OnBnClickedCheckVideoMode)
	ON_BN_CLICKED(IDC_CHECK_VIDEO_PREVIEW, &CVideoConfClientDlg::OnBnClickedCheckVideoPreview)
	ON_BN_CLICKED(IDC_BUTTON_VIDEO_RECORD, &CVideoConfClientDlg::OnBnClickedButtonVideoRecord)
	ON_BN_CLICKED(IDC_CHECK1, &CVideoConfClientDlg::OnBnClickedCheck1)
	ON_BN_CLICKED(IDC_BUTTON_FAST_UPDATE, &CVideoConfClientDlg::OnBnClickedButtonFastUpdate)
END_MESSAGE_MAP()


// CVideoConfClientDlg 消息处理程序

BOOL CVideoConfClientDlg::OnInitDialog()
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

	mURImode.SetCheck(1);

	the_core = linphone_core_new(&vTable
		,config_file
		,factoryConfig
		,this);

	if (the_core)
	{
		SetTimer(PHONE_CORE_TIMER,30,NULL);
	//	SetTimer(PHONE_CALL_TIMER,500,NULL);

	}

	linphone_core_enable_video(the_core,TRUE,TRUE);
	linphone_core_enable_video_preview(the_core,FALSE);
	linphone_core_enable_self_view(the_core,FALSE);
	linphone_core_set_native_preview_window_id(the_core,(unsigned long)mVideoView.m_hWnd);
	linphone_core_set_native_video_window_id(the_core,(unsigned long)mVideoView.m_hWnd);


	linphone_core_set_sip_port(the_core,6060);
	//linphone_core_set_audio_port(the_core,(0xDFF&+random())+1024);
	//linphone_core_set_video_port(the_core,(0xDFF&+random())+1024);
	linphone_core_set_playback_gain_db(the_core,1.0);

	linphone_core_set_download_bandwidth(the_core,512);
	linphone_core_set_upload_bandwidth(the_core,512);
	linphone_core_set_audio_jittcomp(the_core,120);

	//linphone_core_set_stun_server(the_core,"116.92.10.42");
	//linphone_core_set_firewall_policy(the_core,LinphonePolicyUseStun);

	linphone_core_set_preferred_video_size_by_name(the_core,"vga");

	{//声卡
		const char **audio_devices=linphone_core_get_sound_devices(the_core);
		const char *cur_capdev = linphone_core_get_capture_device(the_core);
		const char *cur_playdev = linphone_core_get_playback_device(the_core);
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
			if (linphone_core_sound_device_can_playback(the_core,dev))
			{
				mSpeaker.AddString(tmp);
				if(cur_playdev && strlen(cur_playdev) && !strcmp(cur_playdev,tmp))
					mSpeaker.SetCurSel(out_index);
				out_index++;
			}

		}

		linphone_core_set_capture_device(the_core,linphone_core_get_capture_device(the_core));
		linphone_core_set_playback_device(the_core,linphone_core_get_playback_device(the_core));
	}


	{//视频
		const char **video_devices=linphone_core_get_video_devices(the_core);
		const char *curdev = linphone_core_get_video_device(the_core);
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


	LinphoneAuthInfo *auth=NULL;
	const MSList *list = linphone_core_get_auth_info_list(the_core);

	if(list!=NULL)
	{
		auth = (LinphoneAuthInfo *)list->data;
		mUsername.SetWindowText(linphone_auth_info_get_username(auth));
		mPassword.SetWindowText(linphone_auth_info_get_passwd(auth));

	}else
	{
		mUsername.SetWindowText("203");
		mPassword.SetWindowText("203");
	}

	mDialStr.SetWindowText("443");

	LinphoneProxyConfig *proxy=NULL;

	linphone_core_get_default_proxy(the_core,&proxy);

	if(proxy!=NULL)
	{
		CString tmp;
		tmp.Format("%s",linphone_proxy_config_get_addr(proxy));
		tmp.Replace("sip:","");
		mHost.SetWindowText(tmp.GetString());
	}else
	{
		mHost.SetWindowText("192.168.1.200");
	}

	mSelfView.SetCheck(0);

	mInputValue.SetPos((50));
	mOutputValue.SetPos((50));

	{
		char resault[LINPHONE_IPADDR_SIZE];
		CString tmp;
		linphone_core_get_local_ip(the_core,"58.213.155.7",resault);
		tmp.Format("本机URL: sip:%s:%d",resault,linphone_core_get_sip_port(the_core));
		mStatus.SetWindowText(tmp);
	}

	mBandWidth.AddString("128K / QCIF(176x144)");
	mBandWidth.AddString("256k / CIF(352x288)");
	mBandWidth.AddString("384k / VGA(640x480)");
	mBandWidth.AddString("512k / VGA(640x480)");
	mBandWidth.AddString("768k / 4CIF(704×576)");
	mBandWidth.AddString("1M / 720P(1280×720)");
	mBandWidth.AddString("2M / 720P(1280×720)");

	mBandWidth.SetCurSel(3);

	mRTMP_URL.SetWindowText("rtmp://localhost/live/cam");
	mRTMP_URL.EnableWindow(FALSE);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE

}

void CVideoConfClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CVideoConfClientDlg::OnPaint()
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
HCURSOR CVideoConfClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CVideoConfClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	if(nIDEvent ==PHONE_CORE_TIMER)
	{
		linphone_core_iterate(the_core);
	}

	if(nIDEvent == PHONE_CALL_TIMER)
	{
		//	if(linphone_core_in_call(the_core))
		{
			CString tmp;
			const MSList *call_list = linphone_core_get_calls(the_core);
//			const MSList *elem;
// 			int i = 0;
// 			for(elem=call_list;elem!=NULL;elem=elem->next){
// 				LinphoneCall *call=(LinphoneCall *)elem->data;
// 				LinphoneCallState cs=linphone_call_get_state(call);
// 				tmp.Format("%s",linphone_call_state_to_string(cs));
// 				mCallList.SetItemText(i,2,tmp);
// 				tmp.Format("%s",Sec2Time(linphone_call_get_duration(call)));
// 				mCallList.SetItemText(i,3,tmp);
// 
// 				i++;
// 			}
		}
	}

	CDialog::OnTimer(nIDEvent);
}

void CVideoConfClientDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	linphone_core_terminate_all_calls(the_core);
	linphone_core_clear_proxy_config(the_core);
	linphone_core_clear_all_auth_info(the_core);
	KillTimer(PHONE_CORE_TIMER);
	KillTimer(PHONE_CALL_TIMER);
	linphone_core_destroy(the_core);
	the_core=NULL;

	CDialog::OnClose();
}

void CVideoConfClientDlg::OnBnClickedButtonReg()
{
	CString username,password,server,szidentity;

	mUsername.GetWindowText(username);
	mPassword.GetWindowText(password);
	mHost.GetWindowText(server);

	if (username.IsEmpty() || password.IsEmpty() || server.IsEmpty())
	{
		return;
	}

	szidentity.Format("sip:%s@%s",username.GetString(),server.GetString());

	const char *identity = szidentity.GetString();
	const char *secret = password.GetString();
	const char *host = server.GetString();

	LinphoneProxyConfig* proxyCfg=NULL;	
	//get default proxy
	linphone_core_get_default_proxy(the_core,&proxyCfg);

	if (proxyCfg!=NULL)
	{
		if (linphone_proxy_config_is_registered(proxyCfg))
		{
			MessageBox("已注册到服务器！","提示");
			return;
		}else{
			linphone_core_clear_proxy_config(the_core);
		}

	}

	proxyCfg = linphone_proxy_config_new();

	// add username password
	LinphoneAddress *from = linphone_address_new(identity);
	LinphoneAuthInfo *info;
	if (from !=0){
		info=linphone_auth_info_new(linphone_address_get_username(from),NULL,secret,NULL,NULL);
		linphone_core_add_auth_info(the_core,info);
	}
	linphone_address_destroy(from);

	// configure proxy entries
	linphone_proxy_config_set_identity(proxyCfg,identity);
	linphone_proxy_config_set_server_addr(proxyCfg,host);
	linphone_proxy_config_enable_register(proxyCfg,true);


	linphone_proxy_config_set_dial_escape_plus(proxyCfg,TRUE);

	linphone_core_add_proxy_config(the_core,proxyCfg);
	//set to default proxy
	linphone_core_set_default_proxy(the_core,proxyCfg);
}

void CVideoConfClientDlg::OnBnClickedButtonUnreg()
{
	linphone_core_clear_proxy_config(the_core);
}

void CVideoConfClientDlg::OnCbnSelchangeComboInput()
{
	CString device;
	mMicphone.GetWindowText(device);
	linphone_core_set_capture_device(the_core,device.GetString());
}

void CVideoConfClientDlg::OnCbnSelchangeComboOutput()
{
	CString device;
	mSpeaker.GetWindowText(device);
	linphone_core_set_playback_device(the_core,device.GetString());
}

void CVideoConfClientDlg::OnCbnSelchangeComboCamera()
{
	CString device;
	mCamera.GetWindowText(device);
	linphone_core_set_video_device(the_core,device.GetString());
}

static bool_t aec = FALSE;

void CVideoConfClientDlg::OnBnClickedCheckAec()
{
	if(mEchoCancel.GetCheck() ==1)
	{
		linphone_core_enable_echo_cancellation(the_core,TRUE);
	}else
	{
		linphone_core_enable_echo_cancellation(the_core,FALSE);
	}
}

static bool_t agc = FALSE;
void CVideoConfClientDlg::OnBnClickedCheckAgc2()
{
	if(mAutomicGainControl.GetCheck() ==1)
	{
		 linphone_core_enable_agc(the_core,TRUE);
	}else
	{
		linphone_core_enable_agc(the_core,FALSE);
	}
}

void CVideoConfClientDlg::OnBnClickedButtonDial()
{
	CString num;

	mDialStr.GetWindowText(num);

	linphone_core_pause_all_calls(the_core);

	if(num.IsEmpty()) return;

	char normalizedUserName[384];



	if(mURImode.GetCheck()==1)
	{
		linphone_core_invite(the_core,num);
	}else
	{
		LinphoneProxyConfig* proxyCfg=NULL;	
		//get default proxy
		linphone_core_get_default_proxy(the_core,&proxyCfg);

		if(proxyCfg!=NULL){
			linphone_proxy_config_normalize_number(proxyCfg,num.GetString(), normalizedUserName,sizeof(normalizedUserName));
			const char * uri = linphone_core_get_identity(the_core);
			LinphoneAddress* tmpAddress = linphone_address_new(uri);
			linphone_address_set_username(tmpAddress,normalizedUserName);
			//	linphone_address_set_display_name(tmpAddress,"libsipua3");
			linphone_core_invite(the_core,linphone_address_as_string(tmpAddress)) ;
			linphone_address_destroy(tmpAddress);
			//linphone_core_use_preview_window(the_core,FALSE);
		}
	}
}

void CVideoConfClientDlg::OnBnClickedButtonHangup()
{

	linphone_core_terminate_all_calls(the_core);
}

void CVideoConfClientDlg::OnBnClickedCheckSelfView()
{
	if(mSelfView.GetCheck() == 1)
	{
		linphone_core_enable_self_view(the_core,TRUE);
	}else
		linphone_core_enable_self_view(the_core,FALSE);
}


void CVideoConfClientDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{

	CSliderCtrl *m_bar=(CSliderCtrl *)pScrollBar;

	if(m_bar== &mInputValue){
		linphone_core_set_rec_level(the_core,m_bar->GetPos());
	}

	if(m_bar== &mOutputValue){
		linphone_core_set_play_level(the_core,m_bar->GetPos());
	}

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}
void CVideoConfClientDlg::OnBnClickedButtonCaptureImage()
{
	LinphoneCall *call = linphone_core_get_current_call(the_core);

	if(call)
	{
		linphone_call_take_video_snapshot(call,"snapshot");
		mStatus.SetWindowText("截图成功 snapshot.jpg");
	}
}
static bool_t use_udt=FALSE;
void CVideoConfClientDlg::OnBnClickedButtonUdt()
{
	use_udt = !use_udt;
	linphone_core_call_enable_udt_video(the_core,linphone_core_get_current_call(the_core),use_udt);

}

void CVideoConfClientDlg::OnCbnSelchangeComboBandwith()
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
		linphone_core_set_download_bandwidth(the_core,1024);
		linphone_core_set_upload_bandwidth(the_core,1024);

		linphone_core_set_preferred_video_size_by_name(the_core,"720p");
	}else if(cur_sel ==6)
	{
		linphone_core_set_download_bandwidth(the_core,2048);
		linphone_core_set_upload_bandwidth(the_core,2048);

		linphone_core_set_preferred_video_size_by_name(the_core,"720p");
	}
}

void CVideoConfClientDlg::OnBnClickedCheckVideoMode()
{
	if(mVideoViewMode.GetCheck()==0){
		linphone_core_set_native_preview_window_id(the_core,(unsigned long)mVideoView.m_hWnd);
		linphone_core_set_native_video_window_id(the_core,(unsigned long)mVideoView.m_hWnd);
	}else{
		linphone_core_set_native_preview_window_id(the_core,NULL);
		linphone_core_set_native_video_window_id(the_core,NULL);
	}
}

void CVideoConfClientDlg::OnBnClickedCheckVideoPreview()
{
	if(mVideoPreViewChk.GetCheck()==0)
	{
		linphone_core_enable_video_preview(the_core,FALSE);
	}else
	{
		linphone_core_enable_video_preview(the_core,TRUE);
	}
}

void CVideoConfClientDlg::OnBnClickedButtonVideoRecord()
{
	int ret = 0;

	if(mRtmpPublishChk.GetCheck()==1)
	{
		CString tmp;
		mRTMP_URL.GetWindowText(tmp);
		if(tmp.GetLength()>8)
		{
			ret = linphone_call_record_video_file(linphone_core_get_current_call(the_core),
				tmp.GetString(),
				linphone_core_get_preferred_video_size(the_core),
				linphone_core_get_upload_bandwidth(the_core));
		}

	}else
	{
		ret = linphone_call_record_video_file(linphone_core_get_current_call(the_core),"videorecord.mp4",
			linphone_core_get_preferred_video_size(the_core),
			linphone_core_get_upload_bandwidth(the_core));
	}
	
	if(ret < 0)
		MessageBox("请先接通呼叫!!");
	
}

void CVideoConfClientDlg::OnBnClickedCheck1()
{
	if(mRtmpPublishChk.GetCheck()==1)
	{
		mRTMP_URL.EnableWindow(TRUE);

		mRecrodBnt.SetWindowText("推送视频");
	}else
	{
		mRTMP_URL.EnableWindow(FALSE);
		mRecrodBnt.SetWindowText("录制视频");
	}
}

void CVideoConfClientDlg::OnBnClickedButtonFastUpdate()
{
	linphone_core_send_picture_fast_update(linphone_core_get_current_call(the_core));
}
