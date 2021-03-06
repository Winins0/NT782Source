/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 Copyright (c) 1989 Microsoft Corporation

 Module Name:
	
	pickle.hxx

 Abstract:

    Header file for pickle code generation class.

 Notes:


 History:

    Mar-20-1994 VibhasC     Created.
 ----------------------------------------------------------------------------*/

/****************************************************************************
 *	include files
 ***************************************************************************/
#include "nulldefs.h"
extern "C"
	{
	#include <stdio.h>
	#include <assert.h>
	}

#include "ndrcls.hxx"
/****************************************************************************
 *	local definitions
 ***************************************************************************/

#define TYPE_ALIGN_SIZE_CODE 0
#define TYPE_ENCODE_CODE     1
#define TYPE_DECODE_CODE     2

/****************************************************************************
 *	local data
 ***************************************************************************/

/****************************************************************************
 *	externs
 ***************************************************************************/
extern    void                        GenStdMesPrototype( CCB *   pCCB,
                                                          PNAME   pTypeName,
                                                          int     Code,
                                                          BOOL    fImpBin );
/****************************************************************************/

//
// The pickle code generation class.
//

class   CG_TYPE_ENCODE   : public CG_CLASS
    {
private:

    BOOL                        fEncode;
    BOOL                        fDecode;
    BOOL                        fImplicitHandle;
    RESOURCE                *   pBindingResource;

	//
	// Keep a link to the actual specification of the type.
	//

	node_skl			*	pTypeNode;

    // Generate the sizing stub for the type.

    CG_STATUS                   GenTypeSize( CCB * pCCB );

    // Generate the encoding stub for the type.

    CG_STATUS                   GenTypeEncode( CCB * pCCB );

    // Generate the decoding stub for type.

    CG_STATUS                   GenTypeDecode( CCB * pCCB );

    // Miscellaneous.

    void                        AllocateEncodeResources( CCB * pCCB );

    _expr_proc_call     *       CreateStdMesEngineProc( CCB * pCCB,
                                                        int   Code );

public:
    
    // The constructor.
                                CG_TYPE_ENCODE( node_skl * pT,
                                				BOOL fE, 
                                				BOOL fD)
                                    {
                                    SetType( pT );
                                    fEncode             = fE;
                                    fDecode             = fD;
                                    pBindingResource    = 0;
                                    fImplicitHandle    = FALSE;
                                    }


    // Identify encode or decode.

    BOOL                        IsEncode()
                                    {
                                    return (BOOL) fEncode;
                                    }
    BOOL                        IsDecode()
                                    {
                                    return (BOOL) fDecode;
                                    }

    ID_CG                       GetCGID()
                                    {
                                    return ID_CG_TYPE_ENCODE;
                                    }

    void                        SetHasImplicitHandle()
                                    {
                                    fImplicitHandle = 1;
                                    }

    BOOL                        HasImplicitHandle()
                                    {
                                    return (BOOL) (fImplicitHandle == 1);
                                    }

    void                        SetBindingResource( RESOURCE * pRes )
                                    {
                                    pBindingResource = pRes;
                                    }

    RESOURCE                *   GetBindingResource()
                                    {
                                    return pBindingResource;
                                    }

    // Generate the pickling code.

    virtual
    CG_STATUS                   GenTypeEncodingStub( CCB    *   pCCB );

	node_skl			    *	GetType()
								    {
								    return pTypeNode;
								    }

	node_skl			    *	SetType( node_skl * pT )
								    {
								    return pTypeNode = pT;
								    }

    };
