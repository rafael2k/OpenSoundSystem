// HalWaveDevice.h: interface for the HalWaveDevice class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_HALWAVEDEVICE_H__6F31BA60_956B_11D4_BA01_005004612939__INCLUDED_)
#define AFX_HALWAVEDEVICE_H__6F31BA60_956B_11D4_BA01_005004612939__INCLUDED_

#include "Hal.h"
#include "HalDevice.h"
#include "HalRegister.h"
#include "HalEnv.h"		// Added by ClassView

enum
{
  ASIOMODE_OFF = 0,
  ASIOMODE_PLAYRECORD,
  ASIOMODE_PLAYONLY
};

class CHalWaveDevice:public CHalDevice
{
public:
  CHalWaveDevice ()
  {
  }
   ~CHalWaveDevice ()
  {
  }

  USHORT Open (PHALADAPTER pHalAdapter, ULONG ulDeviceNumber);
  USHORT Close ();
  USHORT Start ();
  USHORT Stop ();
  USHORT Service ();

  BOOLEAN IsRecord ()
  {
    return (m_bIsRecord);
  };

  USHORT SetFormat (USHORT wFormatTag, LONG lChannels, LONG lSampleRate,
		    LONG lBitsPerSample, LONG lBlockAlign);
  USHORT ValidateFormat (USHORT wFormatTag, LONG lChannels, LONG lSampleRate,
			 LONG lBitsPerSample, LONG lBlockAlign);
  ULONG GetSampleFormat ()
  {
    return ((m_lBitsPerSample | (m_lNumChannels << 8)));
  }
  LONG GetBytesPerBlock ()
  {
    return (m_lBytesPerBlock);
  };
  LONG GetNumChannels ()
  {
    return (m_lNumChannels);
  };
  LONG GetSampleRate ()
  {
    return (m_lSampleRate);
  };

  LONG GetHWIndex (BOOLEAN bFromHardware = TRUE);
  USHORT EnableSyncStart ();
  USHORT DisableSyncStart ();

  USHORT GetTransferSize (PULONG pulTransferSize,
			  PULONG pulCircularBufferSize);
  USHORT TransferAudio (PVOID pBuffer, ULONG ulBufferSize,
			PULONG pulBytesTransfered, LONG lPCIndex = -1L);
  USHORT TransferAudio (PVOID pLeft, PVOID pRight, ULONG ulBufferSize,
			LONG lPCIndex = -1L);
  USHORT TransferComplete (LONG lBytesProcessed);
  USHORT SetInterruptSamples (ULONG ulSampleCount);

  ULONG GetBytesTransferred ()
  {
    return (m_ulBytesTransferred);
  }
  ULONG GetSamplesTransferred ()
  {
    return (m_ulSamplesTransferred);
  }
  ULONG GetOverrunCount ();

  USHORT ZeroPosition (void);
  USHORT GetSamplePosition (PULONG pulSamplePosition);
  ULONGLONG GetBytePosition (void);
  //ULONGLONG     GetSamplesPlayed( void );

  PHALREGISTER GetStreamControl ()
  {
    return (&m_RegStreamControl);
  }
  PHALREGISTER GetStreamStatus ()
  {
    return (&m_RegStreamStatus);
  }
#ifdef DEBUG
  VOID DebugPrintStatus (VOID);
#endif

  VOID IncreaseOverrunCount (ULONG ulCount);	// DAH Added 11/14/2011

  // This must be public so HalDMA can access it
  BOOLEAN m_bSyncStartEnabled;

protected:
  USHORT PrepareForNonPCM (void);
  USHORT PrepareForPCM (void);

  // allow these member variables to be accessed by child classes (namely CHalWaveDMADevice)
  BOOLEAN m_bIsRecord;
  BOOLEAN m_bIsAES16e;

  USHORT m_wFormatTag;
  LONG m_lNumChannels;
  LONG m_lSampleRate;
  LONG m_lBitsPerSample;
  LONG m_lBytesPerBlock;

  ULONG m_ulOverrunCount;	// can be changed in the interrupt service routine

  CHalRegister m_RegStreamControl;	// Stream Control Register
  CHalRegister m_RegStreamStatus;	// Stream Status

  //PULONG                        m_pulPosLoReg;
  //PULONG                        m_pulPosHiReg;

  PHALMIXER m_pHalMixer;
  USHORT m_usDstLine;
  USHORT m_usSrcLine;

private:
  // these member variables are completely private to this class
  PULONG m_pAudioBuffer;
  LONG m_lHWIndex;		// Shadow of HW Index
  LONG m_lPCIndex;		// Shadow of PC Index
  ULONG m_ulInterruptSamples;
  ULONG m_ulBytesTransferred;
  ULONG m_ulSamplesTransferred;

  // for GetBytePosition
  ULONGLONG m_ullBytePosition;
  ULONG m_ulGBPEntryCount;
  LONG m_lGBPLastHWIndex;
};

#endif // !defined(AFX_HALWAVEDEVICE_H__6F31BA60_956B_11D4_BA01_005004612939__INCLUDED_)
