// HalWaveDMADevice.h: interface for the HalWaveDMADevice class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _HALWAVEDMADEVICE_H
#define _HALWAVEDMADEVICE_H

#include "Hal.h"
#include "HalDMA.h"

#include "HalWaveDevice.h"

class CHalWaveDMADevice:public CHalWaveDevice
{
public:
  CHalWaveDMADevice ()
  {
  }
   ~CHalWaveDMADevice ()
  {
  }

  USHORT Open (PHALADAPTER pHalAdapter, ULONG ulDeviceNumber);
  USHORT Close ();

  USHORT Start ();
  USHORT Stop ();

  PHALDMA GetDMA ()
  {
    return (&m_DMA);
  }

  USHORT Service (BOOLEAN bDMABufferComplete = FALSE);

  ULONG GetSampleFormat ()
  {
    return (m_lBitsPerSample | (m_lNumChannels << 8) |
	    ((m_wFormatTag ==
	      WAVE_FORMAT_WMA_SPDIF) << 27) | ((m_wFormatTag ==
						WAVE_FORMAT_DOLBY_AC3_SPDIF)
					       << 28) | ((m_wFormatTag ==
							  WAVE_FORMAT_PCM) <<
							 29) | (m_bIsDMAActive
								<< 30) |
	    (m_bDualMono << 31));
  }

  void SetAutoFree (BOOLEAN bAutoFree)
  {
    m_bAutoFree = bAutoFree;
  }
  void SetOverrunIE (BOOLEAN bOverrunIE)
  {
    m_bOverrunIE = bOverrunIE;
  }
  void SetLimitIE (BOOLEAN bLimitIE)
  {
    m_bLimitIE = bLimitIE;
  }
  void SetDMASingle (BOOLEAN bDMASingle)
  {
    m_bDMASingle = bDMASingle;
  }
  void SetDualMono (BOOLEAN bDualMono)
  {
    m_bDualMono = bDualMono;
  }
  void SetOverrunIgnore (BOOLEAN bOverIgnore)
  {
    m_bOverIgnore = bOverIgnore;
  }

  BOOLEAN IsDMAActive ()
  {
    return (m_bIsDMAActive);
  }
  BOOLEAN GetOverrunIE ()
  {
    return (m_bOverrunIE);
  }
  BOOLEAN GetLimitIE ()
  {
    return (m_bLimitIE);
  }
  BOOLEAN GetDMASingle ()
  {
    return (m_bDMASingle);
  }
  BOOLEAN GetDualMono ()
  {
    return (m_bDualMono);
  }
  BOOLEAN GetOverrunIgnore ()
  {
    return (m_bOverIgnore);
  }

private:
  CHalDMA m_DMA;

  BOOLEAN m_bAutoFree;
  BOOLEAN m_bIsDMAActive;
  BOOLEAN m_bOverrunIE;
  BOOLEAN m_bLimitIE;
  BOOLEAN m_bDMASingle;
  BOOLEAN m_bDualMono;
  BOOLEAN m_bOverIgnore;
};

#endif // _HALWAVEDMADEVICE_H
