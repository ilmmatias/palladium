/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_VIDFUNCS_H_
#define _KERNEL_DETAIL_VIDFUNCS_H_

#include <stdarg.h>
#include <stdint.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void VidResetDisplay(void);

void VidSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor);
void VidGetColor(uint32_t *BackgroundColor, uint32_t *ForegroundColor);
void VidSetCursor(uint16_t X, uint16_t Y);
void VidGetCursor(uint16_t *X, uint16_t *Y);

void VidPutChar(char Character);
void VidPutString(const char *String);
void VidPrintVariadic(const char *Message, va_list Arguments);
__attribute__((format(printf, 1, 2))) void VidPrint(const char *Message, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_VIDFUNCS_H_ */
