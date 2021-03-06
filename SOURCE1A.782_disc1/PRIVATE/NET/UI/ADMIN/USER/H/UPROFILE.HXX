/**********************************************************************/
/**			  Microsoft Windows NT  		     **/
/**		Copyright(c) Microsoft Corp., 1991		     **/
/**********************************************************************/

/*
    UProfile.hxx

    Header file for the Profile subdialog class

    USERPROF_DLG is the Profile subdialog class.  This header file
    describes this class.  The inheritance diagram is as follows:

	 ...
	  |
    DIALOG_WINDOW  PERFORMER
	       \    /
            BASEPROP_DLG
	    /		\
	SUBPROP_DLG   PROP_DLG
	   /		  \
    USER_SUBPROP_DLG    USERPROP_DLG
	  |
     USERPROF_DLG
        |       \
USERPROF_DLG_NT  USERPROF_DLG_DOWNLEVEL



    FILE HISTORY:
	JonN	18-Feb-1992	Templated from useracct.hxx
	JonN	27-Feb-1992	Split USERPROF_DLG into DOWNLEVEL and NT variants
	JonN	10-Mar-1992	Added %USERNAME% logic
	JonN	22-Apr-1992	Reformed %USERNAME% logic (NTISSUE #974)
	JonN	28-Apr-1992	Enabled USERPROF_DLG_NT
	JonN	10-Jun-1992	Profile field only from LANMANNT variants
        JonN    31-Aug-1992     Re-enable %USERNAME%
*/

#ifndef _USERPROF_DLG
#define _USERPROF_DLG


#include <userprop.hxx>
#include <usubprop.hxx>
#include <slestrip.hxx>
#include <devcb.hxx>    // DEVICE_COMBO


class LAZY_USER_LISTBOX;

/*************************************************************************

    NAME:	USERPROF_DLG

    SYNOPSIS:	USERPROF_DLG is the class for the Profile subdialog.

    INTERFACE:	USERPROF_DLG()	-	constructor
    		~USERPROF_DLG()	-	destructor

                GeneralizeString() -    If this is not a CloneUser variant,
                                        do nothing.  Otherwise, if the last
                                        path component of the string is
                                        <ClonedUsername><Extension>, replace
                                        that with %USERNAME%<Extension>.
                DeGeneralizeString() -  If this is not a CloneUser variant,
                                        do nothing.  Otherwise, if the last
                                        path component of the string is
                                        %USERNAME%<Extension>, replace
                                        this with <CurrentUsername><Extension>.

    PARENT:	USER_SUBPROP_DLG

    NOTES:	_fIndeterminateX: TRUE iff multiple users are
		selected who did not originally all have
		the same X value.

    HISTORY:
	JonN	18-Feb-1992	Templated from useracct.cxx
	JonN	10-Mar-1992	Added %USERNAME% logic
	JonN	22-Apr-1992	Reformed %USERNAME% logic (NTISSUE #974)
        JonN    31-Aug-1992     Re-enable %USERNAME%

**************************************************************************/

class USERPROF_DLG: public USER_SUBPROP_DLG
{
friend class USERPROF_DLG_NT; // in order to _sleLogonScript.ClaimFocus()

private:

    NLS_STR	  _nlsLogonScript;
    BOOL	  _fIndeterminateLogonScript;
    BOOL	  _fIndetNowLogonScript;
    SLE_STRIP	  _sleLogonScript;

protected:

    APIERR	  CheckPath(
	SLE_STRIP *	sle,
	NLS_STR *	pnls,
	BOOL    	fIndeterminate,
	BOOL    *	pfIndetNow,
	ULONG		ulType,
	ULONG		ulMask,
        BOOL            fDoDegeneralize = FALSE,
        const NLS_STR * pnlsDegeneralizeExtension = NULL
	);

    /* inherited from BASEPROP_DLG */
    virtual APIERR InitControls();

    virtual ULONG QueryHelpContext( void );

    /* four next ones inherited from USER_SUBPROP_DLG */
    virtual APIERR W_LMOBJtoMembers( UINT iObject );

    virtual APIERR W_MembersToLMOBJ(
	USER_2 *	puser2,
	USER_MEMB *	pusermemb
	);

    virtual APIERR W_DialogToMembers();

    virtual BOOL ChangesUser2Ptr( UINT iObject );

    inline APIERR GeneralizeString  ( NLS_STR * pnlsGeneralizeString,
                                      const TCHAR * pchGeneralizeFromUsername,
                                      const NLS_STR * pnlsExtension = NULL )
        { return QueryParent()->GeneralizeString( pnlsGeneralizeString,
                                                  pchGeneralizeFromUsername,
                                                  pnlsExtension ); }

    inline APIERR DegeneralizeString( NLS_STR * pnlsDegeneralizeString,
                                      const TCHAR * pchDegeneralizeToUsername,
                                      const NLS_STR * pnlsExtension = NULL,
                                      BOOL * pfDidDegeneralize = NULL )
        { return QueryParent()->DegeneralizeString( pnlsDegeneralizeString,
                                                    pchDegeneralizeToUsername,
                                                    pnlsExtension,
                                                    pfDidDegeneralize ); }


public:

    USERPROF_DLG( USERPROP_DLG * puserpropdlgParent,
                  const TCHAR * pszResourceName,
    		  const LAZY_USER_LISTBOX * pulb );

    ~USERPROF_DLG();

} ;


/*************************************************************************

    NAME:	USERPROF_DLG_DOWNLEVEL

    SYNOPSIS:	USERPROF_DLG_DOWNLEVEL is the class for the Profile subdialog,
                LM2.x variant.

    INTERFACE:	USERPROF_DLG_DOWNLEVEL()	-	constructor
    		~USERPROF_DLG_DOWNLEVEL()	-	destructor

    PARENT:	USERPROF_DLG

    NOTES:	_fIndeterminateX: TRUE iff multiple users are
		selected who did not originally all have
		the same X value.

    HISTORY:
	JonN	27-Feb-1992	Split from USERPROF_DLG
	JonN	22-Apr-1992	Reformed %USERNAME% logic (NTISSUE #974)

**************************************************************************/

class USERPROF_DLG_DOWNLEVEL: public USERPROF_DLG
{
private:

    NLS_STR	  _nlsHomeDir;
    BOOL	  _fIndeterminateHomeDir;
    BOOL	  _fIndetNowHomeDir;
    SLE_STRIP	  _sleHomeDir;

protected:

    /* inherited from BASEPROP_DLG */
    virtual APIERR InitControls();

    /* four next ones inherited from USER_SUBPROP_DLG */
    virtual APIERR W_LMOBJtoMembers( UINT iObject );

    virtual APIERR W_MembersToLMOBJ(
	USER_2 *	puser2,
	USER_MEMB *	pusermemb
	);

    virtual APIERR W_DialogToMembers();

public:

    USERPROF_DLG_DOWNLEVEL( USERPROP_DLG * puserpropdlgParent,
    		  const LAZY_USER_LISTBOX * pulb );

    ~USERPROF_DLG_DOWNLEVEL();

} ;


/*************************************************************************

    NAME:	USERPROF_DLG_NT

    SYNOPSIS:	USERPROF_DLG_NT is the class for the Profile subdialog,
                NT variant.

    INTERFACE:	USERPROF_DLG_NT()	-	constructor
    		~USERPROF_DLG_NT()	-	destructor

    PARENT:	USERPROF_DLG

    NOTES:	_fIndeterminateX: TRUE iff multiple users are
		selected who did not originally all have
		the same X value.

    HISTORY:
	JonN	27-Feb-1992	Split from USERPROF_DLG
	JonN	22-Apr-1992	Reformed %USERNAME% logic (NTISSUE #974)
	JonN	28-Apr-1992	Enabled USERPROF_DLG_NT, nuked UserMaint stuff
	JonN	10-Jun-1992	Profile field only from LANMANNT variants
        JonN    18-Aug-1992     Added _psltProfileText

**************************************************************************/

class USERPROF_DLG_NT: public USERPROF_DLG
{
private:

    NLS_STR	  _nlsProfile;
    BOOL	  _fIndeterminateProfile;
    BOOL	  _fIndetNowProfile;
    SLE_STRIP *	  _psleProfile;
    SLT *         _psltProfileText;

    BOOL          _fIsLocalHomeDir;
    BOOL          _fIndeterminateIsLocalHomeDir;
    BOOL          _fIndetNowIsLocalHomeDir;
    MAGIC_GROUP   _mgrpIsLocalHomeDir;

    NLS_STR	  _nlsLocalHomeDir;
    BOOL	  _fIndeterminateLocalHomeDir;
    BOOL	  _fIndetNowLocalHomeDir;
    SLE_STRIP	  _sleLocalHomeDir;

    NLS_STR       _nlsRemoteDevice;
    BOOL          _fIndeterminateRemoteDevice;
    BOOL          _fIndetNowRemoteDevice;
    DEVICE_COMBO  _devcbRemoteDevice;

    NLS_STR	  _nlsRemoteHomeDir;
    BOOL	  _fIndeterminateRemoteHomeDir;
    BOOL	  _fIndetNowRemoteHomeDir;
    SLE_STRIP     _sleRemoteHomeDir;

protected:

    /* inherited from BASEPROP_DLG */
    virtual APIERR InitControls();

    /* four next ones inherited from USER_SUBPROP_DLG */
    virtual APIERR W_LMOBJtoMembers( UINT iObject );

    virtual APIERR W_MembersToLMOBJ(
	USER_2 *	puser2,
	USER_MEMB *	pusermemb
	);

    virtual APIERR W_DialogToMembers();

public:

    USERPROF_DLG_NT( USERPROP_DLG * puserpropdlgParent,
    		  const LAZY_USER_LISTBOX * pulb );

    ~USERPROF_DLG_NT();

} ;

#endif //_USERPROF_DLG
