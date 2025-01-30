/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.altmacro
.text

#include <amd64/context.inc>
#include <amd64/except.inc>
#include <amd64/irql.inc>

.extern EvpProcessQueue
.extern EvpHandleTimer
.extern HalpDispatchException
.extern HalpDispatchInterrup
.extern HalpDispatchTrap
.extern HalpDispatchNmi
.extern HalpSendEoi
.extern PspProcessQueue

.global HalpDefaultInterruptHandlers
HalpDefaultInterruptHandlers:
.set i, 32
.rept 224
    push $i
    jmp HalpDefaultInterruptEntry
    .align 16
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
    mov %rsp, %rcx
    mov $RT_EXC_NUMERIC_COPROCESSOR, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpDoubleFaultTrapEntry
.global HalpDoubleFaultTrapEntry
HalpDoubleFaultTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_DOUBLE_FAULT, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpSegmentOverrunTrapEntry
.global HalpSegmentOverrunTrapEntry
HalpSegmentOverrunTrapEntry:
    ENTER_INTERRUPT
    mov %rsp, %rcx
    mov $RT_EXC_SEGMENT_OVERRUN, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpInvalidTssTrapEntry
.global HalpInvalidTssTrapEntry
HalpInvalidTssTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_INVALID_TSS, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpSegmentNotPresentTrapEntry
.global HalpSegmentNotPresentTrapEntry
HalpSegmentNotPresentTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_SEGMENT_NOT_PRESENT, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpStackSegmentTrapEntry
.global HalpStackSegmentTrapEntry
HalpStackSegmentTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_STACK_SEGMENT, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpGeneralProtectionTrapEntry
.global HalpGeneralProtectionTrapEntry
HalpGeneralProtectionTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $RT_EXC_GENERAL_PROTECTION_FAULT, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpPageFaultTrapEntry
.global HalpPageFaultTrapEntry
HalpPageFaultTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $RT_EXC_PAGE_FAULT, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpReservedTrapEntry
.global HalpReservedTrapEntry
HalpReservedTrapEntry:
    ENTER_INTERRUPT
    mov %rsp, %rcx
    mov $RT_EXC_RESERVED, %rdx
    call HalpDispatchTrap
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
    mov %rsp, %rcx
    mov $RT_EXC_MACHINE_CHECK, %rdx
    call HalpDispatchTrap
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
    mov %rsp, %rcx
    mov $RT_EXC_VIRTUALIZATION, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpControlProtectionTrapEntry
.global HalpControlProtectionTrapEntry
HalpControlProtectionTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_CONTROL_PROTECTION, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpHypervisorInjectionTrapEntry
.global HalpHypervisorInjectionTrapEntry
HalpHypervisorInjectionTrapEntry:
    ENTER_INTERRUPT
    mov %rsp, %rcx
    mov $RT_EXC_HYPERVISOR_INJECTION, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpVmmCommunicationTrapEntry
.global HalpVmmCommunicationTrapEntry
HalpVmmCommunicationTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_VMM_COMMUNICATION, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpSecurityTrapEntry
.global HalpSecurityTrapEntry
HalpSecurityTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_SECURITY, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpDispatchEntry
.global HalpDispatchEntry
HalpDispatchEntry:
    ENTER_INTERRUPT
    mov $KE_IRQL_DISPATCH, %rcx
    mov %rcx, %cr8
    call HalpSendEoi
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
    mov $HAL_INT_TIMER_IRQL, %rax
    mov %rax, %cr8
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
