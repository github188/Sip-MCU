#ifndef LIBUDT_H
#define LIBUDT_H

#ifdef __cplusplus
extern "C"{
#endif

struct _UDT_transport;
typedef struct _UDT_transport UDT_transport;

void UDT_Init();
UDT_transport *UDT_transport_new();
void UDT_transport_set_rtp_session(UDT_transport *udt,RtpSession *session);
int UDT_transport_connect(UDT_transport *udt);
void UDT_transport_disconnect(UDT_transport *udt);
RtpTransport *UDT_create_rtp_transport(UDT_transport *udt);

#ifdef __cplusplus
}
#endif

#include "mediastreamer2/msfilter.h"
#define MS_UDT_SET_RTP_SESSION		MS_FILTER_METHOD(MS_UDT_TRANSPORT_ID,0,RtpSession*)

#endif;