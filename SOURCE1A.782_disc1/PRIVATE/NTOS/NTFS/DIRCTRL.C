/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    DirCtrl.c

Abstract:

    This module implements the File Directory Control routine for Ntfs called
    by the dispatch driver.

Author:

    Tom Miller      [TomM]          1-Jan-1992

        (Based heavily on GaryKi's dirctrl.c for pinball.)

Revision History:

--*/

#include "NtfsProc.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_DIRCTRL)

NTSTATUS
NtfsQueryDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NtfsNotifyChangeDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCommonDirectoryControl)
#pragma alloc_text(PAGE, NtfsFsdDirectoryControl)
#pragma alloc_text(PAGE, NtfsNotifyChangeDirectory)
#pragma alloc_text(PAGE, NtfsQueryDirectory)
#endif


NTSTATUS
NtfsFsdDirectoryControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Directory Control.

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    NTSTATUS Status = STATUS_SUCCESS;
    PIRP_CONTEXT IrpContext = NULL;

    UNREFERENCED_PARAMETER( VolumeDeviceObject );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NtfsFsdDirectoryControl\n", 0);

    //
    //  Call the common Directory Control routine
    //

    FsRtlEnterFileSystem();

    //
    //  Always make these requests look top level.
    //

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, TRUE, TRUE );

    do {

        try {

            //
            //  We are either initiating this request or retrying it.
            //

            if (IrpContext == NULL) {

                IrpContext = NtfsCreateIrpContext( Irp, CanFsdWait( Irp ) );
                NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

            } else if (Status == STATUS_LOG_FILE_FULL) {

                NtfsCheckpointForLogFileFull( IrpContext );
            }

            Status = NtfsCommonDirectoryControl( IrpContext, Irp );
            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            Status = NtfsProcessException( IrpContext, Irp, GetExceptionCode() );
        }

    } while (Status == STATUS_CANT_WAIT ||
             Status == STATUS_LOG_FILE_FULL);

    if (ThreadTopLevelContext == &TopLevelContext) {
        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NtfsFsdDirectoryControl -> %08lx\n", Status);

    return Status;
}


NTSTATUS
NtfsCommonDirectoryControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for Directory Control called by both the fsd
    and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PSCB Scb;
    PCCB Ccb;
    PFCB Fcb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "NtfsCommonDirectoryControl\n", 0);
    DebugTrace( 0, Dbg, "IrpContext = %08lx\n", IrpContext);
    DebugTrace( 0, Dbg, "Irp        = %08lx\n", Irp);

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  The only type of opens we accept are user directory opens
    //

    if ((TypeOfOpen != UserDirectoryOpen)
        && (TypeOfOpen != UserOpenDirectoryById)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "NtfsCommonDirectoryControl -> STATUS_INVALID_PARAMETER\n", 0);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  We know this is a directory control so we'll case on the
    //  minor function, and call a internal worker routine to complete
    //  the irp.
    //

    switch ( IrpSp->MinorFunction ) {

    case IRP_MN_QUERY_DIRECTORY:

        Status = NtfsQueryDirectory( IrpContext, Irp, Vcb, Scb, Ccb );
        break;

    case IRP_MN_NOTIFY_CHANGE_DIRECTORY:

        //
        //  We can't perform this operation on open by Id or if the caller has
        //  closed his handle.
        //

        if (TypeOfOpen == UserOpenDirectoryById
            || FlagOn( FileObject->Flags, FO_CLEANUP_COMPLETE )) {

            NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

            DebugTrace(-1, Dbg, "NtfsCommonDirectoryControl -> STATUS_INVALID_PARAMETER\n", 0);
            return STATUS_INVALID_PARAMETER;
        }

        Status = NtfsNotifyChangeDirectory( IrpContext, Irp, Vcb, Scb, Ccb );
        break;

    default:

        DebugTrace(0, Dbg, "Invalid Minor Function %08lx\n", IrpSp->MinorFunction);

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_DEVICE_REQUEST );

        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NtfsCommonDirectoryControl -> %08lx\n", Status);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsQueryDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the query directory operation.  It is responsible
    for either completing of enqueuing the input Irp.

Arguments:

    Irp - Supplies the Irp to process

    Vcb - Supplies its Vcb

    Scb - Supplies its Scb

    Ccb - Supplies its Ccb

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;

    PUCHAR Buffer;
    CLONG UserBufferLength;

    PUNICODE_STRING UniFileName;
    FILE_INFORMATION_CLASS FileInformationClass;
    ULONG FileIndex;
    BOOLEAN RestartScan;
    BOOLEAN ReturnSingleEntry;
    BOOLEAN IndexSpecified;

    BOOLEAN IgnoreCase;

    BOOLEAN NextFlag;

    BOOLEAN GotEntry;

    BOOLEAN CallRestart;

    ULONG NextEntry;
    ULONG LastEntry;

    PFILE_DIRECTORY_INFORMATION DirInfo;
    PFILE_FULL_DIR_INFORMATION FullDirInfo;
    PFILE_BOTH_DIR_INFORMATION BothDirInfo;
    PFILE_NAMES_INFORMATION NamesInfo;

    PFILE_NAME FileNameBuffer;
    PVOID UnwindFileNameBuffer = NULL;
    ULONG FileNameLength;

    ULONG SizeOfFileName = FIELD_OFFSET( FILE_NAME, FileName );

    INDEX_CONTEXT OtherContext;

    BOOLEAN ScbAcquired = FALSE;
    BOOLEAN FirstQuery = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );
    ASSERT_VCB( Vcb );
    ASSERT_CCB( Ccb );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    //
    //  Get the current Stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "NtfsQueryDirectory...\n", 0);
    DebugTrace( 0, Dbg, "IrpContext = %08lx\n", IrpContext);
    DebugTrace( 0, Dbg, "Irp        = %08lx\n", Irp);
    DebugTrace( 0, Dbg, " ->Length               = %08lx\n", IrpSp->Parameters.QueryDirectory.Length);
    DebugTrace( 0, Dbg, " ->FileName             = %08lx\n", IrpSp->Parameters.QueryDirectory.FileName);
    DebugTrace( 0, Dbg, " ->FileInformationClass = %08lx\n", IrpSp->Parameters.QueryDirectory.FileInformationClass);
    DebugTrace( 0, Dbg, " ->FileIndex            = %08lx\n", IrpSp->Parameters.QueryDirectory.FileIndex);
    DebugTrace( 0, Dbg, " ->SystemBuffer         = %08lx\n", Irp->AssociatedIrp.SystemBuffer);
    DebugTrace( 0, Dbg, " ->RestartScan          = %08lx\n", FlagOn(IrpSp->Flags, SL_RESTART_SCAN));
    DebugTrace( 0, Dbg, " ->ReturnSingleEntry    = %08lx\n", FlagOn(IrpSp->Flags, SL_RETURN_SINGLE_ENTRY));
    DebugTrace( 0, Dbg, " ->IndexSpecified       = %08lx\n", FlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED));
    DebugTrace( 0, Dbg, "Vcb        = %08lx\n", Vcb);
    DebugTrace( 0, Dbg, "Scb        = %08lx\n", Scb);
    DebugTrace( 0, Dbg, "Ccb        = %08lx\n", Ccb);

    //
    //  Because we probably need to do the I/O anyway we'll reject any request
    //  right now that cannot wait for I/O.  We do not want to abort after
    //  processing a few index entries.
    //

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

        DebugTrace(0, Dbg, "Automatically enqueue Irp to Fsp\n", 0);

        Status = NtfsPostRequest( IrpContext, Irp );

        DebugTrace(-1, Dbg, "NtfsQueryDirectory -> %08lx\n", Status);
        return Status;
    }

    //
    //  Reference our input parameters to make things easier
    //

    UserBufferLength = IrpSp->Parameters.QueryDirectory.Length;

    FileInformationClass = IrpSp->Parameters.QueryDirectory.FileInformationClass;
    FileIndex = IrpSp->Parameters.QueryDirectory.FileIndex;

    //
    //  Look in the Ccb to see the type of search.
    //

    IgnoreCase = BooleanFlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE );

    RestartScan       = BooleanFlagOn(IrpSp->Flags, SL_RESTART_SCAN);
    ReturnSingleEntry = BooleanFlagOn(IrpSp->Flags, SL_RETURN_SINGLE_ENTRY);
    IndexSpecified    = BooleanFlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED);

    //
    //  We have to create a File Name string for querying if there is either
    //  one specified in this request, or we do not already have a value
    //  in the Ccb.
    //

    if ((IrpSp->Parameters.QueryDirectory.FileName != NULL)

            ||

        (Ccb->QueryBuffer == NULL)) {

        //
        //  Now, if the input string is NULL, we have to create the default
        //  string "*".
        //

        if (IrpSp->Parameters.QueryDirectory.FileName == NULL) {

            FileNameLength = SizeOfFileName + sizeof(WCHAR);
            FileNameBuffer = FsRtlAllocatePool( NonPagedPool, FileNameLength );

            //
            //  Initialize it.
            //

            FileNameBuffer->ParentDirectory = Scb->Fcb->FileReference;
            FileNameBuffer->FileNameLength = 1;
            FileNameBuffer->Flags = 0;
            FileNameBuffer->FileName[0] = '*';

        //
        //  We know we have an input file name, and we may or may not already
        //  have one in the Ccb.  Allocate space for it, initialize it, and
        //  set up to deallocate on the way out if we already have a pattern
        //  in the Ccb.
        //

        } else {

            UniFileName = (PUNICODE_STRING) IrpSp->Parameters.QueryDirectory.FileName;

            if (!NtfsIsFileNameValid(IrpContext, UniFileName, TRUE)) {

                if (Ccb->QueryBuffer == NULL
                    || UniFileName->Length > 4
                    || UniFileName->Length == 0
                    || UniFileName->Buffer[0] != L'.'
                    || (UniFileName->Length == 4
                        && UniFileName->Buffer[1] != L'.')) {

                    Status = STATUS_OBJECT_NAME_INVALID;

                    DebugTrace(-1, Dbg, "NtfsQueryDirectory -> %08lx\n", Status);

                    NtfsCompleteRequest( &IrpContext, &Irp, Status );

                    return Status;
                }
            }

            FileNameLength = (USHORT)IrpSp->Parameters.QueryDirectory.FileName->Length;

            FileNameBuffer = (PFILE_NAME)FsRtlAllocatePool( NonPagedPool,
                                                            SizeOfFileName +
                                                              FileNameLength );

            RtlCopyMemory( FileNameBuffer->FileName,
                           UniFileName->Buffer,
                           FileNameLength );

            FileNameLength += SizeOfFileName;

            FileNameBuffer->ParentDirectory = Scb->Fcb->FileReference;
            FileNameBuffer->FileNameLength = (UCHAR)((FileNameLength - SizeOfFileName) / 2);
            FileNameBuffer->Flags = 0;
        }

        //
        //  If we already have a query buffer, deallocate this on the way
        //  out.
        //

        if (Ccb->QueryBuffer != NULL) {

            //
            //  If we have a name to resume from then override the restart
            //  scan boolean.
            //

            if ((UnwindFileNameBuffer = FileNameBuffer) != NULL) {

                RestartScan = FALSE;
            }

        //
        //  Otherwise, store this one in the Ccb.
        //

        } else {

            UNICODE_STRING Expression;

            Ccb->QueryBuffer = (PVOID)FileNameBuffer;
            Ccb->QueryLength = FileNameLength;
            FirstQuery = TRUE;

            //
            //  If the search expression contains a wild card then remember this in
            //  the Ccb.
            //

            Expression.MaximumLength =
            Expression.Length = FileNameBuffer->FileNameLength * sizeof( WCHAR );
            Expression.Buffer = FileNameBuffer->FileName;

            //
            //  When we establish the search pattern, we must also establish
            //  whether the user wants to see "." and "..".  This code does
            //  not necessarily have to be perfect (he said), but should be
            //  good enough to catch the common cases.  Dos does not have
            //  perfect semantics for these cases, and the following determination
            //  will mimic what FastFat does exactly.
            //

            if (Scb != Vcb->RootIndexScb) {

                WCHAR Dot = L'.';
                UNICODE_STRING DotString = {2, 2, &Dot};

                if (FsRtlDoesNameContainWildCards(&Expression)) {

                    if (FsRtlIsNameInExpression( &Expression,
                                                 &DotString,
                                                 FALSE,
                                                 NULL )) {


                        SetFlag( Ccb->Flags, CCB_FLAG_RETURN_DOT | CCB_FLAG_RETURN_DOTDOT );
                    }
                } else {

                    if (FsRtlAreNamesEqual( &Expression,
                                            &DotString,
                                            FALSE,
                                            NULL )) {

                        SetFlag( Ccb->Flags, CCB_FLAG_RETURN_DOT | CCB_FLAG_RETURN_DOTDOT );
                    }
                }
            }
        }

    //
    //  Otherwise we are just restarting the query from the Ccb.
    //

    } else {

        FileNameBuffer = (PFILE_NAME)Ccb->QueryBuffer;
        FileNameLength = Ccb->QueryLength;
    }

    Irp->IoStatus.Information = 0;

    NtfsInitializeIndexContext( IrpContext, &OtherContext );

    try {

        ULONG BaseLength;
        ULONG BytesToCopy;

        //
        //  Acquire shared access to the Scb.
        //

        NtfsAcquireSharedScb( IrpContext, Scb );
        ScbAcquired = TRUE;

        //
        // If we are in the Fsp now because we had to wait earlier,
        // we must map the user buffer, otherwise we can use the
        // user's buffer directly.
        //

        Buffer = NtfsMapUserBuffer( IrpContext, Irp );

        //
        //  Check if this is the first call to query directory for this file
        //  object.  It is the first call if the enumeration context field of
        //  the ccb is null.  Also check if we are to restart the scan.
        //

        if (FirstQuery || RestartScan) {

            CallRestart = TRUE;
            NextFlag = FALSE;

            //
            //  On first/restarted scan, note that we have not returned either
            //  of these guys.
            //

            ClearFlag( Ccb->Flags, CCB_FLAG_DOT_RETURNED | CCB_FLAG_DOTDOT_RETURNED );

        //
        //  Otherwise check to see if we were given a file name to restart from
        //

        } else if (UnwindFileNameBuffer != NULL) {

            CallRestart = TRUE;
            NextFlag = TRUE;

            //
            //  The guy could actually be asking to return to one of the dot
            //  file positions, so we must handle that correctly.
            //

            if ((FileNameBuffer->FileNameLength <= 2) &&
                (FileNameBuffer->FileName[0] == L'.')) {

                if (FileNameBuffer->FileNameLength == 1) {

                    //
                    //  He wants to resume after ".", so we set to return
                    //  ".." again, and change the temporary pattern to
                    //  rewind our context to the front.
                    //

                    ClearFlag( Ccb->Flags, CCB_FLAG_DOTDOT_RETURNED );
                    SetFlag( Ccb->Flags, CCB_FLAG_DOT_RETURNED );

                    FileNameBuffer->FileName[0] = L'*';
                    NextFlag = FALSE;

                } else if (FileNameBuffer->FileName[1] == L'.') {

                    //
                    //  He wants to resume after "..", so we the change
                    //  the temporary pattern to rewind our context to the
                    //  front.
                    //

                    SetFlag( Ccb->Flags, CCB_FLAG_DOT_RETURNED | CCB_FLAG_DOTDOT_RETURNED );
                    FileNameBuffer->FileName[0] =
                    FileNameBuffer->FileName[1] = L'*';
                    NextFlag = FALSE;
                }

            //
            //  Always return the entry after the user's file name.
            //

            } else {

                SetFlag( Ccb->Flags, CCB_FLAG_DOT_RETURNED | CCB_FLAG_DOTDOT_RETURNED );
            }

        //
        //  Otherwise we're simply continuing a previous enumeration from
        //  where we last left off.  And we always leave off one beyond the
        //  last entry we returned.
        //

        } else {

            CallRestart = FALSE;
            NextFlag = FALSE;
        }

        //
        //  At this point we are about to enter our query loop.  We have
        //  already decided if we need to call restart or continue when we
        //  go after an index entry.  The variables LastEntry and NextEntry are
        //  used to index into the user buffer.  LastEntry is the last entry
        //  we added to the user buffer, and NextEntry is the current
        //  one we're working on.
        //

        LastEntry = 0;
        NextEntry = 0;

        //
        //  Determine the size of the constant part of the structure.
        //

        switch (FileInformationClass) {

        case FileDirectoryInformation:

            BaseLength = FIELD_OFFSET( FILE_DIRECTORY_INFORMATION,
                                       FileName[0] );
            break;

        case FileFullDirectoryInformation:

            BaseLength = FIELD_OFFSET( FILE_FULL_DIR_INFORMATION,
                                       FileName[0] );
            break;

        case FileNamesInformation:

            BaseLength = FIELD_OFFSET( FILE_NAMES_INFORMATION,
                                       FileName[0] );
            break;

        case FileBothDirectoryInformation:

            BaseLength = FIELD_OFFSET( FILE_BOTH_DIR_INFORMATION,
                                       FileName[0] );
            break;

        default:

            try_return( Status = STATUS_INVALID_INFO_CLASS );
        }

        while (TRUE) {

            PINDEX_ENTRY IndexEntry;
            UNICODE_STRING TempName;
            PFILE_NAME NameInIndex;
            PDUPLICATED_INFORMATION DupInfo;
            PINDEX_ENTRY DosIndexEntry;

            ULONG BytesRemainingInBuffer;
            ULONG FoundFileNameLength;

            struct {

                FILE_NAME FileName;
                WCHAR LastChar;
            } DotDotName;

            DebugTrace(0, Dbg, "Top of Loop\n", 0);
            DebugTrace(0, Dbg, "LastEntry = %08lx\n", LastEntry);
            DebugTrace(0, Dbg, "NextEntry = %08lx\n", NextEntry);

            DosIndexEntry = NULL;

            //
            //  Lookup the next index entry.  Check if we need to do the lookup
            //  by calling restart or continue.  If we do need to call restart
            //  check to see if we have a real AnsiFileName.  And set ourselves
            //  up for subsequent iternations through the loop
            //

            if (CallRestart) {

                GotEntry = NtfsRestartIndexEnumeration( IrpContext,
                                                        Ccb,
                                                        Scb,
                                                        (PVOID)FileNameBuffer,
                                                        FileNameLength,
                                                        IgnoreCase,
                                                        NextFlag,
                                                        &IndexEntry );

                CallRestart = FALSE;

            } else {

                GotEntry = NtfsContinueIndexEnumeration( IrpContext,
                                                         Ccb,
                                                         Scb,
                                                         NextFlag,
                                                         &IndexEntry );

            }

            //
            //  Check to see if we should quit the loop because we are only
            //  returning a single entry.  We actually want to spin around
            //  the loop top twice so that our enumeration has has us left off
            //  at the last entry we didn't return.  We know this is now our
            //  second time though the loop if NextEntry is not zero.
            //

            if ((ReturnSingleEntry) && (NextEntry != 0)) {

                break;
            }

            //
            //  Assume we are to return one of the names "." or "..".
            //  We should not search farther in the index so we set
            //  NextFlag to FALSE.
            //

            RtlZeroMemory( &DotDotName, sizeof(DotDotName) );
            NameInIndex = &DotDotName.FileName;
            NameInIndex->Flags = FILE_NAME_NTFS | FILE_NAME_DOS;
            NameInIndex->FileName[0] =
            NameInIndex->FileName[1] = L'.';
            DupInfo = &Scb->Fcb->Info;
            NextFlag = FALSE;

            //
            //  Handle "." first.
            //

            if (!FlagOn(Ccb->Flags, CCB_FLAG_DOT_RETURNED) &&
                FlagOn(Ccb->Flags, CCB_FLAG_RETURN_DOT)) {

                FoundFileNameLength = 2;
                GotEntry = TRUE;
                SetFlag( Ccb->Flags, CCB_FLAG_DOT_RETURNED );

            //
            //  Handle ".." next.
            //

            } else if (!FlagOn(Ccb->Flags, CCB_FLAG_DOTDOT_RETURNED) &&
                       FlagOn(Ccb->Flags, CCB_FLAG_RETURN_DOTDOT)) {

                FoundFileNameLength = 4;
                GotEntry = TRUE;
                SetFlag( Ccb->Flags, CCB_FLAG_DOTDOT_RETURNED );

            } else {

                //
                //  Compute the length of the name we found.
                //

                if (GotEntry) {

                    FoundFileNameLength = IndexEntry->AttributeLength - SizeOfFileName;

                    NameInIndex = (PFILE_NAME)(IndexEntry + 1);
                    DupInfo = &NameInIndex->Info;
                    NextFlag = TRUE;
                }
            }

            //
            //  Now check to see if we actually got another index entry.  If
            //  we didn't then we also need to check if we never got any
            //  or if we just ran out.  If we just ran out then we break out
            //  of the main loop and finish the Irp after the loop
            //

            if (!GotEntry) {

                DebugTrace(0, Dbg, "GotEntry is FALSE\n", 0);

                if (NextEntry == 0) {

                    if (FirstQuery) {

                        try_return( Status = STATUS_NO_SUCH_FILE );
                    }

                    try_return( Status = STATUS_NO_MORE_FILES );
                }

                break;
            }

            //
            //  Cleanup and reinitialize context from previous loop.
            //

            NtfsReinitializeIndexContext( IrpContext, &OtherContext );

            //
            //  We may have matched a Dos-Only name.  If so we will save
            //  it and go get the Ntfs name.
            //

            if (!FlagOn(NameInIndex->Flags, FILE_NAME_NTFS) &&
                FlagOn(NameInIndex->Flags, FILE_NAME_DOS)) {

                DosIndexEntry = IndexEntry;

                IndexEntry = NtfsRetrieveOtherIndexEntry ( IrpContext,
                                                           Ccb,
                                                           Scb,
                                                           DosIndexEntry,
                                                           &OtherContext );

                //
                //  If we got an Ntfs name, then we need to list this entry now
                //  iff the Ntfs name is not in the expression.  If the Ntfs
                //  name is in the expression, we can just continue and print
                //  this name when we encounter it by the Ntfs name.
                //

                if (IndexEntry != NULL) {

                    if (FlagOn(Ccb->Flags, CCB_FLAG_WILDCARD_IN_EXPRESSION)) {


                        if ((*NtfsIsInExpression[COLLATION_FILE_NAME])
                                                 ( IrpContext,
                                                   (PVOID)Ccb->QueryBuffer,
                                                   Ccb->QueryLength,
                                                   IndexEntry,
                                                   IgnoreCase )) {

                            continue;
                        }

                    } else {

                        if ((*NtfsIsEqual[COLLATION_FILE_NAME])
                                          ( IrpContext,
                                            (PVOID)Ccb->QueryBuffer,
                                            Ccb->QueryLength,
                                            IndexEntry,
                                            IgnoreCase )) {

                            continue;
                        }
                    }

                    FoundFileNameLength = IndexEntry->AttributeLength - SizeOfFileName;

                    NameInIndex = (PFILE_NAME)(IndexEntry + 1);
                    DupInfo = &NameInIndex->Info;

                } else {

                    continue;
                }
            }

            //
            //  Here are the rules concerning filling up the buffer:
            //
            //  1.  The Io system garentees that there will always be
            //      enough room for at least one base record.
            //
            //  2.  If the full first record (including file name) cannot
            //      fit, as much of the name as possible is copied and
            //      STATUS_BUFFER_OVERFLOW is returned.
            //
            //  3.  If a subsequent record cannot completely fit into the
            //      buffer, none of it (as in 0 bytes) is copied, and
            //      STATUS_SUCCESS is returned.  A subsequent query will
            //      pick up with this record.
            //

            BytesRemainingInBuffer = UserBufferLength - NextEntry;

            if ( (NextEntry != 0) &&
                 ( (BaseLength + FoundFileNameLength > BytesRemainingInBuffer) ||
                   (UserBufferLength < NextEntry) ) ) {

                DebugTrace(0, Dbg, "Next entry won't fit\n", 0);

                try_return( Status = STATUS_SUCCESS );
            }

            ASSERT( BytesRemainingInBuffer >= BaseLength );

            //
            //  Zero the base part of the structure.
            //

            RtlZeroMemory( &Buffer[NextEntry], BaseLength );

            //
            //  Now we have an entry to return to our caller. we'll
            //  case on the type of information requested and fill up the
            //  user buffer if everything fits
            //

            switch (FileInformationClass) {

            case FileBothDirectoryInformation:

                BothDirInfo = (PFILE_BOTH_DIR_INFORMATION)&Buffer[NextEntry];
                TempName.Buffer = NameInIndex->FileName;
                TempName.MaximumLength =
                TempName.Length = (USHORT)FoundFileNameLength;

                //
                //  If this is not also a Dos name, and the Ntfs flag is set
                //  (meaning there is a separate Dos name), then call the
                //  routine to get the short name.
                //

                if (!FlagOn(NameInIndex->Flags, FILE_NAME_DOS) &&
                    FlagOn(NameInIndex->Flags, FILE_NAME_NTFS) &&
                    !NtfsIsDosNameInCurrentCodePage(&TempName)) {

                    PFILE_NAME DosFileName;

                    if (DosIndexEntry == NULL) {

                        DosIndexEntry = NtfsRetrieveOtherIndexEntry ( IrpContext,
                                                                      Ccb,
                                                                      Scb,
                                                                      IndexEntry,
                                                                      &OtherContext );
                    }

                    if (DosIndexEntry != NULL) {

                        DosFileName = (PFILE_NAME)(DosIndexEntry + 1);
                        BothDirInfo->ShortNameLength = DosFileName->FileNameLength * 2;
                        RtlCopyMemory( BothDirInfo->ShortName,
                                       DosFileName->FileName,
                                       BothDirInfo->ShortNameLength );
                    }
                }

            case FileFullDirectoryInformation:

                DebugTrace(0, Dbg, "Getting file full Unicode directory information\n", 0);

                FullDirInfo = (PFILE_FULL_DIR_INFORMATION)&Buffer[NextEntry];

                FullDirInfo->EaSize = DupInfo->PackedEaSize;

                //
                //  Add 4 bytes for the CbListHeader.
                //

                if (DupInfo->PackedEaSize != 0) {

                    FullDirInfo->EaSize += 4;
                }

            case FileDirectoryInformation:

                DebugTrace(0, Dbg, "Getting file Unicode directory information\n", 0);

                DirInfo = (PFILE_DIRECTORY_INFORMATION)&Buffer[NextEntry];

                DirInfo->CreationTime.QuadPart = DupInfo->CreationTime;
                DirInfo->LastAccessTime.QuadPart = DupInfo->LastAccessTime;
                DirInfo->LastWriteTime.QuadPart = DupInfo->LastModificationTime;
                DirInfo->ChangeTime.QuadPart = DupInfo->LastChangeTime;

                DirInfo->FileAttributes = DupInfo->FileAttributes & FILE_ATTRIBUTE_VALID_FLAGS;

                if (FlagOn( DupInfo->FileAttributes, DUP_FILE_NAME_INDEX_PRESENT)) {
                    DirInfo->FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
                }
                if (DirInfo->FileAttributes == 0) {
                    DirInfo->FileAttributes = FILE_ATTRIBUTE_NORMAL;
                }

                DirInfo->FileNameLength = FoundFileNameLength;

                DirInfo->EndOfFile.QuadPart = DupInfo->FileSize;
                DirInfo->AllocationSize.QuadPart = DupInfo->AllocatedLength;

                break;

            case FileNamesInformation:

                DebugTrace(0, Dbg, "Getting file Unicode names information\n", 0);

                NamesInfo = (PFILE_NAMES_INFORMATION)&Buffer[NextEntry];

                NamesInfo->FileNameLength = FoundFileNameLength;

                break;

            default:

                try_return( Status = STATUS_INVALID_INFO_CLASS );
            }

            //
            //  Compute how many bytes we can copy.  This should only be less
            //  than the file name length if we are only returning a single
            //  entry.
            //

            if (BytesRemainingInBuffer >= BaseLength + FoundFileNameLength) {

                BytesToCopy = FoundFileNameLength;

            } else {

                BytesToCopy = BytesRemainingInBuffer - BaseLength;

                Status = STATUS_BUFFER_OVERFLOW;
            }

            RtlCopyMemory( &Buffer[NextEntry + BaseLength],
                           NameInIndex->FileName,
                           BytesToCopy );

            //
            //  Set up the previous next entry offset
            //

            *((PULONG)(&Buffer[LastEntry])) = NextEntry - LastEntry;

            //
            //  And indicate how much of the user buffer we have currently
            //  used up.  We must compute this value before we long align
            //  ourselves for the next entry
            //

            Irp->IoStatus.Information += BaseLength + BytesToCopy;

            //
            //  If we weren't able to copy the whole name, then we bail here.
            //

            if ( !NT_SUCCESS( Status ) ) {

                try_return( Status );
            }

            //
            //  Set ourselves up for the next iteration
            //

            LastEntry = NextEntry;
            NextEntry += (ULONG)QuadAlign( BaseLength + BytesToCopy );
        }

        //
        //  At this point we've successfully filled up some of the buffer so
        //  now is the time to set our status to success.
        //

        Status = STATUS_SUCCESS;

    try_exit:

        //
        //  Abort transaction on error by raising.
        //

        NtfsCleanupTransaction( IrpContext, Status );

        //
        //  Set the last access flag in the Fcb if the caller
        //  didn't set it explicitly.
        //

        if (!FlagOn( Ccb->Flags, CCB_FLAG_USER_SET_LAST_ACCESS_TIME )) {

            NtfsGetCurrentTime( IrpContext, Scb->Fcb->CurrentLastAccess );
        }

    } finally {

        DebugUnwind( NtfsQueryDirectory );

        if (ScbAcquired) {
            NtfsReleaseScb( IrpContext, Scb );
        }

        NtfsCleanupAfterEnumeration( IrpContext, Ccb );
        NtfsCleanupIndexContext( IrpContext, &OtherContext );

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        if (UnwindFileNameBuffer != NULL) {

            ExFreePool(UnwindFileNameBuffer);
        }
    }

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NtfsQueryDirectory -> %08lx\n", Status);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsNotifyChangeDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the notify change directory operation.  It is
    responsible for either completing of enqueuing the input Irp.

Arguments:

    Irp - Supplies the Irp to process

    Vcb - Supplies its Vcb

    Scb - Supplies its Scb

    Ccb - Supplies its Ccb

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    ULONG CompletionFilter;
    BOOLEAN WatchTree;

    PSECURITY_SUBJECT_CONTEXT SubjectContext;
    BOOLEAN FreeSubjectContext = FALSE;

    PCHECK_FOR_TRAVERSE_ACCESS CallBack;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );
    ASSERT_VCB( Vcb );
    ASSERT_CCB( Ccb );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    //
    //  Get the current Stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "NtfsNotifyChangeDirectory...\n", 0);
    DebugTrace( 0, Dbg, "IrpContext = %08lx\n", IrpContext);
    DebugTrace( 0, Dbg, "Irp        = %08lx\n", Irp);
    DebugTrace( 0, Dbg, " ->CompletionFilter = %08lx\n", IrpSp->Parameters.NotifyDirectory.CompletionFilter);
    DebugTrace( 0, Dbg, " ->WatchTree        = %08lx\n", FlagOn( IrpSp->Flags, SL_WATCH_TREE ));
    DebugTrace( 0, Dbg, "Vcb        = %08lx\n", Vcb);
    DebugTrace( 0, Dbg, "Ccb        = %08lx\n", Ccb);
    DebugTrace( 0, Dbg, "Scb        = %08lx\n", Scb);

    //
    //  Reference our input parameter to make things easier
    //

    CompletionFilter = IrpSp->Parameters.NotifyDirectory.CompletionFilter;
    WatchTree        = BooleanFlagOn( IrpSp->Flags, SL_WATCH_TREE );

    //
    //  Always set the wait bit in the IrpContext so the initial wait can't fail.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

    //
    //  We will only acquire the Vcb to perform the dirnotify task.  The dirnotify
    //  package will provide synchronization between this operation and cleanup.
    //  We need the Vcb to synchronize with any rename or link operations underway.
    //

    NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );

    try {

        //
        //  If we need to verify traverse access for this caller then allocate and
        //  capture the subject context to pass to the dir notify package.  That
        //  package will be responsible for deallocating it.
        //

        if (FlagOn( Ccb->Flags, CCB_FLAG_TRAVERSE_CHECK )) {

            SubjectContext = FsRtlAllocatePool( PagedPool,
                                                sizeof( SECURITY_SUBJECT_CONTEXT ));

            FreeSubjectContext = TRUE;
            SeCaptureSubjectContext( SubjectContext );

            FreeSubjectContext = FALSE;
            CallBack = NtfsNotifyTraverseCheck;

        } else {

            SubjectContext = NULL;
            CallBack = NULL;
        }

        //
        //  Call the Fsrtl package to process the request.  We cast the
        //  unicode strings to ansi strings as the dir notify package
        //  only deals with memory matching.
        //

        FsRtlNotifyFullChangeDirectory( Vcb->NotifySync,
                                        &Vcb->DirNotifyList,
                                        Ccb,
                                        (PSTRING) &Scb->ScbType.Index.NormalizedName,
                                        WatchTree,
                                        FALSE,
                                        CompletionFilter,
                                        Irp,
                                        CallBack,
                                        SubjectContext );

        Status = STATUS_PENDING;

    } finally {

        DebugUnwind( NtfsNotifyChangeDirectory );

        NtfsReleaseVcb( IrpContext, Vcb, NULL );

        //
        //  Since the dir notify package is holding the Irp, we discard the
        //  the IrpContext.
        //

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, NULL, 0 );

        } else if (FreeSubjectContext) {

            ExFreePool( SubjectContext );
        }
    }

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NtfsNotifyChangeDirectory -> %08lx\n", Status);

    return Status;
}
