
//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1992 - 1993.
//
//  File:	lockbyte.cpp
//
//  Contents:	Apis for working with the standard ILockByte implementation
//		on memory
//
//  Classes:
//
//  Functions:	CreateILockBytesOnHGlobal
//		GetHGlobalFromILockBytes
//
//  History:    dd-mmm-yy Author    Comment
//		11-Jan-93 alexgo    added VDATEHEAP macros to every function
//				    fixed compile warnings
//		16-Dec-93 alexgo    fixed bad memory bugs
//		02-Dec-93 alexgo    32bit port
//		15-Sep-92 jasonful  author
//
//--------------------------------------------------------------------------

#include <le2int.h>
#pragma SEG(lockbyte)

#include "memstm.h"
#include <reterr.h>

NAME_SEG(LockBytes)
ASSERTDATA


//+-------------------------------------------------------------------------
//
//  Function:  	CreateILockBytesOnHGlobal 
//
//  Synopsis:   Creates a CMemBytes on the given HGlobal
//
//  Effects:    
//
//  Arguments: 	[hGlobal]	-- the memory to use (may be NULL) 
//		[fDeleteOnRelease]	-- if TRUE, then [hGlobal will
//					   be freed when CMemBytes is
//					   freed via a Release
//		[pplkbyt]	-- where to put the pointer to the CMemByte
//				   instance
//  Requires:   
//
//  Returns:    HRESULT
//
//  Signals:    
//
//  Modifies:   
//
//  Algorithm:
//
//  History:    dd-mmm-yy Author    Comment
//		11-Jan-94 alexgo    removed initialization of cbSize to -1
//			 	    to fix a compile warning
//		16-Dec-93 alexgo    fixed bogus usage of MAKELONG (turned
//				    into a GlobalLock)
//		02-Dec-93 alexgo    32bit port, fixed memory leak bug
//
//  Notes:  	REVIEW32:  It's fine to *ask* for shared memory on NT, you
//		just won't get it.  We need to make sure that any callers
//		(looks like apps at the moment) don't have the wrong idea :)
//
//--------------------------------------------------------------------------

#pragma SEG(CreateILockBytesOnHGlobal)
STDAPI CreateILockBytesOnHGlobal
	(HGLOBAL			hGlobal,
	BOOL				fDeleteOnRelease,
	LPLOCKBYTES FAR*		pplkbyt)
{
	VDATEHEAP();

	HANDLE				hMem	= NULL; 						
    	struct MEMSTM FAR*   		pData	= NULL;
    	ILockBytes FAR* 	 	pBytes	= NULL;
	DWORD 		 		cbSize;
	BOOL				fAllochGlobal = FALSE;
	
	VDATEPTRIN (pplkbyt, LPLOCKBYTES);
	*pplkbyt = NULL;

    	if (NULL==hGlobal)
	{
		hGlobal = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, 0);
	    	if (hGlobal == NULL)
	    	{
    	    		goto ErrorExit;
    	    	}
    	    	fAllochGlobal = TRUE;
    	    	
    		cbSize = 0;
	}
	else
	{
		cbSize = GlobalSize (hGlobal);
		// Is there a way to verify a zero-sized handle?
		if (cbSize!=0)
		{
			// verify validity of passed-in handle
			if (NULL==GlobalLock(hGlobal))
			{
				// bad handle
				return ResultFromScode (E_INVALIDARG);
			}
			GlobalUnlock (hGlobal);
		}
	}

	hMem = GlobalAlloc (GMEM_DDESHARE | GMEM_MOVEABLE, sizeof (MEMSTM));
    	if (hMem == NULL)
    	{
   	    	goto ErrorExit;
  	}

	pData = (MEMSTM FAR *)GlobalLock(hMem);
	
    	if (pData == NULL)
    	{
   	    goto FreeMem;
	}

	pData->cRef = 0;
   	pData->cb = cbSize;
	pData->fDeleteOnRelease = fDeleteOnRelease;
	pData->hGlobal = hGlobal;

    	pBytes = CMemBytes::Create(hMem); // Create the ILockBytes
    	
    	if (pBytes == NULL)
    	{
        	goto FreeMem;
        }

    	*pplkbyt = pBytes;
    	GlobalUnlock(hMem);
    	
	return NOERROR;
	
FreeMem:
	if (pData)
	{
		GlobalUnlock(hMem);
	}	
	if (hMem)
	{
	    GlobalFree(hMem);
	}

	if (fAllochGlobal && hGlobal )
	{
		GlobalFree(hGlobal);
	}
ErrorExit:
	Assert (0);
    	return ResultFromScode(E_OUTOFMEMORY);
}



//+-------------------------------------------------------------------------
//
//  Function: 	GetHGlobalFromILockBytes  
//
//  Synopsis:   Retrieves the hGlobal the ILockBytes was created with
//
//  Effects:    
//
//  Arguments:  [plkbyt]	-- pointer to the ILockBytes implementation
//		[phglobal]	-- where to put the hglobal
//
//  Requires:   
//
//  Returns:    HRESULT
//
//  Signals:    
//
//  Modifies:   
//
//  Algorithm:  hacked--does a pointer cast and checks the signature :( :(
//
//  History:    dd-mmm-yy Author    Comment
//		02-Dec-93 alexgo    32bit port
//
//  Notes:      
//
//--------------------------------------------------------------------------

#pragma SEG(GetHGlobalFromILockBytes)
STDAPI GetHGlobalFromILockBytes
	(LPLOCKBYTES 	plkbyt,
	HGLOBAL	FAR*	phglobal)
{
	VDATEHEAP();

	VDATEIFACE (plkbyt);
	VDATEPTRIN (phglobal, HANDLE);
	
	*phglobal = NULL;
	CMemBytes FAR* pCMemByte = (CMemBytes FAR*)plkbyt;

	if (IsBadReadPtr (&(pCMemByte->m_dwSig), sizeof(ULONG))
		|| pCMemByte->m_dwSig != LOCKBYTE_SIG)
	{
		// we were passed someone else's implementation of ILockBytes
		return ResultFromScode (E_INVALIDARG);
	}

	MEMSTM FAR* pMem= pCMemByte->m_pData;
	if (NULL==pMem)
	{
		Assert (0);
		return ResultFromScode (E_OUTOFMEMORY);
	}
	Assert (pMem->cb <= GlobalSize (pMem->hGlobal));
	Verify (*phglobal = pMem->hGlobal);

	return NOERROR;
}
