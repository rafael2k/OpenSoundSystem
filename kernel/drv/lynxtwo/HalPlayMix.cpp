/****************************************************************************
 HalPlayMix.cpp

 Description:	Lynx Application Programming Interface Header File

 Created: David A. Hoatson, September 2000
	
 Copyright © 2000 Lynx Studio Technology, Inc.

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

#define NOT_IN_USE	0xFF

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalPlayMix::Open (PHALADAPTER pHalAdapter, USHORT usDstLine,
		     PPLAYMIXCTL pPlayMixCtl, PULONG pPlayMixStatus)
/////////////////////////////////////////////////////////////////////////////
{
  int i;

  m_pHalAdapter = pHalAdapter;
  m_pHalMixer = m_pHalAdapter->GetMixer ();
  m_usDstLine = usDstLine;	// which output this play mixer is on

  for (i = 0; i < NUM_PMIX_LINES; i++)
    {
      m_RegMixControl[i].Init (m_pHalAdapter, &pPlayMixCtl->PMixControl[i]);

      m_abConnected[i] = FALSE;
      m_asSource[i] = NOT_IN_USE;	// flag as not in use
      m_aulVolume[i] = MAX_VOLUME;
      m_abMute[i] = TRUE;
      m_abPhase[i] = FALSE;
    }

  m_RegMixStatus.Init (m_pHalAdapter, pPlayMixStatus);

  m_bMasterMute = FALSE;
  m_bMasterPhase = FALSE;
  m_bMasterDither = FALSE;
  m_ulOverloadCount = 0;

  // Set the master volume, also writes out the volumes to all the play mixers
  SetVolume (MAX_VOLUME);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::UpdateVolume (SHORT usLine)
// private
// Notes:
//      ConvertLine must be called on the usLine prior to calling this function
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulVolume;
  SHORT sVolume = 0;
  BOOLEAN bBypass = FALSE;

  // if either mute is set, then bBypass will be FALSE and the volume will be zero
  if (!m_abMute[usLine] && !m_bMasterMute)
    {
      // is this channel eligible for volume bypass?
      if ((m_aulVolume[usLine] >= MAX_VOLUME) &&
	  (m_ulMasterVolume >= MAX_VOLUME) && !m_abPhase[usLine])
	{
	  bBypass = TRUE;
	}
      else
	{
	  ulVolume = (m_aulVolume[usLine] * m_ulMasterVolume) / 65535;	// not >>16 as this would be a / 65536 or 0x10000
	  sVolume = (SHORT) (ulVolume >> 1);

	  if (m_abPhase[usLine])
	    sVolume *= -1;

	  //cmn_err (CE_WARN,"Volume %d\n", sVolume );
	}
    }

  if (bBypass)
    {
      m_RegMixControl[usLine].Write (REG_PMIX_VOLBYPASS, REG_PMIX_VOLBYPASS);
    }
  else
    {
      // turns off the volume bypass as well.  
      // Must typecast to a USHORT to avoid sign extend when phase is turned on
      m_RegMixControl[usLine].Write ((USHORT) sVolume,
				     (REG_PMIX_VOLBYPASS |
				      REG_PMIX_VOLUME_MASK));
    }
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::SetVolume (ULONG ulVolume)
// Set the volume of the master line
/////////////////////////////////////////////////////////////////////////////
{
  if (ulVolume >= MAX_VOLUME)
    ulVolume = MAX_VOLUME;

  m_ulMasterVolume = ulVolume;	// save this value for future use

  UpdateVolume (PMIX_LINE_A);
  UpdateVolume (PMIX_LINE_B);
  UpdateVolume (PMIX_LINE_C);
  UpdateVolume (PMIX_LINE_D);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMix::GetVolume (USHORT usLine)
/////////////////////////////////////////////////////////////////////////////
{
  return (m_aulVolume[ConvertLine (usLine)]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::SetVolume (USHORT usLine, ULONG ulVolume)
/////////////////////////////////////////////////////////////////////////////
{
  usLine = ConvertLine (usLine);

  if (ulVolume >= MAX_VOLUME)
    ulVolume = MAX_VOLUME;

  m_aulVolume[usLine] = ulVolume;	// save this value for future use

  UpdateVolume (usLine);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalPlayMix::GetPhase (USHORT usLine)
/////////////////////////////////////////////////////////////////////////////
{
  return (m_abPhase[ConvertLine (usLine)]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::SetPhase (USHORT usLine, BOOLEAN bPhase)
/////////////////////////////////////////////////////////////////////////////
{
  usLine = ConvertLine (usLine);

  m_abPhase[usLine] = bPhase;	// save this value for future use

  UpdateVolume (usLine);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::SetMute (BOOLEAN bMute)
// On the Master
/////////////////////////////////////////////////////////////////////////////
{
  m_bMasterMute = bMute;	// save this value for future use

  UpdateVolume (PMIX_LINE_A);
  UpdateVolume (PMIX_LINE_B);
  UpdateVolume (PMIX_LINE_C);
  UpdateVolume (PMIX_LINE_D);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalPlayMix::GetMute (USHORT usLine)
/////////////////////////////////////////////////////////////////////////////
{
  return (m_abMute[ConvertLine (usLine)]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::SetMute (USHORT usLine, BOOLEAN bMute)
/////////////////////////////////////////////////////////////////////////////
{
  usLine = ConvertLine (usLine);

  if (m_asSource[usLine] == NOT_IN_USE)
    bMute = TRUE;

  m_abMute[usLine] = bMute;	// save this value for future use

  UpdateVolume (usLine);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMix::GetLevel ()
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulLevel = m_RegMixStatus.Read () & REG_PMIXSTAT_LEVEL_MASK;
  if (ulLevel > 0x7FFFF)
    ulLevel = 0x7FFFF;
  return (ulLevel);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::ResetLevel ()
/////////////////////////////////////////////////////////////////////////////
{
  m_RegMixStatus.BitSet (REG_PMIXSTAT_LEVEL_RESET, TRUE);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMix::GetOverload ()
/////////////////////////////////////////////////////////////////////////////
{
  if (m_RegMixStatus.Read () & REG_PMIXSTAT_OVERLOAD)
    {
      m_RegMixStatus.BitSet (REG_PMIXSTAT_OVERLOAD_RESET, TRUE);
      m_ulOverloadCount++;
    }

  // max out the overload count at 9
  if (m_ulOverloadCount > 9)
    m_ulOverloadCount = 9;

  return (m_ulOverloadCount);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::ResetOverload ()
/////////////////////////////////////////////////////////////////////////////
{
  m_ulOverloadCount = 0;
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::SetPhase (BOOLEAN bPhase)
/////////////////////////////////////////////////////////////////////////////
{
  m_bMasterPhase = bPhase;

  // Master phase control overrides all the individual controls
  m_abPhase[PMIX_LINE_A] = bPhase;
  m_abPhase[PMIX_LINE_B] = bPhase;
  m_abPhase[PMIX_LINE_C] = bPhase;
  m_abPhase[PMIX_LINE_D] = bPhase;

  UpdateVolume (PMIX_LINE_A);
  UpdateVolume (PMIX_LINE_B);
  UpdateVolume (PMIX_LINE_C);
  UpdateVolume (PMIX_LINE_D);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::SetDither (BOOLEAN bDither)
/////////////////////////////////////////////////////////////////////////////
{
  m_bMasterDither = bDither;

  m_RegMixControl[PMIX_LINE_A].BitSet (REG_PMIX_DITHER, bDither);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalPlayMix::GetSource (USHORT usLine)
/////////////////////////////////////////////////////////////////////////////
{
  return (m_asSource[ConvertLine (usLine)]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMix::SetSource (USHORT usLine, USHORT usSource)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulSource = usSource & 0x1F;	// valid range is 0..31
  usLine = ConvertLine (usLine);

  // are we disconnecting all lines?
  if (usSource >= 32)
    {
      m_abConnected[usLine] = FALSE;
      m_asSource[usLine] = NOT_IN_USE;	// flag as not in use
      m_aulVolume[usLine] = MAX_VOLUME;
      m_abMute[usLine] = TRUE;
      m_abPhase[usLine] = FALSE;
      UpdateVolume (usLine);

      // Need to inform the driver that the volume, mute & phase just changed
      m_pHalMixer->ControlChanged (m_usDstLine, LINE_PLAYMIX_1 + usLine,
				   CONTROL_VOLUME);
      m_pHalMixer->ControlChanged (m_usDstLine, LINE_PLAYMIX_1 + usLine,
				   CONTROL_MUTE);
      m_pHalMixer->ControlChanged (m_usDstLine, LINE_PLAYMIX_1 + usLine,
				   CONTROL_PHASE);
    }
  else
    {
      m_RegMixControl[usLine].Write ((ulSource << REG_PMIX_PLAYSOURCE_OFFSET),
				     REG_PMIX_PLAYSOURCE_MASK);
      m_asSource[usLine] = usSource;
      m_abConnected[usLine] = TRUE;
    }
}

/*
/////////////////////////////////////////////////////////////////////////////
USHORT	CHalPlayMix::GetFirstAvailableConnection( PUSHORT pusLine )
// Only called by CHalMonitorMix
/////////////////////////////////////////////////////////////////////////////
{
	*pusLine = NOT_CONNECTED;

	for( int i=0; i<NUM_PMIX_LINES; i++ )
	{
		if( !m_abConnected[ i ] )
		{
			*pusLine = (USHORT)i;
			return( HSTATUS_OK );
		}
	}

	return( HSTATUS_INSUFFICIENT_RESOURCES );
}

/////////////////////////////////////////////////////////////////////////////
USHORT	CHalPlayMix::SetConnection( USHORT usLine, BOOLEAN bConnect )
// Only called by CHalMonitorMix
/////////////////////////////////////////////////////////////////////////////
{
	if( bConnect )
	{
		if( m_abConnected[ usLine ] )
			return( HSTATUS_ALREADY_IN_USE );

		m_abConnected[ usLine ] = TRUE;
	}
	else
	{
		m_abConnected[ usLine ]	= FALSE;
		m_asSource[ usLine ]	= NOT_IN_USE;	// flag as not in use
		m_aulVolume[ usLine ]	= MAX_VOLUME;
		m_abMute[ usLine ]		= TRUE;
		m_abPhase[ usLine ]		= FALSE;
		UpdateVolume( usLine );
	}
	
	return( HSTATUS_OK );
}
*/
/////////////////////////////////////////////////////////////////////////////
USHORT
CHalPlayMix::ConvertLine (USHORT usLine)
// private
/////////////////////////////////////////////////////////////////////////////
{
  // convert mixer line to playmix line
  switch (usLine)
    {
    default:
    case LINE_PLAYMIX_1:
      return (PMIX_LINE_A);
    case LINE_PLAYMIX_2:
      return (PMIX_LINE_B);
    case LINE_PLAYMIX_3:
      return (PMIX_LINE_C);
    case LINE_PLAYMIX_4:
      return (PMIX_LINE_D);
    }
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalPlayMix::SetDefaults (BOOLEAN bDriverLoading)
/////////////////////////////////////////////////////////////////////////////
{
  SetSource (LINE_PLAYMIX_1, MIXVAL_PMIXSRC_PLAY0L + (m_usDstLine - LINE_OUT_1));	// 16..31 is PLAY0L..PLAY7R
  m_abMute[PMIX_LINE_A] = FALSE;
  m_abPhase[PMIX_LINE_A] = FALSE;
  SetVolume (LINE_PLAYMIX_1, MAX_VOLUME);

  SetSource (LINE_PLAYMIX_2, MIXVAL_PMIXSRC_RECORD0L + (m_usDstLine - LINE_OUT_1));	// 0..15 is RECORD0L..RECORD7R
  m_abMute[PMIX_LINE_B] = TRUE;
  m_abPhase[PMIX_LINE_B] = FALSE;
  SetVolume (LINE_PLAYMIX_2, MAX_VOLUME);

  if (!bDriverLoading)
    {
      SetSource (LINE_PLAYMIX_3, 0xFFFF);	// No Source - volume, mute & phase are reset
      SetSource (LINE_PLAYMIX_4, 0xFFFF);	// No Source - volume, mute & phase are reset

      // Master
      SetVolume (MAX_VOLUME);
      SetMute (FALSE);
      SetPhase (FALSE);		// Master phase control overrides all the individual controls
      SetDither (FALSE);
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalPlayMix::SetMixerControl (USHORT usSrcLine, USHORT usControl,
				ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  // handle the play mix controls first
  switch (usSrcLine)
    {
    case LINE_NO_SOURCE:
      switch (usControl)
	{
	case CONTROL_VOLUME:
	  SetVolume (ulValue);
	  return (HSTATUS_OK);
	case CONTROL_MUTE:
	  SetMute ((BOOLEAN) ulValue);
	  return (HSTATUS_OK);
	case CONTROL_PHASE:
	  SetPhase ((BOOLEAN) ulValue);
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

    case LINE_PLAYMIX_1:
    case LINE_PLAYMIX_2:
    case LINE_PLAYMIX_3:
    case LINE_PLAYMIX_4:
      switch (usControl)
	{
	case CONTROL_VOLUME:
	  SetVolume (usSrcLine, ulValue);
	  return (HSTATUS_OK);
	case CONTROL_MUTE:
	  SetMute (usSrcLine, (BOOLEAN) ulValue);
	  return (HSTATUS_OK);
	case CONTROL_PHASE:
	  SetPhase (usSrcLine, (BOOLEAN) ulValue);
	  return (HSTATUS_OK);
	case CONTROL_SOURCE:
	  SetSource (usSrcLine, (USHORT) ulValue);
	  return (HSTATUS_OK);
	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;
    }

  return (HSTATUS_SERVICE_NOT_REQUIRED);	// special case to let the mixer know not to return
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalPlayMix::GetMixerControl (USHORT usSrcLine, USHORT usControl,
				PULONG pulValue)
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

	case CONTROL_VOLUME:
	  *pulValue = GetVolume ();
	  return (HSTATUS_OK);

	case CONTROL_MUTE:
	  *pulValue = GetMute ();
	  return (HSTATUS_OK);

	case CONTROL_PEAKMETER:
	  // level returned is 0..7FFFF (19 bits), shift to 15 bits
	  *pulValue = (GetLevel () >> 4);
	  ResetLevel ();	// after we have read the level, reset it
	  return (HSTATUS_OK);

	case CONTROL_OVERLOAD:
	  *pulValue = GetOverload ();
	  return (HSTATUS_OK);

	case CONTROL_PHASE:
	  *pulValue = GetPhase ();
	  return (HSTATUS_OK);

	case CONTROL_DITHER:
	  *pulValue = GetDither ();
	  return (HSTATUS_OK);

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;

    case LINE_PLAYMIX_1:
    case LINE_PLAYMIX_2:
    case LINE_PLAYMIX_3:
    case LINE_PLAYMIX_4:
      switch (usControl)
	{
	case CONTROL_NUMCHANNELS:	// the number of channels this line has
	  *pulValue = 1;
	  return (HSTATUS_OK);	// we return here instead of allowing more code to run

	case CONTROL_VOLUME:
	  *pulValue = GetVolume (usSrcLine);
	  return (HSTATUS_OK);

	case CONTROL_MUTE:
	  *pulValue = GetMute (usSrcLine);
	  return (HSTATUS_OK);

	case CONTROL_PHASE:
	  *pulValue = GetPhase (usSrcLine);
	  return (HSTATUS_OK);

	case CONTROL_SOURCE:
	  *pulValue = GetSource (usSrcLine);
	  return (HSTATUS_OK);

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;
    }
  return (HSTATUS_SERVICE_NOT_REQUIRED);	// special case to let the mixer know not to return
}
