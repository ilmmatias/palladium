/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_HALP_H_
#define _AMD64_HALP_H_

#include <amd64/hal.h>
#include <halp.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void HalpInitializeIdt(KeProcessor *Processor);
void HalpInitializeGdt(KeProcessor *Processor);
void HalpFlushGdt(void);

void HalpInitializeApic(void);
void HalpEnableApic(void);
uint64_t HalpReadLapicRegister(uint32_t Number);
void HalpWriteLapicRegister(uint32_t Number, uint64_t Data);
uint32_t HalpReadLapicId(void);
void HalpSendIpi(uint32_t Target, uint32_t Vector);
void HalpSendNmi(uint32_t Target);
void HalpWaitIpiDelivery(void);
void HalpSendEoi(void);

void HalpInitializeIoapic(void);
void HalpEnableIrq(uint8_t Irq, uint8_t Vector);
void HalpEnableGsi(uint8_t Gsi, uint8_t Vector);

void HalpInitializeHpet(void);
void HalpInitializeApicTimer(void);

void HalpInitializeSmp(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _AMD64_HALP_H_ */
