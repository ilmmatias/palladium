/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _VID_H_
#define _VID_H_

#include <stdint.h>

#define VID_COLOR_DEFAULT 0
#define VID_COLOR_PANIC 1

void VidResetDisplay(void);

void VidSetColor(int Color);
int VidGetColor(void);
void VidSetCursor(int X, int Y);
void VidGetCursor(int *X, int *Y);

void VidPutChar(char Character);
void VidPutString(const char *String);

#endif /* _VID_H_ */
