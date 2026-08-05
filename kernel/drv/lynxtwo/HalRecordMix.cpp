/****************************************************************************
 HalRecordMix.cpp

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
  CHalRecordMix::Open (PHALADAPTER pHalAdapter, PULONG pRecordMixCtl,
		       PULONG pRecordMixStatus)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalRecordMix::Open\n"));

  m_pHalAdapter = pHalAdapter;
  m_RegMixControl.Init (m_pHalAdapter, pRecordMixCtl);
  m_RegMixStatus.Init (m_pHalAdapter, pRecordMixStatus);
  m_bHasPMixV3 = m_pHalAdapter->HasPMixV3 ();	// Shadow this so we don't have to keep looking it up

  m_asSource = 0;

  SetMute (FALSE);
  m_usDitherDepth = 24;
  SetDitherDepth (MIXVAL_DITHERDEPTH_AUTO);
  SetDither (TRUE);

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
ULONG
CHalRecordMix::GetLevel ()
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulLevel = m_RegMixStatus.Read () & REG_RMIXSTAT_LEVEL_MASK;
  if (ulLevel > 0x7FFFF)
    ulLevel = 0x7FFFF;
  return (ulLevel);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRecordMix::ResetLevel ()
/////////////////////////////////////////////////////////////////////////////
{
  if (!m_bHasPMixV3)
    {
      m_RegMixStatus.BitSet (REG_RMIXSTAT_LEVEL_RESET, TRUE);
    }
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRecordMix::SetSource (USHORT usSource)
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalRecordMix::SetSource\n"));

  ULONG ulSource = usSource & REG_RMIX_INSRC_MASK_V3;

  if (m_bHasPMixV3)
    {
      //ulSource 0..31 then 64..111 are valid
      if (ulSource > 31)
	ulSource += 32;		// 32 == 64, 32-64
    }
  else if (m_pHalAdapter->IsLynxTWO ())
    {
      if (ulSource > 7)
	ulSource += 24;		// 8 == 32
    }
  else				// AES16 / AES16e
    {
      if (ulSource > 15)
	ulSource += 16;		// 16 == 32, 32-47
    }

  if (m_bHasPMixV3)
    m_RegMixControl.Write (ulSource, REG_RMIX_INSRC_MASK_V3);
  else
    m_RegMixControl.Write (ulSource, REG_RMIX_INSRC_MASK);

  m_asSource = usSource;
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRecordMix::SetMute (BOOLEAN bMute)
/////////////////////////////////////////////////////////////////////////////
{
  m_bMute = bMute ? TRUE : FALSE;
  m_RegMixControl.BitSet (REG_RMIX_MUTE, m_bMute);
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRecordMix::SetDitherDepth (USHORT usDitherDepth)
// Input is either from Application trying to set dither depth or from 
// driver trying to inform us of the bit depth 
/////////////////////////////////////////////////////////////////////////////
{
  //cmn_err((CE_WARN,"CHalRecordMix::SetDitherDepth %u\n", usDitherDepth ));

  // if this call from the driver trying to set the dither depth...
  if (usDitherDepth > MIXVAL_DITHERDEPTH_COUNT)
    {
      m_usDitherDepth = usDitherDepth;	// remember for later...
      usDitherDepth = m_usDitherControl;	// reset usDitherDepth so the code below will work
    }
  m_usDitherControl = usDitherDepth;	// save value for GetDitherDepth

  switch (m_usDitherControl)
    {
    case MIXVAL_DITHERDEPTH_AUTO:
      // The user wants the driver to select the dither depth.  
      // Next time this device gets started the dither depth will get set.
      break;
    case MIXVAL_DITHERDEPTH_8BIT:
      m_usDitherDepth = 8;
      break;
    case MIXVAL_DITHERDEPTH_16BIT:
      m_usDitherDepth = 16;
      break;
    case MIXVAL_DITHERDEPTH_20BIT:
      m_usDitherDepth = 20;
      break;
    case MIXVAL_DITHERDEPTH_24BIT:	// Dither is off
      m_usDitherDepth = 24;
      break;
    }

  ULONG ulDitherDepth;

  switch (m_usDitherDepth)
    {
    case 8:
      ulDitherDepth = REG_RMIX_DITHERDEPTH_8BITS;
      break;
    case 16:
      ulDitherDepth = REG_RMIX_DITHERDEPTH_16BITS;
      break;
    case 20:
      ulDitherDepth = REG_RMIX_DITHERDEPTH_20BITS;
      break;
    default:
    case 24:
      ulDitherDepth = REG_RMIX_DITHERDEPTH_24BITS;
      m_usDitherDepth = 24;
      break;
    }

  //cmn_err((CE_WARN,"ulDitherDepth %08lx\n", ulDitherDepth ));
  m_RegMixControl.Write (ulDitherDepth, REG_RMIX_DITHERDEPTH_MASK);
  SetDither (m_bDither);	// update the dither on/off status
}

/////////////////////////////////////////////////////////////////////////////
void
CHalRecordMix::SetDither (BOOLEAN bDither)
/////////////////////////////////////////////////////////////////////////////
{
  m_bDither = bDither ? TRUE : FALSE;

  // if the dither depth is 24, turn off dither in the hardware
  if (m_usDitherDepth == 24)
    m_RegMixControl.BitSet (REG_RMIX_DITHER, FALSE);
  else
    {
      //cmn_err((CE_WARN,"REG_RMIX_DITHER %08lx\n", m_bDither ));
      m_RegMixControl.BitSet (REG_RMIX_DITHER, m_bDither);
    }
}

// PLEASE NOTE: See CHalAdapter::SetDitherType( PHALREGISTER pRMix0Control, USHORT usDitherType ) to set the DitherType
