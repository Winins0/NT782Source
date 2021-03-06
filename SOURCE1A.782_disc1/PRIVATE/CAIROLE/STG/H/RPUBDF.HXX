//+--------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1992 - 1992.
//
//  File:	rpubdf.hxx
//
//  Contents:	Root public docfile header
//
//  Classes:	CRootPubDocFile
//
//  History:	26-Aug-92	DrewB	Created
//
//---------------------------------------------------------------

#ifndef __RPUBDF_HXX__
#define __RPUBDF_HXX__

#include <publicdf.hxx>

//+--------------------------------------------------------------
//
//  Class:	CRootPubDocFile (rpdf)
//
//  Purpose:	Root form of the public docfile
//
//  Interface:	See below
//
//  History:	26-Aug-92	DrewB	Created
//
//---------------------------------------------------------------

class CRootPubDocFile : public CPubDocFile
{
public:
    CRootPubDocFile(IMalloc * const pMalloc);
    SCODE InitRoot(ILockBytes *plstBase,
		   DWORD const dwStartFlags,
		   DFLAGS const df,
		   SNBW snbExclude,
#ifndef REF
		   CDFBasis **ppdfb,
#endif //!REF
		   ULONG *pulOpenLock);
#ifndef REF
    // C700 - Virtual destructors aren't working properly so explicitly
    // declare one
#endif //!REF
    virtual void vdtor(void);

    virtual SCODE Stat(STATSTGW *pstatstg, DWORD grfStatFlag);

#ifndef REF
    SCODE SwitchToFile(OLECHAR const *ptcsFile,
                       ILockBytes *plkb,
                       ULONG *pulOpenLock);
#endif //!REF

private:
#ifndef REF
    SCODE InitInd(ILockBytes *plstBase,
		  SNBW snbExclude,
		  DWORD const dwStartFlags,
		  DFLAGS const df);
#endif //!REF
#ifndef REF
    SCODE InitNotInd(ILockBytes *plstBase,
		     SNBW snbExclude,
		     DWORD const dwStartFlags);
    ULONG _ulPriLock;
#else
    SCODE Init(ILockBytes *plstBase,
            SNBW snbExclude,
            DWORD const dwStartFlags);
#endif //!REF
    IMalloc * const _pMalloc;
};

#endif // #ifndef __RPUBDF_HXX__
