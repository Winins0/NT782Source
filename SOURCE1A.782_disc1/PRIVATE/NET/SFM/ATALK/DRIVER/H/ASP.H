/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	asp.h

Abstract:

	This module contains definitions for the ASP code.

Author:

	Jameel Hyder (jameelh@microsoft.com)
	Nikhil Kamkolkar (nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version

Notes:	Tab stop: 4
--*/

#ifndef	_ASP_
#define	_ASP_

// This defines only the server side ASP protocol solely for the consumption of the
// AFP Server. Hence any functionality not needed by the AFP Server is missing here

// ASP command type bytes:
#define ASP_CLOSE_SESSION				1
#define ASP_CMD							2
#define ASP_GET_STATUS					3
#define ASP_OPEN_SESSION				4
#define ASP_TICKLE						5
#define ASP_WRITE						6
#define ASP_WRITE_DATA					7
#define ASP_ATTENTION					8

// ASP version:
#define ASP_VERSION						"\001\000"

// Asp error codes that are visible on the wire
#define	ASP_BAD_VERSION					(USHORT)-1066
#define	ASP_BUFFER_TOO_SMALL			(USHORT)-1067
#define	ASP_NO_MORE_SESSIONS			(USHORT)-1068
#define	ASP_NO_SERVERS					(USHORT)-1069
#define	ASP_PARAM_ERROR					(USHORT)-1070
#define	ASP_SERVER_BUSY					(USHORT)-1071
#define	ASP_SIZE_ERROR					(USHORT)-1073
#define	ASP_TOO_MANY_CLIENTS			(USHORT)-1074
#define	ASP_NO_ACK						(USHORT)-1075

// Offsets into the ATP user bytes for finding various fields:
#define ASP_CMD_OFF						0
#define ASP_CMD_RESULT_OFF				0
#define ASP_SSS_OFF						0
#define ASP_SESSIONID_OFF				1
#define ASP_WSS_OFF						1
#define ASP_VERSION_OFF					2
#define ASP_ERRORCODE_OFF				2
#define ASP_ATTN_WORD_OFF				2
#define ASP_SEQUENCE_NUM_OFF			2

#define ASP_CMD_RESULT_SIZE				4

// ASP timer values:
#define ATP_MAX_INTERVAL_FOR_ASP		20		// In 100ms units
#define ATP_MIN_INTERVAL_FOR_ASP		3		// In 100ms units
#define	ATP_INITIAL_INTERVAL_FOR_ASP	3

#define ASP_TICKLE_INTERVAL				300		// In 100ms units
#define ASP_MAX_SESSION_IDLE_TIME		1200	// In 100ms units. How long before we kill it
#define ASP_SESSION_MAINTENANCE_TIMER	1200	// In 100ms units. How often the timer runs
#define ATP_RETRIES_FOR_ASP				10		// For open, status, close;
										    	// infinite for others.
#define ASP_WRITE_DATA_SIZE				2		// WriteData command has two
												// bytes of data with it.
// Session status size:
#define ASP_MAX_STATUS_SIZE				ATP_MAX_TOTAL_RESPONSE_SIZE
#define	MAX_WRITE_REQ_SIZE				20

// How many ATP request handlers should be posted on each session listener
// socket (SLS) for handling Open, GetStatus and Tickle events. A few of
// them for the SSS just for kicks.
#define OUTSTANDING_SLS_HANDLERS		3
#define OUTSTANDING_SSS_HANDLERS		5

#define	ASP_CONN_HASH_BUCKETS			37	// Hashed by NodeAddr

#define	HASH_SRCADDR(pSrcAddr)	\
			((((pSrcAddr)->ata_Node >> 2) +	\
			  ((pSrcAddr)->ata_Network & 0xFF)) % ASP_CONN_HASH_BUCKETS)

// For resolving forward references
struct _AspAddress;
struct _AspConnxn;
struct _AspRequest;

#define	ASPAO_CLOSING			0x8000
#define	ASPAO_SIGNATURE			*(PULONG)"ASAO"

#if	DBG
#define	VALID_ASPAO(pAspAddr)	(((pAspAddr) != NULL) && \
								 ((pAspAddr)->aspao_Signature == ASPAO_SIGNATURE))
#else
#define	VALID_ASPAO(pAspAddr)	((pAspAddr) != NULL)
#endif

// Also known as the listener.
typedef struct _AspAddress
{
#if	DBG
	ULONG					aspao_Signature;
#endif
	struct _AspAddress	*	aspao_Next;			// Link
	LONG					aspao_RefCount;		// References to the address obj
	ULONG					aspao_Flags;
	PATP_ADDROBJ			aspao_pSlsAtpAddr;	// Sls Atp Socket
	PATP_ADDROBJ			aspao_pSssAtpAddr;	// Sss Atp Socket
	struct _AspConnxn	*	aspao_pSessions[ASP_CONN_HASH_BUCKETS];
												// List of open sessions
	LONG					aspao_NumActiveSessions;
	PBYTE					aspao_pStatusBuf;	// Status buffer
	USHORT					aspao_StsBufSize;	// Size of the status buffer
	BYTE					aspao_NextSessionId;// Id of the next session that comes in
	BOOLEAN					aspao_EnableNewConnections;
	GENERIC_COMPLETION		aspao_CloseCompletion;
	PVOID					aspao_CloseContext;
	ASP_CLIENT_ENTRIES		aspao_ClientEntries;// Entry points from the client
	ATALK_SPIN_LOCK			aspao_Lock;
} ASP_ADDROBJ, *PASP_ADDROBJ;

#define	ASPCO_ACTIVE			0x0001
#define	ASPCO_TICKLING			0x0002
#define	ASPCO_CLEANING_UP		0x0010
#define	ASPCO_LOCAL_CLOSE		0x0020
#define	ASPCO_REMOTE_CLOSE		0x0040
#define	ASPCO_DROPPED			0x0080
#define	ASPCO_CLOSING			0x8000
#define	ASPCO_SIGNATURE			*(PULONG)"ASCO"

#if	DBG
#define	VALID_ASPCO(pAspSess)	(((pAspSess) != NULL) && \
								 ((pAspSess)->aspco_Signature == ASPCO_SIGNATURE))
#else
#define	VALID_ASPCO(pAspSess)	((pAspSess) != NULL)
#endif

typedef struct _AspConnxn
{
#if	DBG
	ULONG					aspco_Signature;
#endif
	struct _AspConnxn	*	aspco_NextOverflow;	// Overflow link for hash bucket
												// These is non NULL only when on
												// active list
	struct _AspConnxn	*	aspco_NextSession;	// Linked to active session list
	struct _AspConnxn	**	aspco_PrevSession;	// Linked to active session list

	LONG					aspco_RefCount;		// References to the conn obj
	struct _AspAddress	*	aspco_pAspAddr;		// Back pointer to the listener

	struct _AspRequest	*	aspco_pActiveReqs;	// List of requests being processed
	struct _AspRequest	*	aspco_pFreeReqs;	// Free requests
	PVOID					aspco_ConnContext;	// User context associated with this conn.
	LONG					aspco_LastContactTime;
	ATALK_ADDR				aspco_WssRemoteAddr;// This is the remote addr which
												// issues the commands/writes
	BYTE					aspco_SessionId;
	BYTE					aspco_cReqsInProcess;// Count of requests in process
	USHORT					aspco_Flags;		// ASPCO_xxx values
	USHORT					aspco_NextExpectedSeqNum;
	USHORT					aspco_TickleXactId;	// Transaction id for tickles
	RT						aspco_RT;			// Used for adaptive round-trip time calculation
	PVOID					aspco_CloseContext;
	CLIENT_CLOSE_COMPLETION	aspco_CloseCompletion;
	PVOID					aspco_AttentionContext;
	ATALK_SPIN_LOCK			aspco_Lock;
} ASP_CONNOBJ, *PASP_CONNOBJ;

#define	ASPRQ_WRTCONT			0x01		// Set if we are doing a write continue
#define	ASPRQ_WRTCONT_CANCELLED	0x10		// Set if a write continue was cancelled
#define	ASPRQ_REPLY				0x02		// Set if a reply is being processed
#define	ASPRQ_REPLY_CANCELLED	0x20		// Set if a reply is cancelled

// The request gets created when a GetAnyRequest gets posted on the address object.
// Once an incoming request is satisfied, this then moves to the connection object. The posted
// request hangs off the address object.
#define	ASPRQ_SIGNATURE			*(PULONG)"ASRQ"
#if	DBG
#define	VALID_ASPRQ(pAspReq)	(((pAspReq) != NULL) && \
								 ((pAspReq)->asprq_Signature == ASPRQ_SIGNATURE))
#else
#define	VALID_ASPCO(pAspReq)	((pAspReq) != NULL)
#endif

typedef struct _AspRequest
{
#if	DBG
	ULONG					asprq_Signature;
#endif
	struct _AspRequest	*	asprq_Next;     // Link to next request
	struct _AspConnxn	*	asprq_pAspConn;	// Owning connection
	USHORT					asprq_SeqNum;	// As generated by the wksta end
	USHORT					asprq_WCXactId;	// Transaction Id of the write
											// continue in progress
	PATP_RESP				asprq_pAtpResp;	// Used by PostResp/Cancel
	BYTE					asprq_ReqType;	// Cmd/WrtCont
	BYTE					asprq_Flags;	// Various ASPRQ_xxx values
	ATALK_ADDR				asprq_RemoteAddr;// This address is used for
											// future communications but only
											// for this request
	union
	{
		PMDL				asprq_ReplyMdl;	// Valid during a reply
		PMDL				asprq_WriteMdl;	// Valid during a write continue
	};

	UCHAR					asprq_ReqBuf[MAX_WRITE_REQ_SIZE];
											// The request is copied here during a
											// write request
	USHORT					asprq_ReqLen;
	BYTE					asprq_WrtContRespBuf[ASP_WRITE_DATA_SIZE];
} ASP_REQUEST, *PASP_REQUEST;

//	MACROS
#define	AtalkAspGetDdpAddress(pAspAddr)	\
							AtalkAtpGetDdpAddress((pAspAddr)->aspao_pSlsAtpAddr)

extern
VOID
AtalkInitAspInitialize(
	VOID
);

extern
ATALK_ERROR
AtalkAspCreateAddress(
	OUT	PASP_ADDROBJ	*		ppAspAddr
);

extern
ATALK_ERROR
AtalkAspCloseAddress(
	IN	PASP_ADDROBJ			pAspAddr,
	IN	GENERIC_COMPLETION		CompletionRoutine,
	IN	PVOID					CloseContext
);

extern
ATALK_ERROR
AtalkAspBind(
	IN	PASP_ADDROBJ			pAspAddr,
	IN	PASP_BIND_PARAMS		pBindParms,
	IN	PACTREQ					pActReq
);

ATALK_ERROR
AtalkAspReply(
	IN	PASP_REQUEST			pAspReq,
	IN	PBYTE					pResultCode,// Pointer to the 4-byte result
	IN	PMDL					pAMdl		// Response buffer
);

extern
NTSTATUS
AtalkAspSetStatus(
	IN	PASP_ADDROBJ			pAspAddr,
	IN	PUCHAR					pStatusBuf,
	IN	USHORT					StsBufSize
);

extern
NTSTATUS
AtalkAspListenControl(
	IN	PASP_ADDROBJ			pAspAddr,
	IN	BOOLEAN					Enable
);

extern
PASP_ADDROBJ
AtalkAspReferenceAddr(
	IN	PASP_ADDROBJ			pAspAddr
);

extern
VOID
AtalkAspDereferenceAddr(
	IN	PASP_ADDROBJ			pAspAddr
);

extern
ATALK_ERROR
AtalkAspCleanupConnection(
	IN	PASP_CONNOBJ			pAspConn
);

extern
ATALK_ERROR
AtalkAspCloseConnection(
	IN	PASP_CONNOBJ			pAspConn
);

extern
NTSTATUS
AtalkAspSendAttention(
	IN	PASP_CONNOBJ			pAspConn,
	IN	USHORT					AttentionWord,
	IN	PVOID					pContext
);

extern
VOID
AtalkAspDereferenceConn(
	IN	PASP_CONNOBJ			pAspConn
);

#ifdef	ASP_LOCALS

// This is a list of all active connections. This is scanned by the session
// maintenance timer.
LOCAL	PASP_CONNOBJ	atalkAspConnList = NULL;
LOCAL	TIMERLIST		atalkAspSMTTimer = { 0 };
LOCAL	ATALK_SPIN_LOCK	atalkAspLock = {0};

typedef	struct
{
	PATP_RESP		aps_pAtpResp;
	PAMDL			aps_pAMdl;
} ASP_POSTSTAT_CTX, *PASP_POSTSTAT_CTX;

LOCAL ATALK_ERROR
atalkAspPostWriteContinue(
	IN	PASP_REQUEST			pAspReq
);

LOCAL PASP_CONNOBJ
atalkAspReferenceConnBySrcAddr(
	IN	PASP_ADDROBJ			pAspAddr,
	IN	PATALK_ADDR				pSrcAddr,
	IN	BYTE					SessionId
);

LOCAL BOOLEAN
atalkAspSlsXHandler(
	IN	ATALK_ERROR				ErrorCode,
	IN	PASP_ADDROBJ			pAspAddr,		// Listener (our context)
	IN	PATP_RESP				RespCtxt,		// Used by PostResp/CancelResp
	IN	PATALK_ADDR				pSrcAddr,		// Address of requestor
	IN	PAMDL					pAMdlReq,
	IN	USHORT					PktLen,
	IN	PBYTE					pPkt,
	IN	PBYTE					pUserBytes
);

LOCAL BOOLEAN
atalkAspSssXHandler(
	IN	ATALK_ERROR				ErrorCode,
	IN	PASP_ADDROBJ			pAspAddr,		// Listener (our context)
	IN	PATP_RESP				RespCtxt,		// Used by PostResp/CancelResp
	IN	PATALK_ADDR				pSrcAddr,		// Address of requestor
	IN	PAMDL					pAMdlReq,
	IN	USHORT					PktLen,
	IN	PBYTE					pPkt,
	IN	PBYTE					pUserBytes
);

LOCAL VOID
atalkAspReplyRelease(
	IN	ATALK_ERROR				Error,
	IN	PASP_REQUEST			pAspReq
);

LOCAL VOID
atalkAspWriteContinueResp(
	IN	ATALK_ERROR				Error,
	IN	PASP_REQUEST			pAspReq,
	IN	PAMDL					pReqAMdl,
	IN	PAMDL					pRespAMdl,
	IN	ULONG					RespSize,
	IN	PBYTE					RespUserBytes
);

LOCAL VOID
atalkAspSendAttentionResp(
	IN	ATALK_ERROR				Error,
	IN	PVOID					pContext,
	IN	PAMDL					pReqAMdl,
	IN	PAMDL					pRespAMdl,
	IN	ULONG					RespSize,
	IN	PBYTE					RespUserBytes
);

LOCAL VOID
atalkAspSessionClose(
	IN	PASP_CONNOBJ			pAspConn
);

LOCAL LONG
atalkAspSessionMaintenanceTimer(
	IN	PTIMERLIST				pContext,
	IN	BOOLEAN					TimerShuttingDown
);

LOCAL VOID
atalkAspRespComplete(
	IN	ATALK_ERROR				Error,
	IN	PASP_POSTSTAT_CTX		pStsCtx
);

LOCAL VOID
atalkAspCloseComplete(
	IN	ATALK_ERROR				Status,
	IN	PASP_ADDROBJ			pAspAddr
);

LOCAL VOID
atalkAspReturnResp(
	IN	PATP_RESP				pAtpResp,
	IN	PATALK_ADDR				pDstAddr,
	IN	BYTE					Byte0,
	IN	BYTE					Byte1,
	IN	USHORT					Word2
);

LOCAL VOID
atalkAspGenericRespComplete(
	IN	ATALK_ERROR		ErrorCode,
	IN	PATP_RESP		pAtpResp
);

#endif	// ASP_LOCALS

#endif	// _ASP_

