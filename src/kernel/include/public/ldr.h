/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _LDR_H_
#define _LDR_H_

#include <rt/list.h>

typedef struct {
    RtSList ListHeader;
    const char *Name;
    uint64_t ImageBase;
    uint64_t ImageSize;
} LdrModule;

#endif /* _LDR_H_ */
