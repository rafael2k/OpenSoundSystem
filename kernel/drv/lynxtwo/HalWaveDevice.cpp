/****************************************************************************
 HalWaveDevice.cpp

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
 Dec 03 03 DAH	Added code to Validate to insure the requested sample rate is 
				the same as the currently selected sample rate if any devices
				are already active.
 Jun 17 03 DAH	Added SetSamplePosition
 Jun 04 03 DAH	Added GetSamplePosition
****************************************************************************/

#include <StdAfx.h>
#include "HalAdapter.h"

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::Open (PHALADAPTER pHalAdapter, ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  // NOTE: Do NOT use m_pHalAdapter in this function!  It isn't assigned yet!
  m_bIsAES16e = pHalAdapter->IsAES16e ();
  m_bIsRecord = pHalAdapter->IsWaveDeviceRecord (ulDeviceNumber);

  PLYNXTWOREGISTERS pRegisters;
  PLYNXTWOAUDIOBUFFERS pAudioBuffers;

  pRegisters = pHalAdapter->GetRegisters ();
  m_pHalMixer = pHalAdapter->GetMixer ();	// mixer device is created with the adapter device, safe to use here
  pAudioBuffers = pHalAdapter->GetAudioBuffers ();

  //cmn_err((CE_WARN,"CHalWaveDevice::Open %lu\n", ulDeviceNumber ));

  // assign the target mode address for this devices buffer
  // play devices are after record devices so this will work for both
  if (pAudioBuffers)
    m_pAudioBuffer = (PULONG) & pAudioBuffers->Record[ulDeviceNumber];
  else
    m_pAudioBuffer = NULL;

  if (m_bIsAES16e)
    {
      /*
         m_pulPosLoReg = &pRegisters->AESePos.Play0Lo;
         m_pulPosHiReg = &pRegisters->AESePos.Play0Hi;
         cmn_err(CE_WARN,"pRegisters %08lx PosLoReg %08lx PosHiReg %08lx Device %ld\n", (ULONG)pRegisters, m_pulPosLoReg, m_pulPosHiReg, ulDeviceNumber );
       */
      ULONG ulDeviceOffset = 0;

      // if we are in 16-channel mode, then we need to offset to get the hardware address correct
      if (!pHalAdapter->In32ChannelMode () && !m_bIsRecord)
	ulDeviceOffset = 8;	// 8=16

      //cmn_err((CE_WARN,"Device %lu IsRecord %d Offset %lu\n", ulDeviceNumber, (short)m_bIsRecord, ulDeviceOffset ));
      m_RegStreamControl.Init (pHalAdapter,
			       &pRegisters->SCBlock.E.
			       RecordControl[ulDeviceNumber +
					     ulDeviceOffset]);
      m_RegStreamStatus.Init (pHalAdapter,
			      &pRegisters->SCBlock.E.
			      RecordStatus[ulDeviceNumber + ulDeviceOffset],
			      REG_READONLY);
    }
  else
    {
      m_RegStreamControl.Init (pHalAdapter,
			       &pRegisters->SCBlock.L2.
			       RecordControl[ulDeviceNumber]);
      m_RegStreamStatus.Init (pHalAdapter,
			      &pRegisters->SCBlock.L2.
			      RecordStatus[ulDeviceNumber], REG_READONLY);
    }

  m_RegStreamControl.Write (0);	// init the stream control register

  m_lHWIndex = 0;
  m_lPCIndex = 0;
  m_ulInterruptSamples = 0;
  m_ulBytesTransferred = 0;
  m_ulSamplesTransferred = 0;
  m_ulOverrunCount = 0;
  m_bSyncStartEnabled = FALSE;

  m_wFormatTag = WAVE_FORMAT_PCM;
  m_lNumChannels = 2;
  m_lSampleRate = DEFAULT_SAMPLE_RATE;
  m_lBitsPerSample = 24;
  m_lBytesPerBlock = (m_lBitsPerSample * m_lNumChannels) / 8;

  m_ulGBPEntryCount = 0;
  m_lGBPLastHWIndex = 0;
  m_ullBytePosition = 0;

  // determine which mixer line this is
  if (m_bIsRecord)
    {
      m_usDstLine = LINE_RECORD_0 + (USHORT) ulDeviceNumber;
      m_usSrcLine = LINE_NO_SOURCE;
    }
  else
    {
      m_usDstLine = LINE_OUT_1;
      m_usSrcLine =
	LINE_PLAY_0 + (USHORT) (ulDeviceNumber -
				pHalAdapter->GetNumWaveInDevices ());
    }

  return (CHalDevice::Open (pHalAdapter, ulDeviceNumber));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::Close ()
/////////////////////////////////////////////////////////////////////////////
{
  return (CHalDevice::Close ());
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::Start ()
// Never used for AES16e
/////////////////////////////////////////////////////////////////////////////
{
  LONG lCurrentRate;
  PHALSAMPLECLOCK pClock;
  ULONG ulStreamControl = m_lPCIndex & REG_L2STRMCTL_PCPTR_MASK;

  DS (" Start ", COLOR_BOLD);
  DX8 ((BYTE) m_ulDeviceNumber, COLOR_BOLD);
  DC (' ');
  //cmn_err((CE_WARN,"CHalWaveDevice::Start %ld\n", m_ulDeviceNumber ));

  // if the sample rate is not the same as the current sample rate on the card, change it...
  pClock = m_pHalAdapter->GetSampleClock ();
  pClock->Get (&lCurrentRate);
  if (lCurrentRate != m_lSampleRate)
    if (pClock->Set (m_lSampleRate))
      return (HSTATUS_INVALID_SAMPLERATE);

  if (!m_ulInterruptSamples)
    SetInterruptSamples (0);	// make sure the interrupt samples gets set for this sample rate

  // set the bits for the sample format
  switch (m_lBitsPerSample)
    {
    case 8:
      SET (ulStreamControl, REG_L2STRMCTL_FMT_PCM8);
      break;
    case 16:
      SET (ulStreamControl, REG_L2STRMCTL_FMT_PCM16);
      break;
    case 24:
      SET (ulStreamControl, REG_L2STRMCTL_FMT_PCM24);
      break;
    case 32:
      SET (ulStreamControl, REG_L2STRMCTL_FMT_PCM32);
      break;
    }

  // if this is a record device, set the dither depth (this only does something when the dither depth is auto
  if (m_bIsRecord)
    {
      m_pHalMixer->SetControl (m_usDstLine, m_usSrcLine, CONTROL_DITHER_DEPTH,
			       LEFT, (ULONG) m_lBitsPerSample);
      m_pHalMixer->SetControl (m_usDstLine, m_usSrcLine, CONTROL_DITHER_DEPTH,
			       RIGHT, (ULONG) m_lBitsPerSample);
    }

  // if this device is in stereo mode, set the appropriate bit
  if (m_lNumChannels == 2)
    SET (ulStreamControl, REG_L2STRMCTL_CHNUM_STEREO);

  if (m_lNumChannels > 2)
    {
      if (m_ulDeviceNumber == WAVE_RECORD0_DEVICE)
	{
	  if (m_pHalAdapter->SetMultiChannelRecord (m_lNumChannels))
	    return (HSTATUS_ALREADY_IN_USE);
	}
      else if (m_ulDeviceNumber == m_pHalAdapter->GetNumWaveInDevices ())	// is this the first play device?
	{
	  if (m_pHalAdapter->SetMultiChannelPlay (m_lNumChannels))
	    return (HSTATUS_ALREADY_IN_USE);
	}
      else
	{
	  return (HSTATUS_INVALID_FORMAT);
	}
    }

  // Enable the IO Processor and the Limit Interrupt
  SET (ulStreamControl, (REG_L2STRMCTL_LIMIE | REG_L2STRMCTL_OVERIE));

  // It is OK to set these to zero here, because the hardware hasn't been put into play mode yet (HWIndex is really zero).
  m_ulGBPEntryCount = 0;
  m_lGBPLastHWIndex = 0;

  //m_lHWIndex                            = 0;    don't set the HWIndex to zero here, do it in the Stop code instead
  m_ulOverrunCount = 0;
  m_pHalMixer->ControlChanged (m_usDstLine, m_usSrcLine,
			       CONTROL_OVERRUN_COUNT);

  // the format has changed, inform the driver
  m_pHalMixer->ControlChanged (m_usDstLine, m_usSrcLine,
			       CONTROL_SAMPLE_FORMAT);

  // Check for WAVE_FORMAT_DOLBY_AC3_SPDIF or WAVE_FORMAT_WMA_SPDIF and route this play device to the Digital Output.
  // Only do this on cards that have an CS8420
  if (m_pHalAdapter->HasCS8420 () && !m_bIsRecord)
    {
      if (m_wFormatTag != WAVE_FORMAT_PCM)
	{
	  PrepareForNonPCM ();
	}
      else
	{
	  PrepareForPCM ();
	}
    }

  if (m_bSyncStartEnabled)
    {
      // only firmware 16 & above has sync start enabled
      if (m_pHalAdapter->HasGlobalSyncStart ())
	{
	  SET (ulStreamControl, REG_L2STRMCTL_MODE_SYNCREADY);
	  //cmn_err((CE_WARN,"MODE_SYNCREADY\n"));
	}
      else
	{
	  SET (ulStreamControl, REG_L2STRMCTL_MODE_RUN);
	  //cmn_err((CE_WARN,"MODE_RUN\n"));
	}

      //cmn_err((CE_WARN,"SynStartReady [%04lx]\n", ulStreamControl & REG_L2STRMCTL_FMT_MASK ));
      m_pHalAdapter->SyncStartReady (m_ulDeviceNumber, ulStreamControl);
      // reset the sync start enabled status for next time.
      m_bSyncStartEnabled = FALSE;
    }
  else
    {
      //cmn_err((CE_WARN,"Starting Device...\n"));
      // Write the control register to the hardware
      SET (ulStreamControl, REG_L2STRMCTL_MODE_RUN);
      m_RegStreamControl = ulStreamControl;
    }

  return (CHalDevice::Start ());
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::Stop ()
/////////////////////////////////////////////////////////////////////////////
{
  DisableSyncStart ();

  //cmn_err((CE_WARN,"CHalWaveDevice::Stop %ld\n", m_ulDeviceNumber ));
  if (m_usMode == MODE_RUNNING)
    {
      DS (" Stop ", COLOR_BOLD);
      DX8 ((BYTE) m_ulDeviceNumber, COLOR_BOLD);
      DC (' ');

      // stop the device first
      if (m_bIsAES16e)
	m_RegStreamControl.BitSet ((REG_ESTRMCTL_MODE_MASK |
				    REG_ESTRMCTL_DMAEN), FALSE);
      else
	m_RegStreamControl.BitSet ((REG_L2STRMCTL_PCPTR_MASK |
				    REG_L2STRMCTL_MODE_MASK |
				    REG_L2STRMCTL_DMAEN | REG_L2STRMCTL_LIMIE
				    | REG_L2STRMCTL_OVERIE), FALSE);

      // reset the member variables to reflect the device is stopped
      m_ulBytesTransferred = 0;	// no longer used
      m_ulSamplesTransferred = 0;
      m_lHWIndex = 0;
      m_lPCIndex = 0;

      m_ulGBPEntryCount = 0;
      m_lGBPLastHWIndex = 0;

      // must reflect stopped status before calling SetInterruptSamples
      CHalDevice::Stop ();

      SetInterruptSamples (0);

      if (m_lNumChannels > 2)
	{
	  if (m_ulDeviceNumber == WAVE_RECORD0_DEVICE)
	    {
	      m_pHalAdapter->ClearMultiChannelRecord ();
	    }
	  else if (m_ulDeviceNumber == m_pHalAdapter->GetNumWaveInDevices ())	// is this the first play device?
	    {
	      m_pHalAdapter->ClearMultiChannelPlay ();
	    }
	}
    }

  // the format has changed, inform the driver
  m_pHalMixer->ControlChanged (m_usDstLine, m_usSrcLine,
			       CONTROL_SAMPLE_FORMAT);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::EnableSyncStart ()
/////////////////////////////////////////////////////////////////////////////
{
  if (m_pHalAdapter->GetSyncStartState ())
    {
      //cmn_err((CE_WARN,"Adding Device to Start Group\n"));
      m_bSyncStartEnabled = TRUE;
      m_pHalAdapter->AddToStartGroup (m_ulDeviceNumber);
    }
  else
    {
      cmn_err (CE_WARN, "Adapter SyncStart Disabled\n");
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::DisableSyncStart ()
// This is used to insure that any devices that Stop, are no longer part of
// the StartGroup
/////////////////////////////////////////////////////////////////////////////
{
  m_bSyncStartEnabled = FALSE;
  m_pHalAdapter->RemoveFromStartGroup (m_ulDeviceNumber);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalWaveDevice::SetFormat (USHORT wFormatTag, LONG lChannels,
			     LONG lSampleRate, LONG lBitsPerSample,
			     LONG lBlockAlign)
// This doesn't actually touch the hardware - all format changes are done
// when the device goes into RUN mode.
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usStatus;

  //cmn_err((CE_WARN,"CHalWaveDevice::SetFormat\n"));

  // must check to see if device is idle first
  if (m_usMode != MODE_STOP)
    {
      cmn_err (CE_WARN, "CHalWaveDevice::SetFormat: Device Not IDLE!\n");
      return (HSTATUS_INVALID_MODE);
    }

  // make sure this is a valid format
  usStatus =
    ValidateFormat (wFormatTag, lChannels, lSampleRate, lBitsPerSample,
		    lBlockAlign);
  if (usStatus)
    return (usStatus);

  // remember the format for our device
  m_wFormatTag = wFormatTag;
  m_lNumChannels = lChannels;
  m_lSampleRate = lSampleRate;
  m_lBitsPerSample = lBitsPerSample;
  m_lBytesPerBlock = lBlockAlign;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalWaveDevice::ValidateFormat (USHORT wFormatTag, LONG lChannels,
				  LONG lSampleRate, LONG lBitsPerSample,
				  LONG lBlockAlign)
/////////////////////////////////////////////////////////////////////////////
{
  PHALSAMPLECLOCK pClock = m_pHalAdapter->GetSampleClock ();
  LONG lMin, lMax;
  //cmn_err((CE_WARN,"CHalWaveDevice::ValidateFormat\n"));

  // Validate wFormatTag field
  if ((wFormatTag != WAVE_FORMAT_PCM)
      && (wFormatTag != WAVE_FORMAT_DOLBY_AC3_SPDIF)
      && (wFormatTag != WAVE_FORMAT_WMA_SPDIF))
    {
      //cmn_err((CE_WARN,"CHalWaveDevice::Validate: Not WAVE_FORMAT_PCM!"));

      cmn_err (CE_WARN, "Format [%04x] ", wFormatTag);
      cmn_err (CE_WARN, "Ch [%ld] ", lChannels);
      cmn_err (CE_WARN, "SR [%ld] ", lSampleRate);
      cmn_err (CE_WARN, "Bits [%ld]\n", lBitsPerSample);

      return (HSTATUS_INVALID_FORMAT);
    }

  //BUGBUG
  // keep 24-bit mono from going through
//      if( (pWaveFormat->wBitsPerSample == 24) && (pWaveFormat->nChannels == 1) )
//      {
//              cmn_err((CE_WARN,"CHalWaveDevice::Validate: Cannot do 24-bit mono!\n"));
//              return( HSTATUS_INVALID_FORMAT );
//      }

  // Validate wBitsPerSample field
  if ((lBitsPerSample != 8)
      && (lBitsPerSample != 16)
      && (lBitsPerSample != 24) && (lBitsPerSample != 32))
    {
      cmn_err (CE_WARN,
	       "CHalWaveDevice::Validate: Format Not 8, 16, 24 or 32 bits!\n");
      return (HSTATUS_INVALID_FORMAT);
    }

  // Validate nChannels field
  if (lChannels < 1)
    {
      cmn_err (CE_WARN, "CHalWaveDevice::Validate: Invalid nChannels!\n");
      return (HSTATUS_INVALID_FORMAT);
    }
  if (m_pHalAdapter->HasMultiChannel ())
    {
      if (lChannels > 16)
	{
	  cmn_err (CE_WARN, "CHalWaveDevice::Validate: Invalid nChannels!\n");
	  return (HSTATUS_INVALID_FORMAT);
	}
    }
  else
    {
      if (lChannels > 2)
	{
	  cmn_err (CE_WARN, "CHalWaveDevice::Validate: Invalid nChannels!\n");
	  return (HSTATUS_INVALID_FORMAT);
	}
    }

  // validate the sample rate is in range
  pClock->GetMinMax (&lMin, &lMax);

  if (lSampleRate < lMin)
    {
      return (HSTATUS_INVALID_SAMPLERATE);
    }
  if (lSampleRate > lMax)
    {
      return (HSTATUS_INVALID_SAMPLERATE);
    }

  // if any other devices on the card are active, limit the sample rate to the currently selected rate
  if (!pClock->IsFrequencyAgile ())
    {
      LONG lCurrentRate;

      pClock->Get (&lCurrentRate);

      if (lCurrentRate != lSampleRate)
	{
	  //cmn_err((CE_WARN,"CHalWaveDevice::Validate: lSampleRate doesn't match rate of running devices!\n"));
	  return (HSTATUS_INVALID_SAMPLERATE);
	}
    }

  // Validate nSamplesPerSec field
  if (lSampleRate < MIN_SAMPLE_RATE)
    {
      //cmn_err((CE_WARN,"CHalWaveDevice::Validate: Invalid lSampleRate! %lu\n", lSampleRate ));
      return (HSTATUS_INVALID_SAMPLERATE);
    }

  if (lSampleRate > MAX_SAMPLE_RATE)
    {
      //cmn_err((CE_WARN,"CHalWaveDevice::Validate: Invalid lSampleRate! %lu\n", lSampleRate ));
      return (HSTATUS_INVALID_SAMPLERATE);
    }

  // Validate lBlockAlign
  if (lBlockAlign != ((lBitsPerSample * lChannels) / 8))
    {
      cmn_err (CE_WARN,
	       "CHalWaveDevice::Validate: Invalid lBlockAlign %ld!\n",
	       lBlockAlign);
      return (HSTATUS_INVALID_FORMAT);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::PrepareForNonPCM (void)
// This code is only useful on a card with a CS8420
/////////////////////////////////////////////////////////////////////////////
{
  PHALMIXER pMixer = m_pHalAdapter->GetMixer ();
  USHORT usDstLine, usSrcLine;
  ULONG ulSource;
  ULONG ulPlayMixLeft = (m_ulDeviceNumber * 2) + LEFT;
  ULONG ulPlayMixRight = (m_ulDeviceNumber * 2) + RIGHT;
  USHORT usPMixSrc = LINE_PLAYMIX_3;

  cmn_err (CE_WARN,
	   "WAVE_FORMAT_DOLBY_AC3_SPDIF or WAVE_FORMAT_WMA_SPDIF on PLAYx: Changing Mixer Layout\n");

  // Mute any of the Analog Outputs that have this device routed
  for (usDstLine = LINE_OUT_1; usDstLine <= LINE_OUT_6; usDstLine++)
    {
      for (usSrcLine = LINE_PLAYMIX_1; usSrcLine <= LINE_PLAYMIX_4;
	   usSrcLine++)
	{
	  pMixer->GetControl (usDstLine, usSrcLine, CONTROL_SOURCE, 0,
			      &ulSource);
	  if ((ulSource == ulPlayMixLeft) || (ulSource == ulPlayMixRight))
	    {
	      pMixer->SetControl (usDstLine, usSrcLine, CONTROL_MUTE, 0,
				  TRUE);
	      pMixer->ControlChanged (usDstLine, usSrcLine, CONTROL_MUTE);
	    }
	}
    }

  // Mute all playmix'ers for the Digital Output
  for (usSrcLine = LINE_PLAYMIX_1; usSrcLine <= LINE_PLAYMIX_4; usSrcLine++)
    {
      pMixer->SetControl (LINE_OUT_7, usSrcLine, CONTROL_MUTE, 0, TRUE);
      pMixer->SetControl (LINE_OUT_8, usSrcLine, CONTROL_MUTE, 0, TRUE);
    }

  // Route this device to the Digital Output
  pMixer->SetControl (LINE_OUT_7, usPMixSrc, CONTROL_SOURCE, 0,
		      ulPlayMixLeft);
  pMixer->SetControl (LINE_OUT_8, usPMixSrc, CONTROL_SOURCE, 0,
		      ulPlayMixRight);

  // Unmute this playmix line
  pMixer->SetControl (LINE_OUT_7, usPMixSrc, CONTROL_MUTE, 0, FALSE);
  pMixer->SetControl (LINE_OUT_8, usPMixSrc, CONTROL_MUTE, 0, FALSE);

  // Set the volume to MAX on both the Play Mix and Master
  pMixer->SetControl (LINE_OUT_7, usPMixSrc, CONTROL_VOLUME, 0, MAX_VOLUME);
  pMixer->SetControl (LINE_OUT_8, usPMixSrc, CONTROL_VOLUME, 0, MAX_VOLUME);
  pMixer->SetControl (LINE_OUT_7, LINE_NO_SOURCE, CONTROL_VOLUME, 0,
		      MAX_VOLUME);
  pMixer->SetControl (LINE_OUT_8, LINE_NO_SOURCE, CONTROL_VOLUME, 0,
		      MAX_VOLUME);

  // Turn off Phase & Dither
  pMixer->SetControl (LINE_OUT_7, usPMixSrc, CONTROL_PHASE, 0, FALSE);
  pMixer->SetControl (LINE_OUT_8, usPMixSrc, CONTROL_PHASE, 0, FALSE);
  pMixer->SetControl (LINE_OUT_7, LINE_NO_SOURCE, CONTROL_PHASE, 0, FALSE);
  pMixer->SetControl (LINE_OUT_8, LINE_NO_SOURCE, CONTROL_PHASE, 0, FALSE);
  pMixer->SetControl (LINE_OUT_7, LINE_NO_SOURCE, CONTROL_DITHER, 0, FALSE);
  pMixer->SetControl (LINE_OUT_8, LINE_NO_SOURCE, CONTROL_DITHER, 0, FALSE);

  // Turn on Non-Audio
  m_pHalAdapter->Get8420 ()->SetOutputNonAudio (TRUE);

  // Update the Mixer UI
  for (usSrcLine = LINE_PLAYMIX_1; usSrcLine <= LINE_PLAYMIX_4; usSrcLine++)
    {
      pMixer->ControlChanged (LINE_OUT_7, usSrcLine, CONTROL_MUTE);
      pMixer->ControlChanged (LINE_OUT_8, usSrcLine, CONTROL_MUTE);
    }

  pMixer->ControlChanged (LINE_OUT_7, usPMixSrc, CONTROL_SOURCE);
  pMixer->ControlChanged (LINE_OUT_8, usPMixSrc, CONTROL_SOURCE);

  pMixer->ControlChanged (LINE_OUT_7, usPMixSrc, CONTROL_VOLUME);
  pMixer->ControlChanged (LINE_OUT_8, usPMixSrc, CONTROL_VOLUME);
  pMixer->ControlChanged (LINE_OUT_7, LINE_NO_SOURCE, CONTROL_VOLUME);
  pMixer->ControlChanged (LINE_OUT_8, LINE_NO_SOURCE, CONTROL_VOLUME);

  pMixer->ControlChanged (LINE_OUT_7, LINE_NO_SOURCE, CONTROL_DITHER);
  pMixer->ControlChanged (LINE_OUT_8, LINE_NO_SOURCE, CONTROL_DITHER);

  pMixer->ControlChanged (LINE_ADAPTER, LINE_NO_SOURCE,
			  CONTROL_DIGITALOUT_STATUS);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::PrepareForPCM (void)
// This code is only useful on a card with a CS8420
/////////////////////////////////////////////////////////////////////////////
{
  PHAL8420 pCS8420 = m_pHalAdapter->Get8420 ();
  PHALMIXER pMixer = m_pHalAdapter->GetMixer ();

  if (!pCS8420)
    return (HSTATUS_INVALID_MODE);

  ULONG ulOutputStatus = pCS8420->GetOutputStatus ();

  // if the 8420 is currently showing Non-Audio turned ON, then we restore the mixer state
  if (ulOutputStatus & MIXVAL_OUTSTATUS_NONAUDIO)
    {
      USHORT usDstLine, usSrcLine;
      ULONG ulSource;
      ULONG ulPlayMixLeft = (m_ulDeviceNumber * 2) + LEFT;
      ULONG ulPlayMixRight = (m_ulDeviceNumber * 2) + RIGHT;

      cmn_err (CE_WARN, "WAVE_FORMAT_PCM on PLAYx: Changing Mixer Layout\n");

      // Mute any of the Analog Outputs that have this device routed
      for (usDstLine = LINE_OUT_1; usDstLine <= LINE_OUT_6; usDstLine++)
	{
	  for (usSrcLine = LINE_PLAYMIX_1; usSrcLine <= LINE_PLAYMIX_4;
	       usSrcLine++)
	    {
	      pMixer->GetControl (usDstLine, usSrcLine, CONTROL_SOURCE, 0,
				  &ulSource);
	      if ((ulSource == ulPlayMixLeft) || (ulSource == ulPlayMixRight))
		{
		  pMixer->SetControl (usDstLine, usSrcLine, CONTROL_MUTE, 0, FALSE);	// turn the mute OFF
		  pMixer->ControlChanged (usDstLine, usSrcLine, CONTROL_MUTE);
		}
	    }
	}

      // DAH Jul 7, 2006
      // Unmute the playmix line
      pMixer->SetControl (LINE_OUT_7, LINE_PLAYMIX_1, CONTROL_MUTE, 0, FALSE);
      pMixer->SetControl (LINE_OUT_8, LINE_PLAYMIX_1, CONTROL_MUTE, 0, FALSE);
      pMixer->ControlChanged (LINE_OUT_7, LINE_PLAYMIX_1, CONTROL_MUTE);
      pMixer->ControlChanged (LINE_OUT_8, LINE_PLAYMIX_1, CONTROL_MUTE);

      // Mute the playmix line
      pMixer->SetControl (LINE_OUT_7, LINE_PLAYMIX_3, CONTROL_MUTE, 0, TRUE);
      pMixer->SetControl (LINE_OUT_8, LINE_PLAYMIX_3, CONTROL_MUTE, 0, TRUE);
      pMixer->ControlChanged (LINE_OUT_7, LINE_PLAYMIX_3, CONTROL_MUTE);
      pMixer->ControlChanged (LINE_OUT_8, LINE_PLAYMIX_3, CONTROL_MUTE);

      // Turn OFF Non-Audio
      m_pHalAdapter->Get8420 ()->SetOutputNonAudio (FALSE);
      pMixer->ControlChanged (LINE_ADAPTER, LINE_NO_SOURCE,
			      CONTROL_DIGITALOUT_STATUS);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
LONG
CHalWaveDevice::GetHWIndex (BOOLEAN bFromHardware)
// Never used for V2 driver
/////////////////////////////////////////////////////////////////////////////
{
  if (bFromHardware)
    return (m_RegStreamStatus.Read () & REG_L2STRMSTAT_L2PTR_MASK);
  else
    return (m_lHWIndex);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalWaveDevice::GetTransferSize (PULONG pulTransferSize,
				   PULONG pulCircularBufferSize)
// Never used for AES16e
// Called at interupt to time determine the amount of audio to transfer
/////////////////////////////////////////////////////////////////////////////
{
  LONG lSize;

  //cmn_err((CE_WARN,"CHalWaveDevice::GetTransferSize\n"));

  // if we are using pre-programmed interrupt sizes, don't read the actual hardware
  if (!m_ulInterruptSamples)
    m_lHWIndex = GetHWIndex ();

  //DB('h',COLOR_BOLD);   DX16( (USHORT)m_lHWIndex, COLOR_BOLD );

  *pulCircularBufferSize = (WAVE_CIRCULAR_BUFFER_SIZE - 1) * sizeof (DWORD);	// in bytes

  if (m_usMode == MODE_RUNNING)
    {
      lSize = m_lHWIndex - m_lPCIndex;
      //cmn_err((CE_WARN, " H%ld P%ld S%ld ", m_lHWIndex, m_lPCIndex, lSize ));
      //DB('h',COLOR_UNDERLINE);      DX16( (USHORT)m_lHWIndex, COLOR_BOLD );
      //DB('p',COLOR_UNDERLINE);      DX16( (USHORT)m_lPCIndex, COLOR_BOLD );
      //DB('s',COLOR_UNDERLINE);      DX16( (USHORT)lSize, COLOR_BOLD );

      // if this is a record device
      if (m_bIsRecord)
	{
	  // lSize == 0 means no data is waiting
	  if (lSize < 0)
	    lSize += WAVE_CIRCULAR_BUFFER_SIZE;

	  if (!lSize)
	    {
	      // on the LynxTWO, if the HWIndex & PCIndex are exactly equal AND the 
	      // overrun bit is set then the buffer is completely FULL not empty.
	      ULONG ulStreamStatus = m_RegStreamStatus.Read ();

	      // was the overrun bit set?
	      if (ulStreamStatus & REG_L2STRMSTAT_OVER)
		{
		  lSize = WAVE_CIRCULAR_BUFFER_SIZE;
		  cmn_err (CE_WARN, "Buffer Overrun+Full!\n");
		}
	      else
		{
		  cmn_err (CE_WARN,
			   "HW Error: Record interrupt generated when no data is available!\n");
		}
	    }
	  lSize--;		// DAH Mar 22 02 make sure we don't empty the buffer
	}
      else			// this is a play device
	{
	  // lSize == 0 means the buffer can be completely filled
	  if (lSize <= 0)
	    lSize += WAVE_CIRCULAR_BUFFER_SIZE;
	  lSize--;		// make sure we don't fill the buffer
	}
    }
  else				// in IDLE mode
    {
      lSize = WAVE_CIRCULAR_BUFFER_SIZE - 1;
    }

  // make sure we always leave one DWORD free
  if (lSize >= WAVE_CIRCULAR_BUFFER_SIZE)
    lSize = WAVE_CIRCULAR_BUFFER_SIZE - 1;

  //cmn_err((CE_WARN,"s%ld ", lSize ));

  // lSize is now in DWORDs - must convert it to bytes
  *pulTransferSize = lSize * sizeof (DWORD);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalWaveDevice::TransferAudio (PVOID pBuffer, ULONG ulBufferSize,
				 PULONG pulBytesTransfered, LONG lPCIndex)
// MME / DirectSound Version
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulToTransfer, ulTransferred;
  ULONG ulLength;
  PULONG pHWBuffer;
  PULONG pPCBuffer = (PULONG) pBuffer;

  ulLength = ulBufferSize / sizeof (DWORD);
  *pulBytesTransfered = ulLength * sizeof (DWORD);

  // are we overriding the PCIndex (DirectSound only)
  if (lPCIndex != -1)
    m_lPCIndex = lPCIndex;

  while (ulLength)
    {
      // will this transfer need to be split into two parts?
      if ((ulLength + m_lPCIndex) >= WAVE_CIRCULAR_BUFFER_SIZE)
	ulToTransfer = WAVE_CIRCULAR_BUFFER_SIZE - m_lPCIndex;	// from the current position to the end of the buffer
      else
	ulToTransfer = ulLength;	// Transfer the entire buffer in one shot

      pHWBuffer = m_pAudioBuffer + m_lPCIndex;

      ulTransferred = ulToTransfer;	// save the amount we are going to transfer 
      // ulToTransfer might get destroyed during the transfer

      if (m_bIsRecord)
	{
	  READ_REGISTER_BUFFER_ULONG (pHWBuffer, pPCBuffer, ulToTransfer);
	}
      else
	{
	  WRITE_REGISTER_BUFFER_ULONG (pHWBuffer, pPCBuffer, ulToTransfer);
	}

      m_lPCIndex += ulTransferred;	// advance the PCIndex
      ulLength -= ulTransferred;	// decrease the amount to go
      m_ulBytesTransferred += (ulTransferred * sizeof (DWORD));
      if (m_lBytesPerBlock)
	m_ulSamplesTransferred +=
	  ((ulTransferred * sizeof (DWORD)) / m_lBytesPerBlock);

      // if we still have more to go, move the buffer pointer forward
      if (ulLength)
	{
	  pPCBuffer += ulTransferred;
	  m_lPCIndex = 0;	// second part will always start at the begining of the buffer
	}
    }

  if (m_lPCIndex >= WAVE_CIRCULAR_BUFFER_SIZE)
    m_lPCIndex -= WAVE_CIRCULAR_BUFFER_SIZE;

  // Write the PCIndex to the hardware
  //DB('P',COLOR_NORMAL); DX16( (USHORT)m_lPCIndex, COLOR_NORMAL );       DC(' ');
  m_RegStreamControl.Write ((REG_L2STRMCTL_XFERDONE | m_lPCIndex),
			    (REG_L2STRMCTL_PCPTR_MASK |
			     REG_L2STRMCTL_XFERDONE));
  //cmn_err((CE_WARN,"h%ld p%ld ", m_RegStreamStatus.Read() & REG_STREAMSTAT_L2PTR_MASK, m_lPCIndex ));

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalWaveDevice::TransferAudio (PVOID pvLeft, PVOID pvRight,
				 ULONG ulSamplesToTransfer, LONG lPCIndex)
// ASIO Version, Sample Format will always be 32-bit Stereo
/////////////////////////////////////////////////////////////////////////////
{
  PULONG pHWBuffer = (PULONG) m_pAudioBuffer;
  LONG lLSample = 0, lRSample = 0;

  // are we overriding the PCIndex
  if (lPCIndex != -1)
    m_lPCIndex = lPCIndex;

  m_ulBytesTransferred += (ulSamplesToTransfer * sizeof (DWORD) * 2);
  m_ulSamplesTransferred += ulSamplesToTransfer;

  if (m_bIsRecord)
    {
      register PLONG pLDst = (PLONG) pvLeft;
      register PLONG pRDst = (PLONG) pvRight;

      //DC('r');      DX16( (USHORT)ulSamplesToTransfer, COLOR_NORMAL );      DC(' ');

      while (ulSamplesToTransfer--)
	{
	  lLSample = READ_REGISTER_ULONG (&pHWBuffer[m_lPCIndex++]);
	  lRSample = READ_REGISTER_ULONG (&pHWBuffer[m_lPCIndex++]);
	  if (m_lPCIndex >= WAVE_CIRCULAR_BUFFER_SIZE)
	    m_lPCIndex = 0;
	  if (pLDst)
	    *pLDst++ = lLSample;
	  if (pRDst)
	    *pRDst++ = lRSample;
	}			// while
    }
  else
    {
      register PLONG pLSrc = (PLONG) pvLeft;
      register PLONG pRSrc = (PLONG) pvRight;

      //DC('p');      DX16( (USHORT)ulSamplesToTransfer, COLOR_NORMAL );      DC(' ');

      while (ulSamplesToTransfer--)
	{
	  if (pLSrc)
	    lLSample = *pLSrc++;
	  if (pRSrc)
	    lRSample = *pRSrc++;
	  WRITE_REGISTER_ULONG (&pHWBuffer[m_lPCIndex++], lLSample);	// Left Channel
	  WRITE_REGISTER_ULONG (&pHWBuffer[m_lPCIndex++], lRSample);	// Right Channel
	  if (m_lPCIndex >= WAVE_CIRCULAR_BUFFER_SIZE)
	    m_lPCIndex = 0;
	}			// while
    }

  if (m_lPCIndex >= WAVE_CIRCULAR_BUFFER_SIZE)
    m_lPCIndex -= WAVE_CIRCULAR_BUFFER_SIZE;

  // Write the PCIndex to the hardware
  //DC('{'); DX16( (USHORT)m_lPCIndex, COLOR_BOLD ); DC('}');
  m_RegStreamControl.Write ((REG_L2STRMCTL_XFERDONE | m_lPCIndex),
			    (REG_L2STRMCTL_PCPTR_MASK |
			     REG_L2STRMCTL_XFERDONE));
  //cmn_err((CE_WARN,"h%ld p%ld ", m_RegStreamStatus.Read() & REG_STREAMSTAT_L2PTR_MASK, m_lPCIndex ));

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::TransferComplete (LONG lBytesProcessed)
// Called at interupt to inform hardware that the transfer has been completed
/////////////////////////////////////////////////////////////////////////////
{
  // if we are using a pre-programmed interrupt rate
  if (m_ulInterruptSamples)
    {
      // advance the (shadow) hardware index
      m_lHWIndex += m_ulInterruptSamples;
      while (m_lHWIndex >= WAVE_CIRCULAR_BUFFER_SIZE)
	m_lHWIndex -= WAVE_CIRCULAR_BUFFER_SIZE;
      //cmn_err((CE_WARN,"H[%ld] ", m_lHWIndex ));
    }

  // call the Position code to make sure the HWIndex doesn't roll over without us knowing it
  GetBytePosition ();

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::SetInterruptSamples (ULONG ulInterruptSamples)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulInterruptSamples > (WAVE_CIRCULAR_BUFFER_SIZE / 2))
    ulInterruptSamples = (WAVE_CIRCULAR_BUFFER_SIZE / 2);

  m_ulInterruptSamples = ulInterruptSamples;

  return (m_pHalAdapter->SetInterruptSamples (ulInterruptSamples));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::ZeroPosition (void)
/////////////////////////////////////////////////////////////////////////////
{
  m_ullBytePosition = 0;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::GetSamplePosition (PULONG pulSamplePosition)
// DRIVER MUST CALL GetBytePosition BEFORE CALLING THIS FUNCTION SO 
// m_ullBytePosition IS UPDATED!
/////////////////////////////////////////////////////////////////////////////
{
  if (m_lBytesPerBlock)
    {
      *pulSamplePosition = (ULONG) (m_ullBytePosition / m_lBytesPerBlock);
    }
  else
    {
      cmn_err (CE_WARN,
	       "CHalWaveDevice::GetSamplePosition m_lBytesPerBlock is ZERO\n");
      return (HSTATUS_INVALID_PARAMETER);
    }

  return (HSTATUS_OK);
}

/*
/////////////////////////////////////////////////////////////////////////////
ULONGLONG	CHalWaveDevice::GetSamplesPlayed( void )
/////////////////////////////////////////////////////////////////////////////
{
	ULONGLONG	ullPosition = 0;
	ULONG		ulLo, ulHi;

	if( !m_bIsRecord )
	{
		ulLo = READ_REGISTER_ULONG( m_pulPosLoReg );
		ulHi = READ_REGISTER_ULONG( m_pulPosHiReg );
		cmn_err(CE_WARN,"%08lx %08lx ", ulLo, ulHi );
		ullPosition = (ULONGLONG)ulLo + ((ULONGLONG)ulHi << 32);
	}

	return( ullPosition );
}
*/
/////////////////////////////////////////////////////////////////////////////
ULONGLONG
CHalWaveDevice::GetBytePosition (void)
/////////////////////////////////////////////////////////////////////////////
{
  LONG lHWIndex;
  LONG lDiff;

  // protect this function from re-entrancy problems - without a spin lock or other such thing
  if (!m_ulGBPEntryCount)
    {
      m_ulGBPEntryCount++;
      ////////////////////
      // Protected
      ////////////////////

      lHWIndex = GetHWIndex (TRUE);

      lDiff = lHWIndex - m_lGBPLastHWIndex;
      if (lDiff < 0)
	lDiff += WAVE_CIRCULAR_BUFFER_SIZE;

      // save the current HW index as the last HW index for the next time around.
      m_lGBPLastHWIndex = lHWIndex;

      // lDiff now has the number of DWORDs that have be processed by the hardware
      // since the last GetBytePosition call.  This needs to be changed to the number
      // of bytes by multiplying by 4.
      lDiff *= sizeof (DWORD);

      // NOTE: DAH May 06, 2005
      // Due to an apparent bug in the MSVC6.0 compiler, we must cast this to a ULONGLONG before the add
      // to get it to generate code that does a carry.

      // increase the overall sample count for this device
      m_ullBytePosition += (ULONGLONG) lDiff;

      ////////////////////
      // Protected
      ////////////////////
      m_ulGBPEntryCount--;
    }

  return (m_ullBytePosition);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalWaveDevice::GetOverrunCount (VOID)
// Called by:
//      CHalMixer::GetSharedControls
/////////////////////////////////////////////////////////////////////////////
{
  if (m_bIsAES16e)
    {
      // 10-bit dropout counters in b31-b22 of the stream status registers
      return (m_RegStreamStatus.Read () >> 22);
    }

  return (m_ulOverrunCount);
}

/////////////////////////////////////////////////////////////////////////////
VOID
CHalWaveDevice::IncreaseOverrunCount (ULONG ulCount)
// DAH Added 11/14/2011
/////////////////////////////////////////////////////////////////////////////
{
  m_ulOverrunCount += ulCount;
}

#ifdef DEBUG
/////////////////////////////////////////////////////////////////////////////
VOID
CHalWaveDevice::DebugPrintStatus (VOID)
// Never used with V2 driver
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulStreamStatus = m_RegStreamStatus.Read ();
  ULONG ulStreamControl = m_RegStreamControl.Read ();
  DC ('[');
  DX16 ((USHORT) (ulStreamStatus & REG_L2STRMSTAT_L2PTR_MASK), COLOR_NORMAL);
  DC (',');
  DX16 ((USHORT) (ulStreamControl & REG_L2STRMCTL_PCPTR_MASK), COLOR_NORMAL);
  DC (']');
}
#endif

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDevice::Service ()
// Called at interrupt time to service the circular buffer
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulReason = kReasonWave;
  //DC('W');
  cmn_err (CE_WARN, "CHalWaveDevice::Service: %ld\n", m_ulDeviceNumber);
  //DB('s',COLOR_BOLD);   DX8( (BYTE)m_ulDeviceNumber, COLOR_NORMAL );

  // go see why we were called
  ULONG ulStreamStatus = m_RegStreamStatus.Read ();
  //DebugPrintStatus();

  //cmn_err((CE_WARN,"Stream Status %08lx\n", ulStreamStatus ));

  // was the limit hit bit set?
  if (ulStreamStatus & REG_L2STRMSTAT_LIMHIT)
    {
      //DC('L');
      //cmn_err((CE_WARN,"Limit Hit\n"));
    }

  // was the overrun bit set?
  if (ulStreamStatus & REG_L2STRMSTAT_OVER)
    {
      DB ('O', COLOR_BOLD);
      m_ulOverrunCount++;
      //cmn_err((CE_WARN,"\nCHalWaveDevice: Overrun Detected %ld\n", m_ulOverrunCount ));
      ulReason = kReasonWaveEmpty;
    }

  if (m_usMode != MODE_RUNNING)
    {
      //cmn_err((CE_WARN,"<%02ld %08lx %08lx>", m_ulDeviceNumber, m_RegStreamControl.Read(), ulStreamStatus ));
      Stop ();
      //cmn_err((CE_WARN,"Interrupt Detected on IDLE device %02ld\n", m_ulDeviceNumber ));
      return (HSTATUS_INVALID_MODE);
    }

  // let the driver service the interrupt for this device
  CHalDevice::Service (ulReason);

  return (HSTATUS_OK);
}
