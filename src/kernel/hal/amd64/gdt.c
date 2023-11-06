/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t Limit;
    uint64_t Base;
} GdtDescriptor;

extern void HalpFlushGdt(void);

static __attribute__((aligned(0x10))) uint64_t Entries[5];
static GdtDescriptor Descriptor;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the Global Descriptor Table. This, in combination with
 *     InitializeIdt, means we're safe to unmap the first 2MiB (and map the SMP entry point to it).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeGdt(void) {
    Entries[0] = 0x0000000000000000;
    Entries[1] = 0x00AF9A000000FFFF;
    Entries[2] = 0x00AF92000000FFFF;
    Entries[3] = 0x00AFFA000000FFFF;
    Entries[4] = 0x00AFF2000000FFFF;

    Descriptor.Limit = sizeof(Entries) - 1;
    Descriptor.Base = (uint64_t)Entries;

    __asm__ volatile("lgdt %0" :: "m"(Descriptor));
    HalpFlushGdt();
}
