/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_HALP_H_
#define _AMD64_HALP_H_

#include <amd64/hal.h>
#include <halp.h>

typedef struct {
    HalProcessor Base;
    uint32_t ApicId;
    uint64_t GdtEntries[5];
    struct __attribute__((packed)) {
        uint16_t BaseLow;
        uint16_t Cs;
        uint8_t Ist;
        uint8_t Attributes;
        uint16_t BaseMid;
        uint32_t BaseHigh;
        uint32_t Reserved;
    } IdtEntries[256];
    struct {
        RtSList ListHead;
        uint32_t Usage;
    } IdtSlots[224];
    struct __attribute__((packed)) {
        uint16_t Limit;
        uint64_t Base;
    } GdtDescriptor, IdtDescriptor;
} HalpProcessor;

void HalpInitializeIdt(HalpProcessor *Processor);
void HalpInitializeGdt(HalpProcessor *Processor);
void HalpFlushGdt(void);

void HalpInitializeApic(void);
void HalpEnableApic(void);
uint64_t HalpReadLapicRegister(uint32_t Number);
void HalpWriteLapicRegister(uint32_t Number, uint64_t Data);
void HalpClearApicErrors(void);
void HalpSendIpi(uint32_t Target, uint32_t Vector);
void HalpWaitIpiDelivery(void);
void HalpSendEoi(void);

void HalpInitializeIoapic(void);
void HalpEnableIrq(uint8_t Irq, uint8_t Vector);
void HalpEnableGsi(uint8_t Gsi, uint8_t Vector);

void HalpInitializeHpet(void);
void HalpInitializeApicTimer(void);

void HalpInitializeSmp(void);

#endif /* _AMD64_HALP_H_ */
