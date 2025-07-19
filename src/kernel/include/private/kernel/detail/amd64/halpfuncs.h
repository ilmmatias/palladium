/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_HALPFUNCS_H_
#define _KERNEL_DETAIL_AMD64_HALPFUNCS_H_

#include <kernel/detail/ketypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void HalpInitializeIdt(KeProcessor *Processor);
void HalpInitializeGdt(KeProcessor *Processor);
void HalpFlushGdt(void);
void HalpUpdateTss(void);

void HalpInitializeApic(void);
void HalpEnableApic(void);
uint64_t HalpReadLapicRegister(uint32_t Number);
void HalpWriteLapicRegister(uint32_t Number, uint64_t Data);
uint32_t HalpReadLapicId(void);
void HalpSendIpi(uint32_t Target, uint8_t Vector, uint8_t DeliveryMode);
void HalpSendEoi(void);

void HalpInitializeIoapic(void);
bool HalpTranslateIrq(uint8_t Irq, uint8_t *Gsi, uint8_t *PinPolarity, uint8_t *TriggerMode);
void HalpEnableGsi(uint8_t Gsi, uint8_t Vector, uint8_t PinPolarity, uint8_t TriggerMode);
void HalpDisableGsi(uint8_t Gsi);

void HalpInitializeHpet(void);
int HalpGetHpetWidth(void);
uint64_t HalpGetHpetFrequency(void);
uint64_t HalpGetHpetTicks(void);

void HalpInitializeTsc(void);
uint64_t HalpGetTscFrequency(void);
uint64_t HalpGetTscTicks(void);

void HalpInitializeTimer(void);
void HalpInitializeApicTimer(void);

void HalpInitializeSmp(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_AMD64_HALPFUNCS_H_ */
