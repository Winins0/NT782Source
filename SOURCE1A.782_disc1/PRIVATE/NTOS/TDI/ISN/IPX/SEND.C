/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    send.c

Abstract:

    This module contains code that implements the send engine for the
    IPX transport provider.

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop


#if 0
#define IpxGetMdlChainLength(Mdl, Length) { \
    PMDL _Mdl = (Mdl); \
    *(Length) = 0; \
    while (_Mdl) { \
        *(Length) += MmGetMdlByteCount(_Mdl); \
        _Mdl = _Mdl->Next; \
    } \
}
#endif

VOID
IpxSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by the I/O system to indicate that a connection-
    oriented packet has been shipped and is no longer needed by the Physical
    Provider.

Arguments:

    ProtocolBindingContext - The ADAPTER structure for this binding.

    NdisPacket/RequestHandle - A pointer to the NDIS_PACKET that we sent.

    NdisStatus - the completion status of the send.

Return Value:

    none.

--*/

{

    PIPX_SEND_RESERVED Reserved = (PIPX_SEND_RESERVED)(NdisPacket->ProtocolReserved);
    PADAPTER Adapter = (PADAPTER)ProtocolBindingContext;
    PREQUEST Request;
    PADDRESS_FILE AddressFile;
    PDEVICE Device = IpxDevice;
    PBINDING Binding;
    USHORT NewId, OldId;
    ULONG NewOffset, OldOffset;
    PIPX_HEADER IpxHeader;
    IPX_LOCAL_TARGET LocalTarget;
    PIO_STACK_LOCATION irpSp;

#if DBG
    if (Adapter != NULL) {
        ASSERT_ADAPTER(Adapter);
    }
#endif

    //
    // See if this send was padded.
    //

    if (Reserved->PaddingBuffer) {
        CTEAssert (NDIS_BUFFER_LINKAGE(Reserved->PaddingBuffer->NdisBuffer) == NULL);
        IPX_PUSH_ENTRY_LIST(
            &Device->PaddingBufferList,
            &Reserved->PaddingBuffer->PoolLinkage,
            &Device->Lock);
        Reserved->PaddingBuffer = NULL;
        NDIS_BUFFER_LINKAGE (Reserved->PreviousTail) = (PNDIS_BUFFER)NULL;
        if (Reserved->Identifier < IDENTIFIER_IPX) {
            NdisRecalculatePacketCounts (NdisPacket);
        }
    }

FunctionStart:;

    switch (Reserved->Identifier) {

    case IDENTIFIER_IPX:

#if DBG
        CTEAssert (Reserved->SendInProgress);
        Reserved->SendInProgress = FALSE;
#endif

        //
        // Check if this packet should be sent to all
        // networks.
        //

        if (Reserved->u.SR_DG.CurrentNicId) {

            if (NdisStatus == NDIS_STATUS_SUCCESS) {
                Reserved->u.SR_DG.Net0SendSucceeded = TRUE;
            }

            OldId = Reserved->u.SR_DG.CurrentNicId;

            for (NewId = OldId+1; NewId <= Device->HighestExternalNicId; NewId++) {
                if ((Binding = Device->Bindings[NewId])
                                &&
                    ((!Device->SingleNetworkActive) ||
                     (Device->ActiveNetworkWan == Binding->Adapter->MacInfo.MediumAsync))
                                &&
                    ((!Device->DisableDialoutSap) ||
                     (!Binding->DialOutAsync) ||
                     (!Reserved->u.SR_DG.OutgoingSap))) {

                    //
                    // The binding exists, and we either are not configured
                    // for "SingleNetworkActive", or we are and this binding
                    // is the right type (i.e. the active network is wan and
                    // this is a wan binding, or the active network is not
                    // wan and this is not a wan binding), and this is not
                    // an outgoing sap that we are trying to send with
                    // "DisableDialoutSap" set.
                    //

                    break;
                }
            }

            if (NewId <= Device->HighestExternalNicId) {

                //
                // Yes, we found another net to send it on, so
                // move the header around if needed and do so.
                //

                Reserved->u.SR_DG.CurrentNicId = NewId;
                CTEAssert ((Reserved->DestinationType == DESTINATION_BCAST) ||
                           (Reserved->DestinationType == DESTINATION_MCAST));

                NewOffset = Binding->BcMcHeaderSize;
                OldOffset = Device->Bindings[OldId]->BcMcHeaderSize;

                if (OldOffset != NewOffset) {

                    RtlMoveMemory(
                        &Reserved->Header[NewOffset],
                        &Reserved->Header[OldOffset],
                        sizeof(IPX_HEADER));

                }

                IpxHeader = (PIPX_HEADER)(&Reserved->Header[NewOffset]);

                LocalTarget.NicId = NewId;
                RtlCopyMemory(LocalTarget.MacAddress, IpxHeader->DestinationNode, 6);

                if (Device->MultiCardZeroVirtual ||
                    (IpxHeader->DestinationSocket == SAP_SOCKET)) {

                    //
                    // SAP frames need to look like they come from the
                    // local network, not the virtual one. The same is
                    // true if we are running multiple nets without
                    // a virtual net.
                    //

                    *(UNALIGNED ULONG *)IpxHeader->SourceNetwork = Binding->LocalAddress.NetworkAddress;
                    RtlCopyMemory (IpxHeader->SourceNode, Binding->LocalAddress.NodeAddress, 6);
                }

                //
                // Fill in the MAC header and submit the frame to NDIS.
                //

#if DBG
                CTEAssert (!Reserved->SendInProgress);
                Reserved->SendInProgress = TRUE;
#endif

                if ((NdisStatus = IpxSendFrame(
                        &LocalTarget,
                        NdisPacket,
                        REQUEST_INFORMATION(Reserved->u.SR_DG.Request) + sizeof(IPX_HEADER),
                        sizeof(IPX_HEADER))) != NDIS_STATUS_PENDING) {

                    Adapter = Binding->Adapter;
                    goto FunctionStart;
                }

                return;

            } else {

                //
                // If any of the sends succeeded then return
                // success on the datagram send, otherwise
                // use the most recent failure status.
                //

                if (Reserved->u.SR_DG.Net0SendSucceeded) {
                    NdisStatus = NDIS_STATUS_SUCCESS;
                }

            }

        }


#if 0
        //
        // NOTE: We don't NULL out the linkage field of the
        // HeaderBuffer, which will leave the old buffer chain
        // hanging off it; but that is OK because if we reuse
        // this packet we will replace that chain with the new
        // one, and before we free it we NULL it out.
        //
        // I.e. we don't do this:
        //

        NDIS_BUFFER_LINKAGE (Reserved->HeaderBuffer) = NULL;
        NdisRecalculatePacketCounts (NdisPacket);
#endif

#if 0
        {
            ULONG ActualLength;
            IpxGetMdlChainLength(NDIS_BUFFER_LINKAGE(Reserved->HeaderBuffer), &ActualLength);
            if (ActualLength != REQUEST_INFORMATION(Reserved->u.SR_DG.Request)) {
                DbgPrint ("IPX: At completion, IRP %lx has parameter length %d, buffer chain length %d\n",
                        Reserved->u.SR_DG.Request, REQUEST_INFORMATION(Reserved->u.SR_DG.Request), ActualLength);
                DbgBreakPoint();
            }
        }
#endif

        //
        // Save these so we can free the packet.
        //

        Request = Reserved->u.SR_DG.Request;
        AddressFile = Reserved->u.SR_DG.AddressFile;

        if (Reserved->OwnedByAddress) {

            Reserved->Address->SendPacketInUse = FALSE;

        } else {

            IPX_PUSH_ENTRY_LIST(
                &Device->SendPacketList,
                &Reserved->PoolLinkage,
                &Device->Lock);
        }

        ++Device->Statistics.PacketsSent;

        //
        // If this is a fast send irp, we bypass the file system and
        // call the completion routine directly.
        //

        REQUEST_STATUS(Request) = NdisStatus;
        irpSp = IoGetCurrentIrpStackLocation( Request );

        if ( irpSp->MinorFunction == TDI_DIRECT_SEND_DATAGRAM ) {

            Request->CurrentLocation++,
            Request->Tail.Overlay.CurrentStackLocation++;

            (VOID) irpSp->CompletionRoutine(
                                        NULL,
                                        Request,
                                        irpSp->Context
                                        );

        } else {
            IpxCompleteRequest (Request);
        }

        IpxFreeRequest(Device, Request);

        IpxDereferenceAddressFileSync (AddressFile, AFREF_SEND_DGRAM);

        break;

    case IDENTIFIER_RIP_INTERNAL:

        CTEAssert (Reserved->SendInProgress);
        Reserved->SendInProgress = FALSE;
        break;

    case IDENTIFIER_RIP_RESPONSE:

        CTEAssert (Reserved->SendInProgress);
        Reserved->SendInProgress = FALSE;

        Reserved->Identifier = IDENTIFIER_IPX;
        IPX_PUSH_ENTRY_LIST(
            &Device->SendPacketList,
            &Reserved->PoolLinkage,
            &Device->Lock);

        IpxDereferenceDevice (Device, DREF_RIP_PACKET);
        break;

    default:

        (*Device->UpperDrivers[Reserved->Identifier].SendCompleteHandler)(
            NdisPacket,
            NdisStatus);
        break;

    }

}   /* IpxSendComplete */

NTSTATUS
IpxTdiSendDatagram(
    IN PDEVICE_OBJECT DeviceObject,
    IN PREQUEST Request
    )

/*++

Routine Description:

    This routine performs the TdiSendDatagram request for the transport
    provider.

Arguments:

    Request - Pointer to the request.

Return Value:

    NTSTATUS - status of operation.

--*/

{

    PADDRESS_FILE AddressFile;
    PADDRESS Address;
    PNDIS_PACKET Packet;
    PIPX_SEND_RESERVED Reserved;
    PSINGLE_LIST_ENTRY s;
    TDI_ADDRESS_IPX UNALIGNED * RemoteAddress;
    TDI_ADDRESS_IPX TempAddress;
    TA_ADDRESS UNALIGNED * AddressName;
    PTDI_CONNECTION_INFORMATION Information;
    PTDI_REQUEST_KERNEL_SENDDG Parameters;
    PBINDING Binding;
    IPX_LOCAL_TARGET TempLocalTarget;
    PIPX_LOCAL_TARGET LocalTarget;
    PDEVICE Device = IpxDevice;
    UCHAR PacketType;
    NTSTATUS Status;
    PIPX_HEADER IpxHeader;
    NDIS_STATUS NdisStatus;
    USHORT LengthIncludingHeader;
    IPX_DEFINE_SYNC_CONTEXT (SyncContext)
    IPX_DEFINE_LOCK_HANDLE (LockHandle)
    PIO_STACK_LOCATION irpSp;                                       \

    //
    // Do a quick check of the validity of the address.
    //

    AddressFile = (PADDRESS_FILE)REQUEST_OPEN_CONTEXT(Request);

    IPX_BEGIN_SYNC (&SyncContext);

    if ((AddressFile->Size == sizeof (ADDRESS_FILE)) &&
        (AddressFile->Type == IPX_ADDRESSFILE_SIGNATURE) &&
        ((Address = AddressFile->Address) != NULL)) {

        IPX_GET_LOCK (&Address->Lock, &LockHandle);

        if (AddressFile->State != ADDRESSFILE_STATE_CLOSING) {

            Parameters = (PTDI_REQUEST_KERNEL_SENDDG)REQUEST_PARAMETERS(Request);
            Information = Parameters->SendDatagramInformation;

            //
            // Do a quick check if this address has only one entry.
            //

            AddressName = &((TRANSPORT_ADDRESS UNALIGNED *)(Information->RemoteAddress))->Address[0];

            if ((AddressName->AddressType == TDI_ADDRESS_TYPE_IPX) &&
                (AddressName->AddressLength >= sizeof(TDI_ADDRESS_IPX))) {

                RemoteAddress = (TDI_ADDRESS_IPX UNALIGNED *)(AddressName->Address);

            } else if ((RemoteAddress = IpxParseTdiAddress (Information->RemoteAddress)) == NULL) {

                IPX_FREE_LOCK (&Address->Lock, LockHandle);
                Status = STATUS_INVALID_ADDRESS;
                goto error_send_no_packet;
            }

            IPX_DEBUG (SEND, ("Send on %lx, network %lx socket %lx\n",
                                   Address, RemoteAddress->NetworkAddress, RemoteAddress->Socket));

            //
            // Every address has one packet committed to it, use that
            // if possible, otherwise take one out of the pool.
            //

            if (!Address->SendPacketInUse) {

                Address->SendPacketInUse = TRUE;
                Packet = PACKET(&Address->SendPacket);
                Reserved = (PIPX_SEND_RESERVED)(Packet->ProtocolReserved);

            } else {

                s = IPX_POP_ENTRY_LIST(
                        &Device->SendPacketList,
                        &Device->Lock);

                if (s != NULL) {
                    goto GotPacket;
                }

                //
                // This function tries to allocate another packet pool.
                //

                s = IpxPopSendPacket(Device);

                //
                // Possibly we should queue the packet up to wait
                // for one to become free.
                //

                if (s == NULL) {
                    IPX_FREE_LOCK (&Address->Lock, LockHandle);
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto error_send_no_packet;
                }

GotPacket:

                Reserved = CONTAINING_RECORD (s, IPX_SEND_RESERVED, PoolLinkage);
                Packet = CONTAINING_RECORD (Reserved, NDIS_PACKET, ProtocolReserved[0]);

            }

            IpxReferenceAddressFileLock (AddressFile, AFREF_SEND_DGRAM);

            IPX_FREE_LOCK (&Address->Lock, LockHandle);

            //
            // Save this now while we have Parameters available.
            //

            REQUEST_INFORMATION(Request) = Parameters->SendLength;
            LengthIncludingHeader = (USHORT)(Parameters->SendLength + sizeof(IPX_HEADER));

#if 0
            {
                ULONG ActualLength;
                IpxGetMdlChainLength(REQUEST_NDIS_BUFFER(Request), &ActualLength);
                if (ActualLength != Parameters->SendLength) {
                    DbgPrint ("IPX: IRP %lx has parameter length %d, buffer chain length %d\n",
                            Request, Parameters->SendLength, ActualLength);
                    DbgBreakPoint();
                }
            }
#endif

            Reserved->u.SR_DG.AddressFile = AddressFile;
            Reserved->u.SR_DG.Request = Request;
            CTEAssert (Reserved->Identifier == IDENTIFIER_IPX);


            //
            // Set this to 0; this means the packet is not one that
            // should be broadcast on all nets. We will change it
            // later if it turns out this is the case.
            //

            Reserved->u.SR_DG.CurrentNicId = 0;

            //
            // We need this to track these packets specially.
            //

            Reserved->u.SR_DG.OutgoingSap = AddressFile->IsSapSocket;

            //
            // Add the MDL chain after the pre-allocated header buffer.
            // NOTE: THIS WILL ONLY WORK IF WE EVENTUALLY CALL
            // NDISRECALCULATEPACKETCOUNTS (which we do in IpxSendFrame).
            //
            //

            NDIS_BUFFER_LINKAGE (Reserved->HeaderBuffer) = REQUEST_NDIS_BUFFER(Request);

            if (Information->OptionsLength < sizeof(IPX_DATAGRAM_OPTIONS)) {

                //
                // The caller did not supply the local target for this
                // send, so we look it up ourselves.
                //

                UINT Segment;

                //
                // We calculate this now since we need to know
                // if it is directed below.
                //

                if (RemoteAddress->NodeAddress[0] == 0xff) {
                    // BUGBUG: What about multicast?
                    if ((*(UNALIGNED ULONG *)(RemoteAddress->NodeAddress) != 0xffffffff) ||
                        (*(UNALIGNED USHORT *)(RemoteAddress->NodeAddress+4) != 0xffff)) {
                        Reserved->DestinationType = DESTINATION_MCAST;
                    } else {
                        Reserved->DestinationType = DESTINATION_BCAST;
                    }
                } else {
                    Reserved->DestinationType = DESTINATION_DEF;   // directed send
                }

                //
                // If there are no options, then check if the
                // caller is passing the packet type as a final byte
                // in the remote address; if not use the default.
                //

                if (Information->OptionsLength == 0) {
                    if (AddressFile->ExtendedAddressing) {
                        PacketType = ((PUCHAR)(RemoteAddress+1))[0];
                    } else {
                        PacketType = AddressFile->DefaultPacketType;
                    }
                } else {
                    PacketType = ((PUCHAR)(Information->Options))[0];
                }

                if ((Reserved->DestinationType != DESTINATION_DEF) &&
                    ((RemoteAddress->NetworkAddress == 0) ||
                     (Device->VirtualNetwork &&
                      (RemoteAddress->NetworkAddress == Device->SourceAddress.NetworkAddress)))) {

                    //
                    // This packet needs to be broadcast to all networks.
                    // Make sure it is not too big for any of them.
                    //

                    if (Parameters->SendLength > Device->RealMaxDatagramSize) {
                        IPX_DEBUG (SEND, ("Send %d bytes too large (%d)\n",
                            Parameters->SendLength, Device->RealMaxDatagramSize));
                        Status = STATUS_INVALID_BUFFER_SIZE;
                        goto error_send_with_packet;
                    }

                    //
                    // If this is a broadcast to the virtual net, we
                    // need to construct a fake remote address which
                    // has network 0 in there instead.
                    //

                    if (Device->VirtualNetwork &&
                        (RemoteAddress->NetworkAddress == Device->SourceAddress.NetworkAddress)) {

                        RtlCopyMemory (&TempAddress, (PVOID)RemoteAddress, sizeof(TDI_ADDRESS_IPX));
                        TempAddress.NetworkAddress = 0;
                        RemoteAddress = (TDI_ADDRESS_IPX UNALIGNED *)&TempAddress;
                    }

                    //
                    // If someone is sending to the SAP socket and
                    // we are running with multiple cards without a
                    // virtual network, AND this packet is a SAP response,
                    // then we log an error to warn them that the
                    // system may not work as they like (since there
                    // is no virtual network to advertise, we use
                    // the first card's net/node as our local address).
                    // We only do this once per boot, using the
                    // SapWarningLogged variable to control that.
                    //

                    if ((RemoteAddress->Socket == SAP_SOCKET) &&
                        (!Device->SapWarningLogged) &&
                        (Device->MultiCardZeroVirtual)) {

                        PNDIS_BUFFER FirstBuffer;
                        UINT FirstBufferLength;
                        USHORT UNALIGNED * FirstBufferData;

                        if ((FirstBuffer = REQUEST_NDIS_BUFFER(Request)) != NULL) {

                            NdisQueryBuffer(
                                FirstBuffer,
                                (PVOID *)&FirstBufferData,
                                &FirstBufferLength);

                            //
                            // The first two bytes of a SAP packet are the
                            // operation, 0x2 (in network order) is response.
                            //

                            if ((FirstBufferLength >= sizeof(USHORT)) &&
                                (*FirstBufferData == 0x0200)) {

                                Device->SapWarningLogged = TRUE;

                                IpxWriteGeneralErrorLog(
                                    Device->DeviceObject,
                                    EVENT_IPX_SAP_ANNOUNCE,
                                    777,
                                    STATUS_NOT_SUPPORTED,
                                    NULL,
                                    0,
                                    NULL);
                            }
                        }
                    }


                    //
                    // In this case we do not RIP but instead set the
                    // packet up so it is sent to each network in turn.
                    //
                    // Special case: If this packet is from the SAP
                    // socket and we are running with multiple cards
                    // without a virtual network, we only send this
                    // on the card with NIC ID 1, so we leave
                    // CurrentNicId set to 0.
                    //

                    //
                    // BUGBUG: What if NicId 1 is invalid? Should scan
                    // for first valid one, fail send if none.
                    //

                    if ((Address->Socket != SAP_SOCKET) ||
                        (!Device->MultiCardZeroVirtual)) {

                        if (Device->SingleNetworkActive) {

                            if (Device->ActiveNetworkWan) {
                                Reserved->u.SR_DG.CurrentNicId = Device->FirstWanNicId;
                            } else {
                                Reserved->u.SR_DG.CurrentNicId = Device->FirstLanNicId;
                            }

                        } else {

                            Reserved->u.SR_DG.CurrentNicId = 1;

                        }

                        Reserved->u.SR_DG.Net0SendSucceeded = FALSE;

                        //
                        // In this case, we need to scan for the first
                        // non-dialout wan socket.
                        //

                        if ((Device->DisableDialoutSap) &&
                            (Address->Socket == SAP_SOCKET)) {

                            PBINDING TempBinding;
                            while (TRUE) {
                                TempBinding = Device->Bindings[Reserved->u.SR_DG.CurrentNicId];
                                if ((TempBinding != NULL) &&
                                    (!TempBinding->DialOutAsync)) {
                                    break;
                                }
                                ++Reserved->u.SR_DG.CurrentNicId;
                            }
                            if (!TempBinding) {
                                Status = STATUS_DEVICE_DOES_NOT_EXIST;
                                goto error_send_with_packet;
                            }
                        }

                        TempLocalTarget.NicId = Reserved->u.SR_DG.CurrentNicId;

                    } else {

                        TempLocalTarget.NicId = 1;
                    }

                    RtlCopyMemory(TempLocalTarget.MacAddress, RemoteAddress->NodeAddress, 6);

                } else {

                    Segment = RipGetSegment((PUCHAR)&RemoteAddress->NetworkAddress);

                    IPX_GET_LOCK (&Device->SegmentLocks[Segment], &LockHandle);

                    //
                    // This call will return STATUS_PENDING if we need to
                    // RIP for the packet.
                    //

                    Status = RipGetLocalTarget(
                                 Segment,
                                 RemoteAddress,
                                 IPX_FIND_ROUTE_RIP_IF_NEEDED,
                                 &TempLocalTarget,
                                 NULL);

                    if (Status == STATUS_SUCCESS) {

                        //
                        // We found the route, TempLocalTarget is filled in.
                        //

                        IPX_FREE_LOCK (&Device->SegmentLocks[Segment], LockHandle);

                        if (Parameters->SendLength >
                                Device->Bindings[TempLocalTarget.NicId]->RealMaxDatagramSize) {

                            IPX_DEBUG (SEND, ("Send %d bytes too large (%d)\n",
                                Parameters->SendLength,
                                Device->Bindings[TempLocalTarget.NicId]->RealMaxDatagramSize));

                            REQUEST_INFORMATION(Request) = 0;
                            Status = STATUS_INVALID_BUFFER_SIZE;
                            goto error_send_with_packet;
                        }

                        if ((Device->DisableDialoutSap) &&
                            (Address->Socket == SAP_SOCKET) &&
                            (Device->Bindings[TempLocalTarget.NicId]->DialOutAsync)) {

                            REQUEST_INFORMATION(Request) = 0;
                            Status = STATUS_DEVICE_DOES_NOT_EXIST;
                            goto error_send_with_packet;
                        }

                    } else if (Status == STATUS_PENDING) {

                        //
                        // A RIP request went out on the network; we queue
                        // this packet for transmission when the RIP
                        // response arrives. First we fill in the IPX
                        // header; the only thing we don't know is where
                        // exactly to fill it in, so we choose
                        // the most common location.
                        //

                        IpxConstructHeader(
                            &Reserved->Header[Device->IncludedHeaderOffset],
                            LengthIncludingHeader,
                            PacketType,
                            RemoteAddress,
                            &Address->LocalAddress);

                        IPX_DEBUG (RIP, ("Queueing packet %lx\n", Reserved));
                        InsertTailList(
                            &Device->Segments[Segment].WaitingForRoute,
                            &Reserved->WaitLinkage);

                        IPX_FREE_LOCK (&Device->SegmentLocks[Segment], LockHandle);
                        IPX_END_SYNC (&SyncContext);

                        return STATUS_PENDING;

                    } else {

                        IPX_FREE_LOCK (&Device->SegmentLocks[Segment], LockHandle);
                        goto error_send_with_packet;

                    }
                }

                LocalTarget = &TempLocalTarget;

                //
                // Now we know the local target, we can figure out
                // the offset for the IPX header.
                //

                Binding = Device->Bindings[LocalTarget->NicId];

                if (Reserved->DestinationType == DESTINATION_DEF) {
                    IpxHeader = (PIPX_HEADER)&Reserved->Header[Binding->DefHeaderSize];
                } else {
                    IpxHeader = (PIPX_HEADER)&Reserved->Header[Binding->BcMcHeaderSize];
                }

            } else {

                PacketType = ((PUCHAR)(Information->Options))[0];
                LocalTarget = &((PIPX_DATAGRAM_OPTIONS)(Information->Options))->LocalTarget;

                //
                // Calculate the binding and the correct location
                // for the IPX header. We can do this at the same
                // time as we calculate the DestinationType which
                // saves an if like the one 15 lines up.
                //

                Binding = Device->Bindings[LocalTarget->NicId];

#if 0
                //
                // This shouldn't be needed because even WAN bindings
                // don't go away once they are added.
                //

                if (Binding == NULL) {
                    Status = STATUS_DEVICE_DOES_NOT_EXIST;
                    goto error_send_with_packet;
                }
#endif

                if (RemoteAddress->NodeAddress[0] == 0xff) {
                    // BUGBUG: What about multicast?
                    if ((*(UNALIGNED ULONG *)(RemoteAddress->NodeAddress) != 0xffffffff) ||
                        (*(UNALIGNED USHORT *)(RemoteAddress->NodeAddress+4) != 0xffff)) {
                        Reserved->DestinationType = DESTINATION_MCAST;
                    } else {
                        Reserved->DestinationType = DESTINATION_BCAST;
                    }
                    IpxHeader = (PIPX_HEADER)&Reserved->Header[Binding->BcMcHeaderSize];
                } else {
                    Reserved->DestinationType = DESTINATION_DEF;   // directed send
                    IpxHeader = (PIPX_HEADER)&Reserved->Header[Binding->DefHeaderSize];
                }

            }

            ++Device->TempDatagramsSent;
            Device->TempDatagramBytesSent += Parameters->SendLength;

            if (Device->MultiCardZeroVirtual ||
                (RemoteAddress->Socket == SAP_SOCKET)) {

                //
                // SAP frames need to look like they come from the
                // local network, not the virtual one. The same is
                // true if we are running multiple nets without
                // a virtual network number.
                //
                // If this is a binding set member and a local target
                // was provided we will send using the real node of
                // the binding, even if it was a slave. This is
                // intentional. If no local target was provided then
                // this will not be a binding slave.
                //

                IpxConstructHeader(
                    (PUCHAR)IpxHeader,
                    LengthIncludingHeader,
                    PacketType,
                    RemoteAddress,
                    &Binding->LocalAddress);

                IpxHeader->SourceSocket = Address->SendSourceSocket;

            } else {

                IpxConstructHeader(
                    (PUCHAR)IpxHeader,
                    LengthIncludingHeader,
                    PacketType,
                    RemoteAddress,
                    &Address->LocalAddress);

            }


            //
            // Fill in the MAC header and submit the frame to NDIS.
            //

#if DBG
            CTEAssert (!Reserved->SendInProgress);
            Reserved->SendInProgress = TRUE;
#endif

            if ((NdisStatus = (*Binding->SendFrameHandler)(
                     Binding->Adapter,
                     LocalTarget,
                     Packet,
                     Parameters->SendLength + sizeof(IPX_HEADER),
                     sizeof(IPX_HEADER))) != NDIS_STATUS_PENDING) {

                IpxSendComplete(
                    (NDIS_HANDLE)Binding->Adapter,
                    Packet,
                    NdisStatus);
            }

            IPX_END_SYNC (&SyncContext);

            return STATUS_PENDING;

        } else {

            //
            // The address file state was closing.
            //

            IPX_FREE_LOCK (&Address->Lock, LockHandle);
            Status = STATUS_INVALID_HANDLE;
            goto error_send_no_packet;

        }

    } else {

        //
        // The address file didn't look like one.
        //

        Status = STATUS_INVALID_HANDLE;
        goto error_send_no_packet;
    }

    //
    // Jump here if we want to fail the send and we have already
    // allocated the packet and ref'ed the address file.
    //

error_send_with_packet:

    if (Reserved->OwnedByAddress) {
        Reserved->Address->SendPacketInUse = FALSE;
    } else {
        IPX_PUSH_ENTRY_LIST(
            &Device->SendPacketList,
            &Reserved->PoolLinkage,
            &Device->Lock);
    }
    IpxDereferenceAddressFileSync (AddressFile, AFREF_SEND_DGRAM);

error_send_no_packet:

    //
    // Jump here if we fail before doing any of that.
    //

    IPX_END_SYNC (&SyncContext);

    irpSp = IoGetCurrentIrpStackLocation( Request );
    if ( irpSp->MinorFunction == TDI_DIRECT_SEND_DATAGRAM ) {

        REQUEST_STATUS(Request) = Status;
        Request->CurrentLocation++,
        Request->Tail.Overlay.CurrentStackLocation++;

        (VOID) irpSp->CompletionRoutine(
                                    NULL,
                                    Request,
                                    irpSp->Context
                                    );

        IpxFreeRequest (DeviceObject, Request);
    }

    return Status;

}   /* IpxTdiSendDatagram */


#if DBG
VOID
IpxConstructHeader(
    IN PUCHAR Header,
    IN USHORT PacketLength,
    IN UCHAR PacketType,
    IN TDI_ADDRESS_IPX UNALIGNED * RemoteAddress,
    IN PTDI_ADDRESS_IPX LocalAddress
    )

/*++

Routine Description:

    This routine constructs an IPX header in a packet.

Arguments:

    Header - The location at which the header should be built.

    PacketLength - The length of the packet, including the IPX header.

    PacketType - The packet type of the frame.

    RemoteAddress - The remote IPX address.

    LocalAddress - The local IPX address.

Return Value:

    None.

--*/

{

    PIPX_HEADER IpxHeader = (PIPX_HEADER)Header;

    IpxHeader->CheckSum = 0xffff;
    IpxHeader->PacketLength[0] = (UCHAR)(PacketLength / 256);
    IpxHeader->PacketLength[1] = (UCHAR)(PacketLength % 256);
    IpxHeader->TransportControl = 0;
    IpxHeader->PacketType = PacketType;

    //
    // These copies depend on the fact that the destination
    // network is the first field in the 12-byte address.
    //

    RtlCopyMemory(IpxHeader->DestinationNetwork, (PVOID)RemoteAddress, 12);
    RtlCopyMemory(IpxHeader->SourceNetwork, LocalAddress, 12);

}   /* IpxConstructHeader */
#endif

