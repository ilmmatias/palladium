/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/hal.h>

extern void HalpThreadEntry(void);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes a thread context to run the specified entry point.
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
void HalpInitializeContext(
    HalContextFrame *Context,
    char *Stack,
    uint64_t StackSize,
    void (*EntryPoint)(void *),
    void *Parameter) {
    HalStartFrame *StartFrame = ((HalStartFrame *)(Stack + StackSize)) - 1;
    HalExceptionFrame *ExceptionFrame = (HalExceptionFrame *)StartFrame - 1;
    StartFrame->EntryPoint = EntryPoint;
    StartFrame->Parameter = Parameter;
    ExceptionFrame->Mxcsr = 0x1F80;
    ExceptionFrame->ReturnAddress = (uint64_t)HalpThreadEntry;
    Context->Rsp = (uint64_t)ExceptionFrame;
}
