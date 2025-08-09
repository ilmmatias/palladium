/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_EXCEPT_H_
#define _RT_EXCEPT_H_

#include <rt/context.h>
#include <stddef.h>

#define RT_EXC_NUMBER_OF_PARAMETERS 16

#define RT_EXC_ACCESS_VIOLATION 0x00000000
#define RT_EXC_ARRAY_BOUNDS_EXCEEDED 0x00000001
#define RT_EXC_STACK_OVERFLOW 0x00000002
#define RT_EXC_BREAKPOINT 0x00000003
#define RT_EXC_SINGLE_STEP 0x00000004
#define RT_EXC_DATATYPE_MISALIGNMENT 0x00000005
#define RT_EXC_FLT_DENORMAL_OPERAND 0x00000006
#define RT_EXC_FLT_DIVIDE_BY_ZERO 0x00000007
#define RT_EXC_FLT_INEXACT_RESULT 0x00000008
#define RT_EXC_FLT_INVALID_OPERATION 0x00000009
#define RT_EXC_FLT_OVERFLOW 0x0000000A
#define RT_EXC_FLT_UNDERFLOW 0x0000000B
#define RT_EXC_FLT_STACK_CHECK 0x0000000C
#define RT_EXC_INT_DIVIDE_BY_ZERO 0x0000000D
#define RT_EXC_INT_OVERFLOW 0x0000000E
#define RT_EXC_ILLEGAL_INSTRUCTION 0x0000000F
#define RT_EXC_PRIV_INSTRUCTION 0x00000010
#define RT_EXC_SECURITY_CHECK_FAILURE 0x00000011
#define RT_EXC_INVALID_DISPOSITION 0x00000012
#define RT_EXC_NONCONTINUABLE_EXCEPTION 0x00000013
#define RT_EXC_UNWIND 0x00000014

#ifdef ARCH_amd64
#define RT_EXC_NUMERIC_COPROCESSOR 0x10000000
#define RT_EXC_DOUBLE_FAULT 0x10000001
#define RT_EXC_SEGMENT_OVERRUN 0x10000002
#define RT_EXC_INVALID_TSS 0x10000003
#define RT_EXC_SEGMENT_NOT_PRESENT 0x10000004
#define RT_EXC_STACK_SEGMENT 0x10000005
#define RT_EXC_GENERAL_PROTECTION_FAULT 0x10000006
#define RT_EXC_PAGE_FAULT 0x10000007
#define RT_EXC_MACHINE_CHECK 0x10000008
#define RT_EXC_VIRTUALIZATION 0x10000009
#define RT_EXC_CONTROL_PROTECTION 0x1000000A
#define RT_EXC_HYPERVISOR_INJECTION 0x1000000B
#define RT_EXC_VMM_COMMUNICATION 0x1000000C
#define RT_EXC_SECURITY 0x1000000D
#define RT_EXC_RESERVED 0x1000000E
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

int RtCaptureStackTrace(void **Frames, int FramesToCapture, int FramesToSkip);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RT_EXCEPT_H_ */
