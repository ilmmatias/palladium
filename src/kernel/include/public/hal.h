/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _HAL_H_
#define _HAL_H_

#include <stdint.h>

void *HalFindAcpiTable(const char Signature[4], int Index);

#endif /* _HAL_H_ */
