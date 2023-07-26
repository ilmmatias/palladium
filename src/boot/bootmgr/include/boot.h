/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BOOT_H_
#define _BOOT_H_

#include <stddef.h>
#include <stdint.h>

void BmInitArch();

void BmInitMemory(void *BootBlock);
void *BmAllocate(size_t Size);
void BmFree(void *Base);

void BmInitDisplay();
void BmSetCursor(uint16_t x, uint16_t y);
void BmGetCursor(uint16_t *x, uint16_t *y);
void BmSetCursorX(uint16_t x);
void BmSetCursorY(uint16_t y);
uint16_t BmGetCursorX(void);
uint16_t BmGetCursorY(void);
void BiPutChar(char c);
void BmPut(const char *s, ...);

#endif /* _BOOT_H_ */
