if NT_INST
else
        TITLE   "Spin Locks"
;++
;
;  Copyright (c) 1989  Microsoft Corporation
;
;  Module Name:
;
;     spindbg.asm
;
;  Abstract:
;
;  Author:
;
;     Bryan Willman (bryanwi) 13 Dec 89
;
;  Environment:
;
;     Kernel mode only.
;
;  Revision History:
;
;--

        PAGE

.386p

include ks386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc


if DBG
        EXTRNP  _KeBugCheck,1
ifdef DBGMP
        EXTRNP  _KiPollDebugger,0
endif
        extrn   _KeTickCount:DWORD
        extrn   _KiSpinlockTimeout:DWORD
endif


_TEXT$00   SEGMENT  DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
;  VOID
;  Kii386SpinOnSpinLock (
;     IN PKSPIN_LOCK SpinLock
;     IN ULONG       Flag
;     )
;
;  Routine Description:
;
;     This function is called on a debug build to spin on a spinlock.
;     It is invoked by the DEBUG version of SPIN_ON_SPINLOCK macro.
;
;  Warning:
;
;     Not called with C calling conventions
;     Does not destroy any register
;
;--

cPublicProc Kii386SpinOnSpinLock,2

if DBG
cPublicFpo 2,2
        push    eax
        push    ebx

        mov     eax, [esp+12]           ; (eax) = LockAddress

        mov     ebx, PCR[PcPrcbData.PbCurrentThread]
        or      ebx, 1                  ; or on busy bit
        cmp     ebx, [eax]              ; current thread the owner?
        je      short ssl_sameid        ; Yes, go abort

ssl_10:
        mov     ebx, _KeTickCount       ; Current time
        add     ebx, _KiSpinlockTimeout ; wait n ticks

ifdef DBGMP
        test    byte ptr [esp+16], 2    ; poll debugger while waiting?
        jnz     short ssl_30
endif

;
; Spin while watching KeTickCount
;

ssl_20: cmp     _KeTickCount, ebx       ; check current time
        jnc     short ssl_timeout       ; NC, too many ticks have gone by

        test    dword ptr [eax], 1
        jnz     short ssl_20

ssl_exit:
        pop     ebx                     ; Spinlock is not busy, return
        pop     eax
        stdRET  Kii386SpinOnSpinLock

ifdef DBGMP
;
; Spin while watching KeTickCount & poll debugger
;

ssl_30: cmp     _KeTickCount, ebx       ; check current time
        jnc     short ssl_timeout       ; overflowed

        stdCall _KiPollDebugger

        test    dword ptr [eax], 1
        jnz     short ssl_30

        pop     ebx                     ; Spinlock is not busy, return
        pop     eax
        stdRET  Kii386SpinOnSpinLock
endif

;
; Out of line expection conditions
;

ssl_sameid:
        test    byte ptr [esp+16], 1    ; ID check enabled?
        jz      short ssl_10            ; no, continue

        stdCall _KeBugCheck,<eax>       ; recursed on lock, abort

ssl_timeout:
        test    byte ptr [esp+16], 4    ; Timeout check enabled?
        jz      short ssl_10            ; no, continue

        test    dword ptr [eax], 1      ; Check to see if spinlock was freed
        jz      short ssl_exit          ; while timer interrupt was handled

        public SpinLockSpinningForTooLong
SpinLockSpinningForTooLong:

        int 3                           ; Stop here
        jmp     short ssl_10            ; re-wait

else    ; DBG
        stdRET  Kii386SpinOnSpinLock
endif
stdENDP Kii386SpinOnSpinLock,2

_TEXT$00   ends

endif   ; NT_INST
        end

