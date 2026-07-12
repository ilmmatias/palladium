/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/halp.h> */

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
void HalpInitializeDiagnosticDevice(KiLoaderBlock *LoaderBlock);
bool HalpDiagnosticDeviceEnabled(void);
void HalpWriteDiagnosticDevice(const char *Buffer, size_t Size);
void HalpInitializePlatform(KiLoaderBlock *LoaderBlock);
void HalpInitializeBootProcessor(void);
void HalpInitializeApplicationProcessor(KeProcessor *Processor);

void HalpInitializeEarlyMap(KiLoaderBlock *LoaderBlock);
void *HalpMapDebuggerMemory(uint64_t PhysicalAddress, size_t Size, int Flags);
void HalpUnmapDebuggerMemory(void *VirtualAddress, size_t Size);
void *HalpMapEarlyMemory(uint64_t PhysicalAddress, size_t Size, int Flags);
void HalpUnmapEarlyMemory(void *VirtualAddress, size_t Size);

void HalpInitializeLateAcpi(KiLoaderBlock *LoaderBlock);

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

#ifdef PALLADIUM_ENABLE_SELF_TESTS
void HalpSetPageTableAllocationFailure(uint64_t Index);
uint64_t HalpGetPageTableAllocationCount(void);
#endif /* PALLADIUM_ENABLE_SELF_TESTS */

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
