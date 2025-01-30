/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_EXCEPT_H_
#define _RT_EXCEPT_H_

#include <rt/context.h>
#include <stddef.h>

#define RT_EXC_NUMBER_OF_PARAMETERS 16

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
#define RT_EXC_FLT_STACK_CHECK 12
#define RT_EXC_INT_DIVIDE_BY_ZERO 13
#define RT_EXC_INT_OVERFLOW 14
#define RT_EXC_ILLEGAL_INSTRUCTION 15
#define RT_EXC_PRIV_INSTRUCTION 16
#define RT_EXC_INVALID_DISPOSITION 17
#define RT_EXC_NONCONTINUABLE_EXCEPTION 18
#define RT_EXC_UNWIND 19

#ifdef ARCH_amd64
#define RT_EXC_NUMERIC_COPROCESSOR 20
#define RT_EXC_DOUBLE_FAULT 21
#define RT_EXC_SEGMENT_OVERRUN 22
#define RT_EXC_INVALID_TSS 23
#define RT_EXC_SEGMENT_NOT_PRESENT 24
#define RT_EXC_STACK_SEGMENT 25
#define RT_EXC_GENERAL_PROTECTION_FAULT 26
#define RT_EXC_PAGE_FAULT 27
#define RT_EXC_MACHINE_CHECK 28
#define RT_EXC_VIRTUALIZATION 29
#define RT_EXC_CONTROL_PROTECTION 30
#define RT_EXC_HYPERVISOR_INJECTION 31
#define RT_EXC_VMM_COMMUNICATION 32
#define RT_EXC_SECURITY 33
#define RT_EXC_RESERVED 34
#endif

#define RT_EXC_FLAG_NONCONTINUABLE 0x01
#define RT_EXC_FLAG_UNWIND 0x02
#define RT_EXC_FLAG_EXIT_UNWIND 0x04
#define RT_EXC_FLAG_TARGET_UNWIND 0x08
#define RT_EXC_FLAG_COLLIDED_UNWIND 0x10

#define RT_EXC_EXECUTE_HANDLER -1
#define RT_EXC_CONTINUE_EXECUTION 0
#define RT_EXC_CONTINUE_SEARCH 1
#define RT_EXC_NESTED_EXCEPTION 2
#define RT_EXC_COLLIDED_UNWIND 3

typedef struct RtExceptionRecord {
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    struct RtExceptionRecord *ExceptionRecord;
    void *ExceptionAddress;
    uint32_t NumberOfParameters;
    void *ExceptionInformation[RT_EXC_NUMBER_OF_PARAMETERS];
} RtExceptionRecord;

typedef struct {
    RtExceptionRecord *ExceptionRecord;
    RtContext *ContextRecord;
} RtExceptionPointers;

struct RtDispatcherContext;

typedef int (*RtExceptionRoutine)(
    RtExceptionRecord *ExceptionRecord,
    uint64_t EstablisherFrame,
    RtContext *ContextRecord,
    struct RtDispatcherContext *DispatcherContext);

typedef int (*RtExceptionFilter)(RtExceptionPointers *ExceptionPointers, uint64_t EstablisherFrame);
typedef void (*RtTerminationHandler)(int AbnormalTermination, uint64_t EstablisherFrame);

#if defined(ARCH_amd64)
#include <rt/amd64/unwind.h>
#else
#error "Undefined ARCH for the rt module!"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint64_t RtLookupImageBase(uint64_t Address);
RtRuntimeFunction *RtLookupFunctionEntry(uint64_t ImageBase, uint64_t Address);

RtExceptionRoutine RtVirtualUnwind(
    int HandlerType,
    uint64_t ImageBase,
    uint64_t ControlPc,
    RtRuntimeFunction *FunctionEntry,
    RtContext *ContextRecord,
    void **HandlerData,
    uint64_t *EstablisherFrame);

[[noreturn]] void
RtUnwind(void *TargetFrame, void *TargetIp, RtExceptionRecord *ExceptionRecord, void *ReturnValue);

bool RtDispatchException(RtExceptionRecord *ExceptionRecord, RtContext *ContextRecord);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RT_EXCEPT_H_ */
