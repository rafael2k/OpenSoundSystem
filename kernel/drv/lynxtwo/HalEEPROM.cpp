/****************************************************************************
 HalEEPROM.cpp

 Description:	Read the serial number from a LynxTWO class board

 Created: David A. Hoatson, April 2003
	
 Copyright © 2003 Lynx Studio Technology, Inc.

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

 Notes: Hey, it ain't pretty but it works!  Most of this code came 
 from Xilinx example code, so shoot them for how unreadable this is.

 4 spaces per tab

 Revision History
 
 When      Who  Description
 --------- ---  ------------------------------------------------------------
****************************************************************************/
#include <StdAfx.h>

#include "HalAdapter.h"
#include <SharedControls.h>

#define TCK 0
#define TMS 1
#define TDI 2

/* 4.04 [NEW] Error codes for xsvfExecute. */
#define XSVF_ERROR_NONE         0
#define XSVF_ERROR_UNKNOWN      1
#define XSVF_ERROR_TDOMISMATCH  2
#define XSVF_ERROR_MAXRETRIES   3	/* TDO mismatch after max retries */
#define XSVF_ERROR_ILLEGALCMD   4
#define XSVF_ERROR_ILLEGALSTATE 5
#define XSVF_ERROR_DATAOVERFLOW 6	/* Data > lenVal MAX_LEN buffer size */
/* Insert new errors here */
#define XSVF_ERROR_LAST         7

/* the lenVal structure is a byte oriented type used to store an */
/* arbitrary length binary value. As an example, the hex value   */
/* 0x0e3d is represented as a lenVal with len=2 (since 2 bytes   */
/* and val[0]=0e and val[1]=3d.  val[2-MAX_LEN] are undefined    */

/* maximum length (in bytes) of value to read in        */
/* this needs to be at least 4, and longer than the     */
/* length of the longest SDR instruction.  If there is, */
/* only 1 device in the chain, MAX_LEN must be at least */
/* ceil(27/8) == 4.  For 6 devices in a chain, MAX_LEN  */
/* must be 5, for 14 devices MAX_LEN must be 6, for 20  */
/* devices MAX_LEN must be 7, etc..                     */
/* You can safely set MAX_LEN to a smaller number if you*/
/* know how many devices will be in your chain.         */
#define MAX_LEN 10

typedef struct var_len_byte
{
  SHORT len;			/* number of chars in this value */
  BYTE val[MAX_LEN + 1];	/* bytes of data */
} LENVALUE, *PLENVALUE;

LONG LVValue (PLENVALUE x);
SHORT LVEqual (PLENVALUE expected, PLENVALUE actual, PLENVALUE mask);
void LVReadValue (PLENVALUE x, SHORT numBytes);


/*****************************************************************************
* Struct:       SXsvfInfo
* Description:  This structure contains all of the data used during the
*               execution of the XSVF.  Some data is persistent, predefined
*               information (e.g. lRunTestTime).  The bulk of this struct's
*               size is due to the lenVal structs (defined in lenval.h)
*               which contain buffers for the active shift data.  The MAX_LEN
*               #define in lenval.h defines the size of these buffers.
*               These buffers must be large enough to store the longest
*               shift data in your XSVF file.  For example:
*                   MAX_LEN >= ( longest_shift_data_in_bits / 8 )
*               Because the lenVal struct dominates the space usage of this
*               struct, the rough size of this struct is:
*                   sizeof( SXsvfInfo ) ~= MAX_LEN * 7 (number of lenVals)
*****************************************************************************/
typedef struct tagSXsvfInfo
{
  /* XSVF status information */
  BYTE ucComplete;		/* 0 = running; 1 = complete */
  BYTE ucCommand;		/* Current XSVF command BYTE */
  LONG lCommandCount;		/* Number of commands processed */
  int iErrorCode;		/* An error code. 0 = no error. */

  /* TAP state/sequencing information */
  BYTE ucTapState;		/* Current TAP state */
  BYTE ucEndIR;			/* ENDIR TAP state (See SVF) */
  BYTE ucEndDR;			/* ENDDR TAP state (See SVF) */

  /* RUNTEST information */
  BYTE ucMaxRepeat;		/* Max repeat loops (for xc9500/xl) */
  LONG lRunTestTime;		/* Pre-specified RUNTEST time (usec) */

  /* Shift Data Info and Buffers */
  LONG lShiftLengthBits;	/* Len. current shift data in bits */
  SHORT sShiftLengthBytes;	/* Len. current shift data in bytes */

  LENVALUE lvTdi;		/* Current TDI shift data */
  LENVALUE lvTdoExpected;	/* Expected TDO shift data */
  LENVALUE lvTdoCaptured;	/* Captured TDO shift data */
  LENVALUE lvTdoMask;		/* TDO mask: 0=dontcare; 1=compare */
} SXsvfInfo, *pSXsvfInfo;

// Declare pointer to functions that perform XSVF commands 
typedef int (*TXsvfDoCmdFuncPtr) (SXsvfInfo *);

/////////////////////////////////////////////////////////////////////////////
// XSVF Command Bytes
/////////////////////////////////////////////////////////////////////////////

// encodings of xsvf instructions 
#define XCOMPLETE        0
#define XSDRE            14
#define XSDRTDOE         17
#define XLASTCMD         21

/////////////////////////////////////////////////////////////////////////////
// XSVF Command Parameter Values
/////////////////////////////////////////////////////////////////////////////

#define XTAPSTATE_RESET     0x00
#define XTAPSTATE_RUNTEST   0x01
#define XTAPSTATE_PAUSE     0x02
#define XTAPSTATE_PAUSEDR   0x12
#define XTAPSTATE_PAUSEIR   0x22
#define XTAPSTATE_SHIFT     0x04
#define XTAPSTATE_SHIFTDR   0x14
#define XTAPSTATE_SHIFTIR   0x24
#define XTAPSTATE_RESERVED  0x08	// Unused TAP state bit
#define XTAPSTATE_MASKDR    0x10	// Identifies DR register states
#define XTAPSTATE_MASKIR    0x20	// Identifies IR register states
#define XTAPSTATE_MASKREG   0x30	// Mask to extract the register kind

/////////////////////////////////////////////////////////////////////////////
class CEEPROM
/////////////////////////////////////////////////////////////////////////////
{
public:
  CEEPROM (PHALADAPTER pHalAdapter, PBYTE pucBuffer);	// Constructor
  ~CEEPROM ();			// Destructor
  int xsvfExecute (pSXsvfInfo pXsvfInfo);

private:
  void LVReadValue (PLENVALUE plv, SHORT sNumBytes);

  // Utility Functions
  void xsvfTmsTransition (SHORT sTms);
  int xsvfGotoTapState (PBYTE pucTapState, BYTE ucTargetState);
  void xsvfShiftOnly (LONG lNumBits, PLENVALUE plvTdi,
		      PLENVALUE plvTdoCaptured);
  int xsvfShift (PBYTE pucTapState, BYTE ucStartState, LONG lNumBits,
		 PLENVALUE plvTdi, PLENVALUE plvTdoCaptured,
		 PLENVALUE plvTdoExpected, PLENVALUE plvTdoMask,
		 BYTE ucEndState, LONG lRunTestTime, BYTE ucMaxRepeat);
  int xsvfBasicXSDRTDO (PBYTE pucTapState, LONG lShiftLengthBits,
			SHORT sShiftLengthBytes, PLENVALUE plvTdi,
			PLENVALUE plvTdoCaptured, PLENVALUE plvTdoExpected,
			PLENVALUE plvTdoMask, BYTE ucEndState,
			LONG lRunTestTime, BYTE ucMaxRepeat);

  // XSVF Command Functions (type = TXsvfDoCmdFuncPtr)
  int xsvfDoIllegalCmd (SXsvfInfo * pXsvfInfo);
  int xsvfDoXCOMPLETE (SXsvfInfo * pXsvfInfo);
  int xsvfDoXTDOMASK (SXsvfInfo * pXsvfInfo);
  int xsvfDoXSIR (SXsvfInfo * pXsvfInfo);
  int xsvfDoXSDR (SXsvfInfo * pXsvfInfo);
  int xsvfDoXRUNTEST (SXsvfInfo * pXsvfInfo);
  int xsvfDoXREPEAT (SXsvfInfo * pXsvfInfo);
  int xsvfDoXSDRSIZE (SXsvfInfo * pXsvfInfo);
  int xsvfDoXSDRTDO (SXsvfInfo * pXsvfInfo);
  int xsvfDoXSDRBCE (SXsvfInfo * pXsvfInfo);
  int xsvfDoXSDRTDOBCE (SXsvfInfo * pXsvfInfo);
  int xsvfDoXSTATE (SXsvfInfo * pXsvfInfo);

  // Main XSV functions
  int xsvfRun (SXsvfInfo * pXsvfInfo);

  // Lynx defined utility functions
  void EEPulseClock ();
  void EESetPort (SHORT p, SHORT val);
  BYTE EEReadTDOBit ();
  void EEWaitTime (LONG lMicroSeconds);
  void EEGetNextByte (PBYTE pucByte);

  // Lynx variables
  PHALADAPTER m_pHalAdapter;

  PBYTE m_pCurrentBufferBase;
  ULONG m_ulCurrentOffset;
  ULONG m_ulControlReg;
  PULONG m_pL2ControlReg;
  PULONG m_pL2StatusReg;
};

/////////////////////////////////////////////////////////////////////////////
LONG
LVValue (PLENVALUE plvValue)
//      Description:  Extract the LONG value from the lenval array.
//      Parameters:   plvValue    - ptr to lenval.
//      Returns:      LONG        - the extracted value.
/////////////////////////////////////////////////////////////////////////////
{
  LONG lValue;			// result to hold the accumulated result 
  SHORT sIndex;

  lValue = 0;
  for (sIndex = 0; sIndex < plvValue->len; ++sIndex)
    {
      lValue <<= 8;		// shift the accumulated result
      lValue |= plvValue->val[sIndex];	// get the last byte first
    }

  return (lValue);
}

/////////////////////////////////////////////////////////////////////////////
SHORT
LVEqual (PLENVALUE plvTdoExpected, PLENVALUE plvTdoCaptured,
	 PLENVALUE plvTdoMask)
// Description:  Compare two lenval arrays with an optional mask.
// Parameters:   plvTdoExpected  - ptr to lenval #1.
//               plvTdoCaptured  - ptr to lenval #2.
//               plvTdoMask      - optional ptr to mask (=0 if no mask).
// Returns:      SHORT   - 0 = mismatch; 1 = equal.
/////////////////////////////////////////////////////////////////////////////
{
  SHORT sEqual;
  SHORT sIndex;
  BYTE ucByteVal1;
  BYTE ucByteVal2;
  BYTE ucByteMask;

  sEqual = 1;
  sIndex = plvTdoExpected->len;

  while (sEqual && sIndex--)
    {
      ucByteVal1 = plvTdoExpected->val[sIndex];
      ucByteVal2 = plvTdoCaptured->val[sIndex];
      if (plvTdoMask)
	{
	  ucByteMask = plvTdoMask->val[sIndex];
	  ucByteVal1 &= ucByteMask;
	  ucByteVal2 &= ucByteMask;
	}
      if (ucByteVal1 != ucByteVal2)
	{
	  sEqual = 0;
	}
    }

  return (sEqual);
}

/////////////////////////////////////////////////////////////////////////////
void
CEEPROM::LVReadValue (PLENVALUE plv, SHORT sNumBytes)
// Description:  read from XSVF numBytes bytes of data into x.
// Parameters:   plv         - ptr to lenval in which to put the bytes read.
//               sNumBytes   - the number of bytes to read.
/////////////////////////////////////////////////////////////////////////////
{
  PBYTE pucVal;

  plv->len = sNumBytes;		// set the length of the lenVal        
  for (pucVal = plv->val; sNumBytes; --sNumBytes, ++pucVal)
    {
      // read a byte of data into the lenVal 
      EEGetNextByte (pucVal);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Utility Functions
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Function:     xsvfGetAsNumBytes
// Description:  Calculate the number of bytes the given number of bits
//               consumes.
// Parameters:   lNumBits    - the number of bits.
// Returns:      SHORT       - the number of bytes to store the number of bits.
/////////////////////////////////////////////////////////////////////////////
SHORT
xsvfGetAsNumBytes (LONG lNumBits)
{
  return ((SHORT) ((lNumBits + 7L) / 8L));
}

/////////////////////////////////////////////////////////////////////////////
// Function:     xsvfTmsTransition
// Description:  Apply TMS and transition TAP controller by applying one TCK
//               cycle.
// Parameters:   sTms    - new TMS value.
// Returns:      void.
/////////////////////////////////////////////////////////////////////////////
void
CEEPROM::xsvfTmsTransition (SHORT sTms)
{
  EESetPort (TMS, sTms);
  EEPulseClock ();
}

#define XSVFTMSTRANSITION(sTms,pzNewState,iRegister) { xsvfTmsTransition(sTms); }

/*****************************************************************************
* Function:     xsvfGotoTapState
* Description:  From the current TAP state, go to the named TAP state.
*               A target state of RESET ALWAYS causes TMS reset sequence.
*               Except for RESET, when target==current state, then do nothing.
*               Otherwise, this function SUPPORTS these state transitions:
*               CURRENT - TARGET:   CURRENT -> intermediate... -> TARGET
*               -------   -------   ------------------------------------------
*               *       - RESET:    * -> ? -> ? -> ? -> ? -> RESET
*               RESET   - RUNTEST:  RESET -> RUNTEST
*               RESET   - SHIFTDR:  RESET -> runtest -> seldr -> capdr
*                                       -> SHIFTDR
*               RESET   - SHIFTIR:  RESET -> runtest -> seldr -> selir
*                                       -> capdr -> SHIFTIR
*               RUNTEST - SHIFTDR:  RUNTEST -> seldr -> capdr -> SHIFTDR
*               RUNTEST - SHIFTIR:  RUNTEST -> seldr -> selir -> capdr
*                                       -> SHIFTIR
*               PAUSEDR - RUNTEST:  PAUSEDR -> exit2dr -> updatedr -> RUNTEST
*               PAUSEDR - SHIFTDR:  PAUSEDR -> exit2dr -> SHIFTDR
*               PAUSEDR - SHIFTIR:  PAUSEDR -> exit2dr -> updatedr -> seldr
*                                       -> selir -> capir -> SHIFTIR
*               PAUSEIR - RUNTEST:  PAUSEIR -> exit2ir -> updateir -> RUNTEST
*               PAUSEIR - SHIFTDR:  PAUSEIR -> exit2ir -> updateir -> seldr
*                                       -> capdr -> SHIFTDR
*               PAUSEIR - SHIFTIR:  PAUSEIR -> exit2ir -> SHIFTIR
*               SHIFTDR - PAUSEDR:  SHIFTDR -> exit1dr -> PAUSEDR
*               SHIFTDR - RUNTEST:  SHIFTDR -> exit1dr -> updatedr -> RUNTEST
*               SHIFTDR - SHIFTIR:  SHIFTDR -> exit1dr -> updatedr -> seldr
*                                       -> selir -> capir -> SHIFTIR
*               SHIFTIR - PAUSEIR:  SHIFTIR -> exit1ir -> PAUSEIR
*               SHIFTIR - RUNTEST:  SHIFTIR -> exit1ir -> updateir -> RUNTEST
*               SHIFTIR - SHIFTDR:  SHIFTIR -> exit1ir -> updateir
*                                       -> seldr -> capdr -> SHIFTDR
*
*               The following transitions are NOT supported:
*               CURRENT - NEW:      CURRENT -> intermediate... -> NEW
*               -------   -------   ------------------------------------------
*               RESET   - PAUSEDR   Not supported.
*               RESET   - PAUSEIR   Not supported.
*               RUNTEST - PAUSEDR   Not supported.
*               RUNTEST - PAUSEIR   Not supported.
*               SHIFTDR - PAUSEIR   Not supported.
*               SHIFTIR - PAUSEDR   Not supported.
* Parameters:   pucTapState     - Current TAP state.
*               ucTargetState   - New target TAP state.
* Returns:      int             - 0 = success; otherwise error.
*****************************************************************************/
int
CEEPROM::xsvfGotoTapState (PBYTE pucTapState, BYTE ucTargetState)
{
  int i;
  int iErrorCode;

  iErrorCode = XSVF_ERROR_NONE;
  if (ucTargetState == XTAPSTATE_RESET)
    {
      // If RESET, always perform TMS reset sequence to reset/sync TAPs 
      XSVFTMSTRANSITION (1, "RESET", 0);
      for (i = 0; i < 4; ++i)
	{
	  EEPulseClock ();
	}
    }
  else if (ucTargetState == *pucTapState)
    {
      // Already in stable state;  do nothing 
    }
  else if (ucTargetState & XTAPSTATE_PAUSE)
    {
      if ((!(*pucTapState & XTAPSTATE_SHIFT)) ||
	  (!(*pucTapState & ucTargetState & XTAPSTATE_MASKREG)))
	{
	  // Not already in same shift state. 
	  iErrorCode = XSVF_ERROR_ILLEGALSTATE;
	  //cmn_err((CE_WARN,"xsvfGotoTapState: XSVF_ERROR_ILLEGALSTATE\n"));
	}
      else
	{
	  // Already be in shift state;  goto pause from shift 
	  XSVFTMSTRANSITION (1, "EXIT1", (ucTargetState & XTAPSTATE_MASKREG));
	  XSVFTMSTRANSITION (0, "PAUSE", (ucTargetState & XTAPSTATE_MASKREG));
	}
    }
  else				// Go to RUNTEST or SHIFT 
    {
      if (*pucTapState & (XTAPSTATE_SHIFT | XTAPSTATE_PAUSE))
	{
	  if ((*pucTapState & XTAPSTATE_PAUSE) &&
	      (*pucTapState & XTAPSTATE_MASKREG & ucTargetState))
	    {
	      // Return to same register shift 
	      XSVFTMSTRANSITION (1, "EXIT2",
				 (ucTargetState & XTAPSTATE_MASKREG));
	      XSVFTMSTRANSITION (0, "SHIFT",
				 (ucTargetState & XTAPSTATE_MASKREG));
	      *pucTapState = ucTargetState;
	    }
	  else
	    {
	      XSVFTMSTRANSITION (1, "EXIT",
				 (ucTargetState & XTAPSTATE_MASKREG));
	      XSVFTMSTRANSITION (1, "UPDATE",
				 (ucTargetState & XTAPSTATE_MASKREG));
	    }
	}
      // Now in Update, Reset, RunTest, or Shift 
      if (ucTargetState == XTAPSTATE_RUNTEST)
	{
	  XSVFTMSTRANSITION (0, "RUNTEST", 0);
	}
      else if (*pucTapState != ucTargetState)
	{
	  // Go to SHIFT 
	  if (*pucTapState == XTAPSTATE_RESET)
	    {
	      XSVFTMSTRANSITION (0, "RUNTEST", 0);
	    }
	  // Now in Update, RunTest 
	  XSVFTMSTRANSITION (1, "SELECTDR", 0);
	  if (ucTargetState & XTAPSTATE_MASKIR)
	    {
	      XSVFTMSTRANSITION (1, "SELECTIR", 0);
	    }
	  XSVFTMSTRANSITION (0, "CAPTURE",
			     (ucTargetState & XTAPSTATE_MASKREG));
	  XSVFTMSTRANSITION (0, "SHIFT", (ucTargetState & XTAPSTATE_MASKREG));
	}
    }

  // Set the final state 
  *pucTapState = ucTargetState;

  return (iErrorCode);
}

/*****************************************************************************
* Function:     xsvfShiftOnly
* Description:  Assumes that starting TAP state is SHIFT-DR or SHIFT-IR.
*               Shift the given TDI data into the JTAG scan chain.
*               Optionally, save the TDO data shifted out of the scan chain.
*               Last shift cycle is special:  capture last TDO, set last TDI,
*               but does not pulse TCK.  Caller must pulse TCK and optionally
*               set TMS=1 to exit shift state.
* Parameters:   lNumBits        - number of bits to shift.
*               plvTdi          - ptr to lenval for TDI data.
*               plvTdoCaptured  - ptr to lenval for storing captured TDO data.
* Returns:      void.
*****************************************************************************/
void
CEEPROM::xsvfShiftOnly (LONG lNumBits, PLENVALUE plvTdi,
			PLENVALUE plvTdoCaptured)
{
  PBYTE pucTdi;
  PBYTE pucTdo;
  BYTE ucTdiByte;
  BYTE ucTdoByte;
  BYTE ucTdoBit;
  int i;

  // assert( ( ( lNumBits + 7 ) / 8 ) == plvTdi->len ); 

  // Initialize TDO storage len == TDI len 
  pucTdo = 0;
  if (plvTdoCaptured)
    {
      plvTdoCaptured->len = plvTdi->len;
      pucTdo = plvTdoCaptured->val + plvTdi->len;
    }

  // Shift LSB first.  val[N-1] == LSB.  val[0] == MSB. 
  pucTdi = plvTdi->val + plvTdi->len;
  while (lNumBits)
    {
      // Process on a byte-basis 
      ucTdiByte = (*(--pucTdi));
      ucTdoByte = 0;
      for (i = 0; (lNumBits && (i < 8)); ++i)
	{
	  if (pucTdo)
	    {
	      // Save the TDO value 
	      ucTdoBit = EEReadTDOBit ();
	      ucTdoByte |= (ucTdoBit << i);
	    }

	  // Set the new TDI value 
	  EESetPort (TDI, (SHORT) (ucTdiByte & 1));
	  ucTdiByte >>= 1;

	  // Shift data except for last TDI bit 
	  if (--lNumBits)
	    {
	      EEPulseClock ();
	    }
	}

      // Save the TDO byte value 
      if (pucTdo)
	{
	  (*(--pucTdo)) = ucTdoByte;
	}
    }
}

/*****************************************************************************
* Function:     xsvfShift
* Description:  Goes to the given starting TAP state.
*               Calls xsvfShiftOnly to shift in the given TDI data and
*               optionally capture the TDO data.
*               Compares the TDO captured data against the TDO expected
*               data.
*               If a data mismatch occurs, then executes the exception
*               handling loop upto ucMaxRepeat times.
* Parameters:   pucTapState     - Ptr to current TAP state.
*               ucStartState    - Starting shift state: Shift-DR or Shift-IR.
*               lNumBits        - number of bits to shift.
*               plvTdi          - ptr to lenval for TDI data.
*               plvTdoCaptured  - ptr to lenval for storing TDO data.
*               plvTdoExpected  - ptr to expected TDO data.
*               plvTdoMask      - ptr to TDO mask.
*               ucEndState      - state in which to end the shift.
*               lRunTestTime    - amount of time to wait after the shift.
*               ucMaxRepeat     - Maximum number of retries on TDO mismatch.
* Returns:      int             - 0 = success; otherwise TDO mismatch.
* Notes:        XC9500XL-only Optimization:
*               Skip the EEWaitTime() if plvTdoMask->val[0:plvTdoMask->len-1]
*               is NOT all zeros and sMatch==1.
*****************************************************************************/
int
CEEPROM::xsvfShift (PBYTE pucTapState, BYTE ucStartState, LONG lNumBits,
		    PLENVALUE plvTdi, PLENVALUE plvTdoCaptured,
		    PLENVALUE plvTdoExpected, PLENVALUE plvTdoMask,
		    BYTE ucEndState, LONG lRunTestTime, BYTE ucMaxRepeat)
{
  int iErrorCode;
  int iMismatch;
  BYTE ucRepeat;

  iErrorCode = XSVF_ERROR_NONE;
  iMismatch = 0;
  ucRepeat = 0;

  if (!lNumBits)
    {
      // Compatibility with XSVF2.00:  XSDR 0 = no shift, but wait in RTI 
      if (lRunTestTime)
	{
	  // Wait for prespecified XRUNTEST time 
	  xsvfGotoTapState (pucTapState, XTAPSTATE_RUNTEST);
	  EEWaitTime (lRunTestTime);
	}
    }
  else
    {
      do
	{
	  // Goto Shift-DR or Shift-IR 
	  xsvfGotoTapState (pucTapState, ucStartState);

	  // Shift TDI and capture TDO 
	  xsvfShiftOnly (lNumBits, plvTdi, plvTdoCaptured);

	  if (plvTdoExpected)
	    {
	      // Compare TDO data to expected TDO data 
	      iMismatch =
		!LVEqual (plvTdoExpected, plvTdoCaptured, plvTdoMask);
	    }

	  if (ucStartState == ucEndState)
	    {
	      // Staying/continuing in shift state;  clock in last TDI bit 
	      EEPulseClock ();
	    }
	  else			// Exit shift 
	    {
	      if (iMismatch && lRunTestTime && (ucRepeat < ucMaxRepeat))
		{
		  // Do exception handling retry - ShiftDR only 
		  xsvfGotoTapState (pucTapState, XTAPSTATE_PAUSEDR);
		  // Shift 1 extra bit 
		  xsvfGotoTapState (pucTapState, XTAPSTATE_SHIFTDR);
		  // Increment RUNTEST time by an additional 25% 
		  lRunTestTime += (lRunTestTime >> 2);
		}
	      else
		{
		  // Do normal exit from Shift-XR 
		  xsvfGotoTapState (pucTapState, ucEndState);
		}

	      if (lRunTestTime)
		{
		  // Wait for prespecified XRUNTEST time 
		  xsvfGotoTapState (pucTapState, XTAPSTATE_RUNTEST);
		  EEWaitTime (lRunTestTime);
		}
	    }
	}
      while (iMismatch && (ucRepeat++ < ucMaxRepeat));
    }

  if (iMismatch)
    {
      if (ucMaxRepeat && (ucRepeat > ucMaxRepeat))
	{
	  iErrorCode = XSVF_ERROR_MAXRETRIES;
	  //cmn_err((CE_WARN,"xsvfShift: XSVF_ERROR_MAXRETRIES\n"));
	}
      else
	{
	  iErrorCode = XSVF_ERROR_TDOMISMATCH;
	  //cmn_err((CE_WARN,"xsvfShift: XSVF_ERROR_TDOMISMATCH\n"));
	}
    }

  return (iErrorCode);
}

/****************************************************************************
* Function:     xsvfBasicXSDRTDO
* Description:  Get the XSDRTDO parameters and execute the XSDRTDO command.
*               This is the common function for all XSDRTDO commands.
* Parameters:   pucTapState         - Current TAP state.
*               lShiftLengthBits    - number of bits to shift.
*               sShiftLengthBytes   - number of bytes to read.
*               plvTdi              - ptr to lenval for TDI data.
*               lvTdoCaptured       - ptr to lenval for storing TDO data.
*               iEndState           - state in which to end the shift.
*               lRunTestTime        - amount of time to wait after the shift.
*               ucMaxRepeat         - maximum xc9500/xl retries.
* Returns:      int                 - 0 = success; otherwise TDO mismatch.
****************************************************************************/
int
CEEPROM::xsvfBasicXSDRTDO (PBYTE pucTapState, LONG lShiftLengthBits,
			   SHORT sShiftLengthBytes, PLENVALUE plvTdi,
			   PLENVALUE plvTdoCaptured, PLENVALUE plvTdoExpected,
			   PLENVALUE plvTdoMask, BYTE ucEndState,
			   LONG lRunTestTime, BYTE ucMaxRepeat)
{
  LVReadValue (plvTdi, sShiftLengthBytes);
  if (plvTdoExpected)
    {
      LVReadValue (plvTdoExpected, sShiftLengthBytes);
    }
  return (xsvfShift (pucTapState, XTAPSTATE_SHIFTDR, lShiftLengthBits,
		     plvTdi, plvTdoCaptured, plvTdoExpected, plvTdoMask,
		     ucEndState, lRunTestTime, ucMaxRepeat));
}

//============================================================================
// XSVF Command Functions (type = TXsvfDoCmdFuncPtr)
// These functions update pXsvfInfo->iErrorCode only on an error.
// Otherwise, the error code is left alone.
// The function returns the error code from the function.
//============================================================================

/****************************************************************************
* Function:     xsvfDoIllegalCmd
* Description:  Function place holder for illegal/unsupported commands.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
****************************************************************************/
int
CEEPROM::xsvfDoIllegalCmd (SXsvfInfo * pXsvfInfo)
{
  pXsvfInfo->iErrorCode = XSVF_ERROR_ILLEGALCMD;
  return (pXsvfInfo->iErrorCode);
}

/****************************************************************************
* Function:     xsvfDoXCOMPLETE
* Description:  XCOMPLETE (no parameters)
*               Update complete status for XSVF player.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
****************************************************************************/
int
CEEPROM::xsvfDoXCOMPLETE (SXsvfInfo * pXsvfInfo)
{
  pXsvfInfo->ucComplete = 1;
  return (XSVF_ERROR_NONE);
}

/****************************************************************************
* Function:     xsvfDoXTDOMASK
* Description:  XTDOMASK <lenVal.TdoMask[XSDRSIZE]>
*               Prespecify the TDO compare mask.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
****************************************************************************/
int
CEEPROM::xsvfDoXTDOMASK (SXsvfInfo * pXsvfInfo)
{
  LVReadValue (&(pXsvfInfo->lvTdoMask), pXsvfInfo->sShiftLengthBytes);
  return (XSVF_ERROR_NONE);
}

/*****************************************************************************
* Function:     xsvfDoXSIR
* Description:  XSIR <(byte)shiftlen> <lenVal.TDI[shiftlen]>
*               Get the instruction and shift the instruction into the TAP.
*               If prespecified XRUNTEST!=0, goto RUNTEST and wait after
*               the shift for XRUNTEST usec.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXSIR (SXsvfInfo * pXsvfInfo)
{
  BYTE ucShiftIrBits;
  SHORT sShiftIrBytes;
  int iErrorCode;

  /* Get the shift length and store */
  EEGetNextByte (&ucShiftIrBits);
  sShiftIrBytes = xsvfGetAsNumBytes (ucShiftIrBits);

  if (sShiftIrBytes > MAX_LEN)
    {
      iErrorCode = XSVF_ERROR_DATAOVERFLOW;
      //cmn_err((CE_WARN,"xsvfDoXSIR: XSVF_ERROR_DATAOVERFLOW\n"));
    }
  else
    {
      /* Get and store instruction to shift in */
      LVReadValue (&(pXsvfInfo->lvTdi), xsvfGetAsNumBytes (ucShiftIrBits));

      /* Shift the data */
      iErrorCode = xsvfShift (&(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTIR,
			      ucShiftIrBits, &(pXsvfInfo->lvTdi),
			      /*plvTdoCaptured */ 0, /*plvTdoExpected */ 0,
			      /*plvTdoMask */ 0, pXsvfInfo->ucEndIR,
			      pXsvfInfo->lRunTestTime, /*ucMaxRepeat */ 0);
    }

  if (iErrorCode != XSVF_ERROR_NONE)
    {
      pXsvfInfo->iErrorCode = iErrorCode;
    }
  return (iErrorCode);
}

/*****************************************************************************
* Function:     xsvfDoXSDR
* Description:  XSDR <lenVal.TDI[XSDRSIZE]>
*               Shift the given TDI data into the JTAG scan chain.
*               Compare the captured TDO with the expected TDO from the
*               previous XSDRTDO command using the previously specified
*               XTDOMASK.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXSDR (SXsvfInfo * pXsvfInfo)
{
  int iErrorCode;
  LVReadValue (&(pXsvfInfo->lvTdi), pXsvfInfo->sShiftLengthBytes);
  /* use TDOExpected from last XSDRTDO instruction */
  iErrorCode = xsvfShift (&(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTDR,
			  pXsvfInfo->lShiftLengthBits, &(pXsvfInfo->lvTdi),
			  &(pXsvfInfo->lvTdoCaptured),
			  &(pXsvfInfo->lvTdoExpected),
			  &(pXsvfInfo->lvTdoMask), pXsvfInfo->ucEndDR,
			  pXsvfInfo->lRunTestTime, pXsvfInfo->ucMaxRepeat);
  if (iErrorCode != XSVF_ERROR_NONE)
    {
      pXsvfInfo->iErrorCode = iErrorCode;
    }
  return (iErrorCode);
}

/*****************************************************************************
* Function:     xsvfDoXRUNTEST
* Description:  XRUNTEST <uint32>
*               Prespecify the XRUNTEST wait time for shift operations.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXRUNTEST (SXsvfInfo * pXsvfInfo)
{
  LVReadValue (&(pXsvfInfo->lvTdi), 4);
  pXsvfInfo->lRunTestTime = LVValue (&(pXsvfInfo->lvTdi));
  return (XSVF_ERROR_NONE);
}

/*****************************************************************************
* Function:     xsvfDoXREPEAT
* Description:  XREPEAT <byte>
*               Prespecify the maximum number of XC9500/XL retries.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXREPEAT (SXsvfInfo * pXsvfInfo)
{
  EEGetNextByte (&(pXsvfInfo->ucMaxRepeat));
  return (XSVF_ERROR_NONE);
}

/*****************************************************************************
* Function:     xsvfDoXSDRSIZE
* Description:  XSDRSIZE <uint32>
*               Prespecify the XRUNTEST wait time for shift operations.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXSDRSIZE (SXsvfInfo * pXsvfInfo)
{
  int iErrorCode;
  iErrorCode = XSVF_ERROR_NONE;
  LVReadValue (&(pXsvfInfo->lvTdi), 4);
  pXsvfInfo->lShiftLengthBits = LVValue (&(pXsvfInfo->lvTdi));
  pXsvfInfo->sShiftLengthBytes =
    xsvfGetAsNumBytes (pXsvfInfo->lShiftLengthBits);
  if (pXsvfInfo->sShiftLengthBytes > MAX_LEN)
    {
      iErrorCode = XSVF_ERROR_DATAOVERFLOW;
      //cmn_err((CE_WARN,"xsvfDoXSDRSIZE: XSVF_ERROR_DATAOVERFLOW\n"));
      pXsvfInfo->iErrorCode = iErrorCode;
    }
  return (iErrorCode);
}

/*****************************************************************************
* Function:     xsvfDoXSDRTDO
* Description:  XSDRTDO <lenVal.TDI[XSDRSIZE]> <lenVal.TDO[XSDRSIZE]>
*               Get the TDI and expected TDO values.  Then, shift.
*               Compare the expected TDO with the captured TDO using the
*               prespecified XTDOMASK.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXSDRTDO (SXsvfInfo * pXsvfInfo)
{
  int iErrorCode;
  iErrorCode = xsvfBasicXSDRTDO (&(pXsvfInfo->ucTapState),
				 pXsvfInfo->lShiftLengthBits,
				 pXsvfInfo->sShiftLengthBytes,
				 &(pXsvfInfo->lvTdi),
				 &(pXsvfInfo->lvTdoCaptured),
				 &(pXsvfInfo->lvTdoExpected),
				 &(pXsvfInfo->lvTdoMask),
				 pXsvfInfo->ucEndDR,
				 pXsvfInfo->lRunTestTime,
				 pXsvfInfo->ucMaxRepeat);
  if (iErrorCode != XSVF_ERROR_NONE)
    {
      pXsvfInfo->iErrorCode = iErrorCode;
    }
  return (iErrorCode);
}

/*****************************************************************************
* Function:     xsvfDoXSDRBCE
* Description:  XSDRB/XSDRC/XSDRE <lenVal.TDI[XSDRSIZE]>
*               If not already in SHIFTDR, goto SHIFTDR.
*               Shift the given TDI data into the JTAG scan chain.
*               Ignore TDO.
*               If cmd==XSDRE, then goto ENDDR.  Otherwise, stay in ShiftDR.
*               XSDRB, XSDRC, and XSDRE are the same implementation.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXSDRBCE (SXsvfInfo * pXsvfInfo)
{
  BYTE ucEndDR;
  int iErrorCode;
  ucEndDR = (BYTE) ((pXsvfInfo->ucCommand == XSDRE) ?
		    pXsvfInfo->ucEndDR : XTAPSTATE_SHIFTDR);
  iErrorCode = xsvfBasicXSDRTDO (&(pXsvfInfo->ucTapState),
				 pXsvfInfo->lShiftLengthBits,
				 pXsvfInfo->sShiftLengthBytes,
				 &(pXsvfInfo->lvTdi), 0, 0, 0, ucEndDR, 0, 0);
  if (iErrorCode != XSVF_ERROR_NONE)
    {
      pXsvfInfo->iErrorCode = iErrorCode;
    }
  return (iErrorCode);
}

/*****************************************************************************
* Function:     xsvfDoXSDRTDOBCE
* Description:  XSDRB/XSDRC/XSDRE <lenVal.TDI[XSDRSIZE]> <lenVal.TDO[XSDRSIZE]>
*               If not already in SHIFTDR, goto SHIFTDR.
*               Shift the given TDI data into the JTAG scan chain.
*               Compare TDO, but do NOT use XTDOMASK.
*               If cmd==XSDRTDOE, then goto ENDDR.  Otherwise, stay in ShiftDR.
*               XSDRTDOB, XSDRTDOC, and XSDRTDOE are the same implementation.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXSDRTDOBCE (SXsvfInfo * pXsvfInfo)
{
  BYTE ucEndDR;
  int iErrorCode;
  ucEndDR = (BYTE) ((pXsvfInfo->ucCommand == XSDRTDOE) ?
		    pXsvfInfo->ucEndDR : XTAPSTATE_SHIFTDR);
  iErrorCode = xsvfBasicXSDRTDO (&(pXsvfInfo->ucTapState),
				 pXsvfInfo->lShiftLengthBits,
				 pXsvfInfo->sShiftLengthBytes,
				 &(pXsvfInfo->lvTdi),
				 &(pXsvfInfo->lvTdoCaptured),
				 &(pXsvfInfo->lvTdoExpected),
				 0, ucEndDR, 0, 0);
  if (iErrorCode != XSVF_ERROR_NONE)
    {
      pXsvfInfo->iErrorCode = iErrorCode;
    }
  return (iErrorCode);
}

/*****************************************************************************
* Function:     xsvfDoXSTATE
* Description:  XSTATE <byte>
*               <byte> == XTAPSTATE;
*               Get the state parameter and transition the TAP to that state.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int
CEEPROM::xsvfDoXSTATE (SXsvfInfo * pXsvfInfo)
{
  BYTE ucNextState;
  int iErrorCode;
  EEGetNextByte (&ucNextState);
  iErrorCode = xsvfGotoTapState (&(pXsvfInfo->ucTapState), ucNextState);
  if (iErrorCode != XSVF_ERROR_NONE)
    {
      pXsvfInfo->iErrorCode = iErrorCode;
    }
  return (iErrorCode);
}

/*****************************************************************************
* Description:  Run the xsvf player for a single command and return.
*               First, call xsvfInitialize.
*               Then, repeatedly call this function until an error is detected
*               or until the pXsvfInfo->ucComplete variable is non-zero.
*               Finally, call xsvfCleanup to cleanup any remnants.
* Parameters:   pXsvfInfo   - ptr to the XSVF information.
* Returns:      int         - 0 = success; otherwise error.
*****************************************************************************/
int
CEEPROM::xsvfRun (SXsvfInfo * pXsvfInfo)
{
  /* Process the XSVF commands */
  if ((!pXsvfInfo->iErrorCode) && (!pXsvfInfo->ucComplete))
    {
      /* read 1 byte for the instruction */
      EEGetNextByte (&(pXsvfInfo->ucCommand));
      ++(pXsvfInfo->lCommandCount);

      if (pXsvfInfo->ucCommand < XLASTCMD)
	{
	  switch (pXsvfInfo->ucCommand)
	    {
	    case 0:
	      xsvfDoXCOMPLETE (pXsvfInfo);
	      break;
	    case 1:
	      xsvfDoXTDOMASK (pXsvfInfo);
	      break;
	    case 2:
	      xsvfDoXSIR (pXsvfInfo);
	      break;
	    case 3:
	      xsvfDoXSDR (pXsvfInfo);
	      break;
	    case 4:
	      xsvfDoXRUNTEST (pXsvfInfo);
	      break;
	    case 7:
	      xsvfDoXREPEAT (pXsvfInfo);
	      break;
	    case 8:
	      xsvfDoXSDRSIZE (pXsvfInfo);
	      break;
	    case 9:
	      xsvfDoXSDRTDO (pXsvfInfo);
	      break;
	    case 12:
	      xsvfDoXSDRBCE (pXsvfInfo);
	      break;
	    case 13:
	      xsvfDoXSDRBCE (pXsvfInfo);
	      break;
	    case 14:
	      xsvfDoXSDRBCE (pXsvfInfo);
	      break;
	    case 15:
	      xsvfDoXSDRTDOBCE (pXsvfInfo);
	      break;
	    case 16:
	      xsvfDoXSDRTDOBCE (pXsvfInfo);
	      break;
	    case 17:
	      xsvfDoXSDRTDOBCE (pXsvfInfo);
	      break;
	    case 18:
	      xsvfDoXSTATE (pXsvfInfo);
	      break;
	    default:
	      pXsvfInfo->iErrorCode = XSVF_ERROR_ILLEGALCMD;
	      break;
	    }
	}
      else
	{
	  pXsvfInfo->iErrorCode = XSVF_ERROR_ILLEGALCMD;
	}
    }

  return (pXsvfInfo->iErrorCode);
}

/////////////////////////////////////////////////////////////////////////////
int
CEEPROM::xsvfExecute (pSXsvfInfo pXsvfInfo)
/////////////////////////////////////////////////////////////////////////////
{
  // Initialize values
  pXsvfInfo->ucComplete = 0;
  pXsvfInfo->ucCommand = XCOMPLETE;
  pXsvfInfo->lCommandCount = 0;
  pXsvfInfo->ucMaxRepeat = 0;
  pXsvfInfo->ucTapState = XTAPSTATE_RESET;
  pXsvfInfo->ucEndIR = XTAPSTATE_RUNTEST;
  pXsvfInfo->ucEndDR = XTAPSTATE_RUNTEST;
  pXsvfInfo->lShiftLengthBits = 0L;
  pXsvfInfo->sShiftLengthBytes = 0;
  pXsvfInfo->lRunTestTime = 0L;

  // Initialize the TAPs
  pXsvfInfo->iErrorCode =
    xsvfGotoTapState (&(pXsvfInfo->ucTapState), XTAPSTATE_RESET);

  while (!pXsvfInfo->iErrorCode && (!pXsvfInfo->ucComplete))
    {
      xsvfRun (pXsvfInfo);
    }

  return (pXsvfInfo->iErrorCode);
}

/////////////////////////////////////////////////////////////////////////////
void
CEEPROM::EEPulseClock ()
//      toggle tck LHL
/////////////////////////////////////////////////////////////////////////////
{
  EESetPort (TCK, 0);		// set the TCK port to low  - Set TDI & TMS 
  EESetPort (TCK, 1);		// set the TCK port to high - Latch in TDI & TMS 
  EESetPort (TCK, 0);		// set the TCK port to low  - Latch out TDO 
}

#define WAIT_TIME	5	// in microseconds

/////////////////////////////////////////////////////////////////////////////
void
CEEPROM::EESetPort (SHORT p, SHORT val)
/////////////////////////////////////////////////////////////////////////////
{
  if (p == TMS)			// bit 5
    {
      if (val)
	SET (m_ulControlReg, kBit5);
      else
	CLR (m_ulControlReg, kBit5);
    }
  if (p == TDI)			// bit 4
    {
      if (val)
	SET (m_ulControlReg, kBit4);
      else
	CLR (m_ulControlReg, kBit4);
    }
  if (p == TCK)			// bit 6
    {
      if (val)
	SET (m_ulControlReg, kBit6);
      else
	CLR (m_ulControlReg, kBit6);

      //cmn_err((CE_WARN,"[%08lx] ", gulControlReg )); 
      WRITE_REGISTER_ULONG (m_pL2ControlReg, m_ulControlReg);
      EEWaitTime (WAIT_TIME);
    }
}

/////////////////////////////////////////////////////////////////////////////
BYTE
CEEPROM::EEReadTDOBit ()
//      read the TDO bit from port
// only called from xsvfShiftOnly
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulValue;
  // Bit 7 in MISTAT register
  ulValue = READ_REGISTER_ULONG (m_pL2StatusReg);
  //cmn_err((CE_WARN,"%08lx ", ulValue ));
  return ((BYTE) ((ulValue & kBit7) >> 7));
}

/////////////////////////////////////////////////////////////////////////////
void
CEEPROM::EEWaitTime (LONG lMicroSeconds)
//      Wait at least the specified number of microsec.                           
/////////////////////////////////////////////////////////////////////////////
{
  ULONG ulDummy;		// keep the compiler from complaining
  // each PCI read takes 330ns best case (fastest).  It takes 3 reads per microsecond
  LONG lCount = lMicroSeconds * 10;

  while (lCount--)
    {
      ulDummy = READ_REGISTER_ULONG (m_pL2ControlReg);
    }
}

/////////////////////////////////////////////////////////////////////////////
void
CEEPROM::EEGetNextByte (PBYTE pucByte)
//      read in a byte of data from the prom
/////////////////////////////////////////////////////////////////////////////
{
  *pucByte = (BYTE) * (m_pCurrentBufferBase + m_ulCurrentOffset);
  m_ulCurrentOffset++;
}

//////////////////////////////////////////////////////////////////////////////
CEEPROM::CEEPROM (PHALADAPTER pHalAdapter, PBYTE pucBuffer)
//////////////////////////////////////////////////////////////////////////////
{
  m_pHalAdapter = pHalAdapter;

  m_pCurrentBufferBase = pucBuffer;
  m_ulCurrentOffset = 0;

  PLYNXTWOREGISTERS pL2Registers;
  pL2Registers = m_pHalAdapter->GetRegisters ();

  m_pL2ControlReg = &pL2Registers->PCICTL;
  m_pL2StatusReg = &pL2Registers->MISTAT;

  m_ulControlReg = 0;

  // Initialize the I/O.  SetPort initializes I/O on first call 
  EESetPort (TMS, 1);
}

//////////////////////////////////////////////////////////////////////////////
CEEPROM::~CEEPROM ()
//////////////////////////////////////////////////////////////////////////////
{
  // Disable PCI Configuration and put the register back to how it should be...
  WRITE_REGISTER_ULONG (m_pL2ControlReg, 0);
}

//////////////////////////////////////////////////////////////////////////////
BOOLEAN
EEPROMGetSerialNumber (PHALADAPTER pHalAdapter, PULONG pulSerialNumber)
//////////////////////////////////////////////////////////////////////////////
{
  BOOLEAN bFail = FALSE;
  BYTE ucL2IDRead[] = {
    0x07, 0x08, 0x12, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0D, 0x1F,
    0xFF, 0x02, 0x0D, 0x1F,
    0xDF, 0x08, 0x00, 0x00, 0x00, 0x21, 0x01, 0x00, 0x01, 0xF1, 0xFF, 0xFF,
    0x09, 0x01, 0xFF, 0xFF,
    0xFF, 0xFE, 0x00, 0x0A, 0x04, 0xA1, 0x26, 0x02, 0x0D, 0x1F, 0xFF, 0x02,
    0x0D, 0x1D, 0x1F, 0x08,
    0x00, 0x00, 0x00, 0x07, 0x01, 0x00, 0x09, 0x08, 0x00, 0x08, 0x00, 0x00,
    0x00, 0x11, 0x01, 0x00,
    0x00, 0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0D, 0x1F,
    0xBF, 0x08, 0x00, 0x00,
    0x00, 0x21, 0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x09, 0x01, 0xFF, 0xFF,
    0xFF, 0xFE, 0x00, 0xFF,
    0xFF, 0xFF, 0xFF, 0x02, 0x0D, 0x1F, 0xFF, 0x00
  };

#if defined(WINNT) || defined(WDM)
  __try				// DAH: I am very suspicious of the Xilinx code, so I'm wrapping it in an exception handler.
    // This probably won't work on the Mac.
#endif
  {
    SXsvfInfo xsvfInfo;
    CEEPROM *pEEPROM;

    RtlZeroMemory (&xsvfInfo, sizeof (SXsvfInfo));

    pEEPROM = new CEEPROM (pHalAdapter, ucL2IDRead);
    if (pEEPROM)
      {
	// Execute the XSVF in the file 
	pEEPROM->xsvfExecute (&xsvfInfo);

	delete pEEPROM;
      }

    if ((xsvfInfo.iErrorCode != 0) && (xsvfInfo.lCommandCount == 20) &&	// 21th cmd on file (was 20) (changed back to 20 on 12/23/2004)
	(xsvfInfo.lvTdoExpected.len == 5))
      {
	*pulSerialNumber = (ULONG) LVValue (&xsvfInfo.lvTdoCaptured) >> 1;
      }
    else
      {
#ifdef DEBUG
	//USHORT usDeviceID = (USHORT)READ_REGISTER_ULONG( &((PLYNXTWOREGISTERS)pL2Registers)->PDBlock.DeviceID );
	//cmn_err((CE_WARN,"EEPROMGetSerialNumber Failed! %x Err [%d] Count [%ld] Len [%d]\n", usDeviceID, xsvfInfo.iErrorCode, xsvfInfo.lCommandCount, xsvfInfo.lvTdoExpected.len ));
#endif
	bFail = TRUE;
      }
  }
#if defined(WINNT) || defined(WDM)
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    cmn_err (CE_WARN, "EEPROMGetSerialNumber Exception Handler Failed!\n");
    return (TRUE);
  }
#endif

  return (bFail);
}
