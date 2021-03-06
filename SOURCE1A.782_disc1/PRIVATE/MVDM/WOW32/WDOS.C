/* wdos.c - DOS realted functions for WOW
 *
 * Modification History
 *
 * Sudeepb 23-Aug-1991 Created
 */

#include "precomp.h"
#pragma hdrstop
#include "curdir.h"


MODNAME(wdos.c);

ULONG demClientErrorEx (HANDLE hFile, CHAR chDrive, BOOL bSetRegs);

extern DOSWOWDATA DosWowData;
extern PWORD16 pCurTDB, pCurDirOwner;


/* GetCurrentDir - Updatess current dir in CDS structure
 *
 * Entry - pcds    = pointer to CDS
 *         chDrive = Physical Drive in question (0, 1 ...)
 *
 * Exit
 *      SUCCESS - returns TRUE
 *
 *      FAILURE - returns FALSE
 */
BOOL GetCurrentDir (PCDS pcds, UCHAR Drive)
{
    CHAR  EnvVar[] = "=?:";
    DWORD EnvVarLen;
    BOOL bStatus = TRUE;
    UCHAR FixedCount;
    int i;
    PCDS pcdstemp;

    FixedCount = *(PUCHAR) DosWowData.lpCDSCount;
    //
    // from Macro.Asm in DOS:
    // ; Sudeepb 20-Dec-1991 ; Added for redirected drives
    // ; We always sync the redirected drives. Local drives are sync
    // ; as per the curdir_tosync flag and SCS_ToSync
    //

    if (*(PUCHAR)DosWowData.lpSCS_ToSync) {

        pcdstemp = (PCDS) DosWowData.lpCDSFixedTable;
        for (i=0;i < (int)FixedCount; i++, pcdstemp++)
            pcdstemp->CurDir_Flags |= CURDIR_TOSYNC;

        // Mark tosync in network drive as well
        pcdstemp = (PCDS)DosWowData.lpCDSBuffer;
        pcdstemp->CurDir_Flags |= CURDIR_TOSYNC;

        *(PUCHAR)DosWowData.lpSCS_ToSync = 0;
    }

    // If CDS needs to be synched or if the requested drive is different
    // then the the drive being used by NetCDS go refresh the CDS.
    if ((pcds->CurDir_Flags & CURDIR_TOSYNC) ||
        ((Drive >= FixedCount) && (pcds->CurDir_Text[0] != (Drive + 'A') &&
                                   pcds->CurDir_Text[0] != (Drive + 'a')))) {
        // validate media
        EnvVar[1] = Drive + 'A';
        if((EnvVarLen = GetEnvironmentVariableOem (EnvVar, (LPSTR)pcds,
                                                MAXIMUM_VDM_CURRENT_DIR+3)) == 0){

        // if its not in env then and drive exist then we have'nt
        // yet touched it.

            pcds->CurDir_Text[0] = EnvVar[1];
            pcds->CurDir_Text[1] = ':';
            pcds->CurDir_Text[2] = '\\';
            pcds->CurDir_Text[3] = 0;
            SetEnvironmentVariableOem ((LPSTR)EnvVar,(LPSTR)pcds);
        }

        if (EnvVarLen > MAXIMUM_VDM_CURRENT_DIR+3) {
            bStatus = FALSE;
        }
        pcds->CurDir_Flags &= 0xFFFF - CURDIR_TOSYNC;
        pcds->CurDir_End = 2;

    }

    if (!bStatus) {

        *(PUCHAR)DosWowData.lpDrvErr = ERROR_INVALID_DRIVE;
    }

    return (bStatus);

}

/* SetCurrentDir - Set the current directory
 *
 *
 * Entry - lpBuf   = pointer to string specifying new directory
 *         chDrive = Physical Drive in question (0, 1 ...)
 *
 * Exit
 *     SUCCESS returns TRUE
 *     FAILURE returns FALSE
 *
 */

BOOL SetCurrentDir (LPSTR lpBuf, UCHAR Drive)
{
    CHAR EnvVar[] = "=?:";
    CHAR chDrive = Drive + 'A';
    BOOL bRet = FALSE;

    if (SetCurrentDirectoryOem (lpBuf) == FALSE){
        demClientErrorEx(INVALID_HANDLE_VALUE, chDrive, FALSE);
    } else {

        EnvVar[1] = chDrive;
        if (SetEnvironmentVariableOem ((LPSTR)EnvVar,lpBuf) == TRUE) {
            bRet = TRUE;
        }
    }

    return (bRet);
}


/* QueryCurrentDir - Verifies current dir provided in CDS structure
 *                      for $CURRENT_DIR
 *
 * First Validates Media, if invalid -> i24 error
 * Next  Validates Path, if invalid set path to root (not an error)
 *
 * Entry - Client (DS:SI) Buffer to CDS path to verify
 *     Client (AL)    Physical Drive in question (A=0, B=1, ...)
 *
 * Exit
 *     SUCCESS
 *       Client (CY) = 0
 *
 *         FAILURE
 *           Client (CY) = 1 , I24 drive invalid
 */
BOOL QueryCurrentDir (PCDS pcds, UCHAR Drive)
{
    DWORD dw;
    CHAR  chDrive;
    CHAR  pPath[]="?:\\";
    CHAR  EnvVar[] = "=?:";

    // validate media
    chDrive = Drive + 'A';
    pPath[0] = chDrive;
    dw = GetFileAttributesOem(pPath);
    if (dw == 0xFFFFFFFF || !(dw & FILE_ATTRIBUTE_DIRECTORY))
      {
        demClientErrorEx(INVALID_HANDLE_VALUE, chDrive, FALSE);
        return (FALSE);
        }

    // if invalid path, set path to the root
    // reset CDS, and win32 env for win32
    dw = GetFileAttributesOem(pcds->CurDir_Text);
    if (dw == 0xFFFFFFFF || !(dw & FILE_ATTRIBUTE_DIRECTORY))
      {
        strcpy(pcds->CurDir_Text, pPath);
        pcds->CurDir_End = 2;
        EnvVar[1] = chDrive;
        SetEnvironmentVariableOem(EnvVar,pPath);
        }

    return (TRUE);
}


/* strcpyCDS - copies CDS paths
 *
 *  This routine emulates how DOS was coping the directory path. It is
 *  unclear if it is still necessary to do it this way.
 *
 * Entry -
 *
 * Exit
 *     SUCCESS
 *
 *         FAILURE
 */
VOID strcpyCDS (PCDS source, LPSTR dest)
{
    char ch;
    int index;

    index = source->CurDir_End;

    if (source->CurDir_Text[index]=='\\')
        index++;

    while (ch = source->CurDir_Text[index]) {

        if ((ch == 0x05) && (source->CurDir_Text[index-1] == '\\')) {
            ch = (CHAR) 0xE5;
        }

        *dest++ = toupper(ch);
        index++;
    }

    *dest = ch;                                 // trailing zero

}


/* GetCDSFromDrv - Updates current dir in CDS structure
 *
 * Entry - Drive    = Physical Drive in question (0, 1 ...)
 *
 * Exit
 *      SUCCESS - returns v86 pointer to CDS structure in DOS
 *
 *      FAILURE - returns 0
 */

PCDS GetCDSFromDrv (UCHAR Drive)
{
    PCDS  pCDS = NULL;
    CHAR  pPath[]="?:\\";
    CHAR  chDrive;
     //
    // Is Drive valid?
    //

    if (Drive >= *(PUCHAR)DosWowData.lpCDSCount) {

        if (Drive <= 25) {

            chDrive = Drive + 'A';
            pPath[0] = chDrive;

            //
            // test to see if non-fixed/floppy drive exists
            //

            if ((*(PUCHAR)DosWowData.lpCurDrv == Drive) ||
                (GetDriveType(pPath) > 1)) {

                //
                // Network drive
                //

                pCDS = (PCDS) DosWowData.lpCDSBuffer;
            }

        }

    } else {

        chDrive = Drive + 'A';
        pPath[0] = chDrive;
        if ((Drive != 1) || (DRIVE_REMOVABLE == GetDriveType(pPath))) {

            //
            // Drive defined in fixed table
            //

            pCDS = (PCDS) DosWowData.lpCDSFixedTable;
            pCDS = (PCDS)((ULONG)pCDS + (Drive*sizeof(CDS)));
            }

    }

    return (pCDS);
}


/* DosWowSetDefaultDrive - Emulate DOS set default drive call
 *
 * Entry -
 *  BYTE  DriveNum;    = drive number to switch to
 *
 * Exit
 *       returns client AX
 *
 */

ULONG DosWowSetDefaultDrive(UCHAR Drive)
{
    PCDS pCDS;

    if (NULL != (pCDS = GetCDSFromDrv (Drive))) {

        if (GetCurrentDir (pCDS, Drive)) {

            if (*(PUCHAR)DosWowData.lpCurDrv != Drive) {

                // The upper bit in the TDB_Drive byte is used to indicate
                // that the current drive and directory information in the
                // TDB is stale. Turn it off here.
                PTDB pTDB;
                if (*pCurTDB) {
                    pTDB = (PTDB)SEGPTR(*pCurTDB,0);
                    if (TDB_SIGNATURE == pTDB->TDB_sig) {
                        pTDB->TDB_Drive &= ~TDB_DIR_VALID;
                    }
                }

                *(PUCHAR)DosWowData.lpCurDrv = Drive;
            }

        }
    }

    return (*(PUCHAR)DosWowData.lpCurDrv);

}


/* DosWowGetCurrentDirectory - Emulate DOS Get current Directory call
 *
 *
 * Entry -
 *    Drive - Drive number for directory request
 *    pszDir- pointer to receive directory
 *
 * Exit
 *     SUCCESS
 *       0
 *
 *     FAILURE
 *       system status code
 *
 */
ULONG DosWowGetCurrentDirectory(UCHAR Drive, LPSTR pszDir)
{
    PCDS pCDS;
    DWORD dwRet = 0xFFFF000F;       // assume error

    //
    // Handle default drive value of 0
    //

    if (Drive == 0) {
        Drive = *(PUCHAR)DosWowData.lpCurDrv;
    } else {
        Drive--;
    }

    if (NULL != (pCDS = GetCDSFromDrv (Drive))) {

        if (GetCurrentDir (pCDS, Drive)) {
            // for removable media we need to check that media is present.
            // fixed disks, network drives and CDROM drives are fixed drives in
            // DOS. sudeepb 30-Dec-1993
            if (!(pCDS->CurDir_Flags & CURDIR_NT_FIX)) {
                if(QueryCurrentDir (pCDS, Drive) == FALSE)
                    return (dwRet);         // fail
            }
            strcpyCDS(pCDS, pszDir);
            dwRet = 0;
        }
    }

    return (dwRet);

}

/* DosWowSetCurrentDirectory - Emulate DOS Set current Directory call
 *
 *
 * Entry -
 *    lpDosDirectory - pointer to new DOS directory
 *
 * Exit
 *     SUCCESS
 *       0
 *
 *     FAILURE
 *       system status code
 *
 */
ULONG DosWowSetCurrentDirectory(LPSTR pszDir)
{
    PCDS pCDS;
    UCHAR Drive;
    LPTSTR pLast;
    PSTR lpDirName;
    UCHAR szPath[MAXIMUM_VDM_CURRENT_DIR+3];
    DWORD dwRet = 0xFFFF0003;       // assume error
    CHAR  EnvVar[] = "=?:";
    BOOL  ItsANamedPipe = FALSE;

    if (':' == pszDir[1]) {
        Drive = toupper(pszDir[0]) - 'A';
    } else {
        Drive = *(PUCHAR)DosWowData.lpCurDrv;
    }

    if (NULL != (pCDS = GetCDSFromDrv (Drive))) {

        lpDirName = NormalizeDosPath(pszDir, Drive, &ItsANamedPipe);

        GetFullPathName(lpDirName, MAXIMUM_VDM_CURRENT_DIR+3, szPath, &pLast);

        if (SetCurrentDir(szPath, Drive)) {
            PTDB pTDB;
            strcpy(&pCDS->CurDir_Text[0], szPath);
            dwRet = 0;

            //
            // Update kernel16's "directory owner" with the current TDB.
            // Also, if we are changing to a directory that is different than
            // what is currently in the TDB, then mark it as invalid.
            //
            if (*pCurTDB) {
                pTDB = (PTDB)SEGPTR(*pCurTDB,0);
                if (TDB_SIGNATURE == pTDB->TDB_sig) {
                    *pCurDirOwner = *pCurTDB;
                    if ((pTDB->TDB_Drive&TDB_DIR_VALID) &&
                        (*(PUCHAR)DosWowData.lpCurDrv == Drive) &&
                        ((Drive != (pTDB->TDB_Drive & ~TDB_DIR_VALID)) ||
                            (strcmp(&szPath[2], pTDB->TDB_Directory)))) {
                        pTDB->TDB_Drive &= ~TDB_DIR_VALID;
                    }
                }
            }
        }

    }

    return (dwRet);
}


//*****************************************************************************
// UpdateDosCurrentDirectory -
//
// Entry -
//    fDir - specifies which directory should be updated
//
// Exit - 
//    TRUE if the update was successful, FALSE otherwise
//
// Notes:
//
// There are actually three different current directories:
// - The WIN32 current directory (this is really the one that counts)
// - The DOS current directory, kept on a per drive basis 
// - The TASK current directory, kept in the TDB of a win16 task
//
// It is the responsibility of this routine to effectively copy the contents
// of one of these directories into another. From where to where is determined
// by the passed parameter, so it is the caller's responsibility to be sure
// what exactly needs to be sync'd up with what.
//
//*****************************************************************************

BOOL UpdateDosCurrentDirectory(UDCDFUNC fDir)
{
    LONG   lReturn = (LONG)FALSE;

    switch(fDir)  {

        case DIR_DOS_TO_NT: {

            UCHAR szPath[MAXIMUM_VDM_CURRENT_DIR+5] = "?:\\";
            PTDB pTDB;
           
            WOW32ASSERT(DosWowData.lpCurDrv != (ULONG) NULL);

            if ((*pCurTDB) && (*pCurDirOwner != *pCurTDB)) {

                pTDB = (PTDB)SEGPTR(*pCurTDB,0);

                if ((TDB_SIGNATURE == pTDB->TDB_sig) &&
                    (pTDB->TDB_Drive & TDB_DIR_VALID)) {

                    szPath[0] = 'A' + (pTDB->TDB_Drive & ~TDB_DIR_VALID);
                    strcpy(&szPath[2], pTDB->TDB_Directory);

                    SetCurrentDirectoryOem(szPath);
                    break;          // EXIT case
                }
            }


            szPath[0] = *(PUCHAR)DosWowData.lpCurDrv + 'A';
            
            if (DosWowGetCurrentDirectory(0, &szPath[3])) {
                LOGDEBUG(LOG_ERROR, ("DowWowGetCurrentDirectory failed\n"));
            } else {
                SetCurrentDirectoryOem(szPath);
                lReturn = TRUE;
            }
            break;
        }

        case DIR_NT_TO_DOS: {

            UCHAR szPath[MAXIMUM_VDM_CURRENT_DIR+5];
           
            if (((GetCurrentDirectoryOem(0, NULL)+1) > MAXIMUM_VDM_CURRENT_DIR+5)||
                (!GetCurrentDirectoryOem(MAXIMUM_VDM_CURRENT_DIR+5, &szPath[0]))) {
           
                LOGDEBUG(LOG_ERROR, ("DowWowSetCurrentDirectory failed\n"));
           
            } else {
           
                LOGDEBUG(LOG_WARNING, ("UpdateDosCurrentDirectory: %s\n", &szPath[0]));
                if (szPath[1] == ':') {
                    DosWowSetDefaultDrive((UCHAR) (szPath[0] - 'A'));
                    DosWowSetCurrentDirectory(szPath);
                    lReturn = TRUE;
                }
           
            }
            break;
        }

    }
    return (BOOL)lReturn;
}

/***************************************************************************

 Stub entry points (called by KRNL386, 286 via thunks)

 ***************************************************************************/


/* WK32SetDefaultDrive - Emulate DOS set default drive call
 *
 * Entry -
 *  BYTE  DriveNum;    = drive number to switch to
 *
 * Exit
 *       returns client AX
 *
 */

ULONG FASTCALL WK32SetDefaultDrive(PVDMFRAME pFrame)
{
    PWOWSETDEFAULTDRIVE16   parg16;
    UCHAR Drive;

    GETARGPTR(pFrame, sizeof(WOWSETDEFAULTDRIVE16), parg16);

    Drive = (UCHAR) parg16->wDriveNum;

    FREEARGPTR(parg16);

    return (DosWowSetDefaultDrive (Drive));

}


/* WK32SetCurrentDirectory - Emulate DOS set current Directory call
 *
 * Entry -
 *    DWORD lpDosData    = pointer to DosWowData structure in DOS
 *    parg16->lpDosDirectory - pointer to real mode DOS pdb variable
 *    parg16->wNewDirectory  - 16-bit pmode selector for new Directory
 *
 * Exit
 *     SUCCESS
 *       0
 *
 *     FAILURE
 *       system status code
 *
 */
ULONG FASTCALL WK32SetCurrentDirectory(PVDMFRAME pFrame)
{

    PWOWSETCURRENTDIRECTORY16   parg16;
    LPSTR pszDir;

    GETARGPTR(pFrame, sizeof(WOWSETCURRENTDIRECTORY16), parg16);

    pszDir = GetPModeVDMPointer(FETCHDWORD(parg16->lpCurDir), 4);

    FREEARGPTR(parg16);

    return (DosWowSetCurrentDirectory (pszDir));

}


/* WK32GetCurrentDirectory - Emulate DOS Get current Directory call
 *
 *
 * Entry -
 *    DWORD lpDosData    = pointer to DosWowData structure in DOS
 *    parg16->lpDosDirectory - pointer to real mode DOS pdb variable
 *    parg16->wNewDirectory  - 16-bit pmode selector for new Directory
 *
 * Exit
 *     SUCCESS
 *       0
 *
 *     FAILURE
 *       system status code
 *
 */
ULONG FASTCALL WK32GetCurrentDirectory(PVDMFRAME pFrame)
{
    PWOWGETCURRENTDIRECTORY16   parg16;
    LPSTR pszDir;
    UCHAR Drive;

    GETARGPTR(pFrame, sizeof(WOWGETCURRENTDIRECTORY16), parg16);

    pszDir = GetPModeVDMPointer(FETCHDWORD(parg16->lpCurDir), 4);

    Drive = (UCHAR) parg16->wDriveNum;

    FREEARGPTR(parg16);

    return (DosWowGetCurrentDirectory (Drive, pszDir));

}

/* WK32GetCurrentDate - Emulate DOS Get current Date call
 *
 *
 * Entry -
 *
 * Exit
 *    return value is packed with date information
 *
 */
ULONG FASTCALL WK32GetCurrentDate(PVDMFRAME pFrame)
{
    SYSTEMTIME systemtime;

    UNREFERENCED_PARAMETER(pFrame);

    GetLocalTime(&systemtime);

    return ((DWORD) (systemtime.wYear  << 16 |
                     systemtime.wDay   << 8  |
                     systemtime.wMonth << 4  |
                     systemtime.wDayOfWeek
                     ));

}


/* WK32GetCurrentTime - Emulate DOS Get current Time call
 *
 *
 * Entry -
 *
 * Exit
 *    return value is packed with time information
 *
 */
ULONG FASTCALL WK32GetCurrentTime(PVDMFRAME pFrame)
{
    SYSTEMTIME systemtime;

    UNREFERENCED_PARAMETER(pFrame);

    GetLocalTime(&systemtime);

    return ((DWORD) (systemtime.wHour   << 24 |
                     systemtime.wMinute << 16 |
                     systemtime.wSecond << 8  |
                     systemtime.wMilliseconds/10
                     ));

}

/* WK32DeviceIOCTL - Emulate misc. DOS IOCTLs
 *
 * Entry -
 *  BYTE  DriveNum;    = drive number
 *
 * Exit
 *       returns client AX
 *
 */

ULONG FASTCALL WK32DeviceIOCTL(PVDMFRAME pFrame)
{
    PWOWDEVICEIOCTL16   parg16;
    UCHAR Drive;
    UCHAR Cmd;
    DWORD dwReturn = 0xFFFF0001;        // error invalid function
    UINT uiDriveStatus;
    CHAR  pPath[]="?:\\";

    GETARGPTR(pFrame, sizeof(WOWDEVICEIOCTL16), parg16);

    Cmd = (UCHAR) parg16->wCmd;
    Drive = (UCHAR) parg16->wDriveNum;

    FREEARGPTR(parg16);

    if (Cmd != 8) {                     // Does Device Use Removeable Media
        return (dwReturn);
    }

    if (Drive == 0) {
        Drive = *(PUCHAR)DosWowData.lpCurDrv;
    } else {
        Drive--;
    }

    pPath[0] = Drive + 'A';
    uiDriveStatus = GetDriveType(pPath);

    if ((uiDriveStatus == 0) || (uiDriveStatus == 1)) {
        return (0xFFFF000F);            // error invalid drive
    }

    if (uiDriveStatus == DRIVE_REMOVABLE) {
        dwReturn = 0;
    } else {
        dwReturn = 1;
    }

    return (dwReturn);

}
