/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

void OslPutChar(char Character);
void OslPutString(const char *String);
__attribute__((format(printf, 1, 2))) void OslPrint(const char *Format, ...);

#endif /* _CONSOLE_H_ */
