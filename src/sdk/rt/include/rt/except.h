/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RT_EXCEPT_H_
#define _RT_EXCEPT_H_

#include <rt/context.h>
#include <stddef.h>

#define RT_EXC_NUMBER_OF_PARAMETERS 16

#define RT_EXC_ACCESS_VIOLATION 0
#define RT_EXC_ARRAY_BOUNDS_EXCEEDED 1
#define RT_EXC_BREAKPOINT 2
#define RT_EXC_DATATYPE_MISALIGNMENT 3
#define RT_EXC_FLT_DENORMAL_OPERAND 4
#define RT_EXC_FLT_DIVIDE_BY_ZERO 5
#define RT_EXC_FLT_INEXACT_RESULT 6
#define RT_EXC_FLT_INVALID_OPERATION 7
#define RT_EXC_FLT_OVERFLOW 8
#define RT_EXC_FLT_STACK_CHECK 9
#define RT_EXC_FLT_UNDERFLOW 10
#define RT_EXC_ILLEGAL_INSTRUCTION 11
#define RT_EXC_IN_PAGE_ERROR 12
#define RT_EXC_INT_DIVIDE_BY_ZERO 13
#define RT_EXC_INT_OVERFLOW 14
#define RT_EXC_INVALID_DISPOSITION 15
#define RT_EXC_NONCONTINUABLE_EXCEPTION 16
#define RT_EXC_PRIV_INSTRUCTION 17
#define RT_EXC_SINGLE_STEP 18
#define RT_EXC_STACK_OVERFLOW 19
#define RT_EXC_UNWIND 20

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

#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <rt/amd64/unwind.h>
#else
#error "Undefined ARCH for the rt module!"
#endif

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

int RtDispatchException(RtExceptionRecord *ExceptionRecord, RtContext *ContextRecord);

#endif /* _RT_EXCEPT_H_ */
