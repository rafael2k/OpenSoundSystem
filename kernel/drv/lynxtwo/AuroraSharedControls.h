/****************************************************************************
 AuroraSharedControls.h

 Created: David A. Hoatson, October 2004
	
 Copyright © 2004 Lynx Studio Technology, Inc.

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
#ifndef _AURORASHAREDCONTROLS_H
#define	_AURORASHAREDCONTROLS_H

#define AURORA_NUMBER_OF_OUTPUTS	32

enum
{
  AURORA_PMIXA = 0,
  AURORA_PMIXB,
  AURORA_NUMBER_OF_PMIX
};

#if !defined(sun)
#pragma pack(push,1)
#endif

typedef struct
{
  // Metering
  BYTE aucAnalogInMeters[16];
  BYTE aucAnalogOutMeters[16];
  BYTE aucDigitalInMeters[16];
  BYTE aucDigitalOutMeters[16];
} AURORAFASTUPDATECONTROLS, *PAURORAFASTUPDATECONTROLS;

typedef struct
{
  // Metering
  BYTE aucAnalogInOverload[16];
  BYTE aucAnalogOutOverload[16];
  BYTE aucDigitalInOverload[16];
  BYTE aucDigitalOutOverload[16];	// 64 bytes

  // Input Source

  // Status
  USHORT usProductID;		// 2 bytes

  // TRIMCLK
  ULONG ulTrim[8];		// 32 bytes
  ULONG ulClockSource;		// 1
  ULONG bSynchroLockEnable;	// 2
  ULONG ulClockRate;		// 3

  // MISC
  ULONG ulAnalogOutSource;	// 4
  ULONG ulDigitalOutSource;	// 5
  ULONG bDualWireIn;		// 6
  ULONG bDualWireOut;		// 7
  ULONG ulFPMeterSource;	// 8
  ULONG ulLSlotSource;		// 9
  ULONG ulTrimOrigin;		// 10
  ULONG ulLocalTrim;		// 11
  //ULONG ulPowerUpMode;                                  // 12
  //ULONG ulLSlotChannelMode;                             // 13
  //ULONG ulWordClockOut1X;                               // 14

  // LOCKSTAT
  ULONG ulAESLockStatus;	// 15
  ULONG ulSynchroLockStatus;	// 16

  // PMIX
  BYTE aucMonitorSource[AURORA_NUMBER_OF_OUTPUTS][AURORA_NUMBER_OF_PMIX];	// 32 bytes
  BYTE aucMonitorVolume[AURORA_NUMBER_OF_OUTPUTS][AURORA_NUMBER_OF_PMIX];	// 32 bytes
  BYTE abMonitorMute[AURORA_NUMBER_OF_OUTPUTS][AURORA_NUMBER_OF_PMIX];	// 32 bytes
  BYTE abDither[AURORA_NUMBER_OF_OUTPUTS];	// 32 bytes (128 bytes total)

} AURORASLOWUPDATECONTROLS, *PAURORASLOWUPDATECONTROLS;

typedef struct
{
  LONGLONG llTimestamp;		// 8 bytes
  ULONG ulControlRequest;	// 4 bytes
  AURORAFASTUPDATECONTROLS Fast;	// 64 bytes
  AURORASLOWUPDATECONTROLS Slow;	// 278 bytes
} AURORASHAREDCONTROLS, *PAURORASHAREDCONTROLS;	// 450 bytes

#if !defined(sun)
#pragma pack(pop)
#endif

#define	REQ_FAST_CONTROLS	kBit0
#define	REQ_SLOW_CONTROLS	kBit1

enum
{
  MIXVAL_AURORA_CLKSRC_INTERNAL = 0,
  MIXVAL_AURORA_CLKSRC_EXTERNAL,
  MIXVAL_AURORA_CLKSRC_EXTERNAL_DIV2,
  MIXVAL_AURORA_CLKSRC_AESA,
  MIXVAL_AURORA_CLKSRC_AESB,
  MIXVAL_AURORA_CLKSRC_LSLOT,
  MIXVAL_AURORA_CLKSRC_EXTERNAL_DIV4	// DAH Aug 1, 2007
};

enum
{
  MIXVAL_AURORA_SR44100 = 0,
  MIXVAL_AURORA_SR48000,
  MIXVAL_AURORA_SR88200,
  MIXVAL_AURORA_SR96000,
  MIXVAL_AURORA_SR176400,
  MIXVAL_AURORA_SR192000
};

/////////////////////////////////////////////////////////////////////////////
// LTMODE
/////////////////////////////////////////////////////////////////////////////
#define MIXVAL_LSLOT_SLAVE_LOCK	 kBit0	// 8
#define MIXVAL_LSLOT_IN_MUX_18	 kBit4	// 12
#define MIXVAL_LSLOT_IN_MUX_916	 kBit5	// 13


#endif // _AURORASHAREDCONTROLS_H
