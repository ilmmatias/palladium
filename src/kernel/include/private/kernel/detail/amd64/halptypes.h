/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_AMD64_HALPTYPES_H_
#define _KERNEL_DETAIL_AMD64_HALPTYPES_H_

#include <stdint.h>

typedef union {
    struct __attribute__((packed)) {
        uint64_t Present : 1;
        uint64_t Writable : 1;
        uint64_t User : 1;
        uint64_t WriteThrough : 1;
        uint64_t CacheDisable : 1;
        uint64_t Accessed : 1;
        uint64_t Dirty : 1;
        uint64_t PageSize : 1;
        uint64_t Global : 1;
        uint64_t Available0 : 2;
        uint64_t Pat : 1;
        uint64_t Address : 40;
        uint64_t Available1 : 7;
        uint64_t ProtectionKey : 4;
        uint64_t NoExecute : 1;
    };
    uint64_t RawData;
} HalpPageFrame;

#endif /* _KERNEL_DETAIL_AMD64_HALPTYPES_H_ */
