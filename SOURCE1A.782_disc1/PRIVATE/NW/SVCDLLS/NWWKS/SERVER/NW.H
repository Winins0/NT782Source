/*++

Copyright (c) 1992-1993  Microsoft Corporation

Module Name:

    nw.h

Abstract:

    Main header of the NetWare Workstation service included by all
    modules.

Author:

    Rita Wong      (ritaw)      11-Dec-1992

Environment:

    User Mode -Win32

Revision History:

--*/

#ifndef _NW_INCLUDED_
#define _NW_INCLUDED_

//
// Includes
//
#include <wcstr.h>
#include <string.h>

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <winsvc.h>
#include <winreg.h>
#include <winspool.h>

#include <align.h>
#include <nwcanon.h>
#include <nwpkstr.h>
#include <nwrpcp.h>

#include <rpc.h>
#include <nwwks.h>

#include <nwevent.h>
#include <ntddnwfs.h>
#include <nwsvc.h>

#define NW_DRIVER_NAME       DD_NWFS_FILESYS_NAME_U


//
// Debug trace level bits for turning on/off trace statements in the
// Workstation service
//

//
// Initialization and reading info from registry
//
#define NW_DEBUG_INIT         0x00000001

//
// Connection APIs
//
#define NW_DEBUG_CONNECT      0x00000002

//
// Enumeration APIs
//
#define NW_DEBUG_ENUM         0x00000004

//
// Credential management APIs
//
#define NW_DEBUG_LOGON        0x00000008

//
// Queue management APIs
//
#define NW_DEBUG_QUEUE        0x00000010

//
// Print Provider APIs
//
#define NW_DEBUG_PRINT        0x00000020

//
// Calls to redirector
//
#define NW_DEBUG_DEVICE       0x00000040

//
// Message APIs
//
#define NW_DEBUG_MESSAGE      0x00000080

#if DBG

extern DWORD WorkstationTrace;

#define IF_DEBUG(DebugCode) if (WorkstationTrace & NW_DEBUG_ ## DebugCode)

#define STATIC

#else

#define IF_DEBUG(DebugCode) if (FALSE)

#define STATIC static

#endif // DBG

//
// Initialization states
//
#define NW_EVENTS_CREATED         0x00000001
#define NW_RDR_INITIALIZED        0x00000002
#define NW_BOUND_TO_TRANSPORTS    0x00000004
#define NW_RPC_SERVER_STARTED     0x00000008
#define NW_INITIALIZED_MESSAGE    0x00000010
#define NW_GATEWAY_LOGON          0x00000020

//
// Key path to redirector driver entry
//
#define SERVICE_REGISTRY_KEY L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\"


//
// Event that will be signaled when the service is stopping
//
extern HANDLE NwDoneEvent;

//
// Events for controlling popups, and the global popup data.
//
extern HANDLE NwPopupEvent;
extern HANDLE NwPopupDoneEvent;

typedef struct _NWWKS_POPUP_DATA {
    DWORD  MessageId ;
    DWORD  InsertCount ;
    LPWSTR InsertStrings[10] ;
} NWWKS_POPUP_DATA, *LPNWWKS_POPUP_DATA ;

extern NWWKS_POPUP_DATA PopupData ;

//
// Name of the network provider and print provider
//
extern WCHAR NwProviderName[];
extern DWORD NwPacketBurstSize;
extern DWORD NwPrintOption;
extern DWORD NwGatewayPrintOption;
extern BOOL  GatewayLoggedOn ;
extern BOOL  GatewayConnectionAlways ;

//
// Functions from device.c
//
DWORD
NwInitializeRedirector(
    VOID
    );

DWORD
NwOpenRedirector(
    VOID
    );

DWORD
NwShutdownRedirector(
    VOID
    );

DWORD
NwLoadOrUnloadDriver(
    BOOL Load
    );

DWORD
NwBindToTransports(
    VOID
    );

DWORD
NwOpenPreferredServer(
    PHANDLE ServerHandle
    );

VOID
NwInitializePrintProvider(
    VOID
    );

VOID
NwTerminatePrintProvider(
    VOID
    );

VOID
NwInitializeServiceProvider(
    VOID
    );

VOID
NwTerminateServiceProvider(
    VOID
    );

DWORD
NwRedirFsControl(
    IN  HANDLE FileHandle,
    IN  ULONG RedirControlCode,
    IN  PNWR_REQUEST_PACKET Rrp,
    IN  ULONG RrpLength,
    IN  PVOID SecondBuffer OPTIONAL,
    IN  ULONG SecondBufferLength,
    OUT PULONG Information OPTIONAL
    );

DWORD
NwCreateTreeConnectName(
    IN  LPWSTR UncName,
    IN  LPWSTR LocalName OPTIONAL,
    OUT PUNICODE_STRING TreeConnectStr
    );

DWORD
NwOpenCreateConnection(
    IN PUNICODE_STRING TreeConnectionName,
    IN LPWSTR UserName OPTIONAL,
    IN LPWSTR Password OPTIONAL,
    IN LPWSTR UncName,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG CreateDisposition,
    IN ULONG CreateOptions,
    IN ULONG ConnectionType,
    OUT PHANDLE TreeConnectionHandle,
    OUT PULONG Information OPTIONAL
    );

DWORD
NwNukeConnection(
    IN HANDLE TreeConnection,
    IN DWORD UseForce
    );

DWORD
NwGetServerResource(
    IN LPWSTR LocalName,
    IN DWORD LocalNameLength,
    OUT LPWSTR RemoteName,
    IN DWORD RemoteNameLen,
    OUT LPDWORD CharsRequired
    );

DWORD
NwEnumerateConnections(
    IN OUT LPDWORD ResumeId,
    IN DWORD EntriesRequested,
    IN LPBYTE Buffer,
    IN DWORD BufferSize,
    OUT LPDWORD BytesNeeded,
    OUT LPDWORD EntriesRead,
    IN DWORD ConnectionType
    );

DWORD
NwGetNextServerEntry(
    IN HANDLE PreferredServer,
    IN OUT LPDWORD LastObjectId,
    OUT LPSTR ServerName
    );


DWORD
NwGetNextVolumeEntry(
    IN HANDLE ServerConnection,
    IN DWORD LastObjectId,
    OUT LPSTR VolumeName
    );

DWORD
NwRdrLogonUser(
    IN PLUID LogonId,
    IN LPWSTR UserName,
    IN DWORD UserNameSize,
    IN LPWSTR Password OPTIONAL,
    IN DWORD PasswordSize,
    IN LPWSTR PreferredServer OPTIONAL,
    IN DWORD PreferredServerSize
    );

VOID
NwRdrChangePassword(
    IN PNWR_REQUEST_PACKET Rrp
    );

DWORD
NwRdrSetInfo(
    IN DWORD  PrintOption,
    IN DWORD  PacketBurstSize,
    IN LPWSTR PreferredServer,
    IN DWORD  PreferredServerSize,
    IN LPWSTR ProviderName,
    IN DWORD  ProviderNameSize 
    );

DWORD
NwRdrLogoffUser(
    IN PLUID LogonId
    );

DWORD
NwConnectToServer(
    IN LPWSTR ServerName
    );

NTSTATUS
NwOpenHandle(
    IN PUNICODE_STRING ObjectName,
    IN BOOL ValidateFlag,
    OUT PHANDLE ObjectHandle
    );

NTSTATUS
NwCallNtOpenFile(
    OUT PHANDLE ObjectHandle,
    IN ACCESS_MASK DesiredAccess,
    IN PUNICODE_STRING ObjectName,
    IN ULONG OpenOptions
    );

//
// Functions from queue.c
//
DWORD
NwGetNextQueueEntry(
    IN HANDLE PreferredServer,
    IN OUT LPDWORD LastObjectId,
    OUT LPSTR QueueName
    );

DWORD
NwAttachToNetwareServer(
    IN  LPWSTR  ServerName,
    OUT LPHANDLE phandleServer
    );

//
// Functions from enum.c
//
DWORD
NwOpenEnumPrintServers(
    OUT LPNWWKSTA_CONTEXT_HANDLE EnumHandle
    );

DWORD
NwOpenEnumPrintQueues(
    IN LPWSTR ServerName,
    OUT LPNWWKSTA_CONTEXT_HANDLE EnumHandle
    );

DWORD
NwWriteNetResourceEntry(
    IN OUT LPBYTE *FixedPortion,
    IN OUT LPWSTR *EndOfVariableData,
    IN LPWSTR ContainerName OPTIONAL,
    IN LPWSTR LocalName OPTIONAL,
    IN LPWSTR RemoteName,
    IN DWORD ScopeFlags,
    IN DWORD DisplayFlags,
    IN DWORD UsageFlags,
    IN DWORD ResourceType,
    OUT LPDWORD BytesNeeded
    );

DWORD
NwWritePrinterInfoEntry(
    IN OUT LPBYTE *FixedPortion,
    IN OUT LPWSTR *EndOfVariableData,
    IN LPWSTR ContainerName OPTIONAL,
    IN LPWSTR RemoteName,
    IN DWORD  Flags,
    OUT LPDWORD BytesNeeded
    );

//
// Functions from credentl.c
//
VOID
NwInitializeLogon(
    VOID
    );

VOID
NwGetLogonCredential(
    VOID
    );

DWORD
NwGatewayLogon(
    VOID
    );

//
// Functions from util.c
//
DWORD
NwMapStatus(
    IN  NTSTATUS NtStatus
    );

DWORD
NwMapBinderyCompletionCode(
    IN  NTSTATUS NtStatus
    );

DWORD
NwImpersonateClient(
    VOID
    );

DWORD
NwRevertToSelf(
    VOID
    );

VOID
NwLogEvent(
    DWORD MessageId,
    DWORD NumberOfSubStrings,
    LPWSTR *SubStrings,
    DWORD ErrorCode
    );

BOOL
NwConvertToUnicode(
    OUT LPWSTR *UnicodeOut,
    IN LPSTR  OemIn
    );

VOID
DeleteAllConnections(
    VOID
    );
DWORD

//
// Functions from connect.c
//
NwCreateSymbolicLink(
    IN  LPWSTR Local,
    IN  LPWSTR TreeConnectStr
    );

VOID
NwDeleteSymbolicLink(
    IN  LPWSTR LocalDeviceName,
    IN  LPWSTR TreeConnectStr
    );

DWORD
NwOpenHandleToDeleteConn(
    IN LPWSTR UncName,
    IN LPWSTR LocalName OPTIONAL,
    IN DWORD UseForce,
    IN BOOL IsStopWksta
    );


//
// Functions from gateway.c
//
DWORD
NwEnumerateGWDevices( 
    LPDWORD Index,
    LPBYTE Buffer,
    DWORD BufferSize,
    LPDWORD BytesNeeded,
    LPDWORD EntriesRead
    ) ;

DWORD
NwCreateGWDevice( 
    LPWSTR DeviceName,
    LPWSTR RemoteName,
    DWORD  Flags
    ) ;

DWORD
NwRemoveGWDevice( 
    LPWSTR DeviceName,
    DWORD  Flags
    ) ;

DWORD
NwQueryGWAccount(
    LPWSTR   AccountName,
    DWORD    AccountNameLen,
    LPDWORD  AccountCharsNeeded,
    LPWSTR   Password,
    DWORD    PasswordLen,
    LPDWORD  PasswordCharsNeeded
    ) ;

DWORD
NwSetGWAccount(
    LPWSTR AccountName,
    LPWSTR Password
    ) ;

DWORD
NwGetGatewayResource(
    IN LPWSTR LocalName,
    OUT LPWSTR RemoteName,
    IN DWORD RemoteNameLen,
    OUT LPDWORD CharsRequired
    );

DWORD
NwCreateRedirections(
    LPWSTR Account,
    LPWSTR Password
    );

DWORD
NwDeleteRedirections(
    VOID
    );

DWORD
NwCreateGWConnection(
    IN LPWSTR RemoteName,
    IN LPWSTR UserName,
    IN LPWSTR Password,
    IN BOOL   KeepConnection
    );

DWORD
NwDeleteGWConnection(
    IN LPWSTR RemoteName
    );


#endif // _NW_INCLUDED_
