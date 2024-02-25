/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

void OslPutChar(char Character);
void OslPutString(const char *String);
void OslPrint(const char *Format, ...);

#endif /* _CONSOLE_H_ */
