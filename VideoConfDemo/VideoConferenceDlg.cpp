
// VideoConferenceDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "VideoConference.h"
#include "VideoConferenceDlg.h"
#include "crtdefs.h"
#include "conference.h"
#include "file_writer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include "crtdbg.h"
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif 


#define PHONE_CORE_TIMER 100
#define PHONE_CALL_TIMER 101
#define START_CONFERENCE_TIMER 301
#define JOIN_CONFERENCE_TIMER 302
#define AUTO_ACCEPT_CALL_TIMER 303
#ifdef __cplusplus
extern "C" {
#endif

#include "linphonecore.h"
#include "linphonecore_utils.h"

#pragma comment(lib,"libconference.lib") 

	static LinphoneConference *conf=NULL;
	static LinphoneCoreVTable vTable;
	static const char *config_file = "libsipua3.ini";
	static const char *factoryConfig = "libsipua3.ini";
	static LinphoneCore *the_core=NULL;
	static ConfSoundCard *sndcard=NULL;

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

		CVideoConferenceDlg *the_ui = (CVideoConferenceDlg *)linphone_core_get_user_data(lc);

		if(cstate == LinphoneCallIncomingReceived)
		{
			//LinphoneCall *call = linphone_core_get_current_call(the_core);
			linphone_core_accept_call(the_core,call);

			if(conf)
			{
				//SetTimer(the_ui->m_hWnd,AUTO_ACCEPT_CALL_TIMER,100,NULL);
			}
			
		}

		if(cstate == LinphoneCallStreamsRunning && conf)
		{
			if(linphone_core_call_in_conferencing(the_core,conf,call) == FALSE)
			{
				//LinphoneConferenceMember *mb = linphone_conference_new_mb(the_core,call);
				//linphone_conference_add_mb(the_core,conf,mb,0);
				SetTimer(the_ui->m_hWnd,JOIN_CONFERENCE_TIMER,100,NULL);
			}
		}

		if(the_ui)
		{ //通话列表操作
			CString tmp;
			const MSList *call_list = linphone_core_get_calls(lc);
			const MSList *elem;
			int i = 0;
			//CListCtrl callListReport = the_ui->mCallList;
			the_ui->mCallList.DeleteAllItems();
			for(elem=call_list;elem!=NULL;elem=elem->next){
				LinphoneCall *call=(LinphoneCall *)elem->data;
				LinphoneCallState cs=linphone_call_get_state(call);
				const LinphoneAddress *call_addr = linphone_call_get_remote_address(call);
				LinphoneCallDir dir= linphone_call_get_dir(call);
				//	if (cs==LinphoneCallStreamsRunning || cs==LinphoneCallPausedByRemote){

				tmp.Format("%d",i);
				the_ui->mCallList.InsertItem(i,tmp);
				tmp.Format("%s",linphone_address_get_username(call_addr));
				the_ui->mCallList.SetItemText(i,1,tmp);
				tmp.Format("%s",linphone_call_state_to_string(cs));
				the_ui->mCallList.SetItemText(i,2,tmp);
				tmp.Format("%s",the_ui->Sec2Time(linphone_call_get_duration(call)));
				the_ui->mCallList.SetItemText(i,3,tmp);
				tmp.Format("%s",(dir==LinphoneCallIncoming) ? _T("来电") : _T("去电"));
				the_ui->mCallList.SetItemText(i,4,tmp);
				the_ui->mCallList.SetItemData(i,(DWORD_PTR)call);
				i++;
				//	}
			}
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
		CVideoConferenceDlg *the_ui = (CVideoConferenceDlg *)linphone_core_get_user_data(lc);

		if(the_ui)
			the_ui->ShowWindow(SW_SHOW);
	}
	/** Callback prototype */
	void theDisplayStatusCb(struct _LinphoneCore *lc, const char *message)
	{
		CVideoConferenceDlg *the_ui = (CVideoConferenceDlg *)linphone_core_get_user_data(lc);
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
		CVideoConferenceDlg *the_ui = (CVideoConferenceDlg *)linphone_core_get_user_data(lc);
		CString tmp;

		tmp.Format("收到 %s, DTMF 按键 %c",linphone_call_get_remote_address_as_string(call),dtmf);
		if(the_ui)
			the_ui->mStatus.SetWindowText(tmp);

		tmp.Format("%c",dtmf);

		//使用DTMF信号控制画面切换 0 显示所有  1-9 分别显示1-9参与者
		if(conf && linphone_core_call_in_conferencing(lc,conf,call)){
			int index = atoi(tmp.GetString());
			linphone_conference_show_member_at_index(conf,index-1);
		}
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


// CVideoConferenceDlg 对话框




CVideoConferenceDlg::CVideoConferenceDlg(CWnd* pParent /*=NULL*/)
: CDialog(CVideoConferenceDlg::IDD, pParent)
, mCurrentCallPos(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CVideoConferenceDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_STATUS, mStatus);
	DDX_Control(pDX, IDC_EDIT_VIDEO_VIEW, mVideoView);
	DDX_Control(pDX, IDC_EDIT_DIAL, mDialNum);
	DDX_Control(pDX, IDC_CHECK_URI_MODE, mURImode);
	DDX_Control(pDX, IDC_CALL_LIST, mCallList);
	DDX_Control(pDX, IDC_CHECK_ENABLED_VIDEO, mVideoCallChk);
	//	DDX_Control(pDX, IDC_CHECK_VIDEO_CONF, mChkVideoConf);
	DDX_Control(pDX, IDC_COMBO_COM_MAX_USERS, mMaxConfMembers);
	DDX_Control(pDX, IDC_COMBO_BANDWIDTH, m_bandwidth);
	DDX_Control(pDX, IDC_CHECK_VIDEO_MONITOR, mchkVideoLocalMonitor);
	DDX_Control(pDX, IDC_CHECK2, mConfVideoAutoLayout);
	DDX_Control(pDX, IDC_EDIT_CALL_VIDEO_VIEW, mCallVideoView);
	DDX_Control(pDX, IDC_STATIC_MCU_URL, mMCUURL);
	DDX_Control(pDX, IDC_RADIO_ALL_PARTY, mLine_all);
	DDX_Control(pDX, IDC_RADIO_LINE_1, mLine_1);
	DDX_Control(pDX, IDC_RADIO_LINE_2, mLine_2);
	DDX_Control(pDX, IDC_RADIO_LINE_3, mLine_3);
	DDX_Control(pDX, IDC_RADIO_LINE_4, mLine_4);
	DDX_Control(pDX, IDC_RADIO_LINE_5, mLine_5);
	DDX_Control(pDX, IDC_RADIO_LINE_6, mLine_6);
	DDX_Control(pDX, IDC_RADIO_LINE_7, mLine_7);
	DDX_Control(pDX, IDC_RADIO_LINE_8, mLine_8);
	DDX_Control(pDX, IDC_RADIO_LINE_9, mLine_9);
	DDX_Control(pDX, IDC_EDIT_ACCOUNT, mUserName);
	DDX_Control(pDX, IDC_EDIT_SECRET, mSecret);
	DDX_Control(pDX, IDC_EDIT_SERVER, mSipServer);
	DDX_Control(pDX, IDC_EDIT1, mRTMPUrl);
	DDX_Control(pDX, IDC_BUTTON_CONF_RECORD, mBntConfRecdoing);
}

BEGIN_MESSAGE_MAP(CVideoConferenceDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_WM_TIMER()
	ON_BN_CLICKED(ID_BUTTON_DIAL, &CVideoConferenceDlg::OnBnClickedButtonDial)
	ON_BN_CLICKED(IDC_BUTTON_HANGUP, &CVideoConferenceDlg::OnBnClickedButtonHangup)
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_HSCROLL()
	ON_NOTIFY(NM_RCLICK, IDC_CALL_LIST, &CVideoConferenceDlg::OnNMRClickCallList)
	ON_COMMAND(ID_CALLIST_DROP_CALL, &CVideoConferenceDlg::OnCallistDropCall)
	ON_COMMAND(ID_CALLIST_ACCEPT_CALL, &CVideoConferenceDlg::OnCallistAcceptCall)
	ON_COMMAND(ID_CALLIST_HOLD_CALL, &CVideoConferenceDlg::OnCallistHoldCall)
	ON_COMMAND(ID_CALLIST_RESUME_CALL, &CVideoConferenceDlg::OnCallistResumeCall)
	ON_COMMAND(ID_CALLIST_JOIN_TO_CONF, &CVideoConferenceDlg::OnCallistJoinToConf)
	ON_BN_CLICKED(IDC_CHECK_ENABLED_VIDEO, &CVideoConferenceDlg::OnBnClickedCheckEnabledVideo)
	ON_BN_CLICKED(IDC_BUTTON_START_CONF, &CVideoConferenceDlg::OnBnClickedButtonStartConf)
	ON_COMMAND(ID_CALLIST_EXIT_CONF, &CVideoConferenceDlg::OnCallistExitConf)
	ON_CBN_SELCHANGE(IDC_COMBO_BANDWIDTH, &CVideoConferenceDlg::OnCbnSelchangeComboBandwidth)
	ON_BN_CLICKED(IDC_CHECK2, &CVideoConferenceDlg::OnBnClickedCheck2)
	ON_EN_CHANGE(IDC_EDIT_VIDEO_VIEW, &CVideoConferenceDlg::OnEnChangeEditVideoView)
	ON_BN_CLICKED(IDC_CHECK_VIDEO_CONF, &CVideoConferenceDlg::OnBnClickedCheckVideoConf)
	ON_CBN_SELCHANGE(IDC_COMBO_COM_MAX_USERS, &CVideoConferenceDlg::OnCbnSelchangeComboComMaxUsers)
	ON_BN_CLICKED(IDC_RADIO_ALL_PARTY, &CVideoConferenceDlg::OnBnClickedRadioAllParty)
	ON_BN_CLICKED(IDC_RADIO_LINE_1, &CVideoConferenceDlg::OnBnClickedRadioLine1)
	ON_BN_CLICKED(IDC_RADIO_LINE_2, &CVideoConferenceDlg::OnBnClickedRadioLine2)
	ON_BN_CLICKED(IDC_RADIO_LINE_3, &CVideoConferenceDlg::OnBnClickedRadioLine3)
	ON_BN_CLICKED(IDC_RADIO_LINE_4, &CVideoConferenceDlg::OnBnClickedRadioLine4)
	ON_BN_CLICKED(IDC_RADIO_LINE_5, &CVideoConferenceDlg::OnBnClickedRadioLine5)
	ON_BN_CLICKED(IDC_RADIO_LINE_6, &CVideoConferenceDlg::OnBnClickedRadioLine6)
	ON_BN_CLICKED(IDC_RADIO_LINE_7, &CVideoConferenceDlg::OnBnClickedRadioLine7)
	ON_BN_CLICKED(IDC_RADIO_LINE_8, &CVideoConferenceDlg::OnBnClickedRadioLine8)
	ON_BN_CLICKED(IDC_RADIO_LINE_9, &CVideoConferenceDlg::OnBnClickedRadioLine9)
	ON_BN_CLICKED(IDC_BUTTON_REGISTER, &CVideoConferenceDlg::OnBnClickedButtonRegister)
	ON_BN_CLICKED(IDC_BUTTON_UNREGISTER, &CVideoConferenceDlg::OnBnClickedButtonUnregister)
	ON_BN_CLICKED(IDC_BUTTON_CONF_RECORD, &CVideoConferenceDlg::OnBnClickedButtonConfRecord)
END_MESSAGE_MAP()


// CVideoConferenceDlg 消息处理程序

BOOL CVideoConferenceDlg::OnInitDialog()
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


	the_core = linphone_core_new(&vTable
		,config_file
		,factoryConfig
		,this);

	if (the_core)
	{
		SetTimer(PHONE_CORE_TIMER,30,NULL);
		SetTimer(PHONE_CALL_TIMER,500,NULL);

	}

	linphone_core_enable_video(the_core,TRUE,TRUE);
	linphone_core_enable_video_preview(the_core,FALSE);
	//？
	//linphone_core_set_native_preview_window_id(the_core,(unsigned long)mVideoView.m_hWnd);
	linphone_core_set_native_video_window_id(the_core,(unsigned long)mCallVideoView.m_hWnd);
	/*
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
	mSndCap.AddString(tmp);
	if(cur_capdev && strlen(cur_capdev) && !strcmp(cur_capdev,tmp))
	mSndCap.SetCurSel(in_index);
	in_index++;
	}
	if (linphone_core_sound_device_can_playback(the_core,dev))
	{
	mSndPlay.AddString(tmp);
	if(cur_playdev && strlen(cur_playdev) && !strcmp(cur_playdev,tmp))
	mSndPlay.SetCurSel(out_index);
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
	for(;dev=video_devices[i];i++){
	CString tmp = CString(dev);
	mVideoDevices.AddString(tmp);
	if(curdev && strlen(curdev) && !strcmp(curdev,tmp))
	mVideoDevices.SetCurSel(i);
	}

	linphone_core_set_video_device(the_core,linphone_core_get_video_device(the_core));
	}
	*/

	linphone_core_enable_self_view(the_core,FALSE);
	linphone_core_set_video_device(the_core,"Static picture");

	linphone_core_set_download_bandwidth(the_core,512);
	linphone_core_set_upload_bandwidth(the_core,512);

	linphone_core_set_preferred_video_size_by_name(the_core,"vga");

	linphone_core_enable_echo_cancellation(the_core,FALSE);
	linphone_core_enable_agc(the_core,FALSE);
	linphone_core_set_playback_gain_db(the_core,1.0);

	linphone_core_set_sip_port(the_core,5060);
	linphone_core_set_audio_port(the_core,17188);
	linphone_core_set_video_port(the_core,17288);

	linphone_core_set_audio_jittcomp(the_core,120);

	//LinphoneSoundDaemon *lsd;
	//LsdPlayer *p;
	//lsd=linphone_sound_daemon_new (NULL,8000,1);

	//linphone_core_use_sound_daemon(the_core,lsd);

	m_bandwidth.AddString("256k / CIF(352x288)");
	m_bandwidth.AddString("512k / VGA(640x480)");
	m_bandwidth.AddString("768k / 4CIF(704×576)");
	m_bandwidth.AddString("1M / 720P(1280×720)");
	m_bandwidth.AddString("2M / 720P(1280×720)");

	m_bandwidth.SetCurSel(1);

	mVideoCallChk.SetCheck(1);
	mURImode.SetCheck(1);
//	mChkVideoConf.SetCheck(1);
	mchkVideoLocalMonitor.SetCheck(1);

	mMaxConfMembers.AddString(_T("4"));
	mMaxConfMembers.AddString(_T("5"));
	mMaxConfMembers.AddString(_T("6"));
	mMaxConfMembers.AddString(_T("7"));
	mMaxConfMembers.AddString(_T("8"));
	mMaxConfMembers.AddString(_T("9"));

	mMaxConfMembers.SetCurSel(2);

	/*
	linphone_core_set_firewall_policy(the_core,LinphonePolicyUseStun);
	linphone_core_set_stun_server(the_core,"stun.xten.com");
	*/

	//mCallList.SetImageList(&m_image,LVSIL_SMALL);
	mCallList.SetExtendedStyle(mCallList.GetExtendedStyle()|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
	mCallList.InsertColumn(1,_T("ID"),LVCFMT_CENTER);
	mCallList.InsertColumn(2,_T("号码"),LVCFMT_CENTER);
	mCallList.InsertColumn(3,_T("状态"),LVCFMT_CENTER);
	mCallList.InsertColumn(4,_T("时长"),LVCFMT_CENTER);
	mCallList.InsertColumn(5,_T("局向"),LVCFMT_CENTER);
	mCallList.SetColumnWidth(0,30);
	mCallList.SetColumnWidth(1,150);
	mCallList.SetColumnWidth(2,150);
	mCallList.SetColumnWidth(3,50);
	mCallList.SetColumnWidth(4,80);

	m_pos.SetRangeMax(100,TRUE);

	mInputValue.SetPos(30);
	mOutputValue.SetPos(30);

	{
		char resault[LINPHONE_IPADDR_SIZE];
		CString tmp;
		linphone_core_get_local_ip(the_core,"58.213.155.7",resault);
		tmp.Format("服务器地址: sip:%s:%d",resault,linphone_core_get_sip_port(the_core));
		mMCUURL.SetWindowText(tmp);
	}

	SetPartyControl(6);

	linphone_core_set_mcu_mode(the_core,TRUE);

	mUserName.SetWindowText("205");
	mSecret.SetWindowText("205");
	mSipServer.SetWindowText("192.168.1.200");

	mRTMPUrl.SetWindowText("rtmp://localhost/live/cam live=1");

	SetTimer(START_CONFERENCE_TIMER,2000,NULL);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CVideoConferenceDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CVideoConferenceDlg::OnPaint()
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
HCURSOR CVideoConferenceDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CVideoConferenceDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	if(nIDEvent ==PHONE_CORE_TIMER)
	{
		linphone_core_iterate(the_core);
	}

	if(nIDEvent==START_CONFERENCE_TIMER)
	{
		KillTimer(START_CONFERENCE_TIMER);
		OnBnClickedButtonStartConf();
	}

	if(nIDEvent == AUTO_ACCEPT_CALL_TIMER)
	{
		KillTimer(AUTO_ACCEPT_CALL_TIMER);

		LinphoneCall *call = linphone_core_get_current_call(the_core);
		if(call && conf)
		{
			//linphone_core_accept_call(the_core,call);
		}
		

	}

	if (nIDEvent == JOIN_CONFERENCE_TIMER)
	{
		KillTimer(JOIN_CONFERENCE_TIMER);

		LinphoneCall *call = linphone_core_get_current_call(the_core);
		if(call && conf)
		{
			LinphoneConferenceMember *mb = linphone_conference_new_mb(the_core,call);
			linphone_conference_add_mb(the_core,conf,mb,0);
		}
	}

	if(nIDEvent == PHONE_CALL_TIMER)
	{
		//	if(linphone_core_in_call(the_core))
		{
			CString tmp;
			const MSList *call_list = linphone_core_get_calls(the_core);
			const MSList *elem;
			int i = 0;
			for(elem=call_list;elem!=NULL;elem=elem->next){
				LinphoneCall *call=(LinphoneCall *)elem->data;
				LinphoneCallState cs=linphone_call_get_state(call);
				tmp.Format("%s",linphone_call_state_to_string(cs));
				mCallList.SetItemText(i,2,tmp);
				tmp.Format("%s",Sec2Time(linphone_call_get_duration(call)));
				mCallList.SetItemText(i,3,tmp);

				i++;
			}
		}
	}
	CDialog::OnTimer(nIDEvent);
}

void CVideoConferenceDlg::OnBnClickedButtonDial()
{
	CString num;

	mDialNum.GetWindowText(num);

	linphone_core_pause_all_calls(the_core);

	if(num.IsEmpty()) return;

	char normalizedUserName[256];



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
		}
	}
}

void CVideoConferenceDlg::OnBnClickedButtonHangup()
{
	/*if(conf!=NULL)
	{
	linphone_conference_remove_all_mbs(the_core,conf);
	}*/

	linphone_core_terminate_all_calls(the_core);
}

void CVideoConferenceDlg::OnDestroy()
{
	CDialog::OnDestroy();
}

void CVideoConferenceDlg::OnClose()
{
	if(conf!=NULL)
		linphone_conference_stop(the_core,conf);

	linphone_core_terminate_all_calls(the_core);
	linphone_core_clear_proxy_config(the_core);
	linphone_core_clear_all_auth_info(the_core);
	KillTimer(PHONE_CORE_TIMER);
	KillTimer(PHONE_CALL_TIMER);
	linphone_core_destroy(the_core);
	the_core=NULL;
	CDialog::OnClose();
}

void CVideoConferenceDlg::OnBnClickedButtonRegister()
{
	CString username,password,server,szidentity;

	mUserName.GetWindowText(username);
	mSecret.GetWindowText(password);
	mSipServer.GetWindowText(server);

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
			MessageBox("已注册到服务器！");
			return;
		}else
		{
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

void CVideoConferenceDlg::OnBnClickedButtonUnregister()
{
	linphone_core_clear_proxy_config(the_core);
}


void CVideoConferenceDlg::OnBnClickedButtonCapturePic()
{
	LinphoneCall *call = linphone_core_get_current_call(the_core);
	if(call)
		linphone_call_take_video_snapshot(call,"snapshot");
	if(conf)
	{
		MSList *list = linphone_conference_get_members(conf);
		linphone_call_take_video_snapshot(linphone_conference_mb_get_call((LinphoneConferenceMember *)list->data),"snapshot.jpeg");
	}
}

CString CVideoConferenceDlg::Sec2Time(int sec)
{
	CString tmp;
	tmp.Format("%02d:%02d", sec/60,sec%60);
	return tmp;
}

void CVideoConferenceDlg::OnNMRClickCallList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);


	int nIndex = mCallList.GetNextItem(-1,LVNI_SELECTED);

	if(nIndex==-1)
	{
		return;
	}

	mCurrentCallPos = nIndex;

	CMenu *pMenu,menu;
	menu.LoadMenu(IDR_MENU_CALL_LIST);
	//InitMenu(&menu);
	pMenu = menu.GetSubMenu(0);
	CPoint pt;
	GetCursorPos(&pt);

	DWORD dwCmd = TrackPopupMenu(pMenu->m_hMenu,TPM_RIGHTBUTTON,pt.x,pt.y,0,this->m_hWnd,NULL);

	*pResult = 0;
}

void CVideoConferenceDlg::OnCallistAcceptCall()
{
	LinphoneCall *call=(LinphoneCall *)mCallList.GetItemData(mCurrentCallPos);
	if(call!=NULL)
	{
		linphone_core_pause_all_calls(the_core);
		linphone_core_accept_call(the_core,call);
	}
}

void CVideoConferenceDlg::OnCallistDropCall()
{
	LinphoneCall *call=(LinphoneCall *)mCallList.GetItemData(mCurrentCallPos);
	if(call!=NULL)
	{
		if(conf!=NULL)
		{
			if(!linphone_core_call_in_conferencing(the_core,conf,call))
				linphone_core_terminate_call(the_core,call);
		}else
			linphone_core_terminate_call(the_core,call);

	}
}

void CVideoConferenceDlg::OnCallistHoldCall()
{
	LinphoneCall *call=(LinphoneCall *)mCallList.GetItemData(mCurrentCallPos);
	if(call!=NULL)
	{
		linphone_core_pause_call(the_core,call);
	}
}

void CVideoConferenceDlg::OnCallistResumeCall()
{
	LinphoneCall *call=(LinphoneCall *)mCallList.GetItemData(mCurrentCallPos);
	if(call!=NULL)
	{
		linphone_core_pause_all_calls(the_core);
		linphone_core_resume_call(the_core,call);
	}
}

void CVideoConferenceDlg::OnCallistJoinToConf()
{
	LinphoneCall *call=(LinphoneCall *)mCallList.GetItemData(mCurrentCallPos);
	if(call!=NULL && conf !=NULL)
	{
		LinphoneConferenceMember *mb = linphone_conference_new_mb(the_core,call);
		linphone_conference_add_mb(the_core,conf,mb,0);
	}
}

void CVideoConferenceDlg::OnBnClickedCheckEnabledVideo()
{
	if(mVideoCallChk.GetCheck()==1)
	{

		linphone_core_enable_video(the_core,TRUE,TRUE);
		linphone_core_enable_video_preview(the_core,FALSE);
	}else
	{
		linphone_core_enable_video(the_core,FALSE,FALSE);
		linphone_core_enable_video_preview(the_core,FALSE);

	}
}

void CVideoConferenceDlg::OnCbnSelchangeComboVideoDevices()
{
	CString device;
	mVideoDevices.GetWindowText(device);
	linphone_core_set_video_device(the_core,device.GetString());
}


static bool_t conferencd_enabled=FALSE;
void CVideoConferenceDlg::OnBnClickedButtonStartConf()
{
	// TODO: 在此添加控件通知处理程序代码
	MSVideoSize vsize;
	CString tmp;
	vsize.width = MS_VIDEO_SIZE_VGA_W;
	vsize.height = MS_VIDEO_SIZE_VGA_H;
	bool_t video_conf = TRUE;// mChkVideoConf.GetCheck();
	int conf_max_users = 4;
	int cur_sel = m_bandwidth.GetCurSel();
	bool_t local_mode = mLocaMode.GetCheck();
	bool_t local_monitor = mchkVideoLocalMonitor.GetCheck();
	bool_t auto_layout = mConfVideoAutoLayout.GetCheck();
	bool_t hd_mode = FALSE;//mHDMode.GetCheck();

	if(cur_sel ==0)
	{
		vsize.width = MS_VIDEO_SIZE_CIF_W;
		vsize.height = MS_VIDEO_SIZE_CIF_H;
	}else if(cur_sel == 1)
	{
		vsize.width = MS_VIDEO_SIZE_VGA_W;
		vsize.height = MS_VIDEO_SIZE_VGA_H;
	}else if(cur_sel == 2)
	{
		vsize.width = MS_VIDEO_SIZE_VGA_W;
		vsize.height = MS_VIDEO_SIZE_VGA_H;
	}else if(cur_sel == 3)
	{
		vsize.width = MS_VIDEO_SIZE_4CIF_W;
		vsize.height = MS_VIDEO_SIZE_4CIF_H;
	}else if(cur_sel == 4)
	{
		vsize.width = MS_VIDEO_SIZE_720P_W;
		vsize.height = MS_VIDEO_SIZE_720P_H;
	}else if(cur_sel == 5)
	{
		vsize.width = MS_VIDEO_SIZE_720P_W;
		vsize.height = MS_VIDEO_SIZE_720P_H;
	}

	if(!conferencd_enabled)
	{
		if(conf==NULL)
		{
			GetDlgItem(IDC_BUTTON_START_CONF)->SetWindowText("停止会议");
			mMaxConfMembers.GetWindowText(tmp);
			conf_max_users = atoi(tmp.GetString());
			conf = linphone_conference_new(the_core,
				"Video Confernce Demo",
				conf_max_users,
				video_conf,
				FALSE,
				hd_mode,
				local_mode,
				local_monitor,
				auto_layout,vsize,
				(unsigned long)mVideoView.m_hWnd,
				linphone_core_get_upload_bandwidth(the_core));

			linphone_conference_start(the_core,conf);
			mLine_all.SetCheck(1);
		}

	}else
	{
		GetDlgItem(IDC_BUTTON_START_CONF)->SetWindowText("开始会议");
		mBntConfRecdoing.SetWindowText("录制/发布视频");

		if(conf!=NULL)
			linphone_conference_stop(the_core,conf);
		sndcard=NULL;
		conf=NULL;
	}

	conferencd_enabled = !conferencd_enabled;
}

static bool_t playfile = FALSE;

void CVideoConferenceDlg::OnBnClickedButtonConfPlayAudio()
{
	if(!playfile){

		if(conf!=NULL){

			if(linphone_conference_get_sample_rate(conf)==16000)
				linphone_conference_play_audio(the_core,conf,"test_hd.wav");
			else
				linphone_conference_play_audio(the_core,conf,"test.wav");
			playfile = !playfile;
			GetDlgItem(IDC_BUTTON_CONF_PLAY_AUDIO)->SetWindowText("停止");
		}
	}else{

		if(conf!=NULL){
			linphone_conference_play_audio_stop(the_core,conf);
			playfile = !playfile;
			GetDlgItem(IDC_BUTTON_CONF_PLAY_AUDIO)->SetWindowText("放音");
		}
	}
}


static bool_t record = FALSE;

void CVideoConferenceDlg::OnBnClickedButtonConfRecordFile()
{
	if(!record)
	{

		if(conf!=NULL)
		{
			linphone_conference_record_audio(the_core,conf,"conf_record.wav");
			record = !record;
			GetDlgItem(IDC_BUTTON_CONF_RECORD_FILE)->SetWindowText("停止");
		}

	}else
	{

		if(conf!=NULL)
		{
			linphone_conference_record_audio_stop(the_core,conf);
			record = !record;
			GetDlgItem(IDC_BUTTON_CONF_RECORD_FILE)->SetWindowText("录音");
		}
	}



}

void CVideoConferenceDlg::OnCbnSelchangeComboAudipCap()
{
	CString device;
	mSndCap.GetWindowText(device);
	linphone_core_set_capture_device(the_core,device.GetString());

}

void CVideoConferenceDlg::OnCbnSelchangeComboAudioPlay()
{
	CString device;
	mSndPlay.GetWindowText(device);
	linphone_core_set_playback_device(the_core,device.GetString());
}

void CVideoConferenceDlg::OnCallistExitConf()
{
	//离开会议室
	LinphoneCall *call=(LinphoneCall *)mCallList.GetItemData(mCurrentCallPos);
	if(call!=NULL && conf !=NULL)
	{
		LinphoneConferenceMember *mb = linphone_conference_find_mb_by_call(the_core,conf,call);
		linphone_conference_remove_mb(the_core,conf,mb);
	}
}
static bool_t vad = TRUE;
void CVideoConferenceDlg::OnBnClickedButtonConfVad()
{
	if(conf)
		linphone_conference_enable_vad(the_core,conf,vad);

	vad = !vad;
}
static bool_t agc = TRUE;
void CVideoConferenceDlg::OnBnClickedButtonConfAgc()
{
	if(conf)
		linphone_conference_enable_agc(the_core,conf,agc? 30.0:0.0);
	agc = !agc;
}

static bool_t half_duplex = TRUE;
void CVideoConferenceDlg::OnBnClickedButtonHalfDuplex()
{
	if(conf)
		linphone_conference_enable_halfduplex(the_core,conf,half_duplex);
	half_duplex = !half_duplex;
}

static bool_t direct_mode = TRUE;
void CVideoConferenceDlg::OnBnClickedButtonDirectMode()
{
	if(conf)
		linphone_conference_enable_directmode(the_core,conf,direct_mode);
	direct_mode = !direct_mode;
}

static bool_t vad_prob_start = TRUE;
void CVideoConferenceDlg::OnBnClickedButtonVadProbStart()
{
	if(conf)
		linphone_conference_set_vad_prob_start(the_core,conf,vad_prob_start);
	vad_prob_start = !vad_prob_start;
}

static bool_t vad_prob_continue = TRUE;
void CVideoConferenceDlg::OnBnClickedButton6()
{
	if(conf)
		linphone_conference_set_vad_prob_continue(the_core,conf,vad_prob_continue);
	vad_prob_continue = !vad_prob_continue;
}

void CVideoConferenceDlg::OnCbnSelchangeComboBandwidth()
{
	int cur_sel = m_bandwidth.GetCurSel();

	if(cur_sel ==0)
	{
		linphone_core_set_download_bandwidth(the_core,256);
		linphone_core_set_upload_bandwidth(the_core,256);

		linphone_core_set_preferred_video_size_by_name(the_core,"cif");
	}else if(cur_sel == -1)
	{
		linphone_core_set_download_bandwidth(the_core,384);
		linphone_core_set_upload_bandwidth(the_core,384);

		linphone_core_set_preferred_video_size_by_name(the_core,"vga");
	}else if(cur_sel == 1)
	{
		linphone_core_set_download_bandwidth(the_core,512);
		linphone_core_set_upload_bandwidth(the_core,512);

		linphone_core_set_preferred_video_size_by_name(the_core,"vga");
	}else if(cur_sel ==2)
	{
		linphone_core_set_download_bandwidth(the_core,768);
		linphone_core_set_upload_bandwidth(the_core,768);

		linphone_core_set_preferred_video_size_by_name(the_core,"4cif");
	}else if(cur_sel ==3)
	{
		linphone_core_set_download_bandwidth(the_core,1024);
		linphone_core_set_upload_bandwidth(the_core,1024);

		linphone_core_set_preferred_video_size_by_name(the_core,"720p");
	}else if(cur_sel ==4)
	{
		linphone_core_set_download_bandwidth(the_core,2048);
		linphone_core_set_upload_bandwidth(the_core,2048);

		linphone_core_set_preferred_video_size_by_name(the_core,"720p");
	}
	if(conf)
		MessageBox("带宽设置变更,需要重启会议!");
}

void CVideoConferenceDlg::OnBnClickedButtonIKey()
{
	LinphoneCall *call =linphone_core_get_current_call(the_core);

	if(call)
	{
		linphone_call_video_vfu_request(call);
	}
}

static bool_t test_on = FALSE;

void CVideoConferenceDlg::OnBnClickedButtonTest()
{
	if(!test_on)
	{
		if(sndcard==NULL)
		{	test_on = !test_on;
		int rate = linphone_conference_get_sample_rate(conf);
		sndcard = local_sound_card_new();
		local_sound_card_start(sndcard,linphone_core_sound_get_default_playback_device(the_core),linphone_core_sound_get_default_capture_device(the_core),rate);

		GetDlgItem(IDC_BUTTON_TEST)->SetWindowText("移除声卡");

		linphone_conference_add_local_sndcard(conf,sndcard);
		}
	}else
	{
		if(sndcard!=NULL)
		{
			linphone_conference_remove_local_sndcard(conf);
			local_sound_card_stop(sndcard);
			test_on = !test_on;
			GetDlgItem(IDC_BUTTON_TEST)->SetWindowText("添加声卡");
			sndcard =NULL;
		}else
		{
			test_on = !test_on;
		}
	}
}

void CVideoConferenceDlg::OnBnClickedButtonAddCam()
{
	CString device;
	MSWebCam *dev=NULL;
	MSVideoSize vsize;
	float fps;
	mVideoDevices.GetWindowText(device);

	dev = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),device.GetString());

	if(conf!=NULL && dev!=NULL && linphone_conference_video_enabled(conf))
	{
		vsize = linphone_conference_get_video_size(conf);
		fps = linphone_conference_get_video_fps(conf);

		ConfWebCam *localcam= local_webcam_new();

		if(local_webcam_start(localcam,dev,vsize,fps)==0)
		{
			linphone_conference_add_local_webcam(conf,localcam);
		}
	}
}

void CVideoConferenceDlg::OnBnClickedButtonRemoveCam()
{
	linphone_conference_remove_local_all_webcams(conf);
}

void CVideoConferenceDlg::OnBnClickedCheck2()
{
	bool_t enabled = mConfVideoAutoLayout.GetCheck();

	if(conf!=NULL)
	{
		linphone_conference_enable_auto_layout(conf,enabled);
	}
}

void CVideoConferenceDlg::OnBnClickedCheckHdMode()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CVideoConferenceDlg::OnBnClickedButtonPlayVideoFile()
{
	CString tmp;
	CFileDialog fdlg(TRUE,NULL,NULL,NULL,"MP4 mp4(.mp4)|*.mp4|AVI avi(.avi)|*.avi|所有文件 (*.*)|*.*||");
	if(IDOK==fdlg.DoModal())
	{
		tmp=fdlg.GetPathName();
		linphone_core_play_video_file(the_core,tmp.GetString());
		linphone_call_video_vfu_request(linphone_core_get_current_call(the_core));
	}else
	{
		linphone_core_play_video_file_stop(the_core);
	}

}

#include "file_stream.h"
static VideoState *is=NULL;
static VideoPlayer *vp=NULL;
void CVideoConferenceDlg::OnBnClickedButtonPlayer()
{

	if(is==NULL && vp==NULL)
	{
		m_pos.SetPos(0);
		is = file_stream_new();
		file_stream_init(is);
		CString tmp;
		CFileDialog fdlg(TRUE,NULL,NULL,NULL,"MP4 mp4(.mp4)|*.mp4|AVI avi(.avi)|*.avi|所有文件 (*.*)|*.*||");
		if(IDOK==fdlg.DoModal())
		{
			tmp=fdlg.GetPathName();
			file_stream_open(is,tmp.GetString());

			vp = video_player_new();

			video_player_set_audio_source(vp,file_stream_create_audio_filter(is));
			video_player_set_video_source(vp,file_stream_create_video_filter(is));

			video_player_set_rate(vp,16000);
			video_player_set_nchannels(vp,2);

			video_player_set_video_size(vp,file_stream_video_get_vsize(is));
			video_player_set_fps(vp,file_stream_video_get_fps(is));

//			video_player_start(vp,the_core->sound_conf.play_sndcard,(unsigned long)mVideoView.m_hWnd);

			file_stream_output_data(is,TRUE);

		}

	}else{
		if(vp!=NULL) video_player_stop(vp);
		if(is!=NULL)file_stream_stop(is);

		vp=NULL;
		is=NULL;
	}
}

//void CVideoConferenceDlg::OnNMCustomdrawSlider1(NMHDR *pNMHDR, LRESULT *pResult)
//{
//	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
//	// TODO: 在此添加控件通知处理程序代码
//	*pResult = 0;
//}

//音量滑竿处纴E
void CVideoConferenceDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{

	CSliderCtrl *m_bar=(CSliderCtrl *)pScrollBar;

	if(m_bar== &m_pos){
		if(is)
			file_stream_do_seek(is,m_pos.GetPos());
	}


	if(m_bar== &mInputValue){
		linphone_core_set_rec_level(the_core,m_bar->GetPos());
	}

	if(m_bar== &mOutputValue){
		linphone_core_set_play_level(the_core,m_bar->GetPos());
	}

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}
/*
static void calibration_finished(LinphoneCore *lc, LinphoneEcCalibratorStatus status, int delay, void *data){
ms_message("echo calibration finished %s.",status==LinphoneEcCalibratorDone ? "successfully" : "with faillure");
if (status==LinphoneEcCalibratorDone) ms_message("Measured delay is %i",delay);
}*/

#include "video_mail_record.h"
static VideoMailRecord *os =NULL;

void CVideoConferenceDlg::OnBnClickedButtonEcc(){
	MSVideoSize vsize;
	CString device;
	MSWebCam *dev=NULL;
	mVideoDevices.GetWindowText(device);
	//vsize.width = MS_VIDEO_SIZE_WXGA900P_W;
	//vsize.height = MS_VIDEO_SIZE_WXGA900P_H;

	vsize.width = MS_VIDEO_SIZE_720P_W;
	vsize.height = MS_VIDEO_SIZE_720P_H;

	dev = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),device.GetString());

	if(os==NULL){
		os = video_mail_record_new();
		video_mail_record_set_rate(os,8000);
		video_mail_record_set_nchannels(os,1);
		video_mail_record_set_fps(os,15.00);
		video_mail_record_set_video_size(os,vsize);
		video_mail_record_start(os,linphone_core_sound_get_default_capture_device(the_core),dev,(unsigned long)mVideoView.m_hWnd,"bbb.mp4");

	}else{
		video_mail_record_stop(os);
		os =NULL;
	}
}

static bool_t enabled_video_record=FALSE;
void CVideoConferenceDlg::OnBnClickedButtonRecord(){

	enabled_video_record =!enabled_video_record;

	if(os) video_mail_record_start_recording(os,enabled_video_record);
}

void CVideoConferenceDlg::OnEnChangeEditVideoView()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}

static bool_t use_udt=FALSE;
void CVideoConferenceDlg::OnBnClickedButtonUdt()
{
	use_udt = !use_udt;
	linphone_core_call_enable_udt_video(the_core,linphone_core_get_current_call(the_core),use_udt);
}
void CVideoConferenceDlg::OnBnClickedCheckVideoConf()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CVideoConferenceDlg::OnCbnSelchangeComboComMaxUsers()
{
	int conf_max_users;
	CString tmp;

	mMaxConfMembers.GetWindowText(tmp);
	conf_max_users = atoi(tmp.GetString());

	SetPartyControl(conf_max_users);

	if(conf)
		MessageBox("设置变更,需要重启会议!");
}

void CVideoConferenceDlg::SetPartyControl(int max_users){
	
	int base_radio_id = IDC_RADIO_ALL_PARTY;
	int i;

	for(i=0;i<9;i++)
	{
		base_radio_id+=1;

		if(max_users > i)
			((CButton *)GetDlgItem(base_radio_id))->EnableWindow(TRUE);
		else
			((CButton *)GetDlgItem(base_radio_id))->EnableWindow(FALSE);
	}
}
void CVideoConferenceDlg::OnBnClickedRadioAllParty()
{
	if(conf) linphone_conference_show_member_at_index(conf,-1);
}

void CVideoConferenceDlg::OnBnClickedRadioLine1()
{
	if(conf) linphone_conference_show_member_at_index(conf,0);
}

void CVideoConferenceDlg::OnBnClickedRadioLine2()
{
	if(conf) linphone_conference_show_member_at_index(conf,1);
}

void CVideoConferenceDlg::OnBnClickedRadioLine3()
{
	if(conf) linphone_conference_show_member_at_index(conf,2);
}

void CVideoConferenceDlg::OnBnClickedRadioLine4()
{
	if(conf) linphone_conference_show_member_at_index(conf,3);
}

void CVideoConferenceDlg::OnBnClickedRadioLine5()
{
	if(conf) linphone_conference_show_member_at_index(conf,4);
}

void CVideoConferenceDlg::OnBnClickedRadioLine6()
{
	if(conf) linphone_conference_show_member_at_index(conf,5);
}

void CVideoConferenceDlg::OnBnClickedRadioLine7()
{
	if(conf) linphone_conference_show_member_at_index(conf,6);
}

void CVideoConferenceDlg::OnBnClickedRadioLine8()
{
	if(conf) linphone_conference_show_member_at_index(conf,7);
}

void CVideoConferenceDlg::OnBnClickedRadioLine9()
{
	if(conf) linphone_conference_show_member_at_index(conf,8);
}


void CVideoConferenceDlg::OnBnClickedButtonConfRecord()
{
	if(conf)
	{

		if(linphone_conference_in_video_recording(conf))
		{
			linphone_conference_stop_video_record(conf);
			linphone_conference_unmute_alls_ouput(conf);
		}else{
			CString tmp;
			mRTMPUrl.GetWindowText(tmp);

			if(tmp.GetLength()>3)
			{
				linphone_conference_mute_alls_ouput(conf);
				linphone_conference_start_video_record(conf,tmp.GetString());
			}
		}

		if(linphone_conference_in_video_recording(conf)){
			mBntConfRecdoing.SetWindowText("停止录制");
		}else{
			mBntConfRecdoing.SetWindowText("录制/发布会议");
		}

	}
}
