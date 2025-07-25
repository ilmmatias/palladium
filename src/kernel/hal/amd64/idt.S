/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.altmacro
.text

#include <kernel/detail/amd64/context.inc>
#include <kernel/detail/amd64/except.inc>
#include <kernel/detail/amd64/irql.inc>

.extern EvpProcessQueue
.extern EvpHandleTimer
.extern HalpDispatchException
.extern HalpDispatchInterrup
.extern HalpDispatchTrap
.extern HalpDispatchNmi
.extern HalpHandleTimer
.extern HalpSendEoi
.extern KiHandleIpi
.extern KiProcessWorkQueue
.extern PspProcessQueue
.extern PspProcessAlertQueue

.global HalpDefaultInterruptHandlers
HalpDefaultInterruptHandlers:
.set i, 0
.rept 256
    push $i
    jmp HalpDefaultInterruptEntry
    .align 16
    .set i, i + 1
.endr

.seh_proc HalpDefaultInterruptEntry
.global HalpDefaultInterruptEntry
.align 16
HalpDefaultInterruptEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov INTERRUPT_FRAME_INTERRUPT_NUMBER(%rsp), %rcx
    shr $4, %rcx
    mov %rcx, %cr8
    sti
    mov %rsp, %rcx
    call HalpDispatchInterrupt
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpDivisionTrapEntry
.global HalpDivisionTrapEntry
.align 16
HalpDivisionTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $RT_EXC_INT_DIVIDE_BY_ZERO, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpDebugTrapEntry
.global HalpDebugTrapEntry
.align 16
HalpDebugTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $RT_EXC_SINGLE_STEP, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpNmiTrapEntry
.global HalpNmiTrapEntry
.align 16
HalpNmiTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    call HalpDispatchNmi
1:  jmp 1b
.seh_endproc

.seh_proc HalpBreakpointTrapEntry
.global HalpBreakpointTrapEntry
.align 16
HalpBreakpointTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $RT_EXC_BREAKPOINT, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpOverflowTrapEntry
.global HalpOverflowTrapEntry
.align 16
HalpOverflowTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $RT_EXC_INT_OVERFLOW, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpBoundRangeTrapEntry
.global HalpBoundRangeTrapEntry
.align 16
HalpBoundRangeTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $RT_EXC_ARRAY_BOUNDS_EXCEEDED, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpInvalidOpcodeTrapEntry
.global HalpInvalidOpcodeTrapEntry
.align 16
HalpInvalidOpcodeTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $RT_EXC_ILLEGAL_INSTRUCTION, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpNumericCoprocessorTrapEntry
.global HalpNumericCoprocessorTrapEntry
.align 16
HalpNumericCoprocessorTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov %rsp, %rcx
    mov $RT_EXC_NUMERIC_COPROCESSOR, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpDoubleFaultTrapEntry
.global HalpDoubleFaultTrapEntry
.align 16
HalpDoubleFaultTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_DOUBLE_FAULT, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpSegmentOverrunTrapEntry
.global HalpSegmentOverrunTrapEntry
.align 16
HalpSegmentOverrunTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov %rsp, %rcx
    mov $RT_EXC_SEGMENT_OVERRUN, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpInvalidTssTrapEntry
.global HalpInvalidTssTrapEntry
.align 16
HalpInvalidTssTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_INVALID_TSS, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpSegmentNotPresentTrapEntry
.global HalpSegmentNotPresentTrapEntry
.align 16
HalpSegmentNotPresentTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_SEGMENT_NOT_PRESENT, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpStackSegmentTrapEntry
.global HalpStackSegmentTrapEntry
.align 16
HalpStackSegmentTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_STACK_SEGMENT, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpGeneralProtectionTrapEntry
.global HalpGeneralProtectionTrapEntry
.align 16
HalpGeneralProtectionTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $RT_EXC_GENERAL_PROTECTION_FAULT, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpPageFaultTrapEntry
.global HalpPageFaultTrapEntry
.align 16
HalpPageFaultTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $RT_EXC_PAGE_FAULT, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpReservedTrapEntry
.global HalpReservedTrapEntry
.align 16
HalpReservedTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov %rsp, %rcx
    mov $RT_EXC_RESERVED, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpX87FloatingPointTrapEntry
.global HalpX87FloatingPointTrapEntry
.align 16
HalpX87FloatingPointTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $RT_EXC_FLT_INVALID_OPERATION, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpAlignmentCheckTrapEntry
.global HalpAlignmentCheckTrapEntry
.align 16
HalpAlignmentCheckTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov $RT_EXC_DATATYPE_MISALIGNMENT, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpMachineCheckTrapEntry
.global HalpMachineCheckTrapEntry
.align 16
HalpMachineCheckTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov %rsp, %rcx
    mov $RT_EXC_MACHINE_CHECK, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpSimdFloatingPointTrapEntry
.global HalpSimdFloatingPointTrapEntry
.align 16
HalpSimdFloatingPointTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $RT_EXC_FLT_INVALID_OPERATION, %rcx
    mov %rsp, %rdx
    call HalpExceptionEntry
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpVirtualizationTrapEntry
.global HalpVirtualizationTrapEntry
.align 16
HalpVirtualizationTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov %rsp, %rcx
    mov $RT_EXC_VIRTUALIZATION, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpControlProtectionTrapEntry
.global HalpControlProtectionTrapEntry
.align 16
HalpControlProtectionTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_CONTROL_PROTECTION, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpHypervisorInjectionTrapEntry
.global HalpHypervisorInjectionTrapEntry
.align 16
HalpHypervisorInjectionTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov %rsp, %rcx
    mov $RT_EXC_HYPERVISOR_INJECTION, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpVmmCommunicationTrapEntry
.global HalpVmmCommunicationTrapEntry
.align 16
HalpVmmCommunicationTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_VMM_COMMUNICATION, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpSecurityTrapEntry
.global HalpSecurityTrapEntry
.align 16
HalpSecurityTrapEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_HAS_ERROR_CODE)
    mov %rsp, %rcx
    mov $RT_EXC_SECURITY, %rdx
    call HalpDispatchTrap
1:  jmp 1b
.seh_endproc

.seh_proc HalpAlertEntry
.global HalpAlertEntry
.align 16
HalpAlertEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $KE_IRQL_ALERT, %rcx
    mov %rcx, %cr8
    call HalpSendEoi
    sti
    call PspProcessAlertQueue
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpDispatchEntry
.global HalpDispatchEntry
.align 16
HalpDispatchEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $KE_IRQL_DISPATCH, %rcx
    mov %rcx, %cr8
    call HalpSendEoi
    sti
    call KiProcessWorkQueue
    call PspProcessQueue
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpTimerEntry
.global HalpTimerEntry
.align 16
HalpTimerEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $HALP_INT_TIMER_IRQL, %rcx
    mov %rcx, %cr8
    call HalpHandleTimer
    mov %rsp, %rcx
    call EvpHandleTimer
    call HalpSendEoi
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpIpiEntry
.global HalpIpiEntry
.align 16
HalpIpiEntry:
    ENTER_INTERRUPT (INTERRUPT_FLAGS_NONE)
    mov $HALP_INT_IPI_IRQL, %rcx
    mov %rcx, %cr8
    call KiHandleIpi
    call HalpSendEoi
    LEAVE_INTERRUPT
.seh_endproc

.seh_proc HalpSpuriousEntry
.global HalpSpuriousEntry
.align 16
HalpSpuriousEntry:
    .seh_pushframe
    .seh_endprologue
    iretq
.seh_endproc

.seh_proc HalpExceptionEntry
.global HalpExceptionEntry
.align 16
HalpExceptionEntry:
    ENTER_EXCEPTION
    mov %rsp, %r8
    call HalpDispatchException
    LEAVE_EXCEPTION
.seh_endproc
