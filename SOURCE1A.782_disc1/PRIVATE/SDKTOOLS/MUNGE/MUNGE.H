/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    munge.h

Abstract:

    This is the main include file for the source file munger.

Author:

    Steve Wood (stevewo) 16-May-1989

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

NTSTATUS
BaseRtlCreateAtomTable(
    IN ULONG NumberOfBuckets,
    IN ULONG MaxAtomTableSize,
    OUT PVOID *AtomTableHandle
    );

NTSTATUS
BaseRtlAddAtomToAtomTable(
    IN PVOID AtomTableHandle,
    IN PSTRING AtomName,
    IN PULONG AtomValue OPTIONAL,
    OUT PULONG Atom OPTIONAL
    );

NTSTATUS
BaseRtlLookupAtomInAtomTable(
    IN PVOID AtomTableHandle,
    IN PSTRING AtomName,
    OUT PULONG AtomValue OPTIONAL,
    OUT PULONG Atom OPTIONAL
    );

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys\types.h>
#include <sys\stat.h>

#include <tools.h>
