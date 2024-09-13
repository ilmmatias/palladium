/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _HALP_H_
#define _HALP_H_

#ifdef ARCH_amd64
#include <amd64/regs.h>
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#include <hal.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern uint32_t HalpProcessorCount;
extern KeProcessor **HalpProcessorList;

void HalpInitializePlatform(KeProcessor *Processor, int IsBsp);
void HalpStopProcessor(void);
void HalpPauseProcessor(void);

uint64_t HalpGetPhysicalAddress(void *VirtualAddress);
int HalpMapPage(void *VirtualAddress, uint64_t PhysicalAddress, int Flags);
void HalpUnmapPage(void *VirtualAddress);

void HalpNotifyProcessor(KeProcessor *Processor, int WaitDelivery);

void *HalpEnterCriticalSection(void);
void HalpLeaveCriticalSection(void *Context);

KeIrql HalpGetIrql(void);
void HalpSetIrql(KeIrql NewIrql);

void HalpInitializeThreadContext(
    HalRegisterState *Context,
    char *Stack,
    uint64_t StackSize,
    void (*EntryPoint)(void *),
    void *Parameter);
void HalpSaveThreadContext(HalRegisterState *Source, HalRegisterState *Thread);
void HalpRestoreThreadContext(HalRegisterState *Target, HalRegisterState *Thread);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _HALP_H_ */
