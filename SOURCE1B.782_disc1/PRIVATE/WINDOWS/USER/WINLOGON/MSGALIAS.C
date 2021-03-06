/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    msgalias.c

Abstract:

    This file contains routines for adding and deleting message aliases
    when a user logs on/off.

Author:

    Dan Lafferty (danl)     21-Aug-1992

Environment:

    User Mode -Win32

Revision History:

    21-Aug-1992     danl
        created

--*/
#include "precomp.h"
#pragma hdrstop

#define LPTSTR  LPWSTR




VOID
DeleteMsgAliases(
    VOID
    )

/*++

Routine Description:

    This function removes all message aliases except the ComputerName.

Arguments:

    none

Return Value:

    none

--*/
{
    DWORD           status;
    LPMSG_INFO_0    InfoStruct;
    DWORD           entriesRead;
    DWORD           totalEntries;
    DWORD           resumeHandle = 0;
    DWORD           i;
    WCHAR           ComputerName[MAX_COMPUTERNAME_LENGTH + 1];
    LPWSTR          NewServerName = NULL;
    DWORD           bufLen = MAX_COMPUTERNAME_LENGTH + 1;
    HANDLE          dllHandle;
    PMSG_NAME_DEL   NetMessageNameDel = NULL;
    PMSG_NAME_ENUM  NetMessageNameEnum = NULL;

    //
    // Get the computer name
    //

    if (!GetComputerNameW(ComputerName,&bufLen)) {
        WLPrint(("failed to obtain the computername"));
    }

    //
    // Get the address of the functions we need from netapi32.dll
    //
    dllHandle = LoadLibraryW(L"netapi32.dll");
    if (dllHandle == NULL) {
        return;
    }


    NetMessageNameEnum = (PMSG_NAME_ENUM) GetProcAddress(
                            dllHandle,
                            "NetMessageNameEnum");

    if (NetMessageNameEnum == NULL) {
        FreeLibrary(dllHandle);
        return;
    }

    NetMessageNameDel = (PMSG_NAME_DEL) GetProcAddress(
                            dllHandle,
                            "NetMessageNameDel");

    if (NetMessageNameDel == NULL) {
        FreeLibrary(dllHandle);
        return;
    }

    //
    // Enumerate all the Message Aliases
    //

    status = NetMessageNameEnum (
                NULL,                   // ServerName - Local version
                0,                      // Level
                (LPBYTE *)&InfoStruct,  // return status buffer pointer
                0xffffffff,             // preferred max length
                &entriesRead,           // entries read
                &totalEntries,          // total entries
                &resumeHandle);         // resume handle

    if (status != NERR_Success) {
        // WLPrint(("NetMessageNameEnum failed %d",status));
        FreeLibrary(dllHandle);
        return;
    }

    //
    // Remove the aliases that are not the computername.
    //
    for (i=0; i<entriesRead ;i++) {

        if (wcsicmp(InfoStruct->msgi0_name, ComputerName) != 0) {

            status = NetMessageNameDel(
                        NULL,
                        InfoStruct->msgi0_name);
            if (status != NERR_Success) {
                WLPrint(("DeleteMsgAliases: Name - %ws - delete failed",
                                            InfoStruct->msgi0_name));
            }
        }
        InfoStruct++;
    }

    FreeLibrary(dllHandle);
}



