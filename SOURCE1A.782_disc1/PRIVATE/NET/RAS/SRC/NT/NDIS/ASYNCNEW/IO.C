/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    io.c

Abstract:


Author:

    Thomas J. Dimitri  (TommyD) 08-May-1992

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/
#include "asyncall.h"

// asyncmac.c will define the global parameters.
#include "globals.h"


NTSTATUS
AsyncSetupIrp(
	IN PASYNC_FRAME Frame)

/*++

	This is the routine which intializes the Irp

--*/
{
    //    PMDL 				mdl;
	PIRP			irp=Frame->Irp;
	PDEVICE_OBJECT	deviceObject=Frame->Info->DeviceObject;
	PFILE_OBJECT	fileObject=Frame->Info->FileObject;

	//
    // Set the file object to the Not-Signaled state.
    //  I DON'T THINK THIS NEEDS TO BE DONE.  WHO'S WAITING ON
	//  ON THIS FILEOBJECT??
    (VOID) KeResetEvent(&fileObject->Event);

	irp->Tail.Overlay.OriginalFileObject = fileObject;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.AuxiliaryBuffer = NULL;
    irp->RequestorMode = KernelMode;
	irp->PendingReturned = FALSE;

    //
    // Fill in the service independent parameters in the IRP.
    //

    irp->UserEvent = NULL;
    irp->UserIosb = &Frame->IoStatusBlock;
    irp->Overlay.AsynchronousParameters.UserApcRoutine = NULL;
    irp->Overlay.AsynchronousParameters.UserApcContext = NULL;

    //
    // Now determine whether this device expects to have data buffered to it
    // or whether it performs direct I/O.  This is based on the DO_BUFFERED_IO
    // flag in the device object.  If the flag is set, then a system buffer is
    // allocated and the caller's data is copied into it.  Otherwise, a Memory
    // Descriptor List (MDL) is allocated and the caller's buffer is locked
    // down using it.
    //

    if (deviceObject->Flags & DO_BUFFERED_IO) {

        //
        // The device does not support direct I/O.  Allocate a system buffer,
        // and copy the caller's data into it.  This is done using an
        // exception handler that will perform cleanup if the operation
        // fails.  Note that this is only done if the operation has a non-zero
        // length.
        //

        irp->AssociatedIrp.SystemBuffer = Frame->Frame;

		//
        // Set the IRP_BUFFERED_IO flag in the IRP so that I/O completion
        // will know that this is not a direct I/O operation.
		//

        irp->Flags = IRP_BUFFERED_IO;


    } else if (deviceObject->Flags & DO_DIRECT_IO) {

        //
        // This is a direct I/O operation.  Allocate an MDL and invoke the
        // memory management routine to lock the buffer into memory.  This
        // is done using an exception handler that will perform cleanup if
        // the operation fails.  Note that no MDL is allocated, nor is any
        // memory probed or locked if the length of the request was zero.
        //

//        mdl = (PMDL) NULL;
//
//        try {
//
//                //
//                // Allocate an MDL, charging quota for it, and hang it off of
//                // the IRP.  Probe and lock the pages associated with the
//                // caller's buffer for read access and fill in the MDL with
//                // the PFNs of those pages.
//                //
//
//                mdl = IoAllocateMdl(
//						Frame->Frame,
//						Frame->FrameLength,
//						FALSE,
//						TRUE,
//						irp);
//
//                if (mdl == NULL) {
//                    ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
//                }
//
//                MmProbeAndLockPages(mdl, requestorMode, (LOCK_OPERATION)IoReadAccess);
//
//		} except(EXCEPTION_EXECUTE_HANDLER) {

            //
            // An exception was incurred while either allocating the MDL
            // or while attempting to probe and lock the caller's buffer.
            // Determine what actually happened, clean everything up, and
            // return an appropriate error status code.
            //

//            IopExceptionCleanup(
//				fileObject,
//				irp,
//				EventObject,
//				(PKEVENT)NULL);
//
//            return GetExceptionCode();
//        }

		DbgPrintf(("The DeviceObject is NOT BUFFERED_IO!! IRP FAILURE!!\n"));
		DbgBreakPoint();

    } else {

        //
        // Pass the address of the caller's buffer to the device driver.  It
        // is now up to the driver to do everything.
        //

        irp->UserBuffer = Frame->Frame;

    }

	// For now, if we get this far, it means success!
	return(STATUS_SUCCESS);
}
