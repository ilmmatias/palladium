/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BOOT_H_
#define _BOOT_H_

#include <stddef.h>
#include <stdint.h>

#if defined(ARCH_x86) || defined(ARCH_amd64)
#define PAGE_SHIFT 12
#else
#error "Undefined ARCH for the bootmgr module!"
#endif /* ARCH */

#define PAGE_SIZE (1 << (PAGE_SHIFT))

void BmInitArch(void *BootBlock);

void BmInitMemory(void *BootBlock);
void *BmAllocatePages(size_t Pages);
void BmFreePages(void *Base, size_t Pages);

void BmInitDisplay(void);
void BmSetColor(uint8_t Color);
uint8_t BmGetColor(void);
void BmSetCursor(uint16_t X, uint16_t Y);
void BmGetCursor(uint16_t *X, uint16_t *Y);
void BmSetCursorX(uint16_t X);
void BmSetCursorY(uint16_t Y);
uint16_t BmGetCursorX(void);
uint16_t BmGetCursorY(void);
void BiPutChar(char c);

#endif /* _BOOT_H_ */
