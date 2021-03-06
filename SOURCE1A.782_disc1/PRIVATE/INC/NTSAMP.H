/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ntsamp.h

Abstract:

    This file contains structures that would normally be part of ntsam.h
    but are intended for system use only.

Author:

    David Chalmers (Davidc) 27-Mar-1992

Environment:

    User Mode - Win32

Revision History:


--*/


#ifndef _NTSAMPRIVATE_
#define _NTSAMPRIVATE_



#include <crypt.h>
#include <lsass.h>




//
// Structures usable in SetUserInformation and QueryUserInformation API calls
// by trusted clients only
//


typedef struct _USER_INTERNAL1_INFORMATION {
    NT_OWF_PASSWORD             NtOwfPassword;
    LM_OWF_PASSWORD             LmOwfPassword;
    BOOLEAN                     NtPasswordPresent;
    BOOLEAN                     LmPasswordPresent;
    BOOLEAN                     PasswordExpired; // A 'write-only' flag
} USER_INTERNAL1_INFORMATION, *PUSER_INTERNAL1_INFORMATION;


typedef struct _USER_INTERNAL2_INFORMATION {
    ULONG StatisticsToApply;
    OLD_LARGE_INTEGER LastLogon;
    OLD_LARGE_INTEGER LastLogoff;
    USHORT BadPasswordCount;
    USHORT LogonCount;
} USER_INTERNAL2_INFORMATION;


//
//
//
// The following flags may be used in the StatisticsToApply field.
//
//   USER_LOGON_STAT_LAST_LOGOFF - Replace the LastLogoff time in the
//      user record.
//
//  USER_LOGON_STATUS_LAST_LOGON - Replace the LastLogon time in the
//      user record.
//
//  USER_LOGON_STATUS_BAD_PWD_COUNT  - Replace the BadPasswordCount
//      field in the user record.
//
//  USER_LOGON_STATUS_LOGON_COUNT - Replace the LogonCount field in the
//      user record.
//
//  USER_LOGON_SUCCESSFUL_LOGON - Change user field values to indicate
//      that a successful logon has occured.
//
//  USER_LOGON_SUCCESSFUL_LOGOFF - Change user field values to indicate
//      that a successful logoff has occured.
//
//  USER_LOGON_BAD_PASSWORD - Change user field values to indicate that
//      an attempt was made to logon to the account with a bad password.
//
//
// NOTE:
//          USER_LOGON_BAD_PASSWORD
//          USER_LOGON_INTER_SUCCESS_LOGON
//          USER_LOGON_INTER_SUCCESS_LOGOFF
//          USER_LOGON_NET_SUCCESS_LOGON
//          USER_LOGON_NET_SUCCESS_LOGOFF
//
// may not be used in conjunction with ANY other flags (including
// each other).  That is, when one of these flags is used, there
// may be NO other flags set in StatisticsToApply.
//

#define USER_LOGON_STAT_LAST_LOGOFF      (0x00000001L)
#define USER_LOGON_STAT_LAST_LOGON       (0x00000002L)
#define USER_LOGON_STAT_BAD_PWD_COUNT    (0x00000004L)
#define USER_LOGON_STAT_LOGON_COUNT      (0x00000008L)

#define USER_LOGON_BAD_PASSWORD          (0x08000000L)
#define USER_LOGON_INTER_SUCCESS_LOGON   (0x1000000L)
#define USER_LOGON_INTER_SUCCESS_LOGOFF  (0x20000000L)
#define USER_LOGON_NET_SUCCESS_LOGON     (0x40000000L)
#define USER_LOGON_NET_SUCCESS_LOGOFF    (0x80000000L)


typedef struct _USER_INTERNAL3_INFORMATION {
    USER_ALL_INFORMATION I1;
    LARGE_INTEGER       LastBadPasswordTime;
} USER_INTERNAL3_INFORMATION,  *PUSER_INTERNAL3_INFORMATION;


//
// The following is for SamrGetUserDomainPasswordInformation(), which is
// only used in wrappers.c.
//

typedef struct _USER_DOMAIN_PASSWORD_INFORMATION {
    USHORT MinPasswordLength;
    ULONG PasswordProperties;
} USER_DOMAIN_PASSWORD_INFORMATION, *PUSER_DOMAIN_PASSWORD_INFORMATION;



//
// ChangePassword API for One-Way-Function-aware clients
//

NTSTATUS
SamiChangePasswordUser(
    IN SAM_HANDLE UserHandle,
    IN BOOLEAN LmOldPresent,
    IN PLM_OWF_PASSWORD LmOldOwfPassword,
    IN PLM_OWF_PASSWORD LmNewOwfPassword,
    IN BOOLEAN NtPresent,
    IN PNT_OWF_PASSWORD NtOldOwfPassword,
    IN PNT_OWF_PASSWORD NtNewOwfPassword
    );


NTSTATUS
SamiLmChangePasswordUser(
    IN SAM_HANDLE UserHandle,
    IN PENCRYPTED_LM_OWF_PASSWORD LmOldEncryptedWithLmNew,
    IN PENCRYPTED_LM_OWF_PASSWORD LmNewEncryptedWithLmOld
    );

#endif  // _NTSAMPRIVATE_
