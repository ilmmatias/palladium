/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _FILE_H_
#define _FILE_H_

#include <efi/spec.h>
#include <stdint.h>

EFI_STATUS OslpInitializeRootVolume(void);
void *OslReadFile(const char *Path, uint64_t *Size);

#endif /* _FILE_H_ */
