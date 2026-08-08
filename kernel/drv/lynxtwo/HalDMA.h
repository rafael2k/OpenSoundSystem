/****************************************************************************
 HalDMA.h

 Description: Interface for the HalDMA class.

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
#ifndef _HALDMA_H
#define _HALDMA_H

#include "Hal.h"
#include "LynxTWO.h"

class CHalDMA
{
public:
  CHalDMA ()
  {
  }				// Constructor
   ~CHalDMA ()
  {
  }				// Destructor

  USHORT Open (PHALADAPTER pHalAdapter, ULONG ulDeviceNumber);
  USHORT Close ();

  USHORT Start ();
  USHORT Stop ();

  USHORT AddEntry (PVOID pBuffer, ULONG ulSize, BOOLEAN bInterrupt);
  USHORT FreeEntry ();
  ULONG GetHostBufferIndex ()
  {
    return (m_ulPCBufferIndex);
  }
  ULONG GetNumberOfEntries ()
  {
    return (m_lEntriesInList);
  }
  ULONG GetEntriesInHardware ();
  ULONG GetDMABufferIndex ();
  ULONG GetBytesRemaining (ULONG ulIndex);
  ULONG GetPreloadTiming ()
  {
    return (MAKEULONG (m_ulLastPreloadTime, m_ulLastPreloadSize));
  }
  BOOLEAN IsDMAStarved ();

protected:			// DAH 09/27/2005 moved these functions into protected
  friend class CHalAdapter;	// DAH 09/27/2005 Allow HalAdapter to call these functions...
  friend class CHalWaveDMADevice;	// DAH 04/17/2006 Allow CHalWaveDMADevice to call these functions...

  USHORT StartPlayPreload ();	// Called by CHalWaveDMADevice
  BOOLEAN IsPreloadComplete ();	// Called by CHalAdapter
  USHORT WaitForPreloadToComplete ();	// Called by CHalAdapter

private:
  PHALADAPTER m_pHalAdapter;
  ULONG m_ulPCBufferIndex;
  volatile LONG m_lEntriesInList;
  PDMABUFFERBLOCK m_pBufferBlock;
  ULONG m_ulDeviceNumber;
  PHALWAVEDMADEVICE m_pWaveDMADevice;
  PHALREGISTER m_pRegStreamControl;
  PHALREGISTER m_pRegStreamStatus;
  ULONG m_ulLastBufferIndex;
  ULONG m_ulPreloadSize;
  ULONGLONG m_ullPreloadStartTime;

  ULONG m_ulLastPreloadSize;
  ULONG m_ulLastPreloadTime;

};

#endif // _HALDMA_H
