/****************************************************************************
 HalAdapter.h

 Description:	Interface for the HalAdapter class.

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
#ifndef _HALADAPTER_H
#define _HALADAPTER_H

#include "Hal.h"
#include "HalDMA.h"
#include "HalTimecode.h"
#include "HalSampleClock.h"
#include "Hal8420.h"
#include "Hal4114.h"
#include "HalAES16e.h"
#include "HalWaveDMADevice.h"
#include "HalMIDIDevice.h"
#include "HalMixer.h"
#include "HalLStream.h"
#include "HalAurora.h"

#include "LynxTWO.h"

enum
{
  kMIDIReasonLTCQFM = 0,
  kMIDIReasonADATQFM,
  kMIDIReasonADATMIDI
};

// for MTC Source
enum
{
  MIXVAL_MTCSOURCE_LTCRX = 0,
  MIXVAL_MTCSOURCE_LSTREAM1_ADAT_SYNCIN,
  MIXVAL_MTCSOURCE_LSTREAM2_ADAT_SYNCIN,
  MIXVAL_MTCSOURCE_COUNT
};

class CHalAdapter
{
public:
  oss_device_t * m_osdev;	//OSS Device handle
  CHalAdapter (PHALDRIVERINFO pDrvInfo, ULONG ulAdapterNumber = 0);	// constructor
  ~CHalAdapter ();		// destructor

  USHORT Find ();		// finds the next available adapter
  USHORT Open (BOOLEAN bResume = FALSE);	// opens the adapter for use
  USHORT Close (BOOLEAN bSuspend = FALSE);	// close the adapter

  BOOLEAN IsOpen ()
  {
    return (m_bOpen);
  }

  void EnableInterrupts (void);
  void DisableInterrupts (void);

  // called at interrupt time to get the current device interrupt status
  USHORT SaveInterruptContext (ULONG ulAISTAT = 0, ULONG ulMISTAT = 0);
  // called at interrupt time to service the devices requesting service
  USHORT Service (BOOLEAN bPolled = FALSE);

  USHORT SetConverterSpeed (LONG lSampleRate);

  USHORT SetTrim (USHORT usControl, ULONG ulValue);
  USHORT GetTrim (USHORT usControl, PULONG pulValue);

  USHORT SetADHPF (USHORT usControl, BOOLEAN bEnable);
  BOOLEAN GetADHPF (USHORT usControl);

  USHORT SetDADeEmphasis (BOOLEAN bEnable);
  BOOLEAN GetDADeEmphasis ()
  {
    return (m_bDACDeEmphasis);
  }

  USHORT SetAutoRecalibrate (BOOLEAN bEnable);
  BOOLEAN GetAutoRecalibrate ()
  {
    return (m_bAutoRecalibrate);
  }
  USHORT CalibrateConverters ();
  USHORT CalibrateADConverters ();
  USHORT CalibrateDAConverters ();

  USHORT GetFrequencyCounter (USHORT usRegister, PULONG pulFrequency);
  BOOLEAN GetNTSCPAL (void);
  USHORT NormalizeFrequency (ULONG ulFrequency, PLONG plRate,
			     PLONG plReference = NULL);

  USHORT SetMultiChannelRecord (LONG lNumChannels);
  USHORT ClearMultiChannelRecord (void);
  USHORT SetMultiChannelPlay (LONG lNumChannels);
  USHORT ClearMultiChannelPlay (void);

  USHORT Update (void);

  PLYNXTWOAUDIOBUFFERS GetAudioBuffers ()
  {
    return (m_pAudioBuffers);
  }
  PLYNXTWOREGISTERS GetRegisters ()
  {
    return (m_pRegisters);
  }
  PDMABUFFERLIST GetDMABufferList ()
  {
    return (m_pDMA_VAddr);
  }

  PVOID GetBAR0VirtualAddress ()
  {
    return (m_PCIConfig.Base[PCI_REGISTERS_INDEX].pvAddress);
  }
  PVOID GetBAR0PhysicalAddress ()
  {
    return (m_PCIConfig.Base[PCI_REGISTERS_INDEX].pvPhysicalAddress);
  }
  ULONG GetBAR0Size ()
  {
    return (m_PCIConfig.Base[PCI_REGISTERS_INDEX].ulSize);
  }

  PAURORABLK GetAuroraBlk ()
  {
    return (&m_pRegisters->AURORABlock);
  }
  USHORT GetBusNumber ()
  {
    return (m_PCIConfig.usBusNumber);
  };
  USHORT GetDeviceFunction ()
  {
    return (m_PCIConfig.usDeviceFunction);
  };

#ifdef MACINTOSH
  RegEntryID GetEntryID ()
  {
    return (m_PCIConfig.EntryID);
  };
  char *GetSlotName ()
  {
    return (m_PCIConfig.szSlotName);
  };
#endif

#if OSX && TARGET_CPU_PPC
  static ULONG ReadRegisterULONG (PHALADAPTER pHalAdapter, PULONG pulAddress);
  static VOID WriteRegisterULONG (PHALADAPTER pHalAdapter, PULONG pulAddress,
				  ULONG ulValue);

  BOOLEAN NeedsSwap (VOID)
  {
    return (m_bSwap);
  }
#endif

  USHORT GetDeviceID ()
  {
    return (m_usDeviceID);
  }
  USHORT GetPCBRev ()
  {
    return (m_usPCBRev);
  }
  USHORT GetFirmwareRev ()
  {
    return (m_usFWREVID);
  }				// DAH Changed on 10/22/2009
  USHORT GetFirmwareDate ()
  {
    return (m_usFirmwareDate);
  }
  USHORT GetMinSoftwareAPIRev ()
  {
    return (m_usMinSoftwareAPIRev);
  }
  ULONG GetSerialNumber ()
  {
    return (m_ulSerialNumber);
  }

  USHORT SetDitherType (PHALREGISTER pRMix0Control, USHORT usDitherType);
  USHORT GetDitherType ()
  {
    return (m_usDitherType);
  }

  PHAL8420 Get8420 ()
  {
    if (m_bHasCS8420)
      return (&m_CS8420);
    else
      return (NULL);
  }
  PHAL4114 Get4114 ()
  {
    if (m_bHasAK4114)
      return (&m_AK4114);
    else
      return (NULL);
  }
  PHALAES16E GetAES16eDIO ()
  {
    if (m_bHasAES16eDIO)
      return (&m_AES16eDIO);
    else
      return (NULL);
  }
  PHALAURORA GetAurora ()
  {
    if (m_bHasAurora)
      return (&m_Aurora);
    else
      return (NULL);
  }
  PHALSAMPLECLOCK GetSampleClock ()
  {
    return (&m_SampleClock);
  }
  PHALMIXER GetMixer ()
  {
    return (&m_Mixer);
  }
  PHALTIMECODE GetTCRx ()
  {
    if (m_bHasLTC)
      return (&m_TimecodeRX);
    else
      return (NULL);
  }
  PHALTIMECODE GetTCTx ()
  {
    if (m_bHasLTC)
      return (&m_TimecodeTX);
    else
      return (NULL);
  }
  PHALREGISTER GetRegLTCControl ()
  {
    return (&m_RegLTCControl);
  }				// only called from HalTimecode.cpp
  PHALLSTREAM GetLStream ()
  {
    return (&m_LStream);
  }

  ULONG GetAdapterNumber ()
  {
    return (m_ulAdapterNumber);
  }

  ULONG GetNumWaveDevices ()
  {
    return (m_ulNumWaveDevices);
  }
  ULONG GetNumWaveInDevices ()
  {
    return (m_ulNumWaveRecordDevices);
  }
  ULONG GetNumWaveOutDevices ()
  {
    return (m_ulNumWavePlayDevices);
  }
  ULONG GetNumActiveWaveDevices (void);

  // Because of LStream ports, all devices get the virtual MIDI ports
  ULONG GetNumMIDIDevices ()
  {
    return (NUM_MIDI_DEVICES);
  }
  ULONG GetNumMIDIInDevices ()
  {
    return (NUM_MIDI_RECORD_DEVICES);
  }
  ULONG GetNumMIDIOutDevices ()
  {
    return (NUM_MIDI_PLAY_DEVICES);
  }

  // SyncStart Related
  USHORT SetSyncStartState (BOOLEAN bEnable);
  BOOLEAN GetSyncStartState ()
  {
    return (m_bSyncStart);
  }
  USHORT SyncStartPrime ();
  USHORT SyncStartGo ();
  USHORT SyncStartReady (ULONG ulDeviceNumber, ULONG ulStreamControl);
  USHORT AddToStartGroup (ULONG ulDeviceNumber);
  USHORT RemoveFromStartGroup (ULONG ulDeviceNumber);
  USHORT EnableLStreamSyncStart (BOOLEAN bEnable = TRUE);
  BOOLEAN GetSyncStartLStreamEnable ()
  {
    return (m_bLStreamSyncStart);
  }

  USHORT GetMRMEnable (PULONG pulValue);
  USHORT SetMRMEnable (ULONG ulValue);

  USHORT SetInterruptSamples (ULONG ulInterruptSamples = 0);
  ULONG GetInterruptSamples (VOID)
  {
    return (WAVE_CIRCULAR_BUFFER_SIZE - m_ulInterruptSamples);
  }

#ifdef DEBUG
  VOID ChangeDebugBit0 (BOOLEAN bSet);
  VOID ChangeDebugBit1 (BOOLEAN bSet);
#endif

  BOOLEAN IsWaveDeviceRecord (ULONG ulDeviceNumber);
  PHALWAVEDEVICE GetWaveDevice (ULONG ulDeviceNumber);
  PHALWAVEDMADEVICE GetWaveDMADevice (ULONG ulDeviceNumber);
  PHALWAVEDMADEVICE GetWaveDMARecordDevice (ULONG ulDeviceNumber);
  PHALWAVEDMADEVICE GetWaveDMAPlayDevice (ULONG ulDeviceNumber);
  //PHALWAVEDEVICE        GetWaveInDevice( ULONG ulDeviceNumber );
  //PHALWAVEDEVICE        GetWaveOutDevice( ULONG ulDeviceNumber );

  BOOLEAN IsMIDIDeviceRecord (ULONG ulDeviceNumber);
  PHALMIDIDEVICE GetMIDIDevice (ULONG ulDeviceNumber);
  PHALMIDIDEVICE GetMIDIInDevice (ULONG ulDeviceNumber);
  PHALMIDIDEVICE GetMIDIOutDevice (ULONG ulDeviceNumber);

  USHORT SetMTCSource (ULONG ulMTCSource);
  USHORT GetMTCSource (PULONG pulMTCSource);

  USHORT IORead (BYTE ucAddress, PBYTE pucData, BOOLEAN bReadShadowOnly =
		 FALSE);
  USHORT IOWrite (BYTE ucAddress, BYTE ucData, BYTE ucMask = 0xFF);

  PHALREGISTER GetPCICTL ()
  {
    return (&m_RegPCICTL);
  }

  // DeviceID / Firmware
  BOOLEAN IsLynxTWO ()
  {
    return (m_bIsLynxTWO);
  }
  BOOLEAN IsAES16 ()
  {
    return (m_bIsAES16);
  }
  BOOLEAN IsAES16e ()
  {
    return (m_bIsAES16e);
  }

  BOOLEAN HasAK5394A ()
  {
    return (m_bHasAK5394A);
  }
  BOOLEAN HasCS8420 ()
  {
    return (m_bHasCS8420);
  }
  BOOLEAN HasAK4114 ()
  {
    return (m_bHasAK4114);
  }
  BOOLEAN HasGlobalSyncStart ()
  {
    return (m_bHasGlobalSyncStart);
  }
  BOOLEAN HasTIVideoPLL ()
  {
    return (m_bHasTIVideoPLL);
  }
  BOOLEAN HasP16 ()
  {
    return (m_bHasP16);
  }
  BOOLEAN HasDualMono ()
  {
    return (m_bHasDualMono);
  }
  BOOLEAN HasLStream11 ()
  {
    return (m_bHasLStream11);
  }
  BOOLEAN HasLRClock ()
  {
    return (m_bHasLRClock);
  }
  BOOLEAN HasLTC ()
  {
    return (m_bHasLTC);
  }
  ULONG GetXtalSpeed ()
  {
    return (m_ulXtalSpeed);
  }
  BOOLEAN HasMultiChannel ()
  {
    return (m_bHasMultiChannel);
  }
  BOOLEAN HasSynchroLock ()
  {
    return (m_bHasSynchroLock);
  }
  BOOLEAN HasWideWireOut ()
  {
    return (m_bHasWideWireOut);
  }
  BOOLEAN HasNewPMix ()
  {
    return (m_bHasNewPMix);
  }
  BOOLEAN HasPMixV3 ()
  {
    return (m_bHasPMixV3);
  }
  BOOLEAN In32ChannelMode ()
  {
    return (m_bIn32ChannelMode);
  }
  BOOLEAN HasAES50 ()
  {
    return (m_bHasAES50);
  }

private:

#ifdef OSX
  PHALADAPTER m_pHalAdapter;
  BOOLEAN m_bSwap;
#endif

  PLYNXTWOREGISTERS m_pRegisters;	// Ptr to BAR0
  PLYNXTWOAUDIOBUFFERS m_pAudioBuffers;	// Ptr to BAR1
  BYTE m_aIORegisters[NUM_IO_REGISTERS];	// Shadow registers

  BOOLEAN m_bOpen;
  HALDRIVERINFO m_HalDriverInfo;
  PCI_CONFIGURATION m_PCIConfig;
  ULONG m_ulAdapterNumber;

  ULONG m_ulNumWaveDevices;
  ULONG m_ulNumWaveRecordDevices;
  ULONG m_ulNumWavePlayDevices;

  // PDBLOCK shadows
  USHORT m_usDeviceID;
  USHORT m_usPCBRev;
  USHORT m_usFWREVID;
  USHORT m_usFirmwareDate;
  USHORT m_usMinSoftwareAPIRev;
  ULONG m_ulSerialNumber;

  CHalWaveDMADevice m_WaveDevice[MAX_NUM_WAVE_DEVICES];
  CHalMIDIDevice m_MIDIDevice[NUM_MIDI_DEVICES];
  CHalMixer m_Mixer;
  CHal8420 m_CS8420;
  CHal4114 m_AK4114;
  CHalAES16e m_AES16eDIO;
  CHalAurora m_Aurora;
  CHalSampleClock m_SampleClock;
  CHalTimecode m_TimecodeRX;
  CHalTimecode m_TimecodeTX;
  CHalLStream m_LStream;

  CHalRegister m_RegPCICTL;
  CHalRegister m_RegSTRMCTL;
  CHalRegister m_RegLTCControl;

  PVOID m_pDMA_BufferList;	// The DMA Buffer List Object (from the Driver) 
  PDMABUFFERLIST m_pDMA_VAddr;	// Virtual Address of DMA Buffer List
  ULONG m_pDMA_PAddr;		// Physical Address of DMA Buffer List

  LYNXTWOINTERRUPTCONEXT m_aInterruptContext[MAX_PENDING_INTERRUPTS];
  ULONG m_ulICHead;
  ULONG m_ulICTail;

  volatile LONG m_lIOEntryCount;

  ULONG m_ulInterruptSamples;

  USHORT m_usDitherType;
  BOOLEAN m_bSyncStart;		// adapter global sync start enable
  ULONG m_ulSyncGroup;
  ULONG m_ulSyncReady;
  ULONG m_aulStreamControl[MAX_NUM_WAVE_DEVICES];

  LONG m_lTrimAI12;		// these get init'ed from the HalMixer code
  LONG m_lTrimAI34;
  LONG m_lTrimAI56;
  LONG m_lTrimAO12;
  LONG m_lTrimAO34;
  LONG m_lTrimAO56;

  BOOLEAN m_bAIn12HPFEnable;
  BOOLEAN m_bAIn34HPFEnable;
  BOOLEAN m_bAIn56HPFEnable;

  BOOLEAN m_bIsLynxTWO;
  BOOLEAN m_bIsAES16;
  BOOLEAN m_bIsAES16e;

  BOOLEAN m_bHasAK5394A;
  BOOLEAN m_bHasCS8420;
  BOOLEAN m_bHasAK4114;
  BOOLEAN m_bHasAES16eDIO;
  BOOLEAN m_bHasAurora;
  BOOLEAN m_bHasGlobalSyncStart;
  BOOLEAN m_bHasTIVideoPLL;
  BOOLEAN m_bHasP16;
  BOOLEAN m_bHasDualMono;
  BOOLEAN m_bHasLStream11;
  BOOLEAN m_bHasLRClock;
  BOOLEAN m_bHasLTC;
  ULONG m_ulXtalSpeed;
  BOOLEAN m_bHasMultiChannel;
  BOOLEAN m_bHasSynchroLock;
  BOOLEAN m_bHasWideWireOut;
  BOOLEAN m_bHasNewPMix;
  BOOLEAN m_bHasPMixV3;
  BOOLEAN m_bIn32ChannelMode;
  BOOLEAN m_bHasMRMEnable;
  BOOLEAN m_bHasSRAM;
  BOOLEAN m_bHasPLLCTLE;
  BOOLEAN m_bHasAES50;

  BOOLEAN m_bAutoRecalibrate;
  BOOLEAN m_bDACDeEmphasis;
  ULONG m_ulMTCSource;
  BOOLEAN m_bLStreamSyncStart;
};

#endif // _HALADAPTER_H
