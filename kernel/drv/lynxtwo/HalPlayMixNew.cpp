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

#define MIN_ATTENUATION		0
#define MAX_ATTENUATION		254
#define MUTE_ATTENUATION	255

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalPlayMixNew::Open (PHALADAPTER pHalAdapter, USHORT usDstLine,
			PPLAYMIXCTL_LO pPlayMixCtlLO,
			PPLAYMIXCTL_HI pPlayMixCtlHI, PULONG pPlayMixStatus)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN,"CHalPlayMixNew::Open %04lx\n", usDstLine ));

  int i;

  m_pHalAdapter = pHalAdapter;
  m_pHalMixer = m_pHalAdapter->GetMixer ();
  m_usDstLine = usDstLine;	// which output this play mixer is on

  for (i = 0; i < NUM_PMIXNEW_LINES; i++)
    {
      if (i < PMIXNEW_LINE_1617)
	{
	  m_RegMixControl[i].Init (m_pHalAdapter,
				   &pPlayMixCtlLO->PMixControl[i]);
	}
      else
	{
	  m_RegMixControl[i].Init (m_pHalAdapter,
				   &pPlayMixCtlHI->PMixControl[i -
							       PMIXNEW_LINE_1617]);
	}
    }

  for (i = 0; i < NUM_PMIXNEW_LINES * 2; i++)
    {
      m_aucMonitorVolume[i] = MIN_ATTENUATION;
      m_aucMonitorSource[i] = NOT_IN_USE;	// flag as not in use
      m_abMonitorMute[i] = TRUE;
    }

  m_RegMixStatus.Init (m_pHalAdapter, pPlayMixStatus);

  m_bMasterMute = FALSE;
  m_bDither = FALSE;
  m_ulOverloadCount = 0;

  // Set the master volume, also writes out the volumes to all the play mixers
  SetVolume (MIN_ATTENUATION);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixNew::UpdateVolume (SHORT usLine)
// private
// Notes:
//      ConvertLine must be called on the usLine prior to calling this function
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usVolume;
  BOOLEAN bBypass = FALSE;

  // if either mute is set, then bBypass will be FALSE and the volume should be set to 255
  if (!m_abMonitorMute[usLine] && !m_bMasterMute)
    {
      // is this channel eligible for volume bypass?
      if ((m_aucMonitorVolume[usLine] == 0) && (m_ucMasterVolume == 0))
	{
	  bBypass = TRUE;
	}
      else
	{
	  usVolume =
	    ((USHORT) m_aucMonitorVolume[usLine] + (USHORT) m_ucMasterVolume);
	  if (usVolume > MAX_ATTENUATION)
	    usVolume = MAX_ATTENUATION;

	  //cmn_err (CE_WARN,"Volume %d\n", sVolume ));
	}
    }
  else
    {
      // one of the two mutes are on, this channel will get muted
      usVolume = MUTE_ATTENUATION;
    }

  USHORT usNewLine = usLine / 2;
  USHORT usOffset = (usLine % 2) * 16;	// either 0 or 16

  if (bBypass)
    {
      m_RegMixControl[usNewLine].Write ((REG_PMIXNEW_EVEN_VOLBYPASS <<
					 usOffset),
					(REG_PMIXNEW_EVEN_VOLBYPASS <<
					 usOffset));
    }
  else
    {
      // turns off the volume bypass as well.  
      m_RegMixControl[usNewLine].Write ((usVolume << usOffset),
					((REG_PMIXNEW_EVEN_VOLBYPASS |
					  REG_PMIXNEW_EVEN_VOLUME_MASK) <<
					 usOffset));
    }
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixNew::SetVolume (ULONG ulVolume)
// Set the volume of the master line
/////////////////////////////////////////////////////////////////////////////
{
  if (ulVolume > MAX_ATTENUATION)
    ulVolume = MAX_ATTENUATION;

  m_ucMasterVolume = (BYTE) ulVolume;	// save this value for future use

  for (int i = 0; i < (NUM_PMIXNEW_LINES * 2); i++)
    {
      UpdateVolume ((SHORT) i);
    }
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMixNew::GetVolume (USHORT usLine)
/////////////////////////////////////////////////////////////////////////////
{
  return ((ULONG) m_aucMonitorVolume[ConvertLine (usLine)]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixNew::SetVolume (USHORT usLine, ULONG ulVolume)
/////////////////////////////////////////////////////////////////////////////
{
  usLine = ConvertLine (usLine);

  if (ulVolume > MAX_ATTENUATION)
    ulVolume = MAX_ATTENUATION;

  m_aucMonitorVolume[usLine] = (BYTE) ulVolume;	// save this value for future use

  UpdateVolume (usLine);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixNew::SetMute (BOOLEAN bMute)
// On the Master
/////////////////////////////////////////////////////////////////////////////
{
  m_bMasterMute = bMute;	// save this value for future use

  for (int i = 0; i < (NUM_PMIXNEW_LINES * 2); i++)
    {
      UpdateVolume ((SHORT) i);
    }
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalPlayMixNew::GetMute (USHORT usLine)
/////////////////////////////////////////////////////////////////////////////
{
  return (m_abMonitorMute[ConvertLine (usLine)]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixNew::SetMute (USHORT usLine, BOOLEAN bMute)
/////////////////////////////////////////////////////////////////////////////
{
  usLine = ConvertLine (usLine);

  if (m_aucMonitorSource[usLine] == NOT_IN_USE)
    bMute = TRUE;

  m_abMonitorMute[usLine] = bMute;	// save this value for future use

  UpdateVolume (usLine);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMixNew::GetLevel ()
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulLevel = m_RegMixStatus.Read () & REG_PMIXSTAT_LEVEL_MASK;
  if (ulLevel > 0x7FFFF)
    ulLevel = 0x7FFFF;
  return (ulLevel);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixNew::ResetLevel ()
/////////////////////////////////////////////////////////////////////////////
{
  m_RegMixStatus.BitSet (REG_PMIXSTAT_LEVEL_RESET, TRUE);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMixNew::GetOverload ()
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
CHalPlayMixNew::ResetOverload ()
/////////////////////////////////////////////////////////////////////////////
{
  m_ulOverloadCount = 0;
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixNew::SetDither (BOOLEAN bDither)
/////////////////////////////////////////////////////////////////////////////
{
  m_bDither = bDither;

  m_RegMixControl[0].BitSet (REG_PMIXNEW_DITHER, bDither);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalPlayMixNew::GetSource (USHORT usLine)
/////////////////////////////////////////////////////////////////////////////
{
  return ((ULONG) m_aucMonitorSource[ConvertLine (usLine)]);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalPlayMixNew::SetSource (USHORT usSrcLine, ULONG ulSource)
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usLine = ConvertLine (usSrcLine);
  //cmn_err (CE_WARN,"CHalPlayMixNew::SetSource %04x %04x\n", usLine, ulSource ));

  // are we disconnecting all lines?
  if (ulSource >= 32)
    {
      m_aucMonitorSource[usLine] = NOT_IN_USE;	// flag as not in use
      m_aucMonitorVolume[usLine] = MIN_ATTENUATION;
      m_abMonitorMute[usLine] = TRUE;

      UpdateVolume (usLine);

      // Need to inform the driver that the volume, mute just changed
      m_pHalMixer->ControlChanged (m_usDstLine, LINE_PLAYMIX_1 + usLine,
				   CONTROL_VOLUME);
      m_pHalMixer->ControlChanged (m_usDstLine, LINE_PLAYMIX_1 + usLine,
				   CONTROL_MUTE);
    }
  else
    {
      USHORT usNewLine = usLine / 2;
      USHORT usOffset = (usLine % 2) * 16;	// either 0 or 16

      ulSource = ulSource & 0x1F;	// valid range is 0..31

      ULONG ulValue =
	(ulSource << REG_PMIXNEW_EVEN_SOURCE_OFFSET) << usOffset;
      ULONG ulMask = (REG_PMIXNEW_EVEN_SOURCE_MASK << usOffset);

#if defined(IA64) || defined(AMD64)
#else
      //cmn_err (CE_WARN,"Write %04lx %04x %04x %04x %2u %08lx %08lx\n", ((ULONG)m_RegMixControl[ usNewLine ].GetAddress() - (ULONG)m_pHalAdapter->GetRegisters()), usLine, ulSource, usNewLine, usOffset, ulValue, ulMask ));
#endif

      m_RegMixControl[usNewLine].Write (ulValue, ulMask);

      m_aucMonitorSource[usLine] = (BYTE) ulSource;
    }
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalPlayMixNew::ConvertLine (USHORT usLine)
// private
/////////////////////////////////////////////////////////////////////////////
{
  return (usLine - LINE_PLAYMIX_1);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalPlayMixNew::SetDefaults (BOOLEAN bDriverLoading)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN,"CHalPlayMixNew::SetDefaults\n"));

  if (m_pHalAdapter->IsAES16e ())
    SetSource (LINE_PLAYMIX_1, MIXVAL_PMIXSRC_PLAY0L + m_usDstLine);	// BUGBUG - Need to change this later
  else
    SetSource (LINE_PLAYMIX_1, MIXVAL_PMIXSRC_PLAY0L + m_usDstLine);	// 16..31 is PLAY0L..PLAY7R

  SetMute (LINE_PLAYMIX_1, FALSE);
  SetVolume (LINE_PLAYMIX_1, MIN_ATTENUATION);

  SetSource (LINE_PLAYMIX_2, MIXVAL_PMIXSRC_RECORD0L + m_usDstLine);	// 0..15 is RECORD0L..RECORD7R
  SetMute (LINE_PLAYMIX_2, TRUE);
  SetVolume (LINE_PLAYMIX_2, MIN_ATTENUATION);

  for (USHORT usLine = LINE_PLAYMIX_3; usLine <= LINE_PLAYMIX_22; usLine++)
    {
      SetSource (usLine, NOT_IN_USE);	// No Source - volume, mute & phase are reset
    }

  if (!bDriverLoading)
    {
      // Master
      SetVolume (MIN_ATTENUATION);
      SetMute (FALSE);
      SetDither (FALSE);
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalPlayMixNew::SetMixerControl (USHORT usSrcLine, USHORT usControl,
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
    case LINE_PLAYMIX_5:
    case LINE_PLAYMIX_6:
    case LINE_PLAYMIX_7:
    case LINE_PLAYMIX_8:
    case LINE_PLAYMIX_9:
    case LINE_PLAYMIX_10:
    case LINE_PLAYMIX_11:
    case LINE_PLAYMIX_12:
    case LINE_PLAYMIX_13:
    case LINE_PLAYMIX_14:
    case LINE_PLAYMIX_15:
    case LINE_PLAYMIX_16:
    case LINE_PLAYMIX_17:
    case LINE_PLAYMIX_18:
    case LINE_PLAYMIX_19:
    case LINE_PLAYMIX_20:
    case LINE_PLAYMIX_21:
    case LINE_PLAYMIX_22:
      switch (usControl)
	{
	case CONTROL_VOLUME:
	  SetVolume (usSrcLine, ulValue);
	  return (HSTATUS_OK);
	case CONTROL_MUTE:
	  SetMute (usSrcLine, (BOOLEAN) ulValue);
	  return (HSTATUS_OK);
	case CONTROL_SOURCE:
	  SetSource (usSrcLine, ulValue);
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
  CHalPlayMixNew::GetMixerControl (USHORT usSrcLine, USHORT usControl,
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
    case LINE_PLAYMIX_5:
    case LINE_PLAYMIX_6:
    case LINE_PLAYMIX_7:
    case LINE_PLAYMIX_8:
    case LINE_PLAYMIX_9:
    case LINE_PLAYMIX_10:
    case LINE_PLAYMIX_11:
    case LINE_PLAYMIX_12:
    case LINE_PLAYMIX_13:
    case LINE_PLAYMIX_14:
    case LINE_PLAYMIX_15:
    case LINE_PLAYMIX_16:
    case LINE_PLAYMIX_17:
    case LINE_PLAYMIX_18:
    case LINE_PLAYMIX_19:
    case LINE_PLAYMIX_20:
    case LINE_PLAYMIX_21:
    case LINE_PLAYMIX_22:
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
