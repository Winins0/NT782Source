/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    AttrData.c

Abstract:

    This module contains an initial image of the Attribute Definition File.

Author:

    Tom Miller      [TomM]          7-Jun-1991

Revision History:

--*/

#include "NtfsProc.h"

//
// Define an array to hold the initial attribute definitions.  This is
// essentially the initial contents of the Attribute Definition File.
// NTFS may find it convenient to use this module for attribute
// definitions prior to getting an NTFS volume mounted, however it is valid
// for NTFS to assume knowledge of the system-defined attributes without
// consulting this table.
//

ATTRIBUTE_DEFINITION_COLUMNS NtfsAttributeDefinitions[$EA + 1] =

{
    {{'$','S','T','A','N','D','A','R','D','_','I','N','F','O','R','M','A','T','I','O','N'},
    $STANDARD_INFORMATION,                              // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    ATTRIBUTE_DEF_MUST_BE_RESIDENT,                     // Flags
    {sizeof(STANDARD_INFORMATION), 0},                  // Minimum length
    {sizeof(STANDARD_INFORMATION), 0}},                 // Maximum length

    {{'$','A','T','T','R','I','B','U','T','E','_','L','I','S','T'},
    $ATTRIBUTE_LIST,                                    // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    0,                                                  // Flags
    {0,0},                                              // Minimum length
    {MAXULONG,MAXULONG}},                               // Maximum length

    {{'$','F','I','L','E','_','N','A','M','E'},
    $FILE_NAME,                                         // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    ATTRIBUTE_DEF_MUST_BE_RESIDENT | ATTRIBUTE_DEF_INDEXABLE,   // Flags
    {sizeof(FILE_NAME), 0},                             // Minimum length
    {sizeof(FILE_NAME) + (255 * sizeof(WCHAR)), 0}},    // Maximum length

    {{'$','V','O','L','U','M','E','_','V','E','R','S','I','O','N'},
    $VOLUME_VERSION,                                    // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    ATTRIBUTE_DEF_MUST_BE_RESIDENT,                     // Flags
    {sizeof(VOLUME_VERSION), 0},                        // Minimum length
    {sizeof(VOLUME_VERSION), 0}},                       // Maximum length

    {{'$','S','E','C','U','R','I','T','Y','_','D','E','S','C','R','I','P','T','O','R'},
    $SECURITY_DESCRIPTOR,                               // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    0,                                                  // Flags
    {0,0},                                              // Minimum length
    {MAXULONG,MAXULONG}},                               // Maximum length

    {{'$','V','O','L','U','M','E','_','N','A','M','E'},
    $VOLUME_NAME,                                       // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    ATTRIBUTE_DEF_MUST_BE_RESIDENT,                     // Flags
    {2,0},                                              // Minimum length
    {256,0}},                                           // Maximum length

    {{'$','V','O','L','U','M','E','_','I','N','F','O','R','M','A','T','I','O','N'},
    $VOLUME_INFORMATION,                                // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    ATTRIBUTE_DEF_MUST_BE_RESIDENT,                     // Flags
    {sizeof(VOLUME_INFORMATION),0},                     // Minimum length
    {sizeof(VOLUME_INFORMATION),0}},                    // Maximum length

    {{'$','D','A','T','A'},
    $DATA,                                              // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    0,                                                  // Flags
    {0,0},                                              // Minimum length
    {MAXULONG,MAXULONG}},                               // Maximum length

    {{'$','I','N','D','E','X','_','L','I','S','T'},
    $INDEX_ROOT,                                        // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    ATTRIBUTE_DEF_MUST_BE_RESIDENT,                     // Flags
    {0,0},                                              // Minimum length
    {MAXULONG,MAXULONG}},                               // Maximum length

    {{'$','I','N','D','E','X','_','A','L','L','O','C','A','T','I','O','N'},
    $INDEX_ALLOCATION,                                  // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    0,                                                  // Flags
    {0,0},                                              // Minimum length
    {MAXULONG,MAXULONG}},                               // Maximum length

    {{'$','B','I','T','M','A','P'},
    $BITMAP,                                            // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    0,                                                  // Flags
    {0,0},                                              // Minimum length
    {MAXULONG,MAXULONG}},                               // Maximum length

    {{'$','S','Y','M','B','O','L','I','C','_','L','I','N','K'},
    $SYMBOLIC_LINK,                                     // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    0,                                                  // Flags
    {0,0},                                              // Minimum length
    {MAXULONG,MAXULONG}},                               // Maximum length

    {{'$','E','A','_','I','N','F','O','R','M','A','T','I','O','N'},
    $EA_INFORMATION,                                    // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    ATTRIBUTE_DEF_MUST_BE_RESIDENT,                     // Flags
    {sizeof(EA_INFORMATION), 0},                        // Minimum length
    {sizeof(EA_INFORMATION), 0}},                       // Maximum length

    {{'$','E','A',},
    $EA,                                                // Attribute code
    0,                                                  // Display rule
    0,                                                  // Collation rule
    0,                                                  // Flags
    {0,0},                                              // Minimum length
    {0x10000,0}}                                        // Maximum length
};

