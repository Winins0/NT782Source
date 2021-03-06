#ifndef _RPLPULL_
#define _RPLPULL_


#ifdef __cplusplus
extern "C" {
#endif

/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:
	rplpull.h
	

Abstract:

 



Functions:



Portability:


	This header is portable.

Author:

	Pradeep Bahl	(PradeepB)	Jan-1993



Revision History:

	Modification Date	Person		Description of Modification
	------------------	-------		---------------------------

--*/

/*
  includes
*/
#include "wins.h"
#include "rpl.h"
/*
  defines
*/



/*
  macros
*/

/*
 externs
*/


extern HANDLE		RplPullCnfEvtHdl; //handle to event signaled by main
					  //thread when a configuration change
					  //has to be given to the Pull handler
					  //thread



#if 0
extern  BOOL		fRplPullAddDiffInCurrRplCycle;

extern  BOOL		fRplPullTriggeredWins; //indicates that during the current
					 //replication cycle, one or more 
					 //WINS's were triggered.  This
					 //when TRUE, then if the above
					 //"AddDiff.." flag is TRUE, it means
					 //that the PULL thread should trigger
					//all PULL Pnrs that have an INVALID
					//metric in their UpdateCount field
					//(of the RPL_CONFIG_T struct)

extern BOOL		fRplPullTrigger;//Indication to the PULL thread to
					//trigger Pull pnrs since one or more
					//address changed.  fRplPullTriggerWins
					//has got be FALSE when this is true
#endif

//
// indicates whether the pull thread sent a continue signal to the SC
//
extern   BOOL   fRplPullContinueSent;

//
//  This array is indexed by the id. of an RQ server that has entries in
//  our database.  Each owner's max. version number is stored in this array
//
extern  RPL_VERS_NOS_T	RplPullOwnerVersNo[RPL_MAX_OWNERS];

extern  DWORD  RplPullCnfMagicNo;
/* 
 typedef  definitions
*/
/* 
 function declarations
*/

extern DWORD	RplPullInit(LPVOID);


extern
VOID
RplPullPullEntries(
	PCOMM_HDL_T 		pDlgHdl,	
	DWORD			OwnerId,
	VERS_NO_T		MaxVersNo,	
	VERS_NO_T		MinVersNo,
	WINS_CLIENT_E		Client_e,
	LPBYTE			*ppRspBuff,
	BOOL			fUpdCntrs
	);


extern
VOID
RplPullRegRepl(
	LPBYTE		pName,
	DWORD		NameLen,
	DWORD		Flag,
	DWORD		OwnerId,
	VERS_NO_T	VersNo,
	DWORD		NoOfAdds,
	PCOMM_ADD_T	pNodeAdd,
	PCOMM_ADD_T	pOwnerWinsAdd
	);
#ifdef __cplusplus
}
#endif

#endif //_RPLPULL_
