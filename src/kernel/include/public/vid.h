/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _VID_H_
#define _VID_H_

#include <stdarg.h>
#include <stdint.h>

#define VID_COLOR_DEFAULT 0x000000, 0xAAAAAA
#define VID_COLOR_INVERSE 0xAAAAAA, 0x000000
#define VID_COLOR_HIGHLIGHT 0x000000, 0xFFFFFF
#define VID_COLOR_PANIC 0xAA0000, 0xFFFFFF

#define VID_MESSAGE_ERROR 0
#define VID_MESSAGE_TRACE 1
#define VID_MESSAGE_DEBUG 2
#define VID_MESSAGE_INFO 3

#define VID_ENABLE_MESSAGE_TRACE 0
#define VID_ENABLE_MESSAGE_DEBUG 1
#define VID_ENABLE_MESSAGE_INFO 1

void VidResetDisplay(void);

void VidSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor);
void VidGetColor(uint32_t *BackgroundColor, uint32_t *ForegroundColor);
void VidSetCursor(uint16_t X, uint16_t Y);
void VidGetCursor(uint16_t *X, uint16_t *Y);

void VidPutChar(char Character);
void VidPutString(const char *String);
void VidPrintVariadic(int Type, const char *Prefix, const char *Message, va_list Arguments);
void VidPrint(int Type, const char *Prefix, const char *Message, ...);

#endif /* _VID_H_ */
