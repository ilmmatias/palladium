/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BOOT_H_
#define _BOOT_H_

#include <stddef.h>
#include <stdint.h>

void BmInitStdio(void);
void BmInitArch(void* BootBlock);

[[noreturn]] void BmEnterMenu(void);

uint32_t BmHashData(const void* Buffer, size_t Length);

#endif /* _BOOT_H_ */
