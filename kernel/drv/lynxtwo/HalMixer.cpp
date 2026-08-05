/****************************************************************************
 HalMixer.cpp

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
 Oct 08 04 DAH	Added support for Aurora
 May 19 03 DAH	Renamed RegisterCallback to RegisterCallbacks and added 
				RestoreSceneCallback & SaveSceneCallback so driver can restore
				/save scenes to/from persistent storage.
 May 19 03 DAH  Added CONTROL_MIXER_RESTORE_SCENE & CONTROL_MIXER_SAVE_SCENE.
 May 19 03 DAH	Moved GetControlType() and IsControlWriteable() from driver 
				into HAL.
 May 19 03 DAH	Added SetDefaults() function
****************************************************************************/

#include <StdAfx.h>
#include "HalAdapter.h"

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalMixer::Open (PHALADAPTER pHalAdapter)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err(CE_WARN,"CHalMixer::Open\n");

  PLYNXTWOREGISTERS pRegisters;
  ULONG i;

  m_pHalAdapter = pHalAdapter;
  m_pSampleClock = m_pHalAdapter->GetSampleClock ();	// Cannot use, Has *NOT* been opened yet
  m_pCS8420 = m_pHalAdapter->Get8420 ();	// Cannot use, Has *NOT* been opened yet
  m_pAK4114 = m_pHalAdapter->Get4114 ();
  m_pAES16eDIO = m_pHalAdapter->GetAES16eDIO ();
  m_pLStream = m_pHalAdapter->GetLStream ();
  m_pTCTx = m_pHalAdapter->GetTCTx ();
  m_pTCRx = m_pHalAdapter->GetTCRx ();
  m_pAurora = m_pHalAdapter->GetAurora ();

  pRegisters = m_pHalAdapter->GetRegisters ();
  m_usDeviceID = m_pHalAdapter->GetDeviceID ();

  m_bHasNewPMix = m_pHalAdapter->HasNewPMix ();	// Shadow this so we don't have to keep looking it up
  m_bHasPMixV3 = m_pHalAdapter->HasPMixV3 ();	// Shadow this so we don't have to keep looking it up

  m_bIsAES16 = m_pHalAdapter->IsAES16 ();
  m_bIsAES16e = m_pHalAdapter->IsAES16e ();

  m_ulNumInputs = 16;
  m_ulNumOutputs = 16;

  if (m_bHasPMixV3)
    {
      m_ulNumInputs = m_pHalAdapter->GetNumWaveInDevices () * 2;
      m_ulNumOutputs = m_pHalAdapter->GetNumWaveOutDevices () * 2;
    }

  if (m_bHasPMixV3)
    {
      PMIXBLOCKV3 pMixBlock = (PMIXBLOCKV3) & pRegisters->MIXBlockV3;

      for (i = 0; i < m_ulNumOutputs; i++)
	{
	  m_aPlayMixV3[i].Open (m_pHalAdapter,
				(USHORT) i,
				&pMixBlock->PMixVolume[i],
				&pMixBlock->PMixMiscControl[i],
				&pMixBlock->PMixStatus[i]);
	}
      for (i = 0; i < m_ulNumInputs; i++)
	{
	  m_aRecordMix[i].Open (pHalAdapter,
				&pMixBlock->RMixControl[i],
				&pMixBlock->RMixStatus[i]);
	}
    }
  else if (m_bHasNewPMix)
    {
      PMIXBLOCKNEW pMixBlock = (PMIXBLOCKNEW) & pRegisters->MIXBlock;

      for (i = 0; i < m_ulNumOutputs; i++)
	{
	  m_aPlayMixNew[i].Open (m_pHalAdapter,
				 (USHORT) i,
				 &pMixBlock->PMixControlLO[i],
				 &pMixBlock->PMixControlHI[i],
				 &pMixBlock->PMixStatus[i]);
	}
      for (i = 0; i < m_ulNumInputs; i++)
	{
	  m_aRecordMix[i].Open (pHalAdapter,
				&pMixBlock->RMixControl[i],
				&pMixBlock->RMixStatus[i]);
	}
    }
  else
    {
      for (i = 0; i < m_ulNumOutputs; i++)
	{
	  m_aPlayMix[i].Open (m_pHalAdapter,
			      (USHORT) i,
			      &pRegisters->MIXBlock.PMixControl[i],
			      &pRegisters->MIXBlock.PMixStatus[i]);
	}
      for (i = 0; i < m_ulNumInputs; i++)
	{
	  m_aRecordMix[i].Open (pHalAdapter,
				&pRegisters->MIXBlock.RMixControl[i],
				&pRegisters->MIXBlock.RMixStatus[i]);
	}
    }

  m_pControlChangedCallback = NULL;
  m_pSaveSceneCallback = NULL;
  m_pRestoreSceneCallback = NULL;
  m_pContext = NULL;

  SetDefaults (TRUE);

  m_bOpen = TRUE;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalMixer::Close ()
/////////////////////////////////////////////////////////////////////////////
{
  m_bOpen = FALSE;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalMixer::RegisterCallbacks (PMIXERCONTROLCHANGEDCALLBACK
				pControlChangedCallback,
				PMIXERSCENECALLBACK pRestoreSceneCallback,
				PMIXERSCENECALLBACK pSaveSceneCallback,
				PVOID pContext)
/////////////////////////////////////////////////////////////////////////////
{
  m_pControlChangedCallback = pControlChangedCallback;
  m_pRestoreSceneCallback = pRestoreSceneCallback,
    m_pSaveSceneCallback = pSaveSceneCallback, m_pContext = pContext;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalMixer::ControlChanged (USHORT usDstLine, USHORT usSrcLine,
			     USHORT usControl)
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usStatus = HSTATUS_OK;

  //cmn_err(CE_WARN,"ControlChanged %04x %04x %04x\n", usDstLine, usSrcLine, usControl );
  if (!m_bOpen)
    return (HSTATUS_ADAPTER_NOT_OPEN);

  if (m_pControlChangedCallback)
    usStatus =
      m_pControlChangedCallback (m_pContext, usDstLine, usSrcLine, usControl);

  return (usStatus);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalMixer::SetDefaults (BOOLEAN bDriverLoading)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG i;

  //cmn_err(CE_WARN,"CHalMixer::SetDefaults\n");

  //cmn_err(CE_WARN,"CHalMixer::SetDefaults Outputs\n");

  for (i = 0; i < m_ulNumOutputs; i++)
    {
      if (m_bHasPMixV3)
	m_aPlayMixV3[i].SetDefaults (bDriverLoading);
      else if (m_bHasNewPMix)
	m_aPlayMixNew[i].SetDefaults (bDriverLoading);
      else
	m_aPlayMix[i].SetDefaults (bDriverLoading);
    }

  //cmn_err(CE_WARN,"CHalMixer::SetDefaults Inputs\n");
  for (i = 0; i < m_ulNumInputs; i++)
    {
      if (m_bIsAES16 || m_bIsAES16e)
	{
	  m_aRecordMix[i].SetSource ((USHORT) i);
	}
      else
	{
	  // Set the source to a default value
	  if (i > 7)
	    m_aRecordMix[i].SetSource ((USHORT) i + 16);	// LStream 2
	  else
	    m_aRecordMix[i].SetSource ((USHORT) i);
	}

      if (!bDriverLoading)
	{
	  m_aRecordMix[i].SetMute (FALSE);
	  m_aRecordMix[i].SetDither (TRUE);
	}
    }

  // if this is a B Model, reassign the Record Source Selection
  if (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
    {
      m_aRecordMix[LINE_OUT_3].SetSource (0);
      m_aRecordMix[LINE_OUT_4].SetSource (1);
      m_aRecordMix[LINE_OUT_5].SetSource (0);
      m_aRecordMix[LINE_OUT_6].SetSource (1);
    }

  if (!bDriverLoading)
    {
      m_pSampleClock->SetDefaults ();

      // LynxTWO Digital I/O
      if (m_pCS8420)
	{
	  m_pCS8420->SetDefaults ();
	}

      // AES16 Digital I/O
      if (m_pAK4114)
	{
	  m_pAK4114->SetDefaults ();
	}

      // AES16e Digital I/O
      if (m_pAES16eDIO)
	{
	  m_pAES16eDIO->SetDefaults ();
	}

      // m_pCS8420->SetDefaults(); already does this...
      //m_pLStream->AESSetInputMuteOnError( LSTREAM_BRACKET, TRUE );
      //m_pLStream->AESSetInputMuteOnError( LSTREAM_HEADER, TRUE );

      // LTC Generator
      if (m_pTCTx)
	{
	  m_pTCTx->SetDefaults ();
	}

      // BUGBUG This should call HalAdapter's SetDefaults (if we had one!)
      m_pHalAdapter->SetMTCSource (MIXVAL_MTCSOURCE_LTCRX);

      // Misc controls
      m_pHalAdapter->SetDitherType (m_aRecordMix[0].GetMixControl (),
				    MIXVAL_DITHER_TRIANGULAR_PDF);

      m_pHalAdapter->SetADHPF (CONTROL_AIN12_HPF, TRUE);
      m_pHalAdapter->SetADHPF (CONTROL_AIN34_HPF, TRUE);
      m_pHalAdapter->SetADHPF (CONTROL_AIN56_HPF, TRUE);

      m_pHalAdapter->SetDADeEmphasis (FALSE);
      m_pHalAdapter->SetSyncStartState (TRUE);
    }

  if (m_pHalAdapter->IsLynxTWO ())
    {
      // It doesn't matter which model we are using, we set them all to +4 regardless.
      m_pHalAdapter->SetTrim (CONTROL_AIN12_TRIM, MIXVAL_TRIM_PLUS4);
      m_pHalAdapter->SetTrim (CONTROL_AIN34_TRIM, MIXVAL_TRIM_PLUS4);
      m_pHalAdapter->SetTrim (CONTROL_AIN56_TRIM, MIXVAL_TRIM_PLUS4);
      m_pHalAdapter->SetTrim (CONTROL_AOUT12_TRIM, MIXVAL_TRIM_PLUS4);
      m_pHalAdapter->SetTrim (CONTROL_AOUT34_TRIM, MIXVAL_TRIM_PLUS4);
      m_pHalAdapter->SetTrim (CONTROL_AOUT56_TRIM, MIXVAL_TRIM_PLUS4);
    }

  if (m_pAurora && !bDriverLoading)
    {
      m_pAurora->SetDefaults ();
    }

  // DAH Added 5/8/2008
  if (m_pLStream && !bDriverLoading)
    {
      m_pLStream->SetDefaults ();
    }

#ifdef OSX
  m_ulDriverVariables[CONTROL_DRVBUFSIZE_1X - CONTROL_DRVBUFSIZE_1X] = 32;
  m_ulDriverVariables[CONTROL_DRVNUMRECDEVS_1X - CONTROL_DRVBUFSIZE_1X] = 8;
  m_ulDriverVariables[CONTROL_DRVNUMPLAYDEVS_1X - CONTROL_DRVBUFSIZE_1X] = 8;

  m_ulDriverVariables[CONTROL_DRVBUFSIZE_2X - CONTROL_DRVBUFSIZE_1X] = 64;
  m_ulDriverVariables[CONTROL_DRVNUMRECDEVS_2X - CONTROL_DRVBUFSIZE_1X] = 8;
  m_ulDriverVariables[CONTROL_DRVNUMPLAYDEVS_2X - CONTROL_DRVBUFSIZE_1X] = 8;

  m_ulDriverVariables[CONTROL_DRVBUFSIZE_4X - CONTROL_DRVBUFSIZE_1X] = 128;
  m_ulDriverVariables[CONTROL_DRVNUMRECDEVS_4X - CONTROL_DRVBUFSIZE_1X] = 4;
  m_ulDriverVariables[CONTROL_DRVNUMPLAYDEVS_4X - CONTROL_DRVBUFSIZE_1X] = 4;
#else
  m_ulDriverVariables[CONTROL_DRVBUFSIZE_1X - CONTROL_DRVBUFSIZE_1X] = 256;
  m_ulDriverVariables[CONTROL_DRVNUMPLAYDEVS_1X - CONTROL_DRVBUFSIZE_1X] = 2;	// Number of Channels for Play 1 & Record 1

  m_ulDriverVariables[CONTROL_DRVBUFSIZE_4X - CONTROL_DRVBUFSIZE_1X] = 0;
#endif

  m_bMixerLock = FALSE;

  //cmn_err(CE_WARN,"CHalMixer::SetDefaults Done.\n");

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalMixer::GetFirstMatchingLine (USHORT usSource, PUSHORT pusDstLine,
				   PUSHORT pusSrcLine)
//      Searches each play mixer (starting with Analog Out 1, PMixA) for the 
//      first matching connection.
//      Returns 0xFFFF if no connection found
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usDstLine, usSrcLine, usLine;

  for (usDstLine = LINE_OUT_1; usDstLine <= (LINE_OUT_1 + m_ulNumOutputs);
       usDstLine++)
    {
      if (m_bHasPMixV3)
	{
	  return (HSTATUS_INVALID_MIXER_LINE);
	}
      else if (m_bHasNewPMix)
	{
	  for (usSrcLine = LINE_PLAYMIX_1; usSrcLine <= LINE_PLAYMIX_22;
	       usSrcLine++)
	    {
	      usLine =
		(USHORT) m_aPlayMixNew[usDstLine].GetSource (usSrcLine);
	      if (usLine == usSource)
		{
		  *pusDstLine = usDstLine;
		  *pusSrcLine = usSrcLine;
		  return (HSTATUS_OK);
		}
	    }
	}
      else
	{
	  for (usSrcLine = LINE_PLAYMIX_1; usSrcLine <= LINE_PLAYMIX_4;
	       usSrcLine++)
	    {
	      usLine = m_aPlayMix[usDstLine].GetSource (usSrcLine);
	      if (usLine == usSource)
		{
		  *pusDstLine = usDstLine;
		  *pusSrcLine = usSrcLine;
		  return (HSTATUS_OK);
		}
	    }
	}
    }

  return (HSTATUS_INVALID_MIXER_LINE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalMixer::GetSharedControls (USHORT usControl, PSHAREDCONTROLS pShared)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG i;

  //cmn_err(CE_WARN,"CHalMixer::GetSharedControls\n");
  //DB('s',COLOR_UNDERLINE);

  RtlZeroMemory (pShared, sizeof (SHAREDCONTROLS));

  pShared->ulControlRequest = REQ_FAST_CONTROLS;

  // Metering
  for (i = 0; i < m_ulNumOutputs; i++)
    {
      pShared->Fast.aulInputMeters[i] = m_aRecordMix[i].GetLevel ();
      m_aRecordMix[i].ResetLevel ();

      if (m_bHasPMixV3)
	{
	  pShared->Fast.aulOutputMeters[i] = m_aPlayMixV3[i].GetLevel ();
	}
      else if (m_bHasNewPMix)
	{
	  pShared->Fast.aulOutputMeters[i] = m_aPlayMixNew[i].GetLevel ();
	  m_aPlayMixNew[i].ResetLevel ();
	}
      else
	{
	  pShared->Fast.aulOutputMeters[i] = m_aPlayMix[i].GetLevel ();
	  m_aPlayMix[i].ResetLevel ();
	}
    }

  // LTC In
  if (m_pTCRx)
    m_pTCRx->GetPosition (&pShared->Fast.ulLTCInPosition);
  // LTC Out
  if (m_pTCTx)
    m_pTCTx->GetPosition (&pShared->Fast.ulLTCOutPosition);

  // LStream
  m_pLStream->ADATGetSyncInTimeCode (LSTREAM_BRACKET,
				     &pShared->Fast.ulLS1ADATPosition);
  m_pLStream->ADATGetSyncInTimeCode (LSTREAM_HEADER,
				     &pShared->Fast.ulLS2ADATPosition);

  if (usControl == CONTROL_SLOW_SHARED_CONTROLS)
    {
      SET (pShared->ulControlRequest, REQ_SLOW_CONTROLS);
      //DC('s');

      // Sample Clock
      m_pSampleClock->GetMixerControl (CONTROL_CLOCKSOURCE,
				       &pShared->Slow.ulCurrentClockSource);
      m_pSampleClock->GetMixerControl (CONTROL_CLOCKRATE,
				       &pShared->Slow.ulCurrentClockRate);

      // Metering
      for (i = 0; i < m_ulNumOutputs; i++)
	{
	  if (m_bHasPMixV3)
	    pShared->Slow.aulOutputOverload[i] =
	      m_aPlayMixV3[i].GetOverload ();
	  else if (m_bHasNewPMix)
	    pShared->Slow.aulOutputOverload[i] =
	      m_aPlayMixNew[i].GetOverload ();
	  else
	    pShared->Slow.aulOutputOverload[i] = m_aPlayMix[i].GetOverload ();
	}
      // Digital I/O
      if (m_pCS8420)
	{
	  m_pCS8420->GetInputSampleRate (&pShared->Slow.lDigitalInRate);
	  m_pCS8420->GetSRCRatio (&pShared->Slow.ulDigitalInSRCRatio);
	  m_pCS8420->GetInputStatus (&pShared->Slow.ulDigitalInStatus);
	}
      // Frequency Counters
      for (i = 0; i < SC_NUM_FREQUENCY_COUNTERS; i++)
	{
	  m_pHalAdapter->GetFrequencyCounter ((USHORT) i,
					      &pShared->Slow.
					      aulFrequencyCounters[i]);
	}
      // AES16
      if (m_pAK4114)
	{
	  for (i = 0; i < SC_NUM_AES16_STATUS; i++)
	    {
	      m_pAK4114->GetInputStatus (i,
					 &pShared->Slow.
					 aulDigitalInStatus[i]);
	    }
	  for (i = 0; i < SC_NUM_AES16_SRC; i++)
	    {
	      m_pAK4114->GetSRCRatio (kSRC0 + i,
				      &pShared->Slow.aulDigitalInSRCRatio[4 +
									  i]);
	    }
	}
      // AES16e
      if (m_pAES16eDIO)
	{
	  for (i = 0; i < SC_NUM_AES16_STATUS; i++)
	    {
	      m_pAES16eDIO->GetInputStatus (i,
					    &pShared->Slow.
					    aulDigitalInStatus[i]);
	    }
	  for (i = 0; i < SC_NUM_AES16e_SRC; i++)
	    {
	      m_pAES16eDIO->GetSRCRatio (i,
					 &pShared->Slow.
					 aulDigitalInSRCRatio[i]);
	    }
	}

      if (m_pHalAdapter->HasSynchroLock ())
	{
	  m_pSampleClock->GetMixerControl (CONTROL_SYNCHROLOCK_STATUS,
					   &pShared->Slow.
					   ulSynchroLockStatus);
	}

      // LTC In
      if (m_pTCRx)
	{
	  m_pTCRx->GetFrameRate (&pShared->Slow.ulLTCInFramerate);
	  pShared->Slow.bLTCInLock = m_pTCRx->IsInputLocked ();
	  pShared->Slow.ulLTCInDirection = m_pTCRx->GetInputDirection ();
	  pShared->Slow.bLTCInDropframe = m_pTCRx->GetDropFrame ();
	}
      // Device Dropout Count
      for (i = 0; i < (m_ulNumInputs / 2); i++)
	{
	  pShared->Slow.aulRecordDeviceDropout[i] =
	    m_pHalAdapter->GetWaveDMARecordDevice (i)->GetOverrunCount ();
	  pShared->Slow.aulPlayDeviceDropout[i] =
	    m_pHalAdapter->GetWaveDMAPlayDevice (i)->GetOverrunCount ();
	}

      // LStream
      ULONG ulLStream, ulChip;

      for (ulLStream = 0; ulLStream < SC_NUM_LSTREAM_PORTS; ulLStream++)
	{
	  pShared->Slow.ulLSDeviceID[ulLStream] =
	    m_pLStream->GetDeviceID (ulLStream);

	  if (pShared->Slow.ulLSDeviceID[ulLStream] == REG_LSDEVID_LSADAT)
	    for (ulChip = 0; ulChip < SC_NUM_LSADAT_PORTS; ulChip++)
	      pShared->Slow.bLSADATInLock[ulLStream][ulChip] =
		m_pLStream->ADATIsLocked (i, ulChip);

	  if (pShared->Slow.ulLSDeviceID[ulLStream] == REG_LSDEVID_LSAES)
	    {
	      for (ulChip = 0; ulChip < SC_NUM_LSAES_PORTS; ulChip++)
		{
		  pShared->Slow.ulLSAESStatus[ulLStream][ulChip] =
		    m_pLStream->AESGetInputStatus (ulLStream,
						   k8420_A + ulChip);
		  pShared->Slow.ulLSAESSRCRatio[ulLStream][ulChip] =
		    m_pLStream->AESGetSRCRatio (ulLStream, k8420_A + ulChip);
		}
	    }
	}
    }
  return (HSTATUS_OK);
}

///////////////////////////////////////////////////////////////////////////////
ULONG
CHalMixer::GetControlType (USHORT usControl)
// Converts a HAL control name to a standardized mixer control type
///////////////////////////////////////////////////////////////////////////////
{
  switch (usControl)
    {
    case CONTROL_VOLUME:	// ushort
//      case CONTROL_MONITOR_VOLUME:
      return (MIXER_CONTROL_TYPE_VOLUME);

//      case CONTROL_MONITOR_PAN:
//              return( MIXER_CONTROL_TYPE_PAN );

    case CONTROL_PEAKMETER:	// short
      return (MIXER_CONTROL_TYPE_PEAKMETER);

//      case CONTROL_MONITOR_LEFT:                      // mixer
//      case CONTROL_MONITOR_RIGHT:                     // mixer
//              return( MIXER_CONTROL_TYPE_MIXER );

    case CONTROL_DITHER:	// BOOLEAN
    case CONTROL_MONITOR:	// BOOLEAN
    case CONTROL_PHASE:	// BOOLEAN
    case CONTROL_CLOCKRATE_LOCK:	// BOOLEAN
    case CONTROL_WIDEWIREIN:	// BOOLEAN
    case CONTROL_WIDEWIREOUT:	// BOOLEAN
    case CONTROL_AUTOCLOCKSELECT:	// BOOLEAN
    case CONTROL_DIGITALIN_MUTE_ON_ERROR:	// BOOLEAN
    case CONTROL_LEVELS:	// BOOLEAN
    case CONTROL_SYNCIN_NTSC:	// BOOLEAN
    case CONTROL_DIGITALIN5_SRC_ENABLE:	// BOOLEAN
    case CONTROL_DIGITALIN6_SRC_ENABLE:	// BOOLEAN
    case CONTROL_DIGITALIN7_SRC_ENABLE:	// BOOLEAN
    case CONTROL_DIGITALIN8_SRC_ENABLE:	// BOOLEAN
    case CONTROL_DIO1_SRC_MODE:	// BOOLEAN
    case CONTROL_DIO2_SRC_MODE:	// BOOLEAN
    case CONTROL_DIO3_SRC_MODE:	// BOOLEAN
    case CONTROL_DIO4_SRC_MODE:	// BOOLEAN
    case CONTROL_DIO5_SRC_MODE:	// BOOLEAN
    case CONTROL_DIO6_SRC_MODE:	// BOOLEAN
    case CONTROL_DIO7_SRC_MODE:	// BOOLEAN
    case CONTROL_DIO8_SRC_MODE:	// BOOLEAN
    case CONTROL_LTCIN_LOCKED:	// BOOLEAN
    case CONTROL_LTCIN_DIRECTION:	// BOOLEAN
    case CONTROL_LTCIN_DROPFRAME:	// BOOLEAN
    case CONTROL_LTCOUT_ENABLE:	// BOOLEAN
    case CONTROL_LTCOUT_DROPFRAME:	// BOOLEAN
    case CONTROL_LS1_ADAT_IN1_LOCK:	// BOOLEAN
    case CONTROL_LS1_ADAT_IN2_LOCK:	// BOOLEAN
    case CONTROL_LS1_ADAT_CUEPOINT_ENABLE:	// BOOLEAN
    case CONTROL_LS1_AES_WIDEWIRE:	// BOOLEAN
    case CONTROL_LS2_ADAT_IN1_LOCK:	// BOOLEAN
    case CONTROL_LS2_ADAT_IN2_LOCK:	// BOOLEAN
    case CONTROL_LS2_ADAT_CUEPOINT_ENABLE:	// BOOLEAN
    case CONTROL_LS2_AES_WIDEWIRE:	// BOOLEAN
    case CONTROL_ADDA_RECALIBRATE:	// BOOLEAN
    case CONTROL_AD_RECALIBRATE:	// BOOLEAN
    case CONTROL_DA_RECALIBRATE:	// BOOLEAN
    case CONTROL_AUTO_RECALIBRATE:	// BOOLEAN
    case CONTROL_AIN12_HPF:	// BOOLEAN
    case CONTROL_AIN34_HPF:	// BOOLEAN
    case CONTROL_AIN56_HPF:	// BOOLEAN
    case CONTROL_ADHIPASSFILTER:	// BOOLEAN
    case CONTROL_DA_AUTOMUTE:	// BOOLEAN
    case CONTROL_DA_DEEMPHASIS:	// BOOLEAN
    case CONTROL_CLOCK_CHANGE_IF_ACTIVE:	// BOOLEAN
    case CONTROL_SYNCHROLOCK_ENABLE:	// BOOLEAN
    case CONTROL_DIGITALIN_SRC_MATCHPHASE:	// BOOLEAN
    case CONTROL_SYNCSTART:	// BOOLEAN
    case CONTROL_LSTREAM_DUAL_INTERNAL:	// BOOLEAN
    case CONTROL_MRM_ENABLE:	// BOOLEAN
    case CONTROL_GPOUT:	// BOOLEAN
    case CONTROL_RESET_DIGITALIO_LOCK:	// BOOLEAN
    case CONTROL_MONITOR_OFF_PLAY:	// BOOLEAN
    case CONTROL_MONITOR_ON_RECORD:	// BOOLEAN
    case CONTROL_MIXER_LOCK:	// BOOLEAN
      return (MIXER_CONTROL_TYPE_BOOLEAN);

//      case CONTROL_MONITOR_MUTE:
    case CONTROL_MUTE:
    case CONTROL_MUTEA:
    case CONTROL_MUTEB:
      return (MIXER_CONTROL_TYPE_MUTE);

    case CONTROL_SOURCE:	// MUX
    case CONTROL_SOURCE_LEFT:	// MUX
    case CONTROL_SOURCE_RIGHT:	// MUX
    case CONTROL_DITHER_DEPTH:	// MUX
    case CONTROL_INPUT_SOURCE:	// MUX
    case CONTROL_CLOCKSOURCE:	// MUX
    case CONTROL_CLOCKSOURCE_PREFERRED:	// MUX
    case CONTROL_DIGITAL_FORMAT:	// MUX
    case CONTROL_SRC_MODE:	// MUX
    case CONTROL_TRIM:		// MUX
    case CONTROL_MONITOR_SOURCE:	// MUX
    case CONTROL_AIN12_TRIM:	// MUX
    case CONTROL_AIN34_TRIM:	// MUX
    case CONTROL_AIN56_TRIM:	// MUX
    case CONTROL_AOUT12_TRIM:	// MUX
    case CONTROL_AOUT34_TRIM:	// MUX
    case CONTROL_AOUT56_TRIM:	// MUX
    case CONTROL_LTCOUT_FRAMERATE:	// MUX
    case CONTROL_LTCOUT_SYNCSOURCE:	// MUX
    case CONTROL_LS1_OUTSEL:	// MUX
    case CONTROL_LS1_ADAT_CLKSRC:	// MUX
    case CONTROL_LS1_AES_CLKSRC:	// MUX
    case CONTROL_LS1_D1_FORMAT:	// MUX
    case CONTROL_LS1_DI1_SRC_MODE:	// MUX
    case CONTROL_LS1_D2_FORMAT:	// MUX
    case CONTROL_LS1_DI2_SRC_MODE:	// MUX
    case CONTROL_LS1_D3_FORMAT:	// MUX
    case CONTROL_LS1_DI3_SRC_MODE:	// MUX
    case CONTROL_LS1_D4_FORMAT:	// MUX
    case CONTROL_LS1_DI4_SRC_MODE:	// MUX
    case CONTROL_LS2_OUTSEL:	// MUX
    case CONTROL_LS2_ADAT_CLKSRC:	// MUX
    case CONTROL_LS2_AES_CLKSRC:	// MUX
    case CONTROL_LS2_D1_FORMAT:	// MUX
    case CONTROL_LS2_DI1_SRC_MODE:	// MUX
    case CONTROL_LS2_D2_FORMAT:	// MUX
    case CONTROL_LS2_DI2_SRC_MODE:	// MUX
    case CONTROL_LS2_D3_FORMAT:	// MUX
    case CONTROL_LS2_DI3_SRC_MODE:	// MUX
    case CONTROL_LS2_D4_FORMAT:	// MUX
    case CONTROL_LS2_DI4_SRC_MODE:	// MUX
    case CONTROL_FP_ANALOGOUT_SOURCE:	// MUX
    case CONTROL_FP_DIGITALOUT_SOURCE:	// MUX
    case CONTROL_FP_METER_SOURCE:	// MUX
    case CONTROL_LSLOT_OUT_SOURCE:	// MUX
    case CONTROL_TRIM_ORIGIN:	// MUX
    case CONTROL_FP_TRIM:	// MUX
    case CONTROL_TRIM_AIN_1_4:	// MUX
    case CONTROL_TRIM_AIN_5_8:	// MUX
    case CONTROL_TRIM_AIN_9_12:	// MUX
    case CONTROL_TRIM_AIN_13_16:	// MUX
    case CONTROL_TRIM_AOUT_1_4:	// MUX
    case CONTROL_TRIM_AOUT_5_8:	// MUX
    case CONTROL_TRIM_AOUT_9_12:	// MUX
    case CONTROL_TRIM_AOUT_13_16:	// MUX
    case CONTROL_SOURCEA:	// MUX
    case CONTROL_SOURCEB:	// MUX
    case CONTROL_DITHER_TYPE:	// MUX
    case CONTROL_PLAY_DITHER:	// MUX
    case CONTROL_RECORD_DITHER:	// MUX
    case CONTROL_MTC_SOURCE:	// MUX
      return (MIXER_CONTROL_TYPE_MUX);

    case CONTROL_OVERLOAD:	// ULONG
    case CONTROL_OVERRUN_COUNT:	// ULONG
    case CONTROL_SAMPLE_COUNT:	// ULONG
    case CONTROL_SAMPLE_FORMAT:	// ULONG
    case CONTROL_CLOCKRATE:	// ULONG
    case CONTROL_CLOCKRATE_SELECT:	// ULONG
    case CONTROL_SYNCHROLOCK_STATUS:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_1:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_2:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_3:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_4:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_5:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_6:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_7:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_8:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_9:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_10:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_11:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_12:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_13:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_14:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_15:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_16:	// ULONG
    case CONTROL_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALIN_RATE:	// ULONG
    case CONTROL_DIGITALIN_STATUS:	// ULONG
    case CONTROL_DIGITALOUT_STATUS:	// ULONG
    case CONTROL_DIGITALIN1_STATUS:	// ULONG
    case CONTROL_DIGITALIN1_RATE:	// ULONG
    case CONTROL_DIGITALIN2_STATUS:	// ULONG
    case CONTROL_DIGITALIN2_RATE:	// ULONG
    case CONTROL_DIGITALIN3_STATUS:	// ULONG
    case CONTROL_DIGITALIN3_RATE:	// ULONG
    case CONTROL_DIGITALIN4_STATUS:	// ULONG
    case CONTROL_DIGITALIN4_RATE:	// ULONG
    case CONTROL_DIGITALIN5_STATUS:	// ULONG
    case CONTROL_DIGITALIN5_RATE:	// ULONG
    case CONTROL_DIGITALIN5_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALIN6_STATUS:	// ULONG
    case CONTROL_DIGITALIN6_RATE:	// ULONG
    case CONTROL_DIGITALIN6_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALIN7_STATUS:	// ULONG
    case CONTROL_DIGITALIN7_RATE:	// ULONG
    case CONTROL_DIGITALIN7_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALIN8_STATUS:	// ULONG
    case CONTROL_DIGITALIN8_RATE:	// ULONG
    case CONTROL_DIGITALIN8_SRC_RATIO:	// ULONG
    case CONTROL_DIO1_SRC_RATIO:	// ULONG
    case CONTROL_DIO2_SRC_RATIO:	// ULONG
    case CONTROL_DIO3_SRC_RATIO:	// ULONG
    case CONTROL_DIO4_SRC_RATIO:	// ULONG
    case CONTROL_DIO5_SRC_RATIO:	// ULONG
    case CONTROL_DIO6_SRC_RATIO:	// ULONG
    case CONTROL_DIO7_SRC_RATIO:	// ULONG
    case CONTROL_DIO8_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALOUT1_STATUS:	// ULONG
    case CONTROL_DIGITALOUT2_STATUS:	// ULONG
    case CONTROL_DIGITALOUT3_STATUS:	// ULONG
    case CONTROL_DIGITALOUT4_STATUS:	// ULONG
    case CONTROL_DIGITALOUT5_STATUS:	// ULONG
    case CONTROL_DIGITALOUT6_STATUS:	// ULONG
    case CONTROL_DIGITALOUT7_STATUS:	// ULONG
    case CONTROL_DIGITALOUT8_STATUS:	// ULONG
    case CONTROL_AES50_STATUS:	// ULONG
    case CONTROL_LTCIN_FRAMERATE:	// ULONG
    case CONTROL_LTCIN_POSITION:	// ULONG
    case CONTROL_LTCOUT_POSITION:	// ULONG
    case CONTROL_LS1_DEVICEID:	// ULONG
    case CONTROL_LS1_PCBREV:	// ULONG
    case CONTROL_LS1_FIRMWAREREV:	// ULONG
    case CONTROL_LS1_ADAT_POSITION:	// ULONG
    case CONTROL_LS1_ADAT_CUEPOINT:	// ULONG
    case CONTROL_LS1_DI1_RATE:	// ULONG
    case CONTROL_LS1_DI1_SRC_RATIO:	// ULONG
    case CONTROL_LS1_DI1_STATUS:	// ULONG
    case CONTROL_LS1_DO1_STATUS:	// ULONG
    case CONTROL_LS1_DI2_RATE:	// ULONG
    case CONTROL_LS1_DI2_SRC_RATIO:	// ULONG
    case CONTROL_LS1_DI2_STATUS:	// ULONG
    case CONTROL_LS1_DO2_STATUS:	// ULONG
    case CONTROL_LS1_DI3_RATE:	// ULONG
    case CONTROL_LS1_DI3_SRC_RATIO:	// ULONG
    case CONTROL_LS1_DI3_STATUS:	// ULONG
    case CONTROL_LS1_DO3_STATUS:	// ULONG
    case CONTROL_LS1_DI4_RATE:	// ULONG
    case CONTROL_LS1_DI4_SRC_RATIO:	// ULONG
    case CONTROL_LS1_DI4_STATUS:	// ULONG
    case CONTROL_LS1_DO4_STATUS:	// ULONG
    case CONTROL_LS2_DEVICEID:	// ULONG
    case CONTROL_LS2_PCBREV:	// ULONG
    case CONTROL_LS2_FIRMWAREREV:	// ULONG
    case CONTROL_LS2_ADAT_POSITION:	// ULONG
    case CONTROL_LS2_ADAT_CUEPOINT:	// ULONG
    case CONTROL_LS2_DI1_RATE:	// ULONG
    case CONTROL_LS2_DI1_SRC_RATIO:	// ULONG
    case CONTROL_LS2_DI1_STATUS:	// ULONG
    case CONTROL_LS2_DO1_STATUS:	// ULONG
    case CONTROL_LS2_DI2_RATE:	// ULONG
    case CONTROL_LS2_DI2_SRC_RATIO:	// ULONG
    case CONTROL_LS2_DI2_STATUS:	// ULONG
    case CONTROL_LS2_DO2_STATUS:	// ULONG
    case CONTROL_LS2_DI3_RATE:	// ULONG
    case CONTROL_LS2_DI3_SRC_RATIO:	// ULONG
    case CONTROL_LS2_DI3_STATUS:	// ULONG
    case CONTROL_LS2_DO3_STATUS:	// ULONG
    case CONTROL_LS2_DI4_RATE:	// ULONG
    case CONTROL_LS2_DI4_SRC_RATIO:	// ULONG
    case CONTROL_LS2_DI4_STATUS:	// ULONG
    case CONTROL_LS2_DO4_STATUS:	// ULONG
    case CONTROL_VOLUMEA:	// ULONG
    case CONTROL_VOLUMEB:	// ULONG
    case CONTROL_DEVICEID:	// ULONG
    case CONTROL_PCBREV:	// ULONG
    case CONTROL_FIRMWAREREV:	// ULONG
    case CONTROL_FIRMWAREDATE:	// ULONG
    case CONTROL_MINSOFTWAREREV:	// ULONG
    case CONTROL_SERIALNUMBER:	// ULONG
    case CONTROL_MFGDATE:	// ULONG
    case CONTROL_DRVBUFSIZE_1X:	// ULONG
    case CONTROL_DRVBUFSIZE_2X:	// ULONG
    case CONTROL_DRVBUFSIZE_4X:	// ULONG
    case CONTROL_DRVNUMRECDEVS_1X:	// ULONG
    case CONTROL_DRVNUMRECDEVS_2X:	// ULONG
    case CONTROL_DRVNUMRECDEVS_4X:	// ULONG
    case CONTROL_DRVNUMPLAYDEVS_1X:	// ULONG
    case CONTROL_DRVNUMPLAYDEVS_2X:	// ULONG
    case CONTROL_DRVNUMPLAYDEVS_4X:	// ULONG
    case CONTROL_FAST_SHARED_CONTROLS:	// ULONG
    case CONTROL_SLOW_SHARED_CONTROLS:	// ULONG
    case CONTROL_MIXER_RESTORE_SCENE:	// ULONG
    case CONTROL_MIXER_SAVE_SCENE:	// ULONG
      return (MIXER_CONTROL_TYPE_UNSIGNED);
    default:
      cmn_err (CE_WARN, "CHalMixer::GetControlType: Unknown Control %d\n",
	       usControl);
      break;
    }

  return (MIXER_CONTROL_TYPE_UNSIGNED);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalMixer::IsControlWriteable (USHORT usControl)
// Returns FALSE for controls that should not be saved in a scene
/////////////////////////////////////////////////////////////////////////////
{
  switch (usControl)
    {
    case CONTROL_OVERLOAD:	// BOOLEAN
    case CONTROL_PEAKMETER:	// ULONG
    case CONTROL_OVERRUN_COUNT:	// ULONG
    case CONTROL_SAMPLE_COUNT:	// ULONG
    case CONTROL_SAMPLE_FORMAT:	// ULONG
    case CONTROL_CLOCKSOURCE:	// MUX
    case CONTROL_CLOCKRATE:	// ULONG
    case CONTROL_DIGITALIN_RATE:	// ULONG
    case CONTROL_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALIN_STATUS:	// ULONG

      // AES16
    case CONTROL_SYNCHROLOCK_STATUS:	// ULONG
    case CONTROL_DIGITALIN1_STATUS:	// ULONG
    case CONTROL_DIGITALIN1_RATE:	// ULONG
    case CONTROL_DIGITALIN2_STATUS:	// ULONG
    case CONTROL_DIGITALIN2_RATE:	// ULONG
    case CONTROL_DIGITALIN3_STATUS:	// ULONG
    case CONTROL_DIGITALIN3_RATE:	// ULONG
    case CONTROL_DIGITALIN4_STATUS:	// ULONG
    case CONTROL_DIGITALIN4_RATE:	// ULONG
    case CONTROL_DIGITALIN5_STATUS:	// ULONG
    case CONTROL_DIGITALIN5_RATE:	// ULONG
    case CONTROL_DIGITALIN5_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALIN6_STATUS:	// ULONG
    case CONTROL_DIGITALIN6_RATE:	// ULONG
    case CONTROL_DIGITALIN6_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALIN7_STATUS:	// ULONG
    case CONTROL_DIGITALIN7_RATE:	// ULONG
    case CONTROL_DIGITALIN7_SRC_RATIO:	// ULONG
    case CONTROL_DIGITALIN8_STATUS:	// ULONG
    case CONTROL_DIGITALIN8_RATE:	// ULONG
    case CONTROL_DIGITALIN8_SRC_RATIO:	// ULONG

      // AES16e
    case CONTROL_DIO1_SRC_RATIO:	// ULONG
    case CONTROL_DIO2_SRC_RATIO:	// ULONG
    case CONTROL_DIO3_SRC_RATIO:	// ULONG
    case CONTROL_DIO4_SRC_RATIO:	// ULONG
    case CONTROL_DIO5_SRC_RATIO:	// ULONG
    case CONTROL_DIO6_SRC_RATIO:	// ULONG
    case CONTROL_DIO7_SRC_RATIO:	// ULONG
    case CONTROL_DIO8_SRC_RATIO:	// ULONG
    case CONTROL_AES50_STATUS:	// ULONG

    case CONTROL_FREQUENCY_COUNTER_1:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_2:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_3:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_4:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_5:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_6:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_7:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_8:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_9:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_10:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_11:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_12:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_13:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_14:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_15:	// ULONG
    case CONTROL_FREQUENCY_COUNTER_16:	// ULONG
    case CONTROL_SYNCIN_NTSC:	// BOOLEAN
    case CONTROL_LTCIN_LOCKED:	// BOOLEAN
    case CONTROL_LTCIN_DIRECTION:	// BOOLEAN
    case CONTROL_LTCIN_DROPFRAME:	// BOOLEAN
    case CONTROL_LTCIN_FRAMERATE:	// ULONG
    case CONTROL_LTCIN_POSITION:	// ULONG

    case CONTROL_LS1_DEVICEID:	// ULONG
    case CONTROL_LS1_PCBREV:	// ULONG
    case CONTROL_LS1_FIRMWAREREV:	// ULONG
    case CONTROL_LS1_ADAT_IN1_LOCK:	// BOOLEAN
    case CONTROL_LS1_ADAT_IN2_LOCK:	// BOOLEAN
    case CONTROL_LS1_ADAT_POSITION:
    case CONTROL_LS1_ADAT_CUEPOINT_ENABLE:

    case CONTROL_LS1_DI1_RATE:	// ULONG
    case CONTROL_LS1_DI1_SRC_RATIO:	// ULONG
    case CONTROL_LS1_DI1_STATUS:	// ULONG
    case CONTROL_LS1_DI2_RATE:	// ULONG
    case CONTROL_LS1_DI2_SRC_RATIO:	// ULONG
    case CONTROL_LS1_DI2_STATUS:	// ULONG
    case CONTROL_LS1_DI3_RATE:	// ULONG
    case CONTROL_LS1_DI3_SRC_RATIO:	// ULONG
    case CONTROL_LS1_DI3_STATUS:	// ULONG
    case CONTROL_LS1_DI4_RATE:	// ULONG
    case CONTROL_LS1_DI4_SRC_RATIO:	// ULONG
    case CONTROL_LS1_DI4_STATUS:	// ULONG

    case CONTROL_LS2_DEVICEID:	// ULONG
    case CONTROL_LS2_PCBREV:	// ULONG
    case CONTROL_LS2_FIRMWAREREV:	// ULONG
    case CONTROL_LS2_ADAT_IN1_LOCK:	// BOOLEAN
    case CONTROL_LS2_ADAT_IN2_LOCK:	// BOOLEAN
    case CONTROL_LS2_ADAT_POSITION:
    case CONTROL_LS2_ADAT_CUEPOINT_ENABLE:

    case CONTROL_LS2_DI1_RATE:	// ULONG
    case CONTROL_LS2_DI1_SRC_RATIO:	// ULONG
    case CONTROL_LS2_DI1_STATUS:	// ULONG
    case CONTROL_LS2_DI2_RATE:	// ULONG
    case CONTROL_LS2_DI2_SRC_RATIO:	// ULONG
    case CONTROL_LS2_DI2_STATUS:	// ULONG
    case CONTROL_LS2_DI3_RATE:	// ULONG
    case CONTROL_LS2_DI3_SRC_RATIO:	// ULONG
    case CONTROL_LS2_DI3_STATUS:	// ULONG
    case CONTROL_LS2_DI4_RATE:	// ULONG
    case CONTROL_LS2_DI4_SRC_RATIO:	// ULONG
    case CONTROL_LS2_DI4_STATUS:	// ULONG

    case CONTROL_ADDA_RECALIBRATE:	// BOOLEAN
    case CONTROL_AD_RECALIBRATE:	// BOOLEAN
    case CONTROL_DA_RECALIBRATE:	// BOOLEAN
    case CONTROL_DEVICEID:	// ULONG
    case CONTROL_PCBREV:	// ULONG
    case CONTROL_FIRMWAREREV:	// ULONG
    case CONTROL_FIRMWAREDATE:	// ULONG
    case CONTROL_MINSOFTWAREREV:	// ULONG
    case CONTROL_SERIALNUMBER:	// ULONG
    case CONTROL_MFGDATE:	// ULONG
    case CONTROL_DITHER_DEPTH:	// MUX
    case CONTROL_FAST_SHARED_CONTROLS:
    case CONTROL_SLOW_SHARED_CONTROLS:
    case CONTROL_MIXER_RESTORE_SCENE:
    case CONTROL_MIXER_SAVE_SCENE:
      return (FALSE);
    }

  return (TRUE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalMixer::SetControl (USHORT usDstLine, USHORT usSrcLine, USHORT usControl,
			 USHORT usChannel, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usCh, usDst, usSrc;

  if (!m_bOpen)
    return (HSTATUS_ADAPTER_NOT_OPEN);

  //cmn_err(CE_WARN,"CHalMixer::SetControl Dst %u Src %u Ctl %u V[%08lx]\n", usDstLine, usSrcLine, usControl, (ULONG)ulValue );

  if (m_bMixerLock)
    {
      switch (usControl)
	{
	  // allow these controls through no matter what
	case CONTROL_MIXER_LOCK:
	case CONTROL_MIXER_RESTORE_SCENE:
	case CONTROL_MIXER_SAVE_SCENE:
	  break;
	default:
	  return (HSTATUS_MIXER_LOCKED);
	}
    }

  // handle the play mix controls first
  if (usDstLine < m_ulNumOutputs)
    {
      USHORT usStatus = HSTATUS_OK;

      if (m_bHasPMixV3)
	usStatus =
	  m_aPlayMixV3[usDstLine].SetMixerControl (usSrcLine, usControl,
						   usChannel, ulValue);
      else if (m_bHasNewPMix)
	usStatus =
	  m_aPlayMixNew[usDstLine].SetMixerControl (usSrcLine, usControl,
						    ulValue);
      else
	usStatus =
	  m_aPlayMix[usDstLine].SetMixerControl (usSrcLine, usControl,
						 ulValue);

      if (usStatus != HSTATUS_SERVICE_NOT_REQUIRED)
	return (usStatus);
    }

  switch (usDstLine)
    {
    case LINE_ADAPTER:
      // only allow NO_SOURCE thru...
      if (usSrcLine != LINE_NO_SOURCE)
	return (HSTATUS_INVALID_MIXER_LINE);

      switch (usControl)
	{
	  // Sample Clock Controls
	case CONTROL_CLOCKSOURCE_PREFERRED:	// MUX
	case CONTROL_CLOCKRATE_SELECT:	// ULONG
	case CONTROL_CLOCKRATE_LOCK:	// BOOLEAN
	case CONTROL_CLOCK_CHANGE_IF_ACTIVE:	// BOOLEAN
	case CONTROL_WIDEWIREIN:	// BOOLEAN
	case CONTROL_WIDEWIREOUT:	// BOOLEAN
	case CONTROL_SYNCHROLOCK_ENABLE:	// BOOLEAN
	  return (m_pSampleClock->SetMixerControl (usControl, ulValue));

	  // CS8420 Digital I/O
	case CONTROL_DIGITAL_FORMAT:
	case CONTROL_SRC_MODE:
	case CONTROL_DIGITALIN_MUTE_ON_ERROR:
	case CONTROL_DIGITALOUT_STATUS:
	case CONTROL_RESET_DIGITALIO_LOCK:
	  if (!m_pCS8420)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  return (m_pCS8420->SetMixerControl (usControl, ulValue));

	  // AES16 / AES16e Digital I/O
	case CONTROL_DIGITALIN5_SRC_ENABLE:	// BOOLEAN (Note: The AES16e code will return INVALID_MIXER_CONTROL for these)
	case CONTROL_DIGITALIN6_SRC_ENABLE:	// BOOLEAN
	case CONTROL_DIGITALIN7_SRC_ENABLE:	// BOOLEAN
	case CONTROL_DIGITALIN8_SRC_ENABLE:	// BOOLEAN
	case CONTROL_DIGITALIN_SRC_MATCHPHASE:	// BOOLEAN
	case CONTROL_DIO1_SRC_MODE:	// MUX (Note: The AK4114 code will return INVALID_MIXER_CONTROL for these)
	case CONTROL_DIO2_SRC_MODE:	// MUX
	case CONTROL_DIO3_SRC_MODE:	// MUX
	case CONTROL_DIO4_SRC_MODE:	// MUX
	case CONTROL_DIO5_SRC_MODE:	// MUX
	case CONTROL_DIO6_SRC_MODE:	// MUX
	case CONTROL_DIO7_SRC_MODE:	// MUX
	case CONTROL_DIO8_SRC_MODE:	// MUX
	case CONTROL_DIGITALOUT1_STATUS:	// ULONG (Both AES16 and AES16e have these)
	case CONTROL_DIGITALOUT2_STATUS:	// ULONG
	case CONTROL_DIGITALOUT3_STATUS:	// ULONG
	case CONTROL_DIGITALOUT4_STATUS:	// ULONG
	case CONTROL_DIGITALOUT5_STATUS:	// ULONG
	case CONTROL_DIGITALOUT6_STATUS:	// ULONG
	case CONTROL_DIGITALOUT7_STATUS:	// ULONG
	case CONTROL_DIGITALOUT8_STATUS:	// ULONG
	case CONTROL_AES50_STATUS:	// ULONG
	  if (m_pAK4114)
	    return (m_pAK4114->SetMixerControl (usControl, ulValue));
	  if (m_pAES16eDIO)
	    return (m_pAES16eDIO->SetMixerControl (usControl, ulValue));
	  return (HSTATUS_INVALID_MIXER_CONTROL);

	case CONTROL_AIN12_TRIM:
	case CONTROL_AIN34_TRIM:
	case CONTROL_AIN56_TRIM:
	case CONTROL_AOUT12_TRIM:
	case CONTROL_AOUT34_TRIM:
	case CONTROL_AOUT56_TRIM:
	  m_pHalAdapter->SetTrim (usControl, ulValue);
	  break;

	  // Timecode
	case CONTROL_LTCOUT_ENABLE:	// BOOLEAN
	case CONTROL_LTCOUT_FRAMERATE:	// MUX
	case CONTROL_LTCOUT_DROPFRAME:	// BOOLEAN
	case CONTROL_LTCOUT_SYNCSOURCE:	// MUX
	case CONTROL_LTCOUT_POSITION:	// ULONG
	  if (!m_pTCTx)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  m_pTCTx->SetMixerControl (usControl, ulValue);
	  break;

	case CONTROL_MTC_SOURCE:
	  m_pHalAdapter->SetMTCSource (ulValue);
	  break;

	case CONTROL_ADDA_RECALIBRATE:	// BOOLEAN
	  m_pHalAdapter->CalibrateConverters ();
	  break;
	case CONTROL_AD_RECALIBRATE:	// BOOLEAN
	  m_pHalAdapter->CalibrateADConverters ();
	  break;
	case CONTROL_DA_RECALIBRATE:	// BOOLEAN
	  m_pHalAdapter->CalibrateDAConverters ();
	  break;
	case CONTROL_AUTO_RECALIBRATE:	// BOOLEAN
	  m_pHalAdapter->SetAutoRecalibrate ((BOOLEAN) ulValue);
	  break;

	case CONTROL_DITHER_TYPE:	// MUX
	  m_pHalAdapter->SetDitherType (m_aRecordMix[0].GetMixControl (),
					(USHORT) ulValue);
	  break;

	case CONTROL_AIN12_HPF:	// BOOLEAN
	case CONTROL_AIN34_HPF:	// BOOLEAN
	case CONTROL_AIN56_HPF:	// BOOLEAN
	  m_pHalAdapter->SetADHPF (usControl, (BOOLEAN) ulValue);
	  break;
	case CONTROL_DA_DEEMPHASIS:
	  m_pHalAdapter->SetDADeEmphasis ((BOOLEAN) ulValue);
	  break;
	case CONTROL_SYNCSTART:	// BOOLEAN
	  m_pHalAdapter->SetSyncStartState ((BOOLEAN) ulValue);
	  break;

	case CONTROL_LSTREAM_DUAL_INTERNAL:	// BOOLEAN
	  m_pLStream->SetLStreamDualInternal (ulValue);
	  break;

	case CONTROL_MRM_ENABLE:
	  m_pHalAdapter->SetMRMEnable (ulValue);
	  break;

	case CONTROL_GPOUT:	// BOOLEAN
	  m_pLStream->SetGPOut (ulValue);
	  break;

	case CONTROL_MIXER_SAVE_SCENE:
	  if (ulValue)
	    {
	      cmn_err (CE_WARN, "CONTROL_MIXER_SAVE_SCENE\n");
	      if (m_pSaveSceneCallback)
		m_pSaveSceneCallback (m_pContext, ulValue);
	    }
	  else			// scene 0 isn't writable
	    {
	      return (HSTATUS_INVALID_MIXER_VALUE);
	    }
	  break;
	case CONTROL_MIXER_RESTORE_SCENE:
	  m_bMixerLock = FALSE;	// must have mixer lock off before calling this function

	  if (ulValue)
	    {
	      cmn_err (CE_WARN, "CONTROL_MIXER_RESTORE_SCENE\n");
	      if (m_pRestoreSceneCallback)
		m_pRestoreSceneCallback (m_pContext, ulValue);
	    }
	  else
	    {
	      SetDefaults ();
	    }
	  break;

	case CONTROL_DRVBUFSIZE_4X:	// ULONG
	  m_ulDriverVariables[usControl - CONTROL_DRVBUFSIZE_1X] = ulValue;
	  break;

	case CONTROL_DRVBUFSIZE_1X:	// ULONG
	case CONTROL_DRVBUFSIZE_2X:	// ULONG
	  //case CONTROL_DRVBUFSIZE_4X:   // ULONG
	case CONTROL_DRVNUMRECDEVS_1X:	// ULONG
	case CONTROL_DRVNUMRECDEVS_2X:	// ULONG
	case CONTROL_DRVNUMRECDEVS_4X:	// ULONG
	case CONTROL_DRVNUMPLAYDEVS_1X:	// ULONG
	case CONTROL_DRVNUMPLAYDEVS_2X:	// ULONG
	case CONTROL_DRVNUMPLAYDEVS_4X:	// ULONG
	  // Only allow changing these if no devices are active
	  // There is a special case: If this is being set by the ASIO driver,
	  // then we need to allow that call to succeed.  We detect this by 
	  // the channel number being 1 instead of 0.
	  if (m_pHalAdapter->GetNumActiveWaveDevices () && (usChannel == 0))
	    return (HSTATUS_INVALID_MODE);

	  //cmn_err(CE_WARN,"Setting Driver Variable %u to %lu\n", usControl, ulValue );
	  m_ulDriverVariables[usControl - CONTROL_DRVBUFSIZE_1X] = ulValue;
	  break;

	case CONTROL_MIXER_LOCK:	// BOOLEAN
	  m_bMixerLock = (BOOLEAN) (ulValue & 0x1);
	  break;

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      break;

      /////////////////////////////////////////////////////////////////////////
    case LINE_LSTREAM:
      /////////////////////////////////////////////////////////////////////////
      // only allow NO_SOURCE thru...
      if (usSrcLine != LINE_NO_SOURCE)
	return (HSTATUS_INVALID_MIXER_LINE);

      if (!m_pLStream)
	return (HSTATUS_INVALID_MIXER_LINE);
      return (m_pLStream->SetMixerControl (usControl, ulValue));

      /////////////////////////////////////////////////////////////////////////
    case LINE_AURORA:
      /////////////////////////////////////////////////////////////////////////
      if (!m_pAurora)
	return (HSTATUS_INVALID_MIXER_LINE);
      return (m_pAurora->SetMixerControl (usSrcLine, usControl, ulValue));

      // Destinations
    case LINE_OUT_1:
      switch (usSrcLine)
	{
	case LINE_NO_SOURCE:	// controls on the destination, PMIX controls are handled above
	  return (HSTATUS_INVALID_MIXER_LINE);
	case LINE_PLAY_0:
	case LINE_PLAY_1:
	case LINE_PLAY_2:
	case LINE_PLAY_3:
	case LINE_PLAY_4:
	case LINE_PLAY_5:
	case LINE_PLAY_6:
	case LINE_PLAY_7:
	case LINE_PLAY_8:
	case LINE_PLAY_9:
	case LINE_PLAY_10:
	case LINE_PLAY_11:
	case LINE_PLAY_12:
	case LINE_PLAY_13:
	case LINE_PLAY_14:
	case LINE_PLAY_15:
	  switch (usControl)
	    {
	    case CONTROL_VOLUME:
	      // a number between 0..15
	      if (!GetFirstMatchingLine
		  (MIXVAL_PMIXSRC_PLAY0L +
		   (((usSrcLine - LINE_PLAY_0) * 2) + usChannel), &usDst,
		   &usSrc))
		{
		  if (m_bHasPMixV3)	// BUGBUG Not Valid
		    return (HSTATUS_INVALID_MIXER_CONTROL);
		  else if (m_bHasNewPMix)
		    m_aPlayMixNew[usDst].SetVolume (usSrc, ulValue);
		  else
		    m_aPlayMix[usDst].SetVolume (usSrc, ulValue);

		  ControlChanged (usDst, usSrc, CONTROL_VOLUME);
		}
	      break;
	    default:
	      return (HSTATUS_INVALID_MIXER_CONTROL);
	    }
	  break;
	default:
	  return (HSTATUS_INVALID_MIXER_LINE);
	}
      break;

    case LINE_RECORD_0:
    case LINE_RECORD_1:
    case LINE_RECORD_2:
    case LINE_RECORD_3:
    case LINE_RECORD_4:
    case LINE_RECORD_5:
    case LINE_RECORD_6:
    case LINE_RECORD_7:
    case LINE_RECORD_8:
    case LINE_RECORD_9:
    case LINE_RECORD_10:
    case LINE_RECORD_11:
    case LINE_RECORD_12:
    case LINE_RECORD_13:
    case LINE_RECORD_14:
    case LINE_RECORD_15:

      usCh = ((usDstLine - LINE_RECORD_0) * 2) + usChannel;

      switch (usSrcLine)
	{
	case LINE_NO_SOURCE:	// controls on the destination
	  switch (usControl)
	    {
	    case CONTROL_SOURCE_LEFT:
	      // restrict LynxTWO-B's source selection 
	      if (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
		if (ulValue >= 2 && ulValue <= 5)
		  return (HSTATUS_INVALID_MIXER_VALUE);
	      m_aRecordMix[usCh + LEFT].SetSource ((USHORT) ulValue);
	      break;
	    case CONTROL_SOURCE_RIGHT:
	      // restrict LynxTWO-B's source selection 
	      if (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
		if (ulValue >= 2 && ulValue <= 5)
		  return (HSTATUS_INVALID_MIXER_VALUE);
	      m_aRecordMix[usCh + RIGHT].SetSource ((USHORT) ulValue);
	      break;
	    case CONTROL_MUTE:
	      m_aRecordMix[usCh].SetMute ((BOOLEAN) ulValue);
	      break;
	    case CONTROL_DITHER:
	      m_aRecordMix[usCh].SetDither ((BOOLEAN) ulValue);
	      break;
	    case CONTROL_DITHER_DEPTH:
	      m_aRecordMix[usCh].SetDitherDepth ((USHORT) ulValue);	// LMixer.exe will only give dither depth controls for the LEFT channel
	      if (((usCh % 2) == 0) && (ulValue < MIXVAL_DITHERDEPTH_COUNT))	// is this the left channel and coming from LMixer?
		m_aRecordMix[usCh + RIGHT].SetDitherDepth ((USHORT) ulValue);	// so we must set the right channel as well
	      break;
	    default:
	      return (HSTATUS_INVALID_MIXER_CONTROL);
	    }
	  break;
	}
      break;

    default:
      return (HSTATUS_INVALID_MIXER_LINE);
    }				// switch( usDstLine )

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalMixer::GetControl (USHORT usDstLine, USHORT usSrcLine, USHORT usControl,
			 USHORT usChannel, PULONG pulValue, ULONG ulSize)
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usDst, usSrc, usCh;

//  cmn_err(CE_WARN,"CHalMixer::GetControl Dst %u Src %u Ctl %u V[%08lx]\n", usDstLine, usSrcLine, usControl, *pulValue );

  if (!pulValue)
    {
      cmn_err (CE_WARN, "CHalMixer::GetControl pulValue is NULL!\n");
      return (HSTATUS_INVALID_PARAMETER);
    }

  // start out with no value
  *pulValue = 0;

  if (!m_bOpen)
    {
      cmn_err (CE_WARN, "CHalMixer::GetControl m_bOpen is FALSE!\n");
      return (HSTATUS_ADAPTER_NOT_OPEN);
    }

  // handle the play mix controls first
  if (usDstLine < m_ulNumOutputs)
    {
      USHORT usStatus = HSTATUS_OK;

      if (m_bHasPMixV3)
	usStatus =
	  m_aPlayMixV3[usDstLine].GetMixerControl (usSrcLine, usControl,
						   usChannel, pulValue);
      else if (m_bHasNewPMix)
	usStatus =
	  m_aPlayMixNew[usDstLine].GetMixerControl (usSrcLine, usControl,
						    pulValue);
      else
	usStatus =
	  m_aPlayMix[usDstLine].GetMixerControl (usSrcLine, usControl,
						 pulValue);

      if (usStatus != HSTATUS_SERVICE_NOT_REQUIRED)
	return (usStatus);
    }

  switch (usDstLine)
    {
    case LINE_ADAPTER:
      // only allow NO_SOURCE thru...
      if (usSrcLine != LINE_NO_SOURCE)
	return (HSTATUS_INVALID_MIXER_LINE);

      switch (usControl)
	{
	case CONTROL_NUMCHANNELS:	// the number of channels this line has
	  *pulValue = 1;
	  break;

	case CONTROL_CLOCKSOURCE:	// MUX
	case CONTROL_CLOCKSOURCE_PREFERRED:	// MUX
	case CONTROL_CLOCKRATE:	// ULONG
	case CONTROL_CLOCKRATE_SELECT:	// ULONG
	case CONTROL_CLOCKRATE_LOCK:	// BOOLEAN
	case CONTROL_CLOCK_CHANGE_IF_ACTIVE:	// BOOLEAN
	case CONTROL_WIDEWIREIN:	// BOOLEAN
	case CONTROL_WIDEWIREOUT:	// BOOLEAN
	case CONTROL_SYNCHROLOCK_ENABLE:	// BOOLEAN
	case CONTROL_SYNCHROLOCK_STATUS:	// ULONG
	  return (m_pSampleClock->GetMixerControl (usControl, pulValue));

	  // LynxTWO Digital I/O
	case CONTROL_DIGITAL_FORMAT:
	case CONTROL_SRC_MODE:
	case CONTROL_SRC_RATIO:
	case CONTROL_DIGITALIN_RATE:
	case CONTROL_DIGITALIN_MUTE_ON_ERROR:
	case CONTROL_DIGITALIN_STATUS:
	case CONTROL_DIGITALOUT_STATUS:
	case CONTROL_RESET_DIGITALIO_LOCK:
	  if (!m_pCS8420)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  return (m_pCS8420->GetMixerControl (usControl, pulValue));

	  // AES16 / AES16e Digital I/O
	case CONTROL_DIGITALIN1_STATUS:	// ULONG
	case CONTROL_DIGITALIN1_RATE:	// ULONG
	case CONTROL_DIGITALIN2_STATUS:	// ULONG
	case CONTROL_DIGITALIN2_RATE:	// ULONG
	case CONTROL_DIGITALIN3_STATUS:	// ULONG
	case CONTROL_DIGITALIN3_RATE:	// ULONG
	case CONTROL_DIGITALIN4_STATUS:	// ULONG
	case CONTROL_DIGITALIN4_RATE:	// ULONG
	case CONTROL_DIGITALIN5_STATUS:	// ULONG
	case CONTROL_DIGITALIN5_RATE:	// ULONG
	case CONTROL_DIGITALIN5_SRC_ENABLE:	// BOOLEAN
	case CONTROL_DIGITALIN5_SRC_RATIO:	// ULONG
	case CONTROL_DIGITALIN6_STATUS:	// ULONG
	case CONTROL_DIGITALIN6_RATE:	// ULONG
	case CONTROL_DIGITALIN6_SRC_ENABLE:	// BOOLEAN
	case CONTROL_DIGITALIN6_SRC_RATIO:	// ULONG
	case CONTROL_DIGITALIN7_STATUS:	// ULONG
	case CONTROL_DIGITALIN7_RATE:	// ULONG
	case CONTROL_DIGITALIN7_SRC_ENABLE:	// BOOLEAN
	case CONTROL_DIGITALIN7_SRC_RATIO:	// ULONG
	case CONTROL_DIGITALIN8_STATUS:	// ULONG
	case CONTROL_DIGITALIN8_RATE:	// ULONG
	case CONTROL_DIGITALIN8_SRC_ENABLE:	// BOOLEAN
	case CONTROL_DIGITALIN8_SRC_RATIO:	// ULONG
	case CONTROL_DIGITALIN_SRC_MATCHPHASE:	// BOOLEAN
	case CONTROL_DIO1_SRC_MODE:	// MUX
	case CONTROL_DIO1_SRC_RATIO:	// ULONG
	case CONTROL_DIO2_SRC_MODE:	// MUX
	case CONTROL_DIO2_SRC_RATIO:	// ULONG
	case CONTROL_DIO3_SRC_MODE:	// MUX
	case CONTROL_DIO3_SRC_RATIO:	// ULONG
	case CONTROL_DIO4_SRC_MODE:	// MUX
	case CONTROL_DIO4_SRC_RATIO:	// ULONG
	case CONTROL_DIO5_SRC_MODE:	// MUX
	case CONTROL_DIO5_SRC_RATIO:	// ULONG
	case CONTROL_DIO6_SRC_MODE:	// MUX
	case CONTROL_DIO6_SRC_RATIO:	// ULONG
	case CONTROL_DIO7_SRC_MODE:	// MUX
	case CONTROL_DIO7_SRC_RATIO:	// ULONG
	case CONTROL_DIO8_SRC_MODE:	// MUX
	case CONTROL_DIO8_SRC_RATIO:	// ULONG
	case CONTROL_DIGITALOUT1_STATUS:	// ULONG
	case CONTROL_DIGITALOUT2_STATUS:	// ULONG
	case CONTROL_DIGITALOUT3_STATUS:	// ULONG
	case CONTROL_DIGITALOUT4_STATUS:	// ULONG
	case CONTROL_DIGITALOUT5_STATUS:	// ULONG
	case CONTROL_DIGITALOUT6_STATUS:	// ULONG
	case CONTROL_DIGITALOUT7_STATUS:	// ULONG
	case CONTROL_DIGITALOUT8_STATUS:	// ULONG
	case CONTROL_AES50_STATUS:	// ULONG
	  if (m_pAK4114)
	    return (m_pAK4114->GetMixerControl (usControl, pulValue));
	  if (m_pAES16eDIO)
	    return (m_pAES16eDIO->GetMixerControl (usControl, pulValue));
	  return (HSTATUS_INVALID_MIXER_CONTROL);

	  // Trim
	case CONTROL_AIN12_TRIM:
	case CONTROL_AIN34_TRIM:
	case CONTROL_AIN56_TRIM:
	case CONTROL_AOUT12_TRIM:
	case CONTROL_AOUT34_TRIM:
	case CONTROL_AOUT56_TRIM:
	  return (m_pHalAdapter->GetTrim (usControl, pulValue));

	  // Frequency Counters
	case CONTROL_FREQUENCY_COUNTER_1:
	case CONTROL_FREQUENCY_COUNTER_2:
	case CONTROL_FREQUENCY_COUNTER_3:
	case CONTROL_FREQUENCY_COUNTER_4:
	case CONTROL_FREQUENCY_COUNTER_5:
	case CONTROL_FREQUENCY_COUNTER_6:
	case CONTROL_FREQUENCY_COUNTER_7:
	case CONTROL_FREQUENCY_COUNTER_8:
	case CONTROL_FREQUENCY_COUNTER_9:
	case CONTROL_FREQUENCY_COUNTER_10:
	case CONTROL_FREQUENCY_COUNTER_11:
	case CONTROL_FREQUENCY_COUNTER_12:
	case CONTROL_FREQUENCY_COUNTER_13:
	case CONTROL_FREQUENCY_COUNTER_14:
	case CONTROL_FREQUENCY_COUNTER_15:
	case CONTROL_FREQUENCY_COUNTER_16:
	  return (m_pHalAdapter->GetFrequencyCounter (usControl -
						      CONTROL_FREQUENCY_COUNTER_1,
						      pulValue));

	case CONTROL_SYNCIN_NTSC:
	  // only cards with an LTC Receiver have this control
	  if (!m_pTCRx)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  *pulValue = m_pHalAdapter->GetNTSCPAL ();
	  break;

	  // LTC In
	case CONTROL_LTCIN_FRAMERATE:	// ulong
	case CONTROL_LTCIN_LOCKED:	// boolean
	case CONTROL_LTCIN_DIRECTION:	// boolean
	case CONTROL_LTCIN_DROPFRAME:	// boolean
	case CONTROL_LTCIN_POSITION:	// ulong
	  if (!m_pTCRx)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  return (m_pTCRx->GetMixerControl (usControl, pulValue));

	  // LTC Out
	case CONTROL_LTCOUT_ENABLE:	// boolean
	case CONTROL_LTCOUT_FRAMERATE:	// mux
	case CONTROL_LTCOUT_DROPFRAME:	// boolean
	case CONTROL_LTCOUT_SYNCSOURCE:	// MUX
	case CONTROL_LTCOUT_POSITION:	// ulong
	  if (!m_pTCTx)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  return (m_pTCTx->GetMixerControl (usControl, pulValue));

	  // Misc Controls
	case CONTROL_MTC_SOURCE:
	  return (m_pHalAdapter->GetMTCSource (pulValue));

	case CONTROL_ADDA_RECALIBRATE:	// BOOLEAN (Write Only)
	case CONTROL_AD_RECALIBRATE:	// BOOLEAN (Write Only)
	case CONTROL_DA_RECALIBRATE:	// BOOLEAN (Write Only)
	  if (m_bIsAES16 || m_bIsAES16e)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  break;
	case CONTROL_AUTO_RECALIBRATE:	// BOOLEAN
	  if (m_bIsAES16 || m_bIsAES16e)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  *pulValue = m_pHalAdapter->GetAutoRecalibrate ();
	  break;

	case CONTROL_DITHER_TYPE:	// MUX
	  *pulValue = m_pHalAdapter->GetDitherType ();
	  break;

	case CONTROL_AIN12_HPF:	// BOOLEAN
	  if (!
	      ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
	       || (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
	       || (m_usDeviceID == PCIDEVICE_LYNXTWO_C)
	       || (m_usDeviceID == PCIDEVICE_LYNX_L22)))
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  *pulValue = m_pHalAdapter->GetADHPF (usControl);
	  break;
	case CONTROL_AIN34_HPF:	// BOOLEAN
	  if (!
	      ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
	       || (m_usDeviceID == PCIDEVICE_LYNXTWO_C)))
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  *pulValue = m_pHalAdapter->GetADHPF (usControl);
	  break;
	case CONTROL_AIN56_HPF:	// BOOLEAN
	  // only LynxTWO-C gets this
	  if (m_usDeviceID != PCIDEVICE_LYNXTWO_C)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  *pulValue = m_pHalAdapter->GetADHPF (usControl);
	  break;
	case CONTROL_DA_DEEMPHASIS:
	  if (m_bIsAES16 || m_bIsAES16e)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  *pulValue = m_pHalAdapter->GetDADeEmphasis ();
	  break;
	case CONTROL_SYNCSTART:	// BOOLEAN
	  *pulValue = m_pHalAdapter->GetSyncStartState ();
	  break;
	case CONTROL_LSTREAM_DUAL_INTERNAL:	// BOOLEAN
	  // only a single LStream port on the AES16
	  if (m_bIsAES16 || m_bIsAES16e)
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	  *pulValue = m_pLStream->GetLStreamDualInternal ();
	  break;

	case CONTROL_MRM_ENABLE:
	  return (m_pHalAdapter->GetMRMEnable (pulValue));

	case CONTROL_GPOUT:	// BOOLEAN
	  *pulValue = m_pLStream->GetGPOut ();
	  break;

	case CONTROL_DRVBUFSIZE_1X:	// ULONG
	case CONTROL_DRVBUFSIZE_2X:	// ULONG
	case CONTROL_DRVBUFSIZE_4X:	// ULONG
	case CONTROL_DRVNUMRECDEVS_1X:	// ULONG
	case CONTROL_DRVNUMRECDEVS_2X:	// ULONG
	case CONTROL_DRVNUMRECDEVS_4X:	// ULONG
	case CONTROL_DRVNUMPLAYDEVS_1X:	// ULONG
	case CONTROL_DRVNUMPLAYDEVS_2X:	// ULONG
	case CONTROL_DRVNUMPLAYDEVS_4X:	// ULONG
	  //cmn_err(CE_WARN,"Driver Variable %u is %lu\n", usControl, m_ulDriverVariables[ usControl - CONTROL_DRVBUFSIZE_1X ] );
	  *pulValue = m_ulDriverVariables[usControl - CONTROL_DRVBUFSIZE_1X];
	  break;

	case CONTROL_DEVICEID:	// ULONG
	  *pulValue = m_usDeviceID;
	  break;
	case CONTROL_PCBREV:	// ULONG
	  *pulValue = m_pHalAdapter->GetPCBRev ();
	  break;
	case CONTROL_FIRMWAREREV:	// ULONG
	  *pulValue = m_pHalAdapter->GetFirmwareRev ();
	  break;
	case CONTROL_FIRMWAREDATE:	// ULONG
	  *pulValue = m_pHalAdapter->GetFirmwareDate ();
	  break;
	case CONTROL_SERIALNUMBER:	// ULONG
	  *pulValue = m_pHalAdapter->GetSerialNumber ();
	  break;
	case CONTROL_FAST_SHARED_CONTROLS:
	case CONTROL_SLOW_SHARED_CONTROLS:
	  if (ulSize == sizeof (DWORD))
	    return (HSTATUS_OK);
	  if (ulSize != sizeof (SHAREDCONTROLS))
	    {
	      cmn_err (CE_WARN, "SHAREDCONTROLS structure incorrect size!\n");
	      return (HSTATUS_INVALID_PARAMETER);
	    }
	  GetSharedControls (usControl, (PSHAREDCONTROLS) pulValue);
	  break;
	case CONTROL_MIXER_RESTORE_SCENE:
	case CONTROL_MIXER_SAVE_SCENE:
	  *pulValue = 0;	// nothing to return here - scene numbers aren't persistant
	  break;
	case CONTROL_MIXER_LOCK:	// BOOLEAN
	  *pulValue = m_bMixerLock;
	  break;
	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);

	}
      break;

      /////////////////////////////////////////////////////////////////////////
    case LINE_LSTREAM:
      /////////////////////////////////////////////////////////////////////////
      // only allow NO_SOURCE thru...
      if (usSrcLine != LINE_NO_SOURCE)
	return (HSTATUS_INVALID_MIXER_LINE);

      if (!m_pLStream)
	return (HSTATUS_INVALID_MIXER_LINE);
      return (m_pLStream->GetMixerControl (usControl, pulValue));

      /////////////////////////////////////////////////////////////////////////
    case LINE_AURORA:
      /////////////////////////////////////////////////////////////////////////
      if (!m_pAurora)
	return (HSTATUS_INVALID_MIXER_LINE);
      return (m_pAurora->GetMixerControl
	      (usSrcLine, usControl, pulValue, ulSize));

      // Destinations
    case LINE_OUT_1:
    case LINE_OUT_2:
      switch (usSrcLine)
	{
	case LINE_NO_SOURCE:	// controls on the destination, PMIX controls are handled above
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	case LINE_PLAY_0:
	case LINE_PLAY_1:
	case LINE_PLAY_2:
	case LINE_PLAY_3:
	case LINE_PLAY_4:
	case LINE_PLAY_5:
	case LINE_PLAY_6:
	case LINE_PLAY_7:
	case LINE_PLAY_8:
	case LINE_PLAY_9:
	case LINE_PLAY_10:
	case LINE_PLAY_11:
	case LINE_PLAY_12:
	case LINE_PLAY_13:
	case LINE_PLAY_14:
	case LINE_PLAY_15:
	  // only allow LINE_OUT_1 to have these source lines
	  if (usDstLine != LINE_OUT_1)
	    return (HSTATUS_INVALID_MIXER_LINE);

	  // make sure we only allow play devices that exist on this adapter
	  if (usSrcLine >= (LINE_PLAY_0 + (m_ulNumOutputs / 2)))
	    return (HSTATUS_INVALID_MIXER_LINE);

	  switch (usControl)
	    {
	    case CONTROL_NUMCHANNELS:	// the number of channels this line has
	      *pulValue = 2;
	      break;
	    case CONTROL_PEAKMETER:
	      // a number between 0..15
	      if (!GetFirstMatchingLine
		  (MIXVAL_PMIXSRC_PLAY0L +
		   (((usSrcLine - LINE_PLAY_0) * 2) + usChannel), &usDst,
		   &usSrc))
		{
		  // level returned is 0..7FFFF (19 bits), shift to 15 bits
		  if (m_bHasPMixV3)
		    {
		      *pulValue = (m_aPlayMixV3[usDst].GetLevel () >> 4);
		    }
		  else if (m_bHasNewPMix)
		    {
		      *pulValue = (m_aPlayMixNew[usDst].GetLevel () >> 4);
		      m_aPlayMixNew[usDst].ResetLevel ();
		    }
		  else
		    {
		      *pulValue = (m_aPlayMix[usDst].GetLevel () >> 4);
		      m_aPlayMix[usDst].ResetLevel ();
		    }
		}
	      break;

	    case CONTROL_VOLUME:
	      // a number between 0..15
	      if (!GetFirstMatchingLine
		  (MIXVAL_PMIXSRC_PLAY0L +
		   (((usSrcLine - LINE_PLAY_0) * 2) + usChannel), &usDst,
		   &usSrc))
		{
		  if (m_bHasPMixV3)
		    return (HSTATUS_INVALID_MIXER_CONTROL);
		  else if (m_bHasNewPMix)
		    *pulValue = m_aPlayMixNew[usDst].GetVolume (usSrc);
		  else
		    *pulValue = m_aPlayMix[usDst].GetVolume (usSrc);
		}
	      break;

	    case CONTROL_SAMPLE_FORMAT:
	      *pulValue =
		m_pHalAdapter->GetWaveDMAPlayDevice (usSrcLine -
						     LINE_PLAY_0)->
		GetSampleFormat ();
	      break;
	    case CONTROL_SAMPLE_COUNT:
	      *pulValue =
		m_pHalAdapter->GetWaveDMAPlayDevice (usSrcLine -
						     LINE_PLAY_0)->GetDMA ()->
		GetPreloadTiming ();
	      break;
	    case CONTROL_OVERRUN_COUNT:
	      *pulValue =
		m_pHalAdapter->GetWaveDMAPlayDevice (usSrcLine -
						     LINE_PLAY_0)->
		GetOverrunCount ();
	      break;
	    default:
	      return (HSTATUS_INVALID_MIXER_CONTROL);
	    }
	  break;
	default:
	  return (HSTATUS_INVALID_MIXER_LINE);
	}
      break;

    case LINE_OUT_3:
    case LINE_OUT_4:
    case LINE_OUT_5:
    case LINE_OUT_6:
    case LINE_OUT_7:
    case LINE_OUT_8:
    case LINE_OUT_9:
    case LINE_OUT_10:
    case LINE_OUT_11:
    case LINE_OUT_12:
    case LINE_OUT_13:
    case LINE_OUT_14:
    case LINE_OUT_15:
    case LINE_OUT_16:
    case LINE_OUT_17:
    case LINE_OUT_18:
    case LINE_OUT_19:
    case LINE_OUT_20:
    case LINE_OUT_21:
    case LINE_OUT_22:
    case LINE_OUT_23:
    case LINE_OUT_24:
    case LINE_OUT_25:
    case LINE_OUT_26:
    case LINE_OUT_27:
    case LINE_OUT_28:
    case LINE_OUT_29:
    case LINE_OUT_30:
    case LINE_OUT_31:
    case LINE_OUT_32:
      if (usDstLine > m_ulNumOutputs)
	return (HSTATUS_INVALID_MIXER_LINE);

      if (usSrcLine == LINE_NO_SOURCE)	// controls on the destination, PMIX controls are handled above
	{
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      else
	return (HSTATUS_INVALID_MIXER_LINE);
      break;

    case LINE_RECORD_0:
    case LINE_RECORD_1:
    case LINE_RECORD_2:
    case LINE_RECORD_3:
    case LINE_RECORD_4:
    case LINE_RECORD_5:
    case LINE_RECORD_6:
    case LINE_RECORD_7:
    case LINE_RECORD_8:
    case LINE_RECORD_9:
    case LINE_RECORD_10:
    case LINE_RECORD_11:
    case LINE_RECORD_12:
    case LINE_RECORD_13:
    case LINE_RECORD_14:
    case LINE_RECORD_15:
      // make sure we only allow record devices that exist on this adapter
      if (usDstLine >= (LINE_RECORD_0 + (m_ulNumInputs / 2)))
	return (HSTATUS_INVALID_MIXER_LINE);

      // a number between 0..15
      usCh = ((usDstLine - LINE_RECORD_0) * 2) + usChannel;

      switch (usSrcLine)
	{
	case LINE_NO_SOURCE:	// controls on the destination
	  switch (usControl)
	    {
	    case CONTROL_NUMCHANNELS:	// the number of channels this line has
	      *pulValue = 2;
	      break;
	    case CONTROL_PEAKMETER:
	      // level returned is 0..7FFFF (19 bits), shift to 15 bits
	      *pulValue = (m_aRecordMix[usCh].GetLevel () >> 4);
	      m_aRecordMix[usCh].ResetLevel ();	// after we have read the level, reset it
	      break;
	    case CONTROL_SOURCE_LEFT:
	      *pulValue = m_aRecordMix[usCh + LEFT].GetSource ();
	      break;
	    case CONTROL_SOURCE_RIGHT:
	      *pulValue = m_aRecordMix[usCh + RIGHT].GetSource ();
	      break;
	    case CONTROL_MUTE:
	      *pulValue = m_aRecordMix[usCh].GetMute ();
	      break;
	    case CONTROL_DITHER:
	      *pulValue = m_aRecordMix[usCh].GetDither ();
	      break;
	    case CONTROL_DITHER_DEPTH:
	      *pulValue = m_aRecordMix[usCh].GetDitherDepth ();
	      break;
	    case CONTROL_SAMPLE_FORMAT:
	      *pulValue =
		m_pHalAdapter->GetWaveDMARecordDevice (usDstLine -
						       LINE_RECORD_0)->
		GetSampleFormat ();
	      break;
	    case CONTROL_SAMPLE_COUNT:
	      *pulValue =
		m_pHalAdapter->GetWaveDMARecordDevice (usDstLine -
						       LINE_RECORD_0)->GetDMA
		()->GetPreloadTiming ();
	      break;
	    case CONTROL_OVERRUN_COUNT:
	      *pulValue =
		m_pHalAdapter->GetWaveDMARecordDevice (usDstLine -
						       LINE_RECORD_0)->
		GetOverrunCount ();
	      break;
	    default:
	      return (HSTATUS_INVALID_MIXER_CONTROL);
	    }
	  break;

	default:
	  return (HSTATUS_INVALID_MIXER_LINE);
	}
      break;

    default:
      return (HSTATUS_INVALID_MIXER_LINE);
    }				// switch( usDstLine )

  return (HSTATUS_OK);
}
