//        TITLE("Interprocessor Interrupt support routines")
//++
//
// Copyright (c) 1993  Microsoft Corporation
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    mpipi.s
//
// Abstract:
//
//    This module implements the Alpha AXP specific functions required to
//    support multiprocessor systems.
//
// Author:
//
//    David N. Cutler (davec) 22-Apr-1993
//    Joe Notarangelo  29-Nov-1993
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "ksalpha.h"

        SBTTL("Interprocess Interrupt Processing")
//++
//
// VOID
// KeIpiInterrupt (
//    IN PKTRAP_FRAME TrapFrame
//    );
//
// Routine Description:
//
//    This routine is entered as the result of an interprocessor interrupt.
//    It's function is to process all interprocess immediate and packet
//    requests.
//
// Arguments:
//
//    TrapFrame (fp/s6) - Supplies a ppointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        NESTED_ENTRY(KeIpiInterrupt, ExceptionFrameLength, zero)

        lda     sp, -ExceptionFrameLength(sp) // allocate exception frame
        stq     ra, ExIntRa(sp)         // save return address

        PROLOGUE_END

//
// Process all interprocessor requests.
//

        bsr     ra, KiIpiProcessRequests // process requests
        and     v0, IPI_FREEZE, t0      // check if freeze is requested
        beq     t0, 10f                 // if eq, no freeze requested

//
// Save the volatile floating state, the nonvolatile floating state,
// and the nonvolatile integer state.
//

        bsr     ra, KiSaveVolatileFloatState // save volatile float in trap

        bsr     ra, KiSaveNonVolatileFloatState // save nv float in exception

        stq     s0, ExIntS0(sp)         // save nonvolatile integer state
        stq     s1, ExIntS1(sp)         //
        stq     s2, ExIntS2(sp)         //
        stq     s3, ExIntS3(sp)         //
        stq     s4, ExIntS4(sp)         //
        stq     s5, ExIntS5(sp)         //
        stq     fp, ExIntFp(sp)         //

//
// Freeze the execution of the current processor.
//

        bis     fp, zero, a0            // set address of trap frame
        bis     sp, zero, a1            // set address of exception frame
        bsr     ra, KiFreezeTargetExecution // freeze current processor

//
// Restore the volatile floating state, the nonvolatile floating state,
// and the nonvolatile integer state.
//

        ldq     s0, ExIntS0(sp)         // restore nonvolatile integer state
        ldq     s1, ExIntS1(sp)         //
        ldq     s2, ExIntS2(sp)         //
        ldq     s3, ExIntS3(sp)         //
        ldq     s4, ExIntS4(sp)         //
        ldq     s5, ExIntS5(sp)         //
        ldq     fp, ExIntFp(sp)         //

        bsr     ra, KiRestoreVolatileFloatState // restore volatile float

        bsr     ra, KiRestoreNonVolatileFloatState // restore nv float state


//
// Cleanup and return to the caller.
//

10:
        ldq     ra, ExIntRa(sp)         // restore return address
        lda     sp, ExceptionFrameLength(sp) // deallocate exception frame
        ret     zero, (ra)              // return

        .end    KeIpiInterrupt

        SBTTL("Processor Request")
//++
//
// ULONG
// KiIpiProcessRequests (
//    VOID
//    );
//
// Routine Description:
//
//    This routine processes interprocessor requests and returns a summary
//    of the requests that were processed.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The request summary is returned as the function value.
//
//--

        .struct 0
PrS0:   .space  8                       // saved integer register s0
PrS1:   .space  8                       // saved integer register s1
        .space  8                       // fill
PrRa:   .space  8                       // saved return address
PrFrameLength:

        NESTED_ENTRY(KiIpiProcessRequests, PrFrameLength, zero)

        lda     sp, -PrFrameLength(sp)  // allocate stack frame
        stq     s0, PrS0(sp)            // save integer register s0


#if NT_INST

        stq     s1, PrS1(sp)            // save integer register s1

#endif

        stq     ra, PrRa(sp)            // save return address

        PROLOGUE_END

//
// Read request summary and write a zero result interlocked.
//
        mb                              // get consistent view of memory

        GET_PROCESSOR_CONTROL_REGION_BASE // v0 = Pcr base address
        ldl     t0, PcPrcb(v0)          // get current processor block address

#if NT_INST

        ldl     s1, PbIpiCounts(t0)     // get interrupt count structure

#endif

10:
        ldq_l   s0, PbRequestSummary(t0) // get request summary and entry address
        bis     zero, zero, t1          // set zero value for store
        stq_c   t1, PbRequestSummary(t0) // zero request summary, conditionally
        beq     t1, 15f                 // if eq, store conditional failed
        sra     s0, 32, a0              // shift out entry address

//
// Check for Packet ready.
//
// If a packet is ready, then get the address of the requested function
// and call the function passing the address of the packet address as a
// parameter.
//

        and     s0, IPI_PACKET_READY, t2 // check for packet ready
        beq     t2, 20f                 // if eq, no packet ready
        ldl     t1, 0(a0)               // get sender's PRCB address
        ldl     t2, PbWorkerRoutine(t1) // get address of worker function
        ldl     a1, PbCurrentPacket(t1) // get request parameters
        ldl     a2, PbCurrentPacket +4(t1)
        ldl     a3, PbCurrentPacket +8(t1)
        jsr     ra, (t2)                // call worker routine

        wmb

#if NT_INST

        ldl     t1, IcPacket(s1)        // increment number of packet requests
        addl    t1, 1, t1               //
        stl     t1, IcPacket(s1)        //

#endif

//
// Check for APC interrupt request.
//
// If an APC interrupt is requested, then request a software interrupt at
// APC level on the current processor.
//

20:
        and     s0, IPI_APC, t1         // check for APC interrupt request
        beq     t1, 30f                 // if eq no APC interrupt requested
        ldil    a0, APC_LEVEL           // set interrupt request level

        REQUEST_SOFTWARE_INTERRUPT      // request APC interrupt

#if NT_INST

        ldl     t1, IcAPC(s1)           // increment number of APC requests
        addl    t1, 1, t1               //
        stl     t1, IcAPC(s1)           //

#endif

//
// Check for DPC interrupt request.
//
// If a DPC interrupt is requested, then request a software interrupt at
// DPC level on the current processor.
//

30:
        and     s0, IPI_DPC, t1         // check for DPC interrupt request
        beq     t1, 40f                 // if eq, no DPC interrupt requested
        ldil    a0, DISPATCH_LEVEL      // set interrupt request level

        REQUEST_SOFTWARE_INTERRUPT      // request DPC interrupt

#if NT_INST

        ldl     t1, IcDPC(s1)           // increment number of DPC requests
        addl    t1, 1, t1               //
        stl     t1, IcDPC(s1)           //

#endif

//
// Set function return value, restore registers, and return.
//

40:
        bis     s0, zero, v0            // set function return value

        ldq     s0, PrS0(sp)            // restore integer register s0

#if NT_INST

        and     v0, IPI_FREEZE, t1      // check if freeze requested
        beq     t1, 50f                 // if eq, no freeze requested

        ldl     t1, IcFreeze(s1)        // increment number of freeze requests
        addl    t1, 1, t1               //
        stl     t1, IcFreeze(s1)        //
50:
        ldq     s1, PrS1(sp)            // restore integer register s1

#endif

        ldq     ra, PrRa(sp)            // restore return address
        lda     sp, PrFrameLength(sp)   // deallocate stack frame
        ret     zero, (ra)              // return

15:
        br      zero, 10b               // store conditonal failed, retry

        .end    KiIpiProcessRequests

        SBTTL("Send Interprocess Request")
//++
//
// VOID
// KiIpiSend (
//    IN KAFINITY TargetProcessors,
//    IN KIPI_REQUEST IpiRequest
//    );
//
// Routine Description:
//
//    This routine requests the specified operation on the target set of
//    processors.
//
// Arguments:
//
//    TargetProcessors (a0) - Supplies the set of processors on which the
//        specified operation is to be executed.
//
//    IpiRequest (a1) - Supplies the request operation mask.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiIpiSend)

#if !defined(NT_UP)

        bis     a0, zero, t0            // copy target processor set
        lda     t1, KiProcessorBlock    // get processor block array address

10:

        blbc    t0, 30f                 // if clear target processor not set

        ldl     t2, 0(t1)               // get target processor block address

//
// Merge the new request into the target processor request summary.
// The store is conditional to ensure that no updates are lost.
//

20:
        ldq_l   t3, PbRequestSummary(t2) // get target request summary
        bis     t3, a1, t4              // merge new request with summary
        stq_c   t4, PbRequestSummary(t2) // set new request summary
        beq     t4, 25f                 // if eq, store conditional failed


30:
        srl     t0, 1, t0               // shift to next target
        lda     t1, 4(t1)               // get next processor block element
        bne     t0, 10b                 // if ne, more targets requested

        wmb

        ldl     t0, __imp_HalRequestIpi
        jmp     zero, (t0)              // request IPI interrupt on targets

#else

        ret     zero, (ra)              // simply return for uni-processor

#endif

25:
        br      zero, 20b               // store conditional failed, retry

        .end    KiIpiSend

        SBTTL("Send Interprocess Request Packet")
//++
//
// VOID
// KiIpiSendPacket (
//    IN KAFINITY TargetProcessors,
//    IN PKIPI_WORKER WorkerFunction,
//    IN PVOID Parameter1,
//    IN PVOID Parameter2,
//    IN PVOID Parameter3
//    );
//
// Routine Description:
//
//    This routine executes the specified worker function on the specified
//    set of processors.
//
// Arguments:
//
//    TargetProcessors (a0) - Supplies the set of processors on which the
//        specified operation is to be executed.
//
//    WorkerFunction (a1) - Supplies the address of the worker function.
//
//    Parameter1 - Parameter3 - Supplies arguments for worker.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiIpiSendPacket)

#if !defined(NT_UP)

        GET_PROCESSOR_CONTROL_REGION_BASE // v0 = Pcr base address
        ldl     t5, PcPrcb(v0)          // get current processor block address

        bis     a0, zero, t1            // copy target processor set
        lda     t2, KiProcessorBlock    // get processor block array address
        lda     t0, PbRequestPacket(t5) // compute address of packet array

//
// Store function address and parameters in the packet area of the PRCB on
// the current processor.
//
        stl     a1, PbWorkerRoutine(t5) // set worker function address
        stl     a2, PbCurrentPacket(t5) // store worker function parameters
        stl     a3, PbCurrentPacket +4(t5)
        stl     a4, PbCurrentPacket +8(t5)


//
// Loop through the target processors and send the packet to the specified
// recipients.
//

10:
        blbc    t1, 30f                 // if eq, target not specified
        ldl     v0, 0(t2)               // get target processor block address
        stl     t5, 0(t0)               // store source PRCB in entry

        sll     t0, 32, t3              // shift packet address to upper 32 bits
        bis     t3, IPI_PACKET_READY, t3    // set packet ready in low 32 bits
20:
        ldq_l   t4, PbRequestSummary(v0)    // get request summary of target
        and     t4, IPI_PACKET_READY, t6    // check if target packet busy
        bne     t6, 25f                     // if ne, target packet busy
        bis     t4, t3, t4                  // set entry address in request summary
        stq_c   t4, PbRequestSummary(v0)    // store request summary and address
        beq     t4, 20b                 // if eq, store conditional failed

30:
        lda     t0, 4(t0)               // advance to next packet entry
        lda     t2, 4(t2)               // advance to next array element
        srl     t1, 1, t1               // shift to next target
        bne     t1, 10b                 // if ne, more targets to process

//
// Ensure writes get to memory
//
        wmb

        ldl     t0, __imp_HalRequestIpi
        jmp     zero, (t0)              // request IPI interrupt on targets

#else

        ret     zero, (ra)              // simply return for uni-processor

#endif

#if !defined(NT_UP)

25:
//
// Packet not ready, spin in cache until it looks available.
//
        ldq     t4, PbRequestSummary(v0)    // get request summary of target
        and     t4, IPI_PACKET_READY, t6    // check if target packet busy
        beq     t6, 20b                     // looks available, try again
        br      zero, 25b                   // spin again

#endif //!NT_UP

        .end    KiIpiSendPacket

        SBTTL("Save Processor Control State")
//++
//
// VOID
// KiSaveProcessorState (
//    IN PKPROCESSOR_STATE ProcessorState
//    );
//
// Routine Description:
//
//    This routine saves the processor's control state for debugger.
//
// Arguments:
//
//    ProcessorState (a0) - Pointer to PROCSSOR_STATE
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiSaveProcessorControlState)

        ret     zero, (ra)              // return

        .end    KiSaveProcessorControlState
