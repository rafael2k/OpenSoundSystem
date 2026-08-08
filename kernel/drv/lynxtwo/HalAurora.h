/****************************************************************************
 HalAurora.h

 Description:	Interface for the HalAurora class.

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
#ifndef _HALAURORA_H
#define _HALAURORA_H

#include "Hal.h"
#include <AuroraSharedControls.h>

#define REG_ACTL_WREQ	kBit15	// General Write Request Bit

enum
{
  REG_ACTL_COMMAND = 0,
  REG_ACTL_TRIM,
  REG_ACTL_CLOCK,
  REG_ACTL_MISCLO,
  REG_ACTL_MISCHI,
  REG_ACTL_LTMODE = 0x13,	// DAH 5/17/2011
  REG_ACTL_PMIX0_VOLA = 0x80,	// 128 byte offset into the array so this matches the STATUS array
  REG_ACTL_PMIX0_SRCA,
  REG_ACTL_PMIX0_VOLB,
  REG_ACTL_PMIX0_SRCB,
  REG_ACTL_PMIX1_VOLA,
  REG_ACTL_PMIX1_SRCA,
  REG_ACTL_PMIX1_VOLB,
  REG_ACTL_PMIX1_SRCB,
  REG_ACTL_PMIX2_VOLA,
  REG_ACTL_PMIX2_SRCA,
  REG_ACTL_PMIX2_VOLB,
  REG_ACTL_PMIX2_SRCB,
  REG_ACTL_PMIX3_VOLA,
  REG_ACTL_PMIX3_SRCA,
  REG_ACTL_PMIX3_VOLB,
  REG_ACTL_PMIX3_SRCB,
  REG_ACTL_PMIX4_VOLA,
  REG_ACTL_PMIX4_SRCA,
  REG_ACTL_PMIX4_VOLB,
  REG_ACTL_PMIX4_SRCB,
  REG_ACTL_PMIX5_VOLA,
  REG_ACTL_PMIX5_SRCA,
  REG_ACTL_PMIX5_VOLB,
  REG_ACTL_PMIX5_SRCB,
  REG_ACTL_PMIX6_VOLA,
  REG_ACTL_PMIX6_SRCA,
  REG_ACTL_PMIX6_VOLB,
  REG_ACTL_PMIX6_SRCB,
  REG_ACTL_PMIX7_VOLA,
  REG_ACTL_PMIX7_SRCA,
  REG_ACTL_PMIX7_VOLB,
  REG_ACTL_PMIX7_SRCB,
  REG_ACTL_PMIX8_VOLA,
  REG_ACTL_PMIX8_SRCA,
  REG_ACTL_PMIX8_VOLB,
  REG_ACTL_PMIX8_SRCB,
  REG_ACTL_PMIX9_VOLA,
  REG_ACTL_PMIX9_SRCA,
  REG_ACTL_PMIX9_VOLB,
  REG_ACTL_PMIX9_SRCB,
  REG_ACTL_PMIX10_VOLA,
  REG_ACTL_PMIX10_SRCA,
  REG_ACTL_PMIX10_VOLB,
  REG_ACTL_PMIX10_SRCB,
  REG_ACTL_PMIX11_VOLA,
  REG_ACTL_PMIX11_SRCA,
  REG_ACTL_PMIX11_VOLB,
  REG_ACTL_PMIX11_SRCB,
  REG_ACTL_PMIX12_VOLA,
  REG_ACTL_PMIX12_SRCA,
  REG_ACTL_PMIX12_VOLB,
  REG_ACTL_PMIX12_SRCB,
  REG_ACTL_PMIX13_VOLA,
  REG_ACTL_PMIX13_SRCA,
  REG_ACTL_PMIX13_VOLB,
  REG_ACTL_PMIX13_SRCB,
  REG_ACTL_PMIX14_VOLA,
  REG_ACTL_PMIX14_SRCA,
  REG_ACTL_PMIX14_VOLB,
  REG_ACTL_PMIX14_SRCB,
  REG_ACTL_PMIX15_VOLA,
  REG_ACTL_PMIX15_SRCA,
  REG_ACTL_PMIX15_VOLB,
  REG_ACTL_PMIX15_SRCB,
  REG_ACTL_PMIX16_VOLA,
  REG_ACTL_PMIX16_SRCA,
  REG_ACTL_PMIX16_VOLB,
  REG_ACTL_PMIX16_SRCB,
  REG_ACTL_PMIX17_VOLA,
  REG_ACTL_PMIX17_SRCA,
  REG_ACTL_PMIX17_VOLB,
  REG_ACTL_PMIX17_SRCB,
  REG_ACTL_PMIX18_VOLA,
  REG_ACTL_PMIX18_SRCA,
  REG_ACTL_PMIX18_VOLB,
  REG_ACTL_PMIX18_SRCB,
  REG_ACTL_PMIX19_VOLA,
  REG_ACTL_PMIX19_SRCA,
  REG_ACTL_PMIX19_VOLB,
  REG_ACTL_PMIX19_SRCB,
  REG_ACTL_PMIX20_VOLA,
  REG_ACTL_PMIX20_SRCA,
  REG_ACTL_PMIX20_VOLB,
  REG_ACTL_PMIX20_SRCB,
  REG_ACTL_PMIX21_VOLA,
  REG_ACTL_PMIX21_SRCA,
  REG_ACTL_PMIX21_VOLB,
  REG_ACTL_PMIX21_SRCB,
  REG_ACTL_PMIX22_VOLA,
  REG_ACTL_PMIX22_SRCA,
  REG_ACTL_PMIX22_VOLB,
  REG_ACTL_PMIX22_SRCB,
  REG_ACTL_PMIX23_VOLA,
  REG_ACTL_PMIX23_SRCA,
  REG_ACTL_PMIX23_VOLB,
  REG_ACTL_PMIX23_SRCB,
  REG_ACTL_PMIX24_VOLA,
  REG_ACTL_PMIX24_SRCA,
  REG_ACTL_PMIX24_VOLB,
  REG_ACTL_PMIX24_SRCB,
  REG_ACTL_PMIX25_VOLA,
  REG_ACTL_PMIX25_SRCA,
  REG_ACTL_PMIX25_VOLB,
  REG_ACTL_PMIX25_SRCB,
  REG_ACTL_PMIX26_VOLA,
  REG_ACTL_PMIX26_SRCA,
  REG_ACTL_PMIX26_VOLB,
  REG_ACTL_PMIX26_SRCB,
  REG_ACTL_PMIX27_VOLA,
  REG_ACTL_PMIX27_SRCA,
  REG_ACTL_PMIX27_VOLB,
  REG_ACTL_PMIX27_SRCB,
  REG_ACTL_PMIX28_VOLA,
  REG_ACTL_PMIX28_SRCA,
  REG_ACTL_PMIX28_VOLB,
  REG_ACTL_PMIX28_SRCB,
  REG_ACTL_PMIX29_VOLA,
  REG_ACTL_PMIX29_SRCA,
  REG_ACTL_PMIX29_VOLB,
  REG_ACTL_PMIX29_SRCB,
  REG_ACTL_PMIX30_VOLA,
  REG_ACTL_PMIX30_SRCA,
  REG_ACTL_PMIX30_VOLB,
  REG_ACTL_PMIX30_SRCB,
  REG_ACTL_PMIX31_VOLA,
  REG_ACTL_PMIX31_SRCA,
  REG_ACTL_PMIX31_VOLB,
  REG_ACTL_PMIX31_SRCB,
  REG_ACTL_NUMBER_OF_REGISTERS
};

// Shadow register array offsets
enum
{
  REG_ASTAT_PID = 0,
  REG_ASTAT_LINK,
  REG_ASTAT_TRIM,
  REG_ASTAT_CLOCK,
  REG_ASTAT_MISC,
  REG_ASTAT_AESLOCK,
  REG_ASTAT_SLOCK,
  REG_ASTAT_SYNCCODE,
  REG_ASTAT_PGMSTAT,
  REG_ASTAT_FWREVA,
  REG_ASTAT_FWREVB,
  REG_ASTAT_SNDATEWEEK,
  REG_ASTAT_SNDATEYEAR,
  REG_ASTAT_SNUNITA,
  REG_ASTAT_SNUNITB,

  REG_ASTAT_AINLVL1,
  REG_ASTAT_AINLVL2,
  REG_ASTAT_AINLVL3,
  REG_ASTAT_AINLVL4,
  REG_ASTAT_AINLVL5,
  REG_ASTAT_AINLVL6,
  REG_ASTAT_AINLVL7,
  REG_ASTAT_AINLVL8,
  REG_ASTAT_AINLVL9,
  REG_ASTAT_AINLVL10,
  REG_ASTAT_AINLVL11,
  REG_ASTAT_AINLVL12,
  REG_ASTAT_AINLVL13,
  REG_ASTAT_AINLVL14,
  REG_ASTAT_AINLVL15,
  REG_ASTAT_AINLVL16,
  REG_ASTAT_AOUTLVL1,
  REG_ASTAT_AOUTLVL2,
  REG_ASTAT_AOUTLVL3,
  REG_ASTAT_AOUTLVL4,
  REG_ASTAT_AOUTLVL5,
  REG_ASTAT_AOUTLVL6,
  REG_ASTAT_AOUTLVL7,
  REG_ASTAT_AOUTLVL8,
  REG_ASTAT_AOUTLVL9,
  REG_ASTAT_AOUTLVL10,
  REG_ASTAT_AOUTLVL11,
  REG_ASTAT_AOUTLVL12,
  REG_ASTAT_AOUTLVL13,
  REG_ASTAT_AOUTLVL14,
  REG_ASTAT_AOUTLVL15,
  REG_ASTAT_AOUTLVL16,
  REG_ASTAT_DINLVL1,
  REG_ASTAT_DINLVL2,
  REG_ASTAT_DINLVL3,
  REG_ASTAT_DINLVL4,
  REG_ASTAT_DINLVL5,
  REG_ASTAT_DINLVL6,
  REG_ASTAT_DINLVL7,
  REG_ASTAT_DINLVL8,
  REG_ASTAT_DINLVL9,
  REG_ASTAT_DINLVL10,
  REG_ASTAT_DINLVL11,
  REG_ASTAT_DINLVL12,
  REG_ASTAT_DINLVL13,
  REG_ASTAT_DINLVL14,
  REG_ASTAT_DINLVL15,
  REG_ASTAT_DINLVL16,
  REG_ASTAT_DOUTLVL1,
  REG_ASTAT_DOUTLVL2,
  REG_ASTAT_DOUTLVL3,
  REG_ASTAT_DOUTLVL4,
  REG_ASTAT_DOUTLVL5,
  REG_ASTAT_DOUTLVL6,
  REG_ASTAT_DOUTLVL7,
  REG_ASTAT_DOUTLVL8,
  REG_ASTAT_DOUTLVL9,
  REG_ASTAT_DOUTLVL10,
  REG_ASTAT_DOUTLVL11,
  REG_ASTAT_DOUTLVL12,
  REG_ASTAT_DOUTLVL13,
  REG_ASTAT_DOUTLVL14,
  REG_ASTAT_DOUTLVL15,
  REG_ASTAT_DOUTLVL16,
  REG_ASTAT_PMIX0_VOLA = REG_ACTL_PMIX0_VOLA,	// make sure the command and status registers match
  REG_ASTAT_PMIX0_SRCA,
  REG_ASTAT_PMIX0_VOLB,
  REG_ASTAT_PMIX0_SRCB,
  REG_ASTAT_PMIX1_VOLA,
  REG_ASTAT_PMIX1_SRCA,
  REG_ASTAT_PMIX1_VOLB,
  REG_ASTAT_PMIX1_SRCB,
  REG_ASTAT_PMIX2_VOLA,
  REG_ASTAT_PMIX2_SRCA,
  REG_ASTAT_PMIX2_VOLB,
  REG_ASTAT_PMIX2_SRCB,
  REG_ASTAT_PMIX3_VOLA,
  REG_ASTAT_PMIX3_SRCA,
  REG_ASTAT_PMIX3_VOLB,
  REG_ASTAT_PMIX3_SRCB,
  REG_ASTAT_PMIX4_VOLA,
  REG_ASTAT_PMIX4_SRCA,
  REG_ASTAT_PMIX4_VOLB,
  REG_ASTAT_PMIX4_SRCB,
  REG_ASTAT_PMIX5_VOLA,
  REG_ASTAT_PMIX5_SRCA,
  REG_ASTAT_PMIX5_VOLB,
  REG_ASTAT_PMIX5_SRCB,
  REG_ASTAT_PMIX6_VOLA,
  REG_ASTAT_PMIX6_SRCA,
  REG_ASTAT_PMIX6_VOLB,
  REG_ASTAT_PMIX6_SRCB,
  REG_ASTAT_PMIX7_VOLA,
  REG_ASTAT_PMIX7_SRCA,
  REG_ASTAT_PMIX7_VOLB,
  REG_ASTAT_PMIX7_SRCB,
  REG_ASTAT_PMIX8_VOLA,
  REG_ASTAT_PMIX8_SRCA,
  REG_ASTAT_PMIX8_VOLB,
  REG_ASTAT_PMIX8_SRCB,
  REG_ASTAT_PMIX9_VOLA,
  REG_ASTAT_PMIX9_SRCA,
  REG_ASTAT_PMIX9_VOLB,
  REG_ASTAT_PMIX9_SRCB,
  REG_ASTAT_PMIX10_VOLA,
  REG_ASTAT_PMIX10_SRCA,
  REG_ASTAT_PMIX10_VOLB,
  REG_ASTAT_PMIX10_SRCB,
  REG_ASTAT_PMIX11_VOLA,
  REG_ASTAT_PMIX11_SRCA,
  REG_ASTAT_PMIX11_VOLB,
  REG_ASTAT_PMIX11_SRCB,
  REG_ASTAT_PMIX12_VOLA,
  REG_ASTAT_PMIX12_SRCA,
  REG_ASTAT_PMIX12_VOLB,
  REG_ASTAT_PMIX12_SRCB,
  REG_ASTAT_PMIX13_VOLA,
  REG_ASTAT_PMIX13_SRCA,
  REG_ASTAT_PMIX13_VOLB,
  REG_ASTAT_PMIX13_SRCB,
  REG_ASTAT_PMIX14_VOLA,
  REG_ASTAT_PMIX14_SRCA,
  REG_ASTAT_PMIX14_VOLB,
  REG_ASTAT_PMIX14_SRCB,
  REG_ASTAT_PMIX15_VOLA,
  REG_ASTAT_PMIX15_SRCA,
  REG_ASTAT_PMIX15_VOLB,
  REG_ASTAT_PMIX15_SRCB,
  REG_ASTAT_PMIX16_VOLA,
  REG_ASTAT_PMIX16_SRCA,
  REG_ASTAT_PMIX16_VOLB,
  REG_ASTAT_PMIX16_SRCB,
  REG_ASTAT_PMIX17_VOLA,
  REG_ASTAT_PMIX17_SRCA,
  REG_ASTAT_PMIX17_VOLB,
  REG_ASTAT_PMIX17_SRCB,
  REG_ASTAT_PMIX18_VOLA,
  REG_ASTAT_PMIX18_SRCA,
  REG_ASTAT_PMIX18_VOLB,
  REG_ASTAT_PMIX18_SRCB,
  REG_ASTAT_PMIX19_VOLA,
  REG_ASTAT_PMIX19_SRCA,
  REG_ASTAT_PMIX19_VOLB,
  REG_ASTAT_PMIX19_SRCB,
  REG_ASTAT_PMIX20_VOLA,
  REG_ASTAT_PMIX20_SRCA,
  REG_ASTAT_PMIX20_VOLB,
  REG_ASTAT_PMIX20_SRCB,
  REG_ASTAT_PMIX21_VOLA,
  REG_ASTAT_PMIX21_SRCA,
  REG_ASTAT_PMIX21_VOLB,
  REG_ASTAT_PMIX21_SRCB,
  REG_ASTAT_PMIX22_VOLA,
  REG_ASTAT_PMIX22_SRCA,
  REG_ASTAT_PMIX22_VOLB,
  REG_ASTAT_PMIX22_SRCB,
  REG_ASTAT_PMIX23_VOLA,
  REG_ASTAT_PMIX23_SRCA,
  REG_ASTAT_PMIX23_VOLB,
  REG_ASTAT_PMIX23_SRCB,
  REG_ASTAT_PMIX24_VOLA,
  REG_ASTAT_PMIX24_SRCA,
  REG_ASTAT_PMIX24_VOLB,
  REG_ASTAT_PMIX24_SRCB,
  REG_ASTAT_PMIX25_VOLA,
  REG_ASTAT_PMIX25_SRCA,
  REG_ASTAT_PMIX25_VOLB,
  REG_ASTAT_PMIX25_SRCB,
  REG_ASTAT_PMIX26_VOLA,
  REG_ASTAT_PMIX26_SRCA,
  REG_ASTAT_PMIX26_VOLB,
  REG_ASTAT_PMIX26_SRCB,
  REG_ASTAT_PMIX27_VOLA,
  REG_ASTAT_PMIX27_SRCA,
  REG_ASTAT_PMIX27_VOLB,
  REG_ASTAT_PMIX27_SRCB,
  REG_ASTAT_PMIX28_VOLA,
  REG_ASTAT_PMIX28_SRCA,
  REG_ASTAT_PMIX28_VOLB,
  REG_ASTAT_PMIX28_SRCB,
  REG_ASTAT_PMIX29_VOLA,
  REG_ASTAT_PMIX29_SRCA,
  REG_ASTAT_PMIX29_VOLB,
  REG_ASTAT_PMIX29_SRCB,
  REG_ASTAT_PMIX30_VOLA,
  REG_ASTAT_PMIX30_SRCA,
  REG_ASTAT_PMIX30_VOLB,
  REG_ASTAT_PMIX30_SRCB,
  REG_ASTAT_PMIX31_VOLA,
  REG_ASTAT_PMIX31_SRCA,
  REG_ASTAT_PMIX31_VOLB,
  REG_ASTAT_PMIX31_SRCB,
  REG_ASTAT_NUMBER_OF_REGISTERS
};

/////////////////////////////////////////////////////////////////////////////
// COMMAND / PGMSTAT
/////////////////////////////////////////////////////////////////////////////
enum
{
  REG_ACTL_COMMAND_REGISTER_WRITE = 0,	// 0: Register Write
  REG_ACTL_COMMAND_RESTORE_DEFAULTS,	// 1: Restore defaults
  REG_ACTL_COMMAND_SAVE_SCENE,	// 2: Store settings - saves current "scene" in EEPROM
  REG_ACTL_COMMAND_ERASE_CFG_BANK,	// 3: Erase configuration bank in EEPROM
  REG_ACTL_COMMAND_PROGRAM_CFG_BANK,	// 4: Program configuration bank in EEPROM with 128-byte packet in 40 - 7F
  REG_ACTL_COMMAND_VERIFY_CFG_BANK,	// 5: Verify configuration bank in EEPROM, 128-byte packets returned in C0 - FF
  REG_ACTL_COMMAND_CFG_BANK1_SELECT,	// 6: Configuration Bank 1 Select
  REG_ACTL_COMMAND_CFG_BANK2_SELECT,	// 7: Configuration Bank 2 Select
  REG_ACTL_COMMAND_SET_CFG_BANK1_ACTIVE,	// 8: Set configuration bank 1 active
  REG_ACTL_COMMAND_SET_CFG_BANK2_ACTIVE,	// 9: Set configuration bank 2 active
  REG_ACTL_COMMAND_PROGRAM_SERIALNUMBER,	// 10: Program Serial Number (write SNUNIT & SNDATE prior to issuing)
  REG_ACTL_COMMAND_RESUME,	// 11: Resume (assert on failed verify)
  REG_ACTL_COMMAND_WRITE_ENABLE,	// 12: Write enable - must be sent prior to Erase, Program, Set Config Bank, Program SN
  REG_ACTL_COMMAND_REQUEST_STATUS,	// 13: Request Status
  REG_ACTL_COMMAND_STATUS_DATA,	// 14: Status Data - used only for send messages to host, used for IR and MIDI only
  REG_ACTL_COMMAND_PROGRAM_DATA	// 15: Program Data
};

#define REG_ASTAT_PGMSTAT_PIP			kBit0
#define REG_ASTAT_PGMSTAT_PGMERR		kBit1
#define REG_ASTAT_PGMSTAT_TXERR			kBit2
#define REG_ASTAT_PGMSTAT_PAKRDY		kBit3

#define REG_ASTAT_PGMSTAT_FWWORKBANK	kBit6	// kBit14 In HIWORD
#define REG_ASTAT_PGMSTAT_FWBANK		kBit7	// kBit15 In HIWORD

#define REG_PGMSTAT_PIP					kBit8
#define REG_PGMSTAT_PGMERR				kBit9
#define REG_PGMSTAT_TXERR				kBit10
#define REG_PGMSTAT_PAKRDY				kBit11

#define REG_PGMSTAT_FWWORKBANK			kBit14
#define REG_PGMSTAT_FWBANK				kBit15

#define FW_BANK_1					0
#define FW_BANK_2					1

/////////////////////////////////////////////////////////////////////////////
// LOCKSTAT
/////////////////////////////////////////////////////////////////////////////
#define REG_LOCKSTAT_AESLOCK_MASK		0x00FF

#define REG_LOCKSTAT_SLOCK_MASK			0xFF00
#define REG_LOCKSTAT_SLOCK_OFFSET		8

/////////////////////////////////////////////////////////////////////////////
// TRIMCLKCTL / TRIMCLKSTAT
/////////////////////////////////////////////////////////////////////////////
enum
{
  AURORA_TRIMIN0 = 0,
  AURORA_TRIMIN1,
  AURORA_TRIMIN2,
  AURORA_TRIMIN3,
  AURORA_TRIMOUT0,
  AURORA_TRIMOUT1,
  AURORA_TRIMOUT2,
  AURORA_TRIMOUT3,
  AURORA_NUM_TRIM
};

enum
{
  REG_TRIMCLK_CLKSRC_LSLOT = 0,
  REG_TRIMCLK_CLKSRC_AESB,
  REG_TRIMCLK_CLKSRC_AESA,
  REG_TRIMCLK_CLKSRC_EXTERNAL_DIV2,
  REG_TRIMCLK_CLKSRC_EXTERNAL,
  REG_TRIMCLK_CLKSRC_INTERNAL,
  REG_TRIMCLK_CLKSRC_EXTERNAL_DIV4	// DAH Aug 1, 2007
};

#define REG_ACTL_CLOCK_CLKSRC_MASK	(kBit0 | kBit1 | kBit2)
#define REG_ACTL_CLOCK_SYNCHROLOCK	kBit3

#define REG_ACTL_CLOCK_SR_MASK		(kBit4 | kBit5 | kBit6)
#define REG_ACTL_CLOCK_SR_OFFSET	4

#define REG_ASTAT_CLOCK_SYNCERROR	kBit7

/////////////////////////////////////////////////////////////////////////////
// MISCCTL / MISCSTAT
/////////////////////////////////////////////////////////////////////////////
enum
{
  MIXVAL_AURORA_OUTSOURCE_ANALOGIN = 0,
  MIXVAL_AURORA_OUTSOURCE_AESIN,
  MIXVAL_AURORA_OUTSOURCE_LSLOTIN,
  MIXVAL_AURORA_OUTSOURCE_REMOTE
};

#define REG_MISC_AOUTSRC_AIN		kBit1
#define REG_MISC_AOUTSRC_DIN		kBit0
#define REG_MISC_AOUTSRC_LSLOTIN	0
#define REG_MISC_AOUTSRC_REMOTE		(kBit0 | kBit1)
#define REG_MISC_AOUTSRC_MASK		(kBit0 | kBit1)

#define REG_MISC_DOUTSRC_AIN		kBit3
#define REG_MISC_DOUTSRC_DIN		kBit2
#define REG_MISC_DOUTSRC_LSLOTIN	0
#define REG_MISC_DOUTSRC_REMOTE		(kBit2 | kBit3)
#define REG_MISC_DOUTSRC_MASK		(kBit2 | kBit3)
#define REG_MISC_DOUTSRC_OFFSET		2

#define REG_MISC_DUALWIREIN			kBit4
#define REG_MISC_DUALWIREOUT		kBit5

#define REG_MISC_FPMETERSOURCE_ANALOG	0
#define REG_MISC_FPMETERSOURCE_DIGITAL	kBit6
#define REG_MISC_FPMETERSOURCE_MASK		kBit6
#define REG_MISC_FPMETERSOURCE_OFFSET	6

#define MIXVAL_AURORA_FPMETERSOURCE_ANALOG	0
#define MIXVAL_AURORA_FPMETERSOURCE_DIGITAL	1

#define REG_MISC_LSLOTSRC_ANALOG	kBit7
#define REG_MISC_LSLOTSRC_DIGITAL	0
#define REG_MISC_LSLOTSRC_MASK		kBit7
#define REG_MISC_LSLOTSRC_OFFSET	7

#define MIXVAL_AURORA_LSLOTOUTSOURCE_ANALOGOUT	0
#define MIXVAL_AURORA_LSLOTOUTSOURCE_DIGITALOUT	1

#define REG_MISC_TRIMORG_LOCAL		0
#define REG_MISC_TRIMORG_REMOTE		kBit8
#define REG_MISC_TRIMORG_OFFSET		8

#define REG_MISC_LOCALTRIM_MINUS10	0
#define REG_MISC_LOCALTRIM_PLUS4	kBit9
#define REG_MISC_LOCALTRIM_OFFSET	9

#define REG_MISC_POWERUP_ON			kBit10
#define REG_MISC_POWERUP_STANDBY	0
#define REG_MISC_POWERUP_OFFSET		10

#define REG_MISC_LTCHMODESTAT_16		0
#define REG_MISC_LTCHMODESTAT_32		kBit11	// Read Only
#define REG_MISC_LTCHMODESTAT_OFFSET	11

#define REG_MISC_LTPGMEN_AURORA			0
#define REG_MISC_LTPGMEN_LSLOT			kBit12
#define REG_MISC_LTPGMEN_OFFSET			12

#define REG_MISC_WCLKBASE_SAMPLERATE	0
#define REG_MISC_WCLKBASE_1X			kBit13
#define REG_MISC_WCLKBASE_OFFSET		13

#define REG_MISC_IDENTIFY				kBit14

/////////////////////////////////////////////////////////////////////////////

#define REG_MISCHI_TRIMORG_LOCAL		0
#define REG_MISCHI_TRIMORG_REMOTE		kBit0

#define REG_MISCHI_LOCALTRIM_MINUS10	0
#define REG_MISCHI_LOCALTRIM_PLUS4		kBit1
#define REG_MISCHI_LOCALTRIM_MASK		kBit1
#define REG_MISCHI_LOCALTRIM_OFFSET		1

#define REG_MISCHI_POWERUP_ON			kBit2
#define REG_MISCHI_POWERUP_STANDBY		0
#define REG_MISCHI_POWERUP_OFFSET		2

#define REG_MISCHI_LTCHMODESTAT_16		0
#define REG_MISCHI_LTCHMODESTAT_32		kBit3	// Read Only
#define REG_MISCHI_LTCHMODESTAT_OFFSET	3

#define REG_MISCHI_LTPGMEN_AURORA		0
#define REG_MISCHI_LTPGMEN_LSLOT		kBit4
#define REG_MISCHI_LTPGMEN_OFFSET		4

#define REG_MISCHI_WCLKBASE_SAMPLERATE	0
#define REG_MISCHI_WCLKBASE_1X			kBit5
#define REG_MISCHI_WCLKBASE_OFFSET		5

#define REG_MISCHI_IDENTIFY				kBit6

#define REG_MISCHI_IDENTIFY_BURST		kBit7

/////////////////////////////////////////////////////////////////////////////
// REV STATUS
/////////////////////////////////////////////////////////////////////////////
#define REG_ASTAT_REV_CURFWREV_MASK	(kBit0 | kBit1 | kBit2 | kBit3 | kBit4 | kBit5)
#define REG_ASTAT_REV_ALTFWREV_MASK	(kBit6 | kBit7 | kBit8 | kBit9 | kBit10 | kBit11)
#define REG_ASTAT_REV_PCBREV_MASK	(kBit12 | kBit13 | kBit14 | kBit15)

#define REG_ASTAT_REV_ALTFWREV_OFFSET	6
#define REG_ASTAT_REV_PCBREV_OFFSET		12

/////////////////////////////////////////////////////////////////////////////
// LTMODE
/////////////////////////////////////////////////////////////////////////////
#define REG_LTPCBREVMODE_LSLOT_PCBREV_MASK	0x00FF
#define REG_LTPCBREVMODE_LSLOT_LTMODE_MASK	0xFF00
#define REG_LTPCBREVMODE_LSLOT_SLAVE_LOCK	kBit8
#define REG_LTPCBREVMODE_LSLOT_IN_MUX_18	kBit12
#define REG_LTPCBREVMODE_LSLOT_IN_MUX_916	kBit13

/////////////////////////////////////////////////////////////////////////////
// LEVELS
/////////////////////////////////////////////////////////////////////////////

typedef struct
{
  ULONG Levels[8];		// Odd channels are in the low byte, even channels in the high byte
} LEVELS;

/////////////////////////////////////////////////////////////////////////////
// PMIXCTL / PMIXSTAT
/////////////////////////////////////////////////////////////////////////////
typedef struct
{
  ULONG PMixA;			// 16-bit
  ULONG PMixB;
} PMIXCTLSTAT, *PPMIXCTLSTAT;

/* DAH Aug 29, 2005 moved to AuroraSharedControls.h
enum
{
	AURORA_PMIXA=0,
	AURORA_PMIXB,
	AURORA_NUMBER_OF_PMIX
};
*/

#define AURORA_VOLUME_MAX	0
#define AURORA_VOLUME_MIN	0xFE
#define AURORA_VOLUME_MUTE	0xFF

#define REG_ACTL_PMIX_CHANNEL_OFFSET	(REG_ACTL_PMIX1_VOLA - REG_ACTL_PMIX0_VOLA)

#define REG_ACTL_PMIX_SRC_MASK			0x3F
#define REG_ACTL_PMIX_SRCA_DITHER		kBit6

/////////////////////////////////////////////////////////////////////////////
// AURORABLK for AES16
/////////////////////////////////////////////////////////////////////////////
typedef struct
{
  // Control Registers
  ULONG COMMAND;		// 0x00 (00, 01)
  ULONG TRIMCLKCTL;		// 0x01 (02, 03)
  ULONG MISCCTL;		// 0x02 (04, 05)
  ULONG NotDefined3;		// 0x03 (06, 07)
  ULONG NotDefined4;		// 0x04 (08, 09)
  ULONG NotDefined5;		// 0x05 (0A, 0B)
  ULONG SNDATECTL;		// 0x06 (0C, 0D)
  ULONG SNUNITCTL;		// 0x07 (0E, 0F)
  ULONG NotDefined8;		// 0x08 (10, 11)
  ULONG LTPCBREVMODECTL;	// 0x09 (12, 13)
  ULONG NotDefinedA[0x36];	// 0x0A..0x3F

  // PMIXCTL
  PMIXCTLSTAT PMIXCTL[32];	// 0x40..0x7F

  // Status Registers
  ULONG LINKPID;		// 0x80
  ULONG TRIMCLKSTAT;
  ULONG MISCSTAT;
  ULONG LOCKSTAT;
  ULONG PGMSTAT;
  ULONG REV;
  ULONG SNDATESTAT;
  ULONG SNUNITSTAT;		// 0x87

  // DAH Added 08/22/2006
  ULONG LTDEVIDFWREVSTAT;	// LSlot device ID & LSlot device firmware revision
  ULONG LTPCBREVMODESTAT;	// LSlot PCB revision &  LSlot device mode, b0 = Slave/Lockout

  ULONG NotDefined8A[0x16];	// 0x8A..0x9F

  // Levels
  LEVELS AINLVL;		// 0xA0..0xA7
  LEVELS AOUTLVL;
  LEVELS DINLVL;
  LEVELS DOUTLVL;

  // PMIXSTAT
  PMIXCTLSTAT PMIXSTAT[32];	// 0xC0..0xFF
} AURORABLK, *PAURORABLK;

enum
{
  AURORA_PORT_AES16 = 0,
  AURORA_PORT_LSTREAM1,
  AURORA_PORT_LSTREAM2,		// highly unlikely?
  AURORA_PORT_MIDI,		// not possible
  AURORA_PORT_IRDA,		// not possible
  AURORA_PORT_MAX
};

/////////////////////////////////////////////////////////////////////////////
// CHalAurora
/////////////////////////////////////////////////////////////////////////////
class CHalAurora
{
public:
  CHalAurora ()
  {
  }				// constructor
   ~CHalAurora ()
  {
  }				// destructor

  USHORT Open (PHALADAPTER pHalAdapter);
  USHORT Close ();

  USHORT SetDefaults (void);

  USHORT WriteControl (ULONG ulControlRegister, BYTE ucValue, BYTE ucMask =
		       0xFF);
  BYTE ReadStatus (ULONG ulStatusRegister);

  USHORT SampleClockChanged (LONG lRate, LONG lSource);

  /////////////////////////////////////////////////////////////////////////
  // TRIMCLK
  /////////////////////////////////////////////////////////////////////////
  USHORT SetTrim (ULONG ulControl, ULONG ulValue);
  USHORT GetTrim (ULONG ulControl, PULONG pulValue);

  USHORT SetClockSource (ULONG ulClockSource);
  USHORT GetClockSource (PULONG pulClockSource);

  USHORT SetSynchroLockEnable (ULONG ulEnable);
  USHORT GetSynchroLockEnable (PULONG pulValue)
  {
    *pulValue = m_bSynchroLockEnable;
    return (HSTATUS_OK);
  }

  USHORT SetSampleRate (ULONG ulSampleRate);
  USHORT GetSampleRate (PULONG pulSampleRate);

  /////////////////////////////////////////////////////////////////////////
  // LOCKSTAT
  /////////////////////////////////////////////////////////////////////////
  USHORT GetDigitalInStatus (PULONG pulValue)
  {
    *pulValue = m_ulAESLockStatus;
    return (HSTATUS_OK);
  }
  USHORT GetSynchroLockStatus (PULONG pulValue)
  {
    *pulValue = m_ulSynchroLockStatus;
    return (HSTATUS_OK);
  }

  /////////////////////////////////////////////////////////////////////////
  // MISCCTL / MISCSTAT
  /////////////////////////////////////////////////////////////////////////
  USHORT SetAnalogOutSource (ULONG ulValue);
  USHORT GetAnalogOutSource (PULONG pulValue)
  {
    *pulValue = m_ulAnalogOutSource;
    return (HSTATUS_OK);
  }

  USHORT SetDigitalOutSource (ULONG ulValue);
  USHORT GetDigitalOutSource (PULONG pulValue)
  {
    *pulValue = m_ulDigitalOutSource;
    return (HSTATUS_OK);
  }

  USHORT SetDualWireIn (ULONG ulEnable);
  USHORT GetDualWireIn (PULONG pulValue)
  {
    *pulValue = m_bDualWireIn;
    return (HSTATUS_OK);
  }

  USHORT SetDualWireOut (ULONG ulEnable);
  USHORT GetDualWireOut (PULONG pulValue)
  {
    *pulValue = m_bDualWireOut;
    return (HSTATUS_OK);
  }

  USHORT SetFPMeterSource (ULONG ulValue);
  USHORT GetFPMeterSource (PULONG pulValue)
  {
    *pulValue = m_ulFPMeterSource;
    return (HSTATUS_OK);
  }

  USHORT SetLSlotOutSource (ULONG ulValue);
  USHORT GetLSlotOutSource (PULONG pulValue)
  {
    *pulValue = m_ulLSlotSource;
    return (HSTATUS_OK);
  }

  USHORT SetTrimOrigin (ULONG ulValue);
  USHORT GetTrimOrigin (PULONG pulValue)
  {
    *pulValue = m_ulTrimOrigin;
    return (HSTATUS_OK);
  }

  USHORT SetLocalTrim (ULONG ulValue);
  USHORT GetLocalTrim (PULONG pulValue)
  {
    *pulValue = m_ulLocalTrim;
    return (HSTATUS_OK);
  }

  USHORT SetMonitorSource (ULONG ulChannel, ULONG ulTheSource, ULONG ulValue);
  USHORT GetMonitorSource (ULONG ulChannel, ULONG ulTheSource,
			   PULONG pulValue);

  USHORT SetVolume (ULONG ulChannel, ULONG ulTheSource, ULONG ulValue);
  USHORT GetVolume (ULONG ulChannel, ULONG ulTheSource, PULONG pulValue);

  USHORT SetMute (ULONG ulChannel, ULONG ulTheSource, ULONG ulValue);
  USHORT GetMute (ULONG ulChannel, ULONG ulTheSource, PULONG pulValue);

  USHORT SetVolume (ULONG ulChannel, ULONG ulValue);
  USHORT GetVolume (ULONG ulChannel, PULONG pulValue);

  USHORT SetMute (ULONG ulChannel, ULONG ulValue);
  USHORT GetMute (ULONG ulChannel, PULONG pulValue);

  USHORT SetDither (ULONG ulChannel, ULONG ulValue);
  USHORT GetDither (ULONG ulChannel, PULONG pulValue);

  USHORT ResetOverload (USHORT usSrcLine);
  USHORT GetOverload (USHORT usSrcLine, PULONG pulValue);
  USHORT GetLevel (USHORT usSrcLine, PULONG pulValue);

  USHORT GetProductID (PULONG pulValue);
  USHORT GetPCBRev (PULONG pulValue);
  USHORT GetCurrentFirmwareRev (PULONG pulValue);
  USHORT GetAlternateFirmwareRev (PULONG pulValue);

  USHORT GetSerialNumber (PULONG pulSerialNumber);
  USHORT SetSerialNumber (ULONG ulSerialNumber);

  USHORT GetLSlotProductID (PULONG pulValue);
  USHORT GetLSlotPCBRev (PULONG pulValue);
  USHORT GetLSlotFirmwareRev (PULONG pulValue);
  USHORT GetLSlotMode (PULONG pulValue);
  USHORT SetLSlotMode (ULONG ulValue);

  // Firmware Update
  USHORT SelectAlternateBank (void);
  USHORT StartFirmwareErase (void);
  USHORT GetBlockIndex (PBYTE pucBlockIndex);
  USHORT GetProgrammingStatus (PBYTE pucStatus);
  USHORT WriteFirmwarePacket (PBYTE pucFirmwarePacket, ULONG ulLength);
  USHORT RequestVerify (void);
  USHORT ReadVerifyPacket (PBYTE pucFirmwarePacket, ULONG ulLength);
  USHORT SetProgrammingComplete (void);
  USHORT AbortFirmwareUpdate (void);
  USHORT WriteSerialNumber (void);
  USHORT SelectLSlot (ULONG ulValue);

  USHORT AES16WriteCommand (BYTE ucCommand);
  USHORT AES16WriteFirmwarePacket (PBYTE pucFirmwarePacket, ULONG ulLength);
  USHORT AES16ReadVerifyPacket (PBYTE pucFirmwarePacket, ULONG ulLength);

  USHORT GetSharedControls (USHORT usControl,
			    PAURORASHAREDCONTROLS pSharedControls);
  USHORT AES16GetSharedControls (USHORT usControl,
				 PAURORASHAREDCONTROLS pSharedControls);
  USHORT AES16GetPMixStatus (BOOLEAN bDriverLoad);

  // Used in OSX driver!
  USHORT AES16GetLINKPID (PUSHORT pusValue);
  USHORT AES16GetPGMSTAT (PUSHORT pusValue);
  USHORT AES16GetREV (PUSHORT pusValue);

  USHORT SetMixerControl (USHORT usSrcLine, USHORT usControl, ULONG ulValue);
  USHORT GetMixerControl (USHORT usSrcLine, USHORT usControl, PULONG pulValue,
			  ULONG ulSize = sizeof (ULONG));

private:
  USHORT AES16WriteControl (ULONG ulControlRegister, BYTE ucValue,
			    BYTE ucMask);
  BYTE AES16ReadStatus (ULONG ulStatusRegister);

  USHORT ReadCurrentStatus (void);
  USHORT AES16ReadCurrentStatus (void);

  USHORT ReadStaticStatus (void);
  USHORT AES16ReadStaticStatus (void);

  void UpdateVolume (ULONG ulChannel, ULONG ulTheSource);

  USHORT AES16ReadLinkPID (BOOLEAN bMixerNotify = FALSE);

  USHORT AES16SelectAlternateBank (void);
  USHORT AES16StartFirmwareErase (void);
  USHORT AES16GetBlockIndex (PBYTE pucBlockIndex);
  USHORT AES16GetProgrammingStatus (PBYTE pucStatus);
  USHORT AES16RequestVerify (void);
  USHORT AES16SetProgrammingComplete (void);
  USHORT AES16AbortFirmwareUpdate (void);
  USHORT AES16WriteSerialNumber (void);

  PHALADAPTER m_pHalAdapter;
  PHALMIXER m_pHalMixer;
  PHAL4114 m_pHal4114;
  PHALAES16E m_pAES16eDIO;
  ULONG m_ulPort;

  PAURORABLK m_pAuroraBlk;

  // AES16 Shadows
  USHORT m_usRegLINKPID;
  USHORT m_usRegTRIMCLK;
  USHORT m_usRegMISC;
  USHORT m_usRegLOCKSTAT;
  USHORT m_usRegPGMSTAT;
  USHORT m_usRegREV;
  USHORT m_usRegSNDATE;
  USHORT m_usRegSNUNIT;
  USHORT m_usRegLTPCBREVMODE;

  // Static Control Shadows
  BOOLEAN m_bStaticStatusUpToDate;
  USHORT m_usProductID;
  BOOLEAN m_bFWBank;
  USHORT m_usCurFWRev;
  USHORT m_usAltFWRev;
  USHORT m_usPCBRev;
  ULONG m_ulSerialNumber;
  USHORT m_usLTPCBREV;

  // Dynamic Control Shadows

  // TRIMCLK
  ULONG m_ulTrim[AURORA_NUM_TRIM];
  ULONG m_ulClockSource;
  BOOLEAN m_bSynchroLockEnable;
  ULONG m_ulClockRate;

  // MISC
  ULONG m_ulAnalogOutSource;
  ULONG m_ulDigitalOutSource;
  BOOLEAN m_bDualWireIn;
  BOOLEAN m_bDualWireOut;
  ULONG m_ulFPMeterSource;
  ULONG m_ulLSlotSource;
  ULONG m_ulTrimOrigin;
  ULONG m_ulLocalTrim;

  // LOCKSTAT
  ULONG m_ulAESLockStatus;
  ULONG m_ulSynchroLockStatus;

  // OVERLOAD
  ULONG m_ulOverloadCount[64];

#define AURORA_NUMBER_OF_OUTPUTS	32

  BYTE m_ucMonitorSource[AURORA_NUMBER_OF_OUTPUTS][AURORA_NUMBER_OF_PMIX];
  BYTE m_ucMonitorVolume[AURORA_NUMBER_OF_OUTPUTS][AURORA_NUMBER_OF_PMIX];
  BOOLEAN m_bMonitorMute[AURORA_NUMBER_OF_OUTPUTS][AURORA_NUMBER_OF_PMIX];

  //BYTE                  m_ucMasterVolume[ AURORA_NUMBER_OF_OUTPUTS ];
  //BOOLEAN                       m_bMasterMute[ AURORA_NUMBER_OF_OUTPUTS ];
  BOOLEAN m_bDither[AURORA_NUMBER_OF_OUTPUTS];

  BYTE m_aucControlRegisters[REG_ACTL_NUMBER_OF_REGISTERS];
  BYTE m_aucStatusRegisters[REG_ASTAT_NUMBER_OF_REGISTERS];
};

#endif // _HALAURORA_H
