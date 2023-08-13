/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <stdint.h>

void BmInitDisplay(void);
void BmSetColor(uint8_t Color);
uint8_t BmGetColor(void);
void BmSetCursor(uint16_t X, uint16_t Y);
void BmGetCursor(uint16_t *X, uint16_t *Y);
void BmSetCursorX(uint16_t X);
void BmSetCursorY(uint16_t Y);
uint16_t BmGetCursorX(void);
uint16_t BmGetCursorY(void);
void BmPutChar(char Character);
void BmPutString(const char *String);

#endif /* _DISPLAY_H_ */
