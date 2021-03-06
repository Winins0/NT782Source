 /********************************************************************/
/**               Copyright(c) 1989 Microsoft Corporation.	   **/
/********************************************************************/

//***
//
// Filename:	lcp.h
//
// Description: 
//
// History:
//	Nov 11,1993.	NarenG		Created original version.
//

#ifndef _LCP_
#define _LCP_

//
// LCP option types 
//

#define LCP_OPTION_MRU          0x01
#define LCP_OPTION_ACCM         0x02
#define LCP_OPTION_AUTHENT      0x03
#define LCP_OPTION_MAGIC        0x05
#define LCP_OPTION_PFC          0x07
#define LCP_OPTION_ACFC         0x08
#define LCP_OPTION_CALLBACK     0x0D
#define LCP_OPTION_LIMIT        0x0D    // highest # we can handle 

//
// Authentication protocols
//

#define  LCP_AP_CHAP_MS         0x00000001
#define  LCP_AP_CHAP_MD5        0x00000002
#define  LCP_AP_SPAP_NEW        0x00000004
#define  LCP_AP_SPAP_OLD        0x00000008
#define  LCP_AP_PAP             0x00000010
#define  LCP_AP_MAX             0x00000020

//
// Table for LCP configuration requests 
//

typedef struct _LCP_OPTIONS 
{
    DWORD Negotiate;       	// negotiation flags 

#define LCP_N_MRU               (1 << LCP_OPTION_MRU)
#define LCP_N_ACCM              (1 << LCP_OPTION_ACCM)
#define LCP_N_AUTHENT           (1 << LCP_OPTION_AUTHENT)
#define LCP_N_MAGIC             (1 << LCP_OPTION_MAGIC)
#define LCP_N_PFC               (1 << LCP_OPTION_PFC)
#define LCP_N_ACFC              (1 << LCP_OPTION_ACFC)
#define LCP_N_CALLBACK          (1 << LCP_OPTION_CALLBACK)

    DWORD MRU;                  // Maximum Receive Unit 
    DWORD ACCM;                 // Async Control Char Map 
    DWORD AP;                   // Authentication protocol 
    DWORD APDataSize;           // Auth. protocol data size in bytes
    PBYTE pAPData;              // Pointer Auth. protocol data
    DWORD MagicNumber;          // Magic number value 
    DWORD PFC;          	// Protocol field compression.
    DWORD ACFC;          	// Address and Control Field Compression.
    DWORD Callback;          	// Callback

} LCP_OPTIONS, *PLCP_OPTIONS;

#define PPP_NEGOTIATE_CALLBACK  0x06

//
// Other configuration option values 
//

#define LCP_ACCM_DEFAULT        	0xffffffffL
#define LCP_MRU_HI      		1500            // High MRU limit 
#define LCP_MRU_LO      		128             // Lower MRU limit 
#define LCP_DEFAULT_MRU 		1500

#define LCP_REQ_TRY     20              /* REQ attempts */

#define LCP_SPAP_VERSION        0x01000001

//
//  Local.Want:           Options to request.
//                        Contains desired value.
//                        Only non-default options need to be negotiated.
//                        Initially, all are default.
//  Local.WillNegotiate:  Options to accept in a NAK from remote.
//  local.Work:           Options currently being negotiated.
//                        Value is valid only when negotiate bit is set.
//
//  Remote.Want:          Options to suggest by NAK if not present in REQ.
//                        Contains desired value.
//  Remote.WillNegotiate: Options to accept in a REQ from remote.
//  Remote.Work:          Options currently being negotiated.
//                        Value is valid only when negotiate bit is set.
// 

typedef struct _LCP_SIDE
{
    DWORD   	WillNegotiate;

    DWORD       fLastAPTried;

    LCP_OPTIONS Want;

    LCP_OPTIONS Work;

} LCP_SIDE, *PLCP_SIDE;

//
// LCP control block 
//

typedef struct _LCPCB
{
    HPORT               hPort;
   
    BOOL                fServer;

    DWORD               dwMagicNumberFailureCount;

    DWORD               fAPsAvailable;

    PPP_CONFIG_INFO     PppConfigInfo;

    LCP_SIDE            Local;

    LCP_SIDE            Remote;

}LCPCB, *PLCPCB;


#endif
