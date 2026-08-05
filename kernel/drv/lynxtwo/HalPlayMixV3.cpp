/****************************************************************************
 HalPlayMixV3.cpp

 Description:	Lynx Application Programming Interface Header File

 Created: David A. Hoatson, March 2009
	
 Copyright © 2009 Lynx Studio Technology, Inc.

 This software contains the valuable TRADE SECRETS and CONFIDENTIAL INFORMATION 
 of Lynx Studio Technology, Inc. The software is protected under copyright 
 laws as an unpublished work of Lynx Studio Technology, Inc.  Notice is 
 for informational purposes only and does not imply publication.  The user 
 of this software may make copies of the software for use with products 
 manufactured by Lynx Studio Technology, Inc. or under license from 
 Lynx Studio Technology, Inc. and for no other use.

 THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
 KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
 PURPOSE.

 Environment: 

 4 spaces per tab

 Revision History
 
 When      Who  Description
 --------- ---  ------------------------------------------------------------
****************************************************************************/

#include <StdAfx.h>
#include "HalAdapter.h"
#include "HalPlayMixV3.h"

#define NOT_IN_USE	0xFF

#define GAIN_MAX	120	// +12.0dB      Index   0       Multipler 260904
#define GAIN_UNITY	0	//   0.0dB      Index  24       Multipler 65536
#define GAIN_MIN	-960	// -96.0dB      Index 216       Multipler 1 (NOTE: hardware only goes to -90.0dB)
#define GAIN_STEP	5	//   0.5dB
#define UNITY_INDEX	24	// Special code for math
#define MUTE_INDEX	255	// Special code for muting


/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalPlayMixV3::Open (PHALADAPTER pHalAdapter, USHORT usDstLine,
		       PMIXCTLV3 pPlayMixV3, PULONG pPlayMixControl,
		       PULONG pPlayMixStatus)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalPlayMixV3::Open %04lx\n", usDstLine ));

  int i;

  m_pHalAdapter = pHalAdapter;
  m_pHalMixer = m_pHalAdapter->GetMixer ();
  m_usDstLine = usDstLine;	// which output this play mixer is on

  //cmn_err((CE_WARN,"DstLine: %u [%08lx]\n", usDstLine, (ULONG)pPlayMixV3 - (ULONG)pHalAdapter->GetRegisters() ));

  for (i = 0; i < MBV3_NUM_MIX_VOLUMES; i++)
    {
      m_RegMixVolume[i].Init (m_pHalAdapter, &pPlayMixV3->MixVolume[i], REG_WRITEONLY, 0xA5A5A5A5);	// write A5 to each shadow so it will get updated next time around
    }

  m_RegMixControl.Init (m_pHalAdapter, pPlayMixControl);
  m_RegMixStatus.Init (m_pHalAdapter, pPlayMixStatus);

  m_bMasterMute = FALSE;
  m_bDither = FALSE;
  m_ulOverloadCount = 0;

  // initalize the shadows, gets written to hardware when the master is set
  for (i = 0; i < MBV3_NUM_MONITOR_LINES; i++)
    {
      m_asMonitorVolume[i][LEFT] = m_usDstLine % 2 ? GAIN_MIN : GAIN_UNITY;	// left goes to out 1
      m_asMonitorVolume[i][RIGHT] = m_usDstLine % 2 ? GAIN_UNITY : GAIN_MIN;	// right goes to out 2

      m_abMonitorMute[i][LEFT] =
	i < (MBV3_NUM_MONITOR_LINES / 2) ? TRUE : i ==
	((m_usDstLine / 2) + 16) ? FALSE : TRUE;
      m_abMonitorMute[i][RIGHT] =
	i < (MBV3_NUM_MONITOR_LINES / 2) ? TRUE : i ==
	((m_usDstLine / 2) + 16) ? FALSE : TRUE;

    }

  // Set the master volume, also writes out the volumes to all the play mixers
  SetVolume (GAIN_UNITY);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixV3::UpdateVolume (int nRegister)
// private
// Notes:
//      There are four volumes per register, so usRegister should be 0..15
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulValue;
  SHORT sVolume;
  USHORT usLine, usChannel;
  BYTE aucIndex[4];

  for (int i = 0; i < 4; i++)
    {
      usLine = (nRegister * 2) + (i / 2);
      usChannel = i % 2;	// left or right

      // if either mute is set, then bBypass will be FALSE and the volume should be set to 255
      if (!m_abMonitorMute[usLine][usChannel] && !m_bMasterMute)
	{
	  sVolume = m_asMonitorVolume[usLine][usChannel] + m_sMasterVolume;
	  if (sVolume > GAIN_MAX)
	    sVolume = GAIN_MAX;
	  if (sVolume < GAIN_MIN)
	    sVolume = GAIN_MIN;

	  //cmn_err((CE_WARN,"Volume %d\n", sVolume ));
	  aucIndex[i] = (((sVolume / GAIN_STEP) - UNITY_INDEX) * -1);
	}
      else
	{
	  // one of the two mutes are on, this channel will get muted
	  aucIndex[i] = MUTE_INDEX;
	}
    }

  ulValue =
    MAKEULONG (MAKEWORD (aucIndex[0], aucIndex[1]),
	       MAKEWORD (aucIndex[2], aucIndex[3]));

  if (m_RegMixVolume[nRegister] != ulValue)
    {
      //cmn_err((CE_WARN,"[%u] m_RegMixVolume[ %d ].Write( %08lx )\n", m_usDstLine, nRegister, ulValue ));
      m_RegMixVolume[nRegister].Write (ulValue);
    }
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixV3::SetVolume (SHORT sVolume)
// Set the volume of the master line
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalPlayMixV3::SetVolume [%u] [%d]\n", m_usDstLine, sVolume ));

  int nRegister;

  if (sVolume > GAIN_UNITY)	// limit the volume on the master to unity
    sVolume = GAIN_UNITY;
  if (sVolume < GAIN_MIN)
    sVolume = GAIN_MIN;

  m_sMasterVolume = sVolume;	// save this value for future use

  for (nRegister = 0; nRegister < MBV3_NUM_MIX_VOLUMES; nRegister++)
    {
      UpdateVolume (nRegister);
    }
}

/////////////////////////////////////////////////////////////////////////////
SHORT
CHalPlayMixV3::GetVolume (USHORT usLine, USHORT usChannel)
// Get the volume from the shadow register
/////////////////////////////////////////////////////////////////////////////
{
  return (m_asMonitorVolume[ConvertLine (usLine)][usChannel]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixV3::SetVolume (USHORT usLine, USHORT usChannel, SHORT sVolume)
/////////////////////////////////////////////////////////////////////////////
{
  usLine = ConvertLine (usLine);	// a number from 0..31

  if (sVolume > GAIN_MAX)
    sVolume = GAIN_MAX;
  if (sVolume < GAIN_MIN)
    sVolume = GAIN_MIN;

  m_asMonitorVolume[usLine][usChannel] = sVolume;	// save this value for future use

  UpdateVolume (usLine / 2);	// a number from 0..15
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixV3::SetMute (BOOLEAN bMute)
// On the Master
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalPlayMixV3::SetMute [%u] [%d]\n", m_usDstLine, bMute ));

  m_bMasterMute = bMute;	// save this value for future use

  for (int nRegister = 0; nRegister < MBV3_NUM_MIX_VOLUMES; nRegister++)
    {
      UpdateVolume (nRegister);
    }
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalPlayMixV3::GetMute (USHORT usLine, USHORT usChannel)
/////////////////////////////////////////////////////////////////////////////
{
  return (m_abMonitorMute[ConvertLine (usLine)][usChannel]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixV3::SetMute (USHORT usLine, USHORT usChannel, BOOLEAN bMute)
/////////////////////////////////////////////////////////////////////////////
{
  usLine = ConvertLine (usLine);	// a number from 0..31

  m_abMonitorMute[usLine][usChannel] = bMute;	// save this value for future use

  UpdateVolume (usLine / 2);	// a number from 0..15
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMixV3::GetLevel ()
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulValue = m_RegMixStatus.Read ();
  ULONG ulLevel = ulValue & REG_PMIXSTAT_LEVEL_MASK;

  // this seems dumb!
  if (ulLevel > 0x7FFFF)
    ulLevel = 0x7FFFF;

  if (ulValue & kBit20)
    m_ulOverloadCount++;

  return (ulLevel);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMixV3::GetOverload ()
// PLEASE NOTE: This code assumes that GetLevel above has already been called
/////////////////////////////////////////////////////////////////////////////
{
  // max out the overload count at 9
  if (m_ulOverloadCount > 9)
    m_ulOverloadCount = 9;

  return (m_ulOverloadCount);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixV3::ResetOverload ()
/////////////////////////////////////////////////////////////////////////////
{
  m_ulOverloadCount = 0;
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixV3::SetDither (BOOLEAN bDither)
/////////////////////////////////////////////////////////////////////////////
{
  m_bDither = bDither;

  m_RegMixControl.BitSet (kBit0, bDither);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalPlayMixV3::ConvertLine (USHORT usLine)
// private
/////////////////////////////////////////////////////////////////////////////
{
  return (usLine - LINE_SRC_RECORD_0);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalPlayMixV3::SetDefaults (BOOLEAN bDriverLoading)
/////////////////////////////////////////////////////////////////////////////
{
  int i;
  //cmn_err((CE_WARN,"CHalPlayMixV3::SetDefaults\n"));

  m_sMasterVolume = GAIN_UNITY;
  m_bMasterMute = FALSE;
  m_bDither = FALSE;
  m_ulOverloadCount = 0;

  // initalize the shadows, gets written to hardware when the master is set
  for (i = 0; i < MBV3_NUM_MONITOR_LINES; i++)
    {
      m_asMonitorVolume[i][LEFT] = m_usDstLine % 2 ? GAIN_MIN : GAIN_UNITY;	// left goes to out 1
      m_asMonitorVolume[i][RIGHT] = m_usDstLine % 2 ? GAIN_UNITY : GAIN_MIN;	// right goes to out 2
      //m_abMonitorMute[ i ][ LEFT ]  = i<(MBV3_NUM_MONITOR_LINES/2) ? TRUE : FALSE;  // records are muted
      //m_abMonitorMute[ i ][ RIGHT ] = i<(MBV3_NUM_MONITOR_LINES/2) ? TRUE : FALSE;  // plays are not

      m_abMonitorMute[i][LEFT] =
	i < (MBV3_NUM_MONITOR_LINES / 2) ? TRUE : i ==
	((m_usDstLine / 2) + 16) ? FALSE : TRUE;
      m_abMonitorMute[i][RIGHT] =
	i < (MBV3_NUM_MONITOR_LINES / 2) ? TRUE : i ==
	((m_usDstLine / 2) + 16) ? FALSE : TRUE;
    }

  // Set the master volume, also writes out the volumes to all the play mixers
  if (!bDriverLoading)
    {
      SetVolume (m_sMasterVolume);
      SetDither (m_bDither);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalPlayMixV3::SetMixerControl (USHORT usSrcLine, USHORT usControl,
				  USHORT usChannel, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  // handle the play mix controls first
  switch (usSrcLine)
    {
    case LINE_NO_SOURCE:
      switch (usControl)
	{
	case CONTROL_VOLUME:
	  SetVolume ((SHORT) ulValue);
	  return (HSTATUS_OK);
	case CONTROL_MUTE:
	  SetMute ((BOOLEAN) ulValue);
	  return (HSTATUS_OK);
	case CONTROL_DITHER:
	  SetDither ((BOOLEAN) ulValue);
	  return (HSTATUS_OK);
	case CONTROL_OVERLOAD:
	  ResetOverload ();
	  return (HSTATUS_OK);
	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;

    case LINE_SRC_RECORD_0:
    case LINE_SRC_RECORD_1:
    case LINE_SRC_RECORD_2:
    case LINE_SRC_RECORD_3:
    case LINE_SRC_RECORD_4:
    case LINE_SRC_RECORD_5:
    case LINE_SRC_RECORD_6:
    case LINE_SRC_RECORD_7:
    case LINE_SRC_RECORD_8:
    case LINE_SRC_RECORD_9:
    case LINE_SRC_RECORD_10:
    case LINE_SRC_RECORD_11:
    case LINE_SRC_RECORD_12:
    case LINE_SRC_RECORD_13:
    case LINE_SRC_RECORD_14:
    case LINE_SRC_RECORD_15:
    case LINE_SRC_PLAY_0:
    case LINE_SRC_PLAY_1:
    case LINE_SRC_PLAY_2:
    case LINE_SRC_PLAY_3:
    case LINE_SRC_PLAY_4:
    case LINE_SRC_PLAY_5:
    case LINE_SRC_PLAY_6:
    case LINE_SRC_PLAY_7:
    case LINE_SRC_PLAY_8:
    case LINE_SRC_PLAY_9:
    case LINE_SRC_PLAY_10:
    case LINE_SRC_PLAY_11:
    case LINE_SRC_PLAY_12:
    case LINE_SRC_PLAY_13:
    case LINE_SRC_PLAY_14:
    case LINE_SRC_PLAY_15:
      switch (usControl)
	{
	case CONTROL_VOLUME:
	  SetVolume (usSrcLine, usChannel, (SHORT) ulValue);
	  return (HSTATUS_OK);

	case CONTROL_MUTE:
	  SetMute (usSrcLine, usChannel, (BOOLEAN) ulValue);
	  return (HSTATUS_OK);

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;
    }

  return (HSTATUS_SERVICE_NOT_REQUIRED);	// special case to let the mixer know not to just return
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalPlayMixV3::GetMixerControl (USHORT usSrcLine, USHORT usControl,
				  USHORT usChannel, PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  // handle the play mix controls first
  switch (usSrcLine)
    {
    case LINE_NO_SOURCE:
      switch (usControl)
	{
	case CONTROL_NUMCHANNELS:
	  *pulValue = 1;
	  return (HSTATUS_OK);	// we return here instead of allowing more code to run

	case CONTROL_VOLUME:	// master volume
	  *pulValue = GetVolume ();
	  return (HSTATUS_OK);

	case CONTROL_MUTE:	// master mute
	  *pulValue = GetMute ();
	  return (HSTATUS_OK);

	case CONTROL_PEAKMETER:	// main output meter
	  // level returned is 0..7FFFF (19 bits), shift to 15 bits
	  *pulValue = (GetLevel () >> 4);
	  return (HSTATUS_OK);

	case CONTROL_OVERLOAD:
	  *pulValue = GetOverload ();
	  return (HSTATUS_OK);

	case CONTROL_DITHER:
	  *pulValue = GetDither ();
	  return (HSTATUS_OK);

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;

    case LINE_SRC_RECORD_0:
    case LINE_SRC_RECORD_1:
    case LINE_SRC_RECORD_2:
    case LINE_SRC_RECORD_3:
    case LINE_SRC_RECORD_4:
    case LINE_SRC_RECORD_5:
    case LINE_SRC_RECORD_6:
    case LINE_SRC_RECORD_7:
    case LINE_SRC_RECORD_8:
    case LINE_SRC_RECORD_9:
    case LINE_SRC_RECORD_10:
    case LINE_SRC_RECORD_11:
    case LINE_SRC_RECORD_12:
    case LINE_SRC_RECORD_13:
    case LINE_SRC_RECORD_14:
    case LINE_SRC_RECORD_15:
      if (usSrcLine >=
	  (LINE_SRC_RECORD_0 + m_pHalAdapter->GetNumWaveInDevices ()))
	return (HSTATUS_INVALID_MIXER_LINE);

      switch (usControl)
	{
	case CONTROL_NUMCHANNELS:	// the number of channels this line has
	  *pulValue = 2;
	  return (HSTATUS_OK);	// we return here instead of allowing more code to run

	case CONTROL_VOLUME:
	  *pulValue = GetVolume (usSrcLine, usChannel);
	  return (HSTATUS_OK);

	case CONTROL_MUTE:
	  *pulValue = GetMute (usSrcLine, usChannel);
	  return (HSTATUS_OK);

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;

    case LINE_SRC_PLAY_0:
    case LINE_SRC_PLAY_1:
    case LINE_SRC_PLAY_2:
    case LINE_SRC_PLAY_3:
    case LINE_SRC_PLAY_4:
    case LINE_SRC_PLAY_5:
    case LINE_SRC_PLAY_6:
    case LINE_SRC_PLAY_7:
    case LINE_SRC_PLAY_8:
    case LINE_SRC_PLAY_9:
    case LINE_SRC_PLAY_10:
    case LINE_SRC_PLAY_11:
    case LINE_SRC_PLAY_12:
    case LINE_SRC_PLAY_13:
    case LINE_SRC_PLAY_14:
    case LINE_SRC_PLAY_15:
      if (usSrcLine >=
	  (LINE_SRC_PLAY_0 + m_pHalAdapter->GetNumWaveOutDevices ()))
	return (HSTATUS_INVALID_MIXER_LINE);

      switch (usControl)
	{
	case CONTROL_NUMCHANNELS:	// the number of channels this line has
	  *pulValue = 2;
	  return (HSTATUS_OK);	// we return here instead of allowing more code to run

	case CONTROL_VOLUME:
	  *pulValue = GetVolume (usSrcLine, usChannel);
	  return (HSTATUS_OK);

	case CONTROL_MUTE:
	  *pulValue = GetMute (usSrcLine, usChannel);
	  return (HSTATUS_OK);

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;
    }
  return (HSTATUS_SERVICE_NOT_REQUIRED);	// special case to let the mixer know not to return
}
