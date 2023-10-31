/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BOOT_H_
#define _BOOT_H_

#include <stddef.h>
#include <stdint.h>

#define PAGE_WRITE 0x01
#define PAGE_EXEC 0x02

typedef struct __attribute__((packed)) {
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t ImageSize;
    uint64_t EntryPoint;
    int* PageFlags;
} LoadedImage;

void BiInitArch(void* BootBlock);

void BiLoadMenuEntries(void);
[[noreturn]] void BiEnterMenu(void);
[[noreturn]] void BiLoadPalladium(const char* SystemFolder);

void BiCheckCompatibility(void);
[[noreturn]] void BiTransferExecution(LoadedImage* Images, size_t ImageCount);

[[noreturn]] void BmPanic(const char* Message);

#endif /* _BOOT_H_ */
