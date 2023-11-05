/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_HPET_H_
#define _AMD64_HPET_H_

#include <stdint.h>

#define HPET_CAP_REG 0x000
#define HPET_CFG_REG 0x010
#define HPET_STS_REG 0x020
#define HPET_VAL_REG 0x0F0
#define HPET_TIMER_CAP_REG(n) (0x100 + ((n) << 5))
#define HPET_TIMER_CMP_REG(n) (0x108 + ((n) << 5))
#define HPET_TIMER_FSB_REG(n) (0x110 + ((n) << 5))

typedef struct __attribute__((packed)) {
    char Unused[36];
    uint32_t HardwareId;
    uint8_t AddressSpaceId;
    uint8_t RegisterBitWidth;
    uint8_t RegisterBitOffset;
    uint8_t Reserved;
    uint64_t Address;
    uint8_t SequenceNumber;
    uint16_t MinimumTicks;
    uint8_t PageProtection;
} HpetHeader;

#endif /* _AMD64_HPET_H_ */
