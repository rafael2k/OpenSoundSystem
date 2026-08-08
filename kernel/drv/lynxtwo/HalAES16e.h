/****************************************************************************
 HalAES16e.h

 Description:	Interface for the HalAES16e class.

 Created: David A. Hoatson, December 2007 from Hal4114.h
	
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
****************************************************************************/
#ifndef _HALAES16e_H
#define _HALAES16e_H

#include "Hal.h"

enum
{
  kDIO1 = 0,
  kDIO2,
  kDIO3,
  kDIO4,
  kDIO5,
  kDIO6,
  kDIO7,
  kDIO8,
  kNumberOfDIO
};

/////////////////////////////////////////////////////////////////////////////
// AES Transceiver Control
/////////////////////////////////////////////////////////////////////////////
#define REG_AESTXCTL_OUTSTATUS_VALID			kBit0
#define REG_AESTXCTL_OUTSTATUS_NONAUDIO			kBit1
#define REG_AESTXCTL_OUTSTATUS_EMPHASIS_MASK	(kBit2 | kBit3)
#define REG_AESTXCTL_OUTSTATUS_EMPHASIS_NONE	(0)
#define REG_AESTXCTL_OUTSTATUS_EMPHASIS_5015	kBit2
#define REG_AESTXCTL_OUTSTATUS_EMPHASIS_J17		kBit3
#define REG_AESTXCTL_OUTSTATUS_MASK				(kBit0 | kBit1 | kBit2 | kBit3)

#define REG_AESTXCTL_SRCMODE_OFF		(0)
#define REG_AESTXCTL_SRCMODE_ON_DIGIN	kBit4
#define REG_AESTXCTL_SRCMODE_MASK		(kBit4 | kBit5)

enum
{
  MIXVAL_SRCMODE_OFF = 0,	// Normal AES16 operation
  MIXVAL_SRCMODE_ON_DIGIN,	// SRC On - Digital Input
  NUM_SRCMODES
};

#define REG_AES50STAT_RX_FORMAT			kBit0	// 0 = DSD, 1 = PCM
#define REG_AES50STAT_RX_PCM_MODE		kBit1	// Receiver PCM mode, 0 = AES3 compliant, 1 = DXD
#define REG_AES50STAT_RX_PROTOCOL_MASK	(kBit2 | kBit3)	// 00=v3.0, other = unsupported
#define REG_AES50STAT_RX_WIDTH_MASK		(kBit4 | kBit5 | kBit6 | kBit7)	// 0x4 = 16-bit, 0x5 = 20-bit, 0x6 = 24-bit, other = invalid
#define REG_AES50STAT_RX_SPEED_MASK		(kBit8 | kBit9)	// Receiver speed, 0=1X BR, 1 = 2X BR, 3 = 4X BR, 4 = 8X BR
#define REG_AES50STAT_RX_BASERATE		kBit10	// Receiver base rate, 0 = 44.1kHz, 1 = 48kHz
#define REG_AES50STAT_SYNC_FREQ			kBit13	// sync_in freq: 0 = BR/2048, 1=BR detected
#define REG_AES50STAT_SYNCED			kBit14	// sync_in signal locked to internal BR phase
#define REG_AES50STAT_RX_LOCKED			kBit15

class CHalAES16e
{
public:
  CHalAES16e ()
  {
  }				// constructor
   ~CHalAES16e ()
  {
  }				// destructor

  USHORT Open (PHALADAPTER pHalAdapter);
  USHORT Close ();

  USHORT SetDefaults (void);
  USHORT SampleClockChanged (LONG lRate, LONG lSource);

  USHORT GetInputRate (ULONG ulTheChip, PLONG plRate);
  USHORT GetInputStatus (ULONG ulTheChip, PULONG pulStatus);
  BOOLEAN IsInputLocked (ULONG ulTheChip);
  USHORT GetSRCMode (ULONG ulTheChip, PULONG pulMode);
  USHORT SetSRCMode (ULONG ulTheChip, ULONG ulMode);
  USHORT GetSRCRatio (ULONG ulTheChip, PULONG pulSRCRatio);
  USHORT GetOutputStatus (ULONG ulTheChip, PULONG pulStatus);
  USHORT SetOutputStatus (ULONG ulTheChip, ULONG ulStatus);
  USHORT SetOutputValid (BOOLEAN bValid);
  USHORT GetAES50Status (PULONG pulStatus);

  PHALREGISTER GetMISCTL ()
  {
    return (&m_RegMISCTL);
  }

  USHORT SetMixerControl (USHORT usControl, ULONG ulValue);
  USHORT GetMixerControl (USHORT usControl, PULONG pulValue);

private:
  void SetMasterSlave (ULONG ulTheChip, LONG lSource = -1);

  CHalRegister m_pRegAESControl[kNumberOfDIO];
  CHalRegister m_pRegAESStatus[kNumberOfDIO];
  CHalRegister m_RegMISCTL;
  CHalRegister m_RegAES50Status;

  PHALADAPTER m_pHalAdapter;
  USHORT m_usDeviceID;
  BOOLEAN m_bHasSRC;
  BOOLEAN m_bHasAES50;

  ULONG m_ulSRCMode[kNumberOfDIO];
  ULONG m_ulOutputStatus[kNumberOfDIO];
};

#endif // _HAL4114_H
