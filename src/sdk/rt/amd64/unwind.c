/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <os/pe.h>
#include <rt/except.h>
#include <string.h>

static const int UnwindOpSlots[] = {
    /* RT_UWOP_PUSH_NONVOL */
    1,
    /* RT_UWOP_ALLOC_LARGE */
    2,
    /* RT_UWOP_ALLOC_SMALL */
    1,
    /* RT_UWOP_SET_FPREG */
    1,
    /* RT_UWOP_SAVE_NONVOL */
    2,
    /* RT_UWOP_SAVE_NONVOL_FAR */
    3,
    /* RT_UWOP_EPILOG */
    2,
    /* RT_UWOP_SPARE_CODE */
    3,
    /* RT_UWOP_SAVE_XMM128 */
    2,
    /* RT_UWOP_SAVE_XMM128_FAR */
    3,
    /* RT_UWOP_PUSH_MACHFRAME */
    1,
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets how many slots the given UWOP takes.
 *
 * PARAMETERS:
 *     Code - Which op we're parsing/ignoring.
 *
 * RETURN VALUE:
 *     How many slots the op takes.
 *-----------------------------------------------------------------------------------------------*/
static int GetUnwindOpSlots(RtUnwindCode *Code) {
    if (Code->UnwindOp == RT_UWOP_ALLOC_LARGE && Code->OpInfo) {
        return 3;
    } else {
        return UnwindOpSlots[Code->UnwindOp];
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes all unwind opcodes in the given frame until we run out of them.
 *
 * PARAMETERS:
 *     ContextRecord - Register state the frame was being executed in.
 *     UnwindInfo - Pointer to the function's unwind/exception data.
 *     FrameBase - Stack frame address of the current function.
 *     OpIndex - Where to start parsing the opcodes.
 *
 * RETURN VALUE:
 *     true if we encountered a MACHFRAME, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool ProcessUnwindOps(
    RtContext *ContextRecord,
    RtUnwindInfo *UnwindInfo,
    uint64_t FrameBase,
    int OpIndex) {
    while (OpIndex < UnwindInfo->CountOfCodes) {
        RtUnwindCode *Code = &UnwindInfo->UnwindCode[OpIndex];

        switch (Code->UnwindOp) {
            case RT_UWOP_PUSH_NONVOL:
                ContextRecord->Gpr[Code->OpInfo] = *(uint64_t *)ContextRecord->Rsp;
                ContextRecord->Rsp += sizeof(uint64_t);
                OpIndex += 1;
                break;

            case RT_UWOP_ALLOC_LARGE:
                if (Code->OpInfo) {
                    ContextRecord->Rsp += *(uint32_t *)(&UnwindInfo->UnwindCode[OpIndex + 1]);
                    OpIndex += 3;
                } else {
                    ContextRecord->Rsp += UnwindInfo->UnwindCode[OpIndex + 1].FrameOffset * 8;
                    OpIndex += 2;
                }

                break;

            case RT_UWOP_ALLOC_SMALL:
                ContextRecord->Rsp += (Code->OpInfo + 1) * 8;
                OpIndex += 1;
                break;

            case RT_UWOP_SET_FPREG:
                ContextRecord->Rsp =
                    ContextRecord->Gpr[UnwindInfo->FrameRegister] - UnwindInfo->FrameOffset * 16;
                OpIndex += 1;
                break;

            case RT_UWOP_SAVE_NONVOL:
                ContextRecord->Gpr[Code->OpInfo] =
                    *(uint64_t *)(FrameBase + UnwindInfo->UnwindCode[OpIndex + 1].FrameOffset * 8);
                OpIndex += 2;
                break;

            case RT_UWOP_SAVE_NONVOL_FAR:
                ContextRecord->Gpr[Code->OpInfo] =
                    *(uint64_t *)(FrameBase + *(uint32_t *)&UnwindInfo->UnwindCode[OpIndex + 1]);
                OpIndex += 3;
                break;

            case RT_UWOP_EPILOG:
                OpIndex += 2;
                break;

            case RT_UWOP_SPARE_CODE:
                OpIndex += 3;
                break;

            case RT_UWOP_SAVE_XMM128:
                ContextRecord->Xmm[Code->OpInfo] =
                    *(__m128 *)(FrameBase + UnwindInfo->UnwindCode[OpIndex + 1].FrameOffset * 16);
                OpIndex += 2;
                break;

            case RT_UWOP_SAVE_XMM128_FAR:
                ContextRecord->Xmm[Code->OpInfo] =
                    *(__m128 *)(FrameBase + *(uint32_t *)&UnwindInfo->UnwindCode[OpIndex + 1]);
                OpIndex += 3;
                break;

            case RT_UWOP_PUSH_MACHFRAME:
                ContextRecord->Rsp += Code->OpInfo * sizeof(uint64_t);
                ContextRecord->Rip = *(uint64_t *)ContextRecord->Rsp;
                ContextRecord->Rsp = *(uint64_t *)(ContextRecord->Rsp + 24);
                return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for the unwind function entry the given address belongs to.
 *
 * PARAMETERS:
 *     ImageBase - Start of the EXE/DLL the address is located in.
 *     Address - Which address we're resolving.
 *
 * RETURN VALUE:
 *     Either the entry, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
RtRuntimeFunction *RtLookupFunctionEntry(uint64_t ImageBase, uint64_t Address) {
    /* All the search inside the image is relative to the image base. */
    Address -= ImageBase;

    /* We always compile everything (even the kernel) with SEH enabled, but still, validate the
       exception table exists (maybe glitchy compiler if it doesn't?) */
    PeHeader *Header = (PeHeader *)(ImageBase + *(uint16_t *)(ImageBase + 0x3C));
    RtRuntimeFunction *Table =
        (RtRuntimeFunction *)(ImageBase + Header->DataDirectories.ExceptionTable.VirtualAddress);
    uint32_t Size = Header->DataDirectories.ExceptionTable.Size / sizeof(RtRuntimeFunction);
    if (!Size) {
        return NULL;
    }

    /* The table should be sorted, so we can do a binary search. */
    uint32_t Start = 0;
    uint32_t End = Size;
    while (Start < End) {
        uint32_t Mid = (Start + End) / 2;
        RtRuntimeFunction *Entry = &Table[Mid];

        if (Address < Entry->BeginAddress) {
            End = Mid;
        } else if (Address >= Entry->EndAddress) {
            Start = Mid + 1;
        } else {
            return Entry;
        }
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does a single unwind step, restoring the register state to how it was before
 *     the current frame.
 *
 * PARAMETERS:
 *     HandlerType - Which language handler we're searching for (if any).
 *     ImageBase - Start of the EXE/DLL the instruction pointer is located in.
 *     ControlPc - Value of the instruction pointer.
 *     FunctionEntry - Exception/unwind data about the function.
 *     ContextRecord - Register state the function was being executed in.
 *     HandlerData - Output; Language-specific handler data.
 *     EstablisherFrame - Output; Base of the fixed stack allocation for this function.
 *
 * RETURN VALUE:
 *     Language handler function, if one of the specified handler type exists, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
RtExceptionRoutine RtVirtualUnwind(
    int HandlerType,
    uint64_t ImageBase,
    uint64_t ControlPc,
    RtRuntimeFunction *FunctionEntry,
    RtContext *ContextRecord,
    void **HandlerData,
    uint64_t *EstablisherFrame) {
    if (!FunctionEntry) {
        /* Leaf function, no SEH data, RIP should be currently pushed into the stack (and nothing
           else). */
        ContextRecord->Rip = *(uint64_t *)ContextRecord->Rsp;
        ContextRecord->Rsp += sizeof(uint64_t);
        return NULL;
    }

    uint64_t Offset = ControlPc - ImageBase - FunctionEntry->BeginAddress;
    RtUnwindInfo *UnwindInfo = (RtUnwindInfo *)(ImageBase + FunctionEntry->UnwindData);
    uint64_t FrameBase = 0;

    do {
        /* If we have no frame pointer, the frame pointer is RSP. */
        if (!UnwindInfo->FrameRegister) {
            FrameBase = ContextRecord->Rsp;
            break;
        }

        /* FrameRegister existing means we have SET_FPREG somewhere in the prolog; If we went
           past it (or at least reached it), we're free to read the frame register. */
        bool HasSetFpreg =
            Offset >= UnwindInfo->SizeOfProlog || (UnwindInfo->Flags & RT_UNW_FLAG_CHAININFO);

        int OpIndex = 0;
        while (!HasSetFpreg && OpIndex < UnwindInfo->CountOfCodes) {
            RtUnwindCode *UnwindCode = &UnwindInfo->UnwindCode[OpIndex];

            if (UnwindCode->CodeOffset > Offset || UnwindCode->UnwindOp != RT_UWOP_SET_FPREG) {
                OpIndex += GetUnwindOpSlots(UnwindCode);
                continue;
            }

            HasSetFpreg = true;
        }

        if (HasSetFpreg) {
            FrameBase =
                ContextRecord->Gpr[UnwindInfo->FrameRegister] - UnwindInfo->FrameOffset * 16;
        } else {
            FrameBase = ContextRecord->Rsp;
        }
    } while (false);

    if (EstablisherFrame) {
        *EstablisherFrame = FrameBase;
    }

    do {
        if (UnwindInfo->Version >= 2) {
            break;
        }

        /* For versions<2, we're forced to manually detect the epilogue sequence and simulate it. */
        if (Offset < UnwindInfo->SizeOfProlog) {
            break;
        }

        RtContext LocalContext;
        memcpy(&LocalContext, ContextRecord, sizeof(RtContext));

        /* Epilogs are allowed an ADD RSP, CONSTANT or a LEA RSP, CONSTANT[FPREG] at the
           start. */
        uint64_t InstrPtr = ControlPc;
        uint32_t Instr = *(uint32_t *)InstrPtr;
        if ((Instr & 0xFFFFFF) == 0xC48348) {
            /* ADD RSP, IMM8 */
            LocalContext.Rsp += Instr >> 24;
            InstrPtr += 4;
        } else if ((Instr & 0xFFFFFF) == 0xC48148) {
            /* ADD RSP, IMM32 */
            LocalContext.Rsp += *(uint32_t *)(InstrPtr + 3);
            InstrPtr += 7;
        } else if ((Instr & 0x38FFFE) == 0x208D48) {
            /* LEA RSP, M */
            LocalContext.Rsp = LocalContext.Gpr[((Instr >> 16) & 0x07) + (Instr & 0x01) * 8];
            switch ((Instr >> 22) & 0x03) {
                /* [R] */
                case 0:
                    InstrPtr += 3;
                    break;
                /* [R + imm8] */
                case 1:
                    LocalContext.Rsp += (int8_t)(Instr >> 24);
                    InstrPtr += 4;
                    break;
                /* [R + imm32] */
                case 2:
                    LocalContext.Rsp += *(int32_t *)(InstrPtr + 3);
                    InstrPtr += 7;
                    break;
            }
        }

        /* Now, we should have N register pops, anything other than a pop (or a return/jump),
           means this isn't an epilog. */
        while (true) {
            Instr = *(uint32_t *)InstrPtr;
            if ((Instr & 0xF8) == 0x58) {
                /* POP REG */
                LocalContext.Gpr[Instr & 0x07] = *(uint64_t *)LocalContext.Rsp;
                LocalContext.Rsp += sizeof(uint64_t);
                InstrPtr += 1;
            } else if ((Instr & 0xF8FB) == 0x5841) {
                /* REX.B POP REG */
                LocalContext.Gpr[((Instr >> 8) & 0x07) + 8] = *(uint64_t *)LocalContext.Rsp;
                LocalContext.Rsp += sizeof(uint64_t);
                InstrPtr += 2;
            } else {
                break;
            }
        }

        /* A REPNE prefix (which is actually BND prefix) is allowed before the return/jump
         * instruction. */
        Instr = *(uint32_t *)InstrPtr;
        if ((Instr & 0xFF) == 0xF2) {
            InstrPtr += 1;
            Instr = *(uint32_t *)InstrPtr;
        }

        /* Now we should be followed by a jump or return to outside the current function. */
        if ((Instr & 0xFF) == 0xEB || (Instr & 0xFF) == 0xE9) {
            /* JMP IMM
               Both branches into another function and into tail recursion means this is an
               epilog. */

            uint64_t Target = InstrPtr - ImageBase;
            if ((Instr & 0xFF) == 0xEB) {
                /* JMP IMM8 */
                Target += (int8_t)(Instr >> 8) + 2;
            } else {
                /* JMP IMM32 */
                Target += *(int32_t *)(InstrPtr + 1) + 5;
            }

            /* TODO: This isn't enough to be sure this is a jump to outside this function; We should
             * take into consideration chained function entries too. */
            if ((Target > FunctionEntry->BeginAddress && Target <= FunctionEntry->EndAddress) ||
                (Target == FunctionEntry->BeginAddress &&
                 (UnwindInfo->Flags & RT_UNW_FLAG_CHAININFO))) {
                break;
            }
        } else if (
            (Instr & 0xFF) != 0xC2 && (Instr & 0xFF) != 0xC3 && (Instr & 0xFFFF) != 0xC3F3 &&
            (Instr & 0xFFFF) != 0x25FF && (Instr & 0x38FFF8) != 0x20FF48) {
            break;
        }

        /* We're RETing anyways, even on jumps (remember that we're trying to backtrack, not go
           forward). */
        LocalContext.Rip = *(uint64_t *)LocalContext.Rsp;
        LocalContext.Rsp += sizeof(uint64_t);

        memcpy(ContextRecord, &LocalContext, sizeof(RtContext));
        return NULL;
    } while (false);

    if (UnwindInfo->Version >= 2) {
        /* TODO: For versions>=2, we need to use UWOP_EPILOGUE. */
    }

    while (true) {
        Offset = ControlPc - ImageBase - FunctionEntry->BeginAddress;
        UnwindInfo = (RtUnwindInfo *)(ImageBase + FunctionEntry->UnwindData);

        /* Skip any ops smaller than the current offset (they don't need to be
         * executed/unwinded). */
        int OpIndex = 0;
        while (OpIndex < UnwindInfo->CountOfCodes &&
               UnwindInfo->UnwindCode[OpIndex].CodeOffset > Offset) {
            OpIndex += GetUnwindOpSlots(&UnwindInfo->UnwindCode[OpIndex]);
        }

        /* Process any remaining unwind ops. */
        bool HasMachineFrame = ProcessUnwindOps(ContextRecord, UnwindInfo, FrameBase, OpIndex);

        /* And update the function entry if we have more chained info. */
        if (UnwindInfo->Flags & RT_UNW_FLAG_CHAININFO) {
            FunctionEntry = RtGetChainedFunctionEntry(ImageBase, UnwindInfo);
        } else {
            /* Or pop RIP (fully restoring the frame) if we haven't done that yet. */
            if (!HasMachineFrame) {
                ContextRecord->Rip = *(uint64_t *)ContextRecord->Rsp;
                ContextRecord->Rsp += sizeof(uint64_t);
            }

            break;
        }
    }

    /* We want to return the handler/callback if we have one matching the request type. */
    if (Offset > UnwindInfo->SizeOfProlog &&
        (UnwindInfo->Flags & (HandlerType & (RT_UNW_FLAG_EHANDLER | RT_UNW_FLAG_UHANDLER)))) {
        *HandlerData = RtGetExceptionDataPtr(UnwindInfo);
        return RtGetExceptionHandler(ImageBase, UnwindInfo);
    } else {
        return NULL;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unwinds the given frame, calling any termination handlers along the way.
 *
 * PARAMETERS:
 *     TargetFrame - Which frame we should stop at.
 *     TargetIp - Which address we should start at after the unwind.
 *     ExceptionRecord - Data about the current exception; This will be filled with dummy info if
 *                       NULL.
 *     ReturnValue - What to be put in the return register before calling TargetIp.
 *
 * RETURN VALUE:
 *     Does not return (might actually crash/throw an access violation if there's no target
 *     frame).
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void
RtUnwind(void *TargetFrame, void *TargetIp, RtExceptionRecord *ExceptionRecord, void *ReturnValue) {
    RtContext UnwindContext;
    RtContext ActiveContext;
    RtSaveContext(&ActiveContext);

    /* We need this to validate the establisher frame. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    uint64_t StackBase = (uint64_t)Processor->StackBase;
    uint64_t StackLimit = (uint64_t)Processor->StackLimit;

    RtExceptionRecord DefaultExceptionRecord;
    if (!ExceptionRecord) {
        DefaultExceptionRecord.ExceptionCode = RT_EXC_UNWIND;
        DefaultExceptionRecord.ExceptionFlags = 0;
        DefaultExceptionRecord.ExceptionRecord = NULL;
        DefaultExceptionRecord.ExceptionAddress = (void *)ActiveContext.Rip;
        DefaultExceptionRecord.NumberOfParameters = 0;
        ExceptionRecord = &DefaultExceptionRecord;
    }

    ExceptionRecord->ExceptionFlags |= RT_EXC_FLAG_UNWIND;
    if (!TargetFrame) {
        /* This is somewhat dangerous! Do not start an exit unwind unless you know what
           you're doing, or this might crash! */
        ExceptionRecord->ExceptionFlags |= RT_EXC_FLAG_EXIT_UNWIND;
    }

    uint64_t EstablisherFrame;
    while (ActiveContext.Rsp >= StackBase && ActiveContext.Rsp < StackLimit) {
        uint64_t ControlPc = ActiveContext.Rip;
        uint64_t ImageBase = RtLookupImageBase(ControlPc);
        if (!ImageBase) {
            /* We've unwound past the limit of the stack, this is bad (but at least we
               didn't access anything invalid). */
            while (true)
                ;
        }

        RtRuntimeFunction *FunctionEntry = RtLookupFunctionEntry(ImageBase, ControlPc);
        if (!FunctionEntry) {
            /* Leaf function, manually skip this frame and keep on looping (as the code below
               assumes we're not one, even though RtVirtualUnwind doesn't). */
            ActiveContext.Rip = *(uint64_t *)ActiveContext.Rsp;
            ActiveContext.Rsp += sizeof(uint64_t);
            continue;
        }

        memcpy(&UnwindContext, &ActiveContext, sizeof(RtContext));

        /* As long as we pass EstablisherFrame to VirtualUnwind, it should always be filled up. */
        void *HandlerData;
        RtExceptionRoutine LanguageHandler = RtVirtualUnwind(
            RT_UNW_FLAG_UHANDLER,
            ImageBase,
            ControlPc,
            FunctionEntry,
            &UnwindContext,
            &HandlerData,
            &EstablisherFrame);

        /* TODO: Raise a bad stack exception. */
        if ((EstablisherFrame & 7) || EstablisherFrame < StackBase ||
            EstablisherFrame >= StackLimit ||
            (TargetFrame && (uint64_t)TargetFrame < EstablisherFrame)) {
            while (true)
                ;
        }

        if (LanguageHandler) {
            RtDispatcherContext DispatcherContext;
            DispatcherContext.ControlPc = ControlPc;
            DispatcherContext.ImageBase = ImageBase;
            DispatcherContext.FunctionEntry = FunctionEntry;
            DispatcherContext.EstablisherFrame = EstablisherFrame;
            DispatcherContext.TargetIp = (uint64_t)TargetIp;
            DispatcherContext.ScopeIndex = 0;

            do {
                if (EstablisherFrame == (uint64_t)TargetFrame) {
                    ExceptionRecord->ExceptionFlags |= RT_EXC_FLAG_TARGET_UNWIND;
                }

                ActiveContext.Rax = (uint64_t)ReturnValue;
                DispatcherContext.ContextRecord = &ActiveContext;
                DispatcherContext.LanguageHandler = LanguageHandler;
                DispatcherContext.HandlerData = HandlerData;

                int Disposition = LanguageHandler(
                    ExceptionRecord, EstablisherFrame, &ActiveContext, &DispatcherContext);

                /* Don't propagate those to the next iterations (unless requested/necessary). */
                ExceptionRecord->ExceptionFlags &=
                    ~(RT_EXC_FLAG_TARGET_UNWIND | RT_EXC_COLLIDED_UNWIND);

                switch (Disposition) {
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
                        while (true)
                            ;
                    }
                }
            } while (ExceptionRecord->ExceptionFlags & RT_EXC_FLAG_COLLIDED_UNWIND);

            /* TODO: Raise a bad stack exception. */
            if ((EstablisherFrame & 7) || EstablisherFrame < StackBase ||
                EstablisherFrame >= StackLimit ||
                (TargetFrame && (uint64_t)TargetFrame < EstablisherFrame)) {
                while (true)
                    ;
            }
        }

        if (EstablisherFrame == (uint64_t)TargetFrame) {
            break;
        }

        /* Descriptions online tell us to swap this with the unwind context, but the start
           of the loop trashes the unwind context?????? */
        memcpy(&ActiveContext, &UnwindContext, sizeof(RtContext));
    }

    if (EstablisherFrame != (uint64_t)TargetFrame) {
        /* TODO: Raise an exception here. */
        while (true)
            ;
    }

    ActiveContext.Rax = (uint64_t)ReturnValue;
    ActiveContext.Rip = (uint64_t)TargetIp;
    RtRestoreContext(&ActiveContext);
}
