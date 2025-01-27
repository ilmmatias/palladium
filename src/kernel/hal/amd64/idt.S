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

/* And sync these ones with their values on except.h. */
#define RT_EXC_ACCESS_VIOLATION 0
#define RT_EXC_ARRAY_BOUNDS_EXCEEDED 1
#define RT_EXC_STACK_OVERFLOW 2
#define RT_EXC_BREAKPOINT 3
#define RT_EXC_SINGLE_STEP 4
#define RT_EXC_DATATYPE_MISALIGNMENT 5
#define RT_EXC_FLT_DENORMAL_OPERAND 6
#define RT_EXC_FLT_DIVIDE_BY_ZERO 7
#define RT_EXC_FLT_INEXACT_RESULT 8
#define RT_EXC_FLT_INVALID_OPERATION 9
#define RT_EXC_FLT_OVERFLOW 10
#define RT_EXC_FLT_UNDERFLOW 11
#define RT_EXC_INT_DIVIDE_BY_ZERO 12
#define RT_EXC_INT_OVERFLOW 13
#define RT_EXC_ILLEGAL_INSTRUCTION 14
#define RT_EXC_PRIV_INSTRUCTION 15
#define RT_EXC_INVALID_DISPOSITION 16
#define RT_EXC_NONCONTINUABLE_EXCEPTION 17
#define RT_EXC_UNWIND 18
#define RT_EXC_FLT_STACK_CHECK 19
#define RT_EXC_NUMERIC_COPROCESSOR 20
#define RT_EXC_DOUBLE_FAULT 21
#define RT_EXC_SEGMENT_OVERRUN 22
#define RT_EXC_INVALID_TSS 23
#define RT_EXC_SEGMENT_NOT_PRESENT 24
#define RT_EXC_STACK_SEGMENT 25
#define RT_EXC_MACHINE_CHECK 26
#define RT_EXC_VIRTUALIZATION 27
#define RT_EXC_CONTROL_PROTECTION 28
#define RT_EXC_HYPERVISOR_INJECTION 29
#define RT_EXC_VMM_COMMUNICATION 30
#define RT_EXC_SECURITY 31
#define RT_EXC_RESERVED 32

.extern EvpProcessQueue
.extern EvpHandleTimer
.extern HalpDispatchException
.extern HalpDispatchInterrupt
.extern HalpDispatchNmi
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

.seh_proc HalpDivisionTrapEntry
.global HalpDivisionTrapEntry
HalpDivisionTrapEntry:
    ENTER_INTERRUPT
    mov $RT_EXC_INT_DIVIDE_BY_ZERO, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpDebugTrapEntry
.global HalpDebugTrapEntry
HalpDebugTrapEntry:
    ENTER_INTERRUPT
    mov $RT_EXC_SINGLE_STEP, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpNmiTrapEntry
.global HalpNmiTrapEntry
HalpNmiTrapEntry:
    ENTER_INTERRUPT
    call HalpDispatchNmi
1:  jmp 1b
.seh_endproc

.seh_proc HalpBreakpointTrapEntry
.global HalpBreakpointTrapEntry
HalpBreakpointTrapEntry:
    ENTER_INTERRUPT
    mov $RT_EXC_BREAKPOINT, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpOverflowTrapEntry
.global HalpOverflowTrapEntry
HalpOverflowTrapEntry:
    ENTER_INTERRUPT
    mov $RT_EXC_INT_OVERFLOW, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpBoundRangeTrapEntry
.global HalpBoundRangeTrapEntry
HalpBoundRangeTrapEntry:
    ENTER_INTERRUPT
    mov $RT_EXC_ARRAY_BOUNDS_EXCEEDED, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpInvalidOpcodeTrapEntry
.global HalpInvalidOpcodeTrapEntry
HalpInvalidOpcodeTrapEntry:
    ENTER_INTERRUPT
    mov $RT_EXC_ILLEGAL_INSTRUCTION, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpNumericCoprocessorTrapEntry
.global HalpNumericCoprocessorTrapEntry
HalpNumericCoprocessorTrapEntry:
    ENTER_INTERRUPT
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpDoubleFaultTrapEntry
.global HalpDoubleFaultTrapEntry
HalpDoubleFaultTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpSegmentOverrunTrapEntry
.global HalpSegmentOverrunTrapEntry
HalpSegmentOverrunTrapEntry:
    ENTER_INTERRUPT
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpInvalidTssTrapEntry
.global HalpInvalidTssTrapEntry
HalpInvalidTssTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpSegmentNotPresentTrapEntry
.global HalpSegmentNotPresentTrapEntry
HalpSegmentNotPresentTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpStackSegmentTrapEntry
.global HalpStackSegmentTrapEntry
HalpStackSegmentTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpGeneralProtectionTrapEntry
.global HalpGeneralProtectionTrapEntry
HalpGeneralProtectionTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $RT_EXC_ACCESS_VIOLATION, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpPageFaultTrapEntry
.global HalpPageFaultTrapEntry
HalpPageFaultTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $RT_EXC_ACCESS_VIOLATION, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpReservedTrapEntry
.global HalpReservedTrapEntry
HalpReservedTrapEntry:
    ENTER_INTERRUPT
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpX87FloatingPointTrapEntry
.global HalpX87FloatingPointTrapEntry
HalpX87FloatingPointTrapEntry:
    ENTER_INTERRUPT
    mov $RT_EXC_FLT_INVALID_OPERATION, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpAlignmentCheckTrapEntry
.global HalpAlignmentCheckTrapEntry
HalpAlignmentCheckTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $RT_EXC_DATATYPE_MISALIGNMENT, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpMachineCheckTrapEntry
.global HalpMachineCheckTrapEntry
HalpMachineCheckTrapEntry:
    ENTER_INTERRUPT
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpSimdFloatingPointTrapEntry
.global HalpSimdFloatingPointTrapEntry
HalpSimdFloatingPointTrapEntry:
    ENTER_INTERRUPT
    mov $RT_EXC_FLT_INVALID_OPERATION, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpVirtualizationTrapEntry
.global HalpVirtualizationTrapEntry
HalpVirtualizationTrapEntry:
    ENTER_INTERRUPT
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpControlProtectionTrapEntry
.global HalpControlProtectionTrapEntry
HalpControlProtectionTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpHypervisorInjectionTrapEntry
.global HalpHypervisorInjectionTrapEntry
HalpHypervisorInjectionTrapEntry:
    ENTER_INTERRUPT
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpVmmCommunicationTrapEntry
.global HalpVmmCommunicationTrapEntry
HalpVmmCommunicationTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
1:  jmp 1b
.seh_endproc

.seh_proc HalpSecurityTrapEntry
.global HalpSecurityTrapEntry
HalpSecurityTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $KE_PANIC_TRAP_NOT_HANDLED, %rcx
    call KeFatalError
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

.seh_proc HalpExceptionEntry
.global HalpExceptionEntry
HalpExceptionEntry:
    ENTER_EXCEPTION
    mov %rsp, %r8
    call HalpDispatchException
    LEAVE_EXCEPTION
.seh_endproc
