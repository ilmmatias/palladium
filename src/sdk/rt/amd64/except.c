/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <hal.h>
#include <rt/except.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unwinds the given frame, dispatching the given exception (and calling
 *     __except{} handlers along the way).
 *
 * PARAMETERS:
 *     ExceptionRecord - Data about the exception.
 *     ContextRecord - Pointer where the unwind context is located.
 *
 * RETURN VALUE:
 *     1 if someone handled the exception (and we should continue), 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int RtDispatchException(RtExceptionRecord *ExceptionRecord, RtContext *ContextRecord) {
    RtContext UnwindContext;
    RtContext ActiveContext;
    memcpy(&ActiveContext, ContextRecord, sizeof(RtContext));

    /* We need this to validate the establisher frame. */
    uint64_t StackBase = (uint64_t)HalGetCurrentProcessor()->CurrentThread->Stack;
    uint64_t StackLimit = StackBase + KE_STACK_SIZE;
    uint64_t ControlPc = (uint64_t)ExceptionRecord->ExceptionAddress;

    while (ActiveContext.Rsp >= StackBase && ActiveContext.Rsp < StackLimit) {
        uint64_t ImageBase = RtLookupImageBase(ControlPc);
        if (!ImageBase) {
            break;
        }

        RtRuntimeFunction *FunctionEntry = RtLookupFunctionEntry(ImageBase, ControlPc);
        if (!FunctionEntry) {
            /* Leaf function, manually skip this frame and keep on looping (as the code below
               assumes we're not one, even though RtVirtualUnwind doesn't). */
            ActiveContext.Rip = *(uint64_t *)ActiveContext.Rsp;
            ActiveContext.Rsp += sizeof(uint64_t);
            ControlPc = ActiveContext.Rip;
            continue;
        }

        memcpy(&UnwindContext, &ActiveContext, sizeof(RtContext));

        /* As long as we pass EstablisherFrame to VirtualUnwind, it should always be filled up. */
        uint64_t EstablisherFrame;
        void *HandlerData;
        RtExceptionRoutine LanguageHandler = RtVirtualUnwind(
            RT_UNW_FLAG_EHANDLER,
            ImageBase,
            ControlPc,
            FunctionEntry,
            &UnwindContext,
            &HandlerData,
            &EstablisherFrame);

        /* TODO: Raise a bad stack exception. */
        if ((EstablisherFrame & 7) || EstablisherFrame < StackBase ||
            EstablisherFrame >= StackLimit) {
            while (1)
                ;
        }

        if (LanguageHandler) {
            RtDispatcherContext DispatcherContext;
            DispatcherContext.ControlPc = ControlPc;
            DispatcherContext.ImageBase = ImageBase;
            DispatcherContext.FunctionEntry = FunctionEntry;
            DispatcherContext.EstablisherFrame = EstablisherFrame;
            DispatcherContext.ScopeIndex = 0;

            do {
                DispatcherContext.ContextRecord = &ActiveContext;
                DispatcherContext.LanguageHandler = LanguageHandler;
                DispatcherContext.HandlerData = HandlerData;

                int Disposition = LanguageHandler(
                    ExceptionRecord, EstablisherFrame, &ActiveContext, &DispatcherContext);

                /* Don't propagate those to the next iterations (unless requested/necessary). */
                ExceptionRecord->ExceptionFlags &=
                    ~(RT_EXC_FLAG_TARGET_UNWIND | RT_EXC_COLLIDED_UNWIND);

                switch (Disposition) {
                    case RT_EXC_CONTINUE_EXECUTION: {
                        return 1;
                    }

                    case RT_EXC_CONTINUE_SEARCH: {
                        break;
                    }

                    case RT_EXC_COLLIDED_UNWIND: {
                        /* Nested unwind, we need to copy all info back, and unwind again (this
                           time not saving/modifying anything but the active context). */
                        EstablisherFrame = DispatcherContext.EstablisherFrame;
                        LanguageHandler = DispatcherContext.LanguageHandler;
                        HandlerData = DispatcherContext.HandlerData;

                        memcpy(&UnwindContext, &ActiveContext, sizeof(RtContext));
                        RtVirtualUnwind(
                            RT_UNW_FLAG_NHANDLER,
                            DispatcherContext.ImageBase,
                            DispatcherContext.ControlPc,
                            DispatcherContext.FunctionEntry,
                            &UnwindContext,
                            NULL,
                            NULL);

                        ExceptionRecord->ExceptionFlags |= RT_EXC_FLAG_COLLIDED_UNWIND;
                        break;
                    }

                    default: {
                        /* TODO: Raise a bad disposition exception. */
                        while (1)
                            ;
                    }
                }
            } while (ExceptionRecord->ExceptionFlags & RT_EXC_FLAG_COLLIDED_UNWIND);

            /* TODO: Raise a bad stack exception. */
            if ((EstablisherFrame & 7) || EstablisherFrame < StackBase ||
                EstablisherFrame >= StackLimit) {
                while (1)
                    ;
            }
        }

        /* Descriptions online tell us to swap this with the unwind context, but the start
           of the loop trashes the unwind context?????? */
        memcpy(&ActiveContext, &UnwindContext, sizeof(RtContext));
        ControlPc = ActiveContext.Rip;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the C-specific SEH handling code (for when something
 *     throws/raises an exception).
 *
 * PARAMETERS:
 *     ExceptionRecord - Data about the current exception.
 *     EstablisherFrame - Base of the fixed stack allocation for the frame.
 *     ContextRecord - Current unwind context frame.
 *     DispatcherContext - Information about the exception dispatch.
 *
 * RETURN VALUE:
 *     1 on RT_EXC_CONTINUE_EXECUTION (for EHANDLER), 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __C_specific_handler(
    RtExceptionRecord *ExceptionRecord,
    uint64_t EstablisherFrame,
    RtContext *ContextRecord,
    RtDispatcherContext *DispatcherContext) {
    uint64_t ControlOffset = DispatcherContext->ControlPc - DispatcherContext->ImageBase;
    uint64_t TargetOffset = DispatcherContext->TargetIp - DispatcherContext->ImageBase;
    RtScopeTable *ScopeTable = DispatcherContext->HandlerData;

    /* Handle indirect scope tables (the 32nd bit of the count should tell us if it is one.) */
    if (ScopeTable->Count & 0x80000000) {
        ScopeTable =
            (RtScopeTable *)(DispatcherContext->ImageBase + (ScopeTable->Count & 0x7FFFFFFF));
    }

    RtExceptionPointers ExceptionPointers;
    ExceptionPointers.ExceptionRecord = ExceptionRecord;
    ExceptionPointers.ContextRecord = ContextRecord;

    /* Exceptions want to execute any __except{} they can, while normal unwinds want to execute
      __finally{}. */
    if (ExceptionRecord->ExceptionFlags & RT_EXC_FLAG_UNWIND) {
        for (; DispatcherContext->ScopeIndex < ScopeTable->Count; DispatcherContext->ScopeIndex++) {
            if (ControlOffset <
                    ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].BeginAddress ||
                ControlOffset >=
                    ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].EndAddress) {
                continue;
            }

            /* On TARGET_UNWIND, we want to check this record against the target too. */
            if ((ExceptionRecord->ExceptionFlags & RT_EXC_FLAG_TARGET_UNWIND) &&
                TargetOffset >=
                    ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].BeginAddress &&
                TargetOffset < ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].EndAddress) {
                return RT_EXC_CONTINUE_SEARCH;
            }

            /* Execute any __finally{} handlers. */
            if (!ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].JumpTarget) {
                ((RtTerminationHandler)(DispatcherContext->ImageBase +
                                        ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex]
                                            .HandlerAddress))(1, EstablisherFrame);
            } else if (
                ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].JumpTarget == TargetOffset) {
                return RT_EXC_CONTINUE_SEARCH;
            }
        }
    } else {
        for (; DispatcherContext->ScopeIndex < ScopeTable->Count; DispatcherContext->ScopeIndex++) {
            if (ControlOffset <
                    ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].BeginAddress ||
                ControlOffset >=
                    ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].EndAddress ||
                !ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].JumpTarget) {
                continue;
            }

            /* EXC_EXECUTE_HANDLER (-1) is our fixed value for "don't call the filter". */
            int FilterResult =
                ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].HandlerAddress ==
                (uint32_t)RT_EXC_EXECUTE_HANDLER;
            if (!FilterResult) {
                FilterResult =
                    ((RtExceptionFilter)(DispatcherContext->ImageBase +
                                         ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex]
                                             .HandlerAddress))(
                        &ExceptionPointers, EstablisherFrame);
            }

            if (FilterResult == RT_EXC_CONTINUE_EXECUTION) {
                return RT_EXC_CONTINUE_EXECUTION;
            } else if (FilterResult == RT_EXC_EXECUTE_HANDLER) {
                RtUnwind(
                    (void *)EstablisherFrame,
                    (void *)(DispatcherContext->ImageBase +
                             ScopeTable->ScopeRecord[DispatcherContext->ScopeIndex].JumpTarget),
                    ExceptionRecord,
                    (void *)(size_t)ExceptionRecord->ExceptionCode);
            }
        }
    }

    return RT_EXC_CONTINUE_SEARCH;
}
