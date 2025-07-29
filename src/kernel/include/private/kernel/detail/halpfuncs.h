/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_HALPFUNCS_H_
#define _KERNEL_DETAIL_HALPFUNCS_H_

#include <kernel/detail/halfuncs.h>
#include <kernel/detail/kitypes.h>
#include <kernel/detail/midefs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, halpfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, halpfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern uint32_t HalpProcessorCount;
extern uint32_t HalpOnlineProcessorCount;
extern KeProcessor **HalpProcessorList;

void HalpInitializeBootStack(KiLoaderBlock *LoaderBlock);
void HalpInitializePlatform(KiLoaderBlock *LoaderBlock);
void HalpInitializeBootProcessor(void);
void HalpInitializeApplicationProcessor(KeProcessor *Processor);

/* I'm not fully sure, should this be here? Initializing/using the kernel debugging stuff is
 * probably going to need this, so probably? */
void HalpInitializeEarlyMap(KiLoaderBlock *LoaderBlock);
void *HalpMapEarlyMemory(uint64_t PhysicalAddress, size_t Size, int Flags);
void HalpUnmapEarlyMemory(void *VirtualAddress, size_t Size);

/* I'm pretty sure this can go to the amd64-specific halpfuncs.h, but I'll leave here for now (at
 * least until we re-add a way to query the ACPI tables for the rest of the kernel). */
void HalpInitializeEarlyAcpi(KiLoaderBlock *LoaderBlock);
void *HalpFindEarlyAcpiTable(const char *Signature);

uint64_t HalpGetPhysicalAddress(void *VirtualAddress);
bool HalpMapContiguousPages(
    void *VirtualAddress,
    uint64_t PhysicalAddresses,
    uint64_t Size,
    int Flags);
bool HalpMapNonContiguousPages(
    void *VirtualAddress,
    uint64_t *PhysicalAddresses,
    uint64_t Size,
    int Flags);
void HalpUnmapPages(void *VirtualAddress, uint64_t Size);

void HalpBroadcastIpi(void);
void HalpBroadcastFreeze(void);
void HalpNotifyProcessor(KeProcessor *Processor, KeIrql TargetIrql);

void *HalpEnterCriticalSection(void);
void HalpLeaveCriticalSection(void *Context);

void HalpInitializeContext(
    HalContextFrame *Context,
    char *Stack,
    uint64_t StackSize,
    void (*EntryPoint)(void *),
    void *Parameter);
void HalpSwitchContext(HalContextFrame *CurrentContext, HalContextFrame *TargetContext);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_HALPFUNCS_H_ */
