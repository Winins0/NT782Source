/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    dlcdrvr.c

Abstract:

    This module contains code which implements the NT DLC driver API
    using the generic data link module.

    Contents:
        DriverEntry
        CreateAdapterFileContext
        CleanupAdapterFileContext
        DlcDriverUnload
        CloseAdapterFileContext
        DlcDeviceIoControl
        DlcCompleteIoRequest

Author:

    Antti Saarenheimo 8-Jul-1991

Environment:

    Kernel mode

Revision History:


--*/

#define INCLUDE_IO_BUFFER_SIZE_TABLE        // includes the io buffer sizes

#include <dlc.h>
#include "dlcreg.h"

#define DEFINE_DLC_DIAGNOSTICS
#include "dlcdebug.h"

/*++

The queueing of asynchronous commands (21 Oct. 1991)
----------------------------------------------------

DLC driver uses basically three different methods to queue and
complete its asyncronous commands:

1. Using LLC request handle

In this case the LLC driver takes care of the command queueing.
This method is used by:
- all transmit commands
- all close commands
- dlc connect
- dlc disconnect

2. DLC (READ / RECEIVE) Command queue (FIFO)

The read and receive commands are saved to the command completion
queue, that is is circular single enty link list.
The events are handled by the oldest pending command.
These commands also check the event queue and the command
is queued only if there are no pending events.

3. Timer queue

Timer queue is a null terminated single entry link list.
They are sorted by the relative expiration time.  The timer
tick completes all timer commands having the same expiration
time.  The expiration times are relative to all previous
commands.  For example, timer commands having expiration times 1, 2 and 3
would all have 1 tick count in the queue.  Thus the timer tick
needs to increment only one tick from the beginning of the list
and complete all timer commands having zero expiration time.
When command is cancelled, its tick count must be added
to the next element in the queue.

--*/


/*++

New stuff in Feb-20-1992


(reason: with a free build (because it is so fast) I run out of
 non-paged pool and several problems did arise in DLC driver:
 commands were sometimes lost, DLC.RESET did never complete and
 the adapter close


The recovery rules when no non-paged memory is available:

1. The adapter close (or file context close by handle) must work always =>
   the adapter close packet must be in the file context.

2. The commands must allocate all resources needed to complete the
   command before they return a pending status (transmit!), if that
   cannot be done then the command must fail immediately.

3. The received data must not be acknowledged before all resources
   have been allocated to receive it.

4. Probablem: we may lose an important link state change indication if we
   cannot allocate a packet for it.  For example, the application may
   not know, that the link is in the local busy state.  The dlc status events
   of quite many link stations (255) may also very soon eat all non-paged
   pool, if they are not read by client.  A static event packet would
   prevent that.
   A solution: a static event indication packet
   in the dlc object, connect and disconnect confirmation would be handled
   as now (they are indicated in the command completion entry).
   The indication status would be reset, when it is read from the command
   queue.  The same event indication may have several flags.


(Do first 1, 2, 3 and 4, make the test even more stressing for non-paged
 pool and test then what happens.  Then we may fix the bug in the buffer
 pool shrinkin and implement dynamic packet pools).

The long term solutions:

Dynamic memory management in dlc!  The current dlc memory management
is very fast and the top memory consumption is minimal (without the
default 33% overhead of binary buddy algorithm), but it never release
the resources, that it has once allocated.


1. The packet pools should release the extra memory when they are not
   needed any more, implementation: each memory block allocated for
   the packet pool has a reference count, that memory block is deallocated
   when the reference count is zero.  This cleanup could be done once in
   a second.  The algorithm scans the free list of packets, removes the
   packet from the free list, if the reference count of free packets
   is the same as the total packet count on a memory block.
   The memory blocks can be relesed in the next loop while the block itself
   is disconnected from the single entry list of all memory blocks in
   the packet pool.

2. The buffer pool memory management should also be able to shrink the
   number of locked pages (there must be a bug in the current implementation)
   AND also to free all MDLs and extra packets, when the buffer pool pages
   are unlocked.


3. Data link driver should not allocated any memory resources (except
   packet pools to send its own frames).  The objects should be created
   by in the dlc driver => all extra resources are released when
   a dlc driver is released (actually not a big deal, because dynamic
   packet pool management fixes the problem with the link stations).

--*/

//  Local IOCTL dispatcher table:
// ***************************************************
//  THE ORDER OF THESE FUNCTIONS MUST BE THE SAME AS
//  THE IOCTL COMMAND CODES IN NTDDDLC.H
// ***************************************************

static PFDLC_COMMAND_HANDLER DispatchTable[IOCTL_DLC_LAST_COMMAND] = {
    DlcReadRequest,
    DlcReceiveRequest,
    DlcTransmit,
    DlcBufferFree,
    DlcBufferGet,
    DlcBufferCreate,
    DirSetExceptionFlags,
    DlcCloseStation,
    DlcConnectStation,
    DlcFlowControl,
    DlcOpenLinkStation,
    DlcReset,
    DlcReadCancel,
    DlcReceiveCancel,
    DlcQueryInformation,
    DlcSetInformation,
    DirTimerCancel,
    DirTimerCancelGroup,
    DirTimerSet,
    DlcOpenSap,
    DlcCloseStation,
    DirOpenDirect,
    DlcCloseStation,
    DirOpenAdapter,
    DirCloseAdapter,
    DlcReallocate,
    DlcReadRequest,
    DlcReceiveRequest,
    DlcTransmit,
    DlcCompleteCommand
};

USHORT aSpecialOutputBuffers[3] = {
    sizeof(LLC_READ_OUTPUT_PARMS),
    sizeof(PVOID),                     // pFirstBuffer
    sizeof(UCHAR)                      // TransmitFrameStatus
};

NDIS_SPIN_LOCK DlcLock;

#ifdef LOCK_CHECK

LONG DlcLockLevel = 0;

ULONG __line = 0;
PCHAR __file = NULL;
LONG __last = 1;
HANDLE __process = (HANDLE)0;
HANDLE __thread = (HANDLE)0;

#endif

#if LLC_DBG

extern PVOID pAdapters;

ULONG AllocatedNonPagedPool = 0;
ULONG LockedPageCount = 0;
ULONG AllocatedMdlCount = 0;
ULONG AllocatedPackets = 0;
ULONG cExAllocatePoolFailed = 0;
ULONG FailedMemoryLockings = 0;
NDIS_SPIN_LOCK MemCheckLock;

ULONG cFramesReceived = 0;
ULONG cFramesIndicated = 0;
ULONG cFramesReleased = 0;

ULONG cLockedXmitBuffers = 0;
ULONG cUnlockedXmitBuffers = 0;

//UINT InputIndex = 0;
//LLC_SM_TRACE aLast[LLC_INPUT_TABLE_SIZE];

//UINT LlcTraceIndex = 0;
//UCHAR LlcTraceTable[LLC_TRACE_TABLE_SIZE];

#endif


//
// global data
//

BOOLEAN MemoryLockFailed = FALSE;   // this limits unneceasary memory locks
KSPIN_LOCK DlcSpinLock;             // syncnhronize the final cleanup
PDEVICE_OBJECT ThisDeviceContext;   // required for unloading driver

#if DBG

BOOLEAN Prolix;
MEMORY_USAGE DriverMemoryUsage;
MEMORY_USAGE DriverStringUsage; // how much string does it take to hang a DLC driver?

//
// in the debug version we maintain a singly-linked list of FILE_CONTEXT
// structures to aid in figuring out where memory went. Required because
// in some cases, we lose the pointer information from ADAPTER or BINDING
// contexts
//

SINGLE_LIST_ENTRY FileContexts = {NULL};
KSPIN_LOCK FileContextsLock;
KIRQL PreviousIrql;

VOID
LinkFileContext(
    IN PDLC_FILE_CONTEXT pFileContext
    );

VOID
UnlinkFileContext(
    IN PDLC_FILE_CONTEXT pFileContext
    );

#endif

//
// external data
//

extern POBJECT_TYPE *IoFileObjectType;


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT pDriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This function is called when the I/O subsystem loads the DLC driver

    This routine performs the initialization of NT DLC API driver.
    Eventually this should be called after the first reference to
    DLC driver.

Arguments:

    pDriverObject   - Pointer to driver object created by the system
    RegistryPath    - The name of DLC's node in the registry

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    NTSTATUS Status;
    PDEVICE_OBJECT pDeviceObject;
    UNICODE_STRING DriverName;

    ASSUME_IRQL(PASSIVE_LEVEL);

#if DBG
    if (Prolix) {
        DbgPrint("DLC.DriverEntry\n");
    }
    KeInitializeSpinLock(&DriverMemoryUsage.SpinLock);
    KeInitializeSpinLock(&DriverStringUsage.SpinLock);
    KeInitializeSpinLock(&FileContextsLock);
    InitializeMemoryPackage();
#endif

    //
    // load any initialization-time parameters from the registry
    //

    DlcRegistryInitialization(RegistryPath);
    LoadDlcConfiguration();

    //
    // LLC init makes ourselves known to the NDIS wrapper,
    // but we don't yet bind to any NDIS driver (don't know even the name)
    //

    Status = LlcInitialize();
    if (Status != STATUS_SUCCESS) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Create the DLC device object. For now, we simply create \Device\Dlc
    // using a Unicode string. In the future we may need to load an ACL
    //

    RtlInitUnicodeString(&DriverName, DD_DLC_DEVICE_NAME);

    //
    // Create the device object for DLC driver, we don't have any
    // device specific data, because DLC needs only one device context.
    // Thus it can just use statics and globals.
    //

    Status = IoCreateDevice(pDriverObject,
                            0,
                            &DriverName,
                            FILE_DEVICE_DLC,
                            0,
                            FALSE,
                            &pDeviceObject
                            );
    if (!NT_SUCCESS(Status)) {
        return Status;
    } else {

        //
        // need to keep a pointer to device context for IoDeleteDevice
        //

        ThisDeviceContext = pDeviceObject;
    }

    //
    // DLC driver never calls other device drivers: 1 I/O stack in IRP is enough
    //

    pDeviceObject->StackSize = 1;
    pDeviceObject->Flags |= DO_DIRECT_IO;

    KeInitializeSpinLock(&DlcSpinLock);

    NdisAllocateSpinLock(&DlcLock);

    //
    // Initialize the driver object with this driver's entry points.
    //

    pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateAdapterFileContext;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CloseAdapterFileContext;
    pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = CleanupAdapterFileContext;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DlcDeviceIoControl;
    pDriverObject->DriverUnload = DlcDriverUnload;

    return STATUS_SUCCESS;
}


NTSTATUS
CreateAdapterFileContext(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    )

/*++

Routine Description:

    This function is called when a handle to the DLC driver is opened (via
    NtCreateFile)

    The Create function creates file context for a DLC application.
    A DLC application needs at least one file context for each
    network adapter it is using.  The DLC file contexts may share
    the same buffer pool, but otherwise they are totally isolated
    from each other.

Arguments:

    DeviceObject    - Pointer to the device object for this driver
    Irp             - Pointer to the request packet representing the I/O request

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PDLC_FILE_CONTEXT pFileContext;
    PIO_STACK_LOCATION pIrpSp;

    UNREFERENCED_PARAMETER(pDeviceObject);

    ASSUME_IRQL(PASSIVE_LEVEL);

#if LLC_DBG == 2
    PrintMemStatus();
#endif

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pFileContext = (PDLC_FILE_CONTEXT)ALLOCATE_ZEROMEMORY_DRIVER(sizeof(DLC_FILE_CONTEXT));
    if (pFileContext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorExit2;
    }

#if DBG

    //
    // add this file context to our global list of opened file contexts
    //

    LinkFileContext(pFileContext);

    //
    // record who owns this memory usage and add it to our global list of
    // memory usages
    //

    pFileContext->MemoryUsage.Owner = (PVOID)pFileContext;
    pFileContext->MemoryUsage.OwnerObjectId = FileContextObject;
    LinkMemoryUsage(&pFileContext->MemoryUsage);
#endif

    pIrpSp->FileObject->FsContext = pFileContext;
    pFileContext->FileObject = pIrpSp->FileObject;

    InitializeListHead(&pFileContext->EventQueue);
    InitializeListHead(&pFileContext->CommandQueue);
    InitializeListHead(&pFileContext->ReceiveQueue);
    InitializeListHead(&pFileContext->FlowControlQueue);

    //
    // create pool of command/event packets
    //

    pFileContext->hPacketPool = CREATE_PACKET_POOL_FILE(DlcPacketPoolObject,
                                                        sizeof(DLC_PACKET),
                                                        8
                                                        );
    if (pFileContext->hPacketPool == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorExit1;
    }

    //
    // create pool of DLC-level SAP/LINK/DIRECT objects
    //

    pFileContext->hLinkStationPool = CREATE_PACKET_POOL_FILE(DlcLinkPoolObject,
                                                             sizeof(DLC_OBJECT),
                                                             4
                                                             );
    if (pFileContext->hLinkStationPool == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorExit1;
    }

    //
    // we have done everything required to provide the caller with an open file
    // handle to a DLC device instance. Increment the reference count of the
    // I/O subsystem's file object. This prevents the file object from being
    // deleted until we have performed the corresponding close processing
    //

    //
    // RLF 03/22/93
    //
    // We should NOT be messing with the reference counters on the file object
    // from within a driver. However, removing this may break DLC! Corresponding
    // dereference code has been removed from DereferenceFileContext[ByTwo]
    //

//
//    Status = ObReferenceObjectByPointer(pIrpSp->FileObject,
//                                        FILE_ALL_ACCESS,
//                                        *IoFileObjectType,
//                                        KernelMode
//                                        );

    //
    // similarly, we increment the reference count in our file context
    //

    ReferenceFileContext(pFileContext);

    //
    // the call to open a handle to the driver may have succeeded, but we don't
    // yet have an open adapter context
    //

    pFileContext->State = DLC_FILE_CONTEXT_CLOSED;
    ALLOCATE_SPIN_LOCK(&pFileContext->SpinLock);

ErrorExit1:

    if (Status != STATUS_SUCCESS) {
        DELETE_PACKET_POOL_FILE(&pFileContext->hLinkStationPool);
        DELETE_PACKET_POOL_FILE(&pFileContext->hPacketPool);
        CHECK_MEMORY_RETURNED_FILE();

#if DBG
        UnlinkFileContext(pFileContext);
        UnlinkMemoryUsage(&pFileContext->MemoryUsage);
#endif

        FREE_MEMORY_DRIVER(pFileContext);
    }

ErrorExit2:

    pIrp->IoStatus.Status = Status;
    DlcCompleteIoRequest(pIrp, FALSE);
    return Status;
}


NTSTATUS
CleanupAdapterFileContext(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    )

/*++

Routine Description:

    This function is called when the last reference to an open file handle is
    removed. This is an opportunity, given us by the I/O subsystem, to ensure
    that all pending I/O requests for the file object being closed have been
    completed

    The routine checks, that the file context is really closed.
    Otherwise it executes a panic closing of all resources in
    the same way as in the DirCloseAdapter call.
    It happens when an application makes process exit without
    calling the DirCloseAdapter.

Arguments:

    DeviceObject    - Pointer to the device object for this driver
    Irp             - Pointer to the request packet representing the I/O request

Return Value:

    The function value is the status of the operation.

--*/

{
    PIO_STACK_LOCATION pIrpSp;
    PDLC_FILE_CONTEXT pFileContext;
    NTSTATUS Status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(pDeviceObject);

    DIAG_FUNCTION("CleanupAdapterFileContext");

#if DBG
    if (Prolix) {
        DbgPrint("CleanupAdapterFileContext\n");
    }
#endif

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pFileContext = pIrpSp->FileObject->FsContext;

    //
    // We may have a pending close or Initialize operation going on
    //

    ACQUIRE_DRIVER_LOCK();

    ENTER_DLC(pFileContext);

    if (pFileContext->State == DLC_FILE_CONTEXT_OPEN) {
        IoMarkIrpPending(pIrp);
        ReferenceFileContextByTwo(pFileContext);

#if DBG
        DbgPrint("IRP_MJ_CLEANUP: Panic Close!!!\n");
#endif

        Status = DirCloseAdapter(pIrp,
                                 pFileContext,
                                 NULL,
                                 0,
                                 0
                                 );

#if LLC_DBG
        if (Status != STATUS_PENDING) {
            DbgBreakPoint();
        }
#endif

        //
        // We always return a pending status from DirCloseAdapter
        //

        MY_ASSERT(Status == STATUS_PENDING);

        DereferenceFileContext(pFileContext);

        LEAVE_DLC(pFileContext);

        RELEASE_DRIVER_LOCK();

    } else {

        LEAVE_DLC(pFileContext);

        RELEASE_DRIVER_LOCK();

        DlcCompleteIoRequest(pIrp, FALSE);
    }

    return Status;
}


VOID
DlcDriverUnload(
    IN PDRIVER_OBJECT pDeviceObject
    )

/*++

Routine Description:

    This functions is called when a called is made to the I/O subsystem to
    remove the DLC driver

Arguments:

    DeviceObject - Pointer to the device object for this driver.

Return Value:

    The function value is the status of the operation.

--*/

{
    UNREFERENCED_PARAMETER(pDeviceObject);

    ASSUME_IRQL(PASSIVE_LEVEL);

    LlcTerminate();
    DlcRegistryTermination();

    CHECK_MEMORY_RETURNED_DRIVER();
    CHECK_STRING_RETURNED_DRIVER();
    CHECK_DRIVER_MEMORY_USAGE(TRUE);

    NdisFreeSpinLock(&DlcLock);

    //
    // now tell I/O subsystem that this device context is no longer current
    //

    IoDeleteDevice(ThisDeviceContext);
}


NTSTATUS
CloseAdapterFileContext(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    )

/*++

Routine Description:

    This routine is called when the file object reference count is zero. The file
    object is really being deleted by the I/O subsystem. The file context had
    better be closed by now (should have been cleared out by Cleanup)

Arguments:

    DeviceObject    - Pointer to the device object for this driver
    Irp             - Pointer to the request packet representing the I/O request

Return Value:

    The function value is the status of the operation.

--*/

{
    PIO_STACK_LOCATION pIrpSp;
    PDLC_FILE_CONTEXT pFileContext;
    KIRQL irql;
    PVOID pBindingContext;

    UNREFERENCED_PARAMETER(pDeviceObject);

    DIAG_FUNCTION("CloseAdapterFileContext");

#if DBG
    if (Prolix) {
        DbgPrint("CloseAdapterFileContext\n");
    }
#endif

    //
    // It seems to be possible, that this routine is called twice!
    // We can have only one guy closing the file context.
    //

    ACQUIRE_DRIVER_LOCK();

    ACQUIRE_DLC_LOCK(irql);

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pFileContext = pIrpSp->FileObject->FsContext;
    if (pFileContext != NULL) {
        pIrpSp->FileObject->FsContext = NULL;

        RELEASE_DLC_LOCK(irql);

        //
        // Free all memory ever allocated for this file context.
        // Spinlock checks, that nobody is still accessing to the file context!!!
        // FileObject may be dereferenced asynchronously =>
        // File context reference count hits zero and io-system
        // calls this routine, the spin lock prevent us to delete file
        // context when it is still used by somebody.
        //

        ENTER_DLC(pFileContext);

        DELETE_PACKET_POOL_FILE(&pFileContext->hPacketPool);
        DELETE_PACKET_POOL_FILE(&pFileContext->hLinkStationPool);

        LEAVE_DLC(pFileContext);

        DEALLOCATE_SPIN_LOCK(&pFileContext->SpinLock);

        pBindingContext = pFileContext->pBindingContext;

        //
        // Finally, close the NDIS adapter. we have already disabled all
        // indications from it
        //

        if (pBindingContext) {

            //
            // RLF 04/26/94
            //
            // We need to call LlcDisableAdapter here to terminate the DLC timer
            // if it is not already terminated. Else we can end up with the timer
            // still in the adapter's tick list (if there are other bindings to
            // the adapter), and sooner or later that will cause an access
            // violation, followed very shortly thereafter by a blue screen
            //

            LlcDisableAdapter(pBindingContext);
            LlcCloseAdapter(pBindingContext, TRUE);
        }

        CHECK_MEMORY_RETURNED_FILE();

#if DBG
        UnlinkFileContext(pFileContext);
        UnlinkMemoryUsage(&pFileContext->MemoryUsage);
#endif

        FREE_MEMORY_DRIVER(pFileContext);

#if LLC_DBG
        if ((LockedPageCount != 0
        || AllocatedMdlCount != 0
        || AllocatedNonPagedPool != 0)
        && pAdapters == NULL) {
            DbgPrint("DLC.CloseAdapterFileContext: Error: Resources not released\n");
            //PrintMemStatus();
            DbgBreakPoint();
        }
        FailedMemoryLockings = 0;
#endif

    } else {

        RELEASE_DLC_LOCK(irql);

    }

    RELEASE_DRIVER_LOCK();

    DlcCompleteIoRequest(pIrp, FALSE);

#if DBG
    if (Prolix) {
        CHECK_DRIVER_MEMORY_USAGE(FALSE);
    }
#endif

    return STATUS_SUCCESS;
}


NTSTATUS
DlcDeviceIoControl(
    IN PDEVICE_OBJECT pDeviceContext,
    IN PIRP pIrp
    )

/*++

Routine Description:

    This routine dispatches DLC requests to different handlers based
    on the minor IOCTL function code in the IRP's current stack location.
    In addition to cracking the minor function code, this routine also
    reaches into the IRP and passes the packetized parameters stored there
    as parameters to the various DLC request handlers so that they are
    not IRP-dependent.

    DlcDeviceControl and LlcReceiveIndication are the most time critical
    procedures in DLC.  This code has been optimized for the asynchronous
    command (read and transmit)

Arguments:

    pDeviceContext  - Pointer to the device object for this driver (unused)
    pIrp            - Pointer to the request packet representing the I/O request

Return Value:

    NTSTATUS
        Success - STATUS_SUCCESS
                    The I/O request has been successfully completed

                  STATUS_PENDING
                    The I/O request has been submitted and will be completed
                    asynchronously

        Failure - DLC_STATUS_XXX
                  LLC_STATUS_XXX
                    The I/O request has been completed, but an error occurred

--*/

{
    USHORT TmpIndex;
    PDLC_FILE_CONTEXT pFileContext; // FsContext in FILE_OBJECT.
    PIO_STACK_LOCATION pIrpSp;
    ULONG ioControlCode;

    UNREFERENCED_PARAMETER(pDeviceContext);

    ASSUME_IRQL(PASSIVE_LEVEL);

    //
    // Make sure status information is consistent every time
    //

    pIrp->IoStatus.Status = STATUS_SUCCESS;

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    //
    // Branch to the appropriate request handler, but do first
    // preliminary checking of the input and output buffers,
    // the size of the request block is performed here so that it is known
    // in the handlers that the minimum input parameters are readable.  It
    // is *not* determined here whether variable length input fields are
    // passed correctly; this is a check which must be made within each routine.
    //

    ioControlCode = pIrpSp->Parameters.DeviceIoControl.IoControlCode;
    TmpIndex = (((USHORT)ioControlCode) >> 2) & 0x0fff;
    if (TmpIndex >= IOCTL_DLC_LAST_COMMAND) {
        pIrp->IoStatus.Information = 0;
        DlcCompleteIoRequest(pIrp, FALSE);
        return DLC_STATUS_INVALID_COMMAND;
    }

    if (pIrpSp->Parameters.DeviceIoControl.InputBufferLength
                            < (ULONG)aDlcIoBuffers[TmpIndex].InputBufferSize
        ||

        pIrpSp->Parameters.DeviceIoControl.OutputBufferLength
                            < (ULONG)aDlcIoBuffers[TmpIndex].OutputBufferSize) {

        //
        // This error code should never be returned to user
        // If this happpens, then there is something wrong with ACSLAN
        //

        pIrp->IoStatus.Information = 0;
        DlcCompleteIoRequest(pIrp, FALSE);
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Save the length of the actual output buffer to Information field.
    // This number of bytes will be copied back to user buffer.
    //

    pIrp->IoStatus.Information = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    //
    // there are 3 cases of asynchronous commands where we need to lock extra
    // user memory for returned information. This goes in the parameter table
    // which can be anywhere in user memory (ie not near the CCB):
    //
    //  TRANSMIT
    //      - TRANSMIT_FS - a single byte!
    //
    //  RECEIVE
    //      - FIRST_BUFFER - a DWORD - pointer to the first received frame
    //
    //  READ
    //      - the entire parameter table needs to be locked. Virtually all
    //        the fields are output. Still, this is only a max of 30 bytes
    //

    if (TmpIndex <= IOCTL_DLC_TRANSMIT_INDEX) {

        PVOID pDestination;
        PNT_DLC_PARMS pDlcParms;

        pDlcParms = (PNT_DLC_PARMS)pIrp->AssociatedIrp.SystemBuffer;

        //
        // Get the pointer of output parameters in user memory.
        // Note that we are not accessing anything in user address space.
        //

        switch (TmpIndex) {
        case IOCTL_DLC_READ_INDEX:
            pDestination = &pDlcParms->Async.Ccb.u.pParameterTable->Read.uchEvent;
            break;

        case IOCTL_DLC_RECEIVE_INDEX:
            pDestination = &pDlcParms->Async.Ccb.u.pParameterTable->Receive.pFirstBuffer;
            break;

        case IOCTL_DLC_TRANSMIT_INDEX:
            pDestination = &pDlcParms->Async.Ccb.u.pParameterTable->Transmit.uchTransmitFs;
            break;
        }

        //
        // allocate another MDL for the 1, 4, or 30 byte parameter table and lock
        // the page(s!)
        //

        pDlcParms->Async.Ccb.u.pMdl = AllocateProbeAndLockMdl(
                                            pDestination,
                                            aSpecialOutputBuffers[TmpIndex]
                                            );
        if (pDlcParms->Async.Ccb.u.pMdl == NULL) {
            DlcCompleteIoRequest(pIrp, FALSE);
            return DLC_STATUS_MEMORY_LOCK_FAILED;
        }
    }

    pFileContext = (PDLC_FILE_CONTEXT)pIrpSp->FileObject->FsContext;

    ACQUIRE_DRIVER_LOCK();

    ENTER_DLC(pFileContext);

    //
    // We must leave immediately, if the reference counter is zero
    // or if we have a pending close or Initialize operation going on.
    // (This is not 100% safe, if app would create a file context,
    // open adapter, close adapter and immediately would close it again
    // when the previous command is pending, but that cannot be happen
    // with dlcapi.dll)
    //

    if ((pFileContext->ReferenceCount == 0)
    || ((pFileContext->State != DLC_FILE_CONTEXT_OPEN)
    && (TmpIndex != IOCTL_DLC_OPEN_ADAPTER_INDEX))) {

        LEAVE_DLC(pFileContext);

        RELEASE_DRIVER_LOCK();

        DlcCompleteIoRequest(pIrp, FALSE);
        return LLC_STATUS_ADAPTER_CLOSED;

    } else {

        NTSTATUS Status;

        DLC_TRACE('F');

        //
        // set the default IRP cancel routine. We are not going to handle
        // transmit cases now
        //

        //SetIrpCancelRoutine(pIrp,
        //                    (BOOLEAN)
        //                    !( (ioControlCode == IOCTL_DLC_TRANSMIT)
        //                    || (ioControlCode == IOCTL_DLC_TRANSMIT2) )
        //                    );

        //
        // and set the irp I/O status to pending
        //

        IoMarkIrpPending(pIrp);

        //
        // We use the file context reference counter to prevent the IO system
        // from deleting the file context due to a Close request arriving
        // while we are still in this function.
        // ReferenceObjectByPointer / DereferenceObject are called only when
        // the file context reference count is zero
        //

        ReferenceFileContextByTwo(pFileContext);

        //
        // Irp and IrpSp are used just as in NBF
        //

        Status = DispatchTable[TmpIndex](
                    pIrp,
                    pFileContext,
                    (PNT_DLC_PARMS)pIrp->AssociatedIrp.SystemBuffer,
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength,
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength
                    );

        //
        // ensure the function returned with the correct IRQL
        //

        ASSUME_IRQL(DISPATCH_LEVEL);

        //
        // the following error codes are valid:
        //
        //  STATUS_PENDING
        //      The request has been accepted
        //      The driver will complete the request asynchronously
        //      The output CCB should contain 0xFF in its status field (unless
        //      already completed)
        //
        //  STATUS_SUCCESS
        //      The request has successfully completed synchronously
        //      The output CCB should contain 0x00 in its status field
        //
        //  0x6001 - 0x6069
        //  0x6080 - 0x6081
        //  0x60A1 - 0x60A3
        //  0x60C0 - 0x60CB
        //  0x60FF
        //      The request has failed with a DLC-specific error
        //      The error code is converted to a DLC status code (-0x6000) and
        //      the output CCB status field is set to the DLC status code
        //      No asynchronous completion will be taken for this request
        //

        if (Status != STATUS_PENDING) {

            DLC_TRACE('G');

            pIrpSp->Control &= ~SL_PENDING_RETURNED;

            if (Status != STATUS_SUCCESS) {

                PNT_DLC_PARMS pDlcParms = (PNT_DLC_PARMS)pIrp->AssociatedIrp.SystemBuffer;

                if (Status >= DLC_STATUS_ERROR_BASE && Status < DLC_STATUS_MAX_ERROR) {
                    Status -= DLC_STATUS_ERROR_BASE;
                }

                //
                // RLF 04/20/94
                //
                // make sure the CCB has the correct value written to it on
                // output if we're not returning pending status
                //

                pDlcParms->Async.Ccb.uchDlcStatus = (UCHAR)Status;

                //
                // the CCB request has failed. Make sure the pNext field is reset
                //

                if ((pIrpSp->Parameters.DeviceIoControl.IoControlCode & 3) == METHOD_OUT_DIRECT) {

                    //
                    // the CCB address may actually be unaligned DOS CCB1
                    //

                    LLC_CCB UNALIGNED * pCcb;

                    pCcb = MmGetSystemAddressForMdl(pIrp->MdlAddress);
                    pCcb->pNext = NULL;
                } else {
                    pDlcParms->Async.Ccb.pCcbAddress = NULL;
                }
            }

            if (ioControlCode != IOCTL_DLC_RESET) {
                DereferenceFileContextByTwo(pFileContext);
            } else {
                DereferenceFileContext(pFileContext);
            }

            LEAVE_DLC(pFileContext);

            RELEASE_DRIVER_LOCK();

            //
            // RLF 06/07/93
            //
            // if the request is DLC.RESET, the IRP will have already been
            // completed if we're here, so don't complete it again (else we'll
            // bugcheck)
            //

            if (ioControlCode != IOCTL_DLC_RESET) {
                DlcCompleteIoRequest(pIrp, FALSE);
            }

            return Status;

        } else {

            DLC_TRACE('H');

            //
            // Reallocate the buffer pool size, if a threshold has been exceeded
            //

            if (BufferPoolCheckThresholds(pFileContext->hBufferPool)) {
                ReferenceBufferPool(pFileContext);

                LEAVE_DLC(pFileContext);

                BufferPoolExpand((PDLC_BUFFER_POOL)pFileContext->hBufferPool);

                ENTER_DLC(pFileContext);

                DereferenceBufferPool(pFileContext);
            }
            DereferenceFileContext(pFileContext);

            LEAVE_DLC(pFileContext);

            RELEASE_DRIVER_LOCK();

            return STATUS_PENDING;
        }
    }
}


VOID
DlcCompleteIoRequest(
    IN PIRP pIrp,
    IN BOOLEAN InCancel
    )

/*++

Routine Description:

    This routine completes the given DLC IRP

Arguments:

    pIrp        - Pointer to the request packet representing the I/O request.
    InCancel    - TRUE if called on Irp cancel path

Return Value:

    None

--*/

{
    //
    // we are about to complete this IRP - remove the cancel routine. The check
    // stops us spinning forever if this function is called from within an IRP
    // cancellation
    //

    if (!InCancel) {
        SetIrpCancelRoutine(pIrp, FALSE);
    }

    //
    // unlock and free any MDLs we allocated
    //

    if (IoGetCurrentIrpStackLocation(pIrp)->MajorFunction == IRP_MJ_DEVICE_CONTROL
    && IoGetCurrentIrpStackLocation(pIrp)->Parameters.DeviceIoControl.IoControlCode <= IOCTL_DLC_TRANSMIT) {

        //
        // We enter here only if something has gone wrong in the main
        // function of an async operation => the status field and
        // next pointer will be updated synchronously.
        // On the other hand, all other async functions having no output
        // parameters except CCB status and next pointer are upated
        // by the normal code path.  They should just copy
        // back the pending status and next pointer pointing to CCB itself.
        // That should not affect anything, because the DLL will update
        // those fields, when we return synchronous status
        //

        PNT_DLC_PARMS pDlcParms = (PNT_DLC_PARMS)pIrp->AssociatedIrp.SystemBuffer;

        if (pDlcParms->Async.Ccb.u.pMdl != NULL) {
            UnlockAndFreeMdl(pDlcParms->Async.Ccb.u.pMdl);
        }
    }
    IoCompleteRequest(pIrp, (CCHAR)IO_NETWORK_INCREMENT);
}

#if DBG

VOID
LinkFileContext(
    IN PDLC_FILE_CONTEXT pFileContext
    )
{
    KeAcquireSpinLock(&FileContextsLock, &PreviousIrql);
    PushEntryList(&FileContexts, &pFileContext->List);
    KeReleaseSpinLock(&FileContextsLock, PreviousIrql);
}

VOID
UnlinkFileContext(
    IN PDLC_FILE_CONTEXT pFileContext
    )
{
    PSINGLE_LIST_ENTRY p, prev = (PSINGLE_LIST_ENTRY)&FileContexts;

    KeAcquireSpinLock(&FileContextsLock, &PreviousIrql);
    for (p = FileContexts.Next; p && p != (PSINGLE_LIST_ENTRY)pFileContext; ) {
        prev = p;
        p = p->Next;
    }
    if (p) {
        prev->Next = p->Next;
    } else {
        DbgPrint("DLC.UnlinkFileContext: Error: FILE_CONTEXT @%08X not on list??\n",
                pFileContext
                );
    }
    KeReleaseSpinLock(&FileContextsLock, PreviousIrql);
}

#endif
