/****************************************************************************
 HalDMA.cpp

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
#include "HalAdapter.h"

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalDMA::Open (PHALADAPTER pHalAdapter, ULONG ulDeviceNumber)
// NOTE: CHalWaveDevice::Open has not been run yet so you cannot depend on 
// any varibles from the base class of CHalWaveDMADevice.
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalDMA::Open\n");

  // set defaults values for member variables
  m_pHalAdapter = pHalAdapter;
  m_pWaveDMADevice = m_pHalAdapter->GetWaveDMADevice (ulDeviceNumber);
  m_ulDeviceNumber = ulDeviceNumber;

  // play devices are after record devices so this will work for both
  if (m_pHalAdapter->IsAES16e ())
    {
      ULONG ulDeviceOffset = 0;

      // if we are in 16-channel mode, then we need to offset to get the hardware address correct
      if (!m_pHalAdapter->In32ChannelMode ()
	  && !m_pHalAdapter->IsWaveDeviceRecord (ulDeviceNumber))
	ulDeviceOffset = 8;

      //cmn_err((CE_WARN,"Device %lu Offset %lu\n", ulDeviceNumber, ulDeviceOffset );
      m_pBufferBlock =
	&m_pHalAdapter->GetDMABufferList ()->E.Record[ulDeviceNumber +
						      ulDeviceOffset];
    }
  else
    m_pBufferBlock =
      &m_pHalAdapter->GetDMABufferList ()->L2.Record[ulDeviceNumber];

  m_pRegStreamControl = m_pWaveDMADevice->GetStreamControl ();	// this is OK because this is just a pointer to a static class
  m_pRegStreamStatus = m_pWaveDMADevice->GetStreamStatus ();	// this is OK because this is just a pointer to a static class

  m_ulPCBufferIndex = 0;
  m_lEntriesInList = 0;
  m_ulLastBufferIndex = 0xFFFFFFFF;	// Invalid
  m_ulPreloadSize = 0;
  m_ullPreloadStartTime = 0;
  m_ulLastPreloadSize = 0;
  m_ulLastPreloadTime = 0;

  // make sure the DMA buffer block gets cleared
  RtlZeroMemory (m_pBufferBlock, sizeof (DMABUFFERBLOCK));

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalDMA::Close ()
/////////////////////////////////////////////////////////////////////////////
{
  return (HSTATUS_OK);
}

#ifdef WDM
#define USE_PRELOAD_TIMING	1
#endif

#ifdef USE_PRELOAD_TIMING
// Forward declaration for stupid function call hack
LONGLONG SoundGetTime (VOID);
#endif

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalDMA::StartPlayPreload ()
// DMA Buffer List should have at least one entry already.
/////////////////////////////////////////////////////////////////////////////
{
  if (!m_pWaveDMADevice->IsRecord ())
    {
#ifdef DEBUG
      // make sure there is at least one buffer in the DMA buffer list
      if (!m_lEntriesInList)
	{
	  cmn_err (CE_WARN,
		   "CHalDMA::StartPlayPreload called when NO buffers have been added!\n");
	}
#endif
      // DAH 04/17/2006 made sure multiple calls to StartPlayPreload will only change the RegStreamControl once
      if (!m_ulPreloadSize)
	{
	  m_ulPreloadSize =
	    (m_pBufferBlock->Entry[0].ulControl & DMACONTROL_BUFLEN_MASK);

	  //cmn_err((CE_WARN,"Preload %lu Size %lu\n", m_ulDeviceNumber + 16, m_ulPreloadSize );

	  //cmn_err((CE_WARN,"StartPlayPreload\n");
	  DB ('P', COLOR_UNDERLINE);
	  //TEStart( m_ulDeviceNumber + 16 );

#ifdef USE_PRELOAD_TIMING
	  m_ullPreloadStartTime = SoundGetTime ();	// OK - so this is calling into the driver - which the HAL should never do...
#endif
	  // DAH 08/13/2003 Turned DMASINGLE back on so 1024 buffer size in ASIO will playback both channels.
	  // we just read from this register in Start, so no extra read is needed here to update the shadow register.
	  ULONG ulStreamControl = 0;

	  if (m_pHalAdapter->IsAES16e ())
	    {
	      if (m_pWaveDMADevice->GetDualMono ())	// DAH 03/14/2008 added DMADUALMONO set here
		SET (ulStreamControl, REG_ESTRMCTL_DMADUALMONO);

	      SET (ulStreamControl,
		   (REG_ESTRMCTL_DMAEN | REG_ESTRMCTL_DMAHST |
		    REG_ESTRMCTL_DMASINGLE));
	    }
	  else
	    {
	      if (m_pWaveDMADevice->GetDualMono ())	// DAH 03/14/2008 added DMADUALMONO set here
		SET (ulStreamControl, REG_L2STRMCTL_DMADUALMONO);

	      SET (ulStreamControl,
		   (REG_L2STRMCTL_DMAEN | REG_L2STRMCTL_DMAHST |
		    REG_L2STRMCTL_DMASINGLE));
	    }

	  m_pRegStreamControl->BitSet (ulStreamControl, TRUE);
	}
#ifdef DEBUG
      else
	{
	  cmn_err (CE_WARN,
		   "CHalDMA::StartPlayPreload call more than once! %lu Size %lu\n",
		   m_ulDeviceNumber + 16, m_ulPreloadSize);
	}
#endif
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalDMA::IsPreloadComplete ()
// Returns TRUE immediately if this is a record device
/////////////////////////////////////////////////////////////////////////////
{
  BOOLEAN bComplete = TRUE;

  if (!m_pWaveDMADevice->IsRecord ())
    {
      // DAH 09/10/2004 changed to waiting on DMA entry 0 having its length set to 0 to keep
      // the driver from accessing the PCI bus in such a tight loop while in preload mode.
      // This was causing audio break-up on other devices that were already running.
      //bComplete = (m_pRegStreamControl->Read() & REG_STREAMCTL_DMAHST) ? FALSE : TRUE;
      if (!m_ulPreloadSize)
	{
	  bComplete = TRUE;
	}
      else
	{
	  bComplete =
	    (m_pBufferBlock->Entry[0].ulControl & DMACONTROL_BUFLEN_MASK) !=
	    m_ulPreloadSize;
	}
      // DAH 04/17/2006 added this clear of preload size so multiple calls of IsPreloadComplete will still work OK
      if (bComplete)
	{
	  if (m_ulPreloadSize)
	    {
	      m_ulLastPreloadSize = m_ulPreloadSize;
#ifdef USE_PRELOAD_TIMING
	      m_ulLastPreloadTime =
		(ULONG) (SoundGetTime () - m_ullPreloadStartTime);
#endif

	      // DWORDs per millisecond would be: (m_ulLastPreloadSize*10000)/m_ulLastPreloadTime

	      //cmn_err((CE_WARN,"Preload Size: %lu Time: %lu DWORDs/ms: %lu\n", m_ulLastPreloadSize, m_ulLastPreloadTime, (m_ulLastPreloadSize*10000)/m_ulLastPreloadTime );
	    }
	  m_ulPreloadSize = 0;
	}
    }

  return (bComplete);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalDMA::WaitForPreloadToComplete ()
// return value is ignored
/////////////////////////////////////////////////////////////////////////////
{
  int nCounter;

  //DPET(NULL);
#define PRELOAD_TIMEOUT	10000000
  // wait for the preload to complete
  for (nCounter = 0; nCounter < PRELOAD_TIMEOUT; nCounter++)
    if (IsPreloadComplete ())
      break;

  //TEStop( m_ulDeviceNumber + 16 );
#ifdef DEBUG
  if (nCounter > (PRELOAD_TIMEOUT - 1))
    {
      cmn_err (CE_WARN, "CHalDMA::Start: Preload Counter Timeout!\n");
      return (HSTATUS_TIMEOUT);
    }
#endif

  //DPS(("C[%ld]", nCounter));

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalDMA::Start ()
// Puts this DMA channel into RUN mode.  DMA Buffer List should have at 
// least one entry already.
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulStreamControl = m_pRegStreamControl->Read ();	// make sure we start with the current contents of the hardware register

  //cmn_err((CE_WARN,"CHalDMA::Start %ld\n", m_ulDeviceNumber );
  DS (" DMAStart ", COLOR_BOLD);
  DX8 ((BYTE) m_ulDeviceNumber, COLOR_BOLD);
  DC (' ');

  m_ulLastBufferIndex = 0xFFFFFFFF;	// Invalid

  //TECreate( m_ulDeviceNumber + 16, "Preload:" );

  // if this is a play device, start the preload process
  // the StartPlayPreload only does something for play devices
  StartPlayPreload ();

  if (m_pHalAdapter->IsAES16e ())
    {
      // Turn off DMA Host Start and make sure all the optional stuff is cleared out
      CLR (ulStreamControl,
	   (REG_ESTRMCTL_DMAHST | REG_ESTRMCTL_DMASINGLE |
	    REG_ESTRMCTL_DMADUALMONO | REG_ESTRMCTL_OVERIGNORE));

      // Enable the DMA Engine
      SET (ulStreamControl, REG_ESTRMCTL_DMAEN);

      // Check to see if the app needs any of the extra bits turned on (these all get reset to OFF upon stop)
      if (m_pWaveDMADevice->GetDMASingle ())
	SET (ulStreamControl, REG_ESTRMCTL_DMASINGLE);

      if (m_pWaveDMADevice->GetDualMono ())
	SET (ulStreamControl, REG_ESTRMCTL_DMADUALMONO);

      if (m_pWaveDMADevice->GetOverrunIgnore ())
	SET (ulStreamControl, REG_ESTRMCTL_OVERIGNORE);
    }
  else
    {
      // Turn off DMA Host Start and make sure all the optional stuff is cleared out
      CLR (ulStreamControl,
	   (REG_L2STRMCTL_DMAHST | REG_L2STRMCTL_LIMIE | REG_L2STRMCTL_OVERIE
	    | REG_L2STRMCTL_DMASINGLE | REG_L2STRMCTL_DMADUALMONO |
	    REG_L2STRMCTL_OVERIGNORE));

      // Enable the DMA Engine
      SET (ulStreamControl, REG_L2STRMCTL_DMAEN);

      // Check to see if the app needs any of the extra bits turned on (these all get reset to OFF upon stop)
      if (m_pWaveDMADevice->GetLimitIE ())
	SET (ulStreamControl, REG_L2STRMCTL_LIMIE);

      if (m_pWaveDMADevice->GetOverrunIE ())
	SET (ulStreamControl, REG_L2STRMCTL_OVERIE);

      if (m_pWaveDMADevice->GetDMASingle ())
	SET (ulStreamControl, REG_L2STRMCTL_DMASINGLE);

      if (m_pWaveDMADevice->GetDualMono ())
	SET (ulStreamControl, REG_L2STRMCTL_DMADUALMONO);

      if (m_pWaveDMADevice->GetOverrunIgnore ())
	SET (ulStreamControl, REG_L2STRMCTL_OVERIGNORE);
    }

  if (m_pWaveDMADevice->m_bSyncStartEnabled)
    {
      if (m_pHalAdapter->IsAES16e ())
	{
	  SET (ulStreamControl, REG_ESTRMCTL_MODE_SYNCREADY);
	}
      else
	{
	  // only firmware 16 & above has sync start enabled
	  if (m_pHalAdapter->HasGlobalSyncStart ())
	    {
	      SET (ulStreamControl, REG_L2STRMCTL_MODE_SYNCREADY);
	      //cmn_err((CE_WARN,"MODE_SYNCREADY\n");
	      DS (" SYNCREADY ", COLOR_NORMAL);
	    }
	  else
	    {
	      SET (ulStreamControl, REG_L2STRMCTL_MODE_RUN);
	      //cmn_err((CE_WARN,"MODE_RUN\n");
	      DS (" RUN ", COLOR_NORMAL);
	    }
	}

      m_pHalAdapter->SyncStartReady (m_ulDeviceNumber, ulStreamControl);
      // reset the sync start enabled status for next time.
      m_pWaveDMADevice->m_bSyncStartEnabled = FALSE;
    }
  else
    {
      WaitForPreloadToComplete ();

      if (m_pHalAdapter->IsAES16e ())
	{
	  //cmn_err((CE_WARN,"Starting Device...\n");
	  // Write the control register to the hardware
	  SET (ulStreamControl, REG_ESTRMCTL_MODE_RUN);
	}
      else
	{
	  // must re-read PCPTR so this write to the stream control register doesn't clear it...
	  CLR (ulStreamControl, REG_L2STRMCTL_PCPTR_MASK);
	  ulStreamControl |=
	    (m_pRegStreamControl->Read () & REG_L2STRMCTL_PCPTR_MASK);

	  //cmn_err((CE_WARN,"Starting Device...\n");
	  // Write the control register to the hardware
	  SET (ulStreamControl, REG_L2STRMCTL_MODE_RUN);
	}

      m_pRegStreamControl->Write (ulStreamControl);
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalDMA::Stop ()
// Puts this DMA channel into IDLE mode
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalDMA::Stop %ld\n", m_ulDeviceNumber );

  m_ulLastBufferIndex = GetDMABufferIndex ();	// Save this off to a shadow register for use later...

  // Disable the IO Processor & the DMA Engine
  if (m_pHalAdapter->IsAES16e ())
    m_pRegStreamControl->BitSet ((REG_ESTRMCTL_MODE_MASK |
				  REG_ESTRMCTL_DMAEN), FALSE);
  else
    m_pRegStreamControl->BitSet ((REG_L2STRMCTL_MODE_MASK |
				  REG_L2STRMCTL_DMAEN | REG_L2STRMCTL_OVERIE |
				  REG_L2STRMCTL_LIMIE), FALSE);

  m_ulPCBufferIndex = 0;
  m_lEntriesInList = 0;
  m_ulPreloadSize = 0;

#ifdef DEBUG
  if (!m_pWaveDMADevice->IsRecord ())
    {
      //TERelease( m_ulDeviceNumber + 16 );
    }
#endif

  // make sure the DMA buffer block gets cleared
  RtlZeroMemory (m_pBufferBlock, sizeof (DMABUFFERBLOCK));

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalDMA::AddEntry (PVOID pBuffer, ULONG ulSize, BOOLEAN bInterrupt)
// Make sure the Control DWORD is written LAST as this is what the DMA 
// engine is looking see change to begin the next transfer when in DMAEN is high
// NOTE ulSize is in DWORDs
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"[AddEntry %02lx %lu %08lx %08lx] ", m_ulDeviceNumber, m_ulPCBufferIndex, (ULONG)pBuffer, ulSize );
  DB ('a', COLOR_BOLD_U);

  //DX32( m_pBufferBlock->Entry[ m_ulPCBufferIndex-1 ].ulHostPtr, COLOR_UNDERLINE );

  if (!ulSize || !pBuffer)
    {
      DB ('I', COLOR_BOLD_U);
      //cmn_err((CE_WARN,"CHalDMA::AddEntry: Invalid Parameter D[%ld] B[%p] A[%08lx]!\n", m_ulDeviceNumber, pBuffer, ulSize );
      return (HSTATUS_INVALID_PARAMETER);
    }

  //DX8((BYTE)m_lEntriesInList, COLOR_BOLD_U);

  // first determine if there is room in the buffer block for another entry (this allows all 16 entires to be used)
  if (m_lEntriesInList >= NUM_BUFFERS_PER_STREAM)
    {
      // This should never happen!
      DB ('F', COLOR_BOLD);
      cmn_err (CE_WARN, "CHalDMA::AddEntry: DMA Buffer List Full!\n");
      return (HSTATUS_BUFFER_FULL);
    }

#ifdef DEBUG
  if (!pBuffer)
    {
      cmn_err (CE_WARN, "CHalDMA::AddEntry: pBuffer is NULL!\n");
    }
  if (!ulSize)
    {
      cmn_err (CE_WARN, "CHalDMA::AddEntry: ulSize is ZERO!\n");
    }
#endif

#if defined(IA64) || defined(AMD64) || defined(__x86_64__)
  // this must be a 32-bit physical address, so the upper 32-bits must be zero anyway
  // I really hate having special case code, but there really isn't a way around this
  // for IA64 & AMD64
  m_pBufferBlock->Entry[m_ulPCBufferIndex].ulHostPtr =
    (ULONG) ((ULONGLONG) pBuffer);
#else
#if OSX && TARGET_CPU_PPC
  if (m_pHalAdapter->NeedsSwap ())
    m_pBufferBlock->Entry[m_ulPCBufferIndex].ulHostPtr =
      MySwapInt32 ((ULONG) pBuffer);
  else
#endif
    m_pBufferBlock->Entry[m_ulPCBufferIndex].ulHostPtr = (ULONG) pBuffer;
#endif
  if (bInterrupt)
    {
      //DC('i');
#if OSX && TARGET_CPU_PPC
      if (m_pHalAdapter->NeedsSwap ())
	m_pBufferBlock->Entry[m_ulPCBufferIndex].ulControl =
	  MySwapInt32 ((ulSize | DMACONTROL_HBUFIE));
      else
#endif
	m_pBufferBlock->Entry[m_ulPCBufferIndex].ulControl =
	  (ulSize | DMACONTROL_HBUFIE);
    }
  else
    {
      //DC('n');
#if OSX && TARGET_CPU_PPC
      if (m_pHalAdapter->NeedsSwap ())
	m_pBufferBlock->Entry[m_ulPCBufferIndex].ulControl =
	  MySwapInt32 (ulSize);
      else
#endif
	m_pBufferBlock->Entry[m_ulPCBufferIndex].ulControl = ulSize;
    }

  m_lEntriesInList++;		// update the entry count
  m_ulPCBufferIndex++;
  m_ulPCBufferIndex &= 0xF;

  //DC('+');
  //DX8(m_lEntriesInList,COLOR_NORMAL);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
USHORT
CHalDMA::FreeEntry ()
// Called at interrupt time to inform the DMA manager that an entry has been completed
/////////////////////////////////////////////////////////////////////////////
{
  DB ('f', COLOR_UNDERLINE);

  // make sure we don't free an entry that doesn't exist
  if (m_lEntriesInList > 0)
    {
      m_lEntriesInList--;
      //DC('-');
      //DX8(m_lEntriesInList,COLOR_NORMAL);
    }
  else
    {
      DB ('0', COLOR_BOLD);
      cmn_err (CE_WARN, "CHalDMA::FreeEntry: No Entries In List!\n");
      return (HSTATUS_INVALID_PARAMETER);	// never checked
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalDMA::GetDMABufferIndex ()
/////////////////////////////////////////////////////////////////////////////
{
  if (m_pWaveDMADevice->IsDMAActive ())
    {
      if (m_pHalAdapter->IsAES16e ())
	return ((m_pRegStreamStatus->Read () & REG_ESTRMSTAT_DMABINX_MASK) >>
		REG_ESTRMSTAT_DMABINX_OFFSET);
      else
	{
	  DB ('2', COLOR_UNDERLINE);
	  return ((m_pRegStreamControl->Read () & REG_L2STRMCTL_DMABINX_MASK)
		  >> REG_L2STRMCTL_DMABINX_OFFSET);
	}
    }
  else
    {
      DB ('L', COLOR_BOLD);
      return (m_ulLastBufferIndex);
    }
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalDMA::GetEntriesInHardware ()
/////////////////////////////////////////////////////////////////////////////
{
  LONG lEntries;

  // NOTE: DMA index is always the tail for record and play
  lEntries = (LONG) m_ulPCBufferIndex - (LONG) GetDMABufferIndex ();

  if (lEntries < 0)
    lEntries += NUM_BUFFERS_PER_STREAM;
  return ((ULONG) lEntries);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalDMA::GetBytesRemaining (ULONG ulIndex)
/////////////////////////////////////////////////////////////////////////////
{
  return ((m_pBufferBlock->Entry[ulIndex].
	   ulControl & DMACONTROL_BUFLEN_MASK) << 2);
}

/////////////////////////////////////////////////////////////////////////////
BOOLEAN
CHalDMA::IsDMAStarved ()
/////////////////////////////////////////////////////////////////////////////
{
  if (m_pHalAdapter->IsAES16e ())
    return (m_pRegStreamStatus->Read () & REG_ESTRMSTAT_DMASTARV ? TRUE :
	    FALSE);
  else
    return (m_pRegStreamControl->Read () & REG_L2STRMCTL_DMASTARV ? TRUE :
	    FALSE);
}
