// HalRecordMix.h: interface for the CHalRecordMix class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _HALRECORDMIX_H
#define _HALRECORDMIX_H

#include "Hal.h"

class CHalRecordMix
{
public:
  CHalRecordMix ()
  {
  }
   ~CHalRecordMix ()
  {
  }

  USHORT Open (PHALADAPTER pHalAdapter, PULONG pRecordMixCtl,
	       PULONG pRecordMixStatus);

  ULONG GetLevel ();
  void ResetLevel ();
  USHORT GetSource ()
  {
    return (m_asSource);
  }
  void SetSource (USHORT usSource);
  BOOLEAN GetMute ()
  {
    return (m_bMute);
  }
  void SetMute (BOOLEAN bMute);
  USHORT GetDitherDepth ()
  {
    return (m_usDitherControl);
  }
  void SetDitherDepth (USHORT usDitherDepth);
  BOOLEAN GetDither ()
  {
    return (m_bDither);
  }
  void SetDither (BOOLEAN bDither);

  PHALREGISTER GetMixControl ()
  {
    return (&m_RegMixControl);
  }

private:
  PHALADAPTER m_pHalAdapter;
  CHalRegister m_RegMixControl;
  CHalRegister m_RegMixStatus;
  SHORT m_asSource;
  BOOLEAN m_bMute;
  BOOLEAN m_bDither;
  USHORT m_usDitherDepth;	// 8, 16, 20 or 24 (off)
  USHORT m_usDitherControl;	// MIXVAL_DITHERDEPTH_XX
  USHORT m_usDitherType;

  BOOLEAN m_bHasPMixV3;
};

#endif
