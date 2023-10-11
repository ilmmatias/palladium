/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <stdint.h>

#define DISPLAY_COLOR_DEFAULT 0x000000, 0xAAAAAA
#define DISPLAY_COLOR_INVERSE 0xAAAAAA, 0x000000
#define DISPLAY_COLOR_HIGHLIGHT 0x000000, 0xFFFFFF
#define DISPLAY_COLOR_PANIC 0xAA0000, 0xFFFFFF

void BmInitDisplay(void);
void BmSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor);
void BmGetColor(uint32_t *BackgroundColor, uint32_t *ForegroundColor);
void BmSetCursor(uint16_t X, uint16_t Y);
void BmGetCursor(uint16_t *X, uint16_t *Y);
void BmClearLine(int LeftOffset, int RightOffset);
void BmPutChar(char Character);
void BmPutString(const char *String);

#endif /* _DISPLAY_H_ */
