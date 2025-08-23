/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_HALTYPES_H_
#define _KERNEL_DETAIL_HALTYPES_H_

#include <kernel/detail/ketypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, haltypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, haltypes.h)
#endif /* __has__include */
/* clang-format on */

typedef void (*HalInterruptHandler)(void *);

typedef struct {
    RtDList ListHeader;
    bool Enabled;
    KeSpinLock Lock;
    HalInterruptData Data;
    void (*Handler)(void *);
    void *HandlerContext;
} HalInterrupt;

typedef struct __attribute__((packed)) {
    uint16_t VendorId;
    uint16_t DeviceId;
    uint16_t Command;
    uint16_t Status;
    uint8_t RevisionId;
    uint8_t ProgIf;
    uint8_t SubClass;
    uint8_t Class;
    uint8_t CacheLineSize;
    uint8_t LatencyTimer;
    uint8_t HeaderType;
    uint8_t Bist;
    union {
        struct __attribute__((packed)) {
            uint32_t BarAddress[6];
            uint32_t CardbusCisPointer;
            uint16_t SubsystemVendorId;
            uint16_t SubsystemId;
            uint32_t ExpansionRomBaseAddress;
            uint8_t CapabilitiesPointer;
            uint8_t Reserved[7];
            uint8_t InterruptLine;
            uint8_t InterruptPin;
            uint8_t MinGrant;
            uint8_t MaxLatency;
        } Type0;
    };
} HalPciHeader;

#endif /* _KERNEL_DETAIL_HALTYPES_H_ */
