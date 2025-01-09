/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/regs.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes a thread context, for initial "restore"/enqueue.
 *
 * PARAMETERS:
 *     Context - Which thread context we're initializing.
 *     Stack - Base address of the thread stack.
 *     StackSize - Size of the thread stack.
 *     EntryPoint - What we should execute once the thread gets enqueued.
 *     Parameter - What should be passed into the EntryPoint.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeThreadContext(
    HalRegisterState *Context,
    char *Stack,
    uint64_t StackSize,
    void (*EntryPoint)(void *),
    void *Parameter) {
    Context->Rcx = (uint64_t)Parameter;
    Context->Rip = (uint64_t)EntryPoint;
    Context->Cs = 0x08;
    Context->Rflags = 0x202;
    Context->Rsp = (uint64_t)Stack + StackSize;
    Context->Ss = 0x10;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves a register context into a thread for a future restore.
 *
 * PARAMETERS:
 *     Source - Source context for the thread.
 *     Target - Which thread context to save it into.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpSaveThreadContext(HalRegisterState *Source, HalRegisterState *Target) {
    memcpy(Target, Source, sizeof(HalRegisterState));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores a previously saved (or setup) context into a HalRegisterState
 *     structure.
 *
 * PARAMETERS:
 *     Target - Target thread context.
 *     Source - Which thread to use as base.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpRestoreThreadContext(HalRegisterState *Target, HalRegisterState *Source) {
    memcpy(Target, Source, sizeof(HalRegisterState));
}
