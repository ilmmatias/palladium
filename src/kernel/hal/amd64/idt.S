/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.altmacro
.text

#include <amd64/context.inc>

/* Sync these ones to their values in ke.h (if the list gets reorganized). */
#define KE_PANIC_TRAP_NOT_HANDLED 7
#define KE_PANIC_PAGE_FAULT_NOT_HANDLED 8
#define KE_PANIC_NMI_HARDWARE_FAILURE 9

/* Sync these ones to their values in ke.h as well. */
#define KE_IRQL_DISPATCH 2

.extern EvpProcessQueue
.extern EvpHandleTimer
.extern HalpDispatchInterrupt
.extern HalpSendEoi
.extern KeFatalError
.extern PspProcessQueue

.global HalpDefaultInterruptHandlers
HalpDefaultInterruptHandlers:
.set i, 32
.rept 224
    push $i
    jmp HalpDefaultInterruptEntry
    .set i, i + 1
.endr

.seh_proc HalpDefaultInterruptEntry
.global HalpDefaultInterruptEntry
HalpDefaultInterruptEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    call HalpDispatchInterrupt
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpDefaultTrapEntryWithoutErrorCode
.global HalpDefaultTrapEntryWithoutErrorCode
HalpDefaultTrapEntryWithoutErrorCode:
    ENTER_INTERRUPT
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpDefaultTrapEntryWithErrorCode
.global HalpDefaultTrapEntryWithErrorCode
HalpDefaultTrapEntryWithErrorCode:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpPageFaultTrapEntry
.global HalpPageFaultTrapEntry
HalpPageFaultTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_PAGE_FAULT_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpNmiTrapEntry
.global HalpNmiTrapEntry
HalpNmiTrapEntry:
    ENTER_INTERRUPT
    call HalpNmiHandler
1:  jmp 1b
.seh_endproc

.seh_proc HalpDispatchEntry
.global HalpDispatchEntry
HalpDispatchEntry:
    ENTER_INTERRUPT
    mov $KE_IRQL_DISPATCH, %rcx
    mov %rcx, %cr8
    call HalpSendEoi
    mov %rsp, %rcx
    call EvpProcessQueue
    sti
    mov %rsp, %rcx
    call PspProcessQueue
    cli
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpTimerEntry
.global HalpTimerEntry
HalpTimerEntry:
    ENTER_INTERRUPT
    mov %rsp, %rcx
    call EvpHandleTimer
    call HalpSendEoi
    LEAVE_INTERRUPT
.seh_endproc
