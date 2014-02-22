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

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

#include <mmsystem.h>
#ifdef _MSC_VER
#include <mmreg.h>
#endif
#include <msacm.h>

#if defined(_WIN32_WCE)
//#define DISABLE_SPEEX
//#define WCE_OPTICON_WORKAROUND 1000
#endif


#define WINSND_NBUFS 10
#define WINSND_OUT_DELAY 0.100
#define WINSND_OUT_NBUFS 20
#define WINSND_NSAMPLES 320
#define WINSND_MINIMUMBUFFER 5

static MSFilter *ms_winsnd_read_new(MSSndCard *card);
static MSFilter *ms_winsnd_write_new(MSSndCard *card);

static float input_level = 0.0f;
static float output_level = 0.0f;

#define LEVEL_MAX                   (0)
#define LEVEL_MIN                   (-50)

static float vol_to_db(float vol)
{
	/* avoid calling log10() on zero which yields inf or
	* negative numbers which yield nan */
	if ( vol <= 0.0f )
		return -99.9f;
	else
		return log10f(vol) * 20;
}



//·ÖÎöÉù¿¨ÒôÁ¿
static void calculate_level(short *audio, int len, float *level)
{
	int big_sample = 0;
	int i;
	for ( i = 0; i < len; i++ )
	{
		const int sample = abs(audio[i]);
		big_sample = sample > big_sample ? sample : big_sample;
	}
	*level += ((float)big_sample / 32767.0f - *level) / 5.0f;

}

int get_speech_volume_level(int *speech_input_level, int *speech_output_level)
{
	int i_vol =  (int)(input_level*100.0);
	int o_vol =  (int)(output_level*100.0);

	if(i_vol>100) i_vol =100;
	else if(i_vol < 0) i_vol = 0;

	if(o_vol>100) o_vol =100;
	else if(o_vol < 0) o_vol = 0;

	*speech_input_level = i_vol;
	*speech_output_level = o_vol;

}

typedef struct WinSndCard{
	int in_devid;
	int out_devid;
}WinSndCard;

#if 0
static void winsndcard_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent){
    MMRESULT mr = MMSYSERR_NOERROR;
    DWORD dwVolume = 0xFFFF;
    dwVolume = ((0xFFFF) * percent) / 100;

	switch(e){
		case MS_SND_CARD_MASTER:
            /*mr = waveOutSetVolume(d->waveoutdev, dwVolume); */
	        if (mr != MMSYSERR_NOERROR)
	        {
                ms_warning("Failed to set master volume. (waveOutSetVolume:0x%i)", mr);
                return;
	        }
        break;
        case MS_SND_CARD_CAPTURE:
		break;
		case MS_SND_CARD_PLAYBACK:
		break;
        default:
			ms_warning("winsnd_card_set_level: unsupported command.");
	}
}

static int winsndcard_get_level(MSSndCard *card, MSSndCardMixerElem e){
	switch(e){
		case MS_SND_CARD_MASTER:
            /*mr=waveOutGetVolume(d->waveoutdev, &dwVolume);*/
            /* Transform to 0 to 100 scale*/
            /*dwVolume = (dwVolume *100) / (0xFFFF);*/
            return 60;
        break;
        case MS_SND_CARD_CAPTURE:
		break;
		case MS_SND_CARD_PLAYBACK:
		break;
		default:
			ms_warning("winsnd_card_get_level: unsupported command.");
			return -1;
	}
	return -1;
}
#endif // 0

static void winsndcard_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent){
	WinSndCard *d=(WinSndCard*)card->data;

	UINT uMixerID;
	DWORD dwMixerHandle;
	MIXERLINE MixerLine;
	MIXERLINE Line;
	UINT uLineIndex;

	MIXERLINECONTROLS mlc = {0};
	MIXERCONTROL mc = {0};
	MIXERCONTROLDETAILS mcd = {0};
	MIXERCONTROLDETAILS_UNSIGNED mcdu = {0};

	MMRESULT mr = MMSYSERR_NOERROR;
	DWORD dwVolume = ((0xFFFF) * percent) / 100;

	WORD wLeftVol, wRightVol;
	DWORD dwNewVol;
	wLeftVol = LOWORD(dwVolume); // get higher WORD
	wRightVol = LOWORD(dwVolume); // get lower WORD

	dwNewVol = MAKELONG(wLeftVol, wRightVol);

	switch(e){
		case MS_SND_CARD_PLAYBACK:
		case MS_SND_CARD_MASTER:
			{

				mr = mixerGetID( (HMIXEROBJ)((d->out_devid == -1)? d->out_devid+1 : d->out_devid), &uMixerID, MIXER_OBJECTF_WAVEOUT );
				if ( mr != MMSYSERR_NOERROR )
				{
					ms_error("winsndcard_set_level: mixerGetID failed. (0x%x)", mr);
					return;
				}
				mr = mixerOpen( (LPHMIXER)&dwMixerHandle, uMixerID, 0L, 0L, 0L );
				if ( mr != MMSYSERR_NOERROR )
				{
					mixerClose( (HMIXER)dwMixerHandle );
					ms_error("winsndcard_set_level: mixerOpen failed. (0x%x)", mr);
					return;
				}
				memset( &MixerLine, 0, sizeof(MIXERLINE) );
				MixerLine.cbStruct = sizeof(MIXERLINE);
				if (MS_SND_CARD_MASTER==e)
					MixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
				else
					MixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT;
				mr = mixerGetLineInfo( (HMIXEROBJ)dwMixerHandle, &MixerLine, MIXER_GETLINEINFOF_COMPONENTTYPE );
				if ( mr != MMSYSERR_NOERROR )
				{
					mixerClose( (HMIXER)dwMixerHandle );
					ms_error("winsndcard_set_level: mixerGetLineInfo failed. (0x%x)", mr);
					return;
				}

				/* ms_message("Name: %s\n", MixerLine.szName); */
				/* ms_message("Source Line: %d\n", MixerLine.dwSource); */
				/* ms_message("ComponentType: %d\n", MixerLine.dwComponentType); */

				mlc.cbStruct = sizeof(MIXERLINECONTROLS);
				mlc.dwLineID = MixerLine.dwLineID;
				mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
				mlc.cControls = 1;
				mlc.pamxctrl = &mc;
				mlc.cbmxctrl = sizeof(MIXERCONTROL);
				mr = mixerGetLineControls((HMIXEROBJ)dwMixerHandle, 
					&mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);


				mcdu.dwValue = 65535*percent/100; /* the volume is a number between 0 and 65535 */

				mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
				mcd.hwndOwner = 0;
				mcd.dwControlID = mc.dwControlID;
				mcd.paDetails = &mcdu;
				mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
				mcd.cChannels = 1;
				mr = mixerSetControlDetails((HMIXEROBJ)dwMixerHandle, 
					&mcd, MIXER_SETCONTROLDETAILSF_VALUE);

				if (mr != MMSYSERR_NOERROR)
				{
					ms_error("winsndcard_set_level: mixerSetControlDetails failed. (0x%x)", mr);
					return;
				}
			}
			break;
		case MS_SND_CARD_CAPTURE:
			mr = mixerGetID( (HMIXEROBJ)((d->in_devid == -1)? d->in_devid+1 : d->in_devid), &uMixerID, MIXER_OBJECTF_WAVEIN );
			if ( mr != MMSYSERR_NOERROR )
			{
				ms_error("winsndcard_set_level: mixerGetID failed. (0x%x)", mr);
				return;
			}
			mr = mixerOpen( (LPHMIXER)&dwMixerHandle, uMixerID, 0L, 0L, 0L );
			if ( mr != MMSYSERR_NOERROR )
			{
				mixerClose( (HMIXER)dwMixerHandle );
				ms_error("winsndcard_set_level: mixerGetLineInfo failed. (0x%x)", mr);
				return;
			}
			memset( &MixerLine, 0, sizeof(MIXERLINE) );
			MixerLine.cbStruct = sizeof(MIXERLINE);
			MixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;
			mr = mixerGetLineInfo( (HMIXEROBJ)dwMixerHandle, &MixerLine, MIXER_GETLINEINFOF_COMPONENTTYPE );
			if ( mr != MMSYSERR_NOERROR )
			{
				mixerClose( (HMIXER)dwMixerHandle );
				ms_error("winsndcard_set_level: mixerGetLineInfo failed. (0x%x)", mr);
				return;
			}
			/* ms_message("Name: %s\n", MixerLine.szName); */
			/* ms_message("Source Line: %d\n", MixerLine.dwSource); */
			/* ms_message("ComponentType: %d\n", MixerLine.dwComponentType); */

			mlc.cbStruct = sizeof(MIXERLINECONTROLS);
			mlc.dwLineID = MixerLine.dwLineID;
			mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
			mlc.cControls = 1;
			mlc.pamxctrl = &mc;
			mlc.cbmxctrl = sizeof(MIXERCONTROL);
			mr = mixerGetLineControls((HMIXEROBJ)dwMixerHandle, 
				&mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);

			if (mr == MMSYSERR_NOERROR)
			{
				mcdu.dwValue = 65535*percent/100; /* the volume is a number between 0 and 65535 */

				mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
				mcd.hwndOwner = 0;
				mcd.dwControlID = mc.dwControlID;
				mcd.paDetails = &mcdu;
				mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
				mcd.cChannels = 1;
				mr = mixerSetControlDetails((HMIXEROBJ)dwMixerHandle, 
					&mcd, MIXER_SETCONTROLDETAILSF_VALUE);

				if (mr == MMSYSERR_NOERROR)
				{
					return;
				}
				ms_warning("winsndcard_set_level: mixerSetControlDetails failed. (0x%x)", mr);
				ms_warning("winsndcard_set_level: control the SRC_MICROPHONE instead");
			}
			else
			{
				ms_message("winsndcard_set_level: mixerGetLineControls failed. (0x%x)", mr);
				ms_message("winsndcard_set_level: control the SRC_MICROPHONE instead");
			}

			/* In case capture doesn't work: use the SRC_MICROPHONE volume */

			for (uLineIndex = 0; uLineIndex < MixerLine.cConnections; uLineIndex++)
			{
				memset( &Line, 0, sizeof(MIXERLINE) );
				Line.cbStruct = sizeof(MIXERLINE);
				Line.dwDestination = MixerLine.dwDestination;
				Line.dwSource = uLineIndex;
				mr = mixerGetLineInfo( (HMIXEROBJ)dwMixerHandle, &Line, MIXER_GETLINEINFOF_LINEID);
				if ( mr != MMSYSERR_NOERROR )
				{
					mixerClose( (HMIXER)dwMixerHandle );
					ms_error("winsndcard_set_level: mixerGetLineInfo failed. (0x%x)", mr);
					return;
				}

				/* ms_message("Name: %s\n", MixerLine.szName); */
				/* ms_message("Source Line: %d\n", MixerLine.dwSource); */
				/* ms_message("LineID: %d\n", MixerLine.dwLineID); */
				/* ms_message("ComponentType: %d\n", MixerLine.dwComponentType); */

				memset( &Line, 0, sizeof(MIXERLINE) );
				Line.cbStruct = sizeof(MIXERLINE);
				Line.dwDestination = MixerLine.dwDestination;
				Line.dwSource = uLineIndex;
				mr = mixerGetLineInfo( (HMIXEROBJ)dwMixerHandle, &Line, MIXER_GETLINEINFOF_SOURCE);
				if ( mr != MMSYSERR_NOERROR )
				{
					mixerClose( (HMIXER)dwMixerHandle );
					ms_warning("winsndcard_set_level: mixerGetLineInfo failed. (0x%x)", mr);
					return;
				}

				/* ms_message("Name: %s\n", MixerLine.szName); */
				/* ms_message("Source Line: %d\n", MixerLine.dwSource); */
				/* ms_message("LineID: %d\n", MixerLine.dwLineID); */
				/* ms_message("ComponentType: %d\n", MixerLine.dwComponentType); */

				if (MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE == Line.dwComponentType)  
				{
					LPMIXERCONTROL pmxctrl = (LPMIXERCONTROL)malloc(sizeof(MIXERCONTROL));
					MIXERLINECONTROLS mxlctrl = {sizeof(mxlctrl), Line.dwLineID, MIXERCONTROL_CONTROLTYPE_VOLUME, 1, sizeof(MIXERCONTROL), pmxctrl};  
					if(!mixerGetLineControls((HMIXEROBJ)dwMixerHandle, &mxlctrl,  
						MIXER_GETLINECONTROLSF_ONEBYTYPE)){  
							DWORD cChannels = Line.cChannels;
							LPMIXERCONTROLDETAILS_UNSIGNED pUnsigned;
							MIXERCONTROLDETAILS mxcd;
							if (MIXERCONTROL_CONTROLF_UNIFORM & pmxctrl->fdwControl)  
								cChannels = 1;  
							pUnsigned =  
								(LPMIXERCONTROLDETAILS_UNSIGNED)  
								malloc(cChannels * sizeof(MIXERCONTROLDETAILS_UNSIGNED));

							mxcd.cbStruct = sizeof(mxcd);
							mxcd.dwControlID = pmxctrl->dwControlID;
							mxcd.cChannels = cChannels;
							mxcd.hwndOwner = (HWND)0;
							mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
							mxcd.paDetails = (LPVOID) pUnsigned;

							mixerGetControlDetails((HMIXEROBJ)dwMixerHandle, &mxcd,  
								MIXER_SETCONTROLDETAILSF_VALUE);  
							pUnsigned[0].dwValue = pUnsigned[cChannels - 1].dwValue
								=  pmxctrl->Bounds.dwMaximum*percent/100;
							mixerSetControlDetails((HMIXEROBJ)dwMixerHandle, &mxcd,  
								MIXER_SETCONTROLDETAILSF_VALUE);  
							free(pmxctrl);  
							free(pUnsigned);  
					}  
					else  
						free(pmxctrl);  
				}
			}
			mixerClose( (HMIXER)dwMixerHandle );
			if (mr != MMSYSERR_NOERROR)
			{
				ms_warning("winsndcard_set_level: mixerClose failed. (0x%x)", mr);
				return;
			}
			break;
		default:
			ms_warning("winsnd_card_set_level: unsupported command.");
	}
}

static int winsndcard_get_level(MSSndCard *card, MSSndCardMixerElem e){
	WinSndCard *d=(WinSndCard*)card->data;

	UINT uMixerID;
	DWORD dwMixerHandle;
	MIXERLINE MixerLine;
	MIXERLINE Line;
	UINT uLineIndex;

	MIXERLINECONTROLS mlc = {0};
	MIXERCONTROL mc = {0};
	MIXERCONTROLDETAILS mcd = {0};
	MIXERCONTROLDETAILS_UNSIGNED mcdu = {0};

	MMRESULT mr = MMSYSERR_NOERROR;
	int percent;

	switch(e){
		case MS_SND_CARD_MASTER:
		case MS_SND_CARD_PLAYBACK:
			{
				mr = mixerGetID( (HMIXEROBJ)d->out_devid, &uMixerID, MIXER_OBJECTF_WAVEOUT );
				if ( mr != MMSYSERR_NOERROR )
				{
					ms_error("winsndcard_get_level: mixerGetID failed. (0x%x)", mr);
					return -1;
				}
				mr = mixerOpen( (LPHMIXER)&dwMixerHandle, uMixerID, 0L, 0L, 0L );
				if ( mr != MMSYSERR_NOERROR )
				{
					mixerClose( (HMIXER)dwMixerHandle );
					ms_error("winsndcard_get_level: mixerOpen failed. (0x%x)", mr);
					return -1;
				}
				memset( &MixerLine, 0, sizeof(MIXERLINE) );
				MixerLine.cbStruct = sizeof(MIXERLINE);
				if (MS_SND_CARD_MASTER==e)
					MixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
				else
					MixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT;
				mr = mixerGetLineInfo( (HMIXEROBJ)dwMixerHandle, &MixerLine, MIXER_GETLINEINFOF_COMPONENTTYPE );
				if ( mr != MMSYSERR_NOERROR )
				{
					mixerClose( (HMIXER)dwMixerHandle );
					ms_error("winsndcard_get_level: mixerGetLineInfo failed. (0x%x)", mr);
					return -1;
				}

				/* ms_message("Name: %s\n", MixerLine.szName); */
				/* ms_message("Source Line: %d\n", MixerLine.dwSource); */
				/* ms_message("ComponentType: %d\n", MixerLine.dwComponentType); */

				mlc.cbStruct = sizeof(MIXERLINECONTROLS);
				mlc.dwLineID = MixerLine.dwLineID;
				mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
				mlc.cControls = 1;
				mlc.pamxctrl = &mc;
				mlc.cbmxctrl = sizeof(MIXERCONTROL);
				mr = mixerGetLineControls((HMIXEROBJ)dwMixerHandle, 
					&mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);
				if (mr != MMSYSERR_NOERROR)
				{
					ms_warning("winsndcard_get_level: mixerGetLineControls failed. (0x%x)", mr);
					return -1;
				}

				mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
				mcd.hwndOwner = 0;
				mcd.dwControlID = mc.dwControlID;
				mcd.paDetails = &mcdu;
				mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
				mcd.cChannels = 1;
				mr = mixerGetControlDetails((HMIXEROBJ)dwMixerHandle, &mcd,  
					MIXER_SETCONTROLDETAILSF_VALUE);  
				percent = (mcdu.dwValue *100) / (mc.Bounds.dwMaximum);

				if (mr != MMSYSERR_NOERROR)
				{
					ms_warning("winsndcard_get_level: mixerGetControlDetails failed. (0x%x)", mr);
					return -1;
				}
				return percent;
			}
			break;
		case MS_SND_CARD_CAPTURE:
			mr = mixerGetID( (HMIXEROBJ)d->in_devid, &uMixerID, MIXER_OBJECTF_WAVEIN );
			if ( mr != MMSYSERR_NOERROR )
			{
				ms_warning("winsndcard_get_level: mixerGetID failed. (0x%x)", mr);
				return -1;
			}
			mr = mixerOpen( (LPHMIXER)&dwMixerHandle, uMixerID, 0L, 0L, 0L );
			if ( mr != MMSYSERR_NOERROR )
			{
				mixerClose( (HMIXER)dwMixerHandle );
				ms_warning("winsndcard_get_level: mixerOpen failed. (0x%x)", mr);
				return -1;
			}
			memset( &MixerLine, 0, sizeof(MIXERLINE) );
			MixerLine.cbStruct = sizeof(MIXERLINE);
			MixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;
			mr = mixerGetLineInfo( (HMIXEROBJ)dwMixerHandle, &MixerLine, MIXER_GETLINEINFOF_COMPONENTTYPE );
			if ( mr != MMSYSERR_NOERROR )
			{
				mixerClose( (HMIXER)dwMixerHandle );
				ms_error("winsndcard_get_level: mixerGetLineInfo failed. (0x%x)", mr);
				return -1;
			}

			mlc.cbStruct = sizeof(MIXERLINECONTROLS);
			mlc.dwLineID = MixerLine.dwLineID;
			mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
			mlc.cControls = 1;
			mlc.pamxctrl = &mc;
			mlc.cbmxctrl = sizeof(MIXERCONTROL);
			mr = mixerGetLineControls((HMIXEROBJ)dwMixerHandle, 
				&mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);
			if (mr == MMSYSERR_NOERROR)
			{
				mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
				mcd.hwndOwner = 0;
				mcd.dwControlID = mc.dwControlID;
				mcd.paDetails = &mcdu;
				mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
				mcd.cChannels = 1;
				mr = mixerGetControlDetails((HMIXEROBJ)dwMixerHandle, &mcd,  
					MIXER_SETCONTROLDETAILSF_VALUE);  
				percent = (mcdu.dwValue *100) / (mc.Bounds.dwMaximum);

				if (mr == MMSYSERR_NOERROR)
				{
					return percent;
				}
				ms_warning("winsndcard_get_level: mixerGetControlDetails failed. (0x%x)", mr);
				ms_warning("winsndcard_get_level: control the SRC_MICROPHONE instead");
			}
			else
			{
				ms_warning("winsndcard_get_level: mixerGetLineControls failed. (0x%x)", mr);
				ms_warning("winsndcard_get_level: control the SRC_MICROPHONE instead");
			}

			/* ms_message("Name: %s\n", MixerLine.szName); */
			/* ms_message("Source Line: %d\n", MixerLine.dwSource); */
			/* ms_message("ComponentType: %d\n", MixerLine.dwComponentType); */

			for (uLineIndex = 0; uLineIndex < MixerLine.cConnections; uLineIndex++)
			{
				memset( &Line, 0, sizeof(MIXERLINE) );
				Line.cbStruct = sizeof(MIXERLINE);
				Line.dwDestination = MixerLine.dwDestination;
				Line.dwSource = uLineIndex;
				mr = mixerGetLineInfo( (HMIXEROBJ)dwMixerHandle, &Line, MIXER_GETLINEINFOF_LINEID);
				if ( mr != MMSYSERR_NOERROR )
				{
					mixerClose( (HMIXER)dwMixerHandle );
					ms_error("winsndcard_get_level: mixerGetLineInfo failed. (0x%x)", mr);
					return -1;
				}

				/* ms_message("Name: %s\n", MixerLine.szName); */
				/* ms_message("Source Line: %d\n", MixerLine.dwSource); */
				/* ms_message("LineID: %d\n", MixerLine.dwLineID); */
				/* ms_message("ComponentType: %d\n", MixerLine.dwComponentType); */

				memset( &Line, 0, sizeof(MIXERLINE) );
				Line.cbStruct = sizeof(MIXERLINE);
				Line.dwDestination = MixerLine.dwDestination;
				Line.dwSource = uLineIndex;
				mr = mixerGetLineInfo( (HMIXEROBJ)dwMixerHandle, &Line, MIXER_GETLINEINFOF_SOURCE);
				if ( mr != MMSYSERR_NOERROR )
				{
					mixerClose( (HMIXER)dwMixerHandle );
					ms_error("winsndcard_get_level: mixerGetLineInfo failed. (0x%x)", mr);
					return -1;
				}

				/* ms_message("Name: %s\n", MixerLine.szName); */
				/* ms_message("Source Line: %d\n", MixerLine.dwSource); */
				/* ms_message("LineID: %d\n", MixerLine.dwLineID); */
				/* ms_message("ComponentType: %d\n", MixerLine.dwComponentType); */

				if (MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE == Line.dwComponentType)  
				{
					LPMIXERCONTROL pmxctrl = (LPMIXERCONTROL)malloc(sizeof(MIXERCONTROL));
					MIXERLINECONTROLS mxlctrl = {sizeof(mxlctrl), Line.dwLineID, MIXERCONTROL_CONTROLTYPE_VOLUME, 1, sizeof(MIXERCONTROL), pmxctrl};  
					if(!mixerGetLineControls((HMIXEROBJ)dwMixerHandle, &mxlctrl,  
						MIXER_GETLINECONTROLSF_ONEBYTYPE)){  
							DWORD cChannels = Line.cChannels;
							LPMIXERCONTROLDETAILS_UNSIGNED pUnsigned;
							MIXERCONTROLDETAILS mxcd;
							if (MIXERCONTROL_CONTROLF_UNIFORM & pmxctrl->fdwControl)  
								cChannels = 1;  
							pUnsigned =  
								(LPMIXERCONTROLDETAILS_UNSIGNED)  
								malloc(cChannels * sizeof(MIXERCONTROLDETAILS_UNSIGNED));

							mxcd.cbStruct = sizeof(mxcd);
							mxcd.dwControlID = pmxctrl->dwControlID;
							mxcd.cChannels = cChannels;
							mxcd.hwndOwner = (HWND)0;
							mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
							mxcd.paDetails = (LPVOID) pUnsigned;

							mixerGetControlDetails((HMIXEROBJ)dwMixerHandle, &mxcd,  
								MIXER_SETCONTROLDETAILSF_VALUE);  
							percent = (pUnsigned[0].dwValue *100) / (pmxctrl->Bounds.dwMaximum);
							free(pmxctrl);
							free(pUnsigned);
					}  
					else  
						free(pmxctrl);  
				}
			}
			mixerClose( (HMIXER)dwMixerHandle );
			if (mr != MMSYSERR_NOERROR)
			{
				ms_error("winsndcard_get_level: mixerClose failed. (0x%x)", mr);
				return -1;
			}
			return percent;
			break;
		default:
			ms_warning("winsndcard_get_level: unsupported command.");
			return -1;
	}
	return -1;
}

static void winsndcard_set_source(MSSndCard *card, MSSndCardCapture source){

	switch(source){
		case MS_SND_CARD_MIC:
		break;
		case MS_SND_CARD_LINE:
		break;
	}	
}

static void winsndcard_init(MSSndCard *card){
	WinSndCard *c=(WinSndCard *)ms_new(WinSndCard,1);
	card->data=c;
}

static void winsndcard_uninit(MSSndCard *card){
	ms_free(card->data);
}

static void winsndcard_detect(MSSndCardManager *m);
static  MSSndCard *winsndcard_dup(MSSndCard *obj);

MSSndCardDesc winsnd_card_desc={
	"WINSND",
	winsndcard_detect,
	winsndcard_init,
	winsndcard_set_level,
	winsndcard_get_level,
	winsndcard_set_source,
	NULL,
	NULL,
	ms_winsnd_read_new,
	ms_winsnd_write_new,
	winsndcard_uninit,
	winsndcard_dup
};

static  MSSndCard *winsndcard_dup(MSSndCard *obj){
	MSSndCard *card=ms_snd_card_new(&winsnd_card_desc);
	card->name=ms_strdup(obj->name);
	card->data=ms_new(WinSndCard,1);
	memcpy(card->data,obj->data,sizeof(WinSndCard));
	return card;
}

static MSSndCard *winsndcard_new(const char *name, int in_dev, int out_dev, unsigned cap){
	MSSndCard *card=ms_snd_card_new(&winsnd_card_desc);
	WinSndCard *d=(WinSndCard*)card->data;
	card->name=ms_strdup(name);
	d->in_devid=in_dev;
	d->out_devid=out_dev;
	card->capabilities=cap;
	return card;
}

static void add_or_update_card(MSSndCardManager *m, const char *name, int indev, int outdev, unsigned int capability){
	MSSndCard *card;
	const MSList *elem=ms_snd_card_manager_get_list(m);
	for(;elem!=NULL;elem=elem->next){
		card=(MSSndCard*)elem->data;
		if (strcmp(card->name,name)==0){
			/*update already entered card */
			WinSndCard *d=(WinSndCard*)card->data;
			card->capabilities|=capability;
			if (indev!=-1) 
				d->in_devid=indev;
			if (outdev!=-1)
				d->out_devid=outdev;
				
			return;
		}
	}
	/* add this new card:*/
	ms_snd_card_manager_add_card(m,winsndcard_new(name,indev,outdev,capability));
}

#if 0
static void winsndcard_detect(MSSndCardManager *m){
    MMRESULT mr = NOERROR;
    unsigned int nOutDevices = waveOutGetNumDevs ();
    unsigned int nInDevices = waveInGetNumDevs ();
    unsigned int item;

    if (nOutDevices>nInDevices)
		nInDevices = nOutDevices;

    for (item = 0; item < nInDevices; item++){
		
        WAVEINCAPS incaps;
        WAVEOUTCAPS outcaps;
        mr = waveInGetDevCaps (item, &incaps, sizeof (WAVEINCAPS));
        if (mr == MMSYSERR_NOERROR)
		{
#if defined(_WIN32_WCE)
			char card[256];
			snprintf(card, sizeof(card), "Input card %i", item);
			add_or_update_card(m,card,item,-1,MS_SND_CARD_CAP_CAPTURE);
			/* _tprintf(L"new card: %s", incaps.szPname); */
#else
			add_or_update_card(m,incaps.szPname,item,-1,MS_SND_CARD_CAP_CAPTURE);
#endif
		}
    	mr = waveOutGetDevCaps (item, &outcaps, sizeof (WAVEOUTCAPS));
        if (mr == MMSYSERR_NOERROR)
		{
#if defined(_WIN32_WCE)
			char card[256];
			snprintf(card, sizeof(card), "Output card %i", item);
    		add_or_update_card(m,card,-1,item,MS_SND_CARD_CAP_PLAYBACK);
			/* _tprintf(L"new card: %s", outcaps.szPname); */
#else
    		add_or_update_card(m,outcaps.szPname,-1,item,MS_SND_CARD_CAP_PLAYBACK);
#endif
		}
    }
}
#endif // 0

static void winsndcard_detect(MSSndCardManager *m){
	MMRESULT mr = NOERROR;
	int nOutDevices = waveOutGetNumDevs ();
	int nInDevices = waveInGetNumDevs ();
	int item = -1;

	if (nOutDevices>nInDevices)
		nInDevices = nOutDevices;

	for (item = -1; item < nInDevices; item++){

		WAVEINCAPS incaps;
		WAVEOUTCAPS outcaps;
		mr = waveInGetDevCaps (item, &incaps, sizeof (WAVEINCAPS));
		if (mr == MMSYSERR_NOERROR)
		{
#if defined(_WIN32_WCE)
			char card[256];
			snprintf(card, sizeof(card), "Input card %i", item);
			add_or_update_card(m,card,item,-1,MS_SND_CARD_CAP_CAPTURE);
			/* _tprintf(L"new card: %s", incaps.szPname); */
#else
			add_or_update_card(m,incaps.szPname,item,-1,MS_SND_CARD_CAP_CAPTURE);
#endif
		}
		mr = waveOutGetDevCaps (item, &outcaps, sizeof (WAVEOUTCAPS));
		if (mr == MMSYSERR_NOERROR)
		{
#if defined(_WIN32_WCE)
			char card[256];
			snprintf(card, sizeof(card), "Output card %i", item);
			add_or_update_card(m,card,-1,item,MS_SND_CARD_CAP_PLAYBACK);
			/* _tprintf(L"new card: %s", outcaps.szPname); */
#else
			add_or_update_card(m,outcaps.szPname,-1,item,MS_SND_CARD_CAP_PLAYBACK);
#endif
		}
	}
}

typedef struct WinSnd{
	int dev_id;
	HWAVEIN indev;
	HWAVEOUT outdev;
	WAVEFORMATEX wfx;
	WAVEHDR hdrs_read[WINSND_NBUFS];
	WAVEHDR hdrs_write[WINSND_OUT_NBUFS];
	queue_t rq;
	ms_mutex_t mutex;
	unsigned int bytes_read;
	unsigned int nbufs_playing;
	bool_t running;
	int outcurbuf;
	int nsamples;
	queue_t wq;
	int32_t stat_input;
	int32_t stat_output;
	int32_t stat_notplayed;

	int32_t stat_minimumbuffer;
	int ready;
	int workaround; /* workaround for opticon audio device */
	bool_t overrun;
}WinSnd;

static void winsnd_apply_settings(WinSnd *d){
	d->wfx.nBlockAlign=d->wfx.nChannels*d->wfx.wBitsPerSample/8;
	d->wfx.nAvgBytesPerSec=d->wfx.nSamplesPerSec*d->wfx.nBlockAlign;
}


#ifndef _TRUE_TIME
static uint64_t winsnd_get_cur_time( void *data){
	WinSnd *d=(WinSnd*)data;
	uint64_t curtime=((uint64_t)d->bytes_read*1000)/(uint64_t)d->wfx.nAvgBytesPerSec;
	/* ms_debug("winsnd_get_cur_time: bytes_read=%u return %lu\n",d->bytes_read,(unsigned long)curtime); */
	return curtime;
}
#endif


static void winsnd_init(MSFilter *f){
	WinSnd *d=(WinSnd *)ms_new0(WinSnd,1);
	d->wfx.wFormatTag = WAVE_FORMAT_PCM;
	d->wfx.cbSize = 0;
	d->wfx.nAvgBytesPerSec = 16000;
	d->wfx.nBlockAlign = 2;
	d->wfx.nChannels = 1;
	d->wfx.nSamplesPerSec = 8000;
	d->wfx.wBitsPerSample = 16;
	qinit(&d->rq);
	qinit(&d->wq);
	d->ready=0;
	d->workaround=0;
	ms_mutex_init(&d->mutex,NULL);
	f->data=d;

	d->stat_input=0;
	d->stat_output=0;
	d->stat_notplayed=0;
	d->stat_minimumbuffer=WINSND_MINIMUMBUFFER;
}

static void winsnd_uninit(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	flushq(&d->rq,0);
	flushq(&d->wq,0);
	d->ready=0;
	d->workaround=0;
	ms_mutex_destroy(&d->mutex);
	ms_free(f->data);
}

static void add_input_buffer(WinSnd *d, WAVEHDR *hdr, int buflen){
	mblk_t *m=allocb(buflen,0);
	MMRESULT mr;
	memset(hdr,0,sizeof(*hdr));
	if (buflen==0) ms_error("add_input_buffer: buflen=0 !");
	hdr->lpData=(LPSTR)m->b_wptr;
	hdr->dwBufferLength=buflen;
	hdr->dwFlags = 0;
	hdr->dwUser = (DWORD)m;
	mr = waveInPrepareHeader (d->indev,hdr,sizeof(*hdr));
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveInPrepareHeader() error");
		return ;
	}
	mr=waveInAddBuffer(d->indev,hdr,sizeof(*hdr));
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveInAddBuffer() error");
		return ;
	}
}

static void CALLBACK 
read_callback (HWAVEIN waveindev, UINT uMsg, DWORD dwInstance, DWORD dwParam1,
                DWORD dwParam2)
{
	WAVEHDR *wHdr=(WAVEHDR *) dwParam1;
	MSFilter *f=(MSFilter *)dwInstance;
	WinSnd *d=(WinSnd*)f->data;
	mblk_t *m;
	int bsize;
	switch (uMsg){
		case WIM_OPEN:
			ms_debug("read_callback : WIM_OPEN");
		break;
		case WIM_CLOSE:
			ms_debug("read_callback : WIM_CLOSE");
		break;
		case WIM_DATA:
			bsize=wHdr->dwBytesRecorded;
			/* ms_warning("read_callback : WIM_DATA (%p,%i)",wHdr,bsize); */
			m=(mblk_t*)wHdr->dwUser;
			calculate_level((short *)m->b_wptr,bsize/2,&input_level);
			m->b_wptr+=bsize;
			wHdr->dwUser=0;
			ms_mutex_lock(&d->mutex);
			putq(&d->rq,m);
			ms_mutex_unlock(&d->mutex);
			d->bytes_read+=wHdr->dwBufferLength;
			d->stat_input++;
			d->stat_input++;
#ifdef WIN32_TIMERS
			if (f->ticker->TimeEvent!=NULL)
				SetEvent(f->ticker->TimeEvent);
#endif
		break;
	}
}


static void winsnd_read_preprocess(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	MMRESULT mr;
	int i;
	int bsize;
	DWORD dwFlag;

	d->stat_input=0;
	d->stat_output=0;
	d->stat_notplayed=0;
	d->stat_minimumbuffer=WINSND_MINIMUMBUFFER;

	winsnd_apply_settings(d);
	/* Init Microphone device */
	dwFlag = CALLBACK_FUNCTION;
	if (d->dev_id != WAVE_MAPPER)
		dwFlag = WAVE_MAPPED | CALLBACK_FUNCTION;
	mr = waveInOpen (&d->indev, d->dev_id, &d->wfx,
	            (DWORD) read_callback, (DWORD)f, dwFlag);
	if (mr != MMSYSERR_NOERROR)
	{
	    ms_error("Failed to prepare windows sound device. (waveInOpen:0x%i)", mr);
		mr = waveInOpen (&d->indev, WAVE_MAPPER, &d->wfx,
					(DWORD) read_callback, (DWORD)f, CALLBACK_FUNCTION);
		if (mr != MMSYSERR_NOERROR)
		{
			d->indev=NULL;
			ms_error("Failed to prepare windows sound device. (waveInOpen:0x%i)", mr);
		    return ;
		}
	}
	bsize=WINSND_NSAMPLES*d->wfx.nAvgBytesPerSec/8000;
	ms_debug("Using input buffers of %i bytes",bsize);
	for(i=0;i<WINSND_NBUFS;++i){
		WAVEHDR *hdr=&d->hdrs_read[i];
		add_input_buffer(d,hdr,bsize);
	}
	d->running=TRUE;
	mr=waveInStart(d->indev);
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveInStart() error");
		return ;
	}
#ifndef _TRUE_TIME
	ms_ticker_set_time_func(f->ticker,winsnd_get_cur_time,d);
#endif
}

static void winsnd_read_postprocess(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	MMRESULT mr;
	int i;
#ifndef _TRUE_TIME
	ms_ticker_set_time_func(f->ticker,NULL,NULL);
#endif
	d->running=FALSE;
	mr=waveInStop(d->indev);
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveInStop() error");
		return ;
	}
	mr=waveInReset(d->indev);
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveInReset() error");
		return ;
	}
	for(i=0;i<WINSND_NBUFS;++i){
		WAVEHDR *hdr=&d->hdrs_read[i];
		if (hdr->dwFlags & WHDR_PREPARED)
		{
			mr = waveInUnprepareHeader(d->indev,hdr,sizeof (*hdr));
			if (mr != MMSYSERR_NOERROR){
				ms_error("waveInUnPrepareHeader() error");
			}
		}
	}
	mr = waveInClose(d->indev);
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveInClose() error");
		return ;
	}

	ms_message("Shutting down sound device (playing: %i) (input-output: %i) (notplayed: %i)", d->nbufs_playing, d->stat_input - d->stat_output, d->stat_notplayed);
	flushq(&d->rq,0);
}

static void winsnd_read_process(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	mblk_t *m;
	int i;
	ms_mutex_lock(&d->mutex);
	while((m=getq(&d->rq))!=NULL){
		ms_queue_put(f->outputs[0],m);
	}
	ms_mutex_unlock(&d->mutex);
	for(i=0;i<WINSND_NBUFS;++i){
		WAVEHDR *hdr=&d->hdrs_read[i];
		if (hdr->dwUser==0) {
			MMRESULT mr;
			mr=waveInUnprepareHeader(d->indev,hdr,sizeof(*hdr));
			if (mr!=MMSYSERR_NOERROR)
				ms_warning("winsnd_read_process: Fail to unprepare header!");
			add_input_buffer(d,hdr,hdr->dwBufferLength);
		}
	}
}

static void CALLBACK
write_callback(HWAVEOUT outdev, UINT uMsg, DWORD dwInstance,
                 DWORD dwParam1, DWORD dwParam2)
{
	WAVEHDR *hdr=(WAVEHDR *) dwParam1;
	WinSnd *d=(WinSnd*)dwInstance;
	
	switch (uMsg){
		case WOM_OPEN:
			break;
		case WOM_CLOSE:
		case WOM_DONE:
			if (hdr){
				calculate_level((short *)hdr->lpData,hdr->dwBufferLength/2,&output_level);
				d->nbufs_playing--;
			}
			if (d->stat_output==0)
			{
				d->stat_input=1; /* reset */
				d->stat_notplayed=0;
			}
			d->stat_output++;
		break;
	}
}

static void winsnd_write_preprocess(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	MMRESULT mr;
	DWORD dwFlag;
	int i;

	d->stat_input=0;
	d->stat_output=0;
	d->stat_notplayed=0;
	d->stat_minimumbuffer=WINSND_MINIMUMBUFFER;

	winsnd_apply_settings(d);
	/* Init Microphone device */
	dwFlag = CALLBACK_FUNCTION;
	if (d->dev_id != WAVE_MAPPER)
		dwFlag = WAVE_MAPPED | CALLBACK_FUNCTION;
	mr = waveOutOpen (&d->outdev, d->dev_id, &d->wfx,
	            (DWORD) write_callback, (DWORD)d, dwFlag);
	if (mr != MMSYSERR_NOERROR)
	{
		ms_error("Failed to open windows sound device %i. (waveOutOpen:0x%i)",d->dev_id, mr);
		mr = waveOutOpen (&d->outdev, WAVE_MAPPER, &d->wfx,
					(DWORD) write_callback, (DWORD)d, CALLBACK_FUNCTION);
		if (mr != MMSYSERR_NOERROR)
		{
			ms_error("Failed to open windows sound device %i. (waveOutOpen:0x%i)",d->dev_id, mr);
			d->outdev=NULL;
			return ;
		}
	}
	for(i=0;i<WINSND_OUT_NBUFS;++i){
		WAVEHDR *hdr=&d->hdrs_write[i];
		hdr->dwFlags=0;
		hdr->dwUser=0;
	}
	d->outcurbuf=0;
	d->overrun=FALSE;
	d->nsamples=0;
}

static void winsnd_write_postprocess(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	MMRESULT mr;
	int i;
	if (d->outdev==NULL) return;
	mr=waveOutReset(d->outdev);
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveOutReset() error");
		return ;
	}
	for(i=0;i<WINSND_OUT_NBUFS;++i){
		WAVEHDR *hdr=&d->hdrs_write[i];
		mblk_t *old;
		if (hdr->dwFlags & WHDR_DONE){
			mr=waveOutUnprepareHeader(d->outdev,hdr,sizeof(*hdr));
			if (mr != MMSYSERR_NOERROR){
				ms_error("waveOutUnprepareHeader error");
			}
			old=(mblk_t*)hdr->dwUser;
			if (old) freemsg(old);
			hdr->dwUser=0;
		}
	}
	mr=waveOutClose(d->outdev);
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveOutClose() error");
		return ;
	}
	d->ready=0;
	d->workaround=0;
}

static void playout_buf(WinSnd *d, WAVEHDR *hdr, mblk_t *m){
	MMRESULT mr;
	hdr->dwUser=(DWORD)m;
	hdr->lpData=(LPSTR)m->b_rptr;
	hdr->dwBufferLength=msgdsize(m);
	hdr->dwFlags = 0;
	mr = waveOutPrepareHeader(d->outdev,hdr,sizeof(*hdr));
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveOutPrepareHeader() error");
		d->stat_notplayed++;
	}
	mr=waveOutWrite(d->outdev,hdr,sizeof(*hdr));
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveOutWrite() error");
		d->stat_notplayed++;
	}else {
		d->nbufs_playing++;
	}
}

static void winsnd_write_process(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	mblk_t *m;
	MMRESULT mr;
	mblk_t *old;
	if (d->outdev==NULL) {
		ms_queue_flush(f->inputs[0]);
		return;
	}
	if (d->overrun){
		ms_warning("nbufs_playing=%i",d->nbufs_playing);
		if (d->nbufs_playing>0){
			ms_queue_flush(f->inputs[0]);
			return;
		}
		else d->overrun=FALSE;
	}
	while(1){
		int outcurbuf=d->outcurbuf % WINSND_OUT_NBUFS;
		WAVEHDR *hdr=&d->hdrs_write[outcurbuf];
		old=(mblk_t*)hdr->dwUser;
		if (d->nsamples==0){
			int tmpsize=WINSND_OUT_DELAY*d->wfx.nAvgBytesPerSec;
			mblk_t *tmp=allocb(tmpsize,0);
			memset(tmp->b_wptr,0,tmpsize);
			tmp->b_wptr+=tmpsize;
			playout_buf(d,hdr,tmp);
			d->outcurbuf++;
			d->nsamples+=WINSND_OUT_DELAY*d->wfx.nSamplesPerSec;
			continue;
		}
		m=ms_queue_get(f->inputs[0]);
		if (!m) break;
		d->nsamples+=msgdsize(m)/d->wfx.nBlockAlign;
		/*if the output buffer has finished to play, unprepare it*/
		if (hdr->dwFlags & WHDR_DONE){
			mr=waveOutUnprepareHeader(d->outdev,hdr,sizeof(*hdr));
			if (mr != MMSYSERR_NOERROR){
				ms_error("waveOutUnprepareHeader error");
			}
			freemsg(old);
			old=NULL;
			hdr->dwFlags=0;
			hdr->dwUser=0;
		}
		if (old==NULL){
			/* a free wavheader */
			playout_buf(d,hdr,m);
		}else{
			/* no more free wavheader, overrun !*/
			ms_warning("WINSND overrun, restarting");
			d->overrun=TRUE;
			d->nsamples=0;
			waveOutReset(d->outdev);
			break;
		}
		d->outcurbuf++;
	}
}

static int set_rate(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;
	d->wfx.nSamplesPerSec=*((int*)arg);
	return 0;
}

static int get_rate(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;
	*((int*)arg)=d->wfx.nSamplesPerSec;
	return 0;
}

static int set_nchannels(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;
	d->wfx.nChannels=*((int*)arg);
	return 0;
}

static int winsnd_get_stat_input(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;
	return d->stat_input;
}

static int winsnd_get_stat_ouptut(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;

	return d->stat_output;
}

static int winsnd_get_stat_discarded(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;

	return d->stat_notplayed;
}

static MSFilterMethod winsnd_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	MS_FILTER_GET_STAT_INPUT, winsnd_get_stat_input },
	{	MS_FILTER_GET_STAT_OUTPUT, winsnd_get_stat_ouptut },
	{	MS_FILTER_GET_STAT_DISCARDED, winsnd_get_stat_discarded },
	{	0				, NULL		}
};

MSFilterDesc winsnd_read_desc={
	MS_WINSND_READ_ID,
	"MSWinSndRead",
	"Sound capture filter for Windows Sound drivers",
	MS_FILTER_OTHER,
	NULL,
    0,
	1,
	winsnd_init,
    winsnd_read_preprocess,
	winsnd_read_process,
	winsnd_read_postprocess,
    winsnd_uninit,
	winsnd_methods
};


MSFilterDesc winsnd_write_desc={
	MS_WINSND_WRITE_ID,
	"MSWinSndWrite",
	"Sound playback filter for Windows Sound drivers",
	MS_FILTER_OTHER,
	NULL,
    1,
	0,
	winsnd_init,
    winsnd_write_preprocess,
	winsnd_write_process,
	winsnd_write_postprocess,
	winsnd_uninit,
    winsnd_methods
};

MSFilter *ms_winsnd_read_new(MSSndCard *card){
	MSFilter *f=ms_filter_new_from_desc(&winsnd_read_desc);
	WinSndCard *wc=(WinSndCard*)card->data;
	WinSnd *d=(WinSnd*)f->data;
	d->dev_id=wc->in_devid;
	return f;
}


MSFilter *ms_winsnd_write_new(MSSndCard *card){
	MSFilter *f=ms_filter_new_from_desc(&winsnd_write_desc);
	WinSndCard *wc=(WinSndCard*)card->data;
	WinSnd *d=(WinSnd*)f->data;
	d->dev_id=wc->out_devid;
	return f;
}

MS_FILTER_DESC_EXPORT(winsnd_read_desc)
MS_FILTER_DESC_EXPORT(winsnd_write_desc)
