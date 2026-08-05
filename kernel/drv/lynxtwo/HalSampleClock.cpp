/****************************************************************************
 HalSampleClock.cpp

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
 Feb 01 05 DAH	Changed how sample clock source is set, so a preferred source
				is selected, and the HAL determines the correct source
 Nov 20 03 DAH	Changed override of sample rate to be disabled if bForce is set.
****************************************************************************/

#include <StdAfx.h>
#include "HalAdapter.h"

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::Open (PHALADAPTER pHalAdapter)
/////////////////////////////////////////////////////////////////////////////
{
  PLYNXTWOREGISTERS pRegisters;

  //cmn_err((CE_WARN,"CHalSampleClock::Open\n"));

  m_pHalAdapter = pHalAdapter;
  pRegisters = m_pHalAdapter->GetRegisters ();

  m_RegPLLCTL.Init (m_pHalAdapter, &pRegisters->PLLCTL, REG_WRITEONLY);

  if (m_pHalAdapter->HasSynchroLock ())
    {
      m_RegVCXOCTLWrite.Init (m_pHalAdapter, &pRegisters->VCXOCTL, REG_WRITEONLY);	// Setting an initial value in Init doesn't do a write to hardware
      m_RegVCXOCTLRead.Init (m_pHalAdapter, &pRegisters->VCXOCTL);	// Setting an initial value in Init doesn't do a write to hardware
    }

  m_bIsAES16 = FALSE;
  if (m_pHalAdapter->IsAES16 () || m_pHalAdapter->IsAES16e ())
    m_bIsAES16 = TRUE;

  SetDefaults ();

  //cmn_err((CE_WARN,"CHalSampleClock::Open Complete\n"));

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::Close ()
/////////////////////////////////////////////////////////////////////////////
{
  return (HSTATUS_OK);
}

// Internal 32MHz Clock Standard Frequency PLL Values
// M,N,and P values selected for minimal PLL jitter and frequency error
static SRREGS tbl32MHzClk[] = {
  // Fsr     M     N    PX2  
  //------------- SPEED 1X-------------------------
  {8000, 125, 128, SR_P16},	// exact
  {11025, 107, 151, SR_P16},	// 10.6 ppm error
  {11025, 214, 151, SR_P8},	// 10.6 ppm error (must be second so it is only selected for non-P16 firmware)
  {22050, 107, 151, SR_P8},	// 10.6 ppm error
  {32000, 125, 128, SR_P4},	// exact
  {44056, 266, 375, SR_P4},	// 12.5 ppm error
  {44100, 107, 151, SR_P4},	// 10.6 ppm error
  {44144, 206, 291, SR_P4},	// 9.5  ppm error
  {48000, 125, 192, SR_P4},	// exact
  //------------- SPEED 2X-------------------------
  {51200, 224, 367, SR_P4},	// 4.36 ppm error, better jitter than 0ppm
  {52428, 211, 354, SR_P4},	// 2.08ppm
  {88112, 266, 375, SR_P2},	// 12.5 ppm error
  {88200, 107, 151, SR_P2},	// 10.6 ppm error
  {88288, 206, 291, SR_P2},	// 9.5  ppm error
  {96000, 125, 192, SR_P2},	// exact
  //------------- SPEED 4X-------------------------
  {102400, 224, 367, SR_P4},	// 4.36ppm
  {104857, 211, 354, SR_P4},	// 2.08ppm
  {176224, 266, 375, SR_P2},	// 12.5 ppm error       128FS runs at same rate as 2X speed
  {176400, 107, 151, SR_P2},	// 10.6 ppm error       128FS runs at same rate as 2X speed
  {176576, 206, 291, SR_P2},	// 9.5  ppm error       128FS runs at same rate as 2X speed
  {192000, 125, 192, SR_P2},	// exact                        128FS runs at same rate as 2X speed
  {200000, 125, 200, SR_P2},	// exact                        128FS runs at same rate as 2X speed
  {-1, 125, 0, SR_P2}		// this value is used to determine the incremental rates
};

// AES16 Internal Clock
static SRREGS tbl40MHzClk[] = {
  // Fsr     M     N    PX2               // NOTE: The AES16 doesn't use the P register, divisor is controlled by SPEED bits
  //------------- SPEED 1X-------------------------
  {32000, 177, 145, SR_P4},	// 11.03 ppm error
  {44056, 133, 150, SR_P4},	// 12.45 ppm error
  {44100, 442, 499, SR_P4},	//  0.64 ppm error
  {44144, 123, 139, SR_P4},	//  4.51 ppm error
  {48000, 118, 145, SR_P4},	// 11.03 ppm error
  //------------- SPEED 2X-------------------------
  {88112, 133, 150, SR_P2},	// 12.45 ppm error
  {88200, 442, 499, SR_P2},	//  0.64 ppm error
  {88288, 123, 139, SR_P2},	//  4.51 ppm error
  {96000, 118, 145, SR_P2},	// 11.03 ppm error
  //------------- SPEED 4X-------------------------
  {176224, 133, 150, SR_P2},	// 12.45 ppm error      128FS runs at same rate as 2X speed
  {176400, 442, 499, SR_P2},	//  0.64 ppm error      128FS runs at same rate as 2X speed
  {176576, 123, 139, SR_P2},	//  4.51 ppm error      128FS runs at same rate as 2X speed
  {192000, 118, 145, SR_P2},	// 11.03 ppm error      128FS runs at same rate as 2X speed
  {200000, 100, 128, SR_P2},	// exact                        128FS runs at same rate as 2X speed
  {-1, 156, 0, SR_P2}		// this value is used to determine the incremental rates
};

// AES16e Internal Clock
static SRREGS tbl50MHzClk[] = {
  // Fsr     M     N    PX2               // NOTE: The AES16e doesn't use the P register, divisor is controlled by SPEED bits
  //------------- SPEED 1X-------------------------
  {32000, 737, 483, SR_P4},	// 0.66 ppm error
  {44056, 573, 517, SR_P4},	// 2.08 ppm error
  {44100, 568, 513, SR_P4},	// 1.12 ppm error
  {44144, 344, 311, SR_P4},	// 0.72 ppm error
  {48000, 825, 811, SR_P4},	// 9.86 ppm error
  //------------- SPEED 2X-------------------------
  {88112, 573, 517, SR_P2},	// 2.08 ppm error
  {88200, 568, 513, SR_P2},	// 1.12 ppm error
  {88288, 344, 311, SR_P2},	// 0.72 ppm error
  {96000, 825, 811, SR_P2},	// 9.86 ppm error
  //------------- SPEED 4X-------------------------
  {176224, 573, 517, SR_P2},	// 2.08 ppm error       128FS runs at same rate as 2X speed
  {176400, 568, 513, SR_P2},	// 1.12 ppm error       128FS runs at same rate as 2X speed
  {176576, 344, 311, SR_P2},	// 0.72 ppm error       128FS runs at same rate as 2X speed
  {192000, 825, 811, SR_P2},	// 9.86 ppm error       128FS runs at same rate as 2X speed
  {200000, 125, 128, SR_P2},	// exact                        128FS runs at same rate as 2X speed
  {-1, 195, 0, SR_P2}		// this value is used to determine the incremental rates
};

// 1X Pixel Clock
static SRREGS tbl13p5MHzClk[] = {
  // Fsr     M     N    PX2   
  {8000, 220, 534, SR_P16},	// 5.54  ppm error
  {11025, 142, 475, SR_P16},	// 1.123 ppm error
  {11025, 284, 475, SR_P8},	// 1.123 ppm error (must be second so it is only selected for non-P16 firmware)
  {22050, 142, 475, SR_P8},	// 1.123 ppm error
  {32000, 220, 534, SR_P4},	// 5.54  ppm error
  {44056, 158, 528, SR_P4},	// 12.8  ppm error
  {44100, 142, 475, SR_P4},	// 1.123 ppm error
  {44144, 155, 519, SR_P4},	// 5.09  ppm error
  {48000, 181, 659, SR_P4},	// 1.349 ppm error
  {88112, 158, 528, SR_P2},	// 12.8 ppm error
  {88200, 142, 475, SR_P2},	// 1.123 ppm error
  {88288, 155, 519, SR_P2},	// 5.09  ppm error
  {96000, 181, 659, SR_P2},	// 1.349 ppm error
  {176224, 158, 528, SR_P2},	// 12.8 ppm
  {176400, 142, 475, SR_P2},	// 1.12 ppm
  {176576, 155, 519, SR_P2},	// 5.09 ppm
  {192000, 181, 659, SR_P2},	// 1.349 ppm
  {200000, 135, 512, SR_P2},	// 0 ppm
  {-1, 53, 0, SR_P2}		// this value is used to determine the incremental rates
};

// Video Clock
static SRREGS tbl24MHzClk[] = {
  // Fsr     M     N    PX2  
  {8000, 750, 1024, SR_P16},	// 0 ppm
  {11025, 549, 1033, SR_P16},	// 0 ppm
  {11025, 625, 588, SR_P8},	// 0 ppm (must be second so it is only selected for non-P16 firmware)
  {22050, 549, 1033, SR_P8},	// 1.5 ppm
  {32000, 750, 1024, SR_P4},	// 0 ppm
  {44056, 582, 1094, SR_P4},	// 1.3 ppm - 44056
  {44100, 549, 1033, SR_P4},	// 1.5 ppm, better jitter than 0 ppm
  {44144, 575, 1083, SR_P4},	// 0.5 ppm - 44144
  {48000, 500, 1024, SR_P4},	// 0 ppm
  {88112, 582, 1094, SR_P2},	// 1.3 ppm
  {88200, 549, 1033, SR_P2},	// 1.5 ppm
  {88288, 575, 1083, SR_P2},	// 0.5 ppm
  {96000, 500, 1024, SR_P2},	// 0ppm
  {176224, 582, 1094, SR_P2},	// 1.3 ppm
  {176400, 549, 1033, SR_P2},	// 1.5 ppm
  {176576, 575, 1083, SR_P2},	// 0.5 ppm
  {192000, 500, 1024, SR_P2},	// 0 ppm
  {200000, 480, 1024, SR_P2},	// 0 ppm
  {-1, 94, 0, SR_P2}		// this value is used to determine the incremental rates
};

// 2X Pixel Clock
static SRREGS tbl27MHzClk[] = {
  // Fsr     M     N    PX2  
  {8000, 440, 534, SR_P16},	// 5.54 ppm error
  {11025, 284, 475, SR_P16},	// 1.12 ppm error
  {11025, 568, 475, SR_P8},	// 1.12 ppm error (must be second so it is only selected for non-P16 firmware)
  {22050, 284, 475, SR_P8},	// 1.12 ppm error
  {32000, 440, 534, SR_P4},	// 5.54 ppm error
  {44056, 319, 533, SR_P4},	// 10.9 ppm error
  {44100, 284, 475, SR_P4},	// 1.12  ppm error
  {44144, 221, 370, SR_P4},	// 3.63  ppm error
  {48000, 362, 659, SR_P4},	// 1.3 ppm 
  {88112, 319, 533, SR_P2},	// 10.9 ppm error
  {88200, 284, 475, SR_P2},	// 1.12 ppm error
  {88288, 221, 370, SR_P2},	// 3.63 ppm error
  {96000, 362, 659, SR_P2},	// 1.3 ppm error
  {176224, 319, 533, SR_P2},	// 10.9 ppm
  {176400, 284, 475, SR_P2},	// 1.12 ppm error
  {176576, 221, 370, SR_P2},	// 3.63 ppm
  {192000, 362, 659, SR_P2},	// 1.3 ppm
  {200000, 135, 256, SR_P2},	// 0 ppm
  {-1, 105, 0, SR_P2}		// this value is used to determine the incremental rates
};

// Specific Values for TI Video PLL
static SRREGS tblTIPll_27MHzClk[] = {
  // Fsr     M     N    PX2  
  {8000, 543, 659, SR_P16},	// 1.35 ppm error
  {11025, 568, 950, SR_P16},	// 1.12  ppm error
  {11025, 568, 475, SR_P8},	// 1.12 ppm error (must be second so it is only selected for non-P16 firmware)
  {22050, 568, 950, SR_P8},	// 1.12  ppm error
  {32000, 543, 659, SR_P4},	// 1.35 ppm error
  {44056, 319, 533, SR_P4},	// 10.9 ppm error
  {44100, 568, 950, SR_P4},	// 1.12  ppm error
  {44144, 221, 370, SR_P4},	// 3.63  ppm error
  {48000, 362, 659, SR_P4},	// 1.35 ppm 
  {88112, 319, 533, SR_P2},	// 10.9 ppm error
  {88200, 568, 950, SR_P2},	// 1.12  ppm error
  {88288, 221, 370, SR_P2},	// 3.63 ppm error
  {96000, 362, 659, SR_P2},	// 1.3 ppm error
  {176224, 319, 533, SR_P2},	// 10.9 ppm
  {176400, 568, 950, SR_P2},	// 1.12  ppm error
  {176576, 221, 370, SR_P2},	// 3.63 ppm
  {192000, 362, 659, SR_P2},	// 1.3 ppm
  {200000, 270, 512, SR_P2},	// 0 ppm
  {-1, 105, 0, SR_P2}		// this value is used to determine the incremental rates
};

#define LYNX_ARRAY_SIZE( a )		(sizeof(a)/sizeof(a[0]))

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::Set (LONG lRate, BOOLEAN bForceChange)
/////////////////////////////////////////////////////////////////////////////
{
  PLLCLOCKINFO ClockInfo;
  PHAL8420 pCS8420 = m_pHalAdapter->Get8420 ();
  PHAL4114 pAK4114 = m_pHalAdapter->Get4114 ();
  PHALAES16E pAES16eDIO = m_pHalAdapter->GetAES16eDIO ();
  PHALMIXER pMixer = m_pHalAdapter->GetMixer ();
  LONG lSource;
  LONG lReference = MIXVAL_CLKREF_AUTO;
  LONG lMin, lMax;
  LONG lOrgSource, lOrgRate;

  // don't allow ANY changes to the sample clock rate if rate lock is on
  if (m_bRateLock && (lRate != m_lRate))
    {
      cmn_err (CE_WARN, "CHalSampleClock::Set: Rate Lock is ON!\n");
      return (HSTATUS_MIXER_LOCKED);
    }

  //cmn_err((CE_WARN,"CHalSampleClock::Set %ld\n", lRate ));

  GetMinMax (&lMin, &lMax);

  // if the requested sample rate is less than the minimum rate, set it at the minimum
  if (lRate < lMin)
    lRate = lMin;

  // if the requested sample rate is greater than the maximum rate, set it at the maximum
  if (lRate > lMax)
    lRate = lMax;

  // start with the preferred source
  lSource = m_lPreferredSource;

  // correct for any code that isn't AES16 aware
  if (m_bIsAES16 && (lSource < MIXVAL_AES16_CLKSRC_INTERNAL))
    lSource += MIXVAL_AES16_CLKSRC_INTERNAL;

  // always check the clock rates if external
  if (GetClockRate (&lRate, &lSource, &lReference))
    {
      // This can only fail if the external clock select is not locked
      cmn_err (CE_WARN,
	       "CHalSampleClock::GetClockRate Failed! Reverting to Internal Clock.\n");

      // switch back to internal clock
      if (lSource < MIXVAL_AES16_CLKSRC_INTERNAL)	// Is this a LynxTWO/L22?
	{
	  lSource = MIXVAL_L2_CLKSRC_INTERNAL;	// Yes
	  lReference = MIXVAL_CLKREF_AUTO;
	}
      else
	{
	  lSource = MIXVAL_AES16_CLKSRC_INTERNAL;	// No, AES16
	  lReference = MIXVAL_CLKREF_AUTO;
	}

      // make sure this goes through
      bForceChange = TRUE;
    }

  // if there is no change from current setting, just return.
  if ((m_lRate == lRate) && (m_lSource == lSource) && (m_lReference == lReference) && !bForceChange)	// if Force is set, always run the entire routine
    {
      //cmn_err((CE_WARN,"CHalSampleClock::Set: Requested Sample Rate already active!\n"));
      return (HSTATUS_OK);
    }

  // don't allow the change to occur unless bForceChange is TRUE or m_bAllowClockChangeIfActive is TRUE
  if (!bForceChange && !m_bAllowClockChangeIfActive)
    {
      // make sure all the devices are idle
      if (m_pHalAdapter->GetNumActiveWaveDevices ())
	{
	  cmn_err (CE_WARN,
		   "CHalSampleClock::Set: Device in use. Cannot change sample rate.\n");
	  return (HSTATUS_ALREADY_IN_USE);
	}
    }

  //cmn_err((CE_WARN,"CHalSampleClock::Set %ld\n", lRate ));

  // start off with everything turned off
  RtlZeroMemory (&ClockInfo, sizeof (PLLCLOCKINFO));

  if (m_bIsAES16)
    ClockInfo.ulClkSrc = lSource - MIXVAL_AES16_CLKSRC_INTERNAL;
  else
    ClockInfo.ulClkSrc = lSource;

  switch (lSource)
    {
    case MIXVAL_L2_CLKSRC_INTERNAL:
    case MIXVAL_AES16_CLKSRC_INTERNAL:
      //cmn_err((CE_WARN,"MIXVAL_L2_CLKSRC_INTERNAL\n"));
      lReference = MIXVAL_CLKREF_AUTO;
      switch (m_pHalAdapter->GetXtalSpeed ())
	{
	case 32000000:
	  GetClockInfo (&lRate, tbl32MHzClk, &ClockInfo,
			LYNX_ARRAY_SIZE (tbl32MHzClk));
	  break;
	case 40000000:
	  GetClockInfo (&lRate, tbl40MHzClk, &ClockInfo,
			LYNX_ARRAY_SIZE (tbl40MHzClk));
	  break;
	case 50000000:
	  GetClockInfo (&lRate, tbl50MHzClk, &ClockInfo,
			LYNX_ARRAY_SIZE (tbl50MHzClk));
	  break;
	}

      // if this is an AES16, and an Aurora is connected, make sure it isn't on internal
      // only do the check if Force isn't set
      if (m_bIsAES16 && !bForceChange && m_pHalAdapter->IsOpen ())
	{
	  PHALAURORA pHalAurora = m_pHalAdapter->GetAurora ();
	  if (pHalAurora)
	    {
	      ULONG ulProductID;
	      // this is going to call either Hal4114 or HalAES16e, which upon driver load has not been opened yet
	      pHalAurora->GetProductID (&ulProductID);
	      if (ulProductID)
		{
		  ULONG ulAuroraClockSource;

		  // first make sure the registers by reading the value twice
		  pHalAurora->GetClockSource (&ulAuroraClockSource);

		  pHalAurora->GetClockSource (&ulAuroraClockSource);

		  if (ulAuroraClockSource == MIXVAL_AURORA_CLKSRC_INTERNAL)
		    {
		      cmn_err (CE_WARN,
			       "CHalSampleClock::Set: Detected AES16/Aurora both set to INTERNAL!\n");
		      return (HSTATUS_INVALID_MODE);
		    }
		}
	    }
	}
      break;
    case MIXVAL_L2_CLKSRC_EXTERNAL:
    case MIXVAL_L2_CLKSRC_HEADER:
    case MIXVAL_AES16_CLKSRC_EXTERNAL:
    case MIXVAL_AES16_CLKSRC_HEADER:
      //cmn_err((CE_WARN,"MIXVAL_L2_CLKSRC_EXTERNAL/HEADER\n"));
      // make sure we allow the change from INTERNAL or DIGITAL
      if (lReference == MIXVAL_CLKREF_AUTO)
	lReference = MIXVAL_CLKREF_27MHZ;

      switch (lReference)
	{
	case MIXVAL_CLKREF_WORD:
	  //cmn_err((CE_WARN,"MIXVAL_CLKREF_WORD\n"));
	  // Just set the word bit here, the rest will get taken care of later.
	  ClockInfo.ulWord = 1;
	  break;
	case MIXVAL_CLKREF_WORD256:
	  //cmn_err((CE_WARN,"MIXVAL_CLKREF_WORD256\n"));

	  //Set M,N,P for quad, single, double speed
	  if (lRate > 100000)
	    {
	      ClockInfo.ulM = 96;
	      ClockInfo.ulN = 96;
	      ClockInfo.ulP = SR_P2;
	    }
	  else if (lRate > 50000)
	    {
	      ClockInfo.ulM = 48;
	      ClockInfo.ulN = 96;
	      ClockInfo.ulP = SR_P2;
	    }
	  else
	    {
	      ClockInfo.ulM = 24;
	      ClockInfo.ulN = 96;
	      ClockInfo.ulP = SR_P4;
	    }

	  break;
	case MIXVAL_CLKREF_13p5MHZ:
	  //cmn_err((CE_WARN,"MIXVAL_CLKREF_13p5MHZ\n"));
	  GetClockInfo (&lRate, tbl13p5MHzClk, &ClockInfo,
			LYNX_ARRAY_SIZE (tbl13p5MHzClk));
	  break;
	case MIXVAL_CLKREF_27MHZ:
	  //cmn_err((CE_WARN,"MIXVAL_CLKREF_27MHZ\n"));
	  GetClockInfo (&lRate, tbl27MHzClk, &ClockInfo,
			LYNX_ARRAY_SIZE (tbl27MHzClk));
	  break;
	default:
	  cmn_err (CE_WARN, "Invalid lReference [%08lx]\n", lReference);
	  return (HSTATUS_INVALID_MIXER_VALUE);
	}
      break;
    case MIXVAL_L2_CLKSRC_VIDEO:
      //cmn_err((CE_WARN,"MIXVAL_L2_CLKSRC_VIDEO\n"));
      if (m_pHalAdapter->GetDeviceID () == PCIDEVICE_LYNX_L22)
	return (HSTATUS_INVALID_MIXER_VALUE);

      lReference = MIXVAL_CLKREF_AUTO;
      if (m_pHalAdapter->HasTIVideoPLL ())
	GetClockInfo (&lRate, tblTIPll_27MHzClk, &ClockInfo,
		      LYNX_ARRAY_SIZE (tblTIPll_27MHzClk));
      else
	GetClockInfo (&lRate, tbl24MHzClk, &ClockInfo,
		      LYNX_ARRAY_SIZE (tbl24MHzClk));
      break;
    case MIXVAL_L2_CLKSRC_LSTREAM_PORT1:
    case MIXVAL_L2_CLKSRC_LSTREAM_PORT2:
    case MIXVAL_AES16_CLKSRC_LSTREAM:
      //cmn_err((CE_WARN,"MIXVAL_L2_CLKSRC_LSTREAM_PORTX\n"));
      lReference = MIXVAL_CLKREF_WORD;	// force to word
      // Just set the word bit here, the rest will get taken care of later.
      ClockInfo.ulWord = 1;
      break;
    case MIXVAL_AES16_CLKSRC_AES50:	// must come before the digital 4-7 since PMixV3 is a given with AES50
      if (!m_pHalAdapter->HasAES50 ())
	return (HSTATUS_INVALID_MIXER_VALUE);
      // fall-through...
    case MIXVAL_AES16_CLKSRC_DIGITAL_4:
    case MIXVAL_AES16_CLKSRC_DIGITAL_5:
    case MIXVAL_AES16_CLKSRC_DIGITAL_6:
    case MIXVAL_AES16_CLKSRC_DIGITAL_7:
      if (!m_pHalAdapter->HasPMixV3 ())
	return (HSTATUS_INVALID_MIXER_VALUE);
      // fall-through...
    case MIXVAL_AES16_CLKSRC_DIGITAL_2:
    case MIXVAL_AES16_CLKSRC_DIGITAL_3:
      // rev NC AES16 boards can only sync to digital in 1 & 2
      if (m_pHalAdapter->IsAES16 () && !m_pHalAdapter->GetPCBRev ())
	return (HSTATUS_INVALID_MIXER_VALUE);
      // fall-through...
    case MIXVAL_AES16_CLKSRC_DIGITAL_0:
    case MIXVAL_AES16_CLKSRC_DIGITAL_1:
    case MIXVAL_L2_CLKSRC_DIGITAL:
      //cmn_err((CE_WARN,"MIXVAL_L2_CLKSRC_DIGITAL\n"));
      lReference = MIXVAL_CLKREF_AUTO;	// force to auto
      // Just set the word bit here, the rest will get taken care of later.
      ClockInfo.ulWord = 1;
      break;
    default:
      cmn_err (CE_WARN, "Invalid lSource [%08lx]\n", lSource);
      return (HSTATUS_INVALID_MIXER_VALUE);
    }

  // if this is a word clock, check for the 24kHz lower limit
  if ((lReference == MIXVAL_CLKREF_WORD) && (lRate < 24000))
    {
      // if the old reference wasn't WORD
      if (m_lReference != lReference)
	{
	  // set at the minimum
	  lRate = 24000;
	}
      else
	{
	  //cmn_err((CE_WARN,"Invalid Sample Rate for Word Clock!\n"));
	  cmn_err (CE_WARN, "SetSampleRate: Word Clock Below 24000!\n");
	  return (HSTATUS_INVALID_SAMPLERATE);
	}
    }

  // Set the speed based on the sample rate
  if (lRate > 100000)
    ClockInfo.ulSpeed = SR_SPEED_4X;
  else if (lRate > 50000)
    ClockInfo.ulSpeed = SR_SPEED_2X;
  else
    ClockInfo.ulSpeed = SR_SPEED_1X;

  // if the word bit is set, make sure the M, N & P registers are correct for the speed setting
  if (ClockInfo.ulWord)
    {
      ClockInfo.ulM = 1;	// Ignored
      ClockInfo.ulBypassM = 1;

      switch (ClockInfo.ulSpeed)
	{
	case SR_SPEED_1X:
	  ClockInfo.ulN = 1024;
	  ClockInfo.ulP = SR_P4;
	  break;
	case SR_SPEED_2X:
	  ClockInfo.ulN = 512;
	  ClockInfo.ulP = SR_P2;
	  break;
	case SR_SPEED_4X:
	  ClockInfo.ulN = 256;
	  ClockInfo.ulP = SR_P2;
	  break;
	}
    }

  // if this is an AES16, and the speed isn't 1X, and widewire is on, and the digital input is the sample clock source (or external/word)
  // NOTE: WideWire has already been read above
  if (m_bIsAES16 && (lRate > 50000) && m_bWideWireIn
      && ((lSource >= MIXVAL_AES16_CLKSRC_DIGITAL_0)
	  || ((lSource == MIXVAL_AES16_CLKSRC_EXTERNAL)
	      && (lReference == MIXVAL_CLKREF_WORD))))
    {
      ClockInfo.ulN *= 2;	// double the N value
    }

  // change the sample rate in hardware
  //cmn_err((CE_WARN,"Changing Sample Rate in Hardware\n"));
  if (m_pHalAdapter->HasPMixV3 ())	// only cards with PMixV3 also have PLLCTLE
    m_RegPLLCTL =
      MAKE_PLLCTLE (ClockInfo.ulM, ClockInfo.ulBypassM, ClockInfo.ulN,
		    ClockInfo.ulP, ClockInfo.ulClkSrc, ClockInfo.ulWord,
		    ClockInfo.ulSpeed);
  else
    m_RegPLLCTL =
      MAKE_PLLCTL (ClockInfo.ulM, ClockInfo.ulBypassM, ClockInfo.ulN,
		   ClockInfo.ulP, ClockInfo.ulClkSrc, ClockInfo.ulWord,
		   ClockInfo.ulSpeed);

  //set speed in A/D and D/A
  // since these writes are slow, update only if there is a speed change.
  if (m_ulSpeed != ClockInfo.ulSpeed)
    {
      m_pHalAdapter->SetConverterSpeed (lRate);
    }

  // if auto-recalibration is on (which is the default)
  if (m_pHalAdapter->GetAutoRecalibrate ())
    {
      // if P changed or the sample clock changed, recalibrate the converters
      if ((m_lSource != lSource) || (m_lReference != lReference)
	  || (m_ulP != ClockInfo.ulP))
	{
	  m_pHalAdapter->CalibrateConverters ();
	}
    }

  // remember the old values
  lOrgSource = m_lSource;
  lOrgRate = m_lRate;

  // save the new values
  m_lRate = lRate;
  m_lSource = lSource;
  m_lReference = lReference;
  m_ulSpeed = ClockInfo.ulSpeed;
  m_ulP = ClockInfo.ulP;

  // let the 8420 update the digital out channel status.  Since the 8420 is opened after 
  // the sample clock, make sure we don't call the 8420 until after the adapter is opened.
  if (m_pHalAdapter->IsOpen ())
    {
      if (m_pHalAdapter->HasSynchroLock ())
	{
	  // Note: VCXOLOEN and VCXOHIEN bits are only used when the FPGA detects that the clock source is internal 
	  switch (m_lRate)
	    {
	    case 44100:
	    case 88200:
	    case 176400:
	      m_RegVCXOCTLWrite.Write (REG_VCXOCTL_VCXOLOEN,
				       (REG_VCXOCTL_VCXOLOEN |
					REG_VCXOCTL_VCXOHIEN));
	      break;
	    case 48000:
	    case 96000:
	    case 192000:
	      m_RegVCXOCTLWrite.Write (REG_VCXOCTL_VCXOHIEN,
				       (REG_VCXOCTL_VCXOLOEN |
					REG_VCXOCTL_VCXOHIEN));
	      break;
	    default:
	      m_RegVCXOCTLWrite.Write (0, (REG_VCXOCTL_VCXOLOEN | REG_VCXOCTL_VCXOHIEN));	// turn both bits off
	      break;
	    }

	  // if the clock source is changing...
	  if (lOrgSource != lSource)
	    {
	      // if the current clock source is AES50, turn off SynchroLock
	      if (lSource == MIXVAL_AES16_CLKSRC_AES50)
		{
		  m_RegVCXOCTLWrite.BitSet (REG_VCXOCTL_SLOCK, FALSE);
		}
	      else
		{
		  m_RegVCXOCTLWrite.BitSet (REG_VCXOCTL_SLOCK,
					    m_bSynchroLock);
		}
	    }
	}

      if (pCS8420)
	pCS8420->SampleClockChanged (m_lRate, m_lSource);
      if (pAK4114)
	pAK4114->SampleClockChanged (m_lRate, m_lSource);	// widewire must be set in hardware in this code...
      if (pAES16eDIO)
	pAES16eDIO->SampleClockChanged (m_lRate, m_lSource);	// widewire must be set in hardware in this code...
      m_pHalAdapter->GetLStream ()->SampleClockChanged (m_lRate, m_lSource);
    }

  // inform the driver of the changes
  if (lOrgSource != lSource)
    {
      pMixer->ControlChanged (LINE_ADAPTER, LINE_NO_SOURCE,
			      CONTROL_CLOCKSOURCE);
    }
  if (lOrgRate != lRate)
    {
      pMixer->ControlChanged (LINE_ADAPTER, LINE_NO_SOURCE,
			      CONTROL_CLOCKRATE);
      pMixer->ControlChanged (LINE_ADAPTER, LINE_NO_SOURCE,
			      CONTROL_CLOCKRATE_SELECT);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::Get (LONG * plRate)
/////////////////////////////////////////////////////////////////////////////
{
  *plRate = m_lRate;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::Get (LONG * plRate, LONG * plSource)
// Always returns the CURRENT clock source, not the preferred source
/////////////////////////////////////////////////////////////////////////////
{
  *plRate = m_lRate;
  *plSource = m_lSource;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalSampleClock::GetClockRate (LONG * plRate, LONG * plSource,
				 LONG * plReference)
// Given a specific rate, source and reference, this function determines if they are valid
// If not, it makes changes to each of the three input variables
/////////////////////////////////////////////////////////////////////////////
{
  PHAL8420 pCS8420 = m_pHalAdapter->Get8420 ();
  PHAL4114 pAK4114 = m_pHalAdapter->Get4114 ();
  PHALAES16E pAES16eDIO = m_pHalAdapter->GetAES16eDIO ();
  //ULONG         bWideWireIn = FALSE;
  LONG lMeasuredRate;
  ULONG ulFrequency;
  USHORT usRegister;

  /*
     if( pAK4114 )
     {
     // read the wide wire bit for use later on...
     pAK4114->GetWideWireIn( &bWideWireIn );
     }
   */

  switch (*plSource)
    {
    case MIXVAL_L2_CLKSRC_DIGITAL:
      // if we are requesting the clock source to switch to Digital, make sure the Digital Input is locked
      if (pCS8420)
	{
	  // make sure the sample rate matches the digital input rate (Normalize gets called for us here...)
	  pCS8420->GetInputSampleRate (&lMeasuredRate);

	  if (!lMeasuredRate)
	    {
	      cmn_err (CE_WARN,
		       "CHalSampleClock::GetClockRate: CS8420 In not locked!\n");
	      return (HSTATUS_INVALID_MODE);
	    }

	  // force the sample rate to the digital input rate
	  *plRate = lMeasuredRate;
	}
      break;

    case MIXVAL_AES16_CLKSRC_DIGITAL_0:
    case MIXVAL_AES16_CLKSRC_DIGITAL_1:
    case MIXVAL_AES16_CLKSRC_DIGITAL_2:
    case MIXVAL_AES16_CLKSRC_DIGITAL_3:
    case MIXVAL_AES16_CLKSRC_DIGITAL_4:
    case MIXVAL_AES16_CLKSRC_DIGITAL_5:
    case MIXVAL_AES16_CLKSRC_DIGITAL_6:
    case MIXVAL_AES16_CLKSRC_DIGITAL_7:
      // if we are requesting the clock source to switch to Digital, make sure the Digital Input is locked
      if (pAK4114 || pAES16eDIO)
	{
	  // make sure the sample rate matches the digital input rate (Normalize gets called for us here...)
	  if (pAK4114)
	    pAK4114->GetInputRate ((*plSource -
				    MIXVAL_AES16_CLKSRC_DIGITAL_0),
				   &lMeasuredRate);
	  if (pAES16eDIO)
	    pAES16eDIO->GetInputRate ((*plSource -
				       MIXVAL_AES16_CLKSRC_DIGITAL_0),
				      &lMeasuredRate);

	  if (!lMeasuredRate)
	    {
	      cmn_err (CE_WARN,
		       "CHalSampleClock::GetClockRate: Digital In not locked!\n");
	      return (HSTATUS_INVALID_MODE);
	    }

	  // wire wire has already been read above
	  if (m_bWideWireIn)
	    {
	      //cmn_err((CE_WARN,"WideWire is on, doubling the lMeasuredRate %ld to %ld\n", lMeasuredRate, lMeasuredRate*2 )); 
	      lMeasuredRate *= 2;
	    }

	  PHALAURORA pHalAurora = m_pHalAdapter->GetAurora ();
	  if (pHalAurora && m_pHalAdapter->IsOpen ())
	    {
	      ULONG ulProductID;
	      pHalAurora->GetProductID (&ulProductID);
	      if (ulProductID)
		{
		  ULONG ulAuroraClockSource;

		  // first make sure the registers by reading the value twice
		  pHalAurora->GetClockSource (&ulAuroraClockSource);

		  pHalAurora->GetClockSource (&ulAuroraClockSource);

		  switch (ulAuroraClockSource)
		    {
		    case MIXVAL_AURORA_CLKSRC_AESA:
		    case MIXVAL_AURORA_CLKSRC_AESB:
		      cmn_err (CE_WARN,
			       "CHalSampleClock::GetClockRate: Detected AES16/Aurora Clock Loop!\n");
		      return (HSTATUS_INVALID_MODE);
		      //SetPreferredSource( MIXVAL_AES16_CLKSRC_INTERNAL );
		      //return( Set( lRate, bForceChange ) );
		      //return( HSTATUS_INVALID_MODE );
		    }
		}
	    }

	  // force the sample rate to the digital input rate
	  *plRate = lMeasuredRate;
	}
      break;

    case MIXVAL_AES16_CLKSRC_AES50:
      if (pAES16eDIO)
	{
	  ULONG ulRate;

	  m_pHalAdapter->GetFrequencyCounter (AES16_FREQCOUNTER_AES50,
					      &ulRate);
	  m_pHalAdapter->NormalizeFrequency (ulRate, &lMeasuredRate);

	  if (!lMeasuredRate)
	    {
	      cmn_err (CE_WARN,
		       "CHalSampleClock::GetClockRate: AES50 In not locked!\n");
	      return (HSTATUS_INVALID_MODE);
	    }

	  // force the sample rate to the digital input rate
	  *plRate = lMeasuredRate;
	}
      break;

    case MIXVAL_L2_CLKSRC_EXTERNAL:
    case MIXVAL_L2_CLKSRC_HEADER:
    case MIXVAL_AES16_CLKSRC_EXTERNAL:
    case MIXVAL_AES16_CLKSRC_HEADER:
      // External Clock
      switch (*plSource)
	{
	case MIXVAL_L2_CLKSRC_EXTERNAL:
	  usRegister = L2_FREQCOUNTER_EXTERNAL;
	  break;
	case MIXVAL_L2_CLKSRC_HEADER:
	  usRegister = L2_FREQCOUNTER_HEADER;
	  break;
	case MIXVAL_AES16_CLKSRC_EXTERNAL:
	  usRegister = AES16_FREQCOUNTER_EXTERNAL;
	  break;
	case MIXVAL_AES16_CLKSRC_HEADER:
	  usRegister = AES16_FREQCOUNTER_HEADER;
	  break;
	}

      // get the current clock rate of that port
      m_pHalAdapter->GetFrequencyCounter (usRegister, &ulFrequency);

      // normalize the frequency
      m_pHalAdapter->NormalizeFrequency (ulFrequency, &lMeasuredRate,
					 plReference);

      if ((*plReference == MIXVAL_CLKREF_13p5MHZ)
	  || (*plReference == MIXVAL_CLKREF_27MHZ))
	{
	  // if we are not running with a word clock, then lMeasuredRate will come back 
	  // with zero so we must ignore it. Because of this there really isn't anything 
	  // we can do here so the plRate that was passed in will be what is passed back
	  // without change.
	}
      else
	{
	  // plReference will only ever come back with MIXVAL_CLKREF_AUTO if lMeasuredRate
	  // is also zero, so we catch that here.

	  // if there is no rate at all, don't allow the switch.  We also disallow anything
	  // under 24kHz since word clocks can't do that...
	  if (!lMeasuredRate || (lMeasuredRate < 24000))
	    {
	      cmn_err (CE_WARN,
		       "CHalSampleClock::GetClockRate: External or Header Port not locked!\n");
	      return (HSTATUS_INVALID_MODE);
	    }

	  // we now can only have a word clock, since AUTO is excluded above, and we are in
	  // the else case of 13.5MHz or 27MHz and those are the only 5 possiblites.

	  // only if the reference is WORD or WORD256 can we determine the correct sample rate
	  // If we are on an AES16 & WideWire is on (it has already been read above) double the rate
	  if (m_bIsAES16 && (*plSource == MIXVAL_AES16_CLKSRC_EXTERNAL)
	      && m_bWideWireIn)
	    {
	      //cmn_err((CE_WARN,"WideWire is on, doubling the lMeasuredRate %ld to %ld\n", lMeasuredRate, lMeasuredRate*2 )); 
	      lMeasuredRate *= 2;
	    }

	  // force the sample rate to the measured clock rate
	  *plRate = lMeasuredRate;
	}
      break;

    case MIXVAL_L2_CLKSRC_VIDEO:
      // get the current clock rate of the video input
      m_pHalAdapter->GetFrequencyCounter (L2_FREQCOUNTER_VIDEO, &ulFrequency);

      // if there is no rate at all, don't allow the switch
      if (!ulFrequency)
	{
	  cmn_err (CE_WARN,
		   "CHalSampleClock::GetClockRate: Video Input not locked!\n");
	  return (HSTATUS_INVALID_MODE);
	}

      // if we get here, then we have a valid video input, plRate should be fine
      *plReference = MIXVAL_CLKREF_AUTO;
      break;

    case MIXVAL_L2_CLKSRC_LSTREAM_PORT1:
    case MIXVAL_L2_CLKSRC_LSTREAM_PORT2:
    case MIXVAL_AES16_CLKSRC_LSTREAM:
      // if we are requesting the clock source to switch to LStream 1 or 2, check the rates to make sure they are valid
      ULONG ulPort;		// DAH Mar 13, 2006 see no reason for this: = (*plSource == MIXVAL_L2_CLKSRC_LSTREAM_PORT1) ? LSTREAM_BRACKET : LSTREAM_HEADER;
      ULONG ulLStreamDeviceID;

      switch (*plSource)
	{
	case MIXVAL_L2_CLKSRC_LSTREAM_PORT1:
	  usRegister = L2_FREQCOUNTER_LSTREAM1;
	  ulPort = LSTREAM_BRACKET;
	  break;
	case MIXVAL_L2_CLKSRC_LSTREAM_PORT2:
	  usRegister = L2_FREQCOUNTER_LSTREAM2;
	  ulPort = LSTREAM_HEADER;
	  break;
	case MIXVAL_AES16_CLKSRC_LSTREAM:
	  usRegister = AES16_FREQCOUNTER_LSTREAM;
	  ulPort = LSTREAM_HEADER;
	  break;
	}
      // go see what is plugged into the requested LStream port
      ulLStreamDeviceID = m_pHalAdapter->GetLStream ()->GetDeviceID (ulPort);

      // NOTE: If there is no LStream device ID, we go ahead and let things get set - even though we probably shouldn't
      if (ulLStreamDeviceID)
	{
	  if (ulLStreamDeviceID == REG_LSDEVID_LSAES)
	    {
	      // Change the source to HEADER and measure the frequency from the header
	      if (*plSource == MIXVAL_AES16_CLKSRC_LSTREAM)
		{
		  *plSource = MIXVAL_AES16_CLKSRC_HEADER;
		  *plReference = MIXVAL_CLKREF_WORD;
		  usRegister = AES16_FREQCOUNTER_HEADER;
		}
	      else
		{
		  *plSource = MIXVAL_L2_CLKSRC_HEADER;
		  *plReference = MIXVAL_CLKREF_WORD;
		  usRegister = L2_FREQCOUNTER_HEADER;
		}
	    }

	  // get the current clock rate of that port
	  m_pHalAdapter->GetFrequencyCounter (usRegister, &ulFrequency);

	  // normalize the frequency (we don't care about the reference because we know this is a word clock)
	  m_pHalAdapter->NormalizeFrequency (ulFrequency, &lMeasuredRate);

	  // if there is no rate at all, don't allow the switch
	  if (!lMeasuredRate)
	    {
	      cmn_err (CE_WARN,
		       "CHalSampleClock::GetClockRate: LStream Port not locked!\n");
	      return (HSTATUS_INVALID_MODE);
	    }

	  // if there is an LS-ADAT connected, allow 2X & 4X rates to pass
	  if (ulLStreamDeviceID == REG_LSDEVID_LSADAT)
	    {
	      if (*plRate != (lMeasuredRate * 2))
		{
		  if (*plRate != (lMeasuredRate * 4))
		    {
		      *plRate = lMeasuredRate;
		    }
		}
	    }
	  else
	    {
	      // force the sample rate to the LStream rate
	      *plRate = lMeasuredRate;
	    }
	}
      break;
    }				// switch

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::GetMinMax (LONG * plMin, LONG * plMax)
/////////////////////////////////////////////////////////////////////////////
{
  // if this is an AES16 or AES16e
  if (m_bIsAES16)
    {
      *plMin = 32000;
      *plMax = 192000;		// BUGBUG Logic 8 requires an extra entry after the max rate
/*
		// if an Aurora is connected, limit the sample rate further...
		PHALAURORA	pHalAurora = m_pHalAdapter->GetAurora();
		ULONG		ulProductID;

		if( pHalAurora )
		{
			// this is going to call either Hal4114 or HalAES16e, which upon driver load has not been opened yet
			pHalAurora->GetProductID( &ulProductID );
			if( ulProductID )
			{
				*plMin = 44100;
				*plMax = 192000;
			}
		}
//**/
    }
  else				// not an AES16
    {
      if (m_pHalAdapter->HasP16 ())
	*plMin = MIN_SAMPLE_RATE;
      else
	*plMin = 11025;

      *plMax = MAX_SAMPLE_RATE;
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalSampleClock::GetClockInfo (LONG * plRate, PSRREGS pSRRegs,
				 PPLLCLOCKINFO pClockInfo,
				 int ulNumberOfEntires)
// private
/////////////////////////////////////////////////////////////////////////////
{
  int i;

  //cmn_err((CE_WARN,"CHalSampleClock::GetClockInfo\n"));

  ulNumberOfEntires--;		// last entry in table is always the M value to use for the non-table based sample rates

  // search for the sample rate in our table
  for (i = 0; i < ulNumberOfEntires; i++)
    {
      // if this firmware doesn't support P16, and this table entry specifies P16, skip it
      if (!m_pHalAdapter->HasP16 () && (pSRRegs[i].usP == SR_P16))
	continue;

      if (*plRate == pSRRegs[i].lSRate)
	{
	  pClockInfo->ulM = pSRRegs[i].usM;
	  pClockInfo->ulN = pSRRegs[i].usN;
	  pClockInfo->ulP = pSRRegs[i].usP;
	  break;		// done
	}
    }

  // if we didn't find a rate in the table
  if (i == ulNumberOfEntires)
    {
      pClockInfo->ulM = pSRRegs[i].usM;	// get the special value at the end of the table

      if (*plRate > 100000)	//1000 Hz resolution
	{
	  pClockInfo->ulN = *plRate / 1000;
	  pClockInfo->ulP = SR_P2;	// SPEED bits makes this P1
	  *plRate = pClockInfo->ulN * 1000;
	}
      else if (*plRate > 50000)	//500 Hz resolution
	{
	  pClockInfo->ulN = *plRate / 500;
	  pClockInfo->ulP = SR_P2;
	  *plRate = pClockInfo->ulN * 500;
	}
      else if (*plRate > 25000)	//250 Hz resolution
	{
	  pClockInfo->ulN = *plRate / 250;
	  pClockInfo->ulP = SR_P4;
	  *plRate = pClockInfo->ulN * 250;
	}
      else if (*plRate > 12500)	//125 Hz resolution
	{
	  pClockInfo->ulN = *plRate / 125;
	  pClockInfo->ulP = SR_P8;
	  *plRate = pClockInfo->ulN * 125;
	}
      else			// Samples Rates 0 - 15kHz  - 62.5 Hz resoluion
	{
	  if (m_pHalAdapter->HasP16 ())
	    {
	      pClockInfo->ulN = (*plRate * 2) / 125;
	      pClockInfo->ulP = SR_P16;	//works with FW with 4 bit P counter
	      *plRate = (pClockInfo->ulN * 125) / 2;
	    }
	  else			// lowest sample rate is limited in SetSampleRate to 11025
	    {
	      pClockInfo->ulN = *plRate / 125;
	      pClockInfo->ulP = SR_P8;
	      *plRate = pClockInfo->ulN * 125;
	    }
	}
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalSampleClock::IsFrequencyAgile (void)
// Returns TRUE if the sample clock is currently frequency agile
// Returns FALSE if the sample clock is fixed to a specific frequency (for any reason)
/////////////////////////////////////////////////////////////////////////////
{
  // first test to see if any devices are currently active
  // if so, we cannot change the frequency unless m_bAllowClockChangeIfActive is TRUE
  if (m_pHalAdapter->GetNumActiveWaveDevices ()
      && !m_bAllowClockChangeIfActive)
    return (FALSE);

  // next, if rate lock is on, then we must not allow a sample rate change
  if (m_bRateLock)
    return (FALSE);

  // next see if the clock source is currently set to something that is fixed
  switch (m_lSource)
    {
    case MIXVAL_L2_CLKSRC_INTERNAL:
    case MIXVAL_L2_CLKSRC_VIDEO:
    case MIXVAL_AES16_CLKSRC_INTERNAL:
      return (TRUE);

    case MIXVAL_L2_CLKSRC_EXTERNAL:
    case MIXVAL_L2_CLKSRC_HEADER:
    case MIXVAL_AES16_CLKSRC_EXTERNAL:
    case MIXVAL_AES16_CLKSRC_HEADER:
      // if the reference is a word clock, then we are fixed
      if ((m_lReference == MIXVAL_CLKREF_WORD)
	  || (m_lReference == MIXVAL_CLKREF_WORD256))
	return (FALSE);

      return (TRUE);

      // all of these sources are always fixed
    case MIXVAL_L2_CLKSRC_LSTREAM_PORT1:
    case MIXVAL_L2_CLKSRC_LSTREAM_PORT2:
    case MIXVAL_L2_CLKSRC_DIGITAL:

    case MIXVAL_AES16_CLKSRC_LSTREAM:
    case MIXVAL_AES16_CLKSRC_DIGITAL_0:
    case MIXVAL_AES16_CLKSRC_DIGITAL_1:
    case MIXVAL_AES16_CLKSRC_DIGITAL_2:
    case MIXVAL_AES16_CLKSRC_DIGITAL_3:
    case MIXVAL_AES16_CLKSRC_DIGITAL_4:	//  8 Digital In 4
    case MIXVAL_AES16_CLKSRC_DIGITAL_5:	//  9 Digital In 5
    case MIXVAL_AES16_CLKSRC_DIGITAL_6:	// 10 Digital In 6
    case MIXVAL_AES16_CLKSRC_DIGITAL_7:	// 11 Digital In 7
    case MIXVAL_AES16_CLKSRC_AES50:
    default:
      return (FALSE);
    }

  // should never get here...
  return (FALSE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::UpdateClockSource (void)
// This method should be called no LESS than once every second to insure
// the clock sources get updated when external changes occur
/////////////////////////////////////////////////////////////////////////////
{
  Set (m_lRate);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::SetPreferredSource (LONG lPreferredSource)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalSampleClock::SetPreferredSource\n"));

  // correct for any code that isn't AES16 aware
  if (m_bIsAES16 && (lPreferredSource < MIXVAL_AES16_CLKSRC_INTERNAL))
    m_lPreferredSource = lPreferredSource + MIXVAL_AES16_CLKSRC_INTERNAL;
  else
    m_lPreferredSource = lPreferredSource;

  //cmn_err((CE_WARN,"CHalSampleClock::SetPreferredSource %ld %ld\n", m_lPreferredSource, lPreferredSource )); 

  m_pHalAdapter->GetMixer ()->ControlChanged (LINE_ADAPTER, LINE_NO_SOURCE,
					      CONTROL_CLOCKSOURCE_PREFERRED);

  // try to switch to the new preferred source
  Set (m_lRate);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::GetPreferredSource (PLONG plPreferredSource)
/////////////////////////////////////////////////////////////////////////////
{
  // if we are using an AES16, correct for the offset
  if (m_bIsAES16 && (m_lPreferredSource >= MIXVAL_AES16_CLKSRC_INTERNAL))
    *plPreferredSource = m_lPreferredSource - MIXVAL_AES16_CLKSRC_INTERNAL;
  else
    *plPreferredSource = m_lPreferredSource;

  //cmn_err((CE_WARN,"CHalSampleClock::GetPreferredSource %ld %ld\n", *plPreferredSource, m_lPreferredSource )); 

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::SetDefaults (void)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalSampleClock::SetDefaults\n"));

  m_ulSpeed = 0xFFFFFFFF;	// invalid
  m_ulP = 0xFFFFFFFF;		// invalid

  m_lRate = -1;
  m_lSource = -1;
  m_lReference = -1;

  SetPreferredSource (MIXVAL_L2_CLKSRC_INTERNAL);

  m_bRateLock = FALSE;
  m_bAllowClockChangeIfActive = FALSE;
  m_bSynchroLock = TRUE;
  m_bWideWireIn = FALSE;
  m_bWideWireOut = FALSE;

  Set (DEFAULT_SAMPLE_RATE, TRUE);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::SetMixerControl (USHORT usControl, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usStatus = HSTATUS_OK;

  //cmn_err((CE_WARN,"CHalSampleClock::SetMixerControl: %u [%lu]\n", usControl, ulValue ));

  switch (usControl)
    {
    case CONTROL_CLOCKSOURCE_PREFERRED:	// MUX
      SetPreferredSource ((LONG) ulValue);
      break;
    case CONTROL_CLOCKRATE_SELECT:	// ULONG
      usStatus = Set ((LONG) ulValue);
      break;
    case CONTROL_CLOCKRATE_LOCK:	// BOOLEAN
      m_bRateLock = (BOOLEAN) ulValue;
      break;
    case CONTROL_CLOCK_CHANGE_IF_ACTIVE:	// BOOLEAN
      m_bAllowClockChangeIfActive = (BOOLEAN) ulValue;
      break;
    case CONTROL_SYNCHROLOCK_ENABLE:
      if (!m_pHalAdapter->HasSynchroLock ())
	return (HSTATUS_INVALID_MIXER_CONTROL);

      m_bSynchroLock = ulValue ? TRUE : FALSE;	// allow the change to happen no matter what

      // if AES50 is the current clock source, SynchroLock must be disabled
      if (m_pHalAdapter->HasAES50 ())
	{
	  if (m_lSource == MIXVAL_AES16_CLKSRC_AES50)
	    {
	      ulValue = FALSE;
	    }
	}

      m_RegVCXOCTLWrite.BitSet (REG_VCXOCTL_SLOCK, (BOOLEAN) ulValue);
      break;
    case CONTROL_WIDEWIREIN:
      if (!m_bIsAES16)
	return (HSTATUS_INVALID_MIXER_CONTROL);

      m_bWideWireIn = ulValue ? TRUE : FALSE;
      // Need to call CHalSampleClock::Set...
      Set (m_lRate, (BOOLEAN) TRUE);	// will set the actual widewire bit in hardware
      break;
    case CONTROL_WIDEWIREOUT:
      if (!m_pHalAdapter->HasWideWireOut ())
	return (HSTATUS_INVALID_MIXER_CONTROL);

      m_bWideWireOut = ulValue ? TRUE : FALSE;
      // Need to call CHalSampleClock::Set...
      Set (m_lRate, (BOOLEAN) TRUE);	// will set the actual widewire bit in hardware
      break;
    default:
      usStatus = HSTATUS_INVALID_MIXER_CONTROL;
      break;
    }
  return (usStatus);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalSampleClock::GetMixerControl (USHORT usControl, PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  switch (usControl)
    {
    case CONTROL_CLOCKSOURCE:	// MUX
      // correct for any code that isn't AES16 aware
      if (m_bIsAES16 && (m_lSource >= MIXVAL_AES16_CLKSRC_INTERNAL))
	*pulValue = m_lSource - MIXVAL_AES16_CLKSRC_INTERNAL;
      else
	*pulValue = m_lSource;
      break;
    case CONTROL_CLOCKSOURCE_PREFERRED:	// MUX
      GetPreferredSource ((PLONG) pulValue);
      break;
    case CONTROL_CLOCKRATE:	// ULONG
      // LRClock is zero in all cases
      m_pHalAdapter->GetFrequencyCounter (L2_FREQCOUNTER_LRCLOCK, pulValue);
      break;
    case CONTROL_CLOCKRATE_SELECT:	// ULONG
      *pulValue = m_lRate;
      break;
    case CONTROL_CLOCKRATE_LOCK:	// BOOLEAN
      *pulValue = m_bRateLock;
      break;
    case CONTROL_CLOCK_CHANGE_IF_ACTIVE:	// BOOLEAN
      *pulValue = m_bAllowClockChangeIfActive;
      break;
    case CONTROL_WIDEWIREIN:
      if (!m_bIsAES16)
	return (HSTATUS_INVALID_MIXER_CONTROL);

      *pulValue = m_bWideWireIn;
      break;
    case CONTROL_WIDEWIREOUT:
      if (!m_pHalAdapter->HasWideWireOut ())
	return (HSTATUS_INVALID_MIXER_CONTROL);

      *pulValue = m_bWideWireOut;
      break;
    case CONTROL_SYNCHROLOCK_ENABLE:
      if (!m_pHalAdapter->HasSynchroLock ())
	return (HSTATUS_INVALID_MIXER_CONTROL);

      *pulValue = m_bSynchroLock;
      // if AES50 is the current clock source, SynchroLock must be disabled
      if (m_pHalAdapter->HasAES50 ())
	{
	  if (m_lSource == MIXVAL_AES16_CLKSRC_AES50)
	    {
	      *pulValue = FALSE;
	    }
	}
      break;
    case CONTROL_SYNCHROLOCK_STATUS:
      if (!m_pHalAdapter->HasSynchroLock ())
	return (HSTATUS_INVALID_MIXER_CONTROL);

      *pulValue = m_RegVCXOCTLRead.Read ();
      break;
    default:
      return (HSTATUS_INVALID_MIXER_CONTROL);
    }
  return (HSTATUS_OK);
}
