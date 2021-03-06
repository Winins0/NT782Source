/*++

Copyright (c) 1990, 1991  Microsoft Corporation


Module Name:

   cmdatini.c

Abstract:

   contains code to init static STRING structures for registry name space.

Author:

    Andre Vachon (andreva) 08-Apr-1992


Environment:

    Kernel mode.

Revision History:

--*/

#include "cmp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,CmpInitializeRegistryNames)
#endif

extern UNICODE_STRING CmRegistryRootName;
extern UNICODE_STRING CmRegistryMachineName;
extern UNICODE_STRING CmRegistryMachineHardwareName;
extern UNICODE_STRING CmRegistryMachineHardwareDescriptionName;
extern UNICODE_STRING CmRegistryMachineHardwareDescriptionSystemName;
extern UNICODE_STRING CmRegistryMachineHardwareDeviceMapName;
extern UNICODE_STRING CmRegistryMachineHardwareResourceMapName;
extern UNICODE_STRING CmRegistryMachineSystemName;
extern UNICODE_STRING CmRegistryMachineSystemCurrentControlSet;
extern UNICODE_STRING CmRegistryUserName;
extern UNICODE_STRING CmRegistrySystemCloneName;
extern UNICODE_STRING CmpSystemFileName;

extern PWCHAR CmpRegistryRootString;
extern PWCHAR CmpRegistryMachineString;
extern PWCHAR CmpRegistryMachineHardwareString;
extern PWCHAR CmpRegistryMachineHardwareDescriptionString;
extern PWCHAR CmpRegistryMachineHardwareDescriptionSystemString;
extern PWCHAR CmpRegistryMachineHardwareDeviceMapString;
extern PWCHAR CmpRegistryMachineHardwareResourceMapString;
extern PWCHAR CmpRegistryMachineSystemString;
extern PWCHAR CmpRegistryMachineSystemCurrentControlSetString;
extern PWCHAR CmpRegistryUserString;
extern PWCHAR CmpRegistrySystemCloneString;
extern PWCHAR CmpRegistrySystemFileNameString;



VOID
CmpInitializeRegistryNames(
VOID
)

/*++

Routine Description:

    This routine creates all the Unicode strings for the various names used
    in and by the registry

Arguments:

    None.

Returns:

    None.

--*/
{
    ULONG i;

    RtlInitUnicodeString( &CmRegistryRootName,
                          CmpRegistryRootString );

    RtlInitUnicodeString( &CmRegistryMachineName,
                          CmpRegistryMachineString );

    RtlInitUnicodeString( &CmRegistryMachineHardwareName,
                          CmpRegistryMachineHardwareString );

    RtlInitUnicodeString( &CmRegistryMachineHardwareDescriptionName,
                          CmpRegistryMachineHardwareDescriptionString );

    RtlInitUnicodeString( &CmRegistryMachineHardwareDescriptionSystemName,
                          CmpRegistryMachineHardwareDescriptionSystemString );

    RtlInitUnicodeString( &CmRegistryMachineHardwareDeviceMapName,
                          CmpRegistryMachineHardwareDeviceMapString );

    RtlInitUnicodeString( &CmRegistryMachineHardwareResourceMapName,
                          CmpRegistryMachineHardwareResourceMapString );

    RtlInitUnicodeString( &CmRegistryMachineSystemName,
                          CmpRegistryMachineSystemString );

    RtlInitUnicodeString( &CmRegistryMachineSystemCurrentControlSet,
                          CmpRegistryMachineSystemCurrentControlSetString);

    RtlInitUnicodeString( &CmRegistryUserName,
                          CmpRegistryUserString );

    RtlInitUnicodeString( &CmRegistrySystemCloneName,
                          CmpRegistrySystemCloneString );

    RtlInitUnicodeString( &CmpSystemFileName,
                          CmpRegistrySystemFileNameString );

    //
    // Initialize the type names for the hardware tree.
    //

    for (i = 0; i <= MaximumType; i++) {

        RtlInitUnicodeString( &(CmTypeName[i]),
                              CmTypeString[i] );

    }

    //
    // Initialize the class names for the hardware tree.
    //

    for (i = 0; i <= MaximumClass; i++) {

        RtlInitUnicodeString( &(CmClassName[i]),
                              CmClassString[i] );

    }

    return;
}

