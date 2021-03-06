/*++

Copyright (c) 1991-1992  Microsoft Corporation

Module Name:

    wksstub.c

Abstract:

    Client stubs of the Browser service APIs.

Author:

    Rita Wong (ritaw) 10-May-1991
    Larry Osterman (LarryO) 23-Mar-1992

Environment:

    User Mode - Win32

Revision History:

    18-Jun-1991 JohnRo
        Remote NetUse APIs to downlevel servers.
    24-Jul-1991 JohnRo
        Use NET_REMOTE_TRY_RPC etc macros for NetUse APIs.
        Moved NetpIsServiceStarted() into NetLib.
    25-Jul-1991 JohnRo
        Quiet DLL stub debug output.
    19-Aug-1991 JohnRo
        Implement downlevel NetWksta APIs.  Use NetRpc.h for NetWksta APIs.
    07-Nov-1991 JohnRo
        RAID 4186: assert in RxNetShareAdd and other DLL stub problems.
    19-Nov-1991 JohnRo
        Make sure status is correct for APIs not supported on downlevel.
        Implement remote NetWkstaUserEnum().
    09-Nov-1992 JohnRo
        Fix NET_API_FUNCTION references.
        Avoid compiler warnings.
--*/

#include "brclient.h"
#undef IF_DEBUG                 // avoid wsclient.h vs. debuglib.h conflicts.
#include <debuglib.h>           // IF_DEBUG() (needed by netrpc.h).
#include <lmserver.h>
#include <lmsvc.h>
#include <rxuse.h>              // RxNetUse APIs.
#include <rxwksta.h>            // RxNetWksta and RxNetWkstaUser APIs.
#include <rap.h>                // Needed by rxserver.h
#include <rxserver.h>           // RxNetServerEnum API.
#include <netlib.h>             // NetpServiceIsStarted() (needed by netrpc.h).
#include <ntddbrow.h>           // Browser definitions
#include <netrpc.h>             // NET_REMOTE macros.
#include <align.h>
#include <tstr.h>
#include <tstring.h>            // NetpInitOemString().
#include <brcommon.h>           // Routines common between client & server
#include <lmapibuf.h>           // NetApiBufferFree().
#include <lmbrowsr.h>           // Definition of I_BrowserServerEnum
#include <icanon.h>
#include <lmapibuf.h>

//-------------------------------------------------------------------//
//                                                                   //
// Global variables                                                  //
//                                                                   //
//-------------------------------------------------------------------//

#define API_SUCCESS(x)  ((x) == NERR_Success || (x) == ERROR_MORE_DATA)


//-------------------------------------------------------------------//
//                                                                   //
// Global types                                                      //
//                                                                   //
//-------------------------------------------------------------------//



//-------------------------------------------------------------------//
//                                                                   //
// Private routines                                                  //
//                                                                   //
//-------------------------------------------------------------------//


NET_API_STATUS
GetBrowserTransportList(
    OUT PLMDR_TRANSPORT_LIST *TransportList
    );

LPTSTR
GetPreconfiguredRASDomain(
    VOID
    );

NET_API_STATUS
EnumServersForTransport(
    IN PUNICODE_STRING TransportName,
    IN LPTSTR DomainName OPTIONAL,
    IN ULONG level,
    IN ULONG prefmaxlen,
    IN ULONG servertype,
    IN LPTSTR CurrentComputerName,
    OUT PINTERIM_SERVER_LIST InterimServerList,
    IN OUT LPDWORD resume_handle OPTIONAL,
    IN BOOLEAN WannishTransport,
    IN BOOLEAN RasTransport,
    IN BOOLEAN IPXTransport
    );

NET_API_STATUS NET_API_FUNCTION
NetServerEnum(
    IN  LPTSTR      servername OPTIONAL,
    IN  DWORD       level,
    OUT LPBYTE      *bufptr,
    IN  DWORD       prefmaxlen,
    OUT LPDWORD     entriesread,
    OUT LPDWORD     totalentries,
    IN  DWORD       servertype,
    IN  LPTSTR      domain OPTIONAL,
    IN OUT LPDWORD  resume_handle OPTIONAL
    )
/*++

Routine Description:

    This is the DLL entrypoint for NetServerEnum.

Arguments:

    servername - Supplies the name of server to execute this function

    level - Supplies the requested level of information.

    bufptr - Returns a pointer to a buffer which contains the
        requested transport information.

    prefmaxlen - Supplies the number of bytes of information
        to return in the buffer.  If this value is MAXULONG, we will try
        to return all available information if there is enough memory
        resource.

    entriesread - Returns the number of entries read into the buffer.  This
        value is returned only if the return code is NERR_Success or
        ERROR_MORE_DATA.

    totalentries - Returns the total number of entries available.  This value
        is returned only if the return code is NERR_Success or ERROR_MORE_DATA.

    servertype - Supplies the type of server to enumerate.

    domain - Supplies the name of one of the active domains to enumerate the
        servers from.  If NULL, servers from the primary domain, logon domain
        and other domains are enumerated.

    resume_handle - Supplies and returns the point to continue with enumeration.

Return Value:

    NET_API_STATUS - NERR_Success or reason for failure.

--*/
{
    PLMDR_TRANSPORT_LIST TransportList = NULL;
    PLMDR_TRANSPORT_LIST TransportEntry = NULL;
    INTERIM_SERVER_LIST InterimServerList;
    NET_API_STATUS Status;
    DWORD DomainNameSize = 0;
    TCHAR DomainName[DNLEN + 1];

    //
    //
    //  The workstation has to be started for the NetServerEnum API to work.
    //
    //

    if ((Status = CheckForService(SERVICE_WORKSTATION, NULL)) != NERR_Success) {
        return Status;
    }

    if ((servername != NULL) &&
        ( *servername != TCHAR_EOS)) {

        //
        // Call downlevel version of the API
        //

        Status = RxNetServerEnum(
                     servername,
                     NULL,
                     level,
                     bufptr,
                     prefmaxlen,
                     entriesread,
                     totalentries,
                     servertype,
                     domain,
                     resume_handle
                     );

        return Status;
    }

    //
    // Only levels 100 and 101 are valid
    //

    if ((level != 100) && (level != 101)) {
        return ERROR_INVALID_LEVEL;
    }

    if (servertype != SV_TYPE_ALL) {
        if (servertype & SV_TYPE_DOMAIN_ENUM) {
            if (servertype != SV_TYPE_DOMAIN_ENUM) {
                return ERROR_INVALID_FUNCTION;
            }
        }
    }

    if (ARGUMENT_PRESENT(domain)) {

        if ((Status = I_NetNameCanonicalize(
                          NULL,
                          domain,
                          DomainName,
                          (DNLEN + 1) * sizeof(TCHAR),
                          NAMETYPE_WORKGROUP,
                          LM2X_COMPATIBLE
                          )) != NERR_Success) {
            return ERROR_INVALID_PARAMETER;
        }

        DomainNameSize = STRLEN(DomainName) * sizeof(WCHAR);

        domain = DomainName;
    }


    //
    //  Initialize the buffer to a known value.
    //

    *bufptr = NULL;

    *entriesread = 0;

    *totalentries = 0;

    Status = InitializeInterimServerList(&InterimServerList, NULL, NULL, NULL, NULL);

    try {
        BOOL AnyEnumServersSucceeded = FALSE;
        LPTSTR MyComputerName = NULL;

        Status = NetpGetComputerName( &MyComputerName);

        if ( Status != NERR_Success ) {
            goto try_exit;
        }

        //
        //  Retrieve the list of transports from the browser.
        //

        Status = GetBrowserTransportList(&TransportList);

        if (Status != NERR_Success) {
            goto try_exit;
        }

        TransportEntry = TransportList;

        while (TransportEntry != NULL) {
            UNICODE_STRING TransportName;
            ULONG EntriesInList = 0;
            ULONG TotalEntriesInList = 0;

            TransportName.Buffer = TransportEntry->TransportName;
            TransportName.Length = (USHORT)TransportEntry->TransportNameLength;
            TransportName.MaximumLength = (USHORT)TransportEntry->TransportNameLength;

            Status = EnumServersForTransport(&TransportName,
                                             domain,
                                             level,
                                             prefmaxlen,
                                             servertype,
                                             MyComputerName,
                                             &InterimServerList,
                                             resume_handle,
                                             (BOOLEAN)((TransportEntry->Flags & LMDR_TRANSPORT_WANNISH) != 0),
                                             (BOOLEAN)((TransportEntry->Flags & LMDR_TRANSPORT_RAS) != 0),
                                             (BOOLEAN)((TransportEntry->Flags & LMDR_TRANSPORT_IPX) != 0));

            if (API_SUCCESS(Status)) {
                AnyEnumServersSucceeded = TRUE;
            }

            if (TransportEntry->NextEntryOffset == 0) {
                TransportEntry = NULL;
            } else {
                TransportEntry = (PLMDR_TRANSPORT_LIST)((PCHAR)TransportEntry+TransportEntry->NextEntryOffset);
            }

        }

        if ( MyComputerName != NULL ) {
            (void) NetApiBufferFree( MyComputerName );
        }

        if (AnyEnumServersSucceeded) {

            //
            //  Pack the interim server list into its final form.
            //

            Status = PackServerList(&InterimServerList,
                            level,
                            servertype,
                            prefmaxlen,
                            (PVOID *)bufptr,
                            entriesread,
                            totalentries);
        }

try_exit:NOTHING;
    } finally {
        if (TransportList != NULL) {
            MIDL_user_free(TransportList);
        }

        UninitializeInterimServerList(&InterimServerList);
    }

    return Status;
}

NET_API_STATUS
EnumServersForTransport(
    IN PUNICODE_STRING TransportName,
    IN LPTSTR DomainName OPTIONAL,
    IN ULONG level,
    IN ULONG prefmaxlen,
    IN ULONG servertype,
    IN LPTSTR CurrentComputerName,
    OUT PINTERIM_SERVER_LIST InterimServerList,
    IN OUT LPDWORD resume_handle,
    IN BOOLEAN WannishTransport,
    IN BOOLEAN RasTransport,
    IN BOOLEAN IpxTransport
    )
{
    PWSTR *BrowserList = NULL;
    ULONG BrowserListLength = 0;
    NET_API_STATUS Status;
    PVOID ServerList = NULL;
    ULONG EntriesInList = 0;
    ULONG TotalEntriesInList = 0;
    ULONG ServerIndex = 0;

    //
    //  Skip over IPX transports - we can't contact machines over them anyway.
    //

    if (IpxTransport) {
        return NERR_Success;
    }

    //
    //  Retrieve a new browser list.  Do not force a revalidation.
    //

    Status = GetBrowserServerList(TransportName,
                                    DomainName,
                                    &BrowserList,
                                    &BrowserListLength,
                                    FALSE);

    //
    //  If a domain name was specified and we were unable to find the browse
    //  master for the domain and we are running on a wannish transport,
    //  invoke the "double hop" code and allow a local browser server
    //  remote the API to the browse master for that domain (we assume that
    //  this means that the workgroup is on a different subnet of a WAN).
    //

    if (!API_SUCCESS(Status) &&
        DomainName != NULL) {

        Status = GetBrowserServerList(TransportName,
                                    NULL,
                                    &BrowserList,
                                    &BrowserListLength,
                                    FALSE);


    }

    //
    //  Ok, we STILL couldn't find a master browser.  If this is a WAN-ish
    //  transport, then RAS will have seeded the name of the domain that was
    //  used to authenticate when we logged in over RAS in the registry, and
    //  if THAT works,
    //

    if (!API_SUCCESS(Status) &&

        RasTransport &&

        servertype == SV_TYPE_DOMAIN_ENUM &&

        DomainName != NULL) {

        LPTSTR ConfiguredDomain = NULL;

        ConfiguredDomain = GetPreconfiguredRASDomain();

        if (ConfiguredDomain != NULL) {

            Status = GetBrowserServerList(TransportName,
                                    ConfiguredDomain,
                                    &BrowserList,
                                    &BrowserListLength,
                                    FALSE);

            MIDL_user_free(ConfiguredDomain);
        }
    }

    //
    //  If we were able to retrieve the list, remote the API.  Otherwise
    //  return.
    //

    if (API_SUCCESS(Status)) {

        do {
            LPTSTR Transport;
            LPTSTR ServerName;

            //
            // Remote the API to that server.
            //

            Transport = TransportName->Buffer;
            ServerName = BrowserList[0];

            // add 2 to skip double backslash at start of ServerName

            if ( STRICMP(ServerName + 2, CurrentComputerName ) == 0 ) {

                //
                //  If we are going to remote the API to ourselves,
                //  and we are running the browser service, simply
                //  use RPC to get the information we need, don't
                //  bother using the redirector.  This allows us to
                //  avoid tying up RPCXLATE thread.
                //

                Status = I_BrowserServerEnum (
                                NULL,
                                Transport,
                                CurrentComputerName,
                                level,
                                (LPBYTE *)&ServerList,
                                prefmaxlen,
                                &EntriesInList,
                                &TotalEntriesInList,
                                servertype,
                                DomainName,
                                resume_handle
                                );

            } else {

                Status = RxNetServerEnum(
                             ServerName,
                             Transport,
                             level,
                             (LPBYTE *)&ServerList,
                             prefmaxlen,
                             &EntriesInList,
                             &TotalEntriesInList,
                             servertype,
                             DomainName,
                             resume_handle
                             );

            }

            if ( !API_SUCCESS(Status)) {
                NET_API_STATUS GetBListStatus;

                //
                //  If we failed to remote the API for some reason,
                //  we want to regenerate the bowsers list of browser
                //  servers.
                //

                if (BrowserList != NULL) {

                    MIDL_user_free(BrowserList);

                    BrowserList = NULL;
                }


                GetBListStatus = GetBrowserServerList(TransportName,
                                                            DomainName,
                                                            &BrowserList,
                                                            &BrowserListLength,
                                                            TRUE);
                if (GetBListStatus != NERR_Success) {

                    //
                    //  If we were unable to reload the list,
                    //  try the next transport.
                    //

                    break;
                }

                ServerIndex += 1;

                //
                //  If we've looped more times than we got servers
                //  in the list, we're done.
                //

                if ( ServerIndex > BrowserListLength ) {
                    break;
                }

            } else {

                Status = MergeServerList(
                                        InterimServerList,
                                        level,
                                        ServerList,
                                        EntriesInList,
                                        TotalEntriesInList
                                        );

                //
                //  The remote API succeeded.
                //
                //  Now free up the remaining parts of the list.
                //

                if (BrowserList != NULL) {
                    MIDL_user_free(BrowserList);
                    BrowserList = NULL;
                }

                if (ServerList != NULL) {
                    NetApiBufferFree(ServerList);
                    ServerList = NULL;
                }

            }

        } while ( !API_SUCCESS(Status) );

    }

    //
    //  Free up the browser list.
    //

    if (BrowserList != NULL) {
        MIDL_user_free(BrowserList);
        BrowserList = NULL;
    }

    return Status;
}


NET_API_STATUS
GetBrowserTransportList(
    OUT PLMDR_TRANSPORT_LIST *TransportList
    )

/*++

Routine Description:

    This routine returns the list of transports bound into the browser.

Arguments:

    OUT PLMDR_TRANSPORT_LIST *TransportList - Transport list to return.

Return Value:

    NET_API_STATUS - NERR_Success or reason for failure.

--*/

{

    NET_API_STATUS Status;
    HANDLE BrowserHandle;
    LMDR_REQUEST_PACKET RequestPacket;

    Status = OpenBrowser(&BrowserHandle);

    if (Status != NERR_Success) {
        return Status;
    }

    RequestPacket.Version = LMDR_REQUEST_PACKET_VERSION;

    RequestPacket.Type = EnumerateXports;

    RtlInitUnicodeString(&RequestPacket.TransportName, NULL);

    Status = DeviceControlGetInfo(
                BrowserHandle,
                IOCTL_LMDR_ENUMERATE_TRANSPORTS,
                &RequestPacket,
                sizeof(RequestPacket),
                (PVOID *)TransportList,
                0xffffffff,
                4096,
                NULL);

    NtClose(BrowserHandle);

    return Status;
}

NET_API_STATUS
I_BrowserServerEnum (
    IN  LPTSTR      servername OPTIONAL,
    IN  LPTSTR      transport OPTIONAL,
    IN  LPTSTR      clientname OPTIONAL,
    IN  DWORD       level,
    OUT LPBYTE      *bufptr,
    IN  DWORD       prefmaxlen,
    OUT LPDWORD     entriesread,
    OUT LPDWORD     totalentries,
    IN  DWORD       servertype,
    IN  LPTSTR      domain OPTIONAL,
    IN OUT LPDWORD  resume_handle OPTIONAL
    )

/*++

Routine Description:

    This is the DLL entrypoint for NetWkstaSetInfo.

Arguments:

    servername - Supplies the name of server to execute this function

    level - Supplies the level of information.

    buf - Supplies a buffer which contains the information structure of fields
        to set.  The level denotes the structure in this buffer.

    parm_err - Returns the identifier to the invalid parameter in buf if this
        function returns ERROR_INVALID_PARAMETER.

Return Value:

    NET_API_STATUS - NERR_Success or reason for failure.

--*/
{
    NET_API_STATUS status;
    GENERIC_INFO_CONTAINER GenericInfoContainer;
    GENERIC_ENUM_STRUCT InfoStruct;

    GenericInfoContainer.Buffer = NULL;
    GenericInfoContainer.EntriesRead = 0;

    InfoStruct.Container = &GenericInfoContainer;
    InfoStruct.Level = level;

    NET_REMOTE_TRY_RPC

        //
        // Try RPC (local or remote) version of API.
        //

        status = I_BrowserrServerEnum(
                     servername,
                     transport,
                     clientname,
                     (LPSERVER_ENUM_STRUCT)&InfoStruct,
                     prefmaxlen,
                     totalentries,
                     servertype,
                     domain,
                     resume_handle
                     );

        if (status == NERR_Success || status == ERROR_MORE_DATA) {
            *bufptr = (LPBYTE) GenericInfoContainer.Buffer;
            *entriesread = GenericInfoContainer.EntriesRead;

#if 0
            if (((servertype == SV_TYPE_ALL || servertype == SV_TYPE_DOMAIN_ENUM)) &&
                (STRICMP(transport, L"\\Device\\Streams\\NBT"))) {
                if (*entriesread <= 20) {
                    KdPrint(("RPC API Returned EntriesRead == %ld on transport %ws\n", *entriesread, transport));
                }
                if (*totalentries <= 20) {
                    KdPrint(("RPC API Returned TotalEntries == %ld on transport %ws\n", *totalentries, transport));
                }
            }
#endif
        }

    NET_REMOTE_RPC_FAILED("I_BrServerEnum",
            servername,
            status,
            NET_REMOTE_FLAG_NORMAL,
            SERVICE_BROWSER )

        //
        // There is no downlevel version of api.
        //
        status = ERROR_NOT_SUPPORTED;

    NET_REMOTE_END

#if 0
    if ((servertype == SV_TYPE_ALL || servertype == SV_TYPE_DOMAIN_ENUM) &&
        (STRICMP(transport, L"\\Device\\Streams\\NBT"))) {
        if (*entriesread <= 20) {
            KdPrint(("Client API Returned EntriesRead == %ld on transport %ws\n", *entriesread, transport));
        }
        if (*totalentries <= 20) {
            KdPrint(("Client API Returned TotalEntries == %ld on transport %ws\n", *totalentries, transport));
        }
    }
#endif

    return status;
}


NET_API_STATUS NET_API_FUNCTION
I_BrowserQueryOtherDomains (
    IN  LPTSTR      servername OPTIONAL,
    OUT LPBYTE      *bufptr,
    OUT LPDWORD     entriesread,
    OUT LPDWORD     totalentries
    )

/*++

Routine Description:

    This is the DLL entrypoint for NetWkstaSetInfo.

Arguments:

    servername - Supplies the name of server to execute this function

    buf - Supplies a buffer which contains the information structure of fields
        to set.  The level denotes the structure in this buffer.

Return Value:

    NET_API_STATUS - NERR_Success or reason for failure.

--*/
{
    NET_API_STATUS status;
    GENERIC_INFO_CONTAINER GenericInfoContainer;
    GENERIC_ENUM_STRUCT InfoStruct;

    GenericInfoContainer.Buffer = NULL;
    GenericInfoContainer.EntriesRead = 0;

    InfoStruct.Container = &GenericInfoContainer;
    InfoStruct.Level = 100;

    NET_REMOTE_TRY_RPC

        //
        // Try RPC (local or remote) version of API.
        //

        status = I_BrowserrQueryOtherDomains (
                     servername,
                     (LPSERVER_ENUM_STRUCT)&InfoStruct,
                     totalentries
                     );

        if (status == NERR_Success || status == ERROR_MORE_DATA) {
            *bufptr = (LPBYTE) GenericInfoContainer.Buffer;
            *entriesread = GenericInfoContainer.EntriesRead;
        }

    NET_REMOTE_RPC_FAILED("I_BrowserQueryOtherDomains",
            servername,
            status,
            NET_REMOTE_FLAG_NORMAL,
            SERVICE_BROWSER )

        //
        // There is no downlevel version of api.
        //
        status = ERROR_NOT_SUPPORTED;

    NET_REMOTE_END

    return status;
}
NET_API_STATUS
I_BrowserResetNetlogonState (
    IN  LPTSTR      servername OPTIONAL
    )

/*++

Routine Description:

    This is the DLL entrypoint for NetWkstaSetInfo.

Arguments:

    servername - Supplies the name of server to execute this function

    buf - Supplies a buffer which contains the information structure of fields
        to set.  The level denotes the structure in this buffer.

Return Value:

    NET_API_STATUS - NERR_Success or reason for failure.

--*/
{
    NET_API_STATUS status;

    NET_REMOTE_TRY_RPC

        //
        // Try RPC (local or remote) version of API.
        //

        status = I_BrowserrResetNetlogonState (
                     servername
                     );

    NET_REMOTE_RPC_FAILED("I_BrowserResetNetlogonState",
            servername,
            status,
            NET_REMOTE_FLAG_NORMAL,
            SERVICE_BROWSER )

        //
        // There is no downlevel version of api.
        //
        status = ERROR_NOT_SUPPORTED;

    NET_REMOTE_END

    return status;
}


NET_API_STATUS
I_BrowserDebugCall (
    IN  LPTSTR      servername OPTIONAL,
    IN  DWORD DebugCode,
    IN  DWORD Options
    )
{
    NET_API_STATUS status;

    NET_REMOTE_TRY_RPC

        //
        // Try RPC (local or remote) version of API.
        //

        status = I_BrowserrDebugCall(
                     servername,
                     DebugCode,
                     Options
                     );


    NET_REMOTE_RPC_FAILED("I_BrowserDebugCall",
            servername,
            status,
            NET_REMOTE_FLAG_NORMAL,
            SERVICE_BROWSER )

        //
        // There is no downlevel version of api.
        //
        status = ERROR_NOT_SUPPORTED;

    NET_REMOTE_END

    return status;

}

NET_API_STATUS
I_BrowserDebugTrace (
    IN  LPTSTR      servername OPTIONAL,
    IN  PCHAR DebugString
    )
{
    NET_API_STATUS status;

    NET_REMOTE_TRY_RPC

        //
        // Try RPC (local or remote) version of API.
        //

        status = I_BrowserrDebugTrace(
                     servername,
                     DebugString
                     );


    NET_REMOTE_RPC_FAILED("I_BrowserDebugTrace",
            servername,
            status,
            NET_REMOTE_FLAG_NORMAL,
            SERVICE_BROWSER )

        //
        // There is no downlevel version of api.
        //
        status = ERROR_NOT_SUPPORTED;

    NET_REMOTE_END

    return status;

}


NET_API_STATUS
I_BrowserQueryStatistics (
    IN  LPTSTR      servername OPTIONAL,
    IN  OUT LPBROWSER_STATISTICS *Statistics
    )
{
    NET_API_STATUS status;

    NET_REMOTE_TRY_RPC

        //
        // Try RPC (local or remote) version of API.
        //

        status = I_BrowserrQueryStatistics(
                     servername,
                     Statistics
                     );


    NET_REMOTE_RPC_FAILED("I_BrowserQueryStatistics",
            servername,
            status,
            NET_REMOTE_FLAG_NORMAL,
            SERVICE_BROWSER )

        //
        // There is no downlevel version of api.
        //
        status = ERROR_NOT_SUPPORTED;

    NET_REMOTE_END

    return status;

}

NET_API_STATUS
I_BrowserResetStatistics (
    IN  LPTSTR      servername OPTIONAL
    )
{
    NET_API_STATUS status;

    NET_REMOTE_TRY_RPC

        //
        // Try RPC (local or remote) version of API.
        //

        status = I_BrowserrResetStatistics(
                     servername
                     );


    NET_REMOTE_RPC_FAILED("I_BrowserResetStatistics",
            servername,
            status,
            NET_REMOTE_FLAG_NORMAL,
            SERVICE_BROWSER )

        //
        // There is no downlevel version of api.
        //
        status = ERROR_NOT_SUPPORTED;

    NET_REMOTE_END

    return status;

}


NET_API_STATUS
NetBrowserStatisticsGet(
    IN  LPTSTR  ServerName,
    IN  DWORD   Level,
    OUT LPBYTE* Buffer
    )

/*++

Routine Description:

    Wrapper for workstation statistics retrieval routine - either calls the
    client-side RPC function or calls RxNetStatisticsGet to retrieve the
    statistics from a down-level workstation service

Arguments:

    ServerName  - where to remote this function
    Level       - of information required (100, or 101)
    Buffer      - pointer to pointer to returned buffer

Return Value:

    NET_API_STATUS
        Success - NERR_Success
        Failure - ERROR_INVALID_LEVEL
                    Level not 0
                  ERROR_INVALID_PARAMETER
                    Unsupported options requested
                  ERROR_NOT_SUPPORTED
                    Service is not SERVER or WORKSTATION
                  ERROR_ACCESS_DENIED
                    Caller doesn't have necessary access rights for request

--*/

{
    NET_API_STATUS  status;
    GENERIC_INFO_CONTAINER GenericInfoContainer;
    GENERIC_ENUM_STRUCT InfoStruct;

    //
    // set the caller's buffer pointer to known value. This will kill the
    // calling app if it gave us a bad pointer and didn't use try...except
    //

    *Buffer = NULL;

    //
    // validate parms
    //

    if (Level != 100 && Level != 101) {
        return ERROR_INVALID_LEVEL;
    }

    GenericInfoContainer.Buffer = NULL;
    GenericInfoContainer.EntriesRead = 0;

    InfoStruct.Container = &GenericInfoContainer;
    InfoStruct.Level = Level;


    NET_REMOTE_TRY_RPC
        status = NetrBrowserStatisticsGet(ServerName,
                                                Level,
                                                (PBROWSER_STATISTICS_STRUCT)&InfoStruct
                                                );

        if (status == NERR_Success || status == ERROR_MORE_DATA) {
            *Buffer = (LPBYTE) GenericInfoContainer.Buffer;
        }

    NET_REMOTE_RPC_FAILED("NetBrowserStatisticsGet",
                            ServerName,
                            status,
                            NET_REMOTE_FLAG_NORMAL,
                            SERVICE_BROWSER
                            )

        status = ERROR_NOT_SUPPORTED;

    NET_REMOTE_END

    return status;
}

LPTSTR
GetPreconfiguredRASDomain(
    VOID
    )
{

    return NULL;
}
