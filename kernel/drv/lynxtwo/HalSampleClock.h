/****************************************************************************
 HalSampleClock.h

 Description:	Interface for the HalSampleClock class.

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

#ifndef _HALSAMPLECLOCK_H
#define _HALSAMPLECLOCK_H

#include "Hal.h"

typedef struct
{
  ULONG ulM;
  ULONG ulBypassM;
  ULONG ulN;
  ULONG ulP;
  ULONG ulClkSrc;
  ULONG ulWord;
  ULONG ulSpeed;
} PLLCLOCKINFO, *PPLLCLOCKINFO;

typedef struct
{
  LONG lSRate;
  USHORT usM;
  USHORT usN;
  USHORT usP;
} SRREGS, *PSRREGS;

class CHalSampleClock
{
public:
  CHalSampleClock ()
  {
  }
   ~CHalSampleClock ()
  {
  }

  void operator= (LONG lRate)
  {
    Set (lRate);
  }
  operator    LONG ()
  {
    return (m_lRate);
  }

  USHORT Open (PHALADAPTER pHalAdapter);
  USHORT Close ();

  USHORT Get (LONG * plRate, LONG * plSource);
  USHORT Get (LONG * plRate);
  USHORT Set (LONG lRate, BOOLEAN bForce = FALSE);

  USHORT GetClockRate (LONG * plRate, LONG * plSource, LONG * plReference);

  USHORT GetMinMax (LONG * plMin, LONG * plMax);

  USHORT UpdateClockSource (void);	// To be called no LESS than once every second by the driver
  BOOLEAN IsFrequencyAgile (void);

  USHORT SetPreferredSource (LONG lPreferredSource);
  USHORT GetPreferredSource (PLONG plPreferredSource);

  USHORT SetDefaults (void);
  USHORT SetMixerControl (USHORT usControl, ULONG ulValue);
  USHORT GetMixerControl (USHORT usControl, PULONG pulValue);

private:
  USHORT GetClockInfo (LONG * plRate, PSRREGS pSRRegs,
		       PPLLCLOCKINFO pClockInfo, int ulNumberOfEntires);

  PHALADAPTER m_pHalAdapter;
  CHalRegister m_RegPLLCTL;
  LONG m_lRate;
  LONG m_lPreferredSource;
  LONG m_lSource;
  BOOLEAN m_bRateLock;
  BOOLEAN m_bAllowClockChangeIfActive;

  // AES16 / AES16e stuff
  CHalRegister m_RegVCXOCTLWrite;
  CHalRegister m_RegVCXOCTLRead;
  BOOLEAN m_bWideWireIn;
  BOOLEAN m_bWideWireOut;
  BOOLEAN m_bSynchroLock;
  //ULONG                 m_ulSynchroLock;
  BOOLEAN m_bIsAES16;

  // internal use only
  LONG m_lReference;
  ULONG m_ulSpeed;
  ULONG m_ulP;
};

#endif // _HALSAMPLECLOCK_H
