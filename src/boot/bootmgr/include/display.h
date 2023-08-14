/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <stdint.h>

#if defined(ARCH_x86) || defined(ARCH_amd64)
#define DISPLAY_WIDTH 80
#define DISPLAY_HEIGHT 25
#define DISPLAY_COLOR_DEFAULT 0x07
#define DISPLAY_COLOR_HIGHLIGHT 0x0F
#define DISPLAY_COLOR_INVERSE 0x70
#else
#error "Undefined ARCH for the bootmgr module!"
#endif /* ARCH */

void BmInitDisplay(void);
void BmSetColor(uint8_t Color);
uint8_t BmGetColor(void);
void BmSetCursor(uint16_t X, uint16_t Y);
void BmGetCursor(uint16_t *X, uint16_t *Y);
void BmSetCursorX(uint16_t X);
void BmSetCursorY(uint16_t Y);
uint16_t BmGetCursorX(void);
uint16_t BmGetCursorY(void);
void BmClearLine(int LeftOffset, int RightOffset);
void BmPutChar(char Character);
void BmPutString(const char *String);

#endif /* _DISPLAY_H_ */
