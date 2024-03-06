/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <memory.h>

/* Copy of the KeProcessor struct from src/kernel/include/public/amd64/processor.h. */
typedef struct {
    RtSList ListHeader;
    uint16_t Number;
    uint32_t ApicId;
    int Online;
    int ThreadQueueLock;
    RtDList ThreadQueue;
    uint32_t ThreadQueueSize;
    void *InitialThread;
    void *CurrentThread;
    void *IdleThread;
    int ForceYield;
    int EventStatus;
    RtDList DpcQueue;
    RtDList EventQueue;
    char SystemStack[8192] __attribute__((aligned(4096)));
    uint64_t GdtEntries[5];
    struct __attribute__((packed)) __attribute__((aligned(4))) {
        uint16_t Limit;
        uint64_t Base;
    } GdtDescriptor;
    char IdtEntries[4096];
    struct __attribute__((packed)) __attribute__((aligned(4))) {
        uint16_t Limit;
        uint64_t Base;
    } IdtDescriptor;
    struct {
        RtSList ListHead;
        uint32_t Usage;
    } IdtSlots[224];
    uintptr_t IdtIrqlSlots[256];
} KeProcessor;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the boot processor structure (which also contains the initial kernel
 *     stack).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the KeProcessor structure, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *OslpInitializeBsp(void) {
    KeProcessor *BootProcessor = OslAllocatePages(sizeof(KeProcessor), PAGE_BOOT_PROCESSOR);
    if (!BootProcessor) {
        OslPrint("The system ran out of memory while creating the boot processor structure.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return NULL;
    }

    /* HalpInitializePlatform+the other HAL functions should setup the BSP (and any other AP)
     * structures, so that's all we need to do. */
    return BootProcessor;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the initial/boot kernel stack from the BSP structure.
 *
 * PARAMETERS:
 *     BspPtr - Pointer returned by OslpInitializeBsp.
 *     StackSize - Output; Size of the kernel stack.
 *
 * RETURN VALUE:
 *     Pointer to the top of the initial kernel stack.
 *-----------------------------------------------------------------------------------------------*/
void *OslpGetBootStack(void *BspPtr) {
    KeProcessor *BootProcessor = BspPtr;
    return BootProcessor->SystemStack + sizeof(BootProcessor->SystemStack);
}
