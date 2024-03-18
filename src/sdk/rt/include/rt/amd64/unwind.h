/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_AMD64_UNWIND_H_
#define _RT_AMD64_UNWIND_H_

#include <stdint.h>

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
    ((void *)&RtGetUnwindCodeEntry((info), ((info)->CountOfCodes + 1) & ~1))
#define RtGetExceptionHandler(base, info) \
    ((void *)((base) + *(uint32_t *)RtGetLanguageSpecificData(info)))
#define RtGetChainedFunctionEntry(base, info) \
    ((RtRuntimeFunction *)((base) + *(uint32_t *)RtGetLanguageSpecificData(info)))
#define RtGetExceptionDataPtr(info) ((void *)((uint32_t *)RtGetLanguageSpecificData(info) + 1))

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

typedef struct RtDispatcherContext {
    uint64_t ControlPc;
    uint64_t ImageBase;
    RtRuntimeFunction *FunctionEntry;
    uint64_t EstablisherFrame;
    uint64_t TargetIp;
    RtContext *ContextRecord;
    RtExceptionRoutine LanguageHandler;
    void *HandlerData;
    uint32_t ScopeIndex;
} RtDispatcherContext;

typedef struct {
    uint32_t Count;
    struct {
        uint32_t BeginAddress;
        uint32_t EndAddress;
        uint32_t HandlerAddress;
        uint32_t JumpTarget;
    } ScopeRecord[];
} RtScopeTable;

#endif /* _RT_AMD64_UNWIND_H_ */
