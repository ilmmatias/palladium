/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BOOT_H_
#define _BOOT_H_

#include <stddef.h>
#include <stdint.h>

#define PAGE_WRITE 0x01
#define PAGE_EXEC 0x02

typedef struct {
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t ImageSize;
    int* PageFlags;
} LoadedImage;

void BmInitStdio(void);
void BmInitArch(void* BootBlock);

void BmLoadMenuEntries(void);
[[noreturn]] void BmEnterMenu(void);
[[noreturn]] void BmLoadPalladium(const char* SystemFolder);

void BmCheckCompatibility(void);
[[noreturn]] void BmTransferExecution(LoadedImage* Images, size_t ImageCount, uint64_t EntryPoint);

[[noreturn]] void BmPanic(const char* Message);

#endif /* _BOOT_H_ */
