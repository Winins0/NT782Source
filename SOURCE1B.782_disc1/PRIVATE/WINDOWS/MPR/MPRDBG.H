/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    mprdbg.h 

Abstract:

    Contains definitions used in debugging the messenger service.

Author:

    Dan Lafferty    (danl)  07-Oct-1991

Environment:

    User Mode -Win32

Revision History:

    22-Jul-1992     Danl
        Added different debug macros based on the number of parameters.
        Make the macros resolve to nothing when DBG is not defined.

--*/

#ifndef _MPRDBG_INCLUDED
#define _MPRDBG_INCLUDED

//
// Information levels used in switch statements.
//
#define LEVEL_0     0L
#define LEVEL_1     1L
#define LEVEL_2     2L

//
// Debug macros and constants.
//
#if DBG

#define DEBUG_STATE 1
#define STATIC

#else

#define DEBUG_STATE 0
#define STATIC static

#endif

extern DWORD    MprDebugLevel;

//
// The following allow debug print syntax to look like:
//
//   MPR_LOG(DEBUG_TRACE, "An error occured %x\n",status)
//

#if DBG

//
// debugging macros.
//
#define MPR_LOG0(level,string)                  \
    if( MprDebugLevel & (DEBUG_ ## level)){     \
        (VOID) DbgPrint("[MPR] ");              \
        (VOID) DbgPrint(string);                \
    }
#define MPR_LOG1(level,string,var)              \
    if( MprDebugLevel & (DEBUG_ ## level)){     \
        (VOID) DbgPrint("[MPR] ");              \
        (VOID) DbgPrint(string,var);            \
    }
#define MPR_LOG2(level,string,var1,var2)        \
    if( MprDebugLevel & (DEBUG_ ## level)){     \
        (VOID) DbgPrint("[MPR] ");              \
        (VOID) DbgPrint(string,var1,var2);      \
    }
#define MPR_LOG3(level,string,var1,var2,var3)   \
    if( MprDebugLevel & (DEBUG_ ## level)){     \
        (VOID) DbgPrint("[MPR] ");              \
        (VOID) DbgPrint(string,var1,var2,var3); \
    }
#define MPR_LOG(level,string,var)               \
    if( MprDebugLevel & (DEBUG_ ## level)){     \
        (VOID) DbgPrint("[MPR] ");              \
        (VOID) DbgPrint(string,var);            \
    }

#else  // DBG

#define MPR_LOG0(level,string)
#define MPR_LOG1(level,string,var)
#define MPR_LOG2(level,string,var1,var2)
#define MPR_LOG3(level,string,var1,var2,var3)
#define MPR_LOG(level,string,var)

#endif // DBG



#define DEBUG_NONE      0x00000000
#define DEBUG_ERROR     0x00000001
#define DEBUG_TRACE     0x00000002      // Miscellaneous trace info
#define DEBUG_LOCKS     0x00000004      // Multi-thread data locks
#define DEBUG_PS        0x00000008      // Thread and Process information
#define DEBUG_RESTORE   0x00000010      // Restore Connection information
#define DEBUG_CNOTIFY   0x00000020      // Connection Notify information

#define DEBUG_ALL       0xffffffff

#endif // _MPRDBG_INCLUDED

//
// Function Prototypes
//

VOID
PrintKeyInfo(
    HKEY  key);


