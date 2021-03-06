#ifndef hash_h
#define hash_h

//-------------------------- MODULE DESCRIPTION ----------------------------
//  
//  hash.h
//  
//  Copyright 1992 Technology Dynamics, Inc.
//  
//  All Rights Reserved!!!
//  
//	This source code is CONFIDENTIAL and PROPRIETARY to Technology 
//	Dynamics. Unauthorized distribution, adaptation or use may be 
//	subject to civil and criminal penalties.
//
//  All Rights Reserved!!!
//
//---------------------------------------------------------------------------
//  
//  Constants, types, and prototypes for Hash Table and supporting functions.
//
//  Project:  Implementation of an SNMP Agent for Microsoft's NT Kernel
//
//  $Revision:   1.0  $
//  $Date:   09 Jun 1992 13:43:34  $
//  $Author:   todd  $
//
//  $Log:   N:/lmalrt2/vcs/hash.h_v  $
//  
//     Rev 1.0   09 Jun 1992 13:43:34   todd
//  Initial revision.
//  
//---------------------------------------------------------------------------

//--------------------------- VERSION INFO ----------------------------------

static char *hash__h = "@(#) $Logfile:   N:/lmalrt2/vcs/hash.h_v  $ $Revision:   1.0  $";

//--------------------------- PUBLIC CONSTANTS ------------------------------

#include <snmp.h>

#include "alrtmib.h"

//--------------------------- PUBLIC STRUCTS --------------------------------

//--------------------------- PUBLIC VARIABLES --(same as in module.c file)--

//--------------------------- PUBLIC PROTOTYPES -----------------------------

SNMPAPI MIB_HashInit(
           void
           );

UINT MIB_Hash(
        IN AsnObjectIdentifier *Oid // OID to hash
	);

MIB_ENTRY *MIB_HashLookup(
              IN AsnObjectIdentifier *Oid // OID to lookup
	      );

#ifdef MIB_DEBUG
void MIB_HashPerformance( void );
#endif

//------------------------------- END ---------------------------------------

#endif /* hash_h */

