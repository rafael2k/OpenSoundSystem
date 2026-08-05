/****************************************************************************
 HalAurora.cpp

 Description:	Lynx Application Programming Interface Header File

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

#include <StdAfx.h>
#include "HalAdapter.h"
#include "HalAurora.h"

#include <AuroraSharedControls.h>
#include <ControlList.h>

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::Open (PHALADAPTER pHalAdapter)
/////////////////////////////////////////////////////////////////////////////
{
  int nReg, nChannel;

  //cmn_err((CE_WARN,"CHalAurora::Open\n");

  // save the pointer to the parent adapter object
  m_pHalAdapter = pHalAdapter;
  m_pHalMixer = m_pHalAdapter->GetMixer ();
  m_pHal4114 = m_pHalAdapter->Get4114 ();
  m_pAES16eDIO = m_pHalAdapter->GetAES16eDIO ();

  //cmn_err((CE_WARN,"AURORABLK %08lx\n", (ULONG)m_pAuroraBlk );

  switch (pHalAdapter->GetDeviceID ())
    {
    case PCIDEVICE_LYNXTWO_A:
    case PCIDEVICE_LYNXTWO_B:
    case PCIDEVICE_LYNXTWO_C:
    case PCIDEVICE_LYNX_L22:
      // LStream 1 or 2
      m_ulPort = AURORA_PORT_LSTREAM1;	// assume LStream 1 for now
      break;
    case PCIDEVICE_LYNX_AES16:
    case PCIDEVICE_LYNX_AES16SRC:
    case PCIDEVICE_LYNX_AES16e:
    case PCIDEVICE_LYNX_AES16eSRC:
    case PCIDEVICE_LYNX_AES16e50:
      m_pAuroraBlk = m_pHalAdapter->GetAuroraBlk ();
      m_ulPort = AURORA_PORT_AES16;
      break;
    }

  // blast the shadow register values (only used for volume)
  for (nReg = 0; nReg < REG_ACTL_NUMBER_OF_REGISTERS; nReg++)
    m_aucControlRegisters[nReg] = 0xA5;

  for (nChannel = 0; nChannel < 64; nChannel++)
    m_ulOverloadCount[nChannel] = 0;

  // blast the shadow status registers (these are currently not used)
  for (nReg = 0; nReg < REG_ASTAT_NUMBER_OF_REGISTERS; nReg++)
    m_aucStatusRegisters[nReg] = 0;

  m_bStaticStatusUpToDate = FALSE;

  if (m_ulPort == AURORA_PORT_AES16)
    AES16GetPMixStatus (TRUE);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::Close ()
/////////////////////////////////////////////////////////////////////////////
{
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetDefaults (void)
/////////////////////////////////////////////////////////////////////////////
{
  int nChannel;

  //cmn_err (CE_WARN, "CHalAurora::SetDefaults\n");
  WriteControl (REG_ACTL_COMMAND, REG_ACTL_COMMAND_RESTORE_DEFAULTS);

  for (nChannel = 0; nChannel < 64; nChannel++)
    m_ulOverloadCount[nChannel] = 0;

  m_bStaticStatusUpToDate = FALSE;
  ReadStaticStatus ();

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::WriteControl (ULONG ulControlRegister, BYTE ucValue,
			    BYTE ucMask)
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usStatus = HSTATUS_OK;

  //cmn_err((CE_WARN,"CHalAurora::WriteControl %lu\n", ulControlRegister );

  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      usStatus = AES16WriteControl (ulControlRegister, ucValue, ucMask);
      break;
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
      break;
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }

  return (usStatus);
}

/////////////////////////////////////////////////////////////////////////////
BYTE
CHalAurora::ReadStatus (ULONG ulStatusRegister)
// Called from GetLevel, which is no longer used
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucValue;

  ucValue = m_aucStatusRegisters[ulStatusRegister];

  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      ucValue = AES16ReadStatus (ulStatusRegister);
      break;
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
      break;
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }

  // save this off to the shadow register (probably not needed)
  m_aucStatusRegisters[ulStatusRegister] = ucValue;

  return (ucValue);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::AES16WriteControl (ULONG ulControlRegister, BYTE ucValue,
				 BYTE ucMask)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulValue, usValue;
  BYTE ucShadow;

  //cmn_err((CE_WARN,"CHalAurora::WriteControlAES16 %lu\n", ulControlRegister );

  // There are two groups of control registers

  // Settings
  if (ulControlRegister < REG_ACTL_PMIX0_VOLA)
    {
      switch (ulControlRegister)
	{
	case REG_ACTL_TRIM:
	  m_usRegTRIMCLK =
	    (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->TRIMCLKSTAT);
	  // extract the trim byte
	  ucShadow = LOBYTE (m_usRegTRIMCLK);

	  // change value
	  CLR (ucShadow, ucMask);
	  SET (ucShadow, (ucValue & ucMask));

	  // put humpty together again
	  ulValue = MAKEWORD (ucShadow, HIBYTE (m_usRegTRIMCLK));
	  m_usRegTRIMCLK = (USHORT) ulValue;

	  //cmn_err (CE_WARN, "Write TRIMCLKCTL %04lx\n", ulValue);
	  WRITE_REGISTER_ULONG (&m_pAuroraBlk->TRIMCLKCTL,
				(REG_ACTL_WREQ | ulValue));
	  break;
	case REG_ACTL_CLOCK:
	  m_usRegTRIMCLK =
	    (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->TRIMCLKSTAT);
	  // extract the clock byte
	  ucShadow = HIBYTE (m_usRegTRIMCLK);

	  //cmn_err((CE_WARN,"REG_ACTL_CLOCK BEFORE m_usRegTRIMCLK %04x V %02x M %02x\n", m_usRegTRIMCLK, (USHORT)ucValue, (USHORT)ucMask );

	  // change value
	  CLR (ucShadow, ucMask);
	  SET (ucShadow, (ucValue & ucMask));

	  // put humpty together again
	  ulValue = MAKEWORD (LOBYTE (m_usRegTRIMCLK), ucShadow);
	  m_usRegTRIMCLK = (USHORT) ulValue;

	  //cmn_err (CE_WARN, "Write TRIMCLKCTL %04lx\n", ulValue);
	  WRITE_REGISTER_ULONG (&m_pAuroraBlk->TRIMCLKCTL,
				(REG_ACTL_WREQ | ulValue));
	  break;
	case REG_ACTL_MISCLO:
	  m_usRegMISC =
	    (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->MISCSTAT);
	  ucShadow = LOBYTE (m_usRegMISC);

	  // change value
	  CLR (ucShadow, ucMask);
	  SET (ucShadow, (ucValue & ucMask));

	  // put humpty together again
	  ulValue = MAKEWORD (ucShadow, HIBYTE (m_usRegMISC));
	  m_usRegMISC = (USHORT) ulValue;

	  //cmn_err (CE_WARN, "Write MISCCTL %04lx\n", ulValue);
	  WRITE_REGISTER_ULONG (&m_pAuroraBlk->MISCCTL,
				(REG_ACTL_WREQ | ulValue));
	  break;
	case REG_ACTL_MISCHI:
	  m_usRegMISC =
	    (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->MISCSTAT);
	  ucShadow = HIBYTE (m_usRegMISC);

	  // change value
	  CLR (ucShadow, ucMask);
	  SET (ucShadow, (ucValue & ucMask));

	  // put humpty together again
	  ulValue = MAKEWORD (LOBYTE (m_usRegMISC), ucShadow);
	  m_usRegMISC = (USHORT) ulValue;

	  //cmn_err (CE_WARN, "Write MISCCTL %04lx\n", ulValue);
	  WRITE_REGISTER_ULONG (&m_pAuroraBlk->MISCCTL,
				(REG_ACTL_WREQ | ulValue));
	  break;
	}
    }
  // PMIXes
  else
    {
      ULONG ulOffset, ulPMix, ulAorB, ulVorS;

      ulOffset = ulControlRegister - REG_ACTL_PMIX0_VOLA;

      ulPMix = ulOffset / 4;	// 0..31
      ulAorB = (ulOffset - (ulPMix * 4)) / 2;	// 0..1
      ulVorS = (ulOffset - (ulPMix * 4)) % 2;	// 0..1

      // read the status register (this will pickup the current dither status)
      if (ulAorB == 0)
	usValue =
	  (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PMIXSTAT[ulPMix].
					PMixA);
      else
	usValue =
	  (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PMIXSTAT[ulPMix].
					PMixB);

      //cmn_err((CE_WARN,"Read PMIXSTAT%c[%d] %04lx\n", ulAorB == 0 ? 'A' : 'B', ulPMix, usValue );

      // extract the byte we are looking to change
      if (ulVorS == 0)
	ucShadow = LOBYTE (usValue);	// volume
      else
	ucShadow = HIBYTE (usValue);	// source

      // apply the new value
      CLR (ucShadow, ucMask);
      SET (ucShadow, (ucValue & ucMask));

      // put it back
      if (ulVorS == 0)
	usValue = MAKEWORD (ucShadow, HIBYTE (usValue));	// volume
      else
	usValue = MAKEWORD (LOBYTE (usValue), ucShadow);	// source

      //cmn_err((CE_WARN,"Write PMIXCTL%c[%d] %04lx\n", ulAorB == 0 ? 'A' : 'B', ulPMix, usValue );

      if (ulAorB == 0)
	{
	  WRITE_REGISTER_ULONG (&m_pAuroraBlk->PMIXCTL[ulPMix].PMixA,
				(REG_ACTL_WREQ | usValue));
	}
      else
	{
	  WRITE_REGISTER_ULONG (&m_pAuroraBlk->PMIXCTL[ulPMix].PMixB,
				(REG_ACTL_WREQ | usValue));
	}
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
BYTE
CHalAurora::AES16ReadStatus (ULONG ulStatusRegister)
// Only Called from GetLevel, which is no longer used
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulValue;

  //DC('S');

  if (ulStatusRegister < REG_ASTAT_AINLVL1)
    {
/*		switch( ulStatusRegister )
		{
		case REG_ASTAT_AESLOCK:
		case REG_ASTAT_SLOCK:
			m_usRegLOCKSTAT = (USHORT)READ_REGISTER_ULONG( &m_pAuroraBlk->LOCKSTAT );
			if( ulStatusRegister == REG_ASTAT_AESLOCK )	return( LOBYTE(m_usRegLOCKSTAT) );
			else										return( HIBYTE(m_usRegLOCKSTAT) );
			break;
		}
*/
    }
  // Levels
  else if (ulStatusRegister < REG_ASTAT_PMIX0_VOLA)
    {
      ULONG ulOffset, ulGroup, ulPair, ulChannel;

      ulOffset = ulStatusRegister - REG_ASTAT_AINLVL1;
      // four groups of levels...
      ulGroup = ulOffset / 16;	// 0..3
      ulPair = (ulOffset - (ulGroup * 16)) / 2;	// 0..7
      ulChannel = ulOffset % 2;	// 0..1

      switch (ulGroup)
	{
	case 0:
	  ulValue =
	    READ_REGISTER_ULONG (&m_pAuroraBlk->AINLVL.Levels[ulPair]);
	  break;		// Analog In
	case 1:
	  ulValue =
	    READ_REGISTER_ULONG (&m_pAuroraBlk->AOUTLVL.Levels[ulPair]);
	  break;		// Analog Out
	case 2:
	  ulValue =
	    READ_REGISTER_ULONG (&m_pAuroraBlk->DINLVL.Levels[ulPair]);
	  break;		// Digital In
	case 3:
	  ulValue =
	    READ_REGISTER_ULONG (&m_pAuroraBlk->DOUTLVL.Levels[ulPair]);
	  break;		// Digital Out
	}

      if (ulChannel == 0)
	return (LOBYTE (ulValue));
      else
	return (HIBYTE (ulValue));
    }
  // PMIXes
  else
    {
      ULONG ulOffset, ulPMix, ulAorB, ulVorS;

      ulOffset = ulStatusRegister - REG_ASTAT_PMIX0_VOLA;

      ulPMix = ulOffset / 4;	// 0..31
      ulAorB = (ulOffset - (ulPMix * 4)) / 2;	// 0..1
      ulVorS = (ulOffset - (ulPMix * 4)) % 2;	// 0..1

      if (ulAorB == 0)
	ulValue = READ_REGISTER_ULONG (&m_pAuroraBlk->PMIXSTAT[ulPMix].PMixA);
      else
	ulValue = READ_REGISTER_ULONG (&m_pAuroraBlk->PMIXSTAT[ulPMix].PMixB);

      if (ulVorS == 0)
	return (LOBYTE (ulValue));	// volume
      else
	return (HIBYTE (ulValue));	// source
    }

  return (0);			// keep the compiler happy
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SampleClockChanged (LONG lRate, LONG lSource)
// Called by CHalSampleClock when the clock rate is changed
/////////////////////////////////////////////////////////////////////////////
{
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetClockSource (ULONG ulClockSource)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN, "CHalAurora::SetClockSource %02lx\n", ulClockSource);

  switch (ulClockSource)
    {
    case MIXVAL_AURORA_CLKSRC_INTERNAL:
      m_ulClockSource = REG_TRIMCLK_CLKSRC_INTERNAL;
      break;
    case MIXVAL_AURORA_CLKSRC_EXTERNAL:
      m_ulClockSource = REG_TRIMCLK_CLKSRC_EXTERNAL;
      break;
    case MIXVAL_AURORA_CLKSRC_EXTERNAL_DIV2:
      m_ulClockSource = REG_TRIMCLK_CLKSRC_EXTERNAL_DIV2;
      break;
    case MIXVAL_AURORA_CLKSRC_AESA:
      m_ulClockSource = REG_TRIMCLK_CLKSRC_AESA;
      break;
    case MIXVAL_AURORA_CLKSRC_AESB:
      m_ulClockSource = REG_TRIMCLK_CLKSRC_AESB;
      break;
    case MIXVAL_AURORA_CLKSRC_LSLOT:
      m_ulClockSource = REG_TRIMCLK_CLKSRC_LSLOT;
      break;
    case MIXVAL_AURORA_CLKSRC_EXTERNAL_DIV4:
      m_ulClockSource = REG_TRIMCLK_CLKSRC_EXTERNAL_DIV4;
      break;
    default:
      return (HSTATUS_INVALID_MIXER_VALUE);
    }

  WriteControl (REG_ACTL_CLOCK, (BYTE) m_ulClockSource,
		REG_ACTL_CLOCK_CLKSRC_MASK);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetClockSource (PULONG pulClockSource)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulClockSource;

  switch (m_ulClockSource)
    {
    case REG_TRIMCLK_CLKSRC_INTERNAL:
      ulClockSource = MIXVAL_AURORA_CLKSRC_INTERNAL;
      break;
    case REG_TRIMCLK_CLKSRC_EXTERNAL:
      ulClockSource = MIXVAL_AURORA_CLKSRC_EXTERNAL;
      break;
    case REG_TRIMCLK_CLKSRC_EXTERNAL_DIV2:
      ulClockSource = MIXVAL_AURORA_CLKSRC_EXTERNAL_DIV2;
      break;
    case REG_TRIMCLK_CLKSRC_AESA:
      ulClockSource = MIXVAL_AURORA_CLKSRC_AESA;
      break;
    case REG_TRIMCLK_CLKSRC_AESB:
      ulClockSource = MIXVAL_AURORA_CLKSRC_AESB;
      break;
    case REG_TRIMCLK_CLKSRC_LSLOT:
      ulClockSource = MIXVAL_AURORA_CLKSRC_LSLOT;
      break;
    case REG_TRIMCLK_CLKSRC_EXTERNAL_DIV4:
      ulClockSource = MIXVAL_AURORA_CLKSRC_EXTERNAL_DIV4;
      break;
    }

  *pulClockSource = ulClockSource;

  ReadCurrentStatus ();

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetSynchroLockEnable (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_bSynchroLockEnable = (BOOLEAN) ulValue;
  WriteControl (REG_ACTL_CLOCK,
		(BYTE) (ulValue ? REG_ACTL_CLOCK_SYNCHROLOCK : 0),
		REG_ACTL_CLOCK_SYNCHROLOCK);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetSampleRate (ULONG ulSampleRate)
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucValue;

  //cmn_err (CE_WARN, "SetSampleRate %lu\n", ulSampleRate);

  switch (ulSampleRate)
    {
    default:
    case 44100:
      ucValue = MIXVAL_AURORA_SR44100;
      break;
    case 48000:
      ucValue = MIXVAL_AURORA_SR48000;
      break;
    case 88200:
      ucValue = MIXVAL_AURORA_SR88200;
      break;
    case 96000:
      ucValue = MIXVAL_AURORA_SR96000;
      break;
    case 176400:
      ucValue = MIXVAL_AURORA_SR176400;
      break;
    case 192000:
      ucValue = MIXVAL_AURORA_SR192000;
      break;
    }

  m_ulClockRate = (ULONG) ucValue;
  WriteControl (REG_ACTL_CLOCK, (ucValue << REG_ACTL_CLOCK_SR_OFFSET),
		REG_ACTL_CLOCK_SR_MASK);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetSampleRate (PULONG pulSampleRate)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulClockRate)
    {
    default:
    case MIXVAL_AURORA_SR44100:
      *pulSampleRate = 44100;
      break;
    case MIXVAL_AURORA_SR48000:
      *pulSampleRate = 48000;
      break;
    case MIXVAL_AURORA_SR88200:
      *pulSampleRate = 88200;
      break;
    case MIXVAL_AURORA_SR96000:
      *pulSampleRate = 96000;
      break;
    case MIXVAL_AURORA_SR176400:
      *pulSampleRate = 176400;
      break;
    case MIXVAL_AURORA_SR192000:
      *pulSampleRate = 192000;
      break;
    }

  ReadCurrentStatus ();

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// MISCCTL / MISCSTAT
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetAnalogOutSource (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucValue;

  m_ulAnalogOutSource = ulValue;

  switch (m_ulAnalogOutSource)
    {
    case MIXVAL_AURORA_OUTSOURCE_ANALOGIN:
      ucValue = REG_MISC_AOUTSRC_AIN;
      break;
    case MIXVAL_AURORA_OUTSOURCE_AESIN:
      ucValue = REG_MISC_AOUTSRC_DIN;
      break;
    case MIXVAL_AURORA_OUTSOURCE_LSLOTIN:
      ucValue = REG_MISC_AOUTSRC_LSLOTIN;
      break;
    case MIXVAL_AURORA_OUTSOURCE_REMOTE:
      ucValue = REG_MISC_AOUTSRC_REMOTE;
      break;
    }

  WriteControl (REG_ACTL_MISCLO, ucValue, REG_MISC_AOUTSRC_MASK);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetDigitalOutSource (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucValue;

  m_ulDigitalOutSource = ulValue;

  switch (m_ulDigitalOutSource)
    {
    case MIXVAL_AURORA_OUTSOURCE_ANALOGIN:
      ucValue = REG_MISC_DOUTSRC_AIN;
      break;
    case MIXVAL_AURORA_OUTSOURCE_AESIN:
      ucValue = REG_MISC_DOUTSRC_DIN;
      break;
    case MIXVAL_AURORA_OUTSOURCE_LSLOTIN:
      ucValue = REG_MISC_DOUTSRC_LSLOTIN;
      break;
    case MIXVAL_AURORA_OUTSOURCE_REMOTE:
      ucValue = REG_MISC_DOUTSRC_REMOTE;
      break;
    }

  WriteControl (REG_ACTL_MISCLO, ucValue, REG_MISC_DOUTSRC_MASK);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetDualWireIn (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_bDualWireIn = (BOOLEAN) ulValue;
  WriteControl (REG_ACTL_MISCLO, (BYTE) (ulValue ? REG_MISC_DUALWIREIN : 0),
		REG_MISC_DUALWIREIN);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetDualWireOut (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_bDualWireOut = (BOOLEAN) ulValue;
  WriteControl (REG_ACTL_MISCLO, (BYTE) (ulValue ? REG_MISC_DUALWIREOUT : 0),
		REG_MISC_DUALWIREOUT);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetFPMeterSource (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_ulFPMeterSource = ulValue;
  WriteControl (REG_ACTL_MISCLO,
		(BYTE) (ulValue ? REG_MISC_FPMETERSOURCE_DIGITAL :
			REG_MISC_FPMETERSOURCE_ANALOG),
		REG_MISC_FPMETERSOURCE_MASK);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetLSlotOutSource (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_ulLSlotSource = ulValue;
  WriteControl (REG_ACTL_MISCLO,
		(BYTE) (ulValue ? REG_MISC_LSLOTSRC_DIGITAL :
			REG_MISC_LSLOTSRC_ANALOG), REG_MISC_LSLOTSRC_MASK);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetTrimOrigin (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_ulTrimOrigin = ulValue;
  WriteControl (REG_ACTL_MISCHI,
		(BYTE) (ulValue ? REG_MISCHI_TRIMORG_REMOTE :
			REG_MISCHI_TRIMORG_LOCAL), REG_MISCHI_TRIMORG_REMOTE);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetLocalTrim (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_ulLocalTrim = ulValue;
  if (ulValue == MIXVAL_TRIM_PLUS4)
    WriteControl (REG_ACTL_MISCHI, REG_MISCHI_LOCALTRIM_PLUS4,
		  REG_MISCHI_LOCALTRIM_MASK);
  else
    WriteControl (REG_ACTL_MISCHI, REG_MISCHI_LOCALTRIM_MINUS10,
		  REG_MISCHI_LOCALTRIM_MASK);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetTrim (ULONG ulControl, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_ulTrim[ulControl] = ulValue;

  if (ulValue == MIXVAL_TRIM_PLUS4)
    WriteControl (REG_ACTL_TRIM, (BYTE) (1 << ulControl),
		  (BYTE) (1 << ulControl));
  else
    WriteControl (REG_ACTL_TRIM, (BYTE) 0, (BYTE) (1 << ulControl));
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetTrim (ULONG ulControl, PULONG pulValue)
// ulControl is 0..7
/////////////////////////////////////////////////////////////////////////////
{
  *pulValue = m_ulTrim[ulControl];

  if (ulControl == AURORA_TRIMIN0)
    {
      ReadCurrentStatus ();
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::SetMonitorSource (ULONG ulChannel, ULONG ulTheSource,
				ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulRegister;

  m_ucMonitorSource[ulChannel][ulTheSource] = (BYTE) ulValue;

  // look at the shadow register for this control
  if (ulTheSource == AURORA_PMIXA)
    ulRegister =
      REG_ACTL_PMIX0_SRCA + (ulChannel * REG_ACTL_PMIX_CHANNEL_OFFSET);
  else
    ulRegister =
      REG_ACTL_PMIX0_SRCB + (ulChannel * REG_ACTL_PMIX_CHANNEL_OFFSET);

  WriteControl (ulRegister, m_ucMonitorSource[ulChannel][ulTheSource],
		REG_ACTL_PMIX_SRC_MASK);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::GetMonitorSource (ULONG ulChannel, ULONG ulTheSource,
				PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  *pulValue = (ULONG) m_ucMonitorSource[ulChannel][ulTheSource];
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::SetVolume (ULONG ulChannel, ULONG ulTheSource, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulRegister;

  // make sure we don't exceed the valid value
  if (ulValue > AURORA_VOLUME_MIN)
    ulValue = AURORA_VOLUME_MIN;
  //if( ulValue < AURORA_VOLUME_MAX )     // this causes the AMD64 compiler to choke
  //      ulValue = AURORA_VOLUME_MAX;

  m_ucMonitorVolume[ulChannel][ulTheSource] = (BYTE) ulValue;

  if (ulTheSource == AURORA_PMIXA)
    ulRegister =
      REG_ACTL_PMIX0_VOLA + (ulChannel * REG_ACTL_PMIX_CHANNEL_OFFSET);
  else
    ulRegister =
      REG_ACTL_PMIX0_VOLB + (ulChannel * REG_ACTL_PMIX_CHANNEL_OFFSET);

  if (m_bMonitorMute[ulChannel][ulTheSource])
    WriteControl (ulRegister, (BYTE) AURORA_VOLUME_MUTE);
  else
    WriteControl (ulRegister, (BYTE) ulValue);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::GetVolume (ULONG ulChannel, ULONG ulTheSource, PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  *pulValue = (ULONG) m_ucMonitorVolume[ulChannel][ulTheSource];
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetMute (ULONG ulChannel, ULONG ulTheSource, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_bMonitorMute[ulChannel][ulTheSource] = (BOOLEAN) ulValue;

  SetVolume (ulChannel, ulTheSource,
	     m_ucMonitorVolume[ulChannel][ulTheSource]);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::GetMute (ULONG ulChannel, ULONG ulTheSource, PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  *pulValue = (ULONG) m_bMonitorMute[ulChannel][ulTheSource];
  return (HSTATUS_OK);
}

/*
/////////////////////////////////////////////////////////////////////////////
USHORT	CHalAurora::SetDither( ULONG ulChannel, ULONG ulValue )
/////////////////////////////////////////////////////////////////////////////
{
	ULONG	ulRegister;
	BYTE	ucValue;

	m_bDither[ ulChannel ] = (BOOLEAN)ulValue;
	
	ulRegister = REG_ACTL_PMIX0_SRCA + (ulChannel * REG_ACTL_PMIX_CHANNEL_OFFSET);
	ucValue = m_bDither[ ulChannel ] ? REG_ACTL_PMIX_SRCA_DITHER : 0;

	WriteControl( ulRegister, ucValue, REG_ACTL_PMIX_SRCA_DITHER );

	return( HSTATUS_OK );
}

/////////////////////////////////////////////////////////////////////////////
USHORT	CHalAurora::GetDither( ULONG ulChannel, PULONG pulValue )
/////////////////////////////////////////////////////////////////////////////
{
	*pulValue = (ULONG)m_bDither[ ulChannel ];
	return( HSTATUS_OK );
}
*/
/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::ResetOverload (USHORT usSrcLine)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulChannel = usSrcLine - LINE_ANALOGIN_1;
  m_ulOverloadCount[ulChannel] = 0;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetOverload (USHORT usSrcLine, PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  *pulValue = m_ulOverloadCount[usSrcLine - LINE_ANALOGIN_1];
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetLevel (USHORT usSrcLine, PULONG pulValue)
// This is no longer used
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulStatusRegister = 0;
  ULONG ulLine, ulGroup, ulChannel;
  BYTE ucLevel;

  ulLine = usSrcLine - LINE_ANALOGIN_1;

  ulGroup = ulLine / 16;	// 0..3
  ulChannel = ulLine - (ulGroup * 16);

  switch (ulGroup)
    {
    case 0:			// Analog In
      // get the digital in from the AES16...
      //return( m_pHalAdapter->GetMixer()->GetControl( LINE_RECORD_0 + (ulChannel/2), LINE_NO_SOURCE, CONTROL_PEAKMETER, (ulChannel%2), pulValue ) );
      ulStatusRegister = REG_ASTAT_AINLVL1 + ulChannel;
      break;
    case 1:			// Digital In
      //return( m_pHalAdapter->GetMixer()->GetControl( LINE_OUT_1 + ulChannel, LINE_NO_SOURCE, CONTROL_PEAKMETER, 0, pulValue ) );
      ulStatusRegister = REG_ASTAT_DINLVL1 + ulChannel;
      break;
    case 2:			// Analog Out
      // get the digital out from the AES16...
      //return( m_pHalAdapter->GetMixer()->GetControl( LINE_OUT_1 + ulChannel, LINE_NO_SOURCE, CONTROL_PEAKMETER, 0, pulValue ) );
      ulStatusRegister = REG_ASTAT_AOUTLVL1 + ulChannel;
      break;
    case 3:			// Digital Out
      //return( m_pHalAdapter->GetMixer()->GetControl( LINE_RECORD_0 + (ulChannel/2), LINE_NO_SOURCE, CONTROL_PEAKMETER, (ulChannel%2), pulValue ) );
      ulStatusRegister = REG_ASTAT_DOUTLVL1 + ulChannel;
      break;
    default:
      return (HSTATUS_INVALID_PARAMETER);
    }

  ucLevel = ReadStatus (ulStatusRegister);

  // a value of 0xFF means overload was detected
  if (ucLevel == 0xFF)
    {
      ucLevel = 0;		// max out the level
      m_ulOverloadCount[ulLine]++;
      // max out the overload count at 9
      if (m_ulOverloadCount[ulLine] > 9)
	m_ulOverloadCount[ulLine] = 9;
    }

  *pulValue = (ULONG) ucLevel;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::ReadCurrentStatus (void)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16ReadCurrentStatus ();
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16ReadCurrentStatus (void)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulValue;
  USHORT usValue;
  BYTE ucValue;
  BOOLEAN bValue;

  // It is possible that the user disconnected an Aurora 16, and connected 
  // an Aurora 8 - we detect that here.

  AES16ReadLinkPID (TRUE);

  usValue = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->TRIMCLKSTAT);
  if (m_usRegTRIMCLK != usValue)
    {
      int nTrim;

      m_usRegTRIMCLK = usValue;

      //cmn_err((CE_WARN,"TRIMCLKSTAT %04x\n", m_usRegTRIMCLK );

      // decode TRIMCLK and put it into the control shadow registers

      // Trim Portion
      for (nTrim = 0; nTrim < AURORA_NUM_TRIM; nTrim++)
	{
	  ulValue =
	    (m_usRegTRIMCLK & (1 << nTrim)) ? MIXVAL_TRIM_PLUS4 :
	    MIXVAL_TRIM_MINUS10;

	  if (m_ulTrim[nTrim] != ulValue)
	    {
	      m_ulTrim[nTrim] = ulValue;
	      m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
					   CONTROL_TRIM_AIN_1_4 + nTrim);
	    }
	}

      // Clock Portion
      ucValue = HIBYTE (m_usRegTRIMCLK);

      ulValue = (ULONG) (ucValue & REG_ACTL_CLOCK_CLKSRC_MASK);
      if (m_ulClockSource != ulValue)
	{
	  m_ulClockSource = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_CLOCKSOURCE);
	}

      bValue = (ucValue & REG_ACTL_CLOCK_SYNCHROLOCK) ? TRUE : FALSE;
      if (m_bSynchroLockEnable != bValue)
	{
	  m_bSynchroLockEnable = bValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_SYNCHROLOCK_ENABLE);
	}

      ulValue =
	(ucValue & REG_ACTL_CLOCK_SR_MASK) >> REG_ACTL_CLOCK_SR_OFFSET;
      if (m_ulClockRate != ulValue)
	{
	  m_ulClockRate = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_CLOCKRATE_SELECT);
	}
    }

  usValue = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->MISCSTAT);
  if (m_usRegMISC != usValue)
    {
      m_usRegMISC = usValue;

      //cmn_err((CE_WARN,"MISCSTAT %04x\n", m_usRegMISC );

      // decode MISC and put it into the control shadow registers

      usValue = (m_usRegMISC & REG_MISC_AOUTSRC_MASK);
      switch (usValue)
	{
	case REG_MISC_AOUTSRC_AIN:
	  ulValue = MIXVAL_AURORA_OUTSOURCE_ANALOGIN;
	  break;
	case REG_MISC_AOUTSRC_DIN:
	  ulValue = MIXVAL_AURORA_OUTSOURCE_AESIN;
	  break;
	case REG_MISC_AOUTSRC_LSLOTIN:
	  ulValue = MIXVAL_AURORA_OUTSOURCE_LSLOTIN;
	  break;
	case REG_MISC_AOUTSRC_REMOTE:
	  ulValue = MIXVAL_AURORA_OUTSOURCE_REMOTE;
	  break;
	}
      if (m_ulAnalogOutSource != ulValue)
	{
	  m_ulAnalogOutSource = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_FP_ANALOGOUT_SOURCE);
	}

      usValue = (m_usRegMISC & REG_MISC_DOUTSRC_MASK);
      switch (usValue)
	{
	case REG_MISC_DOUTSRC_AIN:
	  ulValue = MIXVAL_AURORA_OUTSOURCE_ANALOGIN;
	  break;
	case REG_MISC_DOUTSRC_DIN:
	  ulValue = MIXVAL_AURORA_OUTSOURCE_AESIN;
	  break;
	case REG_MISC_DOUTSRC_LSLOTIN:
	  ulValue = MIXVAL_AURORA_OUTSOURCE_LSLOTIN;
	  break;
	case REG_MISC_DOUTSRC_REMOTE:
	  ulValue = MIXVAL_AURORA_OUTSOURCE_REMOTE;
	  break;
	}
      if (m_ulDigitalOutSource != ulValue)
	{
	  m_ulDigitalOutSource = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_FP_DIGITALOUT_SOURCE);
	}

      bValue = (m_usRegMISC & REG_MISC_DUALWIREIN) ? TRUE : FALSE;
      if (m_bDualWireIn != bValue)
	{
	  m_bDualWireIn = bValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_WIDEWIREIN);
	}

      bValue = (m_usRegMISC & REG_MISC_DUALWIREOUT) ? TRUE : FALSE;
      if (m_bDualWireOut != bValue)
	{
	  m_bDualWireOut = bValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_WIDEWIREOUT);
	}

      ulValue =
	(m_usRegMISC & REG_MISC_FPMETERSOURCE_MASK) >>
	REG_MISC_FPMETERSOURCE_OFFSET;
      if (m_ulFPMeterSource != ulValue)
	{
	  m_ulFPMeterSource = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_FP_METER_SOURCE);
	}

      ulValue =
	(m_usRegMISC & REG_MISC_LSLOTSRC_MASK) ==
	REG_MISC_LSLOTSRC_ANALOG ? MIXVAL_AURORA_LSLOTOUTSOURCE_ANALOGOUT :
	MIXVAL_AURORA_LSLOTOUTSOURCE_DIGITALOUT;
      if (m_ulLSlotSource != ulValue)
	{
	  m_ulLSlotSource = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_LSLOT_OUT_SOURCE);
	}

      ulValue =
	(m_usRegMISC & REG_MISC_TRIMORG_REMOTE) >> REG_MISC_TRIMORG_OFFSET;
      if (m_ulTrimOrigin != ulValue)
	{
	  m_ulTrimOrigin = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_TRIM_ORIGIN);
	}

      usValue =
	(m_usRegMISC & REG_MISC_LOCALTRIM_PLUS4) >> REG_MISC_LOCALTRIM_OFFSET;
      if (usValue)
	ulValue = MIXVAL_TRIM_PLUS4;
      else
	ulValue = MIXVAL_TRIM_MINUS10;
      if (m_ulLocalTrim != ulValue)
	{
	  m_ulLocalTrim = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_FP_TRIM);
	}
    }

  usValue = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->LOCKSTAT);
  if (m_usRegLOCKSTAT != usValue)
    {
      m_usRegLOCKSTAT = usValue;

      //cmn_err((CE_WARN,"LOCKSTAT %04x\n", m_usRegLOCKSTAT );

      // decode LOCK and put it into the control shadow registers
      ulValue = m_usRegLOCKSTAT & REG_LOCKSTAT_AESLOCK_MASK;
      if (m_ulAESLockStatus != ulValue)
	{
	  m_ulAESLockStatus = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_DIGITALIN_STATUS);
	}

      ulValue =
	(m_usRegLOCKSTAT & REG_LOCKSTAT_SLOCK_MASK) >>
	REG_LOCKSTAT_SLOCK_OFFSET;
      if (m_ulSynchroLockStatus != ulValue)
	{
	  m_ulSynchroLockStatus = ulValue;
	  m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
				       CONTROL_SYNCHROLOCK_STATUS);
	}
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
// Static Status
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::ReadStaticStatus (void)
// private
/////////////////////////////////////////////////////////////////////////////
{
  if (!m_bStaticStatusUpToDate)
    {
      if (m_ulPort == AURORA_PORT_AES16)
	AES16ReadStaticStatus ();
      else
	return (HSTATUS_INVALID_MODE);

      //m_bStaticStatusUpToDate = TRUE;
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16ReadStaticStatus (void)
/////////////////////////////////////////////////////////////////////////////
{
  // Read the registers from the AES16
  AES16ReadLinkPID ();
  m_usRegPGMSTAT = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PGMSTAT);
  m_usRegREV = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->REV);
  m_usRegSNDATE = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->SNDATESTAT);
  m_usRegSNUNIT = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->SNUNITSTAT);
/*
	cmn_err((CE_WARN,"LINKPID %04x\n", m_usRegLINKPID );
	cmn_err((CE_WARN,"PGMSTAT %04x\n", m_usRegPGMSTAT );
	cmn_err((CE_WARN,"REV     %04x\n", m_usRegREV );
	cmn_err((CE_WARN,"SNDATE  %04x\n", m_usRegSNDATE );
	cmn_err((CE_WARN,"SNUNIT  %04x\n", m_usRegSNUNIT );
*/
  // Now decode the registers and fill in the static control shadows
  m_bFWBank = (m_usRegPGMSTAT & REG_PGMSTAT_FWBANK) ? FW_BANK_2 : FW_BANK_1;

  m_usCurFWRev = (m_usRegREV & REG_ASTAT_REV_CURFWREV_MASK);
  m_usAltFWRev =
    (m_usRegREV & REG_ASTAT_REV_ALTFWREV_MASK) >>
    REG_ASTAT_REV_ALTFWREV_OFFSET;
  m_usPCBRev =
    (m_usRegREV & REG_ASTAT_REV_PCBREV_MASK) >> REG_ASTAT_REV_PCBREV_OFFSET;

  m_ulSerialNumber = 0;
  L2SN_SET_UNITID (m_ulSerialNumber, m_usRegSNUNIT);
  L2SN_SET_WEEK (m_ulSerialNumber, LOBYTE (m_usRegSNDATE));
  L2SN_SET_YEAR (m_ulSerialNumber, HIBYTE (m_usRegSNDATE));
  L2SN_SET_MODEL (m_ulSerialNumber, m_usProductID);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetProductID (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  ReadStaticStatus ();
  *pulValue = (ULONG) m_usProductID;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetPCBRev (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  ReadStaticStatus ();
  *pulValue = (ULONG) m_usPCBRev;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetCurrentFirmwareRev (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  ReadStaticStatus ();
  *pulValue = (ULONG) m_usCurFWRev;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetAlternateFirmwareRev (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  ReadStaticStatus ();
  *pulValue = (ULONG) m_usAltFWRev;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetSerialNumber (PULONG pulSerialNumber)
/////////////////////////////////////////////////////////////////////////////
{
  ReadStaticStatus ();
/*
	if( m_ulPort == AURORA_PORT_AES16 )
	{
		AES16ReadLinkPID();
		m_usRegSNDATE	= (USHORT)READ_REGISTER_ULONG( &m_pAuroraBlk->SNDATESTAT );
		m_usRegSNUNIT	= (USHORT)READ_REGISTER_ULONG( &m_pAuroraBlk->SNUNITSTAT );

		//cmn_err((CE_WARN,"LINKPID [%04x] SNDATE [%04x] SNUNIT [%04x]\n", m_usRegLINKPID, m_usRegSNDATE, m_usRegSNUNIT );

		// Now decode the registers and fill in the static control shadows
		m_ulSerialNumber = 0;
		L2SN_SET_UNITID( m_ulSerialNumber, m_usRegSNUNIT );
		L2SN_SET_WEEK( m_ulSerialNumber, LOBYTE( m_usRegSNDATE ) );
		L2SN_SET_YEAR( m_ulSerialNumber, HIBYTE( m_usRegSNDATE ) );
		L2SN_SET_MODEL( m_ulSerialNumber, m_usProductID );
	}
*/
  *pulSerialNumber = m_ulSerialNumber;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetSerialNumber (ULONG ulSerialNumber)
/////////////////////////////////////////////////////////////////////////////
{
  // BUGBUG: no error checking here!
  m_ulSerialNumber = ulSerialNumber;

  if (m_ulPort == AURORA_PORT_AES16)
    {
      m_usRegSNDATE =
	MAKEWORD (L2SN_GET_WEEK (m_ulSerialNumber),
		  L2SN_GET_YEAR (m_ulSerialNumber));
      m_usRegSNUNIT = (USHORT) L2SN_GET_UNITID (m_ulSerialNumber);

      WRITE_REGISTER_ULONG (&m_pAuroraBlk->SNDATECTL,
			    (REG_ACTL_WREQ | m_usRegSNDATE));
      WRITE_REGISTER_ULONG (&m_pAuroraBlk->SNUNITCTL,
			    (REG_ACTL_WREQ | m_usRegSNUNIT));
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetLSlotProductID (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN, "CHalAurora::GetLSlotProductID\n");

  BYTE ucProductID = 0;

  if (m_ulPort == AURORA_PORT_AES16)
    {
      USHORT usRegLTDEVIDFWREV;

      usRegLTDEVIDFWREV =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->LTDEVIDFWREVSTAT);

      ucProductID = LOBYTE (usRegLTDEVIDFWREV);
    }

  *pulValue = (ULONG) ucProductID;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetLSlotPCBRev (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucPCBRev = 0;

  if (m_ulPort == AURORA_PORT_AES16)
    {
      USHORT usLTPCBREVMODE;

      usLTPCBREVMODE =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->LTPCBREVMODESTAT);

      //cmn_err((CE_WARN,"usLTPCBREVMODE [%04x]\n", usLTPCBREVMODE);

      ucPCBRev = usLTPCBREVMODE & REG_LTPCBREVMODE_LSLOT_PCBREV_MASK;
    }

  *pulValue = (ULONG) ucPCBRev;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetLSlotFirmwareRev (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucFWRev = 0;

  if (m_ulPort == AURORA_PORT_AES16)
    {
      USHORT usRegLTDEVIDFWREV;

      usRegLTDEVIDFWREV =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->LTDEVIDFWREVSTAT);

      ucFWRev = HIBYTE (usRegLTDEVIDFWREV);
    }

  *pulValue = (ULONG) ucFWRev;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetLSlotMode (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  BYTE ucLSlotMode = 0;

  if (m_ulPort == AURORA_PORT_AES16)
    {
      USHORT usLTPCBREVMODE;

      usLTPCBREVMODE =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->LTPCBREVMODESTAT);

      ucLSlotMode = HIBYTE (usLTPCBREVMODE);
    }

  *pulValue = (ULONG) ucLSlotMode;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetLSlotMode (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  if (m_ulPort == AURORA_PORT_AES16)
    {
      USHORT usLTPCBREVMODE;

      usLTPCBREVMODE =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->LTPCBREVMODESTAT);

      CLR (usLTPCBREVMODE, REG_LTPCBREVMODE_LSLOT_LTMODE_MASK);
      SET (usLTPCBREVMODE, (ulValue << 8));

      WRITE_REGISTER_ULONG (&m_pAuroraBlk->LTPCBREVMODECTL,
			    (REG_ACTL_WREQ | usLTPCBREVMODE));
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
// Firmware Update
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SelectAlternateBank (void)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16SelectAlternateBank ();
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::StartFirmwareErase (void)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16StartFirmwareErase ();
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetBlockIndex (PBYTE pucBlockIndex)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16GetBlockIndex (pucBlockIndex);
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::GetProgrammingStatus (PBYTE pucStatus)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16GetProgrammingStatus (pucStatus);
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::WriteFirmwarePacket (PBYTE pucFirmwarePacket, ULONG ulLength)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16WriteFirmwarePacket (pucFirmwarePacket, ulLength);
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::RequestVerify (void)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16RequestVerify ();
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::ReadVerifyPacket (PBYTE pucFirmwarePacket, ULONG ulLength)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16ReadVerifyPacket (pucFirmwarePacket, ulLength);
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SetProgrammingComplete (void)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16SetProgrammingComplete ();
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AbortFirmwareUpdate (void)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16AbortFirmwareUpdate ();
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::WriteSerialNumber (void)
/////////////////////////////////////////////////////////////////////////////
{
  switch (m_ulPort)
    {
    case AURORA_PORT_AES16:
      return AES16WriteSerialNumber ();
    case AURORA_PORT_LSTREAM1:
    case AURORA_PORT_LSTREAM2:	// highly unlikely?
    case AURORA_PORT_MIDI:	// not possible
    case AURORA_PORT_IRDA:	// not possible
      break;
    }
  return (HSTATUS_INVALID_MODE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::SelectLSlot (ULONG ulValue)
// Only works with Aurora firmware Version 14 or higher 
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN, "CHalAurora::SelectLSlot %lu\n", ulValue);
  WriteControl (REG_ACTL_MISCHI,
		(BYTE) (ulValue ? REG_MISCHI_LTPGMEN_LSLOT :
			REG_MISCHI_LTPGMEN_AURORA), REG_MISCHI_LTPGMEN_LSLOT);
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16WriteCommand (BYTE ucCommand)
/////////////////////////////////////////////////////////////////////////////
{
  WRITE_REGISTER_ULONG (&m_pAuroraBlk->COMMAND, (REG_ACTL_WREQ | ucCommand));
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16SelectAlternateBank (void)
// This is the first command that comes down when the firmware update process
// begins.
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalAurora::AES16SelectAlternateBank\n");

  // Make sure m_bFWBank is up to date by forcing a re-read of the static status!
  AES16ReadStaticStatus ();

  // Make the Alternate Bank the Active Bank
  if (m_bFWBank == FW_BANK_1)
    {
      //cmn_err (CE_WARN, "REG_ACTL_COMMAND_CFG_BANK2_SELECT\n");
      AES16WriteCommand (REG_ACTL_COMMAND_CFG_BANK2_SELECT);
    }
  else
    {
      //cmn_err (CE_WARN, "REG_ACTL_COMMAND_CFG_BANK1_SELECT\n");
      AES16WriteCommand (REG_ACTL_COMMAND_CFG_BANK1_SELECT);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16StartFirmwareErase (void)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalAurora::AES16StartFirmwareErase\n");
  // Start the Erase Process
  return (AES16WriteCommand (REG_ACTL_COMMAND_ERASE_CFG_BANK));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16GetBlockIndex (PBYTE pucBlockIndex)
/////////////////////////////////////////////////////////////////////////////
{
  AES16ReadLinkPID ();
  *pucBlockIndex = LOBYTE (m_usRegLINKPID);
  //DPS(("(%02x)", (USHORT)*pucBlockIndex ));
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16GetProgrammingStatus (PBYTE pucStatus)
/////////////////////////////////////////////////////////////////////////////
{
  m_usRegPGMSTAT = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PGMSTAT);
  *pucStatus = HIBYTE (m_usRegPGMSTAT);
  //DPS(("[%02x]", (USHORT)*pucStatus ));
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::AES16WriteFirmwarePacket (PBYTE pucFirmwarePacket,
					ULONG ulLength)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG i;
  BYTE ucLoByte, ucHiByte, ucCRC = 0;
  PULONG pulAddress = (PULONG) & m_pAuroraBlk->PMIXCTL[0];

  //cmn_err((CE_WARN,"CHalAurora::AES16WriteFirmwarePacket\n");

  if (ulLength != 128)
    return (HSTATUS_INVALID_PARAMETER);

  // encode the data into the lower and upper BYTEs of the LOWORD and compute the CRC
  for (i = 0; i < ulLength;)
    {
      ucLoByte = pucFirmwarePacket[i++];
      ucCRC += ucLoByte;

      ucHiByte = pucFirmwarePacket[i++];
      ucCRC += ucHiByte;

      WRITE_REGISTER_ULONG (pulAddress,
			    (ULONG) MAKEWORD (ucLoByte, ucHiByte));

      pulAddress++;
    }

  AES16WriteCommand (REG_ACTL_COMMAND_PROGRAM_CFG_BANK | (ucCRC << 8));

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16RequestVerify (void)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalAurora::AES16RequestVerify\n");
  return (AES16WriteCommand (REG_ACTL_COMMAND_VERIFY_CFG_BANK));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::AES16ReadVerifyPacket (PBYTE pucFirmwarePacket, ULONG ulLength)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG i, ulValue;
  PULONG pulAddress = (PULONG) & m_pAuroraBlk->PMIXSTAT[0];

  //cmn_err((CE_WARN,"CHalAurora::AES16ReadVerifyPacket\n");

  if (ulLength != 128)
    return (HSTATUS_INVALID_PARAMETER);

  // read the data from the AES16
  for (i = 0; i < ulLength;)
    {
      ulValue = READ_REGISTER_ULONG (pulAddress);

      pucFirmwarePacket[i++] = LOBYTE (ulValue);
      pucFirmwarePacket[i++] = HIBYTE (ulValue);

      //cmn_err((CE_WARN,"%02x %02x ", LOBYTE( ulValue ), HIBYTE( ulValue ) );

      pulAddress++;
    }

  //cmn_err((CE_WARN,"\n");

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16SetProgrammingComplete (void)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN, "CHalAurora::AES16SetProgrammingComplete\n");

  // Make the Alternate Bank the Active Bank
  if (m_bFWBank == FW_BANK_1)
    {
      //cmn_err (CE_WARN, "REG_ACTL_COMMAND_SET_CFG_BANK2_ACTIVE\n");
      AES16WriteCommand (REG_ACTL_COMMAND_SET_CFG_BANK2_ACTIVE);
    }
  else
    {
      //cmn_err (CE_WARN, "REG_ACTL_COMMAND_SET_CFG_BANK1_ACTIVE\n");
      AES16WriteCommand (REG_ACTL_COMMAND_SET_CFG_BANK1_ACTIVE);
    }

  // invalidate the static status
  m_bStaticStatusUpToDate = FALSE;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16AbortFirmwareUpdate (void)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN, "CHalAurora::AES16AbortFirmwareUpdate\n");
  return (AES16WriteCommand (REG_ACTL_COMMAND_RESUME));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16WriteSerialNumber (void)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN, "CHalAurora::AES16WriteSerialNumber\n");
  return (AES16WriteCommand (REG_ACTL_COMMAND_PROGRAM_SERIALNUMBER));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::GetSharedControls (USHORT usControl,
				 PAURORASHAREDCONTROLS pShared)
/////////////////////////////////////////////////////////////////////////////
{
  return (AES16GetSharedControls (usControl, pShared));
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16ReadLinkPID (BOOLEAN bMixerNotify)
// private
// DAH Added this method so we have a standardized way of reading the PID
// register just in case the LOCK on the AES16 goes away or the Aurora
// connected to the AES16 changes model.
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usValue;

  usValue = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->LINKPID);

  // is the AES16 Digital In 1 Locked?
  if (m_pHal4114 && !m_pHal4114->IsInputLocked (0))
    {
      // No - clear the PID portion of the register
      CLR (usValue, 0xFF00);
    }

  // is the AES16e Digital In 1 Locked?
  if (m_pAES16eDIO && !m_pAES16eDIO->IsInputLocked (0))
    {
      // No - clear the PID portion of the register
      CLR (usValue, 0xFF00);
    }

  if (m_usRegLINKPID != usValue)
    {
      m_usRegLINKPID = usValue;

      // decode LINKPID and put it into the control shadow registers
      if (m_usProductID != HIBYTE (m_usRegLINKPID))
	{
	  m_usProductID = HIBYTE (m_usRegLINKPID);
	  if (bMixerNotify)
	    {
	      m_pHalMixer->ControlChanged (LINE_AURORA, LINE_NO_SOURCE,
					   CONTROL_DEVICEID);
	    }
	}
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16GetLINKPID (PUSHORT pusValue)
/////////////////////////////////////////////////////////////////////////////
{
  if (m_ulPort != AURORA_PORT_AES16)
    return (HSTATUS_INVALID_MODE);

  AES16ReadLinkPID (FALSE);
  *pusValue = m_usRegLINKPID;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16GetPGMSTAT (PUSHORT pusValue)
/////////////////////////////////////////////////////////////////////////////
{
  if (m_ulPort != AURORA_PORT_AES16)
    return (HSTATUS_INVALID_MODE);

  m_usRegPGMSTAT = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PGMSTAT);
  *pusValue = m_usRegPGMSTAT;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16GetREV (PUSHORT pusValue)
/////////////////////////////////////////////////////////////////////////////
{
  if (m_ulPort != AURORA_PORT_AES16)
    return (HSTATUS_INVALID_MODE);

  m_usRegREV = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->REV);
  *pusValue = m_usRegREV;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::AES16GetSharedControls (USHORT usControl,
				      PAURORASHAREDCONTROLS pShared)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulPair;
  int nTrim, nChannel;
  BYTE ucValue;
  USHORT usValue;

  RtlZeroMemory (pShared, sizeof (AURORASHAREDCONTROLS));

  pShared->ulControlRequest = REQ_FAST_CONTROLS;

  // Meters
  for (nChannel = 0, ulPair = 0; ulPair < 8; ulPair++)
    {
      usValue = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->AINLVL.Levels[ulPair]);	// Analog In
      pShared->Fast.aucAnalogInMeters[nChannel] = LOBYTE (usValue) >> 4;
      pShared->Fast.aucAnalogInMeters[nChannel + 1] = HIBYTE (usValue) >> 4;
      pShared->Slow.aucAnalogInOverload[nChannel] = LOBYTE (usValue) & 0x0F;
      pShared->Slow.aucAnalogInOverload[nChannel + 1] =
	HIBYTE (usValue) & 0x0F;

      usValue = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->AOUTLVL.Levels[ulPair]);	// Analog Out
      pShared->Fast.aucAnalogOutMeters[nChannel] = LOBYTE (usValue) >> 4;
      pShared->Fast.aucAnalogOutMeters[nChannel + 1] = HIBYTE (usValue) >> 4;
      pShared->Slow.aucAnalogOutOverload[nChannel] = LOBYTE (usValue) & 0x0F;
      pShared->Slow.aucAnalogOutOverload[nChannel + 1] =
	HIBYTE (usValue) & 0x0F;

      usValue = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->DINLVL.Levels[ulPair]);	// Digital In
      pShared->Fast.aucDigitalInMeters[nChannel] = LOBYTE (usValue) >> 4;
      pShared->Fast.aucDigitalInMeters[nChannel + 1] = HIBYTE (usValue) >> 4;
      pShared->Slow.aucDigitalInOverload[nChannel] = LOBYTE (usValue) & 0x0F;
      pShared->Slow.aucDigitalInOverload[nChannel + 1] =
	HIBYTE (usValue) & 0x0F;

      usValue = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->DOUTLVL.Levels[ulPair]);	// Digital Out
      pShared->Fast.aucDigitalOutMeters[nChannel] = LOBYTE (usValue) >> 4;
      pShared->Fast.aucDigitalOutMeters[nChannel + 1] = HIBYTE (usValue) >> 4;
      pShared->Slow.aucDigitalOutOverload[nChannel] = LOBYTE (usValue) & 0x0F;
      pShared->Slow.aucDigitalOutOverload[nChannel + 1] =
	HIBYTE (usValue) & 0x0F;

      nChannel += 2;
    }

  if (usControl == CONTROL_SLOW_SHARED_CONTROLS)
    {
      SET (pShared->ulControlRequest, REQ_SLOW_CONTROLS);

      // Overloads (Not currently supported!)
      /*
         for( nChannel=0; nChannel<16; nChannel++ )
         {
         pShared->Slow.aucAnalogInOverload[ nChannel ]        = (BYTE)m_ulOverloadCount[ nChannel ];
         pShared->Slow.aucAnalogOutOverload[ nChannel ]       = (BYTE)m_ulOverloadCount[ nChannel + 16 ];
         pShared->Slow.aucDigitalInOverload[ nChannel ]       = (BYTE)m_ulOverloadCount[ nChannel + 32 ];
         pShared->Slow.aucDigitalOutOverload[ nChannel ]      = (BYTE)m_ulOverloadCount[ nChannel + 48 ];
         }
       */

      /////////////////////////////////////////////////////////////////////////
      // LINKPID
      /////////////////////////////////////////////////////////////////////////
      AES16ReadLinkPID ();
      pShared->Slow.usProductID = m_usProductID;

      /////////////////////////////////////////////////////////////////////////
      // TRIMCLK
      /////////////////////////////////////////////////////////////////////////
      m_usRegTRIMCLK =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->TRIMCLKSTAT);

      // Trim Portion
      for (nTrim = 0; nTrim < AURORA_NUM_TRIM; nTrim++)
	{
	  pShared->Slow.ulTrim[nTrim] = m_ulTrim[nTrim] =
	    (m_usRegTRIMCLK & (1 << nTrim)) ? MIXVAL_TRIM_PLUS4 :
	    MIXVAL_TRIM_MINUS10;
	}

      // Clock Portion
      ucValue = HIBYTE (m_usRegTRIMCLK);

      m_ulClockSource = (ULONG) (ucValue & REG_ACTL_CLOCK_CLKSRC_MASK);
      switch (m_ulClockSource)
	{
	case REG_TRIMCLK_CLKSRC_INTERNAL:
	  pShared->Slow.ulClockSource = MIXVAL_AURORA_CLKSRC_INTERNAL;
	  break;
	case REG_TRIMCLK_CLKSRC_EXTERNAL:
	  pShared->Slow.ulClockSource = MIXVAL_AURORA_CLKSRC_EXTERNAL;
	  break;
	case REG_TRIMCLK_CLKSRC_EXTERNAL_DIV2:
	  pShared->Slow.ulClockSource = MIXVAL_AURORA_CLKSRC_EXTERNAL_DIV2;
	  break;
	case REG_TRIMCLK_CLKSRC_EXTERNAL_DIV4:
	  pShared->Slow.ulClockSource = MIXVAL_AURORA_CLKSRC_EXTERNAL_DIV4;
	  break;
	case REG_TRIMCLK_CLKSRC_AESA:
	  pShared->Slow.ulClockSource = MIXVAL_AURORA_CLKSRC_AESA;
	  break;
	case REG_TRIMCLK_CLKSRC_AESB:
	  pShared->Slow.ulClockSource = MIXVAL_AURORA_CLKSRC_AESB;
	  break;
	case REG_TRIMCLK_CLKSRC_LSLOT:
	  pShared->Slow.ulClockSource = MIXVAL_AURORA_CLKSRC_LSLOT;
	  break;
	}
      pShared->Slow.bSynchroLockEnable = m_bSynchroLockEnable =
	(ucValue & REG_ACTL_CLOCK_SYNCHROLOCK) ? TRUE : FALSE;

      m_ulClockRate =
	(ucValue & REG_ACTL_CLOCK_SR_MASK) >> REG_ACTL_CLOCK_SR_OFFSET;
      switch (m_ulClockRate)
	{
	default:
	case MIXVAL_AURORA_SR44100:
	  pShared->Slow.ulClockRate = 44100;
	  break;
	case MIXVAL_AURORA_SR48000:
	  pShared->Slow.ulClockRate = 48000;
	  break;
	case MIXVAL_AURORA_SR88200:
	  pShared->Slow.ulClockRate = 88200;
	  break;
	case MIXVAL_AURORA_SR96000:
	  pShared->Slow.ulClockRate = 96000;
	  break;
	case MIXVAL_AURORA_SR176400:
	  pShared->Slow.ulClockRate = 176400;
	  break;
	case MIXVAL_AURORA_SR192000:
	  pShared->Slow.ulClockRate = 192000;
	  break;
	}

      /////////////////////////////////////////////////////////////////////////
      // MISC
      /////////////////////////////////////////////////////////////////////////
      m_usRegMISC = (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->MISCSTAT);
      // decode MISC and put it into the control shadow registers
      usValue = (m_usRegMISC & REG_MISC_AOUTSRC_MASK);
      switch (usValue)
	{
	case REG_MISC_AOUTSRC_AIN:
	  m_ulAnalogOutSource = MIXVAL_AURORA_OUTSOURCE_ANALOGIN;
	  break;
	case REG_MISC_AOUTSRC_DIN:
	  m_ulAnalogOutSource = MIXVAL_AURORA_OUTSOURCE_AESIN;
	  break;
	case REG_MISC_AOUTSRC_LSLOTIN:
	  m_ulAnalogOutSource = MIXVAL_AURORA_OUTSOURCE_LSLOTIN;
	  break;
	case REG_MISC_AOUTSRC_REMOTE:
	  m_ulAnalogOutSource = MIXVAL_AURORA_OUTSOURCE_REMOTE;
	  break;
	}
      pShared->Slow.ulAnalogOutSource = m_ulAnalogOutSource;

      usValue = (m_usRegMISC & REG_MISC_DOUTSRC_MASK);
      switch (usValue)
	{
	case REG_MISC_DOUTSRC_AIN:
	  m_ulDigitalOutSource = MIXVAL_AURORA_OUTSOURCE_ANALOGIN;
	  break;
	case REG_MISC_DOUTSRC_DIN:
	  m_ulDigitalOutSource = MIXVAL_AURORA_OUTSOURCE_AESIN;
	  break;
	case REG_MISC_DOUTSRC_LSLOTIN:
	  m_ulDigitalOutSource = MIXVAL_AURORA_OUTSOURCE_LSLOTIN;
	  break;
	case REG_MISC_DOUTSRC_REMOTE:
	  m_ulDigitalOutSource = MIXVAL_AURORA_OUTSOURCE_REMOTE;
	  break;
	}
      pShared->Slow.ulDigitalOutSource = m_ulDigitalOutSource;

      pShared->Slow.bDualWireIn = m_bDualWireIn =
	(m_usRegMISC & REG_MISC_DUALWIREIN) ? TRUE : FALSE;
      pShared->Slow.bDualWireOut = m_bDualWireOut =
	(m_usRegMISC & REG_MISC_DUALWIREOUT) ? TRUE : FALSE;
      pShared->Slow.ulFPMeterSource = m_ulFPMeterSource =
	(m_usRegMISC & REG_MISC_FPMETERSOURCE_MASK) >>
	REG_MISC_FPMETERSOURCE_OFFSET;
      pShared->Slow.ulLSlotSource = m_ulLSlotSource =
	(m_usRegMISC & REG_MISC_LSLOTSRC_MASK) ==
	REG_MISC_LSLOTSRC_ANALOG ? MIXVAL_AURORA_LSLOTOUTSOURCE_ANALOGOUT :
	MIXVAL_AURORA_LSLOTOUTSOURCE_DIGITALOUT;
      pShared->Slow.ulTrimOrigin = m_ulTrimOrigin =
	(m_usRegMISC & REG_MISC_TRIMORG_REMOTE) >> REG_MISC_TRIMORG_OFFSET;

      if ((m_usRegMISC & REG_MISC_LOCALTRIM_PLUS4) >>
	  REG_MISC_LOCALTRIM_OFFSET)
	m_ulLocalTrim = MIXVAL_TRIM_PLUS4;
      else
	m_ulLocalTrim = MIXVAL_TRIM_MINUS10;
      pShared->Slow.ulLocalTrim = m_ulLocalTrim;

      /////////////////////////////////////////////////////////////////////////
      // LOCKSTAT
      /////////////////////////////////////////////////////////////////////////
      m_usRegLOCKSTAT =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->LOCKSTAT);
      // decode LOCK and put it into the control shadow registers
      pShared->Slow.ulAESLockStatus = m_ulAESLockStatus =
	m_usRegLOCKSTAT & REG_LOCKSTAT_AESLOCK_MASK;
      pShared->Slow.ulSynchroLockStatus = m_ulSynchroLockStatus =
	(m_usRegLOCKSTAT & REG_LOCKSTAT_SLOCK_MASK) >>
	REG_LOCKSTAT_SLOCK_OFFSET;

      /////////////////////////////////////////////////////////////////////////
      // PMIX
      /////////////////////////////////////////////////////////////////////////
      for (nChannel = 0; nChannel < AURORA_NUMBER_OF_OUTPUTS; nChannel++)
	{
	  // PMIXA
	  usValue =
	    (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PMIXSTAT[nChannel].
					  PMixA);

	  // VOL
	  if (LOBYTE (usValue) == AURORA_VOLUME_MUTE)
	    {
	      pShared->Slow.abMonitorMute[nChannel][AURORA_PMIXA] = TRUE;
	      pShared->Slow.aucMonitorVolume[nChannel][AURORA_PMIXA] =
		m_ucMonitorVolume[nChannel][AURORA_PMIXA];
	    }
	  else
	    {
	      pShared->Slow.abMonitorMute[nChannel][AURORA_PMIXA] = FALSE;
	      pShared->Slow.aucMonitorVolume[nChannel][AURORA_PMIXA] =
		m_ucMonitorVolume[nChannel][AURORA_PMIXA] = LOBYTE (usValue);
	    }

	  // SOURCE
	  pShared->Slow.aucMonitorSource[nChannel][AURORA_PMIXA] =
	    m_ucMonitorSource[nChannel][AURORA_PMIXA] =
	    HIBYTE (usValue) & REG_ACTL_PMIX_SRC_MASK;

	  // DITHEN
	  //pShared->Slow.abDither[ nChannel ] = (usValue & kBit13) ? TRUE : FALSE;

	  // PMIXB
	  usValue =
	    (USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PMIXSTAT[nChannel].
					  PMixB);

	  // VOL
	  if (LOBYTE (usValue) == AURORA_VOLUME_MUTE)
	    {
	      pShared->Slow.abMonitorMute[nChannel][AURORA_PMIXB] = TRUE;
	      pShared->Slow.aucMonitorVolume[nChannel][AURORA_PMIXB] =
		m_ucMonitorVolume[nChannel][AURORA_PMIXB];
	    }
	  else
	    {
	      pShared->Slow.abMonitorMute[nChannel][AURORA_PMIXB] = FALSE;
	      pShared->Slow.aucMonitorVolume[nChannel][AURORA_PMIXB] =
		m_ucMonitorVolume[nChannel][AURORA_PMIXB] = LOBYTE (usValue);
	    }

	  // SOURCE
	  pShared->Slow.aucMonitorSource[nChannel][AURORA_PMIXB] =
	    m_ucMonitorSource[nChannel][AURORA_PMIXB] =
	    HIBYTE (usValue) & REG_ACTL_PMIX_SRC_MASK;
	}
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAurora::AES16GetPMixStatus (BOOLEAN bDriverLoad)
/////////////////////////////////////////////////////////////////////////////
{
  int nChannel;
  USHORT usValue;

  for (nChannel = 0; nChannel < AURORA_NUMBER_OF_OUTPUTS; nChannel++)
    {
      // PMIXA
      usValue =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PMIXSTAT[nChannel].
				      PMixA);

      // VOL
      if (LOBYTE (usValue) == AURORA_VOLUME_MUTE)
	{
	  m_bMonitorMute[nChannel][AURORA_PMIXA] = TRUE;
	  m_ucMonitorVolume[nChannel][AURORA_PMIXA] = AURORA_VOLUME_MAX;
	}
      else
	{
	  m_bMonitorMute[nChannel][AURORA_PMIXA] = FALSE;
	  m_ucMonitorVolume[nChannel][AURORA_PMIXA] = LOBYTE (usValue);
	}

      // SOURCE
      m_ucMonitorSource[nChannel][AURORA_PMIXA] =
	HIBYTE (usValue) & REG_ACTL_PMIX_SRC_MASK;

      // DITHEN
      //m_bDither[ nChannel ] = (usValue & kBit13) ? TRUE : FALSE;

      // PMIXB
      usValue =
	(USHORT) READ_REGISTER_ULONG (&m_pAuroraBlk->PMIXSTAT[nChannel].
				      PMixB);

      // VOL
      if (LOBYTE (usValue) == AURORA_VOLUME_MUTE)
	{
	  m_bMonitorMute[nChannel][AURORA_PMIXB] = TRUE;
	  m_ucMonitorVolume[nChannel][AURORA_PMIXB] = AURORA_VOLUME_MAX;
	}
      else
	{
	  m_bMonitorMute[nChannel][AURORA_PMIXB] = FALSE;
	  m_ucMonitorVolume[nChannel][AURORA_PMIXB] = LOBYTE (usValue);
	}

      // SOURCE
      m_ucMonitorSource[nChannel][AURORA_PMIXB] =
	HIBYTE (usValue) & REG_ACTL_PMIX_SRC_MASK;
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::SetMixerControl (USHORT usSrcLine, USHORT usControl,
			       ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  if (usSrcLine == LINE_NO_SOURCE)
    {
      switch (usControl)
	{
	case CONTROL_TRIM_AIN_1_4:
	  return (SetTrim (AURORA_TRIMIN0, ulValue));
	case CONTROL_TRIM_AIN_5_8:
	  return (SetTrim (AURORA_TRIMIN1, ulValue));
	case CONTROL_TRIM_AIN_9_12:
	  return (SetTrim (AURORA_TRIMIN2, ulValue));
	case CONTROL_TRIM_AIN_13_16:
	  return (SetTrim (AURORA_TRIMIN3, ulValue));
	case CONTROL_TRIM_AOUT_1_4:
	  return (SetTrim (AURORA_TRIMOUT0, ulValue));
	case CONTROL_TRIM_AOUT_5_8:
	  return (SetTrim (AURORA_TRIMOUT1, ulValue));
	case CONTROL_TRIM_AOUT_9_12:
	  return (SetTrim (AURORA_TRIMOUT2, ulValue));
	case CONTROL_TRIM_AOUT_13_16:
	  return (SetTrim (AURORA_TRIMOUT3, ulValue));

	case CONTROL_CLOCKSOURCE:
	  return (SetClockSource (ulValue));
	case CONTROL_SYNCHROLOCK_ENABLE:
	  return (SetSynchroLockEnable (ulValue));
	case CONTROL_CLOCKRATE_SELECT:
	  return (SetSampleRate (ulValue));

	case CONTROL_FP_ANALOGOUT_SOURCE:
	  return (SetAnalogOutSource (ulValue));
	case CONTROL_FP_DIGITALOUT_SOURCE:
	  return (SetDigitalOutSource (ulValue));
	case CONTROL_WIDEWIREIN:
	  return (SetDualWireIn (ulValue));
	case CONTROL_WIDEWIREOUT:
	  return (SetDualWireOut (ulValue));
	case CONTROL_FP_METER_SOURCE:
	  return (SetFPMeterSource (ulValue));
	case CONTROL_LSLOT_OUT_SOURCE:
	  return (SetLSlotOutSource (ulValue));
	case CONTROL_TRIM_ORIGIN:
	  return (SetTrimOrigin (ulValue));
	case CONTROL_FP_TRIM:
	  return (SetLocalTrim (ulValue));

	case CONTROL_LS1_LTMODE:
	  return (SetLSlotMode (ulValue));

	case CONTROL_SERIALNUMBER:
	  return (SetSerialNumber (ulValue));

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
    }
  else
    {
      // make sure only valid lines get through
      if (!
	  ((usSrcLine >= LINE_ANALOGIN_1)
	   && (usSrcLine <= LINE_DIGITALOUT_16)))
	return (HSTATUS_INVALID_MIXER_LINE);

      ULONG ulChannel = (usSrcLine - LINE_ANALOGIN_1);
      BOOLEAN bIsInput = (usSrcLine < LINE_ANALOGOUT_1);

      if (bIsInput)
	{
	  if (usControl == CONTROL_OVERLOAD)
	    return (ResetOverload (usSrcLine));
	  else
	    return (HSTATUS_INVALID_MIXER_CONTROL);
	}
      else
	{
	  ulChannel -= 32;	// get rid of input channel offset

	  switch (usControl)
	    {
	    case CONTROL_SOURCEA:
	      return (SetMonitorSource (ulChannel, AURORA_PMIXA, ulValue));
	    case CONTROL_VOLUMEA:
	      return (SetVolume (ulChannel, AURORA_PMIXA, ulValue));
	    case CONTROL_MUTEA:
	      return (SetMute (ulChannel, AURORA_PMIXA, ulValue));

	    case CONTROL_SOURCEB:
	      return (SetMonitorSource (ulChannel, AURORA_PMIXB, ulValue));
	    case CONTROL_VOLUMEB:
	      return (SetVolume (ulChannel, AURORA_PMIXB, ulValue));
	    case CONTROL_MUTEB:
	      return (SetMute (ulChannel, AURORA_PMIXB, ulValue));

	      // master for this channel
	      //case CONTROL_VOLUME:                  return( SetVolume( ulChannel, ulValue ) );
	      //case CONTROL_MUTE:                            return( SetMute( ulChannel, ulValue ) );
	      //case CONTROL_DITHER:                  return( SetDither( ulChannel, ulValue ) );

	    default:
	      return (HSTATUS_INVALID_MIXER_CONTROL);
	    }
	}
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAurora::GetMixerControl (USHORT usSrcLine, USHORT usControl,
			       PULONG pulValue, ULONG ulSize)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalAurora::GetMixerControl: %u %u\n", usSrcLine, usControl );

  if (usSrcLine == LINE_NO_SOURCE)
    {
      switch (usControl)
	{
	case CONTROL_NUMCHANNELS:
	  *pulValue = 1;
	  return (HSTATUS_OK);

	case CONTROL_FAST_SHARED_CONTROLS:
	case CONTROL_SLOW_SHARED_CONTROLS:
	  if (ulSize == sizeof (DWORD))
	    return (HSTATUS_OK);
	  if (ulSize != sizeof (AURORASHAREDCONTROLS))
	    {
	      cmn_err (CE_WARN,
		       "AURORASHAREDCONTROLS structure incorrect size!\n");
	      return (HSTATUS_INVALID_PARAMETER);
	    }
	  GetSharedControls (usControl, (PAURORASHAREDCONTROLS) pulValue);
	  break;

	case CONTROL_TRIM_AIN_1_4:
	  return (GetTrim (AURORA_TRIMIN0, pulValue));
	case CONTROL_TRIM_AIN_5_8:
	  return (GetTrim (AURORA_TRIMIN1, pulValue));
	case CONTROL_TRIM_AIN_9_12:
	  return (GetTrim (AURORA_TRIMIN2, pulValue));
	case CONTROL_TRIM_AIN_13_16:
	  return (GetTrim (AURORA_TRIMIN3, pulValue));
	case CONTROL_TRIM_AOUT_1_4:
	  return (GetTrim (AURORA_TRIMOUT0, pulValue));
	case CONTROL_TRIM_AOUT_5_8:
	  return (GetTrim (AURORA_TRIMOUT1, pulValue));
	case CONTROL_TRIM_AOUT_9_12:
	  return (GetTrim (AURORA_TRIMOUT2, pulValue));
	case CONTROL_TRIM_AOUT_13_16:
	  return (GetTrim (AURORA_TRIMOUT3, pulValue));

	case CONTROL_CLOCKSOURCE:
	  return (GetClockSource (pulValue));
	case CONTROL_SYNCHROLOCK_ENABLE:
	  return (GetSynchroLockEnable (pulValue));
	case CONTROL_CLOCKRATE_SELECT:
	  return (GetSampleRate (pulValue));

	case CONTROL_FP_ANALOGOUT_SOURCE:
	  return (GetAnalogOutSource (pulValue));
	case CONTROL_FP_DIGITALOUT_SOURCE:
	  return (GetDigitalOutSource (pulValue));
	case CONTROL_WIDEWIREIN:
	  return (GetDualWireIn (pulValue));
	case CONTROL_WIDEWIREOUT:
	  return (GetDualWireOut (pulValue));
	case CONTROL_FP_METER_SOURCE:
	  return (GetFPMeterSource (pulValue));
	case CONTROL_LSLOT_OUT_SOURCE:
	  return (GetLSlotOutSource (pulValue));
	case CONTROL_TRIM_ORIGIN:
	  return (GetTrimOrigin (pulValue));
	case CONTROL_FP_TRIM:
	  return (GetLocalTrim (pulValue));

	case CONTROL_DIGITALIN_STATUS:
	  return (GetDigitalInStatus (pulValue));
	case CONTROL_SYNCHROLOCK_STATUS:
	  return (GetSynchroLockStatus (pulValue));

	case CONTROL_DEVICEID:
	  return (GetProductID (pulValue));
	case CONTROL_PCBREV:
	  return (GetPCBRev (pulValue));
	case CONTROL_FIRMWAREREV:
	  return (GetCurrentFirmwareRev (pulValue));
	case CONTROL_SERIALNUMBER:
	  return (GetSerialNumber (pulValue));

	case CONTROL_LS1_DEVICEID:
	  return (GetLSlotProductID (pulValue));
	case CONTROL_LS1_PCBREV:
	  return (GetLSlotPCBRev (pulValue));
	case CONTROL_LS1_FIRMWAREREV:
	  return (GetLSlotFirmwareRev (pulValue));
	case CONTROL_LS1_LTMODE:
	  return (GetLSlotMode (pulValue));

	default:
	  return (HSTATUS_INVALID_MIXER_CONTROL);
	}
    }
  else
    {
      // make sure only valid lines get through
      if (!
	  ((usSrcLine >= LINE_ANALOGIN_1)
	   && (usSrcLine <= LINE_DIGITALOUT_16)))
	return (HSTATUS_INVALID_MIXER_LINE);

      if (usControl == CONTROL_NUMCHANNELS)
	{
	  *pulValue = 1;
	  return (HSTATUS_OK);
	}

      ULONG ulChannel = (usSrcLine - LINE_ANALOGIN_1);
      BOOLEAN bIsInput = (usSrcLine < LINE_ANALOGOUT_1);

      if (bIsInput)
	{
	  switch (usControl)
	    {
	    case CONTROL_OVERLOAD:
	      return (GetOverload (usSrcLine, pulValue));
	    case CONTROL_PEAKMETER:
	      return (GetLevel (usSrcLine, pulValue));
	    default:
	      return (HSTATUS_INVALID_MIXER_CONTROL);
	    }
	}
      else
	{
	  ulChannel -= AURORA_NUMBER_OF_OUTPUTS;	// get rid of input channel offset

	  switch (usControl)
	    {
	    case CONTROL_SOURCEA:
	      return (GetMonitorSource (ulChannel, AURORA_PMIXA, pulValue));
	    case CONTROL_VOLUMEA:
	      return (GetVolume (ulChannel, AURORA_PMIXA, pulValue));
	    case CONTROL_MUTEA:
	      return (GetMute (ulChannel, AURORA_PMIXA, pulValue));

	    case CONTROL_SOURCEB:
	      return (GetMonitorSource (ulChannel, AURORA_PMIXB, pulValue));
	    case CONTROL_VOLUMEB:
	      return (GetVolume (ulChannel, AURORA_PMIXB, pulValue));
	    case CONTROL_MUTEB:
	      return (GetMute (ulChannel, AURORA_PMIXB, pulValue));

	      // master for this channel
	    case CONTROL_OVERLOAD:
	      return (GetOverload (usSrcLine, pulValue));
	    case CONTROL_PEAKMETER:
	      return (GetLevel (usSrcLine, pulValue));
	      //case CONTROL_VOLUME:                  return( GetVolume( ulChannel, pulValue ) );
	      //case CONTROL_MUTE:                            return( GetMute( ulChannel, pulValue ) );
	      //case CONTROL_DITHER:                  return( GetDither( ulChannel, pulValue ) );

	    default:
	      return (HSTATUS_INVALID_MIXER_CONTROL);
	    }
	}
    }
  return (HSTATUS_OK);
}
