/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _HALP_H_
#define _HALP_H_

#ifdef ARCH_amd64
#include <amd64/regs.h>
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#include <hal.h>

extern uint32_t HalpProcessorCount;
extern RtSList HalpProcessorListHead;

void HalpInitializePlatform(int IsBsp, void *Processor);

uint64_t HalpGetPhysicalAddress(void *VirtualAddress);
int HalpMapPage(void *VirtualAddress, uint64_t PhysicalAddress, int Flags);

void HalpSetEvent(uint64_t Time);
void HalpNotifyProcessor(HalProcessor *Processor, int WaitDelivery);

void *HalpEnterCriticalSection(void);
void HalpLeaveCriticalSection(void *Context);

void HalpInitializeContext(
    HalRegisterState *Context,
    char *Stack,
    uint64_t StackSize,
    void (*EntryPoint)(void *),
    void *Parameter);
void HalpSaveContext(HalRegisterState *Source, HalRegisterState *Thread);
void HalpRestoreContext(HalRegisterState *Target, HalRegisterState *Thread);

#endif /* _HALP_H_ */
