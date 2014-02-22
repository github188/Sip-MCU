// #ifndef WIN32
// #include <unistd.h>
// #include <cstdlib>
// #include <cstring>
// #include <netdb.h>
// #else
// #include <winsock2.h>
// #include <ws2tcpip.h>
// #include <wspiapi.h>
// #include <ws2def.h>
// #endif

#include "mediastreamer2/mediastream.h"
#include "msudt.h"
#include <udt.h>
#include "cc.h"

void* monitor(void*);

typedef int UDTSOCKET;

typedef struct _UDT_transport
{
	RtpSession *rtp_session;
	UDTSOCKET udt_socket;
	struct addrinfo hints, *local, *peer;
	RtpTransport ortp_transport;
	bool_t run;
	ms_thread_t	monitor_thread;
}UDT_transport;



static bool_t libudt_initialized = FALSE;

static void libudt_init_check()
{
	if(!libudt_initialized)
	{
		UDT_Init();
		libudt_initialized = TRUE;
	}
}

void UDT_Init()
{
	UDT::startup();
}

/*
typedef struct _RtpTransport
{
void *data;
ortp_socket_t (*t_getsocket)(struct _RtpTransport *t);
int  (*t_sendto)(struct _RtpTransport *t, mblk_t *msg , int flags, const struct sockaddr *to, socklen_t tolen);
int  (*t_recvfrom)(struct _RtpTransport *t, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen);
struct _RtpSession *session;//<back pointer to the owning session, set by oRTP
}  RtpTransport;
*/

ortp_socket_t udt_getsocket(struct _RtpTransport *t){
	UDT_transport *myudt = (UDT_transport *)t->data;

	return rtp_session_get_rtp_socket(myudt->rtp_session);
}

int  udt_sendto(struct _RtpTransport *t, mblk_t *msg , int flags, const struct sockaddr *to, socklen_t tolen){
	UDT_transport *myudt = (UDT_transport *)t->data;
	int ss=0;
	if (msg->b_cont!=NULL)
		msgpullup(msg,-1);

	int bufsz=(int) (msg->b_wptr - msg->b_rptr);

	return UDT::sendmsg(myudt->udt_socket, (char*)msg->b_rptr,bufsz,-1,true);
}

int  udt_recvfrom(struct _RtpTransport *t, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen){
	int rs=0;
	UDT_transport *myudt = (UDT_transport *)t->data;

	int bufsz = (int)(msg->b_datap->db_lim - msg->b_datap->db_base);

	rs = UDT::recvmsg(myudt->udt_socket, (char*)msg->b_wptr, bufsz);

	return (rs==-1)?  0:rs;
}

UDT_transport *UDT_transport_new(){
	UDT_transport *transport = ms_new0(UDT_transport,1);

	memset(&transport->hints, 0, sizeof(struct addrinfo));
	transport->hints.ai_flags = AI_PASSIVE;
	transport->hints.ai_family = AF_INET;
	transport->hints.ai_socktype = SOCK_DGRAM;
	//transport->hints.ai_socktype = SOCK_STREAM

	transport->ortp_transport.data = transport;
	transport->ortp_transport.t_getsocket = udt_getsocket;
	transport->ortp_transport.t_recvfrom = udt_recvfrom;
	transport->ortp_transport.t_sendto = udt_sendto;
	return transport;
}


void UDT_transport_set_rtp_session(UDT_transport *udt,RtpSession *session){
	char *local_port = ortp_strdup_printf("%d", session->rtp.loc_port);

	udt->rtp_session = session;
	udt->ortp_transport.session = session;

	if (0 != getaddrinfo(NULL, local_port, &udt->hints, &udt->local)){
		ms_message("incorrect network address.");
		return;
	}

	udt->udt_socket = UDT::socket(udt->local->ai_family,
		udt->local->ai_socktype,
		udt->local->ai_protocol);

	/*设置传输参数*/
	UDT::setsockopt(udt->udt_socket, 0, UDT_MSS, new int(ms_get_payload_max_size()), sizeof(int)); //MTU size
	UDT::setsockopt(udt->udt_socket, 0, UDT_RCVSYN, new bool(true), sizeof(bool));
	UDT::setsockopt(udt->udt_socket, 0, UDT_SNDSYN, new bool(true), sizeof(bool)); //非阻塞模式
	UDT::setsockopt(udt->udt_socket, 0, UDT_SNDBUF, new int(1024000), sizeof(int)); //接受缓冲
	UDT::setsockopt(udt->udt_socket, 0, UDT_RCVBUF, new int(1024000), sizeof(int)); //发送缓冲
	UDT::setsockopt(udt->udt_socket, 0, UDT_RCVTIMEO, new int(1), sizeof(int)); //阻塞模式超时设置
	UDT::setsockopt(udt->udt_socket, 0, UDT_SNDTIMEO, new int(1), sizeof(int)); //阻塞模式超时设置


	UDT::setsockopt(udt->udt_socket, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));

	if (UDT::ERROR == UDT::bind(udt->udt_socket, rtp_session_get_rtp_socket(udt->rtp_session))){
		ms_error("bind: %s",UDT::getlasterror().getErrorMessage());
	}

	ms_free(local_port);
}


int UDT_transport_connect(UDT_transport *udt)
{
	char * remote_ip = NULL;
	char * remote_port = NULL;

	remote_ip = ortp_strdup_printf("%s",inet_ntoa(((struct sockaddr_in *)&udt->rtp_session->rtp.rem_addr)->sin_addr));
	remote_port = ortp_strdup_printf("%d",ntohs(((struct sockaddr_in *)&udt->rtp_session->rtp.rem_addr)->sin_port));

	if (0 != getaddrinfo(remote_ip, remote_port, &udt->hints, &udt->peer)){
		return -1;
	}

	if (UDT::ERROR == UDT::connect(udt->udt_socket,udt-> peer->ai_addr,udt->peer->ai_addrlen)){
		ms_error("connect: %s",UDT::getlasterror().getErrorMessage());
		return -2;
	}

	//using CC method
	CUDPBlast* cchandle = NULL;
	int temp;
	UDT::getsockopt(udt->udt_socket, 0, UDT_CC, &cchandle, &temp);
	if (NULL != cchandle)
		cchandle->setRate(1);

	ms_free(remote_ip);
	ms_free(remote_port);
	return 0;
}

RtpTransport *UDT_create_rtp_transport(UDT_transport *udt)
{
	return &udt->ortp_transport;
}

void UDT_transport_disconnect(UDT_transport *udt)
{
	udt->run=FALSE;
	ms_thread_join(udt->monitor_thread,NULL);

	UDT::close(udt->udt_socket);
	ms_free(udt);
}


static UDT_transport *UDT_transport_connecting(RtpSession *session)
{
	RtpTransport * rtp_transport = NULL;
	libudt_init_check();

	UDT_transport *udt = UDT_transport_new();
	UDT_transport_set_rtp_session(udt,session);
	if(UDT_transport_connect(udt) < 0)
	{
		UDT_transport_disconnect(udt);
		ms_message("Can't start udt transport !!");
		udt =NULL;
		return NULL;
	}
	else
	{
		rtp_transport = UDT_create_rtp_transport(udt);

		if(rtp_transport){
			rtp_session_set_transports(session,rtp_transport,NULL);
			ms_message("enable mediastreamer2 udt transport !!");

			udt->run = TRUE;
			ms_thread_create(&udt->monitor_thread,NULL,&monitor,udt);
		}
		return udt;
	}
}

void* monitor(void* s)
{
	UDT_transport *udt = (UDT_transport *)s;
	UDTSOCKET u = udt->udt_socket;

	UDT::TRACEINFO perf;

	ms_message("SendRate(Mb/s)\t\tRTT(ms)\tCWnd\tPktSndPeriod(us)\tRecvACK\tRecvNAK");

	while (udt->run)
	{
#ifndef WIN32
		sleep(1);
#else
		Sleep(1000);
#endif

		if (UDT::ERROR == UDT::perfmon(u, &perf))
		{
			ms_message("perfmon: %s",UDT::getlasterror().getErrorMessage());
			break;
		}

		ms_message("%f\t\t%d\t%d\t%f\t\t\t%d\t%d",
			perf.mbpsSendRate,
			perf.msRTT,
			perf.pktCongestionWindow,
			perf.usPktSndPeriod, 
			perf.pktRecvACK,
			perf.pktRecvNAK);
	}

	ms_thread_exit(NULL);
#ifndef WIN32
	return NULL;
#else
	return 0;
#endif
}



typedef struct MsUDTState{
	UDT_transport *udt;
	RtpSession *session;
	bool_t start_connect;
}MsUDTState;

static void ms_udt_init(MSFilter *f){
	MsUDTState *s=ms_new(MsUDTState,1);;
	s->start_connect = FALSE;
	s->udt = NULL;
	s->session = NULL;
	f->data=s;
}

static void ms_udt_uninit(MSFilter *f){
	MsUDTState *s=(MsUDTState *)f->data;
	if(s->udt!=NULL)
	{
		UDT_transport_disconnect(s->udt);
		s->udt = NULL;
	}
	ms_free(s);
}

static void ms_udt_process(MSFilter *f){
	MsUDTState *s=(MsUDTState *)f->data;

	if(s->start_connect && s->session)
	{
		ms_message("enable udt transport !!!");
		s->udt = UDT_transport_connecting(s->session);
		s->start_connect =FALSE;
	}
}

static int ms_udt_set_rtp_session(MSFilter * f, void *arg)
{
	MsUDTState *d=(MsUDTState *)f->data;
	RtpSession *s = (RtpSession *) arg;
	if(d->session) return -1;
	d->session = s;
	d->start_connect =TRUE;
	return 0;
}

static MSFilterMethod sink_methods[]={
	{	MS_UDT_SET_RTP_SESSION , ms_udt_set_rtp_session },
	{ 0, NULL }
};

#ifdef _MSC_VER

extern "C" MSFilterDesc ms_udt_transport_desc={
	MS_UDT_TRANSPORT_ID,
	"MSUDTTransport",
	N_("UDT transport filter."),
	MS_FILTER_OTHER,
	NULL,
	0,
	0,
	ms_udt_init,
	NULL,
	ms_udt_process,
	NULL,
	ms_udt_uninit,
	sink_methods
};

#else

MSFilterDesc ms_udt_transport_desc={
	.id=MS_UDT_TRANSPORT_ID,
	.name="MSUDTTransport",
	.text=N_("Inter ticker communication filter."),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.preprocess=ms_udt_preprocess,
	.process=ms_udt_process,
	.methods=sink_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_udt_transport_desc)