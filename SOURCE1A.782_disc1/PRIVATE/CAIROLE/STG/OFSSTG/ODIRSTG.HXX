//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1992 - 1993.
//
//  File:	odirstg.hxx
//
//  Contents:	COfsDirStorage header
//
//  Classes:	COfsDirStorage
//
//  History:	24-Jun-93	DrewB	Created
//
//----------------------------------------------------------------------------

#ifndef __ODIRSTG_HXX__
#define __ODIRSTG_HXX__

#include <ofsps.hxx>

//+---------------------------------------------------------------------------
//
//  Class:	COfsDirStorage (ds)
//
//  Purpose:	Implements IStorage for a directory
//
//  Interface:	See below
//
//  History:	24-Jun-93	DrewB	Created
//
//----------------------------------------------------------------------------

interface COfsDirStorage
    : public COfsPropSet,
      INHERIT_TRACKING,
      public IStorage,
      public INativeFileSystem
{
public:
    COfsDirStorage(void);
    SCODE InitFromHandle(HANDLE h, DWORD grfMode);
    SCODE InitFromPath(HANDLE hParent,
                       WCHAR const *pwcsName,
                       DWORD grfMode,
                       CREATEOPEN co,
                       LPSTGSECURITY pssSecurity);
    ~COfsDirStorage(void);

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID iid, void **ppvObj);
    DECLARE_STD_REFCOUNTING;

    // IStorage
    STDMETHOD(CreateStream)(WCHAR const *pwcsName,
                            DWORD grfMode,
                            DWORD reserved1,
                            DWORD reserved2,
                            IStream **ppstm);
    STDMETHOD(OpenStream)(WCHAR const *pwcsName,
			  void *reserved1,
                          DWORD grfMode,
                          DWORD reserved2,
                          IStream **ppstm);
    STDMETHOD(CreateStorage)(WCHAR const *pwcsName,
                             DWORD grfMode,
                             DWORD stgType,
                             DWORD /* BUGBUG LPSTGSECURITY */ pssSecurity,
                             IStorage **ppstg);
    STDMETHOD(OpenStorage)(WCHAR const *pwcsName,
                           IStorage *pstgPriority,
                           DWORD grfMode,
                           SNB snbExclude,
                           DWORD reserved,
                           IStorage **ppstg);
    STDMETHOD(CopyTo)(DWORD ciidExclude,
		      IID const *rgiidExclude,
		      SNB snbExclude,
		      IStorage *pstgDest);
    STDMETHOD(MoveElementTo)(WCHAR const *lpszName,
    			     IStorage *pstgDest,
                             WCHAR const *lpszNewName,
                             DWORD grfFlags);
    STDMETHOD(Commit)(DWORD grfCommitFlags);
    STDMETHOD(Revert)(void);
    STDMETHOD(EnumElements)(DWORD reserved1,
			    void *reserved2,
			    DWORD reserved3,
			    IEnumSTATSTG **ppenm);
    STDMETHOD(DestroyElement)(WCHAR const *pwcsName);
    STDMETHOD(RenameElement)(WCHAR const *pwcsOldName,
                             WCHAR const *pwcsNewName);
    STDMETHOD(SetElementTimes)(const WCHAR *lpszName,
    			       FILETIME const *pctime,
                               FILETIME const *patime,
                               FILETIME const *pmtime);
    STDMETHOD(SetClass)(REFCLSID clsid);
    STDMETHOD(SetStateBits)(DWORD grfStateBits, DWORD grfMask);
    STDMETHOD(Stat)(STATSTG *pstatstg, DWORD grfStatFlag);

    STDMETHOD(GetHandle(HANDLE *ph));

protected:
    inline SCODE Validate(void) const;
private:
    virtual SCODE ExtValidate(void);
    SCODE ValidateMode(DWORD grfMode);

    ULONG _sig;
    DWORD _grfMode;
};

SAFE_INTERFACE_PTR(SafeCOfsDirStorage, COfsDirStorage);

#define COFSDIRSTORAGE_SIG LONGSIG('O', 'D', 'S', 'G')
#define COFSDIRSTORAGE_SIGDEL LONGSIG('O', 'd', 'S', 'g')

//+--------------------------------------------------------------
//
//  Member:	COfsDirStorage::Validate, private
//
//  Synopsis:	Validates the class signature
//
//  Returns:	Returns STG_E_INVALIDHANDLE for failure
//
//  History:	24-Jun-93	DrewB	Created
//
//---------------------------------------------------------------

inline SCODE COfsDirStorage::Validate(void) const
{
    return (this == NULL || _sig != COFSDIRSTORAGE_SIG) ?
	STG_E_INVALIDHANDLE : S_OK;
}

#endif // #ifndef __DIRSTG_HXX__
