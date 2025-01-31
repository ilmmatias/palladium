/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_HPET_H_
#define _KERNEL_DETAIL_AMD64_HPET_H_

#include <stdint.h>

#define HPET_CAP_REG 0x000
#define HPET_CFG_REG 0x010
#define HPET_STS_REG 0x020
#define HPET_VAL_REG 0x0F0
#define HPET_TIMER_CAP_REG(n) (0x100 + ((n) << 5))
#define HPET_TIMER_CMP_REG(n) (0x108 + ((n) << 5))
#define HPET_TIMER_FSB_REG(n) (0x110 + ((n) << 5))

#define HPET_CAP_64B 0x2000
#define HPET_CAP_FREQ_START 32

#define HPET_CFG_INT_ENABLE 0x01
#define HPET_CFG_LEGACY_ENABLE 0x02
#define HPET_CFG_MASK (HPET_CFG_INT_ENABLE | HPET_CFG_LEGACY_ENABLE)

#define HPET_TIMER_INT_ENABLE 0x04
#define HPET_TIMER_32B_ENABLE 0x100
#define HPET_TIMER_FSB_ENABLE 0x4000
#define HPET_TIMER_MASK (HPET_TIMER_INT_ENABLE | HPET_TIMER_32B_ENABLE | HPET_TIMER_FSB_ENABLE)

typedef struct __attribute__((packed)) {
    char Unused[36];
    uint8_t HardwareId;
    uint8_t ComparatorCount : 5;
    uint8_t CounterSize : 1;
    uint8_t Reserved0 : 1;
    uint8_t LegacyReplacement : 1;
    uint16_t PciVendorId;
    uint8_t AddressSpaceId;
    uint8_t RegisterBitWidth;
    uint8_t RegisterBitOffset;
    uint8_t Reserved1;
    uint64_t Address;
    uint8_t SequenceNumber;
    uint16_t MinimumTicks;
    uint8_t PageProtection;
} HpetHeader;

#endif /* _KERNEL_DETAIL_AMD64_HPET_H_ */
