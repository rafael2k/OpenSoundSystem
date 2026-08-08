/****************************************************************************
 Hal8420.cpp

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
 Feb 01 05 DAH	Changed GetSRCRatio to return 0xFFFFFFFF if SRC is OFF
 May 28 03 DAH	GetInputSampleRate now uses a range of sample rates to 
				determine the base rate.
 May 28 03 DAH	Noted that calls to CHalAdapter::IORead may lockup the 
				chipset on beige G3's.
 Sep 05 02 DAH	8420 now no longer generates any interrupts.  D to E 
				transfers are disabled.
****************************************************************************/

#include <StdAfx.h>
#include "HalAdapter.h"

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::Open (PHALADAPTER pHalAdapter)
/////////////////////////////////////////////////////////////////////////////
{
  // save the pointer to the parent adapter object
  m_pHalAdapter = pHalAdapter;

#ifdef USE_RESET_ON_UNLOCK
  m_bResetDigitalIOLock = FALSE;
  m_ulLockState = LOCKSTATE_LOCKED;	// assume we are locked
  m_lInIOActive = 0;
#endif
  m_ucInputErrors = 0;

  //RtlZeroMemory( &m_QSubcode, sizeof( QCHANNELSUBCODE ) );
  RtlZeroMemory (&m_TxCBuffer, sizeof (m_TxCBuffer));

  m_bWriteCUFirstTime = TRUE;	// Force the CU buffer to be completely written the first time through
  m_bMuteOnError = TRUE;
  m_ulSRCMode = MIXVAL_SRCMODE_SRC_ON;
  m_ulFormat = MIXVAL_DF_AESEBU;
  m_ulOutputStatus = MIXVAL_OUTSTATUS_VALID;	// Non-Audio OFF, Emphasis OFF

  InitializeChip ();

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::Close ()
/////////////////////////////////////////////////////////////////////////////
{
  SetSRCMode (MIXVAL_SRCMODE_SRC_ON);
  Write (k8420ClockSourceControl, (BYTE) 0, k8420_CSC_RUN);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::InitializeChip ()
/////////////////////////////////////////////////////////////////////////////
{
  // 1: Set the direction of TCBL to output
  Write (k8420MiscControl1, k8420_MC1_TCBLD);

  // 2: Set RMCk output frequncy to 128 Fsi
  Write (k8420MiscControl2, k8420_MC2_RMCKF | k8420_MC2_HOLD01);	// Mute On Error: ON

  // 3: Data Flow Control
  // Mute on Loss of Lock, Everything else gets set in SetMode
  Write (k8420DataFlowControl, k8420_DFC_AMLL);

  // 4: Put the chip in run mode
  Write (k8420ClockSourceControl, k8420_CSC_RUN);

  // 5: Serial audio input port format: slave, 24-bit, I2S 
  Write (k8420SerialInputFormat, (k8420_SAI_SIDEL | k8420_SAI_SILRPOL));

  // 6: Serial audio output port format: 24-bit, I2S 
  //NOTE: SOMS=1: master mode, OLRCK and OSCLK are outputs,  slave to AES, set DILRCKDIR=1
  //              SOMS=0: slave mode, OLRCK and OSCLK are inputs - clock for AES in derived externally
  Write (k8420SerialOutputFormat, (k8420_SAO_SOLRPOL | k8420_SAO_SODEL));	//SRC, slave mode

  // 7: Interrupt Register 1 Status, Read Only
  // 8: Interrupt Register 2 Status, Read Only

  // 9: Interrupt 1 Register Mask, Enable Receiver Error Interrupt
#ifdef USE_RESET_ON_UNLOCK
  if (m_bResetDigitalIOLock)
    {
      Write (k8420Interrupt1Mask, k8420_Int1_RERR);	// DAH 10/13/2005
      m_ulLockState = LOCKSTATE_UNLOCKED;
    }
  else
#endif
    {
      Write (k8420Interrupt1Mask, 0);	// DAH 10/13/2005
    }
  //Write( k8420Interrupt1Mask, (k8420_Int1_RERR | k8420_Int1_EFTC | k8420_Int1_DETC) );

  // 10&11: Interrupt Register 1 Mode MSB & LSB, Default values OK (Rising edge always!)

  // 12: Interrupt 2 Register Mask, Enable SRC UNLOCK Interrupt
  Write (k8420Interrupt2Mask, 0);	//k8420_Int2_REUNLOCK );        // DAH 07/15/2005
  //Write( k8420Interrupt2Mask, k8420_Int2_QCH );

  // 13&14: Interrupt Register 2 Mode MSB & LSB, Default values OK (Rising edge always!)

  // 15: Receiver Channel Status, Read Only
  // 16: Receiver Error, Read Only

  // 17: setup which errors we are interested in...
  Write (k8420RxErrorMask,
	 (k8420_RxErr_PAR | k8420_RxErr_BIP | k8420_RxErr_CONF |
	  k8420_RxErr_VAL | k8420_RxErr_UNLOCK | k8420_RxErr_CCRC |
	  k8420_RxErr_QCRC));

  // 18: Channel Status Data Buffer Control
  // only interested in DtoE transfers (Receiver), disable EtoF transfers (Transmitter)
  Write (k8420CSDataBufferControl, k8420_CsDB_DETCI, k8420_CsDB_DETCI);	// DAH Sep 05 2002
  //Write( k8420CSDataBufferControl, k8420_CsDB_EFTCI, k8420_CsDB_EFTCI );

  // 19: User Data Buffer Control,
  Write (k8420UDataBufferControl, k8420_UDB_UD);	//set U pin as output

  // 20-29: Q-Channel Subcode Bytes, Read Only
  // 30: Sample Rate Ratio, Read Only
  // 32-55: C or U bit data buffer

  ULONG ulSavedSRCMode = m_ulSRCMode;

  SetSRCMode (MIXVAL_SRCMODE_TXONLY, TRUE);	// Start with Mode 5 so transmitter will always turn on
  SetFormat (m_ulFormat);	// Calls WriteCUBuffer
  SetOutputStatus (m_ulOutputStatus);	// Calls WriteCUBuffer
  SetSRCMode (ulSavedSRCMode, TRUE);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::Write (ULONG ulRegister, BYTE ucValue, BYTE ucMask)
// Write a single byte to a 8420 register
/////////////////////////////////////////////////////////////////////////////
{
  return (m_pHalAdapter->IOWrite ((BYTE) ulRegister, ucValue, ucMask));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::Write (ULONG ulRegister, PBYTE pucValue, ULONG ulSize)
// Write a buffer to the 8420 starting at a specific register
/////////////////////////////////////////////////////////////////////////////
{
  while (ulSize--)
    {
      m_pHalAdapter->IOWrite ((BYTE) ulRegister++, *pucValue++);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::Read (ULONG ulRegister, PBYTE pucValue)
// Read a single byte to from a 8420 register
/////////////////////////////////////////////////////////////////////////////
{
  // this may hang older beige G3's
  return (m_pHalAdapter->IORead ((BYTE) ulRegister, pucValue));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::Read (ULONG ulRegister, PBYTE pucValue, ULONG ulSize)
// only called from ReadCUBuffer which is never used
// Read a buffer from the 8420 starting at a specific register
/////////////////////////////////////////////////////////////////////////////
{
  while (ulSize--)
    {
      m_pHalAdapter->IORead ((BYTE) ulRegister++, pucValue++);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
BYTE
CHal8420::GetInputErrors ()
// private
/////////////////////////////////////////////////////////////////////////////
{
  // NOTE: This may hang older Mac G3 computers!
  Read (k8420RxErrors, &m_ucInputErrors);
  return (m_ucInputErrors);
}

/////////////////////////////////////////////////////////////////////////////
BYTE
CHal8420::GetRxChannelStatus ()
// private
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucStatus;
  // NOTE: This may hang older Mac G3 computers!
  Read (k8420RxChannelStatus, &ucStatus);
  return (ucStatus);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHal8420::IsInputLocked ()
// No longer looks at the input channel status
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulDigitalInRate;

  m_pHalAdapter->GetFrequencyCounter (L2_FREQCOUNTER_DIGITALIN, &ulDigitalInRate);	// this will return 0 if the input is unlocked

  if (ulDigitalInRate)
    return (TRUE);

  return (FALSE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::GetInputStatus (PULONG pulStatus)
// This only called from CHalMixer::GetSharedControls and CHal8420::GetMixerControl
/////////////////////////////////////////////////////////////////////////////
{
#if defined(OSX) || defined(MACINTOSH)
  if (IsInputLocked ())
    *pulStatus = DIS_RXCS_PRO;
  else
    *pulStatus = DIS_ERR_UNLOCK;
#else
  *pulStatus =
    ((ULONG) GetInputErrors () | ((ULONG) GetRxChannelStatus () << 8));
#endif
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::UpdateStatus (BOOLEAN bInISR)
// This function insures that if the CS8420 goes into an invalid state 
// (See CS8420 Rev D Errata # 3) that we can recover from it.
// Return Value never checked.
/////////////////////////////////////////////////////////////////////////////
{
#ifdef USE_RESET_ON_UNLOCK

#if defined(OSX) || defined(MACINTOSH)
#else
  //cmn_err(CE_WARN,"CHal8420::UpdateStatus\n");

  if (m_bResetDigitalIOLock)
    {
      // If we are being called from the ISR, then we can only be in the state UNLOCKED or LOCKED.
      if (bInISR)
	{
	  if (!
	      ((m_ulLockState == LOCKSTATE_UNLOCKED)
	       || (m_ulLockState == LOCKSTATE_LOCKED)))
	    {
	      //cmn_err(CE_WARN,"UpdateStatus NOT UNLOCKED or LOCKED\n");
	      // don't do anything...
	      return (HSTATUS_OK);
	    }
	}

      BOOLEAN bIsInputLocked;

      // DAH 10/13/2005 moved to after check for ISR so this doesn't get called from within the ISR when it isn't needed.
      bIsInputLocked =
	(GetInputErrors () & k8420_RxErr_UNLOCK) ? FALSE : TRUE;
      //if( bIsInputLocked )  DC('L');        else    DC('U');

      switch (m_ulLockState)
	{
	case LOCKSTATE_UNLOCKED:
	  //cmn_err((CE_WARN,"LOCKSTATE_UNLOCKED\n");
	  if (bIsInputLocked)
	    {
	      // we are transitioning from UNLOCK to LOCK

	      //Write( kMisc, (BYTE)0, IO_MISC_CS8420RSTn );                  // put the CS8420 into reset mode
	      //Write( kMisc, IO_MISC_CS8420RSTn, IO_MISC_CS8420RSTn );       // take the CS8420 out of reset mode

	      //Write( k8420ClockSourceControl, (BYTE)0, k8420_CSC_RUN );

	      //cmn_err (CE_WARN, "Set WAITLOCK1\n");
	      m_ulLockState = LOCKSTATE_WAITINGFORLOCK1;
	    }
	  break;

	case LOCKSTATE_WAITINGFORLOCK1:
	  //cmn_err (CE_WARN, "WAITLOCK1\n");
	  // if we are still locked, then toggle the RUN bit
	  if (bIsInputLocked)
	    {
	      //PHALSAMPLECLOCK       pClock = m_pHalAdapter->GetSampleClock();
	      //ULONG ulSavedSRCMode;
	      //LONG  lPreferredSource;

	      //cmn_err (CE_WARN, "[R]");
	      // toggle the run bit
	      m_lInIOActive++;
	      Write (k8420ClockSourceControl, (BYTE) 0, k8420_CSC_RUN);
	      Write (k8420ClockSourceControl, k8420_CSC_RUN, k8420_CSC_RUN);

	      // When going into RUN mode, the transmitter will be dead if the AES receiver isn't locked.
	      // Normally this would not be a problem, but in loopback mode this will keep the transmitter
	      // from ever working.  Doing the SRCMode dance will solve this problem.

	      // The two calls to SetSRCMode will correct the TX error problem, but will blast the 
	      // SRC mode and PreferredClockSource, so we save them off first...
	      //ulSavedSRCMode = m_ulSRCMode;
	      //pClock->GetPreferredSource( &lPreferredSource );

	      // The CS8420 seems to lose the transmit channel status when turning off RUN mode, so we
	      // force a new write.
	      //cmn_err (CE_WARN, "[F]");
	      m_bWriteCUFirstTime = TRUE;
	      SetFormat (m_ulFormat);
	      m_lInIOActive--;

	      //cmn_err (CE_WARN, "Setting WAITLOCK2\n");
	      m_ulLockState = LOCKSTATE_WAITINGFORLOCK2;
	    }
	  else
	    {
	      //cmn_err (CE_WARN, "Reverting to UNLOCKED\n");
	      m_ulLockState = LOCKSTATE_UNLOCKED;
	    }
	  break;

	case LOCKSTATE_WAITINGFORLOCK2:
	  // we may be showing locked or unlocked here, it doesn't matter...
	  //cmn_err (CE_WARN, "Setting LOCKED\n");
	  m_ulLockState = LOCKSTATE_LOCKED;
	  break;

	case LOCKSTATE_LOCKED:
	  //cmn_err((CE_WARN,"LOCKSTATE_LOCKED\n");
	  if (!bIsInputLocked)
	    {
	      // LOCKED to UNLOCK transition
	      //cmn_err (CE_WARN, "Setting UNLOCKED\n");
	      m_ulLockState = LOCKSTATE_UNLOCKED;
	    }
	  break;
	}
    }

  //cmn_err(CE_WARN,"CHal8420::UpdateStatus Done\n");
#endif
#endif

  return (HSTATUS_OK);
}

#ifdef USE_RESET_ON_UNLOCK
/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SetResetDigitalIOUnlock (BOOLEAN bValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_bResetDigitalIOLock = bValue;

  if (m_bResetDigitalIOLock)
    {
      Write (k8420Interrupt1Mask, k8420_Int1_RERR);	// DAH 10/13/2005
      m_ulLockState = LOCKSTATE_UNLOCKED;
    }
  else
    {
      Write (k8420Interrupt1Mask, 0);	// DAH 10/13/2005
    }

  return (HSTATUS_OK);
}
#endif

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SetInputMuteOnError (BOOLEAN bMuteOnError)
/////////////////////////////////////////////////////////////////////////////
{
  PHALLSTREAM pLStream;

  if (bMuteOnError)
    Write (k8420MiscControl2, k8420_MC2_HOLD01, k8420_MC2_HOLD_MASK);
  else
    Write (k8420MiscControl2, k8420_MC2_HOLD10, k8420_MC2_HOLD_MASK);

  m_bMuteOnError = bMuteOnError;

  // Need to set the mute on error for the LStream devices
  pLStream = m_pHalAdapter->GetLStream ();
  if (pLStream)
    {
      pLStream->AESSetInputMuteOnError (LSTREAM_BRACKET, bMuteOnError);
      pLStream->AESSetInputMuteOnError (LSTREAM_HEADER, bMuteOnError);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SetOutputNonAudio (BOOLEAN bNonAudio)
// Only called from CHalWaveDevice and CHalWaveDMADevice
/////////////////////////////////////////////////////////////////////////////
{
  if (bNonAudio)
    {
      SET (m_ulOutputStatus, MIXVAL_OUTSTATUS_NONAUDIO);
      SET (m_TxCBuffer[0], MIXVAL_DCS_BYTE0_NONPCM);
    }
  else
    {
      CLR (m_ulOutputStatus, MIXVAL_OUTSTATUS_NONAUDIO);
      CLR (m_TxCBuffer[0], MIXVAL_DCS_BYTE0_NONPCM);
    }

  WriteCUBuffer (m_TxCBuffer);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SetOutputStatus (ULONG ulStatus)
/////////////////////////////////////////////////////////////////////////////
{
  // Validity
  if (ulStatus & MIXVAL_OUTSTATUS_VALID)
    Write (k8420MiscControl1, (BYTE) 0, k8420_MC1_VSET);	// transmit a 1 for the V bit (Invalid)
  else
    Write (k8420MiscControl1, k8420_MC1_VSET, k8420_MC1_VSET);	// transmit a 0 for the V bit (Valid)

  // Non-Audio
  if (ulStatus & MIXVAL_OUTSTATUS_NONAUDIO)
    SET (m_TxCBuffer[0], MIXVAL_DCS_BYTE0_NONPCM);
  else
    CLR (m_TxCBuffer[0], MIXVAL_DCS_BYTE0_NONPCM);

  // Emphasis
  if (m_TxCBuffer[0] & MIXVAL_DCS_BYTE0_PRO)
    {
      CLR (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_EMPH_MASK);
      switch (ulStatus & MIXVAL_OUTSTATUS_EMPHASIS_MASK)
	{
	default:
	case MIXVAL_OUTSTATUS_EMPHASIS_NONE:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_EMPH_NONE);
	  break;
	case MIXVAL_OUTSTATUS_EMPHASIS_5015:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_EMPH_5015);
	  break;
	case MIXVAL_OUTSTATUS_EMPHASIS_J17:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_EMPH_CCITTJ17);
	  break;
	}
    }
  else
    {
      if (ulStatus & MIXVAL_OUTSTATUS_EMPHASIS_5015)
	SET (m_TxCBuffer[0], MIXVAL_DCS_CON_BYTE0_EMPH_5015);
      else
	CLR (m_TxCBuffer[0], MIXVAL_DCS_CON_BYTE0_EMPH_5015);
    }

  m_ulOutputStatus = ulStatus;

  WriteCUBuffer (m_TxCBuffer);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::GetInputSampleRate (PLONG plRate)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulRate;

  m_pHalAdapter->GetFrequencyCounter (L2_FREQCOUNTER_DIGITALIN, &ulRate);

  m_pHalAdapter->NormalizeFrequency (ulRate, plRate);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
BYTE
Invert (BYTE ucIn)
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucOut = 0;

  for (int i = 0; i < 8; i++)
    if ((ucIn >> i) & 1)
      ucOut |= (1 << (7 - i));

  return (ucOut);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SampleClockChanged (LONG lRate, LONG lSource)
// Called by CHalSampleClock when the clock rate is changed
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err(CE_WARN,"CHal8420::SampleClockChanged\n");

  // if the Sample Clock Source is Digital, then we must set the 8420 to Mode 3 (SRC Off)
  if (lSource == MIXVAL_L2_CLKSRC_DIGITAL)
    {
      // only make the change if the SRC mode is ON or OFF Clock Sync
      if ((m_ulSRCMode == MIXVAL_SRCMODE_SRC_ON) ||
	  (m_ulSRCMode == MIXVAL_SRCMODE_SRC_ON_DIGOUT) ||
	  (m_ulSRCMode == MIXVAL_SRCMODE_SRC_OFF_CLKSYNC))
	{
	  SetSRCMode (MIXVAL_SRCMODE_SRC_OFF);
	}
    }
  else
    {				// if the Clock Source is not Digital
      // and the SRC mode is OFF, turn the SRC mode ON
      if (m_ulSRCMode == MIXVAL_SRCMODE_SRC_OFF)
	{
	  SetSRCMode (MIXVAL_SRCMODE_SRC_ON);
	}
    }

  return (SetFormat (m_ulFormat, lRate));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SetFormat (ULONG ulFormat, LONG lRate)
/////////////////////////////////////////////////////////////////////////////
{
  RtlZeroMemory (&m_TxCBuffer, sizeof (m_TxCBuffer));

  if (!lRate)
    {
      // get the current sample rate
      m_pHalAdapter->GetSampleClock ()->Get (&lRate);
    }

  switch (ulFormat)
    {
    case MIXVAL_DF_AESEBU:
      // Change the relay
      m_pHalAdapter->IOWrite (kMisc, IO_MISC_DF_AESEBU, IO_MISC_DF_MASK);
      m_ulFormat = MIXVAL_DF_AESEBU;

      m_TxCBuffer[0] = MIXVAL_DCS_BYTE0_PRO | MIXVAL_DCS_PRO_BYTE0_LOCKED;

      switch (m_ulOutputStatus & MIXVAL_OUTSTATUS_EMPHASIS_MASK)
	{
	default:
	case MIXVAL_OUTSTATUS_EMPHASIS_NONE:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_EMPH_NONE);
	  break;
	case MIXVAL_OUTSTATUS_EMPHASIS_5015:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_EMPH_5015);
	  break;
	case MIXVAL_OUTSTATUS_EMPHASIS_J17:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_EMPH_CCITTJ17);
	  break;
	}

      switch (lRate)
	{
	case 22050:
	  SET (m_TxCBuffer[4], MIXVAL_DCS_PRO_BYTE4_FS_22050);
	  break;
	case 24000:
	  SET (m_TxCBuffer[4], MIXVAL_DCS_PRO_BYTE4_FS_24000);
	  break;
	case 32000:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_FS_32000);
	  break;
	case 44056:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_FS_44100);
	  SET (m_TxCBuffer[4], MIXVAL_DCS_PRO_BYTE4_FS_PULLDOWN);
	  break;
	case 44100:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_FS_44100);
	  break;
	case 48000:
	  SET (m_TxCBuffer[0], MIXVAL_DCS_PRO_BYTE0_FS_48000);
	  break;
	case 88200:
	  SET (m_TxCBuffer[4], MIXVAL_DCS_PRO_BYTE4_FS_88200);
	  break;
	case 96000:
	  SET (m_TxCBuffer[4], MIXVAL_DCS_PRO_BYTE4_FS_96000);
	  break;
	case 176400:
	  SET (m_TxCBuffer[4], MIXVAL_DCS_PRO_BYTE4_FS_176400);
	  break;
	case 192000:
	  SET (m_TxCBuffer[4], MIXVAL_DCS_PRO_BYTE4_FS_192000);
	  break;
	}

      m_TxCBuffer[1] = MIXVAL_DCS_PRO_BYTE1_CM_STEREO;
      m_TxCBuffer[2] =
	MIXVAL_DCS_PRO_BYTE2_AUX_MAIN24 | MIXVAL_DCS_PRO_BYTE2_AUX_24BITS;

      m_TxCBuffer[6] = 'L';
      if (m_pHalAdapter->GetDeviceID () == PCIDEVICE_LYNX_L22)
	{
	  m_TxCBuffer[7] = '2';
	  m_TxCBuffer[8] = '2';
	  m_TxCBuffer[9] = ' ';
	}
      else
	{
	  m_TxCBuffer[7] = 'T';
	  m_TxCBuffer[8] = 'W';
	  m_TxCBuffer[9] = 'O';
	}
      break;
    case MIXVAL_DF_SPDIF:
      // Change the relay
      m_pHalAdapter->IOWrite (kMisc, IO_MISC_DF_SPDIF, IO_MISC_DF_MASK);
      m_ulFormat = MIXVAL_DF_SPDIF;

      m_TxCBuffer[0] =
	MIXVAL_DCS_BYTE0_CON | MIXVAL_DCS_CON_BYTE0_COPY_PERMIT;

      if (m_ulOutputStatus & MIXVAL_OUTSTATUS_EMPHASIS_5015)
	SET (m_TxCBuffer[0], MIXVAL_DCS_CON_BYTE0_EMPH_5015);

      // default is 44100
      switch (lRate)
	{
	case 32000:
	  SET (m_TxCBuffer[3], MIXVAL_DCS_CON_BYTE3_FS_32000);
	  break;
	case 44100:
	  SET (m_TxCBuffer[3], MIXVAL_DCS_CON_BYTE3_FS_44100);
	  break;
	case 48000:
	  SET (m_TxCBuffer[3], MIXVAL_DCS_CON_BYTE3_FS_48000);
	  break;
	}
      SET (m_TxCBuffer[3], MIXVAL_DCS_CON_BYTE3_CA_LEVELI);
      break;
    }

  if (m_ulOutputStatus & MIXVAL_OUTSTATUS_NONAUDIO)
    SET (m_TxCBuffer[0], MIXVAL_DCS_BYTE0_NONPCM);

  // write the transmitters C bit data
  WriteCUBuffer (m_TxCBuffer);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SetSRCMode (ULONG ulMode, BOOLEAN bInISR)
/////////////////////////////////////////////////////////////////////////////
{
  PHALSAMPLECLOCK pClock = m_pHalAdapter->GetSampleClock ();
  LONG lRate, lSource;
  //USHORT        usStatus;

  //cmn_err(CE_WARN,"CHal8420::SetMode\n");

  pClock->Get (&lRate, &lSource);

  switch (ulMode)
    {
    case MIXVAL_SRCMODE_SRC_ON:	// (CS8420 Page 14, Figure 12) AES In, SRC
      if (lSource == MIXVAL_L2_CLKSRC_DIGITAL)
	{
	  // SetPreferredSource is going to call Set, which in turn is going to call this function
	  // to keep it from doing this, we must set the mode right now
	  m_ulSRCMode = ulMode;
	  pClock->SetPreferredSource (MIXVAL_L2_CLKSRC_INTERNAL);
	}

      // Set the serial audio output to slave-in mode
      Write (k8420SerialOutputFormat, (BYTE) 0, k8420_SAO_SOMS);

      // Set the Digital Input LRCK Direction to Output
      m_pHalAdapter->IOWrite (kMisc, 0, IO_MISC_DILRCKDIR);

      Write (k8420MiscControl1, (BYTE) 0, k8420_MC1_MUTESAO);	// enable the serial audio output
      Write (k8420DataFlowControl,
	     (k8420_DFC_TXD01 | k8420_DFC_SPD00 | k8420_DFC_SRCD),
	     (k8420_DFC_TXD_MASK | k8420_DFC_SPD_MASK | k8420_DFC_SRCD));
      Write (k8420ClockSourceControl, k8420_CSC_RXD01,
	     (k8420_CSC_OUTC | k8420_CSC_INC | k8420_CSC_RXD_MASK));
      break;
    case MIXVAL_SRCMODE_SRC_OFF_CLKSYNC:	// (CS8420 Page 14, Figure 13 Modified) Synchronous AES In, No SRC
      if (lSource == MIXVAL_L2_CLKSRC_DIGITAL)
	{
	  // SetPreferredSource is going to call Set, which in turn is going to call this function
	  // to keep it from doing this, we must set the mode right now
	  m_ulSRCMode = ulMode;
	  pClock->SetPreferredSource (MIXVAL_L2_CLKSRC_INTERNAL);
	}

      // Set the serial audio output to slave-in mode
      Write (k8420SerialOutputFormat, (BYTE) 0, k8420_SAO_SOMS);

      // Set the Digital Input LRCK Direction to Output
      m_pHalAdapter->IOWrite (kMisc, 0, IO_MISC_DILRCKDIR);

      Write (k8420MiscControl1, (BYTE) 0, k8420_MC1_MUTESAO);	// enable the serial audio output
      Write (k8420DataFlowControl, (k8420_DFC_TXD01 | k8420_DFC_SPD10),
	     (k8420_DFC_TXD_MASK | k8420_DFC_SPD_MASK | k8420_DFC_SRCD));
      Write (k8420ClockSourceControl, k8420_CSC_RXD01,
	     (k8420_CSC_OUTC | k8420_CSC_INC | k8420_CSC_RXD_MASK));
      break;
    case MIXVAL_SRCMODE_SRC_OFF:	// (CS8420 Page 14, Figure 13) Slave to AES In, No SRC
      // must set the Sample Clock Source to Digital before we change the 
      // rest of this so the digital input is still locked.
      if (lSource != MIXVAL_L2_CLKSRC_DIGITAL)
	{
	  // SetPreferredSource is going to call Set, which in turn is going to call this function
	  // to keep it from doing this, we must set the mode right now
	  m_ulSRCMode = ulMode;
	  pClock->SetPreferredSource (MIXVAL_L2_CLKSRC_DIGITAL);
	}
      // Set the Digital Input LRCK Direction to Input
      m_pHalAdapter->IOWrite (kMisc, IO_MISC_DILRCKDIR, IO_MISC_DILRCKDIR);

      // Set the serial audio output to Master-out mode
      Write (k8420SerialOutputFormat, k8420_SAO_SOMS, k8420_SAO_SOMS);

      Write (k8420MiscControl1, (BYTE) 0, k8420_MC1_MUTESAO);	// enable the serial audio output
      Write (k8420DataFlowControl, (k8420_DFC_TXD01 | k8420_DFC_SPD10),
	     (k8420_DFC_TXD_MASK | k8420_DFC_SPD_MASK | k8420_DFC_SRCD));
      Write (k8420ClockSourceControl, (k8420_CSC_OUTC | k8420_CSC_RXD01),
	     (k8420_CSC_OUTC | k8420_CSC_INC | k8420_CSC_RXD_MASK));
      break;
    case MIXVAL_SRCMODE_SRC_ON_DIGOUT:	// (CS8420 Page 14, Figure 11) AES out SRC to AES in
      if (lSource == MIXVAL_L2_CLKSRC_DIGITAL)
	{
	  // SetPreferredSource is going to call Set, which in turn is going to call this function
	  // to keep it from doing this, we must set the mode right now
	  m_ulSRCMode = ulMode;
	  pClock->SetPreferredSource (MIXVAL_L2_CLKSRC_INTERNAL);
	}

      // Set the serial audio output to slave-in mode
      Write (k8420SerialOutputFormat, (BYTE) 0, k8420_SAO_SOMS);

      // Set the Digital Input LRCK Direction to Output
      m_pHalAdapter->IOWrite (kMisc, 0, IO_MISC_DILRCKDIR);

      Write (k8420MiscControl1, k8420_MC1_MUTESAO, k8420_MC1_MUTESAO);	// mute the serial audio output
      Write (k8420DataFlowControl, (BYTE) 0,
	     (k8420_DFC_TXD_MASK | k8420_DFC_SPD_MASK | k8420_DFC_SRCD));
      Write (k8420ClockSourceControl,
	     (k8420_CSC_OUTC | k8420_CSC_INC | k8420_CSC_RXD01),
	     (k8420_CSC_OUTC | k8420_CSC_INC | k8420_CSC_RXD_MASK));
      break;
    case MIXVAL_SRCMODE_TXONLY:	// (CS8420 Page 14, Figure 15) Transmit Only
      if (lSource == MIXVAL_L2_CLKSRC_DIGITAL)
	{
	  // SetPreferredSource is going to call Set, which in turn is going to call this function
	  // to keep it from doing this, we must set the mode right now
	  m_ulSRCMode = ulMode;
	  pClock->SetPreferredSource (MIXVAL_L2_CLKSRC_INTERNAL);
	}

      Write (k8420DataFlowControl, (k8420_DFC_TXD01 | k8420_DFC_SPD01),
	     (k8420_DFC_TXD_MASK | k8420_DFC_SPD_MASK | k8420_DFC_SRCD));
      Write (k8420ClockSourceControl, (k8420_CSC_INC | k8420_CSC_RXD00),
	     (k8420_CSC_OUTC | k8420_CSC_INC | k8420_CSC_RXD_MASK));
      break;
    default:
      return (HSTATUS_INVALID_PARAMETER);
    }

  m_ulSRCMode = ulMode;

  // Notify mixer of changes (except if we are being called from within the ISR)
  if (!bInISR)
    m_pHalAdapter->GetMixer ()->ControlChanged (LINE_ADAPTER, LINE_NO_SOURCE,
						CONTROL_SRC_MODE);

  return (HSTATUS_OK);
}

#define SRCRATIO_SCALE	10000L	// 5 digits of precision
#define SRCRATIO_NORMAL	0x10000	// 1:1

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::GetSRCRatio (PULONG pulSRCRatio)
// returned as a 16:16 fixed point number where the fractional part is in 
// the lower 16 bits of the ULONG.  
// Returns 0xFFFFFFFF is SRC is OFF
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulLRClock, ulDigitalInRate, ulSRCRatio = 0;

  if ((m_ulSRCMode == MIXVAL_SRCMODE_SRC_ON)
      || (m_ulSRCMode == MIXVAL_SRCMODE_SRC_ON_DIGOUT))
    {
      m_pHalAdapter->GetFrequencyCounter (L2_FREQCOUNTER_DIGITALIN, &ulDigitalInRate);	// this will return 0 if the input is unlocked

      // The ratio is LRClock / DigitalIn
      // This is returned as a 16:16 fixed point number
      if (ulDigitalInRate)
	{
	  m_pHalAdapter->GetFrequencyCounter (L2_FREQCOUNTER_LRCLOCK,
					      &ulLRClock);
	  // Valid lock range of 8K to 108K
	  if ((ulLRClock >= 8000) && (ulLRClock <= 108000))
	    {
	      ulSRCRatio =
		(((ulLRClock * SRCRATIO_SCALE) / ulDigitalInRate) *
		 SRCRATIO_NORMAL) / SRCRATIO_SCALE;
	    }
	}
    }
  else
    {
      ulSRCRatio = 0xFFFFFFFF;	// let the applications know that the SRC is OFF
    }

  *pulSRCRatio = ulSRCRatio;

  return (HSTATUS_OK);
}

/*
/////////////////////////////////////////////////////////////////////////////
USHORT	CHal8420::GetQChannelSubcode( PQCHANNELSUBCODE pSubcode )
/////////////////////////////////////////////////////////////////////////////
{
	// copy the current Q-Subcode into the users buffer
	// *pSubcode = m_QSubcode;
	return( HSTATUS_OK );
}
*/
/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::ReadCUBuffer (PBYTE pBuffer)
// never used
/////////////////////////////////////////////////////////////////////////////
{
  Read (k8420CorUDataBuffer, pBuffer, 24);	// 192 bits or 24 bytes
  for (int i = 0; i < 24; i++)
    pBuffer[i] = Invert (pBuffer[i]);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::WriteCUBuffer (PBYTE pBuffer)
// Called From:
//      CHal8420::SetOutputNonAudio
//      CHal8420::SetOutputStatus
//      CHal8420::SetFormat
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucByte;
  BYTE ucShadowByte;
  BYTE ucRegister = k8420CorUDataBuffer;

  // invert the bytes in the buffer (prepare for transmission)
  for (int i = 0; i < 24; i++, ucRegister++)
    {
      ucByte = Invert (pBuffer[i]);
      // if this is the first time for the CU write, write all the bytes down
      if (m_bWriteCUFirstTime)
	{
	  // only write if non-zero
	  if (ucByte)
	    {
	      Write (ucRegister, ucByte);
	    }
	}
      // if not the first time, only write the bytes that actually changed
      else
	{
	  m_pHalAdapter->IORead (ucRegister, &ucShadowByte, TRUE);	// Read the shadow register only
	  if (ucShadowByte != ucByte)
	    Write (ucRegister, ucByte);
	}
    }

  // Disable both transfers
  //Write( k8420CSDataBufferControl, (k8420_CsDB_EFTCI | k8420_CsDB_DETCI), (k8420_CsDB_EFTCI | k8420_CsDB_DETCI) );

  // Write the E buffer
  //Write( k8420CorUDataBuffer, LocalBuffer, 24 );        // 192 bits or 24 bytes

  // Enable EtoF transfers
  //Write( k8420CSDataBufferControl, (BYTE)0, k8420_CsDB_EFTCI );

  m_bWriteCUFirstTime = FALSE;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SetDefaults (void)
/////////////////////////////////////////////////////////////////////////////
{
  SetFormat (MIXVAL_DF_AESEBU);
  SetSRCMode (MIXVAL_SRCMODE_SRC_ON);
  SetInputMuteOnError (TRUE);
  SetOutputStatus (MIXVAL_OUTSTATUS_VALID);	// Non-Audio Off, Emphasis Off

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::SetMixerControl (USHORT usControl, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  switch (usControl)
    {
    case CONTROL_DIGITAL_FORMAT:
      return (SetFormat (ulValue));
    case CONTROL_SRC_MODE:
      return (SetSRCMode (ulValue));
    case CONTROL_DIGITALIN_MUTE_ON_ERROR:
      return (SetInputMuteOnError ((BOOLEAN) ulValue));
    case CONTROL_DIGITALOUT_STATUS:
      return (SetOutputStatus (ulValue));
#ifdef USE_RESET_ON_UNLOCK
    case CONTROL_RESET_DIGITALIO_LOCK:
      return (SetResetDigitalIOUnlock ((BOOLEAN) ulValue));
#endif
    default:
      return (HSTATUS_INVALID_MIXER_CONTROL);
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::GetMixerControl (USHORT usControl, PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  switch (usControl)
    {
    case CONTROL_DIGITAL_FORMAT:
      *pulValue = GetFormat ();
      break;
    case CONTROL_SRC_MODE:
      *pulValue = GetSRCMode ();
      break;
    case CONTROL_SRC_RATIO:
      return (GetSRCRatio (pulValue));
    case CONTROL_DIGITALIN_RATE:
      return (GetInputSampleRate ((PLONG) pulValue));
    case CONTROL_DIGITALIN_MUTE_ON_ERROR:
      *pulValue = GetInputMuteOnError ();
      break;
    case CONTROL_DIGITALIN_STATUS:
      return (GetInputStatus (pulValue));
    case CONTROL_DIGITALOUT_STATUS:
      *pulValue = GetOutputStatus ();
      break;
#ifdef USE_RESET_ON_UNLOCK
    case CONTROL_RESET_DIGITALIO_LOCK:
      *pulValue = m_bResetDigitalIOLock;
      break;
#endif

    default:
      return (HSTATUS_INVALID_MIXER_CONTROL);
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHal8420::Service ()
// called by the ISR to service the 8420
/////////////////////////////////////////////////////////////////////////////
{
#ifdef USE_RESET_ON_UNLOCK
  BYTE ucInterrupt1Status = 0;	//, ucInterrupt2Status;

  //DC('8');
  //cmn_err(CE_WARN,"CHal8420::Service\n");

  // go read both interrupt status registers
  // TODO: This read really needs to happen before we read the MISTAT register, 
  // otherwise the hardware will generate another interrupt for no reason.

  // This read is happening from the ISR level (not DPC), so any code that is running
  // at DPC level is going to get pre-empted by this code, and potentially cause a problem
  // on the PCI bus.  In any case, this read can fail if some other code is running
  // and IORead or IOWrite right now, so that will also cause a problem because this
  // read MUST actually hit the hardware for the 8420 to stop interrupting.

  // if the foreground is actively doing IOWrites, we wait here until that process is complete
  if (m_lInIOActive)
    {
      cmn_err (CE_WARN, "ACTIVE!\n");
    }

  // this while loop is dangerous because it may lockup the machine on single processor computers
  while (m_lInIOActive)
    ;

  while (Read (k8420Interrupt1Status, &ucInterrupt1Status) != HSTATUS_OK)
    ;
  //Read( k8420Interrupt2Status, &ucInterrupt2Status );

  if (!ucInterrupt1Status)	//&& !ucInterrupt2Status )
    {
      //DC('!');
      return (HSTATUS_INVALID_PARAMETER);
    }

  //DC('<');      DX8( ucInterrupt1Status, COLOR_NORMAL ); DC(' '); DX8( ucInterrupt2Status, COLOR_NORMAL ); DC(' ');
  //cmn_err(CE_WARN,"CHal8420::Service %02x %02x\n", ucInterrupt1Status, ucInterrupt2Status );

/*
	if( ucInterrupt1Status & k8420_Int1_EFTC ) // E to F Transfer C bit (Transmitter)
	{
		// This is now a one-shot interrupt.  Everytime the the CS data to 
		// transmit changes we disable the DtoE transfers, write the transmit
		// data, enable the EtoF transfer and wait for it to complete.  This
		// interrupt comes in when the EtoF transfer is complete and we can
		// resume normal operation.
		//cmn_err(CE_WARN,"8420: E to F Transfer\n");
		DC('f');
		// inhibit another E to F transfer
		// enable D to E transfer
		Write( k8420CSDataBufferControl, k8420_CsDB_EFTCI, (k8420_CsDB_EFTCI | k8420_CsDB_DETCI) );
	}
	if( ucInterrupt1Status & k8420_Int1_DETC ) // D to E Transfer C bit (Receiver)
	{
		// This interrupt should never come in...
		//cmn_err(CE_WARN,"8420: D to E Transfer\n");
		DC('d');
		// This interrupt occurs every 192 AES3 frames which is 
		// 229.69 times a second at 44.1kHz (4.35ms) and
		// 500 times a second at 96kHz (2ms).

		// inhibit another D to E transfer
		//Write( k8420CSDataBufferControl, k8420_CsDB_DETCI, k8420_CsDB_DETCI );
		// read the receivers C bit data
		//ReadCUBuffer( m_RxCBuffer );
		// write the transmitters C bit data
		//WriteCUBuffer( m_TxCBuffer );
		// enable E to F transfer
		//Write( k8420CSDataBufferControl, (BYTE)0, k8420_CsDB_EFTCI );
		//Write( k8420Interrupt1Mask, (k8420_Int1_EFTC), (k8420_Int1_EFTC | k8420_Int1_DETC) );

	}
*/
  if (ucInterrupt1Status & k8420_Int1_RERR)
    {
      UpdateStatus (TRUE);
    }
/*
	if( ucInterrupt2Status & k8420_Int2_QCH )
	{
		//cmn_err(CE_WARN,"8420: Q-Subcode\n");
		DC('q');
		// handle the Q-Subcode interrupt.
		// This interrupt occurs every 588 AES3 frames which is 
		// 75 times a second at 44.1kHz (13.33ms) and
		// 163.27 times a second at 96kHz (6.125ms).
		// read just the first byte to see if this is valid sub-code

		//Read( k8420QSubCodeData, (PBYTE)&m_QSubcode );
		//if( m_QSubcode.ucAddrCntl == 0x01 )
		//{
		//	Read( k8420QSubCodeData, (PBYTE)&m_QSubcode, sizeof( QCHANNELSUBCODE ) );
			// TODO: alert the driver that a mixer control has changed...
			//cmn_err(CE_WARN,"A %02x T %02x I %02x %02x:%02x:%02x %02x:%02x:%02x   \r", 
			//	m_QSubcode.ucAddrCntl, m_QSubcode.ucTrack, m_QSubcode.ucIndex, 
			//	m_QSubcode.ucMinute, m_QSubcode.ucSecond, m_QSubcode.ucFrame,
			//	m_QSubcode.ucABSMinute, m_QSubcode.ucABSSecond, m_QSubcode.ucABSFrame );
		//}
	}
*/
#endif // USE_RESET_ON_UNLOCK
  //DC('>');
  return (HSTATUS_OK);
}
