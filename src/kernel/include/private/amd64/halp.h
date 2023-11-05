/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_HALP_H_
#define _AMD64_HALP_H_

#include <amd64/hal.h>

void HalpInitializeIdt(void);

void HalpInitializeApic(void);
uint32_t HalpGetCurrentApicId(void);
void HalpSendEoi(void);

void HalpInitializeIoapic(void);
void HalpEnableIrq(uint8_t Irq, uint8_t Vector);
void HalpEnableGsi(uint8_t Gsi, uint8_t Vector);

void HalpInitializeHpet(void);

#endif /* _AMD64_HALP_H_ */
