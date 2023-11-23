/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <hal.h>
#include <pe.h>
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
    1,
    /* RT_UWOP_SPARE_CODE */
    2,
    /* RT_UWOP_SAVE_XMM128 */
    2,
    /* RT_UWOP_SAVE_XMM128_FAR */
    3,
    /* RT_UWOP_PUSH_MACHFRAME */
    1,
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the value of the Nth register.
 *
 * PARAMETERS:
 *     State - Register state.
 *     Reg - Which GPR we want to read.
 *
 * RETURN VALUE:
 *     The value of the GPR.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t GetReg(HalRegisterState *State, int Reg) {
    /* TODO: We should really just reorganize HalRegisterState so that we can just do a lookup
       (like State->Gpr[Reg]). */
    switch (Reg) {
        case 0:
            return State->Rax;
        case 1:
            return State->Rcx;
        case 2:
            return State->Rdx;
        case 3:
            return State->Rbx;
        case 4:
            return State->Rsp;
        case 5:
            return State->Rbp;
        case 6:
            return State->Rsi;
        case 7:
            return State->Rdi;
        case 8:
            return State->R8;
        case 9:
            return State->R9;
        case 10:
            return State->R10;
        case 11:
            return State->R11;
        case 12:
            return State->R12;
        case 13:
            return State->R13;
        case 14:
            return State->R14;
        default:
            return State->R15;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function stores a new value into the Nth register.
 *
 * PARAMETERS:
 *     State - Register state.
 *     Reg - Which GPR we want to write into.
 *     Value - What we want to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void SetReg(HalRegisterState *State, int Reg, uint64_t Value) {
    /* TODO: We should really just reorganize HalRegisterState so that we can just do a lookup
       (like State->Gpr[Reg] = Value). */
    switch (Reg) {
        case 0:
            State->Rax = Value;
            break;
        case 1:
            State->Rcx = Value;
            break;
        case 2:
            State->Rdx = Value;
            break;
        case 3:
            State->Rbx = Value;
            break;
        case 4:
            State->Rsp = Value;
            break;
        case 5:
            State->Rbp = Value;
            break;
        case 6:
            State->Rsi = Value;
            break;
        case 7:
            State->Rdi = Value;
            break;
        case 8:
            State->R8 = Value;
            break;
        case 9:
            State->R9 = Value;
            break;
        case 10:
            State->R10 = Value;
            break;
        case 11:
            State->R11 = Value;
            break;
        case 12:
            State->R12 = Value;
            break;
        case 13:
            State->R13 = Value;
            break;
        case 14:
            State->R14 = Value;
            break;
        case 15:
            State->R15 = Value;
            break;
    }
}

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
 *     Context - Register state the frame was being executed in.
 *     UnwindInfo - Pointer to the function's unwind/exception data.
 *     OpIndex - Where to start parsing the opcodes.
 *
 * RETURN VALUE:
 *     1 if we encountered a MACHFRAME (we're fully done), 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ProcessUnwindOps(HalRegisterState *Context, RtUnwindInfo *UnwindInfo, int OpIndex) {
    while (OpIndex < UnwindInfo->CountOfCodes) {
        RtUnwindCode *Code = &UnwindInfo->UnwindCode[OpIndex];

        switch (Code->UnwindOp) {
            case RT_UWOP_PUSH_NONVOL:
                SetReg(Context, Code->OpInfo, *(uint64_t *)Context->Rsp);
                Context->Rsp += sizeof(uint64_t);
                OpIndex += 1;
                break;

            case RT_UWOP_ALLOC_LARGE:
                if (Code->OpInfo) {
                    Context->Rsp += *(uint32_t *)(&UnwindInfo->UnwindCode[OpIndex + 1]);
                    OpIndex += 3;
                } else {
                    Context->Rsp += UnwindInfo->UnwindCode[OpIndex + 1].FrameOffset * 8;
                    OpIndex += 2;
                }

                break;

            case RT_UWOP_ALLOC_SMALL:
                Context->Rsp += (Code->OpInfo + 1) * 8;
                OpIndex += 1;
                break;

            case RT_UWOP_SET_FPREG:
                Context->Rsp = GetReg(Context, Code->OpInfo) - Code->FrameOffset * 16;
                OpIndex += 1;
                break;

            case RT_UWOP_SAVE_NONVOL:
                SetReg(
                    Context,
                    Code->OpInfo,
                    *(uint64_t *)(Context->Rsp + UnwindInfo->UnwindCode[OpIndex + 1].FrameOffset));
                OpIndex += 2;
                break;

            case RT_UWOP_SAVE_NONVOL_FAR:
                SetReg(
                    Context,
                    Code->OpInfo,
                    *(uint64_t *)(Context->Rsp +
                                  *(uint32_t *)&UnwindInfo->UnwindCode[OpIndex + 1]));
                OpIndex += 3;
                break;

            case RT_UWOP_EPILOG:
                OpIndex += 1;
                break;

            case RT_UWOP_SPARE_CODE:
                OpIndex += 2;
                break;

            /* We currently ignore/don't use XMM registers on the kernel, but once we start
               handling that, TODO: restore XMM context here too! */
            case RT_UWOP_SAVE_XMM128:
                OpIndex += 2;
                break;

            case RT_UWOP_SAVE_XMM128_FAR:
                OpIndex += 3;
                break;

            case RT_UWOP_PUSH_MACHFRAME:
                Context->Rsp += Code->OpInfo * sizeof(uint64_t);
                Context->Rip = *(uint64_t *)Context->Rsp;
                Context->Rsp = *(uint64_t *)(Context->Rsp + 24);

                /* MACHFRAME should mean we're done (no matter if we have more ops ahead, though
                   we should have none). */
                return 1;
        }
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for the unwind function entry the given address belongs to.
 *
 * PARAMETERS:
 *     Address - Which address we're resolving.
 *
 * RETURN VALUE:
 *     Either the entry, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
RtRuntimeFunction *RtLookupFunctionEntry(uint64_t Address) {
    uint64_t ImageBase = RtLookupImageBase(Address);
    if (!ImageBase) {
        return NULL;
    }

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
    while (Start <= End) {
        uint32_t Mid = (Start + End) / 2;
        RtRuntimeFunction *Entry = &Table[Mid];

        if (Address < Entry->BeginAddress) {
            End = Mid;
        } else if (Address > Entry->EndAddress) {
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
 *     Context - Register state the frame was being executed in.
 *     LanguageHandler - Output; Language-specific handler.
 *     LanguageData - Output; Language-specific handler data.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int RtUnwindFrame(
    HalRegisterState *Context,
    RtExceptionRoutine *LanguageHandler,
    void **LanguageData) {
    *LanguageHandler = NULL;
    *LanguageData = NULL;

    uint64_t ImageBase = RtLookupImageBase(Context->Rip);
    if (!ImageBase) {
        return 0;
    }

    RtRuntimeFunction *FunctionEntry = RtLookupFunctionEntry(Context->Rip);
    if (!FunctionEntry) {
        /* Leaf function, no SEH data, RIP should be currently pushed into the stack (and nothing
           else). */
        Context->Rip = *(uint64_t *)Context->Rsp;
        Context->Rsp += sizeof(uint64_t);
        return 1;
    }

    uint64_t Offset = Context->Rip - FunctionEntry->BeginAddress;
    RtUnwindInfo *UnwindInfo = (RtUnwindInfo *)(ImageBase + FunctionEntry->UnwindData);

    /* Microsoft's docs says that for epilogs and prologs, there will never be any handler;
       Prologs get handled like usual (ProcessUnwindOps), epilogs get handled by manually
       executing all their instructions. */
    do {
        if (Offset < UnwindInfo->SizeOfProlog) {
            break;
        }

        HalRegisterState LocalContext;
        memcpy(&LocalContext, Context, sizeof(HalRegisterState));

        /* Epilogs are allowed an ADD RSP, CONSTANT or a LEA RSP, CONSTANT[FPREG] at the
           start. */
        uint32_t Instr = *(uint32_t *)LocalContext.Rip;
        if ((Instr & 0xFFFFFF) == 0xC48348) {
            /* ADD RSP, IMM8 */
            LocalContext.Rsp += LocalContext.Rip >> 24;
            LocalContext.Rip += 4;
        } else if ((Instr & 0xFFFFFF) == 0xC48148) {
            /* ADD RSP, IMM32 */
            LocalContext.Rsp += *(uint32_t *)(LocalContext.Rip + 3);
            LocalContext.Rip += 7;
        } else if ((Instr & 0x38FFFE) == 0x208D48) {
            /* LEA RSP, M */
            LocalContext.Rsp = GetReg(&LocalContext, ((Instr >> 16) & 0x07) + (Instr & 0x01) * 8);
            switch ((Instr >> 22) & 0x03) {
                /* [R] */
                case 0:
                    LocalContext.Rip += 3;
                    break;
                /* [R + imm8] */
                case 1:
                    LocalContext.Rsp += (int8_t)(Instr >> 24);
                    LocalContext.Rip += 4;
                    break;
                /* [R + imm32] */
                case 2:
                    LocalContext.Rsp += *(int32_t *)(LocalContext.Rip + 3);
                    LocalContext.Rip += 7;
                    break;
            }
        }

        /* Now, we should have N register pops, anything other than a pop (or a return/jump),
           means this isn't an epilog. */
        uint64_t FunctionLimit = ImageBase + FunctionEntry->EndAddress;
        while (LocalContext.Rip < FunctionLimit - 1) {
            Instr = *(uint32_t *)LocalContext.Rip;

            if ((Instr & 0xF8) == 0x58) {
                /* POP REG */
                SetReg(&LocalContext, Instr & 0x07, *(uint64_t *)LocalContext.Rsp);
                LocalContext.Rsp += sizeof(uint64_t);
                LocalContext.Rip += 1;
            } else if ((Instr & 0xF8FB) == 0x5841) {
                /* REX.B POP REG */
                SetReg(&LocalContext, ((Instr >> 8) & 0x07) + 8, *(uint64_t *)LocalContext.Rsp);
                LocalContext.Rsp += sizeof(uint64_t);
                LocalContext.Rip += 2;
            } else {
                break;
            }
        }

        if (LocalContext.Rip >= FunctionLimit - 1) {
            /* We should have at least 1 byte left (for a RET). */
            break;
        }

        Instr = *(uint8_t *)LocalContext.Rip;
        if (Instr != 0xC2 && Instr != 0xC3) {
            /* This isn't a single byte ret, we need at least 2 bytes left. */
            if (LocalContext.Rip >= FunctionLimit - 2) {
                break;
            }
        }

        if ((Instr & 0xFF) == 0xEB || (Instr & 0xFF) == 0xE9) {
            /* JMP IMM
               Both branches into another function and into tail recursion means this is an
               epilog. */

            uint64_t Target = LocalContext.Rip - ImageBase;
            if ((Instr & 0xFF) == 0xEB) {
                /* JMP IMM8 */
                Target += (int8_t)(Instr >> 8) + 2;
            } else if (LocalContext.Rip >= FunctionLimit - 5) {
                break;
            } else {
                /* JMP IMM32 */
                Target += *(int32_t *)(LocalContext.Rip + 1) + 5;
            }

            if ((Target > FunctionEntry->BeginAddress && Target < FunctionEntry->EndAddress) ||
                (Target == FunctionEntry->BeginAddress &&
                 (UnwindInfo->Flags & RT_UNW_FLAG_CHAININFO))) {
                break;
            }
        } else if ((Instr & 0xFF48) == 0xFF48) {
            /* Possibly JMP m16:64, we need to check the modrm to be sure. */
            if (LocalContext.Rip >= FunctionLimit - 3) {
                break;
            }

            Instr = *(uint8_t *)(LocalContext.Rip + 2);
            if ((Instr & 0x38) != 0x28) {
                break;
            }
        } else if (
            Instr != 0xC2 && Instr != 0xC3 && Instr != 0xC3F3 && (Instr & 0x20FF) != 0x20FF) {
            break;
        }

        /* We're RETing anyways, even on jumps (remember that we're trying to backtrack, not go
           forward). */
        LocalContext.Rip = *(uint64_t *)LocalContext.Rsp;
        LocalContext.Rsp += sizeof(uint64_t);

        memcpy(Context, &LocalContext, sizeof(HalRegisterState));
        return 1;
    } while (0);

    /* Skip any ops smaller than the current offset (they don't need to be executed/unwinded). */
    int OpIndex = 0;
    while (OpIndex < UnwindInfo->CountOfCodes &&
           UnwindInfo->UnwindCode[OpIndex].CodeOffset > Offset) {
        OpIndex += GetUnwindOpSlots(&UnwindInfo->UnwindCode[OpIndex]);
    }

    /* Process any remaining unwind ops. */
    int Done = ProcessUnwindOps(Context, UnwindInfo, OpIndex);

    /* Process the chained info if this function entry has it. */
    while (!Done && (UnwindInfo->Flags & RT_UNW_FLAG_CHAININFO)) {
        UnwindInfo = (RtUnwindInfo *)(ImageBase +
                                      RtGetChainedFunctionEntry(ImageBase, UnwindInfo)->UnwindData);
        Done = ProcessUnwindOps(Context, UnwindInfo, 0);
    }

    /* Pop RIP (fully restoring the frame) if we haven't done that yet. */
    if (!Done && Context->Rsp) {
        Context->Rip = *(uint64_t *)Context->Rsp;
        Context->Rsp += sizeof(uint64_t);
    }

    /* Return the handler/callback if we have one, or NULL if we have nothing more. */
    if (LanguageHandler && Offset > UnwindInfo->SizeOfProlog &&
        (UnwindInfo->Flags & (RT_UNW_FLAG_EHANDLER | RT_UNW_FLAG_UHANDLER))) {
        uint64_t *LanguageSpecificData = RtGetLanguageSpecificData(UnwindInfo);
        *LanguageHandler = (RtExceptionRoutine)(ImageBase + *LanguageSpecificData);
        *LanguageData = LanguageSpecificData + 1;
    }

    return 1;
}
