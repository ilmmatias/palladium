/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RT_EXCEPT_H_
#define _RT_EXCEPT_H_

#include <stddef.h>
#include <stdint.h>

#define RT_MAX_EXCEPT_ARGS 15

typedef struct RtExceptionRecord {
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    struct RtExceptionRecord *ExceptionRecord;
    void *ExceptionAddress;
    uint32_t NumberParameters;
    uintptr_t ExceptionInformation[RT_MAX_EXCEPT_ARGS];
} RtExceptionRecord;

#if defined(ARCH_x86) || defined(ARCH_amd64)
#define RT_UNW_FLAG_NHANDLER 0x00
#define RT_UNW_FLAG_EHANDLER 0x01
#define RT_UNW_FLAG_UHANDLER 0x02
#define RT_UNW_FLAG_CHAININFO 0x04

#define RT_UWOP_PUSH_NONVOL 0
#define RT_UWOP_ALLOC_LARGE 1
#define RT_UWOP_ALLOC_SMALL 2
#define RT_UWOP_SET_FPREG 3
#define RT_UWOP_SAVE_NONVOL 4
#define RT_UWOP_SAVE_NONVOL_FAR 5
#define RT_UWOP_EPILOG 6
#define RT_UWOP_SPARE_CODE 7
#define RT_UWOP_SAVE_XMM128 8
#define RT_UWOP_SAVE_XMM128_FAR 9
#define RT_UWOP_PUSH_MACHFRAME 10

#define RtGetUnwindCodeEntry(info, index) ((info)->UnwindCode[index])
#define RtGetLanguageSpecificData(info) \
    ((uint64_t *)&RtGetUnwindCodeEntry((info), ((info)->CountOfCodes + 1) & ~1))
#define RtGetExceptionHandler(base, info) \
    ((RtExceptionRoutine *)((base) + *(uintptr_t *)RtGetLanguageSpecificData(info)))
#define RtGetChainedFunctionEntry(base, info) \
    ((RtRuntimeFunction *)((base) + *(uintptr_t *)RtGetLanguageSpecificData(info)))
#define RtGetExceptionDataPtr(info) ((void *)((uintptr_t *)RtGetLanguageSpecificData(info) + 1))

typedef union {
    struct {
        uint8_t CodeOffset;
        uint8_t UnwindOp : 4;
        uint8_t OpInfo : 4;
    };
    uint16_t FrameOffset;
} RtUnwindCode;

typedef struct {
    uint8_t Version : 3;
    uint8_t Flags : 5;
    uint8_t SizeOfProlog;
    uint8_t CountOfCodes;
    uint8_t FrameRegister : 4;
    uint8_t FrameOffset : 4;
    RtUnwindCode UnwindCode[];
} RtUnwindInfo;

typedef struct {
    uint32_t BeginAddress;
    uint32_t EndAddress;
    uint32_t UnwindData;
} RtRuntimeFunction;

typedef int (*RtExceptionRoutine)(
    RtExceptionRecord *ExceptionRecord,
    uint64_t EstablisherFrame,
    void *ContextRecord,
    void *DispatcherContext);
#else
#error "Undefined ARCH for the rt module!"
#endif

uint64_t RtLookupImageBase(uint64_t Address);
RtRuntimeFunction *RtLookupFunctionEntry(uint64_t Address);
int RtUnwindFrame(
    HalRegisterState *Context,
    RtExceptionRoutine *LanguageHandler,
    void **LanguageData);

#endif /* _RT_EXCEPT_H_ */
