/* Copyright (c) 1993, Microsoft Corporation, all rights reserved
**
** raspap.h
** Remote Access PPP Password Authentication Protocol
**
** 11/05/93 Steve Cobb
*/

#ifndef _RASPAP_H_
#define _RASPAP_H_


/* PAP packet codes from PAP spec.
*/
#define PAPCODE_Req 1
#define PAPCODE_Ack 2
#define PAPCODE_Nak 3

#define MAXPAPCODE 3

/* Returned by receive buffer parsing routines that discover the packet is
** corrupt, usually because the length fields don't make sense.
*/
#define ERRORBADPACKET (DWORD )-1

/* Defines states within the PAP protocol.
*/
#define PAPSTATE enum tagPAPSTATE

PAPSTATE
{
    PS_Initial,
    PS_RequestSent,
    PS_WaitForRequest,
    PS_Done
};


/* Defines the WorkBuf stored for us by the PPP engine.
*/
#define PAPWB struct tagPAPWB

PAPWB
{
    /* True if role is server, false if client.
    */
    BOOL fServer;

    /* The domain\username and password (applies to client only).
    */
    CHAR szAccount[ DNLEN + 1 + UNLEN + 1 ];
    CHAR szPassword[ PWLEN ];

    /* The current state in the PAP protocol.
    */
    PAPSTATE state;

    /* Last sequencing ID sent on this port.  Incremented for each
    ** Authenticate-Req packet sent.
    */
    BYTE bIdSent;

    /* The final result, used to duplicate the original response for all
    ** subsequent Authenticate-Req packets.  This is per PAP spec to cover
    ** lost Ack/Nak case without allowing malicious client to discover
    ** alternative identities under the covers during a connection.  (applies
    ** to server only)
    */
    PPPAP_RESULT result;
};


/* Prototypes.
*/
DWORD CheckCredentials( CHAR*, CHAR*, CHAR*, DWORD*, BOOL*, CHAR*,
          BYTE*, CHAR* );
DWORD CMakeMessage( PAPWB*, PPP_CONFIG*, PPP_CONFIG*, DWORD, PPPAP_RESULT* );
DWORD GetCredentialsFromRequest( PPP_CONFIG*, CHAR*, CHAR*, CHAR* );
DWORD GetErrorFromNak( PPP_CONFIG* );
VOID  MakeRequestMessage( PAPWB*, PPP_CONFIG*, DWORD );
VOID  MakeResultMessage( DWORD, BYTE, PPP_CONFIG*, DWORD );
DWORD PapBegin( VOID**, VOID* );
DWORD PapEnd( VOID* );
DWORD PapMakeMessage( VOID*, PPP_CONFIG*, PPP_CONFIG*, DWORD, PPPAP_RESULT*,
          PPPAP_INPUT* pInput );
DWORD SMakeMessage( PAPWB*, PPP_CONFIG*, PPP_CONFIG*, DWORD, PPPAP_RESULT* );


/* Globals.
*/
#ifdef RASPAPGLOBALS
#define GLOBALS
#define EXTERN
#else
#define EXTERN extern
#endif

/* Next packet identifier to assign.  Unlike CPs, APs must handle updating
** this sequence number themselves because the engine can't make as many
** assumptions about the protocol.  It is stored global to all ports and
** authentication sessions to make it less likely that an ID will be used in
** sequential authentication sessions.  Not to be confused with the 'bIdSent'
** updated on a per-port basis and used for matching.
*/
EXTERN BYTE BNextId
#ifdef GLOBALS
    = 0
#endif
;

#undef EXTERN
#undef GLOBALS


#endif // _RASPAP_H_
