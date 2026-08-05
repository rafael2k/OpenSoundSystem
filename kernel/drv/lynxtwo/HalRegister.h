// HalRegister.h: interface for the CHalRegister class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _HALREGISTER_H
#define _HALREGISTER_H

#include "Hal.h"

class CHalAdapter;
typedef CHalAdapter *PHALADAPTER;

enum
{
  REG_READONLY = 1,
  REG_WRITEONLY = 2,
  REG_READWRITE = 3
};

class CHalRegister
{
public:
  CHalRegister ();
  CHalRegister (PHALADAPTER pHalAdapter, PULONG pAddress, ULONG ulType =
		REG_READWRITE, ULONG ulValue = 0);
  virtual ~ CHalRegister ();
  // overload of the assignment operator
  void operator= (ULONG ulValue)
  {
    Write (ulValue);
  }
  operator    ULONG ()
  {
    return Read ();
  }

  void Init (PHALADAPTER pHalAdapter, PULONG pAddress, ULONG ulType =
	     REG_READWRITE, ULONG ulValue = 0);
  ULONG Read ();
  void Write (ULONG ulValue);
  void Write (ULONG ulValue, ULONG ulMask);
  void BitSet (ULONG ulBitMask, BOOLEAN bValue);
#ifdef DEBUG
  PULONG GetAddress (void)
  {
    return (m_pAddress);
  }
#endif

private:
  PHALADAPTER m_pHalAdapter;
  PULONG m_pAddress;
  ULONG m_ulType;
  ULONG m_ulValue;
};

#endif // _HALREGISTER_H
