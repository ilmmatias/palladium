/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <stddef.h>
#include <stdint.h>

#define DISPLAY_COLOR_DEFAULT 0x000000, 0xAAAAAA
#define DISPLAY_COLOR_INVERSE 0xAAAAAA, 0x000000
#define DISPLAY_COLOR_HIGHLIGHT 0x000000, 0xFFFFFF

void BiInitializeDisplay(void);
void BmResetDisplay(void);

void BmSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor);
void BmGetColor(uint32_t *BackgroundColor, uint32_t *ForegroundColor);
void BmSetCursor(uint16_t X, uint16_t Y);
void BmGetCursor(uint16_t *X, uint16_t *Y);

void BmClearLine(int LeftOffset, int RightOffset);
size_t BmGetStringWidth(const char *String);

void BmPutChar(char Character);
void BmPutString(const char *String);
void BmPrint(const char *Format, ...);

#endif /* _DISPLAY_H_ */
