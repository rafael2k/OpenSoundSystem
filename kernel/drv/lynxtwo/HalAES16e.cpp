/****************************************************************************
 HalAES16e.cpp

 Description:	Lynx Application Programming Interface Header File

 Created: David A. Hoatson, December 2007 from Hal4114.cpp
	
 Copyright © 2007 Lynx Studio Technology, Inc.

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
 ***************************************************************************/

#include <StdAfx.h>
#include "HalAdapter.h"

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::Open (PHALADAPTER pHalAdapter)
/////////////////////////////////////////////////////////////////////////////
{
  PLYNXTWOREGISTERS pRegisters = pHalAdapter->GetRegisters ();

  // save the pointer to the parent adapter object
  m_pHalAdapter = pHalAdapter;
  m_usDeviceID = pHalAdapter->GetDeviceID ();
  m_bHasSRC = FALSE;

  if ((m_usDeviceID == PCIDEVICE_LYNX_AES16eSRC)
      || (m_usDeviceID == PCIDEVICE_LYNX_AES16e50))
    {
      m_bHasSRC = TRUE;
    }

  if (m_usDeviceID == PCIDEVICE_LYNX_AES16e50)
    {
      m_bHasAES50 = TRUE;
      m_RegAES50Status.Init (pHalAdapter, &pRegisters->AESeBlock.AES50Status,
			     REG_READONLY);
    }


  for (ULONG ulTheChip = kChip1; ulTheChip < kNumberOfDIO; ulTheChip++)
    {
      m_pRegAESControl[ulTheChip].Init (pHalAdapter, &pRegisters->AESeBlock.AESControl[ulTheChip], REG_WRITEONLY);	// value defaults to 0, not written to hardware at this point
      m_pRegAESStatus[ulTheChip].Init (pHalAdapter,
				       &pRegisters->AESeBlock.
				       AESStatus[ulTheChip]);
    }

  m_RegMISCTL.Init (m_pHalAdapter, &pRegisters->MISCTL, REG_WRITEONLY);	// Setting an initial value in Init doesn't do a write to hardware

  SetDefaults ();

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::Close ()
/////////////////////////////////////////////////////////////////////////////
{
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::SetDefaults (void)
/////////////////////////////////////////////////////////////////////////////
{
  for (ULONG ulTheChip = kDIO1; ulTheChip < kNumberOfDIO; ulTheChip++)
    {
      SetOutputStatus (ulTheChip, MIXVAL_OUTSTATUS_VALID);
      SetSRCMode (ulTheChip, MIXVAL_SRCMODE_OFF);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::GetInputRate (ULONG ulTheChip, PLONG plRate)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulRate;

  m_pHalAdapter->GetFrequencyCounter (AES16_FREQCOUNTER_DI1 +
				      (USHORT) ulTheChip, &ulRate);

  m_pHalAdapter->NormalizeFrequency (ulRate, plRate);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::GetInputStatus (ULONG ulTheChip, PULONG pulStatus)
/////////////////////////////////////////////////////////////////////////////
{
  *pulStatus = m_pRegAESStatus[ulTheChip];

  /*
     LONG lRate;

     GetInputRate( ulTheChip, &lRate );

     if( !lRate )
     {
     *pulStatus = kInStatusErrUnlock;
     }
     else
     {
     switch( lRate )
     {
     case 44100:          *pulStatus = kInStatusSR44100;  break;
     case 48000:          *pulStatus = kInStatusSR48000;  break;
     case 88200:          *pulStatus = kInStatusSR88200;  break;
     case 96000:          *pulStatus = kInStatusSR96000;  break;
     case 176400: *pulStatus = kInStatusSR176400; break;
     case 192000: *pulStatus = kInStatusSR192000; break;
     }

     *pulStatus |= (kInStatusValidity | kInStatusProfessional | kInStatusPCM24 | kInStatusEmphNone);
     }
   */

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalAES16e::IsInputLocked (ULONG ulTheChip)
/////////////////////////////////////////////////////////////////////////////
{
  return (m_pRegAESStatus[ulTheChip].Read () & kInStatusErrUnlock ? FALSE :
	  TRUE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::GetSRCMode (ULONG ulTheChip, PULONG pulMode)
/////////////////////////////////////////////////////////////////////////////
{
  if (!m_bHasSRC)
    return (HSTATUS_INVALID_MIXER_CONTROL);

  *pulMode = m_ulSRCMode[ulTheChip];

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::SetSRCMode (ULONG ulTheChip, ULONG ulMode)
/////////////////////////////////////////////////////////////////////////////
{
  if (!m_bHasSRC)
    return (HSTATUS_INVALID_MIXER_CONTROL);

  //cmn_err((CE_WARN,"SetSRCMode: %lu %lu\n", ulTheChip, ulMode );

  m_ulSRCMode[ulTheChip] = ulMode;

  switch (ulMode)
    {
    case MIXVAL_SRCMODE_OFF:
      m_pRegAESControl[ulTheChip].Write (REG_AESTXCTL_SRCMODE_OFF,
					 REG_AESTXCTL_SRCMODE_MASK);
      break;
    case MIXVAL_SRCMODE_ON_DIGIN:
      m_pRegAESControl[ulTheChip].Write (REG_AESTXCTL_SRCMODE_ON_DIGIN,
					 REG_AESTXCTL_SRCMODE_MASK);
      break;
    }

  //m_RegMISCTL.BitSet( (REG_MISCTL_SRCEN_0 << ulTheChip), (BOOLEAN)ulEnable );

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::GetSRCRatio (ULONG ulTheChip, PULONG pulSRCRatio)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulLRClock, ulDigitalIn, ulSRCRatio = 0xFFFFFFFF;

  if (!m_bHasSRC)
    return (HSTATUS_INVALID_MIXER_CONTROL);

  // if the SRC is OFF, then the SRC Ratio should be 0xFFFFFFFF
  if (m_ulSRCMode[ulTheChip] == MIXVAL_SRCMODE_ON_DIGIN)
    {
      m_pHalAdapter->GetFrequencyCounter (AES16_FREQCOUNTER_DI1 + (USHORT) ulTheChip, &ulDigitalIn);	// this will return 0 if the input is unlocked

      // The ratio is LRClock / DigitalIn
      // This is returned as a 16:16 fixed point number
      if (ulDigitalIn)
	{
	  m_pHalAdapter->GetFrequencyCounter (AES16_FREQCOUNTER_LRCLOCK,
					      &ulLRClock);
	  //cmn_err (CE_WARN, "LRClock %lu Digital In %lu\n", ulLRClock, ulDigitalIn);
	  ulSRCRatio =
	    (((ulLRClock * SRC_RATIO_SCALE) / ulDigitalIn) *
	     SRC_RATIO_NORMAL) / SRC_RATIO_SCALE;
	}
    }

  *pulSRCRatio = ulSRCRatio;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::GetOutputStatus (ULONG ulTheChip, PULONG pulStatus)
/////////////////////////////////////////////////////////////////////////////
{
  *pulStatus = m_pRegAESControl[ulTheChip] & REG_AESTXCTL_OUTSTATUS_MASK;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::SetOutputStatus (ULONG ulTheChip, ULONG ulStatus)
/////////////////////////////////////////////////////////////////////////////
{
  m_pRegAESControl[ulTheChip].Write (ulStatus, REG_AESTXCTL_OUTSTATUS_MASK);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::SampleClockChanged (LONG lRate, LONG lSource)
// Called by CHalSampleClock when the clock rate has changed
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulWideWireIn, ulWideWireOut;
  PHALSAMPLECLOCK pClock = m_pHalAdapter->GetSampleClock ();

  pClock->GetMixerControl (CONTROL_WIDEWIREIN, &ulWideWireIn);
  pClock->GetMixerControl (CONTROL_WIDEWIREOUT, &ulWideWireOut);

  // DAH 3/16/2009 Added catch for 2X & 4X rates
  if (ulWideWireIn && (lRate > 50000))
    {
      m_RegMISCTL.BitSet (REG_MISCTLE_WIDEWIREIN, TRUE);
    }
  else
    {
      // DAH 3/16/2009 Added else FALSE so hardware gets reset
      m_RegMISCTL.BitSet (REG_MISCTLE_WIDEWIREIN, FALSE);
    }

  // only turn on wide wire out in hardware if it is required
  if (ulWideWireOut && (lRate > 50000))
    {
      m_RegMISCTL.BitSet (REG_MISCTLE_WIDEWIREOUT, TRUE);
    }
  else
    {
      m_RegMISCTL.BitSet (REG_MISCTLE_WIDEWIREOUT, FALSE);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::GetAES50Status (PULONG pulStatus)
/////////////////////////////////////////////////////////////////////////////
{
  if (!m_bHasAES50)
    return (HSTATUS_INVALID_MIXER_CONTROL);

  *pulStatus = m_RegAES50Status.Read ();

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Set/Get Mixer Control
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::SetMixerControl (USHORT usControl, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
//cmn_err (CE_WARN, "HALAES16e SetMixerControl ctl=%x, val=%x\n", usControl, ulValue);
  switch (usControl)
    {
    case CONTROL_DIO1_SRC_MODE:
      return (SetSRCMode (kDIO1, ulValue));
    case CONTROL_DIO2_SRC_MODE:
      return (SetSRCMode (kDIO2, ulValue));
    case CONTROL_DIO3_SRC_MODE:
      return (SetSRCMode (kDIO3, ulValue));
    case CONTROL_DIO4_SRC_MODE:
      return (SetSRCMode (kDIO4, ulValue));
    case CONTROL_DIO5_SRC_MODE:
      return (SetSRCMode (kDIO5, ulValue));
    case CONTROL_DIO6_SRC_MODE:
      return (SetSRCMode (kDIO6, ulValue));
    case CONTROL_DIO7_SRC_MODE:
      return (SetSRCMode (kDIO7, ulValue));
    case CONTROL_DIO8_SRC_MODE:
      return (SetSRCMode (kDIO8, ulValue));

    case CONTROL_DIGITALOUT1_STATUS:
    case CONTROL_DIGITALOUT2_STATUS:
    case CONTROL_DIGITALOUT3_STATUS:
    case CONTROL_DIGITALOUT4_STATUS:
    case CONTROL_DIGITALOUT5_STATUS:
    case CONTROL_DIGITALOUT6_STATUS:
    case CONTROL_DIGITALOUT7_STATUS:
    case CONTROL_DIGITALOUT8_STATUS:
      return (SetOutputStatus
	      (usControl - CONTROL_DIGITALOUT1_STATUS, ulValue));

    default:
      return (HSTATUS_INVALID_MIXER_CONTROL);
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAES16e::GetMixerControl (USHORT usControl, PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
//cmn_err (CE_WARN, "HALAES16e GetMixerControl ctl=%x, val=%x\n", usControl, *pulValue);
  switch (usControl)
    {
    case CONTROL_DIGITALIN1_RATE:
      return (m_pHalAdapter->GetFrequencyCounter (AES16_FREQCOUNTER_DI1, pulValue));	// these controls are not contiguous so 
    case CONTROL_DIGITALIN2_RATE:
      return (m_pHalAdapter->GetFrequencyCounter (AES16_FREQCOUNTER_DI2, pulValue));	// so we must call each one seperately.
    case CONTROL_DIGITALIN3_RATE:
      return (m_pHalAdapter->GetFrequencyCounter
	      (AES16_FREQCOUNTER_DI3, pulValue));
    case CONTROL_DIGITALIN4_RATE:
      return (m_pHalAdapter->GetFrequencyCounter
	      (AES16_FREQCOUNTER_DI4, pulValue));
    case CONTROL_DIGITALIN5_RATE:
      return (m_pHalAdapter->GetFrequencyCounter
	      (AES16_FREQCOUNTER_DI5, pulValue));
    case CONTROL_DIGITALIN6_RATE:
      return (m_pHalAdapter->GetFrequencyCounter
	      (AES16_FREQCOUNTER_DI6, pulValue));
    case CONTROL_DIGITALIN7_RATE:
      return (m_pHalAdapter->GetFrequencyCounter
	      (AES16_FREQCOUNTER_DI7, pulValue));
    case CONTROL_DIGITALIN8_RATE:
      return (m_pHalAdapter->GetFrequencyCounter
	      (AES16_FREQCOUNTER_DI8, pulValue));

    case CONTROL_DIGITALIN1_STATUS:
      return (GetInputStatus (kDIO1, pulValue));	// these controls are not contiguous so 
    case CONTROL_DIGITALIN2_STATUS:
      return (GetInputStatus (kDIO2, pulValue));	// so we must call each one seperately.
    case CONTROL_DIGITALIN3_STATUS:
      return (GetInputStatus (kDIO3, pulValue));
    case CONTROL_DIGITALIN4_STATUS:
      return (GetInputStatus (kDIO4, pulValue));
    case CONTROL_DIGITALIN5_STATUS:
      return (GetInputStatus (kDIO5, pulValue));
    case CONTROL_DIGITALIN6_STATUS:
      return (GetInputStatus (kDIO6, pulValue));
    case CONTROL_DIGITALIN7_STATUS:
      return (GetInputStatus (kDIO7, pulValue));
    case CONTROL_DIGITALIN8_STATUS:
      return (GetInputStatus (kDIO8, pulValue));

    case CONTROL_DIO1_SRC_MODE:
      return (GetSRCMode (kDIO1, pulValue));
    case CONTROL_DIO2_SRC_MODE:
      return (GetSRCMode (kDIO2, pulValue));
    case CONTROL_DIO3_SRC_MODE:
      return (GetSRCMode (kDIO3, pulValue));
    case CONTROL_DIO4_SRC_MODE:
      return (GetSRCMode (kDIO4, pulValue));
    case CONTROL_DIO5_SRC_MODE:
      return (GetSRCMode (kDIO5, pulValue));
    case CONTROL_DIO6_SRC_MODE:
      return (GetSRCMode (kDIO6, pulValue));
    case CONTROL_DIO7_SRC_MODE:
      return (GetSRCMode (kDIO7, pulValue));
    case CONTROL_DIO8_SRC_MODE:
      return (GetSRCMode (kDIO8, pulValue));

    case CONTROL_DIO1_SRC_RATIO:
      return (GetSRCRatio (kDIO1, pulValue));
    case CONTROL_DIO2_SRC_RATIO:
      return (GetSRCRatio (kDIO2, pulValue));
    case CONTROL_DIO3_SRC_RATIO:
      return (GetSRCRatio (kDIO3, pulValue));
    case CONTROL_DIO4_SRC_RATIO:
      return (GetSRCRatio (kDIO4, pulValue));
    case CONTROL_DIO5_SRC_RATIO:
      return (GetSRCRatio (kDIO5, pulValue));
    case CONTROL_DIO6_SRC_RATIO:
      return (GetSRCRatio (kDIO6, pulValue));
    case CONTROL_DIO7_SRC_RATIO:
      return (GetSRCRatio (kDIO7, pulValue));
    case CONTROL_DIO8_SRC_RATIO:
      return (GetSRCRatio (kDIO8, pulValue));

    case CONTROL_DIGITALOUT1_STATUS:	// these controls ARE contiguous so we can group them together...
    case CONTROL_DIGITALOUT2_STATUS:
    case CONTROL_DIGITALOUT3_STATUS:
    case CONTROL_DIGITALOUT4_STATUS:
    case CONTROL_DIGITALOUT5_STATUS:
    case CONTROL_DIGITALOUT6_STATUS:
    case CONTROL_DIGITALOUT7_STATUS:
    case CONTROL_DIGITALOUT8_STATUS:
      return (GetOutputStatus
	      (usControl - CONTROL_DIGITALOUT1_STATUS, pulValue));

    case CONTROL_AES50_STATUS:
      return (GetAES50Status (pulValue));

    default:
      return (HSTATUS_INVALID_MIXER_CONTROL);
    }
  return (HSTATUS_OK);
}
