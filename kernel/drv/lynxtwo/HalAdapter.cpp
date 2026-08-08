/****************************************************************************
 HalAdapter.cpp

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
 Dec 21 04 DAH	Fixed problem with ADCREV when week code was less than 17
 May 16 03 DAH	Added m_bAutoRecalibrate, and broke out the HPF for the OEM B9 cards.
 May 15 03 DAH	Added call to EEPROMGetSerialNumber and special case code for
				specific serial numbers.
****************************************************************************/

#include <StdAfx.h>
#include "HalAdapter.h"

//#ifdef NT
//#define INTERRUPT_TIMING
//#endif

/////////////////////////////////////////////////////////////////////////////
CHalAdapter::CHalAdapter (PHALDRIVERINFO pHalDriverInfo,
			  ULONG ulAdapterNumber)
// Constructor - Cannot touch the hardware here...
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err (CE_WARN, "CHalAdapter::CHalAdapter %lu\n", ulAdapterNumber);

#ifdef OSX
  m_pHalAdapter = this;
  m_bSwap = FALSE;
#endif

  // clear the PCI Configuration Structure
  RtlZeroMemory (&m_PCIConfig, sizeof (PCI_CONFIGURATION));

  RtlZeroMemory (m_aIORegisters, NUM_IO_REGISTERS);

  RtlCopyMemory (&m_HalDriverInfo, pHalDriverInfo, sizeof (HALDRIVERINFO));

  //RtlZeroMemory( &m_aulFCValues, sizeof( m_aulFCValues ) );
  //RtlZeroMemory( &m_aucFCEntry, sizeof( m_aucFCEntry ) );

  // Initialize the member variables
  m_bOpen = FALSE;
  m_pRegisters = NULL;
  m_pAudioBuffers = NULL;
  m_ulAdapterNumber = ulAdapterNumber;
  m_PCIConfig.ulAdapterIndex = m_ulAdapterNumber;

  m_bIn32ChannelMode = FALSE;
  m_ulNumWaveDevices = 16;
  m_ulNumWaveRecordDevices = 8;
  m_ulNumWavePlayDevices = 8;

  m_pDMA_BufferList = NULL;
  m_pDMA_VAddr = NULL;
  m_pDMA_PAddr = 0;		//was NULL

  m_ulICHead = 0;
  m_ulICTail = 0;

  m_lIOEntryCount = 0;
  m_ulInterruptSamples = 0;	// will be set by the open command

  m_usDeviceID = 0;
  m_usPCBRev = 0;
  m_usFWREVID = 0;
  m_usFirmwareDate = 0;
  m_usMinSoftwareAPIRev = 0;
  m_ulSerialNumber = 0;

  m_usDitherType = MIXVAL_DITHER_TRIANGULAR_PDF;
  m_bAutoRecalibrate = TRUE;
  m_bAIn12HPFEnable = TRUE;
  m_bAIn34HPFEnable = TRUE;
  m_bAIn56HPFEnable = TRUE;
  m_bDACDeEmphasis = FALSE;

  m_bSyncStart = TRUE;
  m_ulSyncGroup = 0;
  m_ulSyncReady = 0;
  m_bLStreamSyncStart = FALSE;

  m_ulMTCSource = MIXVAL_MTCSOURCE_LTCRX;

  m_bIsLynxTWO = FALSE;
  m_bIsAES16 = FALSE;
  m_bIsAES16e = FALSE;

  m_bHasAK5394A = FALSE;
  m_bHasCS8420 = FALSE;
  m_bHasAK4114 = FALSE;
  m_bHasAES16eDIO = FALSE;
  m_bHasAurora = FALSE;
  m_bHasGlobalSyncStart = FALSE;
  m_bHasTIVideoPLL = FALSE;
  m_bHasP16 = FALSE;
  m_bHasDualMono = FALSE;
  m_bHasLStream11 = FALSE;
  m_bHasLRClock = FALSE;
  m_bHasLTC = FALSE;
  m_ulXtalSpeed = 32000000;
  m_bHasMultiChannel = FALSE;
  m_bHasSynchroLock = FALSE;
  m_bHasWideWireOut = FALSE;
  m_bHasNewPMix = FALSE;
  m_bHasPMixV3 = FALSE;
  m_bHasMRMEnable = FALSE;
  m_bHasSRAM = TRUE;
  m_bHasAES50 = FALSE;
}

/////////////////////////////////////////////////////////////////////////////
CHalAdapter::~CHalAdapter ()
// Destructor, also cannot touch hardware
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalAdapter::~CHalAdapter\n");
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::Find ()
// should be called before Open
/////////////////////////////////////////////////////////////////////////////
{
  m_PCIConfig.usVendorID = PCIVENDOR_LYNX;

  // call the driver to find the next Lynx device
  if (m_HalDriverInfo.pFind (m_HalDriverInfo.pContext, &m_PCIConfig))
    {
      //cmn_err((CE_WARN,"m_HalDriverInfo.pFind Failed! %04x %04x\n", m_PCIConfig.usVendorID, m_PCIConfig.usDeviceID );
      return (HSTATUS_CANNOT_FIND_ADAPTER);
    }

  // this will get overwritten in Open by the PDBLOCK
  m_usDeviceID = m_PCIConfig.usDeviceID;	// this lets the world know what device we are
  //cmn_err (CE_WARN, "CHalAdapter::Find %04x %04x\n", m_PCIConfig.usVendorID, m_PCIConfig.usDeviceID);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::Open (BOOLEAN bResume)
// Returns:
//      HSTATUS_CANNOT_FIND_ADAPTER     (Find Failed)
//      HSTATUS_CANNOT_MAP_ADAPTER      (Map Failed)
//      HSTATUS_INVALID_ADDRESS         (PCI Configuration Registers incorrect)
//      HSTATUS_INCORRECT_FIRMWARE      (Has Mac Firmware on PC)
//      HSTATUS_BAD_ADAPTER_RAM         (Memory Failure)
//      HSTATUS_OK                                      (Open Succeeded)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulDevice;
  USHORT usStatus;
  USHORT usFirmwareRev = 0;

  //cmn_err (CE_WARN, "CHalAdapter::Open %ld\n", m_ulAdapterNumber);

  // Call find if nobody else has yet...
  if (!m_PCIConfig.usVendorID)
    {
      usStatus = Find ();
      if (usStatus)
	return (usStatus);
    }

  if (!bResume)
    {
      // call the driver to map the adapter's BARs
      if (m_HalDriverInfo.pMap (m_HalDriverInfo.pContext, &m_PCIConfig))
	{
	  cmn_err (CE_WARN, "m_HalDriverInfo.pMap Failed!\n");
	  return (HSTATUS_CANNOT_MAP_ADAPTER);
	}
    }

  // verify we actually got something back, and it looks like our adapter
  if (!m_PCIConfig.Base[PCI_REGISTERS_INDEX].pvAddress)
    {
      cmn_err (CE_WARN, "BAR0 is NULL!\n");
      m_HalDriverInfo.pUnmap (m_HalDriverInfo.pContext, &m_PCIConfig);
      return (HSTATUS_INVALID_ADDRESS);
    }

  if (!((m_PCIConfig.Base[PCI_REGISTERS_INDEX].ulSize == LYNXTWO_BAR0_SIZE) ||
	(m_PCIConfig.Base[PCI_REGISTERS_INDEX].ulSize == AES16_BAR0_SIZE) ||
	(m_PCIConfig.Base[PCI_REGISTERS_INDEX].ulSize == AES16e_BAR0_SIZE)))
    {
      cmn_err (CE_WARN, "BAR0 size is Invalid!\n");
      m_HalDriverInfo.pUnmap (m_HalDriverInfo.pContext, &m_PCIConfig);
      return (HSTATUS_INVALID_ADDRESS);
    }

  m_pRegisters =
    (PLYNXTWOREGISTERS) m_PCIConfig.Base[PCI_REGISTERS_INDEX].pvAddress;
  /////////////////////////////////////////////////////////////////////////
  // Verify that the PDBLOCK has what we are looking for...
  /////////////////////////////////////////////////////////////////////////
  usStatus = (USHORT) READ_REGISTER_ULONG (&m_pRegisters->PDBlock.LY);
  //cmn_err (CE_WARN, "PDBLOCK-LY %08lx\n", READ_REGISTER_ULONG (&m_pRegisters->PDBlock.LY));
  if (usStatus != 0x4C59)	// 0x4C='L' 0x59='Y'
    {
#if OSX && TARGET_CPU_PPC
      m_bSwap = TRUE;
      cmn_err (CE_WARN, "Trying Swap!\n");
      usStatus = (USHORT) READ_REGISTER_ULONG (&m_pRegisters->PDBlock.LY);
      if (usStatus != 0x4C59)	// 0x4C='L' 0x59='Y'
	{
#endif

	  cmn_err (CE_WARN,
		   "BAR0 does not have valid PDBLOCK (Memory Window Invalid?)! %08lx\n",
		   READ_REGISTER_ULONG (&m_pRegisters->PDBlock.LY));
	  m_HalDriverInfo.pUnmap (m_HalDriverInfo.pContext, &m_PCIConfig);
	  return (HSTATUS_INCORRECT_FIRMWARE);
#if OSX && TARGET_CPU_PPC
	}
#endif
    }

  usStatus = (USHORT) READ_REGISTER_ULONG (&m_pRegisters->PDBlock.NX);
  //cmn_err (CE_WARN, "PDBLOCK-NX %08lx\n", READ_REGISTER_ULONG (&m_pRegisters->PDBlock.NX));

  if (usStatus != 0x4E58)	// 0x4E='N' 0x58='X'
    {
      cmn_err (CE_WARN,
	       "BAR0 does not have valid PDBLOCK (Memory Window Invalid?)! %08lx\n",
	       READ_REGISTER_ULONG (&m_pRegisters->PDBlock.NX));
      m_HalDriverInfo.pUnmap (m_HalDriverInfo.pContext, &m_PCIConfig);
      return (HSTATUS_INCORRECT_FIRMWARE);
    }

  m_usDeviceID =
    (USHORT) READ_REGISTER_ULONG (&m_pRegisters->PDBlock.DeviceID);

  // fix for old device id
  if (m_usDeviceID == 0x2a)
    m_usDeviceID = PCIDEVICE_LYNX_AES16e50;

  //cmn_err (CE_WARN, "CHalAdapter::Open DeviceID [%04x]\n", m_usDeviceID);

  if ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
      || (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
      || (m_usDeviceID == PCIDEVICE_LYNXTWO_C)
      || (m_usDeviceID == PCIDEVICE_LYNX_L22))
    m_bIsLynxTWO = TRUE;

  if ((m_usDeviceID == PCIDEVICE_LYNX_AES16)
      || (m_usDeviceID == PCIDEVICE_LYNX_AES16SRC))
    m_bIsAES16 = TRUE;

  if ((m_usDeviceID == PCIDEVICE_LYNX_AES16e)
      || (m_usDeviceID == PCIDEVICE_LYNX_AES16eSRC)
      || (m_usDeviceID == PCIDEVICE_LYNX_AES16e50))
    m_bIsAES16e = TRUE;

  if (m_bIsAES16e)
    {
      //m_usDeviceID = PCIDEVICE_LYNX_AES16e50;       // hack to test the AES50 card
      m_bHasSRAM = FALSE;
    }
  else
    {
      m_bHasSRAM = TRUE;
    }

  if (m_bHasSRAM)
    {
      if (!m_PCIConfig.Base[AUDIO_DATA_INDEX].pvAddress ||
	  m_PCIConfig.Base[AUDIO_DATA_INDEX].ulSize != BAR1_SIZE)
	{
	  cmn_err (CE_WARN, "BAR1 is Invalid!\n");
	  m_HalDriverInfo.pUnmap (m_HalDriverInfo.pContext, &m_PCIConfig);
	  return (HSTATUS_INVALID_ADDRESS);
	}

      // assign LynxTWO registers the correct starting address
      m_pAudioBuffers =
	(PLYNXTWOAUDIOBUFFERS) m_PCIConfig.Base[AUDIO_DATA_INDEX].pvAddress;
    }

  //cmn_err((CE_WARN,"LYNXTWOREGISTERS [%08lx] LYNXTWOAUDIOBUFFERS [%08lx]\n", (ULONG)m_pRegisters, (ULONG)m_pAudioBuffers );

#ifndef DOS
  //cmn_err((CE_WARN,"Test the SRAM\n");
  /////////////////////////////////////////////////////////////////////////
  // Test the SRAM
  // DAH 09/27/2007 Don't test the RAM on an AES16e model
  /////////////////////////////////////////////////////////////////////////
  if (m_bHasSRAM)
    {
      PULONG pAddr;
      ULONG i;
      ULONG ulData;
      ULONG ulTestData;

#define TEST_PATTERN	0x1234ABCD

      pAddr = (PULONG) m_pAudioBuffers;
      ulTestData = TEST_PATTERN;
      for (i = 0; i < MEMORY_TEST_SIZE; i++)
	{
	  WRITE_REGISTER_ULONG (pAddr + i, ulTestData);
	  ulTestData += TEST_PATTERN;
	}
      pAddr = (PULONG) m_pAudioBuffers;
      ulTestData = TEST_PATTERN;
      for (i = 0; i < MEMORY_TEST_SIZE; i++)
	{
	  ulData = READ_REGISTER_ULONG (pAddr + i);
	  if (ulData != ulTestData)
	    {
	      cmn_err (CE_WARN,
		       "Bad Adapter Ram at %08lx [%08lx] Read [%08lx] XOR [%08lx]\n",
		       i, ulTestData, ulData, ulData ^ ulTestData);
	      m_HalDriverInfo.pUnmap (m_HalDriverInfo.pContext, &m_PCIConfig);
	      return (HSTATUS_BAD_ADAPTER_RAM);
	    }
	  ulTestData += TEST_PATTERN;
	}

      /////////////////////////////////////////////////////////////////////////
      // Clear the SRAM
      /////////////////////////////////////////////////////////////////////////
      pAddr = (PULONG) m_pAudioBuffers;
      for (i = 0; i < MEMORY_TEST_SIZE; i++)
	WRITE_REGISTER_ULONG (pAddr + i, 0);
    }
  //cmn_err((CE_WARN,"SRAM Test Complete\n");
#endif // DOS

  /////////////////////////////////////////////////////////////////////////
  // Setup internal HAL variables
  // From here on, the Open must succeed and cannot return an error
  /////////////////////////////////////////////////////////////////////////

  // this is a bit tricky - we must allocate memory for the DMA buffer list and it must be on a 4096 byte boundry
  m_HalDriverInfo.pAllocateMemory (m_osdev, (PVOID *) & m_pDMA_BufferList,
				   (PVOID *) & m_pDMA_VAddr,
				   (PVOID *) & m_pDMA_PAddr,
				   sizeof (DMABUFFERLIST), 0xFFFFF000);

  // now inform the hardware where the DMA Double Buffer is Located
  // only shift out 10 bits because GDBLADDR starts at bit position 1
  if (m_bIsAES16e)
    WRITE_REGISTER_ULONG (&m_pRegisters->DMACTL,
			  ((m_pDMA_PAddr >> 11) | REG_DMACTL_GDMAEN));
  else
    WRITE_REGISTER_ULONG (&m_pRegisters->DMACTL,
			  ((m_pDMA_PAddr >> 10) | REG_DMACTL_GDMAEN));

  m_RegPCICTL.Init (this, &m_pRegisters->PCICTL, REG_WRITEONLY);	// value defaults to 0, not written to hardware at this point
  m_RegSTRMCTL.Init (this, &m_pRegisters->STRMCTL, REG_WRITEONLY);

  // Fill in the PDBLOCK shadow registers
  m_usPCBRev = (USHORT) READ_REGISTER_ULONG (&m_pRegisters->PDBlock.PCBRev);
  m_usFWREVID = (USHORT) READ_REGISTER_ULONG (&m_pRegisters->PDBlock.FWRevID);	// DAH Changed on 10/22/2009
  usFirmwareRev = PDBLOCK_GET_FWREV (m_usFWREVID);
  //m_usFirmwareID                        = PDBLOCK_GET_FWID( m_usFWREVID );
  //m_usFirmwareBE                        = PDBLOCK_GET_FWBEFLAG( m_usFWREVID );
  m_usFirmwareDate =
    (USHORT) READ_REGISTER_ULONG (&m_pRegisters->PDBlock.FWDate);
  m_usMinSoftwareAPIRev =
    (USHORT) READ_REGISTER_ULONG (&m_pRegisters->PDBlock.MinSWAPIRev);

  if (m_bIsAES16e)
    {
      m_ulSerialNumber =
	READ_REGISTER_ULONG (&m_pRegisters->PDBlock.AES16eSN);

      // validate the serial number
      if (L2SN_GET_MODEL (m_ulSerialNumber) != m_usDeviceID)
	{
	  m_ulSerialNumber = 0xFFFFFFFF;
	  cmn_err (CE_WARN,
		   "CHalAdapter::Open: Serial Number Device ID does not match hardware Device ID!\n");
	}
    }
  else
    {
      // PCICTL & MISTAT are used in this function, PCICTL is set to 0 upon exit
      if (EEPROMGetSerialNumber (this, &m_ulSerialNumber))
	{
	  m_ulSerialNumber = 0xFFFFFFFF;
	  cmn_err (CE_WARN,
		   "CHalAdapter::Open: Unable to read Serial Number!\n");
	}
      else
	{
	  // validate the serial number
	  if (L2SN_GET_MODEL (m_ulSerialNumber) != m_usDeviceID)
	    {
	      m_ulSerialNumber = 0xFFFFFFFF;
	      cmn_err (CE_WARN,
		       "CHalAdapter::Open: Serial Number Device ID does not match hardware Device ID!\n");
	    }
	}
    }

  /////////////////////////////////////////////////////////////////////////
  // do any serial number specific work
  /////////////////////////////////////////////////////////////////////////
  if (m_ulSerialNumber != 0xFFFFFFFF)
    {
      //cmn_err((CE_WARN,"Serial Number: %02lx%02ld%02ld%04ld\n", L2SN_GET_MODEL(m_ulSerialNumber), L2SN_GET_YEAR(m_ulSerialNumber), L2SN_GET_WEEK(m_ulSerialNumber), L2SN_GET_UNITID(m_ulSerialNumber) );
      // Custom OEM LynxTWO-A Board Requirements
      if ((L2SN_GET_MODEL (m_ulSerialNumber) == PCIDEVICE_LYNXTWO_A) &&
	  (((L2SN_GET_YEAR (m_ulSerialNumber) == 3)
	    && (L2SN_GET_WEEK (m_ulSerialNumber) == 18))
	   || ((L2SN_GET_YEAR (m_ulSerialNumber) == 4)
	       && (L2SN_GET_WEEK (m_ulSerialNumber) == 1))))
	{
	  m_bAutoRecalibrate = FALSE;
	  m_bAIn12HPFEnable = TRUE;
	  m_bAIn34HPFEnable = FALSE;
	}

      if (((L2SN_GET_MODEL (m_ulSerialNumber) == PCIDEVICE_LYNXTWO_A) ||
	   (L2SN_GET_MODEL (m_ulSerialNumber) == PCIDEVICE_LYNXTWO_B) ||
	   (L2SN_GET_MODEL (m_ulSerialNumber) == PCIDEVICE_LYNXTWO_C) ||
	   (L2SN_GET_MODEL (m_ulSerialNumber) == PCIDEVICE_LYNX_L22))
	  &&
	  (((L2SN_GET_YEAR (m_ulSerialNumber) == 3)
	    && (L2SN_GET_WEEK (m_ulSerialNumber) >= 17))
	   || (L2SN_GET_YEAR (m_ulSerialNumber) >= 4)))
	{
	  m_bHasAK5394A = TRUE;
	  //cmn_err((CE_WARN,"Setting ADCREV to TRUE\n");
	  m_RegPCICTL.BitSet (REG_PCICTL_ADCREV, TRUE);	// all other bits are already 0
	}
    }

  /////////////////////////////////////////////////////////////////////////
  // Firmware Version Specific Conditionals
  /////////////////////////////////////////////////////////////////////////
  if (((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (usFirmwareRev >= 16)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_B) && (usFirmwareRev >= 2)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_C) && (usFirmwareRev >= 2)) ||
      (m_usDeviceID == PCIDEVICE_LYNX_L22) || m_bIsAES16 || m_bIsAES16e)
    m_bHasGlobalSyncStart = TRUE;

  if (((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (m_usPCBRev > 1)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_B) && (m_usPCBRev > 0)) ||
      (m_usDeviceID == PCIDEVICE_LYNXTWO_C)
      || (m_usDeviceID == PCIDEVICE_LYNX_L22) || m_bIsAES16 || m_bIsAES16e)
    m_bHasTIVideoPLL = TRUE;

  if (((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (usFirmwareRev >= 17)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_B) && (usFirmwareRev >= 2)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_C) && (usFirmwareRev >= 3)) ||
      (m_usDeviceID == PCIDEVICE_LYNX_L22) || m_bIsAES16 || m_bIsAES16e)
    m_bHasP16 = TRUE;

  if (((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (usFirmwareRev >= 19)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_B) && (usFirmwareRev >= 3)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_C) && (usFirmwareRev >= 5)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_L22) && (usFirmwareRev >= 2)) ||
      m_bIsAES16 || m_bIsAES16e)
    m_bHasDualMono = TRUE;

  if (((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (usFirmwareRev >= 20)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_B) && (usFirmwareRev >= 4)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_C) && (usFirmwareRev >= 6)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_L22) && (usFirmwareRev >= 4)) ||
      m_bIsAES16 || m_bIsAES16e)
    m_bHasLStream11 = TRUE;

  if (((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (usFirmwareRev >= 23)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_B) && (usFirmwareRev >= 7)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_C) && (usFirmwareRev >= 9)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_L22) && (usFirmwareRev >= 7)) ||
      m_bIsAES16 || m_bIsAES16e)
    m_bHasLRClock = TRUE;

  if ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
      || (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
      || (m_usDeviceID == PCIDEVICE_LYNXTWO_C))
    m_bHasLTC = TRUE;

  if ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
      || (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
      || (m_usDeviceID == PCIDEVICE_LYNXTWO_C)
      || (m_usDeviceID == PCIDEVICE_LYNX_L22))
    m_bHasCS8420 = TRUE;

  if ((m_usDeviceID == PCIDEVICE_LYNX_AES16)
      || (m_usDeviceID == PCIDEVICE_LYNX_AES16SRC))
    m_bHasAK4114 = TRUE;

  if (m_bIsAES16e)
    m_bHasAES16eDIO = TRUE;

  if (((m_usDeviceID == PCIDEVICE_LYNX_AES16) && (usFirmwareRev >= 19)
       && (m_usPCBRev > 1)) || ((m_usDeviceID == PCIDEVICE_LYNX_AES16SRC)
				&& (usFirmwareRev >= 19) && (m_usPCBRev > 1))
      || m_bIsAES16e)
    m_bHasAurora = TRUE;

  if (m_bIsAES16)
    m_ulXtalSpeed = 40000000;
  // SN: 2503230103 & 2503230106 both have 32MHz Crystals

  if (m_bIsAES16e)
    m_ulXtalSpeed = 50000000;

  if (((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (usFirmwareRev >= 28)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_B) && (usFirmwareRev >= 10)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_C) && (usFirmwareRev >= 12)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_L22) && (usFirmwareRev >= 9)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_AES16) && (usFirmwareRev >= 24)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_AES16SRC) && (usFirmwareRev >= 24)) ||
      m_bIsAES16e)
    m_bHasMultiChannel = TRUE;

  if (m_bIsAES16 || m_bIsAES16e)
    m_bHasSynchroLock = TRUE;

  if (((m_usDeviceID == PCIDEVICE_LYNX_AES16) && (usFirmwareRev >= 16)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_AES16SRC) && (usFirmwareRev >= 16)) ||
      m_bIsAES16e)
    m_bHasWideWireOut = TRUE;

  if (((m_usDeviceID == PCIDEVICE_LYNX_AES16) && (usFirmwareRev >= 24)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_AES16SRC) && (usFirmwareRev >= 24)) ||
      m_bIsAES16e)
    m_bHasNewPMix = TRUE;

  if (m_bIsAES16e && (usFirmwareRev >= 8))
    {
      m_bHasNewPMix = FALSE;	// NOTE: We must turn OFF NewPMix which was just set ON above.
      m_bHasPMixV3 = TRUE;	// This enables the 64x32 mixer engine support in HalMixer.cpp

      if ((m_usDeviceID == PCIDEVICE_LYNX_AES16e50)
	  || (m_PCIConfig.usDeviceCount == 32))
	{
	  m_bIn32ChannelMode = TRUE;	// Controls the Interrupt Service Routine number of wave devices
	  m_ulNumWaveDevices = 32;
	  m_ulNumWaveRecordDevices = 16;
	  m_ulNumWavePlayDevices = 16;
	}
      else
	{
	  m_bIn32ChannelMode = FALSE;	// Controls the Interrupt Service Routine number of wave devices
	  m_ulNumWaveDevices = 16;
	  m_ulNumWaveRecordDevices = 8;
	  m_ulNumWavePlayDevices = 8;
	}
    }

  if (((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (usFirmwareRev >= 33)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_B) && (usFirmwareRev >= 14)) ||
      ((m_usDeviceID == PCIDEVICE_LYNXTWO_C) && (usFirmwareRev >= 15)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_L22) && (usFirmwareRev >= 13)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_AES16) && (usFirmwareRev >= 24)) ||
      ((m_usDeviceID == PCIDEVICE_LYNX_AES16SRC) && (usFirmwareRev >= 24)))
    m_bHasMRMEnable = TRUE;

  if (m_usDeviceID == PCIDEVICE_LYNX_AES16e50)
    m_bHasAES50 = TRUE;

  /////////////////////////////////////////////////////////////////////////
  // Open the devices for this adapter
  /////////////////////////////////////////////////////////////////////////
  //cmn_err((CE_WARN,"CHalAdapter::Open Devices\n");
  for (ulDevice = 0; ulDevice < m_ulNumWaveDevices; ulDevice++)
    m_WaveDevice[ulDevice].Open (this, ulDevice);	// This opens the DMA object as well (No writes to hardware)

  for (ulDevice = 0; ulDevice < NUM_MIDI_DEVICES; ulDevice++)
    m_MIDIDevice[ulDevice].Open (this, ulDevice);	// No writes to hardware

  // if this is a LynxTWO
  if (m_bIsLynxTWO)
    {
      // Setup the Misc Control Reg
      // DILRCKDIR = 1, DILRCK and DISLCK are inputs
      // DIRMCKDIR = 1, DIRMCK is input
      // Enable DACS and 8420
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_A) && (m_usPCBRev == 0))
	IOWrite (kMisc, (IO_MISC_DIRMCKDIR | IO_MISC_DACARSTn | IO_MISC_DACBRSTn | IO_MISC_DACCRSTn | IO_MISC_CS8420RSTn | IO_MISC_VIDEN));	// L2 master clock source
      else
	IOWrite (kMisc, (IO_MISC_DIRMCKDIR | IO_MISC_DACARSTn | IO_MISC_DACBRSTn | IO_MISC_DACCRSTn | IO_MISC_CS8420RSTn));	// L2 master clock source

      if (!m_bHasLStream11)
	{
	  // Configure everything as an input
	  IOWrite (kOptionIOControl,
		   (IO_OPT_OPHD1DIR | IO_OPT_OPHD2DIR | IO_OPT_OPHSIGDIR |
		    IO_OPT_OPBFCKDIR | IO_OPT_OPHFCKDIR));
	}
    }

  if (m_bHasAurora)
    {
      m_Aurora.Open (this);	// Reads from hardware
    }

  // set the mixer
  m_Mixer.Open (this);		// Writes to hardware

  // AES16e must be done before SampleClock open
  if (m_bHasAES16eDIO)
    {
      m_AES16eDIO.Open (this);
    }

  // Set sample rate & clock source to the default values
  m_SampleClock.Open (this);	// Writes to hardware

  // setup the 8420, this must be done *AFTER* the sample clock open
  if (m_bHasCS8420)
    {
      m_CS8420.Open (this);	// Writes to hardware
    }

  // setup the 4114, this must be done *AFTER* the sample clock open
  if (m_bHasAK4114)
    {
      m_AK4114.Open (this);	// Writes to hardware
    }

  // Sample Clock must be setup before LStream
  m_LStream.Open (this);	// Writes to hardware

  m_RegLTCControl.Init (this, &m_pRegisters->TCBlock.LTCControl);

  if (m_bHasLTC)
    {
      m_TimecodeRX.Open (this, TC_RX);	// Writes to hardware
      m_TimecodeTX.Open (this, TC_TX);	// Writes to hardware
    }

  if (m_bHasMRMEnable)
    {
      SetMRMEnable (TRUE);
    }

  // set the global stream control register (Global Circular Buffer Limit Size) to half the buffer
  SetInterruptSamples ();	// RegSTRMCTL must be init'ed before calling this! (SampleClock must be open as well!)

  /////////////////////////////////////////////////////////////////////////
  // Initialize the hardware to a known state
  /////////////////////////////////////////////////////////////////////////

  // if this is a LynxTWO
  if (m_bIsLynxTWO)
    {
      // Setup the ADCs (pin-controlled)  Doesn't matter if the ADC doesn't exist
      IOWrite (kADC_A_Control, IO_ADC_CNTL_RSTn);	// DAH Jun 14 2002 Recommended by AKM that ZCAL is Zero
      IOWrite (kADC_B_Control, IO_ADC_CNTL_RSTn);
      IOWrite (kADC_C_Control, IO_ADC_CNTL_RSTn);
      SetADHPF (CONTROL_AIN12_HPF, m_bAIn12HPFEnable);	// DAH May 21 2003
      SetADHPF (CONTROL_AIN34_HPF, m_bAIn34HPFEnable);
      SetADHPF (CONTROL_AIN56_HPF, m_bAIn56HPFEnable);

      // Setup the DACs (serial-controlled)
      IOWrite (kDAC_A_Control,
	       (IO_DAC_CNTL_MODE_1X_EMPOFF | IO_DAC_CNTL_MUTEn));
      // A & B Only
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
	  || (m_usDeviceID == PCIDEVICE_LYNXTWO_B))
	IOWrite (kDAC_B_Control,
		 (IO_DAC_CNTL_MODE_1X_EMPOFF | IO_DAC_CNTL_MUTEn));
      // B Only
      if (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
	IOWrite (kDAC_C_Control,
		 (IO_DAC_CNTL_MODE_1X_EMPOFF | IO_DAC_CNTL_MUTEn));
    }

  // set the state to open before enabling the interrupts so if an interrupt 
  // is pending now, it can get serviced now
  m_bOpen = TRUE;

  /////////////////////////////////////////////////////////////////////////
  // Turn on the LED
  /////////////////////////////////////////////////////////////////////////
  m_RegPCICTL.BitSet (REG_PCICTL_LED, TRUE);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::Close (BOOLEAN bSuspend)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulDevice;
  // put the adapter in power down mode

  if (!m_bOpen)
    return (HSTATUS_ADAPTER_NOT_OPEN);

  for (ulDevice = 0; ulDevice < m_ulNumWaveDevices; ulDevice++)
    m_WaveDevice[ulDevice].Close ();

  for (ulDevice = 0; ulDevice < NUM_MIDI_DEVICES; ulDevice++)
    m_MIDIDevice[ulDevice].Close ();

  if (m_bHasLTC)
    {
      m_TimecodeRX.Close ();
      m_TimecodeTX.Close ();
    }

  m_LStream.Close ();

  m_Mixer.Close ();
  if (m_bHasCS8420)
    {
      m_CS8420.Close ();
    }
  if (m_bHasAK4114)
    {
      m_AK4114.Close ();
    }
  if (m_bHasAES16eDIO)
    {
      m_AES16eDIO.Close ();
    }
  m_SampleClock.Close ();

  // turn off all the interrupts and the LED, put the FPGA in CFGEN mode
  WRITE_REGISTER_ULONG (&m_pRegisters->PCICTL, REG_PCICTL_CNFEN);
  {
    ULONG ulDummy;		// keep the compiler from complaining about empty statements...

    // clear any pending interrupts
    ulDummy = READ_REGISTER_ULONG (&m_pRegisters->MISTAT);
    ulDummy = READ_REGISTER_ULONG (&m_pRegisters->AISTAT);
  }

  if (m_pDMA_BufferList)
    {
      m_HalDriverInfo.pFreeMemory ((PVOID) m_pDMA_BufferList,
				   (PVOID) m_pDMA_VAddr);
      m_pDMA_BufferList = NULL;
      m_pDMA_VAddr = NULL;
      m_pDMA_PAddr = 0;		//was NULL
    }

  // unmap the configuration (cannot do this until all access to the hardware is done)
  if (!bSuspend)
    {
      m_HalDriverInfo.pUnmap (m_HalDriverInfo.pContext, &m_PCIConfig);
    }
  m_pRegisters = NULL;
  m_pAudioBuffers = NULL;

  if (!bSuspend)
    {
      // Reset this to zero so we start over again...
      m_PCIConfig.usBusNumber = 0;
      m_PCIConfig.usDeviceFunction = 0;
    }

  m_bOpen = FALSE;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalAdapter::EnableInterrupts (void)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalAdapter::EnableInterrupts\n");

  if (m_bOpen)
    {
      ULONG ulDummy;		// keep the compiler from complaining about empty statements...

      // clear any pending interrupts
      ulDummy = READ_REGISTER_ULONG (&m_pRegisters->MISTAT);
      ulDummy = READ_REGISTER_ULONG (&m_pRegisters->AISTAT);
      m_RegPCICTL.BitSet (REG_PCICTL_GIE | REG_PCICTL_AIE | REG_PCICTL_MIE,
			  TRUE);
    }
}

/////////////////////////////////////////////////////////////////////////////
void
CHalAdapter::DisableInterrupts (void)
/////////////////////////////////////////////////////////////////////////////
{
  if (m_bOpen)
    {
      m_RegPCICTL.BitSet (REG_PCICTL_GIE | REG_PCICTL_AIE | REG_PCICTL_MIE,
			  FALSE);
    }
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalAdapter::IsWaveDeviceRecord (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber < m_ulNumWaveRecordDevices)
    return (TRUE);
  else
    return (FALSE);
}

/////////////////////////////////////////////////////////////////////////////
PHALWAVEDEVICE
CHalAdapter::GetWaveDevice (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber >= m_ulNumWaveDevices)
    return (NULL);

  return (&m_WaveDevice[ulDeviceNumber]);
}

/////////////////////////////////////////////////////////////////////////////
PHALWAVEDMADEVICE
CHalAdapter::GetWaveDMADevice (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber >= m_ulNumWaveDevices)
    return (NULL);

  return (&m_WaveDevice[ulDeviceNumber]);
}

/////////////////////////////////////////////////////////////////////////////
PHALWAVEDMADEVICE
CHalAdapter::GetWaveDMARecordDevice (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber >= m_ulNumWaveRecordDevices)
    return (NULL);

  return (&m_WaveDevice[ulDeviceNumber]);
}

/////////////////////////////////////////////////////////////////////////////
PHALWAVEDMADEVICE
CHalAdapter::GetWaveDMAPlayDevice (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber >= m_ulNumWavePlayDevices)
    return (NULL);

  return (&m_WaveDevice[m_ulNumWaveRecordDevices + ulDeviceNumber]);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalAdapter::GetNumActiveWaveDevices (void)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulNumActive = 0;
  int nDevice;

  for (nDevice = 0; nDevice < (int) GetNumWaveDevices (); nDevice++)
    {
      if (GetWaveDMADevice (nDevice)->IsRunning ())
	{
	  ulNumActive++;
	}
    }

  return (ulNumActive);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalAdapter::IsMIDIDeviceRecord (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber < NUM_MIDI_RECORD_DEVICES)
    return (TRUE);
  else
    return (FALSE);
}

/////////////////////////////////////////////////////////////////////////////
PHALMIDIDEVICE
CHalAdapter::GetMIDIDevice (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber >= NUM_MIDI_DEVICES)
    return (NULL);

  return (&m_MIDIDevice[ulDeviceNumber]);
}

/////////////////////////////////////////////////////////////////////////////
PHALMIDIDEVICE
CHalAdapter::GetMIDIInDevice (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber >= NUM_MIDI_RECORD_DEVICES)
    return (NULL);

  return (&m_MIDIDevice[ulDeviceNumber]);
}

/////////////////////////////////////////////////////////////////////////////
PHALMIDIDEVICE
CHalAdapter::GetMIDIOutDevice (ULONG ulDeviceNumber)
/////////////////////////////////////////////////////////////////////////////
{
  if (ulDeviceNumber >= NUM_MIDI_PLAY_DEVICES)
    return (NULL);

  return (&m_MIDIDevice[NUM_MIDI_RECORD_DEVICES + ulDeviceNumber]);
}

static ULONG gulAdapterSyncGroup = 0;
static ULONG gulAdapterSyncReady = 0;

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetSyncStartState (BOOLEAN bEnable)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalAdapter::SetSyncStartState\n");

  m_bSyncStart = bEnable;
  // TODO: if any devices are waiting to start, do it now!
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::AddToStartGroup (ULONG ulDeviceNumber)
// Called from a Wave Device when it wants to be added to the start group for this adapter
/////////////////////////////////////////////////////////////////////////////
{
  // Add to the start group on this adapter
  SET (m_ulSyncGroup, (1 << ulDeviceNumber));

  // Add this adapter to the start group for the entire machine
  SET (gulAdapterSyncGroup, (1 << m_ulAdapterNumber));

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::RemoveFromStartGroup (ULONG ulDeviceNumber)
// Called from a Wave Device when it wants to be removed from the start group 
// for this adapter.  This code should see if other devices are waiting, and
// start them if needed - but it currently doesn't.
/////////////////////////////////////////////////////////////////////////////
{
  // Remove from the start group on this adapter
  CLR (m_ulSyncGroup, (1 << ulDeviceNumber));

  // is the sync group now clear?
  if (!m_ulSyncGroup)
    {
      // Remove this adapter to the start group for the entire machine
      CLR (gulAdapterSyncGroup, (1 << m_ulAdapterNumber));
    }

  return (HSTATUS_OK);
}

#ifdef INTERRUPT_TIMING
#ifdef MACINTOSH
LONGLONG SoundGetTime (VOID);
LONGLONG gllLastTime = 0;
#else
LONGLONG SoundGetTime (VOID);
	//#include <stdio.h>
LONGLONG gllLastTime = 0;
#endif
#endif

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SyncStartPrime ()
// This function insures that all devices that are part of a sync start 
// group on *this* adapter are ready to go
// Called By: CHalAdapter::SyncStartReady
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulDevice;

  //cmn_err((CE_WARN,"CHalAdapter::SyncStartPrime A[%lu]\n", m_ulAdapterNumber );

  // make sure the preload is complete for all DMA play devices
  for (ulDevice = m_ulNumWaveRecordDevices; ulDevice < m_ulNumWaveDevices;
       ulDevice++)
    {
      PHALWAVEDMADEVICE pHDD = GetWaveDMADevice (ulDevice);

      // is this device part of the sync group?
      if (((1 << ulDevice) & m_ulSyncGroup) && pHDD->IsDMAActive ())
	{
	  PHALDMA pHDMA = pHDD->GetDMA ();

	  // wait for the preload to complete
	  pHDMA->WaitForPreloadToComplete ();

	  // we must now re-read the stream control register and OR it with the stored value
	  // because the PCPTR value has now changed.
	  if (!m_bIsAES16e)
	    {
	      CLR (m_aulStreamControl[ulDevice], REG_L2STRMCTL_PCPTR_MASK);
	      m_aulStreamControl[ulDevice] |=
		(pHDD->GetStreamControl ()->
		 Read () & REG_L2STRMCTL_PCPTR_MASK);
	    }
	  //cmn_err((CE_WARN,"Read StreamControl [%04lx] %ld\n", m_aulStreamControl[ ulDevice ] & REG_STREAMCTL_FMT_MASK, ulDevice );
	}
    }

  //cmn_err((CE_WARN,"Starting All Devices\n");
  for (ulDevice = 0; ulDevice < m_ulNumWaveDevices; ulDevice++)
    {
      // is this device part of the sync group?
      if (m_ulSyncGroup & (1 << ulDevice))
	{
	  //DS("SyncStart",COLOR_BOLD);   DB('0'+ulDevice,COLOR_UNDERLINE);
	  m_WaveDevice[ulDevice].GetStreamControl ()->
	    Write (m_aulStreamControl[ulDevice]);
	  //cmn_err((CE_WARN,"Write StreamControl [%04lx] %ld\n", m_aulStreamControl[ ulDevice ] & REG_STREAMCTL_FMT_MASK, ulDevice );
	}
    }

  m_ulSyncGroup = 0;
  m_ulSyncReady = 0;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::EnableLStreamSyncStart (BOOLEAN bEnable)
// Called when the application wants the LStream port to send the GSYNC
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"EnableLStreamSyncStart %d\n", bEnable );
  m_bLStreamSyncStart = bEnable;

  // Must update both controls in the mixer
  m_Mixer.ControlChanged (LINE_LSTREAM, LINE_NO_SOURCE,
			  CONTROL_LS1_ADAT_CUEPOINT_ENABLE);
  m_Mixer.ControlChanged (LINE_LSTREAM, LINE_NO_SOURCE,
			  CONTROL_LS2_ADAT_CUEPOINT_ENABLE);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SyncStartGo ()
// Starts all devices by setting GSYNC
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalAdapter::SyncStartGo\n");

  if (m_bLStreamSyncStart)
    {
      // one-shot.  Doesn't write GSYNC to the hardware, lets the LStream port to that
      m_bLStreamSyncStart = FALSE;

      // Tell the LStream module to go ahead and start looking for the timecode
      // if this fails, then we must go ahead and start the device as normal
      if (m_LStream.ADATCuePointEnable ())
	{
	  //cmn_err((CE_WARN,"ADATCuePointEnable Failed!\n");
	  DS (" GSYNC ", COLOR_NORMAL);
	  m_RegSTRMCTL.BitSet (REG_STRMCTL_GSYNC, TRUE);
	}

      // Must update both controls in the mixer
      m_Mixer.ControlChanged (LINE_LSTREAM, LINE_NO_SOURCE,
			      CONTROL_LS1_ADAT_CUEPOINT_ENABLE);
      m_Mixer.ControlChanged (LINE_LSTREAM, LINE_NO_SOURCE,
			      CONTROL_LS2_ADAT_CUEPOINT_ENABLE);
    }
  else
    {
      //DS(" GSYNC ",COLOR_NORMAL);
      m_RegSTRMCTL.BitSet (REG_STRMCTL_GSYNC, TRUE);
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAdapter::SyncStartReady (ULONG ulDeviceNumber, ULONG ulStreamControl)
// Called from a Wave Device when it is ready to start
/////////////////////////////////////////////////////////////////////////////
{
  PHALADAPTER pHA[MAX_NUMBER_OF_ADAPTERS];	// The list of active adapter objects
  int nAdapter;
  int nNumActiveAdapters = 0;
  //PHALADAPTER   pMaster = NULL;
  //int nDevice;

  //cmn_err((CE_WARN,"CHalAdapter::SyncStartReady A[%lu] D[%lu]\n", m_ulAdapterNumber, ulDeviceNumber );

  // save this for later
  m_aulStreamControl[ulDeviceNumber] = ulStreamControl;

  // show that this device is ready to start
  SET (m_ulSyncReady, (1 << ulDeviceNumber));

  // are all the devices in the group on this adapter ready to start?
  if (m_ulSyncGroup == m_ulSyncReady)
    SET (gulAdapterSyncReady, (1 << m_ulAdapterNumber));

  // are all the adapters in the group ready to start?
  if (gulAdapterSyncGroup == gulAdapterSyncReady)
    {
      nNumActiveAdapters = 0;
      // scan thru each opened adapter
      for (nAdapter = 0; nAdapter < MAX_NUMBER_OF_ADAPTERS; nAdapter++)
	{
	  pHA[nAdapter] = NULL;

	  // does this adapter have any devices in the sync group?
	  if (gulAdapterSyncGroup & (1 << nAdapter))
	    {
	      pHA[nNumActiveAdapters] =
		m_HalDriverInfo.pGetAdapter (m_HalDriverInfo.pContext,
					     nAdapter);
	      if (!pHA[nNumActiveAdapters])
		continue;

	      pHA[nNumActiveAdapters]->SyncStartPrime ();
	      nNumActiveAdapters++;
	    }
	}

      // Multicard Sample Accurate Sync-Start...
      if (pHA[0] && pHA[0]->HasLRClock () && (nNumActiveAdapters > 1))
	{
	  PLYNXTWOREGISTERS pRegs = pHA[0]->GetRegisters ();
	  PULONG pulOPBUFSTAT = &pRegs->OPBUFSTAT;

	  //cmn_err((CE_WARN,"Waiting for LRClock...\n");

	  // Wait for L/R clock to transition
	  // if LRCK is currently high, wait for it to go low
	  while (!
		 (READ_REGISTER_ULONG (pulOPBUFSTAT) & REG_OPBUFSTAT_LRCLOCK))
	    ;
	  // LRCK is low, wait for it to go high
	  while ((READ_REGISTER_ULONG (pulOPBUFSTAT) & REG_OPBUFSTAT_LRCLOCK))
	    ;
	  // ok, we should be safe to start the adapters now
	}

      // Start all the adapters
      for (nAdapter = 0; nAdapter < nNumActiveAdapters; nAdapter++)
	{
	  if (pHA[nAdapter])
	    pHA[nAdapter]->SyncStartGo ();
	}

      //cmn_err((CE_WARN,"CHalAdapter::SyncStartReady COMPLETE!\n");

      // reset for next time around
      gulAdapterSyncGroup = 0;
      gulAdapterSyncReady = 0;

      //DPET();
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetInterruptSamples (ULONG ulInterruptSamples)
// Sets the number of samples to try and keep in the buffer (the interrupt point)
// This means that if you set the Interrupt Samples to 32, then the limit
// interrupt will fire when there are 32 samples *left to play* leaving only
// 32 sample periods to transfer in new data before an underrun error.
/////////////////////////////////////////////////////////////////////////////
{
  if (!ulInterruptSamples)
    {
/*
		LONG	lSampleRate;
		m_SampleClock.Get( &lSampleRate );

		if( lSampleRate > 100000 )		ulInterruptSamples = WAVE_CIRCULAR_BUFFER_SIZE - 2048;
		else if( lSampleRate > 50000 )	ulInterruptSamples = WAVE_CIRCULAR_BUFFER_SIZE - 1024;
		else if( lSampleRate > 25000 )	ulInterruptSamples = WAVE_CIRCULAR_BUFFER_SIZE - 512;
		else if( lSampleRate > 12500 )	ulInterruptSamples = WAVE_CIRCULAR_BUFFER_SIZE - 256;
		else							ulInterruptSamples = WAVE_CIRCULAR_BUFFER_SIZE - 128;
		//cmn_err((CE_WARN,"ulInterruptSamples %lu lSampleRate %ld\n", ulInterruptSamples, lSampleRate );
*/
      ulInterruptSamples = (WAVE_CIRCULAR_BUFFER_SIZE / 2);	// Set to half the buffer
    }

  // we need to make sure all the devices are idle before changing this...
  if (GetNumActiveWaveDevices ())
    {
      //cmn_err((CE_WARN,"Device in use. Cannot change Interrupt Samples.\n");
      return (HSTATUS_ALREADY_IN_USE);
    }

  if (m_bIsAES16e)
    {
      // DAH 05/19/08 changed for AES16e 
      // here we are setting the buffer size divided by 16
      ulInterruptSamples = (ulInterruptSamples / 16);
    }
  else
    {
      // FPGA Logic is:
      // RECORD: Limit set when 0 < (PCPTR-L2PTR) < GLIMIT; where PCPTR-L2PTR indicates the number of empty DWORDS in circular buffer. 
      // NOTE:Record limit logic was changed on 4/29/02.
      // PLAY: Limit set when  (L2PTR-PCPTR) > GLIMIT; where L2PTR-PCPTR indicates the number of already played DWORDS in circular buffer.
      //                         214-400=E14 >  1000-200=E00
      //                         E12-200=C12 >  1000-200=E00
      //                         314-600=D14 >  1000-100=F00
      //                         0CA-800=D14 >  1000-400=C00

      // GLIMIT is actually the number of EMPTY samples to fire the interrupt on,
      // not the number of samples remaining.
      ulInterruptSamples = WAVE_CIRCULAR_BUFFER_SIZE - ulInterruptSamples;
    }

  if (m_ulInterruptSamples != ulInterruptSamples)
    {
      //cmn_err((CE_WARN,"Setting Interrupt Samples in Hardware %08lx %ld\n", ulInterruptSamples, WAVE_CIRCULAR_BUFFER_SIZE - ulInterruptSamples );
      //DS(" IntSamples:", COLOR_NORMAL);     DX16( (USHORT)ulInterruptSamples, COLOR_NORMAL );       DC(' ');
      // set the global stream control register (Global Circular Buffer Limit Size)
      // DAH 10/16/02 make sure we don't write the GSYNC bit by accident (this code clear's the shadow register)
      m_RegSTRMCTL.Write (ulInterruptSamples,
			  REG_STRMCTL_GSYNC | REG_STRMCTL_GLIMIT_MASK);
      m_ulInterruptSamples = ulInterruptSamples;
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAdapter::GetFrequencyCounter (USHORT usRegister, PULONG pulFrequency)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulCount, ulScale, ulValue;
  LONGLONG llReference;

  if ((m_usDeviceID == PCIDEVICE_LYNX_L22)
      && (usRegister == L2_FREQCOUNTER_VIDEO))
    return (HSTATUS_INVALID_MIXER_CONTROL);

  if (m_bIsAES16 || m_bIsAES16e)
    {
      if (usRegister >= AES16_FREQCOUNTER_NUMENTIRES)
	return (HSTATUS_INVALID_MIXER_CONTROL);

      if (!m_bHasAES50 && (usRegister >= AES16_FREQCOUNTER_AES50))
	return (HSTATUS_INVALID_MIXER_CONTROL);

    }
  else
    {
      if (usRegister >= L2_FREQCOUNTER_NUMENTIRES)
	return (HSTATUS_INVALID_MIXER_CONTROL);
    }

  llReference = m_ulXtalSpeed;	// only this number needs to be 64 bit for the precision to be OK

  ulCount =
    READ_REGISTER_ULONG (&m_pRegisters->FRBlock[usRegister].
			 Count) & REG_FCBLOCK_COUNT_MASK;
  ulScale =
    READ_REGISTER_ULONG (&m_pRegisters->FRBlock[usRegister].
			 Scale) & REG_FCBLOCK_SCALE_MASK;
  // Count can be 65535 Max and 32768 Min (except for Scale==9, then Count can be any 16-bit number)
  // Scale 9 is     1MHz -> up
  // Scale 8 is   500kHz -> 1MHz
  // Scale 7 is   250kHz -> 500kHz
  // Scale 6 is   125kHz -> 250kHz
  // Scale 5 is  62.5kHz -> 125kHz
  // Scale 4 is 31.25kHz -> 62.5kHz 
  // Scale 3 is  15625Hz -> 31250Hz 
  // Scale 2 is   7812Hz -> 15625Hz

  if (!ulCount || !ulScale)
    {
      *pulFrequency = 0;
    }
  else
    {
      // Range of SCALE is 0..9, but we allow 0..15
      // by using a LONGLONG as the llReference, the intermediate number is 64 bit
      // We add ulCount/2 for rounding
      ulValue =
	(ULONG) (((llReference << (ulScale + 2)) + (ulCount / 2)) / ulCount);

      //cmn_err((CE_WARN,"Counter %lu Scale %lu Count %lu Value %lu\n", (ULONG)usRegister, ulScale, ulCount, ulValue );

      switch (m_ulXtalSpeed)
	{
	case 32000000:		// LynxTWO/L22 cards
	  // if this is the digital input counter, scale it so it reads correctly
	  if (usRegister == L2_FREQCOUNTER_DIGITALIN)
	    {
	      ulValue /= 128;

	      // make sure the value is within the acceptable range
	      if (!((ulValue >= 8000) && (ulValue <= 108000)))
		ulValue = 0;
	    }
	  // put some limits on the video counter
	  if (usRegister == L2_FREQCOUNTER_VIDEO)
	    {
	      // Video Input, should be either 15.734kHz NTSC or 15.625 kHz PAL
	      if (!(((ulValue > 15600) && (ulValue < 15650)) ||
		    ((ulValue > 15709) && (ulValue < 15759))))
		{
		  ulValue = 0;
		}
	    }
	  break;

	case 40000000:		// AES16 & AES16-SRC
	  if ((usRegister >= AES16_FREQCOUNTER_DI1)
	      && (usRegister <= AES16_FREQCOUNTER_DI8))
	    {
	      if (!m_AK4114.IsInputLocked
		  (usRegister - AES16_FREQCOUNTER_DI1))
		{
		  ulValue = 0;
		}
	      else
		{
		  ulValue /= 128;

		  if (ulValue < MIN_SAMPLE_RATE)
		    ulValue = 0;
		}
	    }
	  break;

	case 50000000:		// AES16e, AES16e-SRC & AES16e-50
	  if ((usRegister >= AES16_FREQCOUNTER_DI1)
	      && (usRegister <= AES16_FREQCOUNTER_DI8))
	    {
	      /*
	         if( )
	         {
	         ulValue = 0;
	         }
	         else
	       */
	      {
		if (ulValue < MIN_SAMPLE_RATE)
		  ulValue = 0;
	      }
	    }
	  break;
	}

      *pulFrequency = ulValue;
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAdapter::NormalizeFrequency (ULONG ulFrequency, PLONG plRate,
				   PLONG plReference)
// Takes any frequency as an input and returns a normalized (standardized)
// frequency in return
/////////////////////////////////////////////////////////////////////////////
{
  LONG lRate = 0;
  LONG lReference;

  if (plReference)
    lReference = *plReference;

  // normalize the rate
  if (ulFrequency < 16000)
    {
      lRate = 0;
    }
  else if (ulFrequency < 27025)	// half way between 22050 & 32kHz
    {
      lRate = 22050;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 38050)	// half way between 32kHz & 44.1kHz
    {
      lRate = 32000;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 46050)	// half way between 44.1kHz & 48kHz
    {
      lRate = 44100;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 49600)	// you get the idea
    {
      lRate = 48000;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 57600)
    {
      lRate = 51200;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 76100)
    {
      lRate = 64000;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 92100)
    {
      lRate = 88200;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 99200)
    {
      lRate = 96000;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 115200)
    {
      lRate = 102400;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 152200)
    {
      lRate = 128000;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 184200)
    {
      lRate = 176400;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 215000)	// 215kHz is the top end of this set
    {
      lRate = 192000;
      lReference = MIXVAL_CLKREF_WORD;
    }
  else if (ulFrequency < 7372800)	// 8.192MHz - 10%
    {
      lRate = 0;
    }
  else if (ulFrequency < 9740800)	// half way between 32kHz & 44.1kHz
    {
      lRate = 32000;
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else if (ulFrequency < 11788800)	// half way between 44.1kHz & 48kHz
    {
      lRate = 44100;
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else if (ulFrequency < 12894000)	// half way between 44.1kHz & 48kHz
    {
      lRate = 48000;
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else if (ulFrequency < 14942000)
    {
      lReference = MIXVAL_CLKREF_13p5MHZ;
    }
  else if (ulFrequency < 19481600)
    {
      lRate = 64000;
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else if (ulFrequency < 23577600)
    {
      lRate = 88200;
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else if (ulFrequency < 25788000)
    {
      lRate = 96000;		// 24576000
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else if (ulFrequency < 29884000)
    {
      lReference = MIXVAL_CLKREF_27MHZ;
    }
  else if (ulFrequency < 38963200)
    {
      lRate = 128000;		// 32768000
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else if (ulFrequency < 47155200)
    {
      lRate = 176400;
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else if (ulFrequency < 54067200)	// 49MHz + 10%
    {
      lRate = 192000;
      lReference = MIXVAL_CLKREF_WORD256;
    }
  else
    {
      lRate = 0;
    }

  *plRate = lRate;

  if (plReference)
    *plReference = lReference;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalAdapter::GetNTSCPAL (void)
// returns TRUE for NTSC and FALSE for PAL
/////////////////////////////////////////////////////////////////////////////
{
  if (READ_REGISTER_ULONG (&m_pRegisters->FRBlock[0].Scale) &
      REG_FCBLOCK_NTSCPALn)
    return (TRUE);

  return (FALSE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::IOWrite (BYTE ucAddress, BYTE ucData, BYTE ucMask)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulAddress = (ULONG) ucAddress;
  ULONG ulData;

  // IOWrite & IORead are invalid on the AES16
  if (m_bIsAES16 || m_bIsAES16e)
    return (HSTATUS_INVALID_PARAMETER);

  if (ucAddress >= NUM_IO_REGISTERS)
    return (HSTATUS_INVALID_PARAMETER);

  // read the current register out of the shadow memory
  ulData = (ULONG) m_aIORegisters[ucAddress];

  CLR (ulData, ucMask);
  SET (ulData, (ucData & ucMask));

  //cmn_err((CE_WARN,"IOWrite %02x %02x\n", (USHORT)ulAddress, (USHORT)ulData );

  // protect this function from re-entrancy problems - without a spin lock or other such thing
  if (!m_lIOEntryCount)
    {
      m_lIOEntryCount++;
      ////////////////////
      // Protected
      ////////////////////

      ///////////////////// THIS MUST NOT BE INTERRUPTED //////////////////////
      WRITE_REGISTER_ULONG (&m_pRegisters->IODATA, (ulData & 0xFF));
      WRITE_REGISTER_ULONG (&m_pRegisters->IOADDR,
			    (ulAddress | REG_IOADDR_WRITE));
      ///////////////////// THIS MUST NOT BE INTERRUPTED //////////////////////

      ////////////////////
      // Protected
      ////////////////////
      m_lIOEntryCount--;

      // DAH 10/13/2005 moved to inside of if statement so reentrancy will not update the shadow
      // save the current register back to the shadow memory
      m_aIORegisters[ucAddress] = (BYTE) ulData;
    }
#ifdef DEBUG
  else
    {
      cmn_err (CE_WARN, "IOWrite Reentrant!\n");
    }
#endif

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAdapter::IORead (BYTE ucAddress, PBYTE pucData, BOOLEAN bReadShadowOnly)
// Only called by Hal8420
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulAddress = (ULONG) ucAddress;
  ULONG ulData;
  USHORT usStatus = HSTATUS_OK;

  // IOWrite & IORead are invalid on the AES16
  if (m_bIsAES16 || m_bIsAES16e)
    return (HSTATUS_INVALID_PARAMETER);

  if (ucAddress >= NUM_IO_REGISTERS)
    return (HSTATUS_INVALID_PARAMETER);

  // if this is a read-write register
  if ((ucAddress < 0x90) && !bReadShadowOnly)
    {
      //cmn_err((CE_WARN,"IORead %02x ", (USHORT)ulAddress );
      ulData = m_aIORegisters[ucAddress];	// fake read from shadow in case we are reentering and we will fail...

      // protect this function from re-entrancy problems - without a spin lock or other such thing
      if (!m_lIOEntryCount)
	{
	  m_lIOEntryCount++;
	  ////////////////////
	  // Protected
	  ////////////////////

	  ///////////////////// THIS MUST NOT BE INTERRUPTED //////////////////////
	  WRITE_REGISTER_ULONG (&m_pRegisters->IOADDR,
				(ulAddress | REG_IOADDR_READ));
	  ulData = READ_REGISTER_ULONG (&m_pRegisters->IODATA) & 0xFF;
	  ///////////////////// THIS MUST NOT BE INTERRUPTED //////////////////////

	  ////////////////////
	  // Protected
	  ////////////////////
	  m_lIOEntryCount--;
	}
#ifdef DEBUG
      else
	{
	  cmn_err (CE_WARN, "IORead Reentrant!\n");
	  usStatus = HSTATUS_INVALID_MODE;
	}
#endif

      // save the current register back to the shadow memory
      m_aIORegisters[ucAddress] = (BYTE) ulData;
    }

  *pucData = m_aIORegisters[ucAddress];

  return (usStatus);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetTrim (USHORT usControl, ULONG ulValue)
// Must check for device id here too - because the OSX mixer is dumb about
// how it saves controls
/////////////////////////////////////////////////////////////////////////////
{
  switch (usControl)
    {
    case CONTROL_AIN12_TRIM:
      // All cards have these in common
      if (ulValue)
	IOWrite (kTrim, IO_TRIM_ABC_AIN12_MINUS10, IO_TRIM_ABC_AIN12_MASK);
      else
	IOWrite (kTrim, IO_TRIM_ABC_AIN12_PLUS4, IO_TRIM_ABC_AIN12_MASK);
      m_lTrimAI12 = ulValue;
      break;
    case CONTROL_AIN34_TRIM:
      // A&C Only
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_B)
	  || (m_usDeviceID == PCIDEVICE_LYNX_L22))
	return (HSTATUS_INVALID_MIXER_CONTROL);

      if (ulValue)
	IOWrite (kTrim, IO_TRIM_AC_AIN34_MINUS10, IO_TRIM_AC_AIN34_MASK);
      else
	IOWrite (kTrim, IO_TRIM_AC_AIN34_PLUS4, IO_TRIM_AC_AIN34_MASK);
      m_lTrimAI34 = ulValue;
      break;
    case CONTROL_AIN56_TRIM:
      // C Only
      if (m_usDeviceID != PCIDEVICE_LYNXTWO_C)
	return (HSTATUS_INVALID_MIXER_CONTROL);

      if (ulValue)
	IOWrite (kTrim, IO_TRIM_C_AIN56_MINUS10, IO_TRIM_C_AIN56_MASK);
      else
	IOWrite (kTrim, IO_TRIM_C_AIN56_PLUS4, IO_TRIM_C_AIN56_MASK);
      m_lTrimAI56 = ulValue;
      break;
    case CONTROL_AOUT12_TRIM:
      // C is different than all the other cards
      if (m_usDeviceID == PCIDEVICE_LYNXTWO_C)
	{
	  // Bit3
	  if (ulValue)
	    IOWrite (kTrim, IO_TRIM_C_AOUT12_MINUS10, IO_TRIM_C_AOUT12_MASK);
	  else
	    IOWrite (kTrim, IO_TRIM_C_AOUT12_PLUS4, IO_TRIM_C_AOUT12_MASK);
	}
      else
	{
	  // Bit2
	  if (ulValue)
	    IOWrite (kTrim, IO_TRIM_AB_AOUT12_MINUS10,
		     IO_TRIM_AB_AOUT12_MASK);
	  else
	    IOWrite (kTrim, IO_TRIM_AB_AOUT12_PLUS4, IO_TRIM_AB_AOUT12_MASK);
	}
      m_lTrimAO12 = ulValue;
      break;
    case CONTROL_AOUT34_TRIM:
      // A&B Only - Bit3
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_C)
	  || (m_usDeviceID == PCIDEVICE_LYNX_L22))
	return (HSTATUS_INVALID_MIXER_CONTROL);

      if (ulValue)
	IOWrite (kTrim, IO_TRIM_AB_AOUT34_MINUS10, IO_TRIM_AB_AOUT34_MASK);
      else
	IOWrite (kTrim, IO_TRIM_AB_AOUT34_PLUS4, IO_TRIM_AB_AOUT34_MASK);
      m_lTrimAO34 = ulValue;
      break;
    case CONTROL_AOUT56_TRIM:
      // B Only - Bit1
      if (m_usDeviceID != PCIDEVICE_LYNXTWO_B)
	return (HSTATUS_INVALID_MIXER_CONTROL);

      if (ulValue)
	IOWrite (kTrim, IO_TRIM_B_AOUT56_MINUS10, IO_TRIM_B_AOUT56_MASK);
      else
	IOWrite (kTrim, IO_TRIM_B_AOUT56_PLUS4, IO_TRIM_B_AOUT56_MASK);
      m_lTrimAO56 = ulValue;
      break;
    }
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::GetTrim (USHORT usControl, PULONG pulValue)
// MUST check for the model here!
/////////////////////////////////////////////////////////////////////////////
{
  if (!m_bIsLynxTWO)
    return (HSTATUS_INVALID_MIXER_CONTROL);

  switch (usControl)
    {
    case CONTROL_AIN12_TRIM:
      // All have this
      *pulValue = m_lTrimAI12;
      break;
    case CONTROL_AIN34_TRIM:
      // only the A&C have this control
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_B)
	  || (m_usDeviceID == PCIDEVICE_LYNX_L22))
	return (HSTATUS_INVALID_MIXER_CONTROL);
      *pulValue = m_lTrimAI34;
      break;
    case CONTROL_AIN56_TRIM:
      // only the C has this control
      if (m_usDeviceID != PCIDEVICE_LYNXTWO_C)
	return (HSTATUS_INVALID_MIXER_CONTROL);
      *pulValue = m_lTrimAI56;
      break;
    case CONTROL_AOUT12_TRIM:
      // All have this
      *pulValue = m_lTrimAO12;
      break;
    case CONTROL_AOUT34_TRIM:
      // only the A&B have this control
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_C)
	  || (m_usDeviceID == PCIDEVICE_LYNX_L22))
	return (HSTATUS_INVALID_MIXER_CONTROL);
      *pulValue = m_lTrimAO34;
      break;
    case CONTROL_AOUT56_TRIM:
      // only the B has this control
      if (m_usDeviceID != PCIDEVICE_LYNXTWO_B)
	return (HSTATUS_INVALID_MIXER_CONTROL);
      *pulValue = m_lTrimAO56;
      break;
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetADHPF (USHORT usControl, BOOLEAN bEnable)
/////////////////////////////////////////////////////////////////////////////
{
  // no need to check for validity of usControl here...
  // ADCs are pin-controlled.  Doesn't matter if the ADC don't exist
  switch (usControl)
    {
    case CONTROL_AIN12_HPF:
      IOWrite (kADC_A_Control, bEnable ? IO_ADC_CNTL_HPFE : IO_ADC_CNTL_ZCAL,
	       IO_ADC_CNTL_HPFE | IO_ADC_CNTL_ZCAL);
      m_bAIn12HPFEnable = bEnable;
      break;
    case CONTROL_AIN34_HPF:
      IOWrite (kADC_B_Control, bEnable ? IO_ADC_CNTL_HPFE : IO_ADC_CNTL_ZCAL,
	       IO_ADC_CNTL_HPFE | IO_ADC_CNTL_ZCAL);
      m_bAIn34HPFEnable = bEnable;
      break;
    case CONTROL_AIN56_HPF:
      IOWrite (kADC_C_Control, bEnable ? IO_ADC_CNTL_HPFE : IO_ADC_CNTL_ZCAL,
	       IO_ADC_CNTL_HPFE | IO_ADC_CNTL_ZCAL);
      m_bAIn56HPFEnable = bEnable;
      break;
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalAdapter::GetADHPF (USHORT usControl)
/////////////////////////////////////////////////////////////////////////////
{
  // no need to check for validity of usControl here...
  switch (usControl)
    {
    case CONTROL_AIN12_HPF:
      return (m_bAIn12HPFEnable);
    case CONTROL_AIN34_HPF:
      return (m_bAIn34HPFEnable);
    case CONTROL_AIN56_HPF:
      return (m_bAIn56HPFEnable);
    }
  return (FALSE);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetConverterSpeed (LONG lSampleRate)
/////////////////////////////////////////////////////////////////////////////
{
  // there are no converters on the AES16...
  if (m_bIsAES16 || m_bIsAES16e)
    return (HSTATUS_OK);

  //cmn_err((CE_WARN,"Changing Speed\n");
  if (lSampleRate <= 50000)
    {
      BYTE ucMode = IO_DAC_CNTL_MODE_1X_EMPOFF;

      // Setup the ADCs (pin-controlled)
      IOWrite (kADC_A_Control, IO_ADC_CNTL_DFS_1X, IO_ADC_CNTL_DFS_MASK);
      IOWrite (kADC_B_Control, IO_ADC_CNTL_DFS_1X, IO_ADC_CNTL_DFS_MASK);
      IOWrite (kADC_C_Control, IO_ADC_CNTL_DFS_1X, IO_ADC_CNTL_DFS_MASK);

      if (m_bDACDeEmphasis)
	{
	  // Half-way between 44.1khz & 32kHz
	  if (lSampleRate < 38050)
	    ucMode = IO_DAC_CNTL_MODE_1X_EMP32K;
	  // Half-way between 48kHz & 44.1kHz
	  else if (lSampleRate < 46050)
	    ucMode = IO_DAC_CNTL_MODE_1X_EMP44K;
	  // Everything else is 48kHz
	  else
	    ucMode = IO_DAC_CNTL_MODE_1X_EMP48K;
	}

      // Setup the DACs (serial-controlled)
      IOWrite (kDAC_A_Control, ucMode, IO_DAC_CNTL_MODE_MASK);
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
	  || (m_usDeviceID == PCIDEVICE_LYNXTWO_B))
	IOWrite (kDAC_B_Control, ucMode, IO_DAC_CNTL_MODE_MASK);
      if (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
	IOWrite (kDAC_C_Control, ucMode, IO_DAC_CNTL_MODE_MASK);
    }
  else if (lSampleRate <= 100000)
    {
      IOWrite (kADC_A_Control, IO_ADC_CNTL_DFS_2X, IO_ADC_CNTL_DFS_MASK);
      IOWrite (kADC_B_Control, IO_ADC_CNTL_DFS_2X, IO_ADC_CNTL_DFS_MASK);
      IOWrite (kADC_C_Control, IO_ADC_CNTL_DFS_2X, IO_ADC_CNTL_DFS_MASK);

      IOWrite (kDAC_A_Control, IO_DAC_CNTL_MODE_2X, IO_DAC_CNTL_MODE_MASK);
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
	  || (m_usDeviceID == PCIDEVICE_LYNXTWO_B))
	IOWrite (kDAC_B_Control, IO_DAC_CNTL_MODE_2X, IO_DAC_CNTL_MODE_MASK);
      if (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
	IOWrite (kDAC_C_Control, IO_DAC_CNTL_MODE_2X, IO_DAC_CNTL_MODE_MASK);
    }
  else				// above 100kHz
    {
      IOWrite (kADC_A_Control, IO_ADC_CNTL_DFS_4X, IO_ADC_CNTL_DFS_MASK);
      IOWrite (kADC_B_Control, IO_ADC_CNTL_DFS_4X, IO_ADC_CNTL_DFS_MASK);
      IOWrite (kADC_C_Control, IO_ADC_CNTL_DFS_4X, IO_ADC_CNTL_DFS_MASK);

      IOWrite (kDAC_A_Control, IO_DAC_CNTL_MODE_4X, IO_DAC_CNTL_MODE_MASK);
      if ((m_usDeviceID == PCIDEVICE_LYNXTWO_A)
	  || (m_usDeviceID == PCIDEVICE_LYNXTWO_B))
	IOWrite (kDAC_B_Control, IO_DAC_CNTL_MODE_4X, IO_DAC_CNTL_MODE_MASK);
      if (m_usDeviceID == PCIDEVICE_LYNXTWO_B)
	IOWrite (kDAC_C_Control, IO_DAC_CNTL_MODE_4X, IO_DAC_CNTL_MODE_MASK);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetDADeEmphasis (BOOLEAN bEnable)
/////////////////////////////////////////////////////////////////////////////
{
  LONG lRate;

  m_SampleClock.Get (&lRate);

  m_bDACDeEmphasis = bEnable;

  // Setting the converter speed also changes the de-emphasis bits on the DACs
  SetConverterSpeed (lRate);

  return (HSTATUS_OK);
}

////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::CalibrateConverters ()
/////////////////////////////////////////////////////////////////////////////
{
  // there are no converters on the AES16...
  if (m_bIsAES16 || m_bIsAES16e)
    return (HSTATUS_OK);

  // enable DC offset calibration on all converters
  IOWrite (kMisc, 0, (IO_MISC_DACARSTn | IO_MISC_DACBRSTn | IO_MISC_DACCRSTn));	//reset DAC's
  IOWrite (kADC_A_Control, (BYTE) 0, IO_ADC_CNTL_RSTn);
  IOWrite (kADC_B_Control, (BYTE) 0, IO_ADC_CNTL_RSTn);
  IOWrite (kADC_C_Control, (BYTE) 0, IO_ADC_CNTL_RSTn);

  IOWrite (kMisc,
	   (IO_MISC_DACARSTn | IO_MISC_DACBRSTn | IO_MISC_DACCRSTn),
	   (IO_MISC_DACARSTn | IO_MISC_DACBRSTn | IO_MISC_DACCRSTn));

  //Re-write control registers after being cleared by reset
  IOWrite (kDAC_A_Control, (BYTE) 0, IO_DAC_CNTL_CAL);
  IOWrite (kDAC_B_Control, (BYTE) 0, IO_DAC_CNTL_CAL);
  IOWrite (kDAC_C_Control, (BYTE) 0, IO_DAC_CNTL_CAL);

  IOWrite (kADC_A_Control, IO_ADC_CNTL_RSTn, IO_ADC_CNTL_RSTn);
  IOWrite (kADC_B_Control, IO_ADC_CNTL_RSTn, IO_ADC_CNTL_RSTn);
  IOWrite (kADC_C_Control, IO_ADC_CNTL_RSTn, IO_ADC_CNTL_RSTn);

  return (HSTATUS_OK);
}

////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::CalibrateADConverters ()
/////////////////////////////////////////////////////////////////////////////
{
  // there are no converters on the AES16...
  if (m_bIsAES16 || m_bIsAES16e)
    return (HSTATUS_OK);

  // enable DC offset calibration on all converters
  IOWrite (kADC_A_Control, (BYTE) 0, IO_ADC_CNTL_RSTn);
  IOWrite (kADC_B_Control, (BYTE) 0, IO_ADC_CNTL_RSTn);
  IOWrite (kADC_C_Control, (BYTE) 0, IO_ADC_CNTL_RSTn);

  // Do we need to wait here for a moment?

  IOWrite (kADC_A_Control, IO_ADC_CNTL_RSTn, IO_ADC_CNTL_RSTn);
  IOWrite (kADC_B_Control, IO_ADC_CNTL_RSTn, IO_ADC_CNTL_RSTn);
  IOWrite (kADC_C_Control, IO_ADC_CNTL_RSTn, IO_ADC_CNTL_RSTn);

  return (HSTATUS_OK);
}

////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::CalibrateDAConverters ()
/////////////////////////////////////////////////////////////////////////////
{
  // there are no converters on the AES16...
  if (m_bIsAES16 || m_bIsAES16e)
    return (HSTATUS_OK);

  // enable DC offset calibration on all converters
  IOWrite (kMisc, 0, (IO_MISC_DACARSTn | IO_MISC_DACBRSTn | IO_MISC_DACCRSTn));	//reset DAC's

  // Do we need to wait here for a moment?

  IOWrite (kMisc,
	   (IO_MISC_DACARSTn | IO_MISC_DACBRSTn | IO_MISC_DACCRSTn),
	   (IO_MISC_DACARSTn | IO_MISC_DACBRSTn | IO_MISC_DACCRSTn));

  //Re-write control registers after being cleared by reset
  IOWrite (kDAC_A_Control, (BYTE) 0, IO_DAC_CNTL_CAL);
  IOWrite (kDAC_B_Control, (BYTE) 0, IO_DAC_CNTL_CAL);
  IOWrite (kDAC_C_Control, (BYTE) 0, IO_DAC_CNTL_CAL);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetAutoRecalibrate (BOOLEAN bEnable)
/////////////////////////////////////////////////////////////////////////////
{
  m_bAutoRecalibrate = bEnable;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
  CHalAdapter::SetDitherType (PHALREGISTER pRMix0Control, USHORT usDitherType)
/////////////////////////////////////////////////////////////////////////////
{
  // V3 MixBlock handles this differently
  if (m_bHasPMixV3)
    {
      //cmn_err (CE_WARN, "CHalAdapter::SetDitherType %u\n", usDitherType);

      switch (usDitherType)
	{
	  //case MIXVAL_DITHER_NONE:                              pRMix0Control->Write( REG_RMIX_V3_DITHERTYPE_NONE,                              REG_RMIX_V3_DITHERTYPE_MASK );  break;
	case MIXVAL_DITHER_TRIANGULAR_PDF:
	  pRMix0Control->Write (REG_RMIX_V3_DITHERTYPE_TRIANGULAR_PDF,
				REG_RMIX_V3_DITHERTYPE_MASK);
	  break;
	case MIXVAL_DITHER_RECTANGULAR_PDF:
	  pRMix0Control->Write (REG_RMIX_V3_DITHERTYPE_RECTANGULAR_PDF,
				REG_RMIX_V3_DITHERTYPE_MASK);
	  break;
	default:
	  return (HSTATUS_INVALID_PARAMETER);
	}
    }
  else
    {
      switch (usDitherType)
	{
	  //case MIXVAL_DITHER_NONE:                              pRMix0Control->Write( REG_RMIX_DITHERTYPE_NONE,                                   REG_RMIX_DITHERTYPE_MASK );   break;
	case MIXVAL_DITHER_TRIANGULAR_PDF:
	  pRMix0Control->Write (REG_RMIX_DITHERTYPE_TRIANGULAR_PDF,
				REG_RMIX_DITHERTYPE_MASK);
	  break;
	  //case MIXVAL_DITHER_TRIANGULAR_NS_PDF: pRMix0Control->Write( REG_RMIX_DITHERTYPE_TRIANGULAR_PDF_HI_PASS, REG_RMIX_DITHERTYPE_MASK );   break;
	case MIXVAL_DITHER_RECTANGULAR_PDF:
	  pRMix0Control->Write (REG_RMIX_DITHERTYPE_RECTANGULAR_PDF,
				REG_RMIX_DITHERTYPE_MASK);
	  break;
	default:
	  return (HSTATUS_INVALID_PARAMETER);
	}
    }
  m_usDitherType = usDitherType;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetMTCSource (ULONG ulMTCSource)
/////////////////////////////////////////////////////////////////////////////
{
  // The AES16 doesn't need this control!
  if (m_bIsAES16 || m_bIsAES16e)
    {
      return (HSTATUS_INVALID_MIXER_CONTROL);
    }

  switch (ulMTCSource)
    {
    case MIXVAL_MTCSOURCE_LTCRX:
      if (m_usDeviceID == PCIDEVICE_LYNX_L22)
	{
	  // change the default
	  if (m_ulMTCSource == MIXVAL_MTCSOURCE_LTCRX)
	    m_ulMTCSource = MIXVAL_MTCSOURCE_LSTREAM2_ADAT_SYNCIN;
	  return (HSTATUS_INVALID_MIXER_VALUE);
	}
    case MIXVAL_MTCSOURCE_LSTREAM1_ADAT_SYNCIN:
    case MIXVAL_MTCSOURCE_LSTREAM2_ADAT_SYNCIN:
      m_ulMTCSource = ulMTCSource;
      break;
    default:
      return (HSTATUS_INVALID_MIXER_VALUE);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::GetMTCSource (PULONG pulMTCSource)
/////////////////////////////////////////////////////////////////////////////
{
  // The AES16 doesn't need this control!
  if (m_bIsAES16 || m_bIsAES16e)
    {
      // set the default to Header LStream
      *pulMTCSource = MIXVAL_MTCSOURCE_LSTREAM2_ADAT_SYNCIN;
      return (HSTATUS_INVALID_MIXER_CONTROL);
    }

  *pulMTCSource = m_ulMTCSource;

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetMultiChannelRecord (LONG lNumChannels)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulNumberOfChannels;
  int nDevice;
  PHALWAVEDMADEVICE pHD;

  if (!m_bHasMultiChannel)
    {
      cmn_err (CE_WARN, "Firmware does not support multi-channel!\n");
      return (HSTATUS_INVALID_MODE);
    }

  ulNumberOfChannels = (lNumChannels / 2) - 1;

  if (((ulNumberOfChannels + 1) * 2) != (ULONG) lNumChannels)
    return (HSTATUS_INVALID_PARAMETER);

  // Acquire all the record devices on this adapter (except device 0, 
  // which should already be Acquired by the driver)
  for (nDevice = 1; nDevice < (int) m_ulNumWaveRecordDevices; nDevice++)
    {
      pHD = GetWaveDMARecordDevice (nDevice);
      if (pHD->Acquire ())
	{
	  ClearMultiChannelRecord ();
	  return (HSTATUS_ALREADY_IN_USE);
	}

      // call *just* the base class start method so the device looks active
      //((CHalDevice *)pHD)->Start();
    }

  m_RegSTRMCTL.Write (REG_STRMCTL_RMULTI |
		      (ulNumberOfChannels << REG_STRMCTL_RNUMCHNL_OFFSET),
		      REG_STRMCTL_RMULTI | REG_STRMCTL_RNUMCHNL_MASK);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::ClearMultiChannelRecord (void)
/////////////////////////////////////////////////////////////////////////////
{
  int nDevice;
  PHALWAVEDMADEVICE pHD;

  for (nDevice = 1; nDevice < (int) m_ulNumWaveRecordDevices; nDevice++)
    {
      pHD = GetWaveDMARecordDevice (nDevice);
      pHD->Release ();

      // call *just* the base class stop method
      //((CHalDevice *)pHD)->Stop();
    }

  m_RegSTRMCTL.Write (0, REG_STRMCTL_RMULTI | REG_STRMCTL_RNUMCHNL_MASK);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetMultiChannelPlay (LONG lNumChannels)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulNumberOfChannels;
  int nDevice;
  PHALWAVEDMADEVICE pHD;

  if (!m_bHasMultiChannel)
    {
      cmn_err (CE_WARN, "Firmware does not support multi-channel!\n");
      return (HSTATUS_INVALID_MODE);
    }

  //cmn_err((CE_WARN,"SetMultiChannelPlay %ld\n", lNumChannels );

  ulNumberOfChannels = (lNumChannels / 2) - 1;

  if (((ulNumberOfChannels + 1) * 2) != (ULONG) lNumChannels)
    {
      cmn_err (CE_WARN, "lNumChannels does not match!\n");
      return (HSTATUS_INVALID_PARAMETER);
    }

  // Acquire all the play devices on this adapter (except device 0, 
  // which should already be Acquired by the driver)
  for (nDevice = 1; nDevice < (int) m_ulNumWavePlayDevices; nDevice++)
    {
      pHD = GetWaveDMAPlayDevice (nDevice);
      if (pHD->Acquire ())
	{
	  ClearMultiChannelPlay ();
	  cmn_err (CE_WARN,
		   "CHalAdapter::SetMultiChannelPlay: Device [%d] Already In Use!\n",
		   nDevice);
	  return (HSTATUS_ALREADY_IN_USE);
	}

      // call *just* the base class start method so the device looks active
      //((CHalDevice *)pHD)->Start();
    }

  m_RegSTRMCTL.Write (REG_STRMCTL_PMULTI |
		      (ulNumberOfChannels << REG_STRMCTL_PNUMCHNL_OFFSET),
		      REG_STRMCTL_PMULTI | REG_STRMCTL_PNUMCHNL_MASK);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::ClearMultiChannelPlay (void)
/////////////////////////////////////////////////////////////////////////////
{
  int nDevice;
  PHALWAVEDMADEVICE pHD;

  //cmn_err((CE_WARN,"ClearMultiChannelPlay\n");

  for (nDevice = 1; nDevice < (int) m_ulNumWavePlayDevices; nDevice++)
    {
      pHD = GetWaveDMAPlayDevice (nDevice);
      pHD->Release ();

      // call *just* the base class stop method
      //((CHalDevice *)pHD)->Stop();
    }

  m_RegSTRMCTL.Write (0, REG_STRMCTL_PMULTI | REG_STRMCTL_PNUMCHNL_MASK);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::GetMRMEnable (PULONG pulValue)
/////////////////////////////////////////////////////////////////////////////
{
  if (!m_bHasMRMEnable)
    return (HSTATUS_INVALID_MIXER_CONTROL);

  *pulValue = (m_RegPCICTL.Read () & REG_PCICTL_MRM_EN) ? TRUE : FALSE;
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SetMRMEnable (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_RegPCICTL.BitSet (REG_PCICTL_MRM_EN, (BOOLEAN) (ulValue & kBit0));
  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::Update (void)
// This function needs to be called every 250ms to insure that asynchronous 
// events that occur external to the card are noticed.
/////////////////////////////////////////////////////////////////////////////
{
  m_SampleClock.UpdateClockSource ();

  if (m_bHasCS8420)
    m_CS8420.UpdateStatus ();

  //if( m_bIsAES16e )
  //      m_WaveDevice[ NUM_E_WAVE_RECORD_DEVICES ].GetSamplesPlayed();

  return (HSTATUS_OK);
}

#if OSX && TARGET_CPU_PPC
/////////////////////////////////////////////////////////////////////////////
ULONG
MySwapInt32 (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  register BYTE HH, HL, LH, LL;
  HH = HIBYTE (HIWORD (ulValue));
  HL = LOBYTE (HIWORD (ulValue));
  LH = HIBYTE (LOWORD (ulValue));
  LL = LOBYTE (LOWORD (ulValue));
  return (MAKEULONG (MAKEUSHORT (HH, HL), MAKEUSHORT (LH, LL)));
}

/////////////////////////////////////////////////////////////////////////////
ULONG
  CHalAdapter::ReadRegisterULONG (PHALADAPTER pHalAdapter, PULONG pulAddress)
/////////////////////////////////////////////////////////////////////////////
{
  if (pHalAdapter->m_bSwap)
    return (MySwapInt32 (*pulAddress));
  else
    return (*pulAddress);
}

/////////////////////////////////////////////////////////////////////////////
VOID
  CHalAdapter::WriteRegisterULONG (PHALADAPTER pHalAdapter, PULONG pulAddress,
				   ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  if (pHalAdapter->m_bSwap)
    *pulAddress = MySwapInt32 (ulValue);
  else
    *pulAddress = ulValue;

}
#endif

#ifdef DEBUG
/////////////////////////////////////////////////////////////////////////////
VOID
CHalAdapter::ChangeDebugBit0 (BOOLEAN bSet)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulValue = 0;

  if (bSet)
    SET (ulValue, REG_STRMCTL_SW_DEBUG_0);

  m_RegSTRMCTL.Write (ulValue, REG_STRMCTL_SW_DEBUG_0);
}

/////////////////////////////////////////////////////////////////////////////
VOID
CHalAdapter::ChangeDebugBit1 (BOOLEAN bSet)
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulValue = 0;

  if (bSet)
    SET (ulValue, REG_STRMCTL_SW_DEBUG_1);

  m_RegSTRMCTL.Write (ulValue, REG_STRMCTL_SW_DEBUG_1);
}
#endif

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::SaveInterruptContext (ULONG ulAISTAT, ULONG ulMISTAT)
// Called at interrupt time
/////////////////////////////////////////////////////////////////////////////
{
  USHORT usStatus = HSTATUS_OK;

  //DPET();

  //DC('I');
  if (!m_bOpen)
    {
      DB ('!', COLOR_BOLD);
      cmn_err (CE_WARN,
	       "SaveInterruptContext called when adapter is not open!\n");
      return (HSTATUS_ADAPTER_NOT_OPEN);
    }

  //if( m_lEntryCount )
  //{
  //      cmn_err((CE_WARN,"Entry Count Non-Zero!\n");
  //}
  //m_lEntryCount++;

  LONG lNumberOfPendingInterrupts = (LONG) m_ulICHead - (LONG) m_ulICTail;
  if (lNumberOfPendingInterrupts < 0)
    lNumberOfPendingInterrupts += MAX_PENDING_INTERRUPTS;

  // don't think this can ever happen the way the above is coded
  if (lNumberOfPendingInterrupts >= MAX_PENDING_INTERRUPTS)
    {
      DB ('E', COLOR_BOLD);
      cmn_err (CE_WARN, "Exceeded Maximum Pending Interrupts!\n");
      return (HSTATUS_INVALID_PARAMETER);
    }

  if (lNumberOfPendingInterrupts < 0)
    {
      cmn_err (CE_WARN, "lNumberOfPendingInterrupts < 0!\n");
    }
  //DX8((BYTE)lNumberOfPendingInterrupts, COLOR_REVERSE);

  if (ulMISTAT)
    m_aInterruptContext[m_ulICHead].MISTAT = ulMISTAT;
  else
    m_aInterruptContext[m_ulICHead].MISTAT =
      READ_REGISTER_ULONG (&m_pRegisters->MISTAT);
  if (ulAISTAT)
    m_aInterruptContext[m_ulICHead].AISTAT = ulAISTAT;
  else
    m_aInterruptContext[m_ulICHead].AISTAT =
      READ_REGISTER_ULONG (&m_pRegisters->AISTAT);

  // Clear the CNFDI and BOOTPROM bits in case they were set - so the compare to zero later in this function will work properly
  CLR (m_aInterruptContext[m_ulICHead].MISTAT,
       (REG_MISTAT_CNFDI | REG_MISTAT_BOOTPROM));

  //DPET("Int:");
  //DC(' '); DX32( m_aInterruptContext[ m_ulICHead ].AISTAT, COLOR_UNDERLINE ); DC(' '); DX32( m_aInterruptContext[ m_ulICHead ].MISTAT, COLOR_UNDERLINE ); DC(' ');
  //cmn_err((CE_WARN,"Adapter [%ld] AISTAT [%08lx] MISTAT [%08lx]\n", m_ulAdapterNumber, m_aInterruptContext[ m_ulICHead ].AISTAT, m_aInterruptContext[ m_ulICHead ].MISTAT );

  // CS8420 interrupt flag
  if (m_aInterruptContext[m_ulICHead].MISTAT & REG_MISTAT_RX8420)
    {
      if (m_bHasCS8420)
	{
	  m_CS8420.Service ();	// Cannot call driver
	}

      // re-read the MISTAT register and OR it with the previous copy.  This clears any extra interrupts from the CS8420
      if (ulMISTAT)
	m_aInterruptContext[m_ulICHead].MISTAT |= ulMISTAT;
      else
	m_aInterruptContext[m_ulICHead].MISTAT |=
	  READ_REGISTER_ULONG (&m_pRegisters->MISTAT);

      CLR (m_aInterruptContext[m_ulICHead].MISTAT, REG_MISTAT_RX8420);
      // signal that service *may* not be required
      // We check below to see if MISTAT or AISTAT still have pending interrupts
      // and if they do, we service them.  If not, then this status is used.
      usStatus = HSTATUS_SERVICE_NOT_REQUIRED;
    }

  // Option port status received interrupt
  if (m_aInterruptContext[m_ulICHead].MISTAT & REG_MISTAT_OPSTATIF)
    {
      CLR (m_aInterruptContext[m_ulICHead].MISTAT, REG_MISTAT_OPSTATIF);
      usStatus = m_LStream.Service ();
      switch (usStatus)
	{
	case HSTATUS_MIDI1_SERVICE_REQUIRED:
	  SET (m_aInterruptContext[m_ulICHead].MISTAT, REG_MISTAT_MIDI1);
	  usStatus = HSTATUS_OK;
	  break;
	case HSTATUS_MIDI2_SERVICE_REQUIRED:
	  SET (m_aInterruptContext[m_ulICHead].MISTAT, REG_MISTAT_MIDI2);
	  usStatus = HSTATUS_OK;
	  break;
	default:
	  usStatus = HSTATUS_SERVICE_NOT_REQUIRED;
	  break;
	}
    }

  if (!m_aInterruptContext[m_ulICHead].MISTAT
      && !m_aInterruptContext[m_ulICHead].AISTAT)
    {
      if (usStatus == HSTATUS_OK)
	{
	  //cmn_err((CE_WARN,"CHalAdapter::SaveInterruptContext - No Service Required!\n");
	  DB ('N', COLOR_BOLD);
	  usStatus = HSTATUS_INVALID_MODE;
	}
    }
  else
    {
      m_ulICHead++;
      m_ulICHead &= (MAX_PENDING_INTERRUPTS - 1);
      //m_lNumPendingInterrupts++;
      //DB('+',COLOR_REVERSE);
      usStatus = HSTATUS_OK;
    }

  //m_lEntryCount--;
  return (usStatus);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalAdapter::Service (BOOLEAN bPolled)
// Called at interrupt time
/////////////////////////////////////////////////////////////////////////////
{
  PLYNXTWOINTERRUPTCONTEXT pInterrupt;
  ULONG ulDevice;
  USHORT usStatus;

  if (!m_bOpen)
    {
      DB ('!', COLOR_BOLD_U);
      cmn_err (CE_WARN, "Service called when adapter is not open!\n");
      return (HSTATUS_ADAPTER_NOT_OPEN);
    }

  //DB('s',COLOR_UNDERLINE);
  //cmn_err((CE_WARN,"CHalAdapter::Service\n");

  if (bPolled)
    {
      usStatus = SaveInterruptContext ();
      if (usStatus)
	return (usStatus);
    }

  LONG lNumberOfPendingInterrupts = (LONG) m_ulICHead - (LONG) m_ulICTail;
  if (lNumberOfPendingInterrupts < 0)
    lNumberOfPendingInterrupts += MAX_PENDING_INTERRUPTS;

  //DX8((BYTE)lNumberOfPendingInterrupts, COLOR_REVERSE);

  // fetch an interrupt context off the circular buffer
  if (lNumberOfPendingInterrupts < 1)
    {
      DB ('I', COLOR_BOLD);
      //cmn_err((CE_WARN,"Service - No Interrupt!\n");
      return (HSTATUS_INVALID_MODE);
    }

  pInterrupt = &m_aInterruptContext[m_ulICTail++];
  m_ulICTail &= (MAX_PENDING_INTERRUPTS - 1);

  //DX32( pInterrupt->AISTAT, COLOR_UNDERLINE );

  if (m_bHasLTC)
    {
      // LTC receive buffer A
      if (pInterrupt->MISTAT & REG_MISTAT_LRXAIF)
	{
	  m_TimecodeRX.Service (TC_BUFFERA);
	}
      // LTC receive buffer B
      if (pInterrupt->MISTAT & REG_MISTAT_LRXBIF)
	{
	  m_TimecodeRX.Service (TC_BUFFERB);
	}
      // LTC transmit buffer A
      if (pInterrupt->MISTAT & REG_MISTAT_LTXAIF)
	{
	  m_TimecodeTX.Service (TC_BUFFERA);
	}
      // LTC transmit buffer B
      if (pInterrupt->MISTAT & REG_MISTAT_LTXBIF)
	{
	  m_TimecodeTX.Service (TC_BUFFERB);
	}
      // MTC Quarter Frame from LTC Receiver
      if (pInterrupt->MISTAT & REG_MISTAT_LRXQFIF)
	{
	  m_MIDIDevice[MIDI_RECORD0_DEVICE].Service (kMIDIReasonLTCQFM);
	}
    }
  // Does the Phantom MIDI port need service?
  if (pInterrupt->MISTAT & REG_MISTAT_MIDI1)
    {
      m_MIDIDevice[MIDI_RECORD0_DEVICE].Service (kMIDIReasonADATQFM);
    }
  // Does the Phantom MIDI port need service?
  if (pInterrupt->MISTAT & REG_MISTAT_MIDI2)
    {
      m_MIDIDevice[MIDI_RECORD0_DEVICE].Service (kMIDIReasonADATMIDI);
    }

  // scan each wave device for an interrupt
  if (pInterrupt->AISTAT)
    {
#ifdef INTERRUPT_TIMING
      //DPET();
#endif
      // scan each wave device for an interrupt
      for (ulDevice = 0; ulDevice < m_ulNumWaveDevices; ulDevice++)
	{
	  if (m_bIsAES16e)
	    {
	      if (m_bIn32ChannelMode)
		{
		  // if in 32-channel mode, device numbers are in order
		  if (pInterrupt->AISTAT & (1 << ulDevice))
		    {
		      //DX8((BYTE)ulDevice,COLOR_NORMAL);
		      m_WaveDevice[ulDevice].Service (TRUE);
		    }
		}
	      else
		{
		  ULONG ulDeviceOffset = 0;

		  // in 16-channel mode device numbers go 0..7, 16..31
		  if (ulDevice >= m_ulNumWaveRecordDevices)	// if this is a play device, do the offset
		    ulDeviceOffset = 8;

		  // check for the device interrupt flag (only DMA is supported on AES16e)
		  if (pInterrupt->AISTAT & (1 << (ulDevice + ulDeviceOffset)))
		    {
		      //DX8((BYTE)ulDevice,COLOR_NORMAL);
		      m_WaveDevice[ulDevice].Service (TRUE);
		    }
		}
	    }
	  else
	    {
	      // check for the limit/overrun buffer interrupt flag
	      if (pInterrupt->AISTAT & (1 << ulDevice))
		{
		  if (m_WaveDevice[ulDevice].IsDMAActive ())
		    GetWaveDMADevice (ulDevice)->Service ();
		  else
		    GetWaveDevice (ulDevice)->Service ();
		}

	      // check for the DMA interrupt flag
	      if ((pInterrupt->AISTAT >> m_ulNumWaveDevices) &
		  (1 << ulDevice))
		{
		  m_WaveDevice[ulDevice].Service (TRUE);
		}
	    }
	}
    }

  // we are done with this interrupt context
  pInterrupt->MISTAT = 0;
  pInterrupt->AISTAT = 0;
  //m_lNumPendingInterrupts--;
  //DB('-',COLOR_REVERSE);

  return (HSTATUS_OK);
}
