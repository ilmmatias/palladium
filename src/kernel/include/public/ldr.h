/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

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
