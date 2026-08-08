/****************************************************************************
 HalWaveDMADevice.cpp

 Description: 

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

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDMADevice::Open (PHALADAPTER pHalAdapter, ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  m_DMA.Open (pHalAdapter, ulDeviceNumber);

  m_bIsDMAActive = FALSE;
  m_bAutoFree = TRUE;
  m_bOverrunIE = FALSE;
  m_bLimitIE = FALSE;
  m_bDMASingle = FALSE;
  m_bDualMono = FALSE;
  m_bOverIgnore = FALSE;

  return (CHalWaveDevice::Open (pHalAdapter, ulDeviceNumber));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDMADevice::Close ()
/////////////////////////////////////////////////////////////////////////////
{
  Stop ();

  m_DMA.Close ();

  return (CHalWaveDevice::Close ());
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDMADevice::Start ()
/////////////////////////////////////////////////////////////////////////////
{
  LONG lCurrentRate;
  PHALSAMPLECLOCK pClock;
  ULONG ulStreamControl = 0;

  //cmn_err((CE_WARN,"CHalWaveDMADevice::Start Adapter [%ld] Device [%ld]\n", m_pHalAdapter->GetAdapterNumber(), m_ulDeviceNumber ));

  m_bIsDMAActive = TRUE;

  // if the sample rate is not the same as the current sample rate on the card, change it...
  pClock = m_pHalAdapter->GetSampleClock ();
  pClock->Get (&lCurrentRate);
  if (lCurrentRate != m_lSampleRate)
    if (pClock->Set (m_lSampleRate))
      return (HSTATUS_INVALID_SAMPLERATE);

  if (m_bIsAES16e)
    {
      // set the bits for the sample format
      switch (m_lBitsPerSample)
	{
	case 8:
	  SET (ulStreamControl, REG_ESTRMCTL_FMT_PCM8);
	  break;
	case 16:
	  SET (ulStreamControl, REG_ESTRMCTL_FMT_PCM16);
	  break;
	case 24:
	  SET (ulStreamControl, REG_ESTRMCTL_FMT_PCM24);
	  break;
	case 32:
	  SET (ulStreamControl, REG_ESTRMCTL_FMT_PCM32);
	  break;
	}
    }
  else
    {
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
    {
      if (m_bIsAES16e)
	SET (ulStreamControl, REG_ESTRMCTL_CHNUM_STEREO);
      else
	SET (ulStreamControl, REG_L2STRMCTL_CHNUM_STEREO);
    }

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

  // Write the control register to the hardware
  m_RegStreamControl.Write (ulStreamControl);

  // start the DMA channel
  m_DMA.Start ();

  // note that we call the CHalDevice parent class, not CHalWaveDevice!
  return (CHalDevice::Start ());
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDMADevice::Stop ()
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalWaveDMADevice::Stop %ld\n", m_ulDeviceNumber ));

  // stop the DMA channel
  m_DMA.Stop ();

  m_bIsDMAActive = FALSE;
  m_bOverrunIE = FALSE;
  m_bLimitIE = FALSE;
  m_bDMASingle = FALSE;
  m_bDualMono = FALSE;
  m_bOverIgnore = FALSE;
  m_bAutoFree = TRUE;		// DAH Oct 4, 2004

  // and call the parent class
  return (CHalWaveDevice::Stop ());
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalWaveDMADevice::Service (BOOLEAN bDMABufferComplete)
// Called at interrupt time to service the DMA
// There could be several reasons why this gets called:
//      1. Over/Under run interrupt
//      2. Limit interrupt
//      3. DMA buffer completed
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalWaveDMADevice::Service %ld\n", m_ulDeviceNumber ));
  ULONG ulReason = kReasonNone;

  if (m_bIsAES16e)
    {
#ifdef DEBUG0
      ULONG ulStreamStatus = m_RegStreamStatus.Read ();
      DC ('[');
      DX8 ((BYTE) m_ulDeviceNumber, COLOR_NORMAL);
      DC (',');
      DX16 ((USHORT)
	    ((ulStreamStatus & REG_ESTRMSTAT_L2PTR_MASK) >>
	     REG_ESTRMSTAT_L2PTR_OFFSET), COLOR_NORMAL);
      DC (',');
      DX8 ((BYTE)
	   ((ulStreamStatus & REG_ESTRMSTAT_DMABINX_MASK) >>
	    REG_ESTRMSTAT_DMABINX_OFFSET), COLOR_NORMAL);
      DC (']');
#endif

      if (bDMABufferComplete)
	{
	  //DC('D');
	  ulReason = kReasonDMABufferComplete;
	}
      else
	{
	  DC ('E');
	  cmn_err (CE_WARN,
		   "ERROR! CHalWaveDMADevice::Service %ld CALLED with bDMABufferComplete FALSE!\n",
		   m_ulDeviceNumber);
	}
    }
  else
    {
      ULONG ulStreamStatus = m_RegStreamStatus.Read ();

#ifdef DEBUG0
      ULONG ulStreamControl = m_RegStreamControl.Read ();

      //DB('d',COLOR_BOLD);   DX8( (BYTE)m_ulDeviceNumber, COLOR_NORMAL );
      //cmn_err((CE_WARN,"CHalWaveDMADevice::Service\n"));

      DC ('[');
      DX8 ((BYTE) m_ulDeviceNumber, COLOR_NORMAL);
      DC (',');
      DX16 ((USHORT) (ulStreamStatus & REG_L2STRMSTAT_L2PTR_MASK),
	    COLOR_NORMAL);
      DC (',');
      DX16 ((USHORT) (ulStreamControl & REG_L2STRMCTL_PCPTR_MASK),
	    COLOR_NORMAL);
      DC (',');
      DX8 ((BYTE)
	   ((ulStreamControl & REG_L2STRMCTL_DMABINX_MASK) >>
	    REG_L2STRMCTL_DMABINX_OFFSET), COLOR_NORMAL);
      DC (']');
#endif
      if (bDMABufferComplete)
	{
	  //DC('D');
	  ulReason = kReasonDMABufferComplete;
	}
      else			// make the limit reason mutually-exclusive to the DMA Buffer Complete reason
	// was the limit hit bit set?
	// NOTE: It is possible that the limit bit may be cleared by the hardware 
	// before the PC has time to read the stream status register.
      if (ulStreamStatus & REG_L2STRMSTAT_LIMHIT)
	{
	  DC ('L');
	  //cmn_err((CE_WARN,"Limit Hit\n"));
	  ulReason = kReasonDMALimit;
	}

      // was the overrun bit set? (Note: This overrides kReasonDMABufferComplete or kReasonDMALimit)
      if (ulStreamStatus & REG_L2STRMSTAT_OVER)
	{
	  DB ('O', COLOR_BOLD_U);
	  m_ulOverrunCount++;
	  cmn_err (CE_WARN, " Dev %ld Dropout [%lu] ", m_ulDeviceNumber,
		   m_ulOverrunCount);
	  ulReason = kReasonDMAEmpty;
	}
    }

  if (ulReason != kReasonDMAEmpty)
    {
      // free an entry from the DMA list
      if (m_bAutoFree)
	m_DMA.FreeEntry ();
    }

  // let the driver service the interrupt for this device
  CHalDevice::Service (ulReason);

  return (HSTATUS_OK);
}
