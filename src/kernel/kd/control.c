/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/hal.h>
#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <kernel/ke.h>
#include <os/intrin.h>
#include <rt/except.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern bool HalpSmpInitializationComplete;

typedef struct {
    uint32_t Identifier;
    bool Enabled;
    uint8_t Original;
    uint64_t Address;
    uint64_t HitCount;
} KdpBreakpoint;

static KdpAmd64Context Contexts[KE_MAX_PROCESSORS];
static volatile uint64_t ContextGenerations[KE_MAX_PROCESSORS];
static KdpBreakpoint Breakpoints[KDP_MAX_BREAKPOINTS];
static uint32_t BreakpointCount;
static uint32_t NextBreakpointIdentifier = 1;
static volatile uint32_t TargetState = KDP_TARGET_BOOTING;
static volatile uint32_t StopOwner;
static volatile uint64_t StopGeneration;
static volatile uint64_t ReleaseGeneration;
static volatile bool LeaveStopLoop;
static uint32_t ActiveStopReason;
static uint32_t ActiveBreakpointIdentifier;
static HalInterruptFrame *ActiveInterruptFrame;
static HalExceptionFrame *ActiveExceptionFrame;
static bool ManualStopArmed;
static uint32_t PendingManualStopReason = KDP_STOP_REASON_MANUAL;
static bool AutomaticContinue;
static bool InternalStepArmed;
static bool InternalStepReportsStop;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function serializes the two amd64 exception frames into the debugger wire context.
 *
 * PARAMETERS:
 *     Context - Destination debugger context.
 *     InterruptFrame - Volatile register and machine-frame state.
 *     ExceptionFrame - Nonvolatile register state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void CaptureContext(
    KdpAmd64Context *Context,
    HalInterruptFrame *InterruptFrame,
    HalExceptionFrame *ExceptionFrame) {
    memset(Context, 0, sizeof(*Context));
    Context->Size = sizeof(*Context);
    Context->Architecture = KDP_PROTOCOL_ARCH_AMD64;
    Context->Version = KDP_PROTOCOL_CONTEXT_VERSION;
    Context->ValidFields = KDP_CONTEXT_VALID_GPR | KDP_CONTEXT_VALID_INSTRUCTION |
                           KDP_CONTEXT_VALID_SEGMENTS | KDP_CONTEXT_VALID_EXCEPTION |
                           KDP_CONTEXT_VALID_XMM;
    Context->Gpr[0] = InterruptFrame->Rax;
    Context->Gpr[1] = InterruptFrame->Rcx;
    Context->Gpr[2] = InterruptFrame->Rdx;
    Context->Gpr[3] = ExceptionFrame->Rbx;
    Context->Gpr[4] = InterruptFrame->Rsp;
    Context->Gpr[5] = ExceptionFrame->Rbp;
    Context->Gpr[6] = ExceptionFrame->Rsi;
    Context->Gpr[7] = ExceptionFrame->Rdi;
    Context->Gpr[8] = InterruptFrame->R8;
    Context->Gpr[9] = InterruptFrame->R9;
    Context->Gpr[10] = InterruptFrame->R10;
    Context->Gpr[11] = InterruptFrame->R11;
    Context->Gpr[12] = ExceptionFrame->R12;
    Context->Gpr[13] = ExceptionFrame->R13;
    Context->Gpr[14] = ExceptionFrame->R14;
    Context->Gpr[15] = ExceptionFrame->R15;
    Context->Rip = InterruptFrame->Rip;
    Context->Rflags = InterruptFrame->Rflags;
    Context->Cr2 = InterruptFrame->FaultAddress;
    Context->ErrorCode = InterruptFrame->ErrorCode;
    Context->Mxcsr = InterruptFrame->Mxcsr;
    Context->Cs = InterruptFrame->SegCs;
    Context->Ss = InterruptFrame->SegSs;
    memcpy(Context->Xmm[0], &InterruptFrame->Xmm0, sizeof(InterruptFrame->Xmm0) * 6);
    memcpy(Context->Xmm[6], &ExceptionFrame->Xmm6, sizeof(ExceptionFrame->Xmm6) * 10);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds a target-owned breakpoint by identifier.
 *
 * PARAMETERS:
 *     Identifier - Stable breakpoint identifier.
 *
 * RETURN VALUE:
 *     Pointer to the entry, or NULL if it does not exist.
 *-----------------------------------------------------------------------------------------------*/
static KdpBreakpoint *FindBreakpointByIdentifier(uint32_t Identifier) {
    for (uint32_t i = 0; i < KDP_MAX_BREAKPOINTS; i++) {
        if (Breakpoints[i].Identifier == Identifier) {
            return &Breakpoints[i];
        }
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds an enabled target-owned breakpoint by address.
 *
 * PARAMETERS:
 *     Address - Virtual address containing INT3.
 *
 * RETURN VALUE:
 *     Pointer to the entry, or NULL if it does not exist.
 *-----------------------------------------------------------------------------------------------*/
static KdpBreakpoint *FindBreakpointByAddress(uint64_t Address) {
    for (uint32_t i = 0; i < KDP_MAX_BREAKPOINTS; i++) {
        if (Breakpoints[i].Identifier && Breakpoints[i].Enabled &&
            Breakpoints[i].Address == Address) {
            return &Breakpoints[i];
        }
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases every processor parked for the active stop generation.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ReleaseProcessors(void) {
    __atomic_store_n(&ReleaseGeneration, StopGeneration, __ATOMIC_RELEASE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores and deletes every target-owned software breakpoint.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true if every installed byte was restored, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool RemoveAllBreakpoints(void) {
    for (uint32_t i = 0; i < KDP_MAX_BREAKPOINTS; i++) {
        if (!Breakpoints[i].Identifier) {
            continue;
        }
        if (Breakpoints[i].Enabled &&
            !HalpPatchDebuggerByte((void *)Breakpoints[i].Address, 0xCC, Breakpoints[i].Original)) {
            return false;
        }
        if (Breakpoints[i].Identifier == ActiveBreakpointIdentifier && ActiveInterruptFrame) {
            ActiveInterruptFrame->Rip = Breakpoints[i].Address;
            ActiveStopReason = KDP_STOP_REASON_DEBUGGER_LOST;
        }
        memset(&Breakpoints[i], 0, sizeof(Breakpoints[i]));
    }
    BreakpointCount = 0;
    ActiveBreakpointIdentifier = 0;
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enters the debugger owner loop after capturing and quiescing the machine.
 *
 * PARAMETERS:
 *     Reason - Stop reason exposed to the debugger.
 *     InterruptFrame - Owner volatile register state.
 *     ExceptionFrame - Owner nonvolatile register state.
 *
 * RETURN VALUE:
 *     true if every processor joined the stop, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool
EnterStop(uint32_t Reason, HalInterruptFrame *InterruptFrame, HalExceptionFrame *ExceptionFrame) {
    uint32_t Processor = KeGetCurrentProcessor()->Number;
    uint32_t ProcessorCount = HalpOnlineProcessorCount ? HalpOnlineProcessorCount : 1;
    uint64_t Generation = __atomic_add_fetch(&StopGeneration, 1, __ATOMIC_ACQ_REL);
    StopOwner = Processor;
    ActiveStopReason = Reason;
    ActiveInterruptFrame = InterruptFrame;
    ActiveExceptionFrame = ExceptionFrame;
    LeaveStopLoop = false;
    __atomic_store_n(&TargetState, KDP_TARGET_STOPPING, __ATOMIC_RELEASE);
    CaptureContext(&Contexts[Processor], InterruptFrame, ExceptionFrame);
    __atomic_store_n(&ContextGenerations[Processor], Generation, __ATOMIC_RELEASE);

    if (HalpSmpInitializationComplete && ProcessorCount > 1) {
        HalpBroadcastDebuggerIpi();
    }

    uint64_t Start = HalGetTimerTicks();
    uint64_t Frequency = HalGetTimerFrequency();
    uint64_t End = Start > UINT64_MAX - Frequency ? UINT64_MAX : Start + Frequency;
    while (true) {
        uint32_t Joined = 0;
        for (uint32_t i = 0; i < ProcessorCount; i++) {
            if (__atomic_load_n(&ContextGenerations[i], __ATOMIC_ACQUIRE) == Generation) {
                Joined++;
            }
        }

        if (Joined == ProcessorCount) {
            break;
        }

        if (HalGetTimerTicks() >= End) {
            ReleaseProcessors();
            __atomic_store_n(&TargetState, KDP_TARGET_RUNNING, __ATOMIC_RELEASE);
            return false;
        }

        PauseProcessor();
    }

    __atomic_store_n(&TargetState, KDP_TARGET_STOPPED, __ATOMIC_RELEASE);
    if (AutomaticContinue) {
        AutomaticContinue = false;
        if (!RemoveAllBreakpoints()) {
            KeFatalError(KE_PANIC_TRAP_NOT_HANDLED, RT_EXC_BREAKPOINT, 0, 0, 0);
        }
        ReleaseProcessors();
        __atomic_store_n(&TargetState, KDP_TARGET_RUNNING, __ATOMIC_RELEASE);
        return true;
    }
    KdpSendStopEvent(Reason, true);
    while (!__atomic_load_n(&LeaveStopLoop, __ATOMIC_ACQUIRE)) {
        KdpPollTransport(KDP_STATE_LATE);
        KdpCheckHeartbeat();
        PauseProcessor();
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parks a processor in the current debugger rendezvous.
 *
 * PARAMETERS:
 *     ExceptionFrame - Nonvolatile register state captured by the debugger IPI entry.
 *     InterruptFrame - Volatile register and machine-frame state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpHandleDebuggerIpi(HalExceptionFrame *ExceptionFrame, HalInterruptFrame *InterruptFrame) {
    uint32_t Processor = KeGetCurrentProcessor()->Number;
    uint64_t Generation = __atomic_load_n(&StopGeneration, __ATOMIC_ACQUIRE);
    CaptureContext(&Contexts[Processor], InterruptFrame, ExceptionFrame);
    __atomic_store_n(&ContextGenerations[Processor], Generation, __ATOMIC_RELEASE);
    while (__atomic_load_n(&ReleaseGeneration, __ATOMIC_ACQUIRE) < Generation) {
        PauseProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles debugger-owned breakpoint and single-step exceptions.
 *
 * PARAMETERS:
 *     ExceptionCode - RT exception code.
 *     InterruptFrame - Volatile register and machine-frame state.
 *     ExceptionFrame - Nonvolatile register state.
 *
 * RETURN VALUE:
 *     true if KD consumed the exception, false if normal RT dispatch must handle it.
 *-----------------------------------------------------------------------------------------------*/
bool KdpHandleException(
    uint32_t ExceptionCode,
    HalInterruptFrame *InterruptFrame,
    HalExceptionFrame *ExceptionFrame) {
    if (ExceptionCode == RT_EXC_SINGLE_STEP && InternalStepArmed) {
        KdpBreakpoint *Breakpoint = FindBreakpointByIdentifier(ActiveBreakpointIdentifier);
        if (Breakpoint &&
            !HalpPatchDebuggerByte((void *)Breakpoint->Address, Breakpoint->Original, 0xCC)) {
            KeFatalError(KE_PANIC_TRAP_NOT_HANDLED, ExceptionCode, InterruptFrame->Rip, 0, 0);
        }

        InterruptFrame->Rflags &= ~(1ull << 8);
        InternalStepArmed = false;
        ActiveBreakpointIdentifier = 0;
        if (InternalStepReportsStop) {
            ActiveInterruptFrame = InterruptFrame;
            ActiveExceptionFrame = ExceptionFrame;
            ActiveStopReason = KDP_STOP_REASON_SINGLE_STEP;
            CaptureContext(&Contexts[StopOwner], InterruptFrame, ExceptionFrame);
            __atomic_store_n(&ContextGenerations[StopOwner], StopGeneration, __ATOMIC_RELEASE);
            __atomic_store_n(&TargetState, KDP_TARGET_STOPPED, __ATOMIC_RELEASE);
            LeaveStopLoop = false;
            KdpSendStopEvent(KDP_STOP_REASON_SINGLE_STEP, true);
            while (!__atomic_load_n(&LeaveStopLoop, __ATOMIC_ACQUIRE)) {
                KdpPollTransport(KDP_STATE_LATE);
                KdpCheckHeartbeat();
                PauseProcessor();
            }
        } else {
            ReleaseProcessors();
            __atomic_store_n(&TargetState, KDP_TARGET_RUNNING, __ATOMIC_RELEASE);
        }

        return true;
    }

    if (ExceptionCode != RT_EXC_BREAKPOINT) {
        return false;
    }

    if (ManualStopArmed) {
        ManualStopArmed = false;
        uint32_t Reason = PendingManualStopReason;
        PendingManualStopReason = KDP_STOP_REASON_MANUAL;
        EnterStop(Reason, InterruptFrame, ExceptionFrame);
        return true;
    }

    if (!InterruptFrame->Rip) {
        return false;
    }

    KdpBreakpoint *Breakpoint = FindBreakpointByAddress(InterruptFrame->Rip - 1);
    if (!Breakpoint) {
        return false;
    }

    Breakpoint->HitCount++;
    ActiveBreakpointIdentifier = Breakpoint->Identifier;
    if (!EnterStop(KDP_STOP_REASON_SOFTWARE_BREAKPOINT, InterruptFrame, ExceptionFrame)) {
        KeFatalError(KE_PANIC_TRAP_NOT_HANDLED, ExceptionCode, InterruptFrame->Rip, 0, 0);
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function triggers an internal breakpoint used for a debugger-requested stop.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true after the target is eventually resumed.
 *-----------------------------------------------------------------------------------------------*/
bool KdpRequestStop(void) {
    if (__atomic_load_n(&TargetState, __ATOMIC_ACQUIRE) != KDP_TARGET_RUNNING) {
        return false;
    }

    ManualStopArmed = true;
    PendingManualStopReason = KDP_STOP_REASON_MANUAL;
    __asm__ volatile("int3" : : : "memory");
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function applies the configured debugger-loss policy from the runtime poll worker.
 *
 * PARAMETERS:
 *     Continue - true to restore breakpoints and resume automatically, false to remain stopped.
 *
 * RETURN VALUE:
 *     true after the loss stop was entered, false if the target was not running.
 *-----------------------------------------------------------------------------------------------*/
bool KdpRequestDisconnect(bool Continue) {
    if (__atomic_load_n(&TargetState, __ATOMIC_ACQUIRE) != KDP_TARGET_RUNNING) {
        return false;
    }
    AutomaticContinue = Continue;
    PendingManualStopReason = KDP_STOP_REASON_DEBUGGER_LOST;
    ManualStopArmed = true;
    __asm__ volatile("int3" : : : "memory");
    return true;
}

void KdpHandleStoppedDisconnect(bool Continue) {
    if (!Continue || __atomic_load_n(&TargetState, __ATOMIC_ACQUIRE) != KDP_TARGET_STOPPED) {
        return;
    }
    if (!RemoveAllBreakpoints()) {
        KeFatalError(KE_PANIC_TRAP_NOT_HANDLED, RT_EXC_BREAKPOINT, 0, 0, 0);
    }
    KdpResume(false);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resumes or single-steps the stopped owner processor.
 *
 * PARAMETERS:
 *     Step - true to report the next single-step stop.
 *
 * RETURN VALUE:
 *     Protocol status describing the operation.
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpResume(bool Step) {
    if (__atomic_load_n(&TargetState, __ATOMIC_ACQUIRE) != KDP_TARGET_STOPPED ||
        !ActiveInterruptFrame) {
        return KDP_PROTOCOL_STATUS_INVALID_STATE;
    }

    if (Step || ActiveStopReason == KDP_STOP_REASON_SOFTWARE_BREAKPOINT) {
        if (ActiveInterruptFrame->Rflags & (1ull << 8)) {
            return KDP_PROTOCOL_STATUS_CONFLICT;
        }

        if (ActiveStopReason == KDP_STOP_REASON_SOFTWARE_BREAKPOINT) {
            KdpBreakpoint *Breakpoint = FindBreakpointByIdentifier(ActiveBreakpointIdentifier);
            if (!Breakpoint ||
                !HalpPatchDebuggerByte((void *)Breakpoint->Address, 0xCC, Breakpoint->Original)) {
                return KDP_PROTOCOL_STATUS_INTERNAL_ERROR;
            }

            ActiveInterruptFrame->Rip = Breakpoint->Address;
        }

        ActiveInterruptFrame->Rflags |= 1ull << 8;
        InternalStepArmed = true;
        InternalStepReportsStop = Step;
        __atomic_store_n(&TargetState, KDP_TARGET_RESUMING, __ATOMIC_RELEASE);
        __atomic_store_n(&LeaveStopLoop, true, __ATOMIC_RELEASE);
        return KDP_PROTOCOL_STATUS_SUCCESS;
    }

    ReleaseProcessors();
    __atomic_store_n(&TargetState, KDP_TARGET_RUNNING, __ATOMIC_RELEASE);
    __atomic_store_n(&LeaveStopLoop, true, __ATOMIC_RELEASE);
    return KDP_PROTOCOL_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds and installs a software breakpoint.
 *
 * PARAMETERS:
 *     Address - Target executable virtual address.
 *     Result - Optional returned public breakpoint entry.
 *
 * RETURN VALUE:
 *     Protocol status describing the operation.
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpAddBreakpoint(uint64_t Address, KdpProtocolBreakpointEntry *Result) {
    if (__atomic_load_n(&TargetState, __ATOMIC_ACQUIRE) != KDP_TARGET_STOPPED) {
        return KDP_PROTOCOL_STATUS_INVALID_STATE;
    }

    for (uint32_t i = 0; i < KDP_MAX_BREAKPOINTS; i++) {
        if (Breakpoints[i].Identifier && Breakpoints[i].Address == Address) {
            return KDP_PROTOCOL_STATUS_CONFLICT;
        }
    }

    uint32_t Slot = KDP_MAX_BREAKPOINTS;
    for (uint32_t i = 0; i < KDP_MAX_BREAKPOINTS; i++) {
        if (!Breakpoints[i].Identifier) {
            Slot = i;
            break;
        }
    }

    if (Slot == KDP_MAX_BREAKPOINTS) {
        return KDP_PROTOCOL_STATUS_TOO_LARGE;
    }

    uint8_t Original = 0;
    if (!HalpReadDebuggerByte((void *)Address, &Original) || Original == 0xCC ||
        !HalpPatchDebuggerByte((void *)Address, Original, 0xCC)) {
        return KDP_PROTOCOL_STATUS_CONFLICT;
    }

    Breakpoints[Slot] = (KdpBreakpoint){
        .Identifier = NextBreakpointIdentifier++,
        .Enabled = true,
        .Original = Original,
        .Address = Address,
    };
    BreakpointCount++;
    if (Result) {
        *Result = (KdpProtocolBreakpointEntry){
            .Identifier = Breakpoints[Slot].Identifier,
            .Flags = KDP_BREAKPOINT_FLAG_ENABLED,
            .Address = Address,
        };
    }

    return KDP_PROTOCOL_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes, enables, or disables one target-owned breakpoint.
 *
 * PARAMETERS:
 *     Identifier - Breakpoint identifier.
 *     Operation - Zero removes, one enables, and two disables the breakpoint.
 *
 * RETURN VALUE:
 *     Protocol status describing the operation.
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpChangeBreakpoint(uint32_t Identifier, int Operation) {
    if (__atomic_load_n(&TargetState, __ATOMIC_ACQUIRE) != KDP_TARGET_STOPPED) {
        return KDP_PROTOCOL_STATUS_INVALID_STATE;
    }

    KdpBreakpoint *Breakpoint = FindBreakpointByIdentifier(Identifier);
    if (!Breakpoint) {
        return KDP_PROTOCOL_STATUS_NOT_FOUND;
    }

    if (Operation == 1 && !Breakpoint->Enabled) {
        if (!HalpPatchDebuggerByte((void *)Breakpoint->Address, Breakpoint->Original, 0xCC)) {
            return KDP_PROTOCOL_STATUS_CONFLICT;
        }
        Breakpoint->Enabled = true;
    } else if ((Operation == 0 || Operation == 2) && Breakpoint->Enabled) {
        if (!HalpPatchDebuggerByte((void *)Breakpoint->Address, 0xCC, Breakpoint->Original)) {
            return KDP_PROTOCOL_STATUS_CONFLICT;
        }
        Breakpoint->Enabled = false;
    }

    if (!Operation) {
        if (Identifier == ActiveBreakpointIdentifier &&
            ActiveStopReason == KDP_STOP_REASON_SOFTWARE_BREAKPOINT) {
            ActiveInterruptFrame->Rip = Breakpoint->Address;
            ActiveBreakpointIdentifier = 0;
            ActiveStopReason = KDP_STOP_REASON_MANUAL;
        }
        memset(Breakpoint, 0, sizeof(*Breakpoint));
        BreakpointCount--;
    }

    return KDP_PROTOCOL_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns one page from the target-owned breakpoint table.
 *
 * PARAMETERS:
 *     StartIndex - Logical breakpoint index to begin with.
 *     MaximumEntries - Capacity of the destination array.
 *     Entries - Destination entries.
 *     TotalCount - Returned total breakpoint count.
 *     ReturnedCount - Returned number of entries.
 *
 * RETURN VALUE:
 *     Protocol status describing the operation.
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpListBreakpoints(
    uint32_t StartIndex,
    uint32_t MaximumEntries,
    KdpProtocolBreakpointEntry *Entries,
    uint32_t *TotalCount,
    uint32_t *ReturnedCount) {
    if (!Entries || !TotalCount || !ReturnedCount || StartIndex > BreakpointCount) {
        return KDP_PROTOCOL_STATUS_INVALID_ARGUMENT;
    }

    *TotalCount = BreakpointCount;
    *ReturnedCount = 0;
    uint32_t LogicalIndex = 0;
    for (uint32_t i = 0; i < KDP_MAX_BREAKPOINTS && *ReturnedCount < MaximumEntries; i++) {
        if (!Breakpoints[i].Identifier) {
            continue;
        }
        if (LogicalIndex++ < StartIndex) {
            continue;
        }

        Entries[*ReturnedCount] = (KdpProtocolBreakpointEntry){
            .Identifier = Breakpoints[i].Identifier,
            .Flags = Breakpoints[i].Enabled ? KDP_BREAKPOINT_FLAG_ENABLED : 0,
            .Address = Breakpoints[i].Address,
            .HitCount = Breakpoints[i].HitCount,
        };
        (*ReturnedCount)++;
    }

    return KDP_PROTOCOL_STATUS_SUCCESS;
}

bool KdpGetContext(uint32_t Processor, uint64_t *Generation, KdpAmd64Context *Context) {
    if (__atomic_load_n(&TargetState, __ATOMIC_ACQUIRE) != KDP_TARGET_STOPPED ||
        Processor >= HalpOnlineProcessorCount || !Generation || !Context) {
        return false;
    }
    *Generation = __atomic_load_n(&ContextGenerations[Processor], __ATOMIC_ACQUIRE);
    memcpy(Context, &Contexts[Processor], sizeof(*Context));
    return *Generation == StopGeneration;
}

uint32_t KdpGetTargetState(void) {
    return __atomic_load_n(&TargetState, __ATOMIC_ACQUIRE);
}

uint32_t KdpGetStopOwner(void) {
    return StopOwner;
}

uint64_t KdpGetStopGeneration(void) {
    return StopGeneration;
}

uint32_t KdpGetBreakpointCount(void) {
    return BreakpointCount;
}

uint32_t KdpGetStopReason(void) {
    return ActiveStopReason;
}

void KdpSetTargetRunning(void) {
    __atomic_store_n(&TargetState, KDP_TARGET_RUNNING, __ATOMIC_RELEASE);
}

void KdpSetTargetPanic(void) {
    __atomic_store_n(&TargetState, KDP_TARGET_PANIC, __ATOMIC_RELEASE);
}
