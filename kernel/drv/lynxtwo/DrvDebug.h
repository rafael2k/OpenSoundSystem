/****************************************************************************
 DrvDebug.h

 Created: David A. Hoatson, March 1998
	
 Copyright © 1998, 1999	Lynx Studio Technology, Inc.

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
	
 Environment: Windows NT Kernel mode

 Revision History
 
 When      Who  Description
 --------- ---  ---------------------------------------------------------------
****************************************************************************/
#ifndef _DRVDEBUG_H
#define _DRVDEBUG_H

#define COLOR_UNDERLINE	1
#define COLOR_NORMAL	7
#define COLOR_BOLD_U	9
#define COLOR_BOLD		15
#define COLOR_REVERSE	120

#ifdef DEBUG

VOID DbgEnter (VOID);
VOID DbgExit (VOID);
VOID DbgPrintMono (char *pszFormat, ...);

typedef struct _GLOBAL_INFO *PGLOBAL_INFO;

NTSTATUS CreateLynxDebug (PGLOBAL_INFO pGDI, PDRIVER_OBJECT pDriverObject,
			  PDEVICE_OBJECT pPhysicalDeviceObject,
			  PDEVICE_OBJECT * ppDeviceObject);
NTSTATUS DeleteLynxDebug (PDEVICE_OBJECT pDeviceObject);
NTSTATUS DebugDispatch (IN PIRP pIrp, IN PIO_STACK_LOCATION pIrpStack);

#define USETRACE
#ifdef USETRACE

BOOLEAN TraceCreate (VOID);
BOOLEAN TraceDelete (VOID);
VOID TraceLogChar (UCHAR cChar);
VOID TraceLogString (char *psz);
VOID TraceLogTime (VOID);
VOID TracePrintF (char *pszFormat, ...);
VOID TracePutChar (char cChar, BYTE ucFormat);
VOID TracePutString (char *szStr, BYTE ucFormat);
VOID TracePutPrintF (char *pszFormat, ...);
VOID TracePutX8 (UCHAR uc8, BYTE ucFormat);
VOID TracePutX16 (USHORT w16, BYTE ucFormat);
VOID TracePutX32 (ULONG dw32, BYTE ucFormat);
VOID TracePrintElapsedTime (char *psz);

BOOLEAN TraceTimedEventCreate (int nEventID, char *pszTitle);
VOID TraceTimedEventStart (int nEventID);
VOID TraceTimedEventStop (int nEventID);
BOOLEAN TraceTimedEventRelease (int nEventID);

#else

// Forward declarations of debug functions
VOID DbgPutCh (char cChar, unsigned char cColor);
VOID DbgPutStr (char *szStr, UCHAR cColor);
VOID DbgPutX8 (UCHAR uc8, UCHAR cColor);
VOID DbgPutX16 (USHORT w16, UCHAR cColor);
VOID DbgPutX32 (ULONG dw32, UCHAR cColor);
VOID DbgPrintMono (char *szFormat, ...);
VOID DbgPutTextXY (char *szStr, UCHAR cColor, UCHAR X, UCHAR Y);
VOID DbgPrintF (const char *pszFormat, ...);
VOID DbgPrintElapsedTime (VOID);

#endif

VOID LEDOn (BYTE ucState);
VOID LEDOff (BYTE ucState);

#define	LED_0	(1<<0)
#define	LED_1	(1<<1)
#define	LED_2	(1<<2)
#define	LED_3	(1<<3)
#define	LED_4	(1<<4)
#define	LED_5	(1<<5)
#define	LED_6	(1<<6)
#define	LED_7	(1<<7)

#ifdef USETRACE
#define DC( a )				TracePutChar( a, COLOR_NORMAL )
#define DB( a, b )			TracePutChar( a, b )
#define DS( a, b )			TracePutString( a, b )
#define DPS( _SZ_ )			TracePutPrintF _SZ_
#define DX8( a, b )			TracePutX8( a, b )
#define DX16( a, b )		TracePutX16( a, b )
#define DX32( a, b )		TracePutX32( a, b )
#define DPET( _SZ_ )		TracePrintElapsedTime( _SZ_ )
#define DPF( _SZ_ )			TracePrintF _SZ_

#define TECreate( ID, _SZ_ ) TraceTimedEventCreate( ID, _SZ_ )
#define TEStart( ID )		TraceTimedEventStart( ID )
#define TEStop( ID )		TraceTimedEventStop( ID )
#define TERelease( ID )		TraceTimedEventRelease( ID )
#else
#ifndef DOS
#define DC( a )				DbgPutCh( a, COLOR_NORMAL )
#define DB( a, b )			DbgPutCh( a, b )
#define DS( a, b )			DbgPutStr( a, b )
#define DPS( _SZ_ )
#define DX8( a, b )			DbgPutX8( a, b )
#define DX16( a, b )		DbgPutX16( a, b )
#define DX32( a, b )		DbgPutX32( a, b )
#define DSXY( a, b, c, d )	DbgPutTextXY( a, b, c, d )
#define DPET( _SZ_ )		DbgPrintElapsedTime()
#else
#define DbgInitialize()
#define DbgClose()

#define DC( a )
#define DB( a, b )
#define DS( a, b )
#define DPS( _SZ_ )
#define DX8( a, b )
#define DX16( a, b )
#define DX32( a, b )
#define DSXY( a, b, c, d )
#define DPET( _SZ_ )

#define TECreate( ID, _SZ_ )
#define TEStart( ID )
#define TEStop( ID )
#define TERelease( ID )
#endif
#endif

#ifdef ALPHA
#define DPF( _SZ_ )		DbgPrint _SZ_
#define DPET( _SZ_ )	DbgPrintElapsedTime()
#endif

#ifdef DOS
#define DPF( _SZ_ )		printf _SZ_
#define DPET( _SZ_ )
#endif

#ifdef MACINTOSH
#define DPF( _SZ_ )	DbgPrintF _SZ_
	//#define DPET()
#endif

#ifndef USETRACE
#ifdef NT
typedef struct _MONO_INFO
{
  //KSPIN_LOCK    DeviceSpinLock;
  KMUTEX DeviceMutex;
} MONO_INFO, *PMONO_INFO;

#define DPF( _SZ_ )		DbgPrintMono _SZ_
#define DPET( _SZ_ )	DbgPrintElapsedTime()
#endif
#endif

#ifndef DPF
#define DPF( _SZ_ )		DbgPrintMono _SZ_
#endif

#else // non-debug
#if defined(sun)
#undef DS
#endif

#define DbgInitialize()
#define DbgClose()

#define DC( a )
#define DB( a, b )
#define DS( a, b )
#define DPS( _SZ_ )
#define DX8( a, b )
#define DX16( a, b )
#define DX32( a, b )
#ifndef DPF
#define DPF(_X_)
#endif
#define DSXY( a, b, c, d )
#define DPET( _SZ_ )

#define TECreate( ID, _SZ_ )
#define TEStart( ID )
#define TEStop( ID )
#define TERelease( ID )
#endif

#endif
