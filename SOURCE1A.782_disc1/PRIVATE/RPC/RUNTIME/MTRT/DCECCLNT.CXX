/*++

Copyright (c) 1991 Microsoft Corporation

Module Name:

    dcecclnt.cxx

Abstract:

    This file just defines RPC_CLIENT_SIDE_ONLY and then includes
    rpccmmn.cxx to get the client parts of the common routines.
    
Author:

    Michael Montague (mikemon) 04-Nov-1991

Revision History:

--*/

#define RPC_CLIENT_SIDE_ONLY
#include <dcecmmn.cxx>
