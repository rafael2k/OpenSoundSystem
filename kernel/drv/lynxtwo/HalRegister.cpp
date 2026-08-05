/****************************************************************************
 HalRegister.cpp

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
****************************************************************************/

#include <StdAfx.h>
#include "HalRegister.h"
#include "HalAdapter.h"

/////////////////////////////////////////////////////////////////////////////
CHalRegister::CHalRegister ()
/////////////////////////////////////////////////////////////////////////////
{
  m_pHalAdapter = NULL;
  m_pAddress = NULL;
  m_ulValue = 0;
}

/////////////////////////////////////////////////////////////////////////////
CHalRegister::CHalRegister (PHALADAPTER pHalAdapter, PULONG pAddress,
			    ULONG ulType, ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  Init (pHalAdapter, pAddress, ulType, ulValue);
}

/////////////////////////////////////////////////////////////////////////////
CHalRegister::~CHalRegister ()
/////////////////////////////////////////////////////////////////////////////
{
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRegister::Init (PHALADAPTER pHalAdapter, PULONG pAddress, ULONG ulType,
		    ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
  m_pHalAdapter = pHalAdapter;
  m_pAddress = pAddress;
  m_ulType = ulType;
  m_ulValue = ulValue;

  // DAH 06/26/2002 These next two lines were commented out.
  //if( m_ulType != REG_READONLY )
  //      WRITE_REGISTER_ULONG( m_pAddress, m_ulValue );
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalRegister::Read ()
/////////////////////////////////////////////////////////////////////////////
{
#ifdef DEBUG
  if (!m_pAddress)
    {
      cmn_err (CE_WARN, "CHalRegister::Read called with m_pAddress NULL!\n");
      return (0);
    }
#endif
  if (m_ulType != REG_WRITEONLY)
    m_ulValue = READ_REGISTER_ULONG (m_pAddress);

  return (m_ulValue);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRegister::Write (ULONG ulValue)
/////////////////////////////////////////////////////////////////////////////
{
#ifdef DEBUG
  if (!m_pAddress)
    {
      cmn_err (CE_WARN, "CHalRegister::Read called with m_pAddress NULL!\n");
      return;
    }
#endif
  m_ulValue = ulValue;

  if (m_ulType != REG_READONLY)
    WRITE_REGISTER_ULONG (m_pAddress, m_ulValue);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRegister::Write (ULONG ulValue, ULONG ulMask)
/////////////////////////////////////////////////////////////////////////////
{
  CLR (m_ulValue, ulMask);
  SET (m_ulValue, (ulValue & ulMask));

  Write (m_ulValue);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRegister::BitSet (ULONG ulBitMask, BOOLEAN bValue)
/////////////////////////////////////////////////////////////////////////////
{
  CLR (m_ulValue, ulBitMask);	// clear position(s)

  if (bValue)
    SET (m_ulValue, ulBitMask);	// if SET then set position(s)

  Write (m_ulValue);
}
